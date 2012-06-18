/* vi: set sw=4 ts=4: */
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
#ifndef LINUX_JBD_H
#define LINUX_JBD_H 1

#include <sys/types.h>
#include <linux/types.h>
#include "ext2fs.h"

/*
 * Standard header for all descriptor blocks:
 */

typedef struct journal_header_s
{
	__u32		h_magic;
	__u32		h_blocktype;
	__u32		h_sequence;
} journal_header_t;

/*
 * This is the global e2fsck structure.
 */
typedef struct e2fsck_struct *e2fsck_t;


struct inode {
	e2fsck_t        i_ctx;
	ext2_ino_t      i_ino;
	struct ext2_inode i_ext2;
};


/*
 * The journal superblock.  All fields are in big-endian byte order.
 */
typedef struct journal_superblock_s
{
/* 0x0000 */
	journal_header_t s_header;

/* 0x000C */
	/* Static information describing the journal */
	__u32	s_blocksize;		/* journal device blocksize */
	__u32	s_maxlen;		/* total blocks in journal file */
	__u32	s_first;		/* first block of log information */

/* 0x0018 */
	/* Dynamic information describing the current state of the log */
	__u32	s_sequence;		/* first commit ID expected in log */
	__u32	s_start;		/* blocknr of start of log */

/* 0x0020 */
	/* Error value, as set by journal_abort(). */
	__s32	s_errno;

/* 0x0024 */
	/* Remaining fields are only valid in a version-2 superblock */
	__u32	s_feature_compat;	/* compatible feature set */
	__u32	s_feature_incompat;	/* incompatible feature set */
	__u32	s_feature_ro_compat;	/* readonly-compatible feature set */
/* 0x0030 */
	__u8	s_uuid[16];		/* 128-bit uuid for journal */

/* 0x0040 */
	__u32	s_nr_users;		/* Nr of filesystems sharing log */

	__u32	s_dynsuper;		/* Blocknr of dynamic superblock copy*/

/* 0x0048 */
	__u32	s_max_transaction;	/* Limit of journal blocks per trans.*/
	__u32	s_max_trans_data;	/* Limit of data blocks per trans. */

/* 0x0050 */
	__u32	s_padding[44];

/* 0x0100 */
	__u8	s_users[16*48];		/* ids of all fs'es sharing the log */
/* 0x0400 */
} journal_superblock_t;


extern int journal_blocks_per_page(struct inode *inode);
extern int jbd_blocks_per_page(struct inode *inode);

#define JFS_MIN_JOURNAL_BLOCKS 1024


/*
 * Internal structures used by the logging mechanism:
 */

#define JFS_MAGIC_NUMBER 0xc03b3998U /* The first 4 bytes of /dev/random! */

/*
 * Descriptor block types:
 */

#define JFS_DESCRIPTOR_BLOCK	1
#define JFS_COMMIT_BLOCK	2
#define JFS_SUPERBLOCK_V1	3
#define JFS_SUPERBLOCK_V2	4
#define JFS_REVOKE_BLOCK	5

/*
 * The block tag: used to describe a single buffer in the journal
 */
typedef struct journal_block_tag_s
{
	__u32		t_blocknr;	/* The on-disk block number */
	__u32		t_flags;	/* See below */
} journal_block_tag_t;

/*
 * The revoke descriptor: used on disk to describe a series of blocks to
 * be revoked from the log
 */
typedef struct journal_revoke_header_s
{
	journal_header_t r_header;
	int		 r_count;	/* Count of bytes used in the block */
} journal_revoke_header_t;


/* Definitions for the journal tag flags word: */
#define JFS_FLAG_ESCAPE		1	/* on-disk block is escaped */
#define JFS_FLAG_SAME_UUID	2	/* block has same uuid as previous */
#define JFS_FLAG_DELETED	4	/* block deleted by this transaction */
#define JFS_FLAG_LAST_TAG	8	/* last tag in this descriptor block */




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


/*
 * Definitions which augment the buffer_head layer
 */

/* journaling buffer types */
#define BJ_None		0	/* Not journaled */
#define BJ_SyncData	1	/* Normal data: flush before commit */
#define BJ_AsyncData	2	/* writepage data: wait on it before commit */
#define BJ_Metadata	3	/* Normal journaled metadata */
#define BJ_Forget	4	/* Buffer superceded by this transaction */
#define BJ_IO		5	/* Buffer is for temporary IO use */
#define BJ_Shadow	6	/* Buffer contents being shadowed to the log */
#define BJ_LogCtl	7	/* Buffer contains log descriptors */
#define BJ_Reserved	8	/* Buffer is reserved for access by journal */
#define BJ_Types	9


struct kdev_s {
	e2fsck_t        k_ctx;
	int             k_dev;
};

typedef struct kdev_s *kdev_t;
typedef unsigned int tid_t;

struct journal_s
{
	unsigned long		j_flags;
	int			j_errno;
	struct buffer_head *	j_sb_buffer;
	struct journal_superblock_s *j_superblock;
	int			j_format_version;
	unsigned long		j_head;
	unsigned long		j_tail;
	unsigned long		j_free;
	unsigned long		j_first, j_last;
	kdev_t			j_dev;
	kdev_t			j_fs_dev;
	int			j_blocksize;
	unsigned int		j_blk_offset;
	unsigned int		j_maxlen;
	struct inode *		j_inode;
	tid_t			j_tail_sequence;
	tid_t			j_transaction_sequence;
	__u8			j_uuid[16];
	struct jbd_revoke_table_s *j_revoke;
};

typedef struct journal_s journal_t;

extern int	   journal_recover    (journal_t *journal);
extern int	   journal_skip_recovery (journal_t *);

/* Primary revoke support */
extern int	   journal_init_revoke(journal_t *, int);
extern void	   journal_destroy_revoke_caches(void);
extern int	   journal_init_revoke_caches(void);

/* Recovery revoke support */
extern int	   journal_set_revoke(journal_t *, unsigned long, tid_t);
extern int	   journal_test_revoke(journal_t *, unsigned long, tid_t);
extern void	   journal_clear_revoke(journal_t *);
extern void	   journal_brelse_array(struct buffer_head *b[], int n);

extern void	   journal_destroy_revoke(journal_t *);


#endif
