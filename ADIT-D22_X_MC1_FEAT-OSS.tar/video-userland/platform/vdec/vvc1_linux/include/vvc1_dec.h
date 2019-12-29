/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *                                                                          
 * DESCRIPTION:         decoder definitions
 *                                                                          
 *+++++++++++++++++++++++++++++ FileHeaderEnd ++++++++++++++++++++++++++++++*/
/*
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
 */

#ifndef _VVC1_DEC_H_
#define _VVC1_DEC_H_


#define VVC1_VID_STATUS_ERROR_INIT         -6
#define VVC1_VID_STATUS_ERROR_MEMALLOC     -5
#define VVC1_VID_STATUS_ERROR_UNSUPPORTED  -4
#define VVC1_VID_STATUS_ERROR_MP_TIMEOUT   -3
#define VVC1_VID_STATUS_ERROR_NOBUFFER	   -2
#define VVC1_VID_STATUS_ERROR		   -1
#define VVC1_VID_STATUS_OK		    1
#define VVC1_VID_STATUS_FRAME_END           2
#define VVC1_VID_STATUS_FRAME_NOT_FINISHED  3

#define VVC1_VID_IMAGE_FORMAT_YUV420        1
#define VVC1_VID_IMAGE_FORMAT_YUV422        5

#ifndef LINUX
#include "vmp_interface.h"
#else
#define IMPORT extern
#define LOCAL  static

#include "adit_typedef.h"
#include "vmp_fbmem_allocator.h"
#include "vid_lowlevelif.h"
#endif

typedef S64 VVC1_VID_timestamp ; 

typedef struct {
  unsigned x_dim_0;
  unsigned y_dim_0;
  VMP_FrameBuffer *image_0;
  unsigned format_0;
  unsigned x_dim_1;
  unsigned y_dim_1;
  VMP_FrameBuffer *image_1;
  unsigned format_1;
  int      status;
  int      new_buffer_request;
  int      error_resilience_status;
  VVC1_VID_timestamp pts;
  VVC1_VID_timestamp dts;
} VVC1_VID_DEC_INFO;




//
// Parameters of memory allocator function
//    buffer_number      : number of buffers to be allocated
//    buffersize         : size of buffers to be allocated in byte
//    buffer_ptr_array   : array to hold pointers to buffers (return values)
// Return value
//    number of buffers allocated, -1 if failed 

typedef S32 (* VVC1_fbmem_allocator_fnptr) (U32 buffer_number, SVGUint16 width, SVGUint16 height, SVGColorMode color, VMP_FrameBuffer * buffer_ptr_array) ;

typedef void (* VVC1_fbmem_deallocator_fnptr) (U32 buffer_number, VMP_FrameBuffer * buffer_ptr_array) ;

/*** DEFINE_IFLIB

[LIBRARY HEADER FILE]
ifvvc1_dec.h

[FNUMBER HEADER FILE]
fnvvc1_dec.h

[INCLUDE FILE]
"VVC1_dec.h"

[PREFIX]
VVC1
***/

/* [BEGIN SYSCALLS] */
// =============================================================================
// function prototypes 
// =============================================================================
//
// Parameters of memory allocator function
//    buffer_number      : number of buffers to be allocated
//    buffersize         : size of buffers to be allocated in byte
//    buffer_ptr_array   : array to hold pointers to buffers (return values)
// Return value
//    number of buffers allocated, -1 if failed 

//
// Parameters of dec_init function
//    streambuf    : pointer to buffer containing encoded video stream
//    buffertype   : 0 (buffer contains sequence info) or 1 (separate function used for sequence info) 
//    fnptr        : pointer to memory allocator function
//    image_format : format descriptor for output color format
// Return value
//    Error code

IMPORT S32 VVC1_dec_init(U8 *streambuf, U32 buffertype, VVC1_fbmem_allocator_fnptr fnptr, U32 image_format);

//
// Parameters of dec_frame function
//    streambuf         : pointer to buffer containing encoded video stream
//    bufsize           : number of bytes used in buffer
//    pts               : image presentation time stamp  
//    dts               : image decoding time stamp
//    freebuffers_ptr   : pointer to array of buffer pointers that can be re-used
//    freebuffers_count : number of pointers valid in buffer pointer array
//    VVC1_dec_frame  : Structure containing frame pointers and status information
// Return value
//    Error code

IMPORT S32 VVC1_dec_frame(U8 *streambuf, U32 bufsize, VVC1_VID_timestamp pts, VVC1_VID_timestamp dts, VMP_FrameBuffer **freebuffers_ptr, U32 freebuffers_count, VVC1_VID_DEC_INFO *VVC1_dec_info);

//
// Parameters of dec_finish function
//    none
// Return value
//    none

IMPORT void VVC1_dec_finish(VVC1_fbmem_deallocator_fnptr fnptr);



/* [END SYSCALLS] */


#endif // defined(_VVC1_DEC_H_)
