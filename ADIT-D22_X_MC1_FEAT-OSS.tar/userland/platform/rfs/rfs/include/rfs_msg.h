/*
 *   rfs_msg.h
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

#ifndef _RFS_MSG_H_
#define _RFS_MSG_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include    <sys/types.h>
#include    <dirent.h>
#include    <sys/utime.h>
#include    <sys/statvfs.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

#if 1 /*#ifndef __USE_BSD*/

#define htole32(v)  (v)
#define le32toh(v)  (v)
#define htole64(v)  (v)
#define le64toh(v)  (v)

#endif

#define RFS_QNAME_DOWN "rfs-down"
#define RFS_QNAME_UP   "rfs-up"

#define MAX_RBUF   (4096*4)
#define MAX_WBUF   (4096*4)

#define RFS_O_RDONLY    0x00000001  /* open for reading only */
#define RFS_O_WRONLY    0x00000002  /* open for writing only */
#define RFS_O_RDWR      0x00000003  /* open for reading and writing */
#define RFS_O_ACCMODE   0x00000003  /* mask for above modes */

#define RFS_O_CREAT     00000100
#define RFS_O_EXCL      00000200
#define RFS_O_NOCTTY    00000400
#define RFS_O_TRUNC     00001000
#define RFS_O_APPEND    00002000
#define RFS_O_NONBLOCK  00004000
/* oops, even Linux arm and x86 have different values for O_DIRECTORY */
#define RFS_O_DIRECTORY 00040000


/**
 * because the <undisclosed> LFS mangles chown+chmod, we need a way to
 * say: change the owner but dont touch the groupid, and vice versa
 */
#define RFS_CHOWN_DONTCHANGE  (-11)
#define RFS_CHGRP_DONTCHANGE  (-11)

/**
 * because we don't want to include the <undisclosed> include file
 * we hardcode the values, if they don't match: weird things will happen
 */
#define RFS_OPEN           0
#define RFS_CLOSE          1
#define RFS_READ           2
#define RFS_WRITE          3
#define RFS_IOCTL          4
#define RFS_STAT           5
#define RFS_FSTAT          6
#define RFS_ACCESS         7
#define RFS_READDIR        8
#define RFS_CHDIR          9
#define RFS_FCHDIR         10
#define RFS_FSYNC          11
#define RFS_TRUNC          12
#define RFS_LINK           13
#define RFS_UNLINK         14
#define RFS_RENAME         15
#define RFS_CHMOD          16
#define RFS_FCHMOD         17
#define RFS_CHOWN          18
#define RFS_UTIME          19
#define RFS_MKDIR          20
#define RFS_RMDIR          21
#define RFS_STATVFS        22
#define RFS_FSTATVFS       23
#define RFS_FCHOWN         24
#define RFS_SELECT         25
#define RFS_FCNTL          26
#define RFS_ATTACH_A       0x100
#define RFS_ATTACH_B       0x101
#define RFS_PERFTEST       0x200


#define RFS_RESPONSE_MASK       (0x4000)

#define RFS_OPEN_RE             (RFS_RESPONSE_MASK | RFS_OPEN)
#define RFS_CLOSE_RE            (RFS_RESPONSE_MASK | RFS_CLOSE)
#define RFS_READ_RE             (RFS_RESPONSE_MASK | RFS_READ)
#define RFS_WRITE_RE            (RFS_RESPONSE_MASK | RFS_WRITE)
#define RFS_IOCTL_RE            (RFS_RESPONSE_MASK | RFS_IOCTL)
#define RFS_STAT_RE             (RFS_RESPONSE_MASK | RFS_STAT)
#define RFS_FSTAT_RE            (RFS_RESPONSE_MASK | RFS_FSTAT)
#define RFS_ACCESS_RE           (RFS_RESPONSE_MASK | RFS_ACCESS)
#define RFS_READDIR_RE          (RFS_RESPONSE_MASK | RFS_READDIR)
#define RFS_CHDIR_RE            (RFS_RESPONSE_MASK | RFS_CHDIR)
#define RFS_FCHDIR_RE           (RFS_RESPONSE_MASK | RFS_FCHDIR)
#define RFS_FSYNC_RE            (RFS_RESPONSE_MASK | RFS_FSYNC)
#define RFS_TRUNC_RE            (RFS_RESPONSE_MASK | RFS_TRUNC)
#define RFS_LINK_RE             (RFS_RESPONSE_MASK | RFS_LINK)
#define RFS_UNLINK_RE           (RFS_RESPONSE_MASK | RFS_UNLINK)
#define RFS_RENAME_RE           (RFS_RESPONSE_MASK | RFS_RENAME)
#define RFS_CHMOD_RE            (RFS_RESPONSE_MASK | RFS_CHMOD)
#define RFS_FCHMOD_RE           (RFS_RESPONSE_MASK | RFS_FCHMOD)
#define RFS_CHOWN_RE            (RFS_RESPONSE_MASK | RFS_CHOWN)
#define RFS_UTIME_RE            (RFS_RESPONSE_MASK | RFS_UTIME)
#define RFS_MKDIR_RE            (RFS_RESPONSE_MASK | RFS_MKDIR)
#define RFS_RMDIR_RE            (RFS_RESPONSE_MASK | RFS_RMDIR)
#define RFS_STATVFS_RE          (RFS_RESPONSE_MASK | RFS_STATVFS)
#define RFS_FSTATVFS_RE         (RFS_RESPONSE_MASK | RFS_FSTATVFS)
#define RFS_FCHOWN_RE           (RFS_RESPONSE_MASK | RFS_FCHOWN)
#define RFS_SELECT_RE           (RFS_RESPONSE_MASK | RFS_SELECT)
#define RFS_FCNTL_RE            (RFS_RESPONSE_MASK | RFS_FCNTL)
#define RFS_ATTACH_A_RE         (RFS_RESPONSE_MASK | RFS_ATTACH_A)
#define RFS_ATTACH_B_RE         (RFS_RESPONSE_MASK | RFS_ATTACH_B)


/**
 * these relevant error codes differ between Linux (on wire) and T-Kernel.
 *
 * For the wire interface the Linux generic error codes are used. These can be
 * found in following files of the Linux kernel:
 *   - include/asm-generic/errno.h
 *   - include/asm-generic/errno-base.h
 */
#define RFS_ENOENT        2 /* No such file or directory */
#define RFS_EINTR         4 /* Interrupted system call */
#define RFS_EAGAIN       11 /* Try again */
#define RFS_ENOTDIR      20 /* Not a directory */
#define RFS_EISDIR       21 /* Is a directory */
#define RFS_EDEADLK      35 /* Resource deadlock would occur */
#define RFS_ENAMETOOLONG 36 /* File name too long */
#define RFS_ENOSYS       38 /* Function not implemented */
#define RFS_ENOTEMPTY    39 /* Directory not empty */
#define RFS_ELOOP        40 /* Too many symbolic links encountered */
#define RFS_EOVERFLOW    75 /* Value too large for defined data type */
#define RFS_ENOTSOCK     88 /* Socket operation on non-socket */
#define RFS_EMSGSIZE     90 /* Message too long */
#define RFS_EOPNOTSUPP   95 /* Operation not supported on transport endpoint */

/* TODO:
 * st_ino should be 64bits?
 * the uid,gid are 32bits
 * the timestamps are with resolution of seconds
 * Because it's only a matter of time, until we get a software update blob
 * that is bigger than 3GB -> use 64bits (with information loss on LFS
 */
 /* TODO: there was a weird error, if st_atime, st_mtime, st_ctime members
  *   were defined and cross compiled on Linux for arm...
In file included from readdir.c:25:
../include/rfs_msg.h:110: error: expected ':', ',', ';', '}' \
		or '__attribute__' before '.' token

  */

/*
 * d_fsdata
 * a struct that defines the fs_specific data
 * in struct dentry from linux/include/dcache.h
 */
struct rvfs_d_fsdata {
    uint32_t fid;       /* from T-Kernel */
    uint32_t opened;    /* how often this file has been opened?! */
    uint64_t poff;      /* previous offset (used for readdir) */
};

struct stat_wire {
    uint32_t   st_dev;       /* inode's device */
    uint32_t   st_ino;       /* inode's number */
    uint32_t   st_mode;      /* inode protection mode */
    uint32_t   st_nlink;     /* number of hard links */

    uint32_t   st_rdev;      /* major/minor in case of device node */
    uint32_t   st_access;     /* time of last access */
    uint32_t   st_modify;     /* time of last data modification */
    uint32_t   st_change;     /* time of last file status change */

    uint64_t   st_size;      /* file size, in bytes */
    uint64_t   st_blocks;    /* occupied 512bytes blocks, */

    uint32_t   st_uid;       /* user ID of the file's owner */
    uint32_t   st_gid;       /* group ID of the file's group */
    uint32_t   st_blksize;   /* "optimal" IO size */
};


/* eSol's statvfs is a copy of linux statvfs, but limited to 32bits
 */
struct statvfs_wire {
    uint32_t f_bsize;   /* File system block size */
    uint32_t f_frsize;  /* Fundamental file system block size */

    uint64_t f_blocks;  /* Total number of blocks in units of f_frsize */
    uint64_t f_bfree;   /* Total number of free blocks */
    uint64_t f_bavail;  /* Number of free blocks for non-privileged process */
    uint64_t f_files;   /* Total number of file serial numbers */
    uint64_t f_ffree;   /* Total number of free file serial numbers */
    uint64_t f_favail;  /* Number of inodes available to normal users */

    uint32_t f_fsid;    /* File system ID */
    uint32_t f_flag;    /* Bit mask of f_flag values */
    uint32_t f_namemax; /* Maximum filename length */
};

#define NAME_MAX_WIRE   (255)
/* the name is zero terminated */
struct dirent_wire {
    uint32_t        d_ino;
    uint32_t        d_mode;
    uint16_t        d_reclen;
    char            d_name[NAME_MAX_WIRE+1];
};

struct timespec_wire {
	uint32_t	tv_sec;			/* seconds */
	uint32_t	tv_nsec;		/* nanoseconds */
};

struct utimes_wire {
	struct timespec_wire	actime;		/* File access time	*/
	struct timespec_wire	modtime;	/* File modification time */
};

struct varstr {
    uint16_t	len;
    char	str[1];
};

struct blob {
    uint16_t	len;
    char	data[1];
};

struct rfs_gen_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
};

struct rfs_gen_reply {
    uint16_t        code;           /* Command code                 */
    uint16_t        reqId;          /* requestId, like transactionId */
    int32_t         rc;
};

struct  rfs_open_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    uint32_t        flags;                  /* Open flags                   */
    uint32_t        mode;                   /* Open mode                    */
    struct varstr   name;                   /* File name                    */
};

struct  rfs_open_reply {
    uint16_t        code;           		/* Command code                 */
    uint16_t        reqId;          		/* requestId, like transactionId */
    int32_t         rc;
    uint32_t        fid;            		/* return fid        */
};

struct  rfs_close_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        fid;                    /* PFS file ID                  */
};

struct  rfs_read_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        fid;                    /* PFS file ID                  */
    uint32_t        len;                    /* Read and return size ptr     */
    uint64_t        off;                    /* File offset ptr              */
};

struct  rfs_read_reply {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    int32_t         rc;
    uint32_t        fid;                    /* PFS file ID                  */
    uint32_t        len;                    /* Read and return size ptr     */
    uint64_t        ret;                    /* Return offset ptr            */
    char            buf[1];                 /* User buffer                  */
};

struct  rfs_write_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        fid;                    /* PFS file ID                  */
    int32_t         len;                    /* Write and return size ptr    */
    uint64_t        off;                    /* File offset ptr              */
    char            buf[1];                 /* User buffer                  */
};

struct  rfs_write_reply {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    int32_t        rc;
    uint32_t        fid;                    /* PFS file ID                  */
    int32_t         len;                    /* Write and return size ptr    */
    uint64_t        ret;                    /* Return offset ptr            */
};


struct  rfs_link_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    struct varstr   oldn;                   /* Old file name                */
    struct varstr   newn;                   /* New file name                */
};


struct  rfs_fsync_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    uint32_t        fid;                    /* PFS file ID                  */
    uint32_t        type;                   /* Fsync or fdatasync           */
};
#define TYPE_FSYNC      0                   /* Fsync command                */
#define TYPE_FDATASYNC  1                   /* Fdatasync command            */


struct  rfs_access_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    uint32_t        amode;                  /* Access mode                  */
    struct varstr   name;                   /* File name                    */
};

struct  rfs_fstat_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        fid;                    /* PFS file ID                  */
};

struct  rfs_stat_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    struct varstr   name;                   /* File name                    */
};

struct rfs_stat_reply {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    int32_t        rc;
    struct stat_wire stat;                  /* stat structure    */
};

struct  rfs_chmod_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    uint32_t        mode;                   /* File mode                    */
    struct varstr   name;                   /* File name                    */
};

struct  rfs_fchmod_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    uint32_t        fid;                    /* PFS file ID                  */
    uint32_t        mode;                   /* File mode                    */
};

struct  rfs_chown_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    uint32_t        owner;                  /* Owner ID                     */
    uint32_t        group;                  /* Group ID                     */
    struct varstr   name;                   /* File name                    */
};

struct  rfs_fchown_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    uint32_t        fid;                    /* PFS file ID                  */
    uint32_t        owner;                  /* Owner ID                     */
    uint32_t        group;                  /* Group ID                     */
};

struct  rfs_utime_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    struct utimes_wire times;               /* acc and mod time structure */
    struct varstr   name;                   /* File name                    */
};

struct  rfs_trunc_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */

    uint64_t        len;                    /* Target length                */
    uint32_t        fid;                    /* PFS file ID                  */
};

struct  rfs_unlink_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    struct varstr   name;                   /* File name                    */
};

struct  rfs_rename_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    struct varstr   oldn;                   /* Old file name                */
    struct varstr   newn;                   /* New file name                */
};

struct  rfs_rmdir_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    struct varstr   name;                   /* File name                    */
};

struct  rfs_mkdir_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    uint32_t        mode;                   /* File mode                    */
    struct varstr   name;                   /* File name                    */
};

/**
 * replies with rfs_gen_reply: for success or failure, for LFS it's more like:
 * Does this directory exist?
 */
struct  rfs_chdir_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    struct varstr   name;                   /* File name                    */
};

struct  rfs_fchdir_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    uint32_t        fid;                    /* PFS file ID                  */
};

struct  rfs_fchdir_reply {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    int32_t        rc;
    uint32_t        fid;                    /* PFS file ID                  */
    struct varstr   name;                   /* Name of dir::fid             */
};

struct  rfs_readdir_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        fid;                    /* PFS file ID                  */
    uint32_t        len;                    /* Read and return size ptr     */
    uint64_t        off;                    /* File offset ptr              */
};

struct  rfs_readdir_reply {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    int32_t        rc;
    uint32_t        fid;                    /* PFS file ID                  */
    uint32_t        len;                    /* Read and return size ptr     */
    uint64_t        ret;                    /* Return offset ptr            */
    struct dirent_wire   dirent;         /* Directory entry              */

};

struct  rfs_statvfs_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    struct varstr   name;                   /* File name                    */
};

struct  rfs_fstatvfs_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    uint32_t        fid;                    /* PFS file ID                  */
};

struct  rfs_statvfs_reply {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    int32_t        rc;
    struct statvfs_wire  statvfs;           /* statvfs structure */
};

struct  rfs_ioctl_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    uint32_t        fid;                    /* PFS file ID                  */
    uint32_t        flags;                  /* Open flags                   */
    uint32_t        dcmd;                   /* Command to device            */
    struct blob     data;                   /* Ptr to in/out data           */
};

struct  rfs_ioctl_reply {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    int32_t         rc;
    uint32_t        fid;                    /* PFS file ID                  */
    uint32_t        dcmd;                   /* Command to device            */
    struct blob     data;                   /* Ptr to in/out data           */
};

#define LFS_SELECT_READ   1
#define LFS_SELECT_WRITE  2
#define LFS_SELECT_ERROR  4

struct  selreq_wire {
    uint32_t        fildes;                 /* File descriptor              */
    uint8_t         status;                 /* Select status (r | w | e)    */
    uint8_t         request;                /* Select request (r | w | e)   */
};


struct  rfs_select_req {
    uint16_t        code;                   /* Command code                 */
    uint16_t        reqId;                  /* requestId, like transactionId */
    uint32_t        pflags;                 /* PFS flags & index            */
    uint32_t        fid;                    /* PFS file ID                  */
    uint32_t        flags;                  /* Open flags                   */
    uint32_t        count;
    struct selreq_wire selreq[1];           /* Select request block         */
};

struct  rfs_select_reply {
    uint16_t        code;         /* Command code */
    uint16_t        reqId;        /* requestId, like transactionId */
    uint32_t        rc;           /* Return code (error) */
    uint32_t        count;        /* Number of entries in selreq */
    struct selreq_wire selreq[1]; /* Select request block */
};

struct rfs_attach_req {
    uint16_t		code;
    uint16_t		reqId;
    uint32_t		mtu;		/* the maximum transfer unit */
    struct varstr	path;		/* the exported path */
};

struct rfs_fcntl_req {
    uint16_t        code;           /* Command code                  */
    uint16_t        reqId;          /* requestId, like transactionId */
    uint32_t        fid;            /* PFS file ID                   */
    uint32_t        flags;          /* Open flags                    */
};

#ifdef COMPONENT_TEST
struct rfs_perftest_req {
    uint16_t        code;         /* Command code */
    uint16_t        reqId;        /* requestId, like transactionId */
    uint16_t        size;         /* Size of the buffer */
    char            buf[1];       /* Performance test buffer */
};

struct rfs_perftest_reply {
    uint16_t        code;         /* Command code */
    uint16_t        reqId;        /* requestId, like transactionId */
    uint16_t        size;         /* Size of the buffer */
    char            buf[1];       /* Performance test buffer */
};
#endif

/* PRQA: QAC Message 750: A union is used to combine different messages */
/* PRQA S 750 L3 */

union   rfs_pdu {
    uint16_t                    code;
    struct  rfs_gen_req         r_gen;
    struct  rfs_gen_reply       r_gen_reply;
    struct  rfs_open_req        r_open;
    struct  rfs_close_req       r_close;
    struct  rfs_read_req        r_read;
    struct  rfs_write_req       r_write;
    struct  rfs_ioctl_req       r_ioctl;
    struct  rfs_link_req        r_link;
    struct  rfs_fsync_req       r_fsync;
    struct  rfs_access_req      r_access;

    struct  rfs_fstat_req       r_fstat;
    struct  rfs_stat_req        r_stat;
    struct  rfs_chmod_req       r_chmod;
    struct  rfs_fchmod_req      r_fchmod;
    struct  rfs_chown_req       r_chown;
    struct  rfs_fchown_req      r_fchown;
    struct  rfs_utime_req       r_utime;
    struct  rfs_trunc_req       r_trunc;
    struct  rfs_unlink_req      r_unlink;
    struct  rfs_rename_req      r_rename;
    struct  rfs_rmdir_req       r_rmdir;
    struct  rfs_mkdir_req       r_mkdir;
    struct  rfs_chdir_req       r_chdir;
    struct  rfs_fchdir_req      r_fchdir;
    struct  rfs_select_req      r_select;
    struct  rfs_readdir_req     r_readdir;
    struct  rfs_statvfs_req     r_statvfs;
    struct  rfs_fstatvfs_req    r_fstatvfs;
    struct  rfs_attach_req      r_attach;
    struct  rfs_fcntl_req       r_fcntl;
#ifdef COMPONENT_TEST
    struct  rfs_perftest_req    r_perftest;
#endif

    struct  rfs_open_reply      r_open_reply;
    struct  rfs_read_reply      r_read_reply;
    struct  rfs_write_reply     r_write_reply;
    struct  rfs_stat_reply      r_stat_reply;
    struct  rfs_fchdir_reply    r_fchdir_reply;
    struct  rfs_select_reply    r_select_reply;
    struct  rfs_readdir_reply   r_readdir_reply;
    struct  rfs_statvfs_reply   r_statvfs_reply;
    struct  rfs_ioctl_reply     r_ioctl_reply;
#ifdef COMPONENT_TEST
    struct  rfs_perftest_reply  r_perftest_reply;
#endif
};

typedef union rfs_pdu rfs_pdu_t;


#ifdef __cplusplus
}
#endif

#endif  /* _RFS_MSG_H_ */
