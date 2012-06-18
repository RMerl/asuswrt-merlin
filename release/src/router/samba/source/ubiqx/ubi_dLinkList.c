/* ========================================================================== **
 *                              ubi_dLinkList.c
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
 * Log: ubi_dLinkList.c,v
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
 * Revision 0.5  1998/03/10 02:55:00  crh
 * Simplified the code and added macros for stack & queue manipulations.
 *
 * Revision 0.4  1998/01/03 01:53:56  crh
 * Added ubi_dlCount() macro.
 *
 * Revision 0.3  1997/10/15 03:05:39  crh
 * Added some handy type casting to the macros.  Added AddHere and RemThis
 * macros.
 *
 * Revision 0.2  1997/10/08 03:07:21  crh
 * Fixed a few forgotten link-ups in Insert(), and fixed the AddHead()
 * macro, which was passing the wrong value for <After> to Insert().
 *
 * Revision 0.1  1997/10/07 04:34:07  crh
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

#include "ubi_dLinkList.h"  /* Header for *this* module. */

/* ========================================================================== **
 * Functions...
 */

ubi_dlListPtr ubi_dlInitList( ubi_dlListPtr ListPtr )
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
  {
  ListPtr->Head  = NULL;
  ListPtr->Tail  = NULL;
  ListPtr->count = 0;
  return( ListPtr );
  } /* ubi_dlInitList */

ubi_dlNodePtr ubi_dlInsert( ubi_dlListPtr ListPtr,
                            ubi_dlNodePtr New,
                            ubi_dlNodePtr After )
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
  {
  ubi_dlNodePtr PredNode = After ? After : (ubi_dlNodePtr)ListPtr;

  New->Next         = PredNode->Next;
  New->Prev         = After;
  PredNode->Next    = New;
  if( New->Next )
    New->Next->Prev = New;
  else
    ListPtr->Tail   = New;

  (ListPtr->count)++;

  return( New );
  } /* ubi_dlInsert */

ubi_dlNodePtr ubi_dlRemove( ubi_dlListPtr ListPtr, ubi_dlNodePtr Old )
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
  {
  if( Old )
    {
    if( Old->Next )
      Old->Next->Prev = Old->Prev;
    else
      ListPtr->Tail = Old->Prev;

    if( Old->Prev )
      Old->Prev->Next = Old->Next;
    else
      ListPtr->Head = Old->Next;

    (ListPtr->count)--;
    }

  return( Old );
  } /* ubi_dlRemove */

/* ================================ The End ================================= */
