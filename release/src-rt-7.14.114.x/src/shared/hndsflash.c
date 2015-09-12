/*
 * Broadcom chipcommon NAND flash interface
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
 * $Id:$
 */

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <hndsoc.h>
#include <sbhndcpu.h>
#include <sbchipc.h>
#include <bcmdevs.h>
#include <hndsflash.h>

/* Private global state */
static hndsflash_t *hndsflash;

hndsflash_t *ccsflash_init(si_t *sih);
hndsflash_t *spiflash_init(si_t *sih);

/* Initialize nand flash access */
hndsflash_t *
hndsflash_init(si_t *sih)
{
	uint32 origidx;

	ASSERT(sih);

	/* Already initialized ? */
	if (hndsflash)
		return hndsflash;

	/* spin lock here */
	origidx = si_coreidx(sih);

#ifdef	__mips__
	if (!hndsflash)
		hndsflash = ccsflash_init(sih);
#endif	/* __mips__ */
#ifdef __ARM_ARCH_7A__
	if (!hndsflash)
		hndsflash = spiflash_init(sih);
#endif	/* __ARM_ARCH_7A__ */

	si_setcoreidx(sih, origidx);
	return hndsflash;
}

/* Read len bytes starting at offset into buf. Returns number of bytes read. */
int
hndsflash_read(hndsflash_t *sfl, uint offset, uint len, const uchar *buf)
{
	ASSERT(sfl);
	ASSERT(sfl->read);

	return (sfl->read)(sfl, offset, len, buf);
}

/* Write len bytes starting at offset into buf. Returns number of bytes
 * written.
 */
int
hndsflash_write(hndsflash_t *sfl, uint offset, uint len, const uchar *buf)
{
	ASSERT(sfl);
	ASSERT(sfl->write);

	return (sfl->write)(sfl, offset, len, (const uchar *)buf);
}

/* Erase a region. Returns number of bytes scheduled for erasure.
 * Caller should poll for completion.
 */
int
hndsflash_erase(hndsflash_t *sfl, uint offset)
{
	ASSERT(sfl);
	ASSERT(sfl->erase);

	return (sfl->erase)(sfl, offset);
}

/*
 * writes the appropriate range of flash, a NULL buf simply erases
 * the region of flash
 */
int hndsflash_commit(hndsflash_t *sfl, uint offset, uint len, const uchar *buf)
{
	ASSERT(sfl);
	ASSERT(sfl->commit);

	return (sfl->commit)(sfl, offset, len, (const uchar *)buf);
}

/* Poll for command completion. Returns zero when complete. */
int hndsflash_poll(hndsflash_t *sfl, uint offset)
{
	ASSERT(sfl);

	if (!sfl->poll)
		return 0;

	return (sfl->poll)(sfl, offset);
}
