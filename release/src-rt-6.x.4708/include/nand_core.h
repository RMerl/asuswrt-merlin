/*
 * nand - Broadcom NAND specific definitions
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
 * $Id:$
 */

#ifndef	_nand_core_h_
#define	_nand_core_h_

/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */

typedef volatile struct nandregs {
	uint32 revision;		/* 0x000 */
	uint32 cmd_start;		/* 0x004 */
	uint32 cmd_ext_address;		/* 0x008 */
	uint32 cmd_address;		/* 0x00c */
	uint32 cmd_end_address;		/* 0x010 */
	uint32 intfc_status;		/* 0x014 */
	uint32 cs_nand_select;		/* 0x018 */
	uint32 cs_nand_xor;		/* 0x01c */
	uint32 ll_op;			/* 0x020 */
	uint32 mplane_base_ext_address;	/* 0x024 */
	uint32 mplane_base_address;	/* 0x028 */
	uint32 PAD[9];			/* 0x02C ~ 0x04C */

	uint32 acc_control_cs0;		/* 0x050 */
	uint32 config_cs0;		/* 0x054 */
	uint32 timing_1_cs0;		/* 0x058 */
	uint32 timing_2_cs0;		/* 0x05c */
	uint32 acc_control_cs1;		/* 0x060 */
	uint32 config_cs1;		/* 0x064 */
	uint32 timing_1_cs1;		/* 0x068 */
	uint32 timing_2_cs1;		/* 0x06c */
	uint32 PAD[20];			/* 0x070 ~ 0x0bc */

	uint32 corr_stat_threshold;	/* 0x0c0 */
	uint32 PAD[1];			/* 0x0c4 */

	uint32 blk_wr_protect;		/* 0x0c8 */
	uint32 multiplane_opcodes_1;	/* 0x0cc */
	uint32 multiplane_opcodes_2;	/* 0x0d0 */
	uint32 multiplane_ctrl;		/* 0x0d4 */
	uint32 PAD[9];			/* 0x0d8 ~ 0x0f8 */

	uint32 uncorr_error_count;	/* 0x0fc */
	uint32 corr_error_count;	/* 0x100 */
	uint32 read_error_count;	/* 0x104 */
	uint32 block_lock_status;	/* 0x108 */
	uint32 ecc_corr_ext_addr;	/* 0x10c */
	uint32 ecc_corr_addr;		/* 0x110 */
	uint32 ecc_unc_ext_addr;	/* 0x114 */
	uint32 ecc_unc_addr;		/* 0x118 */
	uint32 flash_read_ext_addr;	/* 0x11c */
	uint32 flash_read_addr;		/* 0x120 */
	uint32 program_page_ext_addr;	/* 0x124 */
	uint32 program_page_addr;	/* 0x128 */
	uint32 copy_back_ext_addr;	/* 0x12c */
	uint32 copy_back_addr;		/* 0x130 */
	uint32 block_erase_ext_addr;	/* 0x134 */
	uint32 block_erase_addr;	/* 0x138 */
	uint32 inv_read_ext_addr;	/* 0x13c */
	uint32 inv_read_addr;		/* 0x140 */
	uint32 init_status;		/* 0x144 */
	uint32 onfi_status;		/* 0x148 */
	uint32 onfi_debug_data;		/* 0x14c */
	uint32 semaphore;		/* 0x150 */
	uint32 PAD[16];			/* 0x154 ~ 0x190 */

	uint32 flash_device_id;		/* 0x194 */
	uint32 flash_device_id_ext;	/* 0x198 */
	uint32 ll_rddata;		/* 0x19c */
	uint32 PAD[24];			/* 0x1a0 ~ 0x1fc */

	uint32 spare_area_read_ofs[16];	/* 0x200 ~ 0x23c */
	uint32 PAD[16];			/* 0x240 ~ 0x27c */

	uint32 spare_area_write_ofs[16];	/* 0x280 ~ 0x2bc */
	uint32 PAD[80];			/* 0x2c0 ~ 0x3fc */

	uint32 flash_cache[128];	/* 0x400~0x5fc */
	uint32 PAD[576];		/* 0x600 ~ 0xefc */

	uint32 direct_read_rd_miss;	/* 0xf00 */
	uint32 block_erase_complete;	/* 0xf04 */
	uint32 copy_back_complete;	/* 0xf08 */
	uint32 program_page_complete;	/* 0xf0c */
	uint32 no_ctlr_ready;		/* 0xf10 */
	uint32 nand_rb_b;		/* 0xf14 */
	uint32 ecc_mips_uncorr;		/* 0xf18 */
	uint32 ecc_mips_corr;		/* 0xf1c */
} nandregs_t;

/* nand cache size */
#define NANDCACHE_SIZE				512

/* nand spare cache size */
#define NANDSPARECACHE_SIZE			64

/* nand_cs_nand_select */
#define NANDCSEL_NAND_WP			0x20000000
#define NANDCSEL_AUTO_ID_CFG			0x40000000

/* nand_cmd_start commands */
#define NANDCMD_NULL				0x00000000
#define NANDCMD_PAGE_RD				0x01000000
#define NANDCMD_SPARE_RD			0x02000000
#define NANDCMD_STATUS_RD			0x03000000
#define NANDCMD_PAGE_PROG			0x04000000
#define NANDCMD_SPARE_PROG			0x05000000
#define NANDCMD_COPY_BACK			0x06000000
#define NANDCMD_ID_RD				0x07000000
#define NANDCMD_BLOCK_ERASE			0x08000000
#define NANDCMD_FLASH_RESET			0x09000000
#define NANDCMD_LOCK				0x0a000000
#define NANDCMD_LOCK_DOWN			0x0b000000
#define NANDCMD_UNLOCK				0x0c000000
#define NANDCMD_LOCK_STATUS			0x0d000000
#define NANDCMD_PARAMETER_READ			0x0e000000
#define NANDCMD_PARAMETER_CHANGE_COL		0x0f000000
#define NANDCMD_LOW_LEVEL_OP			0x10000000
#define NANDCMD_PAGE_READ_MULTI			0x11000000
#define NANDCMD_STATUS_READ_MULTI		0x12000000
#define NANDCMD_OPCODE_MASK			0x1f000000

/* nand_acc_control_cs0 */
#define	NANDAC_CS0_RD_ECC_EN			0x80000000
#define	NANDAC_CS0_WR_ECC_EN			0x40000000
#define	NANDAC_CS0_RD_ECC_BLK0_EN		0x20000000
#define	NANDAC_CS0_FAST_PGM_RDIN		0x10000000
#define	NANDAC_CS0_RD_ERASED_ECC_EN		0x08000000
#define	NANDAC_CS0_PARTIAL_PAGE_EN		0x04000000
#define	NANDAC_CS0_WR_PREEMPT_EN		0x02000000
#define	NANDAC_CS0_PAGE_HIT_EN			0x01000000
#define	NANDAC_CS0_PREFETCH_EN			0x00800000
#define	NANDAC_CS0_CACHE_MODE_EN		0x00400000
#define	NANDAC_CS0_CACHE_MODE_LAST_PAGE		0x00200000
#define	NANDAC_CS0_ECC_LEVEL_MASK		0x001f0000
#define	NANDAC_CS0_ECC_LEVEL_SHIFT		16
#define	NANDAC_CS0_ECC_LEVEL_20			0x00140000
#define	NANDAC_CS0_SECTOR_SIZE_1K		0x00000080
#define	NANDAC_CS0_SECTOR_SIZE_1K_SHIFT		7
#define	NANDAC_CS0_SPARE_AREA_SIZE		0x0000007f
#define	NANDAC_CS0_SPARE_AREA_SHIFT		0
#define	NANDAC_CS0_SPARE_AREA_45B		0x0000002d

/* nand_config_cs0 */
#define	NANDCF_CS0_CONFIG_LOCK			0x80000000
#define	NANDCF_CS0_BLOCK_SIZE_MASK		0x70000000
#define	NANDCF_CS0_BLOCK_SIZE_SHIFT		28
#define NANDCF_CS0_BLOCK_SIZE_1MB		5
#define	NANDCF_CS0_DEVICE_SIZE_MASK		0x0f000000
#define	NANDCF_CS0_DEVICE_SIZE_SHIFT		24
#define NANDCF_CS0_DEVICE_SIZE_8GB		11
#define	NANDCF_CS0_DEVICE_WIDTH			0x00800000
#define	NANDCF_CS0_PAGE_SIZE_MASK		0x00300000
#define	NANDCF_CS0_PAGE_SIZE_SHIFT		20
#define	NANDCF_CS0_FULL_ADDR_BYTES_MASK		0x00070000
#define	NANDCF_CS0_FULL_ADDR_BYTES_SHIFT	16
#define	NANDCF_CS0_COL_ADDR_BYTES_MASK		0x00007000
#define	NANDCF_CS0_COL_ADDR_BYTES_SHIFT		12
#define	NANDCF_CS0_BLK_ADDR_BYTES_MASK		0x00000700
#define	NANDCF_CS0_BLK_ADDR_BYTES_SHIFT		8

/* nand_intfc_status */
#define	NANDIST_CTRL_READY			0x80000000
#define	NANDIST_FLASH_READY			0x40000000
#define	NANDIST_CACHE_VALID			0x20000000
#define	NANDIST_SPARE_VALID			0x10000000
#define	NANDIST_ERASED				0x08000000
#define	NANDIST_STATUS				0x000000ff
#define	NANDIST_STATUS_FAIL			0x00000001

/* cmd_ext_address */
#define NANDCMD_CS_SEL_MASK			0x00070000
#define NANDCMD_CS_SEL_SHIFT			16
#define NANDCMD_EXT_ADDR_MASK			0x0000ffff

/* nand_timing_1 */
#define	NANDTIMING1_TWP_MASK			0xf0000000
#define	NANDTIMING1_TWP_SHIFT			28
#define	NANDTIMING1_TWH_MASK			0x0f000000
#define	NANDTIMING1_TWH_SHIFT			24
#define	NANDTIMING1_TRP_MASK			0x00f00000
#define	NANDTIMING1_TRP_SHIFT			20
#define	NANDTIMING1_TREH_MASK			0x000f0000
#define	NANDTIMING1_TREH_SHIFT			16
#define	NANDTIMING1_TCS_MASK			0x0000f000
#define	NANDTIMING1_TCS_SHIFT			12
#define	NANDTIMING1_TCLH_MASK			0x00000f00
#define	NANDTIMING1_TCLH_SHIFT			8
#define	NANDTIMING1_TALH_MASK			0x000000f0
#define	NANDTIMING1_TALH_SHIFT			4
#define	NANDTIMING1_TADL_MASK			0x0000000f
#define	NANDTIMING1_TADL_SHIFT			0

/* nand_timing_2 */
#define	NANDTIMING2_CLK_SEL_MASK		0x80000000
#define	NANDTIMING2_CLK_SEL_SHIFT		31
#define	NANDTIMING2_TWB_MASK			0x00000e00
#define	NANDTIMING2_TWB_SHIFT			9
#define	NANDTIMING2_TWHR_MASK			0x000001f0
#define	NANDTIMING2_TWHR_SHIFT			4
#define	NANDTIMING2_TREAD_MASK			0x0000000f
#define	NANDTIMING2_TREAD_SHIFT			0

/*
 * NAND IDM
 */
/* Core specific control flags */
#define NAND_APB_LITTLE_ENDIAN			0x01000000
#define NAND_RO_CTRL_READY			0x00000001

#endif	/* _nand_core_h_ */
