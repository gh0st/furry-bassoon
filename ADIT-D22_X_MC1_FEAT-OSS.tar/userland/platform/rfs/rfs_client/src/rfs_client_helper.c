/*
 * rfs_c_helper.c
 *
 *  Created on: May 27, 2011
 *      Author: serhard
 */

#include "rfs_client_helper.h"

int conv_flags_lx_rfs(int lx_flags)
{
	int rfs_flags = 0;

	switch ((lx_flags & O_ACCMODE)) {
	case O_RDONLY:
		rfs_flags = RFS_O_RDONLY;
		break;
	case O_WRONLY:
		rfs_flags = RFS_O_WRONLY;
		break;
	case O_RDWR:
		rfs_flags = RFS_O_RDWR;
		break;
	default:
		rfs_flags = RFS_O_RDONLY;
		break;
	}

	rfs_flags |= (lx_flags & O_RDONLY) ? RFS_O_RDONLY : 0;
	rfs_flags |= (lx_flags & O_WRONLY) ? RFS_O_WRONLY : 0;
	rfs_flags |= (lx_flags & O_RDWR) ? RFS_O_RDWR : 0;

	rfs_flags |= (lx_flags & O_CREAT) ?		RFS_O_CREAT : 0;
	rfs_flags |= (lx_flags & O_EXCL) ?		RFS_O_EXCL : 0;
	rfs_flags |= (lx_flags & O_NOCTTY) ?	RFS_O_NOCTTY : 0;
	rfs_flags |= (lx_flags & O_TRUNC) ?		RFS_O_TRUNC : 0;
	rfs_flags |= (lx_flags & O_APPEND) ?	RFS_O_APPEND : 0;
	rfs_flags |= (lx_flags & O_NONBLOCK) ?	RFS_O_NONBLOCK : 0;

	return rfs_flags;
}

int conv_flags_rfs_lx(int rfs_flags)
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

	lx_flags |= (rfs_flags & RFS_O_RDONLY) ?	O_RDONLY : 0;
	lx_flags |= (rfs_flags & RFS_O_WRONLY) ?	O_WRONLY : 0;
	lx_flags |= (rfs_flags & RFS_O_RDWR) ?		O_RDWR	: 0;

	lx_flags |= (rfs_flags & RFS_O_CREAT) ?		O_CREAT		: 0;
	lx_flags |= (rfs_flags & RFS_O_EXCL) ?		O_EXCL		: 0;
	lx_flags |= (rfs_flags & RFS_O_NOCTTY) ?	O_NOCTTY	: 0;
	lx_flags |= (rfs_flags & RFS_O_TRUNC) ?		O_TRUNC		: 0;
	lx_flags |= (rfs_flags & RFS_O_APPEND) ?	O_APPEND	: 0;
	lx_flags |= (rfs_flags & RFS_O_NONBLOCK) ?	O_NONBLOCK	: 0;

	/*
	 * currently the flags defining the file type are not converted
	 */
	lx_flags |= (rfs_flags & S_IFMT);

	return lx_flags;
}

int
get_file_type(int mode)
{
	int ftype = 0;

	if (S_ISLNK(mode)) {
		ftype = DT_LNK;
	} else if (S_ISREG(mode)) {
		ftype = DT_REG;
	} else if (S_ISDIR(mode)) {
		ftype = DT_DIR;
	} else if (S_ISCHR(mode)) {
		ftype = DT_CHR;
	} else if (S_ISBLK(mode)) {
		ftype = DT_BLK;
	} else if (S_ISFIFO(mode)) {
		ftype = DT_FIFO;
	} else if (S_ISSOCK(mode)) {
		ftype = DT_SOCK;
	} else{
		ftype = DT_UNKNOWN;
	}

	return ftype;
}

int
rfs_c_get_stat(struct dentry *entry, struct rfs_stat_reply *resp)
{
	int ret = 0;
	char *inmsg;
	char *outmsg;
	struct rfs_stat_req *req;
	struct rfs_stat_reply *rep;

	if (!rfs_c_lookup_path(entry)) {

		inmsg = kcalloc(1, max_len, GFP_KERNEL);
		outmsg = kcalloc(1, max_len, GFP_KERNEL);
		if ((outmsg == NULL) || (inmsg == NULL)) {
			printk(KERN_CRIT "rfs_c_get_stat: kcalloc failed, thread exiting\n");
			goto nomem;
		}

		req = (struct rfs_stat_req *)outmsg;
		req->code = htole32(RFS_STAT);
		/*!
		 * \todo how does RFS create / handle the reqId?
		 * <- array[4] of requests... struct request reqTable[4]
		 */
		req->reqId = 0;

		/*!
		 * \todo currently only the name withouht path information is transmitted!
		 *		as T-Kernel doesn't know about the current directory from linux
		 *		this can't work... we need to iterate all directories and submit
		 *		the full T-Kernel path!!!
		 */

		/*
		spin_lock(&entry->d_lock);
			req->name.len = htole32(entry->d_name.len)+1;
			strncpy(req->name.str, entry->d_name.name, req->name.len);
			req->name.str[req->name.len] = 0;
		spin_unlock(&entry->d_lock);
		*/

		req->name.len = htole32(strlen(&full_path[0]) + 1);
		strncpy(req->name.str, &full_path[0], req->name.len);

		ret = vrfs_send(VIRTIO_ID_RFS_B, outmsg, sizeof(*req)+req->name.len);
		if (0 > ret) {
			printk(KERN_CRIT "rfs_c_get_stat: vrfs_send failed!\n");
			goto oops;
		}

		ret = vrfs_receive(VIRTIO_ID_RFS_B, inmsg, max_len);

		if (0 > ret) {
			printk(KERN_CRIT "rfs_c_get_stat: read failed!\n");
			goto oops;
		}

		rep = (struct rfs_stat_reply *)inmsg;

		if (!((0 == le32toh(rep->rc)) && (RFS_STAT_RE == le32toh(rep->code)))) {
			printk(KERN_CRIT "rfs_client_helper.c -> rfs_c_get_stat:"
					" ERROR: %d == resp->rc, 0x%x == resp->code\n",
					le32toh(rep->rc), le32toh(rep->code)
					);
			goto oops;
		} else{
			resp->rc = rep->rc;
			resp->code = rep->code;
			resp->stat.st_gid = current_fsgid();

			resp->stat.st_uid = current_fsuid();

			resp->stat.st_access = le32toh(rep->stat.st_access);
			resp->stat.st_blksize = le32toh(rep->stat.st_blksize);
			resp->stat.st_blocks = le64toh(rep->stat.st_blocks);
			resp->stat.st_change = le32toh(rep->stat.st_change);
			resp->stat.st_dev = le32toh(rep->stat.st_dev);
			resp->stat.st_ino = 0;
			if (entry) {
				if (entry->d_inode) {
					resp->stat.st_ino = entry->d_inode->i_ino;
				}
			}
			resp->stat.st_mode =
					conv_flags_rfs_lx(le32toh(rep->stat.st_mode));
			resp->stat.st_modify = le32toh(rep->stat.st_modify);
			resp->stat.st_nlink = le32toh(rep->stat.st_nlink);
			resp->stat.st_rdev = le32toh(rep->stat.st_rdev);
			resp->stat.st_size = le64toh(rep->stat.st_size);
		}

		kfree(inmsg);
		kfree(outmsg);
		return 0;
	} else {
		printk(KERN_CRIT "rfs_client_helper.c -> rfs_c_get_stat: name too long!\n");
		return -ENOENT;
	}

nomem:
	printk(KERN_CRIT "rfs_c_get_stat: returning -ENOMEM!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOMEM;
oops:
	printk(KERN_CRIT "rfs_c_get_stat: returning -ENOENT!\n");
	kfree(inmsg);
	kfree(outmsg);
	return -ENOENT;
}

int
rfs_c_lookup_path(struct dentry *entry)
{
	int ret = 0;
	int len = 0;
	int complete_len = 0;
	struct dentry *curr_entry = NULL;
	struct dentry *srch_entry = NULL;

	/* initialize with zero */
	memset(&full_path[0], 0, NAME_MAX);

	/*
	 * copy the current entry's name which is
	 * to be looked up + null byte
	 */
	memmove(&full_path[0], entry->d_name.name, entry->d_name.len);

	curr_entry = entry;
	srch_entry = entry->d_parent;
	while (curr_entry != srch_entry) {
		len = srch_entry->d_name.len;
		complete_len = strlen(&full_path[0] + 1 + len);

		if (!NAME_MAX <= complete_len) {
			/*
			 * move the current path string to
			 * make space for the parent entry string...
			 * plus one for the slash after a directory name...
			 */
			memmove(
					&full_path[0]+len+1,
					&full_path[0],
					strlen(&full_path[0])
					);

			/* copy the slash in front of the previous folder */
			memcpy(&full_path[0]+len, "/", 1);

			/*
			 * copy the parents name to the beginning of the string...
			 */
			memcpy(&full_path[0], srch_entry->d_name.name, len);

			curr_entry = curr_entry->d_parent;
			srch_entry = curr_entry->d_parent;
		} else {
			srch_entry = curr_entry;
			ret = 1;
		}
	}
	return ret;
}

int
mknames(char *to, struct dentry *src_entry, struct dentry *to_entry)
{
	int ret = 0;
	struct rfs_rename_req *req;
	int req_strc_size = sizeof(struct rfs_rename_req);
	int check_strc_size = sizeof(struct rfs_link_req);

	if (req_strc_size == check_strc_size) {
		req = (struct rfs_rename_req *)to;

		/*
		 * set the old name to be renamed
		 */
		if (!rfs_c_lookup_path(src_entry)) {
			req->names.len1 = strlen(&full_path[0]) + 1;
			if ((req_strc_size + req->names.len1) < max_len) {
				memmove(&req->names.str[0], &full_path[0], req->names.len1);
				req->names.len1 = htole32(req->names.len1);
			} else {
				printk(KERN_CRIT "rfs_client_helper.c -> rfs_c_lookup_path: "
						"name is longer than max_len!\n");
				ret = RFS_ENAMETOOLONG;
			}
		} else {
			printk(KERN_CRIT "rfs_client_helper.c -> rfs_c_rename:"
					" rfs_c_lookup_path returned an error!\n");
			ret = RFS_ENOENT;
		}

		/*
		 * set the new name to be renamed
		 */
		if (!rfs_c_lookup_path(to_entry)) {
			req->names.len2 = strlen(&full_path[0]) + 1;
			if ((req_strc_size + req->names.len1 + req->names.len2) < max_len) {
				memmove(
							(&req->names.str[0] + req->names.len1),
							&full_path[0],
							req->names.len2
						);
				req->names.len2 = htole32(req->names.len2);
			} else {
				printk(KERN_CRIT "rfs_client_helper.c -> rfs_c_lookup_path: "
						"name is longer than max_len!\n");
				ret = RFS_ENAMETOOLONG;
			}
		} else {
			printk(KERN_CRIT "rfs_client_helper.c -> rfs_c_rename:"
					" rfs_c_lookup_path returned an error!\n");
			ret = RFS_ENOENT;
		}
	} else {
		printk(KERN_CRIT "rfs_client_helper.c -> mknames: structures differ in size!\n");
		ret = RFS_ENOENT;
	}

	return ret;
}

