/* Copyright (C) 2005, 2009 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <string.h>

/* Set memory like memset, but different argument order and no return
   value required.  */
#if defined(__ARM_NEON__)
  void __attribute__((naked))
  __aeabi_memset (void *dest, size_t n, int c)
  {
    asm("fmrx	r3, fpscr\n\t"
        "push	{r3, lr}\n\t"
        "vpush	{d0-d7}\n\t"
        "vpush	{d16-d31}\n\t"
        "mov	r3, r1\n\t"
        "mov	r1, r2\n\t"
        "mov	r2, r3\n\t"
        "bl	memset\n\t"
        "vpop	{d16-d31}\n\t"
        "vpop	{d0-d7}\n\t"
        "pop	{r3, lr}\n\t"
        "fmxr	fpscr, r3\n\t"
        "bx	lr");
  }
#else
  void
  __aeabi_memset (void *dest, size_t n, int c)
  {
    memset (dest, c, n);
  }
#endif

/* Versions of the above which may assume memory alignment.  */
strong_alias (__aeabi_memset, __aeabi_memset4)
strong_alias (__aeabi_memset, __aeabi_memset8)
