/*
 * Copyright (c) 2000-2001,2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef	__XFS_BUF_ITEM_H__
#define	__XFS_BUF_ITEM_H__

extern kmem_zone_t	*xfs_buf_item_zone;

/*
 * This is the structure used to lay out a buf log item in the
 * log.  The data map describes which 128 byte chunks of the buffer
 * have been logged.
 * For 6.2 and beyond, this is XFS_LI_BUF.  We use this to log everything.
 */
typedef struct xfs_buf_log_format {
	unsigned short	blf_type;	/* buf log item type indicator */
	unsigned short	blf_size;	/* size of this item */
	ushort		blf_flags;	/* misc state */
	ushort		blf_len;	/* number of blocks in this buf */
	__int64_t	blf_blkno;	/* starting blkno of this buf */
	unsigned int	blf_map_size;	/* size of data bitmap in words */
	unsigned int	blf_data_map[1];/* variable size bitmap of */
					/*   regions of buffer in this item */
} xfs_buf_log_format_t;

/*
 * This flag indicates that the buffer contains on disk inodes
 * and requires special recovery handling.
 */
#define	XFS_BLF_INODE_BUF	0x1
/*
 * This flag indicates that the buffer should not be replayed
 * during recovery because its blocks are being freed.
 */
#define	XFS_BLF_CANCEL		0x2
/*
 * This flag indicates that the buffer contains on disk
 * user or group dquots and may require special recovery handling.
 */
#define	XFS_BLF_UDQUOT_BUF	0x4
#define XFS_BLF_PDQUOT_BUF	0x8
#define	XFS_BLF_GDQUOT_BUF	0x10

#define	XFS_BLF_CHUNK		128
#define	XFS_BLF_SHIFT		7
#define	BIT_TO_WORD_SHIFT	5
#define	NBWORD			(NBBY * sizeof(unsigned int))

/*
 * buf log item flags
 */
#define	XFS_BLI_HOLD		0x01
#define	XFS_BLI_DIRTY		0x02
#define	XFS_BLI_STALE		0x04
#define	XFS_BLI_LOGGED		0x08
#define	XFS_BLI_INODE_ALLOC_BUF	0x10
#define XFS_BLI_STALE_INODE	0x20
#define	XFS_BLI_INODE_BUF	0x40

#define XFS_BLI_FLAGS \
	{ XFS_BLI_HOLD,		"HOLD" }, \
	{ XFS_BLI_DIRTY,	"DIRTY" }, \
	{ XFS_BLI_STALE,	"STALE" }, \
	{ XFS_BLI_LOGGED,	"LOGGED" }, \
	{ XFS_BLI_INODE_ALLOC_BUF, "INODE_ALLOC" }, \
	{ XFS_BLI_STALE_INODE,	"STALE_INODE" }, \
	{ XFS_BLI_INODE_BUF,	"INODE_BUF" }


#ifdef __KERNEL__

struct xfs_buf;
struct xfs_mount;
struct xfs_buf_log_item;

/*
 * This is the in core log item structure used to track information
 * needed to log buffers.  It tracks how many times the lock has been
 * locked, and which 128 byte chunks of the buffer are dirty.
 */
typedef struct xfs_buf_log_item {
	xfs_log_item_t		bli_item;	/* common item structure */
	struct xfs_buf		*bli_buf;	/* real buffer pointer */
	unsigned int		bli_flags;	/* misc flags */
	unsigned int		bli_recur;	/* lock recursion count */
	atomic_t		bli_refcount;	/* cnt of tp refs */
#ifdef XFS_TRANS_DEBUG
	char			*bli_orig;	/* original buffer copy */
	char			*bli_logged;	/* bytes logged (bitmap) */
#endif
	xfs_buf_log_format_t	bli_format;	/* in-log header */
} xfs_buf_log_item_t;

/*
 * This structure is used during recovery to record the buf log
 * items which have been canceled and should not be replayed.
 */
typedef struct xfs_buf_cancel {
	xfs_daddr_t		bc_blkno;
	uint			bc_len;
	int			bc_refcount;
	struct xfs_buf_cancel	*bc_next;
} xfs_buf_cancel_t;

void	xfs_buf_item_init(struct xfs_buf *, struct xfs_mount *);
void	xfs_buf_item_relse(struct xfs_buf *);
void	xfs_buf_item_log(xfs_buf_log_item_t *, uint, uint);
uint	xfs_buf_item_dirty(xfs_buf_log_item_t *);
void	xfs_buf_attach_iodone(struct xfs_buf *,
			      void(*)(struct xfs_buf *, xfs_log_item_t *),
			      xfs_log_item_t *);
void	xfs_buf_iodone_callbacks(struct xfs_buf *);
void	xfs_buf_iodone(struct xfs_buf *, struct xfs_log_item *);

#ifdef XFS_TRANS_DEBUG
void
xfs_buf_item_flush_log_debug(
	struct xfs_buf *bp,
	uint	first,
	uint	last);
#else
#define	xfs_buf_item_flush_log_debug(bp, first, last)
#endif

#endif	/* __KERNEL__ */

#endif	/* __XFS_BUF_ITEM_H__ */
