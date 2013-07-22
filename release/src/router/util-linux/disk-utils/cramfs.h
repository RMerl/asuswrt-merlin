/*
 * cramfs_common - cramfs common code
 *
 * Copyright (c) 2008 Roy Peled, the.roy.peled  -at-  gmail
 * Copyright (c) 2004-2006 by Michael Holzt, kju -at- fqdn.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __CRAMFS_H
#define __CRAMFS_H

#include <stdint.h>

#define CRAMFS_MAGIC		0x28cd3d45	/* some random number */
#define CRAMFS_SIGNATURE	"Compressed ROMFS"

/*
 * Width of various bitfields in struct cramfs_inode.
 * Primarily used to generate warnings in mkcramfs.
 */
#define CRAMFS_MODE_WIDTH 16
#define CRAMFS_UID_WIDTH 16
#define CRAMFS_SIZE_WIDTH 24
#define CRAMFS_GID_WIDTH 8
#define CRAMFS_NAMELEN_WIDTH 6
#define CRAMFS_OFFSET_WIDTH 26

#ifndef HOST_IS_BIG_ENDIAN
#ifdef WORDS_BIGENDIAN
#define HOST_IS_BIG_ENDIAN 1
#else
#define HOST_IS_BIG_ENDIAN 0
#endif
#endif

/*
 * Reasonably terse representation of the inode data.
 */
struct cramfs_inode {
	uint32_t mode:16, uid:16;
	/* SIZE for device files is i_rdev */
	uint32_t size:24, gid:8;
	/*
	 * NAMELEN is the length of the file name, divided by 4 and
	 * rounded up.  (cramfs doesn't support hard links.)
	 *
	 * OFFSET: For symlinks and non-empty regular files, this
	 * contains the offset (divided by 4) of the file data in
	 * compressed form (starting with an array of block pointers;
	 * see README).  For non-empty directories it is the offset
	 * (divided by 4) of the inode of the first file in that
	 * directory.  For anything else, offset is zero.
	 */
	uint32_t namelen:6, offset:26;
};

struct cramfs_info {
	uint32_t crc;
	uint32_t edition;
	uint32_t blocks;
	uint32_t files;
};

/*
 * Superblock information at the beginning of the FS.
 */
struct cramfs_super {
	uint32_t magic;		/* 0x28cd3d45 - random number */
	uint32_t size;		/* Not used.  mkcramfs currently
				   writes a constant 1<<16 here. */
	uint32_t flags;		/* 0 */
	uint32_t future;		/* 0 */
	uint8_t signature[16];	/* "Compressed ROMFS" */
	struct cramfs_info fsid;/* unique filesystem info */
	uint8_t name[16];		/* user-defined name */
	struct cramfs_inode root;	/* Root inode data */
};

#define CRAMFS_FLAG_FSID_VERSION_2	0x00000001	/* fsid version #2 */
#define CRAMFS_FLAG_SORTED_DIRS		0x00000002	/* sorted dirs */
#define CRAMFS_FLAG_HOLES		0x00000100	/* support for holes */
#define CRAMFS_FLAG_WRONG_SIGNATURE	0x00000200	/* reserved */
#define CRAMFS_FLAG_SHIFTED_ROOT_OFFSET 0x00000400	/* shifted root fs */

/*
 * Valid values in super.flags.  Currently we refuse to mount
 * if (flags & ~CRAMFS_SUPPORTED_FLAGS).  Maybe that should be
 * changed to test super.future instead.
 */
#define CRAMFS_SUPPORTED_FLAGS (0xff)

/* Uncompression interfaces to the underlying zlib */
int cramfs_uncompress_block(void *dst, int dstlen, void *src, int srclen);
int cramfs_uncompress_init(void);
int cramfs_uncompress_exit(void);

uint32_t u32_toggle_endianness(int big_endian, uint32_t what);
void super_toggle_endianness(int from_big_endian, struct cramfs_super *super);
void inode_to_host(int from_big_endian, struct cramfs_inode *inode_in,
		   struct cramfs_inode *inode_out);
void inode_from_host(int to_big_endian, struct cramfs_inode *inode_in,
		     struct cramfs_inode *inode_out);

#endif
