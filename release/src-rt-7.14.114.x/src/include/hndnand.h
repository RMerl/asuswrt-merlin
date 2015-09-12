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
 * $Id: nflash.h 256080 2011-04-28 08:40:56Z $
 */

#ifndef _hndnand_h_
#define _hndnand_h_

#define	NFL_VENDOR_AMD			0x01
#define	NFL_VENDOR_NUMONYX		0x20
#define	NFL_VENDOR_MICRON		0x2C
#define	NFL_VENDOR_TOSHIBA		0x98
#define NFL_VENDOR_HYNIX		0xAD
#define NFL_VENDOR_SAMSUNG		0xEC
#define NFL_VENDOR_ESMT			0x92
#define NFL_VENDOR_MXIC			0xC2
#define NFL_VENDOR_ZENTEL_ESMT		0xC8
#define NFL_VENDOR_WINBOND		0xF1

#define NFL_SECTOR_SIZE			512
#define NFL_TABLE_END			0xffffffff

#define NFL_BOOT_SIZE			0x200000
/* ASUS LINUX MTD SIZE */
#if CONFIG_LINUX_MTD == 32
 #define NFL_BOOT_OS_SIZE                      0x2000000
#elif CONFIG_LINUX_MTD == 64
#define NFL_BOOT_OS_SIZE                       0x4000000
#elif CONFIG_LINUX_MTD == 128
#define NFL_BOOT_OS_SIZE                       0x8000000
#else
#define NFL_BOOT_OS_SIZE                       0x2000000
#endif
#define NFL_BBT_SIZE			0x100000

#ifdef __ARM_ARCH_7A__
#define NFL_BIG_BOOT_SIZE		0x800000	/* 8 MB */
#define NFL_BIG_BOOT_OS_SIZE		0x2600000	/* 38 MB */
#define NFL_1M_BLOCK_SIZE		1024		/* KB */

#define nfl_boot_size(nfl)		((((nfl)->blocksize >> 10) >= NFL_1M_BLOCK_SIZE) ? \
						NFL_BIG_BOOT_SIZE : NFL_BOOT_SIZE)
#define nfl_boot_os_size(nfl)		((((nfl)->blocksize >> 10) >= NFL_1M_BLOCK_SIZE) ? \
						NFL_BIG_BOOT_OS_SIZE : NFL_BOOT_OS_SIZE)
#else
#define nfl_boot_size(nfl)		NFL_BOOT_SIZE
#define nfl_boot_os_size(nfl)		NFL_BOOT_OS_SIZE
#endif /* __ARM_ARCH_7A__ */

#ifndef _CFE_
/* Command functions  commands */
#define CMDFUNC_ERASE1			1
#define CMDFUNC_ERASE2			2
#define CMDFUNC_SEQIN			3
#define CMDFUNC_READ			4
#define CMDFUNC_RESET			5
#define CMDFUNC_READID			6
#define CMDFUNC_STATUS			7
#define CMDFUNC_READOOB			8
#endif /* !_CFE_ */

struct hndnand;
typedef struct hndnand hndnand_t;

struct hndnand {
	uint blocksize;		/* Block size */
	uint pagesize;		/* Page size */
	uint oobsize;		/* OOB size per page */
	uint numblocks;		/* Number of blocks */
	uint32 type;		/* Type */
	uint size;		/* Total size in Mbytes */
	uint8 id[8];
	uint32 base;
	uint32 phybase;

	uint sectorsize;	/* 512 or 1K */
	uint sparesize;		/* Spare size per sector */
	uint eccbytes;		/* ECC code bytes per sector */
	uint ecclevel0;		/* ECC algorithm for blocks 0 */
	uint ecclevel;		/* ECC algorithm for blocks other than block 0 */
	uint32 chipidx;		/* Current active chip index */
	uint32 width;		/* Device width 0/1 => 8/16 bit */

	si_t *sih;
	void *core;
	void *wrap;
	void (*enable)(hndnand_t *nfl, int enable);
	int (*read)(hndnand_t *nfl, uint64 offset, uint len, uchar *buf);
	int (*write)(hndnand_t *nfl, uint64 offset, uint len, const uchar *buf);
	int (*erase)(hndnand_t *nfl, uint64 offset);
	int (*checkbadb)(hndnand_t *nfl, uint64 offset);
	int (*markbadb)(hndnand_t *nfl, uint64 offset);
	int (*read_oob)(hndnand_t *nfl, uint64 addr, uint8 *oob);

#ifndef _CFE_
	int (*dev_ready)(hndnand_t *nfl);
	int (*select_chip)(hndnand_t *nfl, int chip);
	int (*cmdfunc)(hndnand_t *nfl, uint64 addr, int cmd);
	int (*waitfunc)(hndnand_t *nfl, int *status);
	int (*write_oob)(hndnand_t *nfl, uint64 addr, uint8 *oob);
	int (*read_page)(hndnand_t *nfl, uint64 addr, uint8 *buf, uint8 *oob, bool ecc,
		uint32 *herr, uint32 *serr);
	int (*write_page)(hndnand_t *nfl, uint64 addr, const uint8 *buf, uint8 *oob, bool ecc);
	int (*cmd_read_byte)(hndnand_t *nfl, int cmd, int arg);
#endif /* !_CFE_ */
};

hndnand_t *hndnand_init(si_t *sih);
void hndnand_enable(hndnand_t *nfl, int enable);
int hndnand_read(hndnand_t *nfl, uint64 offset, uint len, uchar *buf);
int hndnand_write(hndnand_t *nfl, uint64 offset, uint len, const uchar *buf);
int hndnand_erase(hndnand_t *nfl, uint64 offset);
int hndnand_checkbadb(hndnand_t *nfl, uint64 offset);
int hndnand_mark_badb(hndnand_t *nfl, uint64 offset);
int hndnand_read_oob(hndnand_t *nfl, uint64 addr, uint8 *oob);

#ifndef _CFE_
int hndnand_dev_ready(hndnand_t *nfl);
int hndnand_select_chip(hndnand_t *nfl, int chip);
int hndnand_devcie_width(hndnand_t *nfl, int chip);
int hndnand_cmdfunc(hndnand_t *nfl, uint64 addr, int cmd);
int hndnand_waitfunc(hndnand_t *nfl, int *status);
int hndnand_write_oob(hndnand_t *nfl, uint64 addr, uint8 *oob);
int hndnand_read_page(hndnand_t *nfl, uint64 addr, uint8 *buf, uint8 *oob, bool ecc,
	uint32 *herr, uint32 *serr);
int hndnand_write_page(hndnand_t *nfl, uint64 addr, const uint8 *buf, uint8 *oob, bool ecc);
int hndnand_cmd_read_byte(hndnand_t *nfl, int cmd, int arg);
#endif /* !_CFE_ */

#endif /* _hndnand_h_ */
