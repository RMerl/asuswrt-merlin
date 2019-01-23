/*
 * bzip2 is written by Julian Seward <jseward@bzip.org>.
 * Adapted for busybox by Denys Vlasenko <vda.linux@googlemail.com>.
 * See README and LICENSE files in this directory for more information.
 */

/*-------------------------------------------------------------*/
/*--- Compression machinery (not incl block sorting)        ---*/
/*---                                            compress.c ---*/
/*-------------------------------------------------------------*/

/* ------------------------------------------------------------------
This file is part of bzip2/libbzip2, a program and library for
lossless, block-sorting data compression.

bzip2/libbzip2 version 1.0.4 of 20 December 2006
Copyright (C) 1996-2006 Julian Seward <jseward@bzip.org>

Please read the WARNING, DISCLAIMER and PATENTS sections in the
README file.

This program is released under the terms of the license contained
in the file LICENSE.
------------------------------------------------------------------ */

/* CHANGES
 * 0.9.0    -- original version.
 * 0.9.0a/b -- no changes in this file.
 * 0.9.0c   -- changed setting of nGroups in sendMTFValues()
 *             so as to do a bit better on small files
*/

/* #include "bzlib_private.h" */

/*---------------------------------------------------*/
/*--- Bit stream I/O                              ---*/
/*---------------------------------------------------*/

/*---------------------------------------------------*/
static
void BZ2_bsInitWrite(EState* s)
{
	s->bsLive = 0;
	s->bsBuff = 0;
}


/*---------------------------------------------------*/
static NOINLINE
void bsFinishWrite(EState* s)
{
	while (s->bsLive > 0) {
		s->zbits[s->numZ] = (uint8_t)(s->bsBuff >> 24);
		s->numZ++;
		s->bsBuff <<= 8;
		s->bsLive -= 8;
	}
}


/*---------------------------------------------------*/
static
/* Helps only on level 5, on other levels hurts. ? */
#if CONFIG_BZIP2_FAST >= 5
ALWAYS_INLINE
#endif
void bsW(EState* s, int32_t n, uint32_t v)
{
	while (s->bsLive >= 8) {
		s->zbits[s->numZ] = (uint8_t)(s->bsBuff >> 24);
		s->numZ++;
		s->bsBuff <<= 8;
		s->bsLive -= 8;
	}
	s->bsBuff |= (v << (32 - s->bsLive - n));
	s->bsLive += n;
}


/*---------------------------------------------------*/
static
void bsPutU32(EState* s, unsigned u)
{
	bsW(s, 8, (u >> 24) & 0xff);
	bsW(s, 8, (u >> 16) & 0xff);
	bsW(s, 8, (u >>  8) & 0xff);
	bsW(s, 8,  u        & 0xff);
}


/*---------------------------------------------------*/
static
void bsPutU16(EState* s, unsigned u)
{
	bsW(s, 8, (u >>  8) & 0xff);
	bsW(s, 8,  u        & 0xff);
}


/*---------------------------------------------------*/
/*--- The back end proper                         ---*/
/*---------------------------------------------------*/

/*---------------------------------------------------*/
static
void makeMaps_e(EState* s)
{
	int i;
	s->nInUse = 0;
	for (i = 0; i < 256; i++) {
		if (s->inUse[i]) {
			s->unseqToSeq[i] = s->nInUse;
			s->nInUse++;
		}
	}
}


/*---------------------------------------------------*/
static NOINLINE
void generateMTFValues(EState* s)
{
	uint8_t yy[256];
	int32_t i, j;
	int32_t zPend;
	int32_t wr;
	int32_t EOB;

	/*
	 * After sorting (eg, here),
	 * s->arr1[0 .. s->nblock-1] holds sorted order,
	 * and
	 * ((uint8_t*)s->arr2)[0 .. s->nblock-1]
	 * holds the original block data.
	 *
	 * The first thing to do is generate the MTF values,
	 * and put them in ((uint16_t*)s->arr1)[0 .. s->nblock-1].
	 *
	 * Because there are strictly fewer or equal MTF values
	 * than block values, ptr values in this area are overwritten
	 * with MTF values only when they are no longer needed.
	 *
	 * The final compressed bitstream is generated into the
	 * area starting at &((uint8_t*)s->arr2)[s->nblock]
	 *
	 * These storage aliases are set up in bzCompressInit(),
	 * except for the last one, which is arranged in
	 * compressBlock().
	 */
	uint32_t* ptr   = s->ptr;
	uint8_t*  block = s->block;
	uint16_t* mtfv  = s->mtfv;

	makeMaps_e(s);
	EOB = s->nInUse+1;

	for (i = 0; i <= EOB; i++)
		s->mtfFreq[i] = 0;

	wr = 0;
	zPend = 0;
	for (i = 0; i < s->nInUse; i++)
		yy[i] = (uint8_t) i;

	for (i = 0; i < s->nblock; i++) {
		uint8_t ll_i;
		AssertD(wr <= i, "generateMTFValues(1)");
		j = ptr[i] - 1;
		if (j < 0)
			j += s->nblock;
		ll_i = s->unseqToSeq[block[j]];
		AssertD(ll_i < s->nInUse, "generateMTFValues(2a)");

		if (yy[0] == ll_i) {
			zPend++;
		} else {
			if (zPend > 0) {
				zPend--;
				while (1) {
					if (zPend & 1) {
						mtfv[wr] = BZ_RUNB; wr++;
						s->mtfFreq[BZ_RUNB]++;
					} else {
						mtfv[wr] = BZ_RUNA; wr++;
						s->mtfFreq[BZ_RUNA]++;
					}
					if (zPend < 2) break;
					zPend = (uint32_t)(zPend - 2) / 2;
					/* bbox: unsigned div is easier */
				};
				zPend = 0;
			}
			{
				register uint8_t  rtmp;
				register uint8_t* ryy_j;
				register uint8_t  rll_i;
				rtmp  = yy[1];
				yy[1] = yy[0];
				ryy_j = &(yy[1]);
				rll_i = ll_i;
				while (rll_i != rtmp) {
					register uint8_t rtmp2;
					ryy_j++;
					rtmp2  = rtmp;
					rtmp   = *ryy_j;
					*ryy_j = rtmp2;
				};
				yy[0] = rtmp;
				j = ryy_j - &(yy[0]);
				mtfv[wr] = j+1;
				wr++;
				s->mtfFreq[j+1]++;
			}
		}
	}

	if (zPend > 0) {
		zPend--;
		while (1) {
			if (zPend & 1) {
				mtfv[wr] = BZ_RUNB;
				wr++;
				s->mtfFreq[BZ_RUNB]++;
			} else {
				mtfv[wr] = BZ_RUNA;
				wr++;
				s->mtfFreq[BZ_RUNA]++;
			}
			if (zPend < 2)
				break;
			zPend = (uint32_t)(zPend - 2) / 2;
			/* bbox: unsigned div is easier */
		};
		zPend = 0;
	}

	mtfv[wr] = EOB;
	wr++;
	s->mtfFreq[EOB]++;

	s->nMTF = wr;
}


/*---------------------------------------------------*/
#define BZ_LESSER_ICOST  0
#define BZ_GREATER_ICOST 15

static NOINLINE
void sendMTFValues(EState* s)
{
	int32_t v, t, i, j, gs, ge, bt, bc, iter;
	int32_t nSelectors, alphaSize, minLen, maxLen, selCtr;
	int32_t nGroups;

	/*
	 * uint8_t len[BZ_N_GROUPS][BZ_MAX_ALPHA_SIZE];
	 * is a global since the decoder also needs it.
	 *
	 * int32_t  code[BZ_N_GROUPS][BZ_MAX_ALPHA_SIZE];
	 * int32_t  rfreq[BZ_N_GROUPS][BZ_MAX_ALPHA_SIZE];
	 * are also globals only used in this proc.
	 * Made global to keep stack frame size small.
	 */
#define code     sendMTFValues__code
#define rfreq    sendMTFValues__rfreq
#define len_pack sendMTFValues__len_pack

	uint16_t cost[BZ_N_GROUPS];
	int32_t  fave[BZ_N_GROUPS];

	uint16_t* mtfv = s->mtfv;

	alphaSize = s->nInUse + 2;
	for (t = 0; t < BZ_N_GROUPS; t++)
		for (v = 0; v < alphaSize; v++)
			s->len[t][v] = BZ_GREATER_ICOST;

	/*--- Decide how many coding tables to use ---*/
	AssertH(s->nMTF > 0, 3001);
	if (s->nMTF < 200)  nGroups = 2; else
	if (s->nMTF < 600)  nGroups = 3; else
	if (s->nMTF < 1200) nGroups = 4; else
	if (s->nMTF < 2400) nGroups = 5; else
	nGroups = 6;

	/*--- Generate an initial set of coding tables ---*/
	{
		int32_t nPart, remF, tFreq, aFreq;

		nPart = nGroups;
		remF  = s->nMTF;
		gs = 0;
		while (nPart > 0) {
			tFreq = remF / nPart;
			ge = gs - 1;
			aFreq = 0;
			while (aFreq < tFreq && ge < alphaSize-1) {
				ge++;
				aFreq += s->mtfFreq[ge];
			}

			if (ge > gs
			 && nPart != nGroups && nPart != 1
			 && ((nGroups - nPart) % 2 == 1) /* bbox: can this be replaced by x & 1? */
			) {
				aFreq -= s->mtfFreq[ge];
				ge--;
			}

			for (v = 0; v < alphaSize; v++)
				if (v >= gs && v <= ge)
					s->len[nPart-1][v] = BZ_LESSER_ICOST;
				else
					s->len[nPart-1][v] = BZ_GREATER_ICOST;

			nPart--;
			gs = ge + 1;
			remF -= aFreq;
		}
	}

	/*
	 * Iterate up to BZ_N_ITERS times to improve the tables.
	 */
	for (iter = 0; iter < BZ_N_ITERS; iter++) {
		for (t = 0; t < nGroups; t++)
			fave[t] = 0;

		for (t = 0; t < nGroups; t++)
			for (v = 0; v < alphaSize; v++)
				s->rfreq[t][v] = 0;

#if CONFIG_BZIP2_FAST >= 5
		/*
		 * Set up an auxiliary length table which is used to fast-track
		 * the common case (nGroups == 6).
		 */
		if (nGroups == 6) {
			for (v = 0; v < alphaSize; v++) {
				s->len_pack[v][0] = (s->len[1][v] << 16) | s->len[0][v];
				s->len_pack[v][1] = (s->len[3][v] << 16) | s->len[2][v];
				s->len_pack[v][2] = (s->len[5][v] << 16) | s->len[4][v];
			}
		}
#endif
		nSelectors = 0;
		gs = 0;
		while (1) {
			/*--- Set group start & end marks. --*/
			if (gs >= s->nMTF)
				break;
			ge = gs + BZ_G_SIZE - 1;
			if (ge >= s->nMTF)
				ge = s->nMTF-1;

			/*
			 * Calculate the cost of this group as coded
			 * by each of the coding tables.
			 */
			for (t = 0; t < nGroups; t++)
				cost[t] = 0;
#if CONFIG_BZIP2_FAST >= 5
			if (nGroups == 6 && 50 == ge-gs+1) {
				/*--- fast track the common case ---*/
				register uint32_t cost01, cost23, cost45;
				register uint16_t icv;
				cost01 = cost23 = cost45 = 0;
#define BZ_ITER(nn) \
	icv = mtfv[gs+(nn)]; \
	cost01 += s->len_pack[icv][0]; \
	cost23 += s->len_pack[icv][1]; \
	cost45 += s->len_pack[icv][2];
				BZ_ITER(0);  BZ_ITER(1);  BZ_ITER(2);  BZ_ITER(3);  BZ_ITER(4);
				BZ_ITER(5);  BZ_ITER(6);  BZ_ITER(7);  BZ_ITER(8);  BZ_ITER(9);
				BZ_ITER(10); BZ_ITER(11); BZ_ITER(12); BZ_ITER(13); BZ_ITER(14);
				BZ_ITER(15); BZ_ITER(16); BZ_ITER(17); BZ_ITER(18); BZ_ITER(19);
				BZ_ITER(20); BZ_ITER(21); BZ_ITER(22); BZ_ITER(23); BZ_ITER(24);
				BZ_ITER(25); BZ_ITER(26); BZ_ITER(27); BZ_ITER(28); BZ_ITER(29);
				BZ_ITER(30); BZ_ITER(31); BZ_ITER(32); BZ_ITER(33); BZ_ITER(34);
				BZ_ITER(35); BZ_ITER(36); BZ_ITER(37); BZ_ITER(38); BZ_ITER(39);
				BZ_ITER(40); BZ_ITER(41); BZ_ITER(42); BZ_ITER(43); BZ_ITER(44);
				BZ_ITER(45); BZ_ITER(46); BZ_ITER(47); BZ_ITER(48); BZ_ITER(49);
#undef BZ_ITER
				cost[0] = cost01 & 0xffff; cost[1] = cost01 >> 16;
				cost[2] = cost23 & 0xffff; cost[3] = cost23 >> 16;
				cost[4] = cost45 & 0xffff; cost[5] = cost45 >> 16;
			} else
#endif
			{
				/*--- slow version which correctly handles all situations ---*/
				for (i = gs; i <= ge; i++) {
					uint16_t icv = mtfv[i];
					for (t = 0; t < nGroups; t++)
						cost[t] += s->len[t][icv];
				}
			}
			/*
			 * Find the coding table which is best for this group,
			 * and record its identity in the selector table.
			 */
			/*bc = 999999999;*/
			/*bt = -1;*/
			bc = cost[0];
			bt = 0;
			for (t = 1 /*0*/; t < nGroups; t++) {
				if (cost[t] < bc) {
					bc = cost[t];
					bt = t;
				}
			}
			fave[bt]++;
			s->selector[nSelectors] = bt;
			nSelectors++;

			/*
			 * Increment the symbol frequencies for the selected table.
			 */
/* 1% faster compress. +800 bytes */
#if CONFIG_BZIP2_FAST >= 4
			if (nGroups == 6 && 50 == ge-gs+1) {
				/*--- fast track the common case ---*/
#define BZ_ITUR(nn) s->rfreq[bt][mtfv[gs + (nn)]]++
				BZ_ITUR(0);  BZ_ITUR(1);  BZ_ITUR(2);  BZ_ITUR(3);  BZ_ITUR(4);
				BZ_ITUR(5);  BZ_ITUR(6);  BZ_ITUR(7);  BZ_ITUR(8);  BZ_ITUR(9);
				BZ_ITUR(10); BZ_ITUR(11); BZ_ITUR(12); BZ_ITUR(13); BZ_ITUR(14);
				BZ_ITUR(15); BZ_ITUR(16); BZ_ITUR(17); BZ_ITUR(18); BZ_ITUR(19);
				BZ_ITUR(20); BZ_ITUR(21); BZ_ITUR(22); BZ_ITUR(23); BZ_ITUR(24);
				BZ_ITUR(25); BZ_ITUR(26); BZ_ITUR(27); BZ_ITUR(28); BZ_ITUR(29);
				BZ_ITUR(30); BZ_ITUR(31); BZ_ITUR(32); BZ_ITUR(33); BZ_ITUR(34);
				BZ_ITUR(35); BZ_ITUR(36); BZ_ITUR(37); BZ_ITUR(38); BZ_ITUR(39);
				BZ_ITUR(40); BZ_ITUR(41); BZ_ITUR(42); BZ_ITUR(43); BZ_ITUR(44);
				BZ_ITUR(45); BZ_ITUR(46); BZ_ITUR(47); BZ_ITUR(48); BZ_ITUR(49);
#undef BZ_ITUR
				gs = ge + 1;
			} else
#endif
			{
				/*--- slow version which correctly handles all situations ---*/
				while (gs <= ge) {
					s->rfreq[bt][mtfv[gs]]++;
					gs++;
				}
				/* already is: gs = ge + 1; */
			}
		}

		/*
		 * Recompute the tables based on the accumulated frequencies.
		 */
		/* maxLen was changed from 20 to 17 in bzip2-1.0.3.  See
		 * comment in huffman.c for details. */
		for (t = 0; t < nGroups; t++)
			BZ2_hbMakeCodeLengths(s, &(s->len[t][0]), &(s->rfreq[t][0]), alphaSize, 17 /*20*/);
	}

	AssertH(nGroups < 8, 3002);
	AssertH(nSelectors < 32768 && nSelectors <= (2 + (900000 / BZ_G_SIZE)), 3003);

	/*--- Compute MTF values for the selectors. ---*/
	{
		uint8_t pos[BZ_N_GROUPS], ll_i, tmp2, tmp;

		for (i = 0; i < nGroups; i++)
			pos[i] = i;
		for (i = 0; i < nSelectors; i++) {
			ll_i = s->selector[i];
			j = 0;
			tmp = pos[j];
			while (ll_i != tmp) {
				j++;
				tmp2 = tmp;
				tmp = pos[j];
				pos[j] = tmp2;
			};
			pos[0] = tmp;
			s->selectorMtf[i] = j;
		}
	};

	/*--- Assign actual codes for the tables. --*/
	for (t = 0; t < nGroups; t++) {
		minLen = 32;
		maxLen = 0;
		for (i = 0; i < alphaSize; i++) {
			if (s->len[t][i] > maxLen) maxLen = s->len[t][i];
			if (s->len[t][i] < minLen) minLen = s->len[t][i];
		}
		AssertH(!(maxLen > 17 /*20*/), 3004);
		AssertH(!(minLen < 1), 3005);
		BZ2_hbAssignCodes(&(s->code[t][0]), &(s->len[t][0]), minLen, maxLen, alphaSize);
	}

	/*--- Transmit the mapping table. ---*/
	{
		/* bbox: optimized a bit more than in bzip2 */
		int inUse16 = 0;
		for (i = 0; i < 16; i++) {
			if (sizeof(long) <= 4) {
				inUse16 = inUse16*2 +
					((*(bb__aliased_uint32_t*)&(s->inUse[i * 16 + 0])
					| *(bb__aliased_uint32_t*)&(s->inUse[i * 16 + 4])
					| *(bb__aliased_uint32_t*)&(s->inUse[i * 16 + 8])
					| *(bb__aliased_uint32_t*)&(s->inUse[i * 16 + 12])) != 0);
			} else { /* Our CPU can do better */
				inUse16 = inUse16*2 +
					((*(bb__aliased_uint64_t*)&(s->inUse[i * 16 + 0])
					| *(bb__aliased_uint64_t*)&(s->inUse[i * 16 + 8])) != 0);
			}
		}

		bsW(s, 16, inUse16);

		inUse16 <<= (sizeof(int)*8 - 16); /* move 15th bit into sign bit */
		for (i = 0; i < 16; i++) {
			if (inUse16 < 0) {
				unsigned v16 = 0;
				for (j = 0; j < 16; j++)
					v16 = v16*2 + s->inUse[i * 16 + j];
				bsW(s, 16, v16);
			}
			inUse16 <<= 1;
		}
	}

	/*--- Now the selectors. ---*/
	bsW(s, 3, nGroups);
	bsW(s, 15, nSelectors);
	for (i = 0; i < nSelectors; i++) {
		for (j = 0; j < s->selectorMtf[i]; j++)
			bsW(s, 1, 1);
		bsW(s, 1, 0);
	}

	/*--- Now the coding tables. ---*/
	for (t = 0; t < nGroups; t++) {
		int32_t curr = s->len[t][0];
		bsW(s, 5, curr);
		for (i = 0; i < alphaSize; i++) {
			while (curr < s->len[t][i]) { bsW(s, 2, 2); curr++; /* 10 */ };
			while (curr > s->len[t][i]) { bsW(s, 2, 3); curr--; /* 11 */ };
			bsW(s, 1, 0);
		}
	}

	/*--- And finally, the block data proper ---*/
	selCtr = 0;
	gs = 0;
	while (1) {
		if (gs >= s->nMTF)
			break;
		ge = gs + BZ_G_SIZE - 1;
		if (ge >= s->nMTF)
			ge = s->nMTF-1;
		AssertH(s->selector[selCtr] < nGroups, 3006);

/* Costs 1300 bytes and is _slower_ (on Intel Core 2) */
#if 0
		if (nGroups == 6 && 50 == ge-gs+1) {
			/*--- fast track the common case ---*/
			uint16_t mtfv_i;
			uint8_t* s_len_sel_selCtr  = &(s->len[s->selector[selCtr]][0]);
			int32_t* s_code_sel_selCtr = &(s->code[s->selector[selCtr]][0]);
#define BZ_ITAH(nn) \
	mtfv_i = mtfv[gs+(nn)]; \
	bsW(s, s_len_sel_selCtr[mtfv_i], s_code_sel_selCtr[mtfv_i])
			BZ_ITAH(0);  BZ_ITAH(1);  BZ_ITAH(2);  BZ_ITAH(3);  BZ_ITAH(4);
			BZ_ITAH(5);  BZ_ITAH(6);  BZ_ITAH(7);  BZ_ITAH(8);  BZ_ITAH(9);
			BZ_ITAH(10); BZ_ITAH(11); BZ_ITAH(12); BZ_ITAH(13); BZ_ITAH(14);
			BZ_ITAH(15); BZ_ITAH(16); BZ_ITAH(17); BZ_ITAH(18); BZ_ITAH(19);
			BZ_ITAH(20); BZ_ITAH(21); BZ_ITAH(22); BZ_ITAH(23); BZ_ITAH(24);
			BZ_ITAH(25); BZ_ITAH(26); BZ_ITAH(27); BZ_ITAH(28); BZ_ITAH(29);
			BZ_ITAH(30); BZ_ITAH(31); BZ_ITAH(32); BZ_ITAH(33); BZ_ITAH(34);
			BZ_ITAH(35); BZ_ITAH(36); BZ_ITAH(37); BZ_ITAH(38); BZ_ITAH(39);
			BZ_ITAH(40); BZ_ITAH(41); BZ_ITAH(42); BZ_ITAH(43); BZ_ITAH(44);
			BZ_ITAH(45); BZ_ITAH(46); BZ_ITAH(47); BZ_ITAH(48); BZ_ITAH(49);
#undef BZ_ITAH
			gs = ge+1;
		} else
#endif
		{
			/*--- slow version which correctly handles all situations ---*/
			/* code is bit bigger, but moves multiply out of the loop */
			uint8_t* s_len_sel_selCtr  = &(s->len [s->selector[selCtr]][0]);
			int32_t* s_code_sel_selCtr = &(s->code[s->selector[selCtr]][0]);
			while (gs <= ge) {
				bsW(s,
					s_len_sel_selCtr[mtfv[gs]],
					s_code_sel_selCtr[mtfv[gs]]
				);
				gs++;
			}
			/* already is: gs = ge+1; */
		}
		selCtr++;
	}
	AssertH(selCtr == nSelectors, 3007);
#undef code
#undef rfreq
#undef len_pack
}


/*---------------------------------------------------*/
static
void BZ2_compressBlock(EState* s, int is_last_block)
{
	if (s->nblock > 0) {
		BZ_FINALISE_CRC(s->blockCRC);
		s->combinedCRC = (s->combinedCRC << 1) | (s->combinedCRC >> 31);
		s->combinedCRC ^= s->blockCRC;
		if (s->blockNo > 1)
			s->numZ = 0;

		BZ2_blockSort(s);
	}

	s->zbits = &((uint8_t*)s->arr2)[s->nblock];

	/*-- If this is the first block, create the stream header. --*/
	if (s->blockNo == 1) {
		BZ2_bsInitWrite(s);
		/*bsPutU8(s, BZ_HDR_B);*/
		/*bsPutU8(s, BZ_HDR_Z);*/
		/*bsPutU8(s, BZ_HDR_h);*/
		/*bsPutU8(s, BZ_HDR_0 + s->blockSize100k);*/
		bsPutU32(s, BZ_HDR_BZh0 + s->blockSize100k);
	}

	if (s->nblock > 0) {
		/*bsPutU8(s, 0x31);*/
		/*bsPutU8(s, 0x41);*/
		/*bsPutU8(s, 0x59);*/
		/*bsPutU8(s, 0x26);*/
		bsPutU32(s, 0x31415926);
		/*bsPutU8(s, 0x53);*/
		/*bsPutU8(s, 0x59);*/
		bsPutU16(s, 0x5359);

		/*-- Now the block's CRC, so it is in a known place. --*/
		bsPutU32(s, s->blockCRC);

		/*
		 * Now a single bit indicating (non-)randomisation.
		 * As of version 0.9.5, we use a better sorting algorithm
		 * which makes randomisation unnecessary.  So always set
		 * the randomised bit to 'no'.  Of course, the decoder
		 * still needs to be able to handle randomised blocks
		 * so as to maintain backwards compatibility with
		 * older versions of bzip2.
		 */
		bsW(s, 1, 0);

		bsW(s, 24, s->origPtr);
		generateMTFValues(s);
		sendMTFValues(s);
	}

	/*-- If this is the last block, add the stream trailer. --*/
	if (is_last_block) {
		/*bsPutU8(s, 0x17);*/
		/*bsPutU8(s, 0x72);*/
		/*bsPutU8(s, 0x45);*/
		/*bsPutU8(s, 0x38);*/
		bsPutU32(s, 0x17724538);
		/*bsPutU8(s, 0x50);*/
		/*bsPutU8(s, 0x90);*/
		bsPutU16(s, 0x5090);
		bsPutU32(s, s->combinedCRC);
		bsFinishWrite(s);
	}
}


/*-------------------------------------------------------------*/
/*--- end                                        compress.c ---*/
/*-------------------------------------------------------------*/
