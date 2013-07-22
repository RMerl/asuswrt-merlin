/*
 * JFFS2 LZO Compression Interface.
 *
 * Copyright (C) 2007 Nokia Corporation. All rights reserved.
 *
 * Author: Richard Purdie <rpurdie@openedhand.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef WITHOUT_LZO
#include <asm/types.h>
#include <linux/jffs2.h>
#include <lzo/lzo1x.h>
#include "compr.h"

extern int page_size;

static void *lzo_mem;
static void *lzo_compress_buf;

/*
 * Note about LZO compression.
 *
 * We want to use the _999_ compression routine which gives better compression
 * rates at the expense of time. Decompression time is unaffected. We might as
 * well use the standard lzo library routines for this but they will overflow
 * the destination buffer since they don't check the destination size.
 *
 * We therefore compress to a temporary buffer and copy if it will fit.
 *
 */
static int jffs2_lzo_cmpr(unsigned char *data_in, unsigned char *cpage_out,
			  uint32_t *sourcelen, uint32_t *dstlen)
{
	lzo_uint compress_size;
	int ret;

	ret = lzo1x_999_compress(data_in, *sourcelen, lzo_compress_buf, &compress_size, lzo_mem);

	if (ret != LZO_E_OK)
		return -1;

	if (compress_size > *dstlen)
		return -1;

	memcpy(cpage_out, lzo_compress_buf, compress_size);
	*dstlen = compress_size;

	return 0;
}

static int jffs2_lzo_decompress(unsigned char *data_in, unsigned char *cpage_out,
				 uint32_t srclen, uint32_t destlen)
{
	int ret;
	lzo_uint dl;

	ret = lzo1x_decompress_safe(data_in,srclen,cpage_out,&dl,NULL);

	if (ret != LZO_E_OK || dl != destlen)
		return -1;

	return 0;
}

static struct jffs2_compressor jffs2_lzo_comp = {
	.priority = JFFS2_LZO_PRIORITY,
	.name = "lzo",
	.compr = JFFS2_COMPR_LZO,
	.compress = &jffs2_lzo_cmpr,
	.decompress = &jffs2_lzo_decompress,
	.disabled = 1,
};

int jffs2_lzo_init(void)
{
	int ret;

	lzo_mem = malloc(LZO1X_999_MEM_COMPRESS);
	if (!lzo_mem)
		return -1;

	/* Worse case LZO compression size from their FAQ */
	lzo_compress_buf = malloc(page_size + (page_size / 16) + 64 + 3);
	if (!lzo_compress_buf) {
		free(lzo_mem);
		return -1;
	}

	ret = jffs2_register_compressor(&jffs2_lzo_comp);
	if (ret < 0) {
		free(lzo_compress_buf);
		free(lzo_mem);
	}

	return ret;
}

void jffs2_lzo_exit(void)
{
	jffs2_unregister_compressor(&jffs2_lzo_comp);
	free(lzo_compress_buf);
	free(lzo_mem);
}

#else

int jffs2_lzo_init(void)
{
	return 0;
}

void jffs2_lzo_exit(void)
{
}

#endif
