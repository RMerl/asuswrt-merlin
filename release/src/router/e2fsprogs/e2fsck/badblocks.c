/*
 * badblocks.c --- replace/append bad blocks to the bad block inode
 *
 * Copyright (C) 1993, 1994 Theodore Ts'o.  This file may be
 * redistributed under the terms of the GNU Public License.
 */

#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <et/com_err.h>
#include "e2fsck.h"

static int check_bb_inode_blocks(ext2_filsys fs, blk_t *block_nr, int blockcnt,
				 void *priv_data);


static void invalid_block(ext2_filsys fs EXT2FS_ATTR((unused)), blk_t blk)
{
	printf(_("Bad block %u out of range; ignored.\n"), blk);
	return;
}

void read_bad_blocks_file(e2fsck_t ctx, const char *bad_blocks_file,
			  int replace_bad_blocks)
{
	ext2_filsys fs = ctx->fs;
	errcode_t	retval;
	badblocks_list	bb_list = 0;
	FILE		*f;
	char		buf[1024];

	e2fsck_read_bitmaps(ctx);

	/*
	 * Make sure the bad block inode is sane.  If there are any
	 * illegal blocks, clear them.
	 */
	retval = ext2fs_block_iterate(fs, EXT2_BAD_INO, 0, 0,
				      check_bb_inode_blocks, 0);
	if (retval) {
		com_err("ext2fs_block_iterate", retval,
			_("while sanity checking the bad blocks inode"));
		goto fatal;
	}

	/*
	 * If we're appending to the bad blocks inode, read in the
	 * current bad blocks.
	 */
	if (!replace_bad_blocks) {
		retval = ext2fs_read_bb_inode(fs, &bb_list);
		if (retval) {
			com_err("ext2fs_read_bb_inode", retval,
				_("while reading the bad blocks inode"));
			goto fatal;
		}
	}

	/*
	 * Now read in the bad blocks from the file; if
	 * bad_blocks_file is null, then try to run the badblocks
	 * command.
	 */
	if (bad_blocks_file) {
		f = fopen(bad_blocks_file, "r");
		if (!f) {
			com_err("read_bad_blocks_file", errno,
				_("while trying to open %s"), bad_blocks_file);
			goto fatal;
		}
	} else {
		sprintf(buf, "badblocks -b %d -X %s%s%s %d", fs->blocksize,
			(ctx->options & E2F_OPT_PREEN) ? "" : "-s ",
			(ctx->options & E2F_OPT_WRITECHECK) ? "-n " : "",
			fs->device_name, fs->super->s_blocks_count-1);
		f = popen(buf, "r");
		if (!f) {
			com_err("read_bad_blocks_file", errno,
				_("while trying popen '%s'"), buf);
			goto fatal;
		}
	}
	retval = ext2fs_read_bb_FILE(fs, f, &bb_list, invalid_block);
	if (bad_blocks_file)
		fclose(f);
	else
		pclose(f);
	if (retval) {
		com_err("ext2fs_read_bb_FILE", retval,
			_("while reading in list of bad blocks from file"));
		goto fatal;
	}

	/*
	 * Finally, update the bad blocks from the bad_block_map
	 */
	printf("%s: Updating bad block inode.\n", ctx->device_name);
	retval = ext2fs_update_bb_inode(fs, bb_list);
	if (retval) {
		com_err("ext2fs_update_bb_inode", retval,
			_("while updating bad block inode"));
		goto fatal;
	}

	ext2fs_badblocks_list_free(bb_list);
	return;

fatal:
	ctx->flags |= E2F_FLAG_ABORT;
	return;

}

static int check_bb_inode_blocks(ext2_filsys fs,
				 blk_t *block_nr,
				 int blockcnt EXT2FS_ATTR((unused)),
				 void *priv_data EXT2FS_ATTR((unused)))
{
	if (!*block_nr)
		return 0;

	/*
	 * If the block number is outrageous, clear it and ignore it.
	 */
	if (*block_nr >= fs->super->s_blocks_count ||
	    *block_nr < fs->super->s_first_data_block) {
		printf(_("Warning: illegal block %u found in bad block inode.  "
			 "Cleared.\n"), *block_nr);
		*block_nr = 0;
		return BLOCK_CHANGED;
	}

	return 0;
}

