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
	u_int32_t dqh_magic;	/* Magic number identifying file */
	u_int32_t dqh_version;	/* File version */
} __attribute__ ((packed));

/* Flags for version specific files */
#define V2_DQF_MASK  0x0000	/* Mask for all valid ondisk flags */

/* Header with type and version specific information */
struct v2_disk_dqinfo {
	u_int32_t dqi_bgrace;	/* Time before block soft limit becomes
				 * hard limit */
	u_int32_t dqi_igrace;	/* Time before inode soft limit becomes
				 * hard limit */
	u_int32_t dqi_flags;	/* Flags for quotafile (DQF_*) */
	u_int32_t dqi_blocks;	/* Number of blocks in file */
	u_int32_t dqi_free_blk;	/* Number of first free block in the list */
	u_int32_t dqi_free_entry;	/* Number of block with at least one
					 * free entry */
} __attribute__ ((packed));

struct v2r1_disk_dqblk {
	u_int32_t dqb_id;	/* id this quota applies to */
	u_int32_t dqb_pad;
	u_int64_t dqb_ihardlimit;	/* absolute limit on allocated inodes */
	u_int64_t dqb_isoftlimit;	/* preferred inode limit */
	u_int64_t dqb_curinodes;	/* current # allocated inodes */
	u_int64_t dqb_bhardlimit;	/* absolute limit on disk space
					 * (in QUOTABLOCK_SIZE) */
	u_int64_t dqb_bsoftlimit;	/* preferred limit on disk space
					 * (in QUOTABLOCK_SIZE) */
	u_int64_t dqb_curspace;	/* current space occupied (in bytes) */
	u_int64_t dqb_btime;	/* time limit for excessive disk use */
	u_int64_t dqb_itime;	/* time limit for excessive inode use */
} __attribute__ ((packed));

#endif
