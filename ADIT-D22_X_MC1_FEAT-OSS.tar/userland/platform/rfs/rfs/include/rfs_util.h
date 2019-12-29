/*
 *   rfs_util.h
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

#ifndef _RFS_UTIL_H_
#define _RFS_UTIL_H_

extern int debug;
char *code2str(int code);
#define dprintk(level, fmt, args...) if (debug >= level) printk(fmt, ##args)
#define DBG_0	0
#define DBG_1	1
#define DBG_2	2
#define DBG_3	3

int wire2vfs(rfs_pdu_t *wire, rfs_pdu_t *ret_reply, int maxsize);
int fd_close(unsigned int fd);
uint16_t get_new_rid(void);


#endif
