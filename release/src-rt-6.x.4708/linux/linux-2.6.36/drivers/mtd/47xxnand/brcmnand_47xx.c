/*
 * Broadcom NAND flash controller interface
 *
 * Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
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

#include <linux/version.h>

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
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
#define	NFLASH_SUPPORT
#include <sbchipc.h>
#include <nflash.h>

#include "brcmnand_priv.h"

spinlock_t *partitions_lock_init(void);
#define NFLASH_LOCK(lock)       if (lock) spin_lock(lock)
#define NFLASH_UNLOCK(lock)     if (lock) spin_unlock(lock)

#ifdef CONFIG_MTD_PARTITIONS
#include <linux/mtd/partitions.h>

extern struct mtd_partition * init_brcmnand_mtd_partitions(struct mtd_info *mtd, size_t size);
#endif

static int nflash_lock = 0;

#ifdef	__mips__
#define PLATFORM_IOFLUSH_WAR()  __sync()
#else
#define	PLATFORM_IOFLUSH_WAR	mb	/* Should work for MIPS too */
#endif

#define BRCMNAND_POLL_TIMEOUT	3000

#define BRCMNAND_CORRECTABLE_ECC_ERROR      (1)
#define BRCMNAND_SUCCESS                    (0)
#define BRCMNAND_UNCORRECTABLE_ECC_ERROR    (-1)
#define BRCMNAND_FLASH_STATUS_ERROR         (-2)
#define BRCMNAND_TIMED_OUT                  (-3)

#define BRCMNAND_OOBBUF(pbuf) (&((pbuf)->databuf[NAND_MAX_PAGESIZE]))

/* Fill-in internal bit fields if missing */
#ifndef	NAND_ALE_COL
#define	NAND_ALE_COL	0x0100
#define	NAND_ALE_ROW	0x0200
#endif

/*
 * Number of required ECC bytes per 512B slice
 */
static const unsigned int brcmnand_eccbytes[16] = {
	[BRCMNAND_ECC_DISABLE]  = 0,
	[BRCMNAND_ECC_BCH_1]    = 2,
	[BRCMNAND_ECC_BCH_2]    = 4,
	[BRCMNAND_ECC_BCH_3]    = 5,
	[BRCMNAND_ECC_BCH_4]    = 7,
	[BRCMNAND_ECC_BCH_5]    = 9,
	[BRCMNAND_ECC_BCH_6]    = 10,
	[BRCMNAND_ECC_BCH_7]    = 12,
	[BRCMNAND_ECC_BCH_8]    = 13,
	[BRCMNAND_ECC_BCH_9]    = 15,
	[BRCMNAND_ECC_BCH_10]   = 17,
	[BRCMNAND_ECC_BCH_11]   = 18,
	[BRCMNAND_ECC_BCH_12]   = 20,
	[BRCMNAND_ECC_RESVD_1]  = 0,
	[BRCMNAND_ECC_RESVD_2]  = 0,
	[BRCMNAND_ECC_HAMMING]  = 3,
};

static const unsigned char ffchars[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 16 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 32 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 48 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 64 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 80 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 96 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 112 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 128 */
};

static struct nand_ecclayout brcmnand_oob_128 = {
	.eccbytes = 24,
	.eccpos =
	{
		6, 7, 8,
		22, 23, 24,
		38, 39, 40,
		54, 55, 56,
		70, 71, 72,
		86, 87, 88,
		102, 103, 104,
		118, 119, 120
	},
	.oobfree =
	{
		/* 0-1 used for BBT and/or manufacturer bad block marker,
		 * first slice loses 2 bytes for BBT
		 */
		{.offset = 2, .length = 4},
		{.offset = 9, .length = 13},
		/* First slice {9,7} 2nd slice {16,6}are combined */ 
		/* ST uses 6th byte (offset=5) as Bad Block Indicator, 
		 * in addition to the 1st byte, and will be adjusted at run time
		 */
		{.offset = 25, .length = 13}, /* 2nd slice */
		{.offset = 41, .length = 13}, /* 4th slice */
		{.offset = 57, .length = 13}, /* 5th slice */
		{.offset = 73, .length = 13}, /* 6th slice */
		{.offset = 89, .length = 13}, /* 7th slice */
		{.offset = 105, .length = 13}, /* 8th slice */
#if	MTD_MAX_OOBFREE_ENTRIES > 8
		{.offset = 121, .length = 7}, /* 9th slice */
		{.offset = 0, .length = 0} /* End marker */
#endif
	}
};

static struct nand_ecclayout brcmnand_oob_64 = {
	.eccbytes = 12,
	.eccpos =
	{
		6, 7, 8,
		22, 23, 24,
		38, 39, 40,
		54, 55, 56
	},
	.oobfree =
	{
		/* 0-1 used for BBT and/or manufacturer bad block marker,
		 * first slice loses 2 bytes for BBT
		 */
		{.offset = 2, .length = 4},
		{.offset = 9, .length = 13},
		/* First slice {9,7} 2nd slice {16,6}are combined */ 
		/* ST uses 6th byte (offset=5) as Bad Block Indicator, 
		 * in addition to the 1st byte, and will be adjusted at run time
		 */
		{.offset = 25, .length = 13}, /* 2nd slice */
		{.offset = 41, .length = 13}, /* 3rd slice */
		{.offset = 57, .length = 7}, /* 4th slice */
		{.offset = 0, .length = 0} /* End marker */
	}
};

/**
 * brcmnand_oob oob info for 512 page
 */
static struct nand_ecclayout brcmnand_oob_16 = {
	.eccbytes = 3,
	.eccpos = {6, 7, 8},
	.oobfree = {
		{.offset = 0, .length = 5},
		{.offset = 9, .length = 7}, /* Byte 5 (6th byte) used for BI */
		{.offset = 0, .length = 0}} /* End marker */
		/* Bytes offset 4&5 are used by BBT.  Actually only byte 5 is used,
		 * but in order to accomodate for 16 bit bus width, byte 4 is also not used.
		 * If we only use byte-width chip, (We did)
		 * then we can also use byte 4 as free bytes.
		 */
};

/* Small page with BCH-4 */
static struct nand_ecclayout brcmnand_oob_bch4_512 = {
	.eccbytes = 7,
	.eccpos = {9, 10, 11, 12, 13, 14, 15},
	.oobfree = {
		{.offset = 0, .length = 5},
		{.offset = 7, .length = 2}, /* Byte 5 (6th byte) used for BI */
		{.offset = 0, .length = 0}} /* End marker */
};

/*
 * 2K page SLC/MLC with BCH-4 ECC, uses 7 ECC bytes per 512B ECC step
 */
static struct nand_ecclayout brcmnand_oob_bch4_2k = {
	.eccbytes = 7 * 8, /* 7 * 8 = 56 bytes */
	.eccpos =
	{
		9, 10, 11, 12, 13, 14, 15,
		25, 26, 27, 28, 29, 30, 31,
		41, 42, 43, 44, 45, 46, 47,
		57, 58, 59, 60, 61, 62, 63
	},
	.oobfree =
	{
		/* 0 used for BBT and/or manufacturer bad block marker, 
		 * first slice loses 1 byte for BBT
		 */
		{.offset = 1, .length = 8}, /* 1st slice loses byte 0 */
		{.offset = 16, .length = 9}, /* 2nd slice */
		{.offset = 32, .length = 9}, /* 3rd slice  */
		{.offset = 48, .length = 9}, /* 4th slice */
		{.offset = 0, .length = 0} /* End marker */
	}
};


static void *page_buffer = NULL;

/* Private global state */
struct brcmnand_mtd brcmnand_info;

static INLINE void
brcmnand_cmd(osl_t *osh, chipcregs_t *cc, uint opcode)
{
	W_REG(osh, &cc->nand_cmd_start, opcode);
	/* read after write to flush the command */
	R_REG(osh, &cc->nand_cmd_start);
}

int brcmnand_ctrl_verify_ecc(struct nand_chip *chip, int state)
{
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;
	uint32_t addr, ext_addr;
	int err = 0;

	if (state != FL_READING)
		return BRCMNAND_SUCCESS;
	osh = si_osh(sih);
	addr = R_REG(osh, &cc->nand_ecc_corr_addr);
	if (addr) {
		ext_addr = R_REG(osh, &cc->nand_ecc_corr_addr_x);
		/* clear */
		W_REG(osh, &cc->nand_ecc_corr_addr, 0);
		W_REG(osh, &cc->nand_ecc_corr_addr_x, 0);
		err = BRCMNAND_CORRECTABLE_ECC_ERROR;
	}
	/* In BCH4 case, the controller will report BRCMNAND_UNCORRECTABLE_ECC_ERROR
	 * but we cannot resolve this issue in this version. In this case, if we don't
	 * check nand_ecc_unc_addr the process also work smoothly.
	 */
	if (sih->ccrev != 38) {
		addr = R_REG(osh, &cc->nand_ecc_unc_addr);
		if (addr) {
			ext_addr = R_REG(osh, &cc->nand_ecc_unc_addr_x);
			/* clear */
			W_REG(osh, &cc->nand_ecc_unc_addr, 0);
			W_REG(osh, &cc->nand_ecc_unc_addr_x, 0);
			/* If the block was just erased, and have not yet been written to,
			 * this will be flagged, so this could be a false alarm
			 */
			err = BRCMNAND_UNCORRECTABLE_ECC_ERROR;
		}
	}
	return (err);
}

uint32 brcmnand_poll(uint32 pollmask)
{
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;
	uint32 status;

	osh = si_osh(sih);
	status = R_REG(osh, &cc->nand_intfc_status);
	status &= pollmask;

	return status;
}

int brcmnand_cache_is_valid(struct mtd_info *mtd, struct nand_chip *chip, int state)
{
	uint32 pollmask = NIST_CTRL_READY | 0x1;
	unsigned long timeout = msecs_to_jiffies(BRCMNAND_POLL_TIMEOUT);
	unsigned long now = jiffies;
	uint32 status = 0;
	int ret;

	for (;;) {
		if ((status = brcmnand_poll(pollmask)) != 0) {
			break;
		}
		if (time_after(jiffies, now + timeout)) {
			status = brcmnand_poll(pollmask);
			break;
		}
		udelay(1);
	}

	if (status == 0)
		ret = BRCMNAND_TIMED_OUT;
	else if (status & 0x1)
		ret = BRCMNAND_FLASH_STATUS_ERROR;
	else
		ret = brcmnand_ctrl_verify_ecc(chip, state);

	return ret;
}

int brcmnand_spare_is_valid(struct mtd_info *mtd, struct nand_chip *chip, int state)
{
	uint32 pollmask = NIST_CTRL_READY;
	unsigned long timeout = msecs_to_jiffies(BRCMNAND_POLL_TIMEOUT);
	unsigned long now = jiffies;
	uint32 status = 0;
	int ret;

	for (;;) {
		if ((status = brcmnand_poll(pollmask)) != 0) {
			break;
		}
		if (time_after(jiffies, now + timeout)) {
			status = brcmnand_poll(pollmask);
			break;
		}
		udelay(1);
	}

	if (status == 0)
		ret = 0 /* timed out */;
	else
		ret = 1;

	return ret;
}

/**
 * nand_release_device - [GENERIC] release chip
 * @mtd:	MTD device structure
 *
 * Deselect, release chip lock and wake up anyone waiting on the device
 */
static spinlock_t mtd_lock;
static void brcmnand_release_device(struct mtd_info *mtd)
{
	if (nflash_lock == 1) {
		brcmnand_info.nflash->enable( brcmnand_info.nflash, 0);
		NFLASH_LOCK(mtd->mlock);
	}
	nflash_lock --;
	spin_unlock(&mtd_lock);
}

/**
 * brcmnand_get_device - [GENERIC] Get chip for selected access
 * @param chip      the nand chip descriptor
 * @param mtd       MTD device structure
 * @param new_state the state which is requested
 *
 * Get the device and lock it for exclusive access
 */
static int brcmnand_get_device( struct mtd_info *mtd)
{
	spin_lock(&mtd_lock);
	if (nflash_lock == 0) {
		NFLASH_UNLOCK(mtd->mlock);
		brcmnand_info.nflash->enable( brcmnand_info.nflash, 1);
	}
	nflash_lock ++;
	return 0;
}

/**
 * brcmnand_release_device_bcm4706 - [GENERIC] release chip
 * @mtd:	MTD device structure
 *
 * Deselect, release chip lock and wake up anyone waiting on the device
 */
static void
brcmnand_release_device_bcm4706(struct mtd_info *mtd)
{
	NFLASH_UNLOCK(mtd->mlock);
}

/**
 * brcmnand_get_device_bcm4706 - [GENERIC] Get chip for selected access
 * @param chip      the nand chip descriptor
 * @param mtd       MTD device structure
 * @param new_state the state which is requested
 *
 * Get the device and lock it for exclusive access
 */
static int
brcmnand_get_device_bcm4706( struct mtd_info *mtd )
{
	NFLASH_LOCK(mtd->mlock);
	return 0;
}

/**
 * brcmnand_block_checkbad - [GENERIC] Check if a block is marked bad
 * @mtd:	MTD device structure
 * @ofs:	offset from device start
 * @getchip:	0, if the chip is already selected
 * @allowbbt:	1, if its allowed to access the bbt area
 *
 * Check, if the block is bad. Either by reading the bad block table or
 * calling of the scan function.
 */
static int brcmnand_block_checkbad(struct mtd_info *mtd, loff_t ofs, int getchip,
	int allowbbt)
{
	struct nand_chip *chip = mtd->priv;
	int ret;

	if (getchip)
		brcmnand_get_device( mtd );

	if (!chip->bbt)
		ret = chip->block_bad(mtd, ofs, getchip);
	else
		ret = brcmnand_isbad_bbt(mtd, ofs, allowbbt);

	if (getchip)
		brcmnand_release_device(mtd);
	return (ret);
}

/*
 * Returns 0 on success
 */
static int brcmnand_handle_false_read_ecc_unc_errors(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, uint8_t *oob, uint32_t offset)
{
	static uint32_t oobbuf[4];
	uint32_t *p32 = (oob ?  (uint32_t *)oob :  (uint32_t *)&oobbuf[0]);
	int ret = 0;
	uint8_t *oobarea;
	int erased = 0, allFF = 0;
	int i;
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;

	osh = si_osh(sih);
	oobarea = (uint8_t *)p32;
	for (i = 0; i < 4; i++) {
		p32[i] = R_REG(osh, (uint32_t *)((uint32_t)&cc->nand_spare_rd0 + (i * 4)));
	}
	if (brcmnand_info.level == BRCMNAND_ECC_HAMMING) {
		erased =
			(oobarea[6] == 0xff && oobarea[7] == 0xff && oobarea[8] == 0xff);
		allFF =
			(oobarea[6] == 0x00 && oobarea[7] == 0x00 && oobarea[8] == 0x00);
	} else if (brcmnand_info.level >= BRCMNAND_ECC_BCH_1 &&
	           brcmnand_info.level <= BRCMNAND_ECC_BCH_12) {
		erased = allFF = 1;
		/* For BCH-n, the ECC bytes are at the end of the OOB area */
		for (i = mtd->oobsize - chip->ecc.bytes; i < mtd->oobsize; i++) {
			erased = erased && (oobarea[i] == 0xff);
			allFF = allFF && (oobarea[i] == 0x00);
		}
	} else {
		printk("BUG: Unsupported ECC level %d\n", brcmnand_info.level );
		BUG();
	}

	if (erased || allFF) {
		/*
		 * For the first case, the slice is an erased block, and the ECC bytes
		 * are all 0xFF, for the 2nd, all bytes are 0xFF, so the Hamming Codes
		 * for it are all zeroes.  The current version of the BrcmNAND
		 * controller treats these as un-correctable errors.  For either case,
		 * fill data buffer with 0xff and return success.  The error has
		 * already been cleared inside brcmnand_verify_ecc.  Both case will be
		 * handled correctly by the BrcmNand controller in later releases.
		 */
		p32 = (uint32_t *)buf;
		for (i = 0; i < chip->ecc.size/4; i++) {
			p32[i] = 0xFFFFFFFF;
		}
		ret = 0; /* Success */
	} else {
		/* Real error: Disturb read returns uncorrectable errors */
		ret = -EBADMSG;
		printk("<-- %s: ret -EBADMSG\n", __FUNCTION__);
	}
	return ret;
}

static int brcmnand_posted_read_cache(struct mtd_info *mtd, struct nand_chip *chip,
	uint8_t *buf, uint8_t *oob, uint32_t offset)
{
	uint32_t mask = chip->ecc.size - 1;
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;
	int valid;
	uint32_t *to;
	int ret = 0, i;

	if (offset & mask)
		return -EINVAL;

	osh = si_osh(sih);
	W_REG(osh, &cc->nand_cmd_addr, offset);
	PLATFORM_IOFLUSH_WAR();
	brcmnand_cmd(osh, cc, NCMD_PAGE_RD);
	valid = brcmnand_cache_is_valid(mtd, chip, FL_READING);

	switch (valid) {
	case BRCMNAND_CORRECTABLE_ECC_ERROR:
	case BRCMNAND_SUCCESS:
		if (buf) {
			to = (uint32_t *)buf;
			PLATFORM_IOFLUSH_WAR();
			for (i = 0; i < chip->ecc.size; i += 4, to++) {
				*to = R_REG(osh, &cc->nand_cache_data);
			}
		}
		if (oob) {
			to = (uint32_t *)oob;
			PLATFORM_IOFLUSH_WAR();
			for (i = 0; i < mtd->oobsize; i += 4, to++) {
				*to = R_REG(osh, (uint32_t *)((uint32_t)&cc->nand_spare_rd0 + i));
			}
		}
		break;
	case BRCMNAND_UNCORRECTABLE_ECC_ERROR:
		ret = brcmnand_handle_false_read_ecc_unc_errors(mtd, chip, buf, oob, offset);
		break;
	case BRCMNAND_FLASH_STATUS_ERROR:
		ret = -EBADMSG;
		break;
	case BRCMNAND_TIMED_OUT:
		ret = -ETIMEDOUT;
		break;
	default:
		ret = -EFAULT;
		break;
	}

	return (ret);
}

/**
 * nand_read_page_hwecc - [REPLACABLE] hardware ecc based page read function
 * @mtd:	mtd info structure
 * @chip:	nand chip info structure
 * @buf:	buffer to store read data
 *
 * Not for syndrome calculating ecc controllers which need a special oob layout
 */
static int brcmnand_read_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
	uint8_t *buf, int page)
{
	int eccsteps;
	int data_read = 0;
	int oob_read = 0;
	int corrected = 0;
	int ret = 0;
	uint32_t offset = page << chip->page_shift;
	uint8_t *oob = chip->oob_poi;

	brcmnand_get_device( mtd );
	for (eccsteps = 0; eccsteps < chip->ecc.steps; eccsteps++) {
		ret = brcmnand_posted_read_cache(mtd, chip, &buf[data_read],
			oob ? &oob[oob_read]: NULL, offset + data_read);
		if (ret == BRCMNAND_CORRECTABLE_ECC_ERROR && !corrected) {
			mtd->ecc_stats.corrected++;
			corrected = 1;
			ret = 0;
		} else {
			if (ret < 0)
				break;
		}
		data_read += chip->ecc.size;
		oob_read += mtd->oobsize;
	}
	brcmnand_release_device(mtd);
	return (ret);
}

/**
 * brcmnand_transfer_oob - [Internal] Transfer oob to client buffer
 * @chip:	nand chip structure
 * @oob:	oob destination address
 * @ops:	oob ops structure
 * @len:	size of oob to transfer
 */
static uint8_t *brcmnand_transfer_oob(struct nand_chip *chip, uint8_t *oob,
	struct mtd_oob_ops *ops, size_t len)
{
	switch (ops->mode) {

	case MTD_OOB_PLACE:
	case MTD_OOB_RAW:
		memcpy(oob, chip->oob_poi + ops->ooboffs, len);
		return oob + len;

	case MTD_OOB_AUTO: {
		struct nand_oobfree *free = chip->ecc.layout->oobfree;
		uint32_t boffs = 0, roffs = ops->ooboffs;
		size_t bytes = 0;

		for (; free->length && len; free++, len -= bytes) {
			/* Read request not from offset 0 ? */
			if (unlikely(roffs)) {
				if (roffs >= free->length) {
					roffs -= free->length;
					continue;
				}
				boffs = free->offset + roffs;
				bytes = min_t(size_t, len,
				              (free->length - roffs));
				roffs = 0;
			} else {
				bytes = min_t(size_t, len, free->length);
				boffs = free->offset;
			}
			memcpy(oob, chip->oob_poi + boffs, bytes);
			oob += bytes;
		}
		return oob;
	}
	default:
		BUG();
	}
	return NULL;
}

/**
 * brcmnand_do_read_ops - [Internal] Read data with ECC
 *
 * @mtd:	MTD device structure
 * @from:	offset to read from
 * @ops:	oob ops structure
 *
 * Internal function. Called with chip held.
 */
static int brcmnand_do_read_ops(struct mtd_info *mtd, loff_t from,
	struct mtd_oob_ops *ops)
{
	int page, realpage, col, bytes, aligned;
	struct nand_chip *chip = mtd->priv;
	struct mtd_ecc_stats stats;
	int ret = 0;
	uint32_t readlen = ops->len;
	uint32_t oobreadlen = ops->ooblen;
	uint8_t *bufpoi, *oob, *buf;

	stats = mtd->ecc_stats;

	realpage = (int)(from >> chip->page_shift);
	page = realpage & chip->pagemask;

	col = (int)(from & (mtd->writesize - 1));

	buf = ops->datbuf;
	oob = ops->oobbuf;

	while (1) {
		bytes = min(mtd->writesize - col, readlen);
		aligned = (bytes == mtd->writesize);

		/* Is the current page in the buffer ? */
		if (realpage != chip->pagebuf || oob) {
			bufpoi = aligned ? buf : chip->buffers->databuf;
			chip->pagebuf = page;
			/* Now read the page into the buffer */
			ret = chip->ecc.read_page(mtd, chip, bufpoi, page);
			if (ret < 0)
				break;

			/* Transfer not aligned data */
			if (!aligned) {
				chip->pagebuf = realpage;
				memcpy(buf, chip->buffers->databuf + col, bytes);
			}

			buf += bytes;

			if (unlikely(oob)) {
				if (ops->mode != MTD_OOB_RAW) {
					int toread = min(oobreadlen,
						chip->ecc.layout->oobavail);
					if (toread) {
						oob = brcmnand_transfer_oob(chip,
							oob, ops, toread);
						oobreadlen -= toread;
					}
				} else
					buf = brcmnand_transfer_oob(chip,
						buf, ops, mtd->oobsize);
			}
		} else {
			memcpy(buf, chip->buffers->databuf + col, bytes);
			buf += bytes;
		}

		readlen -= bytes;

		if (!readlen)
			break;

		/* For subsequent reads align to page boundary. */
		col = 0;
		/* Increment page address */
		realpage++;

		page = realpage & chip->pagemask;
	}

	ops->retlen = ops->len - (size_t) readlen;
	if (oob)
		ops->oobretlen = ops->ooblen - oobreadlen;

	if (ret)
		return ret;

	if (mtd->ecc_stats.failed - stats.failed)
		return -EBADMSG;

	return  mtd->ecc_stats.corrected - stats.corrected ? -EUCLEAN : 0;
}

/**
 * brcmnand_read - [MTD Interface] MTD compability function for nand_do_read_ecc
 * @mtd:    MTD device structure
 * @from:   offset to read from
 * @len:    number of bytes to read
 * @retlen: pointer to variable to store the number of read bytes
 * @buf:    the databuffer to put data
 *
 * Get hold of the chip and call nand_do_read
 */
static int
brcmnand_read(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct nand_chip *chip = mtd->priv;
	int ret;

	if ((from + len) > mtd->size)
		return -EINVAL;
	if (!len)
		return 0;

	brcmnand_get_device( mtd );
	chip->ops.len = len;
	chip->ops.datbuf = buf;
	chip->ops.oobbuf = NULL;

	ret = brcmnand_do_read_ops(mtd, from, &chip->ops);

	*retlen = chip->ops.retlen;

	brcmnand_release_device(mtd);

	return ret;
}

static int brcmnand_posted_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
	uint8_t *oob, uint32_t offset)
{
	uint32_t mask = chip->ecc.size - 1;
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;
	int valid;
	uint32 *to;
	int ret = 0, i;

	if (offset & mask)
		return -EINVAL;

	osh = si_osh(sih);
	W_REG(osh, &cc->nand_cmd_addr, offset);
	PLATFORM_IOFLUSH_WAR();
	brcmnand_cmd(osh, cc, NCMD_SPARE_RD);
	valid = brcmnand_spare_is_valid(mtd, chip, FL_READING);

	switch (valid) {
	case 1:
		if (oob) {
			to = (uint32 *)oob;
			for (i = 0; i < mtd->oobsize; i += 4, to++) {
				*to = R_REG(osh, (uint32_t *)((uint32_t)&cc->nand_spare_rd0 + i));
			}
		}
		break;
	case 0:
		ret = -ETIMEDOUT;
		break;
	default:
		ret = -EFAULT;
		break;
	}
	return (ret);
}

/**
 * brcmnand_read_oob_hwecc - [REPLACABLE] the most common OOB data read function
 * @mtd:	mtd info structure
 * @chip:	nand chip info structure
 * @page:	page number to read
 * @sndcmd:	flag whether to issue read command or not
 */
static int brcmnand_read_oob_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
	int page, int sndcmd)
{
	int eccsteps;
	int data_read = 0;
	int oob_read = 0;
	int corrected = 0;
	int ret = 0;
	uint32_t offset = page << chip->page_shift;
	uint8_t *oob = chip->oob_poi;

	for (eccsteps = 0; eccsteps < chip->ecc.steps; eccsteps++) {
		ret = brcmnand_posted_read_oob(mtd, chip, &oob[oob_read], offset + data_read);
		if (ret == BRCMNAND_CORRECTABLE_ECC_ERROR && !corrected) {
			mtd->ecc_stats.corrected++;
			/* Only update stats once per page */
			corrected = 1;
			ret = 0;
		} else {
			if (ret < 0)
				break;
		}
		data_read += chip->ecc.size;
		oob_read += mtd->oobsize;
	}

	return (ret);
}

/**
 * brcmnand_do_read_oob - [Intern] NAND read out-of-band
 * @mtd:	MTD device structure
 * @from:	offset to read from
 * @ops:	oob operations description structure
 *
 * NAND read out-of-band data from the spare area
 */
static int brcmnand_do_read_oob(struct mtd_info *mtd, loff_t from,
	struct mtd_oob_ops *ops)
{
	int page, realpage;
	struct nand_chip *chip = mtd->priv;
	int readlen = ops->ooblen;
	int len;
	uint8_t *buf = ops->oobbuf;
	int ret;

	DEBUG(MTD_DEBUG_LEVEL3, "nand_read_oob: from = 0x%08Lx, len = %i\n",
	      (unsigned long long)from, readlen);

	if (ops->mode == MTD_OOB_AUTO)
		len = chip->ecc.layout->oobavail;
	else
		len = mtd->oobsize;

	if (unlikely(ops->ooboffs >= len)) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_read_oob: "
			"Attempt to start read outside oob\n");
		return -EINVAL;
	}

	/* Do not allow reads past end of device */
	if (unlikely(from >= mtd->size ||
		ops->ooboffs + readlen > ((mtd->size >> chip->page_shift) -
		(from >> chip->page_shift)) * len)) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_read_oob: "
			"Attempt read beyond end of device\n");
		return -EINVAL;
	}
	/* Shift to get page */
	realpage = (int)(from >> chip->page_shift);
	page = realpage & chip->pagemask;

	while (1) {
		ret = chip->ecc.read_oob(mtd, chip, page, 0);
		if (ret)
			break;
		len = min(len, readlen);
		buf = brcmnand_transfer_oob(chip, buf, ops, len);

		readlen -= len;
		if (!readlen)
			break;

		/* Increment page address */
		realpage++;

		page = realpage & chip->pagemask;
	}

	ops->oobretlen = ops->ooblen;
	return (ret);
}

/**
 * brcmnand_read_oob - [MTD Interface] NAND read data and/or out-of-band
 * @mtd:	MTD device structure
 * @from:	offset to read from
 * @ops:	oob operation description structure
 *
 * NAND read data and/or out-of-band data
 */
static int brcmnand_read_oob(struct mtd_info *mtd, loff_t from,
	struct mtd_oob_ops *ops)
{
	int ret = -ENOTSUPP;

	ops->retlen = 0;

	/* Do not allow reads past end of device */
	if (ops->datbuf && (from + ops->len) > mtd->size) {
		DEBUG(MTD_DEBUG_LEVEL0, "brcmnand_read_oob: "
		      "Attempt read beyond end of device\n");
		return -EINVAL;
	}

	brcmnand_get_device( mtd );

	switch (ops->mode) {
	case MTD_OOB_PLACE:
	case MTD_OOB_AUTO:
	case MTD_OOB_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = brcmnand_do_read_oob(mtd, from, ops);
	else
		ret = brcmnand_do_read_ops(mtd, from, ops);

out:
	brcmnand_release_device(mtd);
	return ret;
}

static int brcmnand_ctrl_write_is_complete(struct mtd_info *mtd, struct nand_chip *chip,
	int *need_bbt)
{
	uint32 pollmask = NIST_CTRL_READY | 0x1;
	unsigned long timeout = msecs_to_jiffies(BRCMNAND_POLL_TIMEOUT);
	unsigned long now = jiffies;
	uint32 status = 0;
	int ret;

	for (;;) {
		if ((status = brcmnand_poll(pollmask)) != 0) {
			break;
		}
		if (time_after(jiffies, now + timeout)) {
			status = brcmnand_poll(pollmask);
			break;
		}
		udelay(1);
	}

	*need_bbt = 0;
	if (status == 0)
		ret = 0; /* timed out */
	else {
		ret = 1;
		if (status & 0x1)
			*need_bbt = 1;
	}

	return ret;
}

/**
 * brcmnand_posted_write - [BrcmNAND Interface] Write a buffer to the flash
 *  cache
 * Assuming brcmnand_get_device() has been called to obtain exclusive lock
 *
 * @param mtd       MTD data structure
 * @param chip	    nand chip info structure
 * @param buf       the databuffer to put/get data
 * @param oob	    Spare area, pass NULL if not interested
 * @param offset    offset to write to, and must be 512B aligned
 *
 */
static int brcmnand_posted_write_cache(struct mtd_info *mtd, struct nand_chip *chip,
	const uint8_t *buf, uint8_t *oob, uint32_t offset)
{
	uint32_t mask = chip->ecc.size - 1;
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;
	int i, ret = 0;
	uint32_t *from;

	if (offset & mask) {
		ret = -EINVAL;
		goto out;
	}

	osh = si_osh(sih);
	from = (uint32_t *)buf;
	for (i = 0; i < chip->ecc.size; i += 4, from++) {
		W_REG(osh, &cc->nand_cache_data, *from);
	}
out:
	return (ret);
}

/**
 * brcmnand_write_page_hwecc - [REPLACABLE] hardware ecc based page write function
 * @mtd:	mtd info structure
 * @chip:	nand chip info structure
 * @buf:	data buffer
 */
static void brcmnand_write_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
	const uint8_t *buf)
{
	int eccsize = chip->ecc.size;
	int eccsteps;
	int data_written = 0;
	int oob_written = 0;
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;
	uint32_t reg;
	int ret = 0, need_bbt = 0;
	uint32_t offset = chip->pagebuf << chip->page_shift;

	uint8_t oob_buf[NAND_MAX_OOBSIZE];
	int *eccpos = chip->ecc.layout->eccpos;
	int i;
	uint8_t *oob = chip->oob_poi;

	brcmnand_get_device( mtd );
	osh = si_osh(sih);
	/* full page write */
	/* disable partial page enable */
	reg = R_REG(osh, &cc->nand_acc_control);
	reg &= ~NAC_PARTIAL_PAGE_EN;
	W_REG(osh, &cc->nand_acc_control, reg);

	for (eccsteps = 0; eccsteps < chip->ecc.steps; eccsteps++) {
		W_REG(osh, &cc->nand_cache_addr, 0);
		W_REG(osh, &cc->nand_cmd_addr, data_written);
		ret = brcmnand_posted_write_cache(mtd, chip, &buf[data_written],
			oob ? &oob[oob_written]: NULL, offset + data_written);
		if (ret < 0) {
			goto out;
		}
		data_written += eccsize;
		oob_written += mtd->oobsize;
	}

	W_REG(osh, &cc->nand_cmd_addr, offset + mtd->writesize - NFL_SECTOR_SIZE);
	brcmnand_cmd(osh, cc, NCMD_PAGE_PROG);
	if (brcmnand_ctrl_write_is_complete(mtd, chip, &need_bbt)) {
		if (!need_bbt) {
			/* write the oob */
			if (oob) {
				/* Enable partial page program so that we can
				 * overwrite the spare area
				 */
				reg = R_REG(osh, &cc->nand_acc_control);
				reg |= NAC_PARTIAL_PAGE_EN;
				W_REG(osh, &cc->nand_acc_control, reg);

				memcpy(oob_buf, oob, NAND_MAX_OOBSIZE);
				/* read from the spare area first */
				ret = chip->ecc.read_oob(mtd, chip, chip->pagebuf, 0);
				if (ret != 0)
					goto out;
				/* merge the oob */
				for (i = 0; i < chip->ecc.total; i++)
					oob_buf[eccpos[i]] = chip->oob_poi[eccpos[i]];
				memcpy(chip->oob_poi, oob_buf, NAND_MAX_OOBSIZE);
				/* write back to the spare area */
				ret = chip->ecc.write_oob(mtd, chip, chip->pagebuf);
			}
			goto out;
		} else {
			ret = chip->block_markbad(mtd, offset);
			goto out;
		}
	}
	/* timed out */
	ret = -ETIMEDOUT;

out:
	if (ret != 0)
		printk(KERN_ERR "brcmnand_write_page_hwecc failed\n");
	brcmnand_release_device(mtd);
	return;
}

/* 
 * brcmnand_posted_write_oob - [BrcmNAND Interface] Write the spare area
 * @mtd:	    MTD data structure
 * @chip:	    nand chip info structure
 * @oob:	    Spare area, pass NULL if not interested.  Must be able to
 *                  hold mtd->oobsize (16) bytes.
 * @offset:	    offset to write to, and must be 512B aligned
 * 
 */
static int brcmnand_posted_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
	uint8_t *oob, uint32_t offset)
{
	uint32_t mask = chip->ecc.size - 1;
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;
	int i, ret = 0, need_bbt = 0;
	uint32_t *from;
	uint32_t reg;
	uint8_t oob_buf0[16];

	if (offset & mask) {
		ret = -EINVAL;
		goto out;
	}

	osh = si_osh(sih);
	/* Make sure we are in partial page program mode */
	reg = R_REG(osh, &cc->nand_acc_control);
	reg |= NAC_PARTIAL_PAGE_EN;
	W_REG(osh, &cc->nand_acc_control, reg);

	W_REG(osh, &cc->nand_cmd_addr, offset);
	if (!oob) {
		ret = -EINVAL;
		goto out;
	}
	memcpy(oob_buf0, oob, mtd->oobsize);
	from = (uint32_t *)oob_buf0;
	for (i = 0; i < mtd->oobsize; i += 4, from++) {
		W_REG(osh, (uint32_t *)((uint32_t)&cc->nand_spare_wr0 + i), *from);
	}
	PLATFORM_IOFLUSH_WAR();
	brcmnand_cmd(osh, cc, NCMD_SPARE_PROG);
	if (brcmnand_ctrl_write_is_complete(mtd, chip, &need_bbt)) {
		if (!need_bbt) {
			ret = 0;
			goto out;
		} else {
			ret = chip->block_markbad(mtd, offset);
			goto out;
		}
	}
	/* timed out */
	ret = -ETIMEDOUT;
out:
	return ret;
}


/**
 * brcmnand_write_page - [REPLACEABLE] write one page
 * @mtd:	MTD device structure
 * @chip:	NAND chip descriptor
 * @buf:	the data to write
 * @page:	page number to write
 * @cached:	cached programming
 * @raw:	use _raw version of write_page
 */
static int brcmnand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
	const uint8_t *buf, int page, int cached, int raw)
{
	chip->pagebuf = page;
	chip->ecc.write_page(mtd, chip, buf);

	return 0;
}

/**
 * brcmnand_fill_oob - [Internal] Transfer client buffer to oob
 * @chip:	nand chip structure
 * @oob:	oob data buffer
 * @ops:	oob ops structure
 */
static uint8_t *brcmnand_fill_oob(struct nand_chip *chip, uint8_t *oob,
	struct mtd_oob_ops *ops)
{
	size_t len = ops->ooblen;

	switch (ops->mode) {

	case MTD_OOB_PLACE:
	case MTD_OOB_RAW:
		memcpy(chip->oob_poi + ops->ooboffs, oob, len);
		return oob + len;

	case MTD_OOB_AUTO: {
		struct nand_oobfree *free = chip->ecc.layout->oobfree;
		uint32_t boffs = 0, woffs = ops->ooboffs;
		size_t bytes = 0;

		for (; free->length && len; free++, len -= bytes) {
			/* Write request not from offset 0 ? */
			if (unlikely(woffs)) {
				if (woffs >= free->length) {
					woffs -= free->length;
					continue;
				}
				boffs = free->offset + woffs;
				bytes = min_t(size_t, len,
				              (free->length - woffs));
				woffs = 0;
			} else {
				bytes = min_t(size_t, len, free->length);
				boffs = free->offset;
			}
			memcpy(chip->oob_poi + boffs, oob, bytes);
			oob += bytes;
		}
		return oob;
	}
	default:
		BUG();
	}
	return NULL;
}

#define NOTALIGNED(x)	(x & (chip->subpagesize - 1)) != 0

/**
 * brcmnand_do_write_ops - [Internal] NAND write with ECC
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @ops:	oob operations description structure
 *
 * NAND write with ECC
 */
static int brcmnand_do_write_ops(struct mtd_info *mtd, loff_t to,
	struct mtd_oob_ops *ops)
{
	int realpage, page, blockmask;
	struct nand_chip *chip = mtd->priv;
	uint32_t writelen = ops->len;
	uint8_t *oob = ops->oobbuf;
	uint8_t *buf = ops->datbuf;
	int ret;

	ops->retlen = 0;
	if (!writelen)
		return 0;

	/* reject writes, which are not page aligned */
	if (NOTALIGNED(to) || NOTALIGNED(ops->len)) {
		printk(KERN_NOTICE "nand_write: "
		       "Attempt to write not page aligned data\n");
		return -EINVAL;
	}


	realpage = (int)(to >> chip->page_shift);
	page = realpage & chip->pagemask;
	blockmask = (1 << (chip->phys_erase_shift - chip->page_shift)) - 1;

	/* Invalidate the page cache, when we write to the cached page */
	if (to <= (chip->pagebuf << chip->page_shift) &&
	    (chip->pagebuf << chip->page_shift) < (to + ops->len))
		chip->pagebuf = -1;

	/* If we're not given explicit OOB data, let it be 0xFF */
	if (likely(!oob))
		memset(chip->oob_poi, 0xff, mtd->oobsize);

	while (1) {
		int bytes = mtd->writesize;
		int cached = writelen > bytes && page != blockmask;
		uint8_t *wbuf = buf;

		if (unlikely(oob))
			oob = brcmnand_fill_oob(chip, oob, ops);

		ret = chip->write_page(mtd, chip, wbuf, page, cached,
		                       (ops->mode == MTD_OOB_RAW));
		if (ret)
			break;

		writelen -= bytes;
		if (!writelen)
			break;

		buf += bytes;
		realpage++;

		page = realpage & chip->pagemask;
	}

	ops->retlen = ops->len - writelen;
	if (unlikely(oob))
		ops->oobretlen = ops->ooblen;
	return ret;
}

/**
 * brcmnand_write - [MTD Interface] NAND write with ECC
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @len:	number of bytes to write
 * @retlen:	pointer to variable to store the number of written bytes
 * @buf:	the data to write
 *
 * NAND write with ECC
 */
static int brcmnand_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const uint8_t *buf)
{
	struct nand_chip *chip = mtd->priv;
	int ret;

	/* Do not allow reads past end of device */
	if ((to + len) > mtd->size)
		return -EINVAL;
	if (!len)
		return 0;

	brcmnand_get_device( mtd );

	chip->ops.len = len;
	chip->ops.datbuf = (uint8_t *)buf;
	chip->ops.oobbuf = NULL;

	ret = brcmnand_do_write_ops(mtd, to, &chip->ops);

	*retlen = chip->ops.retlen;

	brcmnand_release_device(mtd);

	return ret;
}

/**
 * brcmnand_write_oob_hwecc - [INTERNAL] write one page
 * @mtd:    MTD device structure
 * @chip:   NAND chip descriptor. The oob_poi ptr points to the OOB buffer.
 * @page:   page number to write
 */
static int brcmnand_write_oob_hwecc(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	int eccsteps;
	int oob_written = 0, data_written = 0;
	uint32_t offset = page << chip->page_shift;
	uint8_t *oob = chip->oob_poi;
	int ret = 0;

	for (eccsteps = 0; eccsteps < chip->ecc.steps; eccsteps++) {
		ret = brcmnand_posted_write_oob(mtd, chip, oob + oob_written,
			offset + data_written);
		if (ret < 0)
			break;
		data_written += chip->ecc.size;
		oob_written += mtd->oobsize;
	}
	return (ret);
}

/**
 * brcmnand_do_write_oob - [MTD Interface] NAND write out-of-band
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @ops:	oob operation description structure
 *
 * NAND write out-of-band
 */
static int brcmnand_do_write_oob(struct mtd_info *mtd, loff_t to,
	struct mtd_oob_ops *ops)
{
	int page, status, len;
	struct nand_chip *chip = mtd->priv;

	DEBUG(MTD_DEBUG_LEVEL3, "nand_write_oob: to = 0x%08x, len = %i\n",
	      (unsigned int)to, (int)ops->ooblen);

	if (ops->mode == MTD_OOB_AUTO)
		len = chip->ecc.layout->oobavail;
	else
		len = mtd->oobsize;

	/* Do not allow write past end of page */
	if ((ops->ooboffs + ops->ooblen) > len) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_write_oob: "
		      "Attempt to write past end of page\n");
		return -EINVAL;
	}

	if (unlikely(ops->ooboffs >= len)) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_write_oob: "
			"Attempt to start write outside oob\n");
		return -EINVAL;
	}

	/* Do not allow reads past end of device */
	if (unlikely(to >= mtd->size ||
	             ops->ooboffs + ops->ooblen >
			((mtd->size >> chip->page_shift) -
			 (to >> chip->page_shift)) * len)) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_read_oob: "
			"Attempt write beyond end of device\n");
		return -EINVAL;
	}


	/* Shift to get page */
	page = (int)(to >> chip->page_shift);

	/* Invalidate the page cache, if we write to the cached page */
	if (page == chip->pagebuf)
		chip->pagebuf = -1;

	memset(chip->oob_poi, 0xff, mtd->oobsize);
	brcmnand_fill_oob(chip, ops->oobbuf, ops);
	status = chip->ecc.write_oob(mtd, chip, page & chip->pagemask);
	memset(chip->oob_poi, 0xff, mtd->oobsize);

	if (status)
		return status;

	ops->oobretlen = ops->ooblen;

	return 0;
}

/**
 * brcmnand_write_oob - [MTD Interface] NAND write data and/or out-of-band
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @ops:	oob operation description structure
 */
static int brcmnand_write_oob(struct mtd_info *mtd, loff_t to,
	struct mtd_oob_ops *ops)
{
	int ret = -ENOTSUPP;

	ops->retlen = 0;

	/* Do not allow writes past end of device */
	if (ops->datbuf && (to + ops->len) > mtd->size) {
		DEBUG(MTD_DEBUG_LEVEL0, "brcmnand_write_oob: "
		      "Attempt write beyond end of device\n");
		return -EINVAL;
	}

	brcmnand_get_device( mtd );

	switch (ops->mode) {
	case MTD_OOB_PLACE:
	case MTD_OOB_AUTO:
	case MTD_OOB_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = brcmnand_do_write_oob(mtd, to, ops);
	else
		ret = brcmnand_do_write_ops(mtd, to, ops);

out:
	brcmnand_release_device(mtd);
	return ret;
}

static int brcmnand_erase_bbt(struct mtd_info *mtd, struct erase_info *instr, int allowbbt)
{
	struct nand_chip * chip = mtd->priv;
	int page, len, pages_per_block, block_size;
	loff_t addr;
	int ret = 0;
	int need_bbt = 0;
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;

	DEBUG(MTD_DEBUG_LEVEL3, "nand_erase: start = 0x%08x, len = %i\n",
	      (unsigned int)instr->addr, (unsigned int)instr->len);

	block_size = 1 << chip->phys_erase_shift;

	/* Start address must align on block boundary */
	if (instr->addr & (block_size - 1)) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_erase: Unaligned address\n");
		return -EINVAL;
	}

	/* Length must align on block boundary */
	if (instr->len & (block_size - 1)) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_erase: "
		      "Length not block aligned\n");
		return -EINVAL;
	}

	/* Do not allow erase past end of device */
	if ((instr->len + instr->addr) > mtd->size) {
		DEBUG(MTD_DEBUG_LEVEL0, "nand_erase: "
		      "Erase past end of device\n");
		return -EINVAL;
	}

	instr->fail_addr = 0xffffffff;

	/* Grab the lock and see if the device is available */
	brcmnand_get_device( mtd );

	/* Shift to get first page */
	page = (int)(instr->addr >> chip->page_shift);

	/* Calculate pages in each block */
	pages_per_block = 1 << (chip->phys_erase_shift - chip->page_shift);

	osh = si_osh(sih);
	/* Clear ECC registers */
	W_REG(osh, &cc->nand_ecc_corr_addr, 0);
	W_REG(osh, &cc->nand_ecc_corr_addr_x, 0);
	W_REG(osh, &cc->nand_ecc_unc_addr, 0);
	W_REG(osh, &cc->nand_ecc_unc_addr_x, 0);

	/* Loop throught the pages */
	len = instr->len;
	addr = instr->addr;
	instr->state = MTD_ERASING;

	while (len) {
		/*
		 * heck if we have a bad block, we do not erase bad blocks !
		 */
		if (brcmnand_block_checkbad(mtd, ((loff_t) page) <<
					chip->page_shift, 0, allowbbt)) {
			printk(KERN_WARNING "nand_erase: attempt to erase a "
			       "bad block at page 0x%08x\n", page);
			instr->state = MTD_ERASE_FAILED;
			goto erase_exit;
		}

		/*
		 * Invalidate the page cache, if we erase the block which
		 * contains the current cached page
		 */
		if (page <= chip->pagebuf && chip->pagebuf <
		    (page + pages_per_block))
			chip->pagebuf = -1;

		W_REG(osh, &cc->nand_cmd_addr, (page << chip->page_shift));
		brcmnand_cmd(osh, cc, NCMD_BLOCK_ERASE);

		/* Wait until flash is ready */
		ret = brcmnand_ctrl_write_is_complete(mtd, chip, &need_bbt);

		if (need_bbt) {
			if (!allowbbt) {
				DEBUG(MTD_DEBUG_LEVEL0, "nand_erase: "
				      "Failed erase, page 0x%08x\n", page);
				instr->state = MTD_ERASE_FAILED;
				instr->fail_addr = (page << chip->page_shift);
				chip->block_markbad(mtd, addr);
				goto erase_exit;
			}
		}

		/* Increment page address and decrement length */
		len -= (1 << chip->phys_erase_shift);
		page += pages_per_block;
	}
	instr->state = MTD_ERASE_DONE;

erase_exit:

	ret = instr->state == MTD_ERASE_DONE ? 0 : -EIO;
	/* Do call back function */
	if (!ret)
		mtd_erase_callback(instr);

	/* Deselect and wake up anyone waiting on the device */
	brcmnand_release_device(mtd);

	return ret;
}

static int
brcmnand_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	int allowbbt = 0;
	int ret = 0;

	/* do not allow erase of bbt */
	ret = brcmnand_erase_bbt(mtd, instr, allowbbt);

	return ret;
}

/**
 * brcmnand_sync - [MTD Interface] sync
 * @mtd:	MTD device structure
 *
 * Sync is actually a wait for chip ready function
 */
static void brcmnand_sync(struct mtd_info *mtd)
{

	DEBUG(MTD_DEBUG_LEVEL3, "nand_sync: called\n");

	/* Grab the lock and see if the device is available */
	brcmnand_get_device( mtd );
	PLATFORM_IOFLUSH_WAR();

	/* Release it and go back */
	brcmnand_release_device(mtd);
}

/**
 * brcmnand_block_isbad - [MTD Interface] Check if block at offset is bad
 * @mtd:	MTD device structure
 * @offs:	offset relative to mtd start
 */
static int brcmnand_block_isbad(struct mtd_info *mtd, loff_t offs)
{
	/* Check for invalid offset */
	if (offs > mtd->size)
		return -EINVAL;

	return brcmnand_block_checkbad(mtd, offs, 1, 0);
}

/**
 * brcmnand_default_block_markbad - [DEFAULT] mark a block bad
 * @mtd:	MTD device structure
 * @ofs:	offset from device start
 *
 * This is the default implementation, which can be overridden by
 * a hardware specific driver.
*/
static int brcmnand_default_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = mtd->priv;
	uint8_t bbmarker[1] = {0};
	uint8_t *buf = chip->oob_poi;
	int block, ret;
	int page, dir;

	/* Get block number */
	block = (int)(ofs >> chip->bbt_erase_shift);
	/* Get page number */
		page = block << (chip->bbt_erase_shift - chip->page_shift);
		dir = 1;

	if (chip->bbt)
		chip->bbt[block >> 2] |= 0x01 << ((block & 0x03) << 1);
	memcpy(buf, ffchars, NAND_MAX_OOBSIZE);
	memcpy(buf + chip->badblockpos, bbmarker, sizeof(bbmarker));
	ret = chip->ecc.write_oob(mtd, chip, page);
	page += dir;
	ret = chip->ecc.write_oob(mtd, chip, page);

	/* According to the HW guy, even if the write fails, the controller have
	 * written a 0 pattern that certainly would have written a non 0xFF value
	 * into the BI marker.
	 * 
	 * Ignoring ret.  Even if we fail to write the BI bytes, just ignore it, 
	 * and mark the block as bad in the BBT
	 */
	ret = brcmnand_update_bbt(mtd, ofs);
	mtd->ecc_stats.badblocks++;
	return ret;
}

/**
 * brcmnand_block_markbad - [MTD Interface] Mark block at the given offset as bad
 * @mtd:	MTD device structure
 * @ofs:	offset relative to mtd start
 */
static int brcmnand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = mtd->priv;
	int ret;

	if ((ret = brcmnand_block_isbad(mtd, ofs))) {
		/* If it was bad already, return success and do nothing. */
		if (ret > 0)
			return 0;
		return ret;
	}

	return chip->block_markbad(mtd, ofs);
}

/**
 * brcmnand_suspend - [MTD Interface] Suspend the NAND flash
 * @mtd:	MTD device structure
 */
static int brcmnand_suspend(struct mtd_info *mtd)
{
	return brcmnand_get_device( mtd );
}

/**
 * brcmnand_resume - [MTD Interface] Resume the NAND flash
 * @mtd:	MTD device structure
 */
static void brcmnand_resume(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	if (chip->state == FL_PM_SUSPENDED)
		brcmnand_release_device(mtd);
	else
		printk(KERN_ERR "brcmnand_resume() called for a chip which is not "
		       "in suspended state\n");
}

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

struct mtd_partition *init_brcmnand_mtd_partitions(struct mtd_info *mtd, size_t size)
{
	brcmnand_parts[0].offset = NFL_BOOT_OS_SIZE;
	brcmnand_parts[0].size = size - NFL_BOOT_OS_SIZE - NFL_BBT_SIZE;

	return brcmnand_parts;
}

/**
 * brcmnand_check_command_done - [DEFAULT] check if command is done
 * @mtd:	MTD device structure
 *
 * Return 0 to process next command
 */
static int
brcmnand_check_command_done(void)
{
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;
	int count = 0;

	osh = si_osh(sih);

	while (R_REG(osh, &cc->nflashctrl) & NFC_START) {
		if (++count > BRCMNAND_POLL_TIMEOUT) {
			printk("brcmnand_check_command_done: command timeout\n");
			return -1;
		}
	}

	return 0;
}

/**
 * brcmnand_hwcontrol - [DEFAULT] Issue command and address cycles to the chip
 * @mtd:	MTD device structure
 * @cmd:	the command to be sent
 * @ctrl:	the control code to be sent
 *
 * Issue command and address cycles to the chip
 */
static void
brcmnand_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;
	unsigned int val = 0;

	osh = si_osh(sih);

	if (cmd == NAND_CMD_NONE)
		return;

	if (ctrl & NAND_CLE) {
		val = cmd | NFC_CMD0;
	}
	else {
		switch (ctrl & (NAND_ALE_COL | NAND_ALE_ROW)) {
		case NAND_ALE_COL:
			W_REG(osh, &cc->nflashcoladdr, cmd);
			val = NFC_COL;
			break;
		case NAND_ALE_ROW:
			W_REG(osh, &cc->nflashrowaddr, cmd);
			val = NFC_ROW;
			break;
		default:
			BUG();
		}
	}

	/* nCS is not needed for reset command */ 
	if (cmd != NAND_CMD_RESET)
		val |= NFC_CSA;

	val |= NFC_START;
	W_REG(osh, &cc->nflashctrl, val);

	brcmnand_check_command_done();
}

/**
 * brcmnand_command_lp - [DEFAULT] Send command to NAND large page device
 * @mtd:	MTD device structure
 * @command:	the command to be sent
 * @column:	the column address for this command, -1 if none
 * @page_addr:	the page address for this command, -1 if none
 *
 * Send command to NAND device. This is the version for the new large page
 * devices We dont have the separate regions as we have in the small page
 * devices.  We must emulate NAND_CMD_READOOB to keep the code compatible.
 */
static void
brcmnand_command_lp(struct mtd_info *mtd, unsigned int command, int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;

	/* Emulate NAND_CMD_READOOB */
	if (command == NAND_CMD_READOOB) {
		column += mtd->writesize;
		command = NAND_CMD_READ0;
	}

	/* Command latch cycle */
	chip->cmd_ctrl(mtd, command & 0xff, NAND_NCE | NAND_CLE);

	if (column != -1 || page_addr != -1) {
		int ctrl = NAND_NCE | NAND_ALE;

		/* Serially input address */
		if (column != -1) {
			ctrl |= NAND_ALE_COL;

			/* Adjust columns for 16 bit buswidth */
			if (chip->options & NAND_BUSWIDTH_16)
				column >>= 1;

			chip->cmd_ctrl(mtd, column, ctrl);
		}

		if (page_addr != -1) {
			ctrl &= ~NAND_ALE_COL;
			ctrl |= NAND_ALE_ROW;

			chip->cmd_ctrl(mtd, page_addr, ctrl);
		}
	}

	chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE);

	/*
	 * program and erase have their own busy handlers
	 * status, sequential in, and deplete1 need no delay
	 */
	switch (command) {

	case NAND_CMD_CACHEDPROG:
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_RNDIN:
	case NAND_CMD_STATUS:
	case NAND_CMD_DEPLETE1:
		return;

	/*
	 * read error status commands require only a short delay
	 */
	case NAND_CMD_STATUS_ERROR:
	case NAND_CMD_STATUS_ERROR0:
	case NAND_CMD_STATUS_ERROR1:
	case NAND_CMD_STATUS_ERROR2:
	case NAND_CMD_STATUS_ERROR3:
		udelay(chip->chip_delay);
		return;

	case NAND_CMD_RESET:
		if (chip->dev_ready)
			break;

		udelay(chip->chip_delay);

		chip->cmd_ctrl(mtd, NAND_CMD_STATUS, NAND_NCE | NAND_CLE);
		chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE);

		while (!(chip->read_byte(mtd) & NAND_STATUS_READY));
		return;

	case NAND_CMD_RNDOUT:
		/* No ready / busy check necessary */
		chip->cmd_ctrl(mtd, NAND_CMD_RNDOUTSTART, NAND_NCE | NAND_CLE);
		chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE);
		return;

	case NAND_CMD_READ0:
		chip->cmd_ctrl(mtd, NAND_CMD_READSTART, NAND_NCE | NAND_CLE);
		chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE);

	/* This applies to read commands */
	default:
		/*
		 * If we don't have access to the busy pin, we apply the given
		 * command delay
		 */
		if (!chip->dev_ready) {
			udelay(chip->chip_delay);
			return;
		}
	}

	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine.
	 */
	ndelay(100);

	nand_wait_ready(mtd);
}

/**
 * brcmnand_command - [DEFAULT] Send command to NAND device
 * @mtd:	MTD device structure
 * @command:	the command to be sent
 * @column:	the column address for this command, -1 if none
 * @page_addr:	the page address for this command, -1 if none
 *
 * Send command to NAND device. This function is used for small page
 * devices (256/512 Bytes per page)
 */
static void
brcmnand_command(struct mtd_info *mtd, unsigned int command, int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;
	int ctrl = NAND_CTRL_CLE;

	/* Invoke large page command function */
	if (mtd->writesize > 512) {
		brcmnand_command_lp(mtd, command, column, page_addr);
		return;
	}

	/*
	 * Write out the command to the device.
	 */
	if (command == NAND_CMD_SEQIN) {
		int readcmd;

		if (column >= mtd->writesize) {
			/* OOB area */
			column -= mtd->writesize;
			readcmd = NAND_CMD_READOOB;
		} else if (column < 256) {
			/* First 256 bytes --> READ0 */
			readcmd = NAND_CMD_READ0;
		} else {
			column -= 256;
			readcmd = NAND_CMD_READ1;
		}

		chip->cmd_ctrl(mtd, readcmd, ctrl);
	}

	chip->cmd_ctrl(mtd, command, ctrl);

	/*
	 * Address cycle, when necessary
	 */
	ctrl = NAND_CTRL_ALE;

	/* Serially input address */
	if (column != -1) {
		ctrl |= NAND_ALE_COL;

		/* Adjust columns for 16 bit buswidth */
		if (chip->options & NAND_BUSWIDTH_16)
			column >>= 1;

		chip->cmd_ctrl(mtd, column, ctrl);
	}

	if (page_addr != -1) {
		ctrl &= ~NAND_ALE_COL;
		ctrl |= NAND_ALE_ROW;

		chip->cmd_ctrl(mtd, page_addr, ctrl);
	}

	chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE);

	/*
	 * program and erase have their own busy handlers
	 * status and sequential in needs no delay
	 */
	switch (command) {

	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_STATUS:
		return;

	case NAND_CMD_RESET:
		if (chip->dev_ready)
			break;

		udelay(chip->chip_delay);

		chip->cmd_ctrl(mtd, NAND_CMD_STATUS, NAND_CTRL_CLE);

		chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE);

		while (!(chip->read_byte(mtd) & NAND_STATUS_READY));

		return;

		/* This applies to read commands */
	default:
		/*
		 * If we don't have access to the busy pin, we apply the given
		 * command delay
		 */
		if (!chip->dev_ready) {
			udelay(chip->chip_delay);
			return;
		}
	}

	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine.
	 */
	ndelay(100);

	nand_wait_ready(mtd);
}

/**
 * brcmnand_read_byte - [DEFAULT] read one byte from the chip
 * @mtd:	MTD device structure
 *
 * Default read function for 8bit bus width
 */
static uint8_t
brcmnand_read_byte(struct mtd_info *mtd)
{
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;
	register struct nand_chip *chip = mtd->priv;
	unsigned int val;

	osh = si_osh(sih);

	val = NFC_DREAD | NFC_CSA | NFC_START;
	W_REG(osh, &cc->nflashctrl, val);

	brcmnand_check_command_done();

	return readb(chip->IO_ADDR_R);
}

/**
 * brcmnand_write_byte - [DEFAULT] write one byte from the chip
 * @mtd:	MTD device structure
 *
 * Default write function for 8bit bus width
 */
static int
brcmnand_write_byte(struct mtd_info *mtd, u_char ch)
{
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;
	unsigned int val;

	osh = si_osh(sih);

	W_REG(osh, &cc->nflashdata, (unsigned int)ch);

	val = NFC_DWRITE | NFC_CSA | NFC_START;
	W_REG(osh, &cc->nflashctrl, val);

	brcmnand_check_command_done();

	return 0;
}

/**
 * brcmnand_read_buf - [DEFAULT] read data from chip into buf
 * @mtd:	MTD device structure
 * @buf:	data buffer
 * @len:	number of bytes to read
 *
 * Default read function for 8bit bus width
 */
static void
brcmnand_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	int count = 0;

	while (len > 0) {
		buf[count++] = brcmnand_read_byte(mtd);
		len--;
	}
}

/**
 * brcmnand_write_buf - [DEFAULT] write buffer to chip
 * @mtd:	MTD device structure
 * @buf:	data buffer
 * @len:	number of bytes to write
 *
 * Default write function for 8bit bus width
 */
static void
brcmnand_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	int count = 0;

	while (len > 0) {
		brcmnand_write_byte(mtd, buf[count++]);
		len--;
	}
}

/**
 * nand_verify_buf - [DEFAULT] Verify chip data against buffer
 * @mtd:	MTD device structure
 * @buf:	buffer containing the data to compare
 * @len:	number of bytes to compare
 *
 * Default verify function for 8bit buswith
 */
static int
brcmnand_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;
	uint8_t chbuf;

	for (i = 0; i < len; i++) {
		chbuf = chip->read_byte(mtd);
		if (buf[i] != chbuf) {
			return -EFAULT;
		}
	}

	return 0;
}

/**
 * brcmnand_devready - [DEFAULT] Check if nand flash device is ready
 * @mtd:	MTD device structure
 *
 * Return 0 if nand flash device is busy
 */
static int
brcmnand_devready(struct mtd_info *mtd)
{
	si_t *sih = brcmnand_info.sih;
	chipcregs_t *cc = brcmnand_info.cc;
	osl_t *osh;
	int status;

	osh = si_osh(sih);

	status = (R_REG(osh, &cc->nflashctrl) & NFC_RDYBUSY) ? 1 : 0;

	return status;
}

/**
 * brcmnand_select_chip - [DEFAULT] select chip
 * @mtd:	MTD device structure
 * @chip:	chip to be selected
 *
 * For BCM4706 just return because of only one chip is used
 */
static void
brcmnand_select_chip(struct mtd_info *mtd, int chip)
{
	return;
}

/**
 * brcmnand_init_nandchip - [DEFAULT] init mtd_info and nand_chip
 * @mtd:	MTD device structure
 * @chip:	chip to be selected
 *
 */
static int
brcmnand_init_nandchip(struct mtd_info *mtd, struct nand_chip *chip)
{
	chipcregs_t *cc = brcmnand_info.cc;
	int ret = 0;

	chip->cmdfunc = brcmnand_command;
	chip->read_byte = brcmnand_read_byte;
	chip->write_buf = brcmnand_write_buf;
	chip->read_buf = brcmnand_read_buf;
	chip->verify_buf = brcmnand_verify_buf;
	chip->select_chip = brcmnand_select_chip;
	chip->cmd_ctrl = brcmnand_hwcontrol;
	chip->dev_ready = brcmnand_devready;
	mtd->get_device = brcmnand_get_device_bcm4706;
	mtd->put_device = brcmnand_release_device_bcm4706;

	chip->numchips = 1;
	chip->chip_shift = 0;
	chip->chip_delay = 50;
	chip->priv = mtd;
	chip->options = NAND_USE_FLASH_BBT;

	chip->controller = &chip->hwcontrol;
	spin_lock_init(&chip->controller->lock);
	init_waitqueue_head(&chip->controller->wq);

	chip->IO_ADDR_W = (void __iomem *)&cc->nflashdata;
	chip->IO_ADDR_R = chip->IO_ADDR_W;

	/* BCM4706 only support software ECC mode */
	chip->ecc.mode = NAND_ECC_SOFT;
	chip->ecc.layout = NULL;

	mtd->name = "brcmnand";
	mtd->priv = chip;
	mtd->owner = THIS_MODULE;

	mtd->mlock = partitions_lock_init();
	if (!mtd->mlock)
		ret = -ENOMEM;

	return ret;
}

static int __init
brcmnand_mtd_init(void)
{
	int ret = 0;
	hndnand_t *info;
	struct pci_dev *dev = NULL;
	struct nand_chip *chip;
	struct mtd_info *mtd;
#ifdef CONFIG_MTD_PARTITIONS
	struct mtd_partition *parts;
	int i;
#endif

	list_for_each_entry(dev, &((pci_find_bus(0, 0))->devices), bus_list) {
		if ((dev != NULL) && (dev->device == CC_CORE_ID))
			break;
	}

	if (dev == NULL) {
		printk(KERN_ERR "brcmnand: chipcommon not found\n");
		return -ENODEV;
	}

	memset(&brcmnand_info, 0, sizeof(struct brcmnand_mtd));

	/* attach to the backplane */
	if (!(brcmnand_info.sih = si_kattach(SI_OSH))) {
		printk(KERN_ERR "brcmnand: error attaching to backplane\n");
		ret = -EIO;
		goto fail;
	}

	/* Map registers and flash base */
	if (!(brcmnand_info.cc = ioremap_nocache(
		pci_resource_start(dev, 0),
		pci_resource_len(dev, 0)))) {
		printk(KERN_ERR "brcmnand: error mapping registers\n");
		ret = -EIO;
		goto fail;
	}

	/* Initialize serial flash access */
	if (!(info = hndnand_init(brcmnand_info.sih ))) {
		printk(KERN_ERR "brcmnand: found no supported devices\n");
		ret = -ENODEV;
		goto fail;
	}

	if (CHIPID(brcmnand_info.sih->chip) == BCM4706_CHIP_ID) {
		mtd = &brcmnand_info.mtd;
		chip = &brcmnand_info.chip;

		if ((ret = brcmnand_init_nandchip(mtd, chip)) != 0) {
			printk(KERN_ERR "brcmnand_mtd_init: brcmnand_init_nandchip failed\n");
			goto fail;
		}

		if ((ret = nand_scan(mtd, chip->numchips)) != 0) {
			printk(KERN_ERR "brcmnand_mtd_init: nand_scan failed\n");
			goto fail;
		}

		goto init_partitions;
	}

	page_buffer = kmalloc(sizeof(struct nand_buffers), GFP_KERNEL);
	if (!page_buffer) {
		printk(KERN_ERR "brcmnand: cannot allocate memory for page buffer\n");
		return -ENOMEM;
	}
	memset(page_buffer, 0, sizeof(struct nand_buffers));

	chip = &brcmnand_info.chip;
	mtd = &brcmnand_info.mtd;
	brcmnand_info.nflash = info ;

	chip->ecc.mode = NAND_ECC_HW;

	chip->buffers = (struct nand_buffers *)page_buffer;
	chip->numchips = 1;
	chip->chip_shift = 0;
	chip->priv = mtd;
	chip->options |= NAND_USE_FLASH_BBT;
	/* At most 2GB is supported */
	chip->chipsize = (info->size >= (1 << 11)) ? (1 << 31) : (info->size << 20);
	brcmnand_info.level = info->ecclevel;

	/* Register with MTD */
	mtd->name = "brcmnand";
	mtd->priv = &brcmnand_info.chip;
	mtd->owner = THIS_MODULE;
	mtd->mlock = partitions_lock_init();
	spin_lock_init(&mtd_lock);
	if (!mtd->mlock) {
		ret = -ENOMEM;
		goto fail;
	}

	mtd->size = chip->chipsize;
	mtd->erasesize = info->blocksize;
	mtd->writesize = info->pagesize;
	/* 16B oob for 512B page, 64B for 2KB page, etc.. */
	mtd->oobsize = (info->pagesize >> 5);

	/* Calculate the address shift from the page size */
	chip->page_shift = ffs(mtd->writesize) - 1;
	/* Convert chipsize to number of pages per chip -1. */
	chip->pagemask = (chip->chipsize >> chip->page_shift) - 1;

	chip->bbt_erase_shift = chip->phys_erase_shift =
		ffs(mtd->erasesize) - 1;
	chip->chip_shift = ffs(chip->chipsize) - 1;

	/* Set the bad block position */
	chip->badblockpos = (mtd->writesize > 512) ?
		NAND_LARGE_BADBLOCK_POS : NAND_SMALL_BADBLOCK_POS;

	if (!chip->controller) {
		chip->controller = &chip->hwcontrol;
		spin_lock_init(&chip->controller->lock);
		init_waitqueue_head(&chip->controller->wq);
	}

	/* Preset the internal oob write buffer */
	memset(BRCMNAND_OOBBUF(chip->buffers), 0xff, mtd->oobsize);

	/* Set the internal oob buffer location, just after the page data */
	chip->oob_poi = BRCMNAND_OOBBUF(chip->buffers);

	/*
	 * If no default placement scheme is given, select an appropriate one
	 */
	if (!chip->ecc.layout) {
		switch (mtd->oobsize) {
		case 16:
			if (brcmnand_info.level== BRCMNAND_ECC_HAMMING)
				chip->ecc.layout = &brcmnand_oob_16;
			else
				chip->ecc.layout = &brcmnand_oob_bch4_512;
			break;
		case 64:
			if (brcmnand_info.level== BRCMNAND_ECC_HAMMING)
				chip->ecc.layout = &brcmnand_oob_64;
			else if (brcmnand_info.level== BRCMNAND_ECC_BCH_4) {
				if (mtd->writesize == 2048)
					chip->ecc.layout = &brcmnand_oob_bch4_2k;
				else {
					printk(KERN_ERR "Unsupported page size of %d\n",
						mtd->writesize);
					BUG();
				}
			}
			break;
		case 128:
			if (brcmnand_info.level== BRCMNAND_ECC_HAMMING)
				chip->ecc.layout = &brcmnand_oob_128;
			else {
				printk(KERN_ERR "Unsupported page size of %d\n",
					mtd->writesize);
				BUG();
			}
			break;
		default:
			printk(KERN_WARNING "No oob scheme defined for "
			       "oobsize %d\n", mtd->oobsize);
			BUG();
		}
	}

	if (!chip->write_page)
		chip->write_page = brcmnand_write_page;

	switch (chip->ecc.mode) {
	case NAND_ECC_HW:
		if (!chip->ecc.read_page)
			chip->ecc.read_page = brcmnand_read_page_hwecc;
		if (!chip->ecc.write_page)
			chip->ecc.write_page = brcmnand_write_page_hwecc;
		if (!chip->ecc.read_oob)
			chip->ecc.read_oob = brcmnand_read_oob_hwecc;
		if (!chip->ecc.write_oob)
			chip->ecc.write_oob = brcmnand_write_oob_hwecc;
		break;
	case NAND_ECC_SOFT:
		break;
	case NAND_ECC_NONE:
		break;
	default:
		printk(KERN_WARNING "Invalid NAND_ECC_MODE %d\n",
		       chip->ecc.mode);
		BUG();
		break;
	}

	/*
	 * The number of bytes available for a client to place data into
	 * the out of band area
	 */
	chip->ecc.layout->oobavail = 0;
	for (i = 0; chip->ecc.layout->oobfree[i].length; i++)
		chip->ecc.layout->oobavail +=
			chip->ecc.layout->oobfree[i].length;
	mtd->oobavail = chip->ecc.layout->oobavail;

	/*
	 * Set the number of read / write steps for one page
	 */
	chip->ecc.size = NFL_SECTOR_SIZE; /* Fixed for Broadcom controller. */
	mtd->oobsize = 16; /* Fixed for Hamming code or 4-bit BCH for now. */
	chip->ecc.bytes = brcmnand_eccbytes[brcmnand_info.level];
	chip->ecc.steps = mtd->writesize / chip->ecc.size;
	if (chip->ecc.steps * chip->ecc.size != mtd->writesize) {
		printk(KERN_WARNING "Invalid ecc parameters\n");
		BUG();
	}
	chip->ecc.total = chip->ecc.steps * chip->ecc.bytes;

	/*
	 * Allow subpage writes up to ecc.steps. Not possible for MLC
	 * FLASH.
	 */
	if (!(chip->options & NAND_NO_SUBPAGE_WRITE) &&
	    !(chip->cellinfo & NAND_CI_CELLTYPE_MSK)) {
		switch (chip->ecc.steps) {
		case 2:
			mtd->subpage_sft = 1;
			break;
		case 4:
			mtd->subpage_sft = 2;
			break;
		case 8:
			mtd->subpage_sft = 3;
			break;
		}
	}
	chip->subpagesize = mtd->writesize >> mtd->subpage_sft;

	/* Initialize state */
	chip->state = FL_READY;

	/* Invalidate the pagebuffer reference */
	chip->pagebuf = -1;

	if (!chip->block_markbad)
		chip->block_markbad = brcmnand_default_block_markbad;
	if (!chip->scan_bbt)
		chip->scan_bbt = brcmnand_default_bbt;
	if (!mtd->get_device)
		mtd->get_device = brcmnand_get_device;
	if (!mtd->put_device)
		mtd->put_device = brcmnand_release_device;

	mtd->type = MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH;
	mtd->erase = brcmnand_erase;
	mtd->point = NULL;
	mtd->unpoint = NULL;
	mtd->read = brcmnand_read;
	mtd->write = brcmnand_write;
	mtd->read_oob = brcmnand_read_oob;
	mtd->write_oob = brcmnand_write_oob;
	mtd->sync = brcmnand_sync;
	mtd->lock = NULL;
	mtd->unlock = NULL;
	mtd->suspend = brcmnand_suspend;
	mtd->resume = brcmnand_resume;
	mtd->block_isbad = brcmnand_block_isbad;
	mtd->block_markbad = brcmnand_block_markbad;

	/* propagate ecc.layout to mtd_info */
	mtd->ecclayout = chip->ecc.layout;

	ret = chip->scan_bbt(mtd);
	if (ret) {
		printk(KERN_ERR "brcmnand: scan_bbt failed\n");
		goto fail;
	}

init_partitions:
#ifdef CONFIG_MTD_PARTITIONS
	parts = init_brcmnand_mtd_partitions(mtd, mtd->size);
	if (!parts)
		goto fail;
	for (i = 0; parts[i].name; i++);
	ret = add_mtd_partitions(mtd, parts, i);
	if (ret) {
		printk(KERN_ERR "brcmnand: add_mtd failed\n");
		goto fail;
	}
	brcmnand_info.parts = parts;
#endif
	return 0;

fail:
	if (brcmnand_info.cc)
		iounmap((void *) brcmnand_info.cc);
	if (brcmnand_info.sih)
		si_detach(brcmnand_info.sih);
	if (page_buffer)
		kfree(page_buffer);
	return ret;
}

static void __exit
brcmnand_mtd_exit(void)
{
#ifdef CONFIG_MTD_PARTITIONS
	del_mtd_partitions(&brcmnand_info.mtd);
#else
	del_mtd_device(&brcmnand_info.mtd);
#endif
	iounmap((void *) brcmnand_info.cc);
	si_detach(brcmnand_info.sih);
}

module_init(brcmnand_mtd_init);
module_exit(brcmnand_mtd_exit);
