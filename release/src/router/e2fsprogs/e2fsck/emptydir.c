/*
 * emptydir.c --- clear empty directory blocks
 *
 * Copyright (C) 1998 Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 *
 * This file has the necessary routines to search for empty directory
 * blocks and get rid of them.
 */

#include "e2fsck.h"
#include "problem.h"

/*
 * For e2fsck.h
 */
struct empty_dir_info_struct {
	ext2_dblist empty_dblist;
	ext2fs_block_bitmap empty_dir_blocks;
	ext2fs_inode_bitmap dir_map;
	char *block_buf;
	ext2_ino_t ino;
	struct ext2_inode inode;
	blk_t	logblk;
	blk_t	freed_blocks;
};

typedef struct empty_dir_info_struct *empty_dir_info;

extern empty_dir_info init_empty_dir(e2fsck_t ctx);
extern void free_empty_dirblock(empty_dir_info edi);
extern void add_empty_dirblock(empty_dir_info edi,
			       struct ext2_db_entry *db);
extern void process_empty_dirblock(e2fsck_t ctx, empty_dir_info edi);


empty_dir_info init_empty_dir(e2fsck_t ctx)
{
	empty_dir_info	edi;
	errcode_t	retval;

	edi = malloc(sizeof(struct empty_dir_info_struct));
	if (!edi)
		return NULL;

	memset(edi, 0, sizeof(struct empty_dir_info_struct));

	retval = ext2fs_init_dblist(ctx->fs, &edi->empty_dblist);
	if (retval)
		goto errout;

	retval = ext2fs_allocate_block_bitmap(ctx->fs, _("empty dirblocks"),
					      &edi->empty_dir_blocks);
	if (retval)
		goto errout;

	retval = ext2fs_allocate_inode_bitmap(ctx->fs, _("empty dir map"),
					      &edi->dir_map);
	if (retval)
		goto errout;

	return (edi);

errout:
	free_empty_dirblock(edi);
	return NULL;
}

void free_empty_dirblock(empty_dir_info edi)
{
	if (!edi)
		return;
	if (edi->empty_dblist)
		ext2fs_free_dblist(edi->empty_dblist);
	if (edi->empty_dir_blocks)
		ext2fs_free_block_bitmap(edi->empty_dir_blocks);
	if (edi->dir_map)
		ext2fs_free_inode_bitmap(edi->dir_map);

	memset(edi, 0, sizeof(struct empty_dir_info_struct));
	free(edi);
}

void add_empty_dirblock(empty_dir_info edi,
			struct ext2_db_entry *db)
{
	if (!edi || !db)
		return;

	if (db->ino == 11)
		return;		/* Inode number 11 is usually lost+found */

	printf(_("Empty directory block %u (#%d) in inode %u\n"),
	       db->blk, db->blockcnt, db->ino);

	ext2fs_mark_block_bitmap(edi->empty_dir_blocks, db->blk);
	if (ext2fs_test_inode_bitmap(edi->dir_map, db->ino))
		return;
	ext2fs_mark_inode_bitmap(edi->dir_map, db->ino);

	ext2fs_add_dir_block(edi->empty_dblist, db->ino,
			     db->blk, db->blockcnt);
}

/*
 * Helper function used by fix_directory.
 *
 * XXX need to finish this.  General approach is to use bmap to
 * iterate over all of the logical blocks using the bmap function, and
 * copy the block reference as necessary.  Big question --- what do
 * about error recovery?
 *
 * Also question --- how to free the indirect blocks.
 */
int empty_pass1(ext2_filsys fs, blk_t *block_nr, e2_blkcnt_t blockcnt,
		blk_t ref_block, int ref_offset, void *priv_data)
{
	empty_dir_info edi = (empty_dir_info) priv_data;
	blk_t	block, new_block;
	errcode_t	retval;

	if (blockcnt < 0)
		return 0;
	block = *block_nr;
	do {
		retval = ext2fs_bmap(fs, edi->ino, &edi->inode,
				     edi->block_buf, 0, edi->logblk,
				     &new_block);
		if (retval)
			return DIRENT_ABORT;   /* XXX what to do? */
		if (new_block == 0)
			break;
		edi->logblk++;
	} while (ext2fs_test_block_bitmap(edi->empty_dir_blocks, new_block));

	if (new_block == block)
		return 0;
	if (new_block == 0)
		edi->freed_blocks++;
	*block_nr = new_block;
	return BLOCK_CHANGED;
}

static int fix_directory(ext2_filsys fs,
			 struct ext2_db_entry *db,
			 void *priv_data)
{
	errcode_t	retval;

	empty_dir_info edi = (empty_dir_info) priv_data;

	edi->logblk = 0;
	edi->freed_blocks = 0;
	edi->ino = db->ino;

	retval = ext2fs_read_inode(fs, db->ino, &edi->inode);
	if (retval)
		return 0;

	retval = ext2fs_block_iterate2(fs, db->ino, 0, edi->block_buf,
				       empty_pass1, edi);
	if (retval)
		return 0;

	if (edi->freed_blocks) {
		edi->inode.i_size -= edi->freed_blocks * fs->blocksize;
		ext2fs_iblk_add_blocks(fs, &edi->inode, edi->freed_blocks);
		retval = ext2fs_write_inode(fs, db->ino, &edi->inode);
		if (retval)
			return 0;
	}
	return 0;
}

void process_empty_dirblock(e2fsck_t ctx, empty_dir_info edi)
{
	if (!edi)
		return;

	edi->block_buf = malloc(ctx->fs->blocksize * 3);

	if (edi->block_buf) {
		(void) ext2fs_dblist_iterate(edi->empty_dblist,
					     fix_directory, &edi);
	}
	free(edi->block_buf);
	free_empty_dirblock(edi);
}

