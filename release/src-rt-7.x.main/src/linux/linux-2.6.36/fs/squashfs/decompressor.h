/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
#ifndef DECOMPRESSOR_H
#define DECOMPRESSOR_H
/*
 * Squashfs - a compressed read only filesystem for Linux
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009
 * Phillip Lougher <phillip@lougher.demon.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * decompressor.h
 */

struct squashfs_decompressor {
	void	*(*init)(struct squashfs_sb_info *, void *, int);
	void	(*free)(void *);
	int	(*decompress)(struct squashfs_sb_info *, void **,
		struct buffer_head **, int, int, int, int, int);
	int	id;
	char	*name;
	int	supported;
};

static inline void squashfs_decompressor_free(struct squashfs_sb_info *msblk,
	void *s)
{
	if (msblk->decompressor)
		msblk->decompressor->free(s);
}

static inline int squashfs_decompress(struct squashfs_sb_info *msblk,
	void **buffer, struct buffer_head **bh, int b, int offset, int length,
	int srclength, int pages)
{
	return msblk->decompressor->decompress(msblk, buffer, bh, b, offset,
		length, srclength, pages);
}

#ifdef CONFIG_SQUASHFS_XZ
extern const struct squashfs_decompressor squashfs_xz_comp_ops;
#endif

#ifdef CONFIG_SQUASHFS_LZO
extern const struct squashfs_decompressor squashfs_lzo_comp_ops;
#endif

#endif
