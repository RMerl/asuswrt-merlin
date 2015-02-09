/*  *********************************************************************
    *  P5064 Board Support Package
    *  
    *  CPU initialization			File: rm5200_cpuinit.S
    *  
    *  This module contains code to initialize the CPU.
    *  
    *  Note: all the routines in this module rely on registers only,
    *        since DRAM may not be active yet.
    *
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

#include "sbmips.h"
#include "bsp_config.h"
#include "mipsmacros.h"
#include "exception.h"


		.text
		.set mips64


/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define CFG_BE		0x00008000	/* Big Endian */
#define CFG_EPMASK	0x0f000000	/* Transmit data pattern */
#define CFG_EPD		0x00000000	/* D */
#define NTLBENTRIES 64

/*
 * Duplicates from cfe_iocb.h -- warning!
 */

#define CFE_CACHE_FLUSH_D	1
#define CFE_CACHE_INVAL_I	2
#define CFE_CACHE_INVAL_D	4
#define CFE_CACHE_INVAL_L2	8
#define CFE_CACHE_FLUSH_L2	16

/*  *********************************************************************
    *  Linkage tables
    ********************************************************************* */

#define R_CPU_CP0INIT	_TBLIDX(0)
#define R_CPU_L1CINIT	_TBLIDX(1)
#define R_CPU_SETLEDS	_TBLIDX(2)

cpuinit_table:
		_LONG_  rm5200_cp0_init		# [  0] R_CPU_CP0INIT
		_LONG_  rm5200_l1cache_init	# [  1] R_CPU_L1CINIT
		_LONG_  board_setleds		# [  2] R_CPU_SETLEDS

#define SETLEDS1(a,b,c,d)                     \
       li     a0,(((a)<<24)|((b)<<16)|((c)<<8)|(d)) ;    \
       CALLINIT_KSEG1(cpuinit_table,R_CPU_SETLEDS)
#define SETLEDS(a,b,c,d)                     \
       li     a0,(((a)<<24)|((b)<<16)|((c)<<8)|(d)) ;    \
       CALLINIT_KSEG0(cpuinit_table,R_CPU_SETLEDS)



/*  *********************************************************************
    *  RM5200_CP0_INIT
    *  
    *  Initialize CP0 registers for an RM5200 core.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(rm5200_cp0_init)

	/*
	 * Set SR and CAUSE to something sensible.
	 */
		
		.set noreorder
		mfc0	t0,C0_PRID
		mfc0	v0,C0_SR
		mtc0	zero,C0_WATCHLO
		mtc0	zero,C0_WATCHHI
		li	t1,-1
		mtc0	t1,C0_COMPARE
		and	v0,M_SR_SR		# preserve Soft Reset
		or	v0,M_SR_BEV		# set Boot Exceptions
		mtc0	zero,C0_CAUSE		# Must clear WP before
		mtc0	v0,C0_SR		# writing STATUS register.
		.set reorder

	
		mfc0	t1,C0_CONFIG

		mtc0	zero,C0_WATCHLO
		mtc0	zero,C0_WATCHHI

	/* 
	 * make KSEG0 cacheable 
	 */

	 	and	t1,~M_CFG_K0COH
		or	t1,V_CFG_K0COH(K_CFG_K0COH_CACHEABLE)
	
	/* 
	 * set DDDD rate for CPUs that aren't hardware configured 
 	 */

	 	and	t1,~CFG_EPMASK
		or	t1,CFG_EPD

#ifdef __MIPSEB
		or	t1,CFG_BE	
#else
		and	t1,~CFG_BE	
#endif
		mtc0	t1,C0_CONFIG


	/* 
	 * initialize tlb 
	 */
	 	mtc0	zero,C0_TLBLO0		/* tlblo0 = invalid */
		mtc0	zero,C0_TLBLO1		/* tlblo1 = invalid */
		mtc0	zero,C0_PGMASK
		li	t8,K1BASE		/* tlbhi  = impossible vpn */
		li	t9,(NTLBENTRIES-1)	/* index */
		

		.set noreorder
		nop
1:		mtc0	t8,C0_TLBHI
		mtc0	t9,C0_INX
		addu	t8,0x2000		/* inc vpn */
		tlbwi
		bnez	t9,1b
		subu	t9,1			# BDSLOT
		.set reorder

		j	ra

END(rm5200_cp0_init)


/*  *********************************************************************
    *  RM5200_NULL_INIT
    *  
    *  This stub routine is used by initialization functions that
    *  we leave unimplemented.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(rm5200_null_init)
		j	ra
END(rm5200_null_init)



/*  *********************************************************************
    *  RM5200_CPUINIT
    *  
    *  Initialize the CPU core.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(rm5200_cpuinit)

		move	k0,ra			/* will be trashing RA */

	/*
	 * Basic CPU initialization
	 */

		CALLINIT_KSEG1(cpuinit_table,R_CPU_CP0INIT)

	/*
	 * CP0 registers
	 */


#------------------------------------------------------------------------------

	/*
	 * Init the L1 cache.  
	 */

#if CFG_INIT_L1
		SETLEDS1('L','1','C','I')
		CALLINIT_KSEG1(cpuinit_table,R_CPU_L1CINIT)
#endif

	/*
         * There is no L2 cache.
         */

		move	ra,k0			/* saved return address */
		j	ra


END(rm5200_cpuinit)

/*  *********************************************************************
    *  RM5200_CPURESTART
    *  
    *  'Restart' the CPU (reset things back to some sane state after
    *  a program returns to the firmware)
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(rm5200_cpurestart)

		move	k0,ra

		CALLINIT_KSEG0(cpuinit_table,R_CPU_CP0INIT)

		LR	v0,cfe_pagetable		# reestablish 
		dsll	v0,v0,13			# see mips_arena.c for this
		dmtc0	v0,C0_CTEXT			# boot area TLBs

		move	ra,k0
		j	ra

END(rm5200_cpurestart)


LEAF(rm5200_cacheops)

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

		and	a0,v1,CFE_CACHE_FLUSH_D
		beq	a0,zero,1f
		jal	rm5200_l1cache_flush_d
1:

	/*
	 * Invalidate the I-Cache, so that addresses in the program
	 * region will miss and need to be filled from the data we 
	 * just flushed above.
	 */

		and	a0,v1,CFE_CACHE_INVAL_I
		beq	a0,zero,1f
		jal	rm5200_l1cache_inval_i
1:


		move	ra,s0
		j	ra

END(rm5200_cacheops)

/*  *********************************************************************
    *  RM5200_TLBHANDLER
    *  
    *  This is the TLB exception handler for the RM5200
    *  
    *  Note: only K0 and K1 are available to us at this time.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */


LEAF(rm5200_tlbhandler)
                .set    noreorder
                .set    noat

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
		dmfc0	k0,C0_TLBHI

		dmfc0	k0,C0_CTEXT		# Get context
		dsra	k0,8			# keep hi part
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

1:		dmfc0	k0,C0_CTEXT		# Get context
		dsra	k0,13			# Shift PTEbase 
		li	k1,0x3FF		# Generate mask to kill 
		not	k1			# BadVPN2 bits
		and	k0,k1			# keep only PTEBase part.

		dmfc0	k1,C0_CTEXT		# Get Context
		and	k1,0x3F0		# Keep only BadVPN2 bits
		or	k1,k0			# Replace PTEBase

		ld	k0,0(k1)		# Load entrylo0
		ld	k1,8(k1)		# Load entrylo1
		mtc0	k0,C0_TLBLO0		# and write to CP0
		mtc0	k1,C0_TLBLO1
		tlbwr				# put it in the TLB
		eret
		nop

		.set	reorder
		.set	at

END(rm5200_tlbhandler)

/*  *********************************************************************
    *  End
    ********************************************************************* */
