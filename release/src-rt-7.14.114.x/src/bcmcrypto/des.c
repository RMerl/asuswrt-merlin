/*
 * DES functions
 * Copied from des-ka9q-1.0-portable, a public domain DES implementation,
 * and diddled with only enough to compile without warnings and link
 * with our driver.
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: des.c 467210 2014-04-02 20:16:21Z $
 */

/* Portable C code to create DES key schedules from user-provided keys
 * This doesn't have to be fast unless you're cracking keys or UNIX
 * passwords
 */

#include <typedefs.h>

#ifdef BCMDRIVER
#include <osl.h>
#else
#include <string.h>
#endif	/* BCMDRIVER */

#include <bcmcrypto/des.h>

const unsigned int BCMROMDATA(Spbox)[8][64] = {
	{
		0x01010400, 0x00000000, 0x00010000, 0x01010404,
		0x01010004, 0x00010404, 0x00000004, 0x00010000,
		0x00000400, 0x01010400, 0x01010404, 0x00000400,
		0x01000404, 0x01010004, 0x01000000, 0x00000004,
		0x00000404, 0x01000400, 0x01000400, 0x00010400,
		0x00010400, 0x01010000, 0x01010000, 0x01000404,
		0x00010004, 0x01000004, 0x01000004, 0x00010004,
		0x00000000, 0x00000404, 0x00010404, 0x01000000,
		0x00010000, 0x01010404, 0x00000004, 0x01010000,
		0x01010400, 0x01000000, 0x01000000, 0x00000400,
		0x01010004, 0x00010000, 0x00010400, 0x01000004,
		0x00000400, 0x00000004, 0x01000404, 0x00010404,
		0x01010404, 0x00010004, 0x01010000, 0x01000404,
		0x01000004, 0x00000404, 0x00010404, 0x01010400,
		0x00000404, 0x01000400, 0x01000400, 0x00000000,
		0x00010004, 0x00010400, 0x00000000, 0x01010004
	},
	{
		0x80108020, 0x80008000, 0x00008000, 0x00108020,
		0x00100000, 0x00000020, 0x80100020, 0x80008020,
		0x80000020, 0x80108020, 0x80108000, 0x80000000,
		0x80008000, 0x00100000, 0x00000020, 0x80100020,
		0x00108000, 0x00100020, 0x80008020, 0x00000000,
		0x80000000, 0x00008000, 0x00108020, 0x80100000,
		0x00100020, 0x80000020, 0x00000000, 0x00108000,
		0x00008020, 0x80108000, 0x80100000, 0x00008020,
		0x00000000, 0x00108020, 0x80100020, 0x00100000,
		0x80008020, 0x80100000, 0x80108000, 0x00008000,
		0x80100000, 0x80008000, 0x00000020, 0x80108020,
		0x00108020, 0x00000020, 0x00008000, 0x80000000,
		0x00008020, 0x80108000, 0x00100000, 0x80000020,
		0x00100020, 0x80008020, 0x80000020, 0x00100020,
		0x00108000, 0x00000000, 0x80008000, 0x00008020,
		0x80000000, 0x80100020, 0x80108020, 0x00108000
	},
	{
		0x00000208, 0x08020200, 0x00000000, 0x08020008,
		0x08000200, 0x00000000, 0x00020208, 0x08000200,
		0x00020008, 0x08000008, 0x08000008, 0x00020000,
		0x08020208, 0x00020008, 0x08020000, 0x00000208,
		0x08000000, 0x00000008, 0x08020200, 0x00000200,
		0x00020200, 0x08020000, 0x08020008, 0x00020208,
		0x08000208, 0x00020200, 0x00020000, 0x08000208,
		0x00000008, 0x08020208, 0x00000200, 0x08000000,
		0x08020200, 0x08000000, 0x00020008, 0x00000208,
		0x00020000, 0x08020200, 0x08000200, 0x00000000,
		0x00000200, 0x00020008, 0x08020208, 0x08000200,
		0x08000008, 0x00000200, 0x00000000, 0x08020008,
		0x08000208, 0x00020000, 0x08000000, 0x08020208,
		0x00000008, 0x00020208, 0x00020200, 0x08000008,
		0x08020000, 0x08000208, 0x00000208, 0x08020000,
		0x00020208, 0x00000008, 0x08020008, 0x00020200
	},
	{
		0x00802001, 0x00002081, 0x00002081, 0x00000080,
		0x00802080, 0x00800081, 0x00800001, 0x00002001,
		0x00000000, 0x00802000, 0x00802000, 0x00802081,
		0x00000081, 0x00000000, 0x00800080, 0x00800001,
		0x00000001, 0x00002000, 0x00800000, 0x00802001,
		0x00000080, 0x00800000, 0x00002001, 0x00002080,
		0x00800081, 0x00000001, 0x00002080, 0x00800080,
		0x00002000, 0x00802080, 0x00802081, 0x00000081,
		0x00800080, 0x00800001, 0x00802000, 0x00802081,
		0x00000081, 0x00000000, 0x00000000, 0x00802000,
		0x00002080, 0x00800080, 0x00800081, 0x00000001,
		0x00802001, 0x00002081, 0x00002081, 0x00000080,
		0x00802081, 0x00000081, 0x00000001, 0x00002000,
		0x00800001, 0x00002001, 0x00802080, 0x00800081,
		0x00002001, 0x00002080, 0x00800000, 0x00802001,
		0x00000080, 0x00800000, 0x00002000, 0x00802080
	},
	{
		0x00000100, 0x02080100, 0x02080000, 0x42000100,
		0x00080000, 0x00000100, 0x40000000, 0x02080000,
		0x40080100, 0x00080000, 0x02000100, 0x40080100,
		0x42000100, 0x42080000, 0x00080100, 0x40000000,
		0x02000000, 0x40080000, 0x40080000, 0x00000000,
		0x40000100, 0x42080100, 0x42080100, 0x02000100,
		0x42080000, 0x40000100, 0x00000000, 0x42000000,
		0x02080100, 0x02000000, 0x42000000, 0x00080100,
		0x00080000, 0x42000100, 0x00000100, 0x02000000,
		0x40000000, 0x02080000, 0x42000100, 0x40080100,
		0x02000100, 0x40000000, 0x42080000, 0x02080100,
		0x40080100, 0x00000100, 0x02000000, 0x42080000,
		0x42080100, 0x00080100, 0x42000000, 0x42080100,
		0x02080000, 0x00000000, 0x40080000, 0x42000000,
		0x00080100, 0x02000100, 0x40000100, 0x00080000,
		0x00000000, 0x40080000, 0x02080100, 0x40000100
	},
	{
		0x20000010, 0x20400000, 0x00004000, 0x20404010,
		0x20400000, 0x00000010, 0x20404010, 0x00400000,
		0x20004000, 0x00404010, 0x00400000, 0x20000010,
		0x00400010, 0x20004000, 0x20000000, 0x00004010,
		0x00000000, 0x00400010, 0x20004010, 0x00004000,
		0x00404000, 0x20004010, 0x00000010, 0x20400010,
		0x20400010, 0x00000000, 0x00404010, 0x20404000,
		0x00004010, 0x00404000, 0x20404000, 0x20000000,
		0x20004000, 0x00000010, 0x20400010, 0x00404000,
		0x20404010, 0x00400000, 0x00004010, 0x20000010,
		0x00400000, 0x20004000, 0x20000000, 0x00004010,
		0x20000010, 0x20404010, 0x00404000, 0x20400000,
		0x00404010, 0x20404000, 0x00000000, 0x20400010,
		0x00000010, 0x00004000, 0x20400000, 0x00404010,
		0x00004000, 0x00400010, 0x20004010, 0x00000000,
		0x20404000, 0x20000000, 0x00400010, 0x20004010
	},
	{
		0x00200000, 0x04200002, 0x04000802, 0x00000000,
		0x00000800, 0x04000802, 0x00200802, 0x04200800,
		0x04200802, 0x00200000, 0x00000000, 0x04000002,
		0x00000002, 0x04000000, 0x04200002, 0x00000802,
		0x04000800, 0x00200802, 0x00200002, 0x04000800,
		0x04000002, 0x04200000, 0x04200800, 0x00200002,
		0x04200000, 0x00000800, 0x00000802, 0x04200802,
		0x00200800, 0x00000002, 0x04000000, 0x00200800,
		0x04000000, 0x00200800, 0x00200000, 0x04000802,
		0x04000802, 0x04200002, 0x04200002, 0x00000002,
		0x00200002, 0x04000000, 0x04000800, 0x00200000,
		0x04200800, 0x00000802, 0x00200802, 0x04200800,
		0x00000802, 0x04000002, 0x04200802, 0x04200000,
		0x00200800, 0x00000000, 0x00000002, 0x04200802,
		0x00000000, 0x00200802, 0x04200000, 0x00000800,
		0x04000002, 0x04000800, 0x00000800, 0x00200002
	},
	{
		0x10001040, 0x00001000, 0x00040000, 0x10041040,
		0x10000000, 0x10001040, 0x00000040, 0x10000000,
		0x00040040, 0x10040000, 0x10041040, 0x00041000,
		0x10041000, 0x00041040, 0x00001000, 0x00000040,
		0x10040000, 0x10000040, 0x10001000, 0x00001040,
		0x00041000, 0x00040040, 0x10040040, 0x10041000,
		0x00001040, 0x00000000, 0x00000000, 0x10040040,
		0x10000040, 0x10001000, 0x00041040, 0x00040000,
		0x00041040, 0x00040000, 0x10041000, 0x00001000,
		0x00000040, 0x10040040, 0x00001000, 0x00041040,
		0x10001000, 0x00000040, 0x10000040, 0x10040000,
		0x10040040, 0x10000000, 0x00040000, 0x10001040,
		0x00000000, 0x10041040, 0x00040040, 0x10000040,
		0x10040000, 0x10001000, 0x10001040, 0x00000000,
		0x10041040, 0x00041000, 0x00041000, 0x00001040,
		0x00001040, 0x00040040, 0x10000000, 0x10041000
	}
};


/* Key schedule-related tables from FIPS-46 */

/* permuted choice table (key) */
static const unsigned char pc1[] = {
	57, 49, 41, 33, 25, 17,  9,
	01, 58, 50, 42, 34, 26, 18,
	10,  2, 59, 51, 43, 35, 27,
	19, 11,  3, 60, 52, 44, 36,

	63, 55, 47, 39, 31, 23, 15,
	07, 62, 54, 46, 38, 30, 22,
	14,  6, 61, 53, 45, 37, 29,
	21, 13,  5, 28, 20, 12,  4
};

/* number left rotations of pc1 */
static const unsigned char totrot[] = {
	1, 2, 4, 6, 8, 10, 12, 14, 15, 17, 19, 21, 23, 25, 27, 28
};

/* permuted choice key (table) */
static const unsigned char pc2[] = {
	14, 17, 11, 24,  1,  5,
	03, 28, 15,  6, 21, 10,
	23, 19, 12,  4, 26,  8,
	16,  7, 27, 20, 13,  2,
	41, 52, 31, 37, 47, 55,
	30, 40, 51, 45, 33, 48,
	44, 49, 39, 56, 34, 53,
	46, 42, 50, 36, 29, 32
};

/* End of DES-defined tables */


/* int Asmversion = 0; */
#define	Asmversion	0

/* bit 0 is left-most in byte */
static const int bytebit[] = {
	0200, 0100, 040, 020, 010, 04, 02, 01
};


/* Generate key schedule for encryption or decryption
 * depending on the value of "decrypt"
 */
void
BCMROMFN(deskey)(DES_KS k,		/* Key schedule array */
		 unsigned char *key,	/* 64 bits (will use only 56) */
		 int decrypt)		/* 0 = encrypt, 1 = decrypt */
{
	unsigned char pc1m[56];		/* place to modify pc1 into */
	unsigned char pcr[56];		/* place to rotate pc1 into */
	register int i, j, l;
	int m;
	unsigned char ks[8];

	for (j = 0; j < 56; j++) {		/* convert pc1 to bits of key */
		l = pc1[j] - 1;			/* integer bit location	 */
		m = l & 07;			/* find bit		 */
		pc1m[j] = (key[l >> 3] &	/* find which key byte l is in */
		           bytebit[m])		/* and which bit of that byte */
		        ? 1 : 0;		/* and store 1-bit result */
	}
	for (i = 0; i < 16; i++) {		/* key chunk for each iteration */
		memset(ks, 0, sizeof(ks));	/* Clear key schedule */
		for (j = 0; j < 56; j++)	/* rotate pc1 the right amount */
			pcr[j] = pc1m[(l = j + totrot[decrypt ? 15 - i : i]) <
			              (j < 28 ? 28 : 56) ? l : l - 28];
		/* rotate left and right halves independently */
		for (j = 0; j < 48; j++) {	/* select bits individually */
			/* check bit that goes to ks[j] */
			if (pcr[pc2[j] - 1]) {
				/* mask it in if it's there */
				l = j % 6;
				ks[j/6] |= bytebit[l] >> 2;
			}
		}
		/* Now convert to packed odd/even interleaved form */
		k[i][0] = (((unsigned int)ks[0] << 24) |
		           ((unsigned int)ks[2] << 16) |
		           ((unsigned int)ks[4] << 8) |
		           ((unsigned int)ks[6]));
		k[i][1] = (((unsigned int)ks[1] << 24) |
		           ((unsigned int)ks[3] << 16) |
		           ((unsigned int)ks[5] << 8) |
		           ((unsigned int)ks[7]));
		if (Asmversion) {
			/* The assembler versions pre-shift each subkey 2 bits
			 * so the Spbox indexes are already computed
			 */
			k[i][0] <<= 2;
			k[i][1] <<= 2;
		}
	}
}

/* Portable C version of des() function */

/* Tables defined in the Data Encryption Standard documents
 * Three of these tables, the initial permutation, the final
 * permutation and the expansion operator, are regular enough that
 * for speed, we hard-code them. They're here for reference only.
 * Also, the S and P boxes are used by a separate program, gensp.c,
 * to build the combined SP box, Spbox[]. They're also here just
 * for reference.
 */
#ifdef	notdef
/* initial permutation IP */
static unsigned char ip[] = {
	58, 50, 42, 34, 26, 18, 10,  2,
	60, 52, 44, 36, 28, 20, 12,  4,
	62, 54, 46, 38, 30, 22, 14,  6,
	64, 56, 48, 40, 32, 24, 16,  8,
	57, 49, 41, 33, 25, 17,  9,  1,
	59, 51, 43, 35, 27, 19, 11,  3,
	61, 53, 45, 37, 29, 21, 13,  5,
	63, 55, 47, 39, 31, 23, 15,  7
};

/* final permutation IP^-1 */
static unsigned char fp[] = {
	40,  8, 48, 16, 56, 24, 64, 32,
	39,  7, 47, 15, 55, 23, 63, 31,
	38,  6, 46, 14, 54, 22, 62, 30,
	37,  5, 45, 13, 53, 21, 61, 29,
	36,  4, 44, 12, 52, 20, 60, 28,
	35,  3, 43, 11, 51, 19, 59, 27,
	34,  2, 42, 10, 50, 18, 58, 26,
	33,  1, 41,  9, 49, 17, 57, 25
};
/* expansion operation matrix */
static unsigned char ei[] = {
	32,  1,  2,  3,  4,  5,
	4,   5,  6,  7,  8,  9,
	8,   9, 10, 11, 12, 13,
	12, 13, 14, 15, 16, 17,
	16, 17, 18, 19, 20, 21,
	20, 21, 22, 23, 24, 25,
	24, 25, 26, 27, 28, 29,
	28, 29, 30, 31, 32,  1
};
/* The (in)famous S-boxes */
static unsigned char sbox[8][64] = {
	/* S1 */
	14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7,
	00, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8,
	04,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0,
	15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13,

	/* S2 */
	15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10,
	03, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5,
	00, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15,
	13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9,

	/* S3 */
	10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8,
	13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1,
	13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7,
	01, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12,

	/* S4 */
	07, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15,
	13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9,
	10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4,
	03, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14,

	/* S5 */
	02, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9,
	14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6,
	04,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14,
	11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3,

	/* S6 */
	12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11,
	10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8,
	9,  14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6,
	4,   3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13,

	/* S7 */
	04, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1,
	13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6,
	01,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2,
	06, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12,

	/* S8 */
	13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7,
	01, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2,
	07, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8,
	02,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11
};

/* 32-bit permutation function P used on the output of the S-boxes */
static unsigned char p32i[] = {
	16,  7, 20, 21,
	29, 12, 28, 17,
	01, 15, 23, 26,
	05, 18, 31, 10,
	02,  8, 24, 14,
	32, 27,  3,  9,
	19, 13, 30,  6,
	22, 11,  4, 25
};
#endif	/* notdef */

/* Combined SP lookup table, linked in
 * For best results, ensure that this is aligned on a 32-bit boundary;
 * Borland C++ 3.1 doesn't guarantee this!
 */

/* Primitive function F.
 * Input is r, subkey array in keys, output is XORed into l.
 * Each round consumes eight 6-bit subkeys, one for
 * each of the 8 S-boxes, 2 longs for each round.
 * Each int contains four 6-bit subkeys, each taking up a byte.
 * The first int contains, from high to low end, the subkeys for
 * S-boxes 1, 3, 5 & 7; the second contains the subkeys for S-boxes
 * 2, 4, 6 & 8 (using the origin-1 S-box numbering in the standard,
 * not the origin-0 numbering used elsewhere in this code)
 * See comments elsewhere about the pre-rotated values of r and Spbox.
 */
#define	F(l, r, key) { \
	work = ((r >> 4) | (r << 28)) ^ key[0]; \
	l ^= Spbox[6][work & 0x3f]; \
	l ^= Spbox[4][(work >> 8) & 0x3f]; \
	l ^= Spbox[2][(work >> 16) & 0x3f]; \
	l ^= Spbox[0][(work >> 24) & 0x3f]; \
	work = r ^ key[1]; \
	l ^= Spbox[7][work & 0x3f]; \
	l ^= Spbox[5][(work >> 8) & 0x3f]; \
	l ^= Spbox[3][(work >> 16) & 0x3f]; \
	l ^= Spbox[1][(work >> 24) & 0x3f]; \
}
/* Encrypt or decrypt a block of data in ECB mode */
void
BCMROMFN(des)(unsigned int ks[16][2],		/* Key schedule */
	      unsigned char block[8])		/* Data block */
{
/* Optimize for size. */
#define DES_SIZE_OPTIMIZE

	unsigned int left, right, work;
#if defined(DES_SIZE_OPTIMIZE)
	int round;
#endif

	/* Read input block and place in left/right in big-endian order */
	left = (((unsigned int)block[0] << 24) |
	        ((unsigned int)block[1] << 16) |
	        ((unsigned int)block[2] << 8) |
	        (unsigned int)block[3]);
	right = (((unsigned int)block[4] << 24) |
	         ((unsigned int)block[5] << 16) |
	         ((unsigned int)block[6] << 8) |
	         (unsigned int)block[7]);

	/* Hoey's clever initial permutation algorithm, from Outerbridge
	 * (see Schneier p 478)
	 *
	 * The convention here is the same as Outerbridge: rotate each
	 * register left by 1 bit, i.e., so that "left" contains permuted
	 * input bits 2, 3, 4, ... 1 and "right" contains 33, 34, 35, ... 32
	 * (using origin-1 numbering as in the FIPS). This allows us to avoid
	 * one of the two rotates that would otherwise be required in each of
	 * the 16 rounds.
	 */
	work = ((left >> 4) ^ right) & 0x0f0f0f0f;
	right ^= work;
	left ^= work << 4;
	work = ((left >> 16) ^ right) & 0xffff;
	right ^= work;
	left ^= work << 16;
	work = ((right >> 2) ^ left) & 0x33333333;
	left ^= work;
	right ^= (work << 2);
	work = ((right >> 8) ^ left) & 0xff00ff;
	left ^= work;
	right ^= (work << 8);
	right = (right << 1) | (right >> 31);
	work = (left ^ right) & 0xaaaaaaaa;
	left ^= work;
	right ^= work;
	left = (left << 1) | (left >> 31);

	/* Now do the 16 rounds.
	 * Fully unrolling generates 4x larger ARM code for this function
	 */
#if defined(DES_SIZE_OPTIMIZE)
	for (round = 0; round < 16; round += 2) {
		F(left, right, ks[round + 0]);
		F(right, left, ks[round + 1]);
	}
#else /* !DES_SIZE_OPTIMIZE */
	F(left, right, ks[0]);
	F(right, left, ks[1]);
	F(left, right, ks[2]);
	F(right, left, ks[3]);
	F(left, right, ks[4]);
	F(right, left, ks[5]);
	F(left, right, ks[6]);
	F(right, left, ks[7]);
	F(left, right, ks[8]);
	F(right, left, ks[9]);
	F(left, right, ks[10]);
	F(right, left, ks[11]);
	F(left, right, ks[12]);
	F(right, left, ks[13]);
	F(left, right, ks[14]);
	F(right, left, ks[15]);
#endif /* !DES_SIZE_OPTIMIZE */

	/* Inverse permutation, also from Hoey via Outerbridge and Schneier */
	right = (right << 31) | (right >> 1);
	work = (left ^ right) & 0xaaaaaaaa;
	left ^= work;
	right ^= work;
	left = (left >> 1) | (left  << 31);
	work = ((left >> 8) ^ right) & 0xff00ff;
	right ^= work;
	left ^= work << 8;
	work = ((left >> 2) ^ right) & 0x33333333;
	right ^= work;
	left ^= work << 2;
	work = ((right >> 16) ^ left) & 0xffff;
	left ^= work;
	right ^= work << 16;
	work = ((right >> 4) ^ left) & 0x0f0f0f0f;
	left ^= work;
	right ^= work << 4;

	/* Put the block back into the user's buffer with final swap */
	block[0] = (unsigned char)(right >> 24);
	block[1] = (unsigned char)(right >> 16);
	block[2] = (unsigned char)(right >> 8);
	block[3] = (unsigned char)right;
	block[4] = (unsigned char)(left >> 24);
	block[5] = (unsigned char)(left >> 16);
	block[6] = (unsigned char)(left >> 8);
	block[7] = (unsigned char)left;
}

#ifdef DES_TEST_STANDALONE
#include <stdio.h>
#include <unistd.h>
#include <time.h>

static unsigned int
randlong(void)
{
	static unsigned int r = 0;
	if (r == 0)
		r = getpid() * time(0) * 153846157;
	r = (r + 7180351) * 74449801;
	return r;
}

static void
dumpit(int sample, unsigned int ks[16][2], unsigned char in[8], unsigned char out[8])
{
	int i;

	printf("unsigned int sample%d_ks[16][2] = {\n", sample);
	for (i = 0; i < 16; i++)
		printf("\t{ 0x%08lx, 0x%08lx },\n", ks[i][0], ks[i][1]);
	printf("};\n\n");
	printf("unsigned char sample%d_in[8] = {\n\t", sample);
	for (i = 0; i < 8; i++)
		printf("%s0x%02x,", i > 0 ? " " : "", in[i]);
	printf("\n};\n\n");
	printf("unsigned char sample%d_out[8] = {\n\t", sample);
	for (i = 0; i < 8; i++)
		printf("%s0x%02x,", i > 0 ? " " : "", out[i]);
	printf("\n};\n\n");
}

void
gensample(int sample)
{
	unsigned int ks[16][2];
	unsigned char in[8], out[8];
	int i, j;

	for (i = 0; i < 16; i++)
		for (j = 0; j < 2; j++)
			ks[i][j] = randlong();

	for (i = 0; i < 8; i++)
		in[i] = out[i] = randlong() & 0xff;

	des(ks, out);

	dumpit(sample, ks, in, out);
}

int
checksample(unsigned int ks[16][2], unsigned char in[8], unsigned char expected_out[8])
{
	des(ks, in);
	return (memcmp(in, expected_out, 8) == 0);
}

unsigned int sample1_ks[16][2] = {
	{ 0xe75fea2c, 0xb035c443 },
	{ 0x7245bf92, 0xab5529d9 },
	{ 0xfde3ded8, 0x03f1b84f },
	{ 0x6af77dfe, 0xd6ef55a5 },
	{ 0x7b28b304, 0x5f83c7db },
	{ 0x1f9073ea, 0x8eecb4f1 },
	{ 0xcb1e96b0, 0x5705a2e7 },
	{ 0x1edfd156, 0xadbdf7bd },
	{ 0x2623b9dc, 0x2f6ef973 },
	{ 0x3882c642, 0x2611ce09 },
	{ 0x77c44c88, 0x9b557b7f },
	{ 0xbfa482ae, 0xfb94e7d5 },
	{ 0xbd7a7eb4, 0xafacd90b },
	{ 0xc5be369a, 0xf3b1f521 },
	{ 0xdd6e8060, 0x1086c217 },
	{ 0x4d571206, 0xfe51a5ed },
};

unsigned char sample1_in[8] = {
	0x8c, 0xa3, 0xf2, 0x39, 0x38, 0xaf, 0x5e, 0x05,
};

unsigned char sample1_out[8] = {
	0x0e, 0x38, 0x62, 0x0a, 0x72, 0x93, 0xc8, 0x25,
};

unsigned int sample2_ks[16][2] = {
	{ 0xf130845c, 0x97a6d7f3 },
	{ 0xfe0458c2, 0x7520b489 },
	{ 0xcb712708, 0x0165e9ff },
	{ 0xe7bb252e, 0xcb995e55 },
	{ 0xf5416934, 0x75dfd78b },
	{ 0xc803e91a, 0x1415fba1 },
	{ 0x37097ae0, 0x6d665097 },
	{ 0xf7a5d486, 0x26bf3c6d },
	{ 0xb89f8c0c, 0xbc290523 },
	{ 0x93361772, 0x0efbd0b9 },
	{ 0xc807ccb8, 0x2ef5a52f },
	{ 0x9dd7e1de, 0xe4306885 },
	{ 0x18346ce4, 0x5ff7e0bb },
	{ 0x1bfc63ca, 0x5a7fb3d1 },
	{ 0x77c59c90, 0xc37967c7 },
	{ 0x2622cd36, 0x0b8a629d },
};

unsigned char sample2_in[8] = {
	0xbc, 0x53, 0x22, 0xe9, 0x68, 0x5f, 0x8e, 0xb5,
};

unsigned char sample2_out[8] = {
	0x76, 0xb9, 0xdf, 0xb8, 0x2a, 0x5b, 0x50, 0x48,
};

int
main(int argc, char **argv)
{

	if (!checksample(sample1_ks, sample1_in, sample1_out))
		printf("sample1 bad\n");
	else if (!checksample(sample2_ks, sample2_in, sample2_out))
		printf("sample2 bad\n");
	else
		printf("pass\n");

	return 0;
}

#endif /* DES_TEST_STANDALONE */
