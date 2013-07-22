/*
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

#ifndef __UBIFS_COMPRESS_H__
#define __UBIFS_COMPRESS_H__

/*
 * Compressors may end-up with more data in the output buffer than in the input
 * buffer. This constant defined the worst case factor, i.e. we assume that the
 * output buffer may be at max. WORST_COMPR_FACTOR times larger than input
 * buffer.
 */
#define WORST_COMPR_FACTOR 4

enum compression_type
{
	MKFS_UBIFS_COMPR_NONE,
	MKFS_UBIFS_COMPR_LZO,
	MKFS_UBIFS_COMPR_ZLIB,
};

int compress_data(void *in_buf, size_t in_len, void *out_buf, size_t *out_len,
		  int type);
int init_compression(void);
void destroy_compression(void);

#endif
