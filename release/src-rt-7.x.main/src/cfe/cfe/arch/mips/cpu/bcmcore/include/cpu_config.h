/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  CPU Configuration file			File: cpu_config.h
    *  
    *  This file contains the names of the routines to be used
    *  in the dispatch table in init_mips.S
    *
    *  It lives here in the CPU directory so we can direct
    *  the init calls to routines named in this directory.
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

/*
 */

#define CPUCFG_CPUINIT		bcmcore_cpuinit
#define CPUCFG_ALTCPU_START1	bcmcore_null
#define CPUCFG_ALTCPU_START2	bcmcore_null
#define CPUCFG_ALTCPU_RESET	bcmcore_null
#define CPUCFG_CPURESTART	bcmcore_cpurestart
#define CPUCFG_DRAMINIT		board_draminit		/* no dram on CPU */
#define CPUCFG_CACHEOPS		bcmcore_cacheops
#define CPUCFG_ARENAINIT	bcmcore_arena_init
#define CPUCFG_PAGETBLINIT	bcmcore_pagetable_init
#define CPUCFG_TLBHANDLER	bcmcore_tlbhandler
#define CPUCFG_DIAG_TEST1	bcmcore_null
#define CPUCFG_DIAG_TEST2	bcmcore_null

/* 
 * The BCMCORE ticks CP0 every other cycle.
 */

#define CPUCFG_CYCLESPERCPUTICK	2

/*
 * Hazard macro
 */

#define HAZARD nop ; nop ; nop ; nop ; nop ; nop ; nop

/*
 * Return from exception macro
 */

#ifdef BCM4710
#define ERET \
		.set push ; \
		.set mips4 ; \
		nop ; \
		nop ; \
		eret ; \
		.set pop 
#else
#define ERET \
		.set push ; \
		.set mips4 ; \
		eret ; \
		.set pop 
#endif
