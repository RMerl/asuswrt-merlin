/* vi: set sw=4 ts=4: */
/*
 * brel.h
 *
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

struct ext2_block_relocate_entry {
	blk_t	new;
	__s16	offset;
	__u16	flags;
	union {
		blk_t		block_ref;
		ext2_ino_t	inode_ref;
	} owner;
};

#define RELOCATE_TYPE_REF  0x0007
#define RELOCATE_BLOCK_REF 0x0001
#define RELOCATE_INODE_REF 0x0002

typedef struct ext2_block_relocation_table *ext2_brel;

struct ext2_block_relocation_table {
	__u32	magic;
	char	*name;
	blk_t	current;
	void	*priv_data;

	/*
	 * Add a block relocation entry.
	 */
	errcode_t (*put)(ext2_brel brel, blk_t old,
			      struct ext2_block_relocate_entry *ent);

	/*
	 * Get a block relocation entry.
	 */
	errcode_t (*get)(ext2_brel brel, blk_t old,
			      struct ext2_block_relocate_entry *ent);

	/*
	 * Initialize for iterating over the block relocation entries.
	 */
	errcode_t (*start_iter)(ext2_brel brel);

	/*
	 * The iterator function for the inode relocation entries.
	 * Returns an inode number of 0 when out of entries.
	 */
	errcode_t (*next)(ext2_brel brel, blk_t *old,
			  struct ext2_block_relocate_entry *ent);

	/*
	 * Move the inode relocation table from one block number to
	 * another.
	 */
	errcode_t (*move)(ext2_brel brel, blk_t old, blk_t new);

	/*
	 * Remove a block relocation entry.
	 */
	errcode_t (*delete)(ext2_brel brel, blk_t old);


	/*
	 * Free the block relocation table.
	 */
	errcode_t (*free)(ext2_brel brel);
};

errcode_t ext2fs_brel_memarray_create(char *name, blk_t max_block,
				    ext2_brel *brel);

#define ext2fs_brel_put(brel, old, ent) ((brel)->put((brel), old, ent))
#define ext2fs_brel_get(brel, old, ent) ((brel)->get((brel), old, ent))
#define ext2fs_brel_start_iter(brel) ((brel)->start_iter((brel)))
#define ext2fs_brel_next(brel, old, ent) ((brel)->next((brel), old, ent))
#define ext2fs_brel_move(brel, old, new) ((brel)->move((brel), old, new))
#define ext2fs_brel_delete(brel, old) ((brel)->delete((brel), old))
#define ext2fs_brel_free(brel) ((brel)->free((brel)))

