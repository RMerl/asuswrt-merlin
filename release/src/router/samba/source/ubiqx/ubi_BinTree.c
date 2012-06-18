/* ========================================================================== **
 *                              ubi_BinTree.c
 *
 *  Copyright (C) 1991-1998 by Christopher R. Hertel
 *
 *  Email:  crh@ubiqx.mn.org
 * -------------------------------------------------------------------------- **
 *
 *  This module implements a simple binary tree.
 *
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
 * Log: ubi_BinTree.c,v
 * Revision 4.7  1998/10/21 06:14:42  crh
 * Fixed bugs in FirstOf() and LastOf() reported by Massimo Campostrini.
 * See function comments.
 *
 * Revision 4.6  1998/07/25 17:02:10  crh
 * Added the ubi_trNewTree() macro.
 *
 * Revision 4.5  1998/06/04 21:29:27  crh
 * Upper-cased defined constants (eg UBI_BINTREE_H) in some header files.
 * This is more "standard", and is what people expect.  Weird, eh?
 *
 * Revision 4.4  1998/06/03 17:42:46  crh
 * Further fiddling with sys_include.h.  It's now in ubi_BinTree.h which is
 * included by all of the binary tree files.
 *
 * Reminder: Some of the ubi_tr* macros in ubi_BinTree.h are redefined in
 *           ubi_AVLtree.h and ubi_SplayTree.h.  This allows easy swapping
 *           of tree types by simply changing a header.  Unfortunately, the
 *           macro redefinitions in ubi_AVLtree.h and ubi_SplayTree.h will
 *           conflict if used together.  You must either choose a single tree
 *           type, or use the underlying function calls directly.  Compare
 *           the two header files for more information.
 *
 * Revision 4.3  1998/06/02 01:28:43  crh
 * Changed ubi_null.h to sys_include.h to make it more generic.
 *
 * Revision 4.2  1998/05/20 04:32:36  crh
 * The C file now includes ubi_null.h.  See ubi_null.h for more info.
 * Also, the balance and gender fields of the node were declared as
 * signed char.  As I understand it, at least one SunOS or Solaris
 * compiler doesn't like "signed char".  The declarations were
 * wrong anyway, so I changed them to simple "char".
 *
 * Revision 4.1  1998/03/31 06:11:57  crh
 * Thomas Aglassinger sent E'mail pointing out errors in the
 * dereferencing of function pointers, and a missing typecast.
 * Thanks, Thomas!
 *
 * Revision 4.0  1998/03/10 03:19:22  crh
 * Added the AVL field 'balance' to the ubi_btNode structure.  This means
 * that all BinTree modules now use the same basic node structure, which
 * greatly simplifies the AVL module.
 * Decided that this was a big enough change to justify a new major revision
 * number.  3.0 was an error, so we're at 4.0.
 *
 * Revision 2.6  1998/01/24 06:27:46  crh
 * Added ubi_trCount() macro.
 *
 * Revision 2.5  1997/12/23 03:56:29  crh
 * In this version, all constants & macros defined in the header file have
 * the ubi_tr prefix.  Also cleaned up anything that gcc complained about
 * when run with '-pedantic -fsyntax-only -Wall'.
 *
 * Revision 2.4  1997/07/26 04:11:10  crh
 * + Just to be annoying I changed ubi_TRUE and ubi_FALSE to ubi_trTRUE
 *   and ubi_trFALSE.
 * + There is now a type ubi_trBool to go with ubi_trTRUE and ubi_trFALSE.
 * + There used to be something called "ubi_TypeDefs.h".  I got rid of it.
 * + Added function ubi_btLeafNode().
 *
 * Revision 2.3  1997/06/03 05:16:17  crh
 * Changed TRUE and FALSE to ubi_TRUE and ubi_FALSE to avoid conflicts.
 * Also changed the interface to function InitTree().  See the comments
 * for this function for more information.
 *
 * Revision 2.2  1995/10/03 22:00:07  CRH
 * Ubisized!
 * 
 * Revision 2.1  95/03/09  23:37:10  CRH
 * Added the ModuleID static string and function.  These modules are now
 * self-identifying.
 * 
 * Revision 2.0  95/02/27  22:00:17  CRH
 * Revision 2.0 of this program includes the following changes:
 *
 *     1)  A fix to a major typo in the RepaceNode() function.
 *     2)  The addition of the static function Border().
 *     3)  The addition of the public functions FirstOf() and LastOf(), which
 *         use Border(). These functions are used with trees that allow
 *         duplicate keys.
 *     4)  A complete rewrite of the Locate() function.  Locate() now accepts
 *         a "comparison" operator.
 *     5)  Overall enhancements to both code and comments.
 *
 * I decided to give this a new major rev number because the interface has
 * changed.  In particular, there are two new functions, and changes to the
 * Locate() function.
 *
 * Revision 1.0  93/10/15  22:44:59  CRH
 * With this revision, I have added a set of #define's that provide a single,
 * standard API to all existing tree modules.  Until now, each of the three
 * existing modules had a different function and typedef prefix, as follows:
 *
 *       Module        Prefix
 *     ubi_BinTree     ubi_bt
 *     ubi_AVLtree     ubi_avl
 *     ubi_SplayTree   ubi_spt
 *
 * To further complicate matters, only those portions of the base module
 * (ubi_BinTree) that were superceeded in the new module had the new names.
 * For example, if you were using ubi_SplayTree, the locate function was
 * called "ubi_sptLocate", but the next and previous functions remained
 * "ubi_btNext" and "ubi_btPrev".
 *
 * This was not too terrible if you were familiar with the modules and knew
 * exactly which tree model you wanted to use.  If you wanted to be able to
 * change modules (for speed comparisons, etc), things could get messy very
 * quickly.
 *
 * So, I have added a set of defined names that get redefined in any of the
 * descendant modules.  To use this standardized interface in your code,
 * simply replace all occurances of "ubi_bt", "ubi_avl", and "ubi_spt" with
 * "ubi_tr".  The "ubi_tr" names will resolve to the correct function or
 * datatype names for the module that you are using.  Just remember to
 * include the header for that module in your program file.  Because these
 * names are handled by the preprocessor, there is no added run-time
 * overhead.
 *
 * Note that the original names do still exist, and can be used if you wish
 * to write code directly to a specific module.  This should probably only be
 * done if you are planning to implement a new descendant type, such as
 * red/black trees.  CRH
 *
 *  V0.0 - June, 1991   -  Written by Christopher R. Hertel (CRH).
 *
 * ========================================================================== **
 */

#include "ubi_BinTree.h"  /* Header for this module.   */

/* ========================================================================== **
 * Static data.
 */

static char ModuleID[] = "ubi_BinTree\n\
\tRevision: 4.7\n\
\tDate: 1998/10/21 06:14:42\n\
\tAuthor: crh\n";

/* ========================================================================== **
 * Internal (private) functions.
 */

static ubi_btNodePtr qFind( ubi_btCompFunc cmp,
                            ubi_btItemPtr  FindMe,
                   register ubi_btNodePtr  p )
  /* ------------------------------------------------------------------------ **
   * This function performs a non-recursive search of a tree for a node
   * matching a specific key.  It is called "qFind()" because it is
   * faster that TreeFind (below).
   *
   *  Input:
   *     cmp      -  a pointer to the tree's comparison function.
   *     FindMe   -  a pointer to the key value for which to search.
   *     p        -  a pointer to the starting point of the search.  <p>
   *                 is considered to be the root of a subtree, and only
   *                 the subtree will be searched.
   *
   *  Output:
   *     A pointer to a node with a key that matches the key indicated by
   *     FindMe, or NULL if no such node was found.
   *
   *  Note:   In a tree that allows duplicates, the pointer returned *might
   *          not* point to the (sequentially) first occurance of the
   *          desired key.
   * ------------------------------------------------------------------------ **
   */
  {
  int tmp;

  while( p && (( tmp = ubi_trAbNormal((*cmp)(FindMe, p)) ) != ubi_trEQUAL) )
    p = p->Link[tmp];

  return( p );
  } /* qFind */

static ubi_btNodePtr TreeFind( ubi_btItemPtr  findme,
                               ubi_btNodePtr  p,
                               ubi_btNodePtr *parentp,
                               char          *gender,
                               ubi_btCompFunc CmpFunc )
  /* ------------------------------------------------------------------------ **
   * TreeFind() searches a tree for a given value (findme).  It will return a
   * pointer to the target node, if found, or NULL if the target node was not
   * found.
   *
   * TreeFind() also returns, via parameters, a pointer to the parent of the
   * target node, and a LEFT or RIGHT value indicating which child of the
   * parent is the target node.  *If the target is not found*, then these
   * values indicate the place at which the target *should be found*.  This
   * is useful when inserting a new node into a tree or searching for nodes
   * "near" the target node.
   *
   * The parameters are:
   *
   *  findme   -  is a pointer to the key information to be searched for.
   *  p        -  points to the root of the tree to be searched.
   *  parentp  -  will return a pointer to a pointer to the !parent! of the
   *              target node, which can be especially usefull if the target
   *              was not found.
   *  gender   -  returns LEFT or RIGHT to indicate which child of *parentp
   *              was last searched.
   *  CmpFunc  -  points to the comparison function.
   *
   * This function is called by ubi_btLocate() and ubi_btInsert().
   * ------------------------------------------------------------------------ **
   */
  {
  register ubi_btNodePtr tmp_p      = p;
  ubi_btNodePtr          tmp_pp     = NULL;
  char                   tmp_gender = ubi_trEQUAL;
  int                    tmp_cmp;

  while( tmp_p
     && (ubi_trEQUAL != (tmp_cmp = ubi_trAbNormal((*CmpFunc)(findme, tmp_p)))) )
    {
    tmp_pp     = tmp_p;                 /* Keep track of previous node. */
    tmp_gender = (char)tmp_cmp;         /* Keep track of sex of child.  */
    tmp_p      = tmp_p->Link[tmp_cmp];  /* Go to child. */
    }
  *parentp = tmp_pp;                /* Return results. */
  *gender  = tmp_gender;
  return( tmp_p );
  } /* TreeFind */

static void ReplaceNode( ubi_btNodePtr *parent,
                         ubi_btNodePtr  oldnode,
                         ubi_btNodePtr  newnode )
  /* ------------------------------------------------------------------------ **
   * Remove node oldnode from the tree, replacing it with node newnode.
   *
   * Input:
   *  parent   - A pointer to he parent pointer of the node to be
   *             replaced.  <parent> may point to the Link[] field of
   *             a parent node, or it may indicate the root pointer at
   *             the top of the tree.
   *  oldnode  - A pointer to the node that is to be replaced.
   *  newnode  - A pointer to the node that is to be installed in the
   *             place of <*oldnode>.
   *
   * Notes:    Don't forget to free oldnode.
   *           Also, this function used to have a really nasty typo
   *           bug.  "oldnode" and "newnode" were swapped in the line
   *           that now reads:
   *     ((unsigned char *)newnode)[i] = ((unsigned char *)oldnode)[i];
   *           Bleah!
   * ------------------------------------------------------------------------ **
   */
  {
  register int i;
  register int btNodeSize = sizeof( ubi_btNode );

  for( i = 0; i < btNodeSize; i++ ) /* Copy node internals to new node. */
    ((unsigned char *)newnode)[i] = ((unsigned char *)oldnode)[i];
  (*parent) = newnode;              /* Old node's parent points to new child. */
  /* Now tell the children about their new step-parent. */
  if( oldnode->Link[ubi_trLEFT] )
    (oldnode->Link[ubi_trLEFT])->Link[ubi_trPARENT] = newnode;
  if( oldnode->Link[ubi_trRIGHT] )
    (oldnode->Link[ubi_trRIGHT])->Link[ubi_trPARENT] = newnode;
  } /* ReplaceNode */

static void SwapNodes( ubi_btRootPtr RootPtr,
                       ubi_btNodePtr Node1,
                       ubi_btNodePtr Node2 )
  /* ------------------------------------------------------------------------ **
   * This function swaps two nodes in the tree.  Node1 will take the place of
   * Node2, and Node2 will fill in the space left vacant by Node 1.
   *
   * Input:
   *  RootPtr  - pointer to the tree header structure for this tree.
   *  Node1    - \
   *              > These are the two nodes which are to be swapped.
   *  Node2    - /
   *
   * Notes:
   *  This function does a three step swap, using a dummy node as a place
   *  holder.  This function is used by ubi_btRemove().
   * ------------------------------------------------------------------------ **
   */
  {
  ubi_btNodePtr *Parent;
  ubi_btNode     dummy;
  ubi_btNodePtr  dummy_p = &dummy;

  /* Replace Node 1 with the dummy, thus removing Node1 from the tree. */
  if( Node1->Link[ubi_trPARENT] )
    Parent = &((Node1->Link[ubi_trPARENT])->Link[(int)(Node1->gender)]);
  else
    Parent = &(RootPtr->root);
  ReplaceNode( Parent, Node1, dummy_p );

  /* Swap Node 1 with Node 2, placing Node 1 back into the tree. */
  if( Node2->Link[ubi_trPARENT] )
    Parent = &((Node2->Link[ubi_trPARENT])->Link[(int)(Node2->gender)]);
  else
    Parent = &(RootPtr->root);
  ReplaceNode( Parent, Node2, Node1 );

  /* Swap Node 2 and the dummy, thus placing Node 2 back into the tree. */
  if( dummy_p->Link[ubi_trPARENT] )
    Parent = &((dummy_p->Link[ubi_trPARENT])->Link[(int)(dummy_p->gender)]);
  else
    Parent = &(RootPtr->root);
  ReplaceNode( Parent, dummy_p, Node2 );
  } /* SwapNodes */

/* -------------------------------------------------------------------------- **
 * These routines allow you to walk through the tree, forwards or backwards.
 */

static ubi_btNodePtr SubSlide( register ubi_btNodePtr P,
                               register int           whichway )
  /* ------------------------------------------------------------------------ **
   * Slide down the side of a subtree.
   *
   * Given a starting node, this function returns a pointer to the LEFT-, or
   * RIGHT-most descendent, *or* (if whichway is PARENT) to the tree root.
   *
   *  Input:  P         - a pointer to a starting place.
   *          whichway  - the direction (LEFT, RIGHT, or PARENT) in which to
   *                      travel.
   *  Output: A pointer to a node that is either the root, or has no
   *          whichway-th child but is within the subtree of P.  Note that
   *          the return value may be the same as P.  The return value *will
   *          be* NULL if P is NULL.
   * ------------------------------------------------------------------------ **
   */
  {
  ubi_btNodePtr Q = NULL;

  while( P )
    {
    Q = P;
    P = P->Link[ whichway ];
    }
  return( Q );
  } /* SubSlide */

static ubi_btNodePtr Neighbor( register ubi_btNodePtr P,
                               register int           whichway )
  /* ------------------------------------------------------------------------ **
   * Given starting point p, return the (key order) next or preceeding node
   * in the tree.
   *
   *  Input:  P         - Pointer to our starting place node.
   *          whichway  - the direction in which to travel to find the
   *                      neighbor, i.e., the RIGHT neighbor or the LEFT
   *                      neighbor.
   *
   *  Output: A pointer to the neighboring node, or NULL if P was NULL.
   *
   *  Notes:  If whichway is PARENT, the results are unpredictable.
   * ------------------------------------------------------------------------ **
   */
  {
  if( P )
    {
    if( P->Link[ whichway ] )
      return( SubSlide( P->Link[ whichway ], (char)ubi_trRevWay(whichway) ) );
    else
      while( P->Link[ ubi_trPARENT ] )
        {
        if( (P->Link[ ubi_trPARENT ])->Link[ whichway ] == P )
          P = P->Link[ ubi_trPARENT ];
        else
          return( P->Link[ ubi_trPARENT ] );
        }
    }
  return( NULL );
  } /* Neighbor */

static ubi_btNodePtr Border( ubi_btRootPtr RootPtr,
                             ubi_btItemPtr FindMe,
                             ubi_btNodePtr p,
                             int           whichway )
  /* ------------------------------------------------------------------------ **
   * Given starting point p, which has a key value equal to *FindMe, locate
   * the first (index order) node with the same key value.
   *
   * This function is useful in trees that have can have duplicate keys.
   * For example, consider the following tree:
   *     Tree                                                      Traversal
   *       2    If <p> points to the root and <whichway> is RIGHT,     3
   *      / \    then the return value will be a pointer to the       / \
   *     2   2    RIGHT child of the root node.  The tree on         2   5
   *    /   / \    the right shows the order of traversal.          /   / \
   *   1   2   3                                                   1   4   6
   *
   *  Input:  RootPtr   - Pointer to the tree root structure.
   *          FindMe    - Key value for comparisons.
   *          p         - Pointer to the starting-point node.
   *          whichway  - the direction in which to travel to find the
   *                      neighbor, i.e., the RIGHT neighbor or the LEFT
   *                      neighbor.
   *
   *  Output: A pointer to the first (index, or "traversal", order) node with
   *          a Key value that matches *FindMe.
   *
   *  Notes:  If whichway is PARENT, or if the tree does not allow duplicate
   *          keys, this function will return <p>.
   * ------------------------------------------------------------------------ **
   */
  {
  register ubi_btNodePtr q;

  /* Exit if there's nothing that can be done. */
  if( !ubi_trDups_OK( RootPtr ) || (ubi_trPARENT == whichway) )
    return( p );

  /* First, if needed, move up the tree.  We need to get to the root of the
   * subtree that contains all of the matching nodes.
   */
  q = p->Link[ubi_trPARENT];
  while( q && (ubi_trEQUAL == ubi_trAbNormal( (*(RootPtr->cmp))(FindMe, q) )) )
    {
    p = q;
    q = p->Link[ubi_trPARENT];
    }

  /* Next, move back down in the "whichway" direction. */
  q = p->Link[whichway];
  while( q )
    {
    q = qFind( RootPtr->cmp, FindMe, q );
    if( q )
      {
      p = q;
      q = p->Link[whichway];
      }
    }
  return( p );
  } /* Border */


/* ========================================================================== **
 * Exported utilities.
 */

long ubi_btSgn( register long x )
  /* ------------------------------------------------------------------------ **
   * Return the sign of x; {negative,zero,positive} ==> {-1, 0, 1}.
   *
   *  Input:  x - a signed long integer value.
   *
   *  Output: the "sign" of x, represented as follows:
   *            -1 == negative
   *             0 == zero (no sign)
   *             1 == positive
   *
   * Note: This utility is provided in order to facilitate the conversion
   *       of C comparison function return values into BinTree direction
   *       values: {LEFT, PARENT, EQUAL}.  It is INCORPORATED into the
   *       ubi_trAbNormal() conversion macro!
   *
   * ------------------------------------------------------------------------ **
   */
  {
  return( (x)?((x>0)?(1):(-1)):(0) );
  } /* ubi_btSgn */

ubi_btNodePtr ubi_btInitNode( ubi_btNodePtr NodePtr )
  /* ------------------------------------------------------------------------ **
   * Initialize a tree node.
   *
   *  Input:  a pointer to a ubi_btNode structure to be initialized.
   *  Output: a pointer to the initialized ubi_btNode structure (ie. the
   *          same as the input pointer).
   * ------------------------------------------------------------------------ **
   */
  {
  NodePtr->Link[ ubi_trLEFT ]   = NULL;
  NodePtr->Link[ ubi_trPARENT ] = NULL;
  NodePtr->Link[ ubi_trRIGHT ]  = NULL;
  NodePtr->gender               = ubi_trEQUAL;
  NodePtr->balance              = ubi_trEQUAL;
  return( NodePtr );
  } /* ubi_btInitNode */

ubi_btRootPtr ubi_btInitTree( ubi_btRootPtr   RootPtr,
                              ubi_btCompFunc  CompFunc,
                              char            Flags )
  /* ------------------------------------------------------------------------ **
   * Initialize the fields of a Tree Root header structure.
   *
   *  Input:   RootPtr   - a pointer to an ubi_btRoot structure to be
   *                       initialized.
   *           CompFunc  - a pointer to a comparison function that will be used
   *                       whenever nodes in the tree must be compared against
   *                       outside values.
   *           Flags     - One bytes worth of flags.  Flags include
   *                       ubi_trOVERWRITE and ubi_trDUPKEY.  See the header
   *                       file for more info.
   *
   *  Output:  a pointer to the initialized ubi_btRoot structure (ie. the
   *           same value as RootPtr).
   *
   *  Note:    The interface to this function has changed from that of
   *           previous versions.  The <Flags> parameter replaces two
   *           boolean parameters that had the same basic effect.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  if( RootPtr )
    {
    RootPtr->root   = NULL;
    RootPtr->count  = 0L;
    RootPtr->cmp    = CompFunc;
    RootPtr->flags  = (Flags & ubi_trDUPKEY) ? ubi_trDUPKEY : Flags;
    }                 /* There are only two supported flags, and they are
                       * mutually exclusive.  ubi_trDUPKEY takes precedence
                       * over ubi_trOVERWRITE.
                       */
  return( RootPtr );
  } /* ubi_btInitTree */

ubi_trBool ubi_btInsert( ubi_btRootPtr  RootPtr,
                         ubi_btNodePtr  NewNode,
                         ubi_btItemPtr  ItemPtr,
                         ubi_btNodePtr *OldNode )
  /* ------------------------------------------------------------------------ **
   * This function uses a non-recursive algorithm to add a new element to the
   * tree.
   *
   *  Input:   RootPtr  -  a pointer to the ubi_btRoot structure that indicates
   *                       the root of the tree to which NewNode is to be added.
   *           NewNode  -  a pointer to an ubi_btNode structure that is NOT
   *                       part of any tree.
   *           ItemPtr  -  A pointer to the sort key that is stored within
   *                       *NewNode.  ItemPtr MUST point to information stored
   *                       in *NewNode or an EXACT DUPLICATE.  The key data
   *                       indicated by ItemPtr is used to place the new node
   *                       into the tree.
   *           OldNode  -  a pointer to an ubi_btNodePtr.  When searching
   *                       the tree, a duplicate node may be found.  If
   *                       duplicates are allowed, then the new node will
   *                       be simply placed into the tree.  If duplicates
   *                       are not allowed, however, then one of two things
   *                       may happen.
   *                       1) if overwritting *is not* allowed, this
   *                          function will return FALSE (indicating that
   *                          the new node could not be inserted), and
   *                          *OldNode will point to the duplicate that is
   *                          still in the tree.
   *                       2) if overwritting *is* allowed, then this
   *                          function will swap **OldNode for *NewNode.
   *                          In this case, *OldNode will point to the node
   *                          that was removed (thus allowing you to free
   *                          the node).
   *                          **  If you are using overwrite mode, ALWAYS  **
   *                          ** check the return value of this parameter! **
   *                 Note: You may pass NULL in this parameter, the
   *                       function knows how to cope.  If you do this,
   *                       however, there will be no way to return a
   *                       pointer to an old (ie. replaced) node (which is
   *                       a problem if you are using overwrite mode).
   *
   *  Output:  a boolean value indicating success or failure.  The function
   *           will return FALSE if the node could not be added to the tree.
   *           Such failure will only occur if duplicates are not allowed,
   *           nodes cannot be overwritten, AND a duplicate key was found
   *           within the tree.
   * ------------------------------------------------------------------------ **
   */
  {
  ubi_btNodePtr OtherP,
                parent = NULL;
  char          tmp;

  if( !(OldNode) )       /* If they didn't give us a pointer, supply our own. */
    OldNode = &OtherP;

  (void)ubi_btInitNode( NewNode );     /* Init the new node's BinTree fields. */

  /* Find a place for the new node. */
  *OldNode = TreeFind(ItemPtr, (RootPtr->root), &parent, &tmp, (RootPtr->cmp));

  /* Now add the node to the tree... */
  if (!(*OldNode)) /* The easy one: we have a space for a new node! */
    {
    if (!(parent))
      RootPtr->root = NewNode;
    else
      {
      parent->Link[(int)tmp]      = NewNode;
      NewNode->Link[ubi_trPARENT] = parent;
      NewNode->gender             = tmp;
      }
    (RootPtr->count)++;
    return( ubi_trTRUE );
    }

  /* If we reach this point, we know that a duplicate node exists.  This
   * section adds the node to the tree if duplicate keys are allowed.
   */
  if( ubi_trDups_OK(RootPtr) )    /* Key exists, add duplicate */
    {
    ubi_btNodePtr q;

    tmp = ubi_trRIGHT;
    q = (*OldNode);
    *OldNode = NULL;
    while( q )
      {
      parent = q;
      if( tmp == ubi_trEQUAL )
        tmp = ubi_trRIGHT;
      q = q->Link[(int)tmp];
      if ( q )
        tmp = ubi_trAbNormal( (*(RootPtr->cmp))(ItemPtr, q) );
      }
    parent->Link[(int)tmp]       = NewNode;
    NewNode->Link[ubi_trPARENT]  = parent;
    NewNode->gender              = tmp;
    (RootPtr->count)++;
    return( ubi_trTRUE );
    }

  /* If we get to *this* point, we know that we are not allowed to have
   * duplicate nodes, but our node keys match, so... may we replace the
   * old one?
   */
  if( ubi_trOvwt_OK(RootPtr) )    /* Key exists, we replace */
    {
    if (!(parent))
      ReplaceNode( &(RootPtr->root), *OldNode, NewNode );
    else
      ReplaceNode( &(parent->Link[(int)((*OldNode)->gender)]),
                   *OldNode, NewNode );
    return( ubi_trTRUE );
    }

  return( ubi_trFALSE );      /* Failure: could not replace an existing node. */
  } /* ubi_btInsert */

ubi_btNodePtr ubi_btRemove( ubi_btRootPtr RootPtr,
                            ubi_btNodePtr DeadNode )
  /* ------------------------------------------------------------------------ **
   * This function removes the indicated node from the tree.
   *
   *  Input:   RootPtr  -  A pointer to the header of the tree that contains
   *                       the node to be removed.
   *           DeadNode -  A pointer to the node that will be removed.
   *
   *  Output:  This function returns a pointer to the node that was removed
   *           from the tree (ie. the same as DeadNode).
   *
   *  Note:    The node MUST be in the tree indicated by RootPtr.  If not,
   *           strange and evil things will happen to your trees.
   * ------------------------------------------------------------------------ **
   */
  {
  ubi_btNodePtr p,
               *parentp;
  int           tmp;

  /* if the node has both left and right subtrees, then we have to swap
   * it with another node.  The other node we choose will be the Prev()ious
   * node, which is garunteed to have no RIGHT child.
   */
  if( (DeadNode->Link[ubi_trLEFT]) && (DeadNode->Link[ubi_trRIGHT]) )
    SwapNodes( RootPtr, DeadNode, ubi_btPrev( DeadNode ) );

  /* The parent of the node to be deleted may be another node, or it may be
   * the root of the tree.  Since we're not sure, it's best just to have
   * a pointer to the parent pointer, whatever it is.
   */
  if (DeadNode->Link[ubi_trPARENT])
    parentp = &((DeadNode->Link[ubi_trPARENT])->Link[(int)(DeadNode->gender)]);
  else
    parentp = &( RootPtr->root );

  /* Now link the parent to the only grand-child and patch up the gender. */
  tmp = ((DeadNode->Link[ubi_trLEFT])?ubi_trLEFT:ubi_trRIGHT);

  p = (DeadNode->Link[tmp]);
  if( p )
    {
    p->Link[ubi_trPARENT] = DeadNode->Link[ubi_trPARENT];
    p->gender       = DeadNode->gender;
    }
  (*parentp) = p;

  /* Finished, reduce the node count and return. */
  (RootPtr->count)--;
  return( DeadNode );
  } /* ubi_btRemove */

ubi_btNodePtr ubi_btLocate( ubi_btRootPtr RootPtr,
                            ubi_btItemPtr FindMe,
                            ubi_trCompOps CompOp )
  /* ------------------------------------------------------------------------ **
   * The purpose of ubi_btLocate() is to find a node or set of nodes given
   * a target value and a "comparison operator".  The Locate() function is
   * more flexible and (in the case of trees that may contain dupicate keys)
   * more precise than the ubi_btFind() function.  The latter is faster,
   * but it only searches for exact matches and, if the tree contains
   * duplicates, Find() may return a pointer to any one of the duplicate-
   * keyed records.
   *
   *  Input:
   *     RootPtr  -  A pointer to the header of the tree to be searched.
   *     FindMe   -  An ubi_btItemPtr that indicates the key for which to
   *                 search.
   *     CompOp   -  One of the following:
   *                    CompOp     Return a pointer to the node with
   *                    ------     ---------------------------------
   *                   ubi_trLT - the last key value that is less
   *                              than FindMe.
   *                   ubi_trLE - the first key matching FindMe, or
   *                              the last key that is less than
   *                              FindMe.
   *                   ubi_trEQ - the first key matching FindMe.
   *                   ubi_trGE - the first key matching FindMe, or the
   *                              first key greater than FindMe.
   *                   ubi_trGT - the first key greater than FindMe.
   *  Output:
   *     A pointer to the node matching the criteria listed above under
   *     CompOp, or NULL if no node matched the criteria.
   *
   *  Notes:
   *     In the case of trees with duplicate keys, Locate() will behave as
   *     follows:
   *
   *     Find:  3                       Find: 3
   *     Keys:  1 2 2 2 3 3 3 3 3 4 4   Keys: 1 1 2 2 2 4 4 5 5 5 6
   *                  ^ ^         ^                   ^ ^
   *                 LT EQ        GT                 LE GE
   *
   *     That is, when returning a pointer to a node with a key that is LESS
   *     THAN the target key (FindMe), Locate() will return a pointer to the
   *     LAST matching node.
   *     When returning a pointer to a node with a key that is GREATER
   *     THAN the target key (FindMe), Locate() will return a pointer to the
   *     FIRST matching node.
   *
   *  See Also: ubi_btFind(), ubi_btFirstOf(), ubi_btLastOf().
   * ------------------------------------------------------------------------ **
   */
  {
  register ubi_btNodePtr p;
  ubi_btNodePtr   parent;
  char            whichkid;

  /* Start by searching for a matching node. */
  p = TreeFind( FindMe,
                RootPtr->root,
                &parent,
                &whichkid,
                RootPtr->cmp );

  if( p )   /* If we have found a match, we can resolve as follows: */
    {
    switch( CompOp )
      {
      case ubi_trLT:            /* It's just a jump to the left...  */
        p = Border( RootPtr, FindMe, p, ubi_trLEFT );
        return( Neighbor( p, ubi_trLEFT ) );
      case ubi_trGT:            /* ...and then a jump to the right. */
        p = Border( RootPtr, FindMe, p, ubi_trRIGHT );
        return( Neighbor( p, ubi_trRIGHT ) );
      default:
        p = Border( RootPtr, FindMe, p, ubi_trLEFT );
        return( p );
      }
    }

  /* Else, no match. */
  if( ubi_trEQ == CompOp )    /* If we were looking for an exact match... */
    return( NULL );           /* ...forget it.                            */

  /* We can still return a valid result for GT, GE, LE, and LT.
   * <parent> points to a node with a value that is either just before or
   * just after the target value.
   * Remaining possibilities are LT and GT (including LE & GE).
   */
  if( (ubi_trLT == CompOp) || (ubi_trLE == CompOp) )
    return( (ubi_trLEFT == whichkid) ? Neighbor( parent, whichkid ) : parent );
  else
    return( (ubi_trRIGHT == whichkid) ? Neighbor( parent, whichkid ) : parent );
  } /* ubi_btLocate */

ubi_btNodePtr ubi_btFind( ubi_btRootPtr RootPtr,
                          ubi_btItemPtr FindMe )
  /* ------------------------------------------------------------------------ **
   * This function performs a non-recursive search of a tree for any node
   * matching a specific key.
   *
   *  Input:
   *     RootPtr  -  a pointer to the header of the tree to be searched.
   *     FindMe   -  a pointer to the key value for which to search.
   *
   *  Output:
   *     A pointer to a node with a key that matches the key indicated by
   *     FindMe, or NULL if no such node was found.
   *
   *  Note:   In a tree that allows duplicates, the pointer returned *might
   *          not* point to the (sequentially) first occurance of the
   *          desired key.  In such a tree, it may be more useful to use
   *          ubi_btLocate().
   * ------------------------------------------------------------------------ **
   */
  {
  return( qFind( RootPtr->cmp, FindMe, RootPtr->root ) );
  } /* ubi_btFind */

ubi_btNodePtr ubi_btNext( ubi_btNodePtr P )
  /* ------------------------------------------------------------------------ **
   * Given the node indicated by P, find the (sorted order) Next node in the
   * tree.
   *  Input:   P  -  a pointer to a node that exists in a binary tree.
   *  Output:  A pointer to the "next" node in the tree, or NULL if P pointed
   *           to the "last" node in the tree or was NULL.
   * ------------------------------------------------------------------------ **
   */
  {
  return( Neighbor( P, ubi_trRIGHT ) );
  } /* ubi_btNext */

ubi_btNodePtr ubi_btPrev( ubi_btNodePtr P )
  /* ------------------------------------------------------------------------ **
   * Given the node indicated by P, find the (sorted order) Previous node in
   * the tree.
   *  Input:   P  -  a pointer to a node that exists in a binary tree.
   *  Output:  A pointer to the "previous" node in the tree, or NULL if P
   *           pointed to the "first" node in the tree or was NULL.
   * ------------------------------------------------------------------------ **
   */
  {
  return( Neighbor( P, ubi_trLEFT ) );
  } /* ubi_btPrev */

ubi_btNodePtr ubi_btFirst( ubi_btNodePtr P )
  /* ------------------------------------------------------------------------ **
   * Given the node indicated by P, find the (sorted order) First node in the
   * subtree of which *P is the root.
   *  Input:   P  -  a pointer to a node that exists in a binary tree.
   *  Output:  A pointer to the "first" node in a subtree that has *P as its
   *           root.  This function will return NULL only if P is NULL.
   *  Note:    In general, you will be passing in the value of the root field
   *           of an ubi_btRoot structure.
   * ------------------------------------------------------------------------ **
   */
  {
  return( SubSlide( P, ubi_trLEFT ) );
  } /* ubi_btFirst */

ubi_btNodePtr ubi_btLast( ubi_btNodePtr P )
  /* ------------------------------------------------------------------------ **
   * Given the node indicated by P, find the (sorted order) Last node in the
   * subtree of which *P is the root.
   *  Input:   P  -  a pointer to a node that exists in a binary tree.
   *  Output:  A pointer to the "last" node in a subtree that has *P as its
   *           root.  This function will return NULL only if P is NULL.
   *  Note:    In general, you will be passing in the value of the root field
   *           of an ubi_btRoot structure.
   * ------------------------------------------------------------------------ **
   */
  {
  return( SubSlide( P, ubi_trRIGHT ) );
  } /* ubi_btLast */

ubi_btNodePtr ubi_btFirstOf( ubi_btRootPtr RootPtr,
                             ubi_btItemPtr MatchMe,
                             ubi_btNodePtr p )
  /* ------------------------------------------------------------------------ **
   * Given a tree that a allows duplicate keys, and a pointer to a node in
   * the tree, this function will return a pointer to the first (traversal
   * order) node with the same key value.
   *
   *  Input:  RootPtr - A pointer to the root of the tree.
   *          MatchMe - A pointer to the key value.  This should probably
   *                    point to the key within node *p.
   *          p       - A pointer to a node in the tree.
   *  Output: A pointer to the first node in the set of nodes with keys
   *          matching <FindMe>.
   *  Notes:  Node *p MUST be in the set of nodes with keys matching
   *          <FindMe>.  If not, this function will return NULL.
   *
   *          4.7: Bug found & fixed by Massimo Campostrini,
   *               Istituto Nazionale di Fisica Nucleare, Sezione di Pisa.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  /* If our starting point is invalid, return NULL. */
  if( !p || (ubi_trEQUAL != ubi_trAbNormal( (*(RootPtr->cmp))( MatchMe, p ) )) )
    return( NULL );
  return( Border( RootPtr, MatchMe, p, ubi_trLEFT ) );
  } /* ubi_btFirstOf */

ubi_btNodePtr ubi_btLastOf( ubi_btRootPtr RootPtr,
                            ubi_btItemPtr MatchMe,
                            ubi_btNodePtr p )
  /* ------------------------------------------------------------------------ **
   * Given a tree that a allows duplicate keys, and a pointer to a node in
   * the tree, this function will return a pointer to the last (traversal
   * order) node with the same key value.
   *
   *  Input:  RootPtr - A pointer to the root of the tree.
   *          MatchMe - A pointer to the key value.  This should probably
   *                    point to the key within node *p.
   *          p       - A pointer to a node in the tree.
   *  Output: A pointer to the last node in the set of nodes with keys
   *          matching <FindMe>.
   *  Notes:  Node *p MUST be in the set of nodes with keys matching
   *          <FindMe>.  If not, this function will return NULL.
   *
   *          4.7: Bug found & fixed by Massimo Campostrini,
   *               Istituto Nazionale di Fisica Nucleare, Sezione di Pisa.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  /* If our starting point is invalid, return NULL. */
  if( !p || (ubi_trEQUAL != ubi_trAbNormal( (*(RootPtr->cmp))( MatchMe, p ) )) )
    return( NULL );
  return( Border( RootPtr, MatchMe, p, ubi_trRIGHT ) );
  } /* ubi_btLastOf */

ubi_trBool ubi_btTraverse( ubi_btRootPtr   RootPtr,
                           ubi_btActionRtn EachNode,
                           void           *UserData )
  /* ------------------------------------------------------------------------ **
   * Traverse a tree in sorted order (non-recursively).  At each node, call
   * (*EachNode)(), passing a pointer to the current node, and UserData as the
   * second parameter.
   *  Input:   RootPtr  -  a pointer to an ubi_btRoot structure that indicates
   *                       the tree to be traversed.
   *           EachNode -  a pointer to a function to be called at each node
   *                       as the node is visited.
   *           UserData -  a generic pointer that may point to anything that
   *                       you choose.
   *  Output:  A boolean value.  FALSE if the tree is empty, otherwise TRUE.
   * ------------------------------------------------------------------------ **
   */
  {
  ubi_btNodePtr p;

  if( !(p = ubi_btFirst( RootPtr->root )) ) return( ubi_trFALSE );

  while( p )
    {
    (*EachNode)( p, UserData );
    p = ubi_btNext( p );
    }
  return( ubi_trTRUE );
  } /* ubi_btTraverse */

ubi_trBool ubi_btKillTree( ubi_btRootPtr     RootPtr,
                           ubi_btKillNodeRtn FreeNode )
  /* ------------------------------------------------------------------------ **
   * Delete an entire tree (non-recursively) and reinitialize the ubi_btRoot
   * structure.  Note that this function will return FALSE if either parameter
   * is NULL.
   *
   *  Input:   RootPtr  -  a pointer to an ubi_btRoot structure that indicates
   *                       the root of the tree to delete.
   *           FreeNode -  a function that will be called for each node in the
   *                       tree to deallocate the memory used by the node.
   *
   *  Output:  A boolean value.  FALSE if either input parameter was NULL, else
   *           TRUE.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  ubi_btNodePtr p, q;

  if( !(RootPtr) || !(FreeNode) )
    return( ubi_trFALSE );

  p = ubi_btFirst( RootPtr->root );
  while( p )
    {
    q = p;
    while( q->Link[ubi_trRIGHT] )
      q = SubSlide( q->Link[ubi_trRIGHT], ubi_trLEFT );
    p = q->Link[ubi_trPARENT];
    if( p )
      p->Link[ ((p->Link[ubi_trLEFT] == q)?ubi_trLEFT:ubi_trRIGHT) ] = NULL;
    (*FreeNode)((void *)q);
    }

  (void)ubi_btInitTree( RootPtr,
                        RootPtr->cmp,
                        RootPtr->flags );
  return( ubi_trTRUE );
  } /* ubi_btKillTree */

ubi_btNodePtr ubi_btLeafNode( ubi_btNodePtr leader )
  /* ------------------------------------------------------------------------ **
   * Returns a pointer to a leaf node.
   *
   *  Input:  leader  - Pointer to a node at which to start the descent.
   *
   *  Output: A pointer to a leaf node selected in a somewhat arbitrary
   *          manner.
   *
   *  Notes:  I wrote this function because I was using splay trees as a
   *          database cache.  The cache had a maximum size on it, and I
   *          needed a way of choosing a node to sacrifice if the cache
   *          became full.  In a splay tree, less recently accessed nodes
   *          tend toward the bottom of the tree, meaning that leaf nodes
   *          are good candidates for removal.  (I really can't think of
   *          any other reason to use this function.)
   *        + In a simple binary tree or an AVL tree, the most recently
   *          added nodes tend to be nearer the bottom, making this a *bad*
   *          way to choose which node to remove from the cache.
   *        + Randomizing the traversal order is probably a good idea.  You
   *          can improve the randomization of leaf node selection by passing
   *          in pointers to nodes other than the root node each time.  A
   *          pointer to any node in the tree will do.  Of course, if you
   *          pass a pointer to a leaf node you'll get the same thing back.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  ubi_btNodePtr follower = NULL;
  int           whichway = ubi_trLEFT;

  while( NULL != leader )
    {
    follower = leader;
    leader   = follower->Link[ whichway ];
    if( NULL == leader )
      {
      whichway = ubi_trRevWay( whichway );
      leader   = follower->Link[ whichway ];
      }
    }

  return( follower );
  } /* ubi_btLeafNode */

int ubi_btModuleID( int size, char *list[] )
  /* ------------------------------------------------------------------------ **
   * Returns a set of strings that identify the module.
   *
   *  Input:  size  - The number of elements in the array <list>.
   *          list  - An array of pointers of type (char *).  This array
   *                  should, initially, be empty.  This function will fill
   *                  in the array with pointers to strings.
   *  Output: The number of elements of <list> that were used.  If this value
   *          is less than <size>, the values of the remaining elements are
   *          not guaranteed.
   *
   *  Notes:  Please keep in mind that the pointers returned indicate strings
   *          stored in static memory.  Don't free() them, don't write over
   *          them, etc.  Just read them.
   * ------------------------------------------------------------------------ **
   */
  {
  if( size > 0 )
    {
    list[0] = ModuleID;
    if( size > 1 )
      list[1] = NULL;
    return( 1 );
    }
  return( 0 );
  } /* ubi_btModuleID */


/* ========================================================================== */
