/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *                                                                          
 * DESCRIPTION:         decoding table definitions
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

#ifndef _VVC1_TABLES_H_
#define _VVC1_TABLES_H_


typedef struct {
    int bits;
    short (*table)[2];
    int table_size, table_allocated;
} VLCCoeff;


typedef struct
{
  unsigned char value;
  unsigned char length;
} VLCCode;


typedef struct
{
  unsigned char length;
  unsigned char value;
} VLCCodeR;


typedef struct
{
    unsigned char Numerator;
    unsigned char Denominator;
    unsigned char ScaleFactor;
} vc1_sBFraction;


extern const VLCCode P_Picture_Low_Rate_Motion_Vector_Mode[16];
extern const VLCCode P_Picture_High_Rate_Motion_Vector_Mode[16];
extern const VLCCode B_Picture_Low_Rate_Motion_Vector_Mode[8];
extern const VLCCode B_Picture_High_Rate_Motion_Vector_Mode[8];
extern const VLCCode *P_Picture_Low_Rate_Motion_Vector_Mode_2;
extern const VLCCode *P_Picture_High_Rate_Motion_Vector_Mode_2;
extern const VLCCode Conditional_Overlap_Table[4];
extern const VLCCode Motion_Vector_Range_Table[8];
extern const VLCCode Differential_Motion_Vector_Range_Table[8];
extern const VLCCode Intensity_Compensation_Field_Table[4];

extern const VLCCode IMODE_Table[16];
extern const VLCCode Norm2_Table[8];
extern const VLCCode Code_3x2_2x3_tiles_0 [16];
extern const VLCCode Code_3x2_2x3_tiles_1 [256];
extern const VLCCode Code_3x2_2x3_tiles_2 [91];
extern const VLCCodeR I_Picture_CBPCY_VLC_1 [64];
extern const VLCCodeR I_Picture_CBPCY_VLC_2 [48];
extern const VLCCodeR I_Picture_CBPCY_VLC_3 [512];
extern const VLCCodeR I_Picture_CBPCY_VLC_4 [256];

extern const VLCCodeR Block_Escape_Mode_3_PQUANT_1_To_7[32];
extern const VLCCodeR Block_Escape_Mode_3_PQUANT_8_To_31[64];

extern const vc1_sBFraction pBFraction[23]; 
extern const unsigned char NonUniformImplicit[32];

extern const unsigned VVC1_ac_sizes[8];
extern const unsigned VVC1_last_decode_table[8];
extern const unsigned char VVC1_index_decode_table[8][185][2];
extern const unsigned char VVC1_delta_level_table[8][31];
extern const unsigned char VVC1_last_delta_level_table[8][44];
extern const unsigned char VVC1_delta_run_table[8][57];
extern const unsigned char VVC1_last_delta_run_table[8][10];
extern VLCCoeff VVC1_ac_coeff_table_2[8];
extern VLCCoeff VVC1_dc_luma_vlc_2[2];
extern VLCCoeff VVC1_dc_chroma_vlc_2[2];
extern VLCCoeff VVC1_norm6_vlc_table;
extern const unsigned VVC1_dc_scale_table[32];
extern const unsigned VVC1_dqscale_table[64];
extern const unsigned char VVC1_scan_tables_transp[3][64];
extern const unsigned char VVC1_simple_progressive_8x8_zz_transp[64];
extern const unsigned char VVC1_simple_progressive_8x4_zz_transp[64];
extern const unsigned char VVC1_simple_progressive_4x8_zz_transp[64];
extern const unsigned char VVC1_simple_progressive_4x4_zz_transp[64];
extern const int VVC1_ttfrm_to_tt[4];

extern const short VVC1_dc_luma_bits_codes[2][1536][2];
extern const short VVC1_dc_chroma_bits_codes[2][1536][2];
extern const short VVC1_ac_coeff_bits_codes[8][3076][2];
extern const short VVC1_cbpcy_p_vlc[4][560][2];
extern const short VVC1_mv_diff_vlc[4][652][2];
extern const short VVC1_ttmb_vlc[3][520][2];
extern const short VVC1_ttblk_vlc[3][32][2];
extern const short VVC1_subblkpat_vlc[3][64][2];



// function declarations
extern void VVC1_init_tables(void);

#endif

