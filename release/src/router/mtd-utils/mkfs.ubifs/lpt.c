/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006, 2007 Nokia Corporation.
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
 * Authors: Adrian Hunter
 *          Artem Bityutskiy
 */

#include "mkfs.ubifs.h"

/**
 * do_calc_lpt_geom - calculate sizes for the LPT area.
 * @c: the UBIFS file-system description object
 *
 * Calculate the sizes of LPT bit fields, nodes, and tree, based on the
 * properties of the flash and whether LPT is "big" (c->big_lpt).
 */
static void do_calc_lpt_geom(struct ubifs_info *c)
{
	int n, bits, per_leb_wastage;
	long long sz, tot_wastage;

	c->pnode_cnt = (c->main_lebs + UBIFS_LPT_FANOUT - 1) / UBIFS_LPT_FANOUT;

	n = (c->pnode_cnt + UBIFS_LPT_FANOUT - 1) / UBIFS_LPT_FANOUT;
	c->nnode_cnt = n;
	while (n > 1) {
		n = (n + UBIFS_LPT_FANOUT - 1) / UBIFS_LPT_FANOUT;
		c->nnode_cnt += n;
	}

	c->lpt_hght = 1;
	n = UBIFS_LPT_FANOUT;
	while (n < c->pnode_cnt) {
		c->lpt_hght += 1;
		n <<= UBIFS_LPT_FANOUT_SHIFT;
	}

	c->space_bits = fls(c->leb_size) - 3;
	c->lpt_lnum_bits = fls(c->lpt_lebs);
	c->lpt_offs_bits = fls(c->leb_size - 1);
	c->lpt_spc_bits = fls(c->leb_size);

	n = (c->max_leb_cnt + UBIFS_LPT_FANOUT - 1) / UBIFS_LPT_FANOUT;
	c->pcnt_bits = fls(n - 1);

	c->lnum_bits = fls(c->max_leb_cnt - 1);

	bits = UBIFS_LPT_CRC_BITS + UBIFS_LPT_TYPE_BITS +
	       (c->big_lpt ? c->pcnt_bits : 0) +
	       (c->space_bits * 2 + 1) * UBIFS_LPT_FANOUT;
	c->pnode_sz = (bits + 7) / 8;

	bits = UBIFS_LPT_CRC_BITS + UBIFS_LPT_TYPE_BITS +
	       (c->big_lpt ? c->pcnt_bits : 0) +
	       (c->lpt_lnum_bits + c->lpt_offs_bits) * UBIFS_LPT_FANOUT;
	c->nnode_sz = (bits + 7) / 8;

	bits = UBIFS_LPT_CRC_BITS + UBIFS_LPT_TYPE_BITS +
	       c->lpt_lebs * c->lpt_spc_bits * 2;
	c->ltab_sz = (bits + 7) / 8;

	bits = UBIFS_LPT_CRC_BITS + UBIFS_LPT_TYPE_BITS +
	       c->lnum_bits * c->lsave_cnt;
	c->lsave_sz = (bits + 7) / 8;

	/* Calculate the minimum LPT size */
	c->lpt_sz = (long long)c->pnode_cnt * c->pnode_sz;
	c->lpt_sz += (long long)c->nnode_cnt * c->nnode_sz;
	c->lpt_sz += c->ltab_sz;
	c->lpt_sz += c->lsave_sz;

	/* Add wastage */
	sz = c->lpt_sz;
	per_leb_wastage = max_t(int, c->pnode_sz, c->nnode_sz);
	sz += per_leb_wastage;
	tot_wastage = per_leb_wastage;
	while (sz > c->leb_size) {
		sz += per_leb_wastage;
		sz -= c->leb_size;
		tot_wastage += per_leb_wastage;
	}
	tot_wastage += ALIGN(sz, c->min_io_size) - sz;
	c->lpt_sz += tot_wastage;
}

/**
 * calc_dflt_lpt_geom - calculate default LPT geometry.
 * @c: the UBIFS file-system description object
 * @main_lebs: number of main area LEBs is passed and returned here
 * @big_lpt: whether the LPT area is "big" is returned here
 *
 * The size of the LPT area depends on parameters that themselves are dependent
 * on the size of the LPT area. This function, successively recalculates the LPT
 * area geometry until the parameters and resultant geometry are consistent.
 *
 * This function returns %0 on success and a negative error code on failure.
 */
int calc_dflt_lpt_geom(struct ubifs_info *c, int *main_lebs, int *big_lpt)
{
	int i, lebs_needed;
	long long sz;

	/* Start by assuming the minimum number of LPT LEBs */
	c->lpt_lebs = UBIFS_MIN_LPT_LEBS;
	c->main_lebs = *main_lebs - c->lpt_lebs;
	if (c->main_lebs <= 0)
		return -EINVAL;

	/* And assume we will use the small LPT model */
	c->big_lpt = 0;

	/*
	 * Calculate the geometry based on assumptions above and then see if it
	 * makes sense
	 */
	do_calc_lpt_geom(c);

	/* Small LPT model must have lpt_sz < leb_size */
	if (c->lpt_sz > c->leb_size) {
		/* Nope, so try again using big LPT model */
		c->big_lpt = 1;
		do_calc_lpt_geom(c);
	}

	/* Now check there are enough LPT LEBs */
	for (i = 0; i < 64 ; i++) {
		sz = c->lpt_sz * 4; /* Allow 4 times the size */
		sz += c->leb_size - 1;
		do_div(sz, c->leb_size);
		lebs_needed = sz;
		if (lebs_needed > c->lpt_lebs) {
			/* Not enough LPT LEBs so try again with more */
			c->lpt_lebs = lebs_needed;
			c->main_lebs = *main_lebs - c->lpt_lebs;
			if (c->main_lebs <= 0)
				return -EINVAL;
			do_calc_lpt_geom(c);
			continue;
		}
		if (c->ltab_sz > c->leb_size) {
			err_msg("LPT ltab too big");
			return -EINVAL;
		}
		*main_lebs = c->main_lebs;
		*big_lpt = c->big_lpt;
		return 0;
	}
	return -EINVAL;
}

/**
 * pack_bits - pack bit fields end-to-end.
 * @addr: address at which to pack (passed and next address returned)
 * @pos: bit position at which to pack (passed and next position returned)
 * @val: value to pack
 * @nrbits: number of bits of value to pack (1-32)
 */
static void pack_bits(uint8_t **addr, int *pos, uint32_t val, int nrbits)
{
	uint8_t *p = *addr;
	int b = *pos;

	if (b) {
		*p |= ((uint8_t)val) << b;
		nrbits += b;
		if (nrbits > 8) {
			*++p = (uint8_t)(val >>= (8 - b));
			if (nrbits > 16) {
				*++p = (uint8_t)(val >>= 8);
				if (nrbits > 24) {
					*++p = (uint8_t)(val >>= 8);
					if (nrbits > 32)
						*++p = (uint8_t)(val >>= 8);
				}
			}
		}
	} else {
		*p = (uint8_t)val;
		if (nrbits > 8) {
			*++p = (uint8_t)(val >>= 8);
			if (nrbits > 16) {
				*++p = (uint8_t)(val >>= 8);
				if (nrbits > 24)
					*++p = (uint8_t)(val >>= 8);
			}
		}
	}
	b = nrbits & 7;
	if (b == 0)
		p++;
	*addr = p;
	*pos = b;
}

/**
 * pack_pnode - pack all the bit fields of a pnode.
 * @c: UBIFS file-system description object
 * @buf: buffer into which to pack
 * @pnode: pnode to pack
 */
static void pack_pnode(struct ubifs_info *c, void *buf,
		       struct ubifs_pnode *pnode)
{
	uint8_t *addr = buf + UBIFS_LPT_CRC_BYTES;
	int i, pos = 0;
	uint16_t crc;

	pack_bits(&addr, &pos, UBIFS_LPT_PNODE, UBIFS_LPT_TYPE_BITS);
	if (c->big_lpt)
		pack_bits(&addr, &pos, pnode->num, c->pcnt_bits);
	for (i = 0; i < UBIFS_LPT_FANOUT; i++) {
		pack_bits(&addr, &pos, pnode->lprops[i].free >> 3,
			  c->space_bits);
		pack_bits(&addr, &pos, pnode->lprops[i].dirty >> 3,
			  c->space_bits);
		if (pnode->lprops[i].flags & LPROPS_INDEX)
			pack_bits(&addr, &pos, 1, 1);
		else
			pack_bits(&addr, &pos, 0, 1);
	}
	crc = crc16(-1, buf + UBIFS_LPT_CRC_BYTES,
		    c->pnode_sz - UBIFS_LPT_CRC_BYTES);
	addr = buf;
	pos = 0;
	pack_bits(&addr, &pos, crc, UBIFS_LPT_CRC_BITS);
}

/**
 * pack_nnode - pack all the bit fields of a nnode.
 * @c: UBIFS file-system description object
 * @buf: buffer into which to pack
 * @nnode: nnode to pack
 */
static void pack_nnode(struct ubifs_info *c, void *buf,
		       struct ubifs_nnode *nnode)
{
	uint8_t *addr = buf + UBIFS_LPT_CRC_BYTES;
	int i, pos = 0;
	uint16_t crc;

	pack_bits(&addr, &pos, UBIFS_LPT_NNODE, UBIFS_LPT_TYPE_BITS);
	if (c->big_lpt)
		pack_bits(&addr, &pos, nnode->num, c->pcnt_bits);
	for (i = 0; i < UBIFS_LPT_FANOUT; i++) {
		int lnum = nnode->nbranch[i].lnum;

		if (lnum == 0)
			lnum = c->lpt_last + 1;
		pack_bits(&addr, &pos, lnum - c->lpt_first, c->lpt_lnum_bits);
		pack_bits(&addr, &pos, nnode->nbranch[i].offs,
			  c->lpt_offs_bits);
	}
	crc = crc16(-1, buf + UBIFS_LPT_CRC_BYTES,
		    c->nnode_sz - UBIFS_LPT_CRC_BYTES);
	addr = buf;
	pos = 0;
	pack_bits(&addr, &pos, crc, UBIFS_LPT_CRC_BITS);
}

/**
 * pack_ltab - pack the LPT's own lprops table.
 * @c: UBIFS file-system description object
 * @buf: buffer into which to pack
 * @ltab: LPT's own lprops table to pack
 */
static void pack_ltab(struct ubifs_info *c, void *buf,
			 struct ubifs_lpt_lprops *ltab)
{
	uint8_t *addr = buf + UBIFS_LPT_CRC_BYTES;
	int i, pos = 0;
	uint16_t crc;

	pack_bits(&addr, &pos, UBIFS_LPT_LTAB, UBIFS_LPT_TYPE_BITS);
	for (i = 0; i < c->lpt_lebs; i++) {
		pack_bits(&addr, &pos, ltab[i].free, c->lpt_spc_bits);
		pack_bits(&addr, &pos, ltab[i].dirty, c->lpt_spc_bits);
	}
	crc = crc16(-1, buf + UBIFS_LPT_CRC_BYTES,
		    c->ltab_sz - UBIFS_LPT_CRC_BYTES);
	addr = buf;
	pos = 0;
	pack_bits(&addr, &pos, crc, UBIFS_LPT_CRC_BITS);
}

/**
 * pack_lsave - pack the LPT's save table.
 * @c: UBIFS file-system description object
 * @buf: buffer into which to pack
 * @lsave: LPT's save table to pack
 */
static void pack_lsave(struct ubifs_info *c, void *buf, int *lsave)
{
	uint8_t *addr = buf + UBIFS_LPT_CRC_BYTES;
	int i, pos = 0;
	uint16_t crc;

	pack_bits(&addr, &pos, UBIFS_LPT_LSAVE, UBIFS_LPT_TYPE_BITS);
	for (i = 0; i < c->lsave_cnt; i++)
		pack_bits(&addr, &pos, lsave[i], c->lnum_bits);
	crc = crc16(-1, buf + UBIFS_LPT_CRC_BYTES,
		    c->lsave_sz - UBIFS_LPT_CRC_BYTES);
	addr = buf;
	pos = 0;
	pack_bits(&addr, &pos, crc, UBIFS_LPT_CRC_BITS);
}

/**
 * set_ltab - set LPT LEB properties.
 * @c: UBIFS file-system description object
 * @lnum: LEB number
 * @free: amount of free space
 * @dirty: amount of dirty space
 */
static void set_ltab(struct ubifs_info *c, int lnum, int free, int dirty)
{
	dbg_msg(3, "LEB %d free %d dirty %d to %d %d",
		lnum, c->ltab[lnum - c->lpt_first].free,
		c->ltab[lnum - c->lpt_first].dirty, free, dirty);
	c->ltab[lnum - c->lpt_first].free = free;
	c->ltab[lnum - c->lpt_first].dirty = dirty;
}

/**
 * calc_nnode_num - calculate nnode number.
 * @row: the row in the tree (root is zero)
 * @col: the column in the row (leftmost is zero)
 *
 * The nnode number is a number that uniquely identifies a nnode and can be used
 * easily to traverse the tree from the root to that nnode.
 *
 * This function calculates and returns the nnode number for the nnode at @row
 * and @col.
 */
static int calc_nnode_num(int row, int col)
{
	int num, bits;

	num = 1;
	while (row--) {
		bits = (col & (UBIFS_LPT_FANOUT - 1));
		col >>= UBIFS_LPT_FANOUT_SHIFT;
		num <<= UBIFS_LPT_FANOUT_SHIFT;
		num |= bits;
	}
	return num;
}

/**
 * create_lpt - create LPT.
 * @c: UBIFS file-system description object
 *
 * This function returns %0 on success and a negative error code on failure.
 */
int create_lpt(struct ubifs_info *c)
{
	int lnum, err = 0, i, j, cnt, len, alen, row;
	int blnum, boffs, bsz, bcnt;
	struct ubifs_pnode *pnode = NULL;
	struct ubifs_nnode *nnode = NULL;
	void *buf = NULL, *p;
	int *lsave = NULL;

	pnode = malloc(sizeof(struct ubifs_pnode));
	nnode = malloc(sizeof(struct ubifs_nnode));
	buf = malloc(c->leb_size);
	lsave = malloc(sizeof(int) * c->lsave_cnt);
	if (!pnode || !nnode || !buf || !lsave) {
		err = -ENOMEM;
		goto out;
	}
	memset(pnode, 0 , sizeof(struct ubifs_pnode));
	memset(nnode, 0 , sizeof(struct ubifs_pnode));

	c->lscan_lnum = c->main_first;

	lnum = c->lpt_first;
	p = buf;
	len = 0;
	/* Number of leaf nodes (pnodes) */
	cnt = (c->main_lebs + UBIFS_LPT_FANOUT - 1) >> UBIFS_LPT_FANOUT_SHIFT;
	//printf("pnode_cnt=%d\n",cnt);

	/*
	 * To calculate the internal node branches, we keep information about
	 * the level below.
	 */
	blnum = lnum; /* LEB number of level below */
	boffs = 0; /* Offset of level below */
	bcnt = cnt; /* Number of nodes in level below */
	bsz = c->pnode_sz; /* Size of nodes in level below */

	/* Add pnodes */
	for (i = 0; i < cnt; i++) {
		if (len + c->pnode_sz > c->leb_size) {
			alen = ALIGN(len, c->min_io_size);
			set_ltab(c, lnum, c->leb_size - alen, alen - len);
			memset(p, 0xff, alen - len);
			err = write_leb(lnum++, alen, buf, UBI_SHORTTERM);
			if (err)
				goto out;
			p = buf;
			len = 0;
		}
		/* Fill in the pnode */
		for (j = 0; j < UBIFS_LPT_FANOUT; j++) {
			int k = (i << UBIFS_LPT_FANOUT_SHIFT) + j;

			if (k < c->main_lebs)
				pnode->lprops[j] = c->lpt[k];
			else {
				pnode->lprops[j].free = c->leb_size;
				pnode->lprops[j].dirty = 0;
				pnode->lprops[j].flags = 0;
			}
		}
		pack_pnode(c, p, pnode);
		p += c->pnode_sz;
		len += c->pnode_sz;
		/*
		 * pnodes are simply numbered left to right starting at zero,
		 * which means the pnode number can be used easily to traverse
		 * down the tree to the corresponding pnode.
		 */
		pnode->num += 1;
	}

	row = c->lpt_hght - 1;
	/* Add all nnodes, one level at a time */
	while (1) {
		/* Number of internal nodes (nnodes) at next level */
		cnt = (cnt + UBIFS_LPT_FANOUT - 1) / UBIFS_LPT_FANOUT;
		if (cnt == 0)
			cnt = 1;
		for (i = 0; i < cnt; i++) {
			if (len + c->nnode_sz > c->leb_size) {
				alen = ALIGN(len, c->min_io_size);
				set_ltab(c, lnum, c->leb_size - alen,
					    alen - len);
				memset(p, 0xff, alen - len);
				err = write_leb(lnum++, alen, buf, UBI_SHORTTERM);
				if (err)
					goto out;
				p = buf;
				len = 0;
			}
			/* The root is on row zero */
			if (row == 0) {
				c->lpt_lnum = lnum;
				c->lpt_offs = len;
			}
			/* Set branches to the level below */
			for (j = 0; j < UBIFS_LPT_FANOUT; j++) {
				if (bcnt) {
					if (boffs + bsz > c->leb_size) {
						blnum += 1;
						boffs = 0;
					}
					nnode->nbranch[j].lnum = blnum;
					nnode->nbranch[j].offs = boffs;
					boffs += bsz;
					bcnt--;
				} else {
					nnode->nbranch[j].lnum = 0;
					nnode->nbranch[j].offs = 0;
				}
			}
			nnode->num = calc_nnode_num(row, i);
			pack_nnode(c, p, nnode);
			p += c->nnode_sz;
			len += c->nnode_sz;
		}
		/* Row zero  is the top row */
		if (row == 0)
			break;
		/* Update the information about the level below */
		bcnt = cnt;
		bsz = c->nnode_sz;
		row -= 1;
	}

	if (c->big_lpt) {
		/* Need to add LPT's save table */
		if (len + c->lsave_sz > c->leb_size) {
			alen = ALIGN(len, c->min_io_size);
			set_ltab(c, lnum, c->leb_size - alen, alen - len);
			memset(p, 0xff, alen - len);
			err = write_leb(lnum++, alen, buf, UBI_SHORTTERM);
			if (err)
				goto out;
			p = buf;
			len = 0;
		}

		c->lsave_lnum = lnum;
		c->lsave_offs = len;

		for (i = 0; i < c->lsave_cnt; i++)
			lsave[i] = c->main_first + i;

		pack_lsave(c, p, lsave);
		p += c->lsave_sz;
		len += c->lsave_sz;
	}

	/* Need to add LPT's own LEB properties table */
	if (len + c->ltab_sz > c->leb_size) {
		alen = ALIGN(len, c->min_io_size);
		set_ltab(c, lnum, c->leb_size - alen, alen - len);
		memset(p, 0xff, alen - len);
		err = write_leb(lnum++, alen, buf, UBI_SHORTTERM);
		if (err)
			goto out;
		p = buf;
		len = 0;
	}

	c->ltab_lnum = lnum;
	c->ltab_offs = len;

	/* Update ltab before packing it */
	len += c->ltab_sz;
	alen = ALIGN(len, c->min_io_size);
	set_ltab(c, lnum, c->leb_size - alen, alen - len);

	pack_ltab(c, p, c->ltab);
	p += c->ltab_sz;

	/* Write remaining buffer */
	memset(p, 0xff, alen - len);
	err = write_leb(lnum, alen, buf, UBI_SHORTTERM);
	if (err)
		goto out;

	c->nhead_lnum = lnum;
	c->nhead_offs = ALIGN(len, c->min_io_size);

	dbg_msg(1, "lpt_sz:         %lld", c->lpt_sz);
	dbg_msg(1, "space_bits:     %d", c->space_bits);
	dbg_msg(1, "lpt_lnum_bits:  %d", c->lpt_lnum_bits);
	dbg_msg(1, "lpt_offs_bits:  %d", c->lpt_offs_bits);
	dbg_msg(1, "lpt_spc_bits:   %d", c->lpt_spc_bits);
	dbg_msg(1, "pcnt_bits:      %d", c->pcnt_bits);
	dbg_msg(1, "lnum_bits:      %d", c->lnum_bits);
	dbg_msg(1, "pnode_sz:       %d", c->pnode_sz);
	dbg_msg(1, "nnode_sz:       %d", c->nnode_sz);
	dbg_msg(1, "ltab_sz:        %d", c->ltab_sz);
	dbg_msg(1, "lsave_sz:       %d", c->lsave_sz);
	dbg_msg(1, "lsave_cnt:      %d", c->lsave_cnt);
	dbg_msg(1, "lpt_hght:       %d", c->lpt_hght);
	dbg_msg(1, "big_lpt:        %d", c->big_lpt);
	dbg_msg(1, "LPT root is at  %d:%d", c->lpt_lnum, c->lpt_offs);
	dbg_msg(1, "LPT head is at  %d:%d", c->nhead_lnum, c->nhead_offs);
	dbg_msg(1, "LPT ltab is at  %d:%d", c->ltab_lnum, c->ltab_offs);
	if (c->big_lpt)
		dbg_msg(1, "LPT lsave is at %d:%d",
		        c->lsave_lnum, c->lsave_offs);
out:
	free(lsave);
	free(buf);
	free(nnode);
	free(pnode);
	return err;
}
