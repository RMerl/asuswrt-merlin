/** quotaio.c
 *
 * Generic IO operations on quotafiles
 * Jan Kara <jack@suse.cz> - sponsored by SuSE CR
 * Aditya Kali <adityakali@google.com> - Ported to e2fsprogs
 */

#include "config.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#include "common.h"
#include "quotaio.h"

static const char * const extensions[MAXQUOTAS] = {"user", "group"};
static const char * const basenames[] = {
	"",		/* undefined */
	"quota",	/* QFMT_VFS_OLD */
	"aquota",	/* QFMT_VFS_V0 */
	"",		/* QFMT_OCFS2 */
	"aquota"	/* QFMT_VFS_V1 */
};

/* Header in all newer quotafiles */
struct disk_dqheader {
	u_int32_t dqh_magic;
	u_int32_t dqh_version;
} __attribute__ ((packed));

/**
 * Convert type of quota to written representation
 */
const char *type2name(int type)
{
	return extensions[type];
}

/**
 * Creates a quota file name for given type and format.
 */
const char *quota_get_qf_name(int type, int fmt, char *buf)
{
	if (!buf)
		return NULL;
	snprintf(buf, QUOTA_NAME_LEN, "%s.%s",
		 basenames[fmt], extensions[type]);

	return buf;
}

const char *quota_get_qf_path(const char *mntpt, int qtype, int fmt,
			      char *path_buf, size_t path_buf_size)
{
	char qf_name[QUOTA_NAME_LEN];

	if (!mntpt || !path_buf || !path_buf_size)
		return NULL;

	strncpy(path_buf, mntpt, path_buf_size);
	strncat(path_buf, "/", 1);
	strncat(path_buf, quota_get_qf_name(qtype, fmt, qf_name),
		path_buf_size - strlen(path_buf));

	return path_buf;
}

/*
 * Set grace time if needed
 */
void update_grace_times(struct dquot *q)
{
	time_t now;

	time(&now);
	if (q->dq_dqb.dqb_bsoftlimit && toqb(q->dq_dqb.dqb_curspace) >
			q->dq_dqb.dqb_bsoftlimit) {
		if (!q->dq_dqb.dqb_btime)
			q->dq_dqb.dqb_btime =
				now + q->dq_h->qh_info.dqi_bgrace;
	} else {
		q->dq_dqb.dqb_btime = 0;
	}

	if (q->dq_dqb.dqb_isoftlimit && q->dq_dqb.dqb_curinodes >
			q->dq_dqb.dqb_isoftlimit) {
		if (!q->dq_dqb.dqb_itime)
				q->dq_dqb.dqb_itime =
					now + q->dq_h->qh_info.dqi_igrace;
	} else {
		q->dq_dqb.dqb_itime = 0;
	}
}

static int release_blocks_proc(ext2_filsys fs, blk64_t *blocknr,
			       e2_blkcnt_t blockcnt EXT2FS_ATTR((unused)),
			       blk64_t ref_block EXT2FS_ATTR((unused)),
			       int ref_offset EXT2FS_ATTR((unused)),
			       void *private EXT2FS_ATTR((unused)))
{
	blk64_t	block;

	block = *blocknr;
	ext2fs_block_alloc_stats2(fs, block, -1);
	return 0;
}

static int compute_num_blocks_proc(ext2_filsys fs, blk64_t *blocknr,
			       e2_blkcnt_t blockcnt EXT2FS_ATTR((unused)),
			       blk64_t ref_block EXT2FS_ATTR((unused)),
			       int ref_offset EXT2FS_ATTR((unused)),
			       void *private)
{
	blk64_t *num_blocks = private;

	*num_blocks += 1;
	return 0;
}

errcode_t quota_inode_truncate(ext2_filsys fs, ext2_ino_t ino)
{
	struct ext2_inode inode;
	errcode_t err;

	if ((err = ext2fs_read_inode(fs, ino, &inode)))
		return err;

	if ((ino == EXT4_USR_QUOTA_INO) || (ino == EXT4_GRP_QUOTA_INO)) {
		inode.i_dtime = fs->now ? fs->now : time(0);
		if (!ext2fs_inode_has_valid_blocks2(fs, &inode))
			return 0;

		ext2fs_block_iterate3(fs, ino, BLOCK_FLAG_READ_ONLY, NULL,
				      release_blocks_proc, NULL);
		fs->flags &= ~EXT2_FLAG_SUPER_ONLY;
		memset(&inode, 0, sizeof(struct ext2_inode));
	} else {
		inode.i_flags &= ~EXT2_IMMUTABLE_FL;
	}
	err = ext2fs_write_inode(fs, ino, &inode);
	return err;
}

static ext2_off64_t compute_inode_size(ext2_filsys fs, ext2_ino_t ino)
{
	blk64_t num_blocks = 0;

	ext2fs_block_iterate3(fs, ino,
			      BLOCK_FLAG_READ_ONLY,
			      NULL,
			      compute_num_blocks_proc,
			      &num_blocks);
	return num_blocks * fs->blocksize;
}

/* Functions to read/write quota file. */
static unsigned int quota_write_nomount(struct quota_file *qf,
					ext2_loff_t offset,
					void *buf, unsigned int size)
{
	ext2_file_t	e2_file = qf->e2_file;
	unsigned int	bytes_written = 0;
	errcode_t	err;

	err = ext2fs_file_llseek(e2_file, offset, EXT2_SEEK_SET, NULL);
	if (err) {
		log_err("ext2fs_file_llseek failed: %ld", err);
		return 0;
	}

	err = ext2fs_file_write(e2_file, buf, size, &bytes_written);
	if (err) {
		log_err("ext2fs_file_write failed: %ld", err);
		return 0;
	}

	/* Correct inode.i_size is set in end_io. */
	return bytes_written;
}

static unsigned int quota_read_nomount(struct quota_file *qf,
				       ext2_loff_t offset,
				       void *buf, unsigned int size)
{
	ext2_file_t	e2_file = qf->e2_file;
	unsigned int	bytes_read = 0;
	errcode_t	err;

	err = ext2fs_file_llseek(e2_file, offset, EXT2_SEEK_SET, NULL);
	if (err) {
		log_err("ext2fs_file_llseek failed: %ld", err);
		return 0;
	}

	err = ext2fs_file_read(e2_file, buf, size, &bytes_read);
	if (err) {
		log_err("ext2fs_file_read failed: %ld", err);
		return 0;
	}

	return bytes_read;
}

/*
 * Detect quota format and initialize quota IO
 */
errcode_t quota_file_open(struct quota_handle *h, ext2_filsys fs,
			  ext2_ino_t qf_ino, int type, int fmt, int flags)
{
	ext2_file_t e2_file;
	errcode_t err;

	if (fmt == -1)
		fmt = QFMT_VFS_V1;

	err = ext2fs_read_bitmaps(fs);
	if (err)
		return err;

	log_debug("Opening quota ino=%lu, type=%d", qf_ino, type);
	err = ext2fs_file_open(fs, qf_ino, flags, &e2_file);
	if (err) {
		log_err("ext2fs_file_open failed: %s", error_message(err));
		return err;
	}
	h->qh_qf.e2_file = e2_file;

	h->qh_qf.fs = fs;
	h->qh_qf.ino = qf_ino;
	h->e2fs_write = quota_write_nomount;
	h->e2fs_read = quota_read_nomount;
	h->qh_io_flags = 0;
	h->qh_type = type;
	h->qh_fmt = fmt;
	memset(&h->qh_info, 0, sizeof(h->qh_info));
	h->qh_ops = &quotafile_ops_2;

	if (h->qh_ops->check_file &&
	    (h->qh_ops->check_file(h, type, fmt) == 0)) {
		log_err("qh_ops->check_file failed");
		ext2fs_file_close(e2_file);
		return -1;
	}

	if (h->qh_ops->init_io && (h->qh_ops->init_io(h) < 0)) {
		log_err("qh_ops->init_io failed");
		ext2fs_file_close(e2_file);
		return -1;
	}

	return 0;
}

static errcode_t quota_inode_init_new(ext2_filsys fs, ext2_ino_t ino)
{
	struct ext2_inode inode;
	errcode_t err = 0;

	err = ext2fs_read_inode(fs, ino, &inode);
	if (err) {
		log_err("ex2fs_read_inode failed");
		return err;
	}

	if (EXT2_I_SIZE(&inode))
		quota_inode_truncate(fs, ino);

	memset(&inode, 0, sizeof(struct ext2_inode));
	ext2fs_iblk_set(fs, &inode, 0);
	inode.i_atime = inode.i_mtime =
		inode.i_ctime = fs->now ? fs->now : time(0);
	inode.i_links_count = 1;
	inode.i_mode = LINUX_S_IFREG | 0600;
	inode.i_flags |= EXT2_IMMUTABLE_FL;
	if (fs->super->s_feature_incompat &
			EXT3_FEATURE_INCOMPAT_EXTENTS)
		inode.i_flags |= EXT4_EXTENTS_FL;

	err = ext2fs_write_new_inode(fs, ino, &inode);
	if (err) {
		log_err("ext2fs_write_new_inode failed: %ld", err);
		return err;
	}
	return err;
}

/*
 * Create new quotafile of specified format on given filesystem
 */
errcode_t quota_file_create(struct quota_handle *h, ext2_filsys fs, int type, int fmt)
{
	ext2_file_t e2_file;
	int err;
	unsigned long qf_inum;

	if (fmt == -1)
		fmt = QFMT_VFS_V1;

	h->qh_qf.fs = fs;
	if (type == USRQUOTA)
		qf_inum = EXT4_USR_QUOTA_INO;
	else if (type == GRPQUOTA)
		qf_inum = EXT4_GRP_QUOTA_INO;
	else
		return -1;

	err = ext2fs_read_bitmaps(fs);
	if (err)
		goto out_err;

	err = quota_inode_init_new(fs, qf_inum);
	if (err) {
		log_err("init_new_quota_inode failed");
		goto out_err;
	}
	h->qh_qf.ino = qf_inum;
	h->e2fs_write = quota_write_nomount;
	h->e2fs_read = quota_read_nomount;

	log_debug("Creating quota ino=%lu, type=%d", qf_inum, type);
	err = ext2fs_file_open(fs, qf_inum,
			EXT2_FILE_WRITE | EXT2_FILE_CREATE, &e2_file);
	if (err) {
		log_err("ext2fs_file_open failed: %d", err);
		goto out_err;
	}
	h->qh_qf.e2_file = e2_file;

	h->qh_io_flags = 0;
	h->qh_type = type;
	h->qh_fmt = fmt;
	memset(&h->qh_info, 0, sizeof(h->qh_info));
	h->qh_ops = &quotafile_ops_2;

	if (h->qh_ops->new_io && (h->qh_ops->new_io(h) < 0)) {
		log_err("qh_ops->new_io failed");
		goto out_err1;
	}

	return 0;

out_err1:
	ext2fs_file_close(e2_file);
out_err:

	if (qf_inum)
		quota_inode_truncate(fs, qf_inum);

	return -1;
}

/*
 * Close quotafile and release handle
 */
errcode_t quota_file_close(struct quota_handle *h)
{
	if (h->qh_io_flags & IOFL_INFODIRTY) {
		if (h->qh_ops->write_info && h->qh_ops->write_info(h) < 0)
			return -1;
		h->qh_io_flags &= ~IOFL_INFODIRTY;
	}

	if (h->qh_ops->end_io && h->qh_ops->end_io(h) < 0)
		return -1;
	if (h->qh_qf.e2_file) {
		ext2fs_file_flush(h->qh_qf.e2_file);
		ext2fs_file_set_size2(h->qh_qf.e2_file,
			compute_inode_size(h->qh_qf.fs, h->qh_qf.ino));
		ext2fs_file_close(h->qh_qf.e2_file);
	}

	return 0;
}

/*
 * Create empty quota structure
 */
struct dquot *get_empty_dquot(void)
{
	struct dquot *dquot;

	if (ext2fs_get_memzero(sizeof(struct dquot), &dquot)) {
		log_err("Failed to allocate dquot");
		return NULL;
	}

	dquot->dq_id = -1;
	return dquot;
}
