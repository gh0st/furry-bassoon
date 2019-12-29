#ifndef RFS_CLIENT_GLOBALS_H_
#define RFS_CLIENT_GLOBALS_H_

#include <linux/errno.h>
#include <linux/time.h>
#include <linux/stat.h>
#include <linux/bitops.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/mqueue.h>

#include <linux/msg.h>
#include <asm/stat.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/dirent.h>
#include <linux/statfs.h>
#include <linux/mount.h>
#include <linux/fdtable.h>
#include <linux/netdevice.h>

#include "rfs_msg.h"
#include "rfs_util.h"
#include "../drivers/virtio/virtio_nemid/virtio_rfs.h"

extern int max_len;
extern char full_path[];
extern struct inode_operations *rfs_c_iops_p;
extern struct file_operations *rfs_c_fops_p;

#endif /* RFS_CLIENT_GLOBALS_H_ */
