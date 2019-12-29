/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *                                                                          
 * DESCRIPTION:         Header file for Buffer Data Structure
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


#ifndef	_VVC1_BUFFER_H
#define	_VVC1_BUFFER_H

/*****************************************************************************/
/* Global Variables			     		     */
/*****************************************************************************/

extern unsigned int *VVC1_Buffer;
extern unsigned int *VVC1_BufferEnd;
extern unsigned int VVC1_BitstrL, VVC1_BitstrR;
extern unsigned int VVC1_CurOffset;
extern unsigned int *VVC1_CurBitstr;

/*****************************************************************************/
/* Functions			     		     */
/*****************************************************************************/

extern void VVC1_init_buf(void);
extern unsigned VVC1_show_bits(unsigned);
extern void VVC1_flush_bits(unsigned);
extern unsigned VVC1_get_bits(unsigned);
extern void VVC1_byte_align(void);
extern unsigned VVC1_bits_to_byte_align(void);
extern unsigned VVC1_show_bits_byte_align(unsigned);


#endif // defined(_VVC1_BUFFER_H)
