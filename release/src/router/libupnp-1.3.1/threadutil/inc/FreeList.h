///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2003 Intel Corporation 
// All rights reserved. 
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met: 
//
// * Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer. 
// * Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution. 
// * Neither name of Intel Corporation nor the names of its contributors 
// may be used to endorse or promote products derived from this software 
// without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#ifndef FREE_LIST_H
#define FREE_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <malloc.h>


#include "ithread.h"
#include <errno.h>

/****************************************************************************
 * Name: FreeListNode
 *
 *  Description:
 *      free list node. points to next free item.
 *      memory for node is borrowed from allocated items.
 *      Internal Use Only.
 *****************************************************************************/
typedef struct FREELISTNODE
{
	struct FREELISTNODE*next; //pointer to next free node
} FreeListNode;


/****************************************************************************
 * Name: FreeList
 *
 *  Description:
 *      Stores head and size of free list, as well as mutex for protection.
 *      Internal Use Only.
 *****************************************************************************/
typedef struct FREELIST
{
	FreeListNode *head; //head of free list
	size_t element_size;	//size of elements in free 
 							//list
 	int maxFreeListLength; //max size of free structures 
						 //to keep
	int freeListLength; //current size of free list
        
}FreeList;

/****************************************************************************
 * Function: FreeListInit
 *
 *  Description:
 *      Initializes Free List. Must be called first.
 *      And only once for FreeList.
 *  Parameters:
 *      free_list  - must be valid, non null, pointer to a linked list.
 *      size_t -     size of elements to store in free list
 *      maxFreeListSize - max size that the free list can grow to
 *                        before returning memory to O.S.
 *  Returns:
 *      0 on success. Nonzero on failure.
 *      Always returns 0.
 *****************************************************************************/
int FreeListInit(FreeList *free_list, 
				 size_t elementSize, 
				 int maxFreeListSize);

/****************************************************************************
 * Function: FreeListAlloc
 *
 *  Description:
 *      Allocates chunk of set size.
 *      If a free item is available in the list, returnes the stored item.
 *      Otherwise calls the O.S. to allocate memory.
 *  Parameters:
 *      free_list  - must be valid, non null, pointer to a linked list.
 *  Returns:
 *      Non NULL on success. NULL on failure.
 *****************************************************************************/
void * FreeListAlloc (FreeList *free_list);

/****************************************************************************
 * Function: FreeListFree
 *
 *  Description:
 *      Returns an item to the Free List.
 *      If the free list is smaller than the max size than
 *      adds the item to the free list.
 *      Otherwise returns the item to the O.S.
 *  Parameters:
 *      free_list  - must be valid, non null, pointer to a linked list.
 *  Returns:
 *      0 on success. Nonzero on failure.
 *      Always returns 0.
 *****************************************************************************/
int FreeListFree (FreeList *free_list,void * element);

/****************************************************************************
 * Function: FreeListDestroy
 *
 *  Description:
 *      Releases the resources stored with the free list.
 *  Parameters:
 *      free_list  - must be valid, non null, pointer to a linked list.
 *  Returns:
 *      0 on success. Nonzero on failure.
 *      Always returns 0.
 *****************************************************************************/
int FreeListDestroy (FreeList *free_list);


#ifdef __cplusplus
}
#endif

#endif // FREE_LIST_H
