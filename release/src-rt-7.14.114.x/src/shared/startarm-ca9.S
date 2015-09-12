/*
 * BCM53XXX ARM/Cortext A9 boot code for standalone apps.
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
 * $Id$
 */

#include <arminc.h>
#include <sbhndarm.h>
#include <hndrte_armtrap.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <bcmdevs.h>
#include <ddr_core.h>
#include <pcieg2_core.h>
#include <armca9_core.h>

#ifdef _CFE_

#include <armmacros.h>
#include "initdata.h"
#include "bsp_config.h"
#include "cfe_devfuncs.h"

#define c_main	cfe_main

/*
 * This is the size of the stack, rounded to KByte boundaries.
 */

#ifndef CFG_STACK_SIZE
#error "CFG_STACK_SIZE not defined"
#else
#define STACK_SIZE	((CFG_STACK_SIZE+1023) & ~1023)
#endif

#define envram_ofs	0x1000

#else /* !_CFE_ */

#if defined(__thumb__)
#define FUNC(x)	THUMBLEAF(x)
#else
#define FUNC(x)	LEAF(x)
#endif

/* Debug macro */
#ifdef TRACE
#undef TRACE
#endif
#ifdef BCMDBG
#define TRACE(x) \
	ldr	r9,=(x); \
	ldr     r10,=SI_ENUM_BASE; \
	str	r9,[r10,#0x64]
#else
#define TRACE(x)
#endif

#define envram_ofs	0x400

#if CFG_BIENDIAN
#define CFE_EPTSEAL_REV 0x31454643
#endif
#define CFE_EPTSEAL 0x43464531
#define cfe_command_restart 0

#endif /* _CFE_ */

#ifdef CFG_SHMOO
/* MMU table */
	.section section_mmu_table
page_table:
	.word	0x00004c06
	.word	0x00104c06
	.word	0x00204c06
	.word	0x00304c06
	.word	0x00404c06
	.word	0x00504c06
	.word	0x00604c06
	.word	0x00704c06
	.word	0x00804c06
	.word	0x00904c06
	.word	0x00a04c06
	.word	0x00b04c06
	.word	0x00c04c06
	.word	0x00d04c06
	.word	0x00e04c06
	.word	0x00f04c06
	.word	0x01004c06 @DDR_SHMOO_RELATED_CHANGE: make 16MB-32MB area as L1 cacheable write-back mode requested by shmoo memory test
	.word	0x01104c06 
	.word	0x01204c06 
	.word	0x01304c06
	.word	0x01404c06
	.word	0x01504c06
	.word	0x01604c06
	.word	0x01704c06
	.word	0x01804c06
	.word	0x01904c06
	.word	0x01a04c06
	.word	0x01b04c06
	.word	0x01c04c06
	.word	0x01d04c06
	.word	0x01e04c06
	.word	0x01f04c06 
	.word	0x02004c06 @DDR_SHMOO_RELATED_CHANGE: make 32MB-128MB area as L1&L2 cacheable write-back mode requested by stress memory test
	.word	0x02104c06 
	.word	0x02204c06 
	.word	0x02304c06 
	.word	0x02404c06
	.word	0x02504c06
	.word	0x02604c06
	.word	0x02704c06
	.word	0x02804c06
	.word	0x02904c06
	.word	0x02a04c06
	.word	0x02b04c06
	.word	0x02c04c06
	.word	0x02d04c06
	.word	0x02e04c06
	.word	0x02f04c06
	.word	0x03004c06
	.word	0x03104c06
	.word	0x03204c06
	.word	0x03304c06
	.word	0x03404c06
	.word	0x03504c06
	.word	0x03604c06
	.word	0x03704c06
	.word	0x03804c06
	.word	0x03904c06
	.word	0x03a04c06
	.word	0x03b04c06
	.word	0x03c04c06
	.word	0x03d04c06
	.word	0x03e04c06
	.word	0x03f04c06
	.word	0x04004c06
	.word	0x04104c06
	.word	0x04204c06
	.word	0x04304c06
	.word	0x04404c06
	.word	0x04504c06
	.word	0x04604c06
	.word	0x04704c06
	.word	0x04804c06
	.word	0x04904c06
	.word	0x04a04c06
	.word	0x04b04c06
	.word	0x04c04c06
	.word	0x04d04c06
	.word	0x04e04c06
	.word	0x04f04c06
	.word	0x05004c06
	.word	0x05104c06
	.word	0x05204c06
	.word	0x05304c06
	.word	0x05404c06
	.word	0x05504c06
	.word	0x05604c06
	.word	0x05704c06
	.word	0x05804c06
	.word	0x05904c06
	.word	0x05a04c06
	.word	0x05b04c06
	.word	0x05c04c06
	.word	0x05d04c06
	.word	0x05e04c06
	.word	0x05f04c06
	.word	0x06004c06
	.word	0x06104c06
	.word	0x06204c06
	.word	0x06304c06
	.word	0x06404c06
	.word	0x06504c06
	.word	0x06604c06
	.word	0x06704c06
	.word	0x06804c06
	.word	0x06904c06
	.word	0x06a04c06
	.word	0x06b04c06
	.word	0x06c04c06
	.word	0x06d04c06
	.word	0x06e04c06
	.word	0x06f04c06
	.word	0x07004c06
	.word	0x07104c06
	.word	0x07204c06
	.word	0x07304c06
	.word	0x07404c06
	.word	0x07504c06
	.word	0x07604c06
	.word	0x07704c06
	.word	0x07804c06
	.word	0x07904c06
	.word	0x07a04c06
	.word	0x07b04c06
	.word	0x07c04c06
	.word	0x07d04c06
	.word	0x07e04c06
	.word	0x07f04c06 @ DDR_SHMOO_RELATED_CHANGE: end of L1&L2 cacheable write-back mode
	.word	0x08002c02
	.word	0x08102c02
	.word	0x08202c02
	.word	0x08302c02
	.word	0x08402c02
	.word	0x08502c02
	.word	0x08602c02
	.word	0x08702c02
	.word	0x08802c02
	.word	0x08902c02
	.word	0x08a02c02
	.word	0x08b02c02
	.word	0x08c02c02
	.word	0x08d02c02
	.word	0x08e02c02
	.word	0x08f02c02
	.word	0x09002c02
	.word	0x09102c02
	.word	0x09202c02
	.word	0x09302c02
	.word	0x09402c02
	.word	0x09502c02
	.word	0x09602c02
	.word	0x09702c02
	.word	0x09802c02
	.word	0x09902c02
	.word	0x09a02c02
	.word	0x09b02c02
	.word	0x09c02c02
	.word	0x09d02c02
	.word	0x09e02c02
	.word	0x09f02c02
	.word	0x0a002c02
	.word	0x0a102c02
	.word	0x0a202c02
	.word	0x0a302c02
	.word	0x0a402c02
	.word	0x0a502c02
	.word	0x0a602c02
	.word	0x0a702c02
	.word	0x0a802c02
	.word	0x0a902c02
	.word	0x0aa02c02
	.word	0x0ab02c02
	.word	0x0ac02c02
	.word	0x0ad02c02
	.word	0x0ae02c02
	.word	0x0af02c02
	.word	0x0b002c02
	.word	0x0b102c02
	.word	0x0b202c02
	.word	0x0b302c02
	.word	0x0b402c02
	.word	0x0b502c02
	.word	0x0b602c02
	.word	0x0b702c02
	.word	0x0b802c02
	.word	0x0b902c02
	.word	0x0ba02c02
	.word	0x0bb02c02
	.word	0x0bc02c02
	.word	0x0bd02c02
	.word	0x0be02c02
	.word	0x0bf02c02
	.word	0x0c002c02
	.word	0x0c102c02
	.word	0x0c202c02
	.word	0x0c302c02
	.word	0x0c402c02
	.word	0x0c502c02
	.word	0x0c602c02
	.word	0x0c702c02
	.word	0x0c802c02
	.word	0x0c902c02
	.word	0x0ca02c02
	.word	0x0cb02c02
	.word	0x0cc02c02
	.word	0x0cd02c02
	.word	0x0ce02c02
	.word	0x0cf02c02
	.word	0x0d002c02
	.word	0x0d102c02
	.word	0x0d202c02
	.word	0x0d302c02
	.word	0x0d402c02
	.word	0x0d502c02
	.word	0x0d602c02
	.word	0x0d702c02
	.word	0x0d802c02
	.word	0x0d902c02
	.word	0x0da02c02
	.word	0x0db02c02
	.word	0x0dc02c02
	.word	0x0dd02c02
	.word	0x0de02c02
	.word	0x0df02c02
	.word	0x0e002c02
	.word	0x0e102c02
	.word	0x0e202c02
	.word	0x0e302c02
	.word	0x0e402c02
	.word	0x0e502c02
	.word	0x0e602c02
	.word	0x0e702c02
	.word	0x0e802c02
	.word	0x0e902c02
	.word	0x0ea02c02
	.word	0x0eb02c02
	.word	0x0ec02c02
	.word	0x0ed02c02
	.word	0x0ee02c02
	.word	0x0ef02c02
	.word	0x0f002c02
	.word	0x0f102c02
	.word	0x0f202c02
	.word	0x0f302c02
	.word	0x0f402c02
	.word	0x0f502c02
	.word	0x0f602c02
	.word	0x0f702c02
	.word	0x0f802c02
	.word	0x0f902c02
	.word	0x0fa02c02
	.word	0x0fb02c02
	.word	0x0fc02c02
	.word	0x0fd02c02
	.word	0x0fe02c02
	.word	0x0ff02c02
	.word	0x10002c02
	.word	0x10102c02
	.word	0x10202c02
	.word	0x10302c02
	.word	0x10402c02
	.word	0x10502c02
	.word	0x10602c02
	.word	0x10702c02
	.word	0x10802c02
	.word	0x10902c02
	.word	0x10a02c02
	.word	0x10b02c02
	.word	0x10c02c02
	.word	0x10d02c02
	.word	0x10e02c02
	.word	0x10f02c02
	.word	0x11002c02
	.word	0x11102c02
	.word	0x11202c02
	.word	0x11302c02
	.word	0x11402c02
	.word	0x11502c02
	.word	0x11602c02
	.word	0x11702c02
	.word	0x11802c02
	.word	0x11902c02
	.word	0x11a02c02
	.word	0x11b02c02
	.word	0x11c02c02
	.word	0x11d02c02
	.word	0x11e02c02
	.word	0x11f02c02
	.word	0x12002c02
	.word	0x12102c02
	.word	0x12202c02
	.word	0x12302c02
	.word	0x12402c02
	.word	0x12502c02
	.word	0x12602c02
	.word	0x12702c02
	.word	0x12802c02
	.word	0x12902c02
	.word	0x12a02c02
	.word	0x12b02c02
	.word	0x12c02c02
	.word	0x12d02c02
	.word	0x12e02c02
	.word	0x12f02c02
	.word	0x13002c02
	.word	0x13102c02
	.word	0x13202c02
	.word	0x13302c02
	.word	0x13402c02
	.word	0x13502c02
	.word	0x13602c02
	.word	0x13702c02
	.word	0x13802c02
	.word	0x13902c02
	.word	0x13a02c02
	.word	0x13b02c02
	.word	0x13c02c02
	.word	0x13d02c02
	.word	0x13e02c02
	.word	0x13f02c02
	.word	0x14002c02
	.word	0x14102c02
	.word	0x14202c02
	.word	0x14302c02
	.word	0x14402c02
	.word	0x14502c02
	.word	0x14602c02
	.word	0x14702c02
	.word	0x14802c02
	.word	0x14902c02
	.word	0x14a02c02
	.word	0x14b02c02
	.word	0x14c02c02
	.word	0x14d02c02
	.word	0x14e02c02
	.word	0x14f02c02
	.word	0x15002c02
	.word	0x15102c02
	.word	0x15202c02
	.word	0x15302c02
	.word	0x15402c02
	.word	0x15502c02
	.word	0x15602c02
	.word	0x15702c02
	.word	0x15802c02
	.word	0x15902c02
	.word	0x15a02c02
	.word	0x15b02c02
	.word	0x15c02c02
	.word	0x15d02c02
	.word	0x15e02c02
	.word	0x15f02c02
	.word	0x16002c02
	.word	0x16102c02
	.word	0x16202c02
	.word	0x16302c02
	.word	0x16402c02
	.word	0x16502c02
	.word	0x16602c02
	.word	0x16702c02
	.word	0x16802c02
	.word	0x16902c02
	.word	0x16a02c02
	.word	0x16b02c02
	.word	0x16c02c02
	.word	0x16d02c02
	.word	0x16e02c02
	.word	0x16f02c02
	.word	0x17002c02
	.word	0x17102c02
	.word	0x17202c02
	.word	0x17302c02
	.word	0x17402c02
	.word	0x17502c02
	.word	0x17602c02
	.word	0x17702c02
	.word	0x17802c02
	.word	0x17902c02
	.word	0x17a02c02
	.word	0x17b02c02
	.word	0x17c02c02
	.word	0x17d02c02
	.word	0x17e02c02
	.word	0x17f02c02
	.word	0x18002c02
	.word	0x18102c02
	.word	0x18202c02
	.word	0x18302c02
	.word	0x18402c02
	.word	0x18502c02
	.word	0x18602c02
	.word	0x18702c02
	.word	0x18802c02
	.word	0x18902c02
	.word	0x18a02c02
	.word	0x18b02c02
	.word	0x18c02c02
	.word	0x18d02c02
	.word	0x18e02c02
	.word	0x18f02c02
	.word	0x19002c02
	.word	0x19102c02
	.word	0x19202c02
	.word	0x19302c02
	.word	0x19402c02
	.word	0x19502c02
	.word	0x19602c02
	.word	0x19702c02
	.word	0x19802c02
	.word	0x19902c02
	.word	0x19a02c02
	.word	0x19b02c02
	.word	0x19c02c02
	.word	0x19d02c02
	.word	0x19e02c02
	.word	0x19f02c02
	.word	0x1a002c02
	.word	0x1a102c02
	.word	0x1a202c02
	.word	0x1a302c02
	.word	0x1a402c02
	.word	0x1a502c02
	.word	0x1a602c02
	.word	0x1a702c02
	.word	0x1a802c02
	.word	0x1a902c02
	.word	0x1aa02c02
	.word	0x1ab02c02
	.word	0x1ac02c02
	.word	0x1ad02c02
	.word	0x1ae02c02
	.word	0x1af02c02
	.word	0x1b002c02
	.word	0x1b102c02
	.word	0x1b202c02
	.word	0x1b302c02
	.word	0x1b402c02
	.word	0x1b502c02
	.word	0x1b602c02
	.word	0x1b702c02
	.word	0x1b802c02
	.word	0x1b902c02
	.word	0x1ba02c02
	.word	0x1bb02c02
	.word	0x1bc02c02
	.word	0x1bd02c02
	.word	0x1be02c02
	.word	0x1bf02c02
	.word	0x1c002c02   @ bcm95301x_svk_iproc_nand_config: make NAND flash code L1&L2 cachable
	.word	0x1c102c02
	.word	0x1c202c02
	.word	0x1c302c02
	.word	0x1c402c02
	.word	0x1c502c02
	.word	0x1c602c02
	.word	0x1c702c02
	.word	0x1c802c02
	.word	0x1c902c02
	.word	0x1ca02c02
	.word	0x1cb02c02
	.word	0x1cc02c02
	.word	0x1cd02c02
	.word	0x1ce02c02
	.word	0x1cf02c02
	.word	0x1d002c02
	.word	0x1d102c02
	.word	0x1d202c02
	.word	0x1d302c02
	.word	0x1d402c02
	.word	0x1d502c02
	.word	0x1d602c02
	.word	0x1d702c02
	.word	0x1d802c02
	.word	0x1d902c02
	.word	0x1da02c02
	.word	0x1db02c02
	.word	0x1dc02c02
	.word	0x1dd02c02
	.word	0x1de02c02
	.word	0x1df02c02
	.word	0x1e004c02  @DDR_SHMOO_RELATED_CHANGE: change 0x1e014ca2 to 0x1e016caa to make flash code L1&L2 cachable   
	.word	0x1e104c02
	.word	0x1e204c02
	.word	0x1e304c02
	.word	0x1e404c02
	.word	0x1e504c02
	.word	0x1e604c02
	.word	0x1e704c02
	.word	0x1e804c0a
	.word	0x1e904c0a
	.word	0x1ea04c0a
	.word	0x1eb04c0a
	.word	0x1ec04c0a
	.word	0x1ed04c0a
	.word	0x1ee04c0a
	.word	0x1ef04c0a
	.word	0x1f004c0a
	.word	0x1f104c0a
	.word	0x1f204c0a
	.word	0x1f304c0a
	.word	0x1f404c0a
	.word	0x1f504c0a
	.word	0x1f604c0a
	.word	0x1f704c0a
	.word	0x1f804c0a
	.word	0x1f904c0a
	.word	0x1fa04c0a
	.word	0x1fb04c0a
	.word	0x1fc04c0a
	.word	0x1fd04c0a
	.word	0x1fe04c0a
	.word	0x1ff04c0a
	.word	0x20002c02
	.word	0x20102c02
	.word	0x20202c02
	.word	0x20302c02
	.word	0x20402c02
	.word	0x20502c02
	.word	0x20602c02
	.word	0x20702c02
	.word	0x20802c02
	.word	0x20902c02
	.word	0x20a02c02
	.word	0x20b02c02
	.word	0x20c02c02
	.word	0x20d02c02
	.word	0x20e02c02
	.word	0x20f02c02
	.word	0x21002c02
	.word	0x21102c02
	.word	0x21202c02
	.word	0x21302c02
	.word	0x21402c02
	.word	0x21502c02
	.word	0x21602c02
	.word	0x21702c02
	.word	0x21802c02
	.word	0x21902c02
	.word	0x21a02c02
	.word	0x21b02c02
	.word	0x21c02c02
	.word	0x21d02c02
	.word	0x21e02c02
	.word	0x21f02c02
	.word	0x22002c02
	.word	0x22102c02
	.word	0x22202c02
	.word	0x22302c02
	.word	0x22402c02
	.word	0x22502c02
	.word	0x22602c02
	.word	0x22702c02
	.word	0x22802c02
	.word	0x22902c02
	.word	0x22a02c02
	.word	0x22b02c02
	.word	0x22c02c02
	.word	0x22d02c02
	.word	0x22e02c02
	.word	0x22f02c02
	.word	0x23002c02
	.word	0x23102c02
	.word	0x23202c02
	.word	0x23302c02
	.word	0x23402c02
	.word	0x23502c02
	.word	0x23602c02
	.word	0x23702c02
	.word	0x23802c02
	.word	0x23902c02
	.word	0x23a02c02
	.word	0x23b02c02
	.word	0x23c02c02
	.word	0x23d02c02
	.word	0x23e02c02
	.word	0x23f02c02
	.word	0x24002c02
	.word	0x24102c02
	.word	0x24202c02
	.word	0x24302c02
	.word	0x24402c02
	.word	0x24502c02
	.word	0x24602c02
	.word	0x24702c02
	.word	0x24802c02
	.word	0x24902c02
	.word	0x24a02c02
	.word	0x24b02c02
	.word	0x24c02c02
	.word	0x24d02c02
	.word	0x24e02c02
	.word	0x24f02c02
	.word	0x25002c02
	.word	0x25102c02
	.word	0x25202c02
	.word	0x25302c02
	.word	0x25402c02
	.word	0x25502c02
	.word	0x25602c02
	.word	0x25702c02
	.word	0x25802c02
	.word	0x25902c02
	.word	0x25a02c02
	.word	0x25b02c02
	.word	0x25c02c02
	.word	0x25d02c02
	.word	0x25e02c02
	.word	0x25f02c02
	.word	0x26002c02
	.word	0x26102c02
	.word	0x26202c02
	.word	0x26302c02
	.word	0x26402c02
	.word	0x26502c02
	.word	0x26602c02
	.word	0x26702c02
	.word	0x26802c02
	.word	0x26902c02
	.word	0x26a02c02
	.word	0x26b02c02
	.word	0x26c02c02
	.word	0x26d02c02
	.word	0x26e02c02
	.word	0x26f02c02
	.word	0x27002c02
	.word	0x27102c02
	.word	0x27202c02
	.word	0x27302c02
	.word	0x27402c02
	.word	0x27502c02
	.word	0x27602c02
	.word	0x27702c02
	.word	0x27802c02
	.word	0x27902c02
	.word	0x27a02c02
	.word	0x27b02c02
	.word	0x27c02c02
	.word	0x27d02c02
	.word	0x27e02c02
	.word	0x27f02c02
	.word	0x28002c02
	.word	0x28102c02
	.word	0x28202c02
	.word	0x28302c02
	.word	0x28402c02
	.word	0x28502c02
	.word	0x28602c02
	.word	0x28702c02
	.word	0x28802c02
	.word	0x28902c02
	.word	0x28a02c02
	.word	0x28b02c02
	.word	0x28c02c02
	.word	0x28d02c02
	.word	0x28e02c02
	.word	0x28f02c02
	.word	0x29002c02
	.word	0x29102c02
	.word	0x29202c02
	.word	0x29302c02
	.word	0x29402c02
	.word	0x29502c02
	.word	0x29602c02
	.word	0x29702c02
	.word	0x29802c02
	.word	0x29902c02
	.word	0x29a02c02
	.word	0x29b02c02
	.word	0x29c02c02
	.word	0x29d02c02
	.word	0x29e02c02
	.word	0x29f02c02
	.word	0x2a002c02
	.word	0x2a102c02
	.word	0x2a202c02
	.word	0x2a302c02
	.word	0x2a402c02
	.word	0x2a502c02
	.word	0x2a602c02
	.word	0x2a702c02
	.word	0x2a802c02
	.word	0x2a902c02
	.word	0x2aa02c02
	.word	0x2ab02c02
	.word	0x2ac02c02
	.word	0x2ad02c02
	.word	0x2ae02c02
	.word	0x2af02c02
	.word	0x2b002c02
	.word	0x2b102c02
	.word	0x2b202c02
	.word	0x2b302c02
	.word	0x2b402c02
	.word	0x2b502c02
	.word	0x2b602c02
	.word	0x2b702c02
	.word	0x2b802c02
	.word	0x2b902c02
	.word	0x2ba02c02
	.word	0x2bb02c02
	.word	0x2bc02c02
	.word	0x2bd02c02
	.word	0x2be02c02
	.word	0x2bf02c02
	.word	0x2c002c02
	.word	0x2c102c02
	.word	0x2c202c02
	.word	0x2c302c02
	.word	0x2c402c02
	.word	0x2c502c02
	.word	0x2c602c02
	.word	0x2c702c02
	.word	0x2c802c02
	.word	0x2c902c02
	.word	0x2ca02c02
	.word	0x2cb02c02
	.word	0x2cc02c02
	.word	0x2cd02c02
	.word	0x2ce02c02
	.word	0x2cf02c02
	.word	0x2d002c02
	.word	0x2d102c02
	.word	0x2d202c02
	.word	0x2d302c02
	.word	0x2d402c02
	.word	0x2d502c02
	.word	0x2d602c02
	.word	0x2d702c02
	.word	0x2d802c02
	.word	0x2d902c02
	.word	0x2da02c02
	.word	0x2db02c02
	.word	0x2dc02c02
	.word	0x2dd02c02
	.word	0x2de02c02
	.word	0x2df02c02
	.word	0x2e002c02
	.word	0x2e102c02
	.word	0x2e202c02
	.word	0x2e302c02
	.word	0x2e402c02
	.word	0x2e502c02
	.word	0x2e602c02
	.word	0x2e702c02
	.word	0x2e802c02
	.word	0x2e902c02
	.word	0x2ea02c02
	.word	0x2eb02c02
	.word	0x2ec02c02
	.word	0x2ed02c02
	.word	0x2ee02c02
	.word	0x2ef02c02
	.word	0x2f002c02
	.word	0x2f102c02
	.word	0x2f202c02
	.word	0x2f302c02
	.word	0x2f402c02
	.word	0x2f502c02
	.word	0x2f602c02
	.word	0x2f702c02
	.word	0x2f802c02
	.word	0x2f902c02
	.word	0x2fa02c02
	.word	0x2fb02c02
	.word	0x2fc02c02
	.word	0x2fd02c02
	.word	0x2fe02c02
	.word	0x2ff02c02
	.word	0x30002c02
	.word	0x30102c02
	.word	0x30202c02
	.word	0x30302c02
	.word	0x30402c02
	.word	0x30502c02
	.word	0x30602c02
	.word	0x30702c02
	.word	0x30802c02
	.word	0x30902c02
	.word	0x30a02c02
	.word	0x30b02c02
	.word	0x30c02c02
	.word	0x30d02c02
	.word	0x30e02c02
	.word	0x30f02c02
	.word	0x31002c02
	.word	0x31102c02
	.word	0x31202c02
	.word	0x31302c02
	.word	0x31402c02
	.word	0x31502c02
	.word	0x31602c02
	.word	0x31702c02
	.word	0x31802c02
	.word	0x31902c02
	.word	0x31a02c02
	.word	0x31b02c02
	.word	0x31c02c02
	.word	0x31d02c02
	.word	0x31e02c02
	.word	0x31f02c02
	.word	0x32002c02
	.word	0x32102c02
	.word	0x32202c02
	.word	0x32302c02
	.word	0x32402c02
	.word	0x32502c02
	.word	0x32602c02
	.word	0x32702c02
	.word	0x32802c02
	.word	0x32902c02
	.word	0x32a02c02
	.word	0x32b02c02
	.word	0x32c02c02
	.word	0x32d02c02
	.word	0x32e02c02
	.word	0x32f02c02
	.word	0x33002c02
	.word	0x33102c02
	.word	0x33202c02
	.word	0x33302c02
	.word	0x33402c02
	.word	0x33502c02
	.word	0x33602c02
	.word	0x33702c02
	.word	0x33802c02
	.word	0x33902c02
	.word	0x33a02c02
	.word	0x33b02c02
	.word	0x33c02c02
	.word	0x33d02c02
	.word	0x33e02c02
	.word	0x33f02c02
	.word	0x34002c02
	.word	0x34102c02
	.word	0x34202c02
	.word	0x34302c02
	.word	0x34402c02
	.word	0x34502c02
	.word	0x34602c02
	.word	0x34702c02
	.word	0x34802c02
	.word	0x34902c02
	.word	0x34a02c02
	.word	0x34b02c02
	.word	0x34c02c02
	.word	0x34d02c02
	.word	0x34e02c02
	.word	0x34f02c02
	.word	0x35002c02
	.word	0x35102c02
	.word	0x35202c02
	.word	0x35302c02
	.word	0x35402c02
	.word	0x35502c02
	.word	0x35602c02
	.word	0x35702c02
	.word	0x35802c02
	.word	0x35902c02
	.word	0x35a02c02
	.word	0x35b02c02
	.word	0x35c02c02
	.word	0x35d02c02
	.word	0x35e02c02
	.word	0x35f02c02
	.word	0x36002c02
	.word	0x36102c02
	.word	0x36202c02
	.word	0x36302c02
	.word	0x36402c02
	.word	0x36502c02
	.word	0x36602c02
	.word	0x36702c02
	.word	0x36802c02
	.word	0x36902c02
	.word	0x36a02c02
	.word	0x36b02c02
	.word	0x36c02c02
	.word	0x36d02c02
	.word	0x36e02c02
	.word	0x36f02c02
	.word	0x37002c02
	.word	0x37102c02
	.word	0x37202c02
	.word	0x37302c02
	.word	0x37402c02
	.word	0x37502c02
	.word	0x37602c02
	.word	0x37702c02
	.word	0x37802c02
	.word	0x37902c02
	.word	0x37a02c02
	.word	0x37b02c02
	.word	0x37c02c02
	.word	0x37d02c02
	.word	0x37e02c02
	.word	0x37f02c02
	.word	0x38002c02
	.word	0x38102c02
	.word	0x38202c02
	.word	0x38302c02
	.word	0x38402c02
	.word	0x38502c02
	.word	0x38602c02
	.word	0x38702c02
	.word	0x38802c02
	.word	0x38902c02
	.word	0x38a02c02
	.word	0x38b02c02
	.word	0x38c02c02
	.word	0x38d02c02
	.word	0x38e02c02
	.word	0x38f02c02
	.word	0x39002c02
	.word	0x39102c02
	.word	0x39202c02
	.word	0x39302c02
	.word	0x39402c02
	.word	0x39502c02
	.word	0x39602c02
	.word	0x39702c02
	.word	0x39802c02
	.word	0x39902c02
	.word	0x39a02c02
	.word	0x39b02c02
	.word	0x39c02c02
	.word	0x39d02c02
	.word	0x39e02c02
	.word	0x39f02c02
	.word	0x3a002c02
	.word	0x3a102c02
	.word	0x3a202c02
	.word	0x3a302c02
	.word	0x3a402c02
	.word	0x3a502c02
	.word	0x3a602c02
	.word	0x3a702c02
	.word	0x3a802c02
	.word	0x3a902c02
	.word	0x3aa02c02
	.word	0x3ab02c02
	.word	0x3ac02c02
	.word	0x3ad02c02
	.word	0x3ae02c02
	.word	0x3af02c02
	.word	0x3b002c02
	.word	0x3b102c02
	.word	0x3b202c02
	.word	0x3b302c02
	.word	0x3b402c02
	.word	0x3b502c02
	.word	0x3b602c02
	.word	0x3b702c02
	.word	0x3b802c02
	.word	0x3b902c02
	.word	0x3ba02c02
	.word	0x3bb02c02
	.word	0x3bc02c02
	.word	0x3bd02c02
	.word	0x3be02c02
	.word	0x3bf02c02
	.word	0x3c002c02
	.word	0x3c102c02
	.word	0x3c202c02
	.word	0x3c302c02
	.word	0x3c402c02
	.word	0x3c502c02
	.word	0x3c602c02
	.word	0x3c702c02
	.word	0x3c802c02
	.word	0x3c902c02
	.word	0x3ca02c02
	.word	0x3cb02c02
	.word	0x3cc02c02
	.word	0x3cd02c02
	.word	0x3ce02c02
	.word	0x3cf02c02
	.word	0x3d002c02
	.word	0x3d102c02
	.word	0x3d202c02
	.word	0x3d302c02
	.word	0x3d402c02
	.word	0x3d502c02
	.word	0x3d602c02
	.word	0x3d702c02
	.word	0x3d802c02
	.word	0x3d902c02
	.word	0x3da02c02
	.word	0x3db02c02
	.word	0x3dc02c02
	.word	0x3dd02c02
	.word	0x3de02c02
	.word	0x3df02c02
	.word	0x3e002c02
	.word	0x3e102c02
	.word	0x3e202c02
	.word	0x3e302c02
	.word	0x3e402c02
	.word	0x3e502c02
	.word	0x3e602c02
	.word	0x3e702c02
	.word	0x3e802c02
	.word	0x3e902c02
	.word	0x3ea02c02
	.word	0x3eb02c02
	.word	0x3ec02c02
	.word	0x3ed02c02
	.word	0x3ee02c02
	.word	0x3ef02c02
	.word	0x3f002c02
	.word	0x3f102c02
	.word	0x3f202c02
	.word	0x3f302c02
	.word	0x3f402c02
	.word	0x3f502c02
	.word	0x3f602c02
	.word	0x3f702c02
	.word	0x3f802c02
	.word	0x3f902c02
	.word	0x3fa02c02
	.word	0x3fb02c02
	.word	0x3fc02c02
	.word	0x3fd02c02
	.word	0x3fe02c02
	.word	0x3ff02c02
	.word	0x40002c02
	.word	0x40102c02
	.word	0x40202c02
	.word	0x40302c02
	.word	0x40402c02
	.word	0x40502c02
	.word	0x40602c02
	.word	0x40702c02
	.word	0x40802c02
	.word	0x40902c02
	.word	0x40a02c02
	.word	0x40b02c02
	.word	0x40c02c02
	.word	0x40d02c02
	.word	0x40e02c02
	.word	0x40f02c02
	.word	0x41002c02
	.word	0x41102c02
	.word	0x41202c02
	.word	0x41302c02
	.word	0x41402c02
	.word	0x41502c02
	.word	0x41602c02
	.word	0x41702c02
	.word	0x41802c02
	.word	0x41902c02
	.word	0x41a02c02
	.word	0x41b02c02
	.word	0x41c02c02
	.word	0x41d02c02
	.word	0x41e02c02
	.word	0x41f02c02
	.word	0x42002c02
	.word	0x42102c02
	.word	0x42202c02
	.word	0x42302c02
	.word	0x42402c02
	.word	0x42502c02
	.word	0x42602c02
	.word	0x42702c02
	.word	0x42802c02
	.word	0x42902c02
	.word	0x42a02c02
	.word	0x42b02c02
	.word	0x42c02c02
	.word	0x42d02c02
	.word	0x42e02c02
	.word	0x42f02c02
	.word	0x43002c02
	.word	0x43102c02
	.word	0x43202c02
	.word	0x43302c02
	.word	0x43402c02
	.word	0x43502c02
	.word	0x43602c02
	.word	0x43702c02
	.word	0x43802c02
	.word	0x43902c02
	.word	0x43a02c02
	.word	0x43b02c02
	.word	0x43c02c02
	.word	0x43d02c02
	.word	0x43e02c02
	.word	0x43f02c02
	.word	0x44002c02
	.word	0x44102c02
	.word	0x44202c02
	.word	0x44302c02
	.word	0x44402c02
	.word	0x44502c02
	.word	0x44602c02
	.word	0x44702c02
	.word	0x44802c02
	.word	0x44902c02
	.word	0x44a02c02
	.word	0x44b02c02
	.word	0x44c02c02
	.word	0x44d02c02
	.word	0x44e02c02
	.word	0x44f02c02
	.word	0x45002c02
	.word	0x45102c02
	.word	0x45202c02
	.word	0x45302c02
	.word	0x45402c02
	.word	0x45502c02
	.word	0x45602c02
	.word	0x45702c02
	.word	0x45802c02
	.word	0x45902c02
	.word	0x45a02c02
	.word	0x45b02c02
	.word	0x45c02c02
	.word	0x45d02c02
	.word	0x45e02c02
	.word	0x45f02c02
	.word	0x46002c02
	.word	0x46102c02
	.word	0x46202c02
	.word	0x46302c02
	.word	0x46402c02
	.word	0x46502c02
	.word	0x46602c02
	.word	0x46702c02
	.word	0x46802c02
	.word	0x46902c02
	.word	0x46a02c02
	.word	0x46b02c02
	.word	0x46c02c02
	.word	0x46d02c02
	.word	0x46e02c02
	.word	0x46f02c02
	.word	0x47002c02
	.word	0x47102c02
	.word	0x47202c02
	.word	0x47302c02
	.word	0x47402c02
	.word	0x47502c02
	.word	0x47602c02
	.word	0x47702c02
	.word	0x47802c02
	.word	0x47902c02
	.word	0x47a02c02
	.word	0x47b02c02
	.word	0x47c02c02
	.word	0x47d02c02
	.word	0x47e02c02
	.word	0x47f02c02
	.word	0x48002c02
	.word	0x48102c02
	.word	0x48202c02
	.word	0x48302c02
	.word	0x48402c02
	.word	0x48502c02
	.word	0x48602c02
	.word	0x48702c02
	.word	0x48802c02
	.word	0x48902c02
	.word	0x48a02c02
	.word	0x48b02c02
	.word	0x48c02c02
	.word	0x48d02c02
	.word	0x48e02c02
	.word	0x48f02c02
	.word	0x49002c02
	.word	0x49102c02
	.word	0x49202c02
	.word	0x49302c02
	.word	0x49402c02
	.word	0x49502c02
	.word	0x49602c02
	.word	0x49702c02
	.word	0x49802c02
	.word	0x49902c02
	.word	0x49a02c02
	.word	0x49b02c02
	.word	0x49c02c02
	.word	0x49d02c02
	.word	0x49e02c02
	.word	0x49f02c02
	.word	0x4a002c02
	.word	0x4a102c02
	.word	0x4a202c02
	.word	0x4a302c02
	.word	0x4a402c02
	.word	0x4a502c02
	.word	0x4a602c02
	.word	0x4a702c02
	.word	0x4a802c02
	.word	0x4a902c02
	.word	0x4aa02c02
	.word	0x4ab02c02
	.word	0x4ac02c02
	.word	0x4ad02c02
	.word	0x4ae02c02
	.word	0x4af02c02
	.word	0x4b002c02
	.word	0x4b102c02
	.word	0x4b202c02
	.word	0x4b302c02
	.word	0x4b402c02
	.word	0x4b502c02
	.word	0x4b602c02
	.word	0x4b702c02
	.word	0x4b802c02
	.word	0x4b902c02
	.word	0x4ba02c02
	.word	0x4bb02c02
	.word	0x4bc02c02
	.word	0x4bd02c02
	.word	0x4be02c02
	.word	0x4bf02c02
	.word	0x4c002c02
	.word	0x4c102c02
	.word	0x4c202c02
	.word	0x4c302c02
	.word	0x4c402c02
	.word	0x4c502c02
	.word	0x4c602c02
	.word	0x4c702c02
	.word	0x4c802c02
	.word	0x4c902c02
	.word	0x4ca02c02
	.word	0x4cb02c02
	.word	0x4cc02c02
	.word	0x4cd02c02
	.word	0x4ce02c02
	.word	0x4cf02c02
	.word	0x4d002c02
	.word	0x4d102c02
	.word	0x4d202c02
	.word	0x4d302c02
	.word	0x4d402c02
	.word	0x4d502c02
	.word	0x4d602c02
	.word	0x4d702c02
	.word	0x4d802c02
	.word	0x4d902c02
	.word	0x4da02c02
	.word	0x4db02c02
	.word	0x4dc02c02
	.word	0x4dd02c02
	.word	0x4de02c02
	.word	0x4df02c02
	.word	0x4e002c02
	.word	0x4e102c02
	.word	0x4e202c02
	.word	0x4e302c02
	.word	0x4e402c02
	.word	0x4e502c02
	.word	0x4e602c02
	.word	0x4e702c02
	.word	0x4e802c02
	.word	0x4e902c02
	.word	0x4ea02c02
	.word	0x4eb02c02
	.word	0x4ec02c02
	.word	0x4ed02c02
	.word	0x4ee02c02
	.word	0x4ef02c02
	.word	0x4f002c02
	.word	0x4f102c02
	.word	0x4f202c02
	.word	0x4f302c02
	.word	0x4f402c02
	.word	0x4f502c02
	.word	0x4f602c02
	.word	0x4f702c02
	.word	0x4f802c02
	.word	0x4f902c02
	.word	0x4fa02c02
	.word	0x4fb02c02
	.word	0x4fc02c02
	.word	0x4fd02c02
	.word	0x4fe02c02
	.word	0x4ff02c02
	.word	0x50002c02
	.word	0x50102c02
	.word	0x50202c02
	.word	0x50302c02
	.word	0x50402c02
	.word	0x50502c02
	.word	0x50602c02
	.word	0x50702c02
	.word	0x50802c02
	.word	0x50902c02
	.word	0x50a02c02
	.word	0x50b02c02
	.word	0x50c02c02
	.word	0x50d02c02
	.word	0x50e02c02
	.word	0x50f02c02
	.word	0x51002c02
	.word	0x51102c02
	.word	0x51202c02
	.word	0x51302c02
	.word	0x51402c02
	.word	0x51502c02
	.word	0x51602c02
	.word	0x51702c02
	.word	0x51802c02
	.word	0x51902c02
	.word	0x51a02c02
	.word	0x51b02c02
	.word	0x51c02c02
	.word	0x51d02c02
	.word	0x51e02c02
	.word	0x51f02c02
	.word	0x52002c02
	.word	0x52102c02
	.word	0x52202c02
	.word	0x52302c02
	.word	0x52402c02
	.word	0x52502c02
	.word	0x52602c02
	.word	0x52702c02
	.word	0x52802c02
	.word	0x52902c02
	.word	0x52a02c02
	.word	0x52b02c02
	.word	0x52c02c02
	.word	0x52d02c02
	.word	0x52e02c02
	.word	0x52f02c02
	.word	0x53002c02
	.word	0x53102c02
	.word	0x53202c02
	.word	0x53302c02
	.word	0x53402c02
	.word	0x53502c02
	.word	0x53602c02
	.word	0x53702c02
	.word	0x53802c02
	.word	0x53902c02
	.word	0x53a02c02
	.word	0x53b02c02
	.word	0x53c02c02
	.word	0x53d02c02
	.word	0x53e02c02
	.word	0x53f02c02
	.word	0x54002c02
	.word	0x54102c02
	.word	0x54202c02
	.word	0x54302c02
	.word	0x54402c02
	.word	0x54502c02
	.word	0x54602c02
	.word	0x54702c02
	.word	0x54802c02
	.word	0x54902c02
	.word	0x54a02c02
	.word	0x54b02c02
	.word	0x54c02c02
	.word	0x54d02c02
	.word	0x54e02c02
	.word	0x54f02c02
	.word	0x55002c02
	.word	0x55102c02
	.word	0x55202c02
	.word	0x55302c02
	.word	0x55402c02
	.word	0x55502c02
	.word	0x55602c02
	.word	0x55702c02
	.word	0x55802c02
	.word	0x55902c02
	.word	0x55a02c02
	.word	0x55b02c02
	.word	0x55c02c02
	.word	0x55d02c02
	.word	0x55e02c02
	.word	0x55f02c02
	.word	0x56002c02
	.word	0x56102c02
	.word	0x56202c02
	.word	0x56302c02
	.word	0x56402c02
	.word	0x56502c02
	.word	0x56602c02
	.word	0x56702c02
	.word	0x56802c02
	.word	0x56902c02
	.word	0x56a02c02
	.word	0x56b02c02
	.word	0x56c02c02
	.word	0x56d02c02
	.word	0x56e02c02
	.word	0x56f02c02
	.word	0x57002c02
	.word	0x57102c02
	.word	0x57202c02
	.word	0x57302c02
	.word	0x57402c02
	.word	0x57502c02
	.word	0x57602c02
	.word	0x57702c02
	.word	0x57802c02
	.word	0x57902c02
	.word	0x57a02c02
	.word	0x57b02c02
	.word	0x57c02c02
	.word	0x57d02c02
	.word	0x57e02c02
	.word	0x57f02c02
	.word	0x58002c02
	.word	0x58102c02
	.word	0x58202c02
	.word	0x58302c02
	.word	0x58402c02
	.word	0x58502c02
	.word	0x58602c02
	.word	0x58702c02
	.word	0x58802c02
	.word	0x58902c02
	.word	0x58a02c02
	.word	0x58b02c02
	.word	0x58c02c02
	.word	0x58d02c02
	.word	0x58e02c02
	.word	0x58f02c02
	.word	0x59002c02
	.word	0x59102c02
	.word	0x59202c02
	.word	0x59302c02
	.word	0x59402c02
	.word	0x59502c02
	.word	0x59602c02
	.word	0x59702c02
	.word	0x59802c02
	.word	0x59902c02
	.word	0x59a02c02
	.word	0x59b02c02
	.word	0x59c02c02
	.word	0x59d02c02
	.word	0x59e02c02
	.word	0x59f02c02
	.word	0x5a002c02
	.word	0x5a102c02
	.word	0x5a202c02
	.word	0x5a302c02
	.word	0x5a402c02
	.word	0x5a502c02
	.word	0x5a602c02
	.word	0x5a702c02
	.word	0x5a802c02
	.word	0x5a902c02
	.word	0x5aa02c02
	.word	0x5ab02c02
	.word	0x5ac02c02
	.word	0x5ad02c02
	.word	0x5ae02c02
	.word	0x5af02c02
	.word	0x5b002c02
	.word	0x5b102c02
	.word	0x5b202c02
	.word	0x5b302c02
	.word	0x5b402c02
	.word	0x5b502c02
	.word	0x5b602c02
	.word	0x5b702c02
	.word	0x5b802c02
	.word	0x5b902c02
	.word	0x5ba02c02
	.word	0x5bb02c02
	.word	0x5bc02c02
	.word	0x5bd02c02
	.word	0x5be02c02
	.word	0x5bf02c02
	.word	0x5c002c02
	.word	0x5c102c02
	.word	0x5c202c02
	.word	0x5c302c02
	.word	0x5c402c02
	.word	0x5c502c02
	.word	0x5c602c02
	.word	0x5c702c02
	.word	0x5c802c02
	.word	0x5c902c02
	.word	0x5ca02c02
	.word	0x5cb02c02
	.word	0x5cc02c02
	.word	0x5cd02c02
	.word	0x5ce02c02
	.word	0x5cf02c02
	.word	0x5d002c02
	.word	0x5d102c02
	.word	0x5d202c02
	.word	0x5d302c02
	.word	0x5d402c02
	.word	0x5d502c02
	.word	0x5d602c02
	.word	0x5d702c02
	.word	0x5d802c02
	.word	0x5d902c02
	.word	0x5da02c02
	.word	0x5db02c02
	.word	0x5dc02c02
	.word	0x5dd02c02
	.word	0x5de02c02
	.word	0x5df02c02
	.word	0x5e002c02
	.word	0x5e102c02
	.word	0x5e202c02
	.word	0x5e302c02
	.word	0x5e402c02
	.word	0x5e502c02
	.word	0x5e602c02
	.word	0x5e702c02
	.word	0x5e802c02
	.word	0x5e902c02
	.word	0x5ea02c02
	.word	0x5eb02c02
	.word	0x5ec02c02
	.word	0x5ed02c02
	.word	0x5ee02c02
	.word	0x5ef02c02
	.word	0x5f002c02
	.word	0x5f102c02
	.word	0x5f202c02
	.word	0x5f302c02
	.word	0x5f402c02
	.word	0x5f502c02
	.word	0x5f602c02
	.word	0x5f702c02
	.word	0x5f802c02
	.word	0x5f902c02
	.word	0x5fa02c02
	.word	0x5fb02c02
	.word	0x5fc02c02
	.word	0x5fd02c02
	.word	0x5fe02c02
	.word	0x5ff02c02
	.word	0x60002c02
	.word	0x60102c02
	.word	0x60202c02
	.word	0x60302c02
	.word	0x60402c02
	.word	0x60502c02
	.word	0x60602c02
	.word	0x60702c02
	.word	0x60802c02
	.word	0x60902c02
	.word	0x60a02c02
	.word	0x60b02c02
	.word	0x60c02c02
	.word	0x60d02c02
	.word	0x60e02c02
	.word	0x60f02c02
	.word	0x61002c02
	.word	0x61102c02
	.word	0x61202c02
	.word	0x61302c02
	.word	0x61402c02
	.word	0x61502c02
	.word	0x61602c02
	.word	0x61702c02
	.word	0x61802c02
	.word	0x61902c02
	.word	0x61a02c02
	.word	0x61b02c02
	.word	0x61c02c02
	.word	0x61d02c02
	.word	0x61e02c02
	.word	0x61f02c02
	.word	0x62002c02
	.word	0x62102c02
	.word	0x62202c02
	.word	0x62302c02
	.word	0x62402c02
	.word	0x62502c02
	.word	0x62602c02
	.word	0x62702c02
	.word	0x62802c02
	.word	0x62902c02
	.word	0x62a02c02
	.word	0x62b02c02
	.word	0x62c02c02
	.word	0x62d02c02
	.word	0x62e02c02
	.word	0x62f02c02
	.word	0x63002c02
	.word	0x63102c02
	.word	0x63202c02
	.word	0x63302c02
	.word	0x63402c02
	.word	0x63502c02
	.word	0x63602c02
	.word	0x63702c02
	.word	0x63802c02
	.word	0x63902c02
	.word	0x63a02c02
	.word	0x63b02c02
	.word	0x63c02c02
	.word	0x63d02c02
	.word	0x63e02c02
	.word	0x63f02c02
	.word	0x64002c02
	.word	0x64102c02
	.word	0x64202c02
	.word	0x64302c02
	.word	0x64402c02
	.word	0x64502c02
	.word	0x64602c02
	.word	0x64702c02
	.word	0x64802c02
	.word	0x64902c02
	.word	0x64a02c02
	.word	0x64b02c02
	.word	0x64c02c02
	.word	0x64d02c02
	.word	0x64e02c02
	.word	0x64f02c02
	.word	0x65002c02
	.word	0x65102c02
	.word	0x65202c02
	.word	0x65302c02
	.word	0x65402c02
	.word	0x65502c02
	.word	0x65602c02
	.word	0x65702c02
	.word	0x65802c02
	.word	0x65902c02
	.word	0x65a02c02
	.word	0x65b02c02
	.word	0x65c02c02
	.word	0x65d02c02
	.word	0x65e02c02
	.word	0x65f02c02
	.word	0x66002c02
	.word	0x66102c02
	.word	0x66202c02
	.word	0x66302c02
	.word	0x66402c02
	.word	0x66502c02
	.word	0x66602c02
	.word	0x66702c02
	.word	0x66802c02
	.word	0x66902c02
	.word	0x66a02c02
	.word	0x66b02c02
	.word	0x66c02c02
	.word	0x66d02c02
	.word	0x66e02c02
	.word	0x66f02c02
	.word	0x67002c02
	.word	0x67102c02
	.word	0x67202c02
	.word	0x67302c02
	.word	0x67402c02
	.word	0x67502c02
	.word	0x67602c02
	.word	0x67702c02
	.word	0x67802c02
	.word	0x67902c02
	.word	0x67a02c02
	.word	0x67b02c02
	.word	0x67c02c02
	.word	0x67d02c02
	.word	0x67e02c02
	.word	0x67f02c02
	.word	0x68002c02
	.word	0x68102c02
	.word	0x68202c02
	.word	0x68302c02
	.word	0x68402c02
	.word	0x68502c02
	.word	0x68602c02
	.word	0x68702c02
	.word	0x68802c02
	.word	0x68902c02
	.word	0x68a02c02
	.word	0x68b02c02
	.word	0x68c02c02
	.word	0x68d02c02
	.word	0x68e02c02
	.word	0x68f02c02
	.word	0x69002c02
	.word	0x69102c02
	.word	0x69202c02
	.word	0x69302c02
	.word	0x69402c02
	.word	0x69502c02
	.word	0x69602c02
	.word	0x69702c02
	.word	0x69802c02
	.word	0x69902c02
	.word	0x69a02c02
	.word	0x69b02c02
	.word	0x69c02c02
	.word	0x69d02c02
	.word	0x69e02c02
	.word	0x69f02c02
	.word	0x6a002c02
	.word	0x6a102c02
	.word	0x6a202c02
	.word	0x6a302c02
	.word	0x6a402c02
	.word	0x6a502c02
	.word	0x6a602c02
	.word	0x6a702c02
	.word	0x6a802c02
	.word	0x6a902c02
	.word	0x6aa02c02
	.word	0x6ab02c02
	.word	0x6ac02c02
	.word	0x6ad02c02
	.word	0x6ae02c02
	.word	0x6af02c02
	.word	0x6b002c02
	.word	0x6b102c02
	.word	0x6b202c02
	.word	0x6b302c02
	.word	0x6b402c02
	.word	0x6b502c02
	.word	0x6b602c02
	.word	0x6b702c02
	.word	0x6b802c02
	.word	0x6b902c02
	.word	0x6ba02c02
	.word	0x6bb02c02
	.word	0x6bc02c02
	.word	0x6bd02c02
	.word	0x6be02c02
	.word	0x6bf02c02
	.word	0x6c002c02
	.word	0x6c102c02
	.word	0x6c202c02
	.word	0x6c302c02
	.word	0x6c402c02
	.word	0x6c502c02
	.word	0x6c602c02
	.word	0x6c702c02
	.word	0x6c802c02
	.word	0x6c902c02
	.word	0x6ca02c02
	.word	0x6cb02c02
	.word	0x6cc02c02
	.word	0x6cd02c02
	.word	0x6ce02c02
	.word	0x6cf02c02
	.word	0x6d002c02
	.word	0x6d102c02
	.word	0x6d202c02
	.word	0x6d302c02
	.word	0x6d402c02
	.word	0x6d502c02
	.word	0x6d602c02
	.word	0x6d702c02
	.word	0x6d802c02
	.word	0x6d902c02
	.word	0x6da02c02
	.word	0x6db02c02
	.word	0x6dc02c02
	.word	0x6dd02c02
	.word	0x6de02c02
	.word	0x6df02c02
	.word	0x6e002c02
	.word	0x6e102c02
	.word	0x6e202c02
	.word	0x6e302c02
	.word	0x6e402c02
	.word	0x6e502c02
	.word	0x6e602c02
	.word	0x6e702c02
	.word	0x6e802c02
	.word	0x6e902c02
	.word	0x6ea02c02
	.word	0x6eb02c02
	.word	0x6ec02c02
	.word	0x6ed02c02
	.word	0x6ee02c02
	.word	0x6ef02c02
	.word	0x6f002c02
	.word	0x6f102c02
	.word	0x6f202c02
	.word	0x6f302c02
	.word	0x6f402c02
	.word	0x6f502c02
	.word	0x6f602c02
	.word	0x6f702c02
	.word	0x6f802c02
	.word	0x6f902c02
	.word	0x6fa02c02
	.word	0x6fb02c02
	.word	0x6fc02c02
	.word	0x6fd02c02
	.word	0x6fe02c02
	.word	0x6ff02c02
	.word	0x70002c02
	.word	0x70102c02
	.word	0x70202c02
	.word	0x70302c02
	.word	0x70402c02
	.word	0x70502c02
	.word	0x70602c02
	.word	0x70702c02
	.word	0x70802c02
	.word	0x70902c02
	.word	0x70a02c02
	.word	0x70b02c02
	.word	0x70c02c02
	.word	0x70d02c02
	.word	0x70e02c02
	.word	0x70f02c02
	.word	0x71002c02
	.word	0x71102c02
	.word	0x71202c02
	.word	0x71302c02
	.word	0x71402c02
	.word	0x71502c02
	.word	0x71602c02
	.word	0x71702c02
	.word	0x71802c02
	.word	0x71902c02
	.word	0x71a02c02
	.word	0x71b02c02
	.word	0x71c02c02
	.word	0x71d02c02
	.word	0x71e02c02
	.word	0x71f02c02
	.word	0x72002c02
	.word	0x72102c02
	.word	0x72202c02
	.word	0x72302c02
	.word	0x72402c02
	.word	0x72502c02
	.word	0x72602c02
	.word	0x72702c02
	.word	0x72802c02
	.word	0x72902c02
	.word	0x72a02c02
	.word	0x72b02c02
	.word	0x72c02c02
	.word	0x72d02c02
	.word	0x72e02c02
	.word	0x72f02c02
	.word	0x73002c02
	.word	0x73102c02
	.word	0x73202c02
	.word	0x73302c02
	.word	0x73402c02
	.word	0x73502c02
	.word	0x73602c02
	.word	0x73702c02
	.word	0x73802c02
	.word	0x73902c02
	.word	0x73a02c02
	.word	0x73b02c02
	.word	0x73c02c02
	.word	0x73d02c02
	.word	0x73e02c02
	.word	0x73f02c02
	.word	0x74002c02
	.word	0x74102c02
	.word	0x74202c02
	.word	0x74302c02
	.word	0x74402c02
	.word	0x74502c02
	.word	0x74602c02
	.word	0x74702c02
	.word	0x74802c02
	.word	0x74902c02
	.word	0x74a02c02
	.word	0x74b02c02
	.word	0x74c02c02
	.word	0x74d02c02
	.word	0x74e02c02
	.word	0x74f02c02
	.word	0x75002c02
	.word	0x75102c02
	.word	0x75202c02
	.word	0x75302c02
	.word	0x75402c02
	.word	0x75502c02
	.word	0x75602c02
	.word	0x75702c02
	.word	0x75802c02
	.word	0x75902c02
	.word	0x75a02c02
	.word	0x75b02c02
	.word	0x75c02c02
	.word	0x75d02c02
	.word	0x75e02c02
	.word	0x75f02c02
	.word	0x76002c02
	.word	0x76102c02
	.word	0x76202c02
	.word	0x76302c02
	.word	0x76402c02
	.word	0x76502c02
	.word	0x76602c02
	.word	0x76702c02
	.word	0x76802c02
	.word	0x76902c02
	.word	0x76a02c02
	.word	0x76b02c02
	.word	0x76c02c02
	.word	0x76d02c02
	.word	0x76e02c02
	.word	0x76f02c02
	.word	0x77002c02
	.word	0x77102c02
	.word	0x77202c02
	.word	0x77302c02
	.word	0x77402c02
	.word	0x77502c02
	.word	0x77602c02
	.word	0x77702c02
	.word	0x77802c02
	.word	0x77902c02
	.word	0x77a02c02
	.word	0x77b02c02
	.word	0x77c02c02
	.word	0x77d02c02
	.word	0x77e02c02
	.word	0x77f02c02
	.word	0x78002c02
	.word	0x78102c02
	.word	0x78202c02
	.word	0x78302c02
	.word	0x78402c02
	.word	0x78502c02
	.word	0x78602c02
	.word	0x78702c02
	.word	0x78802c02
	.word	0x78902c02
	.word	0x78a02c02
	.word	0x78b02c02
	.word	0x78c02c02
	.word	0x78d02c02
	.word	0x78e02c02
	.word	0x78f02c02
	.word	0x79002c02
	.word	0x79102c02
	.word	0x79202c02
	.word	0x79302c02
	.word	0x79402c02
	.word	0x79502c02
	.word	0x79602c02
	.word	0x79702c02
	.word	0x79802c02
	.word	0x79902c02
	.word	0x79a02c02
	.word	0x79b02c02
	.word	0x79c02c02
	.word	0x79d02c02
	.word	0x79e02c02
	.word	0x79f02c02
	.word	0x7a002c02
	.word	0x7a102c02
	.word	0x7a202c02
	.word	0x7a302c02
	.word	0x7a402c02
	.word	0x7a502c02
	.word	0x7a602c02
	.word	0x7a702c02
	.word	0x7a802c02
	.word	0x7a902c02
	.word	0x7aa02c02
	.word	0x7ab02c02
	.word	0x7ac02c02
	.word	0x7ad02c02
	.word	0x7ae02c02
	.word	0x7af02c02
	.word	0x7b002c02
	.word	0x7b102c02
	.word	0x7b202c02
	.word	0x7b302c02
	.word	0x7b402c02
	.word	0x7b502c02
	.word	0x7b602c02
	.word	0x7b702c02
	.word	0x7b802c02
	.word	0x7b902c02
	.word	0x7ba02c02
	.word	0x7bb02c02
	.word	0x7bc02c02
	.word	0x7bd02c02
	.word	0x7be02c02
	.word	0x7bf02c02
	.word	0x7c002c02
	.word	0x7c102c02
	.word	0x7c202c02
	.word	0x7c302c02
	.word	0x7c402c02
	.word	0x7c502c02
	.word	0x7c602c02
	.word	0x7c702c02
	.word	0x7c802c02
	.word	0x7c902c02
	.word	0x7ca02c02
	.word	0x7cb02c02
	.word	0x7cc02c02
	.word	0x7cd02c02
	.word	0x7ce02c02
	.word	0x7cf02c02
	.word	0x7d002c02
	.word	0x7d102c02
	.word	0x7d202c02
	.word	0x7d302c02
	.word	0x7d402c02
	.word	0x7d502c02
	.word	0x7d602c02
	.word	0x7d702c02
	.word	0x7d802c02
	.word	0x7d902c02
	.word	0x7da02c02
	.word	0x7db02c02
	.word	0x7dc02c02
	.word	0x7dd02c02
	.word	0x7de02c02
	.word	0x7df02c02
	.word	0x7e002c02
	.word	0x7e102c02
	.word	0x7e202c02
	.word	0x7e302c02
	.word	0x7e402c02
	.word	0x7e502c02
	.word	0x7e602c02
	.word	0x7e702c02
	.word	0x7e802c02
	.word	0x7e902c02
	.word	0x7ea02c02
	.word	0x7eb02c02
	.word	0x7ec02c02
	.word	0x7ed02c02
	.word	0x7ee02c02
	.word	0x7ef02c02
	.word	0x7f002c02
	.word	0x7f102c02
	.word	0x7f202c02
	.word	0x7f302c02
	.word	0x7f402c02
	.word	0x7f502c02
	.word	0x7f602c02
	.word	0x7f702c02
	.word	0x7f802c02
	.word	0x7f902c02
	.word	0x7fa02c02
	.word	0x7fb02c02
	.word	0x7fc02c02
	.word	0x7fd02c02
	.word	0x7fe02c02
	.word	0x7ff02c02
	.word	0x80002c02
	.word	0x80102c02
	.word	0x80202c02
	.word	0x80302c02
	.word	0x80402c02
	.word	0x80502c02
	.word	0x80602c02
	.word	0x80702c02
	.word	0x80802c02
	.word	0x80902c02
	.word	0x80a02c02
	.word	0x80b02c02
	.word	0x80c02c02
	.word	0x80d02c02
	.word	0x80e02c02
	.word	0x80f02c02
	.word	0x81002c02
	.word	0x81102c02
	.word	0x81202c02
	.word	0x81302c02
	.word	0x81402c02
	.word	0x81502c02
	.word	0x81602c02
	.word	0x81702c02
	.word	0x81802c02
	.word	0x81902c02
	.word	0x81a02c02
	.word	0x81b02c02
	.word	0x81c02c02
	.word	0x81d02c02
	.word	0x81e02c02
	.word	0x81f02c02
	.word	0x82002c02
	.word	0x82102c02
	.word	0x82202c02
	.word	0x82302c02
	.word	0x82402c02
	.word	0x82502c02
	.word	0x82602c02
	.word	0x82702c02
	.word	0x82802c02
	.word	0x82902c02
	.word	0x82a02c02
	.word	0x82b02c02
	.word	0x82c02c02
	.word	0x82d02c02
	.word	0x82e02c02
	.word	0x82f02c02
	.word	0x83002c02
	.word	0x83102c02
	.word	0x83202c02
	.word	0x83302c02
	.word	0x83402c02
	.word	0x83502c02
	.word	0x83602c02
	.word	0x83702c02
	.word	0x83802c02
	.word	0x83902c02
	.word	0x83a02c02
	.word	0x83b02c02
	.word	0x83c02c02
	.word	0x83d02c02
	.word	0x83e02c02
	.word	0x83f02c02
	.word	0x84002c02
	.word	0x84102c02
	.word	0x84202c02
	.word	0x84302c02
	.word	0x84402c02
	.word	0x84502c02
	.word	0x84602c02
	.word	0x84702c02
	.word	0x84802c02
	.word	0x84902c02
	.word	0x84a02c02
	.word	0x84b02c02
	.word	0x84c02c02
	.word	0x84d02c02
	.word	0x84e02c02
	.word	0x84f02c02
	.word	0x85002c02
	.word	0x85102c02
	.word	0x85202c02
	.word	0x85302c02
	.word	0x85402c02
	.word	0x85502c02
	.word	0x85602c02
	.word	0x85702c02
	.word	0x85802c02
	.word	0x85902c02
	.word	0x85a02c02
	.word	0x85b02c02
	.word	0x85c02c02
	.word	0x85d02c02
	.word	0x85e02c02
	.word	0x85f02c02
	.word	0x86002c02
	.word	0x86102c02
	.word	0x86202c02
	.word	0x86302c02
	.word	0x86402c02
	.word	0x86502c02
	.word	0x86602c02
	.word	0x86702c02
	.word	0x86802c02
	.word	0x86902c02
	.word	0x86a02c02
	.word	0x86b02c02
	.word	0x86c02c02
	.word	0x86d02c02
	.word	0x86e02c02
	.word	0x86f02c02
	.word	0x87002c02
	.word	0x87102c02
	.word	0x87202c02
	.word	0x87302c02
	.word	0x87402c02
	.word	0x87502c02
	.word	0x87602c02
	.word	0x87702c02
	.word	0x87802c02
	.word	0x87902c02
	.word	0x87a02c02
	.word	0x87b02c02
	.word	0x87c02c02
	.word	0x87d02c02
	.word	0x87e02c02
	.word	0x87f02c02
	.word	0x88002c02
	.word	0x88102c02
	.word	0x88202c02
	.word	0x88302c02
	.word	0x88402c02
	.word	0x88502c02
	.word	0x88602c02
	.word	0x88702c02
	.word	0x88802c02
	.word	0x88902c02
	.word	0x88a02c02
	.word	0x88b02c02
	.word	0x88c02c02
	.word	0x88d02c02
	.word	0x88e02c02
	.word	0x88f02c02
	.word	0x89002c02
	.word	0x89102c02
	.word	0x89202c02
	.word	0x89302c02
	.word	0x89402c02
	.word	0x89502c02
	.word	0x89602c02
	.word	0x89702c02
	.word	0x89802c02
	.word	0x89902c02
	.word	0x89a02c02
	.word	0x89b02c02
	.word	0x89c02c02
	.word	0x89d02c02
	.word	0x89e02c02
	.word	0x89f02c02
	.word	0x8a002c02
	.word	0x8a102c02
	.word	0x8a202c02
	.word	0x8a302c02
	.word	0x8a402c02
	.word	0x8a502c02
	.word	0x8a602c02
	.word	0x8a702c02
	.word	0x8a802c02
	.word	0x8a902c02
	.word	0x8aa02c02
	.word	0x8ab02c02
	.word	0x8ac02c02
	.word	0x8ad02c02
	.word	0x8ae02c02
	.word	0x8af02c02
	.word	0x8b002c02
	.word	0x8b102c02
	.word	0x8b202c02
	.word	0x8b302c02
	.word	0x8b402c02
	.word	0x8b502c02
	.word	0x8b602c02
	.word	0x8b702c02
	.word	0x8b802c02
	.word	0x8b902c02
	.word	0x8ba02c02
	.word	0x8bb02c02
	.word	0x8bc02c02
	.word	0x8bd02c02
	.word	0x8be02c02
	.word	0x8bf02c02
	.word	0x8c002c02
	.word	0x8c102c02
	.word	0x8c202c02
	.word	0x8c302c02
	.word	0x8c402c02
	.word	0x8c502c02
	.word	0x8c602c02
	.word	0x8c702c02
	.word	0x8c802c02
	.word	0x8c902c02
	.word	0x8ca02c02
	.word	0x8cb02c02
	.word	0x8cc02c02
	.word	0x8cd02c02
	.word	0x8ce02c02
	.word	0x8cf02c02
	.word	0x8d002c02
	.word	0x8d102c02
	.word	0x8d202c02
	.word	0x8d302c02
	.word	0x8d402c02
	.word	0x8d502c02
	.word	0x8d602c02
	.word	0x8d702c02
	.word	0x8d802c02
	.word	0x8d902c02
	.word	0x8da02c02
	.word	0x8db02c02
	.word	0x8dc02c02
	.word	0x8dd02c02
	.word	0x8de02c02
	.word	0x8df02c02
	.word	0x8e002c02
	.word	0x8e102c02
	.word	0x8e202c02
	.word	0x8e302c02
	.word	0x8e402c02
	.word	0x8e502c02
	.word	0x8e602c02
	.word	0x8e702c02
	.word	0x8e802c02
	.word	0x8e902c02
	.word	0x8ea02c02
	.word	0x8eb02c02
	.word	0x8ec02c02
	.word	0x8ed02c02
	.word	0x8ee02c02
	.word	0x8ef02c02
	.word	0x8f002c02
	.word	0x8f102c02
	.word	0x8f202c02
	.word	0x8f302c02
	.word	0x8f402c02
	.word	0x8f502c02
	.word	0x8f602c02
	.word	0x8f702c02
	.word	0x8f802c02
	.word	0x8f902c02
	.word	0x8fa02c02
	.word	0x8fb02c02
	.word	0x8fc02c02
	.word	0x8fd02c02
	.word	0x8fe02c02
	.word	0x8ff02c02
	.word	0x90002c02
	.word	0x90102c02
	.word	0x90202c02
	.word	0x90302c02
	.word	0x90402c02
	.word	0x90502c02
	.word	0x90602c02
	.word	0x90702c02
	.word	0x90802c02
	.word	0x90902c02
	.word	0x90a02c02
	.word	0x90b02c02
	.word	0x90c02c02
	.word	0x90d02c02
	.word	0x90e02c02
	.word	0x90f02c02
	.word	0x91002c02
	.word	0x91102c02
	.word	0x91202c02
	.word	0x91302c02
	.word	0x91402c02
	.word	0x91502c02
	.word	0x91602c02
	.word	0x91702c02
	.word	0x91802c02
	.word	0x91902c02
	.word	0x91a02c02
	.word	0x91b02c02
	.word	0x91c02c02
	.word	0x91d02c02
	.word	0x91e02c02
	.word	0x91f02c02
	.word	0x92002c02
	.word	0x92102c02
	.word	0x92202c02
	.word	0x92302c02
	.word	0x92402c02
	.word	0x92502c02
	.word	0x92602c02
	.word	0x92702c02
	.word	0x92802c02
	.word	0x92902c02
	.word	0x92a02c02
	.word	0x92b02c02
	.word	0x92c02c02
	.word	0x92d02c02
	.word	0x92e02c02
	.word	0x92f02c02
	.word	0x93002c02
	.word	0x93102c02
	.word	0x93202c02
	.word	0x93302c02
	.word	0x93402c02
	.word	0x93502c02
	.word	0x93602c02
	.word	0x93702c02
	.word	0x93802c02
	.word	0x93902c02
	.word	0x93a02c02
	.word	0x93b02c02
	.word	0x93c02c02
	.word	0x93d02c02
	.word	0x93e02c02
	.word	0x93f02c02
	.word	0x94002c02
	.word	0x94102c02
	.word	0x94202c02
	.word	0x94302c02
	.word	0x94402c02
	.word	0x94502c02
	.word	0x94602c02
	.word	0x94702c02
	.word	0x94802c02
	.word	0x94902c02
	.word	0x94a02c02
	.word	0x94b02c02
	.word	0x94c02c02
	.word	0x94d02c02
	.word	0x94e02c02
	.word	0x94f02c02
	.word	0x95002c02
	.word	0x95102c02
	.word	0x95202c02
	.word	0x95302c02
	.word	0x95402c02
	.word	0x95502c02
	.word	0x95602c02
	.word	0x95702c02
	.word	0x95802c02
	.word	0x95902c02
	.word	0x95a02c02
	.word	0x95b02c02
	.word	0x95c02c02
	.word	0x95d02c02
	.word	0x95e02c02
	.word	0x95f02c02
	.word	0x96002c02
	.word	0x96102c02
	.word	0x96202c02
	.word	0x96302c02
	.word	0x96402c02
	.word	0x96502c02
	.word	0x96602c02
	.word	0x96702c02
	.word	0x96802c02
	.word	0x96902c02
	.word	0x96a02c02
	.word	0x96b02c02
	.word	0x96c02c02
	.word	0x96d02c02
	.word	0x96e02c02
	.word	0x96f02c02
	.word	0x97002c02
	.word	0x97102c02
	.word	0x97202c02
	.word	0x97302c02
	.word	0x97402c02
	.word	0x97502c02
	.word	0x97602c02
	.word	0x97702c02
	.word	0x97802c02
	.word	0x97902c02
	.word	0x97a02c02
	.word	0x97b02c02
	.word	0x97c02c02
	.word	0x97d02c02
	.word	0x97e02c02
	.word	0x97f02c02
	.word	0x98002c02
	.word	0x98102c02
	.word	0x98202c02
	.word	0x98302c02
	.word	0x98402c02
	.word	0x98502c02
	.word	0x98602c02
	.word	0x98702c02
	.word	0x98802c02
	.word	0x98902c02
	.word	0x98a02c02
	.word	0x98b02c02
	.word	0x98c02c02
	.word	0x98d02c02
	.word	0x98e02c02
	.word	0x98f02c02
	.word	0x99002c02
	.word	0x99102c02
	.word	0x99202c02
	.word	0x99302c02
	.word	0x99402c02
	.word	0x99502c02
	.word	0x99602c02
	.word	0x99702c02
	.word	0x99802c02
	.word	0x99902c02
	.word	0x99a02c02
	.word	0x99b02c02
	.word	0x99c02c02
	.word	0x99d02c02
	.word	0x99e02c02
	.word	0x99f02c02
	.word	0x9a002c02
	.word	0x9a102c02
	.word	0x9a202c02
	.word	0x9a302c02
	.word	0x9a402c02
	.word	0x9a502c02
	.word	0x9a602c02
	.word	0x9a702c02
	.word	0x9a802c02
	.word	0x9a902c02
	.word	0x9aa02c02
	.word	0x9ab02c02
	.word	0x9ac02c02
	.word	0x9ad02c02
	.word	0x9ae02c02
	.word	0x9af02c02
	.word	0x9b002c02
	.word	0x9b102c02
	.word	0x9b202c02
	.word	0x9b302c02
	.word	0x9b402c02
	.word	0x9b502c02
	.word	0x9b602c02
	.word	0x9b702c02
	.word	0x9b802c02
	.word	0x9b902c02
	.word	0x9ba02c02
	.word	0x9bb02c02
	.word	0x9bc02c02
	.word	0x9bd02c02
	.word	0x9be02c02
	.word	0x9bf02c02
	.word	0x9c002c02
	.word	0x9c102c02
	.word	0x9c202c02
	.word	0x9c302c02
	.word	0x9c402c02
	.word	0x9c502c02
	.word	0x9c602c02
	.word	0x9c702c02
	.word	0x9c802c02
	.word	0x9c902c02
	.word	0x9ca02c02
	.word	0x9cb02c02
	.word	0x9cc02c02
	.word	0x9cd02c02
	.word	0x9ce02c02
	.word	0x9cf02c02
	.word	0x9d002c02
	.word	0x9d102c02
	.word	0x9d202c02
	.word	0x9d302c02
	.word	0x9d402c02
	.word	0x9d502c02
	.word	0x9d602c02
	.word	0x9d702c02
	.word	0x9d802c02
	.word	0x9d902c02
	.word	0x9da02c02
	.word	0x9db02c02
	.word	0x9dc02c02
	.word	0x9dd02c02
	.word	0x9de02c02
	.word	0x9df02c02
	.word	0x9e002c02
	.word	0x9e102c02
	.word	0x9e202c02
	.word	0x9e302c02
	.word	0x9e402c02
	.word	0x9e502c02
	.word	0x9e602c02
	.word	0x9e702c02
	.word	0x9e802c02
	.word	0x9e902c02
	.word	0x9ea02c02
	.word	0x9eb02c02
	.word	0x9ec02c02
	.word	0x9ed02c02
	.word	0x9ee02c02
	.word	0x9ef02c02
	.word	0x9f002c02
	.word	0x9f102c02
	.word	0x9f202c02
	.word	0x9f302c02
	.word	0x9f402c02
	.word	0x9f502c02
	.word	0x9f602c02
	.word	0x9f702c02
	.word	0x9f802c02
	.word	0x9f902c02
	.word	0x9fa02c02
	.word	0x9fb02c02
	.word	0x9fc02c02
	.word	0x9fd02c02
	.word	0x9fe02c02
	.word	0x9ff02c02
	.word	0xa0002c02
	.word	0xa0102c02
	.word	0xa0202c02
	.word	0xa0302c02
	.word	0xa0402c02
	.word	0xa0502c02
	.word	0xa0602c02
	.word	0xa0702c02
	.word	0xa0802c02
	.word	0xa0902c02
	.word	0xa0a02c02
	.word	0xa0b02c02
	.word	0xa0c02c02
	.word	0xa0d02c02
	.word	0xa0e02c02
	.word	0xa0f02c02
	.word	0xa1002c02
	.word	0xa1102c02
	.word	0xa1202c02
	.word	0xa1302c02
	.word	0xa1402c02
	.word	0xa1502c02
	.word	0xa1602c02
	.word	0xa1702c02
	.word	0xa1802c02
	.word	0xa1902c02
	.word	0xa1a02c02
	.word	0xa1b02c02
	.word	0xa1c02c02
	.word	0xa1d02c02
	.word	0xa1e02c02
	.word	0xa1f02c02
	.word	0xa2002c02
	.word	0xa2102c02
	.word	0xa2202c02
	.word	0xa2302c02
	.word	0xa2402c02
	.word	0xa2502c02
	.word	0xa2602c02
	.word	0xa2702c02
	.word	0xa2802c02
	.word	0xa2902c02
	.word	0xa2a02c02
	.word	0xa2b02c02
	.word	0xa2c02c02
	.word	0xa2d02c02
	.word	0xa2e02c02
	.word	0xa2f02c02
	.word	0xa3002c02
	.word	0xa3102c02
	.word	0xa3202c02
	.word	0xa3302c02
	.word	0xa3402c02
	.word	0xa3502c02
	.word	0xa3602c02
	.word	0xa3702c02
	.word	0xa3802c02
	.word	0xa3902c02
	.word	0xa3a02c02
	.word	0xa3b02c02
	.word	0xa3c02c02
	.word	0xa3d02c02
	.word	0xa3e02c02
	.word	0xa3f02c02
	.word	0xa4002c02
	.word	0xa4102c02
	.word	0xa4202c02
	.word	0xa4302c02
	.word	0xa4402c02
	.word	0xa4502c02
	.word	0xa4602c02
	.word	0xa4702c02
	.word	0xa4802c02
	.word	0xa4902c02
	.word	0xa4a02c02
	.word	0xa4b02c02
	.word	0xa4c02c02
	.word	0xa4d02c02
	.word	0xa4e02c02
	.word	0xa4f02c02
	.word	0xa5002c02
	.word	0xa5102c02
	.word	0xa5202c02
	.word	0xa5302c02
	.word	0xa5402c02
	.word	0xa5502c02
	.word	0xa5602c02
	.word	0xa5702c02
	.word	0xa5802c02
	.word	0xa5902c02
	.word	0xa5a02c02
	.word	0xa5b02c02
	.word	0xa5c02c02
	.word	0xa5d02c02
	.word	0xa5e02c02
	.word	0xa5f02c02
	.word	0xa6002c02
	.word	0xa6102c02
	.word	0xa6202c02
	.word	0xa6302c02
	.word	0xa6402c02
	.word	0xa6502c02
	.word	0xa6602c02
	.word	0xa6702c02
	.word	0xa6802c02
	.word	0xa6902c02
	.word	0xa6a02c02
	.word	0xa6b02c02
	.word	0xa6c02c02
	.word	0xa6d02c02
	.word	0xa6e02c02
	.word	0xa6f02c02
	.word	0xa7002c02
	.word	0xa7102c02
	.word	0xa7202c02
	.word	0xa7302c02
	.word	0xa7402c02
	.word	0xa7502c02
	.word	0xa7602c02
	.word	0xa7702c02
	.word	0xa7802c02
	.word	0xa7902c02
	.word	0xa7a02c02
	.word	0xa7b02c02
	.word	0xa7c02c02
	.word	0xa7d02c02
	.word	0xa7e02c02
	.word	0xa7f02c02
	.word	0xa8002c02
	.word	0xa8102c02
	.word	0xa8202c02
	.word	0xa8302c02
	.word	0xa8402c02
	.word	0xa8502c02
	.word	0xa8602c02
	.word	0xa8702c02
	.word	0xa8802c02
	.word	0xa8902c02
	.word	0xa8a02c02
	.word	0xa8b02c02
	.word	0xa8c02c02
	.word	0xa8d02c02
	.word	0xa8e02c02
	.word	0xa8f02c02
	.word	0xa9002c02
	.word	0xa9102c02
	.word	0xa9202c02
	.word	0xa9302c02
	.word	0xa9402c02
	.word	0xa9502c02
	.word	0xa9602c02
	.word	0xa9702c02
	.word	0xa9802c02
	.word	0xa9902c02
	.word	0xa9a02c02
	.word	0xa9b02c02
	.word	0xa9c02c02
	.word	0xa9d02c02
	.word	0xa9e02c02
	.word	0xa9f02c02
	.word	0xaa002c02
	.word	0xaa102c02
	.word	0xaa202c02
	.word	0xaa302c02
	.word	0xaa402c02
	.word	0xaa502c02
	.word	0xaa602c02
	.word	0xaa702c02
	.word	0xaa802c02
	.word	0xaa902c02
	.word	0xaaa02c02
	.word	0xaab02c02
	.word	0xaac02c02
	.word	0xaad02c02
	.word	0xaae02c02
	.word	0xaaf02c02
	.word	0xab002c02
	.word	0xab102c02
	.word	0xab202c02
	.word	0xab302c02
	.word	0xab402c02
	.word	0xab502c02
	.word	0xab602c02
	.word	0xab702c02
	.word	0xab802c02
	.word	0xab902c02
	.word	0xaba02c02
	.word	0xabb02c02
	.word	0xabc02c02
	.word	0xabd02c02
	.word	0xabe02c02
	.word	0xabf02c02
	.word	0xac002c02
	.word	0xac102c02
	.word	0xac202c02
	.word	0xac302c02
	.word	0xac402c02
	.word	0xac502c02
	.word	0xac602c02
	.word	0xac702c02
	.word	0xac802c02
	.word	0xac902c02
	.word	0xaca02c02
	.word	0xacb02c02
	.word	0xacc02c02
	.word	0xacd02c02
	.word	0xace02c02
	.word	0xacf02c02
	.word	0xad002c02
	.word	0xad102c02
	.word	0xad202c02
	.word	0xad302c02
	.word	0xad402c02
	.word	0xad502c02
	.word	0xad602c02
	.word	0xad702c02
	.word	0xad802c02
	.word	0xad902c02
	.word	0xada02c02
	.word	0xadb02c02
	.word	0xadc02c02
	.word	0xadd02c02
	.word	0xade02c02
	.word	0xadf02c02
	.word	0xae002c02
	.word	0xae102c02
	.word	0xae202c02
	.word	0xae302c02
	.word	0xae402c02
	.word	0xae502c02
	.word	0xae602c02
	.word	0xae702c02
	.word	0xae802c02
	.word	0xae902c02
	.word	0xaea02c02
	.word	0xaeb02c02
	.word	0xaec02c02
	.word	0xaed02c02
	.word	0xaee02c02
	.word	0xaef02c02
	.word	0xaf002c02
	.word	0xaf102c02
	.word	0xaf202c02
	.word	0xaf302c02
	.word	0xaf402c02
	.word	0xaf502c02
	.word	0xaf602c02
	.word	0xaf702c02
	.word	0xaf802c02
	.word	0xaf902c02
	.word	0xafa02c02
	.word	0xafb02c02
	.word	0xafc02c02
	.word	0xafd02c02
	.word	0xafe02c02
	.word	0xaff02c02
	.word	0xb0002c02
	.word	0xb0102c02
	.word	0xb0202c02
	.word	0xb0302c02
	.word	0xb0402c02
	.word	0xb0502c02
	.word	0xb0602c02
	.word	0xb0702c02
	.word	0xb0802c02
	.word	0xb0902c02
	.word	0xb0a02c02
	.word	0xb0b02c02
	.word	0xb0c02c02
	.word	0xb0d02c02
	.word	0xb0e02c02
	.word	0xb0f02c02
	.word	0xb1002c02
	.word	0xb1102c02
	.word	0xb1202c02
	.word	0xb1302c02
	.word	0xb1402c02
	.word	0xb1502c02
	.word	0xb1602c02
	.word	0xb1702c02
	.word	0xb1802c02
	.word	0xb1902c02
	.word	0xb1a02c02
	.word	0xb1b02c02
	.word	0xb1c02c02
	.word	0xb1d02c02
	.word	0xb1e02c02
	.word	0xb1f02c02
	.word	0xb2002c02
	.word	0xb2102c02
	.word	0xb2202c02
	.word	0xb2302c02
	.word	0xb2402c02
	.word	0xb2502c02
	.word	0xb2602c02
	.word	0xb2702c02
	.word	0xb2802c02
	.word	0xb2902c02
	.word	0xb2a02c02
	.word	0xb2b02c02
	.word	0xb2c02c02
	.word	0xb2d02c02
	.word	0xb2e02c02
	.word	0xb2f02c02
	.word	0xb3002c02
	.word	0xb3102c02
	.word	0xb3202c02
	.word	0xb3302c02
	.word	0xb3402c02
	.word	0xb3502c02
	.word	0xb3602c02
	.word	0xb3702c02
	.word	0xb3802c02
	.word	0xb3902c02
	.word	0xb3a02c02
	.word	0xb3b02c02
	.word	0xb3c02c02
	.word	0xb3d02c02
	.word	0xb3e02c02
	.word	0xb3f02c02
	.word	0xb4002c02
	.word	0xb4102c02
	.word	0xb4202c02
	.word	0xb4302c02
	.word	0xb4402c02
	.word	0xb4502c02
	.word	0xb4602c02
	.word	0xb4702c02
	.word	0xb4802c02
	.word	0xb4902c02
	.word	0xb4a02c02
	.word	0xb4b02c02
	.word	0xb4c02c02
	.word	0xb4d02c02
	.word	0xb4e02c02
	.word	0xb4f02c02
	.word	0xb5002c02
	.word	0xb5102c02
	.word	0xb5202c02
	.word	0xb5302c02
	.word	0xb5402c02
	.word	0xb5502c02
	.word	0xb5602c02
	.word	0xb5702c02
	.word	0xb5802c02
	.word	0xb5902c02
	.word	0xb5a02c02
	.word	0xb5b02c02
	.word	0xb5c02c02
	.word	0xb5d02c02
	.word	0xb5e02c02
	.word	0xb5f02c02
	.word	0xb6002c02
	.word	0xb6102c02
	.word	0xb6202c02
	.word	0xb6302c02
	.word	0xb6402c02
	.word	0xb6502c02
	.word	0xb6602c02
	.word	0xb6702c02
	.word	0xb6802c02
	.word	0xb6902c02
	.word	0xb6a02c02
	.word	0xb6b02c02
	.word	0xb6c02c02
	.word	0xb6d02c02
	.word	0xb6e02c02
	.word	0xb6f02c02
	.word	0xb7002c02
	.word	0xb7102c02
	.word	0xb7202c02
	.word	0xb7302c02
	.word	0xb7402c02
	.word	0xb7502c02
	.word	0xb7602c02
	.word	0xb7702c02
	.word	0xb7802c02
	.word	0xb7902c02
	.word	0xb7a02c02
	.word	0xb7b02c02
	.word	0xb7c02c02
	.word	0xb7d02c02
	.word	0xb7e02c02
	.word	0xb7f02c02
	.word	0xb8002c02
	.word	0xb8102c02
	.word	0xb8202c02
	.word	0xb8302c02
	.word	0xb8402c02
	.word	0xb8502c02
	.word	0xb8602c02
	.word	0xb8702c02
	.word	0xb8802c02
	.word	0xb8902c02
	.word	0xb8a02c02
	.word	0xb8b02c02
	.word	0xb8c02c02
	.word	0xb8d02c02
	.word	0xb8e02c02
	.word	0xb8f02c02
	.word	0xb9002c02
	.word	0xb9102c02
	.word	0xb9202c02
	.word	0xb9302c02
	.word	0xb9402c02
	.word	0xb9502c02
	.word	0xb9602c02
	.word	0xb9702c02
	.word	0xb9802c02
	.word	0xb9902c02
	.word	0xb9a02c02
	.word	0xb9b02c02
	.word	0xb9c02c02
	.word	0xb9d02c02
	.word	0xb9e02c02
	.word	0xb9f02c02
	.word	0xba002c02
	.word	0xba102c02
	.word	0xba202c02
	.word	0xba302c02
	.word	0xba402c02
	.word	0xba502c02
	.word	0xba602c02
	.word	0xba702c02
	.word	0xba802c02
	.word	0xba902c02
	.word	0xbaa02c02
	.word	0xbab02c02
	.word	0xbac02c02
	.word	0xbad02c02
	.word	0xbae02c02
	.word	0xbaf02c02
	.word	0xbb002c02
	.word	0xbb102c02
	.word	0xbb202c02
	.word	0xbb302c02
	.word	0xbb402c02
	.word	0xbb502c02
	.word	0xbb602c02
	.word	0xbb702c02
	.word	0xbb802c02
	.word	0xbb902c02
	.word	0xbba02c02
	.word	0xbbb02c02
	.word	0xbbc02c02
	.word	0xbbd02c02
	.word	0xbbe02c02
	.word	0xbbf02c02
	.word	0xbc002c02
	.word	0xbc102c02
	.word	0xbc202c02
	.word	0xbc302c02
	.word	0xbc402c02
	.word	0xbc502c02
	.word	0xbc602c02
	.word	0xbc702c02
	.word	0xbc802c02
	.word	0xbc902c02
	.word	0xbca02c02
	.word	0xbcb02c02
	.word	0xbcc02c02
	.word	0xbcd02c02
	.word	0xbce02c02
	.word	0xbcf02c02
	.word	0xbd002c02
	.word	0xbd102c02
	.word	0xbd202c02
	.word	0xbd302c02
	.word	0xbd402c02
	.word	0xbd502c02
	.word	0xbd602c02
	.word	0xbd702c02
	.word	0xbd802c02
	.word	0xbd902c02
	.word	0xbda02c02
	.word	0xbdb02c02
	.word	0xbdc02c02
	.word	0xbdd02c02
	.word	0xbde02c02
	.word	0xbdf02c02
	.word	0xbe002c02
	.word	0xbe102c02
	.word	0xbe202c02
	.word	0xbe302c02
	.word	0xbe402c02
	.word	0xbe502c02
	.word	0xbe602c02
	.word	0xbe702c02
	.word	0xbe802c02
	.word	0xbe902c02
	.word	0xbea02c02
	.word	0xbeb02c02
	.word	0xbec02c02
	.word	0xbed02c02
	.word	0xbee02c02
	.word	0xbef02c02
	.word	0xbf002c02
	.word	0xbf102c02
	.word	0xbf202c02
	.word	0xbf302c02
	.word	0xbf402c02
	.word	0xbf502c02
	.word	0xbf602c02
	.word	0xbf702c02
	.word	0xbf802c02
	.word	0xbf902c02
	.word	0xbfa02c02
	.word	0xbfb02c02
	.word	0xbfc02c02
	.word	0xbfd02c02
	.word	0xbfe02c02
	.word	0xbff02c02
	.word	0xc0015ca2
	.word	0xc0102c02
	.word	0xc0202c02
	.word	0xc0302c02
	.word	0xc0402c02
	.word	0xc0502c02
	.word	0xc0602c02
	.word	0xc0702c02
	.word	0xc0802c02
	.word	0xc0902c02
	.word	0xc0a02c02
	.word	0xc0b02c02
	.word	0xc0c02c02
	.word	0xc0d02c02
	.word	0xc0e02c02
	.word	0xc0f02c02
	.word	0xc1002c02
	.word	0xc1102c02
	.word	0xc1202c02
	.word	0xc1302c02
	.word	0xc1402c02
	.word	0xc1502c02
	.word	0xc1602c02
	.word	0xc1702c02
	.word	0xc1802c02
	.word	0xc1902c02
	.word	0xc1a02c02
	.word	0xc1b02c02
	.word	0xc1c02c02
	.word	0xc1d02c02
	.word	0xc1e02c02
	.word	0xc1f02c02
	.word	0xc2002c02
	.word	0xc2102c02
	.word	0xc2202c02
	.word	0xc2302c02
	.word	0xc2402c02
	.word	0xc2502c02
	.word	0xc2602c02
	.word	0xc2702c02
	.word	0xc2802c02
	.word	0xc2902c02
	.word	0xc2a02c02
	.word	0xc2b02c02
	.word	0xc2c02c02
	.word	0xc2d02c02
	.word	0xc2e02c02
	.word	0xc2f02c02
	.word	0xc3002c02
	.word	0xc3102c02
	.word	0xc3202c02
	.word	0xc3302c02
	.word	0xc3402c02
	.word	0xc3502c02
	.word	0xc3602c02
	.word	0xc3702c02
	.word	0xc3802c02
	.word	0xc3902c02
	.word	0xc3a02c02
	.word	0xc3b02c02
	.word	0xc3c02c02
	.word	0xc3d02c02
	.word	0xc3e02c02
	.word	0xc3f02c02
	.word	0xc4002c02
	.word	0xc4102c02
	.word	0xc4202c02
	.word	0xc4302c02
	.word	0xc4402c02
	.word	0xc4502c02
	.word	0xc4602c02
	.word	0xc4702c02
	.word	0xc4802c02
	.word	0xc4902c02
	.word	0xc4a02c02
	.word	0xc4b02c02
	.word	0xc4c02c02
	.word	0xc4d02c02
	.word	0xc4e02c02
	.word	0xc4f02c02
	.word	0xc5002c02
	.word	0xc5102c02
	.word	0xc5202c02
	.word	0xc5302c02
	.word	0xc5402c02
	.word	0xc5502c02
	.word	0xc5602c02
	.word	0xc5702c02
	.word	0xc5802c02
	.word	0xc5902c02
	.word	0xc5a02c02
	.word	0xc5b02c02
	.word	0xc5c02c02
	.word	0xc5d02c02
	.word	0xc5e02c02
	.word	0xc5f02c02
	.word	0xc6002c02
	.word	0xc6102c02
	.word	0xc6202c02
	.word	0xc6302c02
	.word	0xc6402c02
	.word	0xc6502c02
	.word	0xc6602c02
	.word	0xc6702c02
	.word	0xc6802c02
	.word	0xc6902c02
	.word	0xc6a02c02
	.word	0xc6b02c02
	.word	0xc6c02c02
	.word	0xc6d02c02
	.word	0xc6e02c02
	.word	0xc6f02c02
	.word	0xc7002c02
	.word	0xc7102c02
	.word	0xc7202c02
	.word	0xc7302c02
	.word	0xc7402c02
	.word	0xc7502c02
	.word	0xc7602c02
	.word	0xc7702c02
	.word	0xc7802c02
	.word	0xc7902c02
	.word	0xc7a02c02
	.word	0xc7b02c02
	.word	0xc7c02c02
	.word	0xc7d02c02
	.word	0xc7e02c02
	.word	0xc7f02c02
	.word	0xc8002c02
	.word	0xc8102c02
	.word	0xc8202c02
	.word	0xc8302c02
	.word	0xc8402c02
	.word	0xc8502c02
	.word	0xc8602c02
	.word	0xc8702c02
	.word	0xc8802c02
	.word	0xc8902c02
	.word	0xc8a02c02
	.word	0xc8b02c02
	.word	0xc8c02c02
	.word	0xc8d02c02
	.word	0xc8e02c02
	.word	0xc8f02c02
	.word	0xc9002c02
	.word	0xc9102c02
	.word	0xc9202c02
	.word	0xc9302c02
	.word	0xc9402c02
	.word	0xc9502c02
	.word	0xc9602c02
	.word	0xc9702c02
	.word	0xc9802c02
	.word	0xc9902c02
	.word	0xc9a02c02
	.word	0xc9b02c02
	.word	0xc9c02c02
	.word	0xc9d02c02
	.word	0xc9e02c02
	.word	0xc9f02c02
	.word	0xca002c02
	.word	0xca102c02
	.word	0xca202c02
	.word	0xca302c02
	.word	0xca402c02
	.word	0xca502c02
	.word	0xca602c02
	.word	0xca702c02
	.word	0xca802c02
	.word	0xca902c02
	.word	0xcaa02c02
	.word	0xcab02c02
	.word	0xcac02c02
	.word	0xcad02c02
	.word	0xcae02c02
	.word	0xcaf02c02
	.word	0xcb002c02
	.word	0xcb102c02
	.word	0xcb202c02
	.word	0xcb302c02
	.word	0xcb402c02
	.word	0xcb502c02
	.word	0xcb602c02
	.word	0xcb702c02
	.word	0xcb802c02
	.word	0xcb902c02
	.word	0xcba02c02
	.word	0xcbb02c02
	.word	0xcbc02c02
	.word	0xcbd02c02
	.word	0xcbe02c02
	.word	0xcbf02c02
	.word	0xcc002c02
	.word	0xcc102c02
	.word	0xcc202c02
	.word	0xcc302c02
	.word	0xcc402c02
	.word	0xcc502c02
	.word	0xcc602c02
	.word	0xcc702c02
	.word	0xcc802c02
	.word	0xcc902c02
	.word	0xcca02c02
	.word	0xccb02c02
	.word	0xccc02c02
	.word	0xccd02c02
	.word	0xcce02c02
	.word	0xccf02c02
	.word	0xcd002c02
	.word	0xcd102c02
	.word	0xcd202c02
	.word	0xcd302c02
	.word	0xcd402c02
	.word	0xcd502c02
	.word	0xcd602c02
	.word	0xcd702c02
	.word	0xcd802c02
	.word	0xcd902c02
	.word	0xcda02c02
	.word	0xcdb02c02
	.word	0xcdc02c02
	.word	0xcdd02c02
	.word	0xcde02c02
	.word	0xcdf02c02
	.word	0xce002c02
	.word	0xce102c02
	.word	0xce202c02
	.word	0xce302c02
	.word	0xce402c02
	.word	0xce502c02
	.word	0xce602c02
	.word	0xce702c02
	.word	0xce802c02
	.word	0xce902c02
	.word	0xcea02c02
	.word	0xceb02c02
	.word	0xcec02c02
	.word	0xced02c02
	.word	0xcee02c02
	.word	0xcef02c02
	.word	0xcf002c02
	.word	0xcf102c02
	.word	0xcf202c02
	.word	0xcf302c02
	.word	0xcf402c02
	.word	0xcf502c02
	.word	0xcf602c02
	.word	0xcf702c02
	.word	0xcf802c02
	.word	0xcf902c02
	.word	0xcfa02c02
	.word	0xcfb02c02
	.word	0xcfc02c02
	.word	0xcfd02c02
	.word	0xcfe02c02
	.word	0xcff02c02
	.word	0xd0002c02
	.word	0xd0102c02
	.word	0xd0202c02
	.word	0xd0302c02
	.word	0xd0402c02
	.word	0xd0502c02
	.word	0xd0602c02
	.word	0xd0702c02
	.word	0xd0802c02
	.word	0xd0902c02
	.word	0xd0a02c02
	.word	0xd0b02c02
	.word	0xd0c02c02
	.word	0xd0d02c02
	.word	0xd0e02c02
	.word	0xd0f02c02
	.word	0xd1002c02
	.word	0xd1102c02
	.word	0xd1202c02
	.word	0xd1302c02
	.word	0xd1402c02
	.word	0xd1502c02
	.word	0xd1602c02
	.word	0xd1702c02
	.word	0xd1802c02
	.word	0xd1902c02
	.word	0xd1a02c02
	.word	0xd1b02c02
	.word	0xd1c02c02
	.word	0xd1d02c02
	.word	0xd1e02c02
	.word	0xd1f02c02
	.word	0xd2002c02
	.word	0xd2102c02
	.word	0xd2202c02
	.word	0xd2302c02
	.word	0xd2402c02
	.word	0xd2502c02
	.word	0xd2602c02
	.word	0xd2702c02
	.word	0xd2802c02
	.word	0xd2902c02
	.word	0xd2a02c02
	.word	0xd2b02c02
	.word	0xd2c02c02
	.word	0xd2d02c02
	.word	0xd2e02c02
	.word	0xd2f02c02
	.word	0xd3002c02
	.word	0xd3102c02
	.word	0xd3202c02
	.word	0xd3302c02
	.word	0xd3402c02
	.word	0xd3502c02
	.word	0xd3602c02
	.word	0xd3702c02
	.word	0xd3802c02
	.word	0xd3902c02
	.word	0xd3a02c02
	.word	0xd3b02c02
	.word	0xd3c02c02
	.word	0xd3d02c02
	.word	0xd3e02c02
	.word	0xd3f02c02
	.word	0xd4002c02
	.word	0xd4102c02
	.word	0xd4202c02
	.word	0xd4302c02
	.word	0xd4402c02
	.word	0xd4502c02
	.word	0xd4602c02
	.word	0xd4702c02
	.word	0xd4802c02
	.word	0xd4902c02
	.word	0xd4a02c02
	.word	0xd4b02c02
	.word	0xd4c02c02
	.word	0xd4d02c02
	.word	0xd4e02c02
	.word	0xd4f02c02
	.word	0xd5002c02
	.word	0xd5102c02
	.word	0xd5202c02
	.word	0xd5302c02
	.word	0xd5402c02
	.word	0xd5502c02
	.word	0xd5602c02
	.word	0xd5702c02
	.word	0xd5802c02
	.word	0xd5902c02
	.word	0xd5a02c02
	.word	0xd5b02c02
	.word	0xd5c02c02
	.word	0xd5d02c02
	.word	0xd5e02c02
	.word	0xd5f02c02
	.word	0xd6002c02
	.word	0xd6102c02
	.word	0xd6202c02
	.word	0xd6302c02
	.word	0xd6402c02
	.word	0xd6502c02
	.word	0xd6602c02
	.word	0xd6702c02
	.word	0xd6802c02
	.word	0xd6902c02
	.word	0xd6a02c02
	.word	0xd6b02c02
	.word	0xd6c02c02
	.word	0xd6d02c02
	.word	0xd6e02c02
	.word	0xd6f02c02
	.word	0xd7002c02
	.word	0xd7102c02
	.word	0xd7202c02
	.word	0xd7302c02
	.word	0xd7402c02
	.word	0xd7502c02
	.word	0xd7602c02
	.word	0xd7702c02
	.word	0xd7802c02
	.word	0xd7902c02
	.word	0xd7a02c02
	.word	0xd7b02c02
	.word	0xd7c02c02
	.word	0xd7d02c02
	.word	0xd7e02c02
	.word	0xd7f02c02
	.word	0xd8002c02
	.word	0xd8102c02
	.word	0xd8202c02
	.word	0xd8302c02
	.word	0xd8402c02
	.word	0xd8502c02
	.word	0xd8602c02
	.word	0xd8702c02
	.word	0xd8802c02
	.word	0xd8902c02
	.word	0xd8a02c02
	.word	0xd8b02c02
	.word	0xd8c02c02
	.word	0xd8d02c02
	.word	0xd8e02c02
	.word	0xd8f02c02
	.word	0xd9002c02
	.word	0xd9102c02
	.word	0xd9202c02
	.word	0xd9302c02
	.word	0xd9402c02
	.word	0xd9502c02
	.word	0xd9602c02
	.word	0xd9702c02
	.word	0xd9802c02
	.word	0xd9902c02
	.word	0xd9a02c02
	.word	0xd9b02c02
	.word	0xd9c02c02
	.word	0xd9d02c02
	.word	0xd9e02c02
	.word	0xd9f02c02
	.word	0xda002c02
	.word	0xda102c02
	.word	0xda202c02
	.word	0xda302c02
	.word	0xda402c02
	.word	0xda502c02
	.word	0xda602c02
	.word	0xda702c02
	.word	0xda802c02
	.word	0xda902c02
	.word	0xdaa02c02
	.word	0xdab02c02
	.word	0xdac02c02
	.word	0xdad02c02
	.word	0xdae02c02
	.word	0xdaf02c02
	.word	0xdb002c02
	.word	0xdb102c02
	.word	0xdb202c02
	.word	0xdb302c02
	.word	0xdb402c02
	.word	0xdb502c02
	.word	0xdb602c02
	.word	0xdb702c02
	.word	0xdb802c02
	.word	0xdb902c02
	.word	0xdba02c02
	.word	0xdbb02c02
	.word	0xdbc02c02
	.word	0xdbd02c02
	.word	0xdbe02c02
	.word	0xdbf02c02
	.word	0xdc002c02
	.word	0xdc102c02
	.word	0xdc202c02
	.word	0xdc302c02
	.word	0xdc402c02
	.word	0xdc502c02
	.word	0xdc602c02
	.word	0xdc702c02
	.word	0xdc802c02
	.word	0xdc902c02
	.word	0xdca02c02
	.word	0xdcb02c02
	.word	0xdcc02c02
	.word	0xdcd02c02
	.word	0xdce02c02
	.word	0xdcf02c02
	.word	0xdd002c02
	.word	0xdd102c02
	.word	0xdd202c02
	.word	0xdd302c02
	.word	0xdd402c02
	.word	0xdd502c02
	.word	0xdd602c02
	.word	0xdd702c02
	.word	0xdd802c02
	.word	0xdd902c02
	.word	0xdda02c02
	.word	0xddb02c02
	.word	0xddc02c02
	.word	0xddd02c02
	.word	0xdde02c02
	.word	0xddf02c02
	.word	0xde002c02
	.word	0xde102c02
	.word	0xde202c02
	.word	0xde302c02
	.word	0xde402c02
	.word	0xde502c02
	.word	0xde602c02
	.word	0xde702c02
	.word	0xde802c02
	.word	0xde902c02
	.word	0xdea02c02
	.word	0xdeb02c02
	.word	0xdec02c02
	.word	0xded02c02
	.word	0xdee02c02
	.word	0xdef02c02
	.word	0xdf002c02
	.word	0xdf102c02
	.word	0xdf202c02
	.word	0xdf302c02
	.word	0xdf402c02
	.word	0xdf502c02
	.word	0xdf602c02
	.word	0xdf702c02
	.word	0xdf802c02
	.word	0xdf902c02
	.word	0xdfa02c02
	.word	0xdfb02c02
	.word	0xdfc02c02
	.word	0xdfd02c02
	.word	0xdfe02c02
	.word	0xdff02c02
	.word	0xe0002c02
	.word	0xe0102c02
	.word	0xe0202c02
	.word	0xe0302c02
	.word	0xe0402c02
	.word	0xe0502c02
	.word	0xe0602c02
	.word	0xe0702c02
	.word	0xe0802c02
	.word	0xe0902c02
	.word	0xe0a02c02
	.word	0xe0b02c02
	.word	0xe0c02c02
	.word	0xe0d02c02
	.word	0xe0e02c02
	.word	0xe0f02c02
	.word	0xe1002c02
	.word	0xe1102c02
	.word	0xe1202c02
	.word	0xe1302c02
	.word	0xe1402c02
	.word	0xe1502c02
	.word	0xe1602c02
	.word	0xe1702c02
	.word	0xe1802c02
	.word	0xe1902c02
	.word	0xe1a02c02
	.word	0xe1b02c02
	.word	0xe1c02c02
	.word	0xe1d02c02
	.word	0xe1e02c02
	.word	0xe1f02c02
	.word	0xe2002c02
	.word	0xe2102c02
	.word	0xe2202c02
	.word	0xe2302c02
	.word	0xe2402c02
	.word	0xe2502c02
	.word	0xe2602c02
	.word	0xe2702c02
	.word	0xe2802c02
	.word	0xe2902c02
	.word	0xe2a02c02
	.word	0xe2b02c02
	.word	0xe2c02c02
	.word	0xe2d02c02
	.word	0xe2e02c02
	.word	0xe2f02c02
	.word	0xe3002c02
	.word	0xe3102c02
	.word	0xe3202c02
	.word	0xe3302c02
	.word	0xe3402c02
	.word	0xe3502c02
	.word	0xe3602c02
	.word	0xe3702c02
	.word	0xe3802c02
	.word	0xe3902c02
	.word	0xe3a02c02
	.word	0xe3b02c02
	.word	0xe3c02c02
	.word	0xe3d02c02
	.word	0xe3e02c02
	.word	0xe3f02c02
	.word	0xe4002c02
	.word	0xe4102c02
	.word	0xe4202c02
	.word	0xe4302c02
	.word	0xe4402c02
	.word	0xe4502c02
	.word	0xe4602c02
	.word	0xe4702c02
	.word	0xe4802c02
	.word	0xe4902c02
	.word	0xe4a02c02
	.word	0xe4b02c02
	.word	0xe4c02c02
	.word	0xe4d02c02
	.word	0xe4e02c02
	.word	0xe4f02c02
	.word	0xe5002c02
	.word	0xe5102c02
	.word	0xe5202c02
	.word	0xe5302c02
	.word	0xe5402c02
	.word	0xe5502c02
	.word	0xe5602c02
	.word	0xe5702c02
	.word	0xe5802c02
	.word	0xe5902c02
	.word	0xe5a02c02
	.word	0xe5b02c02
	.word	0xe5c02c02
	.word	0xe5d02c02
	.word	0xe5e02c02
	.word	0xe5f02c02
	.word	0xe6002c02
	.word	0xe6102c02
	.word	0xe6202c02
	.word	0xe6302c02
	.word	0xe6402c02
	.word	0xe6502c02
	.word	0xe6602c02
	.word	0xe6702c02
	.word	0xe6802c02
	.word	0xe6902c02
	.word	0xe6a02c02
	.word	0xe6b02c02
	.word	0xe6c02c02
	.word	0xe6d02c02
	.word	0xe6e02c02
	.word	0xe6f02c02
	.word	0xe7002c02
	.word	0xe7102c02
	.word	0xe7202c02
	.word	0xe7302c02
	.word	0xe7402c02
	.word	0xe7502c02
	.word	0xe7602c02
	.word	0xe7702c02
	.word	0xe7802c02
	.word	0xe7902c02
	.word	0xe7a02c02
	.word	0xe7b02c02
	.word	0xe7c02c02
	.word	0xe7d02c02
	.word	0xe7e02c02
	.word	0xe7f02c02
	.word	0xe8002c02
	.word	0xe8102c02
	.word	0xe8202c02
	.word	0xe8302c02
	.word	0xe8402c02
	.word	0xe8502c02
	.word	0xe8602c02
	.word	0xe8702c02
	.word	0xe8802c02
	.word	0xe8902c02
	.word	0xe8a02c02
	.word	0xe8b02c02
	.word	0xe8c02c02
	.word	0xe8d02c02
	.word	0xe8e02c02
	.word	0xe8f02c02
	.word	0xe9002c02
	.word	0xe9102c02
	.word	0xe9202c02
	.word	0xe9302c02
	.word	0xe9402c02
	.word	0xe9502c02
	.word	0xe9602c02
	.word	0xe9702c02
	.word	0xe9802c02
	.word	0xe9902c02
	.word	0xe9a02c02
	.word	0xe9b02c02
	.word	0xe9c02c02
	.word	0xe9d02c02
	.word	0xe9e02c02
	.word	0xe9f02c02
	.word	0xea002c02
	.word	0xea102c02
	.word	0xea202c02
	.word	0xea302c02
	.word	0xea402c02
	.word	0xea502c02
	.word	0xea602c02
	.word	0xea702c02
	.word	0xea802c02
	.word	0xea902c02
	.word	0xeaa02c02
	.word	0xeab02c02
	.word	0xeac02c02
	.word	0xead02c02
	.word	0xeae02c02
	.word	0xeaf02c02
	.word	0xeb002c02
	.word	0xeb102c02
	.word	0xeb202c02
	.word	0xeb302c02
	.word	0xeb402c02
	.word	0xeb502c02
	.word	0xeb602c02
	.word	0xeb702c02
	.word	0xeb802c02
	.word	0xeb902c02
	.word	0xeba02c02
	.word	0xebb02c02
	.word	0xebc02c02
	.word	0xebd02c02
	.word	0xebe02c02
	.word	0xebf02c02
	.word	0xec002c02
	.word	0xec102c02
	.word	0xec202c02
	.word	0xec302c02
	.word	0xec402c02
	.word	0xec502c02
	.word	0xec602c02
	.word	0xec702c02
	.word	0xec802c02
	.word	0xec902c02
	.word	0xeca02c02
	.word	0xecb02c02
	.word	0xecc02c02
	.word	0xecd02c02
	.word	0xece02c02
	.word	0xecf02c02
	.word	0xed002c02
	.word	0xed102c02
	.word	0xed202c02
	.word	0xed302c02
	.word	0xed402c02
	.word	0xed502c02
	.word	0xed602c02
	.word	0xed702c02
	.word	0xed802c02
	.word	0xed902c02
	.word	0xeda02c02
	.word	0xedb02c02
	.word	0xedc02c02
	.word	0xedd02c02
	.word	0xede02c02
	.word	0xedf02c02
	.word	0xee002c02
	.word	0xee102c02
	.word	0xee202c02
	.word	0xee302c02
	.word	0xee402c02
	.word	0xee502c02
	.word	0xee602c02
	.word	0xee702c02
	.word	0xee802c02
	.word	0xee902c02
	.word	0xeea02c02
	.word	0xeeb02c02
	.word	0xeec02c02
	.word	0xeed02c02
	.word	0xeee02c02
	.word	0xeef02c02
	.word	0xef002c02
	.word	0xef102c02
	.word	0xef202c02
	.word	0xef302c02
	.word	0xef402c02
	.word	0xef502c02
	.word	0xef602c02
	.word	0xef702c02
	.word	0xef802c02
	.word	0xef902c02
	.word	0xefa02c02
	.word	0xefb02c02
	.word	0xefc02c02
	.word	0xefd02c02
	.word	0xefe02c02
	.word	0xeff02c02
	.word	0xf0002c02
	.word	0xf0102c02
	.word	0xf0202c02
	.word	0xf0302c02
	.word	0xf0402c02
	.word	0xf0502c02
	.word	0xf0602c02
	.word	0xf0702c02
	.word	0xf0802c02
	.word	0xf0902c02
	.word	0xf0a02c02
	.word	0xf0b02c02
	.word	0xf0c02c02
	.word	0xf0d02c02
	.word	0xf0e02c02
	.word	0xf0f02c02
	.word	0xf1002c02
	.word	0xf1102c02
	.word	0xf1202c02
	.word	0xf1302c02
	.word	0xf1402c02
	.word	0xf1502c02
	.word	0xf1602c02
	.word	0xf1702c02
	.word	0xf1802c02
	.word	0xf1902c02
	.word	0xf1a02c02
	.word	0xf1b02c02
	.word	0xf1c02c02
	.word	0xf1d02c02
	.word	0xf1e02c02
	.word	0xf1f02c02
	.word	0xf2002c02
	.word	0xf2102c02
	.word	0xf2202c02
	.word	0xf2302c02
	.word	0xf2402c02
	.word	0xf2502c02
	.word	0xf2602c02
	.word	0xf2702c02
	.word	0xf2802c02
	.word	0xf2902c02
	.word	0xf2a02c02
	.word	0xf2b02c02
	.word	0xf2c02c02
	.word	0xf2d02c02
	.word	0xf2e02c02
	.word	0xf2f02c02
	.word	0xf3002c02
	.word	0xf3102c02
	.word	0xf3202c02
	.word	0xf3302c02
	.word	0xf3402c02
	.word	0xf3502c02
	.word	0xf3602c02
	.word	0xf3702c02
	.word	0xf3802c02
	.word	0xf3902c02
	.word	0xf3a02c02
	.word	0xf3b02c02
	.word	0xf3c02c02
	.word	0xf3d02c02
	.word	0xf3e02c02
	.word	0xf3f02c02
	.word	0xf4002c02
	.word	0xf4102c02
	.word	0xf4202c02
	.word	0xf4302c02
	.word	0xf4402c02
	.word	0xf4502c02
	.word	0xf4602c02
	.word	0xf4702c02
	.word	0xf4802c02
	.word	0xf4902c02
	.word	0xf4a02c02
	.word	0xf4b02c02
	.word	0xf4c02c02
	.word	0xf4d02c02
	.word	0xf4e02c02
	.word	0xf4f02c02
	.word	0xf5002c02
	.word	0xf5102c02
	.word	0xf5202c02
	.word	0xf5302c02
	.word	0xf5402c02
	.word	0xf5502c02
	.word	0xf5602c02
	.word	0xf5702c02
	.word	0xf5802c02
	.word	0xf5902c02
	.word	0xf5a02c02
	.word	0xf5b02c02
	.word	0xf5c02c02
	.word	0xf5d02c02
	.word	0xf5e02c02
	.word	0xf5f02c02
	.word	0xf6002c02
	.word	0xf6102c02
	.word	0xf6202c02
	.word	0xf6302c02
	.word	0xf6402c02
	.word	0xf6502c02
	.word	0xf6602c02
	.word	0xf6702c02
	.word	0xf6802c02
	.word	0xf6902c02
	.word	0xf6a02c02
	.word	0xf6b02c02
	.word	0xf6c02c02
	.word	0xf6d02c02
	.word	0xf6e02c02
	.word	0xf6f02c02
	.word	0xf7002c02
	.word	0xf7102c02
	.word	0xf7202c02
	.word	0xf7302c02
	.word	0xf7402c02
	.word	0xf7502c02
	.word	0xf7602c02
	.word	0xf7702c02
	.word	0xf7802c02
	.word	0xf7902c02
	.word	0xf7a02c02
	.word	0xf7b02c02
	.word	0xf7c02c02
	.word	0xf7d02c02
	.word	0xf7e02c02
	.word	0xf7f02c02
	.word	0xf8002c02
	.word	0xf8102c02
	.word	0xf8202c02
	.word	0xf8302c02
	.word	0xf8402c02
	.word	0xf8502c02
	.word	0xf8602c02
	.word	0xf8702c02
	.word	0xf8802c02
	.word	0xf8902c02
	.word	0xf8a02c02
	.word	0xf8b02c02
	.word	0xf8c02c02
	.word	0xf8d02c02
	.word	0xf8e02c02
	.word	0xf8f02c02
	.word	0xf9002c02
	.word	0xf9102c02
	.word	0xf9202c02
	.word	0xf9302c02
	.word	0xf9402c02
	.word	0xf9502c02
	.word	0xf9602c02
	.word	0xf9702c02
	.word	0xf9802c02
	.word	0xf9902c02
	.word	0xf9a02c02
	.word	0xf9b02c02
	.word	0xf9c02c02
	.word	0xf9d02c02
	.word	0xf9e02c02
	.word	0xf9f02c02
	.word	0xfa002c02
	.word	0xfa102c02
	.word	0xfa202c02
	.word	0xfa302c02
	.word	0xfa402c02
	.word	0xfa502c02
	.word	0xfa602c02
	.word	0xfa702c02
	.word	0xfa802c02
	.word	0xfa902c02
	.word	0xfaa02c02
	.word	0xfab02c02
	.word	0xfac02c02
	.word	0xfad02c02
	.word	0xfae02c02
	.word	0xfaf02c02
	.word	0xfb002c02
	.word	0xfb102c02
	.word	0xfb202c02
	.word	0xfb302c02
	.word	0xfb402c02
	.word	0xfb502c02
	.word	0xfb602c02
	.word	0xfb702c02
	.word	0xfb802c02
	.word	0xfb902c02
	.word	0xfba02c02
	.word	0xfbb02c02
	.word	0xfbc02c02
	.word	0xfbd02c02
	.word	0xfbe02c02
	.word	0xfbf02c02
	.word	0xfc002c02
	.word	0xfc102c02
	.word	0xfc202c02
	.word	0xfc302c02
	.word	0xfc402c02
	.word	0xfc502c02
	.word	0xfc602c02
	.word	0xfc702c02
	.word	0xfc802c02
	.word	0xfc902c02
	.word	0xfca02c02
	.word	0xfcb02c02
	.word	0xfcc02c02
	.word	0xfcd02c02
	.word	0xfce02c02
	.word	0xfcf02c02
	.word	0xfd002c02
	.word	0xfd102c02
	.word	0xfd202c02
	.word	0xfd302c02
	.word	0xfd402c02
	.word	0xfd502c02
	.word	0xfd602c02
	.word	0xfd702c02
	.word	0xfd802c02
	.word	0xfd902c02
	.word	0xfda02c02
	.word	0xfdb02c02
	.word	0xfdc02c02
	.word	0xfdd02c02
	.word	0xfde02c02
	.word	0xfdf02c02
	.word	0xfe002c02
	.word	0xfe102c02
	.word	0xfe202c02
	.word	0xfe302c02
	.word	0xfe402c02
	.word	0xfe502c02
	.word	0xfe602c02
	.word	0xfe702c02
	.word	0xfe802c02
	.word	0xfe902c02
	.word	0xfea02c02
	.word	0xfeb02c02
	.word	0xfec02c02
	.word	0xfed02c02
	.word	0xfee02c02
	.word	0xfef02c02
	.word	0xff002c02
	.word	0xff102c02
	.word	0xff202c02
	.word	0xff302c02
	.word	0xff402c02
	.word	0xff502c02
	.word	0xff602c02
	.word	0xff702c02
	.word	0xff802c02
	.word	0xff902c02
	.word	0xffa02c02
	.word	0xffb02c02
	.word	0xffc02c02
	.word	0xffd02c02
	.word	0xffe02c02
	.word	0xfff02c02
#endif /* CFG_SHMOO */

/* Entry point */
	.section .text.startup

	.globl startup
startup:
	b	tr_rst	/* 0 - reset */
	ldr	pc,_tr_und	/* 4 - undefined instruction */
	ldr	pc,_tr_swi	/* 8 - software interrupt */
	ldr	pc,_tr_iab	/* 0xc - prefetch abort */
	ldr	pc,_tr_dab	/* 0x10 - data abort */
	ldr	pc,_tr_bad	/* 0x14 - reserved */
	ldr	pc,_tr_irq	/* 0x18 - external interrupt */
	ldr	pc,_tr_fiq	/* 0x1c - fast interrupt */

_tr_und:
	.word tr_und
_tr_swi:
	.word tr_swi
_tr_iab:
	.word tr_iab
_tr_dab:
	.word tr_dab
_tr_bad:
	.word tr_bad
_tr_irq:
	.word tr_irq
_tr_fiq:
	.word tr_fiq
_pad:
	.word 0x12345678 /* now 16*4=64 */

	.org	(BISZ_OFFSET-4)
_ddrclk:
	.word	0x0
	/* Record the size of the binary */
	.org	BISZ_OFFSET

	.word	BISZ_MAGIC
	.word	text_start
	.word	text_end
	.word	data_start
	.word	data_end
	.word	bss_start
	.word	bss_end
	.word	_end

	.balign envram_ofs
	/* Embedded NVRAM */
	.global	embedded_nvram
embedded_nvram:
	.fill	0x400,4,~0x48534C46	/* 'FLSH' */
        .long   0x4c5a4d41              /* LZMA NVRAM Supported */

#ifdef _CFE_
#if CFG_EMBEDDED_PIC
#define RUNTIME_RELOC_START	__runtime_reloc_start
#define RUNTIME_RELOC_STOP	__runtime_reloc_stop
#else
#define RUNTIME_RELOC_START	0
#define RUNTIME_RELOC_STOP	0
#endif

#include "segtable.h"

	.globl segment_table
segment_table:
	.word	text_end			/* [  0] End of text (R_SEG_ETEXT) */
	.word	data_start			/* [  1] Beginning of data (R_SEG_FDATA) */
	.word	data_end			/* [  2] End of data (R_SEG_EDATA) */
	.word	_end				/* [  3] End of BSS (R_SEG_END) */
	.word	text_start			/* [  4] Beginning of text (R_SEG_FTEXT) */
	.word	bss_start			/* [  5] Beginning of BSS (R_SEG_FBSS) */
	.word	0					/* [  6] Global Pointer (R_SEG_GP) */
	.word	RUNTIME_RELOC_START	/* [  7] Beginning of reloc entries */
	.word	RUNTIME_RELOC_STOP	/* [  8] End of reloc entries */
	.word	0					/* [  9] R_SEG_APIENTRY */
#endif /* __CFE_ */

/*************************************************************************
 *
 * Startup Code (reset vector)
 *
 * do important init only if we don't start from memory!
 * setup Memory and board specific bits prior to relocation.
 * relocate armboot to ram
 * setup stack
 *
 *************************************************************************/

/*
 * the actual reset code
 */
tr_rst:
	mrs	r0,cpsr
	mov	r1,#(PS_MM)
	bic	r0,r0,r1
	/* Switch to system mode and make sure interrupts are disabled */
	mov 	r1,#(PS_I | PS_F | PS_SYS)
	orr	r0,r0,r1
	msr	cpsr,r0
initcpu:
	mov	r0,#0

	/* Initialize cp15 registers */
	mcr	p15,0,r0,c5,c0,0		/* DFSR */
	mcr	p15,0,r0,c5,c0,1		/* IFSR */
	mcr	p15,0,r0,c6,c0,0		/* DFAR */
	mcr	p15,0,r0,c6,c0,1		/* WFSR */

	/* Initialize system control register */
	mrc	p15,0,r1,c1,c0,0
	bic	r1,r1,#0x7			/* Clear C, A, M bits */
	bic	r1,r1,#(0x1 << 12)		/* Clear I bit */
	orr	r1,r1,#0x2000			/* Set V bit: high vectors (0xffff0000) */
	orr	r1,r1,#0x2			/* Set A bit: enable strict alignment */
	mcr	p15,0,r1,c1,c0,0

	mcr	p15,0,r0,c8,c7,0		/* Invalidate TLB */
	mcr	p15,0,r0,c7,c5,0		/* Invalidate icache */
	mcr	p15,0,r0,c7,c5,6		/* Invalidate BTAC */
	mcr	p15,0,r0,c7,c10,4		/* DSB */
	mcr	p15,0,r0,c7,c5,4		/* ISB */

	/* Identify current CPU */
	mrc	p15,0,r0,c0,c0,5
	and	r0,r0,#0xf			/* retrieve cpu core id */
	cmp	r0,#0				/* cpu core 0? */
	beq	check_startaddr

	/* Changed the jump address for core 1 */
	ldr	r0,=0xffff002c			/* This address contains a wfe instruction */
	ldr	r1,=0xffff0000			/* SKU ROM starting address */
	str	r0, [r1, #0x400]

	ldr	pc,=0xffff002c			/* Go back to SKU ROM, waiting kernel SMP */

check_startaddr:
	adrl	r0,startup		/* r0: source start address */
	ldr	r1,=startup		/* r1: target address */
	cmp	r0,r1
	beq	szmem

	/* Wake up core 1 */
	mov	r0,#0
	mcr 	p15,0,r0,c7,c10,4
	sev	

enable_cache:
	/* Enable I cache */
	mrc	p15,0,r1,c1,c0,0
	orr	r1,r1,#0x1800			/* Enable I$ and branch prediction */
	mcr	p15,0,r1,c1,c0,0

#if defined(_CFEZ_)
genpll_mdiv:
	/* Correct the GENPLL setting for OTPed 4709C0 */
	ldr	r0, =0x18000000			/* SI_ENUM_BASE */
	ldr	r1, =0				/* CC_CHIPID */
	ldr	r1, [r0, r1]
	ldr	r0, =0x0000ffff			/* CID_ID_MASK */
	and	r2, r1, r0
	ldr	r0, =53030			/* BCM4709C0_CHIP_ID */
	cmp	r2, r0				/* r2 to store chip id */
	bne	genpll_mdiv_done
	ldr	r0, =0x00300000			/* CID_PACKAGE_OPTION_MASK */
	and	r2, r1, r0
	cmp	r2, #0				/* BCM4709C0 package is 0 */
	bne	genpll_mdiv_done

	/* To set correct CH0-5 mdiv */
	ldr	r0,=0x1800c154
	ldr	r0,[r0]
	ldr	r1,=0x3ff00000			/* Get CRU_GENPLL_CONTROL5 bit[29:20] */
	and	r0,r0,r1
	ldr	r1,=0x05000000			/* Check GENPLL NDIV = 0x50? */
	cmp	r0,r1
	bne	genpll_mdiv_done		/* Not equal. Skip this */
	/* If we have done it, skip it */
	ldr	r0,=0x1800c15c			/* CRU_GENPLL_CONTROL7 */
	ldr	r0,[r0]
	ldr	r1,=0x00ff0000
	and	r0,r0,r1			/* Get CH3_MDIV */
	ldr	r1,=0x00040000			/* If CH3_MDIV = 4, done */
	cmp	r0,r1
	beq	genpll_mdiv_done
	/* Configure mdiv for GENPLL channel 0~3 */
	/* Set CRU_CLKSET_KEY to unlock */
	ldr	r0,=0x1800c180
	ldr	r1,=0x0000ea68
	str	r1,[r0]
	/* Configure CRU_GENPLL_CONTROL6 */
	ldr	r0,=0x1800c158
	ldr	r1,[r0]
	ldr	r2,=0xff000000
	and	r1,r1,r2
	ldr	r2,=0x00080c32			/* CH0_MDIV = 8, CH1_MDIV = 12, CH2_MDIV = 50 */
	orr	r1,r1,r2
	str	r1,[r0]
	/* Configure CRU_GENPLL_CONTROL7 */
	ldr	r0,=0x1800c15c
	ldr	r1,[r0]
	ldr	r2,=0xff000000
	and	r1,r1,r2
	ldr	r2,=0x00040810			/* CH3_MDIV = 4, CH4_MDIV = 8, CH5_MDIV = 16 */
	orr	r1,r1,r2
	str	r1,[r0]
	/* Configure CRU_GENPLL_CONTROL5 */
	ldr	r0,=0x1800c154
	ldr	r1,[r0]
	ldr	r2,=0x80000000
	orr	r1,r1,r2			/* Enable SEL_SW_SETTING */
	str	r1,[r0]
	/* Configure CRU_GENPLL_CONTROL1 */
	ldr	r0,=0x1800c144
	ldr	r1,[r0]
	bic	r1,r1,#0xf			/* Reload mdiv for CH0~3 */
	str	r1,[r0]
	orr	r1,r1,#0xf
	str	r1,[r0]
	bic	r1,r1,#0xf
	str	r1,[r0]
	/* Set CRU_CLKSET_KEY to unlock */
	ldr	r0,=0x1800c180
	ldr	r1,=0x0
	str	r1,[r0]
genpll_mdiv_done:
#endif /* _CFEZ_ */

#ifdef CFG_SHMOO
	/* Invalidate data cache */
	bl	cpu_inv_dcache_all

	/* Enable D cache */
	mcr	p15,0,r0,c7,c10,4		/* DSB */
	mrc	p15,0,r1,c1,c0,0
	orr	r1,r1,#0x4
	mcr	p15,0,r1,c1,c0,0
	mcr	p15,0,r0,c7,c5,4		/* ISB */

	/* Invalidate TLB */
	mcr	p15,0,r0,c8,c7,0

scu_enable:
	ldr	r1,=IHOST_SCU_INVALIDATE
	mov	r2,#0xff
	str	r2,[r1]
	ldr	r1,=IHOST_SCU_CONTROL
	ldr	r2,[r1]
	orr	r2,r2,#0x1
	str	r2,[r1]
mmu_init:
	ldr	r1,=startup
	ldr	r2,=page_table
	sub	r2,r2,r1
	adrl	r1,startup
	add	r1,r1,r2

	/* Configure translation table base (TTB) control register cp15,c2
	 * to a value of all zeros, indicates we are using TTB register 0
	 */
	mov	r0,#0
	mcr	p15,0,r0,c2,c0,2
	/* Load page table address to cp15 */
	mcr	p15,0,r1,c2,c0,0
	/* Set the access control to client */
	ldr	r1,=0xffffffff
	mcr	p15,0,r1,c3,c0,0
	/* Enable MMU */
	mrc	p15,0,r1,c1,c0,0
	orr	r1,r1,#0x1
	mcr	p15,0,r1,c1,c0,0
	mcr	p15,0,r0,c7,c5,4		/* ISB */

l2cc_init:
	ldr	r1,=SI_ENUM_BASE
	ldr	r2,=CC_CHIPID
	ldr	r2,[r1,r2]
	ldr	r1,=CID_ID_MASK
	and	r2,r2,r1
	ldr	r1,=0xcf12	/* NS Ax/Bx */
	cmp	r2,r1
	beq	l2cc_256k
	ldr	r1,=0xcf26	/* NS 47094 */
	cmp	r2,r1
	beq	l2cc_256k
	ldr	r1,=0xcf19
	cmp	r2,r1
	bge	l2cc_512k
l2cc_256k:
	ldr	r1,=0x0a130000
	ldr	r4,=0x111
	b	l2cc_inv
l2cc_512k:
	ldr	r1,=0x0a150000
	ldr	r4,=0x131
l2cc_inv:
	ldr	r2,=IHOST_L2C_CACHE_ID
	ldr	r3,[r2,#0x104]
	bic	r3,r3,#0x000f0000
	orr	r3,r3,r1
	str	r3,[r2,#0x104]
	ldr	r1,=0x0
	str	r1,[r2,#0x108]
	str	r4,[r2,#0x10c]
	ldr	r1,=0xffff
	str	r1,[r2,#0x77c]
test_inv_way:
	ldr	r1,[r2,#0x77c]
	cmp	r1,r0
	bne	test_inv_way

	ldr	r1,=0x1ff
	str	r1,[r2,#0x220]			/* int_clear */
	mov	r1,#0x1
	str	r1,[r2,#0x100]			/* control */
	mcr	p15,0,r0,c7,c10,4		/* DSB */
	mcr	p15,0,r0,c7,c5,4		/* ISB */

l2cc_lockdown:
	ldr	r1,=0xc0000000
	ldr	r2,=0xc0020000			/* 128KB */
	ldr	r4,=0xa5a5a5a5
	ldr	r5,=0xa5a5a5a5
	ldr	r6,=0xa5a5a5a5
	ldr	r7,=0xa5a5a5a5
	ldr	r8,=0xa5a5a5a5
	ldr	r9,=0xa5a5a5a5
	ldr	r10,=0xa5a5a5a5
	ldr	r11,=0xa5a5a5a5
	/* Enable lockdown by line */
	mov	r3,#0x01
	ldr	r0,=IHOST_L2C_CACHE_ID
	str	r3,[r0,#0x950]
1:
	stmia	r1!,{r4-r11}
	ldr	r3,=0x10
cdelay1:
	sub	r3,r3,#1
	cmp	r3,#0
	bne	cdelay1
	cmp	r1,r2
	bne	1b

	/* Disable lockdown by line */
	mov	r3,#0
	ldr	r0,=IHOST_L2C_CACHE_ID
	str	r3,[r0,#0x950]		
l2cc_init_done:
#endif /* CFG_SHMOO */

	/* BCM4708 WAR for PCIE2 */
	ldr	r0,=0x1a000000
	ldr	r1,=0x0
	str	r1,[r0]
	ldr	r0,=0x18130e04
	ldr	r1,=0x0
	str	r1,[r0]
	ldr	r0,=0x18130e00
	ldr	r1,=0x1
	str	r1,[r0]
	
init_pcie:
	ldr	r0,=SI_ENUM_BASE		/* r0:	core regs SI base address */
	ldr	r1,=CC_EROMPTR
	ldr	r1,[r0,r1]			/* r1:	erom pointer */
1:	ldr	r2,[r1]				/* r2:	erom entry */
	ldr	r3,=(ER_TAG | ER_VALID)
	and	r3,r3,r2
	cmp	r3,#(ER_END | ER_VALID)		/* Is this the END marker? */
	beq	no_pcie				/* Yes: done and did not find pcie */

	cmp	r3,#(ER_CI | ER_VALID)
	beq	2f

	add	r1,r1,#4			/* Next erom entry */
	b	1b

no_pcie:
	b	find_bootdev
	
	/* Found a CIA */
2:	add	r1,r1,#8			/* Skip both CIA & CIB */
	ldr	r3,=CIA_CID_MASK
	and	r3,r3,r2
	ldr	r4,=(NS_PCIEG2_CORE_ID << CIA_CID_SHIFT)
	cmp	r3,r4
	bne	1b

	/* Found pcie, get regs and wrapper and save them */
	bl	ai_get_slave
	mov	r8,r0
	bl	ai_get_wrapper
	mov	r9,r0
	
	/* Initialize memory controller and size memory */
	mov	r0,r8
	mov	r1,r9
	bl	pcie_init
	bl	pcie_phy_init

find_bootdev:
	ldr	r0,=SI_ENUM_BASE		/* r0:	core regs SI base address */
	ldr	r1,=CC_EROMPTR
	ldr	r1,[r0,r1]			/* r1:	erom pointer */
1:	ldr	r2,[r1]				/* r2:	erom entry */
	ldr	r3,=(ER_TAG | ER_VALID)
	and	r3,r3,r2
	cmp	r3,#(ER_END | ER_VALID)		/* Is this the END marker? */
	beq	no_socrom_core			/* Yes: done and did not find rom core */

	cmp	r3,#(ER_CI | ER_VALID)
	beq	2f

	add	r1,r1,#4			/* Next erom entry */
	b	1b

no_socrom_core:
	b	init_ddr
	
	/* Found a CIA */
2:	add	r1,r1,#8			/* Skip both CIA & CIB */
	ldr	r3,=CIA_CID_MASK
	and	r3,r3,r2
	ldr	r4,=(NS_ROM_CORE_ID << CIA_CID_SHIFT)
	cmp	r3,r4
	bne	1b
	
	/* Found socrom, get regs and wrapper and save them */
	bl	ai_get_wrapper
	ldr	r1,[r0,#AI_IOSTATUS]
	ldr	r2,=SISF_NS_BOOTDEV_MASK
	and	r1,r1,r2
	mov	r7,r1
	ldr	r0,=SISF_NS_BOOTDEV_OFFLOAD
	cmp	r1,r0
	bne	init_ddr

offload_mode:
	ldr	r4,=0x1
	ldr	r5,=0xffff042c
	str	r4,[r5]
	ldr	r4,=0x0
	ldr	r5,=0xffff0430
wait_for_host:
	ldr	r6,[r5]
	cmp	r6,r4
	beq	wait_for_host
	bx	r6	

/*
 * Find ddr core
 */
init_ddr:
#ifdef CFG_SHMOO
	/* Copy to cache */
copy2cache:
	adrl	r0,startup		/* r0: source start address */
	ldr	r1,=startup		/* r1: target address */
	cmp	r0,r1
	beq	l2c_clear_bss
	ldr	r3,_bss_start_ofs
	add	r2,r0,r3		/* r2: source end address */
l2c_copy_loop:
	ldmia	r0!,{r9-r10}		/* copy from source address [r0] */
	stmia	r1!,{r9-r10}		/* copy to   target address [r1] */
	cmp	r0,r2			/* until source end address [r2] */
	blo	l2c_copy_loop

	/* Clear BSS */
l2c_clear_bss:
	ldr	r0,_bss_start_ofs
	ldr	r1,_bss_end_ofs
	ldr	r4,=startup
	add	r0,r0,r4
	add	r1,r1,r4
	mov	r2,#0x00000000
	/* empty bss section checking */
	cmp	r0,r1
	beq	enable_arm_cyclecount

l2c_clbss_l:
	str	r2,[r0]
	add	r0,r0,#4
	cmp	r0,r1
	bne	l2c_clbss_l

/* Cycle Count */
enable_arm_cyclecount:
	mrc	p15,0,r1,c9,c12,0
	ldr	r2,=1
	orr	r1,r1,r2
	mcr	p15,0,r1,c9,c12,0	/* Set enable bit in PMNC */
	ldr	r1,=0x80000000
	mcr	p15,0,r1,c9,c12,1

	/* setup stack pointer */
	ldr	r0,=0xc0000000
	ldr	r1,=0x20000			/* 128KB */
	add	r0,r0,r1
	sub	r0,r0,#8
	mov	sp,r0
	bic	sp,sp,#7
	ldr	r0,_c_ddr_init_ofs
	ldr	r1,=startup
	add	lr,r1,r0
	ldr	r0,_c_ddr_init_done_ofs
	adrl	r1,startup
	add	r0,r1,r0
	mov	pc,lr
c_ddr_init_done:
#else	/* CFG_SHMOO */
	ldr	r0,=SI_ENUM_BASE		/* r0:	core regs SI base address */
	ldr	r1,=CC_EROMPTR
	ldr	r1,[r0,r1]			/* r1:	erom pointer */
1:	ldr	r2,[r1]				/* r2:	erom entry */
	ldr	r3,=(ER_TAG | ER_VALID)
	and	r3,r3,r2
	cmp	r3,#(ER_END | ER_VALID)		/* Is this the END marker? */
	beq	panic				/* Yes: done and did not find ddr */

	cmp	r3,#(ER_CI | ER_VALID)
	beq	2f

	add	r1,r1,#4			/* Next erom entry */
	b	1b

panic:	b	.
	
	/* Found a CIA */
2:	add	r1,r1,#8			/* Skip both CIA & CIB */
	ldr	r3,=CIA_CID_MASK
	and	r3,r3,r2
	ldr	r4,=(NS_DDR23_CORE_ID << CIA_CID_SHIFT)
	cmp	r3,r4
	bne	1b

	/* Found ddr, get regs and wrapper and save them */
	bl	ai_get_slave
	mov	r8,r0
	bl	ai_get_wrapper
	mov	r9,r0
	
	/* Initialize memory controller and size memory */
	mov	r0,r8
	mov	r1,r9
	mov	r2,r7			/* boot_dev */

	bl	ddr_init
#endif  /* CFG_SHMOO */

szmem:
	ldr	r3,=0x80000000
	ldr	r2,=0xaa55beef
	ldr	r1,=0x00000000
	str	r1,[r3,#0x3c]
	ldr	r0,=(1 << 20)
	ldr	r5,=(1 << 30)
1:
	ldr	r4,=0x80000000
	add	r4,r4,r0
	str	r2,[r4,#0x3c]
	ldr	r1,[r4,#0x3c]			/* Read back to ensure completion */
	ldr	r1,[r3,#0x3c]
	cmp	r1,r2
	beq	2f

	lsl	r0,r0,#1
	cmp	r0,r5
	bne	1b

	/* Didn't find an alias, must be 128MB */
2:
	mov	r8,r0			/* r8: the memory size in bytes */

/* Do CRU configure after DDR initied */
#if !defined(CFG_ROMBOOT)
set_cru_policy7:
	ldr	r0,=IHOST_PROC_CLK_WR_ACCESS
	ldr	r1,=0xa5a501
	str	r1,[r0]
	
	ldr	r3,=SI_ENUM_BASE
	ldr	r4,=CC_CHIPID
	ldr	r4,[r3,r4]
	ldr	r3,=CID_PKG_MASK
	and	r4,r4,r3
	lsr	r3,r4,#CID_PKG_SHIFT

	cmp	r3,#0
	beq	cpu_1g
	ldr	r1,=0x1004001		/* 800MHz */
	b	set_clocks
	
cpu_1g:
	ldr	r1,=0x1005001		/* 1GHz */ 
	b	set_clocks

set_clocks:	
	ldr	r0,=IHOST_PROC_CLK_PLLARMA
	str	r1,[r0]
	ldr	r2,=0x0
	
	ldr	r4,=0x100000
poll_plllock:
	cmp	r4,r2
	beq	poll_plllock_done
	sub	r4,r4,#1
	ldr	r1,[r0]
	and	r3,r1,#(1 << IHOST_PROC_CLK_PLLARMA__PLLARM_LOCK)
	cmp	r3,r2
	beq	poll_plllock
poll_plllock_done:
	orr	r1,r1,#(1 << IHOST_PROC_CLK_PLLARMA__PLLARM_SOFT_POST_RESETB)
	str	r1,[r0]
	ldr	r0,=IHOST_PROC_CLK_POLICY_FREQ
	ldr	r1,=0x87070707
	str	r1,[r0]
	ldr	r0,=IHOST_PROC_CLK_CORE0_CLKGATE
	ldr	r1,=0x00010303
	str	r1,[r0]
	ldr	r0,=IHOST_PROC_CLK_CORE1_CLKGATE
	ldr	r2,=0x00000303
	str	r2,[r0]
	ldr	r0,=IHOST_PROC_CLK_ARM_SWITCH_CLKGATE
	str	r1,[r0]
	ldr	r0,=IHOST_PROC_CLK_ARM_PERIPH_CLKGATE
	str	r1,[r0]
	ldr	r0,=IHOST_PROC_CLK_APB0_CLKGATE
	str	r1,[r0]

	ldr	r0,=IHOST_PROC_CLK_POLICY_CTL
	ldr	r2,=(1 << IHOST_PROC_CLK_POLICY_CTL__GO)
	ldr	r3,=(1 << IHOST_PROC_CLK_POLICY_CTL__GO_AC)
	orr	r3,r3,r2
	str	r3,[r0]
	ldr	r3,=0x0
	ldr	r4,=0x100000
poll_ccu:
	cmp	r4,r3
	beq	poll_ccu_done
	sub	r4,r4,#1
	ldr	r1,[r0]
	and	r1,r1,r2
	cmp	r1,r3
	bne	poll_ccu
poll_ccu_done:
#endif /* !defined(CFG_ROMBOOT) */

#ifndef CFG_SHMOO
        /* Copy to RAM */
copy2ram:
	adrl	r0,startup		/* r0: source start address */
	ldr	r1,=startup		/* r1: target address */
	cmp	r0,r1
	beq	clear_bss
	ldr	r3,_bss_start_ofs
	add	r2,r0,r3		/* r2: source end address */
copy_loop:
	ldmia	r0!,{r9-r10}		/* copy from source address [r0] */
	stmia	r1!,{r9-r10}		/* copy to   target address [r1] */
	cmp	r0,r2			/* until source end address [r2] */
	blo	copy_loop

	/* Clear BSS */
clear_bss:
	ldr	r0,_bss_start_ofs
	ldr	r1,_bss_end_ofs
	ldr	r4,=startup
	add	r0,r0,r4
	add	r1,r1,r4
	mov	r2,#0x00000000
	/* empty bss section checking */
	cmp	r0,r1
	beq	enable_arm_cyclecount

clbss_l:
	str	r2,[r0]
	add	r0,r0,#4
	cmp	r0,r1
	bne	clbss_l

/* Cycle Count */
enable_arm_cyclecount:
	mrc	p15,0,r1,c9,c12,0
	ldr	r2,=1
	orr	r1,r1,r2
	mcr	p15,0,r1,c9,c12,0	/* Set enable bit in PMNC */
	ldr	r1,=0x80000000
	mcr	p15,0,r1,c9,c12,1
#endif /* !CFG_SHMOO */
/* Set stackpointer in internal RAM to call c_main */
call_c_main:
	/* Save memory size in bytes */
	mov	r0,r8
	cmp	r0,#(128<<20)
	ble	1f
	mov	r0,#(128<<20)
1:
	ldr	r1,=_memsize
	str r0,[r1]

#ifdef _CFE_
	_LSR_ r8,r8,#10			/* memory size in Kbytes */
	ldr	r1,=mem_totalsize
	str	r8,[r1]

	/* not relocating, no offset */
	ldr	r0,=0
	ldr	r1,=mem_datareloc
	str	r0,[r1]
	ldr	r1,=mem_textreloc
	str	r0,[r1]

	ldr r0,_text_start_ofs
	add	r0,r0,r4
	ldr	r1,=mem_bottomofmem
	str	r0,[r1]

	ldr	r0,__end_ofs
	add	r0,r0,r4
	ldr	r1,=mem_heapstart
	str	r0,[r1]

	add	r0,#(CFG_HEAP_SIZE*1024)
	add	r0,#STACK_SIZE
	ldr	r1,=mem_topofmem
	str	r0,[r1]

	ldr	r0,_text_start_ofs
	add	r0,r0,r4
	ldr	r2,_text_end_ofs
	add	r2,r2,r4
	sub	r2,r2,r0
	ldr	r1,=mem_textsize
	str	r2,[r1]
	ldr	r1,=mem_textbase
	str	r0,[r1]

	ldr	r0,=0x00000000
	ldr	r1,=cpu_prid
	str	r0,[r1]

	/* Set the segment_table */
	/* text_end */
	ldr	r0,_text_end_ofs
	add	r0,r0,r4
	ldr	r1,=segment_table
	str	r0,[r1]

	/* data_start */
	ldr	r0,_data_start_ofs
	add	r0,r0,r4
	add	r1,r1,#4
	str	r0,[r1]

	/* data_end */
	ldr	r0,_data_end_ofs
	add	r0,r0,r4
	add	r1,r1,#4
	str	r0,[r1]

	/* _end */
	ldr	r0,__end_ofs
	add	r0,r0,r4
	add	r1,r1,#4
	str	r0,[r1]

	/* text_start */
	ldr	r0,_text_start_ofs
	add	r0,r0,r4
	add	r1,r1,#4
	str	r0,[r1]

	/* bss_start */
	ldr	r0,_bss_start_ofs
	add	r0,r0,r4
	add	r1,r1,#4
	str	r0,[r1]

	/* _gp (0) */
	mov	r0,#0
	add	r1,r1,#4
	str	r0,[r1]

	/* RUNTIME_RELOC_START (0) */
	mov	r0,#0
	add	r1,r1,#4
	str	r0,[r1]

	/* RUNTIME_RELOC_STOP (0) */
	mov	r0,#0
	add	r1,r1,#4
	str	r0,[r1]

	/* cpu_apientry (0) */
	mov	r0,#0
	add	r1,r1,#4
	str	r0,[r1]

	ldr	r1,=mem_topofmem
#else /* !_CFE_ */
	ldr	r1,=_memsize
#endif /* _CFE_ */

	ldr	r0,[r1]
	sub	r0,r0,#8
	mov	sp,r0
	bic	sp,sp,#7		/* 8-byte alignment for ABI compliance */

	ldr	r0,_c_main_ofs
	ldr	r1,=startup
	add	lr, r0, r1
	/* setup parameters for c_main */
#ifdef _CFE_
	/* CFE: clear argument a & b in cfe_main */
	ldr	r0,=0x00000000
	ldr	r1,=0x00000000
#else
	/* CFEZ: set return addr to argment ra */
	ldr	r0,_theend_ofs
	add	r0,r0,r2
#endif
	/* jump to it ... */
	mov	pc,lr

/* In case c_main returns */
theend:
	TRACE(0x424feedd)
	wfe
	b	theend


/*
 * These are defined in the board-specific linker script.
 */

#ifdef _CFE_
.globl _text_start_ofs
_text_start_ofs:
	.word text_start - startup

.globl _text_end_ofs
_text_end_ofs:
	.word text_end - startup

.globl _data_start_ofs
_data_start_ofs:
	.word data_start - startup

.globl _data_end_ofs
_data_end_ofs:
	.word data_end - startup

.globl __end_ofs
__end_ofs:
	.word _end - startup
#endif /* _CFE_ */

.globl _bss_start_ofs
_bss_start_ofs:
	.word bss_start - startup

.globl _bss_end_ofs
_bss_end_ofs:
	.word bss_end - startup

#ifdef CFG_SHMOO
_c_ddr_init_ofs:
	.word c_ddr_init - startup

_c_ddr_init_done_ofs:
	.word c_ddr_init_done - startup
#endif /* CFG_SHMOO */

_c_main_ofs:
	.word c_main - startup

_theend_ofs:
	.word theend - startup

/* Init PCIE mappings */
FUNC(pcie_init)
	ldr	r5,=REG_BAR2_CONFIG_F0
	str	r5,[r0,#PAXB_INDR_ADDR]
	ldr	r3,[r0,#PAXB_INDR_DATA]
	and	r3,r3,#0xfffffff0
	orr	r3,r3,#0xc
	str	r3,[r0,#PAXB_INDR_DATA]

	ldr	r3,[r0,#PAXB_IARR_1_LOWER]
	and	r3,r3,#0xffffff00
	orr	r3,r3,#0x80
	str	r3,[r0,#PAXB_IARR_1_LOWER]
	ldr	r3,=0x40000000
	str	r3,[r0,#PAXB_IARR_1_UPPER]
	
	ldr	r3,=0x00000001
	str	r3,[r0,#PAXB_FUNC0_IMAP1]
	bx	lr
END(pcie_init)

/*
 * Find the first slave address for a core in the AI EROM
 * Input:
 *	r1 - pointer to EROM after CIB
 * Output:
 *	r0 - slave address
 * Changed:
 *	r0, r1, r2
 */
FUNC(ai_get_slave)
1:	ldr	r0,[r1]
	ldr	r2,=(ER_TAG | ER_VALID)
	add	r1,r1,#4			/* Next erom entry */
	and	r2,r2,r0
	cmp	r2,#(ER_ADD | ER_VALID)
	bne	1b

2:	ldr	r2,=AD_ADDR_MASK
	and	r0,r0,r2
	bx	lr
END(ai_get_slave)


/*
 * Find the first wrapper address for a core in the AI EROM
 * Input:
 *	r1 - pointer to EROM after first slave ADD
 * Output:
 *	r0 - wrapper address
 * Changed:
 *	r0, r1, r2, r3
 */
FUNC(ai_get_wrapper)
1:	ldr	r0,[r1]
	ldr	r2,=(ER_TAG | ER_VALID)
	add	r1,r1,#4			/* Next erom entry */
	and	r2,r2,r0
	cmp	r2,#(ER_ADD | ER_VALID)
	bne	1b

	/* An address descriptor, is it a wrapper? */
	ldr	r2,=AD_ST_SWRAP			/* We test just for the SWRAP bit set, */
	tst	r0,r2				/*  that gets both Master and Slave */
	beq	1b				/*  wrappers. */

	/* It is, isolate address and return */
	ldr	r2,=AD_ADDR_MASK
	and	r0,r0,r2
	bx	lr
END(ai_get_wrapper)


/* Interrupt handlers */
/* Note: hndrte_arm.c for EXT_CBALL expects each trap handler to be 16 bytes */
LEAF(__traps)

/* Undefined instruction exception handler */
tr_und:
	srsdb	sp!, #PS_SYS	/* use system mode stack */
	cps	#PS_SYS		

	/* now cr4 is in system mode */
	/* lr has the value before the exception, push it to stack */
	push 	{r0}	/* hold the place for r15 */
	push	{lr}
	sub	sp,sp,#24	/* skip r8-r13 now */
	push 	{r0-r7}	/* save r0-r7 to the stack */
	eor	r0,r0,r0
	add	r0,r0,#TR_UND
	b	trap

/* Software interrupt exception handler */
tr_swi:
	srsdb	sp!, #PS_SYS	/* use system mode stack */
	cps	#PS_SYS

	push 	{r0}	/* hold the place for r15 */
	push	{lr}
	sub	sp,sp,#24	/* skip r8-r13 now */
	push 	{r0-r7}
	eor	r0,r0,r0
	add	r0,r0,#TR_SWI
	b	trap

/* Prefetch abort exception handler */
tr_iab:
	/* adjust lr to the address of abort insturction fetch */
	mov 	sp,lr
	sub	sp,sp,#4
	mov 	lr,sp
	srsdb	sp!, #PS_SYS	/* use system mode stack */
	cps	#PS_SYS

	push 	{r0}	/* hold the place for r15 */
	push	{lr}
	sub	sp,sp,#24	/* skip r8-r13 now */
	push 	{r0-r7}
	eor	r0,r0,r0
	add	r0,r0,#TR_IAB
	b	trap

/* Data abort exception handler */
tr_dab:
	/* adjust lr to the address of aborted data fetch */
	mov 	sp,lr
	sub	sp,sp,#8
	mov 	lr,sp
	srsdb	sp!, #PS_SYS	/* use system mode stack */
	cps	#PS_SYS	

	push 	{r0}	/* hold the place for r15 */
	push	{lr}
	sub	sp,sp,#24	/* skip r8-r13 now */
	push 	{r0-r7}
	eor	r0,r0,r0
	add	r0,r0,#TR_DAB
	b	trap

tr_bad: /* reserved */
	mov 	sp,lr
	sub	sp,sp,#4
	mov 	lr,sp
	srsdb	sp!, #PS_SYS	/* use system mode stack */
	cps	#PS_SYS

	push 	{r0}	/* hold the place for r15 */
	push	{lr}
	sub	sp,sp,#24	/* skip r8-r13 now */
	push 	{r0-r7}
	eor	r0,r0,r0
	add	r0,r0,#TR_BAD
	b	trap

/* Interrupt handler */
tr_irq:
	/* subtract the offset */
	mov 	sp,lr
	sub	sp,sp,#4
	mov 	lr,sp
	srsdb	sp!, #PS_SYS	/* use system mode stack */
	cps	#PS_SYS

	push 	{r0}	/* hold the place for r15 */
	push	{lr}
	sub	sp,sp,#24	/* skip r8-r13 now */
	push 	{r0-r7}
	eor	r0,r0,r0
	add	r0,r0,#TR_IRQ
	b	trap


/* Fast interrupt handler */
tr_fiq:
	mov 	sp,lr
	sub	sp,sp,#4
	mov 	lr,sp
	srsdb	sp!, #PS_SYS	/* use system mode stack */
	cps	#PS_SYS

	push 	{r0}	/* hold the place for r15 */
	push	{lr}
	sub	sp,sp,#24	/* skip r8-r13 now */
	push 	{r0-r7}
	eor	r0,r0,r0
	add	r0,r0,#TR_FIQ
	b	trap

/* Generic exception handler */
trap:
	/*
	 * construt the trap structure in system mode stack 
	*/
	/* save trap type, epc, cpsr and spsr */
	mov 	r4, sp	/* sp is now at r0 */
	add	r4,r4,#64
	ldmia	r4!, {r1,r3} /* r1<-epc, r3<-spsr */
	mrs	r2, cpsr
	push	{r0,r1,r2,r3}	/* trap type(r0), epc(r1), cpsr,spsr */

	/* fix the value of pc in trap structure */
	sub	r4,r4,#12
	str	r1,[r4]	/* save the value or epc as r15 into the stack */ 
	
	/* save r8 - r13 */
	mov 	r7,sp	/* sp is at the being of the trap structure now */
	add	r7,r7,#TRAP_T_SIZE + 8	/* sp value before the exception */
	mov 	r6,r12
	mov	r5,r11
	mov	r4,r10
	mov	r3,r9
	mov	r2,r8
	add	sp,sp,#72 /* move sp to r14 */
	push	{r2-r7}	/* save r8-r13 to stack */		

	sub	sp,sp,#48 /* move sp back to the top of trap structure */	

	/* If trap_hook is null, go to the end */
	ldr	r4,=trap_hook
	ldr	r4,[r4]
	cmp	r4,#0
1:	beq	1b
	/* else call (*trap_hook)(trap_t) */
	mov	r0,sp
	blx	r4

/* Restore the state from the trap_t */
rfe:	
	/* pop r8-r14 */
	add	sp,sp,#48
	pop	{r0-r6} /* r8 - r14 */
	mov	r8,r0
	mov	r9,r1
	mov	r10,r2
	mov	r11,r3
	mov	r12,r4
	mov	lr,r6	/* restore lr */

	sub	sp,sp,#60 /* move sp to point to r0 */
	pop	{r0-r7}

	/* move sp to point to epc and spsr saved by srs */
	add	sp,sp,#32

	/* Restore cpsr, sp and return */
	rfeia	sp!	/* done! */
END(__traps)

FUNC(_getticks)
	mrc	p15,0,r0,c9,c13,0
	bx	lr
END(_getticks)

/* Flush the whole D-cache */
cpu_flush_dcache_all:
	mrc	p15, 1, r0, c0, c0, 1		@ read clidr
	ands	r3, r0, #0x7000000		@ extract loc from clidr
	mov	r3, r3, lsr #23			@ left align loc bit field
	beq	finished			@ if loc is 0, then no need to clean
	mov	r10, #0				@ start clean at cache level 0
loop1:
	add	r2, r10, r10, lsr #1		@ work out 3x current cache level
	mov	r1, r0, lsr r2			@ extract cache type bits from clidr
	and	r1, r1, #7			@ mask of the bits for current cache only
	cmp	r1, #2				@ see what cache we have at this level
	blt	skip				@ skip if no cache, or just i-cache
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	mcr	p15, 0, r10, c7, c5, 4		@ flush prefetch buffer,
						@ with armv7 this is 'isb',
						@ but we compile with armv5
	mrc	p15, 1, r1, c0, c0, 0		@ read the new csidr
	and	r2, r1, #7			@ extract the length of the cache lines
	add	r2, r2, #4			@ add 4 (line length offset)
	ldr	r4, =0x3ff
	ands	r4, r4, r1, lsr #3		@ find maximum number on the way size
	clz	r5, r4				@ find bit position of way size increment
	ldr	r7, =0x7fff
	ands	r7, r7, r1, lsr #13		@ extract max number of the index size
loop2:
	mov	r9, r4				@ create working copy of max way size
loop3:
	orr	r11, r10, r9, lsl r5		@ factor way and cache number into r11
	orr	r11, r11, r7, lsl r2		@ factor index number into r11
	mcr	p15, 0, r11, c7, c14, 2		@ clean & invalidate by set/way
	subs	r9, r9, #1			@ decrement the way
	bge	loop3
	subs	r7, r7, #1			@ decrement the index
	bge	loop2
skip:
	add	r10, r10, #2			@ increment cache number
	cmp	r3, r10
	bgt	loop1
finished:
	mov	r10, #0				@ swith back to cache level 0
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	mcr	p15, 0, r10, c7, c5, 4		@ flush prefetch buffer,
						@ with armv7 this is 'isb',
						@ but we compile with armv5
	mov	pc, lr

FUNC(cpu_flush_cache_all)
	stmfd	sp!, {r0-r7, r9-r11, lr}
	bl	cpu_flush_dcache_all
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0		@ I+BTB cache invalidate
	ldmfd	sp!, {r0-r7, r9-r11, lr}
	mov	pc, lr
END(cpu_flush_cache_all)

/* Invalidate the whole D-cache */
cpu_inv_dcache_all:
	mrc	p15, 1, r0, c0, c0, 1		@ read clidr
	ands	r3, r0, #0x7000000		@ extract loc from clidr
	mov	r3, r3, lsr #23			@ left align loc bit field
	beq	invfinished			@ if loc is 0, then no need to clean
	mov	r10, #0				@ start clean at cache level 0
invloop1:
	add	r2, r10, r10, lsr #1		@ work out 3x current cache level
	mov	r1, r0, lsr r2			@ extract cache type bits from clidr
	and	r1, r1, #7			@ mask of the bits for current cache only
	cmp	r1, #2				@ see what cache we have at this level
	blt	invskip				@ skip if no cache, or just i-cache
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	mcr	p15, 0, r10, c7, c5, 4		@ flush prefetch buffer,
						@ with armv7 this is 'isb',
						@ but we compile with armv5
	mrc	p15, 1, r1, c0, c0, 0		@ read the new csidr
	and	r2, r1, #7			@ extract the length of the cache lines
	add	r2, r2, #4			@ add 4 (line length offset)
	ldr	r4, =0x3ff
	ands	r4, r4, r1, lsr #3		@ find maximum number on the way size
	clz	r5, r4				@ find bit position of way size increment
	ldr	r7, =0x7fff
	ands	r7, r7, r1, lsr #13		@ extract max number of the index size
invloop2:
	mov	r9, r4				@ create working copy of max way size
invloop3:
	orr	r11, r10, r9, lsl r5		@ factor way and cache number into r11
	orr	r11, r11, r7, lsl r2		@ factor index number into r11
	mcr	p15, 0, r11, c7, c6, 2		@ invalidate by set/way
	subs	r9, r9, #1			@ decrement the way
	bge	invloop3
	subs	r7, r7, #1			@ decrement the index
	bge	invloop2
invskip:
	add	r10, r10, #2			@ increment cache number
	cmp	r3, r10
	bgt	invloop1
invfinished:
	mov	r10, #0				@ swith back to cache level 0
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	mcr	p15, 0, r10, c7, c5, 4		@ flush prefetch buffer,
						@ with armv7 this is 'isb',
						@ but we compile with armv5
	mov	pc, lr

FUNC(cpu_inv_cache_all)
	stmfd	sp!, {r0-r7, r9-r11, lr}
	bl	cpu_inv_dcache_all
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0		@ I+BTB cache invalidate
	ldmfd	sp!, {r0-r7, r9-r11, lr}
	mov	pc, lr
END(cpu_inv_cache_all)

/* Dummy function just for link happy */
FUNC(raise)

	mov	r0, #0
	mov pc, lr

END(raise)

FUNC(__aeabi_unwind_cpp_pr0)

	mov pc, lr

END(__aeabi_unwind_cpp_pr0)

/*
 * Common code/data start here...
 */
#ifdef CONFIG_XIP
	.data
#endif

	DW(_memsize, 0)

/* C trap handler */
	DW(trap_hook, 0)

#ifdef	CONFIG_XIP
	.text
#endif

#ifdef _CFE_

/*
 * Setup the trap handler.
 */
FUNC(hndrte_set_trap)

	ldr	r2,=trap_hook
	ldr	r1,[r2]
	str	r0,[r2]
	mov	r0,r1
	bx	lr

END(hndrte_set_trap)

/*
 * Turn remap off and then jump to an given address
 */
FUNC(arm_jumpto)

	bx	r0

END(arm_jumpto)

/*  *********************************************************************
    *  CFE_FLUSHCACHE
    *
    *  Perform certain cache operations
    *
    *  Input parameters:
    *  	   a0 - flags (CFE_CACHE_xxx flags, or zero for a default)
    *      a1,a2 - start/end of range for "range invalidate" operations
    *      (not used otherwise)
    *
    *  Return value:
    *  	   nothing
    ********************************************************************* */

FUNC(_cfe_flushcache)

#ifndef CFG_UNCACHED
	mov	ip,lr			/* save lr across calls */
	bl	cpu_flush_cache_all
	mov	lr,ip			/* restore lr */
#endif

	mov pc, lr
	

END(_cfe_flushcache)

#
# arguments: r0=start, r1=size
#
FUNC(_cfe_flushcache_rang)

	mrc	p15, 1, r3, c0, c0, 0	@ read CSIDR
	and	r3, r3, #7				@ cache line size encoding
	mov	r2, #16					@ size offset
	mov	r2, r2, lsl r3			@ actual cache line size

	sub	r3, r2, #1
	bic	r0, r0, r3
1:
	mcr	p15, 0, r0, c7, c10, 1		@ clean D / U line
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
#	dsb
	mcr	p15, 0, r10, c7, c5, 4		@ flush prefetch buffer,
						@ with armv7 this is 'isb',
						@ but we compile with armv5
	mov	pc, lr

END(_cfe_flushcache_rang)

/*  *********************************************************************
    *  CFE_LAUNCH
    *
    *  Start the user program.  The program is passed a handle
    *  that must be passed back when calling the firmware.
    *
    *  Parameters passed to the called program are as follows:
    *
    *      a0 - CFE handle
    *      a1 - entry vector
    *      a2 - reserved, will be 0
    *      a3 - entrypoint signature.
    *
    *  Input parameters:
    *  	   a0 - entry vector
    *
    *  Return value:
    *  	   does not return
    ********************************************************************* */

FUNC(cfe_launch)

	sub  sp,sp,#8
	push {r0}

	/*
	 * Mask all interrupts.
	 */

	mrs	r0,cpsr
	ldr	r1,=(PS_I | PS_F)
	orr	r0,r0,r1
	msr	cpsr,r0

	/*
	 * Flush the D-Cache, since the program we loaded is "data".
	 * Invalidate the I-Cache, so that addresses in the program
	 * region will miss and need to be filled from the data we
	 * just flushed above.
	 */

	bl _cfe_flushcache

	/*
	 * Disable the D-Cache, MMU and I-Cache bit
	 */

	mrc	p15,0,r0,c1,c0,0	@ read SCTLR
	bic	r0,r0,#4			@ disable D-Cache
	bic	r0,r0,#1			@ disable MMU
	bic	r0,r0,#1000			@ disable I-Cache
	mcr	p15,0,r0,c1,c0,0

	/*
	 * Set things up for launching the program.  Pass the
	 * handle in A0 - apps need to remember that and pass it
	 * back.
	 */
	b RunProgram

END(cfe_launch)

/*
 * This is a nice place to set a breakpoint.
 */
FUNC(RunProgram)

	LOADREL(r2,=segment_table)
	ldr r2,[r2,#R_SEG_APIENTRY]	/* A2 = code entry */

#if CFG_EMBEDDED_PIC
	ldr r1,mem_textreloc	/* relocate table entry */
	add r2,r2,r1
#endif

	mov r4,r0
	mov r1,#0		/* A1 = 0 */
	mov r0,#0		/* A0 = handle */
	ldr r3,=CFE_EPTSEAL  /* A3 = entrypoint signature */
	pop {r4}		/* entry point */

	/* Check if the MERGE_FRAG function should execute or not,
	 * according to the information within structure cfe_frag_info_t.
	 * _frag_info_sz is the size of structure cfe_frag_info_t.
	 * Structure cfe_frag_info_t is stored right before MERGE_FRAG function, and
	 * MERGE_FRAG function is stored right before entry point.
	 * Note: {r0-r4} must be preserved
	 */
	ldr r7, =_frag_merge_func_len
	ldr r6, [r7]
	ldr r8, =_frag_info_sz
	ldr r7, [r8]
	add r6, r6, r7
	sub r5, r4, r6
	ldr r6, [r5]
	cmp r6, #0
	beq do_run
	mov pc, r6	/* Go to _merge_func */
do_run:
	mov pc, r4		/* go for it. */
END(RunProgram)


	.globl _frag_merge_func
_frag_merge_func:
	.word _merge_func

	.globl _frag_merge_func_len
_frag_merge_func_len:
	.word _merge_func_end - _merge_func

       .globl _frag_info_sz
_frag_info_sz:
	.word   0

/* This merge_func will relocate at address which is stored at MERGE_FRAG.
 * It will reference the value which is stored at MERGE_FRAG.
 * There are the merge_func pointer, des_addr, src_addr and length.
 * r4 is the entry point, r0-r3 are the parameters for the image.
 * r5 is the address of MERGE_FRAG
 */
FUNC(_merge_func)
	ldmia	r5, {r9-r12}
1:
	ldmia	r11!, {r7-r8}			@ copy 8 bytes from the memory at r11 address
	stmia	r10!, {r7-r8}			@ store 8 bytes to the memory at r10 address
						@ r12 is the length, each loop copy 8 bytes
	subs	r12, r12, #8			@ adjust the length
	bgt	1b

	mov	pc, r4				@ jump to the entry point
_merge_func_end:
END(_merge_func)

/*  *********************************************************************
    *  BOARD_SETLEDS(x)
    *
    *  Set LEDs for boot-time progress indication.  Not used if
    *  the board does not have progress LEDs.  This routine
    *  must not call any other routines, since it may be invoked
    *  either from KSEG0 or KSEG1 and it may be invoked
    *  whether or not the icache is operational.
    *
    *  Input parameters:
    *  	   a0 - LED value (8 bits per character, 4 characters)
    *
    *  Return value:
    *  	   nothing
    ********************************************************************* */
FUNC(board_setleds)

	mov	pc, lr			@ back to my caller

END(board_setleds)

/*  *********************************************************************
    *  CFE_LEDS
    *
    *  Set the on-board LEDs.
    *
    *  Input parameters:
    *  	   a0 - LEDs
    *
    *  Return value:
    *  	   nothing
    ********************************************************************* */
FUNC(cfe_leds)

	/* jump to BSP routine */
	//bl	board_setleds

	/* return */
	mov	pc, lr

END(cfe_leds)

/*  *********************************************************************
    *  _GETSEGTBL()
    *
    *  Return the address of the segment table.  We use this
    *  to display the startup messages.
    *
    *  You can't just address the table from C because it lives
    *  in the text segment.
    *
    *  Input parameters:
    *  	   nothing
    *
    *  Return value:
    *      address of table
    ********************************************************************* */

FUNC(_getsegtbl)

	mov	ip, lr		/* persevere link reg across call */
	LOADREL(r0,=segment_table)
	mov	lr, ip		/* restore link */
	mov pc, lr		/* back to my caller */

END(_getsegtbl)

/*  *********************************************************************
    *  _SETCONTEXT()
    *
    *  Set the CONTEXT register.
    *
    *  Input parameters:
    *  	   a0 - context
    *
    *  Return value:
    *  	   nothing
    ********************************************************************* */

FUNC(_setcontext)

	mov	pc,lr

END(_setcontext)

/*  *********************************************************************
    *  CFE_WARMSTART
    *
    *  Restart the command interpreter
    *
    *  Input parameters:
    *      A0 - command status
    *  	   nothing (GP has already been set up for us)
    *
    *  Return value:
    *  	   nothing
    ********************************************************************* */

FUNC(cfe_warmstart)

	mov	pc,lr

END(cfe_warmstart)

#endif /* _CFE_ */
