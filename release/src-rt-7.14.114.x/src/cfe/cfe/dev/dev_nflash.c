/*
 * Broadcom chipcommon NAND flash interface
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dev_nflash.c 421468 2013-09-03 09:48:30Z $
 */

#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "addrspace.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_error.h"
#include "dev_newflash.h"

#include "bsp_config.h"

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <hndnand.h>
#include <hndsoc.h>

#define isaligned(x, y) (((x) % (y)) == 0)
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

struct nflash_cfe {
	hndnand_t *info;
	flashpart_t parts[FLASH_MAX_PARTITIONS];
	unsigned char *map;
};

static int nflashidx = 0;

static int
nflash_cfe_open(cfe_devctx_t *ctx)
{
	return 0;
}

static int
nflash_cfe_read(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
	flashpart_t *part = (flashpart_t *) ctx->dev_softc;
	struct nflash_cfe *nflash = (struct nflash_cfe *) part->fp_dev;
	uint64 offset = buffer->buf_offset + part->fp_offset;
	uint len = (uint) buffer->buf_length;
	uchar *buf = NULL;
	int bytes, ret = 0;
	uint extra = 0;
	uchar *tmpbuf = NULL;
	int size;
	uint64 blk_offset, off, skip_bytes = 0;
	uint blocksize, pagesize, mask, good_bytes = 0;
	int blk_idx;
	int need_copy = 0;

	buffer->buf_retlen = 0;

	/* Check address range */
	if (!len)
		return 0;

	pagesize = nflash->info->pagesize;
	if ((offset & (pagesize - 1)) != 0) {
		extra = offset & (pagesize - 1);
		offset -= extra;
		len += extra;
		need_copy = 1;
	}

	size = (len + (pagesize - 1)) & ~(pagesize - 1);
	if (size != len)
		need_copy = 1;
	if (!need_copy) {
		buf = buffer->buf_ptr;
	} else {
		tmpbuf = (uchar *)KMALLOC(size, 0);
		buf = tmpbuf;
	}

	if ((((offset + len) >> 20) > nflash->info->size) ||
	    ((((offset + len) >> 20) == nflash->info->size) &&
	     (((offset + len) & ((1 << 20) - 1)) != 0))) {
		ret = CFE_ERR_IOERR;
		goto done;
	}

	blocksize = nflash->info->blocksize;
	mask = blocksize - 1;
	blk_offset = offset & ~mask;
	good_bytes = part->fp_offset & ~mask;
	/* Check and skip bad blocks */
	for (blk_idx = good_bytes/blocksize; blk_idx < nflash->info->numblocks; blk_idx++) {
		fl_offset_t blk_off = ((fl_offset_t)blocksize * blk_idx);

		if ((nflash->map[blk_idx] != 0) ||
		    (hndnand_checkbadb(nflash->info, blk_off) != 0)) {
			skip_bytes += blocksize;
			nflash->map[blk_idx] = 1;
		} else {
			if (good_bytes == blk_offset)
				break;
			good_bytes += blocksize;
		}
	}
	if (blk_idx == nflash->info->numblocks) {
		ret = CFE_ERR_IOERR;
		goto done;
	}
	blk_offset = blocksize * blk_idx;
	while (len > 0) {
		off = offset + skip_bytes;

		/* Check and skip bad blocks */
		if (off >= (blk_offset + blocksize)) {
			blk_offset += blocksize;
			blk_idx++;
			while (((nflash->map[blk_idx] != 0) ||
				(hndnand_checkbadb(nflash->info, blk_offset) != 0)) &&
			       ((blk_offset >> 20) < nflash->info->size)) {
				skip_bytes += blocksize;
				nflash->map[blk_idx] = 1;
				blk_offset += blocksize;
				blk_idx++;
			}
			if ((blk_offset >> 20) >= nflash->info->size) {
				ret = CFE_ERR_IOERR;
				goto done;
			}
			off = offset + skip_bytes;
		}
		if ((bytes = hndnand_read(nflash->info, off, pagesize, buf)) < 0) {
			ret = bytes;
			goto done;
		}
		if (bytes > len)
			bytes = len;
		offset += bytes;
		len -= bytes;
		buf += bytes;
		buffer->buf_retlen += bytes;
	}


done:
	if (tmpbuf) {
		buf = (uchar *) buffer->buf_ptr;
		buffer->buf_retlen -= extra;
		memcpy(buf, tmpbuf+extra, buffer->buf_retlen);
		KFREE(tmpbuf);
	}
	return ret;
}

static int
nflash_cfe_inpstat(cfe_devctx_t *ctx, iocb_inpstat_t *inpstat)
{
	inpstat->inp_status = 1;
	return 0;
}

static int
nflash_cfe_write(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
	flashpart_t *part = (flashpart_t *) ctx->dev_softc;
	struct nflash_cfe *nflash = (struct nflash_cfe *) part->fp_dev;
	uint64 offset = buffer->buf_offset + part->fp_offset;
	uint len = (uint) buffer->buf_length;
	uchar *buf = (uchar *) buffer->buf_ptr;
	uchar *block = NULL;
	iocb_buffer_t cur;
	int bytes, ret = 0;
	uint64 blk_offset, off = 0, skip_bytes = 0;
	uint blocksize, mask, good_bytes;
	int blk_idx;
	int docopy = 1;

	buffer->buf_retlen = 0;
	/* Check address range */
	if (!len)
		return 0;
	if ((offset + len) > (part->fp_offset + part->fp_size))
		return CFE_ERR_IOERR;
	blocksize = nflash->info->blocksize;
	if (!(block = KMALLOC(blocksize, 0)))
		return CFE_ERR_NOMEM;
	mask = blocksize - 1;
	/* Check and skip bad blocks */
	blk_offset = offset & ~mask;
	good_bytes = part->fp_offset & ~mask;
	for (blk_idx = good_bytes/blocksize;
	     blk_idx < (part->fp_offset+part->fp_size)/blocksize;
	     blk_idx++) {
		fl_offset_t blk_off = ((fl_offset_t)blocksize * blk_idx);

		if ((nflash->map[blk_idx] != 0) ||
		    (hndnand_checkbadb(nflash->info, blk_off) != 0)) {
			skip_bytes += blocksize;
			nflash->map[blk_idx] = 1;
		} else {
			if (good_bytes == blk_offset)
				break;
			good_bytes += blocksize;
		}
	}
	if (blk_idx == (part->fp_offset+part->fp_size)/blocksize) {
		ret = CFE_ERR_IOERR;
		goto done;
	}
	blk_offset = blocksize * blk_idx;
	/* Backup and erase one block at a time */
	while (len) {
		if (docopy) {
			/* Align offset */
			cur.buf_offset = offset & ~mask;
			cur.buf_length = blocksize;
			cur.buf_ptr = block;

			/* Copy existing data into holding sector if necessary */
			if (!isaligned((uint)offset, blocksize) || (len < blocksize)) {
				cur.buf_offset -= part->fp_offset;
				if ((ret = nflash_cfe_read(ctx, &cur)))
					goto done;
				if (cur.buf_retlen != cur.buf_length) {
					ret = CFE_ERR_IOERR;
					goto done;
				}
				cur.buf_offset += part->fp_offset;
			}
			/* Copy input data into holding block */
			cur.buf_retlen = min(len, blocksize - (offset & mask));
			memcpy(cur.buf_ptr + (offset & mask), buf, cur.buf_retlen);
		}

		off = (uint) cur.buf_offset + skip_bytes;
		/* Erase block */
		if ((ret = hndnand_erase(nflash->info, off)) < 0) {
			hndnand_mark_badb(nflash->info, off);
			nflash->map[blk_idx] = 1;
			skip_bytes += blocksize;
			docopy = 0;
		}
		else {
			/* Write holding sector */
			while (cur.buf_length) {
				if ((bytes = hndnand_write(nflash->info,
					cur.buf_offset + skip_bytes,
					(uint)cur.buf_length,
					(uchar *)cur.buf_ptr)) < 0) {
					/* Mark bad block */
					hndnand_mark_badb(nflash->info, off);
					nflash->map[blk_idx] = 1;
					skip_bytes += blocksize;
					docopy = 0;
					break;
				}
				cur.buf_offset += bytes;
				cur.buf_length -= bytes;
				cur.buf_ptr += bytes;
				docopy = 1;
			}
			if (docopy) {
				offset += cur.buf_retlen;
				len -= cur.buf_retlen;
				buf += cur.buf_retlen;
				buffer->buf_retlen += cur.buf_retlen;
			}
		}
		/* Check and skip bad blocks */
		if (len) {
			blk_offset += blocksize;
			blk_idx++;
			while (((nflash->map[blk_idx] != 0) ||
				(hndnand_checkbadb(nflash->info, blk_offset) != 0)) &&
			       (blk_offset < (part->fp_offset+part->fp_size))) {
				skip_bytes += blocksize;
				nflash->map[blk_idx] = 1;
				blk_offset += blocksize;
				blk_idx++;
			}
			if (blk_offset >= (part->fp_offset+part->fp_size)) {
				ret = CFE_ERR_IOERR;
				goto done;
			}
		}
	}
done:
	if (block)
		KFREE(block);
	return ret;
}

#ifdef CFE_FLASH_ERASE_FLASH_ENABLED
static int
nflash_cfe_erase_range(cfe_devctx_t *ctx, flash_range_t *range)
{
	flashpart_t *part = (flashpart_t *) ctx->dev_softc;
	struct nflash_cfe *nflash = (struct nflash_cfe *) part->fp_dev;
	uint blocksize = nflash->info->blocksize;

	uint64 offset, len;

	offset = part->fp_offset + range->range_base;
	len = range->range_length;

	if ((offset < part->fp_offset) ||
		((offset + len) > (part->fp_offset + part->fp_size))) {
		printf("!! offset 0x%llx, len=0x%llx over partition boundary: "
			"start: 0x%"FL_FMT"x, end: 0x%"FL_FMT"x",
			offset, len, part->fp_offset, part->fp_offset + part->fp_size);
		return CFE_ERR_INV_PARAM;
	}
	if ((offset & (blocksize - 1)) != 0) {
		printf("ersae offset must aligned to %llx = 0x%x\n", offset, blocksize);
		return CFE_ERR_INV_PARAM;
	}

	while (len > 0) {
		hndnand_erase(nflash->info, offset);
		offset += blocksize;
		len -= blocksize;
	}

	return 0;
}
#endif	/* CFE_FLASH_ERASE_FLASH_ENABLED */

static int
nflash_cfe_ioctl(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
	flashpart_t *part = (flashpart_t *) ctx->dev_softc;
	struct nflash_cfe *nflash = (struct nflash_cfe *) part->fp_dev;
	flash_info_t *info;
#ifdef CFE_FLASH_ERASE_FLASH_ENABLED
	flash_range_t *range;
#endif

	switch (buffer->buf_ioctlcmd) {
	case IOCTL_FLASH_WRITE_ALL:
		nflash_cfe_write(ctx, buffer);
		break;
	case IOCTL_FLASH_GETINFO:
		info = (flash_info_t *) buffer->buf_ptr;
		info->flash_base = 0;
#ifdef	__ARM_ARCH_7A__
		info->flash_size = ((fl_size_t)nflash->info->size << 20);
#else
		/* 2GB is supported for now */
		info->flash_size =
			(nflash->info->size >= (1 << 11)) ? (1 << 31) : (nflash->info->size << 20);
#endif /* __ARM_ARCH_7A__ */
		info->flash_type = nflash->info->type;
		info->flash_flags = FLASH_FLAG_NOERASE;
		break;
#ifdef CFE_FLASH_ERASE_FLASH_ENABLED
	case IOCTL_FLASH_ERASE_RANGE:
		range = (flash_range_t *) buffer->buf_ptr;
		/* View the base 0 and range 0 as erase all parition */
		if (range->range_base == 0 && range->range_length == 0) {
			range->range_length = part->fp_size;
			printf("Erase whole mtd partition!\n");
		}
		xprintf("Erase range: 0x%x - 0x%x\n",
			range->range_base, range->range_base + range->range_length);
		nflash_cfe_erase_range(ctx, range);
		break;
#endif
	default:
		return CFE_ERR_INV_COMMAND;
	}

	return 0;
}

static int
nflash_cfe_close(cfe_devctx_t *ctx)
{
	return 0;
}

static const cfe_devdisp_t nflash_cfe_dispatch = {
	nflash_cfe_open,
	nflash_cfe_read,
	nflash_cfe_inpstat,
	nflash_cfe_write,
	nflash_cfe_ioctl,
	nflash_cfe_close,
	NULL,
	NULL
};

static void
nflash_do_parts(struct nflash_cfe *nflash, newflash_probe_t *probe)
{
	int idx;
	int middlepart = -1;
	flashpart_t *parts = nflash->parts;
#ifdef	__ARM_ARCH_7A__
	fl_size_t lobound = 0;
	fl_size_t hibound = ((fl_size_t)nflash->info->size << 20);
#else
	int lobound = 0;
	int hibound = (nflash->info->size >= (1 << 11)) ? (1 << 31) : (nflash->info->size << 20);
#endif /* __ARM_ARCH_7A__ */

	for (idx = 0; idx < probe->flash_nparts; idx++) {
		if (probe->flash_parts[idx].fp_size == 0) {
			middlepart = idx;
			break;
		}
		parts[idx].fp_offset = lobound;
		parts[idx].fp_size = probe->flash_parts[idx].fp_size;
		lobound += probe->flash_parts[idx].fp_size;
	}

	if (idx != probe->flash_nparts) {
		for (idx = probe->flash_nparts - 1; idx > middlepart; idx--) {
			parts[idx].fp_size = probe->flash_parts[idx].fp_size;
			hibound -= probe->flash_parts[idx].fp_size;
			parts[idx].fp_offset = hibound;
		}
	}

	if (middlepart != -1) {
		parts[middlepart].fp_offset = lobound;
		parts[middlepart].fp_size = hibound - lobound;
	}
}

static void
nflash_cfe_probe(cfe_driver_t *drv,
                 unsigned long probe_a, unsigned long probe_b,
                 void *probe_ptr)
{
	newflash_probe_t *probe = (newflash_probe_t *) probe_ptr;
	struct nflash_cfe *nflash;
	char type[80], descr[80], name[80];
	int idx;
	si_t *sih;

	if (!(nflash = (struct nflash_cfe *)KMALLOC(sizeof(struct nflash_cfe), 0)))
		return;
	memset(nflash, 0, sizeof(struct nflash_cfe));
	/* Attach to the backplane and map to chipc */
	sih = si_kattach(SI_OSH);

	/* Initialize serial flash access */
	if (!(nflash->info = hndnand_init(sih))) {
		xprintf("nflash: found no supported devices\n");
		KFREE(nflash);
		return;
	}
	/* Set description */
	switch (nflash->info->type) {
	case NFL_VENDOR_AMD:
		sprintf(type, "AMD");
		break;
	case NFL_VENDOR_NUMONYX:
		sprintf(type, "Numonyx");
		break;
	case NFL_VENDOR_MICRON:
		sprintf(type, "Micron");
		break;
	case NFL_VENDOR_TOSHIBA:
		sprintf(type, "Toshiba");
		break;
	case NFL_VENDOR_HYNIX:
		sprintf(type, "Hynix");
		break;
	case NFL_VENDOR_SAMSUNG:
		sprintf(type, "Samsung");
		break;
	case NFL_VENDOR_ZENTEL_ESMT:
		sprintf(type, "Zentel");
		break;
	default:
		sprintf(type, "Unknown type %d", nflash->info->type);
		break;
	}
	nflash->map = (unsigned char *)KMALLOC(nflash->info->numblocks, 0);
	if (nflash->map)
		memset(nflash->map, 0, nflash->info->numblocks);
	if (probe->flash_nparts == 0) {
		/* Just instantiate one device */
		nflash->parts[0].fp_dev = (flashdev_t *)nflash;
		nflash->parts[0].fp_offset = 0;
#ifdef	__ARM_ARCH_7A__
		nflash->parts[0].fp_size = ((fl_size_t)nflash->info->size) << 20;
#else
		/* At most 2GB for one partition */
		nflash->parts[0].fp_size =
			(nflash->info->size >= (1 << 11)) ? (1 << 31) : (nflash->info->size << 20);
#endif /* __ARM_ARCH_7A__ */

		sprintf(descr, "%s %s size %uKB",
			type, drv->drv_description,
			nflash->info->size << 10);
		cfe_attach(drv, &nflash->parts[0], NULL, descr);
	} else {
		/* Partition flash into chunks */
		nflash_do_parts(nflash, probe);
		/* Instantiate devices for each piece */
		for (idx = 0; idx < probe->flash_nparts; idx++) {
			sprintf(descr, "%s %s offset %"FL_FMT"X size %"FL_FMT"uKB",
				type, drv->drv_description,
				nflash->parts[idx].fp_offset,
				(nflash->parts[idx].fp_size + 1023) / 1024);
			nflash->parts[idx].fp_dev = (flashdev_t *) nflash;
			if (probe->flash_parts[idx].fp_name)
				strcpy(name, probe->flash_parts[idx].fp_name);
			else
				sprintf(name, "%d", idx);
			cfe_attach_idx(drv, nflashidx, &nflash->parts[idx], name, descr);
		}
		nflashidx++;
	}
}

const cfe_driver_t nflashdrv = {
	"NAND flash",
	"nflash",
	CFE_DEV_FLASH,
	&nflash_cfe_dispatch,
	nflash_cfe_probe
};
