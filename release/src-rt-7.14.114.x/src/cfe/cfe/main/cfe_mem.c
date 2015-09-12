/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Physical Memory (arena) manager		File: cfe_mem.c
    *  
    *  This module describes the physical memory available to the 
    *  firmware.
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

#include "cpu_config.h"		/* for definition of CPUCFG_ARENAINIT */

#include "addrspace.h"		/* for macros dealing with addresses */


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

arena_t cfe_arena;
extern void _setcontext(int64_t);

unsigned int mem_bootarea_start;
unsigned int mem_bootarea_size;

extern void CPUCFG_ARENAINIT(void);
extern void cfe_bootarea_init(void);
extern void CPUCFG_PAGETBLINIT(uint64_t *ptaddr,unsigned int ptstart);



/*  *********************************************************************
    *  cfe_arena_init()
    *  
    *  Create the initial map of physical memory
    *
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void cfe_arena_init(void)
{
    uint64_t memlo,memhi;

    /*
     * This macro expands via cpu_config.h to an appropriate function
     * name for creating an empty arena appropriately for our CPU
     */

    CPUCFG_ARENAINIT();

    /*
     * Round the area used by the firmware to a page boundary and
     * mark it in use
     */

    memhi = PHYSADDR((mem_topofmem + 4095) & ~4095);
    memlo = PHYSADDR(mem_bottomofmem) & ~4095;

    ARENA_RANGE(memlo,memhi-1,MEMTYPE_DRAM_USEDBYFIRMWARE);

    /*
     * Create the initial page table
     */

    cfe_bootarea_init();

}


/*  *********************************************************************
    *  cfe_arena_enum(idx,type,start,size)
    *  
    *  Enumerate available memory.  This is called by the user
    *  API dispatcher so that operating systems can determine what
    *  memory regions are available to them.
    *  
    *  Input parameters: 
    *  	   idx - index.  Start at zero and increment until an error
    *  	         is returned.
    *  	   type,start,size: pointers to variables to receive the 
    *  	         arena entry's information
    *	   allrecs - true to retrieve all records, false to retrieve
    * 		 only available DRAM
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   CFE_ERR_NOMORE if idx is beyond the last entry
    ********************************************************************* */

int cfe_arena_enum(int idx,int *type,uint64_t *start,uint64_t *size,int allrecs)
{
    arena_node_t *node;
    queue_t *qb;
    arena_t *arena = &cfe_arena;


    for (qb = (arena->arena_list.q_next); qb != &(arena->arena_list);
	 qb = qb->q_next) {
	node = (arena_node_t *) qb;

	if (allrecs || (!allrecs && (node->an_type == MEMTYPE_DRAM_AVAILABLE))) {
	    if (idx == 0) {
		*type = node->an_type;
		*start = node->an_address;
		*size = node->an_length;
		return 0;
		}
	    idx--;
	    }
	}

    return CFE_ERR_NOMORE;

}

/*  *********************************************************************
    *  cfe_arena_loadcheck(start,size)
    *  
    *  Determine if the specified memory area is within the available
    *  DRAM.  This is used while loading executables to be sure we 
    *  don't trash the firmware.
    *  
    *  Input parameters: 
    *  	   start - starting physical address
    *  	   size - size of requested region
    *  	   
    *  Return value:
    *  	   true - ok to copy memory here
    *  	   false - not ok, memory overlaps firmware
    ********************************************************************* */

int cfe_arena_loadcheck(uintptr_t start,unsigned int size)
{
    arena_node_t *node;
    queue_t *qb;
    arena_t *arena = &cfe_arena;

    /*
     * If the address is in our boot area, it's okay
     * for it to be a virtual address.
     */

    if ((start >= BOOT_START_ADDRESS) && 
	((start+size) <= (BOOT_START_ADDRESS+BOOT_AREA_SIZE))) {
	return TRUE;
	}

    /*
     * Otherwise, make a physical address.
     */

    start = PHYSADDR(start);

    /*
     * Because all of the arena nodes of the same type are
     * coalesced together, all we need to do is determine if the
     * requested region is entirely within an arena node,
     * so there's no need to look for things that span nodes.
     */

    for (qb = (arena->arena_list.q_next); qb != &(arena->arena_list);
	 qb = qb->q_next) {
	node = (arena_node_t *) qb;

	/* If the memory is available, the region is OK. */

	if ((start >= node->an_address) &&
	    ((start+size) <= (node->an_address+node->an_length)) &&
	    (node->an_type == MEMTYPE_DRAM_AVAILABLE)) {
	    return TRUE;
	    }
	}

    /*
     * Otherwise, it's not.  We could go through the arena again and
     * look for regions of other types that intersect the requested 
     * region, to get a more detailed error, but this'll do.
     */

    return FALSE;
       
}
