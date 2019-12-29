/**
 * \file: hid_com_server.h
 *
 * Convenience wrapper for hid communication
 *
 * \component: hid class wrapper
 *
 * \author: SErhard
 *
 * \copyright: (c) 2003 - 2011 ADIT Corporation
 *
 * This Software is under a dual license (MIT/GPL).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef USB_COMMAND_H_
#define USB_COMMAND_H_

//#ifdef __cplusplus
//	extern "C" {
//#endif

#include <adit_typedef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

//#ifndef S32
//#define S32 int
//#endif

//#ifndef U32
//#define U32 unsigned int
//#endif

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

/*
 * names of abstract unix sockets
 */
#define CTRL_SCK_NAME       "xHidCtrlSock"
#define EVNT_SCK_NAME       "xHidEvntSock_0xXX"
#define EVNT_SCK_NAME_TMPL  "xHidEvntSock_0x%02x"

#if !defined(_HidSck_)

#define SCK_BUFF_MAX_ELEM 512    /* USB_MBF_MAXMSZ / sizeof(S32) */
#define SCK_BUFF_MAX (sizeof(S32)*SCK_BUFF_MAX_ELEM)

#define HID_MSG_CMD         0
#define HID_MSG_IPOD        1
#define HID_MSG_THR         1
#define HID_MSG_DD          1
#define HID_MSG_RC          1
			    
#define HID_MSG_BYTES       2
#define HID_MSG_ID          3
#define HID_MSG_VAL         4

#define HID_MSG_REPORT_ID   2
#define HID_MSG_REPORT_LEN  3
#define HID_MSG_REPORT_DATA 4

#define HID_MSG_VENDOR      2
#define HID_MSG_PRODUCT     3

#define HID_MSG_DEV_LEN     2
#define HID_MSG_DEV         3
#define HID_MSG_DD_RET      2

#define HID_MSG_NEXT_LEN    2
#define HID_MSG_NEXT_POS    3
#define HID_MSG_NEXT_DATA   4

#define HID_MSG_SIZE_IN     2
#define HID_MSG_SIZE_OUT    3


#define HID_LEN_CMD        sizeof(S32)
#define HID_LEN_THR        sizeof(S32)
#define HID_LEN_DD         sizeof(S32)
#define HID_LEN_RC         sizeof(S32)
#define HID_LEN_BYTES      sizeof(S32)
#define HID_LEN_REPORT_ID  sizeof(S32)
#define HID_LEN_VALUE      sizeof(S32)
#define HID_LEN_REPORT_LEN sizeof(S32)
#define HID_LEN_VENDOR     sizeof(S32)
#define HID_LEN_PRODUCT    sizeof(S32)
#define HID_LEN_DEV_LEN    sizeof(S32)
#define HID_LEN_DD_RET     sizeof(S32)
#define HID_LEN_SIZE_IN    sizeof(S32)
#define HID_LEN_SIZE_OUT   sizeof(S32)


#define HID_LEN_WRITE_REPORT \
	(HID_LEN_CMD + HID_LEN_DD + HID_LEN_REPORT_ID + HID_LEN_REPORT_LEN)

#define HID_LEN_OPEN \
	(HID_LEN_CMD + sizeof(S32) + HID_LEN_DEV_LEN)

/*
 * general socket configuration
 */
struct HidSck
{
    S32	Sck;	                /* socket handle */
    S32 SckBnd;                 /* handle to client */
    struct sockaddr_un SckAddr; /* socket address config (not null terminated) */
    socklen_t SckAddrLen;       /* socket address len */
    S32 SckBuff[SCK_BUFF_MAX];  /* transfer buffer */
    U32 SckBuffTransLen;        /* size of bytes to be send / received */
    S32 cmd;
    int dd;
    S32 len;
    S32 cur;
};
#  define _HidSck_ 1
#endif /* !define(_HidSck_) */

struct HIDDevComs
{
	S32            NumEvntThrs;
	struct HidSck *pDevScks;
};

enum _retValues
{
    HID_OK   =  0,
    HID_FAIL = -1,
} retValues;

enum ReportType
{
    REPORT_ID_IN = 0,
    REPORT_ID_OUT = 1,
    REPORT_ID_MAX = 2,
};

/*
 * list of commands supported by usb_server
 */
enum UsbComTypes
{
    HID_CMD_NOP             = 0,
    HID_CMD_INIT            = 1,
    HID_CMD_RD_DEV_PROP     = 2,
    HID_CMD_RD_REP_ID       = 3,
    HID_CMD_WR_REP          = 4,

    HID_CMD_OPEN            = 6,
    HID_CMD_CLOSE           = 7,
    HID_CMD_NEXT_MSG_WAIT   = 8,
    HID_CMD_NEXT_MSG        = 9,
};

//#ifdef __cplusplus
//}
//#endif

#endif /* USB_COMMAND_H_ */
