/*
 * Copyright (C) Sistina Software, Inc.  1997-2003 All rights reserved.
 * Copyright (C) 2004-2006 Red Hat, Inc.  All rights reserved.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License version 2.
 */

#ifndef __LOG_DOT_H__
#define __LOG_DOT_H__

#include <linux/list.h>
#include <linux/spinlock.h>
#include "incore.h"

/**
 * gfs2_log_lock - acquire the right to mess with the log manager
 * @sdp: the filesystem
 *
 */

static inline void gfs2_log_lock(struct gfs2_sbd *sdp)
__acquires(&sdp->sd_log_lock)
{
	spin_lock(&sdp->sd_log_lock);
}

/**
 * gfs2_log_unlock - release the right to mess with the log manager
 * @sdp: the filesystem
 *
 */

static inline void gfs2_log_unlock(struct gfs2_sbd *sdp)
__releases(&sdp->sd_log_lock)
{
	spin_unlock(&sdp->sd_log_lock);
}

static inline void gfs2_log_pointers_init(struct gfs2_sbd *sdp,
					  unsigned int value)
{
	if (++value == sdp->sd_jdesc->jd_blocks) {
		value = 0;
	}
	sdp->sd_log_head = sdp->sd_log_tail = value;
}

extern unsigned int gfs2_struct2blk(struct gfs2_sbd *sdp, unsigned int nstruct,
			    unsigned int ssize);

extern int gfs2_log_reserve(struct gfs2_sbd *sdp, unsigned int blks);
extern void gfs2_log_incr_head(struct gfs2_sbd *sdp);

extern struct buffer_head *gfs2_log_get_buf(struct gfs2_sbd *sdp);
extern struct buffer_head *gfs2_log_fake_buf(struct gfs2_sbd *sdp,
				      struct buffer_head *real);
extern void gfs2_log_flush(struct gfs2_sbd *sdp, struct gfs2_glock *gl);
extern void gfs2_log_commit(struct gfs2_sbd *sdp, struct gfs2_trans *trans);
extern void gfs2_remove_from_ail(struct gfs2_bufdata *bd);

extern void gfs2_log_shutdown(struct gfs2_sbd *sdp);
extern void gfs2_meta_syncfs(struct gfs2_sbd *sdp);
extern int gfs2_logd(void *data);

#endif /* __LOG_DOT_H__ */
