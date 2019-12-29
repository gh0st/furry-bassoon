/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *                                                                          
 * DESCRIPTION:         main decoder routines (embedded)
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

#include <string.h>

#include "adit_typedef.h"
#ifndef LINUX
#include "tk/tkernel.h"
#include "vmp_lowlevelif.h"
#else
#include "vid_lowlevelif.h"
#endif


#include "vvc1_dec.h"
#include "vvc1_decoder.h"
#include "vvc1_buffer.h"
#include "vvc1_bitstream.h"
#include "vvc1_decode_frame.h"
#include "vvc1_ctrl_struct.h"
#include "vvc1_tables.h"
#ifdef _INDIRECT
#include "vvc1_vmp2_bin.h.indirect"
#else
#include "vvc1_vmp2_bin.h.direct"
#endif

/* PRQA: QAC Message 3347, 3760, 3771: deactivation because of pre-existing IP*/
/* PRQA S 3347, 3760, 3771 L1*/
/* PRQA: Lint Message 572, 644, 715, 826:deactivation because inline asm not recognized / pre-existing IP */
/*lint -save -e572 -e644 -e715 -e826*/


/* private prototypes */
static int vid_vc1dec_init(unsigned char *, unsigned);
static VVC1_VID_DEC_INFO vid_vc1dec_decode_frame(unsigned char *, unsigned, VVC1_VID_timestamp, VVC1_VID_timestamp, 
						 VMP_FrameBuffer **freebuffers_ptr, U32 freebuffers_count);
static int vid_vc1dec_uninit(void);


DECODER VVC1_decoder;
unsigned int VVC1_buf_num;
unsigned int VVC1_icm_num;

VMP_FrameBuffer VVC1_fbmem_frame_buf[NR_OF_VID_BUFFERS];
VMP_FrameBuffer VVC1_fbmem_disp_buf[NR_OF_VID_BUFFERS];
int VVC1_fbmem_buf_used[NR_OF_VID_BUFFERS];
VVC1_VID_timestamp VVC1_pts_buf[NR_OF_VID_BUFFERS];
VVC1_VID_timestamp VVC1_dts_buf[NR_OF_VID_BUFFERS];

MB_CONTEXT VVC1_colocated_mb[MAX_MBS];
PRED_DC_ENTRY VVC1_predict_mem[MAX_PRED_DC_MBS], VVC1_predict_left, VVC1_predict_current;
unsigned char VVC1_motionvectortype[MAX_MBS];
unsigned char VVC1_bframedirectmode[MAX_MBS];
unsigned char VVC1_skipmb[MAX_MBS];





S32 VVC1_dec_init(U8 *streambuf, U32 buffertype, VVC1_fbmem_allocator_fnptr fnptr, U32 image_format) 
{ 
  S32 res;
  int val;
  if (fnptr == NULL) {
    return VVC1_VID_STATUS_ERROR;
  }
  if (vid_lowlevel_init() != VMP_OK) {
    return VVC1_VID_STATUS_ERROR_INIT;
  }
  res = vid_vc1dec_init(streambuf, image_format);
  if (res != VVC1_VID_STATUS_OK) {
    return res;
  }
  VVC1_decoder.fnptr = fnptr;
  val = VVC1_read_sequence_header();
  return (S32)val;
}


S32 VVC1_dec_frame(U8 *streambuf, U32 bufsize, VVC1_VID_timestamp pts, VVC1_VID_timestamp dts, VMP_FrameBuffer **freebuffers_ptr, U32 freebuffers_count, VVC1_VID_DEC_INFO *VVC1_dec_info)
{
  *VVC1_dec_info = vid_vc1dec_decode_frame(streambuf, bufsize, pts, dts, freebuffers_ptr, freebuffers_count);
  return VVC1_dec_info->status;
}


void VVC1_dec_finish(VVC1_fbmem_deallocator_fnptr fnptr) 
{
  if (fnptr != NULL) {
    (*fnptr)(NR_OF_VID_BUFFERS, VVC1_fbmem_frame_buf);
    (*fnptr)(NR_OF_VID_BUFFERS, VVC1_fbmem_disp_buf);
  }
  vid_vc1dec_uninit();
  vid_stop_vmp(0);
  vid_lowlevel_release();
}




int vid_vc1dec_init(unsigned char *seqheader, unsigned image_format)
{
  VID_ADDR_PTRS ic_mem_ptr;
  S32 i;

  if (vid_icm_alloc(&ic_mem_ptr, CORE_USED) != VMP_OK) {
    return VVC1_VID_STATUS_ERROR_INIT;
  }
  *(ic_mem_ptr.virt_addr) = 1;
  *(ic_mem_ptr.virt_addr+(sizeof(CTRL_STRUCT)/sizeof(int))) = 1;

  if (vid_boot_vmp((unsigned char *) &VVC1_vmp2_bin[0], VVC1_vmp2_bin_size, 0) != VMP_OK) {
    return VVC1_VID_STATUS_ERROR_INIT;
  }

  VVC1_init_tables();

  VVC1_Buffer = (unsigned int *) seqheader;

  VVC1_buf_num = 0;
  VVC1_icm_num = 0;

  memset(&VVC1_decoder, 0, sizeof(DECODER));
  memset(&VVC1_colocated_mb, 0, MAX_MBS * sizeof(MB_CONTEXT));

  VVC1_init_buf();

  VVC1_decoder.motionvectortype = VVC1_motionvectortype;
  VVC1_decoder.bframedirectmode = VVC1_bframedirectmode;
  VVC1_decoder.skipmb = VVC1_skipmb;

  VVC1_decoder.frames = 0;

  if (image_format == VVC1_VID_IMAGE_FORMAT_YUV422) {
    VVC1_decoder.yuv422 = 1;
  } else {
    VVC1_decoder.yuv422 = 0;
  }

  VVC1_decoder.width = 1;
  VVC1_decoder.height = 1;
  VVC1_decoder.resize = 0;

  VVC1_decoder.last_coding_type = I_VOP;
  VVC1_decoder.previous_BVOP = 0;
  VVC1_decoder.count = 0;
  VVC1_decoder.toggle = NR_OF_VID_BUFFERS-NR_OF_IP_BUFFERS;
  
  VVC1_decoder.flushed = 0;

  for (i=0; i<NR_OF_VID_BUFFERS; i++) {
    VVC1_pts_buf[i] = (VVC1_VID_timestamp)0;
    VVC1_dts_buf[i] = (VVC1_VID_timestamp)0;
  }

  return VVC1_VID_STATUS_OK;
}



VVC1_VID_DEC_INFO vid_vc1dec_decode_frame(unsigned char *streambuf, unsigned bufsize, VVC1_VID_timestamp pts, VVC1_VID_timestamp dts, 
					  VMP_FrameBuffer **freebuffers_ptr, U32 freebuffers_count)
{
  VVC1_VID_DEC_INFO vid_vc1_info;
  VMP_FrameBuffer *fbmem_420;
  VMP_FrameBuffer *fbmem_422;
  int used_idx = 0;
  int coding_type;
  int retVal;


  // get free buffers
  if (freebuffers_count) {
    int i,j;
    int found_buffer;
    
    if (freebuffers_count > NR_OF_VID_BUFFERS) {
      vid_vc1_info.status = VVC1_VID_STATUS_ERROR;
      return vid_vc1_info;
    }
    
    /* get back free buffers from display subsystem */
    for (i=0; i < freebuffers_count; i++) {
      found_buffer = 0;
            
      for(j=0; j < NR_OF_VID_BUFFERS; j++){
	if (VVC1_decoder.yuv422) {
	  if (VVC1_fbmem_disp_buf[j].phy_addr == freebuffers_ptr[i]->phy_addr) {
	    VVC1_fbmem_buf_used[j] = 0;
	    found_buffer = 1;	    
	  }
	} else {
	  if (VVC1_fbmem_frame_buf[j].phy_addr == freebuffers_ptr[i]->phy_addr) {
	    VVC1_fbmem_buf_used[j] = 0;
	    found_buffer = 1;	    
	  }	
	}
      }

      if (!found_buffer) {
	vid_vc1_info.status = VVC1_VID_STATUS_ERROR;
	return vid_vc1_info;
      }    
    }
  }


  // flush display queue
  if (streambuf == NULL) {

    if (VVC1_decoder.flushed >= FRM_DISP_DELAY) {
      vid_vc1_info.status = VVC1_VID_STATUS_FRAME_NOT_FINISHED;
      return vid_vc1_info;
    }
    
    VVC1_decoder.flushed++;
      
    VVC1_decoder.frames++;
           
    if (!VVC1_decoder.previous_BVOP) {
      used_idx = (VVC1_decoder.count+(NR_OF_IP_BUFFERS-FRM_DISP_DELAY)+(VVC1_decoder.last_coding_type==N_VOP))%NR_OF_IP_BUFFERS;
    } else {
      used_idx = (NR_OF_IP_BUFFERS+VVC1_decoder.toggle)%NR_OF_VID_BUFFERS;
    }
    fbmem_420 = &VVC1_fbmem_frame_buf[used_idx];
    fbmem_422 = &VVC1_fbmem_disp_buf[used_idx];
    vid_vc1_info.pts = VVC1_pts_buf[used_idx];
    vid_vc1_info.dts = VVC1_dts_buf[used_idx];
      
    VVC1_decoder.count++;
    VVC1_decoder.previous_BVOP = 0;
    VVC1_decoder.last_coding_type = I_VOP;

    vid_vc1_info.x_dim_0 = VVC1_decoder.disp_width;
    vid_vc1_info.y_dim_0 = VVC1_decoder.disp_height;
    vid_vc1_info.image_0 = fbmem_420;
    vid_vc1_info.format_0 = VVC1_VID_IMAGE_FORMAT_YUV420;
  
    vid_vc1_info.x_dim_1 = VVC1_decoder.disp_width;
    vid_vc1_info.y_dim_1 = VVC1_decoder.disp_height;
    vid_vc1_info.image_1 = fbmem_422;
    vid_vc1_info.format_1 = VVC1_VID_IMAGE_FORMAT_YUV422;
  
    if (VVC1_decoder.frames <= FRM_DISP_DELAY) {
      vid_vc1_info.status = VVC1_VID_STATUS_FRAME_NOT_FINISHED;
    } else {
      vid_vc1_info.status = VVC1_VID_STATUS_OK;
    }

    vid_vc1_info.new_buffer_request = VVC1_VID_STATUS_FRAME_END;
    vid_vc1_info.error_resilience_status = VVC1_VID_STATUS_OK;
      
    return vid_vc1_info;
  }


  // restart nach flushed display queue
  if (VVC1_decoder.flushed > 0) {    
    VVC1_decoder.frames = 0;
    VVC1_decoder.previous_BVOP = 0;
    VVC1_decoder.count -= FRM_DISP_DELAY;
    VVC1_decoder.toggle = NR_OF_VID_BUFFERS-NR_OF_IP_BUFFERS;
    VVC1_decoder.flushed = 0;
  }



  VVC1_Buffer = (unsigned int *) streambuf;
  VVC1_BufferEnd = (unsigned int *) (streambuf + bufsize);
  VVC1_init_buf();

 repeat:

  coding_type =	VVC1_read_headers();

  if (coding_type == -1) {
    goto repeat;
  }
  
  if (!VVC1_decoder.resize) {
    vid_vc1_info.status = VVC1_VID_STATUS_ERROR_UNSUPPORTED;
    return vid_vc1_info;  
  }

  if (!VVC1_decoder.reserved && coding_type != I_VOP) {
    vid_vc1_info.status = VVC1_VID_STATUS_FRAME_NOT_FINISHED;
    return vid_vc1_info;  
  }

  if (coding_type == 2) {
    VVC1_decoder.sp_info = (3 << 16) | 3;
  } else if (coding_type == 3) {
    VVC1_decoder.sp_info = (2 << 16) | 3;
  } else {
    VVC1_decoder.sp_info = (coding_type << 16) | 3;
  }

  VVC1_cs[VVC1_buf_num]->YUV422 = VVC1_decoder.yuv422;


  if (coding_type == N_VOP)
    {
      if (!VVC1_decoder.previous_BVOP) {
	used_idx = (VVC1_decoder.count+(NR_OF_IP_BUFFERS-FRM_DISP_DELAY)+(VVC1_decoder.last_coding_type==N_VOP))%NR_OF_IP_BUFFERS;
      } else {
	used_idx = (NR_OF_IP_BUFFERS+VVC1_decoder.toggle)%NR_OF_VID_BUFFERS;
      }

      if (VVC1_fbmem_buf_used[VVC1_decoder.count%NR_OF_IP_BUFFERS]) {
	vid_vc1_info.status = VVC1_VID_STATUS_ERROR_NOBUFFER;
	vid_vc1_info.pts = VVC1_pts_buf[used_idx];
	vid_vc1_info.dts = VVC1_dts_buf[used_idx];
	return vid_vc1_info;
      } else {
	VVC1_cs[VVC1_buf_num]->DISP_BUF = 
	  ((unsigned int)VVC1_fbmem_disp_buf[VVC1_decoder.count%NR_OF_IP_BUFFERS].phy_addr)>>3;

	VVC1_fbmem_buf_used[VVC1_decoder.count%NR_OF_IP_BUFFERS] = 1;
      }

      fbmem_420 = &VVC1_fbmem_frame_buf[used_idx];
      fbmem_422 = &VVC1_fbmem_disp_buf[used_idx];
      vid_vc1_info.pts = VVC1_pts_buf[used_idx];
      vid_vc1_info.dts = VVC1_dts_buf[used_idx];

      VVC1_pts_buf[VVC1_decoder.count%NR_OF_IP_BUFFERS] = pts;
      VVC1_dts_buf[VVC1_decoder.count%NR_OF_IP_BUFFERS] = dts;

      VVC1_decoder.previous_BVOP = 0;
      VVC1_decoder.last_coding_type = coding_type;
    }
  else if ((coding_type != B_VOP) && (coding_type != BI_VOP))
    {
      if (!VVC1_decoder.previous_BVOP) {
	used_idx = (VVC1_decoder.count+(NR_OF_IP_BUFFERS-FRM_DISP_DELAY)+(VVC1_decoder.last_coding_type==N_VOP))%NR_OF_IP_BUFFERS;
      } else {
	used_idx = (NR_OF_IP_BUFFERS+VVC1_decoder.toggle)%NR_OF_VID_BUFFERS;
      }

      if (VVC1_fbmem_buf_used[VVC1_decoder.count%NR_OF_IP_BUFFERS]) {
	vid_vc1_info.status = VVC1_VID_STATUS_ERROR_NOBUFFER;
	vid_vc1_info.pts = VVC1_pts_buf[used_idx];
	vid_vc1_info.dts = VVC1_dts_buf[used_idx];
	return vid_vc1_info;
      } else {
	VVC1_cs[VVC1_buf_num]->DISP_BUF = 
	  ((unsigned int)VVC1_fbmem_disp_buf[VVC1_decoder.count%NR_OF_IP_BUFFERS].phy_addr)>>3;

	VVC1_fbmem_buf_used[VVC1_decoder.count%NR_OF_IP_BUFFERS] = 1;
      }

      switch(coding_type)
	{
	case I_VOP :
	  retVal = VVC1_decode_iframe();
	  break;
	case P_VOP :
	  retVal = VVC1_decode_pframe();
	  break;
	}

      if (retVal != VVC1_VID_STATUS_OK) {
	vid_vc1_info.status = retVal;
	return vid_vc1_info;
      }

      fbmem_420 = &VVC1_fbmem_frame_buf[used_idx];
      fbmem_422 = &VVC1_fbmem_disp_buf[used_idx];
      vid_vc1_info.pts = VVC1_pts_buf[used_idx];
      vid_vc1_info.dts = VVC1_dts_buf[used_idx];

      VVC1_pts_buf[VVC1_decoder.count%NR_OF_IP_BUFFERS] = pts;
      VVC1_dts_buf[VVC1_decoder.count%NR_OF_IP_BUFFERS] = dts;

      VVC1_decoder.count++;
      VVC1_decoder.previous_BVOP = 0;
      VVC1_decoder.last_coding_type = coding_type;
    }
  else
    {  
      if (!VVC1_decoder.previous_BVOP) {
	used_idx = (VVC1_decoder.count+(NR_OF_IP_BUFFERS-FRM_DISP_DELAY)+(VVC1_decoder.last_coding_type==N_VOP))%NR_OF_IP_BUFFERS;
      } else {
	used_idx = (NR_OF_IP_BUFFERS+VVC1_decoder.toggle)%NR_OF_VID_BUFFERS;
      }

      if (VVC1_fbmem_buf_used[NR_OF_IP_BUFFERS+((VVC1_decoder.toggle+1)%(NR_OF_VID_BUFFERS-NR_OF_IP_BUFFERS))]) {
	vid_vc1_info.status = VVC1_VID_STATUS_ERROR_NOBUFFER;
	vid_vc1_info.pts = VVC1_pts_buf[used_idx];
	vid_vc1_info.dts = VVC1_dts_buf[used_idx];
	return vid_vc1_info;
      } else {
	VVC1_cs[VVC1_buf_num]->DISP_BUF = 
	  ((unsigned int)VVC1_fbmem_disp_buf[NR_OF_IP_BUFFERS+((VVC1_decoder.toggle+1)%(NR_OF_VID_BUFFERS-NR_OF_IP_BUFFERS))].phy_addr)>>3;

	VVC1_fbmem_buf_used[NR_OF_IP_BUFFERS+((VVC1_decoder.toggle+1)%(NR_OF_VID_BUFFERS-NR_OF_IP_BUFFERS))] = 1;
      }

      switch(coding_type)
	{
	case BI_VOP :
	  retVal = VVC1_decode_iframe();
	  break;
	case B_VOP :
	  retVal = VVC1_decode_bframe();
	  break;
	}

      if (retVal != VVC1_VID_STATUS_OK) {
	vid_vc1_info.status = retVal;
	return vid_vc1_info;
      }

      fbmem_420 = &VVC1_fbmem_frame_buf[used_idx];
      fbmem_422 = &VVC1_fbmem_disp_buf[used_idx];
      vid_vc1_info.pts = VVC1_pts_buf[used_idx];
      vid_vc1_info.dts = VVC1_dts_buf[used_idx];

      VVC1_pts_buf[NR_OF_IP_BUFFERS+((VVC1_decoder.toggle+1)%(NR_OF_VID_BUFFERS-NR_OF_IP_BUFFERS))] = pts;
      VVC1_dts_buf[NR_OF_IP_BUFFERS+((VVC1_decoder.toggle+1)%(NR_OF_VID_BUFFERS-NR_OF_IP_BUFFERS))] = dts;

      VVC1_decoder.toggle = (VVC1_decoder.toggle+1)%(NR_OF_VID_BUFFERS-NR_OF_IP_BUFFERS);
      VVC1_decoder.previous_BVOP = 1;
      VVC1_decoder.last_coding_type = coding_type;
    }

  VVC1_decoder.frames++;
  
  VVC1_cs[VVC1_buf_num]->DEC_BUF = 
    ((unsigned int)(VVC1_fbmem_frame_buf[VVC1_decoder.count%NR_OF_IP_BUFFERS].phy_addr))>>3;
  VVC1_cs[VVC1_buf_num]->REF1_BUF = 
    ((unsigned int)(VVC1_fbmem_frame_buf[((NR_OF_IP_BUFFERS+VVC1_decoder.count)-1)%NR_OF_IP_BUFFERS].phy_addr))>>3;
  VVC1_cs[VVC1_buf_num]->REF2_BUF = 
    ((unsigned int)(VVC1_fbmem_frame_buf[((NR_OF_IP_BUFFERS+VVC1_decoder.count)-2)%NR_OF_IP_BUFFERS].phy_addr))>>3;
  VVC1_cs[VVC1_buf_num]->BDEC_BUF = 
    ((unsigned int)(VVC1_fbmem_frame_buf[NR_OF_IP_BUFFERS+((VVC1_decoder.toggle+1)%(NR_OF_VID_BUFFERS-NR_OF_IP_BUFFERS))].phy_addr))>>3;


  vid_vc1_info.x_dim_0 = VVC1_decoder.disp_width;
  vid_vc1_info.y_dim_0 = VVC1_decoder.disp_height;
  vid_vc1_info.image_0 = fbmem_420;
  vid_vc1_info.format_0 = VVC1_VID_IMAGE_FORMAT_YUV420;
  
  vid_vc1_info.x_dim_1 = VVC1_decoder.disp_width;
  vid_vc1_info.y_dim_1 = VVC1_decoder.disp_height;
  vid_vc1_info.image_1 = fbmem_422;
  vid_vc1_info.format_1 = VVC1_VID_IMAGE_FORMAT_YUV422;
  
  if (VVC1_decoder.frames <= FRM_DISP_DELAY) {
    vid_vc1_info.status = VVC1_VID_STATUS_FRAME_NOT_FINISHED;
  } else {
    vid_vc1_info.status = VVC1_VID_STATUS_OK;
  }

  vid_vc1_info.new_buffer_request = VVC1_VID_STATUS_FRAME_END;
  if (VVC1_decoder.fault == 0) {
    vid_vc1_info.error_resilience_status = VVC1_VID_STATUS_OK;
  } else {
    vid_vc1_info.error_resilience_status = VVC1_VID_STATUS_ERROR;
  }

  return vid_vc1_info;
}


int vid_vc1dec_uninit(void) {

  return VVC1_VID_STATUS_OK;
}







#ifdef _INDIRECT
#ifndef LINUX
void VVC1_vid_setup_dmasync(ID * id_ptr) 
{
  T_CFLG flg = {0} ;
  ID flg_id = E_OK ;
            
  flg.flgatr = TA_TFIFO | TA_WMUL ;
  flg.iflgptn = 0 ;
  flg_id = tk_cre_flg(&flg) ; 
  *id_ptr = flg_id ;
}
#endif
#endif


/*lint -restore */
/* PRQA L:L1 */
