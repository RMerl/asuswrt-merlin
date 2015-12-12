/*
 * BCM47XX Sonics SiliconBackplane SDRAM/MEMC core initialization
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
 * $Id: sbsdram.S,v 1.45 2008-11-14 17:51:32 $
 */

#include <hndsoc.h>
#include <sbmemc.h>
#include <sbsocram.h>
#include <sbchipc.h>
#include <bcmdevs.h>
#include <bcmnvram.h>

#include <mipsinc.h>

/* #define	DEBUG_SBSDRAM	1 */
#ifdef	DEBUG_SBSDRAM
	/* Trace function
	 * Write to prog space so we can see it in
	 * the Logic Analizer
	 */
#define	LATRACEINIT		\
	li	k0,0xb8000120;	\
	li	k1,0x11;	\
	sw	k1,0(k0);	\
	li	k1,0x01020108;	\
	sw	k1,4(k0);	\
	li	k0,0xbb000000

#define	LATRACE(num, reg)	\
	sw	reg,((num & 0xff) << 4)(k0)

#else
#define	LATRACEINIT
#define	LATRACE(num, reg)
#endif	/* DEBUG_SBSDRAM */
/* Debug macro - write a number to a chipc reg - use it with caution,
 *  it changes k0 and k1 registers.
 */
#ifdef	BCMDBG

#define	FLADDR_OFF	4
#define	FLDATA_OFF	8

#define TRACEINIT(x) \
	li	k0,KSEG1ADDR(0x18000040); \
	li	k1,x; \
	sw	k1,FLADDR_OFF(k0)
	
#define TRACE(x) \
	li	k1,x; \
	sw	k1,FLADDR_OFF(k0)

#define TRACE2(x) \
	li	k1,x; \
	sw	k1,FLDATA_OFF(k0)

#else
#define TRACEINIT(x)
#define TRACE(x)
#define TRACE2(x)
#endif	/* BCMDBG */

/*
 * Register usage within this file:
 *
 *		top	ncdlsearch	test_mem	Xdr_do_init	sb_reset_core
 *	v0:	retval			retval
 *	v1:	corerev	-		-		corerev		-
 *	a0:	coreptr	coreptr		-		coreptr		coreptr
 *	a1:		x		x		x		sdr/ddr flag
 *	a2:	NVRAM			x		x
 *	a3:				x
 *	t0:	-	-				config
 *	t1:	-	-				mode
 *	t2:	-	wr/strm		off		wr/strm
 *	t3:	-	rd/strd				rd/strd
 *	t4:	-	g/clkd				g/clkd
 *	t5:				x
 *	t6:	retaddr	-				-		-
 *	t7:	-	-				retaddr		-
 *	s0:		pass_count			-		-
 *	s1:		wrsum/clkdsum			-		-
 *	s2:		rdsum/pass_countmax		-		-
 *	s3:		gsum/strmmax			-		-
 *	s4:		wrlim/stdmmax			-		-
 *	s5:		rdlim/clkdmax			-		-
 *	s6:		glim/clkdlim			-		-
 *	s7:		dll				-		-
 *	t8:	-	-				x		tmp
 *	t9:	-	-				x		retaddr
 *	k0:	trace	trace		trace		-		-
 *	k1:	trace	trace		trace		-		-
 *	gp:	PRESERVED
 *	sp:
 *	s8:	-	step				-		-
 *	ra:
 */


	.text
	.set	mips32

	LEAF(sb_draminit)
	.set	noreorder

	LATRACEINIT
	TRACEINIT(0x425300)

	/* Save return address */
	move	t6,ra

	/* Scan for a memory controller */
	move	a0,s2
1:	lw	v1,(SBCONFIGOFF + SBIDHIGH)(a0)
	and	a1,v1,SBIDH_CC_MASK
	srl	a1,a1,SBIDH_CC_SHIFT
	beq	a1,MEMC_CORE_ID,foundctrl
	nop
	beq	a1,SOCRAM_CORE_ID,foundctrl
	nop
	addu	a0,SI_CORE_SIZE
	bne	a1,(SBIDH_CC_MASK >> SBIDH_CC_SHIFT),1b
	nop

	/* No memory controller */
	li	v0,-1
	jr	t6
	nop

foundctrl:
	TRACE(0x425301)
	/* If we are already in RAM, just go and size it */
	bal	1f
	nop
1:	li	t0,PHYSADDR_MASK
	and	t0,t0,ra
	li	t1,SI_FLASH1
	blt	t0,t1,memprio_szmem
	nop

	/* For socram we don't need any nvram parms, just do a core reset */
socram_init:
	bne	a1,SOCRAM_CORE_ID,read_nvram
	nop
	TRACE(0x425302)
	bal	sb_core_reset
	li	a2,0
	/* and size memory */
	b	memprio_szmem
	nop

read_nvram:
	TRACE(0x425303)
	/* Find NVRAM (a2) */
	/* 1: Isolate memc's corerev in v1 */
	and	t2,v1,SBIDH_RCE_MASK
	srl	t2,t2,SBIDH_RCE_SHIFT
	and	v1,v1,SBIDH_RC_MASK
	or	v1,t2

	/* 1.5: 5365a0 lies about its revision, it is really 1 */
	bnez	v1,1f
	nop

	TRACE(0x425304)
	li	t0,KSEG1ADDR(SI_ENUM_BASE)	# Is there a chipcommon core?
	lw	t1,CC_CHIPID(t0)		# Get chipid
	andi	t1,CID_ID_MASK			# Check chipid
	bne	t1,BCM5365_CHIP_ID,1f
	nop
	li	v1,1

	/* 2: find_nvram, use the 32MB window */
1:	li	t2,KSEG1ADDR(SI_FLASH2 - MAX_NVRAM_SPACE)
	li	t4,SI_FLASH2_SZ
	li	t3,FLASH_MIN 
	li	t0,NVRAM_MAGIC

2:	add	a2,t2,t3
	lw	t1,0(a2)
	beq	t0,t1,read_parms
	nop

	TRACE(0x425306)
	sll	t3,t3,1
	ble	t3,t4,2b
	nop

	/* Try embedded NVRAM at 4 KB and 1 KB as last resorts */
	li	a2,KSEG1ADDR(SI_FLASH1 + 0x1000)
	lw	t1,0(a2)
	beq	t0,t1,read_parms
	nop

	TRACE(0x425307)
	li	a2,KSEG1ADDR(SI_FLASH1 + 0x400)
	lw	t1,0(a2)
	beq	t0,t1,read_parms
	nop

	TRACE(0x425308)
	b	memc_init			# No NVRAM
	li	a2, 0

read_parms:
	/* Get SDRAM parameters (t0, t1, t2) from NVRAM (a2) */
	TRACE(0x425309)
	lw	t0,8(a2)		# SDRAM init
	srl	t0,16
	lw	t2,12(a2)
	andi	t1,t2,0xffff		# SDRAM config
	srl	t2,16			# SDRAM refresh
	lw	t3,16(a2)		# SDRAM ncdl

memc_init:
	/* Initialize memc core */
	TRACE(0x425309)
	bnez	a2,1f			# Already have the parms in
	nop				#  t0, t1, t2, t3

	/* No nvram parms: assume DDRM16MX16X2 */

	li	t0,MEMC_DDR_INIT
	li	t1,MEMC_DDR_MODE
	li	t3,MEMC_DDR1_NCDL	# If rev1 (4712):
	beq	v1,1,1f
	nop
					# rev0, 2:
	li	t3,MEMC_DDR_NCDL

1:
	andi	a3,t0,MEMC_CONFIG_DDR	# Is it ddr or sdr?
	TRACE(0x42530a)
	beqz	a3,memc_sdr_init
	nop

	/* Initialize DDR SDRAM */
memc_ddr_init:
	beqz	t3,ddr_find_ncdl	# Do we have ncdl values? (0s)
	nop

	li	t4,-1			# or ffs
	bne	t3,t4,break_ddr_ncdl
	nop

ddr_find_ncdl:
	TRACE(0x42530b)

/* Register usage */
#define	pass_count	s0
#define	wrsum		s1
#define	rdsum		s2
#define	gsum		s3
#define	wrlim		s4
#define	rdlim		s5
#define	glim		s6
#define	dll		s7
#define	step		s8
#define	wr		t2
#define	rd		t3
#define	g		t4

	/* Initialize counter & accumulators */
	move	pass_count,zero
	move	wrsum,zero
	move	rdsum,zero
	move	gsum,zero

	/* Initialize with default values */
	li	wr,5
	li	rd,5
	bal	ddr_do_init
	li	g,10

	/* Read dll value */
	lw	dll,MEMC_NCDLCTL(a0)
	LATRACE(0xc, dll)
	andi	dll,dll,0xfe
	srl	dll,dll,1
	beqz	dll,memprio_szmem		/* If zero, leave the default values */
	nop

	move	wrlim,dll		/* dll value is lim for wr, rd and g */
	move	rdlim,dll
	move	glim,dll
	LATRACE(0xd, wrlim)
	LATRACE(0xe, rdlim)
	LATRACE(0xf, glim)

	addi	step,dll,15		/* step = (dll + 16 - 1) / 16 */
	srl	step,step,4
	LATRACE(0x10, step)

	sub	wr,zero,dll		/* Negate dll as initial value */
	move	rd,wr
	move	g,wr

	/* Inner loop:	call ddr_do_init to re-initialize and the test mem */
loop:
	LATRACE(0x11, wr)
	LATRACE(0x12, rd)
	LATRACE(0x13, g)	
	bal	ddr_do_init
	nop

	bal	test_mem
	nop

	LATRACE(0x14, v0)
	beqz	v0,nextg
	nop

	/* Memory is ok */

	addi	pass_count,1
	add	wrsum,wrsum,wr
	add	rdsum,rdsum,rd
	add	gsum,gsum,g
	LATRACE(0x15, pass_count)
	LATRACE(0x16, wrsum)
	LATRACE(0x17, rdsum)
	LATRACE(0x18, gsum)

	bne	wr,dll,1f
	nop
	sll	wrlim,dll,1
	LATRACE(0x19, wrlim)
1:

	bne	rd,dll,2f
	nop
	sll	rdlim,dll,1
	LATRACE(0x1a, rdlim)
2:

	bne	g,dll,nextg
	nop
	sll	glim,dll,1
	LATRACE(0x1b, glim)

nextg:
	add	g,g,step
	ble	g,glim,loop
	nop
	sub	g,zero,dll
	move	glim,dll
	LATRACE(0x1c, g)

	/* nextrd: */
	add	rd,rd,step
	ble	rd,rdlim,loop
	nop
	sub	rd,zero,dll
	move	rdlim,dll
	LATRACE(0x1d, rd)

	/* nextwr: */
	add	wr,wr,step
	ble	wr,wrlim,loop
	nop

	/* All done, calculate average values and program them */
	
	LATRACE(0x1e, pass_count)
	LATRACE(0x1f, wrsum)
	LATRACE(0x20, rdsum)
	LATRACE(0x21, gsum)
	beqz	pass_count,1f
	nop

	div	zero,wrsum,pass_count
	mflo	wr

	div	zero,rdsum,pass_count
	mflo	rd

	div	zero,gsum,pass_count
	mflo	g

	b	ddr_got_ncdl
	nop

	/* No passing values, panic! (use defaults) */
1:
	li	t3,MEMC_DDR1_NCDL	# If rev1:
	beq	v1,1,2f
	nop
					# rev0, 2:
	li	t3,MEMC_DDR_NCDL
2:

break_ddr_ncdl:
	TRACE(0x425322)
	andi	t4,t3,0xff		# t4:	g
	srl	t2,t3,16		# t2:	wr
	andi	t2,t2,0xff
	srl	t3,t3,8			# t3:	rd
	andi	t3,t3,0xff

ddr_got_ncdl:
	LATRACE(0x23, wr)
	LATRACE(0x24, rd)
	LATRACE(0x25, g)
	bal	ddr_do_init
	nop

	b	memprio_szmem
	nop

memc_sdr_init:
	beqz	t3,sdr_find_ncdl	# Do we have ncdl values?
	nop

	li	t4,-1
	bne	t3,t4,break_sdr_ncdl
	nop

sdr_find_ncdl:
	TRACE(0x425326)

/* Register usage */
#define	pass_count	s0
#define	clkdsum		s1
#define	pass_countmax	s2
#define	strmmax		s3
#define	strdmax		s4
#define	clkdmax		s5
#define	clkdlim		s6
#define	strm		t2
#define	strd		t3
#define	clkd		t4

#define	STRMLIM		4
#define	STRDLIM		16
#define	CLKDLIM		128
#define	CLKDLIM_IC	256

	/* Initialize counter & saved values */
	move	pass_countmax,zero
	move	strmmax,zero
	move	strdmax,zero
	li	clkdlim,CLKDLIM

	and	strm,t0,0x2000		/* Test for internal clock (Using strm as a temp) */
	beqz	strm,strmloop
	nop

	li	clkdlim,CLKDLIM_IC

	move	strm,zero		/* strm loop */
strmloop:
	move	strd,zero
strdloop:
	move	pass_count,zero
	move	clkdsum,zero
	move	clkd,zero

	/* Inner loop:	call sdr_do_init to re-initialize and the test mem */
clkdloop:
	LATRACE(0x27, strm)
	LATRACE(0x28, strd)
	LATRACE(0x29, clkd)	
	bal	sdr_do_init
	nop

	bal	test_mem
	nop

	LATRACE(0x2a, v0)
	beqz	v0,failclkd
	nop

	/* Memory is ok */

	addi	pass_count,1
	add	clkdsum,clkdsum,clkd
	LATRACE(0x2b, pass_count)
	LATRACE(0x2c, clkdsum)
	b	nextclkd
	nop

failclkd:
	bnez	pass_count,clkdout	# End of passing range, leave clkd loop
	nop

nextclkd:
	addi	clkd,clkd,1
	blt	clkd,clkdlim,clkdloop
	nop

clkdout:
	/* If no passing values, skip to next strm */
	beqz	pass_count,nextstrm
	nop

	/* If this is a new max, Save the values */
	ble	pass_count,pass_countmax,nextstrd
	nop

	move	pass_countmax,pass_count
	div	zero,clkdsum,pass_count
	mflo	clkdmax
	move	strdmax,strd
	move	strmmax,strm
	LATRACE(0x2d, pass_count)
	LATRACE(0x2e, clkdmax)
	LATRACE(0x2f, strdmax)
	LATRACE(0x30, strmmax)

nextstrd:
	addi	strd,strd,1
	blt	strd,STRDLIM,strdloop
	nop

nextstrm:
	addi	strm,strm,1
	blt	strm,STRMLIM,strmloop
	nop

	/* All done, program the new ncdl values */
	
	LATRACE(0x31, pass_count)
	LATRACE(0x32, clkdmax)
	LATRACE(0x33, strdmax)
	LATRACE(0x34, strmmax)

	beqz	pass_countmax,1f
	nop

	move	clkd,clkdmax
	move	strd,strdmax
	move	strm,strmmax
	b	sdr_got_ncdl
	nop

	/* No passing values, panic! (use defaults) */
1:
	li	t3,MEMC_SDR1_NCDL	# If rev1:
	beq	v1,1,2f
	nop
					# rev0, 2:
	li	t3,MEMC_SDR_NCDL
2:

break_sdr_ncdl:
	TRACE(0x425335)
	andi	t4,t3,0xff		# t4:	cd
	srl	t2,t3,16		# t2:	sm
	andi	t2,t2,3			#	sm is 2 bits only
	srl	t3,t3,8			# t3:	sd
	andi	t3,t3,0xf		#	sd is 4 bits

sdr_got_ncdl:
	bal	sdr_do_init
	nop

	/* Change the memory priority inversion counter value */
	/* If an SOCRAM, determine memory size and return */
memprio_szmem:
	TRACE(0x425337)

#ifdef APPLE
	lw	t0, MEMC_PRIORINV(a0) 
	li 	t1, 0xFFFF0000
	and     t0, t0, t1
	ori	t0, t0, 0x1
	sw      t0, MEMC_PRIORINV(a0)
#else
	li	t0,KSEG1ADDR(SI_ENUM_BASE)	# is there a chipcommon core?
	lw	t1,(SBCONFIGOFF + SBIDHIGH)(t0)
	and	t1,t1,SBIDH_CC_MASK
	srl	t1,t1,SBIDH_CC_SHIFT
	bne	t1,CC_CORE_ID,0f
	nop
	lw	t1,CC_CHIPID(t0)		# is this BCM4785 chip?
	and	t1,t1,CID_ID_MASK
	bne	t1,BCM4785_CHIP_ID,0f
	nop
	lw	t0,MEMC_PRIORINV(a0)		# change PriorInvTim to 2
	and     t0,t0,0xFFFF0000
	ori	t0,t0,0x02
	sw      t0,MEMC_PRIORINV(a0)
#endif
0:	TRACE(0x425338)
	lw	t0,(SBCONFIGOFF + SBIDHIGH)(a0)
	and	t1,t0,SBIDH_CC_MASK
	srl	t1,t1,SBIDH_CC_SHIFT
	beq	t1,SOCRAM_CORE_ID,1f
	nop
	jr	t6
	li	v0,0

	/* The socram core tells us how much memory there is */
1:	TRACE(0x425339)
	lw	t1,SR_COREINFO(a0)
	and	t0,t0,SBIDH_RC_MASK		/* Find corerev */
	beq	t0,zero,crev0

	/* Its corerev >= 1 */
	and	t2,t1,SRCI_SRNB_MASK		/* Find number of blocks */
	srl	t2,t2,SRCI_SRNB_SHIFT
	and	t1,t1,SRCI_SRBSZ_MASK		/* Find block size */
	addi	t1,t1,SR_BSZ_BASE
	li	t0,1
	sll	t0,t0,t1
	mul	v0,t0,t2
	jr	t6
	nop

crev0:
	and	t1,t1,SRCI_MS0_MASK
	add	t1,t1,SR_MS0_BASE
	li	v0,1
	sll	v0,v0,t1
	jr	t6
	nop

	/*
	 * Test memory
	 *
	 * Uses arg in t2(wr/sd), t3(rd/sm) and t4(g/clkd)
	 * Returns success (1) or failure (0) in v0
	 * Uses a1, a2, a3 & t5
	 */
test_mem:
	/* Use t4 to generate a semi-random address in the second KB */
	li	a1,KSEG1
	addi	a2,t4,255
	sll	a2,a2,2
	add	a1,a1,a2

	/* First set: 0 & its negation */
	li	a2,0
	sw	a2,0(a1)
	not	a3,a2
	sw	a3,4(a1)
	nop
	lw	t5,0(a1)
	bne	a2,t5,bad_mem
	nop

	lw	t5,4(a1)
	bne	a3,t5,bad_mem
	nop

	/* Second set: 0xaaaaaaaa & its negation */
	li	a2,0xaaaaaaaa
	sw	a2,0(a1)
	not	a3,a2
	sw	a3,4(a1)
	nop
	lw	t5,0(a1)
	bne	a2,t5,bad_mem
	nop

	lw	t5,4(a1)
	bne	a3,t5,bad_mem
	nop

	/* Third set: 0x12345678 & its negation */
	li	a2,0x12345678
	sw	a2,0(a1)
	not	a3,a2
	sw	a3,4(a1)
	nop
	lw	t5,0(a1)
	bne	a2,t5,bad_mem
	nop

	lw	t5,4(a1)
	bne	a3,t5,bad_mem
	nop

	/* Fourth set: the ncdl & its negation */
	sll	a2,t2,8
	or	a2,t3
	sll	a2,a2,8
	or	a2,t4
	sw	a2,0(a1)
	not	a3,a2
	sw	a3,4(a1)
	nop
	lw	t5,0(a1)
	bne	a2,t5,bad_mem
	nop

	lw	t5,4(a1)
	bne	a3,t5,bad_mem
	nop

	/* Fifth set: the CPU count register & its negation */
	mfc0	a2,$9
	sw	a2,0(a1)
	not	a3,a2
	sw	a3,4(a1)
	nop
	lw	t5,0(a1)
	bne	a2,t5,bad_mem
	nop

	lw	t5,4(a1)
	bne	a3,t5,bad_mem
	nop

	jr	ra
	li	v0,1

bad_mem:
	jr	ra
	li	v0,0


	/* Do an init of the memc core for ddr
	 *	a0:	memc core pointer
	 *	t0:	memc config value
	 *	t1:	memc mode value
	 *	t2:	memc wr ncdl value
	 *	t3:	memc rd ncdl value
	 *	t4:	memc g ncdl value
	 *
	 * Uses a1, a2, t7, t8, t9 (here and by calling sb_core_reset)
	 */
ddr_do_init:
	TRACE(0x42533a)

	/* Save return address */
	move	t7,ra

	bal	sb_core_reset
	li	a1,0

	li	a1,MEMC_CONFIG_INIT
	or	a1,a1,t0
	lui	a2, 0x8			# set DQMGate for memc rev 4 or more
	or	a1, a1, a2
	sw	a1,MEMC_CONFIG(a0)

	li	a1,MEMC_DRAMTIM25_INIT		# Assume CAS latency of 2.5
	andi	t8,t1,0xf0			# Find out the CAS latency
	bne	t8,0x20,1f
	nop
	li	a1,MEMC_DRAMTIM2_INIT		# CAS latency is 2
1:	
	sw	a1,MEMC_DRAMTIM(a0)

#ifndef	BCM_ATE
	li	t8,KSEG1ADDR(SI_ENUM_BASE)	# Get package options
	lw	a1,CC_CHIPID(t8)		# Get chipid
	li	t9,CID_PKG_MASK			# Check package options
	and	a1,a1,t9
	srl	a1,a1,CID_PKG_SHIFT
	addi	t8,SBCONFIGOFF			# Get corerev for chipcommon
	lw	a2,SBIDHIGH(t8)
	li	t8,SBIDH_RCE_MASK
	and	t8,t8,a2
	srl	t8,SBIDH_RCE_SHIFT
	li	t9,SBIDH_RC_MASK
	and	t9,t9,a2
	or	t8,t8,t9
	bge	t8,10,1f			# If ccrev is >= 10 use 4bit pkg opt
	nop
	ori	a1,8				#  else add a bit to the 3bit field
1:
	beq	a1,HDLSIM_PKG_ID,hdlsim		# Special case for hdl sim:
	nop

	li	t8,KSEG1ADDR(SI_ENUM_BASE)	# Get chipid again
	lw	a2,CC_CHIPID(t8)
	li	t9,BCM5350_CHIP_ID		# 5350 ChipID
	li	t8,CID_ID_MASK
	and	t8,t8,a2
	bne	t8,t9,notsim			# if not 5350keep going
	nop

	bne	a1,(8 | HDLSIM5350_PKG_ID),notsim	# If 5350, is it (3/4-bit) vsim?
	nop
#endif	/* !BCM_ATE */

hdlsim:
	li	a1,MEMC_RDNCDLCOR_SIMINIT	#  Fixed 0xf6 rdncdl and no inits
	sw	a1,MEMC_RDNCDLCOR(a0)		#  of wrncdl, dqsgate and miscdly.

#ifndef	BCM_ATE
	b	simskip
	nop

notsim:	andi	t8,t3,0xff
	sll	a1,t8,8				# Replicate rd ncdl 4 times
	or	a1,a1,t8
	sll	t8,a1,16
	or	t8,t8,a1
	li	a1,MEMC_RDNCDLCOR_INIT
	or	a1,a1,t8
	sw	a1,MEMC_RDNCDLCOR(a0)

	li	a1,MEMC_1_WRNCDLCOR_INIT # If rev1:
	beq	v1,1,1f
	nop
					# rev0, 2
	li	a1,MEMC_WRNCDLCOR_INIT
1:
	andi	t8,t2,0xff
	or	a1,a1,t8
	sw	a1,MEMC_WRNCDLCOR(a0)

	li	a1,MEMC_DQSGATENCDL_INIT
	andi	t8,t4,0xff
	or	a1,a1,t8
	sw	a1,MEMC_DQSGATENCDL(a0)

	li	a1,MEMC_1_MISCDLYCTL_INIT # If rev1:
	beq	v1,1,2f
	nop
					# rev0,2
	li	a1,MEMC_MISCDLYCTL_INIT
2:
	sw	a1,MEMC_MISCDLYCTL(a0)
#endif	/* !BCM_ATE */

simskip:
	li	a1,MEMC_NCDLCTL_INIT
	sw	a1,MEMC_NCDLCTL(a0)

	li	a1,MEMC_CONTROL_INIT0
	sw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_CONTROL_INIT1
	sw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_MODEBUF_INIT0
	sw	a1,MEMC_MODEBUF(a0)

	li	a1,MEMC_CONTROL_INIT2
	sw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_MODEBUF_INIT1
	or	a1,a1,t1
	sw	a1,MEMC_MODEBUF(a0)

	li	a1,MEMC_CONTROL_INIT3
	sw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_CONTROL_INIT4
	sw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_CONTROL_INIT5
	sw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_CONTROL_INIT5
	sw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_REFRESH_INIT
	sw	a1,MEMC_REFRESH(a0)
	
	li	a1,MEMC_MODEBUF_INIT2
	or	a1,a1,t1
	sw	a1,MEMC_MODEBUF(a0)

	li	a1,MEMC_CONTROL_INIT6
	sw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_CONTROL_INIT7
	sw	a1,MEMC_CONTROL(a0)

	/* Wait for SDRAM controller to refresh.
	 * We want 8uS delay.
	 */
	li	t8,50
1:	lw	a1,(SBCONFIGOFF + SBIDLOW)(a0)
	lw	a1,(SBCONFIGOFF + SBIDHIGH)(a0)

	bnez	t8,1b
	subu	t8,1

	jr	t7
	nop

	/* Do an init of the memc core for sdr
	 *	a0:	memc core pointer
	 *	t0:	memc config value
	 *	t1:	memc mode value
	 *	t2:	memc strobe mode ncdl value
	 *	t3:	memc strobe delay ncdl value
	 *	t4:	memc clock delay ncdl value
	 *
	 * Uses a1, t7, t8, t9 (here and by calling sb_core_reset)
	 */
sdr_do_init:
	TRACE(0x42533b)

	/* Save return address */
	move	t7,ra

	bal	sb_core_reset
	li	a1,0x40

	/* Initialize for SDR SDRAM */
	li	a1,MEMC_SD_CONFIG_INIT
	or	a1,a1,t0
	sw	a1,MEMC_CONFIG(a0)

	li	a1,MEMC_SD_DRAMTIM3_INIT # Assume CAS latency of 3
	andi	t8,t1,0xf0		# Find out the CAS latency
	bne	t8,0x20,1f
	nop
	li	a1,MEMC_SD_DRAMTIM2_INIT # CAS latency is 2
1:	
	sw	a1,MEMC_DRAMTIM(a0)

	andi	t8,t4,0xff
	ble	t8,MEMC_CD_THRESHOLD,1f	# if (cd <= MEMC_CD_THRESHOLD) rd = cd
	nop

	li	t8,MEMC_CD_THRESHOLD	# else rd = MEMC_CD_THRESHOLD

1:					# t8 is now rd
	sll	a1,t8,8			#  .. replicate it 4 times
	or	a1,a1,t8
	sll	t8,a1,16
	or	t8,t8,a1
	li	a1,MEMC_SD_RDNCDLCOR_INIT
	or	a1,a1,t8
	sw	a1,MEMC_RDNCDLCOR(a0)

	li	a1,MEMC_SD1_WRNCDLCOR_INIT # rev1
	beq	v1,1,1f
	nop

	li	a1,MEMC_SD_WRNCDLCOR_INIT # rev0, 2
1:
	li	t8,0
	ble	t4,MEMC_CD_THRESHOLD,2f	# if (cd <= MEMC_CD_THRESHOLD) wr = 0
	nop
	
	andi	t8,t4,0xff		# else wr = cd - MEMC_CD_THRESHOLD
	sub	t8,t8,MEMC_CD_THRESHOLD
	andi	t8,t8,0xff

2:					# t8 is now wr, a0 is extra bits
	or	a1,a1,t8
	sw	a1,MEMC_WRNCDLCOR(a0)

	andi	t8,t2,3
	sll	a1,t8,28
	andi	t8,t3,0xf
	sll	t8,t8,24
	or	t8,t8,a1
	li	a1,MEMC_SD1_MISCDLYCTL_INIT
	beq	v1,1,3f			# If rev1:
	nop
					# rev0, 2:
	li	a1,MEMC_SD_MISCDLYCTL_INIT
3:
	or	a1,a1,t8
	sw	a1,MEMC_MISCDLYCTL(a0)

	li	a1,MEMC_SD_CONTROL_INIT0
	sw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_SD_CONTROL_INIT1
	sw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_SD_CONTROL_INIT2
	sw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_SD_CONTROL_INIT2
	sw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_SD_CONTROL_INIT2
	sw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)
	lw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_SD_REFRESH_INIT
	sw	a1,MEMC_REFRESH(a0)

	li	a1,MEMC_SD_MODEBUF_INIT
	or	a1,a1,t1
	sw	a1,MEMC_MODEBUF(a0)

	li	a1,MEMC_SD_CONTROL_INIT3
	sw	a1,MEMC_CONTROL(a0)

	li	a1,MEMC_SD_CONTROL_INIT4
	sw	a1,MEMC_CONTROL(a0)

	li	t8,50
1:	lw	a1,(SBCONFIGOFF + SBIDLOW)(a0)
	lw	a1,(SBCONFIGOFF + SBIDHIGH)(a0)
	bnez	t8,1b
	subu	t8,1

	jr	t7
	nop


	/* Special sb_core_reset that makes sure the first time
	 * clock is enabled, address line 6 is in the state specified
	 * by a1.
	 *
	 * a0:	Core pointer
	 * a1:	0x40 if a6 needs to be 1, 0 otherwise
	 * uses t8, t9
	 */

	.align 6

sb_core_reset:

	/* Save return address */
	move	t9,ra
        
	/* run uncached */
	bal     kseg1_switch
	nop                                

	/* Figure out our address */
	bal	h0
	nop
h0:	add	t8,ra,24		# This is (h1 - h0)
	andi	t8,t8,0x40
	bne	t8,a1,alt_core_reset
	nop

	/* Set reset while enabling the clock */
	li	t8,(((SICF_FGC | SICF_CLOCK_EN) << SBTML_SICF_SHIFT) | SBTML_RESET)
h1:	sw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)
	b	cont
	nop

	/* Now pad to 0x40: We want (h2 - h1) == 0x40 and there
	 * are 5 instructions inbetween them.
	 */
	.space	(0x40 - 20)

alt_core_reset:
	/* Set reset while enabling the clock */
	li	t8,(((SICF_FGC | SICF_CLOCK_EN) << SBTML_SICF_SHIFT) | SBTML_RESET)
h2:	sw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)

cont:
	/* Read back and delay */
	lw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)
	lw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)
	lw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)

	/* Clear reset */
	li	t8,((SICF_FGC | SICF_CLOCK_EN) << SBTML_SICF_SHIFT)
	sw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)

	/* Read back and delay */
	lw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)
	lw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)
	lw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)

	/* Leave clock enabled */
	li	t8,(SICF_CLOCK_EN << SBTML_SICF_SHIFT)
	sw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)

	/* Read back and delay */
	lw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)
	lw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)
	lw	t8,(SBCONFIGOFF + SBTMSTATELOW)(a0)

	jr	t9
	nop

        
kseg1_switch:
	and     ra,ra,PHYSADDR_MASK
	or      ra,ra,KSEG1
	jr      ra
	nop 
        
	.set	reorder
	END(sb_draminit)
