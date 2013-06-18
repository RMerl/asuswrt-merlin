/*
 * Header file for disk format of new quotafile format
 *
 * Jan Kara <jack@suse.cz> - sponsored by SuSE CR
 */

#ifndef __QUOTA_DQBLK_V2_H__
#define __QUOTA_DQBLK_V2_H__

#include "quotaio_tree.h"

/* Structure for format specific information */
struct v2_mem_dqinfo {
	struct qtree_mem_dqinfo dqi_qtree;
	unsigned int dqi_flags;		/* Flags set in quotafile */
	unsigned int dqi_used_entries;	/* Number of entries in file -
					   updated by scan_dquots */
	unsigned int dqi_data_blocks;	/* Number of data blocks in file -
					   updated by scan_dquots */
};

struct v2_mem_dqblk {
	long long dqb_off;	/* Offset of dquot in file */
};

struct quotafile_ops;		/* Will be defined later in quotaio.h */

/* Operations above this format */
extern struct quotafile_ops quotafile_ops_2;

#endif  /* __QUOTA_DQBLK_V2_H__ */
