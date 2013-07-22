/*
 * Copyright (C) 2009 Red Hat, Inc.
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include "superblocks.h"

/*
 * BFS actually has two different labels in the superblock, each
 * of them only 6 bytes long.  Until we find out what their use
 * we just ignore them.
 */
const struct blkid_idinfo bfs_idinfo =
{
	.name		= "bfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.magics		= {
		{ .magic = "\xce\xfa\xad\x1b", .len = 4 },
		{ NULL }
	}
};
