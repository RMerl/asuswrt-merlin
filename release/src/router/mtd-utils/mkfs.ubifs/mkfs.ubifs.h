/*
 * Copyright (C) 2008 Nokia Corporation.
 * Copyright (C) 2008 University of Szeged, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Artem Bityutskiy
 *          Adrian Hunter
 *          Zoltan Sogor
 */

#ifndef __MKFS_UBIFS_H__
#define __MKFS_UBIFS_H__

#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>
#include <linux/types.h>
#include <linux/fs.h>

#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <ctype.h>
#include <uuid/uuid.h>
#include <sys/file.h>

#include "libubi.h"
#include "defs.h"
#include "crc16.h"
#include "ubifs-media.h"
#include "ubifs.h"
#include "key.h"
#include "lpt.h"
#include "compr.h"

/*
 * Compression flags are duplicated so that compr.c can compile without ubifs.h.
 * Here we make sure they are the same.
 */
#if MKFS_UBIFS_COMPR_NONE != UBIFS_COMPR_NONE
#error MKFS_UBIFS_COMPR_NONE != UBIFS_COMPR_NONE
#endif
#if MKFS_UBIFS_COMPR_LZO != UBIFS_COMPR_LZO
#error MKFS_UBIFS_COMPR_LZO != UBIFS_COMPR_LZO
#endif
#if MKFS_UBIFS_COMPR_ZLIB != UBIFS_COMPR_ZLIB
#error MKFS_UBIFS_COMPR_ZLIB != UBIFS_COMPR_ZLIB
#endif

extern int verbose;
extern int debug_level;

#define dbg_msg(lvl, fmt, ...) do {if (debug_level >= lvl)                \
	printf("mkfs.ubifs: %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__); \
} while(0)

#define err_msg(fmt, ...) ({                                \
	fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); \
	-1;                                                 \
})

#define sys_err_msg(fmt, ...) ({                                         \
	int err_ = errno;                                                \
	fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__);              \
	fprintf(stderr, "       %s (error %d)\n", strerror(err_), err_); \
	-1;                                                              \
})

/**
 * struct path_htbl_element - an element of the path hash table.
 * @path: the UBIFS path the element describes (the key of the element)
 * @name_htbl: one more (nested) hash table containing names of all
 *             files/directories/device nodes which should be created at this
 *             path
 *
 * See device table handling for more information.
 */
struct path_htbl_element {
	const char *path;
	struct hashtable *name_htbl;
};

/**
 * struct name_htbl_element - an element in the name hash table
 * @name: name of the file/directory/device node (the key of the element)
 * @mode: accsess rights and file type
 * @uid: user ID
 * @gid: group ID
 * @major: device node major number
 * @minor: device node minor number
 *
 * This is an element of the name hash table. Name hash table sits in the path
 * hash table elements and describes file names which should be created/changed
 * at this path.
 */
struct name_htbl_element {
	const char *name;
	unsigned int mode;
	unsigned int uid;
	unsigned int gid;
	dev_t dev;
};

extern struct ubifs_info info_;

struct hashtable_itr;

int write_leb(int lnum, int len, void *buf, int dtype);
int parse_devtable(const char *tbl_file);
struct path_htbl_element *devtbl_find_path(const char *path);
struct name_htbl_element *devtbl_find_name(struct path_htbl_element *ph_elt,
					   const char *name);
int override_attributes(struct stat *st, struct path_htbl_element *ph_elt,
			struct name_htbl_element *nh_elt);
struct name_htbl_element *
first_name_htbl_element(struct path_htbl_element *ph_elt,
			struct hashtable_itr **itr);
struct name_htbl_element *
next_name_htbl_element(struct path_htbl_element *ph_elt,
		       struct hashtable_itr **itr);
void free_devtable_info(void);

#endif
