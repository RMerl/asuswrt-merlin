/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2008 Nokia Corporation.
 * Copyright (C) 2008 University of Szeged, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Artem Bityutskiy
 *          Adrian Hunter
 *          Zoltan Sogor
 */

#ifndef __UBIFS_H__
#define __UBIFS_H__

/* Maximum logical eraseblock size in bytes */
#define UBIFS_MAX_LEB_SZ (2*1024*1024)

/* Minimum amount of data UBIFS writes to the flash */
#define MIN_WRITE_SZ (UBIFS_DATA_NODE_SZ + 8)

/* Largest key size supported in this implementation */
#define CUR_MAX_KEY_LEN UBIFS_SK_LEN

/*
 * There is no notion of truncation key because truncation nodes do not exist
 * in TNC. However, when replaying, it is handy to introduce fake "truncation"
 * keys for truncation nodes because the code becomes simpler. So we define
 * %UBIFS_TRUN_KEY type.
 */
#define UBIFS_TRUN_KEY UBIFS_KEY_TYPES_CNT

/* The below union makes it easier to deal with keys */
union ubifs_key
{
	uint8_t u8[CUR_MAX_KEY_LEN];
	uint32_t u32[CUR_MAX_KEY_LEN/4];
	uint64_t u64[CUR_MAX_KEY_LEN/8];
	__le32 j32[CUR_MAX_KEY_LEN/4];
};

/*
 * LEB properties flags.
 *
 * LPROPS_UNCAT: not categorized
 * LPROPS_DIRTY: dirty > 0, not index
 * LPROPS_DIRTY_IDX: dirty + free > UBIFS_CH_SZ and index
 * LPROPS_FREE: free > 0, not empty, not index
 * LPROPS_HEAP_CNT: number of heaps used for storing categorized LEBs
 * LPROPS_EMPTY: LEB is empty, not taken
 * LPROPS_FREEABLE: free + dirty == leb_size, not index, not taken
 * LPROPS_FRDI_IDX: free + dirty == leb_size and index, may be taken
 * LPROPS_CAT_MASK: mask for the LEB categories above
 * LPROPS_TAKEN: LEB was taken (this flag is not saved on the media)
 * LPROPS_INDEX: LEB contains indexing nodes (this flag also exists on flash)
 */
enum {
	LPROPS_UNCAT     =  0,
	LPROPS_DIRTY     =  1,
	LPROPS_DIRTY_IDX =  2,
	LPROPS_FREE      =  3,
	LPROPS_HEAP_CNT  =  3,
	LPROPS_EMPTY     =  4,
	LPROPS_FREEABLE  =  5,
	LPROPS_FRDI_IDX  =  6,
	LPROPS_CAT_MASK  = 15,
	LPROPS_TAKEN     = 16,
	LPROPS_INDEX     = 32,
};

/**
 * struct ubifs_lprops - logical eraseblock properties.
 * @free: amount of free space in bytes
 * @dirty: amount of dirty space in bytes
 * @flags: LEB properties flags (see above)
 */
struct ubifs_lprops
{
	int free;
	int dirty;
	int flags;
};

/**
 * struct ubifs_lpt_lprops - LPT logical eraseblock properties.
 * @free: amount of free space in bytes
 * @dirty: amount of dirty space in bytes
 */
struct ubifs_lpt_lprops
{
	int free;
	int dirty;
};

struct ubifs_nnode;

/**
 * struct ubifs_cnode - LEB Properties Tree common node.
 * @parent: parent nnode
 * @cnext: next cnode to commit
 * @flags: flags (%DIRTY_LPT_NODE or %OBSOLETE_LPT_NODE)
 * @iip: index in parent
 * @level: level in the tree (zero for pnodes, greater than zero for nnodes)
 * @num: node number
 */
struct ubifs_cnode
{
	struct ubifs_nnode *parent;
	struct ubifs_cnode *cnext;
	unsigned long flags;
	int iip;
	int level;
	int num;
};

/**
 * struct ubifs_pnode - LEB Properties Tree leaf node.
 * @parent: parent nnode
 * @cnext: next cnode to commit
 * @flags: flags (%DIRTY_LPT_NODE or %OBSOLETE_LPT_NODE)
 * @iip: index in parent
 * @level: level in the tree (always zero for pnodes)
 * @num: node number
 * @lprops: LEB properties array
 */
struct ubifs_pnode
{
	struct ubifs_nnode *parent;
	struct ubifs_cnode *cnext;
	unsigned long flags;
	int iip;
	int level;
	int num;
	struct ubifs_lprops lprops[UBIFS_LPT_FANOUT];
};

/**
 * struct ubifs_nbranch - LEB Properties Tree internal node branch.
 * @lnum: LEB number of child
 * @offs: offset of child
 * @nnode: nnode child
 * @pnode: pnode child
 * @cnode: cnode child
 */
struct ubifs_nbranch
{
	int lnum;
	int offs;
	union
	{
		struct ubifs_nnode *nnode;
		struct ubifs_pnode *pnode;
		struct ubifs_cnode *cnode;
	};
};

/**
 * struct ubifs_nnode - LEB Properties Tree internal node.
 * @parent: parent nnode
 * @cnext: next cnode to commit
 * @flags: flags (%DIRTY_LPT_NODE or %OBSOLETE_LPT_NODE)
 * @iip: index in parent
 * @level: level in the tree (always greater than zero for nnodes)
 * @num: node number
 * @nbranch: branches to child nodes
 */
struct ubifs_nnode
{
	struct ubifs_nnode *parent;
	struct ubifs_cnode *cnext;
	unsigned long flags;
	int iip;
	int level;
	int num;
	struct ubifs_nbranch nbranch[UBIFS_LPT_FANOUT];
};

/**
 * struct ubifs_lp_stats - statistics of eraseblocks in the main area.
 * @empty_lebs: number of empty LEBs
 * @taken_empty_lebs: number of taken LEBs
 * @idx_lebs: number of indexing LEBs
 * @total_free: total free space in bytes
 * @total_dirty: total dirty space in bytes
 * @total_used: total used space in bytes (includes only data LEBs)
 * @total_dead: total dead space in bytes (includes only data LEBs)
 * @total_dark: total dark space in bytes (includes only data LEBs)
 */
struct ubifs_lp_stats {
	int empty_lebs;
	int taken_empty_lebs;
	int idx_lebs;
	long long total_free;
	long long total_dirty;
	long long total_used;
	long long total_dead;
	long long total_dark;
};

/**
 * struct ubifs_zbranch - key/coordinate/length branch stored in znodes.
 * @key: key
 * @znode: znode address in memory
 * @lnum: LEB number of the indexing node
 * @offs: offset of the indexing node within @lnum
 * @len: target node length
 */
struct ubifs_zbranch
{
	union ubifs_key key;
	struct ubifs_znode *znode;
	int lnum;
	int offs;
	int len;
};

/**
 * struct ubifs_znode - in-memory representation of an indexing node.
 * @parent: parent znode or NULL if it is the root
 * @cnext: next znode to commit
 * @flags: flags
 * @time: last access time (seconds)
 * @level: level of the entry in the TNC tree
 * @child_cnt: count of child znodes
 * @iip: index in parent's zbranch array
 * @alt: lower bound of key range has altered i.e. child inserted at slot 0
 * @zbranch: array of znode branches (@c->fanout elements)
 */
struct ubifs_znode
{
	struct ubifs_znode *parent;
	struct ubifs_znode *cnext;
	unsigned long flags;
	unsigned long time;
	int level;
	int child_cnt;
	int iip;
	int alt;
#ifdef CONFIG_UBIFS_FS_DEBUG
	int lnum, offs, len;
#endif
	struct ubifs_zbranch zbranch[];
};

/**
 * struct ubifs_info - UBIFS file-system description data structure
 * (per-superblock).
 *
 * @highest_inum: highest used inode number
 * @max_sqnum: current global sequence number
 *
 * @jhead_cnt: count of journal heads
 * @max_bud_bytes: maximum number of bytes allowed in buds
 *
 * @zroot: zbranch which points to the root index node and znode
 * @ihead_lnum: LEB number of index head
 * @ihead_offs: offset of index head
 *
 * @log_lebs: number of logical eraseblocks in the log
 * @lpt_lebs: number of LEBs used for lprops table
 * @lpt_first: first LEB of the lprops table area
 * @lpt_last: last LEB of the lprops table area
 * @main_lebs: count of LEBs in the main area
 * @main_first: first LEB of the main area
 * @default_compr: default compression type
 * @favor_lzo: favor LZO compression method
 * @favor_percent: lzo vs. zlib threshold used in case favor LZO
 *
 * @key_hash_type: type of the key hash
 * @key_hash: direntry key hash function
 * @key_fmt: key format
 * @key_len: key length
 * @fanout: fanout of the index tree (number of links per indexing node)
 *
 * @min_io_size: minimal input/output unit size
 * @leb_size: logical eraseblock size in bytes
 * @leb_cnt: count of logical eraseblocks
 * @max_leb_cnt: maximum count of logical eraseblocks
 *
 * @old_idx_sz: size of index on flash
 * @lst: lprops statistics
 *
 * @dead_wm: LEB dead space watermark
 * @dark_wm: LEB dark space watermark
 *
 * @di: UBI device information
 * @vi: UBI volume information
 *
 * @gc_lnum: LEB number used for garbage collection
 * @rp_size: reserved pool size
 *
 * @space_bits: number of bits needed to record free or dirty space
 * @lpt_lnum_bits: number of bits needed to record a LEB number in the LPT
 * @lpt_offs_bits: number of bits needed to record an offset in the LPT
 * @lpt_spc_bits: number of bits needed to space in the LPT
 * @pcnt_bits: number of bits needed to record pnode or nnode number
 * @lnum_bits: number of bits needed to record LEB number
 * @nnode_sz: size of on-flash nnode
 * @pnode_sz: size of on-flash pnode
 * @ltab_sz: size of on-flash LPT lprops table
 * @lsave_sz: size of on-flash LPT save table
 * @pnode_cnt: number of pnodes
 * @nnode_cnt: number of nnodes
 * @lpt_hght: height of the LPT
 *
 * @lpt_lnum: LEB number of the root nnode of the LPT
 * @lpt_offs: offset of the root nnode of the LPT
 * @nhead_lnum: LEB number of LPT head
 * @nhead_offs: offset of LPT head
 * @big_lpt: flag that LPT is too big to write whole during commit
 * @space_fixup: flag indicating that free space in LEBs needs to be cleaned up
 * @lpt_sz: LPT size
 *
 * @ltab_lnum: LEB number of LPT's own lprops table
 * @ltab_offs: offset of LPT's own lprops table
 * @lpt: lprops table
 * @ltab: LPT's own lprops table
 * @lsave_cnt: number of LEB numbers in LPT's save table
 * @lsave_lnum: LEB number of LPT's save table
 * @lsave_offs: offset of LPT's save table
 * @lsave: LPT's save table
 * @lscan_lnum: LEB number of last LPT scan
 */
struct ubifs_info
{
	ino_t highest_inum;
	unsigned long long max_sqnum;

	int jhead_cnt;
	long long max_bud_bytes;

	struct ubifs_zbranch zroot;
	int ihead_lnum;
	int ihead_offs;

	int log_lebs;
	int lpt_lebs;
	int lpt_first;
	int lpt_last;
	int orph_lebs;
	int main_lebs;
	int main_first;
	int default_compr;
	int favor_lzo;
	int favor_percent;

	uint8_t key_hash_type;
	uint32_t (*key_hash)(const char *str, int len);
	int key_fmt;
	int key_len;
	int fanout;

	int min_io_size;
	int leb_size;
	int leb_cnt;
	int max_leb_cnt;

	unsigned long long old_idx_sz;
	struct ubifs_lp_stats lst;

	int dead_wm;
	int dark_wm;

	struct ubi_dev_info di;
	struct ubi_vol_info vi;

	int gc_lnum;
	long long rp_size;

	int space_bits;
	int lpt_lnum_bits;
	int lpt_offs_bits;
	int lpt_spc_bits;
	int pcnt_bits;
	int lnum_bits;
	int nnode_sz;
	int pnode_sz;
	int ltab_sz;
	int lsave_sz;
	int pnode_cnt;
	int nnode_cnt;
	int lpt_hght;

	int lpt_lnum;
	int lpt_offs;
	int nhead_lnum;
	int nhead_offs;
	int big_lpt;
	int space_fixup;
	long long lpt_sz;

	int ltab_lnum;
	int ltab_offs;
	struct ubifs_lprops *lpt;
	struct ubifs_lpt_lprops *ltab;
	int lsave_cnt;
	int lsave_lnum;
	int lsave_offs;
	int *lsave;
	int lscan_lnum;

};

/**
 * ubifs_idx_node_sz - return index node size.
 * @c: the UBIFS file-system description object
 * @child_cnt: number of children of this index node
 */
static inline int ubifs_idx_node_sz(const struct ubifs_info *c, int child_cnt)
{
	return UBIFS_IDX_NODE_SZ + (UBIFS_BRANCH_SZ + c->key_len) * child_cnt;
}

/**
 * ubifs_idx_branch - return pointer to an index branch.
 * @c: the UBIFS file-system description object
 * @idx: index node
 * @bnum: branch number
 */
static inline
struct ubifs_branch *ubifs_idx_branch(const struct ubifs_info *c,
				      const struct ubifs_idx_node *idx,
				      int bnum)
{
	return (struct ubifs_branch *)((void *)idx->branches +
				       (UBIFS_BRANCH_SZ + c->key_len) * bnum);
}

#endif /* __UBIFS_H__ */
