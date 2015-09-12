/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Test commands				File: ui_memtest.c
    *  
    *  A simple memory test
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

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "cfe_devfuncs.h"

#include "cfe_error.h"

#include "ui_command.h"
#include "cfe.h"

#include "bsp_config.h"

#include "cfe_mem.h"


#ifdef __long64
static int ui_cmd_memorytest(ui_cmdline_t *cmd,int argc,char *argv[]);
#endif

#ifndef _SB_MAKE64
#define _SB_MAKE64(x) ((uint64_t)(x))
#endif
#ifndef _SB_MAKEMASK
#define _SB_MAKEMASK(v,n) (_SB_MAKE64((_SB_MAKE64(1)<<(v))-1) << _SB_MAKE64(n))
#endif
#ifndef _SB_MAKEMASK1
#define _SB_MAKEMASK1(n) (_SB_MAKE64(1) << _SB_MAKE64(n))
#endif


int ui_init_memtestcmds(void);

int ui_init_memtestcmds(void)
{
#ifdef __long64
    cmd_addcmd("memorytest",
	       ui_cmd_memorytest,
	       NULL,
	       "Tests all available memory",
	       "",
	       "-loop;Loop forever or until keypress|"
	       "-stoponerror;Stop if error occurs while looping|"
	       "-cca=*;Use specified cacheability attribute|"
	       "-arena=*;Test only specified arena index");
#endif
    return 0;
}


#ifdef __long64
/* extensive memory tests */

static void inline uacwrite(volatile long *srcadr,long *dstadr) 
{
__asm __volatile ("ld $8, 0(%0) ; "
		  "ld $9, 8(%0) ; "  
		  "ld $10, 16(%0) ; " 
		  "ld $11, 24(%0) ; " 
		  "sync ; " 
		  "sd $8,  0(%1) ; " 
		  "sd $9,  8(%1) ; " 
		  "sd $10, 16(%1) ; " 
		  "sd $11, 24(%1) ; " 
		  "sync" :: "r"(srcadr),"r"(dstadr) : "$8","$9","$10","$11");
}


#define TEST_DATA_LEN 4
#define CACHE_LINE_LEN 32

static int ui_cmd_memorytest(ui_cmdline_t *cmd,int argc,char *argv[])
{

    static volatile long test_data[TEST_DATA_LEN] = {
	0xaaaaaaaaaaaaaaaa, 0x5555555555555555, 0xcccccccccccccccc, 0x3333333333333333, /* one cache line */
    };
    int arena, exitLoop;
    int error;
    int arena_type;
    uint64_t arena_start, arena_size;
    long phys_addr, offset, mem_base, cache_mem_base, i;
    long *dst_adr, *cache_dst_adr;
    long cda,tda;
    int forever;
    int passcnt;
    int stoponerr = 0;
    int cca = K_CALG_UNCACHED_ACCEL;
    int arenanum = -1;
    char *x;

    arena = 0;
    exitLoop = 0; 
    offset = 0; 
    mem_base = 0;
    passcnt = 0;
    error = 0;

    forever = cmd_sw_isset(cmd,"-loop");
    stoponerr = cmd_sw_isset(cmd,"-stoponerror");
    if (cmd_sw_value(cmd,"-cca",&x)) cca = atoi(x);
    if (cmd_sw_value(cmd,"-arena",&x)) arenanum = atoi(x);

    printf("Available memory arenas:\n");
    while (cfe_arena_enum(arena, &arena_type, &arena_start, &arena_size, FALSE) == 0) {
	phys_addr = (long) arena_start;    /* actual physical address */
	mem_base = PHYS_TO_XKPHYS(cca, phys_addr); /* virtual address */
	xprintf("phys = %016llX, virt = %016llX, size = %016llX\n", phys_addr, mem_base, arena_size);
	arena++;
	}

    printf("\nTesting memory.\n");
    do {

	passcnt++;
	if (forever) {
	    if (console_status()) break;
	    printf("***** Iteration %d *****\n",passcnt);
	    }

	arena = 0; 
	exitLoop = 0;
	error = 0;

	while (cfe_arena_enum(arena, &arena_type, &arena_start, &arena_size, FALSE) == 0) {

	    if ((arenanum >= 0) && (arena != arenanum)) {
		arena++;
		continue;
		}

	    test_data[0] = 0xAAAAAAAAAAAAAAAA;
	    test_data[1] = 0x5555555555555555;
	    test_data[2] = 0xCCCCCCCCCCCCCCCC;
	    test_data[3] = 0x3333333333333333;

	    phys_addr = (long) arena_start;    /* actual physical address */
	    mem_base = PHYS_TO_XKPHYS(cca, phys_addr); /* virtual address */
	    cache_mem_base = PHYS_TO_K0(phys_addr);

	    xprintf("\n");
	    xprintf("Testing: phys = %016llX, virt = %016llX, size = %016llX\n", phys_addr, mem_base, arena_size);

	    xprintf("Writing: a/5/c/3\n");

	    for (offset = 0; (offset < arena_size); offset += CACHE_LINE_LEN) {
		dst_adr = (long*)(mem_base+offset);
		uacwrite(test_data, dst_adr);
		}

	    xprintf("Reading: a/5/c/3\n");

	    for (offset = 0; (offset < arena_size); offset += CACHE_LINE_LEN) {
		dst_adr = (long*)(mem_base+offset);
		cache_dst_adr = (long*)(mem_base+offset);
		for (i = 0; i < TEST_DATA_LEN; i++) {
		    cda = cache_dst_adr[i];
		    tda = test_data[i];
		    if (cda != tda) {
			xprintf("mem[%016llX] %016llX != %016llX\n",
				mem_base+offset+(i*8), cda, tda);
			exitLoop = 1;  
			}	
		    }
		if (exitLoop) break;
		}


	    if (exitLoop) {
		exitLoop = 0;
		error++;
		arena++;
		continue;
		}

	    xprintf("Writing: address|5555/inv/aaaa|address\n");
	    exitLoop = 0;

	    for (offset = 0; (offset < arena_size); offset += CACHE_LINE_LEN) {
		dst_adr = (long*)(mem_base+offset);
		test_data[0] = ((long)dst_adr<<32)|0x55555555;
		test_data[1] = ~test_data[0];
		test_data[2] = 0xaaaaaaaa00000000|((long)dst_adr & 0xffffffff);
		test_data[3] = ~test_data[2];
		uacwrite(test_data, dst_adr);
		}

	    xprintf("Reading: address|5555/inv/aaaa|address\n");

	    for (offset = 0; (offset < arena_size); offset += CACHE_LINE_LEN) {
		dst_adr = (long*)(mem_base+offset);
		test_data[0] = ((long)dst_adr<<32)|0x55555555;
		test_data[1] = ~test_data[0];
		test_data[2] = 0xaaaaaaaa00000000|((long)dst_adr & 0xffffffff);
		test_data[3] = ~test_data[2];
		cache_dst_adr = (long*)(mem_base+offset);
		for (i = 0; i < TEST_DATA_LEN; i++) {
		    cda = cache_dst_adr[i];
		    tda = test_data[i];
		    if (cda != tda) {
			xprintf("mem[%016llX] %016llX != %016llX\n",
				mem_base+offset+(i*8),cda,tda);
			exitLoop = 1;
			}	
		    }
		if (exitLoop) break;
		}

	    if (exitLoop) {
		error++;
		exitLoop = 0;
		if (stoponerr) forever = 0;
		}

	    arena++;
	    }
	} while (forever);

    if (error) printf("Failing address: %016llX\n",mem_base+offset);

    return error ? -1 : 0;
}

#endif
