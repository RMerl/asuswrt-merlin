/*
 * Broadcom NAND flash controller interface
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
 * $Id $
 */


#ifndef _BRCMNAND_PRIV_H_
#define _BRCMNAND_PRIV_H_

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
#include <linux/autoconf.h>
#endif

#include <linux/vmalloc.h>

#include <linux/mtd/nand.h>

#define BRCMNAND_malloc(size) vmalloc(size)
#define BRCMNAND_free(addr) vfree(addr)

typedef enum
{
	BRCMNAND_ECC_DISABLE    = 0u,
	BRCMNAND_ECC_BCH_1      = 1u,
	BRCMNAND_ECC_BCH_2      = 2u,
	BRCMNAND_ECC_BCH_3      = 3u,
	BRCMNAND_ECC_BCH_4      = 4u,
	BRCMNAND_ECC_BCH_5      = 5u,
	BRCMNAND_ECC_BCH_6      = 6u,
	BRCMNAND_ECC_BCH_7      = 7u,
	BRCMNAND_ECC_BCH_8      = 8u,
	BRCMNAND_ECC_BCH_9      = 9u,
	BRCMNAND_ECC_BCH_10     = 10u,
	BRCMNAND_ECC_BCH_11     = 11u,
	BRCMNAND_ECC_BCH_12     = 12u,
	BRCMNAND_ECC_RESVD_1    = 13u,
	BRCMNAND_ECC_RESVD_2    = 14u,
	BRCMNAND_ECC_HAMMING    = 15u
} brcmnand_ecc_level_t;

struct brcmnand_mtd {
	si_t *sih;
	chipcregs_t *cc;
	struct mtd_info mtd;
	struct nand_chip chip;
	brcmnand_ecc_level_t level;
	hndnand_t * nflash;
#ifdef CONFIG_MTD_PARTITIONS
	struct mtd_partition *parts;
#endif
};


/**
 * brcmnand_scan - [BrcmNAND Interface] Scan for the BrcmNAND device
 * @param mtd       MTD device structure
 * @param maxchips  Number of chips to scan for
 *
 * This fills out all the not initialized function pointers
 * with the defaults.
 * The flash ID is read and the mtd/chip structures are
 * filled with the appropriate values.
 *
 * THT: For now, maxchips should always be 1.
 */
extern int brcmnand_scan(struct mtd_info *mtd, int maxchips);

/**
 * brcmnand_release - [BrcmNAND Interface] Free resources held by the
 *  BrcmNAND device
 * @param mtd       MTD device structure
 */
extern void brcmnand_release(struct mtd_info *mtd);

extern int brcmnand_scan_bbt(struct mtd_info *mtd, struct nand_bbt_descr *bd);
extern int brcmnand_default_bbt(struct mtd_info *mtd);
extern int brcmnand_isbad_bbt(struct mtd_info *mtd, loff_t offs, int allowbbt);

extern int brcmnand_update_bbt(struct mtd_info *mtd, loff_t offs);

extern void* get_brcmnand_handle(void);

extern void print_oobbuf(const unsigned char* buf, int len);
extern void print_databuf(const unsigned char* buf, int len);

#ifdef CONFIG_MTD_BRCMNAND_CORRECTABLE_ERR_HANDLING
extern int brcmnand_cet_update(struct mtd_info *mtd, loff_t from, int *status);
extern int brcmnand_cet_prepare_reboot(struct mtd_info *mtd);
extern int brcmnand_cet_erasecallback(struct mtd_info *mtd, u_int32_t addr);
extern int brcmnand_create_cet(struct mtd_info *mtd);
#endif /* CONFIG_MTD_BRCMNAND_CORRECTABLE_ERR_HANDLING */

#endif /* _BRCMNAND_PRIV_H_ */
