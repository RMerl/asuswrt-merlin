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
#include "vlist.h"

void
vlist_init(struct vlist_tree *tree, avl_tree_comp cmp, vlist_update_cb update)
{
	tree->update = update;
	tree->version = 1;

	avl_init(&tree->avl, cmp, 0, tree);
}

void
vlist_delete(struct vlist_tree *tree, struct vlist_node *node)
{
	if (!tree->no_delete)
		avl_delete(&tree->avl, &node->avl);
	tree->update(tree, NULL, node);
}

void
vlist_add(struct vlist_tree *tree, struct vlist_node *node, const void *key)
{
	struct vlist_node *old_node = NULL;
	struct avl_node *anode;

	node->avl.key = key;
	node->version = tree->version;

	anode = avl_find(&tree->avl, key);
	if (anode) {
		old_node = container_of(anode, struct vlist_node, avl);
		if (tree->keep_old || tree->no_delete) {
			old_node->version = tree->version;
			goto update_only;
		}

		avl_delete(&tree->avl, anode);
	}

	avl_insert(&tree->avl, &node->avl);

update_only:
	tree->update(tree, node, old_node);
}

void
vlist_flush(struct vlist_tree *tree)
{
	struct vlist_node *node, *tmp;

	avl_for_each_element_safe(&tree->avl, node, avl, tmp) {
		if ((node->version == tree->version || node->version == -1) &&
		    tree->version != -1)
			continue;

		vlist_delete(tree, node);
	}
}

void
vlist_flush_all(struct vlist_tree *tree)
{
	tree->version = -1;
	vlist_flush(tree);
}


