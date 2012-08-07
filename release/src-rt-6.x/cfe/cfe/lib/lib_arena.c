/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Arena Manager				File lib_arena.c
    *  
    *  This module manages the _arena_, a sorted linked list of 
    *  memory regions and attributes.  We use this to keep track
    *  of physical memory regions and what is assigned to them.
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
#include "lib_queue.h"
#include "lib_arena.h"
#include "lib_malloc.h"

/*  *********************************************************************
    *  arena_print(arena,msg)
    *  
    *  Debug routine to print out an arena entry
    *  
    *  Input parameters: 
    *  	   arena - arena descriptor
    *  	   msg - heading message
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

#ifdef _TESTPROG_
static void arena_print(arena_t *arena,char *msg)
{
    arena_node_t *node;
    queue_t *qb;

    printf("%s\n",msg);

    for (qb = (arena->arena_list.q_next); qb != &(arena->arena_list);
	 qb = qb->q_next) {
	node = (arena_node_t *) qb;

	printf("Start %5I64d End %5I64d Type %d\n",
	       node->an_address,
	       node->an_address+node->an_length,
	       node->an_type);

	}

}
#endif

/*  *********************************************************************
    *  arena_init(arena,physmembase,physmemsize)
    *  
    *  Initialize an arena descriptor.  The arena is typically
    *  created to describe the entire physical memory address space.
    *  
    *  Input parameters: 
    *  	   arena - arena descriptor
    *  	   physmembase - base of region to manage (usually 0)
    *  	   physmemsize - size of region to manage (typically maxint)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void arena_init(arena_t *arena,uint64_t physmembase,uint64_t physmemsize)
{
    arena_node_t *an = NULL;

    an = (arena_node_t *) KMALLOC(sizeof(arena_node_t),sizeof(uint64_t));


    arena->arena_base = physmembase;
    arena->arena_size = physmemsize;

    an->an_address = physmembase;
    an->an_length = physmemsize;
    an->an_type = 0;
    an->an_descr = NULL;

    q_init(&(arena->arena_list));
    q_enqueue(&(arena->arena_list),(queue_t *) an);
}


/*  *********************************************************************
    *  arena_find(arena,pt)
    *  
    *  Locate the arena node containing a particular point in the
    *  address space.  This routine walks the list and finds the node
    *  whose address range contains the specified point.
    *  
    *  Input parameters: 
    *  	   arena - arena descriptor
    *  	   pt - point to look for
    *  	   
    *  Return value:
    *  	   arena node pointer, or NULL if no node found
    ********************************************************************* */

static arena_node_t *arena_find(arena_t *arena,uint64_t pt)
{
    queue_t *qb;
    arena_node_t *an;

    for (qb = (arena->arena_list.q_next); qb != &(arena->arena_list);
	 qb = qb->q_next) {
	an = (arena_node_t *) qb;

	if ((pt >= an->an_address) &&
	    (pt < (an->an_address + an->an_length))) return an;

	}

    return NULL;
}

/*  *********************************************************************
    *  arena_split(arena,splitpoint)
    *  
    *  Split the node containing the specified point.  When we carve
    *  the arena up, we split the arena at the points on the edges
    *  of the new region, change their types, and then coalesce the
    *  arena.  This handles the "split" part of that process.
    *  
    *  Input parameters: 
    *  	   arena - arena descriptor
    *  	   splitpoint - address to split arena at
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   -1 if could not split
    ********************************************************************* */

static int arena_split(arena_t *arena,uint64_t splitpoint)
{
    arena_node_t *node;
    arena_node_t *newnode;

    /* 
     * Don't need to split if it's the *last* address in the arena
     */

    if (splitpoint == (arena->arena_base+arena->arena_size)) return 0;

    /*
     * Find the block that contains the split point.
     */

    node = arena_find(arena,splitpoint);
    if (node == NULL) return -1;	/* should not happen */

    /*
     * If the address matches exactly, don't need to split
     */
    if (node->an_address == splitpoint) return 0;

    /*
     * Allocate a new node and adjust the length of the node we're
     * splitting.
     */

    newnode = (arena_node_t *) KMALLOC(sizeof(arena_node_t),sizeof(uint64_t));

    newnode->an_length = node->an_length - (splitpoint - node->an_address);
    node->an_length = splitpoint - node->an_address;
    newnode->an_address = splitpoint;
    newnode->an_type = node->an_type;

    /*
     * Put the new node in the arena
     */

    q_enqueue(node->an_next.q_next,(queue_t *) newnode);

    return 0;
}

/*  *********************************************************************
    *  arena_coalesce(arena)
    *  
    *  Coalesce the arena, merging regions that have the same type
    *  together.  After coalescing, no two adjacent nodes will
    *  have the same type.
    *  
    *  Input parameters: 
    *  	   arena - arena descriptor
    *
    *  Return value:
    *      nothing
    ********************************************************************* */

static void arena_coalesce(arena_t *arena)
{
    arena_node_t *node;
    arena_node_t *nextnode;
    int removed;
    queue_t *qb;

    do {
	removed = 0;
	for (qb = (arena->arena_list.q_next); qb != &(arena->arena_list);
	     qb = qb->q_next) {

	    node = (arena_node_t *) qb;
	    nextnode = (arena_node_t *) node->an_next.q_next;

	    if ((queue_t *) nextnode == &(arena->arena_list)) break;

	    if (node->an_type == nextnode->an_type) {
		node->an_length += nextnode->an_length;
		q_dequeue((queue_t *) nextnode);
		KFREE(nextnode);
		removed++;
		}
	    }
	} while (removed > 0);
}


/*  *********************************************************************
    *  arena_markrange(arena,address,length,type,descr)
    *  
    *  Mark a region in the arena, changing the types of nodes and
    *  splitting nodes as necessary.  This routine is called for
    *  each region we want to add.  The order of marking regions is
    *  important, since new marks overwrite old ones.  Therefore, you 
    *  could mark a whole range as DRAM, and then mark sub-regions
    *  within that as used by firmware.
    *  
    *  Input parameters: 
    *  	   arena - arena descriptor
    *  	   address,length - region to mark
    *  	   type - type code for region
    *  	   descr - text description of region (optional)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   -1 if error
    ********************************************************************* */

int arena_markrange(arena_t *arena,uint64_t address,uint64_t length,int type,char *descr)
{
    queue_t *qb;
    arena_node_t *node;

    /*
     * Range check the region we want to mark
     */

    if ((address < arena->arena_base) || 
	((address+length) > (arena->arena_base + arena->arena_size))) {
	return -1;
	}

    /*
     * Force the arena to be split at the two points at the
     * beginning and end of the range we want.  If we have
     * problems, coalesce the arena again and get out.
     */

    if (arena_split(arena,address) < 0) {
	/* don't need to coalesce, we didn't split */
	return -1;
	}
    if (arena_split(arena,(address+length)) < 0) {
	/* recombine nodes split above */
	arena_coalesce(arena);
	return -1;
	}

    /*
     * Assuming we've split the arena at the beginning and ending
     * split points, we'll never mark any places outside the range
     * specified in the "Address,length" args.
     */

    for (qb = (arena->arena_list.q_next); qb != &(arena->arena_list);
	 qb = qb->q_next) {
	node = (arena_node_t *) qb;

	if ((node->an_address >= address) &&
	    ((node->an_address + node->an_length) <= (address+length))) {
	    node->an_type = type;
	    node->an_descr = descr;
	    }
	}

    /*
     * Now, coalesce adjacent pieces with the same type back together again
     */

    arena_coalesce(arena);

    return 0;
}



/*  *********************************************************************
    *  main(argc,argv)
    *  
    *  Test program.
    *  
    *  Input parameters: 
    *  	   argc,argv - guess
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

#ifdef _TESTPROG_
void main(int argc,char *argv[])
{
    arena_t arena;

    arena_init(&arena,0,1024);

    arena_markrange(&arena,100,10,1);
    arena_markrange(&arena,120,10,2);
    arena_markrange(&arena,140,10,3);
    arena_print(&arena,"Before big markrange---------");

    arena_markrange(&arena,50,200,4);
    arena_print(&arena,"after big markrange---------");

}
#endif
