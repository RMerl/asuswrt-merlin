/*
 * BCM47XX Denali based memory controller initialization
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
 * $Id: aisdram.S,v 1.37 2011-01-20 04:26:45 $
 */

#include <hndsoc.h>
#include <bcmdevs.h>
#include <bcmnvram.h>
#include <sbsdram.h>
#include <sbmemc.h>
#include <sbsocram.h>
#include <dmemc_core.h>
#include <amemc_core.h>
#include <sbchipc.h>

#if	defined(NFLASH_SUPPORT)
#include <nflash.h>
#endif

#include <mipsinc.h>


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

dmemc_init:
	.word	0,	0x00000101
	.word	1,	0x01030100
	.word	2,	0x01030101
	.word	3,	0x01010101
	.word	5,	0x00000100
	.word	7,	0x01010000
	.word	8,	0x00000100
	.word	9,	0x00000100
	.word	10,	0x01010101
	.word	12,	0x00010200
	.word	16,	0x0f00000a
	.word	20,	0x64000000
	.word	27,	0x00000001
	.word	28,	0x0039000a
	.word	29,	0x081b002b
	.word	30,	0x00000000
	.word	31,	0x00000000
	.word	33,	0x000200c8
	.word	35,	0x000000c8
	.word	36,	0x00000000
	.word	125,	0x01010000
	.word	126,	0x02000100
	.word	129,	0x02000200
	.word	132,	0x00000000
	.word	138,	0x00000000
	.word	139,	0x00000000
	.word	151,	0x00000005
	.word	DMEMC_TABLE_END

sdr_init:
	.word	4,	0x00000100
	.word	6,	0x00000000
	.word	11,	0x00030001
	.word	14,	0x00020000
	.word	15,	0x0f000102
	.word	17,	0x05000002
	.word	18,	0x00000003
	.word	19,	0x00090200
	.word	21,	0x70006400
	.word	22,	0x7f000070
	.word	23,	0x00400000
	.word	24,	0x09030600
	.word	25,	0x00170017
	.word	32,	0x00320000
	.word	34,	0x000a4186
	.word	DMEMC_TABLE_END

ddr1_init:
	.word	4,	0x00000100
	.word	6,	0x00000100
	.word	11,	0x00030000
	.word	14,	0x02030200
	.word	15,	0x0f010203
	.word	17,	0x06000002
	.word	18,	0x00000006
	.word	19,	0x0010010e
	.word	21,	0x20006400
	.word	22,	0x5700002a
	.word	23,	0x00340000
	.word	24,	0x0e030800
	.word	25,	0x000f000f
	.word	26,	0x000f000f
	.word	32,	0x00000000
	.word	34,	0x000d2d89
	.word	127,	0x00000001
	.word	128,	0x07000300
	.word	130,	0x00010103
	.word	131,	0x00000200
	.word	133,	0x06120000
	.word	134,	0x06120612
	.word	135,	0x06120612
	.word	136,	0x00000612
	.word	137,	0x00000032
	.word	140,	0x001f4008
	.word	141,	0x001f4008
	.word	142,	0x002ee004
	.word	143,	0x002ee004
	.word	146,	0x000f0033
	.word	147,	0xf4103c17
	.word	148,	0xf4103c17
	.word	149,	0x26c00301
	.word	150,	0x26c00301
	.word	DMEMC_TABLE_END

ddr2_init:
	.word	4,	0x00010100
	.word	6,	0x00000000
	.word	11,	0x00030000
	.word	14,	0x02030203
	.word	15,	0x0f030204
	.word	17,	0x08000002
	.word	18,	0x00000004
	.word	19,	0x000f020e
	.word	21,	0x16006400
	.word	22,	0x6026162a
	.word	23,	0x00340000
	.word	24,	0x35060c00
	.word	25,	0x00200020
	.word	26,	0x00200020
	.word	32,	0x006b0000
	.word	34,	0x003848e1
	.word	127,	0x00000002
	.word	128,	0x07000404
	.word	130,	0x03020304
	.word	131,	0x00000400
	.word	133,	0x081b0000
	.word	134,	0x081b081b
	.word	135,	0x081b081b
	.word	136,	0x0000081b
	.word	137,	0x00400642
	.word	140,	0x00164008
	.word	141,	0x00164008
	.word	142,	0x00236004
	.word	143,	0x00236004
	.word	146,	0x000f0133
	.word	147,	0xf4112c17
	.word	148,	0xf4112c17
	.word	149,	0x26c00300
	.word	150,	0x26c00300
	.word	DMEMC_TABLE_END

#if	defined(NFLASH_SUPPORT)
nfl_pagesz_map:
	/* page size mapping */
	.word	0x200, 0x800, 0x1000, 0x2000

nfl_blksz_map:
	/* block size mapping */
	.word	0x4000, 0x20000, 0x2000, 0x80000, 0x40000, 0, 0, 0
#endif

	/* Register conventions.
	 *	Inherited from sisdram.S:
	 *		s2 = SI_ENUM_BASE
	 *		s5 = Relocation factor
	 *		s6 = ChipId reg
	 *		s7 = ChipType
	 *	Local:
	 *		s0 = sdram_config + sdram_refresh values
	 *		s1 = package opt
	 *		s3 = Controller corerev
	 *		s4 = Controller coreid
	 *		s8 = config_ncdl
	 *		a1 = dmemc regs
	 *		a2 = dmemc DMP regs
	 *		a3 = memory type (sdr,ddr1,ddr2)
	 */

LEAF(ai_draminit)
	.set	mips32
	.set	noreorder

	TRACE(0x415301)

	move	t6,ra

	/* Scan for a Denali DDR controller (a0) */
	lw	a0,CC_EROMPTR(s2)
	li	t0,KSEG1			# t0 = KSEG1
	or	a0,a0,t0			# a0 points to the EROM
	TRACE(0x415302)
1:
#if	defined(IL_BIGENDIAN) && defined(BCMHND74K)
	xor	t0,a0,4
	lw	t0,0(t0)
	add	t3,a0,4
	xor	t3,t3,4
	lw	t3,0(t3)
#else
	lw	t0,0(a0)			# t0 = CIA
	lw	t3,4(a0)			# t3 = CIB
#endif
	and	t1,t0,ER_TAG
	TRACE(0x415303)
	beq	t1,ER_END,noctrl
	nop
	TRACE(0x415304)
	beq	t1,ER_CI,2f
	nop
	TRACE(0x415305)
	b	1b
	addi	a0,4

2:	TRACE(0x415306)
	and	s4,t0,CIA_CID_MASK
	srl	s4,s4,CIA_CID_SHIFT		# s4 has controler coreid

	beq	s4,DMEMC_CORE_ID,founddmemc
	nop

	beq	s4,DMEMS_CORE_ID,founddmemc
	nop

	beq	s4,AMEMC_CORE_ID,founddmemc
	nop

	b	1b
	addi	a0,8				# Skip CIB too

	/* No DMEMC controller found */
noctrl:	TRACE(0x415307)
	jr	t6
	li	v0,-1


founddmemc:
	TRACE(0x415308)
	/* If we found the controller, but we are already in RAM, there is nothing
	 * to do. This will change if/when we have an AI chip with MIPS and
	 * SOCRAM only.
	 */
	bnez	s5,1f				# Not running from RAM, go ahead
	nop

	jr	t6				# Return with 0 rc.
	move	v0,zero

	/* We'll cheat a little: memory controllers don't have master ports, so
	 * the EROM entry right after the CIDs is the slave port for the registers
	 */
1:
#if	defined(IL_BIGENDIAN) && defined(BCMHND74K)
	add	a1,a0,8
	xor	a1,a1,4
	lw	a1,0(a1)
#else
	lw	a1,8(a0)
#endif
	li	t2,AD_ADDR_MASK
	and	a1,a1,t2
	li	t0,KSEG1			# t0 = KSEG1
	or	a1,a1,t0			# a1: dmemc regs
	/* after that, the first slave wrapper will be its DMP registers */
	addi	a0,12
1:
#if	defined(IL_BIGENDIAN) && defined(BCMHND74K)
	xor	t0,a0,4
	lw	t0,0(t0)
#else
	lw	t0,0(a0)
#endif
	and	t1,t0,ER_TAG
	beq	t1,ER_ADD,addesc
	nop
	b	1b
	addi	a0,4

addesc:	and	t1,t0,AD_ST_MASK
	beq	t1,AD_ST_SWRAP,swrap
	nop
	b	1b
	addi	a0,4

swrap:	and	a2,t0,t2
	li	t0,KSEG1			# t0 = KSEG1
	or	a2,a2,t0			# a2: dmemc DMP regs

	/* Got our core, reset it */
	TRACE(0x415309)
	bal	ai_core_reset
	nop

	/* Get package option for later */
	TRACE(0x41530a)
	li	t0,CID_PKG_MASK
	and	t0,t0,s6
	srl	s1,t0,CID_PKG_SHIFT		# s1 = package opt

	/* Find out the type of memory from the straps */
	/* Corerevs 0 & 1 did not have this register, so we have to
	 * check the corerev and use chipstatus for those two.
	 */
	and	t3,t3,CIB_REV_MASK
	srl	s3,t3,CIB_REV_SHIFT		# s3 = core revision
	beq     s4,DMEMS_CORE_ID,1f
	nop

	/* Go find nvram if the controller is AMEMC. */
	beq     s4,AMEMC_CORE_ID,find_nvram
	nop

	ble	s3,1,is16x
	nop

1:	/* Not a 4716/47162 (a0?) read the stat register */
	lw	t0,DMEMC_STAT(a1)
	li	t1,DM_STAT_MASK
	and	a3,t0,t1			# a3 == 4 if ddr2, 2 if ddr1, 1 if sdr.
	b	find_nvram
	nop

	/* Check chipc:	chipstatus for the ddr1/ddr2 strapping option */
is16x:	TRACE(0x41530b)
	lw	t0,CC_CHIPST(s2)
	li	t1,0x200
	and	t0,t0,t1
	beqz	t0,find_nvram
	li	a3,DM_STAT_DDR2

	li	a3,DM_STAT_DDR1

	/* Read sdram_config from nvram */
find_nvram:
	TRACE(0x41530c)
	li	t0,KSEG1ADDR(SI_FLASH2 - MAX_NVRAM_SPACE)
	li	t1,FLASH_MIN
	li	t2,SI_FLASH2_SZ
	li	t3,NVRAM_MAGIC
#ifndef SDRAM_PARAM_FROM_EMBEDDED_NVRAM
#if	defined(NFLASH_SUPPORT)
	/* Take care of 5357 NAND boot */
	li	t4,CID_ID_MASK
	and	t4,t4,s6
	bne	t4,BCM5357_CHIP_ID,1f
	nop
	lw	t5,CC_CHIPST(s2)
	li	t4,0x10
	and	t4,t4,t5
	beqz	t4,1f
	nop

	la	t4,nfl_size_block
	add	t4,t4,s5
	jalr	t4
	nop
	beqz	v0,embedded_nv
	nop

	/* skip bad blocks and locate nvram */
	move	t1,v0					# block size
	move	t2,t1
check_badb:
	move	a0,t2
	move	t0,a1					# save a1
	move	a1,v1					# page size
	la	t4,nfl_check_badb
	add	t4,t4,s5
	jalr	t4
	nop
	move	a1,t0					# restore a1
	bnez	v0,skip_badb
	nop

	li	t0,KSEG1ADDR(SI_FLASH1)
	add	t4,t0,t2
	lw	t5,0(t4)
	beq	t3,t5,read_config
	nop
skip_badb:
	add	t2,t2,t1
	li	t5,SI_FLASH1_SZ
	blt	t2,t5,check_badb
	nop
	b	embedded_nv
	nop
#endif

1:	add	t4,t0,t1
	lw	t5,0(t4)
	beq	t3,t5,read_config
	nop

	sll	t1,t1,1
	ble	t1,t2,1b
	nop
#endif
#if	defined(NFLASH_SUPPORT)
embedded_nv:
#endif
	/* Try embedded NVRAM at 4 KB and 1 KB as last resorts */
	TRACE(0x41530d)
	li	t4,KSEG1ADDR(SI_FLASH1 + 0x1000)
	lw	t5,0(t4)
	beq	t3,t5,read_config
	nop

	TRACE(0x41530e)
	li	t4,KSEG1ADDR(SI_FLASH1 + 0x400)
	lw	t5,0(t4)
	beq	t3,t5,read_config
	nop

	/* No nvram, pick sone defaults */

	/* assume DDRM16MX16 if ddr1 */
	TRACE(0x41530f)
	bne	a3,DM_STAT_DDR1,1f
	nop

	beq	s4,DMEMC_CORE_ID,init_dmemc
	li	s0,0x283		# Value for dmemc

	/* Use DDR1M16MX16 if QT and DDR1M32MX16 otherwise */
	beq	s1,HWSIM_PKG_ID,init_dmemc
	li	s0,0x103		# Value for dmems

	b	init_dmemc
	li	s0,0x003		# Value for dmems at 200MHz

1:	beq	a3,DM_STAT_SDR,init_dmemc
	li	s0,0x002		# Value for SDR

	/* For ddr2, use DDR2M32X16X2 if QT and DDR2M128X16X2 otherwise */
	beq	s1,HWSIM_PKG_ID,init_dmemc
	li	s0,0x104

	b	init_dmemc
	li	s0,0x144

read_config:
	/* sdram_config is a 16bit value 12 bytes inside the nvram hdr.
	 * Currently it is defined as:
	 *	10:8	column_size as per control13
	 *	7	reduc i.e. memory is half the width of the controller
	 *	6	8 banks
	 *	5	bypass
	 *	2:0	cas latency
	 *
	 * sdram_refresh:
	 *	15:8	delay_dqs_0
	 *	7:0	clk_wr_delay, or clk_wr_delay_0 (for corerev >= 2)
	 *
	 * sdram_ncdl:	control22
	 *	31:24	clk_dqs_delay, or clk_wr_delay_1 (for corerev >= 2)
	 *	23:16	delay_dqs_3
	 *	15:8	delay_dqs_2
	 *	7:0	delay_dqs_1
	 */
	TRACE(0x415310)
	lw	s0,12(t4)	# Pick up sdram_config & sdram_refresh

	/* Determine if it is DMEMC or AMEMC */
	bne s4,AMEMC_CORE_ID,init_dmemc
	lw	s8,16(t4)	# Pick up sdram_ncdl

	/* Initailize AMEMC */
	la	t2,ai_amemcinit
	add	t2,t2,s5
	jalr	t2
	nop

	jr	t6
	move	v0,zero

	/* Initialize DMEMC/DMEMS */
init_dmemc:
	/* For DDR2, init pvt controller */
	bne	a3,DM_STAT_DDR2,init_regs
	nop

	TRACE(0x415311)
	li	t0,1
	sw	t0,DMEMC_PVTGROUPJ(a1)
	sw	zero,DMEMC_PVTGROUPA(a1)
	sw	t0,DMEMC_PVTGROUPA(a1)

	/* Init the common regs */
init_regs:
	TRACE(0x415312)
	la	a0,dmemc_init
	bal	dmemc_init_regs			# dmemc_init_regs(a0): Inits from the table @ a0
	nop

	la	a0,sdr_init
	beq	a3,DM_STAT_SDR,1f
	nop

	TRACE(0x415313)
	la	a0,ddr1_init
	beq	a3,DM_STAT_DDR1,1f
	nop

	TRACE(0x415314)
	la	a0,ddr2_init
1:	bal	dmemc_init_regs
	nop

	/* Fixup differences between dmems & dmemc */
fixs:	bne	s4,DMEMS_CORE_ID,fix2
	nop

	lw	t1,DMEMC_DDR_CTRL(a1)
	li	t2,0x00000001			# Set DDR_CTRL bit 0 if SDR
	or	t1,t1,t2
	li	t0,0x02020002
	beq	a3,DM_STAT_SDR,1f
	nop

	li	t0,0x18006400
	sw	t0,DMEMC_CONTROL21(a1)
	li	t0,0x7f000018			# DLL DQS delay for DDR1
	sw	t0,DMEMC_CONTROL22(a1)
	li	t0,0x00500000
	sw	t0,DMEMC_CONTROL23(a1)
	li	t0,0x00320000			# Change MRS data for DDR1
	sw	t0,DMEMC_CONTROL32(a1)

	li	t0,0x02000002
	li	t2,~0x00000001			# Clear DDR_CTRL bit 0 if DDR1
	and	t1,t1,t2

1:	sw	t0,DMEMC_CONTROL52(a1)
	sw	t1,DMEMC_DDR_CTRL(a1)
	li	t0,0x00000001
	sw	t0,DMEMC_CONTROL53(a1)

	/* Fixup differences in 47162 */
fix2:	li	t0,CID_ID_MASK
	and	t0,t0,s6
	bne	t0,BCM47162_CHIP_ID,nvover
	nop

	li	t0,0x16006400
	sw	t0,DMEMC_CONTROL21(a1)
	li	t0,0x00480000
	sw	t0,DMEMC_CONTROL23(a1)

	/* Presumaby this is for DDR1 only? */
	li	t0,0x61161616
	sw	t0,DMEMC_CONTROL22(a1)

	/* Override the dll delays from nvram if provided */
nvover:	beqz	s8,chtref
	nop

	# Check for dmems (any rev)
	beq	s4,DMEMS_CORE_ID,old_ncdl
	nop

	# Dmemc rev < 2?
	ble	s3,1,old_ncdl
	nop

	li	t0,0x7f000000
	and	t0,t0,s0		# delay_dqs_0
	srl	t0,t0,15
	lw	t1,DMEMC_CONTROL140(a1)
	li	t2,~0x0000fe00
	and	t1,t1,t2
	or	t0,t0,t1
	sw	t0,DMEMC_CONTROL140(a1)

	li	t0,0x7f
	and	t0,t0,s8		# delay_dqs_1
	sll	t0,t0,9
	lw	t1,DMEMC_CONTROL141(a1)
	and	t1,t1,t2
	or	t0,t0,t1
	sw	t0,DMEMC_CONTROL141(a1)

	li	t0,0x7f0000
	and	t0,t0,s0		# clk_wr_delay_0
	srl	t0,t0,8
	lw	t1,DMEMC_CONTROL142(a1)
	li	t2,~0x00007f00
	and	t1,t1,t2
	or	t0,t0,t1
	sw	t0,DMEMC_CONTROL142(a1)

	li	t0,0x7f000000
	and	t0,t0,s8		# clk_wr_delay_1
	srl	t0,t0,16
	lw	t1,DMEMC_CONTROL143(a1)
	and	t1,t1,t2
	or	t0,t0,t1
	sw	t0,DMEMC_CONTROL143(a1)

	b	chtref
	nop

old_ncdl:
	li	t0,0x7f000000
	and	t0,t0,s0		# delay_dqs_0
	or	t0,t0,0x6400
	sw	t0,DMEMC_CONTROL21(a1)
	li	t0,0x007f0000
	and	t0,t0,s0		# clk_wr_delay
	sw	t0,DMEMC_CONTROL23(a1)

	sw	s8,DMEMC_CONTROL22(a1)

chtref:
	lw	t0,8(t4)		# Pick up sdram_init value for TREF
	lw	t2,DMEMC_CONTROL29(a1)
	beq	s4,DMEMS_CORE_ID,1f
	nop

	# Dmemc rev < 2?
	ble	s3,1,chhalf
	nop

	li	t1,0x3fff0000
	and	t0,t0,t1
	beqz	t0,chhalf
	nop

	li	t1,~0x3fff0000
	and	t2,t2,t1
	b	settref
	nop
1:
	li	t1,0x0fff0000
	and	t0,t0,t1
	beqz	t0,chhalf
	nop

	li	t1,~0x0fff0000
	and	t2,t2,t1

settref:
	or	t0,t0,t2
	sw	t0,DMEMC_CONTROL29(a1)

	/* Check for half-width */
chhalf:	li	t0,0x80
	and	t0,t0,s0
	beqz	t0,ch8banks
	nop

setreduc:
	/* Set reduc bit if half-wide */
	TRACE(0x415315)
	lw	t0,DMEMC_CONTROL08(a1)
	li	t1,0x01000000
	or	t0,t0,t1
	sw	t0,DMEMC_CONTROL08(a1)

	/* Check for 8-bank DDRs */
ch8banks:
	li	t0,0x40
	and	t0,t0,s0
	beqz	t0,docaslat
	nop

	/* Change regs for 8-bank DDRs */
do8banks:
	lw	t0,DMEMC_CONTROL05(a1)
	li	t1,0x00010000
	or	t0,t0,t1
	sw	t0,DMEMC_CONTROL05(a1)

	lw	t0,DMEMC_CONTROL19(a1)
	li	t1,0x0000000e
	or	t0,t0,t1
	sw	t0,DMEMC_CONTROL19(a1)

	lw	t0,DMEMC_CONTROL24(a1)
	li	t1,~0xff000000
	li	t2,0x22000000
	and	t0,t0,t1
	or	t0,t0,t2
	sw	t0,DMEMC_CONTROL24(a1)

	lw	t0,DMEMC_CONTROL34(a1)
	li	t1,~0x00ff0000
	li	t2,0x00250000
	and	t0,t0,t1
	or	t0,t0,t2
	sw	t0,DMEMC_CONTROL34(a1)

	/* Set the right value for column size and CAS latency */
docaslat:
	TRACE(0x415316)
	li	t0,0x0707
	and	t0,t0,s0
	sw	t0,DMEMC_CONTROL13(a1)
	andi	t0,s0,7			# Isolate cas
	beq	s4,DMEMS_CORE_ID,setcaslin
	nop
	ble	s3,1,setcaslin
	nop
	/* Additional settings required for dmemc rev >= 2 */
	sll	t1,t0,8
	and	t1,0xf00
	bne	a3,DM_STAT_DDR1,1f
	nop
	and	t1,0x300
1:	lw	t2,DMEMC_CONTROL128(a1)
	and	t2,~0xf00
	or	t2,t2,t1
	sw	t2,DMEMC_CONTROL128(a1)
	sll	t1,t0,4
	lw	t2,DMEMC_CONTROL137(a1)
	and	t2,~0x70
	or	t2,t2,t1
	sw	t2,DMEMC_CONTROL137(a1)
	bne	a3,DM_STAT_DDR2,setcaslin
	nop
	sub	t1,t0,1
	sll	t1,8
	lw	t2,DMEMC_CONTROL130(a1)
	and	t2,~0xf00
	or	t2,t2,t1
	sw	t2,DMEMC_CONTROL130(a1)
	sll	t1,8
	lw	t2,DMEMC_CONTROL15(a1)
	and	t2,~0xf0000
	or	t2,t2,t1
	sw	t2,DMEMC_CONTROL15(a1)
setcaslin:
	/* Set caslat_lin and caslat_lin_gate */
	lw	t2,DMEMC_CONTROL16(a1)
	/* Take care of fractional CAS latencies for DDR1 */
	li	t3,0
	bne	a3,DM_STAT_DDR1,1f
	nop
	andi	t1,t0,4
	beqz	t1,1f
	nop
	andi	t0,t0,3			# take off half bit
	li	t3,1
1:	sll	t0,t0,1			#  * 2
	add	t0,t0,t3
	addi	t1,t0,1			#  + 1 => caslin
	sll	t1,t1,8
	or	t2,t2,t1
	addi	t0,t0,-1		# and -1 => caslin_gate
	sll	t0,t0,16
	or	t2,t0,t2
	sw	t2,DMEMC_CONTROL16(a1)

	/* Finally set bypass mode if needed, but always for quickturn */
ckbyp:	beq	s1,HWSIM_PKG_ID,dobypass
	nop
	li	t0,0x20
	and	t0,t0,s0
	beqz	t0,ckvsim
	nop

dobypass:
	TRACE(0x415317)

	beq	s4,DMEMS_CORE_ID,1f
	nop

	bgt	s3,1,4f
	nop

1:	li	t0,0x00170017
	beq	a3,DM_STAT_SDR,2f
	nop

	li	t0,0x000f000f
	beq	a3,DM_STAT_DDR1,2f
	nop

	li	t0,0x00200020

2:	sw	t0,DMEMC_CONTROL25(a1)
	beq	s4,DMEMS_CORE_ID,3f
	nop
	sw	t0,DMEMC_CONTROL26(a1)

3:	lw	t0,DMEMC_CONTROL28(a1)
	li	t1,0x00ff0000
	or	t0,t0,t1
	sw	t0,DMEMC_CONTROL28(a1)

4:	lw	t0,DMEMC_CONTROL29(a1)
	li	t1,~0x000000ff
	li	t2,0x0000005f
	and	t0,t0,t1
	or	t0,t0,t2
	sw	t0,DMEMC_CONTROL29(a1)

	lw	t0,DMEMC_CONTROL05(a1)
	li	t1,0x00000001
	or	t0,t0,t1
	sw	t0,DMEMC_CONTROL05(a1)

	beq	s4,DMEMS_CORE_ID,ckvsim
	nop

	ble	s3,1,ckvsim
	nop

	lw	t0,DMEMC_CONTROL140(a1)
	li	t1,0x10000000
	or	t0,t0,t1
	sw	t0,DMEMC_CONTROL140(a1)

	lw	t0,DMEMC_CONTROL141(a1)
	or	t0,t0,t1
	sw	t0,DMEMC_CONTROL141(a1)

	/* For vsim change tinit so sims run faster */
ckvsim:	bne	s1,HDLSIM_PKG_ID,turnon
	nop

	TRACE(0x415318)
	li	t0,0x36
	sw	t0,DMEMC_CONTROL36(a1)

turnon:
	/* We are ready, turn controller on */
	TRACE(0x415319)
	lw	t0,DMEMC_CONTROL09(a1)		# Read current control09 reg
	or	t0,t0,DMC09_START		# Add start bit
	sw	t0,DMEMC_CONTROL09(a1)		# Start the controller

	beq	s4,DMEMS_CORE_ID,2f
	nop

	ble	s3,1,2f
	nop

1:	lw	t0,DMEMC_CONTROL133(a1)		# Poll for INT_INIT_DONE (dmemc >=2)
	and	t1,t0,DM_INT_INIT_DONE
	beqz	t1,1b
	nop
	/* Bypass mode programming */
	lw	t0,DMEMC_CONTROL05(a1)
	li	t1,0x00000001
	and	t0,t0,t1
	beqz	t0,ack_ints
	nop
	lw	t1,DMEMC_CONTROL144(a1)
	srl	t1,t1,3
	lw	t2,DMEMC_CONTROL140(a1)
	li	t0,~0x03ff0000
	and	t2,t2,t0
	sll	t0,t1,16
	or	t2,t2,t0
	sw	t2,DMEMC_CONTROL140(a1)

	lw	t2,DMEMC_CONTROL142(a1)
	li	t0,~0x01ff8000
	and	t2,t2,t0
	sll	t0,t1,1
	add	t0,t0,t1
	li	t1,0x3ff
	and	t0,t0,t1
	sll	t0,t0,15
	or	t2,t2,t0
	sw	t2,DMEMC_CONTROL142(a1)

	lw	t1,DMEMC_CONTROL145(a1)
	srl	t1,t1,3
	lw	t2,DMEMC_CONTROL141(a1)
	li	t0,~0x03ff0000
	and	t2,t2,t0
	sll	t0,t1,16
	or	t2,t2,t0
	sw	t2,DMEMC_CONTROL141(a1)

	lw	t2,DMEMC_CONTROL143(a1)
	li	t0,~0x01ff8000
	and	t2,t2,t0
	sll	t0,t1,1
	add	t0,t0,t1
	li	t1,0x3ff
	and	t0,t0,t1
	sll	t0,t0,15
	or	t2,t2,t0
	sw	t2,DMEMC_CONTROL143(a1)
ack_ints:
	/* Clear any pending interrupts from dmemc */
	li	t1,DMC132_INTACK_MASK
	and	t0,t0,t1			# t0 = control133 & mask
	lw	t2,DMEMC_CONTROL132(a1)
	not	t1
	and	t2,t1,t2			# t2 = control132 & ~mask
	or	t0,t0,t2			# Or them and ...
	sw	t0,DMEMC_CONTROL132(a1)		# Ack all ints
	b	3f
	nop

2:	lw	t0,DMEMC_CONTROL24(a1)		# Poll for INT_INIT_DONE (dmems & dmemc<2)
	and	t1,t0,DM_INT_INIT_DONE
	beqz	t1,2b
	nop
	/* Clear any pending interrupts from dmemc */
	li	t1,DMC23_INTACK_MASK
	and	t0,t0,t1			# t0 = control24 & mask
	lw	t2,DMEMC_CONTROL23(a1)
	not	t1
	and	t2,t1,t2			# t2 = control23 & ~mask
	or	t0,t0,t2			# Or them and ...
	sw	t0,DMEMC_CONTROL23(a1)		# Ack all ints

3:	jr	t6
	li	v0,0

	/* Reset core using DMP regs at (a2) */
ai_core_reset:
	/* Set reset while enabling the clock */
	li	t9,(SICF_FGC | SICF_CLOCK_EN)
	li	t8,AIRC_RESET
	sw	t9,AI_IOCTRLSET(a2)
	sw	t8,AI_RESETCTRL(a2)

	/* Read back and delay */
	lw	t8,AI_RESETCTRL(a2)
	lw	t8,AI_RESETCTRL(a2)
	lw	t8,AI_RESETCTRL(a2)

	/* Clear reset */
	li	t8,0
	sw	t8,AI_RESETCTRL(a2)

	/* Read back and delay */
	lw	t8,AI_RESETCTRL(a2)
	lw	t8,AI_RESETCTRL(a2)
	lw	t8,AI_RESETCTRL(a2)

	/* Clear Force Gated Clock */
	li	t9,SICF_FGC
	sw	t9,AI_IOCTRLCLEAR(a2)

	/* Read back and delay */
	lw	t9,AI_IOCTRL(a2)
	lw	t9,AI_IOCTRL(a2)
	lw	t9,AI_IOCTRL(a2)

	jr	ra
	nop

	/* Use table at (a0) to init dmemc regs.
	 * Assumes (a1) points to the regs.
	 */
dmemc_init_regs:
	beq     s4,DMEMS_CORE_ID,loop_regs
	li	t3,128

	ble	s3,1,loop_regs
	nop

	li	t3,256

loop_regs:
	add	a0,a0,s5			# Relocate address
	li	t0,DMEMC_TABLE_END
1:	lw	t1,0(a0)			# Pick up reg num
	bge	t1,t3,2f			# Return if the reg num >= num of supported regs
	nop
	beq	t1,t0,2f			# Return if done
	nop

	lw	t2,4(a0)			# Get reg value
	sll	t1,2				# Reg num * 4 is reg offset
	addu	t1,a1,t1
#if	defined(IL_BIGENDIAN) && defined(BCMHND74K)
	xor	t1,t1,4
#endif
	sw	t2,0(t1)			# Write reg
	b	1b
	addi	a0,8

2:	jr	ra
	nop

END(ai_draminit)

#if	defined(NFLASH_SUPPORT)
LEAF(nfl_size_block)
	lw	t9,CC_NAND_CONFIG(s2)
	li	t8,0x3
	sll	t8,20
	and	t8,t8,t9
	srl	t8,18				# index = (ncf & (0x3 << 20) >> 18)
	la	a0,nfl_pagesz_map
	add	a0,a0,s5			# Relocate address
	add	t8,t8,a0
	lw	v1,0(t8)

	li	t8,0x7
	sll	t8,28
	and	t8,t8,t9
	srl	t8,26				# index = (ncf & (0x7 << 28) >> 26)
	la	a0,nfl_blksz_map
	add	a0,a0,s5			# Relocate address
	add	t8,t8,a0
	lw	v0,0(t8)
	jr	ra
	nop
END(nfl_size_block)

	.balign 0x200


LEAF(nfl_check_badb)
	add	t9,a0,zero
	move	t5,zero
check_spare:
	/* Wait until nflash controller interface ready */
	li	v0,NIST_CTRL_READY
	li	t8,NIST_FLASH_READY
	or	v0,v0,t8
1:	lw	t8,CC_NAND_INTFC_STATUS(s2)
	and	t7,t8,v0
	bne	t7,v0,1b
	nop

	sw	t9,CC_NAND_CMD_ADDR(s2)
	lw	t9,CC_NAND_CMD_ADDR(s2)		# read back
	li	t9,NCMD_SPARE_RD
	sw	t9,CC_NAND_CMD_START(s2)
	lw	t9,CC_NAND_CMD_START(s2)	# read back
	/* Polling */
	li	t9,NIST_CTRL_READY
	li	t8,NIST_FLASH_READY
	or	t9,t9,t8
1:	lw	t8,CC_NAND_INTFC_STATUS(s2)
	and	t7,t8,t9
	bne	t7,t9,1b
	nop
	/* Check spare valid */
	li	t9,NIST_SPARE_VALID
	and	t7,t8,t9
	beqz	t7,badb
	nop
	/* Check spare byte */
	lw	t9,CC_NAND_SPARE_RD_0(s2)
	li	t8,0xff
	and	t7,t8,t9
	bne	t7,t8,badb
	nop
	/* Read next page */
	add	t9,a0,a1
	add	t5,t5,1
	beq	t5,1,check_spare
	nop

	jr	ra
	li	v0,0
badb:
	jr	ra
	li	v0,1
END(nfl_check_badb)
#endif

/*  *********************************************************************
    *  AI_AMEMCINIT
    *
    *  AI version of DDR / memory controller initialization
    *
    *  This routine deals with DDR23_PHY and PL341 MEMC.
    *
    ********************************************************************* */
/* Convenient macro for writing registers (use t0 for base) */
#define ddr_write(offset, value)   \
    li      a0, (value);           \
    sw      a0, offset(t0);

LEAF(ai_amemcinit)
        .set    noreorder

        /*
         * ddr23_phy_init
         */

        li      t0, KSEG1ADDR(AI_DDRPHY_BASE)

#ifndef CFG_QUICKTURN

        li      t2, 0x10000000
        li      t3, 0
        li      t5, 0
2:
        /* Do recalibration */
        beq     t5, PVT_MAX_RETRY, 9f
        lw      t7, DDR23PHY_ZQ_PVT_COMP_CTL(t0)
        ddr_write(DDR23PHY_ZQ_PVT_COMP_CTL, 0x04000000);
1:
        /* Wait until sample_done == 1 */
        lw      t1, DDR23PHY_ZQ_PVT_COMP_CTL(t0)
        and     t1, t1, t2;
        beq     t1, zero, 1b
        nop

        /* Increase total retry count */
        add     t5, t5, 1

        /* Check if the result is the same as previous one */
        lw      t1, DDR23PHY_ZQ_PVT_COMP_CTL(t0)
        beq     t1, t7, 3f
        nop

        /* If not, clear matched count */
        b       2b
        li      t3, 0
3:
        /* If so, increase matched count; continue if matched count == 3 */
        add     t3, t3, 1
        bne     t3, PVT_MATCHED_COUNT, 2b
        nop
9:

        /* setup PLL */
        ddr_write(DDR23PHY_PLL_CONFIG,      0x8000000c);
        ddr_write(DDR23PHY_PLL_PRE_DIVIDER,
                0x00000011 + (PLL_NDIV_INT_VAL << 8));
        ddr_write(DDR23PHY_PLL_DIVIDER,     0x02000000);
        ddr_write(DDR23PHY_PLL_CONFIG,      0x80000008);

        /* Wait for PLL locked */
        li      t2, 1;
1:      /* Wait until lock == 1 */
        lw      t1, DDR23PHY_PLL_STATUS(t0)
        and     t1, t1, t2;
        beq     t1, zero, 1b
        nop

        /* De-assert PLL reset */
        ddr_write(DDR23PHY_PLL_CONFIG,      0x80000000);
        ddr_write(DDR23PHY_PLL_CONFIG,      0x00000000);

        /*
         * Calibrate VDL
         */

        /* calib_once + calib_fast (for all BL) */
        ddr_write(DDR23PHY_BL3_VDL_CALIBRATE, 0x00000003);
        ddr_write(DDR23PHY_BL2_VDL_CALIBRATE, 0x00000003);
        ddr_write(DDR23PHY_BL1_VDL_CALIBRATE, 0x00000003);
        ddr_write(DDR23PHY_BL0_VDL_CALIBRATE, 0x00000003);

        li      t2, 0x0000003;
        li      t0, KSEG1ADDR(AI_DDRPHY_BASE)
1:      /* Wait until calib_idle == 1 and locked for all BL */
        lw      t1, DDR23PHY_BL3_VDL_STATUS(t0)
        and     t1, t1, t2;
        beq     t1, zero, 1b
        nop
        lw      t1, DDR23PHY_BL2_VDL_STATUS(t0)
        and     t1, t1, t2;
        beq     t1, zero, 1b
        nop
        lw      t1, DDR23PHY_BL1_VDL_STATUS(t0)
        and     t1, t1, t2;
        beq     t1, zero, 1b
        nop
        lw      t1, DDR23PHY_BL0_VDL_STATUS(t0)
        and     t1, t1, t2;
        beq     t1, zero, 1b
        nop

        /* VDL override */
        lw      t1, DDR23PHY_BL0_VDL_STATUS(t0)
        srl     t1, t1, 8
        andi    t2, t1, 0x3f                        /* VDL step size */
        li      t1, 1
        sll     t1, 16
        or      t2, t2, t1                          /* ovr_en */
        li      t1, 1
        sll     t1, 20
        or      t2, t2, t1                          /* ovr_force */
        sw      t2, DDR23PHY_STATIC_VDL_OVERRIDE(t0)

#endif /* !CFG_QUICKTURN */

        /*
         * Memory controller PL341 initialization
         */

        /* De-assert core reset */
        move	t0, a2	# AMEMC DMP regs
        ddr_write(AI_RESETCTRL, 0x00000000)

        move	t0, a1 # AMEMC regs

#ifdef CFG_DDR_REFRESH_PRD
        /* refresh_prd */
        ddr_write(PL341_refresh_prd, MEMCYCLES_MIN(CFG_DDR_REFRESH_PRD));
#endif

#ifdef CFG_DDR_T_MRD
        /* MRD time [6:0] */
        ddr_write(PL341_t_mrd, MEMCYCLES(CFG_DDR_T_MRD));
#endif

#ifdef CFG_DDR_T_RAS
        /* ras time [4:0] */
        ddr_write(PL341_t_ras, MEMCYCLES(CFG_DDR_T_RAS));
#endif

#ifdef CFG_DDR_T_RC
        /* t_rc [4:0] */
        ddr_write(PL341_t_rc, MEMCYCLES(CFG_DDR_T_RC));
#endif

#ifdef CFG_DDR_T_RCD
        /* t_rcd [4:0] */
        ddr_write(PL341_t_rcd, MEMCYCLES(CFG_DDR_T_RCD));
#endif

#ifdef CFG_DDR_T_RFC
        /* t_rfc [6:0]  schedule_rfc[14:8] */
        ddr_write(PL341_t_rfc,
            ((MEMCYCLES(CFG_DDR_T_RFC) - 3) << 8) | MEMCYCLES(CFG_DDR_T_RFC));
#endif

#ifdef CFG_DDR_T_RP
        /* t_rp [3:0] */
        ddr_write(PL341_t_rp,
            ((MEMCYCLES(CFG_DDR_T_RP) - 3) << 8) | MEMCYCLES(CFG_DDR_T_RP));
#endif

#ifdef CFG_DDR_T_RRD
        /* t_rrd [2:0] */
        ddr_write(PL341_t_rrd, MEMCYCLES(CFG_DDR_T_RRD));
#endif

#ifdef CFG_DDR_T_WR
        /* t_wr */
        ddr_write(PL341_t_wr, MEMCYCLES(CFG_DDR_T_WR));
#endif

#ifdef CFG_DDR_T_WTR
        /* t_wtr */
        ddr_write(PL341_t_wtr, MEMCYCLES(CFG_DDR_T_WTR));
#endif

#ifdef CFG_DDR_T_XP
        /* t_xp[7:0] */
        ddr_write(PL341_t_xp, MEMCYCLES(CFG_DDR_T_XP));
#endif

#ifdef CFG_DDR_T_XSR
        /* t_xsr[7:0] */
        ddr_write(PL341_t_xsr, MEMCYCLES(CFG_DDR_T_XSR));
#endif

#ifdef CFG_DDR_T_ESR
        /* t_esr[7:0] */
        ddr_write(PL341_t_esr, MEMCYCLES(CFG_DDR_T_ESR));
#endif

#ifdef CFG_DDR_T_FAW
        /* t_faw */
        ddr_write(PL341_t_faw,
            ((MEMCYCLES(CFG_DDR_T_FAW) - 3) << 8) | MEMCYCLES(CFG_DDR_T_FAW));
#endif

        /* Check if sdram_config is nonzero */
        and	t1, s0, 0xffff
        bne	t1, $0, sdram_mem_cfg
        nop

        /* sdram_config is 0, configure by code-default values */
        /* Figure out if it's a low cost package */
        andi	t1, s1, 0x03

        /* Standard package, assume 1024-column, 32-bit, 8-bank DDR */
        beq	t1, $0, sdram_mem_cfg
        li	s0, (0x0140 | CFG_DDR_CAS_LATENCY)

        /* Low cost package, assume 1024-column, 16-bit, 8-bank DDR */
        li	s0, (0x01c0 | CFG_DDR_CAS_LATENCY)

sdram_mem_cfg:
        /* CAS Latency:
        *  sdram_config[2:0]: CAS latency
        *  PL341_cas_latency[3:1]: CL range from 2 to 6
        *  PL341_write_latency[2:0]: CL - 1
        */
        andi	t1, s0, 0x07
        sll	t2, t1, 1
        sw	t2, PL341_cas_latency(a1)
        subu	t2, t1, 1
        sw	t2, PL341_write_latency(a1)

        /* PL341_memory_cfg: Rows and Columns */
        lw	t2, PL341_memory_cfg(a1)	# Read PL341_memory_cfg

        /* Columns:
        *  sdram_config[10:8]: 0=2048; 1=1024; 2=512; 3=256 columns
        *  PL341_memory_cfg[2:0]: 1=9-bit; 2=10-bit; 3=11-bit; Others=Reserved.
        */
        li	t3, ~0x7			# columns(bit2:0)
        and	t2, t2, t3		# clear column
        srl	t1, s0, 8			# Column fields from sdram_config s0(bit10:8)
        andi	t1, t1, 0x07
        li	t3, 0x3
        subu	t1, t3, t1
        andi	t1, t1, 0x07
        or	t2, t2, t1
        sw	t2, PL341_memory_cfg(a1)

        /* PL341_memory_cfg2: Bus width and Banks */
        lw	t2, PL341_memory_cfg2(a1)	# Read PL341_memory_cfg2

        /* Low cost package is 16-bit bus */
        andi	t1, s1, 0x03
        li	t3, ~0xc0
        bne	t1, $0, 2f	# nonzero: low cost package with 16-bit bus
        and	t2, t2, t3	# Clear bit[7:6] to work in 16-bit mode

        /* Bus width:
        *  sdram_config[7]: 0 default bus width, 1: reduced width
        *  PL341_memory_cfg2[7:6]: 00 for 16bit, 01=32bit, 10=64bit, 11=reserved
        */
        andi	t1, s0, 0x80
        bne	t1, $0, 2f	# Work in 16-bit mode
        nop
        or	t2, t2, 0x40	# Set bit[7:6] to 32-bit mode
2:
        /* Banks:
        *  sdram_config[6]: 0 for 4 banks, 1 for 8 banks
        *  PL341_memory_cfg2[5:4]: 00 for 4 banks, 11 for 8 banks
        */
        andi	t1, s0, 0x40	# Bank configuration
        li	t3, ~0x30
        beq	t1, $0, 1f
        and	t2, t2, t3	# Clear bit[5:4] to work in 4-bank mode
        or	t2, t2, 0x30	# Set bit[5:4] to 11 for 8-bank mode
1:
        sw	t2, PL341_memory_cfg2(a1);

        /* chip0 configuration */
        ddr_write(PL341_chip_0_cfg, 0x00000000);

        /* user_config0 */
        ddr_write(PL341_user_config0, 0x00000003);

        /*
         * DDR2 chip initialization
         */

        /* Issue NOP */
        ddr_write(PL341_direct_cmd, MCHIP_CMD_NOP);

        /* issue precharge all */
        ddr_write(PL341_direct_cmd, MCHIP_CMD_PRECHARGE_ALL);

        /* Set EMR2 */
        ddr_write(PL341_direct_cmd, MCHIP_CMD_MODE_REG | MCHIP_MODEREG_SEL(2));

        /* Set EMR3 */
        ddr_write(PL341_direct_cmd, MCHIP_CMD_MODE_REG | MCHIP_MODEREG_SEL(3));

        /* DLL Enable */
        ddr_write(PL341_direct_cmd,
            MCHIP_CMD_MODE_REG | MCHIP_MODEREG_SEL(1) |
            MCHIP_EMR1_DLL_DISABLE(0)
            );

        /* DLL Reset */
        /* Set CAS to external memory devices */
        lw	t1, PL341_cas_latency(a1)		# CAS value in bit3:1
        li	t2, (MCHIP_CMD_MODE_REG | MCHIP_MODEREG_SEL(0) | \
            MCHIP_MR_WRITE_RECOVERY(MEMCYCLES(CFG_DDR_T_WR)) | \
            MCHIP_MR_DLL_RESET(1) | MCHIP_MR_BURST_LENGTH)
        sll	t1, t1, 3		# Shift to bit6:4 for DDR MRS register
        or	t2, t2, t1
        sw	t2, PL341_direct_cmd(a1)

        /* issue precharge all */
        ddr_write(PL341_direct_cmd, MCHIP_CMD_PRECHARGE_ALL);

        /* auto-refresh 2 times */
        ddr_write(PL341_direct_cmd, MCHIP_CMD_AUTO_REFRESH);
        ddr_write(PL341_direct_cmd, MCHIP_CMD_AUTO_REFRESH);

        /* DLL Reset=0 (Un-Reset DLL) */
        and	t2, t2, ~MCHIP_MR_DLL_RESET(1)
        sw	t2, PL341_direct_cmd(a1)

        /* DLL Enable & RTT 75ohm */
        ddr_write(PL341_direct_cmd,
            MCHIP_CMD_MODE_REG | MCHIP_MODEREG_SEL(1) |
            MCHIP_EMR1_DLL_DISABLE(0) | MCHIP_EMR1_RTT_75_OHM |
            MCHIP_EMR1_OCD_CALI_EXIT
            );

        /* OCD Defaults */
        ddr_write(PL341_direct_cmd,
            MCHIP_CMD_MODE_REG | MCHIP_MODEREG_SEL(1) |
            MCHIP_EMR1_DLL_DISABLE(0) | MCHIP_EMR1_RTT_75_OHM |
            MCHIP_EMR1_OCD_CALI_DEFAULT
            );

        /* OCD Exit */
        ddr_write(PL341_direct_cmd,
            MCHIP_CMD_MODE_REG | MCHIP_MODEREG_SEL(1) |
            MCHIP_EMR1_DLL_DISABLE(0) | MCHIP_EMR1_RTT_75_OHM |
            MCHIP_EMR1_OCD_CALI_EXIT
            );

        /* set MEMC to GO */
        ddr_write(PL341_memc_cmd, 0);
        nop
        nop

        jr      ra
        nop

        .set    reorder
END(ai_amemcinit)
