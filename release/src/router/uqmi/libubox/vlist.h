/*
 * Copyright (C) 2012 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __LIBUBOX_VLIST_H
#define __LIBUBOX_VLIST_H

#include "avl.h"

struct vlist_tree;
struct vlist_node;

typedef void (*vlist_update_cb)(struct vlist_tree *tree,
				struct vlist_node *node_new,
				struct vlist_node *node_old);

struct vlist_tree {
	struct avl_tree avl;

	vlist_update_cb update;
	bool keep_old;
	bool no_delete;

	int version;
};

struct vlist_node {
	struct avl_node avl;
	int version;
};

void vlist_init(struct vlist_tree *tree, avl_tree_comp cmp, vlist_update_cb update);

#define vlist_find(tree, name, element, node_member) \
	avl_find_element(&(tree)->avl, name, element, node_member.avl)

static inline void vlist_update(struct vlist_tree *tree)
{
	tree->version++;
}

void vlist_add(struct vlist_tree *tree, struct vlist_node *node, const void *key);
void vlist_delete(struct vlist_tree *tree, struct vlist_node *node);
void vlist_flush(struct vlist_tree *tree);
void vlist_flush_all(struct vlist_tree *tree);

#define vlist_for_each_element(tree, element, node_member) \
	avl_for_each_element(&(tree)->avl, element, node_member.avl)

#endif
