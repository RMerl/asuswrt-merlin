/*
 * Broadcom SiliconBackplane chipcommon serial flash interface
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id: sflash.c 345826 2012-07-19 06:34:55Z $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <hndsoc.h>
#include <sbhndcpu.h>
#include <sbchipc.h>
#include <bcmdevs.h>
#include <sflash.h>

#ifdef BCMDBG
#define	SFL_MSG(args)	printf args
#else
#define	SFL_MSG(args)
#endif	/* BCMDBG */

/* Private global state */
static struct sflash sflash;

/* Issue a serial flash command */
static INLINE void
sflash_cmd(osl_t *osh, chipcregs_t *cc, uint opcode)
{
	W_REG(osh, &cc->flashcontrol, SFLASH_START | opcode);
	while (R_REG(osh, &cc->flashcontrol) & SFLASH_BUSY);
}

static bool firsttime = TRUE;

/* Initialize serial flash access */
struct sflash *
sflash_init(si_t *sih, chipcregs_t *cc)
{
	uint32 id, id2;
	const char *name = "";
	osl_t *osh;

	ASSERT(sih);

	osh = si_osh(sih);

	bzero(&sflash, sizeof(sflash));

	sflash.type = sih->cccaps & CC_CAP_FLASH_MASK;

	switch (sflash.type) {
	case SFLASH_ST:
		/* Probe for ST chips */
		name = "ST compatible";
		sflash_cmd(osh, cc, SFLASH_ST_DP);
		W_REG(osh, &cc->flashaddress, 0);
		sflash_cmd(osh, cc, SFLASH_ST_RES);
		id = R_REG(osh, &cc->flashdata);
		sflash.blocksize = 64 * 1024;
		switch (id) {
		case 0x11:
			/* ST M25P20 2 Mbit Serial Flash */
			sflash.numblocks = 4;
			break;
		case 0x12:
			/* ST M25P40 4 Mbit Serial Flash */
			sflash.numblocks = 8;
			break;
		case 0x13:
			sflash_cmd(osh, cc, SFLASH_MXIC_RDID);
			id = R_REG(osh, &cc->flashdata);
			if (id == SFLASH_MXIC_MFID) {
				/* MXIC MX25L8006E 8 Mbit Serial Flash */
				sflash.blocksize = 4 * 1024;
				sflash.numblocks = 16 * 16;
			} else  {
				/* ST M25P80 8 Mbit Serial Flash */
				sflash.numblocks = 16;
			}
			break;
		case 0x14:
			/* ST M25P16 16 Mbit Serial Flash */
			sflash.numblocks = 32;
			break;
		case 0x15:
			/* ST M25P32 32 Mbit Serial Flash */
			sflash.numblocks = 64;
			break;
		case 0x16:
			/* ST M25P64 64 Mbit Serial Flash */
			sflash.numblocks = 128;
			break;
		case 0x17:
			/* ST M25FL128 128 Mbit Serial Flash */
			sflash.numblocks = 256;
			break;
		case 0xbf:
			/* All of the following flashes are SST with
			 * 4KB subsectors. Others should be added but
			 * We'll have to revamp the way we identify them
			 * since RES is not eough to disambiguate them.
			 */
			name = "SST";
			sflash.blocksize = 4 * 1024;
			W_REG(osh, &cc->flashaddress, 1);
			sflash_cmd(osh, cc, SFLASH_ST_RES);
			id2 = R_REG(osh, &cc->flashdata);
			switch (id2) {
			case 1:
				/* SST25WF512 512 Kbit Serial Flash */
				sflash.numblocks = 16;
				break;
			case 0x48:
				/* SST25VF512 512 Kbit Serial Flash */
				sflash.numblocks = 16;
				break;
			case 2:
				/* SST25WF010 1 Mbit Serial Flash */
				sflash.numblocks = 32;
				break;
			case 0x49:
				/* SST25VF010 1 Mbit Serial Flash */
				sflash.numblocks = 32;
				break;
			case 3:
				/* SST25WF020 2 Mbit Serial Flash */
				sflash.numblocks = 64;
				break;
			case 0x43:
				/* SST25VF020 2 Mbit Serial Flash */
				sflash.numblocks = 64;
				break;
			case 4:
				/* SST25WF040 4 Mbit Serial Flash */
				sflash.numblocks = 128;
				break;
			case 0x44:
				/* SST25VF040 4 Mbit Serial Flash */
				sflash.numblocks = 128;
				break;
			case 0x8d:
				/* SST25VF040B 4 Mbit Serial Flash */
				sflash.numblocks = 128;
				break;
			case 5:
				/* SST25WF080 8 Mbit Serial Flash */
				sflash.numblocks = 256;
				break;
			case 0x8e:
				/* SST25VF080B 8 Mbit Serial Flash */
				sflash.numblocks = 256;
				break;
			case 0x41:
				/* SST25VF016 16 Mbit Serial Flash */
				sflash.numblocks = 512;
				break;
			case 0x4a:
				/* SST25VF032 32 Mbit Serial Flash */
				sflash.numblocks = 1024;
				break;
			case 0x4b:
				/* SST25VF064 64 Mbit Serial Flash */
				sflash.numblocks = 2048;
				break;
			}
			break;
		}
		break;

	case SFLASH_AT:
		/* Probe for Atmel chips */
		name = "Atmel";
		sflash_cmd(osh, cc, SFLASH_AT_STATUS);
		id = R_REG(osh, &cc->flashdata) & 0x3c;
		switch (id) {
		case 0xc:
			/* Atmel AT45DB011 1Mbit Serial Flash */
			sflash.blocksize = 256;
			sflash.numblocks = 512;
			break;
		case 0x14:
			/* Atmel AT45DB021 2Mbit Serial Flash */
			sflash.blocksize = 256;
			sflash.numblocks = 1024;
			break;
		case 0x1c:
			/* Atmel AT45DB041 4Mbit Serial Flash */
			sflash.blocksize = 256;
			sflash.numblocks = 2048;
			break;
		case 0x24:
			/* Atmel AT45DB081 8Mbit Serial Flash */
			sflash.blocksize = 256;
			sflash.numblocks = 4096;
			break;
		case 0x2c:
			/* Atmel AT45DB161 16Mbit Serial Flash */
			sflash.blocksize = 512;
			sflash.numblocks = 4096;
			break;
		case 0x34:
			/* Atmel AT45DB321 32Mbit Serial Flash */
			sflash.blocksize = 512;
			sflash.numblocks = 8192;
			break;
		case 0x3c:
			/* Atmel AT45DB642 64Mbit Serial Flash */
			sflash.blocksize = 1024;
			sflash.numblocks = 8192;
			break;
		}
		break;
	}

	sflash.size = sflash.blocksize * sflash.numblocks;
	sflash.phybase = SI_FLASH2;

	if (firsttime)
		printf("Found an %s serial flash with %d %dKB blocks; total size %dMB\n",
		       name, sflash.numblocks, sflash.blocksize / 1024,
		       sflash.size / (1024 * 1024));

	firsttime = FALSE;
	return sflash.size ? &sflash : NULL;
}

/* Read len bytes starting at offset into buf. Returns number of bytes read. */
int
sflash_read(si_t *sih, chipcregs_t *cc, uint offset, uint len, uchar *buf)
{
	uint8 *from, *to;
	int cnt, i;

	ASSERT(sih);

	if (!len)
		return 0;

	if ((offset + len) > sflash.size)
		return -22;

	if ((len >= 4) && (offset & 3))
		cnt = 4 - (offset & 3);
	else if ((len >= 4) && ((uintptr)buf & 3))
		cnt = 4 - ((uintptr)buf & 3);
	else
		cnt = len;

	if (sih->ccrev == 12)
		from = (uint8 *)OSL_UNCACHED((void *)SI_FLASH2 + offset);
	else
		from = (uint8 *)OSL_CACHED((void *)SI_FLASH2 + offset);
	to = (uint8 *)buf;

	if (cnt < 4) {
		for (i = 0; i < cnt; i ++) {
			/* Cannot use R_REG because in bigendian that will
			 * xor the address and we don't want that here.
			 */
			*to = *from;
			from ++;
			to ++;
		}
		return cnt;
	}

	while (cnt >= 4) {
		*(uint32 *)to = *(uint32 *)from;
		from += 4;
		to += 4;
		cnt -= 4;
	}

	return (len - cnt);
}

/* Poll for command completion. Returns zero when complete. */
int
sflash_poll(si_t *sih, chipcregs_t *cc, uint offset)
{
	osl_t *osh;

	ASSERT(sih);

	osh = si_osh(sih);

	if (offset >= sflash.size)
		return -22;

	switch (sflash.type) {
	case SFLASH_ST:
		/* Check for ST Write In Progress bit */
		sflash_cmd(osh, cc, SFLASH_ST_RDSR);
		return R_REG(osh, &cc->flashdata) & SFLASH_ST_WIP;
	case SFLASH_AT:
		/* Check for Atmel Ready bit */
		sflash_cmd(osh, cc, SFLASH_AT_STATUS);
		return !(R_REG(osh, &cc->flashdata) & SFLASH_AT_READY);
	}

	return 0;
}

/* Write len bytes starting at offset into buf. Returns number of bytes
 * written. Caller should poll for completion.
 */
#define	ST_RETRIES	3

#ifdef	IL_BIGENDIAN
#ifdef	BCMHND74K
#define	GET_BYTE(ptr)	(*(uint8 *)((uint32)(ptr) ^ 7))
#else	/* !74K, bcm33xx */
#define	GET_BYTE(ptr)	(*(uint8 *)((uint32)(ptr) ^ 3))
#endif	/* BCMHND74K */
#else	/* !IL_BIGENDIAN */
#define	GET_BYTE(ptr)	(*(ptr))
#endif	/* IL_BIGENDIAN */

int
sflash_write(si_t *sih, chipcregs_t *cc, uint offset, uint length, const uchar *buffer)
{
	struct sflash *sfl;
	uint off = offset, len = length;
	const uint8 *buf = buffer;
	uint8 data;
	int ret = 0, ntry = 0;
	bool is4712b0;
	uint32 page, byte, mask;
	osl_t *osh;

	ASSERT(sih);

	osh = si_osh(sih);

	if (!len)
		return 0;

	sfl = &sflash;
	if ((off + len) > sfl->size)
		return -22;

	switch (sfl->type) {
	case SFLASH_ST:
		is4712b0 = (CHIPID(sih->chip) == BCM4712_CHIP_ID) && (CHIPREV(sih->chiprev) == 3);
		/* Enable writes */
retry:		sflash_cmd(osh, cc, SFLASH_ST_WREN);
		off = offset;
		len = length;
		buf = buffer;
		ntry++;
		if (is4712b0) {
			mask = 1 << 14;
			W_REG(osh, &cc->flashaddress, off);
			data = GET_BYTE(buf);
			buf++;
			W_REG(osh, &cc->flashdata, data);
			/* Set chip select */
			OR_REG(osh, &cc->gpioout, mask);
			/* Issue a page program with the first byte */
			sflash_cmd(osh, cc, SFLASH_ST_PP);
			ret = 1;
			off++;
			len--;
			while (len > 0) {
				if ((off & 255) == 0) {
					/* Page boundary, drop cs and return */
					AND_REG(osh, &cc->gpioout, ~mask);
					OSL_DELAY(1);
					if (!sflash_poll(sih, cc, off)) {
						/* Flash rejected command */
						if (ntry <= ST_RETRIES)
							goto retry;
						else
							return -11;
					}
					return ret;
				} else {
					/* Write single byte */
					data = GET_BYTE(buf);
					buf++;
					sflash_cmd(osh, cc, data);
				}
				ret++;
				off++;
				len--;
			}
			/* All done, drop cs */
			AND_REG(osh, &cc->gpioout, ~mask);
			OSL_DELAY(1);
			if (!sflash_poll(sih, cc, off)) {
				/* Flash rejected command */
				if (ntry <= ST_RETRIES)
					goto retry;
				else
					return -12;
			}
		} else if (sih->ccrev >= 20) {
			W_REG(osh, &cc->flashaddress, off);
			data = GET_BYTE(buf);
			buf++;
			W_REG(osh, &cc->flashdata, data);
			/* Issue a page program with CSA bit set */
			sflash_cmd(osh, cc, SFLASH_ST_CSA | SFLASH_ST_PP);
			ret = 1;
			off++;
			len--;
			while (len > 0) {
				if ((off & 255) == 0) {
					/* Page boundary, poll droping cs and return */
					W_REG(NULL, &cc->flashcontrol, 0);
					OSL_DELAY(1);
					if (sflash_poll(sih, cc, off) == 0) {
						/* Flash rejected command */
						SFL_MSG(("sflash: pp rejected, ntry: %d,"
						         " off: %d/%d, len: %d/%d, ret:"
						         "%d\n", ntry, off, offset, len,
						         length, ret));
						if (ntry <= ST_RETRIES)
							goto retry;
						else
							return -11;
					}
					return ret;
				} else {
					/* Write single byte */
					data = GET_BYTE(buf);
					buf++;
					sflash_cmd(osh, cc, SFLASH_ST_CSA | data);
				}
				ret++;
				off++;
				len--;
			}
			/* All done, drop cs & poll */
			W_REG(NULL, &cc->flashcontrol, 0);
			OSL_DELAY(1);
			if (sflash_poll(sih, cc, off) == 0) {
				/* Flash rejected command */
				SFL_MSG(("sflash: pp rejected, ntry: %d, off: %d/%d,"
				         " len: %d/%d, ret: %d\n",
				         ntry, off, offset, len, length, ret));
				if (ntry <= ST_RETRIES)
					goto retry;
				else
					return -12;
			}
		} else {
			ret = 1;
			W_REG(osh, &cc->flashaddress, off);
			data = GET_BYTE(buf);
			buf++;
			W_REG(osh, &cc->flashdata, data);
			/* Page program */
			sflash_cmd(osh, cc, SFLASH_ST_PP);
		}
		break;
	case SFLASH_AT:
		mask = sfl->blocksize - 1;
		page = (off & ~mask) << 1;
		byte = off & mask;
		/* Read main memory page into buffer 1 */
		if (byte || (len < sfl->blocksize)) {
			W_REG(osh, &cc->flashaddress, page);
			sflash_cmd(osh, cc, SFLASH_AT_BUF1_LOAD);
			/* 250 us for AT45DB321B */
			SPINWAIT(sflash_poll(sih, cc, off), 1000);
			ASSERT(!sflash_poll(sih, cc, off));
		}
		/* Write into buffer 1 */
		for (ret = 0; (ret < (int)len) && (byte < sfl->blocksize); ret++) {
			W_REG(osh, &cc->flashaddress, byte++);
			W_REG(osh, &cc->flashdata, *buf++);
			sflash_cmd(osh, cc, SFLASH_AT_BUF1_WRITE);
		}
		/* Write buffer 1 into main memory page */
		W_REG(osh, &cc->flashaddress, page);
		sflash_cmd(osh, cc, SFLASH_AT_BUF1_PROGRAM);
		break;
	}

	return ret;
}

/* Erase a region. Returns number of bytes scheduled for erasure.
 * Caller should poll for completion.
 */
int
sflash_erase(si_t *sih, chipcregs_t *cc, uint offset)
{
	struct sflash *sfl;
	osl_t *osh;

	ASSERT(sih);

	osh = si_osh(sih);

	sfl = &sflash;
	if (offset >= sfl->size)
		return -22;

	switch (sfl->type) {
	case SFLASH_ST:
		sflash_cmd(osh, cc, SFLASH_ST_WREN);
		W_REG(osh, &cc->flashaddress, offset);
		/* Newer flashes have "sub-sectors" which can be erased independently
		 * with a new command: ST_SSE. The ST_SE command erases 64KB just as
		 * before.
		 */
		sflash_cmd(osh, cc, (sfl->blocksize < (64 * 1024)) ? SFLASH_ST_SSE : SFLASH_ST_SE);
		return sfl->blocksize;
	case SFLASH_AT:
		W_REG(osh, &cc->flashaddress, offset << 1);
		sflash_cmd(osh, cc, SFLASH_AT_PAGE_ERASE);
		return sfl->blocksize;
	}

	return 0;
}

/*
 * writes the appropriate range of flash, a NULL buf simply erases
 * the region of flash
 */
int
sflash_commit(si_t *sih, chipcregs_t *cc, uint offset, uint len, const uchar *buf)
{
	struct sflash *sfl;
	uchar *block = NULL, *cur_ptr, *blk_ptr;
	uint blocksize = 0, mask, cur_offset, cur_length, cur_retlen, remainder;
	uint blk_offset, blk_len, copied;
	int bytes, ret = 0;
	osl_t *osh;

	ASSERT(sih);

	osh = si_osh(sih);

	/* Check address range */
	if (len <= 0)
		return 0;

	sfl = &sflash;
	if ((offset + len) > sfl->size)
		return -1;

	blocksize = sfl->blocksize;
	mask = blocksize - 1;

	/* Allocate a block of mem */
	if (!(block = MALLOC(osh, blocksize)))
		return -1;

	while (len) {
		/* Align offset */
		cur_offset = offset & ~mask;
		cur_length = blocksize;
		cur_ptr = block;

		remainder = blocksize - (offset & mask);
		if (len < remainder)
			cur_retlen = len;
		else
			cur_retlen = remainder;

		/* buf == NULL means erase only */
		if (buf) {
			/* Copy existing data into holding block if necessary */
			if ((offset & mask) || (len < blocksize)) {
				blk_offset = cur_offset;
				blk_len = cur_length;
				blk_ptr = cur_ptr;

				/* Copy entire block */
				while (blk_len) {
					copied = sflash_read(sih, cc, blk_offset, blk_len, blk_ptr);
					blk_offset += copied;
					blk_len -= copied;
					blk_ptr += copied;
				}
			}

			/* Copy input data into holding block */
			memcpy(cur_ptr + (offset & mask), buf, cur_retlen);
		}

		/* Erase block */
		if ((ret = sflash_erase(sih, cc, (uint) cur_offset)) < 0)
			goto done;
		while (sflash_poll(sih, cc, (uint) cur_offset));

		/* buf == NULL means erase only */
		if (!buf) {
			offset += cur_retlen;
			len -= cur_retlen;
			continue;
		}

		/* Write holding block */
		while (cur_length > 0) {
			if ((bytes = sflash_write(sih, cc,
			                          (uint) cur_offset,
			                          (uint) cur_length,
			                          (uchar *) cur_ptr)) < 0) {
				ret = bytes;
				goto done;
			}
			while (sflash_poll(sih, cc, (uint) cur_offset));
			cur_offset += bytes;
			cur_length -= bytes;
			cur_ptr += bytes;
		}

		offset += cur_retlen;
		len -= cur_retlen;
		buf += cur_retlen;
	}

	ret = len;
done:
	if (block)
		MFREE(osh, block, blocksize);
	return ret;
}
