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

#include "LinkedList.h"
#include <malloc.h>
#include <assert.h>

static int
freeListNode( ListNode * node,
              LinkedList * list )
{
    assert( list != NULL );

    return FreeListFree( &list->freeNodeList, node );
}

/****************************************************************************
 * Function: CreateListNode
 *
 *  Description:
 *      Creates a list node. Dynamically.
 *      
 *  Parameters:
 *      void * item - the item to store
 *  Returns:
 *      The new node, NULL on failure.
 *****************************************************************************/
static ListNode *
CreateListNode( void *item,
                LinkedList * list )
{

    ListNode *temp = NULL;

    assert( list != NULL );

    temp = ( ListNode * ) FreeListAlloc( &list->freeNodeList );
    if( temp ) {
        temp->prev = NULL;
        temp->next = NULL;
        temp->item = item;
    }
    return temp;
}

/****************************************************************************
 * Function: ListInit
 *
 *  Description:
 *      Initializes LinkedList. Must be called first.
 *      And only once for List.
 *  Parameters:
 *      list  - must be valid, non null, pointer to a linked list.
 *      cmp_func - function used to compare items. (May be NULL)
 *      free_func - function used to free items. (May be NULL)
 *  Returns:
 *      0 on success, EOUTOFMEM on failure.
 *****************************************************************************/
int
ListInit( LinkedList * list,
          cmp_routine cmp_func,
          free_function free_func )
{

    int retCode = 0;

    assert( list != NULL );

    if( list == NULL )
        return EINVAL;

    list->size = 0;
    list->cmp_func = cmp_func;
    list->free_func = free_func;

    retCode =
        FreeListInit( &list->freeNodeList, sizeof( ListNode ),
                      FREELISTSIZE );

    assert( retCode == 0 );

    list->head.item = NULL;
    list->head.next = &list->tail;
    list->head.prev = NULL;

    list->tail.item = NULL;
    list->tail.prev = &list->head;
    list->tail.next = NULL;

    return 0;
}

/****************************************************************************
 * Function: ListAddHead
 *
 *  Description:
 *      Adds a node to the head of the list.
 *      Node gets immediately after list.head.
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *      void * item - item to be added
 *  Returns:
 *      The pointer to the ListNode on success, NULL on failure.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
ListNode *
ListAddHead( LinkedList * list,
             void *item )
{
    assert( list != NULL );

    if( list == NULL )
        return NULL;

    return ListAddAfter( list, item, &list->head );
}

/****************************************************************************
 * Function: ListAddTail
 *
 *  Description:
 *      Adds a node to the tail of the list.
 *      Node gets added immediately before list.tail.
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *      void * item - item to be added
 *  Returns:
 *      The pointer to the ListNode on success, NULL on failure.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
ListNode *
ListAddTail( LinkedList * list,
             void *item )
{
    assert( list != NULL );

    if( list == NULL )
        return NULL;

    return ListAddBefore( list, item, &list->tail );
}

/****************************************************************************
 * Function: ListAddAfter
 *
 *  Description:
 *      Adds a node after the specified node.
 *      Node gets added immediately after bnode.
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *      void * item - item to be added
 *      ListNode * bnode - node to add after
 *  Returns:
 *      The pointer to the ListNode on success, NULL on failure.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
ListNode *
ListAddAfter( LinkedList * list,
              void *item,
              ListNode * bnode )
{
    ListNode *newNode = NULL;

    assert( list != NULL );

    if( ( list == NULL ) || ( bnode == NULL ) )
        return NULL;

    newNode = CreateListNode( item, list );
    if( newNode ) {
        ListNode *temp = bnode->next;

        bnode->next = newNode;
        newNode->prev = bnode;
        newNode->next = temp;
        temp->prev = newNode;
        list->size++;
        return newNode;
    }
    return NULL;
}

/****************************************************************************
 * Function: ListAddBefore
 *
 *  Description:
 *      Adds a node before the specified node.
 *      Node gets added immediately before anode.
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *      ListNode * anode  - node to add the in front of.
 *      void * item - item to be added
 *  Returns:
 *      The pointer to the ListNode on success, NULL on failure.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
ListNode *
ListAddBefore( LinkedList * list,
               void *item,
               ListNode * anode )
{
    ListNode *newNode = NULL;

    assert( list != NULL );

    if( ( list == NULL ) || ( anode == NULL ) )
        return NULL;

    newNode = CreateListNode( item, list );

    if( newNode ) {
        ListNode *temp = anode->prev;

        anode->prev = newNode;
        newNode->next = anode;
        newNode->prev = temp;
        temp->next = newNode;
        list->size++;
        return newNode;
    }
    return NULL;
}

/****************************************************************************
 * Function: ListDelNode
 *
 *  Description:
 *      Removes a node from the list
 *      The memory for the node is freed but the
 *      the memory for the items are not.
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *      ListNode *dnode - done to delete.
 *  Returns:
 *      The pointer to the item stored in node on success, NULL on failure.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
void *
ListDelNode( LinkedList * list,
             ListNode * dnode,
             int freeItem )
{
    void *temp;

    assert( list != NULL );
    assert( dnode != &list->head );
    assert( dnode != &list->tail );

    if( ( list == NULL ) ||
        ( dnode == &list->head ) ||
        ( dnode == &list->tail ) || ( dnode == NULL ) ) {
        return NULL;
    }

    temp = dnode->item;
    dnode->prev->next = dnode->next;
    dnode->next->prev = dnode->prev;

    freeListNode( dnode, list );
    list->size--;

    if( freeItem && list->free_func ) {
        list->free_func( temp );
        temp = NULL;
    }

    return temp;
}

/****************************************************************************
 * Function: ListDestroy
 *
 *  Description:
 *      Removes all memory associated with list nodes. 
 *      Does not free LinkedList *list. 
 *      Items stored in the list are not freed, only nodes are.
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *  Returns:
 *      0 on success. Nonzero on failure.
 *      Always returns 0.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
int
ListDestroy( LinkedList * list,
             int freeItem )
{
    ListNode *dnode = NULL;
    ListNode *temp = NULL;

    if( list == NULL )
        return EINVAL;

    for( dnode = list->head.next; dnode != &list->tail; ) {
        temp = dnode->next;
        ListDelNode( list, dnode, freeItem );
        dnode = temp;
    }

    list->size = 0;
    FreeListDestroy( &list->freeNodeList );
    return 0;
}

/****************************************************************************
 * Function: ListHead
 *
 *  Description:
 *      Returns the head of the list.
 *    
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *  
 *  Returns:
 *      The head of the list. NULL if list is empty.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
ListNode *
ListHead( LinkedList * list )
{
    assert( list != NULL );

    if( list == NULL )
        return NULL;

    if( list->size == 0 )
        return NULL;
    else
        return list->head.next;
}

/****************************************************************************
 * Function: ListTail
 *
 *  Description:
 *      Returns the tail of the list.
 *    
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *  
 *  Returns:
 *      The tail of the list. NULL if list is empty.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
ListNode *
ListTail( LinkedList * list )
{
    assert( list != NULL );

    if( list == NULL )
        return NULL;

    if( list->size == 0 )
        return NULL;
    else
        return list->tail.prev;
}

/****************************************************************************
 * Function: ListNext
 *
 *  Description:
 *      Returns the next item in the list.
 *    
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *  
 *  Returns:
 *      The next item in the list. NULL if there are no more items in list.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
ListNode *
ListNext( LinkedList * list,
          ListNode * node )
{
    assert( list != NULL );
    assert( node != NULL );

    if( ( list == NULL ) || ( node == NULL ) )
        return NULL;

    if( node->next == &list->tail )
        return NULL;
    else
        return node->next;
}

/****************************************************************************
 * Function: ListPrev
 *
 *  Description:
 *      Returns the previous item in the list.
 *    
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *  
 *  Returns:
 *      The previous item in the list. NULL if there are no more items in list.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
ListNode *
ListPrev( LinkedList * list,
          ListNode * node )
{
    assert( list != NULL );
    assert( node != NULL );

    if( ( list == NULL ) || ( node == NULL ) )
        return NULL;

    if( node->prev == &list->head )
        return NULL;
    else
        return node->prev;
}

/****************************************************************************
 * Function: ListFind
 *
 *  Description:
 *      Finds the specified item in the list.
 *      Uses the compare function specified in ListInit. If compare function
 *      is NULL then compares items as pointers.
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *      ListNode *start - the node to start from, NULL if to start from 
 *                        beginning.
 *      void * item - the item to search for.
 *  Returns:
 *      The node containing the item. NULL if no node contains the item.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
ListNode *
ListFind( LinkedList * list,
          ListNode * start,
          void *item )
{

    ListNode *finger = NULL;

    if( list == NULL )
        return NULL;

    if( start == NULL )
        start = &list->head;

    assert( start );

    finger = start->next;

    assert( finger );

    while( finger != &list->tail ) {
        if( list->cmp_func ) {
            if( list->cmp_func( item, finger->item ) )
                return finger;
        } else {
            if( item == finger->item )
                return finger;
        }
        finger = finger->next;
    }

    return NULL;

}

/****************************************************************************
 * Function: ListSize
 *
 *  Description:
 *     Returns the size of the list.
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 
 *  Returns:
 *      The number of items in the list.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
int
ListSize( LinkedList * list )
{
    assert( list != NULL );

    if( list == NULL )
        return EINVAL;

    return list->size;
}
