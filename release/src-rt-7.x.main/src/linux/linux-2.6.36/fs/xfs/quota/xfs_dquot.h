/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
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
#ifndef __XFS_DQUOT_H__
#define __XFS_DQUOT_H__

/*
 * Dquots are structures that hold quota information about a user or a group,
 * much like inodes are for files. In fact, dquots share many characteristics
 * with inodes. However, dquots can also be a centralized resource, relative
 * to a collection of inodes. In this respect, dquots share some characteristics
 * of the superblock.
 * XFS dquots exploit both those in its algorithms. They make every attempt
 * to not be a bottleneck when quotas are on and have minimal impact, if any,
 * when quotas are off.
 */

/*
 * The hash chain headers (hash buckets)
 */
typedef struct xfs_dqhash {
	struct list_head  qh_list;
	struct mutex	  qh_lock;
	uint		  qh_version;	/* ever increasing version */
	uint		  qh_nelems;	/* number of dquots on the list */
} xfs_dqhash_t;

struct xfs_mount;
struct xfs_trans;

/*
 * The incore dquot structure
 */
typedef struct xfs_dquot {
	uint		 dq_flags;	/* various flags (XFS_DQ_*) */
	struct list_head q_freelist;	/* global free list of dquots */
	struct list_head q_mplist;	/* mount's list of dquots */
	struct list_head q_hashlist;	/* gloabl hash list of dquots */
	xfs_dqhash_t	*q_hash;	/* the hashchain header */
	struct xfs_mount*q_mount;	/* filesystem this relates to */
	struct xfs_trans*q_transp;	/* trans this belongs to currently */
	uint		 q_nrefs;	/* # active refs from inodes */
	xfs_daddr_t	 q_blkno;	/* blkno of dquot buffer */
	int		 q_bufoffset;	/* off of dq in buffer (# dquots) */
	xfs_fileoff_t	 q_fileoffset;	/* offset in quotas file */

	struct xfs_dquot*q_gdquot;	/* group dquot, hint only */
	xfs_disk_dquot_t q_core;	/* actual usage & quotas */
	xfs_dq_logitem_t q_logitem;	/* dquot log item */
	xfs_qcnt_t	 q_res_bcount;	/* total regular nblks used+reserved */
	xfs_qcnt_t	 q_res_icount;	/* total inos allocd+reserved */
	xfs_qcnt_t	 q_res_rtbcount;/* total realtime blks used+reserved */
	struct mutex	 q_qlock;	/* quota lock */
	struct completion q_flush;	/* flush completion queue */
	atomic_t          q_pincount;	/* dquot pin count */
	wait_queue_head_t q_pinwait;	/* dquot pinning wait queue */
} xfs_dquot_t;

/*
 * Lock hierarchy for q_qlock:
 *	XFS_QLOCK_NORMAL is the implicit default,
 * 	XFS_QLOCK_NESTED is the dquot with the higher id in xfs_dqlock2
 */
enum {
	XFS_QLOCK_NORMAL = 0,
	XFS_QLOCK_NESTED,
};

#define XFS_DQHOLD(dqp)		((dqp)->q_nrefs++)

/*
 * Manage the q_flush completion queue embedded in the dquot.  This completion
 * queue synchronizes processes attempting to flush the in-core dquot back to
 * disk.
 */
static inline void xfs_dqflock(xfs_dquot_t *dqp)
{
	wait_for_completion(&dqp->q_flush);
}

static inline int xfs_dqflock_nowait(xfs_dquot_t *dqp)
{
	return try_wait_for_completion(&dqp->q_flush);
}

static inline void xfs_dqfunlock(xfs_dquot_t *dqp)
{
	complete(&dqp->q_flush);
}

#define XFS_DQ_IS_LOCKED(dqp)	(mutex_is_locked(&((dqp)->q_qlock)))
#define XFS_DQ_IS_DIRTY(dqp)	((dqp)->dq_flags & XFS_DQ_DIRTY)
#define XFS_QM_ISUDQ(dqp)	((dqp)->dq_flags & XFS_DQ_USER)
#define XFS_QM_ISPDQ(dqp)	((dqp)->dq_flags & XFS_DQ_PROJ)
#define XFS_QM_ISGDQ(dqp)	((dqp)->dq_flags & XFS_DQ_GROUP)
#define XFS_DQ_TO_QINF(dqp)	((dqp)->q_mount->m_quotainfo)
#define XFS_DQ_TO_QIP(dqp)	(XFS_QM_ISUDQ(dqp) ? \
				 XFS_DQ_TO_QINF(dqp)->qi_uquotaip : \
				 XFS_DQ_TO_QINF(dqp)->qi_gquotaip)

#define XFS_IS_THIS_QUOTA_OFF(d) (! (XFS_QM_ISUDQ(d) ? \
				     (XFS_IS_UQUOTA_ON((d)->q_mount)) : \
				     (XFS_IS_OQUOTA_ON((d)->q_mount))))

#ifdef QUOTADEBUG
extern void		xfs_qm_dqprint(xfs_dquot_t *);
#else
#define xfs_qm_dqprint(a)
#endif

extern void		xfs_qm_dqdestroy(xfs_dquot_t *);
extern int		xfs_qm_dqflush(xfs_dquot_t *, uint);
extern int		xfs_qm_dqpurge(xfs_dquot_t *);
extern void		xfs_qm_dqunpin_wait(xfs_dquot_t *);
extern int		xfs_qm_dqlock_nowait(xfs_dquot_t *);
extern void		xfs_qm_dqflock_pushbuf_wait(xfs_dquot_t *dqp);
extern void		xfs_qm_adjust_dqtimers(xfs_mount_t *,
					xfs_disk_dquot_t *);
extern void		xfs_qm_adjust_dqlimits(xfs_mount_t *,
					xfs_disk_dquot_t *);
extern int		xfs_qm_dqget(xfs_mount_t *, xfs_inode_t *,
					xfs_dqid_t, uint, uint, xfs_dquot_t **);
extern void		xfs_qm_dqput(xfs_dquot_t *);
extern void		xfs_dqlock(xfs_dquot_t *);
extern void		xfs_dqlock2(xfs_dquot_t *, xfs_dquot_t *);
extern void		xfs_dqunlock(xfs_dquot_t *);
extern void		xfs_dqunlock_nonotify(xfs_dquot_t *);

#endif /* __XFS_DQUOT_H__ */
