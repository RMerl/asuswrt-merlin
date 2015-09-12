/*
 * tkhash.c
 * Trimmed version of reference code from "Simple Security Network (SSN) for
 * IEEE 802.11", v0.20, plus test routine.
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: tkhash.c 241182 2011-02-17 21:50:03Z $
 */

/* Reference code from:
 *
 *   Contents:    Generate 802.11 per-packet RC4 key hash test vectors
 *   Date:        April 19, 2002
 *   Authors:     Doug Whiting,   Hifn
 *                Russ Housley,   RSA Labs
 *                Niels Ferguson, MacFergus
 *                Doug Smith,     Cisco
 *   Notes:
 *
 *   This code is released to the public domain use, built solely out of
 *   the goodness of our hearts for the benefit of all mankind. As such,
 *   there are no warranties of any kind given on the correctness or
 *   usefulness of this code.
 *
 *   This code is written for pedagogical purposes, NOT for performance.
 *
 */

#include <bcmcrypto/tkhash.h>

/* macros for extraction/creation of byte/u16b values  */
#define	RotR1(v16)	((((v16) >> 1) & 0x7FFF) ^ (((v16) & 1) << 15))
#define	Lo8(v16)	((uint8)((v16) & 0x00FF))
#define	Hi8(v16)	((uint8)(((v16) >> 8) & 0x00FF))
#define	Lo16(v32)	((uint16)((v32) & 0xFFFF))
#define	Hi16(v32)	((uint16)(((v32) >>16) & 0xFFFF))
#define	Mk16(hi, lo)	((lo) ^ (((uint16)(hi)) << 8))

/* select the Nth 16-bit word of the temporal key byte array TK[]   */
#define	TK16(N)		Mk16(TK[2 * (N) + 1], TK[2 * (N)])

/* fixed algorithm "parameters" */
#define PHASE1_LOOP_CNT   8    /* this needs to be "big enough"     */
#define TA_SIZE           6    /*  48-bit transmitter address       */
#define TK_SIZE          16    /* 128-bit temporal key              */
#define P1K_SIZE         10    /*  80-bit Phase1 key                */
#define RC4_KEY_SIZE     16    /* 128-bit RC4KEY (104 bits unknown) */

/*
 * Note that the Sbox[] table below is a subset of rijndael-alg-fst.c's Te0[256] array.
 * To save 1024 bytes of memory, define SHARE_RIJNDAEL_SBOX to re-use that one here.
 * Can only be used if rijndael-alg-fst.c is included in the build.
 */
#ifdef SHARE_RIJNDAEL_SBOX

extern const uint32 rijndaelTe0[256];

#define SBOX0(ind)	(((rijndaelTe0[ind] >> 16) & 0xff00) | \
			 ((rijndaelTe0[ind] >> 0) & 0x00ff))
#define SBOX1(ind)	(((rijndaelTe0[ind] >> 24) & 0x00ff) | \
			 ((rijndaelTe0[ind] << 8) & 0xff00))

#else /* !SHARE_RIJNDAEL_SBOX */

/* 2-byte by 2-byte subset of the full AES S-box table */
static const uint16 Sbox[2][256] = /* Sbox for hash (can be in ROM)     */
{
	{
		0xC6A5, 0xF884, 0xEE99, 0xF68D, 0xFF0D, 0xD6BD, 0xDEB1, 0x9154,
		0x6050, 0x0203, 0xCEA9, 0x567D, 0xE719, 0xB562, 0x4DE6, 0xEC9A,
		0x8F45, 0x1F9D, 0x8940, 0xFA87, 0xEF15, 0xB2EB, 0x8EC9, 0xFB0B,
		0x41EC, 0xB367, 0x5FFD, 0x45EA, 0x23BF, 0x53F7, 0xE496, 0x9B5B,
		0x75C2, 0xE11C, 0x3DAE, 0x4C6A, 0x6C5A, 0x7E41, 0xF502, 0x834F,
		0x685C, 0x51F4, 0xD134, 0xF908, 0xE293, 0xAB73, 0x6253, 0x2A3F,
		0x080C, 0x9552, 0x4665, 0x9D5E, 0x3028, 0x37A1, 0x0A0F, 0x2FB5,
		0x0E09, 0x2436, 0x1B9B, 0xDF3D, 0xCD26, 0x4E69, 0x7FCD, 0xEA9F,
		0x121B, 0x1D9E, 0x5874, 0x342E, 0x362D, 0xDCB2, 0xB4EE, 0x5BFB,
		0xA4F6, 0x764D, 0xB761, 0x7DCE, 0x527B, 0xDD3E, 0x5E71, 0x1397,
		0xA6F5, 0xB968, 0x0000, 0xC12C, 0x4060, 0xE31F, 0x79C8, 0xB6ED,
		0xD4BE, 0x8D46, 0x67D9, 0x724B, 0x94DE, 0x98D4, 0xB0E8, 0x854A,
		0xBB6B, 0xC52A, 0x4FE5, 0xED16, 0x86C5, 0x9AD7, 0x6655, 0x1194,
		0x8ACF, 0xE910, 0x0406, 0xFE81, 0xA0F0, 0x7844, 0x25BA, 0x4BE3,
		0xA2F3, 0x5DFE, 0x80C0, 0x058A, 0x3FAD, 0x21BC, 0x7048, 0xF104,
		0x63DF, 0x77C1, 0xAF75, 0x4263, 0x2030, 0xE51A, 0xFD0E, 0xBF6D,
		0x814C, 0x1814, 0x2635, 0xC32F, 0xBEE1, 0x35A2, 0x88CC, 0x2E39,
		0x9357, 0x55F2, 0xFC82, 0x7A47, 0xC8AC, 0xBAE7, 0x322B, 0xE695,
		0xC0A0, 0x1998, 0x9ED1, 0xA37F, 0x4466, 0x547E, 0x3BAB, 0x0B83,
		0x8CCA, 0xC729, 0x6BD3, 0x283C, 0xA779, 0xBCE2, 0x161D, 0xAD76,
		0xDB3B, 0x6456, 0x744E, 0x141E, 0x92DB, 0x0C0A, 0x486C, 0xB8E4,
		0x9F5D, 0xBD6E, 0x43EF, 0xC4A6, 0x39A8, 0x31A4, 0xD337, 0xF28B,
		0xD532, 0x8B43, 0x6E59, 0xDAB7, 0x018C, 0xB164, 0x9CD2, 0x49E0,
		0xD8B4, 0xACFA, 0xF307, 0xCF25, 0xCAAF, 0xF48E, 0x47E9, 0x1018,
		0x6FD5, 0xF088, 0x4A6F, 0x5C72, 0x3824, 0x57F1, 0x73C7, 0x9751,
		0xCB23, 0xA17C, 0xE89C, 0x3E21, 0x96DD, 0x61DC, 0x0D86, 0x0F85,
		0xE090, 0x7C42, 0x71C4, 0xCCAA, 0x90D8, 0x0605, 0xF701, 0x1C12,
		0xC2A3, 0x6A5F, 0xAEF9, 0x69D0, 0x1791, 0x9958, 0x3A27, 0x27B9,
		0xD938, 0xEB13, 0x2BB3, 0x2233, 0xD2BB, 0xA970, 0x0789, 0x33A7,
		0x2DB6, 0x3C22, 0x1592, 0xC920, 0x8749, 0xAAFF, 0x5078, 0xA57A,
		0x038F, 0x59F8, 0x0980, 0x1A17, 0x65DA, 0xD731, 0x84C6, 0xD0B8,
		0x82C3, 0x29B0, 0x5A77, 0x1E11, 0x7BCB, 0xA8FC, 0x6DD6, 0x2C3A
	},

	{	/* second half of table is byte-reversed version of first! */
		0xA5C6, 0x84F8, 0x99EE, 0x8DF6, 0x0DFF, 0xBDD6, 0xB1DE, 0x5491,
		0x5060, 0x0302, 0xA9CE, 0x7D56, 0x19E7, 0x62B5, 0xE64D, 0x9AEC,
		0x458F, 0x9D1F, 0x4089, 0x87FA, 0x15EF, 0xEBB2, 0xC98E, 0x0BFB,
		0xEC41, 0x67B3, 0xFD5F, 0xEA45, 0xBF23, 0xF753, 0x96E4, 0x5B9B,
		0xC275, 0x1CE1, 0xAE3D, 0x6A4C, 0x5A6C, 0x417E, 0x02F5, 0x4F83,
		0x5C68, 0xF451, 0x34D1, 0x08F9, 0x93E2, 0x73AB, 0x5362, 0x3F2A,
		0x0C08, 0x5295, 0x6546, 0x5E9D, 0x2830, 0xA137, 0x0F0A, 0xB52F,
		0x090E, 0x3624, 0x9B1B, 0x3DDF, 0x26CD, 0x694E, 0xCD7F, 0x9FEA,
		0x1B12, 0x9E1D, 0x7458, 0x2E34, 0x2D36, 0xB2DC, 0xEEB4, 0xFB5B,
		0xF6A4, 0x4D76, 0x61B7, 0xCE7D, 0x7B52, 0x3EDD, 0x715E, 0x9713,
		0xF5A6, 0x68B9, 0x0000, 0x2CC1, 0x6040, 0x1FE3, 0xC879, 0xEDB6,
		0xBED4, 0x468D, 0xD967, 0x4B72, 0xDE94, 0xD498, 0xE8B0, 0x4A85,
		0x6BBB, 0x2AC5, 0xE54F, 0x16ED, 0xC586, 0xD79A, 0x5566, 0x9411,
		0xCF8A, 0x10E9, 0x0604, 0x81FE, 0xF0A0, 0x4478, 0xBA25, 0xE34B,
		0xF3A2, 0xFE5D, 0xC080, 0x8A05, 0xAD3F, 0xBC21, 0x4870, 0x04F1,
		0xDF63, 0xC177, 0x75AF, 0x6342, 0x3020, 0x1AE5, 0x0EFD, 0x6DBF,
		0x4C81, 0x1418, 0x3526, 0x2FC3, 0xE1BE, 0xA235, 0xCC88, 0x392E,
		0x5793, 0xF255, 0x82FC, 0x477A, 0xACC8, 0xE7BA, 0x2B32, 0x95E6,
		0xA0C0, 0x9819, 0xD19E, 0x7FA3, 0x6644, 0x7E54, 0xAB3B, 0x830B,
		0xCA8C, 0x29C7, 0xD36B, 0x3C28, 0x79A7, 0xE2BC, 0x1D16, 0x76AD,
		0x3BDB, 0x5664, 0x4E74, 0x1E14, 0xDB92, 0x0A0C, 0x6C48, 0xE4B8,
		0x5D9F, 0x6EBD, 0xEF43, 0xA6C4, 0xA839, 0xA431, 0x37D3, 0x8BF2,
		0x32D5, 0x438B, 0x596E, 0xB7DA, 0x8C01, 0x64B1, 0xD29C, 0xE049,
		0xB4D8, 0xFAAC, 0x07F3, 0x25CF, 0xAFCA, 0x8EF4, 0xE947, 0x1810,
		0xD56F, 0x88F0, 0x6F4A, 0x725C, 0x2438, 0xF157, 0xC773, 0x5197,
		0x23CB, 0x7CA1, 0x9CE8, 0x213E, 0xDD96, 0xDC61, 0x860D, 0x850F,
		0x90E0, 0x427C, 0xC471, 0xAACC, 0xD890, 0x0506, 0x01F7, 0x121C,
		0xA3C2, 0x5F6A, 0xF9AE, 0xD069, 0x9117, 0x5899, 0x273A, 0xB927,
		0x38D9, 0x13EB, 0xB32B, 0x3322, 0xBBD2, 0x70A9, 0x8907, 0xA733,
		0xB62D, 0x223C, 0x9215, 0x20C9, 0x4987, 0xFFAA, 0x7850, 0x7AA5,
		0x8F03, 0xF859, 0x8009, 0x171A, 0xDA65, 0x31D7, 0xC684, 0xB8D0,
		0xC382, 0xB029, 0x775A, 0x111E, 0xCB7B, 0xFCA8, 0xD66D, 0x3A2C
	}
};

#define SBOX0(ind)	Sbox[0][ind]
#define SBOX1(ind)	Sbox[1][ind]

#endif /* !SHARE_RIJNDAEL_SBOX */

/* S-box lookup: 16 bits --> 16 bits */
#define	_S_(v16)	(SBOX0(Lo8(v16)) ^ SBOX1(Hi8(v16)))

/*
 * Routine: Phase 1 -- generate P1K, given TA, TK, IV32
 *
 * Inputs:
 *     TK[]      = temporal key                         [128 bits]
 *     TA[]      = transmitter's MAC address            [ 48 bits]
 *     IV32      = upper 32 bits of IV                  [ 32 bits]
 * Output:
 *     P1K[]     = Phase 1 key                          [ 80 bits]
 *
 * Note:
 *     This function only needs to be called every 2**16 packets,
 *     although in theory it could be called every packet.
 *
 */
void
BCMROMFN(tkhash_phase1)(uint16 *P1K, const uint8 *TK, const uint8 *TA, uint32 IV32)
{
	uint16 i;

	/* Initialize the 80 bits of P1K[] from IV32 and TA[0..5]     */
	P1K[0]      = Lo16(IV32);
	P1K[1]      = Hi16(IV32);
	P1K[2]      = Mk16(TA[1], TA[0]); /* use TA[] as little-endian */
	P1K[3]      = Mk16(TA[3], TA[2]);
	P1K[4]      = Mk16(TA[5], TA[4]);

	/* Now compute an unbalanced Feistel cipher with 80-bit block */
	/* size on the 80-bit block P1K[], using the 128-bit key TK[] */
	for (i = 0; i < PHASE1_LOOP_CNT; i++) {
		/* Each add operation here is mod 2**16 */
		P1K[0] += _S_(P1K[4] ^ TK16((i & 1) + 0));
		P1K[1] += _S_(P1K[0] ^ TK16((i & 1) + 2));
		P1K[2] += _S_(P1K[1] ^ TK16((i & 1) + 4));
		P1K[3] += _S_(P1K[2] ^ TK16((i & 1) + 6));
		P1K[4] += _S_(P1K[3] ^ TK16((i & 1) + 0));
		P1K[4] +=  i;                    /* avoid "slide attacks" */
	}
}

/*
 *
 * Routine: Phase 2 -- generate RC4KEY, given TK, P1K, IV16
 *
 * Inputs:
 *     TK[]      = Temporal key                         [128 bits]
 *     P1K[]     = Phase 1 output key                   [ 80 bits]
 *     IV16      = low 16 bits of IV counter            [ 16 bits]
 * Output:
 *     RC4KEY[]  = the key used to encrypt the packet   [128 bits]
 *
 * Note:
 *     The value {TA,IV32,IV16} for Phase1/Phase2 must be unique
 *     across all packets using the same key TK value. Then, for a
 *     given value of TK[], this TKIP48 construction guarantees that
 *     the final RC4KEY value is unique across all packets.
 *
 * Suggested implementation optimization: if PPK[] is "overlaid"
 *     appropriately on RC4KEY[], there is no need for the final
 *     for loop below that copies the PPK[] result into RC4KEY[].
 *
 */
void
BCMROMFN(tkhash_phase2)(uint8 *RC4KEY, const uint8 *TK, const uint16 *P1K, uint16 IV16)
{
	uint16 i;
	uint16 PPK[6];                          /* temporary key for mixing    */

	/* Note: all adds in the PPK[] equations below are mod 2**16         */
	for (i = 0; i < 5; i++) PPK[i]=P1K[i];      /* first, copy P1K to PPK      */
	PPK[5]  =  P1K[4] + IV16;             /* next,  add in IV16          */

	/* Bijective non-linear mixing of the 96 bits of PPK[0..5]           */
	PPK[0] +=    _S_(PPK[5] ^ TK16(0));   /* Mix key in each "round"     */
	PPK[1] +=    _S_(PPK[0] ^ TK16(1));
	PPK[2] +=    _S_(PPK[1] ^ TK16(2));
	PPK[3] +=    _S_(PPK[2] ^ TK16(3));
	PPK[4] +=    _S_(PPK[3] ^ TK16(4));
	PPK[5] +=    _S_(PPK[4] ^ TK16(5));   /* Total # S-box lookups == 6  */

	/* Final sweep: bijective, "linear". Rotates kill LSB correlations   */
	PPK[0] +=  RotR1(PPK[5] ^ TK16(6));
	PPK[1] +=  RotR1(PPK[0] ^ TK16(7));   /* Use all of TK[] in Phase2   */
	PPK[2] +=  RotR1(PPK[1]);
	PPK[3] +=  RotR1(PPK[2]);
	PPK[4] +=  RotR1(PPK[3]);
	PPK[5] +=  RotR1(PPK[4]);
	/* Note: At this point, for a given key TK[0..15], the 96-bit output */
	/*       value PPK[0..5] is guaranteed to be unique, as a function   */
	/*       of the 96-bit "input" value   {TA,IV32,IV16}. That is, P1K  */
	/*       is now a keyed permutation of {TA,IV32,IV16}.               */

	/* Set RC4KEY[0..3], which includes "cleartext" portion of RC4 key   */
	RC4KEY[0] = Hi8(IV16);                /* RC4KEY[0..2] is the WEP IV  */
	RC4KEY[1] =(Hi8(IV16) | 0x20) & 0x7F; /* Help avoid weak (FMS) keys  */
	RC4KEY[2] = Lo8(IV16);
	RC4KEY[3] = Lo8((PPK[5] ^ TK16(0)) >> 1);

	/* Copy 96 bits of PPK[0..5] to RC4KEY[4..15]  (little-endian)       */
	for (i = 0; i < 6; i++) {
		RC4KEY[4 + 2 * i] = Lo8(PPK[i]);
		RC4KEY[5 + 2 * i] = Hi8(PPK[i]);
	}
}


#ifdef BCMTKHASH_TEST
#include <stdio.h>
#include <string.h>

#include "tkhash_vectors.h"
#define NUM_VECTORS  (sizeof(tkhash_vec)/sizeof(tkhash_vec[0]))

int main(int argc, char **argv)
{
	int k, fail = 0;
	uint16 p1k[TKHASH_P1_KEY_SIZE / 2];
	uint8 rc4key[TKHASH_P2_KEY_SIZE];
	for (k = 0; k < NUM_VECTORS; k++) {
		tkhash_phase1(p1k, tkhash_vec[k].tk, tkhash_vec[k].ta,
		              *(tkhash_vec[k].iv32));

		if (memcmp(p1k, tkhash_vec[k].p1k, TKHASH_P1_KEY_SIZE) != 0) {
			printf("%s: TKHash Phase1 %d failed\n", *argv, k);
			fail++;
		} else {
			printf("%s: TKHash Phase1 %d passed\n", *argv, k);
		}

		tkhash_phase2(rc4key, tkhash_vec[k].tk, p1k,
			*(tkhash_vec[k].iv16));

		if (memcmp(rc4key, tkhash_vec[k].rc4key, TKHASH_P2_KEY_SIZE) != 0) {
			printf("%s: TKHash Phase2 %d failed\n", *argv, k);
			fail++;
		} else {
			printf("%s: TKHash Phase2 %d passed\n", *argv, k);
		}

	}

	printf("%s: %s\n", *argv, fail?"FAILED":"PASSED");
	return (fail);
}
#endif /* BCMTKHASH_TEST */
