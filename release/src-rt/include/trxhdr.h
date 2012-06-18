/*
 * TRX image file header format.
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: trxhdr.h,v 13.16.10.1 2010-09-07 19:23:08 Exp $
 */

#ifndef _TRX_HDR_H
#define _TRX_HDR_H

#include <typedefs.h>

#define TRX_MAGIC	0x30524448	/* "HDR0" */
#define TRX_VERSION	1		/* Version 1 */
#define TRX_MAX_LEN	0x3B0000	/* Max length */
#define TRX_NO_HEADER	1		/* Do not write TRX header */
#define TRX_GZ_FILES	0x2     /* Contains up to TRX_MAX_OFFSET individual gzip files */
#define TRX_OVERLAYS	0x4     /* Contains an overlay header after the trx header */
#define TRX_MAX_OFFSET	3		/* Max number of individual files */
#define TRX_UNCOMP_IMAGE	0x20	/* Trx contains uncompressed rtecdc.bin image */

struct trx_header {
	uint32 magic;		/* "HDR0" */
	uint32 len;		/* Length of file including header */
	uint32 crc32;		/* 32-bit CRC from flag_version to end of file */
	uint32 flag_version;	/* 0:15 flags, 16:31 version */
	uint32 offsets[TRX_MAX_OFFSET];	/* Offsets of partitions from start of header */
};

/* Compatibility */
typedef struct trx_header TRXHDR, *PTRXHDR;

#endif /* _TRX_HDR_H */
