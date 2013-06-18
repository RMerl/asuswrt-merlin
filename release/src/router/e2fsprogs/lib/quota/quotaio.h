/** quotaio.h
 *
 * Header of IO operations for quota utilities
 * Jan Kara <jack@suse.cz>
 */

#ifndef GUARD_QUOTAIO_H
#define GUARD_QUOTAIO_H

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ext2fs/ext2fs.h"
#include "dqblk_v2.h"

typedef int64_t qsize_t;	/* Type in which we store size limitations */

#define MAXQUOTAS 2
#define USRQUOTA 0
#define GRPQUOTA 1

/*
 * Definitions of magics and versions of current quota files
 */
#define INITQMAGICS {\
	0xd9c01f11,	/* USRQUOTA */\
	0xd9c01927	/* GRPQUOTA */\
}

/* Size of blocks in which are counted size limits in generic utility parts */
#define QUOTABLOCK_BITS 10
#define QUOTABLOCK_SIZE (1 << QUOTABLOCK_BITS)
#define toqb(x) (((x) + QUOTABLOCK_SIZE - 1) >> QUOTABLOCK_BITS)

/* Quota format type IDs */
#define	QFMT_VFS_OLD 1
#define	QFMT_VFS_V0 2
#define	QFMT_VFS_V1 4

/*
 * The following constants define the default amount of time given a user
 * before the soft limits are treated as hard limits (usually resulting
 * in an allocation failure). The timer is started when the user crosses
 * their soft limit, it is reset when they go below their soft limit.
 */
#define MAX_IQ_TIME  604800	/* (7*24*60*60) 1 week */
#define MAX_DQ_TIME  604800	/* (7*24*60*60) 1 week */

#define IOFL_INFODIRTY	0x01	/* Did info change? */

struct quotafile_ops;

/* Generic information about quotafile */
struct util_dqinfo {
	time_t dqi_bgrace;	/* Block grace time for given quotafile */
	time_t dqi_igrace;	/* Inode grace time for given quotafile */
	union {
		struct v2_mem_dqinfo v2_mdqi;
	} u;			/* Format specific info about quotafile */
};

struct quota_file {
	ext2_filsys fs;
	ext2_ino_t ino;
	ext2_file_t e2_file;
};

/* Structure for one opened quota file */
struct quota_handle {
	int qh_type;		/* Type of quotafile */
	int qh_fmt;		/* Quotafile format */
	int qh_io_flags;	/* IO flags for file */
	struct quota_file qh_qf;
	unsigned int (*e2fs_read)(struct quota_file *qf, ext2_loff_t offset,
			 void *buf, unsigned int size);
	unsigned int (*e2fs_write)(struct quota_file *qf, ext2_loff_t offset,
			  void *buf, unsigned int size);
	struct quotafile_ops *qh_ops;	/* Operations on quotafile */
	struct util_dqinfo qh_info;	/* Generic quotafile info */
};

/* Utility quota block */
struct util_dqblk {
	qsize_t dqb_ihardlimit;
	qsize_t dqb_isoftlimit;
	qsize_t dqb_curinodes;
	qsize_t dqb_bhardlimit;
	qsize_t dqb_bsoftlimit;
	qsize_t dqb_curspace;
	time_t dqb_btime;
	time_t dqb_itime;
	union {
		struct v2_mem_dqblk v2_mdqb;
	} u;			/* Format specific dquot information */
};

/* Structure for one loaded quota */
struct dquot {
	struct dquot *dq_next;	/* Pointer to next dquot in the list */
	qid_t dq_id;		/* ID dquot belongs to */
	int dq_flags;		/* Some flags for utils */
	struct quota_handle *dq_h;	/* Handle of quotafile for this dquot */
	struct util_dqblk dq_dqb;	/* Parsed data of dquot */
};

/* Structure of quotafile operations */
struct quotafile_ops {
	/* Check whether quotafile is in our format */
	int (*check_file) (struct quota_handle *h, int type, int fmt);
	/* Open quotafile */
	int (*init_io) (struct quota_handle *h);
	/* Create new quotafile */
	int (*new_io) (struct quota_handle *h);
	/* Write all changes and close quotafile */
	int (*end_io) (struct quota_handle *h);
	/* Write info about quotafile */
	int (*write_info) (struct quota_handle *h);
	/* Read dquot into memory */
	struct dquot *(*read_dquot) (struct quota_handle *h, qid_t id);
	/* Write given dquot to disk */
	int (*commit_dquot) (struct dquot *dquot);
	/* Scan quotafile and call callback on every structure */
	int (*scan_dquots) (struct quota_handle *h,
			    int (*process_dquot) (struct dquot *dquot,
						  void *data),
			    void *data);
	/* Function to print format specific file information */
	int (*report) (struct quota_handle *h, int verbose);
};

/* This might go into a special header file but that sounds a bit silly... */
extern struct quotafile_ops quotafile_ops_meta;

static inline void mark_quotafile_info_dirty(struct quota_handle *h)
{
	h->qh_io_flags |= IOFL_INFODIRTY;
}

/* Open existing quotafile of given type (and verify its format) on given
 * filesystem. */
errcode_t quota_file_open(struct quota_handle *h, ext2_filsys fs,
			  ext2_ino_t qf_ino, int type, int fmt, int flags);


/* Create new quotafile of specified format on given filesystem */
errcode_t quota_file_create(struct quota_handle *h, ext2_filsys fs,
			    int type, int fmt);

/* Close quotafile */
errcode_t quota_file_close(struct quota_handle *h);

/* Get empty quota structure */
struct dquot *get_empty_dquot(void);

errcode_t quota_inode_truncate(ext2_filsys fs, ext2_ino_t ino);

const char *type2name(int type);

void update_grace_times(struct dquot *q);

/* size for the buffer returned by quota_get_qf_name(); must be greater
   than maxlen of extensions[] and fmtnames[] (plus 2) found in quotaio.c */
#define QUOTA_NAME_LEN 16

const char *quota_get_qf_name(int type, int fmt, char *buf);
const char *quota_get_qf_path(const char *mntpt, int qtype, int fmt,
			      char *path_buf, size_t path_buf_size);

#endif /* GUARD_QUOTAIO_H */
