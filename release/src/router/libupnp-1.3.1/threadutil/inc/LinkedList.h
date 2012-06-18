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

#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include "FreeList.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EOUTOFMEM (-7 & 1<<29)

#define FREELISTSIZE 100
#define LIST_SUCCESS 1
#define LIST_FAIL 0

/****************************************************************************
 * Name: free_routine
 *
 *  Description:
 *     Function for freeing list items
 *****************************************************************************/
typedef void (*free_function)(void *arg);

/****************************************************************************
 * Name: cmp_routine
 *
 *  Description:
 *     Function for comparing list items
 *     Returns 1 if itemA==itemB
 *****************************************************************************/
typedef int (*cmp_routine)(void *itemA,void *itemB);

/****************************************************************************
 * Name: ListNode
 *
 *  Description:
 *      linked list node. stores generic item and pointers to next and prev.
 *      Internal Use Only.
 *****************************************************************************/
typedef struct LISTNODE
{
  struct LISTNODE *prev; //previous node
  struct LISTNODE *next; //next node
  void *item; //item
} ListNode;

/****************************************************************************
 * Name: LinkedList
 *
 *  Description:
 *      linked list (no protection). Internal Use Only.
 *      Because this is for internal use, parameters are NOT checked for 
 *      validity.
 *      The first item of the list is stored at node: head->next
 *      The last item of the list is stored at node: tail->prev
 *      If head->next=tail, then list is empty.
 *      To iterate through the list:
 *
 *       LinkedList g;
 *       ListNode *temp = NULL;
 *       for (temp = ListHead(g);temp!=NULL;temp = ListNext(g,temp))
 *       {
 *        }
 *
 *****************************************************************************/
typedef struct LINKEDLIST
{
  ListNode head; //head, first item is stored at: head->next
  ListNode tail; //tail, last item is stored at: tail->prev
  long size;      //size of list
  FreeList freeNodeList; //free list to use
  free_function free_func; //free function to use
  cmp_routine cmp_func; //compare function to use
} LinkedList;

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
int ListInit(LinkedList *list,cmp_routine cmp_func, free_function free_func);

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
ListNode *ListAddHead(LinkedList *list, void *item);

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
ListNode *ListAddTail(LinkedList *list, void *item);

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
ListNode *ListAddAfter(LinkedList *list, void *item, ListNode *bnode);


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
ListNode *ListAddBefore(LinkedList *list,void *item, ListNode *anode);


/****************************************************************************
 * Function: ListDelNode
 *
 *  Description:
 *      Removes a node from the list
 *      The memory for the node is freed.
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *      ListNode *dnode - done to delete.
 *      freeItem - if !0 then item is freed using free function.
 *                 if 0 (or free function is NULL) then item is not freed
 *  Returns:
 *      The pointer to the item stored in the node or NULL if the item is freed.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
void *ListDelNode(LinkedList *list,ListNode *dnode, int freeItem);

/****************************************************************************
 * Function: ListDestroy
 *
 *  Description:
 *      Removes all memory associated with list nodes. 
 *      Does not free LinkedList *list. 
 *    
 *  Parameters:
 *      LinkedList *list  - must be valid, non null, pointer to a linked list.
 *      freeItem - if !0 then items are freed using the free_function.
 *                 if 0 (or free function is NULL) then items are not freed.
 *  Returns:
 *      0 on success. Always returns 0.
 *  Precondition:
 *      The list has been initialized.
 *****************************************************************************/
int ListDestroy(LinkedList *list, int freeItem);


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
ListNode* ListHead(LinkedList *list);

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
ListNode* ListTail(LinkedList *list);

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
ListNode* ListNext(LinkedList *list, ListNode * node);

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
ListNode* ListPrev(LinkedList *list, ListNode * node);

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
ListNode* ListFind(LinkedList *list, ListNode *start, void * item);

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
int ListSize(LinkedList* list);


#ifdef __cplusplus
}
#endif

#endif //LINKED_LIST_H
