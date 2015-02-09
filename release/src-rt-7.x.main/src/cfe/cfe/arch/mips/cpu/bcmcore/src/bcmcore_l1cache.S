/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  L1C initialization			File: bcmcore_l1cache.S
    *  
    *  This module contains code to initialize the CPU's caches
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
#include "bsp_config.h"

		.text

		.set push	
		.set mips32

/*  *********************************************************************
    *  BCMCORE_L1CACHE_INIT()
    *  
    *  Initialize the L1 Cache
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

LEAF(bcmcore_l1cache_init)

	/*
	 * Size the icache
	 */

		mfc0	t0,C0_CONFIG,1

	/* 
	 * Sets Per Way = 64 << CFG_IS
	 */

		li	t2,M_CFG_IS
		and	t2,t0
		srl	t2,S_CFG_IS
		li	t1,64
		sll	t1,t2

	/*
	 * Associativity = 1 + CFG_IA
	 */

		li	t2,M_CFG_IA
		and	t2,t0
		srl	t2,S_CFG_IA
		addiu	t2,1

	/* 
	 * Associativity * Sets Per Way
	 */

		multu	t1,t2
		mflo	t1
	
	/* 
	 * Line Size = 2 << CFG_IL
	 */

		li	t2,M_CFG_IL
		and	t2,t0
		srl	t2,S_CFG_IL
		li	t0,2
		sll	t2,t0,t2
		
	/* 
	 * Instruction Cache Size = 
	 * Associativity * Line Size * Sets Per Way
	 */

		multu	t1,t2
		mflo	t0

	/*
	 * Zero the icache
	 */

		mtc0	zero,C0_TAGLO
		mtc0	zero,C0_TAGHI
		li	t1,K0BASE
1:		sub	t0,t0,t2
		cache	Index_Store_Tag_I,0(t1)
		add	t1,t2
		bne	t0,zero,1b

	/*
	 * Size the dcache
	 */

		mfc0	t0,C0_CONFIG,1

	/* 
	 * Sets Per Way = 64 << CFG_DS
	 */

		li	t2,M_CFG_DS
		and	t2,t0
		srl	t2,S_CFG_DS
		li	t1,64
		sll	t1,t2


	/*
	 * Associativity = 1 + CFG_DA
	 */

		li	t2,M_CFG_DA
		and	t2,t0
		srl	t2,S_CFG_DA
		addiu	t2,1

	/* 
	 * Associativity * Sets Per Way
	 */

		multu	t1,t2
		mflo	t1
	
	/* 
	 * Line Size = 2 << CFG_DL
	 */

		li	t2,M_CFG_DL
		and	t2,t0
		srl	t2,S_CFG_DL
		li	t0,2
		sll	t2,t0,t2
		
	/* 
	 * Data Cache Size = 
	 * Associativity * Line Size * Sets Per Way
	 */

		multu	t1,t2
		mflo	t0

	/*
	 * Zero the dcache
	 */

		mfc0	t1,C0_CONFIG
		andi	t1,M_CFG_AR
		beqz	t1,r1		# t1 == 0 if mips32r1

		mtc0	zero,C0_TAGLO,2	# For mips32r2 the D$ Tags are in select 2
		mtc0	zero,C0_TAGHI,2
		b	2f
		nop

r1:		mtc0	zero,C0_TAGLO
		mtc0	zero,C0_TAGHI

2:		li	t1,K0BASE
1:		sub	t0,t0,t2
		cache	Index_Store_Tag_D,0(t1)
		add	t1,t2
		bne	t0,zero,1b

	# Now make K0 cacheable

		mfc0	v0,C0_CONFIG		# get current CONFIG register
		srl	v0,v0,3			# strip out K0 bits
		sll	v0,v0,3			# k0 bits now zero
		or	v0,v0,K_CFG_K0COH_CACHEABLE # K0 is cacheable.
		mtc0	v0,C0_CONFIG
		nop

		jr	ra

END(bcmcore_l1cache_init)

/*  *********************************************************************
    *  BCMCORE_L1CACHE_INVAL_I()
    *  
    *  Invalidate the entire ICache
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

LEAF(bcmcore_l1cache_inval_i)
		
	/*
	 * Size the icache
	 */

		mfc0	t0,C0_CONFIG,1

	/* 
	 * Sets Per Way = 64 << CFG_IS
	 */

		li	t2,M_CFG_IS
		and	t2,t0
		srl	t2,S_CFG_IS
		li	t1,64
		sll	t1,t2

	/*
	 * Associativity = 1 + CFG_IA
	 */

		li	t2,M_CFG_IA
		and	t2,t0
		srl	t2,S_CFG_IA
		addiu	t2,1

	/* 
	 * Associativity * Sets Per Way
	 */

		multu	t1,t2
		mflo	t1
	
	/* 
	 * Line Size = 2 << CFG_IL
	 */

		li	t2,M_CFG_IL
		and	t2,t0
		srl	t2,S_CFG_IL
		li	t0,2
		sll	t2,t0,t2
		
	/* 
	 * Instruction Cache Size = 
	 * Associativity * Line Size * Sets Per Way
	 */

		multu	t1,t2
		mflo	t0

	/*
	 * Invalidate the icache
	 */

		mtc0	zero,C0_TAGLO
		mtc0	zero,C0_TAGHI
		li	t1,K0BASE
1:		sub	t0,t0,t2
		cache	Index_Invalidate_I,0(t1)
		add	t1,t2
		bne	t0,zero,1b

		j	ra

END(bcmcore_l1cache_inval_i)

/*  *********************************************************************
    *  BCMCORE_L1CACHE_FLUSH_D()
    *  
    *  Flush the entire DCache
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

LEAF(bcmcore_l1cache_flush_d)

	/*
	 * Size the dcache
	 */

		mfc0	t0,C0_CONFIG,1

	/* 
	 * Sets Per Way = 64 << CFG_DS
	 */

		li	t2,M_CFG_DS
		and	t2,t0
		srl	t2,S_CFG_DS
		li	t1,64
		sll	t1,t2


	/*
	 * Associativity = 1 + CFG_DA
	 */

		li	t2,M_CFG_DA
		and	t2,t0
		srl	t2,S_CFG_DA
		addiu	t2,1

	/* 
	 * Associativity * Sets Per Way
	 */

		multu	t1,t2
		mflo	t1
	
	/* 
	 * Line Size = 2 << CFG_DL
	 */

		li	t2,M_CFG_DL
		and	t2,t0
		srl	t2,S_CFG_DL
		li	t0,2
		sll	t2,t0,t2
		
	/* 
	 * Data Cache Size = 
	 * Associativity * Line Size * Sets Per Way
	 */

		multu	t1,t2
		mflo	t0

	/*
	 * Flush the dcache
	 */

		mtc0	zero,C0_TAGLO
		mtc0	zero,C0_TAGHI
		li	t1,K0BASE
1:		sub	t0,t0,t2
		cache	Index_Writeback_Inv_D,0(t1)
		add	t1,t2
		bne	t0,zero,1b

		j	ra

END(bcmcore_l1cache_flush_d)


		.set pop

/*  *********************************************************************
    *  End
    ********************************************************************* */
