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
 * $Id: nflash.c 411518 2013-07-09 09:02:59Z $
 */

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <hndsoc.h>
#include <sbhndcpu.h>
#include <sbchipc.h>
#include <bcmdevs.h>
#include <nflash.h>
#include <hndpmu.h>

#ifdef BCMDBG
#define	NFL_MSG(args)	printf args
#else
#define	NFL_MSG(args)
#endif	/* BCMDBG */

#define NF_RETRIES	1000000

#define NF_SMALL_BADBLOCK_POS	5
#define NF_LARGE_BADBLOCK_POS	0

/* Private global state */
static hndnand_t nflash;

/* Private variables used for BCM4706 only */
static uint32 nflash_col_mask;
static uint32 nflash_row_shift;

/* Prototype */
static int nflash_poll(si_t *sih, chipcregs_t *cc);
static int nflash_readst(si_t *sih, chipcregs_t *cc, uint8 *status);
static int nflash_readoob(si_t *sih, chipcregs_t *cc, uint64 offset, uint len, uchar *buf);
static int nflash_writeoob(si_t *sih, chipcregs_t *cc, uint64 offset, uint len, uchar *buf);

hndnand_t *nflash_init(si_t *sih);
static void nflash_enable(hndnand_t *nfl, int enable);
static int nflash_read(hndnand_t *nfl, uint64 offset, uint len, uchar *buf);
static int nflash_write(hndnand_t *nfl, uint64 offset, uint len, const uchar *buf);
static int nflash_erase(hndnand_t *nfl, uint64 offset);
static int nflash_checkbadb(hndnand_t *nfl, uint64 offset);
static int nflash_mark_badb(hndnand_t *nfl, uint64 offset);


static void
nflash_enable(hndnand_t *nfl, int enable)
{
	si_t *sih = nfl->sih;

	if (sih->ccrev == 38) {
		/* BCM5357 NAND boot */
		if ((sih->chipst & (1 << 4)) != 0)
			return;

		if (enable)
			si_pmu_chipcontrol(sih, 1, CCTRL5357_NFLASH, CCTRL5357_NFLASH);
		else
			si_pmu_chipcontrol(sih, 1, CCTRL5357_NFLASH, 0);
	}
}

/* Issue a nand flash control command; this is used for BCM4706 */
static INLINE int
nflash_ctrlcmd(osl_t *osh, chipcregs_t *cc, uint ctrlcode)
{
	int cnt = 0;

	W_REG(osh, &cc->nflashctrl, ctrlcode | NFC_START);

	while ((R_REG(osh, &cc->nflashctrl) & NFC_START) != 0) {
		if (++cnt > NF_RETRIES) {
			printf("nflash_ctrlcmd: not ready\n");
			return -1;
		}
	}

	return 0;
}

/* Issue a nand flash command */
static INLINE void
nflash_cmd(osl_t *osh, chipcregs_t *cc, uint opcode)
{
	W_REG(osh, &cc->nand_cmd_start, opcode);
	/* read after write to flush the command */
	R_REG(osh, &cc->nand_cmd_start);
}

static bool firsttime = TRUE;

static char *
nflash_check_id(uint8 *id)
{
	char *name = NULL;

	switch (id[0]) {
	case NFL_VENDOR_AMD:
		name = "AMD";
		break;
	case NFL_VENDOR_NUMONYX:
		name = "Numonyx";
		break;
	case NFL_VENDOR_MICRON:
		name = "Micron";
		break;
	case NFL_VENDOR_TOSHIBA:
		name = "Toshiba";
		break;
	case NFL_VENDOR_HYNIX:
		name = "Hynix";
		break;
	case NFL_VENDOR_SAMSUNG:
		name = "Samsung";
		break;
	case NFL_VENDOR_ESMT:
		name = "Esmt";
		break;
	case NFL_VENDOR_MXIC:
		name = "Mxic";
		break;
	case NFL_VENDOR_ZENTEL_ESMT:
		name = "Zentel/Esmt";
		break;
	case NFL_VENDOR_WINBOND:
		name = "Winbond";
		break;
	default:
		//printf("No NAND flash type found\n");
		name = "";
		break;
	}

	return name;
}

/* Initialize nand flash access */
hndnand_t *
nflash_init(si_t *sih)
{
	chipcregs_t *cc;
	uint32 id, id2;
	char *name = "";
	osl_t *osh;
	int i;
	uint32 ncf, val;
	uint32 acc_control;

	ASSERT(sih);

	if ((cc = (chipcregs_t *)si_setcoreidx(sih, SI_CC_IDX)) == NULL)
		return NULL;

	/* Only support chipcommon revision == 38 and BCM4706 for now */
	if ((CHIPID(sih->chip) != BCM4706_CHIP_ID) && (sih->ccrev != 38))
		return NULL;

	/* Check if nand flash is mounted */
	if ((sih->cccaps & CC_CAP_NFLASH) != CC_CAP_NFLASH)
		return NULL;

	if (!firsttime && nflash.size)
		return &nflash;

	osh = si_osh(sih);
	bzero(&nflash, sizeof(nflash));

	nflash.sih = sih;
	nflash.core = (void *)cc;
	nflash.enable = nflash_enable;
	nflash.read = nflash_read;
	nflash.write = nflash_write;
	nflash.erase = nflash_erase;
	nflash.checkbadb = nflash_checkbadb;
	nflash.markbadb = nflash_mark_badb;

	if (CHIPID(sih->chip) == BCM4706_CHIP_ID) {
	    uint32 cpu_freq, clk;
		uint32 w0, w1, w2, w3, w4;
		uint32 ctrlcode, val;
		uint32 totbits = 0, colbits = 0, rowbits;
		uint32 colbsize, rowbsize;
		uint32 oobsz_per_sector, plane_sz, plane_num;

		/* Enable nand flash interface of BCM4706 (usign CS1) */
		val = R_REG(osh, &cc->eci.flashconf.flashstrconfig) | FLSTRCF4706_NF1;
		W_REG(osh, &cc->eci.flashconf.flashstrconfig, val);

		/* Configure nand flash bus timing */
		if (R_REG(osh, &cc->chipstatus) & CST4706_PKG_OPTION) {
			cpu_freq = (400000000 / 4);
		} else {
			/* Get N divider to determine CPU clock */
			W_REG(osh, &cc->pllcontrol_addr, 4);
			val = (R_REG(osh, &cc->pllcontrol_data) & 0xfff) >> 3;

			/* Fixed reference clock 25MHz and m = 2 */
			cpu_freq = (val * 25000000 / 2) / 4;
		}

		clk = cpu_freq / 1000000;

		w0 = NFLASH_T_WP;
		w1 = NFLASH_T_RR;
		w2 = NFLASH_T_CS - NFLASH_T_WP;
		w3 = NFLASH_T_WH;
		w4 = NFLASH_T_WB;

		w0 = nflash_ns_to_cycle(w0, clk);
		w1 = nflash_ns_to_cycle(w1, clk);
		w2 = nflash_ns_to_cycle(w2, clk);
		w3 = nflash_ns_to_cycle(w3, clk);
		w4 = nflash_ns_to_cycle(w4, clk) - 1;

		val = (w4 << NFLASH_WAITCOUNT_W4_SHIFT) | (w3 << NFLASH_WAITCOUNT_W3_SHIFT)
				| (w2 << NFLASH_WAITCOUNT_W2_SHIFT)
				| (w1 << NFLASH_WAITCOUNT_W1_SHIFT)	| w0;
		W_REG(osh, &cc->nflashwaitcnt0, val);

		/* Read nand flash ID */
		ctrlcode = NFC_CSA | NFC_SPECADDR | NFC_CMD1W | NFC_CMD0 | NFCTRL_ID;

		if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
			return NULL;

		for (i = 0; i < 5; i++) {
			if (i <= 4)
				ctrlcode = (NFC_CSA | NFC_1BYTE | NFC_DREAD);
			else
				ctrlcode = (NFC_1BYTE | NFC_DREAD);

			if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
				return NULL;
			else
				nflash.id[i] = R_REG(osh, &cc->nflashdata) & 0xff;
		}

		name = nflash_check_id(nflash.id);
		if (name == NULL) {
			/* Disable nand flash interface of BCM4706 (usign CS1) */
			val = R_REG(osh, &cc->eci.flashconf.flashstrconfig) & ~FLSTRCF4706_NF1;
			W_REG(osh, &cc->eci.flashconf.flashstrconfig, val);

			return NULL;
		}
		nflash.type = nflash.id[0];

		/* In unit of bytes */
		nflash.pagesize = 1024 << (nflash.id[3] & 0x3);
		nflash.blocksize = (64 * 1024) << ((nflash.id[3] >> 4) & 0x3);

		oobsz_per_sector = (8 << ((nflash.id[3] >> 2) & 0x1));
		nflash.oobsize = oobsz_per_sector * (nflash.pagesize / NFL_SECTOR_SIZE);

		/* In unit of MBytes */
		plane_sz = (8 << ((nflash.id[4] >> 4) & 0x7));
		plane_num = (1 << ((nflash.id[4] >> 2) & 0x3));
		nflash.size = plane_sz * plane_num;

		for (i = 0; i < 32; i++) {
			if ((0x1 << i) == nflash.size) {
				totbits = i + 20;
				break;
			}
		}

		if (totbits == 0) {
#ifdef BCMDBG
			NFL_MSG(("nflash_init: failed to find nflash total bits\n"));
#endif
			return NULL;
		}

		for (i = 0; i < 32; i++) {
			if ((0x1 << i) == nflash.pagesize) {
				colbits = i + 1;	/* including spare oob bytes */
				break;
			}
		}

		if (colbits == 0) {
#ifdef BCMDBG
			NFL_MSG(("nflash_init: failed to find nflash column address bits\n"));
#endif
			return NULL;
		}

		nflash_col_mask = (0x1 << colbits) - 1;
		nflash_row_shift = colbits;

		colbsize = (colbits + 7) / 8;

		rowbits = totbits - colbits + 1;
		rowbsize = (rowbits + 7) / 8;

		val = ((rowbsize - 1) << NFCF_ROWSZ_SHIFT) | ((colbsize - 1) << NFCF_COLSZ_SHIFT)
				| NFCF_DS_8 | NFCF_WE;

		W_REG(osh, &cc->nflashconf, val);
	} else {
		nflash_enable(&nflash, 1);
		nflash_cmd(osh, cc, NCMD_ID_RD);
		if (nflash_poll(sih, cc) < 0) {
			nflash_enable(&nflash, 0);
			return NULL;
		}
		nflash_enable(&nflash, 0);
		id = R_REG(osh, &cc->nand_devid);
		id2 = R_REG(osh, &cc->nand_devid_x);
		for (i = 0; i < 5; i++) {
			if (i < 4)
				nflash.id[i] = (id >> (8*i)) & 0xff;
			else
				nflash.id[i] = id2 & 0xff;
		}

		name = nflash_check_id(nflash.id);
		if (name == NULL)
			return NULL;
		nflash.type = nflash.id[0];

		ncf = R_REG(osh, &cc->nand_config);
		/*  Page size (# of bytes) */
		val = (ncf & NCF_PAGE_SIZE_MASK) >> NCF_PAGE_SIZE_SHIFT;
		switch (val) {
		case 0:
			nflash.pagesize = 512;
			break;
		case 1:
			nflash.pagesize = (1 << 10) * 2;
			break;
			case 2:
			nflash.pagesize = (1 << 10) * 4;
			break;
		case 3:
			nflash.pagesize = (1 << 10) * 8;
			break;
		}
		/* Block size (# of bytes) */
		val = (ncf & NCF_BLOCK_SIZE_MASK) >> NCF_BLOCK_SIZE_SHIFT;
		switch (val) {
		case 0:
			nflash.blocksize = (1 << 10) * 16;
			break;
		case 1:
			nflash.blocksize = (1 << 10) * 128;
			break;
		case 2:
			nflash.blocksize = (1 << 10) * 8;
			break;
		case 3:
			nflash.blocksize = (1 << 10) * 512;
			break;
		case 4:
			nflash.blocksize = (1 << 10) * 256;
			break;
		default:
			printf("Unknown block size\n");
			return NULL;
		}
		/* NAND flash size in MBytes */
		val = (ncf & NCF_DEVICE_SIZE_MASK) >> NCF_DEVICE_SIZE_SHIFT;
		if (val == 0) {
			printf("Unknown flash size\n");
			return NULL;
		}
		nflash.size = (1 << (val - 1)) * 8;

		/* More attribues for ECC functions */
		acc_control = R_REG(osh, &cc->nand_acc_control);
		nflash.ecclevel = (acc_control & NAC_ECC_LEVEL_MASK) >> NAC_ECC_LEVEL_SHIFT;
		nflash.ecclevel0 = (acc_control & NAC_ECC_LEVEL0_MASK) >> NAC_ECC_LEVEL0_SHIFT;
		/* make sure that block-0 and block-n use the same ECC level */
		if (nflash.ecclevel != nflash.ecclevel0) {
			acc_control &= ~(NAC_ECC_LEVEL_MASK | NAC_ECC_LEVEL0_MASK);
			acc_control |=
				(nflash.ecclevel0 << NAC_ECC_LEVEL0_SHIFT) |
				(nflash.ecclevel0 << NAC_ECC_LEVEL_SHIFT);
			W_REG(osh, &cc->nand_acc_control, acc_control);
			nflash.ecclevel = nflash.ecclevel0;
		}
		nflash.phybase = SI_FLASH1;
	}

	nflash.numblocks = (nflash.size * (1 << 10)) / (nflash.blocksize >> 10);
	if (firsttime)
		printf("Found a %s NAND flash with %uB pages or %dKB blocks; total size %dMB\n",
		       name, nflash.pagesize, (nflash.blocksize >> 10), nflash.size);
	firsttime = FALSE;
	return nflash.size ? &nflash : NULL;
}

/* Read len bytes starting at offset into buf. Returns number of bytes read. */
static int
nflash_read(hndnand_t *nfl, uint64 offset, uint len, uchar *buf)
{
	si_t *sih = nfl->sih;
	chipcregs_t *cc = (chipcregs_t *)nfl->core;
	uint32 mask;
	osl_t *osh;
	int i;
	uint32 *to;
	uint32 val;
	uint res;

	ASSERT(sih);
	mask = NFL_SECTOR_SIZE - 1;
	if ((offset & mask) != 0 || (len & mask) != 0)
		return 0;
	if ((((offset + len) >> 20) > nflash.size) ||
	    ((((offset + len) >> 20) == nflash.size) &&
	     (((offset + len) & ((1 << 20) - 1)) != 0)))
		return 0;
	osh = si_osh(sih);
	to = (uint32 *)buf;
	res = len;

	if (CHIPID(sih->chip) == BCM4706_CHIP_ID) {
		uint32 page_addr, page_offset;
		uint32 ctrlcode;

		while (res > 0) {
			page_offset = offset & (nflash.pagesize - 1);
			page_addr = (offset & ~(nflash.pagesize - 1)) * 2;
			page_addr += page_offset;

			W_REG(osh, &cc->nflashcoladdr, page_addr & nflash_col_mask);
			W_REG(osh, &cc->nflashrowaddr, page_addr >> nflash_row_shift);

			ctrlcode = NFC_CSA | NFC_CMD1W | NFC_ROW | NFC_COL | NFC_CMD0 | NFCTRL_READ;

			if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
				break;

			if (nflash_poll(sih, cc) < 0)
				break;

			for (i = 0; i < NFL_SECTOR_SIZE; i += 4, to++) {
				if (i < NFL_SECTOR_SIZE - 4)
					ctrlcode = (NFC_CSA | NFC_4BYTES | NFC_DREAD);
				else
					ctrlcode = (NFC_4BYTES | NFC_DREAD);

				if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
					return (len - res);

				*to = R_REG(osh, &cc->nflashdata);
			}

			res -= NFL_SECTOR_SIZE;
			offset += NFL_SECTOR_SIZE;
		}
	} else {
		nflash_enable(nfl, 1);
		while (res > 0) {
			W_REG(osh, &cc->nand_cmd_addr, offset);
			nflash_cmd(osh, cc, NCMD_PAGE_RD);
			if (nflash_poll(sih, cc) < 0)
				break;
			if (((val = R_REG(osh, &cc->nand_intfc_status)) & NIST_CACHE_VALID) == 0)
				break;
			W_REG(osh, &cc->nand_cache_addr, 0);
			for (i = 0; i < NFL_SECTOR_SIZE; i += 4, to++) {
				*to = R_REG(osh, &cc->nand_cache_data);
			}

			res -= NFL_SECTOR_SIZE;
			offset += NFL_SECTOR_SIZE;
		}
		nflash_enable(nfl, 0);
	}

	return (len - res);
}

/* Poll for command completion. Returns zero when complete. */
static int
nflash_poll(si_t *sih, chipcregs_t *cc)
{
	osl_t *osh;
	int i;
	uint32 pollmask;

	ASSERT(sih);
	osh = si_osh(sih);

	if (CHIPID(sih->chip) == BCM4706_CHIP_ID) {
		for (i = 0; i < NF_RETRIES; i++) {
			if ((R_REG(osh, &cc->nflashctrl) & NFC_RDYBUSY) == NFC_RDYBUSY) {
				if ((R_REG(osh, &cc->nflashctrl) & NFC_ERROR) == NFC_ERROR) {
					printf("nflash_poll: error occurred\n");
					return -1;
				}
				else
					return 0;
			}
		}
	} else {
		pollmask = NIST_CTRL_READY|NIST_FLASH_READY;
		for (i = 0; i < NF_RETRIES; i++) {
			if ((R_REG(osh, &cc->nand_intfc_status) & pollmask) == pollmask) {
				return 0;
			}
		}
	}

	printf("nflash_poll: not ready\n");
	return -1;
}

/* Write len bytes starting at offset into buf. Returns number of bytes
 * written.
 */
static int
nflash_write(hndnand_t *nfl, uint64 offset, uint len, const uchar *buf)
{
	si_t *sih = nfl->sih;
	chipcregs_t *cc = (chipcregs_t *)nfl->core;
	uint32 mask;
	osl_t *osh;
	int i;
	uint32 *from;
	uint res;
	uint32 reg;
	int ret = 0;
	uint8 status;

	ASSERT(sih);
	mask = nflash.pagesize - 1;
	/* Check offset and length */
	if ((offset & mask) != 0 || (len & mask) != 0)
		return 0;
	if ((((offset + len) >> 20) > nflash.size) ||
	    ((((offset + len) >> 20) == nflash.size) &&
	     (((offset + len) & ((1 << 20) - 1)) != 0)))
		return 0;
	osh = si_osh(sih);

	from = (uint32 *)buf;
	res = len;

	if (CHIPID(sih->chip) == BCM4706_CHIP_ID) {
		uint32 page_addr;
		uint32 ctrlcode;

		while (res > 0) {
			page_addr = (offset & ~(nflash.pagesize - 1)) * 2;

			W_REG(osh, &cc->nflashcoladdr, page_addr & nflash_col_mask);
			W_REG(osh, &cc->nflashrowaddr, page_addr >> nflash_row_shift);

			ctrlcode = NFC_CSA | NFC_ROW | NFC_COL | NFC_CMD0 | NFCTRL_PAGEPROG;

			if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
				return -1;

			for (i = 0; i < nflash.pagesize; i += 4, from++) {
				W_REG(osh, &cc->nflashdata, *from);

				if (i < nflash.pagesize - 4)
					ctrlcode = (NFC_CSA | NFC_4BYTES | NFC_DWRITE);
				else
					ctrlcode = (NFC_4BYTES | NFC_DWRITE);

				if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
					return (len - res);
			}

			if (nflash_ctrlcmd(osh, cc, (NFC_CMD0 | NFCTRL_PROGSTART)) != 0)
				return -1;

			if (nflash_poll(sih, cc) < 0)
				return -1;

			if (nflash_readst(sih, cc, &status) != 0)
				return -1;

			if (status & 1) {
				printf("nflash_write: failed with status 0x%02x\n", status);
				return -1;
			}

			res -= nflash.pagesize;
			offset += nflash.pagesize;
		}
	} else {
		nflash_enable(nfl, 1);
		/* disable partial page enable */
		reg = R_REG(osh, &cc->nand_acc_control);
		reg &= ~NAC_PARTIAL_PAGE_EN;
		W_REG(osh, &cc->nand_acc_control, reg);

		while (res > 0) {
			W_REG(osh, &cc->nand_cache_addr, 0);
			for (i = 0; i < nflash.pagesize; i += 4, from++) {
				if (i % 512 == 0)
					W_REG(osh, &cc->nand_cmd_addr, i);
				W_REG(osh, &cc->nand_cache_data, *from);
			}
			W_REG(osh, &cc->nand_cmd_addr, offset + nflash.pagesize - 512);
			nflash_cmd(osh, cc, NCMD_PAGE_PROG);
			if (nflash_poll(sih, cc) < 0)
				break;

			/* Check status */
			W_REG(osh, &cc->nand_cmd_start, NCMD_STATUS_RD);
			if (nflash_poll(sih, cc) < 0)
				break;
			status = R_REG(osh, &cc->nand_intfc_status) & NIST_STATUS;
			if (status & 1) {
				ret = -1;
				break;
			}
			res -= nflash.pagesize;
			offset += nflash.pagesize;
		}

		nflash_enable(nfl, 0);
		if (ret)
			return ret;
	}

	return (len - res);
}

/* Erase a region. Returns number of bytes scheduled for erasure.
 * Caller should poll for completion.
 */
static int
nflash_erase(hndnand_t *nfl, uint64 offset)
{
	si_t *sih = nfl->sih;
	chipcregs_t *cc = (chipcregs_t *)nfl->core;
	osl_t *osh;
	int ret = 0;
	uint8 status = 0;

	ASSERT(sih);

	osh = si_osh(sih);
	if ((offset >> 20) >= nflash.size)
		return -1;
	if ((offset & (nflash.blocksize - 1)) != 0) {
		return -1;
	}

	if (CHIPID(sih->chip) == BCM4706_CHIP_ID) {
		uint32 ctrlcode;
		uint32 row_addr;

		row_addr = (offset & ~(nflash.blocksize - 1)) * 2;

		W_REG(osh, &cc->nflashrowaddr, row_addr >> nflash_row_shift);

		ctrlcode = (NFC_CMD1W | NFC_ROW | NFC_CMD0 | NFCTRL_ERASE);

		if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
			return -1;

		if (nflash_poll(sih, cc) < 0)
			return -1;

		if (nflash_readst(sih, cc, &status) != 0)
			return -1;

		if (status & 1) {
			printf("nflash_erase: failed with status 0x%02x\n", status);
			return -1;
		}
	} else {
		nflash_enable(nfl, 1);
		W_REG(osh, &cc->nand_cmd_addr, offset);
		nflash_cmd(osh, cc, NCMD_BLOCK_ERASE);
		if (nflash_poll(sih, cc) < 0) {
			ret = -1;
			goto err;
		}
		/* Check status */
		W_REG(osh, &cc->nand_cmd_start, NCMD_STATUS_RD);
		if (nflash_poll(sih, cc) < 0) {
			ret = -1;
			goto err;
		}
		status = R_REG(osh, &cc->nand_intfc_status) & NIST_STATUS;
		if (status & 1)
			ret = -1;
err:
		nflash_enable(nfl, 0);
	}

	return ret;
}

static int
nflash_checkbadb(hndnand_t *nfl, uint64 offset)
{
	si_t *sih = nfl->sih;
	chipcregs_t *cc = (chipcregs_t *)nfl->core;
	osl_t *osh;
	int i;
	uint off;
	uint32 nand_intfc_status;
	int ret = 0;

	ASSERT(sih);

	osh = si_osh(sih);
	if ((offset >> 20) >= nflash.size)
		return -1;
	if ((offset & (nflash.blocksize - 1)) != 0) {
		return -1;
	}

	if (CHIPID(sih->chip) == BCM4706_CHIP_ID) {
		uint8 oob_buf1[8], oob_buf2[8];
		uint sec_page_offset;
		int status1, status2, badblk_pos;

		status1 = nflash_readoob(sih, cc, offset, sizeof(oob_buf1), oob_buf1);

		sec_page_offset = offset + nflash.pagesize;
		status2 = nflash_readoob(sih, cc, sec_page_offset, sizeof(oob_buf2), oob_buf2);

		if (status1 <= 0 || status2 <= 0)
			return -1;

		/* Set the bad block position */
		if (nflash.pagesize > 512)
			badblk_pos = NF_LARGE_BADBLOCK_POS;
		else
			badblk_pos = NF_SMALL_BADBLOCK_POS;

		if (oob_buf1[badblk_pos] != 0xff || oob_buf2[badblk_pos] != 0xff)
			return -1;
		else
			return 0;
	} else {
		nflash_enable(nfl, 1);
		for (i = 0; i < 2; i++) {
			off = offset + (nflash.pagesize * i);
			W_REG(osh, &cc->nand_cmd_addr, off);
			nflash_cmd(osh, cc, NCMD_SPARE_RD);
			if (nflash_poll(sih, cc) < 0) {
				ret = -1;
				goto err;
			}
			nand_intfc_status = R_REG(osh, &cc->nand_intfc_status) & NIST_SPARE_VALID;
			if (nand_intfc_status != NIST_SPARE_VALID) {
				ret = -1;
				goto err;
			}
			if ((R_REG(osh, &cc->nand_spare_rd0) & 0xff) != 0xff) {
				ret = -1;
				goto err;
			}
		}
err:
		nflash_enable(nfl, 0);
		return ret;
	}
}

static int
nflash_mark_badb(hndnand_t *nfl, uint64 offset)
{
	si_t *sih = nfl->sih;
	chipcregs_t *cc = (chipcregs_t *)nfl->core;
	osl_t *osh;
	uint off;
	int i, ret = 0;
	uint32 reg;

	ASSERT(sih);

	osh = si_osh(sih);
	if ((offset >> 20) >= nflash.size)
		return -1;
	if ((offset & (nflash.blocksize - 1)) != 0) {
		return -1;
	}

	if (CHIPID(sih->chip) == BCM4706_CHIP_ID) {
		unsigned char oob_buf[128];

		/* Force OOB write even nflash_erase return failure */
		nflash_erase(nfl, offset);

		memset((void *)oob_buf, 0, nflash.oobsize);

		if (nflash_writeoob(sih, cc, offset, nflash.oobsize, oob_buf) < 0)
			return -1;

		if (nflash_writeoob(sih, cc, offset + nflash.pagesize, nflash.oobsize, oob_buf) < 0)
			return -1;
	} else {
		nflash_enable(nfl, 1);
		/* Erase block */
		W_REG(osh, &cc->nand_cmd_addr, offset);
		nflash_cmd(osh, cc, NCMD_BLOCK_ERASE);
		if (nflash_poll(sih, cc) < 0) {
			ret = -1;
			goto err;
		}

		/*
		 * Enable partial page programming and disable ECC checkbit generation
		 * for PROGRAM_SPARE_AREA
		 */
		reg = R_REG(osh, &cc->nand_acc_control);
		reg |= NAC_PARTIAL_PAGE_EN;
		reg &= ~NAC_WR_ECC_EN;
		W_REG(osh, &cc->nand_acc_control, reg);

		for (i = 0; i < 2; i++) {
			off = offset + (nflash.pagesize * i);
			W_REG(osh, &cc->nand_cmd_addr, off);

			W_REG(osh, &cc->nand_spare_wr0, 0);
			W_REG(osh, &cc->nand_spare_wr4, 0);
			W_REG(osh, &cc->nand_spare_wr8, 0);
			W_REG(osh, &cc->nand_spare_wr12, 0);

			nflash_cmd(osh, cc, NCMD_SPARE_PROG);
			if (nflash_poll(sih, cc) < 0) {
				ret = -1;
				goto err;
			}
		}
err:
		/* Restore the default value for spare area write registers */
		W_REG(osh, &cc->nand_spare_wr0, 0xffffffff);
		W_REG(osh, &cc->nand_spare_wr4, 0xffffffff);
		W_REG(osh, &cc->nand_spare_wr8, 0xffffffff);
		W_REG(osh, &cc->nand_spare_wr12, 0xffffffff);

		/*
		 * Disable partial page programming and enable ECC checkbit generation
		 * for PROGRAM_SPARE_AREA
		 */
		reg = R_REG(osh, &cc->nand_acc_control);
		reg &= ~NAC_PARTIAL_PAGE_EN;
		reg |= NAC_WR_ECC_EN;
		W_REG(osh, &cc->nand_acc_control, reg);

		nflash_enable(nfl, 0);
	}
	return ret;
}

static int
nflash_readst(si_t *sih, chipcregs_t *cc, uint8 *status)
{
	osl_t *osh;
	int ret = 0;

	ASSERT(sih);

	osh = si_osh(sih);

	if (CHIPID(sih->chip) == BCM4706_CHIP_ID) {
		uint32 ctrlcode;

		ctrlcode = (NFC_CSA | NFC_CMD0 | NFCTRL_STATUS);

		if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
			return -1;

		ctrlcode = (NFC_1BYTE | NFC_DREAD);

		if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
			return -1;

		*status = (uint8)(R_REG(osh, &cc->nflashdata) & 0xff);

		return 0;
	} else {
		nflash_enable(&nflash, 1);
		W_REG(osh, &cc->nand_cmd_start, NCMD_STATUS_RD);

		if (nflash_poll(sih, cc) < 0)
			ret = -1;

		else
			*status = (uint8)(R_REG(osh, &cc->nand_intfc_status) & NIST_STATUS);

		nflash_enable(&nflash, 0);
		return ret;
	}
}

/* To read len bytes of oob data in the page specified in the page address offset */
static int
nflash_readoob(si_t *sih, chipcregs_t *cc, uint64 offset, uint len, uchar *buf)
{
	uint32 mask;
	osl_t *osh;
	int i;
	uint32 *to;
	uint res;

	ASSERT(sih);

	mask = nflash.pagesize - 1;
	if ((offset & mask) != 0 || (len > nflash.oobsize) || (len & 0x3) != 0)
		return -1;

	osh = si_osh(sih);
	to = (uint32 *)buf;
	res = 0;

	if (CHIPID(sih->chip) == BCM4706_CHIP_ID) {
		uint32 page_addr, page_offset;
		uint32 ctrlcode;

		page_addr = (offset & ~(nflash.pagesize - 1)) * 2;
		page_offset = nflash.pagesize;
		page_addr += page_offset;

		W_REG(osh, &cc->nflashcoladdr, page_addr & nflash_col_mask);
		W_REG(osh, &cc->nflashrowaddr, page_addr >> nflash_row_shift);

		ctrlcode = (NFC_CSA | NFC_CMD1W | NFC_ROW | NFC_COL | NFC_CMD0 | NFCTRL_READ);

		if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
			return -1;

		if (nflash_poll(sih, cc) < 0)
			return -1;

		for (i = 0; i < len; i += 4, to++) {
			if (i < len - 4)
				ctrlcode = (NFC_CSA | NFC_4BYTES | NFC_DREAD);
			else
				ctrlcode = (NFC_4BYTES | NFC_DREAD);

			if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
				return -1;

			*to = R_REG(osh, &cc->nflashdata);

			res += 4;
		}
	} else
		return -1;

	return res;
}

/* To write len bytes of oob data in the page specified in the page address offset */
static int
nflash_writeoob(si_t *sih, chipcregs_t *cc, uint64 offset, uint len, uchar *buf)
{
	uint32 mask;
	osl_t *osh;
	int i;
	uint32 *from;
	uint res;

	ASSERT(sih);

	mask = nflash.pagesize - 1;
	if ((offset & mask) != 0 || (len > nflash.oobsize) || (len & 0x3) != 0)
		return -1;

	osh = si_osh(sih);
	from = (uint32 *)buf;
	res = 0;

	if (CHIPID(sih->chip) == BCM4706_CHIP_ID) {
		uint32 page_addr, page_offset;
		uint32 ctrlcode;
		uint8 status;

		page_addr = (offset & ~(nflash.pagesize - 1)) * 2;
		page_offset = nflash.pagesize;
		page_addr += page_offset;

		W_REG(osh, &cc->nflashcoladdr, page_addr & nflash_col_mask);
		W_REG(osh, &cc->nflashrowaddr, page_addr >> nflash_row_shift);

		ctrlcode = (NFC_CSA | NFC_ROW | NFC_COL | NFC_CMD0 | NFCTRL_PAGEPROG);

		if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
			return -1;

		for (i = 0; i < len; i += 4, from++) {
			W_REG(osh, &cc->nflashdata, *from);

			if (i < len - 4)
				ctrlcode = (NFC_CSA | NFC_4BYTES | NFC_DWRITE);
			else
				ctrlcode = (NFC_4BYTES | NFC_DWRITE);

			if (nflash_ctrlcmd(osh, cc, ctrlcode) != 0)
				return -1;

			res += 4;
		}

		if (nflash_ctrlcmd(osh, cc, (NFC_CMD0 | NFCTRL_PROGSTART)) != 0)
			return -1;

		if (nflash_poll(sih, cc) < 0)
			return -1;

		if (nflash_readst(sih, cc, &status) != 0)
			return -1;

		if (status & 1) {
			printf("nflash_writeoob: failed with status 0x%02x\n", status);
			return -1;
		}

	} else
		return -1;

	return res;
}
