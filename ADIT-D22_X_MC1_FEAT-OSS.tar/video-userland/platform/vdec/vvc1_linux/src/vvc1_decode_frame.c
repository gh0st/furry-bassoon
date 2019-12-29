/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *                                                                          
 * DESCRIPTION:         frame related decoding routines
 *                                                                          
 *+++++++++++++++++++++++++++++ FileHeaderEnd ++++++++++++++++++++++++++++++*/
/*
 * Copyright (c) 2006 Konstantin Shishkov
 * Partly based on vc9.c (c) 2005 Anonymous, Alex Beregszaszi, Michael Niedermayer
 *
 * This file is partly based on FFmpeg.
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
 */


#include <arpa/inet.h>

#include "adit_typedef.h"

#include "vvc1_dec.h"
#include "vvc1_buffer.h"
#include "vvc1_decoder.h"
#include "vvc1_bitstream.h"
#include "vvc1_tables.h"
#include "vvc1_decode_frame.h"
#include "vvc1_decode_mb.h"
#include "vvc1_ctrl_struct.h"

/* PRQA: QAC Message 277, 926, 3201, 3334, 3353, 3760, 3771: deactivation because of pre-existing IP*/
/* PRQA S 277, 926, 3201, 3334, 3353, 3760, 3771 L1*/
/* PRQA: Lint Message 10, 40, 529, 578, 644, 661, 662, 774, 826:deactivation because of pre-existing IP */
/*lint -save -e10 -e40 -e529 -e578 -e644 -e661 -e662 -e774 -e826*/


int VVC1_decode_iframe()
{
  unsigned int x, y;
  unsigned mb_width = VVC1_decoder.mb_width;
  unsigned mb_height = VVC1_decoder.mb_height;
  int blk_type;

  switch(VVC1_decoder.frameACcodingindex2){
  case 0:
    VVC1_decoder.codingset = (VVC1_decoder.pqindex <= 8) ? CS_HIGH_RATE_INTRA : CS_LOW_MOT_INTRA;
    break;
  case 1:
    VVC1_decoder.codingset = CS_HIGH_MOT_INTRA;
    break;
  case 2:
    VVC1_decoder.codingset = CS_MID_RATE_INTRA;
    break;
  }

  switch(VVC1_decoder.frameACcodingindex){
  case 0:
    VVC1_decoder.codingset2 = (VVC1_decoder.pqindex <= 8) ? CS_HIGH_RATE_INTER : CS_LOW_MOT_INTER;
    break;
  case 1:
    VVC1_decoder.codingset2 = CS_HIGH_MOT_INTER;
    break;
  case 2:
    VVC1_decoder.codingset2 = CS_MID_RATE_INTER;
    break;
  }

  VVC1_decoder.esc3_level_length = 0;

  {
    int i, j;
    for (j=0; j<MAX_PRED_DC_MBS; j++) {
      for (i=0; i<4; i++) {
	VVC1_predict_mem[j].coded[i] = 0;
      }
    }
  }

  for (y = 0; y < mb_height; y++) {
    {
      int i;
      for (i=0; i<4; i++) {
	VVC1_predict_left.coded[i] = 0;
      }
    }


    for (x = 0; x < mb_width; x++) {
      int acpred_flag;
      int cbp;
      
      VVC1_cs[VVC1_buf_num]->MB_y = y;
      VVC1_cs[VVC1_buf_num]->MB_x = x;
      
      VVC1_predict_current.quant = VVC1_decoder.pquant;
      
      VVC1_decoder.dcscale = VVC1_dc_scale_table[VVC1_decoder.pquant];

      if (VVC1_show_bits(3) > 3) {
	cbp = 0;
	VVC1_flush_bits(1);
      } else if (VVC1_show_bits(3) == 0) {
	cbp = I_Picture_CBPCY_VLC_1[VVC1_show_bits(9)].value;
	VVC1_flush_bits(I_Picture_CBPCY_VLC_1[VVC1_show_bits(9)].length);
      } else if (VVC1_show_bits(5) < 7) {
	cbp = I_Picture_CBPCY_VLC_2[VVC1_show_bits(9) & 0x3f].value;
	VVC1_flush_bits(I_Picture_CBPCY_VLC_2[VVC1_show_bits(9) & 0x3f].length);
      } else if (VVC1_show_bits(3) > 1) {
	cbp = I_Picture_CBPCY_VLC_3[VVC1_show_bits(11) & 0x1ff].value;
	VVC1_flush_bits(I_Picture_CBPCY_VLC_3[VVC1_show_bits(11) & 0x1ff].length);
      } else {
	cbp = I_Picture_CBPCY_VLC_4[VVC1_show_bits(13) & 0xff].value;
	VVC1_flush_bits(I_Picture_CBPCY_VLC_4[VVC1_show_bits(13) & 0xff].length);
      }
	  
      acpred_flag = VVC1_get_bits(1);

      VVC1_decode_mbintra(x, y, cbp, acpred_flag);

      if ((VVC1_decoder.pquant >= 9) && VVC1_decoder.overlappedtransform)
	blk_type = 0;
      else
	blk_type = 0x3f;

      if(x) {
	int iik ;
	VVC1_predict_mem[x-1].quant = VVC1_predict_left.quant ;
	for (iik=0;iik<6;iik++) {
	  VVC1_predict_mem[x-1].dc_value[iik] = VVC1_predict_left.dc_value[iik] ;
	}
	for (iik=0;iik<4;iik++) {
	  VVC1_predict_mem[x-1].coded[iik] = VVC1_predict_left.coded[iik] ;
	}
      }
      {  
	int iik ;
	VVC1_predict_left.quant = VVC1_predict_current.quant ;
	for (iik=0;iik<6;iik++) {
	  VVC1_predict_left.dc_value[iik] = VVC1_predict_current.dc_value[iik] ;
	}
	for (iik=0;iik<4;iik++) {
	  VVC1_predict_left.coded[iik] = VVC1_predict_current.coded[iik] ;
	}
      }
      if(x == VVC1_decoder.mb_width-1) {
	int iik ;
	VVC1_predict_mem[x].quant = VVC1_predict_current.quant ;
	for (iik=0;iik<6;iik++) {
	  VVC1_predict_mem[x].dc_value[iik] = VVC1_predict_current.dc_value[iik] ;
	}
	for (iik=0;iik<4;iik++) {
	  VVC1_predict_mem[x].coded[iik] = VVC1_predict_current.coded[iik] ;
	}
      } 

      VVC1_mp_sync();

      VVC1_icm_num = (VVC1_icm_num + 1) % ICM_BUFFERS_USED;

      VVC1_cs[VVC1_buf_num]->MB_TYPE = Intra | (blk_type << 4) | ((unsigned)(VVC1_icm_num*0x80)<<16);
      VVC1_cs[VVC1_buf_num]->SP_INFO = VVC1_decoder.sp_info;

#ifdef _INDIRECT
      VVC1_buf_num = (VVC1_buf_num + 1);
#else
      VVC1_buf_num = (VVC1_buf_num + 1) % ICM_BUFFERS_USED;
#endif

      VVC1_decoder.sp_info &= (unsigned)-3;

#ifdef _INDIRECT
      if (VVC1_buf_num == VVC1_decoder.groupHandle.buffergroup_size) {
	int rc;

	VVC1_buf_num = 0;
	
	rc = vid_send_buffers(&VVC1_decoder.groupHandle) ;
	if (VMP_E == rc) {
	  return VVC1_VID_STATUS_ERROR;
	}

	rc = vid_get_buffers(CORE_USED, &VVC1_decoder.groupHandle, 1); // blocking 
	if (rc != VMP_OK) {
	  return VVC1_VID_STATUS_ERROR_MP_TIMEOUT;
	} else {
	  VVC1_cs[0] = (CTRL_STRUCT  *)VVC1_decoder.groupHandle.address.virt_addr;
	  {
	    int i;
	    for (i = 1; i < VVC1_decoder.groupHandle.buffergroup_size; i++) {
	      VVC1_cs[i] = VVC1_cs[0] + i;
	    }
	  }
	}
      }
#endif
    }
  }

  return VVC1_VID_STATUS_OK;
}


static inline int median3(int a, int b, int c)
{
    if(a>b){
        if(c>b){
            if(c>a) b=a;
            else    b=c;
        }
    }else{
        if(b>c){
            if(c>a) b=c;
            else    b=a;
        }
    }
    return b;
}

#define VVC1_MAX(a,b) ((a) > (b) ? (a) : (b))
#define VVC1_MIN(a,b) ((a) > (b) ? (b) : (a))

static inline int median4(int a, int b, int c, int d)
{
    if(a < b) {
        if(c < d) return (VVC1_MIN(b, d) + VVC1_MAX(a, c)) / 2;
        else      return (VVC1_MIN(b, c) + VVC1_MAX(a, d)) / 2;
    } else {
        if(c < d) return (VVC1_MIN(a, d) + VVC1_MAX(b, c)) / 2;
        else      return (VVC1_MIN(a, c) + VVC1_MAX(b, d)) / 2;
    }
}


int VVC1_decode_pframe()
{
  unsigned int x, y;
  unsigned int mb_width = VVC1_decoder.mb_width;
  unsigned int mb_height = VVC1_decoder.mb_height;
  mb_t mb_type;
  mvunit_t mvunit_fwd;
  int blk_type;

  int dmv_x, dmv_y;
  int fourmv;
  int skipped;
  int mquant;

  int index, index1;

  int k_x, k_y;
  int r_x, r_y;

  static const int size_table[6] = { 0, 2, 3, 4, 5, 8 };
  static const int offset_table[6] = { 0, 1, 3, 7, 15, 31 };
  int mb_has_coeffs = 1;

  VVC1_cs[VVC1_buf_num]->MC_FLAGS = (VVC1_decoder.luminancescale << 24) | (VVC1_decoder.luminanceshift << 16) | 
    (VVC1_decoder.intensitycompflag << 5) | ((VVC1_decoder.rangeYscale != 8) << 4) | 
    (VVC1_decoder.fastumvc << 3) | ((VVC1_decoder.rndctrl & 0x1) << 2) | 
    (VVC1_decoder.quarterpel << 1) | (VVC1_decoder.mvmode == vc1_MVMode1MVHalfPelBilinear);

  switch(VVC1_decoder.frameACcodingindex){
  case 0:
    VVC1_decoder.codingset = (VVC1_decoder.pqindex <= 8) ? CS_HIGH_RATE_INTRA : CS_LOW_MOT_INTRA;
    break;
  case 1:
    VVC1_decoder.codingset = CS_HIGH_MOT_INTRA;
    break;
  case 2:
    VVC1_decoder.codingset = CS_MID_RATE_INTRA;
    break;
  }
  switch(VVC1_decoder.frameACcodingindex){
  case 0:
    VVC1_decoder.codingset2 = (VVC1_decoder.pqindex <= 8) ? CS_HIGH_RATE_INTER : CS_LOW_MOT_INTER;
    break;
  case 1:
    VVC1_decoder.codingset2 = CS_HIGH_MOT_INTER;
    break;
  case 2:
    VVC1_decoder.codingset2 = CS_MID_RATE_INTER;
    break;
  }

  VVC1_decoder.esc3_level_length = 0;

  k_x = VVC1_decoder.mvrange + 9 + (VVC1_decoder.mvrange >> 1);
  k_y = VVC1_decoder.mvrange + 8;
  r_x = 1 << (k_x - 1);
  r_y = 1 << (k_y - 1);
  
  {
    int i, j;
    for (j=0; j<MAX_PRED_DC_MBS; j++) {
      for (i=0; i<6; i++) {
	VVC1_predict_mem[j].is_intra[i] = 0;
      }
      for (i=0; i<4; i++) {
	VVC1_predict_mem[j].coded[i] = 0;
	VVC1_predict_mem[j].fmv[i].x = 0;
	VVC1_predict_mem[j].fmv[i].y = 0;
      }
    }
  }

  for (y = 0; y < mb_height; y++) {
    {
      int i;
      for (i=0; i<6; i++) {
	VVC1_predict_left.is_intra[i] = 0;
      }
      for (i=0; i<4; i++) {
	VVC1_predict_left.coded[i] = 0;
	VVC1_predict_left.fmv[i].x = 0;
	VVC1_predict_left.fmv[i].y = 0;
      }
    }
    
    for (x = 0; x < mb_width; x++) {
      MB_CONTEXT *cmb;
      int acpred_flag;
      int cbp;
      int ttmb; 
      
      acpred_flag = 0;
      ttmb = VVC1_decoder.frametransformtype; 

      cmb = &VVC1_colocated_mb[y * mb_width + x];
      
      mquant = VVC1_decoder.pquant;
      
      mb_type = Forward;
      
      blk_type = 0;

      if (VVC1_decoder.motionvectortype_rawmode)
	fourmv = VVC1_get_bits(1);
      else
	fourmv = VVC1_motionvectortype[y*mb_width + x];
	  
      if (VVC1_decoder.skipmb_rawmode)
	skipped = VVC1_get_bits(1);
      else
	skipped = VVC1_skipmb[y*mb_width + x];

      if (!fourmv) {

	mvunit_fwd = _1MV;

	if (!skipped) {
	  int is_intra[6];
	  int mb_intra;
	  unsigned int i;
	  int val;
	  int sign;

	  {
	    unsigned code;
	    int n;

	    code = VVC1_show_bits(9);

	    n     = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code][1];
	    index = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code][0];
		
	    if (n < 0){
	      VVC1_flush_bits(9);
	      code = VVC1_show_bits(-n);
		  
	      n     = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code + index][1];
	      index = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code + index][0];
	    }

	    VVC1_flush_bits(n);
	  }

	  index ++;		   
              
	  if (index > 36) {
	    mb_has_coeffs = 1;                                              
	    index -= 37;
	  } else 
	    mb_has_coeffs = 0;

	  mb_intra = 0;

	  if (!index) { 
	    dmv_x = 0;
	    dmv_y = 0; 
	  } else if (index == 35) {
	    dmv_x = VVC1_get_bits(k_x - 1 + VVC1_decoder.quarterpel);
	    dmv_y = VVC1_get_bits(k_y - 1 + VVC1_decoder.quarterpel);
	  } else if (index == 36) { 
	    dmv_x = 0;
	    dmv_y = 0;
	    mb_intra = 1;                                                
	  } else {
	    index1 = index % 6;
	    if (!VVC1_decoder.quarterpel && index1 == 5) 
	      val = 1;
	    else                                   
	      val = 0;
	    if(size_table[index1] - val > 0)
	      val = VVC1_get_bits(size_table[index1] - val);
	    else
	      val = 0;
	    sign = 0 - (val&1);
	    dmv_x = (sign ^ ((val>>1) + offset_table[index1])) - sign;
		      
	    index1 = index / 6;
	    if (!VVC1_decoder.quarterpel && index1 == 5)
	      val = 1;
	    else
	      val = 0;
	    if(size_table[index1] - val > 0)
	      val = VVC1_get_bits(size_table[index1] - val);
	    else
	      val = 0;
	    sign = 0 - (val&1);
	    dmv_y = (sign ^ ((val>>1) + offset_table[index1])) - sign;
	  }

	  if (mb_intra) {
	    unsigned int i;
	    mb_type = Intra;
	    for (i=0; i<4; i++) {
	      VVC1_predict_current.fmv[i].x = 0;
	      VVC1_predict_current.fmv[i].y = 0;
	    }
	  } else {
	    VECTOR *left, *top, *diag;
	    int px, py;
	    int sum;

	    dmv_x <<= 1 - VVC1_decoder.quarterpel;
	    dmv_y <<= 1 - VVC1_decoder.quarterpel;

	    left = &VVC1_predict_left.fmv[1];
	    top = &VVC1_predict_mem[x].fmv[2];
	    if (x == mb_width-1)
	      diag = &VVC1_predict_mem[x-1].fmv[3];
	    else
	      diag = &VVC1_predict_mem[x+1].fmv[2];
		    
	    if (y) {
	      if (mb_width == 1) {
		px = top->x;
		py = top->y;
	      } else {
		px = diag->x;
		if (top->x > diag->x) {
		  if (left->x > diag->x) {
		    if (left->x > top->x)
		      px = top->x;
		    else
		      px = left->x;
		  }
		} else {
		  if (diag->x > left->x) {
		    if (left->x > top->x)
		      px = left->x;
		    else
		      px = top->x;
		  }
		}
		py = diag->y;
		if (top->y > diag->y) {
		  if (left->y > diag->y) {
		    if (left->y > top->y)
		      py = top->y;
		    else
		      py = left->y;
		  }
		} else {
		  if (diag->y > left->y) {
		    if (left->y > top->y)
		      py = left->y;
		    else
		      py = top->y;
		  }
		}
	      }
	    } else if (x) {
	      px = left->x;
	      py = left->y;
	    } else {
	      px = 0;
	      py = 0;
	    }

	    {
	      int qx, qy, X, Y;

	      qx = x<<6;
	      qy = y<<6;
	      X = (mb_width<<6) - 4;
	      Y = (mb_height<<6) - 4;
	      if (qx+px < -60) 
		px = -60 - qx;
	      if (qy+py < -60) 
		py = -60 - qy;
	      if (qx+px > X) 
		px = X - qx;
	      if (qy+py > Y) 
		py = Y - qy;
	    }
		    
	    if (y && x) {
	      if (VVC1_predict_mem[x].is_intra[2]) {
		sum = ((px < 0) ? -px : px) + ((py < 0) ? -py : py);
	      } else {
		sum = (((px - top->x) < 0) ? -(px - top->x) : (px - top->x)) + 
		  (((py - top->y) < 0) ? -(py - top->y) : (py - top->y));
	      }

	      if (sum > 32) {
		if (VVC1_get_bits(1)) {
		  px = top->x;
		  py = top->y;
		} else {
		  px = left->x;
		  py = left->y;
		}
	      } else {
		if (VVC1_predict_left.is_intra[1]) { 
		  sum = ((px < 0) ? -px : px) + ((py < 0) ? -py : py);
		} else {
		  sum = (((px - left->x) < 0) ? -(px - left->x) : (px - left->x)) + 
		    (((py - left->y) < 0) ? -(py - left->y) : (py - left->y));
		}

		if (sum > 32) {
		  if (VVC1_get_bits(1)) {
		    px = top->x;
		    py = top->y;
		  } else {
		    px = left->x;
		    py = left->y;
		  }
		}
	      }
	    }

	    blk_type = 0x3f;

	    VVC1_predict_current.fmv[0].x = ((px + dmv_x + r_x) & ((r_x << 1) - 1)) - r_x;
	    VVC1_predict_current.fmv[0].y = ((py + dmv_y + r_y) & ((r_y << 1) - 1)) - r_y;
		
	    for (i=1; i<4; i++) {
	      VVC1_predict_current.fmv[i] = VVC1_predict_current.fmv[0];
	    }

	  }

	  if (mb_intra && !mb_has_coeffs) {
	    if (VVC1_decoder.dquantframe) {
	      if (VVC1_decoder.quantmode == vc1_QuantModeMBDual) { 
		mquant = (VVC1_get_bits(1)) ? VVC1_decoder.altpquant : VVC1_decoder.pquant;
	      } else if (VVC1_decoder.quantmode == vc1_QuantModeMBAny) {
		int mqdiff = VVC1_get_bits(3);
		if (mqdiff != 7) 
		  mquant = VVC1_decoder.pquant + mqdiff;
		else 
		  mquant = VVC1_get_bits(5);
	      } else {
		if((VVC1_decoder.quantmode&1) && !x)
		  mquant = VVC1_decoder.altpquant;
		if((VVC1_decoder.quantmode&2) && !y)
		  mquant = VVC1_decoder.altpquant;
		if((VVC1_decoder.quantmode&4) && x == (mb_width - 1))
		  mquant = VVC1_decoder.altpquant;
		if((VVC1_decoder.quantmode&8) && y == (mb_height - 1))
		  mquant = VVC1_decoder.altpquant;
	      }
	    }
	    acpred_flag = VVC1_get_bits(1);
	    cbp = 0;
	  } else if (mb_has_coeffs) {
	    if (mb_intra) {
	      acpred_flag = VVC1_get_bits(1);
	    }
	    {
	      unsigned code;
	      int n;

	      code = VVC1_show_bits(9);
	      n     = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code][1];
	      cbp   = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code][0];
	      if (n < 0){
		VVC1_flush_bits(9);
		code = VVC1_show_bits(-n);
		n     = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code + cbp][1];
		cbp   = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code + cbp][0];
	      }
	      VVC1_flush_bits(n);
	    }

	    if (VVC1_decoder.dquantframe) {
	      if (VVC1_decoder.quantmode == vc1_QuantModeMBDual) { 
		mquant = (VVC1_get_bits(1)) ? VVC1_decoder.altpquant : VVC1_decoder.pquant;
	      } else if (VVC1_decoder.quantmode == vc1_QuantModeMBAny) {
		int mqdiff = VVC1_get_bits(3);
		if (mqdiff != 7) 
		  mquant = VVC1_decoder.pquant + mqdiff;
		else 
		  mquant = VVC1_get_bits(5);
	      } else {
		if((VVC1_decoder.quantmode&1) && !x)
		  mquant = VVC1_decoder.altpquant;
		if((VVC1_decoder.quantmode&2) && !y)
		  mquant = VVC1_decoder.altpquant;
		if((VVC1_decoder.quantmode&4) && x == (mb_width - 1))
		  mquant = VVC1_decoder.altpquant;
		if((VVC1_decoder.quantmode&8) && y == (mb_height - 1))
		  mquant = VVC1_decoder.altpquant;
	      }
	    }
	  } else {
	    mquant = VVC1_decoder.pquant;
	    cbp = 0;
	  }


	  VVC1_predict_current.quant = mquant;


	  if (!VVC1_decoder.mbtransformflag && !mb_intra && mb_has_coeffs) {
	    {
	      unsigned code;
	      int n;

	      code = VVC1_show_bits(9);

	      n     = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code][1];
	      ttmb  = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code][0];
		
	      if (n < 0){
		VVC1_flush_bits(9);
		code = VVC1_show_bits(-n);
		  
		n     = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code + ttmb][1];
		ttmb  = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code + ttmb][0];
	      }
	      VVC1_flush_bits(n);
	    }
	  }

	  for (i=0; i<6; i++) {
	    is_intra[i] = mb_intra;
	    VVC1_predict_current.is_intra[i] = mb_intra;
	  }
	  for (i=0; i<4; i++) {
	    VVC1_predict_current.coded[i] = mb_has_coeffs;
	  }

	  VVC1_decode_mbinter_intra(x, y, cbp, is_intra, mquant, acpred_flag, ttmb);

	} else {
	  VECTOR *left, *top, *diag;
	  int px, py;
	  int sum;
	  unsigned int i;

	  dmv_x = 0;
	  dmv_y = 0;

	  left = &VVC1_predict_left.fmv[1];
	  top = &VVC1_predict_mem[x].fmv[2];
	  if (x == mb_width-1)
	    diag = &VVC1_predict_mem[x-1].fmv[3];
	  else
	    diag = &VVC1_predict_mem[x+1].fmv[2];
		    
	  if (y) {
	    if (mb_width == 1) {
	      px = top->x;
	      py = top->y;
	    } else {
	      px = diag->x;
	      if (top->x > diag->x) {
		if (left->x > diag->x) {
		  if (left->x > top->x)
		    px = top->x;
		  else
		    px = left->x;
		}
	      } else {
		if (diag->x > left->x) {
		  if (left->x > top->x)
		    px = left->x;
		  else
		    px = top->x;
		}
	      }
	      py = diag->y;
	      if (top->y > diag->y) {
		if (left->y > diag->y) {
		  if (left->y > top->y)
		    py = top->y;
		  else
		    py = left->y;
		}
	      } else {
		if (diag->y > left->y) {
		  if (left->y > top->y)
		    py = left->y;
		  else
		    py = top->y;
		}
	      }
	    }
	  } else if (x) {
	    px = left->x;
	    py = left->y;
	  } else {
	    px = 0;
	    py = 0;
	  }

	  {
	    int qx, qy, X, Y;

	    qx = x<<6;
	    qy = y<<6;
	    X = (mb_width<<6) - 4;
	    Y = (mb_height<<6) - 4;
	    if (qx+px < -60) 
	      px = -60 - qx;
	    if (qy+py < -60) 
	      py = -60 - qy;
	    if (qx+px > X) 
	      px = X - qx;
	    if (qy+py > Y) 
	      py = Y - qy;
	  }

	  if (y && x) {
	    if (VVC1_predict_mem[x].is_intra[2]) {
	      sum = ((px < 0) ? -px : px) + ((py < 0) ? -py : py);
	    } else {
	      sum = (((px - top->x) < 0) ? -(px - top->x) : (px - top->x)) + 
		(((py - top->y) < 0) ? -(py - top->y) : (py - top->y));
	    }

	    if (sum > 32) {
	      if (VVC1_get_bits(1)) {
		px = top->x;
		py = top->y;
	      } else {
		px = left->x;
		py = left->y;
	      }
	    } else {
	      if (VVC1_predict_left.is_intra[1]) {
		sum = ((px < 0) ? -px : px) + ((py < 0) ? -py : py);
	      } else {
		sum = (((px - left->x) < 0) ? -(px - left->x) : (px - left->x)) + 
		  (((py - left->y) < 0) ? -(py - left->y) : (py - left->y));
	      }

	      if (sum > 32) {
		if (VVC1_get_bits(1)) {
		  px = top->x;
		  py = top->y;
		} else {
		  px = left->x;
		  py = left->y;
		}
	      }
	    }
	  }

	  blk_type = 0x3f;

	  VVC1_predict_current.fmv[0].x = ((px + dmv_x + r_x) & ((r_x << 1) - 1)) - r_x;
	  VVC1_predict_current.fmv[0].y = ((py + dmv_y + r_y) & ((r_y << 1) - 1)) - r_y;
	  VVC1_predict_current.coded[0] = 0;
	  
	  for (i=1; i<4; i++) {
	    VVC1_predict_current.fmv[i] = VVC1_predict_current.fmv[0];
	    VVC1_predict_current.coded[i] = 0;
	  }

	  for (i=0; i<6; i++) {
	    VVC1_predict_current.is_intra[i] = 0;

#ifdef _INDIRECT
#ifdef _GNUCC
	    { 
	      register U32 *dst_ptr = (U32 *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];

	      asm volatile ( "mov r2, #0\n\t"
			     "mov r3, #0\n\t"
			     "mov r4, #0\n\t"
			     "mov r5, #0\n\t"
			     :
			     :
			     );
	      asm volatile ( "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     : "=r" (dst_ptr)
			     : "0" (dst_ptr)
			     : "r2", "r3", "r4", "r5"
			     );
	    }	    
#else
	    { 
	      register U32 *dst_ptr = (U32 *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];

	      __asm {
		mov r2, #0;
		mov r3, #0;
		mov r4, #0;
		mov r5, #0;
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
	      }
	    }
#endif
#endif
	  }

	  VVC1_cs[VVC1_buf_num]->AC_PRED_DIR = 0;
	  VVC1_cs[VVC1_buf_num]->TR_COMPLEXITY = 0;
	  VVC1_cs[VVC1_buf_num]->OVL_BORDERS = (VVC1_decoder.overlappedtransform && (VVC1_decoder.pquant >= 9)) << 31;
	  VVC1_cs[VVC1_buf_num]->LF_FLAGS_TOP = VVC1_decoder.loopfilter << 31;
	  VVC1_cs[VVC1_buf_num]->LF_FLAGS_LEFT = VVC1_decoder.loopfilter << 31;
	  VVC1_cs[VVC1_buf_num]->POS = 0;

	  VVC1_predict_current.quant = 0;
	}

	if (VVC1_decoder.maxbframes) {
	  cmb->fmv = VVC1_predict_current.fmv[0];

	  {
	    int iX, iY;		
	    iX = ((int)x<<3) + (cmb->fmv.x>>2);
	    iY = ((int)y<<3) + (cmb->fmv.y>>2);		
	    if (iX < -8) 
	      cmb->fmv.x -= (iX+8)*4;
	    else if (iX > (int)mb_width*8)
	      cmb->fmv.x -= (iX-(int)mb_width*8)*4;
	    if (iY < -8) 
	      cmb->fmv.y -= (iY+8)*4;
	    else if (iY > (int)mb_height*8)
	      cmb->fmv.y -= (iY-(int)mb_height*8)*4;
	  }
	}

      } else {

	mvunit_fwd = _4MV;

	if (!skipped) {
	  int mb_intra;
	  int coded_inter = 0;
	  int intra_count = 0;
	  int is_intra[6];
	  int is_coded[6];
	  unsigned int i;
	  int val;
	  int sign;
	      
	  {
	    unsigned code;
	    int n;

	    code = VVC1_show_bits(9);

	    n     = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code][1];
	    cbp   = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code][0];
		
	    if (n < 0){
	      VVC1_flush_bits(9);
	      code = VVC1_show_bits(-n);
		  
	      n     = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code + cbp][1];
	      cbp   = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code + cbp][0];
	    }
	    VVC1_flush_bits(n);
	  }

	  for (i=0; i<6; i++) {

	    val = ((cbp >> (5-i)) & 1);

	    if (i<4) {
	      dmv_x = 0;
	      dmv_y = 0;
	      mb_has_coeffs = 0;
	      mb_intra = 0;

	      if (val) {
		{
		  unsigned code;
		  int n;

		  code = VVC1_show_bits(9);

		  n     = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code][1];
		  index = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code][0];
		
		  if (n < 0){
		    VVC1_flush_bits(9);
		    code = VVC1_show_bits(-n);
		  
		    n     = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code + index][1];
		    index = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code + index][0];
		  }
		  VVC1_flush_bits(n);
		}

		index ++;		   
              
		if (index > 36)                                                   
		  {                                                                 
		    mb_has_coeffs = 1;                                              
		    index -= 37;
		  }
		else 
		  mb_has_coeffs = 0;

		if (!index) { 
		  dmv_x = 0;
		  dmv_y = 0; 
		} else if (index == 35) {
		  dmv_x = VVC1_get_bits(k_x - 1 + VVC1_decoder.quarterpel);
		  dmv_y = VVC1_get_bits(k_y - 1 + VVC1_decoder.quarterpel);
		} else if (index == 36) { 
		  dmv_x = 0;
		  dmv_y = 0;
		  mb_intra = 1;
		} else {
		  index1 = index % 6;
		  if (!VVC1_decoder.quarterpel && index1 == 5) 
		    val = 1;
		  else                                   
		    val = 0;
		  if(size_table[index1] - val > 0)
		    val = VVC1_get_bits(size_table[index1] - val);
		  else
		    val = 0;
		  sign = 0 - (val&1);
		  dmv_x = (sign ^ ((val>>1) + offset_table[index1])) - sign;
		      
		  index1 = index / 6;
		  if (!VVC1_decoder.quarterpel && index1 == 5)
		    val = 1;
		  else
		    val = 0;
		  if(size_table[index1] - val > 0)
		    val = VVC1_get_bits(size_table[index1] - val);
		  else
		    val = 0;
		  sign = 0 - (val&1);
		  dmv_y = (sign ^ ((val>>1) + offset_table[index1])) - sign;
		}
	      }

	      if (mb_intra) {
		VVC1_predict_current.fmv[i].x = 0;
		VVC1_predict_current.fmv[i].y = 0;
	      } else {
		VECTOR *left, *top, *diag;
		int left_intra, top_intra;
		int px, py;
		int sum;		    

		dmv_x <<= 1 - VVC1_decoder.quarterpel;
		dmv_y <<= 1 - VVC1_decoder.quarterpel;

		switch (i) {
		case 0:
		  left = &VVC1_predict_left.fmv[1];
		  left_intra = VVC1_predict_left.is_intra[1];
		  top = &VVC1_predict_mem[x].fmv[2];
		  top_intra = VVC1_predict_mem[x].is_intra[2];
		  if (x)
		    diag = &VVC1_predict_mem[x-1].fmv[3];
		  else
		    diag = &VVC1_predict_mem[x].fmv[3];
		  break;
		case 1:
		  left = &VVC1_predict_current.fmv[0];
		  left_intra = VVC1_predict_current.is_intra[0];
		  top = &VVC1_predict_mem[x].fmv[3];
		  top_intra = VVC1_predict_mem[x].is_intra[3];
		  if (x == mb_width-1)
		    diag = &VVC1_predict_mem[x].fmv[2];
		  else
		    diag = &VVC1_predict_mem[x+1].fmv[2];
		  break;
		case 2:
		  left = &VVC1_predict_left.fmv[3];
		  left_intra = VVC1_predict_left.is_intra[3];
		  top = &VVC1_predict_current.fmv[0];
		  top_intra = VVC1_predict_current.is_intra[0];
		  diag = &VVC1_predict_current.fmv[1];
		  break;
		case 3:
		  left = &VVC1_predict_current.fmv[2];
		  left_intra = VVC1_predict_current.is_intra[2];
		  top = &VVC1_predict_current.fmv[1];
		  top_intra = VVC1_predict_current.is_intra[1];
		  diag = &VVC1_predict_current.fmv[0];
		  break;
		}
		    
		if (y || i&2) {
		  if (mb_width == 1) {
		    px = top->x;
		    py = top->y;
		  } else {
		    px = diag->x;
		    if (top->x > diag->x) {
		      if (left->x > diag->x) {
			if (left->x > top->x)
			  px = top->x;
			else
			  px = left->x;
		      }
		    } else {
		      if (diag->x > left->x) {
			if (left->x > top->x)
			  px = left->x;
			else
			  px = top->x;
		      }
		    }
		    py = diag->y;
		    if (top->y > diag->y) {
		      if (left->y > diag->y) {
			if (left->y > top->y)
			  py = top->y;
			else
			  py = left->y;
		      }
		    } else {
		      if (diag->y > left->y) {
			if (left->y > top->y)
			  py = left->y;
			else
			  py = top->y;
		      }
		    }
		  }
		} else if (x || i&1) {
		  px = left->x;
		  py = left->y;
		} else {
		  px = 0;
		  py = 0;
		}

		{
		  int qx, qy, X, Y;

		  qx = (x<<6) + ((i&1) ? 32 : 0);
		  qy = (y<<6) + ((i&2) ? 32 : 0);
		  X = (mb_width<<6) - 4;
		  Y = (mb_height<<6) - 4;

		  if (qx+px < -28) 
		    px = -28 - qx;
		  if (qy+py < -28) 
		    py = -28 - qy;
		  if (qx+px > X) 
		    px = X - qx;
		  if (qy+py > Y) 
		    py = Y - qy;
		}
		    
		if ((y || i&2) && (x || i&1)) {
		  if (top_intra) {
		    sum = ((px < 0) ? -px : px) + ((py < 0) ? -py : py);
		  } else {
		    sum = (((px - top->x) < 0) ? -(px - top->x) : (px - top->x)) + 
		      (((py - top->y) < 0) ? -(py - top->y) : (py - top->y));
		  }

		  if (sum > 32) {
		    if (VVC1_get_bits(1)) {
		      px = top->x;
		      py = top->y;
		    } else {
		      px = left->x;
		      py = left->y;
		    }
		  } else {
		    if (left_intra) {
		      sum = ((px < 0) ? -px : px) + ((py < 0) ? -py : py);
		    } else {
		      sum = (((px - left->x) < 0) ? -(px - left->x) : (px - left->x)) + 
			(((py - left->y) < 0) ? -(py - left->y) : (py - left->y));
		    }

		    if (sum > 32) {
		      if (VVC1_get_bits(1)) {
			px = top->x;
			py = top->y;
		      } else {
			px = left->x;
			py = left->y;
		      }
		    }
		  }
		}

		VVC1_predict_current.fmv[i].x = ((px + dmv_x + r_x) & ((r_x << 1) - 1)) - r_x;
		VVC1_predict_current.fmv[i].y = ((py + dmv_y + r_y) & ((r_y << 1) - 1)) - r_y;
	      }

	      intra_count += mb_intra;
	      is_intra[i] = mb_intra;
	      is_coded[i] = mb_has_coeffs;
	      VVC1_predict_current.is_intra[i] =  is_intra[i];
	      VVC1_predict_current.coded[i] =  mb_has_coeffs;
	    }

	    if (i & 4) {
	      is_intra[i] = (intra_count >= 3);
	      is_coded[i] = val;
	      VVC1_predict_current.is_intra[i] =  is_intra[i];
	    }

	    blk_type |= !is_intra[i] << i;

	    if (!coded_inter) {
	      coded_inter = !is_intra[i] & is_coded[i];
	    }

	  }


	  if (intra_count || coded_inter) {

	    if (VVC1_decoder.dquantframe) {
	      if (VVC1_decoder.quantmode == vc1_QuantModeMBDual) { 
		mquant = (VVC1_get_bits(1)) ? VVC1_decoder.altpquant : VVC1_decoder.pquant;
	      } else if (VVC1_decoder.quantmode == vc1_QuantModeMBAny) {
		int mqdiff = VVC1_get_bits(3);
		if (mqdiff != 7) 
		  mquant = VVC1_decoder.pquant + mqdiff;
		else 
		  mquant = VVC1_get_bits(5);
	      } else {
		if((VVC1_decoder.quantmode&1) && !x)
		  mquant = VVC1_decoder.altpquant;
		if((VVC1_decoder.quantmode&2) && !y)
		  mquant = VVC1_decoder.altpquant;
		if((VVC1_decoder.quantmode&4) && x == (mb_width - 1))
		  mquant = VVC1_decoder.altpquant;
		if((VVC1_decoder.quantmode&8) && y == (mb_height - 1))
		  mquant = VVC1_decoder.altpquant;
	      }
	    }

	    VVC1_predict_current.quant = mquant;
	      
	    { 
	      int intrapred = 0;
	      for (i=0; i<6; i++) {
		if (is_intra[i]) {
		  switch(i) {
		  case 0:
		    if ((y && VVC1_predict_mem[x].is_intra[2]) || 
			(x && VVC1_predict_left.is_intra[1]))
		      intrapred = 1;
		    break;
		  case 1:
		    if ((y && VVC1_predict_mem[x].is_intra[3]) || 
			VVC1_predict_current.is_intra[0])
		      intrapred = 1;
		    break;
		  case 2:
		    if (VVC1_predict_current.is_intra[0] || 
			(x && VVC1_predict_left.is_intra[3]))
		      intrapred = 1;
		    break;
		  case 3:
		    if (VVC1_predict_current.is_intra[1] || 
			VVC1_predict_current.is_intra[2])
		      intrapred = 1;
		    break;
		  case 4:
		    if ((y && VVC1_predict_mem[x].is_intra[4]) || 
			(x && VVC1_predict_left.is_intra[4]))
		      intrapred = 1;
		    break;
		  case 5:
		    if ((y && VVC1_predict_mem[x].is_intra[5]) || 
			(x && VVC1_predict_left.is_intra[5]))
		      intrapred = 1;
		    break;
		  }		      
		}
	      }

	      if (intrapred)
		acpred_flag = VVC1_get_bits(1);
	      else
		acpred_flag = 0;
	    }
	      

	    if (!VVC1_decoder.mbtransformflag && coded_inter)
	      {
		unsigned code;
		int n;

		code = VVC1_show_bits(9);

		n     = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code][1];
		ttmb  = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code][0];
		
		if (n < 0){
		  VVC1_flush_bits(9);
		  code = VVC1_show_bits(-n);
		  
		  n     = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code + ttmb][1];
		  ttmb  = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code + ttmb][0];
		}
		VVC1_flush_bits(n);
	      }

	    VVC1_decode_mbinter_intra(x, y, 
				      ((is_coded[0]<<5) | (is_coded[1]<<4) | (is_coded[2]<<3) |  
				       (is_coded[3]<<2) | (is_coded[4]<<1) | (is_coded[5])),
				      is_intra, mquant, acpred_flag, ttmb);
	      
	  } else {
		
	    VVC1_cs[VVC1_buf_num]->AC_PRED_DIR = 0;
	    VVC1_cs[VVC1_buf_num]->TR_COMPLEXITY = 0;
	    VVC1_cs[VVC1_buf_num]->OVL_BORDERS = (VVC1_decoder.overlappedtransform && (VVC1_decoder.pquant >= 9)) << 31;
	    VVC1_cs[VVC1_buf_num]->LF_FLAGS_TOP = VVC1_decoder.loopfilter << 31;
	    VVC1_cs[VVC1_buf_num]->LF_FLAGS_LEFT = VVC1_decoder.loopfilter << 31;
	    VVC1_cs[VVC1_buf_num]->POS = 0;

#ifdef _INDIRECT
#ifdef _GNUCC
	    for (i=0; i<6; i++) {
	      { 
		register U32 *dst_ptr = (U32 *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];

		asm volatile ( "mov r2, #0\n\t"
			       "mov r3, #0\n\t"
			       "mov r4, #0\n\t"
			       "mov r5, #0\n\t"
			       :
			       :
			       );
		asm volatile ( "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       : "=r" (dst_ptr)
			       : "0" (dst_ptr)
			       : "r2", "r3", "r4", "r5"
			       );
	      }
	    }
#else
	    for (i=0; i<6; i++) {
	      { 
		register U32 *dst_ptr = (U32 *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];
		    
		__asm {
		  mov r2, #0;
		  mov r3, #0;
		  mov r4, #0;
		  mov r5, #0;
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		}
	      }
	    }
#endif
#endif
	  }

	} else {
	  unsigned int i;

	  for (i=0; i<6; i++) {
	    VVC1_predict_current.is_intra[i] = 0;
	  }

	  blk_type = 0x3f;

	  VVC1_predict_current.quant = 0;	      

	  for (i=0; i<4; i++) {
	    VECTOR *left, *top, *diag;
	    int left_intra, top_intra;
	    int px, py;
	    int sum;		    

	    dmv_x = 0;
	    dmv_y = 0;

	    switch (i) {
	    case 0:
	      left = &VVC1_predict_left.fmv[1];
	      left_intra = VVC1_predict_left.is_intra[1];
	      top = &VVC1_predict_mem[x].fmv[2];
	      top_intra = VVC1_predict_mem[x].is_intra[2];
	      if (x)
		diag = &VVC1_predict_mem[x-1].fmv[3];
	      else
		diag = &VVC1_predict_mem[x].fmv[3];
	      break;
	    case 1:
	      left = &VVC1_predict_current.fmv[0];
	      left_intra = VVC1_predict_current.is_intra[0];
	      top = &VVC1_predict_mem[x].fmv[3];
	      top_intra = VVC1_predict_mem[x].is_intra[3];
	      if (x == mb_width-1)
		diag = &VVC1_predict_mem[x].fmv[2];
	      else
		diag = &VVC1_predict_mem[x+1].fmv[2];
	      break;
	    case 2:
	      left = &VVC1_predict_left.fmv[3];
	      left_intra = VVC1_predict_left.is_intra[3];
	      top = &VVC1_predict_current.fmv[0];
	      top_intra = VVC1_predict_current.is_intra[0];
	      diag = &VVC1_predict_current.fmv[1];
	      break;
	    case 3:
	      left = &VVC1_predict_current.fmv[2];
	      left_intra = VVC1_predict_current.is_intra[2];
	      top = &VVC1_predict_current.fmv[1];
	      top_intra = VVC1_predict_current.is_intra[1];
	      diag = &VVC1_predict_current.fmv[0];
	      break;
	    }
		    
	    if (y || i&2) {
	      if (mb_width == 1) {
		px = top->x;
		py = top->y;
	      } else {
		px = diag->x;
		if (top->x > diag->x) {
		  if (left->x > diag->x) {
		    if (left->x > top->x)
		      px = top->x;
		    else
		      px = left->x;
		  }
		} else {
		  if (diag->x > left->x) {
		    if (left->x > top->x)
		      px = left->x;
		    else
		      px = top->x;
		  }
		}
		py = diag->y;
		if (top->y > diag->y) {
		  if (left->y > diag->y) {
		    if (left->y > top->y)
		      py = top->y;
		    else
		      py = left->y;
		  }
		} else {
		  if (diag->y > left->y) {
		    if (left->y > top->y)
		      py = left->y;
		    else
		      py = top->y;
		  }
		}
	      }
	    } else if (x || i&1) {
	      px = left->x;
	      py = left->y;
	    } else {
	      px = 0;
	      py = 0;
	    }

	    {
	      int qx, qy, X, Y;

	      qx = (x<<6) + ((i&1) ? 32 : 0);
	      qy = (y<<6) + ((i&2) ? 32 : 0);
	      X = (mb_width<<6) - 4;
	      Y = (mb_height<<6) - 4;
	      if (qx+px < -28) 
		px = -28 - qx;
	      if (qy+py < -28) 
		py = -28 - qy;
	      if (qx+px > X) 
		px = X - qx;
	      if (qy+py > Y) 
		py = Y - qy;
	    }
		    
	    if ((y || i&2) && (x || i&1)) {
	      if (top_intra) {
		sum = ((px < 0) ? -px : px) + ((py < 0) ? -py : py);
	      } else {
		sum = (((px - top->x) < 0) ? -(px - top->x) : (px - top->x)) + 
		  (((py - top->y) < 0) ? -(py - top->y) : (py - top->y));
	      }
	      if (sum > 32) {
		if (VVC1_get_bits(1)) {
		  px = top->x;
		  py = top->y;
		} else {
		  px = left->x;
		  py = left->y;
		}
	      } else {
		if (left_intra) {
		  sum = ((px < 0) ? -px : px) + ((py < 0) ? -py : py);
		} else {
		  sum = (((px - left->x) < 0) ? -(px - left->x) : (px - left->x)) + 
		    (((py - left->y) < 0) ? -(py - left->y) : (py - left->y));
		}
		if (sum > 32) {
		  if (VVC1_get_bits(1)) {
		    px = top->x;
		    py = top->y;
		  } else {
		    px = left->x;
		    py = left->y;
		  }
		}
	      }
	    }

	    VVC1_predict_current.fmv[i].x = ((px + dmv_x + r_x) & ((r_x << 1) - 1)) - r_x;
	    VVC1_predict_current.fmv[i].y = ((py + dmv_y + r_y) & ((r_y << 1) - 1)) - r_y;
	    VVC1_predict_current.coded[i] = 0;
	  } 

	  VVC1_cs[VVC1_buf_num]->AC_PRED_DIR = 0;
	  VVC1_cs[VVC1_buf_num]->TR_COMPLEXITY = 0;
	  VVC1_cs[VVC1_buf_num]->OVL_BORDERS = (VVC1_decoder.overlappedtransform && (VVC1_decoder.pquant >= 9)) << 31;
	  VVC1_cs[VVC1_buf_num]->LF_FLAGS_TOP = VVC1_decoder.loopfilter << 31;
	  VVC1_cs[VVC1_buf_num]->LF_FLAGS_LEFT = VVC1_decoder.loopfilter << 31;
	  VVC1_cs[VVC1_buf_num]->POS = 0;

#ifdef _INDIRECT
#ifdef _GNUCC
	  for (i=0; i<6; i++) {
	    { 
	      register U32 *dst_ptr = (U32 *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];
	      
	      asm volatile ( "mov r2, #0\n\t"
			     "mov r3, #0\n\t"
			     "mov r4, #0\n\t"
			     "mov r5, #0\n\t"
			     :
			     :
			     );
	      asm volatile ( "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     "stmia %0!, {r2-r5}\n\t"
			     : "=r" (dst_ptr)
			     : "0" (dst_ptr)
			     : "r2", "r3", "r4", "r5"
			     );
	    }
	  }
#else
	  for (i=0; i<6; i++) {
	    { 
	      register U32 *dst_ptr = (U32 *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];
		    
	      __asm {
		mov r2, #0;
		mov r3, #0;
		mov r4, #0;
		mov r5, #0;
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
		stmia dst_ptr!, {r2-r5}
	      }
	    }
	  }
#endif
#endif
	}

	if (VVC1_decoder.maxbframes) {
	  int idx;
	  static const int count[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
	      
	  idx = (VVC1_predict_current.is_intra[3] << 3) | (VVC1_predict_current.is_intra[2] << 2) | 
	    (VVC1_predict_current.is_intra[1] << 1) | VVC1_predict_current.is_intra[0];
	  if(!idx) {
	    cmb->fmv.x = median4(VVC1_predict_current.fmv[0].x, VVC1_predict_current.fmv[1].x, 
				 VVC1_predict_current.fmv[2].x, VVC1_predict_current.fmv[3].x);
	    cmb->fmv.y = median4(VVC1_predict_current.fmv[0].y, VVC1_predict_current.fmv[1].y, 
				 VVC1_predict_current.fmv[2].y, VVC1_predict_current.fmv[3].y);
	  } else if(count[idx] == 1) {
	    switch(idx) {
	    case 0x1:
	      cmb->fmv.x = median3(VVC1_predict_current.fmv[1].x, VVC1_predict_current.fmv[2].x, VVC1_predict_current.fmv[3].x);
	      cmb->fmv.y = median3(VVC1_predict_current.fmv[1].y, VVC1_predict_current.fmv[2].y, VVC1_predict_current.fmv[3].y);
	      break;
	    case 0x2:
	      cmb->fmv.x = median3(VVC1_predict_current.fmv[0].x, VVC1_predict_current.fmv[2].x, VVC1_predict_current.fmv[3].x);
	      cmb->fmv.y = median3(VVC1_predict_current.fmv[0].y, VVC1_predict_current.fmv[2].y, VVC1_predict_current.fmv[3].y);
	      break;
	    case 0x4:
	      cmb->fmv.x = median3(VVC1_predict_current.fmv[0].x, VVC1_predict_current.fmv[1].x, VVC1_predict_current.fmv[3].x);
	      cmb->fmv.y = median3(VVC1_predict_current.fmv[0].y, VVC1_predict_current.fmv[1].y, VVC1_predict_current.fmv[3].y);
	      break;
	    case 0x8:
	      cmb->fmv.x = median3(VVC1_predict_current.fmv[0].x, VVC1_predict_current.fmv[1].x, VVC1_predict_current.fmv[2].x);
	      cmb->fmv.y = median3(VVC1_predict_current.fmv[0].y, VVC1_predict_current.fmv[1].y, VVC1_predict_current.fmv[2].y);
	      break;
	    }	       
	  } else if(count[idx] == 2) {
	    int i;
	    int t1 = 0, t2 = 0;
	    for(i=0; i<3;i++) if(!VVC1_predict_current.is_intra[i]) {t1 = i; break;}
	    for(i= t1+1; i<4; i++)if(!VVC1_predict_current.is_intra[i]) {t2 = i; break;}
	    cmb->fmv.x = (VVC1_predict_current.fmv[t1].x + VVC1_predict_current.fmv[t2].x) / 2;;
	    cmb->fmv.y = (VVC1_predict_current.fmv[t1].y + VVC1_predict_current.fmv[t2].y) / 2;;
	  } else {
	    cmb->fmv.x = 0;
	    cmb->fmv.y = 0;
	  }

	  {
	    int iX, iY;
	    iX = ((int)x<<3) + (cmb->fmv.x>>2);
	    iY = ((int)y<<3) + (cmb->fmv.y>>2);
	    if (iX < -8) 
	      cmb->fmv.x -= (iX+8)*4;
	    else if (iX > (int)mb_width*8)
	      cmb->fmv.x -= (iX-(int)mb_width*8)*4;
	    if (iY < -8) 
	      cmb->fmv.y -= (iY+8)*4;
	    else if (iY > (int)mb_height*8)
	      cmb->fmv.y -= (iY-(int)mb_height*8)*4;
	  }
	}

      }

      if(x) {
	int iik ;
	VVC1_predict_mem[x-1].quant = VVC1_predict_left.quant ;
	for (iik=0;iik<6;iik++) {
	  VVC1_predict_mem[x-1].dc_value[iik] = VVC1_predict_left.dc_value[iik] ;
	  VVC1_predict_mem[x-1].is_intra[iik] = VVC1_predict_left.is_intra[iik] ;
	}
	for (iik=0;iik<4;iik++) {
	  VVC1_predict_mem[x-1].coded[iik] = VVC1_predict_left.coded[iik] ;
	  VVC1_predict_mem[x-1].fmv[iik] = VVC1_predict_left.fmv[iik] ;
	}
      }
      {  
	int iik ;
	VVC1_predict_left.quant = VVC1_predict_current.quant ;
	for (iik=0;iik<6;iik++) {
	  VVC1_predict_left.dc_value[iik] = VVC1_predict_current.dc_value[iik] ;
	  VVC1_predict_left.is_intra[iik] = VVC1_predict_current.is_intra[iik] ;
	}
	for (iik=0;iik<4;iik++) {
	  VVC1_predict_left.coded[iik] = VVC1_predict_current.coded[iik] ;
	  VVC1_predict_left.fmv[iik] = VVC1_predict_current.fmv[iik] ;
	}
      }
      if(x == mb_width-1) {
	int iik ;
	VVC1_predict_mem[x].quant = VVC1_predict_current.quant ;
	for (iik=0;iik<6;iik++) {
	  VVC1_predict_mem[x].dc_value[iik] = VVC1_predict_current.dc_value[iik] ;
	  VVC1_predict_mem[x].is_intra[iik] = VVC1_predict_current.is_intra[iik] ; 
	}
	for (iik=0;iik<4;iik++) {
	  VVC1_predict_mem[x].coded[iik] = VVC1_predict_current.coded[iik] ; 
	  VVC1_predict_mem[x].fmv[iik] = VVC1_predict_current.fmv[iik] ;
	}
      } 
      
      VVC1_cs[VVC1_buf_num]->MB_y = y;
      VVC1_cs[VVC1_buf_num]->MB_x = x;

      VVC1_cs[VVC1_buf_num]->MVx[0].FWD = VVC1_predict_current.fmv[0].x;
      VVC1_cs[VVC1_buf_num]->MVx[1].FWD = VVC1_predict_current.fmv[1].x;
      VVC1_cs[VVC1_buf_num]->MVx[2].FWD = VVC1_predict_current.fmv[2].x;
      VVC1_cs[VVC1_buf_num]->MVx[3].FWD = VVC1_predict_current.fmv[3].x;
      VVC1_cs[VVC1_buf_num]->MVy[0].FWD = VVC1_predict_current.fmv[0].y;
      VVC1_cs[VVC1_buf_num]->MVy[1].FWD = VVC1_predict_current.fmv[1].y;
      VVC1_cs[VVC1_buf_num]->MVy[2].FWD = VVC1_predict_current.fmv[2].y;
      VVC1_cs[VVC1_buf_num]->MVy[3].FWD = VVC1_predict_current.fmv[3].y;

      if (mvunit_fwd == _1MV) {
	int umv = 0;

	if (((((int)x)<<(5+VVC1_decoder.quarterpel)) + VVC1_predict_current.fmv[0].x) <= 3) {
	  umv = 1;
	}
	if (((((int)y)<<(5+VVC1_decoder.quarterpel)) + VVC1_predict_current.fmv[0].y) <= 3) {
	  umv = 1;
	}
	if (((((int)x)<<(5+VVC1_decoder.quarterpel)) + VVC1_predict_current.fmv[0].x) >= (VVC1_decoder.width-18)<<(1+VVC1_decoder.quarterpel)) {
	  umv = 1;
	}
	if (((((int)y)<<(5+VVC1_decoder.quarterpel)) + VVC1_predict_current.fmv[0].y) >= (VVC1_decoder.height-18)<<(1+VVC1_decoder.quarterpel)) {
	  umv = 1;
	}

	if (umv)
	  mvunit_fwd = _4MV;
      }

      VVC1_cs[VVC1_buf_num]->MVunit.FWD = mvunit_fwd;
      VVC1_cs[VVC1_buf_num]->MVunit.BWD = NoMV;
	  
      VVC1_mp_sync();

      VVC1_icm_num = (VVC1_icm_num + 1) % ICM_BUFFERS_USED;

      VVC1_cs[VVC1_buf_num]->MB_TYPE = mb_type | (blk_type << 4) | ((unsigned)(VVC1_icm_num*0x80)<<16);
      VVC1_cs[VVC1_buf_num]->SP_INFO = VVC1_decoder.sp_info;

#ifdef _INDIRECT
      VVC1_buf_num = (VVC1_buf_num + 1);
#else
      VVC1_buf_num = (VVC1_buf_num + 1) % ICM_BUFFERS_USED;
#endif

      VVC1_decoder.sp_info &= (unsigned)-3;

#ifdef _INDIRECT
      if (VVC1_buf_num == VVC1_decoder.groupHandle.buffergroup_size) {
	int rc;

	VVC1_buf_num = 0;
	
	rc = vid_send_buffers(&VVC1_decoder.groupHandle) ;
	if (VMP_E == rc) {
	  return VVC1_VID_STATUS_ERROR;
	}

	rc = vid_get_buffers(CORE_USED, &VVC1_decoder.groupHandle, 1); // blocking 
	if (rc != VMP_OK) {
	  return VVC1_VID_STATUS_ERROR_MP_TIMEOUT;
	} else {
	  VVC1_cs[0] = (CTRL_STRUCT  *)VVC1_decoder.groupHandle.address.virt_addr;
	  {
	    int i;
	    for (i = 1; i < VVC1_decoder.groupHandle.buffergroup_size; i++) {
	      VVC1_cs[i] = VVC1_cs[0] + i;
	    }
	  }
	}
      }
#endif
    }
  }

  return VVC1_VID_STATUS_OK;
}






int VVC1_decode_bframe()
{	
  unsigned int x, y;
  unsigned int mb_width = VVC1_decoder.mb_width;
  unsigned int mb_height = VVC1_decoder.mb_height;
  VECTOR fmv;
  VECTOR bmv;
  mb_t mb_type;
  mvunit_t mvunit_fwd, mvunit_bwd;
  int blk_type;
  int dmv_x[2], dmv_y[2];
  int direct;
  int skipped;
  int mquant;
  int bmv_type;

  int mb_intra;
  int is_intra[6];

  int index, index1;

  int k_x, k_y;
  int r_x, r_y;

  static const int size_table[6] = { 0, 2, 3, 4, 5, 8 };
  static const int offset_table[6] = { 0, 1, 3, 7, 15, 31 };
  int mb_has_coeffs;

  VVC1_cs[VVC1_buf_num]->MC_FLAGS = (VVC1_decoder.luminancescale << 24) | (VVC1_decoder.luminanceshift << 16) | 
    (VVC1_decoder.intensitycompflag << 5) | ((VVC1_decoder.rangeYscale != 8) << 4) | 
    (VVC1_decoder.fastumvc << 3) | ((VVC1_decoder.rndctrl & 0x1) << 2) | 
    (VVC1_decoder.quarterpel << 1) | (VVC1_decoder.mvmode == vc1_MVMode1MVHalfPelBilinear);

  switch(VVC1_decoder.frameACcodingindex){
  case 0:
    VVC1_decoder.codingset = (VVC1_decoder.pqindex <= 8) ? CS_HIGH_RATE_INTRA : CS_LOW_MOT_INTRA;
    break;
  case 1:
    VVC1_decoder.codingset = CS_HIGH_MOT_INTRA;
    break;
  case 2:
    VVC1_decoder.codingset = CS_MID_RATE_INTRA;
    break;
  }

  switch(VVC1_decoder.frameACcodingindex){
  case 0:
    VVC1_decoder.codingset2 = (VVC1_decoder.pqindex <= 8) ? CS_HIGH_RATE_INTER : CS_LOW_MOT_INTER;
    break;
  case 1:
    VVC1_decoder.codingset2 = CS_HIGH_MOT_INTER;
    break;
  case 2:
    VVC1_decoder.codingset2 = CS_MID_RATE_INTER;
    break;
  }

  VVC1_decoder.esc3_level_length = 0;

  k_x = VVC1_decoder.mvrange + 9 + (VVC1_decoder.mvrange >> 1);
  k_y = VVC1_decoder.mvrange + 8;
  r_x = 1 << (k_x - 1);
  r_y = 1 << (k_y - 1);

  {
    int i, j;
    for (j=0; j<MAX_PRED_DC_MBS; j++) {
      for (i=0; i<6; i++) {
	VVC1_predict_mem[j].is_intra[i] = 0;
      }
      for (i=0; i<4; i++) {
	VVC1_predict_mem[j].coded[i] = 0;
      }
    }
    VVC1_predict_mem[j].fmv[0].x = 0;
    VVC1_predict_mem[j].fmv[0].y = 0;
    VVC1_predict_mem[j].bmv.x = 0;
    VVC1_predict_mem[j].bmv.y = 0;
  }

  for (y = 0; y < mb_height; y++) {

    {
      int i;
      for (i=0; i<6; i++) {
	VVC1_predict_left.is_intra[i] = 0;
      }
      for (i=0; i<4; i++) {
	VVC1_predict_left.coded[i] = 0;
      }
      VVC1_predict_left.fmv[0].x = 0;
      VVC1_predict_left.fmv[0].y = 0;
      VVC1_predict_left.bmv.x = 0;
      VVC1_predict_left.bmv.y = 0;      
    }

    for (x = 0; x < mb_width; x++) {
      MB_CONTEXT *cmb;
      int acpred_flag;
      int cbp;
      int ttmb;
      unsigned int i;

      bmv_type = MODE_BACKWARD;
      mb_has_coeffs = 0;
      mb_intra = 0;
      cbp = 0;
      acpred_flag = 0;
      ttmb = VVC1_decoder.frametransformtype; 

      cmb = &VVC1_colocated_mb[y * mb_width + x];

      mquant = VVC1_decoder.pquant;

      VVC1_predict_current.quant = 0;

      if (VVC1_decoder.bframedirectmode_rawmode)
	direct = VVC1_get_bits(1);
      else
	direct = VVC1_bframedirectmode[y*mb_width + x];
	  
      if (VVC1_decoder.skipmb_rawmode)
	skipped = VVC1_get_bits(1);
      else
	skipped = VVC1_skipmb[y*mb_width + x];

      dmv_x[0] = dmv_y[0] = dmv_x[1] = dmv_y[1] = 0;

      if (!direct) {

	if (!skipped) {
	  int val;
	  int sign;

	  {
	    unsigned code;
	    int n;

	    code = VVC1_show_bits(9);

	    n     = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code][1];
	    index = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code][0];
		
	    if (n < 0){
	      VVC1_flush_bits(9);
	      code = VVC1_show_bits(-n);
		  
	      n     = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code + index][1];
	      index = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code + index][0];
	    }

	    VVC1_flush_bits(n);
	  }

	  index ++;		   
             
	  if (index > 36)                                                   
	    {                                                                 
	      mb_has_coeffs = 1;                                              
	      index -= 37;
	    }
	  else 
	    mb_has_coeffs = 0;

	  mb_intra = 0;

	  if (!index) { 
	    dmv_x[0] = 0;
	    dmv_y[0] = 0; 
	  } else if (index == 35) {
	    dmv_x[0] = VVC1_get_bits(k_x - 1 + VVC1_decoder.quarterpel);
	    dmv_y[0] = VVC1_get_bits(k_y - 1 + VVC1_decoder.quarterpel);
	  } else if (index == 36) { 
	    dmv_x[0] = 0;
	    dmv_y[0] = 0;
	    mb_intra = 1;                                                
	  } else {
	    index1 = index % 6;
	    if (!VVC1_decoder.quarterpel && index1 == 5) 
	      val = 1;
	    else                                   
	      val = 0;
	    if(size_table[index1] - val > 0)
	      val = VVC1_get_bits(size_table[index1] - val);
	    else
	      val = 0;
	    sign = 0 - (val&1);
	    dmv_x[0] = (sign ^ ((val>>1) + offset_table[index1])) - sign;
		      
	    index1 = index / 6;
	    if (!VVC1_decoder.quarterpel && index1 == 5)
	      val = 1;
	    else
	      val = 0;
	    if(size_table[index1] - val > 0)
	      val = VVC1_get_bits(size_table[index1] - val);
	    else
	      val = 0;
	    sign = 0 - (val&1);
	    dmv_y[0] = (sign ^ ((val>>1) + offset_table[index1])) - sign;
	  }

	  dmv_x[1] = dmv_x[0];
	  dmv_y[1] = dmv_y[0];
	}

	if (skipped || !mb_intra) {
	      
	  if (!VVC1_get_bits(1)) {
	    bmv_type = (VVC1_decoder.bfraction >= (B_FRACTION_DEN/2)) ? MODE_BACKWARD : MODE_FORWARD;
	  } else {
	    if (!VVC1_get_bits(1)) {
	      bmv_type = (VVC1_decoder.bfraction >= (B_FRACTION_DEN/2)) ? MODE_FORWARD : MODE_BACKWARD;
	    } else {
	      bmv_type = MODE_INTERPOLATE;
	      dmv_x[0] = dmv_y[0] = 0;
	    }
	  }
	}
      }

      if (skipped) {
	if (direct)
	  bmv_type = MODE_INTERPOLATE;
	    
	for (i=0; i<6; i++) {
	  VVC1_predict_current.is_intra[i] = 0;
	}

	VVC1_cs[VVC1_buf_num]->AC_PRED_DIR = 0;
	VVC1_cs[VVC1_buf_num]->TR_COMPLEXITY = 0;
	VVC1_cs[VVC1_buf_num]->OVL_BORDERS = 0;
	VVC1_cs[VVC1_buf_num]->LF_FLAGS_TOP = VVC1_decoder.loopfilter << 31;
	VVC1_cs[VVC1_buf_num]->LF_FLAGS_LEFT = VVC1_decoder.loopfilter << 31;
	VVC1_cs[VVC1_buf_num]->POS = 0;

#ifdef _INDIRECT
#ifdef _GNUCC
	for (i=0; i<6; i++) {
	  { 
	    register U32 *dst_ptr = (U32 *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];
	    
	    asm volatile ( "mov r2, #0\n\t"
			   "mov r3, #0\n\t"
			   "mov r4, #0\n\t"
			   "mov r5, #0\n\t"
			   :
			   :
			   );
	    asm volatile ( "stmia %0!, {r2-r5}\n\t"
			   "stmia %0!, {r2-r5}\n\t"
			   "stmia %0!, {r2-r5}\n\t"
			   "stmia %0!, {r2-r5}\n\t"
			   "stmia %0!, {r2-r5}\n\t"
			   "stmia %0!, {r2-r5}\n\t"
			   "stmia %0!, {r2-r5}\n\t"
			   "stmia %0!, {r2-r5}\n\t"
			   : "=r" (dst_ptr)
			   : "0" (dst_ptr)
			   : "r2", "r3", "r4", "r5"
			   );
	  }
	}
#else
	for (i=0; i<6; i++) {
	  { 
	    register U32 *dst_ptr = (U32 *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];
		    
	    __asm {
	      mov r2, #0;
	      mov r3, #0;
	      mov r4, #0;
	      mov r5, #0;
	      stmia dst_ptr!, {r2-r5}
	      stmia dst_ptr!, {r2-r5}
	      stmia dst_ptr!, {r2-r5}
	      stmia dst_ptr!, {r2-r5}
	      stmia dst_ptr!, {r2-r5}
	      stmia dst_ptr!, {r2-r5}
	      stmia dst_ptr!, {r2-r5}
	      stmia dst_ptr!, {r2-r5}
	    }
	  }
	}
#endif
#endif
	    
      } else {
	    
	if (direct) {
	  bmv_type = MODE_DIRECT;
	      
	  {
	    unsigned code;
	    int n;

	    code = VVC1_show_bits(9);
	    n     = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code][1];
	    cbp   = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code][0];
	    if (n < 0){
	      VVC1_flush_bits(9);
	      code = VVC1_show_bits(-n);
	      n     = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code + cbp][1];
	      cbp   = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code + cbp][0];
	    }
	    VVC1_flush_bits(n);
	  }

	  if (VVC1_decoder.dquantframe) {
	    if (VVC1_decoder.quantmode == vc1_QuantModeMBDual) { 
	      mquant = (VVC1_get_bits(1)) ? VVC1_decoder.altpquant : VVC1_decoder.pquant;
	    } else if (VVC1_decoder.quantmode == vc1_QuantModeMBAny) {
	      int mqdiff = VVC1_get_bits(3);
	      if (mqdiff != 7) 
		mquant = VVC1_decoder.pquant + mqdiff;
	      else 
		mquant = VVC1_get_bits(5);
	    } else {
	      if((VVC1_decoder.quantmode&1) && !x)
		mquant = VVC1_decoder.altpquant;
	      if((VVC1_decoder.quantmode&2) && !y)
		mquant = VVC1_decoder.altpquant;
	      if((VVC1_decoder.quantmode&4) && x == (mb_width - 1))
		mquant = VVC1_decoder.altpquant;
	      if((VVC1_decoder.quantmode&8) && y == (mb_height - 1))
		mquant = VVC1_decoder.altpquant;
	    }
	  }

	  mb_intra = 0;
	  mb_has_coeffs = 0;

	  VVC1_predict_current.quant = mquant;
	      
	  if (!VVC1_decoder.mbtransformflag) {
	    {
	      unsigned code;
	      int n;

	      code = VVC1_show_bits(9);

	      n     = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code][1];
	      ttmb  = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code][0];
		
	      if (n < 0){
		VVC1_flush_bits(9);
		code = VVC1_show_bits(-n);
		  
		n     = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code + ttmb][1];
		ttmb  = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code + ttmb][0];
	      }
	      VVC1_flush_bits(n);
	    }
	  }

	  dmv_x[0] = dmv_y[0] = dmv_x[1] = dmv_y[1] = 0;

	  for (i=0; i<6; i++) {
	    is_intra[i] = mb_intra;
	    VVC1_predict_current.is_intra[i] = mb_intra;
	  }

	  VVC1_decode_mbinter_intra(x, y, cbp, is_intra, mquant, acpred_flag, ttmb);	      

	} else {
	      
	  if (mb_has_coeffs) {

	    if (bmv_type == MODE_INTERPOLATE) {
	      int val;
	      int sign;

	      {
		unsigned code;
		int n;

		code = VVC1_show_bits(9);

		n     = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code][1];
		index = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code][0];
		
		if (n < 0){
		  VVC1_flush_bits(9);
		  code = VVC1_show_bits(-n);
		  
		  n     = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code + index][1];
		  index = VVC1_mv_diff_vlc[VVC1_decoder.mvtab][code + index][0];
		}

		VVC1_flush_bits(n);
	      }

	      index ++;		   
             
	      if (index > 36)                                                   
		{                                                                 
		  mb_has_coeffs = 1;                                              
		  index -= 37;
		}
	      else 
		mb_has_coeffs = 0;

	      mb_intra = 0;

	      if (!index) { 
		dmv_x[0] = 0;
		dmv_y[0] = 0; 
	      } else if (index == 35) {
		dmv_x[0] = VVC1_get_bits(k_x - 1 + VVC1_decoder.quarterpel);
		dmv_y[0] = VVC1_get_bits(k_y - 1 + VVC1_decoder.quarterpel);
	      } else if (index == 36) { 
		dmv_x[0] = 0;
		dmv_y[0] = 0;
		mb_intra = 1;                                                
	      } else {
		index1 = index % 6;
		if (!VVC1_decoder.quarterpel && index1 == 5) 
		  val = 1;
		else                                   
		  val = 0;
		if(size_table[index1] - val > 0)
		  val = VVC1_get_bits(size_table[index1] - val);
		else
		  val = 0;
		sign = 0 - (val&1);
		dmv_x[0] = (sign ^ ((val>>1) + offset_table[index1])) - sign;
		      
		index1 = index / 6;
		if (!VVC1_decoder.quarterpel && index1 == 5)
		  val = 1;
		else
		  val = 0;
		if(size_table[index1] - val > 0)
		  val = VVC1_get_bits(size_table[index1] - val);
		else
		  val = 0;
		sign = 0 - (val&1);
		dmv_y[0] = (sign ^ ((val>>1) + offset_table[index1])) - sign;
	      }
	    }

	    if (!mb_has_coeffs) {

	      VVC1_cs[VVC1_buf_num]->AC_PRED_DIR = 0;
	      VVC1_cs[VVC1_buf_num]->TR_COMPLEXITY = 0;
	      VVC1_cs[VVC1_buf_num]->OVL_BORDERS = 0;
	      VVC1_cs[VVC1_buf_num]->LF_FLAGS_TOP = VVC1_decoder.loopfilter << 31;
	      VVC1_cs[VVC1_buf_num]->LF_FLAGS_LEFT = VVC1_decoder.loopfilter << 31;
	      VVC1_cs[VVC1_buf_num]->POS = 0;

#ifdef _INDIRECT
#ifdef _GNUCC
	      for (i=0; i<6; i++) {
		{ 
		  register U32 *dst_ptr = (U32 *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];
	    
		  asm volatile ( "mov r2, #0\n\t"
				 "mov r3, #0\n\t"
				 "mov r4, #0\n\t"
				 "mov r5, #0\n\t"
				 :
				 :
				 );
		  asm volatile ( "stmia %0!, {r2-r5}\n\t"
				 "stmia %0!, {r2-r5}\n\t"
				 "stmia %0!, {r2-r5}\n\t"
				 "stmia %0!, {r2-r5}\n\t"
				 "stmia %0!, {r2-r5}\n\t"
				 "stmia %0!, {r2-r5}\n\t"
				 "stmia %0!, {r2-r5}\n\t"
				 "stmia %0!, {r2-r5}\n\t"
				 : "=r" (dst_ptr)
				 : "0" (dst_ptr)
				 : "r2", "r3", "r4", "r5"
				 );
		}
	      }
#else
	      for (i=0; i<6; i++) {
		{ 
		  register U32 *dst_ptr = (U32 *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];
		    
		  __asm {
		    mov r2, #0;
		    mov r3, #0;
		    mov r4, #0;
		    mov r5, #0;
		    stmia dst_ptr!, {r2-r5}
		    stmia dst_ptr!, {r2-r5}
		    stmia dst_ptr!, {r2-r5}
		    stmia dst_ptr!, {r2-r5}
		    stmia dst_ptr!, {r2-r5}
		    stmia dst_ptr!, {r2-r5}
		    stmia dst_ptr!, {r2-r5}
		    stmia dst_ptr!, {r2-r5}
		  }
		}
	      }
#endif
#endif

	    } else {

	      if (mb_intra) {
		acpred_flag = VVC1_get_bits(1);
	      }

	      {
		unsigned code;
		int n;

		code = VVC1_show_bits(9);
		n     = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code][1];
		cbp   = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code][0];
		if (n < 0){
		  VVC1_flush_bits(9);
		  code = VVC1_show_bits(-n);
		  n     = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code + cbp][1];
		  cbp   = VVC1_cbpcy_p_vlc[VVC1_decoder.cbptab][code + cbp][0];
		}
		VVC1_flush_bits(n);
	      }

	      if (VVC1_decoder.dquantframe) {
		if (VVC1_decoder.quantmode == vc1_QuantModeMBDual) { 
		  mquant = (VVC1_get_bits(1)) ? VVC1_decoder.altpquant : VVC1_decoder.pquant;
		} else if (VVC1_decoder.quantmode == vc1_QuantModeMBAny) {
		  int mqdiff = VVC1_get_bits(3);
		  if (mqdiff != 7) 
		    mquant = VVC1_decoder.pquant + mqdiff;
		  else 
		    mquant = VVC1_get_bits(5);
		} else {
		  if((VVC1_decoder.quantmode&1) && !x)
		    mquant = VVC1_decoder.altpquant;
		  if((VVC1_decoder.quantmode&2) && !y)
		    mquant = VVC1_decoder.altpquant;
		  if((VVC1_decoder.quantmode&4) && x == (mb_width - 1))
		    mquant = VVC1_decoder.altpquant;
		  if((VVC1_decoder.quantmode&8) && y == (mb_height - 1))
		    mquant = VVC1_decoder.altpquant;
		}
	      }

	      VVC1_predict_current.quant = mquant;

	      if (!VVC1_decoder.mbtransformflag && !mb_intra && mb_has_coeffs) {
		{
		  unsigned code;
		  int n;

		  code = VVC1_show_bits(9);

		  n     = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code][1];
		  ttmb  = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code][0];
		
		  if (n < 0){
		    VVC1_flush_bits(9);
		    code = VVC1_show_bits(-n);
		  
		    n     = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code + ttmb][1];
		    ttmb  = VVC1_ttmb_vlc[VVC1_decoder.transformtypeindex][code + ttmb][0];
		  }
		  VVC1_flush_bits(n);
		}
	      }

	      for (i=0; i<6; i++) {
		is_intra[i] = mb_intra;
		VVC1_predict_current.is_intra[i] = mb_intra;
	      }

	      VVC1_decode_mbinter_intra(x, y, cbp, is_intra, mquant, acpred_flag, ttmb);		  
	    }

	  } else if (mb_intra) {

	    if (VVC1_decoder.dquantframe) {
	      if (VVC1_decoder.quantmode == vc1_QuantModeMBDual) { 
		mquant = (VVC1_get_bits(1)) ? VVC1_decoder.altpquant : VVC1_decoder.pquant;
	      } else if (VVC1_decoder.quantmode == vc1_QuantModeMBAny) {
		int mqdiff = VVC1_get_bits(3);
		if (mqdiff != 7) 
		  mquant = VVC1_decoder.pquant + mqdiff;
		else 
		  mquant = VVC1_get_bits(5);
	      } else {
		if((VVC1_decoder.quantmode&1) && !x)
		  mquant = VVC1_decoder.altpquant;
		if((VVC1_decoder.quantmode&2) && !y)
		  mquant = VVC1_decoder.altpquant;
		if((VVC1_decoder.quantmode&4) && x == (mb_width - 1))
		  mquant = VVC1_decoder.altpquant;
		if((VVC1_decoder.quantmode&8) && y == (mb_height - 1))
		  mquant = VVC1_decoder.altpquant;
	      }
	    }
		
	    VVC1_predict_current.quant = mquant;

	    acpred_flag = VVC1_get_bits(1);

	    for (i=0; i<6; i++) {
	      is_intra[i] = mb_intra;
	      VVC1_predict_current.is_intra[i] = mb_intra;
	    }

	    VVC1_decode_mbinter_intra(x, y, 0, is_intra, mquant, acpred_flag, ttmb);

	  } else {

	    for (i=0; i<6; i++) {
	      VVC1_predict_current.is_intra[i] = 0;
	    }
	    
	    VVC1_cs[VVC1_buf_num]->AC_PRED_DIR = 0;
	    VVC1_cs[VVC1_buf_num]->TR_COMPLEXITY = 0;
	    VVC1_cs[VVC1_buf_num]->OVL_BORDERS = 0;
	    VVC1_cs[VVC1_buf_num]->LF_FLAGS_TOP = VVC1_decoder.loopfilter << 31;
	    VVC1_cs[VVC1_buf_num]->LF_FLAGS_LEFT = VVC1_decoder.loopfilter << 31;
	    VVC1_cs[VVC1_buf_num]->POS = 0;
	    
#ifdef _INDIRECT
#ifdef _GNUCC
	    for (i=0; i<6; i++) {
	      { 
		register U32 *dst_ptr = (U32 *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];
	    
		asm volatile ( "mov r2, #0\n\t"
			       "mov r3, #0\n\t"
			       "mov r4, #0\n\t"
			       "mov r5, #0\n\t"
			       :
			       :
			       );
		asm volatile ( "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       "stmia %0!, {r2-r5}\n\t"
			       : "=r" (dst_ptr)
			       : "0" (dst_ptr)
			       : "r2", "r3", "r4", "r5"
			       );
	      }
	    }
#else
	    for (i=0; i<6; i++) {
	      { 
		register U32 *dst_ptr = (U32 *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];
		    
		__asm {
		  mov r2, #0;
		  mov r3, #0;
		  mov r4, #0;
		  mov r5, #0;
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		  stmia dst_ptr!, {r2-r5}
		}
	      }
	    }
#endif
#endif
	  }
      	      
	}	    	    
      }

      if (mb_intra) {
	unsigned int i;
	mb_type = Intra;
	fmv.x = 0;
	fmv.y = 0;
	bmv.x = 0;
	bmv.y = 0;
	VVC1_predict_current.fmv[0] = fmv;
	VVC1_predict_current.bmv = bmv;

      } else {
	int fpx, fpy, bpx, bpy;

	if (VVC1_decoder.quarterpel) {
	  fpx = (cmb->fmv.x * VVC1_decoder.bfraction + 128) >> 8;
	  fpy = (cmb->fmv.y * VVC1_decoder.bfraction + 128) >> 8;
	  bpx = (cmb->fmv.x * (VVC1_decoder.bfraction - 256) + 128) >> 8;
	  bpy = (cmb->fmv.y * (VVC1_decoder.bfraction - 256) + 128) >> 8;
	} else {
	  fpx = 2 * (cmb->fmv.x * VVC1_decoder.bfraction + 255) >> 9;
	  fpy = 2 * (cmb->fmv.y * VVC1_decoder.bfraction + 255) >> 9;
	  bpx = 2 * (cmb->fmv.x * (VVC1_decoder.bfraction - 256) + 255) >> 9;
	  bpy = 2 * (cmb->fmv.y * (VVC1_decoder.bfraction - 256) + 255) >> 9;
	}
	    
	{
	  int qx, qy, X, Y;
	      
	  qx = x<<6;
	  qy = y<<6;
	  X = (mb_width<<6) - 4;
	  Y = (mb_height<<6) - 4;
	  if (qx+fpx < -60) 
	    fpx = -60 - qx;
	  if (qy+fpy < -60) 
	    fpy = -60 - qy;
	  if (qx+fpx > X) 
	    fpx = X - qx;
	  if (qy+fpy > Y) 
	    fpy = Y - qy;
	  if (qx+bpx < -60) 
	    bpx = -60 - qx;
	  if (qy+bpy < -60) 
	    bpy = -60 - qy;
	  if (qx+bpx > X) 
	    bpx = X - qx;
	  if (qy+bpy > Y) 
	    bpy = Y - qy;
	}
	    
	fmv.x = fpx;
	fmv.y = fpy;
	bmv.x = bpx;
	bmv.y = bpy;	    
	VVC1_predict_current.fmv[0] = fmv;
	VVC1_predict_current.bmv = bmv;

	if (!direct)  {
	  VECTOR *left, *top, *diag;
	  int px, py;

	  dmv_x[0] <<= 1 - VVC1_decoder.quarterpel;
	  dmv_y[0] <<= 1 - VVC1_decoder.quarterpel;
	  dmv_x[1] <<= 1 - VVC1_decoder.quarterpel;
	  dmv_y[1] <<= 1 - VVC1_decoder.quarterpel;

	  if ((bmv_type == MODE_FORWARD) || (bmv_type == MODE_INTERPOLATE)) {

	    left = &VVC1_predict_left.fmv[0];
	    top = &VVC1_predict_mem[x].fmv[0];
	    if (x == mb_width-1)
	      diag = &VVC1_predict_mem[x-1].fmv[0];
	    else
	      diag = &VVC1_predict_mem[x+1].fmv[0];
		    
	    if (y) {
	      if (mb_width == 1) {
		px = top->x;
		py = top->y;
	      } else {
		px = diag->x;
		if (top->x > diag->x) {
		  if (left->x > diag->x) {
		    if (left->x > top->x)
		      px = top->x;
		    else
		      px = left->x;
		  }
		} else {
		  if (diag->x > left->x) {
		    if (left->x > top->x)
		      px = left->x;
		    else
		      px = top->x;
		  }
		}
		py = diag->y;
		if (top->y > diag->y) {
		  if (left->y > diag->y) {
		    if (left->y > top->y)
		      py = top->y;
		    else
		      py = left->y;
		  }
		} else {
		  if (diag->y > left->y) {
		    if (left->y > top->y)
		      py = left->y;
		    else
		      py = top->y;
		  }
		}
	      }
	    } else if (x) {
	      px = left->x;
	      py = left->y;
	    } else {
	      px = 0;
	      py = 0;
	    }

	    {
	      int qx, qy, X, Y;

	      qx = x<<5;
	      qy = y<<5;
	      X = (mb_width<<5) - 4;
	      Y = (mb_height<<5) - 4;
	      if (qx+px < -28) 
		px = -28 - qx;
	      if (qy+py < -28) 
		py = -28 - qy;
	      if (qx+px > X) 
		px = X - qx;
	      if (qy+py > Y) 
		py = Y - qy;
	    }
		    
	    fmv.x = ((px + dmv_x[0] + r_x) & ((r_x << 1) - 1)) - r_x;
	    fmv.y = ((py + dmv_y[0] + r_y) & ((r_y << 1) - 1)) - r_y;

	    VVC1_predict_current.fmv[0] = fmv;
	  }

	  if ((bmv_type == MODE_BACKWARD) || (bmv_type == MODE_INTERPOLATE)) {

	    left = &VVC1_predict_left.bmv;
	    top = &VVC1_predict_mem[x].bmv;
	    if (x == mb_width-1)
	      diag = &VVC1_predict_mem[x-1].bmv;
	    else
	      diag = &VVC1_predict_mem[x+1].bmv;
		    
	    if (y) {
	      if (mb_width == 1) {
		px = top->x;
		py = top->y;
	      } else {
		px = diag->x;
		if (top->x > diag->x) {
		  if (left->x > diag->x) {
		    if (left->x > top->x)
		      px = top->x;
		    else
		      px = left->x;
		  }
		} else {
		  if (diag->x > left->x) {
		    if (left->x > top->x)
		      px = left->x;
		    else
		      px = top->x;
		  }
		}
		py = diag->y;
		if (top->y > diag->y) {
		  if (left->y > diag->y) {
		    if (left->y > top->y)
		      py = top->y;
		    else
		      py = left->y;
		  }
		} else {
		  if (diag->y > left->y) {
		    if (left->y > top->y)
		      py = left->y;
		    else
		      py = top->y;
		  }
		}
	      }
	    } else if (x) {
	      px = left->x;
	      py = left->y;
	    } else {
	      px = 0;
	      py = 0;
	    }

	    {
	      int qx, qy, X, Y;

	      qx = x<<5;
	      qy = y<<5;
	      X = (mb_width<<5) - 4;
	      Y = (mb_height<<5) - 4;
	      if (qx+px < -28) 
		px = -28 - qx;
	      if (qy+py < -28) 
		py = -28 - qy;
	      if (qx+px > X) 
		px = X - qx;
	      if (qy+py > Y) 
		py = Y - qy;
	    }

	    bmv.x = ((px + dmv_x[1] + r_x) & ((r_x << 1) - 1)) - r_x;
	    bmv.y = ((py + dmv_y[1] + r_y) & ((r_y << 1) - 1)) - r_y;

	    VVC1_predict_current.bmv = bmv;
	  }
	}
      }

      if(x) {
	int iik ;
	VVC1_predict_mem[x-1].quant = VVC1_predict_left.quant ;
	for (iik=0;iik<6;iik++) {
	  VVC1_predict_mem[x-1].dc_value[iik] = VVC1_predict_left.dc_value[iik] ;
	  VVC1_predict_mem[x-1].is_intra[iik] = VVC1_predict_left.is_intra[iik] ;
	}
	for (iik=0;iik<4;iik++) {
	  VVC1_predict_mem[x-1].coded[iik] = VVC1_predict_left.coded[iik] ;
	}
	VVC1_predict_mem[x-1].fmv[0] = VVC1_predict_left.fmv[0] ;
	VVC1_predict_mem[x-1].bmv = VVC1_predict_left.bmv ;
      }
      {  
	int iik ;
	VVC1_predict_left.quant = VVC1_predict_current.quant ;
	for (iik=0;iik<6;iik++) {
	  VVC1_predict_left.dc_value[iik] = VVC1_predict_current.dc_value[iik] ;
	  VVC1_predict_left.is_intra[iik] = VVC1_predict_current.is_intra[iik] ;
	}
	for (iik=0;iik<4;iik++) {
	  VVC1_predict_left.coded[iik] = VVC1_predict_current.coded[iik] ;
	}
	VVC1_predict_left.fmv[0] = VVC1_predict_current.fmv[0] ;
	VVC1_predict_left.bmv = VVC1_predict_current.bmv ;
      }
      if(x == mb_width-1) {
	int iik ;
	VVC1_predict_mem[x].quant = VVC1_predict_current.quant ;
	for (iik=0;iik<6;iik++) {
	  VVC1_predict_mem[x].dc_value[iik] = VVC1_predict_current.dc_value[iik] ;
	  VVC1_predict_mem[x].is_intra[iik] = VVC1_predict_current.is_intra[iik] ; 
	}
	for (iik=0;iik<4;iik++) {
	  VVC1_predict_mem[x].coded[iik] = VVC1_predict_current.coded[iik] ; 
	}
	VVC1_predict_mem[x].fmv[0] = VVC1_predict_current.fmv[0] ;
	VVC1_predict_mem[x].bmv = VVC1_predict_current.bmv ;
      } 

      if (!mb_intra) {
	mvunit_fwd = _1MV;
	mvunit_bwd = _1MV;
	switch (bmv_type) {
	case MODE_FORWARD:
	  mb_type = Forward;
	  mvunit_bwd = NoMV;
	  break;
	case MODE_BACKWARD:
	  mb_type = Backward;
	  mvunit_fwd = NoMV;
	  break;
	case MODE_DIRECT:
	  mb_type = Bidir;
	  break;
	case MODE_INTERPOLATE:
	  mb_type = Bidir;
	  break;
	default:
	  mb_type = Intra;
	}
	blk_type = 0x3f;
      } else {
	mb_type = Intra;
	blk_type = 0;
	mvunit_fwd = NoMV;
	mvunit_bwd = NoMV;
      }

      VVC1_cs[VVC1_buf_num]->MB_y = y;
      VVC1_cs[VVC1_buf_num]->MB_x = x;

      if (mvunit_fwd != NoMV) {
	int umv = 0;

	if (((((int)x)<<(5+VVC1_decoder.quarterpel)) + fmv.x) <= 3) {
	  umv = 1;
	}
	if (((((int)y)<<(5+VVC1_decoder.quarterpel)) + fmv.y) <= 3) {
	  umv = 1;
	}
	if (((((int)x)<<(5+VVC1_decoder.quarterpel)) + fmv.x) >= (VVC1_decoder.width-18)<<(1+VVC1_decoder.quarterpel)) {
	  umv = 1;
	}
	if (((((int)y)<<(5+VVC1_decoder.quarterpel)) + fmv.y) >= (VVC1_decoder.height-18)<<(1+VVC1_decoder.quarterpel)) {
	  umv = 1;
	}

	if (umv) {
	  mvunit_fwd = _4MV;
	}

	VVC1_cs[VVC1_buf_num]->MVx[0].FWD = fmv.x;
	VVC1_cs[VVC1_buf_num]->MVx[1].FWD = fmv.x;
	VVC1_cs[VVC1_buf_num]->MVx[2].FWD = fmv.x;
	VVC1_cs[VVC1_buf_num]->MVx[3].FWD = fmv.x;
	VVC1_cs[VVC1_buf_num]->MVy[0].FWD = fmv.y;
	VVC1_cs[VVC1_buf_num]->MVy[1].FWD = fmv.y;
	VVC1_cs[VVC1_buf_num]->MVy[2].FWD = fmv.y;
	VVC1_cs[VVC1_buf_num]->MVy[3].FWD = fmv.y;
      }

      if (mvunit_bwd != NoMV) {
	int umv = 0;

	if (((((int)x)<<(5+VVC1_decoder.quarterpel)) + bmv.x) <= 3) {
	  umv = 1;
	}
	if (((((int)y)<<(5+VVC1_decoder.quarterpel)) + bmv.y) <= 3) {
	  umv = 1;
	}
	if (((((int)x)<<(5+VVC1_decoder.quarterpel)) + bmv.x) >= (VVC1_decoder.width-18)<<(1+VVC1_decoder.quarterpel)) {
	  umv = 1;
	}
	if (((((int)y)<<(5+VVC1_decoder.quarterpel)) + bmv.y) >= (VVC1_decoder.height-18)<<(1+VVC1_decoder.quarterpel)) {
	  umv = 1;
	}

	if (umv) {
	  mvunit_bwd = _4MV;
	}

	VVC1_cs[VVC1_buf_num]->MVx[0].BWD = bmv.x;
	VVC1_cs[VVC1_buf_num]->MVx[1].BWD = bmv.x;
	VVC1_cs[VVC1_buf_num]->MVx[2].BWD = bmv.x;
	VVC1_cs[VVC1_buf_num]->MVx[3].BWD = bmv.x;
	VVC1_cs[VVC1_buf_num]->MVy[0].BWD = bmv.y;
	VVC1_cs[VVC1_buf_num]->MVy[1].BWD = bmv.y;
	VVC1_cs[VVC1_buf_num]->MVy[2].BWD = bmv.y;
	VVC1_cs[VVC1_buf_num]->MVy[3].BWD = bmv.y;
      }

      if ((mvunit_fwd == _1MV) && (mvunit_bwd == _4MV)) {
	mvunit_fwd = _4MV;
      }
      if ((mvunit_fwd == _4MV) && (mvunit_bwd == _1MV)) {
	mvunit_bwd = _4MV;
      }

      VVC1_cs[VVC1_buf_num]->MVunit.FWD = mvunit_fwd;
      VVC1_cs[VVC1_buf_num]->MVunit.BWD = mvunit_bwd;

      VVC1_mp_sync();

      VVC1_icm_num = (VVC1_icm_num + 1) % ICM_BUFFERS_USED;

      VVC1_cs[VVC1_buf_num]->MB_TYPE = mb_type | (blk_type << 4) | ((unsigned)(VVC1_icm_num*0x80)<<16);
      VVC1_cs[VVC1_buf_num]->SP_INFO = VVC1_decoder.sp_info;

#ifdef _INDIRECT
      VVC1_buf_num = (VVC1_buf_num + 1);
#else
      VVC1_buf_num = (VVC1_buf_num + 1) % ICM_BUFFERS_USED;
#endif

      VVC1_decoder.sp_info &= (unsigned)-3;

#ifdef _INDIRECT
      if (VVC1_buf_num == VVC1_decoder.groupHandle.buffergroup_size) {
	int rc;

	VVC1_buf_num = 0;
	
	rc = vid_send_buffers(&VVC1_decoder.groupHandle) ;
	if (VMP_E == rc) {
	  return VVC1_VID_STATUS_ERROR;
	}

	rc = vid_get_buffers(CORE_USED, &VVC1_decoder.groupHandle, 1); // blocking 
	if (rc != VMP_OK) {
	  return VVC1_VID_STATUS_ERROR_MP_TIMEOUT;
	} else {
	  VVC1_cs[0] = (CTRL_STRUCT  *)VVC1_decoder.groupHandle.address.virt_addr;
	  {
	    int i;
	    for (i = 1; i < VVC1_decoder.groupHandle.buffergroup_size; i++) {
	      VVC1_cs[i] = VVC1_cs[0] + i;
	    }
	  }
	}
      }
#endif
    } 
  }
  
  return VVC1_VID_STATUS_OK;
}



/*lint -restore */
/* PRQA L:L1 */
