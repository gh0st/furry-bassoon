	.arch armv5t
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 2
	.eabi_attribute 18, 4
#APP
	
#include "defs.h"
	
/*@HEADER_ENDS*/
/*@_init_PROLOG_BEGINS*/
	.align	2
	.type	call_gmon_start, %function
call_gmon_start:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	@ link register save eliminated.
	ldr	r3, .L8
	ldr	r2, .L8+4
.LPIC0:
	add	r3, pc, r3
	ldr	r1, [r3, r2]
	cmp	r1, #0
	bxeq	lr
	b	__gmon_start__(PLT)
.L9:
	.align	2
.L8:
	.word	_GLOBAL_OFFSET_TABLE_-(.LPIC0+8)
	.word	__gmon_start__(GOT)
#APP
	.section .init
	.section	.init,"ax",%progbits
	.align	2
	.global	_init
	.type	_init, %function
_init:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	stmfd	sp!, {r4, lr}
	bl	call_gmon_start(PLT)
#APP
@ 96 "../sysdeps/generic/initfini.c" 1
	ALIGN
@ 0 "" 2
@ 97 "../sysdeps/generic/initfini.c" 1
	END_INIT
@ 0 "" 2
@ 99 "../sysdeps/generic/initfini.c" 1
	
/*@_init_PROLOG_ENDS*/
/*@_fini_PROLOG_BEGINS*/
	.section .fini
	.section	.fini,"ax",%progbits
	.align	2
	.global	_fini
	.type	_fini, %function
_fini:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	stmfd	sp!, {r4, lr}
#APP
@ 116 "../sysdeps/generic/initfini.c" 1
	ALIGN
@ 0 "" 2
@ 117 "../sysdeps/generic/initfini.c" 1
	END_FINI
@ 0 "" 2
@ 118 "../sysdeps/generic/initfini.c" 1
	
/*@_fini_PROLOG_ENDS*/
/*@TRAILER_BEGINS*/
	.weak	__gmon_start__
	.ident	"GCC: (MontaVista Linux Sourcery G++ 4.3-328) 4.3.3"
	.section	.note.GNU-stack,"",%progbits
