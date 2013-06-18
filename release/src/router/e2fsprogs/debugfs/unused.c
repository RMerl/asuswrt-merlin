/*
 * unused.c --- quick and dirty unused space dumper
 *
 * Copyright (C) 1997 Theodore Ts'o.  This file may be redistributed
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

void do_dump_unused(int argc EXT2FS_ATTR((unused)), char **argv)
{
	blk64_t		blk;
	unsigned char	buf[EXT2_MAX_BLOCK_SIZE];
	unsigned int	i;
	errcode_t	retval;

	if (common_args_process(argc, argv, 1, 1,
				"dump_unused", "", 0))
		return;

	for (blk=current_fs->super->s_first_data_block;
	     blk < ext2fs_blocks_count(current_fs->super); blk++) {
		if (ext2fs_test_block_bitmap2(current_fs->block_map,blk))
			continue;
		retval = io_channel_read_blk64(current_fs->io, blk, 1, buf);
		if (retval) {
			com_err(argv[0], retval, "While reading block\n");
			return;
		}
		for (i=0; i < current_fs->blocksize; i++)
			if (buf[i])
				break;
		if (i >= current_fs->blocksize)
			continue;
		printf("\nUnused block %llu contains non-zero data:\n\n",
		       blk);
		for (i=0; i < current_fs->blocksize; i++)
			fputc(buf[i], stdout);
	}
}
