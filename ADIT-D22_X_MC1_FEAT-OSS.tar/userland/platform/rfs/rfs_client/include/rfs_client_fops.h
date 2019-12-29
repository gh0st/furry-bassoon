#ifndef RFS_CLIENT_FOPS_H_
#define RFS_CLIENT_FOPS_H_

#include "rfs_client_globals.h"
#include "rfs_client_helper.h"

int
rfs_c_fsync(struct file *filp, struct dentry *entry, int datasync);

/*!
 * \brief open something (file, directory, etc.) and return a file pointer
 * \param node that shall be opened
 * \param filp that shall be initialized
 * \return error code
 */
int
rfs_c_open(struct inode *node, struct file *filp);

/*!
 * \brief read from a file pointer and put the readed data into the user space buffer
 * \param filp to read from
 * \param us shall be filled with user space data (readed data)
 * \param len of data to read
 * \param offset to start from
 * \return the count of data readed
 */
ssize_t
rfs_c_read(struct file *filp, char __user *us, size_t len, loff_t *offset);

/*!
 * \brief read the contents of the directory and fill the kernels dcache
 * \param filp to read from
 * \param vptr UNSED
 * \param fdir is a macro which adds a directory entry to the dcache
 * \return error code
 */
int
rfs_c_readdir(struct file *filp, void *vptr, filldir_t fdir);

/*!
 * \brief delete a inode (actual data) from the media
 * \param node UNUSED
 * \param filp to be released/deleted
 * \return error code
 */
int
rfs_c_release(struct inode *node, struct file *filp);

/*!
 * \brief write the given data from user space to the device
 * \param filp to write to
 * \param us user space data to be written
 * \param len of data to be written
 * \return size of data that has been written
 */
ssize_t
rfs_c_write(struct file *filp,
		const char __user * us, size_t len, loff_t *offset);

#endif /* RFS_CLIENT_FOPS_H_ */
