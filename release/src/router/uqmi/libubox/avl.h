/*
 * PacketBB handler library (see RFC 5444)
 * Copyright (c) 2010 Henning Rogge <hrogge@googlemail.com>
 * Original OLSRd implementation by Hannes Gredler <hannes@gredler.at>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org/git for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 */

#ifndef _AVL_H
#define _AVL_H

#include <stddef.h>
#include <stdbool.h>

#include "list.h"
#include "list_compat.h"

/* Support for OLSR.org linker symbol export */
#define EXPORT(sym) sym

/**
 * This element is a member of a avl-tree. It must be contained in all
 * larger structs that should be put into a tree.
 */
struct avl_node {
  /**
   * Linked list node for supporting easy iteration and multiple
   * elments with the same key.
   *
   * this must be the first element of an avl_node to
   * make casting for lists easier
   */
  struct list_entity list;

  /**
   * Pointer to parent node in tree, NULL if root node
   */
  struct avl_node *parent;

  /**
   * Pointer to left child
   */
  struct avl_node *left;

  /**
   * Pointer to right child
   */
  struct avl_node *right;

  /**
   * pointer to key of node
   */
  const void *key;

  /**
   * balance state of AVL tree (0,-1,+1)
   */
  signed char balance;

  /**
   * true if first of a series of nodes with same key
   */
  bool leader;
};

/**
 * Prototype for avl comparators
 * @param k1 first key
 * @param k2 second key
 * @param ptr custom data for tree comparator
 * @return +1 if k1>k2, -1 if k1<k2, 0 if k1==k2
 */
typedef int (*avl_tree_comp) (const void *k1, const void *k2, void *ptr);

/**
 * This struct is the central management part of an avl tree.
 * One of them is necessary for each avl_tree.
 */
struct avl_tree {
  /**
   * Head of linked list node for supporting easy iteration
   * and multiple elments with the same key.
   */
  struct list_entity list_head;

  /**
   * pointer to the root node of the avl tree, NULL if tree is empty
   */
  struct avl_node *root;

  /**
   * number of nodes in the avl tree
   */
  unsigned int count;

  /**
   * true if multiple nodes with the same key are
   * allowed in the tree, false otherwise
   */
  bool allow_dups;

  /**
   * pointer to the tree comparator
   *
   * First two parameters are keys to compare,
   * third parameter is a copy of cmp_ptr
   */
  avl_tree_comp comp;

  /**
   * custom pointer delivered to the tree comparator
   */
  void *cmp_ptr;
};

/**
 * internal enum for avl_find_... macros
 */
enum avl_find_mode {
  AVL_FIND_EQUAL,
  AVL_FIND_LESSEQUAL,
  AVL_FIND_GREATEREQUAL
};

void EXPORT(avl_init)(struct avl_tree *, avl_tree_comp, bool, void *);
struct avl_node *EXPORT(avl_find)(const struct avl_tree *, const void *);
struct avl_node *EXPORT(avl_find_greaterequal)(const struct avl_tree *tree, const void *key);
struct avl_node *EXPORT(avl_find_lessequal)(const struct avl_tree *tree, const void *key);
int EXPORT(avl_insert)(struct avl_tree *, struct avl_node *);
void EXPORT(avl_delete)(struct avl_tree *, struct avl_node *);

/**
 * @param tree pointer to avl-tree
 * @param node pointer to node of the tree
 * @return true if node is the first one of the tree, false otherwise
 */
static inline bool
avl_is_first(struct avl_tree *tree, struct avl_node *node) {
  return tree->list_head.next == &node->list;
}

/**
 * @param tree pointer to avl-tree
 * @param node pointer to node of the tree
 * @return true if node is the last one of the tree, false otherwise
 */
static inline bool
avl_is_last(struct avl_tree *tree, struct avl_node *node) {
  return tree->list_head.prev == &node->list;
}

/**
 * @param tree pointer to avl-tree
 * @return true if the tree is empty, false otherwise
 */
static inline bool
avl_is_empty(struct avl_tree *tree) {
  return tree->count == 0;
}

/**
 * Internal function to support returning the element from a avl tree query
 * @param tree pointer to avl tree
 * @param key pointer to key
 * @param offset offset of node inside the embedded struct
 * @param mode mode of lookup operation (less equal, equal or greater equal)
 * @param pointer to elemen, NULL if no fitting one was found
 */
static inline void *
__avl_find_element(const struct avl_tree *tree, const void *key, size_t offset, enum avl_find_mode mode) {
  void *node = NULL;

  switch (mode) {
    case AVL_FIND_EQUAL:
      node = avl_find(tree, key);
      break;
    case AVL_FIND_LESSEQUAL:
      node = avl_find_lessequal(tree, key);
      break;
    case AVL_FIND_GREATEREQUAL:
      node = avl_find_greaterequal(tree, key);
      break;
  }
  return node == NULL ? NULL : (((char *)node) - offset);
}

/**
 * @param tree pointer to avl-tree
 * @param key pointer to key
 * @param element pointer to a node element
 *    (don't need to be initialized)
 * @param node_element name of the avl_node element inside the
 *    larger struct
 * @return pointer to tree element with the specified key,
 *    NULL if no element was found
 */
#define avl_find_element(tree, key, element, node_element) \
  ((typeof(*(element)) *)__avl_find_element(tree, key, offsetof(typeof(*(element)), node_element), AVL_FIND_EQUAL))

/**
 * @param tree pointer to avl-tree
 * @param key pointer to specified key
 * @param element pointer to a node element
 *    (don't need to be initialized)
 * @param node_element name of the avl_node element inside the
 *    larger struct
 * return pointer to last tree element with less or equal key than specified key,
 *    NULL if no element was found
 */
#define avl_find_le_element(tree, key, element, node_element) \
  ((typeof(*(element)) *)__avl_find_element(tree, key, offsetof(typeof(*(element)), node_element), AVL_FIND_LESSEQUAL))

/**
 * @param tree pointer to avl-tree
 * @param key pointer to specified key
 * @param element pointer to a node element
 *    (don't need to be initialized)
 * @param node_element name of the avl_node element inside the
 *    larger struct
 * return pointer to first tree element with greater or equal key than specified key,
 *    NULL if no element was found
 */
#define avl_find_ge_element(tree, key, element, node_element) \
  ((typeof(*(element)) *)__avl_find_element(tree, key, offsetof(typeof(*(element)), node_element), AVL_FIND_GREATEREQUAL))

/**
 * This function must not be called for an empty tree
 *
 * @param tree pointer to avl-tree
 * @param element pointer to a node element
 *    (don't need to be initialized)
 * @param node_member name of the avl_node element inside the
 *    larger struct
 * @return pointer to the first element of the avl_tree
 *    (automatically converted to type 'element')
 */
#define avl_first_element(tree, element, node_member) \
  container_of((tree)->list_head.next, typeof(*(element)), node_member.list)

/**
 * @param tree pointer to tree
 * @param element pointer to a node struct that contains the avl_node
 *    (don't need to be initialized)
 * @param node_member name of the avl_node element inside the
 *    larger struct
 * @return pointer to the last element of the avl_tree
 *    (automatically converted to type 'element')
 */
#define avl_last_element(tree, element, node_member) \
  container_of((tree)->list_head.prev, typeof(*(element)), node_member.list)

/**
 * This function must not be called for the last element of
 * an avl tree
 *
 * @param element pointer to a node of the tree
 * @param node_member name of the avl_node element inside the
 *    larger struct
 * @return pointer to the node after 'element'
 *    (automatically converted to type 'element')
 */
#define avl_next_element(element, node_member) \
  container_of((&(element)->node_member.list)->next, typeof(*(element)), node_member.list)

/**
 * This function must not be called for the first element of
 * an avl tree
 *
 * @param element pointer to a node of the tree
 * @param node_member name of the avl_node element inside the
 *    larger struct
 * @return pointer to the node before 'element'
 *    (automatically converted to type 'element')
 */
#define avl_prev_element(element, node_member) \
  container_of((&(element)->node_member.list)->prev, typeof(*(element)), node_member.list)

/**
 * Loop over a block of elements of a tree, used similar to a for() command.
 * This loop should not be used if elements are removed from the tree during
 * the loop.
 *
 * @param first pointer to first element of loop
 * @param last pointer to last element of loop
 * @param element pointer to a node of the tree, this element will
 *    contain the current node of the list during the loop
 * @param node_member name of the avl_node element inside the
 *    larger struct
 */
#define avl_for_element_range(first, last, element, node_member) \
  for (element = (first); \
       element->node_member.list.prev != &(last)->node_member.list; \
       element = avl_next_element(element, node_member))

/**
 * Loop over a block of elements of a tree backwards, used similar to a for() command.
 * This loop should not be used if elements are removed from the tree during
 * the loop.
 *
 * @param first pointer to first element of loop
 * @param last pointer to last element of loop
 * @param element pointer to a node of the tree, this element will
 *    contain the current node of the list during the loop
 * @param node_member name of the avl_node element inside the
 *    larger struct
 */
#define avl_for_element_range_reverse(first, last, element, node_member) \
  for (element = (last); \
       element->node_member.list.next != &(first)->node_member.list; \
       element = avl_prev_element(element, node_member))

/**
 * Loop over all elements of an avl_tree, used similar to a for() command.
 * This loop should not be used if elements are removed from the tree during
 * the loop.
 *
 * @param tree pointer to avl-tree
 * @param element pointer to a node of the tree, this element will
 *    contain the current node of the tree during the loop
 * @param node_member name of the avl_node element inside the
 *    larger struct
 */
#define avl_for_each_element(tree, element, node_member) \
  avl_for_element_range(avl_first_element(tree, element, node_member), \
                        avl_last_element(tree, element,  node_member), \
                        element, node_member)

/**
 * Loop over all elements of an avl_tree backwards, used similar to a for() command.
 * This loop should not be used if elements are removed from the tree during
 * the loop.
 *
 * @param tree pointer to avl-tree
 * @param element pointer to a node of the tree, this element will
 *    contain the current node of the tree during the loop
 * @param node_member name of the avl_node element inside the
 *    larger struct
 */
#define avl_for_each_element_reverse(tree, element, node_member) \
  avl_for_element_range_reverse(avl_first_element(tree, element, node_member), \
                                avl_last_element(tree, element,  node_member), \
                                element, node_member)

/**
 * Loop over a block of elements of a tree, used similar to a for() command.
 * This loop should not be used if elements are removed from the tree during
 * the loop.
 * The loop runs from the element 'first' to the end of the tree.
 *
 * @param tree pointer to avl-tree
 * @param first pointer to first element of loop
 * @param element pointer to a node of the tree, this element will
 *    contain the current node of the list during the loop
 * @param node_member name of the avl_node element inside the
 *    larger struct
 */
#define avl_for_element_to_last(tree, first, element, node_member) \
  avl_for_element_range(first, avl_last_element(tree, element, node_member), element, node_member)


/**
 * Loop over a block of elements of a tree backwards, used similar to a for() command.
 * This loop should not be used if elements are removed from the tree during
 * the loop.
 * The loop runs from the element 'first' to the end of the tree.
 *
 * @param tree pointer to avl-tree
 * @param first pointer to first element of loop
 * @param element pointer to a node of the tree, this element will
 *    contain the current node of the list during the loop
 * @param node_member name of the avl_node element inside the
 *    larger struct
 */
#define avl_for_element_to_last_reverse(tree, first, element, node_member) \
  avl_for_element_range_reverse(first, avl_last_element(tree, element, node_member), element, node_member)

/**
 * Loop over a block of elements of a tree, used similar to a for() command.
 * This loop should not be used if elements are removed from the tree during
 * the loop.
 * The loop runs from the start of the tree to the element 'last'.
 *
 * @param tree pointer to avl-tree
 * @param last pointer to last element of loop
 * @param element pointer to a node of the tree, this element will
 *    contain the current node of the list during the loop
 * @param node_member name of the avl_node element inside the
 *    larger struct
 */
#define avl_for_first_to_element(tree, last, element, node_member) \
  avl_for_element_range(avl_first_element(tree, element, node_member), last, element, node_member)


/**
 * Loop over a block of elements of a tree backwards, used similar to a for() command.
 * This loop should not be used if elements are removed from the tree during
 * the loop.
 * The loop runs from the start of the tree to the element 'last'.
 *
 * @param tree pointer to avl-tree
 * @param last pointer to last element of loop
 * @param element pointer to a node of the tree, this element will
 *    contain the current node of the list during the loop
 * @param node_member name of the avl_node element inside the
 *    larger struct
 */
#define avl_for_first_to_element_reverse(tree, last, element, node_member) \
  avl_for_element_range_reverse(avl_first_element(tree, element, node_member), last, element, node_member)

/**
 * Loop over a block of nodes of a tree, used similar to a for() command.
 * This loop can be used if the current element might be removed from
 * the tree during the loop. Other elements should not be removed during
 * the loop.
 *
 * @param first_element first element of loop
 * @param last_element last element of loop
 * @param element iterator pointer to tree element struct
 * @param node_member name of avl_node within tree element struct
 * @param ptr pointer to tree element struct which is used to store
 *    the next node during the loop
 */
#define avl_for_element_range_safe(first_element, last_element, element, node_member, ptr) \
  for (element = (first_element), ptr = avl_next_element(first_element, node_member); \
       element->node_member.list.prev != &(last_element)->node_member.list; \
       element = ptr, ptr = avl_next_element(ptr, node_member))

/**
 * Loop over a block of elements of a tree backwards, used similar to a for() command.
 * This loop can be used if the current element might be removed from
 * the tree during the loop. Other elements should not be removed during
 * the loop.
 *
 * @param first_element first element of range (will be last returned by the loop)
 * @param last_element last element of range (will be first returned by the loop)
 * @param element iterator pointer to node element struct
 * @param node_member name of avl_node within node element struct
 * @param ptr pointer to node element struct which is used to store
 *    the previous node during the loop
 */
#define avl_for_element_range_reverse_safe(first_element, last_element, element, node_member, ptr) \
  for (element = (last_element), ptr = avl_prev_element(last_element, node_member); \
       element->node_member.list.next != &(first_element)->node_member.list; \
       element = ptr, ptr = avl_prev_element(ptr, node_member))

/**
 * Loop over all elements of an avl_tree, used similar to a for() command.
 * This loop can be used if the current element might be removed from
 * the tree during the loop. Other elements should not be removed during
 * the loop.
 *
 * @param tree pointer to avl-tree
 * @param element pointer to a node of the tree, this element will
 *    contain the current node of the tree during the loop
 * @param node_member name of the avl_node element inside the
 *    larger struct
 * @param ptr pointer to a tree element which is used to store
 *    the next node during the loop
 */
#define avl_for_each_element_safe(tree, element, node_member, ptr) \
  avl_for_element_range_safe(avl_first_element(tree, element, node_member), \
                             avl_last_element(tree, element, node_member), \
                             element, node_member, ptr)

/**
 * Loop over all elements of an avl_tree backwards, used similar to a for() command.
 * This loop can be used if the current element might be removed from
 * the tree during the loop. Other elements should not be removed during
 * the loop.
 *
 * @param tree pointer to avl-tree
 * @param element pointer to a node of the tree, this element will
 *    contain the current node of the tree during the loop
 * @param node_member name of the avl_node element inside the
 *    larger struct
 * @param ptr pointer to a tree element which is used to store
 *    the next node during the loop
 */
#define avl_for_each_element_reverse_safe(tree, element, node_member, ptr) \
  avl_for_element_range_reverse_safe(avl_first_element(tree, element, node_member), \
                                     avl_last_element(tree, element, node_member), \
                                     element, node_member, ptr)

/**
 * A special loop that removes all elements of the tree and cleans up the tree
 * root. The loop body is responsible to free the node elements of the tree.
 *
 * This loop is much faster than a normal one for clearing the tree because it
 * does not rebalance the tree after each removal. Do NOT use a break command
 * inside.
 * You can free the memory of the elements within the loop.
 * Do NOT call avl_delete() on the elements within the loop,
 *
 * @param tree pointer to avl-tree
 * @param element pointer to a node of the tree, this element will
 *    contain the current node of the tree during the loop
 * @param node_member name of the avl_node element inside the
 *    larger struct
 * @param ptr pointer to a tree element which is used to store
 *    the next node during the loop
 */
#define avl_remove_all_elements(tree, element, node_member, ptr) \
  for (element = avl_first_element(tree, element, node_member), \
       ptr = avl_next_element(element, node_member), \
       list_init_head(&(tree)->list_head), \
       (tree)->root = NULL; \
       (tree)->count > 0; \
       element = ptr, ptr = avl_next_element(ptr, node_member), (tree)->count--)

#endif /* _AVL_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
