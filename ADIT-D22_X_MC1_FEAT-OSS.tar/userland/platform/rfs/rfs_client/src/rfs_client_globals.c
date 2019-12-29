/*
 * rfs_c_globals.c
 *
 *  Created on: May 27, 2011
 *      Author: serhard
 */

#include "rfs_client_globals.h"

int max_len;
char full_path[NAME_MAX];
struct inode_operations *rfs_c_iops_p;
struct file_operations *rfs_c_fops_p;
