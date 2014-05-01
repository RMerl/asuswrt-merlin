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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#include "avl.h"
#include "list.h"

#define list_merge(_head, _list) list_merge(_list, _head)
#define list_is_last(_head, _list) list_is_last(_list, _head)
#define list_is_first(_head, _list) list_is_first(_list, _head)

/**
 * internal type save inline function to calculate the maximum of
 * to integers without macro implementation.
 *
 * @param x first parameter of maximum function
 * @param y second parameter of maximum function
 * @return largest integer of both parameters
 */
static inline int avl_max(int x, int y) {
  return x > y ? x : y;
}

/**
 * internal type save inline function to calculate the minimum of
 * to integers without macro implementation.
 *
 * @param x first parameter of minimum function
 * @param y second parameter of minimum function
 * @return smallest integer of both parameters
 */
static inline int avl_min(int x, int y) {
  return x < y ? x : y;
}

static struct avl_node *
avl_find_rec(struct avl_node *node, const void *key, avl_tree_comp comp, void *ptr, int *cmp_result);
static void avl_insert_before(struct avl_tree *tree, struct avl_node *pos_node, struct avl_node *node);
static void avl_insert_after(struct avl_tree *tree, struct avl_node *pos_node, struct avl_node *node);
static void post_insert(struct avl_tree *tree, struct avl_node *node);
static void avl_delete_worker(struct avl_tree *tree, struct avl_node *node);
static void avl_remove(struct avl_tree *tree, struct avl_node *node);

/**
 * Initialize a new avl_tree struct
 * @param tree pointer to avl-tree
 * @param comp pointer to comparator for the tree
 * @param allow_dups true if the tree allows multiple
 *   elements with the same
 * @param ptr custom parameter for comparator
 */
void
avl_init(struct avl_tree *tree, avl_tree_comp comp, bool allow_dups, void *ptr)
{
  list_init_head(&tree->list_head);
  tree->root = NULL;
  tree->count = 0;
  tree->comp = comp;
  tree->allow_dups = allow_dups;
  tree->cmp_ptr = ptr;
}

/**
 * Finds a node in an avl-tree with a certain key
 * @param tree pointer to avl-tree
 * @param key pointer to key
 * @return pointer to avl-node with key, NULL if no node with
 *    this key exists.
 */
struct avl_node *
avl_find(const struct avl_tree *tree, const void *key)
{
  struct avl_node *node;
  int diff;

  if (tree->root == NULL)
    return NULL;

  node = avl_find_rec(tree->root, key, tree->comp, tree->cmp_ptr, &diff);

  return diff == 0 ? node : NULL;
}

/**
 * Finds the last node in an avl-tree with a key less or equal
 * than the specified key
 * @param tree pointer to avl-tree
 * @param key pointer to specified key
 * @return pointer to avl-node, NULL if no node with
 *    key less or equal specified key exists.
 */
struct avl_node *
avl_find_lessequal(const struct avl_tree *tree, const void *key) {
  struct avl_node *node, *next;
  int diff;

  if (tree->root == NULL)
    return NULL;

  node = avl_find_rec(tree->root, key, tree->comp, tree->cmp_ptr, &diff);

  /* go left as long as key<node.key */
  while (diff < 0) {
    if (list_is_first(&tree->list_head, &node->list)) {
      return NULL;
    }

    node = (struct avl_node *)node->list.prev;
    diff = (*tree->comp) (key, node->key, tree->cmp_ptr);
  }

  /* go right as long as key>=next_node.key */
  next = node;
  while (diff >= 0) {
    node = next;
    if (list_is_last(&tree->list_head, &node->list)) {
      break;
    }

    next = (struct avl_node *)node->list.next;
    diff = (*tree->comp) (key, next->key, tree->cmp_ptr);
  }
  return node;
}

/**
 * Finds the first node in an avl-tree with a key greater or equal
 * than the specified key
 * @param tree pointer to avl-tree
 * @param key pointer to specified key
 * @return pointer to avl-node, NULL if no node with
 *    key greater or equal specified key exists.
 */
struct avl_node *
avl_find_greaterequal(const struct avl_tree *tree, const void *key) {
  struct avl_node *node, *next;
  int diff;

  if (tree->root == NULL)
    return NULL;

  node = avl_find_rec(tree->root, key, tree->comp, tree->cmp_ptr, &diff);

  /* go right as long as key>node.key */
  while (diff > 0) {
    if (list_is_last(&tree->list_head, &node->list)) {
      return NULL;
    }

    node = (struct avl_node *)node->list.next;
    diff = (*tree->comp) (key, node->key, tree->cmp_ptr);
  }

  /* go left as long as key<=next_node.key */
  next = node;
  while (diff <= 0) {
    node = next;
    if (list_is_first(&tree->list_head, &node->list)) {
      break;
    }

    next = (struct avl_node *)node->list.prev;
    diff = (*tree->comp) (key, next->key, tree->cmp_ptr);
  }
  return node;
}

/**
 * Inserts an avl_node into a tree
 * @param tree pointer to tree
 * @param new pointer to node
 * @return 0 if node was inserted successfully, -1 if it was not inserted
 *   because of a key collision
 */
int
avl_insert(struct avl_tree *tree, struct avl_node *new)
{
  struct avl_node *node, *next, *last;
  int diff;

  new->parent = NULL;

  new->left = NULL;
  new->right = NULL;

  new->balance = 0;
  new->leader = true;

  if (tree->root == NULL) {
    list_add_head(&tree->list_head, &new->list);
    tree->root = new;
    tree->count = 1;
    return 0;
  }

  node = avl_find_rec(tree->root, new->key, tree->comp, tree->cmp_ptr, &diff);

  last = node;

  while (!list_is_last(&tree->list_head, &last->list)) {
    next = list_next_element(last, list);
    if (next->leader) {
      break;
    }
    last = next;
  }

  diff = (*tree->comp) (new->key, node->key, tree->cmp_ptr);

  if (diff == 0) {
    if (!tree->allow_dups)
      return -1;

    new->leader = 0;

    avl_insert_after(tree, last, new);
    return 0;
  }

  if (node->balance == 1) {
    avl_insert_before(tree, node, new);

    node->balance = 0;
    new->parent = node;
    node->left = new;
    return 0;
  }

  if (node->balance == -1) {
    avl_insert_after(tree, last, new);

    node->balance = 0;
    new->parent = node;
    node->right = new;
    return 0;
  }

  if (diff < 0) {
    avl_insert_before(tree, node, new);

    node->balance = -1;
    new->parent = node;
    node->left = new;
    post_insert(tree, node);
    return 0;
  }

  avl_insert_after(tree, last, new);

  node->balance = 1;
  new->parent = node;
  node->right = new;
  post_insert(tree, node);
  return 0;
}

/**
 * Remove a node from an avl tree
 * @param tree pointer to tree
 * @param node pointer to node
 */
void
avl_delete(struct avl_tree *tree, struct avl_node *node)
{
  struct avl_node *next;
  struct avl_node *parent;
  struct avl_node *left;
  struct avl_node *right;
  if (node->leader) {
    if (tree->allow_dups
        && !list_is_last(&tree->list_head, &node->list)
        && !(next = list_next_element(node, list))->leader) {
      next->leader = true;
      next->balance = node->balance;

      parent = node->parent;
      left = node->left;
      right = node->right;

      next->parent = parent;
      next->left = left;
      next->right = right;

      if (parent == NULL)
        tree->root = next;

      else {
        if (node == parent->left)
          parent->left = next;

        else
          parent->right = next;
      }

      if (left != NULL)
        left->parent = next;

      if (right != NULL)
        right->parent = next;
    }

    else
      avl_delete_worker(tree, node);
  }

  avl_remove(tree, node);
}

static struct avl_node *
avl_find_rec(struct avl_node *node, const void *key, avl_tree_comp comp, void *cmp_ptr, int *cmp_result)
{
  int diff;

  diff = (*comp) (key, node->key, cmp_ptr);
  *cmp_result = diff;

  if (diff < 0) {
    if (node->left != NULL)
      return avl_find_rec(node->left, key, comp, cmp_ptr, cmp_result);

    return node;
  }

  if (diff > 0) {
    if (node->right != NULL)
      return avl_find_rec(node->right, key, comp, cmp_ptr, cmp_result);

    return node;
  }

  return node;
}

static void
avl_rotate_right(struct avl_tree *tree, struct avl_node *node)
{
  struct avl_node *left, *parent;

  left = node->left;
  parent = node->parent;

  left->parent = parent;
  node->parent = left;

  if (parent == NULL)
    tree->root = left;

  else {
    if (parent->left == node)
      parent->left = left;

    else
      parent->right = left;
  }

  node->left = left->right;
  left->right = node;

  if (node->left != NULL)
    node->left->parent = node;

  node->balance += 1 - avl_min(left->balance, 0);
  left->balance += 1 + avl_max(node->balance, 0);
}

static void
avl_rotate_left(struct avl_tree *tree, struct avl_node *node)
{
  struct avl_node *right, *parent;

  right = node->right;
  parent = node->parent;

  right->parent = parent;
  node->parent = right;

  if (parent == NULL)
    tree->root = right;

  else {
    if (parent->left == node)
      parent->left = right;

    else
      parent->right = right;
  }

  node->right = right->left;
  right->left = node;

  if (node->right != NULL)
    node->right->parent = node;

  node->balance -= 1 + avl_max(right->balance, 0);
  right->balance -= 1 - avl_min(node->balance, 0);
}

static void
post_insert(struct avl_tree *tree, struct avl_node *node)
{
  struct avl_node *parent = node->parent;

  if (parent == NULL)
    return;

  if (node == parent->left) {
    parent->balance--;

    if (parent->balance == 0)
      return;

    if (parent->balance == -1) {
      post_insert(tree, parent);
      return;
    }

    if (node->balance == -1) {
      avl_rotate_right(tree, parent);
      return;
    }

    avl_rotate_left(tree, node);
    avl_rotate_right(tree, node->parent->parent);
    return;
  }

  parent->balance++;

  if (parent->balance == 0)
    return;

  if (parent->balance == 1) {
    post_insert(tree, parent);
    return;
  }

  if (node->balance == 1) {
    avl_rotate_left(tree, parent);
    return;
  }

  avl_rotate_right(tree, node);
  avl_rotate_left(tree, node->parent->parent);
}

static void
avl_insert_before(struct avl_tree *tree, struct avl_node *pos_node, struct avl_node *node)
{
  list_add_before(&pos_node->list, &node->list);
  tree->count++;
}

static void
avl_insert_after(struct avl_tree *tree, struct avl_node *pos_node, struct avl_node *node)
{
  list_add_after(&pos_node->list, &node->list);
  tree->count++;
}

static void
avl_remove(struct avl_tree *tree, struct avl_node *node)
{
  list_remove(&node->list);
  tree->count--;
}

static void
avl_post_delete(struct avl_tree *tree, struct avl_node *node)
{
  struct avl_node *parent;

  if ((parent = node->parent) == NULL)
    return;

  if (node == parent->left) {
    parent->balance++;

    if (parent->balance == 0) {
      avl_post_delete(tree, parent);
      return;
    }

    if (parent->balance == 1)
      return;

    if (parent->right->balance == 0) {
      avl_rotate_left(tree, parent);
      return;
    }

    if (parent->right->balance == 1) {
      avl_rotate_left(tree, parent);
      avl_post_delete(tree, parent->parent);
      return;
    }

    avl_rotate_right(tree, parent->right);
    avl_rotate_left(tree, parent);
    avl_post_delete(tree, parent->parent);
    return;
  }

  parent->balance--;

  if (parent->balance == 0) {
    avl_post_delete(tree, parent);
    return;
  }

  if (parent->balance == -1)
    return;

  if (parent->left->balance == 0) {
    avl_rotate_right(tree, parent);
    return;
  }

  if (parent->left->balance == -1) {
    avl_rotate_right(tree, parent);
    avl_post_delete(tree, parent->parent);
    return;
  }

  avl_rotate_left(tree, parent->left);
  avl_rotate_right(tree, parent);
  avl_post_delete(tree, parent->parent);
}

static struct avl_node *
avl_local_min(struct avl_node *node)
{
  while (node->left != NULL)
    node = node->left;

  return node;
}

#if 0
static struct avl_node *
avl_local_max(struct avl_node *node)
{
  while (node->right != NULL)
    node = node->right;

  return node;
}
#endif

static void
avl_delete_worker(struct avl_tree *tree, struct avl_node *node)
{
  struct avl_node *parent, *min;

  parent = node->parent;

  if (node->left == NULL && node->right == NULL) {
    if (parent == NULL) {
      tree->root = NULL;
      return;
    }

    if (parent->left == node) {
      parent->left = NULL;
      parent->balance++;

      if (parent->balance == 1)
        return;

      if (parent->balance == 0) {
        avl_post_delete(tree, parent);
        return;
      }

      if (parent->right->balance == 0) {
        avl_rotate_left(tree, parent);
        return;
      }

      if (parent->right->balance == 1) {
        avl_rotate_left(tree, parent);
        avl_post_delete(tree, parent->parent);
        return;
      }

      avl_rotate_right(tree, parent->right);
      avl_rotate_left(tree, parent);
      avl_post_delete(tree, parent->parent);
      return;
    }

    if (parent->right == node) {
      parent->right = NULL;
      parent->balance--;

      if (parent->balance == -1)
        return;

      if (parent->balance == 0) {
        avl_post_delete(tree, parent);
        return;
      }

      if (parent->left->balance == 0) {
        avl_rotate_right(tree, parent);
        return;
      }

      if (parent->left->balance == -1) {
        avl_rotate_right(tree, parent);
        avl_post_delete(tree, parent->parent);
        return;
      }

      avl_rotate_left(tree, parent->left);
      avl_rotate_right(tree, parent);
      avl_post_delete(tree, parent->parent);
      return;
    }
  }

  if (node->left == NULL) {
    if (parent == NULL) {
      tree->root = node->right;
      node->right->parent = NULL;
      return;
    }

    node->right->parent = parent;

    if (parent->left == node)
      parent->left = node->right;

    else
      parent->right = node->right;

    avl_post_delete(tree, node->right);
    return;
  }

  if (node->right == NULL) {
    if (parent == NULL) {
      tree->root = node->left;
      node->left->parent = NULL;
      return;
    }

    node->left->parent = parent;

    if (parent->left == node)
      parent->left = node->left;

    else
      parent->right = node->left;

    avl_post_delete(tree, node->left);
    return;
  }

  min = avl_local_min(node->right);
  avl_delete_worker(tree, min);
  parent = node->parent;

  min->balance = node->balance;
  min->parent = parent;
  min->left = node->left;
  min->right = node->right;

  if (min->left != NULL)
    min->left->parent = min;

  if (min->right != NULL)
    min->right->parent = min;

  if (parent == NULL) {
    tree->root = min;
    return;
  }

  if (parent->left == node) {
    parent->left = min;
    return;
  }

  parent->right = min;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
