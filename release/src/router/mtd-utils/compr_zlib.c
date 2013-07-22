/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * The original JFFS, from which the design for JFFS2 was derived,
 * was designed and implemented by Axis Communications AB.
 *
 * The contents of this file are subject to the Red Hat eCos Public
 * License Version 1.1 (the "Licence"); you may not use this file
 * except in compliance with the Licence.  You may obtain a copy of
 * the Licence at http://www.redhat.com/
 *
 * Software distributed under the Licence is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing rights and
 * limitations under the Licence.
 *
 * The Original Code is JFFS2 - Journalling Flash File System, version 2
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the RHEPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the RHEPL or the GPL.
 */

#define PROGRAM_NAME "compr_zlib"

#include <stdint.h>
#define crc32 __zlib_crc32
#include <zlib.h>
#undef crc32
#include <stdio.h>
#include <asm/types.h>
#include <linux/jffs2.h>
#include "common.h"
#include "compr.h"

/* Plan: call deflate() with avail_in == *sourcelen,
   avail_out = *dstlen - 12 and flush == Z_FINISH.
   If it doesn't manage to finish,	call it again with
   avail_in == 0 and avail_out set to the remaining 12
   bytes for it to clean up.
Q: Is 12 bytes sufficient?
 */
#define STREAM_END_SPACE 12

static int jffs2_zlib_compress(unsigned char *data_in, unsigned char *cpage_out,
		uint32_t *sourcelen, uint32_t *dstlen)
{
	z_stream strm;
	int ret;

	if (*dstlen <= STREAM_END_SPACE)
		return -1;

	strm.zalloc = (void *)0;
	strm.zfree = (void *)0;

	if (Z_OK != deflateInit(&strm, 3)) {
		return -1;
	}
	strm.next_in = data_in;
	strm.total_in = 0;

	strm.next_out = cpage_out;
	strm.total_out = 0;

	while (strm.total_out < *dstlen - STREAM_END_SPACE && strm.total_in < *sourcelen) {
		strm.avail_out = *dstlen - (strm.total_out + STREAM_END_SPACE);
		strm.avail_in = min((unsigned)(*sourcelen-strm.total_in), strm.avail_out);
		ret = deflate(&strm, Z_PARTIAL_FLUSH);
		if (ret != Z_OK) {
			deflateEnd(&strm);
			return -1;
		}
	}
	strm.avail_out += STREAM_END_SPACE;
	strm.avail_in = 0;
	ret = deflate(&strm, Z_FINISH);
	if (ret != Z_STREAM_END) {
		deflateEnd(&strm);
		return -1;
	}
	deflateEnd(&strm);

	if (strm.total_out >= strm.total_in)
		return -1;


	*dstlen = strm.total_out;
	*sourcelen = strm.total_in;
	return 0;
}

static int jffs2_zlib_decompress(unsigned char *data_in, unsigned char *cpage_out,
		uint32_t srclen, uint32_t destlen)
{
	z_stream strm;
	int ret;

	strm.zalloc = (void *)0;
	strm.zfree = (void *)0;

	if (Z_OK != inflateInit(&strm)) {
		return 1;
	}
	strm.next_in = data_in;
	strm.avail_in = srclen;
	strm.total_in = 0;

	strm.next_out = cpage_out;
	strm.avail_out = destlen;
	strm.total_out = 0;

	while((ret = inflate(&strm, Z_FINISH)) == Z_OK)
		;

	inflateEnd(&strm);
	return 0;
}

static struct jffs2_compressor jffs2_zlib_comp = {
	.priority = JFFS2_ZLIB_PRIORITY,
	.name = "zlib",
	.disabled = 0,
	.compr = JFFS2_COMPR_ZLIB,
	.compress = &jffs2_zlib_compress,
	.decompress = &jffs2_zlib_decompress,
};

int jffs2_zlib_init(void)
{
	return jffs2_register_compressor(&jffs2_zlib_comp);
}

void jffs2_zlib_exit(void)
{
	jffs2_unregister_compressor(&jffs2_zlib_comp);
}
