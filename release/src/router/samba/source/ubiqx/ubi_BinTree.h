#ifndef UBI_BINTREE_H
#define UBI_BINTREE_H
/* ========================================================================== **
 *                              ubi_BinTree.h
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
 * Log: ubi_BinTree.h,v
 * Revision 4.7  1998/10/21 06:15:07  crh
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
 * Revision 4.1  1998/03/31 06:13:47  crh
 * Thomas Aglassinger sent E'mail pointing out errors in the
 * dereferencing of function pointers, and a missing typecast.
 * Thanks, Thomas!
 *
 * Revision 4.0  1998/03/10 03:16:04  crh
 * Added the AVL field 'balance' to the ubi_btNode structure.  This means
 * that all BinTree modules now use the same basic node structure, which
 * greatly simplifies the AVL module.
 * Decided that this was a big enough change to justify a new major revision
 * number.  3.0 was an error, so we're at 4.0.
 *
 * Revision 2.6  1998/01/24 06:27:30  crh
 * Added ubi_trCount() macro.
 *
 * Revision 2.5  1997/12/23 03:59:21  crh
 * In this version, all constants & macros defined in the header file have
 * the ubi_tr prefix.  Also cleaned up anything that gcc complained about
 * when run with '-pedantic -fsyntax-only -Wall'.
 *
 * Revision 2.4  1997/07/26 04:11:14  crh
 * + Just to be annoying I changed ubi_TRUE and ubi_FALSE to ubi_trTRUE
 *   and ubi_trFALSE.
 * + There is now a type ubi_trBool to go with ubi_trTRUE and ubi_trFALSE.
 * + There used to be something called "ubi_TypeDefs.h".  I got rid of it.
 * + Added function ubi_btLeafNode().
 *
 * Revision 2.3  1997/06/03 05:15:27  crh
 * Changed TRUE and FALSE to ubi_TRUE and ubi_FALSE to avoid conflicts.
 * Also changed the interface to function InitTree().  See the comments
 * for this function for more information.
 *
 * Revision 2.2  1995/10/03 22:00:40  CRH
 * Ubisized!
 * 
 * Revision 2.1  95/03/09  23:43:46  CRH
 * Added the ModuleID static string and function.  These modules are now
 * self-identifying.
 * 
 * Revision 2.0  95/02/27  22:00:33  CRH
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
 * Revision 1.0  93/10/15  22:55:04  CRH
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

#include "sys_include.h"  /* Global include file, used to adapt the ubiqx
                           * modules to the host environment and the project
                           * with which the modules will be used.  See
                           * sys_include.h for more info.
                           */

/* -------------------------------------------------------------------------- **
 * Macros and constants.
 *
 *  General purpose:
 *    ubi_trTRUE  - Boolean TRUE.
 *    ubi_trFALSE - Boolean FALSE.
 *
 *  Flags used in the tree header:
 *    ubi_trOVERWRITE   - This flag indicates that an existing node may be
 *                        overwritten by a new node with a matching key.
 *    ubi_trDUPKEY      - This flag indicates that the tree allows duplicate
 *                        keys.  If the tree does allow duplicates, the
 *                        overwrite flag is ignored.
 *
 *  Node link array index constants:  (Each node has an array of three
 *  pointers.  One to the left, one to the right, and one back to the
 *  parent.)
 *    ubi_trLEFT    - Left child pointer.
 *    ubi_trPARENT  - Parent pointer.
 *    ubi_trRIGHT   - Right child pointer.
 *    ubi_trEQUAL   - Synonym for PARENT.
 *
 *  ubi_trCompOps:  These values are used in the ubi_trLocate() function.
 *    ubi_trLT  - request the first instance of the greatest key less than
 *                the search key.
 *    ubi_trLE  - request the first instance of the greatest key that is less
 *                than or equal to the search key.
 *    ubi_trEQ  - request the first instance of key that is equal to the
 *                search key.
 *    ubi_trGE  - request the first instance of a key that is greater than
 *                or equal to the search key.
 *    ubi_trGT  - request the first instance of the first key that is greater
 *                than the search key.
 * -------------------------------------------------------------------------- **
 */

#define ubi_trTRUE  0xFF
#define ubi_trFALSE 0x00

#define ubi_trOVERWRITE 0x01        /* Turn on allow overwrite      */
#define ubi_trDUPKEY    0x02        /* Turn on allow duplicate keys */

/* Pointer array index constants... */
#define ubi_trLEFT   0x00
#define ubi_trPARENT 0x01
#define ubi_trRIGHT  0x02
#define ubi_trEQUAL  ubi_trPARENT

typedef enum {
  ubi_trLT = 1,
  ubi_trLE,
  ubi_trEQ,
  ubi_trGE,
  ubi_trGT
  } ubi_trCompOps;

/* -------------------------------------------------------------------------- **
 * These three macros allow simple manipulation of pointer index values (LEFT,
 * RIGHT, and PARENT).
 *
 *    Normalize() -  converts {LEFT, PARENT, RIGHT} into {-1, 0 ,1}.  C
 *                   uses {negative, zero, positive} values to indicate
 *                   {less than, equal to, greater than}.
 *    AbNormal()  -  converts {negative, zero, positive} to {LEFT, PARENT,
 *                   RIGHT} (opposite of Normalize()).  Note: C comparison
 *                   functions, such as strcmp(), return {negative, zero,
 *                   positive} values, which are not necessarily {-1, 0,
 *                   1}.  This macro uses the the ubi_btSgn() function to
 *                   compensate.
 *    RevWay()    -  converts LEFT to RIGHT and RIGHT to LEFT.  PARENT (EQUAL)
 *                   is left as is.
 * -------------------------------------------------------------------------- **
 */
#define ubi_trNormalize(W) ((char)( (W) - ubi_trEQUAL ))
#define ubi_trAbNormal(W)  ((char)( ((char)ubi_btSgn( (long)(W) )) \
                                         + ubi_trEQUAL ))
#define ubi_trRevWay(W)    ((char)( ubi_trEQUAL - ((W) - ubi_trEQUAL) ))

/* -------------------------------------------------------------------------- **
 * These macros allow us to quickly read the values of the OVERWRITE and
 * DUPlicate KEY bits of the tree root flags field.
 * -------------------------------------------------------------------------- **
 */
#define ubi_trDups_OK(A) \
        ((ubi_trDUPKEY & ((A)->flags))?(ubi_trTRUE):(ubi_trFALSE))
#define ubi_trOvwt_OK(A) \
        ((ubi_trOVERWRITE & ((A)->flags))?(ubi_trTRUE):(ubi_trFALSE))

/* -------------------------------------------------------------------------- **
 * Additional Macros...
 *
 *  ubi_trCount()   - Given a pointer to a tree root, this macro returns the
 *                    number of nodes currently in the tree.
 *
 *  ubi_trNewTree() - This macro makes it easy to declare and initialize a
 *                    tree header in one step.  The line
 *
 *                      static ubi_trNewTree( MyTree, cmpfn, ubi_trDUPKEY );
 *
 *                    is equivalent to
 *
 *                      static ubi_trRoot MyTree[1]
 *                        = {{ NULL, cmpfn, 0, ubi_trDUPKEY }};
 *
 * -------------------------------------------------------------------------- **
 */

#define ubi_trCount( R ) (((ubi_trRootPtr)(R))->count)

#define ubi_trNewTree( N, C, F ) ubi_trRoot (N)[1] = {{ NULL, (C), 0, (F) }}

/* -------------------------------------------------------------------------- **
 * Typedefs...
 * 
 * ubi_trBool   - Your typcial true or false...
 *
 * Item Pointer:  The ubi_btItemPtr is a generic pointer. It is used to
 *                indicate a key that is being searched for within the tree.
 *                Searching occurs whenever the ubi_trFind(), ubi_trLocate(),
 *                or ubi_trInsert() functions are called.
 * -------------------------------------------------------------------------- **
 */

typedef unsigned char ubi_trBool;

typedef void *ubi_btItemPtr;          /* A pointer to key data within a node. */

/*  ------------------------------------------------------------------------- **
 *  Binary Tree Node Structure:  This structure defines the basic elements of
 *       the tree nodes.  In general you *SHOULD NOT PLAY WITH THESE FIELDS*!
 *       But, of course, I have to put the structure into this header so that
 *       you can use it as a building block.
 *
 *  The fields are as follows:
 *    Link     -  an array of pointers.  These pointers are manipulated by
 *                the BT routines.  The pointers indicate the left and right
 *                child nodes and the parent node.  By keeping track of the
 *                parent pointer, we avoid the need for recursive routines or
 *                hand-tooled stacks to keep track of our path back to the
 *                root.  The use of these pointers is subject to change without
 *                notice.
 *    gender   -  a one-byte field indicating whether the node is the RIGHT or
 *                LEFT child of its parent.  If the node is the root of the
 *                tree, gender will be PARENT.
 *    balance  -  only used by the AVL tree module.  This field indicates
 *                the height balance at a given node.  See ubi_AVLtree for
 *                details.
 *
 *  ------------------------------------------------------------------------- **
 */
typedef struct ubi_btNodeStruct {
  struct ubi_btNodeStruct *Link[ 3 ];
  char                     gender;
  char                     balance;
  } ubi_btNode;

typedef ubi_btNode *ubi_btNodePtr;     /* Pointer to an ubi_btNode structure. */

/*  ------------------------------------------------------------------------- **
 * The next three typedefs define standard function types used by the binary
 * tree management routines.  In particular:
 *
 *    ubi_btCompFunc    is a pointer to a comparison function.  Comparison
 *                      functions are passed an ubi_btItemPtr and an
 *                      ubi_btNodePtr.  They return a value that is (<0), 0,
 *                      or (>0) to indicate that the Item is (respectively)
 *                      "less than", "equal to", or "greater than" the Item
 *                      contained within the node.  (See ubi_btInitTree()).
 *    ubi_btActionRtn   is a pointer to a function that may be called for each
 *                      node visited when performing a tree traversal (see
 *                      ubi_btTraverse()).  The function will be passed two
 *                      parameters: the first is a pointer to a node in the
 *                      tree, the second is a generic pointer that may point to
 *                      anything that you like.
 *    ubi_btKillNodeRtn is a pointer to a function that will deallocate the
 *                      memory used by a node (see ubi_btKillTree()).  Since
 *                      memory management is left up to you, deallocation may
 *                      mean anything that you want it to mean.  Just remember
 *                      that the tree *will* be destroyed and that none of the
 *                      node pointers will be valid any more.
 *  ------------------------------------------------------------------------- **
 */

typedef  int (*ubi_btCompFunc)( ubi_btItemPtr, ubi_btNodePtr );

typedef void (*ubi_btActionRtn)( ubi_btNodePtr, void * );

typedef void (*ubi_btKillNodeRtn)( ubi_btNodePtr );

/* -------------------------------------------------------------------------- **
 * Tree Root Structure: This structure gives us a convenient handle for
 *                      accessing whole binary trees.  The fields are:
 *    root  -  A pointer to the root node of the tree.
 *    count -  A count of the number of nodes stored in the tree.
 *    cmp   -  A pointer to the comparison routine to be used when building or
 *             searching the tree.
 *    flags -  A set of bit flags.  Two flags are currently defined:
 *
 *       ubi_trOVERWRITE -  If set, this flag indicates that a new node should
 *         (bit 0x01)       overwrite an old node if the two have identical
 *                          keys (ie., the keys are equal).
 *       ubi_trDUPKEY    -  If set, this flag indicates that the tree is
 *         (bit 0x02)       allowed to contain nodes with duplicate keys.
 *
 *       NOTE: ubi_trInsert() tests ubi_trDUPKEY before ubi_trOVERWRITE.
 *
 * All of these values are set when you initialize the root structure by
 * calling ubi_trInitTree().
 * -------------------------------------------------------------------------- **
 */

typedef struct {
  ubi_btNodePtr  root;     /* A pointer to the root node of the tree       */
  ubi_btCompFunc cmp;      /* A pointer to the tree's comparison function  */
  unsigned long  count;    /* A count of the number of nodes in the tree   */
  char           flags;    /* Overwrite Y|N, Duplicate keys Y|N...         */
  } ubi_btRoot;

typedef ubi_btRoot *ubi_btRootPtr;  /* Pointer to an ubi_btRoot structure. */


/* -------------------------------------------------------------------------- **
 * Function Prototypes.
 */

long ubi_btSgn( long x );
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
   *       AbNormal() conversion macro!
   *
   * ------------------------------------------------------------------------ **
   */

ubi_btNodePtr ubi_btInitNode( ubi_btNodePtr NodePtr );
  /* ------------------------------------------------------------------------ **
   * Initialize a tree node.
   *
   *  Input:   a pointer to a ubi_btNode structure to be initialized.
   *  Output:  a pointer to the initialized ubi_btNode structure (ie. the
   *           same as the input pointer).
   * ------------------------------------------------------------------------ **
   */

ubi_btRootPtr  ubi_btInitTree( ubi_btRootPtr   RootPtr,
                               ubi_btCompFunc  CompFunc,
                               char            Flags );
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
   * ------------------------------------------------------------------------ **
   */

ubi_trBool ubi_btInsert( ubi_btRootPtr  RootPtr,
                         ubi_btNodePtr  NewNode,
                         ubi_btItemPtr  ItemPtr,
                         ubi_btNodePtr *OldNode );
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

ubi_btNodePtr ubi_btRemove( ubi_btRootPtr RootPtr,
                            ubi_btNodePtr DeadNode );
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

ubi_btNodePtr ubi_btLocate( ubi_btRootPtr RootPtr,
                            ubi_btItemPtr FindMe,
                            ubi_trCompOps CompOp );
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

ubi_btNodePtr ubi_btFind( ubi_btRootPtr RootPtr,
                          ubi_btItemPtr FindMe );
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

ubi_btNodePtr ubi_btNext( ubi_btNodePtr P );
  /* ------------------------------------------------------------------------ **
   * Given the node indicated by P, find the (sorted order) Next node in the
   * tree.
   *  Input:   P  -  a pointer to a node that exists in a binary tree.
   *  Output:  A pointer to the "next" node in the tree, or NULL if P pointed
   *           to the "last" node in the tree or was NULL.
   * ------------------------------------------------------------------------ **
   */

ubi_btNodePtr ubi_btPrev( ubi_btNodePtr P );
  /* ------------------------------------------------------------------------ **
   * Given the node indicated by P, find the (sorted order) Previous node in
   * the tree.
   *  Input:   P  -  a pointer to a node that exists in a binary tree.
   *  Output:  A pointer to the "previous" node in the tree, or NULL if P
   *           pointed to the "first" node in the tree or was NULL.
   * ------------------------------------------------------------------------ **
   */

ubi_btNodePtr ubi_btFirst( ubi_btNodePtr P );
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

ubi_btNodePtr ubi_btLast( ubi_btNodePtr P );
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

ubi_btNodePtr ubi_btFirstOf( ubi_btRootPtr RootPtr,
                             ubi_btItemPtr MatchMe,
                             ubi_btNodePtr p );
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

ubi_btNodePtr ubi_btLastOf( ubi_btRootPtr RootPtr,
                            ubi_btItemPtr MatchMe,
                            ubi_btNodePtr p );
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

ubi_trBool ubi_btTraverse( ubi_btRootPtr   RootPtr,
                           ubi_btActionRtn EachNode,
                           void           *UserData );
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

ubi_trBool ubi_btKillTree( ubi_btRootPtr     RootPtr,
                           ubi_btKillNodeRtn FreeNode );
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

ubi_btNodePtr ubi_btLeafNode( ubi_btNodePtr leader );
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


int ubi_btModuleID( int size, char *list[] );
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

/* -------------------------------------------------------------------------- **
 * Masquarade...
 *
 * This set of defines allows you to write programs that will use any of the
 * implemented binary tree modules (currently BinTree, AVLtree, and SplayTree).
 * Instead of using ubi_bt..., use ubi_tr..., and select the tree type by
 * including the appropriate module header.
 */

#define ubi_trItemPtr ubi_btItemPtr

#define ubi_trNode    ubi_btNode
#define ubi_trNodePtr ubi_btNodePtr

#define ubi_trRoot    ubi_btRoot
#define ubi_trRootPtr ubi_btRootPtr

#define ubi_trCompFunc    ubi_btCompFunc
#define ubi_trActionRtn   ubi_btActionRtn
#define ubi_trKillNodeRtn ubi_btKillNodeRtn

#define ubi_trSgn( x ) ubi_btSgn( x )

#define ubi_trInitNode( Np ) ubi_btInitNode( (ubi_btNodePtr)(Np) )

#define ubi_trInitTree( Rp, Cf, Fl ) \
        ubi_btInitTree( (ubi_btRootPtr)(Rp), (ubi_btCompFunc)(Cf), (Fl) )

#define ubi_trInsert( Rp, Nn, Ip, On ) \
        ubi_btInsert( (ubi_btRootPtr)(Rp), (ubi_btNodePtr)(Nn), \
                      (ubi_btItemPtr)(Ip), (ubi_btNodePtr *)(On) )

#define ubi_trRemove( Rp, Dn ) \
        ubi_btRemove( (ubi_btRootPtr)(Rp), (ubi_btNodePtr)(Dn) )

#define ubi_trLocate( Rp, Ip, Op ) \
        ubi_btLocate( (ubi_btRootPtr)(Rp), \
                      (ubi_btItemPtr)(Ip), \
                      (ubi_trCompOps)(Op) )

#define ubi_trFind( Rp, Ip ) \
        ubi_btFind( (ubi_btRootPtr)(Rp), (ubi_btItemPtr)(Ip) )

#define ubi_trNext( P ) ubi_btNext( (ubi_btNodePtr)(P) )

#define ubi_trPrev( P ) ubi_btPrev( (ubi_btNodePtr)(P) )

#define ubi_trFirst( P ) ubi_btFirst( (ubi_btNodePtr)(P) )

#define ubi_trLast( P ) ubi_btLast( (ubi_btNodePtr)(P) )

#define ubi_trFirstOf( Rp, Ip, P ) \
        ubi_btFirstOf( (ubi_btRootPtr)(Rp), \
                       (ubi_btItemPtr)(Ip), \
                       (ubi_btNodePtr)(P) )

#define ubi_trLastOf( Rp, Ip, P ) \
        ubi_btLastOf( (ubi_btRootPtr)(Rp), \
                      (ubi_btItemPtr)(Ip), \
                      (ubi_btNodePtr)(P) )

#define ubi_trTraverse( Rp, En, Ud ) \
        ubi_btTraverse((ubi_btRootPtr)(Rp), (ubi_btActionRtn)(En), (void *)(Ud))

#define ubi_trKillTree( Rp, Fn ) \
        ubi_btKillTree( (ubi_btRootPtr)(Rp), (ubi_btKillNodeRtn)(Fn) )

#define ubi_trLeafNode( Nd ) \
        ubi_btLeafNode( (ubi_btNodePtr)(Nd) )

#define ubi_trModuleID( s, l ) ubi_btModuleID( s, l )

/* ========================================================================== */
#endif /* UBI_BINTREE_H */
