/*
 * DDR23 Denali contoller & DDRPHY init routines.
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
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <hndsoc.h>
#include <siutils.h>
#include <bcmnvram.h>
#include <chipcommonb.h>
#include <ddr_core.h>
#include <hndcpu.h>

#include <ddr40_phy_init.h>
#include <shmoo_public.h>
#include <bcm5301x_otp.h>


#define DDR_DEFAULT_CLOCK	333
#define DDR3_MIN_CLOCK		400

#define OTPP_TRIES	10000000	/* # of tries for OTPP */
#define DEFAULT_OTPCPU_CTRL0 0x00a00600

extern void si_mem_setclock(si_t *sih, uint32 ddrclock);

unsigned int ddr2_init_tab_400[] = {
	0,	0x00000400,
	1,	0x00000000,
	3,	0x00000050,
	4,	0x000000c8,
	5,	0x0c050c02,
	6,	0x04020405,
	7,	0x05031015,
	8,	0x03101504,
	9,	0x05020305,
	10,	0x03006d60,
	11,	0x05020303,
	12,	0x03006d60,
	13,	0x01000003,
	14,	0x05061001,
	15,	0x000b0b06,
	16,	0x030000c8,
	17,	0x00a01212,
	18,	0x060600a0,
	19,	0x00000000,
	20,	0x00003001,
	21,	0x00300c2d,
	22,	0x00050c2d,
	23,	0x00000200,
	24,	0x000a0002,
	25,	0x0002000a,
	26,	0x00020008,
	27,	0x00c80008,
	28,	0x00c80037,
	29,	0x00000037,
	30,	0x03000001,
	31,	0x00030303,
	32,	0x00000000,
	35,	0x00000000,
	36,	0x01000000,
	37,	0x10000000,
	38,	0x00100400,
	39,	0x00000400,
	40,	0x00000100,
	41,	0x00000000,
	42,	0x00000001,
	43,	0x00000000,
	44,	0x000a6300,
	45,	0x00000004,
	46,	0x00040a63,
	47,	0x00000000,
	48,	0x0a630000,
	49,	0x00000004,
	50,	0x00040a63,
	51,	0x00000000,
	52,	0x0a630000,
	53,	0x00000004,
	54,	0x00040a63,
	55,	0x00000000,
	56,	0x0a630000,
	57,	0x00000004,
	58,	0x00040a63,
	59,	0x00000000,
	60,	0x00000000,
	61,	0x00010100,
	62,	0x00000000,
	63,	0x00000000,
	64,	0x00000000,
	65,	0x00000000,
	66,	0x00000000,
	67,	0x00000000,
	68,	0x00000000,
	69,	0x00000000,
	70,	0x00000000,
	71,	0x00000000,
	72,	0x00000000,
	73,	0x00000000,
	74,	0x00000000,
	75,	0x00000000,
	76,	0x00000000,
	77,	0x00000000,
	78,	0x01000200,
	79,	0x02000040,
	80,	0x00400100,
	81,	0x00000200,
	82,	0x01030001,
	83,	0x01ffff0a,
	84,	0x01010101,
	85,	0x03010101,
	86,	0x01000003,
	87,	0x0000010c,
	88,	0x00010000,
	89,	0x00000000,
	90,	0x00000000,
	91,	0x00000000,
	92,	0x00000000,
	93,	0x00000000,
	94,	0x00000000,
	95,	0x00000000,
	96,	0x00000000,
	97,	0x00000000,
	98,	0x00000000,
	99,	0x00000000,
	100,	0x00000000,
	101,	0x00000000,
	102,	0x00000000,
	103,	0x00000000,
	104,	0x00000000,
	105,	0x00000000,
	106,	0x00000000,
	107,	0x00000000,
	108,	0x02020101,
	109,	0x08080404,
	110,	0x03020200,
	111,	0x01000202,
	112,	0x00000200,
	113,	0x00000000,
	114,	0x00000000,
	115,	0x00000000,
	116,	0x19000000,
	117,	0x00000028,
	118,	0x00000000,
	119,	0x00010001,
	120,	0x00010001,
	121,	0x00010001,
	122,	0x00010001,
	123,	0x00010001,
	124,	0x00000000,
	125,	0x00000000,
	126,	0x00000000,
	127,	0x00000000,
	128,	0x001c1c00,
	129,	0x1c1c0001,
	130,	0x00000001,
	131,	0x00000000,
	132,	0x00000000,
	133,	0x00011c1c,
	134,	0x00011c1c,
	135,	0x00000000,
	136,	0x00000000,
	137,	0x001c1c00,
	138,	0x1c1c0001,
	139,	0x00000001,
	140,	0x00000000,
	141,	0x00000000,
	142,	0x00011c1c,
	143,	0x00011c1c,
	144,	0x00000000,
	145,	0x00000000,
	146,	0x001c1c00,
	147,	0x1c1c0001,
	148,	0xffff0001,
	149,	0x00ffff00,
	150,	0x0000ffff,
	151,	0x00000000,
	152,	0x03030303,
	153,	0x03030303,
	156,	0x02006400,
	157,	0x02020202,
	158,	0x02020202,
	160,	0x01020202,
	161,	0x01010064,
	162,	0x01010101,
	163,	0x01010101,
	165,	0x00020101,
	166,	0x00000064,
	167,	0x00000000,
	168,	0x000a0a00,
	169,	0x0c2d0000,
	170,	0x02000200,
	171,	0x02000200,
	172,	0x00000c2d,
	173,	0x00003ce1,
	174,	0x0c2d0505,
	175,	0x02000200,
	176,	0x02000200,
	177,	0x00000c2d,
	178,	0x00003ce1,
	179,	0x02020505,
	180,	0x80000100,
	181,	0x04070303,
	182,	0x0000000a,
	183,	0x00000000,
	184,	0x00000000,
	185,	0x0010ffff,
	186,	0x16070303,
	187,	0x0000000f,
	188,	0x00000000,
	189,	0x00000000,
	190,	0x00000000,
	191,	0x00000000,
	192,	0x00000000,
	193,	0x00000000,
	194,	0x00000204,
	195,	0x00000000,
	196,	0x00000000,
	197,	0x00000000,
	198,	0x00000000,
	199,	0x00000000,
	200,	0x00000000,
	201,	0x00000000,
	202,	0x00000050,
	203,	0x00000050,
	204,	0x00000000,
	205,	0x00000040,
	206,	0x01030301,
	207,	0x00000001,
	0xffffffff
};

#ifdef _UNUSED_
unsigned int ddr3_init_tab[] = {
	14,	0x05051001,
	15,	0x000a0a05,
	36,	0x01000000,
	37,	0x10000000,
	38,	0x00100400,
	39,	0x00000400,
	40,	0x00000100,
	42,	0x00000001,
	61,	0x00010100,
	78,	0x01000200,
	79,	0x02000040,
	80,	0x00400100,
	81,	0x00000200,
	83,	0x01ffff0a,
	84,	0x01010101,
	85,	0x03010101,
	86,	0x01000003,
	87,	0x0000010c,
	88,	0x00010000,
	112,	0x00000200,
	116,	0x19000000,
	117,	0x00000028,
	119,	0x00010001,
	120,	0x00010001,
	121,	0x00010001,
	122,	0x00010001,
	123,	0x00010001,
	130,	0x00000001,
	139,	0x00000001,
	148,	0xffff0001,
	149,	0x00ffff00,
	150,	0x0000ffff,
	152,	0x03030303,
	153,	0x03030303,
	156,	0x02006400,
	157,	0x02020202,
	158,	0x02020202,
	160,	0x01020202,
	161,	0x01010064,
	162,	0x01010101,
	163,	0x01010101,
	165,	0x00020101,
	166,	0x00000064,
	168,	0x000a0a00,
	170,	0x02000200,
	171,	0x02000200,
	175,	0x02000200,
	176,	0x02000200,
	180,	0x80000100,
	181,	0x04070303,
	182,	0x0000000a,
	185,	0x0010ffff,
	187,	0x0000000f,
	194,	0x00000204,
	205,	0x00000040,
	0xffffffff
};

unsigned int ddr3_init_tab_666[] = {
	0,	0x00000600,
	3,	0x0001046b,
	4,	0x00028b0b,
	5,	0x0c050c00,
	6,	0x04040405,
	7,	0x05040e14,
	8,	0x040e1404,
	9,	0x0c040405,
	10,	0x03005b68,
	11,	0x0c040404,
	12,	0x03005b68,
	13,	0x01000004,
	16,	0x03000200,
	17,	0x00000f0f,
	18,	0x05050000,
	20,	0x00007801,
	21,	0x00780a20,
	22,	0x00050a20,
	23,	0x00000300,
	24,	0x000a0003,
	25,	0x0000000a,
	27,	0x02000000,
	28,	0x0200005a,
	29,	0x0000005a,
	30,	0x05000001,
	31,	0x00050505,
	44,	0x00022000,
	45,	0x00000046,
	46,	0x00460210,
	48,	0x02100000,
	49,	0x00000046,
	50,	0x00460210,
	52,	0x02100000,
	53,	0x00000046,
	54,	0x00460210,
	56,	0x02100000,
	57,	0x00000046,
	58,	0x00460210,
	82,	0x01010001,
	108,	0x02040108,
	109,	0x08010402,
	110,	0x02020202,
	111,	0x01000201,
	128,	0x00212100,
	129,	0x21210001,
	133,	0x00012121,
	134,	0x00012121,
	137,	0x00212100,
	138,	0x21210001,
	142,	0x00012121,
	143,	0x00012121,
	146,	0x00212100,
	147,	0x21210001,
	169,	0x0a200000,
	172,	0x00000a20,
	173,	0x000032a0,
	174,	0x0a200505,
	177,	0x00000a20,
	178,	0x000032a0,
	179,	0x02020505,
	186,	0x16070303,
	202,	0x00000004,
	203,	0x00000004,
	206,	0x01040401,
	207,	0x00000001,
	0xffffffff
};

unsigned int ddr3_init_tab_1333[] = {
	0,	0x00000600,
	1,	0x00000000,
	3,	0x00000086,
	4,	0x0000014e,
	5,	0x12071200,
	6,	0x05040407,
	7,	0x09051821,
	8,	0x05182105,
	9,	0x0c040509,
	10,	0x0400b6d0,
	11,	0x0c040504,
	12,	0x0400b6d0,
	13,	0x01000004,
	14,	0x090a1001,
	15,	0x0013130a,
	16,	0x03000200,
	17,	0x00001e1e,
	18,	0x09090000,
	19,	0x00000000,
	20,	0x0000ea01,
	21,	0x00ea1448,
	22,	0x00051448,
	23,	0x00000400,
	24,	0x00100004,
	25,	0x00000010,
	26,	0x00000000,
	27,	0x02000000,
	28,	0x020000f0,
	29,	0x000000f0,
	30,	0x07000001,
	31,	0x00070707,
	32,	0x00000000,
	35,	0x00000000,
	36,	0x01000000,
	37,	0x10000000,
	38,	0x00100400,
	39,	0x00000400,
	40,	0x00000100,
	41,	0x00000000,
	42,	0x00000001,
	43,	0x00000000,
	44,	0x000a5000,
	45,	0x00100046,
	46,	0x00460a50,
	47,	0x00000010,
	48,	0x0a500000,
	49,	0x00100046,
	50,	0x00460a50,
	51,	0x00000010,
	52,	0x0a500000,
	53,	0x00100046,
	54,	0x00460a50,
	55,	0x00000010,
	56,	0x0a500000,
	57,	0x00100046,
	58,	0x00460a50,
	59,	0x00000010,
	60,	0x00000000,
	61,	0x00010100,
	62,	0x00000000,
	63,	0x00000000,
	64,	0x00000000,
	65,	0x00000000,
	66,	0x00000000,
	67,	0x00000000,
	68,	0x00000000,
	69,	0x00000000,
	70,	0x00000000,
	71,	0x00000000,
	72,	0x00000000,
	73,	0x00000000,
	74,	0x00000000,
	75,	0x00000000,
	76,	0x00000000,
	77,	0x00000000,
	78,	0x01000200,
	79,	0x02000040,
	80,	0x00400100,
	81,	0x00000200,
	82,	0x01000001,
	83,	0x01ffff0a,
	84,	0x01010101,
	85,	0x03010101,
	86,	0x01000003,
	87,	0x0000010c,
	88,	0x00010000,
	89,	0x00000000,
	90,	0x00000000,
	91,	0x00000000,
	92,	0x00000000,
	93,	0x00000000,
	94,	0x00000000,
	95,	0x00000000,
	96,	0x00000000,
	97,	0x00000000,
	98,	0x00000000,
	99,	0x00000000,
	100,	0x00000000,
	101,	0x00000000,
	102,	0x00000000,
	103,	0x00000000,
	104,	0x00000000,
	105,	0x00000000,
	106,	0x00000000,
	107,	0x00000000,
	108,	0x02040108,
	109,	0x08010402,
	110,	0x02020202,
	111,	0x01000201,
	112,	0x00000200,
	113,	0x00000000,
	114,	0x00000000,
	115,	0x00000000,
	116,	0x19000000,
	117,	0x00000028,
	118,	0x00000000,
	119,	0x00010001,
	120,	0x00010001,
	121,	0x00010001,
	122,	0x00010001,
	123,	0x00010001,
	124,	0x00000000,
	125,	0x00000000,
	126,	0x00000000,
	127,	0x00000000,
	128,	0x00232300,
	129,	0x23230001,
	130,	0x00000001,
	131,	0x00000000,
	132,	0x00000000,
	133,	0x00012323,
	134,	0x00012323,
	135,	0x00000000,
	136,	0x00000000,
	137,	0x00232300,
	138,	0x23230001,
	139,	0x00000001,
	140,	0x00000000,
	141,	0x00000000,
	142,	0x00012323,
	143,	0x00012323,
	144,	0x00000000,
	145,	0x00000000,
	146,	0x00232300,
	147,	0x23230001,
	148,	0xffff0001,
	149,	0x00ffff00,
	150,	0x0000ffff,
	151,	0x00000000,
	152,	0x03030303,
	153,	0x03030303,
	156,	0x02006400,
	157,	0x02020202,
	158,	0x02020202,
	160,	0x01020202,
	161,	0x01010064,
	162,	0x01010101,
	163,	0x01010101,
	165,	0x00020101,
	166,	0x00000064,
	167,	0x00000000,
	168,	0x000b0b00,
	169,	0x14480000,
	170,	0x02000200,
	171,	0x02000200,
	172,	0x00001448,
	173,	0x00006568,
	174,	0x14480708,
	175,	0x02000200,
	176,	0x02000200,
	177,	0x00001448,
	178,	0x00006568,
	179,	0x02020708,
	180,	0x80000100,
	181,	0x04070303,
	182,	0x0000000a,
	183,	0x00000000,
	184,	0x00000000,
	185,	0x0010ffff,
	186,	0x1a070303,
	187,	0x0000000f,
	188,	0x00000000,
	189,	0x00000000,
	190,	0x00000000,
	191,	0x00000000,
	192,	0x00000000,
	193,	0x00000000,
	194,	0x00000204,
	195,	0x00000000,
	196,	0x00000000,
	197,	0x00000000,
	198,	0x00000000,
	199,	0x00000000,
	200,	0x00000000,
	201,	0x00000000,
	202,	0x00000007,
	203,	0x00000007,
	204,	0x00000000,
	205,	0x00000040,
	206,	0x00060601,
	207,	0x00000000,
	0xffffffff
};
#endif /* _UNUSED_ */

unsigned int ddr3_init_tab_1600[] = {
	0,	0x00000600,
	1,	0x00000000,
	3,	0x000000a0,
	4,	0x00061a80,
	5,	0x16081600,
	6,	0x06040408,
	7,	0x0b061c27,
	8,	0x061c2706,
	9,	0x0c04060b,
	10,	0x0400db60,
	11,	0x0c040604,
	12,	0x0400db60,
	13,	0x01000004,
	14,	0x0b0c1001,
	15,	0x0017170c,
	16,	0x03000200,
	17,	0x00002020,
	18,	0x0b0b0000,
	19,	0x00000000,
	20,	0x00011801,
	21,	0x01181858,
	22,	0x00051858,
	23,	0x00000500,
	24,	0x00140005,
	25,	0x00000014,
	26,	0x00000000,
	27,	0x02000000,
	28,	0x02000120,
	29,	0x00000120,
	30,	0x08000001,
	31,	0x00080808,
	32,	0x00000000,
	35,	0x00000000,
	36,	0x01000000,
	37,	0x10000000,
	38,	0x00100400,
	39,	0x00000400,
	40,	0x00000100,
	41,	0x00000000,
	42,	0x00000001,
	43,	0x00000000,
	44,	0x000c7000,
	45,	0x00180046,
	46,	0x00460c70,
	47,	0x00000018,
	48,	0x0c700000,
	49,	0x00180046,
	50,	0x00460c70,
	51,	0x00000018,
	52,	0x0c700000,
	53,	0x00180046,
	54,	0x00460c70,
	55,	0x00000018,
	56,	0x0c700000,
	57,	0x00180046,
	58,	0x00460c70,
	59,	0x00000018,
	60,	0x00000000,
	61,	0x00010100,
	62,	0x00000000,
	63,	0x00000000,
	64,	0x00000000,
	65,	0x00000000,
	66,	0x00000000,
	67,	0x00000000,
	68,	0x00000000,
	69,	0x00000000,
	70,	0x00000000,
	71,	0x00000000,
	72,	0x00000000,
	73,	0x00000000,
	74,	0x00000000,
	75,	0x00000000,
	76,	0x00000000,
	77,	0x00000000,
	78,	0x01000200,
	79,	0x02000040,
	80,	0x00400100,
	81,	0x00000200,
	82,	0x01000001,
	83,	0x01ffff0a,
	84,	0x01010101,
	85,	0x03010101,
	86,	0x01000003,
	87,	0x0000010c,
	88,	0x00010000,
	89,	0x00000000,
	90,	0x00000000,
	91,	0x00000000,
	92,	0x00000000,
	93,	0x00000000,
	94,	0x00000000,
	95,	0x00000000,
	96,	0x00000000,
	97,	0x00000000,
	98,	0x00000000,
	99,	0x00000000,
	100,	0x00000000,
	101,	0x00000000,
	102,	0x00000000,
	103,	0x00000000,
	104,	0x00000000,
	105,	0x00000000,
	106,	0x00000000,
	107,	0x00000000,
	108,	0x02040108,
	109,	0x08010402,
	110,	0x02020202,
	111,	0x01000201,
	112,	0x00000200,
	113,	0x00000000,
	114,	0x00000000,
	115,	0x00000000,
	116,	0x19000000,
	117,	0x00000028,
	118,	0x00000000,
	119,	0x00010001,
	120,	0x00010001,
	121,	0x00010001,
	122,	0x00010001,
	123,	0x00010001,
	124,	0x00000000,
	125,	0x00000000,
	126,	0x00000000,
	127,	0x00000000,
	128,	0x00232300,
	129,	0x23230001,
	130,	0x00000001,
	131,	0x00000000,
	132,	0x00000000,
	133,	0x00012323,
	134,	0x00012323,
	135,	0x00000000,
	136,	0x00000000,
	137,	0x00232300,
	138,	0x23230001,
	139,	0x00000001,
	140,	0x00000000,
	141,	0x00000000,
	142,	0x00012323,
	143,	0x00012323,
	144,	0x00000000,
	145,	0x00000000,
	146,	0x00232300,
	147,	0x23230001,
	148,	0xffff0001,
	149,	0x00ffff00,
	150,	0x0000ffff,
	151,	0x00000000,
	152,	0x03030303,
	153,	0x03030303,
	156,	0x02006400,
	157,	0x02020202,
	158,	0x02020202,
	160,	0x01020202,
	161,	0x01010064,
	162,	0x01010101,
	163,	0x01010101,
	165,	0x00020101,
	166,	0x00000064,
	167,	0x00000000,
	168,	0x000b0b00,
	169,	0x18580000,
	170,	0x02000200,
	171,	0x02000200,
	172,	0x00001858,
	173,	0x000079b8,
	174,	0x1858080a,
	175,	0x02000200,
	176,	0x02000200,
	177,	0x00001858,
	178,	0x000079b8,
	179,	0x0202080a,
	180,	0x80000100,
	181,	0x04070303,
	182,	0x0000000a,
	183,	0x00000000,
	184,	0x00000000,
	185,	0x0010ffff,
	186,	0x1c070303,
	187,	0x0000000f,
	188,	0x00000000,
	189,	0x00000000,
	190,	0x00000000,
	191,	0x00000000,
	192,	0x00000000,
	193,	0x00000000,
	194,	0x00000204,
	195,	0x00000000,
	196,	0x00000000,
	197,	0x00000000,
	198,	0x00000000,
	199,	0x00000000,
	200,	0x00000000,
	201,	0x00000000,
	202,	0x00000008,
	203,	0x00000008,
	204,	0x00000000,
	205,	0x00000040,
	206,	0x00070701,
	207,	0x00000000,
	0xffffffff
};


static void
ddr_regs_init(si_t *sih, ddrcregs_t *ddr, unsigned int ddr_table[])
{
	osl_t *osh;
	uint32 reg_num, reg_val;
	int idx = 0;

	osh = si_osh(sih);
	while ((reg_num = ddr_table[idx++]) != DDR_TABLE_END) {
		reg_val = ddr_table[idx++];
		W_REG(osh, &ddr->control[reg_num], reg_val);
	}
}

int rewrite_mode_registers(void *sih)
{
	osl_t *osh;
	ddrcregs_t *ddr;
	int nRet = 0;
	int j = 100;
	uint32 val;

	osh = si_osh((si_t *)sih);
	ddr = (ddrcregs_t *)si_setcore((si_t *)sih, NS_DDR23_CORE_ID, 0);
	if (!ddr) {
		nRet = 1;
		goto out;
	}

	val = R_REG(osh, &ddr->control[89]);
	val &= ~(1 << 18);
	W_REG(osh, &ddr->control[89], val);

	/* Set mode register for MR0, MR1, MR2 and MR3 write for all chip selects */
	val = (1 << 17) | (1 << 24) | (1 << 25);
	W_REG(osh, &ddr->control[43], val);

	/* Trigger Mode Register Write(MRW) sequence */
	val |= (1 << 25);
	W_REG(osh, &ddr->control[43], val);

	do {
		if (R_REG(osh, &ddr->control[89]) & (1 << 18)) {
			break;
		}
		--j;
	} while (j);

	if (j == 0 && (R_REG(osh, &ddr->control[89]) & (1 << 18)) == 0) {
		printf("Error: DRAM mode registers write failed\n");
		nRet = 1;
	};
out:
	return nRet;
}

static int
_check_cmd_done(void)
{
	unsigned int k;
	uint32 st;

	for (k = 0; k < OTPP_TRIES; k++) {
		st = R_REG(NULL, (uint32 *)DMU_OTP_CPU_STS);
		if (st & OTPCPU_STS_CMD_DONE_MASK) {
		    break;
		}
	}
	if (k >= OTPP_TRIES) {
		printf("%s: %d _check_cmd_done: %08x\n", __FUNCTION__, __LINE__, k);
		return -1;
	}
	return 0;
}


static int
_issue_read(unsigned int row, uint32 *val)
{
	uint32 ctrl1, ctrl0;
	uint32 start;
	uint32 cmd;
	uint32 fuse;
	uint32 cof;
	uint32 prog_en;
	uint32 mode;
	int rv;


	W_REG(NULL, (uint32 *)DMU_OTP_CPU_ADDR, row);

	start = (1 << OTPCPU_CTRL1_START_SHIFT) & OTPCPU_CTRL1_START_MASK;
	cmd = (OTPCPU_CMD_READ << OTPCPU_CTRL1_CMD_SHIFT) & OTPCPU_CTRL1_CMD_MASK;
	fuse = (1 << OTPCPU_CTRL1_2XFUSE_SHIFT) & OTPCPU_CTRL1_2XFUSE_MASK;
	cof = (2 << OTPCPU_CTRL1_COF_SHIFT) & OTPCPU_CTRL1_COF_MASK;
	prog_en = (0 << OTPCPU_CTRL1_PROG_EN_SHIFT) & OTPCPU_CTRL1_PROG_EN_MASK;
	mode = (1 << OTPCPU_CTRL1_ACCESS_MODE_SHIFT) & OTPCPU_CTRL1_ACCESS_MODE_MASK;

	ctrl1 = cmd;
	ctrl1 |= fuse;
	ctrl1 |= cof;
	ctrl1 |= prog_en;
	ctrl1 |= mode;

	ctrl1 |= start;
	W_REG(NULL, (uint32 *)DMU_OTP_CPU_CTRL1, ctrl1);
	ctrl0 = DEFAULT_OTPCPU_CTRL0;
	W_REG(NULL, (uint32 *)DMU_OTP_CPU_CTRL0, ctrl0);

	/* Check if cmd_done bit is asserted */
	rv = _check_cmd_done();
	if (rv) {
		printf("%s: %d _check_cmd_done: %08x\n", __FUNCTION__, __LINE__, rv);
	    return -1;
	}

	*val = R_REG(NULL, (uint32 *)DMU_OTP_CPU_READ_DATA);

	return 0;
}


static int
_issue_prog_dis(void)
{
	uint32 ctrl1, ctrl0;
	uint32 start;
	uint32 cmd;
	uint32 fuse;
	uint32 cof;
	uint32 prog_en;
	uint32 mode;
	int rv;

	W_REG(NULL, (uint32 *)DMU_OTP_CPU_ADDR, 0);

	start = (1 << OTPCPU_CTRL1_START_SHIFT) & OTPCPU_CTRL1_START_MASK;
	cmd = (OTPCPU_CMD_PROG_DIS << OTPCPU_CTRL1_CMD_SHIFT) & OTPCPU_CTRL1_CMD_MASK;
	fuse = (1 << OTPCPU_CTRL1_2XFUSE_SHIFT) & OTPCPU_CTRL1_2XFUSE_MASK;
	cof = (1 << OTPCPU_CTRL1_COF_SHIFT) & OTPCPU_CTRL1_COF_MASK;
	prog_en = (1 << OTPCPU_CTRL1_PROG_EN_SHIFT) & OTPCPU_CTRL1_PROG_EN_MASK;
	mode = (2 << OTPCPU_CTRL1_ACCESS_MODE_SHIFT) & OTPCPU_CTRL1_ACCESS_MODE_MASK;

	ctrl1 = cmd;
	ctrl1 |= fuse;
	ctrl1 |= cof;
	ctrl1 |= prog_en;
	ctrl1 |= mode;

	ctrl1 |= start;
	W_REG(NULL, (uint32 *)DMU_OTP_CPU_CTRL1, ctrl1);
	ctrl0 = DEFAULT_OTPCPU_CTRL0;
	W_REG(NULL, (uint32 *)DMU_OTP_CPU_CTRL0, ctrl0);

	/* Check if cmd_done bit is asserted */
	rv = _check_cmd_done();
	if (rv) {
		printf("%s: %d _check_cmd_done: %08x\n", __FUNCTION__, __LINE__, rv);
		return -1;
	}

	ctrl1 &= ~start;
	W_REG(NULL, (uint32 *)DMU_OTP_CPU_CTRL1, ctrl1);
	W_REG(NULL, (uint32 *)DMU_OTP_CPU_CTRL0, ctrl0);

	return 0;
}

int
bcm5301x_otp_read_dword(unsigned int wn, uint32 *data)
{
	uint32 cpu_cfg;
	int rv;

	/* Check if CPU mode is enabled */
	cpu_cfg = R_REG(NULL, (uint32 *)DMU_OTP_CPU_CONFIG);
	if (!(cpu_cfg & OTPCPU_CFG_CPU_MODE_MASK)) {
		cpu_cfg |= ((1 << OTPCPU_CFG_CPU_MODE_SHIFT) & OTPCPU_CFG_CPU_MODE_MASK);
		W_REG(NULL, (uint32 *)DMU_OTP_CPU_CONFIG, cpu_cfg);
	}
	cpu_cfg = R_REG(NULL, (uint32 *)DMU_OTP_CPU_CONFIG);

	/* Issue ProgDisable command */
	rv = _issue_prog_dis();
	if (rv) {
		printf("%s: %d _issue_prog_dis failed: %d\n", __FUNCTION__, __LINE__, rv);
		return -1;
	}

	/* Issue ReadWord command */
	rv = _issue_read(wn, data);
	if (rv) {
		printf("%s: %d _issue_read failed: %d\n", __FUNCTION__, __LINE__, rv);
		return -1;
	}

	return 0;
}

int
Program_Digital_Core_Power_Voltage(si_t *sih)
{
#define ROW_NUMBER 0x8
#define MDC_DIV 0x8
#define DIGITAL_POWER_CORE_VOLTAGE_1_POINT_05V	0x520E003C
#define DIGITAL_POWER_CORE_VOLTAGE_1V		0x520E0020
#define DIGITAL_POWER_CORE_VOLTAGE_POINT_975V	0x520E0018
#define DIGITAL_POWER_CORE_VOLTAGE_POINT_9625V	0x520E0014
#define DIGITAL_POWER_CORE_VOLTAGE_POINT_95V	0x520E0010
#define DIGITAL_POWER_CORE_VOLTAGE_POINT_9375V	0x520E000C
#define DIGITAL_POWER_CORE_VOLTAGE_POINT_925V	0x520E0008
#define DIGITAL_POWER_CORE_VOLTAGE_POINT_9125V	0x520E0004
#define DIGITAL_POWER_CORE_VOLTAGE_POINT_9V	0x520E0000
#define AVS_CODE_RS 17
#define AVS_CODE_MASK 7

	osl_t *osh;
	chipcommonbregs_t *chipcb;
	uint32 avs_code = 0;
	int retval = 0;
	char *vol_str = NULL;

	osh = si_osh(sih);
	chipcb = (chipcommonbregs_t *)si_setcore(sih, NS_CCB_CORE_ID, 0);
	if (chipcb == NULL) {
		retval = -1;
		printf("%s: %d chipcb null %d\n", __FUNCTION__, __LINE__, retval);
		return retval;
	}

	if (CHIPID(sih->chip) == BCM4707_CHIP_ID || CHIPID(sih->chip) == BCM47094_CHIP_ID) {
		if (sih->chippkg == BCM4708_PKG_ID) {
			/* access internal regulator phy by setting the MDC/MDIO
			 * bus frequency to 125/8
			 */
			W_REG(osh, &chipcb->pcu_mdio_mgt, MDC_DIV);
			udelay(500);
			/* this is 0.9 V */
			W_REG(osh, &chipcb->pcu_mdio_cmd, DIGITAL_POWER_CORE_VOLTAGE_POINT_9375V);
			printf("Digital core power voltage set to 0.9375V\n");
			return retval;
		} else if ((CHIPID(sih->chip) == BCM47094_CHIP_ID) &&
			(sih->chippkg == BCM4709_PKG_ID)) {
			W_REG(osh, &chipcb->pcu_mdio_mgt, MDC_DIV);
			udelay(500);
			/* this is 1.05 V */
			W_REG(osh, &chipcb->pcu_mdio_cmd, DIGITAL_POWER_CORE_VOLTAGE_1_POINT_05V);
			printf("Digital core power voltage set to 1.05V\n");
			return retval;
		}
	}

	retval = bcm5301x_otp_read_dword(ROW_NUMBER, &avs_code);
	if (retval != 0) {
		printf("%s: %d failed bcm5301x_otp_read_dword: %d\n",
			__FUNCTION__, __LINE__, retval);
		return retval;
	}


	/* bits 17 - 19 is the avs code */
	avs_code = (avs_code >> AVS_CODE_RS) & AVS_CODE_MASK;

	/* access internal regulator phy by setting the MDC/MDIO bus frequency to 125/8 */
	W_REG(osh, &chipcb->pcu_mdio_mgt, MDC_DIV);
	udelay(500);

	switch (avs_code) {
	case 0:
		/* this is 1 V */
		W_REG(osh, &chipcb->pcu_mdio_cmd, DIGITAL_POWER_CORE_VOLTAGE_1V);
		vol_str = "1.0";
		break;
	case 1:
		/* this is 0.975 V */
		W_REG(osh, &chipcb->pcu_mdio_cmd, DIGITAL_POWER_CORE_VOLTAGE_POINT_975V);
		vol_str = "0.975";
		break;
	case 2:
		/* this is 0.9625 V */
		W_REG(osh, &chipcb->pcu_mdio_cmd, DIGITAL_POWER_CORE_VOLTAGE_POINT_9625V);
		vol_str = "0.9625";
		break;
	case 3:
		/* this is 0.95 V */
		W_REG(osh, &chipcb->pcu_mdio_cmd, DIGITAL_POWER_CORE_VOLTAGE_POINT_95V);
		vol_str = "0.95";
		break;
	case 4:
		/* this is 0.9375 V */
		W_REG(osh, &chipcb->pcu_mdio_cmd, DIGITAL_POWER_CORE_VOLTAGE_POINT_9375V);
		vol_str = "0.9375";
		break;
	case 5:
		/* this is 0.925 V */
		W_REG(osh, &chipcb->pcu_mdio_cmd, DIGITAL_POWER_CORE_VOLTAGE_POINT_925V);
		vol_str = "0.925";
		break;
	case 6:
		/* this is 0.9125 V */
		W_REG(osh, &chipcb->pcu_mdio_cmd, DIGITAL_POWER_CORE_VOLTAGE_POINT_9125V);
		vol_str = "0.9125";
		break;
	case 7:
		/* this is 0.9 V */
		W_REG(osh, &chipcb->pcu_mdio_cmd, DIGITAL_POWER_CORE_VOLTAGE_POINT_9V);
		vol_str = "0.9";
		break;
	default:
		printf("%s: %d unrecognized avs_code %d\n", __FUNCTION__, __LINE__, avs_code);
		break;
	}

	if (vol_str)
		printf("Digital core power voltage set to %sV\n", vol_str);

	return retval;
}

void
c_ddr_init(unsigned long ra)
{
	si_t *sih;
	osl_t *osh;
	void *regs;
	int ddrtype_ddr3 = 0;
	int bootdev;
	struct nvram_header *nvh = NULL;
	uintptr flbase;
	uint32 off, sdram_config, sdram_ncdl;
	uint32 config_refresh, sdram_refresh;
	ddrcregs_t *ddr;
	chipcommonbregs_t *chipcb;
	uint32 val, val1;
	int i;
	uint32 ddrclock = DDR_DEFAULT_CLOCK, clkval;
	uint32 params, connect, ovride, status;
	uint32 wire_dly[4] = {0};


	/* Basic initialization */
	sih = (si_t *)osl_init();
	osh = si_osh(sih);
#ifdef BCMDBG
	printf("\n==================== CFE Boot Loader ====================\n");
#endif

	Program_Digital_Core_Power_Voltage(sih);

	regs = (void *)si_setcore(sih, NS_DDR23_CORE_ID, 0);
	if (regs) {
		ddrtype_ddr3 = ((si_core_sflags(sih, 0, 0) & DDR_TYPE_MASK) == DDR_STAT_DDR3);
	}
	if (ddrtype_ddr3) {
		chipcb = (chipcommonbregs_t *)si_setcore(sih, NS_CCB_CORE_ID, 0);
		if (chipcb) {
			/* Configure DDR voltage to 1.5V */
			val = R_REG(osh, &chipcb->pcu_1v8_1v5_vregcntl);
			val = (val & ~PCU_VOLTAGE_SELECT_MASK) | PCU_VOLTAGE_SELECT_1V5;
			W_REG(osh, &chipcb->pcu_1v8_1v5_vregcntl, val);

			/* Enable LDO voltage output */
			val = PCU_AOPC_PWRDIS_SEL;
			W_REG(osh, &chipcb->pcu_aopc_control, val);
			val |= PCU_AOPC_PWRDIS_LATCHEN;
			W_REG(osh, &chipcb->pcu_aopc_control, val);
			val1 = R_REG(osh, &chipcb->pcu_status);
			val1 &= PCU_PWR_EN_STRAPS_MASK;
			val |= (~(val1 << 1) & PCU_AOPC_PWRDIS_MASK);
			val &= ~PCU_AOPC_PWRDIS_DDR;
			W_REG(osh, &chipcb->pcu_aopc_control, val);
		}
	}

	/* Set DDR clock */
	ddr = (ddrcregs_t *)si_setcore(sih, NS_DDR23_CORE_ID, 0);
	if (!ddr)
		goto out;
	val = R_REG(osh, (uint32 *)DDR_S1_IDM_RESET_CONTROL);
	if ((val & AIRC_RESET) == 0)
		val = R_REG(osh, &ddr->control[0]);
	else
		val = 0;
	if (val & DDRC00_START) {
		clkval = *((uint32 *)(0x1000 + BISZ_OFFSET - 4));
		if (clkval) {
			val = AIRC_RESET;
			W_REG(osh, (uint32 *)DDR_S1_IDM_RESET_CONTROL, val);
			W_REG(osh, (uint32 *)DDR_S2_IDM_RESET_CONTROL, val);
			ddrclock = clkval;
			si_mem_setclock(sih, ddrclock);
		}
	} else {
		/* DDR PHY doesn't support 333MHz for DDR3, so set the clock to 400 by default. */
		ddrclock = DDR3_MIN_CLOCK;
		si_mem_setclock(sih, ddrclock);
	}

	/* Find NVRAM for the sdram_config variable */
	bootdev = soc_boot_dev((void *)sih);
#ifdef NFLASH_SUPPORT
	if (bootdev == SOC_BOOTDEV_NANDFLASH) {
		flbase = SI_NS_NANDFLASH;
		goto embedded_nv;
	}
	else
#endif /* NFLASH_SUPPORT */
	{
		/* bootdev == SOC_BOOTDEV_SFLASH */
		flbase = SI_NS_NORFLASH;
		off = FLASH_MIN;
		while (off <= SI_NS_FLASH_WINDOW) {
			nvh = (struct nvram_header *)(flbase + off - MAX_NVRAM_SPACE);
			if (R_REG(osh, &nvh->magic) == NVRAM_MAGIC)
				break;
			off += DEF_NVRAM_SPACE;
			nvh = NULL;
		};
	}
#ifdef NFLASH_SUPPORT
embedded_nv:
#endif
	if (nvh == NULL) {
		nvh = (struct nvram_header *)(flbase + 1024);
		if (R_REG(osh, &nvh->magic) != NVRAM_MAGIC) {
			goto out;
		}
	}
	config_refresh = R_REG(osh, &nvh->config_refresh);
	sdram_config = config_refresh & 0xffff;
	sdram_refresh = (config_refresh >> 16) & 0xffff;
	sdram_ncdl = R_REG(osh, &nvh->config_ncdl);
#ifdef BCMDBG
	printf("%s: sdram_config=0x%04x sdram_ncdl=0x%08x\n",
		__FUNCTION__, sdram_config, sdram_ncdl);
#endif

	/* Take DDR23 core out of reset */
	W_REG(osh, (uint32 *)DDR_S1_IDM_RESET_CONTROL, 0);
	W_REG(osh, (uint32 *)DDR_S2_IDM_RESET_CONTROL, 0);
	/* Set the ddr_ck to 400 MHz, 2x memc clock */
	val = R_REG(osh, (uint32 *)DDR_S1_IDM_IO_CONTROL_DIRECT);
	val &= ~(0xfff << 16);
	val |= (0x190 << 16);
	W_REG(osh, (uint32 *)DDR_S1_IDM_IO_CONTROL_DIRECT, val);

	/* Wait for DDR PHY up */
	for (i = 0; i < 0x19000; i++) {
		val = R_REG(osh, &ddr->phy_control_rev);
		if (val != 0)
			break; /* DDR PHY is up */
	}
	if (i == 0x19000) {
#ifdef BCMDBG
		printf("Recalibrating DDR PHY...\n");
#endif
		si_watchdog(sih, 1);
		while (1);
	}

	/* Change PLL divider values inside PHY */
	W_REG(osh, &ddr->phy_control_plldividers, 0x00000c10);	/* high sku ? */
	if (ddrclock != DDR_DEFAULT_CLOCK) {
		/* SHAMOO related DDR PHY init change */
#ifdef CONFIG_DDR_LONG_PREAMBLE
		params = DDR40_PHY_PARAM_USE_VTT |
			DDR40_PHY_PARAM_ODT_LATE |
			DDR40_PHY_PARAM_ADDR_CTL_ADJUST_0 |
			DDR40_PHY_PARAM_ADDR_CTL_ADJUST_1 |
			DDR40_PHY_PARAM_MAX_ZQ |
			DDR40_PHY_PARAM_LONG_PREAMBLE;
#else
		params = DDR40_PHY_PARAM_USE_VTT |
			DDR40_PHY_PARAM_ODT_LATE |
			DDR40_PHY_PARAM_ADDR_CTL_ADJUST_0 |
			DDR40_PHY_PARAM_ADDR_CTL_ADJUST_1 |
			DDR40_PHY_PARAM_MAX_ZQ;
#endif /* CONFIG_DDR_LONG_PREAMBLE */
		if (ddrtype_ddr3) {
			/* DDR3, 1.5v */
			params |= DDR40_PHY_PARAM_VDDO_VOLT_0;
		}
		else {
			/* DDR2, 1.8v */
			params |= DDR40_PHY_PARAM_VDDO_VOLT_1;
		}
		connect = 0x01CF7FFF;
		ovride = 0x00077FFF;
		status = ddr40_phy_init(ddrclock, params, ddrtype_ddr3,
			wire_dly, connect, ovride, (uint32_t)DDR_PHY_CONTROL_REGS_REVISION);
		if (status != DDR40_PHY_RETURN_OK) {
			printf("Error: ddr40_phy_init failed with error 0x%x\n", status);
			return;
		}
	} else {
		/* Set LDO output voltage control to 1.00 * VDDC, and enable PLL */
		val = (1 << 4);
		W_REG(osh, &ddr->phy_control_pllconfig, val);

		/* Wait for PLL locked */
		for (i = 0; i < 0x1400; i++) {
			val = R_REG(osh, &ddr->phy_control_pllstatus);
			if (val & 0x1)
				break;
		}
		if (i == 0x1400) {
			printf("DDR PHY PLL lock failed\n");
			goto out;
		}

		W_REG(osh, &ddr->phy_ln0_rddata_dly, 3);	/* high sku? */

		/* Write 2 if ddr2, 3 if ddr3 */
		/* Set preamble mode according to DDR type,
		 * and length of write preamble to 1.5 DQs, 0.75 DQ
		 */
		if (ddrtype_ddr3)
			val = 1;
		else
			val = 0;
#ifdef CONFIG_DDR_LONG_PREAMBLE
		val |= 2;
#endif
		W_REG(osh, &ddr->phy_ln0_wr_premb_mode, val);

		/* Initiate a PVT calibration cycle */
		W_REG(osh, &ddr->phy_control_zq_pvt_compctl, (1 << 20));

		/* Initiate auto calibration and apply the results to VDLs */
		W_REG(osh, &ddr->phy_control_vdl_calibrate, 0x08000101);	/* high sku? */

		/* Wait for Calibration lock done */
		for (i = 0; i < 0x1400; i++) {
			val = R_REG(osh, &ddr->phy_control_vdl_calibsts);
			if (val & 0x1)
				break;
		}
		if (i == 0x1400) {
			printf("DDR PHY auto calibration timed out\n");
			goto out;
		}

		if (!(val & 0x2)) {
#ifdef BCMDBG
			printf("Need re-calibrating...\n");
#endif
			si_watchdog(sih, 1);
			while (1);
		}
	}

	if (ddrtype_ddr3) {
		ddr_regs_init(sih, ddr, ddr3_init_tab_1600);
	} else {
		ddr_regs_init(sih, ddr, ddr2_init_tab_400);
	}
	if (ddrclock == DDR_DEFAULT_CLOCK) {
		/* High SKU, DDR-1600 */
		/* Auto initialization fails at DDRCLK 800 MHz, manually override */
		W_REG(osh, &ddr->phy_ln0_vdl_ovride_byte1_r_n, 0x00010120);
		W_REG(osh, &ddr->phy_ln0_vdl_ovride_byte0_bit_rd_en, 0x0001000d);
		W_REG(osh, &ddr->phy_ln0_vdl_ovride_byte1_bit_rd_en, 0x00010020);
	}

	/* Set Tref */
	if (sdram_refresh == 0)
		sdram_refresh = (0x1858 * ddrclock) / 800;

	val = R_REG(osh, &ddr->control[21]);
	val &= ~0x3fff;
	val |= sdram_refresh & 0x3fff;
	W_REG(osh, &ddr->control[21], val);

	val = R_REG(osh, &ddr->control[22]);
	val &= ~0x3fff;
	val |= sdram_refresh & 0x3fff;
	W_REG(osh, &ddr->control[22], val);

	if (sdram_config) {
		uint32 cas, wrlat;

		/* Check reduce mode */
		val = R_REG(osh, &ddr->control[87]);
		if (sdram_config & 0x80)
			val |= (1 << 8);
		W_REG(osh, &ddr->control[87], val);

		val = R_REG(osh, &ddr->control[82]);
		/* Check 8-bank mode */
		if (sdram_config & 0x40)
			val &= ~(3 << 8);
		else {
			/* default 4 banks */
			val &= ~(3 << 8);
			val |= (1 << 8);
		}
		/* Defaul row_diff=0; Check column diff */
		val &= ~(0x707 << 16);
		val |= ((sdram_config & 0x700) << 16);
		W_REG(osh, &ddr->control[82], val);

		/* Now do CAS latency settings */
		cas = sdram_config & 0x1f;
		if (cas > 5)
			wrlat = cas - (cas-4)/2;
		else
			wrlat = cas - 1;

		val = R_REG(osh, &ddr->control[5]);
		val &= ~0x3f1f3f00;
		val = val | (cas << 9) | (cas << 25) | (wrlat << 16);
		W_REG(osh, &ddr->control[5], val);

		val = R_REG(osh, &ddr->control[6]);
		val &= ~0x1f;
		val |= wrlat;
		W_REG(osh, &ddr->control[6], val);

		val = R_REG(osh, &ddr->control[174]);
		val &= ~0x1f3f;
		val |= (wrlat << 8) | (cas-1);
		W_REG(osh, &ddr->control[174], val);

		val = R_REG(osh, &ddr->control[186]);
		val &= ~0xff000000;
		val |= ((cas+17) << 24);
		W_REG(osh, &ddr->control[186], val);

		if (ddrtype_ddr3) {
			val = R_REG(osh, &ddr->control[44]);
			val &= ~0xf000;
			val |= ((cas-4) << 12);
			W_REG(osh, &ddr->control[44], val);

			val = R_REG(osh, &ddr->control[45]);
			val &= ~0x00380000;
			if (cas > 9)
				val |= ((cas-8) << 19);
			else if (cas > 7)
				val |= ((cas-7) << 19);
			else if (cas >= 6)
				val |= ((cas-6) << 19);
			W_REG(osh, &ddr->control[45], val);

			val = R_REG(osh, &ddr->control[206]);
			val &= ~0x00001f00;
			val |= ((wrlat-1) << 8);
			W_REG(osh, &ddr->control[206], val);
		} else {
			val = R_REG(osh, &ddr->control[44]);
			val &= ~0xf000;
			val |= (cas << 12);
			W_REG(osh, &ddr->control[44], val);
		}
	}

	/* Start the DDR */
	val = R_REG(osh, &ddr->control[0]);
	val |= DDRC00_START;
	W_REG(osh, &ddr->control[0], val);
	while (!(R_REG(osh, &ddr->control[89]) & DDR_INT_INIT_DONE));
#ifdef BCMDBG
	printf("%s: DDR PLL locked\n", __FUNCTION__);
#endif

	W_REG(osh, &ddr->phy_ln0_rddata_dly, 3);	/* high sku? */

	/* Run the SHMOO */
	if (ddrtype_ddr3 && ddrclock > DDR3_MIN_CLOCK) {
		status = do_shmoo((void *)sih, DDR_PHY_CONTROL_REGS_REVISION, 0,
			((26 << 16) | (16 << 8) | DO_ALL_SHMOO), 0x1000000);
		if (status != SHMOO_NO_ERROR) {
			printf("Error: do_shmoo failed with error 0x%x\n", status);
#ifdef BCMDBG
			goto out;
#else
			hnd_cpu_reset(sih);
#endif
		}
	}

out:
	asm("mov pc,%0" : : "r"(ra) : "cc");
}
