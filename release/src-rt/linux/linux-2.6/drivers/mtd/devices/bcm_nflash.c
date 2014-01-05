/*
 * Broadcom SiliconBackplane chipcommon serial flash interface
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
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
 * $Id $
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/mtd/compatmac.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <asm/io.h>

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <bcmnvram.h>
#include <siutils.h>
#include <hndpci.h>
#include <pcicfg.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <nflash.h>

#ifdef CONFIG_MTD_PARTITIONS
extern struct mtd_partition *
init_nflash_mtd_partitions(hndnand_t *nfl, struct mtd_info *mtd, size_t size);

struct mtd_partition *nflash_parts;
#endif

extern struct mutex *partitions_mutex_init(void);

struct nflash_mtd {
	si_t *sih;
	chipcregs_t *cc;
	hndnand_t *nfl;
	struct mtd_info mtd;
	struct mtd_erase_region_info region;
	unsigned char *map;
};

/* Private global state */
static struct nflash_mtd nflash;

static int
_nflash_mtd_read(struct mtd_info *mtd, struct mtd_partition *part,
	loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct nflash_mtd *nflash = (struct nflash_mtd *) mtd->priv;
	int bytes, ret = 0;
	uint extra = 0;
	uchar *tmpbuf = NULL;
	int size;
	uint offset, blocksize, mask, blk_offset, off;
	uint skip_bytes = 0, good_bytes = 0, page_size;
	int blk_idx, i;
	int need_copy = 0;
	uchar *ptr = NULL;

	/* Locate the part */
	if (!part) {
		for (i = 0; nflash_parts[i].name; i++) {
			if (from >= nflash_parts[i].offset &&
			((nflash_parts[i+1].name == NULL) || (from < nflash_parts[i+1].offset))) {
				part = &nflash_parts[i];
				break;
			}
		}
		if (!part)
			return -EINVAL;
	}
	/* Check address range */
	if (!len)
		return 0;
	if ((from + len) > mtd->size)
		return -EINVAL;
	offset = from;
	page_size = nflash->nfl->pagesize;
	if ((offset & (page_size - 1)) != 0) {
		extra = offset & (page_size - 1);
		offset -= extra;
		len += extra;
		need_copy = 1;
	}
	size = (len + (page_size - 1)) & ~(page_size - 1);
	if (size != len)
		need_copy = 1;
	if (!need_copy) {
		ptr = buf;
	} else {
		tmpbuf = (uchar *)kmalloc(size, GFP_KERNEL);
		ptr = tmpbuf;
	}

	blocksize = mtd->erasesize;
	mask = blocksize - 1;
	blk_offset = offset & ~mask;
	good_bytes = part->offset & ~mask;
	/* Check and skip bad blocks */
	for (blk_idx = good_bytes/blocksize; blk_idx < mtd->eraseregions->numblocks; blk_idx++) {
		if (nflash->map[blk_idx] != 0) {
			skip_bytes += blocksize;
		} else {
			if (good_bytes == blk_offset)
				break;
			good_bytes += blocksize;
		}
	}
	if (blk_idx == mtd->eraseregions->numblocks) {
		ret = -EINVAL;
		goto done;
	}
	blk_offset = blocksize * blk_idx;
	*retlen = 0;
	while (len > 0) {
		off = offset + skip_bytes;

		/* Check and skip bad blocks */
		if (off >= (blk_offset + blocksize)) {
			blk_offset += blocksize;
			blk_idx++;
			while ((nflash->map[blk_idx] != 0) &&
			       (blk_offset < mtd->size)) {
				skip_bytes += blocksize;
				blk_offset += blocksize;
				blk_idx++;
			}
			if (blk_offset >= mtd->size) {
				ret = -EINVAL;
				goto done;
			}
			off = offset + skip_bytes;
		}

		if ((bytes = hndnand_read(nflash->nfl,
			off, page_size, ptr)) < 0) {
			ret = bytes;
			goto done;
		}
		if (bytes > len)
			bytes = len;
		offset += bytes;
		len -= bytes;
		ptr += bytes;
		*retlen += bytes;
	}

done:
	if (tmpbuf) {
		*retlen -= extra;
		memcpy(buf, tmpbuf+extra, *retlen);
		kfree(tmpbuf);
	}
	return ret;
}

static int
nflash_mtd_read(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	int ret;

	mutex_lock(mtd->mutex);
	ret = _nflash_mtd_read(mtd, NULL, from, len, retlen, buf);
	mutex_unlock(mtd->mutex);

	return ret;
}

static int
nflash_mtd_write(struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const u_char *buf)
{
	struct nflash_mtd *nflash = (struct nflash_mtd *) mtd->priv;
	int bytes, ret = 0;
	struct mtd_partition *part = NULL;
	u_char *block = NULL;
	u_char *ptr = (u_char *)buf;
	uint offset, blocksize, mask, blk_offset, off;
	uint skip_bytes = 0, good_bytes = 0;
	int blk_idx, i;
	int read_len, write_len, copy_len = 0;
	loff_t from = to;
	u_char *write_ptr;
	int docopy = 1;

	/* Locate the part */
	for (i = 0; nflash_parts[i].name; i++) {
		if (to >= nflash_parts[i].offset &&
			((nflash_parts[i+1].name == NULL) ||
			(to < (nflash_parts[i].offset + nflash_parts[i].size)))) {
			part = &nflash_parts[i];
			break;
		}
	}
	if (!part)
		return -EINVAL;
	/* Check address range */
	if (!len)
		return 0;
	if ((to + len) > (part->offset + part->size))
		return -EINVAL;
	offset = to;
	blocksize = mtd->erasesize;
	if (!(block = kmalloc(blocksize, GFP_KERNEL)))
		return -ENOMEM;

	mutex_lock(mtd->mutex);

	mask = blocksize - 1;
	/* Check and skip bad blocks */
	blk_offset = offset & ~mask;
	good_bytes = part->offset & ~mask;
	for (blk_idx = good_bytes/blocksize; blk_idx < (part->offset+part->size)/blocksize;
	blk_idx++) {
		if (nflash->map[blk_idx] != 0) {
			skip_bytes += blocksize;
		} else {
			if (good_bytes == blk_offset)
				break;
			good_bytes += blocksize;
		}
	}
	if (blk_idx == (part->offset+part->size)/blocksize) {
		ret = -EINVAL;
		goto done;
	}
	blk_offset = blocksize * blk_idx;
	/* Backup and erase one block at a time */
	*retlen = 0;
	while (len) {
		if (docopy) {
			/* Align offset */
			from = offset & ~mask;
			/* Copy existing data into holding block if necessary */
			if (((offset & (blocksize-1)) != 0) || (len < blocksize)) {
				ret = _nflash_mtd_read(mtd, part, from, blocksize,
					&read_len, block);
				if (ret)
					goto done;
				if (read_len != blocksize) {
					ret = -EINVAL;
					goto done;
				}
			}
			/* Copy input data into holding block */
			copy_len = min(len, blocksize - (offset & mask));
			memcpy(block + (offset & mask), ptr, copy_len);
		}
		off = (uint) from + skip_bytes;
		/* Erase block */
		if ((ret = hndnand_erase(nflash->nfl, off)) < 0) {
				hndnand_mark_badb(nflash->nfl, off);
				nflash->map[blk_idx] = 1;
				skip_bytes += blocksize;
				docopy = 0;
		}
		else {
			/* Write holding block */
			write_ptr = block;
			write_len = blocksize;
			while (write_len) {
				if ((bytes = hndnand_write(nflash->nfl,
				from + skip_bytes, (uint)write_len,
				(uchar *)write_ptr)) < 0) {
					hndnand_mark_badb(nflash->nfl, off);
					nflash->map[blk_idx] = 1;
					skip_bytes += blocksize;
					docopy = 0;
					break;
				}
				from += bytes;
				write_len -= bytes;
				write_ptr += bytes;
				docopy = 1;
			}
			if (docopy) {
				offset += copy_len;
				len -= copy_len;
				ptr += copy_len;
				*retlen += copy_len;
			}
		}
		/* Check and skip bad blocks */
		if (len) {
			blk_offset += blocksize;
			blk_idx++;
			while ((nflash->map[blk_idx] != 0) &&
			       (blk_offset < (part->offset+part->size))) {
				skip_bytes += blocksize;
				blk_offset += blocksize;
				blk_idx++;
			}
			if (blk_offset >= (part->offset+part->size)) {
				ret = -EINVAL;
				goto done;
			}
		}
	}
done:
	mutex_unlock(mtd->mutex);

	if (block)
		kfree(block);
	return ret;
}

static int
nflash_mtd_erase(struct mtd_info *mtd, struct erase_info *erase)
{
	struct nflash_mtd *nflash = (struct nflash_mtd *) mtd->priv;
	struct mtd_partition *part = NULL;
	int i, ret = 0;
	uint addr, len, blocksize;
	uint part_start_blk, part_end_blk;
	uint blknum, new_addr, erase_blknum;

	addr = erase->addr;
	len = erase->len;

	blocksize = mtd->erasesize;

	/* Check address range */
	if (!len)
		return 0;

	if ((addr + len) > mtd->size)
		return -EINVAL;

	if (addr & (blocksize - 1))
		return -EINVAL;

	/* Locate the part */
	for (i = 0; nflash_parts[i].name; i++) {
		if (addr >= nflash_parts[i].offset &&
			((addr + len) <= (nflash_parts[i].offset + nflash_parts[i].size))) {
			part = &nflash_parts[i];
			break;
		}
	}

	if (!part)
		return -EINVAL;

	mutex_lock(mtd->mutex);

	/* Find the effective start block address to erase */
	part_start_blk = (part->offset & ~(blocksize - 1)) / blocksize;
	part_end_blk = ROUNDUP((part->offset + part->size), blocksize) / blocksize;

	new_addr = part_start_blk * blocksize;
	/* The block number to be skipped relative to the start address of
	 * the MTD partition
	 */
	blknum = (addr - new_addr) / blocksize;

	for (i = part_start_blk; (i < part_end_blk) && (blknum > 0); i++) {
		if (nflash->map[i] != 0) {
			new_addr += blocksize;
		} else {
			new_addr += blocksize;
			blknum--;
		}
	}

	/* Erase the blocks from the new block address */
	erase_blknum = ROUNDUP(len, blocksize) / blocksize;

	if ((new_addr + (erase_blknum * blocksize)) > (part->offset + part->size)) {
		ret = -EINVAL;
		goto done;
	}

	for (i = new_addr; erase_blknum; i += blocksize) {
		/* Skip bad block erase */
		if (nflash->map[i / blocksize] != 0) {
			continue;
		}

		if ((ret = hndnand_erase(nflash->nfl, i)) < 0) {
			hndnand_mark_badb(nflash->nfl, i);
			nflash->map[i / blocksize] = 1;
		} else {
			erase_blknum--;
		}
	}

done:
	/* Set erase status */
	if (ret)
		erase->state = MTD_ERASE_FAILED;
	else
		erase->state = MTD_ERASE_DONE;

	mutex_unlock(mtd->mutex);

	/* Call erase callback */
	if (erase->callback)
		erase->callback(erase);

	return ret;
}

#if LINUX_VERSION_CODE < 0x20212 && defined(MODULE)
#define nflash_mtd_init init_module
#define nflash_mtd_exit cleanup_module
#endif

static int __init
nflash_mtd_init(void)
{
	int ret = 0;
	hndnand_t *info;
	struct pci_dev *dev = NULL;
#ifdef CONFIG_MTD_PARTITIONS
	struct mtd_partition *parts;
	int i;
#endif

	list_for_each_entry(dev, &((pci_find_bus(0, 0))->devices), bus_list) {
		if ((dev != NULL) && (dev->device == CC_CORE_ID))
			break;
	}

	if (dev == NULL) {
		printk(KERN_ERR "nflash: chipcommon not found\n");
		return -ENODEV;
	}

	memset(&nflash, 0, sizeof(struct nflash_mtd));

	/* attach to the backplane */
	if (!(nflash.sih = si_kattach(SI_OSH))) {
		printk(KERN_ERR "nflash: error attaching to backplane\n");
		ret = -EIO;
		goto fail;
	}

	/* Map registers and flash base */
	if (!(nflash.cc = ioremap_nocache(
		pci_resource_start(dev, 0),
		pci_resource_len(dev, 0)))) {
		printk(KERN_ERR "nflash: error mapping registers\n");
		ret = -EIO;
		goto fail;
	}

	/* Initialize serial flash access */
	if (!(info = hndnand_init(nflash.sih))) {
		printk(KERN_ERR "nflash: found no supported devices\n");
		ret = -ENODEV;
		goto fail;
	}
	nflash.nfl = info;

	/* Setup region info */
	nflash.region.offset = 0;
	nflash.region.erasesize = info->blocksize;
	nflash.region.numblocks = info->numblocks;
	if (nflash.region.erasesize > nflash.mtd.erasesize)
		nflash.mtd.erasesize = nflash.region.erasesize;
	/* At most 2GB is supported */
	nflash.mtd.size = (info->size >= (1 << 11)) ? (1 << 31) : (info->size << 20);
	nflash.mtd.numeraseregions = 1;
	nflash.map = (unsigned char *)kmalloc(info->numblocks, GFP_KERNEL);
	if (nflash.map)
		memset(nflash.map, 0, info->numblocks);

	/* Register with MTD */
	nflash.mtd.name = "nflash";
	nflash.mtd.type = MTD_NANDFLASH;
	nflash.mtd.flags = MTD_CAP_NANDFLASH;
	nflash.mtd.eraseregions = &nflash.region;
	nflash.mtd.erase = nflash_mtd_erase;
	nflash.mtd.read = nflash_mtd_read;
	nflash.mtd.write = nflash_mtd_write;
	nflash.mtd.writesize = info->pagesize;
	nflash.mtd.priv = &nflash;
	nflash.mtd.owner = THIS_MODULE;
	nflash.mtd.mutex = partitions_mutex_init();
	if (!nflash.mtd.mutex)
		return -ENOMEM;

	/* Scan bad block */
	mutex_lock(nflash.mtd.mutex);
	for (i = 0; i < info->numblocks; i++) {
		if (hndnand_checkbadb(nflash.nfl, (i * info->blocksize)) != 0) {
			nflash.map[i] = 1;
		}
	}
	mutex_unlock(nflash.mtd.mutex);

#ifdef CONFIG_MTD_PARTITIONS
	parts = init_nflash_mtd_partitions(info, &nflash.mtd, nflash.mtd.size);
	if (!parts)
		goto fail;
	for (i = 0; parts[i].name; i++);
	ret = add_mtd_partitions(&nflash.mtd, parts, i);
	if (ret) {
		printk(KERN_ERR "nflash: add_mtd failed\n");
		goto fail;
	}
	nflash_parts = parts;
#endif
	return 0;

fail:
	if (nflash.cc)
		iounmap((void *) nflash.cc);
	if (nflash.sih)
		si_detach(nflash.sih);
	return ret;
}

static void __exit
nflash_mtd_exit(void)
{
#ifdef CONFIG_MTD_PARTITIONS
	del_mtd_partitions(&nflash.mtd);
#else
	del_mtd_device(&nflash.mtd);
#endif
	iounmap((void *) nflash.cc);
	si_detach(nflash.sih);
}

module_init(nflash_mtd_init);
module_exit(nflash_mtd_exit);
