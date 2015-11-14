/*
 * resize2fs.h --- ext2 resizer header file
 *
 * Copyright (C) 1997, 1998 by Theodore Ts'o and
 * 	PowerQuest, Inc.
 *
 * Copyright (C) 1999, 2000 by Theosore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#if EXT2_FLAT_INCLUDES
#include "ext2_fs.h"
#include "ext2fs.h"
#include "e2p.h"
#else
#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "e2p/e2p.h"
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#define _(a) (gettext (a))
#ifdef gettext_noop
#define N_(a) gettext_noop (a)
#else
#define N_(a) (a)
#endif
#ifndef NLS_CAT_NAME
#define NLS_CAT_NAME "e2fsprogs"
#endif
#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif
#else
#define _(a) (a)
#define N_(a) a
#endif


/*
 * For the extent map
 */
typedef struct _ext2_extent *ext2_extent;

/*
 * For the simple progress meter
 */
typedef struct ext2_sim_progress *ext2_sim_progmeter;

/*
 * Flags for the resizer; most are debugging flags only
 */
#define RESIZE_DEBUG_IO			0x0001
#define RESIZE_DEBUG_BMOVE		0x0002
#define RESIZE_DEBUG_INODEMAP		0x0004
#define RESIZE_DEBUG_ITABLEMOVE		0x0008
#define RESIZE_DEBUG_RTRACK		0x0010
#define RESIZE_DEBUG_MIN_CALC		0x0020

#define RESIZE_PERCENT_COMPLETE		0x0100
#define RESIZE_VERBOSE			0x0200

/*
 * This structure is used for keeping track of how much resources have
 * been used for a particular resize2fs pass.
 */
struct resource_track {
	const char *desc;
	struct timeval time_start;
	struct timeval user_start;
	struct timeval system_start;
	void	*brk_start;
	unsigned long long bytes_read;
	unsigned long long bytes_written;
};

/*
 * The core state structure for the ext2 resizer
 */
typedef struct ext2_resize_struct *ext2_resize_t;

struct ext2_resize_struct {
	ext2_filsys	old_fs;
	ext2_filsys	new_fs;
	ext2fs_block_bitmap reserve_blocks;
	ext2fs_block_bitmap move_blocks;
	ext2_extent	bmap;
	ext2_extent	imap;
	blk64_t		needed_blocks;
	int		flags;
	char		*itable_buf;

	/*
	 * For the block allocator
	 */
	blk64_t		new_blk;
	int		alloc_state;

	/*
	 * For the progress meter
	 */
	errcode_t	(*progress)(ext2_resize_t rfs, int pass,
				    unsigned long cur,
				    unsigned long max);
	void		*prog_data;
};

/*
 * Progress pass numbers...
 */
#define E2_RSZ_EXTEND_ITABLE_PASS	1
#define E2_RSZ_BLOCK_RELOC_PASS		2
#define E2_RSZ_INODE_SCAN_PASS		3
#define E2_RSZ_INODE_REF_UPD_PASS	4
#define E2_RSZ_MOVE_ITABLE_PASS		5


/* prototypes */
extern errcode_t resize_fs(ext2_filsys fs, blk64_t *new_size, int flags,
			   errcode_t	(*progress)(ext2_resize_t rfs,
					    int pass, unsigned long cur,
					    unsigned long max));

extern errcode_t adjust_fs_info(ext2_filsys fs, ext2_filsys old_fs,
				ext2fs_block_bitmap reserve_blocks,
				blk64_t new_size);
extern blk64_t calculate_minimum_resize_size(ext2_filsys fs, int flags);


/* extent.c */
extern errcode_t ext2fs_create_extent_table(ext2_extent *ret_extent,
					    __u64 size);
extern void ext2fs_free_extent_table(ext2_extent extent);
extern errcode_t ext2fs_add_extent_entry(ext2_extent extent,
					 __u64 old_loc, __u64 new_loc);
extern __u64 ext2fs_extent_translate(ext2_extent extent, __u64 old_loc);
extern void ext2fs_extent_dump(ext2_extent extent, FILE *out);
extern errcode_t ext2fs_iterate_extent(ext2_extent extent, __u64 *old_loc,
				       __u64 *new_loc, __u64 *size);

/* main.c */
extern char *program_name;

/* online.c */
extern errcode_t online_resize_fs(ext2_filsys fs, const char *mtpt,
				  blk64_t *new_size, int flags);

/* resource_track.c */
extern void init_resource_track(struct resource_track *track, const char *desc,
				io_channel channel);
extern void print_resource_track(ext2_resize_t rfs,
				 struct resource_track *track,
				 io_channel channel);

/* sim_progress.c */
extern errcode_t ext2fs_progress_init(ext2_sim_progmeter *ret_prog,
				      const char *label,
				      int labelwidth, int barwidth,
				      __u32 maxdone, int flags);
extern void ext2fs_progress_update(ext2_sim_progmeter prog,
					__u32 current);
extern void ext2fs_progress_close(ext2_sim_progmeter prog);


