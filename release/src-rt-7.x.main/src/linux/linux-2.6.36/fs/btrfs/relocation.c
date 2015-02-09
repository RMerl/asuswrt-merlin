/*
 * Copyright (C) 2009 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include <linux/sched.h>
#include <linux/pagemap.h>
#include <linux/writeback.h>
#include <linux/blkdev.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include "ctree.h"
#include "disk-io.h"
#include "transaction.h"
#include "volumes.h"
#include "locking.h"
#include "btrfs_inode.h"
#include "async-thread.h"

/*
 * backref_node, mapping_node and tree_block start with this
 */
struct tree_entry {
	struct rb_node rb_node;
	u64 bytenr;
};

/*
 * present a tree block in the backref cache
 */
struct backref_node {
	struct rb_node rb_node;
	u64 bytenr;

	u64 new_bytenr;
	/* objectid of tree block owner, can be not uptodate */
	u64 owner;
	/* link to pending, changed or detached list */
	struct list_head list;
	/* list of upper level blocks reference this block */
	struct list_head upper;
	/* list of child blocks in the cache */
	struct list_head lower;
	/* NULL if this node is not tree root */
	struct btrfs_root *root;
	/* extent buffer got by COW the block */
	struct extent_buffer *eb;
	/* level of tree block */
	unsigned int level:8;
	/* is the block in non-reference counted tree */
	unsigned int cowonly:1;
	/* 1 if no child node in the cache */
	unsigned int lowest:1;
	/* is the extent buffer locked */
	unsigned int locked:1;
	/* has the block been processed */
	unsigned int processed:1;
	/* have backrefs of this block been checked */
	unsigned int checked:1;
	/*
	 * 1 if corresponding block has been cowed but some upper
	 * level block pointers may not point to the new location
	 */
	unsigned int pending:1;
	/*
	 * 1 if the backref node isn't connected to any other
	 * backref node.
	 */
	unsigned int detached:1;
};

/*
 * present a block pointer in the backref cache
 */
struct backref_edge {
	struct list_head list[2];
	struct backref_node *node[2];
};

#define LOWER	0
#define UPPER	1

struct backref_cache {
	/* red black tree of all backref nodes in the cache */
	struct rb_root rb_root;
	/* for passing backref nodes to btrfs_reloc_cow_block */
	struct backref_node *path[BTRFS_MAX_LEVEL];
	/*
	 * list of blocks that have been cowed but some block
	 * pointers in upper level blocks may not reflect the
	 * new location
	 */
	struct list_head pending[BTRFS_MAX_LEVEL];
	/* list of backref nodes with no child node */
	struct list_head leaves;
	/* list of blocks that have been cowed in current transaction */
	struct list_head changed;
	/* list of detached backref node. */
	struct list_head detached;

	u64 last_trans;

	int nr_nodes;
	int nr_edges;
};

/*
 * map address of tree root to tree
 */
struct mapping_node {
	struct rb_node rb_node;
	u64 bytenr;
	void *data;
};

struct mapping_tree {
	struct rb_root rb_root;
	spinlock_t lock;
};

/*
 * present a tree block to process
 */
struct tree_block {
	struct rb_node rb_node;
	u64 bytenr;
	struct btrfs_key key;
	unsigned int level:8;
	unsigned int key_ready:1;
};

#define MAX_EXTENTS 128

struct file_extent_cluster {
	u64 start;
	u64 end;
	u64 boundary[MAX_EXTENTS];
	unsigned int nr;
};

struct reloc_control {
	/* block group to relocate */
	struct btrfs_block_group_cache *block_group;
	/* extent tree */
	struct btrfs_root *extent_root;
	/* inode for moving data */
	struct inode *data_inode;

	struct btrfs_block_rsv *block_rsv;

	struct backref_cache backref_cache;

	struct file_extent_cluster cluster;
	/* tree blocks have been processed */
	struct extent_io_tree processed_blocks;
	/* map start of tree root to corresponding reloc tree */
	struct mapping_tree reloc_root_tree;
	/* list of reloc trees */
	struct list_head reloc_roots;
	/* size of metadata reservation for merging reloc trees */
	u64 merging_rsv_size;
	/* size of relocated tree nodes */
	u64 nodes_relocated;

	u64 search_start;
	u64 extents_found;

	int block_rsv_retries;

	unsigned int stage:8;
	unsigned int create_reloc_tree:1;
	unsigned int merge_reloc_tree:1;
	unsigned int found_file_extent:1;
	unsigned int commit_transaction:1;
};

/* stages of data relocation */
#define MOVE_DATA_EXTENTS	0
#define UPDATE_DATA_PTRS	1

static void remove_backref_node(struct backref_cache *cache,
				struct backref_node *node);
static void __mark_block_processed(struct reloc_control *rc,
				   struct backref_node *node);

static void mapping_tree_init(struct mapping_tree *tree)
{
	tree->rb_root = RB_ROOT;
	spin_lock_init(&tree->lock);
}

static void backref_cache_init(struct backref_cache *cache)
{
	int i;
	cache->rb_root = RB_ROOT;
	for (i = 0; i < BTRFS_MAX_LEVEL; i++)
		INIT_LIST_HEAD(&cache->pending[i]);
	INIT_LIST_HEAD(&cache->changed);
	INIT_LIST_HEAD(&cache->detached);
	INIT_LIST_HEAD(&cache->leaves);
}

static void backref_cache_cleanup(struct backref_cache *cache)
{
	struct backref_node *node;
	int i;

	while (!list_empty(&cache->detached)) {
		node = list_entry(cache->detached.next,
				  struct backref_node, list);
		remove_backref_node(cache, node);
	}

	while (!list_empty(&cache->leaves)) {
		node = list_entry(cache->leaves.next,
				  struct backref_node, lower);
		remove_backref_node(cache, node);
	}

	cache->last_trans = 0;

	for (i = 0; i < BTRFS_MAX_LEVEL; i++)
		BUG_ON(!list_empty(&cache->pending[i]));
	BUG_ON(!list_empty(&cache->changed));
	BUG_ON(!list_empty(&cache->detached));
	BUG_ON(!RB_EMPTY_ROOT(&cache->rb_root));
	BUG_ON(cache->nr_nodes);
	BUG_ON(cache->nr_edges);
}

static struct backref_node *alloc_backref_node(struct backref_cache *cache)
{
	struct backref_node *node;

	node = kzalloc(sizeof(*node), GFP_NOFS);
	if (node) {
		INIT_LIST_HEAD(&node->list);
		INIT_LIST_HEAD(&node->upper);
		INIT_LIST_HEAD(&node->lower);
		RB_CLEAR_NODE(&node->rb_node);
		cache->nr_nodes++;
	}
	return node;
}

static void free_backref_node(struct backref_cache *cache,
			      struct backref_node *node)
{
	if (node) {
		cache->nr_nodes--;
		kfree(node);
	}
}

static struct backref_edge *alloc_backref_edge(struct backref_cache *cache)
{
	struct backref_edge *edge;

	edge = kzalloc(sizeof(*edge), GFP_NOFS);
	if (edge)
		cache->nr_edges++;
	return edge;
}

static void free_backref_edge(struct backref_cache *cache,
			      struct backref_edge *edge)
{
	if (edge) {
		cache->nr_edges--;
		kfree(edge);
	}
}

static struct rb_node *tree_insert(struct rb_root *root, u64 bytenr,
				   struct rb_node *node)
{
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	struct tree_entry *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct tree_entry, rb_node);

		if (bytenr < entry->bytenr)
			p = &(*p)->rb_left;
		else if (bytenr > entry->bytenr)
			p = &(*p)->rb_right;
		else
			return parent;
	}

	rb_link_node(node, parent, p);
	rb_insert_color(node, root);
	return NULL;
}

static struct rb_node *tree_search(struct rb_root *root, u64 bytenr)
{
	struct rb_node *n = root->rb_node;
	struct tree_entry *entry;

	while (n) {
		entry = rb_entry(n, struct tree_entry, rb_node);

		if (bytenr < entry->bytenr)
			n = n->rb_left;
		else if (bytenr > entry->bytenr)
			n = n->rb_right;
		else
			return n;
	}
	return NULL;
}

/*
 * walk up backref nodes until reach node presents tree root
 */
static struct backref_node *walk_up_backref(struct backref_node *node,
					    struct backref_edge *edges[],
					    int *index)
{
	struct backref_edge *edge;
	int idx = *index;

	while (!list_empty(&node->upper)) {
		edge = list_entry(node->upper.next,
				  struct backref_edge, list[LOWER]);
		edges[idx++] = edge;
		node = edge->node[UPPER];
	}
	BUG_ON(node->detached);
	*index = idx;
	return node;
}

/*
 * walk down backref nodes to find start of next reference path
 */
static struct backref_node *walk_down_backref(struct backref_edge *edges[],
					      int *index)
{
	struct backref_edge *edge;
	struct backref_node *lower;
	int idx = *index;

	while (idx > 0) {
		edge = edges[idx - 1];
		lower = edge->node[LOWER];
		if (list_is_last(&edge->list[LOWER], &lower->upper)) {
			idx--;
			continue;
		}
		edge = list_entry(edge->list[LOWER].next,
				  struct backref_edge, list[LOWER]);
		edges[idx - 1] = edge;
		*index = idx;
		return edge->node[UPPER];
	}
	*index = 0;
	return NULL;
}

static void unlock_node_buffer(struct backref_node *node)
{
	if (node->locked) {
		btrfs_tree_unlock(node->eb);
		node->locked = 0;
	}
}

static void drop_node_buffer(struct backref_node *node)
{
	if (node->eb) {
		unlock_node_buffer(node);
		free_extent_buffer(node->eb);
		node->eb = NULL;
	}
}

static void drop_backref_node(struct backref_cache *tree,
			      struct backref_node *node)
{
	BUG_ON(!list_empty(&node->upper));

	drop_node_buffer(node);
	list_del(&node->list);
	list_del(&node->lower);
	if (!RB_EMPTY_NODE(&node->rb_node))
		rb_erase(&node->rb_node, &tree->rb_root);
	free_backref_node(tree, node);
}

/*
 * remove a backref node from the backref cache
 */
static void remove_backref_node(struct backref_cache *cache,
				struct backref_node *node)
{
	struct backref_node *upper;
	struct backref_edge *edge;

	if (!node)
		return;

	BUG_ON(!node->lowest && !node->detached);
	while (!list_empty(&node->upper)) {
		edge = list_entry(node->upper.next, struct backref_edge,
				  list[LOWER]);
		upper = edge->node[UPPER];
		list_del(&edge->list[LOWER]);
		list_del(&edge->list[UPPER]);
		free_backref_edge(cache, edge);

		if (RB_EMPTY_NODE(&upper->rb_node)) {
			BUG_ON(!list_empty(&node->upper));
			drop_backref_node(cache, node);
			node = upper;
			node->lowest = 1;
			continue;
		}
		/*
		 * add the node to leaf node list if no other
		 * child block cached.
		 */
		if (list_empty(&upper->lower)) {
			list_add_tail(&upper->lower, &cache->leaves);
			upper->lowest = 1;
		}
	}

	drop_backref_node(cache, node);
}

static void update_backref_node(struct backref_cache *cache,
				struct backref_node *node, u64 bytenr)
{
	struct rb_node *rb_node;
	rb_erase(&node->rb_node, &cache->rb_root);
	node->bytenr = bytenr;
	rb_node = tree_insert(&cache->rb_root, node->bytenr, &node->rb_node);
	BUG_ON(rb_node);
}

/*
 * update backref cache after a transaction commit
 */
static int update_backref_cache(struct btrfs_trans_handle *trans,
				struct backref_cache *cache)
{
	struct backref_node *node;
	int level = 0;

	if (cache->last_trans == 0) {
		cache->last_trans = trans->transid;
		return 0;
	}

	if (cache->last_trans == trans->transid)
		return 0;

	/*
	 * detached nodes are used to avoid unnecessary backref
	 * lookup. transaction commit changes the extent tree.
	 * so the detached nodes are no longer useful.
	 */
	while (!list_empty(&cache->detached)) {
		node = list_entry(cache->detached.next,
				  struct backref_node, list);
		remove_backref_node(cache, node);
	}

	while (!list_empty(&cache->changed)) {
		node = list_entry(cache->changed.next,
				  struct backref_node, list);
		list_del_init(&node->list);
		BUG_ON(node->pending);
		update_backref_node(cache, node, node->new_bytenr);
	}

	/*
	 * some nodes can be left in the pending list if there were
	 * errors during processing the pending nodes.
	 */
	for (level = 0; level < BTRFS_MAX_LEVEL; level++) {
		list_for_each_entry(node, &cache->pending[level], list) {
			BUG_ON(!node->pending);
			if (node->bytenr == node->new_bytenr)
				continue;
			update_backref_node(cache, node, node->new_bytenr);
		}
	}

	cache->last_trans = 0;
	return 1;
}

static int should_ignore_root(struct btrfs_root *root)
{
	struct btrfs_root *reloc_root;

	if (!root->ref_cows)
		return 0;

	reloc_root = root->reloc_root;
	if (!reloc_root)
		return 0;

	if (btrfs_root_last_snapshot(&reloc_root->root_item) ==
	    root->fs_info->running_transaction->transid - 1)
		return 0;
	/*
	 * if there is reloc tree and it was created in previous
	 * transaction backref lookup can find the reloc tree,
	 * so backref node for the fs tree root is useless for
	 * relocation.
	 */
	return 1;
}

/*
 * find reloc tree by address of tree root
 */
static struct btrfs_root *find_reloc_root(struct reloc_control *rc,
					  u64 bytenr)
{
	struct rb_node *rb_node;
	struct mapping_node *node;
	struct btrfs_root *root = NULL;

	spin_lock(&rc->reloc_root_tree.lock);
	rb_node = tree_search(&rc->reloc_root_tree.rb_root, bytenr);
	if (rb_node) {
		node = rb_entry(rb_node, struct mapping_node, rb_node);
		root = (struct btrfs_root *)node->data;
	}
	spin_unlock(&rc->reloc_root_tree.lock);
	return root;
}

static int is_cowonly_root(u64 root_objectid)
{
	if (root_objectid == BTRFS_ROOT_TREE_OBJECTID ||
	    root_objectid == BTRFS_EXTENT_TREE_OBJECTID ||
	    root_objectid == BTRFS_CHUNK_TREE_OBJECTID ||
	    root_objectid == BTRFS_DEV_TREE_OBJECTID ||
	    root_objectid == BTRFS_TREE_LOG_OBJECTID ||
	    root_objectid == BTRFS_CSUM_TREE_OBJECTID)
		return 1;
	return 0;
}

static struct btrfs_root *read_fs_root(struct btrfs_fs_info *fs_info,
					u64 root_objectid)
{
	struct btrfs_key key;

	key.objectid = root_objectid;
	key.type = BTRFS_ROOT_ITEM_KEY;
	if (is_cowonly_root(root_objectid))
		key.offset = 0;
	else
		key.offset = (u64)-1;

	return btrfs_read_fs_root_no_name(fs_info, &key);
}

#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
static noinline_for_stack
struct btrfs_root *find_tree_root(struct reloc_control *rc,
				  struct extent_buffer *leaf,
				  struct btrfs_extent_ref_v0 *ref0)
{
	struct btrfs_root *root;
	u64 root_objectid = btrfs_ref_root_v0(leaf, ref0);
	u64 generation = btrfs_ref_generation_v0(leaf, ref0);

	BUG_ON(root_objectid == BTRFS_TREE_RELOC_OBJECTID);

	root = read_fs_root(rc->extent_root->fs_info, root_objectid);
	BUG_ON(IS_ERR(root));

	if (root->ref_cows &&
	    generation != btrfs_root_generation(&root->root_item))
		return NULL;

	return root;
}
#endif

static noinline_for_stack
int find_inline_backref(struct extent_buffer *leaf, int slot,
			unsigned long *ptr, unsigned long *end)
{
	struct btrfs_extent_item *ei;
	struct btrfs_tree_block_info *bi;
	u32 item_size;

	item_size = btrfs_item_size_nr(leaf, slot);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	if (item_size < sizeof(*ei)) {
		WARN_ON(item_size != sizeof(struct btrfs_extent_item_v0));
		return 1;
	}
#endif
	ei = btrfs_item_ptr(leaf, slot, struct btrfs_extent_item);
	WARN_ON(!(btrfs_extent_flags(leaf, ei) &
		  BTRFS_EXTENT_FLAG_TREE_BLOCK));

	if (item_size <= sizeof(*ei) + sizeof(*bi)) {
		WARN_ON(item_size < sizeof(*ei) + sizeof(*bi));
		return 1;
	}

	bi = (struct btrfs_tree_block_info *)(ei + 1);
	*ptr = (unsigned long)(bi + 1);
	*end = (unsigned long)ei + item_size;
	return 0;
}

/*
 * build backref tree for a given tree block. root of the backref tree
 * corresponds the tree block, leaves of the backref tree correspond
 * roots of b-trees that reference the tree block.
 *
 * the basic idea of this function is check backrefs of a given block
 * to find upper level blocks that refernece the block, and then check
 * bakcrefs of these upper level blocks recursively. the recursion stop
 * when tree root is reached or backrefs for the block is cached.
 *
 * NOTE: if we find backrefs for a block are cached, we know backrefs
 * for all upper level blocks that directly/indirectly reference the
 * block are also cached.
 */
static noinline_for_stack
struct backref_node *build_backref_tree(struct reloc_control *rc,
					struct btrfs_key *node_key,
					int level, u64 bytenr)
{
	struct backref_cache *cache = &rc->backref_cache;
	struct btrfs_path *path1;
	struct btrfs_path *path2;
	struct extent_buffer *eb;
	struct btrfs_root *root;
	struct backref_node *cur;
	struct backref_node *upper;
	struct backref_node *lower;
	struct backref_node *node = NULL;
	struct backref_node *exist = NULL;
	struct backref_edge *edge;
	struct rb_node *rb_node;
	struct btrfs_key key;
	unsigned long end;
	unsigned long ptr;
	LIST_HEAD(list);
	LIST_HEAD(useless);
	int cowonly;
	int ret;
	int err = 0;

	path1 = btrfs_alloc_path();
	path2 = btrfs_alloc_path();
	if (!path1 || !path2) {
		err = -ENOMEM;
		goto out;
	}

	node = alloc_backref_node(cache);
	if (!node) {
		err = -ENOMEM;
		goto out;
	}

	node->bytenr = bytenr;
	node->level = level;
	node->lowest = 1;
	cur = node;
again:
	end = 0;
	ptr = 0;
	key.objectid = cur->bytenr;
	key.type = BTRFS_EXTENT_ITEM_KEY;
	key.offset = (u64)-1;

	path1->search_commit_root = 1;
	path1->skip_locking = 1;
	ret = btrfs_search_slot(NULL, rc->extent_root, &key, path1,
				0, 0);
	if (ret < 0) {
		err = ret;
		goto out;
	}
	BUG_ON(!ret || !path1->slots[0]);

	path1->slots[0]--;

	WARN_ON(cur->checked);
	if (!list_empty(&cur->upper)) {
		/*
		 * the backref was added previously when processsing
		 * backref of type BTRFS_TREE_BLOCK_REF_KEY
		 */
		BUG_ON(!list_is_singular(&cur->upper));
		edge = list_entry(cur->upper.next, struct backref_edge,
				  list[LOWER]);
		BUG_ON(!list_empty(&edge->list[UPPER]));
		exist = edge->node[UPPER];
		/*
		 * add the upper level block to pending list if we need
		 * check its backrefs
		 */
		if (!exist->checked)
			list_add_tail(&edge->list[UPPER], &list);
	} else {
		exist = NULL;
	}

	while (1) {
		cond_resched();
		eb = path1->nodes[0];

		if (ptr >= end) {
			if (path1->slots[0] >= btrfs_header_nritems(eb)) {
				ret = btrfs_next_leaf(rc->extent_root, path1);
				if (ret < 0) {
					err = ret;
					goto out;
				}
				if (ret > 0)
					break;
				eb = path1->nodes[0];
			}

			btrfs_item_key_to_cpu(eb, &key, path1->slots[0]);
			if (key.objectid != cur->bytenr) {
				WARN_ON(exist);
				break;
			}

			if (key.type == BTRFS_EXTENT_ITEM_KEY) {
				ret = find_inline_backref(eb, path1->slots[0],
							  &ptr, &end);
				if (ret)
					goto next;
			}
		}

		if (ptr < end) {
			/* update key for inline back ref */
			struct btrfs_extent_inline_ref *iref;
			iref = (struct btrfs_extent_inline_ref *)ptr;
			key.type = btrfs_extent_inline_ref_type(eb, iref);
			key.offset = btrfs_extent_inline_ref_offset(eb, iref);
			WARN_ON(key.type != BTRFS_TREE_BLOCK_REF_KEY &&
				key.type != BTRFS_SHARED_BLOCK_REF_KEY);
		}

		if (exist &&
		    ((key.type == BTRFS_TREE_BLOCK_REF_KEY &&
		      exist->owner == key.offset) ||
		     (key.type == BTRFS_SHARED_BLOCK_REF_KEY &&
		      exist->bytenr == key.offset))) {
			exist = NULL;
			goto next;
		}

#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
		if (key.type == BTRFS_SHARED_BLOCK_REF_KEY ||
		    key.type == BTRFS_EXTENT_REF_V0_KEY) {
			if (key.type == BTRFS_EXTENT_REF_V0_KEY) {
				struct btrfs_extent_ref_v0 *ref0;
				ref0 = btrfs_item_ptr(eb, path1->slots[0],
						struct btrfs_extent_ref_v0);
				if (key.objectid == key.offset) {
					root = find_tree_root(rc, eb, ref0);
					if (root && !should_ignore_root(root))
						cur->root = root;
					else
						list_add(&cur->list, &useless);
					break;
				}
				if (is_cowonly_root(btrfs_ref_root_v0(eb,
								      ref0)))
					cur->cowonly = 1;
			}
#else
		BUG_ON(key.type == BTRFS_EXTENT_REF_V0_KEY);
		if (key.type == BTRFS_SHARED_BLOCK_REF_KEY) {
#endif
			if (key.objectid == key.offset) {
				/*
				 * only root blocks of reloc trees use
				 * backref of this type.
				 */
				root = find_reloc_root(rc, cur->bytenr);
				BUG_ON(!root);
				cur->root = root;
				break;
			}

			edge = alloc_backref_edge(cache);
			if (!edge) {
				err = -ENOMEM;
				goto out;
			}
			rb_node = tree_search(&cache->rb_root, key.offset);
			if (!rb_node) {
				upper = alloc_backref_node(cache);
				if (!upper) {
					free_backref_edge(cache, edge);
					err = -ENOMEM;
					goto out;
				}
				upper->bytenr = key.offset;
				upper->level = cur->level + 1;
				/*
				 *  backrefs for the upper level block isn't
				 *  cached, add the block to pending list
				 */
				list_add_tail(&edge->list[UPPER], &list);
			} else {
				upper = rb_entry(rb_node, struct backref_node,
						 rb_node);
				BUG_ON(!upper->checked);
				INIT_LIST_HEAD(&edge->list[UPPER]);
			}
			list_add_tail(&edge->list[LOWER], &cur->upper);
			edge->node[LOWER] = cur;
			edge->node[UPPER] = upper;

			goto next;
		} else if (key.type != BTRFS_TREE_BLOCK_REF_KEY) {
			goto next;
		}

		/* key.type == BTRFS_TREE_BLOCK_REF_KEY */
		root = read_fs_root(rc->extent_root->fs_info, key.offset);
		if (IS_ERR(root)) {
			err = PTR_ERR(root);
			goto out;
		}

		if (!root->ref_cows)
			cur->cowonly = 1;

		if (btrfs_root_level(&root->root_item) == cur->level) {
			/* tree root */
			BUG_ON(btrfs_root_bytenr(&root->root_item) !=
			       cur->bytenr);
			if (should_ignore_root(root))
				list_add(&cur->list, &useless);
			else
				cur->root = root;
			break;
		}

		level = cur->level + 1;

		/*
		 * searching the tree to find upper level blocks
		 * reference the block.
		 */
		path2->search_commit_root = 1;
		path2->skip_locking = 1;
		path2->lowest_level = level;
		ret = btrfs_search_slot(NULL, root, node_key, path2, 0, 0);
		path2->lowest_level = 0;
		if (ret < 0) {
			err = ret;
			goto out;
		}
		if (ret > 0 && path2->slots[level] > 0)
			path2->slots[level]--;

		eb = path2->nodes[level];
		WARN_ON(btrfs_node_blockptr(eb, path2->slots[level]) !=
			cur->bytenr);

		lower = cur;
		for (; level < BTRFS_MAX_LEVEL; level++) {
			if (!path2->nodes[level]) {
				BUG_ON(btrfs_root_bytenr(&root->root_item) !=
				       lower->bytenr);
				if (should_ignore_root(root))
					list_add(&lower->list, &useless);
				else
					lower->root = root;
				break;
			}

			edge = alloc_backref_edge(cache);
			if (!edge) {
				err = -ENOMEM;
				goto out;
			}

			eb = path2->nodes[level];
			rb_node = tree_search(&cache->rb_root, eb->start);
			if (!rb_node) {
				upper = alloc_backref_node(cache);
				if (!upper) {
					free_backref_edge(cache, edge);
					err = -ENOMEM;
					goto out;
				}
				upper->bytenr = eb->start;
				upper->owner = btrfs_header_owner(eb);
				upper->level = lower->level + 1;
				if (!root->ref_cows)
					upper->cowonly = 1;

				/*
				 * if we know the block isn't shared
				 * we can void checking its backrefs.
				 */
				if (btrfs_block_can_be_shared(root, eb))
					upper->checked = 0;
				else
					upper->checked = 1;

				/*
				 * add the block to pending list if we
				 * need check its backrefs. only block
				 * at 'cur->level + 1' is added to the
				 * tail of pending list. this guarantees
				 * we check backrefs from lower level
				 * blocks to upper level blocks.
				 */
				if (!upper->checked &&
				    level == cur->level + 1) {
					list_add_tail(&edge->list[UPPER],
						      &list);
				} else
					INIT_LIST_HEAD(&edge->list[UPPER]);
			} else {
				upper = rb_entry(rb_node, struct backref_node,
						 rb_node);
				BUG_ON(!upper->checked);
				INIT_LIST_HEAD(&edge->list[UPPER]);
				if (!upper->owner)
					upper->owner = btrfs_header_owner(eb);
			}
			list_add_tail(&edge->list[LOWER], &lower->upper);
			edge->node[LOWER] = lower;
			edge->node[UPPER] = upper;

			if (rb_node)
				break;
			lower = upper;
			upper = NULL;
		}
		btrfs_release_path(root, path2);
next:
		if (ptr < end) {
			ptr += btrfs_extent_inline_ref_size(key.type);
			if (ptr >= end) {
				WARN_ON(ptr > end);
				ptr = 0;
				end = 0;
			}
		}
		if (ptr >= end)
			path1->slots[0]++;
	}
	btrfs_release_path(rc->extent_root, path1);

	cur->checked = 1;
	WARN_ON(exist);

	/* the pending list isn't empty, take the first block to process */
	if (!list_empty(&list)) {
		edge = list_entry(list.next, struct backref_edge, list[UPPER]);
		list_del_init(&edge->list[UPPER]);
		cur = edge->node[UPPER];
		goto again;
	}

	/*
	 * everything goes well, connect backref nodes and insert backref nodes
	 * into the cache.
	 */
	BUG_ON(!node->checked);
	cowonly = node->cowonly;
	if (!cowonly) {
		rb_node = tree_insert(&cache->rb_root, node->bytenr,
				      &node->rb_node);
		BUG_ON(rb_node);
		list_add_tail(&node->lower, &cache->leaves);
	}

	list_for_each_entry(edge, &node->upper, list[LOWER])
		list_add_tail(&edge->list[UPPER], &list);

	while (!list_empty(&list)) {
		edge = list_entry(list.next, struct backref_edge, list[UPPER]);
		list_del_init(&edge->list[UPPER]);
		upper = edge->node[UPPER];
		if (upper->detached) {
			list_del(&edge->list[LOWER]);
			lower = edge->node[LOWER];
			free_backref_edge(cache, edge);
			if (list_empty(&lower->upper))
				list_add(&lower->list, &useless);
			continue;
		}

		if (!RB_EMPTY_NODE(&upper->rb_node)) {
			if (upper->lowest) {
				list_del_init(&upper->lower);
				upper->lowest = 0;
			}

			list_add_tail(&edge->list[UPPER], &upper->lower);
			continue;
		}

		BUG_ON(!upper->checked);
		BUG_ON(cowonly != upper->cowonly);
		if (!cowonly) {
			rb_node = tree_insert(&cache->rb_root, upper->bytenr,
					      &upper->rb_node);
			BUG_ON(rb_node);
		}

		list_add_tail(&edge->list[UPPER], &upper->lower);

		list_for_each_entry(edge, &upper->upper, list[LOWER])
			list_add_tail(&edge->list[UPPER], &list);
	}
	/*
	 * process useless backref nodes. backref nodes for tree leaves
	 * are deleted from the cache. backref nodes for upper level
	 * tree blocks are left in the cache to avoid unnecessary backref
	 * lookup.
	 */
	while (!list_empty(&useless)) {
		upper = list_entry(useless.next, struct backref_node, list);
		list_del_init(&upper->list);
		BUG_ON(!list_empty(&upper->upper));
		if (upper == node)
			node = NULL;
		if (upper->lowest) {
			list_del_init(&upper->lower);
			upper->lowest = 0;
		}
		while (!list_empty(&upper->lower)) {
			edge = list_entry(upper->lower.next,
					  struct backref_edge, list[UPPER]);
			list_del(&edge->list[UPPER]);
			list_del(&edge->list[LOWER]);
			lower = edge->node[LOWER];
			free_backref_edge(cache, edge);

			if (list_empty(&lower->upper))
				list_add(&lower->list, &useless);
		}
		__mark_block_processed(rc, upper);
		if (upper->level > 0) {
			list_add(&upper->list, &cache->detached);
			upper->detached = 1;
		} else {
			rb_erase(&upper->rb_node, &cache->rb_root);
			free_backref_node(cache, upper);
		}
	}
out:
	btrfs_free_path(path1);
	btrfs_free_path(path2);
	if (err) {
		while (!list_empty(&useless)) {
			lower = list_entry(useless.next,
					   struct backref_node, upper);
			list_del_init(&lower->upper);
		}
		upper = node;
		INIT_LIST_HEAD(&list);
		while (upper) {
			if (RB_EMPTY_NODE(&upper->rb_node)) {
				list_splice_tail(&upper->upper, &list);
				free_backref_node(cache, upper);
			}

			if (list_empty(&list))
				break;

			edge = list_entry(list.next, struct backref_edge,
					  list[LOWER]);
			list_del(&edge->list[LOWER]);
			upper = edge->node[UPPER];
			free_backref_edge(cache, edge);
		}
		return ERR_PTR(err);
	}
	BUG_ON(node && node->detached);
	return node;
}

/*
 * helper to add backref node for the newly created snapshot.
 * the backref node is created by cloning backref node that
 * corresponds to root of source tree
 */
static int clone_backref_node(struct btrfs_trans_handle *trans,
			      struct reloc_control *rc,
			      struct btrfs_root *src,
			      struct btrfs_root *dest)
{
	struct btrfs_root *reloc_root = src->reloc_root;
	struct backref_cache *cache = &rc->backref_cache;
	struct backref_node *node = NULL;
	struct backref_node *new_node;
	struct backref_edge *edge;
	struct backref_edge *new_edge;
	struct rb_node *rb_node;

	if (cache->last_trans > 0)
		update_backref_cache(trans, cache);

	rb_node = tree_search(&cache->rb_root, src->commit_root->start);
	if (rb_node) {
		node = rb_entry(rb_node, struct backref_node, rb_node);
		if (node->detached)
			node = NULL;
		else
			BUG_ON(node->new_bytenr != reloc_root->node->start);
	}

	if (!node) {
		rb_node = tree_search(&cache->rb_root,
				      reloc_root->commit_root->start);
		if (rb_node) {
			node = rb_entry(rb_node, struct backref_node,
					rb_node);
			BUG_ON(node->detached);
		}
	}

	if (!node)
		return 0;

	new_node = alloc_backref_node(cache);
	if (!new_node)
		return -ENOMEM;

	new_node->bytenr = dest->node->start;
	new_node->level = node->level;
	new_node->lowest = node->lowest;
	new_node->root = dest;

	if (!node->lowest) {
		list_for_each_entry(edge, &node->lower, list[UPPER]) {
			new_edge = alloc_backref_edge(cache);
			if (!new_edge)
				goto fail;

			new_edge->node[UPPER] = new_node;
			new_edge->node[LOWER] = edge->node[LOWER];
			list_add_tail(&new_edge->list[UPPER],
				      &new_node->lower);
		}
	}

	rb_node = tree_insert(&cache->rb_root, new_node->bytenr,
			      &new_node->rb_node);
	BUG_ON(rb_node);

	if (!new_node->lowest) {
		list_for_each_entry(new_edge, &new_node->lower, list[UPPER]) {
			list_add_tail(&new_edge->list[LOWER],
				      &new_edge->node[LOWER]->upper);
		}
	}
	return 0;
fail:
	while (!list_empty(&new_node->lower)) {
		new_edge = list_entry(new_node->lower.next,
				      struct backref_edge, list[UPPER]);
		list_del(&new_edge->list[UPPER]);
		free_backref_edge(cache, new_edge);
	}
	free_backref_node(cache, new_node);
	return -ENOMEM;
}

/*
 * helper to add 'address of tree root -> reloc tree' mapping
 */
static int __add_reloc_root(struct btrfs_root *root)
{
	struct rb_node *rb_node;
	struct mapping_node *node;
	struct reloc_control *rc = root->fs_info->reloc_ctl;

	node = kmalloc(sizeof(*node), GFP_NOFS);
	BUG_ON(!node);

	node->bytenr = root->node->start;
	node->data = root;

	spin_lock(&rc->reloc_root_tree.lock);
	rb_node = tree_insert(&rc->reloc_root_tree.rb_root,
			      node->bytenr, &node->rb_node);
	spin_unlock(&rc->reloc_root_tree.lock);
	BUG_ON(rb_node);

	list_add_tail(&root->root_list, &rc->reloc_roots);
	return 0;
}

/*
 * helper to update/delete the 'address of tree root -> reloc tree'
 * mapping
 */
static int __update_reloc_root(struct btrfs_root *root, int del)
{
	struct rb_node *rb_node;
	struct mapping_node *node = NULL;
	struct reloc_control *rc = root->fs_info->reloc_ctl;

	spin_lock(&rc->reloc_root_tree.lock);
	rb_node = tree_search(&rc->reloc_root_tree.rb_root,
			      root->commit_root->start);
	if (rb_node) {
		node = rb_entry(rb_node, struct mapping_node, rb_node);
		rb_erase(&node->rb_node, &rc->reloc_root_tree.rb_root);
	}
	spin_unlock(&rc->reloc_root_tree.lock);

	BUG_ON((struct btrfs_root *)node->data != root);

	if (!del) {
		spin_lock(&rc->reloc_root_tree.lock);
		node->bytenr = root->node->start;
		rb_node = tree_insert(&rc->reloc_root_tree.rb_root,
				      node->bytenr, &node->rb_node);
		spin_unlock(&rc->reloc_root_tree.lock);
		BUG_ON(rb_node);
	} else {
		list_del_init(&root->root_list);
		kfree(node);
	}
	return 0;
}

static struct btrfs_root *create_reloc_root(struct btrfs_trans_handle *trans,
					struct btrfs_root *root, u64 objectid)
{
	struct btrfs_root *reloc_root;
	struct extent_buffer *eb;
	struct btrfs_root_item *root_item;
	struct btrfs_key root_key;
	int ret;

	root_item = kmalloc(sizeof(*root_item), GFP_NOFS);
	BUG_ON(!root_item);

	root_key.objectid = BTRFS_TREE_RELOC_OBJECTID;
	root_key.type = BTRFS_ROOT_ITEM_KEY;
	root_key.offset = objectid;

	if (root->root_key.objectid == objectid) {
		/* called by btrfs_init_reloc_root */
		ret = btrfs_copy_root(trans, root, root->commit_root, &eb,
				      BTRFS_TREE_RELOC_OBJECTID);
		BUG_ON(ret);

		btrfs_set_root_last_snapshot(&root->root_item,
					     trans->transid - 1);
	} else {
		/*
		 * called by btrfs_reloc_post_snapshot_hook.
		 * the source tree is a reloc tree, all tree blocks
		 * modified after it was created have RELOC flag
		 * set in their headers. so it's OK to not update
		 * the 'last_snapshot'.
		 */
		ret = btrfs_copy_root(trans, root, root->node, &eb,
				      BTRFS_TREE_RELOC_OBJECTID);
		BUG_ON(ret);
	}

	memcpy(root_item, &root->root_item, sizeof(*root_item));
	btrfs_set_root_bytenr(root_item, eb->start);
	btrfs_set_root_level(root_item, btrfs_header_level(eb));
	btrfs_set_root_generation(root_item, trans->transid);

	if (root->root_key.objectid == objectid) {
		btrfs_set_root_refs(root_item, 0);
		memset(&root_item->drop_progress, 0,
		       sizeof(struct btrfs_disk_key));
		root_item->drop_level = 0;
	}

	btrfs_tree_unlock(eb);
	free_extent_buffer(eb);

	ret = btrfs_insert_root(trans, root->fs_info->tree_root,
				&root_key, root_item);
	BUG_ON(ret);
	kfree(root_item);

	reloc_root = btrfs_read_fs_root_no_radix(root->fs_info->tree_root,
						 &root_key);
	BUG_ON(IS_ERR(reloc_root));
	reloc_root->last_trans = trans->transid;
	return reloc_root;
}

/*
 * create reloc tree for a given fs tree. reloc tree is just a
 * snapshot of the fs tree with special root objectid.
 */
int btrfs_init_reloc_root(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root)
{
	struct btrfs_root *reloc_root;
	struct reloc_control *rc = root->fs_info->reloc_ctl;
	int clear_rsv = 0;

	if (root->reloc_root) {
		reloc_root = root->reloc_root;
		reloc_root->last_trans = trans->transid;
		return 0;
	}

	if (!rc || !rc->create_reloc_tree ||
	    root->root_key.objectid == BTRFS_TREE_RELOC_OBJECTID)
		return 0;

	if (!trans->block_rsv) {
		trans->block_rsv = rc->block_rsv;
		clear_rsv = 1;
	}
	reloc_root = create_reloc_root(trans, root, root->root_key.objectid);
	if (clear_rsv)
		trans->block_rsv = NULL;

	__add_reloc_root(reloc_root);
	root->reloc_root = reloc_root;
	return 0;
}

/*
 * update root item of reloc tree
 */
int btrfs_update_reloc_root(struct btrfs_trans_handle *trans,
			    struct btrfs_root *root)
{
	struct btrfs_root *reloc_root;
	struct btrfs_root_item *root_item;
	int del = 0;
	int ret;

	if (!root->reloc_root)
		return 0;

	reloc_root = root->reloc_root;
	root_item = &reloc_root->root_item;

	if (root->fs_info->reloc_ctl->merge_reloc_tree &&
	    btrfs_root_refs(root_item) == 0) {
		root->reloc_root = NULL;
		del = 1;
	}

	__update_reloc_root(reloc_root, del);

	if (reloc_root->commit_root != reloc_root->node) {
		btrfs_set_root_node(root_item, reloc_root->node);
		free_extent_buffer(reloc_root->commit_root);
		reloc_root->commit_root = btrfs_root_node(reloc_root);
	}

	ret = btrfs_update_root(trans, root->fs_info->tree_root,
				&reloc_root->root_key, root_item);
	BUG_ON(ret);
	return 0;
}

/*
 * helper to find first cached inode with inode number >= objectid
 * in a subvolume
 */
static struct inode *find_next_inode(struct btrfs_root *root, u64 objectid)
{
	struct rb_node *node;
	struct rb_node *prev;
	struct btrfs_inode *entry;
	struct inode *inode;

	spin_lock(&root->inode_lock);
again:
	node = root->inode_tree.rb_node;
	prev = NULL;
	while (node) {
		prev = node;
		entry = rb_entry(node, struct btrfs_inode, rb_node);

		if (objectid < entry->vfs_inode.i_ino)
			node = node->rb_left;
		else if (objectid > entry->vfs_inode.i_ino)
			node = node->rb_right;
		else
			break;
	}
	if (!node) {
		while (prev) {
			entry = rb_entry(prev, struct btrfs_inode, rb_node);
			if (objectid <= entry->vfs_inode.i_ino) {
				node = prev;
				break;
			}
			prev = rb_next(prev);
		}
	}
	while (node) {
		entry = rb_entry(node, struct btrfs_inode, rb_node);
		inode = igrab(&entry->vfs_inode);
		if (inode) {
			spin_unlock(&root->inode_lock);
			return inode;
		}

		objectid = entry->vfs_inode.i_ino + 1;
		if (cond_resched_lock(&root->inode_lock))
			goto again;

		node = rb_next(node);
	}
	spin_unlock(&root->inode_lock);
	return NULL;
}

static int in_block_group(u64 bytenr,
			  struct btrfs_block_group_cache *block_group)
{
	if (bytenr >= block_group->key.objectid &&
	    bytenr < block_group->key.objectid + block_group->key.offset)
		return 1;
	return 0;
}

/*
 * get new location of data
 */
static int get_new_location(struct inode *reloc_inode, u64 *new_bytenr,
			    u64 bytenr, u64 num_bytes)
{
	struct btrfs_root *root = BTRFS_I(reloc_inode)->root;
	struct btrfs_path *path;
	struct btrfs_file_extent_item *fi;
	struct extent_buffer *leaf;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	bytenr -= BTRFS_I(reloc_inode)->index_cnt;
	ret = btrfs_lookup_file_extent(NULL, root, path, reloc_inode->i_ino,
				       bytenr, 0);
	if (ret < 0)
		goto out;
	if (ret > 0) {
		ret = -ENOENT;
		goto out;
	}

	leaf = path->nodes[0];
	fi = btrfs_item_ptr(leaf, path->slots[0],
			    struct btrfs_file_extent_item);

	BUG_ON(btrfs_file_extent_offset(leaf, fi) ||
	       btrfs_file_extent_compression(leaf, fi) ||
	       btrfs_file_extent_encryption(leaf, fi) ||
	       btrfs_file_extent_other_encoding(leaf, fi));

	if (num_bytes != btrfs_file_extent_disk_num_bytes(leaf, fi)) {
		ret = 1;
		goto out;
	}

	*new_bytenr = btrfs_file_extent_disk_bytenr(leaf, fi);
	ret = 0;
out:
	btrfs_free_path(path);
	return ret;
}

/*
 * update file extent items in the tree leaf to point to
 * the new locations.
 */
static noinline_for_stack
int replace_file_extents(struct btrfs_trans_handle *trans,
			 struct reloc_control *rc,
			 struct btrfs_root *root,
			 struct extent_buffer *leaf)
{
	struct btrfs_key key;
	struct btrfs_file_extent_item *fi;
	struct inode *inode = NULL;
	u64 parent;
	u64 bytenr;
	u64 new_bytenr = 0;
	u64 num_bytes;
	u64 end;
	u32 nritems;
	u32 i;
	int ret;
	int first = 1;
	int dirty = 0;

	if (rc->stage != UPDATE_DATA_PTRS)
		return 0;

	/* reloc trees always use full backref */
	if (root->root_key.objectid == BTRFS_TREE_RELOC_OBJECTID)
		parent = leaf->start;
	else
		parent = 0;

	nritems = btrfs_header_nritems(leaf);
	for (i = 0; i < nritems; i++) {
		cond_resched();
		btrfs_item_key_to_cpu(leaf, &key, i);
		if (key.type != BTRFS_EXTENT_DATA_KEY)
			continue;
		fi = btrfs_item_ptr(leaf, i, struct btrfs_file_extent_item);
		if (btrfs_file_extent_type(leaf, fi) ==
		    BTRFS_FILE_EXTENT_INLINE)
			continue;
		bytenr = btrfs_file_extent_disk_bytenr(leaf, fi);
		num_bytes = btrfs_file_extent_disk_num_bytes(leaf, fi);
		if (bytenr == 0)
			continue;
		if (!in_block_group(bytenr, rc->block_group))
			continue;

		/*
		 * if we are modifying block in fs tree, wait for readpage
		 * to complete and drop the extent cache
		 */
		if (root->root_key.objectid != BTRFS_TREE_RELOC_OBJECTID) {
			if (first) {
				inode = find_next_inode(root, key.objectid);
				first = 0;
			} else if (inode && inode->i_ino < key.objectid) {
				btrfs_add_delayed_iput(inode);
				inode = find_next_inode(root, key.objectid);
			}
			if (inode && inode->i_ino == key.objectid) {
				end = key.offset +
				      btrfs_file_extent_num_bytes(leaf, fi);
				WARN_ON(!IS_ALIGNED(key.offset,
						    root->sectorsize));
				WARN_ON(!IS_ALIGNED(end, root->sectorsize));
				end--;
				ret = try_lock_extent(&BTRFS_I(inode)->io_tree,
						      key.offset, end,
						      GFP_NOFS);
				if (!ret)
					continue;

				btrfs_drop_extent_cache(inode, key.offset, end,
							1);
				unlock_extent(&BTRFS_I(inode)->io_tree,
					      key.offset, end, GFP_NOFS);
			}
		}

		ret = get_new_location(rc->data_inode, &new_bytenr,
				       bytenr, num_bytes);
		if (ret > 0) {
			WARN_ON(1);
			continue;
		}
		BUG_ON(ret < 0);

		btrfs_set_file_extent_disk_bytenr(leaf, fi, new_bytenr);
		dirty = 1;

		key.offset -= btrfs_file_extent_offset(leaf, fi);
		ret = btrfs_inc_extent_ref(trans, root, new_bytenr,
					   num_bytes, parent,
					   btrfs_header_owner(leaf),
					   key.objectid, key.offset);
		BUG_ON(ret);

		ret = btrfs_free_extent(trans, root, bytenr, num_bytes,
					parent, btrfs_header_owner(leaf),
					key.objectid, key.offset);
		BUG_ON(ret);
	}
	if (dirty)
		btrfs_mark_buffer_dirty(leaf);
	if (inode)
		btrfs_add_delayed_iput(inode);
	return 0;
}

static noinline_for_stack
int memcmp_node_keys(struct extent_buffer *eb, int slot,
		     struct btrfs_path *path, int level)
{
	struct btrfs_disk_key key1;
	struct btrfs_disk_key key2;
	btrfs_node_key(eb, &key1, slot);
	btrfs_node_key(path->nodes[level], &key2, path->slots[level]);
	return memcmp(&key1, &key2, sizeof(key1));
}

/*
 * try to replace tree blocks in fs tree with the new blocks
 * in reloc tree. tree blocks haven't been modified since the
 * reloc tree was create can be replaced.
 *
 * if a block was replaced, level of the block + 1 is returned.
 * if no block got replaced, 0 is returned. if there are other
 * errors, a negative error number is returned.
 */
static noinline_for_stack
int replace_path(struct btrfs_trans_handle *trans,
		 struct btrfs_root *dest, struct btrfs_root *src,
		 struct btrfs_path *path, struct btrfs_key *next_key,
		 int lowest_level, int max_level)
{
	struct extent_buffer *eb;
	struct extent_buffer *parent;
	struct btrfs_key key;
	u64 old_bytenr;
	u64 new_bytenr;
	u64 old_ptr_gen;
	u64 new_ptr_gen;
	u64 last_snapshot;
	u32 blocksize;
	int cow = 0;
	int level;
	int ret;
	int slot;

	BUG_ON(src->root_key.objectid != BTRFS_TREE_RELOC_OBJECTID);
	BUG_ON(dest->root_key.objectid == BTRFS_TREE_RELOC_OBJECTID);

	last_snapshot = btrfs_root_last_snapshot(&src->root_item);
again:
	slot = path->slots[lowest_level];
	btrfs_node_key_to_cpu(path->nodes[lowest_level], &key, slot);

	eb = btrfs_lock_root_node(dest);
	btrfs_set_lock_blocking(eb);
	level = btrfs_header_level(eb);

	if (level < lowest_level) {
		btrfs_tree_unlock(eb);
		free_extent_buffer(eb);
		return 0;
	}

	if (cow) {
		ret = btrfs_cow_block(trans, dest, eb, NULL, 0, &eb);
		BUG_ON(ret);
	}
	btrfs_set_lock_blocking(eb);

	if (next_key) {
		next_key->objectid = (u64)-1;
		next_key->type = (u8)-1;
		next_key->offset = (u64)-1;
	}

	parent = eb;
	while (1) {
		level = btrfs_header_level(parent);
		BUG_ON(level < lowest_level);

		ret = btrfs_bin_search(parent, &key, level, &slot);
		if (ret && slot > 0)
			slot--;

		if (next_key && slot + 1 < btrfs_header_nritems(parent))
			btrfs_node_key_to_cpu(parent, next_key, slot + 1);

		old_bytenr = btrfs_node_blockptr(parent, slot);
		blocksize = btrfs_level_size(dest, level - 1);
		old_ptr_gen = btrfs_node_ptr_generation(parent, slot);

		if (level <= max_level) {
			eb = path->nodes[level];
			new_bytenr = btrfs_node_blockptr(eb,
							path->slots[level]);
			new_ptr_gen = btrfs_node_ptr_generation(eb,
							path->slots[level]);
		} else {
			new_bytenr = 0;
			new_ptr_gen = 0;
		}

		if (new_bytenr > 0 && new_bytenr == old_bytenr) {
			WARN_ON(1);
			ret = level;
			break;
		}

		if (new_bytenr == 0 || old_ptr_gen > last_snapshot ||
		    memcmp_node_keys(parent, slot, path, level)) {
			if (level <= lowest_level) {
				ret = 0;
				break;
			}

			eb = read_tree_block(dest, old_bytenr, blocksize,
					     old_ptr_gen);
			btrfs_tree_lock(eb);
			if (cow) {
				ret = btrfs_cow_block(trans, dest, eb, parent,
						      slot, &eb);
				BUG_ON(ret);
			}
			btrfs_set_lock_blocking(eb);

			btrfs_tree_unlock(parent);
			free_extent_buffer(parent);

			parent = eb;
			continue;
		}

		if (!cow) {
			btrfs_tree_unlock(parent);
			free_extent_buffer(parent);
			cow = 1;
			goto again;
		}

		btrfs_node_key_to_cpu(path->nodes[level], &key,
				      path->slots[level]);
		btrfs_release_path(src, path);

		path->lowest_level = level;
		ret = btrfs_search_slot(trans, src, &key, path, 0, 1);
		path->lowest_level = 0;
		BUG_ON(ret);

		/*
		 * swap blocks in fs tree and reloc tree.
		 */
		btrfs_set_node_blockptr(parent, slot, new_bytenr);
		btrfs_set_node_ptr_generation(parent, slot, new_ptr_gen);
		btrfs_mark_buffer_dirty(parent);

		btrfs_set_node_blockptr(path->nodes[level],
					path->slots[level], old_bytenr);
		btrfs_set_node_ptr_generation(path->nodes[level],
					      path->slots[level], old_ptr_gen);
		btrfs_mark_buffer_dirty(path->nodes[level]);

		ret = btrfs_inc_extent_ref(trans, src, old_bytenr, blocksize,
					path->nodes[level]->start,
					src->root_key.objectid, level - 1, 0);
		BUG_ON(ret);
		ret = btrfs_inc_extent_ref(trans, dest, new_bytenr, blocksize,
					0, dest->root_key.objectid, level - 1,
					0);
		BUG_ON(ret);

		ret = btrfs_free_extent(trans, src, new_bytenr, blocksize,
					path->nodes[level]->start,
					src->root_key.objectid, level - 1, 0);
		BUG_ON(ret);

		ret = btrfs_free_extent(trans, dest, old_bytenr, blocksize,
					0, dest->root_key.objectid, level - 1,
					0);
		BUG_ON(ret);

		btrfs_unlock_up_safe(path, 0);

		ret = level;
		break;
	}
	btrfs_tree_unlock(parent);
	free_extent_buffer(parent);
	return ret;
}

/*
 * helper to find next relocated block in reloc tree
 */
static noinline_for_stack
int walk_up_reloc_tree(struct btrfs_root *root, struct btrfs_path *path,
		       int *level)
{
	struct extent_buffer *eb;
	int i;
	u64 last_snapshot;
	u32 nritems;

	last_snapshot = btrfs_root_last_snapshot(&root->root_item);

	for (i = 0; i < *level; i++) {
		free_extent_buffer(path->nodes[i]);
		path->nodes[i] = NULL;
	}

	for (i = *level; i < BTRFS_MAX_LEVEL && path->nodes[i]; i++) {
		eb = path->nodes[i];
		nritems = btrfs_header_nritems(eb);
		while (path->slots[i] + 1 < nritems) {
			path->slots[i]++;
			if (btrfs_node_ptr_generation(eb, path->slots[i]) <=
			    last_snapshot)
				continue;

			*level = i;
			return 0;
		}
		free_extent_buffer(path->nodes[i]);
		path->nodes[i] = NULL;
	}
	return 1;
}

/*
 * walk down reloc tree to find relocated block of lowest level
 */
static noinline_for_stack
int walk_down_reloc_tree(struct btrfs_root *root, struct btrfs_path *path,
			 int *level)
{
	struct extent_buffer *eb = NULL;
	int i;
	u64 bytenr;
	u64 ptr_gen = 0;
	u64 last_snapshot;
	u32 blocksize;
	u32 nritems;

	last_snapshot = btrfs_root_last_snapshot(&root->root_item);

	for (i = *level; i > 0; i--) {
		eb = path->nodes[i];
		nritems = btrfs_header_nritems(eb);
		while (path->slots[i] < nritems) {
			ptr_gen = btrfs_node_ptr_generation(eb, path->slots[i]);
			if (ptr_gen > last_snapshot)
				break;
			path->slots[i]++;
		}
		if (path->slots[i] >= nritems) {
			if (i == *level)
				break;
			*level = i + 1;
			return 0;
		}
		if (i == 1) {
			*level = i;
			return 0;
		}

		bytenr = btrfs_node_blockptr(eb, path->slots[i]);
		blocksize = btrfs_level_size(root, i - 1);
		eb = read_tree_block(root, bytenr, blocksize, ptr_gen);
		BUG_ON(btrfs_header_level(eb) != i - 1);
		path->nodes[i - 1] = eb;
		path->slots[i - 1] = 0;
	}
	return 1;
}

/*
 * invalidate extent cache for file extents whose key in range of
 * [min_key, max_key)
 */
static int invalidate_extent_cache(struct btrfs_root *root,
				   struct btrfs_key *min_key,
				   struct btrfs_key *max_key)
{
	struct inode *inode = NULL;
	u64 objectid;
	u64 start, end;

	objectid = min_key->objectid;
	while (1) {
		cond_resched();
		iput(inode);

		if (objectid > max_key->objectid)
			break;

		inode = find_next_inode(root, objectid);
		if (!inode)
			break;

		if (inode->i_ino > max_key->objectid) {
			iput(inode);
			break;
		}

		objectid = inode->i_ino + 1;
		if (!S_ISREG(inode->i_mode))
			continue;

		if (unlikely(min_key->objectid == inode->i_ino)) {
			if (min_key->type > BTRFS_EXTENT_DATA_KEY)
				continue;
			if (min_key->type < BTRFS_EXTENT_DATA_KEY)
				start = 0;
			else {
				start = min_key->offset;
				WARN_ON(!IS_ALIGNED(start, root->sectorsize));
			}
		} else {
			start = 0;
		}

		if (unlikely(max_key->objectid == inode->i_ino)) {
			if (max_key->type < BTRFS_EXTENT_DATA_KEY)
				continue;
			if (max_key->type > BTRFS_EXTENT_DATA_KEY) {
				end = (u64)-1;
			} else {
				if (max_key->offset == 0)
					continue;
				end = max_key->offset;
				WARN_ON(!IS_ALIGNED(end, root->sectorsize));
				end--;
			}
		} else {
			end = (u64)-1;
		}

		/* the lock_extent waits for readpage to complete */
		lock_extent(&BTRFS_I(inode)->io_tree, start, end, GFP_NOFS);
		btrfs_drop_extent_cache(inode, start, end, 1);
		unlock_extent(&BTRFS_I(inode)->io_tree, start, end, GFP_NOFS);
	}
	return 0;
}

static int find_next_key(struct btrfs_path *path, int level,
			 struct btrfs_key *key)

{
	while (level < BTRFS_MAX_LEVEL) {
		if (!path->nodes[level])
			break;
		if (path->slots[level] + 1 <
		    btrfs_header_nritems(path->nodes[level])) {
			btrfs_node_key_to_cpu(path->nodes[level], key,
					      path->slots[level] + 1);
			return 0;
		}
		level++;
	}
	return 1;
}

/*
 * merge the relocated tree blocks in reloc tree with corresponding
 * fs tree.
 */
static noinline_for_stack int merge_reloc_root(struct reloc_control *rc,
					       struct btrfs_root *root)
{
	LIST_HEAD(inode_list);
	struct btrfs_key key;
	struct btrfs_key next_key;
	struct btrfs_trans_handle *trans;
	struct btrfs_root *reloc_root;
	struct btrfs_root_item *root_item;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	unsigned long nr;
	int level;
	int max_level;
	int replaced = 0;
	int ret;
	int err = 0;
	u32 min_reserved;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	reloc_root = root->reloc_root;
	root_item = &reloc_root->root_item;

	if (btrfs_disk_key_objectid(&root_item->drop_progress) == 0) {
		level = btrfs_root_level(root_item);
		extent_buffer_get(reloc_root->node);
		path->nodes[level] = reloc_root->node;
		path->slots[level] = 0;
	} else {
		btrfs_disk_key_to_cpu(&key, &root_item->drop_progress);

		level = root_item->drop_level;
		BUG_ON(level == 0);
		path->lowest_level = level;
		ret = btrfs_search_slot(NULL, reloc_root, &key, path, 0, 0);
		path->lowest_level = 0;
		if (ret < 0) {
			btrfs_free_path(path);
			return ret;
		}

		btrfs_node_key_to_cpu(path->nodes[level], &next_key,
				      path->slots[level]);
		WARN_ON(memcmp(&key, &next_key, sizeof(key)));

		btrfs_unlock_up_safe(path, 0);
	}

	min_reserved = root->nodesize * (BTRFS_MAX_LEVEL - 1) * 2;
	memset(&next_key, 0, sizeof(next_key));

	while (1) {
		trans = btrfs_start_transaction(root, 0);
		trans->block_rsv = rc->block_rsv;

		ret = btrfs_block_rsv_check(trans, root, rc->block_rsv,
					    min_reserved, 0);
		if (ret) {
			BUG_ON(ret != -EAGAIN);
			ret = btrfs_commit_transaction(trans, root);
			BUG_ON(ret);
			continue;
		}

		replaced = 0;
		max_level = level;

		ret = walk_down_reloc_tree(reloc_root, path, &level);
		if (ret < 0) {
			err = ret;
			goto out;
		}
		if (ret > 0)
			break;

		if (!find_next_key(path, level, &key) &&
		    btrfs_comp_cpu_keys(&next_key, &key) >= 0) {
			ret = 0;
		} else {
			ret = replace_path(trans, root, reloc_root, path,
					   &next_key, level, max_level);
		}
		if (ret < 0) {
			err = ret;
			goto out;
		}

		if (ret > 0) {
			level = ret;
			btrfs_node_key_to_cpu(path->nodes[level], &key,
					      path->slots[level]);
			replaced = 1;
		}

		ret = walk_up_reloc_tree(reloc_root, path, &level);
		if (ret > 0)
			break;

		BUG_ON(level == 0);
		/*
		 * save the merging progress in the drop_progress.
		 * this is OK since root refs == 1 in this case.
		 */
		btrfs_node_key(path->nodes[level], &root_item->drop_progress,
			       path->slots[level]);
		root_item->drop_level = level;

		nr = trans->blocks_used;
		btrfs_end_transaction_throttle(trans, root);

		btrfs_btree_balance_dirty(root, nr);

		if (replaced && rc->stage == UPDATE_DATA_PTRS)
			invalidate_extent_cache(root, &key, &next_key);
	}

	/*
	 * handle the case only one block in the fs tree need to be
	 * relocated and the block is tree root.
	 */
	leaf = btrfs_lock_root_node(root);
	ret = btrfs_cow_block(trans, root, leaf, NULL, 0, &leaf);
	btrfs_tree_unlock(leaf);
	free_extent_buffer(leaf);
	if (ret < 0)
		err = ret;
out:
	btrfs_free_path(path);

	if (err == 0) {
		memset(&root_item->drop_progress, 0,
		       sizeof(root_item->drop_progress));
		root_item->drop_level = 0;
		btrfs_set_root_refs(root_item, 0);
		btrfs_update_reloc_root(trans, root);
	}

	nr = trans->blocks_used;
	btrfs_end_transaction_throttle(trans, root);

	btrfs_btree_balance_dirty(root, nr);

	if (replaced && rc->stage == UPDATE_DATA_PTRS)
		invalidate_extent_cache(root, &key, &next_key);

	return err;
}

static noinline_for_stack
int prepare_to_merge(struct reloc_control *rc, int err)
{
	struct btrfs_root *root = rc->extent_root;
	struct btrfs_root *reloc_root;
	struct btrfs_trans_handle *trans;
	LIST_HEAD(reloc_roots);
	u64 num_bytes = 0;
	int ret;
	int retries = 0;

	mutex_lock(&root->fs_info->trans_mutex);
	rc->merging_rsv_size += root->nodesize * (BTRFS_MAX_LEVEL - 1) * 2;
	rc->merging_rsv_size += rc->nodes_relocated * 2;
	mutex_unlock(&root->fs_info->trans_mutex);
again:
	if (!err) {
		num_bytes = rc->merging_rsv_size;
		ret = btrfs_block_rsv_add(NULL, root, rc->block_rsv,
					  num_bytes, &retries);
		if (ret)
			err = ret;
	}

	trans = btrfs_join_transaction(rc->extent_root, 1);

	if (!err) {
		if (num_bytes != rc->merging_rsv_size) {
			btrfs_end_transaction(trans, rc->extent_root);
			btrfs_block_rsv_release(rc->extent_root,
						rc->block_rsv, num_bytes);
			retries = 0;
			goto again;
		}
	}

	rc->merge_reloc_tree = 1;

	while (!list_empty(&rc->reloc_roots)) {
		reloc_root = list_entry(rc->reloc_roots.next,
					struct btrfs_root, root_list);
		list_del_init(&reloc_root->root_list);

		root = read_fs_root(reloc_root->fs_info,
				    reloc_root->root_key.offset);
		BUG_ON(IS_ERR(root));
		BUG_ON(root->reloc_root != reloc_root);

		/*
		 * set reference count to 1, so btrfs_recover_relocation
		 * knows it should resumes merging
		 */
		if (!err)
			btrfs_set_root_refs(&reloc_root->root_item, 1);
		btrfs_update_reloc_root(trans, root);

		list_add(&reloc_root->root_list, &reloc_roots);
	}

	list_splice(&reloc_roots, &rc->reloc_roots);

	if (!err)
		btrfs_commit_transaction(trans, rc->extent_root);
	else
		btrfs_end_transaction(trans, rc->extent_root);
	return err;
}

static noinline_for_stack
int merge_reloc_roots(struct reloc_control *rc)
{
	struct btrfs_root *root;
	struct btrfs_root *reloc_root;
	LIST_HEAD(reloc_roots);
	int found = 0;
	int ret;
again:
	root = rc->extent_root;
	mutex_lock(&root->fs_info->trans_mutex);
	list_splice_init(&rc->reloc_roots, &reloc_roots);
	mutex_unlock(&root->fs_info->trans_mutex);

	while (!list_empty(&reloc_roots)) {
		found = 1;
		reloc_root = list_entry(reloc_roots.next,
					struct btrfs_root, root_list);

		if (btrfs_root_refs(&reloc_root->root_item) > 0) {
			root = read_fs_root(reloc_root->fs_info,
					    reloc_root->root_key.offset);
			BUG_ON(IS_ERR(root));
			BUG_ON(root->reloc_root != reloc_root);

			ret = merge_reloc_root(rc, root);
			BUG_ON(ret);
		} else {
			list_del_init(&reloc_root->root_list);
		}
		btrfs_drop_snapshot(reloc_root, rc->block_rsv, 0);
	}

	if (found) {
		found = 0;
		goto again;
	}
	BUG_ON(!RB_EMPTY_ROOT(&rc->reloc_root_tree.rb_root));
	return 0;
}

static void free_block_list(struct rb_root *blocks)
{
	struct tree_block *block;
	struct rb_node *rb_node;
	while ((rb_node = rb_first(blocks))) {
		block = rb_entry(rb_node, struct tree_block, rb_node);
		rb_erase(rb_node, blocks);
		kfree(block);
	}
}

static int record_reloc_root_in_trans(struct btrfs_trans_handle *trans,
				      struct btrfs_root *reloc_root)
{
	struct btrfs_root *root;

	if (reloc_root->last_trans == trans->transid)
		return 0;

	root = read_fs_root(reloc_root->fs_info, reloc_root->root_key.offset);
	BUG_ON(IS_ERR(root));
	BUG_ON(root->reloc_root != reloc_root);

	return btrfs_record_root_in_trans(trans, root);
}

static noinline_for_stack
struct btrfs_root *select_reloc_root(struct btrfs_trans_handle *trans,
				     struct reloc_control *rc,
				     struct backref_node *node,
				     struct backref_edge *edges[], int *nr)
{
	struct backref_node *next;
	struct btrfs_root *root;
	int index = 0;

	next = node;
	while (1) {
		cond_resched();
		next = walk_up_backref(next, edges, &index);
		root = next->root;
		BUG_ON(!root);
		BUG_ON(!root->ref_cows);

		if (root->root_key.objectid == BTRFS_TREE_RELOC_OBJECTID) {
			record_reloc_root_in_trans(trans, root);
			break;
		}

		btrfs_record_root_in_trans(trans, root);
		root = root->reloc_root;

		if (next->new_bytenr != root->node->start) {
			BUG_ON(next->new_bytenr);
			BUG_ON(!list_empty(&next->list));
			next->new_bytenr = root->node->start;
			next->root = root;
			list_add_tail(&next->list,
				      &rc->backref_cache.changed);
			__mark_block_processed(rc, next);
			break;
		}

		WARN_ON(1);
		root = NULL;
		next = walk_down_backref(edges, &index);
		if (!next || next->level <= node->level)
			break;
	}
	if (!root)
		return NULL;

	*nr = index;
	next = node;
	/* setup backref node path for btrfs_reloc_cow_block */
	while (1) {
		rc->backref_cache.path[next->level] = next;
		if (--index < 0)
			break;
		next = edges[index]->node[UPPER];
	}
	return root;
}

/*
 * select a tree root for relocation. return NULL if the block
 * is reference counted. we should use do_relocation() in this
 * case. return a tree root pointer if the block isn't reference
 * counted. return -ENOENT if the block is root of reloc tree.
 */
static noinline_for_stack
struct btrfs_root *select_one_root(struct btrfs_trans_handle *trans,
				   struct backref_node *node)
{
	struct backref_node *next;
	struct btrfs_root *root;
	struct btrfs_root *fs_root = NULL;
	struct backref_edge *edges[BTRFS_MAX_LEVEL - 1];
	int index = 0;

	next = node;
	while (1) {
		cond_resched();
		next = walk_up_backref(next, edges, &index);
		root = next->root;
		BUG_ON(!root);

		/* no other choice for non-refernce counted tree */
		if (!root->ref_cows)
			return root;

		if (root->root_key.objectid != BTRFS_TREE_RELOC_OBJECTID)
			fs_root = root;

		if (next != node)
			return NULL;

		next = walk_down_backref(edges, &index);
		if (!next || next->level <= node->level)
			break;
	}

	if (!fs_root)
		return ERR_PTR(-ENOENT);
	return fs_root;
}

static noinline_for_stack
u64 calcu_metadata_size(struct reloc_control *rc,
			struct backref_node *node, int reserve)
{
	struct backref_node *next = node;
	struct backref_edge *edge;
	struct backref_edge *edges[BTRFS_MAX_LEVEL - 1];
	u64 num_bytes = 0;
	int index = 0;

	BUG_ON(reserve && node->processed);

	while (next) {
		cond_resched();
		while (1) {
			if (next->processed && (reserve || next != node))
				break;

			num_bytes += btrfs_level_size(rc->extent_root,
						      next->level);

			if (list_empty(&next->upper))
				break;

			edge = list_entry(next->upper.next,
					  struct backref_edge, list[LOWER]);
			edges[index++] = edge;
			next = edge->node[UPPER];
		}
		next = walk_down_backref(edges, &index);
	}
	return num_bytes;
}

static int reserve_metadata_space(struct btrfs_trans_handle *trans,
				  struct reloc_control *rc,
				  struct backref_node *node)
{
	struct btrfs_root *root = rc->extent_root;
	u64 num_bytes;
	int ret;

	num_bytes = calcu_metadata_size(rc, node, 1) * 2;

	trans->block_rsv = rc->block_rsv;
	ret = btrfs_block_rsv_add(trans, root, rc->block_rsv, num_bytes,
				  &rc->block_rsv_retries);
	if (ret) {
		if (ret == -EAGAIN)
			rc->commit_transaction = 1;
		return ret;
	}

	rc->block_rsv_retries = 0;
	return 0;
}

static void release_metadata_space(struct reloc_control *rc,
				   struct backref_node *node)
{
	u64 num_bytes = calcu_metadata_size(rc, node, 0) * 2;
	btrfs_block_rsv_release(rc->extent_root, rc->block_rsv, num_bytes);
}

/*
 * relocate a block tree, and then update pointers in upper level
 * blocks that reference the block to point to the new location.
 *
 * if called by link_to_upper, the block has already been relocated.
 * in that case this function just updates pointers.
 */
static int do_relocation(struct btrfs_trans_handle *trans,
			 struct reloc_control *rc,
			 struct backref_node *node,
			 struct btrfs_key *key,
			 struct btrfs_path *path, int lowest)
{
	struct backref_node *upper;
	struct backref_edge *edge;
	struct backref_edge *edges[BTRFS_MAX_LEVEL - 1];
	struct btrfs_root *root;
	struct extent_buffer *eb;
	u32 blocksize;
	u64 bytenr;
	u64 generation;
	int nr;
	int slot;
	int ret;
	int err = 0;

	BUG_ON(lowest && node->eb);

	path->lowest_level = node->level + 1;
	rc->backref_cache.path[node->level] = node;
	list_for_each_entry(edge, &node->upper, list[LOWER]) {
		cond_resched();

		upper = edge->node[UPPER];
		root = select_reloc_root(trans, rc, upper, edges, &nr);
		BUG_ON(!root);

		if (upper->eb && !upper->locked) {
			if (!lowest) {
				ret = btrfs_bin_search(upper->eb, key,
						       upper->level, &slot);
				BUG_ON(ret);
				bytenr = btrfs_node_blockptr(upper->eb, slot);
				if (node->eb->start == bytenr)
					goto next;
			}
			drop_node_buffer(upper);
		}

		if (!upper->eb) {
			ret = btrfs_search_slot(trans, root, key, path, 0, 1);
			if (ret < 0) {
				err = ret;
				break;
			}
			BUG_ON(ret > 0);

			if (!upper->eb) {
				upper->eb = path->nodes[upper->level];
				path->nodes[upper->level] = NULL;
			} else {
				BUG_ON(upper->eb != path->nodes[upper->level]);
			}

			upper->locked = 1;
			path->locks[upper->level] = 0;

			slot = path->slots[upper->level];
			btrfs_release_path(NULL, path);
		} else {
			ret = btrfs_bin_search(upper->eb, key, upper->level,
					       &slot);
			BUG_ON(ret);
		}

		bytenr = btrfs_node_blockptr(upper->eb, slot);
		if (lowest) {
			BUG_ON(bytenr != node->bytenr);
		} else {
			if (node->eb->start == bytenr)
				goto next;
		}

		blocksize = btrfs_level_size(root, node->level);
		generation = btrfs_node_ptr_generation(upper->eb, slot);
		eb = read_tree_block(root, bytenr, blocksize, generation);
		btrfs_tree_lock(eb);
		btrfs_set_lock_blocking(eb);

		if (!node->eb) {
			ret = btrfs_cow_block(trans, root, eb, upper->eb,
					      slot, &eb);
			btrfs_tree_unlock(eb);
			free_extent_buffer(eb);
			if (ret < 0) {
				err = ret;
				goto next;
			}
			BUG_ON(node->eb != eb);
		} else {
			btrfs_set_node_blockptr(upper->eb, slot,
						node->eb->start);
			btrfs_set_node_ptr_generation(upper->eb, slot,
						      trans->transid);
			btrfs_mark_buffer_dirty(upper->eb);

			ret = btrfs_inc_extent_ref(trans, root,
						node->eb->start, blocksize,
						upper->eb->start,
						btrfs_header_owner(upper->eb),
						node->level, 0);
			BUG_ON(ret);

			ret = btrfs_drop_subtree(trans, root, eb, upper->eb);
			BUG_ON(ret);
		}
next:
		if (!upper->pending)
			drop_node_buffer(upper);
		else
			unlock_node_buffer(upper);
		if (err)
			break;
	}

	if (!err && node->pending) {
		drop_node_buffer(node);
		list_move_tail(&node->list, &rc->backref_cache.changed);
		node->pending = 0;
	}

	path->lowest_level = 0;
	BUG_ON(err == -ENOSPC);
	return err;
}

static int link_to_upper(struct btrfs_trans_handle *trans,
			 struct reloc_control *rc,
			 struct backref_node *node,
			 struct btrfs_path *path)
{
	struct btrfs_key key;

	btrfs_node_key_to_cpu(node->eb, &key, 0);
	return do_relocation(trans, rc, node, &key, path, 0);
}

static int finish_pending_nodes(struct btrfs_trans_handle *trans,
				struct reloc_control *rc,
				struct btrfs_path *path, int err)
{
	LIST_HEAD(list);
	struct backref_cache *cache = &rc->backref_cache;
	struct backref_node *node;
	int level;
	int ret;

	for (level = 0; level < BTRFS_MAX_LEVEL; level++) {
		while (!list_empty(&cache->pending[level])) {
			node = list_entry(cache->pending[level].next,
					  struct backref_node, list);
			list_move_tail(&node->list, &list);
			BUG_ON(!node->pending);

			if (!err) {
				ret = link_to_upper(trans, rc, node, path);
				if (ret < 0)
					err = ret;
			}
		}
		list_splice_init(&list, &cache->pending[level]);
	}
	return err;
}

static void mark_block_processed(struct reloc_control *rc,
				 u64 bytenr, u32 blocksize)
{
	set_extent_bits(&rc->processed_blocks, bytenr, bytenr + blocksize - 1,
			EXTENT_DIRTY, GFP_NOFS);
}

static void __mark_block_processed(struct reloc_control *rc,
				   struct backref_node *node)
{
	u32 blocksize;
	if (node->level == 0 ||
	    in_block_group(node->bytenr, rc->block_group)) {
		blocksize = btrfs_level_size(rc->extent_root, node->level);
		mark_block_processed(rc, node->bytenr, blocksize);
	}
	node->processed = 1;
}

/*
 * mark a block and all blocks directly/indirectly reference the block
 * as processed.
 */
static void update_processed_blocks(struct reloc_control *rc,
				    struct backref_node *node)
{
	struct backref_node *next = node;
	struct backref_edge *edge;
	struct backref_edge *edges[BTRFS_MAX_LEVEL - 1];
	int index = 0;

	while (next) {
		cond_resched();
		while (1) {
			if (next->processed)
				break;

			__mark_block_processed(rc, next);

			if (list_empty(&next->upper))
				break;

			edge = list_entry(next->upper.next,
					  struct backref_edge, list[LOWER]);
			edges[index++] = edge;
			next = edge->node[UPPER];
		}
		next = walk_down_backref(edges, &index);
	}
}

static int tree_block_processed(u64 bytenr, u32 blocksize,
				struct reloc_control *rc)
{
	if (test_range_bit(&rc->processed_blocks, bytenr,
			   bytenr + blocksize - 1, EXTENT_DIRTY, 1, NULL))
		return 1;
	return 0;
}

static int get_tree_block_key(struct reloc_control *rc,
			      struct tree_block *block)
{
	struct extent_buffer *eb;

	BUG_ON(block->key_ready);
	eb = read_tree_block(rc->extent_root, block->bytenr,
			     block->key.objectid, block->key.offset);
	WARN_ON(btrfs_header_level(eb) != block->level);
	if (block->level == 0)
		btrfs_item_key_to_cpu(eb, &block->key, 0);
	else
		btrfs_node_key_to_cpu(eb, &block->key, 0);
	free_extent_buffer(eb);
	block->key_ready = 1;
	return 0;
}

static int reada_tree_block(struct reloc_control *rc,
			    struct tree_block *block)
{
	BUG_ON(block->key_ready);
	readahead_tree_block(rc->extent_root, block->bytenr,
			     block->key.objectid, block->key.offset);
	return 0;
}

/*
 * helper function to relocate a tree block
 */
static int relocate_tree_block(struct btrfs_trans_handle *trans,
				struct reloc_control *rc,
				struct backref_node *node,
				struct btrfs_key *key,
				struct btrfs_path *path)
{
	struct btrfs_root *root;
	int release = 0;
	int ret = 0;

	if (!node)
		return 0;

	BUG_ON(node->processed);
	root = select_one_root(trans, node);
	if (root == ERR_PTR(-ENOENT)) {
		update_processed_blocks(rc, node);
		goto out;
	}

	if (!root || root->ref_cows) {
		ret = reserve_metadata_space(trans, rc, node);
		if (ret)
			goto out;
		release = 1;
	}

	if (root) {
		if (root->ref_cows) {
			BUG_ON(node->new_bytenr);
			BUG_ON(!list_empty(&node->list));
			btrfs_record_root_in_trans(trans, root);
			root = root->reloc_root;
			node->new_bytenr = root->node->start;
			node->root = root;
			list_add_tail(&node->list, &rc->backref_cache.changed);
		} else {
			path->lowest_level = node->level;
			ret = btrfs_search_slot(trans, root, key, path, 0, 1);
			btrfs_release_path(root, path);
			if (ret > 0)
				ret = 0;
		}
		if (!ret)
			update_processed_blocks(rc, node);
	} else {
		ret = do_relocation(trans, rc, node, key, path, 1);
	}
out:
	if (ret || node->level == 0 || node->cowonly) {
		if (release)
			release_metadata_space(rc, node);
		remove_backref_node(&rc->backref_cache, node);
	}
	return ret;
}

/*
 * relocate a list of blocks
 */
static noinline_for_stack
int relocate_tree_blocks(struct btrfs_trans_handle *trans,
			 struct reloc_control *rc, struct rb_root *blocks)
{
	struct backref_node *node;
	struct btrfs_path *path;
	struct tree_block *block;
	struct rb_node *rb_node;
	int ret;
	int err = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	rb_node = rb_first(blocks);
	while (rb_node) {
		block = rb_entry(rb_node, struct tree_block, rb_node);
		if (!block->key_ready)
			reada_tree_block(rc, block);
		rb_node = rb_next(rb_node);
	}

	rb_node = rb_first(blocks);
	while (rb_node) {
		block = rb_entry(rb_node, struct tree_block, rb_node);
		if (!block->key_ready)
			get_tree_block_key(rc, block);
		rb_node = rb_next(rb_node);
	}

	rb_node = rb_first(blocks);
	while (rb_node) {
		block = rb_entry(rb_node, struct tree_block, rb_node);

		node = build_backref_tree(rc, &block->key,
					  block->level, block->bytenr);
		if (IS_ERR(node)) {
			err = PTR_ERR(node);
			goto out;
		}

		ret = relocate_tree_block(trans, rc, node, &block->key,
					  path);
		if (ret < 0) {
			if (ret != -EAGAIN || rb_node == rb_first(blocks))
				err = ret;
			goto out;
		}
		rb_node = rb_next(rb_node);
	}
out:
	free_block_list(blocks);
	err = finish_pending_nodes(trans, rc, path, err);

	btrfs_free_path(path);
	return err;
}

static noinline_for_stack
int prealloc_file_extent_cluster(struct inode *inode,
				 struct file_extent_cluster *cluster)
{
	u64 alloc_hint = 0;
	u64 start;
	u64 end;
	u64 offset = BTRFS_I(inode)->index_cnt;
	u64 num_bytes;
	int nr = 0;
	int ret = 0;

	BUG_ON(cluster->start != cluster->boundary[0]);
	mutex_lock(&inode->i_mutex);

	ret = btrfs_check_data_free_space(inode, cluster->end +
					  1 - cluster->start);
	if (ret)
		goto out;

	while (nr < cluster->nr) {
		start = cluster->boundary[nr] - offset;
		if (nr + 1 < cluster->nr)
			end = cluster->boundary[nr + 1] - 1 - offset;
		else
			end = cluster->end - offset;

		lock_extent(&BTRFS_I(inode)->io_tree, start, end, GFP_NOFS);
		num_bytes = end + 1 - start;
		ret = btrfs_prealloc_file_range(inode, 0, start,
						num_bytes, num_bytes,
						end + 1, &alloc_hint);
		unlock_extent(&BTRFS_I(inode)->io_tree, start, end, GFP_NOFS);
		if (ret)
			break;
		nr++;
	}
	btrfs_free_reserved_data_space(inode, cluster->end +
				       1 - cluster->start);
out:
	mutex_unlock(&inode->i_mutex);
	return ret;
}

static noinline_for_stack
int setup_extent_mapping(struct inode *inode, u64 start, u64 end,
			 u64 block_start)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	struct extent_map *em;
	int ret = 0;

	em = alloc_extent_map(GFP_NOFS);
	if (!em)
		return -ENOMEM;

	em->start = start;
	em->len = end + 1 - start;
	em->block_len = em->len;
	em->block_start = block_start;
	em->bdev = root->fs_info->fs_devices->latest_bdev;
	set_bit(EXTENT_FLAG_PINNED, &em->flags);

	lock_extent(&BTRFS_I(inode)->io_tree, start, end, GFP_NOFS);
	while (1) {
		write_lock(&em_tree->lock);
		ret = add_extent_mapping(em_tree, em);
		write_unlock(&em_tree->lock);
		if (ret != -EEXIST) {
			free_extent_map(em);
			break;
		}
		btrfs_drop_extent_cache(inode, start, end, 0);
	}
	unlock_extent(&BTRFS_I(inode)->io_tree, start, end, GFP_NOFS);
	return ret;
}

static int relocate_file_extent_cluster(struct inode *inode,
					struct file_extent_cluster *cluster)
{
	u64 page_start;
	u64 page_end;
	u64 offset = BTRFS_I(inode)->index_cnt;
	unsigned long index;
	unsigned long last_index;
	struct page *page;
	struct file_ra_state *ra;
	int nr = 0;
	int ret = 0;

	if (!cluster->nr)
		return 0;

	ra = kzalloc(sizeof(*ra), GFP_NOFS);
	if (!ra)
		return -ENOMEM;

	ret = prealloc_file_extent_cluster(inode, cluster);
	if (ret)
		goto out;

	file_ra_state_init(ra, inode->i_mapping);

	ret = setup_extent_mapping(inode, cluster->start - offset,
				   cluster->end - offset, cluster->start);
	if (ret)
		goto out;

	index = (cluster->start - offset) >> PAGE_CACHE_SHIFT;
	last_index = (cluster->end - offset) >> PAGE_CACHE_SHIFT;
	while (index <= last_index) {
		ret = btrfs_delalloc_reserve_metadata(inode, PAGE_CACHE_SIZE);
		if (ret)
			goto out;

		page = find_lock_page(inode->i_mapping, index);
		if (!page) {
			page_cache_sync_readahead(inode->i_mapping,
						  ra, NULL, index,
						  last_index + 1 - index);
			page = grab_cache_page(inode->i_mapping, index);
			if (!page) {
				btrfs_delalloc_release_metadata(inode,
							PAGE_CACHE_SIZE);
				ret = -ENOMEM;
				goto out;
			}
		}

		if (PageReadahead(page)) {
			page_cache_async_readahead(inode->i_mapping,
						   ra, NULL, page, index,
						   last_index + 1 - index);
		}

		if (!PageUptodate(page)) {
			btrfs_readpage(NULL, page);
			lock_page(page);
			if (!PageUptodate(page)) {
				unlock_page(page);
				page_cache_release(page);
				btrfs_delalloc_release_metadata(inode,
							PAGE_CACHE_SIZE);
				ret = -EIO;
				goto out;
			}
		}

		page_start = (u64)page->index << PAGE_CACHE_SHIFT;
		page_end = page_start + PAGE_CACHE_SIZE - 1;

		lock_extent(&BTRFS_I(inode)->io_tree,
			    page_start, page_end, GFP_NOFS);

		set_page_extent_mapped(page);

		if (nr < cluster->nr &&
		    page_start + offset == cluster->boundary[nr]) {
			set_extent_bits(&BTRFS_I(inode)->io_tree,
					page_start, page_end,
					EXTENT_BOUNDARY, GFP_NOFS);
			nr++;
		}

		btrfs_set_extent_delalloc(inode, page_start, page_end, NULL);
		set_page_dirty(page);

		unlock_extent(&BTRFS_I(inode)->io_tree,
			      page_start, page_end, GFP_NOFS);
		unlock_page(page);
		page_cache_release(page);

		index++;
		balance_dirty_pages_ratelimited(inode->i_mapping);
		btrfs_throttle(BTRFS_I(inode)->root);
	}
	WARN_ON(nr != cluster->nr);
out:
	kfree(ra);
	return ret;
}

static noinline_for_stack
int relocate_data_extent(struct inode *inode, struct btrfs_key *extent_key,
			 struct file_extent_cluster *cluster)
{
	int ret;

	if (cluster->nr > 0 && extent_key->objectid != cluster->end + 1) {
		ret = relocate_file_extent_cluster(inode, cluster);
		if (ret)
			return ret;
		cluster->nr = 0;
	}

	if (!cluster->nr)
		cluster->start = extent_key->objectid;
	else
		BUG_ON(cluster->nr >= MAX_EXTENTS);
	cluster->end = extent_key->objectid + extent_key->offset - 1;
	cluster->boundary[cluster->nr] = extent_key->objectid;
	cluster->nr++;

	if (cluster->nr >= MAX_EXTENTS) {
		ret = relocate_file_extent_cluster(inode, cluster);
		if (ret)
			return ret;
		cluster->nr = 0;
	}
	return 0;
}

#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
static int get_ref_objectid_v0(struct reloc_control *rc,
			       struct btrfs_path *path,
			       struct btrfs_key *extent_key,
			       u64 *ref_objectid, int *path_change)
{
	struct btrfs_key key;
	struct extent_buffer *leaf;
	struct btrfs_extent_ref_v0 *ref0;
	int ret;
	int slot;

	leaf = path->nodes[0];
	slot = path->slots[0];
	while (1) {
		if (slot >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(rc->extent_root, path);
			if (ret < 0)
				return ret;
			BUG_ON(ret > 0);
			leaf = path->nodes[0];
			slot = path->slots[0];
			if (path_change)
				*path_change = 1;
		}
		btrfs_item_key_to_cpu(leaf, &key, slot);
		if (key.objectid != extent_key->objectid)
			return -ENOENT;

		if (key.type != BTRFS_EXTENT_REF_V0_KEY) {
			slot++;
			continue;
		}
		ref0 = btrfs_item_ptr(leaf, slot,
				struct btrfs_extent_ref_v0);
		*ref_objectid = btrfs_ref_objectid_v0(leaf, ref0);
		break;
	}
	return 0;
}
#endif

/*
 * helper to add a tree block to the list.
 * the major work is getting the generation and level of the block
 */
static int add_tree_block(struct reloc_control *rc,
			  struct btrfs_key *extent_key,
			  struct btrfs_path *path,
			  struct rb_root *blocks)
{
	struct extent_buffer *eb;
	struct btrfs_extent_item *ei;
	struct btrfs_tree_block_info *bi;
	struct tree_block *block;
	struct rb_node *rb_node;
	u32 item_size;
	int level = -1;
	int generation;

	eb =  path->nodes[0];
	item_size = btrfs_item_size_nr(eb, path->slots[0]);

	if (item_size >= sizeof(*ei) + sizeof(*bi)) {
		ei = btrfs_item_ptr(eb, path->slots[0],
				struct btrfs_extent_item);
		bi = (struct btrfs_tree_block_info *)(ei + 1);
		generation = btrfs_extent_generation(eb, ei);
		level = btrfs_tree_block_level(eb, bi);
	} else {
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
		u64 ref_owner;
		int ret;

		BUG_ON(item_size != sizeof(struct btrfs_extent_item_v0));
		ret = get_ref_objectid_v0(rc, path, extent_key,
					  &ref_owner, NULL);
		BUG_ON(ref_owner >= BTRFS_MAX_LEVEL);
		level = (int)ref_owner;
		generation = 0;
#else
		BUG();
#endif
	}

	btrfs_release_path(rc->extent_root, path);

	BUG_ON(level == -1);

	block = kmalloc(sizeof(*block), GFP_NOFS);
	if (!block)
		return -ENOMEM;

	block->bytenr = extent_key->objectid;
	block->key.objectid = extent_key->offset;
	block->key.offset = generation;
	block->level = level;
	block->key_ready = 0;

	rb_node = tree_insert(blocks, block->bytenr, &block->rb_node);
	BUG_ON(rb_node);

	return 0;
}

/*
 * helper to add tree blocks for backref of type BTRFS_SHARED_DATA_REF_KEY
 */
static int __add_tree_block(struct reloc_control *rc,
			    u64 bytenr, u32 blocksize,
			    struct rb_root *blocks)
{
	struct btrfs_path *path;
	struct btrfs_key key;
	int ret;

	if (tree_block_processed(bytenr, blocksize, rc))
		return 0;

	if (tree_search(blocks, bytenr))
		return 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	key.objectid = bytenr;
	key.type = BTRFS_EXTENT_ITEM_KEY;
	key.offset = blocksize;

	path->search_commit_root = 1;
	path->skip_locking = 1;
	ret = btrfs_search_slot(NULL, rc->extent_root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	BUG_ON(ret);

	btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
	ret = add_tree_block(rc, &key, path, blocks);
out:
	btrfs_free_path(path);
	return ret;
}

/*
 * helper to check if the block use full backrefs for pointers in it
 */
static int block_use_full_backref(struct reloc_control *rc,
				  struct extent_buffer *eb)
{
	u64 flags;
	int ret;

	if (btrfs_header_flag(eb, BTRFS_HEADER_FLAG_RELOC) ||
	    btrfs_header_backref_rev(eb) < BTRFS_MIXED_BACKREF_REV)
		return 1;

	ret = btrfs_lookup_extent_info(NULL, rc->extent_root,
				       eb->start, eb->len, NULL, &flags);
	BUG_ON(ret);

	if (flags & BTRFS_BLOCK_FLAG_FULL_BACKREF)
		ret = 1;
	else
		ret = 0;
	return ret;
}

/*
 * helper to add tree blocks for backref of type BTRFS_EXTENT_DATA_REF_KEY
 * this function scans fs tree to find blocks reference the data extent
 */
static int find_data_references(struct reloc_control *rc,
				struct btrfs_key *extent_key,
				struct extent_buffer *leaf,
				struct btrfs_extent_data_ref *ref,
				struct rb_root *blocks)
{
	struct btrfs_path *path;
	struct tree_block *block;
	struct btrfs_root *root;
	struct btrfs_file_extent_item *fi;
	struct rb_node *rb_node;
	struct btrfs_key key;
	u64 ref_root;
	u64 ref_objectid;
	u64 ref_offset;
	u32 ref_count;
	u32 nritems;
	int err = 0;
	int added = 0;
	int counted;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ref_root = btrfs_extent_data_ref_root(leaf, ref);
	ref_objectid = btrfs_extent_data_ref_objectid(leaf, ref);
	ref_offset = btrfs_extent_data_ref_offset(leaf, ref);
	ref_count = btrfs_extent_data_ref_count(leaf, ref);

	root = read_fs_root(rc->extent_root->fs_info, ref_root);
	if (IS_ERR(root)) {
		err = PTR_ERR(root);
		goto out;
	}

	key.objectid = ref_objectid;
	key.offset = ref_offset;
	key.type = BTRFS_EXTENT_DATA_KEY;

	path->search_commit_root = 1;
	path->skip_locking = 1;
	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0) {
		err = ret;
		goto out;
	}

	leaf = path->nodes[0];
	nritems = btrfs_header_nritems(leaf);
	/*
	 * the references in tree blocks that use full backrefs
	 * are not counted in
	 */
	if (block_use_full_backref(rc, leaf))
		counted = 0;
	else
		counted = 1;
	rb_node = tree_search(blocks, leaf->start);
	if (rb_node) {
		if (counted)
			added = 1;
		else
			path->slots[0] = nritems;
	}

	while (ref_count > 0) {
		while (path->slots[0] >= nritems) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0) {
				err = ret;
				goto out;
			}
			if (ret > 0) {
				WARN_ON(1);
				goto out;
			}

			leaf = path->nodes[0];
			nritems = btrfs_header_nritems(leaf);
			added = 0;

			if (block_use_full_backref(rc, leaf))
				counted = 0;
			else
				counted = 1;
			rb_node = tree_search(blocks, leaf->start);
			if (rb_node) {
				if (counted)
					added = 1;
				else
					path->slots[0] = nritems;
			}
		}

		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		if (key.objectid != ref_objectid ||
		    key.type != BTRFS_EXTENT_DATA_KEY) {
			WARN_ON(1);
			break;
		}

		fi = btrfs_item_ptr(leaf, path->slots[0],
				    struct btrfs_file_extent_item);

		if (btrfs_file_extent_type(leaf, fi) ==
		    BTRFS_FILE_EXTENT_INLINE)
			goto next;

		if (btrfs_file_extent_disk_bytenr(leaf, fi) !=
		    extent_key->objectid)
			goto next;

		key.offset -= btrfs_file_extent_offset(leaf, fi);
		if (key.offset != ref_offset)
			goto next;

		if (counted)
			ref_count--;
		if (added)
			goto next;

		if (!tree_block_processed(leaf->start, leaf->len, rc)) {
			block = kmalloc(sizeof(*block), GFP_NOFS);
			if (!block) {
				err = -ENOMEM;
				break;
			}
			block->bytenr = leaf->start;
			btrfs_item_key_to_cpu(leaf, &block->key, 0);
			block->level = 0;
			block->key_ready = 1;
			rb_node = tree_insert(blocks, block->bytenr,
					      &block->rb_node);
			BUG_ON(rb_node);
		}
		if (counted)
			added = 1;
		else
			path->slots[0] = nritems;
next:
		path->slots[0]++;

	}
out:
	btrfs_free_path(path);
	return err;
}

/*
 * hepler to find all tree blocks that reference a given data extent
 */
static noinline_for_stack
int add_data_references(struct reloc_control *rc,
			struct btrfs_key *extent_key,
			struct btrfs_path *path,
			struct rb_root *blocks)
{
	struct btrfs_key key;
	struct extent_buffer *eb;
	struct btrfs_extent_data_ref *dref;
	struct btrfs_extent_inline_ref *iref;
	unsigned long ptr;
	unsigned long end;
	u32 blocksize = btrfs_level_size(rc->extent_root, 0);
	int ret;
	int err = 0;

	eb = path->nodes[0];
	ptr = btrfs_item_ptr_offset(eb, path->slots[0]);
	end = ptr + btrfs_item_size_nr(eb, path->slots[0]);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	if (ptr + sizeof(struct btrfs_extent_item_v0) == end)
		ptr = end;
	else
#endif
		ptr += sizeof(struct btrfs_extent_item);

	while (ptr < end) {
		iref = (struct btrfs_extent_inline_ref *)ptr;
		key.type = btrfs_extent_inline_ref_type(eb, iref);
		if (key.type == BTRFS_SHARED_DATA_REF_KEY) {
			key.offset = btrfs_extent_inline_ref_offset(eb, iref);
			ret = __add_tree_block(rc, key.offset, blocksize,
					       blocks);
		} else if (key.type == BTRFS_EXTENT_DATA_REF_KEY) {
			dref = (struct btrfs_extent_data_ref *)(&iref->offset);
			ret = find_data_references(rc, extent_key,
						   eb, dref, blocks);
		} else {
			BUG();
		}
		ptr += btrfs_extent_inline_ref_size(key.type);
	}
	WARN_ON(ptr > end);

	while (1) {
		cond_resched();
		eb = path->nodes[0];
		if (path->slots[0] >= btrfs_header_nritems(eb)) {
			ret = btrfs_next_leaf(rc->extent_root, path);
			if (ret < 0) {
				err = ret;
				break;
			}
			if (ret > 0)
				break;
			eb = path->nodes[0];
		}

		btrfs_item_key_to_cpu(eb, &key, path->slots[0]);
		if (key.objectid != extent_key->objectid)
			break;

#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
		if (key.type == BTRFS_SHARED_DATA_REF_KEY ||
		    key.type == BTRFS_EXTENT_REF_V0_KEY) {
#else
		BUG_ON(key.type == BTRFS_EXTENT_REF_V0_KEY);
		if (key.type == BTRFS_SHARED_DATA_REF_KEY) {
#endif
			ret = __add_tree_block(rc, key.offset, blocksize,
					       blocks);
		} else if (key.type == BTRFS_EXTENT_DATA_REF_KEY) {
			dref = btrfs_item_ptr(eb, path->slots[0],
					      struct btrfs_extent_data_ref);
			ret = find_data_references(rc, extent_key,
						   eb, dref, blocks);
		} else {
			ret = 0;
		}
		if (ret) {
			err = ret;
			break;
		}
		path->slots[0]++;
	}
	btrfs_release_path(rc->extent_root, path);
	if (err)
		free_block_list(blocks);
	return err;
}

/*
 * hepler to find next unprocessed extent
 */
static noinline_for_stack
int find_next_extent(struct btrfs_trans_handle *trans,
		     struct reloc_control *rc, struct btrfs_path *path,
		     struct btrfs_key *extent_key)
{
	struct btrfs_key key;
	struct extent_buffer *leaf;
	u64 start, end, last;
	int ret;

	last = rc->block_group->key.objectid + rc->block_group->key.offset;
	while (1) {
		cond_resched();
		if (rc->search_start >= last) {
			ret = 1;
			break;
		}

		key.objectid = rc->search_start;
		key.type = BTRFS_EXTENT_ITEM_KEY;
		key.offset = 0;

		path->search_commit_root = 1;
		path->skip_locking = 1;
		ret = btrfs_search_slot(NULL, rc->extent_root, &key, path,
					0, 0);
		if (ret < 0)
			break;
next:
		leaf = path->nodes[0];
		if (path->slots[0] >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(rc->extent_root, path);
			if (ret != 0)
				break;
			leaf = path->nodes[0];
		}

		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		if (key.objectid >= last) {
			ret = 1;
			break;
		}

		if (key.type != BTRFS_EXTENT_ITEM_KEY ||
		    key.objectid + key.offset <= rc->search_start) {
			path->slots[0]++;
			goto next;
		}

		ret = find_first_extent_bit(&rc->processed_blocks,
					    key.objectid, &start, &end,
					    EXTENT_DIRTY);

		if (ret == 0 && start <= key.objectid) {
			btrfs_release_path(rc->extent_root, path);
			rc->search_start = end + 1;
		} else {
			rc->search_start = key.objectid + key.offset;
			memcpy(extent_key, &key, sizeof(key));
			return 0;
		}
	}
	btrfs_release_path(rc->extent_root, path);
	return ret;
}

static void set_reloc_control(struct reloc_control *rc)
{
	struct btrfs_fs_info *fs_info = rc->extent_root->fs_info;
	mutex_lock(&fs_info->trans_mutex);
	fs_info->reloc_ctl = rc;
	mutex_unlock(&fs_info->trans_mutex);
}

static void unset_reloc_control(struct reloc_control *rc)
{
	struct btrfs_fs_info *fs_info = rc->extent_root->fs_info;
	mutex_lock(&fs_info->trans_mutex);
	fs_info->reloc_ctl = NULL;
	mutex_unlock(&fs_info->trans_mutex);
}

static int check_extent_flags(u64 flags)
{
	if ((flags & BTRFS_EXTENT_FLAG_DATA) &&
	    (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK))
		return 1;
	if (!(flags & BTRFS_EXTENT_FLAG_DATA) &&
	    !(flags & BTRFS_EXTENT_FLAG_TREE_BLOCK))
		return 1;
	if ((flags & BTRFS_EXTENT_FLAG_DATA) &&
	    (flags & BTRFS_BLOCK_FLAG_FULL_BACKREF))
		return 1;
	return 0;
}

static noinline_for_stack
int prepare_to_relocate(struct reloc_control *rc)
{
	struct btrfs_trans_handle *trans;
	int ret;

	rc->block_rsv = btrfs_alloc_block_rsv(rc->extent_root);
	if (!rc->block_rsv)
		return -ENOMEM;

	/*
	 * reserve some space for creating reloc trees.
	 * btrfs_init_reloc_root will use them when there
	 * is no reservation in transaction handle.
	 */
	ret = btrfs_block_rsv_add(NULL, rc->extent_root, rc->block_rsv,
				  rc->extent_root->nodesize * 256,
				  &rc->block_rsv_retries);
	if (ret)
		return ret;

	rc->block_rsv->refill_used = 1;
	btrfs_add_durable_block_rsv(rc->extent_root->fs_info, rc->block_rsv);

	memset(&rc->cluster, 0, sizeof(rc->cluster));
	rc->search_start = rc->block_group->key.objectid;
	rc->extents_found = 0;
	rc->nodes_relocated = 0;
	rc->merging_rsv_size = 0;
	rc->block_rsv_retries = 0;

	rc->create_reloc_tree = 1;
	set_reloc_control(rc);

	trans = btrfs_join_transaction(rc->extent_root, 1);
	btrfs_commit_transaction(trans, rc->extent_root);
	return 0;
}

static noinline_for_stack int relocate_block_group(struct reloc_control *rc)
{
	struct rb_root blocks = RB_ROOT;
	struct btrfs_key key;
	struct btrfs_trans_handle *trans = NULL;
	struct btrfs_path *path;
	struct btrfs_extent_item *ei;
	unsigned long nr;
	u64 flags;
	u32 item_size;
	int ret;
	int err = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = prepare_to_relocate(rc);
	if (ret) {
		err = ret;
		goto out_free;
	}

	while (1) {
		trans = btrfs_start_transaction(rc->extent_root, 0);

		if (update_backref_cache(trans, &rc->backref_cache)) {
			btrfs_end_transaction(trans, rc->extent_root);
			continue;
		}

		ret = find_next_extent(trans, rc, path, &key);
		if (ret < 0)
			err = ret;
		if (ret != 0)
			break;

		rc->extents_found++;

		ei = btrfs_item_ptr(path->nodes[0], path->slots[0],
				    struct btrfs_extent_item);
		item_size = btrfs_item_size_nr(path->nodes[0], path->slots[0]);
		if (item_size >= sizeof(*ei)) {
			flags = btrfs_extent_flags(path->nodes[0], ei);
			ret = check_extent_flags(flags);
			BUG_ON(ret);

		} else {
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
			u64 ref_owner;
			int path_change = 0;

			BUG_ON(item_size !=
			       sizeof(struct btrfs_extent_item_v0));
			ret = get_ref_objectid_v0(rc, path, &key, &ref_owner,
						  &path_change);
			if (ref_owner < BTRFS_FIRST_FREE_OBJECTID)
				flags = BTRFS_EXTENT_FLAG_TREE_BLOCK;
			else
				flags = BTRFS_EXTENT_FLAG_DATA;

			if (path_change) {
				btrfs_release_path(rc->extent_root, path);

				path->search_commit_root = 1;
				path->skip_locking = 1;
				ret = btrfs_search_slot(NULL, rc->extent_root,
							&key, path, 0, 0);
				if (ret < 0) {
					err = ret;
					break;
				}
				BUG_ON(ret > 0);
			}
#else
			BUG();
#endif
		}

		if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK) {
			ret = add_tree_block(rc, &key, path, &blocks);
		} else if (rc->stage == UPDATE_DATA_PTRS &&
			   (flags & BTRFS_EXTENT_FLAG_DATA)) {
			ret = add_data_references(rc, &key, path, &blocks);
		} else {
			btrfs_release_path(rc->extent_root, path);
			ret = 0;
		}
		if (ret < 0) {
			err = ret;
			break;
		}

		if (!RB_EMPTY_ROOT(&blocks)) {
			ret = relocate_tree_blocks(trans, rc, &blocks);
			if (ret < 0) {
				if (ret != -EAGAIN) {
					err = ret;
					break;
				}
				rc->extents_found--;
				rc->search_start = key.objectid;
			}
		}

		ret = btrfs_block_rsv_check(trans, rc->extent_root,
					    rc->block_rsv, 0, 5);
		if (ret < 0) {
			if (ret != -EAGAIN) {
				err = ret;
				WARN_ON(1);
				break;
			}
			rc->commit_transaction = 1;
		}

		if (rc->commit_transaction) {
			rc->commit_transaction = 0;
			ret = btrfs_commit_transaction(trans, rc->extent_root);
			BUG_ON(ret);
		} else {
			nr = trans->blocks_used;
			btrfs_end_transaction_throttle(trans, rc->extent_root);
			btrfs_btree_balance_dirty(rc->extent_root, nr);
		}
		trans = NULL;

		if (rc->stage == MOVE_DATA_EXTENTS &&
		    (flags & BTRFS_EXTENT_FLAG_DATA)) {
			rc->found_file_extent = 1;
			ret = relocate_data_extent(rc->data_inode,
						   &key, &rc->cluster);
			if (ret < 0) {
				err = ret;
				break;
			}
		}
	}

	btrfs_release_path(rc->extent_root, path);
	clear_extent_bits(&rc->processed_blocks, 0, (u64)-1, EXTENT_DIRTY,
			  GFP_NOFS);

	if (trans) {
		nr = trans->blocks_used;
		btrfs_end_transaction_throttle(trans, rc->extent_root);
		btrfs_btree_balance_dirty(rc->extent_root, nr);
	}

	if (!err) {
		ret = relocate_file_extent_cluster(rc->data_inode,
						   &rc->cluster);
		if (ret < 0)
			err = ret;
	}

	rc->create_reloc_tree = 0;
	set_reloc_control(rc);

	backref_cache_cleanup(&rc->backref_cache);
	btrfs_block_rsv_release(rc->extent_root, rc->block_rsv, (u64)-1);

	err = prepare_to_merge(rc, err);

	merge_reloc_roots(rc);

	rc->merge_reloc_tree = 0;
	unset_reloc_control(rc);
	btrfs_block_rsv_release(rc->extent_root, rc->block_rsv, (u64)-1);

	/* get rid of pinned extents */
	trans = btrfs_join_transaction(rc->extent_root, 1);
	btrfs_commit_transaction(trans, rc->extent_root);
out_free:
	btrfs_free_block_rsv(rc->extent_root, rc->block_rsv);
	btrfs_free_path(path);
	return err;
}

static int __insert_orphan_inode(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root, u64 objectid)
{
	struct btrfs_path *path;
	struct btrfs_inode_item *item;
	struct extent_buffer *leaf;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = btrfs_insert_empty_inode(trans, root, path, objectid);
	if (ret)
		goto out;

	leaf = path->nodes[0];
	item = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_inode_item);
	memset_extent_buffer(leaf, 0, (unsigned long)item, sizeof(*item));
	btrfs_set_inode_generation(leaf, item, 1);
	btrfs_set_inode_size(leaf, item, 0);
	btrfs_set_inode_mode(leaf, item, S_IFREG | 0600);
	btrfs_set_inode_flags(leaf, item, BTRFS_INODE_NOCOMPRESS |
					  BTRFS_INODE_PREALLOC);
	btrfs_mark_buffer_dirty(leaf);
	btrfs_release_path(root, path);
out:
	btrfs_free_path(path);
	return ret;
}

/*
 * helper to create inode for data relocation.
 * the inode is in data relocation tree and its link count is 0
 */
static noinline_for_stack
struct inode *create_reloc_inode(struct btrfs_fs_info *fs_info,
				 struct btrfs_block_group_cache *group)
{
	struct inode *inode = NULL;
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root;
	struct btrfs_key key;
	unsigned long nr;
	u64 objectid = BTRFS_FIRST_FREE_OBJECTID;
	int err = 0;

	root = read_fs_root(fs_info, BTRFS_DATA_RELOC_TREE_OBJECTID);
	if (IS_ERR(root))
		return ERR_CAST(root);

	trans = btrfs_start_transaction(root, 6);
	if (IS_ERR(trans))
		return ERR_CAST(trans);

	err = btrfs_find_free_objectid(trans, root, objectid, &objectid);
	if (err)
		goto out;

	err = __insert_orphan_inode(trans, root, objectid);
	BUG_ON(err);

	key.objectid = objectid;
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;
	inode = btrfs_iget(root->fs_info->sb, &key, root, NULL);
	BUG_ON(IS_ERR(inode) || is_bad_inode(inode));
	BTRFS_I(inode)->index_cnt = group->key.objectid;

	err = btrfs_orphan_add(trans, inode);
out:
	nr = trans->blocks_used;
	btrfs_end_transaction(trans, root);
	btrfs_btree_balance_dirty(root, nr);
	if (err) {
		if (inode)
			iput(inode);
		inode = ERR_PTR(err);
	}
	return inode;
}

static struct reloc_control *alloc_reloc_control(void)
{
	struct reloc_control *rc;

	rc = kzalloc(sizeof(*rc), GFP_NOFS);
	if (!rc)
		return NULL;

	INIT_LIST_HEAD(&rc->reloc_roots);
	backref_cache_init(&rc->backref_cache);
	mapping_tree_init(&rc->reloc_root_tree);
	extent_io_tree_init(&rc->processed_blocks, NULL, GFP_NOFS);
	return rc;
}

/*
 * function to relocate all extents in a block group.
 */
int btrfs_relocate_block_group(struct btrfs_root *extent_root, u64 group_start)
{
	struct btrfs_fs_info *fs_info = extent_root->fs_info;
	struct reloc_control *rc;
	int ret;
	int rw = 0;
	int err = 0;

	rc = alloc_reloc_control();
	if (!rc)
		return -ENOMEM;

	rc->extent_root = extent_root;

	rc->block_group = btrfs_lookup_block_group(fs_info, group_start);
	BUG_ON(!rc->block_group);

	if (!rc->block_group->ro) {
		ret = btrfs_set_block_group_ro(extent_root, rc->block_group);
		if (ret) {
			err = ret;
			goto out;
		}
		rw = 1;
	}

	rc->data_inode = create_reloc_inode(fs_info, rc->block_group);
	if (IS_ERR(rc->data_inode)) {
		err = PTR_ERR(rc->data_inode);
		rc->data_inode = NULL;
		goto out;
	}

	printk(KERN_INFO "btrfs: relocating block group %llu flags %llu\n",
	       (unsigned long long)rc->block_group->key.objectid,
	       (unsigned long long)rc->block_group->flags);

	btrfs_start_delalloc_inodes(fs_info->tree_root, 0);
	btrfs_wait_ordered_extents(fs_info->tree_root, 0, 0);

	while (1) {
		mutex_lock(&fs_info->cleaner_mutex);

		btrfs_clean_old_snapshots(fs_info->tree_root);
		ret = relocate_block_group(rc);

		mutex_unlock(&fs_info->cleaner_mutex);
		if (ret < 0) {
			err = ret;
			goto out;
		}

		if (rc->extents_found == 0)
			break;

		printk(KERN_INFO "btrfs: found %llu extents\n",
			(unsigned long long)rc->extents_found);

		if (rc->stage == MOVE_DATA_EXTENTS && rc->found_file_extent) {
			btrfs_wait_ordered_range(rc->data_inode, 0, (u64)-1);
			invalidate_mapping_pages(rc->data_inode->i_mapping,
						 0, -1);
			rc->stage = UPDATE_DATA_PTRS;
		}
	}

	filemap_write_and_wait_range(fs_info->btree_inode->i_mapping,
				     rc->block_group->key.objectid,
				     rc->block_group->key.objectid +
				     rc->block_group->key.offset - 1);

	WARN_ON(rc->block_group->pinned > 0);
	WARN_ON(rc->block_group->reserved > 0);
	WARN_ON(btrfs_block_group_used(&rc->block_group->item) > 0);
out:
	if (err && rw)
		btrfs_set_block_group_rw(extent_root, rc->block_group);
	iput(rc->data_inode);
	btrfs_put_block_group(rc->block_group);
	kfree(rc);
	return err;
}

static noinline_for_stack int mark_garbage_root(struct btrfs_root *root)
{
	struct btrfs_trans_handle *trans;
	int ret;

	trans = btrfs_start_transaction(root->fs_info->tree_root, 0);

	memset(&root->root_item.drop_progress, 0,
		sizeof(root->root_item.drop_progress));
	root->root_item.drop_level = 0;
	btrfs_set_root_refs(&root->root_item, 0);
	ret = btrfs_update_root(trans, root->fs_info->tree_root,
				&root->root_key, &root->root_item);
	BUG_ON(ret);

	ret = btrfs_end_transaction(trans, root->fs_info->tree_root);
	BUG_ON(ret);
	return 0;
}

/*
 * recover relocation interrupted by system crash.
 *
 * this function resumes merging reloc trees with corresponding fs trees.
 * this is important for keeping the sharing of tree blocks
 */
int btrfs_recover_relocation(struct btrfs_root *root)
{
	LIST_HEAD(reloc_roots);
	struct btrfs_key key;
	struct btrfs_root *fs_root;
	struct btrfs_root *reloc_root;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct reloc_control *rc = NULL;
	struct btrfs_trans_handle *trans;
	int ret;
	int err = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	key.objectid = BTRFS_TREE_RELOC_OBJECTID;
	key.type = BTRFS_ROOT_ITEM_KEY;
	key.offset = (u64)-1;

	while (1) {
		ret = btrfs_search_slot(NULL, root->fs_info->tree_root, &key,
					path, 0, 0);
		if (ret < 0) {
			err = ret;
			goto out;
		}
		if (ret > 0) {
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		btrfs_release_path(root->fs_info->tree_root, path);

		if (key.objectid != BTRFS_TREE_RELOC_OBJECTID ||
		    key.type != BTRFS_ROOT_ITEM_KEY)
			break;

		reloc_root = btrfs_read_fs_root_no_radix(root, &key);
		if (IS_ERR(reloc_root)) {
			err = PTR_ERR(reloc_root);
			goto out;
		}

		list_add(&reloc_root->root_list, &reloc_roots);

		if (btrfs_root_refs(&reloc_root->root_item) > 0) {
			fs_root = read_fs_root(root->fs_info,
					       reloc_root->root_key.offset);
			if (IS_ERR(fs_root)) {
				ret = PTR_ERR(fs_root);
				if (ret != -ENOENT) {
					err = ret;
					goto out;
				}
				mark_garbage_root(reloc_root);
			}
		}

		if (key.offset == 0)
			break;

		key.offset--;
	}
	btrfs_release_path(root->fs_info->tree_root, path);

	if (list_empty(&reloc_roots))
		goto out;

	rc = alloc_reloc_control();
	if (!rc) {
		err = -ENOMEM;
		goto out;
	}

	rc->extent_root = root->fs_info->extent_root;

	set_reloc_control(rc);

	trans = btrfs_join_transaction(rc->extent_root, 1);

	rc->merge_reloc_tree = 1;

	while (!list_empty(&reloc_roots)) {
		reloc_root = list_entry(reloc_roots.next,
					struct btrfs_root, root_list);
		list_del(&reloc_root->root_list);

		if (btrfs_root_refs(&reloc_root->root_item) == 0) {
			list_add_tail(&reloc_root->root_list,
				      &rc->reloc_roots);
			continue;
		}

		fs_root = read_fs_root(root->fs_info,
				       reloc_root->root_key.offset);
		BUG_ON(IS_ERR(fs_root));

		__add_reloc_root(reloc_root);
		fs_root->reloc_root = reloc_root;
	}

	btrfs_commit_transaction(trans, rc->extent_root);

	merge_reloc_roots(rc);

	unset_reloc_control(rc);

	trans = btrfs_join_transaction(rc->extent_root, 1);
	btrfs_commit_transaction(trans, rc->extent_root);
out:
	kfree(rc);
	while (!list_empty(&reloc_roots)) {
		reloc_root = list_entry(reloc_roots.next,
					struct btrfs_root, root_list);
		list_del(&reloc_root->root_list);
		free_extent_buffer(reloc_root->node);
		free_extent_buffer(reloc_root->commit_root);
		kfree(reloc_root);
	}
	btrfs_free_path(path);

	if (err == 0) {
		/* cleanup orphan inode in data relocation tree */
		fs_root = read_fs_root(root->fs_info,
				       BTRFS_DATA_RELOC_TREE_OBJECTID);
		if (IS_ERR(fs_root))
			err = PTR_ERR(fs_root);
		else
			btrfs_orphan_cleanup(fs_root);
	}
	return err;
}

/*
 * helper to add ordered checksum for data relocation.
 *
 * cloning checksum properly handles the nodatasum extents.
 * it also saves CPU time to re-calculate the checksum.
 */
int btrfs_reloc_clone_csums(struct inode *inode, u64 file_pos, u64 len)
{
	struct btrfs_ordered_sum *sums;
	struct btrfs_sector_sum *sector_sum;
	struct btrfs_ordered_extent *ordered;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	size_t offset;
	int ret;
	u64 disk_bytenr;
	LIST_HEAD(list);

	ordered = btrfs_lookup_ordered_extent(inode, file_pos);
	BUG_ON(ordered->file_offset != file_pos || ordered->len != len);

	disk_bytenr = file_pos + BTRFS_I(inode)->index_cnt;
	ret = btrfs_lookup_csums_range(root->fs_info->csum_root, disk_bytenr,
				       disk_bytenr + len - 1, &list);

	while (!list_empty(&list)) {
		sums = list_entry(list.next, struct btrfs_ordered_sum, list);
		list_del_init(&sums->list);

		sector_sum = sums->sums;
		sums->bytenr = ordered->start;

		offset = 0;
		while (offset < sums->len) {
			sector_sum->bytenr += ordered->start - disk_bytenr;
			sector_sum++;
			offset += root->sectorsize;
		}

		btrfs_add_ordered_sum(inode, ordered, sums);
	}
	btrfs_put_ordered_extent(ordered);
	return 0;
}

void btrfs_reloc_cow_block(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root, struct extent_buffer *buf,
			   struct extent_buffer *cow)
{
	struct reloc_control *rc;
	struct backref_node *node;
	int first_cow = 0;
	int level;
	int ret;

	rc = root->fs_info->reloc_ctl;
	if (!rc)
		return;

	BUG_ON(rc->stage == UPDATE_DATA_PTRS &&
	       root->root_key.objectid == BTRFS_DATA_RELOC_TREE_OBJECTID);

	level = btrfs_header_level(buf);
	if (btrfs_header_generation(buf) <=
	    btrfs_root_last_snapshot(&root->root_item))
		first_cow = 1;

	if (root->root_key.objectid == BTRFS_TREE_RELOC_OBJECTID &&
	    rc->create_reloc_tree) {
		WARN_ON(!first_cow && level == 0);

		node = rc->backref_cache.path[level];
		BUG_ON(node->bytenr != buf->start &&
		       node->new_bytenr != buf->start);

		drop_node_buffer(node);
		extent_buffer_get(cow);
		node->eb = cow;
		node->new_bytenr = cow->start;

		if (!node->pending) {
			list_move_tail(&node->list,
				       &rc->backref_cache.pending[level]);
			node->pending = 1;
		}

		if (first_cow)
			__mark_block_processed(rc, node);

		if (first_cow && level > 0)
			rc->nodes_relocated += buf->len;
	}

	if (level == 0 && first_cow && rc->stage == UPDATE_DATA_PTRS) {
		ret = replace_file_extents(trans, rc, root, cow);
		BUG_ON(ret);
	}
}

/*
 * called before creating snapshot. it calculates metadata reservation
 * requried for relocating tree blocks in the snapshot
 */
void btrfs_reloc_pre_snapshot(struct btrfs_trans_handle *trans,
			      struct btrfs_pending_snapshot *pending,
			      u64 *bytes_to_reserve)
{
	struct btrfs_root *root;
	struct reloc_control *rc;

	root = pending->root;
	if (!root->reloc_root)
		return;

	rc = root->fs_info->reloc_ctl;
	if (!rc->merge_reloc_tree)
		return;

	root = root->reloc_root;
	BUG_ON(btrfs_root_refs(&root->root_item) == 0);
	/*
	 * relocation is in the stage of merging trees. the space
	 * used by merging a reloc tree is twice the size of
	 * relocated tree nodes in the worst case. half for cowing
	 * the reloc tree, half for cowing the fs tree. the space
	 * used by cowing the reloc tree will be freed after the
	 * tree is dropped. if we create snapshot, cowing the fs
	 * tree may use more space than it frees. so we need
	 * reserve extra space.
	 */
	*bytes_to_reserve += rc->nodes_relocated;
}

/*
 * called after snapshot is created. migrate block reservation
 * and create reloc root for the newly created snapshot
 */
void btrfs_reloc_post_snapshot(struct btrfs_trans_handle *trans,
			       struct btrfs_pending_snapshot *pending)
{
	struct btrfs_root *root = pending->root;
	struct btrfs_root *reloc_root;
	struct btrfs_root *new_root;
	struct reloc_control *rc;
	int ret;

	if (!root->reloc_root)
		return;

	rc = root->fs_info->reloc_ctl;
	rc->merging_rsv_size += rc->nodes_relocated;

	if (rc->merge_reloc_tree) {
		ret = btrfs_block_rsv_migrate(&pending->block_rsv,
					      rc->block_rsv,
					      rc->nodes_relocated);
		BUG_ON(ret);
	}

	new_root = pending->snap;
	reloc_root = create_reloc_root(trans, root->reloc_root,
				       new_root->root_key.objectid);

	__add_reloc_root(reloc_root);
	new_root->reloc_root = reloc_root;

	if (rc->create_reloc_tree) {
		ret = clone_backref_node(trans, rc, root, reloc_root);
		BUG_ON(ret);
	}
}
