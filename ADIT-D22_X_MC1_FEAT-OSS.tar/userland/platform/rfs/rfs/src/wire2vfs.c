/*
 *   wire2vfs.c
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

#include <linux/fs.h>
#include <linux/err.h>
#include <linux/fsnotify.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/statfs.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/security.h>
#include <linux/ioctl.h>
#include <linux/smp_lock.h>

#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>

#include "rfs_msg.h"
#include "rfs_util.h"

char *code2str(int code)
{
	char *rc;
	switch (code) {

	case RFS_OPEN:
		rc = "OPEN";
		break;
	case RFS_CLOSE:
		rc = "CLOSE";
		break;
	case RFS_READ:
		rc = "READ";
		break;
	case RFS_WRITE:
		rc = "WRITE";
		break;
	case RFS_STAT:
		rc = "STAT";
		break;
	case RFS_FSTAT:
		rc = "FSTAT";
		break;
	case RFS_READDIR:
		rc = "READDIR";
		break;
	case RFS_MKDIR:
		rc = "MKDIR";
		break;
	case RFS_RMDIR:
		rc = "RMDIR";
		break;
	case RFS_STATVFS:
		rc = "STATVFS";
		break;
	case RFS_FSTATVFS:
		rc = "FSTATVFS";
		break;
	case RFS_RENAME:
		rc = "RENAME";
		break;
	case RFS_CHMOD:
		rc = "CHMOD";
		break;
	case RFS_FCHMOD:
		rc = "FCHMOD";
		break;
	case RFS_CHDIR:
		rc = "CHDIR";
		break;
	case RFS_FCHDIR:
		rc = "FCHDIR";
		break;
	case RFS_ACCESS:
		rc = "ACCESS";
		break;
	case RFS_IOCTL:
		rc = "IOCTL";
		break;
	case RFS_FSYNC:
		rc = "FSYNC";
		break;
	case RFS_TRUNC:
		rc = "TRUNC";
		break;
	case RFS_LINK:
		rc = "LINK";
		break;
	case RFS_UNLINK:
		rc = "UNLINK";
		break;
	case RFS_CHOWN:
		rc = "CHOWN";
		break;
	case RFS_FCHOWN:
		rc = "FCHOWN";
		break;
	case RFS_UTIME:
		rc = "UTIME";
		break;
	case RFS_SELECT:
		rc = "SELECT";
		break;
	case RFS_ATTACH_A:
		rc = "ATTACH_A";
		break;
	case RFS_ATTACH_A_RE:
		rc = "ATTACH_A_RE";
		break;
	default:
		rc = "<na>";
		break;
	}
	return rc;
}

/*!
 * \brief	convert file flags from intermediate rfs to lnx ...
 * \param rfs_flags rfs flags that shall be converted to linux flags
 * \return returns the flags in linux translated version
 */
static int conv_flags_rfs_lx(int rfs_flags)
{
	int lx_flags = 0;

	switch ((rfs_flags & (O_ACCMODE))) {
	case RFS_O_RDONLY:
		lx_flags = O_RDONLY;
		break;
	case RFS_O_WRONLY:
		lx_flags = O_WRONLY;
		break;
	case RFS_O_RDWR:
		lx_flags |= O_RDONLY;
		lx_flags |= O_WRONLY;
		break;
	default:
		lx_flags = RFS_O_RDONLY;
		break;
	}

	lx_flags |= (rfs_flags & RFS_O_CREAT) ?		O_CREAT		: 0;
	lx_flags |= (rfs_flags & RFS_O_EXCL) ?		O_EXCL		: 0;
	lx_flags |= (rfs_flags & RFS_O_NOCTTY) ?	O_NOCTTY	: 0;
	lx_flags |= (rfs_flags & RFS_O_TRUNC) ?		O_TRUNC		: 0;
	lx_flags |= (rfs_flags & RFS_O_APPEND) ?	O_APPEND	: 0;
	lx_flags |= (rfs_flags & RFS_O_NONBLOCK) ?	O_NONBLOCK	: 0;

	return lx_flags;
}

/**
 * fid maps 1:1 to FD, if the LFS really requires something else ..
 * .. we need something different
 */
static unsigned int fid_to_fd(uint32_t fid)
{
	return fid;
}

static int do_open(const char *filename, int flags, int mode)
{
	int fd;

	fd = get_unused_fd();
	if (fd >= 0) {
		struct file *f = filp_open(filename, flags, mode);
		if (unlikely(IS_ERR(f))) {
			put_unused_fd(fd);
			fd = PTR_ERR(f);
		} else {
			fsnotify_open(f->f_path.dentry);
			fd_install(fd, f);
		}
	}

	return fd;
}

static struct file *fid_to_filp(uint32_t fid)
{
	struct file *filp = NULL;
	struct files_struct *files = current->files;
	struct fdtable *fdt;
	unsigned int fd = fid_to_fd(fid);

	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);
	if (fd < fdt->max_fds)
		filp = fdt->fd[fd];

	spin_unlock(&files->file_lock);
	return filp;
}

int fd_close(unsigned int fd)
{
	struct file *filp;
	struct files_struct *files = current->files;
	struct fdtable *fdt;
	int retval;

	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);
	if (fd >= fdt->max_fds)
		goto out_unlock;
	filp = fdt->fd[fd];
	if (!filp)
		goto out_unlock;
	rcu_assign_pointer(fdt->fd[fd], NULL);
	FD_CLR(fd, fdt->close_on_exec);
	spin_unlock(&files->file_lock);

	put_unused_fd(fd);

	retval = filp_close(filp, files);

	return retval;

out_unlock:
	spin_unlock(&files->file_lock);
	return -EBADF;
}

static  int vfs_open_impl(struct rfs_open_req *req, rfs_pdu_t *rep)
{
	struct rfs_open_reply *reply = (struct rfs_open_reply *) rep;

	printk(KERN_INFO "wire2vfs -> vfs_open_impl: called\n");

	if (reply) {
		int rc;

		reply->code = htole32(RFS_OPEN_RE);
		req->flags = conv_flags_rfs_lx(le32toh(req->flags));

		rc = do_open(req->name.str, req->flags, le32toh(req->mode));
		reply->fid = htole32(rc);

		reply->rc = 0;  /* E_OK */
		if (0 > rc)
			reply->rc = htole32(-rc);

		dprintk(DBG_0, "%s('%s', flags: 0%o, mode: 0%o): %d\n"
			, __func__, req->name.str, le32toh(req->flags)
			, le32toh(req->mode), rc);
	}
	return sizeof(struct  rfs_open_reply);
}

static int vfs_close_impl(struct rfs_close_req *req, rfs_pdu_t *rep)
{
	struct rfs_gen_reply *reply = (struct  rfs_gen_reply *) rep;

	if (reply) {
		long rc;

		reply->code = htole32(RFS_CLOSE_RE);
		reply->reqId = req->reqId;

		rc = sys_close(le32toh(req->fid));

		if (0 > rc)
			rc = -rc;
		reply->rc = htole32((int) rc);
		dprintk(DBG_0, "%s(%d): %ld\n",
				__func__, le32toh(req->fid), rc);
	}
	return sizeof(struct rfs_gen_reply);
}

static int vfs_read_impl(struct rfs_read_req *req, rfs_pdu_t *rep, int maxsize)
{
	int rc = 0;
	int n;
	size_t reqlen, msg_addlen;
	struct rfs_read_reply *reply = (struct rfs_read_reply *) rep;

	reqlen = le32toh(req->len);
	msg_addlen = 0;

	if (unlikely(reqlen + sizeof(struct rfs_read_reply) > maxsize)) {
		reqlen = (maxsize - sizeof(struct rfs_read_reply)) & ~1023;
	}

	if (reply) {
		loff_t pos;
		struct file *filp;

		reply->code = htole32(RFS_READ_RE);
		reply->reqId  = req->reqId;
		reply->fid = req->fid;
		reply->len = 0;

		if (likely(0 == rc)) {
			filp = fget(le32toh(req->fid));

			if (likely(filp)) {
				mm_segment_t	oldfs = get_fs();

				pos = (loff_t) le64toh(req->off);

				set_fs(KERNEL_DS);
				n = filp->f_op->read(filp, reply->buf
					, reqlen, &pos);
				set_fs(oldfs);
				fput(filp);

				reply->ret = htole64(pos);
				if (n < 0)
					rc = -n;
				else {
					reply->len = htole32(n);
					rc = 0;
					msg_addlen = n;
				}

			} else {
				n = rc = EBADF;
			}
		}
		reply->rc = htole32(rc);

		dprintk(DBG_3, "f_op->read(fd:%d,,len:%d, pos: %lld): %d\n"
			, le32toh(req->fid), reqlen, le64toh(req->off), n);

	}
	return sizeof(struct  rfs_read_reply) + msg_addlen;
}

static int vfs_write_impl(struct rfs_write_req *req, rfs_pdu_t *rep)
{
	struct rfs_write_reply *reply = (struct  rfs_write_reply *) rep;

	if (reply) {
		int rc = 0;
		size_t len;
		loff_t pos;
		struct file *filp;

		reply->code = htole32(RFS_WRITE_RE);
		reply->reqId  = req->reqId;
		reply->fid = req->fid;

		len = le32toh(req->len);
		pos = (loff_t) le64toh(req->off);

		filp = fget(le32toh(req->fid));

		if (likely(filp)) {
			mm_segment_t	oldfs = get_fs();
			set_fs(KERNEL_DS);

			rc = filp->f_op->write(filp, req->buf, len , &pos);
			set_fs(oldfs);
			fput(filp);

			reply->ret = htole32(pos);
			reply->rc = 0;
			if (unlikely(rc < 0)) {
				reply->rc = htole32(-rc);
				reply->len = 0;
			} else {
				reply->len = htole32(rc);
			}
		} else {
			reply->rc = htole32(EBADF);
		}
		dprintk(DBG_3, "f_op->write(fd:%d,,len:%d, pos: %lld): %d\n"
			, le32toh(req->fid), len, le64toh(req->off), rc);

	}
	return sizeof(struct rfs_write_reply);
}


struct getdents_callback {
	struct dirent_wire *current_dir;
	struct dirent_wire *previous;
	int count;	/* space left to copy */
	int error;
	uint64_t loc;
};

/* <the undisclosed creator of> LFS reads one single dentry in a loop */
static int filldir(void *__buf, const char *name, int namlen, loff_t offset,
		   uint64_t ino, unsigned int d_type)
{
	struct dirent_wire *dirent;
	struct getdents_callback *buf = (struct getdents_callback *) __buf;
	uint32_t d_ino;
	int reclen = sizeof(struct dirent_wire);

	dprintk(DBG_0, "%s(%p, name:'%.*s', offset: %lld, ino: %lld)\n"
		, __func__, __buf, namlen, name, offset, ino);
	buf->error = -EINVAL;	/* only used if we fail.. */
	if (reclen > buf->count) {
		dprintk(DBG_0, "filldir: reclen: %d, count: %d\n"
			, reclen, buf->count);
		return -EINVAL;
	}
	d_ino = ino;
	if (sizeof(d_ino) < sizeof(ino) && d_ino != ino) {
		buf->error = -EOVERFLOW;
		return -EOVERFLOW;
	}
	dirent = buf->previous;/* what was in the original code here? */
	dirent = buf->current_dir;
	if (__put_user(d_ino, &dirent->d_ino))
		goto efault;
	if (__put_user(reclen, &dirent->d_reclen))
		goto efault;
	if (copy_to_user(dirent->d_name, name, namlen))
		goto efault;
	if (__put_user(0, dirent->d_name + namlen))
		goto efault;

	buf->loc++;

	buf->previous = dirent;
	dirent = (void *)dirent + reclen;
	buf->current_dir = dirent;
	buf->count -= reclen;
	return 0;
efault:
	buf->error = -EFAULT;
	return -EFAULT;
}


static int vfs_readdir_impl(struct rfs_readdir_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	size_t len;
	struct getdents_callback cb;
	struct rfs_readdir_reply *reply = (struct rfs_readdir_reply *) rep;

	printk(KERN_INFO "wire2vfs -> vfs_readdir_impl: called\n");

	len = le32toh(req->len);
	if (unlikely(len > MAX_RBUF)) {
		rc = -EINVAL;
		len = 0;
	}

	if (reply) {
		loff_t pos;
		struct file *filp;
		mm_segment_t	oldfs = get_fs();

		reply->code = htole32(RFS_READDIR_RE);
		reply->reqId  = req->reqId;
		reply->fid = req->fid;

		if (unlikely(rc))
			goto out;

		filp = fget(le32toh(req->fid));

		if (unlikely(NULL == filp)) {
			rc = EBADF;
			goto out;
		}
		pos = (loff_t) le64toh(req->off);

		set_fs(KERNEL_DS);
		cb.current_dir = (struct dirent_wire *) &reply->dirent;
		cb.previous = NULL;
		cb.count = len;	/* writable buffer size */
		cb.error = 0;
		cb.loc = pos;
		if (0 == pos) {
			/* rewind filp, <undisclosed> does not do it */
			rc = vfs_llseek(filp, 0, SEEK_SET);
			dprintk(DBG_0, "f_op->readdir(pos:0):llseek: %d\n", rc);
		}
		rc = vfs_readdir(filp, filldir, &cb);

		set_fs(oldfs);
		fput(filp);
		dprintk(DBG_0, "f_op->readdir(,,len:%d, pos: %lld): %d\n"
			, len, pos, rc);

		/**
		 * at least on /proc vfs_readdir returns >0 on last entry
		 * which seems to be fine
		 */
		if (rc < 0) {
			dprintk(DBG_0, "f_op->readdir failed with: %d\n"
				, cb.error);
			rc = -rc;
		} else
			rc = 0;
		if (0 >= cb.loc - pos) {
			/* clean an empty dirent */
			reply->dirent.d_reclen = 0;
			reply->dirent.d_name[0] = 0;
		}

		/* give new offset in dir back to caller */
		reply->ret = htole64(cb.loc);
		/* how many bytes were read */
		reply->len = htole32((cb.loc - pos)
			* sizeof(struct dirent_wire));
out:
		dprintk(DBG_0, "f_op->readdir(newpos: %lld): %lld\n"
			, cb.loc, cb.loc - pos);
		if (0 == reply->dirent.d_reclen) {
			reply->rc = htole32(RFS_ENOENT);
		} else {
			reply->rc = htole32(rc);
		}

	}

	return sizeof(struct rfs_readdir_reply) + rc;
}

/*     ===    stat    FILE    FILE   FILE   */
static void put_stat(struct stat_wire *p_stat, struct kstat *p_kstat)
{
	p_stat->st_dev = p_kstat->dev;
	p_stat->st_ino = p_kstat->ino;
	p_stat->st_mode = p_kstat->mode;
	p_stat->st_nlink = p_kstat->nlink;

	p_stat->st_uid = p_kstat->uid;
	p_stat->st_gid = p_kstat->gid;
	p_stat->st_size = p_kstat->size;
	p_stat->st_rdev = p_kstat->rdev;

	p_stat->st_access = p_kstat->atime.tv_sec;
	p_stat->st_modify = p_kstat->mtime.tv_sec;
	p_stat->st_change = p_kstat->ctime.tv_sec;

	p_stat->st_blksize = p_kstat->blksize;
	p_stat->st_blocks = p_kstat->blocks;
}

static int vfs_stat_impl(struct rfs_stat_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	struct kstat kstat;
	struct rfs_stat_reply *reply = (struct rfs_stat_reply *) rep;

	if (reply) {
		mm_segment_t	oldfs = get_fs();
		set_fs(KERNEL_DS);

		reply->code = htole32(RFS_STAT_RE);
		reply->reqId  = req->reqId;

		/* AT_SYMLINK_NOFOLLOW gives lstat semantics */
		rc = vfs_fstatat(AT_FDCWD, req->name.str, &kstat, 0);

		set_fs(oldfs);
		if (0 == rc)
			put_stat(&reply->stat, &kstat);
		else
			rc = -rc;

		reply->rc = htole32(rc);
		dprintk(DBG_0, "%s('%s'): %d\n", __func__, req->name.str, rc);

	}
	return sizeof(struct rfs_stat_reply);
}

static int vfs_fstat_impl(struct rfs_fstat_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	struct kstat kstat;
	struct rfs_stat_reply *reply = (struct rfs_stat_reply *) rep;

	if (reply) {
		mm_segment_t	oldfs = get_fs();
		set_fs(KERNEL_DS);

		reply->code = htole32(RFS_STAT_RE);
		reply->reqId  = req->reqId;

		rc = vfs_fstat(le32toh(req->fid), &kstat);
		set_fs(oldfs);
		if (0 == rc)
			put_stat(&reply->stat, &kstat);
		else
			rc = -rc;

		reply->rc = htole32(rc);
		dprintk(DBG_0, "%s(%d): %d\n", __func__, le32toh(req->fid),  rc);
	}
	return sizeof(struct rfs_stat_reply);
}


/*     ===    stat    VFS   VFS    VFS  */
static void put_statvfs(struct statvfs_wire *p_statfs
			, struct kstatfs *p_kstatfs)
{
	p_statfs->f_bsize = p_kstatfs->f_bsize;
	p_statfs->f_frsize = p_kstatfs->f_frsize;

	p_statfs->f_blocks = p_kstatfs->f_blocks;
	p_statfs->f_bfree = p_kstatfs->f_bfree;
	p_statfs->f_bavail = p_kstatfs->f_bavail;
	p_statfs->f_files = p_kstatfs->f_files;
	p_statfs->f_ffree = p_kstatfs->f_ffree; /* free inodes */
	p_statfs->f_favail = p_kstatfs->f_ffree; /* free indoes for users */

	p_statfs->f_fsid = (uint32_t) (p_kstatfs->f_fsid.val[0]);
	p_statfs->f_flag = 0xAFFEAFFE;		/* not used */
	p_statfs->f_namemax = p_kstatfs->f_namelen;
	dprintk(DBG_2,
	"statvfs: bsize:%d,frsize:%d,blocks:%lld,bfree:%lld,bavail:%lld\n"
		, p_statfs->f_bsize
		, p_statfs->f_frsize
		, p_statfs->f_blocks
		, p_statfs->f_bfree
		, p_statfs->f_bavail
	);
}

static int vfs_statvfs_impl(struct rfs_statvfs_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	struct kstatfs kstatfs;
	struct nameidata nd;
	struct rfs_statvfs_reply *reply = (struct  rfs_statvfs_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_STATVFS_RE);
		reply->reqId  = req->reqId;

		rc = path_lookup(req->name.str, LOOKUP_FOLLOW, &nd);

		if (0 == rc) {
			rc = vfs_statfs(nd.path.dentry, &kstatfs);
			if (0 == rc)
				put_statvfs(&reply->statvfs, &kstatfs);
			path_put(&nd.path);
		}
		if (0 > rc)
			rc = -rc;
		reply->rc = htole32(rc);
		dprintk(DBG_0, "%s('%s'): %d\n", __func__, req->name.str, rc);
	}

	return sizeof(struct rfs_statvfs_reply);
}

static int vfs_fstatvfs_impl(struct rfs_fstatvfs_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	struct kstatfs kstatfs;
	struct file *filp;
	struct rfs_statvfs_reply *reply = (struct  rfs_statvfs_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_FSTATVFS_RE);
		reply->reqId  = req->reqId;

		filp = fid_to_filp(le32toh(req->fid));

		if (filp) {
			rc = vfs_statfs(filp->f_path.dentry, &kstatfs);
			if (0 == rc)
				put_statvfs(&reply->statvfs, &kstatfs);
		} else {
			rc = EBADF;
		}
		reply->rc = htole32(rc);
		dprintk(DBG_0, "%s: %d\n", __func__, rc);
	}

	return sizeof(struct rfs_statvfs_reply);
}
/*     ==== */

static int vfs_mkdir_impl(struct rfs_mkdir_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	int mode;
	struct dentry *dentry;
	struct nameidata nd;
	struct rfs_gen_reply *reply = (struct rfs_gen_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_MKDIR_RE);
		reply->reqId  = req->reqId;

		mode = le32toh(req->mode);
		rc = path_lookup(req->name.str, LOOKUP_PARENT, &nd);
		if (rc)
			goto out_err;

		dentry = lookup_create(&nd, 1);
		rc = PTR_ERR(dentry);
		if (IS_ERR(dentry))
			goto out_unlock;

		if (!IS_POSIXACL(nd.path.dentry->d_inode))
			mode &= ~current_umask();
		rc = mnt_want_write(nd.path.mnt);
		if (rc)
			goto out_dput;
		rc = security_path_mkdir(&nd.path, dentry, mode);
		if (rc)
			goto out_drop_write;
		rc = vfs_mkdir(nd.path.dentry->d_inode, dentry, mode);

out_drop_write:
		mnt_drop_write(nd.path.mnt);
out_dput:
		dput(dentry);
out_unlock:
		mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
		path_put(&nd.path);
out_err:
		if (rc < 0)
			rc = -rc;

		reply->rc = htole32(rc);
		dprintk(DBG_0, "%s('%s'): %d\n", __func__, req->name.str, rc);
	}
	return sizeof(struct rfs_gen_reply);
}

static int vfs_rmdir_impl(struct rfs_rmdir_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	struct dentry *dentry;
	struct nameidata parent, dir;
	struct rfs_gen_reply *reply = (struct  rfs_gen_reply *) rep;

	if (NULL == reply)
		return 0;

	reply->code = htole32(RFS_RMDIR_RE);
	reply->reqId  = req->reqId;

	rc = path_lookup(req->name.str, LOOKUP_PARENT, &parent);
	if (rc)
		return rc;

	switch (parent.last_type) {
	case LAST_DOTDOT:
		rc = -ENOTEMPTY;
		goto exit1;
	case LAST_DOT:
		rc = -EINVAL;
		goto exit1;
	case LAST_ROOT:
		rc = -EBUSY;
		goto exit1;
	}
	rc = path_lookup(req->name.str, LOOKUP_PARENT, &dir);
	if (rc)
		return rc;

	parent.flags &= ~LOOKUP_PARENT;

	mutex_lock_nested(&parent.path.dentry->d_inode->i_mutex
				, I_MUTEX_PARENT);

	dentry = lookup_one_len(dir.last.name, parent.path.dentry
				, dir.last.len);
	rc = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto exit2;
	rc = mnt_want_write(parent.path.mnt);
	if (rc)
		goto exit3;
	rc = security_path_rmdir(&parent.path, dentry);
	if (rc)
		goto exit4;
	rc = vfs_rmdir(parent.path.dentry->d_inode, dentry);
exit4:
	mnt_drop_write(parent.path.mnt);
exit3:
	dput(dentry);
exit2:
	mutex_unlock(&parent.path.dentry->d_inode->i_mutex);
exit1:
	path_put(&parent.path);

	if (rc < 0)
		rc = -rc;

	reply->rc = htole32(rc);
	dprintk(DBG_0, "%s('%s'): %d\n", __func__, req->name.str, rc);

	return sizeof(struct rfs_gen_reply);
}


static int vfs_chdir_impl(struct rfs_chdir_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	struct nameidata nd;
	struct inode *inode;
	struct rfs_gen_reply *reply = (struct rfs_gen_reply *) rep;

	printk(KERN_INFO "wire2vfs.c -> vfs_chdir_impl: called!\n");

	if (reply) {
		reply->code = htole32(RFS_CHDIR_RE);
		reply->reqId  = req->reqId;

		rc = path_lookup(req->name.str, LOOKUP_FOLLOW, &nd);

		if (0 == rc) {
			inode = nd.path.dentry->d_inode;
			if (!S_ISDIR(inode->i_mode))
				rc = ENOTDIR;
			else
				rc = inode_permission(inode, MAY_EXEC
						| MAY_ACCESS);

			path_put(&nd.path);
		}
		if (0 > rc)
			rc = -rc;
		reply->rc = htole32(rc);
		dprintk(DBG_0, "%s(%s): %d\n", __func__, req->name.str, rc);
	}

	return sizeof(struct rfs_gen_reply);
}

static int vfs_fchdir_impl(struct rfs_fchdir_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	int n = 0;
	struct file *filp;
	struct rfs_fchdir_reply *reply = (struct rfs_fchdir_reply *) rep;

	printk(KERN_INFO "wire2vfs.c -> vfs_fchdir_impl: called!\n");

	if (reply) {
		reply->code = htole32(RFS_FCHDIR_RE);
		reply->reqId  = req->reqId;

		filp = fid_to_filp(le32toh(req->fid));

		if (filp) {
			/* permission check still needed? */
			reply->fid = req->fid;
			n = filp->f_path.dentry->d_name.len;
			reply->name.len = n;
			strcpy(reply->name.str, filp->f_dentry->d_name.name);
		} else {
			reply->name.len = 0;
			reply->name.str[0] = 0;
			rc = EBADF;
		}
		dprintk(DBG_0, "%s(%d): is '%s': %d\n", __func__
			, le32toh(req->fid), reply->name.str, rc);
		reply->rc = htole32(rc);
	}
	return sizeof(struct rfs_fchdir_reply) + n + 3;
}


static int vfs_rename_impl(struct rfs_rename_req *req, rfs_pdu_t *rep)
{
	struct dentry *old_dir, *new_dir;
	struct dentry *old_dentry, *new_dentry;
	struct dentry *trap;
	struct nameidata oldnd, newnd;
	int error, n;
	char *oldname, *newname;
	struct varstr *dest;
	struct rfs_gen_reply *reply = (struct rfs_gen_reply *) rep;
	static char *errfmt = "%s: op failed on line: %d\n";

	if (NULL == reply)
		return 0;

	reply->code = htole32(RFS_RENAME_RE);
	reply->reqId  = req->reqId;

	oldname = req->oldn.str;
	n = req->oldn.len+1;
	n = (n+2-1) & ~(2-1);
	dest = (struct varstr *) &req->oldn.str[n];

	newname = dest->str;

	error = path_lookup(oldname, LOOKUP_PARENT, &oldnd);
	if (error) {
		dprintk(DBG_0, errfmt, __func__, __LINE__);
		goto exit;
	}
	error = path_lookup(newname, LOOKUP_PARENT, &newnd);
	if (error) {
		dprintk(DBG_0, errfmt, __func__, __LINE__);
		goto exit1;
	}
	error = -EXDEV;
	if (oldnd.path.mnt != newnd.path.mnt) {
		dprintk(DBG_0, errfmt, __func__, __LINE__);
		goto exit2;
	}
	old_dir = oldnd.path.dentry;
	error = -EBUSY;
	if (oldnd.last_type != LAST_NORM) {
		dprintk(DBG_0, errfmt, __func__, __LINE__);
		goto exit2;
	}
	new_dir = newnd.path.dentry;
	if (newnd.last_type != LAST_NORM) {
		dprintk(DBG_0, errfmt, __func__, __LINE__);
		goto exit2;
	}
	oldnd.flags &= ~LOOKUP_PARENT;
	newnd.flags &= ~LOOKUP_PARENT;
	newnd.flags |= LOOKUP_RENAME_TARGET;

	trap = lock_rename(new_dir, old_dir);

	old_dentry = lookup_one_len(oldnd.last.name, oldnd.path.dentry
					, oldnd.last.len);
	error = PTR_ERR(old_dentry);
	if (IS_ERR(old_dentry)) {
		dprintk(DBG_0, errfmt, __func__, __LINE__);
		goto exit3;
	}
	/* source must exist */
	error = -ENOENT;
	if (!old_dentry->d_inode) {
		dprintk(DBG_0, "lookup: last: %s; old.dentry: %p; len: %d\n"
			, oldnd.last.name, oldnd.path.dentry, oldnd.last.len);
		dprintk(DBG_0, errfmt, __func__, __LINE__);
		goto exit4;
	}
	/* unless the source is a directory trailing slashes give -ENOTDIR */
	if (!S_ISDIR(old_dentry->d_inode->i_mode)) {
		error = -ENOTDIR;
		if (oldnd.last.name[oldnd.last.len]) {
			dprintk(DBG_0, errfmt, __func__, __LINE__);
			goto exit4;
		}
		if (newnd.last.name[newnd.last.len]) {
			dprintk(DBG_0, errfmt, __func__, __LINE__);
			goto exit4;
		}
	}
	/* source should not be ancestor of target */
	error = -EINVAL;
	if (old_dentry == trap) {
		dprintk(DBG_0, errfmt, __func__, __LINE__);
		goto exit4;
	}
	new_dentry = lookup_one_len(newnd.last.name, newnd.path.dentry
					, newnd.last.len);
	error = PTR_ERR(new_dentry);
	if (IS_ERR(new_dentry)) {
		dprintk(DBG_0, errfmt, __func__, __LINE__);
		goto exit4;
	}
	/* target should not be an ancestor of source */
	error = -ENOTEMPTY;
	if (new_dentry == trap) {
		dprintk(DBG_0, errfmt, __func__, __LINE__);
		goto exit5;
	}
	error = mnt_want_write(oldnd.path.mnt);
	if (error) {
		dprintk(DBG_0, errfmt, __func__, __LINE__);
		goto exit5;
	}
	error = security_path_rename(&oldnd.path, old_dentry,
				     &newnd.path, new_dentry);
	if (error) {
		dprintk(DBG_0, errfmt, __func__, __LINE__);
		goto exit6;
	}
	error = vfs_rename(old_dir->d_inode, old_dentry,
				   new_dir->d_inode, new_dentry);
exit6:
	mnt_drop_write(oldnd.path.mnt);
exit5:
	dput(new_dentry);
exit4:
	dput(old_dentry);
exit3:
	unlock_rename(new_dir, old_dir);
exit2:
	path_put(&newnd.path);
exit1:
	path_put(&oldnd.path);
exit:
	if (error < 0)
		error = -error;

	reply->rc = htole32(error);
	dprintk(DBG_0, "%s('%s' to '%s'): %d\n", __func__
		, oldname, newname, error);

	return sizeof(struct rfs_gen_reply);
}

static int __chmod(struct path *path, uint32_t mode)
{
	int rc;
	struct inode *inode;
	struct iattr newattrs;

	rc = mnt_want_write(path->mnt);
	if (0 == rc) {
		inode = path->dentry->d_inode;

		mutex_lock(&inode->i_mutex);
		rc = security_path_chmod(path->dentry, path->mnt, mode);
		if (!rc) {
			if (mode == (mode_t) -1)
				mode = inode->i_mode;
			newattrs.ia_mode = (mode & S_IALLUGO)
				| (inode->i_mode & ~S_IALLUGO);
			newattrs.ia_valid = ATTR_MODE | ATTR_CTIME;
			rc = notify_change(path->dentry, &newattrs);
		}
		mutex_unlock(&inode->i_mutex);
		mnt_drop_write(path->mnt);
	}
	return rc;
}

static int vfs_chmod_impl(struct rfs_chmod_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	uint32_t mode;
	struct nameidata nd;
	struct rfs_gen_reply *reply = (struct  rfs_gen_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_CHMOD_RE);
		reply->reqId  = req->reqId;

		mode = le32toh(req->mode);

		rc = path_lookup(req->name.str, LOOKUP_FOLLOW, &nd);
		if (0 == rc) {
			rc = __chmod(&nd.path, mode);
			path_put(&nd.path);
		}
		if (0 > rc)
			rc = -rc;
		dprintk(DBG_0, "%s('%s'): %d\n", __func__, req->name.str, rc);
		reply->rc = htole32(rc);
	}

	return sizeof(struct rfs_gen_reply);
}

static int vfs_fchmod_impl(struct rfs_fchmod_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	uint32_t mode;
	struct file *filp;
	struct rfs_gen_reply *reply = (struct rfs_gen_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_FCHMOD_RE);
		reply->reqId  = req->reqId;

		filp = fid_to_filp(le32toh(req->fid));

		if (filp) {
			mode = le32toh(req->mode);
			rc = __chmod(&filp->f_path, mode);
		} else
			rc = -EBADF;

		if (0 > rc)
			rc = -rc;
		dprintk(DBG_0, "%s(%d): %d\n", __func__, le32toh(req->fid), rc);
		reply->rc = htole32(rc);
	}

	return sizeof(struct rfs_gen_reply);
}

static int vfs_access_impl(struct rfs_access_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	uint32_t mode;
	struct inode *inode;
	struct nameidata nd;
	struct rfs_gen_reply *reply = (struct rfs_gen_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_ACCESS_RE);
		reply->reqId  = req->reqId;

		mode = le32toh(req->amode);

		rc = path_lookup(req->name.str, LOOKUP_FOLLOW, &nd);
		if (0 == rc) {
			inode = nd.path.dentry->d_inode;

			rc = inode_permission(inode, mode | MAY_ACCESS);
			/* SuS v2 requires we report a read only fs too */
			/**
			* This is a rare case where using __mnt_is_readonly()
			* is OK without a mnt_want/drop_write() pair.  Since
			* no actual write to the fs is performed here, we do
			* not need to telegraph to that to anyone.
			*
			* By doing this, we accept that this access is
			* inherently racy and know that the fs may change
			* state before we even see this result.
			*/
			if (!rc && (mode & S_IWOTH)
				&& !special_file(inode->i_mode)) {
				if (__mnt_is_readonly(nd.path.mnt))
					rc = -EROFS;
			}

			path_put(&nd.path);
		}
		if (0 > rc)
			rc = -rc;
		dprintk(DBG_0, "%s('%s'): %d\n", __func__, req->name.str, rc);
		reply->rc = htole32(rc);
	}
	return sizeof(struct rfs_gen_reply);
}

static long vfs_ioctl(struct file *filp, unsigned int cmd,
		      unsigned long arg)
{
	int error = -ENOTTY;

	if (!filp->f_op)
		return error;

	if (filp->f_op->unlocked_ioctl) {
		error = filp->f_op->unlocked_ioctl(filp, cmd, arg);
		if (error == -ENOIOCTLCMD)
			error = -EINVAL;
	} else if (filp->f_op->ioctl) {
		lock_kernel();
		error = filp->f_op->ioctl(filp->f_path.dentry->d_inode,
					  filp, cmd, arg);
		unlock_kernel();
	}
	return error;
}

static int vfs_ioctl_impl(struct rfs_ioctl_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	int fd;
	unsigned int cmd;
	unsigned long arg;	/* this is actually a pointer */
	struct file *filp;
	struct rfs_ioctl_reply *reply = (struct  rfs_ioctl_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_IOCTL_RE);
		reply->reqId  = req->reqId;

		fd = le32toh(req->fid);
		cmd = le32toh(req->dcmd);

		if ((0 == fd) && (0 == cmd))
			goto out;

		filp = fid_to_filp(fd);
		if (filp) {
			size_t len;
			arg = (unsigned long) &req->data.data[0];
			len = le32toh(req->data.len);

			rc = vfs_ioctl(filp, cmd, arg);

			if (0 == rc) {
				/* TODO: check size, otherwise blindly */
				/* return possibly requested data to caller */
				/* deep copy cos reusage of buffers? */
				memcpy(reply->data.data, (char *) arg, len);
			}
		} else
			rc = EBADF;

out:
		dprintk(DBG_0, "%s: %d\n", __func__, rc);
		if (0 > rc)
			rc = -rc;
		reply->rc = htole32(rc);
	}
	return sizeof(struct rfs_gen_reply);
}

static int vfs_fsync_impl(struct rfs_fsync_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	int datasync;
	struct file *filp;
	struct rfs_gen_reply *reply = (struct rfs_gen_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_FSYNC_RE);
		reply->reqId  = req->reqId;

		filp = fid_to_filp(le32toh(req->fid));

		if (filp) {
			/* now, WHO will get this wrong? ;) */
			datasync = le32toh(req->type);
			rc = vfs_fsync(filp, filp->f_dentry, datasync);
		} else
			rc = EBADF;

		if (0 > rc)
			rc = -rc;
		dprintk(DBG_0, "%s(%d): %d\n", __func__, le32toh(req->fid), rc);
		reply->rc = htole32(rc);
	}
	return sizeof(struct rfs_gen_reply);
}

static int truncate(struct dentry *dentry, loff_t length,
	unsigned int time_attrs, struct file *filp)
{
	int ret;
	struct iattr newattrs;

	/* Not pretty: "inode->i_size" shouldn't really be signed. But it is. */
	if (length < 0)
		return -EINVAL;

	newattrs.ia_size = length;
	newattrs.ia_valid = ATTR_SIZE | time_attrs;
	if (filp) {
		newattrs.ia_file = filp;
		newattrs.ia_valid |= ATTR_FILE;
	}

	/* Remove suid/sgid on truncate too */
	ret = should_remove_suid(dentry);
	if (ret)
		newattrs.ia_valid |= ret | ATTR_FORCE;

	mutex_lock(&dentry->d_inode->i_mutex);
	ret = notify_change(dentry, &newattrs);
	mutex_unlock(&dentry->d_inode->i_mutex);
	return ret;
}

static int vfs_trunc_impl(struct rfs_trunc_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	loff_t length;
	struct file *filp;
	struct rfs_gen_reply *reply = (struct  rfs_gen_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_TRUNC_RE);
		reply->reqId  = req->reqId;

		length = le64toh(req->len);

		filp = fid_to_filp(le32toh(req->fid));

		if (filp)
			rc = truncate(filp->f_path.dentry, length, 0, filp);
		else
			rc = EBADF;

		if (0 > rc)
			rc = -rc;
		dprintk(DBG_0, "%s(%d): %d\n", __func__, le32toh(req->fid), rc);
		reply->rc = htole32(rc);
	}
	return sizeof(struct rfs_gen_reply);
}

static int vfs_link_impl(struct rfs_link_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	struct dentry *new_dentry;
	struct nameidata nd;
	struct nameidata old_nd;
	int n;
	char *oldname, *newname;
	struct varstr *dest;
	struct rfs_gen_reply *reply = (struct rfs_gen_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_LINK_RE);
		reply->reqId  = req->reqId;

		oldname = req->oldn.str;
		n = req->oldn.len+1;
		n = (n+2-1) & ~(2-1);
		dest = (struct varstr *) &req->oldn.str[n];

		newname = dest->str;

		rc = path_lookup(oldname, 0, &old_nd);
		if (rc)
			goto exit;

		rc = path_lookup(newname, LOOKUP_PARENT, &nd);
		if (rc)
			goto out;

		rc = -EXDEV;
		if (old_nd.path.mnt != nd.path.mnt)
			goto out_release;

		new_dentry = lookup_create(&nd, 0);
		rc = PTR_ERR(new_dentry);
		if (IS_ERR(new_dentry))
			goto out_unlock;
		rc = mnt_want_write(nd.path.mnt);
		if (rc)
			goto out_dput;

		rc = security_path_link(old_nd.path.dentry, &nd.path
					, new_dentry);
		if (rc)
			goto out_drop_write;
		rc = vfs_link(old_nd.path.dentry, nd.path.dentry->d_inode
			, new_dentry);
out_drop_write:
		mnt_drop_write(nd.path.mnt);
out_dput:
		dput(new_dentry);
out_unlock:
		mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
out_release:
		path_put(&nd.path);
out:
		path_put(&old_nd.path);

exit:
		if (rc < 0)
			rc = -rc;

		dprintk(DBG_0, "%s('%s' to '%s'): %d\n"
			, __func__, oldname, newname, rc);
		reply->rc = htole32(rc);

	}
	return sizeof(struct rfs_gen_reply);
}

static int vfs_unlink_impl(struct rfs_unlink_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	struct dentry *dentry;
	struct nameidata nd;
	struct inode *inode = NULL;
	struct rfs_gen_reply *reply = (struct  rfs_gen_reply *) rep;
	static char *errfmt = "%s: op failed on line: %d\n";

	if (reply) {
		reply->code = htole32(RFS_UNLINK_RE);
		reply->reqId  = req->reqId;

		rc = path_lookup(req->name.str, LOOKUP_PARENT, &nd);
		if (rc) {
			dprintk(DBG_0, errfmt, __func__, __LINE__);
			goto exit0;
		}
		rc = -EISDIR;
		if (nd.last_type != LAST_NORM) {
			dprintk(DBG_0, errfmt, __func__, __LINE__);
			goto exit1;
		}
		nd.flags &= ~LOOKUP_PARENT;

		mutex_lock_nested(&nd.path.dentry->d_inode->i_mutex
				, I_MUTEX_PARENT);
		dentry = lookup_one_len(nd.last.name, nd.path.dentry
				, nd.last.len);

		rc = PTR_ERR(dentry);
		if (IS_ERR(dentry))
			goto exit2;
		/* Why not before? Because we want correct error value */
		if (nd.last.name[nd.last.len]) {
			rc = !dentry->d_inode ? -ENOENT :
				S_ISDIR(dentry->d_inode->i_mode) ?
				-EISDIR : -ENOTDIR;
			dprintk(DBG_0, errfmt, __func__, __LINE__);
		} else {
			inode = dentry->d_inode;
			if (inode)
				atomic_inc(&inode->i_count);

			rc = mnt_want_write(nd.path.mnt);
			if (rc)
				goto exit3;
			rc = security_path_unlink(&nd.path, dentry);
			if (0 == rc)
				rc = vfs_unlink(nd.path.dentry->d_inode, dentry
					); /* isn't it funny? */

			mnt_drop_write(nd.path.mnt);
		}
exit3:
		dput(dentry);

exit2:
		mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
		if (inode)
			iput(inode);	/* truncate the inode here */

exit1:
		path_put(&nd.path);
exit0:
		if (rc < 0)
			rc = -rc;

		reply->rc = htole32(rc);
		dprintk(DBG_0, "%s('%s'): %d\n", __func__, req->name.str, rc);
	}
	return sizeof(struct rfs_gen_reply);
}

static int chown_common(struct path *path, uid_t user, gid_t group)
{
	struct inode *inode = path->dentry->d_inode;
	int error;
	struct iattr newattrs;

	newattrs.ia_valid =  ATTR_CTIME;
	if (user != (uid_t) -1) {
		newattrs.ia_valid |= ATTR_UID;
		newattrs.ia_uid = user;
	}
	if (group != (gid_t) -1) {
		newattrs.ia_valid |= ATTR_GID;
		newattrs.ia_gid = group;
	}
	if (!S_ISDIR(inode->i_mode)) {
		newattrs.ia_valid |= ATTR_KILL_SUID | ATTR_KILL_SGID
			| ATTR_KILL_PRIV;
	}
	mutex_lock(&inode->i_mutex);
	error = security_path_chown(path, user, group);
	if (!error)
		error = notify_change(path->dentry, &newattrs);

	mutex_unlock(&inode->i_mutex);

	return error;
}

static int vfs_chown_impl(struct rfs_chown_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	uid_t user;
	gid_t group;
	struct nameidata nd;
	struct rfs_gen_reply *reply = (struct  rfs_gen_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_CHOWN_RE);
		reply->reqId  = req->reqId;

		user = le32toh(req->owner);
		group = le32toh(req->group);

		rc = path_lookup(req->name.str, LOOKUP_FOLLOW, &nd);
		if (0 == rc) {

			rc = chown_common(&nd.path, user, group);
			path_put(&nd.path);
		}
		if (0 > rc)
			rc = -rc;
		reply->rc = htole32(rc);
		dprintk(DBG_0, "%s('%s', owner: %d, group: %d): %d\n"
			, __func__, req->name.str, user, group, rc);
	}
	return sizeof(struct rfs_gen_reply);
}

static int vfs_fchown_impl(struct rfs_fchown_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	uid_t user;
	gid_t group;
	struct file *filp;
	struct rfs_gen_reply *reply = (struct rfs_gen_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_FCHOWN_RE);
		reply->reqId  = req->reqId;

		filp = fid_to_filp(le32toh(req->fid));
		user = le32toh(req->owner);
		group = le32toh(req->group);

		if (filp)
			rc = chown_common(&filp->f_path, user, group);
		else
			rc = EBADF;

		if (0 > rc)
			rc = -rc;
		reply->rc = htole32(rc);
		dprintk(DBG_0, "%s(fid: %d, owner: %d, group: %d): %d\n"
			, __func__, le32toh(req->fid), user, group, rc);
	}
	return sizeof(struct rfs_gen_reply);
}


static void copy_timespec(struct timespec *to, struct timespec_wire *from)
{
	to->tv_sec  = from->tv_sec;
	to->tv_nsec = from->tv_nsec;
}

static void copy_utimes(struct timespec *to, struct utimes_wire *from)
{
	copy_timespec(&to[0], &from->actime);
	copy_timespec(&to[1], &from->modtime);
}
/* not exported by Linux 2.6.33 (as many others :) */
static int utimes_common(struct path *path, struct timespec *times)
{
	int error;
	struct iattr newattrs;
	struct inode *inode = path->dentry->d_inode;

	error = mnt_want_write(path->mnt);
	if (error)
		goto out;

	if (times && times[0].tv_nsec == UTIME_NOW &&
		     times[1].tv_nsec == UTIME_NOW)
		times = NULL;

	newattrs.ia_valid = ATTR_CTIME | ATTR_MTIME | ATTR_ATIME;
	if (times) {
		if (times[0].tv_nsec == UTIME_OMIT)
			newattrs.ia_valid &= ~ATTR_ATIME;
		else if (times[0].tv_nsec != UTIME_NOW) {
			newattrs.ia_atime.tv_sec = times[0].tv_sec;
			newattrs.ia_atime.tv_nsec = times[0].tv_nsec;
			newattrs.ia_valid |= ATTR_ATIME_SET;
		}

		if (times[1].tv_nsec == UTIME_OMIT)
			newattrs.ia_valid &= ~ATTR_MTIME;
		else if (times[1].tv_nsec != UTIME_NOW) {
			newattrs.ia_mtime.tv_sec = times[1].tv_sec;
			newattrs.ia_mtime.tv_nsec = times[1].tv_nsec;
			newattrs.ia_valid |= ATTR_MTIME_SET;
		}
		/*
		 * Tell inode_change_ok(), that this is an explicit time
		 * update, even if neither ATTR_ATIME_SET nor ATTR_MTIME_SET
		 * were used.
		 */
		newattrs.ia_valid |= ATTR_TIMES_SET;
	} else {
		/*
		 * If times is NULL (or both times are UTIME_NOW),
		 * then we need to check permissions, because
		 * inode_change_ok() won't do it.
		 */
		error = -EACCES;
		if (IS_IMMUTABLE(inode))
			goto mnt_drop_write_and_out;

		if (!is_owner_or_cap(inode)) {
			error = inode_permission(inode, MAY_WRITE);
			if (error)
				goto mnt_drop_write_and_out;
		}
	}
	mutex_lock(&inode->i_mutex);
	error = notify_change(path->dentry, &newattrs);
	mutex_unlock(&inode->i_mutex);

mnt_drop_write_and_out:
	mnt_drop_write(path->mnt);
out:
	return error;
}

static bool nsec_valid(long nsec)
{
	if (nsec == UTIME_OMIT || nsec == UTIME_NOW)
		return true;

	return nsec >= 0 && nsec <= 999999999;
}


static int set_utimes(char *filename, struct timespec *times)
{
	int rc = -EINVAL;
	struct nameidata nd;

	if (times && (!nsec_valid(times[0].tv_nsec) ||
		      !nsec_valid(times[1].tv_nsec)))
		goto out;


	rc = path_lookup(filename, LOOKUP_FOLLOW, &nd);
	if (0 == rc) {
		rc = utimes_common(&nd.path, times);
		path_put(&nd.path);
	}
out:
	return rc;
}

static int vfs_utime_impl(struct rfs_utime_req *req, rfs_pdu_t *rep)
{
	int rc = 0;
	struct timespec times[2];
	struct rfs_gen_reply *reply = (struct rfs_gen_reply *) rep;

	if (reply) {
		reply->code = htole32(RFS_UTIME_RE);
		reply->reqId  = req->reqId;

		copy_utimes(times, &req->times);
		rc = set_utimes(req->name.str, times);

		if (0 > rc)
			rc = -rc;
		reply->rc = htole32(rc);
		dprintk(DBG_0, "%s('%s'): %d\n", __func__, req->name.str, rc);
	}
	return sizeof(struct rfs_gen_reply);
}

/**
 * this handles packets and performs the corresponding Linux VFS calls
 *
 * returns: length of message to send
*/
int wire2vfs(rfs_pdu_t *wire, rfs_pdu_t *rep, int maxsize)
{
	int rc = 0;

	dprintk(DBG_3, "->%s(rid:%d, c:0x%X(%s))\n"
		, __func__
		, wire->r_gen.reqId
		, wire->code
		, code2str(wire->code)
		);
	switch (wire->code) {

	case RFS_OPEN:
		rc = vfs_open_impl((struct rfs_open_req *) wire, rep);
		break;
	case RFS_CLOSE:
		rc = vfs_close_impl((struct rfs_close_req *) wire, rep);
		break;
	case RFS_READ:
		rc = vfs_read_impl(&wire->r_read, rep, maxsize);
		break;
	case RFS_WRITE:
		rc = vfs_write_impl((struct rfs_write_req *) wire, rep);
		break;
	case RFS_STAT:
		rc = vfs_stat_impl((struct rfs_stat_req *) wire, rep);
		break;
	case RFS_FSTAT:
		rc = vfs_fstat_impl((struct rfs_fstat_req *) wire, rep);
		break;
	case RFS_READDIR:
		rc = vfs_readdir_impl((struct rfs_readdir_req *) wire, rep);
		break;
	case RFS_MKDIR:
		rc = vfs_mkdir_impl((struct rfs_mkdir_req *) wire, rep);
		break;
	case RFS_RMDIR:
		rc = vfs_rmdir_impl((struct rfs_rmdir_req *) wire, rep);
		break;
	case RFS_STATVFS:
		rc = vfs_statvfs_impl((struct rfs_statvfs_req *) wire, rep);
		break;
	case RFS_FSTATVFS:
		rc = vfs_fstatvfs_impl((struct rfs_fstatvfs_req *) wire, rep);
		break;
	case RFS_RENAME:
		rc = vfs_rename_impl((struct rfs_rename_req *) wire, rep);
		break;
	case RFS_CHMOD:
		rc = vfs_chmod_impl((struct rfs_chmod_req *) wire, rep);
		break;
	case RFS_FCHMOD:
		rc = vfs_fchmod_impl((struct rfs_fchmod_req *) wire, rep);
		break;

	case RFS_CHDIR:
		rc = vfs_chdir_impl((struct rfs_chdir_req *) wire, rep);
		break;
	case RFS_FCHDIR:
		rc = vfs_fchdir_impl((struct rfs_fchdir_req *) wire, rep);
		break;
	case RFS_ACCESS:
		rc = vfs_access_impl((struct rfs_access_req *) wire, rep);
		break;
	case RFS_IOCTL:
		rc = vfs_ioctl_impl((struct rfs_ioctl_req *) wire, rep);
		break;
	case RFS_FSYNC:
		rc = vfs_fsync_impl((struct rfs_fsync_req *) wire, rep);
		break;
	case RFS_TRUNC:
		rc = vfs_trunc_impl((struct rfs_trunc_req *) wire, rep);
		break;
	case RFS_LINK:
		rc = vfs_link_impl((struct rfs_link_req *) wire, rep);
		break;
	case RFS_UNLINK:
		rc = vfs_unlink_impl((struct rfs_unlink_req *) wire, rep);
		break;
	case RFS_CHOWN:
		rc = vfs_chown_impl((struct rfs_chown_req *) wire, rep);
		break;
	case RFS_FCHOWN:
		rc = vfs_fchown_impl((struct rfs_fchown_req *) wire, rep);
		break;
	case RFS_UTIME:
		rc = vfs_utime_impl((struct rfs_utime_req *) wire, rep);
		break;
	case RFS_SELECT:
		printk(KERN_ERR "unsupported request: %d\n", wire->code);
		rc = sizeof(rep->r_gen_reply);
		rep->r_gen_reply.code = RFS_SELECT_RE;
		rep->r_gen_reply.reqId = wire->r_gen.reqId;
		rep->r_gen_reply.rc = htole32(78); /* RTOS: ENOSYS */
		break;
	case RFS_ATTACH_A_RE:
		printk(KERN_DEBUG "attach reply: %d\n",  wire->r_gen_reply.rc);
		rc = 0;
		break;

	default:
		printk(KERN_ERR "unhandled request: %d\n", wire->code);
		rc = sizeof(rep->r_gen_reply);
		/* we have no counterpart */
		rep->r_gen_reply.code = wire->code;
		rep->r_gen_reply.reqId = wire->r_gen.reqId;
		rep->r_gen_reply.rc = htole32(78); /* RTOS: ENOSYS */
		break;

	}

	dprintk(DBG_2, "<-%s(rid:%d, c:0x%X(%s), rc:%d) reply_len: %d\n"
		, __func__
		, rep->r_gen_reply.reqId
		, wire->code, code2str(wire->code)
		, rep->r_gen_reply.rc , rc);
	return rc;
}
EXPORT_SYMBOL(wire2vfs);
