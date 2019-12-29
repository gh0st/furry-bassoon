/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *                                                                          
 * DESCRIPTION:         decoder definitions
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

#ifndef _VVC1_DECODER_H_
#define _VVC1_DECODER_H_

#include "vvc1_dec.h"

#define _INDIRECT

#define DPB_DISPLAY_QUEUE_BUFFERS 0



#define CORE_USED 0

#define MAX_X_DIM 768
#define MAX_Y_DIM 576

#define MAX_MBX_X (MAX_X_DIM>>4)
#define MAX_MBX_Y (MAX_Y_DIM>>4)
#define MAX_MBS  (MAX_MBX_X * MAX_MBX_Y)

#define MAX_PRED_DC_MBS MAX_MBX_X

#define NR_OF_VID_BUFFERS (8+DPB_DISPLAY_QUEUE_BUFFERS)
#define NR_OF_IP_BUFFERS (5+((DPB_DISPLAY_QUEUE_BUFFERS+1)>>1))
#define FRM_DISP_DELAY 2
#define ICM_BUFFERS_USED 2

#ifdef _INDIRECT
#define NR_OF_ICM_BUFFERS 2048
#define BUFFERSIZE_BYTE 1024
#define BUFFERGROUP_SIZE NR_OF_ICM_BUFFERS
#define BUFFERGROUP_NB 2
#define ICM_SLOTS_NB ICM_BUFFERS_USED
#define ICM_SLOTS_OFFSET_BYTE 1024 

#ifndef LINUX
extern void VVC1_vid_setup_dmasync(ID *);
#endif
#endif



#define MODE_DIRECT	        0
#define MODE_INTERPOLATE	1
#define MODE_BACKWARD		2
#define MODE_FORWARD		3


#define I_VOP	0
#define P_VOP	1
#define B_VOP	2
#define BI_VOP	3
#define N_VOP	4

#define B_FRACTION_DEN  256


typedef enum
{
  vc1_QuantizerImplicit   = 0,
  vc1_QuantizerExplicit   = 1,
  vc1_QuantizerNonUniform = 2,
  vc1_QuantizerUniform    = 3
} vc1_eQuantizer;


typedef enum
{
  vc1_MVRange64_32    = 0,
  vc1_MVRange128_64   = 1,
  vc1_MVRange512_128  = 2,
  vc1_MVRange1024_256 = 3
} vc1_eMVRange;


typedef enum
{
  vc1_MVMode1MVHalfPelBilinear = 0,
  vc1_MVMode1MVHalfPel         = 1,
  vc1_MVMode1MV                = 2,
  vc1_MVModeMixedMV            = 3,
  vc1_MVModeIntensityCompensation
} vc1_eMVMode;


typedef enum
{
  vc1_QuantModeDefault = 0,
  vc1_QuantModeAllEdges = 15,
  vc1_QuantModeLeftTop = 3,
  vc1_QuantModeTopRight = 6,
  vc1_QuantModeRightBottom = 12,
  vc1_QuantModeBottomLeft = 9,
  vc1_QuantModeLeft = 1,
  vc1_QuantModeTop = 2,
  vc1_QuantModeRight = 4,
  vc1_QuantModeBottom = 8,
  vc1_QuantModeMBDual = 10,
  vc1_QuantModeMBAny = 11
} vc1_eQuantMode;


typedef enum
{
  vc1_BitplaneCodingModeNorm2 = 0,
  vc1_BitplaneCodingModeNorm6,
  vc1_BitplaneCodingModeRowskip,
  vc1_BitplaneCodingModeColskip,
  vc1_BitplaneCodingModeDiff2,
  vc1_BitplaneCodingModeDiff6,
  vc1_BitplaneCodingModeRaw
} vc1_eBitplaneCodingMode;


typedef enum
{
  vc1_CondOverNone = 0,
  vc1_CondOverAll  = 1,
  vc1_CondOverSome
} vc1_eCondOver;


enum CodingSet {
  CS_HIGH_MOT_INTRA = 0,
  CS_HIGH_MOT_INTER,
  CS_LOW_MOT_INTRA,
  CS_LOW_MOT_INTER,
  CS_MID_RATE_INTRA,
  CS_MID_RATE_INTER,
  CS_HIGH_RATE_INTRA,
  CS_HIGH_RATE_INTER
};


enum TransformTypes {
  TT_8X8,
  TT_8X4_BOTTOM,
  TT_8X4_TOP,
  TT_8X4,
  TT_4X8_RIGHT,
  TT_4X8_LEFT,
  TT_4X8,
  TT_4X4
};


typedef struct
{
  int x;
  int y;
} VECTOR;


typedef struct {
  short int dc_value[6];
  int is_intra[6];
  int coded[4];

  VECTOR fmv[4];
  VECTOR bmv;

  unsigned int quant;

} PRED_DC_ENTRY;


typedef struct
{
  VECTOR fmv;
} MB_CONTEXT;


typedef struct
{

  int last_coding_type;
  unsigned int mb_width;
  unsigned int mb_height;

  unsigned int sp_info;

  int seqheadersize;
  int loopfilter;
  int multirescoding;
  int fastumvc;
  int extendedmv;
  int dquant;
  int vstransform;
  int overlappedtransform;
  int syncmarker;
  int rangereduction;
  int maxbframes;
  int quantizer;
  int frameinterpolation;
  int reserved;

  int frameinterpolationhint;
  int rangeYscale;
  int rangeUVscale;
  int picturetype;
  int bfraction;
  int bufferfullness;
  int pqindex;
  int pquantizer;

  int mvrange;
  int luminancescale;
  int luminanceshift;
  int pictureres;
  int pquant;
  int halfqpstep;
  int mvmode;
  int intensitycompflag;
  int transformtypeindex;

  unsigned char *motionvectortype;
  unsigned char motionvectortype_rawmode;
  unsigned char *bframedirectmode;
  unsigned char bframedirectmode_rawmode;
  unsigned char *skipmb;
  unsigned char skipmb_rawmode;

  int mvtab;
  int cbptab;
  int frametransformtype;
  int mbtransformflag;
  int dquantframe;
  int altpquant;
  int quantmode;
  int frameACcodingindex;
  int frameACcodingindex2;
  int intratransformDCtable;
  int conditionaloverlap;

  int quarterpel;
  int rndctrl;

  int codingset;
  int codingset2; 
  int esc3_level_length;
  int esc3_run_length;

  unsigned int frames;
  
  unsigned int disp_width;
  unsigned int disp_height;
  unsigned int width;
  unsigned int height;
  int resize;
  
  int size;
  int fault;

  unsigned dcscale;

  short block[64];
  short dblock[64][2];

  unsigned int yuv422;
  unsigned previous_BVOP;
  unsigned count;
  unsigned toggle;
  unsigned flushed;

  VVC1_fbmem_allocator_fnptr fnptr;

#ifdef _INDIRECT
#ifndef LINUX
  ID VMP_TEST_dmaFlag;
#endif
  VID_BUFGROUP_PTR groupHandle;
#endif

} DECODER;


/************* Global variables */

extern DECODER VVC1_decoder;
extern unsigned int VVC1_buf_num;
extern unsigned int VVC1_icm_num;

extern VMP_FrameBuffer VVC1_fbmem_frame_buf[];
extern VMP_FrameBuffer VVC1_fbmem_disp_buf[];
extern int VVC1_fbmem_buf_used[];

extern MB_CONTEXT VVC1_colocated_mb[];

extern PRED_DC_ENTRY VVC1_predict_mem[], VVC1_predict_left, VVC1_predict_current;
extern unsigned char VVC1_motionvectortype[];
extern unsigned char VVC1_bframedirectmode[];
extern unsigned char VVC1_skipmb[];


#endif




/*************  Macros (outside of ifndef (_VVC1_DECODER_H) **********************/


#ifdef _GNUCC
#define VVC1_NTOHL(a,b) asm("rev %0, %1" : "=r" (a) : "r" (b));
#define VVC1_LOOK_AHEAD(res,l,r,o) (res = ((l) << (o)) | (((r) >> (31 - (o)))>>1));
#else
#define VVC1_NTOHL(a,b) __asm{ rev a,b }
#define VVC1_LOOK_AHEAD(res,l,r,o) (res = ((l) << (o)) | (((r) >> (32 - (o)))));
#endif
