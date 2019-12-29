/**
 * \file: hid_com_server.c
 *
 * convinience wrapper for hid communication
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

/*
 * glibc & kernel includes
 */

#include <sys/prctl.h>
#include <sys/ioctl.h>
/*
 * socket includes
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
/*
 * wrapper includes
 */
#include "hid_com_server.h"
/*
 * hid-usb includes
 */
#include <fcntl.h>
#include <linux/hiddev.h>
#include <errno.h>

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include <syslog.h>

#define FAILED           -1
#define EXPECTED_IPODS   16

#define CRIT (LOG_CRIT| LOG_PERROR)
#define NOTI (LOG_NOTICE)

#define COMP             "hid_com: "

#define min(x, y) ((x) > (y) ? (y) : (x))

static int   g_log_fd = FAILED;

static void dumpf(int fd, const char* fmt, ...)
{
	va_list args;
	char    buf[256];
	int     len      = 0;

	if (FAILED != fd)
	{
		va_start(args, fmt);
		len = vsnprintf(buf, sizeof(buf), fmt, args);
		va_end(args);
		
		if (len > 0)
		{
			if (((unsigned int)len) > sizeof(buf))
			{
				len = sizeof(buf);
			}
			write(fd, buf, len);
		}
	}
}

static void dumph(int fd, void *data_, int len)
{
	char	*data    = data_;
	char     hex[]   = "0123456789ABCDEF";
	char     digit[] =  {' ', ' ', ' '};

	if (FAILED != fd)
	{
		while (0 < len)
		{
			digit[0] = hex[0xf & ((*data) >> 4)];
			digit[1] = hex[0xf & (*data)];
			write(fd, digit, sizeof(digit));
			data++;
			len--;
		}
	}
}

static void HIDCloseNoIntr(int *Sck)
{
	if (FAILED != (*Sck))
	{
		int rc = 0;
		do
		{
			errno = 0;
			rc    = close(*Sck);
		}
		while ((FAILED == rc) && (EINTR == errno));
		
		(*Sck) = FAILED;
	}
}

static int InitEp(struct HidSck *pCtrlSck, int *ep)
{
	struct epoll_event	ev = {.events = EPOLLIN, .data.fd = pCtrlSck->Sck};
	int			rc = FAILED;

	(*ep) = epoll_create(EXPECTED_IPODS);
	if (FAILED != (*ep))
	{
		rc = epoll_ctl((*ep), EPOLL_CTL_ADD, pCtrlSck->Sck, &ev);
		if ((FAILED != rc) && (FAILED != pCtrlSck->SckBnd))
		{
			ev.data.fd = pCtrlSck->SckBnd;
			rc = epoll_ctl((*ep), EPOLL_CTL_ADD, pCtrlSck->SckBnd, &ev);
		}
	}
	return rc;
}
	
static void HidSckSrvTx(struct HidSck *pHidSck, char* type);
static void HidSckSrvTx(struct HidSck *pHidSck, char* type)
{
	int rc = send(pHidSck->SckBnd, pHidSck->SckBuff, pHidSck->SckBuffTransLen, 0);

	if (FAILED == rc)
	{
		dumpf(g_log_fd, COMP "%s 0x%08x tx: %s\n", type, pHidSck, strerror(errno));
	}
	else
	{
		dumpf(g_log_fd, COMP "%s 0x%08x tx: [ ", type, pHidSck);
		dumph(g_log_fd, pHidSck->SckBuff, min(80, pHidSck->SckBuffTransLen));
		dumpf(g_log_fd, "]\n");
	}
}

static void HIDDeInit(struct HIDDevComs *psDevComs, int *ep)
{
	S32 i = 0;

	if (NULL != psDevComs->pDevScks)
	{
		for (i = 0; i< psDevComs->NumEvntThrs; i++)
		{
			struct HidSck *pSck = &psDevComs->pDevScks[i];
			if (FAILED != pSck->Sck)
			{
				(void)epoll_ctl((*ep), EPOLL_CTL_DEL, pSck->Sck, NULL);
				HIDCloseNoIntr(&pSck->Sck);
			}
			if (FAILED != pSck->SckBnd)
			{
				(void)epoll_ctl((*ep), EPOLL_CTL_DEL, pSck->SckBnd, NULL);
				HIDCloseNoIntr(&pSck->SckBnd);
			}
			if (FAILED != pSck->dd)
			{
				(void)epoll_ctl((*ep), EPOLL_CTL_DEL, pSck->dd, NULL);
				HIDCloseNoIntr(&pSck->dd);
			}
		}
		free(psDevComs->pDevScks);
		psDevComs->pDevScks = NULL;
	}
}

static void HIDInitRet(struct HidSck *pHidSck, int res);
static void HIDInitRet(struct HidSck *pHidSck, int res)
{
	S32 *msg = pHidSck->SckBuff;

	msg[HID_MSG_CMD]	 = HID_CMD_INIT;
	msg[HID_MSG_RC]		 = res;
	pHidSck->SckBuffTransLen = HID_LEN_CMD + HID_LEN_RC;
	
	HidSckSrvTx(pHidSck, "ctrl");
}

static void HIDInit(struct HidSck *pCtrlSck, struct HIDDevComs *psDevComs, int *ep)
{
	S32	*msg = pCtrlSck->SckBuff;
	S32	 res = HID_FAIL;

	HIDDeInit(psDevComs, ep);

	if ((HID_LEN_CMD + HID_LEN_THR) == pCtrlSck->SckBuffTransLen)
	{
		psDevComs->NumEvntThrs = msg[HID_MSG_THR];
		psDevComs->pDevScks    = malloc(psDevComs->NumEvntThrs
						* sizeof(*psDevComs->pDevScks));
		if (NULL != psDevComs->pDevScks)
		{
			S32 i = 0;
			res = HID_OK;

			for (i = 0; ((i < psDevComs->NumEvntThrs) && (HID_OK == res)); i++)
			{
				struct HidSck	*pSck = &psDevComs->pDevScks[i];
				int		 rc   = 0;
				pSck->Sck    = FAILED;
				pSck->SckBnd = FAILED;
				pSck->SckAddr.sun_family = AF_UNIX;
				snprintf(pSck->SckAddr.sun_path,
					 sizeof(pSck->SckAddr.sun_path),
					 EVNT_SCK_NAME_TMPL,
					 i);
				pSck->SckAddrLen = SUN_LEN(&pSck->SckAddr);
				pSck->SckAddr.sun_path[0] = 0;

				rc = socket(PF_UNIX, SOCK_STREAM, 0);
				pSck->Sck = rc;
				if (FAILED != pSck->Sck)
				{
					rc = bind(pSck->Sck, (struct sockaddr *)&pSck->SckAddr,
						  pSck->SckAddrLen);
					if (FAILED != rc)
					{
						rc = listen(pSck->Sck, 1);
						if (FAILED != rc)
						{
							struct epoll_event ev = {.events  = EPOLLIN,
										 .data.fd = pSck->Sck};
							rc = epoll_ctl((*ep), EPOLL_CTL_ADD, pSck->Sck, &ev);
						}
					}
				}
				if (FAILED == rc)
				{
					res = HID_FAIL;
				}
				pSck->cmd = FAILED;
				pSck->dd  = FAILED;
			}
		}
	}
	HIDInitRet(pCtrlSck, res);
}

static void HIDOpen(struct HidSck *pCtrlSck, struct HIDDevComs	*psEvntThrs)
{
	S32	*msg = pCtrlSck->SckBuff;
	S32	 res = HID_FAIL;
	int	 dd  = FAILED;

	if ((HID_LEN_OPEN <= pCtrlSck->SckBuffTransLen)
	    &&
	    ((HID_LEN_OPEN + msg[HID_MSG_DEV_LEN]) == pCtrlSck->SckBuffTransLen))
	{
        struct HidSck *pHidSck = &psEvntThrs->pDevScks[msg[HID_MSG_IPOD]];

		dd = open((char*)&msg[HID_MSG_DEV], O_RDONLY);
		if (FAILED != dd)
		{
			S32	flag = HIDDEV_FLAG_UREF;
			int	rc   = ioctl(dd, HIDIOCSFLAG, &flag);
			if (FAILED != rc)
			{
				pHidSck->cmd = HID_CMD_OPEN;
				pHidSck->dd  = dd;
				res = HID_OK;
			}
			else
			{
				HIDCloseNoIntr(&dd);
			}
		}
	}

	msg[HID_MSG_RC]		 = res;
	msg[HID_MSG_DD_RET]	 = dd;
	pCtrlSck->SckBuffTransLen = HID_LEN_CMD + HID_LEN_RC + HID_LEN_DD_RET;

	HidSckSrvTx(pCtrlSck, "evnt");
}

static void HIDClose(struct HidSck *pCtrlSck, struct HIDDevComs	*psEvntThrs, int ep)
{
	S32	*ctrl_msg = pCtrlSck->SckBuff;
	S32	 res = HID_FAIL;

	if (HID_LEN_OPEN == pCtrlSck->SckBuffTransLen)
	{
		struct HidSck *pHidSck = &psEvntThrs->pDevScks[ctrl_msg[HID_MSG_IPOD]];
		S32 *evnt_msg = pHidSck->SckBuff;

		if (FAILED != pHidSck->dd)
		{
			res = HID_OK;

            if ((HID_CMD_NEXT_MSG_WAIT == pHidSck->cmd)
                ||
                (HID_CMD_NEXT_MSG      == pHidSck->cmd))
            {
                evnt_msg[HID_MSG_CMD]         = pHidSck->cmd;
                evnt_msg[HID_MSG_RC]          = HID_FAIL;
                pHidSck->SckBuffTransLen = HID_LEN_CMD + HID_LEN_RC;
                
                HidSckSrvTx(pHidSck, "evnt");
            }
			(void)epoll_ctl(ep, EPOLL_CTL_DEL, pHidSck->dd, NULL);

			HIDCloseNoIntr(&pHidSck->dd);
			pHidSck->cmd = HID_CMD_CLOSE;
			pHidSck->dd  = FAILED;
		}
	}
	ctrl_msg[HID_MSG_RC]		 = res;
	pCtrlSck->SckBuffTransLen = HID_LEN_CMD + HID_LEN_RC;
		
	HidSckSrvTx(pCtrlSck, "ctrl");
}

static void HIDGetNextMessageWaitPrepare(struct HidSck *pHidSck, int ep)
{
    S32 *msg = pHidSck->SckBuff;
    S32  res = HID_FAIL;

    if ((HID_LEN_CMD + HID_LEN_DD) == pHidSck->SckBuffTransLen)
    {
	    int         	dd = msg[HID_MSG_DD];
	    struct epoll_event  ev = {.events  = EPOLLIN, .data.fd = dd};
	    int                 rc = epoll_ctl(ep, EPOLL_CTL_ADD, dd, &ev);
	    
	    if (rc != FAILED)
	    {
		    pHidSck->cmd = msg[HID_MSG_CMD];
		    pHidSck->dd  = dd;
		    res = HID_OK;
	    }
	    else
	    {
		    syslog(CRIT, "%s: %m", "EPOLL_CTL_ADD(dd)() failed");
		    dumpf(g_log_fd, COMP "0x%08x EPOLL_CTL_ADD(dd)(): %s\n",
                  pHidSck, strerror(errno));
	    }

    }
    if (HID_FAIL == res)
    {
	    msg[HID_MSG_RC]          = HID_FAIL;
	    pHidSck->SckBuffTransLen = HID_LEN_CMD + HID_LEN_RC
		    + HID_LEN_BYTES + HID_LEN_REPORT_ID + HID_LEN_VALUE;
	    
	    HidSckSrvTx(pHidSck, "evnt");
    }
}

static void HIDGetNextMessageWait(struct HidSck *pHidSck, int ep)
{
    S32				*msg	   = pHidSck->SckBuff;
    S32				 res	   = HID_FAIL;
    int				 dd        = pHidSck->dd;
    int				 bytesread = 0;
    struct hiddev_usage_ref	 data;

    pHidSck->cmd = HID_CMD_OPEN;
    (void)epoll_ctl(ep, EPOLL_CTL_DEL, pHidSck->dd, NULL);
	    
    bytesread = read(dd, &data, sizeof(data));
    if ((sizeof(data) == bytesread) && (0 == data.usage_index))
    {
	    res = HID_OK;
	    
	    msg[HID_MSG_BYTES] = sizeof(data);
	    msg[HID_MSG_ID]    = data.report_id;
	    msg[HID_MSG_VAL]   = data.value;
    }
    else
    {
	    syslog(NOTI, "GetNextMessageWait: read=%d, index=%d: %m",
		   bytesread, data.usage_index);
	    dumpf(g_log_fd, COMP "GetNextMessageWait: read=%d, index=%d, %s\n",
 		  bytesread, data.usage_index, strerror(errno));
    }

    msg[HID_MSG_RC]          = res;
    pHidSck->SckBuffTransLen = HID_LEN_CMD + HID_LEN_RC
	    + HID_LEN_BYTES + HID_LEN_REPORT_ID + HID_LEN_VALUE;

    HidSckSrvTx(pHidSck, "evnt");
}

static void HIDGetNextMessagePrepare(struct HidSck *pHidSck, int ep)
{
	int *msg = pHidSck->SckBuff;
	S32  res = HID_FAIL;

	if ((HID_LEN_CMD + HID_LEN_DD + HID_LEN_REPORT_LEN) == pHidSck->SckBuffTransLen)
	{
		int dd = msg[HID_MSG_DD];

		struct epoll_event  ev = {.events  = EPOLLIN, .data.fd = dd};
		int                 rc = epoll_ctl(ep, EPOLL_CTL_ADD, dd, &ev);

		if (rc != FAILED)
		{
			pHidSck->cmd = msg[HID_MSG_CMD];
			pHidSck->dd  = dd;
			pHidSck->len = msg[HID_MSG_NEXT_LEN];
			pHidSck->cur = 0;
			res = HID_OK;
		}
		else
		{
			syslog(CRIT, "%s: %m", "EPOLL_CTL_ADD(dd)() failed");
			dumpf(g_log_fd, COMP "0x%08x EPOLL_CTL_ADD(dd)(): %s\n",
			      pHidSck, strerror(errno));
		}
	}
	if (HID_FAIL == res)
	{
		msg[HID_MSG_RC]          = HID_FAIL;
		pHidSck->SckBuffTransLen = HID_LEN_CMD + HID_LEN_RC
			+ HID_LEN_REPORT_LEN + HID_LEN_REPORT_LEN;
	    
		HidSckSrvTx(pHidSck, "evnt");
	}
}

static void HIDGetNextMessage(struct HidSck *pHidSck, int ep)
{
	int *msg   = pHidSck->SckBuff;
	S32  dd    = pHidSck->dd;
	S32  len   = pHidSck->len;
	S32  cur   = pHidSck->cur;
	U8  *data  = (U8*)&msg[HID_MSG_NEXT_DATA];

	struct hiddev_usage_ref *uref = malloc(len * sizeof(uref[0]));
	int rc	= 0;
	U32 i       = 0;
	S32 reports = 0;

	msg[HID_MSG_RC] = HID_FAIL;

	if (NULL != uref)
	{
		rc = read(dd, uref, (len - cur) * sizeof(uref[0]));
		if (FAILED != rc)
		{
			reports		= rc / sizeof(uref[0]);
			msg[HID_MSG_RC] = HID_OK;

			for (i = 0; ((i < (U32)reports) && (HID_OK == msg[HID_MSG_RC])); i++)
			{
				data[cur + i] = (U8)uref[i].value;

				if (uref[i].usage_index != (cur + i + 1))
				{
					msg[HID_MSG_RC] = HID_FAIL;
				}
			}
			cur	     += reports;
			pHidSck->cur  = cur;
		}
	}

	if ((msg[HID_MSG_RC] == HID_FAIL) || (cur == len))
	{		
        pHidSck->cmd = HID_CMD_OPEN;
		(void)epoll_ctl(ep, EPOLL_CTL_DEL, pHidSck->dd, NULL);

		msg[HID_MSG_NEXT_LEN]     = len;
		msg[HID_MSG_NEXT_LEN + 1] = len;
		pHidSck->SckBuffTransLen  = HID_LEN_CMD + HID_LEN_RC
			+ HID_LEN_REPORT_LEN + HID_LEN_REPORT_LEN
			+ len * sizeof(U8);
		
		HidSckSrvTx(pHidSck, "evnt");
	}

	if (NULL != uref)
	{
		free(uref);
	}
}

static void HIDUnknownRet(struct HidSck *pHidSck);
static void HIDUnknownRet(struct HidSck *pHidSck)
{
	S32 *msg = pHidSck->SckBuff;

	msg[HID_MSG_RC]		 = HID_FAIL;
	pHidSck->SckBuffTransLen = HID_LEN_CMD + HID_LEN_RC;
	
	HidSckSrvTx(pHidSck, "ctrl");
}

static void HIDReadDeviceProperty(struct HidSck *pHidSck);
static void HIDReadDeviceProperty(struct HidSck *pHidSck)
{
	S32 *msg = pHidSck->SckBuff;
	S32 res  = HID_FAIL;
	struct hiddev_devinfo device_info = {};

	/* get hid device information */
	if ((HID_LEN_CMD + HID_LEN_DD) == pHidSck->SckBuffTransLen)
	{
		int rc = ioctl(msg[HID_MSG_DD], HIDIOCGDEVINFO, &device_info);
		if (FAILED != rc)
		{
			res  = HID_OK;
		}
	}

	msg[HID_MSG_RC]      = res;
	msg[HID_MSG_VENDOR]  = device_info.vendor;
	msg[HID_MSG_PRODUCT] = device_info.product;

	pHidSck->SckBuffTransLen = HID_LEN_CMD + HID_LEN_RC
		+ HID_LEN_VENDOR + HID_LEN_PRODUCT;
	HidSckSrvTx(pHidSck, "ctrl");
}

static void HIDReadReportID(struct HidSck *pHidSck);
static void HIDReadReportID(struct HidSck *pHidSck) 
{
	S32 *msg = pHidSck->SckBuff;
	int dd = (int)msg[HID_MSG_DD];
	struct hiddev_report_info rinfo[REPORT_ID_MAX];
	struct hiddev_field_info finfo[REPORT_ID_MAX];
	U32 i = 0;
	U32 loop = 0;
	U32 pack_size = 0;
	int rc = 0;
	int res = HID_FAIL;

	/*
	 * 0 == cmd,
	 * 1 == rc,
	 * 2 == size_in,
	 * 3 == size_out,
	 * in_data * x elements,
	 * out_data * x elements
	 */
	S32 buf_iterator = 4;

	if ((HID_LEN_CMD + HID_LEN_DD) == pHidSck->SckBuffTransLen)
	{
		rinfo[REPORT_ID_IN].report_type = HID_REPORT_TYPE_INPUT;
		rinfo[REPORT_ID_OUT].report_type = HID_REPORT_TYPE_OUTPUT;

		for (loop = 0; loop < (S32)REPORT_ID_MAX; loop++) {
			/* save the number of reports and return the value
			 * for the caller to allocate enough memory
			 */
			pHidSck->SckBuff[2+loop] = 0;

			rinfo[loop].report_id = HID_REPORT_ID_FIRST;

			while (ioctl(dd, HIDIOCGREPORTINFO, &rinfo[loop]) >= 0)
			{
				for (i = 0; i < rinfo[loop].num_fields; i++)
				{
					memset(&finfo[loop], 0, sizeof(struct hiddev_field_info));
					finfo[loop].report_type = rinfo[loop].report_type;
					finfo[loop].report_id = rinfo[loop].report_id;
					finfo[loop].field_index = i;

					rc = ioctl(dd, HIDIOCGFIELDINFO, &finfo[loop]);

					pHidSck->SckBuff[buf_iterator] = (S32)finfo[loop].maxusage;
					buf_iterator++;

					pHidSck->SckBuff[buf_iterator] = (S32)finfo[loop].report_id;
					buf_iterator++;
				}
				rinfo[loop].report_id |= HID_REPORT_ID_NEXT;
				pHidSck->SckBuff[2+loop] += (S32)rinfo[loop].num_fields;
			}
		}
		res = HID_OK;
	}

	msg[HID_MSG_RC] = res;
	pack_size  = HID_LEN_CMD + HID_LEN_RC + HID_LEN_SIZE_IN + HID_LEN_SIZE_OUT;
	pack_size += 2 * sizeof(S32) * msg[HID_MSG_SIZE_IN];  /* (maxusage, report_id)*entries) */
	pack_size += 2 * sizeof(S32) * msg[HID_MSG_SIZE_OUT]; /* (maxusage, report_id)*entries) */

	pHidSck->SckBuffTransLen = pack_size;

	HidSckSrvTx(pHidSck, "ctrl");
}

static void HIDWriteReport(struct HidSck *pHidSck);
static void HIDWriteReport(struct HidSck *pHidSck)
{
	S32 *msg = pHidSck->SckBuff;
	S32 res  = HID_FAIL;

	if ((HID_LEN_WRITE_REPORT <= pHidSck->SckBuffTransLen)
	    &&
	    ((HID_LEN_WRITE_REPORT + msg[HID_MSG_REPORT_LEN])
	     == pHidSck->SckBuffTransLen))
	{
		U8				*data  = (U8*)&msg[HID_MSG_REPORT_DATA];
		S32				 j     = 0;
		struct hiddev_usage_ref_multi	 uref  = {};
		struct hiddev_report_info	 rinfo = {};
		int rc = 0;

		uref.uref.report_type = HID_REPORT_TYPE_OUTPUT;
		uref.uref.report_id   = msg[HID_MSG_REPORT_ID];
		uref.uref.usage_code  = 0x01;
		uref.num_values	      = msg[HID_MSG_REPORT_LEN];

		for (j = 0; j < msg[HID_MSG_REPORT_LEN]; j++)
		{
			uref.values[j] = (S32)data[j];
		}

		rc = ioctl(msg[HID_MSG_DD], HIDIOCSUSAGES, &uref);
		if (FAILED != rc)
		{
			rinfo.report_type = HID_REPORT_TYPE_OUTPUT;
			rinfo.report_id	  = msg[HID_MSG_REPORT_ID];
			rinfo.num_fields  = 1;

			rc = ioctl(msg[HID_MSG_DD], HIDIOCSREPORT, &rinfo);
			if (FAILED != rc)
			{
				res = HID_OK;
			}
		}
	}

	msg[HID_MSG_RC]		 = res;
	pHidSck->SckBuffTransLen = HID_LEN_CMD + HID_LEN_RC;

	HidSckSrvTx(pHidSck, "ctrl");
}

static void HidAccept(struct HidSck *pCtrlSck, int ep);
static void HidAccept(struct HidSck *pCtrlSck, int ep)
{
	int rc = accept(pCtrlSck->Sck, NULL, NULL);
	if (FAILED != rc)
	{
		(void)epoll_ctl(ep, EPOLL_CTL_DEL, pCtrlSck->SckBnd, NULL);
		HIDCloseNoIntr(&pCtrlSck->SckBnd);
		pCtrlSck->SckBnd = rc;

		rc = fcntl(pCtrlSck->SckBnd, F_SETFL, O_NONBLOCK);
		if (FAILED != rc)
		{
			struct epoll_event ev = {.events  = EPOLLIN,
						 .data.fd = pCtrlSck->SckBnd};

			rc = epoll_ctl(ep, EPOLL_CTL_ADD, pCtrlSck->SckBnd, &ev);
			if (FAILED == rc)
			{
				syslog(CRIT, "%s: %m", "EPOLL_CTL_ADD(client)() failed");
				dumpf(g_log_fd, COMP "0x%08x EPOLL_CTL_ADD(client)(): %s\n",
				      pCtrlSck, strerror(errno));
				HIDCloseNoIntr(&pCtrlSck->SckBnd);
			}
		}
		else
		{
			syslog(CRIT, "%s: %m", "fnctl() failed");
			dumpf(g_log_fd, COMP "0x%08x fnctl(): %s\n", pCtrlSck, strerror(errno));
			HIDCloseNoIntr(&pCtrlSck->SckBnd);
		}
	}
	else
	{
		syslog(CRIT, "%s: %m", "accept() failed");
		dumpf(g_log_fd, COMP "0x%08x accept(): %s\n", pCtrlSck, strerror(errno));
	}
}

static void HidReceive(struct HidSck		*pSck,
		       struct HIDDevComs	*psEvntThrs,
		       int			*ep,
		       char                     *type)
{
	S32	*msg = pSck->SckBuff;
	int	 rc  = 0;

	msg[HID_MSG_CMD]	  = HID_CMD_NOP;
	pSck->SckBuffTransLen = SCK_BUFF_MAX;

	rc = recv(pSck->SckBnd, pSck->SckBuff, pSck->SckBuffTransLen, 0);
	pSck->SckBuffTransLen = rc;

	if ((FAILED == rc) || (0 == rc))
	{
		dumpf(g_log_fd, COMP "%s 0x%08x rx: rc=%#x %s\n", type, pSck, rc, strerror(errno));
		rc = epoll_ctl((*ep), EPOLL_CTL_DEL, pSck->SckBnd, NULL);
		if (FAILED == rc)
		{
			syslog(CRIT, "%s: %m", "EPOLL_CTL_DEL(pSck->SckBnd)");
			exit(EXIT_FAILURE);
		}

		HIDCloseNoIntr(&pSck->SckBnd);
		if (strcmp("evnt", type)) {
			HIDCloseNoIntr(&pSck->dd);
		}
	}
	else
	{
		dumpf(g_log_fd, COMP "%s 0x%08x rx: [ ", type, pSck);
		dumph(g_log_fd, pSck->SckBuff, min(80, pSck->SckBuffTransLen));
		dumpf(g_log_fd, "]\n");

		switch (msg[HID_MSG_CMD])
		{
		case HID_CMD_INIT:
			HIDInit(pSck, psEvntThrs, ep);
			break;
		case HID_CMD_RD_DEV_PROP:
			HIDReadDeviceProperty(pSck);
			break;
		case HID_CMD_RD_REP_ID:
			HIDReadReportID(pSck);
			break;
		case HID_CMD_WR_REP:
			HIDWriteReport(pSck);
			break;
		case HID_CMD_OPEN:
			HIDOpen(pSck, psEvntThrs);
			break;
		case HID_CMD_CLOSE:
			HIDClose(pSck, psEvntThrs, (*ep));
			break;
		case HID_CMD_NEXT_MSG_WAIT:
			HIDGetNextMessageWaitPrepare(pSck, (*ep));
			break;
		case HID_CMD_NEXT_MSG:
			HIDGetNextMessagePrepare(pSck, (*ep));
			break;
		default:
			HIDUnknownRet(pSck);
			syslog(NOTI, "%s: %#x", "received unkown cmd id:",
			       msg[HID_MSG_CMD]);
			rc = 0;
			break;
		}
	}
}

static int Process(struct HidSck *pCtrlSck);
static int Process(struct HidSck *pCtrlSck)
{
	struct HIDDevComs	 sDevComs  = {};
	int                      ep        = 0;
	int			 rc	   = socket(PF_UNIX, SOCK_STREAM, 0);

	pCtrlSck->Sck = rc;
	if (FAILED != rc)
	{
		rc = bind(pCtrlSck->Sck, (struct sockaddr *)&pCtrlSck->SckAddr,
			  pCtrlSck->SckAddrLen);
		if (FAILED != rc)
		{
			rc = listen(pCtrlSck->Sck, 1);
		}
	}
	if (FAILED == rc)
	{
		syslog(CRIT, "%s: %m", "server mode activation failed");
		return EXIT_FAILURE;
	}

	rc = InitEp(pCtrlSck, &ep);
	if (FAILED == rc)
	{
		syslog(CRIT, "%s: %m", "failed to add server socket to poll set");
		return EXIT_FAILURE;
	}

	while (1)
	{
		struct epoll_event events = {};

		rc = epoll_wait(ep, &events, 1, -1);
		if (1 != rc)
		{
			if (EINTR == errno)
			{
				continue;
			}
			syslog(CRIT, "%s: %m", "epoll_wait");
			return EXIT_FAILURE;
		}

		if (pCtrlSck->Sck == events.data.fd)
		{
			HidAccept(pCtrlSck, ep);
		}
		else if (pCtrlSck->SckBnd == events.data.fd)
		{
			HidReceive(pCtrlSck, &sDevComs, &ep, "ctrl");
		}
		else if (NULL != sDevComs.pDevScks)
		{
			int i = 0;
			for (i = 0; i < sDevComs.NumEvntThrs; i++)
			{
				struct HidSck *pSck = &sDevComs.pDevScks[i];
				if (pSck->Sck == events.data.fd)
				{
					HidAccept(pSck, ep);
				}
				else if (pSck->SckBnd == events.data.fd)
				{
					HidReceive(pSck, &sDevComs, &ep, "evnt");
				}
				else if (pSck->dd == events.data.fd)
				{
					if (HID_CMD_NEXT_MSG_WAIT == pSck->cmd)
					{
						HIDGetNextMessageWait(pSck, ep);
					}
					else if (HID_CMD_NEXT_MSG == pSck->cmd)
					{
						HIDGetNextMessage(pSck, ep);
					}
				}
			}
		}
	}
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	struct sigaction ignore  = {.sa_handler = SIG_IGN, .sa_flags = SA_RESTART};
	struct HidSck CtrlSck = {.Sck     = FAILED,
				 .SckBnd  = FAILED,
				 .SckAddr = {.sun_family = AF_UNIX,
					     .sun_path   = CTRL_SCK_NAME}
	};
	int	fd       = 0;
	int	rc       = 0;
	char    prefix[] = "hid_com [4294967295]";

	(void)snprintf(prefix, sizeof(prefix), "hidcom[%d]", getpid());
	openlog(prefix, LOG_CONS, 0);

	syslog(NOTI, "%s", "Starting");

	/* Prepare as a daemon. */
	chdir("/");
	fd = open("/dev/null", O_RDWR);
	if (FAILED != fd)
	{
		rc = dup2(fd, STDIN_FILENO);
		rc = dup2(fd, STDOUT_FILENO);
		rc = close(fd);
	}
	rc = umask(027);

	/* Ignore "broken pipe" signals. */
	rc = sigaction(SIGPIPE, &ignore, NULL);
	if (FAILED == rc)
	{
		syslog(CRIT, "%s: %m", "sigaction for SIGPIPE failed");
		return EXIT_FAILURE;
	}

	/* Anonymus sockets have 0 as first byte, but SUN_LEN() stumples over it. */
	CtrlSck.SckAddrLen          = SUN_LEN(&CtrlSck.SckAddr);
	CtrlSck.SckAddr.sun_path[0] = 0;

	/* Initialise logging. */
	if (2 <= argc)
	{
		g_log_fd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC,
				S_IRUSR | S_IWUSR);
	}

	return Process(&CtrlSck);
}
