#ifdef BCMWAPI_WPI
/*
 * sms4.c
 * SMS-4 block cipher
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: sms4.c,v 1.23 2009-10-22 00:10:59 Exp $
 */

#include <typedefs.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <bcmcrypto/sms4.h>

#include <bcmutils.h>

#ifdef BCMDRIVER
#include <osl.h>
#else
#include <stddef.h>	/* for size_t */
#if defined(__GNUC__)
extern void bcopy(const void *src, void *dst, size_t len);
extern int bcmp(const void *b1, const void *b2, size_t len);
extern void bzero(void *b, size_t len);
#else
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), 0, (len))
#endif	/* __GNUC__ */
#endif	/* BCMDRIVER */

#define ROTATE(x, n)     (((x) << (n)) | ((x) >> (32 - (n))))

static void
sms4_print_bytes(char *name, const uchar *data, int len)
{
}

#if !defined(__i386__)
static const uint8 S_box[] = {
	0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7,
	0x16, 0xb6, 0x14, 0xc2, 0x28, 0xfb, 0x2c, 0x05,
	0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3,
	0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99,
	0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef, 0x98, 0x7a,
	0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62,
	0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95,
	0x80, 0xdf, 0x94, 0xfa, 0x75, 0x8f, 0x3f, 0xa6,
	0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba,
	0x83, 0x59, 0x3c, 0x19, 0xe6, 0x85, 0x4f, 0xa8,
	0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b,
	0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d, 0x35,
	0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2,
	0x25, 0x22, 0x7c, 0x3b, 0x01, 0x21, 0x78, 0x87,
	0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52,
	0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e,
	0xea, 0xbf, 0x8a, 0xd2, 0x40, 0xc7, 0x38, 0xb5,
	0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1,
	0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55,
	0xad, 0x93, 0x32, 0x30, 0xf5, 0x8c, 0xb1, 0xe3,
	0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60,
	0xc0, 0x29, 0x23, 0xab, 0x0d, 0x53, 0x4e, 0x6f,
	0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f,
	0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c, 0x5b, 0x51,
	0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f,
	0x11, 0xd9, 0x5c, 0x41, 0x1f, 0x10, 0x5a, 0xd8,
	0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd,
	0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0,
	0x89, 0x69, 0x97, 0x4a, 0x0c, 0x96, 0x77, 0x7e,
	0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84,
	0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20,
	0x79, 0xee, 0x5f, 0x3e, 0xd7, 0xcb, 0x39, 0x48
};
#else /* __i386__ */
/* slightly better performance on Pentium, worse performance on ARM CM3 */
static const uint32 S_box0[] = {
	0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7,
	0x16, 0xb6, 0x14, 0xc2, 0x28, 0xfb, 0x2c, 0x05,
	0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3,
	0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99,
	0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef, 0x98, 0x7a,
	0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62,
	0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95,
	0x80, 0xdf, 0x94, 0xfa, 0x75, 0x8f, 0x3f, 0xa6,
	0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba,
	0x83, 0x59, 0x3c, 0x19, 0xe6, 0x85, 0x4f, 0xa8,
	0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b,
	0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d, 0x35,
	0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2,
	0x25, 0x22, 0x7c, 0x3b, 0x01, 0x21, 0x78, 0x87,
	0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52,
	0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e,
	0xea, 0xbf, 0x8a, 0xd2, 0x40, 0xc7, 0x38, 0xb5,
	0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1,
	0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55,
	0xad, 0x93, 0x32, 0x30, 0xf5, 0x8c, 0xb1, 0xe3,
	0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60,
	0xc0, 0x29, 0x23, 0xab, 0x0d, 0x53, 0x4e, 0x6f,
	0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f,
	0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c, 0x5b, 0x51,
	0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f,
	0x11, 0xd9, 0x5c, 0x41, 0x1f, 0x10, 0x5a, 0xd8,
	0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd,
	0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0,
	0x89, 0x69, 0x97, 0x4a, 0x0c, 0x96, 0x77, 0x7e,
	0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84,
	0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20,
	0x79, 0xee, 0x5f, 0x3e, 0xd7, 0xcb, 0x39, 0x48
};

static const uint32 S_box1[] = {
	0xd600, 0x9000, 0xe900, 0xfe00, 0xcc00, 0xe100, 0x3d00, 0xb700,
	0x1600, 0xb600, 0x1400, 0xc200, 0x2800, 0xfb00, 0x2c00, 0x0500,
	0x2b00, 0x6700, 0x9a00, 0x7600, 0x2a00, 0xbe00, 0x0400, 0xc300,
	0xaa00, 0x4400, 0x1300, 0x2600, 0x4900, 0x8600, 0x0600, 0x9900,
	0x9c00, 0x4200, 0x5000, 0xf400, 0x9100, 0xef00, 0x9800, 0x7a00,
	0x3300, 0x5400, 0x0b00, 0x4300, 0xed00, 0xcf00, 0xac00, 0x6200,
	0xe400, 0xb300, 0x1c00, 0xa900, 0xc900, 0x0800, 0xe800, 0x9500,
	0x8000, 0xdf00, 0x9400, 0xfa00, 0x7500, 0x8f00, 0x3f00, 0xa600,
	0x4700, 0x0700, 0xa700, 0xfc00, 0xf300, 0x7300, 0x1700, 0xba00,
	0x8300, 0x5900, 0x3c00, 0x1900, 0xe600, 0x8500, 0x4f00, 0xa800,
	0x6800, 0x6b00, 0x8100, 0xb200, 0x7100, 0x6400, 0xda00, 0x8b00,
	0xf800, 0xeb00, 0x0f00, 0x4b00, 0x7000, 0x5600, 0x9d00, 0x3500,
	0x1e00, 0x2400, 0x0e00, 0x5e00, 0x6300, 0x5800, 0xd100, 0xa200,
	0x2500, 0x2200, 0x7c00, 0x3b00, 0x0100, 0x2100, 0x7800, 0x8700,
	0xd400, 0x0000, 0x4600, 0x5700, 0x9f00, 0xd300, 0x2700, 0x5200,
	0x4c00, 0x3600, 0x0200, 0xe700, 0xa000, 0xc400, 0xc800, 0x9e00,
	0xea00, 0xbf00, 0x8a00, 0xd200, 0x4000, 0xc700, 0x3800, 0xb500,
	0xa300, 0xf700, 0xf200, 0xce00, 0xf900, 0x6100, 0x1500, 0xa100,
	0xe000, 0xae00, 0x5d00, 0xa400, 0x9b00, 0x3400, 0x1a00, 0x5500,
	0xad00, 0x9300, 0x3200, 0x3000, 0xf500, 0x8c00, 0xb100, 0xe300,
	0x1d00, 0xf600, 0xe200, 0x2e00, 0x8200, 0x6600, 0xca00, 0x6000,
	0xc000, 0x2900, 0x2300, 0xab00, 0x0d00, 0x5300, 0x4e00, 0x6f00,
	0xd500, 0xdb00, 0x3700, 0x4500, 0xde00, 0xfd00, 0x8e00, 0x2f00,
	0x0300, 0xff00, 0x6a00, 0x7200, 0x6d00, 0x6c00, 0x5b00, 0x5100,
	0x8d00, 0x1b00, 0xaf00, 0x9200, 0xbb00, 0xdd00, 0xbc00, 0x7f00,
	0x1100, 0xd900, 0x5c00, 0x4100, 0x1f00, 0x1000, 0x5a00, 0xd800,
	0x0a00, 0xc100, 0x3100, 0x8800, 0xa500, 0xcd00, 0x7b00, 0xbd00,
	0x2d00, 0x7400, 0xd000, 0x1200, 0xb800, 0xe500, 0xb400, 0xb000,
	0x8900, 0x6900, 0x9700, 0x4a00, 0x0c00, 0x9600, 0x7700, 0x7e00,
	0x6500, 0xb900, 0xf100, 0x0900, 0xc500, 0x6e00, 0xc600, 0x8400,
	0x1800, 0xf000, 0x7d00, 0xec00, 0x3a00, 0xdc00, 0x4d00, 0x2000,
	0x7900, 0xee00, 0x5f00, 0x3e00, 0xd700, 0xcb00, 0x3900, 0x4800
};

static const uint32 S_box2[] = {
	0xd60000, 0x900000, 0xe90000, 0xfe0000,
	0xcc0000, 0xe10000, 0x3d0000, 0xb70000,
	0x160000, 0xb60000, 0x140000, 0xc20000,
	0x280000, 0xfb0000, 0x2c0000, 0x050000,
	0x2b0000, 0x670000, 0x9a0000, 0x760000,
	0x2a0000, 0xbe0000, 0x040000, 0xc30000,
	0xaa0000, 0x440000, 0x130000, 0x260000,
	0x490000, 0x860000, 0x060000, 0x990000,
	0x9c0000, 0x420000, 0x500000, 0xf40000,
	0x910000, 0xef0000, 0x980000, 0x7a0000,
	0x330000, 0x540000, 0x0b0000, 0x430000,
	0xed0000, 0xcf0000, 0xac0000, 0x620000,
	0xe40000, 0xb30000, 0x1c0000, 0xa90000,
	0xc90000, 0x080000, 0xe80000, 0x950000,
	0x800000, 0xdf0000, 0x940000, 0xfa0000,
	0x750000, 0x8f0000, 0x3f0000, 0xa60000,
	0x470000, 0x070000, 0xa70000, 0xfc0000,
	0xf30000, 0x730000, 0x170000, 0xba0000,
	0x830000, 0x590000, 0x3c0000, 0x190000,
	0xe60000, 0x850000, 0x4f0000, 0xa80000,
	0x680000, 0x6b0000, 0x810000, 0xb20000,
	0x710000, 0x640000, 0xda0000, 0x8b0000,
	0xf80000, 0xeb0000, 0x0f0000, 0x4b0000,
	0x700000, 0x560000, 0x9d0000, 0x350000,
	0x1e0000, 0x240000, 0x0e0000, 0x5e0000,
	0x630000, 0x580000, 0xd10000, 0xa20000,
	0x250000, 0x220000, 0x7c0000, 0x3b0000,
	0x010000, 0x210000, 0x780000, 0x870000,
	0xd40000, 0x000000, 0x460000, 0x570000,
	0x9f0000, 0xd30000, 0x270000, 0x520000,
	0x4c0000, 0x360000, 0x020000, 0xe70000,
	0xa00000, 0xc40000, 0xc80000, 0x9e0000,
	0xea0000, 0xbf0000, 0x8a0000, 0xd20000,
	0x400000, 0xc70000, 0x380000, 0xb50000,
	0xa30000, 0xf70000, 0xf20000, 0xce0000,
	0xf90000, 0x610000, 0x150000, 0xa10000,
	0xe00000, 0xae0000, 0x5d0000, 0xa40000,
	0x9b0000, 0x340000, 0x1a0000, 0x550000,
	0xad0000, 0x930000, 0x320000, 0x300000,
	0xf50000, 0x8c0000, 0xb10000, 0xe30000,
	0x1d0000, 0xf60000, 0xe20000, 0x2e0000,
	0x820000, 0x660000, 0xca0000, 0x600000,
	0xc00000, 0x290000, 0x230000, 0xab0000,
	0x0d0000, 0x530000, 0x4e0000, 0x6f0000,
	0xd50000, 0xdb0000, 0x370000, 0x450000,
	0xde0000, 0xfd0000, 0x8e0000, 0x2f0000,
	0x030000, 0xff0000, 0x6a0000, 0x720000,
	0x6d0000, 0x6c0000, 0x5b0000, 0x510000,
	0x8d0000, 0x1b0000, 0xaf0000, 0x920000,
	0xbb0000, 0xdd0000, 0xbc0000, 0x7f0000,
	0x110000, 0xd90000, 0x5c0000, 0x410000,
	0x1f0000, 0x100000, 0x5a0000, 0xd80000,
	0x0a0000, 0xc10000, 0x310000, 0x880000,
	0xa50000, 0xcd0000, 0x7b0000, 0xbd0000,
	0x2d0000, 0x740000, 0xd00000, 0x120000,
	0xb80000, 0xe50000, 0xb40000, 0xb00000,
	0x890000, 0x690000, 0x970000, 0x4a0000,
	0x0c0000, 0x960000, 0x770000, 0x7e0000,
	0x650000, 0xb90000, 0xf10000, 0x090000,
	0xc50000, 0x6e0000, 0xc60000, 0x840000,
	0x180000, 0xf00000, 0x7d0000, 0xec0000,
	0x3a0000, 0xdc0000, 0x4d0000, 0x200000,
	0x790000, 0xee0000, 0x5f0000, 0x3e0000,
	0xd70000, 0xcb0000, 0x390000, 0x480000
};

static const uint32 S_box3[] = {
	0xd6000000, 0x90000000, 0xe9000000, 0xfe000000,
	0xcc000000, 0xe1000000, 0x3d000000, 0xb7000000,
	0x16000000, 0xb6000000, 0x14000000, 0xc2000000,
	0x28000000, 0xfb000000, 0x2c000000, 0x05000000,
	0x2b000000, 0x67000000, 0x9a000000, 0x76000000,
	0x2a000000, 0xbe000000, 0x04000000, 0xc3000000,
	0xaa000000, 0x44000000, 0x13000000, 0x26000000,
	0x49000000, 0x86000000, 0x06000000, 0x99000000,
	0x9c000000, 0x42000000, 0x50000000, 0xf4000000,
	0x91000000, 0xef000000, 0x98000000, 0x7a000000,
	0x33000000, 0x54000000, 0x0b000000, 0x43000000,
	0xed000000, 0xcf000000, 0xac000000, 0x62000000,
	0xe4000000, 0xb3000000, 0x1c000000, 0xa9000000,
	0xc9000000, 0x08000000, 0xe8000000, 0x95000000,
	0x80000000, 0xdf000000, 0x94000000, 0xfa000000,
	0x75000000, 0x8f000000, 0x3f000000, 0xa6000000,
	0x47000000, 0x07000000, 0xa7000000, 0xfc000000,
	0xf3000000, 0x73000000, 0x17000000, 0xba000000,
	0x83000000, 0x59000000, 0x3c000000, 0x19000000,
	0xe6000000, 0x85000000, 0x4f000000, 0xa8000000,
	0x68000000, 0x6b000000, 0x81000000, 0xb2000000,
	0x71000000, 0x64000000, 0xda000000, 0x8b000000,
	0xf8000000, 0xeb000000, 0x0f000000, 0x4b000000,
	0x70000000, 0x56000000, 0x9d000000, 0x35000000,
	0x1e000000, 0x24000000, 0x0e000000, 0x5e000000,
	0x63000000, 0x58000000, 0xd1000000, 0xa2000000,
	0x25000000, 0x22000000, 0x7c000000, 0x3b000000,
	0x01000000, 0x21000000, 0x78000000, 0x87000000,
	0xd4000000, 0x00000000, 0x46000000, 0x57000000,
	0x9f000000, 0xd3000000, 0x27000000, 0x52000000,
	0x4c000000, 0x36000000, 0x02000000, 0xe7000000,
	0xa0000000, 0xc4000000, 0xc8000000, 0x9e000000,
	0xea000000, 0xbf000000, 0x8a000000, 0xd2000000,
	0x40000000, 0xc7000000, 0x38000000, 0xb5000000,
	0xa3000000, 0xf7000000, 0xf2000000, 0xce000000,
	0xf9000000, 0x61000000, 0x15000000, 0xa1000000,
	0xe0000000, 0xae000000, 0x5d000000, 0xa4000000,
	0x9b000000, 0x34000000, 0x1a000000, 0x55000000,
	0xad000000, 0x93000000, 0x32000000, 0x30000000,
	0xf5000000, 0x8c000000, 0xb1000000, 0xe3000000,
	0x1d000000, 0xf6000000, 0xe2000000, 0x2e000000,
	0x82000000, 0x66000000, 0xca000000, 0x60000000,
	0xc0000000, 0x29000000, 0x23000000, 0xab000000,
	0x0d000000, 0x53000000, 0x4e000000, 0x6f000000,
	0xd5000000, 0xdb000000, 0x37000000, 0x45000000,
	0xde000000, 0xfd000000, 0x8e000000, 0x2f000000,
	0x03000000, 0xff000000, 0x6a000000, 0x72000000,
	0x6d000000, 0x6c000000, 0x5b000000, 0x51000000,
	0x8d000000, 0x1b000000, 0xaf000000, 0x92000000,
	0xbb000000, 0xdd000000, 0xbc000000, 0x7f000000,
	0x11000000, 0xd9000000, 0x5c000000, 0x41000000,
	0x1f000000, 0x10000000, 0x5a000000, 0xd8000000,
	0x0a000000, 0xc1000000, 0x31000000, 0x88000000,
	0xa5000000, 0xcd000000, 0x7b000000, 0xbd000000,
	0x2d000000, 0x74000000, 0xd0000000, 0x12000000,
	0xb8000000, 0xe5000000, 0xb4000000, 0xb0000000,
	0x89000000, 0x69000000, 0x97000000, 0x4a000000,
	0x0c000000, 0x96000000, 0x77000000, 0x7e000000,
	0x65000000, 0xb9000000, 0xf1000000, 0x09000000,
	0xc5000000, 0x6e000000, 0xc6000000, 0x84000000,
	0x18000000, 0xf0000000, 0x7d000000, 0xec000000,
	0x3a000000, 0xdc000000, 0x4d000000, 0x20000000,
	0x79000000, 0xee000000, 0x5f000000, 0x3e000000,
	0xd7000000, 0xcb000000, 0x39000000, 0x48000000
};
#endif /* __i386__ */

/* Non-linear transform
 * A = (a0, a1, a2, a3)
 * B = (b0, b1, b2, b3)
 * (b0, b1, b2, b3) = tau(A) = (Sbox(a0), Sbox(a1), Sbox(a2), Sbox(a3))
 */
static INLINE uint32
tau(uint32 A)
{
	uint32 B;

#if !defined(__i386__)
	B = (S_box[A & 0xff] |
	     (S_box[(A & 0xff00) >> 8] << 8) |
	     (S_box[(A & 0xff0000) >> 16] << 16) |
	     (S_box[(A & 0xff000000) >> 24] << 24));
#else /* __i386__ */
/* slightly better performance on Pentium, worse performance on ARM CM3 */
	B = (S_box0[A & 0xff] |
	     S_box1[(A & 0xff00) >> 8] |
	     S_box2[(A & 0xff0000) >> 16] |
	     S_box3[(A & 0xff000000) >> 24]);
#endif /* __i386__ */

	return (B);
}

/* Linear transform
 * C = L(B) = B ^ (B<<<2) ^ (B<<<10) ^ (B<<<18) ^ (B<<<24)
 *   where "<<<" is a circular left shift
 */
static INLINE uint32
L(uint32 B)
{
	uint32 Ba = B ^ ROTATE(B, 24);
	return (Ba ^ ROTATE((ROTATE(Ba, 16) ^ B), 2));
}

/* Compound Transform
 * T(.) = L(tau(.))
 */
static INLINE uint32
T(uint32 X)
{
	return (L(tau(X)));
}

/* Round Function
 * F(X0,X1,X2,X3,RK) = X0 ^ T(X1^X2^X3^RK)
 * static INLINE uint32
 * F(uint32 *X, uint32 RK)
 * {
 * 	return (X[0] ^ T(X[1] ^ X[2] ^ X[3] ^ RK));
 * }
 */

/* Encryption/decryption algorithm
 * Xi+4 = F(Xi, Xi+1, Xi+2, Xi+3, RKj) = Xi ^ T(Xi+1 ^ Xi+2 ^ Xi+3 ^ RKj)
 *   i=0,1,...31, j=i(enc) or j=31-i(dec)
 * (Y0, Y1, Y2, Y3) = (X35, X34, X33, X32)
 */

/* define SMS4_FULL_UNROLL to completely unroll F() - results in slightly faster but bigger code */
#define SMS4_FULL_UNROLL

void
sms4_enc(uint32 *Y, uint32 *X, const uint32 *RK)
{
	uint32 z0 = X[0], z1 = X[1], z2 = X[2], z3 = X[3];
#ifndef SMS4_FULL_UNROLL
	int i;

	for (i = 0; i < SMS4_RK_WORDS; i += 4) {
		z0 ^= T(z1 ^ z2 ^ z3 ^ *RK++);
		z1 ^= T(z2 ^ z3 ^ z0 ^ *RK++);
		z2 ^= T(z3 ^ z0 ^ z1 ^ *RK++);
		z3 ^= T(z0 ^ z1 ^ z2 ^ *RK++);
	}
#else /* SMS4_FULL_UNROLL */
	z0 ^= T(z1 ^ z2 ^ z3 ^ *RK++);
	z1 ^= T(z2 ^ z3 ^ z0 ^ *RK++);
	z2 ^= T(z3 ^ z0 ^ z1 ^ *RK++);
	z3 ^= T(z0 ^ z1 ^ z2 ^ *RK++);
	z0 ^= T(z1 ^ z2 ^ z3 ^ *RK++);
	z1 ^= T(z2 ^ z3 ^ z0 ^ *RK++);
	z2 ^= T(z3 ^ z0 ^ z1 ^ *RK++);
	z3 ^= T(z0 ^ z1 ^ z2 ^ *RK++);
	z0 ^= T(z1 ^ z2 ^ z3 ^ *RK++);
	z1 ^= T(z2 ^ z3 ^ z0 ^ *RK++);
	z2 ^= T(z3 ^ z0 ^ z1 ^ *RK++);
	z3 ^= T(z0 ^ z1 ^ z2 ^ *RK++);
	z0 ^= T(z1 ^ z2 ^ z3 ^ *RK++);
	z1 ^= T(z2 ^ z3 ^ z0 ^ *RK++);
	z2 ^= T(z3 ^ z0 ^ z1 ^ *RK++);
	z3 ^= T(z0 ^ z1 ^ z2 ^ *RK++);
	z0 ^= T(z1 ^ z2 ^ z3 ^ *RK++);
	z1 ^= T(z2 ^ z3 ^ z0 ^ *RK++);
	z2 ^= T(z3 ^ z0 ^ z1 ^ *RK++);
	z3 ^= T(z0 ^ z1 ^ z2 ^ *RK++);
	z0 ^= T(z1 ^ z2 ^ z3 ^ *RK++);
	z1 ^= T(z2 ^ z3 ^ z0 ^ *RK++);
	z2 ^= T(z3 ^ z0 ^ z1 ^ *RK++);
	z3 ^= T(z0 ^ z1 ^ z2 ^ *RK++);
	z0 ^= T(z1 ^ z2 ^ z3 ^ *RK++);
	z1 ^= T(z2 ^ z3 ^ z0 ^ *RK++);
	z2 ^= T(z3 ^ z0 ^ z1 ^ *RK++);
	z3 ^= T(z0 ^ z1 ^ z2 ^ *RK++);
	z0 ^= T(z1 ^ z2 ^ z3 ^ *RK++);
	z1 ^= T(z2 ^ z3 ^ z0 ^ *RK++);
	z2 ^= T(z3 ^ z0 ^ z1 ^ *RK++);
	z3 ^= T(z0 ^ z1 ^ z2 ^ *RK++);
#endif /* SMS4_FULL_UNROLL */

	Y[0] = z3;
	Y[1] = z2;
	Y[2] = z1;
	Y[3] = z0;
}

void
sms4_dec(uint32 *Y, uint32 *X, uint32 *RK)
{
	uint32 z0 = X[0], z1 = X[1], z2 = X[2], z3 = X[3];
	int i;

	RK += 32;

	for (i = 0; i < SMS4_RK_WORDS; i += 4) {
		z0 ^= T(z1 ^ z2 ^ z3 ^ *--RK);
		z1 ^= T(z2 ^ z3 ^ z0 ^ *--RK);
		z2 ^= T(z3 ^ z0 ^ z1 ^ *--RK);
		z3 ^= T(z0 ^ z1 ^ z2 ^ *--RK);
	}

	Y[0] = z3;
	Y[1] = z2;
	Y[2] = z1;
	Y[3] = z0;
}

static const uint32 CK[] = {
	0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269,
	0x70777e85, 0x8c939aa1, 0xa8afb6bd, 0xc4cbd2d9,
	0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249,
	0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9,
	0xc0c7ced5, 0xdce3eaf1, 0xf8ff060d, 0x141b2229,
	0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299,
	0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209,
	0x10171e25, 0x2c333a41, 0x484f565d, 0x646b7279
};

static const uint32 FK[] = {
	0xA3B1BAC6,
	0x56AA3350,
	0x677D9197,
	0xB27022DC
};

/* Key Expansion Linear transform
 * Lprime(B) = B ^ (B<<<13) ^ (B<<<23)
 *   where "<<<" is a circular left shift
 */
static INLINE uint32
Lprime(uint32 B)
{
	return (B ^ ROTATE(B, 13) ^ ROTATE(B, 23));
}

/* Key Expansion Compound Transform
 * Tprime(.) = Lprime(tau(.))
 */
static INLINE uint32
Tprime(uint32 X)
{
	return (Lprime(tau(X)));
}

/* Key Expansion
 * Encryption key MK = (MK0, MK1, MK2, MK3)
 * (K0, K1, K2, K3) = (MK0 ^ FK0, MK1 ^ FK1, MK2 ^ FK2, MK3 ^ FK3)
 * RKi = Ki+4 = Ki ^ Tprime(Ki+1 ^ Ki+2 ^ Ki+3 ^ CKi+4)
 */
void
sms4_key_exp(uint32 *MK, uint32 *RK)
{
	int i;
	uint32 K[36];

	for (i = 0; i < 4; i++)
		K[i] = MK[i] ^ FK[i];

	for (i = 0; i < SMS4_RK_WORDS; i++) {
		K[i+4] = K[i] ^ Tprime(K[i+1] ^ K[i+2] ^ K[i+3] ^ CK[i]);
		RK[i] = K[i+4];
	}

	return;
}

/* SMS4-CBC-MAC mode for WPI
 *	- computes SMS4_WPI_CBC_MAC_LEN MAC
 *	- Integrity Check Key must be SMS4_KEY_LEN bytes
 *	- PN must be SMS4_WPI_PN_LEN bytes
 *	- AAD inludes Key Index, Reserved, and L (data len) fields
 *	- For MAC calculation purposes, the aad and data are each padded with
 *	  NULLs to a multiple of the block size
 *	- ptxt must have sufficient tailroom for storing the MAC
 *	- returns -1 on error
 *	- returns SMS4_WPI_SUCCESS on success, SMS4_WPI_CBC_MAC_ERROR on error
 */
int
sms4_wpi_cbc_mac(const uint8 *ick,
	const uint8 *pn,
	const size_t aad_len,
	const uint8 *aad,
	uint8 *ptxt)
{
	int k, j, rem_len;
	uint32 RK[SMS4_RK_WORDS];
	uint32 X[SMS4_BLOCK_WORDS], Y[SMS4_BLOCK_WORDS];
	uint8 tmp[SMS4_BLOCK_SZ];
	uint16 data_len = (aad[aad_len-2] << 8) | (aad[aad_len-1]);

	if (data_len > SMS4_WPI_MAX_MPDU_LEN)
		return (SMS4_WPI_CBC_MAC_ERROR);

	sms4_print_bytes("MIC Key", (uchar *)ick, SMS4_WPI_CBC_MAC_LEN);
	sms4_print_bytes("PN ", (uchar *)pn, SMS4_WPI_PN_LEN);
	sms4_print_bytes("MIC data: PART1", (uchar *)aad, aad_len);
	sms4_print_bytes("MIC data: PART 2", (uchar *)ptxt, data_len);

	/* Prepare the round key */
	for (k = 0; k < SMS4_BLOCK_WORDS; k++)
		if ((uintptr)ick & 3)
			X[k] = ntoh32_ua(ick + (SMS4_WORD_SZ * k));
		else
			X[k] = ntoh32(*(uint32 *)(ick + (SMS4_WORD_SZ * k)));
	sms4_key_exp(X, RK);

	/* First block: PN */
	for (k = 0; k < SMS4_BLOCK_WORDS; k++)
		if ((uintptr)pn & 3)
			X[k] = ntoh32_ua(pn + (SMS4_WORD_SZ * k));
		else
			X[k] = ntoh32(*(uint32 *)(pn + (SMS4_WORD_SZ * k)));
	sms4_enc(Y, X, RK);

	/* Next blocks: AAD */
	for (j = 0; j < aad_len/SMS4_BLOCK_SZ; j++) {
		for (k = 0; k < SMS4_BLOCK_WORDS; k++)
			if ((uintptr)aad & 3)
				X[k] = Y[k] ^ ntoh32_ua(aad + (SMS4_WORD_SZ * k));
			else
				X[k] = Y[k] ^ ntoh32(*(uint32 *)(aad + (SMS4_WORD_SZ * k)));
		aad += SMS4_BLOCK_SZ;
		sms4_enc(Y, X, RK);
	}
	/* If the last block is partial, pad with NULLs */
	rem_len = aad_len % SMS4_BLOCK_SZ;
	if (rem_len) {
		bcopy(aad, tmp, rem_len);
		bzero(tmp + rem_len, SMS4_BLOCK_SZ - rem_len);
		for (k = 0; k < SMS4_BLOCK_WORDS; k++)
			if ((uintptr)tmp & 3)
				X[k] = Y[k] ^ ntoh32_ua(tmp + (SMS4_WORD_SZ * k));
			else
				X[k] = Y[k] ^ ntoh32(*(uint32 *)(tmp + (SMS4_WORD_SZ * k)));
		sms4_enc(Y, X, RK);
	}

	/* Then the message data */
	for (j = 0; j < (data_len / SMS4_BLOCK_SZ); j++) {
		for (k = 0; k < SMS4_BLOCK_WORDS; k++)
			if ((uintptr)ptxt & 3)
				X[k] = Y[k] ^ ntoh32_ua(ptxt + (SMS4_WORD_SZ * k));
			else
				X[k] = Y[k] ^ ntoh32(*(uint32 *)(ptxt + (SMS4_WORD_SZ * k)));
		ptxt += SMS4_BLOCK_SZ;
		sms4_enc(Y, X, RK);
	}
	/* If the last block is partial, pad with NULLs */
	rem_len = data_len % SMS4_BLOCK_SZ;
	if (rem_len) {
		bcopy(ptxt, tmp, rem_len);
		bzero(tmp + rem_len, SMS4_BLOCK_SZ - rem_len);
		for (k = 0; k < SMS4_BLOCK_WORDS; k++)
			if ((uintptr)tmp & 3)
				X[k] = Y[k] ^ ntoh32_ua(tmp + (SMS4_WORD_SZ * k));
			else
				X[k] = Y[k] ^ ntoh32(*(uint32 *)(tmp + (SMS4_WORD_SZ * k)));
		ptxt += data_len % SMS4_BLOCK_SZ;
		sms4_enc(Y, X, RK);
	}

	for (k = 0; k < SMS4_BLOCK_WORDS; k++) {
		hton32_ua_store(Y[k], ptxt);
		ptxt += SMS4_WORD_SZ;
	}

	return (SMS4_WPI_SUCCESS);
}

/*
 * ick		pn		ptxt	result
 * ltoh		ltoh	ltoh	fail
 * ntoh		ltoh	ltoh	fail
 * ltoh		ntoh	ltoh	fail
 * ntoh		ntoh	ltoh	fail
 *
 * ltoh		ltoh	ntoh	fail
 * ntoh		ltoh	ntoh	fail
 * ltoh		ntoh	ntoh	fail
 * ntoh		ntoh	ntoh	fail
 */

#define	s_ick(a)	ntoh32_ua(a)
#define	s_pn(a)		ntoh32_ua(a)
#define	s_ptxt(a)	ntoh32_ua(a)

#ifdef BCMSMS4_TEST
static int sms4_cbc_mac(const uint8 *ick, const uint8 *pn, const size_t data_len,
	uint8 *ptxt, uint8 *mac);
static int
sms4_cbc_mac(const uint8 *ick,
	const uint8 *pn,
	const size_t data_len,
	uint8 *ptxt,
	uint8 *mac)
{
	int k, j, rem_len;
	uint32 RK[SMS4_RK_WORDS];
	uint32 X[SMS4_BLOCK_WORDS], Y[SMS4_BLOCK_WORDS];
	uint8 tmp[SMS4_BLOCK_SZ];

	if (data_len > SMS4_WPI_MAX_MPDU_LEN)
		return (SMS4_WPI_CBC_MAC_ERROR);

	/* Prepare the round key */
	for (k = 0; k < SMS4_BLOCK_WORDS; k++)
		X[k] = s_ick(ick + (SMS4_WORD_SZ * k));
	sms4_key_exp(X, RK);

	/* First block: PN */
	for (k = 0; k < SMS4_BLOCK_WORDS; k++)
		X[k] = s_pn(pn + (SMS4_WORD_SZ * k));
	sms4_enc(Y, X, RK);

	/* Then the message data */
	for (j = 0; j < (data_len / SMS4_BLOCK_SZ); j++) {
		for (k = 0; k < SMS4_BLOCK_WORDS; k++)
			X[k] = Y[k] ^ s_ptxt(ptxt + (SMS4_WORD_SZ * k));
		ptxt += SMS4_BLOCK_SZ;
		sms4_enc(Y, X, RK);
	}
	/* If the last block is partial, pad with NULLs */
	rem_len = data_len % SMS4_BLOCK_SZ;
	if (rem_len) {
		bcopy(ptxt, tmp, rem_len);
		bzero(tmp + rem_len, SMS4_BLOCK_SZ - rem_len);
		for (k = 0; k < SMS4_BLOCK_WORDS; k++)
			X[k] = Y[k] ^ s_ptxt(tmp + (SMS4_WORD_SZ * k));
		ptxt += data_len % SMS4_BLOCK_SZ;
		sms4_enc(Y, X, RK);
	}

	for (k = 0; k < SMS4_BLOCK_WORDS; k++) {
		hton32_ua_store(Y[k], mac);
		mac += SMS4_WORD_SZ;
	}

	return (SMS4_WPI_SUCCESS);
}
#endif /* BCMSMS4_TEST */

/* SMS4-OFB mode encryption/decryption algorithm
 *	- PN must be SMS4_WPI_PN_LEN bytes
 *	- assumes PN is ready to use as-is (i.e. any
 *		randomization of PN is handled by the caller)
 *	- encrypts data in place
 *	- returns SMS4_WPI_SUCCESS on success, SMS4_WPI_OFB_ERROR on error
 */
int
sms4_ofb_crypt(const uint8 *ek,
	const uint8 *pn,
	const size_t data_len,
	uint8 *ptxt)
{
	size_t j, k;
	uint8 tmp[SMS4_BLOCK_SZ];
	uint32 RK[SMS4_RK_WORDS];
	uint32 X[SMS4_BLOCK_WORDS];

	if (data_len > SMS4_WPI_MAX_MPDU_LEN) return (SMS4_WPI_OFB_ERROR);

	sms4_print_bytes("ENC Key", (uchar *)ek, SMS4_WPI_CBC_MAC_LEN);
	sms4_print_bytes("PN ", (uint8 *)pn, SMS4_WPI_PN_LEN);
	sms4_print_bytes("data", (uchar *)ptxt, data_len);

	/* Prepare the round key */
	for (k = 0; k < SMS4_BLOCK_WORDS; k++)
		if ((uintptr)ek & 3)
			X[k] = ntoh32_ua(ek + (SMS4_WORD_SZ * k));
		else
			X[k] = ntoh32(*(uint32 *)(ek + (SMS4_WORD_SZ * k)));
	sms4_key_exp(X, RK);

	for (k = 0; k < SMS4_BLOCK_WORDS; k++) {
		if ((uintptr)pn & 3)
			X[k] = ntoh32_ua(pn + (SMS4_WORD_SZ * k));
		else
			X[k] = ntoh32(*(uint32 *)(pn + (SMS4_WORD_SZ * k)));
	}

	for (k = 0; k < (data_len / SMS4_BLOCK_SZ); k++) {
		sms4_enc(X, X, RK);
		for (j = 0; j < SMS4_BLOCK_WORDS; j++) {
			hton32_ua_store(X[j], &tmp[j * SMS4_WORD_SZ]);
		}
		xor_128bit_block(ptxt, tmp, ptxt);
		ptxt += SMS4_BLOCK_SZ;
	}

	/* handle partial block */
	if (data_len % SMS4_BLOCK_SZ) {
		sms4_enc(X, X, RK);
		for (j = 0; j < SMS4_BLOCK_WORDS; j++) {
			hton32_ua_store(X[j], &tmp[j * SMS4_WORD_SZ]);
		}
		for (k = 0; k < (data_len % SMS4_BLOCK_SZ); k++)
			ptxt[k] ^= tmp[k];
	}

	return (SMS4_WPI_SUCCESS);
}

/* SMS4-WPI mode encryption of 802.11 packet
 * 	- constructs aad and pn from provided frame
 * 	- calls sms4_wpi_cbc_mac() to compute MAC
 * 	- calls sms4_ofb_crypt() to encrypt frame
 * 	- encrypts data in place
 * 	- supplied packet must have sufficient tailroom for CBC-MAC MAC
 * 	- data_len includes 802.11 header and CBC-MAC MAC
 *	- returns SMS4_WPI_SUCCESS on success, SMS4_WPI_ENCRYPT_ERROR on error
 */
int
sms4_wpi_pkt_encrypt(const uint8 *ek,
                          const uint8 *ick,
                          const size_t data_len,
                          uint8 *p)
{
	uint8 aad[SMS4_WPI_MAX_AAD_LEN];
	uint8 *paad = aad;
	struct dot11_header *h = (struct dot11_header *)p;
	struct wpi_iv *iv_data;
	uint16 fc, seq;
	uint header_len, aad_len, qos_len = 0, hdr_add_len = 0;
	bool wds = FALSE;
	uint8 tmp[SMS4_BLOCK_SZ];
	int k;

	bzero(aad, SMS4_WPI_MAX_AAD_LEN);

	fc = ltoh16_ua(&(h->fc));

	/* WPI only supports protection of DATA frames */
	if (FC_TYPE(fc) != FC_TYPE_DATA)
		return (SMS4_WPI_ENCRYPT_ERROR);

	/* frame must have Protected flag set */
	if (!(fc & FC_WEP))
		return (SMS4_WPI_ENCRYPT_ERROR);

	/* all QoS subtypes have the FC_SUBTYPE_QOS_DATA bit set */
	if (FC_SUBTYPE(fc) & FC_SUBTYPE_QOS_DATA)
		qos_len += 2;

	/* length of A4, if using wds frames */
	wds = ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS));
	if (wds)
		hdr_add_len += ETHER_ADDR_LEN;

	/* length of MPDU header, including PN */
	header_len = DOT11_A3_HDR_LEN + SMS4_WPI_IV_LEN + hdr_add_len + qos_len;

	/* pointer to IV */
	iv_data = (struct wpi_iv *)(p + DOT11_A3_HDR_LEN + qos_len + hdr_add_len);

	/* Payload must be > 0 bytes */
	if (data_len <= (header_len + SMS4_WPI_CBC_MAC_LEN))
		return (SMS4_WPI_ENCRYPT_ERROR);

	/* aad: maskedFC || A1 || A2 || maskedSC || A3 || A4 || KeyIdx || Reserved || L */

	fc &= SMS4_WPI_FC_MASK;
	*paad++ = (uint8)(fc & 0xff);
	*paad++ = (uint8)((fc >> 8) & 0xff);

	bcopy((uchar *)&h->a1, paad, 2*ETHER_ADDR_LEN);
	paad += 2*ETHER_ADDR_LEN;

	seq = ltoh16_ua(&(h->seq));
	seq &= FRAGNUM_MASK;

	*paad++ = (uint8)(seq & 0xff);
	*paad++ = (uint8)((seq >> 8) & 0xff);

	bcopy((uchar *)&h->a3, paad, ETHER_ADDR_LEN);
	paad += ETHER_ADDR_LEN;

	if (wds) {
		bcopy((uchar *)&h->a4, paad, ETHER_ADDR_LEN);
	}
	/* A4 for the MIC, even when there is no A4 in the packet */
	paad += ETHER_ADDR_LEN;

	if (qos_len) {
		*paad++ = p[DOT11_A3_HDR_LEN + hdr_add_len];
		*paad++ = p[DOT11_A3_HDR_LEN + hdr_add_len + 1];
	}

	*paad++ = iv_data->key_idx;
	*paad++ = iv_data->reserved;

	*paad++ = ((data_len - header_len - SMS4_WPI_CBC_MAC_LEN) >> 8) & 0xff;
	*paad++ = (data_len - header_len - SMS4_WPI_CBC_MAC_LEN) & 0xff;

	/* length of AAD */
	aad_len = SMS4_WPI_MIN_AAD_LEN + qos_len;

	for (k = 0; k < SMS4_BLOCK_SZ; k++)
		tmp[SMS4_BLOCK_SZ-(k+1)] = (iv_data->PN)[k];

	/* calculate MAC */
	if (sms4_wpi_cbc_mac(ick, tmp, aad_len, aad, p + header_len))
		return (SMS4_WPI_ENCRYPT_ERROR);

	/* encrypt data */
	if (sms4_ofb_crypt(ek, tmp, data_len - header_len, p + header_len))
		return (SMS4_WPI_ENCRYPT_ERROR);

	return (SMS4_WPI_SUCCESS);
}

/* SMS4-WPI mode decryption of 802.11 packet
 * 	- constructs aad and pn from provided frame
 * 	- calls sms4_ofb_crypt() to decrypt frame
 * 	- calls sms4_wpi_cbc_mac() to compute MAC
 * 	- decrypts in place
 * 	- data_len includes 802.11 header and CBC-MAC MAC
 *	- returns SMS4_WPI_DECRYPT_ERROR on general error
 */
int
sms4_wpi_pkt_decrypt(const uint8 *ek,
                          const uint8 *ick,
                          const size_t data_len,
                          uint8 *p)
{
	uint8 aad[SMS4_WPI_MAX_AAD_LEN];
	uint8 MAC[SMS4_WPI_CBC_MAC_LEN];
	uint8 *paad = aad;
	struct dot11_header *h = (struct dot11_header *)p;
	struct wpi_iv *iv_data;
	uint16 fc, seq;
	uint header_len, aad_len, qos_len = 0, hdr_add_len = 0;
	bool wds = FALSE;
	uint8 tmp[SMS4_BLOCK_SZ];
	int k;

	bzero(aad, SMS4_WPI_MAX_AAD_LEN);

	fc = ltoh16_ua(&(h->fc));

	/* WPI only supports protection of DATA frames */
	if (FC_TYPE(fc) != FC_TYPE_DATA)
		return (SMS4_WPI_DECRYPT_ERROR);

	/* frame must have Protected flag set */
	if (!(fc & FC_WEP))
		return (SMS4_WPI_DECRYPT_ERROR);

	/* all QoS subtypes have the FC_SUBTYPE_QOS_DATA bit set */
	if ((FC_SUBTYPE(fc) & FC_SUBTYPE_QOS_DATA))
		qos_len += 2;

	/* length of A4, if using wds frames */
	wds = ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS));
	if (wds)
		hdr_add_len += ETHER_ADDR_LEN;

	/* length of MPDU header, including PN */
	header_len = DOT11_A3_HDR_LEN + SMS4_WPI_IV_LEN + hdr_add_len + qos_len;

	/* pointer to IV */
	iv_data = (struct wpi_iv *)(p + DOT11_A3_HDR_LEN + qos_len + hdr_add_len);

	/* Payload must be > 0 bytes plus MAC */
	if (data_len <= (header_len + SMS4_WPI_CBC_MAC_LEN))
		return (SMS4_WPI_DECRYPT_ERROR);

	/* aad: maskedFC || A1 || A2 || maskedSC || A3 || A4 || KeyIdx || Reserved || L */

	fc &= SMS4_WPI_FC_MASK;
	*paad++ = (uint8)(fc & 0xff);
	*paad++ = (uint8)((fc >> 8) & 0xff);

	bcopy((uchar *)&h->a1, paad, 2*ETHER_ADDR_LEN);
	paad += 2*ETHER_ADDR_LEN;

	seq = ltoh16_ua(&(h->seq));
	seq &= FRAGNUM_MASK;

	*paad++ = (uint8)(seq & 0xff);
	*paad++ = (uint8)((seq >> 8) & 0xff);

	bcopy((uchar *)&h->a3, paad, ETHER_ADDR_LEN);
	paad += ETHER_ADDR_LEN;

	if (wds) {
		bcopy((uchar *)&h->a4, paad, ETHER_ADDR_LEN);
	}
	/* A4 for the MIC, even when there is no A4 in the packet */
	paad += ETHER_ADDR_LEN;

	if (qos_len) {
		*paad++ = p[DOT11_A3_HDR_LEN + hdr_add_len];
		*paad++ = p[DOT11_A3_HDR_LEN + hdr_add_len + 1];
	}

	*paad++ = iv_data->key_idx;
	*paad++ = iv_data->reserved;

	*paad++ = ((data_len - header_len - SMS4_WPI_CBC_MAC_LEN) >> 8) & 0xff;
	*paad++ = (data_len - header_len - SMS4_WPI_CBC_MAC_LEN) & 0xff;

	/* length of AAD */
	aad_len = SMS4_WPI_MIN_AAD_LEN + qos_len;

	for (k = 0; k < SMS4_BLOCK_SZ; k++)
		tmp[SMS4_BLOCK_SZ-(k+1)] = (iv_data->PN)[k];

	/* decrypt data */
	if (sms4_ofb_crypt(ek, tmp, data_len - header_len,
		p + header_len))
		return (SMS4_WPI_DECRYPT_ERROR);

	/* store MAC */
	bcopy(p + data_len - SMS4_WPI_CBC_MAC_LEN, MAC, SMS4_WPI_CBC_MAC_LEN);

	/* calculate MAC */
	if (sms4_wpi_cbc_mac(ick, tmp, aad_len, aad, p + header_len))
		return (SMS4_WPI_DECRYPT_ERROR);

	/* compare MAC */
	if (bcmp(p + data_len - SMS4_WPI_CBC_MAC_LEN, MAC, SMS4_WPI_CBC_MAC_LEN))
		return (SMS4_WPI_CBC_MAC_ERROR);

	return (SMS4_WPI_SUCCESS);
}

#ifdef BCMSMS4_TEST
#include <stdlib.h>
#include <bcmcrypto/sms4_vectors.h>

#ifdef BCMSMS4_TEST_EMBED

/* Number of iterations is sensitive to the state of the test vectors since
 * encrypt and decrypt are done in place
 */
#define SMS4_TIMING_ITER	100
#define	dbg(args)
#define pres(label, len, data)

/* returns current time in msec */
static void
get_time(uint *t)
{
	*t = hndrte_time();
}

#else
#ifdef BCMDRIVER

/* Number of iterations is sensitive to the state of the test vectors since
 * encrypt and decrypt are done in place
 */
#define SMS4_TIMING_ITER	1000
#define	dbg(args)		printk args
#define pres(label, len, data)

/* returns current time in msec */
static void
get_time(uint *t)
{
	*t = jiffies_to_msecs(jiffies);
}

#else

#define SMS4_TIMING_ITER	10000
#include <stdio.h>
#define	dbg(args)		printf args

#include <sys/time.h>

/* returns current time in msec */
static void
get_time(uint *t)
{
	struct timeval ts;
	gettimeofday(&ts, NULL);
	*t = ts.tv_sec * 1000 + ts.tv_usec / 1000;
}

void
pres(const char *label, const size_t len, const uint8 *data)
{
	int k;
	printf("%s\n", label);
	for (k = 0; k < len; k++) {
		printf("0x%02x, ", data[k]);
		if (!((k + 1) % (SMS4_BLOCK_SZ/2)))
			printf("\n");
	}
	printf("\n");
}
#endif /* BCMDRIVER */
#endif /* BCMSMS4_TEST_EMBED */

int
sms4_test_enc_dec()
{
	uint tstart, tend;
	int i, j, k, fail = 0;
	uint32 RK[32];
	uint32 X[SMS4_BLOCK_WORDS], Y[SMS4_BLOCK_WORDS];

	for (k = 0; k < NUM_SMS4_VECTORS; k++) {
		/* Note that the algorithm spec example output lists X[0] -
		 * X[32], but those should really be labelled X[4] - X[35]
		 * (they're the round output, not the input)
		 */

		dbg(("sms4_test_enc_dec: Instance %d:\n", k + 1));
		dbg(("sms4_test_enc_dec:   Plain Text:\n"));
		for (i = 0; i < SMS4_BLOCK_WORDS; i++)
			dbg(("sms4_test_enc_dec:     PlainText[%02d] = 0x%08x\n",
				i, sms4_vec[k].input[i]));

		dbg(("sms4_test_enc_dec:   Encryption Master Key:\n"));
		for (i = 0; i < SMS4_BLOCK_WORDS; i++)
			dbg(("sms4_test_enc_dec:     MK[%02d] = 0x%08x\n",
				i, sms4_vec[k].key[i]));

		sms4_key_exp(sms4_vec[k].key, RK);
		dbg(("sms4_test_enc_dec:   Round Key:\n"));
		for (i = 0; i < SMS4_RK_WORDS; i++)
			dbg(("sms4_test_enc_dec:     rk[%02d] = 0x%08x\n",
				i, RK[i]));

		for (j = 0; j < SMS4_BLOCK_WORDS; j++)
			Y[j] = sms4_vec[k].input[j];
		get_time(&tstart);
		for (i = 0; i < *sms4_vec[k].niter; i++) {
			for (j = 0; j < SMS4_BLOCK_WORDS; j++)
				X[j] = Y[j];
			sms4_enc(Y, X, RK);
		}
		get_time(&tend);
		dbg(("sms4_test_enc_dec:   Cipher Text:\n"));
		for (i = 0; i < SMS4_BLOCK_WORDS; i++)
			dbg(("sms4_test_enc_dec:     CipherText[%02d] = 0x%08x\n",
				i, Y[i]));
		dbg(("sms4_test_enc_dec:   Time for Instance %d Encrypt: %d msec\n",
			k + 1, tend - tstart));
		for (j = 0; j < SMS4_BLOCK_WORDS; j++) {
			if (Y[j] != sms4_vec[k].ref[j]) {
				dbg(("sms4_test_enc_dec: sms4_enc failed\n"));
				fail++;
			}
		}

		for (j = 0; j < SMS4_BLOCK_WORDS; j++)
			X[j] = sms4_vec[k].ref[j];
		get_time(&tstart);
		for (i = 0; i < *sms4_vec[k].niter; i++) {
			for (j = 0; j < SMS4_BLOCK_WORDS; j++)
				X[j] = Y[j];
			sms4_dec(Y, X, RK);
		}
		get_time(&tend);
		dbg(("sms4_test_enc_dec:  Decrypted Plain Text:\n"));
		for (i = 0; i < SMS4_BLOCK_WORDS; i++)
			dbg(("sms4_test_enc_dec:    PlainText[%02d] = 0x%08x\n", i, Y[i]));
		dbg(("sms4_test_enc_dec:  Time for Instance %d Decrypt: %d msec\n",
			k + 1, tend - tstart));
		for (j = 0; j < SMS4_BLOCK_WORDS; j++) {
			if (Y[j] != sms4_vec[k].input[j]) {
				dbg(("sms4_test_enc_dec: sms4_dec failed\n"));
				fail++;
			}
		}

		dbg(("\n"));
	}

	return (fail);
}

int
sms4_test_cbc_mac()
{
	int retv, k, fail = 0;

	uint8 mac[SMS4_WPI_CBC_MAC_LEN];

	for (k = 0; k < NUM_SMS4_CBC_MAC_VECTORS; k++) {
		dbg(("sms4_test_cbc_mac: SMS4-WPI-CBC-MAC vector %d\n", k));
		retv = sms4_cbc_mac(sms4_cbc_mac_vec[k].ick,
			sms4_cbc_mac_vec[k].pn,
			sms4_cbc_mac_vec[k].al,
			sms4_cbc_mac_vec[k].input,
			mac);
		if (retv) {
			dbg(("sms4_test_cbc_mac: sms4_wpi_cbc_mac of vector %d returned error %d\n",
			     k, retv));
			fail++;
		}

		pres("sms4_test_cbc_mac: SMS4-WPI-CBC-MAC computed: ", SMS4_WPI_CBC_MAC_LEN,
			mac);
		pres("sms4_test_cbc_mac: SMS4-WPI-CBC-MAC reference: ", SMS4_WPI_CBC_MAC_LEN,
			sms4_cbc_mac_vec[k].ref);

		if (bcmp(mac, sms4_cbc_mac_vec[k].ref, SMS4_WPI_CBC_MAC_LEN) != 0) {
			dbg(("sms4_test_cbc_mac: sms4_wpi_cbc_mac of vector %d"
				" reference mismatch\n", k));
			fail++;
		}
	}

	dbg(("\n"));
	return (fail);
}

int
sms4_test_ofb_crypt()
{
	int retv, k, fail = 0;

	for (k = 0; k < NUM_SMS4_OFB_VECTORS; k++) {
		dbg(("sms4_test_ofb_crypt: SMS4-OFB vector %d\n", k));
		retv = sms4_ofb_crypt(sms4_ofb_vec[k].ek,
			sms4_ofb_vec[k].pn,
			sms4_ofb_vec[k].il,
			sms4_ofb_vec[k].input);
		if (retv) {
			dbg(("sms4_test_ofb_crypt: encrypt of vector %d returned error %d\n",
			     k, retv));
			fail++;
		}

		pres("sms4_test_ofb_crypt: SMS4-OFB ctxt: ",
			sms4_ofb_vec[k].il, sms4_ofb_vec[k].input);

		pres("sms4_test_ofb_crypt: SMS4-OFB ref: ",
			sms4_ofb_vec[k].il, sms4_ofb_vec[k].ref);

		if (bcmp(sms4_ofb_vec[k].input, sms4_ofb_vec[k].ref,
			sms4_ofb_vec[k].il) != 0) {
			dbg(("sms4_test_ofb_crypt: sms4_ofb_crypt of vector %d"
				" reference mismatch\n", k));
			fail++;
		}

		/* Run again to decrypt and restore vector */
		retv = sms4_ofb_crypt(sms4_ofb_vec[k].ek,
			sms4_ofb_vec[k].pn,
			sms4_ofb_vec[k].il,
			sms4_ofb_vec[k].input);
		if (retv) {
			dbg(("sms4_test_ofb_crypt: decrypt of vector %d returned error %d\n",
			     k, retv));
			fail++;
		}
	}

	dbg(("\n"));
	return (fail);
}

int
sms4_test_wpi_pkt_encrypt_decrypt_timing(int *t)
{
	uint tstart, tend;
	int retv, j, k, fail = 0;

	*t = 0;

	for (k = 0; k < NUM_SMS4_WPI_TIMING_VECTORS; k++) {
		dbg(("sms4_test_wpi_pkt_encrypt_decrypt_timing: timing SMS4-WPI vector %d\n", k));
		get_time(&tstart);
		for (j = 0; j < SMS4_TIMING_ITER; j++) {
			retv = sms4_wpi_pkt_encrypt(sms4_wpi_tpkt_vec[k].ek,
				sms4_wpi_tpkt_vec[k].ick,
				sms4_wpi_tpkt_vec[k].il,
				sms4_wpi_tpkt_vec[k].input);
			if (retv) {
				fail++;
			}

			retv = sms4_wpi_pkt_decrypt(sms4_wpi_tpkt_vec[k].ek,
				sms4_wpi_tpkt_vec[k].ick,
				sms4_wpi_tpkt_vec[k].il,
				sms4_wpi_tpkt_vec[k].input);
			if (retv) {
				fail++;
			}
		}

		get_time(&tend);

		dbg(("sms4_test_wpi_pkt_encrypt_decrypt_timing: Time for %d iterations of SMS4-WPI "
			" vector %d (total MPDU length %d): %d msec\n",
			SMS4_TIMING_ITER, k, sms4_wpi_tpkt_vec[k].il, tend - tstart));

		*t += tend - tstart;

	}

	return (fail);
}

int
sms4_test_wpi_pkt_encrypt()
{
	int retv, k, fail = 0;

	for (k = 0; k < NUM_SMS4_WPI_PKT_VECTORS; k++) {
		dbg(("sms4_test_wpi_pkt_encrypt: SMS4-WPI packet vector %d\n", k));
		pres("sms4_test_wpi_pkt_encrypt: SMS4-WPI ptxt: ",
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].input);

		retv = sms4_wpi_pkt_encrypt(sms4_wpi_pkt_vec[k].ek,
			sms4_wpi_pkt_vec[k].ick,
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].input);
		if (retv) {
			dbg(("sms4_test_wpi_pkt_encrypt: sms4_wpi_pkt_encrypt of vector %d"
				" returned error %d\n", k, retv));
			fail++;
		}

		pres("sms4_test_wpi_pkt_encrypt: SMS4-WPI ctxt: ",
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].input);

		pres("sms4_test_wpi_pkt_encrypt: SMS4-WPI ref: ",
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].ref);

		if (bcmp(sms4_wpi_pkt_vec[k].input, sms4_wpi_pkt_vec[k].ref,
			sms4_wpi_pkt_vec[k].il) != 0) {
			dbg(("sms4_test_wpi_pkt_encrypt: sms4_wpi_pkt_encrypt of vector %d"
				" reference mismatch\n", k));
			fail++;
		}
	}

	dbg(("\n"));
	return (fail);
}

int
sms4_test_wpi_pkt_decrypt()
{
	int retv, k, fail = 0;

	for (k = 0; k < NUM_SMS4_WPI_PKT_VECTORS; k++) {
		dbg(("sms4_test_wpi_pkt_decrypt: SMS4-WPI packet vector %d\n", k));
		pres("sms4_test_wpi_pkt_decrypt: SMS4-WPI ctxt: ",
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].input);

		pres("sms4_test_wpi_pkt_decrypt: SMS4-WPI ref: ",
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].ref);

		retv = sms4_wpi_pkt_decrypt(sms4_wpi_pkt_vec[k].ek,
			sms4_wpi_pkt_vec[k].ick,
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].input);
		if (retv) {
			dbg(("sms4_test_wpi_pkt_decrypt: sms4_wpi_pkt_decrypt of vector %d"
				" returned error %d\n", k, retv));
			fail++;
		}

		pres("sms4_test_wpi_pkt_decrypt: SMS4-WPI ptxt: ",
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].input);
	}

	dbg(("\n"));
	return (fail);
}

int
sms4_test_wpi_pkt_micfail()
{
	int retv, k, fail = 0;
	uint8 *pkt;

	for (k = 0; k < NUM_SMS4_WPI_PKT_VECTORS; k++) {
		/* copy the reference data, with an error in the last byte */
		pkt = malloc(sms4_wpi_pkt_vec[k].il);
		if (pkt == NULL) {
			dbg(("%s: out of memory\n", __FUNCTION__));
			fail++;
			return (fail);
		}

		bcopy(sms4_wpi_pkt_vec[k].ref, pkt, sms4_wpi_pkt_vec[k].il);

		/* create an error in the last byte of the MIC */
		pkt[sms4_wpi_pkt_vec[k].il - 1]++;

		/* decrypt */
		dbg(("sms4_test_wpi_pkt_decrypt: SMS4-WPI packet vector %d\n", k));
		retv = sms4_wpi_pkt_decrypt(sms4_wpi_pkt_vec[k].ek,
			sms4_wpi_pkt_vec[k].ick,
			sms4_wpi_pkt_vec[k].il,
			pkt);
		if (!retv) {
			dbg(("sms4_test_wpi_pkt_decrypt: sms4_wpi_pkt_decrypt of vector %d"
				" did not return expected error %d\n", k, retv));
			fail++;
		}

		free(pkt);
	}

	dbg(("\n"));
	return (fail);
}

int
sms4_test(int *t)
{
	int fail = 0;

	*t = 0;

#ifndef BCMSMS4_TEST_EMBED
	fail += sms4_test_enc_dec();
	fail += sms4_test_cbc_mac();
	fail += sms4_test_ofb_crypt();
#endif

	/* since encrypt and decrypt are done in place, and these
	 * functions use the same vectors, the tests must be run in order
	 */
	fail += sms4_test_wpi_pkt_encrypt();
	fail += sms4_test_wpi_pkt_decrypt();
	fail += sms4_test_wpi_pkt_micfail();

	fail += sms4_test_wpi_pkt_encrypt_decrypt_timing(t);

	return (fail);
}

#ifdef BCMSMS4_TEST_STANDALONE
int
main(int argc, char **argv)
{
	int fail = 0, t;

	fail += sms4_test(&t);

	dbg(("%s: timing result: %d msec\n", __FUNCTION__, t));
	fprintf(stderr, "%s: %s\n", *argv, fail ? "FAILED" : "PASSED");
	return (fail);

}
#endif /* BCMSMS4_TEST_STANDALONE */

#endif /* BCMSMS4_TEST */

#endif /* BCMWAPI_WPI */
