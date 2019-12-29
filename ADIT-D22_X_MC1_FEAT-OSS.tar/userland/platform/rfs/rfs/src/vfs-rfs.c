/*
 *   vfs-rfs.h
 *   Copyright (c) 2010 by ADIT
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/msg.h>
#include <linux/mqueue.h>
#include <linux/kthread.h>

#undef USE_MQUEUE

#include "rfs_msg.h"
#include "rfs_util.h"
#include <linux/virtio_ids.h>
#include "../drivers/virtio/virtio_nemid/virtio_rfs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ADIT");

#ifdef USE_MQUEUE
static struct task_struct *tsk_mq;

static unsigned int max_msglen = 4200;
module_param(max_msglen, int, 0);
MODULE_PARM_DESC(max_msglen, "maximum msg length (default:4200)");
#endif

int debug = -1;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "debug level");
static int num_extraworker = 3;
module_param(num_extraworker, int, 0);
MODULE_PARM_DESC(num_extraworker, "number of extra worker threads");

static int max_len;
static struct task_struct *tsk_vrfs;
static struct task_struct **tsk_worker;
static DECLARE_COMPLETION(rfs_thread_exit);

#ifdef USE_MQUEUE
static void dprint_fds(int n)
{
	int i;
	struct files_struct *files = current->files;
	struct fdtable *fdt;

	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);

	for (i = 0; i < n ; i++) {
		printk(KERN_DEBUG "fd[%d]: %p  -- ", i, fdt->fd[i]);
		printk(KERN_DEBUG "fcheck(%d): %p\n", i, fcheck(i));
	}
	spin_unlock(&files->file_lock);
}

static void dump_msgh(char *msg, int n)
{
	int i;
	if (n > 0) {
		printk(KERN_DEBUG "msg: ");
		for (i = 0; i < n; i++)
			printk(KERN_DEBUG "[%02X] ", msg[i]);

		printk(KERN_DEBUG "\n");
	}
}

static int rfs_mq(void *unused)
{
	int mlen, ret;
	char *inmsg;
	char *outmsg;
	unsigned int msg_prio;
	int fd_in;
	int fd_out;
	static struct mq_attr attr;

	attr.mq_flags = 0;
	attr.mq_maxmsg = 2;
	attr.mq_msgsize = max_msglen;

	mlen = attr.mq_msgsize;

	daemonize("rfs_mq");
	inmsg = kmalloc(mlen, GFP_KERNEL);
	outmsg = kmalloc(mlen, GFP_KERNEL);
	if ((outmsg == NULL) || (inmsg == NULL)) {
		kfree(inmsg);
		kfree(outmsg);
		printk(KERN_CRIT "rfs: kmalloc failed, mq thread exiting\n");
		return -ENOMEM;
	}
	allow_signal(SIGKILL);

	fd_in = mq_open(RFS_QNAME_DOWN, O_RDONLY|O_CREAT , 0666, &attr);
	printk(KERN_DEBUG "mq_open(%s): %d\n", RFS_QNAME_DOWN, fd_in);
	fd_out = mq_open(RFS_QNAME_UP, O_WRONLY|O_CREAT , 0666, &attr);
	printk(KERN_DEBUG "mq_open(%s): %d\n", RFS_QNAME_UP, fd_out);

	printk(KERN_DEBUG "mq_msgsize: %ld\n", attr.mq_msgsize);
	dprint_fds(4);

	while (!signal_pending(current)) {
		ret = mq_timedreceive(fd_in, inmsg, max_msglen, &msg_prio,
								NULL);

		if (0 < ret) {
			mlen = wire2vfs((rfs_pdu_t *) inmsg
				, (rfs_pdu_t *) outmsg, max_msglen);
			ret = mq_timedsend(fd_out, outmsg, mlen, msg_prio,
					NULL);
		} else {
			printk(KERN_ERR "receive failed: %d; giving up\n", ret
				);
			break;
		}
	}
	set_current_state(TASK_RUNNING);
	printk(KERN_NOTICE "%s: got signal, exit on request\n", __func__);

	(void) fd_close(fd_in);
	(void) fd_close(fd_out);
	dprint_fds(10);
	kfree(inmsg);
	kfree(outmsg);

	ret = mq_unlink(RFS_QNAME_DOWN);
	printk(KERN_DEBUG "mq_unlink(%s): %d\n", RFS_QNAME_DOWN, ret);
	ret = mq_unlink(RFS_QNAME_UP);
	printk(KERN_DEBUG "mq_unlink(%s): %d\n", RFS_QNAME_UP, ret);

	return 0;
}
#endif

static int request_loop(void *unused)
{
	int mlen, ret;
	char *inmsg;
	char *outmsg;

	/**
	 * Become a kernel thread without attached user resources,
	 * releases all open files
	 */
	daemonize("rfs-wt");

	inmsg = kmalloc(max_len, GFP_KERNEL);
	outmsg = kmalloc(max_len, GFP_KERNEL);
	if ((outmsg == NULL) || (inmsg == NULL)) {
		kfree(inmsg);
		kfree(outmsg);
		printk(KERN_CRIT "rfs: kmalloc failed, thread exiting\n");
		return -ENOMEM;
	}

	allow_signal(SIGKILL);

	while (!signal_pending(current)) {
		ret = vrfs_receive(VIRTIO_ID_RFS_A, inmsg, max_len);

		if (0 < ret) {
			mlen = wire2vfs((rfs_pdu_t *) inmsg
				, (rfs_pdu_t *) outmsg, max_len);
			if (mlen > 0)
				ret = vrfs_send(VIRTIO_ID_RFS_A, outmsg, mlen);
		} else {
			printk(KERN_ERR "receive failed: %d; giving up\n", ret
				); /* funny, eh? */
			break;
		}
	}
	printk(KERN_NOTICE "%s: got signal, exit on request\n", __func__);

	kfree(inmsg);
	kfree(outmsg);
	complete_and_exit(&rfs_thread_exit, 0);
	return 0;
}

static int rfs_kthread(void *unused)
{
	int mlen, ret, n;
	char *inmsg;
	char *outmsg;
	rfs_pdu_t *pdu;

	/**
	 * Become a kernel thread without attached user resources,
	 * releases all open files
	 */
	daemonize("rfs");

	max_len = vrfs_operational(VIRTIO_ID_RFS_A);
	if (max_len < 0) {
		printk(KERN_ERR "rfs: not operational, exiting\n");
		return -ENODEV;
	}

	printk(KERN_INFO "rfs: msgsize: %d\n", max_len);

	inmsg = kmalloc(max_len, GFP_KERNEL);
	outmsg = kmalloc(max_len, GFP_KERNEL);
	if ((outmsg == NULL) || (inmsg == NULL)) {
		kfree(inmsg);
		kfree(outmsg);
		printk(KERN_CRIT "rfs: kmalloc failed, thread exiting\n");
		return -ENOMEM;
	}
	pdu = (rfs_pdu_t *) outmsg;
	pdu->r_attach.code = htole32(RFS_ATTACH_A);
	pdu->r_attach.reqId = htole32(0);
	pdu->r_attach.mtu = htole32(max_len);
	pdu->r_attach.path.len = htole32(1);
	strncpy(pdu->r_attach.path.str, "/", 1);

	allow_signal(SIGKILL);

	ret = vrfs_send(VIRTIO_ID_RFS_B, outmsg, sizeof(struct rfs_attach_req) + 2);
	ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);

	for (n = 0; n < num_extraworker; n++)
		tsk_worker[n] = kthread_run(request_loop, NULL, "rfs-wt");

	while (!signal_pending(current)) {
		ret = vrfs_receive(VIRTIO_ID_RFS_A, inmsg, max_len);

		if (0 < ret) {
			mlen = wire2vfs((rfs_pdu_t *) inmsg
				, (rfs_pdu_t *) outmsg, max_len);
			if (mlen > 0)
				ret = vrfs_send(VIRTIO_ID_RFS_A, outmsg, mlen);
		} else {
			printk(KERN_ERR "receive failed: %d; giving up\n", ret
				); /* funny, eh? */
			break;
		}
	}
	printk(KERN_NOTICE "%s: got signal, exit on request\n", __func__);

	kfree(inmsg);
	kfree(outmsg);
	complete_and_exit(&rfs_thread_exit, 0);
	return 0;
}

static ssize_t debug_show(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buff)
{
	return snprintf(buff, PAGE_SIZE, "%d\n", debug);
}

static ssize_t debug_store(struct kobject *kobj, struct kobj_attribute *attr
			, const char *buff, size_t count)
{
	(void) sscanf(buff, "%d", &debug);
	return count;
}

static int snprint_fds(char *buf, int size)
{
	int i;
	struct files_struct *files = tsk_vrfs->files;
	struct fdtable *fdt;
	int to = 0;
	int n = 0;

	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);

	for (i = 0; i < fdt->max_fds; i++) {
		if (fcheck_files(files, i)) {
			n += snprintf(buf + to, size - to, "[%d]: %s\n",
				i, fdt->fd[i]->f_dentry->d_name.name);
		}
		to += n;
	}
	spin_unlock(&files->file_lock);
	return n;
}

static ssize_t openfiles_show(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buff)
{
	return snprint_fds(buff, PAGE_SIZE);
}

static ssize_t openfiles_write(struct kobject *kobj, struct kobj_attribute *attr
			, const char *buff, size_t count)
{
	/* you can close all files here on request or alike */
	return count;
}

static struct kobject *rfs_kobj;
static struct kobj_attribute debug_attr = __ATTR(debug, 0666
	, debug_show, debug_store);
static struct kobj_attribute openfiles_attr = __ATTR(files, 0662
	, openfiles_show, openfiles_write);

static struct attribute *attributes[] = {
	&debug_attr.attr,
	&openfiles_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attributes,
};

static int do_sysfs_registration(void)
{
	int rc;

	rfs_kobj = kobject_create_and_add("rfs", fs_kobj);
	if (!rfs_kobj) {
		printk(KERN_ERR "Unable to create rfs kset\n");
		rc = -ENOMEM;
		goto out;
	}
	rc = sysfs_create_group(rfs_kobj, &attr_group);
	if (rc) {
		printk(KERN_ERR
		       "Unable to create rfs /sys attributes\n");
		kobject_put(rfs_kobj);
	}
out:
	return rc;
}

static void do_sysfs_unregistration(void)
{
	sysfs_remove_group(rfs_kobj, &attr_group);
	kobject_put(rfs_kobj);
}

static int __init mod_entry_func(void)
{
	(void) do_sysfs_registration();
	tsk_worker = kmalloc(sizeof(struct task_struct) *num_extraworker
		, GFP_KERNEL);
	if (!tsk_worker) {
		printk(KERN_ERR "Unable to create worker threads\n");
		return -ENOMEM;
	}
	tsk_vrfs = kthread_run(rfs_kthread, NULL, "rfs");
#ifdef USE_MQUEUE
	tsk_mq = kthread_run(rfs_mq, NULL, "rfs_mq");
#endif
	return 0;
}


static void __exit mod_exit_func(void)
{
	struct pid *pid;
	int n;
#ifdef USE_MQUEUE
	pid = find_get_pid(tsk_mq->pid);
	kill_pid(pid, SIGKILL, 0);
#endif
	pid = find_get_pid(tsk_vrfs->pid);
	kill_pid(pid, SIGKILL, 0);

	do_sysfs_unregistration();
	wait_for_completion(&rfs_thread_exit);

	for (n = 0; n < num_extraworker; n++) {
		pid = find_get_pid(tsk_worker[n]->pid);
		if (pid) {
			kill_pid(pid, SIGKILL, 0);
			wait_for_completion(&rfs_thread_exit);
		}
	}
	kfree(tsk_worker);
	return;
}

module_init(mod_entry_func);
module_exit(mod_exit_func);

