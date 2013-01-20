/*
 * ncheck.c --- given a list of inodes, generate a list of names
 *
 * Copyright (C) 1994 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

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

#include "debugfs.h"

struct inode_walk_struct {
	ext2_ino_t		*iarray;
	int			inodes_left;
	int			num_inodes;
	int			position;
	char			*parent;
};

static int ncheck_proc(struct ext2_dir_entry *dirent,
		       int	offset EXT2FS_ATTR((unused)),
		       int	blocksize EXT2FS_ATTR((unused)),
		       char	*buf EXT2FS_ATTR((unused)),
		       void	*private)
{
	struct inode_walk_struct *iw = (struct inode_walk_struct *) private;
	int	i;

	iw->position++;
	if (iw->position <= 2)
		return 0;
	for (i=0; i < iw->num_inodes; i++) {
		if (iw->iarray[i] == dirent->inode) {
			printf("%u\t%s/%.*s\n", iw->iarray[i], iw->parent,
			       (dirent->name_len & 0xFF), dirent->name);
		}
	}
	if (!iw->inodes_left)
		return DIRENT_ABORT;

	return 0;
}

void do_ncheck(int argc, char **argv)
{
	struct inode_walk_struct iw;
	int			i;
	ext2_inode_scan		scan = 0;
	ext2_ino_t		ino;
	struct ext2_inode	inode;
	errcode_t		retval;
	char			*tmp;

	if (argc < 2) {
		com_err(argv[0], 0, "Usage: ncheck <inode number> ...");
		return;
	}
	if (check_fs_open(argv[0]))
		return;

	iw.iarray = malloc(sizeof(ext2_ino_t) * argc);
	if (!iw.iarray) {
		com_err("ncheck", ENOMEM,
			"while allocating inode info array");
		return;
	}
	memset(iw.iarray, 0, sizeof(ext2_ino_t) * argc);

	for (i=1; i < argc; i++) {
		iw.iarray[i-1] = strtol(argv[i], &tmp, 0);
		if (*tmp) {
			com_err(argv[0], 0, "Bad inode - %s", argv[i]);
			goto error_out;
		}
	}

	iw.num_inodes = iw.inodes_left = argc-1;

	retval = ext2fs_open_inode_scan(current_fs, 0, &scan);
	if (retval) {
		com_err("ncheck", retval, "while opening inode scan");
		goto error_out;
	}

	do {
		retval = ext2fs_get_next_inode(scan, &ino, &inode);
	} while (retval == EXT2_ET_BAD_BLOCK_IN_INODE_TABLE);
	if (retval) {
		com_err("ncheck", retval, "while starting inode scan");
		goto error_out;
	}

	printf("Inode\tPathname\n");
	while (ino) {
		if (!inode.i_links_count)
			goto next;
		/*
		 * To handle filesystems touched by 0.3c extfs; can be
		 * removed later.
		 */
		if (inode.i_dtime)
			goto next;
		/* Ignore anything that isn't a directory */
		if (!LINUX_S_ISDIR(inode.i_mode))
			goto next;

		iw.position = 0;

		retval = ext2fs_get_pathname(current_fs, ino, 0, &iw.parent);
		if (retval) {
			com_err("ncheck", retval, 
				"while calling ext2fs_get_pathname");
			goto next;
		}

		retval = ext2fs_dir_iterate(current_fs, ino, 0, 0,
					    ncheck_proc, &iw);
		ext2fs_free_mem(&iw.parent);
		if (retval) {
			com_err("ncheck", retval,
				"while calling ext2_dir_iterate");
			goto next;
		}

		if (iw.inodes_left == 0)
			break;

	next:
		do {
			retval = ext2fs_get_next_inode(scan, &ino, &inode);
		} while (retval == EXT2_ET_BAD_BLOCK_IN_INODE_TABLE);

		if (retval) {
			com_err("ncheck", retval,
				"while doing inode scan");
			goto error_out;
		}
	}

error_out:
	free(iw.iarray);
	if (scan)
		ext2fs_close_inode_scan(scan);
	return;
}



