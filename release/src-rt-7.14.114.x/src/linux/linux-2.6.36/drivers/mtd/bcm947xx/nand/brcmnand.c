/*
 * Nortstar NAND controller driver
 * for Linux NAND library and MTD interface
 *
 *  Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 *  
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 *  SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 *  OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 *  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * This module interfaces the NAND controller and hardware ECC capabilities
 * tp the generic NAND chip support in the NAND library.
 *
 * Notes:
 *	This driver depends on generic NAND driver, but works at the
 *	page level for operations.
 *
 *	When a page is written, the ECC calculated also protects the OOB
 *	bytes not taken by ECC, and so the OOB must be combined with any
 *	OOB data that preceded the page-write operation in order for the
 *	ECC to be calculated correctly.
 *	Also, when the page is erased, but OOB data is not, HW ECC will
 *	indicate an error, because it checks OOB too, which calls for some
 *	help from the software in this driver.
 *
 * TBD:
 *	Block locking/unlocking support, OTP support
 *
 * $Id: $
 */

#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/bug.h>
#include <linux/err.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#ifdef	CONFIG_MTD_PARTITIONS
#include <linux/mtd/partitions.h>
#endif

#include <typedefs.h>
#include <osl.h>
#include <hndsoc.h>
#include <siutils.h>
#include <hndnand.h>
#include <nand_core.h>

/*
 * Because the bcm_nflash doesn't consider second chip case,
 * So in this brcmnand driver we just define NANDC_MAX_CHIPS to 1
 * And in the brcmnand_select_chip the argument chip_num == -1 means
 * de-select the device. Since the bcm_nflash doesn't do select_chip,
 * we must be careful to select other chip in the brcmnand driver.
 * 
*/
#define	NANDC_MAX_CHIPS		1

#define	DRV_NAME		"brcmnand"
#define	DRV_VERSION		"0.1"
#define	DRV_DESC		"Northstar brcmnand NAND Flash Controller driver"

/* Mutexing is version-dependent */
extern struct nand_hw_control *nand_hwcontrol_lock_init(void);
extern void *partitions_lock_init(void);

/*
 * Driver private control structure
 */
struct brcmnand_mtd {
	si_t			*sih;
	struct mtd_info		mtd;
	struct nand_chip	chip;
#ifdef	CONFIG_MTD_PARTITIONS
	struct mtd_partition	*parts;
#endif
	hndnand_t		*nfl;

	struct nand_ecclayout	ecclayout;
	int			cmd_ret;	/* saved error code */
	int			page_addr;	/* saved page address from SEQIN */
	unsigned char		oob_index;
	unsigned char		id_byte_index;
	unsigned char		last_cmd;
	unsigned char		ecc_level;
	unsigned char		sector_size_shift;
	unsigned char		sec_per_page_shift;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry	*proc;
#endif
};

/* Single device present */
static struct brcmnand_mtd brcmnand;

/*
 * NAND Controller hardware ECC data size
 *
 * The following table contains the number of bytes needed for
 * each of the ECC levels, per "sector", which is either 512 or 1024 bytes.
 * The actual layout is as follows:
 * The entire spare area is equally divided into as many sections as there
 * are sectors per page, and the ECC data is located at the end of each
 * of these sections.
 * For example, given a 2K per page and 64 bytes spare device, configured for
 * sector size 1k and ECC level of 4, the spare area will be divided into 2
 * sections 32 bytes each, and the last 14 bytes of 32 in each section will
 * be filled with ECC data.
 * Note: the name of the algorythm and the number of error bits it can correct
 * is of no consequence to this driver, therefore omitted.
 */
static const struct brcmnand_ecc_size_s {
	unsigned char sector_size_shift;
	unsigned char ecc_level;
	unsigned char ecc_bytes_per_sec;
	unsigned char reserved;
} brcmnand_ecc_sizes [] = {
	{9,	0,	0},
	{10,	0, 	0},
	{9,	1, 	2},
	{10,	1, 	4},
	{9,	2, 	4},
	{10,	2, 	7},
	{9,	3, 	6},
	{10,	3, 	11},
	{9,	4, 	7},
	{10,	4, 	14},
	{9,	5, 	9},
	{10,	5, 	18},
	{9,	6, 	11},
	{10,	6, 	21},
	{9,	7, 	13},
	{10,	7, 	25},
	{9,	8, 	14},
	{10,	8, 	28},

	{9,	9, 	16},
	{9,	10, 	18},
	{9,	11, 	20},
	{9,	12, 	21},

	{10,	9, 	32},
	{10,	10, 	35},
	{10,	11, 	39},
	{10,	12, 	42},
	{10,	40, 	70},
};

static void
brcmnand_get_device(si_t *sih, struct mtd_info *mtd)
{
	/* 53573/47189 series */
	if (sih->ccrev == 54 && mtd->mlock)
		spin_lock(mtd->mlock);
}

static void
brcmnand_release_device(si_t *sih, struct mtd_info *mtd)
{
	/* 53573/47189 series */
	if (sih->ccrev == 54 && mtd->mlock)
		spin_unlock(mtd->mlock);
}

/*
 * INTERNAL - Populate the various fields that depend on how
 * the hardware ECC data is located in the spare area
 *
 * For this controiller, it is easier to fill-in these
 * structures at run time.
 *
 * The bad-block marker is assumed to occupy one byte
 * at chip->badblockpos, which must be in the first
 * sector of the spare area, namely it is either 
 * at offset 0 or 5.
 * Some chips use both for manufacturer's bad block
 * markers, but we ingore that issue here, and assume only
 * one byte is used as bad-block marker always.
 */
static int
brcmnand_hw_ecc_layout(struct brcmnand_mtd *brcmnand)
{
	struct nand_ecclayout *layout;
	unsigned i, j, k;
	unsigned ecc_per_sec, oob_per_sec;
	unsigned bbm_pos = brcmnand->chip.badblockpos;

	DEBUG(MTD_DEBUG_LEVEL1, "%s: ecc_level %d\n",
		__func__, brcmnand->ecc_level);

	/* Caclculate spare area per sector size */
	oob_per_sec = brcmnand->mtd.oobsize >> brcmnand->sec_per_page_shift;

	/* Try to calculate the amount of ECC bytes per sector with a formula */
	if (brcmnand->sector_size_shift == 9)
		ecc_per_sec = ((brcmnand->ecc_level * 14) + 7) >> 3;
	else if (brcmnand->sector_size_shift == 10)
		ecc_per_sec = ((brcmnand->ecc_level * 14) + 3) >> 2;
	else
		ecc_per_sec = oob_per_sec + 1;	/* cause an error if not in table */

	DEBUG(MTD_DEBUG_LEVEL1, "%s: calc eccbytes %d\n", __func__, ecc_per_sec);

	/* Now find out the answer according to the table */
	for (i = 0; i < ARRAY_SIZE(brcmnand_ecc_sizes); i++) {
		if (brcmnand_ecc_sizes[i].ecc_level == brcmnand->ecc_level &&
		    brcmnand_ecc_sizes[i].sector_size_shift == brcmnand->sector_size_shift) {
			DEBUG(MTD_DEBUG_LEVEL1, "%s: table eccbytes %d\n", __func__,
				brcmnand_ecc_sizes[i].ecc_bytes_per_sec);
			break;
		}
	}

	/* Table match overrides formula */
	if (brcmnand_ecc_sizes[i].ecc_level == brcmnand->ecc_level &&
	    brcmnand_ecc_sizes[i].sector_size_shift == brcmnand->sector_size_shift)
		ecc_per_sec = brcmnand_ecc_sizes[i].ecc_bytes_per_sec;

	/* Return an error if calculated ECC leaves no room for OOB */
	if ((brcmnand->sec_per_page_shift != 0 && ecc_per_sec >= oob_per_sec) ||
	    (brcmnand->sec_per_page_shift == 0 && ecc_per_sec >= (oob_per_sec-1))) {
		printk(KERN_WARNING "%s: ERROR: ECC level %d too high, "
			"leaves no room for OOB data\n", __func__, brcmnand->ecc_level);
		return -EINVAL;
	}

	/* Fill in the needed fields */
	brcmnand->chip.ecc.size = brcmnand->mtd.writesize >> brcmnand->sec_per_page_shift;
	brcmnand->chip.ecc.bytes = ecc_per_sec;
	brcmnand->chip.ecc.steps = 1 << brcmnand->sec_per_page_shift;
	brcmnand->chip.ecc.total = ecc_per_sec << brcmnand->sec_per_page_shift;

	/* Build an ecc layout data structure */
	layout = &brcmnand->ecclayout;
	memset(layout, 0, sizeof(*layout));

	/* Total number of bytes used by HW ECC */
	layout->eccbytes = ecc_per_sec << brcmnand->sec_per_page_shift;

	/* Location for each of the HW ECC bytes */
	for (i = j = 0, k = 1;
		i < ARRAY_SIZE(layout->eccpos) && i < layout->eccbytes;
		i++, j++) {
		/* switch sector # */
		if (j == ecc_per_sec) {
			j = 0;
			k ++;
		}
		/* save position of each HW-generated ECC byte */
		layout->eccpos[i] = (oob_per_sec * k) - ecc_per_sec + j;

		/* Check that HW ECC does not overlap bad-block marker */
		if (bbm_pos == layout->eccpos[i]) {
			printk(KERN_WARNING "%s: ERROR: ECC level %d too high, "
			"HW ECC collides with bad-block marker position\n",
			__func__, brcmnand->ecc_level);

			return -EINVAL;
		}
	}

	/* Location of all user-available OOB byte-ranges */
	for (i = 0; i < ARRAY_SIZE(layout->oobfree); i++) {
		if (i >= (1 << brcmnand->sec_per_page_shift))
			break;
		layout->oobfree[i].offset = oob_per_sec * i;
		layout->oobfree[i].length = oob_per_sec - ecc_per_sec;


		/* Bad-block marker must be in the first sector spare area */
		BUG_ON(bbm_pos >= (layout->oobfree[i].offset+layout->oobfree[i].length));
		if (i != 0)
			continue;

		/* Remove bad-block marker from available byte range */
		if (bbm_pos == layout->oobfree[i].offset) {
			layout->oobfree[i].offset += 1;
			layout->oobfree[i].length -= 1;
		}
		else if (bbm_pos ==
		  (layout->oobfree[i].offset+layout->oobfree[i].length-1)) {
			layout->oobfree[i].length -= 1;
		}
		else {
			layout->oobfree[i+1].offset = bbm_pos + 1;
			layout->oobfree[i+1].length =
				layout->oobfree[i].length - bbm_pos - 1;
			layout->oobfree[i].length = bbm_pos;
			i ++;
		}
	}

	layout->oobavail = ((oob_per_sec - ecc_per_sec)	<< brcmnand->sec_per_page_shift) - 1;

	brcmnand->mtd.oobavail = layout->oobavail;
	brcmnand->chip.ecclayout = layout;
	brcmnand->chip.ecc.layout = layout;

	/* Output layout for debugging */
	printk("Spare area=%d eccbytes %d, ecc bytes located at:\n",
		brcmnand->mtd.oobsize, layout->eccbytes);
	for (i = j = 0; i < ARRAY_SIZE(layout->eccpos) && i < layout->eccbytes; i++) {
		printk(" %d", layout->eccpos[i]);
	}
	printk("\nAvailable %d bytes at (off,len):\n", layout->oobavail);
	for (i = 0; i < ARRAY_SIZE(layout->oobfree); i++) {
		printk("(%d,%d) ",
			layout->oobfree[i].offset,
			layout->oobfree[i].length);
	}
	printk("\n");

	return 0;
}

/*
 * INTERNAL - print NAND chip options
 * 
 * Useful for debugging
 */
static void
brcmnand_options_print(unsigned opts)
{
	unsigned bit;
	const char * n;

	printk("Options: ");
	for (bit = 0; bit < 32; bit ++) {
		if (0 == (opts & (1<<bit)))
			continue;
		switch (1 << bit) {
			default:
				printk("OPT_%x", 1 << bit);
				n = NULL;
				break;
			case NAND_NO_AUTOINCR:
				n = "NO_AUTOINCR"; break;
			case NAND_BUSWIDTH_16:
				n = "BUSWIDTH_16"; break;
			case NAND_NO_PADDING:
				n = "NO_PADDING"; break;
			case NAND_CACHEPRG:
				n = "CACHEPRG"; break;
			case NAND_COPYBACK:
				n = "COPYBACK"; break;
			case NAND_IS_AND:
				n = "IS_AND"; break;
			case NAND_4PAGE_ARRAY:
				n = "4PAGE_ARRAY"; break;
			case BBT_AUTO_REFRESH:
				n = "AUTO_REFRESH"; break;
			case NAND_NO_READRDY:
				n = "NO_READRDY"; break;
			case NAND_NO_SUBPAGE_WRITE:
				n = "NO_SUBPAGE_WRITE"; break;
			case NAND_BROKEN_XD:
				n = "BROKEN_XD"; break;
			case NAND_ROM:
				n = "ROM"; break;
			case NAND_USE_FLASH_BBT:
				n = "USE_FLASH_BBT"; break;
			case NAND_SKIP_BBTSCAN:
				n = "SKIP_BBTSCAN"; break;
			case NAND_OWN_BUFFERS:
				n = "OWN_BUFFERS"; break;
			case NAND_SCAN_SILENT_NODEV:
				n = "SCAN_SILENT_NODEV"; break;
			case NAND_CONTROLLER_ALLOC:
				n = "SCAN_SILENT_NODEV"; break;
			case NAND_BBT_SCAN2NDPAGE:
				n = "BBT_SCAN2NDPAGE"; break;
			case NAND_BBT_SCANBYTE1AND6:
				n = "BBT_SCANBYTE1AND6"; break;
			case NAND_BBT_SCANLASTPAGE:
				n = "BBT_SCANLASTPAGE"; break;
		}
		printk("%s,", n);
	}
	printk("\n");
}

/*
 * NAND Interface - dev_ready
 *
 * Return 1 if device is ready, 0 otherwise
 */
static int
brcmnand_dev_ready(struct mtd_info *mtd)
{
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	struct brcmnand_mtd *brcmnand = (struct brcmnand_mtd *)chip->priv;
	int ret = 0;

	brcmnand_get_device(brcmnand->sih, mtd);
	ret = hndnand_dev_ready(brcmnand->nfl);
	brcmnand_release_device(brcmnand->sih, mtd);

	return ret;
}

/*
 * NAND Interface - waitfunc
 */
static int
brcmnand_waitfunc(struct mtd_info *mtd, struct nand_chip *chip)
{
	int ret, status;
	struct brcmnand_mtd *brcmnand = chip->priv;

	/* deliver deferred error code if any */
	if ((ret = brcmnand->cmd_ret) < 0)
		brcmnand->cmd_ret = 0;
	else {
		brcmnand_get_device(brcmnand->sih, mtd);
		if ((ret = hndnand_waitfunc(brcmnand->nfl, &status)) == 0)
			ret = status;
		brcmnand_release_device(brcmnand->sih, mtd);
	}

	/* Timeout */
	if (ret < 0) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: timeout\n", __func__);
		return NAND_STATUS_FAIL;
	}

	DEBUG(MTD_DEBUG_LEVEL3, "%s: status=%#x\n", __func__, ret);

	return ret;
}

/*
 * NAND Interface - read_oob
 */
static int
brcmnand_read_oob(struct mtd_info *mtd, struct nand_chip *chip, int page, int sndcmd)
{
	struct brcmnand_mtd *brcmnand = chip->priv;
	uint64 nand_addr;
	int ret = 0;

	nand_addr = ((uint64)page << chip->page_shift);
	brcmnand_get_device(brcmnand->sih, mtd);
	ret = hndnand_read_oob(brcmnand->nfl, nand_addr, chip->oob_poi);
	brcmnand_release_device(brcmnand->sih, mtd);

	return ret;
}

/*
 * NAND Interface - write_oob
 */
static int
brcmnand_write_oob(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	struct brcmnand_mtd *brcmnand = chip->priv;
	uint64 nand_addr;
	int status = 0;

	nand_addr = ((uint64)page << chip->page_shift);
	brcmnand_get_device(brcmnand->sih, mtd);
	hndnand_write_oob(brcmnand->nfl, nand_addr, chip->oob_poi);
	brcmnand_release_device(brcmnand->sih, mtd);

	status = brcmnand_waitfunc(mtd, chip);

	return status & NAND_STATUS_FAIL ? -EIO : 0;
}

/*
 * INTERNAL - read a page, with or without ECC checking
 */
static int
_brcmnand_read_page_do(struct mtd_info *mtd, struct nand_chip *chip,
	uint8_t *buf, int page, bool ecc)
{
	struct brcmnand_mtd *brcmnand = chip->priv;
	uint64 nand_addr;
	uint32 herr = 0, serr = 0;
	int ret;

	nand_addr = ((uint64)page << chip->page_shift);
	brcmnand_get_device(brcmnand->sih, mtd);
	ret = hndnand_read_page(brcmnand->nfl, nand_addr, buf, chip->oob_poi, ecc, &herr, &serr);
	brcmnand_release_device(brcmnand->sih, mtd);

	if (ecc && ret == 0) {
		/* Report hard ECC errors */
		mtd->ecc_stats.failed += herr;

		/* Get ECC soft error stats */
		mtd->ecc_stats.corrected += serr;

		DEBUG(MTD_DEBUG_LEVEL3, "%s: page=%#x err hard %d soft %d\n", __func__, page,
			mtd->ecc_stats.failed, mtd->ecc_stats.corrected);
	}

	return ret;
}

/*
 * NAND Interface - read_page_ecc
 */
static int
brcmnand_read_page_ecc(struct mtd_info *mtd, struct nand_chip *chip,
                uint8_t *buf, int page)
{
	return _brcmnand_read_page_do(mtd, chip, buf, page, TRUE);
}

/*
 * NAND Interface - read_page_raw
 */
static int
brcmnand_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
                uint8_t *buf, int page)
{
	return _brcmnand_read_page_do(mtd, chip, buf, page, TRUE);
}

/*
 * INTERNAL - do page write, with or without ECC generation enabled
 */
static void
_brcmnand_write_page_do(struct mtd_info *mtd, struct nand_chip *chip, const uint8_t *buf, bool ecc)
{
	struct brcmnand_mtd *brcmnand = chip->priv;
	uint8_t tmp_poi[NAND_MAX_OOBSIZE];
	uint64 nand_addr;
	int i;

	BUG_ON(mtd->oobsize > sizeof(tmp_poi));

	/* Retreive pre-existing OOB values */
	memcpy(tmp_poi, chip->oob_poi, mtd->oobsize);
	brcmnand->cmd_ret = brcmnand_read_oob(mtd, chip, brcmnand->page_addr, 1);
	if (brcmnand->cmd_ret < 0)
		return;

	/* Apply new OOB data bytes just like they would end up on the chip */
	for (i = 0; i < mtd->oobsize; i ++)
		chip->oob_poi[i] &= tmp_poi[i];

	nand_addr = ((uint64)brcmnand->page_addr << chip->page_shift);
	brcmnand_get_device(brcmnand->sih, mtd);
	brcmnand->cmd_ret = hndnand_write_page(brcmnand->nfl, nand_addr, buf,
		chip->oob_poi, ecc);
	brcmnand_release_device(brcmnand->sih, mtd);

	return;
}

/*
 * NAND Interface = write_page_ecc
 */
static void
brcmnand_write_page_ecc(struct mtd_info *mtd, struct nand_chip *chip,
                const uint8_t *buf)
{
	_brcmnand_write_page_do(mtd, chip, buf, TRUE);
}

/*
 * NAND Interface = write_page_raw
 */
static void
brcmnand_write_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
                const uint8_t *buf)
{
	printk(KERN_INFO "%s: Enter!\n", __FUNCTION__);

	_brcmnand_write_page_do(mtd, chip, buf, FALSE);
}

/*
 * MTD Interface - read_byte
 *
 * This function emulates simple controllers behavior
 * for just a few relevant commands
 */
static uint8_t
brcmnand_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct brcmnand_mtd *brcmnand = chip->priv;
	uint32 reg;
	uint8_t b = ~0;

	switch (brcmnand->last_cmd) {
		case NAND_CMD_READID:
			if (brcmnand->id_byte_index < 8) {
				brcmnand_get_device(brcmnand->sih, mtd);
				reg = hndnand_cmd_read_byte(brcmnand->nfl, CMDFUNC_READID,
					(brcmnand->id_byte_index & 0x4));
				brcmnand_release_device(brcmnand->sih, mtd);
				reg >>= 24 - ((brcmnand->id_byte_index & 3) << 3);
				b = (uint8_t) (reg & ((1 << 8) - 1));

				brcmnand->id_byte_index ++;
			}
			break;
		case NAND_CMD_READOOB:
			if (brcmnand->oob_index < mtd->oobsize) {
				b = chip->oob_poi[brcmnand->oob_index++];
			}
			break;
		case NAND_CMD_STATUS:
			brcmnand_get_device(brcmnand->sih, mtd);
			b = (uint8_t)hndnand_cmd_read_byte(brcmnand->nfl, CMDFUNC_STATUS, 0);
			brcmnand_release_device(brcmnand->sih, mtd);
			break;
		default:
			BUG_ON(1);
	}

	DEBUG(MTD_DEBUG_LEVEL3, "%s: %#x\n", __func__, b);

	return b;
}

/*
 * MTD Interface - read_word
 *
 * Can not be tested without x16 chip, but the SoC does not support x16 i/f.
 */
static u16
brcmnand_read_word(struct mtd_info *mtd)
{
	u16 w = ~0;

	w = brcmnand_read_byte(mtd);
	barrier();
	w |= brcmnand_read_byte(mtd) << 8;

	DEBUG(MTD_DEBUG_LEVEL0, "%s: %#x\n", __func__, w);

	return w;
}

/*
 * MTD Interface - select a chip from an array
 */
static void
brcmnand_select_chip(struct mtd_info *mtd, int chip_num)
{
	struct nand_chip *chip = mtd->priv;
	struct brcmnand_mtd *brcmnand = chip->priv;

	/* chip_num == -1 means de-select the device */
	brcmnand_get_device(brcmnand->sih, mtd);
	hndnand_select_chip(brcmnand->nfl, 0);
	brcmnand_release_device(brcmnand->sih, mtd);
}

/*
 * NAND Interface - emulate low-level NAND commands
 *
 * Only a few low-level commands are really needed by generic NAND,
 * and they do not call for CMD_LL operations the controller can support.
 */
static void
brcmnand_cmdfunc(struct mtd_info *mtd, unsigned command, int column, int page_addr)
{
	struct nand_chip *chip = mtd->priv;
	struct brcmnand_mtd *brcmnand = chip->priv;
	uint64 nand_addr;

	DEBUG(MTD_DEBUG_LEVEL3, "%s: cmd=%#x col=%#x pg=%#x\n", __func__,
		command, column, page_addr);

	brcmnand->last_cmd = command;

	switch (command) {
		case NAND_CMD_ERASE1:
			column = 0;
			BUG_ON(column >= mtd->writesize);
			nand_addr = (uint64) column | ((uint64)page_addr << chip->page_shift);
			brcmnand_get_device(brcmnand->sih, mtd);
			hndnand_cmdfunc(brcmnand->nfl, nand_addr, CMDFUNC_ERASE1);
			brcmnand_release_device(brcmnand->sih, mtd);
			break;

		case NAND_CMD_ERASE2:
			brcmnand_get_device(brcmnand->sih, mtd);
			brcmnand->cmd_ret = hndnand_cmdfunc(brcmnand->nfl, 0, CMDFUNC_ERASE2);
			brcmnand_release_device(brcmnand->sih, mtd);
			break;

		case NAND_CMD_SEQIN:
			BUG_ON(column >= mtd->writesize);
			brcmnand->page_addr = page_addr;
			nand_addr = (uint64) column | ((uint64)page_addr << chip->page_shift);
			brcmnand_get_device(brcmnand->sih, mtd);
			hndnand_cmdfunc(brcmnand->nfl, nand_addr, CMDFUNC_SEQIN);
			brcmnand_release_device(brcmnand->sih, mtd);
			break;

		case NAND_CMD_READ0:
		case NAND_CMD_READ1:
			BUG_ON(column >= mtd->writesize);
			nand_addr = (uint64) column | ((uint64)page_addr << chip->page_shift);
			brcmnand_get_device(brcmnand->sih, mtd);
			brcmnand->cmd_ret = hndnand_cmdfunc(brcmnand->nfl, nand_addr, CMDFUNC_READ);
			brcmnand_release_device(brcmnand->sih, mtd);
			break;

		case NAND_CMD_RESET:
			brcmnand_get_device(brcmnand->sih, mtd);
			brcmnand->cmd_ret = hndnand_cmdfunc(brcmnand->nfl, 0, CMDFUNC_RESET);
			brcmnand_release_device(brcmnand->sih, mtd);
			break;

		case NAND_CMD_READID:
			brcmnand_get_device(brcmnand->sih, mtd);
			brcmnand->cmd_ret = hndnand_cmdfunc(brcmnand->nfl, 0, CMDFUNC_READID);
			brcmnand_release_device(brcmnand->sih, mtd);
			brcmnand->id_byte_index = 0;
			break;

		case NAND_CMD_STATUS:
			brcmnand_get_device(brcmnand->sih, mtd);
			brcmnand->cmd_ret = hndnand_cmdfunc(brcmnand->nfl, 0, CMDFUNC_STATUS);
			brcmnand_release_device(brcmnand->sih, mtd);
			break;

		case NAND_CMD_PAGEPROG:
			/* Cmd already set from write_page */
			break;

		case NAND_CMD_READOOB:
			/* Emulate simple interface */
			brcmnand_read_oob(mtd, chip, page_addr, 1);
			brcmnand->oob_index = 0;
			break;

		default:
			BUG_ON(1);
	}
}

static int
brcmnand_scan(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct brcmnand_mtd *brcmnand = chip->priv;
	hndnand_t *nfl = brcmnand->nfl;
	int ret;

	/*
	 * The spin_lock of chip->controller->lock is enough here,
	 * because we don't start any transation yet.
	 */
	spin_lock(&chip->controller->lock);
	ret = nand_scan_ident(mtd, NANDC_MAX_CHIPS, NULL);
	spin_unlock(&chip->controller->lock);
	if (ret)
		return ret;

	DEBUG(MTD_DEBUG_LEVEL0, "%s: scan_ident ret=%d num_chips=%d\n", __func__,
		ret, chip->numchips);

	/* Get configuration from first chip */
	mtd->writesize_shift = chip->page_shift;
	brcmnand->ecc_level = nfl->ecclevel;
	brcmnand->sector_size_shift = ffs(nfl->sectorsize) - 1;
	brcmnand->sec_per_page_shift = mtd->writesize_shift - brcmnand->sector_size_shift;

	/* Generate ecc layout,  will return -EINVAL if OOB space exhausted */
	if ((ret = brcmnand_hw_ecc_layout(brcmnand)) != 0)
		return ret;

	DEBUG(MTD_DEBUG_LEVEL0, "%s: layout.oobavail=%d\n", __func__,
		chip->ecc.layout->oobavail);

	ret = nand_scan_tail(mtd);

	DEBUG(MTD_DEBUG_LEVEL0, "%s: scan_tail ret=%d\n", __func__, ret);

	if (chip->badblockbits == 0)
		chip->badblockbits = 8;
	BUG_ON((1<<chip->page_shift) != mtd->writesize);

	/* Spit out some key chip parameters as detected by nand_base */
	DEBUG(MTD_DEBUG_LEVEL0, "%s: erasesize=%d writesize=%d oobsize=%d "
		" page_shift=%d badblockpos=%d badblockbits=%d\n", __func__,
		mtd->erasesize, mtd->writesize, mtd->oobsize,
		chip->page_shift, chip->badblockpos, chip->badblockbits);

	brcmnand_options_print(chip->options);

	return ret;
}

/*
 * Dummy function to make sure generic NAND does not call anything unexpected.
 */
static int
brcmnand_dummy_func(struct mtd_info * mtd)
{
	BUG_ON(1);
}

#ifdef CONFIG_MTD_PARTITIONS
struct mtd_partition brcmnand_parts[] = {
	{
		.name = "brcmnand",
		.size = 0,
		.offset = 0
	},
	{
		.name = 0,
		.size = 0,
		.offset = 0
	}
};

struct mtd_partition *
init_brcmnand_mtd_partitions(struct mtd_info *mtd, uint64_t size)
{
	int knldev;
	uint64_t offset = 0;
	struct nand_chip *chip = mtd->priv;
	struct brcmnand_mtd *brcmnand = chip->priv;

	knldev = soc_knl_dev((void *)brcmnand->sih);
	if (knldev == SOC_KNLDEV_NANDFLASH)
		offset = nfl_boot_os_size(brcmnand->nfl);

#ifndef CONFIG_YAFFS_FS
	/* Since JFFS2 still uses uint32 for size, hence we have to force the size < 4GB */
	if (size >= ((uint64_t)4 << 30))
		size = ((uint64_t)4 << 30) - mtd->erasesize;
#endif /* CONFIG_YAFFS_FS */

	ASSERT(size > offset);

	brcmnand_parts[0].offset = offset;
	brcmnand_parts[0].size = size - offset;

	return brcmnand_parts;
}

#ifdef CONFIG_PROC_FS
/* Read "brcmnand" partition first block available OOB */
int
brcmnand_read_availoob(char *buffer, char **start, off_t offset, int length, int *eof, void *data)
{
	int len;
	struct mtd_oob_ops ops;
	struct mtd_info *mtd = &brcmnand.mtd;

	if (offset > 0) {
		*eof = 1;
		return 0;
	}

	/* Give the processed buffer back to userland */
	if (!length) {
		printk(KERN_ERR "%s: Not enough return buf space\n", __FUNCTION__);
		return 0;
	}

	if (length > mtd->ecclayout->oobavail)
		length = mtd->ecclayout->oobavail;

	if (!mtd->read_oob)
		return 0;

	ops.ooblen = length;
	ops.ooboffs = 0;
	ops.datbuf = NULL;
	ops.oobbuf = buffer;
	ops.mode = MTD_OOB_AUTO;

	if (ops.ooboffs && ops.ooblen > (mtd->oobsize - ops.ooboffs))
		return 0;

	ASSERT(!strcmp(brcmnand_parts[0].name, "brcmnand"));

	mtd->read_oob(mtd, brcmnand_parts[0].offset, &ops);

	return ops.oobretlen;
}
#endif /* CONFIG_PROC_FS */
#endif /* CONFIG_MTD_PARTITIONS */

static int __init
brcmnand_mtd_init(void)
{
	int ret = 0;
	unsigned chip_num;
	hndnand_t *nfl;
	struct nand_chip *chip;
	struct mtd_info *mtd;
#ifdef CONFIG_MTD_PARTITIONS
	struct mtd_partition *parts;
	int i;
#endif

	printk(KERN_INFO "%s, Version %s (c) Broadcom Inc. 2012\n",
		DRV_DESC, DRV_VERSION);

	memset(&brcmnand, 0, sizeof(struct brcmnand_mtd));

	/* attach to the backplane */
	if (!(brcmnand.sih = si_kattach(SI_OSH))) {
		printk(KERN_ERR "brcmnand: error attaching to backplane\n");
		ret = -EIO;
		goto fail;
	}

	/* Initialize serial flash access */
	if (!(nfl = hndnand_init(brcmnand.sih))) {
		printk(KERN_ERR "brcmnand: found no supported devices\n");
		ret = -ENODEV;
		goto fail;
	}

	brcmnand.nfl = nfl;

	/* Software variables init */
	mtd = &brcmnand.mtd;
	chip = &brcmnand.chip;

	mtd->priv = chip;
	mtd->owner = THIS_MODULE;
	mtd->name = DRV_NAME;
	mtd->mlock = partitions_lock_init();
	if (!mtd->mlock) {
		ret = -ENOMEM;
		goto fail;
	}

	chip->priv = &brcmnand;
	chip->controller = nand_hwcontrol_lock_init();
	if (!chip->controller) {
		ret = -ENOMEM;
		goto fail;
	}

	chip->chip_delay = 5;	/* not used */
	chip->IO_ADDR_R = chip->IO_ADDR_W = (void *)~0L;

	/* CS0 device width */
	if (nfl->width)
		chip->options |= NAND_BUSWIDTH_16;

	chip->options |= NAND_NO_SUBPAGE_WRITE; /* Subpages unsupported */

	chip->ecc.mode = NAND_ECC_HW;

	chip->dev_ready = brcmnand_dev_ready;
	chip->read_byte = brcmnand_read_byte;
	chip->read_word = brcmnand_read_word;
	chip->ecc.read_page_raw = brcmnand_read_page_raw;
	chip->ecc.write_page_raw = brcmnand_write_page_raw;
	chip->ecc.read_page = brcmnand_read_page_ecc;
	chip->ecc.write_page = brcmnand_write_page_ecc;
	chip->ecc.read_oob = brcmnand_read_oob;
	chip->ecc.write_oob = brcmnand_write_oob;
	chip->select_chip = brcmnand_select_chip;
	chip->cmdfunc = brcmnand_cmdfunc;
	chip->waitfunc = brcmnand_waitfunc;
	chip->read_buf = (void *)brcmnand_dummy_func;
	chip->write_buf = (void *)brcmnand_dummy_func;
	chip->verify_buf = (void *)brcmnand_dummy_func;

	if (brcmnand_scan(mtd)) {
		ret = -ENXIO;
		goto fail;
	}

#ifdef CONFIG_MTD_PARTITIONS
	parts = init_brcmnand_mtd_partitions(&brcmnand.mtd, brcmnand.mtd.size);
	if (!parts)
		goto fail;

	for (i = 0; parts[i].name; i++);

	ret = add_mtd_partitions(&brcmnand.mtd, parts, i);
	if (ret < 0) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: failed to add mtd partitions\n", __func__);
		goto fail;
	}

	brcmnand.parts = parts;

#ifdef CONFIG_PROC_FS
	if ((brcmnand.proc = create_proc_entry("brcmnand", 0, NULL)))
		brcmnand.proc->read_proc = brcmnand_read_availoob;
	else {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: failed to create brcmnand proc file\n", __func__);
		goto fail;
	}
#endif /* CONFIG_PROC_FS */
#endif /* CONFIG_MTD_PARTITIONS */

	return 0;

fail:
	return ret;
}

static void __exit
brcmnand_mtd_exit(void)
{
#ifdef CONFIG_MTD_PARTITIONS
	del_mtd_partitions(&brcmnand.mtd);
#ifdef CONFIG_PROC_FS
	if (brcmnand.proc)
		remove_proc_entry("brcmnand", NULL);
#endif
#else
	del_mtd_device(&brcmnand.mtd);
#endif
}

late_initcall(brcmnand_mtd_init);
module_exit(brcmnand_mtd_exit);
