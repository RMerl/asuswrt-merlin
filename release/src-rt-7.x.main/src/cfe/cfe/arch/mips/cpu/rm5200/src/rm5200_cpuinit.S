/*  *********************************************************************
    *  P5064 Board Support Package
    *  
    *  CPU initialization			File: rm5200_cpuinit.S
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


		.text
		.set mips64


/*  *********************************************************************
    *  Macros
    ********************************************************************* */


/*  *********************************************************************
    *  RM5200_ZERO_INT_REGS
    *  
    *  Zero all the CPU's integer registers *except* FP, which we're
    *  using within cpu_init to hold its return address.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing - all registers (except ra/R31) zero
    ********************************************************************* */

LEAF(rm5200_zero_int_regs)

		.set	noat
		move	AT,zero
		.set	at

		move	v0,zero
		move	v1,zero

		move	a0,zero
		move	a1,zero
		move	a2,zero
		move	a3,zero
		
		move	t0,zero
		move	t1,zero
		move	t2,zero
		move	t3,zero
		move	t4,zero
		move	t5,zero
		move	t6,zero
		move	t7,zero
		
		move	ta0,zero
		move	ta1,zero
		move	ta2,zero
		move	ta3,zero

		move	s0,zero
		move	s1,zero
		move	s2,zero
		move	s3,zero
		move	s4,zero
		move	s5,zero
		move	s6,zero
		move	s7,zero

		move	t8,zero
		move	t9,zero

		/* note: do NOT zero k0/k1 here: it's used by VAPI exit. */
		/* move	k0,zero */
		/* move	k1,zero */

		move	gp,zero
		move	sp,zero
		/* note: do NOT zero fp here */

		jr	ra

END(rm5200_zero_int_regs)


/*  *********************************************************************
    *  RM5200_ZERO_FP_REGS()
    *  
    *  Initialize the CP1 (floating-point) registers
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(rm5200_zero_fp_regs)

		mfc0	v0,C0_SR		/* Get old SR_CU1 value */
		or	v1,v0,M_SR_CU1		/* Turn on coprocessor 1 */
		or	v1,v1,M_SR_FR		/* in 32-register mode */
		mtc0	v1,C0_SR

		ssnop				/* wait for mtc0 to finish */
		ssnop
		ssnop
		ssnop
		ssnop
		ssnop
		ssnop

		cfc1	v1,$0			/* get FP impl register */
		beq	v1,zero,no_fp		/* don't do this if no FP */

		ctc1	zero,$31		/* Exception/status register */

		dmtc1	zero,$f0		/* general data registers */
		dmtc1	zero,$f1
		dmtc1	zero,$f2
		dmtc1	zero,$f3
		dmtc1	zero,$f4
		dmtc1	zero,$f5
		dmtc1	zero,$f6
		dmtc1	zero,$f7
		dmtc1	zero,$f8
		dmtc1	zero,$f9
		dmtc1	zero,$f10
		dmtc1	zero,$f11
		dmtc1	zero,$f12
		dmtc1	zero,$f13
		dmtc1	zero,$f14
		dmtc1	zero,$f15
		dmtc1	zero,$f16
		dmtc1	zero,$f17
		dmtc1	zero,$f18
		dmtc1	zero,$f19
		dmtc1	zero,$f20
		dmtc1	zero,$f21
		dmtc1	zero,$f22
		dmtc1	zero,$f23
		dmtc1	zero,$f24
		dmtc1	zero,$f25
		dmtc1	zero,$f26
		dmtc1	zero,$f27
		dmtc1	zero,$f28
		dmtc1	zero,$f29
		dmtc1	zero,$f30
		dmtc1	zero,$f31

no_fp:		mtc0	v0,C0_SR		/* restore to original state */
		j	ra

END(rm5200_zero_fp_regs)

/*  *********************************************************************
    *  RM5200_CP0_INIT()
    *  
    *  Initialize CP0 registers for an RM5200 core
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(rm5200_cp0_init)

		.set	noreorder
		mtc0	zero,C0_WATCHLO		# Clear out the watch regs.
		mtc0	zero,C0_WATCHHI

		mfc0	v0,C0_SR		# Get status register
		and	v0,M_SR_SR		# preserve soft reset
		or	v0,M_SR_BEV		# exceptions to boot vector

		mtc0	zero,C0_CAUSE		# must clear before writing SR

		mtc0	v0,C0_SR		# set up the status register

		mfc0	v0,C0_CONFIG		# get current CONFIG register
		srl	v0,v0,3			# strip out K0 bits
		sll	v0,v0,3			# k0 bits now zero
		or	v0,v0,K_CFG_K0COH_COHERENT # K0 is cacheable.
		mtc0	v0,C0_CONFIG

		mtc0	zero,C0_WATCHLO,0	# Watch registers.
		mtc0	zero,C0_WATCHHI,0
		mtc0	zero,C0_WATCHLO,1
		mtc0	zero,C0_WATCHHI,1

		mtc0	zero,C0_TLBHI		# TLB entry (high half)

	#
	# This is probably not the right init value for C0_COMPARE,
	# but it seems to be necessary for the sim model right now.
	#

		li	v0,-1
		mtc0	v0,C0_COMPARE

	#
	# Initialize all the TLB entries to some invalid value
	#

		mtc0	zero,C0_TLBLO0		/* tlblo0 = invalid */
		mtc0	zero,C0_TLBLO1		/* tlblo1 = invalid */
		mtc0	zero,C0_PGMASK
		li	t0,K1BASE		/* tlbhi  = impossible vpn */
		li	t1,(K_NTLBENTRIES-1)	/* index */
	
		.set noreorder
		nop
1:		mtc0	t0,C0_TLBHI
		mtc0	t1,C0_INX
		addu	t0,0x2000		/* inc vpn */
		tlbwi
		bnez	t1,1b
		subu	t1,1			# BDSLOT
		.set reorder


		jr	ra

END(rm5200_cp0_init)


/*  *********************************************************************
    *  RM5200_CPU_INIT()
    *  
    *  Initialize an RM5200 CPU.
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

	
LEAF(rm5200_cpu_init)

	#
	# We are going to call other subroutines from inside this
	# routine.  Hold onto the return address somewhere else
	# while we do this.
	#

		move	fp,ra			# keep our return addr here.

	#
	# First, zero all the registers.
	#
		bal	sb1_zero_int_regs

	#
	# CP0 initialization
	# 

		bal	sb1_cp0_init

	#
	# Now do the FP unit
	#

		bal	sb1_zero_fp_regs

		jr	fp

END(rm5200_cpu_init)


/*  *********************************************************************
    *  SB1_KSEG0_SWITCH
    *  
    *  Return to the address of the routine that called us, except
    *  in K0seg instead of K1seg
    *  
    *  Input parameters: 
    *  	   nothing - ra is return address
    *  	   
    *  Return value:
    *  	   ra = same return address in K0
    ********************************************************************* */

LEAF(rm5200_kseg0_switch)

		and	ra,(K0SIZE-1)
		or	ra,K0BASE
		jr	ra

END(rm5200_kseg0_switch)

/*  *********************************************************************
    *  End
    ********************************************************************* */
