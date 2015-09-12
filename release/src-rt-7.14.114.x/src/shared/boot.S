/*
 * BCM947XX Boot code for standalone apps.
 *
 * Code should be position-independent until it copies itself to SDRAM.
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
 * $Id: boot.S,v 1.35 2010-08-06 00:03:55 $
 */

#include "mipsinc.h"
#include "hndsoc.h"
#include "sbchipc.h"
#include "bcmdevs.h"

#if	defined(NFLASH_SUPPORT)
#include <nflash.h>
#endif

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

	.text
	LEAF(startup)
	.set	noreorder

#ifdef	BCMDBG
	.set	noat
	li	$1,0x0badc0de
	.set	at
	li	$2,0x0badc0de
	li	$3,0x0badc0de
	li	$4,0x0badc0de
	li	$5,0x0badc0de
	li	$6,0x0badc0de
	li	$7,0x0badc0de
	li	$8,0x0badc0de
	li	$9,0x0badc0de
	li	$10,0x0badc0de
	li	$11,0x0badc0de
	li	$12,0x0badc0de
	li	$13,0x0badc0de
	li	$14,0x0badc0de
	li	$15,0x0badc0de
	li	$16,0x0badc0de
	li	$17,0x0badc0de
	li	$18,0x0badc0de
	li	$19,0x0badc0de
	li	$20,0x0badc0de
	li	$21,0x0badc0de
	li	$22,0x0badc0de
	li	$23,0x0badc0de
	li	$24,0x0badc0de
	li	$25,0x0badc0de
	li	$26,0x0badc0de
	li	$27,0x0badc0de
	li	$28,0x0badc0de
	li	$29,0x0badc0de
	li	$30,0x0badc0de
	li	$31,0x0badc0de
#endif	/* BCMDBG */

	TRACEINIT(0x424f00)
	# XXX: the following code snipet sets clk frequency to 200M
	# correct pll clk freq to real speed in the 5350 case.
	# unless its vsim which we detect as pkg option 1 (should be 0xe)
	# It is Ugly...but
	li	s2,KSEG1ADDR(SI_ENUM_BASE)	# s2 = SI_ENUM_BASE
	li	a3,BCM5350_CHIP_ID		# 5350 ChipID
	lw	s1,CC_CHIPID(s2)		# s1 = ChipID register
	li	t2,CID_ID_MASK			# chip id is bit 0-15
	and	t2,s1,t2
	bne	t2,a3,2f			# if not 5350 then skip
	nop
	
	TRACE(0x424f01)
	li	t2,CID_PKG_MASK			# if it is a vsim 5350, also skip
	and	t2,s1,t2
	li	a3,(HDLSIM5350_PKG_ID << CID_PKG_SHIFT)
	beq	t2,a3,initcpu			# if pkg opt 1 then skip
	nop

	TRACE(0x424f02)
	li	a3,CLKC_5350_N
	lw	t3,CC_CLKC_N(s2)
	beq	a3,t3,initcpu			# move ahead if clk freq set correctly
	nop
	sw	a3,CC_CLKC_N(s2)		# set control N1 to select 6
	li	t3,1
	sw	t3,CC_WATCHDOG(s2)		# set WatchDog Reset
1:	b	1b
	nop

2:	li	a3,BCM5354_CHIP_ID		# 5354 ChipID
	bne	t2,a3,initcpu			# if not 5354 then skip
	nop

	TRACE(0x424f03)
	li	t2,CID_REV_MASK			# Get chip rev
	and	t2,s1,t2
	li	t3,(3 << CID_REV_SHIFT)		# Is it an a3?
	blt	t2,t3,a0a1a2
	nop 

	/* Fix up for a3 (and up?) */
	li	a0,0x01330000			# Value for regcontrol 2
	li	t2,2
	sw	t2,PMU_REG_CONTROL_ADDR(s2)
	sw	a0,PMU_REG_CONTROL_DATA(s2)
	
a0a1a2:
	li	a0,0x00002000			# Value for regcontrol 0
	li	a1,0x06800000			# Value for regcontrol 1
	li	a3,0x02000000			# Value for regcontrol 3
	
fixregctl:
	li	t2,0x1				# Need define
	sw	t2,PMU_REG_CONTROL_ADDR(s2)
	sw	a1,PMU_REG_CONTROL_DATA(s2)
	
	/* 
	 * Trim the output voltage of the 1.2V BB switcher and 2.5V
	 * regulator to the correct value.
	 */
	li	t2,0x0
	sw	t2,PMU_REG_CONTROL_ADDR(s2)
	sw	a0,PMU_REG_CONTROL_DATA(s2)	# BB switcher to get 1.2V
	li	t2,0x3
	sw	t2,PMU_REG_CONTROL_ADDR(s2)
	sw	a3,PMU_REG_CONTROL_DATA(s2)	# of VDDP LDO to get 2.5V

	lw	t2,PMU_CTL(s2)			# Check if PLL has been programmed
	andi	t2,t2,PCTL_XTALFREQ_MASK
	bnez	t2,3f				# Yup, leave it alone
	nop

	li	t2,0x7ffff			# Should only turn off the PLL bit
	sw	t2,PMU_MIN_RES_MASK(s2)		# Disable base band PLL
	sw	t2,PMU_MAX_RES_MASK(s2)

	li	t2,0x1
	sw	t2,PMU_PLL_CONTROL_ADDR(s2)
	li	t2,0x66666602			# Set the PLL Mode properly
	sw	t2,PMU_PLL_CONTROL_DATA(s2)
	li	t2,0xfffff			# Enable base band PLL 
	sw	t2,PMU_MIN_RES_MASK(s2)
	sw	t2,PMU_MAX_RES_MASK(s2)
	nop

	/* Init code for ff4 space without TLB, enabling RAC */
3:	TRACE(0x424f04)
	li 	t0,0x1fa0000c			# Set up CBR to 0x1fax_xxxx
	.set	mips32
	mtc0 	t0,$22,6
	li 	t1,0x1fa00000
	lw 	t2,0x14(t1)
	or 	t3,t2,0xc0000000		# enable ffxx_xxxx space # without programming TLB
	sw 	t3,0x14(t1) 
	li 	t0,0xff40000c			# change CBR to ff4x_xxxx
	mtc0 	t0,$22,6
	.set	mips0


	/* Initialize processor state    */
initcpu:	
	TRACE(0x424f05)
	mfc0	t0,C0_STATUS
	li	t1,~(ST0_CU | ST0_RP | ST0_RE | ST0_TS | ST0_SR | ST0_NMI | ST0_UM | ST0_IE)
	and	t0,t1
	li	t1,ST0_IM
	or	t0,t1
	mtc0	t0,C0_STATUS

	mtc0	zero,C0_CAUSE

	mtc0	zero,C0_COUNT

	li	t0,-1
	mtc0	t0,C0_COMPARE

initram:
	/* Check if we booted from SDRAM */
	bal	1f
	nop

1:	li	t0,PHYSADDR_MASK
	and	t0,t0,ra
	li	t1,SI_FLASH1
	blt	t0,t1,chkcacheon
	move	s5,zero				# In ram, relocate factor is 0

	/* If we are in flash, compute reloc for text addresses */
	TRACE(0x424f06)
	li	t0,KSEG0ADDR(SI_FLASH1)
	la	t1,text_start
	sub	s5,t0,t1			# s5: Relocate factor
	
	/* Figure out if we have an SB or AI chip */
chkchiptype:
	TRACE(0x424f08)
	li	t0,CID_TYPE_MASK
	and	t0,t0,s1
	srl	s7,t0,CID_TYPE_SHIFT		# s7 = ChipType (0 for SB, = 1 for AI)

	bnez	s7,chkcacheon			# If ai chip no need to check chipc rev
	nop

	/* Is this chipc rev 11 or 12 and a serial flash? */
	TRACE(0x424f0a)
	li	t0,KSEG1ADDR(SI_ENUM_BASE)
	lw	t1,(SBCONFIGOFF + SBIDHIGH)(t0)
	and	t2,t1,SBIDH_CC_MASK
	srl	t2,t2,SBIDH_CC_SHIFT
	bne	t2,CC_CORE_ID,chkcacheon	# Not chipc
	nop
	and	t2,t1,SBIDH_RC_MASK
	and	t3,t1,SBIDH_RCE_MASK
	srl	t3,t3,SBIDH_RCE_SHIFT
	or	t2,t3
	ble	t2,10,chkcacheon		# ccrev <= 10
	nop
	bge	t2,13,chkcacheon		# ccrev >= 13
	nop
	lw	t0,CC_CAPABILITIES(t0)
	and	t0,t0,CC_CAP_FLASH_MASK
	beq	t0,SFLASH_AT,switchkseg0	# Atmel sflash
	nop
	beq	t0,SFLASH_ST,switchkseg0	# ST sflash
	nop

	b	chkcacheon
	nop

	/* Black hole for traps with BEV on */
	.org	0x380
bevtrap:
	TRACE2(0x424f0380)
	nop
	nop
	.set	mips32
	wait
	.set	mips0
	nop
	nop
	b	bevtrap
	nop

	/* Record the size of the binary */
	.org	BISZ_OFFSET
	.word	BISZ_MAGIC
	.word	text_start
	.word	text_end
	.word	data_start
	.word	data_end
	.word	bss_start
	.word	bss_end
	.word	_end

	/* Embedded NVRAM */
	.balign	0x400	
	.globl	embedded_nvram
embedded_nvram:
	.fill	0x400,4,~(0x48534c46)
	.long   0x4c5a4d41              # LZMA NVRAM Supported

chkcacheon:
	TRACE(0x424f0b)
	/* Check if the caches are already on */
	mfc0	t0,C0_CONFIG
	and	t0,CONF_CM_CMASK
	bne	t0,CONF_CM_UNCACHED,switchkseg0
	nop

initcaches:
	TRACE(0x424f0c)
	li	t0,(HDLSIM_PKG_ID << CID_PKG_SHIFT)	# vsim package option
	li	t2,CID_PKG_MASK			# isolate chip package
	and	t2,s1,t2
	beq	t0,t2,chcach			# Skip cache init if vsim

	/* Figure if it is a mips32r2 CPU which we take as an indication that
	 * there is no BRCM CP0 register and the D$ tags are in select 2
	 */
	mfc0	s6,C0_CONFIG
	andi	s6,CONF_AR			# s6 != 0 if mips32r2
	beqz	s6,1f
	nop
	b	2f
	nop

	/* Turn on the caches in the CP0 register */
1:	TRACE(0x424f0d)
	mfc0	t0,C0_DIAGNOSTIC
	or	t0,(BRCM_IC_ENABLE | BRCM_DC_ENABLE) # Enable both I$ and D$
	mtc0	t0,C0_DIAGNOSTIC
	nop

2:	/* Get cache sizes */
	TRACE(0x424f0e)
	.set	mips32
	mfc0	a0,C0_CONFIG,1			# a0 has CP0 CONFIG1
	.set	mips0

	li	a1,CONF1_DL_MASK
	and	a1,a0
	beq	a1,zero,initic
	nop

	srl	a1,CONF1_DL_SHIFT
	li	t0,CONF1_DL_BASE
	sll	a1,t0,a1			# a1 has D$ cache line size

	li	a2,CONF1_DA_MASK
	and	a2,a0
	srl	a2,CONF1_DA_SHIFT
	addiu	a2,CONF1_DA_BASE		# a2 now has D$ associativity

	li	t0,CONF1_DS_MASK
	and	t0,a0
	srl	t0,CONF1_DS_SHIFT
	li	a3,CONF1_DS_BASE
	sll	a3,a3,t0			# a3 has D$ sets per way

	multu	a2,a3				# sets/way * associativity
	mflo	t0				# total cache lines

	multu	a1,t0				# D$ linesize * lines
	mflo	a2				# a2 is now D$ size in bytes

	/* Initilize the D$: */
	beqz	s6,1f
	nop
	.set	mips32
	mtc0	zero,C0_TAGLO,2			# For mips32r2 the D$ Tags are in select 2
	mtc0	zero,C0_TAGHI,2
	.set	mips0
	b	2f
	nop

1:	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI

2:	li	t0,KSEG0			# Just an address for the first $ line
	addu	t1,t0,a2			#  + size of cache == end

	.set	mips32
3:	cache	Index_Store_Tag_D,0(t0)
	.set	mips0
	bne	t0,t1,3b
	addu	t0,a1

initic:
	/* Now we get to do it all again for the I$ */

	TRACE(0x424f0f)
	li	a1,CONF1_IL_MASK
	and	a1,a0
	beq	a1,zero,chcach
	nop

	srl	a1,CONF1_IL_SHIFT
	li	t0,CONF1_IL_BASE
	sll	a1,t0,a1			# a1 has I$ cache line size

	li	a2,CONF1_IA_MASK
	and	a2,a0
	srl	a2,CONF1_IA_SHIFT
	addiu	a2,CONF1_IA_BASE		# a2 now has I$ associativity

	li	t0,CONF1_IS_MASK
	and	t0,a0
	srl	t0,CONF1_IS_SHIFT
	li	a3,CONF1_IS_BASE
	sll	a3,a3,t0			# a3 has I$ sets per way

	multu	a2,a3				# sets/way * associativity
	mflo	t0				# total cache lines

	multu	a1,t0				# I$ linesize * lines
	mflo	a2				# a2 is cache size in bytes

	/* Initilize the I$: */
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI

	li	t0,KSEG0			# Just an address for the first $ line
	addu	t1,t0,a2			#  + size of cache == end

	.set	mips32
1:	cache	Index_Store_Tag_I,0(t0)
	.set	mips0
	bne	t0,t1,1b
	addu	t0,a1

chcach:	
	TRACE(0x424f10)
	/* Caches initialized, change cacheability */
	mfc0	t0,C0_CONFIG
	and	t0,~CONF_CM_CMASK
	or	t0,CONF_CM_CACHABLE_NONCOHERENT
	mtc0	t0,C0_CONFIG
	nop

switchkseg0:
	la	t2,board_draminit
	add	t2,t2,s5
	la	ra,backfbd
	add	ra,ra,s5
	jr	t2
	nop

	/* Should return ram size in v0 */
backfbd:
	bnez	v0,1f
	nop

panic:	TRACE2(0x424f0bad)
	b	panic
	nop

	/* Re-check if we booted from SDRAM */
1:	TRACE(0x424f11)
	bal	2f
	nop

2:	li	t0,PHYSADDR_MASK
	and	t0,t0,ra
	li	t1,SI_FLASH1
	blt	t0,t1,inram
	nop

	/* Copy self to SDRAM */
copy2ram:	
	TRACE(0x424f12)
#ifdef	CONFIG_XIP
	la	a0,text_end
	la	a1,data_start
	la	a2,data_end
#else
	li	a0,KSEG0ADDR(SI_FLASH1)
	la	a1,text_start
	li	t0,PHYSADDR_MASK		# Uncached writes to avoid a flush
	and	a1,a1,t0
	li	t1,KSEG1
	or	a1,a1,t1
	la	a2,data_end
	and	a2,a2,t0
	or	a2,a2,t1
#endif

#if	defined(NFLASH_SUPPORT)
	/* Take care of BCM5357 NAND boot (CFE in nand flash) */
	li	t4,CID_ID_MASK
	and	t4,t4,s6			# s6 = ChipId reg
	bne	t4,BCM5357_CHIP_ID,copydata2
	nop

	lw	t5,CC_CHIPST(s2)
	li	t4,0x10
	and	t4,t4,t5
	beqz	t4,copydata2			# not BCM5357 NAND boot
	nop

	move	t1,a0				# save a0
	move	t2,v0				# save v0
	move	t3,v1				# save v1

	la	t0,nfl_size_block		# a0, t8, t9 are corrupted
	add	t0,t0,s5			# relocate address
	jalr	t0				# v0: block size, v1: page size
	nop

	move	a0,t1				# restore a0
	move	t0,v0				# get block size from v0
	move	t1,v1				# get page size from v1
	
	move	v0,t2				# restore v0
	move	v1,t3				# restore v1

	move	t2,t0				# save block size

	li	t0,1
	subu	t2,t2,t0			# block address mask

	li	t3,KSEG0ADDR(SI_FLASH1)
	subu	t3,a0,t3			# block bus address

	/* t1: page size
	 * t2: block size - 1
	 * t3: block bus address
	 * v0: must be saved and restored when using it
	 */
checkalign:
	and	t5,t3,t2			# check block bus address alignment
	bnez	t5,copydata
	nop

	move	t4,a0				# save a0
	move	t6,a1				# save a1
	move	a3,v0				# save v0

	move	a0,t3				# set block bus address, and
	move	a1,t1				# set page size for nfl_check_badb


	la	t0,nfl_check_badb		# a0: block bus address, a1: page size
	add	t0,t0,s5			# relocate address
	jalr	t0
	nop

	move	t7,v0				# get v0 after calling nfl_check_badb
						# v0 = 1: bad block, v0 = 0: good block
	move	a0,t4				# restore a0
	move	a1,t6				# restore a1
	move	v0,a3				# restore v0

	beqz	t7,copydata			# go on data copy for good block
	nop

bypassbadb:
	add	t3,t3,t2			# increased by one block size
	addiu	t3,t3,1				# t2 = block size - 1

	add	a0,a0,t2			# increased by one block size
	addiu	a0,a0,1				# t2 = block size - 1

	blt	a1,a2,checkalign
	nop

copydata:
	lw	t0,0(a0)
	sw	t0,0(a1)
	add	a0,4
	add	a1,4
	add	t3,4
	blt	a1,a2,checkalign
	nop

	b	copydone			# bypass copydata2 since data
	nop					# copy is done
#endif /* NFLASH_SUPPORT */

copydata2:
1:	lw	t0,0(a0)
	sw	t0,0(a1)
	add	a0,4
	add	a1,4
	blt	a1,a2,1b
	nop

copydone:
	/* Go execute from ram now */
	la	t0,inram
	jr	t0
	nop

inram:	
clearbss:
	TRACE(0x424f13)
	/* Clear BSS */	
	la	a0,bss_start
	la	a1,bss_end
1:	sw	zero,0(a0)
	addi	a0,a0,4
	blt	a0,a1,1b
	nop

	/* Setup trap handlers */
	TRACE(0x424f14)
	la	t0,trap_init
	jalr	t0
	nop

	/* Record the memory size */
2:	TRACE(0x424f15)
	la	t0,_memsize
	sw	v0,0(t0)

	/* Set up stack pointer */
	or	v0,KSEG0
	sub	sp,v0,4

/* bootloader supports nvram dl: shift stack pointer beyond nvram */
#ifdef DL_NVRAM
	la	t1,_dlvarsz
	sw	sp,0(t1)
	li	a0,DL_NVRAM
	sub	sp,sp,a0
	la	t1,_dlvars
	sw	sp,0(t1)
	sub	sp,sp,4
#endif /* DL_NVRAM */

	/* Jump to C */
	la	t0,c_main
	jal	t0
	move	a0,ra

	/* In case c_main returns */
theend:	TRACE2(0x424feedd)
	nop
	nop
	.set	mips32
	wait
	.set	mips0
	nop
	nop
	b	theend
	nop

	.set reorder
	END(startup)

	.data
	.globl	_memsize
_memsize:
	.word	0

#ifdef DL_NVRAM
	.globl _dlvars
_dlvars:
	.word	0

	.globl _dlvarsz
_dlvarsz:
	.word	0
#endif /* DL_NVRAM */
