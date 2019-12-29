/*++++++++++++++++++++++++++++ FileHeaderBegin +++++++++++++++++++++++++++++
 * (c) videantis GmbH
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *                                                                          
 * DESCRIPTION:         Control Structure Handling Routines
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

#include "adit_typedef.h"

#ifndef LINUX
#include "vmp_lowlevelif.h"
#else
#include "vid_lowlevelif.h"
#endif

#include "vvc1_decoder.h"
#include "vvc1_ctrl_struct.h"

/* PRQA: QAC Message 3760: deactivation because of pre-existing IP*/
/* PRQA S 3760 L1*/
/* PRQA: Lint Message 826:deactivation because of pre-existing IP */
/*lint -save -e826*/


#ifndef _INDIRECT
volatile CTRL_STRUCT *VVC1_cs[2];
#else
volatile CTRL_STRUCT *VVC1_cs[NR_OF_ICM_BUFFERS];
#endif


void VVC1_mp_sync (void) 
{
#ifndef _INDIRECT // empty for _INDIRECT
  do
    {
      do 
	{
	  /* 
	     synchronization operation implemented with semaphores 
	     placed in intercore memory;
	     on average, a synchronization operation is performed 
	     roughly every 24 microseconds;
	  */
	}
      while ((VVC1_cs[VVC1_buf_num]->SP_INFO & 0x1) != 0);
    }
  while ((VVC1_cs[VVC1_buf_num]->SP_INFO & 0x1) != 0);
#endif  
}


int VVC1_init_cs(void) 
{
#ifdef _INDIRECT

  int rc;

  rc = vid_get_buffers(CORE_USED, &VVC1_decoder.groupHandle, 1); // blocking 
  if (rc != VMP_OK) {
    return VMP_E;
  } else {
    VVC1_cs[0] = (CTRL_STRUCT  *)VVC1_decoder.groupHandle.address.virt_addr;
    {
      int i;
      for (i = 1; i < VVC1_decoder.groupHandle.buffergroup_size; i++) {
	VVC1_cs[i] = VVC1_cs[0] + i;
      }
    }
  }
  return VMP_OK;

#else // ~INDIRECT

  VID_ADDR_PTRS ic_mem_ptr;

  if (vid_icm_alloc(&ic_mem_ptr, CORE_USED) != VMP_OK) {
    return VMP_E;
  }
  VVC1_cs[0] = (volatile CTRL_STRUCT *) ic_mem_ptr.virt_addr;
  VVC1_cs[1] = VVC1_cs[0] + 1;
  return VMP_OK;

#endif
}

/*lint -restore */
/* PRQA L:L1 */
