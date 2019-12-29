/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * FILENAME:             $RCSfile: vid_lowlevelif.h,v $
 *
 *--------------------------------------------------------------------------
 *
 * RESPONSIBLE:         Mark B. Kulaczewski
 *
 * DESCRIPTION:         videantis low level if API include file
 *
 *
 * LAST CHECKED IN BY:  $Author: $, $Date: $
 *
 * NOTES:
 *
 * MODIFIED:
 *
 * $Log: vid_lowlevelif.h,v $
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

#ifndef VID_LOWLEVELIF_H
#define VID_LOWLEVELIF_H

#define VMP_OK  0
#define VMP_E  (-1)


typedef struct {
  volatile unsigned int * virt_addr;
  volatile unsigned int * phys_addr;
} VID_ADDR_PTRS;


typedef struct {
    int memoryid;
    int buffer_max_nb;
    int buffer_size;
    int buffergroup_size;
    int buffergroup_nb;
    int zerobuffer;
    int zerostart;
    int zerolength;
    int icmslots_nb;
    int icmslots_offset;
} VID_IF_CONFIG;

typedef struct {
   int groupid;
   int buffer_size;
   int buffergroup_size;
   VID_ADDR_PTRS address;
} VID_BUFGROUP_PTR;


//
// prototypes
//
//
// lowlevel if basic functions
//
extern int vid_lowlevel_init(void);
extern int vid_lowlevel_release(void);
extern int vid_icm_alloc(VID_ADDR_PTRS * addr_ptr, unsigned int core);
extern int vid_mem_alloc(VID_ADDR_PTRS * addr_ptr, unsigned int size, unsigned int core);
extern void *vid_mem_malloc(unsigned int size);
extern void vid_mem_free(void *ptr);
extern void *vid_mem_to_phys(void *ptr);
extern int vid_mem_is_ovl(void *ptr);
extern int vid_mem_flush(void);
extern int vid_dtcm_init(void);
extern int vid_dtcm_alloc(VID_ADDR_PTRS * addr_ptr);
extern int vid_boot_vmp(unsigned char * vmp2_bin_addr, unsigned vmp2_bin_size, unsigned core);
extern int vid_stop_vmp(unsigned core);
extern int vid_avail_vmp(unsigned core);
extern int vid_send_buffers(VID_BUFGROUP_PTR * grouphandle);
extern int vid_get_buffers(int memoryid, VID_BUFGROUP_PTR * grouphandle, int blockingflag);
extern int vid_get_if_config(VID_IF_CONFIG * config);
extern int vid_set_if_config(VID_IF_CONFIG * config);
extern int vid_reset_ring_buffer_offset(int memory );
extern int vid_test_vmp(unsigned char * vmp2_bin_addr, unsigned vmp2_bin_size, unsigned core);

extern int vid_lowlevel_version(unsigned int *major, unsigned int *minor);
extern int vid_driver_version(unsigned int *major, unsigned int *minor);

//
// verbosity level
//
extern void vid_set_verbosity(int level) ;
//
// additional debug functions
//
extern int vid_debug_init(void) ;
extern int vid_debug_release(void) ;
extern int vid_debug_alloc(VID_ADDR_PTRS * addr_ptr) ;

#endif
