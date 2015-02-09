/*
 * Copyright (C) Sistina Software, Inc.  1997-2003 All rights reserved.
 * Copyright (C) 2004-2006 Red Hat, Inc.  All rights reserved.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License version 2.
 */

#ifndef __DIO_DOT_H__
#define __DIO_DOT_H__

#include <linux/buffer_head.h>
#include <linux/string.h>
#include "incore.h"

static inline void gfs2_buffer_clear(struct buffer_head *bh)
{
	memset(bh->b_data, 0, bh->b_size);
}

static inline void gfs2_buffer_clear_tail(struct buffer_head *bh, int head)
{
	BUG_ON(head > bh->b_size);
	memset(bh->b_data + head, 0, bh->b_size - head);
}

static inline void gfs2_buffer_copy_tail(struct buffer_head *to_bh,
					 int to_head,
					 struct buffer_head *from_bh,
					 int from_head)
{
	BUG_ON(from_head < to_head);
	memcpy(to_bh->b_data + to_head, from_bh->b_data + from_head,
	       from_bh->b_size - from_head);
	memset(to_bh->b_data + to_bh->b_size + to_head - from_head,
	       0, from_head - to_head);
}

extern const struct address_space_operations gfs2_meta_aops;

static inline struct gfs2_sbd *gfs2_mapping2sbd(struct address_space *mapping)
{
	struct inode *inode = mapping->host;
	if (mapping->a_ops == &gfs2_meta_aops)
		return (((struct gfs2_glock *)mapping) - 1)->gl_sbd;
	else
		return inode->i_sb->s_fs_info;
}

void gfs2_meta_sync(struct gfs2_glock *gl);

struct buffer_head *gfs2_meta_new(struct gfs2_glock *gl, u64 blkno);
int gfs2_meta_read(struct gfs2_glock *gl, u64 blkno,
		   int flags, struct buffer_head **bhp);
int gfs2_meta_wait(struct gfs2_sbd *sdp, struct buffer_head *bh);
struct buffer_head *gfs2_getbuf(struct gfs2_glock *gl, u64 blkno, int create);

void gfs2_attach_bufdata(struct gfs2_glock *gl, struct buffer_head *bh,
			 int meta);

void gfs2_remove_from_journal(struct buffer_head *bh, struct gfs2_trans *tr,
			      int meta);

void gfs2_meta_wipe(struct gfs2_inode *ip, u64 bstart, u32 blen);

int gfs2_meta_indirect_buffer(struct gfs2_inode *ip, int height, u64 num,
			      int new, struct buffer_head **bhp);

static inline int gfs2_meta_inode_buffer(struct gfs2_inode *ip,
					 struct buffer_head **bhp)
{
	return gfs2_meta_indirect_buffer(ip, 0, ip->i_no_addr, 0, bhp);
}

struct buffer_head *gfs2_meta_ra(struct gfs2_glock *gl, u64 dblock, u32 extlen);

#define buffer_busy(bh) \
((bh)->b_state & ((1ul << BH_Dirty) | (1ul << BH_Lock) | (1ul << BH_Pinned)))
#define buffer_in_io(bh) \
((bh)->b_state & ((1ul << BH_Dirty) | (1ul << BH_Lock)))

#endif /* __DIO_DOT_H__ */
