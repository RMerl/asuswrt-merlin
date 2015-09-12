/*
 * BCM947XX DRAM init & sizing
 *
 * Used by both cfe and shared/boot.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: sisdram.S,v 1.9 2010-08-06 00:03:55 $
 */

#include "mipsinc.h"
#include "hndsoc.h"
#include "sbchipc.h"
#include "bcmdevs.h"


/* Debug macro - write a number to a pair of chipc regs - use it with caution,
 *  the registers being used only exist in chip rev >= 22, meaning NOT in 5354
 *  and previous chips. Also,  it changes k0 and k1 registers.
 *  Value can be read from epidiag -j using "pci r 0x180000d0 4"
 */
#ifdef	BCMDBG
#if	defined(IL_BIGENDIAN) && defined(BCMHND74K)
#define	BPADDR_OFF	4
#define	BPDATA_OFF	12
#else
#define	BPADDR_OFF	0
#define	BPDATA_OFF	8
#endif

#define TRACEINIT(x) \
	li	k0,KSEG1ADDR(0x180000d0); \
	li	k1,x; \
	sw	k1,BPADDR_OFF(k0)
	
#define TRACE(x) \
	li	k1,x; \
	sw	k1,BPADDR_OFF(k0)

#define TRACE2(x) \
	li	k1,x; \
	sw	k1,BPDATA_OFF(k0)

#else
#define TRACEINIT(x)
#define TRACE(x)
#define TRACE2(x)
#endif	/* BCMDBG */

	/*
	 * This file exists because board_draminit is an entry point for cfe as well
	 * as for hndrte & min_osl. Because of the former case, we cannot assume
	 * any of the register settings from boot.S,
	 */

	.text
	LEAF(board_draminit)
	.set	noreorder

	TRACEINIT(0x535301)
	move	gp,ra

	/* Figure out if we have an SB or AI chip */
	li	s2,KSEG1ADDR(SI_ENUM_BASE)	# s2 = SI_ENUM_BASE
	li	t0,CID_TYPE_MASK
	lw	s6,CC_CHIPID(s2)		# s6 = ChipId reg
	and	t1,t0,s6
	srl	s7,t1,CID_TYPE_SHIFT		# s7 = ChipType (0 for SB, = 1 for AI)

	/* Check if we booted from flash, compute reloc for text addresses */
	bal	1f
	nop

1:	li	t0,PHYSADDR_MASK
	and	t0,t0,ra
	li	t1,SI_FLASH1
	blt	t0,t1,2f
	move	s5,zero
	la	t0,1b
	sub	s5,ra,t0			# s5: Relocation factor

	/* Call appropriate draminit for chip type */
2:	TRACE(0x535302)
#if	!defined(BCMCHIPTYPE) || (BCMCHIPTYPE == 0)
	la	t2,sb_draminit
	beqz	s7,3f
	nop
#endif
	TRACE(0x535303)
#if	!defined(BCMCHIPTYPE) || (BCMCHIPTYPE == 1)
	la	t2,ai_draminit
	bnez	s7,3f
	nop
#endif

#ifdef	BCMCHIPTYPE
	/* BCMCHIPTYPE defined but does not match this chip */
	TRACE2(0x5353fe)
	b	sisdead
	nop
#endif

3:	add	t2,t2,s5
	jalr	t2
	nop

	/* Size memory if needed (Need to reinit TRACE after sb_draminit) */
	TRACEINIT(0x535304)
	beqz	v0,szmem
	nop
	
	li	a0,-1				# -1 means no controller
	bne	v0,a0,4f
	nop

sisdead:
	TRACE2(0x5353ff)
	b	sisdead
	nop

4:	jr	gp
	nop

szmem:
	TRACE(0x535305)
	li	s3,KSEG1			# s3 = KSEG1
	li	t2,0xaa55beef
	sw	zero,0x3c(s3)
	li	v0,(1 << 20)
	li	v1,(128 << 20)

5:	or	t0,v0,s3
	sw	t2,0x3c(t0)
	lw	t1,0x3c(t0)			# Read back to ensure completion
	lw	t1,0x3c(s3)
	beq	t1,t2,6f
	nop

	sll	v0,v0,1
	bne	v0,v1,5b
	nop
	/* Didn't find an alias, must be 128MB */

6:	jr	gp
	nop

	END(board_draminit)
