/*
 * Copyright (c) 2000-2002,2005 Silicon Graphics, Inc.
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
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_types.h"
#include "xfs_bit.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_inode_item.h"
#include "xfs_btree.h"
#include "xfs_btree_trace.h"
#include "xfs_error.h"
#include "xfs_trace.h"

/*
 * Cursor allocation zone.
 */
kmem_zone_t	*xfs_btree_cur_zone;

/*
 * Btree magic numbers.
 */
const __uint32_t xfs_magics[XFS_BTNUM_MAX] = {
	XFS_ABTB_MAGIC, XFS_ABTC_MAGIC, XFS_BMAP_MAGIC, XFS_IBT_MAGIC
};


STATIC int				/* error (0 or EFSCORRUPTED) */
xfs_btree_check_lblock(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	struct xfs_btree_block	*block,	/* btree long form block pointer */
	int			level,	/* level of the btree block */
	struct xfs_buf		*bp)	/* buffer for block, if any */
{
	int			lblock_ok; /* block passes checks */
	struct xfs_mount	*mp;	/* file system mount point */

	mp = cur->bc_mp;
	lblock_ok =
		be32_to_cpu(block->bb_magic) == xfs_magics[cur->bc_btnum] &&
		be16_to_cpu(block->bb_level) == level &&
		be16_to_cpu(block->bb_numrecs) <=
			cur->bc_ops->get_maxrecs(cur, level) &&
		block->bb_u.l.bb_leftsib &&
		(be64_to_cpu(block->bb_u.l.bb_leftsib) == NULLDFSBNO ||
		 XFS_FSB_SANITY_CHECK(mp,
		 	be64_to_cpu(block->bb_u.l.bb_leftsib))) &&
		block->bb_u.l.bb_rightsib &&
		(be64_to_cpu(block->bb_u.l.bb_rightsib) == NULLDFSBNO ||
		 XFS_FSB_SANITY_CHECK(mp,
		 	be64_to_cpu(block->bb_u.l.bb_rightsib)));
	if (unlikely(XFS_TEST_ERROR(!lblock_ok, mp,
			XFS_ERRTAG_BTREE_CHECK_LBLOCK,
			XFS_RANDOM_BTREE_CHECK_LBLOCK))) {
		if (bp)
			trace_xfs_btree_corrupt(bp, _RET_IP_);
		XFS_ERROR_REPORT("xfs_btree_check_lblock", XFS_ERRLEVEL_LOW,
				 mp);
		return XFS_ERROR(EFSCORRUPTED);
	}
	return 0;
}

STATIC int				/* error (0 or EFSCORRUPTED) */
xfs_btree_check_sblock(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	struct xfs_btree_block	*block,	/* btree short form block pointer */
	int			level,	/* level of the btree block */
	struct xfs_buf		*bp)	/* buffer containing block */
{
	struct xfs_buf		*agbp;	/* buffer for ag. freespace struct */
	struct xfs_agf		*agf;	/* ag. freespace structure */
	xfs_agblock_t		agflen;	/* native ag. freespace length */
	int			sblock_ok; /* block passes checks */

	agbp = cur->bc_private.a.agbp;
	agf = XFS_BUF_TO_AGF(agbp);
	agflen = be32_to_cpu(agf->agf_length);
	sblock_ok =
		be32_to_cpu(block->bb_magic) == xfs_magics[cur->bc_btnum] &&
		be16_to_cpu(block->bb_level) == level &&
		be16_to_cpu(block->bb_numrecs) <=
			cur->bc_ops->get_maxrecs(cur, level) &&
		(be32_to_cpu(block->bb_u.s.bb_leftsib) == NULLAGBLOCK ||
		 be32_to_cpu(block->bb_u.s.bb_leftsib) < agflen) &&
		block->bb_u.s.bb_leftsib &&
		(be32_to_cpu(block->bb_u.s.bb_rightsib) == NULLAGBLOCK ||
		 be32_to_cpu(block->bb_u.s.bb_rightsib) < agflen) &&
		block->bb_u.s.bb_rightsib;
	if (unlikely(XFS_TEST_ERROR(!sblock_ok, cur->bc_mp,
			XFS_ERRTAG_BTREE_CHECK_SBLOCK,
			XFS_RANDOM_BTREE_CHECK_SBLOCK))) {
		if (bp)
			trace_xfs_btree_corrupt(bp, _RET_IP_);
		XFS_CORRUPTION_ERROR("xfs_btree_check_sblock",
			XFS_ERRLEVEL_LOW, cur->bc_mp, block);
		return XFS_ERROR(EFSCORRUPTED);
	}
	return 0;
}

/*
 * Debug routine: check that block header is ok.
 */
int
xfs_btree_check_block(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	struct xfs_btree_block	*block,	/* generic btree block pointer */
	int			level,	/* level of the btree block */
	struct xfs_buf		*bp)	/* buffer containing block, if any */
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return xfs_btree_check_lblock(cur, block, level, bp);
	else
		return xfs_btree_check_sblock(cur, block, level, bp);
}

/*
 * Check that (long) pointer is ok.
 */
int					/* error (0 or EFSCORRUPTED) */
xfs_btree_check_lptr(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	xfs_dfsbno_t		bno,	/* btree block disk address */
	int			level)	/* btree block level */
{
	XFS_WANT_CORRUPTED_RETURN(
		level > 0 &&
		bno != NULLDFSBNO &&
		XFS_FSB_SANITY_CHECK(cur->bc_mp, bno));
	return 0;
}

#ifdef DEBUG
/*
 * Check that (short) pointer is ok.
 */
STATIC int				/* error (0 or EFSCORRUPTED) */
xfs_btree_check_sptr(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	xfs_agblock_t		bno,	/* btree block disk address */
	int			level)	/* btree block level */
{
	xfs_agblock_t		agblocks = cur->bc_mp->m_sb.sb_agblocks;

	XFS_WANT_CORRUPTED_RETURN(
		level > 0 &&
		bno != NULLAGBLOCK &&
		bno != 0 &&
		bno < agblocks);
	return 0;
}

/*
 * Check that block ptr is ok.
 */
STATIC int				/* error (0 or EFSCORRUPTED) */
xfs_btree_check_ptr(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	union xfs_btree_ptr	*ptr,	/* btree block disk address */
	int			index,	/* offset from ptr to check */
	int			level)	/* btree block level */
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		return xfs_btree_check_lptr(cur,
				be64_to_cpu((&ptr->l)[index]), level);
	} else {
		return xfs_btree_check_sptr(cur,
				be32_to_cpu((&ptr->s)[index]), level);
	}
}
#endif

/*
 * Delete the btree cursor.
 */
void
xfs_btree_del_cursor(
	xfs_btree_cur_t	*cur,		/* btree cursor */
	int		error)		/* del because of error */
{
	int		i;		/* btree level */

	/*
	 * Clear the buffer pointers, and release the buffers.
	 * If we're doing this in the face of an error, we
	 * need to make sure to inspect all of the entries
	 * in the bc_bufs array for buffers to be unlocked.
	 * This is because some of the btree code works from
	 * level n down to 0, and if we get an error along
	 * the way we won't have initialized all the entries
	 * down to 0.
	 */
	for (i = 0; i < cur->bc_nlevels; i++) {
		if (cur->bc_bufs[i])
			xfs_btree_setbuf(cur, i, NULL);
		else if (!error)
			break;
	}
	/*
	 * Can't free a bmap cursor without having dealt with the
	 * allocated indirect blocks' accounting.
	 */
	ASSERT(cur->bc_btnum != XFS_BTNUM_BMAP ||
	       cur->bc_private.b.allocated == 0);
	/*
	 * Free the cursor.
	 */
	kmem_zone_free(xfs_btree_cur_zone, cur);
}

/*
 * Duplicate the btree cursor.
 * Allocate a new one, copy the record, re-get the buffers.
 */
int					/* error */
xfs_btree_dup_cursor(
	xfs_btree_cur_t	*cur,		/* input cursor */
	xfs_btree_cur_t	**ncur)		/* output cursor */
{
	xfs_buf_t	*bp;		/* btree block's buffer pointer */
	int		error;		/* error return value */
	int		i;		/* level number of btree block */
	xfs_mount_t	*mp;		/* mount structure for filesystem */
	xfs_btree_cur_t	*new;		/* new cursor value */
	xfs_trans_t	*tp;		/* transaction pointer, can be NULL */

	tp = cur->bc_tp;
	mp = cur->bc_mp;

	/*
	 * Allocate a new cursor like the old one.
	 */
	new = cur->bc_ops->dup_cursor(cur);

	/*
	 * Copy the record currently in the cursor.
	 */
	new->bc_rec = cur->bc_rec;

	/*
	 * For each level current, re-get the buffer and copy the ptr value.
	 */
	for (i = 0; i < new->bc_nlevels; i++) {
		new->bc_ptrs[i] = cur->bc_ptrs[i];
		new->bc_ra[i] = cur->bc_ra[i];
		if ((bp = cur->bc_bufs[i])) {
			if ((error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp,
				XFS_BUF_ADDR(bp), mp->m_bsize, 0, &bp))) {
				xfs_btree_del_cursor(new, error);
				*ncur = NULL;
				return error;
			}
			new->bc_bufs[i] = bp;
			ASSERT(bp);
			ASSERT(!XFS_BUF_GETERROR(bp));
		} else
			new->bc_bufs[i] = NULL;
	}
	*ncur = new;
	return 0;
}

/*
 * XFS btree block layout and addressing:
 *
 * There are two types of blocks in the btree: leaf and non-leaf blocks.
 *
 * The leaf record start with a header then followed by records containing
 * the values.  A non-leaf block also starts with the same header, and
 * then first contains lookup keys followed by an equal number of pointers
 * to the btree blocks at the previous level.
 *
 *		+--------+-------+-------+-------+-------+-------+-------+
 * Leaf:	| header | rec 1 | rec 2 | rec 3 | rec 4 | rec 5 | rec N |
 *		+--------+-------+-------+-------+-------+-------+-------+
 *
 *		+--------+-------+-------+-------+-------+-------+-------+
 * Non-Leaf:	| header | key 1 | key 2 | key N | ptr 1 | ptr 2 | ptr N |
 *		+--------+-------+-------+-------+-------+-------+-------+
 *
 * The header is called struct xfs_btree_block for reasons better left unknown
 * and comes in different versions for short (32bit) and long (64bit) block
 * pointers.  The record and key structures are defined by the btree instances
 * and opaque to the btree core.  The block pointers are simple disk endian
 * integers, available in a short (32bit) and long (64bit) variant.
 *
 * The helpers below calculate the offset of a given record, key or pointer
 * into a btree block (xfs_btree_*_offset) or return a pointer to the given
 * record, key or pointer (xfs_btree_*_addr).  Note that all addressing
 * inside the btree block is done using indices starting at one, not zero!
 */

/*
 * Return size of the btree block header for this btree instance.
 */
static inline size_t xfs_btree_block_len(struct xfs_btree_cur *cur)
{
	return (cur->bc_flags & XFS_BTREE_LONG_PTRS) ?
		XFS_BTREE_LBLOCK_LEN :
		XFS_BTREE_SBLOCK_LEN;
}

/*
 * Return size of btree block pointers for this btree instance.
 */
static inline size_t xfs_btree_ptr_len(struct xfs_btree_cur *cur)
{
	return (cur->bc_flags & XFS_BTREE_LONG_PTRS) ?
		sizeof(__be64) : sizeof(__be32);
}

/*
 * Calculate offset of the n-th record in a btree block.
 */
STATIC size_t
xfs_btree_rec_offset(
	struct xfs_btree_cur	*cur,
	int			n)
{
	return xfs_btree_block_len(cur) +
		(n - 1) * cur->bc_ops->rec_len;
}

/*
 * Calculate offset of the n-th key in a btree block.
 */
STATIC size_t
xfs_btree_key_offset(
	struct xfs_btree_cur	*cur,
	int			n)
{
	return xfs_btree_block_len(cur) +
		(n - 1) * cur->bc_ops->key_len;
}

/*
 * Calculate offset of the n-th block pointer in a btree block.
 */
STATIC size_t
xfs_btree_ptr_offset(
	struct xfs_btree_cur	*cur,
	int			n,
	int			level)
{
	return xfs_btree_block_len(cur) +
		cur->bc_ops->get_maxrecs(cur, level) * cur->bc_ops->key_len +
		(n - 1) * xfs_btree_ptr_len(cur);
}

/*
 * Return a pointer to the n-th record in the btree block.
 */
STATIC union xfs_btree_rec *
xfs_btree_rec_addr(
	struct xfs_btree_cur	*cur,
	int			n,
	struct xfs_btree_block	*block)
{
	return (union xfs_btree_rec *)
		((char *)block + xfs_btree_rec_offset(cur, n));
}

/*
 * Return a pointer to the n-th key in the btree block.
 */
STATIC union xfs_btree_key *
xfs_btree_key_addr(
	struct xfs_btree_cur	*cur,
	int			n,
	struct xfs_btree_block	*block)
{
	return (union xfs_btree_key *)
		((char *)block + xfs_btree_key_offset(cur, n));
}

/*
 * Return a pointer to the n-th block pointer in the btree block.
 */
STATIC union xfs_btree_ptr *
xfs_btree_ptr_addr(
	struct xfs_btree_cur	*cur,
	int			n,
	struct xfs_btree_block	*block)
{
	int			level = xfs_btree_get_level(block);

	ASSERT(block->bb_level != 0);

	return (union xfs_btree_ptr *)
		((char *)block + xfs_btree_ptr_offset(cur, n, level));
}

/*
 * Get a the root block which is stored in the inode.
 *
 * For now this btree implementation assumes the btree root is always
 * stored in the if_broot field of an inode fork.
 */
STATIC struct xfs_btree_block *
xfs_btree_get_iroot(
       struct xfs_btree_cur    *cur)
{
       struct xfs_ifork        *ifp;

       ifp = XFS_IFORK_PTR(cur->bc_private.b.ip, cur->bc_private.b.whichfork);
       return (struct xfs_btree_block *)ifp->if_broot;
}

/*
 * Retrieve the block pointer from the cursor at the given level.
 * This may be an inode btree root or from a buffer.
 */
STATIC struct xfs_btree_block *		/* generic btree block pointer */
xfs_btree_get_block(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	int			level,	/* level in btree */
	struct xfs_buf		**bpp)	/* buffer containing the block */
{
	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (level == cur->bc_nlevels - 1)) {
		*bpp = NULL;
		return xfs_btree_get_iroot(cur);
	}

	*bpp = cur->bc_bufs[level];
	return XFS_BUF_TO_BLOCK(*bpp);
}

/*
 * Get a buffer for the block, return it with no data read.
 * Long-form addressing.
 */
xfs_buf_t *				/* buffer for fsbno */
xfs_btree_get_bufl(
	xfs_mount_t	*mp,		/* file system mount point */
	xfs_trans_t	*tp,		/* transaction pointer */
	xfs_fsblock_t	fsbno,		/* file system block number */
	uint		lock)		/* lock flags for get_buf */
{
	xfs_buf_t	*bp;		/* buffer pointer (return value) */
	xfs_daddr_t		d;		/* real disk block address */

	ASSERT(fsbno != NULLFSBLOCK);
	d = XFS_FSB_TO_DADDR(mp, fsbno);
	bp = xfs_trans_get_buf(tp, mp->m_ddev_targp, d, mp->m_bsize, lock);
	ASSERT(bp);
	ASSERT(!XFS_BUF_GETERROR(bp));
	return bp;
}

/*
 * Get a buffer for the block, return it with no data read.
 * Short-form addressing.
 */
xfs_buf_t *				/* buffer for agno/agbno */
xfs_btree_get_bufs(
	xfs_mount_t	*mp,		/* file system mount point */
	xfs_trans_t	*tp,		/* transaction pointer */
	xfs_agnumber_t	agno,		/* allocation group number */
	xfs_agblock_t	agbno,		/* allocation group block number */
	uint		lock)		/* lock flags for get_buf */
{
	xfs_buf_t	*bp;		/* buffer pointer (return value) */
	xfs_daddr_t		d;		/* real disk block address */

	ASSERT(agno != NULLAGNUMBER);
	ASSERT(agbno != NULLAGBLOCK);
	d = XFS_AGB_TO_DADDR(mp, agno, agbno);
	bp = xfs_trans_get_buf(tp, mp->m_ddev_targp, d, mp->m_bsize, lock);
	ASSERT(bp);
	ASSERT(!XFS_BUF_GETERROR(bp));
	return bp;
}

/*
 * Check for the cursor referring to the last block at the given level.
 */
int					/* 1=is last block, 0=not last block */
xfs_btree_islastblock(
	xfs_btree_cur_t		*cur,	/* btree cursor */
	int			level)	/* level to check */
{
	struct xfs_btree_block	*block;	/* generic btree block pointer */
	xfs_buf_t		*bp;	/* buffer containing block */

	block = xfs_btree_get_block(cur, level, &bp);
	xfs_btree_check_block(cur, block, level, bp);
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return be64_to_cpu(block->bb_u.l.bb_rightsib) == NULLDFSBNO;
	else
		return be32_to_cpu(block->bb_u.s.bb_rightsib) == NULLAGBLOCK;
}

/*
 * Change the cursor to point to the first record at the given level.
 * Other levels are unaffected.
 */
STATIC int				/* success=1, failure=0 */
xfs_btree_firstrec(
	xfs_btree_cur_t		*cur,	/* btree cursor */
	int			level)	/* level to change */
{
	struct xfs_btree_block	*block;	/* generic btree block pointer */
	xfs_buf_t		*bp;	/* buffer containing block */

	/*
	 * Get the block pointer for this level.
	 */
	block = xfs_btree_get_block(cur, level, &bp);
	xfs_btree_check_block(cur, block, level, bp);
	/*
	 * It's empty, there is no such record.
	 */
	if (!block->bb_numrecs)
		return 0;
	/*
	 * Set the ptr value to 1, that's the first record/key.
	 */
	cur->bc_ptrs[level] = 1;
	return 1;
}

/*
 * Change the cursor to point to the last record in the current block
 * at the given level.  Other levels are unaffected.
 */
STATIC int				/* success=1, failure=0 */
xfs_btree_lastrec(
	xfs_btree_cur_t		*cur,	/* btree cursor */
	int			level)	/* level to change */
{
	struct xfs_btree_block	*block;	/* generic btree block pointer */
	xfs_buf_t		*bp;	/* buffer containing block */

	/*
	 * Get the block pointer for this level.
	 */
	block = xfs_btree_get_block(cur, level, &bp);
	xfs_btree_check_block(cur, block, level, bp);
	/*
	 * It's empty, there is no such record.
	 */
	if (!block->bb_numrecs)
		return 0;
	/*
	 * Set the ptr value to numrecs, that's the last record/key.
	 */
	cur->bc_ptrs[level] = be16_to_cpu(block->bb_numrecs);
	return 1;
}

/*
 * Compute first and last byte offsets for the fields given.
 * Interprets the offsets table, which contains struct field offsets.
 */
void
xfs_btree_offsets(
	__int64_t	fields,		/* bitmask of fields */
	const short	*offsets,	/* table of field offsets */
	int		nbits,		/* number of bits to inspect */
	int		*first,		/* output: first byte offset */
	int		*last)		/* output: last byte offset */
{
	int		i;		/* current bit number */
	__int64_t	imask;		/* mask for current bit number */

	ASSERT(fields != 0);
	/*
	 * Find the lowest bit, so the first byte offset.
	 */
	for (i = 0, imask = 1LL; ; i++, imask <<= 1) {
		if (imask & fields) {
			*first = offsets[i];
			break;
		}
	}
	/*
	 * Find the highest bit, so the last byte offset.
	 */
	for (i = nbits - 1, imask = 1LL << i; ; i--, imask >>= 1) {
		if (imask & fields) {
			*last = offsets[i + 1] - 1;
			break;
		}
	}
}

/*
 * Get a buffer for the block, return it read in.
 * Long-form addressing.
 */
int					/* error */
xfs_btree_read_bufl(
	xfs_mount_t	*mp,		/* file system mount point */
	xfs_trans_t	*tp,		/* transaction pointer */
	xfs_fsblock_t	fsbno,		/* file system block number */
	uint		lock,		/* lock flags for read_buf */
	xfs_buf_t	**bpp,		/* buffer for fsbno */
	int		refval)		/* ref count value for buffer */
{
	xfs_buf_t	*bp;		/* return value */
	xfs_daddr_t		d;		/* real disk block address */
	int		error;

	ASSERT(fsbno != NULLFSBLOCK);
	d = XFS_FSB_TO_DADDR(mp, fsbno);
	if ((error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp, d,
			mp->m_bsize, lock, &bp))) {
		return error;
	}
	ASSERT(!bp || !XFS_BUF_GETERROR(bp));
	if (bp != NULL) {
		XFS_BUF_SET_VTYPE_REF(bp, B_FS_MAP, refval);
	}
	*bpp = bp;
	return 0;
}

/*
 * Read-ahead the block, don't wait for it, don't return a buffer.
 * Long-form addressing.
 */
/* ARGSUSED */
void
xfs_btree_reada_bufl(
	xfs_mount_t	*mp,		/* file system mount point */
	xfs_fsblock_t	fsbno,		/* file system block number */
	xfs_extlen_t	count)		/* count of filesystem blocks */
{
	xfs_daddr_t		d;

	ASSERT(fsbno != NULLFSBLOCK);
	d = XFS_FSB_TO_DADDR(mp, fsbno);
	xfs_baread(mp->m_ddev_targp, d, mp->m_bsize * count);
}

/*
 * Read-ahead the block, don't wait for it, don't return a buffer.
 * Short-form addressing.
 */
/* ARGSUSED */
void
xfs_btree_reada_bufs(
	xfs_mount_t	*mp,		/* file system mount point */
	xfs_agnumber_t	agno,		/* allocation group number */
	xfs_agblock_t	agbno,		/* allocation group block number */
	xfs_extlen_t	count)		/* count of filesystem blocks */
{
	xfs_daddr_t		d;

	ASSERT(agno != NULLAGNUMBER);
	ASSERT(agbno != NULLAGBLOCK);
	d = XFS_AGB_TO_DADDR(mp, agno, agbno);
	xfs_baread(mp->m_ddev_targp, d, mp->m_bsize * count);
}

STATIC int
xfs_btree_readahead_lblock(
	struct xfs_btree_cur	*cur,
	int			lr,
	struct xfs_btree_block	*block)
{
	int			rval = 0;
	xfs_dfsbno_t		left = be64_to_cpu(block->bb_u.l.bb_leftsib);
	xfs_dfsbno_t		right = be64_to_cpu(block->bb_u.l.bb_rightsib);

	if ((lr & XFS_BTCUR_LEFTRA) && left != NULLDFSBNO) {
		xfs_btree_reada_bufl(cur->bc_mp, left, 1);
		rval++;
	}

	if ((lr & XFS_BTCUR_RIGHTRA) && right != NULLDFSBNO) {
		xfs_btree_reada_bufl(cur->bc_mp, right, 1);
		rval++;
	}

	return rval;
}

STATIC int
xfs_btree_readahead_sblock(
	struct xfs_btree_cur	*cur,
	int			lr,
	struct xfs_btree_block *block)
{
	int			rval = 0;
	xfs_agblock_t		left = be32_to_cpu(block->bb_u.s.bb_leftsib);
	xfs_agblock_t		right = be32_to_cpu(block->bb_u.s.bb_rightsib);


	if ((lr & XFS_BTCUR_LEFTRA) && left != NULLAGBLOCK) {
		xfs_btree_reada_bufs(cur->bc_mp, cur->bc_private.a.agno,
				     left, 1);
		rval++;
	}

	if ((lr & XFS_BTCUR_RIGHTRA) && right != NULLAGBLOCK) {
		xfs_btree_reada_bufs(cur->bc_mp, cur->bc_private.a.agno,
				     right, 1);
		rval++;
	}

	return rval;
}

/*
 * Read-ahead btree blocks, at the given level.
 * Bits in lr are set from XFS_BTCUR_{LEFT,RIGHT}RA.
 */
STATIC int
xfs_btree_readahead(
	struct xfs_btree_cur	*cur,		/* btree cursor */
	int			lev,		/* level in btree */
	int			lr)		/* left/right bits */
{
	struct xfs_btree_block	*block;

	/*
	 * No readahead needed if we are at the root level and the
	 * btree root is stored in the inode.
	 */
	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (lev == cur->bc_nlevels - 1))
		return 0;

	if ((cur->bc_ra[lev] | lr) == cur->bc_ra[lev])
		return 0;

	cur->bc_ra[lev] |= lr;
	block = XFS_BUF_TO_BLOCK(cur->bc_bufs[lev]);

	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return xfs_btree_readahead_lblock(cur, lr, block);
	return xfs_btree_readahead_sblock(cur, lr, block);
}

/*
 * Set the buffer for level "lev" in the cursor to bp, releasing
 * any previous buffer.
 */
void
xfs_btree_setbuf(
	xfs_btree_cur_t		*cur,	/* btree cursor */
	int			lev,	/* level in btree */
	xfs_buf_t		*bp)	/* new buffer to set */
{
	struct xfs_btree_block	*b;	/* btree block */
	xfs_buf_t		*obp;	/* old buffer pointer */

	obp = cur->bc_bufs[lev];
	if (obp)
		xfs_trans_brelse(cur->bc_tp, obp);
	cur->bc_bufs[lev] = bp;
	cur->bc_ra[lev] = 0;
	if (!bp)
		return;
	b = XFS_BUF_TO_BLOCK(bp);
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (be64_to_cpu(b->bb_u.l.bb_leftsib) == NULLDFSBNO)
			cur->bc_ra[lev] |= XFS_BTCUR_LEFTRA;
		if (be64_to_cpu(b->bb_u.l.bb_rightsib) == NULLDFSBNO)
			cur->bc_ra[lev] |= XFS_BTCUR_RIGHTRA;
	} else {
		if (be32_to_cpu(b->bb_u.s.bb_leftsib) == NULLAGBLOCK)
			cur->bc_ra[lev] |= XFS_BTCUR_LEFTRA;
		if (be32_to_cpu(b->bb_u.s.bb_rightsib) == NULLAGBLOCK)
			cur->bc_ra[lev] |= XFS_BTCUR_RIGHTRA;
	}
}

STATIC int
xfs_btree_ptr_is_null(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return be64_to_cpu(ptr->l) == NULLDFSBNO;
	else
		return be32_to_cpu(ptr->s) == NULLAGBLOCK;
}

STATIC void
xfs_btree_set_ptr_null(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		ptr->l = cpu_to_be64(NULLDFSBNO);
	else
		ptr->s = cpu_to_be32(NULLAGBLOCK);
}

/*
 * Get/set/init sibling pointers
 */
STATIC void
xfs_btree_get_sibling(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	union xfs_btree_ptr	*ptr,
	int			lr)
{
	ASSERT(lr == XFS_BB_LEFTSIB || lr == XFS_BB_RIGHTSIB);

	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (lr == XFS_BB_RIGHTSIB)
			ptr->l = block->bb_u.l.bb_rightsib;
		else
			ptr->l = block->bb_u.l.bb_leftsib;
	} else {
		if (lr == XFS_BB_RIGHTSIB)
			ptr->s = block->bb_u.s.bb_rightsib;
		else
			ptr->s = block->bb_u.s.bb_leftsib;
	}
}

STATIC void
xfs_btree_set_sibling(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	union xfs_btree_ptr	*ptr,
	int			lr)
{
	ASSERT(lr == XFS_BB_LEFTSIB || lr == XFS_BB_RIGHTSIB);

	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (lr == XFS_BB_RIGHTSIB)
			block->bb_u.l.bb_rightsib = ptr->l;
		else
			block->bb_u.l.bb_leftsib = ptr->l;
	} else {
		if (lr == XFS_BB_RIGHTSIB)
			block->bb_u.s.bb_rightsib = ptr->s;
		else
			block->bb_u.s.bb_leftsib = ptr->s;
	}
}

STATIC void
xfs_btree_init_block(
	struct xfs_btree_cur	*cur,
	int			level,
	int			numrecs,
	struct xfs_btree_block	*new)	/* new block */
{
	new->bb_magic = cpu_to_be32(xfs_magics[cur->bc_btnum]);
	new->bb_level = cpu_to_be16(level);
	new->bb_numrecs = cpu_to_be16(numrecs);

	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		new->bb_u.l.bb_leftsib = cpu_to_be64(NULLDFSBNO);
		new->bb_u.l.bb_rightsib = cpu_to_be64(NULLDFSBNO);
	} else {
		new->bb_u.s.bb_leftsib = cpu_to_be32(NULLAGBLOCK);
		new->bb_u.s.bb_rightsib = cpu_to_be32(NULLAGBLOCK);
	}
}

/*
 * Return true if ptr is the last record in the btree and
 * we need to track updateѕ to this record.  The decision
 * will be further refined in the update_lastrec method.
 */
STATIC int
xfs_btree_is_lastrec(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	int			level)
{
	union xfs_btree_ptr	ptr;

	if (level > 0)
		return 0;
	if (!(cur->bc_flags & XFS_BTREE_LASTREC_UPDATE))
		return 0;

	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_RIGHTSIB);
	if (!xfs_btree_ptr_is_null(cur, &ptr))
		return 0;
	return 1;
}

STATIC void
xfs_btree_buf_to_ptr(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp,
	union xfs_btree_ptr	*ptr)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		ptr->l = cpu_to_be64(XFS_DADDR_TO_FSB(cur->bc_mp,
					XFS_BUF_ADDR(bp)));
	else {
		ptr->s = cpu_to_be32(xfs_daddr_to_agbno(cur->bc_mp,
					XFS_BUF_ADDR(bp)));
	}
}

STATIC xfs_daddr_t
xfs_btree_ptr_to_daddr(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		ASSERT(be64_to_cpu(ptr->l) != NULLDFSBNO);

		return XFS_FSB_TO_DADDR(cur->bc_mp, be64_to_cpu(ptr->l));
	} else {
		ASSERT(cur->bc_private.a.agno != NULLAGNUMBER);
		ASSERT(be32_to_cpu(ptr->s) != NULLAGBLOCK);

		return XFS_AGB_TO_DADDR(cur->bc_mp, cur->bc_private.a.agno,
					be32_to_cpu(ptr->s));
	}
}

STATIC void
xfs_btree_set_refs(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp)
{
	switch (cur->bc_btnum) {
	case XFS_BTNUM_BNO:
	case XFS_BTNUM_CNT:
		XFS_BUF_SET_VTYPE_REF(*bpp, B_FS_MAP, XFS_ALLOC_BTREE_REF);
		break;
	case XFS_BTNUM_INO:
		XFS_BUF_SET_VTYPE_REF(*bpp, B_FS_INOMAP, XFS_INO_BTREE_REF);
		break;
	case XFS_BTNUM_BMAP:
		XFS_BUF_SET_VTYPE_REF(*bpp, B_FS_MAP, XFS_BMAP_BTREE_REF);
		break;
	default:
		ASSERT(0);
	}
}

STATIC int
xfs_btree_get_buf_block(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr,
	int			flags,
	struct xfs_btree_block	**block,
	struct xfs_buf		**bpp)
{
	struct xfs_mount	*mp = cur->bc_mp;
	xfs_daddr_t		d;

	/* need to sort out how callers deal with failures first */
	ASSERT(!(flags & XBF_TRYLOCK));

	d = xfs_btree_ptr_to_daddr(cur, ptr);
	*bpp = xfs_trans_get_buf(cur->bc_tp, mp->m_ddev_targp, d,
				 mp->m_bsize, flags);

	ASSERT(*bpp);
	ASSERT(!XFS_BUF_GETERROR(*bpp));

	*block = XFS_BUF_TO_BLOCK(*bpp);
	return 0;
}

/*
 * Read in the buffer at the given ptr and return the buffer and
 * the block pointer within the buffer.
 */
STATIC int
xfs_btree_read_buf_block(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr,
	int			level,
	int			flags,
	struct xfs_btree_block	**block,
	struct xfs_buf		**bpp)
{
	struct xfs_mount	*mp = cur->bc_mp;
	xfs_daddr_t		d;
	int			error;

	/* need to sort out how callers deal with failures first */
	ASSERT(!(flags & XBF_TRYLOCK));

	d = xfs_btree_ptr_to_daddr(cur, ptr);
	error = xfs_trans_read_buf(mp, cur->bc_tp, mp->m_ddev_targp, d,
				   mp->m_bsize, flags, bpp);
	if (error)
		return error;

	ASSERT(*bpp != NULL);
	ASSERT(!XFS_BUF_GETERROR(*bpp));

	xfs_btree_set_refs(cur, *bpp);
	*block = XFS_BUF_TO_BLOCK(*bpp);

	error = xfs_btree_check_block(cur, *block, level, *bpp);
	if (error)
		xfs_trans_brelse(cur->bc_tp, *bpp);
	return error;
}

/*
 * Copy keys from one btree block to another.
 */
STATIC void
xfs_btree_copy_keys(
	struct xfs_btree_cur	*cur,
	union xfs_btree_key	*dst_key,
	union xfs_btree_key	*src_key,
	int			numkeys)
{
	ASSERT(numkeys >= 0);
	memcpy(dst_key, src_key, numkeys * cur->bc_ops->key_len);
}

/*
 * Copy records from one btree block to another.
 */
STATIC void
xfs_btree_copy_recs(
	struct xfs_btree_cur	*cur,
	union xfs_btree_rec	*dst_rec,
	union xfs_btree_rec	*src_rec,
	int			numrecs)
{
	ASSERT(numrecs >= 0);
	memcpy(dst_rec, src_rec, numrecs * cur->bc_ops->rec_len);
}

/*
 * Copy block pointers from one btree block to another.
 */
STATIC void
xfs_btree_copy_ptrs(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*dst_ptr,
	union xfs_btree_ptr	*src_ptr,
	int			numptrs)
{
	ASSERT(numptrs >= 0);
	memcpy(dst_ptr, src_ptr, numptrs * xfs_btree_ptr_len(cur));
}

/*
 * Shift keys one index left/right inside a single btree block.
 */
STATIC void
xfs_btree_shift_keys(
	struct xfs_btree_cur	*cur,
	union xfs_btree_key	*key,
	int			dir,
	int			numkeys)
{
	char			*dst_key;

	ASSERT(numkeys >= 0);
	ASSERT(dir == 1 || dir == -1);

	dst_key = (char *)key + (dir * cur->bc_ops->key_len);
	memmove(dst_key, key, numkeys * cur->bc_ops->key_len);
}

/*
 * Shift records one index left/right inside a single btree block.
 */
STATIC void
xfs_btree_shift_recs(
	struct xfs_btree_cur	*cur,
	union xfs_btree_rec	*rec,
	int			dir,
	int			numrecs)
{
	char			*dst_rec;

	ASSERT(numrecs >= 0);
	ASSERT(dir == 1 || dir == -1);

	dst_rec = (char *)rec + (dir * cur->bc_ops->rec_len);
	memmove(dst_rec, rec, numrecs * cur->bc_ops->rec_len);
}

/*
 * Shift block pointers one index left/right inside a single btree block.
 */
STATIC void
xfs_btree_shift_ptrs(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr,
	int			dir,
	int			numptrs)
{
	char			*dst_ptr;

	ASSERT(numptrs >= 0);
	ASSERT(dir == 1 || dir == -1);

	dst_ptr = (char *)ptr + (dir * xfs_btree_ptr_len(cur));
	memmove(dst_ptr, ptr, numptrs * xfs_btree_ptr_len(cur));
}

/*
 * Log key values from the btree block.
 */
STATIC void
xfs_btree_log_keys(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp,
	int			first,
	int			last)
{
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGBII(cur, bp, first, last);

	if (bp) {
		xfs_trans_log_buf(cur->bc_tp, bp,
				  xfs_btree_key_offset(cur, first),
				  xfs_btree_key_offset(cur, last + 1) - 1);
	} else {
		xfs_trans_log_inode(cur->bc_tp, cur->bc_private.b.ip,
				xfs_ilog_fbroot(cur->bc_private.b.whichfork));
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
}

/*
 * Log record values from the btree block.
 */
void
xfs_btree_log_recs(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp,
	int			first,
	int			last)
{
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGBII(cur, bp, first, last);

	xfs_trans_log_buf(cur->bc_tp, bp,
			  xfs_btree_rec_offset(cur, first),
			  xfs_btree_rec_offset(cur, last + 1) - 1);

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
}

/*
 * Log block pointer fields from a btree block (nonleaf).
 */
STATIC void
xfs_btree_log_ptrs(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	struct xfs_buf		*bp,	/* buffer containing btree block */
	int			first,	/* index of first pointer to log */
	int			last)	/* index of last pointer to log */
{
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGBII(cur, bp, first, last);

	if (bp) {
		struct xfs_btree_block	*block = XFS_BUF_TO_BLOCK(bp);
		int			level = xfs_btree_get_level(block);

		xfs_trans_log_buf(cur->bc_tp, bp,
				xfs_btree_ptr_offset(cur, first, level),
				xfs_btree_ptr_offset(cur, last + 1, level) - 1);
	} else {
		xfs_trans_log_inode(cur->bc_tp, cur->bc_private.b.ip,
			xfs_ilog_fbroot(cur->bc_private.b.whichfork));
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
}

/*
 * Log fields from a btree block header.
 */
void
xfs_btree_log_block(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	struct xfs_buf		*bp,	/* buffer containing btree block */
	int			fields)	/* mask of fields: XFS_BB_... */
{
	int			first;	/* first byte offset logged */
	int			last;	/* last byte offset logged */
	static const short	soffsets[] = {	/* table of offsets (short) */
		offsetof(struct xfs_btree_block, bb_magic),
		offsetof(struct xfs_btree_block, bb_level),
		offsetof(struct xfs_btree_block, bb_numrecs),
		offsetof(struct xfs_btree_block, bb_u.s.bb_leftsib),
		offsetof(struct xfs_btree_block, bb_u.s.bb_rightsib),
		XFS_BTREE_SBLOCK_LEN
	};
	static const short	loffsets[] = {	/* table of offsets (long) */
		offsetof(struct xfs_btree_block, bb_magic),
		offsetof(struct xfs_btree_block, bb_level),
		offsetof(struct xfs_btree_block, bb_numrecs),
		offsetof(struct xfs_btree_block, bb_u.l.bb_leftsib),
		offsetof(struct xfs_btree_block, bb_u.l.bb_rightsib),
		XFS_BTREE_LBLOCK_LEN
	};

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGBI(cur, bp, fields);

	if (bp) {
		xfs_btree_offsets(fields,
				  (cur->bc_flags & XFS_BTREE_LONG_PTRS) ?
					loffsets : soffsets,
				  XFS_BB_NUM_BITS, &first, &last);
		xfs_trans_log_buf(cur->bc_tp, bp, first, last);
	} else {
		xfs_trans_log_inode(cur->bc_tp, cur->bc_private.b.ip,
			xfs_ilog_fbroot(cur->bc_private.b.whichfork));
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
}

/*
 * Increment cursor by one record at the level.
 * For nonzero levels the leaf-ward information is untouched.
 */
int						/* error */
xfs_btree_increment(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)		/* success/failure */
{
	struct xfs_btree_block	*block;
	union xfs_btree_ptr	ptr;
	struct xfs_buf		*bp;
	int			error;		/* error return value */
	int			lev;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGI(cur, level);

	ASSERT(level < cur->bc_nlevels);

	/* Read-ahead to the right at this level. */
	xfs_btree_readahead(cur, level, XFS_BTCUR_RIGHTRA);

	/* Get a pointer to the btree block. */
	block = xfs_btree_get_block(cur, level, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;
#endif

	/* We're done if we remain in the block after the increment. */
	if (++cur->bc_ptrs[level] <= xfs_btree_get_numrecs(block))
		goto out1;

	/* Fail if we just went off the right edge of the tree. */
	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_RIGHTSIB);
	if (xfs_btree_ptr_is_null(cur, &ptr))
		goto out0;

	XFS_BTREE_STATS_INC(cur, increment);

	/*
	 * March up the tree incrementing pointers.
	 * Stop when we don't go off the right edge of a block.
	 */
	for (lev = level + 1; lev < cur->bc_nlevels; lev++) {
		block = xfs_btree_get_block(cur, lev, &bp);

#ifdef DEBUG
		error = xfs_btree_check_block(cur, block, lev, bp);
		if (error)
			goto error0;
#endif

		if (++cur->bc_ptrs[lev] <= xfs_btree_get_numrecs(block))
			break;

		/* Read-ahead the right block for the next loop. */
		xfs_btree_readahead(cur, lev, XFS_BTCUR_RIGHTRA);
	}

	/*
	 * If we went off the root then we are either seriously
	 * confused or have the tree root in an inode.
	 */
	if (lev == cur->bc_nlevels) {
		if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE)
			goto out0;
		ASSERT(0);
		error = EFSCORRUPTED;
		goto error0;
	}
	ASSERT(lev < cur->bc_nlevels);

	/*
	 * Now walk back down the tree, fixing up the cursor's buffer
	 * pointers and key numbers.
	 */
	for (block = xfs_btree_get_block(cur, lev, &bp); lev > level; ) {
		union xfs_btree_ptr	*ptrp;

		ptrp = xfs_btree_ptr_addr(cur, cur->bc_ptrs[lev], block);
		error = xfs_btree_read_buf_block(cur, ptrp, --lev,
							0, &block, &bp);
		if (error)
			goto error0;

		xfs_btree_setbuf(cur, lev, bp);
		cur->bc_ptrs[lev] = 1;
	}
out1:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;

out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 0;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

/*
 * Decrement cursor by one record at the level.
 * For nonzero levels the leaf-ward information is untouched.
 */
int						/* error */
xfs_btree_decrement(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)		/* success/failure */
{
	struct xfs_btree_block	*block;
	xfs_buf_t		*bp;
	int			error;		/* error return value */
	int			lev;
	union xfs_btree_ptr	ptr;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGI(cur, level);

	ASSERT(level < cur->bc_nlevels);

	/* Read-ahead to the left at this level. */
	xfs_btree_readahead(cur, level, XFS_BTCUR_LEFTRA);

	/* We're done if we remain in the block after the decrement. */
	if (--cur->bc_ptrs[level] > 0)
		goto out1;

	/* Get a pointer to the btree block. */
	block = xfs_btree_get_block(cur, level, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;
#endif

	/* Fail if we just went off the left edge of the tree. */
	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_LEFTSIB);
	if (xfs_btree_ptr_is_null(cur, &ptr))
		goto out0;

	XFS_BTREE_STATS_INC(cur, decrement);

	/*
	 * March up the tree decrementing pointers.
	 * Stop when we don't go off the left edge of a block.
	 */
	for (lev = level + 1; lev < cur->bc_nlevels; lev++) {
		if (--cur->bc_ptrs[lev] > 0)
			break;
		/* Read-ahead the left block for the next loop. */
		xfs_btree_readahead(cur, lev, XFS_BTCUR_LEFTRA);
	}

	/*
	 * If we went off the root then we are seriously confused.
	 * or the root of the tree is in an inode.
	 */
	if (lev == cur->bc_nlevels) {
		if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE)
			goto out0;
		ASSERT(0);
		error = EFSCORRUPTED;
		goto error0;
	}
	ASSERT(lev < cur->bc_nlevels);

	/*
	 * Now walk back down the tree, fixing up the cursor's buffer
	 * pointers and key numbers.
	 */
	for (block = xfs_btree_get_block(cur, lev, &bp); lev > level; ) {
		union xfs_btree_ptr	*ptrp;

		ptrp = xfs_btree_ptr_addr(cur, cur->bc_ptrs[lev], block);
		error = xfs_btree_read_buf_block(cur, ptrp, --lev,
							0, &block, &bp);
		if (error)
			goto error0;
		xfs_btree_setbuf(cur, lev, bp);
		cur->bc_ptrs[lev] = xfs_btree_get_numrecs(block);
	}
out1:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;

out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 0;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

STATIC int
xfs_btree_lookup_get_block(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	int			level,	/* level in the btree */
	union xfs_btree_ptr	*pp,	/* ptr to btree block */
	struct xfs_btree_block	**blkp) /* return btree block */
{
	struct xfs_buf		*bp;	/* buffer pointer for btree block */
	int			error = 0;

	/* special case the root block if in an inode */
	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (level == cur->bc_nlevels - 1)) {
		*blkp = xfs_btree_get_iroot(cur);
		return 0;
	}

	/*
	 * If the old buffer at this level for the disk address we are
	 * looking for re-use it.
	 *
	 * Otherwise throw it away and get a new one.
	 */
	bp = cur->bc_bufs[level];
	if (bp && XFS_BUF_ADDR(bp) == xfs_btree_ptr_to_daddr(cur, pp)) {
		*blkp = XFS_BUF_TO_BLOCK(bp);
		return 0;
	}

	error = xfs_btree_read_buf_block(cur, pp, level, 0, blkp, &bp);
	if (error)
		return error;

	xfs_btree_setbuf(cur, level, bp);
	return 0;
}

/*
 * Get current search key.  For level 0 we don't actually have a key
 * structure so we make one up from the record.  For all other levels
 * we just return the right key.
 */
STATIC union xfs_btree_key *
xfs_lookup_get_search_key(
	struct xfs_btree_cur	*cur,
	int			level,
	int			keyno,
	struct xfs_btree_block	*block,
	union xfs_btree_key	*kp)
{
	if (level == 0) {
		cur->bc_ops->init_key_from_rec(kp,
				xfs_btree_rec_addr(cur, keyno, block));
		return kp;
	}

	return xfs_btree_key_addr(cur, keyno, block);
}

/*
 * Lookup the record.  The cursor is made to point to it, based on dir.
 * Return 0 if can't find any such record, 1 for success.
 */
int					/* error */
xfs_btree_lookup(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	xfs_lookup_t		dir,	/* <=, ==, or >= */
	int			*stat)	/* success/failure */
{
	struct xfs_btree_block	*block;	/* current btree block */
	__int64_t		diff;	/* difference for the current key */
	int			error;	/* error return value */
	int			keyno;	/* current key number */
	int			level;	/* level in the btree */
	union xfs_btree_ptr	*pp;	/* ptr to btree block */
	union xfs_btree_ptr	ptr;	/* ptr to btree block */

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGI(cur, dir);

	XFS_BTREE_STATS_INC(cur, lookup);

	block = NULL;
	keyno = 0;

	/* initialise start pointer from cursor */
	cur->bc_ops->init_ptr_from_cur(cur, &ptr);
	pp = &ptr;

	/*
	 * Iterate over each level in the btree, starting at the root.
	 * For each level above the leaves, find the key we need, based
	 * on the lookup record, then follow the corresponding block
	 * pointer down to the next level.
	 */
	for (level = cur->bc_nlevels - 1, diff = 1; level >= 0; level--) {
		/* Get the block we need to do the lookup on. */
		error = xfs_btree_lookup_get_block(cur, level, pp, &block);
		if (error)
			goto error0;

		if (diff == 0) {
			/*
			 * If we already had a key match at a higher level, we
			 * know we need to use the first entry in this block.
			 */
			keyno = 1;
		} else {
			/* Otherwise search this block. Do a binary search. */

			int	high;	/* high entry number */
			int	low;	/* low entry number */

			/* Set low and high entry numbers, 1-based. */
			low = 1;
			high = xfs_btree_get_numrecs(block);
			if (!high) {
				/* Block is empty, must be an empty leaf. */
				ASSERT(level == 0 && cur->bc_nlevels == 1);

				cur->bc_ptrs[0] = dir != XFS_LOOKUP_LE;
				XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
				*stat = 0;
				return 0;
			}

			/* Binary search the block. */
			while (low <= high) {
				union xfs_btree_key	key;
				union xfs_btree_key	*kp;

				XFS_BTREE_STATS_INC(cur, compare);

				/* keyno is average of low and high. */
				keyno = (low + high) >> 1;

				/* Get current search key */
				kp = xfs_lookup_get_search_key(cur, level,
						keyno, block, &key);

				/*
				 * Compute difference to get next direction:
				 *  - less than, move right
				 *  - greater than, move left
				 *  - equal, we're done
				 */
				diff = cur->bc_ops->key_diff(cur, kp);
				if (diff < 0)
					low = keyno + 1;
				else if (diff > 0)
					high = keyno - 1;
				else
					break;
			}
		}

		/*
		 * If there are more levels, set up for the next level
		 * by getting the block number and filling in the cursor.
		 */
		if (level > 0) {
			/*
			 * If we moved left, need the previous key number,
			 * unless there isn't one.
			 */
			if (diff > 0 && --keyno < 1)
				keyno = 1;
			pp = xfs_btree_ptr_addr(cur, keyno, block);

#ifdef DEBUG
			error = xfs_btree_check_ptr(cur, pp, 0, level);
			if (error)
				goto error0;
#endif
			cur->bc_ptrs[level] = keyno;
		}
	}

	/* Done with the search. See if we need to adjust the results. */
	if (dir != XFS_LOOKUP_LE && diff < 0) {
		keyno++;
		/*
		 * If ge search and we went off the end of the block, but it's
		 * not the last block, we're in the wrong block.
		 */
		xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_RIGHTSIB);
		if (dir == XFS_LOOKUP_GE &&
		    keyno > xfs_btree_get_numrecs(block) &&
		    !xfs_btree_ptr_is_null(cur, &ptr)) {
			int	i;

			cur->bc_ptrs[0] = keyno;
			error = xfs_btree_increment(cur, 0, &i);
			if (error)
				goto error0;
			XFS_WANT_CORRUPTED_RETURN(i == 1);
			XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
			*stat = 1;
			return 0;
		}
	} else if (dir == XFS_LOOKUP_LE && diff > 0)
		keyno--;
	cur->bc_ptrs[0] = keyno;

	/* Return if we succeeded or not. */
	if (keyno == 0 || keyno > xfs_btree_get_numrecs(block))
		*stat = 0;
	else if (dir != XFS_LOOKUP_EQ || diff == 0)
		*stat = 1;
	else
		*stat = 0;
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

/*
 * Update keys at all levels from here to the root along the cursor's path.
 */
STATIC int
xfs_btree_updkey(
	struct xfs_btree_cur	*cur,
	union xfs_btree_key	*keyp,
	int			level)
{
	struct xfs_btree_block	*block;
	struct xfs_buf		*bp;
	union xfs_btree_key	*kp;
	int			ptr;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGIK(cur, level, keyp);

	ASSERT(!(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) || level >= 1);

	/*
	 * Go up the tree from this level toward the root.
	 * At each level, update the key value to the value input.
	 * Stop when we reach a level where the cursor isn't pointing
	 * at the first entry in the block.
	 */
	for (ptr = 1; ptr == 1 && level < cur->bc_nlevels; level++) {
#ifdef DEBUG
		int		error;
#endif
		block = xfs_btree_get_block(cur, level, &bp);
#ifdef DEBUG
		error = xfs_btree_check_block(cur, block, level, bp);
		if (error) {
			XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
			return error;
		}
#endif
		ptr = cur->bc_ptrs[level];
		kp = xfs_btree_key_addr(cur, ptr, block);
		xfs_btree_copy_keys(cur, kp, keyp, 1);
		xfs_btree_log_keys(cur, bp, ptr, ptr);
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	return 0;
}

/*
 * Update the record referred to by cur to the value in the
 * given record. This either works (return 0) or gets an
 * EFSCORRUPTED error.
 */
int
xfs_btree_update(
	struct xfs_btree_cur	*cur,
	union xfs_btree_rec	*rec)
{
	struct xfs_btree_block	*block;
	struct xfs_buf		*bp;
	int			error;
	int			ptr;
	union xfs_btree_rec	*rp;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGR(cur, rec);

	/* Pick up the current block. */
	block = xfs_btree_get_block(cur, 0, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, 0, bp);
	if (error)
		goto error0;
#endif
	/* Get the address of the rec to be updated. */
	ptr = cur->bc_ptrs[0];
	rp = xfs_btree_rec_addr(cur, ptr, block);

	/* Fill in the new contents and log them. */
	xfs_btree_copy_recs(cur, rp, rec, 1);
	xfs_btree_log_recs(cur, bp, ptr, ptr);

	/*
	 * If we are tracking the last record in the tree and
	 * we are at the far right edge of the tree, update it.
	 */
	if (xfs_btree_is_lastrec(cur, block, 0)) {
		cur->bc_ops->update_lastrec(cur, block, rec,
					    ptr, LASTREC_UPDATE);
	}

	/* Updating first rec in leaf. Pass new key value up to our parent. */
	if (ptr == 1) {
		union xfs_btree_key	key;

		cur->bc_ops->init_key_from_rec(&key, rec);
		error = xfs_btree_updkey(cur, &key, 1);
		if (error)
			goto error0;
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

/*
 * Move 1 record left from cur/level if possible.
 * Update cur to reflect the new path.
 */
STATIC int					/* error */
xfs_btree_lshift(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)		/* success/failure */
{
	union xfs_btree_key	key;		/* btree key */
	struct xfs_buf		*lbp;		/* left buffer pointer */
	struct xfs_btree_block	*left;		/* left btree block */
	int			lrecs;		/* left record count */
	struct xfs_buf		*rbp;		/* right buffer pointer */
	struct xfs_btree_block	*right;		/* right btree block */
	int			rrecs;		/* right record count */
	union xfs_btree_ptr	lptr;		/* left btree pointer */
	union xfs_btree_key	*rkp = NULL;	/* right btree key */
	union xfs_btree_ptr	*rpp = NULL;	/* right address pointer */
	union xfs_btree_rec	*rrp = NULL;	/* right record pointer */
	int			error;		/* error return value */

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGI(cur, level);

	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    level == cur->bc_nlevels - 1)
		goto out0;

	/* Set up variables for this block as "right". */
	right = xfs_btree_get_block(cur, level, &rbp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, right, level, rbp);
	if (error)
		goto error0;
#endif

	/* If we've got no left sibling then we can't shift an entry left. */
	xfs_btree_get_sibling(cur, right, &lptr, XFS_BB_LEFTSIB);
	if (xfs_btree_ptr_is_null(cur, &lptr))
		goto out0;

	/*
	 * If the cursor entry is the one that would be moved, don't
	 * do it... it's too complicated.
	 */
	if (cur->bc_ptrs[level] <= 1)
		goto out0;

	/* Set up the left neighbor as "left". */
	error = xfs_btree_read_buf_block(cur, &lptr, level, 0, &left, &lbp);
	if (error)
		goto error0;

	/* If it's full, it can't take another entry. */
	lrecs = xfs_btree_get_numrecs(left);
	if (lrecs == cur->bc_ops->get_maxrecs(cur, level))
		goto out0;

	rrecs = xfs_btree_get_numrecs(right);

	/*
	 * We add one entry to the left side and remove one for the right side.
	 * Account for it here, the changes will be updated on disk and logged
	 * later.
	 */
	lrecs++;
	rrecs--;

	XFS_BTREE_STATS_INC(cur, lshift);
	XFS_BTREE_STATS_ADD(cur, moves, 1);

	/*
	 * If non-leaf, copy a key and a ptr to the left block.
	 * Log the changes to the left block.
	 */
	if (level > 0) {
		/* It's a non-leaf.  Move keys and pointers. */
		union xfs_btree_key	*lkp;	/* left btree key */
		union xfs_btree_ptr	*lpp;	/* left address pointer */

		lkp = xfs_btree_key_addr(cur, lrecs, left);
		rkp = xfs_btree_key_addr(cur, 1, right);

		lpp = xfs_btree_ptr_addr(cur, lrecs, left);
		rpp = xfs_btree_ptr_addr(cur, 1, right);
#ifdef DEBUG
		error = xfs_btree_check_ptr(cur, rpp, 0, level);
		if (error)
			goto error0;
#endif
		xfs_btree_copy_keys(cur, lkp, rkp, 1);
		xfs_btree_copy_ptrs(cur, lpp, rpp, 1);

		xfs_btree_log_keys(cur, lbp, lrecs, lrecs);
		xfs_btree_log_ptrs(cur, lbp, lrecs, lrecs);

		ASSERT(cur->bc_ops->keys_inorder(cur,
			xfs_btree_key_addr(cur, lrecs - 1, left), lkp));
	} else {
		/* It's a leaf.  Move records.  */
		union xfs_btree_rec	*lrp;	/* left record pointer */

		lrp = xfs_btree_rec_addr(cur, lrecs, left);
		rrp = xfs_btree_rec_addr(cur, 1, right);

		xfs_btree_copy_recs(cur, lrp, rrp, 1);
		xfs_btree_log_recs(cur, lbp, lrecs, lrecs);

		ASSERT(cur->bc_ops->recs_inorder(cur,
			xfs_btree_rec_addr(cur, lrecs - 1, left), lrp));
	}

	xfs_btree_set_numrecs(left, lrecs);
	xfs_btree_log_block(cur, lbp, XFS_BB_NUMRECS);

	xfs_btree_set_numrecs(right, rrecs);
	xfs_btree_log_block(cur, rbp, XFS_BB_NUMRECS);

	/*
	 * Slide the contents of right down one entry.
	 */
	XFS_BTREE_STATS_ADD(cur, moves, rrecs - 1);
	if (level > 0) {
		/* It's a nonleaf. operate on keys and ptrs */
#ifdef DEBUG
		int			i;		/* loop index */

		for (i = 0; i < rrecs; i++) {
			error = xfs_btree_check_ptr(cur, rpp, i + 1, level);
			if (error)
				goto error0;
		}
#endif
		xfs_btree_shift_keys(cur,
				xfs_btree_key_addr(cur, 2, right),
				-1, rrecs);
		xfs_btree_shift_ptrs(cur,
				xfs_btree_ptr_addr(cur, 2, right),
				-1, rrecs);

		xfs_btree_log_keys(cur, rbp, 1, rrecs);
		xfs_btree_log_ptrs(cur, rbp, 1, rrecs);
	} else {
		/* It's a leaf. operate on records */
		xfs_btree_shift_recs(cur,
			xfs_btree_rec_addr(cur, 2, right),
			-1, rrecs);
		xfs_btree_log_recs(cur, rbp, 1, rrecs);

		/*
		 * If it's the first record in the block, we'll need a key
		 * structure to pass up to the next level (updkey).
		 */
		cur->bc_ops->init_key_from_rec(&key,
			xfs_btree_rec_addr(cur, 1, right));
		rkp = &key;
	}

	/* Update the parent key values of right. */
	error = xfs_btree_updkey(cur, rkp, level + 1);
	if (error)
		goto error0;

	/* Slide the cursor value left one. */
	cur->bc_ptrs[level]--;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;

out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 0;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

/*
 * Move 1 record right from cur/level if possible.
 * Update cur to reflect the new path.
 */
STATIC int					/* error */
xfs_btree_rshift(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)		/* success/failure */
{
	union xfs_btree_key	key;		/* btree key */
	struct xfs_buf		*lbp;		/* left buffer pointer */
	struct xfs_btree_block	*left;		/* left btree block */
	struct xfs_buf		*rbp;		/* right buffer pointer */
	struct xfs_btree_block	*right;		/* right btree block */
	struct xfs_btree_cur	*tcur;		/* temporary btree cursor */
	union xfs_btree_ptr	rptr;		/* right block pointer */
	union xfs_btree_key	*rkp;		/* right btree key */
	int			rrecs;		/* right record count */
	int			lrecs;		/* left record count */
	int			error;		/* error return value */
	int			i;		/* loop counter */

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGI(cur, level);

	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (level == cur->bc_nlevels - 1))
		goto out0;

	/* Set up variables for this block as "left". */
	left = xfs_btree_get_block(cur, level, &lbp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, left, level, lbp);
	if (error)
		goto error0;
#endif

	/* If we've got no right sibling then we can't shift an entry right. */
	xfs_btree_get_sibling(cur, left, &rptr, XFS_BB_RIGHTSIB);
	if (xfs_btree_ptr_is_null(cur, &rptr))
		goto out0;

	/*
	 * If the cursor entry is the one that would be moved, don't
	 * do it... it's too complicated.
	 */
	lrecs = xfs_btree_get_numrecs(left);
	if (cur->bc_ptrs[level] >= lrecs)
		goto out0;

	/* Set up the right neighbor as "right". */
	error = xfs_btree_read_buf_block(cur, &rptr, level, 0, &right, &rbp);
	if (error)
		goto error0;

	/* If it's full, it can't take another entry. */
	rrecs = xfs_btree_get_numrecs(right);
	if (rrecs == cur->bc_ops->get_maxrecs(cur, level))
		goto out0;

	XFS_BTREE_STATS_INC(cur, rshift);
	XFS_BTREE_STATS_ADD(cur, moves, rrecs);

	/*
	 * Make a hole at the start of the right neighbor block, then
	 * copy the last left block entry to the hole.
	 */
	if (level > 0) {
		/* It's a nonleaf. make a hole in the keys and ptrs */
		union xfs_btree_key	*lkp;
		union xfs_btree_ptr	*lpp;
		union xfs_btree_ptr	*rpp;

		lkp = xfs_btree_key_addr(cur, lrecs, left);
		lpp = xfs_btree_ptr_addr(cur, lrecs, left);
		rkp = xfs_btree_key_addr(cur, 1, right);
		rpp = xfs_btree_ptr_addr(cur, 1, right);

#ifdef DEBUG
		for (i = rrecs - 1; i >= 0; i--) {
			error = xfs_btree_check_ptr(cur, rpp, i, level);
			if (error)
				goto error0;
		}
#endif

		xfs_btree_shift_keys(cur, rkp, 1, rrecs);
		xfs_btree_shift_ptrs(cur, rpp, 1, rrecs);

#ifdef DEBUG
		error = xfs_btree_check_ptr(cur, lpp, 0, level);
		if (error)
			goto error0;
#endif

		/* Now put the new data in, and log it. */
		xfs_btree_copy_keys(cur, rkp, lkp, 1);
		xfs_btree_copy_ptrs(cur, rpp, lpp, 1);

		xfs_btree_log_keys(cur, rbp, 1, rrecs + 1);
		xfs_btree_log_ptrs(cur, rbp, 1, rrecs + 1);

		ASSERT(cur->bc_ops->keys_inorder(cur, rkp,
			xfs_btree_key_addr(cur, 2, right)));
	} else {
		/* It's a leaf. make a hole in the records */
		union xfs_btree_rec	*lrp;
		union xfs_btree_rec	*rrp;

		lrp = xfs_btree_rec_addr(cur, lrecs, left);
		rrp = xfs_btree_rec_addr(cur, 1, right);

		xfs_btree_shift_recs(cur, rrp, 1, rrecs);

		/* Now put the new data in, and log it. */
		xfs_btree_copy_recs(cur, rrp, lrp, 1);
		xfs_btree_log_recs(cur, rbp, 1, rrecs + 1);

		cur->bc_ops->init_key_from_rec(&key, rrp);
		rkp = &key;

		ASSERT(cur->bc_ops->recs_inorder(cur, rrp,
			xfs_btree_rec_addr(cur, 2, right)));
	}

	/*
	 * Decrement and log left's numrecs, bump and log right's numrecs.
	 */
	xfs_btree_set_numrecs(left, --lrecs);
	xfs_btree_log_block(cur, lbp, XFS_BB_NUMRECS);

	xfs_btree_set_numrecs(right, ++rrecs);
	xfs_btree_log_block(cur, rbp, XFS_BB_NUMRECS);

	/*
	 * Using a temporary cursor, update the parent key values of the
	 * block on the right.
	 */
	error = xfs_btree_dup_cursor(cur, &tcur);
	if (error)
		goto error0;
	i = xfs_btree_lastrec(tcur, level);
	XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

	error = xfs_btree_increment(tcur, level, &i);
	if (error)
		goto error1;

	error = xfs_btree_updkey(tcur, rkp, level + 1);
	if (error)
		goto error1;

	xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;

out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 0;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;

error1:
	XFS_BTREE_TRACE_CURSOR(tcur, XBT_ERROR);
	xfs_btree_del_cursor(tcur, XFS_BTREE_ERROR);
	return error;
}

/*
 * Split cur/level block in half.
 * Return new block number and the key to its first
 * record (to be inserted into parent).
 */
STATIC int					/* error */
xfs_btree_split(
	struct xfs_btree_cur	*cur,
	int			level,
	union xfs_btree_ptr	*ptrp,
	union xfs_btree_key	*key,
	struct xfs_btree_cur	**curp,
	int			*stat)		/* success/failure */
{
	union xfs_btree_ptr	lptr;		/* left sibling block ptr */
	struct xfs_buf		*lbp;		/* left buffer pointer */
	struct xfs_btree_block	*left;		/* left btree block */
	union xfs_btree_ptr	rptr;		/* right sibling block ptr */
	struct xfs_buf		*rbp;		/* right buffer pointer */
	struct xfs_btree_block	*right;		/* right btree block */
	union xfs_btree_ptr	rrptr;		/* right-right sibling ptr */
	struct xfs_buf		*rrbp;		/* right-right buffer pointer */
	struct xfs_btree_block	*rrblock;	/* right-right btree block */
	int			lrecs;
	int			rrecs;
	int			src_index;
	int			error;		/* error return value */
#ifdef DEBUG
	int			i;
#endif

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGIPK(cur, level, *ptrp, key);

	XFS_BTREE_STATS_INC(cur, split);

	/* Set up left block (current one). */
	left = xfs_btree_get_block(cur, level, &lbp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, left, level, lbp);
	if (error)
		goto error0;
#endif

	xfs_btree_buf_to_ptr(cur, lbp, &lptr);

	/* Allocate the new block. If we can't do it, we're toast. Give up. */
	error = cur->bc_ops->alloc_block(cur, &lptr, &rptr, 1, stat);
	if (error)
		goto error0;
	if (*stat == 0)
		goto out0;
	XFS_BTREE_STATS_INC(cur, alloc);

	/* Set up the new block as "right". */
	error = xfs_btree_get_buf_block(cur, &rptr, 0, &right, &rbp);
	if (error)
		goto error0;

	/* Fill in the btree header for the new right block. */
	xfs_btree_init_block(cur, xfs_btree_get_level(left), 0, right);

	/*
	 * Split the entries between the old and the new block evenly.
	 * Make sure that if there's an odd number of entries now, that
	 * each new block will have the same number of entries.
	 */
	lrecs = xfs_btree_get_numrecs(left);
	rrecs = lrecs / 2;
	if ((lrecs & 1) && cur->bc_ptrs[level] <= rrecs + 1)
		rrecs++;
	src_index = (lrecs - rrecs + 1);

	XFS_BTREE_STATS_ADD(cur, moves, rrecs);

	/*
	 * Copy btree block entries from the left block over to the
	 * new block, the right. Update the right block and log the
	 * changes.
	 */
	if (level > 0) {
		/* It's a non-leaf.  Move keys and pointers. */
		union xfs_btree_key	*lkp;	/* left btree key */
		union xfs_btree_ptr	*lpp;	/* left address pointer */
		union xfs_btree_key	*rkp;	/* right btree key */
		union xfs_btree_ptr	*rpp;	/* right address pointer */

		lkp = xfs_btree_key_addr(cur, src_index, left);
		lpp = xfs_btree_ptr_addr(cur, src_index, left);
		rkp = xfs_btree_key_addr(cur, 1, right);
		rpp = xfs_btree_ptr_addr(cur, 1, right);

#ifdef DEBUG
		for (i = src_index; i < rrecs; i++) {
			error = xfs_btree_check_ptr(cur, lpp, i, level);
			if (error)
				goto error0;
		}
#endif

		xfs_btree_copy_keys(cur, rkp, lkp, rrecs);
		xfs_btree_copy_ptrs(cur, rpp, lpp, rrecs);

		xfs_btree_log_keys(cur, rbp, 1, rrecs);
		xfs_btree_log_ptrs(cur, rbp, 1, rrecs);

		/* Grab the keys to the entries moved to the right block */
		xfs_btree_copy_keys(cur, key, rkp, 1);
	} else {
		/* It's a leaf.  Move records.  */
		union xfs_btree_rec	*lrp;	/* left record pointer */
		union xfs_btree_rec	*rrp;	/* right record pointer */

		lrp = xfs_btree_rec_addr(cur, src_index, left);
		rrp = xfs_btree_rec_addr(cur, 1, right);

		xfs_btree_copy_recs(cur, rrp, lrp, rrecs);
		xfs_btree_log_recs(cur, rbp, 1, rrecs);

		cur->bc_ops->init_key_from_rec(key,
			xfs_btree_rec_addr(cur, 1, right));
	}


	/*
	 * Find the left block number by looking in the buffer.
	 * Adjust numrecs, sibling pointers.
	 */
	xfs_btree_get_sibling(cur, left, &rrptr, XFS_BB_RIGHTSIB);
	xfs_btree_set_sibling(cur, right, &rrptr, XFS_BB_RIGHTSIB);
	xfs_btree_set_sibling(cur, right, &lptr, XFS_BB_LEFTSIB);
	xfs_btree_set_sibling(cur, left, &rptr, XFS_BB_RIGHTSIB);

	lrecs -= rrecs;
	xfs_btree_set_numrecs(left, lrecs);
	xfs_btree_set_numrecs(right, xfs_btree_get_numrecs(right) + rrecs);

	xfs_btree_log_block(cur, rbp, XFS_BB_ALL_BITS);
	xfs_btree_log_block(cur, lbp, XFS_BB_NUMRECS | XFS_BB_RIGHTSIB);

	/*
	 * If there's a block to the new block's right, make that block
	 * point back to right instead of to left.
	 */
	if (!xfs_btree_ptr_is_null(cur, &rrptr)) {
		error = xfs_btree_read_buf_block(cur, &rrptr, level,
							0, &rrblock, &rrbp);
		if (error)
			goto error0;
		xfs_btree_set_sibling(cur, rrblock, &rptr, XFS_BB_LEFTSIB);
		xfs_btree_log_block(cur, rrbp, XFS_BB_LEFTSIB);
	}
	/*
	 * If the cursor is really in the right block, move it there.
	 * If it's just pointing past the last entry in left, then we'll
	 * insert there, so don't change anything in that case.
	 */
	if (cur->bc_ptrs[level] > lrecs + 1) {
		xfs_btree_setbuf(cur, level, rbp);
		cur->bc_ptrs[level] -= lrecs;
	}
	/*
	 * If there are more levels, we'll need another cursor which refers
	 * the right block, no matter where this cursor was.
	 */
	if (level + 1 < cur->bc_nlevels) {
		error = xfs_btree_dup_cursor(cur, curp);
		if (error)
			goto error0;
		(*curp)->bc_ptrs[level + 1]++;
	}
	*ptrp = rptr;
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;
out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 0;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

/*
 * Copy the old inode root contents into a real block and make the
 * broot point to it.
 */
int						/* error */
xfs_btree_new_iroot(
	struct xfs_btree_cur	*cur,		/* btree cursor */
	int			*logflags,	/* logging flags for inode */
	int			*stat)		/* return status - 0 fail */
{
	struct xfs_buf		*cbp;		/* buffer for cblock */
	struct xfs_btree_block	*block;		/* btree block */
	struct xfs_btree_block	*cblock;	/* child btree block */
	union xfs_btree_key	*ckp;		/* child key pointer */
	union xfs_btree_ptr	*cpp;		/* child ptr pointer */
	union xfs_btree_key	*kp;		/* pointer to btree key */
	union xfs_btree_ptr	*pp;		/* pointer to block addr */
	union xfs_btree_ptr	nptr;		/* new block addr */
	int			level;		/* btree level */
	int			error;		/* error return code */
#ifdef DEBUG
	int			i;		/* loop counter */
#endif

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_STATS_INC(cur, newroot);

	ASSERT(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE);

	level = cur->bc_nlevels - 1;

	block = xfs_btree_get_iroot(cur);
	pp = xfs_btree_ptr_addr(cur, 1, block);

	/* Allocate the new block. If we can't do it, we're toast. Give up. */
	error = cur->bc_ops->alloc_block(cur, pp, &nptr, 1, stat);
	if (error)
		goto error0;
	if (*stat == 0) {
		XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
		return 0;
	}
	XFS_BTREE_STATS_INC(cur, alloc);

	/* Copy the root into a real block. */
	error = xfs_btree_get_buf_block(cur, &nptr, 0, &cblock, &cbp);
	if (error)
		goto error0;

	memcpy(cblock, block, xfs_btree_block_len(cur));

	be16_add_cpu(&block->bb_level, 1);
	xfs_btree_set_numrecs(block, 1);
	cur->bc_nlevels++;
	cur->bc_ptrs[level + 1] = 1;

	kp = xfs_btree_key_addr(cur, 1, block);
	ckp = xfs_btree_key_addr(cur, 1, cblock);
	xfs_btree_copy_keys(cur, ckp, kp, xfs_btree_get_numrecs(cblock));

	cpp = xfs_btree_ptr_addr(cur, 1, cblock);
#ifdef DEBUG
	for (i = 0; i < be16_to_cpu(cblock->bb_numrecs); i++) {
		error = xfs_btree_check_ptr(cur, pp, i, level);
		if (error)
			goto error0;
	}
#endif
	xfs_btree_copy_ptrs(cur, cpp, pp, xfs_btree_get_numrecs(cblock));

#ifdef DEBUG
	error = xfs_btree_check_ptr(cur, &nptr, 0, level);
	if (error)
		goto error0;
#endif
	xfs_btree_copy_ptrs(cur, pp, &nptr, 1);

	xfs_iroot_realloc(cur->bc_private.b.ip,
			  1 - xfs_btree_get_numrecs(cblock),
			  cur->bc_private.b.whichfork);

	xfs_btree_setbuf(cur, level, cbp);

	/*
	 * Do all this logging at the end so that
	 * the root is at the right level.
	 */
	xfs_btree_log_block(cur, cbp, XFS_BB_ALL_BITS);
	xfs_btree_log_keys(cur, cbp, 1, be16_to_cpu(cblock->bb_numrecs));
	xfs_btree_log_ptrs(cur, cbp, 1, be16_to_cpu(cblock->bb_numrecs));

	*logflags |=
		XFS_ILOG_CORE | xfs_ilog_fbroot(cur->bc_private.b.whichfork);
	*stat = 1;
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	return 0;
error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

/*
 * Allocate a new root block, fill it in.
 */
STATIC int				/* error */
xfs_btree_new_root(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	int			*stat)	/* success/failure */
{
	struct xfs_btree_block	*block;	/* one half of the old root block */
	struct xfs_buf		*bp;	/* buffer containing block */
	int			error;	/* error return value */
	struct xfs_buf		*lbp;	/* left buffer pointer */
	struct xfs_btree_block	*left;	/* left btree block */
	struct xfs_buf		*nbp;	/* new (root) buffer */
	struct xfs_btree_block	*new;	/* new (root) btree block */
	int			nptr;	/* new value for key index, 1 or 2 */
	struct xfs_buf		*rbp;	/* right buffer pointer */
	struct xfs_btree_block	*right;	/* right btree block */
	union xfs_btree_ptr	rptr;
	union xfs_btree_ptr	lptr;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_STATS_INC(cur, newroot);

	/* initialise our start point from the cursor */
	cur->bc_ops->init_ptr_from_cur(cur, &rptr);

	/* Allocate the new block. If we can't do it, we're toast. Give up. */
	error = cur->bc_ops->alloc_block(cur, &rptr, &lptr, 1, stat);
	if (error)
		goto error0;
	if (*stat == 0)
		goto out0;
	XFS_BTREE_STATS_INC(cur, alloc);

	/* Set up the new block. */
	error = xfs_btree_get_buf_block(cur, &lptr, 0, &new, &nbp);
	if (error)
		goto error0;

	/* Set the root in the holding structure  increasing the level by 1. */
	cur->bc_ops->set_root(cur, &lptr, 1);

	/*
	 * At the previous root level there are now two blocks: the old root,
	 * and the new block generated when it was split.  We don't know which
	 * one the cursor is pointing at, so we set up variables "left" and
	 * "right" for each case.
	 */
	block = xfs_btree_get_block(cur, cur->bc_nlevels - 1, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, cur->bc_nlevels - 1, bp);
	if (error)
		goto error0;
#endif

	xfs_btree_get_sibling(cur, block, &rptr, XFS_BB_RIGHTSIB);
	if (!xfs_btree_ptr_is_null(cur, &rptr)) {
		/* Our block is left, pick up the right block. */
		lbp = bp;
		xfs_btree_buf_to_ptr(cur, lbp, &lptr);
		left = block;
		error = xfs_btree_read_buf_block(cur, &rptr,
					cur->bc_nlevels - 1, 0, &right, &rbp);
		if (error)
			goto error0;
		bp = rbp;
		nptr = 1;
	} else {
		/* Our block is right, pick up the left block. */
		rbp = bp;
		xfs_btree_buf_to_ptr(cur, rbp, &rptr);
		right = block;
		xfs_btree_get_sibling(cur, right, &lptr, XFS_BB_LEFTSIB);
		error = xfs_btree_read_buf_block(cur, &lptr,
					cur->bc_nlevels - 1, 0, &left, &lbp);
		if (error)
			goto error0;
		bp = lbp;
		nptr = 2;
	}
	/* Fill in the new block's btree header and log it. */
	xfs_btree_init_block(cur, cur->bc_nlevels, 2, new);
	xfs_btree_log_block(cur, nbp, XFS_BB_ALL_BITS);
	ASSERT(!xfs_btree_ptr_is_null(cur, &lptr) &&
			!xfs_btree_ptr_is_null(cur, &rptr));

	/* Fill in the key data in the new root. */
	if (xfs_btree_get_level(left) > 0) {
		xfs_btree_copy_keys(cur,
				xfs_btree_key_addr(cur, 1, new),
				xfs_btree_key_addr(cur, 1, left), 1);
		xfs_btree_copy_keys(cur,
				xfs_btree_key_addr(cur, 2, new),
				xfs_btree_key_addr(cur, 1, right), 1);
	} else {
		cur->bc_ops->init_key_from_rec(
				xfs_btree_key_addr(cur, 1, new),
				xfs_btree_rec_addr(cur, 1, left));
		cur->bc_ops->init_key_from_rec(
				xfs_btree_key_addr(cur, 2, new),
				xfs_btree_rec_addr(cur, 1, right));
	}
	xfs_btree_log_keys(cur, nbp, 1, 2);

	/* Fill in the pointer data in the new root. */
	xfs_btree_copy_ptrs(cur,
		xfs_btree_ptr_addr(cur, 1, new), &lptr, 1);
	xfs_btree_copy_ptrs(cur,
		xfs_btree_ptr_addr(cur, 2, new), &rptr, 1);
	xfs_btree_log_ptrs(cur, nbp, 1, 2);

	/* Fix up the cursor. */
	xfs_btree_setbuf(cur, cur->bc_nlevels, nbp);
	cur->bc_ptrs[cur->bc_nlevels] = nptr;
	cur->bc_nlevels++;
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;
error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 0;
	return 0;
}

STATIC int
xfs_btree_make_block_unfull(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	int			level,	/* btree level */
	int			numrecs,/* # of recs in block */
	int			*oindex,/* old tree index */
	int			*index,	/* new tree index */
	union xfs_btree_ptr	*nptr,	/* new btree ptr */
	struct xfs_btree_cur	**ncur,	/* new btree cursor */
	union xfs_btree_rec	*nrec,	/* new record */
	int			*stat)
{
	union xfs_btree_key	key;	/* new btree key value */
	int			error = 0;

	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    level == cur->bc_nlevels - 1) {
	    	struct xfs_inode *ip = cur->bc_private.b.ip;

		if (numrecs < cur->bc_ops->get_dmaxrecs(cur, level)) {
			/* A root block that can be made bigger. */

			xfs_iroot_realloc(ip, 1, cur->bc_private.b.whichfork);
		} else {
			/* A root block that needs replacing */
			int	logflags = 0;

			error = xfs_btree_new_iroot(cur, &logflags, stat);
			if (error || *stat == 0)
				return error;

			xfs_trans_log_inode(cur->bc_tp, ip, logflags);
		}

		return 0;
	}

	/* First, try shifting an entry to the right neighbor. */
	error = xfs_btree_rshift(cur, level, stat);
	if (error || *stat)
		return error;

	/* Next, try shifting an entry to the left neighbor. */
	error = xfs_btree_lshift(cur, level, stat);
	if (error)
		return error;

	if (*stat) {
		*oindex = *index = cur->bc_ptrs[level];
		return 0;
	}

	/*
	 * Next, try splitting the current block in half.
	 *
	 * If this works we have to re-set our variables because we
	 * could be in a different block now.
	 */
	error = xfs_btree_split(cur, level, nptr, &key, ncur, stat);
	if (error || *stat == 0)
		return error;


	*index = cur->bc_ptrs[level];
	cur->bc_ops->init_rec_from_key(&key, nrec);
	return 0;
}

/*
 * Insert one record/level.  Return information to the caller
 * allowing the next level up to proceed if necessary.
 */
STATIC int
xfs_btree_insrec(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	int			level,	/* level to insert record at */
	union xfs_btree_ptr	*ptrp,	/* i/o: block number inserted */
	union xfs_btree_rec	*recp,	/* i/o: record data inserted */
	struct xfs_btree_cur	**curp,	/* output: new cursor replacing cur */
	int			*stat)	/* success/failure */
{
	struct xfs_btree_block	*block;	/* btree block */
	struct xfs_buf		*bp;	/* buffer for block */
	union xfs_btree_key	key;	/* btree key */
	union xfs_btree_ptr	nptr;	/* new block ptr */
	struct xfs_btree_cur	*ncur;	/* new btree cursor */
	union xfs_btree_rec	nrec;	/* new record count */
	int			optr;	/* old key/record index */
	int			ptr;	/* key/record index */
	int			numrecs;/* number of records */
	int			error;	/* error return value */
#ifdef DEBUG
	int			i;
#endif

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGIPR(cur, level, *ptrp, recp);

	ncur = NULL;

	/*
	 * If we have an external root pointer, and we've made it to the
	 * root level, allocate a new root block and we're done.
	 */
	if (!(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (level >= cur->bc_nlevels)) {
		error = xfs_btree_new_root(cur, stat);
		xfs_btree_set_ptr_null(cur, ptrp);

		XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
		return error;
	}

	/* If we're off the left edge, return failure. */
	ptr = cur->bc_ptrs[level];
	if (ptr == 0) {
		XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
		*stat = 0;
		return 0;
	}

	/* Make a key out of the record data to be inserted, and save it. */
	cur->bc_ops->init_key_from_rec(&key, recp);

	optr = ptr;

	XFS_BTREE_STATS_INC(cur, insrec);

	/* Get pointers to the btree buffer and block. */
	block = xfs_btree_get_block(cur, level, &bp);
	numrecs = xfs_btree_get_numrecs(block);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;

	/* Check that the new entry is being inserted in the right place. */
	if (ptr <= numrecs) {
		if (level == 0) {
			ASSERT(cur->bc_ops->recs_inorder(cur, recp,
				xfs_btree_rec_addr(cur, ptr, block)));
		} else {
			ASSERT(cur->bc_ops->keys_inorder(cur, &key,
				xfs_btree_key_addr(cur, ptr, block)));
		}
	}
#endif

	/*
	 * If the block is full, we can't insert the new entry until we
	 * make the block un-full.
	 */
	xfs_btree_set_ptr_null(cur, &nptr);
	if (numrecs == cur->bc_ops->get_maxrecs(cur, level)) {
		error = xfs_btree_make_block_unfull(cur, level, numrecs,
					&optr, &ptr, &nptr, &ncur, &nrec, stat);
		if (error || *stat == 0)
			goto error0;
	}

	/*
	 * The current block may have changed if the block was
	 * previously full and we have just made space in it.
	 */
	block = xfs_btree_get_block(cur, level, &bp);
	numrecs = xfs_btree_get_numrecs(block);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		return error;
#endif

	/*
	 * At this point we know there's room for our new entry in the block
	 * we're pointing at.
	 */
	XFS_BTREE_STATS_ADD(cur, moves, numrecs - ptr + 1);

	if (level > 0) {
		/* It's a nonleaf. make a hole in the keys and ptrs */
		union xfs_btree_key	*kp;
		union xfs_btree_ptr	*pp;

		kp = xfs_btree_key_addr(cur, ptr, block);
		pp = xfs_btree_ptr_addr(cur, ptr, block);

#ifdef DEBUG
		for (i = numrecs - ptr; i >= 0; i--) {
			error = xfs_btree_check_ptr(cur, pp, i, level);
			if (error)
				return error;
		}
#endif

		xfs_btree_shift_keys(cur, kp, 1, numrecs - ptr + 1);
		xfs_btree_shift_ptrs(cur, pp, 1, numrecs - ptr + 1);

#ifdef DEBUG
		error = xfs_btree_check_ptr(cur, ptrp, 0, level);
		if (error)
			goto error0;
#endif

		/* Now put the new data in, bump numrecs and log it. */
		xfs_btree_copy_keys(cur, kp, &key, 1);
		xfs_btree_copy_ptrs(cur, pp, ptrp, 1);
		numrecs++;
		xfs_btree_set_numrecs(block, numrecs);
		xfs_btree_log_ptrs(cur, bp, ptr, numrecs);
		xfs_btree_log_keys(cur, bp, ptr, numrecs);
#ifdef DEBUG
		if (ptr < numrecs) {
			ASSERT(cur->bc_ops->keys_inorder(cur, kp,
				xfs_btree_key_addr(cur, ptr + 1, block)));
		}
#endif
	} else {
		/* It's a leaf. make a hole in the records */
		union xfs_btree_rec             *rp;

		rp = xfs_btree_rec_addr(cur, ptr, block);

		xfs_btree_shift_recs(cur, rp, 1, numrecs - ptr + 1);

		/* Now put the new data in, bump numrecs and log it. */
		xfs_btree_copy_recs(cur, rp, recp, 1);
		xfs_btree_set_numrecs(block, ++numrecs);
		xfs_btree_log_recs(cur, bp, ptr, numrecs);
#ifdef DEBUG
		if (ptr < numrecs) {
			ASSERT(cur->bc_ops->recs_inorder(cur, rp,
				xfs_btree_rec_addr(cur, ptr + 1, block)));
		}
#endif
	}

	/* Log the new number of records in the btree header. */
	xfs_btree_log_block(cur, bp, XFS_BB_NUMRECS);

	/* If we inserted at the start of a block, update the parents' keys. */
	if (optr == 1) {
		error = xfs_btree_updkey(cur, &key, level + 1);
		if (error)
			goto error0;
	}

	/*
	 * If we are tracking the last record in the tree and
	 * we are at the far right edge of the tree, update it.
	 */
	if (xfs_btree_is_lastrec(cur, block, level)) {
		cur->bc_ops->update_lastrec(cur, block, recp,
					    ptr, LASTREC_INSREC);
	}

	/*
	 * Return the new block number, if any.
	 * If there is one, give back a record value and a cursor too.
	 */
	*ptrp = nptr;
	if (!xfs_btree_ptr_is_null(cur, &nptr)) {
		*recp = nrec;
		*curp = ncur;
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

/*
 * Insert the record at the point referenced by cur.
 *
 * A multi-level split of the tree on insert will invalidate the original
 * cursor.  All callers of this function should assume that the cursor is
 * no longer valid and revalidate it.
 */
int
xfs_btree_insert(
	struct xfs_btree_cur	*cur,
	int			*stat)
{
	int			error;	/* error return value */
	int			i;	/* result value, 0 for failure */
	int			level;	/* current level number in btree */
	union xfs_btree_ptr	nptr;	/* new block number (split result) */
	struct xfs_btree_cur	*ncur;	/* new cursor (split result) */
	struct xfs_btree_cur	*pcur;	/* previous level's cursor */
	union xfs_btree_rec	rec;	/* record to insert */

	level = 0;
	ncur = NULL;
	pcur = cur;

	xfs_btree_set_ptr_null(cur, &nptr);
	cur->bc_ops->init_rec_from_cur(cur, &rec);

	/*
	 * Loop going up the tree, starting at the leaf level.
	 * Stop when we don't get a split block, that must mean that
	 * the insert is finished with this level.
	 */
	do {
		/*
		 * Insert nrec/nptr into this level of the tree.
		 * Note if we fail, nptr will be null.
		 */
		error = xfs_btree_insrec(pcur, level, &nptr, &rec, &ncur, &i);
		if (error) {
			if (pcur != cur)
				xfs_btree_del_cursor(pcur, XFS_BTREE_ERROR);
			goto error0;
		}

		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		level++;

		/*
		 * See if the cursor we just used is trash.
		 * Can't trash the caller's cursor, but otherwise we should
		 * if ncur is a new cursor or we're about to be done.
		 */
		if (pcur != cur &&
		    (ncur || xfs_btree_ptr_is_null(cur, &nptr))) {
			/* Save the state from the cursor before we trash it */
			if (cur->bc_ops->update_cursor)
				cur->bc_ops->update_cursor(pcur, cur);
			cur->bc_nlevels = pcur->bc_nlevels;
			xfs_btree_del_cursor(pcur, XFS_BTREE_NOERROR);
		}
		/* If we got a new cursor, switch to it. */
		if (ncur) {
			pcur = ncur;
			ncur = NULL;
		}
	} while (!xfs_btree_ptr_is_null(cur, &nptr));

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = i;
	return 0;
error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

/*
 * Try to merge a non-leaf block back into the inode root.
 *
 * Note: the killroot names comes from the fact that we're effectively
 * killing the old root block.  But because we can't just delete the
 * inode we have to copy the single block it was pointing to into the
 * inode.
 */
STATIC int
xfs_btree_kill_iroot(
	struct xfs_btree_cur	*cur)
{
	int			whichfork = cur->bc_private.b.whichfork;
	struct xfs_inode	*ip = cur->bc_private.b.ip;
	struct xfs_ifork	*ifp = XFS_IFORK_PTR(ip, whichfork);
	struct xfs_btree_block	*block;
	struct xfs_btree_block	*cblock;
	union xfs_btree_key	*kp;
	union xfs_btree_key	*ckp;
	union xfs_btree_ptr	*pp;
	union xfs_btree_ptr	*cpp;
	struct xfs_buf		*cbp;
	int			level;
	int			index;
	int			numrecs;
#ifdef DEBUG
	union xfs_btree_ptr	ptr;
	int			i;
#endif

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);

	ASSERT(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE);
	ASSERT(cur->bc_nlevels > 1);

	/*
	 * Don't deal with the root block needs to be a leaf case.
	 * We're just going to turn the thing back into extents anyway.
	 */
	level = cur->bc_nlevels - 1;
	if (level == 1)
		goto out0;

	/*
	 * Give up if the root has multiple children.
	 */
	block = xfs_btree_get_iroot(cur);
	if (xfs_btree_get_numrecs(block) != 1)
		goto out0;

	cblock = xfs_btree_get_block(cur, level - 1, &cbp);
	numrecs = xfs_btree_get_numrecs(cblock);

	/*
	 * Only do this if the next level will fit.
	 * Then the data must be copied up to the inode,
	 * instead of freeing the root you free the next level.
	 */
	if (numrecs > cur->bc_ops->get_dmaxrecs(cur, level))
		goto out0;

	XFS_BTREE_STATS_INC(cur, killroot);

#ifdef DEBUG
	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_LEFTSIB);
	ASSERT(xfs_btree_ptr_is_null(cur, &ptr));
	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_RIGHTSIB);
	ASSERT(xfs_btree_ptr_is_null(cur, &ptr));
#endif

	index = numrecs - cur->bc_ops->get_maxrecs(cur, level);
	if (index) {
		xfs_iroot_realloc(cur->bc_private.b.ip, index,
				  cur->bc_private.b.whichfork);
		block = ifp->if_broot;
	}

	be16_add_cpu(&block->bb_numrecs, index);
	ASSERT(block->bb_numrecs == cblock->bb_numrecs);

	kp = xfs_btree_key_addr(cur, 1, block);
	ckp = xfs_btree_key_addr(cur, 1, cblock);
	xfs_btree_copy_keys(cur, kp, ckp, numrecs);

	pp = xfs_btree_ptr_addr(cur, 1, block);
	cpp = xfs_btree_ptr_addr(cur, 1, cblock);
#ifdef DEBUG
	for (i = 0; i < numrecs; i++) {
		int		error;

		error = xfs_btree_check_ptr(cur, cpp, i, level - 1);
		if (error) {
			XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
			return error;
		}
	}
#endif
	xfs_btree_copy_ptrs(cur, pp, cpp, numrecs);

	cur->bc_ops->free_block(cur, cbp);
	XFS_BTREE_STATS_INC(cur, free);

	cur->bc_bufs[level - 1] = NULL;
	be16_add_cpu(&block->bb_level, -1);
	xfs_trans_log_inode(cur->bc_tp, ip,
		XFS_ILOG_CORE | xfs_ilog_fbroot(cur->bc_private.b.whichfork));
	cur->bc_nlevels--;
out0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	return 0;
}

STATIC int
xfs_btree_dec_cursor(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)
{
	int			error;
	int			i;

	if (level > 0) {
		error = xfs_btree_decrement(cur, level, &i);
		if (error)
			return error;
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = 1;
	return 0;
}

/*
 * Single level of the btree record deletion routine.
 * Delete record pointed to by cur/level.
 * Remove the record from its block then rebalance the tree.
 * Return 0 for error, 1 for done, 2 to go on to the next level.
 */
STATIC int					/* error */
xfs_btree_delrec(
	struct xfs_btree_cur	*cur,		/* btree cursor */
	int			level,		/* level removing record from */
	int			*stat)		/* fail/done/go-on */
{
	struct xfs_btree_block	*block;		/* btree block */
	union xfs_btree_ptr	cptr;		/* current block ptr */
	struct xfs_buf		*bp;		/* buffer for block */
	int			error;		/* error return value */
	int			i;		/* loop counter */
	union xfs_btree_key	key;		/* storage for keyp */
	union xfs_btree_key	*keyp = &key;	/* passed to the next level */
	union xfs_btree_ptr	lptr;		/* left sibling block ptr */
	struct xfs_buf		*lbp;		/* left buffer pointer */
	struct xfs_btree_block	*left;		/* left btree block */
	int			lrecs = 0;	/* left record count */
	int			ptr;		/* key/record index */
	union xfs_btree_ptr	rptr;		/* right sibling block ptr */
	struct xfs_buf		*rbp;		/* right buffer pointer */
	struct xfs_btree_block	*right;		/* right btree block */
	struct xfs_btree_block	*rrblock;	/* right-right btree block */
	struct xfs_buf		*rrbp;		/* right-right buffer pointer */
	int			rrecs = 0;	/* right record count */
	struct xfs_btree_cur	*tcur;		/* temporary btree cursor */
	int			numrecs;	/* temporary numrec count */

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);
	XFS_BTREE_TRACE_ARGI(cur, level);

	tcur = NULL;

	/* Get the index of the entry being deleted, check for nothing there. */
	ptr = cur->bc_ptrs[level];
	if (ptr == 0) {
		XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
		*stat = 0;
		return 0;
	}

	/* Get the buffer & block containing the record or key/ptr. */
	block = xfs_btree_get_block(cur, level, &bp);
	numrecs = xfs_btree_get_numrecs(block);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;
#endif

	/* Fail if we're off the end of the block. */
	if (ptr > numrecs) {
		XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
		*stat = 0;
		return 0;
	}

	XFS_BTREE_STATS_INC(cur, delrec);
	XFS_BTREE_STATS_ADD(cur, moves, numrecs - ptr);

	/* Excise the entries being deleted. */
	if (level > 0) {
		/* It's a nonleaf. operate on keys and ptrs */
		union xfs_btree_key	*lkp;
		union xfs_btree_ptr	*lpp;

		lkp = xfs_btree_key_addr(cur, ptr + 1, block);
		lpp = xfs_btree_ptr_addr(cur, ptr + 1, block);

#ifdef DEBUG
		for (i = 0; i < numrecs - ptr; i++) {
			error = xfs_btree_check_ptr(cur, lpp, i, level);
			if (error)
				goto error0;
		}
#endif

		if (ptr < numrecs) {
			xfs_btree_shift_keys(cur, lkp, -1, numrecs - ptr);
			xfs_btree_shift_ptrs(cur, lpp, -1, numrecs - ptr);
			xfs_btree_log_keys(cur, bp, ptr, numrecs - 1);
			xfs_btree_log_ptrs(cur, bp, ptr, numrecs - 1);
		}

		/*
		 * If it's the first record in the block, we'll need to pass a
		 * key up to the next level (updkey).
		 */
		if (ptr == 1)
			keyp = xfs_btree_key_addr(cur, 1, block);
	} else {
		/* It's a leaf. operate on records */
		if (ptr < numrecs) {
			xfs_btree_shift_recs(cur,
				xfs_btree_rec_addr(cur, ptr + 1, block),
				-1, numrecs - ptr);
			xfs_btree_log_recs(cur, bp, ptr, numrecs - 1);
		}

		/*
		 * If it's the first record in the block, we'll need a key
		 * structure to pass up to the next level (updkey).
		 */
		if (ptr == 1) {
			cur->bc_ops->init_key_from_rec(&key,
					xfs_btree_rec_addr(cur, 1, block));
			keyp = &key;
		}
	}

	/*
	 * Decrement and log the number of entries in the block.
	 */
	xfs_btree_set_numrecs(block, --numrecs);
	xfs_btree_log_block(cur, bp, XFS_BB_NUMRECS);

	/*
	 * If we are tracking the last record in the tree and
	 * we are at the far right edge of the tree, update it.
	 */
	if (xfs_btree_is_lastrec(cur, block, level)) {
		cur->bc_ops->update_lastrec(cur, block, NULL,
					    ptr, LASTREC_DELREC);
	}

	/*
	 * We're at the root level.  First, shrink the root block in-memory.
	 * Try to get rid of the next level down.  If we can't then there's
	 * nothing left to do.
	 */
	if (level == cur->bc_nlevels - 1) {
		if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) {
			xfs_iroot_realloc(cur->bc_private.b.ip, -1,
					  cur->bc_private.b.whichfork);

			error = xfs_btree_kill_iroot(cur);
			if (error)
				goto error0;

			error = xfs_btree_dec_cursor(cur, level, stat);
			if (error)
				goto error0;
			*stat = 1;
			return 0;
		}

		/*
		 * If this is the root level, and there's only one entry left,
		 * and it's NOT the leaf level, then we can get rid of this
		 * level.
		 */
		if (numrecs == 1 && level > 0) {
			union xfs_btree_ptr	*pp;
			/*
			 * pp is still set to the first pointer in the block.
			 * Make it the new root of the btree.
			 */
			pp = xfs_btree_ptr_addr(cur, 1, block);
			error = cur->bc_ops->kill_root(cur, bp, level, pp);
			if (error)
				goto error0;
		} else if (level > 0) {
			error = xfs_btree_dec_cursor(cur, level, stat);
			if (error)
				goto error0;
		}
		*stat = 1;
		return 0;
	}

	/*
	 * If we deleted the leftmost entry in the block, update the
	 * key values above us in the tree.
	 */
	if (ptr == 1) {
		error = xfs_btree_updkey(cur, keyp, level + 1);
		if (error)
			goto error0;
	}

	/*
	 * If the number of records remaining in the block is at least
	 * the minimum, we're done.
	 */
	if (numrecs >= cur->bc_ops->get_minrecs(cur, level)) {
		error = xfs_btree_dec_cursor(cur, level, stat);
		if (error)
			goto error0;
		return 0;
	}

	/*
	 * Otherwise, we have to move some records around to keep the
	 * tree balanced.  Look at the left and right sibling blocks to
	 * see if we can re-balance by moving only one record.
	 */
	xfs_btree_get_sibling(cur, block, &rptr, XFS_BB_RIGHTSIB);
	xfs_btree_get_sibling(cur, block, &lptr, XFS_BB_LEFTSIB);

	if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) {
		/*
		 * One child of root, need to get a chance to copy its contents
		 * into the root and delete it. Can't go up to next level,
		 * there's nothing to delete there.
		 */
		if (xfs_btree_ptr_is_null(cur, &rptr) &&
		    xfs_btree_ptr_is_null(cur, &lptr) &&
		    level == cur->bc_nlevels - 2) {
			error = xfs_btree_kill_iroot(cur);
			if (!error)
				error = xfs_btree_dec_cursor(cur, level, stat);
			if (error)
				goto error0;
			return 0;
		}
	}

	ASSERT(!xfs_btree_ptr_is_null(cur, &rptr) ||
	       !xfs_btree_ptr_is_null(cur, &lptr));

	/*
	 * Duplicate the cursor so our btree manipulations here won't
	 * disrupt the next level up.
	 */
	error = xfs_btree_dup_cursor(cur, &tcur);
	if (error)
		goto error0;

	/*
	 * If there's a right sibling, see if it's ok to shift an entry
	 * out of it.
	 */
	if (!xfs_btree_ptr_is_null(cur, &rptr)) {
		/*
		 * Move the temp cursor to the last entry in the next block.
		 * Actually any entry but the first would suffice.
		 */
		i = xfs_btree_lastrec(tcur, level);
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

		error = xfs_btree_increment(tcur, level, &i);
		if (error)
			goto error0;
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

		i = xfs_btree_lastrec(tcur, level);
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

		/* Grab a pointer to the block. */
		right = xfs_btree_get_block(tcur, level, &rbp);
#ifdef DEBUG
		error = xfs_btree_check_block(tcur, right, level, rbp);
		if (error)
			goto error0;
#endif
		/* Grab the current block number, for future use. */
		xfs_btree_get_sibling(tcur, right, &cptr, XFS_BB_LEFTSIB);

		/*
		 * If right block is full enough so that removing one entry
		 * won't make it too empty, and left-shifting an entry out
		 * of right to us works, we're done.
		 */
		if (xfs_btree_get_numrecs(right) - 1 >=
		    cur->bc_ops->get_minrecs(tcur, level)) {
			error = xfs_btree_lshift(tcur, level, &i);
			if (error)
				goto error0;
			if (i) {
				ASSERT(xfs_btree_get_numrecs(block) >=
				       cur->bc_ops->get_minrecs(tcur, level));

				xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
				tcur = NULL;

				error = xfs_btree_dec_cursor(cur, level, stat);
				if (error)
					goto error0;
				return 0;
			}
		}

		/*
		 * Otherwise, grab the number of records in right for
		 * future reference, and fix up the temp cursor to point
		 * to our block again (last record).
		 */
		rrecs = xfs_btree_get_numrecs(right);
		if (!xfs_btree_ptr_is_null(cur, &lptr)) {
			i = xfs_btree_firstrec(tcur, level);
			XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

			error = xfs_btree_decrement(tcur, level, &i);
			if (error)
				goto error0;
			XFS_WANT_CORRUPTED_GOTO(i == 1, error0);
		}
	}

	/*
	 * If there's a left sibling, see if it's ok to shift an entry
	 * out of it.
	 */
	if (!xfs_btree_ptr_is_null(cur, &lptr)) {
		/*
		 * Move the temp cursor to the first entry in the
		 * previous block.
		 */
		i = xfs_btree_firstrec(tcur, level);
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

		error = xfs_btree_decrement(tcur, level, &i);
		if (error)
			goto error0;
		i = xfs_btree_firstrec(tcur, level);
		XFS_WANT_CORRUPTED_GOTO(i == 1, error0);

		/* Grab a pointer to the block. */
		left = xfs_btree_get_block(tcur, level, &lbp);
#ifdef DEBUG
		error = xfs_btree_check_block(cur, left, level, lbp);
		if (error)
			goto error0;
#endif
		/* Grab the current block number, for future use. */
		xfs_btree_get_sibling(tcur, left, &cptr, XFS_BB_RIGHTSIB);

		/*
		 * If left block is full enough so that removing one entry
		 * won't make it too empty, and right-shifting an entry out
		 * of left to us works, we're done.
		 */
		if (xfs_btree_get_numrecs(left) - 1 >=
		    cur->bc_ops->get_minrecs(tcur, level)) {
			error = xfs_btree_rshift(tcur, level, &i);
			if (error)
				goto error0;
			if (i) {
				ASSERT(xfs_btree_get_numrecs(block) >=
				       cur->bc_ops->get_minrecs(tcur, level));
				xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
				tcur = NULL;
				if (level == 0)
					cur->bc_ptrs[0]++;
				XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
				*stat = 1;
				return 0;
			}
		}

		/*
		 * Otherwise, grab the number of records in right for
		 * future reference.
		 */
		lrecs = xfs_btree_get_numrecs(left);
	}

	/* Delete the temp cursor, we're done with it. */
	xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
	tcur = NULL;

	/* If here, we need to do a join to keep the tree balanced. */
	ASSERT(!xfs_btree_ptr_is_null(cur, &cptr));

	if (!xfs_btree_ptr_is_null(cur, &lptr) &&
	    lrecs + xfs_btree_get_numrecs(block) <=
			cur->bc_ops->get_maxrecs(cur, level)) {
		/*
		 * Set "right" to be the starting block,
		 * "left" to be the left neighbor.
		 */
		rptr = cptr;
		right = block;
		rbp = bp;
		error = xfs_btree_read_buf_block(cur, &lptr, level,
							0, &left, &lbp);
		if (error)
			goto error0;

	/*
	 * If that won't work, see if we can join with the right neighbor block.
	 */
	} else if (!xfs_btree_ptr_is_null(cur, &rptr) &&
		   rrecs + xfs_btree_get_numrecs(block) <=
			cur->bc_ops->get_maxrecs(cur, level)) {
		/*
		 * Set "left" to be the starting block,
		 * "right" to be the right neighbor.
		 */
		lptr = cptr;
		left = block;
		lbp = bp;
		error = xfs_btree_read_buf_block(cur, &rptr, level,
							0, &right, &rbp);
		if (error)
			goto error0;

	/*
	 * Otherwise, we can't fix the imbalance.
	 * Just return.  This is probably a logic error, but it's not fatal.
	 */
	} else {
		error = xfs_btree_dec_cursor(cur, level, stat);
		if (error)
			goto error0;
		return 0;
	}

	rrecs = xfs_btree_get_numrecs(right);
	lrecs = xfs_btree_get_numrecs(left);

	/*
	 * We're now going to join "left" and "right" by moving all the stuff
	 * in "right" to "left" and deleting "right".
	 */
	XFS_BTREE_STATS_ADD(cur, moves, rrecs);
	if (level > 0) {
		/* It's a non-leaf.  Move keys and pointers. */
		union xfs_btree_key	*lkp;	/* left btree key */
		union xfs_btree_ptr	*lpp;	/* left address pointer */
		union xfs_btree_key	*rkp;	/* right btree key */
		union xfs_btree_ptr	*rpp;	/* right address pointer */

		lkp = xfs_btree_key_addr(cur, lrecs + 1, left);
		lpp = xfs_btree_ptr_addr(cur, lrecs + 1, left);
		rkp = xfs_btree_key_addr(cur, 1, right);
		rpp = xfs_btree_ptr_addr(cur, 1, right);
#ifdef DEBUG
		for (i = 1; i < rrecs; i++) {
			error = xfs_btree_check_ptr(cur, rpp, i, level);
			if (error)
				goto error0;
		}
#endif
		xfs_btree_copy_keys(cur, lkp, rkp, rrecs);
		xfs_btree_copy_ptrs(cur, lpp, rpp, rrecs);

		xfs_btree_log_keys(cur, lbp, lrecs + 1, lrecs + rrecs);
		xfs_btree_log_ptrs(cur, lbp, lrecs + 1, lrecs + rrecs);
	} else {
		/* It's a leaf.  Move records.  */
		union xfs_btree_rec	*lrp;	/* left record pointer */
		union xfs_btree_rec	*rrp;	/* right record pointer */

		lrp = xfs_btree_rec_addr(cur, lrecs + 1, left);
		rrp = xfs_btree_rec_addr(cur, 1, right);

		xfs_btree_copy_recs(cur, lrp, rrp, rrecs);
		xfs_btree_log_recs(cur, lbp, lrecs + 1, lrecs + rrecs);
	}

	XFS_BTREE_STATS_INC(cur, join);

	/*
	 * Fix up the number of records and right block pointer in the
	 * surviving block, and log it.
	 */
	xfs_btree_set_numrecs(left, lrecs + rrecs);
	xfs_btree_get_sibling(cur, right, &cptr, XFS_BB_RIGHTSIB),
	xfs_btree_set_sibling(cur, left, &cptr, XFS_BB_RIGHTSIB);
	xfs_btree_log_block(cur, lbp, XFS_BB_NUMRECS | XFS_BB_RIGHTSIB);

	/* If there is a right sibling, point it to the remaining block. */
	xfs_btree_get_sibling(cur, left, &cptr, XFS_BB_RIGHTSIB);
	if (!xfs_btree_ptr_is_null(cur, &cptr)) {
		error = xfs_btree_read_buf_block(cur, &cptr, level,
							0, &rrblock, &rrbp);
		if (error)
			goto error0;
		xfs_btree_set_sibling(cur, rrblock, &lptr, XFS_BB_LEFTSIB);
		xfs_btree_log_block(cur, rrbp, XFS_BB_LEFTSIB);
	}

	/* Free the deleted block. */
	error = cur->bc_ops->free_block(cur, rbp);
	if (error)
		goto error0;
	XFS_BTREE_STATS_INC(cur, free);

	/*
	 * If we joined with the left neighbor, set the buffer in the
	 * cursor to the left block, and fix up the index.
	 */
	if (bp != lbp) {
		cur->bc_bufs[level] = lbp;
		cur->bc_ptrs[level] += lrecs;
		cur->bc_ra[level] = 0;
	}
	/*
	 * If we joined with the right neighbor and there's a level above
	 * us, increment the cursor at that level.
	 */
	else if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) ||
		   (level + 1 < cur->bc_nlevels)) {
		error = xfs_btree_increment(cur, level + 1, &i);
		if (error)
			goto error0;
	}

	/*
	 * Readjust the ptr at this level if it's not a leaf, since it's
	 * still pointing at the deletion point, which makes the cursor
	 * inconsistent.  If this makes the ptr 0, the caller fixes it up.
	 * We can't use decrement because it would change the next level up.
	 */
	if (level > 0)
		cur->bc_ptrs[level]--;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	/* Return value means the next level up has something to do. */
	*stat = 2;
	return 0;

error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	if (tcur)
		xfs_btree_del_cursor(tcur, XFS_BTREE_ERROR);
	return error;
}

/*
 * Delete the record pointed to by cur.
 * The cursor refers to the place where the record was (could be inserted)
 * when the operation returns.
 */
int					/* error */
xfs_btree_delete(
	struct xfs_btree_cur	*cur,
	int			*stat)	/* success/failure */
{
	int			error;	/* error return value */
	int			level;
	int			i;

	XFS_BTREE_TRACE_CURSOR(cur, XBT_ENTRY);

	/*
	 * Go up the tree, starting at leaf level.
	 *
	 * If 2 is returned then a join was done; go to the next level.
	 * Otherwise we are done.
	 */
	for (level = 0, i = 2; i == 2; level++) {
		error = xfs_btree_delrec(cur, level, &i);
		if (error)
			goto error0;
	}

	if (i == 0) {
		for (level = 1; level < cur->bc_nlevels; level++) {
			if (cur->bc_ptrs[level] == 0) {
				error = xfs_btree_decrement(cur, level, &i);
				if (error)
					goto error0;
				break;
			}
		}
	}

	XFS_BTREE_TRACE_CURSOR(cur, XBT_EXIT);
	*stat = i;
	return 0;
error0:
	XFS_BTREE_TRACE_CURSOR(cur, XBT_ERROR);
	return error;
}

/*
 * Get the data from the pointed-to record.
 */
int					/* error */
xfs_btree_get_rec(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	union xfs_btree_rec	**recp,	/* output: btree record */
	int			*stat)	/* output: success/failure */
{
	struct xfs_btree_block	*block;	/* btree block */
	struct xfs_buf		*bp;	/* buffer pointer */
	int			ptr;	/* record number */
#ifdef DEBUG
	int			error;	/* error return value */
#endif

	ptr = cur->bc_ptrs[0];
	block = xfs_btree_get_block(cur, 0, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, 0, bp);
	if (error)
		return error;
#endif

	/*
	 * Off the right end or left end, return failure.
	 */
	if (ptr > xfs_btree_get_numrecs(block) || ptr <= 0) {
		*stat = 0;
		return 0;
	}

	/*
	 * Point to the record and extract its data.
	 */
	*recp = xfs_btree_rec_addr(cur, ptr, block);
	*stat = 1;
	return 0;
}
