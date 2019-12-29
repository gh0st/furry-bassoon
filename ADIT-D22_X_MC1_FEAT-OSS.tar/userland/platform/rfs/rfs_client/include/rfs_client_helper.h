#ifndef RFS_CLIENT_HELPER_H_
#define RFS_CLIENT_HELPER_H_

#include "rfs_client_globals.h"

/*!
 * \brief	convert file flags to intermediate RFS protocol...
 * 			T-Kernel will convert them to again to its own!
 * 	\param lx_flags linux flags that shall be converted to rfs flags
 * 	\return returns the flags in rfs translated version
 */
int conv_flags_lx_rfs(int lx_flags);

/*!
 * \brief	convert file flags from intermediate rfs to lnx ...
 * 	\param rfs_flags rfs flags that shall be converted to linux flags
 * 	\return returns the flags in linux translated version
 */
int conv_flags_rfs_lx(int rfs_flags);

/*!
 * \brief	convert file flags from intermediate rfs to lnx ...
 * 			T-Kernel will convert them to again to its own!
 * 	\param mode the mode of an d_mode (see linux doc for d_mode)
 * 	\return returns the file type
 */
int
get_file_type(int mode);

/*!
 * \brief get file stats (mode, size...)
 * \param entry the file to lookup
 * \param resp the returned stats
 * \return error code
 */
int
rfs_c_get_stat(struct dentry *entry, struct rfs_stat_reply *resp);

/*!
 * \brief This method helps iterating throug the parent
 * directories until the root directory is found and
 * builds the whole file path which is needed to tell
 * T-Kernel what's the complete path...
 */
int
rfs_c_lookup_path(struct dentry *entry);

/*!
 * \brief this method copies the strings from src_entry and to_entry into "to"
 * if the complete size ain't bigger than max_len. This method is used for creating links
 * and for the mv command
 *
 * \param to char pointer to copy the value to
 * \param src_entry the "original" name
 * \param to_entry the new file name
 */
int
mknames(char *to, struct dentry *src_entry, struct dentry *to_entry);

#endif /* RFS_CLIENT_HELPER_H_ */
