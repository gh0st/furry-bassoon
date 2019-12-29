/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *                                                                          
 * DESCRIPTION:         macroblock related decoding routines
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
#include <stdlib.h>

#include "vvc1_buffer.h"
#include "vvc1_decoder.h"
#include "vvc1_bitstream.h"
#include "vvc1_tables.h"
#include "vvc1_decode_mb.h"
#include "vvc1_ctrl_struct.h"

/* PRQA: QAC Message 277, 3747, 3758, 3760, 3768, 3769, 3771: deactivation because of pre-existing IP*/
/* PRQA S 277, 3747, 3758, 3760, 3768, 3769, 3771 L1*/
/* PRQA: Lint Message 10, 40, 64, 529, 676:deactivation because of pre-existing IP */
/*lint -save -e10 -e40 -e64 -e529 -e676*/


static const unsigned short dcpred_table[32] = {
  (unsigned short)-1, 1024,  512,  341,  256,  205,  171,  146,  128,
  114,  102,   93,   85,   79,   73,   68,   64,
  60,   57,   54,   51,   49,   47,   45,   43,
  41,   39,   38,   37,   35,   34,   33
};

static const int ttblk_to_tt[3][8] = {
  { TT_8X4, TT_4X8, TT_8X8, TT_4X4, TT_8X4_TOP, TT_8X4_BOTTOM, TT_4X8_RIGHT, TT_4X8_LEFT },
  { TT_8X8, TT_4X8_RIGHT, TT_4X8_LEFT, TT_4X4, TT_8X4, TT_4X8, TT_8X4_BOTTOM, TT_8X4_TOP },
  { TT_8X8, TT_4X8, TT_4X4, TT_8X4_BOTTOM, TT_4X8_RIGHT, TT_4X8_LEFT, TT_8X4, TT_8X4_TOP }
};




void VVC1_decode_mbintra(int x_pos, int y_pos, int cbp, int acpred_flag)
{
  unsigned int i;
  unsigned char iQuant = (unsigned char) VVC1_predict_current.quant;

  short *block_ptr;
  short dc_val;

  idct_t blk_compl;
  unsigned int tr_complexity = 0;
  unsigned int acpred_dir = 0;
  unsigned int ovl_borders = (VVC1_decoder.overlappedtransform && (VVC1_decoder.pquant >= 9)) << 31;
  unsigned int lf_top_borders = VVC1_decoder.loopfilter << 31;
  unsigned int lf_left_borders = VVC1_decoder.loopfilter << 31;
  short default_dc_value;

  unsigned dcscale = VVC1_decoder.dcscale;

  register int value, n;

  unsigned *LCurBitstr = VVC1_CurBitstr;
  unsigned LBitstrL = VVC1_BitstrL;
  unsigned LBitstrR = VVC1_BitstrR;
  unsigned LCurOffset = VVC1_CurOffset;

  if ((VVC1_decoder.pquant < 9) || !VVC1_decoder.overlappedtransform) {
    default_dc_value = dcpred_table[dcscale];
  } else {
    default_dc_value = 0;
  }

  for (i = 0; i < 6; i++) {
    short DCpredictor;
    unsigned coded;
    unsigned coding_set;
    int acpred_direction;

    block_ptr = (short *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];

    {
      short *left, *top, *diag, *current;

      int DCLeft = default_dc_value;
      int DCTop = default_dc_value;
      int DCDiag = default_dc_value;

      left = top = diag = current = 0;

      if (x_pos) {
	left = &VVC1_predict_left.dc_value[0];
      }
      if (y_pos) {
	top = &VVC1_predict_mem[x_pos].dc_value[0];
      }
      if (x_pos && y_pos) {
	diag = &VVC1_predict_mem[x_pos-1].dc_value[0];
      }
      current = &VVC1_predict_current.dc_value[0];

      switch (i) {
	
      case 0:
	if (left)
	  DCLeft = left[1];
	if (top)
	  DCTop = top[2];
	if (diag)
	  DCDiag = diag[3];
	break;

      case 1:
	DCLeft = current[0];
	if (top) {
	  DCTop = top[3];
	  DCDiag = top[2];
	}
	break;

      case 2:
	if (left) {
	  DCLeft = left[3];
	  DCDiag = left[1];
	}
	DCTop = current[0];
	break;

      case 3:
	DCLeft = current[2];
	DCTop = current[1];
	DCDiag = current[0];
	break;

      case 4:
	if (left)
	  DCLeft = left[4];
	if (top)
	  DCTop = top[4];
	if (diag)
	  DCDiag = diag[4];
	break;

      case 5:
	if (left)
	  DCLeft = left[5];
	if (top)
	  DCTop = top[5];
	if(diag)
	  DCDiag = diag[5];
	break;
      }

      if (abs(DCLeft - DCDiag) < abs(DCDiag - DCTop)) {
	acpred_direction = 1;
	DCpredictor = DCTop;
      } else {
	acpred_direction = 2;
	DCpredictor = DCLeft;
      }

      if (i==0) {
	unsigned top_reset, left_reset;

	top_reset = (top == 0);
	left_reset = (left == 0);
	acpred_dir = (top_reset << 25) | (left_reset << 24);

	VVC1_cs[VVC1_buf_num]->AC_SCALE_TOP = 1<<18;
	VVC1_cs[VVC1_buf_num]->AC_SCALE_LEFT = 1<<18;
      }
    }

    if (VVC1_decoder.overlappedtransform && (VVC1_decoder.pquant >= 9))
      ovl_borders = ovl_borders | (y_pos ? (0x3f<<16) : (0xc<<16)) | (x_pos ? 0x3f : 0xa);

    if (VVC1_decoder.loopfilter) {
      lf_top_borders =  lf_top_borders | 0x333333;
      lf_left_borders = lf_left_borders | 0x333333;
    }

    if (!acpred_flag) {
      acpred_direction = 0;
    }

    acpred_dir = acpred_dir | (acpred_direction << (4*i));

    if (i < 4) {
      unsigned code;

      coded = ((cbp >> (5 - i)) & 1);
      coding_set = VVC1_decoder.codingset;
      
      {
	unsigned nb_bits;
	
	code = ((LBitstrL << LCurOffset) | ((LBitstrR >> (31 - LCurOffset))>>1));
	n     = VVC1_dc_luma_vlc_2[VVC1_decoder.intratransformDCtable].table[(code>>23)][1];
	value = VVC1_dc_luma_vlc_2[VVC1_decoder.intratransformDCtable].table[(code>>23)][0];
	if (n < 0){
	  LCurOffset += 9;
	  code = code << 9;
	  
	  nb_bits = -n;
	  
	  n     = VVC1_dc_luma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][1];
	  value = VVC1_dc_luma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][0];
	  
	  if (n < 0){
	    LCurOffset += nb_bits;
	    code = code << nb_bits;
	    
	    nb_bits = -n;
	    
	    n     = VVC1_dc_luma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][1];
	    value = VVC1_dc_luma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][0];
	  }
	}
	
	LCurOffset += n;


	if (value) {
	  if (value == 119) {

	    if (LCurOffset >= 32) {
	      LCurOffset -= 32;
	      LBitstrL = LBitstrR;
	      VVC1_NTOHL(LBitstrR, *LCurBitstr)
	      LCurBitstr++;
	    }

	    VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)

	    if (VVC1_decoder.pquant == 1) {
	      value = code >> (32-10);
	      value = ((code >> (32-11)) & 0x1) ? -value : value;
	      LCurOffset += 11;
	    } else if (VVC1_decoder.pquant == 2) {
	      value = code >> (32-9);
	      value = ((code >> (32-10)) & 0x1) ? -value : value;
	      LCurOffset += 10;
	    } else {
	      value = code >> (32-8);
	      value = ((code >> (32-9)) & 0x1) ? -value : value;
	      LCurOffset += 9;
	    }
	  } else {
	    code = code << n;

	    if (VVC1_decoder.pquant == 1) {
	      value = (value<<2) + (code >> (32-2)) - 3;
	      value = ((code >> (32-3)) & 0x1) ? -value : value;
	      LCurOffset += 3;
	    } else if (VVC1_decoder.pquant == 2) {
	      value = (value<<1) + (code >> (32-1)) - 1;
	      value = ((code >> (32-2)) & 0x1) ? -value : value;
	      LCurOffset += 2;
	    } else {
	      value = ((code >> (32-1)) & 0x1) ? -value : value;
	      LCurOffset += 1;		  
	    }
	  }
	}

      }

      { 
	unsigned pred = 0;

	switch (i) {
	  
	case 0:
	  if ((x_pos && (VVC1_predict_mem[x_pos-1].coded[3] == VVC1_predict_mem[x_pos].coded[2])) ||
	      (!x_pos && !VVC1_predict_mem[x_pos].coded[2]))
	    pred = VVC1_predict_left.coded[1];
	  else
	    pred = VVC1_predict_mem[x_pos].coded[2];
	  break;
	  
	case 1:
	  if (VVC1_predict_mem[x_pos].coded[2] == VVC1_predict_mem[x_pos].coded[3])
	    pred = VVC1_predict_current.coded[0];
	  else
	    pred = VVC1_predict_mem[x_pos].coded[3];
	  break;
	  
	case 2:
	  if (VVC1_predict_left.coded[1] == VVC1_predict_current.coded[0])
	    pred = VVC1_predict_left.coded[3];
	  else
	    pred = VVC1_predict_current.coded[0];
	  break;
	  
	case 3:
	  if (VVC1_predict_current.coded[0] == VVC1_predict_current.coded[1])
	    pred = VVC1_predict_current.coded[2];
	  else
	    pred = VVC1_predict_current.coded[1];
	  break;
	}
	
	coded = coded ^ pred;
	VVC1_predict_current.coded[i] = coded;
      }

      cbp |= coded << (5 - i);


    } else {
      unsigned code;

      coded = ((cbp >> (5 - i)) & 1);
      coding_set = VVC1_decoder.codingset2;

      {
	unsigned nb_bits;
	
	VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)
	
	n     = VVC1_dc_chroma_vlc_2[VVC1_decoder.intratransformDCtable].table[(code>>23)][1];
	value = VVC1_dc_chroma_vlc_2[VVC1_decoder.intratransformDCtable].table[(code>>23)][0];
	
	if (n < 0){
	  LCurOffset += 9;
	  code = code << 9;
	  
	  nb_bits = -n;
	  
	  n     = VVC1_dc_chroma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][1];
	  value = VVC1_dc_chroma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][0];
	  
	  if (n < 0){
	    LCurOffset += nb_bits;
	    code = code << nb_bits;
	    
	    nb_bits = -n;
	    
	    n     = VVC1_dc_chroma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][1];
	    value = VVC1_dc_chroma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][0];
	  }
	}
		
	LCurOffset += n;

	if (value) {
	  if (value == 119) {
	    
	    if (LCurOffset >= 32) {
	      LCurOffset -= 32;
	      LBitstrL = LBitstrR;
	      VVC1_NTOHL(LBitstrR, *LCurBitstr)
	      LCurBitstr++;
	    }

	    VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)

	    if (VVC1_decoder.pquant == 1) {
	      value = code >> (32-10);
	      value = ((code >> (32-11)) & 0x1) ? -value : value;
	      LCurOffset += 11;
	    } else if (VVC1_decoder.pquant == 2) {
	      value = code >> (32-9);
	      value = ((code >> (32-10)) & 0x1) ? -value : value;
	      LCurOffset += 10;
	    } else {
	      value = code >> (32-8);
	      value = ((code >> (32-9)) & 0x1) ? -value : value;
	      LCurOffset += 9;
	    }
	  } else {
	    code = code << n;

	    if (VVC1_decoder.pquant == 1) {
	      value = (value<<2) + (code >> (32-2)) - 3;
	      value = ((code >> (32-3)) & 0x1) ? -value : value;
	      LCurOffset += 3;
	    } else if (VVC1_decoder.pquant == 2) {
	      value = (value<<1) + (code >> (32-1)) - 1;
	      value = ((code >> (32-2)) & 0x1) ? -value : value;
	      LCurOffset += 2;
	    } else {
	      value = ((code >> (32-1)) & 0x1) ? -value : value;
	      LCurOffset += 1;		  
	    }
	  }
	}
      }

    }

    dc_val = value;
		
    if (LCurOffset >= 32) {
      LCurOffset -= 32;
      LBitstrL = LBitstrR;
      VVC1_NTOHL(LBitstrR, *LCurBitstr)
      LCurBitstr++;
    }

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

    if (coded) {
      int direction = acpred_direction;

      const unsigned ac_sizes = VVC1_ac_sizes[coding_set] - 1;
      const unsigned last_decode = VVC1_last_decode_table[coding_set];

      {
	register unsigned code;
	int escape;
	    
	int coeff = 1;
	short * block = &block_ptr[0];
	const unsigned char *scan = (unsigned char *)&VVC1_scan_tables_transp[direction][0];
	unsigned last = 0;
	int run = 0;
	int level = 0;
	unsigned scan_coeff;

	while (!last) {

	  {
	    unsigned nb_bits;

	    VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)

	    n     = VVC1_ac_coeff_table_2[coding_set].table[(code>>23)][1];
	    value = VVC1_ac_coeff_table_2[coding_set].table[(code>>23)][0];

	    if (n < 0){
	      LCurOffset += 9;
	      code = code << 9;
	  
	      nb_bits = -n;
		
	      n     = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][1];
	      value = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][0];
		
	      if (n < 0){
		LCurOffset += nb_bits;
		code = code << nb_bits;

		nb_bits = -n;
		  
		n     = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][1];
		value = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][0];
	      }
	    }

	  }

	  if (value != ac_sizes) {
	    coeff += VVC1_index_decode_table[coding_set][value][0];
	    level = VVC1_index_decode_table[coding_set][value][1];
	    last = value >= last_decode;

	    LCurOffset += n+1;
	    if (coeff > 63) 
	      break;
	    scan_coeff = scan[coeff];
	    coeff++;
	    block[scan_coeff] = ((code >> (32-n-1)) & 0x1) ? -level : level;

	    if (LCurOffset >= 32) {
	      LCurOffset -= 32;
	      LBitstrL = LBitstrR;
	      VVC1_NTOHL(LBitstrR, *LCurBitstr)
	      LCurBitstr++;
	    }

	  } else {
	    if ((code >> (32-n-1)) & 0x1) {
	      LCurOffset += n+1;
	      escape = 0;
	    } else {
	      LCurOffset += n+2;
	      escape = 2 - ((code >> (32-n-2)) & 0x1);
	    }

	    if (LCurOffset >= 32) {
	      LCurOffset -= 32;
	      LBitstrL = LBitstrR;
	      VVC1_NTOHL(LBitstrR, *LCurBitstr)
	      LCurBitstr++;
	    }

	    if (escape != 2) {
	      {
		unsigned nb_bits;
		      
		VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)
		      
		n     = VVC1_ac_coeff_table_2[coding_set].table[(code>>23)][1];
		value = VVC1_ac_coeff_table_2[coding_set].table[(code>>23)][0];

		if (n < 0){
		  LCurOffset += 9;
		  code = code << 9;
			
		  nb_bits = -n;
			
		  n     = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][1];
		  value = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][0];
			
		  if (n < 0){
		    LCurOffset += nb_bits;
		    code = code << nb_bits;
			  
		    nb_bits = -n;
			  
		    n     = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][1];
		    value = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][0];
		  }
		}
		      
		LCurOffset += n;
	      }

	      run = VVC1_index_decode_table[coding_set][value][0];
	      level = VVC1_index_decode_table[coding_set][value][1];
	      last = value >= last_decode;

	      if(escape == 0) {
		if(last)
		  level += VVC1_last_delta_level_table[coding_set][run];
		else
		  level += VVC1_delta_level_table[coding_set][run];
	      } else {
		if(last)
		  run += VVC1_last_delta_run_table[coding_set][level] + 1;
		else
		  run += VVC1_delta_run_table[coding_set][level] + 1;
	      }
	      LCurOffset += 1;
	      coeff += run;
	      if (coeff > 63) 
		break;
	      scan_coeff = scan[coeff];
	      coeff++;
	      block[scan_coeff] = ((code >> (32-n-1)) & 0x1) ? -level : level;
	  
	    } else {
	      int sign;

	      VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)

	      last = (code >> (32-1));
	      code = code << 1;
		    
	      if(VVC1_decoder.esc3_level_length == 0) {

		if(VVC1_decoder.pquant < 8 || VVC1_decoder.dquantframe) {
		  VVC1_decoder.esc3_level_length = Block_Escape_Mode_3_PQUANT_1_To_7[(code >> (32-5))].value;
		  n = Block_Escape_Mode_3_PQUANT_1_To_7[(code >> (32-5))].length;
		  LCurOffset += n;

		  VVC1_decoder.esc3_run_length = 3 + ((code << n) >> (32-2));
			
		  code = code << (n+2);

		  coeff += (code >> (31-VVC1_decoder.esc3_run_length))>>1;
		  sign = (code >> (32-VVC1_decoder.esc3_run_length-1)) & 0x1;
		  level = (((code << (VVC1_decoder.esc3_run_length+1)) >> (31-VVC1_decoder.esc3_level_length))>>1);

		  LCurOffset += 1 + 2 + VVC1_decoder.esc3_run_length + VVC1_decoder.esc3_level_length + 1;

		  if (coeff > 63) 
		    break;
		  scan_coeff = scan[coeff];
		  coeff++;
		  block[scan_coeff] = sign ? -level : level;
		      
		} else {
		  VVC1_decoder.esc3_level_length = Block_Escape_Mode_3_PQUANT_8_To_31[(code >> (32-6))].value;
		  n = Block_Escape_Mode_3_PQUANT_8_To_31[(code >> (32-6))].length;
		  LCurOffset += n;

		  VVC1_decoder.esc3_run_length = 3 + ((code << n) >> (32-2));

		  code = code << (n+2);

		  coeff += (code >> (31-VVC1_decoder.esc3_run_length))>>1;
		  sign = (code >> (32-VVC1_decoder.esc3_run_length-1)) & 0x1;
		  level = (((code << (VVC1_decoder.esc3_run_length+1)) >> (31-VVC1_decoder.esc3_level_length))>>1);

		  LCurOffset += 1 + 2 + VVC1_decoder.esc3_run_length + VVC1_decoder.esc3_level_length + 1;

		  if (coeff > 63) 
		    break;
		  scan_coeff = scan[coeff];
		  coeff++;
		  block[scan_coeff] = sign ? -level : level;
		      
		}

	      } else {		      
		coeff += (code >> (31-VVC1_decoder.esc3_run_length))>>1;
		sign = (code >> (32-VVC1_decoder.esc3_run_length-1)) & 0x1;
		level = (((code << (VVC1_decoder.esc3_run_length+1)) >> (31-VVC1_decoder.esc3_level_length))>>1);

		LCurOffset += 1 + VVC1_decoder.esc3_run_length + VVC1_decoder.esc3_level_length + 1;

		if (coeff > 63) 
		  break;
		scan_coeff = scan[coeff];
		coeff++;
		block[scan_coeff] = sign ? -level : level;
	      }

	    }

	    if (LCurOffset >= 32) {
	      LCurOffset -= 32;
	      LBitstrL = LBitstrR;
	      VVC1_NTOHL(LBitstrR, *LCurBitstr)
	      LCurBitstr++;
	    }

	  }
		
	}
      }
    }

    dc_val += DCpredictor;

    VVC1_predict_current.dc_value[i] = dc_val;

    block_ptr[0] = dc_val * dcscale;

    blk_compl = Fl;

    tr_complexity = tr_complexity | (blk_compl << (4*i));
  }
  
  VVC1_cs[VVC1_buf_num]->BLOCK_QP_MUL = (unsigned) (2*iQuant + ((iQuant == VVC1_decoder.pquant) ? VVC1_decoder.halfqpstep : 0));
  VVC1_cs[VVC1_buf_num]->BLOCK_QP_ADD = (VVC1_decoder.pquantizer == vc1_QuantizerUniform) ? 0 : (unsigned) iQuant;
			
  VVC1_cs[VVC1_buf_num]->AC_PRED_DIR = acpred_dir;
  VVC1_cs[VVC1_buf_num]->TR_COMPLEXITY = tr_complexity;
  VVC1_cs[VVC1_buf_num]->OVL_BORDERS = ovl_borders;
  VVC1_cs[VVC1_buf_num]->LF_FLAGS_TOP = lf_top_borders;
  VVC1_cs[VVC1_buf_num]->LF_FLAGS_LEFT = lf_left_borders;
  VVC1_cs[VVC1_buf_num]->TRANSFORM_TYPE = (((((((((T_8x8<<4)|T_8x8)<<4)|T_8x8)<<4)|T_8x8)<<4)|T_8x8)<<4)|T_8x8;
  VVC1_cs[VVC1_buf_num]->POS = 0x888888;

  VVC1_CurBitstr = LCurBitstr;
  VVC1_BitstrL = LBitstrL;
  VVC1_BitstrR = LBitstrR;
  VVC1_CurOffset = LCurOffset;
}









void VVC1_decode_mbinter_intra(int x_pos, int y_pos, int cbp, int is_intra[6], 
			       int mquant, int acpred_flag, int ttmb)
{
  unsigned int i;
  unsigned char iQuant = (unsigned char) VVC1_predict_current.quant;

  short *block_ptr;
  short dc_val;

  idct_t blk_compl, subblk_compl;
  unsigned int tr_complexity = 0;
  unsigned int tr_type = 0;
  unsigned int position = 0;
  unsigned int acpred_dir = 0;
  unsigned int ovl_borders = (VVC1_decoder.overlappedtransform && (VVC1_decoder.pquant >= 9)) << 31;
  unsigned int lf_top_borders = VVC1_decoder.loopfilter << 31;
  unsigned int lf_left_borders = VVC1_decoder.loopfilter << 31;
  int ac_scale_top = 0;
  int ac_scale_left = 0;
	    
  unsigned pos = 0;

  unsigned dcscale = VVC1_decoder.dcscale;

  register int value, n;

  int firstblock = 1;

  unsigned *LCurBitstr = VVC1_CurBitstr;
  unsigned LBitstrL = VVC1_BitstrL;
  unsigned LBitstrR = VVC1_BitstrR;
  unsigned LCurOffset = VVC1_CurOffset;

  for (i = 0; i < 6; i++) {
    short DCpredictor;
    unsigned coded;
    unsigned coding_set;

    block_ptr = (short *)VVC1_cs[VVC1_buf_num]->IDCT_DATA[i];

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

    if (is_intra[i]) {

      int acpred_direction = 0;

      mquant = (mquant < 1) ? 0 : ( (mquant>31) ? 31 : mquant );

      dcscale = VVC1_dc_scale_table[mquant];
    
      {
	int left_avail = 0, top_avail = 0;

	unsigned char left_quant = (unsigned char) iQuant;
	unsigned char top_quant = (unsigned char) iQuant;
	unsigned char diag_quant = (unsigned char) iQuant;

	int DCLeft = 0;
	int DCTop = 0;
	int DCDiag = 0;
	
	if (x_pos) {
	  left_quant = VVC1_predict_left.quant;
	}
	if (y_pos) {
	  top_quant = VVC1_predict_mem[x_pos].quant;
	}

	switch (i) {

	case 0:
	  left_avail = x_pos && VVC1_predict_left.is_intra[1];
	  if (left_avail)
	    DCLeft = VVC1_predict_left.dc_value[1];
	  top_avail = y_pos && VVC1_predict_mem[x_pos].is_intra[2];
	  if (top_avail)
	    DCTop = VVC1_predict_mem[x_pos].dc_value[2];
	  if (VVC1_predict_mem[x_pos-1].is_intra[3]) {
	    DCDiag = VVC1_predict_mem[x_pos-1].dc_value[3];
	    diag_quant = VVC1_predict_mem[x_pos-1].quant;
	  }
	  break;

	case 1:
	  left_avail = is_intra[0];
	  if (left_avail)
	    DCLeft = VVC1_predict_current.dc_value[0];
	  left_quant = iQuant;
	  top_avail = y_pos && VVC1_predict_mem[x_pos].is_intra[3];
	  if (top_avail) {
	    DCTop = VVC1_predict_mem[x_pos].dc_value[3];
	  }
	  if (VVC1_predict_mem[x_pos].is_intra[2]) {
	    DCDiag = VVC1_predict_mem[x_pos].dc_value[2];
	    diag_quant = VVC1_predict_mem[x_pos].quant;
	  }
	  break;

	case 2:
	  left_avail = x_pos && VVC1_predict_left.is_intra[3];
	  if (left_avail) {
	    DCLeft = VVC1_predict_left.dc_value[3];
	  }
	  top_avail = is_intra[0];
	  if (top_avail)
	    DCTop = VVC1_predict_current.dc_value[0];
	  top_quant = iQuant;
	  if (VVC1_predict_left.is_intra[1]) {
	    DCDiag = VVC1_predict_left.dc_value[1];
	    diag_quant = VVC1_predict_left.quant;
	  }
	  break;

	case 3:
	  left_avail = is_intra[2];
	  if (left_avail)
	    DCLeft = VVC1_predict_current.dc_value[2];
	  left_quant = iQuant;
	  top_avail = is_intra[1];
	  if (top_avail)
	    DCTop = VVC1_predict_current.dc_value[1];
	  top_quant = iQuant;
	  if (is_intra[0])
	    DCDiag = VVC1_predict_current.dc_value[0];
	  break;

	case 4:
	  left_avail = x_pos && VVC1_predict_left.is_intra[4];
	  if (left_avail)
	    DCLeft = VVC1_predict_left.dc_value[4];
	  top_avail = y_pos && VVC1_predict_mem[x_pos].is_intra[4];
	  if (top_avail)
	    DCTop = VVC1_predict_mem[x_pos].dc_value[4];
	  if (VVC1_predict_mem[x_pos-1].is_intra[4]) {
	    DCDiag = VVC1_predict_mem[x_pos-1].dc_value[4];
	    diag_quant = VVC1_predict_mem[x_pos-1].quant;
	  }
	  break;

	case 5:
	  left_avail = x_pos && VVC1_predict_left.is_intra[5];
	  if (left_avail)
	    DCLeft = VVC1_predict_left.dc_value[5];
	  top_avail = y_pos && VVC1_predict_mem[x_pos].is_intra[5];
	  if (top_avail)
	    DCTop = VVC1_predict_mem[x_pos].dc_value[5];
	  if (VVC1_predict_mem[x_pos-1].is_intra[5]) {
	    DCDiag = VVC1_predict_mem[x_pos-1].dc_value[5];
	    diag_quant = VVC1_predict_mem[x_pos-1].quant;
	  }
	  break;
	}

	if (VVC1_decoder.overlappedtransform && (VVC1_decoder.pquant >= 9))
	  ovl_borders = ovl_borders | (left_avail << i) | (top_avail << (i+16));

	if (VVC1_decoder.loopfilter) {
	  lf_top_borders =  lf_top_borders | 0x333333;
	  lf_left_borders = lf_left_borders | 0x333333;
	}

	if (DCTop && (i != 2) && (i != 3) && (iQuant != top_quant))
	  DCTop = ((int)(DCTop * VVC1_dc_scale_table[top_quant] * VVC1_dqscale_table[dcscale] + 0x20000)) >> 18;
	if (DCLeft && (i != 1) && (i != 3) && (iQuant != left_quant))
	  DCLeft = ((int)(DCLeft * VVC1_dc_scale_table[left_quant] * VVC1_dqscale_table[dcscale] + 0x20000)) >> 18;
	if (DCDiag && (i != 3) && (iQuant != diag_quant))
	  DCDiag = ((int)(DCDiag * VVC1_dc_scale_table[diag_quant] * VVC1_dqscale_table[dcscale] + 0x20000)) >> 18;	


	if (top_avail && left_avail) {
	  if (abs(DCLeft - DCDiag) < abs(DCDiag - DCTop)) {
	    acpred_direction = 1;
	    DCpredictor = DCTop;
	  } else {
	    acpred_direction = 2;
	    DCpredictor = DCLeft;
	  }
	} else if (top_avail) {
	  acpred_direction = 1;
	  DCpredictor = DCTop;	  
	} else if (left_avail) {
	  acpred_direction = 2;
	  DCpredictor = DCLeft;
	} else {
	  acpred_direction = 0;
	  DCpredictor = 0;
	}

	if (i < 4) {
	  unsigned top_reset, left_reset;
	  
	  top_reset = (y_pos == 0);
	  left_reset = (x_pos == 0);
	  acpred_dir = (acpred_dir) | (top_reset << (25+2*i)) | (left_reset << (24+2*i));

	  if (!ac_scale_top && top_avail && top_quant && (iQuant != top_quant)) {
	    ac_scale_top = (2*top_quant + ((top_quant == VVC1_decoder.pquant) ? VVC1_decoder.halfqpstep : 0) - 1) * 
	      VVC1_dqscale_table[(2*iQuant + ((iQuant == VVC1_decoder.pquant) ? VVC1_decoder.halfqpstep : 0)) - 1];	    
	  }
	  
	  if (!ac_scale_left && left_avail && left_quant && (iQuant != left_quant)) {
	    ac_scale_left = (2*left_quant + ((left_quant == VVC1_decoder.pquant) ? VVC1_decoder.halfqpstep : 0) - 1) * 
	      VVC1_dqscale_table[(2*iQuant + ((iQuant == VVC1_decoder.pquant) ? VVC1_decoder.halfqpstep : 0)) - 1];
	  }
	}
      }

      if (!acpred_flag) {
	acpred_direction = 0;
      }

      acpred_dir = acpred_dir | (acpred_direction << 4*i);

      if (i < 4) {
	unsigned code;

	coded = ((cbp >> (5 - i)) & 1);
	coding_set = VVC1_decoder.codingset;

	{
	  unsigned nb_bits;
	
	  VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)

	  n     = VVC1_dc_luma_vlc_2[VVC1_decoder.intratransformDCtable].table[(code>>23)][1];
	  value = VVC1_dc_luma_vlc_2[VVC1_decoder.intratransformDCtable].table[(code>>23)][0];
	  if (n < 0){
	    LCurOffset += 9;
	    code = code << 9;
	  
	    nb_bits = -n;
	  
	    n     = VVC1_dc_luma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][1];
	    value = VVC1_dc_luma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][0];
	  
	    if (n < 0){
	      LCurOffset += nb_bits;
	      code = code << nb_bits;
	    
	      nb_bits = -n;
	    
	      n     = VVC1_dc_luma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][1];
	      value = VVC1_dc_luma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][0];
	    }
	  }
	
	  LCurOffset += n;

	  if (value) {
	    if (value == 119) {
	
	      if (LCurOffset >= 32) {
		LCurOffset -= 32;
		LBitstrL = LBitstrR;
		VVC1_NTOHL(LBitstrR, *LCurBitstr)
		LCurBitstr++;
	      }
	      
	      VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)
	      
	      if (mquant == 1) {
		value = code >> (32-10);
		value = ((code >> (32-11)) & 0x1) ? -value : value;
		LCurOffset += 11;
	      } else if (mquant == 2) {
		value = code >> (32-9);
		value = ((code >> (32-10)) & 0x1) ? -value : value;
		LCurOffset += 10;
	      } else {
		value = code >> (32-8);
		value = ((code >> (32-9)) & 0x1) ? -value : value;
		LCurOffset += 9;
	      }
	    }
	    else
	      {
		code = code << n;

		if (mquant == 1) {
		  value = (value<<2) + (code >> (32-2)) - 3;
		  value = ((code >> (32-3)) & 0x1) ? -value : value;
		  LCurOffset += 3;
		} else if (mquant == 2) {
		  value = (value<<1) + (code >> (32-1)) - 1;
		  value = ((code >> (32-2)) & 0x1) ? -value : value;
		  LCurOffset += 2;
		} else {
		  value = ((code >> (32-1)) & 0x1) ? -value : value;
		  LCurOffset += 1;		  
		}
	      }
	  }
	}

      } else {
	unsigned code;

	coded = ((cbp >> (5 - i)) & 1);
	coding_set = VVC1_decoder.codingset2;

	{
	  unsigned nb_bits;
	
	  VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)
	
	  n     = VVC1_dc_chroma_vlc_2[VVC1_decoder.intratransformDCtable].table[(code>>23)][1];
	  value = VVC1_dc_chroma_vlc_2[VVC1_decoder.intratransformDCtable].table[(code>>23)][0];
	
	  if (n < 0){
	    LCurOffset += 9;
	    code = code << 9;
	  
	    nb_bits = -n;
	  
	    n     = VVC1_dc_chroma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][1];
	    value = VVC1_dc_chroma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][0];
	  
	    if (n < 0){
	      LCurOffset += nb_bits;
	      code = code << nb_bits;
	    
	      nb_bits = -n;
	    
	      n     = VVC1_dc_chroma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][1];
	      value = VVC1_dc_chroma_vlc_2[VVC1_decoder.intratransformDCtable].table[((code>>(32-nb_bits)) + value)][0];
	    }
	  }
		
	  LCurOffset += n;

	  if (value) {
	    if (value == 119) {

	      if (LCurOffset >= 32) {
		LCurOffset -= 32;
		LBitstrL = LBitstrR;
		VVC1_NTOHL(LBitstrR, *LCurBitstr)
		LCurBitstr++;
	      }

	      VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)

	      if (mquant == 1) {
		value = code >> (32-10);
		value = ((code >> (32-11)) & 0x1) ? -value : value;
		LCurOffset += 11;
	      } else if (mquant == 2) {
		value = code >> (32-9);
		value = ((code >> (32-10)) & 0x1) ? -value : value;
		LCurOffset += 10;
	      } else {
		value = code >> (32-8);
		value = ((code >> (32-9)) & 0x1) ? -value : value;
		LCurOffset += 9;
	      }
	    } else {
	      code = code << n;

	      if (mquant == 1) {
		value = (value<<2) + (code >> (32-2)) - 3;
		value = ((code >> (32-3)) & 0x1) ? -value : value;
		LCurOffset += 3;
	      } else if (mquant == 2) {
		value = (value<<1) + (code >> (32-1)) - 1;
		value = ((code >> (32-2)) & 0x1) ? -value : value;
		LCurOffset += 2;
	      } else {
		value = ((code >> (32-1)) & 0x1) ? -value : value;
		LCurOffset += 1;		  
	      }
	    }
	  }
	}
      }

      dc_val = value;

      if (LCurOffset >= 32) {
	LCurOffset -= 32;
	LBitstrL = LBitstrR;
	VVC1_NTOHL(LBitstrR, *LCurBitstr)
	LCurBitstr++;
      }

      pos = 0;

      if (coded) {
	const unsigned ac_sizes = VVC1_ac_sizes[coding_set] - 1;
	const unsigned last_decode = VVC1_last_decode_table[coding_set];

	{
	  register unsigned code;
	  int escape;
	    
	  int coeff = 1;
	  short * block = &block_ptr[0];
	  const unsigned char *scan = (unsigned char *)&VVC1_simple_progressive_8x8_zz_transp[0];
	  unsigned last = 0;
	  int run = 0;
	  int level = 0;
	  unsigned scan_coeff;

	  while (!last) {
	    {
	      unsigned nb_bits;

	      VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)

	      n     = VVC1_ac_coeff_table_2[coding_set].table[(code>>23)][1];
	      value = VVC1_ac_coeff_table_2[coding_set].table[(code>>23)][0];

	      if (n < 0){
		LCurOffset += 9;
		code = code << 9;
	  
		nb_bits = -n;
		
		n     = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][1];
		value = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][0];
		
		if (n < 0){
		  LCurOffset += nb_bits;
		  code = code << nb_bits;

		  nb_bits = -n;
		  
		  n     = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][1];
		  value = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][0];
		}
	      }

	    }

	    if (value != ac_sizes) {
	      coeff += VVC1_index_decode_table[coding_set][value][0];
	      level = VVC1_index_decode_table[coding_set][value][1];
	      last = value >= last_decode;

	      LCurOffset += n+1;
	      if (coeff > 63) 
		break;
	      scan_coeff = scan[coeff];
	      coeff++;
	      block[scan_coeff] = ((code >> (32-n-1)) & 0x1) ? -level : level;

	      if (LCurOffset >= 32) {
		LCurOffset -= 32;
		LBitstrL = LBitstrR;
		VVC1_NTOHL(LBitstrR, *LCurBitstr)
		LCurBitstr++;
	      }

	    } else {

	      if ((code >> (32-n-1)) & 0x1) {
		LCurOffset += n+1;
		escape = 0;
	      } else {
		LCurOffset += n+2;
		escape = 2 - ((code >> (32-n-2)) & 0x1);
	      }

	      if (LCurOffset >= 32) {
		LCurOffset -= 32;
		LBitstrL = LBitstrR;
		VVC1_NTOHL(LBitstrR, *LCurBitstr)
		LCurBitstr++;
	      }

	      if (escape != 2) {
		{
		  unsigned nb_bits;
		      
		  VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)
		      
		  n     = VVC1_ac_coeff_table_2[coding_set].table[(code>>23)][1];
		  value = VVC1_ac_coeff_table_2[coding_set].table[(code>>23)][0];

		  if (n < 0){
		    LCurOffset += 9;
		    code = code << 9;
			
		    nb_bits = -n;
			
		    n     = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][1];
		    value = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][0];
			
		    if (n < 0){
		      LCurOffset += nb_bits;
		      code = code << nb_bits;
			  
		      nb_bits = -n;
			  
		      n     = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][1];
		      value = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][0];
		    }
		  }
		      
		  LCurOffset += n;
		}

		run = VVC1_index_decode_table[coding_set][value][0];
		level = VVC1_index_decode_table[coding_set][value][1];
		last = value >= last_decode;

		if(escape == 0) {
		  if(last)
		    level += VVC1_last_delta_level_table[coding_set][run];
		  else
		    level += VVC1_delta_level_table[coding_set][run];
		} else {
		  if(last)
		    run += VVC1_last_delta_run_table[coding_set][level] + 1;
		  else
		    run += VVC1_delta_run_table[coding_set][level] + 1;
		}
		LCurOffset += 1;
		coeff += run;
		if (coeff > 63) 
		  break;
		scan_coeff = scan[coeff];
		coeff++;
		block[scan_coeff] = ((code >> (32-n-1)) & 0x1) ? -level : level;

		if (LCurOffset >= 32) {
		  LCurOffset -= 32;
		  LBitstrL = LBitstrR;
		  VVC1_NTOHL(LBitstrR, *LCurBitstr)
		  LCurBitstr++;
		}

	      } else {
		int sign;

		VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)

		last = (code >> (32-1));
		code = code << 1;
		    
		if(VVC1_decoder.esc3_level_length == 0) {

		  if(VVC1_decoder.pquant < 8 || VVC1_decoder.dquantframe) {
		    VVC1_decoder.esc3_level_length = Block_Escape_Mode_3_PQUANT_1_To_7[(code >> (32-5))].value;
		    n = Block_Escape_Mode_3_PQUANT_1_To_7[(code >> (32-5))].length;
		    LCurOffset += n;

		    VVC1_decoder.esc3_run_length = 3 + ((code << n) >> (32-2));
			
		    code = code << (n+2);

		    coeff += (code >> (31-VVC1_decoder.esc3_run_length))>>1;
		    sign = (code >> (32-VVC1_decoder.esc3_run_length-1)) & 0x1;
		    level = (((code << (VVC1_decoder.esc3_run_length+1)) >> (31-VVC1_decoder.esc3_level_length))>>1);
		    LCurOffset += 1 + 2 + VVC1_decoder.esc3_run_length + VVC1_decoder.esc3_level_length + 1;

		    if (coeff > 63) 
		      break;
		    scan_coeff = scan[coeff];
		    coeff++;
		    block[scan_coeff] = sign ? -level : level;

		    if (LCurOffset >= 32) {
		      LCurOffset -= 32;
		      LBitstrL = LBitstrR;
		      VVC1_NTOHL(LBitstrR, *LCurBitstr)
		      LCurBitstr++;
		    }
		      
		  } else {
		    VVC1_decoder.esc3_level_length = Block_Escape_Mode_3_PQUANT_8_To_31[(code >> (32-6))].value;
		    n = Block_Escape_Mode_3_PQUANT_8_To_31[(code >> (32-6))].length;
		    LCurOffset += n;

		    VVC1_decoder.esc3_run_length = 3 + ((code << n) >> (32-2));

		    code = code << (n+2);

		    coeff += (code >> (31-VVC1_decoder.esc3_run_length))>>1;
		    sign = (code >> (32-VVC1_decoder.esc3_run_length-1)) & 0x1;
		    level = (((code << (VVC1_decoder.esc3_run_length+1)) >> (31-VVC1_decoder.esc3_level_length))>>1);

		    LCurOffset += 1 + 2 + VVC1_decoder.esc3_run_length + VVC1_decoder.esc3_level_length + 1;

		    if (coeff > 63) 
		      break;
		    scan_coeff = scan[coeff];
		    coeff++;
		    block[scan_coeff] = sign ? -level : level;

		    if (LCurOffset >= 32) {
		      LCurOffset -= 32;
		      LBitstrL = LBitstrR;
		      VVC1_NTOHL(LBitstrR, *LCurBitstr)
		      LCurBitstr++;
		    }		      
		  }

		} else {		      
		  coeff += (code >> (31-VVC1_decoder.esc3_run_length))>>1;
		  sign = (code >> (32-VVC1_decoder.esc3_run_length-1)) & 0x1;
		  level = (((code << (VVC1_decoder.esc3_run_length+1)) >> (31-VVC1_decoder.esc3_level_length))>>1);
		  LCurOffset += 1 + VVC1_decoder.esc3_run_length + VVC1_decoder.esc3_level_length + 1;

		  if (coeff > 63) 
		    break;
		  scan_coeff = scan[coeff];
		  coeff++;
		  block[scan_coeff] = sign ? -level : level;

		  if (LCurOffset >= 32) {
		    LCurOffset -= 32;
		    LBitstrL = LBitstrR;
		    VVC1_NTOHL(LBitstrR, *LCurBitstr)
		    LCurBitstr++;
		  }
		}
	      }	      
	    }
	  }

	  pos = coeff - 1;
	}
      }

      dc_val += DCpredictor;

      VVC1_predict_current.dc_value[i] = dc_val;

      block_ptr[0] = dc_val * dcscale;

      blk_compl = Fl;

      tr_complexity = tr_complexity | (blk_compl << 4*i);

      position = position | (8 << 4*i);

      tr_type = tr_type | (T_8x8 << 4*i);

   
    } else {

      coded = (cbp >> (5 - i)) & 1;

      if (coded) {

	int subblkpat = 0;
	int ttblk = ttmb & 7;

	const unsigned char *scan = (unsigned char *)&VVC1_simple_progressive_8x8_zz_transp[0];
	int loop = 1;
	int max_pos = 63;
	int step = 64;
	int subblkpat_left = 0;

	if (ttmb == -1) {
	  unsigned code;
	  VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)
	    ttblk  = VVC1_ttblk_vlc[VVC1_decoder.transformtypeindex][(code>>27)][0];
	  LCurOffset += VVC1_ttblk_vlc[VVC1_decoder.transformtypeindex][(code>>27)][1];
	  ttblk = ttblk_to_tt[VVC1_decoder.transformtypeindex][ttblk];	

	  if (LCurOffset >= 32) {
	    LCurOffset -= 32;
	    LBitstrL = LBitstrR;
	    VVC1_NTOHL(LBitstrR, *LCurBitstr)
	      LCurBitstr++;
	  }
	}

	if (ttblk == TT_4X4) {
	  unsigned code;
	  VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)
	    subblkpat  = VVC1_subblkpat_vlc[VVC1_decoder.transformtypeindex][(code>>26)][0];
	  LCurOffset += VVC1_subblkpat_vlc[VVC1_decoder.transformtypeindex][(code>>26)][1];
	  subblkpat = ~(subblkpat + 1);

	  scan = (unsigned char *)&VVC1_simple_progressive_4x4_zz_transp[0];
	  loop = 4;
	  max_pos = 15;
	  step = 16;
	  subblkpat_left = subblkpat << 28;

	} else if ((ttblk != TT_8X8 && ttblk != TT_4X4) && 
		   (VVC1_decoder.mbtransformflag || ((ttmb != -1) && (ttmb & 8) && !firstblock))) {
	  if (!((LBitstrL>>(31-LCurOffset)) & 1)) {
	    subblkpat = 0;
	    LCurOffset++;
	  } else {
	    unsigned code;
	    VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)
	      code = (code >> 30) & 1;
	    subblkpat = 2 - code;
	    LCurOffset += 2;
	  }

	  if(ttblk == TT_8X4_TOP || ttblk == TT_8X4_BOTTOM) {
	    ttblk = TT_8X4;
	  } 
	  if(ttblk == TT_4X8_RIGHT || ttblk == TT_4X8_LEFT) {
	    ttblk = TT_4X8;
	  }
	}

	if (LCurOffset >= 32) {
	  LCurOffset -= 32;
	  LBitstrL = LBitstrR;
	  VVC1_NTOHL(LBitstrR, *LCurBitstr)
	    LCurBitstr++;
	}

	if(ttblk == TT_8X4_TOP || ttblk == TT_8X4_BOTTOM) {
	  subblkpat = 2 - (ttblk == TT_8X4_TOP);
	  ttblk = TT_8X4;
	} 
	if(ttblk == TT_4X8_RIGHT || ttblk == TT_4X8_LEFT) {
	  subblkpat = 2 - (ttblk == TT_4X8_LEFT);
	  ttblk = TT_4X8;
	}

	if (ttblk == TT_8X4) {
	  scan = (unsigned char *)&VVC1_simple_progressive_8x4_zz_transp[0];
	  loop = 2;
	  max_pos = 31;
	  step = 32;
	  subblkpat_left = subblkpat << 30;
	} else if (ttblk == TT_4X8) {
	  scan = (unsigned char *)&VVC1_simple_progressive_4x8_zz_transp[0];
	  loop = 2;
	  max_pos = 31;
	  step = 32;
	  subblkpat_left = subblkpat << 30;
	}

	tr_type = tr_type | ((((ttblk & 0x4)>>1) | (ttblk & 0x1)) << 4*i);

	if (VVC1_decoder.loopfilter) {
	  lf_top_borders =  lf_top_borders | 0xffffff;
	  lf_left_borders = lf_left_borders | 0xffffff;
	}

	coding_set = VVC1_decoder.codingset2;

	{
	  const unsigned ac_sizes = VVC1_ac_sizes[coding_set] - 1;
	  const unsigned last_decode = VVC1_last_decode_table[coding_set];

	  {
	    register unsigned code;
	    int escape;
	    
	    int coeff = 0;
	    short * block = &block_ptr[0];
	    unsigned last = 0;
	    int run = 0;
	    int level = 0;
	    unsigned scan_coeff;
	  
	    int j;

	    subblk_compl = No;

	    for (j=0; j<loop; j++) {

	      coeff = j * step;
	      max_pos = coeff + step - 1;

	      last = (subblkpat_left >> (31-j)) & 1;

	      subblk_compl |= ((!last) << j);

	      while (!last) {
		{
		  unsigned nb_bits;

		  VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)

		    n     = VVC1_ac_coeff_table_2[coding_set].table[(code>>23)][1];
		  value = VVC1_ac_coeff_table_2[coding_set].table[(code>>23)][0];

		  if (n < 0){
		    LCurOffset += 9;
		    code = code << 9;
	  
		    nb_bits = -n;
		
		    n     = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][1];
		    value = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][0];
		
		    if (n < 0){
		      LCurOffset += nb_bits;
		      code = code << nb_bits;

		      nb_bits = -n;
		  
		      n     = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][1];
		      value = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][0];
		    }
		  }
		}

		if (value != ac_sizes) {
		  coeff += VVC1_index_decode_table[coding_set][value][0];
		  level = VVC1_index_decode_table[coding_set][value][1];
		  last = value >= last_decode;

		  LCurOffset += n+1;
		  if (coeff > max_pos) 
		    break;
		  scan_coeff = scan[coeff];
		  coeff++;
		  if (scan_coeff > pos) {
		    pos = scan_coeff;
		  }
		  block[scan_coeff] = ((code >> (32-n-1)) & 0x1) ? -level : level;

		  if (LCurOffset >= 32) {
		    LCurOffset -= 32;
		    LBitstrL = LBitstrR;
		    VVC1_NTOHL(LBitstrR, *LCurBitstr)
		      LCurBitstr++;
		  }

		} else {
		  if ((code >> (32-n-1)) & 0x1) {
		    LCurOffset += n+1;
		    escape = 0;
		  } else {
		    LCurOffset += n+2;
		    escape = 2 - ((code >> (32-n-2)) & 0x1);
		  }

		  if (LCurOffset >= 32) {
		    LCurOffset -= 32;
		    LBitstrL = LBitstrR;
		    VVC1_NTOHL(LBitstrR, *LCurBitstr)
		      LCurBitstr++;
		  }

		  if (escape != 2) {
		    {
		      unsigned nb_bits;
		      
		      VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)
		      
			n     = VVC1_ac_coeff_table_2[coding_set].table[(code>>23)][1];
		      value = VVC1_ac_coeff_table_2[coding_set].table[(code>>23)][0];

		      if (n < 0){
			LCurOffset += 9;
			code = code << 9;
			
			nb_bits = -n;
			
			n     = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][1];
			value = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][0];
			
			if (n < 0){
			  LCurOffset += nb_bits;
			  code = code << nb_bits;
			  
			  nb_bits = -n;
			  
			  n     = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][1];
			  value = VVC1_ac_coeff_table_2[coding_set].table[((code>>(32-nb_bits)) + value)][0];
			}
		      }
		      
		      LCurOffset += n;
		    }

		    run = VVC1_index_decode_table[coding_set][value][0];
		    level = VVC1_index_decode_table[coding_set][value][1];
		    last = value >= last_decode;

		    if(escape == 0) {
		      if(last)
			level += VVC1_last_delta_level_table[coding_set][run];
		      else
			level += VVC1_delta_level_table[coding_set][run];
		    } else {
		      if(last)
			run += VVC1_last_delta_run_table[coding_set][level] + 1;
		      else
			run += VVC1_delta_run_table[coding_set][level] + 1;
		    }
		    LCurOffset += 1;
		    coeff += run;
		    if (coeff > 63) 
		      break;
		    scan_coeff = scan[coeff];
		    coeff++;
		    if (scan_coeff > pos) {
		      pos = scan_coeff;
		    }
		    block[scan_coeff] = ((code >> (32-n-1)) & 0x1) ? -level : level;

		    if (LCurOffset >= 32) {
		      LCurOffset -= 32;
		      LBitstrL = LBitstrR;
		      VVC1_NTOHL(LBitstrR, *LCurBitstr)
			LCurBitstr++;
		    }

		  } else {
		    int sign;

		    VVC1_LOOK_AHEAD(code, LBitstrL, LBitstrR, LCurOffset)

		      last = (code >> (32-1));
		    code = code << 1;
		    
		    if(VVC1_decoder.esc3_level_length == 0) {

		      if(VVC1_decoder.pquant < 8 || VVC1_decoder.dquantframe) {
			VVC1_decoder.esc3_level_length = Block_Escape_Mode_3_PQUANT_1_To_7[(code >> (32-5))].value;
			n = Block_Escape_Mode_3_PQUANT_1_To_7[(code >> (32-5))].length;
			LCurOffset += n;

			VVC1_decoder.esc3_run_length = 3 + ((code << n) >> (32-2));
			
			code = code << (n+2);

			coeff += (code >> (31-VVC1_decoder.esc3_run_length))>>1;
			sign = (code >> (32-VVC1_decoder.esc3_run_length-1)) & 0x1;
			level = (((code << (VVC1_decoder.esc3_run_length+1)) >> (31-VVC1_decoder.esc3_level_length))>>1);
			LCurOffset += 1 + 2 + VVC1_decoder.esc3_run_length + VVC1_decoder.esc3_level_length + 1;

			if (coeff > 63) 
			  break;
			scan_coeff = scan[coeff];
			coeff++;
			if (scan_coeff > pos) {
			  pos = scan_coeff;
			}
			block[scan_coeff] = sign ? -level : level;

			if (LCurOffset >= 32) {
			  LCurOffset -= 32;
			  LBitstrL = LBitstrR;
			  VVC1_NTOHL(LBitstrR, *LCurBitstr)
			    LCurBitstr++;
			}
		      
		      } else {
			VVC1_decoder.esc3_level_length = Block_Escape_Mode_3_PQUANT_8_To_31[(code >> (32-6))].value;
			n = Block_Escape_Mode_3_PQUANT_8_To_31[(code >> (32-6))].length;
			LCurOffset += n;

			VVC1_decoder.esc3_run_length = 3 + ((code << n) >> (32-2));

			code = code << (n+2);

			coeff += (code >> (31-VVC1_decoder.esc3_run_length))>>1;
			sign = (code >> (32-VVC1_decoder.esc3_run_length-1)) & 0x1;
			level = (((code << (VVC1_decoder.esc3_run_length+1)) >> (31-VVC1_decoder.esc3_level_length))>>1);

			LCurOffset += 1 + 2 + VVC1_decoder.esc3_run_length + VVC1_decoder.esc3_level_length + 1;

			if (coeff > 63) 
			  break;
			scan_coeff = scan[coeff];
			coeff++;
			if (scan_coeff > pos) {
			  pos = scan_coeff;
			}
			block[scan_coeff] = sign ? -level : level;

			if (LCurOffset >= 32) {
			  LCurOffset -= 32;
			  LBitstrL = LBitstrR;
			  VVC1_NTOHL(LBitstrR, *LCurBitstr)
			    LCurBitstr++;
			}
		      
		      }

		    } else {		      
		      coeff += (code >> (31-VVC1_decoder.esc3_run_length))>>1;
		      sign = (code >> (32-VVC1_decoder.esc3_run_length-1)) & 0x1;
		      level = (((code << (VVC1_decoder.esc3_run_length+1)) >> (31-VVC1_decoder.esc3_level_length))>>1);
		      LCurOffset += 1 + VVC1_decoder.esc3_run_length + VVC1_decoder.esc3_level_length + 1;

		      if (coeff > 63) 
			break;
		      scan_coeff = scan[coeff];
		      coeff++;
		      if (scan_coeff > pos) {
			pos = scan_coeff;
		      }
		      block[scan_coeff] = sign ? -level : level;

		      if (LCurOffset >= 32) {
			LCurOffset -= 32;
			LBitstrL = LBitstrR;
			VVC1_NTOHL(LBitstrR, *LCurBitstr)
			  LCurBitstr++;
		      }
		    }
		  }
		}
	      }

	      if (LCurOffset >= 32) {
		LCurOffset -= 32;
		LBitstrL = LBitstrR;
		VVC1_NTOHL(LBitstrR, *LCurBitstr)
		  LCurBitstr++;
	      }
	    }

	    if (pos > 63) {
	      pos = 63;
	    }
	  }

	}

	if (!VVC1_decoder.mbtransformflag && ttmb < 8)
	  ttmb = -1;

	firstblock = 0;
      
	tr_complexity = tr_complexity | (subblk_compl << 4*i);

	position = position | ((1+(pos>>3)) << 4*i);      
      }
    }  
  }

  VVC1_cs[VVC1_buf_num]->BLOCK_QP_MUL = (unsigned) (2*mquant + ((mquant == VVC1_decoder.pquant) ? VVC1_decoder.halfqpstep : 0));
  VVC1_cs[VVC1_buf_num]->BLOCK_QP_ADD = (VVC1_decoder.pquantizer == vc1_QuantizerUniform) ? 0 : (unsigned) mquant;
			
  VVC1_cs[VVC1_buf_num]->AC_PRED_DIR = acpred_dir;
  
  VVC1_cs[VVC1_buf_num]->AC_SCALE_TOP = (ac_scale_top == 0) ? 1<<18 : ac_scale_top;
  VVC1_cs[VVC1_buf_num]->AC_SCALE_LEFT = (ac_scale_left == 0) ? 1<<18 : ac_scale_left;

  VVC1_cs[VVC1_buf_num]->TR_COMPLEXITY = tr_complexity;
  VVC1_cs[VVC1_buf_num]->OVL_BORDERS = ovl_borders;
  VVC1_cs[VVC1_buf_num]->LF_FLAGS_TOP = lf_top_borders;
  VVC1_cs[VVC1_buf_num]->LF_FLAGS_LEFT = lf_left_borders;
  VVC1_cs[VVC1_buf_num]->TRANSFORM_TYPE = tr_type;
  VVC1_cs[VVC1_buf_num]->POS = position;

  VVC1_CurBitstr = LCurBitstr;
  VVC1_BitstrL = LBitstrL;
  VVC1_BitstrR = LBitstrR;
  VVC1_CurOffset = LCurOffset;  
}




/*lint -restore */
/* PRQA L:L1 */
