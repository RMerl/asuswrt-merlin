/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  CPU initialization			File: bcmcore_cpuinit.S
    *  
    *  This module contains code to initialize the CPU cores.
    *  
    *  Note: all the routines in this module rely on registers only,
    *        since DRAM may not be active yet.
    *
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  XX Copyright 2000,2001
    *  Broadcom Corporation. All rights reserved.
    *
    *  BROADCOM PROPRIETARY AND CONFIDENTIAL
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the license.
    ********************************************************************* */

#include "sbmips.h"
#include "exception.h"
#include "bsp_config.h"
#include "mipsmacros.h"
#include "cpu_config.h"			/* for ERET and HAZARD */
#include "cfe_iocb.h"


		.text

		.set mips32

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define R_CPU_CP0INIT _TBLIDX(0)
#define R_CPU_L1CINIT _TBLIDX(1)
#define R_CPU_SETLEDS _TBLIDX(2)

#define SETLEDS1(a,b,c,d)                     \
       li     a0,(((a)<<24)|((b)<<16)|((c)<<8)|(d)) ;    \
       CALLINIT_KSEG1(cpuinit_table,R_CPU_SETLEDS)
#define SETLEDS(a,b,c,d)                     \
       li     a0,(((a)<<24)|((b)<<16)|((c)<<8)|(d)) ;    \
       CALLINIT_KSEG0(cpuinit_table,R_CPU_SETLEDS)

cpuinit_table:
		_LONG_	bcmcore_cp0_init	# [ 0 ] R_CPU_CP0INIT
		_LONG_	bcmcore_l1cache_init    # [ 1 ] R_CPU_L1CINIT
		_LONG_  board_setleds		# [ 2 ] R_CPU_SETLEDS


/*  *********************************************************************
    *  BCMCORE_CP0_INIT()
    *  
    *  Initialize an BCMCORE CPU's CP0 registers
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   all
    ********************************************************************* */

	
LEAF(bcmcore_cp0_init)

		.set	noreorder

		mtc0	zero,C0_WATCHLO		# Watch registers.
		mtc0	zero,C0_WATCHHI
		mtc0	zero,C0_CAUSE		# must clear before writing SR

		mfc0	v0,C0_SR		# Get status register
		and	v0,M_SR_SR		# preserve soft reset
#ifdef DEBUG_ENV_ICE
		and	v0,~M_SR_BEV
#else
		or	v0,M_SR_BEV		# exceptions to boot vector
#endif
		mtc0	v0,C0_SR		# set up the status register

		mfc0	v0,C0_PRID
		and	v0,0x00ffff00
		bne	v0,0x00029000,1f	# Broadcom BCM330X
		nop
		mfc0	v0,C0_DIAGNOSTIC
		or	v0,0x80000000		# Enable icache
		or	v0,0x40000000		# Enable dcache
		mtc0	v0,C0_DIAGNOSTIC
	
1:		bne	v0,0x00019700,2f	# 74k core
		mfc0	v0,C0_CONFIG,7
		or	v0,(1 << 29)
		mtc0	v0,C0_CONFIG,7

2:		mtc0	zero,C0_COUNT

	#
	# This is probably not the right init value for C0_COMPARE,
	# but it seems to be necessary for the sim model right now.
	#

		li	v0,-1
		mtc0	v0,C0_COMPARE
		nop

	#
	# Initialize all the TLB entries to some invalid value
	#

		mtc0	zero,C0_TLBHI		# TLB entry (high half)
		nop

		mtc0	zero,C0_TLBLO0		/* tlblo0 = invalid */
		nop
		mtc0	zero,C0_TLBLO1		/* tlblo1 = invalid */
		nop
		mtc0	zero,C0_PGMASK		/* 4K pages */
		nop

		li	t0,K1BASE		/* tlbhi  = impossible vpn */
		mfc0	v0,C0_CONFIG,1
		li	t1,M_CFG_MMUSIZE
		and	t1,v0
		srl	t1,S_CFG_MMUSIZE	/* index */
	

		nop
1:		mtc0	t0,C0_TLBHI
		nop
		mtc0	t1,C0_INX
		nop
		addu	t0,0x2000		/* inc vpn */
		tlbwi
		bnez	t1,1b
		subu	t1,1			# BDSLOT
		.set reorder



		jr	ra


END(bcmcore_cp0_init)


/*  *********************************************************************
    *  BCMCORE_CPUINIT
    *  
    *  Do initialization of the Broadcom core
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */


LEAF(bcmcore_cpuinit)

		move	fp,ra

		SETLEDS1('C','P','U','I')
		CALLINIT_KSEG1(cpuinit_table,R_CPU_CP0INIT)
#if  CFG_INIT_L1 > 0
		SETLEDS1('L','1','C','I')
		CALLINIT_KSEG1(cpuinit_table,R_CPU_L1CINIT)
#endif 
		move	ra,fp
		j	ra

END(bcmcore_cpuinit)


/*  *********************************************************************
    *  BCMCORE_NULL
    *  
    *  Dummy handler for routines we don't need to implement, like
    *  the multiprocessor stuff
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   none
    ********************************************************************* */

LEAF(bcmcore_null)

		j	ra

END(bcmcore_null)


/*  *********************************************************************
    *  BCMCORE_CPURESTART
    *  
    *  This routine is called when someone soft-exits to CFE.  We
    *  reinitialize any CP0 stuff here.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(bcmcore_cpurestart)

		j	ra

END(bcmcore_cpurestart)


/*  *********************************************************************
    *  BCMCORE_CACHEOPS
    *  
    *  Perform various cache operations on a BCM Core
    *  
    *  Input parameters: 
    *  	   a0 - flag bits (CFE_CACHE_xxx)
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   t0,t1,t2,t3,v1,s0
    ********************************************************************* */

LEAF(bcmcore_cacheops)

		move	s0,ra

		move	v1,a0

	/*
	 * With no flags, we flush L1D and invalid L1I
	 */

		bne	v1,zero,1f
		li	v1,CFE_CACHE_FLUSH_D | CFE_CACHE_INVAL_I
1:

	/*
	 * Flush the D-Cache, since the program we loaded is "data".
	 */

		and	a0,v1,CFE_CACHE_FLUSH_D | CFE_CACHE_INVAL_D | CFE_CACHE_FLUSH_RANGE | CFE_CACHE_INVAL_RANGE
		beq	a0,zero,1f
		jal	bcmcore_l1cache_flush_d
1:

	/*
	 * Invalidate the I-Cache, so that addresses in the program
	 * region will miss and need to be filled from the data we 
	 * just flushed above.
	 */

		and	a0,v1,CFE_CACHE_INVAL_I
		beq	a0,zero,1f
		jal	bcmcore_l1cache_inval_i
1:

		move	ra,s0
		j	ra

END(bcmcore_cacheops)



/*  *********************************************************************
    *  BCMCORE_TLBHANDLER
    *  
    *  This is the TLB exception handler for the bcmcore
    *  
    *  Note: only K0 and K1 are available to us at this time.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */


LEAF(bcmcore_tlbhandler)
                .set    noreorder
                .set    noat

#ifdef BCM4704        
		nop           
#endif                              


/*
 * This requires a bit of explanation:  We only support 256KB
 * of mapped space for the boot program.  This space will be 
 * mapped from 0x2000_0000 to 0x2004_0000 to some physical 
 * memory allocated by the firmware.  This is 64 pages
 * of 4KB each.
 *
 * We know our BadVPN2 will be in the range
 * 0x100000 to 0x1001F0, since the memory is mapped from
 * 0x2000_0000 to 0x2004_0000.  BadVPN2 plus the four bits
 * of zeroes at the end are bits 31..9
 * 
 * We also want to place the PTEbase on something other than
 * a 16MB boundary.  Each entry is 16 bytes, and there
 * are 64 entries, so we need only 10 bits to address
 * the entire table (it can therefore be aligned on a
 * 1KB boundary).
 *
 * To make this work, we'll shift PTEbase to the right, leaving
 * the bottom ten bits for the page number, as:
 *
 *    Bits 31..10: PTEbase
 *    Bits 9..4:   BadVPN
 *    Bits 3..0:   16 bytes for table entry
 *
 * Therefore:
 *    PTEbase gets shifted right 13 bits.
 *    BadVPN  gets masked at 6 bits (mask is 0x3F0)
 *    The bottom 4 bits are zero.
 *
 * To range check the address, we can shift the Bad VPN
 * right by 9 bits, and check for values of 0x1000 and
 * 0x1001.
 */


	/*
	 * This part range checks the VPN2 field in the 
	 * context register.  We only handle
	 * VPN2s in the range 0x100000 to 0x1001F0
	 */
		mfc0	k0,C0_TLBHI

		mfc0	k0,C0_CTEXT		# Get context
		sra	k0,8			# keep hi part
		and	k0,0x1FFF		# of VPN2
		li	k1,0x1000		# 0x1000 is ok
		beq	k0,k1,1f		#
		nop				# BDSLOT
		li	k1,0x1001		# 0x1001 is ok
		beq	k0,k1,1f		#
		nop				# BDSLOT

		li	k0,XTYPE_TLBFILL	# all other bits are not
		j	_exc_entry
		nop				# BDSLOT

1:		mfc0	k0,C0_CTEXT		# Get context
		sra	k0,13			# Shift PTEbase 
		li	k1,0x3FF		# Generate mask to kill 
		not	k1			# BadVPN2 bits
		and	k0,k1			# keep only PTEBase part.

		mfc0	k1,C0_CTEXT		# Get Context
		and	k1,0x3F0		# Keep only BadVPN2 bits
		or	k1,k0			# Replace PTEBase

		ld	k0,0(k1)		# Load entrylo0
		ld	k1,8(k1)		# Load entrylo1
		mtc0	k0,C0_TLBLO0		# and write to CP0
		mtc0	k1,C0_TLBLO1
		tlbwr				# put it in the TLB
		ERET
		nop

		.set	reorder
		.set	at

END(bcmcore_tlbhandler)


/*  *********************************************************************
    *  End
    ********************************************************************* */
