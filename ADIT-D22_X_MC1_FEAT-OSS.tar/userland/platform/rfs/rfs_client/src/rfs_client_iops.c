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

#include "rfs_client_iops.h"

int
rfs_c_create(struct inode *dir,
		struct dentry *entry, int dunno, struct nameidata *nameid)
{
	struct inode *node;

	node = new_inode(entry->d_sb);
	node->i_fop = rfs_c_fops_p;
	/*
	 * this will avoid WARN_ON in unlock_new_inode(...)
	 */
	node->i_state |= I_NEW;

	if (IS_ERR(node))
		return PTR_ERR(node);

	d_instantiate(entry, node);
	unlock_new_inode(node);
	return 0;

}

int
rfs_c_getattr(struct vfsmount *mnt,
		struct dentry *entry, struct kstat *rfs_c_stat)
{
	struct rfs_stat_reply resp;

	if (0 == rfs_c_get_stat(entry, &resp)) {
		rfs_c_stat->uid = 0;
		rfs_c_stat->gid = 0;

		rfs_c_stat->atime.tv_sec = resp.stat.st_access;
		rfs_c_stat->mtime.tv_sec = resp.stat.st_modify;
		rfs_c_stat->ctime.tv_sec = resp.stat.st_change;

		rfs_c_stat->nlink = resp.stat.st_nlink;
		rfs_c_stat->mode = resp.stat.st_mode;
		rfs_c_stat->ino = resp.stat.st_ino;

		rfs_c_stat->size = resp.stat.st_size;
		rfs_c_stat->blksize = resp.stat.st_blksize;
		rfs_c_stat->blocks = resp.stat.st_blocks;

	} else{
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_getattr: returning -ENOENT\n");
		return -ENOENT;
	}

	return 0;
}

int
rfs_c_link(struct dentry *src_ntr, struct inode *node, struct dentry *dst_ntr)
{
	int ret = 0;
	char *inmsg;
	char *outmsg;
	struct rfs_link_req *req;
	struct rfs_gen_reply *rep;

	inmsg = kcalloc(1, max_len, GFP_KERNEL);
	outmsg = kcalloc(1, max_len, GFP_KERNEL);
	if ((outmsg == NULL) || (inmsg == NULL)) {
		printk(KERN_CRIT "rfs_c_link: kcalloc failed, thread exiting\n");
		goto nomem;
	}

	ret = mknames(outmsg, src_ntr, dst_ntr);

	switch (ret) {
	case RFS_ENAMETOOLONG:
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rename:"
				" mknames returned RFS_ENAMETOOLONG\n");
		goto oops;
	case RFS_ENOENT:
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rename:"
				" mknames returned RFS_ENOENT\n");
		goto oops;
	default:
		break;
	}

	req = (struct rfs_link_req *)outmsg;

	req->code = htole32(RFS_LINK);
	req->reqId = 0;
	req->pflags = 0;

	ret = vrfs_send(
						VIRTIO_ID_RFS_B,
						outmsg,
						(
							sizeof(*req)
							+ le32toh(req->names.len1)
							+ le32toh(req->names.len2)
						)
					);

	if (0 >= ret) {
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_link: vrfs_send failed!\n");
		goto oops;
	}

	/*
	 * just get rid of this message... linux doesn't evaluate the return value
	 */
	ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);
	rep = (struct rfs_gen_reply *)inmsg;

	if (!((0 == le32toh(rep->rc)) && (RFS_LINK_RE == le32toh(rep->code)))) {
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_link:"
				" ERROR: %d == rep->rc, 0x%x == resp->code\n",
				le32toh(rep->rc), le32toh(rep->code)
				);
		  goto oops;
	}

	kfree(inmsg);
	kfree(outmsg);
	return 0;

nomem:
	printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_link: nomem: returning -ENOMEM!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOMEM;

oops:
	printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_link: oops: returning -ENOENT!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOENT;
}

struct dentry *
rfs_c_lookup(struct inode *parent_inode, struct dentry *entry,
		struct nameidata *nmidata)
{
	struct dentry *ret = NULL;
	struct inode *file_inode;
	struct rfs_stat_reply resp;

	/*
	 * TODO: use d_lookup to search for a dentry first before creating a new one!!!
	 */

	ret = d_lookup(entry, &entry->d_name);

	if (NULL != ret) {
		file_inode = entry->d_inode;
	} else {
		file_inode = new_inode(parent_inode->i_sb);
	}

	if (!file_inode || !rfs_c_fops_p || !rfs_c_iops_p)
		return ERR_PTR(-EACCES);

	if (0 == rfs_c_get_stat(entry, &resp)) {

		/*
		 * register callback structures
		 */
		file_inode->i_fop = rfs_c_fops_p;
		file_inode->i_op = rfs_c_iops_p;

		/*!
		 * currently all entries will be mapped to root
		 * \todo a request is to make this depending on module argument
		 */
		file_inode->i_uid = resp.stat.st_uid;
		file_inode->i_gid = resp.stat.st_gid;

		/*
		 * timestamps
		 */
		file_inode->i_atime.tv_sec = resp.stat.st_access;
		file_inode->i_mtime.tv_sec = resp.stat.st_modify;
		file_inode->i_ctime.tv_sec = resp.stat.st_change;

		file_inode->i_blocks = 0;
		file_inode->i_bytes = 0;
		file_inode->i_size = 0;

		/*
		 * file mode: type and privileges
		 */
		file_inode->i_mode = resp.stat.st_mode;

		/*!
		 * \todo some vfs implementations have a different handling
		 * for regular files, directories and others...
		 * Do we need this too?
		 */

		entry->d_inode = file_inode;

		/*!
		 * quick hack for dcache.c:676
		 * \todo research d_count handling!
		 */
		dget(entry);
		atomic_inc(&entry->d_count);

		/*
		 * finally add the new entry!!!
		 * TODO: check what happens when
		 * inserting an already existing entry!
		 */
		d_add(entry, file_inode);

		/*
		 * returning a dentry* will lead to calling dput in do_lookup(namei.c)
		 */
		ret = entry;
	} else{
		d_add(entry, NULL);
		ret = NULL;
	}

	/*!
	 * \todo correct error handling would be?
	 * This handling is stated in the kernel doc about vfs
	 */

	return ret;
}

int
rfs_c_mkdir(struct inode *node, struct dentry *entry, int unknown)
{
	/*!
	 * \todo clearify the "real" name and purpose of unknown!
	 */

	int ret = 0;
	int tx_size = 0;
	char *inmsg;
	char *outmsg;
	struct rfs_mkdir_req *req;
	struct rfs_gen_reply *rep;

	if (!rfs_c_lookup_path(entry)) {

		inmsg = kcalloc(1, max_len, GFP_KERNEL);
		outmsg = kcalloc(1, max_len, GFP_KERNEL);
		if ((outmsg == NULL) || (inmsg == NULL)) {
			printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_mkdir: "
					"kcalloc failed, thread exiting\n");
			goto nomem;
		}

		req = (struct rfs_mkdir_req *)outmsg;
		req->code = htole32(RFS_MKDIR);
		/*!
		 * \todo how does RFS create / handle the reqId?
		 * <- array[4] of requests... struct request reqTable[4]
		 */
		req->reqId = 0;


		req->name.len = strlen(&full_path[0]) + 1;

		tx_size = req->name.len;
		memmove(req->name.str, &full_path[0], req->name.len);
		req->name.len = htole32(req->name.len);

		spin_lock(&node->i_lock);
			req->mode = htole32(conv_flags_lx_rfs(node->i_mode));
		spin_unlock(&node->i_lock);

		ret = vrfs_send(
				VIRTIO_ID_RFS_B,
				outmsg,
				(sizeof(*req) + tx_size + 1)
				);
		if (0 >= ret) {
			printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_mkdir: vrfs_send failed!\n");
			goto oops;
		}

		ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);

		if (0 >= ret) {
			printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_mkdir: vrfs_read failed!\n");
			goto oops;
		}

		rep = (struct rfs_gen_reply *)inmsg;

		if (!((0 == le32toh(rep->rc)) && (RFS_MKDIR_RE == le32toh(rep->code)))) {
			printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_mkdir: "
					"ERROR: %d == resp->rc, 0x%x == resp->code, %s == \n",
					le32toh(rep->rc), le32toh(rep->code), entry->d_name.name
					);
			goto oops;
		}

		d_instantiate(entry, node);

		kfree(inmsg);
		kfree(outmsg);
		return 0;
	} else {
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_mkdir: name too long!!!\n");
		return -ENOENT;
	}

nomem:
	printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_mkdir: returning -ENOMEM!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOMEM;
oops:
	printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_mkdir: returning -ENOENT!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOENT;
}

int
rfs_c_permission(struct inode *node, int unknown, struct nameidata *nmdta)
{
	return 0;

}

int
rfs_c_rename(struct inode *src_node, struct dentry *src_entry,
		struct inode *to_node, struct dentry *to_entry)
{
	int ret = 0;
	char *inmsg;
	char *outmsg;
	struct rfs_rename_req *req;
	struct rfs_gen_reply *rep;

	inmsg = kcalloc(1, max_len, GFP_KERNEL);
	outmsg = kcalloc(1, max_len, GFP_KERNEL);
	if ((outmsg == NULL) || (inmsg == NULL)) {
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rename: kcalloc failed, thread exiting\n");
		goto nomem;
	}

	req = (struct rfs_rename_req *)outmsg;
	req->code = htole32(RFS_RENAME);
	/*!
	 * \todo how does RFS create / handle the reqId? <- array[4] of requests
	 * struct request reqTable[4]
	 */
	req->reqId = 0;

	ret = mknames(outmsg, src_entry, to_entry);

	switch (ret) {
	case RFS_ENAMETOOLONG:
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rename:"
				" mknames returned RFS_ENAMETOOLONG\n");
		goto oops;
	case RFS_ENOENT:
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rename:"
				" mknames returned RFS_ENOENT\n");
		goto oops;
	default:
		break;
	}
	ret = vrfs_send(
						VIRTIO_ID_RFS_B,
						outmsg,
						(
							sizeof(*req)
							+ le32toh(req->names.len1)
							+ le32toh(req->names.len2)
						)
					);

	if (0 >= ret) {
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rename: vrfs_send failed!\n");
		goto oops;
	}

	ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);
	if (0 >= ret) {
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rename: vrfs_read failed!\n");
		goto oops;
	}

	rep = (struct rfs_gen_reply *)inmsg;

	if (!((0 == le32toh(rep->rc)) && (RFS_RENAME_RE == le32toh(rep->code)))) {
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rename: "
				"ERROR: %d == resp->rc, 0x%x == resp->code\n",
				le32toh(rep->rc), le32toh(rep->code)
				);
		goto oops;
	} else{
		ret = rep->rc;
	}

	kfree(inmsg);
	kfree(outmsg);
	return ret;

nomem:
	printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rename: returning -ENOMEM!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOMEM;
oops:
	printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rename: returning -ENOENT!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOENT;
}

int
rfs_c_rmdir(struct inode *node, struct dentry *entry)
{

	int ret = 0;
	char *inmsg;
	char *outmsg;
	struct rfs_rmdir_req *req;
	struct rfs_gen_reply *rep;

	inmsg = kcalloc(1, max_len, GFP_KERNEL);
	outmsg = kcalloc(1, max_len, GFP_KERNEL);
	if ((outmsg == NULL) || (inmsg == NULL)) {
			printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rmdir: kcalloc failed, thread exiting\n");
			goto nomem;
	}

	if (!rfs_c_lookup_path(entry)) {

		req = (struct rfs_rmdir_req *)outmsg;
		req->code = htole32(RFS_RMDIR);
		/*!
		 * \todo how does RFS create / handle the reqId?
		 * <- array[4] of requests... struct request reqTable[4]
		 */
		req->reqId = 0;

		req->name.len = strlen(&full_path[0]) + 1;
		memmove(req->name.str, &full_path[0],
								req->name.len);

		ret = vrfs_send(
				VIRTIO_ID_RFS_B,
				outmsg,
				(sizeof(*req) + req->name.len)
				);
		if (0 >= ret) {
			printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rmdir: vrfs_send failed!\n");
			goto oops;
		}

		ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);
		if (0 >= ret) {
			printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rmdir: vrfs_read failed!\n");
			goto oops;
		}

		rep = (struct rfs_gen_reply *)inmsg;

		if (!((0 == le32toh(rep->rc)) && (RFS_RMDIR_RE == le32toh(rep->code)))) {
			printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rmdir: "
					"ERROR: %d == resp->rc, 0x%x == resp->code\n",
					le32toh(rep->rc), le32toh(rep->code)
					);
			goto oops;
		} else{
			ret = rep->rc;
		}

		kfree(inmsg);
		kfree(outmsg);
		return ret;
	} else {
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rmdir: name too long!\n");
	}

nomem:
	printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rmdir: returning -ENOMEM!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOMEM;
oops:
	printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_rmdir: returning -ENOENT!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOENT;
}


int
rfs_c_unlink(struct inode *node, struct dentry *entry)
{
	int ret = 0;
	char *inmsg;
	char *outmsg;
	struct rfs_unlink_req *req;
	struct rfs_gen_reply *rep;

	if (!rfs_c_lookup_path(entry)) {

		inmsg = kcalloc(1, max_len, GFP_KERNEL);
		outmsg = kcalloc(1, max_len, GFP_KERNEL);
		if ((outmsg == NULL) || (inmsg == NULL)) {
			printk(KERN_CRIT "rfs_c_unlink: kcalloc failed, thread exiting\n");
			goto nomem;
		}

		req = (struct rfs_unlink_req *)outmsg;
		req->code = htole32(RFS_UNLINK);
		/*!
		 * \todo how does RFS create / handle the reqId? <- array[4] of requests...
		 * struct request reqTable[4]
		 */
		req->reqId = 0;
		req->pflags = 0;
		req->name.len = strlen(&full_path[0])+1;

		memmove(&req->name.str[0], &full_path[0], req->name.len);

		ret = vrfs_send(VIRTIO_ID_RFS_B, outmsg, sizeof(*req)+req->name.len);
		if (0 >= ret) {
			printk(KERN_CRIT "rfs_c_unlink: vrfs_send failed!\n");
			goto oops;
		}

		/*
		 * just to get rid of this message...
		 * linux doesn't evaluate if it has been successfull!!!
		 * */
		ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);
		rep = (struct rfs_gen_reply *)inmsg;
		if (!((0 == le32toh(rep->rc)) && (RFS_UNLINK_RE == le32toh(rep->code)))) {
			printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_unlink: "
					"ERROR: %d == rep->rc, 0x%x == resp->code\n",
					le32toh(rep->rc), le32toh(rep->code)
					);
			goto oops;
		}

		kfree(inmsg);
		kfree(outmsg);
		ret = 0;
	} else {
		printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_unlink: name too long!\n");
		ret = -ENOENT;
	}

	return ret;

nomem:
	printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_unlink: nomem: returning -ENOMEM!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOMEM;

oops:
	printk(KERN_CRIT "rfs_client_iops.c -> rfs_c_unlink: oops: returning -ENOENT!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOENT;
}

