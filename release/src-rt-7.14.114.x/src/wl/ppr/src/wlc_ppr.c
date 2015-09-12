/*
 *  PHY module Power-per-rate API. Provides interface functions and definitions for
 * ppr structure for use containing regulatory and board limits and tx power targets.
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: $
 */

#if defined(__NetBSD__) || defined(__FreeBSD__)
#if defined(_KERNEL)
#include <wlc_cfg.h>
#endif /* defined(_KERNEL) */
#endif /* defined(__NetBSD__) || defined(__FreeBSD__) */

#include <typedefs.h>
#include <bcmendian.h>
#include <bcmwifi_channels.h>
#include <wlc_ppr.h>

#ifndef BCMDRIVER

#ifndef WL_BEAMFORMING
#define WL_BEAMFORMING /* enable TxBF definitions for utility code */
#endif

#ifndef bcopy
#include <string.h>
#include <stdlib.h>
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#endif

#ifndef ASSERT
#define ASSERT(exp)	do {} while (0)
#endif
#endif /* BCMDRIVER */

/* ppr local TXBF_ENAB() macro because wlc->pub struct is not accessible */
#ifdef WL_BEAMFORMING
#if defined(WLTXBF_DISABLED)
#define PPR_TXBF_ENAB()	(0)
#else
#define PPR_TXBF_ENAB()	(1)
#endif
#else
#define PPR_TXBF_ENAB()	(0)
#endif /* WL_BEAMFORMING */

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

#define PPR_SERIALIZATION_VER 2

/* ppr deserialization header */
typedef BWL_PRE_PACKED_STRUCT struct ppr_deser_header {
	uint8  version;
	uint8  bw;
	uint16 per_band_size;
	uint32 flags;
	uint16 chain3size; /* ppr data size of 3 Tx chains, needed in deserialisation process */
} BWL_POST_PACKED_STRUCT ppr_deser_header_t;


typedef BWL_PRE_PACKED_STRUCT struct ppr_ser_mem_flag {
	uint32 magic_word;
	uint32 flag;
} BWL_POST_PACKED_STRUCT ppr_ser_mem_flag_t;


#define WLC_TXPWR_DB_FACTOR 4 /* conversion for phy txpwr cacluations that use .25 dB units */


/* QDB() macro takes a dB value and converts to a quarter dB value */
#ifdef QDB
#undef QDB
#endif
#define QDB(n) ((n) * WLC_TXPWR_DB_FACTOR)


/* Flag bits in serialization/deserialization */
#define PPR_MAX_TX_CHAIN_MASK	0x00000003 /* mask of Tx chains */
#define PPR_BEAMFORMING			0x00000004 /* bit indicates BF is on */
#define PPR_SER_MEM_WORD		0xBEEFC0FF /* magic word indicates serialization start */


/* size of serialization header */
#define SER_HDR_LEN    sizeof(ppr_deser_header_t)


/* Per band tx powers */
typedef BWL_PRE_PACKED_STRUCT struct pprpb {
	/* start of 20MHz tx power limits */
	int8 p_1x1dsss[WL_RATESET_SZ_DSSS];			/* Legacy CCK/DSSS */
	int8 p_1x1ofdm[WL_RATESET_SZ_OFDM]; 		/* 20 MHz Legacy OFDM transmission */
	int8 p_1x1vhtss1[WL_RATESET_SZ_VHT_MCS];	/* 8HT/10VHT pwrs starting at 1x1mcs0 */
#if (PPR_MAX_TX_CHAINS > 1)
	int8 p_1x2dsss[WL_RATESET_SZ_DSSS];			/* Legacy CCK/DSSS */
	int8 p_1x2cdd_ofdm[WL_RATESET_SZ_OFDM];		/* 20 MHz Legacy OFDM CDD transmission */
	int8 p_1x2cdd_vhtss1[WL_RATESET_SZ_VHT_MCS]; /* 8HT/10VHT pwrs starting at 1x2cdd_mcs0 */
	int8 p_2x2stbc_vhtss1[WL_RATESET_SZ_VHT_MCS]; /* 8HT/10VHT pwrs starting at 2x2stbc_mcs0 */
	int8 p_2x2vhtss2[WL_RATESET_SZ_VHT_MCS];	/* 8HT/10VHT pwrs starting at 2x2sdm_mcs8 */
#if (PPR_MAX_TX_CHAINS > 2)
	int8 p_1x3dsss[WL_RATESET_SZ_DSSS];			/* Legacy CCK/DSSS */
	int8 p_1x3cdd_ofdm[WL_RATESET_SZ_OFDM];		/* 20 MHz Legacy OFDM CDD transmission */
	int8 p_1x3cdd_vhtss1[WL_RATESET_SZ_VHT_MCS]; /* 8HT/10VHT pwrs starting at 1x3cdd_mcs0 */
	int8 p_2x3stbc_vhtss1[WL_RATESET_SZ_VHT_MCS]; /* 8HT/10VHT pwrs starting at 2x3stbc_mcs0 */
	int8 p_2x3vhtss2[WL_RATESET_SZ_VHT_MCS]; /* 8HT/10VHT pwrs starting at 2x3sdm_mcs8 spexp1 */
	int8 p_3x3vhtss3[WL_RATESET_SZ_VHT_MCS]; /* 8HT/10VHT pwrs starting at 3x3sdm_mcs16 */
#endif

#ifdef WL_BEAMFORMING
	int8 p_1x2txbf_ofdm[WL_RATESET_SZ_OFDM];	/* 20 MHz Legacy OFDM TXBF transmission */
	int8 p_1x2txbf_vhtss1[WL_RATESET_SZ_VHT_MCS]; /* 8HT/10VHT pwrs starting at 1x2txbf_mcs0 */
	int8 p_2x2txbf_vhtss2[WL_RATESET_SZ_VHT_MCS]; /* 8HT/10VHT pwrs starting at 2x2txbf_mcs8 */
#if (PPR_MAX_TX_CHAINS > 2)
	int8 p_1x3txbf_ofdm[WL_RATESET_SZ_OFDM];	/* 20 MHz Legacy OFDM TXBF transmission */
	int8 p_1x3txbf_vhtss1[WL_RATESET_SZ_VHT_MCS]; /* 8HT/10VHT pwrs starting at 1x3txbf_mcs0 */
	int8 p_2x3txbf_vhtss2[WL_RATESET_SZ_VHT_MCS]; /* 8HT/10VHT pwrs starting at 2x3txbf_mcs8 */
	int8 p_3x3txbf_vhtss3[WL_RATESET_SZ_VHT_MCS]; /* 8HT/10VHT pwrs starting at 3x3txbf_mcs16 */
#endif
#endif /* WL_BEAMFORMING */
#endif /* PPR_MAX_TX_CHAINS > 1 */
} BWL_POST_PACKED_STRUCT pprpbw_t;


#define PPR_CHAIN1_FIRST OFFSETOF(pprpbw_t, p_1x1dsss)
#define PPR_CHAIN1_END   (OFFSETOF(pprpbw_t, p_1x1vhtss1) + sizeof(((pprpbw_t *)0)->p_1x1vhtss1))
#define PPR_CHAIN1_SIZE  PPR_CHAIN1_END
#if (PPR_MAX_TX_CHAINS > 1)
#define PPR_CHAIN2_FIRST OFFSETOF(pprpbw_t, p_1x2dsss)
#define PPR_CHAIN2_FIRST_MCS OFFSETOF(pprpbw_t, p_1x2cdd_vhtss1)
#define PPR_CHAIN2_END   (OFFSETOF(pprpbw_t, p_2x2vhtss2) + sizeof(((pprpbw_t *)0)->p_2x2vhtss2))
#define PPR_CHAIN2_SIZE  (PPR_CHAIN2_END - PPR_CHAIN2_FIRST)
#define PPR_CHAIN2_MCS_SIZE  (PPR_CHAIN2_END - PPR_CHAIN2_FIRST_MCS)
#if (PPR_MAX_TX_CHAINS > 2)
#define PPR_CHAIN3_FIRST OFFSETOF(pprpbw_t, p_1x3dsss)
#define PPR_CHAIN3_FIRST_MCS OFFSETOF(pprpbw_t, p_1x3cdd_vhtss1)
#define PPR_CHAIN3_END   (OFFSETOF(pprpbw_t, p_3x3vhtss3) + sizeof(((pprpbw_t *)0)->p_3x3vhtss3))
#define PPR_CHAIN3_SIZE  (PPR_CHAIN3_END - PPR_CHAIN3_FIRST)
#define PPR_CHAIN3_MCS_SIZE  (PPR_CHAIN3_END - PPR_CHAIN3_FIRST_MCS)
#endif

#ifdef WL_BEAMFORMING
#define PPR_BF_CHAIN2_FIRST OFFSETOF(pprpbw_t, p_1x2txbf_ofdm)
#define PPR_BF_CHAIN2_FIRST_MCS OFFSETOF(pprpbw_t, p_1x2txbf_vhtss1)
#define PPR_BF_CHAIN2_END   (OFFSETOF(pprpbw_t, p_2x2txbf_vhtss2) + \
	sizeof(((pprpbw_t *)0)->p_2x2txbf_vhtss2))
#define PPR_BF_CHAIN2_SIZE  (PPR_BF_CHAIN2_END - PPR_BF_CHAIN2_FIRST)
#define PPR_BF_CHAIN2_MCS_SIZE  (PPR_BF_CHAIN2_END - PPR_BF_CHAIN2_FIRST_MCS)
#if (PPR_MAX_TX_CHAINS > 2)
#define PPR_BF_CHAIN3_FIRST OFFSETOF(pprpbw_t, p_1x3txbf_ofdm)
#define PPR_BF_CHAIN3_FIRST_MCS OFFSETOF(pprpbw_t, p_1x3txbf_vhtss1)
#define PPR_BF_CHAIN3_END   (OFFSETOF(pprpbw_t, p_3x3txbf_vhtss3) + \
	sizeof(((pprpbw_t *)0)->p_3x3txbf_vhtss3))
#define PPR_BF_CHAIN3_SIZE  (PPR_BF_CHAIN3_END - PPR_BF_CHAIN3_FIRST)
#define PPR_BF_CHAIN3_MCS_SIZE  (PPR_BF_CHAIN3_END - PPR_BF_CHAIN3_FIRST_MCS)
#endif

#endif /* WL_BEAMFORMING */
#endif /* PPR_MAX_TX_CHAINS > 1 */


#define PPR_BW_MAX WL_TX_BW_80 /* Maximum supported bandwidth */

/* Structure to contain ppr values for a 20MHz channel */
typedef BWL_PRE_PACKED_STRUCT struct ppr_bw_20 {
	/* 20MHz tx power limits */
	pprpbw_t b20;
} BWL_POST_PACKED_STRUCT ppr_bw_20_t;


/* Structure to contain ppr values for a 40MHz channel */
typedef BWL_PRE_PACKED_STRUCT struct ppr_bw_40 {
	/* 40MHz tx power limits */
	pprpbw_t b40;
	/* 20in40MHz tx power limits */
	pprpbw_t b20in40;
} BWL_POST_PACKED_STRUCT ppr_bw_40_t;


/* Structure to contain ppr values for an 80MHz channel */
typedef BWL_PRE_PACKED_STRUCT struct ppr_bw_80 {
	/* 80MHz tx power limits */
	pprpbw_t b80;
	/* 20in80MHz tx power limits */
	pprpbw_t b20in80;
	/* 40in80MHz tx power limits */
	pprpbw_t b40in80;
} BWL_POST_PACKED_STRUCT ppr_bw_80_t;

/* Structure to contain ppr values for a 160MHz channel */
typedef BWL_PRE_PACKED_STRUCT struct ppr_bw_160 {
	/* 160MHz tx power limits */
	pprpbw_t b160;
	/* 20in160MHz tx power limits */
	pprpbw_t b20in160;
	/* 40in160MHz tx power limits */
	pprpbw_t b40in160;
	/* 80in160MHz tx power limits */
	pprpbw_t b80in160;
} BWL_POST_PACKED_STRUCT ppr_bw_160_t;


/* Structure to contain ppr values for an 80+80MHz channel */
typedef BWL_PRE_PACKED_STRUCT struct ppr_bw_8080 {
	/* 80+80MHz chan1 tx power limits */
	pprpbw_t b80ch1;
	/* 80+80MHz chan2 tx power limits */
	pprpbw_t b80ch2;
	/* 80in80+80MHz (chan1 subband) tx power limits */
	pprpbw_t b80in8080;
	/* 20in80+80MHz (chan1 subband) tx power limits */
	pprpbw_t b20in8080;
	/* 40in80+80MHz (chan1 subband) tx power limits */
	pprpbw_t b40in8080;
} BWL_POST_PACKED_STRUCT ppr_bw_8080_t;

/*
 * This is the initial implementation of the structure we're hiding. It is sized to contain only
 * the set of powers it requires, so the union is not necessarily the size of the largest member.
 */

BWL_PRE_PACKED_STRUCT struct ppr {
	wl_tx_bw_t ch_bw;

	BWL_PRE_PACKED_STRUCT union {
		ppr_bw_20_t ch20;
		ppr_bw_40_t ch40;
		ppr_bw_80_t ch80;
		ppr_bw_160_t ch160;
		ppr_bw_8080_t ch8080;
	} ppr_bw;
} BWL_POST_PACKED_STRUCT;

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>


/* Returns a flag of ppr conditions (chains, txbf etc.) */
static uint32 ppr_get_flag(void)
{
	uint32 flag = 0;
	flag  |= PPR_MAX_TX_CHAINS & PPR_MAX_TX_CHAIN_MASK;
#if PPR_MAX_TX_CHAINS > 1
	if (PPR_TXBF_ENAB()) {
		flag  |= PPR_BEAMFORMING;
	}
#endif
	return flag;
}

static uint16 ppr_ser_size_per_band(uint32 flags)
{
	uint16 ret = PPR_CHAIN1_SIZE; /* at least 1 chain rates should be there */
	uint8 chain   = flags & PPR_MAX_TX_CHAIN_MASK;
	bool bf      = (flags & PPR_BEAMFORMING) != 0;
	BCM_REFERENCE(chain);
	BCM_REFERENCE(bf);
#if (PPR_MAX_TX_CHAINS > 1)
	if (chain > 1) {
		ret += PPR_CHAIN2_SIZE;
	}
#if (PPR_MAX_TX_CHAINS > 2)
	if (chain > 2) {
		ret += PPR_CHAIN3_SIZE;
	}
#endif

#ifdef WL_BEAMFORMING
	if (PPR_TXBF_ENAB() && bf) {
		ret += PPR_BF_CHAIN2_SIZE;
	}
#if (PPR_MAX_TX_CHAINS > 2)
	if (PPR_TXBF_ENAB() && bf && chain > 2) {
		ret += PPR_BF_CHAIN3_SIZE;
	}
#endif
#endif /* WL_BEAMFORMING */
#endif /* PPR_MAX_TX_CHAINS > 1 */
	return ret;
}

/* Returns the number of bands for a specific bandwidth bw */
static uint ppr_bands_by_bw(wl_tx_bw_t bw)
{
	switch (bw) {
	case WL_TX_BW_20:
		return sizeof(ppr_bw_20_t)/sizeof(pprpbw_t);
	case WL_TX_BW_40:
		return sizeof(ppr_bw_40_t)/sizeof(pprpbw_t);
	case WL_TX_BW_80:
		return sizeof(ppr_bw_80_t)/sizeof(pprpbw_t);
	case WL_TX_BW_160:
		return sizeof(ppr_bw_160_t)/sizeof(pprpbw_t);
	case WL_TX_BW_8080:
		return sizeof(ppr_bw_8080_t)/sizeof(pprpbw_t);
	default:
		ASSERT(0);
		return 0;
	}
}

/* Return the required serialization size based on the flag field. */
static uint ppr_ser_size_by_flag(uint32 flag, wl_tx_bw_t bw)
{
	return ppr_ser_size_per_band(flag) * ppr_bands_by_bw(bw);
}

#define COPY_PPR_TOBUF(x, y) do { bcopy(&pprbuf[x], *buf, y); \
	*buf += y; ret += y; } while (0);


/* Serialize ppr data of a bandwidth into the given buffer */
static uint ppr_serialize_block(const uint8* pprbuf, uint8** buf, uint32 serflag)
{
	uint ret = 0;
#if (PPR_MAX_TX_CHAINS > 1)
	uint chain   = serflag & PPR_MAX_TX_CHAIN_MASK; /* chain number in serialized block */
	bool bf      = (serflag & PPR_BEAMFORMING) != 0;
#endif

	COPY_PPR_TOBUF(PPR_CHAIN1_FIRST, PPR_CHAIN1_SIZE);
#if (PPR_MAX_TX_CHAINS > 1)
	BCM_REFERENCE(bf);
	if (chain > 1) {
		COPY_PPR_TOBUF(PPR_CHAIN2_FIRST, PPR_CHAIN2_SIZE);
	}
#if (PPR_MAX_TX_CHAINS > 2)
	if (chain > 2) {
		COPY_PPR_TOBUF(PPR_CHAIN3_FIRST, PPR_CHAIN3_SIZE);
	}
#endif
#ifdef WL_BEAMFORMING
	if (PPR_TXBF_ENAB() && bf) {
		COPY_PPR_TOBUF(PPR_BF_CHAIN2_FIRST, PPR_BF_CHAIN2_SIZE);
	}
#if (PPR_MAX_TX_CHAINS > 2)
	if (PPR_TXBF_ENAB() && bf && chain > 2) {
		COPY_PPR_TOBUF(PPR_BF_CHAIN3_FIRST, PPR_BF_CHAIN3_SIZE);
	}
#endif
#endif /* WL_BEAMFORMING */
#endif /* (PPR_MAX_TX_CHAINS > 1) */
	return ret;
}


/* Serialize ppr data of each bandwidth into the given buffer, returns bytes copied */
static uint ppr_serialize_data(const ppr_t *pprptr, uint8* buf, uint32 serflag)
{
	uint i;
	uint bands;
	const uint8* pprbuf;

	uint ret = sizeof(ppr_deser_header_t);
	ppr_deser_header_t* header = (ppr_deser_header_t*)buf;
	ASSERT(pprptr && buf);
	header->version = PPR_SERIALIZATION_VER;
	header->bw      = (uint8)pprptr->ch_bw;
	header->flags   = HTON32(ppr_get_flag());
	header->per_band_size	= HTON16(ppr_ser_size_per_band(serflag));
#if (PPR_MAX_TX_CHAINS > 2)
	header->chain3size = HTON16(PPR_CHAIN3_SIZE);
#else
	header->chain3size = 0;
#endif

	buf += sizeof(*header);

	bands = ppr_bands_by_bw(header->bw);
	pprbuf = (const uint8*)&pprptr->ppr_bw;
	for (i = 0; i < bands; i++) {
		ret += ppr_serialize_block(pprbuf, &buf, serflag);
		pprbuf += sizeof(pprpbw_t); /* Jump to next band */
	}

	return ret;
}


/* Copy serialized ppr data of a bandwidth */
static void
ppr_copy_serdata(uint8* pobuf, const uint8** inbuf, uint32 flag, uint16 per_band_size,
	uint16 chain3size)
{
	uint chain   = flag & PPR_MAX_TX_CHAIN_MASK;
	bool bf      = (flag & PPR_BEAMFORMING) != 0;
	uint16 len   = PPR_CHAIN1_SIZE;
	BCM_REFERENCE(chain);
	BCM_REFERENCE(bf);
	BCM_REFERENCE(chain3size);
	bcopy(*inbuf, pobuf, PPR_CHAIN1_SIZE);
	*inbuf += PPR_CHAIN1_SIZE;
#if (PPR_MAX_TX_CHAINS > 1)
	if (chain > 1) {
		bcopy(*inbuf, &pobuf[PPR_CHAIN2_FIRST], PPR_CHAIN2_SIZE);
		*inbuf += PPR_CHAIN2_SIZE;
		len += PPR_CHAIN2_SIZE;
	}
#if (PPR_MAX_TX_CHAINS > 2)
	if (chain > 2) {
		bcopy(*inbuf, &pobuf[PPR_CHAIN3_FIRST], PPR_CHAIN3_SIZE);
		*inbuf += PPR_CHAIN3_SIZE;
		len += PPR_CHAIN3_SIZE;
	}
#else
	if (chain > 2) {
		 *inbuf += chain3size;
		 len += chain3size;
	}
#endif

#ifdef WL_BEAMFORMING
	if (PPR_TXBF_ENAB() && bf) {
		bcopy(*inbuf, &pobuf[PPR_BF_CHAIN2_FIRST], PPR_BF_CHAIN2_SIZE);
		*inbuf += PPR_BF_CHAIN2_SIZE;
		len += PPR_BF_CHAIN2_SIZE;
	}
#if (PPR_MAX_TX_CHAINS > 2)
	if (PPR_TXBF_ENAB() && bf && chain > 2) {
		bcopy(*inbuf, &pobuf[PPR_BF_CHAIN3_FIRST], PPR_BF_CHAIN3_SIZE);
		*inbuf += PPR_BF_CHAIN3_SIZE;
		len += PPR_BF_CHAIN3_SIZE;
	}
#endif
#endif  /* WL_BEAMFORMING */
#endif /* (PPR_MAX_TX_CHAINS > 1) */
	if (len < per_band_size) {
		 *inbuf += (per_band_size - len);
	}
}


/* Deserialize data into a ppr_t structure */
static void
ppr_deser_cpy(ppr_t* pptr, const uint8* inbuf, uint32 flag, wl_tx_bw_t bw, uint16 per_band_size,
	uint16 chain3size)
{
	uint i;
	uint bands;
	uint8* pobuf;

	pptr->ch_bw = bw;
	bands = ppr_bands_by_bw(bw);
	pobuf = (uint8*)&pptr->ppr_bw;
	for (i = 0; i < bands; i++) {
		ppr_copy_serdata(pobuf, &inbuf, flag, per_band_size, chain3size);
		pobuf += sizeof(pprpbw_t); /* Jump to next band */
	}
}


/* Get a pointer to the power values for a given channel bandwidth */
static pprpbw_t* ppr_get_bw_powers_20(ppr_t* p, wl_tx_bw_t bw)
{
	pprpbw_t* pwrs = NULL;

	if (bw == WL_TX_BW_20)
		pwrs = &p->ppr_bw.ch20.b20;
	/* else */
	/*   ASSERT(0); */
	return pwrs;
}


/* Get a pointer to the power values for a given channel bandwidth */
static pprpbw_t* ppr_get_bw_powers_40(ppr_t* p, wl_tx_bw_t bw)
{
	pprpbw_t* pwrs = NULL;

	switch (bw) {
	case WL_TX_BW_40:
		pwrs = &p->ppr_bw.ch40.b40;
	break;
	case WL_TX_BW_20:

	case WL_TX_BW_20IN40:
		pwrs = &p->ppr_bw.ch40.b20in40;
	break;
	default:
		/* ASSERT(0); */
	break;
	}
	return pwrs;
}


/* Get a pointer to the power values for a given channel bandwidth */
static pprpbw_t* ppr_get_bw_powers_80(ppr_t* p, wl_tx_bw_t bw)
{
	pprpbw_t* pwrs = NULL;

	switch (bw) {
	case WL_TX_BW_80:
		pwrs = &p->ppr_bw.ch80.b80;
	break;
	case WL_TX_BW_20:
	case WL_TX_BW_20IN40:
	case WL_TX_BW_20IN80:
		pwrs = &p->ppr_bw.ch80.b20in80;
	break;
	case WL_TX_BW_40:
	case WL_TX_BW_40IN80:
		pwrs = &p->ppr_bw.ch80.b40in80;
	break;
	default:
		/* ASSERT(0); */
	break;
	}
	return pwrs;
}

/* Get a pointer to the power values for a given channel bandwidth */
static pprpbw_t* ppr_get_bw_powers_160(ppr_t* p, wl_tx_bw_t bw)
{
	pprpbw_t* pwrs = NULL;

	switch (bw) {
	case WL_TX_BW_160:
		pwrs = &p->ppr_bw.ch160.b160;
	break;
	case WL_TX_BW_20:
	case WL_TX_BW_20IN40:
	case WL_TX_BW_20IN80:
	case WL_TX_BW_20IN160:
		pwrs = &p->ppr_bw.ch160.b20in160;
	break;
	case WL_TX_BW_40:
	case WL_TX_BW_40IN80:
	case WL_TX_BW_40IN160:
		pwrs = &p->ppr_bw.ch160.b40in160;
	break;
	case WL_TX_BW_80:
	case WL_TX_BW_80IN160:
		pwrs = &p->ppr_bw.ch160.b80in160;
	break;
	default:
		/* ASSERT(0); */
	break;
	}
	return pwrs;
}


/* Get a pointer to the power values for a given channel bandwidth */
static pprpbw_t* ppr_get_bw_powers_8080(ppr_t* p, wl_tx_bw_t bw)
{
	pprpbw_t* pwrs = NULL;

	switch (bw) {
	case WL_TX_BW_8080:
		pwrs = &p->ppr_bw.ch8080.b80ch1;
	break;
	case WL_TX_BW_8080CHAN2:
		pwrs = &p->ppr_bw.ch8080.b80ch2;
	break;
	case WL_TX_BW_20:
	case WL_TX_BW_20IN8080:
		pwrs = &p->ppr_bw.ch8080.b20in8080;
	break;
	case WL_TX_BW_40:
	case WL_TX_BW_40IN8080:
		pwrs = &p->ppr_bw.ch8080.b40in8080;
	break;
	case WL_TX_BW_80:
	case WL_TX_BW_80IN8080:
		pwrs = &p->ppr_bw.ch8080.b80in8080;
	break;
	default:
		/* ASSERT(0); */
	break;
	}
	return pwrs;
}

typedef pprpbw_t* (*wlc_ppr_get_bw_pwrs_fn_t)(ppr_t* p, wl_tx_bw_t bw);

typedef struct {
	wl_tx_bw_t ch_bw;		/* Bandwidth of the channel for which powers are stored */
	/* Function to retrieve the powers for the requested bandwidth */
	wlc_ppr_get_bw_pwrs_fn_t fn;
} wlc_ppr_get_bw_pwrs_pair_t;


static const wlc_ppr_get_bw_pwrs_pair_t ppr_get_bw_pwrs_fn[] = {
	{WL_TX_BW_20, ppr_get_bw_powers_20},
	{WL_TX_BW_40, ppr_get_bw_powers_40},
	{WL_TX_BW_80, ppr_get_bw_powers_80},
	{WL_TX_BW_160, ppr_get_bw_powers_160},
	{WL_TX_BW_8080, ppr_get_bw_powers_8080}
};


/* Get a pointer to the power values for a given channel bandwidth */
static pprpbw_t* ppr_get_bw_powers(ppr_t* p, wl_tx_bw_t bw)
{
	uint32 i;

	if (p == NULL) {
		return NULL;
	}

	for (i = 0; i < (int)ARRAYSIZE(ppr_get_bw_pwrs_fn); i++) {
		if (ppr_get_bw_pwrs_fn[i].ch_bw == p->ch_bw)
			return ppr_get_bw_pwrs_fn[i].fn(p, bw);
	}

	ASSERT(0);
	return NULL;
}


/*
 * Rate group power finder functions: ppr_get_xxx_group()
 * To preserve the opacity of the PPR struct, even inside the API we try to limit knowledge of
 * its details. Almost all API functions work on the powers for individual rate groups, rather than
 * directly accessing the struct. Once the section of the structure corresponding to the bandwidth
 * has been identified using ppr_get_bw_powers(), the ppr_get_xxx_group() functions use knowledge
 * of the number of spatial streams, the number of tx chains, and the expansion mode to return a
 * pointer to the required group of power values.
 */

/* Get a pointer to the power values for the given dsss rate group for a given channel bandwidth */
static int8* ppr_get_dsss_group(pprpbw_t* bw_pwrs, wl_tx_chains_t tx_chains)
{
	int8* group_pwrs = NULL;

	switch (tx_chains) {
#if (PPR_MAX_TX_CHAINS > 1)
#if (PPR_MAX_TX_CHAINS > 2)
	case WL_TX_CHAINS_3:
		group_pwrs = bw_pwrs->p_1x3dsss;
		break;
#endif
	case WL_TX_CHAINS_2:
		group_pwrs = bw_pwrs->p_1x2dsss;
		break;
#endif /* PPR_MAX_TX_CHAINS > 1 */
	case WL_TX_CHAINS_1:
		group_pwrs = bw_pwrs->p_1x1dsss;
		break;
	default:
		ASSERT(0);
		break;
	}
	return group_pwrs;
}


/* Get a pointer to the power values for the given ofdm rate group for a given channel bandwidth */
static int8* ppr_get_ofdm_group(pprpbw_t* bw_pwrs, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains)
{
	int8* group_pwrs = NULL;
	BCM_REFERENCE(mode);
	switch (tx_chains) {
#if (PPR_MAX_TX_CHAINS > 1)
#if (PPR_MAX_TX_CHAINS > 2)
	case WL_TX_CHAINS_3:
#ifdef WL_BEAMFORMING
		if (mode == WL_TX_MODE_TXBF)
			group_pwrs = bw_pwrs->p_1x3txbf_ofdm;
		else
#endif
			group_pwrs = bw_pwrs->p_1x3cdd_ofdm;
		break;
#endif /* PPR_MAX_TX_CHAINS > 2 */
	case WL_TX_CHAINS_2:
#ifdef WL_BEAMFORMING
		if (mode == WL_TX_MODE_TXBF)
			group_pwrs = bw_pwrs->p_1x2txbf_ofdm;
		else
#endif
			group_pwrs = bw_pwrs->p_1x2cdd_ofdm;
		break;
#endif /* PPR_MAX_TX_CHAINS > 1 */
	case WL_TX_CHAINS_1:
		group_pwrs = bw_pwrs->p_1x1ofdm;
		break;
	default:
		ASSERT(0);
		break;
	}
	return group_pwrs;
}


/*
 * Tables to provide access to HT/VHT rate group powers. This avoids an ugly nested switch with
 * messy conditional compilation.
 *
 * Access to a given table entry is via table[chains - Nss][mode], except for the Nss3 table, which
 * only has one row, so it can be indexed directly by table[mode].
 *
 * Separate tables are provided for each of Nss1, Nss2 and Nss3 because they are all different
 * sizes. A combined table would be very sparse, and this arrangement also simplifies the
 * conditional compilation.
 *
 * Each row represents a given number of chains, so there's no need for a zero row. Because
 * chains >= Nss is always true, there is no one-chain row for Nss2 and there are no one- or
 * two-chain rows for Nss3. With the tables correctly sized, we can index the rows
 * using [chains - Nss].
 *
 * Then, inside each row, we index by mode:
 * WL_TX_MODE_NONE, WL_TX_MODE_STBC, WL_TX_MODE_CDD, WL_TX_MODE_TXBF.
 */

#define OFFSNONE (-1)

static const int mcs_groups_nss1[PPR_MAX_TX_CHAINS][WL_NUM_TX_MODES] = {
	/* WL_TX_MODE_NONE
	   WL_TX_MODE_STBC
	   WL_TX_MODE_CDD
	   WL_TX_MODE_TXBF
	*/
	/* 1 chain */
	{OFFSETOF(pprpbw_t, p_1x1vhtss1),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 1)
	/* 2 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x2cdd_vhtss1),
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x3cdd_vhtss1),
	OFFSNONE}
#endif
#endif /* PPR_MAX_TX_CHAINS > 1 */
};

#ifdef WL_BEAMFORMING
/* mcs group with TXBF data */
static const int mcs_groups_nss1_txbf[PPR_MAX_TX_CHAINS][WL_NUM_TX_MODES] = {
	/* WL_TX_MODE_NONE
	   WL_TX_MODE_STBC
	   WL_TX_MODE_CDD
	   WL_TX_MODE_TXBF
	*/
	/* 1 chain */
	{OFFSETOF(pprpbw_t, p_1x1vhtss1),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 1)
	/* 2 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x2cdd_vhtss1),
	OFFSETOF(pprpbw_t, p_1x2txbf_vhtss1)},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x3cdd_vhtss1),
	OFFSETOF(pprpbw_t, p_1x3txbf_vhtss1)}
#endif
#endif /* PPR_MAX_TX_CHAINS > 1 */
};
#endif /* WL_BEAMFORMING */

#if (PPR_MAX_TX_CHAINS > 1)
static const int mcs_groups_nss2[PPR_MAX_TX_CHAINS - 1][WL_NUM_TX_MODES] = {
	/* 2 chain */
	{OFFSETOF(pprpbw_t, p_2x2vhtss2),
	OFFSETOF(pprpbw_t, p_2x2stbc_vhtss1),
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSETOF(pprpbw_t, p_2x3vhtss2),
	OFFSETOF(pprpbw_t, p_2x3stbc_vhtss1),
	OFFSNONE,
	OFFSNONE}
#endif
};

#ifdef WL_BEAMFORMING
/* mcs group with TXBF data */
static const int mcs_groups_nss2_txbf[PPR_MAX_TX_CHAINS - 1][WL_NUM_TX_MODES] = {
	/* 2 chain */
	{OFFSETOF(pprpbw_t, p_2x2vhtss2),
	OFFSETOF(pprpbw_t, p_2x2stbc_vhtss1),
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_2x2txbf_vhtss2)},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSETOF(pprpbw_t, p_2x3vhtss2),
	OFFSETOF(pprpbw_t, p_2x3stbc_vhtss1),
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_2x3txbf_vhtss2)}
#endif
};
#endif /* WL_BEAMFORMING */

#if (PPR_MAX_TX_CHAINS > 2)
static const int mcs_groups_nss3[WL_NUM_TX_MODES] = {
/* 3 chains only */
	OFFSETOF(pprpbw_t, p_3x3vhtss3),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE,
};

#ifdef WL_BEAMFORMING
/* mcs group with TXBF data */
static const int mcs_groups_nss3_txbf[WL_NUM_TX_MODES] = {
/* 3 chains only */
	OFFSETOF(pprpbw_t, p_3x3vhtss3),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_3x3txbf_vhtss3)
};
#endif /* WL_BEAMFORMING */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */

/* Get a pointer to the power values for the given rate group for a given channel bandwidth */
static int8* ppr_get_mcs_group(pprpbw_t* bw_pwrs, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains)
{
	int8* group_pwrs = NULL;
	int offset;

	switch (Nss) {
#if (PPR_MAX_TX_CHAINS > 1)
#if (PPR_MAX_TX_CHAINS > 2)
	case WL_TX_NSS_3:
		if (tx_chains == WL_TX_CHAINS_3) {
#ifdef WL_BEAMFORMING
			if (PPR_TXBF_ENAB()) {
				offset = mcs_groups_nss3_txbf[mode];
			} else
#endif /* WL_BEAMFORMING */
			{
				offset = mcs_groups_nss3[mode];
			}
			if (offset != OFFSNONE) {
				group_pwrs = (int8*)bw_pwrs + offset;
			}
		}
		else
			ASSERT(0);
		break;
#endif /* PPR_MAX_TX_CHAINS > 2 */
	case WL_TX_NSS_2:
		if ((tx_chains >= WL_TX_CHAINS_2) && (tx_chains <= PPR_MAX_TX_CHAINS)) {
#ifdef WL_BEAMFORMING
			if (PPR_TXBF_ENAB()) {
				offset = mcs_groups_nss2_txbf[tx_chains - Nss][mode];
			} else
#endif /* WL_BEAMFORMING */
			{
				offset = mcs_groups_nss2[tx_chains - Nss][mode];
			}
			if (offset != OFFSNONE) {
				group_pwrs = (int8*)bw_pwrs + offset;
			}
		}
		else
			ASSERT(0);
		break;
#endif /* PPR_MAX_TX_CHAINS > 1 */
	case WL_TX_NSS_1:
		if (tx_chains <= PPR_MAX_TX_CHAINS) {
#ifdef WL_BEAMFORMING
			if (PPR_TXBF_ENAB()) {
				offset = mcs_groups_nss1_txbf[tx_chains - Nss][mode];
			} else
#endif /* WL_BEAMFORMING */
			{
				offset = mcs_groups_nss1[tx_chains - Nss][mode];
			}
			if (offset != OFFSNONE) {
				group_pwrs = (int8*)bw_pwrs + offset;
			}
		}
		else
			ASSERT(0);
		break;
	default:
		ASSERT(0);
		break;
	}
	return group_pwrs;
}

/* Size routine for user alloc/dealloc */
static uint32 ppr_pwrs_size(wl_tx_bw_t bw)
{
	uint32 size;

	switch (bw) {
	case WL_TX_BW_20:
		size = sizeof(ppr_bw_20_t);
	break;
	case WL_TX_BW_40:
		size = sizeof(ppr_bw_40_t);
	break;
	case WL_TX_BW_80:
		size = sizeof(ppr_bw_80_t);
	break;
	default:
		ASSERT(0);
		size = 0;
	break;
	}
	return size;
}


/* Initialization routine */
void ppr_init(ppr_t* pprptr, wl_tx_bw_t bw)
{
	memset(pprptr, (int8)WL_RATE_DISABLED, ppr_size(bw));
	pprptr->ch_bw = bw;
}


/* Reinitialization routine for opaque PPR struct */
void ppr_clear(ppr_t* pprptr)
{
	memset((uchar*)&pprptr->ppr_bw, (int8)WL_RATE_DISABLED, ppr_pwrs_size(pprptr->ch_bw));
}


/* Size routine for user alloc/dealloc */
uint32 ppr_size(wl_tx_bw_t bw)
{
	return ppr_pwrs_size(bw) + sizeof(wl_tx_bw_t);
}


/* Size routine for user serialization alloc */
uint32 ppr_ser_size(const ppr_t* pprptr)
{
	return ppr_pwrs_size(pprptr->ch_bw) + SER_HDR_LEN;	/* struct size plus headers */
}


/* Size routine for user serialization alloc */
uint32 ppr_ser_size_by_bw(wl_tx_bw_t bw)
{
	return ppr_pwrs_size(bw) + SER_HDR_LEN;	/* struct size plus headers */
}


/* Constructor routine for opaque PPR struct */
ppr_t* ppr_create(osl_t *osh, wl_tx_bw_t bw)
{
	ppr_t* pprptr;

	ASSERT((bw == WL_TX_BW_20) || (bw == WL_TX_BW_40) || (bw == WL_TX_BW_80));
#ifndef BCMDRIVER
	BCM_REFERENCE(osh);
	if ((pprptr = (ppr_t*)malloc((uint)ppr_size(bw))) != NULL) {
#else
	if ((pprptr = (ppr_t*)MALLOC(osh, (uint)ppr_size(bw))) != NULL) {
#endif
		ppr_init(pprptr, bw);
	}
	return pprptr;
}


/* Init flags in the memory block for serialization, the serializer will check
 * the flag to decide which ppr to be copied
 */
int ppr_init_ser_mem_by_bw(uint8* pbuf, wl_tx_bw_t bw, uint32 len)
{
	ppr_ser_mem_flag_t *pmflag;

	if (pbuf == NULL || ppr_ser_size_by_bw(bw) > len)
		return BCME_BADARG;

	pmflag = (ppr_ser_mem_flag_t *)pbuf;
	pmflag->magic_word = HTON32(PPR_SER_MEM_WORD);
	pmflag->flag   = HTON32(ppr_get_flag());

	/* init the memory */
	memset(pbuf + sizeof(*pmflag), (uint8)WL_RATE_DISABLED, len-sizeof(*pmflag));
	return BCME_OK;
}


int ppr_init_ser_mem(uint8* pbuf, ppr_t * ppr, uint32 len)
{
	return ppr_init_ser_mem_by_bw(pbuf, ppr->ch_bw, len);
}


/* Destructor routine for opaque PPR struct */
void ppr_delete(osl_t *osh, ppr_t* pprptr)
{
	ASSERT((pprptr->ch_bw == WL_TX_BW_20) || (pprptr->ch_bw == WL_TX_BW_40) ||
		(pprptr->ch_bw == WL_TX_BW_80));
#ifndef BCMDRIVER
	BCM_REFERENCE(osh);
	free(pprptr);
#else
	MFREE(osh, pprptr, (uint)ppr_size(pprptr->ch_bw));
#endif
}


/* Type routine for inferring opaque structure size */
wl_tx_bw_t ppr_get_ch_bw(const ppr_t* pprptr)
{
	return pprptr->ch_bw;
}


/* Type routine to get ppr supported maximum bw */
wl_tx_bw_t ppr_get_max_bw(void)
{
	return PPR_BW_MAX;
}


/* Get the dsss values for the given number of tx_chains and 20, 20in40, etc. */
int ppr_get_dsss(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_chains_t tx_chains,
	ppr_dsss_rateset_t* dsss)
{
	pprpbw_t* bw_pwrs;
	const int8* powers;
	int cnt = 0;

	ASSERT(pprptr);
	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = ppr_get_dsss_group(bw_pwrs, tx_chains);
		if (powers != NULL) {
			bcopy(powers, dsss->pwr, sizeof(*dsss));
			cnt = sizeof(*dsss);
		}
	}
	if (cnt == 0) {
		memset(dsss->pwr, (int8)WL_RATE_DISABLED, sizeof(*dsss));
	}
	return cnt;
}


/* Get the ofdm values for the given number of tx_chains and 20, 20in40, etc. */
int ppr_get_ofdm(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_mode_t mode, wl_tx_chains_t tx_chains,
	ppr_ofdm_rateset_t* ofdm)
{
	pprpbw_t* bw_pwrs;
	const int8* powers;
	int cnt = 0;

	ASSERT(pprptr);
	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = ppr_get_ofdm_group(bw_pwrs, mode, tx_chains);
		if (powers != NULL) {
			bcopy(powers, ofdm->pwr, sizeof(*ofdm));
			cnt = sizeof(*ofdm);
		}
	}
	if (cnt == 0) {
		memset(ofdm->pwr, (int8)WL_RATE_DISABLED, sizeof(*ofdm));
	}
	return cnt;
}


/* Get the HT MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_get_ht_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, ppr_ht_mcs_rateset_t* mcs)
{
	pprpbw_t* bw_pwrs;
	const int8* powers;
	int cnt = 0;

	ASSERT(pprptr);
	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			bcopy(powers, mcs->pwr, sizeof(*mcs));
			cnt = sizeof(*mcs);
		}
	}
	if (cnt == 0) {
		memset(mcs->pwr, (int8)WL_RATE_DISABLED, sizeof(*mcs));
	}

	return cnt;
}


/* Get the VHT MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_get_vht_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, ppr_vht_mcs_rateset_t* mcs)
{
	pprpbw_t* bw_pwrs;
	const int8* powers;
	int cnt = 0;

	ASSERT(pprptr);
	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			bcopy(powers, mcs->pwr, sizeof(*mcs));
			cnt = sizeof(*mcs);
		}
	}
	if (cnt == 0) {
		memset(mcs->pwr, (int8)WL_RATE_DISABLED, sizeof(*mcs));
	}
	return cnt;
}


#define TXPPR_TXPWR_MAX 0x7f             /* WLC_TXPWR_MAX */

/* Get the minimum power for a VHT MCS rate specified by Nss, with the given bw and tx chains.
 * Disabled rates are ignored
 */
int ppr_get_vht_mcs_min(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
 wl_tx_chains_t tx_chains, int8* mcs_min)
{
	pprpbw_t* bw_pwrs;
	const int8* powers;
	int result = BCME_ERROR;
	uint i = 0;

	*mcs_min = TXPPR_TXPWR_MAX;

	ASSERT(pprptr);
	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			for (i = 0; i < sizeof(ppr_vht_mcs_rateset_t); i++) {
				/* ignore disabled rates! */
				if (powers[i] != WL_RATE_DISABLED)
					*mcs_min = MIN(*mcs_min, powers[i]);
			}
			result = BCME_OK;
		}
	}
	return result;
}


/* Routines to set target powers per rate in a group */

/* Set the dsss values for the given number of tx_chains and 20, 20in40, etc. */
int ppr_set_dsss(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_chains_t tx_chains,
	const ppr_dsss_rateset_t* dsss)
{
	pprpbw_t* bw_pwrs;
	int8* powers;
	int cnt = 0;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = (int8*)ppr_get_dsss_group(bw_pwrs, tx_chains);
		if (powers != NULL) {
			bcopy(dsss->pwr, powers, sizeof(*dsss));
			cnt = sizeof(*dsss);
		}
	}
	return cnt;
}


/* Set the ofdm values for the given number of tx_chains and 20, 20in40, etc. */
int ppr_set_ofdm(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_mode_t mode, wl_tx_chains_t tx_chains,
	const ppr_ofdm_rateset_t* ofdm)
{
	pprpbw_t* bw_pwrs;
	int8* powers;
	int cnt = 0;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = (int8*)ppr_get_ofdm_group(bw_pwrs, mode, tx_chains);
		if (powers != NULL) {
			bcopy(ofdm->pwr, powers, sizeof(*ofdm));
			cnt = sizeof(*ofdm);
		}
	}
	return cnt;
}


/* Set the HT MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_set_ht_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, const ppr_ht_mcs_rateset_t* mcs)
{
	pprpbw_t* bw_pwrs;
	int8* powers;
	int cnt = 0;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = (int8*)ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			bcopy(mcs->pwr, powers, sizeof(*mcs));
			cnt = sizeof(*mcs);
		}
	}
	return cnt;
}


/* Set the VHT MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_set_vht_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, const ppr_vht_mcs_rateset_t* mcs)
{
	pprpbw_t* bw_pwrs;
	int8* powers;
	int cnt = 0;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = (int8*)ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			bcopy(mcs->pwr, powers, sizeof(*mcs));
			cnt = sizeof(*mcs);
		}
	}
	return cnt;
}


/* Routines to set rate groups to a single target value */

/* Set the dsss values for the given number of tx_chains and 20, 20in40, etc. */
int ppr_set_same_dsss(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_chains_t tx_chains, const int8 power)
{
	pprpbw_t* bw_pwrs;
	int8* dest_group;
	int cnt = 0;
	int i;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		dest_group = (int8*)ppr_get_dsss_group(bw_pwrs, tx_chains);
		if (dest_group != NULL) {
			cnt = sizeof(ppr_dsss_rateset_t);
			for (i = 0; i < cnt; i++)
				*dest_group++ = power;
		}
	}
	return cnt;
}


/* Set the ofdm values for the given number of tx_chains and 20, 20in40, etc. */
int ppr_set_same_ofdm(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_mode_t mode, wl_tx_chains_t tx_chains,
	const int8 power)
{
	pprpbw_t* bw_pwrs;
	int8* dest_group;
	int cnt = 0;
	int i;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		dest_group = (int8*)ppr_get_ofdm_group(bw_pwrs, mode, tx_chains);
		if (dest_group != NULL) {
			cnt = sizeof(ppr_ofdm_rateset_t);
			for (i = 0; i < cnt; i++)
				*dest_group++ = power;
		}
	}
	return cnt;
}


/* Set the HT MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_set_same_ht_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, const int8 power)
{
	pprpbw_t* bw_pwrs;
	int8* dest_group;
	int cnt = 0;
	int i;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		dest_group = (int8*)ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (dest_group != NULL) {
			cnt = sizeof(ppr_ht_mcs_rateset_t);
			for (i = 0; i < cnt; i++)
				*dest_group++ = power;
		}
	}
	return cnt;
}


/* Set the HT MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_set_same_vht_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, const int8 power)
{
	pprpbw_t* bw_pwrs;
	int8* dest_group;
	int cnt = 0;
	int i;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		dest_group = (int8*)ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (dest_group != NULL) {
			cnt = sizeof(ppr_vht_mcs_rateset_t);
			for (i = 0; i < cnt; i++)
				*dest_group++ = power;
		}
	}
	return cnt;
}


/* Helper routines to operate on the entire ppr set */

/* Ensure no rate limit is greater than the cap */
uint ppr_apply_max(ppr_t* pprptr, int8 maxval)
{
	uint i;
	int8* rptr = (int8*)&pprptr->ppr_bw;

	for (i = 0; i < ppr_pwrs_size(pprptr->ch_bw); i++, rptr++) {
		*rptr = MIN(*rptr, maxval);
	}
	return i;
}


/*
 * Check for any power in the rate group at or below the threshold. If any is
 * found, set the entire group to WL_RATE_DISABLED. An exception is made for
 * VHT 8 and 9 which will not cause the entire group to be disabled if they
 * are disabled or below the threshold.
 */
static void ppr_force_disabled_group(int8* powers, int8 threshold, uint len)
{
	uint i;

	for (i = 0; (i < len) && (i < WL_RATESET_SZ_HT_MCS); i++) {
		/* if we find a below-threshold rate in the set... */
		if (powers[i] <= threshold) {
			/* disable the entire rate set and return */
			for (i = 0; i < len; i++) {
				powers[i] = WL_RATE_DISABLED;
			}
			return;
		}
	}
	/* VHT 8 and 9 can be disabled separately */
	for (; i < len; i++) {
		if (powers[i] <= threshold) {
			powers[i] = WL_RATE_DISABLED;
		}
	}
}


/*
 * Make low power rates (below the threshold) explicitly disabled.
 * If one rate in a group is disabled, disable the whole group.
 */
int ppr_force_disabled(ppr_t* pprptr, int8 threshold)
{
	wl_tx_bw_t bw;

	for (bw = WL_TX_BW_20; bw <= WL_TX_BW_80; bw++) {
		pprpbw_t* bw_pwrs = ppr_get_bw_powers(pprptr, bw);

		if (bw_pwrs != NULL) {
			int8* powers;
#if (PPR_MAX_TX_CHAINS > 1)
			int8* mcs_powers;
#endif
			powers = (int8*)ppr_get_dsss_group(bw_pwrs, WL_TX_CHAINS_1);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_DSSS);

			powers = (int8*)ppr_get_ofdm_group(bw_pwrs,
				WL_TX_MODE_NONE, WL_TX_CHAINS_1);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_OFDM);

			powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1, WL_TX_MODE_NONE,
				WL_TX_CHAINS_1);
			ASSERT(powers);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_VHT_MCS);

#if (PPR_MAX_TX_CHAINS > 1)
			powers = (int8*)ppr_get_dsss_group(bw_pwrs, WL_TX_CHAINS_2);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_DSSS);

			powers = (int8*)ppr_get_ofdm_group(bw_pwrs, WL_TX_MODE_CDD, WL_TX_CHAINS_2);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_OFDM);

			mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2);
			ASSERT(mcs_powers);
			for (powers = mcs_powers; powers < &mcs_powers[PPR_CHAIN2_MCS_SIZE];
				powers += WL_RATESET_SZ_VHT_MCS) {
				ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_VHT_MCS);
			}

#ifdef WL_BEAMFORMING
			if (PPR_TXBF_ENAB()) {
				powers = (int8*)ppr_get_ofdm_group(bw_pwrs, WL_TX_MODE_TXBF,
					WL_TX_CHAINS_2);
				ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_OFDM);

				mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1,
					WL_TX_MODE_TXBF, WL_TX_CHAINS_2);
				ASSERT(mcs_powers);
				for (powers = mcs_powers;
					powers < &mcs_powers[PPR_BF_CHAIN2_MCS_SIZE];
					powers += WL_RATESET_SZ_VHT_MCS) {
					ppr_force_disabled_group(powers, threshold,
						WL_RATESET_SZ_VHT_MCS);
				}
			}
#endif /* WL_BEAMFORMING */

#if (PPR_MAX_TX_CHAINS > 2)
			powers = (int8*)ppr_get_dsss_group(bw_pwrs, WL_TX_CHAINS_3);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_DSSS);

			powers = (int8*)ppr_get_ofdm_group(bw_pwrs, WL_TX_MODE_CDD, WL_TX_CHAINS_3);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_OFDM);

			mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_3);
			ASSERT(mcs_powers);
			for (powers = mcs_powers; powers < &mcs_powers[PPR_CHAIN3_MCS_SIZE];
				powers += WL_RATESET_SZ_VHT_MCS) {
				ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_VHT_MCS);
			}

#ifdef WL_BEAMFORMING
			if (PPR_TXBF_ENAB()) {
				powers = (int8*)ppr_get_ofdm_group(bw_pwrs, WL_TX_MODE_TXBF,
					WL_TX_CHAINS_3);
				ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_OFDM);

				mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1,
					WL_TX_MODE_TXBF, WL_TX_CHAINS_3);
				ASSERT(mcs_powers);
				for (powers = mcs_powers;
					powers < &mcs_powers[PPR_BF_CHAIN3_MCS_SIZE];
					powers += WL_RATESET_SZ_VHT_MCS) {
					ppr_force_disabled_group(powers, threshold,
						WL_RATESET_SZ_VHT_MCS);
				}
			}
#endif /* WL_BEAMFORMING */
#endif /* (PPR_MAX_TX_CHAINS > 2) */
#endif /* (PPR_MAX_TX_CHAINS > 1) */
		}
	}
	return BCME_OK;
}


#if (PPR_MAX_TX_CHAINS > 1)
#define APPLY_CONSTRAINT(x, y, max) do {		\
		ret += (y - x);							\
		for (i = x; i < y; i++)					\
			pprbuf[i] = MIN(pprbuf[i], max);	\
	} while (0);


/* Apply appropriate single-, two- and three-chain constraints across the appropriate ppr block */
static uint ppr_apply_constraint_to_block(int8* pprbuf, int8 constraint)
{
	uint ret = 0;
	uint i = 0;
	int8 constraint_2chain = constraint - QDB(3);
#if (PPR_MAX_TX_CHAINS > 2)
	int8 constraint_3chain = constraint - (QDB(4) + 3); /* - 4.75dBm */
#endif

	APPLY_CONSTRAINT(PPR_CHAIN1_FIRST, PPR_CHAIN1_END, constraint);
	APPLY_CONSTRAINT(PPR_CHAIN2_FIRST, PPR_CHAIN2_END, constraint_2chain);
#if (PPR_MAX_TX_CHAINS > 2)
	APPLY_CONSTRAINT(PPR_CHAIN3_FIRST, PPR_CHAIN3_END, constraint_3chain);
#endif
#ifdef WL_BEAMFORMING
	APPLY_CONSTRAINT(PPR_BF_CHAIN2_FIRST, PPR_BF_CHAIN2_END, constraint_2chain);
#if (PPR_MAX_TX_CHAINS > 2)
	APPLY_CONSTRAINT(PPR_BF_CHAIN3_FIRST, PPR_BF_CHAIN3_END, constraint_3chain);
#endif
#endif /* WL_BEAMFORMING */
	return ret;
}
#endif /* (PPR_MAX_TX_CHAINS > 1) */


/*
 * Reduce total transmitted power to level of constraint.
 * For two chain rates, the per-antenna power must be halved.
 * For three chain rates, it must be a third of the constraint.
 */
uint ppr_apply_constraint_total_tx(ppr_t* pprptr, int8 constraint)
{
	uint ret = 0;

#if (PPR_MAX_TX_CHAINS > 1)
	int8* pprbuf;
	ASSERT(pprptr);

	switch (pprptr->ch_bw) {
	case WL_TX_BW_20:
		{
			pprbuf = (int8*)&pprptr->ppr_bw.ch20.b20;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
		}
		break;
	case WL_TX_BW_40:
		{
			pprbuf = (int8*)&pprptr->ppr_bw.ch40.b40;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch40.b20in40;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
		}
		break;
	case WL_TX_BW_80:
		{
			pprbuf = (int8*)&pprptr->ppr_bw.ch80.b80;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch80.b20in80;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch80.b40in80;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
		}
		break;
	default:
		ASSERT(0);
	}

#else
	ASSERT(pprptr);
	ret = ppr_apply_max(pprptr, constraint);
#endif /* PPR_MAX_TX_CHAINS > 1 */
	return ret;
}


/* Ensure no rate limit is lower than the specified minimum */
uint ppr_apply_min(ppr_t* pprptr, int8 minval)
{
	uint i;
	int8* rptr = (int8*)&pprptr->ppr_bw;

	for (i = 0; i < ppr_pwrs_size(pprptr->ch_bw); i++, rptr++) {
		*rptr = MAX(*rptr, minval);
	}
	return i;
}


/* Ensure no rate limit in this ppr set is greater than the corresponding limit in ppr_cap */
uint ppr_apply_vector_ceiling(ppr_t* pprptr, const ppr_t* ppr_cap)
{
	uint i = 0;
	int8* rptr = (int8*)&pprptr->ppr_bw;
	const int8* capptr = (const int8*)&ppr_cap->ppr_bw;

	if (pprptr->ch_bw == ppr_cap->ch_bw) {
		for (i = 0; i < ppr_pwrs_size(pprptr->ch_bw); i++, rptr++, capptr++) {
			*rptr = MIN(*rptr, *capptr);
		}
	}
	return i;
}


/* Ensure no rate limit in this ppr set is lower than the corresponding limit in ppr_min */
uint ppr_apply_vector_floor(ppr_t* pprptr, const ppr_t* ppr_min)
{
	uint i = 0;
	int8* rptr = (int8*)&pprptr->ppr_bw;
	const int8* minptr = (const int8*)&ppr_min->ppr_bw;

	if (pprptr->ch_bw == ppr_min->ch_bw) {
		for (i = 0; i < ppr_pwrs_size(pprptr->ch_bw); i++, rptr++, minptr++) {
			*rptr = MAX((uint8)*rptr, (uint8)*minptr);
		}
	}
	return i;
}


/* Get the maximum power in the ppr set */
int8 ppr_get_max(ppr_t* pprptr)
{
	uint i;
	int8* rptr = (int8*)&pprptr->ppr_bw;
	int8 maxval = *rptr++;

	for (i = 1; i < ppr_pwrs_size(pprptr->ch_bw); i++, rptr++) {
		maxval = MAX(maxval, *rptr);
	}
	return maxval;
}


/*
 * Get the minimum power in the ppr set, excluding disallowed
 * rates and (possibly) powers set to the minimum for the phy
 */
int8 ppr_get_min(ppr_t* pprptr, int8 floor)
{
	uint i;
	int8* rptr = (int8*)&pprptr->ppr_bw;
	int8 minval = WL_RATE_DISABLED;

	for (i = 0; (i < ppr_pwrs_size(pprptr->ch_bw)) && ((minval == WL_RATE_DISABLED) ||
		(minval == floor)); i++, rptr++) {
		minval = *rptr;
	}
	for (; i < ppr_pwrs_size(pprptr->ch_bw); i++, rptr++) {
		if ((*rptr != WL_RATE_DISABLED) && (*rptr != floor))
			minval = MIN(minval, *rptr);
	}
	return minval;
}


/* Get the maximum power for a given bandwidth in the ppr set */
int8 ppr_get_max_for_bw(ppr_t* pprptr, wl_tx_bw_t bw)
{
	uint i;
	const pprpbw_t* bw_pwrs;
	const int8* rptr;
	int8 maxval;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		rptr = (const int8*)bw_pwrs;
		maxval = *rptr++;

		for (i = 1; i < sizeof(*bw_pwrs); i++, rptr++) {
			maxval = MAX(maxval, *rptr);
		}
	} else {
		maxval = WL_RATE_DISABLED;
	}
	return maxval;
}


/* Get the minimum power for a given bandwidth  in the ppr set */
int8 ppr_get_min_for_bw(ppr_t* pprptr, wl_tx_bw_t bw)
{
	uint i;
	const pprpbw_t* bw_pwrs;
	const int8* rptr;
	int8 minval;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		rptr = (const int8*)bw_pwrs;
		minval = *rptr++;

		for (i = 1; i < sizeof(*bw_pwrs); i++, rptr++) {
			minval = MIN(minval, *rptr);
		}
	} else
		minval = WL_RATE_DISABLED;
	return minval;
}


/* Map the given function with its context value over the two power vectors */
void
ppr_map_vec_dsss(ppr_mapfn_t fn, void* context, ppr_t* pprptr1, ppr_t* pprptr2,
	wl_tx_bw_t bw, wl_tx_chains_t tx_chains)
{
	pprpbw_t* bw_pwrs1;
	pprpbw_t* bw_pwrs2;
	int8* powers1;
	int8* powers2;
	uint i;

	ASSERT(pprptr1);
	ASSERT(pprptr2);

	bw_pwrs1 = ppr_get_bw_powers(pprptr1, bw);
	bw_pwrs2 = ppr_get_bw_powers(pprptr2, bw);
	if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
		powers1 = (int8*)ppr_get_dsss_group(bw_pwrs1, tx_chains);
		powers2 = (int8*)ppr_get_dsss_group(bw_pwrs2, tx_chains);
		if ((powers1 != NULL) && (powers2 != NULL)) {
			for (i = 0; i < WL_RATESET_SZ_DSSS; i++)
				(fn)(context, (uint8*)powers1++, (uint8*)powers2++);
		}
	}
}


/* Map the given function with its context value over the two power vectors */
void
ppr_map_vec_ofdm(ppr_mapfn_t fn, void* context, ppr_t* pprptr1, ppr_t* pprptr2,
	wl_tx_bw_t bw, wl_tx_mode_t mode, wl_tx_chains_t tx_chains)
{
	pprpbw_t* bw_pwrs1;
	pprpbw_t* bw_pwrs2;
	int8* powers1;
	int8* powers2;
	uint i;

	bw_pwrs1 = ppr_get_bw_powers(pprptr1, bw);
	bw_pwrs2 = ppr_get_bw_powers(pprptr2, bw);
	if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
		powers1 = (int8*)ppr_get_ofdm_group(bw_pwrs1, mode, tx_chains);
		powers2 = (int8*)ppr_get_ofdm_group(bw_pwrs2, mode, tx_chains);
		if ((powers1 != NULL) && (powers2 != NULL)) {
			for (i = 0; i < WL_RATESET_SZ_OFDM; i++)
				(fn)(context, (uint8*)powers1++, (uint8*)powers2++);
		}
	}
}


/* Map the given function with its context value over the two power vectors */
void
ppr_map_vec_ht_mcs(ppr_mapfn_t fn, void* context, ppr_t* pprptr1,
	ppr_t* pprptr2, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains)
{
	pprpbw_t* bw_pwrs1;
	pprpbw_t* bw_pwrs2;
	int8* powers1;
	int8* powers2;
	uint i;

	bw_pwrs1 = ppr_get_bw_powers(pprptr1, bw);
	bw_pwrs2 = ppr_get_bw_powers(pprptr2, bw);
	if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
		powers1 = (int8*)ppr_get_mcs_group(bw_pwrs1, Nss, mode, tx_chains);
		powers2 = (int8*)ppr_get_mcs_group(bw_pwrs2, Nss, mode, tx_chains);
		if ((powers1 != NULL) && (powers2 != NULL)) {
			for (i = 0; i < WL_RATESET_SZ_HT_MCS; i++)
				(fn)(context, (uint8*)powers1++, (uint8*)powers2++);
		}
	}
}


/* Map the given function with its context value over the two power vectors */
void
ppr_map_vec_vht_mcs(ppr_mapfn_t fn, void* context, ppr_t* pprptr1,
	ppr_t* pprptr2, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode, wl_tx_chains_t
	tx_chains)
{
	pprpbw_t* bw_pwrs1;
	pprpbw_t* bw_pwrs2;
	int8* powers1;
	int8* powers2;
	uint i;

	bw_pwrs1 = ppr_get_bw_powers(pprptr1, bw);
	bw_pwrs2 = ppr_get_bw_powers(pprptr2, bw);
	if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
		powers1 = (int8*)ppr_get_mcs_group(bw_pwrs1, Nss, mode, tx_chains);
		powers2 = (int8*)ppr_get_mcs_group(bw_pwrs2, Nss, mode, tx_chains);
		if ((powers1 != NULL) && (powers2 != NULL)) {
			for (i = 0; i < WL_RATESET_SZ_VHT_MCS; i++)
				(fn)(context, (uint8*)powers1++, (uint8*)powers2++);
		}
	}
}


/* Map the given function with its context value over the two power vectors */

void
ppr_map_vec_all(ppr_mapfn_t fn, void* context, ppr_t* pprptr1, ppr_t* pprptr2)
{
	uint i;
	pprpbw_t* bw_pwrs1;
	pprpbw_t* bw_pwrs2;
	int8* rptr1 = (int8*)&pprptr1->ppr_bw;
	int8* rptr2 = (int8*)&pprptr2->ppr_bw;

	bw_pwrs1 = ppr_get_bw_powers(pprptr1, WL_TX_BW_20);
	bw_pwrs2 = ppr_get_bw_powers(pprptr2, WL_TX_BW_20);
	if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
		rptr1 = (int8*)bw_pwrs1;
		rptr2 = (int8*)bw_pwrs2;
		for (i = 0; i < sizeof(pprpbw_t); i++, rptr1++, rptr2++) {
			(fn)(context, (uint8*)rptr1, (uint8*)rptr2);
		}
	}

	bw_pwrs1 = ppr_get_bw_powers(pprptr1, WL_TX_BW_40);
	bw_pwrs2 = ppr_get_bw_powers(pprptr2, WL_TX_BW_40);

	if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
		rptr1 = (int8*)bw_pwrs1;
		rptr2 = (int8*)bw_pwrs2;
		for (i = 0; i < sizeof(pprpbw_t); i++, rptr1++, rptr2++) {
			(fn)(context, (uint8*)rptr1, (uint8*)rptr2);
		}

		bw_pwrs1 = ppr_get_bw_powers(pprptr1, WL_TX_BW_20IN40);
		bw_pwrs2 = ppr_get_bw_powers(pprptr2, WL_TX_BW_20IN40);
		if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
			rptr1 = (int8*)bw_pwrs1;
			rptr2 = (int8*)bw_pwrs2;
			for (i = 0; i < sizeof(pprpbw_t); i++, rptr1++, rptr2++) {
				(fn)(context, (uint8*)rptr1, (uint8*)rptr2);
			}
		}
	}

	bw_pwrs1 = ppr_get_bw_powers(pprptr1, WL_TX_BW_80);
	bw_pwrs2 = ppr_get_bw_powers(pprptr2, WL_TX_BW_80);
	if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
		rptr1 = (int8*)bw_pwrs1;
		rptr2 = (int8*)bw_pwrs2;
		for (i = 0; i < sizeof(pprpbw_t); i++, rptr1++, rptr2++) {
			(fn)(context, (uint8*)rptr1, (uint8*)rptr2);
		}

		bw_pwrs1 = ppr_get_bw_powers(pprptr1, WL_TX_BW_20IN80);
		bw_pwrs2 = ppr_get_bw_powers(pprptr2, WL_TX_BW_20IN80);
		if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
			rptr1 = (int8*)bw_pwrs1;
			rptr2 = (int8*)bw_pwrs2;
			for (i = 0; i < sizeof(pprpbw_t); i++, rptr1++, rptr2++) {
				(fn)(context, (uint8*)rptr1, (uint8*)rptr2);
			}
		}

		bw_pwrs1 = ppr_get_bw_powers(pprptr1, WL_TX_BW_40IN80);
		bw_pwrs2 = ppr_get_bw_powers(pprptr2, WL_TX_BW_40IN80);
		if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
			rptr1 = (int8*)bw_pwrs1;
			rptr2 = (int8*)bw_pwrs2;
			for (i = 0; i < sizeof(pprpbw_t); i++, rptr1++, rptr2++) {
				(fn)(context, (uint8*)rptr1, (uint8*)rptr2);
			}
		}
	}
}


/* Set PPR struct to a certain power level */
void
ppr_set_cmn_val(ppr_t* pprptr, int8 val)
{
	memset((uchar*)&pprptr->ppr_bw, val, ppr_pwrs_size(pprptr->ch_bw));
}


/* Make an identical copy of a ppr structure (for ppr_bw==all case) */
void
ppr_copy_struct(ppr_t* pprptr_s, ppr_t* pprptr_d)
{
	int8* rptr_s = (int8*)&pprptr_s->ppr_bw;
	int8* rptr_d = (int8*)&pprptr_d->ppr_bw;
	/* ASSERT(ppr_pwrs_size(pprptr_d->ch_bw) >= ppr_pwrs_size(pprptr_s->ch_bw)); */

	if (pprptr_s->ch_bw == pprptr_d->ch_bw)
		bcopy(rptr_s, rptr_d, ppr_pwrs_size(pprptr_s->ch_bw));
	else {
		const pprpbw_t* src_bw_pwrs;
		pprpbw_t* dest_bw_pwrs;

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_20);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_20);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_40);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_40);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_20IN40);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_20IN40);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_80);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_80);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_20IN80);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_20IN80);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_40IN80);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_40IN80);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));
	}
}


/* Subtract each power from a common value and re-store */
void
ppr_cmn_val_minus(ppr_t* pprptr, int8 val)
{
	uint i;
	int8* rptr = (int8*)&pprptr->ppr_bw;

	for (i = 0; i < ppr_pwrs_size(pprptr->ch_bw); i++, rptr++) {
		if (*rptr != (int8)WL_RATE_DISABLED)
			*rptr = val - *rptr;
	}

}


/* Subtract a common value from each power and re-store */
void
ppr_minus_cmn_val(ppr_t* pprptr, int8 val)
{
	uint i;
	int8* rptr = (int8*)&pprptr->ppr_bw;

	for (i = 0; i < ppr_pwrs_size(pprptr->ch_bw); i++, rptr++) {
		if (*rptr != (int8)WL_RATE_DISABLED)
			*rptr = (*rptr > val) ? (*rptr - val) : 0;
	}

}


/* Add a common value to each power and re-store */
void
ppr_plus_cmn_val(ppr_t* pprptr, int8 val)
{
	uint i;
	int8* rptr = (int8*)&pprptr->ppr_bw;

	for (i = 0; i < ppr_pwrs_size(pprptr->ch_bw); i++, rptr++) {
		if (*rptr != (int8)WL_RATE_DISABLED)
			*rptr += val;
	}

}


/* Multiply by a percentage */
void
ppr_multiply_percentage(ppr_t* pprptr, uint8 val)
{
	uint i;
	int8* rptr = (int8*)&pprptr->ppr_bw;

	for (i = 0; i < ppr_pwrs_size(pprptr->ch_bw); i++, rptr++) {
		if (*rptr != (int8)WL_RATE_DISABLED)
			*rptr = (*rptr * val) / 100;
	}

}


/* Compare two ppr variables p1 and p2, save the min value of each
 * contents to variable p1
 */
void
ppr_compare_min(ppr_t* p1, ppr_t* p2)
{
	uint i;
	int8* rptr1 = NULL;
	int8* rptr2 = NULL;
	uint32 pprsize = 0;

	if (p1->ch_bw == p2->ch_bw) {
		rptr1 = (int8*)&p1->ppr_bw;
		rptr2 = (int8*)&p2->ppr_bw;
		pprsize = ppr_pwrs_size(p1->ch_bw);
	}

	for (i = 0; i < pprsize; i++, rptr1++, rptr2++) {
		*rptr1 = MIN(*rptr1, *rptr2);
	}
}


/* Compare two ppr variables p1 and p2, save the max. value of each
 * contents to variable p1
 */
void
ppr_compare_max(ppr_t* p1, ppr_t* p2)
{
	uint i;
	int8* rptr1 = NULL;
	int8* rptr2 = NULL;
	uint32 pprsize = 0;

	if (p1->ch_bw == p2->ch_bw) {
		rptr1 = (int8*)&p1->ppr_bw;
		rptr2 = (int8*)&p2->ppr_bw;
		pprsize = ppr_pwrs_size(p1->ch_bw);
	}

	for (i = 0; i < pprsize; i++, rptr1++, rptr2++) {
		*rptr1 = MAX(*rptr1, *rptr2);
	}
}


/* Serialize the contents of the opaque ppr struct.
 * Writes number of bytes copied, zero on error.
 * Returns error code, BCME_OK if successful.
 */
int
ppr_serialize(const ppr_t* pprptr, uint8* buf, uint buflen, uint* bytes_copied)
{
	int err = BCME_OK;
	if (buflen <= sizeof(ppr_ser_mem_flag_t)) {
		err = BCME_BUFTOOSHORT;
	} else {
		ppr_ser_mem_flag_t *smem_flag = (ppr_ser_mem_flag_t *)buf;
		uint32 flag = NTOH32(smem_flag->flag);

		/* check if memory contains a valid flag, if not, use current
		 * condition (num of chains, txbf etc.) to serialize data.
		 */
		if (NTOH32(smem_flag->magic_word) != PPR_SER_MEM_WORD) {
			flag = ppr_get_flag();
		}

		if ((flag & PPR_MAX_TX_CHAIN_MASK) == 0) {
			/* chains==0. Indicates incompatible WL */
			/* Use the current driver defaults to serialize data */
			flag = ppr_get_flag();
		}

		if (buflen >= ppr_ser_size_by_flag(flag, pprptr->ch_bw)) {
			*bytes_copied = ppr_serialize_data(pprptr, buf, flag);
		} else {
			err = BCME_BUFTOOSHORT;
		}
	}
	return err;
}


/* Deserialize the contents of a buffer into an opaque ppr struct.
 * Creates an opaque structure referenced by *pptrptr, NULL on error.
 * Returns error code, BCME_OK if successful.
 */
int
ppr_deserialize_create(osl_t *osh, const uint8* buf, uint buflen, ppr_t** pprptr)
{
	const uint8* bptr = buf;
	int err = BCME_OK;
	ppr_t* lpprptr = NULL;

	if ((buflen > SER_HDR_LEN) && (bptr != NULL) && (*bptr == PPR_SERIALIZATION_VER)) {
		const ppr_deser_header_t * ser_head = (const ppr_deser_header_t *)bptr;
		wl_tx_bw_t ch_bw = ser_head->bw;
		/* struct size plus header */
		uint32 ser_size = ppr_pwrs_size(ch_bw) + SER_HDR_LEN;

		if ((lpprptr = ppr_create(osh, ch_bw)) != NULL) {
			uint32 flags = NTOH32(ser_head->flags);
			uint16 per_band_size = NTOH16(ser_head->per_band_size);
			uint16 chain3size = NTOH16(ser_head->chain3size);
			/* set the data with default value before deserialize */
			ppr_set_cmn_val(lpprptr, WL_RATE_DISABLED);

			ppr_deser_cpy(lpprptr, bptr + sizeof(*ser_head), flags, ch_bw,
				per_band_size, chain3size);
		} else if (buflen < ser_size) {
			err = BCME_BUFTOOSHORT;
		} else {
			err = BCME_NOMEM;
		}
	} else if (buflen <= SER_HDR_LEN) {
		err = BCME_BUFTOOSHORT;
	} else if (bptr == NULL) {
		err = BCME_BADARG;
	} else {
		err = BCME_VERSION;
	}
	*pprptr = lpprptr;
	return err;
}


/* Deserialize the contents of a buffer into an opaque ppr struct.
 * Creates an opaque structure referenced by *pptrptr, NULL on error.
 * Returns error code, BCME_OK if successful.
 */
int
ppr_deserialize(ppr_t* pprptr, const uint8* buf, uint buflen)
{
	const uint8* bptr = buf;
	int err = BCME_OK;
	ASSERT(pprptr);
	if ((buflen > SER_HDR_LEN) && (bptr != NULL) && (*bptr == PPR_SERIALIZATION_VER)) {
		const ppr_deser_header_t * ser_head = (const ppr_deser_header_t *)bptr;
		wl_tx_bw_t ch_bw = ser_head->bw;

		if (ch_bw == pprptr->ch_bw) {
			uint32 flags = NTOH32(ser_head->flags);
			uint16 per_band_size = NTOH16(ser_head->per_band_size);
			uint16 chain3size = NTOH16(ser_head->chain3size);
			ppr_set_cmn_val(pprptr, WL_RATE_DISABLED);
			ppr_deser_cpy(pprptr, bptr + sizeof(*ser_head), flags, ch_bw,
				per_band_size, chain3size);
		} else {
			err = BCME_BADARG;
		}
	} else if (buflen <= SER_HDR_LEN) {
		err = BCME_BUFTOOSHORT;
	} else if (bptr == NULL) {
		err = BCME_BADARG;
	} else {
		err = BCME_VERSION;
	}
	return err;
}


#ifdef WLTXPWR_CACHE

#define MAX_TXPWR_CACHE_ENTRIES 2
#define TXPWR_ALL_INVALID	 0xff
#define TXPWR_CACHE_TXPWR_MAX 0x7f             /* WLC_TXPWR_MAX; */

/* transmit power cache */
struct tx_pwr_cache_entry {
	chanspec_t chanspec;
	ppr_t* cache_pwrs[TXPWR_CACHE_NUM_TYPES];
	uint8 tx_pwr_max[PPR_MAX_TX_CHAINS];
	uint8 tx_pwr_min[PPR_MAX_TX_CHAINS];
	int8 txchain_offsets[PPR_MAX_TX_CHAINS];
	uint8 data_invalid_flags;
#if !defined(WLC_LOW) || !defined(WLC_HIGH)
	int stf_tx_target_pwr_min;
#endif
#if defined(WLC_HIGH)
	uint8 stf_tx_max_offset;
#endif
};

#ifdef TXPWR_CACHE_IN_ROM
uint8 txpwr_cache[MAX_TXPWR_CACHE_ENTRIES*16] = {0};
#endif

#ifndef WLTXPWR_CACHE_PHY_ONLY
static int stf_tx_pwr_min = TXPWR_CACHE_TXPWR_MAX;
#endif

static tx_pwr_cache_entry_t* wlc_phy_txpwr_cache_get_entry(tx_pwr_cache_entry_t* cacheptr,
	chanspec_t chanspec);
static tx_pwr_cache_entry_t* wlc_phy_txpwr_cache_get_diff_entry(tx_pwr_cache_entry_t* cacheptr,
	chanspec_t chanspec);
static void wlc_phy_txpwr_cache_clear_entry(osl_t *osh, tx_pwr_cache_entry_t* entryptr);
static tx_pwr_cache_entry_t* tx_pwr_cache_get(tx_pwr_cache_entry_t* cacheptr, uint32 idx);

static tx_pwr_cache_entry_t*
BCMRAMFN(tx_pwr_cache_get)(tx_pwr_cache_entry_t* tx_pwr_cache, uint32 idx)
{
	return (&tx_pwr_cache[idx]);
}

/* Find a cache entry for the specified chanspec. */
static tx_pwr_cache_entry_t* wlc_phy_txpwr_cache_get_entry(tx_pwr_cache_entry_t* cacheptr,
	chanspec_t chanspec)
{
	uint i;
	tx_pwr_cache_entry_t* entryptr = NULL;

	for (i = 0; i < (MAX_TXPWR_CACHE_ENTRIES); i++) {
		entryptr = tx_pwr_cache_get(cacheptr, i);
		if (entryptr->chanspec == chanspec) {
			return entryptr;
		}
	}
	return NULL;
}


/* Find a cache entry that's NOT for the specified chanspec. */
static tx_pwr_cache_entry_t* wlc_phy_txpwr_cache_get_diff_entry(tx_pwr_cache_entry_t* cacheptr,
	chanspec_t chanspec)
{
	uint i;
	tx_pwr_cache_entry_t* entryptr = NULL;

	for (i = 0; i < (MAX_TXPWR_CACHE_ENTRIES) && (entryptr == NULL); i++) {
		if (tx_pwr_cache_get(cacheptr, i)->chanspec != chanspec) {
			entryptr = tx_pwr_cache_get(cacheptr, i);
		}
	}
	return entryptr;
}


/* Clear a specific cache entry. Delete any ppr_t structs and clear the pointers. */
static void wlc_phy_txpwr_cache_clear_entry(osl_t *osh, tx_pwr_cache_entry_t* entryptr)
{
	uint i;

	entryptr->chanspec = 0;

	ASSERT(entryptr != NULL);
	for (i = 0; i < TXPWR_CACHE_NUM_TYPES; i++) {
		if (entryptr->cache_pwrs[i] != NULL) {
			ppr_delete(osh, entryptr->cache_pwrs[i]);
			entryptr->cache_pwrs[i] = NULL;
		}
	}
	/*
	 * Don't bother with max, min and txchain_offsets, as they need to be
	 * initialised when the entry is setup for a new chanspec
	 */
}


/*
 * Get a ppr_t struct of a given type from the cache for the specified chanspec.
 * Don't return the pointer if the cached data is invalid.
 */
ppr_t* wlc_phy_get_cached_pwr(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, uint pwr_type)
{
	ppr_t* pwrptr = NULL;

	if (pwr_type < TXPWR_CACHE_NUM_TYPES) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if ((entryptr != NULL) &&
			((entryptr->data_invalid_flags & (0x01 << pwr_type)) == 0))
			pwrptr = entryptr->cache_pwrs[pwr_type];
	}

	return pwrptr;
}


/* Add a ppr_t struct of a given type to the cache for the specified chanspec. */
int wlc_phy_set_cached_pwr(osl_t *osh, tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec,
	uint pwr_type, ppr_t* pwrptr)
{
	int result = BCME_NOTFOUND;

	if (pwr_type < TXPWR_CACHE_NUM_TYPES) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL) {
			if ((entryptr->cache_pwrs[pwr_type] != NULL) &&
				(entryptr->cache_pwrs[pwr_type] != pwrptr)) {
				ppr_delete(osh, entryptr->cache_pwrs[pwr_type]);
			}
			entryptr->cache_pwrs[pwr_type] = pwrptr;
			entryptr->data_invalid_flags &= ~(0x01 << pwr_type); /* now valid */
			result = BCME_OK;
		}
	}
	return result;
}

/* Indicate if we have cached a particular ppr_t struct for any chanspec. */
bool wlc_phy_is_pwr_cached(tx_pwr_cache_entry_t* cacheptr, uint pwr_type, ppr_t* pwrptr)
{
	bool result = FALSE;
	uint i;

	if (pwr_type < TXPWR_CACHE_NUM_TYPES) {
		for (i = 0; (i < MAX_TXPWR_CACHE_ENTRIES) && (result == FALSE); i++) {
			if (tx_pwr_cache_get(cacheptr, i)->cache_pwrs[pwr_type] == pwrptr) {
				result = TRUE;
			}
		}
	}
	return result;
}

#if !defined(WLC_LOW) || !defined(WLC_HIGH)
/* Get the minimum target power for all cores for the chanspec. */
int wlc_phy_get_cached_stf_target_pwr_min(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec)
{
	int min_pwr = TXPWR_CACHE_TXPWR_MAX;

	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

	if ((entryptr != NULL) &&
		((entryptr->data_invalid_flags & (TXPWR_STF_TARGET_PWR_MIN_INVALID)) == 0))
		min_pwr = entryptr->stf_tx_target_pwr_min;

	return min_pwr;
}


/* set the minimum target power for all cores for the chanspec. */
int wlc_phy_set_cached_stf_target_pwr_min(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec,
	int min_pwr)
{
	int result = BCME_NOTFOUND;

	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

	if (entryptr != NULL) {
		entryptr->stf_tx_target_pwr_min = min_pwr;
		entryptr->data_invalid_flags &= ~TXPWR_STF_TARGET_PWR_MIN_INVALID; /* now valid */
		result = BCME_OK;
	}
	return result;
}
#endif /* !WLC_HIGH || !WLC_LOW */


/* Get the maximum power for the specified core and chanspec. */
uint8 wlc_phy_get_cached_pwr_max(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, uint core)
{
	uint8 max_pwr = WL_RATE_DISABLED;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL)
			max_pwr = entryptr->tx_pwr_max[core];
	}

	return max_pwr;
}


/* Set the maximum power for the specified core and chanspec. */
int wlc_phy_set_cached_pwr_max(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, uint core,
	uint8 max_pwr)
{
	int result = BCME_NOTFOUND;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL) {
			entryptr->tx_pwr_max[core] = max_pwr;
			result = BCME_OK;
		}
	}
	return result;
}


/* Get the minimum power for the specified core and chanspec. */
uint8 wlc_phy_get_cached_pwr_min(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, uint core)
{
	uint8 min_pwr = WL_RATE_DISABLED;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL)
			min_pwr = entryptr->tx_pwr_min[core];
	}

	return min_pwr;
}


/* Set the minimum power for the specified core and chanspec. */
int wlc_phy_set_cached_pwr_min(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, uint core,
	uint8 min_pwr)
{
	int result = BCME_NOTFOUND;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL) {
			entryptr->tx_pwr_min[core] = min_pwr;
			result = BCME_OK;
		}
	}
	return result;
}


/* Get the txchain offsets for the specified chanspec. */
int8 wlc_phy_get_cached_txchain_offsets(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec,
	uint core)
{
	uint8 offset = WL_RATE_DISABLED;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL)
			offset = entryptr->txchain_offsets[core];
	}

	return offset;
}


/* Set the txchain offsets for the specified chanspec. */
int wlc_phy_set_cached_txchain_offsets(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec,
	uint core, int8 offset)
{
	int result = BCME_NOTFOUND;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL) {
			entryptr->txchain_offsets[core] = offset;
			result = BCME_OK;
		}
	}
	return result;
}


/* Indicate if we have a cache entry for the specified chanspec. */
bool wlc_phy_txpwr_cache_is_cached(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec)
{
	bool result = FALSE;

	if (wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec)) {
		result = TRUE;
	}
	return result;
}


/* Find a cache entry that's NOT for the specified chanspec. Return the chanspec. */
chanspec_t wlc_phy_txpwr_cache_find_other_cached_chanspec(tx_pwr_cache_entry_t* cacheptr,
	chanspec_t chanspec)
{
	chanspec_t chan = 0;

	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_diff_entry(cacheptr, chanspec);
	if (entryptr != NULL) {
		chan = entryptr->chanspec;
	}
	return chan;
}


/* Find a specific cache entry and clear it. */
void wlc_phy_txpwr_cache_clear(osl_t *osh, tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec)
{
	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);
	if (entryptr != NULL) {
		wlc_phy_txpwr_cache_clear_entry(osh, entryptr);
	}
}


/* Invalidate all cached data. */
void wlc_phy_txpwr_cache_invalidate(tx_pwr_cache_entry_t* cacheptr)
{
	uint j;

	for (j = 0; j < MAX_TXPWR_CACHE_ENTRIES; j++) {
		tx_pwr_cache_entry_t* entryptr = tx_pwr_cache_get(cacheptr, j);
		if (entryptr->chanspec != 0) {
			uint i;
			entryptr->data_invalid_flags = TXPWR_ALL_INVALID;
			for (i = 0; i < PPR_MAX_TX_CHAINS; i++) {
				entryptr->tx_pwr_min[i] = TXPWR_CACHE_TXPWR_MAX;
				entryptr->tx_pwr_max[i] = WL_RATE_DISABLED;
				entryptr->txchain_offsets[i] = WL_RATE_DISABLED;
			}
#if !defined(WLC_LOW) || !defined(WLC_HIGH)
			entryptr->stf_tx_target_pwr_min = TXPWR_CACHE_TXPWR_MAX;
#endif
#if defined(WLC_HIGH)
			entryptr->stf_tx_max_offset = TXPWR_CACHE_TXPWR_MAX;
#endif
		}
	}
}

/* Clear all cache entries and return memory. */
void wlc_phy_txpwr_cache_close(osl_t *osh, tx_pwr_cache_entry_t* cacheptr)
{
	uint i;

	for (i = 0; i < MAX_TXPWR_CACHE_ENTRIES; i++) {
		tx_pwr_cache_entry_t* entryptr = tx_pwr_cache_get(cacheptr, i);
		if (entryptr->chanspec != 0) {
			wlc_phy_txpwr_cache_clear_entry(osh, entryptr);
		}
	}
	MFREE(osh, cacheptr, (uint)sizeof(*cacheptr) * MAX_TXPWR_CACHE_ENTRIES);
}


/* Allocate space for cache. Clear all cache entries. */
tx_pwr_cache_entry_t* wlc_phy_txpwr_cache_create(osl_t *osh)
{
	tx_pwr_cache_entry_t* cacheptr =
		(tx_pwr_cache_entry_t*)MALLOC(osh,
		(uint)sizeof(tx_pwr_cache_entry_t) * MAX_TXPWR_CACHE_ENTRIES);
	if (cacheptr != NULL) {
		memset(cacheptr, 0, (uint)sizeof(tx_pwr_cache_entry_t) * MAX_TXPWR_CACHE_ENTRIES);
	}
	return cacheptr;
}


/* Find an empty cache entry and initialise it. */
int wlc_phy_txpwr_setup_entry(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec)
{
	int result = BCME_NOTFOUND;

	/* find an empty entry */
	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, 0);
	if (entryptr != NULL) {
		uint i;

		entryptr->chanspec = chanspec;
		for (i = 0; i < PPR_MAX_TX_CHAINS; i++) {
			entryptr->tx_pwr_min[i] = TXPWR_CACHE_TXPWR_MAX; /* WLC_TXPWR_MAX; */
			entryptr->tx_pwr_max[i] = WL_RATE_DISABLED;
			entryptr->txchain_offsets[i] = WL_RATE_DISABLED;
		}
#if !defined(WLC_LOW) || !defined(WLC_HIGH)
		entryptr->stf_tx_target_pwr_min = TXPWR_CACHE_TXPWR_MAX;
#endif
#if !defined(WLC_HIGH)
		entryptr->data_invalid_flags |= TXPWR_STF_TARGET_PWR_NOT_CACHED;

#else
		entryptr->stf_tx_max_offset = TXPWR_CACHE_TXPWR_MAX;
#endif
		result = BCME_OK;
	}
	return result;
}

#ifndef WLC_LOW
/* Drop any reference to a particular ppr_t struct from the cache. */
void wlc_phy_uncache_pwr(tx_pwr_cache_entry_t* cacheptr, uint pwr_type, ppr_t* pwrptr)
{
	bool result = FALSE;
	uint i;

	if (pwr_type < TXPWR_CACHE_NUM_TYPES) {
		for (i = 0; (i < MAX_TXPWR_CACHE_ENTRIES) && (result == FALSE); i++) {
			if (tx_pwr_cache_get(cacheptr, i)->cache_pwrs[pwr_type] == pwrptr) {
				tx_pwr_cache_get(cacheptr, i)->cache_pwrs[pwr_type] = NULL;
			}
		}
	}
}
#endif

#ifndef WLTXPWR_CACHE_PHY_ONLY

#if !defined(WLC_HIGH)
bool wlc_phy_get_stf_ppr_cached(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec)
{
	bool ret = FALSE;
	tx_pwr_cache_entry_t *entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);
	if (entryptr != NULL)
		ret = !(entryptr->data_invalid_flags & TXPWR_STF_TARGET_PWR_NOT_CACHED);
	return ret;
}

void wlc_phy_set_stf_ppr_cached(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, bool bcached)
{
	tx_pwr_cache_entry_t *entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);
	if (entryptr != NULL) {
		if (bcached)
			entryptr->data_invalid_flags &= ~TXPWR_STF_TARGET_PWR_NOT_CACHED;
		else
			entryptr->data_invalid_flags |= TXPWR_STF_TARGET_PWR_NOT_CACHED;
	}
}
#endif /* !defined(WLC_HIGH) */

/* Get the cached minimum Tx power */
int wlc_phy_get_cached_stf_pwr_min_dbm(tx_pwr_cache_entry_t* cacheptr)
{
	return stf_tx_pwr_min;
}

/* set the cached minimum Tx power */
void wlc_phy_set_cached_stf_pwr_min_dbm(tx_pwr_cache_entry_t* cacheptr, int min_pwr)
{
	stf_tx_pwr_min = min_pwr;
}

#if defined(WLC_HIGH)
void wlc_phy_set_cached_stf_max_offset(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec,
	uint8 max_offset)
{
	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

	if (entryptr != NULL) {
		entryptr->stf_tx_max_offset = max_offset;
	}
}

uint8 wlc_phy_get_cached_stf_max_offset(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec)
{
	uint8 max_offset = TXPWR_CACHE_TXPWR_MAX;

	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

	if (entryptr != NULL)
		max_offset = entryptr->stf_tx_max_offset;

	return max_offset;

}
#endif /* WLTXPWR_CACHE_PHY_ONLY */
#endif /* defined(WLC_HIGH) */
#endif /* WLTXPWR_CACHE */
