/*  *********************************************************************
    *  P5064 Board Support Package
    *  
    *  L1 Cache initialization			File: rm5200_l1cache.S
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
#include "r5kc0.h"

		.text


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

#define LINESIZE 32
#define icachesize	t2
#define dcachesize	t3

/*  *********************************************************************
    *  RM5200_L1CACHE_INIT()
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

	
#define FILLBASE 0x9fc00000  /* was K0BASE */

LEAF(rm5200_l1cache_init)

	/*
	 * Determine the cache sizes
	 */

		mfc0	t0,C0_CONFIG

	/* work out primary i-cache size */

		and	t1,t0,CFG_ICMASK
		srl	t1,CFG_ICSHIFT
		li	icachesize,0x1000
		sll	icachesize,t1

	/* work out primary d-cache size */

		and	t1,t0,CFG_DCMASK
		srl	t1,CFG_DCSHIFT
		li	dcachesize,0x1000
		sll	dcachesize,t1
	
	/*
	 * The caches may be in an indeterminate state,
	 * so we force good parity into them by doing an
	 * invalidate, load/fill, invalidate for each line.
	 */

	/* disable all i/u and cache exceptions */

		mfc0	v0,C0_SR
		li	a0,~SR_IE
		and	a0,v0
		or	a0,SR_DE
	
		.set noreorder
		mtc0	a0,C0_SR
		nop
	
	/* disable secondary cache and set zero tag */

		mfc0	t0,C0_CONFIG
		nop
		mtc0	zero,C0_TAGLO
		and	t0,~CFG_SE
		mtc0	t0,C0_CONFIG
		nop; nop; nop; nop
		.set	reorder
	
	/* 
	 * Assume bottom of ROM will generate good parity for the 
	 * primary caches (max 32K)
	 */

	/* 
	 * Initialise primary instruction cache.
	 */

		.set	noreorder
		li	a0,FILLBASE
		addu	a1,a0,icachesize		# limit = base + icachesize 
1:		addu	a0,LINESIZE
		cache	Index_Store_Tag_I,-4(a0)	# clear tag
		nop
		cache	Fill_I,-4(a0)			# fill data line
		nop
		bne	a0,a1,1b
		cache	Index_Store_Tag_I,-4(a0)	# BDSLOT: clear tag
		.set	reorder

	/* 
	 * Initialise primary data cache.
	 * (for 2-way set caches, we do it in 3 passes).
	 */

	/* 1: initialise dcache tags */
	        .set	noreorder
		li	a0,FILLBASE
		addu	a1,a0,dcachesize        	# limit = base + dcachesize 
1:		addu	a0,LINESIZE
		bne	a0,a1,1b
		cache	Index_Store_Tag_D,-4(a0)	# BDSLOT: clear tag
		.set	reorder

	/* 2: fill dcache data */
	        .set	noreorder
		li	a0,FILLBASE
		addu	a1,a0,dcachesize		# limit = base + dcachesize 
1:		addu	a0,LINESIZE
		bne	a0,a1,1b
		lw	zero,-4(a0)			# BDSLOT: fill line
		.set	reorder

	/* 3: clear dcache tags */
	        .set	noreorder
		li	a0,FILLBASE
		addu	a1,a0,dcachesize        	# limit = base + dcachesize 
1:		addu	a0,LINESIZE
		bne	a0,a1,1b
		cache	Index_Store_Tag_D,-4(a0)	# BDSLOT: clear tag
		.set	reorder
	
		mtc0	v0,C0_SR
		j	ra



END(rm5200_l1cache_init)


/*  *********************************************************************
    *  RM5200_L1CACHE_INVAL_I()
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

	
LEAF(rm5200_l1cache_inval_i)

	/*
 	 * Determine the ICache size
	 */

		mfc0	t0,C0_CONFIG

		and	t1,t0,CFG_ICMASK
		srl	t1,CFG_ICSHIFT
		li	icachesize,0x1000		/* t2 */
		sll	icachesize,t1

	/* 
	 * Invalidate primary instruction cache.
	 */

		li	t0,FILLBASE
		addu	t1,t0,icachesize		# limit = base + icachesize 
1:		cache   L1CACHEOP(L1C_I,L1C_OP_IDXINVAL),0(t0)
		addu	t0,LINESIZE
		bne	t0,t1,1b

		j	ra

END(rm5200_l1cache_inval_i)


/*  *********************************************************************
    *  RM5200_L1CACHE_FLUSH_D()
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

	
LEAF(rm5200_l1cache_flush_d)

	/*
 	 * Determine the DCache size
	 */

		mfc0	t0,C0_CONFIG

		and	t1,t0,CFG_DCMASK
		srl	t1,CFG_DCSHIFT
		li	dcachesize,0x1000		/* t3 */
		sll	dcachesize,t1

	/* 
	 * Flush data cache
	 */

		li	t0,FILLBASE
		addu	t1,t0,dcachesize		# limit = base + icachesize 
1:		cache   L1CACHEOP(L1C_D,L1C_OP_IDXINVAL),0(t0)
		addu	t0,LINESIZE
		bne	t0,t1,1b

		j	ra


END(rm5200_l1cache_flush_d)


/*  *********************************************************************
    *  End
    ********************************************************************* */
