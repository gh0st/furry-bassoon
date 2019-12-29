/*
 *   rfs_c_fops.c
 *   Copyright (c) 2011 by ADIT
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

#include "rfs_client_fops.h"

/*
 * New, all-improved, singing, dancing, iBCS2-compliant getdents()
 * interface.
 */
struct linux_dirent {
	unsigned long	d_ino;
	unsigned long	d_off;
	unsigned short	d_reclen;
	char		d_name[1];
};

int
rfs_c_fsync(struct file *filp, struct dentry *entry, int datasync)
{
	int ret = 0;
	char *inmsg;
	char *outmsg;
	struct rfs_fsync_req *req;
	struct rfs_gen_reply *rep;
	int *fsdata = 0;

	inmsg = kcalloc(1, max_len, GFP_KERNEL);
	outmsg = kcalloc(1, max_len, GFP_KERNEL);
	if ((outmsg == NULL) || (inmsg == NULL)) {
		printk(KERN_CRIT "rfs_c_read: kcalloc failed, thread exiting\n");
		goto nomem;
	}

	spin_lock(&filp->f_dentry->d_lock);
		fsdata = filp->f_dentry->d_fsdata;
	spin_unlock(&filp->f_dentry->d_lock);

	if (unlikely((NULL == filp) || (NULL == fsdata))) {
		printk(KERN_CRIT "rfs_c_read: "
				"nullpointer (filp || filp->f_dentry->d_fsdata)\n");
		goto nomem;
	}

	inmsg = kcalloc(1, max_len, GFP_KERNEL);
	outmsg = kcalloc(1, max_len, GFP_KERNEL);
	if ((outmsg == NULL) || (inmsg == NULL)) {
		printk(KERN_CRIT "rfs_c_fsync: kcalloc failed, thread exiting\n");
		goto nomem;
	}

	req = (struct rfs_fsync_req *)outmsg;
	req->code = htole32(RFS_FSYNC);
	/*!
	 * \todo how does RFS create / handle the reqId?
	 * <- array[4] of requests... struct request reqTable[4]
	 */
	req->reqId = 0;
	req->type = htole32(datasync);

	spin_lock(&filp->f_dentry->d_lock);
		req->fid = htole32(((struct rfs_c_d_fsdata *)
				filp->f_dentry->d_fsdata)->fid);
	spin_unlock(&filp->f_dentry->d_lock);

	ret = vrfs_send(VIRTIO_ID_RFS_B, outmsg, sizeof(*req));

	if (0 > ret) {
		printk(KERN_CRIT "rfs_c_fsync: vrfs_send failed!\n");
		goto oops;
	}

	ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);

	if (0 > ret) {
		printk(KERN_CRIT "rfs_c_fsync: read failed!\n");
		goto oops;
	}

	rep = (struct rfs_gen_reply *)inmsg;

	if (!((0 == le32toh(rep->rc)) && (RFS_FSYNC_RE == le32toh(rep->code)))) {
		printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_fsync:"
				" ERROR: %d == resp->rc, 0x%x == resp->code\n",
				le32toh(rep->rc), le32toh(rep->code)
				);
		goto oops;
	}

	kfree(inmsg);
	kfree(outmsg);
	return 0;

nomem:
	printk(KERN_CRIT "rfs_c_fsync: returning -ENOMEM!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOMEM;
oops:
	printk(KERN_CRIT "rfs_c_fsync: returning -ENOENT!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOENT;
}

int
rfs_c_open(struct inode *node, struct file *filp)
{
	int ret = 0;
	char *inmsg;
	char *outmsg;
	struct rfs_open_req *req;
	struct rfs_open_reply *rep;
	int *fsdata = 0;

	if (!rfs_c_lookup_path(filp->f_dentry)) {

		inmsg = kcalloc(1, max_len, GFP_KERNEL || __GFP_ZERO);
		outmsg = kcalloc(1, max_len, GFP_KERNEL);

		if (NULL != fsdata) {
			printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_open:"
					" %p == fsdata, not zero!!!\n", fsdata);
			goto oops;
		}

		/*
		 * we need some space to serve the file id from t-kernel side
		 */
		spin_lock(&filp->f_dentry->d_lock);
			filp->f_dentry->d_fsdata =
					kcalloc(1, sizeof(struct rfs_c_d_fsdata), GFP_KERNEL);
			fsdata = filp->f_dentry->d_fsdata;
		spin_unlock(&filp->f_dentry->d_lock);

		if ((NULL == outmsg) || (NULL == inmsg) || (NULL == fsdata)) {
			kfree(inmsg);
			kfree(outmsg);
			spin_lock(&filp->f_dentry->d_lock);
				kfree(filp->f_dentry->d_fsdata);
			spin_unlock(&filp->f_dentry->d_lock);
			printk(KERN_CRIT "rfs_c_open: kcalloc failed, thread exiting\n");
			return -ENOMEM;
		}

		/*!
		 * \todo replace cast by union!!!
		 * \todo what are pflags?
		 */
		req = (struct rfs_open_req *)outmsg;
		req->code = htole32(RFS_OPEN);
		req->reqId = 0;
		req->pflags = 0;

		req->flags = htole32(conv_flags_lx_rfs(filp->f_flags));
		req->mode = htole32(filp->f_mode);

		req->name.len = strlen(&full_path[0]) + 1;
		memmove(req->name.str, &full_path[0],
								req->name.len);

		ret = vrfs_send(
				VIRTIO_ID_RFS_B,
				outmsg,
				(sizeof(*req)+req->name.len)
				);
		if (0 >= ret) {
			printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_open: "
					"vrfs_send failed! ... %d == ret\n", ret);
			ret = -ENOENT;
		} else{
			ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);
			if (0 >= ret) {
				printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_open: "
						"vrfs_read failed! %d == ret\n", ret);
				ret = -ENOENT;
			}

			rep = (struct rfs_open_reply *)inmsg;

			if (!((0 == le32toh(rep->rc))
					&& (RFS_OPEN_RE == le32toh(rep->code)))) {
				printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_open: "
						"ERROR: %d == resp->rc, 0x%x == resp->code\n",
						le32toh(rep->rc), le32toh(rep->code)
						);
				ret = -ENOENT;
			} else{
				spin_lock(&filp->f_dentry->d_lock);
					((struct rfs_c_d_fsdata *)filp->f_dentry->d_fsdata)
							->fid = le32toh(rep->fid);
					((struct rfs_c_d_fsdata *)
							filp->f_dentry->d_fsdata)->poff = 0;
				spin_unlock(&filp->f_dentry->d_lock);
			}
		}

		if (-ENOENT != ret) {
			ret = 0;
		} else{
			printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_open: returning -ENOENT!\n");
		}

		kfree(inmsg);
		kfree(outmsg);

		return ret;
	} else {
		printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_open: name too long!\n");
		return -ENOENT;
	}

oops:
	printk(KERN_CRIT "rfs_c_open: returning -ENOENT!\n");
	spin_lock(&filp->f_dentry->d_lock);
		kfree(filp->f_dentry->d_fsdata);
	spin_unlock(&filp->f_dentry->d_lock);
	kfree(inmsg);
	kfree(outmsg);
	return -ENOENT;
}

ssize_t
rfs_c_read(struct file *filp, char __user *us, size_t len, loff_t *offset)
{
	int ret = 0;
	char *inmsg;
	char *outmsg;
	struct rfs_read_req *req;
	struct rfs_read_reply *rep;
	int *fsdata = 0;

	inmsg = kcalloc(1, max_len, GFP_KERNEL);
	outmsg = kcalloc(1, max_len, GFP_KERNEL);
	if ((outmsg == NULL) || (inmsg == NULL)) {
		printk(KERN_CRIT "rfs_c_read: kcalloc failed, thread exiting\n");
		goto nomem;
	}

	spin_lock(&filp->f_dentry->d_lock);
		fsdata = filp->f_dentry->d_fsdata;
	spin_unlock(&filp->f_dentry->d_lock);

	if (unlikely((NULL == filp) || (NULL == fsdata))) {
		/*
		 * trying to read the root directory?
		 */
		printk(KERN_CRIT "rfs_c_read: nullpointer "
				"(filp || filp->f_dentry->d_fsdata)\n");
		goto nomem;
	}

	req = (struct rfs_read_req *)outmsg;
	req->code = htole32(RFS_READ);
	/*!
	 * \todo how does RFS create / handle the reqId?
	 * <- array[4] of requests... struct request reqTable[4]
	 */
	req->reqId = 0;
	req->len = htole32(len);
	req->off = htole64(*offset);
	spin_lock(&filp->f_dentry->d_lock);
		req->fid = htole32(((struct rfs_c_d_fsdata *)
				filp->f_dentry->d_fsdata)->fid);
	spin_unlock(&filp->f_dentry->d_lock);

	ret = vrfs_send(VIRTIO_ID_RFS_B, outmsg, sizeof(*req));
	if (0 >= ret) {
		printk(KERN_CRIT "rfs_c_read: vrfs_send failed!\n");
		goto oops;
	}

	ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);
	if (0 >= ret) {
		printk(KERN_CRIT "rfs_c_read: vrfs_read failed!\n");
		goto oops;
	}

	rep = (struct rfs_read_reply *)inmsg;

	if (!((0 == le32toh(rep->rc) || (RFS_EISDIR == le32toh(rep->rc)))
			&& (RFS_READ_RE == le32toh(rep->code)))) {
		printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_read:"
				" ERROR: %d == resp->rc, 0x%x == resp->code\n",
				le32toh(rep->rc), le32toh(rep->code)
				);
		goto oops;
	} else if (RFS_EISDIR == le32toh(rep->rc)) {
		goto isdir;
	} else{
		len = le32toh(rep->len);
		*offset = le64toh(rep->ret);
		if (0 != copy_to_user(us, rep->buf, len)) {
			printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_read: "
					"couldn't copy the content to the user space!\n");
			goto oops;
		}
	}

	kfree(inmsg);
	kfree(outmsg);
	return len;

isdir:
	kfree(inmsg);
	kfree(outmsg);
	return -EISDIR;

nomem:
	printk(KERN_CRIT "rfs_c_read: returning -ENOMEM!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOMEM;
oops:
	printk(KERN_CRIT "rfs_c_read: returning -ENOENT!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOENT;
}

int
rfs_c_readdir(struct file *filp, void *vptr, filldir_t fdir)
{
	int ret = 0;
	char *inmsg = NULL;
	char *outmsg = NULL;
	struct qstr read_name;
	struct inode *node = NULL;
	struct dentry *read_dent = NULL;
	struct rfs_readdir_req *req = NULL;
	struct rfs_readdir_reply *rep = NULL;

	int rc = 0;
	int code = 0;
	int ftype = 0;
	int *fsdata = 0;

	/*
	 * TODO:
	 * search for a way to use existing inodes
	 * instead of requesting new ones for each readdir access
	 */

	inmsg = kcalloc(1, max_len, GFP_KERNEL);
	outmsg = kcalloc(1, max_len, GFP_KERNEL);

	spin_lock(&filp->f_dentry->d_lock);
		fsdata = filp->f_dentry->d_fsdata;
	spin_unlock(&filp->f_dentry->d_lock);

	if (unlikely((outmsg == NULL) || (inmsg == NULL) || (NULL == fsdata))) {
		printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_readdir: kcalloc failed,"
				"thread exiting\n");
		goto nomem;
	}

	req = (struct rfs_readdir_req *)outmsg;
	req->code = htole32(RFS_READDIR);
	/*!
	 * \todo how does RFS create / handle the reqId? <- array[4] of requests...
	 * struct request reqTable[4]
	 */
	req->reqId = 0;
	spin_lock(&filp->f_dentry->d_lock);
		req->fid = htole32(((struct rfs_c_d_fsdata *)
				filp->f_dentry->d_fsdata)->fid);
		req->off = htole64(((struct rfs_c_d_fsdata *)
				filp->f_dentry->d_fsdata)->poff);
		/*!
		 * \todo which sizes are used by T-Engine?
		 */
		req->len = 0;
	spin_unlock(&filp->f_dentry->d_lock);

	ret = vrfs_send(VIRTIO_ID_RFS_B, outmsg, sizeof(*req));
	if (0 > ret) {
		printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_readdir: vrfs_send failed!\n");
		goto oops;
	}

	ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);
	if (0 >= ret) {
		printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_readdir: vrfs_read failed!\n");
		goto oops;
	}

	rep = (struct rfs_readdir_reply *)inmsg;

	rc = le32toh(rep->rc);
	code = le32toh(rep->code);

	if (!((0 == rc || (ENOTDIR == rc) || (ENOENT == rc))
			&& (RFS_READDIR_RE == code))) {
		printk(KERN_CRIT "rfs_client_fops.c ->  rfs_c_readdir:"
				" ERROR: %d == resp->rc, 0x%x == resp->code\n", rc, code);
		goto oops;
	} else{

		if (ENOENT == rc) {
			printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_readdir:"
					" got ENOENT -> returning zero!\n");
			ret = 0;
		} else{

			ftype = conv_flags_rfs_lx(le32toh(rep->dirent.d_mode));
			ftype = get_file_type(ftype);
			spin_lock(&filp->f_dentry->d_lock);
				((struct rfs_c_d_fsdata *)filp->f_dentry->d_fsdata)
						->poff = le64toh(rep->ret);
			spin_unlock(&filp->f_dentry->d_lock);

			/*
			 * build a qstr to be used by d_lookup
			 */
			if (filp->f_dentry) {
				strcpy(&full_path[0], rep->dirent.d_name);

				read_name.name	= (char *)&full_path[0];
				read_name.len	= strlen(&full_path[0]);
				read_name.hash	= full_name_hash(read_name.name, read_name.len);

				read_dent = d_lookup(filp->f_dentry, &read_name);
			}

			/*
			 * check whether the found entry already has an inode
			 */
			if (NULL != read_dent) {
				node = read_dent->d_inode;
			} else {
				node = new_inode(filp->f_path.dentry->d_sb);
				inode_add_to_lists(filp->f_path.dentry->d_sb, node);

				read_dent = d_alloc(filp->f_path.dentry, &read_name);
				read_dent->d_inode = node;
				read_dent->d_sb = filp->f_path.dentry->d_sb;
				read_dent->d_inode->i_mode =
						conv_flags_rfs_lx(le32toh(rep->dirent.d_mode));
				read_dent->d_parent = filp->f_path.dentry;
				read_dent->d_inode->i_fop = rfs_c_fops_p;
				read_dent->d_inode->i_op = rfs_c_iops_p;

				/*
				 * FIXME: this is a development version...
				 * the handling of inodes is still in progress!!!
				 */
				d_add(read_dent, node);
			}
			
			if (0 == rc) {
				rc = fdir(
						vptr,
						rep->dirent.d_name,
						strlen(rep->dirent.d_name),
						le64toh(rep->ret),
						node->i_ino,
						ftype
					);

				/*
				printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_readdir: %d == "
						"rc, "
						"%d == ino,"
						" %s == name, "
						"%llu == le64toh(rep->ret),"
						" 0x%x == ftype",
							rc,
							filp->f_path.dentry->d_inode->i_ino,
							rep->dirent.d_name,
							le64toh(rep->ret),
							ftype);
				*/
			} else {
				printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_readdir: "
						"couldn't add any contents "
						"to the directory structure!\n");
			}
			ret = rc;
		}
	}

	kfree(inmsg);
	kfree(outmsg);
	return ret;

nomem:
	printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_readdir: returning -ENOMEM!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOMEM;
oops:
	printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_readdir: aborting due to error!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOENT;
}


int
rfs_c_release(struct inode *node, struct file *filp)
{
	int ret = 0;
	char *inmsg;
	char *outmsg;
	struct rfs_close_req *req;
	struct rfs_gen_reply *rep;
	int *fsdata = 0;

	spin_lock(&filp->f_dentry->d_lock);
		fsdata = filp->f_dentry->d_fsdata;
	spin_unlock(&filp->f_dentry->d_lock);

	inmsg = kcalloc(1, max_len, GFP_KERNEL);
	outmsg = kcalloc(1, max_len, GFP_KERNEL);
	if ((outmsg == NULL) || (inmsg == NULL) || (NULL == fsdata)) {
		printk(KERN_CRIT "rfs_c_release: kcalloc failed, thread exiting\n");
		goto nomem;
	}

	req = (struct rfs_close_req *)outmsg;
	req->code = htole32(RFS_CLOSE);
	 /*!
	  * \todo how does RFS create / handle the reqId? <- array[4] of
	  * requests... struct request reqTable[4]
	  */
	req->reqId = 0;
	spin_lock(&filp->f_dentry->d_lock);
	req->fid = htole32(((struct rfs_c_d_fsdata *)filp->f_dentry->d_fsdata)
			->fid);
	spin_unlock(&filp->f_dentry->d_lock);

	ret = vrfs_send(VIRTIO_ID_RFS_B, outmsg, sizeof(*req));
	if (0 >= ret) {
		printk(KERN_CRIT "rfs_c_release: vrfs_send failed!\n");
		goto oops;
	}

	/*
	 * just to get rid of this message... linux doesn't evaluate if it has
	 * been successfull!!!
	 */
	ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);
	rep = (struct rfs_gen_reply *)inmsg;
	if (!((0 == le32toh(rep->rc)) && (RFS_CLOSE_RE == le32toh(rep->code)))) {
		printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_release: "
				"ERROR: %d == rep->rc, 0x%x == resp->code\n",
				le32toh(rep->rc),
				le32toh(rep->code)
				);
	} else{
		/*
		 * delete the rfs specific data!!!
		 * fid for example
		 */
		spin_lock(&filp->f_dentry->d_lock);
			kfree(filp->f_dentry->d_fsdata);
		spin_unlock(&filp->f_dentry->d_lock);
	}

	kfree(inmsg);
	kfree(outmsg);
	return 0;

nomem:
	printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_release: nomem: returning -ENOMEM!\n");
	spin_lock(&filp->f_dentry->d_lock);
	kfree(filp->f_dentry->d_fsdata);
	spin_unlock(&filp->f_dentry->d_lock);
	kfree(inmsg);
	kfree(outmsg);
	return -ENOMEM;
oops:
	printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_release: oops: returning -ENOENT!\n");
	spin_lock(&filp->f_dentry->d_lock);
	kfree(filp->f_dentry->d_fsdata);
	spin_unlock(&filp->f_dentry->d_lock);
	kfree(inmsg);
	kfree(outmsg);
	return -ENOENT;
}

ssize_t
rfs_c_write(struct file *filp,
		const char __user * us, size_t len, loff_t *offset)
{
	int ret = 0;
	char *inmsg;
	char *outmsg;
	struct rfs_write_req *req;
	struct rfs_write_reply *rep;
	int *fsdata = 0;
	int max_from_user = 0;

	inmsg = kcalloc(1, max_len, GFP_KERNEL);
	outmsg = kcalloc(1, max_len, GFP_KERNEL);
	if ((outmsg == NULL) || (inmsg == NULL)) {
		printk(KERN_CRIT "rfs_c_write: kcalloc failed, thread exiting\n");
		goto nomem;
	}

	/*
	 * the complete size will be restricted by vrfs_operational (virtio_rfs)
	 * we need to get sure the buffer from user space doesn't oversize the
	 * available buffer of the rfs message. Therefore we need to calculate
	 * the complete message size - protocol overhead.
	 */
	max_from_user = (max_len-(sizeof(struct rfs_write_req)-sizeof(char)));

	/*
	 * if the buffer coming from user space is bigger than the available
	 * rfs buffer size, we've to shorten the user data
	 */
	if (len > max_from_user)
		len = max_from_user;

	spin_lock(&filp->f_dentry->d_lock);
		fsdata = filp->f_dentry->d_fsdata;
	spin_unlock(&filp->f_dentry->d_lock);

	if (unlikely((NULL == filp) || (NULL == fsdata))) {
		printk(KERN_CRIT "rfs_c_write: "
				"nullpointer (filp || filp->f_dentry->d_fsdata)\n");
		goto nomem;
	}

	req = (struct rfs_write_req *)outmsg;
	req->code = htole32(RFS_WRITE);
	/*!
	 * \todo how does RFS create / handle the reqId?
	 * <- array[4] of requests... struct request reqTable[4]
	 */
	req->reqId = 0;
	req->len = htole32(len);
	req->off = htole64(*offset);

	if (0 != copy_from_user(req->buf, us, len)) {
		printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_write: couldn't copy user data!\n");
		goto oops;
	}

	spin_lock(&filp->f_dentry->d_lock);
		req->fid = htole32(((struct rfs_c_d_fsdata *)filp->f_dentry
				->d_fsdata)->fid);
	spin_unlock(&filp->f_dentry->d_lock);

	ret = vrfs_send(
			VIRTIO_ID_RFS_B,
			outmsg,
			(sizeof(struct rfs_write_req)+req->len)
			);
	if (0 >= ret) {
		printk(KERN_CRIT "rfs_c_write: vrfs_send failed!\n");
		goto oops;
	}

	ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);
	if (0 >= ret) {
		printk(KERN_CRIT "rfs_c_write: vrfs_read failed!\n");
		goto oops;
	}

	rep = (struct rfs_write_reply *)inmsg;

	if (!((0 == le32toh(rep->rc)) && (RFS_WRITE_RE == le32toh(rep->code)))) {
		printk(KERN_CRIT "rfs_client_fops.c -> rfs_c_write: "
				"ERROR: %d == resp->rc, 0x%x == resp->code\n",
				le32toh(rep->rc), le32toh(rep->code)
				);
		goto oops;
	} else{
		len = le32toh(rep->len);
		*offset = le64toh(rep->ret);
	}

	kfree(inmsg);
	kfree(outmsg);
	return len;

nomem:
	printk(KERN_CRIT "rfs_c_write: returning -ENOMEM!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOMEM;
oops:
	printk(KERN_CRIT "rfs_c_write: returning -ENOENT!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOENT;
}

