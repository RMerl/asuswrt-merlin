/*
 * Compatibility header file for e2fsck which should be included
 * instead of linux/jfs.h
 *
 * Copyright (C) 2000 Stephen C. Tweedie
 *
 * This file may be redistributed under the terms of the
 * GNU General Public License version 2 or at your discretion
 * any later version.
 */

/*
 * Pull in the definition of the e2fsck context structure
 */
#include "e2fsck.h"

struct buffer_head {
	e2fsck_t	b_ctx;
	io_channel 	b_io;
	int	 	b_size;
	blk_t	 	b_blocknr;
	int	 	b_dirty;
	int	 	b_uptodate;
	int	 	b_err;
	char		b_data[1024];
};

struct inode {
	e2fsck_t	i_ctx;
	ext2_ino_t	i_ino;
	struct ext2_inode i_ext2;
};

struct kdev_s {
	e2fsck_t	k_ctx;
	int		k_dev;
};

#define K_DEV_FS	1
#define K_DEV_JOURNAL	2

typedef struct kdev_s *kdev_t;

#define lock_buffer(bh) do {} while(0)
#define unlock_buffer(bh) do {} while(0)
#define buffer_req(bh) 1
#define do_readahead(journal, start) do {} while(0)

extern e2fsck_t e2fsck_global_ctx;  /* Try your very best not to use this! */

typedef struct {
	int	object_length;
} lkmem_cache_t;

#define kmem_cache_alloc(cache,flags) malloc((cache)->object_length)
#define kmem_cache_free(cache,obj) free(obj)
#define kmem_cache_create(name,len,a,b,c,d) do_cache_create(len)
#define kmem_cache_destroy(cache) do_cache_destroy(cache)
#define kmalloc(len,flags) malloc(len)
#define kfree(p) free(p)

#define cond_resched()	do { } while (0)

typedef unsigned int __be32;

#define __init

/*
 * Now pull in the real linux/jfs.h definitions.
 */
#include <ext2fs/kernel-jbd.h>

/*
 * We use the standard libext2fs portability tricks for inline
 * functions.
 */
extern lkmem_cache_t * do_cache_create(int len);
extern void do_cache_destroy(lkmem_cache_t *cache);
extern size_t journal_tag_bytes(journal_t *journal);

#if (defined(E2FSCK_INCLUDE_INLINE_FUNCS) || !defined(NO_INLINE_FUNCS))
#ifdef E2FSCK_INCLUDE_INLINE_FUNCS
#define _INLINE_ extern
#else
#ifdef __GNUC__
#define _INLINE_ extern __inline__
#else				/* For Watcom C */
#define _INLINE_ extern inline
#endif
#endif

_INLINE_ lkmem_cache_t * do_cache_create(int len)
{
	lkmem_cache_t *new_cache;
	new_cache = malloc(sizeof(*new_cache));
	if (new_cache)
		new_cache->object_length = len;
	return new_cache;
}

_INLINE_ void do_cache_destroy(lkmem_cache_t *cache)
{
	free(cache);
}

/*
 * helper functions to deal with 32 or 64bit block numbers.
 */
_INLINE_ size_t journal_tag_bytes(journal_t *journal)
{
	if (JFS_HAS_INCOMPAT_FEATURE(journal, JFS_FEATURE_INCOMPAT_64BIT))
		return JBD_TAG_SIZE64;
	else
		return JBD_TAG_SIZE32;
}

#undef _INLINE_
#endif

/*
 * Kernel compatibility functions are defined in journal.c
 */
int journal_bmap(journal_t *journal, blk_t block, unsigned long *phys);
struct buffer_head *getblk(kdev_t ctx, blk_t blocknr, int blocksize);
void sync_blockdev(kdev_t kdev);
void ll_rw_block(int rw, int dummy, struct buffer_head *bh[]);
void mark_buffer_dirty(struct buffer_head *bh);
void mark_buffer_uptodate(struct buffer_head *bh, int val);
void brelse(struct buffer_head *bh);
int buffer_uptodate(struct buffer_head *bh);
void wait_on_buffer(struct buffer_head *bh);

/*
 * Define newer 2.5 interfaces
 */
#define __getblk(dev, blocknr, blocksize) getblk(dev, blocknr, blocksize)
#define set_buffer_uptodate(bh) mark_buffer_uptodate(bh, 1)
