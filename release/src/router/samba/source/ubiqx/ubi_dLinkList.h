#ifndef UBI_DLINKLIST_H
#define UBI_DLINKLIST_H
/* ========================================================================== **
 *                              ubi_dLinkList.h
 *
 *  Copyright (C) 1997, 1998 by Christopher R. Hertel
 *
 *  Email: crh@ubiqx.mn.org
 * -------------------------------------------------------------------------- **
 *  This module implements simple doubly-linked lists.
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
 * Log: ubi_dLinkList.h,v
 * Revision 0.10  1998/07/24 07:30:20  crh
 * Added the ubi_dlNewList() macro.
 *
 * Revision 0.9  1998/06/04 21:29:27  crh
 * Upper-cased defined constants (eg UBI_BINTREE_H) in some header files.
 * This is more "standard", and is what people expect.  Weird, eh?
 *
 * Revision 0.8  1998/06/03 18:06:03  crh
 * Further fiddling with sys_include.h, which has been moved from the .c file
 * to the .h file.
 *
 * Revision 0.7  1998/06/02 01:38:47  crh
 * Changed include file name from ubi_null.h to sys_include.h to make it
 * more generic.
 *
 * Revision 0.6  1998/05/20 04:38:05  crh
 * The C file now includes ubi_null.h.  See ubi_null.h for more info.
 *
 * Revision 0.5  1998/03/10 02:54:04  crh
 * Simplified the code and added macros for stack & queue manipulations.
 *
 * Revision 0.4  1998/01/03 01:53:44  crh
 * Added ubi_dlCount() macro.
 *
 * Revision 0.3  1997/10/15 03:04:31  crh
 * Added some handy type casting to the macros.  Added AddHere and RemThis
 * macros.
 *
 * Revision 0.2  1997/10/08 03:08:16  crh
 * Fixed a few forgotten link-ups in Insert(), and fixed the AddHead()
 * macro, which was passing the wrong value for <After> to Insert().
 *
 * Revision 0.1  1997/10/07 04:34:38  crh
 * Initial Revision.
 *
 * -------------------------------------------------------------------------- **
 * This module is similar to the ubi_sLinkList module, but it is neither a
 * descendant type nor an easy drop-in replacement for the latter.  One key
 * difference is that the ubi_dlRemove() function removes the indicated node,
 * while the ubi_slRemove() function (in ubi_sLinkList) removes the node
 * *following* the indicated node.
 *
 * ========================================================================== **
 */

#include "sys_include.h"    /* System-specific includes. */

/* ========================================================================== **
 * Typedefs...
 *
 *  ubi_dlNode    - This is the basic node structure.
 *  ubi_dlNodePtr - Pointer to a node.
 *  ubi_dlList    - This is the list header structure.
 *  ubi_dlListPtr - Pointer to a List (i.e., a list header structure).
 *
 */

typedef struct ubi_dlListNode
  {
  struct ubi_dlListNode *Next;
  struct ubi_dlListNode *Prev;
  } ubi_dlNode;

typedef ubi_dlNode *ubi_dlNodePtr;

typedef struct
  {
  ubi_dlNodePtr Head;
  ubi_dlNodePtr Tail;
  unsigned long count;
  } ubi_dlList;

typedef ubi_dlList *ubi_dlListPtr;

/* ========================================================================== **
 * Macros...
 *
 *  ubi_dlNewList - Macro used to declare and initialize a new list in one
 *                  swell foop.  It is used when defining a variable of
 *                  type ubi_dlList.  The definition
 *                    static ubi_dlNewList( gerbil );
 *                  is translated to
 *                    static ubi_dlList gerbil[1] = {{ NULL, NULL, 0 }};
 *
 *  ubi_dlCount   - Return the number of entries currently in the list.
 *
 *  ubi_dlAddHead - Add a new node at the head of the list.
 *  ubi_dlAddNext - Add a node following the given node.
 *  ubi_dlAddTail - Add a new node at the tail of the list.
 *                  Note: AddTail evaluates the L parameter twice.
 *
 *  ubi_dlRemHead - Remove the node at the head of the list, if any.
 *                  Note: RemHead evaluates the L parameter twice.
 *  ubi_dlRemThis - Remove the indicated node.
 *  ubi_dlRemTail - Remove the node at the tail of the list, if any.
 *                  Note: RemTail evaluates the L parameter twice.
 *
 *  ubi_dlFirst   - Return a pointer to the first node in the list, if any.
 *  ubi_dlLast    - Return a pointer to the last node in the list, if any.
 *  ubi_dlNext    - Given a node, return a pointer to the next node.
 *  ubi_dlPrev    - Given a node, return a pointer to the previous node.
 *
 *  ubi_dlPush    - Add a node at the head of the list (synonym of AddHead).
 *  ubi_dlPop     - Remove a node at the head of the list (synonym of RemHead).
 *  ubi_dlEnqueue - Add a node at the tail of the list (sysnonym of AddTail).
 *  ubi_dlDequeue - Remove a node at the head of the list (synonym of RemHead).
 *
 *  Note that all of these provide type casting of the parameters.  The
 *  Add and Rem macros are nothing more than nice front-ends to the
 *  Insert and Remove operations.
 *
 *  Also note that the First, Next and Last macros do no parameter checking!
 *
 */

#define ubi_dlNewList( L ) ubi_dlList (L)[1] = {{ NULL, NULL, 0 }}

#define ubi_dlCount( L ) (((ubi_dlListPtr)(L))->count)

#define ubi_dlAddHead( L, N ) \
        ubi_dlInsert( (ubi_dlListPtr)(L), (ubi_dlNodePtr)(N), NULL )

#define ubi_dlAddNext( L, N, A ) \
        ubi_dlInsert( (ubi_dlListPtr)(L), \
                      (ubi_dlNodePtr)(N), \
                      (ubi_dlNodePtr)(A) )

#define ubi_dlAddTail( L, N ) \
        ubi_dlInsert( (ubi_dlListPtr)(L), \
                      (ubi_dlNodePtr)(N), \
                    (((ubi_dlListPtr)(L))->Tail) )

#define ubi_dlRemHead( L ) ubi_dlRemove( (ubi_dlListPtr)(L), \
                                         (((ubi_dlListPtr)(L))->Head) )

#define ubi_dlRemThis( L, N ) ubi_dlRemove( (ubi_dlListPtr)(L), \
                                            (ubi_dlNodePtr)(N) )

#define ubi_dlRemTail( L ) ubi_dlRemove( (ubi_dlListPtr)(L), \
                                         (((ubi_dlListPtr)(L))->Tail) )

#define ubi_dlFirst( L ) (((ubi_dlListPtr)(L))->Head)

#define ubi_dlLast( L )  (((ubi_dlListPtr)(L))->Tail)

#define ubi_dlNext( N )  (((ubi_dlNodePtr)(N))->Next)

#define ubi_dlPrev( N )  (((ubi_dlNodePtr)(N))->Prev)

#define ubi_dlPush    ubi_dlAddHead
#define ubi_dlPop     ubi_dlRemHead
#define ubi_dlEnqueue ubi_dlAddTail
#define ubi_dlDequeue ubi_dlRemHead

/* ========================================================================== **
 * Function prototypes...
 */

ubi_dlListPtr ubi_dlInitList( ubi_dlListPtr ListPtr );
  /* ------------------------------------------------------------------------ **
   * Initialize a doubly-linked list header.
   *
   *  Input:  ListPtr - A pointer to the list structure that is to be
   *                    initialized for use.
   *
   *  Output: A pointer to the initialized list header (i.e., same as
   *          <ListPtr>).
   *
   * ------------------------------------------------------------------------ **
   */

ubi_dlNodePtr ubi_dlInsert( ubi_dlListPtr ListPtr,
                            ubi_dlNodePtr New,
                            ubi_dlNodePtr After );
  /* ------------------------------------------------------------------------ **
   * Insert a new node into the list.
   *
   *  Input:  ListPtr - A pointer to the list into which the node is to
   *                    be inserted.
   *          New     - Pointer to the new node.
   *          After   - NULL, or a pointer to a node that is already in the
   *                    list.
   *                    If NULL, then <New> will be added at the head of the
   *                    list, else it will be added following <After>.
   * 
   *  Output: A pointer to the node that was inserted into the list (i.e.,
   *          the same as <New>).
   *
   * ------------------------------------------------------------------------ **
   */

ubi_dlNodePtr ubi_dlRemove( ubi_dlListPtr ListPtr, ubi_dlNodePtr Old );
  /* ------------------------------------------------------------------------ **
   * Remove a node from the list.
   *
   *  Input:  ListPtr - A pointer to the list from which <Old> is to be
   *                    removed.
   *          Old     - A pointer to the node that is to be removed from the
   *                    list.
   *
   *  Output: A pointer to the node that was removed (i.e., <Old>).
   *
   * ------------------------------------------------------------------------ **
   */

/* ================================ The End ================================= */
#endif /* UBI_DLINKLIST_H */
