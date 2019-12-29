#ifndef RFS_CLIENT_IOPS_H_
#define RFS_CLIENT_IOPS_H_

#include "rfs_client_globals.h"
#include "rfs_client_helper.h"

/*!
 * \brief creates a new inode and defines the file operations for it
 * \param dir the parent inode
 * \param entry the dentry which the inode shall be linked with
 * \param dunno ???
 * \param nameid UNUSED
 * \return error code
 */
int
rfs_c_create(struct inode *dir,
		struct dentry *entry, int dunno, struct nameidata *nameid);

/*!
 * \brief get file stats
 * \param mnt UNUSED
 * \param entry to look for
 * \param rfs_c_stat holds the stat values
 * \return error code
 */
int
rfs_c_getattr(struct vfsmount *mnt,
		struct dentry *entry, struct kstat *rfs_c_stat);

int
rfs_c_link(struct dentry *src_ntr,
		struct inode *node, struct dentry *dst_ntr);

/*!
 * \brief Will be called if somebody tries to get the contents of the filesystem
 * (e.g. calls "ls" on a linux terminal emulator to get the directory entrys)
 * \param parent_inode the directory to start the lookup from
 * \param entry to be filled up
 * \param nmidata UNUSED
 * \return error code
 */
struct dentry *
rfs_c_lookup(struct inode *parent_inode,
		struct dentry *entry, struct nameidata *nmedata);

/*!
 * \brief creates a directory on the remote site
 * \param node to be created
 * \param entry to be associated with
 * \return error code
 */
int
rfs_c_mkdir(struct inode *node, struct dentry *entry, int unknown);

/*!
 * \brief normally this method would check the file access permissions...
 * but in our specific case it always grants permission...
 * The real deal will be handled by the LFS from T-K...
 * The LFS will return a error code if
 * a access is forbidden by the file permissions
 * \param node UNUSED
 * \param mask UNUSED
 * \param nmdta UNUSED
 * \return permissions
 */
int
rfs_c_permission(struct inode *node, int unknown, struct nameidata *nmdta);

/*!
 * \brief renames a file to another name
 * \param to_node refers to the new inode
 * \param to_entry refers to the new dentry
 * \param src_node refers to the old inode
 * \param src_entry refers to the old dentry
 * \return error code
 */
int
rfs_c_rename(struct inode *src_node, struct dentry *src_entry,
		struct inode *to_node, struct dentry *to_entry);

/*!
 * \brief remove a directory
 * \param node to be removed
 * \param entry to be removed
 * \return error code
 */
int
rfs_c_rmdir(struct inode *node, struct dentry *entry);

int
rfs_c_unlink(struct inode *node, struct dentry *entry);

#endif /* RFS_CLIENT_IOPS_H_ */
