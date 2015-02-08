/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Memory Map commands			File: ui_memcmds.c
    *  
    *  Memory Manager user interface 
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

#include "ui_command.h"

#include "cfe_mem.h"

#include "cfe.h"


const static char * const cfe_arenatypes[] = {
    "Reserved",
    "DRAM (available)",
    "Memory Controller (unused)",
    "DRAM (in use by firmware)",
    "ROM",
    "I/O Registers",
    "Not available",
    "L2 Cache",
    "LDT/PCI",
    NULL};

extern arena_t cfe_arena;



int ui_init_memcmds(void);

static int ui_cmd_physmap(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_heapstats(ui_cmdline_t *cmd,int argc,char *argv[]);


int ui_init_memcmds(void)
{
    cmd_addcmd("show memory",
	       ui_cmd_physmap,
	       NULL,
	       "Display the system physical memory map.",
	       "show memory [-a]\n\n"
	       "This command displays the arena, or system physical memory map\n"
	       "You can use this command to determine the areas of RAM that will\n"
	       "be made available to operating systems.\n",
	       "-a;Display all entries in the map, not just the blocks\n"
	       "of available memory.");

    cmd_addcmd("show heap",
	       ui_cmd_heapstats,
	       NULL,
	       "Display information about CFE's heap",
	       "show heap\n\n"
	       "This is a debugging command that can be used to determine the health\n"
	       "of CFE's internal memory manager.",
	       "");
	       
    return 0;
}

static int ui_cmd_heapstats(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int res;
    memstats_t stats;

    res = KMEMSTATS(&stats);

    xprintf("\n");
    xprintf("Total bytes:       %d\n",stats.mem_totalbytes);
    xprintf("Free bytes:        %d\n",stats.mem_freebytes);
    xprintf("Free nodes:        %d\n",stats.mem_freenodes);
    xprintf("Allocated bytes:   %d\n",stats.mem_allocbytes);
    xprintf("Allocated nodes:   %d\n",stats.mem_allocnodes);
    xprintf("Largest free node: %d\n",stats.mem_largest);
    xprintf("Heap status:       %s\n",(res == 0) ? "CONSISTENT" : "CORRUPT!");
    xprintf("\n");

    return res;
}


#define PHYSMAP_FLG_ALL		1
#define PHYSMAP_FLG_AVAIL	2

static int ui_cmd_physmap(ui_cmdline_t *cmd,int argc,char *argv[])
{
    arena_node_t *node;
    queue_t *qb;
    arena_t *arena = &cfe_arena;
    int flags = 0;

    if (cmd_sw_isset(cmd,"-a")) {
	flags |= PHYSMAP_FLG_ALL;
	}
    else {
	flags = PHYSMAP_FLG_AVAIL;
	}


    xprintf("Range Start  Range End    Range Size     Description\n");
    xprintf("------------ ------------ -------------- --------------------\n");

    for (qb = (arena->arena_list.q_next); qb != &(arena->arena_list);
	 qb = qb->q_next) {
	node = (arena_node_t *) qb;

	if ((flags & PHYSMAP_FLG_ALL) ||
	    ((flags & PHYSMAP_FLG_AVAIL) && (node->an_type == MEMTYPE_DRAM_AVAILABLE)))  {

	    xprintf("%012llX-%012llX (%012llX) %s\n",
		    node->an_address,
		    node->an_address+node->an_length-1,
		    node->an_length,
		    cfe_arenatypes[node->an_type]);
	    }

	}

    return 0;
}
