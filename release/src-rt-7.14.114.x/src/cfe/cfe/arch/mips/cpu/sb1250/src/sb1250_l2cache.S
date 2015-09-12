/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  L2 Cache initialization			File: sb1250_l2cache.S
    *  
    *  This module contains code to initialize the L2 cache.
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
#include "sb1250_regs.h"
#include "sb1250_l2c.h"
#include "sb1250_mc.h"
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
		.set mips64


/*  *********************************************************************
    *  Macros
    ********************************************************************* */

/*#define PHYS_TO_XKPHYS(x) (0x9000000000000000|(x))*/
#define CACHE_LINE_SIZE	  32
#define HAZARD ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop

#define IF_BIN(binreg,binmask,label) \
	.set noat ; \
	andi AT,binreg,binmask ; \
	bne  AT,zero,label ; \
	.set at

/* In the WID register, 0=top/left, 1=bottom/left, 2=top/right, 3=bottom/right */

#define DISAB_TOP    0x800
#define DISAB_BOT    0x400
#define DISAB_RGT    0x200
#define DISAB_LFT    0x100

/*  *********************************************************************
    *  SB1250_L2CACHE_DISABTABLE
    *  
    *  This table maps the WID l2 diagnostic bits onto the disable
    *  mask for the L2 disable register.
    *  
    *  There are 8 entries in the table, with the index as followS:
    * 
    *       H WW
    *  
    *  H = 0 if only 1/4 of the cache is valid
    *  H = 1 if 1/2 of the cache is valid
    *  WW is the way number (or half number) that is valid
    ********************************************************************* */

/*
 * This is how the quadrant numbers are actually organized:
 *
 *   1 3
 *   0 2
 */

sb1250_l2cache_disabtable:

	.word	DISAB_LFT			/* Good=0(right)      Disable: left */
	.word	DISAB_LFT			/* Good=1(right)      Disable: left */
	.word	DISAB_RGT			/* Good=2(left)       Disable: right */
	.word	DISAB_RGT			/* Good=3(left)       Disable: right */

	.word	DISAB_RGT|DISAB_TOP		/* Good=0(bot/left),  Disable: top,right */
	.word	DISAB_RGT|DISAB_BOT		/* Good=1(top/left),  Disable: bottom,right */
	.word	DISAB_LFT|DISAB_TOP		/* Good=2(bot/right), Disable: top,left */
	.word	DISAB_LFT|DISAB_BOT		/* Good=3(top/right), Disable: bottom,left */


// Old table for historical value, when we
// thought the quadrant numbers were:
//
//   0 2
//   1 3
//
//	.word	DISAB_RGT|DISAB_BOT		/* Good=0(top/left),  Disable: right,bottom */
//	.word	DISAB_RGT|DISAB_TOP		/* Good=1(bot/left),  Disable: right,top */
//	.word	DISAB_LFT|DISAB_BOT		/* Good=2(top/right), Disable: left,bottom */
//	.word	DISAB_LFT|DISAB_TOP		/* Good=3(bot/right), Disable: left,top */


/*  *********************************************************************
    *  SB1250_L2CACHE_INIT()
    *  
    *  Initialize the L2 Cache tags to be "invalid"
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   t0,t1,t2
    ********************************************************************* */

	
LEAF(sb1250_l2cache_init)

	# Do nothing (return immediately) if L2 has been disabled via JTAG.

		li	t0, PHYS_TO_K1(A_SCD_SYSTEM_CFG)
		ld	t0, 0(t0)
		and	t0, t0, M_SYS_L2C_RESET
		beq	zero, t0, 1f
		jr	ra
1:

	# Save the old status register, and set the KX bit.

		mfc0	t2,C0_SR
		or	t1,t2,M_SR_KX
		mtc0	t1,C0_SR
		HAZARD

	# Start the index at the base of the cache management
	# area, but leave the address bit for "Valid" zero.
	# Note that the management tags are at 00_D000_0000,
	# which cannot be expressed with the PHYS_TO_K1 macro,
	# so well need to use a 64-bit address to get to it.

		dli	t0,PHYS_TO_XKSEG_UNCACHED(A_L2C_MGMT_TAG_BASE)

	# Loop through each entry and each way 

#ifdef _FASTINIT_
		li	t1,16
#else
		li	t1,L2C_ENTRIES_PER_WAY*L2C_NUM_WAYS
#endif


	# Write a zero to the cache management register at each
	# address.

		.align 4
1:		sd	zero,0(t0)
		sd	zero,CACHE_LINE_SIZE(t0)
		sd	zero,2*CACHE_LINE_SIZE(t0)
		sd	zero,3*CACHE_LINE_SIZE(t0)
		daddu	t0,(4*CACHE_LINE_SIZE) # size of a cache line
		subu	t1,4
		bne	t1,0,1b

	#
	# Restore old KX bit setting
	#

		mtc0	t2,C0_SR
		HAZARD


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

		beq	t1,0x1250,sb1250_l2cache_check_rev /* Go if real 1250 */
		beq	t1,0x1150,sb1250_l2cache_check_rev /* or 1250 in uni-cpu mode */
		b	sb1250_l2cache_init_good	/* otherwise not a 1250, no WID check */

 	/*
	 * Now, check the revision.  Anything earlier than step A3 
	 * does not need this check. Pass 3 does not need this check also.
	 *
	 * Exception: Step A6 parts return 0x04 in their revision field.
	 * These parts can can be verified as A6 by having a nonzero WID.
	 */

sb1250_l2cache_check_rev:
		ld	t1,0(t0)			/* get the SYSTEM_REVISION again */
		dsrl	t1,t1,S_SYS_REVISION
		andi	t1,t1,(M_SYS_REVISION >> S_SYS_REVISION)
		beq	t1,0x04,sb1250_l2cache_check_wid
		blt	t1,0x05,sb1250_l2cache_init_good
		bge	t1,0x20,sb1250_l2cache_init_good

	/*
	 * Okay, we really need to check the WID now.  If the WID is 
	 * not programmed at all, assume the part is good.
	 * (yes, this includes the wafer/lot bits)
	 */

sb1250_l2cache_check_wid:
		ld	t1,0(t0)			/* Get the WID bits back */
		dsrl	t1,t1,S_SYS_WID			/* wafer ID to bits 0..31 */
		li	t2,(M_SYS_WID >> S_SYS_WID)
		and	t1,t1,t2

		WID_UNCONVOLUTE(t1,t2,t3,t4)

		beq	t1,zero,sb1250_l2cache_init_good

	/*
         * Get the bin number from the WID.  This tells us many things.
	 * For the L1 cache we need to know which ways to use,
	 * and this is determined by what we put in the tag registers.
	 */

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

		IF_BIN(t0,M_WID_BIN_F2,sb1250_l2cache_init_good)

	/* 
	 * Get the WID register again and isolate the L2 cache bits.  Combine
	 * this with the "1/2" attribute in the bin bitmask to form the
	 * offset of our table.
	 */

		li	t1,PHYS_TO_K1(A_SCD_SYSTEM_REVISION)
		ld	t1,0(t1)
		dsrl	t1,t1,S_SYS_WID			/* get WID bits */

		WID_UNCONVOLUTE(t1,t2,t3,t4)

		dsrl	t1,t1,S_WID_L2QTR
		andi	t1,t1,(M_WID_L2QTR >> S_WID_L2QTR)

		IF_BIN(t0,M_WID_BIN_H2,1f)
		ori	t1,t1,0x04			/* t1 = index into table */
1:

		sll	t1,2				/* make 32-bit offset */

		move	t2,ra
		LOADREL(t0,sb1250_l2cache_disabtable)
		or	t0,K1BASE
		move	ra,t2				/* get address of table */

		add	t0,t0,t1			/* t0 = address of element in table */
		lw	t0,0(t0)			/* t0 = address for WAY_DISABLE */
		li	t1,PHYS_TO_K1(A_L2_CACHE_DISABLE) /* Quadrant disable */
		or	t1,t1,t0			/* OR in the address bits */

		sd	t0,0(t1)			/* do the memory reference */


sb1250_l2cache_init_good:
		j	ra		# return to caller

END(sb1250_l2cache_init)


/*  *********************************************************************
    *  SB1250_L2CACHE_DISABLE()
    *  
    *  Convert the entire L2 Cache into static memory, for use by
    *  the bootstrap loader.  Actually, it only removes three of the
    *  ways, since you must leave at least one way active at all
    *  times.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   t0,t1
    ********************************************************************* */


LEAF(sb1250_l2cache_disable)

	# Do nothing (return immediately) if L2 has been disabled via JTAG.

		li	t0, PHYS_TO_K1(A_SCD_SYSTEM_CFG)
		ld	t0, 0(t0)
		and	t0, t0, M_SYS_L2C_RESET
		beq	zero, t0, 1f
		jr	ra
1:

	# Save the old status register, and set the KX bit.
	# Configure the L2 cache as SRAM (all ways disabled except one)
	# Do a memory reference at the "way_disable" address
	# to switch it off.
	# Warning: do NOT try to configure all of the ways off - you 
	# must leave at least one way active!  This code leaves
	# way #3 active and gives ways 0..2 to the program.

		li	t0,PHYS_TO_K1(A_L2_MAKEDISABLE(0x07))
		ld	t0,(t0)

	# Use the result of the load to stall the pipe here.
	# Ref sec 5.4.2
	# XXX is this necessary for global enable/disable operations?

		addu	t0,t0,t0

	# Re-write all the tags

		b	sb1250_l2cache_init

END(sb1250_l2cache_disable)


/*  *********************************************************************
    *  SB1250_L2CACHE_ENABLE()
    *  
    *  Convert the L2 Cache memory into the actual L2 cache, enabling
    *  the cache for future memory accesses.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   t0,t1
    ********************************************************************* */

LEAF(sb1250_l2cache_enable)

	# Do nothing (return immediately) if L2 has been disabled via JTAG.

		li	t0, PHYS_TO_K1(A_SCD_SYSTEM_CFG)
		ld	t0, 0(t0)
		and	t0, t0, M_SYS_L2C_RESET
		beq	zero, t0, 1f
		jr	ra
1:

	# Save the old status register, and set the KX bit.
	# Configure the L2 cache as Cache (all ways enabled)
	# Do a memory reference at the "way_disable" address
	# to switch it on.

		li	t0,PHYS_TO_K1(A_L2_MAKEDISABLE(0x0))
		ld	t0,(t0)

	# Use the result of the load to stall the pipe here.
	# Ref sec 5.4.2
	# XXX is this necessary for global enable/disable operations?

		addu	t0,t0,t0

	# Re-write all the tags

		b	sb1250_l2cache_init

END(sb1250_l2cache_enable)


/*  *********************************************************************
    *  SB1250_L2CACHE_FLUSH()
    *  
    *  Flush the entire L2 cache.  All dirty lines are written back
    *  out to memory.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   v0 - number of lines flushed
    *  
    *  Registers used:
    *  	   t0,t1,t2,t3,t4,t5,t6,t7,a0,s1,s2,s3,s4
    ********************************************************************* */

LEAF(sb1250_l2cache_flush)

	# Do nothing (return immediately) if L2 has been disabled via JTAG.

		li	t0, PHYS_TO_K1(A_SCD_SYSTEM_CFG)
		ld	t0, 0(t0)
		and	t0, t0, M_SYS_L2C_RESET
		beq	zero, t0, 1f
		jr	ra
1:

	# Save the old status register, and set the KX bit.

		mfc0	a0,C0_SR
		or	t1,a0,M_SR_KX
		mtc0	t1,C0_SR
		HAZARD

	#
	# Set the BERR bits in both memory controllers.  We're
	# going to do cacheable reads where there is no memory.
	#
	#
	# Note that on an 1125, we can still do this on MC 0 even
	# though there is only one memory controller.  The register
	# is there, it just ignores the bits we're trying to write.
	#

		li	t0,PHYS_TO_K1(A_MC_REGISTER(0,R_MC_CONFIG))
		ld	t6,0(t0)
		dli	t1,(M_MC_BERR_DISABLE | M_MC_ECC_DISABLE)
		or	t1,t1,t6
		sd	t1,0(t0)

		li	t0,PHYS_TO_K1(A_MC_REGISTER(1,R_MC_CONFIG))
		ld	t7,0(t0)
		dli	t1,(M_MC_BERR_DISABLE | M_MC_ECC_DISABLE)
		or	t1,t1,t7
		sd	t1,0(t0)

	# Start the index at the base of the cache management area.
	# Note that the management tags are at 00_D000_0000,
	# which cannot be expressed with the PHYS_TO_K1 macro,
	# so well need to use a 64-bit address to get to it.

	# Set up the common values which may be massaged by WID info

		li	s1,PHYS_TO_K1(A_L2_READ_ADDRESS)
		li	s2,L2C_NUM_WAYS

		dli	s3,PHYS_TO_XKSEG_UNCACHED(A_L2C_MGMT_TAG_BASE)
		move	v0,zero

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
		b	l2f_full			/* otherwise not a 1250, no WID check */

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
		blt	t1,0x05,l2f_full

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

		beq	t1,zero,l2f_full

	/*
         * Get the bin number from the WID.  This tells us many things.
	 * For the L1 cache we need to know which ways to use,
	 * and this is determined by what we put in the tag registers.
	 */

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

		IF_BIN(t0,M_WID_BIN_F2,l2f_full)

	/* 
	 * Get the WID register again and isolate the L2 cache bits.  Combine
	 * this with the "1/2" attribute in the bin bitmask to form the
	 * offset of our table.
	 */

		li	t1,PHYS_TO_K1(A_SCD_SYSTEM_REVISION)
		ld	t1,0(t1)
		dsrl	t1,t1,S_SYS_WID			/* get WID bits */

		WID_UNCONVOLUTE(t1,t2,t3,t4)

		dsrl	t1,t1,S_WID_L2QTR
		andi	t1,t1,(M_WID_L2QTR >> S_WID_L2QTR)

		IF_BIN(t0,M_WID_BIN_H2,l2f_half)

l2f_quarter:	li	s4,L2C_ENTRIES_PER_WAY/4
		dsll	t1,S_L2C_MGMT_QUADRANT
		daddu	s3,t1
		b	1f

l2f_half:	li	s4,L2C_ENTRIES_PER_WAY/2
		dsrl	t1,1
		xori	t1,1
		dsll	t1,S_L2C_MGMT_HALF
		daddu	s3,t1
		b	1f

l2f_full:	li	s4,L2C_ENTRIES_PER_WAY

	# Loop through each entry and each way 

1:		move	t1,s4
		move	t0,s3

	# Do a read at the cache management address to set the
	# A_L2_READ_TAG register.

2:		ld	t3,0(t0)		# this sets the register.
		daddu	t3,t3,0			# Do an ALU op to ensure ordering
		ld	t4,0(s1)		# Get the tag
		li	t5,M_L2C_TAG_DIRTY
		and	t5,t4,t5		# Test the dirty bit
		beq	t5,zero,3f		# don't flush this line

	#
	# The way that we're looking at now will be the victim, so all we
	# need to do is a cacheable read at any address that does *not*
	# match this tag.  To do this, we're going to OR in some bits 
	# into the physical address to put it way outside the memory area.
	# Then do a cacheable read.  The current way will be replaced
	# with the garbage data.  We'll pick PA 30_0000_0000 in the middle
	# of the 520GB memory expansion area for this purpose.
	#

		add	v0,1			# count this line (debug)

		dli	t5,(M_L2C_TAG_TAG|M_L2C_TAG_INDEX)
		and	t4,t4,t5		# Have a physical address
		dli	t5,PHYS_TO_XKSEG_CACHED(0x3000000000)
		or	t4,t4,t5
		ld	t4,0(t4)		# Do a read.
		daddu	t4,1			# Use it in an ALU op.


3:		daddu	t0,CACHE_LINE_SIZE	# size of a cache line
		subu	t1,1
		bne	t1,0,2b

		daddu	s3,V_L2C_MGMT_WAY(1)
		subu    s2,1
		bne	s2,0,1b


	#
	# Now, reinit the entire cache.  Of course, we could just
	# reinit the lines we flushed, but this routine is mucking
	# the entire cache anyway, so it doesn't matter.
	#


		dli	t0,PHYS_TO_XKSEG_UNCACHED(A_L2C_MGMT_TAG_BASE)
		li	t1,L2C_ENTRIES_PER_WAY*L2C_NUM_WAYS

	# Write a zero to the cache management register at each
	# address.

1:		sd	zero,0(t0)
		sd	zero,CACHE_LINE_SIZE(t0)
		sd	zero,2*CACHE_LINE_SIZE(t0)
		sd	zero,3*CACHE_LINE_SIZE(t0)
		daddu	t0,(4*CACHE_LINE_SIZE) # size of a cache line
		subu	t1,4
		bne	t1,0,1b

	#
	# Restore the old MC register values
	#


		li	t0,PHYS_TO_K1(A_MC_REGISTER(0,R_MC_CONFIG))
		sd	t6,0(t0)

		li	t0,PHYS_TO_K1(A_MC_REGISTER(1,R_MC_CONFIG))
		sd	t7,0(t0)

	#
	# Restore old KX bit setting
	#

		mtc0	a0,C0_SR
		HAZARD

		j	ra		# return to caller

END(sb1250_l2cache_flush)




/*  *********************************************************************
    *  End
    ********************************************************************* */
