/*
 * Copyright (c) 2000,2002,2005 Silicon Graphics, Inc.
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
#ifndef __XFS_TRANS_PRIV_H__
#define	__XFS_TRANS_PRIV_H__

struct xfs_log_item;
struct xfs_log_item_desc;
struct xfs_mount;
struct xfs_trans;

void	xfs_trans_add_item(struct xfs_trans *, struct xfs_log_item *);
void	xfs_trans_del_item(struct xfs_log_item *);
void	xfs_trans_free_items(struct xfs_trans *tp, xfs_lsn_t commit_lsn,
				int flags);
void	xfs_trans_item_committed(struct xfs_log_item *lip,
				xfs_lsn_t commit_lsn, int aborted);
void	xfs_trans_unreserve_and_mod_sb(struct xfs_trans *tp);

/*
 * AIL traversal cursor.
 *
 * Rather than using a generation number for detecting changes in the ail, use
 * a cursor that is protected by the ail lock. The aild cursor exists in the
 * struct xfs_ail, but other traversals can declare it on the stack and link it
 * to the ail list.
 *
 * When an object is deleted from or moved int the AIL, the cursor list is
 * searched to see if the object is a designated cursor item. If it is, it is
 * deleted from the cursor so that the next time the cursor is used traversal
 * will return to the start.
 *
 * This means a traversal colliding with a removal will cause a restart of the
 * list scan, rather than any insertion or deletion anywhere in the list. The
 * low bit of the item pointer is set if the cursor has been invalidated so
 * that we can tell the difference between invalidation and reaching the end
 * of the list to trigger traversal restarts.
 */
struct xfs_ail_cursor {
	struct xfs_ail_cursor	*next;
	struct xfs_log_item	*item;
};

/*
 * Private AIL structures.
 *
 * Eventually we need to drive the locking in here as well.
 */
struct xfs_ail {
	struct xfs_mount	*xa_mount;
	struct list_head	xa_ail;
	uint			xa_gen;
	struct task_struct	*xa_task;
	xfs_lsn_t		xa_target;
	struct xfs_ail_cursor	xa_cursors;
	spinlock_t		xa_lock;
};

/*
 * From xfs_trans_ail.c
 */
void			xfs_trans_ail_update(struct xfs_ail *ailp,
					struct xfs_log_item *lip, xfs_lsn_t lsn)
					__releases(ailp->xa_lock);
void			xfs_trans_ail_delete(struct xfs_ail *ailp,
					struct xfs_log_item *lip)
					__releases(ailp->xa_lock);
void			xfs_trans_ail_push(struct xfs_ail *, xfs_lsn_t);
void			xfs_trans_unlocked_item(struct xfs_ail *,
					xfs_log_item_t *);

xfs_lsn_t		xfs_trans_ail_tail(struct xfs_ail *ailp);

struct xfs_log_item	*xfs_trans_ail_cursor_first(struct xfs_ail *ailp,
					struct xfs_ail_cursor *cur,
					xfs_lsn_t lsn);
struct xfs_log_item	*xfs_trans_ail_cursor_next(struct xfs_ail *ailp,
					struct xfs_ail_cursor *cur);
void			xfs_trans_ail_cursor_done(struct xfs_ail *ailp,
					struct xfs_ail_cursor *cur);

long	xfsaild_push(struct xfs_ail *, xfs_lsn_t *);
void	xfsaild_wakeup(struct xfs_ail *, xfs_lsn_t);
int	xfsaild_start(struct xfs_ail *);
void	xfsaild_stop(struct xfs_ail *);

#if BITS_PER_LONG != 64
static inline void
xfs_trans_ail_copy_lsn(
	struct xfs_ail	*ailp,
	xfs_lsn_t	*dst,
	xfs_lsn_t	*src)
{
	ASSERT(sizeof(xfs_lsn_t) == 8);	/* don't lock if it shrinks */
	spin_lock(&ailp->xa_lock);
	*dst = *src;
	spin_unlock(&ailp->xa_lock);
}
#else
static inline void
xfs_trans_ail_copy_lsn(
	struct xfs_ail	*ailp,
	xfs_lsn_t	*dst,
	xfs_lsn_t	*src)
{
	ASSERT(sizeof(xfs_lsn_t) == 8);
	*dst = *src;
}
#endif
#endif	/* __XFS_TRANS_PRIV_H__ */
