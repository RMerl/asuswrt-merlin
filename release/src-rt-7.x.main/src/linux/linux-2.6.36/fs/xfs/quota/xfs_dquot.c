/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
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
#include "xfs_bit.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_alloc.h"
#include "xfs_quota.h"
#include "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "xfs_inode.h"
#include "xfs_bmap.h"
#include "xfs_rtalloc.h"
#include "xfs_error.h"
#include "xfs_itable.h"
#include "xfs_attr.h"
#include "xfs_buf_item.h"
#include "xfs_trans_space.h"
#include "xfs_trans_priv.h"
#include "xfs_qm.h"
#include "xfs_trace.h"


/*
   LOCK ORDER

   inode lock		    (ilock)
   dquot hash-chain lock    (hashlock)
   xqm dquot freelist lock  (freelistlock
   mount's dquot list lock  (mplistlock)
   user dquot lock - lock ordering among dquots is based on the uid or gid
   group dquot lock - similar to udquots. Between the two dquots, the udquot
		      has to be locked first.
   pin lock - the dquot lock must be held to take this lock.
   flush lock - ditto.
*/

#ifdef DEBUG
xfs_buftarg_t *xfs_dqerror_target;
int xfs_do_dqerror;
int xfs_dqreq_num;
int xfs_dqerror_mod = 33;
#endif

static struct lock_class_key xfs_dquot_other_class;

/*
 * Allocate and initialize a dquot. We don't always allocate fresh memory;
 * we try to reclaim a free dquot if the number of incore dquots are above
 * a threshold.
 * The only field inside the core that gets initialized at this point
 * is the d_id field. The idea is to fill in the entire q_core
 * when we read in the on disk dquot.
 */
STATIC xfs_dquot_t *
xfs_qm_dqinit(
	xfs_mount_t  *mp,
	xfs_dqid_t   id,
	uint	     type)
{
	xfs_dquot_t	*dqp;
	boolean_t	brandnewdquot;

	brandnewdquot = xfs_qm_dqalloc_incore(&dqp);
	dqp->dq_flags = type;
	dqp->q_core.d_id = cpu_to_be32(id);
	dqp->q_mount = mp;

	/*
	 * No need to re-initialize these if this is a reclaimed dquot.
	 */
	if (brandnewdquot) {
		INIT_LIST_HEAD(&dqp->q_freelist);
		mutex_init(&dqp->q_qlock);
		init_waitqueue_head(&dqp->q_pinwait);

		/*
		 * Because we want to use a counting completion, complete
		 * the flush completion once to allow a single access to
		 * the flush completion without blocking.
		 */
		init_completion(&dqp->q_flush);
		complete(&dqp->q_flush);

		trace_xfs_dqinit(dqp);
	} else {
		/*
		 * Only the q_core portion was zeroed in dqreclaim_one().
		 * So, we need to reset others.
		 */
		dqp->q_nrefs = 0;
		dqp->q_blkno = 0;
		INIT_LIST_HEAD(&dqp->q_mplist);
		INIT_LIST_HEAD(&dqp->q_hashlist);
		dqp->q_bufoffset = 0;
		dqp->q_fileoffset = 0;
		dqp->q_transp = NULL;
		dqp->q_gdquot = NULL;
		dqp->q_res_bcount = 0;
		dqp->q_res_icount = 0;
		dqp->q_res_rtbcount = 0;
		atomic_set(&dqp->q_pincount, 0);
		dqp->q_hash = NULL;
		ASSERT(list_empty(&dqp->q_freelist));

		trace_xfs_dqreuse(dqp);
	}

	/*
	 * In either case we need to make sure group quotas have a different
	 * lock class than user quotas, to make sure lockdep knows we can
	 * locks of one of each at the same time.
	 */
	if (!(type & XFS_DQ_USER))
		lockdep_set_class(&dqp->q_qlock, &xfs_dquot_other_class);

	/*
	 * log item gets initialized later
	 */
	return (dqp);
}

/*
 * This is called to free all the memory associated with a dquot
 */
void
xfs_qm_dqdestroy(
	xfs_dquot_t	*dqp)
{
	ASSERT(list_empty(&dqp->q_freelist));

	mutex_destroy(&dqp->q_qlock);
	sv_destroy(&dqp->q_pinwait);
	kmem_zone_free(xfs_Gqm->qm_dqzone, dqp);

	atomic_dec(&xfs_Gqm->qm_totaldquots);
}

/*
 * This is what a 'fresh' dquot inside a dquot chunk looks like on disk.
 */
STATIC void
xfs_qm_dqinit_core(
	xfs_dqid_t	id,
	uint		type,
	xfs_dqblk_t	*d)
{
	/*
	 * Caller has zero'd the entire dquot 'chunk' already.
	 */
	d->dd_diskdq.d_magic = cpu_to_be16(XFS_DQUOT_MAGIC);
	d->dd_diskdq.d_version = XFS_DQUOT_VERSION;
	d->dd_diskdq.d_id = cpu_to_be32(id);
	d->dd_diskdq.d_flags = type;
}

/*
 * If default limits are in force, push them into the dquot now.
 * We overwrite the dquot limits only if they are zero and this
 * is not the root dquot.
 */
void
xfs_qm_adjust_dqlimits(
	xfs_mount_t		*mp,
	xfs_disk_dquot_t	*d)
{
	xfs_quotainfo_t		*q = mp->m_quotainfo;

	ASSERT(d->d_id);

	if (q->qi_bsoftlimit && !d->d_blk_softlimit)
		d->d_blk_softlimit = cpu_to_be64(q->qi_bsoftlimit);
	if (q->qi_bhardlimit && !d->d_blk_hardlimit)
		d->d_blk_hardlimit = cpu_to_be64(q->qi_bhardlimit);
	if (q->qi_isoftlimit && !d->d_ino_softlimit)
		d->d_ino_softlimit = cpu_to_be64(q->qi_isoftlimit);
	if (q->qi_ihardlimit && !d->d_ino_hardlimit)
		d->d_ino_hardlimit = cpu_to_be64(q->qi_ihardlimit);
	if (q->qi_rtbsoftlimit && !d->d_rtb_softlimit)
		d->d_rtb_softlimit = cpu_to_be64(q->qi_rtbsoftlimit);
	if (q->qi_rtbhardlimit && !d->d_rtb_hardlimit)
		d->d_rtb_hardlimit = cpu_to_be64(q->qi_rtbhardlimit);
}

/*
 * Check the limits and timers of a dquot and start or reset timers
 * if necessary.
 * This gets called even when quota enforcement is OFF, which makes our
 * life a little less complicated. (We just don't reject any quota
 * reservations in that case, when enforcement is off).
 * We also return 0 as the values of the timers in Q_GETQUOTA calls, when
 * enforcement's off.
 * In contrast, warnings are a little different in that they don't
 * 'automatically' get started when limits get exceeded.  They do
 * get reset to zero, however, when we find the count to be under
 * the soft limit (they are only ever set non-zero via userspace).
 */
void
xfs_qm_adjust_dqtimers(
	xfs_mount_t		*mp,
	xfs_disk_dquot_t	*d)
{
	ASSERT(d->d_id);

#ifdef QUOTADEBUG
	if (d->d_blk_hardlimit)
		ASSERT(be64_to_cpu(d->d_blk_softlimit) <=
		       be64_to_cpu(d->d_blk_hardlimit));
	if (d->d_ino_hardlimit)
		ASSERT(be64_to_cpu(d->d_ino_softlimit) <=
		       be64_to_cpu(d->d_ino_hardlimit));
	if (d->d_rtb_hardlimit)
		ASSERT(be64_to_cpu(d->d_rtb_softlimit) <=
		       be64_to_cpu(d->d_rtb_hardlimit));
#endif
	if (!d->d_btimer) {
		if ((d->d_blk_softlimit &&
		     (be64_to_cpu(d->d_bcount) >=
		      be64_to_cpu(d->d_blk_softlimit))) ||
		    (d->d_blk_hardlimit &&
		     (be64_to_cpu(d->d_bcount) >=
		      be64_to_cpu(d->d_blk_hardlimit)))) {
			d->d_btimer = cpu_to_be32(get_seconds() +
					mp->m_quotainfo->qi_btimelimit);
		} else {
			d->d_bwarns = 0;
		}
	} else {
		if ((!d->d_blk_softlimit ||
		     (be64_to_cpu(d->d_bcount) <
		      be64_to_cpu(d->d_blk_softlimit))) &&
		    (!d->d_blk_hardlimit ||
		    (be64_to_cpu(d->d_bcount) <
		     be64_to_cpu(d->d_blk_hardlimit)))) {
			d->d_btimer = 0;
		}
	}

	if (!d->d_itimer) {
		if ((d->d_ino_softlimit &&
		     (be64_to_cpu(d->d_icount) >=
		      be64_to_cpu(d->d_ino_softlimit))) ||
		    (d->d_ino_hardlimit &&
		     (be64_to_cpu(d->d_icount) >=
		      be64_to_cpu(d->d_ino_hardlimit)))) {
			d->d_itimer = cpu_to_be32(get_seconds() +
					mp->m_quotainfo->qi_itimelimit);
		} else {
			d->d_iwarns = 0;
		}
	} else {
		if ((!d->d_ino_softlimit ||
		     (be64_to_cpu(d->d_icount) <
		      be64_to_cpu(d->d_ino_softlimit)))  &&
		    (!d->d_ino_hardlimit ||
		     (be64_to_cpu(d->d_icount) <
		      be64_to_cpu(d->d_ino_hardlimit)))) {
			d->d_itimer = 0;
		}
	}

	if (!d->d_rtbtimer) {
		if ((d->d_rtb_softlimit &&
		     (be64_to_cpu(d->d_rtbcount) >=
		      be64_to_cpu(d->d_rtb_softlimit))) ||
		    (d->d_rtb_hardlimit &&
		     (be64_to_cpu(d->d_rtbcount) >=
		      be64_to_cpu(d->d_rtb_hardlimit)))) {
			d->d_rtbtimer = cpu_to_be32(get_seconds() +
					mp->m_quotainfo->qi_rtbtimelimit);
		} else {
			d->d_rtbwarns = 0;
		}
	} else {
		if ((!d->d_rtb_softlimit ||
		     (be64_to_cpu(d->d_rtbcount) <
		      be64_to_cpu(d->d_rtb_softlimit))) &&
		    (!d->d_rtb_hardlimit ||
		     (be64_to_cpu(d->d_rtbcount) <
		      be64_to_cpu(d->d_rtb_hardlimit)))) {
			d->d_rtbtimer = 0;
		}
	}
}

/*
 * initialize a buffer full of dquots and log the whole thing
 */
STATIC void
xfs_qm_init_dquot_blk(
	xfs_trans_t	*tp,
	xfs_mount_t	*mp,
	xfs_dqid_t	id,
	uint		type,
	xfs_buf_t	*bp)
{
	struct xfs_quotainfo	*q = mp->m_quotainfo;
	xfs_dqblk_t	*d;
	int		curid, i;

	ASSERT(tp);
	ASSERT(XFS_BUF_ISBUSY(bp));
	ASSERT(XFS_BUF_VALUSEMA(bp) <= 0);

	d = (xfs_dqblk_t *)XFS_BUF_PTR(bp);

	/*
	 * ID of the first dquot in the block - id's are zero based.
	 */
	curid = id - (id % q->qi_dqperchunk);
	ASSERT(curid >= 0);
	memset(d, 0, BBTOB(q->qi_dqchunklen));
	for (i = 0; i < q->qi_dqperchunk; i++, d++, curid++)
		xfs_qm_dqinit_core(curid, type, d);
	xfs_trans_dquot_buf(tp, bp,
			    (type & XFS_DQ_USER ? XFS_BLF_UDQUOT_BUF :
			    ((type & XFS_DQ_PROJ) ? XFS_BLF_PDQUOT_BUF :
			     XFS_BLF_GDQUOT_BUF)));
	xfs_trans_log_buf(tp, bp, 0, BBTOB(q->qi_dqchunklen) - 1);
}



/*
 * Allocate a block and fill it with dquots.
 * This is called when the bmapi finds a hole.
 */
STATIC int
xfs_qm_dqalloc(
	xfs_trans_t	**tpp,
	xfs_mount_t	*mp,
	xfs_dquot_t	*dqp,
	xfs_inode_t	*quotip,
	xfs_fileoff_t	offset_fsb,
	xfs_buf_t	**O_bpp)
{
	xfs_fsblock_t	firstblock;
	xfs_bmap_free_t flist;
	xfs_bmbt_irec_t map;
	int		nmaps, error, committed;
	xfs_buf_t	*bp;
	xfs_trans_t	*tp = *tpp;

	ASSERT(tp != NULL);

	trace_xfs_dqalloc(dqp);

	/*
	 * Initialize the bmap freelist prior to calling bmapi code.
	 */
	xfs_bmap_init(&flist, &firstblock);
	xfs_ilock(quotip, XFS_ILOCK_EXCL);
	/*
	 * Return if this type of quotas is turned off while we didn't
	 * have an inode lock
	 */
	if (XFS_IS_THIS_QUOTA_OFF(dqp)) {
		xfs_iunlock(quotip, XFS_ILOCK_EXCL);
		return (ESRCH);
	}

	xfs_trans_ijoin_ref(tp, quotip, XFS_ILOCK_EXCL);
	nmaps = 1;
	if ((error = xfs_bmapi(tp, quotip,
			      offset_fsb, XFS_DQUOT_CLUSTER_SIZE_FSB,
			      XFS_BMAPI_METADATA | XFS_BMAPI_WRITE,
			      &firstblock,
			      XFS_QM_DQALLOC_SPACE_RES(mp),
			      &map, &nmaps, &flist))) {
		goto error0;
	}
	ASSERT(map.br_blockcount == XFS_DQUOT_CLUSTER_SIZE_FSB);
	ASSERT(nmaps == 1);
	ASSERT((map.br_startblock != DELAYSTARTBLOCK) &&
	       (map.br_startblock != HOLESTARTBLOCK));

	/*
	 * Keep track of the blkno to save a lookup later
	 */
	dqp->q_blkno = XFS_FSB_TO_DADDR(mp, map.br_startblock);

	/* now we can just get the buffer (there's nothing to read yet) */
	bp = xfs_trans_get_buf(tp, mp->m_ddev_targp,
			       dqp->q_blkno,
			       mp->m_quotainfo->qi_dqchunklen,
			       0);
	if (!bp || (error = XFS_BUF_GETERROR(bp)))
		goto error1;
	/*
	 * Make a chunk of dquots out of this buffer and log
	 * the entire thing.
	 */
	xfs_qm_init_dquot_blk(tp, mp, be32_to_cpu(dqp->q_core.d_id),
			      dqp->dq_flags & XFS_DQ_ALLTYPES, bp);

	/*
	 * xfs_bmap_finish() may commit the current transaction and
	 * start a second transaction if the freelist is not empty.
	 *
	 * Since we still want to modify this buffer, we need to
	 * ensure that the buffer is not released on commit of
	 * the first transaction and ensure the buffer is added to the
	 * second transaction.
	 *
	 * If there is only one transaction then don't stop the buffer
	 * from being released when it commits later on.
	 */

	xfs_trans_bhold(tp, bp);

	if ((error = xfs_bmap_finish(tpp, &flist, &committed))) {
		goto error1;
	}

	if (committed) {
		tp = *tpp;
		xfs_trans_bjoin(tp, bp);
	} else {
		xfs_trans_bhold_release(tp, bp);
	}

	*O_bpp = bp;
	return 0;

      error1:
	xfs_bmap_cancel(&flist);
      error0:
	xfs_iunlock(quotip, XFS_ILOCK_EXCL);

	return (error);
}

/*
 * Maps a dquot to the buffer containing its on-disk version.
 * This returns a ptr to the buffer containing the on-disk dquot
 * in the bpp param, and a ptr to the on-disk dquot within that buffer
 */
STATIC int
xfs_qm_dqtobp(
	xfs_trans_t		**tpp,
	xfs_dquot_t		*dqp,
	xfs_disk_dquot_t	**O_ddpp,
	xfs_buf_t		**O_bpp,
	uint			flags)
{
	xfs_bmbt_irec_t map;
	int		nmaps, error;
	xfs_buf_t	*bp;
	xfs_inode_t	*quotip;
	xfs_mount_t	*mp;
	xfs_disk_dquot_t *ddq;
	xfs_dqid_t	id;
	boolean_t	newdquot;
	xfs_trans_t	*tp = (tpp ? *tpp : NULL);

	mp = dqp->q_mount;
	id = be32_to_cpu(dqp->q_core.d_id);
	nmaps = 1;
	newdquot = B_FALSE;

	/*
	 * If we don't know where the dquot lives, find out.
	 */
	if (dqp->q_blkno == (xfs_daddr_t) 0) {
		/* We use the id as an index */
		dqp->q_fileoffset = (xfs_fileoff_t)id /
					mp->m_quotainfo->qi_dqperchunk;
		nmaps = 1;
		quotip = XFS_DQ_TO_QIP(dqp);
		xfs_ilock(quotip, XFS_ILOCK_SHARED);
		/*
		 * Return if this type of quotas is turned off while we didn't
		 * have an inode lock
		 */
		if (XFS_IS_THIS_QUOTA_OFF(dqp)) {
			xfs_iunlock(quotip, XFS_ILOCK_SHARED);
			return (ESRCH);
		}
		/*
		 * Find the block map; no allocations yet
		 */
		error = xfs_bmapi(NULL, quotip, dqp->q_fileoffset,
				  XFS_DQUOT_CLUSTER_SIZE_FSB,
				  XFS_BMAPI_METADATA,
				  NULL, 0, &map, &nmaps, NULL);

		xfs_iunlock(quotip, XFS_ILOCK_SHARED);
		if (error)
			return (error);
		ASSERT(nmaps == 1);
		ASSERT(map.br_blockcount == 1);

		/*
		 * offset of dquot in the (fixed sized) dquot chunk.
		 */
		dqp->q_bufoffset = (id % mp->m_quotainfo->qi_dqperchunk) *
			sizeof(xfs_dqblk_t);
		if (map.br_startblock == HOLESTARTBLOCK) {
			/*
			 * We don't allocate unless we're asked to
			 */
			if (!(flags & XFS_QMOPT_DQALLOC))
				return (ENOENT);

			ASSERT(tp);
			if ((error = xfs_qm_dqalloc(tpp, mp, dqp, quotip,
						dqp->q_fileoffset, &bp)))
				return (error);
			tp = *tpp;
			newdquot = B_TRUE;
		} else {
			/*
			 * store the blkno etc so that we don't have to do the
			 * mapping all the time
			 */
			dqp->q_blkno = XFS_FSB_TO_DADDR(mp, map.br_startblock);
		}
	}
	ASSERT(dqp->q_blkno != DELAYSTARTBLOCK);
	ASSERT(dqp->q_blkno != HOLESTARTBLOCK);

	/*
	 * Read in the buffer, unless we've just done the allocation
	 * (in which case we already have the buf).
	 */
	if (!newdquot) {
		trace_xfs_dqtobp_read(dqp);

		error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp,
					   dqp->q_blkno,
					   mp->m_quotainfo->qi_dqchunklen,
					   0, &bp);
		if (error || !bp)
			return XFS_ERROR(error);
	}
	ASSERT(XFS_BUF_ISBUSY(bp));
	ASSERT(XFS_BUF_VALUSEMA(bp) <= 0);

	/*
	 * calculate the location of the dquot inside the buffer.
	 */
	ddq = (xfs_disk_dquot_t *)((char *)XFS_BUF_PTR(bp) + dqp->q_bufoffset);

	/*
	 * A simple sanity check in case we got a corrupted dquot...
	 */
	if (xfs_qm_dqcheck(ddq, id, dqp->dq_flags & XFS_DQ_ALLTYPES,
			   flags & (XFS_QMOPT_DQREPAIR|XFS_QMOPT_DOWARN),
			   "dqtobp")) {
		if (!(flags & XFS_QMOPT_DQREPAIR)) {
			xfs_trans_brelse(tp, bp);
			return XFS_ERROR(EIO);
		}
		XFS_BUF_BUSY(bp); /* We dirtied this */
	}

	*O_bpp = bp;
	*O_ddpp = ddq;

	return (0);
}


/*
 * Read in the ondisk dquot using dqtobp() then copy it to an incore version,
 * and release the buffer immediately.
 *
 */
/* ARGSUSED */
STATIC int
xfs_qm_dqread(
	xfs_trans_t	**tpp,
	xfs_dqid_t	id,
	xfs_dquot_t	*dqp,	/* dquot to get filled in */
	uint		flags)
{
	xfs_disk_dquot_t *ddqp;
	xfs_buf_t	 *bp;
	int		 error;
	xfs_trans_t	 *tp;

	ASSERT(tpp);

	trace_xfs_dqread(dqp);

	/*
	 * get a pointer to the on-disk dquot and the buffer containing it
	 * dqp already knows its own type (GROUP/USER).
	 */
	if ((error = xfs_qm_dqtobp(tpp, dqp, &ddqp, &bp, flags))) {
		return (error);
	}
	tp = *tpp;

	/* copy everything from disk dquot to the incore dquot */
	memcpy(&dqp->q_core, ddqp, sizeof(xfs_disk_dquot_t));
	ASSERT(be32_to_cpu(dqp->q_core.d_id) == id);
	xfs_qm_dquot_logitem_init(dqp);

	/*
	 * Reservation counters are defined as reservation plus current usage
	 * to avoid having to add everytime.
	 */
	dqp->q_res_bcount = be64_to_cpu(ddqp->d_bcount);
	dqp->q_res_icount = be64_to_cpu(ddqp->d_icount);
	dqp->q_res_rtbcount = be64_to_cpu(ddqp->d_rtbcount);

	/* Mark the buf so that this will stay incore a little longer */
	XFS_BUF_SET_VTYPE_REF(bp, B_FS_DQUOT, XFS_DQUOT_REF);

	/*
	 * We got the buffer with a xfs_trans_read_buf() (in dqtobp())
	 * So we need to release with xfs_trans_brelse().
	 * The strategy here is identical to that of inodes; we lock
	 * the dquot in xfs_qm_dqget() before making it accessible to
	 * others. This is because dquots, like inodes, need a good level of
	 * concurrency, and we don't want to take locks on the entire buffers
	 * for dquot accesses.
	 * Note also that the dquot buffer may even be dirty at this point, if
	 * this particular dquot was repaired. We still aren't afraid to
	 * brelse it because we have the changes incore.
	 */
	ASSERT(XFS_BUF_ISBUSY(bp));
	ASSERT(XFS_BUF_VALUSEMA(bp) <= 0);
	xfs_trans_brelse(tp, bp);

	return (error);
}


/*
 * allocate an incore dquot from the kernel heap,
 * and fill its core with quota information kept on disk.
 * If XFS_QMOPT_DQALLOC is set, it'll allocate a dquot on disk
 * if it wasn't already allocated.
 */
STATIC int
xfs_qm_idtodq(
	xfs_mount_t	*mp,
	xfs_dqid_t	id,	 /* gid or uid, depending on type */
	uint		type,	 /* UDQUOT or GDQUOT */
	uint		flags,	 /* DQALLOC, DQREPAIR */
	xfs_dquot_t	**O_dqpp)/* OUT : incore dquot, not locked */
{
	xfs_dquot_t	*dqp;
	int		error;
	xfs_trans_t	*tp;
	int		cancelflags=0;

	dqp = xfs_qm_dqinit(mp, id, type);
	tp = NULL;
	if (flags & XFS_QMOPT_DQALLOC) {
		tp = xfs_trans_alloc(mp, XFS_TRANS_QM_DQALLOC);
		error = xfs_trans_reserve(tp, XFS_QM_DQALLOC_SPACE_RES(mp),
				XFS_WRITE_LOG_RES(mp) +
				BBTOB(mp->m_quotainfo->qi_dqchunklen) - 1 +
				128,
				0,
				XFS_TRANS_PERM_LOG_RES,
				XFS_WRITE_LOG_COUNT);
		if (error) {
			cancelflags = 0;
			goto error0;
		}
		cancelflags = XFS_TRANS_RELEASE_LOG_RES;
	}

	/*
	 * Read it from disk; xfs_dqread() takes care of
	 * all the necessary initialization of dquot's fields (locks, etc)
	 */
	if ((error = xfs_qm_dqread(&tp, id, dqp, flags))) {
		/*
		 * This can happen if quotas got turned off (ESRCH),
		 * or if the dquot didn't exist on disk and we ask to
		 * allocate (ENOENT).
		 */
		trace_xfs_dqread_fail(dqp);
		cancelflags |= XFS_TRANS_ABORT;
		goto error0;
	}
	if (tp) {
		if ((error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES)))
			goto error1;
	}

	*O_dqpp = dqp;
	return (0);

 error0:
	ASSERT(error);
	if (tp)
		xfs_trans_cancel(tp, cancelflags);
 error1:
	xfs_qm_dqdestroy(dqp);
	*O_dqpp = NULL;
	return (error);
}

/*
 * Lookup a dquot in the incore dquot hashtable. We keep two separate
 * hashtables for user and group dquots; and, these are global tables
 * inside the XQM, not per-filesystem tables.
 * The hash chain must be locked by caller, and it is left locked
 * on return. Returning dquot is locked.
 */
STATIC int
xfs_qm_dqlookup(
	xfs_mount_t		*mp,
	xfs_dqid_t		id,
	xfs_dqhash_t		*qh,
	xfs_dquot_t		**O_dqpp)
{
	xfs_dquot_t		*dqp;
	uint			flist_locked;

	ASSERT(mutex_is_locked(&qh->qh_lock));

	flist_locked = B_FALSE;

	/*
	 * Traverse the hashchain looking for a match
	 */
	list_for_each_entry(dqp, &qh->qh_list, q_hashlist) {
		/*
		 * We already have the hashlock. We don't need the
		 * dqlock to look at the id field of the dquot, since the
		 * id can't be modified without the hashlock anyway.
		 */
		if (be32_to_cpu(dqp->q_core.d_id) == id && dqp->q_mount == mp) {
			trace_xfs_dqlookup_found(dqp);

			/*
			 * All in core dquots must be on the dqlist of mp
			 */
			ASSERT(!list_empty(&dqp->q_mplist));

			xfs_dqlock(dqp);
			if (dqp->q_nrefs == 0) {
				ASSERT(!list_empty(&dqp->q_freelist));
				if (!mutex_trylock(&xfs_Gqm->qm_dqfrlist_lock)) {
					trace_xfs_dqlookup_want(dqp);

					/*
					 * We may have raced with dqreclaim_one()
					 * (and lost). So, flag that we don't
					 * want the dquot to be reclaimed.
					 */
					dqp->dq_flags |= XFS_DQ_WANT;
					xfs_dqunlock(dqp);
					mutex_lock(&xfs_Gqm->qm_dqfrlist_lock);
					xfs_dqlock(dqp);
					dqp->dq_flags &= ~(XFS_DQ_WANT);
				}
				flist_locked = B_TRUE;
			}

			/*
			 * id couldn't have changed; we had the hashlock all
			 * along
			 */
			ASSERT(be32_to_cpu(dqp->q_core.d_id) == id);

			if (flist_locked) {
				if (dqp->q_nrefs != 0) {
					mutex_unlock(&xfs_Gqm->qm_dqfrlist_lock);
					flist_locked = B_FALSE;
				} else {
					/* take it off the freelist */
					trace_xfs_dqlookup_freelist(dqp);
					list_del_init(&dqp->q_freelist);
					xfs_Gqm->qm_dqfrlist_cnt--;
				}
			}

			XFS_DQHOLD(dqp);

			if (flist_locked)
				mutex_unlock(&xfs_Gqm->qm_dqfrlist_lock);
			/*
			 * move the dquot to the front of the hashchain
			 */
			ASSERT(mutex_is_locked(&qh->qh_lock));
			list_move(&dqp->q_hashlist, &qh->qh_list);
			trace_xfs_dqlookup_done(dqp);
			*O_dqpp = dqp;
			return 0;
		}
	}

	*O_dqpp = NULL;
	ASSERT(mutex_is_locked(&qh->qh_lock));
	return (1);
}

/*
 * Given the file system, inode OR id, and type (UDQUOT/GDQUOT), return a
 * a locked dquot, doing an allocation (if requested) as needed.
 * When both an inode and an id are given, the inode's id takes precedence.
 * That is, if the id changes while we don't hold the ilock inside this
 * function, the new dquot is returned, not necessarily the one requested
 * in the id argument.
 */
int
xfs_qm_dqget(
	xfs_mount_t	*mp,
	xfs_inode_t	*ip,	  /* locked inode (optional) */
	xfs_dqid_t	id,	  /* uid/projid/gid depending on type */
	uint		type,	  /* XFS_DQ_USER/XFS_DQ_PROJ/XFS_DQ_GROUP */
	uint		flags,	  /* DQALLOC, DQSUSER, DQREPAIR, DOWARN */
	xfs_dquot_t	**O_dqpp) /* OUT : locked incore dquot */
{
	xfs_dquot_t	*dqp;
	xfs_dqhash_t	*h;
	uint		version;
	int		error;

	ASSERT(XFS_IS_QUOTA_RUNNING(mp));
	if ((! XFS_IS_UQUOTA_ON(mp) && type == XFS_DQ_USER) ||
	    (! XFS_IS_PQUOTA_ON(mp) && type == XFS_DQ_PROJ) ||
	    (! XFS_IS_GQUOTA_ON(mp) && type == XFS_DQ_GROUP)) {
		return (ESRCH);
	}
	h = XFS_DQ_HASH(mp, id, type);

#ifdef DEBUG
	if (xfs_do_dqerror) {
		if ((xfs_dqerror_target == mp->m_ddev_targp) &&
		    (xfs_dqreq_num++ % xfs_dqerror_mod) == 0) {
			cmn_err(CE_DEBUG, "Returning error in dqget");
			return (EIO);
		}
	}
#endif

 again:

#ifdef DEBUG
	ASSERT(type == XFS_DQ_USER ||
	       type == XFS_DQ_PROJ ||
	       type == XFS_DQ_GROUP);
	if (ip) {
		ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
		if (type == XFS_DQ_USER)
			ASSERT(ip->i_udquot == NULL);
		else
			ASSERT(ip->i_gdquot == NULL);
	}
#endif
	mutex_lock(&h->qh_lock);

	/*
	 * Look in the cache (hashtable).
	 * The chain is kept locked during lookup.
	 */
	if (xfs_qm_dqlookup(mp, id, h, O_dqpp) == 0) {
		XQM_STATS_INC(xqmstats.xs_qm_dqcachehits);
		/*
		 * The dquot was found, moved to the front of the chain,
		 * taken off the freelist if it was on it, and locked
		 * at this point. Just unlock the hashchain and return.
		 */
		ASSERT(*O_dqpp);
		ASSERT(XFS_DQ_IS_LOCKED(*O_dqpp));
		mutex_unlock(&h->qh_lock);
		trace_xfs_dqget_hit(*O_dqpp);
		return (0);	/* success */
	}
	XQM_STATS_INC(xqmstats.xs_qm_dqcachemisses);

	/*
	 * Dquot cache miss. We don't want to keep the inode lock across
	 * a (potential) disk read. Also we don't want to deal with the lock
	 * ordering between quotainode and this inode. OTOH, dropping the inode
	 * lock here means dealing with a chown that can happen before
	 * we re-acquire the lock.
	 */
	if (ip)
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
	/*
	 * Save the hashchain version stamp, and unlock the chain, so that
	 * we don't keep the lock across a disk read
	 */
	version = h->qh_version;
	mutex_unlock(&h->qh_lock);

	/*
	 * Allocate the dquot on the kernel heap, and read the ondisk
	 * portion off the disk. Also, do all the necessary initialization
	 * This can return ENOENT if dquot didn't exist on disk and we didn't
	 * ask it to allocate; ESRCH if quotas got turned off suddenly.
	 */
	if ((error = xfs_qm_idtodq(mp, id, type,
				  flags & (XFS_QMOPT_DQALLOC|XFS_QMOPT_DQREPAIR|
					   XFS_QMOPT_DOWARN),
				  &dqp))) {
		if (ip)
			xfs_ilock(ip, XFS_ILOCK_EXCL);
		return (error);
	}

	/*
	 * See if this is mount code calling to look at the overall quota limits
	 * which are stored in the id == 0 user or group's dquot.
	 * Since we may not have done a quotacheck by this point, just return
	 * the dquot without attaching it to any hashtables, lists, etc, or even
	 * taking a reference.
	 * The caller must dqdestroy this once done.
	 */
	if (flags & XFS_QMOPT_DQSUSER) {
		ASSERT(id == 0);
		ASSERT(! ip);
		goto dqret;
	}

	/*
	 * Dquot lock comes after hashlock in the lock ordering
	 */
	if (ip) {
		xfs_ilock(ip, XFS_ILOCK_EXCL);

		/*
		 * A dquot could be attached to this inode by now, since
		 * we had dropped the ilock.
		 */
		if (type == XFS_DQ_USER) {
			if (!XFS_IS_UQUOTA_ON(mp)) {
				/* inode stays locked on return */
				xfs_qm_dqdestroy(dqp);
				return XFS_ERROR(ESRCH);
			}
			if (ip->i_udquot) {
				xfs_qm_dqdestroy(dqp);
				dqp = ip->i_udquot;
				xfs_dqlock(dqp);
				goto dqret;
			}
		} else {
			if (!XFS_IS_OQUOTA_ON(mp)) {
				/* inode stays locked on return */
				xfs_qm_dqdestroy(dqp);
				return XFS_ERROR(ESRCH);
			}
			if (ip->i_gdquot) {
				xfs_qm_dqdestroy(dqp);
				dqp = ip->i_gdquot;
				xfs_dqlock(dqp);
				goto dqret;
			}
		}
	}

	/*
	 * Hashlock comes after ilock in lock order
	 */
	mutex_lock(&h->qh_lock);
	if (version != h->qh_version) {
		xfs_dquot_t *tmpdqp;
		/*
		 * Now, see if somebody else put the dquot in the
		 * hashtable before us. This can happen because we didn't
		 * keep the hashchain lock. We don't have to worry about
		 * lock order between the two dquots here since dqp isn't
		 * on any findable lists yet.
		 */
		if (xfs_qm_dqlookup(mp, id, h, &tmpdqp) == 0) {
			/*
			 * Duplicate found. Just throw away the new dquot
			 * and start over.
			 */
			xfs_qm_dqput(tmpdqp);
			mutex_unlock(&h->qh_lock);
			xfs_qm_dqdestroy(dqp);
			XQM_STATS_INC(xqmstats.xs_qm_dquot_dups);
			goto again;
		}
	}

	/*
	 * Put the dquot at the beginning of the hash-chain and mp's list
	 * LOCK ORDER: hashlock, freelistlock, mplistlock, udqlock, gdqlock ..
	 */
	ASSERT(mutex_is_locked(&h->qh_lock));
	dqp->q_hash = h;
	list_add(&dqp->q_hashlist, &h->qh_list);
	h->qh_version++;

	/*
	 * Attach this dquot to this filesystem's list of all dquots,
	 * kept inside the mount structure in m_quotainfo field
	 */
	mutex_lock(&mp->m_quotainfo->qi_dqlist_lock);

	/*
	 * We return a locked dquot to the caller, with a reference taken
	 */
	xfs_dqlock(dqp);
	dqp->q_nrefs = 1;

	list_add(&dqp->q_mplist, &mp->m_quotainfo->qi_dqlist);
	mp->m_quotainfo->qi_dquots++;
	mutex_unlock(&mp->m_quotainfo->qi_dqlist_lock);
	mutex_unlock(&h->qh_lock);
 dqret:
	ASSERT((ip == NULL) || xfs_isilocked(ip, XFS_ILOCK_EXCL));
	trace_xfs_dqget_miss(dqp);
	*O_dqpp = dqp;
	return (0);
}


/*
 * Release a reference to the dquot (decrement ref-count)
 * and unlock it. If there is a group quota attached to this
 * dquot, carefully release that too without tripping over
 * deadlocks'n'stuff.
 */
void
xfs_qm_dqput(
	xfs_dquot_t	*dqp)
{
	xfs_dquot_t	*gdqp;

	ASSERT(dqp->q_nrefs > 0);
	ASSERT(XFS_DQ_IS_LOCKED(dqp));

	trace_xfs_dqput(dqp);

	if (dqp->q_nrefs != 1) {
		dqp->q_nrefs--;
		xfs_dqunlock(dqp);
		return;
	}

	/*
	 * drop the dqlock and acquire the freelist and dqlock
	 * in the right order; but try to get it out-of-order first
	 */
	if (!mutex_trylock(&xfs_Gqm->qm_dqfrlist_lock)) {
		trace_xfs_dqput_wait(dqp);
		xfs_dqunlock(dqp);
		mutex_lock(&xfs_Gqm->qm_dqfrlist_lock);
		xfs_dqlock(dqp);
	}

	while (1) {
		gdqp = NULL;

		/* We can't depend on nrefs being == 1 here */
		if (--dqp->q_nrefs == 0) {
			trace_xfs_dqput_free(dqp);

			list_add_tail(&dqp->q_freelist, &xfs_Gqm->qm_dqfrlist);
			xfs_Gqm->qm_dqfrlist_cnt++;

			/*
			 * If we just added a udquot to the freelist, then
			 * we want to release the gdquot reference that
			 * it (probably) has. Otherwise it'll keep the
			 * gdquot from getting reclaimed.
			 */
			if ((gdqp = dqp->q_gdquot)) {
				/*
				 * Avoid a recursive dqput call
				 */
				xfs_dqlock(gdqp);
				dqp->q_gdquot = NULL;
			}
		}
		xfs_dqunlock(dqp);

		/*
		 * If we had a group quota inside the user quota as a hint,
		 * release it now.
		 */
		if (! gdqp)
			break;
		dqp = gdqp;
	}
	mutex_unlock(&xfs_Gqm->qm_dqfrlist_lock);
}

/*
 * Release a dquot. Flush it if dirty, then dqput() it.
 * dquot must not be locked.
 */
void
xfs_qm_dqrele(
	xfs_dquot_t	*dqp)
{
	if (!dqp)
		return;

	trace_xfs_dqrele(dqp);

	xfs_dqlock(dqp);
	/*
	 * We don't care to flush it if the dquot is dirty here.
	 * That will create stutters that we want to avoid.
	 * Instead we do a delayed write when we try to reclaim
	 * a dirty dquot. Also xfs_sync will take part of the burden...
	 */
	xfs_qm_dqput(dqp);
}

/*
 * This is the dquot flushing I/O completion routine.  It is called
 * from interrupt level when the buffer containing the dquot is
 * flushed to disk.  It is responsible for removing the dquot logitem
 * from the AIL if it has not been re-logged, and unlocking the dquot's
 * flush lock. This behavior is very similar to that of inodes..
 */
STATIC void
xfs_qm_dqflush_done(
	struct xfs_buf		*bp,
	struct xfs_log_item	*lip)
{
	xfs_dq_logitem_t	*qip = (struct xfs_dq_logitem *)lip;
	xfs_dquot_t		*dqp = qip->qli_dquot;
	struct xfs_ail		*ailp = lip->li_ailp;

	/*
	 * We only want to pull the item from the AIL if its
	 * location in the log has not changed since we started the flush.
	 * Thus, we only bother if the dquot's lsn has
	 * not changed. First we check the lsn outside the lock
	 * since it's cheaper, and then we recheck while
	 * holding the lock before removing the dquot from the AIL.
	 */
	if ((lip->li_flags & XFS_LI_IN_AIL) &&
	    lip->li_lsn == qip->qli_flush_lsn) {

		/* xfs_trans_ail_delete() drops the AIL lock. */
		spin_lock(&ailp->xa_lock);
		if (lip->li_lsn == qip->qli_flush_lsn)
			xfs_trans_ail_delete(ailp, lip);
		else
			spin_unlock(&ailp->xa_lock);
	}

	/*
	 * Release the dq's flush lock since we're done with it.
	 */
	xfs_dqfunlock(dqp);
}

/*
 * Write a modified dquot to disk.
 * The dquot must be locked and the flush lock too taken by caller.
 * The flush lock will not be unlocked until the dquot reaches the disk,
 * but the dquot is free to be unlocked and modified by the caller
 * in the interim. Dquot is still locked on return. This behavior is
 * identical to that of inodes.
 */
int
xfs_qm_dqflush(
	xfs_dquot_t		*dqp,
	uint			flags)
{
	xfs_mount_t		*mp;
	xfs_buf_t		*bp;
	xfs_disk_dquot_t	*ddqp;
	int			error;

	ASSERT(XFS_DQ_IS_LOCKED(dqp));
	ASSERT(!completion_done(&dqp->q_flush));
	trace_xfs_dqflush(dqp);

	/*
	 * If not dirty, or it's pinned and we are not supposed to
	 * block, nada.
	 */
	if (!XFS_DQ_IS_DIRTY(dqp) ||
	    (!(flags & SYNC_WAIT) && atomic_read(&dqp->q_pincount) > 0)) {
		xfs_dqfunlock(dqp);
		return 0;
	}
	xfs_qm_dqunpin_wait(dqp);

	/*
	 * This may have been unpinned because the filesystem is shutting
	 * down forcibly. If that's the case we must not write this dquot
	 * to disk, because the log record didn't make it to disk!
	 */
	if (XFS_FORCED_SHUTDOWN(dqp->q_mount)) {
		dqp->dq_flags &= ~(XFS_DQ_DIRTY);
		xfs_dqfunlock(dqp);
		return XFS_ERROR(EIO);
	}

	/*
	 * Get the buffer containing the on-disk dquot
	 * We don't need a transaction envelope because we know that the
	 * the ondisk-dquot has already been allocated for.
	 */
	if ((error = xfs_qm_dqtobp(NULL, dqp, &ddqp, &bp, XFS_QMOPT_DOWARN))) {
		ASSERT(error != ENOENT);
		/*
		 * Quotas could have gotten turned off (ESRCH)
		 */
		xfs_dqfunlock(dqp);
		return (error);
	}

	if (xfs_qm_dqcheck(&dqp->q_core, be32_to_cpu(ddqp->d_id),
			   0, XFS_QMOPT_DOWARN, "dqflush (incore copy)")) {
		xfs_force_shutdown(dqp->q_mount, SHUTDOWN_CORRUPT_INCORE);
		return XFS_ERROR(EIO);
	}

	/* This is the only portion of data that needs to persist */
	memcpy(ddqp, &(dqp->q_core), sizeof(xfs_disk_dquot_t));

	/*
	 * Clear the dirty field and remember the flush lsn for later use.
	 */
	dqp->dq_flags &= ~(XFS_DQ_DIRTY);
	mp = dqp->q_mount;

	xfs_trans_ail_copy_lsn(mp->m_ail, &dqp->q_logitem.qli_flush_lsn,
					&dqp->q_logitem.qli_item.li_lsn);

	/*
	 * Attach an iodone routine so that we can remove this dquot from the
	 * AIL and release the flush lock once the dquot is synced to disk.
	 */
	xfs_buf_attach_iodone(bp, xfs_qm_dqflush_done,
				  &dqp->q_logitem.qli_item);

	/*
	 * If the buffer is pinned then push on the log so we won't
	 * get stuck waiting in the write for too long.
	 */
	if (XFS_BUF_ISPINNED(bp)) {
		trace_xfs_dqflush_force(dqp);
		xfs_log_force(mp, 0);
	}

	if (flags & SYNC_WAIT)
		error = xfs_bwrite(mp, bp);
	else
		xfs_bdwrite(mp, bp);

	trace_xfs_dqflush_done(dqp);

	/*
	 * dqp is still locked, but caller is free to unlock it now.
	 */
	return error;

}

int
xfs_qm_dqlock_nowait(
	xfs_dquot_t *dqp)
{
	return mutex_trylock(&dqp->q_qlock);
}

void
xfs_dqlock(
	xfs_dquot_t *dqp)
{
	mutex_lock(&dqp->q_qlock);
}

void
xfs_dqunlock(
	xfs_dquot_t *dqp)
{
	mutex_unlock(&(dqp->q_qlock));
	if (dqp->q_logitem.qli_dquot == dqp) {
		/* Once was dqp->q_mount, but might just have been cleared */
		xfs_trans_unlocked_item(dqp->q_logitem.qli_item.li_ailp,
					(xfs_log_item_t*)&(dqp->q_logitem));
	}
}


void
xfs_dqunlock_nonotify(
	xfs_dquot_t *dqp)
{
	mutex_unlock(&(dqp->q_qlock));
}

/*
 * Lock two xfs_dquot structures.
 *
 * To avoid deadlocks we always lock the quota structure with
 * the lowerd id first.
 */
void
xfs_dqlock2(
	xfs_dquot_t	*d1,
	xfs_dquot_t	*d2)
{
	if (d1 && d2) {
		ASSERT(d1 != d2);
		if (be32_to_cpu(d1->q_core.d_id) >
		    be32_to_cpu(d2->q_core.d_id)) {
			mutex_lock(&d2->q_qlock);
			mutex_lock_nested(&d1->q_qlock, XFS_QLOCK_NESTED);
		} else {
			mutex_lock(&d1->q_qlock);
			mutex_lock_nested(&d2->q_qlock, XFS_QLOCK_NESTED);
		}
	} else if (d1) {
		mutex_lock(&d1->q_qlock);
	} else if (d2) {
		mutex_lock(&d2->q_qlock);
	}
}


/*
 * Take a dquot out of the mount's dqlist as well as the hashlist.
 * This is called via unmount as well as quotaoff, and the purge
 * will always succeed unless there are soft (temp) references
 * outstanding.
 *
 * This returns 0 if it was purged, 1 if it wasn't. It's not an error code
 * that we're returning! XXXsup - not cool.
 */
/* ARGSUSED */
int
xfs_qm_dqpurge(
	xfs_dquot_t	*dqp)
{
	xfs_dqhash_t	*qh = dqp->q_hash;
	xfs_mount_t	*mp = dqp->q_mount;

	ASSERT(mutex_is_locked(&mp->m_quotainfo->qi_dqlist_lock));
	ASSERT(mutex_is_locked(&dqp->q_hash->qh_lock));

	xfs_dqlock(dqp);
	/*
	 * We really can't afford to purge a dquot that is
	 * referenced, because these are hard refs.
	 * It shouldn't happen in general because we went thru _all_ inodes in
	 * dqrele_all_inodes before calling this and didn't let the mountlock go.
	 * However it is possible that we have dquots with temporary
	 * references that are not attached to an inode. e.g. see xfs_setattr().
	 */
	if (dqp->q_nrefs != 0) {
		xfs_dqunlock(dqp);
		mutex_unlock(&dqp->q_hash->qh_lock);
		return (1);
	}

	ASSERT(!list_empty(&dqp->q_freelist));

	/*
	 * If we're turning off quotas, we have to make sure that, for
	 * example, we don't delete quota disk blocks while dquots are
	 * in the process of getting written to those disk blocks.
	 * This dquot might well be on AIL, and we can't leave it there
	 * if we're turning off quotas. Basically, we need this flush
	 * lock, and are willing to block on it.
	 */
	if (!xfs_dqflock_nowait(dqp)) {
		/*
		 * Block on the flush lock after nudging dquot buffer,
		 * if it is incore.
		 */
		xfs_qm_dqflock_pushbuf_wait(dqp);
	}

	/*
	 * XXXIf we're turning this type of quotas off, we don't care
	 * about the dirty metadata sitting in this dquot. OTOH, if
	 * we're unmounting, we do care, so we flush it and wait.
	 */
	if (XFS_DQ_IS_DIRTY(dqp)) {
		int	error;

		/* dqflush unlocks dqflock */
		/*
		 * Given that dqpurge is a very rare occurrence, it is OK
		 * that we're holding the hashlist and mplist locks
		 * across the disk write. But, ... XXXsup
		 *
		 * We don't care about getting disk errors here. We need
		 * to purge this dquot anyway, so we go ahead regardless.
		 */
		error = xfs_qm_dqflush(dqp, SYNC_WAIT);
		if (error)
			xfs_fs_cmn_err(CE_WARN, mp,
				"xfs_qm_dqpurge: dquot %p flush failed", dqp);
		xfs_dqflock(dqp);
	}
	ASSERT(atomic_read(&dqp->q_pincount) == 0);
	ASSERT(XFS_FORCED_SHUTDOWN(mp) ||
	       !(dqp->q_logitem.qli_item.li_flags & XFS_LI_IN_AIL));

	list_del_init(&dqp->q_hashlist);
	qh->qh_version++;
	list_del_init(&dqp->q_mplist);
	mp->m_quotainfo->qi_dqreclaims++;
	mp->m_quotainfo->qi_dquots--;
	ASSERT(!list_empty(&dqp->q_freelist));

	dqp->q_mount = NULL;
	dqp->q_hash = NULL;
	dqp->dq_flags = XFS_DQ_INACTIVE;
	memset(&dqp->q_core, 0, sizeof(dqp->q_core));
	xfs_dqfunlock(dqp);
	xfs_dqunlock(dqp);
	mutex_unlock(&qh->qh_lock);
	return (0);
}


#ifdef QUOTADEBUG
void
xfs_qm_dqprint(xfs_dquot_t *dqp)
{
	cmn_err(CE_DEBUG, "-----------KERNEL DQUOT----------------");
	cmn_err(CE_DEBUG, "---- dquotID =  %d",
		(int)be32_to_cpu(dqp->q_core.d_id));
	cmn_err(CE_DEBUG, "---- type    =  %s", DQFLAGTO_TYPESTR(dqp));
	cmn_err(CE_DEBUG, "---- fs      =  0x%p", dqp->q_mount);
	cmn_err(CE_DEBUG, "---- blkno   =  0x%x", (int) dqp->q_blkno);
	cmn_err(CE_DEBUG, "---- boffset =  0x%x", (int) dqp->q_bufoffset);
	cmn_err(CE_DEBUG, "---- blkhlimit =  %Lu (0x%x)",
		be64_to_cpu(dqp->q_core.d_blk_hardlimit),
		(int)be64_to_cpu(dqp->q_core.d_blk_hardlimit));
	cmn_err(CE_DEBUG, "---- blkslimit =  %Lu (0x%x)",
		be64_to_cpu(dqp->q_core.d_blk_softlimit),
		(int)be64_to_cpu(dqp->q_core.d_blk_softlimit));
	cmn_err(CE_DEBUG, "---- inohlimit =  %Lu (0x%x)",
		be64_to_cpu(dqp->q_core.d_ino_hardlimit),
		(int)be64_to_cpu(dqp->q_core.d_ino_hardlimit));
	cmn_err(CE_DEBUG, "---- inoslimit =  %Lu (0x%x)",
		be64_to_cpu(dqp->q_core.d_ino_softlimit),
		(int)be64_to_cpu(dqp->q_core.d_ino_softlimit));
	cmn_err(CE_DEBUG, "---- bcount  =  %Lu (0x%x)",
		be64_to_cpu(dqp->q_core.d_bcount),
		(int)be64_to_cpu(dqp->q_core.d_bcount));
	cmn_err(CE_DEBUG, "---- icount  =  %Lu (0x%x)",
		be64_to_cpu(dqp->q_core.d_icount),
		(int)be64_to_cpu(dqp->q_core.d_icount));
	cmn_err(CE_DEBUG, "---- btimer  =  %d",
		(int)be32_to_cpu(dqp->q_core.d_btimer));
	cmn_err(CE_DEBUG, "---- itimer  =  %d",
		(int)be32_to_cpu(dqp->q_core.d_itimer));
	cmn_err(CE_DEBUG, "---------------------------");
}
#endif

/*
 * Give the buffer a little push if it is incore and
 * wait on the flush lock.
 */
void
xfs_qm_dqflock_pushbuf_wait(
	xfs_dquot_t	*dqp)
{
	xfs_mount_t	*mp = dqp->q_mount;
	xfs_buf_t	*bp;

	/*
	 * Check to see if the dquot has been flushed delayed
	 * write.  If so, grab its buffer and send it
	 * out immediately.  We'll be able to acquire
	 * the flush lock when the I/O completes.
	 */
	bp = xfs_incore(mp->m_ddev_targp, dqp->q_blkno,
			mp->m_quotainfo->qi_dqchunklen, XBF_TRYLOCK);
	if (!bp)
		goto out_lock;

	if (XFS_BUF_ISDELAYWRITE(bp)) {
		if (XFS_BUF_ISPINNED(bp))
			xfs_log_force(mp, 0);
		xfs_buf_delwri_promote(bp);
		wake_up_process(bp->b_target->bt_task);
	}
	xfs_buf_relse(bp);
out_lock:
	xfs_dqflock(dqp);
}
