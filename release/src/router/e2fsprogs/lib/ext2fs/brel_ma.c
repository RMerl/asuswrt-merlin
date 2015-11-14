/*
 * brel_ma.c
 *
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * TODO: rewrite to not use a direct array!!!  (Fortunately this
 * module isn't really used yet.)
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include "config.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include "ext2_fs.h"
#include "ext2fs.h"
#include "brel.h"

static errcode_t bma_put(ext2_brel brel, blk64_t old,
			struct ext2_block_relocate_entry *ent);
static errcode_t bma_get(ext2_brel brel, blk64_t old,
			struct ext2_block_relocate_entry *ent);
static errcode_t bma_start_iter(ext2_brel brel);
static errcode_t bma_next(ext2_brel brel, blk64_t *old,
			 struct ext2_block_relocate_entry *ent);
static errcode_t bma_move(ext2_brel brel, blk64_t old, blk64_t new);
static errcode_t bma_delete(ext2_brel brel, blk64_t old);
static errcode_t bma_free(ext2_brel brel);

struct brel_ma {
	__u32 magic;
	blk64_t max_block;
	struct ext2_block_relocate_entry *entries;
};

errcode_t ext2fs_brel_memarray_create(char *name, blk64_t max_block,
				      ext2_brel *new_brel)
{
	ext2_brel		brel = 0;
	errcode_t	retval;
	struct brel_ma 	*ma = 0;
	size_t		size;

	*new_brel = 0;

	/*
	 * Allocate memory structures
	 */
	retval = ext2fs_get_mem(sizeof(struct ext2_block_relocation_table),
				&brel);
	if (retval)
		goto errout;
	memset(brel, 0, sizeof(struct ext2_block_relocation_table));

	retval = ext2fs_get_mem(strlen(name)+1, &brel->name);
	if (retval)
		goto errout;
	strcpy(brel->name, name);

	retval = ext2fs_get_mem(sizeof(struct brel_ma), &ma);
	if (retval)
		goto errout;
	memset(ma, 0, sizeof(struct brel_ma));
	brel->priv_data = ma;

	size = (size_t) (sizeof(struct ext2_block_relocate_entry) *
			 (max_block+1));
	retval = ext2fs_get_array(max_block+1,
		sizeof(struct ext2_block_relocate_entry), &ma->entries);
	if (retval)
		goto errout;
	memset(ma->entries, 0, size);
	ma->max_block = max_block;

	/*
	 * Fill in the brel data structure
	 */
	brel->put = bma_put;
	brel->get = bma_get;
	brel->start_iter = bma_start_iter;
	brel->next = bma_next;
	brel->move = bma_move;
	brel->delete = bma_delete;
	brel->free = bma_free;

	*new_brel = brel;
	return 0;

errout:
	bma_free(brel);
	return retval;
}

static errcode_t bma_put(ext2_brel brel, blk64_t old,
			struct ext2_block_relocate_entry *ent)
{
	struct brel_ma 	*ma;

	ma = brel->priv_data;
	if (old > ma->max_block)
		return EXT2_ET_INVALID_ARGUMENT;
	ma->entries[(unsigned)old] = *ent;
	return 0;
}

static errcode_t bma_get(ext2_brel brel, blk64_t old,
			struct ext2_block_relocate_entry *ent)
{
	struct brel_ma 	*ma;

	ma = brel->priv_data;
	if (old > ma->max_block)
		return EXT2_ET_INVALID_ARGUMENT;
	if (ma->entries[(unsigned)old].new == 0)
		return ENOENT;
	*ent = ma->entries[old];
	return 0;
}

static errcode_t bma_start_iter(ext2_brel brel)
{
	brel->current = 0;
	return 0;
}

static errcode_t bma_next(ext2_brel brel, blk64_t *old,
			  struct ext2_block_relocate_entry *ent)
{
	struct brel_ma 	*ma;

	ma = brel->priv_data;
	while (++brel->current < ma->max_block) {
		if (ma->entries[(unsigned)brel->current].new == 0)
			continue;
		*old = brel->current;
		*ent = ma->entries[(unsigned)brel->current];
		return 0;
	}
	*old = 0;
	return 0;
}

static errcode_t bma_move(ext2_brel brel, blk64_t old, blk64_t new)
{
	struct brel_ma 	*ma;

	ma = brel->priv_data;
	if ((old > ma->max_block) || (new > ma->max_block))
		return EXT2_ET_INVALID_ARGUMENT;
	if (ma->entries[(unsigned)old].new == 0)
		return ENOENT;
	ma->entries[(unsigned)new] = ma->entries[old];
	ma->entries[(unsigned)old].new = 0;
	return 0;
}

static errcode_t bma_delete(ext2_brel brel, blk64_t old)
{
	struct brel_ma 	*ma;

	ma = brel->priv_data;
	if (old > ma->max_block)
		return EXT2_ET_INVALID_ARGUMENT;
	if (ma->entries[(unsigned)old].new == 0)
		return ENOENT;
	ma->entries[(unsigned)old].new = 0;
	return 0;
}

static errcode_t bma_free(ext2_brel brel)
{
	struct brel_ma 	*ma;

	if (!brel)
		return 0;

	ma = brel->priv_data;

	if (ma) {
		if (ma->entries)
			ext2fs_free_mem(&ma->entries);
		ext2fs_free_mem(&ma);
	}
	if (brel->name)
		ext2fs_free_mem(&brel->name);
	ext2fs_free_mem(&brel);
	return 0;
}
