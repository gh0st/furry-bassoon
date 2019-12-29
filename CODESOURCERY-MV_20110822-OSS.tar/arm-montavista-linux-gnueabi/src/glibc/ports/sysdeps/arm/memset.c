/* Copyright (c) 2009 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   Contributed by CodeSourcery, Inc.

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

#include "arm_asm.h"
#include <string.h>
#include <stdint.h>

/* Standard operations for word-sized values.  */
#define WORD_REF(ADDRESS, OFFSET) \
	*((WORD_TYPE*)((char*)(ADDRESS) + (OFFSET)))

/* On processors with NEON, we use 128-bit vectors.  Also,
   we need to include arm_neon.h to use these.  */
#if defined(__ARM_NEON__)
  #include <arm_neon.h>

  #define WORD_TYPE uint8x16_t
  #define WORD_SIZE 16

  #define WORD_DUPLICATE(VALUE) \
	vdupq_n_u8(VALUE)

/* On ARM processors with 64-bit ldrd instructions, we use those,
   except on Cortex-M* where benchmarking has shown them to
   be slower.  */
#elif defined(__ARM_ARCH_5E__) || defined(__ARM_ARCH_5TE__) \
	|| defined(__ARM_ARCH_5TEJ__) || defined(_ISA_ARM_6)
  #define WORD_TYPE uint64_t
  #define WORD_SIZE 8

  /* ARM stores 64-bit values in two 32-bit registers and does not
     have 64-bit multiply or bitwise-or instructions, so this union
     operation results in optimal code.  */
  static inline uint64_t splat8(value) {
	union { uint32_t ints[2]; uint64_t result; } quad;
	quad.ints[0] = (unsigned char)(value) * 0x01010101;
	quad.ints[1] = quad.ints[0];
	return quad.result;
  }
  #define WORD_DUPLICATE(VALUE) \
	splat8(VALUE)

/* On everything else, we use 32-bit loads and stores.  */
#else
  #define WORD_TYPE uint32_t
  #define WORD_SIZE 4
  #define WORD_DUPLICATE(VALUE) \
	(unsigned char)(VALUE) * 0x01010101
#endif

/* On all ARM platforms, 'SHORTWORD' is a 32-bit value.  */
#define SHORTWORD_TYPE uint32_t
#define SHORTWORD_SIZE 4
#define SHORTWORD_REF(ADDRESS, OFFSET) \
	*((SHORTWORD_TYPE*)((char*)(ADDRESS) + (OFFSET)))
#define SHORTWORD_DUPLICATE(VALUE) \
	(uint32_t)(unsigned char)(VALUE) * 0x01010101

void *
memset (DST, C, LENGTH)
     void *DST;
     int C;
     size_t LENGTH;
{
  void* DST0 = DST;
  unsigned char C_BYTE = C;

  /* Handle short strings and immediately return.  */
  if (__builtin_expect(LENGTH < SHORTWORD_SIZE, 1)) {
    size_t i = 0;
    while (i < LENGTH) {
      ((char*)DST)[i] = C_BYTE;
      i++;
    }
    return DST;
  }

  const char* DST_end = (char*)DST + LENGTH;

  /* Align DST to SHORTWORD_SIZE.  */
  while ((uintptr_t)DST % SHORTWORD_SIZE != 0) {
    *(char*) (DST++) = C_BYTE;
  }

#if WORD_SIZE > SHORTWORD_SIZE
  SHORTWORD_TYPE C_SHORTWORD = SHORTWORD_DUPLICATE(C_BYTE);

  /* Align DST to WORD_SIZE in steps of SHORTWORD_SIZE.  */
  if (__builtin_expect(DST_end - (char*)DST >= WORD_SIZE, 0)) {
    while ((uintptr_t)DST % WORD_SIZE != 0) {
      SHORTWORD_REF(DST, 0) = C_SHORTWORD;
      DST += SHORTWORD_SIZE;
    }
#endif /* WORD_SIZE > SHORTWORD_SIZE */

    WORD_TYPE C_WORD = WORD_DUPLICATE(C_BYTE);

#if defined(__ARM_NEON__)
    /* Testing on Cortex-A8 indicates that the following idiom
       produces faster assembly code when doing vector copies,
       but not when doing regular copies.  */
    size_t i = 0;
    LENGTH = DST_end - (char*)DST;
    while (i + WORD_SIZE * 16 <= LENGTH) {
      WORD_REF(DST, i) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 1) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 2) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 3) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 4) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 5) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 6) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 7) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 8) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 9) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 10) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 11) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 12) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 13) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 14) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 15) = C_WORD;
      i += WORD_SIZE * 16;
    }
    while (i + WORD_SIZE * 4 <= LENGTH) {
      WORD_REF(DST, i) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 1) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 2) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 3) = C_WORD;
      i += WORD_SIZE * 4;
    }
    while (i + WORD_SIZE <= LENGTH) {
      WORD_REF(DST, i) = C_WORD;
      i += WORD_SIZE;
    }
    DST += i;
#else /* not defined(__ARM_NEON__) */
    /* Note: 16-times unrolling is about 50% faster than 4-times
       unrolling on both ARM Cortex-A8 and Cortex-M3.  */
    while (DST_end - (char*) DST >= WORD_SIZE * 16) {
      WORD_REF(DST, 0) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 1) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 2) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 3) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 4) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 5) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 6) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 7) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 8) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 9) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 10) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 11) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 12) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 13) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 14) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 15) = C_WORD;
      DST += WORD_SIZE * 16;
    }
    while (WORD_SIZE * 4 <= DST_end - (char*) DST) {
      WORD_REF(DST, 0) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 1) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 2) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 3) = C_WORD;
      DST += WORD_SIZE * 4;
    }
    while (WORD_SIZE <= DST_end - (char*) DST) {
      WORD_REF(DST, 0) = C_WORD;
      DST += WORD_SIZE;
    }
#endif /* not defined(__ARM_NEON__) */

#if WORD_SIZE > SHORTWORD_SIZE
  } /* end if N >= WORD_SIZE */

  while (SHORTWORD_SIZE <= DST_end - (char*)DST) {
    SHORTWORD_REF(DST, 0) = C_SHORTWORD;
    DST += SHORTWORD_SIZE;
  }
#endif /* WORD_SIZE > SHORTWORD_SIZE */

  while ((char*)DST < DST_end) {
    *((char*)DST) = C_BYTE;
    DST++;
  }

  return DST0;
}

libc_hidden_builtin_def (memset)
