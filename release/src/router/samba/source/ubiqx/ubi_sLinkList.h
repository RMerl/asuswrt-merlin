#ifndef UBI_SLINKLIST_H
#define UBI_SLINKLIST_H
/* ========================================================================== **
 *                              ubi_sLinkList.h
 *
 *  Copyright (C) 1997, 1998 by Christopher R. Hertel
 *
 *  Email: crh@ubiqx.mn.org
 * -------------------------------------------------------------------------- **
 *  This module implements a simple singly-linked list.
 * -------------------------------------------------------------------------- **
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * -------------------------------------------------------------------------- **
 *
 * Log: ubi_sLinkList.h,v
 * Revision 0.9  1998/07/24 07:30:20  crh
 * Added the ubi_slNewList() macro.
 *
 * Revision 0.8  1998/06/04 21:29:27  crh
 * Upper-cased defined constants (eg UBI_BINTREE_H) in some header files.
 * This is more "standard", and is what people expect.  Weird, eh?
 *
 * Revision 0.7  1998/06/03 18:06:03  crh
 * Further fiddling with sys_include.h, which has been moved from the .c file
 * to the .h file.
 *
 * Revision 0.6  1998/06/02 01:38:47  crh
 * Changed include file name from ubi_null.h to sys_include.h to make it
 * more generic.
 *
 * Revision 0.5  1998/05/20 04:38:05  crh
 * The C file now includes ubi_null.h.  See ubi_null.h for more info.
 *
 * Revision 0.4  1998/03/10 02:22:39  crh
 * Combined ubi_StackQueue and ubi_sLinkList into one module.  Redesigned
 * the functions and macros.  Not a complete rewrite but close to it.
 *
 * Revision 0.3  1998/01/03 02:00:02  crh
 * Added ubi_slCount() macro.
 *
 * Revision 0.2  1997/10/21 03:36:14  crh
 * Added parameter <After> in function Insert().  Made necessary changes
 * to macro AddHead() and added macro AddHere().
 *
 * Revision 0.1  1997/10/16 02:54:08  crh
 * Initial Revision.
 *
 * -------------------------------------------------------------------------- **
 *  This module implements a singly-linked list which may also be used as a
 *  queue or a stack.  For a queue, entries are added at the tail and removed
 *  from the head of the list.  For a stack, the entries are entered and
 *  removed from the head of the list.  A traversal of the list will always
 *  start at the head of the list and proceed toward the tail.  This is all
 *  mind-numbingly simple, but I'm surprised by the number of programs out
 *  there which re-implement this a dozen or so times.
 *
 *  Note:  When the list header is initialized, the Tail pointer is set to
 *         point to the Head pointer.  This simplifies things a great deal,
 *         except that you can't initialize a stack or queue by simply
 *         zeroing it out.  One sure way to initialize the header is to call
 *         ubi_slInit().  Another option would be something like this:
 *
 *           ubi_slNewList( MyList );
 *
 *         Which translates to:
 *
 *           ubi_slList MyList[1] = { NULL, (ubi_slNodePtr)MyList, 0 };
 *
 *         See ubi_slInit(), ubi_slNewList(), and the ubi_slList structure
 *         for more info.
 *
 *        + Also, note that this module is similar to the ubi_dLinkList
 *          module.  There are three key differences:
 *          - This is a singly-linked list, the other is a doubly-linked
 *            list.
 *          - In this module, if the list is empty, the tail pointer will
 *            point back to the head of the list as described above.  This
 *            is not done in ubi_dLinkList.
 *          - The ubi_slRemove() function, by necessity, removed the 'next'
 *            node.  In ubi_dLinkList, the ubi_dlRemove() function removes
 *            the 'current' node.
 *
 * ========================================================================== **
 */

#include "sys_include.h"    /* System-specific includes. */

/* ========================================================================== **
 * Typedefs...
 *
 *  ubi_slNode    - This is the basic node structure.
 *  ubi_slNodePtr - Pointer to a node.
 *  ubi_slList    - This is the list header structure.
 *  ubi_slListPtr - Pointer to a List (i.e., a list header structure).
 *
 */

typedef struct ubi_slListNode
  {
  struct ubi_slListNode *Next;
  } ubi_slNode;

typedef ubi_slNode *ubi_slNodePtr;

typedef struct
  {
  ubi_slNodePtr Head;
  ubi_slNodePtr Tail;
  unsigned long count;
  } ubi_slList;

typedef ubi_slList *ubi_slListPtr;


/* ========================================================================== **
 * Macros...
 * 
 *  ubi_slNewList - Macro used to declare and initialize a list header in
 *                  one step.
 *
 *  ubi_slCount   - Returns the current number of entries in the list.
 *
 *  ubi_slAddHead - Add a new node at the head of the list.
 *  ubi_slAddNext - Add a new node following the indicated node.
 *  ubi_slAddTail - Add a new node to the tail of the list.
 *                  Note: AddTail evaluates the L parameter twice.
 *
 *  ubi_slRemHead - Remove the node at the head of the list, if any.
 *  ubi_slRemNext - Remove the node following the given node.
 *
 *  ubi_slFirst   - Return a pointer to the first node in the list, if any.
 *  ubi_slNext    - Given a node, return a pointer to the next node.
 *  ubi_slLast    - Return a pointer to the last node in the list, if any.
 *
 *  ubi_slPush    - Add a node at the head of the list (synonym of AddHead).
 *  ubi_slPop     - Remove a node at the head of the list (synonym of RemHead).
 *  ubi_slEnqueue - Add a node at the tail of the list (sysnonym of AddTail).
 *  ubi_slDequeue - Remove a node at the head of the list (synonym of RemHead).
 *
 *  Note that all of these provide type casting of the parameters.  The
 *  Add and Rem macros are nothing more than nice front-ends to the
 *  Insert and Remove functions.
 *
 *  Also note that the First, Next and Last macros do no parameter checking!
 *
 */

#define ubi_slNewList( L ) ubi_slList (L)[1] = {{ NULL, (ubi_slNodePtr)(L), 0 }}

#define ubi_slCount( L ) (((ubi_slListPtr)(L))->count)

#define ubi_slAddHead( L, N ) \
        ubi_slInsert( (ubi_slListPtr)(L), (ubi_slNodePtr)(N), NULL )

#define ubi_slAddNext( L, N, A ) \
        ubi_slInsert( (ubi_slListPtr)(L), \
                      (ubi_slNodePtr)(N), \
                      (ubi_slNodePtr)(A) )

#define ubi_slAddTail( L, N ) \
        ubi_slInsert( (ubi_slListPtr)(L), \
                      (ubi_slNodePtr)(N), \
                     ((ubi_slListPtr)(L))->Tail )

#define ubi_slRemHead( L ) ubi_slRemove( (ubi_slListPtr)(L), NULL )

#define ubi_slRemNext( L, N ) \
        ubi_slRemove( (ubi_slListPtr)(L), (ubi_slNodePtr)(N) )

#define ubi_slFirst( L ) (((ubi_slListPtr)(L))->Head)

#define ubi_slNext( N )  (((ubi_slNodePtr)(N))->Next)

#define ubi_slLast( L )  (((ubi_slListPtr)(L))->Tail)

#define ubi_slPush    ubi_slAddHead
#define ubi_slPop     ubi_slRemHead
#define ubi_slEnqueue ubi_slAddTail
#define ubi_slDequeue ubi_slRemHead

/* ========================================================================== **
 * Function prototypes...
 */

ubi_slListPtr ubi_slInitList( ubi_slListPtr ListPtr );
  /* ------------------------------------------------------------------------ **
   * Initialize a singly-linked list header.
   *
   *  Input:  ListPtr - A pointer to the list structure that is to be
   *                    initialized for use.
   *
   *  Output: A pointer to the initialized list header (i.e., same as
   *          <ListPtr>).
   *
   * ------------------------------------------------------------------------ **
   */

ubi_slNodePtr ubi_slInsert( ubi_slListPtr ListPtr,
                            ubi_slNodePtr New,
                            ubi_slNodePtr After );
  /* ------------------------------------------------------------------------ **
   * Add a node to the list.
   *
   *  Input:  ListPtr - A pointer to the list into which the node is to
   *                    be inserted.
   *          New     - Pointer to the node that is to be added to the list.
   *          After   - Pointer to a list in a node after which the new node
   *                    will be inserted.  If NULL, then the new node will
   *                    be added at the head of the list.
   *
   *  Output: A pointer to the node that was inserted into the list (i.e.,
   *          the same as <New>).
   *
   * ------------------------------------------------------------------------ **
   */

ubi_slNodePtr ubi_slRemove( ubi_slListPtr ListPtr, ubi_slNodePtr After );
  /* ------------------------------------------------------------------------ **
   * Remove the node followng <After>.  If <After> is NULL, remove from the
   * head of the list.
   *
   *  Input:  ListPtr - A pointer to the list from which the node is to be
   *                    removed.
   *          After   - Pointer to the node preceeding the node to be
   *                    removed.
   *
   *  Output: A pointer to the node that was removed, or NULL if the list is
   *          empty.
   *
   * ------------------------------------------------------------------------ **
   */

/* ================================ The End ================================= */
#endif /* UBI_SLINKLIST_H */
