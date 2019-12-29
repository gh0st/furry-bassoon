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

/* Copy memory like memcpy, but no return value required.  Can't alias
   to memcpy because it's not defined in the same translation
   unit.  Also for Neon we need to save the vector registers, since
   memcpy may clobber them.  */
#if defined(__ARM_NEON__)
  void __attribute__((naked))
  __aeabi_memcpy (void *dest, const void *src, size_t n)
  {
    asm("fmrx	r3, fpscr\n\t"
        "push	{r3, lr}\n\t"
        "vpush	{d0-d7}\n\t"
        "vpush	{d16-d31}\n\t"
        "bl	memcpy\n\t"
        "vpop	{d16-d31}\n\t"
        "vpop	{d0-d7}\n\t"
        "pop	{r3, lr}\n\t"
        "fmxr	fpscr, r3\n\t"
        "bx	lr");
  }
#else
  void
  __aeabi_memcpy (void *dest, const void *src, size_t n)
  {
    memcpy (dest, src, n);
  }
#endif

/* Versions of the above which may assume memory alignment.  */
strong_alias (__aeabi_memcpy, __aeabi_memcpy4)
strong_alias (__aeabi_memcpy, __aeabi_memcpy8)
