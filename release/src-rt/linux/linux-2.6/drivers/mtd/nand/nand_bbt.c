/*
 *  drivers/mtd/nand_bbt.c
 *
 *  Overview:
 *   Bad block table support for the NAND driver
 *
 *  Copyright (C) 2004 Thomas Gleixner (tglx@linutronix.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Description:
 *
 * When nand_scan_bbt is called, then it tries to find the bad block table
 * depending on the options in the BBT descriptor(s). If no flash based BBT
 * (NAND_USE_FLASH_BBT) is specified then the device is scanned for factory
 * marked good / bad blocks. This information is used to create a memory BBT.
 * Once a new bad block is discovered then the "factory" information is updated
 * on the device.
 * If a flash based BBT is specified then the function first tries to find the
 * BBT on flash. If a BBT is found then the contents are read and the memory
 * based BBT is created. If a mirrored BBT is selected then the mirror is
 * searched too and the versions are compared. If the mirror has a greater
 * version number than the mirror BBT is used to build the memory based BBT.
 * If the tables are not versioned, then we "or" the bad block information.
 * If one of the BBTs is out of date or does not exist it is (re)created.
 * If no BBT exists at all then the device is scanned for factory marked
 * good / bad blocks and the bad block tables are created.
 *
 * For manufacturer created BBTs like the one found on M-SYS DOC devices
 * the BBT is searched and read but never created
 *
 * The auto generated bad block table is located in the last good blocks
 * of the device. The table is mirrored, so it can be updated eventually.
 * The table is marked in the OOB area with an ident pattern and a version
 * number which indicates which of both tables is more up to date. If the NAND
 * controller needs the complete OOB area for the ECC information then the
 * option NAND_USE_FLASH_BBT_NO_OOB should be used: it moves the ident pattern
 * and the version byte into the data area and the OOB area will remain
 * untouched.
 *
 * The table uses 2 bits per block
 * 11b:		block is good
 * 00b:		block is factory marked bad
 * 01b, 10b:	block is marked bad due to wear
 *
 * The memory bad block table uses the following scheme:
 * 00b:		block is good
 * 01b:		block is marked bad due to wear
 * 10b:		block is reserved (to protect the bbt area)
 * 11b:		block is factory marked bad
 *
 * Multichip devices like DOC store the bad block info per floor.
 *
 * Following assumptions are made:
 * - bbts start at a page boundary, if autolocated on a block boundary
 * - the space necessary for a bbt in FLASH does not exceed a block boundary
 *
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>

static int check_pattern_no_oob(uint8_t *buf, struct nand_bbt_descr *td)
{
	int ret;

	ret = memcmp(buf, td->pattern, td->len);
	if (!ret)
		return ret;
	return -1;
}

/**
 * check_pattern - [GENERIC] check if a pattern is in the buffer
 * @buf:	the buffer to search
 * @len:	the length of buffer to search
 * @paglen:	the pagelength
 * @td:		search pattern descriptor
 *
 * Check for a pattern at the given place. Used to search bad block
 * tables and good / bad block identifiers.
 * If the SCAN_EMPTY option is set then check, if all bytes except the
 * pattern area contain 0xff
 *
*/
static int check_pattern(uint8_t *buf, int len, int paglen, struct nand_bbt_descr *td)
{
	int i, end = 0;
	uint8_t *p = buf;

	if (td->options & NAND_BBT_NO_OOB)
		return check_pattern_no_oob(buf, td);

	end = paglen + td->offs;
	if (td->options & NAND_BBT_SCANEMPTY) {
		for (i = 0; i < end; i++) {
			if (p[i] != 0xff)
				return -1;
		}
	}
	p += end;

	/* Compare the pattern */
	for (i = 0; i < td->len; i++) {
		if (p[i] != td->pattern[i])
			return -1;
	}

	/* Check both positions 1 and 6 for pattern? */
	if (td->options & NAND_BBT_SCANBYTE1AND6) {
		if (td->options & NAND_BBT_SCANEMPTY) {
			p += td->len;
			end += NAND_SMALL_BADBLOCK_POS - td->offs;
			/* Check region between positions 1 and 6 */
			for (i = 0; i < NAND_SMALL_BADBLOCK_POS - td->offs - td->len;
					i++) {
				if (*p++ != 0xff)
					return -1;
			}
		}
		else {
			p += NAND_SMALL_BADBLOCK_POS - td->offs;
		}
		/* Compare the pattern */
		for (i = 0; i < td->len; i++) {
			if (p[i] != td->pattern[i])
				return -1;
		}
	}

	if (td->options & NAND_BBT_SCANEMPTY) {
		p += td->len;
		end += td->len;
		for (i = end; i < len; i++) {
			if (*p++ != 0xff)
				return -1;
		}
	}
	return 0;
}

/**
 * check_short_pattern - [GENERIC] check if a pattern is in the buffer
 * @buf:	the buffer to search
 * @td:		search pattern descriptor
 *
 * Check for a pattern at the given place. Used to search bad block
 * tables and good / bad block identifiers. Same as check_pattern, but
 * no optional empty check
 *
*/
static int check_short_pattern(uint8_t *buf, struct nand_bbt_descr *td)
{
	int i;
	uint8_t *p = buf;

	/* Compare the pattern */
	for (i = 0; i < td->len; i++) {
		if (p[td->offs + i] != td->pattern[i])
			return -1;
	}
	/* Need to check location 1 AND 6? */
	if (td->options & NAND_BBT_SCANBYTE1AND6) {
		for (i = 0; i < td->len; i++) {
			if (p[NAND_SMALL_BADBLOCK_POS + i] != td->pattern[i])
				return -1;
		}
	}
	return 0;
}

/**
 * add_marker_len - compute the length of the marker in data area
 * @td:		BBT descriptor used for computation
 *
 * The length will be 0 if the markeris located in OOB area.
 */
static u32 add_marker_len(struct nand_bbt_descr *td)
{
	u32 len;

	if (!(td->options & NAND_BBT_NO_OOB))
		return 0;

	len = td->len;
	if (td->options & NAND_BBT_VERSION)
		len++;
	return len;
}

/**
 * read_bbt - [GENERIC] Read the bad block table starting from page
 * @mtd:	MTD device structure
 * @buf:	temporary buffer
 * @page:	the starting page
 * @num:	the number of bbt descriptors to read
 * @td:		the bbt describtion table
 * @offs:	offset in the memory table
 *
 * Read the bad block table starting from page.
 *
 */
static int read_bbt(struct mtd_info *mtd, uint8_t *buf, int page, int num,
		struct nand_bbt_descr *td, int offs)
{
	int res, i, j, act = 0;
	struct nand_chip *this = mtd->priv;
	size_t retlen, len, totlen;
	loff_t from;
	int bits = td->options & NAND_BBT_NRBITS_MSK;
	uint8_t msk = (uint8_t) ((1 << bits) - 1);
	u32 marker_len;
	int reserved_block_code = td->reserved_block_code;

	totlen = (num * bits) >> 3;
	marker_len = add_marker_len(td);
	from = ((loff_t) page) << this->page_shift;

	while (totlen) {
		len = min(totlen, (size_t) (1 << this->bbt_erase_shift));
		if (marker_len) {
			/*
			 * In case the BBT marker is not in the OOB area it
			 * will be just in the first page.
			 */
			len -= marker_len;
			from += marker_len;
			marker_len = 0;
		}
		res = mtd->read(mtd, from, len, &retlen, buf);
		if (res < 0) {
			if (retlen != len) {
				printk(KERN_INFO "nand_bbt: Error reading bad block table\n");
				return res;
			}
			printk(KERN_WARNING "nand_bbt: ECC error while reading bad block table\n");
		}

		/* Analyse data */
		for (i = 0; i < len; i++) {
			uint8_t dat = buf[i];
			for (j = 0; j < 8; j += bits, act += 2) {
				uint8_t tmp = (dat >> j) & msk;
				if (tmp == msk)
					continue;
				if (reserved_block_code && (tmp == reserved_block_code)) {
					printk(KERN_DEBUG "nand_read_bbt: Reserved block at 0x%012llx\n",
					       (loff_t)((offs << 2) + (act >> 1)) << this->bbt_erase_shift);
					this->bbt[offs + (act >> 3)] |= 0x2 << (act & 0x06);
					mtd->ecc_stats.bbtblocks++;
					continue;
				}
				/* Leave it for now, if its matured we can move this
				 * message to MTD_DEBUG_LEVEL0 */
				printk(KERN_DEBUG "nand_read_bbt: Bad block at 0x%012llx\n",
				       (loff_t)((offs << 2) + (act >> 1)) << this->bbt_erase_shift);
				/* Factory marked bad or worn out ? */
				if (tmp == 0)
					this->bbt[offs + (act >> 3)] |= 0x3 << (act & 0x06);
				else
					this->bbt[offs + (act >> 3)] |= 0x1 << (act & 0x06);
				mtd->ecc_stats.badblocks++;
			}
		}
		totlen -= len;
		from += len;
	}
	return 0;
}

/**
 * read_abs_bbt - [GENERIC] Read the bad block table starting at a given page
 * @mtd:	MTD device structure
 * @buf:	temporary buffer
 * @td:		descriptor for the bad block table
 * @chip:	read the table for a specific chip, -1 read all chips.
 *		Applies only if NAND_BBT_PERCHIP option is set
 *
 * Read the bad block table for all chips starting at a given page
 * We assume that the bbt bits are in consecutive order.
*/
static int read_abs_bbt(struct mtd_info *mtd, uint8_t *buf, struct nand_bbt_descr *td, int chip)
{
	struct nand_chip *this = mtd->priv;
	int res = 0, i;

	if (td->options & NAND_BBT_PERCHIP) {
		int offs = 0;
		for (i = 0; i < this->numchips; i++) {
			if (chip == -1 || chip == i)
				res = read_bbt(mtd, buf, td->pages[i],
					this->chipsize >> this->bbt_erase_shift,
					td, offs);
			if (res)
				return res;
			offs += this->chipsize >> (this->bbt_erase_shift + 2);
		}
	} else {
		res = read_bbt(mtd, buf, td->pages[0],
				mtd->size >> this->bbt_erase_shift, td, 0);
		if (res)
			return res;
	}
	return 0;
}

/*
 * BBT marker is in the first page, no OOB.
 */
static int scan_read_raw_data(struct mtd_info *mtd, uint8_t *buf, loff_t offs,
			 struct nand_bbt_descr *td)
{
	size_t retlen;
	size_t len;

	len = td->len;
	if (td->options & NAND_BBT_VERSION)
		len++;

	return mtd->read(mtd, offs, len, &retlen, buf);
}

/*
 * Scan read raw data from flash
 */
static int scan_read_raw_oob(struct mtd_info *mtd, uint8_t *buf, loff_t offs,
			 size_t len)
{
	struct mtd_oob_ops ops;
	int res;

	ops.mode = MTD_OOB_RAW;
	ops.ooboffs = 0;
	ops.ooblen = mtd->oobsize;


	while (len > 0) {
		if (len <= mtd->writesize) {
			ops.oobbuf = buf + len;
			ops.datbuf = buf;
			ops.len = len;
			return mtd->read_oob(mtd, offs, &ops);
		} else {
			ops.oobbuf = buf + mtd->writesize;
			ops.datbuf = buf;
			ops.len = mtd->writesize;
			res = mtd->read_oob(mtd, offs, &ops);

			if (res)
				return res;
		}

		buf += mtd->oobsize + mtd->writesize;
		len -= mtd->writesize;
	}
	return 0;
}

static int scan_read_raw(struct mtd_info *mtd, uint8_t *buf, loff_t offs,
			 size_t len, struct nand_bbt_descr *td)
{
	if (td->options & NAND_BBT_NO_OOB)
		return scan_read_raw_data(mtd, buf, offs, td);
	else
		return scan_read_raw_oob(mtd, buf, offs, len);
}

/*
 * Scan write data with oob to flash
 */
static int scan_write_bbt(struct mtd_info *mtd, loff_t offs, size_t len,
			  uint8_t *buf, uint8_t *oob)
{
	struct mtd_oob_ops ops;

	ops.mode = MTD_OOB_PLACE;
	ops.ooboffs = 0;
	ops.ooblen = mtd->oobsize;
	ops.datbuf = buf;
	ops.oobbuf = oob;
	ops.len = len;

	return mtd->write_oob(mtd, offs, &ops);
}

static u32 bbt_get_ver_offs(struct mtd_info *mtd, struct nand_bbt_descr *td)
{
	u32 ver_offs = td->veroffs;

	if (!(td->options & NAND_BBT_NO_OOB))
		ver_offs += mtd->writesize;
	return ver_offs;
}

/**
 * read_abs_bbts - [GENERIC] Read the bad block table(s) for all chips starting at a given page
 * @mtd:	MTD device structure
 * @buf:	temporary buffer
 * @td:		descriptor for the bad block table
 * @md:		descriptor for the bad block table mirror
 *
 * Read the bad block table(s) for all chips starting at a given page
 * We assume that the bbt bits are in consecutive order.
 *
*/
static int read_abs_bbts(struct mtd_info *mtd, uint8_t *buf,
			 struct nand_bbt_descr *td, struct nand_bbt_descr *md)
{
	struct nand_chip *this = mtd->priv;

	/* Read the primary version, if available */
	if (td->options & NAND_BBT_VERSION) {
		scan_read_raw(mtd, buf, (loff_t)td->pages[0] << this->page_shift,
			      mtd->writesize, td);
		td->version[0] = buf[bbt_get_ver_offs(mtd, td)];
		printk(KERN_DEBUG "Bad block table at page %d, version 0x%02X\n",
		       td->pages[0], td->version[0]);
	}

	/* Read the mirror version, if available */
	if (md && (md->options & NAND_BBT_VERSION)) {
		scan_read_raw(mtd, buf, (loff_t)md->pages[0] << this->page_shift,
			      mtd->writesize, td);
		md->version[0] = buf[bbt_get_ver_offs(mtd, md)];
		printk(KERN_DEBUG "Bad block table at page %d, version 0x%02X\n",
		       md->pages[0], md->version[0]);
	}
	return 1;
}

/*
 * Scan a given block full
 */
static int scan_block_full(struct mtd_info *mtd, struct nand_bbt_descr *bd,
			   loff_t offs, uint8_t *buf, size_t readlen,
			   int scanlen, int len)
{
	int ret, j;

	ret = scan_read_raw_oob(mtd, buf, offs, readlen);
	if (ret)
		return ret;

	for (j = 0; j < len; j++, buf += scanlen) {
		if (check_pattern(buf, scanlen, mtd->writesize, bd))
			return 1;
	}
	return 0;
}

/*
 * Scan a given block partially
 */
static int scan_block_fast(struct mtd_info *mtd, struct nand_bbt_descr *bd,
			   loff_t offs, uint8_t *buf, int len)
{
	struct mtd_oob_ops ops;
	int j, ret;

	ops.ooblen = mtd->oobsize;
	ops.oobbuf = buf;
	ops.ooboffs = 0;
	ops.datbuf = NULL;
	ops.mode = MTD_OOB_PLACE;

	for (j = 0; j < len; j++) {
		/*
		 * Read the full oob until read_oob is fixed to
		 * handle single byte reads for 16 bit
		 * buswidth
		 */
		ret = mtd->read_oob(mtd, offs, &ops);
		if (ret)
			return ret;

		if (check_short_pattern(buf, bd))
			return 1;

		offs += mtd->writesize;
	}
	return 0;
}

/**
 * create_bbt - [GENERIC] Create a bad block table by scanning the device
 * @mtd:	MTD device structure
 * @buf:	temporary buffer
 * @bd:		descriptor for the good/bad block search pattern
 * @chip:	create the table for a specific chip, -1 read all chips.
 *		Applies only if NAND_BBT_PERCHIP option is set
 *
 * Create a bad block table by scanning the device
 * for the given good/bad block identify pattern
 */
static int create_bbt(struct mtd_info *mtd, uint8_t *buf,
	struct nand_bbt_descr *bd, int chip)
{
	struct nand_chip *this = mtd->priv;
	int i, numblocks, len, scanlen;
	int startblock;
	loff_t from;
	size_t readlen;

	printk(KERN_INFO "Scanning device for bad blocks\n");

	if (bd->options & NAND_BBT_SCANALLPAGES)
		len = 1 << (this->bbt_erase_shift - this->page_shift);
	else if (bd->options & NAND_BBT_SCAN2NDPAGE)
		len = 2;
	else
		len = 1;

	if (!(bd->options & NAND_BBT_SCANEMPTY)) {
		/* We need only read few bytes from the OOB area */
		scanlen = 0;
		readlen = bd->len;
	} else {
		/* Full page content should be read */
		scanlen = mtd->writesize + mtd->oobsize;
		readlen = len * mtd->writesize;
	}

	if (chip == -1) {
		/* Note that numblocks is 2 * (real numblocks) here, see i+=2
		 * below as it makes shifting and masking less painful */
		numblocks = mtd->size >> (this->bbt_erase_shift - 1);
		startblock = 0;
		from = 0;
	} else {
		if (chip >= this->numchips) {
			printk(KERN_WARNING "create_bbt(): chipnr (%d) > available chips (%d)\n",
			       chip + 1, this->numchips);
			return -EINVAL;
		}
		numblocks = this->chipsize >> (this->bbt_erase_shift - 1);
		startblock = chip * numblocks;
		numblocks += startblock;
		from = (loff_t)startblock << (this->bbt_erase_shift - 1);
	}

	if (this->options & NAND_BBT_SCANLASTPAGE)
		from += mtd->erasesize - (mtd->writesize * len);

	for (i = startblock; i < numblocks;) {
		int ret;

		BUG_ON(bd->options & NAND_BBT_NO_OOB);

		if (bd->options & NAND_BBT_SCANALLPAGES)
			ret = scan_block_full(mtd, bd, from, buf, readlen,
					      scanlen, len);
		else
			ret = scan_block_fast(mtd, bd, from, buf, len);

		if (ret < 0)
			return ret;

		if (ret) {
			this->bbt[i >> 3] |= 0x03 << (i & 0x6);
			printk(KERN_WARNING "Bad eraseblock %d at 0x%012llx\n",
			       i >> 1, (unsigned long long)from);
			mtd->ecc_stats.badblocks++;
		}

		i += 2;
		from += (1 << this->bbt_erase_shift);
	}
	return 0;
}

/**
 * search_bbt - [GENERIC] scan the device for a specific bad block table
 * @mtd:	MTD device structure
 * @buf:	temporary buffer
 * @td:		descriptor for the bad block table
 *
 * Read the bad block table by searching for a given ident pattern.
 * Search is preformed either from the beginning up or from the end of
 * the device downwards. The search starts always at the start of a
 * block.
 * If the option NAND_BBT_PERCHIP is given, each chip is searched
 * for a bbt, which contains the bad block information of this chip.
 * This is necessary to provide support for certain DOC devices.
 *
 * The bbt ident pattern resides in the oob area of the first page
 * in a block.
 */
static int search_bbt(struct mtd_info *mtd, uint8_t *buf, struct nand_bbt_descr *td)
{
	struct nand_chip *this = mtd->priv;
	int i, chips;
	int bits, startblock, block, dir;
	int scanlen = mtd->writesize + mtd->oobsize;
	int bbtblocks;
	int blocktopage = this->bbt_erase_shift - this->page_shift;

	/* Search direction top -> down ? */
	if (td->options & NAND_BBT_LASTBLOCK) {
		startblock = (mtd->size >> this->bbt_erase_shift) - 1;
		dir = -1;
	} else {
		startblock = 0;
		dir = 1;
	}

	/* Do we have a bbt per chip ? */
	if (td->options & NAND_BBT_PERCHIP) {
		chips = this->numchips;
		bbtblocks = this->chipsize >> this->bbt_erase_shift;
		startblock &= bbtblocks - 1;
	} else {
		chips = 1;
		bbtblocks = mtd->size >> this->bbt_erase_shift;
	}

	/* Number of bits for each erase block in the bbt */
	bits = td->options & NAND_BBT_NRBITS_MSK;

	for (i = 0; i < chips; i++) {
		/* Reset version information */
		td->version[i] = 0;
		td->pages[i] = -1;
		/* Scan the maximum number of blocks */
		for (block = 0; block < td->maxblocks; block++) {

			int actblock = startblock + dir * block;
			loff_t offs = (loff_t)actblock << this->bbt_erase_shift;

			/* Read first page */
			scan_read_raw(mtd, buf, offs, mtd->writesize, td);
			if (!check_pattern(buf, scanlen, mtd->writesize, td)) {
				td->pages[i] = actblock << blocktopage;
				if (td->options & NAND_BBT_VERSION) {
					offs = bbt_get_ver_offs(mtd, td);
					td->version[i] = buf[offs];
				}
				break;
			}
		}
		startblock += this->chipsize >> this->bbt_erase_shift;
	}
	/* Check, if we found a bbt for each requested chip */
	for (i = 0; i < chips; i++) {
		if (td->pages[i] == -1)
			printk(KERN_WARNING "Bad block table not found for chip %d\n", i);
		else
			printk(KERN_DEBUG "Bad block table found at page %d, version 0x%02X\n", td->pages[i],
			       td->version[i]);
	}
	return 0;
}

/**
 * search_read_bbts - [GENERIC] scan the device for bad block table(s)
 * @mtd:	MTD device structure
 * @buf:	temporary buffer
 * @td:		descriptor for the bad block table
 * @md:		descriptor for the bad block table mirror
 *
 * Search and read the bad block table(s)
*/
static int search_read_bbts(struct mtd_info *mtd, uint8_t * buf, struct nand_bbt_descr *td, struct nand_bbt_descr *md)
{
	/* Search the primary table */
	search_bbt(mtd, buf, td);

	/* Search the mirror table */
	if (md)
		search_bbt(mtd, buf, md);

	/* Force result check */
	return 1;
}

/**
 * write_bbt - [GENERIC] (Re)write the bad block table
 *
 * @mtd:	MTD device structure
 * @buf:	temporary buffer
 * @td:		descriptor for the bad block table
 * @md:		descriptor for the bad block table mirror
 * @chipsel:	selector for a specific chip, -1 for all
 *
 * (Re)write the bad block table
 *
*/
static int write_bbt(struct mtd_info *mtd, uint8_t *buf,
		     struct nand_bbt_descr *td, struct nand_bbt_descr *md,
		     int chipsel)
{
	struct nand_chip *this = mtd->priv;
	struct erase_info einfo;
	int i, j, res, chip = 0;
	int bits, startblock, dir, page, offs, numblocks, sft, sftmsk;
	int nrchips, bbtoffs, pageoffs, ooboffs;
	uint8_t msk[4];
	uint8_t rcode = td->reserved_block_code;
	size_t retlen, len = 0;
	loff_t to;
	struct mtd_oob_ops ops;

	ops.ooblen = mtd->oobsize;
	ops.ooboffs = 0;
	ops.datbuf = NULL;
	ops.mode = MTD_OOB_PLACE;

	if (!rcode)
		rcode = 0xff;
	/* Write bad block table per chip rather than per device ? */
	if (td->options & NAND_BBT_PERCHIP) {
		numblocks = (int)(this->chipsize >> this->bbt_erase_shift);
		/* Full device write or specific chip ? */
		if (chipsel == -1) {
			nrchips = this->numchips;
		} else {
			nrchips = chipsel + 1;
			chip = chipsel;
		}
	} else {
		numblocks = (int)(mtd->size >> this->bbt_erase_shift);
		nrchips = 1;
	}

	/* Loop through the chips */
	for (; chip < nrchips; chip++) {

		/* There was already a version of the table, reuse the page
		 * This applies for absolute placement too, as we have the
		 * page nr. in td->pages.
		 */
		if (td->pages[chip] != -1) {
			page = td->pages[chip];
			goto write;
		}

		/* Automatic placement of the bad block table */
		/* Search direction top -> down ? */
		if (td->options & NAND_BBT_LASTBLOCK) {
			startblock = numblocks * (chip + 1) - 1;
			dir = -1;
		} else {
			startblock = chip * numblocks;
			dir = 1;
		}

		for (i = 0; i < td->maxblocks; i++) {
			int block = startblock + dir * i;
			/* Check, if the block is bad */
			switch ((this->bbt[block >> 2] >>
				 (2 * (block & 0x03))) & 0x03) {
			case 0x01:
			case 0x03:
				continue;
			}
			page = block <<
				(this->bbt_erase_shift - this->page_shift);
			/* Check, if the block is used by the mirror table */
			if (!md || md->pages[chip] != page)
				goto write;
		}
		printk(KERN_ERR "No space left to write bad block table\n");
		return -ENOSPC;
	write:

		/* Set up shift count and masks for the flash table */
		bits = td->options & NAND_BBT_NRBITS_MSK;
		msk[2] = ~rcode;
		switch (bits) {
		case 1: sft = 3; sftmsk = 0x07; msk[0] = 0x00; msk[1] = 0x01;
			msk[3] = 0x01;
			break;
		case 2: sft = 2; sftmsk = 0x06; msk[0] = 0x00; msk[1] = 0x01;
			msk[3] = 0x03;
			break;
		case 4: sft = 1; sftmsk = 0x04; msk[0] = 0x00; msk[1] = 0x0C;
			msk[3] = 0x0f;
			break;
		case 8: sft = 0; sftmsk = 0x00; msk[0] = 0x00; msk[1] = 0x0F;
			msk[3] = 0xff;
			break;
		default: return -EINVAL;
		}

		bbtoffs = chip * (numblocks >> 2);

		to = ((loff_t) page) << this->page_shift;

		/* Must we save the block contents ? */
		if (td->options & NAND_BBT_SAVECONTENT) {
			/* Make it block aligned */
			to &= ~((loff_t) ((1 << this->bbt_erase_shift) - 1));
			len = 1 << this->bbt_erase_shift;
			res = mtd->read(mtd, to, len, &retlen, buf);
			if (res < 0) {
				if (retlen != len) {
					printk(KERN_INFO "nand_bbt: Error "
					       "reading block for writing "
					       "the bad block table\n");
					return res;
				}
				printk(KERN_WARNING "nand_bbt: ECC error "
				       "while reading block for writing "
				       "bad block table\n");
			}
			/* Read oob data */
			ops.ooblen = (len >> this->page_shift) * mtd->oobsize;
			ops.oobbuf = &buf[len];
			res = mtd->read_oob(mtd, to + mtd->writesize, &ops);
			if (res < 0 || ops.oobretlen != ops.ooblen)
				goto outerr;

			/* Calc the byte offset in the buffer */
			pageoffs = page - (int)(to >> this->page_shift);
			offs = pageoffs << this->page_shift;
			/* Preset the bbt area with 0xff */
			memset(&buf[offs], 0xff, (size_t) (numblocks >> sft));
			ooboffs = len + (pageoffs * mtd->oobsize);

		} else if (td->options & NAND_BBT_NO_OOB) {
			ooboffs = 0;
			offs = td->len;
			/* the version byte */
			if (td->options & NAND_BBT_VERSION)
				offs++;
			/* Calc length */
			len = (size_t) (numblocks >> sft);
			len += offs;
			/* Make it page aligned ! */
			len = ALIGN(len, mtd->writesize);
			/* Preset the buffer with 0xff */
			memset(buf, 0xff, len);
			/* Pattern is located at the begin of first page */
			memcpy(buf, td->pattern, td->len);
		} else {
			/* Calc length */
			len = (size_t) (numblocks >> sft);
			/* Make it page aligned ! */
			len = ALIGN(len, mtd->writesize);
			/* Preset the buffer with 0xff */
			memset(buf, 0xff, len +
			       (len >> this->page_shift)* mtd->oobsize);
			offs = 0;
			ooboffs = len;
			/* Pattern is located in oob area of first page */
			memcpy(&buf[ooboffs + td->offs], td->pattern, td->len);
		}

		if (td->options & NAND_BBT_VERSION)
			buf[ooboffs + td->veroffs] = td->version[chip];

		/* walk through the memory table */
		for (i = 0; i < numblocks;) {
			uint8_t dat;
			dat = this->bbt[bbtoffs + (i >> 2)];
			for (j = 0; j < 4; j++, i++) {
				int sftcnt = (i << (3 - sft)) & sftmsk;
				/* Do not store the reserved bbt blocks ! */
				buf[offs + (i >> sft)] &=
					~(msk[dat & 0x03] << sftcnt);
				dat >>= 2;
			}
		}

		memset(&einfo, 0, sizeof(einfo));
		einfo.mtd = mtd;
		einfo.addr = to;
		einfo.len = 1 << this->bbt_erase_shift;
		res = nand_erase_nand(mtd, &einfo, 1);
		if (res < 0)
			goto outerr;

		res = scan_write_bbt(mtd, to, len, buf,
				td->options & NAND_BBT_NO_OOB ? NULL :
				&buf[len]);
		if (res < 0)
			goto outerr;

		printk(KERN_DEBUG "Bad block table written to 0x%012llx, version "
		       "0x%02X\n", (unsigned long long)to, td->version[chip]);

		/* Mark it as used */
		td->pages[chip] = page;
	}
	return 0;

 outerr:
	printk(KERN_WARNING
	       "nand_bbt: Error while writing bad block table %d\n", res);
	return res;
}

/**
 * nand_memory_bbt - [GENERIC] create a memory based bad block table
 * @mtd:	MTD device structure
 * @bd:		descriptor for the good/bad block search pattern
 *
 * The function creates a memory based bbt by scanning the device
 * for manufacturer / software marked good / bad blocks
*/
static inline int nand_memory_bbt(struct mtd_info *mtd, struct nand_bbt_descr *bd)
{
	struct nand_chip *this = mtd->priv;

	bd->options &= ~NAND_BBT_SCANEMPTY;
	return create_bbt(mtd, this->buffers->databuf, bd, -1);
}

/**
 * check_create - [GENERIC] create and write bbt(s) if necessary
 * @mtd:	MTD device structure
 * @buf:	temporary buffer
 * @bd:		descriptor for the good/bad block search pattern
 *
 * The function checks the results of the previous call to read_bbt
 * and creates / updates the bbt(s) if necessary
 * Creation is necessary if no bbt was found for the chip/device
 * Update is necessary if one of the tables is missing or the
 * version nr. of one table is less than the other
*/
static int check_create(struct mtd_info *mtd, uint8_t *buf, struct nand_bbt_descr *bd)
{
	int i, chips, writeops, chipsel, res;
	struct nand_chip *this = mtd->priv;
	struct nand_bbt_descr *td = this->bbt_td;
	struct nand_bbt_descr *md = this->bbt_md;
	struct nand_bbt_descr *rd, *rd2;

	/* Do we have a bbt per chip ? */
	if (td->options & NAND_BBT_PERCHIP)
		chips = this->numchips;
	else
		chips = 1;

	for (i = 0; i < chips; i++) {
		writeops = 0;
		rd = NULL;
		rd2 = NULL;
		/* Per chip or per device ? */
		chipsel = (td->options & NAND_BBT_PERCHIP) ? i : -1;
		/* Mirrored table available ? */
		if (md) {
			if (td->pages[i] == -1 && md->pages[i] == -1) {
				writeops = 0x03;
				goto create;
			}

			if (td->pages[i] == -1) {
				rd = md;
				td->version[i] = md->version[i];
				writeops = 1;
				goto writecheck;
			}

			if (md->pages[i] == -1) {
				rd = td;
				md->version[i] = td->version[i];
				writeops = 2;
				goto writecheck;
			}

			if (td->version[i] == md->version[i]) {
				rd = td;
				if (!(td->options & NAND_BBT_VERSION))
					rd2 = md;
				goto writecheck;
			}

			if (((int8_t) (td->version[i] - md->version[i])) > 0) {
				rd = td;
				md->version[i] = td->version[i];
				writeops = 2;
			} else {
				rd = md;
				td->version[i] = md->version[i];
				writeops = 1;
			}

			goto writecheck;

		} else {
			if (td->pages[i] == -1) {
				writeops = 0x01;
				goto create;
			}
			rd = td;
			goto writecheck;
		}
	create:
		/* Create the bad block table by scanning the device ? */
		if (!(td->options & NAND_BBT_CREATE))
			continue;

		/* Create the table in memory by scanning the chip(s) */
		if (!(this->options & NAND_CREATE_EMPTY_BBT))
			create_bbt(mtd, buf, bd, chipsel);

		td->version[i] = 1;
		if (md)
			md->version[i] = 1;
	writecheck:
		/* read back first ? */
		if (rd)
			read_abs_bbt(mtd, buf, rd, chipsel);
		/* If they weren't versioned, read both. */
		if (rd2)
			read_abs_bbt(mtd, buf, rd2, chipsel);

		/* Write the bad block table to the device ? */
		if ((writeops & 0x01) && (td->options & NAND_BBT_WRITE)) {
			res = write_bbt(mtd, buf, td, md, chipsel);
			if (res < 0)
				return res;
		}

		/* Write the mirror bad block table to the device ? */
		if ((writeops & 0x02) && md && (md->options & NAND_BBT_WRITE)) {
			res = write_bbt(mtd, buf, md, td, chipsel);
			if (res < 0)
				return res;
		}
	}
	return 0;
}

/**
 * mark_bbt_regions - [GENERIC] mark the bad block table regions
 * @mtd:	MTD device structure
 * @td:		bad block table descriptor
 *
 * The bad block table regions are marked as "bad" to prevent
 * accidental erasures / writes. The regions are identified by
 * the mark 0x02.
*/
static void mark_bbt_region(struct mtd_info *mtd, struct nand_bbt_descr *td)
{
	struct nand_chip *this = mtd->priv;
	int i, j, chips, block, nrblocks, update;
	uint8_t oldval, newval;

	/* Do we have a bbt per chip ? */
	if (td->options & NAND_BBT_PERCHIP) {
		chips = this->numchips;
		nrblocks = (int)(this->chipsize >> this->bbt_erase_shift);
	} else {
		chips = 1;
		nrblocks = (int)(mtd->size >> this->bbt_erase_shift);
	}

	for (i = 0; i < chips; i++) {
		if ((td->options & NAND_BBT_ABSPAGE) ||
		    !(td->options & NAND_BBT_WRITE)) {
			if (td->pages[i] == -1)
				continue;
			block = td->pages[i] >> (this->bbt_erase_shift - this->page_shift);
			block <<= 1;
			oldval = this->bbt[(block >> 3)];
			newval = oldval | (0x2 << (block & 0x06));
			this->bbt[(block >> 3)] = newval;
			if ((oldval != newval) && td->reserved_block_code)
				nand_update_bbt(mtd, (loff_t)block << (this->bbt_erase_shift - 1));
			continue;
		}
		update = 0;
		if (td->options & NAND_BBT_LASTBLOCK)
			block = ((i + 1) * nrblocks) - td->maxblocks;
		else
			block = i * nrblocks;
		block <<= 1;
		for (j = 0; j < td->maxblocks; j++) {
			oldval = this->bbt[(block >> 3)];
			newval = oldval | (0x2 << (block & 0x06));
			this->bbt[(block >> 3)] = newval;
			if (oldval != newval)
				update = 1;
			block += 2;
		}
		/* If we want reserved blocks to be recorded to flash, and some
		   new ones have been marked, then we need to update the stored
		   bbts.  This should only happen once. */
		if (update && td->reserved_block_code)
			nand_update_bbt(mtd, (loff_t)(block - 2) << (this->bbt_erase_shift - 1));
	}
}

/**
 * verify_bbt_descr - verify the bad block description
 * @mtd:	MTD device structure
 * @bd:		the table to verify
 *
 * This functions performs a few sanity checks on the bad block description
 * table.
 */
static void verify_bbt_descr(struct mtd_info *mtd, struct nand_bbt_descr *bd)
{
	struct nand_chip *this = mtd->priv;
	u32 pattern_len;
	u32 bits;
	u32 table_size;

	if (!bd)
		return;

	pattern_len = bd->len;
	bits = bd->options & NAND_BBT_NRBITS_MSK;

	BUG_ON((this->options & NAND_USE_FLASH_BBT_NO_OOB) &&
			!(this->options & NAND_USE_FLASH_BBT));
	BUG_ON(!bits);

	if (bd->options & NAND_BBT_VERSION)
		pattern_len++;

	if (bd->options & NAND_BBT_NO_OOB) {
		BUG_ON(!(this->options & NAND_USE_FLASH_BBT));
		BUG_ON(!(this->options & NAND_USE_FLASH_BBT_NO_OOB));
		BUG_ON(bd->offs);
		if (bd->options & NAND_BBT_VERSION)
			BUG_ON(bd->veroffs != bd->len);
		BUG_ON(bd->options & NAND_BBT_SAVECONTENT);
	}

	if (bd->options & NAND_BBT_PERCHIP)
		table_size = this->chipsize >> this->bbt_erase_shift;
	else
		table_size = mtd->size >> this->bbt_erase_shift;
	table_size >>= 3;
	table_size *= bits;
	if (bd->options & NAND_BBT_NO_OOB)
		table_size += pattern_len;
	BUG_ON(table_size > (1 << this->bbt_erase_shift));
}

/**
 * nand_scan_bbt - [NAND Interface] scan, find, read and maybe create bad block table(s)
 * @mtd:	MTD device structure
 * @bd:		descriptor for the good/bad block search pattern
 *
 * The function checks, if a bad block table(s) is/are already
 * available. If not it scans the device for manufacturer
 * marked good / bad blocks and writes the bad block table(s) to
 * the selected place.
 *
 * The bad block table memory is allocated here. It must be freed
 * by calling the nand_free_bbt function.
 *
*/
int nand_scan_bbt(struct mtd_info *mtd, struct nand_bbt_descr *bd)
{
	struct nand_chip *this = mtd->priv;
	int len, res = 0;
	uint8_t *buf;
	struct nand_bbt_descr *td = this->bbt_td;
	struct nand_bbt_descr *md = this->bbt_md;

	len = mtd->size >> (this->bbt_erase_shift + 2);
	/* Allocate memory (2bit per block) and clear the memory bad block table */
	this->bbt = kzalloc(len, GFP_KERNEL);
	if (!this->bbt) {
		printk(KERN_ERR "nand_scan_bbt: Out of memory\n");
		return -ENOMEM;
	}

	/* If no primary table decriptor is given, scan the device
	 * to build a memory based bad block table
	 */
	if (!td) {
		if ((res = nand_memory_bbt(mtd, bd))) {
			printk(KERN_ERR "nand_bbt: Can't scan flash and build the RAM-based BBT\n");
			kfree(this->bbt);
			this->bbt = NULL;
		}
		return res;
	}
	verify_bbt_descr(mtd, td);
	verify_bbt_descr(mtd, md);

	/* Allocate a temporary buffer for one eraseblock incl. oob */
	len = (1 << this->bbt_erase_shift);
	len += (len >> this->page_shift) * mtd->oobsize;
	buf = vmalloc(len);
	if (!buf) {
		printk(KERN_ERR "nand_bbt: Out of memory\n");
		kfree(this->bbt);
		this->bbt = NULL;
		return -ENOMEM;
	}

	/* Is the bbt at a given page ? */
	if (td->options & NAND_BBT_ABSPAGE) {
		res = read_abs_bbts(mtd, buf, td, md);
	} else {
		/* Search the bad block table using a pattern in oob */
		res = search_read_bbts(mtd, buf, td, md);
	}

	if (res)
		res = check_create(mtd, buf, bd);

	/* Prevent the bbt regions from erasing / writing */
	mark_bbt_region(mtd, td);
	if (md)
		mark_bbt_region(mtd, md);

	vfree(buf);
	return res;
}

/**
 * nand_update_bbt - [NAND Interface] update bad block table(s)
 * @mtd:	MTD device structure
 * @offs:	the offset of the newly marked block
 *
 * The function updates the bad block table(s)
*/
int nand_update_bbt(struct mtd_info *mtd, loff_t offs)
{
	struct nand_chip *this = mtd->priv;
	int len, res = 0, writeops = 0;
	int chip, chipsel;
	uint8_t *buf;
	struct nand_bbt_descr *td = this->bbt_td;
	struct nand_bbt_descr *md = this->bbt_md;

	if (!this->bbt || !td)
		return -EINVAL;

	/* Allocate a temporary buffer for one eraseblock incl. oob */
	len = (1 << this->bbt_erase_shift);
	len += (len >> this->page_shift) * mtd->oobsize;
	buf = kmalloc(len, GFP_KERNEL);
	if (!buf) {
		printk(KERN_ERR "nand_update_bbt: Out of memory\n");
		return -ENOMEM;
	}

	writeops = md != NULL ? 0x03 : 0x01;

	/* Do we have a bbt per chip ? */
	if (td->options & NAND_BBT_PERCHIP) {
		chip = (int)(offs >> this->chip_shift);
		chipsel = chip;
	} else {
		chip = 0;
		chipsel = -1;
	}

	td->version[chip]++;
	if (md)
		md->version[chip]++;

	/* Write the bad block table to the device ? */
	if ((writeops & 0x01) && (td->options & NAND_BBT_WRITE)) {
		res = write_bbt(mtd, buf, td, md, chipsel);
		if (res < 0)
			goto out;
	}
	/* Write the mirror bad block table to the device ? */
	if ((writeops & 0x02) && md && (md->options & NAND_BBT_WRITE)) {
		res = write_bbt(mtd, buf, md, td, chipsel);
	}

 out:
	kfree(buf);
	return res;
}

/* Define some generic bad / good block scan pattern which are used
 * while scanning a device for factory marked good / bad blocks. */
static uint8_t scan_ff_pattern[] = { 0xff, 0xff };

static struct nand_bbt_descr smallpage_flashbased = {
	.options = NAND_BBT_SCAN2NDPAGE,
	.offs = NAND_SMALL_BADBLOCK_POS,
	.len = 1,
	.pattern = scan_ff_pattern
};

static struct nand_bbt_descr largepage_flashbased = {
	.options = NAND_BBT_SCAN2NDPAGE,
	.offs = NAND_LARGE_BADBLOCK_POS,
	.len = 2,
	.pattern = scan_ff_pattern
};

static uint8_t scan_agand_pattern[] = { 0x1C, 0x71, 0xC7, 0x1C, 0x71, 0xC7 };

static struct nand_bbt_descr agand_flashbased = {
	.options = NAND_BBT_SCANEMPTY | NAND_BBT_SCANALLPAGES,
	.offs = 0x20,
	.len = 6,
	.pattern = scan_agand_pattern
};

/* Generic flash bbt decriptors
*/
static uint8_t bbt_pattern[] = {'B', 'b', 't', '0' };
static uint8_t mirror_pattern[] = {'1', 't', 'b', 'B' };

static struct nand_bbt_descr bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs =	8,
	.len = 4,
	.veroffs = 12,
	.maxblocks = 4,
	.pattern = bbt_pattern
};

static struct nand_bbt_descr bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs =	8,
	.len = 4,
	.veroffs = 12,
	.maxblocks = 4,
	.pattern = mirror_pattern
};

static struct nand_bbt_descr bbt_main_no_bbt_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP
		| NAND_BBT_NO_OOB,
	.len = 4,
	.veroffs = 4,
	.maxblocks = 4,
	.pattern = bbt_pattern
};

static struct nand_bbt_descr bbt_mirror_no_bbt_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP
		| NAND_BBT_NO_OOB,
	.len = 4,
	.veroffs = 4,
	.maxblocks = 4,
	.pattern = mirror_pattern
};

#define BBT_SCAN_OPTIONS (NAND_BBT_SCANLASTPAGE | NAND_BBT_SCAN2NDPAGE | \
		NAND_BBT_SCANBYTE1AND6)
/**
 * nand_create_default_bbt_descr - [Internal] Creates a BBT descriptor structure
 * @this:	NAND chip to create descriptor for
 *
 * This function allocates and initializes a nand_bbt_descr for BBM detection
 * based on the properties of "this". The new descriptor is stored in
 * this->badblock_pattern. Thus, this->badblock_pattern should be NULL when
 * passed to this function.
 *
 * TODO: Handle other flags, replace other static structs
 *        (e.g. handle NAND_BBT_FLASH for flash-based BBT,
 *             replace smallpage_flashbased)
 *
 */
static int nand_create_default_bbt_descr(struct nand_chip *this)
{
	struct nand_bbt_descr *bd;
	if (this->badblock_pattern) {
		printk(KERN_WARNING "BBT descr already allocated; not replacing.\n");
		return -EINVAL;
	}
	bd = kzalloc(sizeof(*bd), GFP_KERNEL);
	if (!bd) {
		printk(KERN_ERR "nand_create_default_bbt_descr: Out of memory\n");
		return -ENOMEM;
	}
	bd->options = this->options & BBT_SCAN_OPTIONS;
	bd->offs = this->badblockpos;
	bd->len = (this->options & NAND_BUSWIDTH_16) ? 2 : 1;
	bd->pattern = scan_ff_pattern;
	bd->options |= NAND_BBT_DYNAMICSTRUCT;
	this->badblock_pattern = bd;
	return 0;
}

/**
 * nand_default_bbt - [NAND Interface] Select a default bad block table for the device
 * @mtd:	MTD device structure
 *
 * This function selects the default bad block table
 * support for the device and calls the nand_scan_bbt function
 *
*/
int nand_default_bbt(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;

	/* Default for AG-AND. We must use a flash based
	 * bad block table as the devices have factory marked
	 * _good_ blocks. Erasing those blocks leads to loss
	 * of the good / bad information, so we _must_ store
	 * this information in a good / bad table during
	 * startup
	 */
	if (this->options & NAND_IS_AND) {
		/* Use the default pattern descriptors */
		if (!this->bbt_td) {
			this->bbt_td = &bbt_main_descr;
			this->bbt_md = &bbt_mirror_descr;
		}
		this->options |= NAND_USE_FLASH_BBT;
		return nand_scan_bbt(mtd, &agand_flashbased);
	}

	/* Is a flash based bad block table requested ? */
	if (this->options & NAND_USE_FLASH_BBT) {
		/* Use the default pattern descriptors */
		if (!this->bbt_td) {
			if (this->options & NAND_USE_FLASH_BBT_NO_OOB) {
				this->bbt_td = &bbt_main_no_bbt_descr;
				this->bbt_md = &bbt_mirror_no_bbt_descr;
			} else {
				this->bbt_td = &bbt_main_descr;
				this->bbt_md = &bbt_mirror_descr;
			}
		}
		if (!this->badblock_pattern) {
			this->badblock_pattern = (mtd->writesize > 512) ? &largepage_flashbased : &smallpage_flashbased;
		}
	} else {
		this->bbt_td = NULL;
		this->bbt_md = NULL;
		if (!this->badblock_pattern)
			nand_create_default_bbt_descr(this);
	}
	return nand_scan_bbt(mtd, this->badblock_pattern);
}

/**
 * nand_isbad_bbt - [NAND Interface] Check if a block is bad
 * @mtd:	MTD device structure
 * @offs:	offset in the device
 * @allowbbt:	allow access to bad block table region
 *
*/
int nand_isbad_bbt(struct mtd_info *mtd, loff_t offs, int allowbbt)
{
	struct nand_chip *this = mtd->priv;
	int block;
	uint8_t res;

	/* Get block number * 2 */
	block = (int)(offs >> (this->bbt_erase_shift - 1));
	res = (this->bbt[block >> 3] >> (block & 0x06)) & 0x03;

	DEBUG(MTD_DEBUG_LEVEL2, "nand_isbad_bbt(): bbt info for offs 0x%08x: (block %d) 0x%02x\n",
	      (unsigned int)offs, block >> 1, res);

	switch ((int)res) {
	case 0x00:
		return 0;
	case 0x01:
		return 1;
	case 0x02:
		return allowbbt ? 0 : 1;
	}
	return 1;
}

EXPORT_SYMBOL(nand_scan_bbt);
EXPORT_SYMBOL(nand_default_bbt);
