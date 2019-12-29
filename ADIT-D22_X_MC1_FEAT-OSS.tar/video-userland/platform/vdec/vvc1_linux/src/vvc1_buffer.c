/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH 
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *                                                                          
 * DESCRIPTION:         Buffer Handling Routines
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

#include <arpa/inet.h>
#include "adit_typedef.h"

#include "vvc1_decoder.h"
#include "vvc1_buffer.h"


/* PRQA: QAC Message 277, 3771: deactivation because of pre-existing IP (type conversions)*/
/* PRQA S 277, 3771 L1*/
/* PRQA: Lint Message 10, 40:deactivation because of pre-existing IP */
/*lint -save -e10 -e40 */


unsigned int *VVC1_Buffer;
unsigned int *VVC1_BufferEnd;
unsigned int *VVC1_CurBitstr;
unsigned int VVC1_BitstrL, VVC1_BitstrR;
unsigned int VVC1_CurOffset;

static void get_next_word(void);
static void init_bitstr(void);



static void get_next_word(void)
{
  VVC1_BitstrL = VVC1_BitstrR;
  VVC1_NTOHL(VVC1_BitstrR, *VVC1_CurBitstr)
  VVC1_CurBitstr++;
}

static void init_bitstr(void)
{
  VVC1_CurBitstr = VVC1_Buffer;

  VVC1_NTOHL(VVC1_BitstrR, *VVC1_CurBitstr)
  VVC1_CurBitstr++;
  get_next_word();

  VVC1_CurOffset = 0;
}


void VVC1_init_buf(void)
{
  init_bitstr();
}


unsigned VVC1_show_bits(unsigned n) 
{
  int nbit;
  unsigned out;

  nbit = (int)(n + VVC1_CurOffset) - 32;
 
  if (nbit > 0) {
    out = ((VVC1_BitstrL & ((unsigned)-1 >> VVC1_CurOffset)) << nbit) | (VVC1_BitstrR >> (32 - nbit));
  } else {
    out = ((VVC1_BitstrL & ((unsigned)-1 >> VVC1_CurOffset)) >> (32 - VVC1_CurOffset - n));
  }

  return out;
}


void VVC1_flush_bits(unsigned n) 
{
  VVC1_CurOffset += n;
  if (VVC1_CurOffset >= 32) {
    VVC1_CurOffset -= 32;
    get_next_word();
  }
}


unsigned VVC1_get_bits(unsigned n) 
{
  int nbit = (int)(n + VVC1_CurOffset) - 32;
  unsigned Bits;
 
  if (nbit > 0) {
    Bits = ((VVC1_BitstrL & ((unsigned)-1 >> VVC1_CurOffset)) << nbit) | (VVC1_BitstrR >> (32 - nbit));
  } else {
    Bits = ((VVC1_BitstrL & ((unsigned)-1 >> VVC1_CurOffset)) >> (32 - VVC1_CurOffset - n));
  }
  VVC1_CurOffset += n;
  if (VVC1_CurOffset >= 32) {
    VVC1_CurOffset -= 32;
    get_next_word();
  }
  return Bits;
}


void VVC1_byte_align(void) 
{
  unsigned remainder = VVC1_CurOffset % 8;
  if (remainder != 0) {
    VVC1_flush_bits(8-remainder);
  }
}


unsigned VVC1_bits_to_byte_align() 
{
  unsigned n = (32 - VVC1_CurOffset) % 8;
  return n==0 ? 8 : n;
}


unsigned VVC1_show_bits_byte_align(unsigned n) 
{
  unsigned pos = VVC1_CurOffset + VVC1_bits_to_byte_align();
  int nbit = (n + pos) - 32;

  if (pos >= 32) {
    return VVC1_BitstrR >> (32 - nbit);
  } else if (nbit > 0) {
    return ((VVC1_BitstrL & ((unsigned)-1 >> pos)) << nbit) | (VVC1_BitstrR >> (32 - nbit));
  } else {
    return (VVC1_BitstrL & ((unsigned)-1 >> pos)) >> (32 - pos - n);
  }
}



/*lint -restore */
/* PRQA L:L1 */
