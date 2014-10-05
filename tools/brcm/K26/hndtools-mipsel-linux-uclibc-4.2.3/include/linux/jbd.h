/*
 * linux/include/linux/jbd.h
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>
 *
 * Copyright 1998-2000 Red Hat, Inc --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Definitions for transaction data structures for the buffer cache
 * filesystem journaling support.
 */

#ifndef _LINUX_JBD_H
#define _LINUX_JBD_H

/* Allow this file to be included directly into e2fsprogs */
#include "jfs_compat.h"
#define JFS_DEBUG
#define jfs_debug jbd_debug

#define journal_oom_retry 1

/*
 * Define JBD_PARANIOD_IOFAIL to cause a kernel BUG() if ext3 finds
 * certain classes of error which can occur due to failed IOs.  Under
 * normal use we want ext3 to continue after such errors, because
 * hardware _can_ fail, but for debugging purposes when running tests on
 * known-good hardware we may want to trap these errors.
 */
#undef JBD_PARANOID_IOFAIL

/*
 * The default maximum commit age, in seconds.
 */
#define JBD_DEFAULT_MAX_COMMIT_AGE 5

#ifdef CONFIG_JBD_DEBUG
/*
 * Define JBD_EXPENSIVE_CHECKING to enable more expensive internal
 * consistency checks.  By default we don't do this unless
 * CONFIG_JBD_DEBUG is on.
 */
#define JBD_EXPENSIVE_CHECKING
extern int journal_enable_debug;

#define jbd_debug(n, f, a...)						\
	do {								\
		if ((n) <= journal_enable_debug) {			\
			printk (KERN_DEBUG "(%s, %d): %s: ",		\
				__FILE__, __LINE__, __FUNCTION__);	\
			printk (f, ## a);				\
		}							\
	} while (0)
#else
#define jbd_debug(f, a...)	/**/
#endif

extern void * __jbd_kmalloc (const char *where, size_t size, gfp_t flags, int retry);
extern void * jbd_slab_alloc(size_t size, gfp_t flags);
extern void jbd_slab_free(void *ptr, size_t size);

#define jbd_kmalloc(size, flags) \
	__jbd_kmalloc(__FUNCTION__, (size), (flags), journal_oom_retry)
#define jbd_rep_kmalloc(size, flags) \
	__jbd_kmalloc(__FUNCTION__, (size), (flags), 1)

#define JFS_MIN_JOURNAL_BLOCKS 1024


/*
 * Internal structures used by the logging mechanism:
 */

#define JFS_MAGIC_NUMBER 0xc03b3998U /* The first 4 bytes of /dev/random! */

/*
 * On-disk structures
 */

/*
 * Descriptor block types:
 */

#define JFS_DESCRIPTOR_BLOCK	1
#define JFS_COMMIT_BLOCK	2
#define JFS_SUPERBLOCK_V1	3
#define JFS_SUPERBLOCK_V2	4
#define JFS_REVOKE_BLOCK	5

/*
 * Standard header for all descriptor blocks:
 */
typedef struct journal_header_s
{
	__be32		h_magic;
	__be32		h_blocktype;
	__be32		h_sequence;
} journal_header_t;


/*
 * The block tag: used to describe a single buffer in the journal
 */
typedef struct journal_block_tag_s
{
	__be32		t_blocknr;	/* The on-disk block number */
	__be32		t_flags;	/* See below */
} journal_block_tag_t;

/*
 * The revoke descriptor: used on disk to describe a series of blocks to
 * be revoked from the log
 */
typedef struct journal_revoke_header_s
{
	journal_header_t r_header;
	__be32		 r_count;	/* Count of bytes used in the block */
} journal_revoke_header_t;


/* Definitions for the journal tag flags word: */
#define JFS_FLAG_ESCAPE		1	/* on-disk block is escaped */
#define JFS_FLAG_SAME_UUID	2	/* block has same uuid as previous */
#define JFS_FLAG_DELETED	4	/* block deleted by this transaction */
#define JFS_FLAG_LAST_TAG	8	/* last tag in this descriptor block */


/*
 * The journal superblock.  All fields are in big-endian byte order.
 */
typedef struct journal_superblock_s
{
/* 0x0000 */
	journal_header_t s_header;

/* 0x000C */
	/* Static information describing the journal */
	__be32	s_blocksize;		/* journal device blocksize */
	__be32	s_maxlen;		/* total blocks in journal file */
	__be32	s_first;		/* first block of log information */

/* 0x0018 */
	/* Dynamic information describing the current state of the log */
	__be32	s_sequence;		/* first commit ID expected in log */
	__be32	s_start;		/* blocknr of start of log */

/* 0x0020 */
	/* Error value, as set by journal_abort(). */
	__be32	s_errno;

/* 0x0024 */
	/* Remaining fields are only valid in a version-2 superblock */
	__be32	s_feature_compat;	/* compatible feature set */
	__be32	s_feature_incompat;	/* incompatible feature set */
	__be32	s_feature_ro_compat;	/* readonly-compatible feature set */
/* 0x0030 */
	__u8	s_uuid[16];		/* 128-bit uuid for journal */

/* 0x0040 */
	__be32	s_nr_users;		/* Nr of filesystems sharing log */

	__be32	s_dynsuper;		/* Blocknr of dynamic superblock copy*/

/* 0x0048 */
	__be32	s_max_transaction;	/* Limit of journal blocks per trans.*/
	__be32	s_max_trans_data;	/* Limit of data blocks per trans. */

/* 0x0050 */
	__u32	s_padding[44];

/* 0x0100 */
	__u8	s_users[16*48];		/* ids of all fs'es sharing the log */
/* 0x0400 */
} journal_superblock_t;

#define JFS_HAS_COMPAT_FEATURE(j,mask)					\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_compat & cpu_to_be32((mask))))
#define JFS_HAS_RO_COMPAT_FEATURE(j,mask)				\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_ro_compat & cpu_to_be32((mask))))
#define JFS_HAS_INCOMPAT_FEATURE(j,mask)				\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_incompat & cpu_to_be32((mask))))

#define JFS_FEATURE_INCOMPAT_REVOKE	0x00000001

/* Features known to this kernel version: */
#define JFS_KNOWN_COMPAT_FEATURES	0
#define JFS_KNOWN_ROCOMPAT_FEATURES	0
#define JFS_KNOWN_INCOMPAT_FEATURES	JFS_FEATURE_INCOMPAT_REVOKE


/* Comparison functions for transaction IDs: perform comparisons using
 * modulo arithmetic so that they work over sequence number wraps. */

static __inline__ int tid_gt(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference > 0);
}

static __inline__ int tid_geq(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference >= 0);
}

extern int journal_blocks_per_page(struct inode *inode);

/*
 * Return the minimum number of blocks which must be free in the journal
 * before a new transaction may be started.  Must be called under j_state_lock.
 */
static __inline__ int jbd_space_needed(journal_t *journal)
{
	int nblocks = journal->j_max_transaction_buffers;
	if (journal->j_committing_transaction)
		nblocks += journal->j_committing_transaction->
					t_outstanding_credits;
	return nblocks;
}

/*
 * Definitions which augment the buffer_head layer
 */

/* journaling buffer types */
#define BJ_None		0	/* Not journaled */
#define BJ_SyncData	1	/* Normal data: flush before commit */
#define BJ_Metadata	2	/* Normal journaled metadata */
#define BJ_Forget	3	/* Buffer superseded by this transaction */
#define BJ_IO		4	/* Buffer is for temporary IO use */
#define BJ_Shadow	5	/* Buffer contents being shadowed to the log */
#define BJ_LogCtl	6	/* Buffer contains log descriptors */
#define BJ_Reserved	7	/* Buffer is reserved for access by journal */
#define BJ_Locked	8	/* Locked for I/O during commit */
#define BJ_Types	9

extern int jbd_blocks_per_page(struct inode *inode);


#endif	/* _LINUX_JBD_H */
