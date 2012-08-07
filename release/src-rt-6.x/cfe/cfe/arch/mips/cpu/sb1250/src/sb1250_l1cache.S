/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  L1 Cache initialization			File: sb1250_l1cache.S
    *  
    *  This module contains code to initialize the L1 cache.
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
#include "mipsmacros.h"
#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_scd.h"
#include "sb1250_wid.h"

/*
 * This lets us override the WID by poking values into our PromICE 
 */
#ifdef _MAGICWID_
#undef A_SCD_SYSTEM_REVISION
#define A_SCD_SYSTEM_REVISION 0x1FC00508
#endif

		.text

		.set	mips64


/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define L1CACHE_NUMWAYS	4
#define L1CACHE_NUMIDX  256
#define L1CACHE_LINESIZE 32
#define L1CACHE_IDXHIGH (L1CACHE_LINESIZE*L1CACHE_NUMWAYS*L1CACHE_NUMIDX)

#define L1CACHEOP(cachename,op) ((cachename) | ((op) << 2))

#define L1C_OP_IDXINVAL     0
#define L1C_OP_IDXLOADTAG   1
#define L1C_OP_IDXSTORETAG  2
#define L1C_OP_IMPLRSVD     3
#define L1C_OP_HITINVAL     4
#define L1C_OP_FILL         5
#define L1C_OP_HITWRITEBACK 6
#define L1C_OP_FETCHLOCK    7

#define L1C_I		    0
#define L1C_D		    1

/*
 * CP0 C0_TAGHI values for cache freezing/way elimination
 */

#define L1C_LRU_FREEZE0	    (1<<22)|(3<<20)|(2<<18)|(1<<16)|(0<<14)	/* 3,2,1,0[F] */
#define L1C_LRU_FREEZE1	    (1<<22)|(3<<20)|(2<<18)|(0<<16)|(1<<14)	/* 3,2,0,1[F] */
#define L1C_LRU_FREEZE2	    (1<<22)|(3<<20)|(0<<18)|(1<<16)|(2<<14)	/* 3,0,1,2[F] */
#define L1C_LRU_FREEZE3	    (1<<22)|(2<<20)|(1<<18)|(0<<16)|(3<<14)	/* 2,1,0,3[F] */

#define L1C_LRU_REMOVE0	    (1<<22)|(3<<20)|(2<<18)|(1<<16)|(3<<14)	/* 3,2,1,3 */
#define L1C_LRU_REMOVE1	    (1<<22)|(3<<20)|(2<<18)|(0<<16)|(2<<14)	/* 3,2,0,2 */
#define L1C_LRU_REMOVE2	    (1<<22)|(3<<20)|(0<<18)|(1<<16)|(1<<14)	/* 3,0,1,1 */
#define L1C_LRU_REMOVE3	    (1<<22)|(2<<20)|(1<<18)|(0<<16)|(0<<14)	/* 2,1,0,0 */

/*
 * Macro to test bin numbers
 */

#define HAZARD ssnop ;  ssnop ;  ssnop ;  ssnop ;  ssnop ;  ssnop ;  ssnop

#define IF_BIN(binreg,binmask,label) \
	.set noat ; \
	andi AT,binreg,binmask ; \
	bne  AT,zero,label ; \
	.set at

#define IF_CPU1(label) \
	.set noat ; \
	mfc0 AT,C0_PRID ; \
	srl  AT,AT,25 ; \
	andi AT,AT,7 ; \
	bne  AT,zero,label ; \
	.set at

/*  *********************************************************************
    *  SB1250_L1CACHE_DISABLETABLE
    *  
    *  This table maps the bits in the diagnostic result 
    *  register (part of WID) to which ways we want to enable/remove
    *  from the L1.
    *  
    *  The format of the table is:
    *  
    *       X DD II
    *  
    *  Where X is one for 1/4 (way indicated is the GOOD way)
    *  or    X is zero for 3/4 (way indicated is the BAD way)
    *  
    *  and
    *  
    *  DD is the way number for the DCache
    *  II is the way number for the ICache
    *  
    *  there are 32 64-bit entries in this table.
    ********************************************************************* */

#define L1DISTBL(d,i)  (d),(i)

sb1250_l1cache_disabletable:

	.word	L1DISTBL(L1C_LRU_REMOVE0, L1C_LRU_REMOVE0)
	.word	L1DISTBL(L1C_LRU_REMOVE0, L1C_LRU_REMOVE1)
	.word	L1DISTBL(L1C_LRU_REMOVE0, L1C_LRU_REMOVE2)
	.word	L1DISTBL(L1C_LRU_REMOVE0, L1C_LRU_REMOVE3)

	.word	L1DISTBL(L1C_LRU_REMOVE1, L1C_LRU_REMOVE0)
	.word	L1DISTBL(L1C_LRU_REMOVE1, L1C_LRU_REMOVE1)
	.word	L1DISTBL(L1C_LRU_REMOVE1, L1C_LRU_REMOVE2)
	.word	L1DISTBL(L1C_LRU_REMOVE1, L1C_LRU_REMOVE3)

	.word	L1DISTBL(L1C_LRU_REMOVE2, L1C_LRU_REMOVE0)
	.word	L1DISTBL(L1C_LRU_REMOVE2, L1C_LRU_REMOVE1)
	.word	L1DISTBL(L1C_LRU_REMOVE2, L1C_LRU_REMOVE2)
	.word	L1DISTBL(L1C_LRU_REMOVE2, L1C_LRU_REMOVE3)

	.word	L1DISTBL(L1C_LRU_REMOVE3, L1C_LRU_REMOVE0)
	.word	L1DISTBL(L1C_LRU_REMOVE3, L1C_LRU_REMOVE1)
	.word	L1DISTBL(L1C_LRU_REMOVE3, L1C_LRU_REMOVE2)
	.word	L1DISTBL(L1C_LRU_REMOVE3, L1C_LRU_REMOVE3)

	.word	L1DISTBL(L1C_LRU_FREEZE0, L1C_LRU_FREEZE0)
	.word	L1DISTBL(L1C_LRU_FREEZE0, L1C_LRU_FREEZE1)
	.word	L1DISTBL(L1C_LRU_FREEZE0, L1C_LRU_FREEZE2)
	.word	L1DISTBL(L1C_LRU_FREEZE0, L1C_LRU_FREEZE3)

	.word	L1DISTBL(L1C_LRU_FREEZE1, L1C_LRU_FREEZE0)
	.word	L1DISTBL(L1C_LRU_FREEZE1, L1C_LRU_FREEZE1)
	.word	L1DISTBL(L1C_LRU_FREEZE1, L1C_LRU_FREEZE2)
	.word	L1DISTBL(L1C_LRU_FREEZE1, L1C_LRU_FREEZE3)

	.word	L1DISTBL(L1C_LRU_FREEZE2, L1C_LRU_FREEZE0)
	.word	L1DISTBL(L1C_LRU_FREEZE2, L1C_LRU_FREEZE1)
	.word	L1DISTBL(L1C_LRU_FREEZE2, L1C_LRU_FREEZE2)
	.word	L1DISTBL(L1C_LRU_FREEZE2, L1C_LRU_FREEZE3)

	.word	L1DISTBL(L1C_LRU_FREEZE3, L1C_LRU_FREEZE0)
	.word	L1DISTBL(L1C_LRU_FREEZE3, L1C_LRU_FREEZE1)
	.word	L1DISTBL(L1C_LRU_FREEZE3, L1C_LRU_FREEZE2)
	.word	L1DISTBL(L1C_LRU_FREEZE3, L1C_LRU_FREEZE3)




/*  *********************************************************************
    *  SB1250_L1CACHE_INIT()
    *  
    *  Initialize the L1 Cache tags to be "invalid"
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   t0,t1,t2,t3
    ********************************************************************* */

	
LEAF(sb1250_l1cache_init)

	/*
	 * Test to see if we're running on a pre-production part with
	 * a defective L1 cache.  We store information in the SCD
	 * SYSTEM_REVISION register that identifies what is
	 * going on.
	 */

	/*
	 * First, check the part number 
	 */

		li	t0,PHYS_TO_K1(A_SCD_SYSTEM_REVISION)

		ld	t1,0(t0)			/* get SYSTEM_REVISION */

		dsrl	t1,t1,S_SYS_PART
		andi	t1,t1,(M_SYS_PART >> S_SYS_PART)

		beq	t1,0x1250,sb1250_l1cache_check_rev /* Go if real 1250 */
		beq	t1,0x1150,sb1250_l1cache_check_rev /* or 1250 in uni-cpu mode */
		b	sb1250_l1cache_init_good	/* otherwise not a 1250, no WID check */

 	/*
	 * Now, check the revision.  Anything earlier than step A3 
	 * does not need this check. Pass 3 does not need this check also.
	 *
	 * Exception: Step A6 parts return 0x04 in their revision field.
	 * These parts can can be verified as A6 by having a nonzero WID.
	 */

sb1250_l1cache_check_rev:
		ld	t1,0(t0)			/* get the SYSTEM_REVISION again */
		dsrl	t1,t1,S_SYS_REVISION
		andi	t1,t1,(M_SYS_REVISION >> S_SYS_REVISION)
		beq	t1,0x04,sb1250_l1cache_check_wid
		blt	t1,0x05,sb1250_l1cache_init_good
		bge	t1,0x20,sb1250_l1cache_init_good

	/*
	 * Okay, we really need to check the WID now.  If the WID is 
	 * not programmed at all, assume the part is good.
	 * (yes, this includes the wafer/lot bits)
	 */

sb1250_l1cache_check_wid:
		ld	t1,0(t0)			/* Get the WID bits back */
		dsrl	t1,t1,S_SYS_WID			/* wafer ID to bits 0..31 */
		li	t2,(M_SYS_WID >> S_SYS_WID)
		and	t1,t1,t2

		WID_UNCONVOLUTE(t1,t2,t3,t4)

		beq	t1,zero,sb1250_l1cache_init_good

	/*
         * Get the bin number from the WID.  This tells us many things.
	 * For the L1 cache we need to know which ways to use,
	 * and this is determined by what we put in the tag registers.
	 */

		dmtc0	zero,C0_TAGLO			/* assume all is good. */
		dmtc0	zero,C0_TAGHI
		dmtc0	zero,C0_TAGLO,2
		dmtc0	zero,C0_TAGHI,2

		andi	t0,t1,M_WID_BIN			/* bin # into T0 */
		li	t2,1				/* make a bitmask */
		sll	t0,t2,t0			/* put '1' in correct place */

	/*
	 * t0 now contains a single bit set corresponding to the bin number
	 * that this chip belongs to.
	 * for example, if it is in bin 4, then the value is 1<<4
	 */

	/*
	 * Check for the case of a fully operational cache.
	 */

		IF_BIN(t0,M_WID_BIN_FID,sb1250_l1cache_init_good)

	/*
	 * Now compute an index into the table using the WID bits and
	 * the "3/4" value from the bin number.
	 */

		li	t1,PHYS_TO_K1(A_SCD_SYSTEM_REVISION)
		ld	t1,0(t1)			/* get SYSTEM_REVISION */
		dsrl	t1,t1,S_SYS_WID			/* get WID bits */

		WID_UNCONVOLUTE(t1,t2,t3,t4)

		IF_CPU1(sb1250_l1init_cpu1)

		li	t2,PHYS_TO_K1(A_SCD_SYSTEM_CFG)
		ld	t2,0(t2)
		dli	t3,M_SYS_UNICPU1
		and	t2,t3			/* T2 nonzero if UNICPU1 */
		bne	t2,zero,sb1250_l1init_cpu1


sb1250_l1init_cpu0:
		li	t2,S_WID_CPU0
		b	sb1250_l1init_cont

sb1250_l1init_cpu1:
		li	t2,S_WID_CPU1


sb1250_l1init_cont:
		dsrl	t1,t1,t2			/* move CPU way bits into posn */
		andi	t1,t1,(M_WID_CPUX_L1I|M_WID_CPUX_L1D) /* keep only way bits */


		IF_BIN(t0,M_WID_BIN_3ID,1f)
		ori	t1,t1,0x10			/* 1/4 bit set */
1:

	/*
	 * Okay, now t1 is the index into the table.
	 * Look up the I and D way disable values from the table and store
	 * them in the TAGHI and TAGHI,2 registers.
	 */
		sll	t1,3				/* make 64-bit offset */

		move	t3,ra
		LOADREL(t2,sb1250_l1cache_disabletable)
                or      t2,K1BASE
		move	ra,t3
		add	t2,t2,t1			/* t2 points at our table entry */

		lw	t1,0(t2)			/* DCache disable mask */
		dmtc0	t1,C0_TAGHI,2
		lw	t1,4(t2)			/* ICache disable mask */
		dmtc0	t1,C0_TAGHI

	/*
	 * Go ahead and initialize the cache now, using the
	 * values programmed into the TAGHI registers.
	 *
	 * T0 is _still_ our bitmask from the bin register.  We will
	 * need that later.
	 */


		li	t2,K1BASE
#ifdef _FASTINIT_
		li	t3,L1CACHE_LINESIZE*8			/* only 8 indicies now */
#else
		li	t3,L1CACHE_IDXHIGH
#endif

		add     t1,t2,t3
1:		cache   L1CACHEOP(L1C_I,L1C_OP_IDXSTORETAG),0(t2)
		addu    t2,L1CACHE_LINESIZE
		bne     t1,t2,1b

		li	t2,K1BASE
		add     t1,t2,t3
1:		cache   L1CACHEOP(L1C_D,L1C_OP_IDXSTORETAG),0(t2)
		addu    t2,L1CACHE_LINESIZE
		bne     t1,t2,1b


	/*	
	 * Set the defeature register if we're doing the 1/4 case.
	 * If we're here, at least _something_ is wrong.
	 */

		IF_BIN(t0,M_WID_BIN_3ID,2f)

	/*
	 * In the 1/4 case, we'll always freeze the D cache.
	 * We might not freeze the I cache, though.
	 */

		li	t2,(1<<10)			/* Freeze D */
		mfc0	t1,$23,2			/* start with current value */

		IF_BIN(t0,M_WID_BIN_FI,1f)		/* If I cache is ok, no freeze */
		li      t3,(1<<28)
		or      t2,t2,t3			/* also freeze I */
1:
		or	t1,t1,t2			/* Merge in new bits */
		HAZARD
		mtc0	t1,$23,2			/* Write back to defeature register */
		HAZARD

2:
	/*
	 * Done, if you can believe it!
	 */

		j	ra

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	/*
	 * This part of the routine handles a fully operational L1
	 * cache. 
	 */

sb1250_l1cache_init_good:

		dmtc0	zero,C0_TAGLO
		dmtc0	zero,C0_TAGHI

		li	t2,K1BASE
#ifdef _FASTINIT_
		li	t3,L1CACHE_LINESIZE*8			/* only 8 indicies now */
#else
		li	t3,L1CACHE_IDXHIGH
#endif

		add     t0,t2,t3
		.align	4
1:		cache   L1CACHEOP(L1C_I,L1C_OP_IDXSTORETAG),0(t2)
		addu    t2,L1CACHE_LINESIZE
		bne     t0,t2,1b

		dmtc0	zero,C0_TAGLO,2
		dmtc0	zero,C0_TAGHI,2

		li	t2,K1BASE
		add     t0,t2,t3
		.align	4
1:		cache   L1CACHEOP(L1C_D,L1C_OP_IDXSTORETAG),0(t2)
		addu    t2,L1CACHE_LINESIZE
		bne     t0,t2,1b

		j	ra

END(sb1250_l1cache_init)


#if CFG_VAPI

LEAF(sb1250_reset_defeature)

	/*
	 * Test to see if we're running on a pre-production part with
	 * a defective L1 cache.  We store information in the SCD
	 * SYSTEM_REVISION register that identifies what is
	 * going on.
	 */

	/*
	 * First, check the part number 
	 */

		li	t0,PHYS_TO_K1(A_SCD_SYSTEM_REVISION)
		ld	t1,0(t0)			/* get SYSTEM_REVISION */

		dsrl	t1,t1,S_SYS_PART
		andi	t1,t1,(M_SYS_PART >> S_SYS_PART)

		beq	t1,0x1250,1f			/* Go if real 1250 */
		beq	t1,0x1150,1f			/* or 1250 in uni-cpu mode */
		b	sb1250_no_defeature	/* otherwise not a 1250, no WID check */

 	/*
	 * Now, check the revision.  Anything earlier than step A3 
	 * does not need this check.
	 *
	 * Exception: Step A6 parts return 0x04 in their revision field.
	 * These parts can can be verified as A6 by having a nonzero WID.
	 */

1:		ld	t1,0(t0)			/* get the SYSTEM_REVISION again */
		dsrl	t1,t1,S_SYS_REVISION
		andi	t1,t1,(M_SYS_REVISION >> S_SYS_REVISION)
		beq	t1,0x04,1f
		blt	t1,0x05,sb1250_no_defeature

	/*
	 * Okay, we really need to check the WID now.  If the WID is 
	 * not programmed at all, assume the part is good.
	 * (yes, this includes the wafer/lot bits)
	 */

1:		ld	t1,0(t0)			/* Get the WID bits back */
		dsrl	t1,t1,S_SYS_WID			/* wafer ID to bits 0..31 */
		li	t2,(M_SYS_WID >> S_SYS_WID)
		and	t1,t1,t2

		WID_UNCONVOLUTE(t1,t2,t3,t4)

		beq	t1,zero,sb1250_no_defeature

	/*
         * Get the bin number from the WID.  This tells us many things.
	 * For the L1 cache we need to know which ways to use,
	 * and this is determined by what we put in the tag registers.
	 */

		andi	t0,t1,M_WID_BIN			/* bin # into T0 */
		li	t2,1				/* make a bitmask */
		sll	t0,t2,t0			/* put '1' in correct place */

	/*	
	 * Set the defeature register if we're doing the 1/4 case.
	 * If we're here, at least _something_ is wrong.
	 * The 3/4 and full cache cases don't need defeaturing.
	 */

		IF_BIN(t0,(M_WID_BIN_3ID | M_WID_BIN_FID),sb1250_no_defeature)

	/*
	 * In the 1/4 case, we'll always freeze the D cache.
	 * We might not freeze the I cache, though.
	 */

		li	t2,(1<<10)			/* Freeze D */
		mfc0	t1,$23,2			/* start with current value */

		IF_BIN(t0,M_WID_BIN_FI,1f)		/* If I cache is ok, no freeze */
		li      t3,(1<<28)
		or      t2,t2,t3			/* also freeze I */
1:
		or	t1,t1,t2			/* Merge in new bits */
		HAZARD
		mtc0	t1,$23,2			/* Write back to defeature register */
		HAZARD

sb1250_no_defeature:

		j	ra

END(sb1250_reset_defeature)

#endif


/*  *********************************************************************
    *  SB1250_L1CACHE_INVAL_I()
    *  
    *  Invalidate the L1 ICache
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   t0,t1,t2,t3
    ********************************************************************* */

	
LEAF(sb1250_l1cache_inval_i)


		li	t2,K1BASE
		li	t3,L1CACHE_IDXHIGH

		add     t0,t2,t3
		.align	4
1:		cache   L1CACHEOP(L1C_I,L1C_OP_IDXINVAL),0(t2)
		addu    t2,L1CACHE_LINESIZE
		bne     t0,t2,1b

		j	ra

END(sb1250_l1cache_inval_i)


/*  *********************************************************************
    *  SB1250_L1CACHE_FLUSH_D()
    *  
    *  Flush the entire L1 DCache (write dirty lines back to memory)
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   t0,t1,t2,t3
    ********************************************************************* */

	
LEAF(sb1250_l1cache_flush_d)

		li	t2,K1BASE
		li	t3,L1CACHE_IDXHIGH

		li	t2,K1BASE
		add     t0,t2,t3
		.align	4
1:		cache   L1CACHEOP(L1C_D,L1C_OP_IDXINVAL),0(t2)
		addu    t2,L1CACHE_LINESIZE
		bne     t0,t2,1b

		sync
		sync				/* pass1 issue. */

		j	ra

END(sb1250_l1cache_flush_d)


/*  *********************************************************************
    *  End
    ********************************************************************* */
