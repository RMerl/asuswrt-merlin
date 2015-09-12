/*
 * Broadcom SiliconBackplane chipcommon serial flash interface
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dev_sflash.c 421304 2013-09-02 03:21:33Z $
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
#include <hndsflash.h>

#define isaligned(x, y) (((x) % (y)) == 0)
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

struct sflash_cfe {
	si_t *sih;
	struct hndsflash *info;
	flashpart_t parts[FLASH_MAX_PARTITIONS];
};

static int sflashidx = 0;

static int
sflash_cfe_open(cfe_devctx_t *ctx)
{
	return 0;
}

static int
sflash_cfe_read(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
	flashpart_t *part = (flashpart_t *)ctx->dev_softc;
	struct sflash_cfe *sflash = (struct sflash_cfe *)part->fp_dev;
	uint offset = (uint)buffer->buf_offset + part->fp_offset;
	uint len = (uint)buffer->buf_length;
	uchar *buf = (uchar *)buffer->buf_ptr;
	int bytes, ret = 0;

	buffer->buf_retlen = 0;

	/* Check address range */
	if (!len)
		return 0;
	if ((offset + len) > sflash->info->size)
		return CFE_ERR_IOERR;

	while (len) {
		if ((bytes = hndsflash_read(sflash->info, offset, len, buf)) < 0) {
			ret = bytes;
			goto done;
		}
		offset += bytes;
		len -= bytes;
		buf += bytes;
		buffer->buf_retlen += bytes;
	}

done:
	return ret;
}

static int
sflash_cfe_inpstat(cfe_devctx_t *ctx, iocb_inpstat_t *inpstat)
{
	inpstat->inp_status = 1;
	return 0;
}

#ifdef CFE_FLASH_ERASE_FLASH_ENABLED
static int
sflash_cfe_erase_range(cfe_devctx_t *ctx, flash_range_t *range)
{
	flashpart_t *part = (flashpart_t *)ctx->dev_softc;
	struct sflash_cfe *sflash = (struct sflash_cfe *)part->fp_dev;
	int len, offset;

	offset = part->fp_offset + range->range_base;
	len = range->range_length;

	if ((offset < part->fp_offset) ||
		((offset + len) > (part->fp_offset + part->fp_size))) {
		printf("!! offset 0x%x, len=0x%x over partition boundary: start: 0x%x, end: 0x%x",
		offset, len, (uint)part->fp_offset, (uint)(part->fp_offset + part->fp_size));
		return CFE_ERR_INV_PARAM;
	}
	return hndsflash_commit(sflash->info, (uint)offset, (uint)len, NULL);
}
#endif	/* CFE_FLASH_ERASE_FLASH_ENABLED */

static int
sflash_cfe_write(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
	flashpart_t *part = (flashpart_t *)ctx->dev_softc;
	struct sflash_cfe *sflash = (struct sflash_cfe *)part->fp_dev;
	uint offset = (uint)buffer->buf_offset + part->fp_offset;
	uint len = (uint)buffer->buf_length;
	uchar *buf = (uchar *)buffer->buf_ptr;
	uchar *block = NULL;
	uint blocksize = 0, mask;
	iocb_buffer_t cur;
	int bytes, ret = 0;

	buffer->buf_retlen = 0;

	/* Check address range */
	if (!len)
		return 0;

	if ((offset + len) > sflash->info->size)
		return CFE_ERR_IOERR;

	blocksize = sflash->info->blocksize;
	mask = blocksize - 1;

	if (block)
		KFREE(block);
	if (!(block = KMALLOC(blocksize, 0)))
		return CFE_ERR_NOMEM;

	/* Backup and erase one block at a time */
	while (len) {
		/* Align offset */
		cur.buf_offset = offset & ~mask;
		cur.buf_length = blocksize;
		cur.buf_ptr = block;

		/* Copy existing data into holding block if necessary */
		if (!isaligned(offset, blocksize) || (len < blocksize)) {
			cur.buf_offset -= part->fp_offset;
			if ((ret = sflash_cfe_read(ctx, &cur)))
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

		/* Erase block */
		if ((ret = hndsflash_erase(sflash->info, (uint)cur.buf_offset)) < 0)
			goto done;
		while (hndsflash_poll(sflash->info, (uint)cur.buf_offset));

		/* Write holding block */
		while (cur.buf_length) {
			if ((bytes = hndsflash_write(sflash->info,
				(uint)cur.buf_offset,
				(uint)cur.buf_length,
				(uchar *)cur.buf_ptr)) < 0) {
				ret = bytes;
				goto done;
			}
			while (hndsflash_poll(sflash->info, (uint)cur.buf_offset));

			cur.buf_offset += bytes;
			cur.buf_length -= bytes;
			cur.buf_ptr += bytes;
		}

		offset += cur.buf_retlen;
		len -= cur.buf_retlen;
		buf += cur.buf_retlen;
		buffer->buf_retlen += cur.buf_retlen;
	}

done:
	if (block)
		KFREE(block);
	return ret;
}

static int
sflash_cfe_ioctl(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
	flashpart_t *part = (flashpart_t *)ctx->dev_softc;
	struct sflash_cfe *sflash = (struct sflash_cfe *)part->fp_dev;
	flash_info_t *info;
#ifdef CFE_FLASH_ERASE_FLASH_ENABLED
	flash_range_t *range;
#endif

	switch (buffer->buf_ioctlcmd) {
	case IOCTL_FLASH_WRITE_ALL:
		sflash_cfe_write(ctx, buffer);
		break;
	case IOCTL_FLASH_GETINFO:
		info = (flash_info_t *)buffer->buf_ptr;
		info->flash_base = 0;
		info->flash_size = sflash->info->size;
		info->flash_type = sflash->info->type;
		info->flash_flags = FLASH_FLAG_NOERASE;
		break;
#ifdef FLASH_PARTITION_FILL_ENABLED
	case IOCTL_FLASH_PARTITION_INFO:
		info = (flash_info_t *)buffer->buf_ptr;
		info->flash_base = part->fp_offset;
		info->flash_size = part->fp_size;
		break;
#endif
#ifdef CFE_FLASH_ERASE_FLASH_ENABLED
	case IOCTL_FLASH_ERASE_RANGE:
		range = (flash_range_t *)buffer->buf_ptr;
		/* View the base 0 and range 0 as erase all parition */
		if (range->range_base == 0 && range->range_length == 0) {
			range->range_length = part->fp_size;
		}
	        sflash_cfe_erase_range(ctx, range);
		break;
#endif
	default:
		return CFE_ERR_INV_COMMAND;
	}

	return 0;
}

static int
sflash_cfe_close(cfe_devctx_t *ctx)
{
	return 0;
}

static const cfe_devdisp_t sflash_cfe_dispatch = {
	sflash_cfe_open,
	sflash_cfe_read,
	sflash_cfe_inpstat,
	sflash_cfe_write,
	sflash_cfe_ioctl,
	sflash_cfe_close,
	NULL,
	NULL
};

static void
sflash_do_parts(struct sflash_cfe *sflash, newflash_probe_t *probe)
{
	int idx;
	int middlepart = -1;
	int lobound = 0;
	flashpart_t *parts = sflash->parts;
	int hibound = sflash->info->size;

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
sflash_cfe_probe(cfe_driver_t *drv,
	unsigned long probe_a, unsigned long probe_b,
	void *probe_ptr)
{
	newflash_probe_t *probe = (newflash_probe_t *)probe_ptr;
	struct sflash_cfe *sflash;
	char type[80], descr[80], name[80];
	int idx;

	if (!(sflash = (struct sflash_cfe *)KMALLOC(sizeof(struct sflash_cfe), 0)))
		return;
	memset(sflash, 0, sizeof(struct sflash_cfe));

	/* Attach to the backplane and map to chipc */
	sflash->sih = si_kattach(SI_OSH);

	/* Initialize serial flash access */
	if (!(sflash->info = hndsflash_init(sflash->sih))) {
		xprintf("sflash: found no supported devices\n");
		KFREE(sflash);
		return;
	}

	/* Get the flash total size */
	probe->flash_size = sflash->info->size;

	/* Set description */
	switch (sflash->info->type) {
	case SFLASH_ST:
	case QSPIFLASH_ST:
		sprintf(type, "ST Compatible");
		break;
	case SFLASH_AT:
	case QSPIFLASH_AT:
		sprintf(type, "Atmel");
		break;
	default:
		sprintf(type, "Unknown type %d", sflash->info->type);
		break;
	}

	if (probe->flash_nparts == 0) {
		/* Just instantiate one device */
		sflash->parts[0].fp_dev = (flashdev_t *)sflash;
		sflash->parts[0].fp_offset = 0;
		sflash->parts[0].fp_size = sflash->info->size;
		sprintf(descr, "%s %s size %uKB",
			type, drv->drv_description,
			(sflash->info->size + 1023) / 1024);
		cfe_attach(drv, &sflash->parts[0], NULL, descr);
	} else {
		/* Partition flash into chunks */
		sflash_do_parts(sflash, probe);

		/* Instantiate devices for each piece */
		for (idx = 0; idx < probe->flash_nparts; idx++) {
			sprintf(descr, "%s %s offset %08X size %uKB",
				type, drv->drv_description,
				(uint)sflash->parts[idx].fp_offset,
				(uint)(sflash->parts[idx].fp_size + 1023) / 1024);
			sflash->parts[idx].fp_dev = (flashdev_t *) sflash;
			if (probe->flash_parts[idx].fp_name)
				strcpy(name, probe->flash_parts[idx].fp_name);
			else
				sprintf(name, "%d", idx);
			cfe_attach_idx(drv, sflashidx, &sflash->parts[idx], name, descr);
		}
		sflashidx++;
	}
}

const cfe_driver_t sflashdrv = {
	"Serial flash",
	"flash",
	CFE_DEV_FLASH,
	&sflash_cfe_dispatch,
	sflash_cfe_probe
};
