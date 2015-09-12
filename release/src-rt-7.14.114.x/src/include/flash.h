/*
 * flash.h: Common definitions for flash access.
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
 * $Id: flash.h 281527 2011-09-02 17:12:53Z $
 */

/* FILE-CSTYLED Cannot figure out how to make the initialization continuation lines acceptable */

/* Types of flashes we know about */
typedef enum _flash_type {OLD, BSC, SCS, AMD, SST, SFLASH} flash_type_t;

/* Commands to write/erase the flases */
typedef struct _flash_cmds{
	flash_type_t	type;
	bool		need_unlock;
	uint16		pre_erase;
	uint16		erase_block;
	uint16		erase_chip;
	uint16		write_word;
	uint16		write_buf;
	uint16		clear_csr;
	uint16		read_csr;
	uint16		read_id;
	uint16		confirm;
	uint16		read_array;
} flash_cmds_t;

#define	UNLOCK_CMD_WORDS	2	/* 2 words per command */

typedef struct _unlock_cmd {
	uint		addr[UNLOCK_CMD_WORDS];
	uint16	cmd[UNLOCK_CMD_WORDS];
} unlock_cmd_t;

/* Flash descriptors */
typedef struct _flash_desc {
	uint16		mfgid;		/* Manufacturer Id */
	uint16		devid;		/* Device Id */
	uint		size;		/* Total size in bytes */
	uint		width;		/* Device width in bytes */
	flash_type_t	type;		/* Device type old, S, J */
	uint		bsize;		/* Block size */
	uint		nb;		/* Number of blocks */
	uint		ff;		/* First full block */
	uint		lf;		/* Last full block */
	uint		nsub;		/* Number of subblocks */
	uint		*subblocks;	/* Offsets for subblocks */
	char		*desc;		/* Description */
} flash_desc_t;


#ifdef	DECLARE_FLASHES
flash_cmds_t sflash_cmd_t =
	{ SFLASH, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#ifdef NFLASH_SUPPORT
flash_cmds_t nflash_cmd_t =
	{ NFLASH, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#endif /* NFLASH_SUPPORT */

flash_cmds_t flash_cmds[] = {
/*	  type	needu	preera	eraseb	erasech	write	wbuf	clcsr	rdcsr	rdid
 *	  confrm	read
 */
	{ BSC,	0,	0x00,	0x20,	0x00,	0x40,	0x00,	0x50,	0x70,	0x90,
	  0xd0,	0xff },
	{ SCS,	0,	0x00,	0x20,	0x00,	0x40,	0xe8,	0x50,	0x70,	0x90,
	  0xd0,	0xff },
	{ AMD,	1,	0x80,	0x30,	0x10,	0xa0,	0x00,	0x00,	0x00,	0x90,
	  0x00,	0xf0 },
	{ SST,	1,	0x80,	0x50,	0x10,	0xa0,	0x00,	0x00,	0x00,	0x90,
	  0x00,	0xf0 },
	{ 0 }
};

unlock_cmd_t unlock_cmd_amd = {
#ifdef MIPSEB
#ifdef	BCMHND74K
/* addr: */	{ 0x0aac,	0x0552},
#else	/* !74K, bcm33xx */
/* addr: */	{ 0x0aa8,	0x0556},
#endif	/* BCMHND74K */
#else
/* addr: */	{ 0x0aaa,	0x0554},
#endif
/* data: */	{ 0xaa,		0x55}
};

unlock_cmd_t unlock_cmd_sst = {
#ifdef MIPSEB
#ifdef	BCMHND74K
/* addr: */	{ 0xaaac,	0x5552},
#else	/* !74K, bcm33xx */
/* addr: */	{ 0xaaa8,	0x5556},
#endif	/* BCMHND74K */
#else
/* addr: */	{ 0xaaaa,	0x5554},
#endif
/* data: */	{ 0xaa,		0x55}
};

#define AMD_CMD	0xaaa
#define SST_CMD 0xaaaa

/* intel unlock block cmds */
#define INTEL_UNLOCK1	0x60
#define INTEL_UNLOCK2	0xD0

/* Just eight blocks of 8KB byte each */

uint blk8x8k[] = {
	0x00000000,
	0x00002000,
	0x00004000,
	0x00006000,
	0x00008000,
	0x0000a000,
	0x0000c000,
	0x0000e000,
	0x00010000
};

/* Funky AMD arrangement for 29xx800's */
uint amd800[] = {
	0x00000000,		/* 16KB */
	0x00004000,		/* 32KB */
	0x0000c000,		/* 8KB */
	0x0000e000,		/* 8KB */
	0x00010000,		/* 8KB */
	0x00012000,		/* 8KB */
	0x00014000,		/* 32KB */
	0x0001c000,		/* 16KB */
	0x00020000
};

/* AMD arrangement for 29xx160's */
uint amd4112[] = {
	0x00000000,		/* 32KB */
	0x00008000,		/* 8KB */
	0x0000a000,		/* 8KB */
	0x0000c000,		/* 16KB */
	0x00010000
};

uint amd2114[] = {
	0x00000000,		/* 16KB */
	0x00004000,		/* 8KB */
	0x00006000,		/* 8KB */
	0x00008000,		/* 32KB */
	0x00010000
};


flash_desc_t sflash_desc =
	{ 0, 0, 0, 0, SFLASH, 0, 0, 0, 0, 0, NULL, "SFLASH" };

#ifdef NFLASH_SUPPORT
flash_desc_t nflash_desc =
	{ 0, 0, 0, 0, NFLASH, 0, 0, 0, 0, 0, NULL, "NFLASH" };
#endif /* NFLASH_SUPPORT */

flash_desc_t flashes[] = {
	{ 0x00b0, 0x00d0, 0x0200000, 2,	SCS, 0x10000, 32,  0, 31,  0, NULL,
	  "Intel 28F160S3/5 1Mx16" },
	{ 0x00b0, 0x00d4, 0x0400000, 2,	SCS, 0x10000, 64,  0, 63,  0, NULL,
	  "Intel 28F320S3/5 2Mx16" },
	{ 0x0089, 0x8890, 0x0200000, 2,	BSC, 0x10000, 32,  0, 30,  8, blk8x8k,
	  "Intel 28F160B3 1Mx16 TopB" },
	{ 0x0089, 0x8891, 0x0200000, 2,	BSC, 0x10000, 32,  1, 31,  8, blk8x8k,
	  "Intel 28F160B3 1Mx16 BotB" },
	{ 0x0089, 0x8896, 0x0400000, 2,	BSC, 0x10000, 64,  0, 62,  8, blk8x8k,
	  "Intel 28F320B3 2Mx16 TopB" },
	{ 0x0089, 0x8897, 0x0400000, 2,	BSC, 0x10000, 64,  1, 63,  8, blk8x8k,
	  "Intel 28F320B3 2Mx16 BotB" },
	{ 0x0089, 0x8898, 0x0800000, 2,	BSC, 0x10000, 128, 0, 126, 8, blk8x8k,
	  "Intel 28F640B3 4Mx16 TopB" },
	{ 0x0089, 0x8899, 0x0800000, 2,	BSC, 0x10000, 128, 1, 127, 8, blk8x8k,
	  "Intel 28F640B3 4Mx16 BotB" },
	{ 0x0089, 0x88C2, 0x0200000, 2,	BSC, 0x10000, 32,  0, 30,  8, blk8x8k,
	  "Intel 28F160C3 1Mx16 TopB" },
	{ 0x0089, 0x88C3, 0x0200000, 2,	BSC, 0x10000, 32,  1, 31,  8, blk8x8k,
	  "Intel 28F160C3 1Mx16 BotB" },
	{ 0x0089, 0x88C4, 0x0400000, 2,	BSC, 0x10000, 64,  0, 62,  8, blk8x8k,
	  "Intel 28F320C3 2Mx16 TopB" },
	{ 0x0089, 0x88C5, 0x0400000, 2,	BSC, 0x10000, 64,  1, 63,  8, blk8x8k,
	  "Intel 28F320C3 2Mx16 BotB" },
	{ 0x0089, 0x88CC, 0x0800000, 2,	BSC, 0x10000, 128, 0, 126, 8, blk8x8k,
	  "Intel 28F640C3 4Mx16 TopB" },
	{ 0x0089, 0x88CD, 0x0800000, 2,	BSC, 0x10000, 128, 1, 127, 8, blk8x8k,
	  "Intel 28F640C3 4Mx16 BotB" },
	{ 0x0089, 0x0014, 0x0400000, 2,	SCS, 0x20000, 32,  0, 31,  0, NULL,
	  "Intel 28F320J5 2Mx16" },
	{ 0x0089, 0x0015, 0x0800000, 2,	SCS, 0x20000, 64,  0, 63,  0, NULL,
	  "Intel 28F640J5 4Mx16" },
	{ 0x0089, 0x0016, 0x0400000, 2,	SCS, 0x20000, 32,  0, 31,  0, NULL,
	  "Intel 28F320J3 2Mx16" },
	{ 0x0089, 0x0017, 0x0800000, 2,	SCS, 0x20000, 64,  0, 63,  0, NULL,
	  "Intel 28F640J3 4Mx16" },
	{ 0x0089, 0x0018, 0x1000000, 2,	SCS, 0x20000, 128, 0, 127, 0, NULL,
	  "Intel 28F128J3 8Mx16" },
	{ 0x00b0, 0x00e3, 0x0400000, 2,	BSC, 0x10000, 64,  1, 63,  8, blk8x8k,
	  "Sharp 28F320BJE 2Mx16 BotB" },
	{ 0x0001, 0x224a, 0x0100000, 2,	AMD, 0x10000, 16,  0, 13,  8, amd800,
	  "AMD 29DL800BT 512Kx16 TopB" },
	{ 0x0001, 0x22cb, 0x0100000, 2,	AMD, 0x10000, 16,  2, 15,  8, amd800,
	  "AMD 29DL800BB 512Kx16 BotB" },
	{ 0x0001, 0x22c4, 0x0200000, 2,	AMD, 0x10000, 32,  0, 30,  4, amd4112,
	  "AMD 29W160ET 1Mx16 TopB" },
	{ 0x0001, 0x2249, 0x0200000, 2,	AMD, 0x10000, 32,  1, 31,  4, amd2114,
	  "AMD 29lv160DB 1Mx16 BotB" },
	{ 0x0001, 0x22f6, 0x0400000, 2,	AMD, 0x10000, 64,  0, 62,  8, blk8x8k,
	  "AMD 29LV320DT 2Mx16 TopB" },
	{ 0x0001, 0x22f9, 0x0400000, 2,	AMD, 0x10000, 64,  1, 63,  8, blk8x8k,
	  "AMD 29lv320DB 2Mx16 BotB" },
	{ 0x0001, 0x00f9, 0x0400000, 2,	AMD, 0x10000, 64,  1, 63,  8, blk8x8k,
	  "AMD 29lv320DB 2Mx16 BotB in BYTE mode" },
	{ 0x0001, 0x0201, 0x0800000, 2,	AMD, 0x10000, 128,  1, 126,  8, blk8x8k,
	  "AMD 29DL640D 4Mx16" },
	{ 0x0001, 0x1200, 0x01000000, 2, AMD, 0x10000, 256,  0, 255,  0, NULL,
	  "AMD 29LV128MH/L 8Mx16" },
	{ 0x0001, 0x1301, 0x0800000, 2, AMD, 0x10000, 128,  0, 127,  0, NULL,
	  "AMD 29LV641MT 4Mx16" },
	{ 0x0001, 0x1a01, 0x0400000, 2,	AMD, 0x10000, 64,  0, 62,  8, blk8x8k,
	  "AMD 29lv320MT 2Mx16 TopB" },
	{ 0x0001, 0x1a00, 0x0400000, 2,	AMD, 0x10000, 64,  1, 63,  8, blk8x8k,
	  "AMD 29lv320MB 2Mx16 BotB" },
	{ 0x0001, 0x1001, 0x0800000, 2,	AMD, 0x10000, 128,  0, 126,  8, blk8x8k,
	  "Spansion S29GL064A-R3 4Mx16 TopB" },
	{ 0x0001, 0x1000, 0x0800000, 2,	AMD, 0x10000, 128,  1, 127,  8, blk8x8k,
	  "Spansion S29GL064A-R4 4Mx16 BotB" },
	{ 0x0001, 0x0c01, 0x0800000, 2,	AMD, 0x10000, 128,  0, 127,  0, NULL,
	  "Spansion S29GL640A 8Mx16" },
	{ 0x0001, 0x2201, 0x2000000, 2, AMD, 0x20000, 256,  0, 255,  0, NULL,
	  "Spansion S29GL256P 16Mx16" },
	{ 0x0020, 0x22CA, 0x0400000, 2,	AMD, 0x10000, 64,  0, 62,  4, amd4112,
	  "ST 29w320DT 2Mx16 TopB" },
	{ 0x0020, 0x22CB, 0x0400000, 2,	AMD, 0x10000, 64,  1, 63,  4, amd2114,
	  "ST 29w320DB 2Mx16 BotB" },
	{ 0x0020, 0x22c4, 0x0200000, 2,	AMD, 0x10000, 32,  0, 30,  4, amd4112,
	  "ST 29w160ET 1Mx16 TopB" },
	{ 0x0020, 0x2249, 0x0200000, 2,	AMD, 0x10000, 32,  1, 31,  4, amd2114,
	  "ST 29w160ET 1Mx16 BotB" },
	{ 0x0020, 0x225d, 0x0400000, 2,	AMD, 0x10000, 64,  1, 63,  8, blk8x8k,
	  "ST M29DW324DB 2Mx16 TopB" },
	{ 0x007f, 0x22f9, 0x0400000, 2,	AMD, 0x10000, 64,  1, 63,  8, blk8x8k,
	  "EON 29LV320CB 2Mx16 BotB" },
	{ 0x00C2, 0x22c4, 0x0200000, 2,	AMD, 0x10000, 32,  0, 30,  4, amd4112,
	  "MX 29LV160CT 1Mx16 TopB" },
	{ 0x00C2, 0x2249, 0x0200000, 2,	AMD, 0x10000, 32,  1, 31,  4, amd2114,
	  "MX 29LV160CB 1Mx16 BotB" },
	{ 0x00C2, 0x22a8, 0x0400000, 2,	AMD, 0x10000, 64,  1, 63,  8, blk8x8k,
	  "MX 29LV320CB 2Mx16 BotB" },
	{ 0x00C2, 0x00A7, 0x0400000, 2,	AMD, 0x10000, 64,  0, 62,  4, amd4112,
	  "MX29LV320T 2Mx16 TopB" },
	{ 0x00C2, 0x00A8, 0x0400000, 2,	AMD, 0x10000, 64,  1, 63,  4, amd2114,
	  "MX29LV320B 2Mx16 BotB" },
	{ 0x0004, 0x22F6, 0x0400000, 2,	AMD, 0x10000, 64,  0, 62,  4, amd4112,
	  "MBM29LV320TE 2Mx16 TopB" },
	{ 0x0004, 0x22F9, 0x0400000, 2,	AMD, 0x10000, 64,  1, 63,  4, amd2114,
	  "MBM29LV320BE 2Mx16 BotB" },
	{ 0x0098, 0x009A, 0x0400000, 2,	AMD, 0x10000, 64,  0, 62,  4, amd4112,
	  "TC58FVT321 2Mx16 TopB" },
	{ 0x0098, 0x009C, 0x0400000, 2,	AMD, 0x10000, 64,  1, 63,  4, amd2114,
	  "TC58FVB321 2Mx16 BotB" },
	{ 0x00C2, 0x22A7, 0x0400000, 2,	AMD, 0x10000, 64,  0, 62,  4, amd4112,
	  "MX29LV320T 2Mx16 TopB" },
	{ 0x00C2, 0x22A8, 0x0400000, 2,	AMD, 0x10000, 64,  1, 63,  4, amd2114,
	  "MX29LV320B 2Mx16 BotB" },
	{ 0x00BF, 0x2783, 0x0400000, 2,	SST, 0x10000, 64,  0, 63,  0, NULL,
	  "SST39VF320 2Mx16" },
	{ 0x00ec, 0x22e2, 0x0800000, 2, AMD, 0x10000, 128,  1, 127,  8, blk8x8k,
	  "Samsung K8D631UB 4Mx16 BotB" },
	{ 0xddda, 0x0a00, 0x0400000, 2, AMD, 0x10000, 64,  1, 63,  8, blk8x8k,
	  "Samsung K8D631UB 4Mx16 BotB" },
	{ 0,      0,      0,         0,	OLD, 0,       0,   0, 0,   0, NULL,
	  NULL },
};

#else	/* !DECLARE_FLASHES */

extern flash_cmds_t flash_cmds[];
extern unlock_cmd_t unlock_cmd;
extern flash_desc_t flashes[];

#endif	/* DECLARE_FLASHES */
