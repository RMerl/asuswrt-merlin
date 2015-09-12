/*
 * Broadcom chipcommon sflash interface
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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
 * $Id: $
 */

#ifndef _hndsflash_h_
#define _hndsflash_h_

#include <typedefs.h>
#include <sbchipc.h>

struct hndsflash;
typedef struct hndsflash hndsflash_t;

struct hndsflash {
	uint blocksize;		/* Block size */
	uint numblocks;		/* Number of blocks */
	uint32 type;		/* Type */
	uint size;		/* Total size in bytes */
	uint32 base;
	uint32 phybase;
	uint8 vendor_id;
	uint16 device_id;

	si_t *sih;
	void *core;
	int (*read)(hndsflash_t *sfl, uint offset, uint len, const uchar *buf);
	int (*write)(hndsflash_t *sfl, uint offset, uint len, const uchar *buf);
	int (*erase)(hndsflash_t *sfl, uint offset);
	int (*commit)(hndsflash_t *sfl, uint offset, uint len, const uchar *buf);
	int (*poll)(hndsflash_t *sfl, uint offset);
};

hndsflash_t *hndsflash_init(si_t *sih);
int hndsflash_read(hndsflash_t *sfl, uint offset, uint len, const uchar *buf);
int hndsflash_write(hndsflash_t *sfl, uint offset, uint len, const uchar *buf);
int hndsflash_erase(hndsflash_t *sfl, uint offset);
int hndsflash_commit(hndsflash_t *sfl, uint offset, uint len, const uchar *buf);
int hndsflash_poll(hndsflash_t *sfl, uint offset);

#endif /* _hndsflash_h_ */
