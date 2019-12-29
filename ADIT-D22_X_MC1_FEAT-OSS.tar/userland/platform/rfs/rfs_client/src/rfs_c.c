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

#include "rfs_client_globals.h"
#include "rfs_client_helper.h"
#include "rfs_client_iops.h"
#include "rfs_client_fops.h"
#include "rfs_msg.h"
#include "rfs_util.h"
#include "../drivers/virtio/virtio_nemid/virtio_rfs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ADIT");

#undef USE_MQUEUE


static int num_extraworker = 1;

#ifndef S_SPLINT_S
module_param(num_extraworker, int, 0);
static struct task_struct *tsk_vrfs;
static struct task_struct *tsk_worker;
static DECLARE_COMPLETION(rfs_thread_exit);
#endif

#define rfs_c_MAGIC 1
#define ROOT_INO 1

static char *m_id = "rfs_c_mod";

void
rfs_c_delete_ino(struct inode *ino);

void
rfs_c_drop_ino(struct inode *ino);

static int
rfs_c_fill_super(struct super_block *sb, void *data, int silent);

int
rfs_c_get_sb(struct file_system_type *fstype,
		int flags, const char *name, void *data, struct vfsmount *mnt);

static
struct file_system_type rfs_c_fs_type = {
	.name		= "rfs",
	.owner		= THIS_MODULE,
	.fs_flags	= 0,
	.get_sb		= rfs_c_get_sb,
	.kill_sb	= kill_litter_super,
};

static
struct inode_operations rfs_c_iops = {
	.create		= rfs_c_create,
	.getattr	= rfs_c_getattr,
	.link		= rfs_c_link,
	.lookup		= rfs_c_lookup,
	.mkdir		= rfs_c_mkdir,
	.rename		= rfs_c_rename,
	.rmdir		= rfs_c_rmdir,
	.permission = rfs_c_permission,
	.unlink		= rfs_c_unlink,
};

const struct file_operations rfs_c_fops = {
	.fsync   = rfs_c_fsync,
	.open    = rfs_c_open,
	.read    = rfs_c_read,
	.readdir = rfs_c_readdir,
	.release = rfs_c_release,
	.write   = rfs_c_write,
};

static struct super_operations rfs_c_s_ops = {
	.drop_inode	= generic_drop_inode,
	.delete_inode = rfs_c_delete_ino,
};

/*
 * static struct inode rfs_c_inode = {};
 */

/*!
 * \brief	convert file flags from intermediate rfs to lnx ...
 *			T-Kernel will convert them to again to its own!
 *	\param mode the mode of an d_mode (see linux doc for d_mode)
 *	\return returns the file type
 */
void
rfs_c_delete_ino(struct inode *ino)
{
	/*!
	 * \todo map the inode number to a file name!
	 */
	printk(KERN_INFO "rfs_c.c -> rfs_c_delete_ino called\n");
}

void
rfs_c_drop_ino(struct inode *ino)
{
	printk(KERN_INFO "rfs_client.c -> rfs_c_drop_ino: called!\n");
}

/*!
 * \brief create and setup the "root"-inode
 * \param sb superblock to be filled
 * \param data UNUSED
 * \param silent UNUSED
 * \return error code
 */
static int
rfs_c_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *root;

	/*
	 * file system type definition
	 */
	sb->s_type = &rfs_c_fs_type;

	sb->s_blocksize = 4096;
	sb->s_blocksize_bits = 12;

	root = iget_locked(sb, ROOT_INO);

	/*
	 * the file system will be mapped as root implicitly...
	 * (root mounts the module... so current_* will return the root id
	 */
	root->i_uid = current_fsuid();
	root->i_gid = current_fsgid();

	/*
	 * the super node shall be a directory (S_IFDIR)
	 */
	root->i_mode = S_IFDIR;

	/*
	 * inode operations like lookup for listing directory entries
	 * we use custom methods instead of the simple_* ones...
	 */
	root->i_op = &rfs_c_iops;

	/*
	 * file operations like read/write
	 */
	root->i_fop = &rfs_c_fops;

	sb->s_root = d_alloc_root(root);

	if (!sb->s_root) {
		printk(KERN_CRIT "rfs_c_fill_super returning -EINVAL!\n");
		return -EINVAL;
	}

	/* Indicate success */
	return 0;
}

int
rfs_c_get_sb(struct file_system_type *fstype,
		int flags, const char *name, void *data, struct vfsmount *mnt)
{
	return get_sb_nodev(fstype, flags, data, &rfs_c_fill_super, mnt);
}

static void
rfs_c_put_super(struct super_block *sb)
{
	/*
	 * this method is called with a lock on super_block!!!
	 */
}

static int
rfs_kthread(void *unused)
{
	int ret;
	char *inmsg;
	char *outmsg;
	rfs_pdu_t *pdu;

	/**
	 * Become a kernel thread without attached user resources,
	 * releases all open files
	 */
	daemonize("rfs");

	if (max_len < 0) {
		printk(KERN_ERR "rfs: not operational, exiting\n");
		return -ENODEV;
	}

	inmsg = kcalloc(1, max_len, GFP_KERNEL);
	outmsg = kcalloc(1, max_len, GFP_KERNEL);
	if ((outmsg == NULL) || (inmsg == NULL)) {
		kfree(inmsg);
		kfree(outmsg);
		printk(KERN_CRIT "rfs: kcalloc failed, thread exiting\n");
		return -ENOMEM;
	}
	pdu = (rfs_pdu_t *) outmsg;
	pdu->r_attach.code = htole32(RFS_ATTACH_B);
	pdu->r_attach.reqId = htole32(0);
	pdu->r_attach.mtu = htole32(max_len);
	pdu->r_attach.path.len = htole32(1);
	strncpy(pdu->r_attach.path.str, "/", 1);

	allow_signal(SIGKILL);
	ret = vrfs_send(
			VIRTIO_ID_RFS_B,
			outmsg,
			(sizeof(struct rfs_attach_req) + 2)
			);
	ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);

	switch (((rfs_pdu_t *)inmsg)->code) {
	case RFS_ATTACH_B_RE:
		break;
	default:
		printk(KERN_CRIT "%d <- unknown response for RFS_ATTACH_RE "
				"received!\n", ((rfs_pdu_t *)inmsg)->code);
		break;
	}

	kfree(inmsg);
	kfree(outmsg);
	complete_and_exit(&rfs_thread_exit, 0);
	return 0;
}

static int
mod_entry_func(void)
{
	int ret = 0;

	rfs_c_fops_p = &rfs_c_fops;
	rfs_c_iops_p = &rfs_c_iops;

	max_len = vrfs_operational(VIRTIO_ID_RFS_B);

	if (0 >= max_len) {
		printk(KERN_CRIT "rfs_client.c -> mod_entry_func: not operational! Exiting!\n");
		return -1;
	}

	/*
	 * register the file system
	 */
	if (register_filesystem(&rfs_c_fs_type)) {
		printk(KERN_ERR "%s: couldn't register a filesystem!\n", m_id);
		ret = 1;
	}

	/*
	 * allocate mem for the init thread
	 */
	tsk_worker = kmalloc(sizeof(struct task_struct), GFP_KERNEL);

	/*
	 * try to run the as thread
	 */
	if (tsk_worker) {
		tsk_vrfs = kthread_run(rfs_kthread, NULL, "rfs");
#ifdef USE_MQUEUE
		tsk_mq = kthread_run(rfs_mq, NULL, "rfs_mq");
#endif
	} else{
		printk(KERN_ERR "Unable to create worker threads\n");
		ret = -ENOMEM;
	}

	return ret;
}

static void
mod_exit_func(void)
{
	struct pid *pid;

	/*
	 *  unregister the filesystem
	 */
	if (unregister_filesystem(&rfs_c_fs_type)) {
		printk(KERN_ERR "%s: couldn't find and unregister the "
				"filesystem \"rfs\"!\n", m_id);
	}

#ifdef USE_MQUEUE
	pid = find_get_pid(tsk_mq->pid);
	kill_pid(pid, SIGKILL, 0);
#endif

	pid = find_get_pid(tsk_worker->pid);
	if (pid) {
		kill_pid(pid, SIGKILL, 0);
		wait_for_completion(&rfs_thread_exit);
	}
	kfree(tsk_worker);

	return;
}

module_init(mod_entry_func);
module_exit(mod_exit_func);
