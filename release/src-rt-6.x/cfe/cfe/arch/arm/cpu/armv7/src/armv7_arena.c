/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Physical Memory (arena) manager		File: armv7_arena.c
    *  
    *  This module describes the physical memory available to the 
    *  firmware.
    *  
    *  Author: 
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

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "lib_arena.h"

#include "cfe_error.h"

#include "cfe.h"
#include "cfe_mem.h"

#include "initdata.h"

#define _NOPROTOS_
#include "cfe_boot.h"
#undef _NOPROTOS_


/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define ARENA_RANGE(bottom,top,type) arena_markrange(&cfe_arena,(uint64_t)(bottom), \
                                     (uint64_t)(top)-(uint64_t)bottom+1,(type),NULL)

#define MEG	(1024*1024)
#define KB      1024
#define PAGESIZE 4096
#define CFE_BOOTAREA_SIZE (256*KB)
#define CFE_BOOTAREA_ADDR 0x20000000

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

extern arena_t cfe_arena;

void armv7_arena_init(void);
void armv7_pagetable_init(uint64_t *ptaddr,unsigned int physaddr);


/*  *********************************************************************
    *  armv7_arena_init()
    *  
    *  Create the initial map of physical memory
    *
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void armv7_arena_init(void)
{
    int64_t memleft;

    arena_init(&cfe_arena,0x0,0x100000000LL);	/* 2^32 physical bytes */

    /*
     * Mark the ranges from the SB1250's memory map
     */

    ARENA_RANGE(0x0000000000,0x000FFFFFFF,MEMTYPE_DRAM_NOTINSTALLED);

    /*
     * Now, fix up the map with what is known about *this* system.
     *
     * Do each 256MB chunk.
     */

    memleft = ((int64_t)mem_totalsize) << 10;

    arena_markrange(&cfe_arena,0x00000000,memleft,MEMTYPE_DRAM_AVAILABLE,NULL);

    /*
     * Do the boot ROM
     */

    arena_markrange(&cfe_arena,0xFFFF0000,2*1024*1024,MEMTYPE_BOOTROM,NULL);

}


/*  *********************************************************************
    *  ARMV7_PAGETABLE_INIT(ptaddr,physaddr)
    *  
    *  This routine constructs the page table.  256KB is mapped
    *  starting at physical address 'physaddr' - the resulting
    *  table entries are placed at 'ptaddr'
    *  
    *  Input parameters: 
    *  	   ptaddr - base of page table
    *  	   physaddr - starting physical addr of area to map
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void armv7_pagetable_init(uint64_t *ptaddr,unsigned int physaddr)
{
	/* We do it in cfe_l1cache_on() */
}
