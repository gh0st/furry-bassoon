/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *                                                                          
 * DESCRIPTION:         Control structure definition
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

#ifndef	_VVC1_CTRL_STRUCT_H
#define	_VVC1_CTRL_STRUCT_H

typedef enum {Intra, Forward, Backward, Bidir} mb_t;
typedef enum {Intra_blk, Inter_blk} blk_t;
typedef enum {No, Fl, Dc} idct_t;
typedef enum {T_8x8, T_8x4, T_4x8, T_4x4} trans_t;
typedef enum {NoMV, _4MV, _1MV} mvunit_t;
typedef enum {NoPred, VerPred, HorPred} acpred_t;



typedef struct {
	U32			SP_INFO;
        U32                     MB_TYPE;

        U32                     MB_x;
        U32                     MB_y;

        U32                     AC_PRED_DIR;
        U32                     MC_FLAGS;

        U32                     TRANSFORM_TYPE;
        U32                     POS;

        U32                     TR_COMPLEXITY;
        U32                     OVL_BORDERS;

	U32			AC_SCALE_TOP;
        U32			AC_SCALE_LEFT;	

	U32			LF_FLAGS_TOP;
        U32			LF_FLAGS_LEFT;	

	U32			PADDING0[2];

        struct{ U32 FWD,BWD; }  MVunit;

        struct{ U32 FWD,BWD; }  MVx[4];

        struct{ U32 FWD,BWD; }  MVy[4];

	U32			PADDING1[8];

        U32                     BLOCK_QP_ADD;
	U32			BLOCK_QP_MUL;

	U32                     IDCT_DATA[6][32];

	U32                     PADDING2[6];

        U32                     YUV422_PAD;
        U32                     YUV422;

	U32                     DISP_BUF;
	U32                     PAD4;

	U32			MBX;
        U32                     PAD5;	

	U32			MBY;
        U32                     PAD6;

	U32			DEC_BUF;
	U32                     BDEC_BUF;

	U32                     REF1_BUF;
	U32                     REF2_BUF;

	U32                     PADDING3[2];

} CTRL_STRUCT;




#ifndef _INDIRECT
extern volatile CTRL_STRUCT *VVC1_cs[2];
#else
extern volatile CTRL_STRUCT *VVC1_cs[NR_OF_ICM_BUFFERS];
#endif

extern int VVC1_init_cs(void);
extern void VVC1_mp_sync(void);


#endif // defined(_VVC1_CTRL_STRUCT_H)
