/* vi: set sw=4 ts=4: */
/*
 * e2fsck
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 * Copyright (C) 2006 Garrett Kajmowicz
 *
 * Dictionary Abstract Data Type
 * Copyright (C) 1997 Kaz Kylheku <kaz@ashi.footprints.net>
 * Free Software License:
 * All rights are reserved by the author, with the following exceptions:
 * Permission is granted to freely reproduce and distribute this software,
 * possibly in exchange for a fee, provided that this copyright notice appears
 * intact. Permission is also granted to adapt this software to produce
 * derivative works, as long as the modified versions carry this copyright
 * notice and additional notices stating that the work has been modified.
 * This source code may be translated into executable form and incorporated
 * into proprietary software; there is no requirement for such software to
 * contain a copyright notice related to this source.
 *
 * linux/fs/recovery  and linux/fs/revoke
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1999
 *
 * Copyright 1999-2000 Red Hat Software --- All Rights Reserved
 *
 * Journal recovery routines for the generic filesystem journaling code;
 * part of the ext2fs journaling system.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/*
//usage:#define e2fsck_trivial_usage
//usage:       "[-panyrcdfvstDFSV] [-b superblock] [-B blocksize] "
//usage:       "[-I inode_buffer_blocks] [-P process_inode_size] "
//usage:       "[-l|-L bad_blocks_file] [-C fd] [-j external_journal] "
//usage:       "[-E extended-options] device"
//usage:#define e2fsck_full_usage "\n\n"
//usage:       "Check ext2/ext3 file system\n"
//usage:     "\n	-p		Automatic repair (no questions)"
//usage:     "\n	-n		Make no changes to the filesystem"
//usage:     "\n	-y		Assume 'yes' to all questions"
//usage:     "\n	-c		Check for bad blocks and add them to the badblock list"
//usage:     "\n	-f		Force checking even if filesystem is marked clean"
//usage:     "\n	-v		Verbose"
//usage:     "\n	-b superblock	Use alternative superblock"
//usage:     "\n	-B blocksize	Force blocksize when looking for superblock"
//usage:     "\n	-j journal	Set location of the external journal"
//usage:     "\n	-l file		Add to badblocks list"
//usage:     "\n	-L file		Set badblocks list"
*/

#include "e2fsck.h"	/*Put all of our defines here to clean things up*/

#define _(x) x
#define N_(x) x

/*
 * Procedure declarations
 */

static void e2fsck_pass1_dupblocks(e2fsck_t ctx, char *block_buf);

/* pass1.c */
static void e2fsck_use_inode_shortcuts(e2fsck_t ctx, int bool);

/* pass2.c */
static int e2fsck_process_bad_inode(e2fsck_t ctx, ext2_ino_t dir,
				    ext2_ino_t ino, char *buf);

/* pass3.c */
static int e2fsck_reconnect_file(e2fsck_t ctx, ext2_ino_t inode);
static errcode_t e2fsck_expand_directory(e2fsck_t ctx, ext2_ino_t dir,
					 int num, int gauranteed_size);
static ext2_ino_t e2fsck_get_lost_and_found(e2fsck_t ctx, int fix);
static errcode_t e2fsck_adjust_inode_count(e2fsck_t ctx, ext2_ino_t ino,
					   int adj);

/* rehash.c */
static void e2fsck_rehash_directories(e2fsck_t ctx);

/* util.c */
static void *e2fsck_allocate_memory(e2fsck_t ctx, unsigned int size,
				    const char *description);
static int ask(e2fsck_t ctx, const char * string, int def);
static void e2fsck_read_bitmaps(e2fsck_t ctx);
static void preenhalt(e2fsck_t ctx);
static void e2fsck_read_inode(e2fsck_t ctx, unsigned long ino,
			      struct ext2_inode * inode, const char * proc);
static void e2fsck_write_inode(e2fsck_t ctx, unsigned long ino,
			       struct ext2_inode * inode, const char * proc);
static blk_t get_backup_sb(e2fsck_t ctx, ext2_filsys fs,
			   const char *name, io_manager manager);

/* unix.c */
static void e2fsck_clear_progbar(e2fsck_t ctx);
static int e2fsck_simple_progress(e2fsck_t ctx, const char *label,
				  float percent, unsigned int dpynum);


/*
 * problem.h --- e2fsck problem error codes
 */

typedef __u32 problem_t;

struct problem_context {
	errcode_t       errcode;
	ext2_ino_t      ino, ino2, dir;
	struct ext2_inode *inode;
	struct ext2_dir_entry *dirent;
	blk_t           blk, blk2;
	e2_blkcnt_t     blkcount;
	int             group;
	__u64           num;
	const char      *str;
};


/*
 * Function declarations
 */
static int fix_problem(e2fsck_t ctx, problem_t code, struct problem_context *pctx);
static int end_problem_latch(e2fsck_t ctx, int mask);
static int set_latch_flags(int mask, int setflags, int clearflags);
static void clear_problem_context(struct problem_context *ctx);

/*
 * Dictionary Abstract Data Type
 * Copyright (C) 1997 Kaz Kylheku <kaz@ashi.footprints.net>
 *
 * dict.h v 1.22.2.6 2000/11/13 01:36:44 kaz
 * kazlib_1_20
 */

#ifndef DICT_H
#define DICT_H

/*
 * Blurb for inclusion into C++ translation units
 */

typedef unsigned long dictcount_t;
#define DICTCOUNT_T_MAX ULONG_MAX

/*
 * The dictionary is implemented as a red-black tree
 */

typedef enum { dnode_red, dnode_black } dnode_color_t;

typedef struct dnode_t {
	struct dnode_t *dict_left;
	struct dnode_t *dict_right;
	struct dnode_t *dict_parent;
	dnode_color_t dict_color;
	const void *dict_key;
	void *dict_data;
} dnode_t;

typedef int (*dict_comp_t)(const void *, const void *);
typedef void (*dnode_free_t)(dnode_t *);

typedef struct dict_t {
	dnode_t dict_nilnode;
	dictcount_t dict_nodecount;
	dictcount_t dict_maxcount;
	dict_comp_t dict_compare;
	dnode_free_t dict_freenode;
	int dict_dupes;
} dict_t;

typedef void (*dnode_process_t)(dict_t *, dnode_t *, void *);

typedef struct dict_load_t {
	dict_t *dict_dictptr;
	dnode_t dict_nilnode;
} dict_load_t;

#define dict_count(D) ((D)->dict_nodecount)
#define dnode_get(N) ((N)->dict_data)
#define dnode_getkey(N) ((N)->dict_key)

#endif

/*
 * Compatibility header file for e2fsck which should be included
 * instead of linux/jfs.h
 *
 * Copyright (C) 2000 Stephen C. Tweedie
 */

/*
 * Pull in the definition of the e2fsck context structure
 */

struct buffer_head {
	char            b_data[8192];
	e2fsck_t        b_ctx;
	io_channel      b_io;
	int             b_size;
	blk_t           b_blocknr;
	int             b_dirty;
	int             b_uptodate;
	int             b_err;
};


#define K_DEV_FS        1
#define K_DEV_JOURNAL   2

#define lock_buffer(bh) do {} while (0)
#define unlock_buffer(bh) do {} while (0)
#define buffer_req(bh) 1
#define do_readahead(journal, start) do {} while (0)

static e2fsck_t e2fsck_global_ctx;  /* Try your very best not to use this! */

typedef struct {
	int     object_length;
} kmem_cache_t;

#define kmem_cache_alloc(cache,flags) malloc((cache)->object_length)

/*
 * We use the standard libext2fs portability tricks for inline
 * functions.
 */

static kmem_cache_t * do_cache_create(int len)
{
	kmem_cache_t *new_cache;

	new_cache = xmalloc(sizeof(*new_cache));
	new_cache->object_length = len;
	return new_cache;
}

static void do_cache_destroy(kmem_cache_t *cache)
{
	free(cache);
}


/*
 * Dictionary Abstract Data Type
 */


/*
 * These macros provide short convenient names for structure members,
 * which are embellished with dict_ prefixes so that they are
 * properly confined to the documented namespace. It's legal for a
 * program which uses dict to define, for instance, a macro called ``parent''.
 * Such a macro would interfere with the dnode_t struct definition.
 * In general, highly portable and reusable C modules which expose their
 * structures need to confine structure member names to well-defined spaces.
 * The resulting identifiers aren't necessarily convenient to use, nor
 * readable, in the implementation, however!
 */

#define left dict_left
#define right dict_right
#define parent dict_parent
#define color dict_color
#define key dict_key
#define data dict_data

#define nilnode dict_nilnode
#define maxcount dict_maxcount
#define compare dict_compare
#define dupes dict_dupes

#define dict_root(D) ((D)->nilnode.left)
#define dict_nil(D) (&(D)->nilnode)

static void dnode_free(dnode_t *node);

/*
 * Perform a ``left rotation'' adjustment on the tree.  The given node P and
 * its right child C are rearranged so that the P instead becomes the left
 * child of C.   The left subtree of C is inherited as the new right subtree
 * for P.  The ordering of the keys within the tree is thus preserved.
 */

static void rotate_left(dnode_t *upper)
{
	dnode_t *lower, *lowleft, *upparent;

	lower = upper->right;
	upper->right = lowleft = lower->left;
	lowleft->parent = upper;

	lower->parent = upparent = upper->parent;

	/* don't need to check for root node here because root->parent is
	   the sentinel nil node, and root->parent->left points back to root */

	if (upper == upparent->left) {
		upparent->left = lower;
	} else {
		assert (upper == upparent->right);
		upparent->right = lower;
	}

	lower->left = upper;
	upper->parent = lower;
}

/*
 * This operation is the ``mirror'' image of rotate_left. It is
 * the same procedure, but with left and right interchanged.
 */

static void rotate_right(dnode_t *upper)
{
	dnode_t *lower, *lowright, *upparent;

	lower = upper->left;
	upper->left = lowright = lower->right;
	lowright->parent = upper;

	lower->parent = upparent = upper->parent;

	if (upper == upparent->right) {
		upparent->right = lower;
	} else {
		assert (upper == upparent->left);
		upparent->left = lower;
	}

	lower->right = upper;
	upper->parent = lower;
}

/*
 * Do a postorder traversal of the tree rooted at the specified
 * node and free everything under it.  Used by dict_free().
 */

static void free_nodes(dict_t *dict, dnode_t *node, dnode_t *nil)
{
	if (node == nil)
		return;
	free_nodes(dict, node->left, nil);
	free_nodes(dict, node->right, nil);
	dict->dict_freenode(node);
}

/*
 * Verify that the tree contains the given node. This is done by
 * traversing all of the nodes and comparing their pointers to the
 * given pointer. Returns 1 if the node is found, otherwise
 * returns zero. It is intended for debugging purposes.
 */

static int verify_dict_has_node(dnode_t *nil, dnode_t *root, dnode_t *node)
{
	if (root != nil) {
		return root == node
			|| verify_dict_has_node(nil, root->left, node)
			|| verify_dict_has_node(nil, root->right, node);
	}
	return 0;
}


/*
 * Select a different set of node allocator routines.
 */

static void dict_set_allocator(dict_t *dict, dnode_free_t fr)
{
	assert(dict_count(dict) == 0);
	dict->dict_freenode = fr;
}

/*
 * Free all the nodes in the dictionary by using the dictionary's
 * installed free routine. The dictionary is emptied.
 */

static void dict_free_nodes(dict_t *dict)
{
	dnode_t *nil = dict_nil(dict), *root = dict_root(dict);
	free_nodes(dict, root, nil);
	dict->dict_nodecount = 0;
	dict->nilnode.left = &dict->nilnode;
	dict->nilnode.right = &dict->nilnode;
}

/*
 * Initialize a user-supplied dictionary object.
 */

static dict_t *dict_init(dict_t *dict, dictcount_t maxcount, dict_comp_t comp)
{
	dict->compare = comp;
	dict->dict_freenode = dnode_free;
	dict->dict_nodecount = 0;
	dict->maxcount = maxcount;
	dict->nilnode.left = &dict->nilnode;
	dict->nilnode.right = &dict->nilnode;
	dict->nilnode.parent = &dict->nilnode;
	dict->nilnode.color = dnode_black;
	dict->dupes = 0;
	return dict;
}

/*
 * Locate a node in the dictionary having the given key.
 * If the node is not found, a null a pointer is returned (rather than
 * a pointer that dictionary's nil sentinel node), otherwise a pointer to the
 * located node is returned.
 */

static dnode_t *dict_lookup(dict_t *dict, const void *key)
{
	dnode_t *root = dict_root(dict);
	dnode_t *nil = dict_nil(dict);
	dnode_t *saved;
	int result;

	/* simple binary search adapted for trees that contain duplicate keys */

	while (root != nil) {
		result = dict->compare(key, root->key);
		if (result < 0)
			root = root->left;
		else if (result > 0)
			root = root->right;
		else {
			if (!dict->dupes) { /* no duplicates, return match          */
				return root;
			} else {            /* could be dupes, find leftmost one    */
				do {
					saved = root;
					root = root->left;
					while (root != nil && dict->compare(key, root->key))
						root = root->right;
				} while (root != nil);
				return saved;
			}
		}
	}

	return NULL;
}

/*
 * Insert a node into the dictionary. The node should have been
 * initialized with a data field. All other fields are ignored.
 * The behavior is undefined if the user attempts to insert into
 * a dictionary that is already full (for which the dict_isfull()
 * function returns true).
 */

static void dict_insert(dict_t *dict, dnode_t *node, const void *key)
{
	dnode_t *where = dict_root(dict), *nil = dict_nil(dict);
	dnode_t *parent = nil, *uncle, *grandpa;
	int result = -1;

	node->key = key;

	/* basic binary tree insert */

	while (where != nil) {
		parent = where;
		result = dict->compare(key, where->key);
		/* trap attempts at duplicate key insertion unless it's explicitly allowed */
		assert(dict->dupes || result != 0);
		if (result < 0)
			where = where->left;
		else
			where = where->right;
	}

	assert(where == nil);

	if (result < 0)
		parent->left = node;
	else
		parent->right = node;

	node->parent = parent;
	node->left = nil;
	node->right = nil;

	dict->dict_nodecount++;

	/* red black adjustments */

	node->color = dnode_red;

	while (parent->color == dnode_red) {
		grandpa = parent->parent;
		if (parent == grandpa->left) {
			uncle = grandpa->right;
			if (uncle->color == dnode_red) {    /* red parent, red uncle */
				parent->color = dnode_black;
				uncle->color = dnode_black;
				grandpa->color = dnode_red;
				node = grandpa;
				parent = grandpa->parent;
			} else {                            /* red parent, black uncle */
				if (node == parent->right) {
					rotate_left(parent);
					parent = node;
					assert (grandpa == parent->parent);
					/* rotation between parent and child preserves grandpa */
				}
				parent->color = dnode_black;
				grandpa->color = dnode_red;
				rotate_right(grandpa);
				break;
			}
		} else {        /* symmetric cases: parent == parent->parent->right */
			uncle = grandpa->left;
			if (uncle->color == dnode_red) {
				parent->color = dnode_black;
				uncle->color = dnode_black;
				grandpa->color = dnode_red;
				node = grandpa;
				parent = grandpa->parent;
			} else {
				if (node == parent->left) {
					rotate_right(parent);
					parent = node;
					assert (grandpa == parent->parent);
				}
				parent->color = dnode_black;
				grandpa->color = dnode_red;
				rotate_left(grandpa);
				break;
			}
		}
	}

	dict_root(dict)->color = dnode_black;
}

/*
 * Allocate a node using the dictionary's allocator routine, give it
 * the data item.
 */

static dnode_t *dnode_init(dnode_t *dnode, void *data)
{
	dnode->data = data;
	dnode->parent = NULL;
	dnode->left = NULL;
	dnode->right = NULL;
	return dnode;
}

static int dict_alloc_insert(dict_t *dict, const void *key, void *data)
{
	dnode_t *node = xmalloc(sizeof(dnode_t));

	dnode_init(node, data);
	dict_insert(dict, node, key);
	return 1;
}

/*
 * Return the node with the lowest (leftmost) key. If the dictionary is empty
 * (that is, dict_isempty(dict) returns 1) a null pointer is returned.
 */

static dnode_t *dict_first(dict_t *dict)
{
	dnode_t *nil = dict_nil(dict), *root = dict_root(dict), *left;

	if (root != nil)
		while ((left = root->left) != nil)
			root = left;

	return (root == nil) ? NULL : root;
}

/*
 * Return the given node's successor node---the node which has the
 * next key in the left to right ordering. If the node has
 * no successor, a null pointer is returned rather than a pointer to
 * the nil node.
 */

static dnode_t *dict_next(dict_t *dict, dnode_t *curr)
{
	dnode_t *nil = dict_nil(dict), *parent, *left;

	if (curr->right != nil) {
		curr = curr->right;
		while ((left = curr->left) != nil)
			curr = left;
		return curr;
	}

	parent = curr->parent;

	while (parent != nil && curr == parent->right) {
		curr = parent;
		parent = curr->parent;
	}

	return (parent == nil) ? NULL : parent;
}


static void dnode_free(dnode_t *node)
{
	free(node);
}


#undef left
#undef right
#undef parent
#undef color
#undef key
#undef data

#undef nilnode
#undef maxcount
#undef compare
#undef dupes


/*
 * dirinfo.c --- maintains the directory information table for e2fsck.
 */

/*
 * This subroutine is called during pass1 to create a directory info
 * entry.  During pass1, the passed-in parent is 0; it will get filled
 * in during pass2.
 */
static void e2fsck_add_dir_info(e2fsck_t ctx, ext2_ino_t ino, ext2_ino_t parent)
{
	struct dir_info *dir;
	int             i, j;
	ext2_ino_t      num_dirs;
	errcode_t       retval;
	unsigned long   old_size;

	if (!ctx->dir_info) {
		ctx->dir_info_count = 0;
		retval = ext2fs_get_num_dirs(ctx->fs, &num_dirs);
		if (retval)
			num_dirs = 1024;        /* Guess */
		ctx->dir_info_size = num_dirs + 10;
		ctx->dir_info  = (struct dir_info *)
			e2fsck_allocate_memory(ctx, ctx->dir_info_size
					       * sizeof (struct dir_info),
					       "directory map");
	}

	if (ctx->dir_info_count >= ctx->dir_info_size) {
		old_size = ctx->dir_info_size * sizeof(struct dir_info);
		ctx->dir_info_size += 10;
		retval = ext2fs_resize_mem(old_size, ctx->dir_info_size *
					   sizeof(struct dir_info),
					   &ctx->dir_info);
		if (retval) {
			ctx->dir_info_size -= 10;
			return;
		}
	}

	/*
	 * Normally, add_dir_info is called with each inode in
	 * sequential order; but once in a while (like when pass 3
	 * needs to recreate the root directory or lost+found
	 * directory) it is called out of order.  In those cases, we
	 * need to move the dir_info entries down to make room, since
	 * the dir_info array needs to be sorted by inode number for
	 * get_dir_info()'s sake.
	 */
	if (ctx->dir_info_count &&
	    ctx->dir_info[ctx->dir_info_count-1].ino >= ino) {
		for (i = ctx->dir_info_count-1; i > 0; i--)
			if (ctx->dir_info[i-1].ino < ino)
				break;
		dir = &ctx->dir_info[i];
		if (dir->ino != ino)
			for (j = ctx->dir_info_count++; j > i; j--)
				ctx->dir_info[j] = ctx->dir_info[j-1];
	} else
		dir = &ctx->dir_info[ctx->dir_info_count++];

	dir->ino = ino;
	dir->dotdot = parent;
	dir->parent = parent;
}

/*
 * get_dir_info() --- given an inode number, try to find the directory
 * information entry for it.
 */
static struct dir_info *e2fsck_get_dir_info(e2fsck_t ctx, ext2_ino_t ino)
{
	int     low, high, mid;

	low = 0;
	high = ctx->dir_info_count-1;
	if (!ctx->dir_info)
		return 0;
	if (ino == ctx->dir_info[low].ino)
		return &ctx->dir_info[low];
	if  (ino == ctx->dir_info[high].ino)
		return &ctx->dir_info[high];

	while (low < high) {
		mid = (low+high)/2;
		if (mid == low || mid == high)
			break;
		if (ino == ctx->dir_info[mid].ino)
			return &ctx->dir_info[mid];
		if (ino < ctx->dir_info[mid].ino)
			high = mid;
		else
			low = mid;
	}
	return 0;
}

/*
 * Free the dir_info structure when it isn't needed any more.
 */
static void e2fsck_free_dir_info(e2fsck_t ctx)
{
	ext2fs_free_mem(&ctx->dir_info);
	ctx->dir_info_size = 0;
	ctx->dir_info_count = 0;
}

/*
 * Return the count of number of directories in the dir_info structure
 */
static int e2fsck_get_num_dirinfo(e2fsck_t ctx)
{
	return ctx->dir_info_count;
}

/*
 * A simple interator function
 */
static struct dir_info *e2fsck_dir_info_iter(e2fsck_t ctx, int *control)
{
	if (*control >= ctx->dir_info_count)
		return 0;

	return ctx->dir_info + (*control)++;
}

/*
 * dirinfo.c --- maintains the directory information table for e2fsck.
 *
 */

#ifdef ENABLE_HTREE

/*
 * This subroutine is called during pass1 to create a directory info
 * entry.  During pass1, the passed-in parent is 0; it will get filled
 * in during pass2.
 */
static void e2fsck_add_dx_dir(e2fsck_t ctx, ext2_ino_t ino, int num_blocks)
{
	struct dx_dir_info *dir;
	int             i, j;
	errcode_t       retval;
	unsigned long   old_size;

	if (!ctx->dx_dir_info) {
		ctx->dx_dir_info_count = 0;
		ctx->dx_dir_info_size = 100; /* Guess */
		ctx->dx_dir_info  = (struct dx_dir_info *)
			e2fsck_allocate_memory(ctx, ctx->dx_dir_info_size
					       * sizeof (struct dx_dir_info),
					       "directory map");
	}

	if (ctx->dx_dir_info_count >= ctx->dx_dir_info_size) {
		old_size = ctx->dx_dir_info_size * sizeof(struct dx_dir_info);
		ctx->dx_dir_info_size += 10;
		retval = ext2fs_resize_mem(old_size, ctx->dx_dir_info_size *
					   sizeof(struct dx_dir_info),
					   &ctx->dx_dir_info);
		if (retval) {
			ctx->dx_dir_info_size -= 10;
			return;
		}
	}

	/*
	 * Normally, add_dx_dir_info is called with each inode in
	 * sequential order; but once in a while (like when pass 3
	 * needs to recreate the root directory or lost+found
	 * directory) it is called out of order.  In those cases, we
	 * need to move the dx_dir_info entries down to make room, since
	 * the dx_dir_info array needs to be sorted by inode number for
	 * get_dx_dir_info()'s sake.
	 */
	if (ctx->dx_dir_info_count &&
	    ctx->dx_dir_info[ctx->dx_dir_info_count-1].ino >= ino) {
		for (i = ctx->dx_dir_info_count-1; i > 0; i--)
			if (ctx->dx_dir_info[i-1].ino < ino)
				break;
		dir = &ctx->dx_dir_info[i];
		if (dir->ino != ino)
			for (j = ctx->dx_dir_info_count++; j > i; j--)
				ctx->dx_dir_info[j] = ctx->dx_dir_info[j-1];
	} else
		dir = &ctx->dx_dir_info[ctx->dx_dir_info_count++];

	dir->ino = ino;
	dir->numblocks = num_blocks;
	dir->hashversion = 0;
	dir->dx_block = e2fsck_allocate_memory(ctx, num_blocks
				       * sizeof (struct dx_dirblock_info),
				       "dx_block info array");
}

/*
 * get_dx_dir_info() --- given an inode number, try to find the directory
 * information entry for it.
 */
static struct dx_dir_info *e2fsck_get_dx_dir_info(e2fsck_t ctx, ext2_ino_t ino)
{
	int     low, high, mid;

	low = 0;
	high = ctx->dx_dir_info_count-1;
	if (!ctx->dx_dir_info)
		return 0;
	if (ino == ctx->dx_dir_info[low].ino)
		return &ctx->dx_dir_info[low];
	if  (ino == ctx->dx_dir_info[high].ino)
		return &ctx->dx_dir_info[high];

	while (low < high) {
		mid = (low+high)/2;
		if (mid == low || mid == high)
			break;
		if (ino == ctx->dx_dir_info[mid].ino)
			return &ctx->dx_dir_info[mid];
		if (ino < ctx->dx_dir_info[mid].ino)
			high = mid;
		else
			low = mid;
	}
	return 0;
}

/*
 * Free the dx_dir_info structure when it isn't needed any more.
 */
static void e2fsck_free_dx_dir_info(e2fsck_t ctx)
{
	int     i;
	struct dx_dir_info *dir;

	if (ctx->dx_dir_info) {
		dir = ctx->dx_dir_info;
		for (i=0; i < ctx->dx_dir_info_count; i++) {
			ext2fs_free_mem(&dir->dx_block);
		}
		ext2fs_free_mem(&ctx->dx_dir_info);
	}
	ctx->dx_dir_info_size = 0;
	ctx->dx_dir_info_count = 0;
}

/*
 * A simple interator function
 */
static struct dx_dir_info *e2fsck_dx_dir_info_iter(e2fsck_t ctx, int *control)
{
	if (*control >= ctx->dx_dir_info_count)
		return 0;

	return ctx->dx_dir_info + (*control)++;
}

#endif /* ENABLE_HTREE */
/*
 * e2fsck.c - a consistency checker for the new extended file system.
 *
 */

/*
 * This function allocates an e2fsck context
 */
static errcode_t e2fsck_allocate_context(e2fsck_t *ret)
{
	e2fsck_t        context;
	errcode_t       retval;

	retval = ext2fs_get_mem(sizeof(struct e2fsck_struct), &context);
	if (retval)
		return retval;

	memset(context, 0, sizeof(struct e2fsck_struct));

	context->process_inode_size = 256;
	context->ext_attr_ver = 2;

	*ret = context;
	return 0;
}

struct ea_refcount_el {
	blk_t   ea_blk;
	int     ea_count;
};

struct ea_refcount {
	blk_t           count;
	blk_t           size;
	blk_t           cursor;
	struct ea_refcount_el   *list;
};

static void ea_refcount_free(ext2_refcount_t refcount)
{
	if (!refcount)
		return;

	ext2fs_free_mem(&refcount->list);
	ext2fs_free_mem(&refcount);
}

/*
 * This function resets an e2fsck context; it is called when e2fsck
 * needs to be restarted.
 */
static errcode_t e2fsck_reset_context(e2fsck_t ctx)
{
	ctx->flags = 0;
	ctx->lost_and_found = 0;
	ctx->bad_lost_and_found = 0;
	ext2fs_free_inode_bitmap(ctx->inode_used_map);
	ctx->inode_used_map = 0;
	ext2fs_free_inode_bitmap(ctx->inode_dir_map);
	ctx->inode_dir_map = 0;
	ext2fs_free_inode_bitmap(ctx->inode_reg_map);
	ctx->inode_reg_map = 0;
	ext2fs_free_block_bitmap(ctx->block_found_map);
	ctx->block_found_map = 0;
	ext2fs_free_icount(ctx->inode_link_info);
	ctx->inode_link_info = 0;
	if (ctx->journal_io) {
		if (ctx->fs && ctx->fs->io != ctx->journal_io)
			io_channel_close(ctx->journal_io);
		ctx->journal_io = 0;
	}
	if (ctx->fs) {
		ext2fs_free_dblist(ctx->fs->dblist);
		ctx->fs->dblist = 0;
	}
	e2fsck_free_dir_info(ctx);
#ifdef ENABLE_HTREE
	e2fsck_free_dx_dir_info(ctx);
#endif
	ea_refcount_free(ctx->refcount);
	ctx->refcount = 0;
	ea_refcount_free(ctx->refcount_extra);
	ctx->refcount_extra = 0;
	ext2fs_free_block_bitmap(ctx->block_dup_map);
	ctx->block_dup_map = 0;
	ext2fs_free_block_bitmap(ctx->block_ea_map);
	ctx->block_ea_map = 0;
	ext2fs_free_inode_bitmap(ctx->inode_bad_map);
	ctx->inode_bad_map = 0;
	ext2fs_free_inode_bitmap(ctx->inode_imagic_map);
	ctx->inode_imagic_map = 0;
	ext2fs_u32_list_free(ctx->dirs_to_hash);
	ctx->dirs_to_hash = 0;

	/*
	 * Clear the array of invalid meta-data flags
	 */
	ext2fs_free_mem(&ctx->invalid_inode_bitmap_flag);
	ext2fs_free_mem(&ctx->invalid_block_bitmap_flag);
	ext2fs_free_mem(&ctx->invalid_inode_table_flag);

	/* Clear statistic counters */
	ctx->fs_directory_count = 0;
	ctx->fs_regular_count = 0;
	ctx->fs_blockdev_count = 0;
	ctx->fs_chardev_count = 0;
	ctx->fs_links_count = 0;
	ctx->fs_symlinks_count = 0;
	ctx->fs_fast_symlinks_count = 0;
	ctx->fs_fifo_count = 0;
	ctx->fs_total_count = 0;
	ctx->fs_sockets_count = 0;
	ctx->fs_ind_count = 0;
	ctx->fs_dind_count = 0;
	ctx->fs_tind_count = 0;
	ctx->fs_fragmented = 0;
	ctx->large_files = 0;

	/* Reset the superblock to the user's requested value */
	ctx->superblock = ctx->use_superblock;

	return 0;
}

static void e2fsck_free_context(e2fsck_t ctx)
{
	if (!ctx)
		return;

	e2fsck_reset_context(ctx);
	if (ctx->blkid)
		blkid_put_cache(ctx->blkid);

	ext2fs_free_mem(&ctx);
}

/*
 * ea_refcount.c
 */

/*
 * The strategy we use for keeping track of EA refcounts is as
 * follows.  We keep a sorted array of first EA blocks and its
 * reference counts.  Once the refcount has dropped to zero, it is
 * removed from the array to save memory space.  Once the EA block is
 * checked, its bit is set in the block_ea_map bitmap.
 */


static errcode_t ea_refcount_create(int size, ext2_refcount_t *ret)
{
	ext2_refcount_t refcount;
	errcode_t       retval;
	size_t          bytes;

	retval = ext2fs_get_mem(sizeof(struct ea_refcount), &refcount);
	if (retval)
		return retval;
	memset(refcount, 0, sizeof(struct ea_refcount));

	if (!size)
		size = 500;
	refcount->size = size;
	bytes = (size_t) (size * sizeof(struct ea_refcount_el));
#ifdef DEBUG
	printf("Refcount allocated %d entries, %d bytes.\n",
	       refcount->size, bytes);
#endif
	retval = ext2fs_get_mem(bytes, &refcount->list);
	if (retval)
		goto errout;
	memset(refcount->list, 0, bytes);

	refcount->count = 0;
	refcount->cursor = 0;

	*ret = refcount;
	return 0;

errout:
	ea_refcount_free(refcount);
	return retval;
}

/*
 * collapse_refcount() --- go through the refcount array, and get rid
 * of any count == zero entries
 */
static void refcount_collapse(ext2_refcount_t refcount)
{
	unsigned int    i, j;
	struct ea_refcount_el   *list;

	list = refcount->list;
	for (i = 0, j = 0; i < refcount->count; i++) {
		if (list[i].ea_count) {
			if (i != j)
				list[j] = list[i];
			j++;
		}
	}
#if defined(DEBUG) || defined(TEST_PROGRAM)
	printf("Refcount_collapse: size was %d, now %d\n",
	       refcount->count, j);
#endif
	refcount->count = j;
}


/*
 * insert_refcount_el() --- Insert a new entry into the sorted list at a
 *      specified position.
 */
static struct ea_refcount_el *insert_refcount_el(ext2_refcount_t refcount,
						 blk_t blk, int pos)
{
	struct ea_refcount_el   *el;
	errcode_t               retval;
	blk_t                   new_size = 0;
	int                     num;

	if (refcount->count >= refcount->size) {
		new_size = refcount->size + 100;
#ifdef DEBUG
		printf("Reallocating refcount %d entries...\n", new_size);
#endif
		retval = ext2fs_resize_mem((size_t) refcount->size *
					   sizeof(struct ea_refcount_el),
					   (size_t) new_size *
					   sizeof(struct ea_refcount_el),
					   &refcount->list);
		if (retval)
			return 0;
		refcount->size = new_size;
	}
	num = (int) refcount->count - pos;
	if (num < 0)
		return 0;       /* should never happen */
	if (num) {
		memmove(&refcount->list[pos+1], &refcount->list[pos],
			sizeof(struct ea_refcount_el) * num);
	}
	refcount->count++;
	el = &refcount->list[pos];
	el->ea_count = 0;
	el->ea_blk = blk;
	return el;
}


/*
 * get_refcount_el() --- given an block number, try to find refcount
 *      information in the sorted list.  If the create flag is set,
 *      and we can't find an entry, create one in the sorted list.
 */
static struct ea_refcount_el *get_refcount_el(ext2_refcount_t refcount,
					      blk_t blk, int create)
{
	float   range;
	int     low, high, mid;
	blk_t   lowval, highval;

	if (!refcount || !refcount->list)
		return 0;
retry:
	low = 0;
	high = (int) refcount->count-1;
	if (create && ((refcount->count == 0) ||
		       (blk > refcount->list[high].ea_blk))) {
		if (refcount->count >= refcount->size)
			refcount_collapse(refcount);

		return insert_refcount_el(refcount, blk,
					  (unsigned) refcount->count);
	}
	if (refcount->count == 0)
		return 0;

	if (refcount->cursor >= refcount->count)
		refcount->cursor = 0;
	if (blk == refcount->list[refcount->cursor].ea_blk)
		return &refcount->list[refcount->cursor++];
#ifdef DEBUG
	printf("Non-cursor get_refcount_el: %u\n", blk);
#endif
	while (low <= high) {
		if (low == high)
			mid = low;
		else {
			/* Interpolate for efficiency */
			lowval = refcount->list[low].ea_blk;
			highval = refcount->list[high].ea_blk;

			if (blk < lowval)
				range = 0;
			else if (blk > highval)
				range = 1;
			else
				range = ((float) (blk - lowval)) /
					(highval - lowval);
			mid = low + ((int) (range * (high-low)));
		}

		if (blk == refcount->list[mid].ea_blk) {
			refcount->cursor = mid+1;
			return &refcount->list[mid];
		}
		if (blk < refcount->list[mid].ea_blk)
			high = mid-1;
		else
			low = mid+1;
	}
	/*
	 * If we need to create a new entry, it should be right at
	 * low (where high will be left at low-1).
	 */
	if (create) {
		if (refcount->count >= refcount->size) {
			refcount_collapse(refcount);
			if (refcount->count < refcount->size)
				goto retry;
		}
		return insert_refcount_el(refcount, blk, low);
	}
	return 0;
}

static errcode_t
ea_refcount_increment(ext2_refcount_t refcount, blk_t blk, int *ret)
{
	struct ea_refcount_el   *el;

	el = get_refcount_el(refcount, blk, 1);
	if (!el)
		return EXT2_ET_NO_MEMORY;
	el->ea_count++;

	if (ret)
		*ret = el->ea_count;
	return 0;
}

static errcode_t
ea_refcount_decrement(ext2_refcount_t refcount, blk_t blk, int *ret)
{
	struct ea_refcount_el   *el;

	el = get_refcount_el(refcount, blk, 0);
	if (!el || el->ea_count == 0)
		return EXT2_ET_INVALID_ARGUMENT;

	el->ea_count--;

	if (ret)
		*ret = el->ea_count;
	return 0;
}

static errcode_t
ea_refcount_store(ext2_refcount_t refcount, blk_t blk, int count)
{
	struct ea_refcount_el   *el;

	/*
	 * Get the refcount element
	 */
	el = get_refcount_el(refcount, blk, count ? 1 : 0);
	if (!el)
		return count ? EXT2_ET_NO_MEMORY : 0;
	el->ea_count = count;
	return 0;
}

static inline void ea_refcount_intr_begin(ext2_refcount_t refcount)
{
	refcount->cursor = 0;
}


static blk_t ea_refcount_intr_next(ext2_refcount_t refcount, int *ret)
{
	struct ea_refcount_el   *list;

	while (1) {
		if (refcount->cursor >= refcount->count)
			return 0;
		list = refcount->list;
		if (list[refcount->cursor].ea_count) {
			if (ret)
				*ret = list[refcount->cursor].ea_count;
			return list[refcount->cursor++].ea_blk;
		}
		refcount->cursor++;
	}
}


/*
 * ehandler.c --- handle bad block errors which come up during the
 *      course of an e2fsck session.
 */


static const char *operation;

static errcode_t
e2fsck_handle_read_error(io_channel channel, unsigned long block, int count,
			 void *data, size_t size FSCK_ATTR((unused)),
			 int actual FSCK_ATTR((unused)), errcode_t error)
{
	int     i;
	char    *p;
	ext2_filsys fs = (ext2_filsys) channel->app_data;
	e2fsck_t ctx;

	ctx = (e2fsck_t) fs->priv_data;

	/*
	 * If more than one block was read, try reading each block
	 * separately.  We could use the actual bytes read to figure
	 * out where to start, but we don't bother.
	 */
	if (count > 1) {
		p = (char *) data;
		for (i=0; i < count; i++, p += channel->block_size, block++) {
			error = io_channel_read_blk(channel, block,
						    1, p);
			if (error)
				return error;
		}
		return 0;
	}
	if (operation)
		printf(_("Error reading block %lu (%s) while %s.  "), block,
		       error_message(error), operation);
	else
		printf(_("Error reading block %lu (%s).  "), block,
		       error_message(error));
	preenhalt(ctx);
	if (ask(ctx, _("Ignore error"), 1)) {
		if (ask(ctx, _("Force rewrite"), 1))
			io_channel_write_blk(channel, block, 1, data);
		return 0;
	}

	return error;
}

static errcode_t
e2fsck_handle_write_error(io_channel channel, unsigned long block, int count,
			const void *data, size_t size FSCK_ATTR((unused)),
			int actual FSCK_ATTR((unused)), errcode_t error)
{
	int             i;
	const char      *p;
	ext2_filsys fs = (ext2_filsys) channel->app_data;
	e2fsck_t ctx;

	ctx = (e2fsck_t) fs->priv_data;

	/*
	 * If more than one block was written, try writing each block
	 * separately.  We could use the actual bytes read to figure
	 * out where to start, but we don't bother.
	 */
	if (count > 1) {
		p = (const char *) data;
		for (i=0; i < count; i++, p += channel->block_size, block++) {
			error = io_channel_write_blk(channel, block,
						     1, p);
			if (error)
				return error;
		}
		return 0;
	}

	if (operation)
		printf(_("Error writing block %lu (%s) while %s.  "), block,
		       error_message(error), operation);
	else
		printf(_("Error writing block %lu (%s).  "), block,
		       error_message(error));
	preenhalt(ctx);
	if (ask(ctx, _("Ignore error"), 1))
		return 0;

	return error;
}

static const char *ehandler_operation(const char *op)
{
	const char *ret = operation;

	operation = op;
	return ret;
}

static void ehandler_init(io_channel channel)
{
	channel->read_error = e2fsck_handle_read_error;
	channel->write_error = e2fsck_handle_write_error;
}

/*
 * journal.c --- code for handling the "ext3" journal
 *
 * Copyright (C) 2000 Andreas Dilger
 * Copyright (C) 2000 Theodore Ts'o
 *
 * Parts of the code are based on fs/jfs/journal.c by Stephen C. Tweedie
 * Copyright (C) 1999 Red Hat Software
 *
 * This file may be redistributed under the terms of the
 * GNU General Public License version 2 or at your discretion
 * any later version.
 */

/*
 * Define USE_INODE_IO to use the inode_io.c / fileio.c codepaths.
 * This creates a larger static binary, and a smaller binary using
 * shared libraries.  It's also probably slightly less CPU-efficient,
 * which is why it's not on by default.  But, it's a good way of
 * testing the functions in inode_io.c and fileio.c.
 */
#undef USE_INODE_IO

/* Kernel compatibility functions for handling the journal.  These allow us
 * to use the recovery.c file virtually unchanged from the kernel, so we
 * don't have to do much to keep kernel and user recovery in sync.
 */
static int journal_bmap(journal_t *journal, blk_t block, unsigned long *phys)
{
#ifdef USE_INODE_IO
	*phys = block;
	return 0;
#else
	struct inode    *inode = journal->j_inode;
	errcode_t       retval;
	blk_t           pblk;

	if (!inode) {
		*phys = block;
		return 0;
	}

	retval= ext2fs_bmap(inode->i_ctx->fs, inode->i_ino,
			    &inode->i_ext2, NULL, 0, block, &pblk);
	*phys = pblk;
	return retval;
#endif
}

static struct buffer_head *getblk(kdev_t kdev, blk_t blocknr, int blocksize)
{
	struct buffer_head *bh;

	bh = e2fsck_allocate_memory(kdev->k_ctx, sizeof(*bh), "block buffer");
	if (!bh)
		return NULL;

	bh->b_ctx = kdev->k_ctx;
	if (kdev->k_dev == K_DEV_FS)
		bh->b_io = kdev->k_ctx->fs->io;
	else
		bh->b_io = kdev->k_ctx->journal_io;
	bh->b_size = blocksize;
	bh->b_blocknr = blocknr;

	return bh;
}

static void sync_blockdev(kdev_t kdev)
{
	io_channel      io;

	if (kdev->k_dev == K_DEV_FS)
		io = kdev->k_ctx->fs->io;
	else
		io = kdev->k_ctx->journal_io;

	io_channel_flush(io);
}

static void ll_rw_block(int rw, int nr, struct buffer_head *bhp[])
{
	int retval;
	struct buffer_head *bh;

	for (; nr > 0; --nr) {
		bh = *bhp++;
		if (rw == READ && !bh->b_uptodate) {
			retval = io_channel_read_blk(bh->b_io,
						     bh->b_blocknr,
						     1, bh->b_data);
			if (retval) {
				bb_error_msg("while reading block %lu",
					(unsigned long) bh->b_blocknr);
				bh->b_err = retval;
				continue;
			}
			bh->b_uptodate = 1;
		} else if (rw == WRITE && bh->b_dirty) {
			retval = io_channel_write_blk(bh->b_io,
						      bh->b_blocknr,
						      1, bh->b_data);
			if (retval) {
				bb_error_msg("while writing block %lu",
					(unsigned long) bh->b_blocknr);
				bh->b_err = retval;
				continue;
			}
			bh->b_dirty = 0;
			bh->b_uptodate = 1;
		}
	}
}

static void mark_buffer_dirty(struct buffer_head *bh)
{
	bh->b_dirty = 1;
}

static inline void mark_buffer_clean(struct buffer_head * bh)
{
	bh->b_dirty = 0;
}

static void brelse(struct buffer_head *bh)
{
	if (bh->b_dirty)
		ll_rw_block(WRITE, 1, &bh);
	ext2fs_free_mem(&bh);
}

static int buffer_uptodate(struct buffer_head *bh)
{
	return bh->b_uptodate;
}

static inline void mark_buffer_uptodate(struct buffer_head *bh, int val)
{
	bh->b_uptodate = val;
}

static void wait_on_buffer(struct buffer_head *bh)
{
	if (!bh->b_uptodate)
		ll_rw_block(READ, 1, &bh);
}


static void e2fsck_clear_recover(e2fsck_t ctx, int error)
{
	ctx->fs->super->s_feature_incompat &= ~EXT3_FEATURE_INCOMPAT_RECOVER;

	/* if we had an error doing journal recovery, we need a full fsck */
	if (error)
		ctx->fs->super->s_state &= ~EXT2_VALID_FS;
	ext2fs_mark_super_dirty(ctx->fs);
}

static errcode_t e2fsck_get_journal(e2fsck_t ctx, journal_t **ret_journal)
{
	struct ext2_super_block *sb = ctx->fs->super;
	struct ext2_super_block jsuper;
	struct problem_context  pctx;
	struct buffer_head      *bh;
	struct inode            *j_inode = NULL;
	struct kdev_s           *dev_fs = NULL, *dev_journal;
	const char              *journal_name = NULL;
	journal_t               *journal = NULL;
	errcode_t               retval = 0;
	io_manager              io_ptr = 0;
	unsigned long           start = 0;
	blk_t                   blk;
	int                     ext_journal = 0;
	int                     tried_backup_jnl = 0;
	int                     i;

	clear_problem_context(&pctx);

	journal = e2fsck_allocate_memory(ctx, sizeof(journal_t), "journal");
	if (!journal) {
		return EXT2_ET_NO_MEMORY;
	}

	dev_fs = e2fsck_allocate_memory(ctx, 2*sizeof(struct kdev_s), "kdev");
	if (!dev_fs) {
		retval = EXT2_ET_NO_MEMORY;
		goto errout;
	}
	dev_journal = dev_fs+1;

	dev_fs->k_ctx = dev_journal->k_ctx = ctx;
	dev_fs->k_dev = K_DEV_FS;
	dev_journal->k_dev = K_DEV_JOURNAL;

	journal->j_dev = dev_journal;
	journal->j_fs_dev = dev_fs;
	journal->j_inode = NULL;
	journal->j_blocksize = ctx->fs->blocksize;

	if (uuid_is_null(sb->s_journal_uuid)) {
		if (!sb->s_journal_inum)
			return EXT2_ET_BAD_INODE_NUM;
		j_inode = e2fsck_allocate_memory(ctx, sizeof(*j_inode),
						 "journal inode");
		if (!j_inode) {
			retval = EXT2_ET_NO_MEMORY;
			goto errout;
		}

		j_inode->i_ctx = ctx;
		j_inode->i_ino = sb->s_journal_inum;

		if ((retval = ext2fs_read_inode(ctx->fs,
						sb->s_journal_inum,
						&j_inode->i_ext2))) {
		try_backup_journal:
			if (sb->s_jnl_backup_type != EXT3_JNL_BACKUP_BLOCKS ||
			    tried_backup_jnl)
				goto errout;
			memset(&j_inode->i_ext2, 0, sizeof(struct ext2_inode));
			memcpy(&j_inode->i_ext2.i_block[0], sb->s_jnl_blocks,
			       EXT2_N_BLOCKS*4);
			j_inode->i_ext2.i_size = sb->s_jnl_blocks[16];
			j_inode->i_ext2.i_links_count = 1;
			j_inode->i_ext2.i_mode = LINUX_S_IFREG | 0600;
			tried_backup_jnl++;
		}
		if (!j_inode->i_ext2.i_links_count ||
		    !LINUX_S_ISREG(j_inode->i_ext2.i_mode)) {
			retval = EXT2_ET_NO_JOURNAL;
			goto try_backup_journal;
		}
		if (j_inode->i_ext2.i_size / journal->j_blocksize <
		    JFS_MIN_JOURNAL_BLOCKS) {
			retval = EXT2_ET_JOURNAL_TOO_SMALL;
			goto try_backup_journal;
		}
		for (i=0; i < EXT2_N_BLOCKS; i++) {
			blk = j_inode->i_ext2.i_block[i];
			if (!blk) {
				if (i < EXT2_NDIR_BLOCKS) {
					retval = EXT2_ET_JOURNAL_TOO_SMALL;
					goto try_backup_journal;
				}
				continue;
			}
			if (blk < sb->s_first_data_block ||
			    blk >= sb->s_blocks_count) {
				retval = EXT2_ET_BAD_BLOCK_NUM;
				goto try_backup_journal;
			}
		}
		journal->j_maxlen = j_inode->i_ext2.i_size / journal->j_blocksize;

#ifdef USE_INODE_IO
		retval = ext2fs_inode_io_intern2(ctx->fs, sb->s_journal_inum,
						 &j_inode->i_ext2,
						 &journal_name);
		if (retval)
			goto errout;

		io_ptr = inode_io_manager;
#else
		journal->j_inode = j_inode;
		ctx->journal_io = ctx->fs->io;
		if ((retval = journal_bmap(journal, 0, &start)) != 0)
			goto errout;
#endif
	} else {
		ext_journal = 1;
		if (!ctx->journal_name) {
			char uuid[37];

			uuid_unparse(sb->s_journal_uuid, uuid);
			ctx->journal_name = blkid_get_devname(ctx->blkid,
							      "UUID", uuid);
			if (!ctx->journal_name)
				ctx->journal_name = blkid_devno_to_devname(sb->s_journal_dev);
		}
		journal_name = ctx->journal_name;

		if (!journal_name) {
			fix_problem(ctx, PR_0_CANT_FIND_JOURNAL, &pctx);
			return EXT2_ET_LOAD_EXT_JOURNAL;
		}

		io_ptr = unix_io_manager;
	}

#ifndef USE_INODE_IO
	if (ext_journal)
#endif
		retval = io_ptr->open(journal_name, IO_FLAG_RW,
				      &ctx->journal_io);
	if (retval)
		goto errout;

	io_channel_set_blksize(ctx->journal_io, ctx->fs->blocksize);

	if (ext_journal) {
		if (ctx->fs->blocksize == 1024)
			start = 1;
		bh = getblk(dev_journal, start, ctx->fs->blocksize);
		if (!bh) {
			retval = EXT2_ET_NO_MEMORY;
			goto errout;
		}
		ll_rw_block(READ, 1, &bh);
		if ((retval = bh->b_err) != 0)
			goto errout;
		memcpy(&jsuper, start ? bh->b_data :  bh->b_data + 1024,
		       sizeof(jsuper));
		brelse(bh);
#if BB_BIG_ENDIAN
		if (jsuper.s_magic == ext2fs_swab16(EXT2_SUPER_MAGIC))
			ext2fs_swap_super(&jsuper);
#endif
		if (jsuper.s_magic != EXT2_SUPER_MAGIC ||
		    !(jsuper.s_feature_incompat & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV)) {
			fix_problem(ctx, PR_0_EXT_JOURNAL_BAD_SUPER, &pctx);
			retval = EXT2_ET_LOAD_EXT_JOURNAL;
			goto errout;
		}
		/* Make sure the journal UUID is correct */
		if (memcmp(jsuper.s_uuid, ctx->fs->super->s_journal_uuid,
			   sizeof(jsuper.s_uuid))) {
			fix_problem(ctx, PR_0_JOURNAL_BAD_UUID, &pctx);
			retval = EXT2_ET_LOAD_EXT_JOURNAL;
			goto errout;
		}

		journal->j_maxlen = jsuper.s_blocks_count;
		start++;
	}

	if (!(bh = getblk(dev_journal, start, journal->j_blocksize))) {
		retval = EXT2_ET_NO_MEMORY;
		goto errout;
	}

	journal->j_sb_buffer = bh;
	journal->j_superblock = (journal_superblock_t *)bh->b_data;

#ifdef USE_INODE_IO
	ext2fs_free_mem(&j_inode);
#endif

	*ret_journal = journal;
	return 0;

errout:
	ext2fs_free_mem(&dev_fs);
	ext2fs_free_mem(&j_inode);
	ext2fs_free_mem(&journal);
	return retval;
}

static errcode_t e2fsck_journal_fix_bad_inode(e2fsck_t ctx,
					      struct problem_context *pctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	int recover = ctx->fs->super->s_feature_incompat &
		EXT3_FEATURE_INCOMPAT_RECOVER;
	int has_journal = ctx->fs->super->s_feature_compat &
		EXT3_FEATURE_COMPAT_HAS_JOURNAL;

	if (has_journal || sb->s_journal_inum) {
		/* The journal inode is bogus, remove and force full fsck */
		pctx->ino = sb->s_journal_inum;
		if (fix_problem(ctx, PR_0_JOURNAL_BAD_INODE, pctx)) {
			if (has_journal && sb->s_journal_inum)
				printf("*** ext3 journal has been deleted - "
				       "filesystem is now ext2 only ***\n\n");
			sb->s_feature_compat &= ~EXT3_FEATURE_COMPAT_HAS_JOURNAL;
			sb->s_journal_inum = 0;
			ctx->flags |= E2F_FLAG_JOURNAL_INODE; /* FIXME: todo */
			e2fsck_clear_recover(ctx, 1);
			return 0;
		}
		return EXT2_ET_BAD_INODE_NUM;
	} else if (recover) {
		if (fix_problem(ctx, PR_0_JOURNAL_RECOVER_SET, pctx)) {
			e2fsck_clear_recover(ctx, 1);
			return 0;
		}
		return EXT2_ET_UNSUPP_FEATURE;
	}
	return 0;
}

#define V1_SB_SIZE      0x0024
static void clear_v2_journal_fields(journal_t *journal)
{
	e2fsck_t ctx = journal->j_dev->k_ctx;
	struct problem_context pctx;

	clear_problem_context(&pctx);

	if (!fix_problem(ctx, PR_0_CLEAR_V2_JOURNAL, &pctx))
		return;

	memset(((char *) journal->j_superblock) + V1_SB_SIZE, 0,
	       ctx->fs->blocksize-V1_SB_SIZE);
	mark_buffer_dirty(journal->j_sb_buffer);
}


static errcode_t e2fsck_journal_load(journal_t *journal)
{
	e2fsck_t ctx = journal->j_dev->k_ctx;
	journal_superblock_t *jsb;
	struct buffer_head *jbh = journal->j_sb_buffer;
	struct problem_context pctx;

	clear_problem_context(&pctx);

	ll_rw_block(READ, 1, &jbh);
	if (jbh->b_err) {
		bb_error_msg(_("reading journal superblock"));
		return jbh->b_err;
	}

	jsb = journal->j_superblock;
	/* If we don't even have JFS_MAGIC, we probably have a wrong inode */
	if (jsb->s_header.h_magic != htonl(JFS_MAGIC_NUMBER))
		return e2fsck_journal_fix_bad_inode(ctx, &pctx);

	switch (ntohl(jsb->s_header.h_blocktype)) {
	case JFS_SUPERBLOCK_V1:
		journal->j_format_version = 1;
		if (jsb->s_feature_compat ||
		    jsb->s_feature_incompat ||
		    jsb->s_feature_ro_compat ||
		    jsb->s_nr_users)
			clear_v2_journal_fields(journal);
		break;

	case JFS_SUPERBLOCK_V2:
		journal->j_format_version = 2;
		if (ntohl(jsb->s_nr_users) > 1 &&
		    uuid_is_null(ctx->fs->super->s_journal_uuid))
			clear_v2_journal_fields(journal);
		if (ntohl(jsb->s_nr_users) > 1) {
			fix_problem(ctx, PR_0_JOURNAL_UNSUPP_MULTIFS, &pctx);
			return EXT2_ET_JOURNAL_UNSUPP_VERSION;
		}
		break;

	/*
	 * These should never appear in a journal super block, so if
	 * they do, the journal is badly corrupted.
	 */
	case JFS_DESCRIPTOR_BLOCK:
	case JFS_COMMIT_BLOCK:
	case JFS_REVOKE_BLOCK:
		return EXT2_ET_CORRUPT_SUPERBLOCK;

	/* If we don't understand the superblock major type, but there
	 * is a magic number, then it is likely to be a new format we
	 * just don't understand, so leave it alone. */
	default:
		return EXT2_ET_JOURNAL_UNSUPP_VERSION;
	}

	if (JFS_HAS_INCOMPAT_FEATURE(journal, ~JFS_KNOWN_INCOMPAT_FEATURES))
		return EXT2_ET_UNSUPP_FEATURE;

	if (JFS_HAS_RO_COMPAT_FEATURE(journal, ~JFS_KNOWN_ROCOMPAT_FEATURES))
		return EXT2_ET_RO_UNSUPP_FEATURE;

	/* We have now checked whether we know enough about the journal
	 * format to be able to proceed safely, so any other checks that
	 * fail we should attempt to recover from. */
	if (jsb->s_blocksize != htonl(journal->j_blocksize)) {
		bb_error_msg(_("%s: no valid journal superblock found"),
			ctx->device_name);
		return EXT2_ET_CORRUPT_SUPERBLOCK;
	}

	if (ntohl(jsb->s_maxlen) < journal->j_maxlen)
		journal->j_maxlen = ntohl(jsb->s_maxlen);
	else if (ntohl(jsb->s_maxlen) > journal->j_maxlen) {
		bb_error_msg(_("%s: journal too short"),
			ctx->device_name);
		return EXT2_ET_CORRUPT_SUPERBLOCK;
	}

	journal->j_tail_sequence = ntohl(jsb->s_sequence);
	journal->j_transaction_sequence = journal->j_tail_sequence;
	journal->j_tail = ntohl(jsb->s_start);
	journal->j_first = ntohl(jsb->s_first);
	journal->j_last = ntohl(jsb->s_maxlen);

	return 0;
}

static void e2fsck_journal_reset_super(e2fsck_t ctx, journal_superblock_t *jsb,
				       journal_t *journal)
{
	char *p;
	union {
		uuid_t uuid;
		__u32 val[4];
	} u;
	__u32 new_seq = 0;
	int i;

	/* Leave a valid existing V1 superblock signature alone.
	 * Anything unrecognizable we overwrite with a new V2
	 * signature. */

	if (jsb->s_header.h_magic != htonl(JFS_MAGIC_NUMBER) ||
	    jsb->s_header.h_blocktype != htonl(JFS_SUPERBLOCK_V1)) {
		jsb->s_header.h_magic = htonl(JFS_MAGIC_NUMBER);
		jsb->s_header.h_blocktype = htonl(JFS_SUPERBLOCK_V2);
	}

	/* Zero out everything else beyond the superblock header */

	p = ((char *) jsb) + sizeof(journal_header_t);
	memset (p, 0, ctx->fs->blocksize-sizeof(journal_header_t));

	jsb->s_blocksize = htonl(ctx->fs->blocksize);
	jsb->s_maxlen = htonl(journal->j_maxlen);
	jsb->s_first = htonl(1);

	/* Initialize the journal sequence number so that there is "no"
	 * chance we will find old "valid" transactions in the journal.
	 * This avoids the need to zero the whole journal (slow to do,
	 * and risky when we are just recovering the filesystem).
	 */
	uuid_generate(u.uuid);
	for (i = 0; i < 4; i ++)
		new_seq ^= u.val[i];
	jsb->s_sequence = htonl(new_seq);

	mark_buffer_dirty(journal->j_sb_buffer);
	ll_rw_block(WRITE, 1, &journal->j_sb_buffer);
}

static errcode_t e2fsck_journal_fix_corrupt_super(e2fsck_t ctx,
						  journal_t *journal,
						  struct problem_context *pctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	int recover = ctx->fs->super->s_feature_incompat &
		EXT3_FEATURE_INCOMPAT_RECOVER;

	if (sb->s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL) {
		if (fix_problem(ctx, PR_0_JOURNAL_BAD_SUPER, pctx)) {
			e2fsck_journal_reset_super(ctx, journal->j_superblock,
						   journal);
			journal->j_transaction_sequence = 1;
			e2fsck_clear_recover(ctx, recover);
			return 0;
		}
		return EXT2_ET_CORRUPT_SUPERBLOCK;
	} else if (e2fsck_journal_fix_bad_inode(ctx, pctx))
		return EXT2_ET_CORRUPT_SUPERBLOCK;

	return 0;
}

static void e2fsck_journal_release(e2fsck_t ctx, journal_t *journal,
				   int reset, int drop)
{
	journal_superblock_t *jsb;

	if (drop)
		mark_buffer_clean(journal->j_sb_buffer);
	else if (!(ctx->options & E2F_OPT_READONLY)) {
		jsb = journal->j_superblock;
		jsb->s_sequence = htonl(journal->j_transaction_sequence);
		if (reset)
			jsb->s_start = 0; /* this marks the journal as empty */
		mark_buffer_dirty(journal->j_sb_buffer);
	}
	brelse(journal->j_sb_buffer);

	if (ctx->journal_io) {
		if (ctx->fs && ctx->fs->io != ctx->journal_io)
			io_channel_close(ctx->journal_io);
		ctx->journal_io = 0;
	}

#ifndef USE_INODE_IO
	ext2fs_free_mem(&journal->j_inode);
#endif
	ext2fs_free_mem(&journal->j_fs_dev);
	ext2fs_free_mem(&journal);
}

/*
 * This function makes sure that the superblock fields regarding the
 * journal are consistent.
 */
static int e2fsck_check_ext3_journal(e2fsck_t ctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	journal_t *journal;
	int recover = ctx->fs->super->s_feature_incompat &
		EXT3_FEATURE_INCOMPAT_RECOVER;
	struct problem_context pctx;
	problem_t problem;
	int reset = 0, force_fsck = 0;
	int retval;

	/* If we don't have any journal features, don't do anything more */
	if (!(sb->s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL) &&
	    !recover && sb->s_journal_inum == 0 && sb->s_journal_dev == 0 &&
	    uuid_is_null(sb->s_journal_uuid))
		return 0;

	clear_problem_context(&pctx);
	pctx.num = sb->s_journal_inum;

	retval = e2fsck_get_journal(ctx, &journal);
	if (retval) {
		if ((retval == EXT2_ET_BAD_INODE_NUM) ||
		    (retval == EXT2_ET_BAD_BLOCK_NUM) ||
		    (retval == EXT2_ET_JOURNAL_TOO_SMALL) ||
		    (retval == EXT2_ET_NO_JOURNAL))
			return e2fsck_journal_fix_bad_inode(ctx, &pctx);
		return retval;
	}

	retval = e2fsck_journal_load(journal);
	if (retval) {
		if ((retval == EXT2_ET_CORRUPT_SUPERBLOCK) ||
		    ((retval == EXT2_ET_UNSUPP_FEATURE) &&
		    (!fix_problem(ctx, PR_0_JOURNAL_UNSUPP_INCOMPAT,
				  &pctx))) ||
		    ((retval == EXT2_ET_RO_UNSUPP_FEATURE) &&
		    (!fix_problem(ctx, PR_0_JOURNAL_UNSUPP_ROCOMPAT,
				  &pctx))) ||
		    ((retval == EXT2_ET_JOURNAL_UNSUPP_VERSION) &&
		    (!fix_problem(ctx, PR_0_JOURNAL_UNSUPP_VERSION, &pctx))))
			retval = e2fsck_journal_fix_corrupt_super(ctx, journal,
								  &pctx);
		e2fsck_journal_release(ctx, journal, 0, 1);
		return retval;
	}

	/*
	 * We want to make the flags consistent here.  We will not leave with
	 * needs_recovery set but has_journal clear.  We can't get in a loop
	 * with -y, -n, or -p, only if a user isn't making up their mind.
	 */
no_has_journal:
	if (!(sb->s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL)) {
		recover = sb->s_feature_incompat & EXT3_FEATURE_INCOMPAT_RECOVER;
		pctx.str = "inode";
		if (fix_problem(ctx, PR_0_JOURNAL_HAS_JOURNAL, &pctx)) {
			if (recover &&
			    !fix_problem(ctx, PR_0_JOURNAL_RECOVER_SET, &pctx))
				goto no_has_journal;
			/*
			 * Need a full fsck if we are releasing a
			 * journal stored on a reserved inode.
			 */
			force_fsck = recover ||
				(sb->s_journal_inum < EXT2_FIRST_INODE(sb));
			/* Clear all of the journal fields */
			sb->s_journal_inum = 0;
			sb->s_journal_dev = 0;
			memset(sb->s_journal_uuid, 0,
			       sizeof(sb->s_journal_uuid));
			e2fsck_clear_recover(ctx, force_fsck);
		} else if (!(ctx->options & E2F_OPT_READONLY)) {
			sb->s_feature_compat |= EXT3_FEATURE_COMPAT_HAS_JOURNAL;
			ext2fs_mark_super_dirty(ctx->fs);
		}
	}

	if (sb->s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL &&
	    !(sb->s_feature_incompat & EXT3_FEATURE_INCOMPAT_RECOVER) &&
	    journal->j_superblock->s_start != 0) {
		/* Print status information */
		fix_problem(ctx, PR_0_JOURNAL_RECOVERY_CLEAR, &pctx);
		if (ctx->superblock)
			problem = PR_0_JOURNAL_RUN_DEFAULT;
		else
			problem = PR_0_JOURNAL_RUN;
		if (fix_problem(ctx, problem, &pctx)) {
			ctx->options |= E2F_OPT_FORCE;
			sb->s_feature_incompat |=
				EXT3_FEATURE_INCOMPAT_RECOVER;
			ext2fs_mark_super_dirty(ctx->fs);
		} else if (fix_problem(ctx,
				       PR_0_JOURNAL_RESET_JOURNAL, &pctx)) {
			reset = 1;
			sb->s_state &= ~EXT2_VALID_FS;
			ext2fs_mark_super_dirty(ctx->fs);
		}
		/*
		 * If the user answers no to the above question, we
		 * ignore the fact that journal apparently has data;
		 * accidentally replaying over valid data would be far
		 * worse than skipping a questionable recovery.
		 *
		 * XXX should we abort with a fatal error here?  What
		 * will the ext3 kernel code do if a filesystem with
		 * !NEEDS_RECOVERY but with a non-zero
		 * journal->j_superblock->s_start is mounted?
		 */
	}

	e2fsck_journal_release(ctx, journal, reset, 0);
	return retval;
}

static errcode_t recover_ext3_journal(e2fsck_t ctx)
{
	journal_t *journal;
	int retval;

	journal_init_revoke_caches();
	retval = e2fsck_get_journal(ctx, &journal);
	if (retval)
		return retval;

	retval = e2fsck_journal_load(journal);
	if (retval)
		goto errout;

	retval = journal_init_revoke(journal, 1024);
	if (retval)
		goto errout;

	retval = -journal_recover(journal);
	if (retval)
		goto errout;

	if (journal->j_superblock->s_errno) {
		ctx->fs->super->s_state |= EXT2_ERROR_FS;
		ext2fs_mark_super_dirty(ctx->fs);
		journal->j_superblock->s_errno = 0;
		mark_buffer_dirty(journal->j_sb_buffer);
	}

errout:
	journal_destroy_revoke(journal);
	journal_destroy_revoke_caches();
	e2fsck_journal_release(ctx, journal, 1, 0);
	return retval;
}

static int e2fsck_run_ext3_journal(e2fsck_t ctx)
{
	io_manager io_ptr = ctx->fs->io->manager;
	int blocksize = ctx->fs->blocksize;
	errcode_t       retval, recover_retval;

	printf(_("%s: recovering journal\n"), ctx->device_name);
	if (ctx->options & E2F_OPT_READONLY) {
		printf(_("%s: won't do journal recovery while read-only\n"),
		       ctx->device_name);
		return EXT2_ET_FILE_RO;
	}

	if (ctx->fs->flags & EXT2_FLAG_DIRTY)
		ext2fs_flush(ctx->fs);  /* Force out any modifications */

	recover_retval = recover_ext3_journal(ctx);

	/*
	 * Reload the filesystem context to get up-to-date data from disk
	 * because journal recovery will change the filesystem under us.
	 */
	ext2fs_close(ctx->fs);
	retval = ext2fs_open(ctx->filesystem_name, EXT2_FLAG_RW,
			     ctx->superblock, blocksize, io_ptr,
			     &ctx->fs);

	if (retval) {
		bb_error_msg(_("while trying to re-open %s"),
			ctx->device_name);
		bb_error_msg_and_die(0);
	}
	ctx->fs->priv_data = ctx;

	/* Set the superblock flags */
	e2fsck_clear_recover(ctx, recover_retval);
	return recover_retval;
}

/*
 * This function will move the journal inode from a visible file in
 * the filesystem directory hierarchy to the reserved inode if necessary.
 */
static const char *const journal_names[] = {
	".journal", "journal", ".journal.dat", "journal.dat", 0 };

static void e2fsck_move_ext3_journal(e2fsck_t ctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	struct problem_context  pctx;
	struct ext2_inode       inode;
	ext2_filsys             fs = ctx->fs;
	ext2_ino_t              ino;
	errcode_t               retval;
	const char *const *    cpp;
	int                     group, mount_flags;

	clear_problem_context(&pctx);

	/*
	 * If the filesystem is opened read-only, or there is no
	 * journal, then do nothing.
	 */
	if ((ctx->options & E2F_OPT_READONLY) ||
	    (sb->s_journal_inum == 0) ||
	    !(sb->s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL))
		return;

	/*
	 * Read in the journal inode
	 */
	if (ext2fs_read_inode(fs, sb->s_journal_inum, &inode) != 0)
		return;

	/*
	 * If it's necessary to backup the journal inode, do so.
	 */
	if ((sb->s_jnl_backup_type == 0) ||
	    ((sb->s_jnl_backup_type == EXT3_JNL_BACKUP_BLOCKS) &&
	     memcmp(inode.i_block, sb->s_jnl_blocks, EXT2_N_BLOCKS*4))) {
		if (fix_problem(ctx, PR_0_BACKUP_JNL, &pctx)) {
			memcpy(sb->s_jnl_blocks, inode.i_block,
			       EXT2_N_BLOCKS*4);
			sb->s_jnl_blocks[16] = inode.i_size;
			sb->s_jnl_backup_type = EXT3_JNL_BACKUP_BLOCKS;
			ext2fs_mark_super_dirty(fs);
			fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
		}
	}

	/*
	 * If the journal is already the hidden inode, then do nothing
	 */
	if (sb->s_journal_inum == EXT2_JOURNAL_INO)
		return;

	/*
	 * The journal inode had better have only one link and not be readable.
	 */
	if (inode.i_links_count != 1)
		return;

	/*
	 * If the filesystem is mounted, or we can't tell whether
	 * or not it's mounted, do nothing.
	 */
	retval = ext2fs_check_if_mounted(ctx->filesystem_name, &mount_flags);
	if (retval || (mount_flags & EXT2_MF_MOUNTED))
		return;

	/*
	 * If we can't find the name of the journal inode, then do
	 * nothing.
	 */
	for (cpp = journal_names; *cpp; cpp++) {
		retval = ext2fs_lookup(fs, EXT2_ROOT_INO, *cpp,
				       strlen(*cpp), 0, &ino);
		if ((retval == 0) && (ino == sb->s_journal_inum))
			break;
	}
	if (*cpp == 0)
		return;

	/* We need the inode bitmap to be loaded */
	retval = ext2fs_read_bitmaps(fs);
	if (retval)
		return;

	pctx.str = *cpp;
	if (!fix_problem(ctx, PR_0_MOVE_JOURNAL, &pctx))
		return;

	/*
	 * OK, we've done all the checks, let's actually move the
	 * journal inode.  Errors at this point mean we need to force
	 * an ext2 filesystem check.
	 */
	if ((retval = ext2fs_unlink(fs, EXT2_ROOT_INO, *cpp, ino, 0)) != 0)
		goto err_out;
	if ((retval = ext2fs_write_inode(fs, EXT2_JOURNAL_INO, &inode)) != 0)
		goto err_out;
	sb->s_journal_inum = EXT2_JOURNAL_INO;
	ext2fs_mark_super_dirty(fs);
	fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
	inode.i_links_count = 0;
	inode.i_dtime = time(NULL);
	if ((retval = ext2fs_write_inode(fs, ino, &inode)) != 0)
		goto err_out;

	group = ext2fs_group_of_ino(fs, ino);
	ext2fs_unmark_inode_bitmap(fs->inode_map, ino);
	ext2fs_mark_ib_dirty(fs);
	fs->group_desc[group].bg_free_inodes_count++;
	fs->super->s_free_inodes_count++;
	return;

err_out:
	pctx.errcode = retval;
	fix_problem(ctx, PR_0_ERR_MOVE_JOURNAL, &pctx);
	fs->super->s_state &= ~EXT2_VALID_FS;
	ext2fs_mark_super_dirty(fs);
}

/*
 * message.c --- print e2fsck messages (with compression)
 *
 * print_e2fsck_message() prints a message to the user, using
 * compression techniques and expansions of abbreviations.
 *
 * The following % expansions are supported:
 *
 *      %b      <blk>                   block number
 *      %B      <blkcount>              integer
 *      %c      <blk2>                  block number
 *      %Di     <dirent>->ino           inode number
 *      %Dn     <dirent>->name          string
 *      %Dr     <dirent>->rec_len
 *      %Dl     <dirent>->name_len
 *      %Dt     <dirent>->filetype
 *      %d      <dir>                   inode number
 *      %g      <group>                 integer
 *      %i      <ino>                   inode number
 *      %Is     <inode> -> i_size
 *      %IS     <inode> -> i_extra_isize
 *      %Ib     <inode> -> i_blocks
 *      %Il     <inode> -> i_links_count
 *      %Im     <inode> -> i_mode
 *      %IM     <inode> -> i_mtime
 *      %IF     <inode> -> i_faddr
 *      %If     <inode> -> i_file_acl
 *      %Id     <inode> -> i_dir_acl
 *      %Iu     <inode> -> i_uid
 *      %Ig     <inode> -> i_gid
 *      %j      <ino2>                  inode number
 *      %m      <com_err error message>
 *      %N      <num>
 *      %p      ext2fs_get_pathname of directory <ino>
 *      %P      ext2fs_get_pathname of <dirent>->ino with <ino2> as
 *                      the containing directory.  (If dirent is NULL
 *                      then return the pathname of directory <ino2>)
 *      %q      ext2fs_get_pathname of directory <dir>
 *      %Q      ext2fs_get_pathname of directory <ino> with <dir> as
 *                      the containing directory.
 *      %s      <str>                   miscellaneous string
 *      %S      backup superblock
 *      %X      <num> hexadecimal format
 *
 * The following '@' expansions are supported:
 *
 *      @a      extended attribute
 *      @A      error allocating
 *      @b      block
 *      @B      bitmap
 *      @c      compress
 *      @C      conflicts with some other fs block
 *      @D      deleted
 *      @d      directory
 *      @e      entry
 *      @E      Entry '%Dn' in %p (%i)
 *      @f      filesystem
 *      @F      for @i %i (%Q) is
 *      @g      group
 *      @h      HTREE directory inode
 *      @i      inode
 *      @I      illegal
 *      @j      journal
 *      @l      lost+found
 *      @L      is a link
 *      @m      multiply-claimed
 *      @n      invalid
 *      @o      orphaned
 *      @p      problem in
 *      @r      root inode
 *      @s      should be
 *      @S      superblock
 *      @u      unattached
 *      @v      device
 *      @z      zero-length
 */


/*
 * This structure defines the abbreviations used by the text strings
 * below.  The first character in the string is the index letter.  An
 * abbreviation of the form '@<i>' is expanded by looking up the index
 * letter <i> in the table below.
 */
static const char *const abbrevs[] = {
	N_("aextended attribute"),
	N_("Aerror allocating"),
	N_("bblock"),
	N_("Bbitmap"),
	N_("ccompress"),
	N_("Cconflicts with some other fs @b"),
	N_("iinode"),
	N_("Iillegal"),
	N_("jjournal"),
	N_("Ddeleted"),
	N_("ddirectory"),
	N_("eentry"),
	N_("E@e '%Dn' in %p (%i)"),
	N_("ffilesystem"),
	N_("Ffor @i %i (%Q) is"),
	N_("ggroup"),
	N_("hHTREE @d @i"),
	N_("llost+found"),
	N_("Lis a link"),
	N_("mmultiply-claimed"),
	N_("ninvalid"),
	N_("oorphaned"),
	N_("pproblem in"),
	N_("rroot @i"),
	N_("sshould be"),
	N_("Ssuper@b"),
	N_("uunattached"),
	N_("vdevice"),
	N_("zzero-length"),
	"@@",
	0
	};

/*
 * Give more user friendly names to the "special" inodes.
 */
#define num_special_inodes      11
static const char *const special_inode_name[] =
{
	N_("<The NULL inode>"),                 /* 0 */
	N_("<The bad blocks inode>"),           /* 1 */
	"/",                                    /* 2 */
	N_("<The ACL index inode>"),            /* 3 */
	N_("<The ACL data inode>"),             /* 4 */
	N_("<The boot loader inode>"),          /* 5 */
	N_("<The undelete directory inode>"),   /* 6 */
	N_("<The group descriptor inode>"),     /* 7 */
	N_("<The journal inode>"),              /* 8 */
	N_("<Reserved inode 9>"),               /* 9 */
	N_("<Reserved inode 10>"),              /* 10 */
};

/*
 * This function does "safe" printing.  It will convert non-printable
 * ASCII characters using '^' and M- notation.
 */
static void safe_print(const char *cp, int len)
{
	unsigned char   ch;

	if (len < 0)
		len = strlen(cp);

	while (len--) {
		ch = *cp++;
		if (ch > 128) {
			fputs("M-", stdout);
			ch -= 128;
		}
		if ((ch < 32) || (ch == 0x7f)) {
			bb_putchar('^');
			ch ^= 0x40; /* ^@, ^A, ^B; ^? for DEL */
		}
		bb_putchar(ch);
	}
}


/*
 * This function prints a pathname, using the ext2fs_get_pathname
 * function
 */
static void print_pathname(ext2_filsys fs, ext2_ino_t dir, ext2_ino_t ino)
{
	errcode_t       retval;
	char            *path;

	if (!dir && (ino < num_special_inodes)) {
		fputs(_(special_inode_name[ino]), stdout);
		return;
	}

	retval = ext2fs_get_pathname(fs, dir, ino, &path);
	if (retval)
		fputs("???", stdout);
	else {
		safe_print(path, -1);
		ext2fs_free_mem(&path);
	}
}

static void print_e2fsck_message(e2fsck_t ctx, const char *msg,
			  struct problem_context *pctx, int first);
/*
 * This function handles the '@' expansion.  We allow recursive
 * expansion; an @ expression can contain further '@' and '%'
 * expressions.
 */
static void expand_at_expression(e2fsck_t ctx, char ch,
					  struct problem_context *pctx,
					  int *first)
{
	const char *const *cpp;
	const char *str;

	/* Search for the abbreviation */
	for (cpp = abbrevs; *cpp; cpp++) {
		if (ch == *cpp[0])
			break;
	}
	if (*cpp) {
		str = _(*cpp) + 1;
		if (*first && islower(*str)) {
			*first = 0;
			bb_putchar(toupper(*str++));
		}
		print_e2fsck_message(ctx, str, pctx, *first);
	} else
		printf("@%c", ch);
}

/*
 * This function expands '%IX' expressions
 */
static void expand_inode_expression(char ch,
					     struct problem_context *ctx)
{
	struct ext2_inode       *inode;
	struct ext2_inode_large *large_inode;
	char *                  time_str;
	time_t                  t;
	int                     do_gmt = -1;

	if (!ctx || !ctx->inode)
		goto no_inode;

	inode = ctx->inode;
	large_inode = (struct ext2_inode_large *) inode;

	switch (ch) {
	case 's':
		if (LINUX_S_ISDIR(inode->i_mode))
			printf("%u", inode->i_size);
		else {
			printf("%"PRIu64, (inode->i_size |
					((uint64_t) inode->i_size_high << 32)));
		}
		break;
	case 'S':
		printf("%u", large_inode->i_extra_isize);
		break;
	case 'b':
		printf("%u", inode->i_blocks);
		break;
	case 'l':
		printf("%d", inode->i_links_count);
		break;
	case 'm':
		printf("0%o", inode->i_mode);
		break;
	case 'M':
		/* The diet libc doesn't respect the TZ environemnt variable */
		if (do_gmt == -1) {
			time_str = getenv("TZ");
			if (!time_str)
				time_str = "";
			do_gmt = !strcmp(time_str, "GMT");
		}
		t = inode->i_mtime;
		time_str = asctime(do_gmt ? gmtime(&t) : localtime(&t));
		printf("%.24s", time_str);
		break;
	case 'F':
		printf("%u", inode->i_faddr);
		break;
	case 'f':
		printf("%u", inode->i_file_acl);
		break;
	case 'd':
		printf("%u", (LINUX_S_ISDIR(inode->i_mode) ?
			      inode->i_dir_acl : 0));
		break;
	case 'u':
		printf("%d", (inode->i_uid |
			      (inode->osd2.linux2.l_i_uid_high << 16)));
		break;
	case 'g':
		printf("%d", (inode->i_gid |
			      (inode->osd2.linux2.l_i_gid_high << 16)));
		break;
	default:
	no_inode:
		printf("%%I%c", ch);
		break;
	}
}

/*
 * This function expands '%dX' expressions
 */
static void expand_dirent_expression(char ch,
					      struct problem_context *ctx)
{
	struct ext2_dir_entry   *dirent;
	int     len;

	if (!ctx || !ctx->dirent)
		goto no_dirent;

	dirent = ctx->dirent;

	switch (ch) {
	case 'i':
		printf("%u", dirent->inode);
		break;
	case 'n':
		len = dirent->name_len & 0xFF;
		if (len > EXT2_NAME_LEN)
			len = EXT2_NAME_LEN;
		if (len > dirent->rec_len)
			len = dirent->rec_len;
		safe_print(dirent->name, len);
		break;
	case 'r':
		printf("%u", dirent->rec_len);
		break;
	case 'l':
		printf("%u", dirent->name_len & 0xFF);
		break;
	case 't':
		printf("%u", dirent->name_len >> 8);
		break;
	default:
	no_dirent:
		printf("%%D%c", ch);
		break;
	}
}

static void expand_percent_expression(ext2_filsys fs, char ch,
					       struct problem_context *ctx)
{
	if (!ctx)
		goto no_context;

	switch (ch) {
	case '%':
		bb_putchar('%');
		break;
	case 'b':
		printf("%u", ctx->blk);
		break;
	case 'B':
		printf("%"PRIi64, ctx->blkcount);
		break;
	case 'c':
		printf("%u", ctx->blk2);
		break;
	case 'd':
		printf("%u", ctx->dir);
		break;
	case 'g':
		printf("%d", ctx->group);
		break;
	case 'i':
		printf("%u", ctx->ino);
		break;
	case 'j':
		printf("%u", ctx->ino2);
		break;
	case 'm':
		fputs(error_message(ctx->errcode), stdout);
		break;
	case 'N':
		printf("%"PRIi64, ctx->num);
		break;
	case 'p':
		print_pathname(fs, ctx->ino, 0);
		break;
	case 'P':
		print_pathname(fs, ctx->ino2,
			       ctx->dirent ? ctx->dirent->inode : 0);
		break;
	case 'q':
		print_pathname(fs, ctx->dir, 0);
		break;
	case 'Q':
		print_pathname(fs, ctx->dir, ctx->ino);
		break;
	case 'S':
		printf("%d", get_backup_sb(NULL, fs, NULL, NULL));
		break;
	case 's':
		fputs((ctx->str ? ctx->str : "NULL"), stdout);
		break;
	case 'X':
		printf("0x%"PRIi64, ctx->num);
		break;
	default:
	no_context:
		printf("%%%c", ch);
		break;
	}
}


static void print_e2fsck_message(e2fsck_t ctx, const char *msg,
			  struct problem_context *pctx, int first)
{
	ext2_filsys fs = ctx->fs;
	const char *    cp;
	int             i;

	e2fsck_clear_progbar(ctx);
	for (cp = msg; *cp; cp++) {
		if (cp[0] == '@') {
			cp++;
			expand_at_expression(ctx, *cp, pctx, &first);
		} else if (cp[0] == '%' && cp[1] == 'I') {
			cp += 2;
			expand_inode_expression(*cp, pctx);
		} else if (cp[0] == '%' && cp[1] == 'D') {
			cp += 2;
			expand_dirent_expression(*cp, pctx);
		} else if ((cp[0] == '%')) {
			cp++;
			expand_percent_expression(fs, *cp, pctx);
		} else {
			for (i=0; cp[i]; i++)
				if ((cp[i] == '@') || cp[i] == '%')
					break;
			printf("%.*s", i, cp);
			cp += i-1;
		}
		first = 0;
	}
}


/*
 * region.c --- code which manages allocations within a region.
 */

struct region_el {
	region_addr_t   start;
	region_addr_t   end;
	struct region_el *next;
};

struct region_struct {
	region_addr_t   min;
	region_addr_t   max;
	struct region_el *allocated;
};

static region_t region_create(region_addr_t min, region_addr_t max)
{
	region_t        region;

	region = xzalloc(sizeof(struct region_struct));
	region->min = min;
	region->max = max;
	return region;
}

static void region_free(region_t region)
{
	struct region_el        *r, *next;

	for (r = region->allocated; r; r = next) {
		next = r->next;
		free(r);
	}
	memset(region, 0, sizeof(struct region_struct));
	free(region);
}

static int region_allocate(region_t region, region_addr_t start, int n)
{
	struct region_el        *r, *new_region, *prev, *next;
	region_addr_t end;

	end = start+n;
	if ((start < region->min) || (end > region->max))
		return -1;
	if (n == 0)
		return 1;

	/*
	 * Search through the linked list.  If we find that it
	 * conflicts witih something that's already allocated, return
	 * 1; if we can find an existing region which we can grow, do
	 * so.  Otherwise, stop when we find the appropriate place
	 * insert a new region element into the linked list.
	 */
	for (r = region->allocated, prev=NULL; r; prev = r, r = r->next) {
		if (((start >= r->start) && (start < r->end)) ||
		    ((end > r->start) && (end <= r->end)) ||
		    ((start <= r->start) && (end >= r->end)))
			return 1;
		if (end == r->start) {
			r->start = start;
			return 0;
		}
		if (start == r->end) {
			if ((next = r->next)) {
				if (end > next->start)
					return 1;
				if (end == next->start) {
					r->end = next->end;
					r->next = next->next;
					free(next);
					return 0;
				}
			}
			r->end = end;
			return 0;
		}
		if (start < r->start)
			break;
	}
	/*
	 * Insert a new region element structure into the linked list
	 */
	new_region = xmalloc(sizeof(struct region_el));
	new_region->start = start;
	new_region->end = start + n;
	new_region->next = r;
	if (prev)
		prev->next = new_region;
	else
		region->allocated = new_region;
	return 0;
}

/*
 * pass1.c -- pass #1 of e2fsck: sequential scan of the inode table
 *
 * Pass 1 of e2fsck iterates over all the inodes in the filesystems,
 * and applies the following tests to each inode:
 *
 *      - The mode field of the inode must be legal.
 *      - The size and block count fields of the inode are correct.
 *      - A data block must not be used by another inode
 *
 * Pass 1 also gathers the collects the following information:
 *
 *      - A bitmap of which inodes are in use.          (inode_used_map)
 *      - A bitmap of which inodes are directories.     (inode_dir_map)
 *      - A bitmap of which inodes are regular files.   (inode_reg_map)
 *      - A bitmap of which inodes have bad fields.     (inode_bad_map)
 *      - A bitmap of which inodes are imagic inodes.   (inode_imagic_map)
 *      - A bitmap of which blocks are in use.          (block_found_map)
 *      - A bitmap of which blocks are in use by two inodes     (block_dup_map)
 *      - The data blocks of the directory inodes.      (dir_map)
 *
 * Pass 1 is designed to stash away enough information so that the
 * other passes should not need to read in the inode information
 * during the normal course of a filesystem check.  (Althogh if an
 * inconsistency is detected, other passes may need to read in an
 * inode to fix it.)
 *
 * Note that pass 1B will be invoked if there are any duplicate blocks
 * found.
 */


static int process_block(ext2_filsys fs, blk_t  *blocknr,
			 e2_blkcnt_t blockcnt, blk_t ref_blk,
			 int ref_offset, void *priv_data);
static int process_bad_block(ext2_filsys fs, blk_t *block_nr,
			     e2_blkcnt_t blockcnt, blk_t ref_blk,
			     int ref_offset, void *priv_data);
static void check_blocks(e2fsck_t ctx, struct problem_context *pctx,
			 char *block_buf);
static void mark_table_blocks(e2fsck_t ctx);
static void alloc_imagic_map(e2fsck_t ctx);
static void mark_inode_bad(e2fsck_t ctx, ino_t ino);
static void handle_fs_bad_blocks(e2fsck_t ctx);
static void process_inodes(e2fsck_t ctx, char *block_buf);
static int process_inode_cmp(const void *a, const void *b);
static errcode_t scan_callback(ext2_filsys fs,
				  dgrp_t group, void * priv_data);
static void adjust_extattr_refcount(e2fsck_t ctx, ext2_refcount_t refcount,
				    char *block_buf, int adjust_sign);
/* static char *describe_illegal_block(ext2_filsys fs, blk_t block); */

static void e2fsck_write_inode_full(e2fsck_t ctx, unsigned long ino,
			       struct ext2_inode * inode, int bufsize,
			       const char *proc);

struct process_block_struct_1 {
	ext2_ino_t      ino;
	unsigned        is_dir:1, is_reg:1, clear:1, suppress:1,
				fragmented:1, compressed:1, bbcheck:1;
	blk_t           num_blocks;
	blk_t           max_blocks;
	e2_blkcnt_t     last_block;
	int             num_illegal_blocks;
	blk_t           previous_block;
	struct ext2_inode *inode;
	struct problem_context *pctx;
	ext2fs_block_bitmap fs_meta_blocks;
	e2fsck_t        ctx;
};

struct process_inode_block {
	ext2_ino_t ino;
	struct ext2_inode inode;
};

struct scan_callback_struct {
	e2fsck_t        ctx;
	char            *block_buf;
};

/*
 * For the inodes to process list.
 */
static struct process_inode_block *inodes_to_process;
static int process_inode_count;

static __u64 ext2_max_sizes[EXT2_MAX_BLOCK_LOG_SIZE -
			    EXT2_MIN_BLOCK_LOG_SIZE + 1];

/*
 * Free all memory allocated by pass1 in preparation for restarting
 * things.
 */
static void unwind_pass1(void)
{
	ext2fs_free_mem(&inodes_to_process);
}

/*
 * Check to make sure a device inode is real.  Returns 1 if the device
 * checks out, 0 if not.
 *
 * Note: this routine is now also used to check FIFO's and Sockets,
 * since they have the same requirement; the i_block fields should be
 * zero.
 */
static int
e2fsck_pass1_check_device_inode(ext2_filsys fs, struct ext2_inode *inode)
{
	int     i;

	/*
	 * If i_blocks is non-zero, or the index flag is set, then
	 * this is a bogus device/fifo/socket
	 */
	if ((ext2fs_inode_data_blocks(fs, inode) != 0) ||
	    (inode->i_flags & EXT2_INDEX_FL))
		return 0;

	/*
	 * We should be able to do the test below all the time, but
	 * because the kernel doesn't forcibly clear the device
	 * inode's additional i_block fields, there are some rare
	 * occasions when a legitimate device inode will have non-zero
	 * additional i_block fields.  So for now, we only complain
	 * when the immutable flag is set, which should never happen
	 * for devices.  (And that's when the problem is caused, since
	 * you can't set or clear immutable flags for devices.)  Once
	 * the kernel has been fixed we can change this...
	 */
	if (inode->i_flags & (EXT2_IMMUTABLE_FL | EXT2_APPEND_FL)) {
		for (i=4; i < EXT2_N_BLOCKS; i++)
			if (inode->i_block[i])
				return 0;
	}
	return 1;
}

/*
 * Check to make sure a symlink inode is real.  Returns 1 if the symlink
 * checks out, 0 if not.
 */
static int
e2fsck_pass1_check_symlink(ext2_filsys fs, struct ext2_inode *inode, char *buf)
{
	unsigned int len;
	int i;
	blk_t   blocks;

	if ((inode->i_size_high || inode->i_size == 0) ||
	    (inode->i_flags & EXT2_INDEX_FL))
		return 0;

	blocks = ext2fs_inode_data_blocks(fs, inode);
	if (blocks) {
		if ((inode->i_size >= fs->blocksize) ||
		    (blocks != fs->blocksize >> 9) ||
		    (inode->i_block[0] < fs->super->s_first_data_block) ||
		    (inode->i_block[0] >= fs->super->s_blocks_count))
			return 0;

		for (i = 1; i < EXT2_N_BLOCKS; i++)
			if (inode->i_block[i])
				return 0;

		if (io_channel_read_blk(fs->io, inode->i_block[0], 1, buf))
			return 0;

		len = strnlen(buf, fs->blocksize);
		if (len == fs->blocksize)
			return 0;
	} else {
		if (inode->i_size >= sizeof(inode->i_block))
			return 0;

		len = strnlen((char *)inode->i_block, sizeof(inode->i_block));
		if (len == sizeof(inode->i_block))
			return 0;
	}
	if (len != inode->i_size)
		return 0;
	return 1;
}

/*
 * If the immutable (or append-only) flag is set on the inode, offer
 * to clear it.
 */
#define BAD_SPECIAL_FLAGS (EXT2_IMMUTABLE_FL | EXT2_APPEND_FL)
static void check_immutable(e2fsck_t ctx, struct problem_context *pctx)
{
	if (!(pctx->inode->i_flags & BAD_SPECIAL_FLAGS))
		return;

	if (!fix_problem(ctx, PR_1_SET_IMMUTABLE, pctx))
		return;

	pctx->inode->i_flags &= ~BAD_SPECIAL_FLAGS;
	e2fsck_write_inode(ctx, pctx->ino, pctx->inode, "pass1");
}

/*
 * If device, fifo or socket, check size is zero -- if not offer to
 * clear it
 */
static void check_size(e2fsck_t ctx, struct problem_context *pctx)
{
	struct ext2_inode *inode = pctx->inode;

	if ((inode->i_size == 0) && (inode->i_size_high == 0))
		return;

	if (!fix_problem(ctx, PR_1_SET_NONZSIZE, pctx))
		return;

	inode->i_size = 0;
	inode->i_size_high = 0;
	e2fsck_write_inode(ctx, pctx->ino, pctx->inode, "pass1");
}

static void check_ea_in_inode(e2fsck_t ctx, struct problem_context *pctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	struct ext2_inode_large *inode;
	struct ext2_ext_attr_entry *entry;
	char *start, *end;
	int storage_size, remain, offs;
	int problem = 0;

	inode = (struct ext2_inode_large *) pctx->inode;
	storage_size = EXT2_INODE_SIZE(ctx->fs->super) - EXT2_GOOD_OLD_INODE_SIZE -
		inode->i_extra_isize;
	start = ((char *) inode) + EXT2_GOOD_OLD_INODE_SIZE +
		inode->i_extra_isize + sizeof(__u32);
	end = (char *) inode + EXT2_INODE_SIZE(ctx->fs->super);
	entry = (struct ext2_ext_attr_entry *) start;

	/* scan all entry's headers first */

	/* take finish entry 0UL into account */
	remain = storage_size - sizeof(__u32);
	offs = end - start;

	while (!EXT2_EXT_IS_LAST_ENTRY(entry)) {

		/* header eats this space */
		remain -= sizeof(struct ext2_ext_attr_entry);

		/* is attribute name valid? */
		if (EXT2_EXT_ATTR_SIZE(entry->e_name_len) > remain) {
			pctx->num = entry->e_name_len;
			problem = PR_1_ATTR_NAME_LEN;
			goto fix;
		}

		/* attribute len eats this space */
		remain -= EXT2_EXT_ATTR_SIZE(entry->e_name_len);

		/* check value size */
		if (entry->e_value_size == 0 || entry->e_value_size > remain) {
			pctx->num = entry->e_value_size;
			problem = PR_1_ATTR_VALUE_SIZE;
			goto fix;
		}

		/* check value placement */
		if (entry->e_value_offs +
		    EXT2_XATTR_SIZE(entry->e_value_size) != offs) {
			printf("(entry->e_value_offs + entry->e_value_size: %d, offs: %d)\n", entry->e_value_offs + entry->e_value_size, offs);
			pctx->num = entry->e_value_offs;
			problem = PR_1_ATTR_VALUE_OFFSET;
			goto fix;
		}

		/* e_value_block must be 0 in inode's ea */
		if (entry->e_value_block != 0) {
			pctx->num = entry->e_value_block;
			problem = PR_1_ATTR_VALUE_BLOCK;
			goto fix;
		}

		/* e_hash must be 0 in inode's ea */
		if (entry->e_hash != 0) {
			pctx->num = entry->e_hash;
			problem = PR_1_ATTR_HASH;
			goto fix;
		}

		remain -= entry->e_value_size;
		offs -= EXT2_XATTR_SIZE(entry->e_value_size);

		entry = EXT2_EXT_ATTR_NEXT(entry);
	}
fix:
	/*
	 * it seems like a corruption. it's very unlikely we could repair
	 * EA(s) in automatic fashion -bzzz
	 */
	if (problem == 0 || !fix_problem(ctx, problem, pctx))
		return;

	/* simple remove all possible EA(s) */
	*((__u32 *)start) = 0UL;
	e2fsck_write_inode_full(ctx, pctx->ino, (struct ext2_inode *)inode,
				EXT2_INODE_SIZE(sb), "pass1");
}

static void check_inode_extra_space(e2fsck_t ctx, struct problem_context *pctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	struct ext2_inode_large *inode;
	__u32 *eamagic;
	int min, max;

	inode = (struct ext2_inode_large *) pctx->inode;
	if (EXT2_INODE_SIZE(sb) == EXT2_GOOD_OLD_INODE_SIZE) {
		/* this isn't large inode. so, nothing to check */
		return;
	}

	/* i_extra_isize must cover i_extra_isize + i_pad1 at least */
	min = sizeof(inode->i_extra_isize) + sizeof(inode->i_pad1);
	max = EXT2_INODE_SIZE(sb) - EXT2_GOOD_OLD_INODE_SIZE;
	/*
	 * For now we will allow i_extra_isize to be 0, but really
	 * implementations should never allow i_extra_isize to be 0
	 */
	if (inode->i_extra_isize &&
	    (inode->i_extra_isize < min || inode->i_extra_isize > max)) {
		if (!fix_problem(ctx, PR_1_EXTRA_ISIZE, pctx))
			return;
		inode->i_extra_isize = min;
		e2fsck_write_inode_full(ctx, pctx->ino, pctx->inode,
					EXT2_INODE_SIZE(sb), "pass1");
		return;
	}

	eamagic = (__u32 *) (((char *) inode) + EXT2_GOOD_OLD_INODE_SIZE +
			inode->i_extra_isize);
	if (*eamagic == EXT2_EXT_ATTR_MAGIC) {
		/* it seems inode has an extended attribute(s) in body */
		check_ea_in_inode(ctx, pctx);
	}
}

static void e2fsck_pass1(e2fsck_t ctx)
{
	int     i;
	__u64   max_sizes;
	ext2_filsys fs = ctx->fs;
	ext2_ino_t      ino;
	struct ext2_inode *inode;
	ext2_inode_scan scan;
	char            *block_buf;
	unsigned char   frag, fsize;
	struct          problem_context pctx;
	struct          scan_callback_struct scan_struct;
	struct ext2_super_block *sb = ctx->fs->super;
	int             imagic_fs;
	int             busted_fs_time = 0;
	int             inode_size;

	clear_problem_context(&pctx);

	if (!(ctx->options & E2F_OPT_PREEN))
		fix_problem(ctx, PR_1_PASS_HEADER, &pctx);

	if ((fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_DIR_INDEX) &&
	    !(ctx->options & E2F_OPT_NO)) {
		if (ext2fs_u32_list_create(&ctx->dirs_to_hash, 50))
			ctx->dirs_to_hash = 0;
	}

	/* Pass 1 */

#define EXT2_BPP(bits) (1ULL << ((bits) - 2))

	for (i = EXT2_MIN_BLOCK_LOG_SIZE; i <= EXT2_MAX_BLOCK_LOG_SIZE; i++) {
		max_sizes = EXT2_NDIR_BLOCKS + EXT2_BPP(i);
		max_sizes = max_sizes + EXT2_BPP(i) * EXT2_BPP(i);
		max_sizes = max_sizes + EXT2_BPP(i) * EXT2_BPP(i) * EXT2_BPP(i);
		max_sizes = (max_sizes * (1UL << i)) - 1;
		ext2_max_sizes[i - EXT2_MIN_BLOCK_LOG_SIZE] = max_sizes;
	}
#undef EXT2_BPP

	imagic_fs = (sb->s_feature_compat & EXT2_FEATURE_COMPAT_IMAGIC_INODES);

	/*
	 * Allocate bitmaps structures
	 */
	pctx.errcode = ext2fs_allocate_inode_bitmap(fs, _("in-use inode map"),
					      &ctx->inode_used_map);
	if (pctx.errcode) {
		pctx.num = 1;
		fix_problem(ctx, PR_1_ALLOCATE_IBITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	pctx.errcode = ext2fs_allocate_inode_bitmap(fs,
				_("directory inode map"), &ctx->inode_dir_map);
	if (pctx.errcode) {
		pctx.num = 2;
		fix_problem(ctx, PR_1_ALLOCATE_IBITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	pctx.errcode = ext2fs_allocate_inode_bitmap(fs,
			_("regular file inode map"), &ctx->inode_reg_map);
	if (pctx.errcode) {
		pctx.num = 6;
		fix_problem(ctx, PR_1_ALLOCATE_IBITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	pctx.errcode = ext2fs_allocate_block_bitmap(fs, _("in-use block map"),
					      &ctx->block_found_map);
	if (pctx.errcode) {
		pctx.num = 1;
		fix_problem(ctx, PR_1_ALLOCATE_BBITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	pctx.errcode = ext2fs_create_icount2(fs, 0, 0, 0,
					     &ctx->inode_link_info);
	if (pctx.errcode) {
		fix_problem(ctx, PR_1_ALLOCATE_ICOUNT, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	inode_size = EXT2_INODE_SIZE(fs->super);
	inode = (struct ext2_inode *)
		e2fsck_allocate_memory(ctx, inode_size, "scratch inode");

	inodes_to_process = (struct process_inode_block *)
		e2fsck_allocate_memory(ctx,
				       (ctx->process_inode_size *
					sizeof(struct process_inode_block)),
				       "array of inodes to process");
	process_inode_count = 0;

	pctx.errcode = ext2fs_init_dblist(fs, 0);
	if (pctx.errcode) {
		fix_problem(ctx, PR_1_ALLOCATE_DBCOUNT, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

	/*
	 * If the last orphan field is set, clear it, since the pass1
	 * processing will automatically find and clear the orphans.
	 * In the future, we may want to try using the last_orphan
	 * linked list ourselves, but for now, we clear it so that the
	 * ext3 mount code won't get confused.
	 */
	if (!(ctx->options & E2F_OPT_READONLY)) {
		if (fs->super->s_last_orphan) {
			fs->super->s_last_orphan = 0;
			ext2fs_mark_super_dirty(fs);
		}
	}

	mark_table_blocks(ctx);
	block_buf = (char *) e2fsck_allocate_memory(ctx, fs->blocksize * 3,
						    "block interate buffer");
	e2fsck_use_inode_shortcuts(ctx, 1);
	ehandler_operation(_("doing inode scan"));
	pctx.errcode = ext2fs_open_inode_scan(fs, ctx->inode_buffer_blocks,
					      &scan);
	if (pctx.errcode) {
		fix_problem(ctx, PR_1_ISCAN_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	ext2fs_inode_scan_flags(scan, EXT2_SF_SKIP_MISSING_ITABLE, 0);
	ctx->stashed_inode = inode;
	scan_struct.ctx = ctx;
	scan_struct.block_buf = block_buf;
	ext2fs_set_inode_callback(scan, scan_callback, &scan_struct);
	if (ctx->progress)
		if ((ctx->progress)(ctx, 1, 0, ctx->fs->group_desc_count))
			return;
	if ((fs->super->s_wtime < fs->super->s_inodes_count) ||
	    (fs->super->s_mtime < fs->super->s_inodes_count))
		busted_fs_time = 1;

	while (1) {
		pctx.errcode = ext2fs_get_next_inode_full(scan, &ino,
							  inode, inode_size);
		if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
			return;
		if (pctx.errcode == EXT2_ET_BAD_BLOCK_IN_INODE_TABLE) {
			continue;
		}
		if (pctx.errcode) {
			fix_problem(ctx, PR_1_ISCAN_ERROR, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		if (!ino)
			break;
		pctx.ino = ino;
		pctx.inode = inode;
		ctx->stashed_ino = ino;
		if (inode->i_links_count) {
			pctx.errcode = ext2fs_icount_store(ctx->inode_link_info,
					   ino, inode->i_links_count);
			if (pctx.errcode) {
				pctx.num = inode->i_links_count;
				fix_problem(ctx, PR_1_ICOUNT_STORE, &pctx);
				ctx->flags |= E2F_FLAG_ABORT;
				return;
			}
		}
		if (ino == EXT2_BAD_INO) {
			struct process_block_struct_1 pb;

			pctx.errcode = ext2fs_copy_bitmap(ctx->block_found_map,
							  &pb.fs_meta_blocks);
			if (pctx.errcode) {
				pctx.num = 4;
				fix_problem(ctx, PR_1_ALLOCATE_BBITMAP_ERROR, &pctx);
				ctx->flags |= E2F_FLAG_ABORT;
				return;
			}
			pb.ino = EXT2_BAD_INO;
			pb.num_blocks = pb.last_block = 0;
			pb.num_illegal_blocks = 0;
			pb.suppress = 0; pb.clear = 0; pb.is_dir = 0;
			pb.is_reg = 0; pb.fragmented = 0; pb.bbcheck = 0;
			pb.inode = inode;
			pb.pctx = &pctx;
			pb.ctx = ctx;
			pctx.errcode = ext2fs_block_iterate2(fs, ino, 0,
				     block_buf, process_bad_block, &pb);
			ext2fs_free_block_bitmap(pb.fs_meta_blocks);
			if (pctx.errcode) {
				fix_problem(ctx, PR_1_BLOCK_ITERATE, &pctx);
				ctx->flags |= E2F_FLAG_ABORT;
				return;
			}
			if (pb.bbcheck)
				if (!fix_problem(ctx, PR_1_BBINODE_BAD_METABLOCK_PROMPT, &pctx)) {
				ctx->flags |= E2F_FLAG_ABORT;
				return;
			}
			ext2fs_mark_inode_bitmap(ctx->inode_used_map, ino);
			clear_problem_context(&pctx);
			continue;
		} else if (ino == EXT2_ROOT_INO) {
			/*
			 * Make sure the root inode is a directory; if
			 * not, offer to clear it.  It will be
			 * regnerated in pass #3.
			 */
			if (!LINUX_S_ISDIR(inode->i_mode)) {
				if (fix_problem(ctx, PR_1_ROOT_NO_DIR, &pctx)) {
					inode->i_dtime = time(NULL);
					inode->i_links_count = 0;
					ext2fs_icount_store(ctx->inode_link_info,
							    ino, 0);
					e2fsck_write_inode(ctx, ino, inode,
							   "pass1");
				}
			}
			/*
			 * If dtime is set, offer to clear it.  mke2fs
			 * version 0.2b created filesystems with the
			 * dtime field set for the root and lost+found
			 * directories.  We won't worry about
			 * /lost+found, since that can be regenerated
			 * easily.  But we will fix the root directory
			 * as a special case.
			 */
			if (inode->i_dtime && inode->i_links_count) {
				if (fix_problem(ctx, PR_1_ROOT_DTIME, &pctx)) {
					inode->i_dtime = 0;
					e2fsck_write_inode(ctx, ino, inode,
							   "pass1");
				}
			}
		} else if (ino == EXT2_JOURNAL_INO) {
			ext2fs_mark_inode_bitmap(ctx->inode_used_map, ino);
			if (fs->super->s_journal_inum == EXT2_JOURNAL_INO) {
				if (!LINUX_S_ISREG(inode->i_mode) &&
				    fix_problem(ctx, PR_1_JOURNAL_BAD_MODE,
						&pctx)) {
					inode->i_mode = LINUX_S_IFREG;
					e2fsck_write_inode(ctx, ino, inode,
							   "pass1");
				}
				check_blocks(ctx, &pctx, block_buf);
				continue;
			}
			if ((inode->i_links_count || inode->i_blocks ||
			     inode->i_blocks || inode->i_block[0]) &&
			    fix_problem(ctx, PR_1_JOURNAL_INODE_NOT_CLEAR,
					&pctx)) {
				memset(inode, 0, inode_size);
				ext2fs_icount_store(ctx->inode_link_info,
						    ino, 0);
				e2fsck_write_inode_full(ctx, ino, inode,
							inode_size, "pass1");
			}
		} else if (ino < EXT2_FIRST_INODE(fs->super)) {
			int     problem = 0;

			ext2fs_mark_inode_bitmap(ctx->inode_used_map, ino);
			if (ino == EXT2_BOOT_LOADER_INO) {
				if (LINUX_S_ISDIR(inode->i_mode))
					problem = PR_1_RESERVED_BAD_MODE;
			} else if (ino == EXT2_RESIZE_INO) {
				if (inode->i_mode &&
				    !LINUX_S_ISREG(inode->i_mode))
					problem = PR_1_RESERVED_BAD_MODE;
			} else {
				if (inode->i_mode != 0)
					problem = PR_1_RESERVED_BAD_MODE;
			}
			if (problem) {
				if (fix_problem(ctx, problem, &pctx)) {
					inode->i_mode = 0;
					e2fsck_write_inode(ctx, ino, inode,
							   "pass1");
				}
			}
			check_blocks(ctx, &pctx, block_buf);
			continue;
		}
		/*
		 * Check for inodes who might have been part of the
		 * orphaned list linked list.  They should have gotten
		 * dealt with by now, unless the list had somehow been
		 * corrupted.
		 *
		 * FIXME: In the future, inodes which are still in use
		 * (and which are therefore) pending truncation should
		 * be handled specially.  Right now we just clear the
		 * dtime field, and the normal e2fsck handling of
		 * inodes where i_size and the inode blocks are
		 * inconsistent is to fix i_size, instead of releasing
		 * the extra blocks.  This won't catch the inodes that
		 * was at the end of the orphan list, but it's better
		 * than nothing.  The right answer is that there
		 * shouldn't be any bugs in the orphan list handling.  :-)
		 */
		if (inode->i_dtime && !busted_fs_time &&
		    inode->i_dtime < ctx->fs->super->s_inodes_count) {
			if (fix_problem(ctx, PR_1_LOW_DTIME, &pctx)) {
				inode->i_dtime = inode->i_links_count ?
					0 : time(NULL);
				e2fsck_write_inode(ctx, ino, inode,
						   "pass1");
			}
		}

		/*
		 * This code assumes that deleted inodes have
		 * i_links_count set to 0.
		 */
		if (!inode->i_links_count) {
			if (!inode->i_dtime && inode->i_mode) {
				if (fix_problem(ctx,
					    PR_1_ZERO_DTIME, &pctx)) {
					inode->i_dtime = time(NULL);
					e2fsck_write_inode(ctx, ino, inode,
							   "pass1");
				}
			}
			continue;
		}
		/*
		 * n.b.  0.3c ext2fs code didn't clear i_links_count for
		 * deleted files.  Oops.
		 *
		 * Since all new ext2 implementations get this right,
		 * we now assume that the case of non-zero
		 * i_links_count and non-zero dtime means that we
		 * should keep the file, not delete it.
		 *
		 */
		if (inode->i_dtime) {
			if (fix_problem(ctx, PR_1_SET_DTIME, &pctx)) {
				inode->i_dtime = 0;
				e2fsck_write_inode(ctx, ino, inode, "pass1");
			}
		}

		ext2fs_mark_inode_bitmap(ctx->inode_used_map, ino);
		switch (fs->super->s_creator_os) {
		    case EXT2_OS_LINUX:
			frag = inode->osd2.linux2.l_i_frag;
			fsize = inode->osd2.linux2.l_i_fsize;
			break;
		    case EXT2_OS_HURD:
			frag = inode->osd2.hurd2.h_i_frag;
			fsize = inode->osd2.hurd2.h_i_fsize;
			break;
		    case EXT2_OS_MASIX:
			frag = inode->osd2.masix2.m_i_frag;
			fsize = inode->osd2.masix2.m_i_fsize;
			break;
		    default:
			frag = fsize = 0;
		}

		if (inode->i_faddr || frag || fsize ||
		    (LINUX_S_ISDIR(inode->i_mode) && inode->i_dir_acl))
			mark_inode_bad(ctx, ino);
		if (inode->i_flags & EXT2_IMAGIC_FL) {
			if (imagic_fs) {
				if (!ctx->inode_imagic_map)
					alloc_imagic_map(ctx);
				ext2fs_mark_inode_bitmap(ctx->inode_imagic_map,
							 ino);
			} else {
				if (fix_problem(ctx, PR_1_SET_IMAGIC, &pctx)) {
					inode->i_flags &= ~EXT2_IMAGIC_FL;
					e2fsck_write_inode(ctx, ino,
							   inode, "pass1");
				}
			}
		}

		check_inode_extra_space(ctx, &pctx);

		if (LINUX_S_ISDIR(inode->i_mode)) {
			ext2fs_mark_inode_bitmap(ctx->inode_dir_map, ino);
			e2fsck_add_dir_info(ctx, ino, 0);
			ctx->fs_directory_count++;
		} else if (LINUX_S_ISREG (inode->i_mode)) {
			ext2fs_mark_inode_bitmap(ctx->inode_reg_map, ino);
			ctx->fs_regular_count++;
		} else if (LINUX_S_ISCHR (inode->i_mode) &&
			   e2fsck_pass1_check_device_inode(fs, inode)) {
			check_immutable(ctx, &pctx);
			check_size(ctx, &pctx);
			ctx->fs_chardev_count++;
		} else if (LINUX_S_ISBLK (inode->i_mode) &&
			   e2fsck_pass1_check_device_inode(fs, inode)) {
			check_immutable(ctx, &pctx);
			check_size(ctx, &pctx);
			ctx->fs_blockdev_count++;
		} else if (LINUX_S_ISLNK (inode->i_mode) &&
			   e2fsck_pass1_check_symlink(fs, inode, block_buf)) {
			check_immutable(ctx, &pctx);
			ctx->fs_symlinks_count++;
			if (ext2fs_inode_data_blocks(fs, inode) == 0) {
				ctx->fs_fast_symlinks_count++;
				check_blocks(ctx, &pctx, block_buf);
				continue;
			}
		}
		else if (LINUX_S_ISFIFO (inode->i_mode) &&
			 e2fsck_pass1_check_device_inode(fs, inode)) {
			check_immutable(ctx, &pctx);
			check_size(ctx, &pctx);
			ctx->fs_fifo_count++;
		} else if ((LINUX_S_ISSOCK (inode->i_mode)) &&
			   e2fsck_pass1_check_device_inode(fs, inode)) {
			check_immutable(ctx, &pctx);
			check_size(ctx, &pctx);
			ctx->fs_sockets_count++;
		} else
			mark_inode_bad(ctx, ino);
		if (inode->i_block[EXT2_IND_BLOCK])
			ctx->fs_ind_count++;
		if (inode->i_block[EXT2_DIND_BLOCK])
			ctx->fs_dind_count++;
		if (inode->i_block[EXT2_TIND_BLOCK])
			ctx->fs_tind_count++;
		if (inode->i_block[EXT2_IND_BLOCK] ||
		    inode->i_block[EXT2_DIND_BLOCK] ||
		    inode->i_block[EXT2_TIND_BLOCK] ||
		    inode->i_file_acl) {
			inodes_to_process[process_inode_count].ino = ino;
			inodes_to_process[process_inode_count].inode = *inode;
			process_inode_count++;
		} else
			check_blocks(ctx, &pctx, block_buf);

		if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
			return;

		if (process_inode_count >= ctx->process_inode_size) {
			process_inodes(ctx, block_buf);

			if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
				return;
		}
	}
	process_inodes(ctx, block_buf);
	ext2fs_close_inode_scan(scan);
	ehandler_operation(0);

	/*
	 * If any extended attribute blocks' reference counts need to
	 * be adjusted, either up (ctx->refcount_extra), or down
	 * (ctx->refcount), then fix them.
	 */
	if (ctx->refcount) {
		adjust_extattr_refcount(ctx, ctx->refcount, block_buf, -1);
		ea_refcount_free(ctx->refcount);
		ctx->refcount = 0;
	}
	if (ctx->refcount_extra) {
		adjust_extattr_refcount(ctx, ctx->refcount_extra,
					block_buf, +1);
		ea_refcount_free(ctx->refcount_extra);
		ctx->refcount_extra = 0;
	}

	if (ctx->invalid_bitmaps)
		handle_fs_bad_blocks(ctx);

	/* We don't need the block_ea_map any more */
	ext2fs_free_block_bitmap(ctx->block_ea_map);
	ctx->block_ea_map = 0;

	if (ctx->flags & E2F_FLAG_RESIZE_INODE) {
		ext2fs_block_bitmap save_bmap;

		save_bmap = fs->block_map;
		fs->block_map = ctx->block_found_map;
		clear_problem_context(&pctx);
		pctx.errcode = ext2fs_create_resize_inode(fs);
		if (pctx.errcode) {
			fix_problem(ctx, PR_1_RESIZE_INODE_CREATE, &pctx);
			/* Should never get here */
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		e2fsck_read_inode(ctx, EXT2_RESIZE_INO, inode,
				  "recreate inode");
		inode->i_mtime = time(NULL);
		e2fsck_write_inode(ctx, EXT2_RESIZE_INO, inode,
				  "recreate inode");
		fs->block_map = save_bmap;
		ctx->flags &= ~E2F_FLAG_RESIZE_INODE;
	}

	if (ctx->flags & E2F_FLAG_RESTART) {
		/*
		 * Only the master copy of the superblock and block
		 * group descriptors are going to be written during a
		 * restart, so set the superblock to be used to be the
		 * master superblock.
		 */
		ctx->use_superblock = 0;
		unwind_pass1();
		goto endit;
	}

	if (ctx->block_dup_map) {
		if (ctx->options & E2F_OPT_PREEN) {
			clear_problem_context(&pctx);
			fix_problem(ctx, PR_1_DUP_BLOCKS_PREENSTOP, &pctx);
		}
		e2fsck_pass1_dupblocks(ctx, block_buf);
	}
	ext2fs_free_mem(&inodes_to_process);
endit:
	e2fsck_use_inode_shortcuts(ctx, 0);

	ext2fs_free_mem(&block_buf);
	ext2fs_free_mem(&inode);
}

/*
 * When the inode_scan routines call this callback at the end of the
 * glock group, call process_inodes.
 */
static errcode_t scan_callback(ext2_filsys fs,
			       dgrp_t group, void * priv_data)
{
	struct scan_callback_struct *scan_struct;
	e2fsck_t ctx;

	scan_struct = (struct scan_callback_struct *) priv_data;
	ctx = scan_struct->ctx;

	process_inodes((e2fsck_t) fs->priv_data, scan_struct->block_buf);

	if (ctx->progress)
		if ((ctx->progress)(ctx, 1, group+1,
				    ctx->fs->group_desc_count))
			return EXT2_ET_CANCEL_REQUESTED;

	return 0;
}

/*
 * Process the inodes in the "inodes to process" list.
 */
static void process_inodes(e2fsck_t ctx, char *block_buf)
{
	int                     i;
	struct ext2_inode       *old_stashed_inode;
	ext2_ino_t              old_stashed_ino;
	const char              *old_operation;
	char                    buf[80];
	struct problem_context  pctx;

	/* begin process_inodes */
	if (process_inode_count == 0)
		return;
	old_operation = ehandler_operation(0);
	old_stashed_inode = ctx->stashed_inode;
	old_stashed_ino = ctx->stashed_ino;
	qsort(inodes_to_process, process_inode_count,
		      sizeof(struct process_inode_block), process_inode_cmp);
	clear_problem_context(&pctx);
	for (i=0; i < process_inode_count; i++) {
		pctx.inode = ctx->stashed_inode = &inodes_to_process[i].inode;
		pctx.ino = ctx->stashed_ino = inodes_to_process[i].ino;
		sprintf(buf, _("reading indirect blocks of inode %u"),
			pctx.ino);
		ehandler_operation(buf);
		check_blocks(ctx, &pctx, block_buf);
		if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
			break;
	}
	ctx->stashed_inode = old_stashed_inode;
	ctx->stashed_ino = old_stashed_ino;
	process_inode_count = 0;
	/* end process inodes */

	ehandler_operation(old_operation);
}

static int process_inode_cmp(const void *a, const void *b)
{
	const struct process_inode_block *ib_a =
		(const struct process_inode_block *) a;
	const struct process_inode_block *ib_b =
		(const struct process_inode_block *) b;
	int     ret;

	ret = (ib_a->inode.i_block[EXT2_IND_BLOCK] -
	       ib_b->inode.i_block[EXT2_IND_BLOCK]);
	if (ret == 0)
		ret = ib_a->inode.i_file_acl - ib_b->inode.i_file_acl;
	return ret;
}

/*
 * Mark an inode as being bad in some what
 */
static void mark_inode_bad(e2fsck_t ctx, ino_t ino)
{
	struct          problem_context pctx;

	if (!ctx->inode_bad_map) {
		clear_problem_context(&pctx);

		pctx.errcode = ext2fs_allocate_inode_bitmap(ctx->fs,
			    _("bad inode map"), &ctx->inode_bad_map);
		if (pctx.errcode) {
			pctx.num = 3;
			fix_problem(ctx, PR_1_ALLOCATE_IBITMAP_ERROR, &pctx);
			/* Should never get here */
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
	}
	ext2fs_mark_inode_bitmap(ctx->inode_bad_map, ino);
}


/*
 * This procedure will allocate the inode imagic table
 */
static void alloc_imagic_map(e2fsck_t ctx)
{
	struct          problem_context pctx;

	clear_problem_context(&pctx);
	pctx.errcode = ext2fs_allocate_inode_bitmap(ctx->fs,
					      _("imagic inode map"),
					      &ctx->inode_imagic_map);
	if (pctx.errcode) {
		pctx.num = 5;
		fix_problem(ctx, PR_1_ALLOCATE_IBITMAP_ERROR, &pctx);
		/* Should never get here */
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
}

/*
 * Marks a block as in use, setting the dup_map if it's been set
 * already.  Called by process_block and process_bad_block.
 *
 * WARNING: Assumes checks have already been done to make sure block
 * is valid.  This is true in both process_block and process_bad_block.
 */
static void mark_block_used(e2fsck_t ctx, blk_t block)
{
	struct          problem_context pctx;

	clear_problem_context(&pctx);

	if (ext2fs_fast_test_block_bitmap(ctx->block_found_map, block)) {
		if (!ctx->block_dup_map) {
			pctx.errcode = ext2fs_allocate_block_bitmap(ctx->fs,
			      _("multiply claimed block map"),
			      &ctx->block_dup_map);
			if (pctx.errcode) {
				pctx.num = 3;
				fix_problem(ctx, PR_1_ALLOCATE_BBITMAP_ERROR,
					    &pctx);
				/* Should never get here */
				ctx->flags |= E2F_FLAG_ABORT;
				return;
			}
		}
		ext2fs_fast_mark_block_bitmap(ctx->block_dup_map, block);
	} else {
		ext2fs_fast_mark_block_bitmap(ctx->block_found_map, block);
	}
}

/*
 * Adjust the extended attribute block's reference counts at the end
 * of pass 1, either by subtracting out references for EA blocks that
 * are still referenced in ctx->refcount, or by adding references for
 * EA blocks that had extra references as accounted for in
 * ctx->refcount_extra.
 */
static void adjust_extattr_refcount(e2fsck_t ctx, ext2_refcount_t refcount,
				    char *block_buf, int adjust_sign)
{
	struct ext2_ext_attr_header     *header;
	struct problem_context          pctx;
	ext2_filsys                     fs = ctx->fs;
	blk_t                           blk;
	__u32                           should_be;
	int                             count;

	clear_problem_context(&pctx);

	ea_refcount_intr_begin(refcount);
	while (1) {
		if ((blk = ea_refcount_intr_next(refcount, &count)) == 0)
			break;
		pctx.blk = blk;
		pctx.errcode = ext2fs_read_ext_attr(fs, blk, block_buf);
		if (pctx.errcode) {
			fix_problem(ctx, PR_1_EXTATTR_READ_ABORT, &pctx);
			return;
		}
		header = (struct ext2_ext_attr_header *) block_buf;
		pctx.blkcount = header->h_refcount;
		should_be = header->h_refcount + adjust_sign * count;
		pctx.num = should_be;
		if (fix_problem(ctx, PR_1_EXTATTR_REFCOUNT, &pctx)) {
			header->h_refcount = should_be;
			pctx.errcode = ext2fs_write_ext_attr(fs, blk,
							     block_buf);
			if (pctx.errcode) {
				fix_problem(ctx, PR_1_EXTATTR_WRITE, &pctx);
				continue;
			}
		}
	}
}

/*
 * Handle processing the extended attribute blocks
 */
static int check_ext_attr(e2fsck_t ctx, struct problem_context *pctx,
			   char *block_buf)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t      ino = pctx->ino;
	struct ext2_inode *inode = pctx->inode;
	blk_t           blk;
	char *          end;
	struct ext2_ext_attr_header *header;
	struct ext2_ext_attr_entry *entry;
	int             count;
	region_t        region;

	blk = inode->i_file_acl;
	if (blk == 0)
		return 0;

	/*
	 * If the Extended attribute flag isn't set, then a non-zero
	 * file acl means that the inode is corrupted.
	 *
	 * Or if the extended attribute block is an invalid block,
	 * then the inode is also corrupted.
	 */
	if (!(fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_EXT_ATTR) ||
	    (blk < fs->super->s_first_data_block) ||
	    (blk >= fs->super->s_blocks_count)) {
		mark_inode_bad(ctx, ino);
		return 0;
	}

	/* If ea bitmap hasn't been allocated, create it */
	if (!ctx->block_ea_map) {
		pctx->errcode = ext2fs_allocate_block_bitmap(fs,
						      _("ext attr block map"),
						      &ctx->block_ea_map);
		if (pctx->errcode) {
			pctx->num = 2;
			fix_problem(ctx, PR_1_ALLOCATE_BBITMAP_ERROR, pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return 0;
		}
	}

	/* Create the EA refcount structure if necessary */
	if (!ctx->refcount) {
		pctx->errcode = ea_refcount_create(0, &ctx->refcount);
		if (pctx->errcode) {
			pctx->num = 1;
			fix_problem(ctx, PR_1_ALLOCATE_REFCOUNT, pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return 0;
		}
	}

	/* Have we seen this EA block before? */
	if (ext2fs_fast_test_block_bitmap(ctx->block_ea_map, blk)) {
		if (ea_refcount_decrement(ctx->refcount, blk, 0) == 0)
			return 1;
		/* Ooops, this EA was referenced more than it stated */
		if (!ctx->refcount_extra) {
			pctx->errcode = ea_refcount_create(0,
					   &ctx->refcount_extra);
			if (pctx->errcode) {
				pctx->num = 2;
				fix_problem(ctx, PR_1_ALLOCATE_REFCOUNT, pctx);
				ctx->flags |= E2F_FLAG_ABORT;
				return 0;
			}
		}
		ea_refcount_increment(ctx->refcount_extra, blk, 0);
		return 1;
	}

	/*
	 * OK, we haven't seen this EA block yet.  So we need to
	 * validate it
	 */
	pctx->blk = blk;
	pctx->errcode = ext2fs_read_ext_attr(fs, blk, block_buf);
	if (pctx->errcode && fix_problem(ctx, PR_1_READ_EA_BLOCK, pctx))
		goto clear_extattr;
	header = (struct ext2_ext_attr_header *) block_buf;
	pctx->blk = inode->i_file_acl;
	if (((ctx->ext_attr_ver == 1) &&
	     (header->h_magic != EXT2_EXT_ATTR_MAGIC_v1)) ||
	    ((ctx->ext_attr_ver == 2) &&
	     (header->h_magic != EXT2_EXT_ATTR_MAGIC))) {
		if (fix_problem(ctx, PR_1_BAD_EA_BLOCK, pctx))
			goto clear_extattr;
	}

	if (header->h_blocks != 1) {
		if (fix_problem(ctx, PR_1_EA_MULTI_BLOCK, pctx))
			goto clear_extattr;
	}

	region = region_create(0, fs->blocksize);
	if (!region) {
		fix_problem(ctx, PR_1_EA_ALLOC_REGION, pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return 0;
	}
	if (region_allocate(region, 0, sizeof(struct ext2_ext_attr_header))) {
		if (fix_problem(ctx, PR_1_EA_ALLOC_COLLISION, pctx))
			goto clear_extattr;
	}

	entry = (struct ext2_ext_attr_entry *)(header+1);
	end = block_buf + fs->blocksize;
	while ((char *)entry < end && *(__u32 *)entry) {
		if (region_allocate(region, (char *)entry - (char *)header,
				   EXT2_EXT_ATTR_LEN(entry->e_name_len))) {
			if (fix_problem(ctx, PR_1_EA_ALLOC_COLLISION, pctx))
				goto clear_extattr;
		}
		if ((ctx->ext_attr_ver == 1 &&
		     (entry->e_name_len == 0 || entry->e_name_index != 0)) ||
		    (ctx->ext_attr_ver == 2 &&
		     entry->e_name_index == 0)) {
			if (fix_problem(ctx, PR_1_EA_BAD_NAME, pctx))
				goto clear_extattr;
		}
		if (entry->e_value_block != 0) {
			if (fix_problem(ctx, PR_1_EA_BAD_VALUE, pctx))
				goto clear_extattr;
		}
		if (entry->e_value_size &&
		    region_allocate(region, entry->e_value_offs,
				    EXT2_EXT_ATTR_SIZE(entry->e_value_size))) {
			if (fix_problem(ctx, PR_1_EA_ALLOC_COLLISION, pctx))
				goto clear_extattr;
		}
		entry = EXT2_EXT_ATTR_NEXT(entry);
	}
	if (region_allocate(region, (char *)entry - (char *)header, 4)) {
		if (fix_problem(ctx, PR_1_EA_ALLOC_COLLISION, pctx))
			goto clear_extattr;
	}
	region_free(region);

	count = header->h_refcount - 1;
	if (count)
		ea_refcount_store(ctx->refcount, blk, count);
	mark_block_used(ctx, blk);
	ext2fs_fast_mark_block_bitmap(ctx->block_ea_map, blk);

	return 1;

clear_extattr:
	inode->i_file_acl = 0;
	e2fsck_write_inode(ctx, ino, inode, "check_ext_attr");
	return 0;
}

/* Returns 1 if bad htree, 0 if OK */
static int handle_htree(e2fsck_t ctx, struct problem_context *pctx,
			ext2_ino_t ino FSCK_ATTR((unused)),
			struct ext2_inode *inode,
			char *block_buf)
{
	struct ext2_dx_root_info        *root;
	ext2_filsys                     fs = ctx->fs;
	errcode_t                       retval;
	blk_t                           blk;

	if ((!LINUX_S_ISDIR(inode->i_mode) &&
	     fix_problem(ctx, PR_1_HTREE_NODIR, pctx)) ||
	    (!(fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_DIR_INDEX) &&
	     fix_problem(ctx, PR_1_HTREE_SET, pctx)))
		return 1;

	blk = inode->i_block[0];
	if (((blk == 0) ||
	     (blk < fs->super->s_first_data_block) ||
	     (blk >= fs->super->s_blocks_count)) &&
	    fix_problem(ctx, PR_1_HTREE_BADROOT, pctx))
		return 1;

	retval = io_channel_read_blk(fs->io, blk, 1, block_buf);
	if (retval && fix_problem(ctx, PR_1_HTREE_BADROOT, pctx))
		return 1;

	/* XXX should check that beginning matches a directory */
	root = (struct ext2_dx_root_info *) (block_buf + 24);

	if ((root->reserved_zero || root->info_length < 8) &&
	    fix_problem(ctx, PR_1_HTREE_BADROOT, pctx))
		return 1;

	pctx->num = root->hash_version;
	if ((root->hash_version != EXT2_HASH_LEGACY) &&
	    (root->hash_version != EXT2_HASH_HALF_MD4) &&
	    (root->hash_version != EXT2_HASH_TEA) &&
	    fix_problem(ctx, PR_1_HTREE_HASHV, pctx))
		return 1;

	if ((root->unused_flags & EXT2_HASH_FLAG_INCOMPAT) &&
	    fix_problem(ctx, PR_1_HTREE_INCOMPAT, pctx))
		return 1;

	pctx->num = root->indirect_levels;
	if ((root->indirect_levels > 1) &&
	    fix_problem(ctx, PR_1_HTREE_DEPTH, pctx))
		return 1;

	return 0;
}

/*
 * This subroutine is called on each inode to account for all of the
 * blocks used by that inode.
 */
static void check_blocks(e2fsck_t ctx, struct problem_context *pctx,
			 char *block_buf)
{
	ext2_filsys fs = ctx->fs;
	struct process_block_struct_1 pb;
	ext2_ino_t      ino = pctx->ino;
	struct ext2_inode *inode = pctx->inode;
	int             bad_size = 0;
	int             dirty_inode = 0;
	__u64           size;

	pb.ino = ino;
	pb.num_blocks = 0;
	pb.last_block = -1;
	pb.num_illegal_blocks = 0;
	pb.suppress = 0; pb.clear = 0;
	pb.fragmented = 0;
	pb.compressed = 0;
	pb.previous_block = 0;
	pb.is_dir = LINUX_S_ISDIR(inode->i_mode);
	pb.is_reg = LINUX_S_ISREG(inode->i_mode);
	pb.max_blocks = 1 << (31 - fs->super->s_log_block_size);
	pb.inode = inode;
	pb.pctx = pctx;
	pb.ctx = ctx;
	pctx->ino = ino;
	pctx->errcode = 0;

	if (inode->i_flags & EXT2_COMPRBLK_FL) {
		if (fs->super->s_feature_incompat &
		    EXT2_FEATURE_INCOMPAT_COMPRESSION)
			pb.compressed = 1;
		else {
			if (fix_problem(ctx, PR_1_COMPR_SET, pctx)) {
				inode->i_flags &= ~EXT2_COMPRBLK_FL;
				dirty_inode++;
			}
		}
	}

	if (inode->i_file_acl && check_ext_attr(ctx, pctx, block_buf))
		pb.num_blocks++;

	if (ext2fs_inode_has_valid_blocks(inode))
		pctx->errcode = ext2fs_block_iterate2(fs, ino,
				       pb.is_dir ? BLOCK_FLAG_HOLE : 0,
				       block_buf, process_block, &pb);
	end_problem_latch(ctx, PR_LATCH_BLOCK);
	end_problem_latch(ctx, PR_LATCH_TOOBIG);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		goto out;
	if (pctx->errcode)
		fix_problem(ctx, PR_1_BLOCK_ITERATE, pctx);

	if (pb.fragmented && pb.num_blocks < fs->super->s_blocks_per_group)
		ctx->fs_fragmented++;

	if (pb.clear) {
		inode->i_links_count = 0;
		ext2fs_icount_store(ctx->inode_link_info, ino, 0);
		inode->i_dtime = time(NULL);
		dirty_inode++;
		ext2fs_unmark_inode_bitmap(ctx->inode_dir_map, ino);
		ext2fs_unmark_inode_bitmap(ctx->inode_reg_map, ino);
		ext2fs_unmark_inode_bitmap(ctx->inode_used_map, ino);
		/*
		 * The inode was probably partially accounted for
		 * before processing was aborted, so we need to
		 * restart the pass 1 scan.
		 */
		ctx->flags |= E2F_FLAG_RESTART;
		goto out;
	}

	if (inode->i_flags & EXT2_INDEX_FL) {
		if (handle_htree(ctx, pctx, ino, inode, block_buf)) {
			inode->i_flags &= ~EXT2_INDEX_FL;
			dirty_inode++;
		} else {
#ifdef ENABLE_HTREE
			e2fsck_add_dx_dir(ctx, ino, pb.last_block+1);
#endif
		}
	}
	if (ctx->dirs_to_hash && pb.is_dir &&
	    !(inode->i_flags & EXT2_INDEX_FL) &&
	    ((inode->i_size / fs->blocksize) >= 3))
		ext2fs_u32_list_add(ctx->dirs_to_hash, ino);

	if (!pb.num_blocks && pb.is_dir) {
		if (fix_problem(ctx, PR_1_ZERO_LENGTH_DIR, pctx)) {
			inode->i_links_count = 0;
			ext2fs_icount_store(ctx->inode_link_info, ino, 0);
			inode->i_dtime = time(NULL);
			dirty_inode++;
			ext2fs_unmark_inode_bitmap(ctx->inode_dir_map, ino);
			ext2fs_unmark_inode_bitmap(ctx->inode_reg_map, ino);
			ext2fs_unmark_inode_bitmap(ctx->inode_used_map, ino);
			ctx->fs_directory_count--;
			goto out;
		}
	}

	pb.num_blocks *= (fs->blocksize / 512);

	if (pb.is_dir) {
		int nblock = inode->i_size >> EXT2_BLOCK_SIZE_BITS(fs->super);
		if (nblock > (pb.last_block + 1))
			bad_size = 1;
		else if (nblock < (pb.last_block + 1)) {
			if (((pb.last_block + 1) - nblock) >
			    fs->super->s_prealloc_dir_blocks)
				bad_size = 2;
		}
	} else {
		size = EXT2_I_SIZE(inode);
		if ((pb.last_block >= 0) &&
		    (size < (__u64) pb.last_block * fs->blocksize))
			bad_size = 3;
		else if (size > ext2_max_sizes[fs->super->s_log_block_size])
			bad_size = 4;
	}
	/* i_size for symlinks is checked elsewhere */
	if (bad_size && !LINUX_S_ISLNK(inode->i_mode)) {
		pctx->num = (pb.last_block+1) * fs->blocksize;
		if (fix_problem(ctx, PR_1_BAD_I_SIZE, pctx)) {
			inode->i_size = pctx->num;
			if (!LINUX_S_ISDIR(inode->i_mode))
				inode->i_size_high = pctx->num >> 32;
			dirty_inode++;
		}
		pctx->num = 0;
	}
	if (LINUX_S_ISREG(inode->i_mode) &&
	    (inode->i_size_high || inode->i_size & 0x80000000UL))
		ctx->large_files++;
	if (pb.num_blocks != inode->i_blocks) {
		pctx->num = pb.num_blocks;
		if (fix_problem(ctx, PR_1_BAD_I_BLOCKS, pctx)) {
			inode->i_blocks = pb.num_blocks;
			dirty_inode++;
		}
		pctx->num = 0;
	}
out:
	if (dirty_inode)
		e2fsck_write_inode(ctx, ino, inode, "check_blocks");
}


/*
 * This is a helper function for check_blocks().
 */
static int process_block(ext2_filsys fs,
		  blk_t *block_nr,
		  e2_blkcnt_t blockcnt,
		  blk_t ref_block FSCK_ATTR((unused)),
		  int ref_offset FSCK_ATTR((unused)),
		  void *priv_data)
{
	struct process_block_struct_1 *p;
	struct problem_context *pctx;
	blk_t   blk = *block_nr;
	int     ret_code = 0;
	int     problem = 0;
	e2fsck_t        ctx;

	p = (struct process_block_struct_1 *) priv_data;
	pctx = p->pctx;
	ctx = p->ctx;

	if (p->compressed && (blk == EXT2FS_COMPRESSED_BLKADDR)) {
		/* todo: Check that the comprblk_fl is high, that the
		   blkaddr pattern looks right (all non-holes up to
		   first EXT2FS_COMPRESSED_BLKADDR, then all
		   EXT2FS_COMPRESSED_BLKADDR up to end of cluster),
		   that the feature_incompat bit is high, and that the
		   inode is a regular file.  If we're doing a "full
		   check" (a concept introduced to e2fsck by e2compr,
		   meaning that we look at data blocks as well as
		   metadata) then call some library routine that
		   checks the compressed data.  I'll have to think
		   about this, because one particularly important
		   problem to be able to fix is to recalculate the
		   cluster size if necessary.  I think that perhaps
		   we'd better do most/all e2compr-specific checks
		   separately, after the non-e2compr checks.  If not
		   doing a full check, it may be useful to test that
		   the personality is linux; e.g. if it isn't then
		   perhaps this really is just an illegal block. */
		return 0;
	}

	if (blk == 0) {
		if (p->is_dir == 0) {
			/*
			 * Should never happen, since only directories
			 * get called with BLOCK_FLAG_HOLE
			 */
#ifdef DEBUG_E2FSCK
			printf("process_block() called with blk == 0, "
			       "blockcnt=%d, inode %lu???\n",
			       blockcnt, p->ino);
#endif
			return 0;
		}
		if (blockcnt < 0)
			return 0;
		if (blockcnt * fs->blocksize < p->inode->i_size) {
			goto mark_dir;
		}
		return 0;
	}

	/*
	 * Simplistic fragmentation check.  We merely require that the
	 * file be contiguous.  (Which can never be true for really
	 * big files that are greater than a block group.)
	 */
	if (!HOLE_BLKADDR(p->previous_block)) {
		if (p->previous_block+1 != blk)
			p->fragmented = 1;
	}
	p->previous_block = blk;

	if (p->is_dir && blockcnt > (1 << (21 - fs->super->s_log_block_size)))
		problem = PR_1_TOOBIG_DIR;
	if (p->is_reg && p->num_blocks+1 >= p->max_blocks)
		problem = PR_1_TOOBIG_REG;
	if (!p->is_dir && !p->is_reg && blockcnt > 0)
		problem = PR_1_TOOBIG_SYMLINK;

	if (blk < fs->super->s_first_data_block ||
	    blk >= fs->super->s_blocks_count)
		problem = PR_1_ILLEGAL_BLOCK_NUM;

	if (problem) {
		p->num_illegal_blocks++;
		if (!p->suppress && (p->num_illegal_blocks % 12) == 0) {
			if (fix_problem(ctx, PR_1_TOO_MANY_BAD_BLOCKS, pctx)) {
				p->clear = 1;
				return BLOCK_ABORT;
			}
			if (fix_problem(ctx, PR_1_SUPPRESS_MESSAGES, pctx)) {
				p->suppress = 1;
				set_latch_flags(PR_LATCH_BLOCK,
						PRL_SUPPRESS, 0);
			}
		}
		pctx->blk = blk;
		pctx->blkcount = blockcnt;
		if (fix_problem(ctx, problem, pctx)) {
			blk = *block_nr = 0;
			ret_code = BLOCK_CHANGED;
			goto mark_dir;
		} else
			return 0;
	}

	if (p->ino == EXT2_RESIZE_INO) {
		/*
		 * The resize inode has already be sanity checked
		 * during pass #0 (the superblock checks).  All we
		 * have to do is mark the double indirect block as
		 * being in use; all of the other blocks are handled
		 * by mark_table_blocks()).
		 */
		if (blockcnt == BLOCK_COUNT_DIND)
			mark_block_used(ctx, blk);
	} else
		mark_block_used(ctx, blk);
	p->num_blocks++;
	if (blockcnt >= 0)
		p->last_block = blockcnt;
mark_dir:
	if (p->is_dir && (blockcnt >= 0)) {
		pctx->errcode = ext2fs_add_dir_block(fs->dblist, p->ino,
						    blk, blockcnt);
		if (pctx->errcode) {
			pctx->blk = blk;
			pctx->num = blockcnt;
			fix_problem(ctx, PR_1_ADD_DBLOCK, pctx);
			/* Should never get here */
			ctx->flags |= E2F_FLAG_ABORT;
			return BLOCK_ABORT;
		}
	}
	return ret_code;
}

static int process_bad_block(ext2_filsys fs FSCK_ATTR((unused)),
		      blk_t *block_nr,
		      e2_blkcnt_t blockcnt,
		      blk_t ref_block FSCK_ATTR((unused)),
		      int ref_offset FSCK_ATTR((unused)),
		      void *priv_data EXT2FS_ATTR((unused)))
{
	/*
	 * Note: This function processes blocks for the bad blocks
	 * inode, which is never compressed.  So we don't use HOLE_BLKADDR().
	 */

	printf("Unrecoverable Error: Found %"PRIi64" bad blocks starting at block number: %u\n", blockcnt, *block_nr);
	return BLOCK_ERROR;
}

/*
 * This routine gets called at the end of pass 1 if bad blocks are
 * detected in the superblock, group descriptors, inode_bitmaps, or
 * block bitmaps.  At this point, all of the blocks have been mapped
 * out, so we can try to allocate new block(s) to replace the bad
 * blocks.
 */
static void handle_fs_bad_blocks(e2fsck_t ctx)
{
	printf("Bad blocks detected on your filesystem\n"
		"You should get your data off as the device will soon die\n");
}

/*
 * This routine marks all blocks which are used by the superblock,
 * group descriptors, inode bitmaps, and block bitmaps.
 */
static void mark_table_blocks(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	blk_t   block, b;
	dgrp_t  i;
	int     j;
	struct problem_context pctx;

	clear_problem_context(&pctx);

	block = fs->super->s_first_data_block;
	for (i = 0; i < fs->group_desc_count; i++) {
		pctx.group = i;

		ext2fs_reserve_super_and_bgd(fs, i, ctx->block_found_map);

		/*
		 * Mark the blocks used for the inode table
		 */
		if (fs->group_desc[i].bg_inode_table) {
			for (j = 0, b = fs->group_desc[i].bg_inode_table;
			     j < fs->inode_blocks_per_group;
			     j++, b++) {
				if (ext2fs_test_block_bitmap(ctx->block_found_map,
							     b)) {
					pctx.blk = b;
					if (fix_problem(ctx,
						PR_1_ITABLE_CONFLICT, &pctx)) {
						ctx->invalid_inode_table_flag[i]++;
						ctx->invalid_bitmaps++;
					}
				} else {
					ext2fs_mark_block_bitmap(ctx->block_found_map, b);
				}
			}
		}

		/*
		 * Mark block used for the block bitmap
		 */
		if (fs->group_desc[i].bg_block_bitmap) {
			if (ext2fs_test_block_bitmap(ctx->block_found_map,
				     fs->group_desc[i].bg_block_bitmap)) {
				pctx.blk = fs->group_desc[i].bg_block_bitmap;
				if (fix_problem(ctx, PR_1_BB_CONFLICT, &pctx)) {
					ctx->invalid_block_bitmap_flag[i]++;
					ctx->invalid_bitmaps++;
				}
			} else {
				ext2fs_mark_block_bitmap(ctx->block_found_map,
					fs->group_desc[i].bg_block_bitmap);
			}
		}
		/*
		 * Mark block used for the inode bitmap
		 */
		if (fs->group_desc[i].bg_inode_bitmap) {
			if (ext2fs_test_block_bitmap(ctx->block_found_map,
				     fs->group_desc[i].bg_inode_bitmap)) {
				pctx.blk = fs->group_desc[i].bg_inode_bitmap;
				if (fix_problem(ctx, PR_1_IB_CONFLICT, &pctx)) {
					ctx->invalid_inode_bitmap_flag[i]++;
					ctx->invalid_bitmaps++;
				}
			} else {
				ext2fs_mark_block_bitmap(ctx->block_found_map,
					fs->group_desc[i].bg_inode_bitmap);
			}
		}
		block += fs->super->s_blocks_per_group;
	}
}

/*
 * Thes subroutines short circuits ext2fs_get_blocks and
 * ext2fs_check_directory; we use them since we already have the inode
 * structure, so there's no point in letting the ext2fs library read
 * the inode again.
 */
static errcode_t pass1_get_blocks(ext2_filsys fs, ext2_ino_t ino,
				  blk_t *blocks)
{
	e2fsck_t ctx = (e2fsck_t) fs->priv_data;
	int     i;

	if ((ino != ctx->stashed_ino) || !ctx->stashed_inode)
		return EXT2_ET_CALLBACK_NOTHANDLED;

	for (i=0; i < EXT2_N_BLOCKS; i++)
		blocks[i] = ctx->stashed_inode->i_block[i];
	return 0;
}

static errcode_t pass1_read_inode(ext2_filsys fs, ext2_ino_t ino,
				  struct ext2_inode *inode)
{
	e2fsck_t ctx = (e2fsck_t) fs->priv_data;

	if ((ino != ctx->stashed_ino) || !ctx->stashed_inode)
		return EXT2_ET_CALLBACK_NOTHANDLED;
	*inode = *ctx->stashed_inode;
	return 0;
}

static errcode_t pass1_write_inode(ext2_filsys fs, ext2_ino_t ino,
			    struct ext2_inode *inode)
{
	e2fsck_t ctx = (e2fsck_t) fs->priv_data;

	if ((ino == ctx->stashed_ino) && ctx->stashed_inode)
		*ctx->stashed_inode = *inode;
	return EXT2_ET_CALLBACK_NOTHANDLED;
}

static errcode_t pass1_check_directory(ext2_filsys fs, ext2_ino_t ino)
{
	e2fsck_t ctx = (e2fsck_t) fs->priv_data;

	if ((ino != ctx->stashed_ino) || !ctx->stashed_inode)
		return EXT2_ET_CALLBACK_NOTHANDLED;

	if (!LINUX_S_ISDIR(ctx->stashed_inode->i_mode))
		return EXT2_ET_NO_DIRECTORY;
	return 0;
}

void e2fsck_use_inode_shortcuts(e2fsck_t ctx, int bool)
{
	ext2_filsys fs = ctx->fs;

	if (bool) {
		fs->get_blocks = pass1_get_blocks;
		fs->check_directory = pass1_check_directory;
		fs->read_inode = pass1_read_inode;
		fs->write_inode = pass1_write_inode;
		ctx->stashed_ino = 0;
	} else {
		fs->get_blocks = 0;
		fs->check_directory = 0;
		fs->read_inode = 0;
		fs->write_inode = 0;
	}
}

/*
 * pass1b.c --- Pass #1b of e2fsck
 *
 * This file contains pass1B, pass1C, and pass1D of e2fsck.  They are
 * only invoked if pass 1 discovered blocks which are in use by more
 * than one inode.
 *
 * Pass1B scans the data blocks of all the inodes again, generating a
 * complete list of duplicate blocks and which inodes have claimed
 * them.
 *
 * Pass1C does a tree-traversal of the filesystem, to determine the
 * parent directories of these inodes.  This step is necessary so that
 * e2fsck can print out the pathnames of affected inodes.
 *
 * Pass1D is a reconciliation pass.  For each inode with duplicate
 * blocks, the user is prompted if s/he would like to clone the file
 * (so that the file gets a fresh copy of the duplicated blocks) or
 * simply to delete the file.
 *
 */


/* Needed for architectures where sizeof(int) != sizeof(void *) */
#define INT_TO_VOIDPTR(val)  ((void *)(intptr_t)(val))
#define VOIDPTR_TO_INT(ptr)  ((int)(intptr_t)(ptr))

/* Define an extension to the ext2 library's block count information */
#define BLOCK_COUNT_EXTATTR     (-5)

struct block_el {
	blk_t   block;
	struct block_el *next;
};

struct inode_el {
	ext2_ino_t      inode;
	struct inode_el *next;
};

struct dup_block {
	int             num_bad;
	struct inode_el *inode_list;
};

/*
 * This structure stores information about a particular inode which
 * is sharing blocks with other inodes.  This information is collected
 * to display to the user, so that the user knows what files he or she
 * is dealing with, when trying to decide how to resolve the conflict
 * of multiply-claimed blocks.
 */
struct dup_inode {
	ext2_ino_t              dir;
	int                     num_dupblocks;
	struct ext2_inode       inode;
	struct block_el         *block_list;
};

static int process_pass1b_block(ext2_filsys fs, blk_t   *blocknr,
				e2_blkcnt_t blockcnt, blk_t ref_blk,
				int ref_offset, void *priv_data);
static void delete_file(e2fsck_t ctx, ext2_ino_t ino,
			struct dup_inode *dp, char *block_buf);
static int clone_file(e2fsck_t ctx, ext2_ino_t ino,
		      struct dup_inode *dp, char* block_buf);
static int check_if_fs_block(e2fsck_t ctx, blk_t test_blk);

static void pass1b(e2fsck_t ctx, char *block_buf);
static void pass1c(e2fsck_t ctx, char *block_buf);
static void pass1d(e2fsck_t ctx, char *block_buf);

static int dup_inode_count = 0;

static dict_t blk_dict, ino_dict;

static ext2fs_inode_bitmap inode_dup_map;

static int dict_int_cmp(const void *a, const void *b)
{
	intptr_t        ia, ib;

	ia = (intptr_t)a;
	ib = (intptr_t)b;

	return (ia-ib);
}

/*
 * Add a duplicate block record
 */
static void add_dupe(e2fsck_t ctx, ext2_ino_t ino, blk_t blk,
		     struct ext2_inode *inode)
{
	dnode_t *n;
	struct dup_block        *db;
	struct dup_inode        *di;
	struct block_el         *blk_el;
	struct inode_el         *ino_el;

	n = dict_lookup(&blk_dict, INT_TO_VOIDPTR(blk));
	if (n)
		db = (struct dup_block *) dnode_get(n);
	else {
		db = (struct dup_block *) e2fsck_allocate_memory(ctx,
			 sizeof(struct dup_block), "duplicate block header");
		db->num_bad = 0;
		db->inode_list = 0;
		dict_alloc_insert(&blk_dict, INT_TO_VOIDPTR(blk), db);
	}
	ino_el = (struct inode_el *) e2fsck_allocate_memory(ctx,
			 sizeof(struct inode_el), "inode element");
	ino_el->inode = ino;
	ino_el->next = db->inode_list;
	db->inode_list = ino_el;
	db->num_bad++;

	n = dict_lookup(&ino_dict, INT_TO_VOIDPTR(ino));
	if (n)
		di = (struct dup_inode *) dnode_get(n);
	else {
		di = (struct dup_inode *) e2fsck_allocate_memory(ctx,
			 sizeof(struct dup_inode), "duplicate inode header");
		di->dir = (ino == EXT2_ROOT_INO) ? EXT2_ROOT_INO : 0;
		di->num_dupblocks = 0;
		di->block_list = 0;
		di->inode = *inode;
		dict_alloc_insert(&ino_dict, INT_TO_VOIDPTR(ino), di);
	}
	blk_el = (struct block_el *) e2fsck_allocate_memory(ctx,
			 sizeof(struct block_el), "block element");
	blk_el->block = blk;
	blk_el->next = di->block_list;
	di->block_list = blk_el;
	di->num_dupblocks++;
}

/*
 * Free a duplicate inode record
 */
static void inode_dnode_free(dnode_t *node)
{
	struct dup_inode        *di;
	struct block_el         *p, *next;

	di = (struct dup_inode *) dnode_get(node);
	for (p = di->block_list; p; p = next) {
		next = p->next;
		free(p);
	}
	free(node);
}

/*
 * Free a duplicate block record
 */
static void block_dnode_free(dnode_t *node)
{
	struct dup_block        *db;
	struct inode_el         *p, *next;

	db = (struct dup_block *) dnode_get(node);
	for (p = db->inode_list; p; p = next) {
		next = p->next;
		free(p);
	}
	free(node);
}


/*
 * Main procedure for handling duplicate blocks
 */
void e2fsck_pass1_dupblocks(e2fsck_t ctx, char *block_buf)
{
	ext2_filsys             fs = ctx->fs;
	struct problem_context  pctx;

	clear_problem_context(&pctx);

	pctx.errcode = ext2fs_allocate_inode_bitmap(fs,
		      _("multiply claimed inode map"), &inode_dup_map);
	if (pctx.errcode) {
		fix_problem(ctx, PR_1B_ALLOCATE_IBITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

	dict_init(&ino_dict, DICTCOUNT_T_MAX, dict_int_cmp);
	dict_init(&blk_dict, DICTCOUNT_T_MAX, dict_int_cmp);
	dict_set_allocator(&ino_dict, inode_dnode_free);
	dict_set_allocator(&blk_dict, block_dnode_free);

	pass1b(ctx, block_buf);
	pass1c(ctx, block_buf);
	pass1d(ctx, block_buf);

	/*
	 * Time to free all of the accumulated data structures that we
	 * don't need anymore.
	 */
	dict_free_nodes(&ino_dict);
	dict_free_nodes(&blk_dict);
}

/*
 * Scan the inodes looking for inodes that contain duplicate blocks.
 */
struct process_block_struct_1b {
	e2fsck_t        ctx;
	ext2_ino_t      ino;
	int             dup_blocks;
	struct ext2_inode *inode;
	struct problem_context *pctx;
};

static void pass1b(e2fsck_t ctx, char *block_buf)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t ino;
	struct ext2_inode inode;
	ext2_inode_scan scan;
	struct process_block_struct_1b pb;
	struct problem_context pctx;

	clear_problem_context(&pctx);

	if (!(ctx->options & E2F_OPT_PREEN))
		fix_problem(ctx, PR_1B_PASS_HEADER, &pctx);
	pctx.errcode = ext2fs_open_inode_scan(fs, ctx->inode_buffer_blocks,
					      &scan);
	if (pctx.errcode) {
		fix_problem(ctx, PR_1B_ISCAN_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	ctx->stashed_inode = &inode;
	pb.ctx = ctx;
	pb.pctx = &pctx;
	pctx.str = "pass1b";
	while (1) {
		pctx.errcode = ext2fs_get_next_inode(scan, &ino, &inode);
		if (pctx.errcode == EXT2_ET_BAD_BLOCK_IN_INODE_TABLE)
			continue;
		if (pctx.errcode) {
			fix_problem(ctx, PR_1B_ISCAN_ERROR, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		if (!ino)
			break;
		pctx.ino = ctx->stashed_ino = ino;
		if ((ino != EXT2_BAD_INO) &&
		    !ext2fs_test_inode_bitmap(ctx->inode_used_map, ino))
			continue;

		pb.ino = ino;
		pb.dup_blocks = 0;
		pb.inode = &inode;

		if (ext2fs_inode_has_valid_blocks(&inode) ||
		    (ino == EXT2_BAD_INO))
			pctx.errcode = ext2fs_block_iterate2(fs, ino,
				     0, block_buf, process_pass1b_block, &pb);
		if (inode.i_file_acl)
			process_pass1b_block(fs, &inode.i_file_acl,
					     BLOCK_COUNT_EXTATTR, 0, 0, &pb);
		if (pb.dup_blocks) {
			end_problem_latch(ctx, PR_LATCH_DBLOCK);
			if (ino >= EXT2_FIRST_INODE(fs->super) ||
			    ino == EXT2_ROOT_INO)
				dup_inode_count++;
		}
		if (pctx.errcode)
			fix_problem(ctx, PR_1B_BLOCK_ITERATE, &pctx);
	}
	ext2fs_close_inode_scan(scan);
	e2fsck_use_inode_shortcuts(ctx, 0);
}

static int process_pass1b_block(ext2_filsys fs FSCK_ATTR((unused)),
				blk_t   *block_nr,
				e2_blkcnt_t blockcnt FSCK_ATTR((unused)),
				blk_t ref_blk FSCK_ATTR((unused)),
				int ref_offset FSCK_ATTR((unused)),
				void *priv_data)
{
	struct process_block_struct_1b *p;
	e2fsck_t ctx;

	if (HOLE_BLKADDR(*block_nr))
		return 0;
	p = (struct process_block_struct_1b *) priv_data;
	ctx = p->ctx;

	if (!ext2fs_test_block_bitmap(ctx->block_dup_map, *block_nr))
		return 0;

	/* OK, this is a duplicate block */
	if (p->ino != EXT2_BAD_INO) {
		p->pctx->blk = *block_nr;
		fix_problem(ctx, PR_1B_DUP_BLOCK, p->pctx);
	}
	p->dup_blocks++;
	ext2fs_mark_inode_bitmap(inode_dup_map, p->ino);

	add_dupe(ctx, p->ino, *block_nr, p->inode);

	return 0;
}

/*
 * Pass 1c: Scan directories for inodes with duplicate blocks.  This
 * is used so that we can print pathnames when prompting the user for
 * what to do.
 */
struct search_dir_struct {
	int             count;
	ext2_ino_t      first_inode;
	ext2_ino_t      max_inode;
};

static int search_dirent_proc(ext2_ino_t dir, int entry,
			      struct ext2_dir_entry *dirent,
			      int offset FSCK_ATTR((unused)),
			      int blocksize FSCK_ATTR((unused)),
			      char *buf FSCK_ATTR((unused)),
			      void *priv_data)
{
	struct search_dir_struct *sd;
	struct dup_inode        *p;
	dnode_t                 *n;

	sd = (struct search_dir_struct *) priv_data;

	if (dirent->inode > sd->max_inode)
		/* Should abort this inode, but not everything */
		return 0;

	if ((dirent->inode < sd->first_inode) || (entry < DIRENT_OTHER_FILE) ||
	    !ext2fs_test_inode_bitmap(inode_dup_map, dirent->inode))
		return 0;

	n = dict_lookup(&ino_dict, INT_TO_VOIDPTR(dirent->inode));
	if (!n)
		return 0;
	p = (struct dup_inode *) dnode_get(n);
	p->dir = dir;
	sd->count--;

	return sd->count ? 0 : DIRENT_ABORT;
}


static void pass1c(e2fsck_t ctx, char *block_buf)
{
	ext2_filsys fs = ctx->fs;
	struct search_dir_struct sd;
	struct problem_context pctx;

	clear_problem_context(&pctx);

	if (!(ctx->options & E2F_OPT_PREEN))
		fix_problem(ctx, PR_1C_PASS_HEADER, &pctx);

	/*
	 * Search through all directories to translate inodes to names
	 * (by searching for the containing directory for that inode.)
	 */
	sd.count = dup_inode_count;
	sd.first_inode = EXT2_FIRST_INODE(fs->super);
	sd.max_inode = fs->super->s_inodes_count;
	ext2fs_dblist_dir_iterate(fs->dblist, 0, block_buf,
				  search_dirent_proc, &sd);
}

static void pass1d(e2fsck_t ctx, char *block_buf)
{
	ext2_filsys fs = ctx->fs;
	struct dup_inode        *p, *t;
	struct dup_block        *q;
	ext2_ino_t              *shared, ino;
	int     shared_len;
	int     i;
	int     file_ok;
	int     meta_data = 0;
	struct problem_context pctx;
	dnode_t *n, *m;
	struct block_el *s;
	struct inode_el *r;

	clear_problem_context(&pctx);

	if (!(ctx->options & E2F_OPT_PREEN))
		fix_problem(ctx, PR_1D_PASS_HEADER, &pctx);
	e2fsck_read_bitmaps(ctx);

	pctx.num = dup_inode_count; /* dict_count(&ino_dict); */
	fix_problem(ctx, PR_1D_NUM_DUP_INODES, &pctx);
	shared = (ext2_ino_t *) e2fsck_allocate_memory(ctx,
				sizeof(ext2_ino_t) * dict_count(&ino_dict),
				"Shared inode list");
	for (n = dict_first(&ino_dict); n; n = dict_next(&ino_dict, n)) {
		p = (struct dup_inode *) dnode_get(n);
		shared_len = 0;
		file_ok = 1;
		ino = (ext2_ino_t)VOIDPTR_TO_INT(dnode_getkey(n));
		if (ino == EXT2_BAD_INO || ino == EXT2_RESIZE_INO)
			continue;

		/*
		 * Find all of the inodes which share blocks with this
		 * one.  First we find all of the duplicate blocks
		 * belonging to this inode, and then search each block
		 * get the list of inodes, and merge them together.
		 */
		for (s = p->block_list; s; s = s->next) {
			m = dict_lookup(&blk_dict, INT_TO_VOIDPTR(s->block));
			if (!m)
				continue; /* Should never happen... */
			q = (struct dup_block *) dnode_get(m);
			if (q->num_bad > 1)
				file_ok = 0;
			if (check_if_fs_block(ctx, s->block)) {
				file_ok = 0;
				meta_data = 1;
			}

			/*
			 * Add all inodes used by this block to the
			 * shared[] --- which is a unique list, so
			 * if an inode is already in shared[], don't
			 * add it again.
			 */
			for (r = q->inode_list; r; r = r->next) {
				if (r->inode == ino)
					continue;
				for (i = 0; i < shared_len; i++)
					if (shared[i] == r->inode)
						break;
				if (i == shared_len) {
					shared[shared_len++] = r->inode;
				}
			}
		}

		/*
		 * Report the inode that we are working on
		 */
		pctx.inode = &p->inode;
		pctx.ino = ino;
		pctx.dir = p->dir;
		pctx.blkcount = p->num_dupblocks;
		pctx.num = meta_data ? shared_len+1 : shared_len;
		fix_problem(ctx, PR_1D_DUP_FILE, &pctx);
		pctx.blkcount = 0;
		pctx.num = 0;

		if (meta_data)
			fix_problem(ctx, PR_1D_SHARE_METADATA, &pctx);

		for (i = 0; i < shared_len; i++) {
			m = dict_lookup(&ino_dict, INT_TO_VOIDPTR(shared[i]));
			if (!m)
				continue; /* should never happen */
			t = (struct dup_inode *) dnode_get(m);
			/*
			 * Report the inode that we are sharing with
			 */
			pctx.inode = &t->inode;
			pctx.ino = shared[i];
			pctx.dir = t->dir;
			fix_problem(ctx, PR_1D_DUP_FILE_LIST, &pctx);
		}
		if (file_ok) {
			fix_problem(ctx, PR_1D_DUP_BLOCKS_DEALT, &pctx);
			continue;
		}
		if (fix_problem(ctx, PR_1D_CLONE_QUESTION, &pctx)) {
			pctx.errcode = clone_file(ctx, ino, p, block_buf);
			if (pctx.errcode)
				fix_problem(ctx, PR_1D_CLONE_ERROR, &pctx);
			else
				continue;
		}
		if (fix_problem(ctx, PR_1D_DELETE_QUESTION, &pctx))
			delete_file(ctx, ino, p, block_buf);
		else
			ext2fs_unmark_valid(fs);
	}
	ext2fs_free_mem(&shared);
}

/*
 * Drop the refcount on the dup_block structure, and clear the entry
 * in the block_dup_map if appropriate.
 */
static void decrement_badcount(e2fsck_t ctx, blk_t block, struct dup_block *p)
{
	p->num_bad--;
	if (p->num_bad <= 0 ||
	    (p->num_bad == 1 && !check_if_fs_block(ctx, block)))
		ext2fs_unmark_block_bitmap(ctx->block_dup_map, block);
}

static int delete_file_block(ext2_filsys fs,
			     blk_t      *block_nr,
			     e2_blkcnt_t blockcnt FSCK_ATTR((unused)),
			     blk_t ref_block FSCK_ATTR((unused)),
			     int ref_offset FSCK_ATTR((unused)),
			     void *priv_data)
{
	struct process_block_struct_1b *pb;
	struct dup_block *p;
	dnode_t *n;
	e2fsck_t ctx;

	pb = (struct process_block_struct_1b *) priv_data;
	ctx = pb->ctx;

	if (HOLE_BLKADDR(*block_nr))
		return 0;

	if (ext2fs_test_block_bitmap(ctx->block_dup_map, *block_nr)) {
		n = dict_lookup(&blk_dict, INT_TO_VOIDPTR(*block_nr));
		if (n) {
			p = (struct dup_block *) dnode_get(n);
			decrement_badcount(ctx, *block_nr, p);
		} else
			bb_error_msg(_("internal error; can't find dup_blk for %d"),
				*block_nr);
	} else {
		ext2fs_unmark_block_bitmap(ctx->block_found_map, *block_nr);
		ext2fs_block_alloc_stats(fs, *block_nr, -1);
	}

	return 0;
}

static void delete_file(e2fsck_t ctx, ext2_ino_t ino,
			struct dup_inode *dp, char* block_buf)
{
	ext2_filsys fs = ctx->fs;
	struct process_block_struct_1b pb;
	struct ext2_inode       inode;
	struct problem_context  pctx;
	unsigned int            count;

	clear_problem_context(&pctx);
	pctx.ino = pb.ino = ino;
	pb.dup_blocks = dp->num_dupblocks;
	pb.ctx = ctx;
	pctx.str = "delete_file";

	e2fsck_read_inode(ctx, ino, &inode, "delete_file");
	if (ext2fs_inode_has_valid_blocks(&inode))
		pctx.errcode = ext2fs_block_iterate2(fs, ino, 0, block_buf,
						     delete_file_block, &pb);
	if (pctx.errcode)
		fix_problem(ctx, PR_1B_BLOCK_ITERATE, &pctx);
	ext2fs_unmark_inode_bitmap(ctx->inode_used_map, ino);
	ext2fs_unmark_inode_bitmap(ctx->inode_dir_map, ino);
	if (ctx->inode_bad_map)
		ext2fs_unmark_inode_bitmap(ctx->inode_bad_map, ino);
	ext2fs_inode_alloc_stats2(fs, ino, -1, LINUX_S_ISDIR(inode.i_mode));

	/* Inode may have changed by block_iterate, so reread it */
	e2fsck_read_inode(ctx, ino, &inode, "delete_file");
	inode.i_links_count = 0;
	inode.i_dtime = time(NULL);
	if (inode.i_file_acl &&
	    (fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_EXT_ATTR)) {
		count = 1;
		pctx.errcode = ext2fs_adjust_ea_refcount(fs, inode.i_file_acl,
						   block_buf, -1, &count);
		if (pctx.errcode == EXT2_ET_BAD_EA_BLOCK_NUM) {
			pctx.errcode = 0;
			count = 1;
		}
		if (pctx.errcode) {
			pctx.blk = inode.i_file_acl;
			fix_problem(ctx, PR_1B_ADJ_EA_REFCOUNT, &pctx);
		}
		/*
		 * If the count is zero, then arrange to have the
		 * block deleted.  If the block is in the block_dup_map,
		 * also call delete_file_block since it will take care
		 * of keeping the accounting straight.
		 */
		if ((count == 0) ||
		    ext2fs_test_block_bitmap(ctx->block_dup_map,
					     inode.i_file_acl))
			delete_file_block(fs, &inode.i_file_acl,
					  BLOCK_COUNT_EXTATTR, 0, 0, &pb);
	}
	e2fsck_write_inode(ctx, ino, &inode, "delete_file");
}

struct clone_struct {
	errcode_t       errcode;
	ext2_ino_t      dir;
	char    *buf;
	e2fsck_t ctx;
};

static int clone_file_block(ext2_filsys fs,
			    blk_t       *block_nr,
			    e2_blkcnt_t blockcnt,
			    blk_t ref_block FSCK_ATTR((unused)),
			    int ref_offset FSCK_ATTR((unused)),
			    void *priv_data)
{
	struct dup_block *p;
	blk_t   new_block;
	errcode_t       retval;
	struct clone_struct *cs = (struct clone_struct *) priv_data;
	dnode_t *n;
	e2fsck_t ctx;

	ctx = cs->ctx;

	if (HOLE_BLKADDR(*block_nr))
		return 0;

	if (ext2fs_test_block_bitmap(ctx->block_dup_map, *block_nr)) {
		n = dict_lookup(&blk_dict, INT_TO_VOIDPTR(*block_nr));
		if (n) {
			p = (struct dup_block *) dnode_get(n);
			retval = ext2fs_new_block(fs, 0, ctx->block_found_map,
						  &new_block);
			if (retval) {
				cs->errcode = retval;
				return BLOCK_ABORT;
			}
			if (cs->dir && (blockcnt >= 0)) {
				retval = ext2fs_set_dir_block(fs->dblist,
				      cs->dir, new_block, blockcnt);
				if (retval) {
					cs->errcode = retval;
					return BLOCK_ABORT;
				}
			}

			retval = io_channel_read_blk(fs->io, *block_nr, 1,
						     cs->buf);
			if (retval) {
				cs->errcode = retval;
				return BLOCK_ABORT;
			}
			retval = io_channel_write_blk(fs->io, new_block, 1,
						      cs->buf);
			if (retval) {
				cs->errcode = retval;
				return BLOCK_ABORT;
			}
			decrement_badcount(ctx, *block_nr, p);
			*block_nr = new_block;
			ext2fs_mark_block_bitmap(ctx->block_found_map,
						 new_block);
			ext2fs_mark_block_bitmap(fs->block_map, new_block);
			return BLOCK_CHANGED;
		} else
			bb_error_msg(_("internal error; can't find dup_blk for %d"),
				*block_nr);
	}
	return 0;
}

static int clone_file(e2fsck_t ctx, ext2_ino_t ino,
		      struct dup_inode *dp, char* block_buf)
{
	ext2_filsys fs = ctx->fs;
	errcode_t       retval;
	struct clone_struct cs;
	struct problem_context  pctx;
	blk_t           blk;
	dnode_t         *n;
	struct inode_el *ino_el;
	struct dup_block        *db;
	struct dup_inode        *di;

	clear_problem_context(&pctx);
	cs.errcode = 0;
	cs.dir = 0;
	cs.ctx = ctx;
	retval = ext2fs_get_mem(fs->blocksize, &cs.buf);
	if (retval)
		return retval;

	if (ext2fs_test_inode_bitmap(ctx->inode_dir_map, ino))
		cs.dir = ino;

	pctx.ino = ino;
	pctx.str = "clone_file";
	if (ext2fs_inode_has_valid_blocks(&dp->inode))
		pctx.errcode = ext2fs_block_iterate2(fs, ino, 0, block_buf,
						     clone_file_block, &cs);
	ext2fs_mark_bb_dirty(fs);
	if (pctx.errcode) {
		fix_problem(ctx, PR_1B_BLOCK_ITERATE, &pctx);
		retval = pctx.errcode;
		goto errout;
	}
	if (cs.errcode) {
		bb_error_msg(_("returned from clone_file_block"));
		retval = cs.errcode;
		goto errout;
	}
	/* The inode may have changed on disk, so we have to re-read it */
	e2fsck_read_inode(ctx, ino, &dp->inode, "clone file EA");
	blk = dp->inode.i_file_acl;
	if (blk && (clone_file_block(fs, &dp->inode.i_file_acl,
				     BLOCK_COUNT_EXTATTR, 0, 0, &cs) ==
		    BLOCK_CHANGED)) {
		e2fsck_write_inode(ctx, ino, &dp->inode, "clone file EA");
		/*
		 * If we cloned the EA block, find all other inodes
		 * which refered to that EA block, and modify
		 * them to point to the new EA block.
		 */
		n = dict_lookup(&blk_dict, INT_TO_VOIDPTR(blk));
		db = (struct dup_block *) dnode_get(n);
		for (ino_el = db->inode_list; ino_el; ino_el = ino_el->next) {
			if (ino_el->inode == ino)
				continue;
			n = dict_lookup(&ino_dict, INT_TO_VOIDPTR(ino_el->inode));
			di = (struct dup_inode *) dnode_get(n);
			if (di->inode.i_file_acl == blk) {
				di->inode.i_file_acl = dp->inode.i_file_acl;
				e2fsck_write_inode(ctx, ino_el->inode,
					   &di->inode, "clone file EA");
				decrement_badcount(ctx, blk, db);
			}
		}
	}
	retval = 0;
errout:
	ext2fs_free_mem(&cs.buf);
	return retval;
}

/*
 * This routine returns 1 if a block overlaps with one of the superblocks,
 * group descriptors, inode bitmaps, or block bitmaps.
 */
static int check_if_fs_block(e2fsck_t ctx, blk_t test_block)
{
	ext2_filsys fs = ctx->fs;
	blk_t   block;
	dgrp_t  i;

	block = fs->super->s_first_data_block;
	for (i = 0; i < fs->group_desc_count; i++) {

		/* Check superblocks/block group descriptros */
		if (ext2fs_bg_has_super(fs, i)) {
			if (test_block >= block &&
			    (test_block <= block + fs->desc_blocks))
				return 1;
		}

		/* Check the inode table */
		if ((fs->group_desc[i].bg_inode_table) &&
		    (test_block >= fs->group_desc[i].bg_inode_table) &&
		    (test_block < (fs->group_desc[i].bg_inode_table +
				   fs->inode_blocks_per_group)))
			return 1;

		/* Check the bitmap blocks */
		if ((test_block == fs->group_desc[i].bg_block_bitmap) ||
		    (test_block == fs->group_desc[i].bg_inode_bitmap))
			return 1;

		block += fs->super->s_blocks_per_group;
	}
	return 0;
}
/*
 * pass2.c --- check directory structure
 *
 * Pass 2 of e2fsck iterates through all active directory inodes, and
 * applies to following tests to each directory entry in the directory
 * blocks in the inodes:
 *
 *      - The length of the directory entry (rec_len) should be at
 *              least 8 bytes, and no more than the remaining space
 *              left in the directory block.
 *      - The length of the name in the directory entry (name_len)
 *              should be less than (rec_len - 8).
 *      - The inode number in the directory entry should be within
 *              legal bounds.
 *      - The inode number should refer to a in-use inode.
 *      - The first entry should be '.', and its inode should be
 *              the inode of the directory.
 *      - The second entry should be '..'.
 *
 * To minimize disk seek time, the directory blocks are processed in
 * sorted order of block numbers.
 *
 * Pass 2 also collects the following information:
 *      - The inode numbers of the subdirectories for each directory.
 *
 * Pass 2 relies on the following information from previous passes:
 *      - The directory information collected in pass 1.
 *      - The inode_used_map bitmap
 *      - The inode_bad_map bitmap
 *      - The inode_dir_map bitmap
 *
 * Pass 2 frees the following data structures
 *      - The inode_bad_map bitmap
 *      - The inode_reg_map bitmap
 */

/*
 * Keeps track of how many times an inode is referenced.
 */
static void deallocate_inode(e2fsck_t ctx, ext2_ino_t ino, char* block_buf);
static int check_dir_block(ext2_filsys fs,
			   struct ext2_db_entry *dir_blocks_info,
			   void *priv_data);
static int allocate_dir_block(e2fsck_t ctx, struct ext2_db_entry *dir_blocks_info,
			      struct problem_context *pctx);
static int update_dir_block(ext2_filsys fs,
			    blk_t       *block_nr,
			    e2_blkcnt_t blockcnt,
			    blk_t       ref_block,
			    int         ref_offset,
			    void        *priv_data);
static void clear_htree(e2fsck_t ctx, ext2_ino_t ino);
static int htree_depth(struct dx_dir_info *dx_dir,
		       struct dx_dirblock_info *dx_db);
static int special_dir_block_cmp(const void *a, const void *b);

struct check_dir_struct {
	char *buf;
	struct problem_context  pctx;
	int     count, max;
	e2fsck_t ctx;
};

static void e2fsck_pass2(e2fsck_t ctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	struct problem_context  pctx;
	ext2_filsys             fs = ctx->fs;
	char                    *buf;
	struct dir_info         *dir;
	struct check_dir_struct cd;
	struct dx_dir_info      *dx_dir;
	struct dx_dirblock_info *dx_db, *dx_parent;
	int                     b;
	int                     i, depth;
	problem_t               code;
	int                     bad_dir;

	clear_problem_context(&cd.pctx);

	/* Pass 2 */

	if (!(ctx->options & E2F_OPT_PREEN))
		fix_problem(ctx, PR_2_PASS_HEADER, &cd.pctx);

	cd.pctx.errcode = ext2fs_create_icount2(fs, EXT2_ICOUNT_OPT_INCREMENT,
						0, ctx->inode_link_info,
						&ctx->inode_count);
	if (cd.pctx.errcode) {
		fix_problem(ctx, PR_2_ALLOCATE_ICOUNT, &cd.pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	buf = (char *) e2fsck_allocate_memory(ctx, 2*fs->blocksize,
					      "directory scan buffer");

	/*
	 * Set up the parent pointer for the root directory, if
	 * present.  (If the root directory is not present, we will
	 * create it in pass 3.)
	 */
	dir = e2fsck_get_dir_info(ctx, EXT2_ROOT_INO);
	if (dir)
		dir->parent = EXT2_ROOT_INO;

	cd.buf = buf;
	cd.ctx = ctx;
	cd.count = 1;
	cd.max = ext2fs_dblist_count(fs->dblist);

	if (ctx->progress)
		(void) (ctx->progress)(ctx, 2, 0, cd.max);

	if (fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_DIR_INDEX)
		ext2fs_dblist_sort(fs->dblist, special_dir_block_cmp);

	cd.pctx.errcode = ext2fs_dblist_iterate(fs->dblist, check_dir_block,
						&cd);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;
	if (cd.pctx.errcode) {
		fix_problem(ctx, PR_2_DBLIST_ITERATE, &cd.pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

#ifdef ENABLE_HTREE
	for (i=0; (dx_dir = e2fsck_dx_dir_info_iter(ctx, &i)) != 0;) {
		if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
			return;
		if (dx_dir->numblocks == 0)
			continue;
		clear_problem_context(&pctx);
		bad_dir = 0;
		pctx.dir = dx_dir->ino;
		dx_db = dx_dir->dx_block;
		if (dx_db->flags & DX_FLAG_REFERENCED)
			dx_db->flags |= DX_FLAG_DUP_REF;
		else
			dx_db->flags |= DX_FLAG_REFERENCED;
		/*
		 * Find all of the first and last leaf blocks, and
		 * update their parent's min and max hash values
		 */
		for (b=0, dx_db = dx_dir->dx_block;
		     b < dx_dir->numblocks;
		     b++, dx_db++) {
			if ((dx_db->type != DX_DIRBLOCK_LEAF) ||
			    !(dx_db->flags & (DX_FLAG_FIRST | DX_FLAG_LAST)))
				continue;
			dx_parent = &dx_dir->dx_block[dx_db->parent];
			/*
			 * XXX Make sure dx_parent->min_hash > dx_db->min_hash
			 */
			if (dx_db->flags & DX_FLAG_FIRST)
				dx_parent->min_hash = dx_db->min_hash;
			/*
			 * XXX Make sure dx_parent->max_hash < dx_db->max_hash
			 */
			if (dx_db->flags & DX_FLAG_LAST)
				dx_parent->max_hash = dx_db->max_hash;
		}

		for (b=0, dx_db = dx_dir->dx_block;
		     b < dx_dir->numblocks;
		     b++, dx_db++) {
			pctx.blkcount = b;
			pctx.group = dx_db->parent;
			code = 0;
			if (!(dx_db->flags & DX_FLAG_FIRST) &&
			    (dx_db->min_hash < dx_db->node_min_hash)) {
				pctx.blk = dx_db->min_hash;
				pctx.blk2 = dx_db->node_min_hash;
				code = PR_2_HTREE_MIN_HASH;
				fix_problem(ctx, code, &pctx);
				bad_dir++;
			}
			if (dx_db->type == DX_DIRBLOCK_LEAF) {
				depth = htree_depth(dx_dir, dx_db);
				if (depth != dx_dir->depth) {
					code = PR_2_HTREE_BAD_DEPTH;
					fix_problem(ctx, code, &pctx);
					bad_dir++;
				}
			}
			/*
			 * This test doesn't apply for the root block
			 * at block #0
			 */
			if (b &&
			    (dx_db->max_hash > dx_db->node_max_hash)) {
				pctx.blk = dx_db->max_hash;
				pctx.blk2 = dx_db->node_max_hash;
				code = PR_2_HTREE_MAX_HASH;
				fix_problem(ctx, code, &pctx);
				bad_dir++;
			}
			if (!(dx_db->flags & DX_FLAG_REFERENCED)) {
				code = PR_2_HTREE_NOTREF;
				fix_problem(ctx, code, &pctx);
				bad_dir++;
			} else if (dx_db->flags & DX_FLAG_DUP_REF) {
				code = PR_2_HTREE_DUPREF;
				fix_problem(ctx, code, &pctx);
				bad_dir++;
			}
			if (code == 0)
				continue;
		}
		if (bad_dir && fix_problem(ctx, PR_2_HTREE_CLEAR, &pctx)) {
			clear_htree(ctx, dx_dir->ino);
			dx_dir->numblocks = 0;
		}
	}
#endif
	ext2fs_free_mem(&buf);
	ext2fs_free_dblist(fs->dblist);

	ext2fs_free_inode_bitmap(ctx->inode_bad_map);
	ctx->inode_bad_map = 0;
	ext2fs_free_inode_bitmap(ctx->inode_reg_map);
	ctx->inode_reg_map = 0;

	clear_problem_context(&pctx);
	if (ctx->large_files) {
		if (!(sb->s_feature_ro_compat &
		      EXT2_FEATURE_RO_COMPAT_LARGE_FILE) &&
		    fix_problem(ctx, PR_2_FEATURE_LARGE_FILES, &pctx)) {
			sb->s_feature_ro_compat |=
				EXT2_FEATURE_RO_COMPAT_LARGE_FILE;
			ext2fs_mark_super_dirty(fs);
		}
		if (sb->s_rev_level == EXT2_GOOD_OLD_REV &&
		    fix_problem(ctx, PR_1_FS_REV_LEVEL, &pctx)) {
			ext2fs_update_dynamic_rev(fs);
			ext2fs_mark_super_dirty(fs);
		}
	} else if (!ctx->large_files &&
	    (sb->s_feature_ro_compat &
	      EXT2_FEATURE_RO_COMPAT_LARGE_FILE)) {
		if (fs->flags & EXT2_FLAG_RW) {
			sb->s_feature_ro_compat &=
				~EXT2_FEATURE_RO_COMPAT_LARGE_FILE;
			ext2fs_mark_super_dirty(fs);
		}
	}
}

#define MAX_DEPTH 32000
static int htree_depth(struct dx_dir_info *dx_dir,
		       struct dx_dirblock_info *dx_db)
{
	int     depth = 0;

	while (dx_db->type != DX_DIRBLOCK_ROOT && depth < MAX_DEPTH) {
		dx_db = &dx_dir->dx_block[dx_db->parent];
		depth++;
	}
	return depth;
}

static int dict_de_cmp(const void *a, const void *b)
{
	const struct ext2_dir_entry *de_a, *de_b;
	int     a_len, b_len;

	de_a = (const struct ext2_dir_entry *) a;
	a_len = de_a->name_len & 0xFF;
	de_b = (const struct ext2_dir_entry *) b;
	b_len = de_b->name_len & 0xFF;

	if (a_len != b_len)
		return (a_len - b_len);

	return strncmp(de_a->name, de_b->name, a_len);
}

/*
 * This is special sort function that makes sure that directory blocks
 * with a dirblock of zero are sorted to the beginning of the list.
 * This guarantees that the root node of the htree directories are
 * processed first, so we know what hash version to use.
 */
static int special_dir_block_cmp(const void *a, const void *b)
{
	const struct ext2_db_entry *db_a =
		(const struct ext2_db_entry *) a;
	const struct ext2_db_entry *db_b =
		(const struct ext2_db_entry *) b;

	if (db_a->blockcnt && !db_b->blockcnt)
		return 1;

	if (!db_a->blockcnt && db_b->blockcnt)
		return -1;

	if (db_a->blk != db_b->blk)
		return (int) (db_a->blk - db_b->blk);

	if (db_a->ino != db_b->ino)
		return (int) (db_a->ino - db_b->ino);

	return (int) (db_a->blockcnt - db_b->blockcnt);
}


/*
 * Make sure the first entry in the directory is '.', and that the
 * directory entry is sane.
 */
static int check_dot(e2fsck_t ctx,
		     struct ext2_dir_entry *dirent,
		     ext2_ino_t ino, struct problem_context *pctx)
{
	struct ext2_dir_entry *nextdir;
	int     status = 0;
	int     created = 0;
	int     new_len;
	int     problem = 0;

	if (!dirent->inode)
		problem = PR_2_MISSING_DOT;
	else if (((dirent->name_len & 0xFF) != 1) ||
		 (dirent->name[0] != '.'))
		problem = PR_2_1ST_NOT_DOT;
	else if (dirent->name[1] != '\0')
		problem = PR_2_DOT_NULL_TERM;

	if (problem) {
		if (fix_problem(ctx, problem, pctx)) {
			if (dirent->rec_len < 12)
				dirent->rec_len = 12;
			dirent->inode = ino;
			dirent->name_len = 1;
			dirent->name[0] = '.';
			dirent->name[1] = '\0';
			status = 1;
			created = 1;
		}
	}
	if (dirent->inode != ino) {
		if (fix_problem(ctx, PR_2_BAD_INODE_DOT, pctx)) {
			dirent->inode = ino;
			status = 1;
		}
	}
	if (dirent->rec_len > 12) {
		new_len = dirent->rec_len - 12;
		if (new_len > 12) {
			if (created ||
			    fix_problem(ctx, PR_2_SPLIT_DOT, pctx)) {
				nextdir = (struct ext2_dir_entry *)
					((char *) dirent + 12);
				dirent->rec_len = 12;
				nextdir->rec_len = new_len;
				nextdir->inode = 0;
				nextdir->name_len = 0;
				status = 1;
			}
		}
	}
	return status;
}

/*
 * Make sure the second entry in the directory is '..', and that the
 * directory entry is sane.  We do not check the inode number of '..'
 * here; this gets done in pass 3.
 */
static int check_dotdot(e2fsck_t ctx,
			struct ext2_dir_entry *dirent,
			struct dir_info *dir, struct problem_context *pctx)
{
	int             problem = 0;

	if (!dirent->inode)
		problem = PR_2_MISSING_DOT_DOT;
	else if (((dirent->name_len & 0xFF) != 2) ||
		 (dirent->name[0] != '.') ||
		 (dirent->name[1] != '.'))
		problem = PR_2_2ND_NOT_DOT_DOT;
	else if (dirent->name[2] != '\0')
		problem = PR_2_DOT_DOT_NULL_TERM;

	if (problem) {
		if (fix_problem(ctx, problem, pctx)) {
			if (dirent->rec_len < 12)
				dirent->rec_len = 12;
			/*
			 * Note: we don't have the parent inode just
			 * yet, so we will fill it in with the root
			 * inode.  This will get fixed in pass 3.
			 */
			dirent->inode = EXT2_ROOT_INO;
			dirent->name_len = 2;
			dirent->name[0] = '.';
			dirent->name[1] = '.';
			dirent->name[2] = '\0';
			return 1;
		}
		return 0;
	}
	dir->dotdot = dirent->inode;
	return 0;
}

/*
 * Check to make sure a directory entry doesn't contain any illegal
 * characters.
 */
static int check_name(e2fsck_t ctx,
		      struct ext2_dir_entry *dirent,
		      struct problem_context *pctx)
{
	int     i;
	int     fixup = -1;
	int     ret = 0;

	for ( i = 0; i < (dirent->name_len & 0xFF); i++) {
		if (dirent->name[i] == '/' || dirent->name[i] == '\0') {
			if (fixup < 0) {
				fixup = fix_problem(ctx, PR_2_BAD_NAME, pctx);
			}
			if (fixup) {
				dirent->name[i] = '.';
				ret = 1;
			}
		}
	}
	return ret;
}

/*
 * Check the directory filetype (if present)
 */

/*
 * Given a mode, return the ext2 file type
 */
static int ext2_file_type(unsigned int mode)
{
	if (LINUX_S_ISREG(mode))
		return EXT2_FT_REG_FILE;

	if (LINUX_S_ISDIR(mode))
		return EXT2_FT_DIR;

	if (LINUX_S_ISCHR(mode))
		return EXT2_FT_CHRDEV;

	if (LINUX_S_ISBLK(mode))
		return EXT2_FT_BLKDEV;

	if (LINUX_S_ISLNK(mode))
		return EXT2_FT_SYMLINK;

	if (LINUX_S_ISFIFO(mode))
		return EXT2_FT_FIFO;

	if (LINUX_S_ISSOCK(mode))
		return EXT2_FT_SOCK;

	return 0;
}

static int check_filetype(e2fsck_t ctx,
				   struct ext2_dir_entry *dirent,
				   struct problem_context *pctx)
{
	int     filetype = dirent->name_len >> 8;
	int     should_be = EXT2_FT_UNKNOWN;
	struct ext2_inode       inode;

	if (!(ctx->fs->super->s_feature_incompat &
	      EXT2_FEATURE_INCOMPAT_FILETYPE)) {
		if (filetype == 0 ||
		    !fix_problem(ctx, PR_2_CLEAR_FILETYPE, pctx))
			return 0;
		dirent->name_len = dirent->name_len & 0xFF;
		return 1;
	}

	if (ext2fs_test_inode_bitmap(ctx->inode_dir_map, dirent->inode)) {
		should_be = EXT2_FT_DIR;
	} else if (ext2fs_test_inode_bitmap(ctx->inode_reg_map,
					    dirent->inode)) {
		should_be = EXT2_FT_REG_FILE;
	} else if (ctx->inode_bad_map &&
		   ext2fs_test_inode_bitmap(ctx->inode_bad_map,
					    dirent->inode))
		should_be = 0;
	else {
		e2fsck_read_inode(ctx, dirent->inode, &inode,
				  "check_filetype");
		should_be = ext2_file_type(inode.i_mode);
	}
	if (filetype == should_be)
		return 0;
	pctx->num = should_be;

	if (fix_problem(ctx, filetype ? PR_2_BAD_FILETYPE : PR_2_SET_FILETYPE,
			pctx) == 0)
		return 0;

	dirent->name_len = (dirent->name_len & 0xFF) | should_be << 8;
	return 1;
}

#ifdef ENABLE_HTREE
static void parse_int_node(ext2_filsys fs,
			   struct ext2_db_entry *db,
			   struct check_dir_struct *cd,
			   struct dx_dir_info   *dx_dir,
			   char *block_buf)
{
	struct          ext2_dx_root_info  *root;
	struct          ext2_dx_entry *ent;
	struct          ext2_dx_countlimit *limit;
	struct dx_dirblock_info *dx_db;
	int             i, expect_limit, count;
	blk_t           blk;
	ext2_dirhash_t  min_hash = 0xffffffff;
	ext2_dirhash_t  max_hash = 0;
	ext2_dirhash_t  hash = 0, prev_hash;

	if (db->blockcnt == 0) {
		root = (struct ext2_dx_root_info *) (block_buf + 24);
		ent = (struct ext2_dx_entry *) (block_buf + 24 + root->info_length);
	} else {
		ent = (struct ext2_dx_entry *) (block_buf+8);
	}
	limit = (struct ext2_dx_countlimit *) ent;

	count = ext2fs_le16_to_cpu(limit->count);
	expect_limit = (fs->blocksize - ((char *) ent - block_buf)) /
		sizeof(struct ext2_dx_entry);
	if (ext2fs_le16_to_cpu(limit->limit) != expect_limit) {
		cd->pctx.num = ext2fs_le16_to_cpu(limit->limit);
		if (fix_problem(cd->ctx, PR_2_HTREE_BAD_LIMIT, &cd->pctx))
			goto clear_and_exit;
	}
	if (count > expect_limit) {
		cd->pctx.num = count;
		if (fix_problem(cd->ctx, PR_2_HTREE_BAD_COUNT, &cd->pctx))
			goto clear_and_exit;
		count = expect_limit;
	}

	for (i=0; i < count; i++) {
		prev_hash = hash;
		hash = i ? (ext2fs_le32_to_cpu(ent[i].hash) & ~1) : 0;
		blk = ext2fs_le32_to_cpu(ent[i].block) & 0x0ffffff;
		/* Check to make sure the block is valid */
		if (blk > (blk_t) dx_dir->numblocks) {
			cd->pctx.blk = blk;
			if (fix_problem(cd->ctx, PR_2_HTREE_BADBLK,
					&cd->pctx))
				goto clear_and_exit;
		}
		if (hash < prev_hash &&
		    fix_problem(cd->ctx, PR_2_HTREE_HASH_ORDER, &cd->pctx))
			goto clear_and_exit;
		dx_db = &dx_dir->dx_block[blk];
		if (dx_db->flags & DX_FLAG_REFERENCED) {
			dx_db->flags |= DX_FLAG_DUP_REF;
		} else {
			dx_db->flags |= DX_FLAG_REFERENCED;
			dx_db->parent = db->blockcnt;
		}
		if (hash < min_hash)
			min_hash = hash;
		if (hash > max_hash)
			max_hash = hash;
		dx_db->node_min_hash = hash;
		if ((i+1) < count)
			dx_db->node_max_hash =
			  ext2fs_le32_to_cpu(ent[i+1].hash) & ~1;
		else {
			dx_db->node_max_hash = 0xfffffffe;
			dx_db->flags |= DX_FLAG_LAST;
		}
		if (i == 0)
			dx_db->flags |= DX_FLAG_FIRST;
	}
	dx_db = &dx_dir->dx_block[db->blockcnt];
	dx_db->min_hash = min_hash;
	dx_db->max_hash = max_hash;
	return;

clear_and_exit:
	clear_htree(cd->ctx, cd->pctx.ino);
	dx_dir->numblocks = 0;
}
#endif /* ENABLE_HTREE */

/*
 * Given a busted directory, try to salvage it somehow.
 *
 */
static void salvage_directory(ext2_filsys fs,
			      struct ext2_dir_entry *dirent,
			      struct ext2_dir_entry *prev,
			      unsigned int *offset)
{
	char    *cp = (char *) dirent;
	int left = fs->blocksize - *offset - dirent->rec_len;
	int name_len = dirent->name_len & 0xFF;

	/*
	 * Special case of directory entry of size 8: copy what's left
	 * of the directory block up to cover up the invalid hole.
	 */
	if ((left >= 12) && (dirent->rec_len == 8)) {
		memmove(cp, cp+8, left);
		memset(cp + left, 0, 8);
		return;
	}
	/*
	 * If the directory entry overruns the end of the directory
	 * block, and the name is small enough to fit, then adjust the
	 * record length.
	 */
	if ((left < 0) &&
	    (name_len + 8 <= dirent->rec_len + left) &&
	    dirent->inode <= fs->super->s_inodes_count &&
	    strnlen(dirent->name, name_len) == name_len) {
		dirent->rec_len += left;
		return;
	}
	/*
	 * If the directory entry is a multiple of four, so it is
	 * valid, let the previous directory entry absorb the invalid
	 * one.
	 */
	if (prev && dirent->rec_len && (dirent->rec_len % 4) == 0) {
		prev->rec_len += dirent->rec_len;
		*offset += dirent->rec_len;
		return;
	}
	/*
	 * Default salvage method --- kill all of the directory
	 * entries for the rest of the block.  We will either try to
	 * absorb it into the previous directory entry, or create a
	 * new empty directory entry the rest of the directory block.
	 */
	if (prev) {
		prev->rec_len += fs->blocksize - *offset;
		*offset = fs->blocksize;
	} else {
		dirent->rec_len = fs->blocksize - *offset;
		dirent->name_len = 0;
		dirent->inode = 0;
	}
}

static int check_dir_block(ext2_filsys fs,
			   struct ext2_db_entry *db,
			   void *priv_data)
{
	struct dir_info         *subdir, *dir;
	struct dx_dir_info      *dx_dir;
#ifdef ENABLE_HTREE
	struct dx_dirblock_info *dx_db = NULL;
#endif /* ENABLE_HTREE */
	struct ext2_dir_entry   *dirent, *prev;
	ext2_dirhash_t          hash;
	unsigned int            offset = 0;
	int                     dir_modified = 0;
	int                     dot_state;
	blk_t                   block_nr = db->blk;
	ext2_ino_t              ino = db->ino;
	__u16                   links;
	struct check_dir_struct *cd;
	char                    *buf;
	e2fsck_t                ctx;
	int                     problem;
	struct ext2_dx_root_info *root;
	struct ext2_dx_countlimit *limit;
	static dict_t de_dict;
	struct problem_context  pctx;
	int     dups_found = 0;

	cd = (struct check_dir_struct *) priv_data;
	buf = cd->buf;
	ctx = cd->ctx;

	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return DIRENT_ABORT;

	if (ctx->progress && (ctx->progress)(ctx, 2, cd->count++, cd->max))
		return DIRENT_ABORT;

	/*
	 * Make sure the inode is still in use (could have been
	 * deleted in the duplicate/bad blocks pass.
	 */
	if (!(ext2fs_test_inode_bitmap(ctx->inode_used_map, ino)))
		return 0;

	cd->pctx.ino = ino;
	cd->pctx.blk = block_nr;
	cd->pctx.blkcount = db->blockcnt;
	cd->pctx.ino2 = 0;
	cd->pctx.dirent = 0;
	cd->pctx.num = 0;

	if (db->blk == 0) {
		if (allocate_dir_block(ctx, db, &cd->pctx))
			return 0;
		block_nr = db->blk;
	}

	if (db->blockcnt)
		dot_state = 2;
	else
		dot_state = 0;

	if (ctx->dirs_to_hash &&
	    ext2fs_u32_list_test(ctx->dirs_to_hash, ino))
		dups_found++;

	cd->pctx.errcode = ext2fs_read_dir_block(fs, block_nr, buf);
	if (cd->pctx.errcode == EXT2_ET_DIR_CORRUPTED)
		cd->pctx.errcode = 0; /* We'll handle this ourselves */
	if (cd->pctx.errcode) {
		if (!fix_problem(ctx, PR_2_READ_DIRBLOCK, &cd->pctx)) {
			ctx->flags |= E2F_FLAG_ABORT;
			return DIRENT_ABORT;
		}
		memset(buf, 0, fs->blocksize);
	}
#ifdef ENABLE_HTREE
	dx_dir = e2fsck_get_dx_dir_info(ctx, ino);
	if (dx_dir && dx_dir->numblocks) {
		if (db->blockcnt >= dx_dir->numblocks) {
			printf("XXX should never happen!!!\n");
			abort();
		}
		dx_db = &dx_dir->dx_block[db->blockcnt];
		dx_db->type = DX_DIRBLOCK_LEAF;
		dx_db->phys = block_nr;
		dx_db->min_hash = ~0;
		dx_db->max_hash = 0;

		dirent = (struct ext2_dir_entry *) buf;
		limit = (struct ext2_dx_countlimit *) (buf+8);
		if (db->blockcnt == 0) {
			root = (struct ext2_dx_root_info *) (buf + 24);
			dx_db->type = DX_DIRBLOCK_ROOT;
			dx_db->flags |= DX_FLAG_FIRST | DX_FLAG_LAST;
			if ((root->reserved_zero ||
			     root->info_length < 8 ||
			     root->indirect_levels > 1) &&
			    fix_problem(ctx, PR_2_HTREE_BAD_ROOT, &cd->pctx)) {
				clear_htree(ctx, ino);
				dx_dir->numblocks = 0;
				dx_db = 0;
			}
			dx_dir->hashversion = root->hash_version;
			dx_dir->depth = root->indirect_levels + 1;
		} else if ((dirent->inode == 0) &&
			   (dirent->rec_len == fs->blocksize) &&
			   (dirent->name_len == 0) &&
			   (ext2fs_le16_to_cpu(limit->limit) ==
			    ((fs->blocksize-8) /
			     sizeof(struct ext2_dx_entry))))
			dx_db->type = DX_DIRBLOCK_NODE;
	}
#endif /* ENABLE_HTREE */

	dict_init(&de_dict, DICTCOUNT_T_MAX, dict_de_cmp);
	prev = 0;
	do {
		problem = 0;
		dirent = (struct ext2_dir_entry *) (buf + offset);
		cd->pctx.dirent = dirent;
		cd->pctx.num = offset;
		if (((offset + dirent->rec_len) > fs->blocksize) ||
		    (dirent->rec_len < 12) ||
		    ((dirent->rec_len % 4) != 0) ||
		    (((dirent->name_len & 0xFF)+8) > dirent->rec_len)) {
			if (fix_problem(ctx, PR_2_DIR_CORRUPTED, &cd->pctx)) {
				salvage_directory(fs, dirent, prev, &offset);
				dir_modified++;
				continue;
			} else
				goto abort_free_dict;
		}
		if ((dirent->name_len & 0xFF) > EXT2_NAME_LEN) {
			if (fix_problem(ctx, PR_2_FILENAME_LONG, &cd->pctx)) {
				dirent->name_len = EXT2_NAME_LEN;
				dir_modified++;
			}
		}

		if (dot_state == 0) {
			if (check_dot(ctx, dirent, ino, &cd->pctx))
				dir_modified++;
		} else if (dot_state == 1) {
			dir = e2fsck_get_dir_info(ctx, ino);
			if (!dir) {
				fix_problem(ctx, PR_2_NO_DIRINFO, &cd->pctx);
				goto abort_free_dict;
			}
			if (check_dotdot(ctx, dirent, dir, &cd->pctx))
				dir_modified++;
		} else if (dirent->inode == ino) {
			problem = PR_2_LINK_DOT;
			if (fix_problem(ctx, PR_2_LINK_DOT, &cd->pctx)) {
				dirent->inode = 0;
				dir_modified++;
				goto next;
			}
		}
		if (!dirent->inode)
			goto next;

		/*
		 * Make sure the inode listed is a legal one.
		 */
		if (((dirent->inode != EXT2_ROOT_INO) &&
		     (dirent->inode < EXT2_FIRST_INODE(fs->super))) ||
		    (dirent->inode > fs->super->s_inodes_count)) {
			problem = PR_2_BAD_INO;
		} else if (!(ext2fs_test_inode_bitmap(ctx->inode_used_map,
					       dirent->inode))) {
			/*
			 * If the inode is unused, offer to clear it.
			 */
			problem = PR_2_UNUSED_INODE;
		} else if ((dot_state > 1) &&
			   ((dirent->name_len & 0xFF) == 1) &&
			   (dirent->name[0] == '.')) {
			/*
			 * If there's a '.' entry in anything other
			 * than the first directory entry, it's a
			 * duplicate entry that should be removed.
			 */
			problem = PR_2_DUP_DOT;
		} else if ((dot_state > 1) &&
			   ((dirent->name_len & 0xFF) == 2) &&
			   (dirent->name[0] == '.') &&
			   (dirent->name[1] == '.')) {
			/*
			 * If there's a '..' entry in anything other
			 * than the second directory entry, it's a
			 * duplicate entry that should be removed.
			 */
			problem = PR_2_DUP_DOT_DOT;
		} else if ((dot_state > 1) &&
			   (dirent->inode == EXT2_ROOT_INO)) {
			/*
			 * Don't allow links to the root directory.
			 * We check this specially to make sure we
			 * catch this error case even if the root
			 * directory hasn't been created yet.
			 */
			problem = PR_2_LINK_ROOT;
		} else if ((dot_state > 1) &&
			   (dirent->name_len & 0xFF) == 0) {
			/*
			 * Don't allow zero-length directory names.
			 */
			problem = PR_2_NULL_NAME;
		}

		if (problem) {
			if (fix_problem(ctx, problem, &cd->pctx)) {
				dirent->inode = 0;
				dir_modified++;
				goto next;
			} else {
				ext2fs_unmark_valid(fs);
				if (problem == PR_2_BAD_INO)
					goto next;
			}
		}

		/*
		 * If the inode was marked as having bad fields in
		 * pass1, process it and offer to fix/clear it.
		 * (We wait until now so that we can display the
		 * pathname to the user.)
		 */
		if (ctx->inode_bad_map &&
		    ext2fs_test_inode_bitmap(ctx->inode_bad_map,
					     dirent->inode)) {
			if (e2fsck_process_bad_inode(ctx, ino,
						     dirent->inode,
						     buf + fs->blocksize)) {
				dirent->inode = 0;
				dir_modified++;
				goto next;
			}
			if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
				return DIRENT_ABORT;
		}

		if (check_name(ctx, dirent, &cd->pctx))
			dir_modified++;

		if (check_filetype(ctx, dirent, &cd->pctx))
			dir_modified++;

#ifdef ENABLE_HTREE
		if (dx_db) {
			ext2fs_dirhash(dx_dir->hashversion, dirent->name,
				       (dirent->name_len & 0xFF),
				       fs->super->s_hash_seed, &hash, 0);
			if (hash < dx_db->min_hash)
				dx_db->min_hash = hash;
			if (hash > dx_db->max_hash)
				dx_db->max_hash = hash;
		}
#endif

		/*
		 * If this is a directory, then mark its parent in its
		 * dir_info structure.  If the parent field is already
		 * filled in, then this directory has more than one
		 * hard link.  We assume the first link is correct,
		 * and ask the user if he/she wants to clear this one.
		 */
		if ((dot_state > 1) &&
		    (ext2fs_test_inode_bitmap(ctx->inode_dir_map,
					      dirent->inode))) {
			subdir = e2fsck_get_dir_info(ctx, dirent->inode);
			if (!subdir) {
				cd->pctx.ino = dirent->inode;
				fix_problem(ctx, PR_2_NO_DIRINFO, &cd->pctx);
				goto abort_free_dict;
			}
			if (subdir->parent) {
				cd->pctx.ino2 = subdir->parent;
				if (fix_problem(ctx, PR_2_LINK_DIR,
						&cd->pctx)) {
					dirent->inode = 0;
					dir_modified++;
					goto next;
				}
				cd->pctx.ino2 = 0;
			} else
				subdir->parent = ino;
		}

		if (dups_found) {
			;
		} else if (dict_lookup(&de_dict, dirent)) {
			clear_problem_context(&pctx);
			pctx.ino = ino;
			pctx.dirent = dirent;
			fix_problem(ctx, PR_2_REPORT_DUP_DIRENT, &pctx);
			if (!ctx->dirs_to_hash)
				ext2fs_u32_list_create(&ctx->dirs_to_hash, 50);
			if (ctx->dirs_to_hash)
				ext2fs_u32_list_add(ctx->dirs_to_hash, ino);
			dups_found++;
		} else
			dict_alloc_insert(&de_dict, dirent, dirent);

		ext2fs_icount_increment(ctx->inode_count, dirent->inode,
					&links);
		if (links > 1)
			ctx->fs_links_count++;
		ctx->fs_total_count++;
	next:
		prev = dirent;
		offset += dirent->rec_len;
		dot_state++;
	} while (offset < fs->blocksize);
#ifdef ENABLE_HTREE
	if (dx_db) {
		cd->pctx.dir = cd->pctx.ino;
		if ((dx_db->type == DX_DIRBLOCK_ROOT) ||
		    (dx_db->type == DX_DIRBLOCK_NODE))
			parse_int_node(fs, db, cd, dx_dir, buf);
	}
#endif /* ENABLE_HTREE */
	if (offset != fs->blocksize) {
		cd->pctx.num = dirent->rec_len - fs->blocksize + offset;
		if (fix_problem(ctx, PR_2_FINAL_RECLEN, &cd->pctx)) {
			dirent->rec_len = cd->pctx.num;
			dir_modified++;
		}
	}
	if (dir_modified) {
		cd->pctx.errcode = ext2fs_write_dir_block(fs, block_nr, buf);
		if (cd->pctx.errcode) {
			if (!fix_problem(ctx, PR_2_WRITE_DIRBLOCK,
					 &cd->pctx))
				goto abort_free_dict;
		}
		ext2fs_mark_changed(fs);
	}
	dict_free_nodes(&de_dict);
	return 0;
abort_free_dict:
	dict_free_nodes(&de_dict);
	ctx->flags |= E2F_FLAG_ABORT;
	return DIRENT_ABORT;
}

/*
 * This function is called to deallocate a block, and is an interator
 * functioned called by deallocate inode via ext2fs_iterate_block().
 */
static int deallocate_inode_block(ext2_filsys fs, blk_t *block_nr,
				  e2_blkcnt_t blockcnt FSCK_ATTR((unused)),
				  blk_t ref_block FSCK_ATTR((unused)),
				  int ref_offset FSCK_ATTR((unused)),
				  void *priv_data)
{
	e2fsck_t        ctx = (e2fsck_t) priv_data;

	if (HOLE_BLKADDR(*block_nr))
		return 0;
	if ((*block_nr < fs->super->s_first_data_block) ||
	    (*block_nr >= fs->super->s_blocks_count))
		return 0;
	ext2fs_unmark_block_bitmap(ctx->block_found_map, *block_nr);
	ext2fs_block_alloc_stats(fs, *block_nr, -1);
	return 0;
}

/*
 * This fuction deallocates an inode
 */
static void deallocate_inode(e2fsck_t ctx, ext2_ino_t ino, char* block_buf)
{
	ext2_filsys fs = ctx->fs;
	struct ext2_inode       inode;
	struct problem_context  pctx;
	__u32                   count;

	ext2fs_icount_store(ctx->inode_link_info, ino, 0);
	e2fsck_read_inode(ctx, ino, &inode, "deallocate_inode");
	inode.i_links_count = 0;
	inode.i_dtime = time(NULL);
	e2fsck_write_inode(ctx, ino, &inode, "deallocate_inode");
	clear_problem_context(&pctx);
	pctx.ino = ino;

	/*
	 * Fix up the bitmaps...
	 */
	e2fsck_read_bitmaps(ctx);
	ext2fs_unmark_inode_bitmap(ctx->inode_used_map, ino);
	ext2fs_unmark_inode_bitmap(ctx->inode_dir_map, ino);
	if (ctx->inode_bad_map)
		ext2fs_unmark_inode_bitmap(ctx->inode_bad_map, ino);
	ext2fs_inode_alloc_stats2(fs, ino, -1, LINUX_S_ISDIR(inode.i_mode));

	if (inode.i_file_acl &&
	    (fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_EXT_ATTR)) {
		pctx.errcode = ext2fs_adjust_ea_refcount(fs, inode.i_file_acl,
						   block_buf, -1, &count);
		if (pctx.errcode == EXT2_ET_BAD_EA_BLOCK_NUM) {
			pctx.errcode = 0;
			count = 1;
		}
		if (pctx.errcode) {
			pctx.blk = inode.i_file_acl;
			fix_problem(ctx, PR_2_ADJ_EA_REFCOUNT, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		if (count == 0) {
			ext2fs_unmark_block_bitmap(ctx->block_found_map,
						   inode.i_file_acl);
			ext2fs_block_alloc_stats(fs, inode.i_file_acl, -1);
		}
		inode.i_file_acl = 0;
	}

	if (!ext2fs_inode_has_valid_blocks(&inode))
		return;

	if (LINUX_S_ISREG(inode.i_mode) &&
	    (inode.i_size_high || inode.i_size & 0x80000000UL))
		ctx->large_files--;

	pctx.errcode = ext2fs_block_iterate2(fs, ino, 0, block_buf,
					    deallocate_inode_block, ctx);
	if (pctx.errcode) {
		fix_problem(ctx, PR_2_DEALLOC_INODE, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
}

/*
 * This fuction clears the htree flag on an inode
 */
static void clear_htree(e2fsck_t ctx, ext2_ino_t ino)
{
	struct ext2_inode       inode;

	e2fsck_read_inode(ctx, ino, &inode, "clear_htree");
	inode.i_flags = inode.i_flags & ~EXT2_INDEX_FL;
	e2fsck_write_inode(ctx, ino, &inode, "clear_htree");
	if (ctx->dirs_to_hash)
		ext2fs_u32_list_add(ctx->dirs_to_hash, ino);
}


static int e2fsck_process_bad_inode(e2fsck_t ctx, ext2_ino_t dir,
				    ext2_ino_t ino, char *buf)
{
	ext2_filsys fs = ctx->fs;
	struct ext2_inode       inode;
	int                     inode_modified = 0;
	int                     not_fixed = 0;
	unsigned char           *frag, *fsize;
	struct problem_context  pctx;
	int     problem = 0;

	e2fsck_read_inode(ctx, ino, &inode, "process_bad_inode");

	clear_problem_context(&pctx);
	pctx.ino = ino;
	pctx.dir = dir;
	pctx.inode = &inode;

	if (inode.i_file_acl &&
	    !(fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_EXT_ATTR) &&
	    fix_problem(ctx, PR_2_FILE_ACL_ZERO, &pctx)) {
		inode.i_file_acl = 0;
#if BB_BIG_ENDIAN
		/*
		 * This is a special kludge to deal with long symlinks
		 * on big endian systems.  i_blocks had already been
		 * decremented earlier in pass 1, but since i_file_acl
		 * hadn't yet been cleared, ext2fs_read_inode()
		 * assumed that the file was short symlink and would
		 * not have byte swapped i_block[0].  Hence, we have
		 * to byte-swap it here.
		 */
		if (LINUX_S_ISLNK(inode.i_mode) &&
		    (fs->flags & EXT2_FLAG_SWAP_BYTES) &&
		    (inode.i_blocks == fs->blocksize >> 9))
			inode.i_block[0] = ext2fs_swab32(inode.i_block[0]);
#endif
		inode_modified++;
	} else
		not_fixed++;

	if (!LINUX_S_ISDIR(inode.i_mode) && !LINUX_S_ISREG(inode.i_mode) &&
	    !LINUX_S_ISCHR(inode.i_mode) && !LINUX_S_ISBLK(inode.i_mode) &&
	    !LINUX_S_ISLNK(inode.i_mode) && !LINUX_S_ISFIFO(inode.i_mode) &&
	    !(LINUX_S_ISSOCK(inode.i_mode)))
		problem = PR_2_BAD_MODE;
	else if (LINUX_S_ISCHR(inode.i_mode)
		 && !e2fsck_pass1_check_device_inode(fs, &inode))
		problem = PR_2_BAD_CHAR_DEV;
	else if (LINUX_S_ISBLK(inode.i_mode)
		 && !e2fsck_pass1_check_device_inode(fs, &inode))
		problem = PR_2_BAD_BLOCK_DEV;
	else if (LINUX_S_ISFIFO(inode.i_mode)
		 && !e2fsck_pass1_check_device_inode(fs, &inode))
		problem = PR_2_BAD_FIFO;
	else if (LINUX_S_ISSOCK(inode.i_mode)
		 && !e2fsck_pass1_check_device_inode(fs, &inode))
		problem = PR_2_BAD_SOCKET;
	else if (LINUX_S_ISLNK(inode.i_mode)
		 && !e2fsck_pass1_check_symlink(fs, &inode, buf)) {
		problem = PR_2_INVALID_SYMLINK;
	}

	if (problem) {
		if (fix_problem(ctx, problem, &pctx)) {
			deallocate_inode(ctx, ino, 0);
			if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
				return 0;
			return 1;
		} else
			not_fixed++;
		problem = 0;
	}

	if (inode.i_faddr) {
		if (fix_problem(ctx, PR_2_FADDR_ZERO, &pctx)) {
			inode.i_faddr = 0;
			inode_modified++;
		} else
			not_fixed++;
	}

	switch (fs->super->s_creator_os) {
	    case EXT2_OS_LINUX:
		frag = &inode.osd2.linux2.l_i_frag;
		fsize = &inode.osd2.linux2.l_i_fsize;
		break;
	    case EXT2_OS_HURD:
		frag = &inode.osd2.hurd2.h_i_frag;
		fsize = &inode.osd2.hurd2.h_i_fsize;
		break;
	    case EXT2_OS_MASIX:
		frag = &inode.osd2.masix2.m_i_frag;
		fsize = &inode.osd2.masix2.m_i_fsize;
		break;
	    default:
		frag = fsize = 0;
	}
	if (frag && *frag) {
		pctx.num = *frag;
		if (fix_problem(ctx, PR_2_FRAG_ZERO, &pctx)) {
			*frag = 0;
			inode_modified++;
		} else
			not_fixed++;
		pctx.num = 0;
	}
	if (fsize && *fsize) {
		pctx.num = *fsize;
		if (fix_problem(ctx, PR_2_FSIZE_ZERO, &pctx)) {
			*fsize = 0;
			inode_modified++;
		} else
			not_fixed++;
		pctx.num = 0;
	}

	if (inode.i_file_acl &&
	    ((inode.i_file_acl < fs->super->s_first_data_block) ||
	     (inode.i_file_acl >= fs->super->s_blocks_count))) {
		if (fix_problem(ctx, PR_2_FILE_ACL_BAD, &pctx)) {
			inode.i_file_acl = 0;
			inode_modified++;
		} else
			not_fixed++;
	}
	if (inode.i_dir_acl &&
	    LINUX_S_ISDIR(inode.i_mode)) {
		if (fix_problem(ctx, PR_2_DIR_ACL_ZERO, &pctx)) {
			inode.i_dir_acl = 0;
			inode_modified++;
		} else
			not_fixed++;
	}

	if (inode_modified)
		e2fsck_write_inode(ctx, ino, &inode, "process_bad_inode");
	if (!not_fixed)
		ext2fs_unmark_inode_bitmap(ctx->inode_bad_map, ino);
	return 0;
}


/*
 * allocate_dir_block --- this function allocates a new directory
 *      block for a particular inode; this is done if a directory has
 *      a "hole" in it, or if a directory has a illegal block number
 *      that was zeroed out and now needs to be replaced.
 */
static int allocate_dir_block(e2fsck_t ctx, struct ext2_db_entry *db,
			      struct problem_context *pctx)
{
	ext2_filsys fs = ctx->fs;
	blk_t                   blk;
	char                    *block;
	struct ext2_inode       inode;

	if (fix_problem(ctx, PR_2_DIRECTORY_HOLE, pctx) == 0)
		return 1;

	/*
	 * Read the inode and block bitmaps in; we'll be messing with
	 * them.
	 */
	e2fsck_read_bitmaps(ctx);

	/*
	 * First, find a free block
	 */
	pctx->errcode = ext2fs_new_block(fs, 0, ctx->block_found_map, &blk);
	if (pctx->errcode) {
		pctx->str = "ext2fs_new_block";
		fix_problem(ctx, PR_2_ALLOC_DIRBOCK, pctx);
		return 1;
	}
	ext2fs_mark_block_bitmap(ctx->block_found_map, blk);
	ext2fs_mark_block_bitmap(fs->block_map, blk);
	ext2fs_mark_bb_dirty(fs);

	/*
	 * Now let's create the actual data block for the inode
	 */
	if (db->blockcnt)
		pctx->errcode = ext2fs_new_dir_block(fs, 0, 0, &block);
	else
		pctx->errcode = ext2fs_new_dir_block(fs, db->ino,
						     EXT2_ROOT_INO, &block);

	if (pctx->errcode) {
		pctx->str = "ext2fs_new_dir_block";
		fix_problem(ctx, PR_2_ALLOC_DIRBOCK, pctx);
		return 1;
	}

	pctx->errcode = ext2fs_write_dir_block(fs, blk, block);
	ext2fs_free_mem(&block);
	if (pctx->errcode) {
		pctx->str = "ext2fs_write_dir_block";
		fix_problem(ctx, PR_2_ALLOC_DIRBOCK, pctx);
		return 1;
	}

	/*
	 * Update the inode block count
	 */
	e2fsck_read_inode(ctx, db->ino, &inode, "allocate_dir_block");
	inode.i_blocks += fs->blocksize / 512;
	if (inode.i_size < (db->blockcnt+1) * fs->blocksize)
		inode.i_size = (db->blockcnt+1) * fs->blocksize;
	e2fsck_write_inode(ctx, db->ino, &inode, "allocate_dir_block");

	/*
	 * Finally, update the block pointers for the inode
	 */
	db->blk = blk;
	pctx->errcode = ext2fs_block_iterate2(fs, db->ino, BLOCK_FLAG_HOLE,
				      0, update_dir_block, db);
	if (pctx->errcode) {
		pctx->str = "ext2fs_block_iterate";
		fix_problem(ctx, PR_2_ALLOC_DIRBOCK, pctx);
		return 1;
	}

	return 0;
}

/*
 * This is a helper function for allocate_dir_block().
 */
static int update_dir_block(ext2_filsys fs FSCK_ATTR((unused)),
			    blk_t       *block_nr,
			    e2_blkcnt_t blockcnt,
			    blk_t ref_block FSCK_ATTR((unused)),
			    int ref_offset FSCK_ATTR((unused)),
			    void *priv_data)
{
	struct ext2_db_entry *db;

	db = (struct ext2_db_entry *) priv_data;
	if (db->blockcnt == (int) blockcnt) {
		*block_nr = db->blk;
		return BLOCK_CHANGED;
	}
	return 0;
}

/*
 * pass3.c -- pass #3 of e2fsck: Check for directory connectivity
 *
 * Pass #3 assures that all directories are connected to the
 * filesystem tree, using the following algorithm:
 *
 * First, the root directory is checked to make sure it exists; if
 * not, e2fsck will offer to create a new one.  It is then marked as
 * "done".
 *
 * Then, pass3 interates over all directory inodes; for each directory
 * it attempts to trace up the filesystem tree, using dirinfo.parent
 * until it reaches a directory which has been marked "done".  If it
 * cannot do so, then the directory must be disconnected, and e2fsck
 * will offer to reconnect it to /lost+found.  While it is chasing
 * parent pointers up the filesystem tree, if pass3 sees a directory
 * twice, then it has detected a filesystem loop, and it will again
 * offer to reconnect the directory to /lost+found in to break the
 * filesystem loop.
 *
 * Pass 3 also contains the subroutine, e2fsck_reconnect_file() to
 * reconnect inodes to /lost+found; this subroutine is also used by
 * pass 4.  e2fsck_reconnect_file() calls get_lost_and_found(), which
 * is responsible for creating /lost+found if it does not exist.
 *
 * Pass 3 frees the following data structures:
 *      - The dirinfo directory information cache.
 */

static void check_root(e2fsck_t ctx);
static int check_directory(e2fsck_t ctx, struct dir_info *dir,
			   struct problem_context *pctx);
static void fix_dotdot(e2fsck_t ctx, struct dir_info *dir, ext2_ino_t parent);

static ext2fs_inode_bitmap inode_loop_detect;
static ext2fs_inode_bitmap inode_done_map;

static void e2fsck_pass3(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	int             i;
	struct problem_context  pctx;
	struct dir_info *dir;
	unsigned long maxdirs, count;

	clear_problem_context(&pctx);

	/* Pass 3 */

	if (!(ctx->options & E2F_OPT_PREEN))
		fix_problem(ctx, PR_3_PASS_HEADER, &pctx);

	/*
	 * Allocate some bitmaps to do loop detection.
	 */
	pctx.errcode = ext2fs_allocate_inode_bitmap(fs, _("inode done bitmap"),
						    &inode_done_map);
	if (pctx.errcode) {
		pctx.num = 2;
		fix_problem(ctx, PR_3_ALLOCATE_IBITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		goto abort_exit;
	}
	check_root(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		goto abort_exit;

	ext2fs_mark_inode_bitmap(inode_done_map, EXT2_ROOT_INO);

	maxdirs = e2fsck_get_num_dirinfo(ctx);
	count = 1;

	if (ctx->progress)
		if ((ctx->progress)(ctx, 3, 0, maxdirs))
			goto abort_exit;

	for (i=0; (dir = e2fsck_dir_info_iter(ctx, &i)) != 0;) {
		if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
			goto abort_exit;
		if (ctx->progress && (ctx->progress)(ctx, 3, count++, maxdirs))
			goto abort_exit;
		if (ext2fs_test_inode_bitmap(ctx->inode_dir_map, dir->ino))
			if (check_directory(ctx, dir, &pctx))
				goto abort_exit;
	}

	/*
	 * Force the creation of /lost+found if not present
	 */
	if ((ctx->flags & E2F_OPT_READONLY) == 0)
		e2fsck_get_lost_and_found(ctx, 1);

	/*
	 * If there are any directories that need to be indexed or
	 * optimized, do it here.
	 */
	e2fsck_rehash_directories(ctx);

abort_exit:
	e2fsck_free_dir_info(ctx);
	ext2fs_free_inode_bitmap(inode_loop_detect);
	inode_loop_detect = 0;
	ext2fs_free_inode_bitmap(inode_done_map);
	inode_done_map = 0;
}

/*
 * This makes sure the root inode is present; if not, we ask if the
 * user wants us to create it.  Not creating it is a fatal error.
 */
static void check_root(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	blk_t                   blk;
	struct ext2_inode       inode;
	char *                  block;
	struct problem_context  pctx;

	clear_problem_context(&pctx);

	if (ext2fs_test_inode_bitmap(ctx->inode_used_map, EXT2_ROOT_INO)) {
		/*
		 * If the root inode is not a directory, die here.  The
		 * user must have answered 'no' in pass1 when we
		 * offered to clear it.
		 */
		if (!(ext2fs_test_inode_bitmap(ctx->inode_dir_map,
					       EXT2_ROOT_INO))) {
			fix_problem(ctx, PR_3_ROOT_NOT_DIR_ABORT, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
		}
		return;
	}

	if (!fix_problem(ctx, PR_3_NO_ROOT_INODE, &pctx)) {
		fix_problem(ctx, PR_3_NO_ROOT_INODE_ABORT, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

	e2fsck_read_bitmaps(ctx);

	/*
	 * First, find a free block
	 */
	pctx.errcode = ext2fs_new_block(fs, 0, ctx->block_found_map, &blk);
	if (pctx.errcode) {
		pctx.str = "ext2fs_new_block";
		fix_problem(ctx, PR_3_CREATE_ROOT_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	ext2fs_mark_block_bitmap(ctx->block_found_map, blk);
	ext2fs_mark_block_bitmap(fs->block_map, blk);
	ext2fs_mark_bb_dirty(fs);

	/*
	 * Now let's create the actual data block for the inode
	 */
	pctx.errcode = ext2fs_new_dir_block(fs, EXT2_ROOT_INO, EXT2_ROOT_INO,
					    &block);
	if (pctx.errcode) {
		pctx.str = "ext2fs_new_dir_block";
		fix_problem(ctx, PR_3_CREATE_ROOT_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

	pctx.errcode = ext2fs_write_dir_block(fs, blk, block);
	if (pctx.errcode) {
		pctx.str = "ext2fs_write_dir_block";
		fix_problem(ctx, PR_3_CREATE_ROOT_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	ext2fs_free_mem(&block);

	/*
	 * Set up the inode structure
	 */
	memset(&inode, 0, sizeof(inode));
	inode.i_mode = 040755;
	inode.i_size = fs->blocksize;
	inode.i_atime = inode.i_ctime = inode.i_mtime = time(NULL);
	inode.i_links_count = 2;
	inode.i_blocks = fs->blocksize / 512;
	inode.i_block[0] = blk;

	/*
	 * Write out the inode.
	 */
	pctx.errcode = ext2fs_write_new_inode(fs, EXT2_ROOT_INO, &inode);
	if (pctx.errcode) {
		pctx.str = "ext2fs_write_inode";
		fix_problem(ctx, PR_3_CREATE_ROOT_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

	/*
	 * Miscellaneous bookkeeping...
	 */
	e2fsck_add_dir_info(ctx, EXT2_ROOT_INO, EXT2_ROOT_INO);
	ext2fs_icount_store(ctx->inode_count, EXT2_ROOT_INO, 2);
	ext2fs_icount_store(ctx->inode_link_info, EXT2_ROOT_INO, 2);

	ext2fs_mark_inode_bitmap(ctx->inode_used_map, EXT2_ROOT_INO);
	ext2fs_mark_inode_bitmap(ctx->inode_dir_map, EXT2_ROOT_INO);
	ext2fs_mark_inode_bitmap(fs->inode_map, EXT2_ROOT_INO);
	ext2fs_mark_ib_dirty(fs);
}

/*
 * This subroutine is responsible for making sure that a particular
 * directory is connected to the root; if it isn't we trace it up as
 * far as we can go, and then offer to connect the resulting parent to
 * the lost+found.  We have to do loop detection; if we ever discover
 * a loop, we treat that as a disconnected directory and offer to
 * reparent it to lost+found.
 *
 * However, loop detection is expensive, because for very large
 * filesystems, the inode_loop_detect bitmap is huge, and clearing it
 * is non-trivial.  Loops in filesystems are also a rare error case,
 * and we shouldn't optimize for error cases.  So we try two passes of
 * the algorithm.  The first time, we ignore loop detection and merely
 * increment a counter; if the counter exceeds some extreme threshold,
 * then we try again with the loop detection bitmap enabled.
 */
static int check_directory(e2fsck_t ctx, struct dir_info *dir,
			   struct problem_context *pctx)
{
	ext2_filsys     fs = ctx->fs;
	struct dir_info *p = dir;
	int             loop_pass = 0, parent_count = 0;

	if (!p)
		return 0;

	while (1) {
		/*
		 * Mark this inode as being "done"; by the time we
		 * return from this function, the inode we either be
		 * verified as being connected to the directory tree,
		 * or we will have offered to reconnect this to
		 * lost+found.
		 *
		 * If it was marked done already, then we've reached a
		 * parent we've already checked.
		 */
		if (ext2fs_mark_inode_bitmap(inode_done_map, p->ino))
			break;

		/*
		 * If this directory doesn't have a parent, or we've
		 * seen the parent once already, then offer to
		 * reparent it to lost+found
		 */
		if (!p->parent ||
		    (loop_pass &&
		     (ext2fs_test_inode_bitmap(inode_loop_detect,
					      p->parent)))) {
			pctx->ino = p->ino;
			if (fix_problem(ctx, PR_3_UNCONNECTED_DIR, pctx)) {
				if (e2fsck_reconnect_file(ctx, pctx->ino))
					ext2fs_unmark_valid(fs);
				else {
					p = e2fsck_get_dir_info(ctx, pctx->ino);
					p->parent = ctx->lost_and_found;
					fix_dotdot(ctx, p, ctx->lost_and_found);
				}
			}
			break;
		}
		p = e2fsck_get_dir_info(ctx, p->parent);
		if (!p) {
			fix_problem(ctx, PR_3_NO_DIRINFO, pctx);
			return 0;
		}
		if (loop_pass) {
			ext2fs_mark_inode_bitmap(inode_loop_detect,
						 p->ino);
		} else if (parent_count++ > 2048) {
			/*
			 * If we've run into a path depth that's
			 * greater than 2048, try again with the inode
			 * loop bitmap turned on and start from the
			 * top.
			 */
			loop_pass = 1;
			if (inode_loop_detect)
				ext2fs_clear_inode_bitmap(inode_loop_detect);
			else {
				pctx->errcode = ext2fs_allocate_inode_bitmap(fs, _("inode loop detection bitmap"), &inode_loop_detect);
				if (pctx->errcode) {
					pctx->num = 1;
					fix_problem(ctx,
				    PR_3_ALLOCATE_IBITMAP_ERROR, pctx);
					ctx->flags |= E2F_FLAG_ABORT;
					return -1;
				}
			}
			p = dir;
		}
	}

	/*
	 * Make sure that .. and the parent directory are the same;
	 * offer to fix it if not.
	 */
	if (dir->parent != dir->dotdot) {
		pctx->ino = dir->ino;
		pctx->ino2 = dir->dotdot;
		pctx->dir = dir->parent;
		if (fix_problem(ctx, PR_3_BAD_DOT_DOT, pctx))
			fix_dotdot(ctx, dir, dir->parent);
	}
	return 0;
}

/*
 * This routine gets the lost_and_found inode, making it a directory
 * if necessary
 */
ext2_ino_t e2fsck_get_lost_and_found(e2fsck_t ctx, int fix)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t                      ino;
	blk_t                   blk;
	errcode_t               retval;
	struct ext2_inode       inode;
	char *                  block;
	static const char       name[] = "lost+found";
	struct  problem_context pctx;
	struct dir_info         *dirinfo;

	if (ctx->lost_and_found)
		return ctx->lost_and_found;

	clear_problem_context(&pctx);

	retval = ext2fs_lookup(fs, EXT2_ROOT_INO, name,
			       sizeof(name)-1, 0, &ino);
	if (retval && !fix)
		return 0;
	if (!retval) {
		if (ext2fs_test_inode_bitmap(ctx->inode_dir_map, ino)) {
			ctx->lost_and_found = ino;
			return ino;
		}

		/* Lost+found isn't a directory! */
		if (!fix)
			return 0;
		pctx.ino = ino;
		if (!fix_problem(ctx, PR_3_LPF_NOTDIR, &pctx))
			return 0;

		/* OK, unlink the old /lost+found file. */
		pctx.errcode = ext2fs_unlink(fs, EXT2_ROOT_INO, name, ino, 0);
		if (pctx.errcode) {
			pctx.str = "ext2fs_unlink";
			fix_problem(ctx, PR_3_CREATE_LPF_ERROR, &pctx);
			return 0;
		}
		dirinfo = e2fsck_get_dir_info(ctx, ino);
		if (dirinfo)
			dirinfo->parent = 0;
		e2fsck_adjust_inode_count(ctx, ino, -1);
	} else if (retval != EXT2_ET_FILE_NOT_FOUND) {
		pctx.errcode = retval;
		fix_problem(ctx, PR_3_ERR_FIND_LPF, &pctx);
	}
	if (!fix_problem(ctx, PR_3_NO_LF_DIR, 0))
		return 0;

	/*
	 * Read the inode and block bitmaps in; we'll be messing with
	 * them.
	 */
	e2fsck_read_bitmaps(ctx);

	/*
	 * First, find a free block
	 */
	retval = ext2fs_new_block(fs, 0, ctx->block_found_map, &blk);
	if (retval) {
		pctx.errcode = retval;
		fix_problem(ctx, PR_3_ERR_LPF_NEW_BLOCK, &pctx);
		return 0;
	}
	ext2fs_mark_block_bitmap(ctx->block_found_map, blk);
	ext2fs_block_alloc_stats(fs, blk, +1);

	/*
	 * Next find a free inode.
	 */
	retval = ext2fs_new_inode(fs, EXT2_ROOT_INO, 040700,
				  ctx->inode_used_map, &ino);
	if (retval) {
		pctx.errcode = retval;
		fix_problem(ctx, PR_3_ERR_LPF_NEW_INODE, &pctx);
		return 0;
	}
	ext2fs_mark_inode_bitmap(ctx->inode_used_map, ino);
	ext2fs_mark_inode_bitmap(ctx->inode_dir_map, ino);
	ext2fs_inode_alloc_stats2(fs, ino, +1, 1);

	/*
	 * Now let's create the actual data block for the inode
	 */
	retval = ext2fs_new_dir_block(fs, ino, EXT2_ROOT_INO, &block);
	if (retval) {
		pctx.errcode = retval;
		fix_problem(ctx, PR_3_ERR_LPF_NEW_DIR_BLOCK, &pctx);
		return 0;
	}

	retval = ext2fs_write_dir_block(fs, blk, block);
	ext2fs_free_mem(&block);
	if (retval) {
		pctx.errcode = retval;
		fix_problem(ctx, PR_3_ERR_LPF_WRITE_BLOCK, &pctx);
		return 0;
	}

	/*
	 * Set up the inode structure
	 */
	memset(&inode, 0, sizeof(inode));
	inode.i_mode = 040700;
	inode.i_size = fs->blocksize;
	inode.i_atime = inode.i_ctime = inode.i_mtime = time(NULL);
	inode.i_links_count = 2;
	inode.i_blocks = fs->blocksize / 512;
	inode.i_block[0] = blk;

	/*
	 * Next, write out the inode.
	 */
	pctx.errcode = ext2fs_write_new_inode(fs, ino, &inode);
	if (pctx.errcode) {
		pctx.str = "ext2fs_write_inode";
		fix_problem(ctx, PR_3_CREATE_LPF_ERROR, &pctx);
		return 0;
	}
	/*
	 * Finally, create the directory link
	 */
	pctx.errcode = ext2fs_link(fs, EXT2_ROOT_INO, name, ino, EXT2_FT_DIR);
	if (pctx.errcode) {
		pctx.str = "ext2fs_link";
		fix_problem(ctx, PR_3_CREATE_LPF_ERROR, &pctx);
		return 0;
	}

	/*
	 * Miscellaneous bookkeeping that needs to be kept straight.
	 */
	e2fsck_add_dir_info(ctx, ino, EXT2_ROOT_INO);
	e2fsck_adjust_inode_count(ctx, EXT2_ROOT_INO, 1);
	ext2fs_icount_store(ctx->inode_count, ino, 2);
	ext2fs_icount_store(ctx->inode_link_info, ino, 2);
	ctx->lost_and_found = ino;
	return ino;
}

/*
 * This routine will connect a file to lost+found
 */
int e2fsck_reconnect_file(e2fsck_t ctx, ext2_ino_t ino)
{
	ext2_filsys fs = ctx->fs;
	errcode_t       retval;
	char            name[80];
	struct problem_context  pctx;
	struct ext2_inode       inode;
	int             file_type = 0;

	clear_problem_context(&pctx);
	pctx.ino = ino;

	if (!ctx->bad_lost_and_found && !ctx->lost_and_found) {
		if (e2fsck_get_lost_and_found(ctx, 1) == 0)
			ctx->bad_lost_and_found++;
	}
	if (ctx->bad_lost_and_found) {
		fix_problem(ctx, PR_3_NO_LPF, &pctx);
		return 1;
	}

	sprintf(name, "#%u", ino);
	if (ext2fs_read_inode(fs, ino, &inode) == 0)
		file_type = ext2_file_type(inode.i_mode);
	retval = ext2fs_link(fs, ctx->lost_and_found, name, ino, file_type);
	if (retval == EXT2_ET_DIR_NO_SPACE) {
		if (!fix_problem(ctx, PR_3_EXPAND_LF_DIR, &pctx))
			return 1;
		retval = e2fsck_expand_directory(ctx, ctx->lost_and_found,
						 1, 0);
		if (retval) {
			pctx.errcode = retval;
			fix_problem(ctx, PR_3_CANT_EXPAND_LPF, &pctx);
			return 1;
		}
		retval = ext2fs_link(fs, ctx->lost_and_found, name,
				     ino, file_type);
	}
	if (retval) {
		pctx.errcode = retval;
		fix_problem(ctx, PR_3_CANT_RECONNECT, &pctx);
		return 1;
	}
	e2fsck_adjust_inode_count(ctx, ino, 1);

	return 0;
}

/*
 * Utility routine to adjust the inode counts on an inode.
 */
errcode_t e2fsck_adjust_inode_count(e2fsck_t ctx, ext2_ino_t ino, int adj)
{
	ext2_filsys fs = ctx->fs;
	errcode_t               retval;
	struct ext2_inode       inode;

	if (!ino)
		return 0;

	retval = ext2fs_read_inode(fs, ino, &inode);
	if (retval)
		return retval;

	if (adj == 1) {
		ext2fs_icount_increment(ctx->inode_count, ino, 0);
		if (inode.i_links_count == (__u16) ~0)
			return 0;
		ext2fs_icount_increment(ctx->inode_link_info, ino, 0);
		inode.i_links_count++;
	} else if (adj == -1) {
		ext2fs_icount_decrement(ctx->inode_count, ino, 0);
		if (inode.i_links_count == 0)
			return 0;
		ext2fs_icount_decrement(ctx->inode_link_info, ino, 0);
		inode.i_links_count--;
	}

	retval = ext2fs_write_inode(fs, ino, &inode);
	if (retval)
		return retval;

	return 0;
}

/*
 * Fix parent --- this routine fixes up the parent of a directory.
 */
struct fix_dotdot_struct {
	ext2_filsys     fs;
	ext2_ino_t      parent;
	int             done;
	e2fsck_t        ctx;
};

static int fix_dotdot_proc(struct ext2_dir_entry *dirent,
			   int  offset FSCK_ATTR((unused)),
			   int  blocksize FSCK_ATTR((unused)),
			   char *buf FSCK_ATTR((unused)),
			   void *priv_data)
{
	struct fix_dotdot_struct *fp = (struct fix_dotdot_struct *) priv_data;
	errcode_t       retval;
	struct problem_context pctx;

	if ((dirent->name_len & 0xFF) != 2)
		return 0;
	if (strncmp(dirent->name, "..", 2))
		return 0;

	clear_problem_context(&pctx);

	retval = e2fsck_adjust_inode_count(fp->ctx, dirent->inode, -1);
	if (retval) {
		pctx.errcode = retval;
		fix_problem(fp->ctx, PR_3_ADJUST_INODE, &pctx);
	}
	retval = e2fsck_adjust_inode_count(fp->ctx, fp->parent, 1);
	if (retval) {
		pctx.errcode = retval;
		fix_problem(fp->ctx, PR_3_ADJUST_INODE, &pctx);
	}
	dirent->inode = fp->parent;

	fp->done++;
	return DIRENT_ABORT | DIRENT_CHANGED;
}

static void fix_dotdot(e2fsck_t ctx, struct dir_info *dir, ext2_ino_t parent)
{
	ext2_filsys fs = ctx->fs;
	errcode_t       retval;
	struct fix_dotdot_struct fp;
	struct problem_context pctx;

	fp.fs = fs;
	fp.parent = parent;
	fp.done = 0;
	fp.ctx = ctx;

	retval = ext2fs_dir_iterate(fs, dir->ino, DIRENT_FLAG_INCLUDE_EMPTY,
				    0, fix_dotdot_proc, &fp);
	if (retval || !fp.done) {
		clear_problem_context(&pctx);
		pctx.ino = dir->ino;
		pctx.errcode = retval;
		fix_problem(ctx, retval ? PR_3_FIX_PARENT_ERR :
			    PR_3_FIX_PARENT_NOFIND, &pctx);
		ext2fs_unmark_valid(fs);
	}
	dir->dotdot = parent;
}

/*
 * These routines are responsible for expanding a /lost+found if it is
 * too small.
 */

struct expand_dir_struct {
	int                     num;
	int                     guaranteed_size;
	int                     newblocks;
	int                     last_block;
	errcode_t               err;
	e2fsck_t                ctx;
};

static int expand_dir_proc(ext2_filsys fs,
			   blk_t        *blocknr,
			   e2_blkcnt_t  blockcnt,
			   blk_t ref_block FSCK_ATTR((unused)),
			   int ref_offset FSCK_ATTR((unused)),
			   void *priv_data)
{
	struct expand_dir_struct *es = (struct expand_dir_struct *) priv_data;
	blk_t   new_blk;
	static blk_t    last_blk = 0;
	char            *block;
	errcode_t       retval;
	e2fsck_t        ctx;

	ctx = es->ctx;

	if (es->guaranteed_size && blockcnt >= es->guaranteed_size)
		return BLOCK_ABORT;

	if (blockcnt > 0)
		es->last_block = blockcnt;
	if (*blocknr) {
		last_blk = *blocknr;
		return 0;
	}
	retval = ext2fs_new_block(fs, last_blk, ctx->block_found_map,
				  &new_blk);
	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	if (blockcnt > 0) {
		retval = ext2fs_new_dir_block(fs, 0, 0, &block);
		if (retval) {
			es->err = retval;
			return BLOCK_ABORT;
		}
		es->num--;
		retval = ext2fs_write_dir_block(fs, new_blk, block);
	} else {
		retval = ext2fs_get_mem(fs->blocksize, &block);
		if (retval) {
			es->err = retval;
			return BLOCK_ABORT;
		}
		memset(block, 0, fs->blocksize);
		retval = io_channel_write_blk(fs->io, new_blk, 1, block);
	}
	if (retval) {
		es->err = retval;
		return BLOCK_ABORT;
	}
	ext2fs_free_mem(&block);
	*blocknr = new_blk;
	ext2fs_mark_block_bitmap(ctx->block_found_map, new_blk);
	ext2fs_block_alloc_stats(fs, new_blk, +1);
	es->newblocks++;

	if (es->num == 0)
		return (BLOCK_CHANGED | BLOCK_ABORT);
	else
		return BLOCK_CHANGED;
}

errcode_t e2fsck_expand_directory(e2fsck_t ctx, ext2_ino_t dir,
				  int num, int guaranteed_size)
{
	ext2_filsys fs = ctx->fs;
	errcode_t       retval;
	struct expand_dir_struct es;
	struct ext2_inode       inode;

	if (!(fs->flags & EXT2_FLAG_RW))
		return EXT2_ET_RO_FILSYS;

	/*
	 * Read the inode and block bitmaps in; we'll be messing with
	 * them.
	 */
	e2fsck_read_bitmaps(ctx);

	retval = ext2fs_check_directory(fs, dir);
	if (retval)
		return retval;

	es.num = num;
	es.guaranteed_size = guaranteed_size;
	es.last_block = 0;
	es.err = 0;
	es.newblocks = 0;
	es.ctx = ctx;

	retval = ext2fs_block_iterate2(fs, dir, BLOCK_FLAG_APPEND,
				       0, expand_dir_proc, &es);

	if (es.err)
		return es.err;

	/*
	 * Update the size and block count fields in the inode.
	 */
	retval = ext2fs_read_inode(fs, dir, &inode);
	if (retval)
		return retval;

	inode.i_size = (es.last_block + 1) * fs->blocksize;
	inode.i_blocks += (fs->blocksize / 512) * es.newblocks;

	e2fsck_write_inode(ctx, dir, &inode, "expand_directory");

	return 0;
}

/*
 * pass4.c -- pass #4 of e2fsck: Check reference counts
 *
 * Pass 4 frees the following data structures:
 *      - A bitmap of which inodes are imagic inodes.   (inode_imagic_map)
 */

/*
 * This routine is called when an inode is not connected to the
 * directory tree.
 *
 * This subroutine returns 1 then the caller shouldn't bother with the
 * rest of the pass 4 tests.
 */
static int disconnect_inode(e2fsck_t ctx, ext2_ino_t i)
{
	ext2_filsys fs = ctx->fs;
	struct ext2_inode       inode;
	struct problem_context  pctx;

	e2fsck_read_inode(ctx, i, &inode, "pass4: disconnect_inode");
	clear_problem_context(&pctx);
	pctx.ino = i;
	pctx.inode = &inode;

	/*
	 * Offer to delete any zero-length files that does not have
	 * blocks.  If there is an EA block, it might have useful
	 * information, so we won't prompt to delete it, but let it be
	 * reconnected to lost+found.
	 */
	if (!inode.i_blocks && (LINUX_S_ISREG(inode.i_mode) ||
				LINUX_S_ISDIR(inode.i_mode))) {
		if (fix_problem(ctx, PR_4_ZERO_LEN_INODE, &pctx)) {
			ext2fs_icount_store(ctx->inode_link_info, i, 0);
			inode.i_links_count = 0;
			inode.i_dtime = time(NULL);
			e2fsck_write_inode(ctx, i, &inode,
					   "disconnect_inode");
			/*
			 * Fix up the bitmaps...
			 */
			e2fsck_read_bitmaps(ctx);
			ext2fs_unmark_inode_bitmap(ctx->inode_used_map, i);
			ext2fs_unmark_inode_bitmap(ctx->inode_dir_map, i);
			ext2fs_inode_alloc_stats2(fs, i, -1,
						  LINUX_S_ISDIR(inode.i_mode));
			return 0;
		}
	}

	/*
	 * Prompt to reconnect.
	 */
	if (fix_problem(ctx, PR_4_UNATTACHED_INODE, &pctx)) {
		if (e2fsck_reconnect_file(ctx, i))
			ext2fs_unmark_valid(fs);
	} else {
		/*
		 * If we don't attach the inode, then skip the
		 * i_links_test since there's no point in trying to
		 * force i_links_count to zero.
		 */
		ext2fs_unmark_valid(fs);
		return 1;
	}
	return 0;
}


static void e2fsck_pass4(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t      i;
	struct ext2_inode       inode;
	struct problem_context  pctx;
	__u16   link_count, link_counted;
	char    *buf = NULL;
	int     group, maxgroup;

	/* Pass 4 */

	clear_problem_context(&pctx);

	if (!(ctx->options & E2F_OPT_PREEN))
		fix_problem(ctx, PR_4_PASS_HEADER, &pctx);

	group = 0;
	maxgroup = fs->group_desc_count;
	if (ctx->progress)
		if ((ctx->progress)(ctx, 4, 0, maxgroup))
			return;

	for (i=1; i <= fs->super->s_inodes_count; i++) {
		if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
			return;
		if ((i % fs->super->s_inodes_per_group) == 0) {
			group++;
			if (ctx->progress)
				if ((ctx->progress)(ctx, 4, group, maxgroup))
					return;
		}
		if (i == EXT2_BAD_INO ||
		    (i > EXT2_ROOT_INO && i < EXT2_FIRST_INODE(fs->super)))
			continue;
		if (!(ext2fs_test_inode_bitmap(ctx->inode_used_map, i)) ||
		    (ctx->inode_imagic_map &&
		     ext2fs_test_inode_bitmap(ctx->inode_imagic_map, i)))
			continue;
		ext2fs_icount_fetch(ctx->inode_link_info, i, &link_count);
		ext2fs_icount_fetch(ctx->inode_count, i, &link_counted);
		if (link_counted == 0) {
			if (!buf)
				buf = e2fsck_allocate_memory(ctx,
				     fs->blocksize, "bad_inode buffer");
			if (e2fsck_process_bad_inode(ctx, 0, i, buf))
				continue;
			if (disconnect_inode(ctx, i))
				continue;
			ext2fs_icount_fetch(ctx->inode_link_info, i,
					    &link_count);
			ext2fs_icount_fetch(ctx->inode_count, i,
					    &link_counted);
		}
		if (link_counted != link_count) {
			e2fsck_read_inode(ctx, i, &inode, "pass4");
			pctx.ino = i;
			pctx.inode = &inode;
			if (link_count != inode.i_links_count) {
				pctx.num = link_count;
				fix_problem(ctx,
					    PR_4_INCONSISTENT_COUNT, &pctx);
			}
			pctx.num = link_counted;
			if (fix_problem(ctx, PR_4_BAD_REF_COUNT, &pctx)) {
				inode.i_links_count = link_counted;
				e2fsck_write_inode(ctx, i, &inode, "pass4");
			}
		}
	}
	ext2fs_free_icount(ctx->inode_link_info); ctx->inode_link_info = 0;
	ext2fs_free_icount(ctx->inode_count); ctx->inode_count = 0;
	ext2fs_free_inode_bitmap(ctx->inode_imagic_map);
	ctx->inode_imagic_map = 0;
	ext2fs_free_mem(&buf);
}

/*
 * pass5.c --- check block and inode bitmaps against on-disk bitmaps
 */

#define NO_BLK ((blk_t) -1)

static void print_bitmap_problem(e2fsck_t ctx, int problem,
			    struct problem_context *pctx)
{
	switch (problem) {
	case PR_5_BLOCK_UNUSED:
		if (pctx->blk == pctx->blk2)
			pctx->blk2 = 0;
		else
			problem = PR_5_BLOCK_RANGE_UNUSED;
		break;
	case PR_5_BLOCK_USED:
		if (pctx->blk == pctx->blk2)
			pctx->blk2 = 0;
		else
			problem = PR_5_BLOCK_RANGE_USED;
		break;
	case PR_5_INODE_UNUSED:
		if (pctx->ino == pctx->ino2)
			pctx->ino2 = 0;
		else
			problem = PR_5_INODE_RANGE_UNUSED;
		break;
	case PR_5_INODE_USED:
		if (pctx->ino == pctx->ino2)
			pctx->ino2 = 0;
		else
			problem = PR_5_INODE_RANGE_USED;
		break;
	}
	fix_problem(ctx, problem, pctx);
	pctx->blk = pctx->blk2 = NO_BLK;
	pctx->ino = pctx->ino2 = 0;
}

static void check_block_bitmaps(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	blk_t   i;
	int     *free_array;
	int     group = 0;
	unsigned int    blocks = 0;
	unsigned int    free_blocks = 0;
	int     group_free = 0;
	int     actual, bitmap;
	struct problem_context  pctx;
	int     problem, save_problem, fixit, had_problem;
	errcode_t       retval;

	clear_problem_context(&pctx);
	free_array = (int *) e2fsck_allocate_memory(ctx,
	    fs->group_desc_count * sizeof(int), "free block count array");

	if ((fs->super->s_first_data_block <
	     ext2fs_get_block_bitmap_start(ctx->block_found_map)) ||
	    (fs->super->s_blocks_count-1 >
	     ext2fs_get_block_bitmap_end(ctx->block_found_map))) {
		pctx.num = 1;
		pctx.blk = fs->super->s_first_data_block;
		pctx.blk2 = fs->super->s_blocks_count -1;
		pctx.ino = ext2fs_get_block_bitmap_start(ctx->block_found_map);
		pctx.ino2 = ext2fs_get_block_bitmap_end(ctx->block_found_map);
		fix_problem(ctx, PR_5_BMAP_ENDPOINTS, &pctx);

		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}

	if ((fs->super->s_first_data_block <
	     ext2fs_get_block_bitmap_start(fs->block_map)) ||
	    (fs->super->s_blocks_count-1 >
	     ext2fs_get_block_bitmap_end(fs->block_map))) {
		pctx.num = 2;
		pctx.blk = fs->super->s_first_data_block;
		pctx.blk2 = fs->super->s_blocks_count -1;
		pctx.ino = ext2fs_get_block_bitmap_start(fs->block_map);
		pctx.ino2 = ext2fs_get_block_bitmap_end(fs->block_map);
		fix_problem(ctx, PR_5_BMAP_ENDPOINTS, &pctx);

		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}

redo_counts:
	had_problem = 0;
	save_problem = 0;
	pctx.blk = pctx.blk2 = NO_BLK;
	for (i = fs->super->s_first_data_block;
	     i < fs->super->s_blocks_count;
	     i++) {
		actual = ext2fs_fast_test_block_bitmap(ctx->block_found_map, i);
		bitmap = ext2fs_fast_test_block_bitmap(fs->block_map, i);

		if (actual == bitmap)
			goto do_counts;

		if (!actual && bitmap) {
			/*
			 * Block not used, but marked in use in the bitmap.
			 */
			problem = PR_5_BLOCK_UNUSED;
		} else {
			/*
			 * Block used, but not marked in use in the bitmap.
			 */
			problem = PR_5_BLOCK_USED;
		}
		if (pctx.blk == NO_BLK) {
			pctx.blk = pctx.blk2 = i;
			save_problem = problem;
		} else {
			if ((problem == save_problem) &&
			    (pctx.blk2 == i-1))
				pctx.blk2++;
			else {
				print_bitmap_problem(ctx, save_problem, &pctx);
				pctx.blk = pctx.blk2 = i;
				save_problem = problem;
			}
		}
		ctx->flags |= E2F_FLAG_PROG_SUPPRESS;
		had_problem++;

	do_counts:
		if (!bitmap) {
			group_free++;
			free_blocks++;
		}
		blocks ++;
		if ((blocks == fs->super->s_blocks_per_group) ||
		    (i == fs->super->s_blocks_count-1)) {
			free_array[group] = group_free;
			group ++;
			blocks = 0;
			group_free = 0;
			if (ctx->progress)
				if ((ctx->progress)(ctx, 5, group,
						    fs->group_desc_count*2))
					return;
		}
	}
	if (pctx.blk != NO_BLK)
		print_bitmap_problem(ctx, save_problem, &pctx);
	if (had_problem)
		fixit = end_problem_latch(ctx, PR_LATCH_BBITMAP);
	else
		fixit = -1;
	ctx->flags &= ~E2F_FLAG_PROG_SUPPRESS;

	if (fixit == 1) {
		ext2fs_free_block_bitmap(fs->block_map);
		retval = ext2fs_copy_bitmap(ctx->block_found_map,
						  &fs->block_map);
		if (retval) {
			clear_problem_context(&pctx);
			fix_problem(ctx, PR_5_COPY_BBITMAP_ERROR, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		ext2fs_set_bitmap_padding(fs->block_map);
		ext2fs_mark_bb_dirty(fs);

		/* Redo the counts */
		blocks = 0; free_blocks = 0; group_free = 0; group = 0;
		memset(free_array, 0, fs->group_desc_count * sizeof(int));
		goto redo_counts;
	} else if (fixit == 0)
		ext2fs_unmark_valid(fs);

	for (i = 0; i < fs->group_desc_count; i++) {
		if (free_array[i] != fs->group_desc[i].bg_free_blocks_count) {
			pctx.group = i;
			pctx.blk = fs->group_desc[i].bg_free_blocks_count;
			pctx.blk2 = free_array[i];

			if (fix_problem(ctx, PR_5_FREE_BLOCK_COUNT_GROUP,
					&pctx)) {
				fs->group_desc[i].bg_free_blocks_count =
					free_array[i];
				ext2fs_mark_super_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
		}
	}
	if (free_blocks != fs->super->s_free_blocks_count) {
		pctx.group = 0;
		pctx.blk = fs->super->s_free_blocks_count;
		pctx.blk2 = free_blocks;

		if (fix_problem(ctx, PR_5_FREE_BLOCK_COUNT, &pctx)) {
			fs->super->s_free_blocks_count = free_blocks;
			ext2fs_mark_super_dirty(fs);
		} else
			ext2fs_unmark_valid(fs);
	}
	ext2fs_free_mem(&free_array);
}

static void check_inode_bitmaps(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t      i;
	unsigned int    free_inodes = 0;
	int             group_free = 0;
	int             dirs_count = 0;
	int             group = 0;
	unsigned int    inodes = 0;
	int             *free_array;
	int             *dir_array;
	int             actual, bitmap;
	errcode_t       retval;
	struct problem_context  pctx;
	int             problem, save_problem, fixit, had_problem;

	clear_problem_context(&pctx);
	free_array = (int *) e2fsck_allocate_memory(ctx,
	    fs->group_desc_count * sizeof(int), "free inode count array");

	dir_array = (int *) e2fsck_allocate_memory(ctx,
	   fs->group_desc_count * sizeof(int), "directory count array");

	if ((1 < ext2fs_get_inode_bitmap_start(ctx->inode_used_map)) ||
	    (fs->super->s_inodes_count >
	     ext2fs_get_inode_bitmap_end(ctx->inode_used_map))) {
		pctx.num = 3;
		pctx.blk = 1;
		pctx.blk2 = fs->super->s_inodes_count;
		pctx.ino = ext2fs_get_inode_bitmap_start(ctx->inode_used_map);
		pctx.ino2 = ext2fs_get_inode_bitmap_end(ctx->inode_used_map);
		fix_problem(ctx, PR_5_BMAP_ENDPOINTS, &pctx);

		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
	if ((1 < ext2fs_get_inode_bitmap_start(fs->inode_map)) ||
	    (fs->super->s_inodes_count >
	     ext2fs_get_inode_bitmap_end(fs->inode_map))) {
		pctx.num = 4;
		pctx.blk = 1;
		pctx.blk2 = fs->super->s_inodes_count;
		pctx.ino = ext2fs_get_inode_bitmap_start(fs->inode_map);
		pctx.ino2 = ext2fs_get_inode_bitmap_end(fs->inode_map);
		fix_problem(ctx, PR_5_BMAP_ENDPOINTS, &pctx);

		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}

redo_counts:
	had_problem = 0;
	save_problem = 0;
	pctx.ino = pctx.ino2 = 0;
	for (i = 1; i <= fs->super->s_inodes_count; i++) {
		actual = ext2fs_fast_test_inode_bitmap(ctx->inode_used_map, i);
		bitmap = ext2fs_fast_test_inode_bitmap(fs->inode_map, i);

		if (actual == bitmap)
			goto do_counts;

		if (!actual && bitmap) {
			/*
			 * Inode wasn't used, but marked in bitmap
			 */
			problem = PR_5_INODE_UNUSED;
		} else /* if (actual && !bitmap) */ {
			/*
			 * Inode used, but not in bitmap
			 */
			problem = PR_5_INODE_USED;
		}
		if (pctx.ino == 0) {
			pctx.ino = pctx.ino2 = i;
			save_problem = problem;
		} else {
			if ((problem == save_problem) &&
			    (pctx.ino2 == i-1))
				pctx.ino2++;
			else {
				print_bitmap_problem(ctx, save_problem, &pctx);
				pctx.ino = pctx.ino2 = i;
				save_problem = problem;
			}
		}
		ctx->flags |= E2F_FLAG_PROG_SUPPRESS;
		had_problem++;

do_counts:
		if (!bitmap) {
			group_free++;
			free_inodes++;
		} else {
			if (ext2fs_test_inode_bitmap(ctx->inode_dir_map, i))
				dirs_count++;
		}
		inodes++;
		if ((inodes == fs->super->s_inodes_per_group) ||
		    (i == fs->super->s_inodes_count)) {
			free_array[group] = group_free;
			dir_array[group] = dirs_count;
			group ++;
			inodes = 0;
			group_free = 0;
			dirs_count = 0;
			if (ctx->progress)
				if ((ctx->progress)(ctx, 5,
					    group + fs->group_desc_count,
					    fs->group_desc_count*2))
					return;
		}
	}
	if (pctx.ino)
		print_bitmap_problem(ctx, save_problem, &pctx);

	if (had_problem)
		fixit = end_problem_latch(ctx, PR_LATCH_IBITMAP);
	else
		fixit = -1;
	ctx->flags &= ~E2F_FLAG_PROG_SUPPRESS;

	if (fixit == 1) {
		ext2fs_free_inode_bitmap(fs->inode_map);
		retval = ext2fs_copy_bitmap(ctx->inode_used_map,
						  &fs->inode_map);
		if (retval) {
			clear_problem_context(&pctx);
			fix_problem(ctx, PR_5_COPY_IBITMAP_ERROR, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		ext2fs_set_bitmap_padding(fs->inode_map);
		ext2fs_mark_ib_dirty(fs);

		/* redo counts */
		inodes = 0; free_inodes = 0; group_free = 0;
		dirs_count = 0; group = 0;
		memset(free_array, 0, fs->group_desc_count * sizeof(int));
		memset(dir_array, 0, fs->group_desc_count * sizeof(int));
		goto redo_counts;
	} else if (fixit == 0)
		ext2fs_unmark_valid(fs);

	for (i = 0; i < fs->group_desc_count; i++) {
		if (free_array[i] != fs->group_desc[i].bg_free_inodes_count) {
			pctx.group = i;
			pctx.ino = fs->group_desc[i].bg_free_inodes_count;
			pctx.ino2 = free_array[i];
			if (fix_problem(ctx, PR_5_FREE_INODE_COUNT_GROUP,
					&pctx)) {
				fs->group_desc[i].bg_free_inodes_count =
					free_array[i];
				ext2fs_mark_super_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
		}
		if (dir_array[i] != fs->group_desc[i].bg_used_dirs_count) {
			pctx.group = i;
			pctx.ino = fs->group_desc[i].bg_used_dirs_count;
			pctx.ino2 = dir_array[i];

			if (fix_problem(ctx, PR_5_FREE_DIR_COUNT_GROUP,
					&pctx)) {
				fs->group_desc[i].bg_used_dirs_count =
					dir_array[i];
				ext2fs_mark_super_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
		}
	}
	if (free_inodes != fs->super->s_free_inodes_count) {
		pctx.group = -1;
		pctx.ino = fs->super->s_free_inodes_count;
		pctx.ino2 = free_inodes;

		if (fix_problem(ctx, PR_5_FREE_INODE_COUNT, &pctx)) {
			fs->super->s_free_inodes_count = free_inodes;
			ext2fs_mark_super_dirty(fs);
		} else
			ext2fs_unmark_valid(fs);
	}
	ext2fs_free_mem(&free_array);
	ext2fs_free_mem(&dir_array);
}

static void check_inode_end(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t      end, save_inodes_count, i;
	struct problem_context  pctx;

	clear_problem_context(&pctx);

	end = EXT2_INODES_PER_GROUP(fs->super) * fs->group_desc_count;
	pctx.errcode = ext2fs_fudge_inode_bitmap_end(fs->inode_map, end,
						     &save_inodes_count);
	if (pctx.errcode) {
		pctx.num = 1;
		fix_problem(ctx, PR_5_FUDGE_BITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
	if (save_inodes_count == end)
		return;

	for (i = save_inodes_count + 1; i <= end; i++) {
		if (!ext2fs_test_inode_bitmap(fs->inode_map, i)) {
			if (fix_problem(ctx, PR_5_INODE_BMAP_PADDING, &pctx)) {
				for (i = save_inodes_count + 1; i <= end; i++)
					ext2fs_mark_inode_bitmap(fs->inode_map,
								 i);
				ext2fs_mark_ib_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
			break;
		}
	}

	pctx.errcode = ext2fs_fudge_inode_bitmap_end(fs->inode_map,
						     save_inodes_count, 0);
	if (pctx.errcode) {
		pctx.num = 2;
		fix_problem(ctx, PR_5_FUDGE_BITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
}

static void check_block_end(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	blk_t   end, save_blocks_count, i;
	struct problem_context  pctx;

	clear_problem_context(&pctx);

	end = fs->block_map->start +
		(EXT2_BLOCKS_PER_GROUP(fs->super) * fs->group_desc_count) - 1;
	pctx.errcode = ext2fs_fudge_block_bitmap_end(fs->block_map, end,
						     &save_blocks_count);
	if (pctx.errcode) {
		pctx.num = 3;
		fix_problem(ctx, PR_5_FUDGE_BITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
	if (save_blocks_count == end)
		return;

	for (i = save_blocks_count + 1; i <= end; i++) {
		if (!ext2fs_test_block_bitmap(fs->block_map, i)) {
			if (fix_problem(ctx, PR_5_BLOCK_BMAP_PADDING, &pctx)) {
				for (i = save_blocks_count + 1; i <= end; i++)
					ext2fs_mark_block_bitmap(fs->block_map,
								 i);
				ext2fs_mark_bb_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
			break;
		}
	}

	pctx.errcode = ext2fs_fudge_block_bitmap_end(fs->block_map,
						     save_blocks_count, 0);
	if (pctx.errcode) {
		pctx.num = 4;
		fix_problem(ctx, PR_5_FUDGE_BITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
}

static void e2fsck_pass5(e2fsck_t ctx)
{
	struct problem_context  pctx;

	/* Pass 5 */

	clear_problem_context(&pctx);

	if (!(ctx->options & E2F_OPT_PREEN))
		fix_problem(ctx, PR_5_PASS_HEADER, &pctx);

	if (ctx->progress)
		if ((ctx->progress)(ctx, 5, 0, ctx->fs->group_desc_count*2))
			return;

	e2fsck_read_bitmaps(ctx);

	check_block_bitmaps(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;
	check_inode_bitmaps(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;
	check_inode_end(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;
	check_block_end(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;

	ext2fs_free_inode_bitmap(ctx->inode_used_map);
	ctx->inode_used_map = 0;
	ext2fs_free_inode_bitmap(ctx->inode_dir_map);
	ctx->inode_dir_map = 0;
	ext2fs_free_block_bitmap(ctx->block_found_map);
	ctx->block_found_map = 0;
}

/*
 * problem.c --- report filesystem problems to the user
 */

#define PR_PREEN_OK     0x000001 /* Don't need to do preenhalt */
#define PR_NO_OK        0x000002 /* If user answers no, don't make fs invalid */
#define PR_NO_DEFAULT   0x000004 /* Default to no */
#define PR_MSG_ONLY     0x000008 /* Print message only */

/* Bit positions 0x000ff0 are reserved for the PR_LATCH flags */

#define PR_FATAL        0x001000 /* Fatal error */
#define PR_AFTER_CODE   0x002000 /* After asking the first question, */
				 /* ask another */
#define PR_PREEN_NOMSG  0x004000 /* Don't print a message if we're preening */
#define PR_NOCOLLATE    0x008000 /* Don't collate answers for this latch */
#define PR_NO_NOMSG     0x010000 /* Don't print a message if e2fsck -n */
#define PR_PREEN_NO     0x020000 /* Use No as an answer if preening */
#define PR_PREEN_NOHDR  0x040000 /* Don't print the preen header */


#define PROMPT_NONE     0
#define PROMPT_FIX      1
#define PROMPT_CLEAR    2
#define PROMPT_RELOCATE 3
#define PROMPT_ALLOCATE 4
#define PROMPT_EXPAND   5
#define PROMPT_CONNECT  6
#define PROMPT_CREATE   7
#define PROMPT_SALVAGE  8
#define PROMPT_TRUNCATE 9
#define PROMPT_CLEAR_INODE 10
#define PROMPT_ABORT    11
#define PROMPT_SPLIT    12
#define PROMPT_CONTINUE 13
#define PROMPT_CLONE    14
#define PROMPT_DELETE   15
#define PROMPT_SUPPRESS 16
#define PROMPT_UNLINK   17
#define PROMPT_CLEAR_HTREE 18
#define PROMPT_RECREATE 19
#define PROMPT_NULL     20

struct e2fsck_problem {
	problem_t       e2p_code;
	const char *    e2p_description;
	char            prompt;
	int             flags;
	problem_t       second_code;
};

struct latch_descr {
	int             latch_code;
	problem_t       question;
	problem_t       end_message;
	int             flags;
};

/*
 * These are the prompts which are used to ask the user if they want
 * to fix a problem.
 */
static const char *const prompt[] = {
	N_("(no prompt)"),      /* 0 */
	N_("Fix"),              /* 1 */
	N_("Clear"),            /* 2 */
	N_("Relocate"),         /* 3 */
	N_("Allocate"),         /* 4 */
	N_("Expand"),           /* 5 */
	N_("Connect to /lost+found"), /* 6 */
	N_("Create"),           /* 7 */
	N_("Salvage"),          /* 8 */
	N_("Truncate"),         /* 9 */
	N_("Clear inode"),      /* 10 */
	N_("Abort"),            /* 11 */
	N_("Split"),            /* 12 */
	N_("Continue"),         /* 13 */
	N_("Clone multiply-claimed blocks"), /* 14 */
	N_("Delete file"),      /* 15 */
	N_("Suppress messages"),/* 16 */
	N_("Unlink"),           /* 17 */
	N_("Clear HTree index"),/* 18 */
	N_("Recreate"),         /* 19 */
	"",                     /* 20 */
};

/*
 * These messages are printed when we are preen mode and we will be
 * automatically fixing the problem.
 */
static const char *const preen_msg[] = {
	N_("(NONE)"),           /* 0 */
	N_("FIXED"),            /* 1 */
	N_("CLEARED"),          /* 2 */
	N_("RELOCATED"),        /* 3 */
	N_("ALLOCATED"),        /* 4 */
	N_("EXPANDED"),         /* 5 */
	N_("RECONNECTED"),      /* 6 */
	N_("CREATED"),          /* 7 */
	N_("SALVAGED"),         /* 8 */
	N_("TRUNCATED"),        /* 9 */
	N_("INODE CLEARED"),    /* 10 */
	N_("ABORTED"),          /* 11 */
	N_("SPLIT"),            /* 12 */
	N_("CONTINUING"),       /* 13 */
	N_("MULTIPLY-CLAIMED BLOCKS CLONED"), /* 14 */
	N_("FILE DELETED"),     /* 15 */
	N_("SUPPRESSED"),       /* 16 */
	N_("UNLINKED"),         /* 17 */
	N_("HTREE INDEX CLEARED"),/* 18 */
	N_("WILL RECREATE"),    /* 19 */
	"",                     /* 20 */
};

static const struct e2fsck_problem problem_table[] = {

	/* Pre-Pass 1 errors */

	/* Block bitmap not in group */
	{ PR_0_BB_NOT_GROUP, N_("@b @B for @g %g is not in @g.  (@b %b)\n"),
	  PROMPT_RELOCATE, PR_LATCH_RELOC },

	/* Inode bitmap not in group */
	{ PR_0_IB_NOT_GROUP, N_("@i @B for @g %g is not in @g.  (@b %b)\n"),
	  PROMPT_RELOCATE, PR_LATCH_RELOC },

	/* Inode table not in group */
	{ PR_0_ITABLE_NOT_GROUP,
	  N_("@i table for @g %g is not in @g.  (@b %b)\n"
	  "WARNING: SEVERE DATA LOSS POSSIBLE.\n"),
	  PROMPT_RELOCATE, PR_LATCH_RELOC },

	/* Superblock corrupt */
	{ PR_0_SB_CORRUPT,
	  N_("\nThe @S could not be read or does not describe a correct ext2\n"
	  "@f.  If the @v is valid and it really contains an ext2\n"
	  "@f (and not swap or ufs or something else), then the @S\n"
	  "is corrupt, and you might try running e2fsck with an alternate @S:\n"
	  "    e2fsck -b %S <@v>\n\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Filesystem size is wrong */
	{ PR_0_FS_SIZE_WRONG,
	  N_("The @f size (according to the @S) is %b @bs\n"
	  "The physical size of the @v is %c @bs\n"
	  "Either the @S or the partition table is likely to be corrupt!\n"),
	  PROMPT_ABORT, 0 },

	/* Fragments not supported */
	{ PR_0_NO_FRAGMENTS,
	  N_("@S @b_size = %b, fragsize = %c.\n"
	  "This version of e2fsck does not support fragment sizes different\n"
	  "from the @b size.\n"),
	  PROMPT_NONE, PR_FATAL },

	  /* Bad blocks_per_group */
	{ PR_0_BLOCKS_PER_GROUP,
	  N_("@S @bs_per_group = %b, should have been %c\n"),
	  PROMPT_NONE, PR_AFTER_CODE, PR_0_SB_CORRUPT },

	/* Bad first_data_block */
	{ PR_0_FIRST_DATA_BLOCK,
	  N_("@S first_data_@b = %b, should have been %c\n"),
	  PROMPT_NONE, PR_AFTER_CODE, PR_0_SB_CORRUPT },

	/* Adding UUID to filesystem */
	{ PR_0_ADD_UUID,
	  N_("@f did not have a UUID; generating one.\n\n"),
	  PROMPT_NONE, 0 },

	/* Relocate hint */
	{ PR_0_RELOCATE_HINT,
	  N_("Note: if several inode or block bitmap blocks or part\n"
	  "of the inode table require relocation, you may wish to try\n"
	  "running e2fsck with the '-b %S' option first.  The problem\n"
	  "may lie only with the primary block group descriptors, and\n"
	  "the backup block group descriptors may be OK.\n\n"),
	  PROMPT_NONE, PR_PREEN_OK | PR_NOCOLLATE },

	/* Miscellaneous superblock corruption */
	{ PR_0_MISC_CORRUPT_SUPER,
	  N_("Corruption found in @S.  (%s = %N).\n"),
	  PROMPT_NONE, PR_AFTER_CODE, PR_0_SB_CORRUPT },

	/* Error determing physical device size of filesystem */
	{ PR_0_GETSIZE_ERROR,
	  N_("Error determining size of the physical @v: %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Inode count in superblock is incorrect */
	{ PR_0_INODE_COUNT_WRONG,
	  N_("@i count in @S is %i, @s %j.\n"),
	  PROMPT_FIX, 0 },

	{ PR_0_HURD_CLEAR_FILETYPE,
	  N_("The Hurd does not support the filetype feature.\n"),
	  PROMPT_CLEAR, 0 },

	/* Journal inode is invalid */
	{ PR_0_JOURNAL_BAD_INODE,
	  N_("@S has an @n ext3 @j (@i %i).\n"),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* The external journal has (unsupported) multiple filesystems */
	{ PR_0_JOURNAL_UNSUPP_MULTIFS,
	  N_("External @j has multiple @f users (unsupported).\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Can't find external journal */
	{ PR_0_CANT_FIND_JOURNAL,
	  N_("Can't find external @j\n"),
	  PROMPT_NONE, PR_FATAL },

	/* External journal has bad superblock */
	{ PR_0_EXT_JOURNAL_BAD_SUPER,
	  N_("External @j has bad @S\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Superblock has a bad journal UUID */
	{ PR_0_JOURNAL_BAD_UUID,
	  N_("External @j does not support this @f\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Journal has an unknown superblock type */
	{ PR_0_JOURNAL_UNSUPP_SUPER,
	  N_("Ext3 @j @S is unknown type %N (unsupported).\n"
	     "It is likely that your copy of e2fsck is old and/or doesn't "
	     "support this @j format.\n"
	     "It is also possible the @j @S is corrupt.\n"),
	  PROMPT_ABORT, PR_NO_OK | PR_AFTER_CODE, PR_0_JOURNAL_BAD_SUPER },

	/* Journal superblock is corrupt */
	{ PR_0_JOURNAL_BAD_SUPER,
	  N_("Ext3 @j @S is corrupt.\n"),
	  PROMPT_FIX, PR_PREEN_OK },

	/* Superblock flag should be cleared */
	{ PR_0_JOURNAL_HAS_JOURNAL,
	  N_("@S doesn't have has_@j flag, but has ext3 @j %s.\n"),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* Superblock flag is incorrect */
	{ PR_0_JOURNAL_RECOVER_SET,
	  N_("@S has ext3 needs_recovery flag set, but no @j.\n"),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* Journal has data, but recovery flag is clear */
	{ PR_0_JOURNAL_RECOVERY_CLEAR,
	  N_("ext3 recovery flag is clear, but @j has data.\n"),
	  PROMPT_NONE, 0 },

	/* Ask if we should clear the journal */
	{ PR_0_JOURNAL_RESET_JOURNAL,
	  N_("Clear @j"),
	  PROMPT_NULL, PR_PREEN_NOMSG },

	/* Ask if we should run the journal anyway */
	{ PR_0_JOURNAL_RUN,
	  N_("Run @j anyway"),
	  PROMPT_NULL, 0 },

	/* Run the journal by default */
	{ PR_0_JOURNAL_RUN_DEFAULT,
	  N_("Recovery flag not set in backup @S, so running @j anyway.\n"),
	  PROMPT_NONE, 0 },

	/* Clearing orphan inode */
	{ PR_0_ORPHAN_CLEAR_INODE,
	  N_("%s @o @i %i (uid=%Iu, gid=%Ig, mode=%Im, size=%Is)\n"),
	  PROMPT_NONE, 0 },

	/* Illegal block found in orphaned inode */
	{ PR_0_ORPHAN_ILLEGAL_BLOCK_NUM,
	   N_("@I @b #%B (%b) found in @o @i %i.\n"),
	  PROMPT_NONE, 0 },

	/* Already cleared block found in orphaned inode */
	{ PR_0_ORPHAN_ALREADY_CLEARED_BLOCK,
	   N_("Already cleared @b #%B (%b) found in @o @i %i.\n"),
	  PROMPT_NONE, 0 },

	/* Illegal orphan inode in superblock */
	{ PR_0_ORPHAN_ILLEGAL_HEAD_INODE,
	  N_("@I @o @i %i in @S.\n"),
	  PROMPT_NONE, 0 },

	/* Illegal inode in orphaned inode list */
	{ PR_0_ORPHAN_ILLEGAL_INODE,
	  N_("@I @i %i in @o @i list.\n"),
	  PROMPT_NONE, 0 },

	/* Filesystem revision is 0, but feature flags are set */
	{ PR_0_FS_REV_LEVEL,
	  N_("@f has feature flag(s) set, but is a revision 0 @f.  "),
	  PROMPT_FIX, PR_PREEN_OK | PR_NO_OK },

	/* Journal superblock has an unknown read-only feature flag set */
	{ PR_0_JOURNAL_UNSUPP_ROCOMPAT,
	  N_("Ext3 @j @S has an unknown read-only feature flag set.\n"),
	  PROMPT_ABORT, 0 },

	/* Journal superblock has an unknown incompatible feature flag set */
	{ PR_0_JOURNAL_UNSUPP_INCOMPAT,
	  N_("Ext3 @j @S has an unknown incompatible feature flag set.\n"),
	  PROMPT_ABORT, 0 },

	/* Journal has unsupported version number */
	{ PR_0_JOURNAL_UNSUPP_VERSION,
	  N_("@j version not supported by this e2fsck.\n"),
	  PROMPT_ABORT, 0 },

	/* Moving journal to hidden file */
	{ PR_0_MOVE_JOURNAL,
	  N_("Moving @j from /%s to hidden @i.\n\n"),
	  PROMPT_NONE, 0 },

	/* Error moving journal to hidden file */
	{ PR_0_ERR_MOVE_JOURNAL,
	  N_("Error moving @j: %m\n\n"),
	  PROMPT_NONE, 0 },

	/* Clearing V2 journal superblock */
	{ PR_0_CLEAR_V2_JOURNAL,
	  N_("Found @n V2 @j @S fields (from V1 @j).\n"
	     "Clearing fields beyond the V1 @j @S...\n\n"),
	  PROMPT_NONE, 0 },

	/* Backup journal inode blocks */
	{ PR_0_BACKUP_JNL,
	  N_("Backing up @j @i @b information.\n\n"),
	  PROMPT_NONE, 0 },

	/* Reserved blocks w/o resize_inode */
	{ PR_0_NONZERO_RESERVED_GDT_BLOCKS,
	  N_("@f does not have resize_@i enabled, but s_reserved_gdt_@bs\n"
	     "is %N; @s zero.  "),
	  PROMPT_FIX, 0 },

	/* Resize_inode not enabled, but resize inode is non-zero */
	{ PR_0_CLEAR_RESIZE_INODE,
	  N_("Resize_@i not enabled, but the resize @i is non-zero.  "),
	  PROMPT_CLEAR, 0 },

	/* Resize inode invalid */
	{ PR_0_RESIZE_INODE_INVALID,
	  N_("Resize @i not valid.  "),
	  PROMPT_RECREATE, 0 },

	/* Pass 1 errors */

	/* Pass 1: Checking inodes, blocks, and sizes */
	{ PR_1_PASS_HEADER,
	  N_("Pass 1: Checking @is, @bs, and sizes\n"),
	  PROMPT_NONE, 0 },

	/* Root directory is not an inode */
	{ PR_1_ROOT_NO_DIR, N_("@r is not a @d.  "),
	  PROMPT_CLEAR, 0 },

	/* Root directory has dtime set */
	{ PR_1_ROOT_DTIME,
	  N_("@r has dtime set (probably due to old mke2fs).  "),
	  PROMPT_FIX, PR_PREEN_OK },

	/* Reserved inode has bad mode */
	{ PR_1_RESERVED_BAD_MODE,
	  N_("Reserved @i %i (%Q) has @n mode.  "),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* Deleted inode has zero dtime */
	{ PR_1_ZERO_DTIME,
	  N_("@D @i %i has zero dtime.  "),
	  PROMPT_FIX, PR_PREEN_OK },

	/* Inode in use, but dtime set */
	{ PR_1_SET_DTIME,
	  N_("@i %i is in use, but has dtime set.  "),
	  PROMPT_FIX, PR_PREEN_OK },

	/* Zero-length directory */
	{ PR_1_ZERO_LENGTH_DIR,
	  N_("@i %i is a @z @d.  "),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* Block bitmap conflicts with some other fs block */
	{ PR_1_BB_CONFLICT,
	  N_("@g %g's @b @B at %b @C.\n"),
	  PROMPT_RELOCATE, 0 },

	/* Inode bitmap conflicts with some other fs block */
	{ PR_1_IB_CONFLICT,
	  N_("@g %g's @i @B at %b @C.\n"),
	  PROMPT_RELOCATE, 0 },

	/* Inode table conflicts with some other fs block */
	{ PR_1_ITABLE_CONFLICT,
	  N_("@g %g's @i table at %b @C.\n"),
	  PROMPT_RELOCATE, 0 },

	/* Block bitmap is on a bad block */
	{ PR_1_BB_BAD_BLOCK,
	  N_("@g %g's @b @B (%b) is bad.  "),
	  PROMPT_RELOCATE, 0 },

	/* Inode bitmap is on a bad block */
	{ PR_1_IB_BAD_BLOCK,
	  N_("@g %g's @i @B (%b) is bad.  "),
	  PROMPT_RELOCATE, 0 },

	/* Inode has incorrect i_size */
	{ PR_1_BAD_I_SIZE,
	  N_("@i %i, i_size is %Is, @s %N.  "),
	  PROMPT_FIX, PR_PREEN_OK },

	/* Inode has incorrect i_blocks */
	{ PR_1_BAD_I_BLOCKS,
	  N_("@i %i, i_@bs is %Ib, @s %N.  "),
	  PROMPT_FIX, PR_PREEN_OK },

	/* Illegal blocknumber in inode */
	{ PR_1_ILLEGAL_BLOCK_NUM,
	  N_("@I @b #%B (%b) in @i %i.  "),
	  PROMPT_CLEAR, PR_LATCH_BLOCK },

	/* Block number overlaps fs metadata */
	{ PR_1_BLOCK_OVERLAPS_METADATA,
	  N_("@b #%B (%b) overlaps @f metadata in @i %i.  "),
	  PROMPT_CLEAR, PR_LATCH_BLOCK },

	/* Inode has illegal blocks (latch question) */
	{ PR_1_INODE_BLOCK_LATCH,
	  N_("@i %i has illegal @b(s).  "),
	  PROMPT_CLEAR, 0 },

	/* Too many bad blocks in inode */
	{ PR_1_TOO_MANY_BAD_BLOCKS,
	  N_("Too many illegal @bs in @i %i.\n"),
	  PROMPT_CLEAR_INODE, PR_NO_OK },

	/* Illegal block number in bad block inode */
	{ PR_1_BB_ILLEGAL_BLOCK_NUM,
	  N_("@I @b #%B (%b) in bad @b @i.  "),
	  PROMPT_CLEAR, PR_LATCH_BBLOCK },

	/* Bad block inode has illegal blocks (latch question) */
	{ PR_1_INODE_BBLOCK_LATCH,
	  N_("Bad @b @i has illegal @b(s).  "),
	  PROMPT_CLEAR, 0 },

	/* Duplicate or bad blocks in use! */
	{ PR_1_DUP_BLOCKS_PREENSTOP,
	  N_("Duplicate or bad @b in use!\n"),
	  PROMPT_NONE, 0 },

	/* Bad block used as bad block indirect block */
	{ PR_1_BBINODE_BAD_METABLOCK,
	  N_("Bad @b %b used as bad @b @i indirect @b.  "),
	  PROMPT_CLEAR, PR_LATCH_BBLOCK },

	/* Inconsistency can't be fixed prompt */
	{ PR_1_BBINODE_BAD_METABLOCK_PROMPT,
	  N_("\nThe bad @b @i has probably been corrupted.  You probably\n"
	     "should stop now and run ""e2fsck -c"" to scan for bad blocks\n"
	     "in the @f.\n"),
	  PROMPT_CONTINUE, PR_PREEN_NOMSG },

	/* Bad primary block */
	{ PR_1_BAD_PRIMARY_BLOCK,
	  N_("\nIf the @b is really bad, the @f cannot be fixed.\n"),
	  PROMPT_NONE, PR_AFTER_CODE, PR_1_BAD_PRIMARY_BLOCK_PROMPT },

	/* Bad primary block prompt */
	{ PR_1_BAD_PRIMARY_BLOCK_PROMPT,
	  N_("You can remove this @b from the bad @b list and hope\n"
	     "that the @b is really OK.  But there are no guarantees.\n\n"),
	  PROMPT_CLEAR, PR_PREEN_NOMSG },

	/* Bad primary superblock */
	{ PR_1_BAD_PRIMARY_SUPERBLOCK,
	  N_("The primary @S (%b) is on the bad @b list.\n"),
	  PROMPT_NONE, PR_AFTER_CODE, PR_1_BAD_PRIMARY_BLOCK },

	/* Bad primary block group descriptors */
	{ PR_1_BAD_PRIMARY_GROUP_DESCRIPTOR,
	  N_("Block %b in the primary @g descriptors "
	  "is on the bad @b list\n"),
	  PROMPT_NONE, PR_AFTER_CODE, PR_1_BAD_PRIMARY_BLOCK },

	/* Bad superblock in group */
	{ PR_1_BAD_SUPERBLOCK,
	  N_("Warning: Group %g's @S (%b) is bad.\n"),
	  PROMPT_NONE, PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Bad block group descriptors in group */
	{ PR_1_BAD_GROUP_DESCRIPTORS,
	  N_("Warning: Group %g's copy of the @g descriptors has a bad "
	  "@b (%b).\n"),
	  PROMPT_NONE, PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Block claimed for no reason */
	{ PR_1_PROGERR_CLAIMED_BLOCK,
	  N_("Programming error?  @b #%b claimed for no reason in "
	  "process_bad_@b.\n"),
	  PROMPT_NONE, PR_PREEN_OK },

	/* Error allocating blocks for relocating metadata */
	{ PR_1_RELOC_BLOCK_ALLOCATE,
	  N_("@A %N contiguous @b(s) in @b @g %g for %s: %m\n"),
	  PROMPT_NONE, PR_PREEN_OK },

	/* Error allocating block buffer during relocation process */
	{ PR_1_RELOC_MEMORY_ALLOCATE,
	  N_("@A @b buffer for relocating %s\n"),
	  PROMPT_NONE, PR_PREEN_OK },

	/* Relocating metadata group information from X to Y */
	{ PR_1_RELOC_FROM_TO,
	  N_("Relocating @g %g's %s from %b to %c...\n"),
	  PROMPT_NONE, PR_PREEN_OK },

	/* Relocating metatdata group information to X */
	{ PR_1_RELOC_TO,
	  N_("Relocating @g %g's %s to %c...\n"), /* xgettext:no-c-format */
	  PROMPT_NONE, PR_PREEN_OK },

	/* Block read error during relocation process */
	{ PR_1_RELOC_READ_ERR,
	  N_("Warning: could not read @b %b of %s: %m\n"),
	  PROMPT_NONE, PR_PREEN_OK },

	/* Block write error during relocation process */
	{ PR_1_RELOC_WRITE_ERR,
	  N_("Warning: could not write @b %b for %s: %m\n"),
	  PROMPT_NONE, PR_PREEN_OK },

	/* Error allocating inode bitmap */
	{ PR_1_ALLOCATE_IBITMAP_ERROR,
	  N_("@A @i @B (%N): %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error allocating block bitmap */
	{ PR_1_ALLOCATE_BBITMAP_ERROR,
	  N_("@A @b @B (%N): %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error allocating icount structure */
	{ PR_1_ALLOCATE_ICOUNT,
	  N_("@A icount link information: %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error allocating dbcount */
	{ PR_1_ALLOCATE_DBCOUNT,
	  N_("@A @d @b array: %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error while scanning inodes */
	{ PR_1_ISCAN_ERROR,
	  N_("Error while scanning @is (%i): %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error while iterating over blocks */
	{ PR_1_BLOCK_ITERATE,
	  N_("Error while iterating over @bs in @i %i: %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error while storing inode count information */
	{ PR_1_ICOUNT_STORE,
	  N_("Error storing @i count information (@i=%i, count=%N): %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error while storing directory block information */
	{ PR_1_ADD_DBLOCK,
	  N_("Error storing @d @b information "
	  "(@i=%i, @b=%b, num=%N): %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error while reading inode (for clearing) */
	{ PR_1_READ_INODE,
	  N_("Error reading @i %i: %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Suppress messages prompt */
	{ PR_1_SUPPRESS_MESSAGES, "", PROMPT_SUPPRESS, PR_NO_OK },

	/* Imagic flag set on an inode when filesystem doesn't support it */
	{ PR_1_SET_IMAGIC,
	  N_("@i %i has imagic flag set.  "),
	  PROMPT_CLEAR, 0 },

	/* Immutable flag set on a device or socket inode */
	{ PR_1_SET_IMMUTABLE,
	  N_("Special (@v/socket/fifo/symlink) file (@i %i) has immutable\n"
	     "or append-only flag set.  "),
	  PROMPT_CLEAR, PR_PREEN_OK | PR_PREEN_NO | PR_NO_OK },

	/* Compression flag set on an inode when filesystem doesn't support it */
	{ PR_1_COMPR_SET,
	  N_("@i %i has @cion flag set on @f without @cion support.  "),
	  PROMPT_CLEAR, 0 },

	/* Non-zero size for device, fifo or socket inode */
	{ PR_1_SET_NONZSIZE,
	  N_("Special (@v/socket/fifo) @i %i has non-zero size.  "),
	  PROMPT_FIX, PR_PREEN_OK },

	/* Filesystem revision is 0, but feature flags are set */
	{ PR_1_FS_REV_LEVEL,
	  N_("@f has feature flag(s) set, but is a revision 0 @f.  "),
	  PROMPT_FIX, PR_PREEN_OK | PR_NO_OK },

	/* Journal inode is not in use, but contains data */
	{ PR_1_JOURNAL_INODE_NOT_CLEAR,
	  N_("@j @i is not in use, but contains data.  "),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* Journal has bad mode */
	{ PR_1_JOURNAL_BAD_MODE,
	  N_("@j is not regular file.  "),
	  PROMPT_FIX, PR_PREEN_OK },

	/* Deal with inodes that were part of orphan linked list */
	{ PR_1_LOW_DTIME,
	  N_("@i %i was part of the @o @i list.  "),
	  PROMPT_FIX, PR_LATCH_LOW_DTIME, 0 },

	/* Deal with inodes that were part of corrupted orphan linked
	   list (latch question) */
	{ PR_1_ORPHAN_LIST_REFUGEES,
	  N_("@is that were part of a corrupted orphan linked list found.  "),
	  PROMPT_FIX, 0 },

	/* Error allocating refcount structure */
	{ PR_1_ALLOCATE_REFCOUNT,
	  N_("@A refcount structure (%N): %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error reading extended attribute block */
	{ PR_1_READ_EA_BLOCK,
	  N_("Error reading @a @b %b for @i %i.  "),
	  PROMPT_CLEAR, 0 },

	/* Invalid extended attribute block */
	{ PR_1_BAD_EA_BLOCK,
	  N_("@i %i has a bad @a @b %b.  "),
	  PROMPT_CLEAR, 0 },

	/* Error reading Extended Attribute block while fixing refcount */
	{ PR_1_EXTATTR_READ_ABORT,
	  N_("Error reading @a @b %b (%m).  "),
	  PROMPT_ABORT, 0 },

	/* Extended attribute reference count incorrect */
	{ PR_1_EXTATTR_REFCOUNT,
	  N_("@a @b %b has reference count %B, @s %N.  "),
	  PROMPT_FIX, 0 },

	/* Error writing Extended Attribute block while fixing refcount */
	{ PR_1_EXTATTR_WRITE,
	  N_("Error writing @a @b %b (%m).  "),
	  PROMPT_ABORT, 0 },

	/* Multiple EA blocks not supported */
	{ PR_1_EA_MULTI_BLOCK,
	  N_("@a @b %b has h_@bs > 1.  "),
	  PROMPT_CLEAR, 0},

	/* Error allocating EA region allocation structure */
	{ PR_1_EA_ALLOC_REGION,
	  N_("@A @a @b %b.  "),
	  PROMPT_ABORT, 0},

	/* Error EA allocation collision */
	{ PR_1_EA_ALLOC_COLLISION,
	  N_("@a @b %b is corrupt (allocation collision).  "),
	  PROMPT_CLEAR, 0},

	/* Bad extended attribute name */
	{ PR_1_EA_BAD_NAME,
	  N_("@a @b %b is corrupt (@n name).  "),
	  PROMPT_CLEAR, 0},

	/* Bad extended attribute value */
	{ PR_1_EA_BAD_VALUE,
	  N_("@a @b %b is corrupt (@n value).  "),
	  PROMPT_CLEAR, 0},

	/* Inode too big (latch question) */
	{ PR_1_INODE_TOOBIG,
	  N_("@i %i is too big.  "), PROMPT_TRUNCATE, 0 },

	/* Directory too big */
	{ PR_1_TOOBIG_DIR,
	  N_("@b #%B (%b) causes @d to be too big.  "),
	  PROMPT_CLEAR, PR_LATCH_TOOBIG },

	/* Regular file too big */
	{ PR_1_TOOBIG_REG,
	  N_("@b #%B (%b) causes file to be too big.  "),
	  PROMPT_CLEAR, PR_LATCH_TOOBIG },

	/* Symlink too big */
	{ PR_1_TOOBIG_SYMLINK,
	  N_("@b #%B (%b) causes symlink to be too big.  "),
	  PROMPT_CLEAR, PR_LATCH_TOOBIG },

	/* INDEX_FL flag set on a non-HTREE filesystem */
	{ PR_1_HTREE_SET,
	  N_("@i %i has INDEX_FL flag set on @f without htree support.\n"),
	  PROMPT_CLEAR_HTREE, PR_PREEN_OK },

	/* INDEX_FL flag set on a non-directory */
	{ PR_1_HTREE_NODIR,
	  N_("@i %i has INDEX_FL flag set but is not a @d.\n"),
	  PROMPT_CLEAR_HTREE, PR_PREEN_OK },

	/* Invalid root node in HTREE directory */
	{ PR_1_HTREE_BADROOT,
	  N_("@h %i has an @n root node.\n"),
	  PROMPT_CLEAR_HTREE, PR_PREEN_OK },

	/* Unsupported hash version in HTREE directory */
	{ PR_1_HTREE_HASHV,
	  N_("@h %i has an unsupported hash version (%N)\n"),
	  PROMPT_CLEAR_HTREE, PR_PREEN_OK },

	/* Incompatible flag in HTREE root node */
	{ PR_1_HTREE_INCOMPAT,
	  N_("@h %i uses an incompatible htree root node flag.\n"),
	  PROMPT_CLEAR_HTREE, PR_PREEN_OK },

	/* HTREE too deep */
	{ PR_1_HTREE_DEPTH,
	  N_("@h %i has a tree depth (%N) which is too big\n"),
	  PROMPT_CLEAR_HTREE, PR_PREEN_OK },

	/* Bad block has indirect block that conflicts with filesystem block */
	{ PR_1_BB_FS_BLOCK,
	  N_("Bad @b @i has an indirect @b (%b) that conflicts with\n"
	     "@f metadata.  "),
	  PROMPT_CLEAR, PR_LATCH_BBLOCK },

	/* Resize inode failed */
	{ PR_1_RESIZE_INODE_CREATE,
	  N_("Resize @i (re)creation failed: %m."),
	  PROMPT_ABORT, 0 },

	/* invalid inode->i_extra_isize */
	{ PR_1_EXTRA_ISIZE,
	  N_("@i %i has a extra size (%IS) which is @n\n"),
	  PROMPT_FIX, PR_PREEN_OK },

	/* invalid ea entry->e_name_len */
	{ PR_1_ATTR_NAME_LEN,
	  N_("@a in @i %i has a namelen (%N) which is @n\n"),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* invalid ea entry->e_value_size */
	{ PR_1_ATTR_VALUE_SIZE,
	  N_("@a in @i %i has a value size (%N) which is @n\n"),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* invalid ea entry->e_value_offs */
	{ PR_1_ATTR_VALUE_OFFSET,
	  N_("@a in @i %i has a value offset (%N) which is @n\n"),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* invalid ea entry->e_value_block */
	{ PR_1_ATTR_VALUE_BLOCK,
	  N_("@a in @i %i has a value @b (%N) which is @n (must be 0)\n"),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* invalid ea entry->e_hash */
	{ PR_1_ATTR_HASH,
	  N_("@a in @i %i has a hash (%N) which is @n (must be 0)\n"),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* Pass 1b errors */

	/* Pass 1B: Rescan for duplicate/bad blocks */
	{ PR_1B_PASS_HEADER,
	  N_("\nRunning additional passes to resolve @bs claimed by more than one @i...\n"
	  "Pass 1B: Rescanning for @m @bs\n"),
	  PROMPT_NONE, 0 },

	/* Duplicate/bad block(s) header */
	{ PR_1B_DUP_BLOCK_HEADER,
	  N_("@m @b(s) in @i %i:"),
	  PROMPT_NONE, 0 },

	/* Duplicate/bad block(s) in inode */
	{ PR_1B_DUP_BLOCK,
	  " %b",
	  PROMPT_NONE, PR_LATCH_DBLOCK | PR_PREEN_NOHDR },

	/* Duplicate/bad block(s) end */
	{ PR_1B_DUP_BLOCK_END,
	  "\n",
	  PROMPT_NONE, PR_PREEN_NOHDR },

	/* Error while scanning inodes */
	{ PR_1B_ISCAN_ERROR,
	  N_("Error while scanning inodes (%i): %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error allocating inode bitmap */
	{ PR_1B_ALLOCATE_IBITMAP_ERROR,
	  N_("@A @i @B (@i_dup_map): %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error while iterating over blocks */
	{ PR_1B_BLOCK_ITERATE,
	  N_("Error while iterating over @bs in @i %i (%s): %m\n"),
	  PROMPT_NONE, 0 },

	/* Error adjusting EA refcount */
	{ PR_1B_ADJ_EA_REFCOUNT,
	  N_("Error adjusting refcount for @a @b %b (@i %i): %m\n"),
	  PROMPT_NONE, 0 },


	/* Pass 1C: Scan directories for inodes with multiply-claimed blocks. */
	{ PR_1C_PASS_HEADER,
	  N_("Pass 1C: Scanning directories for @is with @m @bs.\n"),
	  PROMPT_NONE, 0 },


	/* Pass 1D: Reconciling multiply-claimed blocks */
	{ PR_1D_PASS_HEADER,
	  N_("Pass 1D: Reconciling @m @bs\n"),
	  PROMPT_NONE, 0 },

	/* File has duplicate blocks */
	{ PR_1D_DUP_FILE,
	  N_("File %Q (@i #%i, mod time %IM)\n"
	  "  has %B @m @b(s), shared with %N file(s):\n"),
	  PROMPT_NONE, 0 },

	/* List of files sharing duplicate blocks */
	{ PR_1D_DUP_FILE_LIST,
	  N_("\t%Q (@i #%i, mod time %IM)\n"),
	  PROMPT_NONE, 0 },

	/* File sharing blocks with filesystem metadata  */
	{ PR_1D_SHARE_METADATA,
	  N_("\t<@f metadata>\n"),
	  PROMPT_NONE, 0 },

	/* Report of how many duplicate/bad inodes */
	{ PR_1D_NUM_DUP_INODES,
	  N_("(There are %N @is containing @m @bs.)\n\n"),
	  PROMPT_NONE, 0 },

	/* Duplicated blocks already reassigned or cloned. */
	{ PR_1D_DUP_BLOCKS_DEALT,
	  N_("@m @bs already reassigned or cloned.\n\n"),
	  PROMPT_NONE, 0 },

	/* Clone duplicate/bad blocks? */
	{ PR_1D_CLONE_QUESTION,
	  "", PROMPT_CLONE, PR_NO_OK },

	/* Delete file? */
	{ PR_1D_DELETE_QUESTION,
	  "", PROMPT_DELETE, 0 },

	/* Couldn't clone file (error) */
	{ PR_1D_CLONE_ERROR,
	  N_("Couldn't clone file: %m\n"), PROMPT_NONE, 0 },

	/* Pass 2 errors */

	/* Pass 2: Checking directory structure */
	{ PR_2_PASS_HEADER,
	  N_("Pass 2: Checking @d structure\n"),
	  PROMPT_NONE, 0 },

	/* Bad inode number for '.' */
	{ PR_2_BAD_INODE_DOT,
	  N_("@n @i number for '.' in @d @i %i.\n"),
	  PROMPT_FIX, 0 },

	/* Directory entry has bad inode number */
	{ PR_2_BAD_INO,
	  N_("@E has @n @i #: %Di.\n"),
	  PROMPT_CLEAR, 0 },

	/* Directory entry has deleted or unused inode */
	{ PR_2_UNUSED_INODE,
	  N_("@E has @D/unused @i %Di.  "),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* Directry entry is link to '.' */
	{ PR_2_LINK_DOT,
	  N_("@E @L to '.'  "),
	  PROMPT_CLEAR, 0 },

	/* Directory entry points to inode now located in a bad block */
	{ PR_2_BB_INODE,
	  N_("@E points to @i (%Di) located in a bad @b.\n"),
	  PROMPT_CLEAR, 0 },

	/* Directory entry contains a link to a directory */
	{ PR_2_LINK_DIR,
	  N_("@E @L to @d %P (%Di).\n"),
	  PROMPT_CLEAR, 0 },

	/* Directory entry contains a link to the root directry */
	{ PR_2_LINK_ROOT,
	  N_("@E @L to the @r.\n"),
	  PROMPT_CLEAR, 0 },

	/* Directory entry has illegal characters in its name */
	{ PR_2_BAD_NAME,
	  N_("@E has illegal characters in its name.\n"),
	  PROMPT_FIX, 0 },

	/* Missing '.' in directory inode */
	{ PR_2_MISSING_DOT,
	  N_("Missing '.' in @d @i %i.\n"),
	  PROMPT_FIX, 0 },

	/* Missing '..' in directory inode */
	{ PR_2_MISSING_DOT_DOT,
	  N_("Missing '..' in @d @i %i.\n"),
	  PROMPT_FIX, 0 },

	/* First entry in directory inode doesn't contain '.' */
	{ PR_2_1ST_NOT_DOT,
	  N_("First @e '%Dn' (@i=%Di) in @d @i %i (%p) @s '.'\n"),
	  PROMPT_FIX, 0 },

	/* Second entry in directory inode doesn't contain '..' */
	{ PR_2_2ND_NOT_DOT_DOT,
	  N_("Second @e '%Dn' (@i=%Di) in @d @i %i @s '..'\n"),
	  PROMPT_FIX, 0 },

	/* i_faddr should be zero */
	{ PR_2_FADDR_ZERO,
	  N_("i_faddr @F %IF, @s zero.\n"),
	  PROMPT_CLEAR, 0 },

	/* i_file_acl should be zero */
	{ PR_2_FILE_ACL_ZERO,
	  N_("i_file_acl @F %If, @s zero.\n"),
	  PROMPT_CLEAR, 0 },

	/* i_dir_acl should be zero */
	{ PR_2_DIR_ACL_ZERO,
	  N_("i_dir_acl @F %Id, @s zero.\n"),
	  PROMPT_CLEAR, 0 },

	/* i_frag should be zero */
	{ PR_2_FRAG_ZERO,
	  N_("i_frag @F %N, @s zero.\n"),
	  PROMPT_CLEAR, 0 },

	/* i_fsize should be zero */
	{ PR_2_FSIZE_ZERO,
	  N_("i_fsize @F %N, @s zero.\n"),
	  PROMPT_CLEAR, 0 },

	/* inode has bad mode */
	{ PR_2_BAD_MODE,
	  N_("@i %i (%Q) has @n mode (%Im).\n"),
	  PROMPT_CLEAR, 0 },

	/* directory corrupted */
	{ PR_2_DIR_CORRUPTED,
	  N_("@d @i %i, @b %B, offset %N: @d corrupted\n"),
	  PROMPT_SALVAGE, 0 },

	/* filename too long */
	{ PR_2_FILENAME_LONG,
	  N_("@d @i %i, @b %B, offset %N: filename too long\n"),
	  PROMPT_TRUNCATE, 0 },

	/* Directory inode has a missing block (hole) */
	{ PR_2_DIRECTORY_HOLE,
	  N_("@d @i %i has an unallocated @b #%B.  "),
	  PROMPT_ALLOCATE, 0 },

	/* '.' is not NULL terminated */
	{ PR_2_DOT_NULL_TERM,
	  N_("'.' @d @e in @d @i %i is not NULL terminated\n"),
	  PROMPT_FIX, 0 },

	/* '..' is not NULL terminated */
	{ PR_2_DOT_DOT_NULL_TERM,
	  N_("'..' @d @e in @d @i %i is not NULL terminated\n"),
	  PROMPT_FIX, 0 },

	/* Illegal character device inode */
	{ PR_2_BAD_CHAR_DEV,
	  N_("@i %i (%Q) is an @I character @v.\n"),
	  PROMPT_CLEAR, 0 },

	/* Illegal block device inode */
	{ PR_2_BAD_BLOCK_DEV,
	  N_("@i %i (%Q) is an @I @b @v.\n"),
	  PROMPT_CLEAR, 0 },

	/* Duplicate '.' entry */
	{ PR_2_DUP_DOT,
	  N_("@E is duplicate '.' @e.\n"),
	  PROMPT_FIX, 0 },

	/* Duplicate '..' entry */
	{ PR_2_DUP_DOT_DOT,
	  N_("@E is duplicate '..' @e.\n"),
	  PROMPT_FIX, 0 },

	/* Internal error: couldn't find dir_info */
	{ PR_2_NO_DIRINFO,
	  N_("Internal error: cannot find dir_info for %i.\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Final rec_len is wrong */
	{ PR_2_FINAL_RECLEN,
	  N_("@E has rec_len of %Dr, @s %N.\n"),
	  PROMPT_FIX, 0 },

	/* Error allocating icount structure */
	{ PR_2_ALLOCATE_ICOUNT,
	  N_("@A icount structure: %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error iterating over directory blocks */
	{ PR_2_DBLIST_ITERATE,
	  N_("Error iterating over @d @bs: %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error reading directory block */
	{ PR_2_READ_DIRBLOCK,
	  N_("Error reading @d @b %b (@i %i): %m\n"),
	  PROMPT_CONTINUE, 0 },

	/* Error writing directory block */
	{ PR_2_WRITE_DIRBLOCK,
	  N_("Error writing @d @b %b (@i %i): %m\n"),
	  PROMPT_CONTINUE, 0 },

	/* Error allocating new directory block */
	{ PR_2_ALLOC_DIRBOCK,
	  N_("@A new @d @b for @i %i (%s): %m\n"),
	  PROMPT_NONE, 0 },

	/* Error deallocating inode */
	{ PR_2_DEALLOC_INODE,
	  N_("Error deallocating @i %i: %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Directory entry for '.' is big.  Split? */
	{ PR_2_SPLIT_DOT,
	  N_("@d @e for '.' is big.  "),
	  PROMPT_SPLIT, PR_NO_OK },

	/* Illegal FIFO inode */
	{ PR_2_BAD_FIFO,
	  N_("@i %i (%Q) is an @I FIFO.\n"),
	  PROMPT_CLEAR, 0 },

	/* Illegal socket inode */
	{ PR_2_BAD_SOCKET,
	  N_("@i %i (%Q) is an @I socket.\n"),
	  PROMPT_CLEAR, 0 },

	/* Directory filetype not set */
	{ PR_2_SET_FILETYPE,
	  N_("Setting filetype for @E to %N.\n"),
	  PROMPT_NONE, PR_PREEN_OK | PR_NO_OK | PR_NO_NOMSG },

	/* Directory filetype incorrect */
	{ PR_2_BAD_FILETYPE,
	  N_("@E has an incorrect filetype (was %Dt, @s %N).\n"),
	  PROMPT_FIX, 0 },

	/* Directory filetype set on filesystem */
	{ PR_2_CLEAR_FILETYPE,
	  N_("@E has filetype set.\n"),
	  PROMPT_CLEAR, PR_PREEN_OK },

	/* Directory filename is null */
	{ PR_2_NULL_NAME,
	  N_("@E has a @z name.\n"),
	  PROMPT_CLEAR, 0 },

	/* Invalid symlink */
	{ PR_2_INVALID_SYMLINK,
	  N_("Symlink %Q (@i #%i) is @n.\n"),
	  PROMPT_CLEAR, 0 },

	/* i_file_acl (extended attribute block) is bad */
	{ PR_2_FILE_ACL_BAD,
	  N_("@a @b @F @n (%If).\n"),
	  PROMPT_CLEAR, 0 },

	/* Filesystem contains large files, but has no such flag in sb */
	{ PR_2_FEATURE_LARGE_FILES,
	  N_("@f contains large files, but lacks LARGE_FILE flag in @S.\n"),
	  PROMPT_FIX, 0 },

	/* Node in HTREE directory not referenced */
	{ PR_2_HTREE_NOTREF,
	  N_("@p @h %d: node (%B) not referenced\n"),
	  PROMPT_NONE, 0 },

	/* Node in HTREE directory referenced twice */
	{ PR_2_HTREE_DUPREF,
	  N_("@p @h %d: node (%B) referenced twice\n"),
	  PROMPT_NONE, 0 },

	/* Node in HTREE directory has bad min hash */
	{ PR_2_HTREE_MIN_HASH,
	  N_("@p @h %d: node (%B) has bad min hash\n"),
	  PROMPT_NONE, 0 },

	/* Node in HTREE directory has bad max hash */
	{ PR_2_HTREE_MAX_HASH,
	  N_("@p @h %d: node (%B) has bad max hash\n"),
	  PROMPT_NONE, 0 },

	/* Clear invalid HTREE directory */
	{ PR_2_HTREE_CLEAR,
	  N_("@n @h %d (%q).  "), PROMPT_CLEAR, 0 },

	/* Bad block in htree interior node */
	{ PR_2_HTREE_BADBLK,
	  N_("@p @h %d (%q): bad @b number %b.\n"),
	  PROMPT_CLEAR_HTREE, 0 },

	/* Error adjusting EA refcount */
	{ PR_2_ADJ_EA_REFCOUNT,
	  N_("Error adjusting refcount for @a @b %b (@i %i): %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Invalid HTREE root node */
	{ PR_2_HTREE_BAD_ROOT,
	  N_("@p @h %d: root node is @n\n"),
	  PROMPT_CLEAR_HTREE, PR_PREEN_OK },

	/* Invalid HTREE limit */
	{ PR_2_HTREE_BAD_LIMIT,
	  N_("@p @h %d: node (%B) has @n limit (%N)\n"),
	  PROMPT_CLEAR_HTREE, PR_PREEN_OK },

	/* Invalid HTREE count */
	{ PR_2_HTREE_BAD_COUNT,
	  N_("@p @h %d: node (%B) has @n count (%N)\n"),
	  PROMPT_CLEAR_HTREE, PR_PREEN_OK },

	/* HTREE interior node has out-of-order hashes in table */
	{ PR_2_HTREE_HASH_ORDER,
	  N_("@p @h %d: node (%B) has an unordered hash table\n"),
	  PROMPT_CLEAR_HTREE, PR_PREEN_OK },

	/* Node in HTREE directory has invalid depth */
	{ PR_2_HTREE_BAD_DEPTH,
	  N_("@p @h %d: node (%B) has @n depth\n"),
	  PROMPT_NONE, 0 },

	/* Duplicate directory entry found */
	{ PR_2_DUPLICATE_DIRENT,
	  N_("Duplicate @E found.  "),
	  PROMPT_CLEAR, 0 },

	/* Non-unique filename found */
	{ PR_2_NON_UNIQUE_FILE, /* xgettext: no-c-format */
	  N_("@E has a non-unique filename.\nRename to %s"),
	  PROMPT_NULL, 0 },

	/* Duplicate directory entry found */
	{ PR_2_REPORT_DUP_DIRENT,
	  N_("Duplicate @e '%Dn' found.\n\tMarking %p (%i) to be rebuilt.\n\n"),
	  PROMPT_NONE, 0 },

	/* Pass 3 errors */

	/* Pass 3: Checking directory connectivity */
	{ PR_3_PASS_HEADER,
	  N_("Pass 3: Checking @d connectivity\n"),
	  PROMPT_NONE, 0 },

	/* Root inode not allocated */
	{ PR_3_NO_ROOT_INODE,
	  N_("@r not allocated.  "),
	  PROMPT_ALLOCATE, 0 },

	/* No room in lost+found */
	{ PR_3_EXPAND_LF_DIR,
	  N_("No room in @l @d.  "),
	  PROMPT_EXPAND, 0 },

	/* Unconnected directory inode */
	{ PR_3_UNCONNECTED_DIR,
	  N_("Unconnected @d @i %i (%p)\n"),
	  PROMPT_CONNECT, 0 },

	/* /lost+found not found */
	{ PR_3_NO_LF_DIR,
	  N_("/@l not found.  "),
	  PROMPT_CREATE, PR_PREEN_OK },

	/* .. entry is incorrect */
	{ PR_3_BAD_DOT_DOT,
	  N_("'..' in %Q (%i) is %P (%j), @s %q (%d).\n"),
	  PROMPT_FIX, 0 },

	/* Bad or non-existent /lost+found.  Cannot reconnect */
	{ PR_3_NO_LPF,
	  N_("Bad or non-existent /@l.  Cannot reconnect.\n"),
	  PROMPT_NONE, 0 },

	/* Could not expand /lost+found */
	{ PR_3_CANT_EXPAND_LPF,
	  N_("Could not expand /@l: %m\n"),
	  PROMPT_NONE, 0 },

	/* Could not reconnect inode */
	{ PR_3_CANT_RECONNECT,
	  N_("Could not reconnect %i: %m\n"),
	  PROMPT_NONE, 0 },

	/* Error while trying to find /lost+found */
	{ PR_3_ERR_FIND_LPF,
	  N_("Error while trying to find /@l: %m\n"),
	  PROMPT_NONE, 0 },

	/* Error in ext2fs_new_block while creating /lost+found */
	{ PR_3_ERR_LPF_NEW_BLOCK,
	  N_("ext2fs_new_@b: %m while trying to create /@l @d\n"),
	  PROMPT_NONE, 0 },

	/* Error in ext2fs_new_inode while creating /lost+found */
	{ PR_3_ERR_LPF_NEW_INODE,
	  N_("ext2fs_new_@i: %m while trying to create /@l @d\n"),
	  PROMPT_NONE, 0 },

	/* Error in ext2fs_new_dir_block while creating /lost+found */
	{ PR_3_ERR_LPF_NEW_DIR_BLOCK,
	  N_("ext2fs_new_dir_@b: %m while creating new @d @b\n"),
	  PROMPT_NONE, 0 },

	/* Error while writing directory block for /lost+found */
	{ PR_3_ERR_LPF_WRITE_BLOCK,
	  N_("ext2fs_write_dir_@b: %m while writing the @d @b for /@l\n"),
	  PROMPT_NONE, 0 },

	/* Error while adjusting inode count */
	{ PR_3_ADJUST_INODE,
	  N_("Error while adjusting @i count on @i %i\n"),
	  PROMPT_NONE, 0 },

	/* Couldn't fix parent directory -- error */
	{ PR_3_FIX_PARENT_ERR,
	  N_("Couldn't fix parent of @i %i: %m\n\n"),
	  PROMPT_NONE, 0 },

	/* Couldn't fix parent directory -- couldn't find it */
	{ PR_3_FIX_PARENT_NOFIND,
	  N_("Couldn't fix parent of @i %i: Couldn't find parent @d @e\n\n"),
	  PROMPT_NONE, 0 },

	/* Error allocating inode bitmap */
	{ PR_3_ALLOCATE_IBITMAP_ERROR,
	  N_("@A @i @B (%N): %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error creating root directory */
	{ PR_3_CREATE_ROOT_ERROR,
	  N_("Error creating root @d (%s): %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error creating lost and found directory */
	{ PR_3_CREATE_LPF_ERROR,
	  N_("Error creating /@l @d (%s): %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Root inode is not directory; aborting */
	{ PR_3_ROOT_NOT_DIR_ABORT,
	  N_("@r is not a @d; aborting.\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Cannot proceed without a root inode. */
	{ PR_3_NO_ROOT_INODE_ABORT,
	  N_("can't proceed without a @r.\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Internal error: couldn't find dir_info */
	{ PR_3_NO_DIRINFO,
	  N_("Internal error: cannot find dir_info for %i.\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Lost+found not a directory */
	{ PR_3_LPF_NOTDIR,
	  N_("/@l is not a @d (ino=%i)\n"),
	  PROMPT_UNLINK, 0 },

	/* Pass 3A Directory Optimization       */

	/* Pass 3A: Optimizing directories */
	{ PR_3A_PASS_HEADER,
	  N_("Pass 3A: Optimizing directories\n"),
	  PROMPT_NONE, PR_PREEN_NOMSG },

	/* Error iterating over directories */
	{ PR_3A_OPTIMIZE_ITER,
	  N_("Failed to create dirs_to_hash iterator: %m"),
	  PROMPT_NONE, 0 },

	/* Error rehash directory */
	{ PR_3A_OPTIMIZE_DIR_ERR,
	  N_("Failed to optimize directory %q (%d): %m"),
	  PROMPT_NONE, 0 },

	/* Rehashing dir header */
	{ PR_3A_OPTIMIZE_DIR_HEADER,
	  N_("Optimizing directories: "),
	  PROMPT_NONE, PR_MSG_ONLY },

	/* Rehashing directory %d */
	{ PR_3A_OPTIMIZE_DIR,
	  " %d",
	  PROMPT_NONE, PR_LATCH_OPTIMIZE_DIR | PR_PREEN_NOHDR},

	/* Rehashing dir end */
	{ PR_3A_OPTIMIZE_DIR_END,
	  "\n",
	  PROMPT_NONE, PR_PREEN_NOHDR },

	/* Pass 4 errors */

	/* Pass 4: Checking reference counts */
	{ PR_4_PASS_HEADER,
	  N_("Pass 4: Checking reference counts\n"),
	  PROMPT_NONE, 0 },

	/* Unattached zero-length inode */
	{ PR_4_ZERO_LEN_INODE,
	  N_("@u @z @i %i.  "),
	  PROMPT_CLEAR, PR_PREEN_OK|PR_NO_OK },

	/* Unattached inode */
	{ PR_4_UNATTACHED_INODE,
	  N_("@u @i %i\n"),
	  PROMPT_CONNECT, 0 },

	/* Inode ref count wrong */
	{ PR_4_BAD_REF_COUNT,
	  N_("@i %i ref count is %Il, @s %N.  "),
	  PROMPT_FIX, PR_PREEN_OK },

	{ PR_4_INCONSISTENT_COUNT,
	  N_("WARNING: PROGRAMMING BUG IN E2FSCK!\n"
	  "\tOR SOME BONEHEAD (YOU) IS CHECKING A MOUNTED (LIVE) FILESYSTEM.\n"
	  "@i_link_info[%i] is %N, @i.i_links_count is %Il.  "
	  "They @s the same!\n"),
	  PROMPT_NONE, 0 },

	/* Pass 5 errors */

	/* Pass 5: Checking group summary information */
	{ PR_5_PASS_HEADER,
	  N_("Pass 5: Checking @g summary information\n"),
	  PROMPT_NONE, 0 },

	/* Padding at end of inode bitmap is not set. */
	{ PR_5_INODE_BMAP_PADDING,
	  N_("Padding at end of @i @B is not set. "),
	  PROMPT_FIX, PR_PREEN_OK },

	/* Padding at end of block bitmap is not set. */
	{ PR_5_BLOCK_BMAP_PADDING,
	  N_("Padding at end of @b @B is not set. "),
	  PROMPT_FIX, PR_PREEN_OK },

	/* Block bitmap differences header */
	{ PR_5_BLOCK_BITMAP_HEADER,
	  N_("@b @B differences: "),
	  PROMPT_NONE, PR_PREEN_OK | PR_PREEN_NOMSG},

	/* Block not used, but marked in bitmap */
	{ PR_5_BLOCK_UNUSED,
	  " -%b",
	  PROMPT_NONE, PR_LATCH_BBITMAP | PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Block used, but not marked used in bitmap */
	{ PR_5_BLOCK_USED,
	  " +%b",
	  PROMPT_NONE, PR_LATCH_BBITMAP | PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Block bitmap differences end */
	{ PR_5_BLOCK_BITMAP_END,
	  "\n",
	  PROMPT_FIX, PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Inode bitmap differences header */
	{ PR_5_INODE_BITMAP_HEADER,
	  N_("@i @B differences: "),
	  PROMPT_NONE, PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Inode not used, but marked in bitmap */
	{ PR_5_INODE_UNUSED,
	  " -%i",
	  PROMPT_NONE, PR_LATCH_IBITMAP | PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Inode used, but not marked used in bitmap */
	{ PR_5_INODE_USED,
	  " +%i",
	  PROMPT_NONE, PR_LATCH_IBITMAP | PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Inode bitmap differences end */
	{ PR_5_INODE_BITMAP_END,
	  "\n",
	  PROMPT_FIX, PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Free inodes count for group wrong */
	{ PR_5_FREE_INODE_COUNT_GROUP,
	  N_("Free @is count wrong for @g #%g (%i, counted=%j).\n"),
	  PROMPT_FIX, PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Directories count for group wrong */
	{ PR_5_FREE_DIR_COUNT_GROUP,
	  N_("Directories count wrong for @g #%g (%i, counted=%j).\n"),
	  PROMPT_FIX, PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Free inodes count wrong */
	{ PR_5_FREE_INODE_COUNT,
	  N_("Free @is count wrong (%i, counted=%j).\n"),
	  PROMPT_FIX, PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Free blocks count for group wrong */
	{ PR_5_FREE_BLOCK_COUNT_GROUP,
	  N_("Free @bs count wrong for @g #%g (%b, counted=%c).\n"),
	  PROMPT_FIX, PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Free blocks count wrong */
	{ PR_5_FREE_BLOCK_COUNT,
	  N_("Free @bs count wrong (%b, counted=%c).\n"),
	  PROMPT_FIX, PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Programming error: bitmap endpoints don't match */
	{ PR_5_BMAP_ENDPOINTS,
	  N_("PROGRAMMING ERROR: @f (#%N) @B endpoints (%b, %c) don't "
	  "match calculated @B endpoints (%i, %j)\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Internal error: fudging end of bitmap */
	{ PR_5_FUDGE_BITMAP_ERROR,
	  N_("Internal error: fudging end of bitmap (%N)\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error copying in replacement inode bitmap */
	{ PR_5_COPY_IBITMAP_ERROR,
	  N_("Error copying in replacement @i @B: %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Error copying in replacement block bitmap */
	{ PR_5_COPY_BBITMAP_ERROR,
	  N_("Error copying in replacement @b @B: %m\n"),
	  PROMPT_NONE, PR_FATAL },

	/* Block range not used, but marked in bitmap */
	{ PR_5_BLOCK_RANGE_UNUSED,
	  " -(%b--%c)",
	  PROMPT_NONE, PR_LATCH_BBITMAP | PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Block range used, but not marked used in bitmap */
	{ PR_5_BLOCK_RANGE_USED,
	  " +(%b--%c)",
	  PROMPT_NONE, PR_LATCH_BBITMAP | PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Inode range not used, but marked in bitmap */
	{ PR_5_INODE_RANGE_UNUSED,
	  " -(%i--%j)",
	  PROMPT_NONE, PR_LATCH_IBITMAP | PR_PREEN_OK | PR_PREEN_NOMSG },

	/* Inode range used, but not marked used in bitmap */
	{ PR_5_INODE_RANGE_USED,
	  " +(%i--%j)",
	  PROMPT_NONE, PR_LATCH_IBITMAP | PR_PREEN_OK | PR_PREEN_NOMSG },

	{ 0 }
};

/*
 * This is the latch flags register.  It allows several problems to be
 * "latched" together.  This means that the user has to answer but one
 * question for the set of problems, and all of the associated
 * problems will be either fixed or not fixed.
 */
static struct latch_descr pr_latch_info[] = {
	{ PR_LATCH_BLOCK, PR_1_INODE_BLOCK_LATCH, 0 },
	{ PR_LATCH_BBLOCK, PR_1_INODE_BBLOCK_LATCH, 0 },
	{ PR_LATCH_IBITMAP, PR_5_INODE_BITMAP_HEADER, PR_5_INODE_BITMAP_END },
	{ PR_LATCH_BBITMAP, PR_5_BLOCK_BITMAP_HEADER, PR_5_BLOCK_BITMAP_END },
	{ PR_LATCH_RELOC, PR_0_RELOCATE_HINT, 0 },
	{ PR_LATCH_DBLOCK, PR_1B_DUP_BLOCK_HEADER, PR_1B_DUP_BLOCK_END },
	{ PR_LATCH_LOW_DTIME, PR_1_ORPHAN_LIST_REFUGEES, 0 },
	{ PR_LATCH_TOOBIG, PR_1_INODE_TOOBIG, 0 },
	{ PR_LATCH_OPTIMIZE_DIR, PR_3A_OPTIMIZE_DIR_HEADER, PR_3A_OPTIMIZE_DIR_END },
	{ -1, 0, 0 },
};

static const struct e2fsck_problem *find_problem(problem_t code)
{
	int     i;

	for (i=0; problem_table[i].e2p_code; i++) {
		if (problem_table[i].e2p_code == code)
			return &problem_table[i];
	}
	return 0;
}

static struct latch_descr *find_latch(int code)
{
	int     i;

	for (i=0; pr_latch_info[i].latch_code >= 0; i++) {
		if (pr_latch_info[i].latch_code == code)
			return &pr_latch_info[i];
	}
	return 0;
}

int end_problem_latch(e2fsck_t ctx, int mask)
{
	struct latch_descr *ldesc;
	struct problem_context pctx;
	int answer = -1;

	ldesc = find_latch(mask);
	if (ldesc->end_message && (ldesc->flags & PRL_LATCHED)) {
		clear_problem_context(&pctx);
		answer = fix_problem(ctx, ldesc->end_message, &pctx);
	}
	ldesc->flags &= ~(PRL_VARIABLE);
	return answer;
}

int set_latch_flags(int mask, int setflags, int clearflags)
{
	struct latch_descr *ldesc;

	ldesc = find_latch(mask);
	if (!ldesc)
		return -1;
	ldesc->flags |= setflags;
	ldesc->flags &= ~clearflags;
	return 0;
}

void clear_problem_context(struct problem_context *ctx)
{
	memset(ctx, 0, sizeof(struct problem_context));
	ctx->blkcount = -1;
	ctx->group = -1;
}

int fix_problem(e2fsck_t ctx, problem_t code, struct problem_context *pctx)
{
	ext2_filsys fs = ctx->fs;
	const struct e2fsck_problem *ptr;
	struct latch_descr *ldesc = NULL;
	const char *message;
	int             def_yn, answer, ans;
	int             print_answer = 0;
	int             suppress = 0;

	ptr = find_problem(code);
	if (!ptr) {
		printf(_("Unhandled error code (0x%x)!\n"), code);
		return 0;
	}
	def_yn = 1;
	if ((ptr->flags & PR_NO_DEFAULT) ||
	    ((ptr->flags & PR_PREEN_NO) && (ctx->options & E2F_OPT_PREEN)) ||
	    (ctx->options & E2F_OPT_NO))
		def_yn= 0;

	/*
	 * Do special latch processing.  This is where we ask the
	 * latch question, if it exists
	 */
	if (ptr->flags & PR_LATCH_MASK) {
		ldesc = find_latch(ptr->flags & PR_LATCH_MASK);
		if (ldesc->question && !(ldesc->flags & PRL_LATCHED)) {
			ans = fix_problem(ctx, ldesc->question, pctx);
			if (ans == 1)
				ldesc->flags |= PRL_YES;
			if (ans == 0)
				ldesc->flags |= PRL_NO;
			ldesc->flags |= PRL_LATCHED;
		}
		if (ldesc->flags & PRL_SUPPRESS)
			suppress++;
	}
	if ((ptr->flags & PR_PREEN_NOMSG) &&
	    (ctx->options & E2F_OPT_PREEN))
		suppress++;
	if ((ptr->flags & PR_NO_NOMSG) &&
	    (ctx->options & E2F_OPT_NO))
		suppress++;
	if (!suppress) {
		message = ptr->e2p_description;
		if ((ctx->options & E2F_OPT_PREEN) &&
		    !(ptr->flags & PR_PREEN_NOHDR)) {
			printf("%s: ", ctx->device_name ?
			       ctx->device_name : ctx->filesystem_name);
		}
		if (*message)
			print_e2fsck_message(ctx, _(message), pctx, 1);
	}
	if (!(ptr->flags & PR_PREEN_OK) && (ptr->prompt != PROMPT_NONE))
		preenhalt(ctx);

	if (ptr->flags & PR_FATAL)
		bb_error_msg_and_die(0);

	if (ptr->prompt == PROMPT_NONE) {
		if (ptr->flags & PR_NOCOLLATE)
			answer = -1;
		else
			answer = def_yn;
	} else {
		if (ctx->options & E2F_OPT_PREEN) {
			answer = def_yn;
			if (!(ptr->flags & PR_PREEN_NOMSG))
				print_answer = 1;
		} else if ((ptr->flags & PR_LATCH_MASK) &&
			   (ldesc->flags & (PRL_YES | PRL_NO))) {
			if (!suppress)
				print_answer = 1;
			if (ldesc->flags & PRL_YES)
				answer = 1;
			else
				answer = 0;
		} else
			answer = ask(ctx, _(prompt[(int) ptr->prompt]), def_yn);
		if (!answer && !(ptr->flags & PR_NO_OK))
			ext2fs_unmark_valid(fs);

		if (print_answer)
			printf("%s.\n", answer ?
			       _(preen_msg[(int) ptr->prompt]) : _("IGNORED"));
	}

	if ((ptr->prompt == PROMPT_ABORT) && answer)
		bb_error_msg_and_die(0);

	if (ptr->flags & PR_AFTER_CODE)
		answer = fix_problem(ctx, ptr->second_code, pctx);

	return answer;
}

/*
 * linux/fs/recovery.c
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1999
 */

/*
 * Maintain information about the progress of the recovery job, so that
 * the different passes can carry information between them.
 */
struct recovery_info
{
	tid_t           start_transaction;
	tid_t           end_transaction;

	int             nr_replays;
	int             nr_revokes;
	int             nr_revoke_hits;
};

enum passtype {PASS_SCAN, PASS_REVOKE, PASS_REPLAY};
static int do_one_pass(journal_t *journal,
				struct recovery_info *info, enum passtype pass);
static int scan_revoke_records(journal_t *, struct buffer_head *,
				tid_t, struct recovery_info *);

/*
 * Read a block from the journal
 */

static int jread(struct buffer_head **bhp, journal_t *journal,
		 unsigned int offset)
{
	int err;
	unsigned long blocknr;
	struct buffer_head *bh;

	*bhp = NULL;

	err = journal_bmap(journal, offset, &blocknr);

	if (err) {
		printf("JBD: bad block at offset %u\n", offset);
		return err;
	}

	bh = getblk(journal->j_dev, blocknr, journal->j_blocksize);
	if (!bh)
		return -ENOMEM;

	if (!buffer_uptodate(bh)) {
		/* If this is a brand new buffer, start readahead.
		   Otherwise, we assume we are already reading it.  */
		if (!buffer_req(bh))
			do_readahead(journal, offset);
		wait_on_buffer(bh);
	}

	if (!buffer_uptodate(bh)) {
		printf("JBD: Failed to read block at offset %u\n", offset);
		brelse(bh);
		return -EIO;
	}

	*bhp = bh;
	return 0;
}


/*
 * Count the number of in-use tags in a journal descriptor block.
 */

static int count_tags(struct buffer_head *bh, int size)
{
	char *                  tagp;
	journal_block_tag_t *   tag;
	int                     nr = 0;

	tagp = &bh->b_data[sizeof(journal_header_t)];

	while ((tagp - bh->b_data + sizeof(journal_block_tag_t)) <= size) {
		tag = (journal_block_tag_t *) tagp;

		nr++;
		tagp += sizeof(journal_block_tag_t);
		if (!(tag->t_flags & htonl(JFS_FLAG_SAME_UUID)))
			tagp += 16;

		if (tag->t_flags & htonl(JFS_FLAG_LAST_TAG))
			break;
	}

	return nr;
}


/* Make sure we wrap around the log correctly! */
#define wrap(journal, var)					      \
do {					                            \
	if (var >= (journal)->j_last)                                   \
		var -= ((journal)->j_last - (journal)->j_first);        \
} while (0)

/**
 * int journal_recover(journal_t *journal) - recovers a on-disk journal
 * @journal: the journal to recover
 *
 * The primary function for recovering the log contents when mounting a
 * journaled device.
 *
 * Recovery is done in three passes.  In the first pass, we look for the
 * end of the log.  In the second, we assemble the list of revoke
 * blocks.  In the third and final pass, we replay any un-revoked blocks
 * in the log.
 */
int journal_recover(journal_t *journal)
{
	int                     err;
	journal_superblock_t *  sb;

	struct recovery_info    info;

	memset(&info, 0, sizeof(info));
	sb = journal->j_superblock;

	/*
	 * The journal superblock's s_start field (the current log head)
	 * is always zero if, and only if, the journal was cleanly
	 * unmounted.
	 */

	if (!sb->s_start) {
		journal->j_transaction_sequence = ntohl(sb->s_sequence) + 1;
		return 0;
	}

	err = do_one_pass(journal, &info, PASS_SCAN);
	if (!err)
		err = do_one_pass(journal, &info, PASS_REVOKE);
	if (!err)
		err = do_one_pass(journal, &info, PASS_REPLAY);

	/* Restart the log at the next transaction ID, thus invalidating
	 * any existing commit records in the log. */
	journal->j_transaction_sequence = ++info.end_transaction;

	journal_clear_revoke(journal);
	sync_blockdev(journal->j_fs_dev);
	return err;
}

static int do_one_pass(journal_t *journal,
			struct recovery_info *info, enum passtype pass)
{
	unsigned int            first_commit_ID, next_commit_ID;
	unsigned long           next_log_block;
	int                     err, success = 0;
	journal_superblock_t *  sb;
	journal_header_t *      tmp;
	struct buffer_head *    bh;
	unsigned int            sequence;
	int                     blocktype;

	/* Precompute the maximum metadata descriptors in a descriptor block */
	int                     MAX_BLOCKS_PER_DESC;
	MAX_BLOCKS_PER_DESC = ((journal->j_blocksize-sizeof(journal_header_t))
			       / sizeof(journal_block_tag_t));

	/*
	 * First thing is to establish what we expect to find in the log
	 * (in terms of transaction IDs), and where (in terms of log
	 * block offsets): query the superblock.
	 */

	sb = journal->j_superblock;
	next_commit_ID = ntohl(sb->s_sequence);
	next_log_block = ntohl(sb->s_start);

	first_commit_ID = next_commit_ID;
	if (pass == PASS_SCAN)
		info->start_transaction = first_commit_ID;

	/*
	 * Now we walk through the log, transaction by transaction,
	 * making sure that each transaction has a commit block in the
	 * expected place.  Each complete transaction gets replayed back
	 * into the main filesystem.
	 */

	while (1) {
		int                     flags;
		char *                  tagp;
		journal_block_tag_t *   tag;
		struct buffer_head *    obh;
		struct buffer_head *    nbh;

		/* If we already know where to stop the log traversal,
		 * check right now that we haven't gone past the end of
		 * the log. */

		if (pass != PASS_SCAN)
			if (tid_geq(next_commit_ID, info->end_transaction))
				break;

		/* Skip over each chunk of the transaction looking
		 * either the next descriptor block or the final commit
		 * record. */

		err = jread(&bh, journal, next_log_block);
		if (err)
			goto failed;

		next_log_block++;
		wrap(journal, next_log_block);

		/* What kind of buffer is it?
		 *
		 * If it is a descriptor block, check that it has the
		 * expected sequence number.  Otherwise, we're all done
		 * here. */

		tmp = (journal_header_t *)bh->b_data;

		if (tmp->h_magic != htonl(JFS_MAGIC_NUMBER)) {
			brelse(bh);
			break;
		}

		blocktype = ntohl(tmp->h_blocktype);
		sequence = ntohl(tmp->h_sequence);

		if (sequence != next_commit_ID) {
			brelse(bh);
			break;
		}

		/* OK, we have a valid descriptor block which matches
		 * all of the sequence number checks.  What are we going
		 * to do with it?  That depends on the pass... */

		switch (blocktype) {
		case JFS_DESCRIPTOR_BLOCK:
			/* If it is a valid descriptor block, replay it
			 * in pass REPLAY; otherwise, just skip over the
			 * blocks it describes. */
			if (pass != PASS_REPLAY) {
				next_log_block +=
					count_tags(bh, journal->j_blocksize);
				wrap(journal, next_log_block);
				brelse(bh);
				continue;
			}

			/* A descriptor block: we can now write all of
			 * the data blocks.  Yay, useful work is finally
			 * getting done here! */

			tagp = &bh->b_data[sizeof(journal_header_t)];
			while ((tagp - bh->b_data +sizeof(journal_block_tag_t))
			       <= journal->j_blocksize) {
				unsigned long io_block;

				tag = (journal_block_tag_t *) tagp;
				flags = ntohl(tag->t_flags);

				io_block = next_log_block++;
				wrap(journal, next_log_block);
				err = jread(&obh, journal, io_block);
				if (err) {
					/* Recover what we can, but
					 * report failure at the end. */
					success = err;
					printf("JBD: IO error %d recovering "
						"block %ld in log\n",
						err, io_block);
				} else {
					unsigned long blocknr;

					blocknr = ntohl(tag->t_blocknr);

					/* If the block has been
					 * revoked, then we're all done
					 * here. */
					if (journal_test_revoke
					    (journal, blocknr,
					     next_commit_ID)) {
						brelse(obh);
						++info->nr_revoke_hits;
						goto skip_write;
					}

					/* Find a buffer for the new
					 * data being restored */
					nbh = getblk(journal->j_fs_dev,
						       blocknr,
						     journal->j_blocksize);
					if (nbh == NULL) {
						printf("JBD: Out of memory "
						       "during recovery.\n");
						err = -ENOMEM;
						brelse(bh);
						brelse(obh);
						goto failed;
					}

					lock_buffer(nbh);
					memcpy(nbh->b_data, obh->b_data,
							journal->j_blocksize);
					if (flags & JFS_FLAG_ESCAPE) {
						*((unsigned int *)bh->b_data) =
							htonl(JFS_MAGIC_NUMBER);
					}

					mark_buffer_uptodate(nbh, 1);
					mark_buffer_dirty(nbh);
					++info->nr_replays;
					/* ll_rw_block(WRITE, 1, &nbh); */
					unlock_buffer(nbh);
					brelse(obh);
					brelse(nbh);
				}

			skip_write:
				tagp += sizeof(journal_block_tag_t);
				if (!(flags & JFS_FLAG_SAME_UUID))
					tagp += 16;

				if (flags & JFS_FLAG_LAST_TAG)
					break;
			}

			brelse(bh);
			continue;

		case JFS_COMMIT_BLOCK:
			/* Found an expected commit block: not much to
			 * do other than move on to the next sequence
			 * number. */
			brelse(bh);
			next_commit_ID++;
			continue;

		case JFS_REVOKE_BLOCK:
			/* If we aren't in the REVOKE pass, then we can
			 * just skip over this block. */
			if (pass != PASS_REVOKE) {
				brelse(bh);
				continue;
			}

			err = scan_revoke_records(journal, bh,
						  next_commit_ID, info);
			brelse(bh);
			if (err)
				goto failed;
			continue;

		default:
			goto done;
		}
	}

 done:
	/*
	 * We broke out of the log scan loop: either we came to the
	 * known end of the log or we found an unexpected block in the
	 * log.  If the latter happened, then we know that the "current"
	 * transaction marks the end of the valid log.
	 */

	if (pass == PASS_SCAN)
		info->end_transaction = next_commit_ID;
	else {
		/* It's really bad news if different passes end up at
		 * different places (but possible due to IO errors). */
		if (info->end_transaction != next_commit_ID) {
			printf("JBD: recovery pass %d ended at "
				"transaction %u, expected %u\n",
				pass, next_commit_ID, info->end_transaction);
			if (!success)
				success = -EIO;
		}
	}

	return success;

 failed:
	return err;
}


/* Scan a revoke record, marking all blocks mentioned as revoked. */

static int scan_revoke_records(journal_t *journal, struct buffer_head *bh,
			       tid_t sequence, struct recovery_info *info)
{
	journal_revoke_header_t *header;
	int offset, max;

	header = (journal_revoke_header_t *) bh->b_data;
	offset = sizeof(journal_revoke_header_t);
	max = ntohl(header->r_count);

	while (offset < max) {
		unsigned long blocknr;
		int err;

		blocknr = ntohl(* ((unsigned int *) (bh->b_data+offset)));
		offset += 4;
		err = journal_set_revoke(journal, blocknr, sequence);
		if (err)
			return err;
		++info->nr_revokes;
	}
	return 0;
}


/*
 * rehash.c --- rebuild hash tree directories
 *
 * This algorithm is designed for simplicity of implementation and to
 * pack the directory as much as possible.  It however requires twice
 * as much memory as the size of the directory.  The maximum size
 * directory supported using a 4k blocksize is roughly a gigabyte, and
 * so there may very well be problems with machines that don't have
 * virtual memory, and obscenely large directories.
 *
 * An alternate algorithm which is much more disk intensive could be
 * written, and probably will need to be written in the future.  The
 * design goals of such an algorithm are: (a) use (roughly) constant
 * amounts of memory, no matter how large the directory, (b) the
 * directory must be safe at all times, even if e2fsck is interrupted
 * in the middle, (c) we must use minimal amounts of extra disk
 * blocks.  This pretty much requires an incremental approach, where
 * we are reading from one part of the directory, and inserting into
 * the front half.  So the algorithm will have to keep track of a
 * moving block boundary between the new tree and the old tree, and
 * files will need to be moved from the old directory and inserted
 * into the new tree.  If the new directory requires space which isn't
 * yet available, blocks from the beginning part of the old directory
 * may need to be moved to the end of the directory to make room for
 * the new tree:
 *
 *    --------------------------------------------------------
 *    |  new tree   |        | old tree                      |
 *    --------------------------------------------------------
 *                  ^ ptr    ^ptr
 *                tail new   head old
 *
 * This is going to be a pain in the tuckus to implement, and will
 * require a lot more disk accesses.  So I'm going to skip it for now;
 * it's only really going to be an issue for really, really big
 * filesystems (when we reach the level of tens of millions of files
 * in a single directory).  It will probably be easier to simply
 * require that e2fsck use VM first.
 */

struct fill_dir_struct {
	char *buf;
	struct ext2_inode *inode;
	int err;
	e2fsck_t ctx;
	struct hash_entry *harray;
	int max_array, num_array;
	int dir_size;
	int compress;
	ino_t parent;
};

struct hash_entry {
	ext2_dirhash_t  hash;
	ext2_dirhash_t  minor_hash;
	struct ext2_dir_entry   *dir;
};

struct out_dir {
	int             num;
	int             max;
	char            *buf;
	ext2_dirhash_t  *hashes;
};

static int fill_dir_block(ext2_filsys fs,
			  blk_t *block_nr,
			  e2_blkcnt_t blockcnt,
			  blk_t ref_block FSCK_ATTR((unused)),
			  int ref_offset FSCK_ATTR((unused)),
			  void *priv_data)
{
	struct fill_dir_struct  *fd = (struct fill_dir_struct *) priv_data;
	struct hash_entry       *new_array, *ent;
	struct ext2_dir_entry   *dirent;
	char                    *dir;
	unsigned int            offset, dir_offset;

	if (blockcnt < 0)
		return 0;

	offset = blockcnt * fs->blocksize;
	if (offset + fs->blocksize > fd->inode->i_size) {
		fd->err = EXT2_ET_DIR_CORRUPTED;
		return BLOCK_ABORT;
	}
	dir = (fd->buf+offset);
	if (HOLE_BLKADDR(*block_nr)) {
		memset(dir, 0, fs->blocksize);
		dirent = (struct ext2_dir_entry *) dir;
		dirent->rec_len = fs->blocksize;
	} else {
		fd->err = ext2fs_read_dir_block(fs, *block_nr, dir);
		if (fd->err)
			return BLOCK_ABORT;
	}
	/* While the directory block is "hot", index it. */
	dir_offset = 0;
	while (dir_offset < fs->blocksize) {
		dirent = (struct ext2_dir_entry *) (dir + dir_offset);
		if (((dir_offset + dirent->rec_len) > fs->blocksize) ||
		    (dirent->rec_len < 8) ||
		    ((dirent->rec_len % 4) != 0) ||
		    (((dirent->name_len & 0xFF)+8) > dirent->rec_len)) {
			fd->err = EXT2_ET_DIR_CORRUPTED;
			return BLOCK_ABORT;
		}
		dir_offset += dirent->rec_len;
		if (dirent->inode == 0)
			continue;
		if (!fd->compress && ((dirent->name_len&0xFF) == 1) &&
		    (dirent->name[0] == '.'))
			continue;
		if (!fd->compress && ((dirent->name_len&0xFF) == 2) &&
		    (dirent->name[0] == '.') && (dirent->name[1] == '.')) {
			fd->parent = dirent->inode;
			continue;
		}
		if (fd->num_array >= fd->max_array) {
			new_array = xrealloc(fd->harray,
			    sizeof(struct hash_entry) * (fd->max_array+500));
			fd->harray = new_array;
			fd->max_array += 500;
		}
		ent = fd->harray + fd->num_array++;
		ent->dir = dirent;
		fd->dir_size += EXT2_DIR_REC_LEN(dirent->name_len & 0xFF);
		if (fd->compress)
			ent->hash = ent->minor_hash = 0;
		else {
			fd->err = ext2fs_dirhash(fs->super->s_def_hash_version,
						 dirent->name,
						 dirent->name_len & 0xFF,
						 fs->super->s_hash_seed,
						 &ent->hash, &ent->minor_hash);
			if (fd->err)
				return BLOCK_ABORT;
		}
	}

	return 0;
}

/* Used for sorting the hash entry */
static int name_cmp(const void *a, const void *b)
{
	const struct hash_entry *he_a = (const struct hash_entry *) a;
	const struct hash_entry *he_b = (const struct hash_entry *) b;
	int     ret;
	int     min_len;

	min_len = he_a->dir->name_len;
	if (min_len > he_b->dir->name_len)
		min_len = he_b->dir->name_len;

	ret = strncmp(he_a->dir->name, he_b->dir->name, min_len);
	if (ret == 0) {
		if (he_a->dir->name_len > he_b->dir->name_len)
			ret = 1;
		else if (he_a->dir->name_len < he_b->dir->name_len)
			ret = -1;
		else
			ret = he_b->dir->inode - he_a->dir->inode;
	}
	return ret;
}

/* Used for sorting the hash entry */
static int hash_cmp(const void *a, const void *b)
{
	const struct hash_entry *he_a = (const struct hash_entry *) a;
	const struct hash_entry *he_b = (const struct hash_entry *) b;
	int     ret;

	if (he_a->hash > he_b->hash)
		ret = 1;
	else if (he_a->hash < he_b->hash)
		ret = -1;
	else {
		if (he_a->minor_hash > he_b->minor_hash)
			ret = 1;
		else if (he_a->minor_hash < he_b->minor_hash)
			ret = -1;
		else
			ret = name_cmp(a, b);
	}
	return ret;
}

static errcode_t alloc_size_dir(ext2_filsys fs, struct out_dir *outdir,
				int blocks)
{
	void                    *new_mem;

	if (outdir->max) {
		new_mem = xrealloc(outdir->buf, blocks * fs->blocksize);
		outdir->buf = new_mem;
		new_mem = xrealloc(outdir->hashes,
				  blocks * sizeof(ext2_dirhash_t));
		outdir->hashes = new_mem;
	} else {
		outdir->buf = xmalloc(blocks * fs->blocksize);
		outdir->hashes = xmalloc(blocks * sizeof(ext2_dirhash_t));
		outdir->num = 0;
	}
	outdir->max = blocks;
	return 0;
}

static void free_out_dir(struct out_dir *outdir)
{
	free(outdir->buf);
	free(outdir->hashes);
	outdir->max = 0;
	outdir->num =0;
}

static errcode_t get_next_block(ext2_filsys fs, struct out_dir *outdir,
			 char ** ret)
{
	errcode_t       retval;

	if (outdir->num >= outdir->max) {
		retval = alloc_size_dir(fs, outdir, outdir->max + 50);
		if (retval)
			return retval;
	}
	*ret = outdir->buf + (outdir->num++ * fs->blocksize);
	memset(*ret, 0, fs->blocksize);
	return 0;
}

/*
 * This function is used to make a unique filename.  We do this by
 * appending ~0, and then incrementing the number.  However, we cannot
 * expand the length of the filename beyond the padding available in
 * the directory entry.
 */
static void mutate_name(char *str, __u16 *len)
{
	int     i;
	__u16   l = *len & 0xFF, h = *len & 0xff00;

	/*
	 * First check to see if it looks the name has been mutated
	 * already
	 */
	for (i = l-1; i > 0; i--) {
		if (!isdigit(str[i]))
			break;
	}
	if ((i == l-1) || (str[i] != '~')) {
		if (((l-1) & 3) < 2)
			l += 2;
		else
			l = (l+3) & ~3;
		str[l-2] = '~';
		str[l-1] = '0';
		*len = l | h;
		return;
	}
	for (i = l-1; i >= 0; i--) {
		if (isdigit(str[i])) {
			if (str[i] == '9')
				str[i] = '0';
			else {
				str[i]++;
				return;
			}
			continue;
		}
		if (i == 1) {
			if (str[0] == 'z')
				str[0] = 'A';
			else if (str[0] == 'Z') {
				str[0] = '~';
				str[1] = '0';
			} else
				str[0]++;
		} else if (i > 0) {
			str[i] = '1';
			str[i-1] = '~';
		} else {
			if (str[0] == '~')
				str[0] = 'a';
			else
				str[0]++;
		}
		break;
	}
}

static int duplicate_search_and_fix(e2fsck_t ctx, ext2_filsys fs,
				    ext2_ino_t ino,
				    struct fill_dir_struct *fd)
{
	struct problem_context  pctx;
	struct hash_entry       *ent, *prev;
	int                     i, j;
	int                     fixed = 0;
	char                    new_name[256];
	__u16                   new_len;

	clear_problem_context(&pctx);
	pctx.ino = ino;

	for (i=1; i < fd->num_array; i++) {
		ent = fd->harray + i;
		prev = ent - 1;
		if (!ent->dir->inode ||
		    ((ent->dir->name_len & 0xFF) !=
		     (prev->dir->name_len & 0xFF)) ||
		    (strncmp(ent->dir->name, prev->dir->name,
			     ent->dir->name_len & 0xFF)))
			continue;
		pctx.dirent = ent->dir;
		if ((ent->dir->inode == prev->dir->inode) &&
		    fix_problem(ctx, PR_2_DUPLICATE_DIRENT, &pctx)) {
			e2fsck_adjust_inode_count(ctx, ent->dir->inode, -1);
			ent->dir->inode = 0;
			fixed++;
			continue;
		}
		memcpy(new_name, ent->dir->name, ent->dir->name_len & 0xFF);
		new_len = ent->dir->name_len;
		mutate_name(new_name, &new_len);
		for (j=0; j < fd->num_array; j++) {
			if ((i==j) ||
			    ((ent->dir->name_len & 0xFF) !=
			     (fd->harray[j].dir->name_len & 0xFF)) ||
			    (strncmp(new_name, fd->harray[j].dir->name,
				     new_len & 0xFF)))
				continue;
			mutate_name(new_name, &new_len);

			j = -1;
		}
		new_name[new_len & 0xFF] = 0;
		pctx.str = new_name;
		if (fix_problem(ctx, PR_2_NON_UNIQUE_FILE, &pctx)) {
			memcpy(ent->dir->name, new_name, new_len & 0xFF);
			ent->dir->name_len = new_len;
			ext2fs_dirhash(fs->super->s_def_hash_version,
				       ent->dir->name,
				       ent->dir->name_len & 0xFF,
				       fs->super->s_hash_seed,
				       &ent->hash, &ent->minor_hash);
			fixed++;
		}
	}
	return fixed;
}


static errcode_t copy_dir_entries(ext2_filsys fs,
				  struct fill_dir_struct *fd,
				  struct out_dir *outdir)
{
	errcode_t               retval;
	char                    *block_start;
	struct hash_entry       *ent;
	struct ext2_dir_entry   *dirent;
	int                     i, rec_len, left;
	ext2_dirhash_t          prev_hash;
	int                     offset;

	outdir->max = 0;
	retval = alloc_size_dir(fs, outdir,
				(fd->dir_size / fs->blocksize) + 2);
	if (retval)
		return retval;
	outdir->num = fd->compress ? 0 : 1;
	offset = 0;
	outdir->hashes[0] = 0;
	prev_hash = 1;
	if ((retval = get_next_block(fs, outdir, &block_start)))
		return retval;
	dirent = (struct ext2_dir_entry *) block_start;
	left = fs->blocksize;
	for (i=0; i < fd->num_array; i++) {
		ent = fd->harray + i;
		if (ent->dir->inode == 0)
			continue;
		rec_len = EXT2_DIR_REC_LEN(ent->dir->name_len & 0xFF);
		if (rec_len > left) {
			if (left)
				dirent->rec_len += left;
			if ((retval = get_next_block(fs, outdir,
						      &block_start)))
				return retval;
			offset = 0;
		}
		left = fs->blocksize - offset;
		dirent = (struct ext2_dir_entry *) (block_start + offset);
		if (offset == 0) {
			if (ent->hash == prev_hash)
				outdir->hashes[outdir->num-1] = ent->hash | 1;
			else
				outdir->hashes[outdir->num-1] = ent->hash;
		}
		dirent->inode = ent->dir->inode;
		dirent->name_len = ent->dir->name_len;
		dirent->rec_len = rec_len;
		memcpy(dirent->name, ent->dir->name, dirent->name_len & 0xFF);
		offset += rec_len;
		left -= rec_len;
		if (left < 12) {
			dirent->rec_len += left;
			offset += left;
			left = 0;
		}
		prev_hash = ent->hash;
	}
	if (left)
		dirent->rec_len += left;

	return 0;
}


static struct ext2_dx_root_info *set_root_node(ext2_filsys fs, char *buf,
				    ext2_ino_t ino, ext2_ino_t parent)
{
	struct ext2_dir_entry           *dir;
	struct ext2_dx_root_info        *root;
	struct ext2_dx_countlimit       *limits;
	int                             filetype = 0;

	if (fs->super->s_feature_incompat & EXT2_FEATURE_INCOMPAT_FILETYPE)
		filetype = EXT2_FT_DIR << 8;

	memset(buf, 0, fs->blocksize);
	dir = (struct ext2_dir_entry *) buf;
	dir->inode = ino;
	dir->name[0] = '.';
	dir->name_len = 1 | filetype;
	dir->rec_len = 12;
	dir = (struct ext2_dir_entry *) (buf + 12);
	dir->inode = parent;
	dir->name[0] = '.';
	dir->name[1] = '.';
	dir->name_len = 2 | filetype;
	dir->rec_len = fs->blocksize - 12;

	root = (struct ext2_dx_root_info *) (buf+24);
	root->reserved_zero = 0;
	root->hash_version = fs->super->s_def_hash_version;
	root->info_length = 8;
	root->indirect_levels = 0;
	root->unused_flags = 0;

	limits = (struct ext2_dx_countlimit *) (buf+32);
	limits->limit = (fs->blocksize - 32) / sizeof(struct ext2_dx_entry);
	limits->count = 0;

	return root;
}


static struct ext2_dx_entry *set_int_node(ext2_filsys fs, char *buf)
{
	struct ext2_dir_entry           *dir;
	struct ext2_dx_countlimit       *limits;

	memset(buf, 0, fs->blocksize);
	dir = (struct ext2_dir_entry *) buf;
	dir->inode = 0;
	dir->rec_len = fs->blocksize;

	limits = (struct ext2_dx_countlimit *) (buf+8);
	limits->limit = (fs->blocksize - 8) / sizeof(struct ext2_dx_entry);
	limits->count = 0;

	return (struct ext2_dx_entry *) limits;
}

/*
 * This function takes the leaf nodes which have been written in
 * outdir, and populates the root node and any necessary interior nodes.
 */
static errcode_t calculate_tree(ext2_filsys fs,
				struct out_dir *outdir,
				ext2_ino_t ino,
				ext2_ino_t parent)
{
	struct ext2_dx_root_info        *root_info;
	struct ext2_dx_entry            *root, *dx_ent = NULL;
	struct ext2_dx_countlimit       *root_limit, *limit;
	errcode_t                       retval;
	char                            * block_start;
	int                             i, c1, c2, nblks;
	int                             limit_offset, root_offset;

	root_info = set_root_node(fs, outdir->buf, ino, parent);
	root_offset = limit_offset = ((char *) root_info - outdir->buf) +
		root_info->info_length;
	root_limit = (struct ext2_dx_countlimit *) (outdir->buf + limit_offset);
	c1 = root_limit->limit;
	nblks = outdir->num;

	/* Write out the pointer blocks */
	if (nblks-1 <= c1) {
		/* Just write out the root block, and we're done */
		root = (struct ext2_dx_entry *) (outdir->buf + root_offset);
		for (i=1; i < nblks; i++) {
			root->block = ext2fs_cpu_to_le32(i);
			if (i != 1)
				root->hash =
					ext2fs_cpu_to_le32(outdir->hashes[i]);
			root++;
			c1--;
		}
	} else {
		c2 = 0;
		limit = 0;
		root_info->indirect_levels = 1;
		for (i=1; i < nblks; i++) {
			if (c1 == 0)
				return ENOSPC;
			if (c2 == 0) {
				if (limit)
					limit->limit = limit->count =
		ext2fs_cpu_to_le16(limit->limit);
				root = (struct ext2_dx_entry *)
					(outdir->buf + root_offset);
				root->block = ext2fs_cpu_to_le32(outdir->num);
				if (i != 1)
					root->hash =
			ext2fs_cpu_to_le32(outdir->hashes[i]);
				if ((retval =  get_next_block(fs, outdir,
							      &block_start)))
					return retval;
				dx_ent = set_int_node(fs, block_start);
				limit = (struct ext2_dx_countlimit *) dx_ent;
				c2 = limit->limit;
				root_offset += sizeof(struct ext2_dx_entry);
				c1--;
			}
			dx_ent->block = ext2fs_cpu_to_le32(i);
			if (c2 != limit->limit)
				dx_ent->hash =
					ext2fs_cpu_to_le32(outdir->hashes[i]);
			dx_ent++;
			c2--;
		}
		limit->count = ext2fs_cpu_to_le16(limit->limit - c2);
		limit->limit = ext2fs_cpu_to_le16(limit->limit);
	}
	root_limit = (struct ext2_dx_countlimit *) (outdir->buf + limit_offset);
	root_limit->count = ext2fs_cpu_to_le16(root_limit->limit - c1);
	root_limit->limit = ext2fs_cpu_to_le16(root_limit->limit);

	return 0;
}

struct write_dir_struct {
	struct out_dir *outdir;
	errcode_t       err;
	e2fsck_t        ctx;
	int             cleared;
};

/*
 * Helper function which writes out a directory block.
 */
static int write_dir_block(ext2_filsys fs,
			   blk_t        *block_nr,
			   e2_blkcnt_t blockcnt,
			   blk_t ref_block FSCK_ATTR((unused)),
			   int ref_offset FSCK_ATTR((unused)),
			   void *priv_data)
{
	struct write_dir_struct *wd = (struct write_dir_struct *) priv_data;
	blk_t   blk;
	char    *dir;

	if (*block_nr == 0)
		return 0;
	if (blockcnt >= wd->outdir->num) {
		e2fsck_read_bitmaps(wd->ctx);
		blk = *block_nr;
		ext2fs_unmark_block_bitmap(wd->ctx->block_found_map, blk);
		ext2fs_block_alloc_stats(fs, blk, -1);
		*block_nr = 0;
		wd->cleared++;
		return BLOCK_CHANGED;
	}
	if (blockcnt < 0)
		return 0;

	dir = wd->outdir->buf + (blockcnt * fs->blocksize);
	wd->err = ext2fs_write_dir_block(fs, *block_nr, dir);
	if (wd->err)
		return BLOCK_ABORT;
	return 0;
}

static errcode_t write_directory(e2fsck_t ctx, ext2_filsys fs,
				 struct out_dir *outdir,
				 ext2_ino_t ino, int compress)
{
	struct write_dir_struct wd;
	errcode_t       retval;
	struct ext2_inode       inode;

	retval = e2fsck_expand_directory(ctx, ino, -1, outdir->num);
	if (retval)
		return retval;

	wd.outdir = outdir;
	wd.err = 0;
	wd.ctx = ctx;
	wd.cleared = 0;

	retval = ext2fs_block_iterate2(fs, ino, 0, 0,
				       write_dir_block, &wd);
	if (retval)
		return retval;
	if (wd.err)
		return wd.err;

	e2fsck_read_inode(ctx, ino, &inode, "rehash_dir");
	if (compress)
		inode.i_flags &= ~EXT2_INDEX_FL;
	else
		inode.i_flags |= EXT2_INDEX_FL;
	inode.i_size = outdir->num * fs->blocksize;
	inode.i_blocks -= (fs->blocksize / 512) * wd.cleared;
	e2fsck_write_inode(ctx, ino, &inode, "rehash_dir");

	return 0;
}

static errcode_t e2fsck_rehash_dir(e2fsck_t ctx, ext2_ino_t ino)
{
	ext2_filsys             fs = ctx->fs;
	errcode_t               retval;
	struct ext2_inode       inode;
	char                    *dir_buf = NULL;
	struct fill_dir_struct  fd;
	struct out_dir          outdir;

	outdir.max = outdir.num = 0;
	outdir.buf = 0;
	outdir.hashes = 0;
	e2fsck_read_inode(ctx, ino, &inode, "rehash_dir");

	retval = ENOMEM;
	fd.harray = 0;
	dir_buf = xmalloc(inode.i_size);

	fd.max_array = inode.i_size / 32;
	fd.num_array = 0;
	fd.harray = xmalloc(fd.max_array * sizeof(struct hash_entry));

	fd.ctx = ctx;
	fd.buf = dir_buf;
	fd.inode = &inode;
	fd.err = 0;
	fd.dir_size = 0;
	fd.compress = 0;
	if (!(fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_DIR_INDEX) ||
	    (inode.i_size / fs->blocksize) < 2)
		fd.compress = 1;
	fd.parent = 0;

	/* Read in the entire directory into memory */
	retval = ext2fs_block_iterate2(fs, ino, 0, 0,
				       fill_dir_block, &fd);
	if (fd.err) {
		retval = fd.err;
		goto errout;
	}

	/* Sort the list */
resort:
	if (fd.compress)
		qsort(fd.harray+2, fd.num_array-2,
		      sizeof(struct hash_entry), name_cmp);
	else
		qsort(fd.harray, fd.num_array,
		      sizeof(struct hash_entry), hash_cmp);

	/*
	 * Look for duplicates
	 */
	if (duplicate_search_and_fix(ctx, fs, ino, &fd))
		goto resort;

	if (ctx->options & E2F_OPT_NO) {
		retval = 0;
		goto errout;
	}

	/*
	 * Copy the directory entries.  In a htree directory these
	 * will become the leaf nodes.
	 */
	retval = copy_dir_entries(fs, &fd, &outdir);
	if (retval)
		goto errout;

	free(dir_buf); dir_buf = 0;

	if (!fd.compress) {
		/* Calculate the interior nodes */
		retval = calculate_tree(fs, &outdir, ino, fd.parent);
		if (retval)
			goto errout;
	}

	retval = write_directory(ctx, fs, &outdir, ino, fd.compress);

errout:
	free(dir_buf);
	free(fd.harray);

	free_out_dir(&outdir);
	return retval;
}

void e2fsck_rehash_directories(e2fsck_t ctx)
{
	struct problem_context  pctx;
	struct dir_info         *dir;
	ext2_u32_iterate        iter;
	ext2_ino_t              ino;
	errcode_t               retval;
	int                     i, cur, max, all_dirs, dir_index, first = 1;

	all_dirs = ctx->options & E2F_OPT_COMPRESS_DIRS;

	if (!ctx->dirs_to_hash && !all_dirs)
		return;

	e2fsck_get_lost_and_found(ctx, 0);

	clear_problem_context(&pctx);

	dir_index = ctx->fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_DIR_INDEX;
	cur = 0;
	if (all_dirs) {
		i = 0;
		max = e2fsck_get_num_dirinfo(ctx);
	} else {
		retval = ext2fs_u32_list_iterate_begin(ctx->dirs_to_hash,
						       &iter);
		if (retval) {
			pctx.errcode = retval;
			fix_problem(ctx, PR_3A_OPTIMIZE_ITER, &pctx);
			return;
		}
		max = ext2fs_u32_list_count(ctx->dirs_to_hash);
	}
	while (1) {
		if (all_dirs) {
			if ((dir = e2fsck_dir_info_iter(ctx, &i)) == 0)
				break;
			ino = dir->ino;
		} else {
			if (!ext2fs_u32_list_iterate(iter, &ino))
				break;
		}
		if (ino == ctx->lost_and_found)
			continue;
		pctx.dir = ino;
		if (first) {
			fix_problem(ctx, PR_3A_PASS_HEADER, &pctx);
			first = 0;
		}
		pctx.errcode = e2fsck_rehash_dir(ctx, ino);
		if (pctx.errcode) {
			end_problem_latch(ctx, PR_LATCH_OPTIMIZE_DIR);
			fix_problem(ctx, PR_3A_OPTIMIZE_DIR_ERR, &pctx);
		}
		if (ctx->progress && !ctx->progress_fd)
			e2fsck_simple_progress(ctx, "Rebuilding directory",
			       100.0 * (float) (++cur) / (float) max, ino);
	}
	end_problem_latch(ctx, PR_LATCH_OPTIMIZE_DIR);
	if (!all_dirs)
		ext2fs_u32_list_iterate_end(iter);

	ext2fs_u32_list_free(ctx->dirs_to_hash);
	ctx->dirs_to_hash = 0;
}

/*
 * linux/fs/revoke.c
 *
 * Journal revoke routines for the generic filesystem journaling code;
 * part of the ext2fs journaling system.
 *
 * Revoke is the mechanism used to prevent old log records for deleted
 * metadata from being replayed on top of newer data using the same
 * blocks.  The revoke mechanism is used in two separate places:
 *
 * + Commit: during commit we write the entire list of the current
 *   transaction's revoked blocks to the journal
 *
 * + Recovery: during recovery we record the transaction ID of all
 *   revoked blocks.  If there are multiple revoke records in the log
 *   for a single block, only the last one counts, and if there is a log
 *   entry for a block beyond the last revoke, then that log entry still
 *   gets replayed.
 *
 * We can get interactions between revokes and new log data within a
 * single transaction:
 *
 * Block is revoked and then journaled:
 *   The desired end result is the journaling of the new block, so we
 *   cancel the revoke before the transaction commits.
 *
 * Block is journaled and then revoked:
 *   The revoke must take precedence over the write of the block, so we
 *   need either to cancel the journal entry or to write the revoke
 *   later in the log than the log block.  In this case, we choose the
 *   latter: journaling a block cancels any revoke record for that block
 *   in the current transaction, so any revoke for that block in the
 *   transaction must have happened after the block was journaled and so
 *   the revoke must take precedence.
 *
 * Block is revoked and then written as data:
 *   The data write is allowed to succeed, but the revoke is _not_
 *   cancelled.  We still need to prevent old log records from
 *   overwriting the new data.  We don't even need to clear the revoke
 *   bit here.
 *
 * Revoke information on buffers is a tri-state value:
 *
 * RevokeValid clear:   no cached revoke status, need to look it up
 * RevokeValid set, Revoked clear:
 *                      buffer has not been revoked, and cancel_revoke
 *                      need do nothing.
 * RevokeValid set, Revoked set:
 *                      buffer has been revoked.
 */

static kmem_cache_t *revoke_record_cache;
static kmem_cache_t *revoke_table_cache;

/* Each revoke record represents one single revoked block.  During
   journal replay, this involves recording the transaction ID of the
   last transaction to revoke this block. */

struct jbd_revoke_record_s
{
	struct list_head  hash;
	tid_t             sequence;     /* Used for recovery only */
	unsigned long     blocknr;
};


/* The revoke table is just a simple hash table of revoke records. */
struct jbd_revoke_table_s
{
	/* It is conceivable that we might want a larger hash table
	 * for recovery.  Must be a power of two. */
	int               hash_size;
	int               hash_shift;
	struct list_head *hash_table;
};


/* Utility functions to maintain the revoke table */

/* Borrowed from buffer.c: this is a tried and tested block hash function */
static int hash(journal_t *journal, unsigned long block)
{
	struct jbd_revoke_table_s *table = journal->j_revoke;
	int hash_shift = table->hash_shift;

	return ((block << (hash_shift - 6)) ^
		(block >> 13) ^
		(block << (hash_shift - 12))) & (table->hash_size - 1);
}

static int insert_revoke_hash(journal_t *journal, unsigned long blocknr,
			      tid_t seq)
{
	struct list_head *hash_list;
	struct jbd_revoke_record_s *record;

	record = kmem_cache_alloc(revoke_record_cache, GFP_NOFS);
	if (!record)
		goto oom;

	record->sequence = seq;
	record->blocknr = blocknr;
	hash_list = &journal->j_revoke->hash_table[hash(journal, blocknr)];
	list_add(&record->hash, hash_list);
	return 0;

oom:
	return -ENOMEM;
}

/* Find a revoke record in the journal's hash table. */

static struct jbd_revoke_record_s *find_revoke_record(journal_t *journal,
						      unsigned long blocknr)
{
	struct list_head *hash_list;
	struct jbd_revoke_record_s *record;

	hash_list = &journal->j_revoke->hash_table[hash(journal, blocknr)];

	record = (struct jbd_revoke_record_s *) hash_list->next;
	while (&(record->hash) != hash_list) {
		if (record->blocknr == blocknr)
			return record;
		record = (struct jbd_revoke_record_s *) record->hash.next;
	}
	return NULL;
}

int journal_init_revoke_caches(void)
{
	revoke_record_cache = do_cache_create(sizeof(struct jbd_revoke_record_s));
	if (revoke_record_cache == 0)
		return -ENOMEM;

	revoke_table_cache = do_cache_create(sizeof(struct jbd_revoke_table_s));
	if (revoke_table_cache == 0) {
		do_cache_destroy(revoke_record_cache);
		revoke_record_cache = NULL;
		return -ENOMEM;
	}
	return 0;
}

void journal_destroy_revoke_caches(void)
{
	do_cache_destroy(revoke_record_cache);
	revoke_record_cache = 0;
	do_cache_destroy(revoke_table_cache);
	revoke_table_cache = 0;
}

/* Initialise the revoke table for a given journal to a given size. */

int journal_init_revoke(journal_t *journal, int hash_size)
{
	int shift, tmp;

	journal->j_revoke = kmem_cache_alloc(revoke_table_cache, GFP_KERNEL);
	if (!journal->j_revoke)
		return -ENOMEM;

	/* Check that the hash_size is a power of two */
	journal->j_revoke->hash_size = hash_size;

	shift = 0;
	tmp = hash_size;
	while ((tmp >>= 1UL) != 0UL)
		shift++;
	journal->j_revoke->hash_shift = shift;

	journal->j_revoke->hash_table = xmalloc(hash_size * sizeof(struct list_head));

	for (tmp = 0; tmp < hash_size; tmp++)
		INIT_LIST_HEAD(&journal->j_revoke->hash_table[tmp]);

	return 0;
}

/* Destoy a journal's revoke table.  The table must already be empty! */

void journal_destroy_revoke(journal_t *journal)
{
	struct jbd_revoke_table_s *table;
	struct list_head *hash_list;
	int i;

	table = journal->j_revoke;
	if (!table)
		return;

	for (i=0; i<table->hash_size; i++) {
		hash_list = &table->hash_table[i];
	}

	free(table->hash_table);
	free(table);
	journal->j_revoke = NULL;
}

/*
 * Revoke support for recovery.
 *
 * Recovery needs to be able to:
 *
 *  record all revoke records, including the tid of the latest instance
 *  of each revoke in the journal
 *
 *  check whether a given block in a given transaction should be replayed
 *  (ie. has not been revoked by a revoke record in that or a subsequent
 *  transaction)
 *
 *  empty the revoke table after recovery.
 */

/*
 * First, setting revoke records.  We create a new revoke record for
 * every block ever revoked in the log as we scan it for recovery, and
 * we update the existing records if we find multiple revokes for a
 * single block.
 */

int journal_set_revoke(journal_t *journal, unsigned long blocknr,
		       tid_t sequence)
{
	struct jbd_revoke_record_s *record;

	record = find_revoke_record(journal, blocknr);
	if (record) {
		/* If we have multiple occurences, only record the
		 * latest sequence number in the hashed record */
		if (tid_gt(sequence, record->sequence))
			record->sequence = sequence;
		return 0;
	}
	return insert_revoke_hash(journal, blocknr, sequence);
}

/*
 * Test revoke records.  For a given block referenced in the log, has
 * that block been revoked?  A revoke record with a given transaction
 * sequence number revokes all blocks in that transaction and earlier
 * ones, but later transactions still need replayed.
 */

int journal_test_revoke(journal_t *journal, unsigned long blocknr,
			tid_t sequence)
{
	struct jbd_revoke_record_s *record;

	record = find_revoke_record(journal, blocknr);
	if (!record)
		return 0;
	if (tid_gt(sequence, record->sequence))
		return 0;
	return 1;
}

/*
 * Finally, once recovery is over, we need to clear the revoke table so
 * that it can be reused by the running filesystem.
 */

void journal_clear_revoke(journal_t *journal)
{
	int i;
	struct list_head *hash_list;
	struct jbd_revoke_record_s *record;
	struct jbd_revoke_table_s *revoke_var;

	revoke_var = journal->j_revoke;

	for (i = 0; i < revoke_var->hash_size; i++) {
		hash_list = &revoke_var->hash_table[i];
		while (!list_empty(hash_list)) {
			record = (struct jbd_revoke_record_s*) hash_list->next;
			list_del(&record->hash);
			free(record);
		}
	}
}

/*
 * e2fsck.c - superblock checks
 */

#define MIN_CHECK 1
#define MAX_CHECK 2

static void check_super_value(e2fsck_t ctx, const char *descr,
			      unsigned long value, int flags,
			      unsigned long min_val, unsigned long max_val)
{
	struct          problem_context pctx;

	if (((flags & MIN_CHECK) && (value < min_val)) ||
	    ((flags & MAX_CHECK) && (value > max_val))) {
		clear_problem_context(&pctx);
		pctx.num = value;
		pctx.str = descr;
		fix_problem(ctx, PR_0_MISC_CORRUPT_SUPER, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* never get here! */
	}
}

/*
 * This routine may get stubbed out in special compilations of the
 * e2fsck code..
 */
#ifndef EXT2_SPECIAL_DEVICE_SIZE
static errcode_t e2fsck_get_device_size(e2fsck_t ctx)
{
	return (ext2fs_get_device_size(ctx->filesystem_name,
				       EXT2_BLOCK_SIZE(ctx->fs->super),
				       &ctx->num_blocks));
}
#endif

/*
 * helper function to release an inode
 */
struct process_block_struct {
	e2fsck_t        ctx;
	char            *buf;
	struct problem_context *pctx;
	int             truncating;
	int             truncate_offset;
	e2_blkcnt_t     truncate_block;
	int             truncated_blocks;
	int             abort;
	errcode_t       errcode;
};

static int release_inode_block(ext2_filsys fs, blk_t *block_nr,
			       e2_blkcnt_t blockcnt,
			       blk_t    ref_blk FSCK_ATTR((unused)),
			       int      ref_offset FSCK_ATTR((unused)),
			       void *priv_data)
{
	struct process_block_struct *pb;
	e2fsck_t                ctx;
	struct problem_context  *pctx;
	blk_t                   blk = *block_nr;
	int                     retval = 0;

	pb = (struct process_block_struct *) priv_data;
	ctx = pb->ctx;
	pctx = pb->pctx;

	pctx->blk = blk;
	pctx->blkcount = blockcnt;

	if (HOLE_BLKADDR(blk))
		return 0;

	if ((blk < fs->super->s_first_data_block) ||
	    (blk >= fs->super->s_blocks_count)) {
		fix_problem(ctx, PR_0_ORPHAN_ILLEGAL_BLOCK_NUM, pctx);
 return_abort:
		pb->abort = 1;
		return BLOCK_ABORT;
	}

	if (!ext2fs_test_block_bitmap(fs->block_map, blk)) {
		fix_problem(ctx, PR_0_ORPHAN_ALREADY_CLEARED_BLOCK, pctx);
		goto return_abort;
	}

	/*
	 * If we are deleting an orphan, then we leave the fields alone.
	 * If we are truncating an orphan, then update the inode fields
	 * and clean up any partial block data.
	 */
	if (pb->truncating) {
		/*
		 * We only remove indirect blocks if they are
		 * completely empty.
		 */
		if (blockcnt < 0) {
			int     i, limit;
			blk_t   *bp;

			pb->errcode = io_channel_read_blk(fs->io, blk, 1,
							pb->buf);
			if (pb->errcode)
				goto return_abort;

			limit = fs->blocksize >> 2;
			for (i = 0, bp = (blk_t *) pb->buf;
			     i < limit;  i++, bp++)
				if (*bp)
					return 0;
		}
		/*
		 * We don't remove direct blocks until we've reached
		 * the truncation block.
		 */
		if (blockcnt >= 0 && blockcnt < pb->truncate_block)
			return 0;
		/*
		 * If part of the last block needs truncating, we do
		 * it here.
		 */
		if ((blockcnt == pb->truncate_block) && pb->truncate_offset) {
			pb->errcode = io_channel_read_blk(fs->io, blk, 1,
							pb->buf);
			if (pb->errcode)
				goto return_abort;
			memset(pb->buf + pb->truncate_offset, 0,
			       fs->blocksize - pb->truncate_offset);
			pb->errcode = io_channel_write_blk(fs->io, blk, 1,
							 pb->buf);
			if (pb->errcode)
				goto return_abort;
		}
		pb->truncated_blocks++;
		*block_nr = 0;
		retval |= BLOCK_CHANGED;
	}

	ext2fs_block_alloc_stats(fs, blk, -1);
	return retval;
}

/*
 * This function releases an inode.  Returns 1 if an inconsistency was
 * found.  If the inode has a link count, then it is being truncated and
 * not deleted.
 */
static int release_inode_blocks(e2fsck_t ctx, ext2_ino_t ino,
				struct ext2_inode *inode, char *block_buf,
				struct problem_context *pctx)
{
	struct process_block_struct     pb;
	ext2_filsys                     fs = ctx->fs;
	errcode_t                       retval;
	__u32                           count;

	if (!ext2fs_inode_has_valid_blocks(inode))
		return 0;

	pb.buf = block_buf + 3 * ctx->fs->blocksize;
	pb.ctx = ctx;
	pb.abort = 0;
	pb.errcode = 0;
	pb.pctx = pctx;
	if (inode->i_links_count) {
		pb.truncating = 1;
		pb.truncate_block = (e2_blkcnt_t)
			((((long long)inode->i_size_high << 32) +
			  inode->i_size + fs->blocksize - 1) /
			 fs->blocksize);
		pb.truncate_offset = inode->i_size % fs->blocksize;
	} else {
		pb.truncating = 0;
		pb.truncate_block = 0;
		pb.truncate_offset = 0;
	}
	pb.truncated_blocks = 0;
	retval = ext2fs_block_iterate2(fs, ino, BLOCK_FLAG_DEPTH_TRAVERSE,
				      block_buf, release_inode_block, &pb);
	if (retval) {
		bb_error_msg(_("while calling ext2fs_block_iterate for inode %d"),
			ino);
		return 1;
	}
	if (pb.abort)
		return 1;

	/* Refresh the inode since ext2fs_block_iterate may have changed it */
	e2fsck_read_inode(ctx, ino, inode, "release_inode_blocks");

	if (pb.truncated_blocks)
		inode->i_blocks -= pb.truncated_blocks *
			(fs->blocksize / 512);

	if (inode->i_file_acl) {
		retval = ext2fs_adjust_ea_refcount(fs, inode->i_file_acl,
						   block_buf, -1, &count);
		if (retval == EXT2_ET_BAD_EA_BLOCK_NUM) {
			retval = 0;
			count = 1;
		}
		if (retval) {
			bb_error_msg(_("while calling ext2fs_adjust_ea_refocunt for inode %d"),
				ino);
			return 1;
		}
		if (count == 0)
			ext2fs_block_alloc_stats(fs, inode->i_file_acl, -1);
		inode->i_file_acl = 0;
	}
	return 0;
}

/*
 * This function releases all of the orphan inodes.  It returns 1 if
 * it hit some error, and 0 on success.
 */
static int release_orphan_inodes(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t      ino, next_ino;
	struct ext2_inode inode;
	struct problem_context pctx;
	char *block_buf;

	if ((ino = fs->super->s_last_orphan) == 0)
		return 0;

	/*
	 * Win or lose, we won't be using the head of the orphan inode
	 * list again.
	 */
	fs->super->s_last_orphan = 0;
	ext2fs_mark_super_dirty(fs);

	/*
	 * If the filesystem contains errors, don't run the orphan
	 * list, since the orphan list can't be trusted; and we're
	 * going to be running a full e2fsck run anyway...
	 */
	if (fs->super->s_state & EXT2_ERROR_FS)
		return 0;

	if ((ino < EXT2_FIRST_INODE(fs->super)) ||
	    (ino > fs->super->s_inodes_count)) {
		clear_problem_context(&pctx);
		pctx.ino = ino;
		fix_problem(ctx, PR_0_ORPHAN_ILLEGAL_HEAD_INODE, &pctx);
		return 1;
	}

	block_buf = (char *) e2fsck_allocate_memory(ctx, fs->blocksize * 4,
						    "block iterate buffer");
	e2fsck_read_bitmaps(ctx);

	while (ino) {
		e2fsck_read_inode(ctx, ino, &inode, "release_orphan_inodes");
		clear_problem_context(&pctx);
		pctx.ino = ino;
		pctx.inode = &inode;
		pctx.str = inode.i_links_count ? _("Truncating") :
			_("Clearing");

		fix_problem(ctx, PR_0_ORPHAN_CLEAR_INODE, &pctx);

		next_ino = inode.i_dtime;
		if (next_ino &&
		    ((next_ino < EXT2_FIRST_INODE(fs->super)) ||
		     (next_ino > fs->super->s_inodes_count))) {
			pctx.ino = next_ino;
			fix_problem(ctx, PR_0_ORPHAN_ILLEGAL_INODE, &pctx);
			goto return_abort;
		}

		if (release_inode_blocks(ctx, ino, &inode, block_buf, &pctx))
			goto return_abort;

		if (!inode.i_links_count) {
			ext2fs_inode_alloc_stats2(fs, ino, -1,
						  LINUX_S_ISDIR(inode.i_mode));
			inode.i_dtime = time(NULL);
		} else {
			inode.i_dtime = 0;
		}
		e2fsck_write_inode(ctx, ino, &inode, "delete_file");
		ino = next_ino;
	}
	ext2fs_free_mem(&block_buf);
	return 0;
 return_abort:
	ext2fs_free_mem(&block_buf);
	return 1;
}

/*
 * Check the resize inode to make sure it is sane.  We check both for
 * the case where on-line resizing is not enabled (in which case the
 * resize inode should be cleared) as well as the case where on-line
 * resizing is enabled.
 */
static void check_resize_inode(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	struct ext2_inode inode;
	struct problem_context  pctx;
	int             i, j, gdt_off, ind_off;
	blk_t           blk, pblk, expect;
	__u32           *dind_buf = NULL, *ind_buf;
	errcode_t       retval;

	clear_problem_context(&pctx);

	/*
	 * If the resize inode feature isn't set, then
	 * s_reserved_gdt_blocks must be zero.
	 */
	if (!(fs->super->s_feature_compat &
	      EXT2_FEATURE_COMPAT_RESIZE_INO)) {
		if (fs->super->s_reserved_gdt_blocks) {
			pctx.num = fs->super->s_reserved_gdt_blocks;
			if (fix_problem(ctx, PR_0_NONZERO_RESERVED_GDT_BLOCKS,
					&pctx)) {
				fs->super->s_reserved_gdt_blocks = 0;
				ext2fs_mark_super_dirty(fs);
			}
		}
	}

	/* Read the resize inode */
	pctx.ino = EXT2_RESIZE_INO;
	retval = ext2fs_read_inode(fs, EXT2_RESIZE_INO, &inode);
	if (retval) {
		if (fs->super->s_feature_compat &
		    EXT2_FEATURE_COMPAT_RESIZE_INO)
			ctx->flags |= E2F_FLAG_RESIZE_INODE;
		return;
	}

	/*
	 * If the resize inode feature isn't set, check to make sure
	 * the resize inode is cleared; then we're done.
	 */
	if (!(fs->super->s_feature_compat &
	      EXT2_FEATURE_COMPAT_RESIZE_INO)) {
		for (i=0; i < EXT2_N_BLOCKS; i++) {
			if (inode.i_block[i])
				break;
		}
		if ((i < EXT2_N_BLOCKS) &&
		    fix_problem(ctx, PR_0_CLEAR_RESIZE_INODE, &pctx)) {
			memset(&inode, 0, sizeof(inode));
			e2fsck_write_inode(ctx, EXT2_RESIZE_INO, &inode,
					   "clear_resize");
		}
		return;
	}

	/*
	 * The resize inode feature is enabled; check to make sure the
	 * only block in use is the double indirect block
	 */
	blk = inode.i_block[EXT2_DIND_BLOCK];
	for (i=0; i < EXT2_N_BLOCKS; i++) {
		if (i != EXT2_DIND_BLOCK && inode.i_block[i])
			break;
	}
	if ((i < EXT2_N_BLOCKS) || !blk || !inode.i_links_count ||
	    !(inode.i_mode & LINUX_S_IFREG) ||
	    (blk < fs->super->s_first_data_block ||
	     blk >= fs->super->s_blocks_count)) {
 resize_inode_invalid:
		if (fix_problem(ctx, PR_0_RESIZE_INODE_INVALID, &pctx)) {
			memset(&inode, 0, sizeof(inode));
			e2fsck_write_inode(ctx, EXT2_RESIZE_INO, &inode,
					   "clear_resize");
			ctx->flags |= E2F_FLAG_RESIZE_INODE;
		}
		if (!(ctx->options & E2F_OPT_READONLY)) {
			fs->super->s_state &= ~EXT2_VALID_FS;
			ext2fs_mark_super_dirty(fs);
		}
		goto cleanup;
	}
	dind_buf = (__u32 *) e2fsck_allocate_memory(ctx, fs->blocksize * 2,
						    "resize dind buffer");
	ind_buf = (__u32 *) ((char *) dind_buf + fs->blocksize);

	retval = ext2fs_read_ind_block(fs, blk, dind_buf);
	if (retval)
		goto resize_inode_invalid;

	gdt_off = fs->desc_blocks;
	pblk = fs->super->s_first_data_block + 1 + fs->desc_blocks;
	for (i = 0; i < fs->super->s_reserved_gdt_blocks / 4;
	     i++, gdt_off++, pblk++) {
		gdt_off %= fs->blocksize/4;
		if (dind_buf[gdt_off] != pblk)
			goto resize_inode_invalid;
		retval = ext2fs_read_ind_block(fs, pblk, ind_buf);
		if (retval)
			goto resize_inode_invalid;
		ind_off = 0;
		for (j = 1; j < fs->group_desc_count; j++) {
			if (!ext2fs_bg_has_super(fs, j))
				continue;
			expect = pblk + (j * fs->super->s_blocks_per_group);
			if (ind_buf[ind_off] != expect)
				goto resize_inode_invalid;
			ind_off++;
		}
	}

 cleanup:
	ext2fs_free_mem(&dind_buf);
}

static void check_super_block(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	blk_t   first_block, last_block;
	struct ext2_super_block *sb = fs->super;
	struct ext2_group_desc *gd;
	blk_t   blocks_per_group = fs->super->s_blocks_per_group;
	blk_t   bpg_max;
	int     inodes_per_block;
	int     ipg_max;
	int     inode_size;
	dgrp_t  i;
	blk_t   should_be;
	struct problem_context  pctx;
	__u32   free_blocks = 0, free_inodes = 0;

	inodes_per_block = EXT2_INODES_PER_BLOCK(fs->super);
	ipg_max = inodes_per_block * (blocks_per_group - 4);
	if (ipg_max > EXT2_MAX_INODES_PER_GROUP(sb))
		ipg_max = EXT2_MAX_INODES_PER_GROUP(sb);
	bpg_max = 8 * EXT2_BLOCK_SIZE(sb);
	if (bpg_max > EXT2_MAX_BLOCKS_PER_GROUP(sb))
		bpg_max = EXT2_MAX_BLOCKS_PER_GROUP(sb);

	ctx->invalid_inode_bitmap_flag = (int *) e2fsck_allocate_memory(ctx,
		 sizeof(int) * fs->group_desc_count, "invalid_inode_bitmap");
	ctx->invalid_block_bitmap_flag = (int *) e2fsck_allocate_memory(ctx,
		 sizeof(int) * fs->group_desc_count, "invalid_block_bitmap");
	ctx->invalid_inode_table_flag = (int *) e2fsck_allocate_memory(ctx,
		sizeof(int) * fs->group_desc_count, "invalid_inode_table");

	clear_problem_context(&pctx);

	/*
	 * Verify the super block constants...
	 */
	check_super_value(ctx, "inodes_count", sb->s_inodes_count,
			  MIN_CHECK, 1, 0);
	check_super_value(ctx, "blocks_count", sb->s_blocks_count,
			  MIN_CHECK, 1, 0);
	check_super_value(ctx, "first_data_block", sb->s_first_data_block,
			  MAX_CHECK, 0, sb->s_blocks_count);
	check_super_value(ctx, "log_block_size", sb->s_log_block_size,
			  MIN_CHECK | MAX_CHECK, 0,
			  EXT2_MAX_BLOCK_LOG_SIZE - EXT2_MIN_BLOCK_LOG_SIZE);
	check_super_value(ctx, "log_frag_size", sb->s_log_frag_size,
			  MIN_CHECK | MAX_CHECK, 0, sb->s_log_block_size);
	check_super_value(ctx, "frags_per_group", sb->s_frags_per_group,
			  MIN_CHECK | MAX_CHECK, sb->s_blocks_per_group,
			  bpg_max);
	check_super_value(ctx, "blocks_per_group", sb->s_blocks_per_group,
			  MIN_CHECK | MAX_CHECK, 8, bpg_max);
	check_super_value(ctx, "inodes_per_group", sb->s_inodes_per_group,
			  MIN_CHECK | MAX_CHECK, inodes_per_block, ipg_max);
	check_super_value(ctx, "r_blocks_count", sb->s_r_blocks_count,
			  MAX_CHECK, 0, sb->s_blocks_count / 2);
	check_super_value(ctx, "reserved_gdt_blocks",
			  sb->s_reserved_gdt_blocks, MAX_CHECK, 0,
			  fs->blocksize/4);
	inode_size = EXT2_INODE_SIZE(sb);
	check_super_value(ctx, "inode_size",
			  inode_size, MIN_CHECK | MAX_CHECK,
			  EXT2_GOOD_OLD_INODE_SIZE, fs->blocksize);
	if (inode_size & (inode_size - 1)) {
		pctx.num = inode_size;
		pctx.str = "inode_size";
		fix_problem(ctx, PR_0_MISC_CORRUPT_SUPER, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* never get here! */
		return;
	}

	if (!ctx->num_blocks) {
		pctx.errcode = e2fsck_get_device_size(ctx);
		if (pctx.errcode && pctx.errcode != EXT2_ET_UNIMPLEMENTED) {
			fix_problem(ctx, PR_0_GETSIZE_ERROR, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		if ((pctx.errcode != EXT2_ET_UNIMPLEMENTED) &&
		    (ctx->num_blocks < sb->s_blocks_count)) {
			pctx.blk = sb->s_blocks_count;
			pctx.blk2 = ctx->num_blocks;
			if (fix_problem(ctx, PR_0_FS_SIZE_WRONG, &pctx)) {
				ctx->flags |= E2F_FLAG_ABORT;
				return;
			}
		}
	}

	if (sb->s_log_block_size != (__u32) sb->s_log_frag_size) {
		pctx.blk = EXT2_BLOCK_SIZE(sb);
		pctx.blk2 = EXT2_FRAG_SIZE(sb);
		fix_problem(ctx, PR_0_NO_FRAGMENTS, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

	should_be = sb->s_frags_per_group >>
		(sb->s_log_block_size - sb->s_log_frag_size);
	if (sb->s_blocks_per_group != should_be) {
		pctx.blk = sb->s_blocks_per_group;
		pctx.blk2 = should_be;
		fix_problem(ctx, PR_0_BLOCKS_PER_GROUP, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

	should_be = (sb->s_log_block_size == 0) ? 1 : 0;
	if (sb->s_first_data_block != should_be) {
		pctx.blk = sb->s_first_data_block;
		pctx.blk2 = should_be;
		fix_problem(ctx, PR_0_FIRST_DATA_BLOCK, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

	should_be = sb->s_inodes_per_group * fs->group_desc_count;
	if (sb->s_inodes_count != should_be) {
		pctx.ino = sb->s_inodes_count;
		pctx.ino2 = should_be;
		if (fix_problem(ctx, PR_0_INODE_COUNT_WRONG, &pctx)) {
			sb->s_inodes_count = should_be;
			ext2fs_mark_super_dirty(fs);
		}
	}

	/*
	 * Verify the group descriptors....
	 */
	first_block =  sb->s_first_data_block;
	last_block = first_block + blocks_per_group;

	for (i = 0, gd=fs->group_desc; i < fs->group_desc_count; i++, gd++) {
		pctx.group = i;

		if (i == fs->group_desc_count - 1)
			last_block = sb->s_blocks_count;
		if ((gd->bg_block_bitmap < first_block) ||
		    (gd->bg_block_bitmap >= last_block)) {
			pctx.blk = gd->bg_block_bitmap;
			if (fix_problem(ctx, PR_0_BB_NOT_GROUP, &pctx))
				gd->bg_block_bitmap = 0;
		}
		if (gd->bg_block_bitmap == 0) {
			ctx->invalid_block_bitmap_flag[i]++;
			ctx->invalid_bitmaps++;
		}
		if ((gd->bg_inode_bitmap < first_block) ||
		    (gd->bg_inode_bitmap >= last_block)) {
			pctx.blk = gd->bg_inode_bitmap;
			if (fix_problem(ctx, PR_0_IB_NOT_GROUP, &pctx))
				gd->bg_inode_bitmap = 0;
		}
		if (gd->bg_inode_bitmap == 0) {
			ctx->invalid_inode_bitmap_flag[i]++;
			ctx->invalid_bitmaps++;
		}
		if ((gd->bg_inode_table < first_block) ||
		    ((gd->bg_inode_table +
		      fs->inode_blocks_per_group - 1) >= last_block)) {
			pctx.blk = gd->bg_inode_table;
			if (fix_problem(ctx, PR_0_ITABLE_NOT_GROUP, &pctx))
				gd->bg_inode_table = 0;
		}
		if (gd->bg_inode_table == 0) {
			ctx->invalid_inode_table_flag[i]++;
			ctx->invalid_bitmaps++;
		}
		free_blocks += gd->bg_free_blocks_count;
		free_inodes += gd->bg_free_inodes_count;
		first_block += sb->s_blocks_per_group;
		last_block += sb->s_blocks_per_group;

		if ((gd->bg_free_blocks_count > sb->s_blocks_per_group) ||
		    (gd->bg_free_inodes_count > sb->s_inodes_per_group) ||
		    (gd->bg_used_dirs_count > sb->s_inodes_per_group))
			ext2fs_unmark_valid(fs);
	}

	/*
	 * Update the global counts from the block group counts.  This
	 * is needed for an experimental patch which eliminates
	 * locking the entire filesystem when allocating blocks or
	 * inodes; if the filesystem is not unmounted cleanly, the
	 * global counts may not be accurate.
	 */
	if ((free_blocks != sb->s_free_blocks_count) ||
	    (free_inodes != sb->s_free_inodes_count)) {
		if (ctx->options & E2F_OPT_READONLY)
			ext2fs_unmark_valid(fs);
		else {
			sb->s_free_blocks_count = free_blocks;
			sb->s_free_inodes_count = free_inodes;
			ext2fs_mark_super_dirty(fs);
		}
	}

	if ((sb->s_free_blocks_count > sb->s_blocks_count) ||
	    (sb->s_free_inodes_count > sb->s_inodes_count))
		ext2fs_unmark_valid(fs);


	/*
	 * If we have invalid bitmaps, set the error state of the
	 * filesystem.
	 */
	if (ctx->invalid_bitmaps && !(ctx->options & E2F_OPT_READONLY)) {
		sb->s_state &= ~EXT2_VALID_FS;
		ext2fs_mark_super_dirty(fs);
	}

	clear_problem_context(&pctx);

	/*
	 * If the UUID field isn't assigned, assign it.
	 */
	if (!(ctx->options & E2F_OPT_READONLY) && uuid_is_null(sb->s_uuid)) {
		if (fix_problem(ctx, PR_0_ADD_UUID, &pctx)) {
			uuid_generate(sb->s_uuid);
			ext2fs_mark_super_dirty(fs);
			fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
		}
	}

	/* FIXME - HURD support?
	 * For the Hurd, check to see if the filetype option is set,
	 * since it doesn't support it.
	 */
	if (!(ctx->options & E2F_OPT_READONLY) &&
	    fs->super->s_creator_os == EXT2_OS_HURD &&
	    (fs->super->s_feature_incompat &
	     EXT2_FEATURE_INCOMPAT_FILETYPE)) {
		if (fix_problem(ctx, PR_0_HURD_CLEAR_FILETYPE, &pctx)) {
			fs->super->s_feature_incompat &=
				~EXT2_FEATURE_INCOMPAT_FILETYPE;
			ext2fs_mark_super_dirty(fs);
		}
	}

	/*
	 * If we have any of the compatibility flags set, we need to have a
	 * revision 1 filesystem.  Most kernels will not check the flags on
	 * a rev 0 filesystem and we may have corruption issues because of
	 * the incompatible changes to the filesystem.
	 */
	if (!(ctx->options & E2F_OPT_READONLY) &&
	    fs->super->s_rev_level == EXT2_GOOD_OLD_REV &&
	    (fs->super->s_feature_compat ||
	     fs->super->s_feature_ro_compat ||
	     fs->super->s_feature_incompat) &&
	    fix_problem(ctx, PR_0_FS_REV_LEVEL, &pctx)) {
		ext2fs_update_dynamic_rev(fs);
		ext2fs_mark_super_dirty(fs);
	}

	check_resize_inode(ctx);

	/*
	 * Clean up any orphan inodes, if present.
	 */
	if (!(ctx->options & E2F_OPT_READONLY) && release_orphan_inodes(ctx)) {
		fs->super->s_state &= ~EXT2_VALID_FS;
		ext2fs_mark_super_dirty(fs);
	}

	/*
	 * Move the ext3 journal file, if necessary.
	 */
	e2fsck_move_ext3_journal(ctx);
}

/*
 * swapfs.c --- byte-swap an ext2 filesystem
 */

#ifdef ENABLE_SWAPFS

struct swap_block_struct {
	ext2_ino_t      ino;
	int             isdir;
	errcode_t       errcode;
	char            *dir_buf;
	struct ext2_inode *inode;
};

/*
 * This is a helper function for block_iterate.  We mark all of the
 * indirect and direct blocks as changed, so that block_iterate will
 * write them out.
 */
static int swap_block(ext2_filsys fs, blk_t *block_nr, int blockcnt,
		      void *priv_data)
{
	errcode_t       retval;

	struct swap_block_struct *sb = (struct swap_block_struct *) priv_data;

	if (sb->isdir && (blockcnt >= 0) && *block_nr) {
		retval = ext2fs_read_dir_block(fs, *block_nr, sb->dir_buf);
		if (retval) {
			sb->errcode = retval;
			return BLOCK_ABORT;
		}
		retval = ext2fs_write_dir_block(fs, *block_nr, sb->dir_buf);
		if (retval) {
			sb->errcode = retval;
			return BLOCK_ABORT;
		}
	}
	if (blockcnt >= 0) {
		if (blockcnt < EXT2_NDIR_BLOCKS)
			return 0;
		return BLOCK_CHANGED;
	}
	if (blockcnt == BLOCK_COUNT_IND) {
		if (*block_nr == sb->inode->i_block[EXT2_IND_BLOCK])
			return 0;
		return BLOCK_CHANGED;
	}
	if (blockcnt == BLOCK_COUNT_DIND) {
		if (*block_nr == sb->inode->i_block[EXT2_DIND_BLOCK])
			return 0;
		return BLOCK_CHANGED;
	}
	if (blockcnt == BLOCK_COUNT_TIND) {
		if (*block_nr == sb->inode->i_block[EXT2_TIND_BLOCK])
			return 0;
		return BLOCK_CHANGED;
	}
	return BLOCK_CHANGED;
}

/*
 * This function is responsible for byte-swapping all of the indirect,
 * block pointers.  It is also responsible for byte-swapping directories.
 */
static void swap_inode_blocks(e2fsck_t ctx, ext2_ino_t ino, char *block_buf,
			      struct ext2_inode *inode)
{
	errcode_t                       retval;
	struct swap_block_struct        sb;

	sb.ino = ino;
	sb.inode = inode;
	sb.dir_buf = block_buf + ctx->fs->blocksize*3;
	sb.errcode = 0;
	sb.isdir = 0;
	if (LINUX_S_ISDIR(inode->i_mode))
		sb.isdir = 1;

	retval = ext2fs_block_iterate(ctx->fs, ino, 0, block_buf,
				      swap_block, &sb);
	if (retval) {
		bb_error_msg(_("while calling ext2fs_block_iterate"));
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	if (sb.errcode) {
		bb_error_msg(_("while calling iterator function"));
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
}

static void swap_inodes(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	dgrp_t                  group;
	unsigned int            i;
	ext2_ino_t              ino = 1;
	char                    *buf, *block_buf;
	errcode_t               retval;
	struct ext2_inode *     inode;

	e2fsck_use_inode_shortcuts(ctx, 1);

	retval = ext2fs_get_mem(fs->blocksize * fs->inode_blocks_per_group,
				&buf);
	if (retval) {
		bb_error_msg(_("while allocating inode buffer"));
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	block_buf = (char *) e2fsck_allocate_memory(ctx, fs->blocksize * 4,
						    "block interate buffer");
	for (group = 0; group < fs->group_desc_count; group++) {
		retval = io_channel_read_blk(fs->io,
		      fs->group_desc[group].bg_inode_table,
		      fs->inode_blocks_per_group, buf);
		if (retval) {
			bb_error_msg(_("while reading inode table (group %d)"),
				group);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		inode = (struct ext2_inode *) buf;
		for (i=0; i < fs->super->s_inodes_per_group;
		     i++, ino++, inode++) {
			ctx->stashed_ino = ino;
			ctx->stashed_inode = inode;

			if (fs->flags & EXT2_FLAG_SWAP_BYTES_READ)
				ext2fs_swap_inode(fs, inode, inode, 0);

			/*
			 * Skip deleted files.
			 */
			if (inode->i_links_count == 0)
				continue;

			if (LINUX_S_ISDIR(inode->i_mode) ||
			    ((inode->i_block[EXT2_IND_BLOCK] ||
			      inode->i_block[EXT2_DIND_BLOCK] ||
			      inode->i_block[EXT2_TIND_BLOCK]) &&
			     ext2fs_inode_has_valid_blocks(inode)))
				swap_inode_blocks(ctx, ino, block_buf, inode);

			if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
				return;

			if (fs->flags & EXT2_FLAG_SWAP_BYTES_WRITE)
				ext2fs_swap_inode(fs, inode, inode, 1);
		}
		retval = io_channel_write_blk(fs->io,
		      fs->group_desc[group].bg_inode_table,
		      fs->inode_blocks_per_group, buf);
		if (retval) {
			bb_error_msg(_("while writing inode table (group %d)"),
				group);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
	}
	ext2fs_free_mem(&buf);
	ext2fs_free_mem(&block_buf);
	e2fsck_use_inode_shortcuts(ctx, 0);
	ext2fs_flush_icache(fs);
}

#if defined(__powerpc__) && BB_BIG_ENDIAN
/*
 * On the PowerPC, the big-endian variant of the ext2 filesystem
 * has its bitmaps stored as 32-bit words with bit 0 as the LSB
 * of each word.  Thus a bitmap with only bit 0 set would be, as
 * a string of bytes, 00 00 00 01 00 ...
 * To cope with this, we byte-reverse each word of a bitmap if
 * we have a big-endian filesystem, that is, if we are *not*
 * byte-swapping other word-sized numbers.
 */
#define EXT2_BIG_ENDIAN_BITMAPS
#endif

#ifdef EXT2_BIG_ENDIAN_BITMAPS
static void ext2fs_swap_bitmap(ext2fs_generic_bitmap bmap)
{
	__u32 *p = (__u32 *) bmap->bitmap;
	int n, nbytes = (bmap->end - bmap->start + 7) / 8;

	for (n = nbytes / sizeof(__u32); n > 0; --n, ++p)
		*p = ext2fs_swab32(*p);
}
#endif


#ifdef ENABLE_SWAPFS
static void swap_filesys(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	if (!(ctx->options & E2F_OPT_PREEN))
		printf(_("Pass 0: Doing byte-swap of filesystem\n"));

	/* Byte swap */

	if (fs->super->s_mnt_count) {
		fprintf(stderr, _("%s: the filesystem must be freshly "
			"checked using fsck\n"
			"and not mounted before trying to "
			"byte-swap it.\n"), ctx->device_name);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	if (fs->flags & EXT2_FLAG_SWAP_BYTES) {
		fs->flags &= ~(EXT2_FLAG_SWAP_BYTES|
			       EXT2_FLAG_SWAP_BYTES_WRITE);
		fs->flags |= EXT2_FLAG_SWAP_BYTES_READ;
	} else {
		fs->flags &= ~EXT2_FLAG_SWAP_BYTES_READ;
		fs->flags |= EXT2_FLAG_SWAP_BYTES_WRITE;
	}
	swap_inodes(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;
	if (fs->flags & EXT2_FLAG_SWAP_BYTES_WRITE)
		fs->flags |= EXT2_FLAG_SWAP_BYTES;
	fs->flags &= ~(EXT2_FLAG_SWAP_BYTES_READ|
		       EXT2_FLAG_SWAP_BYTES_WRITE);

#ifdef EXT2_BIG_ENDIAN_BITMAPS
	e2fsck_read_bitmaps(ctx);
	ext2fs_swap_bitmap(fs->inode_map);
	ext2fs_swap_bitmap(fs->block_map);
	fs->flags |= EXT2_FLAG_BB_DIRTY | EXT2_FLAG_IB_DIRTY;
#endif
	fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
	ext2fs_flush(fs);
	fs->flags |= EXT2_FLAG_MASTER_SB_ONLY;
}
#endif  /* ENABLE_SWAPFS */

#endif

/*
 * util.c --- miscellaneous utilities
 */


void *e2fsck_allocate_memory(e2fsck_t ctx, unsigned int size,
			     const char *description)
{
	void *ret;
	char buf[256];

	ret = xzalloc(size);
	return ret;
}

static char *string_copy(const char *str, int len)
{
	char    *ret;

	if (!str)
		return NULL;
	if (!len)
		len = strlen(str);
	ret = xmalloc(len+1);
	strncpy(ret, str, len);
	ret[len] = 0;
	return ret;
}

#ifndef HAVE_CONIO_H
static int read_a_char(void)
{
	char    c;
	int     r;
	int     fail = 0;

	while (1) {
		if (e2fsck_global_ctx &&
		    (e2fsck_global_ctx->flags & E2F_FLAG_CANCEL)) {
			return 3;
		}
		r = read(0, &c, 1);
		if (r == 1)
			return c;
		if (fail++ > 100)
			break;
	}
	return EOF;
}
#endif

static int ask_yn(const char * string, int def)
{
	int             c;
	const char      *defstr;
	static const char short_yes[] = "yY";
	static const char short_no[] = "nN";

#ifdef HAVE_TERMIOS_H
	struct termios  termios, tmp;

	tcgetattr (0, &termios);
	tmp = termios;
	tmp.c_lflag &= ~(ICANON | ECHO);
	tmp.c_cc[VMIN] = 1;
	tmp.c_cc[VTIME] = 0;
	tcsetattr_stdin_TCSANOW(&tmp);
#endif

	if (def == 1)
		defstr = "<y>";
	else if (def == 0)
		defstr = "<n>";
	else
		defstr = " (y/n)";
	printf("%s%s? ", string, defstr);
	while (1) {
		fflush (stdout);
		if ((c = read_a_char()) == EOF)
			break;
		if (c == 3) {
#ifdef HAVE_TERMIOS_H
			tcsetattr_stdin_TCSANOW(&termios);
#endif
			if (e2fsck_global_ctx &&
			    e2fsck_global_ctx->flags & E2F_FLAG_SETJMP_OK) {
				puts("\n");
				longjmp(e2fsck_global_ctx->abort_loc, 1);
			}
			puts(_("cancelled!\n"));
			return 0;
		}
		if (strchr(short_yes, (char) c)) {
			def = 1;
			break;
		}
		else if (strchr(short_no, (char) c)) {
			def = 0;
			break;
		}
		else if ((c == ' ' || c == '\n') && (def != -1))
			break;
	}
	if (def)
		puts("yes\n");
	else
		puts ("no\n");
#ifdef HAVE_TERMIOS_H
	tcsetattr_stdin_TCSANOW(&termios);
#endif
	return def;
}

int ask (e2fsck_t ctx, const char * string, int def)
{
	if (ctx->options & E2F_OPT_NO) {
		printf(_("%s? no\n\n"), string);
		return 0;
	}
	if (ctx->options & E2F_OPT_YES) {
		printf(_("%s? yes\n\n"), string);
		return 1;
	}
	if (ctx->options & E2F_OPT_PREEN) {
		printf("%s? %s\n\n", string, def ? _("yes") : _("no"));
		return def;
	}
	return ask_yn(string, def);
}

void e2fsck_read_bitmaps(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	errcode_t       retval;

	if (ctx->invalid_bitmaps) {
		bb_error_msg(_("e2fsck_read_bitmaps: illegal bitmap block(s) for %s"),
			ctx->device_name);
		bb_error_msg_and_die(0);
	}

	ehandler_operation(_("reading inode and block bitmaps"));
	retval = ext2fs_read_bitmaps(fs);
	ehandler_operation(0);
	if (retval) {
		bb_error_msg(_("while retrying to read bitmaps for %s"),
			ctx->device_name);
		bb_error_msg_and_die(0);
	}
}

static void e2fsck_write_bitmaps(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	errcode_t       retval;

	if (ext2fs_test_bb_dirty(fs)) {
		ehandler_operation(_("writing block bitmaps"));
		retval = ext2fs_write_block_bitmap(fs);
		ehandler_operation(0);
		if (retval) {
			bb_error_msg(_("while retrying to write block bitmaps for %s"),
				ctx->device_name);
			bb_error_msg_and_die(0);
		}
	}

	if (ext2fs_test_ib_dirty(fs)) {
		ehandler_operation(_("writing inode bitmaps"));
		retval = ext2fs_write_inode_bitmap(fs);
		ehandler_operation(0);
		if (retval) {
			bb_error_msg(_("while retrying to write inode bitmaps for %s"),
				ctx->device_name);
			bb_error_msg_and_die(0);
		}
	}
}

void preenhalt(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;

	if (!(ctx->options & E2F_OPT_PREEN))
		return;
	fprintf(stderr, _("\n\n%s: UNEXPECTED INCONSISTENCY; "
		"RUN fsck MANUALLY.\n\t(i.e., without -a or -p options)\n"),
	       ctx->device_name);
	if (fs != NULL) {
		fs->super->s_state |= EXT2_ERROR_FS;
		ext2fs_mark_super_dirty(fs);
		ext2fs_close(fs);
	}
	exit(EXIT_UNCORRECTED);
}

void e2fsck_read_inode(e2fsck_t ctx, unsigned long ino,
			      struct ext2_inode * inode, const char *proc)
{
	int retval;

	retval = ext2fs_read_inode(ctx->fs, ino, inode);
	if (retval) {
		bb_error_msg(_("while reading inode %ld in %s"), ino, proc);
		bb_error_msg_and_die(0);
	}
}

extern void e2fsck_write_inode_full(e2fsck_t ctx, unsigned long ino,
			       struct ext2_inode * inode, int bufsize,
			       const char *proc)
{
	int retval;

	retval = ext2fs_write_inode_full(ctx->fs, ino, inode, bufsize);
	if (retval) {
		bb_error_msg(_("while writing inode %ld in %s"), ino, proc);
		bb_error_msg_and_die(0);
	}
}

extern void e2fsck_write_inode(e2fsck_t ctx, unsigned long ino,
			       struct ext2_inode * inode, const char *proc)
{
	int retval;

	retval = ext2fs_write_inode(ctx->fs, ino, inode);
	if (retval) {
		bb_error_msg(_("while writing inode %ld in %s"), ino, proc);
		bb_error_msg_and_die(0);
	}
}

blk_t get_backup_sb(e2fsck_t ctx, ext2_filsys fs, const char *name,
		   io_manager manager)
{
	struct ext2_super_block *sb;
	io_channel              io = NULL;
	void                    *buf = NULL;
	int                     blocksize;
	blk_t                   superblock, ret_sb = 8193;

	if (fs && fs->super) {
		ret_sb = (fs->super->s_blocks_per_group +
			  fs->super->s_first_data_block);
		if (ctx) {
			ctx->superblock = ret_sb;
			ctx->blocksize = fs->blocksize;
		}
		return ret_sb;
	}

	if (ctx) {
		if (ctx->blocksize) {
			ret_sb = ctx->blocksize * 8;
			if (ctx->blocksize == 1024)
				ret_sb++;
			ctx->superblock = ret_sb;
			return ret_sb;
		}
		ctx->superblock = ret_sb;
		ctx->blocksize = 1024;
	}

	if (!name || !manager)
		goto cleanup;

	if (manager->open(name, 0, &io) != 0)
		goto cleanup;

	if (ext2fs_get_mem(SUPERBLOCK_SIZE, &buf))
		goto cleanup;
	sb = (struct ext2_super_block *) buf;

	for (blocksize = EXT2_MIN_BLOCK_SIZE;
	     blocksize <= EXT2_MAX_BLOCK_SIZE; blocksize *= 2) {
		superblock = blocksize*8;
		if (blocksize == 1024)
			superblock++;
		io_channel_set_blksize(io, blocksize);
		if (io_channel_read_blk(io, superblock,
					-SUPERBLOCK_SIZE, buf))
			continue;
#if BB_BIG_ENDIAN
		if (sb->s_magic == ext2fs_swab16(EXT2_SUPER_MAGIC))
			ext2fs_swap_super(sb);
#endif
		if (sb->s_magic == EXT2_SUPER_MAGIC) {
			ret_sb = superblock;
			if (ctx) {
				ctx->superblock = superblock;
				ctx->blocksize = blocksize;
			}
			break;
		}
	}

cleanup:
	if (io)
		io_channel_close(io);
	ext2fs_free_mem(&buf);
	return ret_sb;
}


/*
 * This function runs through the e2fsck passes and calls them all,
 * returning restart, abort, or cancel as necessary...
 */
typedef void (*pass_t)(e2fsck_t ctx);

static const pass_t e2fsck_passes[] = {
	e2fsck_pass1, e2fsck_pass2, e2fsck_pass3, e2fsck_pass4,
	e2fsck_pass5, 0 };

#define E2F_FLAG_RUN_RETURN     (E2F_FLAG_SIGNAL_MASK|E2F_FLAG_RESTART)

static int e2fsck_run(e2fsck_t ctx)
{
	int     i;
	pass_t  e2fsck_pass;

	if (setjmp(ctx->abort_loc)) {
		ctx->flags &= ~E2F_FLAG_SETJMP_OK;
		return (ctx->flags & E2F_FLAG_RUN_RETURN);
	}
	ctx->flags |= E2F_FLAG_SETJMP_OK;

	for (i=0; (e2fsck_pass = e2fsck_passes[i]); i++) {
		if (ctx->flags & E2F_FLAG_RUN_RETURN)
			break;
		e2fsck_pass(ctx);
		if (ctx->progress)
			(void) (ctx->progress)(ctx, 0, 0, 0);
	}
	ctx->flags &= ~E2F_FLAG_SETJMP_OK;

	if (ctx->flags & E2F_FLAG_RUN_RETURN)
		return (ctx->flags & E2F_FLAG_RUN_RETURN);
	return 0;
}


/*
 * unix.c - The unix-specific code for e2fsck
 */


/* Command line options */
static int swapfs;
#ifdef ENABLE_SWAPFS
static int normalize_swapfs;
#endif
static int cflag;               /* check disk */
static int show_version_only;
static int verbose;

#define P_E2(singular, plural, n)       n, ((n) == 1 ? singular : plural)

static void show_stats(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	int inodes, inodes_used, blocks, blocks_used;
	int dir_links;
	int num_files, num_links;
	int frag_percent;

	dir_links = 2 * ctx->fs_directory_count - 1;
	num_files = ctx->fs_total_count - dir_links;
	num_links = ctx->fs_links_count - dir_links;
	inodes = fs->super->s_inodes_count;
	inodes_used = (fs->super->s_inodes_count -
		       fs->super->s_free_inodes_count);
	blocks = fs->super->s_blocks_count;
	blocks_used = (fs->super->s_blocks_count -
		       fs->super->s_free_blocks_count);

	frag_percent = (10000 * ctx->fs_fragmented) / inodes_used;
	frag_percent = (frag_percent + 5) / 10;

	if (!verbose) {
		printf("%s: %d/%d files (%0d.%d%% non-contiguous), %d/%d blocks\n",
		       ctx->device_name, inodes_used, inodes,
		       frag_percent / 10, frag_percent % 10,
		       blocks_used, blocks);
		return;
	}
	printf("\n%8d inode%s used (%d%%)\n", P_E2("", "s", inodes_used),
		100 * inodes_used / inodes);
	printf("%8d non-contiguous inode%s (%0d.%d%%)\n",
		P_E2("", "s", ctx->fs_fragmented),
		frag_percent / 10, frag_percent % 10);
	printf(_("         # of inodes with ind/dind/tind blocks: %d/%d/%d\n"),
		ctx->fs_ind_count, ctx->fs_dind_count, ctx->fs_tind_count);
	printf("%8d block%s used (%d%%)\n", P_E2("", "s", blocks_used),
		(int) ((long long) 100 * blocks_used / blocks));
	printf("%8d large file%s\n", P_E2("", "s", ctx->large_files));
	printf("\n%8d regular file%s\n", P_E2("", "s", ctx->fs_regular_count));
	printf("%8d director%s\n", P_E2("y", "ies", ctx->fs_directory_count));
	printf("%8d character device file%s\n", P_E2("", "s", ctx->fs_chardev_count));
	printf("%8d block device file%s\n", P_E2("", "s", ctx->fs_blockdev_count));
	printf("%8d fifo%s\n", P_E2("", "s", ctx->fs_fifo_count));
	printf("%8d link%s\n", P_E2("", "s", ctx->fs_links_count - dir_links));
	printf("%8d symbolic link%s", P_E2("", "s", ctx->fs_symlinks_count));
	printf(" (%d fast symbolic link%s)\n", P_E2("", "s", ctx->fs_fast_symlinks_count));
	printf("%8d socket%s--------\n\n", P_E2("", "s", ctx->fs_sockets_count));
	printf("%8d file%s\n", P_E2("", "s", ctx->fs_total_count - dir_links));
}

static void check_mount(e2fsck_t ctx)
{
	errcode_t       retval;
	int             cont;

	retval = ext2fs_check_if_mounted(ctx->filesystem_name,
					 &ctx->mount_flags);
	if (retval) {
		bb_error_msg(_("while determining whether %s is mounted"),
			ctx->filesystem_name);
		return;
	}

	/*
	 * If the filesystem isn't mounted, or it's the root filesystem
	 * and it's mounted read-only, then everything's fine.
	 */
	if ((!(ctx->mount_flags & EXT2_MF_MOUNTED)) ||
	    ((ctx->mount_flags & EXT2_MF_ISROOT) &&
	     (ctx->mount_flags & EXT2_MF_READONLY)))
		return;

	if (ctx->options & E2F_OPT_READONLY) {
		printf(_("Warning!  %s is mounted.\n"), ctx->filesystem_name);
		return;
	}

	printf(_("%s is mounted.  "), ctx->filesystem_name);
	if (!ctx->interactive)
		bb_error_msg_and_die(_("can't continue, aborting"));
	printf(_("\n\n\007\007\007\007WARNING!!!  "
	       "Running e2fsck on a mounted filesystem may cause\n"
	       "SEVERE filesystem damage.\007\007\007\n\n"));
	cont = ask_yn(_("Do you really want to continue"), -1);
	if (!cont) {
		printf(_("check aborted.\n"));
		exit(0);
	}
}

static int is_on_batt(void)
{
	FILE    *f;
	DIR     *d;
	char    tmp[80], tmp2[80], fname[80];
	unsigned int    acflag;
	struct dirent*  de;

	f = fopen_for_read("/proc/apm");
	if (f) {
		if (fscanf(f, "%s %s %s %x", tmp, tmp, tmp, &acflag) != 4)
			acflag = 1;
		fclose(f);
		return (acflag != 1);
	}
	d = opendir("/proc/acpi/ac_adapter");
	if (d) {
		while ((de=readdir(d)) != NULL) {
			if (!strncmp(".", de->d_name, 1))
				continue;
			snprintf(fname, 80, "/proc/acpi/ac_adapter/%s/state",
				 de->d_name);
			f = fopen_for_read(fname);
			if (!f)
				continue;
			if (fscanf(f, "%s %s", tmp2, tmp) != 2)
				tmp[0] = 0;
			fclose(f);
			if (strncmp(tmp, "off-line", 8) == 0) {
				closedir(d);
				return 1;
			}
		}
		closedir(d);
	}
	return 0;
}

/*
 * This routine checks to see if a filesystem can be skipped; if so,
 * it will exit with EXIT_OK.  Under some conditions it will print a
 * message explaining why a check is being forced.
 */
static void check_if_skip(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	const char *reason = NULL;
	unsigned int reason_arg = 0;
	long next_check;
	int batt = is_on_batt();
	time_t now = time(NULL);

	if ((ctx->options & E2F_OPT_FORCE) || cflag || swapfs)
		return;

	if ((fs->super->s_state & EXT2_ERROR_FS) ||
	    !ext2fs_test_valid(fs))
		reason = _(" contains a file system with errors");
	else if ((fs->super->s_state & EXT2_VALID_FS) == 0)
		reason = _(" was not cleanly unmounted");
	else if ((fs->super->s_max_mnt_count > 0) &&
		 (fs->super->s_mnt_count >=
		  (unsigned) fs->super->s_max_mnt_count)) {
		reason = _(" has been mounted %u times without being checked");
		reason_arg = fs->super->s_mnt_count;
		if (batt && (fs->super->s_mnt_count <
			     (unsigned) fs->super->s_max_mnt_count*2))
			reason = 0;
	} else if (fs->super->s_checkinterval &&
		   ((now - fs->super->s_lastcheck) >=
		    fs->super->s_checkinterval)) {
		reason = _(" has gone %u days without being checked");
		reason_arg = (now - fs->super->s_lastcheck)/(3600*24);
		if (batt && ((now - fs->super->s_lastcheck) <
			     fs->super->s_checkinterval*2))
			reason = 0;
	}
	if (reason) {
		fputs(ctx->device_name, stdout);
		printf(reason, reason_arg);
		fputs(_(", check forced.\n"), stdout);
		return;
	}
	printf(_("%s: clean, %d/%d files, %d/%d blocks"), ctx->device_name,
	       fs->super->s_inodes_count - fs->super->s_free_inodes_count,
	       fs->super->s_inodes_count,
	       fs->super->s_blocks_count - fs->super->s_free_blocks_count,
	       fs->super->s_blocks_count);
	next_check = 100000;
	if (fs->super->s_max_mnt_count > 0) {
		next_check = fs->super->s_max_mnt_count - fs->super->s_mnt_count;
		if (next_check <= 0)
			next_check = 1;
	}
	if (fs->super->s_checkinterval &&
	    ((now - fs->super->s_lastcheck) >= fs->super->s_checkinterval))
		next_check = 1;
	if (next_check <= 5) {
		if (next_check == 1)
			fputs(_(" (check after next mount)"), stdout);
		else
			printf(_(" (check in %ld mounts)"), next_check);
	}
	bb_putchar('\n');
	ext2fs_close(fs);
	ctx->fs = NULL;
	e2fsck_free_context(ctx);
	exit(EXIT_OK);
}

/*
 * For completion notice
 */
struct percent_tbl {
	int     max_pass;
	int     table[32];
};
static const struct percent_tbl e2fsck_tbl = {
	5, { 0, 70, 90, 92,  95, 100 }
};

static char bar[128], spaces[128];

static float calc_percent(const struct percent_tbl *tbl, int pass, int curr,
			  int max)
{
	float   percent;

	if (pass <= 0)
		return 0.0;
	if (pass > tbl->max_pass || max == 0)
		return 100.0;
	percent = ((float) curr) / ((float) max);
	return ((percent * (tbl->table[pass] - tbl->table[pass-1]))
		+ tbl->table[pass-1]);
}

void e2fsck_clear_progbar(e2fsck_t ctx)
{
	if (!(ctx->flags & E2F_FLAG_PROG_BAR))
		return;

	printf("%s%s\r%s", ctx->start_meta, spaces + (sizeof(spaces) - 80),
	       ctx->stop_meta);
	fflush(stdout);
	ctx->flags &= ~E2F_FLAG_PROG_BAR;
}

int e2fsck_simple_progress(e2fsck_t ctx, const char *label, float percent,
			   unsigned int dpynum)
{
	static const char spinner[] = "\\|/-";
	int     i;
	unsigned int    tick;
	struct timeval  tv;
	int dpywidth;
	int fixed_percent;

	if (ctx->flags & E2F_FLAG_PROG_SUPPRESS)
		return 0;

	/*
	 * Calculate the new progress position.  If the
	 * percentage hasn't changed, then we skip out right
	 * away.
	 */
	fixed_percent = (int) ((10 * percent) + 0.5);
	if (ctx->progress_last_percent == fixed_percent)
		return 0;
	ctx->progress_last_percent = fixed_percent;

	/*
	 * If we've already updated the spinner once within
	 * the last 1/8th of a second, no point doing it
	 * again.
	 */
	gettimeofday(&tv, NULL);
	tick = (tv.tv_sec << 3) + (tv.tv_usec / (1000000 / 8));
	if ((tick == ctx->progress_last_time) &&
	    (fixed_percent != 0) && (fixed_percent != 1000))
		return 0;
	ctx->progress_last_time = tick;

	/*
	 * Advance the spinner, and note that the progress bar
	 * will be on the screen
	 */
	ctx->progress_pos = (ctx->progress_pos+1) & 3;
	ctx->flags |= E2F_FLAG_PROG_BAR;

	dpywidth = 66 - strlen(label);
	dpywidth = 8 * (dpywidth / 8);
	if (dpynum)
		dpywidth -= 8;

	i = ((percent * dpywidth) + 50) / 100;
	printf("%s%s: |%s%s", ctx->start_meta, label,
	       bar + (sizeof(bar) - (i+1)),
	       spaces + (sizeof(spaces) - (dpywidth - i + 1)));
	if (fixed_percent == 1000)
		bb_putchar('|');
	else
		bb_putchar(spinner[ctx->progress_pos & 3]);
	printf(" %4.1f%%  ", percent);
	if (dpynum)
		printf("%u\r", dpynum);
	else
		fputs(" \r", stdout);
	fputs(ctx->stop_meta, stdout);

	if (fixed_percent == 1000)
		e2fsck_clear_progbar(ctx);
	fflush(stdout);

	return 0;
}

static int e2fsck_update_progress(e2fsck_t ctx, int pass,
				  unsigned long cur, unsigned long max)
{
	char buf[80];
	float percent;

	if (pass == 0)
		return 0;

	if (ctx->progress_fd) {
		sprintf(buf, "%d %lu %lu\n", pass, cur, max);
		xwrite_str(ctx->progress_fd, buf);
	} else {
		percent = calc_percent(&e2fsck_tbl, pass, cur, max);
		e2fsck_simple_progress(ctx, ctx->device_name,
				       percent, 0);
	}
	return 0;
}

static void reserve_stdio_fds(void)
{
	int     fd;

	while (1) {
		fd = open(bb_dev_null, O_RDWR);
		if (fd > 2)
			break;
		if (fd < 0) {
			fprintf(stderr, _("ERROR: Cannot open "
				"/dev/null (%s)\n"),
				strerror(errno));
			break;
		}
	}
	close(fd);
}

static void signal_progress_on(int sig FSCK_ATTR((unused)))
{
	e2fsck_t ctx = e2fsck_global_ctx;

	if (!ctx)
		return;

	ctx->progress = e2fsck_update_progress;
	ctx->progress_fd = 0;
}

static void signal_progress_off(int sig FSCK_ATTR((unused)))
{
	e2fsck_t ctx = e2fsck_global_ctx;

	if (!ctx)
		return;

	e2fsck_clear_progbar(ctx);
	ctx->progress = 0;
}

static void signal_cancel(int sig FSCK_ATTR((unused)))
{
	e2fsck_t ctx = e2fsck_global_ctx;

	if (!ctx)
		exit(FSCK_CANCELED);

	ctx->flags |= E2F_FLAG_CANCEL;
}

static void parse_extended_opts(e2fsck_t ctx, const char *opts)
{
	char    *buf, *token, *next, *p, *arg;
	int     ea_ver;
	int     extended_usage = 0;

	buf = string_copy(opts, 0);
	for (token = buf; token && *token; token = next) {
		p = strchr(token, ',');
		next = 0;
		if (p) {
			*p = 0;
			next = p+1;
		}
		arg = strchr(token, '=');
		if (arg) {
			*arg = 0;
			arg++;
		}
		if (strcmp(token, "ea_ver") == 0) {
			if (!arg) {
				extended_usage++;
				continue;
			}
			ea_ver = strtoul(arg, &p, 0);
			if (*p ||
			    ((ea_ver != 1) && (ea_ver != 2))) {
				fprintf(stderr,
					_("Invalid EA version.\n"));
				extended_usage++;
				continue;
			}
			ctx->ext_attr_ver = ea_ver;
		} else {
			fprintf(stderr, _("Unknown extended option: %s\n"),
				token);
			extended_usage++;
		}
	}
	if (extended_usage) {
		bb_error_msg_and_die(
			"Extended options are separated by commas, "
			"and may take an argument which\n"
			"is set off by an equals ('=') sign.  "
			"Valid extended options are:\n"
			"\tea_ver=<ea_version (1 or 2)>\n\n");
	}
}


static errcode_t PRS(int argc, char **argv, e2fsck_t *ret_ctx)
{
	int             flush = 0;
	int             c, fd;
	e2fsck_t        ctx;
	errcode_t       retval;
	struct sigaction        sa;
	char            *extended_opts = NULL;

	retval = e2fsck_allocate_context(&ctx);
	if (retval)
		return retval;

	*ret_ctx = ctx;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	setvbuf(stderr, NULL, _IONBF, BUFSIZ);
	if (isatty(0) && isatty(1)) {
		ctx->interactive = 1;
	} else {
		ctx->start_meta[0] = '\001';
		ctx->stop_meta[0] = '\002';
	}
	memset(bar, '=', sizeof(bar)-1);
	memset(spaces, ' ', sizeof(spaces)-1);
	blkid_get_cache(&ctx->blkid, NULL);

	if (argc && *argv)
		ctx->program_name = *argv;
	else
		ctx->program_name = "e2fsck";
	while ((c = getopt (argc, argv, "panyrcC:B:dE:fvtFVM:b:I:j:P:l:L:N:SsDk")) != EOF)
		switch (c) {
		case 'C':
			ctx->progress = e2fsck_update_progress;
			ctx->progress_fd = atoi(optarg);
			if (!ctx->progress_fd)
				break;
			/* Validate the file descriptor to avoid disasters */
			fd = dup(ctx->progress_fd);
			if (fd < 0) {
				fprintf(stderr,
				_("Error validating file descriptor %d: %s\n"),
					ctx->progress_fd,
					error_message(errno));
				bb_error_msg_and_die(_("Invalid completion information file descriptor"));
			} else
				close(fd);
			break;
		case 'D':
			ctx->options |= E2F_OPT_COMPRESS_DIRS;
			break;
		case 'E':
			extended_opts = optarg;
			break;
		case 'p':
		case 'a':
			if (ctx->options & (E2F_OPT_YES|E2F_OPT_NO)) {
			conflict_opt:
				bb_error_msg_and_die(_("only one the options -p/-a, -n or -y may be specified"));
			}
			ctx->options |= E2F_OPT_PREEN;
			break;
		case 'n':
			if (ctx->options & (E2F_OPT_YES|E2F_OPT_PREEN))
				goto conflict_opt;
			ctx->options |= E2F_OPT_NO;
			break;
		case 'y':
			if (ctx->options & (E2F_OPT_PREEN|E2F_OPT_NO))
				goto conflict_opt;
			ctx->options |= E2F_OPT_YES;
			break;
		case 't':
			/* FIXME - This needs to go away in a future path - will change binary */
			fprintf(stderr, _("The -t option is not "
				"supported on this version of e2fsck.\n"));
			break;
		case 'c':
			if (cflag++)
				ctx->options |= E2F_OPT_WRITECHECK;
			ctx->options |= E2F_OPT_CHECKBLOCKS;
			break;
		case 'r':
			/* What we do by default, anyway! */
			break;
		case 'b':
			ctx->use_superblock = atoi(optarg);
			ctx->flags |= E2F_FLAG_SB_SPECIFIED;
			break;
		case 'B':
			ctx->blocksize = atoi(optarg);
			break;
		case 'I':
			ctx->inode_buffer_blocks = atoi(optarg);
			break;
		case 'j':
			ctx->journal_name = string_copy(optarg, 0);
			break;
		case 'P':
			ctx->process_inode_size = atoi(optarg);
			break;
		case 'd':
			ctx->options |= E2F_OPT_DEBUG;
			break;
		case 'f':
			ctx->options |= E2F_OPT_FORCE;
			break;
		case 'F':
			flush = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'V':
			show_version_only = 1;
			break;
		case 'N':
			ctx->device_name = optarg;
			break;
#ifdef ENABLE_SWAPFS
		case 's':
			normalize_swapfs = 1;
		case 'S':
			swapfs = 1;
			break;
#else
		case 's':
		case 'S':
			fprintf(stderr, _("Byte-swapping filesystems "
					  "not compiled in this version "
					  "of e2fsck\n"));
			exit(1);
#endif
		default:
			bb_show_usage();
		}
	if (show_version_only)
		return 0;
	if (optind != argc - 1)
		bb_show_usage();
	if ((ctx->options & E2F_OPT_NO) &&
	    !cflag && !swapfs && !(ctx->options & E2F_OPT_COMPRESS_DIRS))
		ctx->options |= E2F_OPT_READONLY;
	ctx->io_options = strchr(argv[optind], '?');
	if (ctx->io_options)
		*ctx->io_options++ = 0;
	ctx->filesystem_name = blkid_get_devname(ctx->blkid, argv[optind], 0);
	if (!ctx->filesystem_name) {
		bb_error_msg(_("Unable to resolve '%s'"), argv[optind]);
		bb_error_msg_and_die(0);
	}
	if (extended_opts)
		parse_extended_opts(ctx, extended_opts);

	if (flush) {
		fd = open(ctx->filesystem_name, O_RDONLY, 0);
		if (fd < 0) {
			bb_error_msg(_("while opening %s for flushing"),
				ctx->filesystem_name);
			bb_error_msg_and_die(0);
		}
		if ((retval = ext2fs_sync_device(fd, 1))) {
			bb_error_msg(_("while trying to flush %s"),
				ctx->filesystem_name);
			bb_error_msg_and_die(0);
		}
		close(fd);
	}
#ifdef ENABLE_SWAPFS
	if (swapfs && cflag) {
			fprintf(stderr, _("Incompatible options not "
					  "allowed when byte-swapping.\n"));
			exit(EXIT_USAGE);
	}
#endif
	/*
	 * Set up signal action
	 */
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = signal_cancel;
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGTERM, &sa, 0);
#ifdef SA_RESTART
	sa.sa_flags = SA_RESTART;
#endif
	e2fsck_global_ctx = ctx;
	sa.sa_handler = signal_progress_on;
	sigaction(SIGUSR1, &sa, 0);
	sa.sa_handler = signal_progress_off;
	sigaction(SIGUSR2, &sa, 0);

	/* Update our PATH to include /sbin if we need to run badblocks  */
	if (cflag)
		e2fs_set_sbin_path();
	return 0;
}

static const char my_ver_string[] = E2FSPROGS_VERSION;
static const char my_ver_date[] = E2FSPROGS_DATE;

int e2fsck_main (int argc, char **argv);
int e2fsck_main (int argc, char **argv)
{
	errcode_t       retval;
	int             exit_value = EXIT_OK;
	ext2_filsys     fs = 0;
	io_manager      io_ptr;
	struct ext2_super_block *sb;
	const char      *lib_ver_date;
	int             my_ver, lib_ver;
	e2fsck_t        ctx;
	struct problem_context pctx;
	int flags, run_result;

	clear_problem_context(&pctx);

	my_ver = ext2fs_parse_version_string(my_ver_string);
	lib_ver = ext2fs_get_library_version(0, &lib_ver_date);
	if (my_ver > lib_ver) {
		fprintf( stderr, _("Error: ext2fs library version "
			"out of date!\n"));
		show_version_only++;
	}

	retval = PRS(argc, argv, &ctx);
	if (retval) {
		bb_error_msg(_("while trying to initialize program"));
		exit(EXIT_ERROR);
	}
	reserve_stdio_fds();

	if (!(ctx->options & E2F_OPT_PREEN) || show_version_only)
		fprintf(stderr, "e2fsck %s (%s)\n", my_ver_string,
			 my_ver_date);

	if (show_version_only) {
		fprintf(stderr, _("\tUsing %s, %s\n"),
			error_message(EXT2_ET_BASE), lib_ver_date);
		exit(EXIT_OK);
	}

	check_mount(ctx);

	if (!(ctx->options & E2F_OPT_PREEN) &&
	    !(ctx->options & E2F_OPT_NO) &&
	    !(ctx->options & E2F_OPT_YES)) {
		if (!ctx->interactive)
			bb_error_msg_and_die(_("need terminal for interactive repairs"));
	}
	ctx->superblock = ctx->use_superblock;
restart:
#ifdef CONFIG_TESTIO_DEBUG
	io_ptr = test_io_manager;
	test_io_backing_manager = unix_io_manager;
#else
	io_ptr = unix_io_manager;
#endif
	flags = 0;
	if ((ctx->options & E2F_OPT_READONLY) == 0)
		flags |= EXT2_FLAG_RW;

	if (ctx->superblock && ctx->blocksize) {
		retval = ext2fs_open2(ctx->filesystem_name, ctx->io_options,
				      flags, ctx->superblock, ctx->blocksize,
				      io_ptr, &fs);
	} else if (ctx->superblock) {
		int blocksize;
		for (blocksize = EXT2_MIN_BLOCK_SIZE;
		     blocksize <= EXT2_MAX_BLOCK_SIZE; blocksize *= 2) {
			retval = ext2fs_open2(ctx->filesystem_name,
					      ctx->io_options, flags,
					      ctx->superblock, blocksize,
					      io_ptr, &fs);
			if (!retval)
				break;
		}
	} else
		retval = ext2fs_open2(ctx->filesystem_name, ctx->io_options,
				      flags, 0, 0, io_ptr, &fs);
	if (!ctx->superblock && !(ctx->options & E2F_OPT_PREEN) &&
	    !(ctx->flags & E2F_FLAG_SB_SPECIFIED) &&
	    ((retval == EXT2_ET_BAD_MAGIC) ||
	     ((retval == 0) && ext2fs_check_desc(fs)))) {
		if (!fs || (fs->group_desc_count > 1)) {
			printf(_("%s trying backup blocks...\n"),
			       retval ? _("Couldn't find ext2 superblock,") :
			       _("Group descriptors look bad..."));
			get_backup_sb(ctx, fs, ctx->filesystem_name, io_ptr);
			if (fs)
				ext2fs_close(fs);
			goto restart;
		}
	}
	if (retval) {
		bb_error_msg(_("while trying to open %s"),
			ctx->filesystem_name);
		if (retval == EXT2_ET_REV_TOO_HIGH) {
			printf(_("The filesystem revision is apparently "
			       "too high for this version of e2fsck.\n"
			       "(Or the filesystem superblock "
			       "is corrupt)\n\n"));
			fix_problem(ctx, PR_0_SB_CORRUPT, &pctx);
		} else if (retval == EXT2_ET_SHORT_READ)
			printf(_("Could this be a zero-length partition?\n"));
		else if ((retval == EPERM) || (retval == EACCES))
			printf(_("You must have %s access to the "
			       "filesystem or be root\n"),
			       (ctx->options & E2F_OPT_READONLY) ?
			       "r/o" : "r/w");
		else if (retval == ENXIO)
			printf(_("Possibly non-existent or swap device?\n"));
#ifdef EROFS
		else if (retval == EROFS)
			printf(_("Disk write-protected; use the -n option "
			       "to do a read-only\n"
			       "check of the device.\n"));
#endif
		else
			fix_problem(ctx, PR_0_SB_CORRUPT, &pctx);
		bb_error_msg_and_die(0);
	}
	ctx->fs = fs;
	fs->priv_data = ctx;
	sb = fs->super;
	if (sb->s_rev_level > E2FSCK_CURRENT_REV) {
		bb_error_msg(_("while trying to open %s"),
			ctx->filesystem_name);
	get_newer:
		bb_error_msg_and_die(_("Get a newer version of e2fsck!"));
	}

	/*
	 * Set the device name, which is used whenever we print error
	 * or informational messages to the user.
	 */
	if (ctx->device_name == 0 &&
	    (sb->s_volume_name[0] != 0)) {
		ctx->device_name = string_copy(sb->s_volume_name,
					       sizeof(sb->s_volume_name));
	}
	if (ctx->device_name == 0)
		ctx->device_name = ctx->filesystem_name;

	/*
	 * Make sure the ext3 superblock fields are consistent.
	 */
	retval = e2fsck_check_ext3_journal(ctx);
	if (retval) {
		bb_error_msg(_("while checking ext3 journal for %s"),
			ctx->device_name);
		bb_error_msg_and_die(0);
	}

	/*
	 * Check to see if we need to do ext3-style recovery.  If so,
	 * do it, and then restart the fsck.
	 */
	if (sb->s_feature_incompat & EXT3_FEATURE_INCOMPAT_RECOVER) {
		if (ctx->options & E2F_OPT_READONLY) {
			printf(_("Warning: skipping journal recovery "
				 "because doing a read-only filesystem "
				 "check.\n"));
			io_channel_flush(ctx->fs->io);
		} else {
			if (ctx->flags & E2F_FLAG_RESTARTED) {
				/*
				 * Whoops, we attempted to run the
				 * journal twice.  This should never
				 * happen, unless the hardware or
				 * device driver is being bogus.
				 */
				bb_error_msg(_("can't set superblock flags on %s"), ctx->device_name);
				bb_error_msg_and_die(0);
			}
			retval = e2fsck_run_ext3_journal(ctx);
			if (retval) {
				bb_error_msg(_("while recovering ext3 journal of %s"),
					ctx->device_name);
				bb_error_msg_and_die(0);
			}
			ext2fs_close(ctx->fs);
			ctx->fs = 0;
			ctx->flags |= E2F_FLAG_RESTARTED;
			goto restart;
		}
	}

	/*
	 * Check for compatibility with the feature sets.  We need to
	 * be more stringent than ext2fs_open().
	 */
	if ((sb->s_feature_compat & ~EXT2_LIB_FEATURE_COMPAT_SUPP) ||
	    (sb->s_feature_incompat & ~EXT2_LIB_FEATURE_INCOMPAT_SUPP)) {
		bb_error_msg("(%s)", ctx->device_name);
		goto get_newer;
	}
	if (sb->s_feature_ro_compat & ~EXT2_LIB_FEATURE_RO_COMPAT_SUPP) {
		bb_error_msg("(%s)", ctx->device_name);
		goto get_newer;
	}
#ifdef ENABLE_COMPRESSION
	/* FIXME - do we support this at all? */
	if (sb->s_feature_incompat & EXT2_FEATURE_INCOMPAT_COMPRESSION)
		bb_error_msg(_("warning: compression support is experimental"));
#endif
#ifndef ENABLE_HTREE
	if (sb->s_feature_compat & EXT2_FEATURE_COMPAT_DIR_INDEX) {
		bb_error_msg(_("E2fsck not compiled with HTREE support,\n\t"
			  "but filesystem %s has HTREE directories."),
			ctx->device_name);
		goto get_newer;
	}
#endif

	/*
	 * If the user specified a specific superblock, presumably the
	 * master superblock has been trashed.  So we mark the
	 * superblock as dirty, so it can be written out.
	 */
	if (ctx->superblock &&
	    !(ctx->options & E2F_OPT_READONLY))
		ext2fs_mark_super_dirty(fs);

	/*
	 * We only update the master superblock because (a) paranoia;
	 * we don't want to corrupt the backup superblocks, and (b) we
	 * don't need to update the mount count and last checked
	 * fields in the backup superblock (the kernel doesn't
	 * update the backup superblocks anyway).
	 */
	fs->flags |= EXT2_FLAG_MASTER_SB_ONLY;

	ehandler_init(fs->io);

	if (ctx->superblock)
		set_latch_flags(PR_LATCH_RELOC, PRL_LATCHED, 0);
	ext2fs_mark_valid(fs);
	check_super_block(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		bb_error_msg_and_die(0);
	check_if_skip(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		bb_error_msg_and_die(0);
#ifdef ENABLE_SWAPFS

#ifdef WORDS_BIGENDIAN
#define NATIVE_FLAG EXT2_FLAG_SWAP_BYTES
#else
#define NATIVE_FLAG 0
#endif


	if (normalize_swapfs) {
		if ((fs->flags & EXT2_FLAG_SWAP_BYTES) == NATIVE_FLAG) {
			fprintf(stderr, _("%s: Filesystem byte order "
				"already normalized.\n"), ctx->device_name);
			bb_error_msg_and_die(0);
		}
	}
	if (swapfs) {
		swap_filesys(ctx);
		if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
			bb_error_msg_and_die(0);
	}
#endif

	/*
	 * Mark the system as valid, 'til proven otherwise
	 */
	ext2fs_mark_valid(fs);

	retval = ext2fs_read_bb_inode(fs, &fs->badblocks);
	if (retval) {
		bb_error_msg(_("while reading bad blocks inode"));
		preenhalt(ctx);
		printf(_("This doesn't bode well,"
			 " but we'll try to go on...\n"));
	}

	run_result = e2fsck_run(ctx);
	e2fsck_clear_progbar(ctx);
	if (run_result == E2F_FLAG_RESTART) {
		printf(_("Restarting e2fsck from the beginning...\n"));
		retval = e2fsck_reset_context(ctx);
		if (retval) {
			bb_error_msg(_("while resetting context"));
			bb_error_msg_and_die(0);
		}
		ext2fs_close(fs);
		goto restart;
	}
	if (run_result & E2F_FLAG_CANCEL) {
		printf(_("%s: e2fsck canceled.\n"), ctx->device_name ?
		       ctx->device_name : ctx->filesystem_name);
		exit_value |= FSCK_CANCELED;
	}
	if (run_result & E2F_FLAG_ABORT)
		bb_error_msg_and_die(_("aborted"));

	/* Cleanup */
	if (ext2fs_test_changed(fs)) {
		exit_value |= EXIT_NONDESTRUCT;
		if (!(ctx->options & E2F_OPT_PREEN))
		    printf(_("\n%s: ***** FILE SYSTEM WAS MODIFIED *****\n"),
			       ctx->device_name);
		if (ctx->mount_flags & EXT2_MF_ISROOT) {
			printf(_("%s: ***** REBOOT LINUX *****\n"),
			       ctx->device_name);
			exit_value |= EXIT_DESTRUCT;
		}
	}
	if (!ext2fs_test_valid(fs)) {
		printf(_("\n%s: ********** WARNING: Filesystem still has "
			 "errors **********\n\n"), ctx->device_name);
		exit_value |= EXIT_UNCORRECTED;
		exit_value &= ~EXIT_NONDESTRUCT;
	}
	if (exit_value & FSCK_CANCELED)
		exit_value &= ~EXIT_NONDESTRUCT;
	else {
		show_stats(ctx);
		if (!(ctx->options & E2F_OPT_READONLY)) {
			if (ext2fs_test_valid(fs)) {
				if (!(sb->s_state & EXT2_VALID_FS))
					exit_value |= EXIT_NONDESTRUCT;
				sb->s_state = EXT2_VALID_FS;
			} else
				sb->s_state &= ~EXT2_VALID_FS;
			sb->s_mnt_count = 0;
			sb->s_lastcheck = time(NULL);
			ext2fs_mark_super_dirty(fs);
		}
	}

	e2fsck_write_bitmaps(ctx);

	ext2fs_close(fs);
	ctx->fs = NULL;
	free(ctx->filesystem_name);
	free(ctx->journal_name);
	e2fsck_free_context(ctx);

	return exit_value;
}
