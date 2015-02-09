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

/*
 */

#define CPUCFG_CPUINIT		sb1250_cpuinit
#define CPUCFG_ALTCPU_START1	sb1250_altcpu_start1
#define CPUCFG_ALTCPU_START2	sb1250_altcpu_start2
#define CPUCFG_ALTCPU_RESET	sb1250_altcpu_reset
#define CPUCFG_CPURESTART	sb1250_cpurestart
#define CPUCFG_DRAMINIT		sb1250_dram_init
#define CPUCFG_CACHEOPS		sb1250_cacheops
#define CPUCFG_ARENAINIT	sb1250_arena_init
#define CPUCFG_PAGETBLINIT	sb1250_pagetable_init
#define CPUCFG_TLBHANDLER	sb1250_tlbhandler
#define CPUCFG_CERRHANDLER	sb1250_cerrhandler

#ifdef _FUNCSIM_
#define CPUCFG_DIAG_TEST1	diag_null
#else
#define CPUCFG_DIAG_TEST1	diag_main
#endif
#define CPUCFG_DIAG_TEST2	0

/*
 * Hazard macro
 */

#define HAZARD .set push ; .set mips64 ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; .set pop
#define ERET eret

/*
 * Let others know we can do coherent DMA
 */

#define CPUCFG_COHERENT_DMA	1
