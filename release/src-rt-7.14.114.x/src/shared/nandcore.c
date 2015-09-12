/*
 * Broadcom NAND core interface
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
 * $Id: $
 */

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <hndsoc.h>
#include <sbhndcpu.h>
#include <sbchipc.h>
#include <bcmdevs.h>
#include <nand_core.h>
#include <hndnand.h>
#include <hndpmu.h>

#ifdef BCMDBG
#define	NANDFL_MSG(args)	printf args
#else
#define	NANDFL_MSG(args)
#endif	/* BCMDBG */

#define NANDF_RETRIES	1000000

#define NANDF_SMALL_BADBLOCK_POS	5
#define NANDF_LARGE_BADBLOCK_POS	0

extern int nospare;

struct nandpart_timing_info {
	const char	*name;
	uint8	id[8];
	/* Timing unit is ns for the following parameters */
	uint8	tWP;
	uint8	tWH;
	uint8	tRP;
	uint8	tREH;
	uint8	tCS;
	uint8	tCLH;
	uint8	tALH;
	uint16	tADL;
	uint8	tWB;
	uint8	tWHR;
	uint8	tREAD;
};

static struct nandpart_timing_info nandpart_timing_list[] = {
	{"Samsung K9LCG08U0B",
	{0xec, 0xde, 0xd5, 0x7e, 0x68, 0x44},
	11, 11, 11, 11, 25, 5, 5, 300, 100, 176, 37},

	{"Micron MT29F4G08ABADA",
	{0x2c, 0xdc, 0x90, 0x95, 0x56},
	10, 7, 10, 7, 15, 5, 5, 70, 100, 60, 33},

	{"Micron MT29F64G08CBABA",
	{0x2c, 0x64, 0x44, 0x4b, 0xa9},
	50, 30, 50, 30, 70, 20, 20, 200, 200, 120, 57},

	{NULL, }
};

/* Private global state */
static hndnand_t nandcore;

static uint32 num_cache_per_page;
static uint32 spare_per_cache;

/* Prototype */
static int nandcore_poll(si_t *sih, nandregs_t *nc);

hndnand_t *nandcore_init(si_t *sih);
static int nandcore_read(hndnand_t *nfl, uint64 offset, uint len, uchar *buf);
static int nandcore_write(hndnand_t *nfl, uint64 offset, uint len, const uchar *buf);
static int nandcore_erase(hndnand_t *nfl, uint64 offset);
static int nandcore_checkbadb(hndnand_t *nfl, uint64 offset);
static int nandcore_checkbadb_nospare(hndnand_t *nfl, uint64 offset);
static int nandcore_mark_badb(hndnand_t *nfl, uint64 offset);
static int nandcore_read_oob(hndnand_t *nfl, uint64 addr, uint8 *oob);

#ifndef _CFE_
static int nandcore_dev_ready(hndnand_t *nfl);
static int nandcore_select_chip(hndnand_t *nfl, int chip);
static int nandcore_cmdfunc(hndnand_t *nfl, uint64 addr, int cmd);
static int nandcore_waitfunc(hndnand_t *nfl, int *status);
static int nandcore_write_oob(hndnand_t *nfl, uint64 addr, uint8 *oob);
static int nandcore_read_page(hndnand_t *nfl, uint64 addr, uint8 *buf, uint8 *oob, bool ecc,
	uint32 *herr, uint32 *serr);
static int nandcore_write_page(hndnand_t *nfl, uint64 addr, const uint8 *buf, uint8 *oob, bool ecc);
static int nandcore_cmd_read_byte(hndnand_t *nfl, int cmd, int arg);
#endif /* !_CFE_ */


/* Issue a nand flash command */
static INLINE void
nandcore_cmd(osl_t *osh, nandregs_t *nc, uint opcode)
{
	W_REG(osh, &nc->cmd_start, opcode);
}

static bool
_nandcore_buf_erased(const void *buf, unsigned len)
{
	unsigned i;
	const uint32 *p = buf;

	for (i = 0; i < (len >> 2); i++) {
		if (p[i] != 0xffffffff)
			return FALSE;
	}

	return TRUE;
}

static INLINE int
_nandcore_oobbyte_per_cache(hndnand_t *nfl, uint cache, uint32 spare)
{
	uint32 oob_byte;

	if (nfl->sectorsize == 512)
		oob_byte = spare;
	else {
		if ((spare * 2) < NANDSPARECACHE_SIZE)
			oob_byte = spare * 2;
		else
			oob_byte = (cache % 2) ?
			((spare * 2) - NANDSPARECACHE_SIZE) :
			 NANDSPARECACHE_SIZE;
	}

	return oob_byte;
}

static int
_nandcore_read_page(hndnand_t *nfl, uint64 offset, uint8 *buf, uint8 *oob, bool ecc,
	uint32 *herr, uint32 *serr)
{
	osl_t *osh;
	nandregs_t *nc = (nandregs_t *)nfl->core;
	aidmp_t *ai = (aidmp_t *)nfl->wrap;
	unsigned cache, col = 0;
	unsigned hard_err_count = 0;
	uint32 mask, reg, *to;
	uint32 err_soft_reg, err_hard_reg;
	int i, ret;
	uint8 *oob_to = oob;
	uint32 rd_oob_byte, left_oob_byte;

	ASSERT(nfl->sih);

	mask = nfl->pagesize - 1;
	/* Check offset and length */
	if ((offset & mask) != 0)
		return 0;

	if ((((offset + nfl->pagesize) >> 20) > nfl->size) ||
	    ((((offset + nfl->pagesize) >> 20) == nfl->size) &&
	     (((offset + nfl->pagesize) & ((1 << 20) - 1)) != 0)))
		return 0;

	osh = si_osh(nfl->sih);

	/* Reset  ECC error stats */
	err_hard_reg = R_REG(osh, &nc->uncorr_error_count);
	err_soft_reg = R_REG(osh, &nc->read_error_count);

	/* Enable ECC validation for ecc page reads */
	if (ecc)
		OR_REG(osh, nfl->chipidx ? &nc->acc_control_cs1 : &nc->acc_control_cs0,
			NANDAC_CS0_RD_ECC_EN);
	else
		AND_REG(osh, nfl->chipidx ? &nc->acc_control_cs1 : &nc->acc_control_cs0,
			~NANDAC_CS0_RD_ECC_EN);

	/* Loop all caches in page */
	for (cache = 0; cache < num_cache_per_page; cache++, col += NANDCACHE_SIZE) {
		uint32 ext_addr;

		/* Set the page address for the following commands */
		reg = (R_REG(osh, &nc->cmd_ext_address) & ~NANDCMD_EXT_ADDR_MASK);
		ext_addr = ((offset + col) >> 32) & NANDCMD_EXT_ADDR_MASK;
		W_REG(osh, &nc->cmd_ext_address, (reg | ext_addr));
		W_REG(osh, &nc->cmd_address, (uint32)offset + col);

		/* Issue command to read partial page */
		nandcore_cmd(osh, nc, NANDCMD_PAGE_RD);

		/* Wait for the command to complete */
		if ((ret = nandcore_poll(nfl->sih, nc)) < 0)
			return ret;

		/* Set controller to Little Endian mode for copying */
		OR_REG(osh, &ai->ioctrl, NAND_APB_LITTLE_ENDIAN);

		/* Read page data per cache */
		to = (uint32 *)(buf + col);
		for (i = 0; i < (NANDCACHE_SIZE / 4); i++, to++)
			*to = R_REG(osh, &nc->flash_cache[i]);

		/* Read oob data per cache */
		if (oob_to) {
			rd_oob_byte = _nandcore_oobbyte_per_cache(nfl, cache, spare_per_cache);

			left_oob_byte = rd_oob_byte % 4;

			/* Pay attention to natural address alignment access */
			for (i = 0; i < (rd_oob_byte / 4); i++) {
				reg = R_REG(osh, &nc->spare_area_read_ofs[i]);
				memcpy((void *)oob_to, (void *)&reg, 4);
				oob_to += 4;
			}

			if (left_oob_byte != 0) {
				reg = R_REG(osh, &nc->spare_area_read_ofs[i]);
				memcpy((void *)oob_to, (void *)&reg, left_oob_byte);
				oob_to += left_oob_byte;
			}
		}

		/* Return to Big Endian mode for commands etc */
		AND_REG(osh, &ai->ioctrl, ~NAND_APB_LITTLE_ENDIAN);

		/* capture hard errors for each partial */
		if (err_hard_reg != R_REG(osh, &nc->uncorr_error_count)) {
			int era = (R_REG(osh, &nc->intfc_status) & NANDIST_ERASED);
			if ((!era) && (!_nandcore_buf_erased(buf+col, NANDCACHE_SIZE)))
				hard_err_count ++;

			err_hard_reg = R_REG(osh, &nc->uncorr_error_count);
		}
	} /* for cache */

	if (!ecc)
		return 0;

	/* Report hard ECC errors */
	if (herr)
		*herr = hard_err_count;

	/* Get ECC soft error stats */
	if (serr)
		*serr = R_REG(osh, &nc->read_error_count) - err_soft_reg;

	return 0;
}

static int
_nandcore_write_page(hndnand_t *nfl, uint64 offset, const uint8 *buf, uint8 *oob, bool ecc)
{
	osl_t *osh;
	nandregs_t *nc = (nandregs_t *)nfl->core;
	aidmp_t *ai = (aidmp_t *)nfl->wrap;
	unsigned cache, col = 0;
	uint32 mask, reg, *from;
	int i, ret = 0;
	uint8 *oob_from = oob;
	uint32 wr_oob_byte, left_oob_byte;

	ASSERT(nfl->sih);

	mask = nfl->pagesize - 1;
	/* Check offset and length */
	if ((offset & mask) != 0)
		return 0;

	if ((((offset + nfl->pagesize) >> 20) > nfl->size) ||
	    ((((offset + nfl->pagesize) >> 20) == nfl->size) &&
	     (((offset + nfl->pagesize) & ((1 << 20) - 1)) != 0)))
		return 0;

	osh = si_osh(nfl->sih);

	/* Disable WP */
	AND_REG(osh, &nc->cs_nand_select, ~NANDCSEL_NAND_WP);

	/* Enable ECC generation for ecc page write, if requested */
	if (ecc)
		OR_REG(osh, nfl->chipidx ? &nc->acc_control_cs1 : &nc->acc_control_cs0,
			NANDAC_CS0_WR_ECC_EN);
	else
		AND_REG(osh, nfl->chipidx ? &nc->acc_control_cs1 : &nc->acc_control_cs0,
			~NANDAC_CS0_WR_ECC_EN);

	/* Loop all caches in page */
	for (cache = 0; cache < num_cache_per_page; cache++, col += NANDCACHE_SIZE) {
		uint32 ext_addr;

		/* Set the page address for the following commands */
		reg = (R_REG(osh, &nc->cmd_ext_address) & ~NANDCMD_EXT_ADDR_MASK);
		ext_addr = ((offset + col) >> 32) & NANDCMD_EXT_ADDR_MASK;
		W_REG(osh, &nc->cmd_ext_address, (reg | ext_addr));
		W_REG(osh, &nc->cmd_address, (uint32)offset + col);

		/* Set controller to Little Endian mode for copying */
		OR_REG(osh, &ai->ioctrl, NAND_APB_LITTLE_ENDIAN);

		/* Copy sub-page data */
		from = (uint32 *)(buf + col);
		for (i = 0; i < (NANDCACHE_SIZE / 4); i++, from++)
			W_REG(osh, &nc->flash_cache[i], *from);

		/* Set spare area is written at each cache start */
		if (oob_from) {
			/* Fill spare area write cache */
			wr_oob_byte = _nandcore_oobbyte_per_cache(nfl, cache, spare_per_cache);

			left_oob_byte = wr_oob_byte % 4;

			/* Pay attention to natural address alignment access */
			for (i = 0; i < (wr_oob_byte / 4); i++) {
				memcpy((void *)&reg, (void *)oob_from, 4);
				W_REG(osh, &nc->spare_area_write_ofs[i], reg);
				oob_from += 4;
			}

			if (left_oob_byte != 0) {
				reg = 0xffffffff;
				memcpy((void *)&reg, (void *)oob_from,
					left_oob_byte);
				W_REG(osh, &nc->spare_area_write_ofs[i], reg);
				oob_from += left_oob_byte;
				i++;
			}

			for (; i < (NANDSPARECACHE_SIZE / 4); i ++)
				W_REG(osh, &nc->spare_area_write_ofs[i],
					0xffffffff);
		}
		else {
			/* Write 0xffffffff to spare_area_write_ofs register
			 * to prevent old spare_area_write_ofs vale write
			 * when we issue NANDCMD_PAGE_PROG.
			 */
			for (i = 0; i < (NANDSPARECACHE_SIZE / 4); i++)
				W_REG(osh, &nc->spare_area_write_ofs[i],
					0xffffffff);
		}

		/* Return to Big Endian mode for commands etc */
		AND_REG(osh, &ai->ioctrl, ~NAND_APB_LITTLE_ENDIAN);

		/* Push data into internal cache */
		nandcore_cmd(osh, nc, NANDCMD_PAGE_PROG);

		ret = nandcore_poll(nfl->sih, nc);
		if (ret < 0)
			goto err;
	}

err:
	/* Enable WP */
	OR_REG(osh, &nc->cs_nand_select, NANDCSEL_NAND_WP);

	return ret;
}

static bool firsttime = TRUE;

static char *
nandcore_check_id(uint8 *id)
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
		name = " ";
		break;
	}

	return name;
}

static void
nandcore_override_config(hndnand_t *nfl)
{
	nandregs_t *nc = nfl->core;
	osl_t *osh;
	uint32 reg;

	ASSERT(nfl->sih);
	osh = si_osh(nfl->sih);

	/* Samsung K9LCG08U0B */
	if ((nfl->id[0] == 0xec) && (nfl->id[1] == 0xde) &&
	    (nfl->id[2] == 0xd5) && (nfl->id[3] == 0x7e) &&
	    (nfl->id[4] == 0x68) && (nfl->id[5] == 0x44)) {
		/* Block size, total size */
		reg = R_REG(osh, &nc->config_cs0);
		reg &= ~NANDCF_CS0_BLOCK_SIZE_MASK;
		reg |= (NANDCF_CS0_BLOCK_SIZE_1MB << NANDCF_CS0_BLOCK_SIZE_SHIFT);
		reg &= ~NANDCF_CS0_DEVICE_SIZE_MASK;
		reg |= (NANDCF_CS0_DEVICE_SIZE_8GB << NANDCF_CS0_DEVICE_SIZE_SHIFT);
		W_REG(osh, &nc->config_cs0, reg);

		/* Spare size, sector size and ECC level */
		reg = R_REG(osh, &nc->acc_control_cs0);
		reg &= ~NANDAC_CS0_SPARE_AREA_SIZE;
		reg |= NANDAC_CS0_SPARE_AREA_45B;
		reg |= NANDAC_CS0_SECTOR_SIZE_1K;
		reg &= ~NANDAC_CS0_ECC_LEVEL_MASK;
		reg |= NANDAC_CS0_ECC_LEVEL_20;
		W_REG(osh, &nc->acc_control_cs0, reg);
	}

	/* Micron MT29F64G08CBABA */
	if ((nfl->id[0] == 0x2c) && (nfl->id[1] == 0x64) &&
	    (nfl->id[2] == 0x44) && (nfl->id[3] == 0x4b) &&
	    (nfl->id[4] == 0xa9)) {
		/* Spare size, sector size and ECC level */
		reg = R_REG(osh, &nc->acc_control_cs0);
		reg &= ~NANDAC_CS0_SPARE_AREA_SIZE;
		reg |= NANDAC_CS0_SPARE_AREA_45B;
		reg |= NANDAC_CS0_SECTOR_SIZE_1K;
		reg &= ~NANDAC_CS0_ECC_LEVEL_MASK;
		reg |= NANDAC_CS0_ECC_LEVEL_20;
		W_REG(osh, &nc->acc_control_cs0, reg);
	}
}

static void
nandcore_optimize_timing(hndnand_t *nfl)
{
	nandregs_t *nc = nfl->core;
	osl_t *osh;
	struct nandpart_timing_info *info = nandpart_timing_list;
	uint32 reg, tmp_val;
	uint32 clk_select, ns, divisor;

	ASSERT(nfl->sih);
	osh = si_osh(nfl->sih);

	for (; info->name != NULL; info++) {
		if (memcmp(nfl->id, info->id, 5) == 0)
			break;
	}

	if (!info->name)
		return;

	reg = R_REG(osh, nfl->chipidx ? &nc->timing_2_cs1 : &nc->timing_2_cs0);
	clk_select = (reg & NANDTIMING2_CLK_SEL_MASK) >> NANDTIMING2_CLK_SEL_SHIFT;
	ns = (clk_select == 0) ? 8 : 4;
	divisor = (clk_select == 0) ? 2 : 4;

	/* Optimize nand_timing_1 */
	reg = ((info->tWP + (ns - 1)) / ns) << NANDTIMING1_TWP_SHIFT;
	reg |= ((info->tWH + (ns - 1)) / ns) << NANDTIMING1_TWH_SHIFT;
	reg |= ((info->tRP + (ns - 1)) / ns) << NANDTIMING1_TRP_SHIFT;
	reg |= ((info->tREH + (ns - 1)) / ns) << NANDTIMING1_TREH_SHIFT;
	tmp_val = (((info->tCS + (ns - 1)) / ns) + (divisor - 1)) / divisor;
	reg |= tmp_val << NANDTIMING1_TCS_SHIFT;
	reg |= ((info->tCLH + (ns - 1)) / ns) << NANDTIMING1_TCLH_SHIFT;
	tmp_val = (info->tALH > info->tWH) ? info->tALH : info->tWH;
	reg |= ((tmp_val + (ns - 1)) / ns) << NANDTIMING1_TALH_SHIFT;
	tmp_val = (((info->tADL + (ns - 1)) / ns) + (divisor - 1)) / divisor;
	tmp_val = (tmp_val > 0xf) ? 0xf : tmp_val;
	reg |= tmp_val << NANDTIMING1_TADL_SHIFT;
	W_REG(osh, nfl->chipidx ? &nc->timing_1_cs1 : &nc->timing_1_cs0, reg);

	/* Optimize nand_timing_2 */
	reg = clk_select << NANDTIMING2_CLK_SEL_SHIFT;
	tmp_val = (((info->tWB - (ns - 1)) / ns) + (divisor - 1)) / divisor;
	reg |= tmp_val << NANDTIMING2_TWB_SHIFT;
	tmp_val = (((info->tWHR + (ns - 1)) / ns) + (divisor - 1)) / divisor;
	reg |= tmp_val << NANDTIMING2_TWHR_SHIFT;
	tmp_val = info->tRP + info->tREH;
	tmp_val = (info->tREAD > tmp_val) ? tmp_val : info->tREAD;
	reg |= ((tmp_val + (ns - 1)) / ns) << NANDTIMING2_TREAD_SHIFT;
	W_REG(osh, nfl->chipidx ? &nc->timing_2_cs1 : &nc->timing_2_cs0, reg);

	printf("Optimize %s timing.\n", info->name);
#ifdef BCMDBG
	printf("R_REG(timing_1_cs%d)	= 0x%08x\n",
		nfl->chipidx, R_REG(osh, nfl->chipidx ? &nc->timing_1_cs1 : &nc->timing_1_cs0));
	printf("R_REG(timing_2_cs%d)	= 0x%08x\n",
		nfl->chipidx, R_REG(osh, nfl->chipidx ? &nc->timing_2_cs1 : &nc->timing_2_cs0));
#endif /* BCMDBG */

	return;
}

/* Initialize nand flash access */
hndnand_t *
nandcore_init(si_t *sih)
{
	nandregs_t *nc;
	aidmp_t *ai;
	uint32 id, id2;
	char *name = "";
	osl_t *osh;
	int i;
	uint32 ncf, val;
	uint32 acc_control;

	ASSERT(sih);

	/* Only support chipcommon revision == 42 for now */
	if (sih->ccrev != 42)
		return NULL;

	if ((nc = (nandregs_t *)si_setcore(sih, NS_NAND_CORE_ID, 0)) == NULL)
		return NULL;

	if (R_REG(NULL, &nc->flash_device_id) == 0)
		return NULL;

	if (!firsttime && nandcore.size)
		return &nandcore;

	osh = si_osh(sih);
	bzero(&nandcore, sizeof(nandcore));

	nandcore.sih = sih;
	nandcore.core = (void *)nc;
	nandcore.wrap = si_wrapperregs(sih);
	nandcore.read = nandcore_read;
	nandcore.write = nandcore_write;
	nandcore.erase = nandcore_erase;
	nandcore.checkbadb = nandcore_checkbadb;
	nandcore.markbadb = nandcore_mark_badb;
	nandcore.read_oob = nandcore_read_oob;

#ifndef _CFE_
	nandcore.dev_ready = nandcore_dev_ready;
	nandcore.select_chip = nandcore_select_chip;
	nandcore.cmdfunc = nandcore_cmdfunc;
	nandcore.waitfunc = nandcore_waitfunc;
	nandcore.write_oob = nandcore_write_oob;
	nandcore.read_page = nandcore_read_page;
	nandcore.write_page = nandcore_write_page;
	nandcore.cmd_read_byte = nandcore_cmd_read_byte;
#endif

	/* For some nand part, requires to do reset before the other command */
	nandcore_cmd(osh, nc, NANDCMD_FLASH_RESET);
	if (nandcore_poll(sih, nc) < 0) {
		return NULL;
	}

	nandcore_cmd(osh, nc, NANDCMD_ID_RD);
	if (nandcore_poll(sih, nc) < 0) {
		return NULL;
	}

	ai = (aidmp_t *)nandcore.wrap;

	/* Toggle as little endian */
	OR_REG(osh, &ai->ioctrl, NAND_APB_LITTLE_ENDIAN);

	id = R_REG(osh, &nc->flash_device_id);
	id2 = R_REG(osh, &nc->flash_device_id_ext);

	/* Toggle as big endian */
	AND_REG(osh, &ai->ioctrl, ~NAND_APB_LITTLE_ENDIAN);

	for (i = 0; i < 4; i++) {
		nandcore.id[i] = (id >> (8*i)) & 0xff;
		nandcore.id[i + 4] = (id2 >> (8*i)) & 0xff;
	}

	name = nandcore_check_id(nandcore.id);
	if (name == NULL)
		return NULL;
	nandcore.type = nandcore.id[0];

	/* Override configuration for specific nand flash */
	nandcore_override_config(&nandcore);

	ncf = R_REG(osh, &nc->config_cs0);
	/*  Page size (# of bytes) */
	val = (ncf & NANDCF_CS0_PAGE_SIZE_MASK) >> NANDCF_CS0_PAGE_SIZE_SHIFT;
	switch (val) {
	case 0:
		nandcore.pagesize = 512;
		break;
	case 1:
		nandcore.pagesize = (1 << 10) * 2;
		break;
	case 2:
		nandcore.pagesize = (1 << 10) * 4;
		break;
	case 3:
		nandcore.pagesize = (1 << 10) * 8;
		break;
	}
	/* Block size (# of bytes) */
	val = (ncf & NANDCF_CS0_BLOCK_SIZE_MASK) >> NANDCF_CS0_BLOCK_SIZE_SHIFT;
	switch (val) {
	case 0:
		nandcore.blocksize = (1 << 10) * 8;
		break;
	case 1:
		nandcore.blocksize = (1 << 10) * 16;
		break;
	case 2:
		nandcore.blocksize = (1 << 10) * 128;
		break;
	case 3:
		nandcore.blocksize = (1 << 10) * 256;
		break;
	case 4:
		nandcore.blocksize = (1 << 10) * 512;
		break;
	case 5:
		nandcore.blocksize = (1 << 10) * 1024;
		break;
	case 6:
		nandcore.blocksize = (1 << 10) * 2048;
		break;
	default:
		printf("Unknown block size\n");
		return NULL;
	}
	/* NAND flash size in MBytes */
	val = (ncf & NANDCF_CS0_DEVICE_SIZE_MASK) >> NANDCF_CS0_DEVICE_SIZE_SHIFT;
	nandcore.size = (1 << val) * 4;

	/* Get Device I/O data bus width */
	if (ncf & NANDCF_CS0_DEVICE_WIDTH)
		nandcore.width = 1;

	/* Spare size and Spare per cache (# of bytes) */
	acc_control = R_REG(osh, &nc->acc_control_cs0);

	/* Check conflict between 1K sector and page size */
	if (acc_control & NANDAC_CS0_SECTOR_SIZE_1K) {
		nandcore.sectorsize = 1024;
	}
	else
		nandcore.sectorsize = 512;

	if (nandcore.sectorsize == 1024 && nandcore.pagesize == 512) {
		printf("Pin strapping error. Page size is 512, but sector size is 1024\n");
		return NULL;
	}

	/* Get Spare size */
	nandcore.sparesize = acc_control & NANDAC_CS0_SPARE_AREA_SIZE;

	/* Get oob size,  */
	nandcore.oobsize = nandcore.sparesize * (nandcore.pagesize / NANDCACHE_SIZE);

	/* Get ECC level */
	nandcore.ecclevel = (acc_control & NANDAC_CS0_ECC_LEVEL_MASK) >> NANDAC_CS0_ECC_LEVEL_SHIFT;

	/* Adjusted sparesize and eccbytes if sectorsize is 1K */
	if (nandcore.sectorsize == 1024) {
		nandcore.sparesize *= 2;
		nandcore.eccbytes = ((nandcore.ecclevel * 14 + 3) >> 2);
	}
	else
		nandcore.eccbytes = ((nandcore.ecclevel * 14 + 7) >> 3);

	nandcore.numblocks = (nandcore.size * (1 << 10)) / (nandcore.blocksize >> 10);

	/* Get the number of cache per page */
	num_cache_per_page  = nandcore.pagesize / NANDCACHE_SIZE;

	/* Get the spare size per cache */
	spare_per_cache = nandcore.oobsize / num_cache_per_page;

	if (firsttime) {
		printf("Found a %s NAND flash:\n", name);
		printf("Total size:  %uMB\n", nandcore.size);
		printf("Block size:  %uKB\n", (nandcore.blocksize >> 10));
		printf("Page Size:   %uB\n", nandcore.pagesize);
		printf("OOB Size:    %uB\n", nandcore.oobsize);
		printf("Sector size: %uB\n", nandcore.sectorsize);
		printf("Spare size:  %uB\n", nandcore.sparesize);
		printf("ECC level:   %u (%u-bit)\n", nandcore.ecclevel,
			(nandcore.sectorsize == 1024)? nandcore.ecclevel*2 : nandcore.ecclevel);
		printf("Device ID: 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%02x\n",
			nandcore.id[0], nandcore.id[1], nandcore.id[2],
			nandcore.id[3], nandcore.id[4], nandcore.id[5]);
	}
	firsttime = FALSE;

	/* Memory mapping */
	nandcore.phybase = SI_NS_NANDFLASH;
	nandcore.base = (uint32)REG_MAP(SI_NS_NANDFLASH, SI_FLASH_WINDOW);

	/* For 1KB sector size setting */
	if (R_REG(osh, &nc->acc_control_cs0) & NANDAC_CS0_SECTOR_SIZE_1K) {
		AND_REG(osh, &nc->acc_control_cs0, ~NANDAC_CS0_PARTIAL_PAGE_EN);
		printf("Disable PARTIAL_PAGE_EN\n");
		AND_REG(osh, &nc->acc_control_cs0, ~NANDAC_CS0_FAST_PGM_RDIN);
		printf("Disable FAST_PGM_RDIN\n");
	}

	/* Optimize timing */
	nandcore_optimize_timing(&nandcore);

#ifdef BCMDBG
	/* Configuration readback */
	printf("R_REG(nand_revision)	= 0x%08x\n", R_REG(osh, &nc->revision));
	printf("R_REG(cs_nand_select)	= 0x%08x\n", R_REG(osh, &nc->cs_nand_select));
	printf("R_REG(config_cs0)	= 0x%08x\n", R_REG(osh, &nc->config_cs0));
	printf("R_REG(acc_control_cs0)	= 0x%08x\n", R_REG(osh, &nc->acc_control_cs0));
#endif /* BCMDBG */

	return nandcore.size ? &nandcore : NULL;
}

/* Read len bytes starting at offset into buf. Returns number of bytes read. */
static int
nandcore_read(hndnand_t *nfl, uint64 offset, uint len, uchar *buf)
{
	osl_t *osh;
	uint8 *to;
	uint res;
	uint32 herr = 0, serr = 0;

	ASSERT(nfl->sih);
	osh = si_osh(nfl->sih);

	to = buf;
	res = len;

	while (res > 0) {
		_nandcore_read_page(nfl, offset, to, NULL, TRUE, &herr, &serr);

		res -= nfl->pagesize;
		offset += nfl->pagesize;
		to += nfl->pagesize;
	}

	return (len - res);
}

/* Poll for command completion. Returns zero when complete. */
static int
nandcore_poll(si_t *sih, nandregs_t *nc)
{
	osl_t *osh;
	int i;
	uint32 pollmask;

	ASSERT(sih);
	osh = si_osh(sih);

	pollmask = NANDIST_CTRL_READY | NANDIST_FLASH_READY;
	for (i = 0; i < NANDF_RETRIES; i++) {
		if ((R_REG(osh, &nc->intfc_status) & pollmask) == pollmask) {
			return 0;
		}
	}

	printf("%s: not ready\n", __FUNCTION__);
	return -1;
}

/* Write len bytes starting at offset into buf. Returns number of bytes
 * written.
 */
static int
nandcore_write(hndnand_t *nfl, uint64 offset, uint len, const uchar *buf)
{
	int ret = 0;
	osl_t *osh;
	uint res;
	uint8 *from;

	ASSERT(nfl->sih);
	osh = si_osh(nfl->sih);

	from = (uint8 *)buf;
	res = len;

	while (res > 0) {
		ret = _nandcore_write_page(nfl, offset, from, NULL, TRUE);
		if (ret < 0)
			return ret;

		res -= nfl->pagesize;
		offset += nfl->pagesize;
		from += nfl->pagesize;
	}

	if (ret)
		return ret;

	return (len - res);
}

/* Erase a region. Returns number of bytes scheduled for erasure.
 * Caller should poll for completion.
 */
static int
nandcore_erase(hndnand_t *nfl, uint64 offset)
{
	si_t *sih = nfl->sih;
	nandregs_t *nc = (nandregs_t *)nfl->core;
	osl_t *osh;
	int ret = -1;
	uint8 status = 0;
	uint32 reg;

	ASSERT(sih);

	osh = si_osh(sih);
	if ((offset >> 20) >= nfl->size)
		return -1;
	if ((offset & (nfl->blocksize - 1)) != 0) {
		return -1;
	}

	/* Disable WP */
	AND_REG(osh, &nc->cs_nand_select, ~NANDCSEL_NAND_WP);

	/* Set the block address for the following commands */
	reg = (R_REG(osh, &nc->cmd_ext_address) & ~NANDCMD_EXT_ADDR_MASK);
	W_REG(osh, &nc->cmd_ext_address, (reg | ((offset >> 32) & NANDCMD_EXT_ADDR_MASK)));

	W_REG(osh, &nc->cmd_address, (uint32)offset);
	nandcore_cmd(osh, nc, NANDCMD_BLOCK_ERASE);
	if (nandcore_poll(sih, nc) < 0)
		goto exit;

	/* Check status */
	W_REG(osh, &nc->cmd_start, NANDCMD_STATUS_RD);
	if (nandcore_poll(sih, nc) < 0)
		goto exit;

	status = R_REG(osh, &nc->intfc_status) & NANDIST_STATUS;
	if (status & 1)
		goto exit;

	ret = 0;
exit:
	/* Enable WP */
	OR_REG(osh, &nc->cs_nand_select, NANDCSEL_NAND_WP);

	return ret;
}

static int
nandcore_checkbadb_nospare(hndnand_t *nfl, uint64 offset)
{
	si_t *sih = nfl->sih;
	nandregs_t *nc = (nandregs_t *)nfl->core;
	aidmp_t *ai = (aidmp_t *)nfl->wrap;
	osl_t *osh;
	int i;
	uint off;
	uint32 nand_intfc_status;
	int ret = 0;
	uint32 reg;

	ASSERT(sih);

	osh = si_osh(sih);
	if ((offset >> 20) >= nfl->size)
		return -1;
	if ((offset & (nfl->blocksize - 1)) != 0) {
		return -1;
	}

	/* Set the block address for the following commands */
	reg = (R_REG(osh, &nc->cmd_ext_address) & ~NANDCMD_EXT_ADDR_MASK);
	W_REG(osh, &nc->cmd_ext_address, (reg | (offset >> 32)));

	for (i = 0; i < 2; i++) {
		off = offset + (nfl->pagesize * i);
		W_REG(osh, &nc->cmd_address, off);
		nandcore_cmd(osh, nc, NANDCMD_SPARE_RD);
		if (nandcore_poll(sih, nc) < 0) {
			ret = -1;
			goto exit;
		}
		nand_intfc_status = R_REG(osh, &nc->intfc_status) & NANDIST_SPARE_VALID;
		if (nand_intfc_status != NANDIST_SPARE_VALID) {
			ret = -1;
#ifdef BCMDBG
		printf("%s: Spare is not valid\n", __FUNCTION__);
#endif
			goto exit;
		}

		/* Toggle as little endian */
		OR_REG(osh, &ai->ioctrl, NAND_APB_LITTLE_ENDIAN);

		if ((R_REG(osh, &nc->spare_area_read_ofs[0]) & 0xff) != 0xff) {
			ret = -1;
#ifdef BCMDBG
			printf("%s: Bad Block (0x%llx)\n", __FUNCTION__, offset);
#endif
		}

		/* Toggle as big endian */
		AND_REG(osh, &ai->ioctrl, ~NAND_APB_LITTLE_ENDIAN);

		if (ret == -1)
			break;
	}

exit:
	return ret;
}

static int
nandcore_checkbadb(hndnand_t *nfl, uint64 offset)
{
	si_t *sih = nfl->sih;
	nandregs_t *nc = (nandregs_t *)nfl->core;
	aidmp_t *ai = (aidmp_t *)nfl->wrap;
	osl_t *osh;
	int i, j;
	uint64 addr;
	int ret = 0;
	uint32 reg, oob_bi;
	unsigned cache, col = 0;
	uint32 rd_oob_byte, left_oob_byte;

	if(nospare)
		return nandcore_checkbadb_nospare(nfl, offset);

	ASSERT(sih);

	osh = si_osh(sih);
	if ((offset >> 20) >= nfl->size)
		return -1;
	if ((offset & (nfl->blocksize - 1)) != 0) {
		return -1;
	}

	/* Enable ECC validation for spare area reads */
	OR_REG(osh, nfl->chipidx ? &nc->acc_control_cs1 : &nc->acc_control_cs0,
		NANDAC_CS0_RD_ECC_EN);

	/* Check the first two pages for this block */
	for (i = 0; i < 2; i++) {
		addr = offset + (nfl->pagesize * i);
		col = 0;
		/* Loop all caches in page */
		for (cache = 0; cache < num_cache_per_page; cache++, col += NANDCACHE_SIZE) {
			uint32 ext_addr;

			/* Set the page address for the following commands */
			reg = (R_REG(osh, &nc->cmd_ext_address) & ~NANDCMD_EXT_ADDR_MASK);
			ext_addr = ((addr + col) >> 32) & NANDCMD_EXT_ADDR_MASK;
			W_REG(osh, &nc->cmd_ext_address, (reg | ext_addr));
			W_REG(osh, &nc->cmd_address, (uint32)addr + col);

			/* Issue page-read command */
			nandcore_cmd(osh, nc, NANDCMD_PAGE_RD);

			/* Wait for the command to complete */
			if (nandcore_poll(sih, nc) < 0) {
				ret = -1;
				goto exit;
			}

			/* Set controller to Little Endian mode for copying */
			OR_REG(osh, &ai->ioctrl, NAND_APB_LITTLE_ENDIAN);

			rd_oob_byte = _nandcore_oobbyte_per_cache(nfl, cache, spare_per_cache);

			left_oob_byte = rd_oob_byte % 4;

			for (j = 0; j < (rd_oob_byte / 4); j++) {
				if (cache == 0 && j == 0)
					/* Save bad block indicator */
					oob_bi = R_REG(osh, &nc->spare_area_read_ofs[0]);
				else
					reg = R_REG(osh, &nc->spare_area_read_ofs[j]);
			}

			if (left_oob_byte != 0) {
				reg = R_REG(osh, &nc->spare_area_read_ofs[j]);
			}

			/* Return to Big Endian mode for commands etc */
			AND_REG(osh, &ai->ioctrl, ~NAND_APB_LITTLE_ENDIAN);
		}

		/* Check bad block indicator */
		if ((oob_bi & 0xFF) != 0xFF) {
			ret = -1;
#ifdef BCMDBG
			printf("%s: Bad Block (0x%llx)\n", __FUNCTION__, offset);
#endif
			break;
		}
	}

exit:
	return ret;
}

static int
nandcore_mark_badb(hndnand_t *nfl, uint64 offset)
{
	si_t *sih = nfl->sih;
	nandregs_t *nc = (nandregs_t *)nfl->core;
	aidmp_t *ai = (aidmp_t *)nfl->wrap;
	osl_t *osh;
	uint64 off;
	int i, ret = 0;
	uint32 reg;

	ASSERT(sih);

	osh = si_osh(sih);
	if ((offset >> 20) >= nfl->size)
		return -1;
	if ((offset & (nfl->blocksize - 1)) != 0) {
		return -1;
	}

	/* Disable WP */
	AND_REG(osh, &nc->cs_nand_select, ~NANDCSEL_NAND_WP);

	/* Erase block */
	W_REG(osh, &nc->cmd_address, offset);
	nandcore_cmd(osh, nc, NANDCMD_BLOCK_ERASE);
	if (nandcore_poll(sih, nc) < 0) {
		ret = -1;
		/* Still go through the spare area write */
		/* goto err; */
	}

	/*
	 * Enable partial page programming and disable ECC checkbit generation
	 * for PROGRAM_SPARE_AREA
	 */
	reg = R_REG(osh, &nc->acc_control_cs0);
	reg |= NANDAC_CS0_PARTIAL_PAGE_EN;
	reg |= NANDAC_CS0_FAST_PGM_RDIN;
	reg &= ~NANDAC_CS0_WR_ECC_EN;
	W_REG(osh, &nc->acc_control_cs0, reg);

	for (i = 0; i < 2; i++) {
		uint32 ext_addr;

		off = offset + (nfl->pagesize * i);

		/* Set the block address for the following commands */
		reg = (R_REG(osh, &nc->cmd_ext_address) & ~NANDCMD_EXT_ADDR_MASK);
		ext_addr = (off >> 32) & NANDCMD_EXT_ADDR_MASK;
		W_REG(osh, &nc->cmd_ext_address, (reg | ext_addr));

		W_REG(osh, &nc->cmd_address, (uint32)off);

		/* Toggle as little endian */
		OR_REG(osh, &ai->ioctrl, NAND_APB_LITTLE_ENDIAN);

		W_REG(osh, &nc->spare_area_write_ofs[0], 0);
		W_REG(osh, &nc->spare_area_write_ofs[1], 0);
		W_REG(osh, &nc->spare_area_write_ofs[2], 0);
		W_REG(osh, &nc->spare_area_write_ofs[3], 0);

		/* Toggle as big endian */
		AND_REG(osh, &ai->ioctrl, ~NAND_APB_LITTLE_ENDIAN);

		nandcore_cmd(osh, nc, NANDCMD_SPARE_PROG);
		if (nandcore_poll(sih, nc) < 0) {
			ret = -1;
#if BCMDBG
			printf("%s: Spare program is not ready\n", __FUNCTION__);
#endif
			goto err;
		}
	}

err:
	/* Restore the default value for spare area write registers */
	W_REG(osh, &nc->spare_area_write_ofs[0], 0xffffffff);
	W_REG(osh, &nc->spare_area_write_ofs[1], 0xffffffff);
	W_REG(osh, &nc->spare_area_write_ofs[2], 0xffffffff);
	W_REG(osh, &nc->spare_area_write_ofs[3], 0xffffffff);

	/*
	 * Disable partial page programming and enable ECC checkbit generation
	 * for PROGRAM_SPARE_AREA
	 */
	reg = R_REG(osh, &nc->acc_control_cs0);
	reg &= ~NANDAC_CS0_PARTIAL_PAGE_EN;
	reg &= ~NANDAC_CS0_FAST_PGM_RDIN;
	reg |= NANDAC_CS0_WR_ECC_EN;
	W_REG(osh, &nc->acc_control_cs0, reg);

	/* Enable WP */
	OR_REG(osh, &nc->cs_nand_select, NANDCSEL_NAND_WP);

	return ret;
}


#ifndef _CFE_
/* Functions support brcmnand driver */
static void
_nandcore_set_cmd_address(hndnand_t *nfl, uint64 addr)
{
	uint32 reg;
	osl_t *osh;
	si_t *sih = nfl->sih;
	nandregs_t *nc = (nandregs_t *)nfl->core;

	ASSERT(sih);
	osh = si_osh(sih);

	reg = (R_REG(osh, &nc->cmd_ext_address) & ~NANDCMD_EXT_ADDR_MASK);
	W_REG(osh, &nc->cmd_ext_address, (reg | ((addr >> 32) & NANDCMD_EXT_ADDR_MASK)));
	W_REG(osh, &nc->cmd_address, (uint32)addr);
}

static int
nandcore_dev_ready(hndnand_t *nfl)
{
	aidmp_t *ai = (aidmp_t *)nfl->wrap;

	ASSERT(nfl->sih);

	return (R_REG(si_osh(nfl->sih), &ai->iostatus) & NAND_RO_CTRL_READY);
}

static int
nandcore_select_chip(hndnand_t *nfl, int chip)
{
	uint32 reg;
	osl_t *osh;
	si_t *sih = nfl->sih;
	nandregs_t *nc = (nandregs_t *)nfl->core;

	ASSERT(sih);
	osh = si_osh(sih);

	reg = R_REG(osh, &nc->cmd_ext_address);
	reg &= ~NANDCMD_CS_SEL_MASK;
	reg |= (chip << NANDCMD_CS_SEL_SHIFT);
	W_REG(osh, &nc->cmd_ext_address, reg);

	/* Set active chip index */
	nfl->chipidx = chip;

	return 0;
}

static int
nandcore_cmdfunc(hndnand_t *nfl, uint64 addr, int cmd)
{
	int ret = 0;
	osl_t *osh;
	nandregs_t *nc = (nandregs_t *)nfl->core;

	ASSERT(nfl->sih);
	osh = si_osh(nfl->sih);

	switch (cmd) {
	case CMDFUNC_ERASE1:
		_nandcore_set_cmd_address(nfl, addr);
		break;
	case CMDFUNC_ERASE2:
		/* Disable WP */
		AND_REG(osh, &nc->cs_nand_select, ~NANDCSEL_NAND_WP);
		nandcore_cmd(osh, nc, NANDCMD_BLOCK_ERASE);
		ret = nandcore_waitfunc(nfl, NULL);
		/* Enable WP */
		OR_REG(osh, &nc->cs_nand_select, NANDCSEL_NAND_WP);
		break;
	case CMDFUNC_SEQIN:
		_nandcore_set_cmd_address(nfl, addr);
		break;
	case CMDFUNC_READ:
		_nandcore_set_cmd_address(nfl, addr);
		nandcore_cmd(osh, nc, NANDCMD_PAGE_RD);
		ret = nandcore_waitfunc(nfl, NULL);
		break;
	case CMDFUNC_RESET:
		nandcore_cmd(osh, nc, NANDCMD_FLASH_RESET);
		ret = nandcore_waitfunc(nfl, NULL);
		break;
	case CMDFUNC_READID:
		nandcore_cmd(osh, nc, NANDCMD_ID_RD);
		ret = nandcore_waitfunc(nfl, NULL);
		break;
	case CMDFUNC_STATUS:
		/* Disable WP */
		AND_REG(osh, &nc->cs_nand_select, ~NANDCSEL_NAND_WP);
		nandcore_cmd(osh, nc, NANDCMD_STATUS_RD);
		ret = nandcore_waitfunc(nfl, NULL);
		/* Enable WP */
		OR_REG(osh, &nc->cs_nand_select, NANDCSEL_NAND_WP);
		break;
	case CMDFUNC_READOOB:
		break;
	default:
#ifdef BCMDBG
		printf("%s: Unknow command 0x%x\n", __FUNCTION__, cmd);
#endif
		ret = -1;
		break;
	}

	return ret;
}

/* Return intfc_status FLASH_STATUS if CTRL/FLASH is ready otherwise -1 */
static int
nandcore_waitfunc(hndnand_t *nfl, int *status)
{
	int ret;
	osl_t *osh;
	nandregs_t *nc = (nandregs_t *)nfl->core;

	ASSERT(nfl->sih);
	osh = si_osh(nfl->sih);

	ret = nandcore_poll(nfl->sih, nc);
	if (ret == 0 && status)
		*status = R_REG(osh, &nc->intfc_status) & NANDIST_STATUS;

	return ret;
}

#endif

static int
nandcore_read_oob(hndnand_t *nfl, uint64 addr, uint8 *oob)
{
	osl_t *osh;
	si_t *sih = nfl->sih;
	nandregs_t *nc = (nandregs_t *)nfl->core;
	aidmp_t *ai = (aidmp_t *)nfl->wrap;
	uint32 reg;
	unsigned cache, col = 0;
	int i;
	uint8 *to = oob;
	uint32 rd_oob_byte, left_oob_byte;

	ASSERT(sih);
	osh = si_osh(sih);

	/* Enable ECC validation for spare area reads */
	OR_REG(osh, nfl->chipidx ? &nc->acc_control_cs1 : &nc->acc_control_cs0,
		NANDAC_CS0_RD_ECC_EN);

	/* Loop all caches in page */
	for (cache = 0; cache < num_cache_per_page; cache++, col += NANDCACHE_SIZE) {
		uint32 ext_addr;

		/* Set the page address for the following commands */
		reg = (R_REG(osh, &nc->cmd_ext_address) & ~NANDCMD_EXT_ADDR_MASK);
		ext_addr = ((addr + col) >> 32) & NANDCMD_EXT_ADDR_MASK;
		W_REG(osh, &nc->cmd_ext_address, (reg | ext_addr));
		W_REG(osh, &nc->cmd_address, (uint32)(addr + col));

		/* Issue page-read command */
		nandcore_cmd(osh, nc, NANDCMD_PAGE_RD);

		/* Wait for the command to complete */
		if (nandcore_poll(sih, nc))
			return -1;

		/* Set controller to Little Endian mode for copying */
		OR_REG(osh, &ai->ioctrl, NAND_APB_LITTLE_ENDIAN);

		rd_oob_byte = _nandcore_oobbyte_per_cache(nfl, cache, spare_per_cache);

		left_oob_byte = rd_oob_byte % 4;

		/* Pay attention to natural address alignment access */
		for (i = 0; i < (rd_oob_byte / 4); i++) {
			reg = R_REG(osh, &nc->spare_area_read_ofs[i]);
			memcpy((void *)to, (void *)&reg, 4);
			to += 4;
		}

		if (left_oob_byte != 0) {
			reg = R_REG(osh, &nc->spare_area_read_ofs[i]);
			memcpy((void *)to, (void *)&reg, left_oob_byte);
			to += left_oob_byte;
		}

		/* Return to Big Endian mode for commands etc */
		AND_REG(osh, &ai->ioctrl, ~NAND_APB_LITTLE_ENDIAN);
	}

	return 0;
}

#ifndef _CFE_

static int
nandcore_write_oob(hndnand_t *nfl, uint64 addr, uint8 *oob)
{
	osl_t *osh;
	si_t *sih = nfl->sih;
	nandregs_t *nc = (nandregs_t *)nfl->core;
	aidmp_t *ai = (aidmp_t *)nfl->wrap;
	uint32 reg;
	unsigned cache, col = 0;
	int i;
	int ret = 0;
	uint8 *from = oob;
	uint32 wr_oob_byte, left_oob_byte;

	ASSERT(sih);

	osh = si_osh(sih);

	/* Disable WP */
	AND_REG(osh, &nc->cs_nand_select, ~NANDCSEL_NAND_WP);

	/*
	 * Enable partial page programming and disable ECC checkbit generation
	 * for PROGRAM_SPARE_AREA
	 */
	reg = R_REG(osh, nfl->chipidx ? &nc->acc_control_cs1 : &nc->acc_control_cs0);
	if (nfl->sectorsize == 512) {
		reg |= NANDAC_CS0_PARTIAL_PAGE_EN;
		reg |= NANDAC_CS0_FAST_PGM_RDIN;
	}
	reg &= ~NANDAC_CS0_WR_ECC_EN;
	W_REG(osh, nfl->chipidx ? &nc->acc_control_cs1 : &nc->acc_control_cs0, reg);

	/* Loop all caches in page */
	for (cache = 0; cache < num_cache_per_page; cache++, col += NANDCACHE_SIZE) {
		uint32 ext_addr;

		/* Set the page address for the following commands */
		reg = (R_REG(osh, &nc->cmd_ext_address) & ~NANDCMD_EXT_ADDR_MASK);
		ext_addr = ((addr + col) >> 32) & NANDCMD_EXT_ADDR_MASK;
		W_REG(osh, &nc->cmd_ext_address, (reg | ext_addr));
		W_REG(osh, &nc->cmd_address, (uint32)(addr + col));

		/* Set controller to Little Endian mode for copying */
		OR_REG(osh, &ai->ioctrl, NAND_APB_LITTLE_ENDIAN);

		/* Must fill flash cache with all 0xff in each round */
		for (i = 0; i < (NANDCACHE_SIZE / 4); i++)
			W_REG(osh, &nc->flash_cache[i], 0xffffffff);

		/* Fill spare area write cache */
		wr_oob_byte = _nandcore_oobbyte_per_cache(nfl, cache, spare_per_cache);

		left_oob_byte = wr_oob_byte % 4;

		/* Pay attention to natural address alignment access */
		for (i = 0; i < (wr_oob_byte / 4); i++) {
			memcpy((void *)&reg, (void *)from, 4);
			W_REG(osh, &nc->spare_area_write_ofs[i], reg);
			from += 4;
		}

		if (left_oob_byte != 0) {
			reg = 0xffffffff;
			memcpy((void *)&reg, (void *)from, left_oob_byte);
			W_REG(osh, &nc->spare_area_write_ofs[i], reg);
			from += left_oob_byte;
			i++;
		}

		for (; i < (NANDSPARECACHE_SIZE / 4); i++)
			W_REG(osh, &nc->spare_area_write_ofs[i], 0xffffffff);

		/* Return to Big Endian mode for commands etc */
		AND_REG(osh, &ai->ioctrl, ~NAND_APB_LITTLE_ENDIAN);

		/* Push spare bytes into internal buffer, last goes to flash */
		nandcore_cmd(osh, nc, NANDCMD_PAGE_PROG);

		if (nandcore_poll(sih, nc)) {
			ret = -1;
			goto err;
		}
	}

err:
	/*
	 * Disable partial page programming and enable ECC checkbit generation
	 * for PROGRAM_SPARE_AREA
	 */
	reg = R_REG(osh, nfl->chipidx ? &nc->acc_control_cs1 : &nc->acc_control_cs0);
	if (nfl->sectorsize == 512) {
		reg &= ~NANDAC_CS0_PARTIAL_PAGE_EN;
		reg &= ~NANDAC_CS0_FAST_PGM_RDIN;
	}
	reg |= NANDAC_CS0_WR_ECC_EN;
	W_REG(osh, nfl->chipidx ? &nc->acc_control_cs1 : &nc->acc_control_cs0, reg);

	/* Enable WP */
	OR_REG(osh, &nc->cs_nand_select, NANDCSEL_NAND_WP);

	return ret;
}

static int
nandcore_read_page(hndnand_t *nfl, uint64 addr, uint8 *buf, uint8 *oob, bool ecc,
	uint32 *herr, uint32 *serr)
{
	return _nandcore_read_page(nfl, addr, buf, oob, ecc, herr, serr);
}

static int
nandcore_write_page(hndnand_t *nfl, uint64 addr, const uint8 *buf, uint8 *oob, bool ecc)
{
	return _nandcore_write_page(nfl, addr, buf, oob, ecc);
}

static int
nandcore_cmd_read_byte(hndnand_t *nfl, int cmd, int arg)
{
	int id_ext = arg;
	osl_t *osh;
	nandregs_t *nc = (nandregs_t *)nfl->core;

	ASSERT(nfl->sih);
	osh = si_osh(nfl->sih);

	switch (cmd) {
	case CMDFUNC_READID:
		return R_REG(osh, id_ext ? &nc->flash_device_id_ext : &nc->flash_device_id);
	case CMDFUNC_STATUS:
		return (R_REG(osh, &nc->intfc_status) & NANDIST_STATUS);
	default:
#ifdef BCMDBG
		printf("%s: Unknow command 0x%x\n", __FUNCTION__, cmd);
#endif
		break;
	}

	return 0;
}
#endif /* !_CFE_ */
