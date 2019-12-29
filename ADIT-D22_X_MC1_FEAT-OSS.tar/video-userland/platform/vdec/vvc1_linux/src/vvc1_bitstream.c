/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *                                                                          
 * DESCRIPTION:         bitstream parsing routines
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

#include "adit_typedef.h"

#ifndef LINUX
#include "vmp_lowlevelif.h"
#else
#include "vid_lowlevelif.h"
#endif

#include "vvc1_dec.h"
#include "vvc1_decoder.h"
#include "vvc1_buffer.h"
#include "vvc1_tables.h"
#include "vvc1_bitstream.h"
#include "vvc1_ctrl_struct.h"

/* PRQA: QAC Message 3201, 3334, 3757, 3760, 3768, 3771: deactivation because of pre-existing IP*/
/* PRQA S 3201, 3334, 3757, 3760, 3768, 3771 L1*/
/* PRQA: Lint Message 578:deactivation because of pre-existing IP */
/*lint -save -e578*/


static unsigned char decode_bitplane (unsigned char *, int);


int VVC1_read_sequence_header()
{
  unsigned int width, height;
  unsigned int value;
  int i;

  VVC1_decoder.seqheadersize = VVC1_get_bits(8) | (VVC1_get_bits(8) << 8) | (VVC1_get_bits(8) << 16) | (VVC1_get_bits(8) << 24);

  if (VVC1_decoder.seqheadersize < 44) {
    // incorrect header size - unsupported format
    return VVC1_VID_STATUS_ERROR_UNSUPPORTED;
  }

  width = VVC1_get_bits(8) | (VVC1_get_bits(8) << 8) | (VVC1_get_bits(8) << 16) | (VVC1_get_bits(8) << 24);
  height = VVC1_get_bits(8) | (VVC1_get_bits(8) << 8) | (VVC1_get_bits(8) << 16) | (VVC1_get_bits(8) << 24);

  for (i=0; i<7; i++)
    VVC1_get_bits(32);

  value = VVC1_get_bits(4);
  if ((value != 0) && (value != 4)) {
    /* not Simple nor Main Profile */
    return VVC1_VID_STATUS_ERROR_UNSUPPORTED;
  }
  value = VVC1_get_bits(3);
  value = VVC1_get_bits(5);
  value = VVC1_get_bits(1);
  VVC1_decoder.loopfilter = value;
  value = VVC1_get_bits(1);
  value = VVC1_get_bits(1);
  VVC1_decoder.multirescoding = value;
  value = VVC1_get_bits(1);
  VVC1_decoder.fastumvc = VVC1_get_bits(1);
  VVC1_decoder.extendedmv = VVC1_get_bits(1);
  VVC1_decoder.dquant = VVC1_get_bits(2);
  VVC1_decoder.vstransform = VVC1_get_bits(1);
  value = VVC1_get_bits(1);
  VVC1_decoder.overlappedtransform = VVC1_get_bits(1);
  VVC1_decoder.syncmarker = VVC1_get_bits(1);
  VVC1_decoder.rangereduction = VVC1_get_bits(1);
  VVC1_decoder.maxbframes = VVC1_get_bits(3);
  VVC1_decoder.quantizer = VVC1_get_bits(2);
  VVC1_decoder.frameinterpolation = VVC1_get_bits(1);
  VVC1_decoder.reserved = VVC1_get_bits(1);

  if (!VVC1_decoder.resize && (VVC1_decoder.width != width || VVC1_decoder.height != height))
    {
      VVC1_decoder.resize = 1;
      VVC1_decoder.disp_width = width;
      VVC1_decoder.disp_height = height;

      VVC1_decoder.width = ((width + 15) >> 4) << 4;
      VVC1_decoder.height = ((height + 15) >> 4) << 4;

      VVC1_decoder.mb_width = (VVC1_decoder.width + 15) >> 4;
      VVC1_decoder.mb_height = (VVC1_decoder.height + 15) >> 4;

      if ((VVC1_decoder.mb_width > MAX_MBX_X) || (VVC1_decoder.mb_height > MAX_MBX_Y) || (VVC1_decoder.mb_width*VVC1_decoder.mb_height > MAX_MBS)) {
	/* max dimension exceeded */
	return VVC1_VID_STATUS_ERROR_UNSUPPORTED;
      }

#ifdef _INDIRECT
      {
	VID_IF_CONFIG ifConfig;

	ifConfig.buffergroup_size = VVC1_decoder.mb_width*VVC1_decoder.mb_height;
	    
	if (ifConfig.buffergroup_size > BUFFERGROUP_SIZE) {
	  return VVC1_VID_STATUS_ERROR_INIT;
	}
#ifndef LINUX
	VVC1_vid_setup_dmasync(&VVC1_decoder.VMP_TEST_dmaFlag);
#endif
	ifConfig.memoryid = CORE_USED;
	ifConfig.buffer_size = BUFFERSIZE_BYTE;	   
	ifConfig.buffergroup_nb = BUFFERGROUP_NB;
	ifConfig.zerobuffer = 0;
	ifConfig.icmslots_nb = ICM_SLOTS_NB;
	ifConfig.icmslots_offset = ICM_SLOTS_OFFSET_BYTE;
#ifndef LINUX
	ifConfig.dmaMsgBox = VVC1_decoder.VMP_TEST_dmaFlag;
#endif	    
	if (vid_set_if_config(&ifConfig) != VMP_OK) {
	  return VVC1_VID_STATUS_ERROR_INIT;
	}
      }
#endif

      if (VVC1_init_cs() != VMP_OK) {
	return VVC1_VID_STATUS_ERROR_INIT;
      }

      VVC1_cs[0]->MBX = VVC1_decoder.mb_width;
      VVC1_cs[0]->MBY = VVC1_decoder.mb_height;

      {
	int i;

	if ((*VVC1_decoder.fnptr)(NR_OF_VID_BUFFERS, 
				    (SVGUint16)VVC1_decoder.width, 
				    (SVGUint16)VVC1_decoder.height, 
				    SVG_COLOR_YUV422, 
				    VVC1_fbmem_frame_buf) < NR_OF_VID_BUFFERS) {
	  return VVC1_VID_STATUS_ERROR_MEMALLOC;
	}
	VVC1_cs[0]->DEC_BUF = ((unsigned int)VVC1_fbmem_frame_buf[0].phy_addr)>>3;
		
	if ((*VVC1_decoder.fnptr)(NR_OF_VID_BUFFERS, 
				    (SVGUint16)VVC1_decoder.width, 
				    (SVGUint16)VVC1_decoder.height, 
				    SVG_COLOR_YUV422, 
				    VVC1_fbmem_disp_buf) < NR_OF_VID_BUFFERS) {
	  return VVC1_VID_STATUS_ERROR_MEMALLOC;
	}
	VVC1_cs[0]->DISP_BUF = ((unsigned int)VVC1_fbmem_disp_buf[0].phy_addr)>>3;
	VVC1_cs[0]->YUV422_PAD = (VVC1_fbmem_disp_buf[0].strideSize>>1) - VVC1_decoder.width;

	for (i=0; i<NR_OF_VID_BUFFERS; i++)
	  VVC1_fbmem_buf_used[i] = 0;
      }
	      
    }

  return VVC1_VID_STATUS_OK;
}







int VVC1_read_headers()
{
  unsigned int coding_type;
  unsigned int value;

  if (VVC1_decoder.frameinterpolation) {
    VVC1_decoder.frameinterpolationhint = VVC1_get_bits(1);
  } else {
    VVC1_decoder.frameinterpolationhint = 0;
  }

  value = VVC1_get_bits(2);

  if (VVC1_decoder.rangereduction) {
    value = VVC1_get_bits(1);
    VVC1_decoder.rangeYscale = 8 + 8 * value;
    VVC1_decoder.rangeUVscale = 8 + 8 * value;
  } else {
    VVC1_decoder.rangeYscale = 8;
    VVC1_decoder.rangeUVscale = 8;
  }

  value = VVC1_get_bits(1);

  if (value == 1) {
    coding_type = P_VOP;
  } else {
    if (!VVC1_decoder.maxbframes) {
      coding_type = I_VOP;
    } else {
      value = VVC1_get_bits(1);
      if (value == 0) {
	coding_type = B_VOP;
      } else {
	coding_type = I_VOP;
      }
    }
  }

  if (coding_type == B_VOP) {
    if (VVC1_show_bits(3) < 7) {
      value = VVC1_get_bits(3);
    } else {
      value = 7 + (VVC1_get_bits(7) >> 3);
    }
    VVC1_decoder.bfraction = pBFraction[value].ScaleFactor;

    if (pBFraction[value].Denominator == 0) {
      if (pBFraction[value].Numerator == 2) {
	coding_type = BI_VOP;
      }
    }
  }  

  VVC1_decoder.picturetype = coding_type;

 
  if ((coding_type == I_VOP) || (coding_type == BI_VOP)) {
    VVC1_decoder.bufferfullness = VVC1_get_bits(7);
  }

  VVC1_decoder.pqindex = VVC1_get_bits(5);

  if (VVC1_decoder.quantizer != vc1_QuantizerExplicit) {
    int PQUANT = VVC1_decoder.pqindex;
    int NonUniform = 0;
    
    switch (VVC1_decoder.quantizer) {
    case vc1_QuantizerImplicit:
      if (VVC1_decoder.pqindex >= 9) {
	PQUANT = NonUniformImplicit[VVC1_decoder.pqindex];
	NonUniform = 1;
      }
      break;
    case vc1_QuantizerNonUniform:
      NonUniform = 1;
      break;
    default:
      break;
    }
          
    VVC1_decoder.pquant = PQUANT;
    if (NonUniform) {
      VVC1_decoder.pquantizer = vc1_QuantizerNonUniform;
    } else {
      VVC1_decoder.pquantizer = vc1_QuantizerUniform;
    }
  }

  VVC1_decoder.halfqpstep = 0;
  if (VVC1_decoder.pqindex <= 8) {
    VVC1_decoder.halfqpstep = VVC1_get_bits(1);
  }

  if (VVC1_decoder.quantizer == vc1_QuantizerExplicit) {
    value = VVC1_get_bits(1);

    if (value == 0) {
      VVC1_decoder.pquant = VVC1_decoder.pqindex;
      VVC1_decoder.pquantizer = vc1_QuantizerNonUniform;
    } else {
      VVC1_decoder.pquant = VVC1_decoder.pqindex;
      VVC1_decoder.pquantizer = vc1_QuantizerUniform;
    }

  }

  if (VVC1_decoder.extendedmv) {
    VVC1_decoder.mvrange = Motion_Vector_Range_Table[VVC1_show_bits(3)].value;
    VVC1_flush_bits(Motion_Vector_Range_Table[VVC1_show_bits(3)].length);
  } else {
    VVC1_decoder.mvrange = vc1_MVRange64_32;
  }

  if ((coding_type == I_VOP) || (coding_type == P_VOP)) {
    if (VVC1_decoder.multirescoding) {
      VVC1_decoder.pictureres = VVC1_get_bits(2);
    } else {
      VVC1_decoder.pictureres = 0;
    }
  }

  if (coding_type == P_VOP) {
    if (VVC1_decoder.pquant > 12) {
      value = P_Picture_Low_Rate_Motion_Vector_Mode[VVC1_show_bits(4)].value;
      VVC1_flush_bits(P_Picture_Low_Rate_Motion_Vector_Mode[VVC1_show_bits(4)].length);
    } else {
      value = P_Picture_High_Rate_Motion_Vector_Mode[VVC1_show_bits(4)].value;
      VVC1_flush_bits(P_Picture_High_Rate_Motion_Vector_Mode[VVC1_show_bits(4)].length);
    }

    if (value == vc1_MVModeIntensityCompensation) {
      VVC1_decoder.intensitycompflag = 1;
      if (VVC1_decoder.pquant > 12) {
	value = P_Picture_Low_Rate_Motion_Vector_Mode_2[VVC1_show_bits(3)].value;
	VVC1_flush_bits(P_Picture_Low_Rate_Motion_Vector_Mode_2[VVC1_show_bits(3)].length);
      } else {
	value = P_Picture_High_Rate_Motion_Vector_Mode_2[VVC1_show_bits(3)].value;
	VVC1_flush_bits(P_Picture_High_Rate_Motion_Vector_Mode_2[VVC1_show_bits(3)].length);
      }
      VVC1_decoder.mvmode = value;
      VVC1_decoder.luminancescale = VVC1_get_bits(6);
      VVC1_decoder.luminanceshift = VVC1_get_bits(6);	

    } else {
      VVC1_decoder.mvmode = value;
      VVC1_decoder.intensitycompflag = 0;
    }

    if (VVC1_decoder.mvmode == vc1_MVMode1MVHalfPel || VVC1_decoder.mvmode == vc1_MVMode1MVHalfPelBilinear) {
      VVC1_decoder.quarterpel = 0;
    } else {
      VVC1_decoder.quarterpel = 1;
    }

  } else if (coding_type == B_VOP) {
    value = VVC1_get_bits(1);

    if (value == 0) {
      VVC1_decoder.mvmode = vc1_MVMode1MVHalfPelBilinear;
    } else {
      VVC1_decoder.mvmode = vc1_MVMode1MV;
    }

    VVC1_decoder.quarterpel = (VVC1_decoder.mvmode == vc1_MVMode1MV);
  }

  if ((coding_type == B_VOP) || (coding_type == P_VOP)) {

    if (VVC1_decoder.pquant > 12) {
      VVC1_decoder.transformtypeindex = 2;
    } else if (VVC1_decoder.pquant > 4) {
      VVC1_decoder.transformtypeindex = 1;
    } else {
      VVC1_decoder.transformtypeindex = 0;
    }

    if (VVC1_decoder.mvmode == vc1_MVModeMixedMV) {
      int invert = VVC1_get_bits(1);

      VVC1_decoder.motionvectortype_rawmode = decode_bitplane(VVC1_decoder.motionvectortype, invert);
    } else {
      int i;

      VVC1_decoder.motionvectortype_rawmode = 0;
      for (i = 0; i < VVC1_decoder.mb_width*VVC1_decoder.mb_height; i++) {
	VVC1_decoder.motionvectortype[i] = 0;
      }
    }
  }


  if (coding_type == B_VOP) {
    int invert = VVC1_get_bits(1);

    VVC1_decoder.bframedirectmode_rawmode = decode_bitplane(VVC1_decoder.bframedirectmode, invert);
  }


  VVC1_decoder.dquantframe = 0;

  if ((coding_type == B_VOP) || (coding_type == P_VOP)) {
    int invert = VVC1_get_bits(1);

    VVC1_decoder.skipmb_rawmode = decode_bitplane(VVC1_decoder.skipmb, invert);

    value = VVC1_get_bits(2);
    VVC1_decoder.mvtab = value;

    value = VVC1_get_bits(2);
    VVC1_decoder.cbptab = value;

    if (VVC1_decoder.dquant == 1 || VVC1_decoder.dquant == 3) {
      VVC1_decoder.dquantframe = VVC1_get_bits(1);
      if (VVC1_decoder.dquantframe == 1) {
	value = VVC1_get_bits(2);

	if (value == 0) {
	  VVC1_decoder.quantmode = vc1_QuantModeAllEdges;

	} else if (value == 1) {
	  value = VVC1_get_bits(2);
	  VVC1_decoder.quantmode = (vc1_QuantModeLeftTop << value) % 15;

	} else if (value == 2) {
	  value = VVC1_get_bits(2);
	  VVC1_decoder.quantmode = vc1_QuantModeLeft << value;

	} else if (value == 3) {
	  value = VVC1_get_bits(1);
	  if (value == 0) {
	    VVC1_decoder.quantmode = vc1_QuantModeMBAny;
	  } else {
	    VVC1_decoder.quantmode = vc1_QuantModeMBDual;
	  }
	}
      } else {
	VVC1_decoder.quantmode = vc1_QuantModeDefault;
      }
    } else if (VVC1_decoder.dquant == 2) {
      VVC1_decoder.quantmode = vc1_QuantModeAllEdges;
    }

    if ((VVC1_decoder.dquant == 1) || (VVC1_decoder.dquant == 2) || (VVC1_decoder.dquant == 3)) {
      if ((VVC1_decoder.quantmode != vc1_QuantModeDefault) && (VVC1_decoder.quantmode != vc1_QuantModeMBAny)) {
	value = VVC1_get_bits(3);

	if (value != 7) {
	  VVC1_decoder.altpquant = VVC1_decoder.pquant + value + 1;
	} else {
	  VVC1_decoder.altpquant = VVC1_get_bits(5);
	}
      }

    } else {
      VVC1_decoder.altpquant = 0;
      VVC1_decoder.quantmode = vc1_QuantModeDefault;
    }

    VVC1_decoder.frametransformtype = TT_8X8; 

    if (VVC1_decoder.vstransform) {
      VVC1_decoder.mbtransformflag = VVC1_get_bits(1);
      if (VVC1_decoder.mbtransformflag) {
	VVC1_decoder.frametransformtype = VVC1_ttfrm_to_tt[VVC1_get_bits(2)];
      }
    } else {
      VVC1_decoder.mbtransformflag = 1;
      VVC1_decoder.frametransformtype = TT_8X8;
    }

  } else {
    VVC1_decoder.quantmode = vc1_QuantModeDefault;
  }

  value = VVC1_get_bits(1);
  if (value == 0) {
    VVC1_decoder.frameACcodingindex = 0;
  } else {
    value = VVC1_get_bits(1);
    if (value == 0) {
      VVC1_decoder.frameACcodingindex = 1;
    } else {
      VVC1_decoder.frameACcodingindex = 2;
    }
  }

  if ((coding_type == I_VOP) || (coding_type == BI_VOP)) {
    value = VVC1_get_bits(1);
    if (value == 0) {
      VVC1_decoder.frameACcodingindex2 = 0;
    } else {
      value = VVC1_get_bits(1);
      if (value == 0) {
	VVC1_decoder.frameACcodingindex2 = 1;
      } else {
	VVC1_decoder.frameACcodingindex2 = 2;
      }
    }
  }

  VVC1_decoder.intratransformDCtable = VVC1_get_bits(1);

  VVC1_decoder.conditionaloverlap = vc1_CondOverAll;

  if ((coding_type == B_VOP) || (VVC1_decoder.pquant < 9) || !VVC1_decoder.overlappedtransform) {
    VVC1_decoder.conditionaloverlap = vc1_CondOverNone;
  }

  if ((coding_type == I_VOP) || (coding_type == BI_VOP)) {
    VVC1_decoder.rndctrl = 1;
  } else if (coding_type == P_VOP) {
    VVC1_decoder.rndctrl = ~VVC1_decoder.rndctrl;
  }

  return coding_type;
}







static unsigned char decode_bitplane (unsigned char *bitplane, int invert)
{
  int value;
  unsigned char rawmode;
  int mb_width = VVC1_decoder.mb_width;
  int mb_height = VVC1_decoder.mb_height;

  value = IMODE_Table[VVC1_show_bits(4)].value;
  VVC1_flush_bits(IMODE_Table[VVC1_show_bits(4)].length);

  rawmode = 0;

  switch (value) {
    
  case vc1_BitplaneCodingModeNorm2:
    {
      int count = mb_width * mb_height;
      int i, offset = 0;
      
      if (count & 1) {
	bitplane[0] = VVC1_get_bits(1) ^ invert;
	count --;
	offset = 1;
      }
      for (i = 0; i < count/2; i++) {
	value = Norm2_Table[VVC1_show_bits(3)].value;
	VVC1_flush_bits(Norm2_Table[VVC1_show_bits(3)].length);
	bitplane[2*i + offset] = (value >> 1) ^ invert;
	bitplane[2*i + 1 + offset] = (value & 1) ^ invert;
      }
    }
    break;

  case vc1_BitplaneCodingModeNorm6:
    {
      int i, j, k, width_in_tiles, height_in_tiles;
      int residualx, residualy;
      unsigned char *pBit;
	
      if (((mb_height % 3) == 0) && ((mb_width % 3) != 0)) {
	width_in_tiles = mb_width / 2;
	height_in_tiles = mb_height / 3;
	  
	for (j = 0; j < height_in_tiles; j++) {
	  pBit = &bitplane[j * 3 * mb_width];
	  pBit += mb_width & 1;
	    
	  for (i = 0; i < width_in_tiles; i++) {
	    {
	      unsigned nb_bits;
	      int n;
		
	      n     = VVC1_norm6_vlc_table.table[VVC1_show_bits(9)][1];
	      value = VVC1_norm6_vlc_table.table[VVC1_show_bits(9)][0];
		
	      if (n < 0){
		VVC1_flush_bits(9);
		  
		nb_bits = -n;
		  
		n     = VVC1_norm6_vlc_table.table[(VVC1_show_bits(nb_bits) + value)][1];
		value = VVC1_norm6_vlc_table.table[(VVC1_show_bits(nb_bits) + value)][0];
	      }
	      VVC1_flush_bits(n);
	    }

	    for (k = 0; k < 6; k++) {
	      if ((k == 2) || (k == 4)) {
		pBit += mb_width;
		pBit -= 2;
	      }
	      *pBit++ = ((value & (1 << k)) != 0) ? (1 - invert) : invert;
	    }
	    pBit -= (2 * mb_width);
	  }
	}
	residualx = mb_width & 1;
	residualy = 0;
	  
      } else {
	width_in_tiles = mb_width / 3;
	height_in_tiles = mb_height / 2;

	for (j = 0; j < height_in_tiles; j++) {
	  pBit = &bitplane[j * 2 * mb_width];
	  pBit += mb_width % 3;
	  pBit += (mb_height & 1) * mb_width;

	  for (i = 0; i < width_in_tiles; i++) {
	    {
	      unsigned nb_bits;
	      int n;
		
	      n     = VVC1_norm6_vlc_table.table[VVC1_show_bits(9)][1];
	      value = VVC1_norm6_vlc_table.table[VVC1_show_bits(9)][0];
		
	      if (n < 0){
		VVC1_flush_bits(9);
		  
		nb_bits = -n;
		  
		n     = VVC1_norm6_vlc_table.table[(VVC1_show_bits(nb_bits) + value)][1];
		value = VVC1_norm6_vlc_table.table[(VVC1_show_bits(nb_bits) + value)][0];
	      }
	      VVC1_flush_bits(n);
	    }

	    for (k = 0; k < 6; k++) {
	      if (k == 3) {
		pBit += mb_width;
		pBit -= 3;
	      }
	      *pBit++ = ((value & (1 << k)) != 0) ? (1 - invert) : invert;
	    }
	    pBit -= mb_width;
	  }
	}
	residualx = mb_width % 3;
	residualy = mb_height & 1;
      }

      for (i = 0; i < residualx; i++) {
	int colskip = VVC1_get_bits(1);
	for (j = 0; j < mb_height; j++) {
	  value = 0;
	  if (colskip) {
	    value = VVC1_get_bits(1);
	  }
	  bitplane[i + mb_width * j] = value ^ invert;
	}
      }

      for (j = 0; j < residualy; j++) {
	int rowskip = VVC1_get_bits(1);
	for (i = residualx; i < mb_width; i++) {
	  value = 0;
	  if (rowskip) {
	    value = VVC1_get_bits(1);
	  }
	  bitplane[i] = value ^ invert;
	}
      }	    
    }
    break;

  case vc1_BitplaneCodingModeRowskip:
    {
      int i, j; 
      int rowskip;

      for (j = 0; j < mb_height; j++) {
	rowskip = VVC1_get_bits(1);
	for (i = 0; i < mb_width; i++) {
	  value = 0;
	  if (rowskip) {
	    value = VVC1_get_bits(1);
	  }
	  bitplane[i + mb_width * j] = value ^ invert;
	}
      }
    }
    break;

  case vc1_BitplaneCodingModeColskip:
    {
      int i, j; 
      int colskip;

      for (i = 0; i < mb_width; i++) {
	colskip = VVC1_get_bits(1);
	for (j = 0; j < mb_height; j++) {
	  value = 0;
	  if (colskip) {
	    value = VVC1_get_bits(1);
	  }
	  bitplane[i + mb_width * j] = value ^ invert;
	}
      }
    }
    break;

  case vc1_BitplaneCodingModeDiff2:
    {
      int count = mb_width * mb_height;
      int i, offset = 0;

      if (count & 1) {
	bitplane[0] = VVC1_get_bits(1);
	count --;
	offset = 1;
      }
      for (i = 0; i < count/2; i++) {
	value = Norm2_Table[VVC1_show_bits(3)].value;
	VVC1_flush_bits(Norm2_Table[VVC1_show_bits(3)].length);
	bitplane[2*i + offset] = (value >> 1);
	bitplane[2*i + 1 + offset] = (value & 1);
      }
    }

    {
      int i, j;
      int pred = 0;
	    
      for (j = 0; j < mb_height; j++) {
	for (i = 0; i < mb_width; i++) {
	  if ((i == 0) && (j == 0)) {
	    pred = invert;
	  } else if (i == 0) {
	    pred = bitplane[0 + mb_width * (j - 1)];
	  } else if ((j > 0) && (bitplane[i + mb_width * (j - 1)] != 
				 bitplane[(i - 1) + mb_width * j])) {
	    pred = invert;
	  } else {
	    pred = bitplane[i - 1 + mb_width * j];
	  }
	  bitplane[i + mb_width * j] = 
	    bitplane[i + mb_width * j] ^ pred;
	}
      }
    }
    break;

  case vc1_BitplaneCodingModeDiff6:
    {
      int i, j, k, width_in_tiles, height_in_tiles;
      int residualx, residualy;
      unsigned char *pBit;

      if (((mb_height % 3) == 0) && ((mb_width % 3) != 0)) {
	width_in_tiles = mb_width / 2;
	height_in_tiles = mb_height / 3;

	for (j = 0; j < height_in_tiles; j++) {
	  pBit = &bitplane[j * 3 * mb_width];
	  pBit += mb_width & 1;

	  for (i = 0; i < width_in_tiles; i++) {
	    {
	      unsigned nb_bits;
	      int n;
		
	      n     = VVC1_norm6_vlc_table.table[VVC1_show_bits(9)][1];
	      value = VVC1_norm6_vlc_table.table[VVC1_show_bits(9)][0];
		
	      if (n < 0){
		VVC1_flush_bits(9);
		  
		nb_bits = -n;
		  
		n     = VVC1_norm6_vlc_table.table[(VVC1_show_bits(nb_bits) + value)][1];
		value = VVC1_norm6_vlc_table.table[(VVC1_show_bits(nb_bits) + value)][0];
	      }
	      VVC1_flush_bits(n);
	    }

	    for (k = 0; k < 6; k++) {
	      if ((k == 2) || (k == 4)) {
		pBit += mb_width;
		pBit -= 2;
	      }
	      *pBit++ = (value >> k) & 1;
	    }
	    pBit -= (2 * mb_width);    
	  }
	}
	residualx = mb_width & 1;
	residualy = 0;

      } else {
	width_in_tiles = mb_width / 3;
	height_in_tiles = mb_height / 2;

	for (j = 0; j < height_in_tiles; j++) {
	  pBit = &bitplane[j * 2 * mb_width];
	  pBit += mb_width % 3;
	  pBit += (mb_height & 1) * mb_width;

	  for (i = 0; i < width_in_tiles; i++) {
	    {
	      unsigned nb_bits;
	      int n;
		
	      n     = VVC1_norm6_vlc_table.table[VVC1_show_bits(9)][1];
	      value = VVC1_norm6_vlc_table.table[VVC1_show_bits(9)][0];
		
	      if (n < 0){
		VVC1_flush_bits(9);
		  
		nb_bits = -n;
		  
		n     = VVC1_norm6_vlc_table.table[(VVC1_show_bits(nb_bits) + value)][1];
		value = VVC1_norm6_vlc_table.table[(VVC1_show_bits(nb_bits) + value)][0];
	      }
	      VVC1_flush_bits(n);
	    }

	    for (k = 0; k < 6; k++) {
	      if (k == 3) {
		pBit += mb_width;
		pBit -= 3;
	      }
	      *pBit++ = (value >> k) & 1;
	    }
	    pBit -= mb_width;
	  }
	}
	residualx = mb_width % 3;
	residualy = mb_height & 1;
      }

      for (i = 0; i < residualx; i++) {
	int colskip = VVC1_get_bits(1);

	for (j = 0; j < mb_height; j++) {
	  value = 0;
	  if (colskip) {
	    value = VVC1_get_bits(1);
	    bitplane[i + mb_width * j] = value;
	  } else {
	    bitplane[i + mb_width * j] = 0;
	  }
	}
      }

      for (j = 0; j < residualy; j++) {
	int rowskip = VVC1_get_bits(1);
	for (i = residualx; i < mb_width; i++) {
	  value = 0;
	  if (rowskip) {
	    value = VVC1_get_bits(1);
	    bitplane[i] = value;
	  } else {
	    bitplane[i] = 0;
	  }
	}
      }
	    
    }

    {
      int i, j;
      int pred = 0;
	    
      for (j = 0; j < mb_height; j++) {
	for (i = 0; i < mb_width; i++) {
	  if ((i == 0) && (j == 0)) {
	    pred = invert;
	  } else if (i == 0) {
	    pred = bitplane[0 + mb_width * (j - 1)];
	  } else if ((j > 0) && (bitplane[i + mb_width * (j - 1)] != 
				 bitplane[(i - 1) + mb_width * j])) {
	    pred = invert;
	  } else {
	    pred = bitplane[i - 1 + mb_width * j];
	  }
	  bitplane[i + mb_width * j] = 
	    bitplane[i + mb_width * j] ^ pred;
	}
      }
    }	  
    break;

  case vc1_BitplaneCodingModeRaw:
    rawmode = 1;
    break;
  }
  
  return rawmode;
}

/*lint -restore */
/* PRQA L:L1 */
