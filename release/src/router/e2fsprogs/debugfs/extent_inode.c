/*
 * extent_inode.c --- direct extent tree manipulation
 *
 * Copyright (C) 2012 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/types.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int optind;
extern char *optarg;
#endif

#include "debugfs.h"

ext2_ino_t	current_ino;
ext2_extent_handle_t current_handle;

static void dbg_print_extent(char *desc, struct ext2fs_extent *extent)
{
	if (desc)
		printf("%s: ", desc);
	printf("extent: lblk %llu--%llu, len %u, pblk %llu, flags: ",
	       extent->e_lblk, extent->e_lblk + extent->e_len - 1,
	       extent->e_len, extent->e_pblk);
	if (extent->e_flags & EXT2_EXTENT_FLAGS_LEAF)
		fputs("LEAF ", stdout);
	if (extent->e_flags & EXT2_EXTENT_FLAGS_UNINIT)
		fputs("UNINIT ", stdout);
	if (extent->e_flags & EXT2_EXTENT_FLAGS_SECOND_VISIT)
		fputs("2ND_VISIT ", stdout);
	if (!extent->e_flags)
		fputs("(none)", stdout);
	fputc('\n', stdout);

}

static int common_extent_args_process(int argc, char *argv[], int min_argc,
				      int max_argc, const char *cmd,
				      const char *usage, int flags)
{
	if (common_args_process(argc, argv, min_argc, max_argc, cmd,
				usage, flags))
		return 1;

	if (!current_handle) {
		com_err(cmd, 0, "Extent handle not open");
		return 1;
	}
	return 0;
}

static char *orig_prompt, *extent_prompt;

void do_extent_open(int argc, char *argv[])
{
	ext2_ino_t	inode;
	int		ret;
	errcode_t	retval;
	char		*cp;

	if (check_fs_open(argv[0]))
		return;

	if (argc == 1) {
		if (current_ino)
			printf("Current inode is %d\n", current_ino);
		else
			printf("No current inode\n");
		return;
	}

	if (common_inode_args_process(argc, argv, &inode, 0))
		return;

	current_ino = 0;

	retval = ext2fs_extent_open(current_fs, inode, &current_handle);
	if (retval) {
		com_err(argv[1], retval, "while opening extent handle");
		return;
	}

	current_ino = inode;

	orig_prompt = ss_get_prompt(sci_idx);
	extent_prompt = malloc(strlen(orig_prompt) + 32);
	strcpy(extent_prompt, orig_prompt);
	cp = strchr(extent_prompt, ':');
	if (cp)
		*cp = 0;
	sprintf(extent_prompt + strlen(extent_prompt), " (extent ino %d): ",
		current_ino);
	ss_add_request_table(sci_idx, &extent_cmds, 1, &ret);
	ss_set_prompt(sci_idx, extent_prompt);
	return;
}

void do_extent_close(int argc, char *argv[])
{
	int ret;

	if (common_args_process(argc, argv, 1, 1,
				"extent_close", "", 0))
		return;

	if (!current_handle) {
		com_err(argv[0], 0, "Extent handle not open");
		return;
	}

	ext2fs_extent_free(current_handle);
	current_handle = NULL;
	current_ino = 0;
	ss_delete_request_table(sci_idx, &extent_cmds, &ret);
	ss_set_prompt(sci_idx, orig_prompt);
	free(extent_prompt);
	extent_prompt = NULL;
}

static void generic_goto_node(const char *my_name, int argc,
			      char **argv, int op)
{
	struct ext2fs_extent	extent;
	errcode_t		retval;

	if (my_name && common_args_process(argc, argv, 1, 1,
					   my_name, "", 0))
		return;

	if (!current_handle) {
		com_err(argv[0], 0, "Extent handle not open");
		return;
	}

	retval = ext2fs_extent_get(current_handle, op, &extent);
	if (retval) {
		com_err(argv[0], retval, 0);
		return;
	}
	dbg_print_extent(0, &extent);
}

void do_current_node(int argc, char *argv[])
{
	generic_goto_node("current_node", argc, argv, EXT2_EXTENT_CURRENT);
}

void do_root_node(int argc, char *argv[])
{
	generic_goto_node("root_node", argc, argv, EXT2_EXTENT_ROOT);
}

void do_last_leaf(int argc, char *argv[])
{
	generic_goto_node("last_leaf", argc, argv, EXT2_EXTENT_LAST_LEAF);
}

void do_first_sib(int argc, char *argv[])
{
	generic_goto_node("first_sib", argc, argv, EXT2_EXTENT_FIRST_SIB);
}

void do_last_sib(int argc, char *argv[])
{
	generic_goto_node("next_sib", argc, argv, EXT2_EXTENT_LAST_SIB);
}

void do_next_sib(int argc, char *argv[])
{
	generic_goto_node("next_sib", argc, argv, EXT2_EXTENT_NEXT_SIB);
}

void do_prev_sib(int argc, char *argv[])
{
	generic_goto_node("prev_sib", argc, argv, EXT2_EXTENT_PREV_SIB);
}

void do_next_leaf(int argc, char *argv[])
{
	generic_goto_node("next_leaf", argc, argv, EXT2_EXTENT_NEXT_LEAF);
}

void do_prev_leaf(int argc, char *argv[])
{
	generic_goto_node("prev_leaf", argc, argv, EXT2_EXTENT_PREV_LEAF);
}

void do_next(int argc, char *argv[])
{
	generic_goto_node("next", argc, argv, EXT2_EXTENT_NEXT);
}

void do_prev(int argc, char *argv[])
{
	generic_goto_node("prev", argc, argv, EXT2_EXTENT_PREV);
}

void do_up(int argc, char *argv[])
{
	generic_goto_node("up", argc, argv, EXT2_EXTENT_UP);
}

void do_down(int argc, char *argv[])
{
	generic_goto_node("down", argc, argv, EXT2_EXTENT_DOWN);
}

void do_delete_node(int argc, char *argv[])
{
	struct ext2fs_extent extent;
	errcode_t	retval;

	if (common_extent_args_process(argc, argv, 1, 1, "delete_node",
				       "", CHECK_FS_RW | CHECK_FS_BITMAPS))
		return;

	retval = ext2fs_extent_delete(current_handle, 0);
	if (retval) {
		com_err(argv[0], retval, 0);
		return;
	}

	retval = ext2fs_extent_get(current_handle, EXT2_EXTENT_CURRENT,
				   &extent);
	if (retval)
		return;
	dbg_print_extent(0, &extent);
}

void do_replace_node(int argc, char *argv[])
{
	const char	*usage = "[--uninit] <lblk> <len> <pblk>";
	errcode_t	retval;
	struct ext2fs_extent extent;
	int err;

	if (common_extent_args_process(argc, argv, 3, 5, "replace_node",
				       usage, CHECK_FS_RW | CHECK_FS_BITMAPS))
		return;

	extent.e_flags = 0;

	if (!strcmp(argv[1], "--uninit")) {
		argc--;
		argv++;
		extent.e_flags |= EXT2_EXTENT_FLAGS_UNINIT;
	}

	if (argc != 4) {
		fprintf(stderr, "Usage: %s %s\n", argv[0], usage);
		return;
	}

	extent.e_lblk = parse_ulong(argv[1], argv[0], "logical block", &err);
	if (err)
		return;

	extent.e_len = parse_ulong(argv[2], argv[0], "logical block", &err);
	if (err)
		return;

	extent.e_pblk = parse_ulong(argv[3], argv[0], "logical block", &err);
	if (err)
		return;

	retval = ext2fs_extent_replace(current_handle, 0, &extent);
	if (retval) {
		com_err(argv[0], retval, 0);
		return;
	}
	do_current_node(argc, argv);
}

void do_split_node(int argc, char *argv[])
{
	errcode_t	retval;

	if (common_extent_args_process(argc, argv, 1, 1, "split_node",
				       "", CHECK_FS_RW | CHECK_FS_BITMAPS))
		return;

	retval = ext2fs_extent_node_split(current_handle);
	if (retval) {
		com_err(argv[0], retval, 0);
		return;
	}
	do_current_node(argc, argv);
}

void do_insert_node(int argc, char *argv[])
{
	const char	*usage = "[--after] [--uninit] <lblk> <len> <pblk>";
	errcode_t	retval;
	struct ext2fs_extent extent;
	char *cmd;
	int err;
	int flags = 0;

	if (common_extent_args_process(argc, argv, 3, 6, "insert_node",
				       usage, CHECK_FS_RW | CHECK_FS_BITMAPS))
		return;

	cmd = argv[0];

	extent.e_flags = 0;

	while (argc > 2) {
		if (!strcmp(argv[1], "--after")) {
			argc--;
			argv++;
			flags |= EXT2_EXTENT_INSERT_AFTER;
			continue;
		}
		if (!strcmp(argv[1], "--uninit")) {
			argc--;
			argv++;
			extent.e_flags |= EXT2_EXTENT_FLAGS_UNINIT;
			continue;
		}
		break;
	}

	if (argc != 4) {
		fprintf(stderr, "usage: %s %s\n", cmd, usage);
		return;
	}

	extent.e_lblk = parse_ulong(argv[1], cmd,
				    "logical block", &err);
	if (err)
		return;

	extent.e_len = parse_ulong(argv[2], cmd,
				    "length", &err);
	if (err)
		return;

	extent.e_pblk = parse_ulong(argv[3], cmd,
				    "pysical block", &err);
	if (err)
		return;

	retval = ext2fs_extent_insert(current_handle, flags, &extent);
	if (retval) {
		com_err(cmd, retval, 0);
		return;
	}
	do_current_node(argc, argv);
}

void do_set_bmap(int argc, char **argv)
{
	const char	*usage = "[--uninit] <lblk> <pblk>";
	struct ext2fs_extent extent;
	errcode_t	retval;
	blk_t		logical;
	blk_t		physical;
	char		*cmd = argv[0];
	int		flags = 0;
	int		err;

	if (common_extent_args_process(argc, argv, 3, 5, "set_bmap",
				       usage, CHECK_FS_RW | CHECK_FS_BITMAPS))
		return;

	if (argc > 2 && !strcmp(argv[1], "--uninit")) {
		argc--;
		argv++;
		flags |= EXT2_EXTENT_SET_BMAP_UNINIT;
	}

	if (argc != 3) {
		fprintf(stderr, "Usage: %s %s\n", cmd, usage);
		return;
	}

	logical = parse_ulong(argv[1], cmd,
				    "logical block", &err);
	if (err)
		return;

	physical = parse_ulong(argv[2], cmd,
				    "physical block", &err);
	if (err)
		return;

	retval = ext2fs_extent_set_bmap(current_handle, logical,
					(blk64_t) physical, flags);
	if (retval) {
		com_err(cmd, retval, 0);
		return;
	}

	retval = ext2fs_extent_get(current_handle, EXT2_EXTENT_CURRENT,
				   &extent);
	if (retval)
		return;
	dbg_print_extent(0, &extent);
}

void do_print_all(int argc, char **argv)
{
	const char	*usage = "[--leaf-only|--reverse|--reverse-leaf]";
	struct ext2fs_extent	extent;
	errcode_t		retval;
	errcode_t		end_err = EXT2_ET_EXTENT_NO_NEXT;
	int			op = EXT2_EXTENT_NEXT;
	int			first_op = EXT2_EXTENT_ROOT;


	if (common_extent_args_process(argc, argv, 1, 2, "print_all",
				       usage, 0))
		return;

	if (argc == 2) {
		if (!strcmp(argv[1], "--leaf-only"))
			op = EXT2_EXTENT_NEXT_LEAF;
		else if (!strcmp(argv[1], "--reverse")) {
			op = EXT2_EXTENT_PREV;
			first_op = EXT2_EXTENT_LAST_LEAF;
			end_err = EXT2_ET_EXTENT_NO_PREV;
		} else if (!strcmp(argv[1], "--reverse-leaf")) {
			op = EXT2_EXTENT_PREV_LEAF;
			first_op = EXT2_EXTENT_LAST_LEAF;
			end_err = EXT2_ET_EXTENT_NO_PREV;
		} else {
			fprintf(stderr, "Usage: %s %s\n", argv[0], usage);
			return;
		}
	}

	retval = ext2fs_extent_get(current_handle, first_op, &extent);
	if (retval) {
		com_err(argv[0], retval, 0);
		return;
	}
	dbg_print_extent(0, &extent);

	while (1) {
		retval = ext2fs_extent_get(current_handle, op, &extent);
		if (retval == end_err)
			break;

		if (retval) {
			com_err(argv[0], retval, 0);
			return;
		}
		dbg_print_extent(0, &extent);
	}
}

void do_fix_parents(int argc, char **argv)
{
	errcode_t		retval;

	if (common_extent_args_process(argc, argv, 1, 1, "fix_parents", "",
				       CHECK_FS_RW))
		return;

	retval = ext2fs_extent_fix_parents(current_handle);
	if (retval) {
		com_err(argv[0], retval, 0);
		return;
	}
}

void do_info(int argc, char **argv)
{
	struct ext2fs_extent	extent;
	struct ext2_extent_info	info;
	errcode_t		retval;

	if (common_extent_args_process(argc, argv, 1, 1, "info", "", 0))
		return;

	retval = ext2fs_extent_get_info(current_handle, &info);
	if (retval) {
		com_err(argv[0], retval, 0);
		return;
	}

	retval = ext2fs_extent_get(current_handle,
				   EXT2_EXTENT_CURRENT, &extent);
	if (retval) {
		com_err(argv[0], retval, 0);
		return;
	}

	dbg_print_extent(0, &extent);

	printf("Current handle location: %d/%d (max: %d, bytes %d), level %d/%d\n",
	       info.curr_entry, info.num_entries, info.max_entries,
	       info.bytes_avail, info.curr_level, info.max_depth);
	printf("\tmax lblk: %llu, max pblk: %llu\n", info.max_lblk,
	       info.max_pblk);
	printf("\tmax_len: %u, max_uninit_len: %u\n", info.max_len,
	       info.max_uninit_len);
}

void do_goto_block(int argc, char **argv)
{
	errcode_t		retval;
	blk64_t			blk;
	int			level = 0, err;

	if (common_extent_args_process(argc, argv, 2, 3, "goto_block",
				       "block [level]", 0))
		return;

	if (strtoblk(argv[0], argv[1], &blk))
		return;

	if (argc == 3) {
		level = parse_ulong(argv[2], argv[0], "level", &err);
		if (err)
			return;
	}

	retval = ext2fs_extent_goto2(current_handle, level, (blk64_t) blk);

	if (retval) {
		com_err(argv[0], retval,
			"while trying to go to block %llu, level %d",
			(unsigned long long) blk, level);
		return;
	}

	generic_goto_node(NULL, argc, argv, EXT2_EXTENT_CURRENT);
}
