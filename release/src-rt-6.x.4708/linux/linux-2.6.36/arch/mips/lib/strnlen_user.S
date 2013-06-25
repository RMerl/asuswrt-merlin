/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1996, 1998, 1999, 2004 by Ralf Baechle
 * Copyright (c) 1999 Silicon Graphics, Inc.
 * Copyright (C) 2011 MIPS Technologies, Inc.
 */
#include <asm/asm.h>
#include <asm/asm-offsets.h>
#include <asm/regdef.h>

#define EX(insn,reg,addr,handler)			\
9:	insn	reg, addr;				\
	.section __ex_table,"a";			\
	PTR	9b, handler;				\
	.previous

/*
 * Return the size of a string including the ending NUL character upto a
 * maximum of a1 or 0 in case of error.
 *
 * Note: for performance reasons we deliberately accept that a user may
 *       make strlen_user and strnlen_user access the first few KSEG0
 *       bytes.  There's nothing secret there.  On 64-bit accessing beyond
 *       the maximum is a tad hairier ...
 */
LEAF(__strnlen_user_asm)
	.set	noreorder
	LONG_L		v0, TI_ADDR_LIMIT($28)	# pointer ok?
	and		v0, a0
#ifdef CONFIG_CPU_MICROMIPS
	bnezc		v0, .Lfault
#else
	bnez		v0, .Lfault
#endif

FEXPORT(__strnlen_user_nocheck_asm)
	PTR_ADDU	a1, a0			# stop pointer
	move		v0, a0
1:	beq		v0, a1, 1f		# limit reached?
	 nop
	EX(lb, t0, (v0), .Lfault)
#ifdef CONFIG_CPU_MICROMIPS
	addius5		v0, 1
	bnezc		t0, 1b
1:	jr		ra
	PTR_SUBU	v0, a0
#else
	bnez		t0, 1b
	PTR_ADDU	v0, 1
	jr		ra
1:	PTR_SUBU	v0, a0
#endif
	END(__strnlen_user_asm)

.Lfault:
	jr		ra
	move		v0, zero
