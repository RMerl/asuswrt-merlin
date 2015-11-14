/*
 *
 *	Header file for disk format of new quotafile format
 *
 */

#ifndef GUARD_QUOTAIO_V2_H
#define GUARD_QUOTAIO_V2_H

#include <sys/types.h>
#include "quotaio.h"

/* Offset of info header in file */
#define V2_DQINFOOFF		sizeof(struct v2_disk_dqheader)
/* Supported version of quota-tree format */
#define V2_VERSION 1

struct v2_disk_dqheader {
	__u32 dqh_magic;	/* Magic number identifying file */
	__u32 dqh_version;	/* File version */
} __attribute__ ((packed));

/* Flags for version specific files */
#define V2_DQF_MASK  0x0000	/* Mask for all valid ondisk flags */

/* Header with type and version specific information */
struct v2_disk_dqinfo {
	__u32 dqi_bgrace;	/* Time before block soft limit becomes
				 * hard limit */
	__u32 dqi_igrace;	/* Time before inode soft limit becomes
				 * hard limit */
	__u32 dqi_flags;	/* Flags for quotafile (DQF_*) */
	__u32 dqi_blocks;	/* Number of blocks in file */
	__u32 dqi_free_blk;	/* Number of first free block in the list */
	__u32 dqi_free_entry;	/* Number of block with at least one
					 * free entry */
} __attribute__ ((packed));

struct v2r1_disk_dqblk {
	__u32 dqb_id;	/* id this quota applies to */
	__u32 dqb_pad;
	__u64 dqb_ihardlimit;	/* absolute limit on allocated inodes */
	__u64 dqb_isoftlimit;	/* preferred inode limit */
	__u64 dqb_curinodes;	/* current # allocated inodes */
	__u64 dqb_bhardlimit;	/* absolute limit on disk space
					 * (in QUOTABLOCK_SIZE) */
	__u64 dqb_bsoftlimit;	/* preferred limit on disk space
					 * (in QUOTABLOCK_SIZE) */
	__u64 dqb_curspace;	/* current space occupied (in bytes) */
	__u64 dqb_btime;	/* time limit for excessive disk use */
	__u64 dqb_itime;	/* time limit for excessive inode use */
} __attribute__ ((packed));

#endif
