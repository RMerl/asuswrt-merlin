/** mkquota.h
 *
 * Interface to the quota library.
 *
 * The quota library provides interface for creating and updating the quota
 * files and the ext4 superblock fields. It supports the new VFS_V1 quota
 * format. The quota library also provides support for keeping track of quotas
 * in memory.
 * The typical way to use the quota library is as follows:
 * {
 *	quota_ctx_t qctx;
 *
 *	quota_init_context(&qctx, fs, -1);
 *	{
 *		quota_compute_usage(qctx, -1);
 *		AND/OR
 *		quota_data_add/quota_data_sub/quota_data_inodes();
 *	}
 *	quota_write_inode(qctx, USRQUOTA);
 *	quota_write_inode(qctx, GRPQUOTA);
 *	quota_release_context(&qctx);
 * }
 *
 * This initial version does not support reading the quota files. This support
 * will be added in near future.
 *
 * Aditya Kali <adityakali@google.com>
 */

#ifndef __QUOTA_QUOTAIO_H__
#define __QUOTA_QUOTAIO_H__

#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "quotaio.h"
#include "../e2fsck/dict.h"

typedef struct quota_ctx *quota_ctx_t;

struct quota_ctx {
	ext2_filsys	fs;
	dict_t		*quota_dict[MAXQUOTAS];
};

/* In mkquota.c */
errcode_t quota_init_context(quota_ctx_t *qctx, ext2_filsys fs, int qtype);
void quota_data_inodes(quota_ctx_t qctx, struct ext2_inode *inode, ext2_ino_t ino,
		int adjust);
void quota_data_add(quota_ctx_t qctx, struct ext2_inode *inode, ext2_ino_t ino,
		qsize_t space);
void quota_data_sub(quota_ctx_t qctx, struct ext2_inode *inode, ext2_ino_t ino,
		qsize_t space);
errcode_t quota_write_inode(quota_ctx_t qctx, int qtype);
errcode_t quota_update_limits(quota_ctx_t qctx, ext2_ino_t qf_ino, int type);
errcode_t quota_compute_usage(quota_ctx_t qctx);
void quota_release_context(quota_ctx_t *qctx);

errcode_t quota_remove_inode(ext2_filsys fs, int qtype);
int quota_file_exists(ext2_filsys fs, int qtype, int fmt);
void quota_set_sb_inum(ext2_filsys fs, ext2_ino_t ino, int qtype);
errcode_t quota_compare_and_update(quota_ctx_t qctx, int qtype,
				   int *usage_inconsistent);

#endif  /* __QUOTA_QUOTAIO_H__ */
