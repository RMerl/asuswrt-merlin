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
 *
 * The RM5200 doesn't have a built-in memory controller, so
 * the draminit routine is just "board_dram_init" to call a
 * board-specific routine.
 * 
 * There's no secondary CPU cores, so all the ALTCPU entrypoints
 * are null routines.
 */

#define CPUCFG_CPUINIT		rm5200_cpuinit
#define CPUCFG_ALTCPU_START1	rm5200_null_init
#define CPUCFG_ALTCPU_START2	rm5200_null_init
#define CPUCFG_ALTCPU_RESET	rm5200_null_init
#define CPUCFG_CPURESTART	rm5200_null_init
#define CPUCFG_DRAMINIT		board_dram_init
#define CPUCFG_CACHEOPS		rm5200_cacheops
#define CPUCFG_ARENAINIT	rm5200_arena_init
#define CPUCFG_PAGETBLINIT	rm5200_pagetable_init
#define CPUCFG_TLBHANDLER	rm5200_tlbhandler
#define CPUCFG_DIAG_TEST1	rm5200_null_init
#define CPUCFG_DIAG_TEST2	rm5200_null_init

/*
 * Hazard macro
 */

#define HAZARD nop ; nop ; nop ; nop ; nop ; nop ; nop 
#define ERET eret
