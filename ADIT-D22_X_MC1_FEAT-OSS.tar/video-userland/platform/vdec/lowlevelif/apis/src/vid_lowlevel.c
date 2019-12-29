/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * FILENAME:             $RCSfile: vid_lowlevel.c,v $
 *
 *--------------------------------------------------------------------------
 *
 * RESPONSIBLE:         Mark B. Kulaczewski
 *
 * DESCRIPTION:         videantis low level API
 *
 *
 * LAST CHECKED IN BY:  $Author: $, $Date: $
 *
 * NOTES:
 *
 * MODIFIED:
 *
 * $Log: vid_lowlevelif.c,v $
 *
 *
 * This code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this code; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *+++++++++++++++++++++++++++++ FileHeaderEnd ++++++++++++++++++++++++++++++
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

// #define VID_TIMESTAMPS 1

#define LOWLEVELIF_MAJOR_VERSION 2
#define LOWLEVELIF_MINOR_VERSION 5

#include "vid_vmp2Driverif.h"
#include "vid_overlayDriver.h"
#include "vid_debugDriver.h"

#define DEBUG_SEGMENT_SIZE 4096

#define DEBUG_SEGMENT_ADDR_ICM (1022)

#include "vid_lowlevel.h"

static struct param_t VMP2_current_param;
static char *icmdev = "/dev/vid_icm0";
static char *overlaydev = "/dev/vid_overlay0";
static char *debugdev = "/dev/vid_debug";

static int icmfd = 0;
static int overlayfd = 0;
static int debugfd = 0;
static int lowlevelif_verbose = 0;
static debug_config_t debugConfig;

VID_ADDR_PTRS vid_debugMem = { 0, 0 };
VID_ADDR_PTRS vid_icmMem = { 0, 0 };

static int lowlevelif_init_done = 0;

static struct {
  VID_IF_CONFIG ifConfig ;
  VID_BUFGROUP_PTR groupTab[MAX_BUFFERGROUPS] ;
  int indModeConfigured ;
} indModeConfig ;

/*
 * overlay memory related management data
 */
enum mbuf_status_t { BUFF_ALLOC, BUFF_FREE };

struct mbuf_t {
  void *start_addr;
  enum mbuf_status_t status;
  unsigned int bsize;
  struct mbuf_t *next;
  struct mbuf_t *prev;
};

static struct {
  unsigned int isConfigured;
  VID_ADDR_PTRS ovlptr;
  unsigned int size;
  struct mbuf_t *head;
} overlayMemConfig;

#define VID_MEM_MALLOC_ALIGN 8


#ifdef VID_TIMESTAMPS
static struct timeval start_time ;
static struct timeval end_time ;
static unsigned int wait_secs ;
static unsigned int wait_usecs ;
static struct timeval w_start ;
static struct timeval w_end ;
static unsigned int framecount ;
#endif

#define DEB_LL(x...) if (1 == lowlevelif_verbose) printf(x)


/*
 *****************************************************************************
 forwared references for helpers
 *****************************************************************************
 */
unsigned int le2be(unsigned int);
void free_ovlmem_lists(void);
int check_device_availablity(char *devicename);
/*
 ******************************************************************************
 * API functions
 ******************************************************************************
 */

/*
 * \func vid_lowlevel_init
 *
 * Initialize low level interface
 *
 * \return VMP_OK: normal end
 *         VMP_E:  error occurred
 */
int vid_lowlevel_init(void) {

	/* sanity check */
	if (1 == lowlevelif_init_done) {
	    return VMP_OK;
	}

	/* open ICM device */
	icmfd = open(icmdev, O_RDWR);
	if (icmfd == -1) {
		DEB_LL("ERROR vid_lowlevel_init: Cannot open device %s\n", icmdev);
		return (VMP_E);
	}

	/* map ICM memory space */
	vid_icmMem.virt_addr = (unsigned int *) mmap(0, ICM_BYTESIZE * ICM_NUM,	PROT_READ | PROT_WRITE, MAP_SHARED, icmfd, 0);
	DEB_LL("INFO vid_lowlevel_init - Virtual address of ICM memory: 0x%08x\n", (unsigned int)vid_icmMem.virt_addr);
	if ((int) vid_icmMem.virt_addr == -1) {
		DEB_LL("ERROR vid_lowlevel_init: failed to map icm device to memory.\n");
		vid_icmMem.virt_addr = 0 ;
		return (VMP_E);
	}

	/* open overlay device */
	if (1 == check_device_availablity(overlaydev)) {
		overlayfd = open(overlaydev, O_RDWR);
	}

	// =============================================================================
	//  check version number of vmp2Driver and overlayDriver
	// =============================================================================
	{

		struct VID_VMP2_VERSION_T vmp2Version;
		struct VID_OVERLAY_VERSION overlayVersion;

		// get the version number of the vmp2Driver
		if (ioctl(icmfd, VID_VMP2_GET_VERSION, &vmp2Version)) {
			DEB_LL("ERROR vid_lowlevel_init: Can't get  VID_VMP2_GET_VERSION: %s\n", strerror(errno));
			return (VMP_E);
		}

		// get the version number of the overlayDriver
		if (overlayfd) {
			if (ioctl(overlayfd, VID_OVERLAY_GET_VERSION, &overlayVersion)) {
				DEB_LL("ERROR vid_lowlevel_init: Can't get VID_OVERLAY_GET_VERSION: %s\n", strerror(errno));
				return (VMP_E);
			}
		}

		// compare vmp2Driver version number with lowlevelif version number.
		if ((vmp2Version.major != LOWLEVELIF_MAJOR_VERSION)	||
		    (vmp2Version.minor != LOWLEVELIF_MINOR_VERSION)) {
			DEB_LL("ERROR vid_lowlevel_init: Version number mismatch between lowlevelif library and vmp2 driver !!!\n\n");
			return (VMP_E);
		}

		if (overlayfd) {
			// compare overlayDriver version number with lowlevelif version number.
			if ((overlayVersion.major != LOWLEVELIF_MAJOR_VERSION) || (overlayVersion.minor != LOWLEVELIF_MINOR_VERSION)) {
				DEB_LL("ERROR vid_lowlevel_init: Version number mismatch between lowlevelif library and overlay driver !!!\n\n");
				return (VMP_E);
			}
		}
	}

	indModeConfig.indModeConfigured = 0;
	overlayMemConfig.isConfigured = 0;
	overlayMemConfig.head = 0;
	lowlevelif_init_done = 1;

	return (VMP_OK);
}


/**
 * \func vid_lowlevel_release
 *
 * Release low level interface
 *
 * \return VMP_OK:
 */
int vid_lowlevel_release(void) {
	int i;

	if (overlayMemConfig.ovlptr.virt_addr) {
		if (munmap((void *) overlayMemConfig.ovlptr.virt_addr, overlayMemConfig.size) != 0) {
			DEB_LL("INFO vid_lowlevel_release: Failed to unmap overlay memory\n");
		}
	}

	if (vid_debugMem.virt_addr) {
		if (munmap((void *) vid_debugMem.virt_addr, DEBUG_SEGMENT_SIZE) != 0) {
			DEB_LL("INFO vid_lowlevel_release: Failed to unmap debug memory\n");
		}
	}
	if (vid_icmMem.virt_addr) {
		if (munmap((void *) vid_icmMem.virt_addr, ICM_BYTESIZE * ICM_NUM) != 0) {
			DEB_LL("INFO vid_lowlevel_release: Failed to unmap icm memory\n");
		}
	}

	if (1 == indModeConfig.indModeConfigured) {
		for (i=0; i<indModeConfig.ifConfig.buffergroup_nb; i++) {
			VID_BUFGROUP_PTR *bgptr;
			bgptr = &indModeConfig.groupTab[i];

			if (bgptr->address.virt_addr) {
				if (munmap((void *) bgptr->address.virt_addr, DMA_BYTESIZE) != 0) {
				DEB_LL("INFO vid_lowlevel_release: Failed to unmap dma memory\n");
				}
			}
			bgptr->address.virt_addr = NULL;
		}
	}

	if (icmfd) {
		close(icmfd);
	}
	if (overlayfd) {
		close(overlayfd);
	}
	if (debugfd) {
		close(debugfd);
	}

	// clear internal data structures
	memset(&(overlayMemConfig.ovlptr), 0, sizeof(VID_ADDR_PTRS));
	memset(&vid_debugMem, 0, sizeof(vid_debugMem));
	memset(&vid_icmMem, 0, sizeof(vid_icmMem));
	memset(&debugConfig, 0, sizeof(debugConfig));
	icmfd = overlayfd = debugfd = 0;

	indModeConfig.indModeConfigured = 0;
	free_ovlmem_lists();
	overlayMemConfig.isConfigured = 0;
	lowlevelif_init_done = 0;

	return (VMP_OK);
}


/*
 * \func vid_lowlevel_version
 *
 * Provide major and minor version number of lowlevel API lib
 *
 * \return VMP_OK:
 */

int vid_lowlevel_version(unsigned int *major, unsigned int *minor)
{
	*major = LOWLEVELIF_MAJOR_VERSION;
	*minor = LOWLEVELIF_MINOR_VERSION;
	return VMP_OK;
}

/**
 * \func vid_driver_version
 *
 * Provide major and minor version number of the lowlevel vmp2 driver
 *
 * \return VMP_OK:
 */
int vid_driver_version(unsigned int *major, unsigned int *minor)
{
	unsigned int close_on_exit = 0;
	struct VID_VMP2_VERSION_T vmp2Version;

	if (0 == icmfd) {
		icmfd = open(icmdev, O_RDWR);
		if (icmfd == -1) {
			DEB_LL("ERROR vid_driver_version: Cannot open device %s\n", icmdev);
			return (VMP_E);
		}
		close_on_exit = 1;
	}

	/* get the version number of the vmp2Driver */
	if (ioctl(icmfd, VID_VMP2_GET_VERSION, &vmp2Version)) {
		DEB_LL("ERROR vid_driver_version: Can't get  VID_VMP2_GET_VERSION: %s\n", strerror(errno));
		if (1 == close_on_exit) {
			close(icmfd);
		}
		return (VMP_E);
	}
		
	*major = vmp2Version.major;
	*minor = vmp2Version.minor;

	if (1 == close_on_exit) {
		close(icmfd);
	}
	return VMP_OK;
}

/**
 * \func vid_icm_alloc
 *
 * Return for the SW codec library the virtual intercore address memory SW codec
 * library calling function
 *
 * \param addr_ptr:	structure where the physical and virtual address of the icm
 *                  will be stored
 *        core:		not evaluated for ADIT/TRITON
 *
 * \return VMP_OK: normal end
 *         VMP_E:  error occurred, pointer invalid
 */
int vid_icm_alloc(VID_ADDR_PTRS * addr_ptr, unsigned int core) {

	int rc = VMP_OK ;

	DEB_LL("INFO vid_icm_alloc: num %d \n", core);
	if (0 != vid_icmMem.virt_addr) {
		addr_ptr->virt_addr = vid_icmMem.virt_addr + (ICM_BYTESIZE / 4) * core;
	}
	else {
		addr_ptr->virt_addr = 0 ;
		rc = VMP_E ;
	}

	return (rc);
}


/**
 * \func vid_mem_alloc
 *
 * Return for the SW codec library the virtual overlay address memory SW codec
 * library calling function.
 *
 * \param addr_ptr:	structure where the physical and virtual address of the icm
 *                  will be stored
 *        size:		not needed for ADIT project
 *        core:		not needed for ADIT project
 *
 * \return VMP_OK: normal end
 *         VMP_E: error occurred
 *
 */
int vid_mem_alloc(VID_ADDR_PTRS * addr_ptr, unsigned int size, unsigned int core) {

	int rc = VMP_OK;
	struct overlay_config_t overlayConfig;

	/* assignments to eliminate compiler warnings due to unused parameters */
	size = size;
        core = core;

	// get physical address and size
	//
	if (overlayfd) {
		if (ioctl(overlayfd, VID_OVERLAY_GET, &overlayConfig)) {
			DEB_LL("ERROR vid_mem_alloc: Can't get VID_OVERLAY_GET: %s\n", strerror(errno));
			return VMP_E;
		}

		overlayMemConfig.size = overlayConfig.size;
		overlayMemConfig.ovlptr.phys_addr = (volatile unsigned int *) overlayConfig.phys_addr;

		DEB_LL("INFO vid_mem_alloc: physical address of overlay memory: 0x%08x\n", overlayConfig.phys_addr);
		DEB_LL("INFO vid_mem_alloc: size of overlay memory            : 0x%08x\n", overlayConfig.size);

		// map OVERLAY memory space
		// use if-clause to make sure that mmap() is called only once per session
		if (overlayMemConfig.ovlptr.virt_addr == 0) {
			overlayMemConfig.ovlptr.virt_addr = (unsigned int *) mmap(0, overlayConfig.size,	PROT_READ | PROT_WRITE, MAP_SHARED, overlayfd, 0);
			if ((int) overlayMemConfig.ovlptr.virt_addr == -1) {
				DEB_LL("ERROR vid_mem_alloc: failed to map overlay device to memory.\n");
				return VMP_E;
			}
		}

		DEB_LL("INFO vid_mem_alloc: virtual address of overlay memory: 0x%08x\n", (unsigned int)overlayMemConfig.ovlptr.virt_addr);

		// add offsets to base addresses. use typecasts to circumvent pointer arithmetics..
		addr_ptr->virt_addr = overlayMemConfig.ovlptr.virt_addr;
		addr_ptr->phys_addr = overlayMemConfig.ovlptr.phys_addr;
	}
	else {
		rc = VMP_E ;
	}

	return rc;
}


/**
 * \func vid_boot_vmp
 *
 * called by videantis SW library to boot VMP2 program code
 *
 * \param vmp2_bin_addr:	address of mp binary
 *        core:		 		v-MP2-Core number
 *
 * \return VMP_OK: normal end
 *         VMP_E:  error occurred
 */
int vid_boot_vmp(unsigned char * vmp2_bin_addr, unsigned vmp2_bin_size,	unsigned core) {
	int i;
	int rc = VMP_OK;

	DEB_LL("INFO vid_boot_vmp : load binary for MP#%d at 0x%x size 0x%x\n", core, (unsigned int)vmp2_bin_addr, vmp2_bin_size);

 	VMP2_current_param.cmd = VID_VMP2_SET_PARAM_BOOTVMP ;
	VMP2_current_param.vmp2_bin_addr = (unsigned) vmp2_bin_addr;
	VMP2_current_param.vmp2_bin_size = vmp2_bin_size;
	VMP2_current_param.core = core;

	// include debug and features addresses for ioctl()
	// default settings: All zero

	VMP2_current_param.dbg_addr = 0;
	VMP2_current_param.feat_addr = 0;
	VMP2_current_param.res0_addr = 0;
	VMP2_current_param.res1_addr = 0;

	// overwrite defaults if configured appropiately

	VMP2_current_param.dbg_addr = (unsigned) vid_debugMem.phys_addr;

	// adjust debug channel address depending on value of core

	if (1 == core) {
		VMP2_current_param.dbg_addr += 0x10;
	}

	VMP2_current_param.dbg_addr = le2be(VMP2_current_param.dbg_addr);

	DEB_LL("INFO vid_boot_vmp : VMP2_current_param.core %d\n", VMP2_current_param.core);
	DEB_LL("INFO vid_boot_vmp : VMP2_current_param.dbg_addr %x (%x) \n", VMP2_current_param.dbg_addr, le2be(VMP2_current_param.dbg_addr));
	DEB_LL("INFO vid_boot_vmp : VMP2_current_param.feat_addr %x (%x) \n", VMP2_current_param.feat_addr, le2be(VMP2_current_param.feat_addr));

	if (ioctl(icmfd, VID_VMP2_SET_PARAM, &VMP2_current_param)) {
		rc = VMP_E ;
		DEB_LL("ERROR vid_vmp_boot: Can't get  VID_VMP2_SET_PARAM: %s\n", strerror(errno));
	}

	if (lowlevelif_verbose) {
		char *ptr = (char *) vmp2_bin_addr;
		printf("vid_boot_vmp: dump some bytes of beginning of mp2 boot code\n");
		for (i = 0; i < 20; i++) {
			printf("%02x ", ptr[i]);
		}
		printf("\n");
	}

	return (rc);
}


/**
 * \func vid_stop_vmp
 *
 * stop vmp core (enable vmp reset)
 *
 * \param core:	 	v-MP2-Core number  (Not used in ADIT project)
 *
 * \return VMP_OK: normal end
 *         VMP_E:  error occurred
 */
int vid_stop_vmp(unsigned int core) {

	int rc = VMP_OK;

	DEB_LL("INFO vid_stop_vmp : stopping MP#%d.\n", core);

	if (ioctl(icmfd, VID_VMP2_STOP_VMP, &core)) {
		DEB_LL("ERROR vid_stop_vmp: Can't execute  VID_VMP2_STOP_VMP: %s\n", strerror(errno));
		rc = VMP_E ;
	}
#ifdef VID_TIMESTAMPS
	{
	  unsigned int delta_s ;
	  int delta_us ;
	  double rt ;
	  double wt ;
	  double fps ;

	  gettimeofday(&end_time, 0) ;
	  delta_s = end_time.tv_sec - start_time.tv_sec ;
	  delta_us = end_time.tv_usec - start_time.tv_usec ;
	  if (delta_us < 0) {
		  delta_us = - delta_us ;
		  delta_s-- ;
	  }
	  printf("Runtime = %d.%d\n",delta_s, delta_us) ;
	  printf("Framecount = %d\n",framecount) ;
	  rt = (double)delta_s + (double)delta_us/1000000 ;
	  fps = framecount / rt ;
	  printf("fps = %5.2f\n",fps) ;
	  printf("Informative -- wait time = %d.%d\n",wait_secs, wait_usecs) ;
  	  wt = (double)wait_secs + (double)wait_usecs/1000000 ;
	  printf("Informative -- load @ 25fps = %5.2f\%\n",(rt-wt)/rt*(25.0/fps)*100) ;
	}
#endif
	return rc;
}


/**
 * \func vid_avail_vmp
 *
 * called by videantis SW library to check if a given core is available
 *
 * \param core:	 	v-MP2-Core number  (Not used in ADIT project)
 *
 * \return  : 1 if "core" is available,
 *            0 if not,
 *            VMP_E on error
 *
 */
int vid_avail_vmp(unsigned int core) {

	int _core ;
	int rc = VMP_OK ;

	DEB_LL("INFO vid_avail_vmp : checking MP#%d.\n", core);

	_core = (unsigned int) core;
	if (ioctl(icmfd, VID_VMP2_IS_CORE_FREE, &_core)) {
		DEB_LL("ERROR vid_avail_vmp: Can't execute  VID_AVAIL_VMP: %s\n", strerror(errno));
		rc = VMP_E;
	} else {
		if (1 != _core)
			rc = 0;
		else
			rc = 1;
	}

	return (rc);
}


/**
 * \func vid_get_buffers
 *
 *  request group of buffers
 *
 * \param memoryid:           intercore memory to use (not used in ADIT project)
 *        grouphandle:        pointer to struct containing returned handle
 *        blockingflag:       flag indicating blocking operation or non-blocking operation
 *
 * \return VMP_OK: normal return, grouphandle is valid
 *         VMP_E:  error occurred, grouphandle is invalid
 *         VMP_WOULD_BLOCK: non-blocking operation requested, but function call would block.
 *                          grouphandle is invalid.
 */
int vid_get_buffers(int memoryid, VID_BUFGROUP_PTR * grouphandle, int blockingflag) {
  struct ioctl_buf_t params ;
  int rc ;

  DEB_LL("INFO vid_get_buffers: Called\n");

  rc = 0 ;

  if (1 == blockingflag)
	params.mode = VID_VMP2_BLOCKING ;
  else
	params.mode = VID_VMP2_NONBLOCKING ;
  params.numdpram = memoryid ;

  if ( NULL != grouphandle ) {
#ifdef VID_TIMESTAMPS
    gettimeofday(&w_start, 0) ;
#endif
    if (ioctl(icmfd, VID_VMP2_GET_BUFFER, &params)) {
      DEB_LL("ERROR vid_get_buffers: Can't set  VID_VMP2_GET_BUFFER: %s\n", strerror(errno));
      rc = VMP_E ;
    }
    else {
      rc = params.rc ;
      if ( 0 == rc ) {
    	  *grouphandle = indModeConfig.groupTab[params.buffergroupidx] ;
      }
      else {
    	  DEB_LL("ERROR vid_get_buffers: Failed\n");
    	  rc = VMP_E ;
      }
    }
#ifdef VID_TIMESTAMPS
    {
      unsigned int delta_s ;
      int delta_us ;

      gettimeofday(&w_end, 0) ;
      delta_s = w_end.tv_sec - w_start.tv_sec ;
      delta_us = w_end.tv_usec - w_start.tv_usec ;
      if (delta_us < 0) {
	  delta_us = - delta_us ;
	  delta_s-- ;
      }
      wait_secs += delta_s ;
      wait_usecs += delta_us ;
      if (wait_usecs >= 1000000) {
	wait_secs++ ;
	wait_usecs -= 1000000 ;
      }
      framecount++ ;
    }
#endif
  }
  else {
    rc = VMP_E ;
  }

  return rc ;
}


/**
 * \func vid_send_buffers
 *
 *  hand over group of buffers to DMA layer
 *
 * \param grouphandle:        pointer to struct containing valid group handle
 *                            or NULL pointer
 * \return VMP_OK: normal return
 *         VMP_E:  error occurred
  */
int vid_send_buffers(VID_BUFGROUP_PTR *grouphandle) {

  struct ioctl_buf_t params ;
  int rc ;

  rc = VMP_OK ;

  DEB_LL("INFO vid_send_buffers : Called\n") ;

  if (NULL != grouphandle) {
	params.buffergroupidx = grouphandle->groupid ;
	params.numdpram = indModeConfig.ifConfig.memoryid ;
	params.flushcmd = 0 ;
	if (ioctl(icmfd, VID_VMP2_SEND_BUFFER, &params)) {
		DEB_LL("ERROR vid_send_buffers: VID_VMP2_SEND_BUFFER ioctl failed.\n") ;
	}
	rc = params.rc ;
  }
  else {
	params.numdpram = indModeConfig.ifConfig.memoryid ;
	params.flushcmd = 1 ;
	if (ioctl(icmfd, VID_VMP2_SEND_BUFFER, &params)) {
		DEB_LL("ERROR vid_send_buffers: VID_VMP2_SEND_BUFFER flush ioctl failed.\n") ;
	}
	rc = params.rc ;
  }

  DEB_LL("INFO vid_send_buffers: rc = %d\n",rc) ;
  return rc ;
}


/*
 * \func vid_get_if_config
 *
 *  read lowlevel indirect mode configuration
 *
 * \param config:  configuration structure returned
 *
 * \return VMP_OK: normal result
 *         VMP_E:  error occurred
 *                 memoryid != 0 or buffer_size <= 0
 */
int vid_get_if_config(VID_IF_CONFIG *config) {

	/* assignments to eliminate compiler warnings */
	config = config;

	DEB_LL("INFO vid_get_if_config: Called\n");

	return (VMP_OK);
}


/**
 * \func vid_set_if_config
 *
 *  set lowlevel indirect mode configuration
 *
 * \param config:  configuration structure containing requested settings
 *
 * \return VMP_OK: normal end
 *         VMP_E:  error occurred
 */
int vid_set_if_config(VID_IF_CONFIG *config) {

	int i ;

	DEB_LL("INFO vid_set_if_config: Called\n");

	VMP2_current_param.cmd = VID_VMP2_SET_PARAM_IFCONFIG ;
	VMP2_current_param.memoryid = config->memoryid ;
	VMP2_current_param.buffer_size = config->buffer_size ;
	VMP2_current_param.buffergroup_nb = config->buffergroup_nb ;
	VMP2_current_param.buffergroup_size = config->buffergroup_size ;
	VMP2_current_param.zerobuffer = config->zerobuffer ;
	VMP2_current_param.zerostart = config->zerostart ;
	VMP2_current_param.zerolength = config->zerolength ;
	VMP2_current_param.icmslots_nb = config->icmslots_nb ;
	VMP2_current_param.icmslots_offset = config->icmslots_offset ;
	if (ioctl(icmfd, VID_VMP2_SET_PARAM, &VMP2_current_param)) {
		DEB_LL("ERROR vid_send_buffers: VID_VMP2_SET_PARAM ioctl failed.\n");
		return (VMP_E) ;
	}

	/* save configuration */
	indModeConfig.ifConfig = *config ;

	/* map indirect mode buffers */
	for (i=0; i<indModeConfig.ifConfig.buffergroup_nb; i++) {
		VID_BUFGROUP_PTR *bgptr ;

		bgptr = &indModeConfig.groupTab[i] ;

		bgptr->groupid = i ;
		bgptr->buffer_size = indModeConfig.ifConfig.buffer_size ;
		bgptr->buffergroup_size = indModeConfig.ifConfig.buffergroup_size ;
		bgptr->address.virt_addr = (unsigned int *) mmap( 0,
						DMA_BYTESIZE,
						PROT_READ | PROT_WRITE,
						MAP_SHARED,
						icmfd,
					       (i+1)*sysconf(_SC_PAGE_SIZE));
		/* the physical address has no meaning here */
		bgptr->address.phys_addr = 0 ;
	}

	/* tag indirect mode configuration as valid */
	indModeConfig.indModeConfigured = 1;

#ifdef VID_TIMESTAMPS
	gettimeofday(&start_time,0) ;
	wait_secs = 0 ;
	wait_usecs = 0 ;
	framecount = 0 ;
#endif

	return (VMP_OK);
}


/**
 * \func vid_reset_ring_buffer_offset
 *
 * \note     : Not supported on ADIT platform
 *
 * \return VMP_E:  error, since not supported
 *
 */
int vid_set_ring_buffer_offset(int memory) {

	/* assignment to eliminate compiler warnings */
	memory = memory;

	DEB_LL("WARNING vid_reset_ring_buffer_offset: Called but not supported\n");

	return (VMP_E);
}


/**
 * \func vid_test_vmp
 *
 * \note     : called by videantis SW library to test VMP2 integration
 *
 */
int vid_test_vmp(unsigned char * vmp2_bin_addr, unsigned vmp2_bin_size, unsigned core) {
	int i;

	DEB_LL("DEBUG: VMP2 driver Load binary for MP#%d at 0x%x size 0x%x\n", core, (unsigned int)vmp2_bin_addr, vmp2_bin_size);

	VMP2_current_param.cmd = VID_VMP2_SET_PARAM_BOOTVMP ;
	VMP2_current_param.vmp2_bin_addr = (unsigned) vmp2_bin_addr;
	VMP2_current_param.vmp2_bin_size = vmp2_bin_size;
	VMP2_current_param.core = core;

	DEB_LL("DEBUG: VMP2_current_param.core %d\n", VMP2_current_param.core);

	if (ioctl(icmfd, VID_VMP2_TEST_CORES, &VMP2_current_param)) {
		DEB_LL("ERROR vid_test_vmp: ioctl VID_VMP2_TEST_CORES failed: %s\n", strerror(errno));
		return -1;
	}

	if (lowlevelif_verbose) {
		char *ptr = (char *) vmp2_bin_addr;
		printf("dump some bytes of beginning of mp2 boot code\n");
		for (i = 0; i < 20; i++) {
			printf("%02x ", ptr[i]);
		}
		printf("\n");
	}
	return 0;
}


/*
 * ---------------------------------------------------------------------------
 * Verbosity
 * ---------------------------------------------------------------------------
 */
void vid_set_verbosity(int level) {
	if ((level < 0) || (level > 1)) {
		printf("vid_set_verbosity: Illegal verbosity level.\n");
	} else {
		lowlevelif_verbose = level;
	}
}

/*
 * ---------------------------------------------------------------------------
 * Debug IF
 * ---------------------------------------------------------------------------
 */

/*
 * \func vid_debug_init
 *
 */
int vid_debug_init(void) {
	// open Debug device
	//
	debugfd = open(debugdev, O_RDWR);
	if (debugfd == -1) {
		DEB_LL("ERROR vid_debug_init: Cannot open device %s\n", debugdev);
		return (VMP_E);
	}

	// get physical address and size
	//
	if (ioctl(debugfd, VID_DEBUG_GET, &debugConfig)) {
		DEB_LL("ERROR vid_debug_init: Can't get VID_DEBUG_GET: %s\n", strerror(errno));
		return (VMP_E);
	}

	DEB_LL("INFO vid_debug_init : physical address of debug memory  : 0x%08x\n", (unsigned int)debugConfig.phys_addr);
	DEB_LL("INFO vid_debug_init : size of debug memory              : 0x%08x\n", (unsigned int)debugConfig.size);

	// map debug memory space
	vid_debugMem.virt_addr = (unsigned int *) mmap(0, DEBUG_SEGMENT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, debugfd, 0);
	if ((int) vid_debugMem.virt_addr == -1) {
		DEB_LL("ERROR vid_debug_init: failed to map debug device to memory.\n");
		return (VMP_E);
	}

	vid_debugMem.phys_addr = (volatile unsigned int *) debugConfig.phys_addr;
	return (VMP_OK);
}


/*
 * \func vid_debug_alloc
 *
 */
int vid_debug_alloc(VID_ADDR_PTRS * addr_ptr) {

	addr_ptr->virt_addr = vid_debugMem.virt_addr;
	addr_ptr->phys_addr = vid_debugMem.phys_addr;

	return (VMP_OK);

}

/*
 * ---------------------------------------------------------------------------
 * malloc/free system for overlay memory
 * ---------------------------------------------------------------------------
 */

/*
 * \func vid_mem_malloc
 *
 * \note     : Allocate memory segment from overlay memory pool
 *
 * \return
 *
 */
void *vid_mem_malloc(unsigned int size)
{
	int rc;
	struct mbuf_t *mbp;
	struct mbuf_t *mbp2;
	void *returnptr ;
	unsigned int allocsize;


	/* check if we have configured overlay memory before */
	if (0 == overlayMemConfig.isConfigured) {
		/* no, so configure it */
		rc = vid_mem_alloc(&(overlayMemConfig.ovlptr),0,0) ;
		if (VMP_E == rc) {
			DEB_LL("ERROR vid_mem_malloc: vid_mem_alloc failed.\n");
			return 0;
		}
		mbp = malloc(sizeof(struct mbuf_t)) ;
		if (NULL == mbp) {
			DEB_LL("ERROR vid_mem_malloc: failed to malloc control structures.\n");
			return 0;
		}
		overlayMemConfig.head = mbp;
		/* initialize header, which is an empty block */
		mbp->prev = NULL;
		mbp->next = NULL;
		mbp->status = BUFF_FREE;
		mbp->bsize = overlayMemConfig.size;
		mbp->start_addr = (void *)overlayMemConfig.ovlptr.virt_addr ;
		overlayMemConfig.isConfigured = 1;
	}
	/* handle case with size 0 */
	if (0 == size) {
		return 0;
	}
	/*
	 * system is now definitely configured
	 * find free block with sufficient size
	 */
	allocsize = (size + VID_MEM_MALLOC_ALIGN - 1) & ~(VID_MEM_MALLOC_ALIGN - 1);
	mbp = overlayMemConfig.head;
	returnptr = NULL;
	while(NULL != mbp) {
		if (BUFF_ALLOC == mbp->status)
			mbp = mbp->next ;
		else if (BUFF_FREE == mbp->status) {
			if (mbp->bsize > allocsize) {
				/* found larger one, so split buffer */
				mbp2 = malloc(sizeof(struct mbuf_t));
				if (NULL == mbp2) {
					DEB_LL("ERROR vid_mem_malloc: failed to malloc control structures.\n");
					return 0;
				}
				mbp2->next = mbp->next;
				if (mbp2->next != NULL)
					mbp2->next->prev = mbp2;
				mbp2->prev = mbp;
				mbp->next = mbp2;

				mbp2->bsize = mbp->bsize - allocsize;
				mbp2->start_addr = (void *)((unsigned int)(mbp->start_addr) + allocsize);
				mbp2->status = BUFF_FREE;
				mbp->bsize = allocsize;
				mbp->status = BUFF_ALLOC;
				returnptr = mbp->start_addr;
				/* force while exit */
				mbp = NULL;
			}
			else if (mbp->bsize == allocsize) {
				/* perfect match */
				mbp->status = BUFF_ALLOC;
				returnptr = mbp->start_addr;
				/* force while exit */
				mbp = NULL;
			}
			else {
				/* try next buffer */
				mbp = mbp->next ;
			}
		}
		else {
			/* illegal status */
			DEB_LL("ERROR vid_mem_malloc: Illegal management structure status found.\n");
			return NULL ;
		}
	}
	return returnptr;
}


/*
 * \func vid_mem_free
 *
 * \note     : Return memory segement to overlay memory pool
 *
 * \return None
 *
 */

void vid_mem_free(void *ptr)
{
	struct mbuf_t *mbp ;
	struct mbuf_t *mbp2 ;

	/* sanity checks */
	if ((NULL == ptr)||(overlayMemConfig.isConfigured != 1)) {
		return ;
	}

	mbp = overlayMemConfig.head;
	mbp2 = NULL;
	while (mbp != NULL) {
		if (mbp->start_addr == ptr) {
			mbp2 = mbp;
			mbp = NULL;
		}
		else {
			mbp = mbp->next;
		}
	}

	/* mbp2 now contains entry found or, in case of error, NULL */
	if (NULL == mbp2) {
		DEB_LL("ERROR vid_mem_free: free with illegal pointer.\n");
		return;
	}

	/* entry found. mark buffer as free */
	mbp2->status = BUFF_FREE;
	/* perform greedy merge towards head of list */
	mbp = mbp2->prev ;
	while ((mbp != NULL) && (BUFF_FREE == mbp->status)) {
		/*
		 * loop invariant: mbp2 points to current free block
		 *                 mbp to adjacent free block towards head of list
		 */
		mbp->bsize += mbp2->bsize;
		mbp->next = mbp2->next;
		if (mbp->next != NULL) {
			mbp->next->prev = mbp;
		}
		free(mbp2);
		mbp2 = mbp;
		mbp = mbp->prev;
	}
	/* perform greedy merge towards tail of list */
	mbp = mbp2->next;
	while ((mbp != NULL) && (BUFF_FREE == mbp->status)) {
		/*
		 * loop invariant: mbp2 points to current free block
		 *                 mbp to adjacent free block towards tail of list
		 */
		mbp2->bsize += mbp->bsize;
		mbp2->next = mbp->next;
		if (mbp2->next != NULL) {
			mbp2->next->prev = mbp2;
		}
		free(mbp);
		mbp = mbp2->next;
	}
}

/*
 * \func vid_mem_to_phys
 *
 * \note     : Convert virtual overlay memory address to physical
 *
 * \return physical address
 *
 */

void *vid_mem_to_phys(void *ptr)
{
  unsigned int offset;

  if (1 == overlayMemConfig.isConfigured) {
    offset = (unsigned int)ptr - (unsigned int)overlayMemConfig.ovlptr.virt_addr;
    return (void *)((unsigned int)overlayMemConfig.ovlptr.phys_addr + offset);
  }
  else {
    return 0;
  }
}

/*
 * \func vid_mem_is_ovl
 *
 * \note     : Check if address is part of the overlay segment
 *
 * \return 1 if ptr points to overlay segment
 *         0 if not
 */

int vid_mem_is_ovl(void *ptr)
{
  unsigned int lowbound;
  unsigned int highbound;


  if (1 == overlayMemConfig.isConfigured) {
    lowbound = (unsigned int)overlayMemConfig.ovlptr.virt_addr;
    highbound = lowbound + overlayMemConfig.size;
    if (((unsigned int)ptr >= lowbound) && ((unsigned int)ptr < highbound))
      return 1;
    else
      return 0;
  }
  else {
    return 0;
  }
}

/*
 * \func vid_mem_flush
 *
 * \note     : Initiates D-Cache Flush plus invalidate
 *             by calling ioctl in vid_overlayDriver 
 *			  
 *
 * \return VMP_E if ioctl failed
 *         VMP_OK is everything ok
 */

int vid_mem_flush(void)
{
	struct overlay_sync_param_t sync_params;

	if (1 == overlayMemConfig.isConfigured) {
		sync_params.addr = 0;
		sync_params.len = 0;
		sync_params.dir = 3;

		if (ioctl(overlayfd, VID_OVERLAY_SYNC, &sync_params)) {
			DEB_LL("ERROR vid_mem_flush: Can't execute ioctl VID_OVERLAY_FLUSH: %s\n", strerror(errno));
			return (VMP_E);
		}
		return (VMP_OK);
	}
	else {
		return (VMP_E);
	}
}

/*
 *******************************************************************************
 *
 * Internal helper functions
 *
 ******************************************************************************
 */
int check_device_availablity(char *devicename)
{

	int devfd;

	devfd = open(devicename, O_RDWR);
	if (devfd == -1) {
		return (0);
	} else {
		close(devfd);
		return (1);
	}
}

/* switch endianess of unsigned int value so that v-MP reads value with correct endianess */

unsigned int le2be(unsigned int val)
{
	return (((val & 0xff) << 24) | ((val & 0xff00) << 8) | ((val & 0xff0000)
			>> 8) | ((val & 0xff000000) >> 24));

}


/* deallocate all dynamic structure related to ovl memory management */
void free_ovlmem_lists(void)
{

	struct mbuf_t *mbp;
	struct mbuf_t *mbp_n;


	if (1 == overlayMemConfig.isConfigured) {
		mbp = overlayMemConfig.head;
		while (mbp != NULL) {
		      mbp_n = mbp->next;
		      free(mbp);
		      mbp = mbp_n;
		}
		overlayMemConfig.head = 0;
	}
}
