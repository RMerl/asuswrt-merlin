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
#include <hndnand.h>
#include <hndpmu.h>

/* Private global state */
static hndnand_t *hndnand = NULL;

extern hndnand_t *nflash_init(si_t *sih);
extern hndnand_t *nandcore_init(si_t *sih);

/* Initialize nand flash access */
hndnand_t *
hndnand_init(si_t *sih)
{
	uint32 origidx;

	ASSERT(sih);

	/* Already initialized ? */
	if (hndnand)
		return hndnand;

	origidx = si_coreidx(sih);

#ifdef	__mips__
	if (!hndnand)
		hndnand = nflash_init(sih);
#endif
#ifdef __ARM_ARCH_7A__
	if (!hndnand)
		hndnand = nandcore_init(sih);
#endif

	si_setcoreidx(sih, origidx);
	return hndnand;
}

void
hndnand_enable(hndnand_t *nfl, int enable)
{
	ASSERT(nfl);

	if (nfl->enable) {
		/* Should spinlock here */
		(nfl->enable)(nfl, enable);
	}

	return;
}

/* Read len bytes starting at offset into buf. Returns number of bytes read. */
int
hndnand_read(hndnand_t *nfl, uint64 offset, uint len, uchar *buf)
{
	ASSERT(nfl);
	ASSERT(nfl->read);

	return (nfl->read)(nfl, offset, len, buf);
}

/* Write len bytes starting at offset into buf. Returns number of bytes
 * written.
 */
int
hndnand_write(hndnand_t *nfl, uint64 offset, uint len, const uchar *buf)
{
	ASSERT(nfl);
	ASSERT(nfl->write);

	return (nfl->write)(nfl, offset, len, buf);
}

/* Erase a region. Returns number of bytes scheduled for erasure.
 * Caller should poll for completion.
 */
int
hndnand_erase(hndnand_t *nfl, uint64 offset)
{
	ASSERT(nfl);
	ASSERT(nfl->erase);

	return (nfl->erase)(nfl, offset);
}

int
hndnand_checkbadb(hndnand_t *nfl, uint64 offset)
{
	ASSERT(nfl);
	ASSERT(nfl->checkbadb);

	return (nfl->checkbadb)(nfl, offset);
}

int
hndnand_mark_badb(hndnand_t *nfl, uint64 offset)
{
	ASSERT(nfl);
	ASSERT(nfl->markbadb);

	return (nfl->markbadb)(nfl, offset);
}

#ifndef _CFE_
int
hndnand_dev_ready(hndnand_t *nfl)
{
	ASSERT(nfl);
	ASSERT(nfl->dev_ready);

	return (nfl->dev_ready)(nfl);
}

int
hndnand_select_chip(hndnand_t *nfl, int chip)
{
	ASSERT(nfl);
	ASSERT(nfl->select_chip);

	return (nfl->select_chip)(nfl, chip);
}

int hndnand_cmdfunc(hndnand_t *nfl, uint64 addr, int cmd)
{
	ASSERT(nfl);
	ASSERT(nfl->cmdfunc);

	return (nfl->cmdfunc)(nfl, addr, cmd);
}

int
hndnand_waitfunc(hndnand_t *nfl, int *status)
{
	ASSERT(nfl);
	ASSERT(nfl->waitfunc);

	return (nfl->waitfunc)(nfl, status);
}

int
hndnand_read_oob(hndnand_t *nfl, uint64 addr, uint8 *oob)
{
	ASSERT(nfl);
	ASSERT(nfl->read_oob);

	return (nfl->read_oob)(nfl, addr, oob);
}

int
hndnand_write_oob(hndnand_t *nfl, uint64 addr, uint8 *oob)
{
	ASSERT(nfl);
	ASSERT(nfl->write_oob);

	return (nfl->write_oob)(nfl, addr, oob);
}
int
hndnand_read_page(hndnand_t *nfl, uint64 addr, uint8 *buf, uint8 *oob, bool ecc,
	uint32 *herr, uint32 *serr)
{
	ASSERT(nfl);
	ASSERT(nfl->read_page);

	return (nfl->read_page)(nfl, addr, buf, oob, ecc, herr, serr);
}

int
hndnand_write_page(hndnand_t *nfl, uint64 addr, const uint8 *buf, uint8 *oob, bool ecc)
{
	ASSERT(nfl);
	ASSERT(nfl->write_page);

	return (nfl->write_page)(nfl, addr, buf, oob, ecc);
}

int
hndnand_cmd_read_byte(hndnand_t *nfl, int cmd, int arg)
{
	ASSERT(nfl);
	ASSERT(nfl->cmd_read_byte);

	return (nfl->cmd_read_byte)(nfl, cmd, arg);
}
#endif /* _CFE_ */
