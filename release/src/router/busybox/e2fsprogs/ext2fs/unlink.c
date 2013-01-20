/* vi: set sw=4 ts=4: */
/*
 * unlink.c --- delete links in a ext2fs directory
 *
 * Copyright (C) 1993, 1994, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ext2_fs.h"
#include "ext2fs.h"

struct link_struct  {
	const char	*name;
	int		namelen;
	ext2_ino_t	inode;
	int		flags;
	struct ext2_dir_entry *prev;
	int		done;
};

#ifdef __TURBOC__
# pragma argsused
#endif
static int unlink_proc(struct ext2_dir_entry *dirent,
		     int	offset EXT2FS_ATTR((unused)),
		     int	blocksize EXT2FS_ATTR((unused)),
		     char	*buf EXT2FS_ATTR((unused)),
		     void	*priv_data)
{
	struct link_struct *ls = (struct link_struct *) priv_data;
	struct ext2_dir_entry *prev;

	prev = ls->prev;
	ls->prev = dirent;

	if (ls->name) {
		if ((dirent->name_len & 0xFF) != ls->namelen)
			return 0;
		if (strncmp(ls->name, dirent->name, dirent->name_len & 0xFF))
			return 0;
	}
	if (ls->inode) {
		if (dirent->inode != ls->inode)
			return 0;
	} else {
		if (!dirent->inode)
			return 0;
	}

	if (prev)
		prev->rec_len += dirent->rec_len;
	else
		dirent->inode = 0;
	ls->done++;
	return DIRENT_ABORT|DIRENT_CHANGED;
}

#ifdef __TURBOC__
 #pragma argsused
#endif
errcode_t ext2fs_unlink(ext2_filsys fs, ext2_ino_t dir,
			const char *name, ext2_ino_t ino,
			int flags EXT2FS_ATTR((unused)))
{
	errcode_t	retval;
	struct link_struct ls;

	EXT2_CHECK_MAGIC(fs, EXT2_ET_MAGIC_EXT2FS_FILSYS);

	if (!name && !ino)
		return EXT2_ET_INVALID_ARGUMENT;

	if (!(fs->flags & EXT2_FLAG_RW))
		return EXT2_ET_RO_FILSYS;

	ls.name = name;
	ls.namelen = name ? strlen(name) : 0;
	ls.inode = ino;
	ls.flags = 0;
	ls.done = 0;
	ls.prev = 0;

	retval = ext2fs_dir_iterate(fs, dir, DIRENT_FLAG_INCLUDE_EMPTY,
				    0, unlink_proc, &ls);
	if (retval)
		return retval;

	return (ls.done) ? 0 : EXT2_ET_DIR_NO_SPACE;
}
