/*
 * tst_inode.c --- this function tests the inode scan function
 *
 * Copyright (C) 1996 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include "ext2_fs.h"
#include "ext2fs.h"

blk64_t test_vec[] = { 8, 12, 24, 34, 43, 44, 100, 0 };

ext2_filsys	test_fs;
ext2fs_block_bitmap bad_block_map, touched_map;
ext2fs_inode_bitmap bad_inode_map;
badblocks_list	test_badblocks;

int first_no_comma = 1;
int failed = 0;

static void iscan_test_read_blk64(unsigned long long block, int count, errcode_t err)
{
	int	i;

	if (first_no_comma)
		first_no_comma = 0;
	else
		printf(", ");

	if (count > 1)
		printf("%llu-%llu", block, block+count-1);
	else
		printf("%llu", block);

	for (i=0; i < count; i++, block++) {
		if (ext2fs_test_block_bitmap2(touched_map, block)) {
			printf("\nDuplicate block?!? --- %llu\n", block);
			failed++;
			first_no_comma = 1;
		}
		ext2fs_mark_block_bitmap2(touched_map, block);
	}
}

static void iscan_test_read_blk(unsigned long block, int count, errcode_t err)
{
	iscan_test_read_blk64(block, count, err);
}

/*
 * Setup the variables for doing the inode scan test.
 */
static void setup(void)
{
	errcode_t	retval;
	int		i;
	struct ext2_super_block param;

	initialize_ext2_error_table();

	memset(&param, 0, sizeof(param));
	ext2fs_blocks_count_set(&param, 12000);


	test_io_cb_read_blk = iscan_test_read_blk;
	test_io_cb_read_blk64 = iscan_test_read_blk64;

	retval = ext2fs_initialize("test fs", EXT2_FLAG_64BITS, &param,
				   test_io_manager, &test_fs);
	if (retval) {
		com_err("setup", retval,
			"While initializing filesystem");
		exit(1);
	}
	retval = ext2fs_allocate_tables(test_fs);
	if (retval) {
		com_err("setup", retval,
			"While allocating tables for test filesystem");
		exit(1);
	}
	retval = ext2fs_allocate_block_bitmap(test_fs, "bad block map",
					      &bad_block_map);
	if (retval) {
		com_err("setup", retval,
			"While allocating bad_block bitmap");
		exit(1);
	}
	retval = ext2fs_allocate_block_bitmap(test_fs, "touched map",
					      &touched_map);
	if (retval) {
		com_err("setup", retval,
			"While allocating touched block bitmap");
		exit(1);
	}
	retval = ext2fs_allocate_inode_bitmap(test_fs, "bad inode map",
					      &bad_inode_map);
	if (retval) {
		com_err("setup", retval,
			"While allocating bad inode bitmap");
		exit(1);
	}

	retval = ext2fs_badblocks_list_create(&test_badblocks, 5);
	if (retval) {
		com_err("setup", retval, "while creating badblocks list");
		exit(1);
	}
	for (i=0; test_vec[i]; i++) {
		retval = ext2fs_badblocks_list_add(test_badblocks, test_vec[i]);
		if (retval) {
			com_err("setup", retval,
				"while adding test vector %d", i);
			exit(1);
		}
		ext2fs_mark_block_bitmap2(bad_block_map, test_vec[i]);
	}
	test_fs->badblocks = test_badblocks;
}

/*
 * Iterate using inode_scan
 */
static void iterate(void)
{
	struct ext2_inode inode;
	ext2_inode_scan	scan;
	errcode_t	retval;
	ext2_ino_t	ino;

	retval = ext2fs_open_inode_scan(test_fs, 8, &scan);
	if (retval) {
		com_err("iterate", retval, "While opening inode scan");
		exit(1);
	}
	printf("Reading blocks: ");
	retval = ext2fs_get_next_inode(scan, &ino, &inode);
	if (retval) {
		com_err("iterate", retval, "while reading first inode");
		exit(1);
	}
	while (ino) {
		retval = ext2fs_get_next_inode(scan, &ino, &inode);
		if (retval == EXT2_ET_BAD_BLOCK_IN_INODE_TABLE) {
			ext2fs_mark_inode_bitmap2(bad_inode_map, ino);
			continue;
		}
		if (retval) {
			com_err("iterate", retval,
				"while getting next inode");
			exit(1);
		}
	}
	printf("\n");
	ext2fs_close_inode_scan(scan);
}

/*
 * Verify the touched map
 */
static void check_map(void)
{
	int	i, j, first=1;
	blk64_t	blk;

	for (i=0; test_vec[i]; i++) {
		if (ext2fs_test_block_bitmap2(touched_map, test_vec[i])) {
			printf("Bad block was touched --- %llu\n", test_vec[i]);
			failed++;
			first_no_comma = 1;
		}
		ext2fs_mark_block_bitmap2(touched_map, test_vec[i]);
	}
	for (i = 0; i < test_fs->group_desc_count; i++) {
		for (j=0, blk = ext2fs_inode_table_loc(test_fs, i);
		     j < test_fs->inode_blocks_per_group;
		     j++, blk++) {
			if (!ext2fs_test_block_bitmap2(touched_map, blk) &&
			    !ext2fs_test_block_bitmap2(bad_block_map, blk)) {
				printf("Missing block --- %llu\n", blk);
				failed++;
			}
		}
	}
	printf("Bad inodes: ");
	for (i=1; i <= test_fs->super->s_inodes_count; i++) {
		if (ext2fs_test_inode_bitmap2(bad_inode_map, i)) {
			if (first)
				first = 0;
			else
				printf(", ");
			printf("%u", i);
		}
	}
	printf("\n");
}


int main(int argc, char **argv)
{
	setup();
	iterate();
	check_map();
	if (!failed)
		printf("Inode scan tested OK!\n");
	return failed;
}

