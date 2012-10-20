/*
 * Common interface to channel definitions.
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_channel.c,v 1.322.2.57 2011-02-11 05:42:13 Exp $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <proto/wpa.h>
#include <sbconfig.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#include <wlioctl.h>
#include <epivers.h>
#ifdef BCMSUP_PSK
#include <proto/eapol.h>
#include <bcmwpa.h>
#endif /* BCMSUP_PSK */
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_phy_hal.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <wlc_ap.h>
#include <wlc_stf.h>
#include <wlc_channel.h>

typedef struct wlc_cm_band {
	uint8		locale_flags;		/* locale_info_t flags */
	chanvec_t	valid_channels;		/* List of valid channels in the country */
#ifdef BAND5G
	const chanvec_t	*radar_channels;	/* List of radar sensitive channels */
#endif
	struct wlc_channel_txchain_limits chain_limits;	/* per chain power limit */
	uint8		PAD[4];
} wlc_cm_band_t;

struct wlc_cm_info {
	wlc_pub_t	*pub;
	wlc_info_t	*wlc;
	char		srom_ccode[WLC_CNTRY_BUF_SZ];	/* Country Code in SROM */
	uint		srom_regrev;			/* Regulatory Rev for the SROM ccode */
	const country_info_t *country;			/* current country def */
	char		ccode[WLC_CNTRY_BUF_SZ];	/* current internal Country Code */
	uint		regrev;				/* current Regulatory Revision */
	char		country_abbrev[WLC_CNTRY_BUF_SZ];	/* current advertised ccode */
	wlc_cm_band_t	bandstate[MAXBANDS];	/* per-band state (one per phy/radio) */
	/* quiet channels currently for radar sensitivity or 11h support */
	chanvec_t	quiet_channels;		/* channels on which we cannot transmit */

	/* restricted channels */
	chanvec_t	restricted_channels; 	/* copy of the global restricted channels of the */
						/* current local */
	bool		has_restricted_ch;

	/* regulatory class */
	rcvec_t		valid_rcvec;		/* List of valid regulatory class in the country */
	const rcinfo_t	*rcinfo_list[MAXRCLIST];	/* regulatory class info list */
};

static int wlc_channels_init(wlc_cm_info_t *wlc_cm, const country_info_t *country);
static void wlc_set_country_common(
	wlc_cm_info_t *wlc_cm, const char* country_abbrev, const char* ccode, uint regrev,
	const country_info_t *country);
static int wlc_country_aggregate_map(
	wlc_cm_info_t *wlc_cm, const char *ccode, char *mapped_ccode, uint *mapped_regrev);
static const country_info_t* wlc_country_lookup_direct(const char* ccode, uint regrev);
static const country_info_t* wlc_countrycode_map(
	wlc_cm_info_t *wlc_cm, const char *ccode, char *mapped_ccode, uint *mapped_regrev);
static void wlc_channels_commit(wlc_cm_info_t *wlc_cm);
static void wlc_chanspec_list(wlc_info_t *wlc, wl_uint32_list_t *list, chanspec_t chanspec_mask);
static bool wlc_buffalo_map_locale(struct wlc_info *wlc, const char* abbrev);
static bool wlc_japan_ccode(const char *ccode);
static bool wlc_us_ccode(const char *ccode);
static void wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm_info_t *wlc_cm,
	txppr_t *txpwr, uint8 local_constraint_qdbm);
void wlc_locale_add_channels(chanvec_t *target, const chanvec_t *channels);
static void wlc_rcinfo_init(wlc_cm_info_t *wlc_cm);
static void wlc_regclass_vec_init(wlc_cm_info_t *wlc_cm);
#ifdef WL11N
static const locale_mimo_info_t * wlc_get_mimo_2g(uint8 locale_idx);
static const locale_mimo_info_t * wlc_get_mimo_5g(uint8 locale_idx);
#endif /* WL11N */
static void wlc_upd_restricted_chanspec_flag(wlc_cm_info_t *wlc_cm);
static int wlc_channel_update_txchain_offsets(wlc_cm_info_t *wlc_cm, txppr_t *txpwr);
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int wlc_channel_dump_reg_ppr(void *handle, struct bcmstrbuf *b);
static int wlc_channel_dump_reg_local_ppr(void *handle, struct bcmstrbuf *b);
static int wlc_channel_dump_srom_ppr(void *handle, struct bcmstrbuf *b);
static int wlc_channel_dump_margin(void *handle, struct bcmstrbuf *b);
#endif

typedef void (*wlc_channel_mapfn_t)(void *context, uint8 *a, uint8 *b);

/* QDB() macro takes a dB value and converts to a quarter dB value */
#ifdef QDB
#undef QDB
#endif
#define QDB(n) ((n) * WLC_TXPWR_DB_FACTOR)

/* Regulatory Matrix Spreadsheet (CLM) MIMO v3.8.6.4
 * + CLM v4.1.3
 * + CLM v4.2.4
 * + CLM v4.3.1 (Item-1 only EU/9 and Q2/4)
 * + CLM v4.3.3
 * + CLMv 4.3.4_3x3 changes(Skip changes for a13/14).
 * + CLMv 4.4.4_3x3 ALl changes except Item-2 and Item-4
 * + CLMv 4.5.3_3x3 changes for Item-5(Cisco Evora (change AP3500i to Evora)).
 * + CLMv 4.5.3_3x3 changes for Item-3(Create US/61 for BCM94331HM, based on US/53 power levels).
 * + CLMv 4.5.5 3x3 (changes from Create US/63 only)
 * + CLMv 4.5.6 3x3 changes(An6-3 for Cisco E1200/E1550)
 * + CLMv 4.4.4 3x3 changes(Create EU/13 (locales 3r-5, 3rn-4)
 * + CLMv 4.5.3 3x3 changes(Create IL/4 (locale 13-3) for Juniper AX411)
 * 
 */

/*
 * Some common channel sets
 */

/* No channels */
static const chanvec_t chanvec_none = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};

/* All 2.4 GHz HW channels */
const chanvec_t chanvec_all_2G = {
	{0xfe, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};

/* All 5 GHz HW channels */
const chanvec_t chanvec_all_5G = {
	{0x00, 0x00, 0x00, 0x00, 0x54, 0x55, 0x11, 0x11,
	0x01, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x20, 0x22, 0x22, 0x00, 0x00, 0x11,
	0x11, 0x11, 0x11, 0x01}
};

/* Regulatory Matrix Spreadsheet (CLM) MIMO v3.1 ** WITH MODIFICATIONS ** */

/* Parts of CLM v3.7.4 have been incoporated to support a high gain
 * antenna system. The additions are the differences added between
 * CLM v3.7.3 and v3.7.4.
 * Adding country revisions: JP/7 US/29 X0/6 X3/5 X1/5 X2/5
 * Adding supporting locales: 3l, 3jn_1, ,,,
 *
 * Adding the changes introduced in CLM v3.7.7, v3.7.8, and v3.7.9
 * Adding the changes introduced in CLM v4.2.3
 * Adding the changes introduced in CLM v4.3.3
 */

/*
 * Radar channel sets
 */

/* No radar */
#define radar_set_none chanvec_none

#ifdef BAND5G
static const chanvec_t radar_set1 = { /* Channels 52 - 64, 100 - 140 */
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11,	/* 52 - 60 */
	0x01, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x11, 	/* 64, 100 - 124 */
	0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 128 - 140 */
	0x00, 0x00, 0x00, 0x00}
};
#endif	/* BAND5G */

/*
 * Restricted channel sets
 */

#define restricted_set_none chanvec_none

/* Channels 34, 38, 42, 46 */
static const chanvec_t restricted_set_japan_legacy = {
	{0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};

/* Channels 12, 13 */
static const chanvec_t restricted_set_2g_short = {
	{0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};

/* Channel 165 */
static const chanvec_t restricted_chan_165 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};

/* Channels 36 - 48 & 149 - 165 */
static const chanvec_t restricted_low_hi = {
	{0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x20, 0x22, 0x22, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};

/* Channels 12 - 14 */
static const chanvec_t restricted_set_12_13_14 = {
	{0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};

/* Channels 52-64, 100-140 and 149-165 */
static const chanvec_t restricted_set_52_above = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11,
	0x01, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x20, 0x22, 0x22, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};


#define WLC_REGCLASS_USA_2G_20MHZ	12
#define WLC_REGCLASS_EUR_2G_20MHZ	4
#define WLC_REGCLASS_JPN_2G_20MHZ	30
#define WLC_REGCLASS_JPN_2G_20MHZ_CH14	31

#ifdef WL11N
/*
 * bit map of supported regulatory class for USA, Europe & Japan
 */
static const rcvec_t rclass_us = { /* 1-5, 12, 22-25, 27-30, 32-33 */
	{0x3e, 0x10, 0xc0, 0x7b, 0x03}
};
static const rcvec_t rclass_eu = { /* 1-4, 5-12 */
	{0xfe, 0x1f, 0x00, 0x00, 0x00}
};
static const rcvec_t rclass_jp = { /* 1, 30-32 */
	{0x01, 0x00, 0x00, 0xc0, 0x01}
};
#endif /* WL11N */

#ifdef BAND5G
/*
 * channel to regulatory class map for USA
 */
static const rcinfo_t rcinfo_us_20 = {
	24,
	{
	{ 36,  1}, { 40,  1}, { 44,  1}, { 48,  1}, { 52,  2}, { 56,  2}, { 60,  2}, { 64,  2},
	{100,  4}, {104,  4}, {108,  4}, {112,  4}, {116,  4}, {120,  4}, {124,  4}, {128,  4},
	{132,  4}, {136,  4}, {140,  4}, {149,  3}, {153,  3}, {157,  3}, {161,  3}, {165,  5}
	}
};
#endif /* BAND5G */

#ifdef WL11N
/* control channel at lower sb */
static const rcinfo_t rcinfo_us_40lower = {
	18,
	{
	{  1, 32}, {  2, 32}, {  3, 32}, {  4, 32}, {  5, 32}, {  6, 32}, {  7, 32}, { 36, 22},
	{ 44, 22}, { 52, 23}, { 60, 23}, {100, 24}, {108, 24}, {116, 24}, {124, 24}, {132, 24},
	{149, 25}, {157, 25}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
/* control channel at upper sb */
static const rcinfo_t rcinfo_us_40upper = {
	18,
	{
	{  5, 33}, {  6, 33}, {  7, 33}, {  8, 33}, {  9, 33}, { 10, 33}, { 11, 33}, { 40, 27},
	{ 48, 27}, { 56, 28}, { 64, 28}, {104, 29}, {112, 29}, {120, 29}, {128, 29}, {136, 29},
	{153, 30}, {161, 30}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
#endif /* WL11N */

#ifdef BAND5G
/*
 * channel to regulatory class map for Europe
 */
static const rcinfo_t rcinfo_eu_20 = {
	19,
	{
	{ 36,  1}, { 40,  1}, { 44,  1}, { 48,  1}, { 52,  2}, { 56,  2}, { 60,  2}, { 64,  2},
	{100,  3}, {104,  3}, {108,  3}, {112,  3}, {116,  3}, {120,  3}, {124,  3}, {128,  3},
	{132,  3}, {136,  3}, {140,  3}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
#endif /* BAND5G */

#ifdef WL11N
static const rcinfo_t rcinfo_eu_40lower = {
	18,
	{
	{  1, 11}, {  2, 11}, {  3, 11}, {  4, 11}, {  5, 11}, {  6, 11}, {  7, 11}, {  8, 11},
	{  9, 11}, { 36,  5}, { 44,  5}, { 52,  6}, { 60,  6}, {100,  7}, {108,  7}, {116,  7},
	{124,  7}, {132,  7}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
static const rcinfo_t rcinfo_eu_40upper = {
	18,
	{
	{  5, 12}, {  6, 12}, {  7, 12}, {  8, 12}, {  9, 12}, { 10, 12}, { 11, 12}, { 12, 12},
	{ 13, 12}, { 40,  8}, { 48,  8}, { 56,  9}, { 64,  9}, {104, 10}, {112, 10}, {120, 10},
	{128, 10}, {136, 10}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
#endif /* WL11N */

#ifdef BAND5G
/*
 * channel to regulatory class map for Japan
 */
static const rcinfo_t rcinfo_jp_20 = {
	8,
	{
	{ 34,  1}, { 38,  1}, { 42,  1}, { 46,  1}, { 52, 32}, { 56, 32}, { 60, 32}, { 64, 32},
	{  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0},
	{  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
#endif /* BAND5G */

#ifdef WL11N
static const rcinfo_t rcinfo_jp_40 = {
	0,
	{
	{  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0},
	{  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0},
	{  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
#endif

#ifdef WL11D

/* All 5 GHz channels restricted */
#define restricted_set_11d_5G chanvec_all_5G

/* All 2.4 GHz channels restricted */
#define restricted_set_11d_2G chanvec_all_2G

#endif /* WL11D */

#define  LOCALE_CHAN_01_11	 (1<<0)
#define  LOCALE_CHAN_12_13	 (1<<1)
#define  LOCALE_CHAN_14		 (1<<2)
#define  LOCALE_SET_5G_LOW_JP1   (1<<3)   /* 34-48, step 2 */
#define  LOCALE_SET_5G_LOW_JP2   (1<<4)   /* 34-46, step 4 */
#define  LOCALE_SET_5G_LOW1      (1<<5)   /* 36-48, step 4 */
#define  LOCALE_SET_5G_LOW2      (1<<6)   /* 52 */
#define  LOCALE_SET_5G_LOW3      (1<<7)   /* 56-64, step 4 */
#define  LOCALE_SET_5G_MID1      (1<<8)   /* 100-116, step 4 */
#define  LOCALE_SET_5G_MID2	 (1<<9)   /* 120-124, step 4 */
#define  LOCALE_SET_5G_MID3      (1<<10)  /* 128 */
#define  LOCALE_SET_5G_HIGH1     (1<<11)  /* 132-140, step 4 */
#define  LOCALE_SET_5G_HIGH2     (1<<12)  /* 149-161, step 4 */
#define  LOCALE_SET_5G_HIGH3     (1<<13)  /* 165 */
#define  LOCALE_CHAN_52_140_ALL  (1<<14)
#define  LOCALE_SET_5G_HIGH4     (1<<15)  /* 184-216 */

#define  LOCALE_CHAN_36_64       LOCALE_SET_5G_LOW1 | LOCALE_SET_5G_LOW2 | LOCALE_SET_5G_LOW3
#define  LOCALE_CHAN_52_64       LOCALE_SET_5G_LOW2 | LOCALE_SET_5G_LOW3
#define  LOCALE_CHAN_100_124	 LOCALE_SET_5G_MID1 | LOCALE_SET_5G_MID2
#define  LOCALE_CHAN_100_140     \
	LOCALE_SET_5G_MID1 | LOCALE_SET_5G_MID2 | LOCALE_SET_5G_MID3 | LOCALE_SET_5G_HIGH1
#define  LOCALE_CHAN_149_165     LOCALE_SET_5G_HIGH2 | LOCALE_SET_5G_HIGH3
#define  LOCALE_CHAN_184_216     LOCALE_SET_5G_HIGH4

#define  LOCALE_CHAN_01_14	(LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13 | LOCALE_CHAN_14)

#define  LOCALE_RADAR_SET_NONE		  0
#define  LOCALE_RADAR_SET_1		  1

#define  LOCALE_RESTRICTED_NONE		  0
#define  LOCALE_RESTRICTED_SET_2G_SHORT   1
#define  LOCALE_RESTRICTED_CHAN_165       2
#define  LOCALE_CHAN_ALL_5G		  3
#define  LOCALE_RESTRICTED_JAPAN_LEGACY   4
#define  LOCALE_RESTRICTED_11D_2G	  5
#define  LOCALE_RESTRICTED_11D_5G	  6
#define  LOCALE_RESTRICTED_LOW_HI	  7
#define  LOCALE_RESTRICTED_12_13_14	  8
#define  LOCALE_RESTRICTED_52_ABOVE	  9

#define  IS_CCODE_REV(cm, cntry, rev)	(!strncmp(cm->ccode, cntry, WLC_CNTRY_BUF_SZ) && \
					 (cm->regrev == (rev)))

/* global memory to provide working buffer for expanded locale */

#ifdef BAND5G     /* RADAR */
static const chanvec_t * g_table_radar_set[] =
{
	&chanvec_none,
	&radar_set1
};
#endif

static const chanvec_t * g_table_restricted_chan[] =
{
	&chanvec_none, 			/* restricted_set_none */
	&restricted_set_2g_short,
	&restricted_chan_165,
	&chanvec_all_5G,
	&restricted_set_japan_legacy,
	&chanvec_all_2G, 		/* restricted_set_11d_2G */
	&chanvec_all_5G, 		/* restricted_set_11d_5G */
	&restricted_low_hi,
	&restricted_set_12_13_14,
	&restricted_set_52_above
};

static const chanvec_t locale_2g_01_11 = {
	{0xfe, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_2g_12_13 = {
	{0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_2g_14 = {
	{0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};

static const chanvec_t locale_5g_LOW_JP1 = {
	{0x00, 0x00, 0x00, 0x00, 0x54, 0x55, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_5g_LOW_JP2 = {
	{0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_5g_LOW1 = {
	{0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_5g_LOW2 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_5g_LOW3 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_5g_MID1 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_5g_MID2 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_5g_MID3 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_5g_HIGH1 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x10, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_5g_HIGH2 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x20, 0x22, 0x02, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_5g_HIGH3 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_5g_52_140_ALL = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};
static const chanvec_t locale_5g_HIGH4 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
	0x11, 0x11, 0x11, 0x11}
};

static const chanvec_t * g_table_locale_base[] =
{
	&locale_2g_01_11,
	&locale_2g_12_13,
	&locale_2g_14,
	&locale_5g_LOW_JP1,
	&locale_5g_LOW_JP2,
	&locale_5g_LOW1,
	&locale_5g_LOW2,
	&locale_5g_LOW3,
	&locale_5g_MID1,
	&locale_5g_MID2,
	&locale_5g_MID3,
	&locale_5g_HIGH1,
	&locale_5g_HIGH2,
	&locale_5g_HIGH3,
	&locale_5g_52_140_ALL,
	&locale_5g_HIGH4
};
void wlc_locale_add_channels(chanvec_t *target, const chanvec_t *channels)
{
	uint8 i;
	for (i = 0; i < sizeof(chanvec_t); i++) {
		target->vec[i] |= channels->vec[i];
	}
}

void wlc_locale_get_channels(const locale_info_t * locale, chanvec_t *channels)
{
	uint8 i;

	bzero(channels, sizeof(chanvec_t));

	for (i = 0; i < ARRAYSIZE(g_table_locale_base); i++) {
		if (locale->valid_channels & (1<<i)) {
			wlc_locale_add_channels(channels, g_table_locale_base[i]);
		}
	}
}

/*
 * Locale Definitions - 2.4 GHz
 */

#ifdef BAND2G
static const locale_info_t locale_a = {	/* locale a. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
#if 1	/* ASUS modify */
	{QDB(18.5), QDB(20.5), QDB(18.5),
	QDB(13.5), QDB(17.5), /* 16.5 dBm */ 54},
#else
	{QDB(19), QDB(19), QDB(19),
	QDB(19), QDB(19), /* 16.5 dBm */ 66},
#endif
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a_1 = {	/* locale a. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(19), QDB(19), QDB(19),
	QDB(15), QDB(17), QDB(16)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a_2 = {	/* locale a. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(19), QDB(19), QDB(19),
	QDB(18), QDB(18), QDB(18)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a_3 = {	/* locale a_3. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(15), QDB(17), QDB(15),
	QDB(13), QDB(17), QDB(13)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a_5 = {	/* locale a_5. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 16.5 */ 66, 66, 66,
	/* 12.5 */ 50, /* 16.5 */ 66, 66},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a_6 = {	/* locale a_6. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(20), QDB(20), QDB(20),
	QDB(17), QDB(17), QDB(16)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a1 = {	/* locale a. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(18), QDB(19), QDB(18),
	QDB(19), QDB(19), /* 16.5 dBm */ 66},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM v1.12.2.3 has separate power targets for 2.4G OFDM ch 11 and 12/13.
 * Express ch 12/13 limits here and fixup in wlc_channel_reg_limits().
 * CLM v2.1.3 has 2.4G CCK allowed but OFDM prohibited, express CCK here and
 * hack for OFDM in wlc_phy.c
 */
static const locale_info_t locale_a2 = {	/* locale a. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_12_13_14,
	{QDB(19), QDB(19), QDB(12),
	QDB(19), QDB(19), QDB(11)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a3 = {	/* locale a3. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(18), QDB(19), QDB(18),
	QDB(17), QDB(19), QDB(16)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM v3.1 has separate power targets for 2.4G CCK/OFDM ch 3-9 and 2/10.
 * Express ch 3-9 limits here and fixup 2/10 in wlc_channel_reg_limits().
 */
static const locale_info_t locale_a3_1 = {	/* locale a3. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(18), QDB(19), QDB(18),
	QDB(14), QDB(17), QDB(12)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM v7.0 has separate power targets for 2.4G CCK/OFDM ch 3-9 and 2/10.
 * Express ch 3-9 limits here and fixup 2/10 in wlc_channel_reg_limits().
 */
static const locale_info_t locale_a4 = {	/* locale a4. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(21), QDB(23), /* 20.5 dBm */ 82,
	QDB(18), QDB(23), /* 16.5 dBm */ 66},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a4_2 = {	/* locale a4_2. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(21), QDB(23), QDB(21),
	QDB(18), QDB(23), QDB(18)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM v4.3.4 has separate power targets for 2.4G CCK/OFDM ch 3-9 and 2/10.
 * Express ch 3-9 limits here and fixup 2/10 in wlc_channel_reg_limits().
 */
static const locale_info_t locale_a4_3 = {	/* locale a4_3. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(19), QDB(23), QDB(19),
	QDB(16), QDB(23), QDB(16)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM v4.8.2 has separate power targets for 2.4G CCK/OFDM ch 3-9 and 2/10.
 * Express ch 3-9 limits here and fixup 2/10 in wlc_channel_reg_limits().
 */

static const locale_info_t locale_a4_4 = {	/* locale a4_4. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 22.5 */ 90, /* 26.5 */ 106, /* 22.5 */ 90,
	  /* 18.5 */ 74, /* 25.5 */ 102, /* 18.5 */ 74 },
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM v2.1.1 has separate power targets for 2.4G CCK/OFDM ch 2, 3-9 and 10.
 * Express ch 2-9 limits here and fixup 10 in wlc_channel_reg_limits().
 */
static const locale_info_t locale_a5 = {	/* locale a5. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 19.5 dBm */ 78, QDB(21), QDB(18),
	QDB(17), QDB(20), QDB(17)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a5_1 = {	/* locale a5_1. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(18), QDB(18), QDB(18),
	QDB(14), QDB(18), QDB(14)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM v2.1.4 has separate power targets for 2.4G CCK/OFDM ch 2, 3-9 and 10.
 * Express ch 3-9 limits here and fixup 10 in wlc_channel_reg_limits().
 */
static const locale_info_t locale_a6 = {	/* locale a5. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(20), QDB(21), /* 19.5 dBm */ 78,
	/* 16.5 dBm */ 66, QDB(21), /* 16.5 dBm */ 66},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM v3.2 has separate power targets for 2.4G CCK/OFDM ch 2, 3-9 and 10.
 * Express ch 3-9 limits here and fixup 10 in wlc_channel_reg_limits().
 */
static const locale_info_t locale_a6_1 = {	/* locale a5. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(20), QDB(21), /* 18.5 dBm */ 74,
	QDB(14), QDB(21), QDB(14)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM v3.7 has separate power targets for 2.4G CCK/OFDM ch 2, 3-9 and 10.
 * Express ch 3-9 limits here and fixup 10 in wlc_channel_reg_limits().
 */
static const locale_info_t locale_a6_2 = {	/* locale a6_2. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 18.5 dBm */ 74, /* 21.5 dBm */ 86, 74,
	QDB(14), QDB(19), QDB(14)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a6_3 = {	/* locale a6_3. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 21.5 dBm */ 86, 86, 86,
	/* 17.5 */ 70, /* 21.5 dBm */ 86, 70},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a6_8 = {	/* locale a6_8. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(19), QDB(20), /* 20.5 dBm */ 82,
	QDB(15), /* 21.5 dBm */ 86, QDB(18)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a7 = {	/* locale a7 channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
#if 1	/* ASUS modify */
	{QDB(30), QDB(30), QDB(30),
	QDB(30), QDB(30), /* 13.5 dBm */ 120},
#else
	{QDB(19), QDB(19), QDB(17),
	QDB(18), QDB(19), /* 13.5 dBm */ 54},
#endif
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM v2.3 has separate power targets for 2.4G OFDM ch 2, 3, 4-8, 9 and 10.
 * Express ch 4-8 limits here and fixup others in wlc_channel_reg_limits().
 */
static const locale_info_t locale_a9 = {	/* locale a9 channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(19), QDB(19), QDB(18),
	QDB(13), QDB(18), QDB(13)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a10 = {	/* locale a9 channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(17), QDB(17), QDB(17),
	QDB(17), QDB(17), QDB(17)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a10_1 = {
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(17), QDB(17), QDB(17),
	/* 14.5 */ 58, /* 16.5 dBm */ 66, 58},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a10_2 = {
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 16.5 dBm */ 66, 66, 66,
	QDB(14), /* 14.5 */ 58, 58},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a10_3 = {
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(18), QDB(18), QDB(18),
	/* 15.5 */ 62, QDB(16), QDB(16)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a11 = {
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 16.5 */ 66, 66, 66,
	QDB(13), 66, /* 12.75 */ 51},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a12 = {	/* locale a12 channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
#if 1	/* ASUS modify */
	{QDB(22.5), QDB(25.5), QDB(22.5),
	QDB(19.5), QDB(25.5), QDB(19.5)},
#else
	{QDB(17), QDB(17), QDB(14),
	QDB(10), QDB(10), QDB(10)},
#endif
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM 4.3.3 added Locale A14, Chan 1-11, both CCK & OFDM Express Conducted */
static const locale_info_t locale_a14 = {	/* locale a14 channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 18.5 dBm */ 74, QDB(19), QDB(17),
	/* 17.5 dBm */70, 70, QDB(15)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_a20 = {	/* locale a20. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(19), QDB(19), QDB(19),
	 QDB(14), QDB(19), QDB(14)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b = {	/* locale b. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(19), QDB(19), QDB(19),
	 QDB(19), QDB(19), QDB(19)},
	{20, 20, 20, 0},
	WLC_EIRP | WLC_FILT_WAR
};

static const locale_info_t locale_b_1 = {	/* locale b. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(19), QDB(19), QDB(19),
	 QDB(18), QDB(18), QDB(18)},
	{20, 20, 20, 0},
	WLC_EIRP
};

static const locale_info_t locale_b_2 = {	/* locale b. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(17), QDB(18), QDB(17),
	 QDB(17), QDB(18), QDB(17)},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b_3 = {	/* locale b_3. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 14.5 */ 58, 58, 58,
	58, 58, 58},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b_4 = {	/* locale b_4. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(16), QDB(16), QDB(16),
	/* 16.5 */ 66, 66, 66},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b_9 = {	/* locale b_9. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
#if 1	/* ASUS modify */
	{ QDB(18), QDB(18), QDB(18),
	QDB(18), QDB(18), QDB(18)},
#else
	{ QDB(18), QDB(18), QDB(18),
	QDB(18), QDB(18), QDB(18)},
#endif
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b_11 = {	/* locale b_11. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 16.5 */ 66, 66, 66,
	/* 16.5 */ 66, 66, 66},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b_5 = {	/* locale b_5. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
#if 1	/* ASUS modify */
	{QDB(16.5), QDB(16.5), QDB(16.5),
	 QDB(16.5), QDB(16.5), QDB(16.5)},
#else
	{QDB(19), QDB(19), QDB(19),
	 QDB(19), QDB(19), QDB(19)},
#endif
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b_6 = {	/* locale b_6. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(18), QDB(18), QDB(18),
	 QDB(18), QDB(18), QDB(18)},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b_7 = {	/* locale b_7. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(17), QDB(17), QDB(17),
	 QDB(17), QDB(17), QDB(17)},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b_8 = {	/* locale b_8. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(16), QDB(16), QDB(16),
	 QDB(16), QDB(16), QDB(16)},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b2 = { /* locale b. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_12_13_14,
	{QDB(19), QDB(19), QDB(19),
	 QDB(19), QDB(19), QDB(19)},
	{20, 20, 20, 0},
	WLC_EIRP
};

static const locale_info_t locale_b2_1 = { /* locale b. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_12_13_14,
	{QDB(16), QDB(16), QDB(12),
	 QDB(14), QDB(16), QDB(11)},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b2_2 = { /* locale b. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_12_13_14,
	{QDB(16), QDB(16), QDB(12),
	 QDB(16), QDB(16), QDB(11)},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b2_3 = { /* locale b. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_12_13_14,
	{QDB(16), QDB(16), QDB(16),
	 QDB(16), QDB(16), QDB(16)},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b2_4 = { /* locale b. channel 1 - 13 */
	LOCALE_CHAN_01_11 |  LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_12_13_14,
	{QDB(15), QDB(15), QDB(15),
	 /* 13.5 dBm */ 54, 54, 54},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b2_5 = { /* locale b. channel 1 - 13 */
	LOCALE_CHAN_01_11 |  LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_12_13_14,
	{QDB(16), QDB(16), QDB(16),
	 QDB(16), QDB(16), QDB(16)},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b2_6 = { /* locale b. channel 1 - 13 */
	LOCALE_CHAN_01_11 |  LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_12_13_14,
	{QDB(16), QDB(16), QDB(12),
	 QDB(16), QDB(16), QDB(11)},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};


/* CLM 1.12.4 has locale b3. EIRP for CCK but conducted for OFDM
 * Express EIRP here and fixup OFDM in wlc_channel_reg_limits().
 */
static const locale_info_t locale_b3 = {
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(19), QDB(19), QDB(19),
	QDB(11), QDB(16), QDB(11)},
	{20, 20, 20, 0},
	WLC_EIRP
};

static const locale_info_t locale_b3_1 = { /* locale b. channel 1 - 14 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(15), QDB(15), QDB(15),
	 QDB(15), QDB(15), QDB(15)},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b3_2 = { /* locale b. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(13), QDB(13), QDB(13),
	 0, 0, 0},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM 2.1.1 has locale b4. EIRP for CCK but conducted for OFDM
 * Express EIRP here and fixup OFDM in wlc_channel_reg_limits().
 */
static const locale_info_t locale_b4 = {
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
#if 1	/* ASUS modify */
	{QDB(30), QDB(30), QDB(30),
	QDB(30), QDB(30), QDB(30)},
	{30, 30, 30, 0},
#else
	{QDB(19), QDB(19), QDB(19),
	QDB(16), QDB(17), QDB(16)},
	{20, 20, 20, 0},
#endif
	WLC_EIRP
};

static const locale_info_t locale_b5 = { /* locale b5. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_12_13_14,
	{ /* 16.5 dBm */ 66, /* 16.5 dBm */ 66, /* 16.5 dBm */ 66,
	/* 15.5 dBm */ 62, /* 16.5 dBm */ 66, /* 16.5 dBm */ 66},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

/* CLM v2.8 has separate power targets for 2.4G CCK/OFDM ch 11-14.
 * Express some limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_info_t locale_b5_2 = { /* locale b5-2. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_12_13_14,
	{QDB(16), QDB(16), QDB(16),
	 /* 12.5 dBm */ 50, QDB(15), /* 12.5 dBm */ 50},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

/* Locale B5-3 has separate power targets for 2.4G OFDM ch 12-13.
 * Express some limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_info_t locale_b5_3 = { /* locale b5-3. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_12_13_14,
	{ /* 16.5 */ 66, 66, 66,
	QDB(13), 66, 66},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_b5_4 = { /* locale b5-4. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_12_13_14,
	{ /* 16.5 */ 66, 66, 66,
	66, 66, 66},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_c = {	/* locale c. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(19), QDB(19), QDB(19),
	 QDB(19), QDB(19), QDB(19)},
	/* Tx Max Power is given as 10dBm/MHz. To translate to max power for 20 MHz channel width:
	 * 10dBm/MHz * 20MHz
	 * 10dBm + dB(20)
	 * 10dBm + 10*log10(20)
	 * 10dBm + 13dB
	 * = 23 dBm
	 */
	{23, 23, 23, 0},
	WLC_EIRP
};

static const locale_info_t locale_c_1 = {	/* locale c-1. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(15), QDB(15), QDB(15),
	 QDB(15), QDB(15), QDB(15)},
	/* Tx Max Power is given as 10dBm/MHz. To translate to max power for 20 MHz channel width:
	 * 10dBm/MHz * 20MHz
	 * 10dBm + dB(20)
	 * 10dBm + 10*log10(20)
	 * 10dBm + 13dB
	 * = 23 dBm
	 */
	{23, 23, 23, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_c_2 = {	/* locale c-2. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 16.5 */ 66, 66, 66,
	66, 66, 66},
	{23, 23, 23, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_c_5 = {	/* locale c-5. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 15.5 */ 62, 62, 62,
	/* 17.5 */ 70, 70, 70},
	{23, 23, 23, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_c_4 = {	/* locale c-4. channel 1 - 14 */
	LOCALE_CHAN_01_11 |  LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 13.5 */ 54, 54, 54,
	QDB(14), QDB(14), QDB(14)},
	{23, 23, 23, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_f = {	/* locale f. no channel */
	0,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{0, 0, 0,
	 0, 0, 0},
	{0, 0, 0, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_g = {	/* locale g. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(23), QDB(23), QDB(23),
	 QDB(23), QDB(23), QDB(23)},
	{23, 23, 23, 0},
	WLC_EIRP
};

static const locale_info_t locale_h = {	/* locale h. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(25), QDB(25), QDB(25),
	 QDB(25), QDB(25), QDB(25)},
	{36, 36, 36, 0},
	WLC_EIRP
};

static const locale_info_t locale_i = {	/* locale i. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_SET_2G_SHORT,
	{QDB(19), QDB(19), QDB(19),
	 QDB(19), QDB(19), QDB(19)},
	{20, 20, 20, 0},
	WLC_EIRP | WLC_FILT_WAR
};

static const locale_info_t locale_i_1 = {	/* locale i_1. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_SET_2G_SHORT,
	{QDB(15), QDB(15), QDB(15),
	 QDB(14), QDB(16), QDB(16)},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_j = {	/* locale j. channel 1 - 11 */
	LOCALE_CHAN_01_11,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(19), QDB(19), QDB(19),
	QDB(19), QDB(19), QDB(19)},
	{20, 20, 20, 0},
	WLC_EIRP
};

/* CLM 2.0 has locale b3. EIRP for CCK but conducted for OFDM
 * Express EIRP here and fixup OFDM in wlc_channel_reg_limits().
 */
static const locale_info_t locale_k = {	/* locale k. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(19), QDB(19), QDB(19),
	QDB(14), QDB(16), QDB(16)},
	{20, 20, 20, 0},
	WLC_EIRP
};

static const locale_info_t locale_k_1 = {	/* locale k_1. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 16.5 */ 66, 66, 66,
	66, 66, 66},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_k_3 = {	/* locale k_3. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 17.5 */ 70, 70, 70,
	QDB(18), QDB(18), QDB(18)},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_k_4 = {	/* locale k_4. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ /* 16.5 */ 66, 66, 66,
	66, 66, 66},
	{20, 20, 20, 0},
	WLC_PEAK_CONDUCTED
};

/* same as locale_c except no ch 14 */
static const locale_info_t locale_c1 = {	/* locale c. channel 1 - 13 */
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(19), QDB(19), QDB(19),
	 QDB(19), QDB(19), QDB(19)},
	/* Tx Max Power is given as 10dBm/MHz. To translate to max power for 20 MHz channel width:
	 * 10dBm/MHz * 20MHz
	 * 10dBm + dB(20)
	 * 10dBm + 10*log10(20)
	 * 10dBm + 13dB
	 * = 23 dBm
	 */
	{23, 23, 23, 0},
	WLC_EIRP
};

static const locale_info_t locale_2G_band_all = {	/* locale testing. channel 1 - 14 */
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(30), QDB(30), QDB(30),
	 QDB(30), QDB(30), QDB(30)},
	{30, 30, 30, 0},
	WLC_PEAK_CONDUCTED
};
#endif /* BAND2G */

/*
 * Locale Definitions - 5 GHz
 */

#ifdef BAND5G
/* a band locale table */
static const locale_info_t locale_1 = {	/* locale 1. channel 36 - 48, 52 - 64, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), /* 17.5 */ 70, /* 14.5 */ 58, 0, /* 17.5 */ 70},
	{17, 24, 24, 0, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_1a = { /* locale 1a. channel 36 - 48, 52 - 64, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_CHAN_165,
	{QDB(14), /* 17.5 */ 70, /* 14.5 */ 58, 0, /* 17.5 */ 70},
	{17, 24, 24, 0, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_1c = { /* locale 1c. channel 36 - 48, 52 - 64, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(13), /* 17.5 */ 70, /* 14.5 */ 58, 0, /* 17.5 */ 70},
	{17, 24, 24, 0, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_1r = { /* locale 1r. channel 36 - 48, 52 - 64, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_CHAN_ALL_5G,
	{QDB(14), /* 17.5 */ 70, /* 14.5 */ 58, 0, /* 17.5 */ 70},
	{17, 24, 24, 0, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_2 = {	/* locale 2. channel 36 - 48, 52 - 64, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_HIGH1 | LOCALE_SET_5G_HIGH2,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(16), QDB(16), QDB(16), QDB(16), QDB(16)},
	{23, 24, 24, 0, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_3 = {	/* locale 3. channel 36 - 48, 52 - 64, 100 - 140 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(21), QDB(21), QDB(21), QDB(21), 0},
	{23, 23, 23, 30, 0},
	WLC_EIRP | WLC_DFS_EU
};

/* CLM v4.6.6 */
static const locale_info_t locale_3_1 = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{74, 74, 74, 86, 0},
	{23, 23, 23, 30, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_3b = { /* locale 3b. channel 36 - 48, 52 - 64, 100 - 140 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 20.5 */ 82, 82, 82, 82, 0},
	{23, 23, 23, 30, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_3d = { /* locale 3d. channel 36 - 48, 52 - 64, 100 - 140 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 15.5 */ 62, 62, 62, /* 14.5 */ 58, 0},
	{23, 23, 23, 30, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

/* locale 3j. channel 36 - 48, 52 - 64, 100 - 140 */
static const locale_info_t locale_3j = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), QDB(14), QDB(14), QDB(15), 0},
	{23, 23, 23, 30, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* locale 3j-1. channel 36 - 48, 52 - 64, 100 - 140 */
static const locale_info_t locale_3j_1 = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 15.5 */ 62, 62, 62, /* 16.5 */ 66, 0},
	{23, 23, 23, 30, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* locale 3j-3. channel 36 - 48, 52 - 64, 100 - 140 */
static const locale_info_t locale_3j_3 = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 15.5 */ 62, 62, 62, /* 14.5 */ 58, 0},
	{23, 23, 23, 30, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* locale 3j-4. channel 36 - 48, 52 - 64, 100 - 140 */
static const locale_info_t locale_3j_4 = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 13.5 */ 54, 54, 54, 54, 0},
	{23, 23, 23, 30, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* locale 3j-5. channel 36 - 48, 52 - 64, 100 - 140 */
static const locale_info_t locale_3j_5 = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ QDB(15), QDB(10), QDB(10), QDB(16), 0},
	{23, 23, 23, 30, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* locale 3j-8. channel 36 - 48, 52 - 64, 100 - 140 */
static const locale_info_t locale_3j_8 = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ QDB(21), QDB(21), QDB(21), QDB(21), 0},
	{23, 23, 23, 30, 0},
	WLC_EIRP | WLC_DFS_FCC
};

/* 100-116 has different power than 132-140, fixup in wlc_channel_reg_limits(). */
/* locale 3l. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140 */
static const locale_info_t locale_3l = {
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 13.5 dBm */ 54, 54, QDB(13), QDB(13), 0},
	{23, 23, 23, 30, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

/* locale 3l_1. channel 36 - 48, 52 - 64, 100 - 140 */
static const locale_info_t locale_3l_1 = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 13.5 dBm */ 54, 54, 54, 54, 0},
	{23, 23, 23, 30, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};


/* locale 3r. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140 */
static const locale_info_t locale_3r = {
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(21), QDB(21), QDB(21), QDB(21), 0},
	{23, 23, 23, 30, 0},
	WLC_EIRP | WLC_DFS_EU
};

/* locale 3r_4. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140 */
static const locale_info_t locale_3r_4 = {
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(16), QDB(16), QDB(16), QDB(16), 0},
	{23, 23, 23, 30, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

/* locale 3r_5. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140 */
static const locale_info_t locale_3r_5 = {
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(21), QDB(21), QDB(21), QDB(28), 0},
	{23, 23, 23, 30, 0},
	WLC_EIRP | WLC_DFS_EU
};

/* locale 3s. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140 */
static const locale_info_t locale_3s = {
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{ QDB(21), QDB(21), QDB(21), QDB(21), 0},
	{23, 23, 23, 30, 0},
	WLC_EIRP | WLC_DFS_EU
};

static const locale_info_t locale_4 = {	/* locale 4. channel 34 - 46 */
	LOCALE_SET_5G_LOW_JP2,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(21), 0, 0, 0, 0},
	{23, 0, 0, 0, 0},
	WLC_EIRP
};

static const locale_info_t locale_5 = {	/* locale 5. channel 36 - 48, 52 - 64, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(21), QDB(21), QDB(21), 0, QDB(21)},
	{23, 23, 23, 0, 30},
	WLC_EIRP | WLC_DFS_EU
};

/* locale 5a. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140, 149 - 165 */
static const locale_info_t locale_5a = {
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(21), QDB(21), QDB(21), QDB(21), QDB(21)},
	{23, 23, 23, 30, 30},
	WLC_EIRP | WLC_DFS_EU
};

/* locale 5l. channel 36 - 48, 52 - 64, 149 - 165 */
static const locale_info_t locale_5l = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 13.5 dBm */ 54, 54, QDB(13), 0, QDB(15)},
	{23, 23, 23, 0, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

/* locale 5l_1. channel 36 - 48, 52 - 64, 149 - 165 */
static const locale_info_t locale_5l_1 = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 13.5 dBm */ 54, 54, 54, 0, QDB(15)},
	{23, 23, 23, 0, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

/* locale 5l_2. channel 36 - 48, 52 - 64, 149 - 165 */
static const locale_info_t locale_5l_2 = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ QDB(15), QDB(15), QDB(15), 0, QDB(16)},
	{23, 23, 23, 0, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

/* locale 6. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
static const locale_info_t locale_6 = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(21), QDB(21), QDB(21), QDB(21), QDB(21)},
	{23, 23, 23, 24, 30},
	WLC_EIRP | WLC_DFS_FCC
};

/* locale 6a_1. channel 36 - 48, 52 - 64, 149 - 165 */
static const locale_info_t locale_6a_1 = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(15), QDB(15), QDB(15), 0, QDB(17)},
	{23, 23, 23, 0, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* locale 6a. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
static const locale_info_t locale_6a = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 13.5 */ 54, 54, 54, 54, QDB(15)},
	{23, 23, 23, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_7 = {	/* locale 7. 149 - 165 */
	LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{0, 0, 0, 0, QDB(21)},
	{0, 0, 0, 0, 27},
	WLC_EIRP
};

static const locale_info_t locale_7c = {	/* locale 7c. 149 - 165 */
	LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{0, 0, 0, 0, QDB(21)},
	{0, 0, 0, 0, 27},
	WLC_EIRP
};

static const locale_info_t locale_7c_1 = {	/* locale 7c_1. 149 - 165 */
	LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{0, 0, 0, 0, QDB(17)},
	{0, 0, 0, 0, 27},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_8 = {		/* locale 8. channel 56 - 64, 149 - 165 */
	LOCALE_SET_5G_LOW3 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{0, QDB(17), QDB(17), 0, QDB(17)},
	{0, 24, 24, 0, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* locale 8. channel 56 - 64, 100 - 140, 149 - 165 */
static const locale_info_t locale_8a = {
	LOCALE_SET_5G_LOW3 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{0, QDB(14), QDB(14), QDB(17), QDB(17)},
	{0, 17, 17, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* locale 8a_1. channel 56 - 64, 100 - 140, 149 - 165 */
static const locale_info_t locale_8a_1 = {
	LOCALE_SET_5G_LOW3 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{0, QDB(15), QDB(15), QDB(17), QDB(17)},
	{0, 17, 17, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* locale 8b.  channel 56 - 64, 100 - 140, 149 - 165,
 * channel 100, 140  need fixup in wlc_channel_reg_limits()
*/
static const locale_info_t locale_8b = { /* locale 8. channel 56 - 64, 149 - 165 */
	LOCALE_SET_5G_LOW3 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{0, /* 13.5 */ 54, 54, QDB(17), /* 17.5 */ 70},
	{0, 17, 17, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_9 = {		/* locale 9. channel 149 - 161 */
	LOCALE_SET_5G_HIGH2,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{0, 0, 0, 0, QDB(21)},
	{0, 0, 0, 0, 27},
	WLC_EIRP
};

static const locale_info_t locale_9h = {		/* locale 9h. channel 149 - 161 */
	LOCALE_SET_5G_HIGH2,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{0, 0, 0, 0, QDB(23)},
	{0, 0, 0, 0, 33},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_10 = {	/* locale 10. channel 36 - 48, 149 - 165 */
	LOCALE_SET_5G_LOW1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	 {QDB(19), 0, 0, 0, QDB(19)},
	 {20, 0, 0, 0, 20},
	 WLC_EIRP
};

static const locale_info_t locale_11 = {
	/* locale 11. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_CHAN_ALL_5G,
	{QDB(21), QDB(21), QDB(21), QDB(21), QDB(21)},
	{23, 17, 17, 23, 30},
	WLC_EIRP | WLC_DFS_EU
};

static const locale_info_t locale_11_1 = {
	/* locale 11_1. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_CHAN_ALL_5G,
	{50 /* 12.5 dbm */, 50 /* 12.5 dbm */, 50 /* 12.5 dbm */,\
	 50 /* 12.5 dbm */, 50 /* 12.5 dbm */},
	{23, 23, 23, 23, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_11_2 = {
	/* locale 11_2. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_CHAN_ALL_5G,
	{ QDB(14), QDB(15), QDB(14), QDB(15), QDB(15)},
	{23, 23, 23, 23, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_11_3 = {
	/* locale 11_3. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_CHAN_ALL_5G,
	{ QDB(14), 58, 58, QDB(15), 70},
	{23, 23, 23, 23, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_11_4 = {
	/* locale 11_4. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_52_ABOVE,
	{ QDB(14), 58, 58, QDB(15), 70},
	{23, 23, 23, 23, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

/* locale 11l. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
static const locale_info_t locale_11l = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_CHAN_ALL_5G,
	{ /* 13.5 dBm */ 54, /* 12.5 dBm */ 50, 50, QDB(15), QDB(15)},
	{23, 17, 17, 23, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

/* locale 11l_1 channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
static const locale_info_t locale_11l_1 = {
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_CHAN_ALL_5G,
	{ /* 13.5 dBm */ 54, 54, 54, QDB(15), QDB(15)},
	{23, 17, 17, 23, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_12 = {	/* locale 12. channel 149 - 161 */
	LOCALE_SET_5G_HIGH2,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{0, 0, 0, 0, QDB(18)},
	{0, 0, 0, 0, 20},
	WLC_EIRP
};

static const locale_info_t locale_13 = {	/* locale 13. channel 36 - 48, 52 - 64 */
	LOCALE_CHAN_36_64,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(21), QDB(21), QDB(21), 0, 0},
	{23, 23, 23, 0, 0},
	WLC_EIRP | WLC_DFS_EU
};

static const locale_info_t locale_13_1 = { /* locale 13_1. channel 36 - 48, 52 - 64 */
	LOCALE_CHAN_36_64,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 13.5 */ 54, 54, 54, 0, 0},
	{23, 23, 23, 0, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_13_2 = {	/* locale 13_2. channel 36 - 48, 52 - 64 */
	LOCALE_CHAN_36_64,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(16), QDB(16), QDB(16), 0, 0},
	{23, 23, 23, 0, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_13_3 = {	/* locale 13_3. channel 36 - 48, 52 - 64 */
	LOCALE_CHAN_36_64,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 15.5 */ 62, 62, 62, 0, 0},
	{23, 23, 23, 0, 0},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_14 = {	/* locale 14. channel 36 - 48, 52 - 64, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(21), QDB(21), QDB(21), 0, QDB(21)},
	{21, 21, 21, 0, 21},
	WLC_EIRP | WLC_DFS_EU
};

static const locale_info_t locale_14a = { /* locale 14a. channel 36 - 48, 52 - 64, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 13.5 */ 54, 54, 54, 0, QDB(15)},
	{21, 21, 21, 0, 21},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_15 = {	/* locale 15. no channel */
	0,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_16 = {	/* locale 16. channel 36 - 48, 52 - 64, 149 - 161 */
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_HIGH2,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 16.5 */ 66, 66, 66, 0, 66},
	{23, 23, 23, 0, 23},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_16a = { /* locale 16a. channel 36 - 48, 52 - 64, 149 - 161 */
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_HIGH2,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 13.5 */ 54, 54, 54, 0, QDB(15)},
	{23, 23, 23, 0, 23},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_17 = {	/* locale 17. channel 36 - 48, 52 - 64, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 16.5 */ 66, 66, 66, 0, QDB(16)},
	{23, 23, 23, 0, 20},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_17a = { /* locale 17. channel 36 - 48, 52 - 64, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 13.5 */ 54, 54, 54, 0, QDB(15)},
	{23, 23, 23, 0, 20},
	WLC_PEAK_CONDUCTED | WLC_DFS_EU
};

static const locale_info_t locale_18 = {	/* locale 18. channel 36 - 48 */
	LOCALE_SET_5G_LOW1,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(21), 0, 0, 0, 0},
	{23, 0, 0, 0, 0},
	WLC_EIRP
};

static const locale_info_t locale_18_2 = {	/* locale 18_2. channel 36 - 48 */
	LOCALE_SET_5G_LOW1,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(15), 0, 0, 0, 0},
	{23, 0, 0, 0, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_18l= {	/* locale 18l channel 36 - 48 */
	LOCALE_SET_5G_LOW1,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), 0, 0, 0, 0},
	{20, 0, 0, 0, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_19 = {
	/* locale 19. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), /* 17.5 */ 70, /* 14.5 */ 58, QDB(17), /* 17.5 */ 70},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_19_1 = {
	/* locale 19-1. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), QDB(17), QDB(14), QDB(13), QDB(17)},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_19_2 = {
	/* locale 19-2. channel 36 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), QDB(16), QDB(16), QDB(17), /* 20.5 */ 82},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_19_3 = {
	/* locale 19-2. channel 36 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), QDB(16), QDB(14), QDB(16), QDB(16)},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_19_4 = {
	/* locale 19-4. channel 36, 40 - 48, 52-60, 64, 100, 104 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 15.5 */ 62 , 62, QDB(13), /* 14.5 */ 58, 58},
	{17, 24, 24, 24, 24},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_19a = {
	/* locale 19. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_CHAN_165,
	{QDB(14), /* 17.5 */ 70, /* 14.5 */ 58, QDB(17), /* 17.5 */ 70},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_19r = {
	/* locale 19. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_CHAN_165,
	{QDB(14), /* 17.5 */ 70, /* 14.5 */ 58, QDB(17), /* 17.5 */ 70},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_19h_2 = {
	/* locale 19h_2. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 15.5 */ 62, /* 21.5 */ 86, /* 20.5 */ 82, 86, 86},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_19b = {
	/* locale 19. channel 36 - 48, 52 - 64, 100 - 140 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), /* 17.5 */ 70, /* 14.5 */ 58, QDB(17), 0},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_19a_1 = {
	/* locale 19-1. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), QDB(15), QDB(14), QDB(15), QDB(15)},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_19h_1 = {
	/* locale 19h_1. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), /* 17.5 */ 70, /* 15.5 */ 62, /* 16.5 */ 66, /* 18.5 */ 74},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* Channel 140 gets overriden */
static const locale_info_t locale_19l_1 = {
	/* locale 19l-1. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_CHAN_165,
	{QDB(14), /* 17.5 dBm */ 70, /* 14.5 dBm */ 58, QDB(17), /* 17.5 dBm */ 70},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* Channels 100 and 140 get overriden */
static const locale_info_t locale_19l_2 = {
	/* locale 19l-1. channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_CHAN_165,
	{QDB(13), /* 17.5 dBm */ 70, /* 14.5 dBm */ 58, QDB(17), /* 17.5 dBm */ 70},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_20 = {	/* locale 20. channel 100 - 140 */
	LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{0, 0, 0, QDB(21), 0},
	{0, 0, 0, 30, 0},
	WLC_EIRP | WLC_DFS_FCC
};

static const locale_info_t locale_21 = {	/* locale 21. channel 52 - 64, 149 - 165 */
	LOCALE_CHAN_52_64 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{0, QDB(15), QDB(15), 0, QDB(17)},
	{0, 21, 21, 0, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* locale 22. channel 34, 36, 38, 40, 42, 44, 46, 48 */
static const locale_info_t locale_22 = {
	LOCALE_SET_5G_LOW_JP1,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_JAPAN_LEGACY,
	{QDB(21), 0, 0, 0, 0},
	{23, 0, 0, 0, 0},
	WLC_EIRP | WLC_DFS_FCC
};

static const locale_info_t locale_23 = {
	/* locale 23. channel 34, 36, 38, 40, 42, 44, 46, 48, 52-64 */
	LOCALE_SET_5G_LOW_JP1 | LOCALE_CHAN_52_64,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_JAPAN_LEGACY,
	{QDB(21), QDB(21), QDB(21), 0, 0},
	{23, 23, 23, 0, 0},
	WLC_EIRP | WLC_DFS_FCC
};

static const locale_info_t locale_24 = {
	/* locale 24. channel 34, 36, 38, 40, 42, 44, 46, 48, 52-64, 100-140 */
	LOCALE_SET_5G_LOW_JP1 | LOCALE_CHAN_52_64 | LOCALE_CHAN_100_140,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_JAPAN_LEGACY,
	{QDB(21), QDB(21), QDB(21), QDB(21), 0},
	{23, 23, 23, 30, 0},
	WLC_EIRP | WLC_DFS_FCC
};

static const locale_info_t locale_25 = {
	/* locale 25. channel 34 - 46, 36 - 48, 52 - 64, 100 - 124, 149 - 161 */
	LOCALE_SET_5G_LOW_JP1 | LOCALE_CHAN_52_64 | LOCALE_CHAN_100_124 | LOCALE_SET_5G_HIGH2,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(12), QDB(16), QDB(16), QDB(16), QDB(16)},
	{17, 23, 23, 23, 23},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_25h = {
	/* locale 25h. channel 36 - 48, 52 - 64, 100 - 124, 149 - 161 */
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_124 | LOCALE_SET_5G_HIGH2,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(15), QDB(16), QDB(16), QDB(16), QDB(16)},
	{17, 23, 23, 23, 23},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_25l = {
	/* locale 25. channel 34 - 46, 36 - 48, 52 - 64, 100 - 124, 149 - 161 */
	LOCALE_SET_5G_LOW_JP1 | LOCALE_CHAN_52_64 | LOCALE_CHAN_100_124 | LOCALE_SET_5G_HIGH2,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(12), QDB(15), QDB(15), QDB(15), QDB(15)},
	{17, 23, 23, 23, 23},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_27 = {	/* locale 27. channel 36 - 48, 149 - 165 */
	LOCALE_SET_5G_LOW1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), 0, 0, 0, QDB(17)},
	{17, 0, 0, 0, 30},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_27b = {	/* locale 27b. channel 36 - 48, 149 - 165 */
	LOCALE_SET_5G_LOW1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), 0, 0, 0, QDB(23)},
	{17, 0, 0, 0, 30},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_27c = {       /* locale 27c. channel 36 - 48, 149 - 161 */
	LOCALE_SET_5G_LOW1 | LOCALE_SET_5G_HIGH2,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), 0, 0, 0, QDB(23)},
	{17, 0, 0, 0, 30},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_27e = {       /* locale 27e. channel 36 - 48, 149 - 161 */
	LOCALE_SET_5G_LOW1 | LOCALE_SET_5G_HIGH2,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(15), 0, 0, 0, /* 21.5 dBm */ 86},
	{17, 0, 0, 0, 30},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_28 = {	/* locale 28. channel 36 - 48 */
	LOCALE_SET_5G_LOW1,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), 0, 0, 0, 0},
	{17, 0, 0, 0, 0},
	WLC_PEAK_CONDUCTED
};

static const locale_info_t locale_29 = {
	/* locale 29. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), QDB(16), QDB(16), QDB(16), QDB(16)},
	{23, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_29a = {
	/* locale 29a. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), QDB(16), QDB(16), QDB(16), QDB(16)},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* CLM v2.1 has separate power targets for 5G ch 104-116 and 100/132-140.
 * Express ch 104-116 limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_info_t locale_29b = {
	/* locale 29b. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), QDB(15), QDB(15), /* 18.5 dBm */ 74, QDB(20)},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_29b_1 = {
	/* locale 29b. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), /* 20.5 dBm */ 82, 78, 82, QDB(23)},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

/* CLM v4.8.2 has separate power targets for 5G ch 100 and 104-116, 132-136
 * and 140, express in whole range limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_info_t locale_29b_3 = {
	/* locale 29b_3. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(15), /* 20.5 dBm */ 82, /* 19.5 */ 78, 82, /* 26.5 */ 106 },
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_29c = {
	/* locale 29c. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(15), QDB(15), QDB(15), QDB(15), QDB(15)},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_29d = {
	/* locale 29d. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(14), /* 18.5 */ 74, 74, 74, /* 19.5 */ 78},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_29d_1 = {
	/* locale 29d_1. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{ /* 15.5 dBm */ 62, /* 22.5 dBm */ 90, 90, 90, 90},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_29d_2 = {
	/* locale 29d_2. channel 36 - 48, 52 - 64, 100 - 116, 132 - 140, 149 - 165 */
	LOCALE_CHAN_36_64 | LOCALE_SET_5G_MID1 | LOCALE_SET_5G_HIGH1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(15), QDB(17), QDB(17), QDB(17), QDB(17)},
	{17, 24, 24, 24, 30},
	WLC_PEAK_CONDUCTED | WLC_DFS_FCC
};

static const locale_info_t locale_31 = {	/* locale 31. channel 36 - 48, 149 - 165 */
	LOCALE_SET_5G_LOW1 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_LOW_HI,
	{QDB(14), 0, 0, 0, QDB(15)},
	{17, 0, 0, 0, 20},
	WLC_PEAK_CONDUCTED
};

#ifdef BCMDBG
static const locale_info_t locale_5G_radar_only = {	/* Channels 52 - 140 with US power */
	LOCALE_CHAN_52_140_ALL,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(12), QDB(17), QDB(17), 0, QDB(17)},
	{20, 20, 20, 20, 20},
	WLC_PEAK_CONDUCTED | WLC_DFS_TPC
};
#endif /* BCMDBG */

/* All the channels we can do with a 2060ww */
static const locale_info_t locale_5G_band_all = {
	LOCALE_SET_5G_LOW_JP1 | LOCALE_CHAN_52_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165 |
	LOCALE_CHAN_184_216,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_NONE,
	/* Don't throttle power due to regulatory */
	{QDB(30), QDB(30), QDB(30), QDB(30), QDB(30)},
	{30, 30, 30, 30, 30},
	WLC_PEAK_CONDUCTED
};
#endif /* BAND5G */

#ifdef WL11D

#ifdef BAND2G
/* 802.11d locale with all regulatory supported 2.4 GHz channels valid but marked as restricted
 * Channels: 1-14
 */
static const locale_info_t locale_11d_2G = {
	LOCALE_CHAN_01_14,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_11D_2G,
	{QDB(20), QDB(20), QDB(20),
	QDB(20), QDB(20), QDB(20)},
	{0, 0, 0, 0, 0},	/* public pwr limits unused for non-AP */
	WLC_PEAK_CONDUCTED
};
#endif /* BAND2G */
#ifdef BAND5G
/* 802.11d locale with all regulatory supported 5 GHz channels valid but marked as restricted,
 * and the typical radar set, 52 - 64, 100 - 140
 * Channels:
 * 34, 38, 42, 46 (off-by-2 old Japan channels)
 * 36 - 48, 52 - 64, 100 - 140, 149 - 165
 */
static const locale_info_t locale_11d_5G = {
	LOCALE_SET_5G_LOW_JP1 | LOCALE_CHAN_52_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_11D_5G,
	{QDB(17), QDB(20), QDB(20),  QDB(30),  QDB(20)},
	{0, 0, 0, 0, 0},	/* public pwr limits unused for non-AP */
	WLC_PEAK_CONDUCTED | WLC_DFS_TPC
};
#endif /* BAND5G */

#endif /* WL11D */


#define LOCALE_2G_IDX_a			0
#define LOCALE_2G_IDX_a1		1
#define LOCALE_2G_IDX_a2		2
#define LOCALE_2G_IDX_a3		3
#define LOCALE_2G_IDX_b			4
#define LOCALE_2G_IDX_b2		5
#define LOCALE_2G_IDX_b3		6
#define LOCALE_2G_IDX_c			7
#define LOCALE_2G_IDX_f			8
#define LOCALE_2G_IDX_g			9
#define LOCALE_2G_IDX_h			10
#define LOCALE_2G_IDX_i			11
#define LOCALE_2G_IDX_j			12
#define LOCALE_2G_IDX_k			13
#define LOCALE_2G_IDX_c1		14
#define LOCALE_2G_IDX_2G_band_all	15
#define LOCALE_2G_IDX_a4		16
#define LOCALE_2G_IDX_a5		17
#define LOCALE_2G_IDX_b4		18
#define LOCALE_2G_IDX_b5		19
#define LOCALE_2G_IDX_a6		20
#define LOCALE_2G_IDX_11d_2G		21
#define LOCALE_2G_IDX_a7		22
#define LOCALE_2G_IDX_a_2		23
#define LOCALE_2G_IDX_a9		24
#define LOCALE_2G_IDX_b2_1		25
#define LOCALE_2G_IDX_b5_2		26
#define LOCALE_2G_IDX_a3_1		27
#define LOCALE_2G_IDX_b3_1		28
#define LOCALE_2G_IDX_c_1 		29
#define LOCALE_2G_IDX_i_1 		30
#define LOCALE_2G_IDX_a_1		31
#define LOCALE_2G_IDX_a6_1		32
#define LOCALE_2G_IDX_a10		33
#define LOCALE_2G_IDX_b2_2		34
#define LOCALE_2G_IDX_b_1		35
#define LOCALE_2G_IDX_b2_3		36
#define LOCALE_2G_IDX_b2_4		37
#define LOCALE_2G_IDX_a6_2		38
#define LOCALE_2G_IDX_b5_3		39
#define LOCALE_2G_IDX_b_2		40
#define LOCALE_2G_IDX_a10_1		41
#define LOCALE_2G_IDX_b2_5		42
#define LOCALE_2G_IDX_a10_2		43
#define LOCALE_2G_IDX_a10_3		44
#define LOCALE_2G_IDX_b_3		45
#define LOCALE_2G_IDX_b_4		46
#define LOCALE_2G_IDX_a11		47
#define LOCALE_2G_IDX_c_2 		48
#define LOCALE_2G_IDX_a5_1		49
#define LOCALE_2G_IDX_b5_4		50
#define LOCALE_2G_IDX_a_3		51
#define LOCALE_2G_IDX_b_5		52
#define LOCALE_2G_IDX_b_6		53
#define LOCALE_2G_IDX_b_7		54
#define LOCALE_2G_IDX_b_8		55
#define LOCALE_2G_IDX_k_1		56
#define LOCALE_2G_IDX_a12		57
#define LOCALE_2G_IDX_a6_3		58
#define LOCALE_2G_IDX_b_9		59
#define LOCALE_2G_IDX_b2_6		60
#define LOCALE_2G_IDX_k_3		61
#define LOCALE_2G_IDX_b3_2		62
#define LOCALE_2G_IDX_c_4		63
#define LOCALE_2G_IDX_a14		64
#define LOCALE_2G_IDX_b_11		65
#define LOCALE_2G_IDX_a_5		66
#define LOCALE_2G_IDX_a_6		67
#define LOCALE_2G_IDX_a4_3		68
#define LOCALE_2G_IDX_c_5 		69
#define LOCALE_2G_IDX_k_4		70
#define LOCALE_2G_IDX_a4_2		71
#define LOCALE_2G_IDX_a6_8		72
#define LOCALE_2G_IDX_a4_4		73
#define LOCALE_2G_IDX_a20		74


static const locale_info_t * g_locale_2g_table[]=
{
	&locale_a,
	&locale_a1,
	&locale_a2,
	&locale_a3,
	&locale_b,
	&locale_b2,
	&locale_b3,
	&locale_c,
	&locale_f,
	&locale_g,
	&locale_h,
	&locale_i,
	&locale_j,
	&locale_k,
	&locale_c1,
	&locale_2G_band_all,
	&locale_a4,
	&locale_a5,
	&locale_b4,
	&locale_b5,
	&locale_a6,
#if defined(BAND2G) && defined(WL11D)
	&locale_11d_2G,
#else
	NULL,
#endif /* WL11D  && BAND2G */
	&locale_a7,
	&locale_a_2,
	&locale_a9,
	&locale_b2_1,
	&locale_b5_2,
	&locale_a3_1,
	&locale_b3_1,
	&locale_c_1,
	&locale_i_1,
	&locale_a_1,
	&locale_a6_1,
	&locale_a10,
	&locale_b2_2,
	&locale_b_1,
	&locale_b2_3,
	&locale_b2_4,
	&locale_a6_2,
	&locale_b5_3,
	&locale_b_2,
	&locale_a10_1,
	&locale_b2_5,
	&locale_a10_2,
	&locale_a10_3,
	&locale_b_3,
	&locale_b_4,
	&locale_a11,
	&locale_c_2,
	&locale_a5_1,
	&locale_b5_4,
	&locale_a_3,
	&locale_b_5,
	&locale_b_6,
	&locale_b_7,
	&locale_b_8,
	&locale_k_1,
	&locale_a12,
	&locale_a6_3,
	&locale_b_9,
	&locale_b2_6,
	&locale_k_3,
	&locale_b3_2,
	&locale_c_4,
	&locale_a14,
	&locale_b_11,
	&locale_a_5,
	&locale_a_6,
	&locale_a4_3,
	&locale_c_5,
	&locale_k_4,
	&locale_a4_2,
	&locale_a6_8,
	&locale_a4_4,
	&locale_a20

};

#define LOCALE_5G_IDX_1		0
#define LOCALE_5G_IDX_1a	1
#define LOCALE_5G_IDX_1r	2
#define LOCALE_5G_IDX_2		3
#define LOCALE_5G_IDX_3		4
#define LOCALE_5G_IDX_3a	4
#define LOCALE_5G_IDX_3c	4
#define LOCALE_5G_IDX_3r	5
#define LOCALE_5G_IDX_4		6
#define LOCALE_5G_IDX_5		7
#define LOCALE_5G_IDX_5a	8
#define LOCALE_5G_IDX_6		9
#define LOCALE_5G_IDX_7		10
#define LOCALE_5G_IDX_8		11
#define LOCALE_5G_IDX_9		12
#define LOCALE_5G_IDX_10	13
#define LOCALE_5G_IDX_11	14
#define LOCALE_5G_IDX_12	15
#define LOCALE_5G_IDX_13	16
#define LOCALE_5G_IDX_14	17
#define LOCALE_5G_IDX_15	18
#define LOCALE_5G_IDX_16	19
#define LOCALE_5G_IDX_17	20
#define LOCALE_5G_IDX_18	21
#define LOCALE_5G_IDX_19	22
#define LOCALE_5G_IDX_19a	23
#define LOCALE_5G_IDX_19r	24
#define LOCALE_5G_IDX_19b	25
#define LOCALE_5G_IDX_20	26
#define LOCALE_5G_IDX_21	27
#define LOCALE_5G_IDX_22	28
#define LOCALE_5G_IDX_23	29
#define LOCALE_5G_IDX_24	30
#define LOCALE_5G_IDX_25	31
#define LOCALE_5G_IDX_27	32
#define LOCALE_5G_IDX_28	33
#define LOCALE_5G_IDX_29	34
#define LOCALE_5G_IDX_29a	35
#define LOCALE_5G_IDX_29c	36
#define LOCALE_5G_IDX_29b_1	37
#define LOCALE_5G_IDX_31	38
#define LOCALE_5G_IDX_5G_band_all	39
#define LOCALE_5G_IDX_11d_5G	40
#define LOCALE_5G_IDX_7c	41
#define LOCALE_5G_IDX_19h_2	42
#define LOCALE_5G_IDX_27b	43
#define LOCALE_5G_IDX_29b	44
#define LOCALE_5G_IDX_5G_radar_only	45
#define LOCALE_5G_IDX_25l	46
#define LOCALE_5G_IDX_27c	47
#define LOCALE_5G_IDX_19_1	48
#define LOCALE_5G_IDX_19_2      49
#define LOCALE_5G_IDX_19l_1	50
#define LOCALE_5G_IDX_19l_2     51
#define LOCALE_5G_IDX_3l	52
#define LOCALE_5G_IDX_5l	53
#define LOCALE_5G_IDX_8a	54
#define LOCALE_5G_IDX_11_1	55
#define LOCALE_5G_IDX_11_2	56
#define LOCALE_5G_IDX_11l	57
#define LOCALE_5G_IDX_3j	58
#define LOCALE_5G_IDX_3j_1	59
#define LOCALE_5G_IDX_19a_1	60
#define LOCALE_5G_IDX_19_3      61
#define LOCALE_5G_IDX_19h_1	62
#define LOCALE_5G_IDX_29d	63
#define LOCALE_5G_IDX_3b	64
#define LOCALE_5G_IDX_1c	65
#define LOCALE_5G_IDX_3j_4	66
#define LOCALE_5G_IDX_3l_1	67
#define LOCALE_5G_IDX_5l_1	68
#define LOCALE_5G_IDX_6a	69
#define LOCALE_5G_IDX_8b	70
#define LOCALE_5G_IDX_11l_1	71
#define LOCALE_5G_IDX_13_1	72
#define LOCALE_5G_IDX_14a	73
#define LOCALE_5G_IDX_16a	74
#define LOCALE_5G_IDX_17a	75
#define LOCALE_5G_IDX_18l	76
#define LOCALE_5G_IDX_9h	77
#define LOCALE_5G_IDX_11_3	78
#define LOCALE_5G_IDX_11_4	79
#define LOCALE_5G_IDX_3d    	80
#define LOCALE_5G_IDX_3j_3  	81
#define LOCALE_5G_IDX_19_4  	82
#define LOCALE_5G_IDX_3j_5	83
#define LOCALE_5G_IDX_3r_4	84
#define LOCALE_5G_IDX_5l_2	85
#define LOCALE_5G_IDX_6a_1	86
#define LOCALE_5G_IDX_7c_1	87
#define LOCALE_5G_IDX_8a_1	88
#define LOCALE_5G_IDX_13_2	89
#define LOCALE_5G_IDX_25h	90
#define LOCALE_5G_IDX_29d_2	91
#define LOCALE_5G_IDX_3j_8	92
#define LOCALE_5G_IDX_18_2	93
#define LOCALE_5G_IDX_27e	94
#define LOCALE_5G_IDX_3s	95
#define LOCALE_5G_IDX_3r_5  96
#define LOCALE_5G_IDX_13_3  97
#define LOCALE_5G_IDX_29d_1	98
#define LOCALE_5G_IDX_29b_3	99
#define LOCALE_5G_IDX_3_1	100 /* CLM v4.6.6 */

#ifdef BAND5G
static const locale_info_t * g_locale_5g_table[]=
{
	&locale_1,
	&locale_1a,
	&locale_1r,
	&locale_2,
	&locale_3,
	&locale_3r,
	&locale_4,
	&locale_5,
	&locale_5a,
	&locale_6,
	&locale_7,
	&locale_8,
	&locale_9,
	&locale_10,
	&locale_11,
	&locale_12,
	&locale_13,
	&locale_14,
	&locale_15,
	&locale_16,
	&locale_17,
	&locale_18,
	&locale_19,
	&locale_19a,
	&locale_19r,
	&locale_19b,
	&locale_20,
	&locale_21,
	&locale_22,
	&locale_23,
	&locale_24,
	&locale_25,
	&locale_27,
	&locale_28,
	&locale_29,
	&locale_29a,
	&locale_29c,
	&locale_29b_1,
	&locale_31,
	&locale_5G_band_all,
#ifdef WL11D
	&locale_11d_5G,
#else
	NULL,
#endif
	&locale_7c,
	&locale_19h_2,
	&locale_27b,
	&locale_29b,
#ifdef BCMDBG
	&locale_5G_radar_only,
#else
	NULL,
#endif
	&locale_25l,
	&locale_27c,
	&locale_19_1,
	&locale_19_2,
	&locale_19l_1,
	&locale_19l_2,
	&locale_3l,
	&locale_5l,
	&locale_8a,
	&locale_11_1,
	&locale_11_2,
	&locale_11l,
	&locale_3j,
	&locale_3j_1,
	&locale_19a_1,
	&locale_19_3,
	&locale_19h_1,
	&locale_29d,
	&locale_3b,
	&locale_1c,
	&locale_3j_4,
	&locale_3l_1,
	&locale_5l_1,
	&locale_6a,
	&locale_8b,
	&locale_11l_1,
	&locale_13_1,
	&locale_14a,
	&locale_16a,
	&locale_17a,
	&locale_18l,
	&locale_9h,
	&locale_11_3,
	&locale_11_4,
	&locale_3d,
	&locale_3j_3,
	&locale_19_4,
	&locale_3j_5,
	&locale_3r_4,
	&locale_5l_2,
	&locale_6a_1,
	&locale_7c_1,
	&locale_8a_1,
	&locale_13_2,
	&locale_25h,
	&locale_29d_2,
	&locale_3j_8,
	&locale_18_2,
	&locale_27e,
	&locale_3s,
	&locale_3r_5,
	&locale_13_3,
	&locale_29d_1,
	&locale_29b_3,
	&locale_3_1 /* CLM v4.6.6 */

};
#endif /* #ifdef BAND5G */


/* unrestricted power locale for internal country "ALL" */
static const locale_mimo_info_t locale_mimo_all = {
	{QDB(30), QDB(30), QDB(30), QDB(30), QDB(30),
	QDB(30), QDB(30), QDB(30), QDB(30), QDB(30),
	QDB(30), QDB(30), QDB(30), QDB(30)},
	{QDB(30), QDB(30), QDB(30), QDB(30), QDB(30),
	QDB(30), QDB(30), QDB(30), QDB(30), QDB(30),
	QDB(30), QDB(30), QDB(30), QDB(30)},
	0
};

/*
 * MIMO Locale Definitions - 2.4 GHz
 */

#ifdef WL11N
static const locale_mimo_info_t locale_an1 = {
	{QDB(14), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(14), 0, 0},
	{0, 0, QDB(13), QDB(14), QDB(14),
	QDB(14), QDB(14), QDB(12), QDB(12), 0,
	0, 0, 0},
	0
};

/* CLM v3.4 has only SISO power targets.  */
static const locale_mimo_info_t locale_an_2 = {
	{QDB(18), QDB(18), QDB(18), QDB(18), QDB(18),
	QDB(18), QDB(18), QDB(18), QDB(18), QDB(18), QDB(18), 0, 0},
	{0, 0, QDB(15), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16), QDB(16), 0, 0, 0, 0},
	0
};

/* CLM v1.11.5 has separate SISO/CDD power targets and per-MCS limits.
 * Express SISO limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_an1_t1 = {
	{QDB(19), QDB(19), QDB(19), QDB(19), QDB(19),
	QDB(19), QDB(19), QDB(19), QDB(19), QDB(19), /* 16.5 dBm */ 66, 0, 0},
	{0, 0, /* 15.5 dBm */ 62, /* 16.5 dBm */ 66, 66,
	66, 66, /* 15.5 dBm */ 62, /* 14.5 dBm */ 58, 0, 0, 0, 0},
	0
};

/* CLM v1.11.5 has separate SISO/CDD power targets and per-MCS limits.
 * Express SISO limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_an1_t2 = {
	{QDB(19), QDB(19), QDB(19), QDB(19), QDB(19),
	QDB(19), QDB(19), QDB(19), QDB(19), QDB(19), /* 16.5 dBm */ 66, 0, 0},
	{0, 0, /* 15.5 dBm */ 62, /* 16.5 dBm */ 66, 66,
	66, 66, /* 15.5 dBm */ 62, /* 14.5 dBm */ 58, 0, 0, 0, 0},
	0
};

/* CLM v3.4 has only SISO power targets. */
static const locale_mimo_info_t locale_an1_t3 = {
	{QDB(17), QDB(19), QDB(19), QDB(19), QDB(19),
	QDB(19), QDB(19), QDB(19), QDB(19), QDB(19), QDB(16), 0, 0},
	{0, 0, QDB(15), QDB(17), QDB(17),
	QDB(17), QDB(17), QDB(17), QDB(14), 0, 0, 0, 0},
	0
};

/* CLM v3.0 has 40MHz per-MCS limits.
 * Express most limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_an1_t4 = {
	{QDB(12), QDB(14), QDB(14), QDB(14), QDB(14),
	QDB(14), QDB(14), QDB(14), QDB(14), QDB(14), QDB(12), 0, 0},
	{0, 0, QDB(10), QDB(12), QDB(12),
	QDB(12), QDB(12), QDB(12), QDB(10), 0, 0, 0, 0},
	0
};

/* CLM v3.1 has separate SISO/CDD power targets and per-MCS limits.
 * Express SISO limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_an1_t5 = {
	{QDB(14), QDB(16), QDB(17), QDB(17), QDB(17),
	QDB(17), QDB(17), QDB(17), QDB(17), QDB(15), QDB(12), 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_bn1_1 = {
	{QDB(11), QDB(11), QDB(11), QDB(11), QDB(11),
	QDB(11), QDB(11), QDB(11), QDB(11), QDB(11), QDB(11), QDB(11), QDB(11)},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

/* CLM v3.2 has Only SISO power targets and per-MCS limits.  */
static const locale_mimo_info_t locale_an6_1 = {
	{QDB(14), QDB(20), QDB(21), QDB(21), QDB(21),
	QDB(21), QDB(21), QDB(21), QDB(21), QDB(20), QDB(14), 0, 0},
	{0, 0, QDB(14), QDB(20), QDB(21),
	QDB(21), QDB(21), QDB(20), /* 14.5 dbm */ 58, 0, 0, 0, 0},
	0
};

/* CLM v4.3.4 has only CDD power target. */
static const locale_mimo_info_t locale_an6_5 = {
	{QDB(16), QDB(19), QDB(22), QDB(22), QDB(22),
	QDB(22), QDB(22), QDB(22), QDB(22), QDB(19), QDB(16), 0, 0},
	{0, 0, /* 13.5 dbm */ 54, QDB(16), QDB(18),
	QDB(18), QDB(18), QDB(16), 54, 0, 0, 0, 0},
	0
};

/* CLM v4.3.4 has separate SISO/CDD power targets .
 * Express CDD limits here and fixup SISO in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_an6_6 = {
	{QDB(15), QDB(17), QDB(17), QDB(17), QDB(17),
	QDB(17), QDB(17), QDB(17), QDB(17), QDB(17), QDB(14), 0, 0},
	{0, 0, QDB(12), QDB(17), QDB(17),
	QDB(17), QDB(17), QDB(17), QDB(12), 0, 0, 0, 0},
	0
};

/* CLM v4.8.2 has separate SISO/CDD power targets .
 * Express CDD limits here and fixup SISO in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_an6_7 = {
	{ /* 17.5 */ 70, /* 20.5 */ 82, QDB(23), QDB(23), QDB(23),
	QDB(23), QDB(23), QDB(23), QDB(23), /* 20.5 */ 82, /* 17.5 */ 70, 0, 0},
	{0, 0, /* 16.5 */ 66, /* 19.5 */ 78, /* 21.5 */ 86,
	86, 86, /* 19.5 */ 78, /* 16.5 */ 66, 0, 0, 0, 0},
	0
};

/* CLM v3.3 has Only SISO power targets and per-MCS limits.  */
static const locale_mimo_info_t locale_bn2_4 = {
	{QDB(16), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16), QDB(16), QDB(16), QDB(16), QDB(16), /* 11.5 dbm */ 46},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

/* CLM v3.4 has Only SISO power targets and per-MCS limits.  */
static const locale_mimo_info_t locale_bn2_5 = {
	{ /* 13.5 dbm */ 54, 54, 54, 54, 54,
	54, 54, 54, 54, 54, 54, 54, 54},
	{0, 0, QDB(14), QDB(14), QDB(14), QDB(14), QDB(14), QDB(14), QDB(14),
	QDB(14), QDB(14), 0, 0},
	0
};

static const locale_mimo_info_t locale_an2 = {
#if 1	/* ASUS modify */
	{QDB(12.5), /* 16.5 dBm = 66 qdBm */ 70, 70, 70, 70,
	 70, 70, 70, 70, 70,
	 /* 14.5 dBm */ 50, 0, 0},
	{0, 0, QDB(11.5), /* 14.5 dBm */ 66, 66,
	 66, 66, /* 12.5 dBm */ 66, 46, 0,
#else
	{QDB(15), /* 16.5 dBm = 66 qdBm */ 66, 66, 66, 66,
	 66, 66, 66, 66, 66,
	 /* 14.5 dBm */ 58, 0, 0},
	{0, 0, QDB(14), /* 14.5 dBm */ 58, 58,
	 58, 58, /* 12.5 dBm */ 50, 50, 0,
#endif
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an2_1 = {
	{QDB(12), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	/* 11.5 */ 46, 0, 0},
	{0, 0, /* 10.5 */ 42, /* 12.5 */ 50, QDB(15),
	QDB(15), QDB(15), /* 13.5 */ 54, 46, 46,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an2_2 = {
	{ /* 13.5 */ 54, QDB(14), QDB(14), QDB(14), QDB(14),
	QDB(14), QDB(14), QDB(14), QDB(14), QDB(14),
	QDB(14), 0, 0},
	{0, 0, QDB(10), QDB(10), QDB(10),
	QDB(10), QDB(10), QDB(10), /* 9.5 */ 38, 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an2_3 = {
	{QDB(15), /* 15.5 */ 62, 62, 62, 62,
	62, 62, 62, 62, 62,
	62, 0, 0},
	{0, 0, /* 11.5 */ 46, 46, 46,
	46, 46, 46, QDB(11), 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an2_4 = {
	{QDB(13), /* 15.5 */ 62, 62, 62, 62,
	62, 62, 62, 62, 62,
	QDB(13), 0, 0},
	{0, 0, QDB(12), QDB(14), QDB(14),
	QDB(14), QDB(14), QDB(14), QDB(12), 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an2_5 = {
	{QDB(11), /* 16.5 */ 66, 66, 66, 66,
	66, 66, 66, 66, 66,
	QDB(14), 0, 0},
	{0, 0, /* 9.5 */ 38, /* 15.5 */ 62, 62,
	62, 62, 62, /* 12.5 */ 50, 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an3 = {
	{ /* 15.5 dBm */ 62, QDB(17), QDB(17), QDB(17), QDB(17),
	QDB(17), QDB(17), QDB(17), QDB(17), QDB(17),
	QDB(15), 0, 0},
	{0, 0, /* 14.5 dBm */ 58, 58, 58,
	58, 58, QDB(13), QDB(13), 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an4 = {
	{ /* 16.5 dBm */ 66, /* 18.5 dBm */ 74, 74, 74, 74,
	74, 74, 74, 74, 74,
	QDB(16), 0, 0},
	{0, 0, QDB(15), /* 15.5 dBm */ 62, 62,
	62, 62, QDB(15), QDB(15), 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an5 = {
	{ /* 16.5 dBm */ 66, QDB(20), QDB(22), QDB(22), QDB(22),
	QDB(22), QDB(22), QDB(22), QDB(22), QDB(18),
	/* 14.5 dBm */ 58, 0, 0},
	{0, 0, QDB(14), QDB(17), QDB(18),
	QDB(18), QDB(18), QDB(16), QDB(13), 0,
	0, 0, 0},
	0
};

/* CLM v2.1.1 has separate per-MCS limits at 40Mhz. Fixup in wlc_channel_reg_limits(). */
static const locale_mimo_info_t locale_an6 = {
	{ /* 16.5 dBm */ 66, /* 18.5 dBm */ 74, QDB(20), QDB(20), QDB(20),
	QDB(20), QDB(20), QDB(20), QDB(20), /* 18.5 dBm */ 74,
	/* 16.5 dBm */ 66, 0, 0},
	{0, 0, /* 17.5 dBm */ 70, /* 18.5 dBm */ 74, QDB(20),
	QDB(20), QDB(20), /* 18.5 dBm */ 74, /* 16.5 dBm */ 66, 0,
	0, 0, 0},
	0
};

/* CLM v2.3 has separate per-MCS limits at 40Mhz. Fixup in wlc_channel_reg_limits(). */
static const locale_mimo_info_t locale_an6_2 = {
	{ QDB(13), /* 15.5 dBm */ 62, QDB(16), QDB(17), QDB(17),
	QDB(17), QDB(17), QDB(17), QDB(17), /* 14.5 dBm */ 58,
	/* 12.5 dBm */ 50, 0, 0},
	{0, 0, QDB(12), /* 12.5 dBm */ 50, /* 13.5 dBm */ 54,
	54, 54, QDB(13), /* 12.5 dBm */ 50, 0,
	0, 0, 0},
	0
};

/* CLM v4.5.6 has only CDD/SDM limts for 20MHz and 40MHz. */
static const locale_mimo_info_t locale_an6_3 = {
	{ QDB(11), /* 15.5 dBm */ 62, QDB(16), QDB(17), QDB(17),
	QDB(17), QDB(17), QDB(17), QDB(17), /* 14.5 dBm */ 58,
	/* 13.5 dBm */ 54, 0, 0},
	{0, 0, QDB(10), /* 12.5 dBm */ 50, QDB(15),
	QDB(15), QDB(15), 58, /* 12.5 dBm */ 50, 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an6_4 = {
	{ /* 16.5 dBm */ 66, QDB(18), QDB(21), QDB(21), QDB(21),
	QDB(21), QDB(21), QDB(21), QDB(21), /* 16.5 dBm */ 66,
	/* 14.5 dBm */ 58, 0, 0},
	{0, 0, QDB(14), QDB(15), QDB(17),
	QDB(17), QDB(17), QDB(15), QDB(13), 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an7 = {
	{ QDB(16), QDB(20), QDB(21), QDB(21), QDB(21),
	QDB(21), QDB(21), QDB(21), QDB(21), QDB(20),
	QDB(16), 0, 0},
	{0, 0, QDB(14), QDB(17), QDB(19),
	QDB(19), QDB(19), QDB(17), QDB(13), 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an7_2 = {
	{ QDB(13), QDB(19), QDB(20), QDB(20), QDB(20),
	QDB(20), QDB(20), QDB(20), QDB(20), QDB(19),
	QDB(13), 0, 0},
	{0, 0, QDB(13), QDB(15), QDB(16),
	QDB(16), QDB(16), QDB(15), QDB(13), 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an7_3 = {
	{ /* 15.5 */ 62, /* 17.5 */ 70, /* 18.5 */ 74, /* 21.5 */ 86, 86,
	86, 86, 86, 86, 74,
	62, 0, 0},
	{0, 0, /* 14.5 */ 58, 58, 58,
	70, 58, 58, 58, 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an7_8 = {
	{ QDB(15), /* 21.5 dBm */ 86, 86, 86, 86,
	86, 86, 86, 86, 86,
	QDB(18), 0, 0},
	{0, 0, QDB(14), QDB(18), QDB(18),
	QDB(18), QDB(18), QDB(18), /* 16.5dBm */ 66, 0, 0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an8_t1 = {
#if 1	/* ASUS modify */
	{QDB(30), QDB(30), QDB(30), QDB(30), QDB(30),
	QDB(30), QDB(30), QDB(30), QDB(30), QDB(30),
	QDB(30), 0, 0},
	{0, 0, QDB(30), QDB(30), QDB(30),
	QDB(30), QDB(30), QDB(30), /* 10.5 dBm */ 120, 0,
#else
	{QDB(14), QDB(16), QDB(19), QDB(19), QDB(19),
	QDB(19), QDB(19), QDB(19), QDB(19), QDB(14),
	QDB(11), 0, 0},
	{0, 0, QDB(13), QDB(14), QDB(14),
	QDB(14), QDB(14), QDB(14), /* 10.5 dBm */ 42, 0,
#endif
	0, 0, 0},
	0
};

/* CLM v2.7 has separate per-MCS limits at 40Mhz. Fixup in wlc_channel_reg_limits(). */
static const locale_mimo_info_t locale_an8_t2 = {
	{QDB(14), QDB(16), QDB(19), QDB(19), QDB(19),
	QDB(19), QDB(19), QDB(19), QDB(19), /* 14.5 dBm */ 58,
	/* 12.5 dBm */ 50, 0, 0},
	{0, 0, QDB(13), QDB(14), QDB(14),
	QDB(14), QDB(14), QDB(14), QDB(14), 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an9_t1 = {
	{QDB(14), QDB(16), QDB(19), QDB(19), QDB(19),
	QDB(19), QDB(19), QDB(19), QDB(19), QDB(14),
	QDB(14), /* 13.5 dBm */ 54, QDB(12)},
	{0, 0, /* 13.5 dBm */ 54, /* 14.5 dBm */ 58, 58,
	58, 58, 58, QDB(12), QDB(13),
	QDB(7), 0, 0},
	0
};

static const locale_mimo_info_t locale_an10 = {
#if 1	/* ASUS modify */
	{QDB(19.5), QDB(25.5), QDB(25.5), QDB(25.5), QDB(25.5),
	QDB(25.5), QDB(25.5), QDB(25.5), QDB(25.5), QDB(25.5),
	QDB(19.5), 0, 0},
	{0, 0, QDB(18.5), QDB(24.5), QDB(24.5),
	QDB(24.5), QDB(24.5), QDB(24.5), QDB(18.5), 0,
#else
	{QDB(10), QDB(11), QDB(13), QDB(14), QDB(15),
	QDB(16), QDB(16), QDB(15), QDB(14), QDB(11),
	QDB(10), 0, 0},
	{0, 0, QDB(14), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(15), QDB(14), 0,
#endif
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_an2_21 = {
	{QDB(13), QDB(18), QDB(18), QDB(18), QDB(18),
	QDB(18), QDB(18), QDB(18), QDB(18), QDB(18),
	QDB(11), 0, 0},
	{0, 0, QDB(11), QDB(14), QDB(14),
	QDB(14), QDB(14), QDB(14), QDB(12), 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_a_3n = {
	{QDB(11), QDB(15), /* 16.5 dBm */ 66, 66, 66,
	66, 66, 66, 66, QDB(15),
	/* 10.5 dBm */ 42, 0, 0},
	{0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_a1_3n = {
	{QDB(17), QDB(20), /* 22.5 dBm */ 90, 90, 90,
	90, 90, 90, 90, QDB(20),
	QDB(17), 0, 0},
	{0, 0, QDB(16), QDB(19), /* 22.5 dBm */ 90, 
	90, 90, QDB(19), QDB(16), 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_b2_3n = {
	{57 /* 14.25 dBm */, 57, 57, 57, 57,
	57, 57, 57, 57, 57,
	57, 57, 57},
	{0, 0, 57, 57, 57,
	57, 57, 57, 57, 57,
	57, 0, 0},
	0
};

/* CLM v3.7.2 has separate power targets for SISO/CDD.
 * Dfine CDD/SDM here, and fixup SISO in wlc_channel_reg_limits().
*/
static const locale_mimo_info_t locale_bn = {
	{QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13)},
	{0, 0, QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), 0, 0},
	0
};

static const locale_mimo_info_t locale_bn_1 = {
#if 1	/* ASUS modify */
	{QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15)},
	{0, 0, QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), 0, 0},
#else
	{QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15)},
	{0, 0, QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), 0, 0},
#endif
	0
};

static const locale_mimo_info_t locale_bn_2 = {
#if 1	/* ASUS modify */
	{QDB(13.5), QDB(13.5), QDB(13.5), QDB(13.5), QDB(13.5),
	QDB(13.5), QDB(13.5), QDB(13.5), QDB(13.5), QDB(13.5),
	QDB(13.5), QDB(13.5), QDB(13.5)},
	{0, 0, QDB(13.5), QDB(13.5), QDB(13.5),
	QDB(13.5), QDB(13.5), QDB(13.5), QDB(13.5), QDB(13.5),
	QDB(13.5), 0, 0},
#else
	{QDB(16), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16)},
	{0, 0, QDB(16), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(16), 0, 0},
#endif
	0
};

static const locale_mimo_info_t locale_bn_3 = {
	{QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15)},
	{0, 0, QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), 0, 0},
	0
};

static const locale_mimo_info_t locale_bn_4 = {
	{QDB(14), QDB(14), QDB(14), QDB(14), QDB(14),
	QDB(14), QDB(14), QDB(14), QDB(14), QDB(14),
	QDB(14), QDB(14), QDB(14)},
	{0, 0, QDB(14), QDB(14), QDB(14),
	QDB(14), QDB(14), QDB(14), QDB(14), QDB(14),
	QDB(14), 0, 0},
	0
};

static const locale_mimo_info_t locale_bn_5 = {
	{QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13)},
	{0, 0, QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), 0, 0},
	0
};

static const locale_mimo_info_t locale_bn_10 = {
	{ /* 11.5 dBm */ 46, 46, 46, 46, 46,
	46, 46, 46, 46, 46,
	46, 46, 46},
	{0, 0, /* 13.5 */ 54, 54, 54,
	54, 54, 54, 54, 54,
	54, 0, 0},
	0
};

static const locale_mimo_info_t locale_bn_10_3n = {
	{ /* 9.75 dBm */ 39, 39, 39, 39, 39,
	39, 39, 39, 39, 39,
	39, 39, 39},
	{0, 0, /* 11.75 dBm */ 47, 47, 47,
	47, 47, 47, 47, 47,
	47, 0, 0},
	0
};

static const locale_mimo_info_t locale_bn_9 = {
	{ /* 13.5 dBm */ 54, 54, 54, 54, 54, 54,
	  /* 13.5 dBm */ 54, 54, 54, 54, 54, 54, 54},
	{0, 0, /* 13.5 dBm */ 54, 54, 54, 
	/* 13.5 dBm */ 54, 54, 54, 54, 54, 54, 
	0, 0},
	0
};


static const locale_mimo_info_t locale_bn1 = {
	{QDB(10), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13)},
	{0, 0, QDB(10), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), 0, 0},
	0
};

static const locale_mimo_info_t locale_bn2 = {
	{ /* 8.5 dBm */ 34, QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), /* 8.5 dBm */ 34},
	{0, 0, /* 8.5 dBm */ 34, QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), /* 8.5 dBm */ 34, 34,
	34, 0, 0},
	0
};

static const locale_mimo_info_t locale_bn2_1 = {
	{ QDB(9), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(9)},
	{0, 0, QDB(10), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), /* 11.5 dBm */46, 46,
	46, 0, 0},
	0
};

static const locale_mimo_info_t locale_bn2_2 = {
	{ QDB(12), QDB(12), QDB(12), QDB(12), QDB(12),
	QDB(12), QDB(12), QDB(12), QDB(12), QDB(12),
	QDB(12), QDB(12), QDB(12)},
	{0, 0, QDB(12), QDB(12), QDB(12),
	QDB(12), QDB(12), QDB(12), QDB(12), QDB(12), QDB(12), 0, 0},
	0
};

/* CLM v3.3 has separate SISO/CDD power targets and per-MCS limits.
 * Express CDD limits here and fixup SISO in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_bn2_3 = {
	{ QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(8), QDB(8)},
	{0, 0, QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), /* 12.5 dBm */ 50, 0,
	0, 0, 0},
	0
};

/* CLM v3.8.2 has only CDD power targets.  */
static const locale_mimo_info_t locale_bn2_6 = {
	{ /* 12.5 */ 50, QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(15), QDB(15), QDB(15), 50},
	{0, 0, 50, QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(13), 0, 0},
	0
};

/* CLM v4.1.3 has separate SISO/CDD power targets and per-MCS limits.
 * Express CDD limits here and fixup SISO in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_bn2_8 = {
	{ QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(11), QDB(11)},
	{0, 0, QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), /* 12.5 dBm */ 50, 0,
	0, 0, 0},
	0
};


static const locale_mimo_info_t locale_bn4 = {
#if 1	/* ASUS modify */
	{QDB(30), QDB(30), QDB(30), QDB(30), QDB(30),
	QDB(30), QDB(30), QDB(30), QDB(30), QDB(30),
	QDB(30), QDB(30), QDB(30)},
	{0, 0, QDB(30), QDB(30), QDB(30),
	QDB(30), QDB(30), QDB(30), QDB(30), QDB(30),
	QDB(30), 0, 0},
#else
	{QDB(13), QDB(13), QDB(14), QDB(14), QDB(14),
	QDB(14), QDB(14), QDB(14), QDB(14), QDB(14),
	QDB(14), QDB(13), QDB(13)},
	{0, 0, QDB(13), QDB(14), QDB(14),
	QDB(14), QDB(14), QDB(14), QDB(14), QDB(14),
	QDB(13), 0, 0},
#endif
	0
};

static const locale_mimo_info_t locale_bn7 = {
	{QDB(16), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16)},
	{0, 0, QDB(16), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(16), 0, 0},
	WLC_EIRP
};

static const locale_mimo_info_t locale_bn5_1 = {
	{ /* 12.5 dBm */ 50, QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	/* 12.5 dBm */ 50, QDB(15), QDB(15)},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

/* CLM v3.7.2 has separate SISO/CDD power targets and per-MCS limits.
 * Express CDD limits here and fixup in wlc_channel_reg_limits().
*/
static const locale_mimo_info_t locale_cn = {
	{QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13)},
	{0, 0, QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), 0, 0},
	0
};

static const locale_mimo_info_t locale_cn_1 = {
	{QDB(11), QDB(11), QDB(11), QDB(11), QDB(11),
	QDB(11), QDB(11), QDB(11), QDB(11), QDB(11),
	QDB(11), QDB(11), QDB(11), QDB(11)},
	{0, 0, QDB(11), QDB(11), QDB(11),
	QDB(11), QDB(11), QDB(11), QDB(11), QDB(11),
	QDB(11), 0, 0},
	0
};

static const locale_mimo_info_t locale_cn_2 = {
	{ /* 13.5 */ 54, 54, 54, 54,
	54, 54, 54, 54, 54,
	54, 54, 54, 54},
	{0, 0, /* 10.5 */ 42, 42, 42,
	42, 42, 42, 42, 42,
	42, 0, 0},
	0
};

/* CLM v4.3.4 has separate SISO/CDD power targets.
 * Express CDD limits here and fixup SISO in wlc_channel_reg_limits().
*/
static const locale_mimo_info_t locale_cn_3 = {
	{ /* 15.5 */ 62, 62, 62, 62, 62,
	62, 62, 62, 62, 62,
	62, 62, 62, 62},
	{0, 0, /* 14.5 */ 58, 58, 58,
	58, 58, 58, 58, 58,
	58, 0, 0},
	0
};

static const locale_mimo_info_t locale_en = {
	{QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13)},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_en2 = {
	{QDB(8), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(8)},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_en2_1 = {
	{QDB(12), QDB(12), QDB(12), QDB(12), QDB(12),
	QDB(12), QDB(12), QDB(12), QDB(12), QDB(12),
	QDB(12), QDB(12), QDB(12)},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_fn = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	WLC_NO_MIMO
};

static const locale_mimo_info_t locale_dn = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	WLC_NO_MIMO
};

static const locale_mimo_info_t locale_gn = {
	{QDB(16), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16)},
	{QDB(16), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16), QDB(16), QDB(16),
	QDB(16), QDB(16), QDB(16)},
	0
};

static const locale_mimo_info_t locale_hn = {
	{QDB(17), QDB(17), QDB(17), QDB(17), QDB(17),
	QDB(17), QDB(17), QDB(17), QDB(17), QDB(17),
	QDB(17), QDB(17), QDB(17)},
	{QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15)},
	0
};

static const locale_mimo_info_t locale_jn = {
	{QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), 0, 0},
	{0, 0, QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), 0,
	0, 0, 0},
	0
};

static const locale_mimo_info_t locale_kn = {
	{QDB(9), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(9)},
	{0, 0, QDB(10), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), /* 11.5 dBm */ 46, 46,
	46, 0, 0},
	0
};

static const locale_mimo_info_t locale_kn1 = {
	{QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13)},
	{0, 0, /* 11.5 dBm */ 46, QDB(13), QDB(13),
	QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	QDB(13), 0, 0},
	0
};

static const locale_mimo_info_t locale_kn2 = {
	{QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15)},
	{0, 0, QDB(15), QDB(15), QDB(15),
	QDB(15), QDB(15), QDB(15), QDB(15), QDB(15),
	QDB(15), 0, 0},
	0
};

static const locale_mimo_info_t locale_kn4 = {
	{QDB(18), QDB(18), QDB(18), QDB(18), QDB(18),
	QDB(18), QDB(18), QDB(18), QDB(18), QDB(18),
	QDB(18), QDB(18), QDB(18)},
	{0, 0, QDB(18), QDB(18), QDB(18),
	QDB(18), QDB(18), QDB(18), QDB(18), QDB(18),
	QDB(18), 0, 0},
	0
};

/* CLM v4.5.3 has separate SISO/CDD power targets.
 * Express CDD limits here and fixup SISO in wlc_channel_reg_limits().
*/
static const locale_mimo_info_t locale_kn5 = {
	{ /* 13.5 dBm */ 54, 54, 54, 54, 54,
	54, 54, 54, 54, 54,
	54, 54, 54},
	{0, 0, 54, 54, 54,
	54, 54, 54, 54, 54,
	54, 0, 0},
	0
};

/* locale mimo 2g indexes */
#define LOCALE_MIMO_IDX_an1			0
#define LOCALE_MIMO_IDX_an1_t1			1
#define LOCALE_MIMO_IDX_an1_t2			2
#define LOCALE_MIMO_IDX_an2			3
#define LOCALE_MIMO_IDX_an3			4
#define LOCALE_MIMO_IDX_an4			5
#define LOCALE_MIMO_IDX_bn			6
#define LOCALE_MIMO_IDX_bn1			7
#define LOCALE_MIMO_IDX_bn2			8
#define LOCALE_MIMO_IDX_cn			9
#define LOCALE_MIMO_IDX_en			10
#define LOCALE_MIMO_IDX_en2			11
#define LOCALE_MIMO_IDX_fn			12
#define LOCALE_MIMO_IDX_gn			13
#define LOCALE_MIMO_IDX_hn			14
#define LOCALE_MIMO_IDX_jn			15
#define LOCALE_MIMO_IDX_kn			16
#define LOCALE_MIMO_IDX_an5			17
#define LOCALE_MIMO_IDX_an6			18
#define LOCALE_MIMO_IDX_bn4			19
#define LOCALE_MIMO_IDX_an7			20
#define LOCALE_MIMO_IDX_mimo_2g_all		21
#define LOCALE_MIMO_IDX_an8_t1			22
#define LOCALE_MIMO_IDX_an9_t1			23
#define LOCALE_MIMO_IDX_kn1			24
#define LOCALE_MIMO_IDX_an6_2			25
#define LOCALE_MIMO_IDX_bn2_1			26
#define LOCALE_MIMO_IDX_an8_t2			27
#define LOCALE_MIMO_IDX_bn5_1			28
#define LOCALE_MIMO_IDX_an1_t3			29
#define LOCALE_MIMO_IDX_an1_t4			30
#define LOCALE_MIMO_IDX_an1_t5			31
#define LOCALE_MIMO_IDX_bn2_2			32
#define LOCALE_MIMO_IDX_en2_1			33
#define LOCALE_MIMO_IDX_an6_1			34
#define LOCALE_MIMO_IDX_bn2_3			35
#define LOCALE_MIMO_IDX_bn_1			36
#define LOCALE_MIMO_IDX_bn2_4			37
#define LOCALE_MIMO_IDX_an_2			38
#define LOCALE_MIMO_IDX_bn2_5			39
#define LOCALE_MIMO_IDX_an7_2			40
#define LOCALE_MIMO_IDX_bn_2			41
#define LOCALE_MIMO_IDX_an2_1			42
#define LOCALE_MIMO_IDX_an2_2			43
#define LOCALE_MIMO_IDX_an2_3			44
#define LOCALE_MIMO_IDX_bn2_6			45
#define LOCALE_MIMO_IDX_an6_3			46
#define LOCALE_MIMO_IDX_kn2			47
#define LOCALE_MIMO_IDX_an2_4			48
#define LOCALE_MIMO_IDX_bn_3			49
#define LOCALE_MIMO_IDX_bn_4			50
#define LOCALE_MIMO_IDX_bn_5			51
#define LOCALE_MIMO_IDX_an10			52
#define LOCALE_MIMO_IDX_an7_3			53
#define LOCALE_MIMO_IDX_bn2_8			54
#define LOCALE_MIMO_IDX_kn4			55
#define LOCALE_MIMO_IDX_bn1_1			56
#define LOCALE_MIMO_IDX_dn			57
#define LOCALE_MIMO_IDX_bn_9			58
#define LOCALE_MIMO_IDX_cn_2			59
#define LOCALE_MIMO_IDX_an2_5			60
#define LOCALE_MIMO_IDX_an6_6			61
#define LOCALE_MIMO_IDX_cn_3			62
#define LOCALE_MIMO_IDX_an6_5			63
#define LOCALE_MIMO_IDX_cn_1			64
#define LOCALE_MIMO_IDX_kn5			65
#define LOCALE_MIMO_IDX_an6_4			66
#define LOCALE_MIMO_IDX_an6_4_3n		66 /* same as an6_4 */
#define LOCALE_MIMO_IDX_bn_10			67
#define LOCALE_MIMO_IDX_bn_10_3n		68
#define LOCALE_MIMO_IDX_an7_8			69
#define LOCALE_MIMO_IDX_bn7			70
#define LOCALE_MIMO_IDX_a_3n			71
#define LOCALE_MIMO_IDX_an6_7			72
#define LOCALE_MIMO_IDX_a1_3n			73
#define LOCALE_MIMO_IDX_an2_21			74
#define LOCALE_MIMO_IDX_b2_3n			75


static const locale_mimo_info_t * g_mimo_2g_table[]=
{
	&locale_an1,
	&locale_an1_t1,
	&locale_an1_t2,
	&locale_an2,
	&locale_an3,
	&locale_an4,
	&locale_bn,
	&locale_bn1,
	&locale_bn2,
	&locale_cn,
	&locale_en,
	&locale_en2,
	&locale_fn,
	&locale_gn,
	&locale_hn,
	&locale_jn,
	&locale_kn,
	&locale_an5,
	&locale_an6,
	&locale_bn4,
	&locale_an7,
	&locale_mimo_all,
	&locale_an8_t1,
	&locale_an9_t1,
	&locale_kn1,
	&locale_an6_2,
	&locale_bn2_1,
	&locale_an8_t2,
	&locale_bn5_1,
	&locale_an1_t3,
	&locale_an1_t4,
	&locale_an1_t5,
	&locale_bn2_2,
	&locale_en2_1,
	&locale_an6_1,
	&locale_bn2_3,
	&locale_bn_1,
	&locale_bn2_4,
	&locale_an_2,
	&locale_bn2_5,
	&locale_an7_2,
	&locale_bn_2,
	&locale_an2_1,
	&locale_an2_2,
	&locale_an2_3,
	&locale_bn2_6,
	&locale_an6_3,
	&locale_kn2,
	&locale_an2_4,
	&locale_bn_3,
	&locale_bn_4,
	&locale_bn_5,
	&locale_an10,
	&locale_an7_3,
	&locale_bn2_8,
	&locale_kn4,
	&locale_bn1_1,
	&locale_dn,
	&locale_bn_9,
	&locale_cn_2,
	&locale_an2_5,
	&locale_an6_6,
	&locale_cn_3,
	&locale_an6_5,
	&locale_cn_1,
	&locale_kn5,
	&locale_an6_4,
	&locale_bn_10,
	&locale_bn_10_3n,
	&locale_an7_8,
	&locale_bn7,
	&locale_a_3n,
	&locale_an6_7,
	&locale_a1_3n,
	&locale_an2_21,
	&locale_b2_3n

};

/*
 * MIMO Locale Definitions - 5 GHz
 */

static const locale_mimo_info_t locale_1n = {
	{ QDB(10), QDB(16), QDB(13), 0, QDB(15)},
	{ QDB(12), /* 15.5 dBm */ 62, QDB(12), 0, QDB(15)},
	0
};

static const locale_mimo_info_t locale_1an = {
	{ QDB(10), QDB(16), QDB(13), 0, QDB(15)},
	{ QDB(12), /* 15.5 dBm */ 62, QDB(12), 0, QDB(15)},
	0
};

/* CLM 4.2.3 CDD only. per MCS power limit is fixed up in wlc_channel_reg_limits(). */
static const locale_mimo_info_t locale_1cn = {
	{ /* 6.5 */ 26, QDB(13), QDB(13), 0, QDB(17)},
	{ QDB(9), /* 15.5 dBm */ 62, QDB(10), 0, QDB(18)},
	0
};

static const locale_mimo_info_t locale_2n = {
	{ QDB(13), QDB(13), QDB(13), QDB(13), QDB(13)},
	{ QDB(13), QDB(13), QDB(13), QDB(13), QDB(13)},
	0
};

static const locale_mimo_info_t locale_3_3n = {
	{ QDB(16), QDB(16), QDB(16), QDB(16), 0},
	{ QDB(18), QDB(18), QDB(18), QDB(18), 0},
	0
};

static const locale_mimo_info_t locale_3n = {
	{ /* 12.5 dBm */ 50, 50, 50, /* 15.5 dBm */ 62, 0},
	{QDB(14), QDB(14), QDB(14), QDB(16), 0},
	0
};

/* CLM v4.6.6 */
/* Fix up HT20 ch[100-102]=21dBm in wlc_channel_reg_limits() */
static const locale_mimo_info_t locale_3n_1 = {
	{QDB(15), QDB(15), QDB(15), 86, 0},
	{QDB(15), QDB(15), QDB(15), 86, 0},
	0
};

static const locale_mimo_info_t locale_3an = {
	{ /* 12.5 dBm */ 50, 50, 50, QDB(13), 0},
	{0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_3an_1 = {
	{ /* 12.5 dBm */ 50, 50, 50, /* 13.5 dBm */ 54, 0},
	{QDB(14), QDB(14), QDB(14), QDB(16), 0},
	0
};

static const locale_mimo_info_t locale_3bn = {
	{ /* 17.5 dBm */ 70, 70, 70, QDB(20), 0},
	{ 70, 70, 70, QDB(18), 0},
	0
};

static const locale_mimo_info_t locale_3cn = {
	{ /* 12.5 dBm */ 50, 50, 50, QDB(14), 0},
	{QDB(14), QDB(14), QDB(14), QDB(14), 0},
	0
};

static const locale_mimo_info_t locale_3dn = {
	{ /* 15.5 dBm */ 62, 62, 62, /* 14.5 */ 58, 0},
	{ /* 14.5 */ 58, 58, 58, /* 13.5 */ 54, 0},
	0
};

static const locale_mimo_info_t locale_3en = {
	{ QDB(18), QDB(18), QDB(18), QDB(18), 0},
	{ QDB(20), QDB(20), QDB(20), QDB(20), 0},
	WLC_EIRP | WLC_DFS_EU
};

static const locale_mimo_info_t locale_3jn = {
	{ /* 12.5 dBm */ 50, 50, 50, /* 15.5 dBm */ 62, 0},
	{ /* 10.5 dBm */ 42, 42, 42, QDB(16), 0},
	0
};

static const locale_mimo_info_t locale_3jn_1 = {
	{QDB(10), QDB(10), QDB(10), /* 15.5 dBm */ 62, 0},
	{QDB(10), QDB(10), QDB(10), QDB(16), 0},
	0
};

static const locale_mimo_info_t locale_3jn_2 = {
	{ /* 15.5 dBm */ 62, 62, 62, /* 14.5 dBm */ 58, 0},
	{ /* 14.5 dBm */ 58, 58, 58, /* 13.5 dBm */ 54, 0},
	0
};

static const locale_mimo_info_t locale_3jn_3 = {
	{QDB(11), QDB(11), QDB(11), QDB(13), 0},
	{ /* 10.5 */ 42, 42, 42,  QDB(16), 0},
	0
};

static const locale_mimo_info_t locale_3jn_4 = {
	{QDB(12), QDB(8), QDB(8), QDB(16), 0},
	{QDB(10), QDB(8), QDB(8), QDB(16), 0},
	0
};

static const locale_mimo_info_t locale_3ln = {
	{QDB(11), QDB(11), /* 10.5 dBm */ 42, 42, 0},
	{QDB(13), /* 12.5 dBm */ 50, 50, /* 13.5 dBm */ 54, 0},
	0
};

static const locale_mimo_info_t locale_3ln_1 = {
	{QDB(11), QDB(11), QDB(11), QDB(11), 0},
	{ /* 12.5 dBm */ 50, 50, 50, QDB(13), 0},
	0
};

static const locale_mimo_info_t locale_3rn = {
	{ /* 12.5 dBm */ 50, 50, 50, /* 15.5 dBm */ 62, 0},
	{QDB(14), QDB(14), QDB(14), QDB(16), 0},
	0
};

static const locale_mimo_info_t locale_3rn_4 = {
	{ QDB(18), QDB(18), QDB(18), QDB(25), 0},
	{ QDB(18), QDB(18), QDB(18), QDB(25), 0},
	WLC_EIRP
};

static const locale_mimo_info_t locale_3rn_3 = {
	{ QDB(15), QDB(15), QDB(15), QDB(16), 0},
	{ QDB(16), QDB(16), QDB(16), QDB(16), 0},
	0
};

static const locale_mimo_info_t locale_3sn = {
	{ QDB(18), QDB(18), QDB(18), QDB(18), 0},
	{ QDB(18), QDB(18), QDB(18), QDB(18), 0},
	WLC_EIRP
};

static const locale_mimo_info_t locale_3tn = {
	{ QDB(18), QDB(18), QDB(18), QDB(18), 0},
	{ QDB(18), QDB(18), QDB(18), QDB(18), 0},
	WLC_EIRP
};

static const locale_mimo_info_t locale_4n = {
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},
	WLC_NO_MIMO
};

static const locale_mimo_info_t locale_5n = {
	{QDB(13), QDB(13), QDB(13), 0, QDB(15)},
	{QDB(14), QDB(14), QDB(14), 0, QDB(15)},
	0
};

static const locale_mimo_info_t locale_5ln = {
	{QDB(11), QDB(11), /* 10.5 dBm */ 42, 0, QDB(15)},
	{QDB(13), /* 12.5 dBm */ 50, 50, 0, QDB(15)},
	0
};

static const locale_mimo_info_t locale_5ln_1 = {
	{QDB(11), QDB(11), QDB(11), 0, QDB(15)},
	{ /* 12.5 dBm */ 50, 50, 50, 0, QDB(15)},
	0
};

static const locale_mimo_info_t locale_5ln_2 = {
	{QDB(12), QDB(12), QDB(12), 0, QDB(16)},
	{QDB(13), QDB(13), QDB(13), 0, QDB(16)},
	0
};

static const locale_mimo_info_t locale_5n_1 = {
	{ /* 12.5 dBm */ 50, 50, 50, 0, QDB(17)},
	{QDB(14), QDB(14), QDB(14), 0, QDB(18)},
	0
};

static const locale_mimo_info_t locale_5an = {
	{QDB(13), QDB(13), QDB(13), QDB(15), QDB(15)},
	{QDB(14), QDB(14), QDB(14), QDB(15), QDB(15)},
	0
};

static const locale_mimo_info_t locale_6n = {
	{QDB(13), QDB(13), QDB(13), /* 15.5 dBm */ 62, QDB(15)},
	{QDB(14), QDB(14), QDB(14), QDB(16), QDB(15)},
	0
};

static const locale_mimo_info_t locale_6an = {
	{QDB(11), QDB(11), QDB(11), QDB(11), QDB(15)},
	{ /* 12.5 */ 50, 50, 50, QDB(13), QDB(15)},
	0
};

static const locale_mimo_info_t locale_6an_1 = {
	{QDB(11), QDB(11), QDB(11), 0, QDB(17)},
	{QDB(14), QDB(15), QDB(15), 0, QDB(17)},
	0
};

static const locale_mimo_info_t locale_7n = {
	{0, 0, 0, 0, QDB(15)},
	{0, 0, 0, 0, QDB(15)},
	0
};

static const locale_mimo_info_t locale_7cn = {
	{0, 0, 0, 0, QDB(13)},
	{0, 0, 0, 0, QDB(13)},
	0
};

static const locale_mimo_info_t locale_7cn_1 = {
	{0, 0, 0, 0, QDB(16)},
	{0, 0, 0, 0, QDB(16)},
	0
};

static const locale_mimo_info_t locale_8n = {
	{0, QDB(16), QDB(16), 0, QDB(15)},
	{0, /* 16.5 dBm */ 66, 66, 0, QDB(15)},
	0
};

static const locale_mimo_info_t locale_8an = {
	{0, QDB(10), QDB(10),  /* 16.5 dBm */ 66, QDB(17)},
	{0, QDB(12), QDB(12), /* 18.5 dBm */ 74, QDB(18)},
	0
};

static const locale_mimo_info_t locale_8an_1 = {
	{0, QDB(11), QDB(11),  QDB(17), QDB(17)},
	{0, QDB(15), QDB(15), QDB(17), QDB(17)},
	0
};

static const locale_mimo_info_t locale_8bn = {
	{0, QDB(10), QDB(10),  QDB(14), QDB(17)},
	{0, QDB(12), QDB(12), QDB(17), QDB(17)},
	0
};

/* CLM 4.2.3 per MCS power limit is fixed up in wlc_channel_reg_limits(). */
static const locale_mimo_info_t locale_8cn = {
	{0, /* 6.5 */ 26, 26,  /* 14.5 */ 58, QDB(17)},
	{0, QDB(9), QDB(17), QDB(18), QDB(18)},
	0
};

static const locale_mimo_info_t locale_9n = {
	{0, 0, 0, 0, QDB(15)},
	{0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_9hn = {
	{0, 0, 0, 0, QDB(23)},
	{0, 0, 0, 0, QDB(23)},
	0
};

static const locale_mimo_info_t locale_10n = {
	{QDB(10), 0, 0, 0, QDB(10)},
	{QDB(10), 0, 0, 0, QDB(10)},
	0
};

static const locale_mimo_info_t locale_11n = {
	{ /* 12.5 dBm */ 50, 50, 50, QDB(15), QDB(15)},
	{QDB(14), QDB(15), QDB(15), QDB(15), QDB(15)},
	0
};

static const locale_mimo_info_t locale_11ln = {
	{QDB(11), QDB(9), QDB(9), /* 10.5 dBm */ 42, QDB(15)},
	{QDB(13), /* 12.5 dBm */ 50, 50, /* 13.5 dBm */ 54, QDB(15)},
	0
};

/* CLM v4.1.3 has separate SISO/CDD power targets and per-MCS limits.
 * Express CDD limits here and fixup SISO in wlc_channel_reg_limits().
*/
/* CLM v4.1.3-2 defined Locale 11ln-2 and 11ln-3, which are only different
 * in restricted channels: 11ln-2 has restricted set2 and 11ln-3 has restricted
 * set 8. This difference will be reflected at the base locale info, so ony one
 * locale 11ln-2 is needed for mimo locale
 */
static const locale_mimo_info_t locale_11ln_2 = {
	{QDB(10), /* 12.5 dBm */ 50, 50, /* 15.5 dBm */ 62, QDB(17)},
	{ /* 10.5 dBm */ 42, 42, 42, QDB(16), QDB(18)},
	0
};

/* CLM V4.2.3 per MCS power limit fixed in wlc_channel_reg_limit(). */
static const locale_mimo_info_t locale_11ln_4 = {
	{QDB(13), QDB(13), /* 6.5 dBm */ 26, /* 14.5 dBm */ 58, QDB(17)},
	{QDB(13), QDB(13), QDB(9), QDB(17), QDB(18)},
	0
};

static const locale_mimo_info_t locale_12n = {
	{0, 0, 0, 0, QDB(10)},
	{0, 0, 0, 0, QDB(10)},
	0
};

static const locale_mimo_info_t locale_13n = {
	{ /* 12.5 dBm */ 50, 50, 50, 0, 0},
	{QDB(14), QDB(14), QDB(14), 0, 0},
	0
};

static const locale_mimo_info_t locale_13n_2 = {
	{ QDB(15), QDB(15), QDB(15), 0, 0},
	{QDB(16), QDB(16), QDB(16), 0, 0},
	0
};

static const locale_mimo_info_t locale_14n = {
	{ QDB(21), QDB(21), QDB(21), 0, QDB(21)},
	{QDB(21), QDB(21), QDB(21), 0, QDB(21)},
	0
};

static const locale_mimo_info_t locale_13n_1 = {
	{QDB(11), QDB(11), QDB(11), 0, 0},
	{ /* 12.5 */ 50, 50, 50, 0, 0},
	0
};

static const locale_mimo_info_t locale_14an = {
	{QDB(11), QDB(11), QDB(11), 0, QDB(15)},
	{ /* 12.5 */ 50, 50, 50, 0, QDB(15)},
	0
};

static const locale_mimo_info_t locale_15n = {
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},
	WLC_NO_MIMO
};

static const locale_mimo_info_t locale_16n = {
	{ /* 13.5 */ 54, 54, 54, 0, 54},
	{ 54, 54, 54, 0, 54},
	0
};


static const locale_mimo_info_t locale_16an = {
	{ QDB(11), QDB(11), QDB(11), 0, QDB(15)},
	{ /* 12.5 */ 50, 50, 50, 0, QDB(15)},
	0
};

static const locale_mimo_info_t locale_17n = {
	{ /* 12.5 dBm */ 50, 50, 50, 0, 50},
	{QDB(14), QDB(14), QDB(14), 0, QDB(14)},
	0
};


static const locale_mimo_info_t locale_17an = {
	{QDB(11), QDB(11), QDB(11), 0, QDB(15)},
	{ /* 12.5 */ 50, 50, 50, 0, QDB(15)},
	0
};

static const locale_mimo_info_t locale_18n = {
	{ /* 12.5 dBm */ 50, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_18n_1 = {
	{ QDB(12), 0, 0, 0, 0},
	{ QDB(14), 0, 0, 0, 0},
	0
};

static const locale_mimo_info_t locale_18n_1_3n = {
	{ /* 10.25 dBm */ 41, 0, 0, 0, 0},
	{ /* 12.25 dBm */ 49, 0, 0, 0, 0},
	0
};

static const locale_mimo_info_t locale_18bn = {
	{ /* 13.5 dBm */ 54, 0, 0, 0, 0},
	{QDB(15), 0, 0, 0, 0},
	0
};

static const locale_mimo_info_t locale_18rn = {
	{ /* 12.5 dBm */ 50, 0, 0, 0, 0},
	{QDB(14), 0, 0, 0, 0},
	0
};

static const locale_mimo_info_t locale_18ln = {
	{QDB(10), 0, 0, 0, 0},
	{QDB(12), 0, 0, 0, 0},
	0
};

/* CLM v3.0 has separate power targets in the channel 100-140 range.
 * Use ch 100-102 here, and fixup channels 104-140 in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_19n = {
	{QDB(10), QDB(16), QDB(14), /* 16.5 dBm */ 66, QDB(17)},
	{QDB(12), QDB(17), QDB(12),  /* 15.5 dBm */ 62, QDB(18)},
	0
};

/* CLM v3.0 has separate power targets for SISO/CDD and for the channel 100-140 range.
 * Use CDD/SDM and ch 100-102 here, and fixup SISO and channels 104-140 in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_19n_1 = {
	{QDB(10), QDB(17), QDB(16), /* 13.5 dBm */ 54, QDB(17)},
	{QDB(12), QDB(16), QDB(13), QDB(11), QDB(16)},
	0
};

/* CLM v3.7 has common power targets for SISO/CDD except for channels 36 - 48.
 * Use CDD/SDM here, and fixup SISO chan 36-48 in wlc_channel_reg_limits().
 *
 */
static const locale_mimo_info_t locale_19n_2 = {
	{QDB(10), QDB(13), QDB(13), QDB(14), /* 20.5 */ 82},
	{QDB(12), QDB(14), QDB(13), QDB(14), /* 19.5 */ 78},
	0
};

static const locale_mimo_info_t locale_19n_3 = {
	{QDB(11), /* 14.5 */ 58, QDB(11), QDB(14), /* 14.5 */ 58},
	{QDB(9), /* 14.5 */ 58, QDB(9), QDB(13), /* 13.5 */ 54},
	0
};

/* CLM v3.4 has power targets only for SISO. */
static const locale_mimo_info_t locale_11n_1 = {
	{ /* 12.5 dBm */ 50, 50, 50, 50, 50},
	{ /* 12.5 dBm */ 50, 50, 50, /* 13.5 dBm */ 54, 54},
	0
};

/* CLM v1.12.4.2 has 15.5 for channels 100-102/40MHz and 17 for channels 104-140/40MHz
 * used lower levels for channels 100-140 here, and fixup channels 104-140 in
 * wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_19an = {
	{QDB(10), QDB(16), QDB(13), QDB(17), QDB(17)},
	{QDB(12), /* 15.5 dBm */ 62, QDB(12), /* 15.5 dBm */ 62, QDB(18)},
	0
};

/* CLM v1.12.2.2 has 16.5/15.5 for channels 100-102 and 16.5/18.5 for channels 104-140
 * used lower levels for channels 100-140 here, and fixup channels 104-140 in
 * wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_19bn = {
	{QDB(10), QDB(16), QDB(13), /* 16.5 dBm */ 66, 0},
	{QDB(12), /* 15.5 dBm */ 62, QDB(12), /* 15.5 dBm */ 62, 0},
	0
};

/* CLM v3.0 has separate power targets in the 100-140 channel range.
 * Use ch 100-102 here, and fixup channels 104-140 in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_19cn = {
	{QDB(10), QDB(16), QDB(14), /* 16.5 dBm */ 66, QDB(17)},
	{QDB(12), QDB(17), /* 11.5 dBm */ 46, /* 15.5 dBm */ 62, QDB(18)},
	0
};

/* CLM v1.12.4 has separate SISO/CDD power targets and split 100-140 channel range.
 * Express rough CDD/SCM limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_19ln = {
	{ /* 9.5 dBm */ 38, QDB(15), QDB(13), QDB(14), QDB(17)},
	{QDB(12), /* 15.5 dBm */ 62, QDB(12), /* 15.5 dBm */ 62, QDB(17)},
	0
};

/* CLM v3.1 has separate SISO/CDD power targets and split 100-140 channel range.
 * Express rough CDD/SCM limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_19ln_1 = {
	/* channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	{QDB(10), QDB(16), QDB(13), /* 16.5 dBm */ 66, QDB(17)},
	{QDB(12), /* 15.5 dBm */ 62, /* 11.5 dBm */ 46, /* 18.5 dBm */ 74, QDB(18)},
	LOCALE_RESTRICTED_CHAN_165
};

/* CLM v3.7.4 has separate SISO/CDD power targets and split 100-140 channel range.
 * Express rough CDD/SDM limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_19ln_2 = {
	/* channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	{ QDB(7), QDB(13), QDB(13), QDB(13), QDB(17)},
	{QDB(9), /* 15.5 dBm */ 62, QDB(10), QDB(12), QDB(18)},
	LOCALE_RESTRICTED_CHAN_165
};

/* CLM v3.8.5 has only CDD power targets */
static const locale_mimo_info_t locale_19ln_3 = {
	/* channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	{QDB(10), QDB(16), QDB(13), QDB(16), QDB(16)},
	{QDB(12), QDB(14), QDB(12), QDB(15), QDB(16)},
	0
};

/* CLM v4.2.3  per MCS CDD and  SISO power targets is fixed up in wlc_channel_reg_limits(). */
static const locale_mimo_info_t locale_19ln_4 = {
	/* channel 36 - 48, 52 - 64, 100 - 140, 149 - 165 */
	{ /* 6.5 */ 26, QDB(13), QDB(13), /* 14.5 */ 54, QDB(17)},
	{QDB(9), /* 15.5 */ 62, QDB(10), QDB(17), QDB(18)},
	0
};

/* CLM v1.11.5 has separate SISO/CDD power targets and split 100-140 channel range.
 * Express rough CDD/SCM limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_19hn = {
	{QDB(10), QDB(16), QDB(13), /* 16.5 dBm */ 66, QDB(17)},
	{QDB(12), /* 15.5 dBm */ 62, QDB(12), /* 15.5 dBm */ 62, QDB(18)},
	0
};

/* CLM v3.8.6 has separate SISO/CDD power targets and split 100-140 channel range.
 * Express rough CDD/SCM limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_19hn_1 = {
	{QDB(10), /* 17.5 */ 70, /* 15.5 */ 62, 70, /* 18.5 */ 74},
	{QDB(12), /* 16.5 dBm */ 66, /* 14.5 */ 58, 66, 70},
	0
};

static const locale_mimo_info_t locale_19hn_2 = {
	{ /* 11.5 */ 46, /* 21.5 */ 86, /* 17.5 */ 70, 86, 86},
	{ /* 13.5 */ 54, 86, /* 14.5 */ 58, 86, 86},
	0
};

/* CLM v1.12.2.2 has separate SISO/CDD power targets and split 100-140 channel range.
 * Express rough CDD/SCM limits here and fixup in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_19rn = {
	{QDB(10), QDB(16), QDB(13), /* 16.5 dBm */ 66, QDB(17)},
	{QDB(12), /* 15.5 dBm */ 62, QDB(12), /* 15.5 dBm */ 62, QDB(18)},
	0
};

static const locale_mimo_info_t locale_19_3n = {
	{ /* 3.5 */ 14, QDB(12), QDB(12), /* 14.5 dBm */ 58, QDB(17)},
	{QDB(7), QDB(14), QDB(10), QDB(17), QDB(18)},
	0
};

static const locale_mimo_info_t locale_20n = {
	{0, 0, 0, /* 15.5 dBm */ 62, 0},
	{0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_21n = {
	{0, QDB(14), QDB(14), 0, QDB(14)},
	{0, QDB(14), QDB(14), 0, QDB(14)},
	0
};

static const locale_mimo_info_t locale_22n = {
	{ /* 12.5 dBm */ 50, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_23n = {
	{ /* 12.5 dBm */ 50, 50, 50, 0, 0},
	{0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_24n = {
	{ /* 12.5 dBm */ 50, 50, 50, /* 15.5 dBm */ 62, 0},
	{0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_25n = {
	{ QDB(9), QDB(13), QDB(13), QDB(13), QDB(13)},
	{0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_25hn = {
	{QDB(12), QDB(16), QDB(16), 0, 0},
	{QDB(15), QDB(16), QDB(16), 0, 0},
	0
};

static const locale_mimo_info_t locale_25kn = {
	{ QDB(9), QDB(13), QDB(13), QDB(13), QDB(13)},
	{ QDB(12), QDB(13), QDB(13), QDB(13), QDB(13)},
	0
};

static const locale_mimo_info_t locale_25ln = {
	{ QDB(9), QDB(13), QDB(13), QDB(13), QDB(13)},
	{ QDB(12), QDB(12), QDB(12), QDB(12), QDB(12)},
	0
};

static const locale_mimo_info_t locale_26n = {
	{ /* 12.5 dBm */ 50, 50, 50, /* 15.5 dBm */ 62, 0},
	{0, 0, 0, 0, 0},
	WLC_NO_40MHZ
};

static const locale_mimo_info_t locale_27n = {
	{QDB(10), 0, 0, 0, QDB(15)},
	{QDB(12), 0, 0, 0, QDB(15)},
	0
};

static const locale_mimo_info_t locale_27bn = {
	{QDB(10), 0, 0, 0, QDB(20)},
	{QDB(12), 0, 0, 0, /* 23.5 dBm */ 94},
	0
};

static const locale_mimo_info_t locale_27cn = {
	{ /* 11.5 dBm */ 46, 0, 0, 0, QDB(23)},
	{QDB(12), 0, 0, 0, QDB(23)},
	0
};

static const locale_mimo_info_t locale_27en = {
	{ /* 11.5 dBm */ 46, 0, 0, 0, /* 21.5 dBm */ 86},
	{46, 0, 0, 0, 86},
	0
};

static const locale_mimo_info_t locale_28n = {
	{QDB(10), 0, 0, 0, 0},
	{QDB(12), 0, 0, 0, 0},
	0
};

/* CLM v1.12.1 has different SISO vs CDD/SDM limits for 29n.
 * Express CDD/SDM limits here and fixup SISO in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_29n = {
	{QDB(10), QDB(15), QDB(15), QDB(15), QDB(15)},
	{QDB(12), QDB(16), QDB(16), QDB(16), QDB(16)},
	0
};

static const locale_mimo_info_t locale_29_3n = {
	{ /* 11.25 dBm */ 45, /* 15.5 dBm */ 62, QDB(15), /* 15.5 dBm */ 62, /* 24.75 dBm */ 99 },
	{QDB(13), QDB(18), /* 12.5 dBm */ 50, QDB(18), /* 24.75 dBm */ 99 },
	0
};

/* CLM v1.12.2.4 has different SISO vs CDD/SDM limits for 29n.
 * Express CDD/SDM limits here and fixup SISO in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_29an = {
	{QDB(10), QDB(15), QDB(15), QDB(15), QDB(15)},
	{QDB(12), QDB(16), QDB(16), QDB(16), QDB(16)},
	0
};

/* CLM v2.1 has different limits on 20Mhz ch 100-116 vs. 132-140 for 29n.
 * Express ch 132-140 limits here and fixup SISO in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_29bn = {
	{QDB(10), QDB(15), QDB(15), QDB(16), QDB(20)},
	{QDB(12), QDB(15), QDB(12), QDB(15), /* 23.5 dBm */ 94},
	0
};

static const locale_mimo_info_t locale_29bn_1 = {
	{46 /* 11.5 dBm */, 74  /* 18.5 dBm */, 74 , 82 /* 20.5 dBm */, QDB(23)},
	{QDB(12), 82, 66 /* 16.5 dBm */, 82, QDB(23)},
	0
};

/* CLM v4.8.2 has different limits on 20Mhz ch 100-116 vs. 132-140 for 29n.
 * Express ch 132-140 limits here and fixup SISO in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_29bn_3 = {
	{QDB(12), QDB(19),  QDB(19), /* 20.5 dBm */ 82,  /* 26.5 dBm */ 106 },
	{QDB(14), 82, 66 /* 16.5 dBm */, 82, 106},
	0
};

/* CLM v1.12.1 has different SISO vs CDD/SDM limits for 29n.
 * Express CDD/SDM limits here and fixup SISO in wlc_channel_reg_limits().
 */
static const locale_mimo_info_t locale_29cn = {
	{QDB(13), QDB(13), QDB(13), /* 12.5 dBm */ 50, QDB(13)},
	{QDB(13), /* 12.5 dBm */ 50, /* 12.5 dBm */ 50, /* 12.5 dBm */ 50, QDB(13)},
	0
};

static const locale_mimo_info_t locale_29dn = {
	{QDB(10), 58 /* 14.5 */, 58, /* 17.5 dBm */ 70, /* 19.5 */ 78},
	{QDB(12), 58, 58, 58, 78},
	0
};

/* CLM v3.8.15 has only CDD/SDM limits for 29dn_1. */
static const locale_mimo_info_t locale_29dn_1 = {
	{ /* 11.5 dBm */ 46, /* 20.5 dBm */ 82, 82, 82, /* 22.5 dBm */ 90},
	{ /* 13.5 dBm */ 54, /* 22.5 dBm */ 90, 90, 90, 90},
	0
};

static const locale_mimo_info_t locale_29dn_2 = {
	{QDB(11), QDB(17), QDB(17), QDB(17), QDB(17)},
	{QDB(13), QDB(15), QDB(15), QDB(17), QDB(17)},
	0
};

static const locale_mimo_info_t locale_31n = {
	{QDB(10), 0, 0, 0, QDB(15)},
	{QDB(12), 0, 0, 0, QDB(15)},
	0
};


#define LOCALE_MIMO_IDX_1n			0
#define LOCALE_MIMO_IDX_1an			1
#define LOCALE_MIMO_IDX_2n			2
#define LOCALE_MIMO_IDX_3n			3
#define LOCALE_MIMO_IDX_3an			4
#define LOCALE_MIMO_IDX_3jn			5
#define LOCALE_MIMO_IDX_3rn			6
#define LOCALE_MIMO_IDX_5n			7
#define LOCALE_MIMO_IDX_5an			8
#define LOCALE_MIMO_IDX_6n			9
#define LOCALE_MIMO_IDX_7n			10
#define LOCALE_MIMO_IDX_8n			11
#define LOCALE_MIMO_IDX_9n			12
#define LOCALE_MIMO_IDX_10n			13
#define LOCALE_MIMO_IDX_11n			14
#define LOCALE_MIMO_IDX_12n			15
#define LOCALE_MIMO_IDX_13n			16
#define LOCALE_MIMO_IDX_7cn			17
#define LOCALE_MIMO_IDX_15n			18
#define LOCALE_MIMO_IDX_16n			19
#define LOCALE_MIMO_IDX_17n			20
#define LOCALE_MIMO_IDX_18n			21
#define LOCALE_MIMO_IDX_18rn			22
#define LOCALE_MIMO_IDX_19n			23
#define LOCALE_MIMO_IDX_19an			24
#define LOCALE_MIMO_IDX_19bn			25
#define LOCALE_MIMO_IDX_19hn			26
#define LOCALE_MIMO_IDX_19ln			27
#define LOCALE_MIMO_IDX_19rn			28
#define LOCALE_MIMO_IDX_20n			29
#define LOCALE_MIMO_IDX_21n			30
#define LOCALE_MIMO_IDX_22n			31
#define LOCALE_MIMO_IDX_23n			32
#define LOCALE_MIMO_IDX_24n			33
#define LOCALE_MIMO_IDX_25n			34
#define LOCALE_MIMO_IDX_25kn			35
#define LOCALE_MIMO_IDX_26n			36
#define LOCALE_MIMO_IDX_27n			37
#define LOCALE_MIMO_IDX_28n			38
#define LOCALE_MIMO_IDX_29n			39
#define LOCALE_MIMO_IDX_29an			40
#define LOCALE_MIMO_IDX_29cn			41
#define LOCALE_MIMO_IDX_29bn_1			42
#define LOCALE_MIMO_IDX_8bn			43
#define LOCALE_MIMO_IDX_31n			44
#define LOCALE_MIMO_IDX_19hn_2			45
#define LOCALE_MIMO_IDX_18bn			46
#define LOCALE_MIMO_IDX_27bn			47
#define LOCALE_MIMO_IDX_29bn			48
#define LOCALE_MIMO_IDX_mimo_5g_all		49
#define LOCALE_MIMO_IDX_3cn			50
#define LOCALE_MIMO_IDX_25ln			51
#define LOCALE_MIMO_IDX_27cn			52
#define LOCALE_MIMO_IDX_19n_1			53
#define LOCALE_MIMO_IDX_19cn			54
#define LOCALE_MIMO_IDX_19ln_1			55
#define LOCALE_MIMO_IDX_3an_1			56
#define LOCALE_MIMO_IDX_5n_1			57
#define LOCALE_MIMO_IDX_11n_1			58
#define LOCALE_MIMO_IDX_8an			59
#define LOCALE_MIMO_IDX_19n_2			60
#define LOCALE_MIMO_IDX_11ln_2			61
#define LOCALE_MIMO_IDX_3jn_1			62
#define LOCALE_MIMO_IDX_3ln			63
#define LOCALE_MIMO_IDX_5ln			64
#define LOCALE_MIMO_IDX_11ln			65
#define LOCALE_MIMO_IDX_19ln_2			66
#define LOCALE_MIMO_IDX_14n			67
#define LOCALE_MIMO_IDX_19ln_3			68
#define LOCALE_MIMO_IDX_19hn_1			69
#define LOCALE_MIMO_IDX_29dn			70
#define LOCALE_MIMO_IDX_3bn			71
#define LOCALE_MIMO_IDX_19ln_4			72
#define LOCALE_MIMO_IDX_1cn			73
#define LOCALE_MIMO_IDX_3jn_3			74
#define LOCALE_MIMO_IDX_3ln_1			75
#define LOCALE_MIMO_IDX_5ln_1			76
#define LOCALE_MIMO_IDX_6an			77
#define LOCALE_MIMO_IDX_8cn			78
#define LOCALE_MIMO_IDX_11ln_4			79
#define LOCALE_MIMO_IDX_13n_1			80
#define LOCALE_MIMO_IDX_14an			81
#define LOCALE_MIMO_IDX_16an			82
#define LOCALE_MIMO_IDX_17an			83
#define LOCALE_MIMO_IDX_18ln			84
#define LOCALE_MIMO_IDX_9hn			85
#define LOCALE_MIMO_IDX_4n			86
#define LOCALE_MIMO_IDX_3dn			87
#define LOCALE_MIMO_IDX_3jn_2			88
#define LOCALE_MIMO_IDX_19n_3			89
#define LOCALE_MIMO_IDX_3jn_4			90
#define LOCALE_MIMO_IDX_3rn_3			91
#define LOCALE_MIMO_IDX_5ln_2			92
#define LOCALE_MIMO_IDX_6an_1			93
#define LOCALE_MIMO_IDX_7cn_1			94
#define LOCALE_MIMO_IDX_8an_1			95
#define LOCALE_MIMO_IDX_13n_2			96
#define LOCALE_MIMO_IDX_25hn			97
#define LOCALE_MIMO_IDX_29dn_2			98
#define LOCALE_MIMO_IDX_18n_1			99
#define LOCALE_MIMO_IDX_27en			100
#define LOCALE_MIMO_IDX_27en_3n			100 /* same as LOCALE_MIMO_IDX_27en */
#define LOCALE_MIMO_IDX_18n_1_3n		101
#define LOCALE_MIMO_IDX_3tn			102
#define LOCALE_MIMO_IDX_3sn			103
#define LOCALE_MIMO_IDX_19_3n			104
#define LOCALE_MIMO_IDX_3rn_4			105
#define LOCALE_MIMO_IDX_29dn_1			106
#define LOCALE_MIMO_IDX_29bn_3			107
#define LOCALE_MIMO_IDX_29_3n			108
#define LOCALE_MIMO_IDX_3n_1			109 /* CLM v4.6.6 */
#define LOCALE_MIMO_IDX_3en				110
#define LOCALE_MIMO_IDX_3_3n			111


/* 11ln-2 and 11ln-3 should be the same 11n locale, the difference in */
/* restricted channels is reflected in the base locale info in the Country Info */
#define LOCALE_MIMO_IDX_11ln_3 			LOCALE_MIMO_IDX_11ln_2

static const locale_mimo_info_t * g_mimo_5g_table[]=
{
	&locale_1n,
	&locale_1an,
	&locale_2n,
	&locale_3n,
	&locale_3an,
	&locale_3jn,
	&locale_3rn,
	&locale_5n,
	&locale_5an,
	&locale_6n,
	&locale_7n,
	&locale_8n,
	&locale_9n,
	&locale_10n,
	&locale_11n,
	&locale_12n,
	&locale_13n,
	&locale_7cn,
	&locale_15n,
	&locale_16n,
	&locale_17n,
	&locale_18n,
	&locale_18rn,
	&locale_19n,
	&locale_19an,
	&locale_19bn,
	&locale_19ln,
	&locale_19hn,
	&locale_19rn,
	&locale_20n,
	&locale_21n,
	&locale_22n,
	&locale_23n,
	&locale_24n,
	&locale_25n,
	&locale_25kn,
	&locale_26n,
	&locale_27n,
	&locale_28n,
	&locale_29n,
	&locale_29an,
	&locale_29cn,
	&locale_29bn_1,
	&locale_8bn,
	&locale_31n,
	&locale_19hn_2,
	&locale_18bn,
	&locale_27bn,
	&locale_29bn,
	&locale_mimo_all,
	&locale_3cn,
	&locale_25ln,
	&locale_27cn,
	&locale_19n_1,
	&locale_19cn,
	&locale_19ln_1,
	&locale_3an_1,
	&locale_5n_1,
	&locale_11n_1,
	&locale_8an,
	&locale_19n_2,
	&locale_11ln_2,
	&locale_3jn_1,
	&locale_3ln,
	&locale_5ln,
	&locale_11ln,
	&locale_19ln_2,
	&locale_14n,
	&locale_19ln_3,
	&locale_19hn_1,
	&locale_29dn,
	&locale_3bn,
	&locale_19ln_4,
	&locale_1cn,
	&locale_3jn_3,
	&locale_3ln_1,
	&locale_5ln_1,
	&locale_6an,
	&locale_8cn,
	&locale_11ln_4,
	&locale_13n_1,
	&locale_14an,
	&locale_16an,
	&locale_17an,
	&locale_18ln,
	&locale_9hn,
	&locale_4n,
	&locale_3dn,
	&locale_3jn_2,
	&locale_19n_3,
	&locale_3jn_4,
	&locale_3rn_3,
	&locale_5ln_2,
	&locale_6an_1,
	&locale_7cn_1,
	&locale_8an_1,
	&locale_13n_2,
	&locale_25hn,
	&locale_29dn_2,
	&locale_18n_1,
	&locale_27en,
	&locale_18n_1_3n,
	&locale_3tn,
	&locale_3sn,
	&locale_19_3n,
	&locale_3rn_4,
	&locale_29dn_1,
	&locale_29bn_3,
	&locale_29_3n,
	&locale_3n_1, /* CLM v4.6.6 */
	&locale_3en,
	&locale_3_3n
};

#endif /* WL11N */

#ifdef LC
#undef LC
#endif
#define LC(id)	LOCALE_MIMO_IDX_ ## id

#ifdef LC_2G
#undef LC_2G
#endif
#define LC_2G(id)	LOCALE_2G_IDX_ ## id

#ifdef LC_5G
#undef LC_5G
#endif
#define LC_5G(id)	LOCALE_5G_IDX_ ## id

#ifdef WL11N
#if defined(DBAND)
#define LOCALES(band2, band5, mimo2, mimo5)	{LC_2G(band2), LC_5G(band5), LC(mimo2), LC(mimo5)}
#elif defined(BAND2G)
#define LOCALES(band2, band5, mimo2, mimo5)	{LC_2G(band2), 0, LC(mimo2), 0}
#elif defined(BAND5G)
#define LOCALES(band2, band5, mimo2, mimo5)	{0, LC_5G(band5), 0, LC(mimo5)}
#else
#error "Don't know how to define LOCALES() for WL11N"
#endif
#else	/* !WL11N */
#if defined(DBAND)
#define LOCALES(band2, band5, mimo2, mimo5)	{LC_2G(band2), LC_5G(band5)}
#elif defined(BAND2G)
#define LOCALES(band2, band5, mimo2, mimo5)	{LC_2G(band2), 0}
#elif defined(BAND5G)
#define LOCALES(band2, band5, mimo2, mimo5)	{0, LC_5G(band5)}
#else
#error "Don't know how to define LOCALES()"
#endif
#endif	/* !WL11N */

static const struct {
	char abbrev[WLC_CNTRY_BUF_SZ];		/* country abbreviation */
	country_info_t country;
} cntry_locales[] = {
	{"AF",	LOCALES(b, 18,  fn, 15n)},	/* Afghanistan */
	{"AL",	LOCALES(b,  3,  bn,  3n)},	/* Albania */
	{"DZ",	LOCALES(b, 13,  fn, 15n)},	/* Algeria */
	{"AS",	LOCALES(a, 19,  an1_t1, 19n)},	/* American Samoa */
	{"AO",	LOCALES(b, 15,  fn, 15n)},	/* Angola */
	{"AI",	LOCALES(b,  3,  bn,  3n)},	/* Anguilla */
	{"AG",	LOCALES(b, 19,  bn, 19n)},	/* Antigua and Barbuda */
	{"AR",	LOCALES(b, 21,  bn, 21n)},	/* Argentina */
	{"AM",	LOCALES(b, 15,  fn, 15n)},	/* Armenia */
	{"AW",	LOCALES(b,  3,  bn,  3n)},	/* Aruba */
	{"AU",	LOCALES(h,  5,  hn,  5n)},	/* Australia */
	{"AT",	LOCALES(b,  3,  bn,  3n)},	/* Austria */
	{"AZ",	LOCALES(b, 19,  bn, 19n)},	/* Azerbaijan */
	{"BS",	LOCALES(h,  2,  fn, 15n)},	/* Bahamas */
	{"BH",	LOCALES(b,  2,  bn,  2n)},	/* Bahrain */
	{"0B",	LOCALES(a, 19, an1_t1, 19n)},	/* Baker Island */
	{"BD",	LOCALES(b,  7,  bn,  7n)},	/* Bangladesh */
	{"BB",	LOCALES(b,  2,  fn, 15n)},	/* Barbados */
	{"BY",	LOCALES(b,  3,  fn, 15n)},	/* Belarus */
	{"BE",	LOCALES(b,  3,  bn,  3n)},	/* Belgium */
	{"BZ",	LOCALES(b, 15,  fn, 15n)},	/* Belize */
	{"BJ",	LOCALES(b, 15,  fn, 15n)},	/* Benin */
	{"BM",	LOCALES(b,  3,  bn,  3n)},	/* Bermuda */
	{"BT",	LOCALES(b, 15,  fn, 15n)},	/* Bhutan */
	{"BO",	LOCALES(b, 15,  fn, 15n)},	/* Bolivia */
	{"BA",	LOCALES(b,  3,  bn,  3n)},	/* Bosnia and Herzegovina */
	{"BW",	LOCALES(b,  3,  fn, 15n)},	/* Botswana */
	{"BR",	LOCALES(b,  6,  bn,  6n)},	/* Brazil */
	{"IO",	LOCALES(b,  3,  bn,  3n)},	/* British Indian Ocean Territory */
	{"BN",	LOCALES(b, 15,  fn, 15n)},	/* Brunei Darussalam */
	{"BG",	LOCALES(b,  3,  bn,  3n)},	/* Bulgaria */
	{"BF",	LOCALES(b,  7,  fn, 15n)},	/* Burkina Faso */
	{"BI",	LOCALES(b, 15,  fn, 15n)},	/* Burundi */
	{"KH",	LOCALES(b,  6,  bn,  6n)},	/* Cambodia */
	{"CM",	LOCALES(b, 15,  fn, 15n)},	/* Cameroon */
	{"CA",	LOCALES(a,  2, an1_t1, 2n)},	/* Canada */
	{"CV",	LOCALES(b,  3,  fn, 15n)},	/* Cape Verde */
	{"KY",	LOCALES(a, 19, an1_t1, 19n)},	/* Cayman Islands */
	{"CF",	LOCALES(b, 15,  fn, 15n)},	/* Central African Republic */
	{"TD",	LOCALES(b, 15,  fn, 15n)},	/* Chad */
	{"CL",	LOCALES(b, 14,  bn, 14n)},	/* Chile */
	{"CN",	LOCALES(k, 7c,  kn1, 7cn)},	/* China */
	{"CX",	LOCALES(h,  5,  hn,  5n)},	/* Christmas Island */
	{"CO",	LOCALES(b, 19,  bn, 19n)},	/* Colombia */
	{"KM",	LOCALES(b, 15,  fn, 15n)},	/* Comoros */
	{"CG",	LOCALES(b, 15,  fn, 15n)},	/* Congo */
	{"CD",	LOCALES(b, 15,  fn, 15n)},	/* Congo, The Democratic Republic Of The */
	{"CR",	LOCALES(b, 19,  bn, 19hn)},	/* Costa Rica */
	{"CI",	LOCALES(b, 15,  fn, 15n)},	/* Cote D'ivoire */
	{"HR",	LOCALES(b,  3,  bn,  3n)},	/* Croatia */
	{"CU",	LOCALES(b,  9,  fn, 15n)},	/* Cuba */
	{"CY",	LOCALES(b,  3,  bn,  3n)},	/* Cyprus */
	{"CZ",	LOCALES(b,  3,  bn,  3n)},	/* Czech Republic */
	{"DK",	LOCALES(b,  3,  bn,  3n)},	/* Denmark */
	{"DJ",	LOCALES(b, 15,  fn, 15n)},	/* Djibouti */
	{"DM",	LOCALES(b, 19,  fn, 15n)},	/* Dominica */
	{"DO",	LOCALES(b, 19,  fn, 15n)},	/* Dominican Republic */
	{"EC",	LOCALES(b, 19,  bn, 19n)},	/* Ecuador */
	{"EG",	LOCALES(b,  5,  bn,  5n)},	/* Egypt */
	{"SV",	LOCALES(b,  1,  bn,  1n)},	/* El Salvador */
	{"GQ",	LOCALES(b, 15,  fn, 15n)},	/* Equatorial Guinea */
	{"ER",	LOCALES(b, 15,  fn, 15n)},	/* Eritrea */
	{"EE",	LOCALES(b,  3,  bn,  3n)},	/* Estonia */
	{"ET",	LOCALES(b,  3,  bn,  3n)},	/* Ethiopia */
	{"FK",	LOCALES(b,  6,  fn, 15n)},	/* Falkland Islands (Malvinas) */
	{"FO",	LOCALES(b,  3,  bn,  3n)},	/* Faroe Islands */
	{"FJ",	LOCALES(b, 15,  fn, 15n)},	/* Fiji */
	{"FI",	LOCALES(b,  3,  bn,  3n)},	/* Finland */
	{"FR",	LOCALES(b,  3,  bn,  3n)},	/* France */
	{"GF",	LOCALES(b,  3,  bn,  3n)},	/* French Guina */
	{"PF",	LOCALES(b,  3,  bn,  3n)},	/* French Polynesia */
	{"TF",	LOCALES(b,  3,  bn,  3n)},	/* French Southern Territories */
	{"GA",	LOCALES(b, 15,  fn, 15n)},	/* Gabon */
	{"GM",	LOCALES(b, 15,  fn, 15n)},	/* Gambia */
	{"GE",	LOCALES(b, 15,  bn, 15n)},	/* Georgia */
	{"GH",	LOCALES(b, 19,  bn, 19n)},	/* Ghana */
	{"GI",	LOCALES(b,  3,  fn, 15n)},	/* Gibraltar */
	{"DE",	LOCALES(b,  3,  bn,  3n)},	/* Germany */
	{"GR",	LOCALES(b,  3,  bn,  3n)},	/* Greece */
	{"GD",	LOCALES(b,  6,  bn,  6n)},	/* Grenada */
	{"GP",	LOCALES(b,  3,  bn,  3n)},	/* Guadeloupe */
	{"GU",	LOCALES(a, 19, an1_t1, 19n)},	/* Guam */
	{"GT",	LOCALES(b,  1,  fn, 15n)},	/* Guatemala */
	{"GG",	LOCALES(b,  3,  fn, 15n)},	/* Guernsey */
	{"GN",	LOCALES(b, 15,  fn, 15n)},	/* Guinea */
	{"GW",	LOCALES(b, 15,  fn, 15n)},	/* Guinea-bissau */
	{"GY",	LOCALES(b, 15,  fn, 15n)},	/* Guyana */
	{"HT",	LOCALES(b,  1,  fn, 15n)},	/* Haiti */
	{"VA",	LOCALES(b,  3,  bn,  3n)},	/* Holy See (Vatican City State) */
	{"HN",	LOCALES(b,  7,  bn,  7n)},	/* Honduras */
	{"HK",	LOCALES(h,  6,  hn,  6n)},	/* Hong Kong */
	{"HU",	LOCALES(b,  3,  bn,  3n)},	/* Hungary */
	{"IS",	LOCALES(b,  3,  bn,  3n)},	/* Iceland */
	{"IN",	LOCALES(g,  5,  gn,  5n)},	/* India */
	{"ID",	LOCALES(b, 15,  bn, 15n)},	/* Indonesia */
	{"IR",	LOCALES(f,  9,  fn, 15n)},	/* Iran, Islamic Republic Of */
	{"IQ",	LOCALES(b, 15,  fn, 15n)},	/* Iraq */
	{"IE",	LOCALES(b,  3,  bn,  3n)},	/* Ireland */
	{"IL",	LOCALES(b, 13,  bn, 13n)},	/* Israel */
	{"IT",	LOCALES(b,  3,  bn,  3n)},	/* Italy */
	{"JM",	LOCALES(b,  7,  bn,  7n)},	/* Jamaica */
	{"JP",	LOCALES(c,  4,  fn, 15n)},	/* Japan legacy definition, old 5GHz */
	{"J1",	LOCALES(c1,18,  fn, 15n)},	/* Japan_1 */
	{"J2",	LOCALES(c1,13,  fn, 15n)},	/* Japan_2 */
	{"J3",	LOCALES(c1, 3,  fn, 15n)},	/* Japan_3 */
	{"J4",	LOCALES(c1,20,  fn, 15n)},	/* Japan_4 */
	{"J5",	LOCALES(c1,22,  fn, 15n)},	/* Japan_5 */
	{"J6",	LOCALES(c1,23,  fn, 15n)},	/* Japan_6 */
	{"J7",	LOCALES(c1,24,  fn, 15n)},	/* Japan_7 */
	{"J8",	LOCALES(c1, 4,  fn, 15n)},	/* Japan_8 */
	{"J9",	LOCALES(b, 23,  cn, 23n)},	/* Japan_9 */
	{"J10",	LOCALES(c1,23,  cn, 23n)},	/* Japan_10 */
	{"JE",	LOCALES(b,  3,  fn, 15n)},	/* Jersey */
	{"JO",	LOCALES(b, 18,  bn, 18rn)},	/* Jordan */
	{"KZ",	LOCALES(b, 19,  bn, 19n)},	/* Kazakhstan */
	{"KE",	LOCALES(b, 3,  bn, 3rn)},	/* Kenya */
	{"KI",	LOCALES(b,  6,  fn, 15n)},	/* Kiribati */
	{"KR",	LOCALES(b,  9,  fn, 15n)},	/* Korea, Republic Of */
	{"KW",	LOCALES(b, 15,  bn, 15n)},	/* Kuwait */
	{"0A",	LOCALES(b, 15,  fn, 15n)},	/* Kosovo (No country code, use 0A) */
	{"KG",	LOCALES(b, 15,  fn, 15n)},	/* Kyrgyzstan */
	{"LA",	LOCALES(b,  3,  fn, 15n)},	/* Lao People's Democratic Repubic */
	{"LV",	LOCALES(b,  3,  bn,  3n)},	/* Latvia */
	{"LB",	LOCALES(b, 12,  bn, 12n)},	/* Lebanon */
	{"LS",	LOCALES(b,  3,  fn, 15n)},	/* Lesotho */
	{"LR",	LOCALES(b, 15,  fn, 15n)},	/* Liberia */
	{"LY",	LOCALES(b, 15,  fn, 15n)},	/* Libyan Arab Jamahiriya */
	{"LI",	LOCALES(b,  3,  bn,  3n)},	/* Liechtenstein */
	{"LT",	LOCALES(b,  3,  bn,  3n)},	/* Lithuania */
	{"LU",	LOCALES(b,  3,  bn,  3n)},	/* Luxembourg */
	{"MK",  LOCALES(b,  3,  bn,  3n)}, 	/* Macedonia, Former Yugoslav Republic Of */
	{"MG",	LOCALES(b, 15,  fn, 15n)},	/* Madagascar */
	{"MW",	LOCALES(b,  9,  bn,  9n)},	/* Malawi */
	{"MO",	LOCALES(b, 12,  fn, 15n)},	/* Macao */
	{"MY",	LOCALES(g,  5,  gn,  5n)},	/* Malaysia */
	{"MV",	LOCALES(b, 17,  bn, 17n)},	/* Maldives */
	{"ML",	LOCALES(b, 15,  fn, 15n)},	/* Mali */
	{"MT",	LOCALES(b,  3,  bn,  3n)},	/* Malta */
	{"IM",	LOCALES(b,  3,  fn, 15n)},	/* Man, Isle Of */
	{"MQ",	LOCALES(b,  3,  bn,  3n)},	/* Martinique */
	{"MR",	LOCALES(b,  3,  bn,  3n)},	/* Mauritania */
	{"MU",	LOCALES(b,  3,  bn,  3n)},	/* Mauritius */
	{"YT",	LOCALES(b,  3,  bn,  3n)},	/* Mayotte */
	{"MX",	LOCALES(b, 13,  bn, 13n)},	/* Mexico */
	{"FM",	LOCALES(a, 19,  fn, 15n)},	/* Micronesia, Federated States Of */
	{"MD",	LOCALES(b,  3,  bn,  3n)},	/* Moldova, Republic Of */
	{"MC",	LOCALES(b,  3,  bn,  3n)},	/* Monaco */
	{"MN",	LOCALES(b, 15,  fn, 15n)},	/* Mongolia */
	{"ME",	LOCALES(b,  3,  bn,  3n)},	/* Montenegro */
	{"MS",	LOCALES(b,  3,  bn,  3n)},	/* Montserrat */
	{"MA",	LOCALES(b, 18,  en, 15n)},	/* Morocco */
	{"MZ",	LOCALES(b, 19,  bn, 19n)},	/* Mozambique */
	{"MM",	LOCALES(b, 15,  fn, 15n)},	/* Myanmar */
	{"NA",	LOCALES(b, 19,  bn, 19n)},	/* Namibia */
	{"NR",	LOCALES(b, 15,  fn, 15n)},	/* Nauru */
	{"NP",	LOCALES(g, 16,  gn, 16n)},	/* Nepal */
	{"NL",	LOCALES(b,  3,  bn,  3n)},	/* Netherlands */
	{"AN",	LOCALES(b,  3,  bn,  3n)},	/* Netherlands Antilles */
	{"NC",	LOCALES(b, 15,  fn, 15n)},	/* New Caledonia */
	{"NZ",	LOCALES(b,  6,  bn,  6n)},	/* New Zealand */
	{"NI",	LOCALES(b,  6,  bn,  6n)},	/* Nicaragua */
	{"NE",	LOCALES(b, 19,  fn, 15n)},	/* Niger */
	{"NG",	LOCALES(b, 8a,  fn, 15n)},	/* Nigeria */
	{"NU",	LOCALES(b, 19,  fn, 15n)},	/* Niue */
	{"NF",	LOCALES(h,  5,  hn,  5n)},	/* Norfolk Island */
	{"MP",	LOCALES(a, 19,  fn, 15n)},	/* Northern Mariana Islands */
	{"NO",	LOCALES(b,  3,  bn,  3n)},	/* Norway */
	{"OM",	LOCALES(b,  3,  bn,  3n)},	/* Oman */
	{"PK",	LOCALES(b,  7,  bn,  7n)},	/* Pakistan */
	{"PW",	LOCALES(b,  6,  fn, 15n)},	/* Palau */
	{"PA",	LOCALES(b,  1,  bn,  1n)},	/* Panama */
	{"PG",	LOCALES(b,  1,  bn,  1n)},	/* Papua New Guinea */
	{"PY",	LOCALES(b,  7,  bn,  7n)},	/* Paraguay */
	{"PE",	LOCALES(b, 19,  bn, 19n)},	/* Peru */
	{"PH",	LOCALES(b,  6,  bn,  6n)},	/* Philippines */
	{"PL",	LOCALES(b,  3,  bn,  3n)},	/* Poland */
	{"PT",	LOCALES(b,  3,  bn,  3n)},	/* Portugal */
	{"PR",	LOCALES(a, 19, an1_t1, 19n)},	/* Pueto Rico */
	{"QA",	LOCALES(b, 12,  bn, 12n)},	/* Qatar */
	{"RE",	LOCALES(b,  3,  bn,  3n)},	/* Reunion */
	{"RO",	LOCALES(b,  3,  bn,  3n)},	/* Romania */
	{"RW",  LOCALES(b,  7,  bn,  7n)},      /* Rwanda */
	{"KN",	LOCALES(b,  7,  fn, 15n)},	/* Saint Kitts and Nevis */
	{"LC",	LOCALES(b, 15,  fn, 15n)},	/* Saint Lucia */
	{"MF",	LOCALES(b, 15,  fn, 15n)},	/* Sanit Martin / Sint Marteen */
	{"RU",	LOCALES(b, 15,  fn, 15n)},	/* Russian Federation */
	{"PM",	LOCALES(b, 13,  fn, 15n)},	/* Saint Pierre and Miquelon */
	{"VC",  LOCALES(b, 19,  fn, 15n)},      /* Saint Vincent and The Grenadines */
	{"WS",	LOCALES(b, 15,  fn, 15n)},	/* Samoa */
	{"SM",	LOCALES(b,  3,  bn,  3n)},	/* San Marino */
	{"ST",	LOCALES(b, 15,  fn, 15n)},	/* Sao Tome and Principe */
	{"SA",	LOCALES(b, 16,  bn, 16n)},	/* Saudi Arabia */
	{"SN",	LOCALES(b,  3,  bn,  3n)},	/* Senegal */
	{"RS",	LOCALES(b,  3,  bn,  3n)},	/* Serbia */
	{"SC",	LOCALES(b, 15,  fn, 15n)},	/* Seychelles */
	{"SL",	LOCALES(b, 15,  fn, 15n)},	/* Sierra Leone */
	{"SG",	LOCALES(g,  5,  gn,  5n)},	/* Singapore */
	{"SK",	LOCALES(b,  3,  bn,  3n)},	/* Slovakia */
	{"SI",	LOCALES(b,  3,  bn,  3n)},	/* Slovenia */
	{"SB",	LOCALES(b, 15,  fn, 15n)},	/* Solomon Islands */
	{"SO",	LOCALES(b, 15,  fn, 15n)},	/* Somalia */
	{"ZA",	LOCALES(b,  3,  bn,  3n)},	/* South Africa */
	{"ES",	LOCALES(b,  3,  bn,  3n)},	/* Spain */
	{"LK",	LOCALES(g,  6,  gn,  6n)},	/* Sri Lanka */
	{"SR",	LOCALES(b, 15,  fn, 15n)},	/* Suriname */
	{"SZ",	LOCALES(b, 15,  fn, 15n)},	/* Swaziland */
	{"SE",	LOCALES(b,  3,  bn,  3n)},	/* Sweden */
	{"CH",	LOCALES(b,  3,  bn,  3n)},	/* Switzerland */
	{"SY",	LOCALES(f, 12,  fn, 15n)},	/* Syrian Arab Republic */
	{"TW",	LOCALES(a,  8, an1_t1,  8n)},	/* Taiwan, Province Of China */
	{"TJ",	LOCALES(b, 15,  fn, 15n)},	/* Tajikistan */
	{"TZ",	LOCALES(b, 12,  fn, 15n)},	/* Tanzania, United Republic Of */
	{"TH",	LOCALES(b,  6,  bn,  6n)},	/* Thailand */
	{"TG",	LOCALES(b, 15,  fn, 15n)},	/* Togo */
	{"TO",	LOCALES(b, 15,  fn, 15n)},	/* Tonga */
	{"TT",	LOCALES(b,  6,  bn,  6n)},	/* Trinidad and Tobago */
	{"TN",	LOCALES(b, 13,  bn, 13n)},	/* Tunisia */
	{"TR",	LOCALES(b, 13,  bn, 13n)},	/* Turkey */
	{"TM",	LOCALES(b, 15,  fn, 15n)},	/* Turkmenistan */
	{"TC",	LOCALES(b,  3,  bn,  3n)},	/* Turks and Caicos Islands */
	{"TV",	LOCALES(b, 15,  fn, 15n)},	/* Tuvalu */
	{"UG",	LOCALES(h,  3,  fn, 15n)},	/* Uganda */
	{"UA",	LOCALES(b, 15,  fn, 15n)},	/* Ukraine */
	{"AE",	LOCALES(b, 15,  bn, 15n)},	/* United Arab Emirates */
	{"GB",	LOCALES(b,  3,  bn,  3n)},	/* United Kingdom */
	{"US",	LOCALES(a,  1, an1_t1,  1n)},	/* United States */
	{"Q2",	LOCALES(a, 27, an1_t1, 27n)},	/* United States (No DFS) */
	{"UM",	LOCALES(a, 19, an1_t1, 19n)},	/* United States Minor Outlying Islands */
	{"UY",	LOCALES(h,  6,  hn,  6n)},	/* Uruguay */
	{"UZ",	LOCALES(b, 15,  fn, 15n)},	/* Uzbekistan */
	{"VU",	LOCALES(b, 15,  fn, 15n)},	/* Vanuatu */
	{"VE",	LOCALES(b,  5,  bn,  5n)},	/* Venezuela */
	{"VN",	LOCALES(b,  13,  bn,  13n)},	/* Viet Nam */
	{"VG",	LOCALES(b,  3,  bn,  3n)},	/* Virgin Islands, British */
	{"VI",	LOCALES(a, 19, an1_t1, 19n)},	/* Virgin Islands, U.S. */
	{"WF",	LOCALES(b,  3,  bn,  3n)},	/* Wallis and Futuna */
	{"0C",	LOCALES(b, 13,  bn, 13n)},	/* West Bank */
	{"EH",	LOCALES(b, 18,  en, 15n)},	/* Western Sahara */
	{"YE",	LOCALES(b, 15,  fn, 15n)},	/* Yemen */
	{"ZM",	LOCALES(g,  3,  fn, 15n)},	/* Zambia */
	{"ZW",	LOCALES(b, 19,  bn, 19n)},	/* Zimbabwe */
	{"Z2",	LOCALES(a, 19,  fn, 15n)},	/* Country Z2 */
	{"XA",	LOCALES(b, 11,  fn, 15n)},	/* Europe / APAC 2005 */
	{"XB",	LOCALES(a, 1r,  fn, 15n)},	/* North and South America and Taiwan */
	{"X0",	LOCALES(a1,1a, an1_t1, 1an)},	/* FCC Worldwide */
	{"X1",	LOCALES(b,  5,  bn,  5n)},	/* Worldwide APAC */
	{"X2",	LOCALES(i, 11,  bn, 11n)},	/* Worldwide RoW 2 */
	{"X3",	LOCALES(b,  3,  en, 3an)},	/* ETSI */
	{"EU",	LOCALES(b, 18,  bn, 18rn)},	/* European Union */
	{"XS",  LOCALES(b2_6, 11_4, bn2_8, 11ln_3)}, /* Worldwide Safe Enhanced Mode Locale */
	{"XW",	LOCALES(j, 31,  jn, 31n)},	/* Worldwide Locale for Linux driver */
	{"XX",	LOCALES(b2, 3,  fn, 15n)},	/* Worldwide Locale (passive Ch12-14) */
	{"XY",	LOCALES(b,  3,  bn,  3n)},	/* Fake Country Code */
	{"XZ",	LOCALES(b5, 15, fn, 15n)},	/* Worldwide Locale (passive Ch12-14) */
	{"XU",	LOCALES(b_5, 3c, bn_2, 3cn)},	/* European Locale 0dBi antenna in 2.4GHz */
	{"XV",  LOCALES(b2_1, 15, bn2_1, 15n)}, /* Worldwide Safe Mode Locale (passive Ch12-14) */
	/* following entries are for old or internal locales */
#if defined(BCMDBG) || defined(WLTEST)
	{"ALL",	LOCALES(2G_band_all, 5G_band_all, mimo_2g_all, mimo_5g_all)},
#endif 
#ifdef BCMDBG
	{"RDR",	LOCALES(f, 5G_radar_only, mimo_2g_all, mimo_5g_all)},
#endif /* BCMDBG */
#ifdef WL11D
	{"B2",	LOCALES(11d_2G, 11d_5G, fn, 15n)}
#endif
};

const struct {
	char abbrev[3];		/* country abbreviation */
	uint8 rev;		/* regulatory revision */
	country_info_t country;
} cntry_rev_locales[] = {
	{"AR",	1, LOCALES(b,   19,  bn,  19n)},	/* Argentina */
	{"AR",	2, LOCALES(b,   19a_1,  fn,  15n)},	/* Argentina */
	{"AR",  4, LOCALES(b,   19l_2,  bn1_1,  19ln_4)},        /* Argentina */
	{"AU",  1, LOCALES(b,   5a,  fn,  15n)},	/* Australia */
	{"AU",  2, LOCALES(h,   5a,  hn,  5an)},	/* Australia */
	{"AT",  1, LOCALES(b,   3c,  bn,  3cn)},	/* Austria */
	{"AT",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Austria */
	{"BE",  1, LOCALES(b,   3c,  bn,  3cn)},	/* Belgium */
	{"BE",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Belgium */
	{"BN",  1, LOCALES(g,    5,  gn,   5n)},	/* Brunei Darussalam */
	{"BN",  2, LOCALES(b,    5,  fn,   15n)},	/* Brunei Darussalam */
	{"BG",  1, LOCALES(b,   3c,  bn,  3cn)},	/* Bulgaria */
	{"BG",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Bulgaria */
	{"BY",  1, LOCALES(b,  3l_1,  fn, 15n)},      /* Belarus */
	{"BR",	1, LOCALES(b,  6a,  bn1_1,  6an)},	/* Brazil */
	{"CA",  2, LOCALES(a,   29, an1_t2, 29n)},	/* Canada */
	{"CA",  3, LOCALES(a,  29a, an1_t2, 29an)},	/* Canada */
	{"CA",  4, LOCALES(a,  29c, an1_t2, 29cn)},	/* Canada */
	{"CA",  5, LOCALES(a4, 29b, an5, 29bn)},	/* Canada */
	{"CA",  6, LOCALES(a11, 19a_1, fn, 15n)},	/* Canada */
	{"CA",  7, LOCALES(a4, 29d, an5, 29dn)},	/* Canada */
	{"CA",  11, LOCALES(a3_1, 19l_2, an1_t5, 19ln_4)},	/* Canada */
	{"CN",  1, LOCALES(k_1,   7c,  kn1,  7cn)},	/* China */
	{"CN",	3, LOCALES(c_4, 7c,  bn1_1, 7cn)},	/* China */
	{"CN",  4, LOCALES(k_3,   7c,  kn4,  7cn)},	/* China */
	{"CN",  5, LOCALES(k_4,   7c_1,  kn5,  7cn_1)},	/* China */
	{"HR",	1, LOCALES(b,   3c,  bn,  3cn)},	/* Croatia */
	{"HR",	2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Croatia */
	{"CY",  1, LOCALES(b,   3c,  bn,  3cn)},	/* Cyprus */
	{"CY",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Cyprus */
	{"CZ",  1, LOCALES(b,   3c,  bn,  3cn)},	/* Czech Republic */
	{"CZ",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Czech Republic */
	{"CL",	2, LOCALES(b, 14a,  bn1_1, 15n)},	/* Chile */
	{"CO",	1, LOCALES(b, 18l,  bn1_1, 18ln)},	/* Colombia */
	{"CR",	1, LOCALES(b, 19l_2,  bn1_1, 19ln_4)},	/* Costa Rica */
	{"DK",  1, LOCALES(b,   3c,  bn,  3cn)},	/* Denmark */
	{"DK",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Denmark */
	{"DE",	2, LOCALES(b,    3,  bn,  3n)},	/* Germany */
	{"DE",	3, LOCALES(b,    3c, bn, 3cn)},	/* Germany */
	{"DE",	4, LOCALES(b_11, 3d, bn_9, 3dn)},	/* Germany Apple X21 */
	{"DE",	5, LOCALES(b,    3l_1, bn1_1, 3ln_1)},	/* Germany */
	{"EC",	1, LOCALES(b, 19l_2,  bn1_1, 19ln_4)},	/* Ecuador */
	{"SV",	1, LOCALES(b,  1c,  bn1_1,  1cn)},	/* El Salvador */
	{"EE",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Estonia */
	{"EE",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Estonia */
	{"EU",	2, LOCALES(b,  3r,  bn, 3rn)},	/* Europeanwide AP (notch out weather band) */
	{"EU",	3, LOCALES(b4, 15, bn4, 15n)},	/* Europeanwide AP (notch out weather band) */
	{"EU",	4, LOCALES(b, 15, bn4, 15n)},	/* Europeanwide AP (Netgear WNR3500 v2) */
	{"EU",	5, LOCALES(b_9, 3b, bn_1, 3bn)}, /* Europeanwide AP (Netgear WND3400) */
	{"EU",	9, LOCALES(b_3, 18_2, bn_10, 18n_1)}, /* Europeanwide AP (Cisco Linksys E4200) */
	{"EU",	10, LOCALES(b_11, 15, fn, 15n)}, /* Europeanwide AP (HP Bianca) */
	{"EU",	11, LOCALES(b_11, 3r_4, kn5, 3rn_3)}, /* Europeanwide AP (Avaya AP 8120) */
	{"EU",	12, LOCALES(b, 3s, bn7, 3sn)}, /* Europeanwide AP (for Airties) */
	{"EU",	13, LOCALES(b, 3r_5, bn7, 3rn_4)}, /* Europeanwide AP (high power) */
	{"EU",	14, LOCALES(f, 3_1, fn, 3n_1)},	/* CLM v4.6.6 */
	{"FI",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Finland */
	{"FI",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Finland */
	{"FR",  1, LOCALES(b, 3c,  bn, 3cn)},	/* France */
	{"FR",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* France */
	{"GR",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Greece */
	{"ID",	1, LOCALES(b, 9h,  bn, 9hn)},	/* Indonesia */
	{"GR",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Greece */
	{"HU",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Hungary */
	{"HU",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Hungary */
	{"IS",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Iceland */
	{"IS",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Iceland */
	{"IN",  1, LOCALES(b, 5,  fn, 15n)},	/* India */
	{"ID",	2, LOCALES(b, 9h,  bn1_1, 9hn)},	/* Indonesia */
	{"IE",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Ireland */
	{"IE",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Ireland */
	{"IL",	2, LOCALES(b, 13_1,  bn1_1, 13n_1)},	/* Israel */
	{"IL",  3, LOCALES(b_11, 13_2,  kn5, 13n_2)},	/* Israel */
	{"IL",  4, LOCALES(b, 13_3,  bn_5, 13n_1)},	/* Israel (Juniper AX411) */
	{"IT",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Italy */
	{"IT",	2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Italy */
	{"JP",  1, LOCALES(c, 23,  cn, 23n)},	/* Japan */
	{"JP",  2, LOCALES(c, 24,  cn, 24n)},	/* Japan */
	{"JP",  3, LOCALES(c,  3,  cn, 3jn)},	/* Japan */
	{"JP",  4, LOCALES(c,  3,  en, 3jn)},	/* Japan */
	{"JP",  5, LOCALES(b,  3,  bn, 3jn)},	/* Japan */
	{"JP",  6, LOCALES(c_1, 3, cn, 3jn)},	/* Japan */
	{"JP",  7, LOCALES(c_1, 3j, cn, 3jn_1)}, /* Japan */
	{"JP",  8, LOCALES(c_2, 3j_1, fn, 15n)}, /* Japan */
	{"JP",  9, LOCALES(c, 3j_8, cn_1, 3jn)}, /* Japan */
	{"JP", 11, LOCALES(b_11, 3j_3, cn_2, 3jn_2)}, /* Japan */
	{"JP",  12, LOCALES(c_4, 3j_4, bn1_1, 3jn_3)}, /* Japan */
	{"JP",  13, LOCALES(c_4, 3j_4, bn1_1, 3jn_3)}, /* Japan */
	{"JP",  14, LOCALES(c_5, 3j_5, cn_3, 3jn_4)}, /* Japan */
	{"KR",  1, LOCALES(b, 25,  fn, 15n)},	/* Korea, Republic Of (RegRev1) */
	{"KR",  2, LOCALES(k, 25, en2, 25n)},	/* Korea, Republic Of (RegRev2) */
	{"KR",	3, LOCALES(k, 25,  kn, 25kn)},	/* Korea, Republic Of (RegRev3) */
	{"KR",	4, LOCALES(k, 25l, kn1, 25ln)},	/* Korea, Republic Of (RegRev4) */
	{"KR",	5, LOCALES(k, 25l, kn1, 25ln)},	/* Korea, Republic Of (RegRev5) */
	{"KR",	6, LOCALES(k, 25l, kn1, 25ln)},	/* Korea, Republic Of (RegRev6) */
	{"KR",	7, LOCALES(k, 25l, kn2, 25kn)},	/* Korea, Republic Of (RegRev7) */
	{"KR",	8, LOCALES(k_1, 25l, kn1, 25ln)}, /* Korea, Republic Of (RegRev8) */
	{"KR",	10, LOCALES(c_4, 25l, bn1_1, 25ln)},	/* Korea, Republic Of (RegRev10) */
	{"KR",	11, LOCALES(c_4, 25l, bn1_1, 25ln)},	/* Korea, Republic Of (RegRev11) */
	{"KR",	12, LOCALES(k_4, 25h, kn5, 25hn)}, /* Korea, Republic Of (RegRev12) */
	{"KW",	1, LOCALES(b, 13,  bn, 13n)},	/* Kuwait */
	{"KW",	3, LOCALES(b, 13_1,  bn1_1, 13n_1)},	/* Kuwait */
	{"LV",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Latvia */
	{"LV",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Latvia */
	{"LI",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Liechtenstein */
	{"LI",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Liechtenstein */
	{"LT",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Lithuania */
	{"LT",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Lithuania */
	{"LU",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Luxembourg */
	{"LU",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Luxembourg */
	{"MV",	1, LOCALES(b, 17a,  bn1_1, 17an)},	/* Maldives */
	{"MY",  1, LOCALES(b, 5,  fn, 15n)},	/* Malaysia */
	{"MT",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Malta */
	{"MT",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Malta */
	{"MX",  1, LOCALES(b,  2,  bn,  2n)},	/* Mexico */
	{"MX",  2, LOCALES(b,  1c,  bn1_1,  1cn)},	/* Mexico */
	{"MA",  1, LOCALES(b,  3,  en, 26n)},	/* Morocco */
	{"NP",  1, LOCALES(b,  16,  fn, 15n)},	/* Nepal */
	{"NL",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Netherlands */
	{"NL",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Netherlands */
	{"NO",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Norway */
	{"NO",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Norway */
	{"NZ",  1, LOCALES(a_6, 6a_1,  an6_6, 6an_1)},	/* New Zeland - Cisco AP3500i */
	{"PY",  1, LOCALES(b,  6,  bn,  6n)},   /* Paraguay */
	{"PK",	1, LOCALES(b,  7,  bn1_1,  7n)},	/* Pakistan */
	{"PA",	1, LOCALES(g,  1c,  gn,  1cn)},	/* Panama */
	{"PE",	2, LOCALES(b, 19l_2,  bn1_1, 19ln_4)},	/* Peru */
	{"PH",	2, LOCALES(b,  6a,  bn1_1,  6an)},	/* Philippines */
	{"PL",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Poland */
	{"PL",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Poland */
	{"PT",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Portugal */
	{"PT",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Portugal */
	{"PR",	2, LOCALES(a3_1, 19l_2, an1_t5, 19ln_4)},	/* Pueto Rico */
	{"Q1",	17, LOCALES(a20, 15, an2_21, 15n)},	/* United States */
	{"Q2",	2, LOCALES(a4, 27b, an5, 27bn)},	/* United States (No DFS) */
	{"Q2",  3, LOCALES(a4, 27c, an5, 27cn)},        /* United States (No DFS) */
	{"Q2",  4, LOCALES(a4_2, 27e, an6_4, 27en)},    /* United States (No DFS) */
	{"RO",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Romania */
	{"RO",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Romania */
	{"RU",  1, LOCALES(b,  5,  fn, 15n)},	/* Russian Federation */
	{"RU",  2, LOCALES(b, 19,  fn, 15n)},   /* Russian Federation */
	{"RU",  3, LOCALES(b, 19l_2,  dn, 4n)},   /* Russian Federation */
	{"RU",  4, LOCALES(b_11, 2,  kn5, 2n)},   /* Russian Federation */
	{"SA",	2, LOCALES(b, 15,  bn1_1, 15n)},	/* Saudi Arabia */
	{"SG",	1, LOCALES(b_2, 15,  bn_2, 15n)},	/* Singapore */
	{"SG",	2, LOCALES(b, 5,  fn, 15n)},	/* Singapore */
	{"SG",	3, LOCALES(k_4, 5l_2,  kn5, 5ln_2)},	/* Singapore */
	{"SK",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Slovakia */
	{"SK",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Slovakia */
	{"SI",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Slovenia */
	{"SI",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Slovenia */
	{"ES",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Spain */
	{"ES",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Spain */
	{"ZA",	2, LOCALES(b,  3l_1,  bn1_1,  3ln_1)},	/* South Africa */
	{"SE",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Sweden */
	{"SE",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Sweden */
	{"CH",  1, LOCALES(b, 3c,  bn, 3cn)},	/* Switzerland */
	{"CH",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* Switzerland */
	{"CH",  9, LOCALES(b_1, 15,  bn_1, 15n)},	/* Switzerland */
	{"TW",  2, LOCALES(a, 8a, an1_t1, 8an)},	/* Taiwan, Province Of China */
	{"TW",  3, LOCALES(a, 8a, an1_t1, 8bn)},	/* Taiwan, Province Of China */
	{"TW",  4, LOCALES(a3_1, 8b, an1_t5, 8cn)},	/* Taiwan, Province Of China */
	{"TW",  5, LOCALES(a_6, 8a_1, an6_6, 8an_1)},	/* Taiwan, Province Of China */
	{"TH",	2, LOCALES(b,  6a,  bn1_1,  6an)},	/* Thailand */
	{"TR",	3, LOCALES(b, 13_1,  bn1_1, 13n_1)},	/* Turkey */
	{"TR",  4, LOCALES(b, 3, bn7, 3tn)},	/* Turkey (For Airties) */
	{"UA",  1, LOCALES(b, 13,  fn, 15n)},	/* Ukraine */
	{"UA",  2, LOCALES(b,  5,  en,  5n)},   /* Ukraine */
	{"UA",  3, LOCALES(b,  5,  bn1_1,  5n)},   /* Ukraine */
	{"AE",  1, LOCALES(b,  3,  bn,  3n)},	/* United Arab Emirates */
	{"AE",  3, LOCALES(b,  3l_1,  bn1_1,  3ln_1)},	/* United Arab Emirates */
	{"GB",  1, LOCALES(b, 3c,  bn, 3cn)},	/* United Kingdom */
	{"GB",  2, LOCALES(b, 3l_1,  bn1_1, 3ln_1)},	/* United Kingdom */
	{"GB",  6, LOCALES(b, 3,  bn7, 3en)},	/* United Kingdom */
	{"US",  2, LOCALES(a,    1,    an2,   1n)},	/* United States */
	{"US",  3, LOCALES(a,    1,    an3,   1n)},	/* United States */
	{"US",  4, LOCALES(a,    1,    an4,   1n)},	/* United States */
	{"US",  5, LOCALES(a,   19, an1_t1,  19n)},	/* United States */
	{"US",  6, LOCALES(a,   19,    an2,  19n)},	/* United States */
	{"US",  7, LOCALES(a,   19,    an3,  19n)},	/* United States */
	{"US",  8, LOCALES(a,   19,    an4,  19n)},	/* United States */
	{"US",  9, LOCALES(a,   28, an1_t1,  28n)},	/* United States (Low 5G Band only) */
	{"US", 10, LOCALES(a1,  19, an1_t2, 19hn)},	/* United States */
	{"US", 11, LOCALES(a1, 19b,    an4, 19bn)},	/* United States */
	{"US", 12, LOCALES(a2,   1, an1_t1,   1n)},	/* United States */
	{"US", 13, LOCALES(a5,  15,    an6,  15n)},	/* United States */
	{"US", 14, LOCALES(a6,  15,    an7,  15n)},	/* United States */
	{"US", 15, LOCALES(a3,  19,    an2,  19n)},	/* United States */
	{"US", 16, LOCALES(a7,  19, an8_t1, 19cn)},	/* United States */
	{"US", 17, LOCALES(b2_2,  19, bn2_3,  19n)},     /* United States */
	{"US", 18, LOCALES(a9,  15,  an6_2,  15n)},	/* United States */
	{"US", 19, LOCALES(a6,  19, an8_t2,  19cn)},     /* United States */
	{"US", 20, LOCALES(a3, 19_1, an1_t3, 19n_1)},	/* United States */
	{"US", 21, LOCALES(a1,  15, an1_t4,  15n)},	/* United States */
	{"US", 22, LOCALES(a_1, 15, fn, 15n)},   /* United States */
	{"US", 23, LOCALES(a6_1, 15, an6_1,  15n)},     /* United States */
	{"US", 24, LOCALES(a4, 29b_1, an5,  29bn_1)},     /* United States */
	{"US", 25, LOCALES(b2_3, 15, bn2_4,  15n)},     /* United States */
	{"US", 26, LOCALES(a10_1, 15, an2_1,  15n)},     /* United States */
	{"US", 27, LOCALES(f, 19_2, fn,  19n_2)},     /* United States */
	{"US", 28, LOCALES(a6_2, 15, an7_2,  15n)},     /* United States */
	{"US", 29, LOCALES(a3_1, 19l_2, an1_t5, 19ln_2)},    /* United States */
	{"US", 30, LOCALES(b2_5, 15, bn2_4, 15n)},    /* United States */
	{"US", 31, LOCALES(a10_2, 15, an2_2, 15n)},    /* United States */
	{"US", 32, LOCALES(a10_3, 15, an2_3, 15n)},    /* United States */
	{"US", 33, LOCALES(a_2, 15, an_2, 15n)},    /* United States */
	{"US", 34, LOCALES(a11, 19a_1, fn, 15n)},    /* United States */
	{"US", 35, LOCALES(a_3, 19_3, an2_4, 19ln_3)},    /* United States */
	{"US", 36, LOCALES(a, 19h_1, an1_t1,  19hn_1)},     /* United States */
	{"US", 37, LOCALES(a12, 15, an10,  15n)},     /* United States */
	{"US", 38, LOCALES(a5_1, 15, an6_3, 15n)},    /* United States */
	{"US", 39, LOCALES(a6_3, 19h_2, an7_3,  19hn_2)},     /* United States */
	{"VN",	1, LOCALES(b,  13,  bn1_1,  13n)},	/* Viet Nam regrev-1 */
	{"US", 45, LOCALES(a, 19h_2, an1_t1, 19hn_2)},   /* United States */
	{"US", 47, LOCALES(a4, 29d_1, an5, 29dn_1)},   /* United States */
	{"US", 49, LOCALES(a_5, 19_4, an2_5,  19n_3)},     /* United States */
	{"US", 50, LOCALES(a3_1, 19l_2, an1_t5, 19ln_4)},    /* United States */
	{"US", 53, LOCALES(a3_1, 19l_2, an1_t5, 19ln_2)},    /* United States */
	{"US", 54, LOCALES(a14, 15, fn, 15n)},    /* United States */
	{"US", 55, LOCALES(a4_3, 15, an6_5,  15n)},     /* United States */
	{"US", 56, LOCALES(a_6, 29d_2, an6_6,  29dn_2)},     /* United States */
	{"US", 61, LOCALES(a3_1, 19l_2, an1_t5, 19ln_2)},    /* United States */
	{"US", 63, LOCALES(a6_8, 15, an7_8,  15n)},     /* United States */
	{"US", 72, LOCALES(a3_1, 19l_2, an6_4,  19ln_2)},     /* United States */
	{"US", 73, LOCALES(a4_4, 29b_3, an6_7,  29bn_3)},     /* United States */
	{"VE",	1, LOCALES(g,  5,  gn,  5n)},	/* Venezuela */
	{"VN",	1, LOCALES(b,  5,  bn1_1,  5n)},	/* Viet Nam */
	{"XA",  1, LOCALES(b,  3,  en, 3an)},	/* Europe / APAC == X3 for bad SROM for 4321 */
	{"X0",  2, LOCALES(a1, 19r, an1_t1, 19rn)}, /* FCC Worldwide */
	{"X0",  3, LOCALES(a1, 19a, an1_t1, 19an)}, /* FCC Worldwide */
	{"X0",  4, LOCALES(a3, 19a, an1_t1, 19ln)}, /* FCC Worldwide */
	{"X0",  5, LOCALES(a3_1, 19l_1, an1_t5, 19ln_1)}, /* FCC Worldwide */
	{"X0",  6, LOCALES(a3_1, 19l_2, an1_t5, 19ln_2)}, /* FCC Worldwide */
	{"X0",  7, LOCALES(a3_1, 19l_2, an1_t5, 19ln_4)}, /* FCC Worldwide */
	{"X0",  8, LOCALES(a3_1, 19l_2, an1_t5, 19ln_4)}, /* FCC Worldwide */
	{"X1",  2, LOCALES(b,  5, bn1,  5n)},	/* Worldwide APAC */
	{"X1",  3, LOCALES(b,  5, bn2,  5n)},	/* Worldwide APAC */
	{"X1",  4, LOCALES(b3_1, 5, bn2_2, 5n_1)},	/* Worldwide APAC */
	{"X1",  5, LOCALES(b3_1, 5l, bn2_2, 5ln)},	/* Worldwide APAC */
	{"X1",  6, LOCALES(b3_1, 5l_1, bn1_1, 5ln_1)},	/* Worldwide APAC */
	{"X1",  7, LOCALES(b3_1, 5l_1, bn1_1, 5ln_1)},	/* Worldwide APAC */
	{"X2",  2, LOCALES(i, 11, bn1, 11n)},	/* Worldwide RoW 2 */
	{"X2",  3, LOCALES(i, 11, bn2, 11n)},	/* Worldwide RoW 2 */
	{"X2",  4, LOCALES(i_1, 11, bn2_2, 11n)},	/* Worldwide RoW 2 */
	{"X2",  5, LOCALES(i_1, 11l, bn2_2, 11ln)},	/* Worldwide RoW 2 */
	{"X2",  6, LOCALES(i, 11l_1, bn1_1, 11ln_4)},	/* Worldwide RoW 2 */
	{"X2",  7, LOCALES(i, 11l_1, bn1_1, 11ln_4)},	/* Worldwide RoW 2 */
	{"X3",  1, LOCALES(b,  3,  bn, 3an_1)},	/* ETSI */
	{"X3",  2, LOCALES(b,  3,  bn1, 3an_1)},	/* ETSI */
	{"X3",  3, LOCALES(b3,  3, bn2, 3an_1)},	/* ETSI */
	{"X3",  4, LOCALES(b3_1,  3, bn2_2, 3an_1)},	/* ETSI */
	{"X3",  5, LOCALES(b3_1, 3l, bn2_2, 3ln)},	/* ETSI */
	{"X3",  6, LOCALES(b, 3l_1, bn1_1, 3ln_1)},	/* ETSI */
	{"X3",  7, LOCALES(b, 3l_1, bn1_1, 3ln_1)},	/* ETSI */
	{"XX",	1, LOCALES(b5,   15,  fn, 15n)},    /* Worldwide (passive Ch12-14), no MIMO */
	{"XX",	2, LOCALES(b5_2, 15,  bn5_1, 15n)}, /* Worldwide (passive Ch12-14) */
	{"XX",	3, LOCALES(b5_4, 15,  fn, 15n)}, /* Worldwide (passive Ch12-14) */
	{"XZ",  1, LOCALES(b2_4, 11_1, bn2_5, 11n_1)}, /* Worldwide (passive Ch12-14), passive 5G */
	{"XZ",  2, LOCALES(b5_3, 11_2, fn, 15n)}, /* Worldwide (passive Ch12-14), passive 5G */
	{"XY",  1, LOCALES(b_3, 15, bn2_2, 15n)}, /* Worldwide (passive Ch12-14), passive 5G */
	{"XY",  2, LOCALES(b_4, 15, bn2_6, 15n)}, /* Worldwide (passive Ch12-14), passive 5G */
	{"XU",  1, LOCALES(b_6, 3c, bn_3, 3cn)}, /* European locale, 1dBi antenna in 2.4GHz */
	{"XU",  2, LOCALES(b_7, 3c, bn_4, 3cn)}, /* European locale, 2dBi antenna in 2.4GHz */
	{"XU",  3, LOCALES(b_8, 3c, bn_5, 3cn)}, /* European locale, 3dBi antenna in 2.4GHz */
	{"XV",  1, LOCALES(b2_6, 11_3, bn2_8, 11ln_2)}, /* WW Safe Mode Locale */
	{"XT",  4, LOCALES(b2_5, 15, bn2_4, 15n)}, /* For BCM94319SDB  */
	{"XT",  5, LOCALES(b2_5, 15, bn2_4, 15n)}, /* For BCM94319SDHMB  */
	{"ww",  5, LOCALES(a_2, 15, an_2, 15n)}, /* For BCM94319USBWLN4L (HP Bellatrix) */
	{"ww",  6, LOCALES(a14, 15, fn, 15n)}, /* For BCM94336SDGP HP Bianca */
};

/* generic country with no channels */
static const country_info_t country_nochannel_locale =
	LOCALES(f, 15, fn, 15n);

/* these countries use country_nochannel_locale, allowing no 802.11 channel/operation */
static const char country_nochannel[][WLC_CNTRY_BUF_SZ] = {
	"AD",	/* Andorra */
	"AQ",	/* Antarctica */
	"AC",	/* Ascension Island */
	"Z1",	/* Ashmore and Cartier Islands */
	"BV",	/* Bouvet Island */
		/* Channel Islands (Z1 duplicate) */
	"CP",	/* Clipperton Island */
	"CC",	/* Cocos (Keeling) Islands */
	"CK",	/* Cook Islands */
		/* Coral Sea Islands (Z1 duplicate) */
		/* Diego Garcia (Z1 duplicate) */
	"GL",	/* Greenland */
		/* Guantanamo Bay (Z1 duplicate) */
	"HM",	/* Heard Island and Mcdonald Islands */
		/* Jan Mayen (Z1 duplicate) */
	"KP",	/* Korea, Democratic People's Republic Of */
	"MH",	/* Marshall Islands */
	"PS",	/* Palestinian Territory, Occupied */
	"PN",	/* Pitcairn */
		/* Rota Island (Z1 duplicate) */
		/* Saipan (Z1 duplicate) */
	"SH",	/* Saint Helena */
	"GS",	/* South Georgia and The South Sandwich Islands */
	"SD",	/* Sudan */
	"SJ",	/* Svalbard and Jan Mayen */
	"TL",	/* Timor-leste (East Timor) */
		/* Tinian Island (Z1 duplicate) */
	"TK",	/* Tokelau */
	"TA",	/* Tristan Da Cunha */
		/* Wake Island (Z1 duplicate) */
	"YU",	/* Yugoslavia */
};

/* Regulatory Revision mapping support for aggregate country definitions */
struct wlc_cc_map {
	char cc[WLC_CNTRY_BUF_SZ];
	char mapped_cc[WLC_CNTRY_BUF_SZ];
	uint8 mapped_rev;
};

struct wlc_aggregate_cc_map_lookup {
	char cc[WLC_CNTRY_BUF_SZ];
	uint8 rev;
	const struct wlc_cc_map *map;
};

static const struct wlc_cc_map cc_map_ww1[] = {
	{"US", "US", 1},
	{"JP", "JP", 1},
	{"KR", "KR", 1},
	{"BN", "BN", 1},
	{"CA", "CA", 1},
	{"MX", "MX", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 1},
	{"AE", "AE", 1},
	{"AU", "AU", 2},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_ww1_1[] = {
	{"XV", "XV", 1},
	{"US", "US", 1},
	{"JP", "JP", 1},
	{"KR", "KR", 1},
	{"BN", "BN", 1},
	{"CA", "CA", 1},
	{"MX", "MX", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 1},
	{"AE", "AE", 1},
	{"AU", "AU", 2},
	{"", "", 0}
};


static const struct wlc_cc_map cc_map_ww2[] = {
	{"XV", "XV", 1},
	{"US", "US", 5},
	{"JP", "JP", 3},
	{"KR", "KR", 3},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"MX", "MX", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 2},
	{"AE", "AE", 1},
	{"KW", "KW", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_DE1[] = {
	{"US", "US", 1},
	{"JP", "JP", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_DE2[] = {
	{"US", "US", 5},
	{"JP", "JP", 5},
	{"KR", "KR", 3},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"MX", "MX", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 2},
	{"AE", "AE", 1},
	{"KW", "KW", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_XA1[] = {
	{"US", "US", 1},
	{"JP", "JP", 1},
	{"KR", "KR", 2},
	{"BN", "BN", 1},
	{"MX", "MX", 1},
	{"RU", "RU", 1},
	{"AE", "AE", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_X_1[] = {
	{"US", "US", 1},
	{"JP", "JP", 1},
	{"KR", "KR", 2},
	{"BN", "BN", 1},
	{"MX", "MX", 1},
	{"RU", "RU", 1},
	{"AE", "AE", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_X_2[] = {
	{"US", "US", 10},
	{"JP", "JP", 3},
	{"KR", "KR", 3},
	{"CA", "CA", 2},
	{"BN", "BN", 1},
	{"MX", "MX", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 2},
	{"AE", "AE", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_X_3[] = {
	{"US", "US", 10},
	{"JP", "JP", 4},
	{"KR", "KR", 3},
	{"CA", "CA", 2},
	{"BN", "BN", 1},
	{"MX", "MX", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 1},
	{"AE", "AE", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_X_4[] = {
	{"US", "US", 10},
	{"JP", "JP", 3},
	{"KR", "KR", 3},
	{"CA", "CA", 2},
	{"BN", "BN", 1},
	{"MX", "MX", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 3},
	{"AE", "AE", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_X_5[] = {
	{"US", "US", 16},
	{"JP", "JP", 6},
	{"KR", "KR", 4},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"MX", "MX", 1},
	{"RU", "RU", 2},
	{"TW", "TW", 3},
	{"AE", "AE", 1},
	{"AR", "AR", 1},
	{"PY", "PY", 1},
	{"AT", "AT", 1},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"HR", "HR", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"DE", "DE", 3},
	{"GR", "GR", 1},
	{"HU", "HU", 1},
	{"IS", "IS", 1},
	{"IE", "IE", 1},
	{"IT", "IT", 1},
	{"LV", "LV", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"MT", "MT", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"RO", "RO", 1},
	{"SK", "SK", 1},
	{"SI", "SI", 1},
	{"ES", "ES", 1},
	{"SE", "SE", 1},
	{"CH", "CH", 1},
	{"GB", "GB", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_X_6[] = {
	{"US", "US", 29},
	{"JP", "JP", 7},
	{"KR", "KR", 6},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"MX", "MX", 1},
	{"RU", "RU", 2},
	{"TW", "TW", 3},
	{"AE", "AE", 1},
	{"AR", "AR", 1},
	{"PY", "PY", 1},
	{"AT", "AT", 1},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"HR", "HR", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"DE", "DE", 3},
	{"GR", "GR", 1},
	{"HU", "HU", 1},
	{"IS", "IS", 1},
	{"IE", "IE", 1},
	{"IT", "IT", 1},
	{"LV", "LV", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"MT", "MT", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"RO", "RO", 1},
	{"SK", "SK", 1},
	{"SI", "SI", 1},
	{"ES", "ES", 1},
	{"SE", "SE", 1},
	{"CH", "CH", 1},
	{"GB", "GB", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_X_7[] = {
	{"US", "US", 50},
	{"JP", "JP", 12},
	{"KR", "KR", 10},
	{"BN", "BN", 1},
	{"CA", "CA", 11},
	{"MX", "MX", 2},
	{"MA", "MA", 0},
	{"RU", "RU", 3},
	{"TW", "TW", 4},
	{"AE", "AE", 3},
	{"UA", "UA", 3},
	{"KW", "KW", 3},
	{"AU", "AU", 2},
	{"CN", "CN", 3},
	{"IL", "IL", 2},
	{"PH", "PH", 2},
	{"ZA", "ZA", 2},
	{"ID", "ID", 2},
	{"PK", "PK", 1},
	{"VN", "VN", 1},
	{"SA", "SA", 2},
	{"MV", "MV", 1},
	{"AR", "AR", 4},
	{"BR", "BR", 1},
	{"CR", "CR", 1},
	{"CL", "CL", 2},
	{"CO", "CO", 1},
	{"EC", "EC", 1},
	{"SV", "SV", 1},
	{"PA", "PA", 1},
	{"PY", "PY", 1},
	{"PE", "PE", 2},
	{"PR", "PR", 2},
	{"VE", "VE", 1},
	{"AT", "AT", 2},
	{"BE", "BE", 2},
	{"BG", "BG", 2},
	{"HR", "HR", 2},
	{"CY", "CY", 2},
	{"CZ", "CZ", 2},
	{"DK", "DK", 2},
	{"EE", "EE", 2},
	{"FI", "FI", 2},
	{"FR", "FR", 2},
	{"DE", "DE", 5},
	{"GR", "GR", 2},
	{"HU", "HU", 2},
	{"IS", "IS", 2},
	{"IE", "IE", 2},
	{"IT", "IT", 2},
	{"LV", "LV", 2},
	{"LI", "LI", 2},
	{"LT", "LT", 2},
	{"LU", "LU", 2},
	{"MT", "MT", 2},
	{"NL", "NL", 2},
	{"NO", "NO", 2},
	{"PL", "PL", 2},
	{"PT", "PT", 2},
	{"RO", "RO", 2},
	{"SK", "SK", 2},
	{"SI", "SI", 2},
	{"ES", "ES", 2},
	{"SE", "SE", 1},
	{"CH", "CH", 1},
	{"GB", "GB", 1},
	{"TR", "TR", 3},
	{"BY", "BY", 1},
	{"", "", 0}
};


static const struct wlc_cc_map cc_map_US12[] = {
	{"US", "US", 12},
	{"JP", "JP", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_US16[] = {
	{"JP", "JP", 3},
	{"KR", "KR", 4},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"MX", "MX", 1},
	{"MA", "MA", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 2},
	{"AE", "AE", 1},
	{"KW", "KW", 1},
	{"AT", "AT", 1},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"HR", "HR", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"DE", "DE", 3},
	{"GR", "GR", 1},
	{"HU", "HU", 1},
	{"IS", "IS", 1},
	{"IE", "IE", 1},
	{"IT", "IT", 1},
	{"LV", "LV", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"MT", "MT", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"RO", "RO", 1},
	{"SK", "SK", 1},
	{"SI", "SI", 1},
	{"ES", "ES", 1},
	{"SE", "SE", 1},
	{"CH", "CH", 1},
	{"GB", "GB", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_US25[] = {
	{"US", "US", 25},
	{"JP", "JP", 3},
	{"KR", "KR", 4},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"MX", "MX", 1},
	{"MA", "MA", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 2},
	{"AE", "AE", 1},
	{"KW", "KW", 1},
	{"AR", "AR", 1},
	{"PY", "PY", 1},
	{"AT", "AT", 1},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"HR", "HR", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"DE", "DE", 3},
	{"GR", "GR", 1},
	{"HU", "HU", 1},
	{"IS", "IS", 1},
	{"IE", "IE", 1},
	{"IT", "IT", 1},
	{"LV", "LV", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"MT", "MT", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"RO", "RO", 1},
	{"SK", "SK", 1},
	{"SI", "SI", 1},
	{"ES", "ES", 1},
	{"SE", "SE", 1},
	{"CH", "CH", 1},
	{"GB", "GB", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_ww4[] = {
	{"XV", "XV", 1},
	{"US", "US", 25},
	{"JP", "JP", 3},
	{"KR", "KR", 4},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"MX", "MX", 1},
	{"MA", "MA", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 2},
	{"AE", "AE", 1},
	{"KW", "KW", 1},
	{"AR", "AR", 1},
	{"PY", "PY", 1},
	{"AT", "AT", 1},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"HR", "HR", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"DE", "DE", 3},
	{"GR", "GR", 1},
	{"HU", "HU", 1},
	{"IS", "IS", 1},
	{"IE", "IE", 1},
	{"IT", "IT", 1},
	{"LV", "LV", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"MT", "MT", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"RO", "RO", 1},
	{"SK", "SK", 1},
	{"SI", "SI", 1},
	{"ES", "ES", 1},
	{"SE", "SE", 1},
	{"CH", "CH", 1},
	{"GB", "GB", 1},
	{"", "", 0}
};


static struct wlc_cc_map cc_map_US33[] = {
	{"US", "US", 33},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_ww3[] = {
	{"XV", "XV", 1},
	{"US", "US", 16},
	{"JP", "JP", 3},
	{"KR", "KR", 4},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"MX", "MX", 1},
	{"MA", "MA", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 2},
	{"AE", "AE", 1},
	{"KW", "KW", 1},
	{"AT", "AT", 1},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"HR", "HR", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"DE", "DE", 3},
	{"GR", "GR", 1},
	{"HU", "HU", 1},
	{"IS", "IS", 1},
	{"IE", "IE", 1},
	{"IT", "IT", 1},
	{"LV", "LV", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"MT", "MT", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"RO", "RO", 1},
	{"SK", "SK", 1},
	{"SI", "SI", 1},
	{"ES", "ES", 1},
	{"SE", "SE", 1},
	{"CH", "CH", 1},
	{"GB", "GB", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_DE3[] = {
	{"US", "US", 16},
	{"JP", "JP", 5},
	{"KR", "KR", 4},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"MX", "MX", 1},
	{"MA", "MA", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 2},
	{"AE", "AE", 1},
	{"KW", "KW", 1},
	{"AT", "AT", 1},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"HR", "HR", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"DE", "DE", 3},
	{"GR", "GR", 1},
	{"HU", "HU", 1},
	{"IS", "IS", 1},
	{"IE", "IE", 1},
	{"IT", "IT", 1},
	{"LV", "LV", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"MT", "MT", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"RO", "RO", 1},
	{"SK", "SK", 1},
	{"SI", "SI", 1},
	{"ES", "ES", 1},
	{"SE", "SE", 1},
	{"CH", "CH", 1},
	{"GB", "GB", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_US17[] = {
	{"US", "US", 17},
	{"JP", "JP", 3},
	{"KR", "KR", 4},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"MX", "MX", 1},
	{"MA", "MA", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 2},
	{"AE", "AE", 1},
	{"UA", "UA", 0},
	{"KW", "KW", 1},
	{"AT", "AT", 1},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"HR", "HR", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"DE", "DE", 3},
	{"GR", "GR", 1},
	{"HU", "HU", 1},
	{"IS", "IS", 1},
	{"IE", "IE", 1},
	{"IT", "IT", 1},
	{"LV", "LV", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"MT", "MT", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"RO", "RO", 1},
	{"SK", "SK", 1},
	{"SI", "SI", 1},
	{"ES", "ES", 1},
	{"SE", "SE", 1},
	{"CH", "CH", 1},
	{"GB", "GB", 1},
	{"", "", 0}
};

static const struct wlc_cc_map cc_map_XZ_2[] = {
	{"US", "US", 34},
	{"JP", "JP", 8},
	{"KR", "KR", 8},
	{"BN", "BN", 2},
	{"CA", "CA", 6},
	{"MX", "MX", 1},
	{"MA", "MA", 1},
	{"RU", "RU", 1},
	{"TW", "TW", 3},
	{"AE", "AE", 1},
	{"KW", "KW", 1},
	{"AU", "AU", 1},
	{"SG", "SG", 2},
	{"IN", "IN", 1},
	{"MY", "MY", 1},
	{"NP", "NP", 1},
	{"CN", "CN", 1},
	{"AR", "AR", 2},
	{"PY", "PY", 1},
	{"AT", "AT", 1},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"HR", "HR", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"DE", "DE", 3},
	{"GR", "GR", 1},
	{"HU", "HU", 1},
	{"IS", "IS", 1},
	{"IE", "IE", 1},
	{"IT", "IT", 1},
	{"LV", "LV", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"MT", "MT", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"RO", "RO", 1},
	{"SK", "SK", 1},
	{"SI", "SI", 1},
	{"ES", "ES", 1},
	{"SE", "SE", 1},
	{"CH", "CH", 1},
	{"GB", "GB", 1},
	{"", "", 0}
};

static const struct wlc_aggregate_cc_map_lookup aggregate_cc_maps[] = {
	{"XA", 1, cc_map_XA1},
	{"X0", 1, cc_map_X_1},
	{"X1", 1, cc_map_X_1},
	{"X2", 1, cc_map_X_1},
	{"X3", 1, cc_map_X_1},
	{"X0", 2, cc_map_X_2},
	{"X1", 2, cc_map_X_2},
	{"X2", 2, cc_map_X_2},
	{"X3", 2, cc_map_X_2},
	{"X0", 3, cc_map_X_3},
	{"X1", 3, cc_map_X_4},
	{"X2", 3, cc_map_X_4},
	{"X3", 3, cc_map_X_4},
	{"X0", 4, cc_map_X_4},
	{"X0", 5, cc_map_X_5},
	{"X1", 4, cc_map_X_5},
	{"X2", 4, cc_map_X_5},
	{"X3", 4, cc_map_X_5},
	{"X0", 6, cc_map_X_6},
	{"X1", 5, cc_map_X_6},
	{"X2", 5, cc_map_X_6},
	{"X3", 5, cc_map_X_6},
	{"ww", 1, cc_map_ww1_1}, /* "ww" means SROM cc = \0\0 */
	{"X1", 6, cc_map_X_7},
	{"X2", 6, cc_map_X_7},
	{"X3", 6, cc_map_X_7},
	{"X0", 7, cc_map_X_7},
	{"X0", 8, cc_map_X_7}, /* placeholder for X0/8 4331 */
	{"US", 1, cc_map_ww1},
	{"IT", 1, cc_map_ww1},
	{"JP", 1, cc_map_ww1},
	{"CN", 1, cc_map_ww1},
	{"ID", 1, cc_map_ww1},
	{"GB", 1, cc_map_ww1},
	{"DE", 1, cc_map_DE1},
	{"US", 12, cc_map_US12},
	{"DE", 2, cc_map_DE2},
	{"ww", 2, cc_map_ww2}, /* "ww" means SROM cc = \0\0 */
	{"TR", 1, cc_map_DE2},
	{"ww", 3, cc_map_ww3}, /* "ww" means SROM cc = \0\0 */
	{"DE", 3, cc_map_DE3},
	{"US", 16, cc_map_US16},
	{"US", 17, cc_map_US17},
	{"US", 25, cc_map_US25},
	{"ww", 4, cc_map_ww4}, /* "ww" means SROM cc = \0\0 */
	{"ww", 5, cc_map_US33}, /* "ww" means SROM cc = \0\0 */
	{"XZ", 2, cc_map_XZ_2},
	{"", 0, NULL}
};

/* 20MHz channel info for 40MHz pairing support */

struct chan20_info {
	uint8	sb;
	uint8	adj_sbs;
};

/* indicates adjacent channels that are allowed for a 40 Mhz channel and
 * those that permitted by the HT
 */
struct chan20_info chan20_info[] = {
	/* 11b/11g */
/* 0 */		{1,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 1 */		{2,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 2 */		{3,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 3 */		{4,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 4 */		{5,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 5 */		{6,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 6 */		{7,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 7 */		{8,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 8 */		{9,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 9 */		{10,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 10 */	{11,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 11 */	{12,	(CH_LOWER_SB)},
/* 12 */	{13,	(CH_LOWER_SB)},
/* 13 */	{14,	(CH_LOWER_SB)},

/* 11a japan high */
/* 14 */	{34,	(CH_UPPER_SB)},
/* 15 */	{38,	(CH_LOWER_SB)},
/* 16 */	{42,	(CH_LOWER_SB)},
/* 17 */	{46,	(CH_LOWER_SB)},

/* 11a usa low */
/* 18 */	{36,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 19 */	{40,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 20 */	{44,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 21 */	{48,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 22 */	{52,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 23 */	{56,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 24 */	{60,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 25 */	{64,	(CH_LOWER_SB | CH_EWA_VALID)},

/* 11a Europe */
/* 26 */	{100,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 27 */	{104,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 28 */	{108,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 29 */	{112,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 30 */	{116,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 31 */	{120,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 32 */	{124,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 33 */	{128,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 34 */	{132,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 35 */	{136,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 36 */	{140,	(CH_LOWER_SB)},

/* 11a usa high, ref5 only */
/* The 0x80 bit in pdiv means these are REF5, other entries are REF20 */
/* 37 */	{149,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 38 */	{153,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 39 */	{157,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 40 */	{161,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 41 */	{165,	(CH_LOWER_SB)},

/* 11a japan */
/* 42 */	{184,	(CH_UPPER_SB)},
/* 43 */	{188,	(CH_LOWER_SB)},
/* 44 */	{192,	(CH_UPPER_SB)},
/* 45 */	{196,	(CH_LOWER_SB)},
/* 46 */	{200,	(CH_UPPER_SB)},
/* 47 */	{204,	(CH_LOWER_SB)},
/* 48 */	{208,	(CH_UPPER_SB)},
/* 49 */	{212,	(CH_LOWER_SB)},
/* 50 */	{216,	(CH_LOWER_SB)}
};

/* country code mapping for SPROM rev 1 */
static const char def_country[][WLC_CNTRY_BUF_SZ] = {
	"AU",   /* Worldwide */
	"TH",   /* Thailand */
	"IL",   /* Israel */
	"JO",   /* Jordan */
	"CN",   /* China */
	"JP",   /* Japan */
	"US",   /* USA */
	"DE",   /* Europe */
	"US",   /* US Low Band, use US */
	"JP",   /* Japan High Band, use Japan */
};

/* autocountry default country code list */
static const char def_autocountry[][WLC_CNTRY_BUF_SZ] = {
	"XY",
	"XA",
	"XB",
	"X0",
	"X1",
	"X2",
	"X3",
	"XS",
	"XV"
};


const locale_info_t * wlc_get_locale_2g(uint8 locale_idx)
{
#ifdef BAND2G
	if (locale_idx >= ARRAYSIZE(g_locale_2g_table)) {
		WL_ERROR(("wl%d: locale 2g index size out of range\n", locale_idx));
		return NULL;
	}
	return 	g_locale_2g_table[locale_idx];
#else
	return NULL;
#endif
}

const locale_info_t * wlc_get_locale_5g(uint8 locale_idx)
{
#ifdef BAND5G
	if (locale_idx >= ARRAYSIZE(g_locale_5g_table)) {
		WL_ERROR(("wl%d: locale 5g index size out of range\n", locale_idx));
		return NULL;
	}
	return 	g_locale_5g_table[locale_idx];
#else
	return NULL;
#endif
}

#ifdef WL11N
const locale_mimo_info_t * wlc_get_mimo_2g(uint8 locale_idx)
{
	if (locale_idx >= ARRAYSIZE(g_mimo_2g_table)) {
		WL_ERROR(("wl%d: mimo 2g index size out of range\n", locale_idx));
		return NULL;
	}
	return 	g_mimo_2g_table[locale_idx];
}

const locale_mimo_info_t * wlc_get_mimo_5g(uint8 locale_idx)
{
	if (locale_idx >= ARRAYSIZE(g_mimo_5g_table)) {
		WL_ERROR(("wl%d: mimo 5g index size out of range\n", locale_idx));
		return NULL;
	}
	return 	g_mimo_5g_table[locale_idx];
}
#endif /* #ifdef WL11N */

static bool
wlc_autocountry_lookup(char *cc)
{
	uint i;

	for (i = 0; i < ARRAYSIZE(def_autocountry); i++)
		if (!strcmp(def_autocountry[i], cc))
			return TRUE;

	return FALSE;
}

wlc_cm_info_t *
BCMATTACHFN(wlc_channel_mgr_attach)(wlc_info_t *wlc)
{
	wlc_cm_info_t *wlc_cm;
	char country_abbrev[WLC_CNTRY_BUF_SZ];
	const country_info_t *country;
	wlc_pub_t *pub = wlc->pub;
#ifdef PCOEM_LINUXSTA
	bool use_row = TRUE;
#endif

	WL_TRACE(("wl%d: wlc_channel_mgr_attach\n", wlc->pub->unit));

	if ((wlc_cm = (wlc_cm_info_t *)MALLOC(pub->osh, sizeof(wlc_cm_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes", pub->unit,
			__FUNCTION__, MALLOCED(pub->osh)));
		return NULL;
	}
	bzero((char *)wlc_cm, sizeof(wlc_cm_info_t));
	wlc_cm->pub = pub;
	wlc_cm->wlc = wlc;
	wlc->cmi = wlc_cm;

	/* init the per chain limits to max power so they have not effect */
	memset(&wlc_cm->bandstate[BAND_2G_INDEX].chain_limits, WLC_TXPWR_MAX,
	       sizeof(struct wlc_channel_txchain_limits));
	memset(&wlc_cm->bandstate[BAND_5G_INDEX].chain_limits, WLC_TXPWR_MAX,
	       sizeof(struct wlc_channel_txchain_limits));

	/* get the SPROM country code or local, required to initialize channel set below */
	bzero(country_abbrev, WLC_CNTRY_BUF_SZ);
	if (wlc->pub->sromrev > 1) {
		/* get country code */
		char *ccode = getvar(wlc->pub->vars, "ccode");
		if (ccode)
			strncpy(country_abbrev, ccode, WLC_CNTRY_BUF_SZ - 1);
	} else {
		uint locale_num;

		/* get locale */
		locale_num = (uint)getintvar(wlc->pub->vars, "cc");
		/* get mapped country */
		if (locale_num < ARRAYSIZE(def_country))
			strncpy(country_abbrev, def_country[locale_num],
			        sizeof(country_abbrev) - 1);
	}
	strncpy(wlc_cm->srom_ccode, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
	wlc_cm->srom_regrev = getintvar(wlc->pub->vars, "regrev");

	/* For "some" apple boards with KR2, make them as KR3 as they have passed the
	 * FCC test but with wrong SROM contents
	 */
	if ((pub->sih->boardvendor == VENDOR_APPLE) &&
	    ((pub->sih->boardtype == BCM943224M93) ||
	     (pub->sih->boardtype == BCM943224M93A))) {
		if ((wlc_cm->srom_regrev == 2) &&
		    !strncmp(country_abbrev, "KR", WLC_CNTRY_BUF_SZ)) {
			wlc_cm->srom_regrev = 3;
		}
	}

	/* Correct SROM contents of an Apple board */
	if ((pub->sih->boardvendor == VENDOR_APPLE) &&
	    (pub->sih->boardtype == 0x93) &&
	    !strncmp(country_abbrev, "JP", WLC_CNTRY_BUF_SZ) &&
	    (wlc_cm->srom_regrev == 4)) {
		wlc_cm->srom_regrev = 6;
	}

	country = wlc_country_lookup(wlc, country_abbrev);

	/* default to US if country was not specified or not found */
	if (country == NULL) {
		strncpy(country_abbrev, "US", sizeof(country_abbrev) - 1);
		country = wlc_country_lookup(wlc, country_abbrev);
	}

	ASSERT(country != NULL);

	/* save default country for exiting 11d regulatory mode */
	strncpy(wlc->country_default, country_abbrev, WLC_CNTRY_BUF_SZ - 1);

	/* initialize autocountry_default to driver default */
	if (wlc_autocountry_lookup(country_abbrev))
		strncpy(wlc->autocountry_default, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
	else
		strncpy(wlc->autocountry_default, "B2", WLC_CNTRY_BUF_SZ - 1);

#ifdef PCOEM_LINUXSTA
	if ((CHIPID(pub->sih->chip) != BCM4311_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM4312_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM4313_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM4321_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM4322_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43224_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43225_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43421_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM4342_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43131_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43227_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43228_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43428_CHIP_ID)) {
		WL_ERROR(("wl%d: %s: Unsupported Chip (%x)\n", pub->unit, __FUNCTION__,
			pub->sih->chip));
		wlc->cmi = NULL;
		wlc_channel_mgr_detach(wlc_cm);
		return NULL;
	}
	if ((pub->sih->boardvendor == VENDOR_HP) && (!bcmp(country_abbrev, "US", 2) ||
		!bcmp(country_abbrev, "JP", 2) || !bcmp(country_abbrev, "IL", 2)))
		use_row = FALSE;

	/* use RoW locale if set */
	if (use_row) {
		bzero(country_abbrev, WLC_CNTRY_BUF_SZ);
		strncpy(country_abbrev, "XW", WLC_CNTRY_BUF_SZ);
	}

	/* Enable Auto Country Discovery */
	strncpy(wlc->autocountry_default, country_abbrev, WLC_CNTRY_BUF_SZ - 1);

	/* enable regulatory */
	wlc->_regulatory_domain = TRUE;
#endif /* PCOEM_LINUXSTA */

	/* Calling set_countrycode() once do not generate any event, if called more than
	 * once generates COUNTRY_CODE_CHANGED event which will cause the driver to crash
	 * since bsscfg structure is still not initialized
	 */
	wlc_set_countrycode(wlc_cm, country_abbrev);

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(wlc->pub, "txpwr_reg",
	                  (dump_fn_t)wlc_channel_dump_reg_ppr, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "txpwr_local",
	                  (dump_fn_t)wlc_channel_dump_reg_local_ppr, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "txpwr_srom",
	                  (dump_fn_t)wlc_channel_dump_srom_ppr, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "txpwr_margin",
	                  (dump_fn_t)wlc_channel_dump_margin, (void *)wlc_cm);
#endif /* BCMDBG || BCMDBG_DUMP */

	return wlc_cm;
}

void
BCMATTACHFN(wlc_channel_mgr_detach)(wlc_cm_info_t *wlc_cm)
{
	if (wlc_cm)
		MFREE(wlc_cm->pub->osh, wlc_cm, sizeof(wlc_cm_info_t));
}

const char* wlc_channel_country_abbrev(wlc_cm_info_t *wlc_cm)
{
	return wlc_cm->country_abbrev;
}

const char* wlc_channel_ccode(wlc_cm_info_t *wlc_cm)
{
	return wlc_cm->ccode;
}

uint wlc_channel_regrev(wlc_cm_info_t *wlc_cm)
{
	return wlc_cm->regrev;
}

uint8 wlc_channel_locale_flags(wlc_cm_info_t *wlc_cm)
{
	wlc_info_t *wlc = wlc_cm->wlc;

	return wlc_cm->bandstate[wlc->band->bandunit].locale_flags;
}

uint8 wlc_channel_locale_flags_in_band(wlc_cm_info_t *wlc_cm, uint bandunit)
{
	return wlc_cm->bandstate[bandunit].locale_flags;
}

/*
 * return the first valid chanspec for the locale, if one is not found and hw_fallback is true
 * then return the first h/w supported chanspec.
 */
chanspec_t
wlc_default_chanspec(wlc_cm_info_t *wlc_cm, bool hw_fallback)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	chanspec_t  chspec;

	chspec = wlc_create_chspec(wlc, 0);
	/* try to find a chanspec thats valid in this locale */
	if ((chspec = wlc_next_chanspec(wlc_cm, chspec, CHAN_TYPE_ANY, 0)) == INVCHANSPEC)
		/* try to find a chanspec valid for this hardware */
		if (hw_fallback)
			chspec = wlc_phy_chanspec_band_firstch(wlc_cm->wlc->band->pi,
				wlc->band->bandtype);

	return chspec;
}

/*
 * Return the next channel's chanspec.
 */
chanspec_t
wlc_next_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t cur_chanspec, int type, bool any_band)
{
	uint8 ch;
	uint8 cur_chan = CHSPEC_CHANNEL(cur_chanspec);
	chanspec_t chspec;

	/* 0 is an invalid chspec, routines trying to find the first available channel should
	 * now be using wlc_default_chanspec (above)
	 */
	ASSERT(cur_chanspec);

	/* Try all channels in current band */
	ch = cur_chan + 1;
	for (; ch <= MAXCHANNEL; ch++) {
		if (ch == MAXCHANNEL)
			ch = 0;
		if (ch == cur_chan)
			break;
		/* create the next channel spec */
		chspec = cur_chanspec & ~WL_CHANSPEC_CHAN_MASK;
		chspec |= ch;
		if (wlc_valid_chanspec(wlc_cm, chspec)) {
			if ((type == CHAN_TYPE_ANY) ||
			(type == CHAN_TYPE_CHATTY && !wlc_quiet_chanspec(wlc_cm, chspec)) ||
			(type == CHAN_TYPE_QUIET && wlc_quiet_chanspec(wlc_cm, chspec)))
				return chspec;
		}
	}

	if (!any_band)
		return ((chanspec_t)INVCHANSPEC);

	/* Couldn't find any in current band, try other band */
	ch = cur_chan + 1;
	for (; ch <= MAXCHANNEL; ch++) {
		if (ch == MAXCHANNEL)
			ch = 0;
		if (ch == cur_chan)
			break;

		/* create the next channel spec */
		chspec = cur_chanspec & ~(WL_CHANSPEC_CHAN_MASK | WL_CHANSPEC_BAND_MASK);
		chspec |= ch;
		if (CHANNEL_BANDUNIT(wlc, ch) == BAND_2G_INDEX)
			chspec |= WL_CHANSPEC_BAND_2G;
		else
			chspec |= WL_CHANSPEC_BAND_5G;
		if (wlc_valid_chanspec_db(wlc_cm, chspec)) {
			if ((type == CHAN_TYPE_ANY) ||
			(type == CHAN_TYPE_CHATTY && !wlc_quiet_chanspec(wlc_cm, chspec)) ||
			(type == CHAN_TYPE_QUIET && wlc_quiet_chanspec(wlc_cm, chspec)))
				return chspec;
		}
	}

	return ((chanspec_t)INVCHANSPEC);
}

/* return chanvec for a given country code and band */
bool
wlc_channel_get_chanvec(struct wlc_info *wlc, const char* country_abbrev,
	int bandtype, chanvec_t *channels)
{
	const country_info_t *country;
	const locale_info_t *locale = NULL;

	country = wlc_country_lookup(wlc, country_abbrev);
	if (country == NULL)
		return FALSE;

	if (bandtype == WLC_BAND_2G)
		locale = wlc_get_locale_2g(country->locale_2G);
	else if (bandtype == WLC_BAND_5G)
		locale = wlc_get_locale_5g(country->locale_5G);
	if (locale == NULL)
		return FALSE;

	wlc_locale_get_channels(locale, channels);
	return TRUE;
}

/* set the driver's current country and regulatory information using a country code
 * as the source. Lookup built in country information found with the country code.
 */
int
wlc_set_countrycode(wlc_cm_info_t *wlc_cm, const char* ccode)
{
	char country_abbrev[WLC_CNTRY_BUF_SZ];

	WL_NONE(("wl%d: %s: ccode \"%s\"\n", wlc_cm->wlc->pub->unit, __FUNCTION__, ccode));

	/* substitute an external ccode for some application specific built-in country codes,
	 * othewise, use the given abbrev
	 */
	if (wlc_japan_ccode(ccode))
		strncpy(country_abbrev, "JP", WLC_CNTRY_BUF_SZ-1);
	else if (wlc_us_ccode(ccode))
		strncpy(country_abbrev, "US", WLC_CNTRY_BUF_SZ-1);
	else
		strncpy(country_abbrev, ccode, WLC_CNTRY_BUF_SZ-1);
	country_abbrev[WLC_CNTRY_BUF_SZ-1] = '\0';
	return wlc_set_countrycode_rev(wlc_cm, country_abbrev, ccode, -1);
}

int
wlc_set_countrycode_rev(wlc_cm_info_t *wlc_cm,
                        const char* country_abbrev,
                        const char* ccode, int regrev)
{
#ifdef BCMDBG
	wlc_info_t *wlc = wlc_cm->wlc;
#endif
	const country_info_t *country;
	char mapped_ccode[WLC_CNTRY_BUF_SZ];
	uint mapped_regrev = 0;

	WL_NONE(("wl%d: %s: (country_abbrev \"%s\", ccode \"%s\", regrev %d) SPROM \"%s\"/%u\n",
	         wlc->pub->unit, __FUNCTION__,
	         country_abbrev, ccode, regrev, wlc_cm->srom_ccode, wlc_cm->srom_regrev));

	/* if regrev is -1, lookup the mapped country code,
	 * otherwise use the ccode and regrev directly
	 */
	if (regrev == -1) {
		/* map the country code to a built-in country code, regrev, and country_info */
		country = wlc_countrycode_map(wlc_cm, ccode, mapped_ccode, &mapped_regrev);
		if (country)
			WL_NONE(("wl%d: %s: mapped to \"%s\"/%u\n",
			         wlc->pub->unit, __FUNCTION__, ccode, mapped_regrev));
		else
			WL_NONE(("wl%d: %s: failed lookup\n",
			        wlc->pub->unit, __FUNCTION__));
	} else {
		/* find the matching built-in country definition */
		country = wlc_country_lookup_direct(ccode, regrev);
		strncpy(mapped_ccode, ccode, WLC_CNTRY_BUF_SZ-1);
		mapped_ccode[WLC_CNTRY_BUF_SZ-1] = '\0';
		mapped_regrev = regrev;
	}

	if (country == NULL)
		return BCME_BADARG;

	/* set the driver state for the country */
	wlc_set_country_common(wlc_cm, country_abbrev, mapped_ccode, mapped_regrev, country);

	return 0;
}

/* set the driver's current country and regulatory information using a country code
 * as the source. Look up built in country information found with the country code.
 */
static void
wlc_set_country_common(wlc_cm_info_t *wlc_cm,
                       const char* country_abbrev,
                       const char* ccode, uint regrev, const country_info_t *country)
{
#ifdef WL11N
	const locale_mimo_info_t * li_mimo;
#endif
	const locale_info_t * locale;
	wlc_info_t *wlc = wlc_cm->wlc;
	const country_info_t *prev_country = wlc_cm->country;
	char prev_country_abbrev[WLC_CNTRY_BUF_SZ];

	ASSERT(country != NULL);

	/* save current country state */
	wlc_cm->country = country;

	bzero(&prev_country_abbrev, WLC_CNTRY_BUF_SZ);
	strncpy(prev_country_abbrev, wlc_cm->country_abbrev, WLC_CNTRY_BUF_SZ - 1);

	strncpy(wlc_cm->country_abbrev, country_abbrev, WLC_CNTRY_BUF_SZ-1);
	strncpy(wlc_cm->ccode, ccode, WLC_CNTRY_BUF_SZ-1);
	wlc_cm->regrev = regrev;

#ifdef WL11N
	/* disable/restore nmode based on country regulations */
	li_mimo = BAND_5G(wlc->band->bandtype) ?
		wlc_get_mimo_5g(country->locale_mimo_5G) :
		wlc_get_mimo_2g(country->locale_mimo_2G);
	if (li_mimo && (li_mimo->flags & WLC_NO_MIMO)) {
		wlc_set_nmode(wlc, OFF);
		wlc->stf->no_cddstbc = TRUE;
	} else {
		wlc->stf->no_cddstbc = FALSE;
		if (N_ENAB(wlc->pub) != wlc->protection->nmode_user)
			wlc_set_nmode(wlc, wlc->protection->nmode_user);
	}

	wlc_stf_ss_update(wlc, wlc->bandstate[BAND_2G_INDEX]);
	wlc_stf_ss_update(wlc, wlc->bandstate[BAND_5G_INDEX]);
#endif /* WL11N */
	/* set or restore gmode as required by regulatory */
	locale = wlc_get_locale_2g(country->locale_2G);
	if (locale && (locale->flags & WLC_NO_OFDM))
		wlc_set_gmode(wlc, GMODE_LEGACY_B, FALSE);
	else
		wlc_set_gmode(wlc, wlc->protection->gmode_user, FALSE);

#if defined(AP) && defined(RADAR)
	if ((NBANDS(wlc) == 2) || IS_SINGLEBAND_5G(wlc->deviceid)) {
		phy_radar_detect_mode_t mode;
		const locale_info_t * li;

		li = wlc_get_locale_5g(country->locale_5G);
		mode = ISDFS_EU(li->flags) ? RADAR_DETECT_MODE_EU:RADAR_DETECT_MODE_FCC;
		wlc_phy_radar_detect_mode_set(wlc->band->pi, mode);
	}
#endif /* AP && RADAR */

	wlc_channels_init(wlc_cm, country);

	/* Country code changed */
	if (prev_country &&
	    strncmp(wlc_cm->country_abbrev, prev_country_abbrev,
	            strlen(wlc_cm->country_abbrev)) != 0)
		wlc_mac_event(wlc, WLC_E_COUNTRY_CODE_CHANGED, NULL,
		              0, 0, 0, wlc_cm->country_abbrev, strlen(wlc_cm->country_abbrev) + 1);

	return;
}

/* Lookup a country info structure from a null terminated country code
 * The lookup is case sensitive.
 */
const country_info_t*
wlc_country_lookup(struct wlc_info *wlc, const char* ccode)
{
	const country_info_t *country;
	char mapped_ccode[WLC_CNTRY_BUF_SZ];
	uint mapped_regrev;

	WL_NONE(("wl%d: %s: ccode \"%s\", SPROM \"%s\"/%u\n",
	         wlc->pub->unit, __FUNCTION__, ccode, wlc->cmi->srom_ccode, wlc->cmi->srom_regrev));

	/* map the country code to a built-in country code, regrev, and country_info struct */
	country = wlc_countrycode_map(wlc->cmi, ccode, mapped_ccode, &mapped_regrev);

	if (country)
		WL_NONE(("wl%d: %s: mapped to \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, mapped_ccode, mapped_regrev));
	else
		WL_NONE(("wl%d: %s: failed lookup\n",
		         wlc->pub->unit, __FUNCTION__));

	return country;
}

static const country_info_t*
wlc_countrycode_map(wlc_cm_info_t *wlc_cm, const char *ccode,
	char *mapped_ccode, uint *mapped_regrev)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	const country_info_t *country;
	uint srom_regrev = wlc_cm->srom_regrev;
	const char *srom_ccode = wlc_cm->srom_ccode;
	int mapped;

	/* check for currently supported ccode size */
	if (strlen(ccode) > (WLC_CNTRY_BUF_SZ - 1)) {
		WL_ERROR(("wl%d: %s: ccode \"%s\" too long for match\n",
		          wlc->pub->unit, __FUNCTION__, ccode));
		return NULL;
	}

	/* default mapping is the given ccode and regrev 0 */
	strncpy(mapped_ccode, ccode, WLC_CNTRY_BUF_SZ);
	*mapped_regrev = 0;

	/* Map locale for buffalo if needed */
	if (wlc_buffalo_map_locale(wlc, ccode)) {
		strncpy(mapped_ccode, "J10", WLC_CNTRY_BUF_SZ);
		country = wlc_country_lookup_direct(mapped_ccode, *mapped_regrev);
		if (country)
			return country;
	}

	/* If the desired country code matches the srom country code,
	 * then the mapped country is the srom regulatory rev.
	 * Otherwise look for an aggregate mapping.
	 */
	if (!strcmp(srom_ccode, ccode)) {
		WL_NONE(("wl%d: %s: srom ccode and ccode \"%s\" match\n",
		         wlc->pub->unit, __FUNCTION__, ccode));
		*mapped_regrev = srom_regrev;
		mapped = 0;
	} else {
		mapped = wlc_country_aggregate_map(wlc_cm, ccode, mapped_ccode, mapped_regrev);
		if (mapped)
			WL_NONE(("wl%d: %s: found aggregate mapping \"%s\"/%u\n",
			         wlc->pub->unit, __FUNCTION__, mapped_ccode, *mapped_regrev));
	}

	/* CLM 8.2, JAPAN
	 * Use the regrev=1 Japan country definition by default for chips newer than
	 * our d11 core rev 5 4306 chips, or for AP's of any vintage.
	 * For STAs with a 4306, use the legacy Japan country def "JP/0".
	 * Only do the conversion if JP/0 was not specified by a mapping or by an
	 * sprom with a regrev:
	 * Sprom has no specific info about JP's regrev if it's less than rev 3 or it does
	 * not specify "JP" as its country code =>
	 * (strcmp("JP", srom_ccode) || (wlc->pub->sromrev < 3))
	 */
	if (!strcmp("JP", mapped_ccode) && *mapped_regrev == 0 &&
	    !mapped && (strcmp("JP", srom_ccode) || (wlc->pub->sromrev < 3)) &&
	    (AP_ENAB(wlc->pub) || D11REV_GT(wlc->pub->corerev, 5))) {
		*mapped_regrev = 1;
		WL_NONE(("wl%d: %s: Using \"JP/1\" instead of legacy \"JP/0\" since we %s\n",
		         wlc->pub->unit, __FUNCTION__,
		         AP_ENAB(wlc->pub) ? "are operating as an AP" : "are newer than 4306"));
	}

	WL_NONE(("wl%d: %s: searching for country using ccode/rev \"%s\"/%u\n",
	         wlc->pub->unit, __FUNCTION__, mapped_ccode, *mapped_regrev));

	/* find the matching built-in country definition */
	country = wlc_country_lookup_direct(mapped_ccode, *mapped_regrev);

	/* if there is not an exact rev match, default to rev zero */
	if (country == NULL && *mapped_regrev != 0) {
		*mapped_regrev = 0;
		WL_NONE(("wl%d: %s: No country found, use base revision \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, mapped_ccode, *mapped_regrev));
		country = wlc_country_lookup_direct(mapped_ccode, *mapped_regrev);
	}

	if (country == NULL)
		WL_NONE(("wl%d: %s: No country found, failed lookup\n",
		         wlc->pub->unit, __FUNCTION__));

	return country;
}

static int
wlc_country_aggregate_map(wlc_cm_info_t *wlc_cm, const char *ccode,
                          char *mapped_ccode, uint *mapped_regrev)
{
#ifdef BCMDBG
	wlc_info_t *wlc = wlc_cm->wlc;
#endif
	const struct wlc_aggregate_cc_map_lookup *cc_lookup;
	const struct wlc_cc_map *map;
	const char *srom_ccode = wlc_cm->srom_ccode;
	uint srom_regrev = (uint8)wlc_cm->srom_regrev;

	/* Use "ww", WorldWide, for the lookup value for '\0\0' */
	if (srom_ccode[0] == '\0')
		srom_ccode = "ww";

	/* Check for a match in the aggregate country list */
	WL_NONE(("wl%d: %s: searching for agg map for srom ccode/rev \"%s\"/%u\n",
	         wlc->pub->unit, __FUNCTION__, srom_ccode, srom_regrev));

	map = NULL;
	for (cc_lookup = aggregate_cc_maps; cc_lookup->cc[0]; cc_lookup++) {
		if (!strcmp(srom_ccode, cc_lookup->cc) && srom_regrev == cc_lookup->rev) {
			map = cc_lookup->map;
			break;
		}
	}

	if (!map)
		WL_NONE(("wl%d: %s: no map for \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, srom_ccode, srom_regrev));
	else
		WL_NONE(("wl%d: %s: found map for \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, srom_ccode, srom_regrev));

	/* Check if the given country code is mapped in the cc_map */
	while (map && map->cc[0] != '\0') {
		WL_NONE(("wl%d: %s: checking match of \"%s\" with map \"%s\" -> \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, ccode,
		         map->cc, map->mapped_cc, map->mapped_rev));
		/* if there is a match, use it to look up a country */
		if (!strcmp(map->cc, ccode)) {
			WL_NONE(("wl%d: %s: mapped lookup for \"%s\" found country \"%s\"/%u\n",
			         wlc->pub->unit, __FUNCTION__,
			         ccode, map->mapped_cc, map->mapped_rev));
			strncpy(mapped_ccode, map->mapped_cc, WLC_CNTRY_BUF_SZ);
			*mapped_regrev = map->mapped_rev;
			return TRUE;
		}
		map++;
	}

	return FALSE;
}

/* Lookup a country info structure from a null terminated country
 * abbreviation and regrev directly with no translation.
 */
static const country_info_t*
wlc_country_lookup_direct(const char* ccode, uint regrev)
{
	uint size, i;

	/* find a country in the regrev country table */
	size = ARRAYSIZE(cntry_rev_locales);
	for (i = 0; i < size; i++) {
		if (strcmp(ccode, cntry_rev_locales[i].abbrev) == 0 &&
		    regrev == cntry_rev_locales[i].rev) {
			return &cntry_rev_locales[i].country;
		}
	}

	/* all other country def arrays are for regrev == 0, so if regrev is non-zero, fail */
	if (regrev > 0)
		return NULL;

	/* find matched table entry from country code */
	size = ARRAYSIZE(cntry_locales);
	for (i = 0; i < size; i++) {
		if (strcmp(ccode, cntry_locales[i].abbrev) == 0) {
			return &cntry_locales[i].country;
		}
	}

	/* search alternative country code list of countries that do not support
	 * any 802.11 channels
	 */
	size = ARRAYSIZE(country_nochannel);
	for (i = 0; i < size; i++) {
		if (strcmp(ccode, country_nochannel[i]) == 0) {
			return &country_nochannel_locale;
		}
	}

	return NULL;
}

#if defined(STA) && defined(WL11D)
/* Lookup a country info structure considering only legal country codes as found in
 * a Country IE; two ascii alpha followed by " ", "I", or "O".
 * Do not match any user assigned application specifc codes that might be found in the driver table.
 */
const country_info_t*
wlc_country_lookup_ext(wlc_info_t *wlc, const char *ccode)
{
	char country_str_lookup[WLC_CNTRY_BUF_SZ] = { 0 };

	/* only allow ascii alpha uppercase for the first 2 chars, and " ", "I", "O" for the 3rd */
	if (!((0x80 & ccode[0]) == 0 && bcm_isupper(ccode[0]) &&
	      (0x80 & ccode[1]) == 0 && bcm_isupper(ccode[1]) &&
	      (ccode[2] == ' ' || ccode[2] == 'I' || ccode[2] == 'O')))
		return NULL;

	/* for lookup in the driver table of country codes, only use the first
	 * 2 chars, ignore the 3rd character " ", "I", "O" qualifier
	 */
	country_str_lookup[0] = ccode[0];
	country_str_lookup[1] = ccode[1];

	/* do not match ISO 3166-1 user assigned country codes that may be in the driver table */
	if (!strcmp("AA", country_str_lookup) ||	/* AA */
	    !strcmp("ZZ", country_str_lookup) ||	/* ZZ */
	    country_str_lookup[0] == 'X' ||		/* XA - XZ */
	    (country_str_lookup[0] == 'Q' &&		/* QM - QZ */
	     (country_str_lookup[1] >= 'M' && country_str_lookup[1] <= 'Z')))
		return NULL;

#ifdef MACOSX
	if (!strcmp("NA", country_str_lookup))
		return NULL;
#endif /* MACOSX */

	return wlc_country_lookup(wlc, country_str_lookup);
}
#endif /* STA && WL11D */

static int
wlc_channels_init(wlc_cm_info_t *wlc_cm, const country_info_t *country)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	uint i, j;
	wlcband_t * band;
	const locale_info_t * li;
	chanvec_t sup_chan;
#ifdef WL11N
	const locale_mimo_info_t *li_mimo;
#endif /* WL11N */

#ifdef AP
	bzero(wlc->ap->chan_blocked, sizeof(wlc->ap->chan_blocked));
#endif

	bzero(&wlc_cm->restricted_channels, sizeof(chanvec_t));

	band = wlc->band;
	for (i = 0; i < NBANDS(wlc); i++, band = wlc->bandstate[OTHERBANDUNIT(wlc)]) {

		li = BAND_5G(band->bandtype) ?
			wlc_get_locale_5g(country->locale_5G) :
			wlc_get_locale_2g(country->locale_2G);
		wlc_cm->bandstate[band->bandunit].locale_flags = li->flags;
#ifdef WL11N
		li_mimo = BAND_5G(band->bandtype) ?
		        wlc_get_mimo_5g(country->locale_mimo_5G) :
			wlc_get_mimo_2g(country->locale_mimo_2G);

		/* merge the mimo non-mimo locale flags */
		wlc_cm->bandstate[band->bandunit].locale_flags |= li_mimo->flags;
#endif /* WL11N */

		/* initialize restricted channels */
		for (j = 0; j < sizeof(chanvec_t); j++) {
			wlc_cm->restricted_channels.vec[j] |=
			g_table_restricted_chan[li->restricted_channels]->vec[j];
		}
#ifdef BAND5G     /* RADAR */
		wlc_cm->bandstate[band->bandunit].radar_channels =
			g_table_radar_set[li->radar_channels];
#endif

		/* set the channel availability,
		 * masking out the channels that may not be supported on this phy
		 */
		wlc_phy_chanspec_band_validch(band->pi, band->bandtype, &sup_chan);
		wlc_locale_get_channels(li,
			&wlc_cm->bandstate[band->bandunit].valid_channels);
		for (j = 0; j < sizeof(chanvec_t); j++)
			wlc_cm->bandstate[band->bandunit].valid_channels.vec[j] &=
				sup_chan.vec[j];
	}

	wlc_upd_restricted_chanspec_flag(wlc_cm);
	wlc_quiet_channels_reset(wlc_cm);
	wlc_channels_commit(wlc_cm);
	wlc_rcinfo_init(wlc_cm);
	wlc_regclass_vec_init(wlc_cm);

	return (0);
}

/* Update the radio state (enable/disable) and tx power targets
 * based on a new set of channel/regulatory information
 */
static void
wlc_channels_commit(wlc_cm_info_t *wlc_cm)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	uint chan;
	txppr_t txpwr;

	/* search for the existence of any valid channel */
	for (chan = 0; chan < MAXCHANNEL; chan++) {
		if (VALID_CHANNEL20_DB(wlc, chan)) {
			break;
		}
	}
	if (chan == MAXCHANNEL)
		chan = INVCHANNEL;

	/* based on the channel search above, set or clear WL_RADIO_COUNTRY_DISABLE */
	if (chan == INVCHANNEL) {
		/* country/locale with no valid channels, set the radio disable bit */
		mboolset(wlc->pub->radio_disabled, WL_RADIO_COUNTRY_DISABLE);
		WL_ERROR(("wl%d: %s: no valid channel for \"%s\" nbands %d bandlocked %d\n",
		          wlc->pub->unit, __FUNCTION__,
		          wlc_cm->country_abbrev, NBANDS(wlc), wlc->bandlocked));
	} else if (mboolisset(wlc->pub->radio_disabled, WL_RADIO_COUNTRY_DISABLE)) {
		/* country/locale with valid channel, clear the radio disable bit */
		mboolclr(wlc->pub->radio_disabled, WL_RADIO_COUNTRY_DISABLE);
	}

	/* Now that the country abbreviation is set, if the radio supports 2G, then
	 * set channel 14 restrictions based on the new locale.
	 */
	if (NBANDS(wlc) > 1 || BAND_2G(wlc->band->bandtype)) {
		wlc_phy_chanspec_ch14_widefilter_set(wlc->band->pi, wlc_japan(wlc) ? TRUE : FALSE);
	}

	if (wlc->pub->up && chan != INVCHANNEL) {
		/* recompute tx power for new country info */


		/* Where do we get a good chanspec? wlc, phy, set it ourselves? */
		wlc_channel_reg_limits(wlc_cm, wlc->chanspec, &txpwr);

		wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm, &txpwr, WLC_TXPWR_MAX);

		/* Where do we get a good chanspec? wlc, phy, set it ourselves? */
		wlc_phy_txpower_limit_set(wlc->band->pi, &txpwr, wlc->chanspec);
	}
}

/* reset the quiet channels vector to the union of the restricted and radar channel sets */
void
wlc_quiet_channels_reset(wlc_cm_info_t *wlc_cm)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	uint i;
	wlcband_t *band;

	/* initialize quiet channels for restricted channels */
	bcopy(&wlc_cm->restricted_channels, &wlc_cm->quiet_channels, sizeof(chanvec_t));

	band = wlc->band;
	for (i = 0; i < NBANDS(wlc); i++, band = wlc->bandstate[OTHERBANDUNIT(wlc)]) {
#ifdef BAND5G     /* RADAR */
		/* initialize quiet channels for radar if we are in spectrum management mode */
		if (WL11H_ENAB(wlc)) {
			uint j;
			const chanvec_t *chanvec;
			chanvec = wlc_cm->bandstate[band->bandunit].radar_channels;
			for (j = 0; j < sizeof(chanvec_t); j++)
				wlc_cm->quiet_channels.vec[j] |= chanvec->vec[j];
		}
#endif
	}
}

bool
wlc_quiet_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
	return (N_ENAB(wlc_cm->wlc->pub) && CHSPEC_IS40(chspec) ?
		(isset(wlc_cm->quiet_channels.vec, LOWER_20_SB(CHSPEC_CHANNEL(chspec))) ||
		isset(wlc_cm->quiet_channels.vec, UPPER_20_SB(CHSPEC_CHANNEL(chspec)))) :
		isset(wlc_cm->quiet_channels.vec, CHSPEC_CHANNEL(chspec)));
}

void
wlc_set_quiet_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
	if (N_ENAB(wlc_cm->wlc->pub) && CHSPEC_IS40(chspec)) {
		setbit(wlc_cm->quiet_channels.vec, LOWER_20_SB(CHSPEC_CHANNEL(chspec)));
		setbit(wlc_cm->quiet_channels.vec, UPPER_20_SB(CHSPEC_CHANNEL(chspec)));
	} else
		setbit(wlc_cm->quiet_channels.vec, CHSPEC_CHANNEL(chspec));
}

void
wlc_clr_quiet_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
	if (N_ENAB(wlc_cm->wlc->pub) && CHSPEC_IS40(chspec)) {
		clrbit(wlc_cm->quiet_channels.vec, LOWER_20_SB(CHSPEC_CHANNEL(chspec)));
		clrbit(wlc_cm->quiet_channels.vec, UPPER_20_SB(CHSPEC_CHANNEL(chspec)));
	} else
		clrbit(wlc_cm->quiet_channels.vec, CHSPEC_CHANNEL(chspec));
}

/* Is the channel valid for the current locale? (but don't consider channels not
 *   available due to bandlocking)
 */
bool
wlc_valid_channel20_db(wlc_cm_info_t *wlc_cm, uint val)
{
	wlc_info_t *wlc = wlc_cm->wlc;

	return (VALID_CHANNEL20(wlc, val) ||
		(!wlc->bandlocked && VALID_CHANNEL20_IN_BAND(wlc, OTHERBANDUNIT(wlc), val)));
}

/* Is the channel valid for the current locale and specified band? */
bool
wlc_valid_channel20_in_band(wlc_cm_info_t *wlc_cm, uint bandunit, uint val)
{
	return ((val < MAXCHANNEL) && isset(wlc_cm->bandstate[bandunit].valid_channels.vec, val));
}

/* Is the channel valid for the current locale and current band? */
bool
wlc_valid_channel20(wlc_cm_info_t *wlc_cm, uint val)
{
	wlc_info_t *wlc = wlc_cm->wlc;

	return ((val < MAXCHANNEL) &&
		isset(wlc_cm->bandstate[wlc->band->bandunit].valid_channels.vec, val));
}

/* Is the 40 MHz allowed for the current locale and specified band? */
bool
wlc_valid_40chanspec_in_band(wlc_cm_info_t *wlc_cm, uint bandunit)
{
	wlc_info_t *wlc = wlc_cm->wlc;

	return (((wlc_cm->bandstate[bandunit].locale_flags & (WLC_NO_MIMO | WLC_NO_40MHZ)) == 0) &&
		wlc->bandstate[bandunit]->mimo_cap_40);
}


/* Set tx power limits */
/* BMAC_NOTE: this only needs a chanspec so that it can choose which 20/40 limits
 * to save in phy state. Would not need this if we ether saved all the limits and
 * applied them only when we were on the correct channel, or we restricted this fn
 * to be called only when on the correct channel.
 */
static void
wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm_info_t *wlc_cm,
	txppr_t *txpwr, uint8 local_constraint_qdbm)
{
	int j;
#ifdef WL11N
	bool ishtphy = WLCISHTPHY(wlc_cm->wlc->band);
	uint8 *ptxpwr;
#endif
	/* CCK Rates */
	for (j = 0; j < WL_NUM_RATES_CCK; j++) {
		txpwr->cck[j] = MIN(txpwr->cck[j], local_constraint_qdbm);
	}

	/* 20 MHz Legacy OFDM SISO */
	for (j = 0; j < WL_NUM_RATES_OFDM; j++) {
		txpwr->ofdm[j] = MIN(txpwr->ofdm[j], local_constraint_qdbm);
	}

#ifdef WL11N
	/* 20 MHz Legacy OFDM CDD */
	for (j = 0; j < WL_NUM_RATES_OFDM; j++) {
		txpwr->ofdm_cdd[j] = MIN(txpwr->ofdm_cdd[j], local_constraint_qdbm);
	}

	/* 40 MHz Legacy OFDM SISO */
	for (j = 0; j < WL_NUM_RATES_OFDM; j++) {
		txpwr->ofdm_40[j] = MIN(txpwr->ofdm_40[j], local_constraint_qdbm);
	}

	/* 40 MHz Legacy OFDM CDD */
	for (j = 0; j < WL_NUM_RATES_OFDM; j++) {
		txpwr->ofdm_40_cdd[j] = MIN(txpwr->ofdm_40_cdd[j], local_constraint_qdbm);
	}

	/* 20MHz MCS 0-7 SISO or 1 Nsts to 1 Tx */
	ptxpwr = ishtphy ? txpwr->u20.ht.s1x1 : txpwr->u20.n.siso;
	for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++) {
		ptxpwr[j] = MIN(ptxpwr[j], local_constraint_qdbm);
	}

	/* 20MHz MCS 0-7 CDD or 1 Nsts to 2 Tx */
	ptxpwr = ishtphy ? txpwr->u20.ht.s1x2 : txpwr->u20.n.cdd;
	for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++) {
		txpwr->u20.n.cdd[j] = MIN(ptxpwr[j], local_constraint_qdbm);
	}

	/* 20MHz MCS 0-7 STBC or 1 Nsts to 3 Tx */
	ptxpwr = ishtphy ? txpwr->ht.u20s1x3 : txpwr->u20.n.stbc;
	for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++) {
		ptxpwr[j] = MIN(ptxpwr[j], local_constraint_qdbm);
	}

	/* 20MHz MCS 8-15 MIMO or 2 Nsts to 2 Tx */
	ptxpwr = ishtphy ? txpwr->u20.ht.s2x2 : txpwr->u20.n.sdm;
	for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++)
		ptxpwr[j] = MIN(ptxpwr[j], local_constraint_qdbm);

	/* 20MHz 2 Nsts to 3 Tx & 3 Nsts to 3 Tx */
	if (ishtphy) {
		ptxpwr = txpwr->ht.u20s2x3;
		for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++) {
			ptxpwr[j] = MIN(ptxpwr[j], local_constraint_qdbm);
		}
		ptxpwr = txpwr->u20.ht.s3x3;
		for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++) {
			ptxpwr[j] = MIN(ptxpwr[j], local_constraint_qdbm);
		}
	}

	/* 40MHz MCS 0-7 SISO or 1 Nsts to 1 Tx */
	ptxpwr = ishtphy ? txpwr->u40.ht.s1x1 : txpwr->u40.n.siso;
	for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++) {
		ptxpwr[j] = MIN(ptxpwr[j], local_constraint_qdbm);
	}

	/* 40MHz MCS 0-7 CDD or 1 Nsts to 2 Tx */
	ptxpwr = ishtphy ? txpwr->u40.ht.s1x2 : txpwr->u40.n.cdd;
	for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++) {
		ptxpwr[j] = MIN(ptxpwr[j], local_constraint_qdbm);
	}

	/* 40MHz MCS 0-7 STBC or 1 Nsts to 3 Tx */
	ptxpwr = ishtphy ? txpwr->ht.u40s1x3 : txpwr->u40.n.stbc;
	for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++) {
		ptxpwr[j] = MIN(ptxpwr[j], local_constraint_qdbm);
	}

	/* 40MHz MCS 8-15 MIMO or 2 Nsts to 2 Tx */
	ptxpwr = ishtphy ? txpwr->u40.ht.s2x2 : txpwr->u40.n.sdm;
	for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++)
		ptxpwr[j] = MIN(ptxpwr[j], local_constraint_qdbm);

	/* 20MHz 2 Nsts to 3 Tx & 3 Nsts to 3 Tx */
	if (ishtphy) {
		ptxpwr = txpwr->ht.u40s2x3;
		for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++) {
			ptxpwr[j] = MIN(ptxpwr[j], local_constraint_qdbm);
		}
		ptxpwr = txpwr->u40.ht.s3x3;
		for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++) {
			ptxpwr[j] = MIN(ptxpwr[j], local_constraint_qdbm);
		}

		/* 20in40MHz CCK */
		for (j = 0; j < WL_NUM_RATES_CCK; j++)
			txpwr->cck_20ul[j] = MIN(txpwr->cck_20ul[j], local_constraint_qdbm);

		/* 20in40MHz OFDM */
		for (j = 0; j < WL_NUM_RATES_OFDM; j++)
			txpwr->ofdm_20ul[j] = MIN(txpwr->ofdm_20ul[j], local_constraint_qdbm);

		/* 20in40MHz OFDM CDD */
		for (j = 0; j < WL_NUM_RATES_OFDM; j++)
			txpwr->ofdm_20ul_cdd[j] = MIN(txpwr->ofdm_20ul_cdd[j],
				local_constraint_qdbm);

		/* 20in40MHz 1 Nsts to 1 Tx */
		for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++)
			txpwr->ht20ul.s1x1[j] = MIN(txpwr->ht20ul.s1x1[j], local_constraint_qdbm);

		/* 20in40MHz 1 Nsts to 2 Tx */
		for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++)
			txpwr->ht20ul.s1x2[j] = MIN(txpwr->ht20ul.s1x2[j], local_constraint_qdbm);

		/* 20in40MHz 1 Nsts to 3 Tx */
		for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++)
			txpwr->ht.ul20s1x3[j] = MIN(txpwr->ht.ul20s1x3[j], local_constraint_qdbm);

		/* 20in40MHz 2 Nsts to 2 Tx */
		for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++)
			txpwr->ht20ul.s2x2[j] = MIN(txpwr->ht20ul.s2x2[j], local_constraint_qdbm);

		/* 40MHz MCS 2 Nsts to 3 Tx */
		for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++)
			txpwr->ht.ul20s2x3[j] = MIN(txpwr->ht.ul20s2x3[j], local_constraint_qdbm);

		/* 40MHz MCS 3 Nsts to 3 Tx */
		for (j = 0; j < WL_NUM_RATES_MCS_1STREAM; j++)
			txpwr->ht20ul.s3x3[j] = MIN(txpwr->ht20ul.s3x3[j], local_constraint_qdbm);
	}
	/* 40MHz MCS 32 */
	txpwr->mcs32 = MIN(txpwr->mcs32, local_constraint_qdbm);
#endif /* WL11N */

}

static void
wlc_channel_txpower_limits(wlc_cm_info_t *wlc_cm, txppr_t *txpwr)
{
	uint8 local_constraint;
	wlc_info_t *wlc = wlc_cm->wlc;

	local_constraint = wlc_local_constraint_qdbm(wlc);

	wlc_channel_reg_limits(wlc_cm, wlc->chanspec, txpwr);

	wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm, txpwr, local_constraint);
}

void
wlc_channel_set_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chanspec, uint8 local_constraint_qdbm)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	txppr_t txpwr;

	wlc_channel_reg_limits(wlc_cm, chanspec, &txpwr);

	wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm, &txpwr, local_constraint_qdbm);

	wlc_channel_update_txchain_offsets(wlc_cm, &txpwr);

	wlc_bmac_set_chanspec(wlc->hw, chanspec, (wlc_quiet_chanspec(wlc_cm, chanspec) != 0),
		&txpwr);
}

int
wlc_channel_set_txpower_limit(wlc_cm_info_t *wlc_cm, uint8 local_constraint_qdbm)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	txppr_t txpwr;

	wlc_channel_reg_limits(wlc_cm, wlc->chanspec, &txpwr);

	wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm, &txpwr, local_constraint_qdbm);

	wlc_channel_update_txchain_offsets(wlc_cm, &txpwr);

	wlc_phy_txpower_limit_set(wlc->band->pi, &txpwr, wlc->chanspec);

	return 0;
}

void
wlc_channel_reg_limits(wlc_cm_info_t *wlc_cm, chanspec_t chanspec, txppr_t *txpwr)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	uint i;
	uint chan;
	int maxpwr;
	int delta;
	const country_info_t *country;
	wlcband_t * band;
	const locale_info_t * li;
	int conducted_max;
	int conducted_ofdm_max;
#ifdef WL11N
	const locale_mimo_info_t *li_mimo;
	int maxpwr20, maxpwr40;
	int maxpwr_idx;
	uint j;
#endif /* WL11N */
	bool is_4331 = FALSE;
	bool filt_war = FALSE;

	if ((wlc->pub->sih->boardvendor == VENDOR_APPLE) &&
	    ((wlc->pub->sih->boardtype == BCM94331X19) ||
	     (wlc->pub->sih->boardtype == BCM94331PCIEBT3Ax_SSID)))
		is_4331 = TRUE;

	bzero(txpwr, sizeof(txppr_t));


	/* Lookup channel in autocountry_default if not in current country */
	if (!wlc_valid_chanspec_db(wlc_cm, chanspec)) {
		country = wlc_country_lookup(wlc, wlc->autocountry_default);
		if (country == NULL)
			return;
	}
	else
		country = wlc_cm->country;
	chan = CHSPEC_CHANNEL(chanspec);
	band = wlc->bandstate[CHSPEC_WLCBANDUNIT(chanspec)];
	li = BAND_5G(band->bandtype) ?
		wlc_get_locale_5g(country->locale_5G) :
		wlc_get_locale_2g(country->locale_2G);

#ifdef WL11N
	li_mimo = BAND_5G(band->bandtype) ?
	        wlc_get_mimo_5g(country->locale_mimo_5G) :
		wlc_get_mimo_2g(country->locale_mimo_2G);
#endif /* WL11N */

	/* The maxtxpwr for a locale may be specified using either a conducted limit
	 * (measured before the antenna) or EIRP (measured after the antenna).
	 * The WLC_EIRP flag is used to distinguish between the two cases.
	 * The max power limits returned by this routine are all conducted values so
	 * they might need to be adjusted to account for the board's antenna gain.
	 */

	/* Compute a delta to remove from the locale table value.  In the EIRP case this
	 * is simply the antenna gain and would typically be 0 in the conducted case.  But
	 * because we/Broadcom do not want to allow more than a 6 dB antennal gain to a
	 * conducted locale's table value, the delta to remove can be the excess over 6 dB
	 * of antenna gain.
	 */

	if (li->flags & WLC_EIRP) {
		delta = band->antgain;
	} else {
		delta = 0;
		if (band->antgain > QDB(6) && !is_4331)
			delta = band->antgain - QDB(6); /* Excess over 6 dB */
	}

	if (is_4331 && (li->flags & WLC_FILT_WAR) && (chan == 1 || chan == 13))
			filt_war = TRUE;
	wlc_bmac_filter_war_upd(wlc->hw, filt_war);

	/* Need to set the txpwr_local_max to external reg max for
	 * this channel as per the locale selected for AP.
	 */
	if (AP_ONLY(wlc->pub)) {
		wlc->txpwr_local_max = wlc_get_reg_max_power_for_channel(wlc->cmi,
			CHSPEC_CHANNEL(wlc->chanspec), TRUE);
	}

	/* Some EIRP locales also have a conducted CCK or OFDM limit. */
	if (li == &locale_g) {
		conducted_max = QDB(22);
		conducted_ofdm_max = 82; /* 20.5 dBm */
	} else if (li == &locale_h || li == &locale_i) {
		conducted_max = QDB(22);
		conducted_ofdm_max = QDB(22);
	} else {
		conducted_max = WLC_TXPWR_MAX;
		conducted_ofdm_max = WLC_TXPWR_MAX;
	}

	/* CCK txpwr limits for 2.4G band */
	if (BAND_2G(band->bandtype)) {
		maxpwr = li->maxpwr[CHANNEL_POWER_IDX_2G_CCK(chan)];

		/* special cases for 2.4G CCK */
		if (li == &locale_a2) {
			if (chan == 11)
				maxpwr = QDB(19);
			else if (chan == 14)
				maxpwr = QDB(16);
		} else if (li == &locale_b2) {
			if (chan == 14)
				maxpwr = 54;  /* 13.5 dBm */
		} else if (li == &locale_b2_1 || li == &locale_b2_2) {
			if (chan == 11 || chan == 14)
				maxpwr = QDB(16);
		} else if (li == &locale_b2_6) {
			if (chan == 11)
				maxpwr = QDB(16);
		} else if (li == &locale_b5) {
			if (chan == 14)
				maxpwr = 42; /* 10.5 dBm */
		} else if (li == &locale_a4 ||
			li == &locale_a4_2 ||
			li == &locale_a4_3) {
			if (chan == 2)
				maxpwr = QDB(22);
			else if (chan == 10)
				maxpwr = QDB(21);
		} else if (li == &locale_a4_4) {
			if ((chan == 2) || (chan == 10))
				maxpwr = 98; /* 24.5 dBm */
		} else if (li == &locale_a5) {
			if (chan == 10)
				maxpwr = QDB(20);
		} else if (li == &locale_b5_2) {
			if (chan == 14)
				maxpwr = QDB(10);
		} else if (li == &locale_b5_3 || li == &locale_c_2) {
			if (chan == 14)
				maxpwr = QDB(15);
		} else if (li == &locale_a12) {
			if (chan == 7 || chan == 8)
				maxpwr = QDB(16);
			if (chan == 9)
				maxpwr = QDB(12);
			if (chan == 10)
				maxpwr = QDB(15);
		}

		maxpwr = maxpwr - delta;
		maxpwr = MAX(maxpwr, 0);
		maxpwr = MIN(maxpwr, conducted_max);

		for (i = 0; i < WL_NUM_RATES_CCK; i++)
			txpwr->cck[i] = (uint8)maxpwr;
	}

	/* Some locales is EIRP for CCK and conducted for OFDM, fix for OFDM now */
	if (li == &locale_b3 || li == &locale_b4 || li == &locale_k ||
		li == &locale_b_1) {
		delta = 0;
		if (band->antgain > QDB(6))
			delta = band->antgain - QDB(6); /* Excess over 6 dB */
	}

	/* OFDM txpwr limits for 2.4G or 5G bands */
	if (BAND_2G(band->bandtype)) {
		maxpwr = li->maxpwr[CHANNEL_POWER_IDX_2G_OFDM(chan)];

		/* special cases for 2.4G OFDM */
		if (li == &locale_a2) {
			if (chan == 11)
				maxpwr = 66; /* 16.5 dBm */
		} else if (li == &locale_b2) {
			if (chan == 11)
				maxpwr = 70;  /* 17.5 dBm */
		} else if (li == &locale_b2_1 || li == &locale_b2_2) {
			if (chan == 11)
				maxpwr = QDB(16);
		} else if (li == &locale_b2_3 || li == &locale_b2_5) {
			if (chan == 13)
				maxpwr = 46 /* 11.5 dBm */;
		} else if (li == &locale_b2_6) {
			if (chan == 11)
				maxpwr = QDB(16);
		} else if (li == &locale_k) {
			if (chan == 13)
				maxpwr = QDB(14);
		} else if (li == &locale_b5) {
			if (chan == 11)
				maxpwr = 62; /* 15.5 dBm */
		} else if (li == &locale_a4 || li == &locale_a4_2) {
			if (chan == 2 || chan == 10)
				maxpwr = QDB(20);
		} else if (li == &locale_a4_3) {
			if (chan == 2 || chan == 10)
				maxpwr = QDB(19);
		} else if (li == &locale_a4_4) {
			if (chan == 2 || chan == 10)
				maxpwr = 86; /* 21.5 dBm */
		} else if (li == &locale_a5) {
			if (chan == 2 || chan == 10)
				maxpwr = QDB(19);
		} else if (li == &locale_a5_1) {
			if (chan == 2 || chan == 10)
				maxpwr = QDB(16);
		} else if (li == &locale_a6_2) {
			if (chan == 2 || chan == 10)
				maxpwr = QDB(18);
		} else if (li == &locale_a6 || li == &locale_a6_1) {
			if (chan == 2 || chan == 10)
				maxpwr = QDB(20);
		} else if (li == &locale_a9) {
			if (chan == 2 || chan == 10)
				maxpwr = QDB(16);
			else if (chan == 3 || chan == 9)
				maxpwr = 70;  /* 17.5 dBm */
		} else if (li == &locale_b5_2) {
			if (chan == 12 || chan == 13)
				maxpwr = QDB(15);
		} else if (li == &locale_b5_3) {
			if (chan == 11)
				maxpwr = 51; /* 12.75 */
		} else if (li == &locale_i_1) {
			if (chan == 12)
				maxpwr = QDB(16);
			if (chan == 13)
				maxpwr = QDB(14);
		} else if (li == &locale_a3_1) {
			if (chan == 2)
				maxpwr = QDB(16);
			else if (chan == 10)
				maxpwr = QDB(15);
		} else if (li == &locale_a12) {
			if (chan == 2)
				maxpwr = QDB(11);
			else if (chan == 3 || chan == 8)
				maxpwr = QDB(14);
			else if (chan == 4 || chan == 7)
				maxpwr = QDB(15);
			else if (chan == 5 || chan == 6)
				maxpwr = QDB(16);
			else if (chan == 9)
				maxpwr = QDB(12);
		}
	} else {
		maxpwr = li->maxpwr[CHANNEL_POWER_IDX_5G(chan)];

#ifdef BAND5G
		/* special cases for 5G OFDM */
		if (li == &locale_8b) {
			if (chan == 100) {
				maxpwr = 58; /* 14.5 dBm */
			} else if (chan >= 104 && chan <= 136) {
				maxpwr = QDB(17);
			} else if (chan == 140) {
				maxpwr = QDB(15);
			}
		} else if (li == &locale_29b) {
			if (chan == 100) {
				maxpwr = 62; /* 15.5 dBm */
			} else if (chan >= 132 && chan <= 140) {
				maxpwr = QDB(20);
			}
		} else if ((li == &locale_29b_1) || (li == &locale_29b_3))  {
			if (chan == 100) {
				maxpwr = 74; /* 18.5 dBm */
			} else if (chan == 140) {
				maxpwr = QDB(17);
			}
		} else if (li == &locale_29d) {
			if (chan >= 132 && chan <= 140)
				maxpwr = 78 /* 19.5 */;
		} else if (li == &locale_11_2) {
			if (chan == 100) {
				maxpwr = QDB(14);
			}
		} else if (li == &locale_19_1) {
			if (chan >= 104 && chan <= 136) {
				maxpwr = QDB(17);
			} else if (chan == 140) {
				maxpwr = 50; /* 12.5 dBm */
			}
		} else if (li == &locale_19_2) {
			if (chan == 100 || chan == 140) {
				maxpwr = QDB(16);
			} else if (chan == 149 || chan == 165) {
				maxpwr = 78 /* 19.5 */;
			}
		} else if (li == &locale_19h_1) {
			if (chan == 140) {
				maxpwr = 58 /* 14.5 */;
			}
		} else if (li == &locale_19_3) {
			if (chan == 100) {
				maxpwr = QDB(13);
			}
		} else if (li == &locale_19_4) {
			if (chan == 36) {
				maxpwr = QDB(13);
			}
			if (chan == 100) {
				maxpwr = 62 /* 15.5 dBm */;
			}
		} else if (li == &locale_19a_1) {
			if (chan == 100) {
				maxpwr = QDB(14);
			}
		} else if (li == &locale_19l_1) {
			if (chan == 140) {
				maxpwr = QDB(15);
			}
		} else if (li == &locale_19l_2) {
			if (chan == 100) {
				maxpwr = /* 14.5 dBm */ 58;
			} else if (chan == 140) {
				maxpwr = QDB(15);
			}
		} else if (li == &locale_3l) {
			if (chan >= 132 && chan <= 140) {
				maxpwr = 54;  /* 13.5 dBm */
			}
		}
#endif /* BAND5G */
	}

	maxpwr = maxpwr - delta;
	maxpwr = MAX(maxpwr, 0);
	maxpwr = MIN(maxpwr, conducted_ofdm_max);

	/* Keep OFDM lmit below CCK limit */
	if (BAND_2G(band->bandtype))
		maxpwr = MIN(maxpwr, txpwr->cck[0]);

	for (i = 0; i < WL_NUM_RATES_OFDM; i++) {
		txpwr->ofdm[i] = (uint8)maxpwr;
	}

#ifdef WL11N
	for (i = 0; i < WL_NUM_RATES_OFDM; i++) {
		/* OFDM 40 MHz SISO has the same power as the corresponding MCS0-7 rate unless
		 * overriden by the locale specific code. We set this value to 0 as a
		 * flag (presumably 0 dBm isn't a possibility) and then copy the MCS0-7 value
		 * to the 40 MHz value if it wasn't explicitly set.
		 */
		txpwr->ofdm_40[i] = 0;

		txpwr->ofdm_cdd[i] = (uint8)maxpwr;

		/* OFDM 40 MHz CDD has the same power as the corresponding MCS0-7 rate unless
		 * overriden by the locale specific code. We set this value to 0 as a
		 * flag (presumably 0 dBm isn't a possibility) and then copy the MCS0-7 value
		 * to the OFDM value if it wasn't explicitly set.
		 */
		txpwr->ofdm_40_cdd[i] = 0;
	}
#endif /* WL11N */

#ifdef WL11N
	/* MIMO/HT specific limits */

	/* Compute a delta to remove from the locale table value.  In the EIRP case this
	 * is simply the antenna gain and would typically be 0 in the conducted case.  But
	 * because we/Broadcom do not want to allow more than a 6 dB antennal gain to a
	 * conducted locale's table value, the delta to remove can be the excess over 6 dB
	 * of antenna gain.
	 */

	if (li_mimo->flags & WLC_EIRP) {
		delta = band->antgain;
	} else {
		delta = 0;
		if (band->antgain > QDB(6))
			delta = band->antgain - QDB(6); /* Excess over 6 dB */
	}

	if (BAND_2G(band->bandtype))
		maxpwr_idx = (chan - 1);
	else
		maxpwr_idx = CHANNEL_POWER_IDX_5G(chan);

	maxpwr20 = li_mimo->maxpwr20[maxpwr_idx];
	maxpwr40 = li_mimo->maxpwr40[maxpwr_idx];

	if (li_mimo == &locale_3bn) {
		if (chan == 100) {
			maxpwr20 = QDB(20);
			maxpwr40 = QDB(18);
		}
	}

	if (li_mimo == &locale_8an) {
		if (chan >= 100 && chan <= 102) {
			maxpwr40 = 62; /* 15.5 dBm */
		}
	}

	if (li_mimo == &locale_8bn) {
		if (chan >= 100 && chan <= 102) {
			maxpwr40 = 62; /* 15.5 dBm */
		}
	}

	/* Fix channel only splits here */
	if (li_mimo == &locale_8cn) {
		if (chan >= 100 && chan <= 108) {
			maxpwr20 = QDB(13);
			maxpwr40 = QDB(12);
		} else if (chan >= 110 && chan <= 132) {
			maxpwr20 = 58; /* 14.5 dBm */
			maxpwr40 = QDB(17);
		} else if (chan >= 134 && chan <= 136) {
			maxpwr20 = 58; /* 14.5 dBm */
			maxpwr20 = 62; /* 15.5 dBm */
		} else if (chan == 140) {
			maxpwr20 = 54; /* 13.5 dBm */
			maxpwr40 = QDB(18); /* 13.5 dBm */
		}
	}
	if (li_mimo == &locale_19n ||
		li_mimo == &locale_19an ||
		li_mimo == &locale_19bn ||
		li_mimo == &locale_19hn ||
		li_mimo == &locale_19ln ||
		li_mimo == &locale_19rn) {
		/* Fixup 100-102, 104-140 channel split */
		if (chan >= 104 && chan <= 140) {
			if (li_mimo == &locale_19bn ||
				li_mimo == &locale_19hn ||
				li_mimo == &locale_19rn)
				maxpwr20 = 66; /* 16.5 dBm */
			else
				maxpwr20 = QDB(17);

			if (li_mimo == &locale_19an ||
				li_mimo == &locale_19ln)
				maxpwr40 = QDB(17);
			else
				maxpwr40 = 74; /* 18.5 dBm */
		}
	}

	if (li_mimo == &locale_19hn_2) {
		if (chan >= 100 && chan <= 102) {
			maxpwr20 = 82; /* 20.5 */
			maxpwr40 = QDB(18);
		} else if (chan == 60) {
			maxpwr20 = 82; /* 20.5 */
			maxpwr40 = 86; /* 21.5 */
		}
	}

	if (li_mimo == &locale_19n_1) {
		if (chan >= 104 && chan <= 136) {
			maxpwr20 = QDB(17);
			maxpwr40 = QDB(16);
		} else if (chan == 140) {
			maxpwr20 = 54; /* 13.5 dBm */
			maxpwr40 = 0;
		}
	}

	if (li_mimo == &locale_19n_2) {
		if (chan >= 100 && chan <= 102) {
			maxpwr20 = QDB(13);
			maxpwr40 = QDB(13);
		} else if (chan >= 128 && chan <= 136) {
			maxpwr20 = QDB(14);
			maxpwr40 = QDB(13);
		} else if (chan >= 159 && chan <= 161) {
			maxpwr20 = 82 /* 20.5 */;
			maxpwr40 = 82;
		} else if (chan == 140) {
			maxpwr20 = QDB(13);
			maxpwr40 = 0;
		} else if (chan == 165) {
			maxpwr20 = 78 /* 19.5 */;
			maxpwr40 = 0;
		} else if (chan == 149) {
			maxpwr20 = QDB(18);
			maxpwr40 = 0;
		}
	}

	if (li_mimo == &locale_19n_3) {
		if (chan >= 40 && chan <= 48) {
			maxpwr20 = 58; /* 14.5 dBm */
			maxpwr40 = 58; /* 14.5 dBm */
		} else if (chan >= 104 && chan <= 140) {
			maxpwr20 = 58; /* 14.5 dBm */
			maxpwr40 = 54; /* 13.5 dBm */
		}
	}

	if (li_mimo == &locale_29bn) {
		if (chan == 100) {
			maxpwr20 = QDB(16);
			maxpwr40 = QDB(13);
		} else if (chan >= 104 && chan <= 116) {
			maxpwr20 = QDB(16);
			maxpwr40 = QDB(15);
		}
	}

	if (li_mimo == &locale_29dn) {
		if (chan >= 132 && chan <= 140) {
			maxpwr20 = 74 /* 18.5 */;
			maxpwr40 = 62 /* 15.5 */;
		}
	}

	if (li_mimo == &locale_19hn_1) {
		if (chan == 140) {
			maxpwr20 = 58 /* 14.5 */;
			maxpwr40 = 0;
		} else if (chan >= 100 && chan <= 102) {
			maxpwr20 = 58 /* 14.5 */;
			maxpwr40 = 58 /* 14.5 */;
		} else if (chan >= 134 && chan <= 136) {
			maxpwr20 = 70 /* 17.5 */;
			maxpwr40 = 70 /* 17.5 */;
		}
	}

	if (li_mimo == &locale_29bn_1) {
		if (chan == 100) {
			maxpwr20 = QDB(18);
			maxpwr40 = QDB(16);
		} else if (chan == 140) {
			maxpwr20 = 66 /* 16.5 */;
			maxpwr40 = 0;
		} else if (chan >= 132 && chan <= 136) {
			maxpwr40 = 74; /* 18.5 dBm */
		}
	}

	if (li_mimo == &locale_29bn_3) {
		if (chan == 100) {
			maxpwr20 = QDB(18);
			maxpwr40 = QDB(16);
		} else if (chan == 140) {
			maxpwr20 = 66 /* 16.5 */;
			maxpwr40 = 0;
		} else if (chan >= 132 && chan <= 136) {
			maxpwr40 = QDB(19);
		}
	}

	if (li_mimo == &locale_19ln_1) {
		if (chan >= 100 && chan <= 108) {
			maxpwr20 = QDB(13);
			maxpwr40 = QDB(12);
		}
		if (chan >= 110 && chan <= 132) {
			maxpwr40 = 70; /* 18.5 dBm */
		}
		if (chan >= 134 && chan <= 136) {
			maxpwr40 = 62; /* 15.5 dBm */
		}
		if (chan == 140) {
			maxpwr20 = 54; /* 13.5 dBm */
			maxpwr40 = QDB(18);
		}
		if (chan >= 149 && chan < 165) {
			maxpwr20 = QDB(17);
		}
	}

	if (li_mimo == &locale_19ln_2) {
		if (chan >= 110 && chan <= 132) {
			maxpwr20 = /* 14.5 dBm */ 58;
			maxpwr40 = QDB(17);
		}
		if (chan >= 134 && chan <= 136) {
			maxpwr20 = /* 14.5 dBm */ 58;
			maxpwr40 = 62; /* 15.5 dBm */
		}
		if (chan == 140) {
			maxpwr20 = 54; /* 13.5 dBm */
			maxpwr40 = QDB(18);
		}
	}

	if (li_mimo == &locale_19ln_3) {
		if (chan >= 100 && chan <= 102) {
			maxpwr20 = QDB(16);
			maxpwr40 = QDB(13);
		}
	}

	if (li_mimo == &locale_3ln ||
	    li_mimo == &locale_11ln) {
		if (chan >= 132 && chan <= 140) {
			maxpwr20 = QDB(11);
		}
	}

	if (li_mimo == &locale_6an_1) {
		if (chan >= 36 && chan <= 38) {
			maxpwr40 = QDB(13);
		} else if (chan >= 52 && chan <= 54) {
			maxpwr40 = QDB(17);
		}
	}

	if (li_mimo == &locale_8an_1) {
		if (chan >= 100 && chan <= 102) {
			maxpwr20 = QDB(17);
			maxpwr40 = QDB(15);
		}
	}

	if (li_mimo == &locale_29dn_2) {
		if (chan >= 52 && chan <= 54) {
			maxpwr20 = QDB(17);
			maxpwr40 = QDB(17);
		} else if (chan >= 100 && chan <= 108) {
			maxpwr20 = QDB(17);
			maxpwr40 = QDB(15);
		}
	}

	if (li_mimo == &locale_25hn) {
		if ((chan >= 100 && chan <= 124)||
		    (chan >= 149 && chan <= 161)) {
			maxpwr20 = QDB(16);
			maxpwr40 = QDB(16);
		}
	}

	/* CLM v4.6.6 */
	if (li_mimo == &locale_3n_1) {
		if (chan >= 100 && chan <= 102) {
			maxpwr20 = QDB(21);
		}
	}

	maxpwr20 = maxpwr20 - delta;
	maxpwr20 = MAX(maxpwr20, 0);
	maxpwr40 = maxpwr40 - delta;
	maxpwr40 = MAX(maxpwr40, 0);

	/* First fill in the tx power limits based solely on the internal local table
	* structure.  Then modify those values for locales that can't be fully described
	* by the current table structure.
	*/

	/* Fill in the MCS 0-7 (SISO) rates */
	for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {

		/* 20 MHz has the same power as the corresponding OFDM rate unless
		 * overriden by the locale specific code.
		 */
		txpwr->u20.n.siso[i] = txpwr->ofdm[i];

		/* 40 MHz has the same power as the corresponding CDD rate unless
		 * overriden by the locale specific code.  Because we don't know
		 * the CDD value yet, the locale specific code and set it, and
		 * because the locale specific code might want a different
		 * value for MCS 0-7 SISO vs. CDD, we set this value to 0 as a
		 * flag (presumably 0 dBm isn't a possibility) and then copy the CDD value
		 * to the SISO value if it wasn't explicitly set.
		 */
		txpwr->u40.n.siso[i] = 0;
	}

	/* Fill in the MCS 0-7 CDD rates */
	for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
		txpwr->u20.n.cdd[i] = (uint8)maxpwr20;
		txpwr->u40.n.cdd[i] = (uint8)maxpwr40;
	}

	/* These locales have SISO expressed in the table and override CDD later */
	if (li_mimo == &locale_an1_t1 ||
	    li_mimo == &locale_an1_t2 ||
	    li_mimo == &locale_an1_t3 ||
	    li_mimo == &locale_an1_t5 ||
	    li_mimo == &locale_an2_5 ||
	    li_mimo == &locale_an6_1 ||
	    li_mimo == &locale_an6_6 ||
	    li_mimo == &locale_bn2_3 ||
	    li_mimo == &locale_bn2_8 ||
	    li_mimo == &locale_bn ||
	    li_mimo == &locale_cn ||
	    li_mimo == &locale_cn_3 ||
	    li_mimo == &locale_bn2_4 ||
	    li_mimo == &locale_bn_9 ||
	    li_mimo == &locale_an_2 ||
	    li_mimo == &locale_an7_8 ||
	    li_mimo == &locale_kn5) {

		if (li_mimo == &locale_bn2_3) {
			maxpwr20 = QDB(16);
			maxpwr40 = 0;

			if (chan == 3) {
				maxpwr40 = QDB(15);
			} else if (chan >= 4 && chan <= 8) {
				maxpwr40 = QDB(16);
			} else if (chan == 9) {
				maxpwr40 = QDB(14);
			} else if (chan == 12 || chan == 13) {
				maxpwr20 = QDB(11);
			}
		}

		if (li_mimo == &locale_bn2_8) {
			maxpwr20 = QDB(16);
			maxpwr40 = 0;

			if (chan == 3) {
				maxpwr40 = 54; /* 13.5 QDB */
			} else if (chan >= 4 && chan <= 8) {
				maxpwr40 = QDB(16);
			} else if (chan == 9) {
				maxpwr40 = QDB(13);
			} else if (chan == 12 || chan == 13) {
				maxpwr20 = QDB(11);
			}
		}

		if (li_mimo == &locale_bn_9 ||
		    li_mimo == &locale_kn5) {
			maxpwr20 = 66; /* 16.5 dBm */

			if (chan >= 3 && chan <= 11) {
				if (li_mimo == &locale_bn_9)
					maxpwr40 = 54; /* 13.5 QDB */
				else if (li_mimo == &locale_kn5)
					maxpwr40 = 66; /* 16.5 dBm */
			}
		}

		if (li_mimo == &locale_an2_5) {
			if (chan == 1) {
				maxpwr20 = 50; /* 12.5 dBm */
			}
			if (chan == 11) {
				maxpwr20 = 66; /* 16.5 dBm */
			}
			if (chan == 3) {
				maxpwr40 = QDB(11);
			}
			if (chan == 9) {
				maxpwr40 = 58; /* 14.5 dBm */
			}
		}


		if (li_mimo == &locale_bn || li_mimo == &locale_cn) {
			maxpwr20 = QDB(16);
			maxpwr40 = 0;

			if (chan >= 3 && chan <= 11) {
				maxpwr40 = QDB(16);
			}
		}

		if (li_mimo == &locale_cn_3) {
			maxpwr20 = 70; /* 17.5 dBm */
			maxpwr40 = 0;

			if (chan >= 3 && chan <= 11) {
				maxpwr40 = 70; /* 17.5 dBm */
			}
		}

		if (li_mimo == &locale_an6_6) {
			maxpwr20 = QDB(17);
			maxpwr40 = 0;

			if (chan == 11)
				maxpwr20 = QDB(16);
			if (chan == 3 || chan == 9)
				maxpwr40 = QDB(14);
			else if (chan >= 4 && chan <= 8)
				maxpwr20 = QDB(17);
		}

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			txpwr->u20.n.siso[i] = (uint8)maxpwr20;
			txpwr->u40.n.siso[i] = (uint8)maxpwr40;
		}
	}

	/* Fill in the MCS 0-7 STBC rates */
	for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
		/* MCS0-7 20 and 40 MHz STBC rates have the same powers as the corresponding
		 * MCS0-7 20 and 40 MHz CDD rates unless overriden by the locale specific code.
		 * We set this value to 0 as a flag (presumably 0 dBm isn't a possibility)
		 * and then copy the CDD values to the STBC values if they weren't explicitly set.
		 */
		txpwr->u20.n.stbc[i] = 0;
		txpwr->u40.n.stbc[i] = 0;
	}

	/* Fill in the MCS 8-15 SDM rates */
	for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
		txpwr->u20.n.sdm[i] = (uint8)maxpwr20;
		txpwr->u40.n.sdm[i] = (uint8)maxpwr40;
	}

	/* Fill in MCS32 */
	txpwr->mcs32 = (uint8)maxpwr40;

	/* Now override the N enable limits for those locales where
	 * the defaults are not correct.
	 */

	if (li_mimo == &locale_an1_t1 ||
	    li_mimo == &locale_an1_t2 ||
	    li_mimo == &locale_an1_t5) {
		uint max_low_mcs, max_mid_mcs, max_high_mcs;

		/* Fixup CDD/SDM and per-MCS */
		max_low_mcs = 0;
		max_mid_mcs = 0;
		max_high_mcs = 0;

		if (li_mimo == &locale_an1_t1) {
			if (chan == 1) {
				maxpwr20 = QDB(14);
			} else if (chan == 2) {
				maxpwr20 = QDB(16);
			} else if (chan == 3) {
				maxpwr20 = QDB(19);
				max_low_mcs = QDB(13);
				max_mid_mcs = QDB(14);
				max_high_mcs = 58; /* 14.5 dBm */
			} else if (chan >= 4 && chan <= 8) {
				maxpwr20 = QDB(19);
				max_low_mcs = QDB(14);
				max_mid_mcs = QDB(15);
				max_high_mcs = 62; /* 15.5 dBm */
			} else if (chan == 9) {
				maxpwr20 = QDB(19);
				max_low_mcs = QDB(12);
				max_mid_mcs = QDB(13);
				max_high_mcs = 54; /* 13.5 dBm */
			} else if (chan == 10) {
				maxpwr20 = QDB(16);
			} else if (chan == 11) {
				maxpwr20 = QDB(14);
			}
		} else if (li_mimo == &locale_an1_t2) {
			if (chan == 1) {
				maxpwr20 = QDB(14);
			} else if (chan == 2) {
				maxpwr20 = 62; /* 15.5 dBm */
			} else if (chan == 3) {
				maxpwr20 = QDB(19);
				max_low_mcs = 50; /* 12.5 dBm */
				max_mid_mcs = 54; /* 13.5 dBm */
				max_high_mcs = QDB(14);
			} else if (chan >= 4 && chan <= 8) {
				maxpwr20 = QDB(19);
				max_low_mcs = QDB(13);
				max_mid_mcs = QDB(14);
				max_high_mcs = 58; /* 14.5 dBm */
			} else if (chan == 9) {
				maxpwr20 = 74; /* 18.5 dBm */
				max_low_mcs = QDB(12);
				max_mid_mcs = QDB(13);
				max_high_mcs = 54; /* 13.5 dBm */
			} else if (chan == 10) {
				maxpwr20 = 62; /* 15.5 dBm */
			} else if (chan == 11) {
				maxpwr20 = 54; /* 13.5 dBm */
			}
		} else if (li_mimo == &locale_an1_t5) {
			if (chan == 1) {
				maxpwr20 = QDB(11);
			} else if (chan == 2) {
				maxpwr20 = QDB(15);
			} else if (chan >= 3 && chan <= 8) {
				maxpwr20 = QDB(17);
			} else if (chan == 9) {
				maxpwr20 = 66; /* 16.5 dBm */
			} else if (chan == 10) {
				maxpwr20 = QDB(15);
			} else if (chan == 11) {
				maxpwr20 = 42; /* 10.5 dBm */
			}
		}

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			txpwr->u20.n.cdd[i] = (uint8)maxpwr20;

			/* MCS 0-4 are in the low set
			 * MCS 5-7 are in the mid set
			 */
			if (i <= 4)
				txpwr->u40.n.cdd[i] = (uint8)max_low_mcs;
			else
				txpwr->u40.n.cdd[i] = (uint8)max_mid_mcs;
		}

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			txpwr->u20.n.sdm[i] = (uint8)maxpwr20;

			/* MCS  8-11 (i: 0-3) are in the mid set
			 * MCS 12-15 (i: 4-7) are in the high set
			 */
			if (i <= 3)
				txpwr->u40.n.sdm[i] = (uint8)max_mid_mcs;
			else
				txpwr->u40.n.sdm[i] = (uint8)max_high_mcs;
		}
	}

	/* Fixup 40Mhz per-mcs limits */
	if (li_mimo == &locale_an1_t4 ||
	    li_mimo == &locale_an6 ||
	    li_mimo == &locale_an6_2) {
		uint max_low_mcs, max_mid_mcs, max_high_mcs;

		/* Fixup per-MCS */
		max_low_mcs = 0;
		max_mid_mcs = 0;
		max_high_mcs = 0;

		if (li_mimo == &locale_an1_t4) {
			if (chan == 3 || chan == 9) {
				max_low_mcs = QDB(10);
				max_mid_mcs = QDB(11);
				max_high_mcs = 46; /* 11.5 dBm */
			} else if (chan >= 4 && chan <= 8) {
				max_low_mcs = QDB(12);
				max_mid_mcs = QDB(13);
				max_high_mcs = 54; /* 13.5 dBm */
			}
		} else if (li_mimo == &locale_an6) {
			if (chan == 3) {
				max_low_mcs = 66; /* 16.5 dBm */
				max_mid_mcs = 70; /* 17.5 dBm */
				max_high_mcs = QDB(18);
			} else if (chan == 4) {
				max_low_mcs = 70; /* 17.5 dBm */
				max_mid_mcs = 74; /* 18.5 dBm */
				max_high_mcs = QDB(19);
			} else if (chan >= 5 && chan <= 7) {
				max_low_mcs = QDB(19);
				max_mid_mcs = QDB(20);
				max_high_mcs = 82; /* 20.5 dBm */
			} else if (chan == 8) {
				max_low_mcs = 70; /* 17.5 dBm */
				max_mid_mcs = 74; /* 18.5 dBm */
				max_high_mcs = QDB(19);
			} else if (chan == 9) {
				max_low_mcs = 62; /* 15.5 dBm */
				max_mid_mcs = 66; /* 16.5 dBm */
				max_high_mcs = QDB(17);
			}
		} else if (li_mimo == &locale_an6_2) {
			if (chan == 3) {
				max_low_mcs = 46; /* 11.5 dBm */
				max_mid_mcs = QDB(12);
				max_high_mcs = 50; /* 12.5 dBm */
			} else if (chan == 4) {
				max_low_mcs = QDB(12);
				max_mid_mcs = 50; /* 12.5 dBm */
				max_high_mcs = QDB(13);
			} else if (chan >= 5 && chan <= 7) {
				max_low_mcs = 50; /* 12.5 dBm */
				max_mid_mcs = 54; /* 13.5 dBm */
				max_high_mcs = 58; /* 14.5 dBm */
			} else if (chan == 8) {
				max_low_mcs = QDB(12);
				max_mid_mcs = QDB(13);
				max_high_mcs = QDB(14);
			} else if (chan == 9) {
				max_low_mcs = 46; /* 11.5 dBm */
				max_mid_mcs = 50; /* 12.5 dBm */
				max_high_mcs = 54; /* 13.5 dBm */
			}
		}

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			/* MCS 0-4 are in the low set
			 * MCS 5-7 are in the mid set
			 */
			if (i <= 4)
				txpwr->u40.n.cdd[i] = (uint8)max_low_mcs;
			else
				txpwr->u40.n.cdd[i] = (uint8)max_mid_mcs;
		}

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			/* MCS  8-11 (i: 0-3) are in the mid set
			 * MCS 12-15 (i: 4-7) are in the high set
			 */
			if (i <= 3)
				txpwr->u40.n.sdm[i] = (uint8)max_mid_mcs;
			else
				txpwr->u40.n.sdm[i] = (uint8)max_high_mcs;
		}
	}

	/* Per MCS fixup for CLM 4.2.3 */
	if ((li_mimo == &locale_1cn) || (li_mimo == &locale_8cn) ||
		(li_mimo == &locale_19ln_4) || (li_mimo == &locale_11ln_4)) {
		uint max_low_mcs, max_mid_mcs, max_high_mcs, max_mid_mcs_sdm;

		max_low_mcs = 0;
		max_mid_mcs = 0;
		max_high_mcs = 0;
		max_mid_mcs_sdm = 0;
		if ((((li_mimo == &locale_1cn) || (li_mimo == &locale_19ln_4)) &&
			(chan >= 36 && chan <= 48)) || (((li_mimo == &locale_11ln_4) ||
			(li_mimo == &locale_8cn)) && (chan >= 56 && chan <= 64))) {
				max_low_mcs = 26; /* 6.5 dBm */
				max_mid_mcs = 26; /* MCS 4-7 */
				max_mid_mcs_sdm = QDB(10); /* MCS 7-11 (SDM) */
				max_high_mcs = QDB(11);

			for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
				/* MCS 0-4 are in the low set
				 * MCS 5-7 are in the mid set
				 */
				if (i <= 4)
					txpwr->u20.n.cdd[i] = (uint8)max_low_mcs;
				else
					txpwr->u20.n.cdd[i] = (uint8)max_mid_mcs;
			}

			for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
				/* MCS  8-11 (i: 0-3) are in the mid set
				 * MCS 12-15 (i: 4-7) are in the high set
				 */
				if (i <= 3)
					txpwr->u20.n.sdm[i] = (uint8)max_mid_mcs_sdm;
				else
					txpwr->u20.n.sdm[i] = (uint8)max_high_mcs;
			}
		}
		/* fixup 40MHz */
		if ((((li_mimo == &locale_1cn) || (li_mimo == &locale_19ln_4)) &&
			(chan >= 36 && chan <= 48)) || (((li_mimo == &locale_11ln_4) ||
			(li_mimo == &locale_8cn)) && (chan >= 56 && chan <= 64))) {
				max_low_mcs = QDB(9);
				max_mid_mcs = QDB(9);
				max_mid_mcs_sdm = QDB(12); /* MCS 7-11 (SDM) */
				max_high_mcs = QDB(14);

			for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
				/* MCS 0-4 are in the low set
				 * MCS 5-7 are in the mid set
				 */
				if (i <= 4)
					txpwr->u40.n.cdd[i] = (uint8)max_low_mcs;
				else
					txpwr->u40.n.cdd[i] = (uint8)max_mid_mcs;
			}

			for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
				/* MCS  8-11 (i: 0-3) are in the mid set
				 * MCS 12-15 (i: 4-7) are in the high set
				 */
				if (i <= 3)
					txpwr->u40.n.sdm[i] = (uint8)max_mid_mcs_sdm;
				else
					txpwr->u40.n.sdm[i] = (uint8)max_high_mcs;
			}
		}
	}

	/* Per MCS fixup for CLM 4.8.2 */
	if ((li_mimo == &locale_29bn_3) && (chan >= 36 && chan <= 48)) {
		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			txpwr->u20.n.cdd[i] = QDB(10);
			txpwr->u40.n.cdd[i] = QDB(12);
		}
	}


	/* Fixup 40Mhz per-mcs limits */
	if (li_mimo == &locale_an8_t2) {
		uint max_low_mcs, max_mid_mcs, max_high_mcs, max_mid_mcs_sdm;

		/* Fixup per-MCS */
		max_low_mcs = 0;
		max_mid_mcs = 0;
		max_high_mcs = 0;
		max_mid_mcs_sdm = 0;

		if (chan == 3) {
			max_low_mcs = QDB(13);
			max_mid_mcs = QDB(14);
			max_high_mcs = 58; /* 14.5 dBm */
		} else if (chan >= 4 && chan <= 8) {
			max_low_mcs = QDB(14);
			max_mid_mcs = QDB(15);
			max_high_mcs = 62; /* 15.5 dBm */
		} else if (chan == 9) {
			max_low_mcs = QDB(13);
			max_mid_mcs = QDB(14);
			max_high_mcs = 58; /* 14.5 dBm */
		}

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			/* MCS 0-4 are in the low set
			 * MCS 5-7 are in the mid set
			 */
			if (i <= 4)
				txpwr->u40.n.cdd[i] = (uint8)max_low_mcs;
			else
				txpwr->u40.n.cdd[i] = (uint8)max_mid_mcs;
		}

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			/* MCS  8-11 (i: 0-3) are in the mid set
			 * MCS 12-15 (i: 4-7) are in the high set
			 */
			if (i <= 3)
				txpwr->u40.n.sdm[i] = (uint8)max_mid_mcs;
			else
				txpwr->u40.n.sdm[i] = (uint8)max_high_mcs;
		}
	}

	if (li_mimo == &locale_11ln_2) {
		maxpwr20 = 0;
		maxpwr40 = 0;

		/* Fixup SISO */
		if (chan >= 36 && chan <= 48) {
			maxpwr20 = QDB(14);
			maxpwr40 = QDB(13);
		} else if (chan >= 52 && chan <= 64) {
			maxpwr20 = 58; /* 14.5 dBm */
			maxpwr40 = QDB(13);
		} else if (chan >= 100 && chan <= 140) {
			maxpwr20 = QDB(15);
			maxpwr40 = QDB(16);
		} else if (chan >= 149 && chan <= 165) {
			maxpwr20 = 70; /* 17.5 dBm */
			maxpwr40 = QDB(18);
		}
		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			txpwr->u20.n.siso[i] = (uint8)maxpwr20;
			txpwr->u40.n.siso[i] = (uint8)maxpwr40;
		}
	}

	if (li_mimo == &locale_19n_1 ||
	    li_mimo == &locale_19n_2 ||
	    li_mimo == &locale_3jn_4 ||
	    li_mimo == &locale_3rn_3 ||
	    li_mimo == &locale_5ln_2 ||
	    li_mimo == &locale_6an_1 ||
	    li_mimo == &locale_7cn_1 ||
	    li_mimo == &locale_8an_1 ||
	    li_mimo == &locale_13n_2 ||
	    li_mimo == &locale_25hn ||
	    li_mimo == &locale_29dn_2 ||
		li_mimo == &locale_3en ||
		li_mimo == &locale_bn7 ||
	    li_mimo == &locale_2n) {
		/* Fixup SISO */

		if (li_mimo == &locale_19n_1) {
			maxpwr20 = 0;
			maxpwr40 = 0;

			if (chan >= 36 && chan <= 44) {
				maxpwr20 = QDB(14);
				maxpwr40 = QDB(14);
			} else if (chan >= 46 && chan <= 48) {
				maxpwr20 = QDB(17);
				maxpwr40 = QDB(16);
			} else if (chan >= 52 && chan <= 60) {
				maxpwr20 = QDB(14);
				maxpwr40 = QDB(16);
			} else if (chan >= 62 && chan <= 64) {
				maxpwr20 = QDB(13);
				maxpwr40 = QDB(14);
			} else if (chan >= 100 && chan <= 108) {
				maxpwr20 = QDB(17);
				maxpwr40 = QDB(14);
			} else if (chan >= 110 && chan <= 140) {
				maxpwr20 = 50; /* 12.5 dBm */
				maxpwr40 = QDB(16);
			} else if (chan >= 149 && chan <= 165) {
				maxpwr20 = QDB(17);
				maxpwr40 = QDB(16);
			}
		} else if (li_mimo == &locale_19n_2) {
			if (chan >= 36 && chan <= 48) {
				maxpwr20 = QDB(14);
				maxpwr40 = QDB(13);
			} else if (chan >= 52 && chan <= 140) {
				maxpwr20 = QDB(16);
				maxpwr40 = QDB(16);
			} else if (chan >= 149 && chan <= 165) {
				maxpwr20 = QDB(18);
				maxpwr40 = QDB(18);
			}
		} else if (li_mimo == &locale_3jn_4) {
			maxpwr20 = 0;
			maxpwr40 = 0;

			if (chan >= 36 && chan <= 48) {
				maxpwr20 = QDB(15);
				maxpwr40 = QDB(13);
			} else if (chan >= 52 && chan <= 64) {
				maxpwr20 = QDB(10);
				maxpwr40 = QDB(10);
			} else if (chan >= 100 && chan <= 140) {
				maxpwr20 = QDB(16);
				maxpwr40 = QDB(16);
			}
		} else if (li_mimo == &locale_3rn_3) {
			maxpwr20 = 0;
			maxpwr40 = 0;

			if ((chan >= 36 && chan <= 64) ||
			    (chan >= 100 && chan <= 116) ||
			    (chan >= 132 && chan <= 140)) {
				maxpwr20 = QDB(16);
				maxpwr40 = QDB(16);
			}
		} else if (li_mimo == &locale_2n) {
			maxpwr20 = QDB(16);
			maxpwr40 = QDB(16);
		} else if (li_mimo == &locale_5ln_2) {
			maxpwr20 = 0;
			maxpwr40 = 0;

			if (chan >= 36 && chan <= 64) {
				maxpwr20 = QDB(15);
				maxpwr40 = QDB(16);
			} else if (chan >= 149 && chan <= 165) {
				maxpwr20 = QDB(16);
				maxpwr40 = QDB(16);
			}
		} else if (li_mimo == &locale_6an_1) {
			maxpwr20 = 0;
			maxpwr40 = 0;

			if (chan >= 36 && chan <= 38) {
				maxpwr20 = QDB(15);
				maxpwr40 = QDB(14);
			} else if ((chan >= 40 && chan <= 48) ||
				(chan >= 52 && chan <= 64)) {
				maxpwr20 = QDB(15);
				maxpwr40 = QDB(17);
			} else if (chan >= 149 && chan <= 165) {
				maxpwr20 = QDB(17);
				maxpwr40 = QDB(17);
			}
		} else if (li_mimo == &locale_7cn_1) {
			maxpwr20 = 0;
			maxpwr40 = 0;

			if (chan >= 149 && chan <= 165) {
				maxpwr20 = QDB(16);
				maxpwr40 = QDB(16);
			}
		} else if (li_mimo == &locale_8an_1) {
			maxpwr20 = 0;
			maxpwr40 = 0;

			if (chan >= 56 && chan <= 64) {
				maxpwr20 = QDB(15);
				maxpwr40 = QDB(17);
			} else if (chan >= 100 && chan <= 102) {
				maxpwr20 = QDB(17);
				maxpwr40 = QDB(16);
			} else if ((chan >= 104 && chan <= 140) ||
				(chan >= 149 && chan <= 165)) {
				maxpwr20 = QDB(17);
				maxpwr40 = QDB(17);
			}
		} else if (li_mimo == &locale_13n_2) {
			maxpwr20 = 0;
			maxpwr40 = 0;

			if (chan >= 36 && chan <= 64) {
				maxpwr20 = QDB(16);
				maxpwr40 = QDB(16);
			}
		} else if (li_mimo == &locale_25hn) {
			maxpwr20 = 0;
			maxpwr40 = 0;

			if (chan >= 36 && chan <= 48) {
				maxpwr20 = QDB(15);
				maxpwr40 = QDB(16);
			} else if (chan >= 52 && chan <= 64) {
				maxpwr20 = QDB(16);
				maxpwr40 = QDB(16);
			} else if ((chan >= 100 && chan <= 124) ||
				(chan >= 149 && chan <= 161)) {
				maxpwr20 = QDB(16);
				maxpwr40 = QDB(16);
			}
		} else if (li_mimo == &locale_29dn_2) {
			maxpwr20 = 0;
			maxpwr40 = 0;

			if (chan >= 36 && chan <= 38) {
				maxpwr20 = QDB(15);
				maxpwr40 = QDB(14);
			} else if (chan >= 40 && chan <= 48) {
				maxpwr20 = QDB(15);
				maxpwr40 = QDB(17);
			} else if (chan >= 100 && chan <= 108) {
				maxpwr20 = QDB(17);
				maxpwr40 = QDB(16);
			} else if ((chan >= 52 && chan <= 64) ||
				(chan >= 110 && chan <= 116) ||
				(chan >= 132 && chan <= 140) ||
				(chan >= 149 && chan <= 165)) {
				maxpwr20 = QDB(17);
				maxpwr40 = QDB(17);
			}
		} else if (li_mimo == &locale_3en) {
			if (chan >= 36 && chan <= 140) {
				maxpwr20 = QDB(21);
				maxpwr40 = QDB(20);
			}
		}

		if (li_mimo == &locale_bn7) {
			maxpwr20 = QDB(19);
			maxpwr40 = 0;
			if (chan >= 3 && chan <= 11) {
				maxpwr40 = QDB(16);
			}
		}

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			txpwr->u20.n.siso[i] = (uint8)maxpwr20;
			txpwr->u40.n.siso[i] = (uint8)maxpwr40;
		}
	}

	if (li_mimo == &locale_19n_3) {
		maxpwr20 = 0;
		maxpwr40 = 0;

		/* Fixup SISO */
		if (chan >= 36 && chan <= 38) {
			maxpwr20 = QDB(13);
			maxpwr40 = QDB(11);
		} else if (chan >= 40 && chan <= 48) {
			maxpwr20 = 62; /* 15.5 dBm */
			maxpwr40 = 58; /* 14.5 dBm */
		} else if (chan >= 52 && chan <= 60) {
			maxpwr20 = 62; /* 15.5 dBm */
			maxpwr40 = 58; /* 14.5 dBm */
		} else if (chan >= 62 && chan <= 64) {
			maxpwr20 = QDB(13);
			maxpwr40 = 46; /* 11.5 dBm */
		} else if (chan >= 100 && chan <= 102) {
			maxpwr20 = 62; /* 15.5 dBm */
			maxpwr40 = 58; /* 14.5 dBm */
		} else if (chan >= 104 && chan <= 140) {
			maxpwr20 = 58; /* 14.5 dBm */
			maxpwr40 = 54; /* 13.5 dBm */
		} else if (chan >= 149 && chan <= 165) {
			maxpwr20 = 58; /* 14.5 dBm */
			maxpwr40 = 54; /* 13.5 dBm */
		}
		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			txpwr->u20.n.siso[i] = (uint8)maxpwr20;
			txpwr->u40.n.siso[i] = (uint8)maxpwr40;
		}
	}


	if (li_mimo == &locale_19bn ||
		li_mimo == &locale_19hn ||
		li_mimo == &locale_19ln ||
		li_mimo == &locale_19rn ||
		li_mimo == &locale_19ln_1 ||
		li_mimo == &locale_19ln_2 ||
		li_mimo == &locale_19ln_4) {
		/* Fixup SISO */
		maxpwr20 = 0;
		maxpwr40 = 0;

		if (chan >= 36 && chan <= 44) {
			maxpwr20 = QDB(14);
			maxpwr40 = QDB(13);
			if (li_mimo == &locale_19ln_1)
				maxpwr40 = 58; /* 14.5 dBm */

			if (li_mimo == &locale_19ln_2)
				maxpwr20 = QDB(13);

			if (li_mimo == &locale_19ln_4) {
				maxpwr20 = QDB(13);
				maxpwr40 = QDB(16);
			}

		} else if (chan >= 46 && chan <= 48) {
			if ((li_mimo == &locale_19ln_2) || (li_mimo == &locale_19ln_4)) {
				maxpwr20 = QDB(13);
				maxpwr40 = QDB(16);
			} else {
				maxpwr20 = QDB(14);
				maxpwr40 = 66; /* 16.5 dBm */
			}
		} else if (chan >= 52 && chan <= 60) {
			maxpwr20 = 70; /* 17.5 dBm */
			maxpwr40 = QDB(17);
		} else if (chan >= 62 && chan <= 64) {
			if ((li_mimo == &locale_19ln_2) || (li_mimo == &locale_19ln_4)) {
				maxpwr20 = 58; /* 14.5 dBm */
				maxpwr40 = QDB(11);
			} else {
				maxpwr20 = 58; /* 14.5 dBm */
				maxpwr40 = QDB(13);
			}
		} else if (chan >= 100 && chan <= 108) {
			if (li_mimo == &locale_19ln_2)
				maxpwr20 = 58; /* 14.5 dBm */
			else
				maxpwr20 = QDB(17);
			maxpwr40 = 62; /* 15.5 dBm */
			if (li_mimo == &locale_19ln_4) {
				maxpwr20 = 58; /* 14.5 dBm */
				maxpwr40 = 54; /* 13.5 dBm */
			}
		} else if (chan >= 110 && chan <= 140) {
			maxpwr20 = QDB(17);
			maxpwr40 = QDB(18);
			if (chan >= 134 && chan <= 136) {
				if (li_mimo == &locale_19ln_1)
					maxpwr40 = QDB(17);
				else if ((li_mimo == &locale_19ln_2) || (li_mimo == &locale_19ln_4))
					maxpwr40 = QDB(15);
			}
			if (chan == 140 &&
			    (li_mimo == &locale_19ln_2 || (li_mimo == &locale_19ln_1) ||
			     (li_mimo == &locale_19ln_4))) {
				maxpwr20 = QDB(15);
				maxpwr40 = 0;
			}
		} else if (chan >= 149 && chan <= 165) {
			maxpwr20 = 70; /* 17.5 dBm */
			maxpwr40 = 74; /* 18.5 dBm */
		}

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			txpwr->u20.n.siso[i] = (uint8)maxpwr20;
			txpwr->u40.n.siso[i] = (uint8)maxpwr40;
		}
	}

	if (li_mimo == &locale_19hn_1) {
		/* Fixup SISO */
		maxpwr20 = 0;
		maxpwr40 = 0;

		if (chan >= 36 && chan <= 48) {
			maxpwr20 = QDB(14);
			maxpwr40 = 62 /* 15.5 */;
		} else if (chan >= 52 && chan <= 60) {
			maxpwr20 = 70 /* 17.5 */;
			maxpwr40 = 70;
		} else if (chan >= 62 && chan <= 64) {
			maxpwr20 = 62;
			maxpwr40 = 62;
		} else if (chan >= 100 && chan <= 102) {
			maxpwr20 = 66 /* 16.5 */;
			maxpwr40 = 62;
		} else if (chan >= 104 && chan <= 132) {
			maxpwr20 = 66 /* 16.5 */;
			maxpwr40 = 66;
		} else if (chan >= 134 && chan <= 136) {
			maxpwr20 = 66 /* 16.5 */;
			maxpwr40 = 70;
		} else if (chan == 140) {
			maxpwr20 =  58 /* 14.5 */;
		} else if (chan >= 149 && chan <= 165) {
			maxpwr20 = 74 /* 18.5 */;
			maxpwr40 = 70;
		}

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			txpwr->u20.n.siso[i] = (uint8)maxpwr20;
			txpwr->u40.n.siso[i] = (uint8)maxpwr40;
		}
	}
	/* CLM 4.2.3 fixup chan 100-140 CDD power */
	if ((li_mimo == &locale_19ln_4) || (li_mimo == &locale_11ln_4)) {
		if (chan >= 100 && chan <= 108) {
			maxpwr20 = QDB(13);
			maxpwr40 = QDB(12);
		} else if (chan >= 110 && chan <= 132) {
			maxpwr20 = 58; /* 14.5 dBm */
			maxpwr40 = QDB(17);
		} else if (chan >= 134 && chan <= 136) {
			maxpwr20 = 58; /* 14.5 dBm */
			maxpwr40 = 62; /* 15.5 dBm */
		} else if (chan == 140) {
			maxpwr20 = QDB(17);
			maxpwr40 = QDB(18);
		}
		if ((chan >= 100) && (chan <= 140)) {
			for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
				txpwr->u20.n.cdd[i] = (uint8)maxpwr20;
				txpwr->u40.n.cdd[i] = (uint8)maxpwr40;
			}
		}
	}

	if (li_mimo == &locale_29n || li_mimo == &locale_29an) {
		/* Fixup SISO */
		maxpwr20 = 0;
		maxpwr40 = 0;

		if (chan >= 36 && chan <= 48) {
			maxpwr20 = QDB(14);
			maxpwr40 = QDB(14);
		} else if ((chan >= 52 && chan <= 64) ||
			(chan >= 100 && chan <= 140) ||
			(chan >= 149 && chan <= 165)) {
			maxpwr20 = QDB(16);
			maxpwr40 = QDB(16);
		}

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			txpwr->u20.n.siso[i] = (uint8)maxpwr20;
			txpwr->u40.n.siso[i] = (uint8)maxpwr40;
		}
	}

	/* Copy the 40 MHz MCS0-7 CDD value to the 40 MHz OFDM CDD value if it wasn't
	 * provided explicitly. When doing so, the constellation and coding rates of
	 * the corresponding Legacy OFDM and MCS rates should be matched in the same way
	 * as done in wlc_phy_mcs_to_ofdm_powers_nphy(). Specifically, the power of 9 Mbps
	 * Legacy OFDM is set to the power of MCS-0 (same as 6 Mbps power) since no equivalent
	 * of 9 Mbps exists in the 11n standard in terms of constellation and coding rate.
	 */
	for (i = 0, j = 0; i < WL_NUM_RATES_OFDM; i++, j++) {
		if (txpwr->ofdm_40_cdd[i] == 0)
			txpwr->ofdm_40_cdd[i] = txpwr->u40.n.cdd[j];
		if (i == 0) {
			i = i + 1;
			if (txpwr->ofdm_40_cdd[i] == 0)
				txpwr->ofdm_40_cdd[i] = txpwr->u40.n.cdd[j];
		}
	}

	/* Copy the 40 MHZ MCS 0-7 CDD value to the 40 MHZ MCS 0-7 SISO value if it wasn't
	 * provided explicitly.  Note the CDD value might be zero if 40 MHZ operations are
	 * not allowed.
	 */

	for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
		if (txpwr->u40.n.siso[i] == 0)
			txpwr->u40.n.siso[i] = txpwr->u40.n.cdd[i];
	}

	/* Copy the 40 MHz MCS0-7 SISO value to the 40 MHz OFDM SISO value if it wasn't
	 * provided explicitly. When doing so, the constellation and coding rates of
	 * the corresponding Legacy OFDM and MCS rates should be matched in the same way
	 * as done in wlc_phy_mcs_to_ofdm_powers_nphy(). Specifically, the power of 9 Mbps
	 * Legacy OFDM is set to the power of MCS-0 (same as 6 Mbps power) since no equivalent
	 * of 9 Mbps exists in the 11n standard in terms of constellation and coding rate.
	 */
	for (i = 0, j = 0; i < WL_NUM_RATES_OFDM; i++, j++) {
		if (txpwr->ofdm_40[i] == 0)
			txpwr->ofdm_40[i] = txpwr->u40.n.siso[j];
		if (i == 0) {
			i = i + 1;
			if (txpwr->ofdm_40[i] == 0)
				txpwr->ofdm_40[i] = txpwr->u40.n.siso[j];
		}
	}

	/* Copy the 20 and 40 MHz MCS0-7 CDD values to the corresponding STBC values if they weren't
	 * provided explicitly.
	 */
	for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
		if (txpwr->u20.n.stbc[i] == 0)
			txpwr->u20.n.stbc[i] = txpwr->u20.n.cdd[i];

		if (txpwr->u40.n.stbc[i] == 0)
			txpwr->u40.n.stbc[i] = txpwr->u40.n.cdd[i];
	}

#if HTCONF
	if (WLCISHTPHY(wlc->band)) {
		n2x2_t u20pl, u40pl, u20npl, u40npl;

		li_mimo = NULL;
		if (IS_CCODE_REV(wlc_cm, "Q2", 4)) {
			li_mimo = BAND_5G(band->bandtype) ?
				wlc_get_mimo_5g(LOCALE_MIMO_IDX_27en_3n) :
				wlc_get_mimo_2g(LOCALE_MIMO_IDX_an6_4_3n);
		} else if (IS_CCODE_REV(wlc_cm, "EU", 9)) {
			li_mimo = BAND_5G(band->bandtype) ?
				wlc_get_mimo_5g(LOCALE_MIMO_IDX_18n_1_3n) :
				wlc_get_mimo_2g(LOCALE_MIMO_IDX_bn_10_3n);
		} else if (IS_CCODE_REV(wlc_cm, "US", 61) ||
			(IS_CCODE_REV(wlc_cm, "US", 53))) {
			li_mimo = BAND_5G(band->bandtype) ?
				wlc_get_mimo_5g(LOCALE_MIMO_IDX_19_3n) :
				wlc_get_mimo_2g(LOCALE_MIMO_IDX_a_3n);
		} else if (IS_CCODE_REV(wlc_cm, "US", 72)) {
			li_mimo = BAND_5G(band->bandtype) ?
				wlc_get_mimo_5g(LOCALE_MIMO_IDX_19_3n) :
				wlc_get_mimo_2g(LOCALE_MIMO_IDX_an6_4_3n);
		} else if (IS_CCODE_REV(wlc_cm, "US", 73)) {
			li_mimo = BAND_5G(band->bandtype) ?
				wlc_get_mimo_5g(LOCALE_MIMO_IDX_29_3n) :
				wlc_get_mimo_2g(LOCALE_MIMO_IDX_a1_3n);
		} else if (IS_CCODE_REV(wlc_cm, "GB", 6)) {
			li_mimo = BAND_5G(band->bandtype) ?
				wlc_get_mimo_5g(LOCALE_MIMO_IDX_3_3n) :
				wlc_get_mimo_2g(LOCALE_MIMO_IDX_b2_3n);
		}


		/* Copy the 20 in 40MHz CCK, OFDM, MCS0-7 values to the
		 * corresponding values if they weren't provided explicitly.
		 */
		for (i = 0; i < WL_NUM_RATES_CCK; i++) {
			if (li_mimo == &locale_a_3n) {
				/* Using CCK CDD 1x3 power limits for 1x2 as well
				 * As they are not defined in CLM.
				 */
				if ((chan == 1) || (chan == 11)) {
					txpwr->cck_cdd.s1x2[i] = QDB(16);
					txpwr->cck_cdd.s1x3[i] = QDB(16);
					txpwr->cck_20ul_cdd.s1x2[i] = QDB(16);
					txpwr->cck_20ul_cdd.s1x3[i] = QDB(16);
				} else if ((chan >= 2) && (chan <= 10)) {
					txpwr->cck_cdd.s1x2[i] = QDB(18);
					txpwr->cck_cdd.s1x3[i] = QDB(18);
					txpwr->cck_20ul_cdd.s1x2[i] = QDB(18);
					txpwr->cck_20ul_cdd.s1x3[i] = QDB(18);
				}
			}

			if (li_mimo == &locale_a1_3n) {
				/* Using CCK CDD 1x3 power limits for 1x2 as well
				 * As they are not defined in CLM.
				 */
				if (chan == 1)  {
					txpwr->cck_cdd.s1x2[i] = QDB(22);
					txpwr->cck_cdd.s1x3[i] = QDB(21);
					txpwr->cck_20ul_cdd.s1x2[i] = QDB(22);
					txpwr->cck_20ul_cdd.s1x3[i] = QDB(21);
				} else if ((chan >= 2) && (chan <= 10)) {
					txpwr->cck_cdd.s1x2[i] = 94; /* 23.5 dBm */
					txpwr->cck_cdd.s1x3[i] = QDB(18);
					txpwr->cck_20ul_cdd.s1x2[i] = 94; /* 23.5 dBm */
					txpwr->cck_20ul_cdd.s1x3[i] = QDB(18);
				} else if (chan == 11) {
					txpwr->cck_cdd.s1x2[i] = QDB(22); /* 21.5 dBm */
					txpwr->cck_cdd.s1x3[i] = 86;
					txpwr->cck_20ul_cdd.s1x2[i] = QDB(22);
					txpwr->cck_20ul_cdd.s1x3[i] = 86;
				}
			}

			if (li_mimo == &locale_an6_7) {
				/* Using CCK CDD 1x2 power limits for 1x3 as well
				 * As they are not defined in CLM.
				 */
				if (chan == 1) {
					txpwr->cck_cdd.s1x2[i] = QDB(22);
					txpwr->cck_cdd.s1x3[i] = QDB(21);
					txpwr->cck_20ul_cdd.s1x2[i] = QDB(22);
					txpwr->cck_20ul_cdd.s1x3[i] = QDB(21);
				} else if ((chan >= 2) && (chan <= 10)) {
					txpwr->cck_cdd.s1x2[i] = 94; /* 23.5 dBm */
					txpwr->cck_cdd.s1x3[i] = QDB(22);
					txpwr->cck_20ul_cdd.s1x2[i] = 94;
					txpwr->cck_20ul_cdd.s1x3[i] = QDB(22);
				} else if (chan == 11) {
					txpwr->cck_cdd.s1x2[i] = QDB(22);
					txpwr->cck_cdd.s1x3[i] = 86; /* 21.5 dBm */
					txpwr->cck_20ul_cdd.s1x2[i] = QDB(22);
					txpwr->cck_20ul_cdd.s1x3[i] = 86;
				}
			}

			if (txpwr->cck_20ul[i] == 0)
				txpwr->cck_20ul[i] = txpwr->cck[i];
			if (txpwr->cck_cdd.s1x2[i] == 0)
				txpwr->cck_cdd.s1x2[i] = txpwr->cck[i];
			if (txpwr->cck_cdd.s1x3[i] == 0)
				txpwr->cck_cdd.s1x3[i] = txpwr->cck[i];
			if (txpwr->cck_20ul_cdd.s1x2[i] == 0)
				txpwr->cck_20ul_cdd.s1x2[i] = txpwr->cck_20ul[i];
			if (txpwr->cck_20ul_cdd.s1x3[i] == 0)
				txpwr->cck_20ul_cdd.s1x3[i] = txpwr->cck_20ul[i];

		}

		for (i = 0; i < WL_NUM_RATES_OFDM; i++) {
			if (txpwr->ofdm_20ul[i] == 0)
				txpwr->ofdm_20ul[i] = txpwr->ofdm[i];
			if (txpwr->ofdm_20ul_cdd[i] == 0)
				txpwr->ofdm_20ul_cdd[i] = txpwr->ofdm_cdd[i];
		}


		if (li_mimo) {
			maxpwr20 = li_mimo->maxpwr20[maxpwr_idx];
			maxpwr40 = li_mimo->maxpwr40[maxpwr_idx];

			if (li_mimo == &locale_19_3n) {
				if ((chan >= 100) && (chan <= 108)) {
					maxpwr20 = QDB(13);
					maxpwr40 = QDB(12);
				} else if ((chan >= 134) && (chan <= 136)) {
					maxpwr20 = 58; /* 14.5dbm */
					maxpwr40 = 62; /* 15.5dbm */
				} else if (chan == 140) {
					maxpwr20 = 54; /* 13.5dbm */
					maxpwr40 = QDB(18);
				}
			}

			if (li_mimo == &locale_29_3n) {
				if ((chan >= 100) && (chan <= 102)) {
					maxpwr20 = QDB(15);
					maxpwr40 = 42; /* 10.5 dBm */ 
				} else if ((chan >= 104) && (chan <= 140)) {
					maxpwr20 = 62; /* 15.5dbm */
					maxpwr40 = QDB(18);
				} else if ((chan >= 161) && (chan <= 165)) {
					maxpwr40 = 0;
				}
			}
		}

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			if (li_mimo) {
				u20pl.sdm[i] = (uint8)maxpwr20;
				u20pl.cdd[i] = (uint8)maxpwr20;
				u40pl.sdm[i] = (uint8)maxpwr40;
				u40pl.cdd[i] = (uint8)maxpwr40;
			} else {
				u20pl.sdm[i] = txpwr->u20.n.sdm[i];
				u20pl.cdd[i] = txpwr->u20.n.cdd[i];
				u40pl.sdm[i] = txpwr->u40.n.sdm[i];
				u40pl.cdd[i] = txpwr->u40.n.cdd[i];
			}
		}

		/* txpwr->u20/40 declared is union of n/ht powerlimits,
		 * so save the powerlimits for n, for using them later to fill ht.
		 */
		bcopy(&txpwr->u20, &u20npl, sizeof(n2x2_t));
		bcopy(&txpwr->u40, &u40npl, sizeof(n2x2_t));

		for (i = 0; i < WL_NUM_RATES_MCS_1STREAM; i++) {
			txpwr->u20.ht.s2x2[i] = u20npl.sdm[i];
			txpwr->u20.ht.s3x3[i] = u20pl.sdm[i];
			txpwr->ht.u20s1x3[i] = u20pl.cdd[i];
			txpwr->ht.u20s2x3[i] = u20pl.sdm[i];

			txpwr->u40.ht.s2x2[i] = u40npl.sdm[i];
			txpwr->u40.ht.s3x3[i] = u40pl.sdm[i];
			txpwr->ht.u40s1x3[i] = u40pl.cdd[i];
			txpwr->ht.u40s2x3[i] = u40pl.sdm[i];

			txpwr->ht20ul.s1x1[i] = u20npl.siso[i];
			txpwr->ht20ul.s1x2[i] = u20npl.cdd[i];
			txpwr->ht20ul.s2x2[i] = u20npl.sdm[i];
			txpwr->ht20ul.s3x3[i] = u20pl.sdm[i];
			txpwr->ht.ul20s1x3[i] = u20pl.cdd[i];
			txpwr->ht.ul20s2x3[i] = u20pl.sdm[i];
#ifdef NOT_YET
			txpwr->stbc.s2x2[i] = txpwr->u20.ht.s2x2[i];
			txpwr->stbc.s2x3[i] = txpwr->ht.u20s2x3[i];
			txpwr->stbc.u40_s2x2[i] = txpwr->u40.ht.s2x2[i];
			txpwr->stbc.u40_s2x3[i] = txpwr->ht.u40s2x3[i];
			txpwr->stbc.ul20_s2x2[i] = txpwr->ht20ul.s2x2[i];
			txpwr->stbc.ul20_s2x3[i] = txpwr->ht.ul20s2x3[i];
#endif
			if (li_mimo == &locale_19_3n) {
				if ((chan >= 36) && (chan <= 48)) {
					/* MCS 0-7 has seperate CDD limits defined,
					 * set them here
					 */
					txpwr->ht.u20s1x3[i] = 14; /* 3.5 dbm */
					txpwr->ht.u40s1x3[i] = QDB(7);
					txpwr->ht.ul20s1x3[i] = 14; /* 3.5 dbm */

					/* MCS 0-7 has seperate STBC limits defined,
					 * set them here
					 */
#ifdef NOT_YET
					txpwr->stbc.s2x3[i] = QDB(7);
					txpwr->stbc.u40_s2x3[i] = QDB(9);
					txpwr->stbc.ul20_s2x3[i] = QDB(7);
#endif

					if (i <= 3) {
						txpwr->ht.u20s2x3[i] = QDB(8);
						txpwr->ht.u40s2x3[i] = QDB(10);
					} else {
						txpwr->ht.u20s2x3[i] = QDB(9);
						txpwr->ht.u40s2x3[i] = QDB(12);
					}
					if (i <= 2) {
						txpwr->u20.ht.s3x3[i] = QDB(8);
						txpwr->u40.ht.s3x3[i] = QDB(10);
					} else if (i <= 5) {
						txpwr->u20.ht.s3x3[i] = QDB(9);
						txpwr->u40.ht.s3x3[i] = QDB(12);
					} else {
						txpwr->u20.ht.s3x3[i] = QDB(10);
						txpwr->u40.ht.s3x3[i] = QDB(13);
					}
				}
			}

			if (li_mimo == &locale_29_3n) {
				if ((chan >= 36) && (chan <= 48)) {
					/* MCS 0-7 has seperate CDD limits defined,
					 * set them here
					 */
					txpwr->ht.ul20s1x3[i] = 0;
					txpwr->ht.u20s1x3[i] = 0; /* disabled */
					txpwr->ht.u40s1x3[i] = 0;


					/* MCS 0-7 has seperate STBC limits defined,
					 * set them here
					 */
#ifdef NOT_YET
					txpwr->stbc.s2x3[i] = QDB(10);
					txpwr->stbc.u40_s2x3[i] = QDB(13);
					txpwr->stbc.ul20_s2x3[i] = QDB(10);
#endif

					/* SDM (MCS8-15) */
					txpwr->ht.u20s2x3[i] = 41; /* 10.25 dBm */
					txpwr->ht.u40s2x3[i] = QDB(13);
					txpwr->ht.ul20s2x3[i] = 41;

				}
				if ((chan >= 52) && (chan <= 60)) {
					/* MCS 0-7 has seperate CDD limits defined,
					 * set them here
					 */
					txpwr->ht.ul20s1x3[i] = 46 /* 11.5 */;
					txpwr->ht.u40s1x3[i] = 54; /* 13.5 */
					txpwr->ht.u20s1x3[i] = 46;

					/* MCS 0-7 has seperate STBC limits defined,
					 * set them here
					 */
#ifdef NOT_YET
					txpwr->stbc.s2x3[i] = 62; /* 15.5 */
					txpwr->stbc.u40_s2x3[i] = QDB(18);
					txpwr->stbc.ul20_s2x3[i] = 62;
#endif
				}

				if ((chan >= 62) && (chan <= 64)) {
					/* MCS 0-7 has seperate CDD limits defined,
					 * set them here
					 */
					txpwr->ht.ul20s1x3[i] = 46 /* 11.5 */;
					txpwr->ht.u40s1x3[i] = 46;
					txpwr->ht.u20s1x3[i] = 46;

					/* MCS 0-7 has seperate STBC limits defined,
					 * set them here
					 */
#ifdef NOT_YET
					txpwr->stbc.s2x3[i] = QDB(15);
					txpwr->stbc.u40_s2x3[i] = 50; /* 12.5 dBm */
					txpwr->stbc.ul20_s2x3[i] = QDB(15);
#endif
				}

				if ((chan >= 100) && (chan <= 102)) {
					/* MCS 0-7 has seperate CDD limits defined,
					 * set them here
					 */
					txpwr->ht.ul20s1x3[i] = 54 /* 13.5 */;
					txpwr->ht.u40s1x3[i] = 38; /* 9.5 */
					txpwr->ht.u20s1x3[i] = 54;

					/* MCS 0-7 has seperate STBC limits defined,
					 * set them here
					 */
#ifdef NOT_YET
					txpwr->stbc.s2x3[i] = QDB(15);
					txpwr->stbc.u40_s2x3[i] = 42; /* 10.5 dBm */
					txpwr->stbc.ul20_s2x3[i] = QDB(15);
#endif
				}

				if ((chan >= 104) && (chan <= 140)) {
					/* MCS 0-7 has seperate CDD limits defined,
					 * set them here
					 */
					txpwr->ht.ul20s1x3[i] = 54 /* 13.5 */;
					txpwr->ht.u40s1x3[i] = 53; /* 13.25 */
					txpwr->ht.u20s1x3[i] = 54;

					/* MCS 0-7 has seperate STBC limits defined,
					 * set them here
					 */
#ifdef NOT_YET
					txpwr->stbc.s2x3[i] = 62; /* 15.5 dBm */
					txpwr->stbc.u40_s2x3[i] = QDB(18);
					txpwr->stbc.ul20_s2x3[i] = 62;
#endif
				}

			}
		}
	}
#endif /* HTCONF */
#endif /* WL11N */

	WL_NONE(("Channel(chanspec) %d (0x%4.4x)\n", CHSPEC_CHANNEL(chanspec),	chanspec));
#ifdef WLC_LOW
	/* Convoluted WL debug conditional execution of function to avoid warnings. */
	WL_NONE(("%s", (wlc_phy_txpower_limits_dump(txpwr, WLCISHTPHY(wlc->band)), "")));
#endif /* WLC_LOW */

	return;
}

/* Returns TRUE if currently set country is Japan or variant */
bool
wlc_japan(struct wlc_info *wlc)
{
	return wlc_japan_ccode(wlc->cmi->country_abbrev);
}

/* JP, J1 - J10 are Japan ccodes */
static bool
wlc_japan_ccode(const char *ccode)
{
	return (ccode[0] == 'J' &&
		(ccode[1] == 'P' || (ccode[1] >= '1' && ccode[1] <= '9')));
}

/* Q2 is an alternate USA ccode */
static bool
wlc_us_ccode(const char *ccode)
{
	return (!strncmp("US", ccode, 3) || !strncmp("Q2", ccode, 3));
}

void
wlc_rcinfo_init(wlc_cm_info_t *wlc_cm)
{
	if (wlc_us_ccode(wlc_cm->country_abbrev)) {
#ifdef BAND5G
		wlc_cm->rcinfo_list[WLC_RCLIST_20] = &rcinfo_us_20;
#endif
#ifdef WL11N
		if (N_ENAB(wlc_cm->wlc->pub)) {
			wlc_cm->rcinfo_list[WLC_RCLIST_40L] = &rcinfo_us_40lower;
			wlc_cm->rcinfo_list[WLC_RCLIST_40U] = &rcinfo_us_40upper;
		}
#endif
	} else if (wlc_japan_ccode(wlc_cm->country_abbrev)) {
#ifdef BAND5G
		wlc_cm->rcinfo_list[WLC_RCLIST_20] = &rcinfo_jp_20;
#endif
#ifdef WL11N
		if (N_ENAB(wlc_cm->wlc->pub)) {
			wlc_cm->rcinfo_list[WLC_RCLIST_40L] = &rcinfo_jp_40;
			wlc_cm->rcinfo_list[WLC_RCLIST_40U] = &rcinfo_jp_40;
		}
#endif
	} else {
#ifdef BAND5G
		wlc_cm->rcinfo_list[WLC_RCLIST_20] = &rcinfo_eu_20;
#endif
#ifdef WL11N
		if (N_ENAB(wlc_cm->wlc->pub)) {
			wlc_cm->rcinfo_list[WLC_RCLIST_40L] = &rcinfo_eu_40lower;
			wlc_cm->rcinfo_list[WLC_RCLIST_40U] = &rcinfo_eu_40upper;
		}
#endif
	}
}

static void
wlc_regclass_vec_init(wlc_cm_info_t *wlc_cm)
{
	uint8 i, idx;
	chanspec_t chanspec;
#ifdef WL11N
	wlc_info_t *wlc = wlc_cm->wlc;
	bool saved_cap_40, saved_db_cap_40 = TRUE;
#endif
	rcvec_t *rcvec = &wlc_cm->valid_rcvec;

#ifdef WL11N
	/* save 40 MHz cap */
	saved_cap_40 = wlc->band->mimo_cap_40;
	wlc->band->mimo_cap_40 = TRUE;
	if (NBANDS(wlc) > 1) {
		saved_db_cap_40 = wlc->bandstate[OTHERBANDUNIT(wlc)]->mimo_cap_40;
		wlc->bandstate[OTHERBANDUNIT(wlc)]->mimo_cap_40 = TRUE;
	}
#endif

	bzero(rcvec, MAXRCVEC);
	for (i = 0; i < MAXCHANNEL; i++) {
		chanspec = CH20MHZ_CHSPEC(i);
		if (wlc_valid_chanspec_db(wlc_cm, chanspec)) {
			if ((idx = wlc_get_regclass(wlc_cm, chanspec)))
				setbit(rcvec, idx);
		}
#if defined(WL11N) && !defined(WL11N_20MHZONLY)
		if (N_ENAB(wlc->pub)) {
			chanspec = CH40MHZ_CHSPEC(i, WL_CHANSPEC_CTL_SB_LOWER);
			if (wlc_valid_chanspec_db(wlc_cm, chanspec)) {
				if ((idx = wlc_get_regclass(wlc_cm, chanspec)))
					setbit(rcvec, idx);
			}
			chanspec = CH40MHZ_CHSPEC(i, WL_CHANSPEC_CTL_SB_UPPER);
			if (wlc_valid_chanspec_db(wlc_cm, chanspec)) {
				if ((idx = wlc_get_regclass(wlc_cm, chanspec)))
					setbit(rcvec, idx);
			}
		}
#endif /* defined(WL11N) && !defined(WL11N_20MHZONLY) */
	}
#ifdef WL11N
	/* restore 40 MHz cap */
	wlc->band->mimo_cap_40 = saved_cap_40;
	if (NBANDS(wlc) > 1)
		wlc->bandstate[OTHERBANDUNIT(wlc)]->mimo_cap_40 = saved_db_cap_40;
#endif
}

#ifdef WL11N
uint8
wlc_rclass_extch_get(wlc_cm_info_t *wlc_cm, uint8 rclass)
{
	const rcinfo_t *rcinfo;
	uint8 i, extch = DOT11_EXT_CH_NONE;

	if (!isset(wlc_cm->valid_rcvec.vec, rclass)) {
		WL_ERROR(("wl%d: %s %d regulatory class not supported\n",
			wlc_cm->wlc->pub->unit, wlc_cm->country_abbrev, rclass));
		return extch;
	}

	/* rcinfo consist of control channel at lower sb */
	rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_40L];
	for (i = 0; rcinfo && i < rcinfo->len; i++) {
		if (rclass == rcinfo->rctbl[i].rclass) {
			/* ext channel is opposite of control channel */
			extch = DOT11_EXT_CH_UPPER;
			goto exit;
		}
	}

	/* rcinfo consist of control channel at upper sb */
	rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_40U];
	for (i = 0; rcinfo && i < rcinfo->len; i++) {
		if (rclass == rcinfo->rctbl[i].rclass) {
			/* ext channel is opposite of control channel */
			extch = DOT11_EXT_CH_LOWER;
			break;
		}
	}
exit:
	WL_INFORM(("wl%d: %s regulatory class %d has ctl chan %s\n",
		wlc_cm->wlc->pub->unit, wlc_cm->country_abbrev, rclass,
		((!extch) ? "NONE" : (((extch == DOT11_EXT_CH_LOWER) ? "LOWER" : "UPPER")))));

	return extch;
}

/* get the ordered list of supported reg class, with current reg class
 * as first element
 */
uint8
wlc_get_regclass_list(wlc_cm_info_t *wlc_cm, uint8 *rclist, uint lsize,
	chanspec_t chspec, bool ie_order)
{
	uint8 i, cur_rc = 0, idx = 0;

	ASSERT(rclist != NULL);
	ASSERT(lsize > 1);

	if (ie_order) {
		cur_rc = wlc_get_regclass(wlc_cm, chspec);
		if (!cur_rc) {
			WL_ERROR(("wl%d: current regulatory class is not found\n",
				wlc_cm->wlc->pub->unit));
			return 0;
		}
		rclist[idx++] = cur_rc;	/* first element is current reg class */
	}

	for (i = 0; i < MAXREGCLASS && idx < lsize; i++) {
		if (i != cur_rc && isset(wlc_cm->valid_rcvec.vec, i))
			rclist[idx++] = i;
	}

	if (i < MAXREGCLASS && idx == lsize) {
		WL_ERROR(("wl%d: regulatory class list full %d\n", wlc_cm->wlc->pub->unit, idx));
		ASSERT(0);
	}

	return idx;
}
#endif /* WL11N */

static uint8
wlc_get_2g_regclass(wlc_cm_info_t *wlc_cm, uint8 chan)
{
	if (wlc_us_ccode(wlc_cm->country_abbrev))
		return WLC_REGCLASS_USA_2G_20MHZ;
	else if (wlc_japan_ccode(wlc_cm->country_abbrev)) {
		if (chan < 14)
			return WLC_REGCLASS_JPN_2G_20MHZ;
		else
			return WLC_REGCLASS_JPN_2G_20MHZ_CH14;
	} else
		return WLC_REGCLASS_EUR_2G_20MHZ;
}

uint8
wlc_get_regclass(wlc_cm_info_t *wlc_cm, chanspec_t chanspec)
{
	const rcinfo_t *rcinfo = NULL;
	uint8 i;
	uint8 chan;

#ifdef WL11N
	if (CHSPEC_IS40(chanspec)) {
		chan = wf_chspec_ctlchan(chanspec);
		if (CHSPEC_SB_UPPER(chanspec))
			rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_40U];
		else
			rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_40L];
	} else
#endif /* WL11N */
	{
		chan = CHSPEC_CHANNEL(chanspec);
		if (CHSPEC_IS2G(chanspec))
			return (wlc_get_2g_regclass(wlc_cm, chan));
		rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_20];
	}

	for (i = 0; rcinfo != NULL && i < rcinfo->len; i++) {
		if (chan == rcinfo->rctbl[i].chan)
			return (rcinfo->rctbl[i].rclass);
	}

	WL_INFORM(("wl%d: No regulatory class assigned for %s channel %d\n",
		wlc_cm->wlc->pub->unit, wlc_cm->country_abbrev, chan));

	return 0;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
int
wlc_dump_rclist(const char *name, uint8 *rclist, uint8 rclen, struct bcmstrbuf *b)
{
	uint i;

	if (!rclen)
		return 0;

	bcm_bprintf(b, "%s [ ", name ? name : "");
	for (i = 0; i < rclen; i++) {
		bcm_bprintf(b, "%d ", rclist[i]);
	}
	bcm_bprintf(b, "]");
	bcm_bprintf(b, "\n");

	return 0;
}

/* format a qdB value as integer and decimal fraction in a bcmstrbuf */
static void
wlc_channel_dump_qdb(struct bcmstrbuf *b, uint qdb)
{
	const char fraction[4][4] = {"   ", ".25", ".5 ", ".75"};

	bcm_bprintf(b, "%2d%s",
	            qdb / WLC_TXPWR_DB_FACTOR,
	            fraction[qdb % WLC_TXPWR_DB_FACTOR]);
}

/* helper function for wlc_channel_dump_txppr() to print one set of power targets with label */
static void
wlc_channel_dump_pwr_range(struct bcmstrbuf *b, const char *label, uint8 *ptr, uint count)
{
	uint i;

	bcm_bprintf(b, "%s ", label);
	for (i = 0; i < count; i++) {
		wlc_channel_dump_qdb(b, ptr[i]);
		bcm_bprintf(b, " ");
	}
	bcm_bprintf(b, "\n");
}

/* helper function to print a target range line with the typical 8 targets */
static void
wlc_channel_dump_pwr_range8(struct bcmstrbuf *b, const char *label, uint8 *ptr)
{
	wlc_channel_dump_pwr_range(b, label, ptr, 8);
}

/* format the contents of a txppr_t struction for a bcmstrbuf */
static void
wlc_channel_dump_txppr(struct bcmstrbuf *b, txppr_t *txppr, int is_ht_format)
{
	bcm_bprintf(b, "20MHz:\n");
	wlc_channel_dump_pwr_range(b,  "CCK         ", txppr->cck, 4);
	wlc_channel_dump_pwr_range8(b, "OFDM        ", txppr->ofdm);
	wlc_channel_dump_pwr_range8(b, "OFDM-CDD    ", txppr->ofdm_cdd);

	if (!is_ht_format) {
		wlc_channel_dump_pwr_range8(b, "MCS-SISO    ", txppr->u20.n.siso);
		wlc_channel_dump_pwr_range8(b, "MCS-CDD     ", txppr->u20.n.cdd);
		wlc_channel_dump_pwr_range8(b, "MCS STBC    ", txppr->u20.n.stbc);
		wlc_channel_dump_pwr_range8(b, "MCS 8~15    ", txppr->u20.n.sdm);
	} else {
		wlc_channel_dump_pwr_range8(b, "1 Nsts 1 Tx ", txppr->u20.ht.s1x1);
		wlc_channel_dump_pwr_range8(b, "1 Nsts 2 Tx ", txppr->u20.ht.s1x2);
		wlc_channel_dump_pwr_range8(b, "1 Nsts 3 Tx ", txppr->ht.u20s1x3);
		wlc_channel_dump_pwr_range8(b, "2 Nsts 2 Tx ", txppr->u20.ht.s2x2);
		wlc_channel_dump_pwr_range8(b, "2 Nsts 3 Tx ", txppr->ht.u20s2x3);
		wlc_channel_dump_pwr_range8(b, "3 Nsts 3 Tx ", txppr->u20.ht.s3x3);
	}

	bcm_bprintf(b, "\n40MHz:\n");
	wlc_channel_dump_pwr_range8(b, "OFDM        ", txppr->ofdm_40);
	wlc_channel_dump_pwr_range8(b, "OFDM-CDD    ", txppr->ofdm_40_cdd);

	if (!is_ht_format) {
		wlc_channel_dump_pwr_range8(b, "MCS-SISO    ", txppr->u40.n.siso);
		wlc_channel_dump_pwr_range8(b, "MCS-CDD     ", txppr->u40.n.cdd);
		wlc_channel_dump_pwr_range8(b, "MCS STBC    ", txppr->u40.n.stbc);
		wlc_channel_dump_pwr_range8(b, "MCS 8~15    ", txppr->u40.n.sdm);
	} else {
		wlc_channel_dump_pwr_range8(b, "1 Nsts 1 Tx ", txppr->u40.ht.s1x1);
		wlc_channel_dump_pwr_range8(b, "1 Nsts 2 Tx ", txppr->u40.ht.s1x2);
		wlc_channel_dump_pwr_range8(b, "1 Nsts 3 Tx ", txppr->ht.u40s1x3);
		wlc_channel_dump_pwr_range8(b, "2 Nsts 2 Tx ", txppr->u40.ht.s2x2);
		wlc_channel_dump_pwr_range8(b, "2 Nsts 3 Tx ", txppr->ht.u40s2x3);
		wlc_channel_dump_pwr_range8(b, "3 Nsts 3 Tx ", txppr->u40.ht.s3x3);
	}

	bcm_bprintf(b, "MCS32        %2d\n", txppr->mcs32);

	bcm_bprintf(b, "\n20 in 40MHz:\n");
	wlc_channel_dump_pwr_range(b,  "CCK         ", txppr->cck_20ul, 4);
	wlc_channel_dump_pwr_range8(b, "OFDM        ", txppr->ofdm_20ul);
	wlc_channel_dump_pwr_range8(b, "OFDM-CDD    ", txppr->ofdm_20ul_cdd);

	wlc_channel_dump_pwr_range8(b, "1 Nsts 1 Tx ", txppr->ht20ul.s1x1);
	wlc_channel_dump_pwr_range8(b, "1 Nsts 2 Tx ", txppr->ht20ul.s1x2);
	wlc_channel_dump_pwr_range8(b, "1 Nsts 3 Tx ", txppr->ht.ul20s1x3);
	wlc_channel_dump_pwr_range8(b, "2 Nsts 2 Tx ", txppr->ht20ul.s2x2);
	wlc_channel_dump_pwr_range8(b, "2 Nsts 3 Tx ", txppr->ht.ul20s2x3);
	wlc_channel_dump_pwr_range8(b, "3 Nsts 3 Tx ", txppr->ht20ul.s3x3);

	bcm_bprintf(b, "\n");
}
#endif /* BCMDBG || BCMDBG_DUMP */

/*
 * 	if (wlc->country_list_extended) all country listable.
 *	else J1 - J10 is excluded.
 */
static bool
wlc_country_listable(struct wlc_info *wlc, const char *countrystr)
{
	bool listable = TRUE;

	if (wlc->country_list_extended == FALSE) {
		if (countrystr[0] == 'J' &&
			(countrystr[1] >= '1' && countrystr[1] <= '9'))
			listable = FALSE;
	}

	return listable;
}

static bool
wlc_buffalo_map_locale(struct wlc_info *wlc, const char* abbrev)
{
	if ((wlc->pub->sih->boardvendor == VENDOR_BUFFALO) &&
	    D11REV_GT(wlc->pub->corerev, 5) && !strcmp("JP", abbrev))
		return TRUE;
	else
		return FALSE;
}

int
wlc_get_channels_in_country(struct wlc_info *wlc, void *arg)
{
	chanvec_t channels;
	wl_channels_in_country_t *cic = (wl_channels_in_country_t *)arg;
	chanvec_t sup_chan;
	uint count, need, i;

	if ((cic->band != WLC_BAND_5G) && (cic->band != WLC_BAND_2G)) {
		WL_ERROR(("Invalid band %d\n", cic->band));
		return BCME_BADBAND;
	}

	if ((NBANDS(wlc) == 1) && (cic->band != (uint)wlc->band->bandtype)) {
		WL_ERROR(("Invalid band %d for card\n", cic->band));
		return BCME_BADBAND;
	}

	if (wlc_channel_get_chanvec(wlc, cic->country_abbrev, cic->band, &channels) == FALSE) {
		WL_ERROR(("Invalid country %s\n", cic->country_abbrev));
		return BCME_NOTFOUND;
	}

	wlc_phy_chanspec_band_validch(wlc->band->pi, cic->band, &sup_chan);
	for (i = 0; i < sizeof(chanvec_t); i++)
		sup_chan.vec[i] &= channels.vec[i];

	/* find all valid channels */
	for (count = 0, i = 0; i < sizeof(sup_chan.vec)*NBBY; i++) {
		if (isset(sup_chan.vec, i))
			count++;
	}

	need = sizeof(wl_channels_in_country_t) + count*sizeof(cic->channel[0]);

	if (need > cic->buflen) {
		/* too short, need this much */
		WL_ERROR(("WLC_GET_COUNTRY_LIST: Buffer size: Need %d Received %d\n",
			need, cic->buflen));
		cic->buflen = need;
		return BCME_BUFTOOSHORT;
	}

	for (count = 0, i = 0; i < sizeof(sup_chan.vec)*NBBY; i++) {
		if (isset(sup_chan.vec, i))
			cic->channel[count++] = i;
	}

	cic->count = count;
	return 0;
}

int
wlc_get_country_list(struct wlc_info *wlc, void *arg)
{
	chanvec_t channels;
	wl_country_list_t *cl = (wl_country_list_t *)arg;
	const locale_info_t *locale = NULL;
	chanvec_t sup_chan;
	uint count, need, i, j;
	uint cntry_locales_size = ARRAYSIZE(cntry_locales);

	if (cl->band_set == FALSE) {
		/* get for current band */
		cl->band = wlc->band->bandtype;
	}

	if ((cl->band != WLC_BAND_5G) && (cl->band != WLC_BAND_2G)) {
		WL_ERROR(("Invalid band %d\n", cl->band));
		return BCME_BADBAND;
	}

	if ((NBANDS(wlc) == 1) && (cl->band != (uint)wlc->band->bandtype)) {
		WL_INFORM(("Invalid band %d for card\n", cl->band));
		cl->count = 0;
		return 0;
	}

	wlc_phy_chanspec_band_validch(wlc->band->pi, cl->band, &sup_chan);

	for (count = 0, i = 0; i < cntry_locales_size; i++) {
		locale = (cl->band == WLC_BAND_5G) ?
			wlc_get_locale_5g(cntry_locales[i].country.locale_5G) :
			wlc_get_locale_2g(cntry_locales[i].country.locale_2G);

		wlc_locale_get_channels(locale, &channels);

		for (j = 0; j < sizeof(sup_chan.vec); j++) {
			if (sup_chan.vec[j] & channels.vec[j]) {
				count++;
				break;
			}
		}
	}

	need = sizeof(wl_country_list_t) + count*WLC_CNTRY_BUF_SZ;

	if (need > cl->buflen) {
		/* too short, need this much */
		WL_ERROR(("WLC_GET_COUNTRY_LIST: Buffer size: Need %d Received %d\n",
			need, cl->buflen));
		cl->buflen = need;
		return BCME_BUFTOOSHORT;
	}

	for (count = 0, i = 0; i < cntry_locales_size; i++) {
		locale = (cl->band == WLC_BAND_5G) ?
			wlc_get_locale_5g(cntry_locales[i].country.locale_5G) :
			wlc_get_locale_2g(cntry_locales[i].country.locale_2G);

		wlc_locale_get_channels(locale, &channels);

		for (j = 0; j < sizeof(sup_chan.vec); j++) {
			if (sup_chan.vec[j] & channels.vec[j]) {
				if ((wlc_country_listable(wlc, cntry_locales[i].abbrev) ==
					TRUE)) {
					strncpy(&cl->country_abbrev[count*WLC_CNTRY_BUF_SZ],
						cntry_locales[i].abbrev, WLC_CNTRY_BUF_SZ);
					count++;
				}
				break;
			}
		}
	}

	cl->count = count;
	return 0;
}

/* Get regulatory max power for a given channel in a given locale.
 * for external FALSE, it returns limit for brcm hw
 * ---- for 2.4GHz channel, it returns cck limit, not ofdm limit.
 * for external TRUE, it returns 802.11d Country Information Element -
 * 	Maximum Transmit Power Level.
 */
int8
wlc_get_reg_max_power_for_channel(wlc_cm_info_t *wlc_cm, int chan, bool external)
{
	int8 maxpwr;
	int indx;
	const locale_info_t *li;

	if (chan <= CH_MAX_2G_CHANNEL) {
		indx = CHANNEL_POWER_IDX_2G_CCK(chan); /* 2.4 GHz cck index */
		li = wlc_get_locale_2g(wlc_cm->country->locale_2G);
	} else {
		indx = CHANNEL_POWER_IDX_5G(chan); /* 5 GHz channel */
		li = wlc_get_locale_5g(wlc_cm->country->locale_5G);
	}

	maxpwr = external ? li->pub_maxpwr[indx] : li->maxpwr[indx];

	return (maxpwr);
}

/*
 * Validate the chanspec for this locale, for 40MHZ we need to also check that the sidebands
 * are valid 20MZH channels in this locale and they are also a legal HT combination
 */
static bool
wlc_valid_chanspec_ext(wlc_cm_info_t *wlc_cm, chanspec_t chspec, bool dualband)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	uint8 channel = CHSPEC_CHANNEL(chspec);

	/* check the chanspec */
	if (wf_chspec_malformed(chspec)) {
		WL_ERROR(("wl%d: malformed chanspec 0x%x\n", wlc->pub->unit, chspec));
		ASSERT(0);
		return FALSE;
	}

	if (CHANNEL_BANDUNIT(wlc_cm->wlc, channel) != CHSPEC_WLCBANDUNIT(chspec))
		return FALSE;

	/* Check a 20Mhz channel */
	if (CHSPEC_IS20(chspec)) {
		if (dualband)
			return (VALID_CHANNEL20_DB(wlc_cm->wlc, channel));
		else
			return (VALID_CHANNEL20(wlc_cm->wlc, channel));
	}

	/* We know we are now checking a 40MHZ channel, so we should only be here
	 * for NPHYS
	 */
	if (WLCISNPHY(wlc->band) || WLCISHTPHY(wlc->band) ||
		(WLCISSSLPNPHY(wlc->band) && SSLPNREV_GT(wlc->band->phyrev, 2))) {
		uint8 upper_sideband = 0, idx;
		uint8 num_ch20_entries = sizeof(chan20_info)/sizeof(struct chan20_info);

		if (!VALID_40CHANSPEC_IN_BAND(wlc, CHSPEC_WLCBANDUNIT(chspec)))
			return FALSE;

		if (dualband) {
			if (!VALID_CHANNEL20_DB(wlc, LOWER_20_SB(channel)) ||
			    !VALID_CHANNEL20_DB(wlc, UPPER_20_SB(channel)))
				return FALSE;
		} else {
			if (!VALID_CHANNEL20(wlc, LOWER_20_SB(channel)) ||
			    !VALID_CHANNEL20(wlc, UPPER_20_SB(channel)))
				return FALSE;
		}

		/* find the lower sideband info in the sideband array */
		for (idx = 0; idx < num_ch20_entries; idx++) {
			if (chan20_info[idx].sb == LOWER_20_SB(channel))
				upper_sideband = chan20_info[idx].adj_sbs;
		}
		/* check that the lower sideband allows an upper sideband */
		if ((upper_sideband & (CH_UPPER_SB | CH_EWA_VALID)) == (CH_UPPER_SB | CH_EWA_VALID))
			return TRUE;
		return FALSE;
	}

	return FALSE;
}

bool
wlc_valid_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
	return wlc_valid_chanspec_ext(wlc_cm, chspec, FALSE);
}

bool
wlc_valid_chanspec_db(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
	return wlc_valid_chanspec_ext(wlc_cm, chspec, TRUE);
}

/*
 *  Fill in 'list' with validated chanspecs, looping through channels using the chanspec_mask.
 */
static void
wlc_chanspec_list(wlc_info_t *wlc, wl_uint32_list_t *list, chanspec_t chanspec_mask)
{
	uint8 channel;
	chanspec_t chanspec;

	for (channel = 0; channel < MAXCHANNEL; channel++) {
		chanspec = (chanspec_mask | channel);
		if (!wf_chspec_malformed(chanspec) &&
		    ((NBANDS(wlc) > 1) ? wlc_valid_chanspec_db(wlc->cmi, chanspec) :
		     wlc_valid_chanspec(wlc->cmi, chanspec))) {
			list->element[list->count] = chanspec;
			list->count++;
		}
	}
}

/*
 * Returns a list of valid chanspecs meeting the provided settings
 */
void
wlc_get_valid_chanspecs(wlc_cm_info_t *wlc_cm, wl_uint32_list_t *list, bool bw20, bool band2G,
                        const char *abbrev)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	chanspec_t chanspec;
	const country_info_t* country;
	const locale_info_t *locale = NULL;
	chanvec_t saved_valid_channels, saved_db_valid_channels;
#ifdef WL11N
	uint8 saved_locale_flags = 0,  saved_db_locale_flags = 0;
	const locale_mimo_info_t *li_mimo = NULL;
	bool saved_cap_40 = TRUE, saved_db_cap_40 = TRUE;
#endif /* WL11N */

	/* Check if this is a valid band for this card */
	if ((NBANDS(wlc) == 1) &&
	    (BAND_5G(wlc->band->bandtype) == band2G))
		return;

	/* see if we need to look up country. Else, current locale */
	if (strcmp(abbrev, "")) {
		country = wlc_country_lookup(wlc, abbrev);

		if (country == NULL) {
			WL_ERROR(("Invalid country \"%s\"\n", abbrev));
			return;
		}

		locale = band2G ?
			wlc_get_locale_2g(country->locale_2G) :
			wlc_get_locale_5g(country->locale_5G);

#ifdef WL11N
		li_mimo = band2G ?
			wlc_get_mimo_2g(country->locale_mimo_2G) :
			wlc_get_mimo_5g(country->locale_mimo_5G);
#endif /* WL11N */
	}

	/* Save current locales */
	if (locale != NULL) {
		bcopy(&wlc->cmi->bandstate[wlc->band->bandunit].valid_channels,
			&saved_valid_channels, sizeof(chanvec_t));
		wlc_locale_get_channels(locale,
			&wlc->cmi->bandstate[wlc->band->bandunit].valid_channels);
		if (NBANDS(wlc) > 1) {
			bcopy(&wlc->cmi->bandstate[OTHERBANDUNIT(wlc)].valid_channels,
			      &saved_db_valid_channels, sizeof(chanvec_t));
			wlc_locale_get_channels(locale,
			      &wlc->cmi->bandstate[OTHERBANDUNIT(wlc)].valid_channels);
		}
	}

#ifdef WL11N
	if (li_mimo != NULL) {
		saved_locale_flags = wlc_cm->bandstate[wlc->band->bandunit].locale_flags;
		wlc_cm->bandstate[wlc->band->bandunit].locale_flags = li_mimo->flags;
		if (NBANDS(wlc) > 1) {
			saved_db_locale_flags = wlc_cm->bandstate[OTHERBANDUNIT(wlc)].locale_flags;
			wlc_cm->bandstate[OTHERBANDUNIT(wlc)].locale_flags = li_mimo->flags;
		}
	}

	/* save 40 MHz cap */
	saved_cap_40 = wlc->band->mimo_cap_40;
	wlc->band->mimo_cap_40 = TRUE;
	if (NBANDS(wlc) > 1) {
		saved_db_cap_40 = wlc->bandstate[OTHERBANDUNIT(wlc)]->mimo_cap_40;
		wlc->bandstate[OTHERBANDUNIT(wlc)]->mimo_cap_40 = TRUE;
	}

#endif /* WL11N */

	/* Go through 2G 20MHZ chanspecs */
	if (band2G && bw20) {
		chanspec = WL_CHANSPEC_BAND_2G | WL_CHANSPEC_BW_20 | WL_CHANSPEC_CTL_SB_NONE;
		wlc_chanspec_list(wlc, list, chanspec);
	}

	/* Go through 5G 20 MHZ chanspecs */
	if (!band2G && bw20) {
		chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_20 | WL_CHANSPEC_CTL_SB_NONE;
		wlc_chanspec_list(wlc, list, chanspec);
	}

	/* Go through 2G 40MHZ chanspecs only if N mode and 40MHZ are both enabled */
	if (band2G && !bw20 &&
	    N_ENAB(wlc->pub) && wlc->bandstate[BAND_2G_INDEX]->mimo_cap_40) {
		chanspec = WL_CHANSPEC_BAND_2G | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_UPPER;
		wlc_chanspec_list(wlc, list, chanspec);
		chanspec = WL_CHANSPEC_BAND_2G | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_LOWER;
		wlc_chanspec_list(wlc, list, chanspec);
	}

	/* Go through 5G 40MHZ chanspecs only if N mode and 40MHZ are both enabled */
	if (!band2G && !bw20 &&
	    N_ENAB(wlc->pub) && ((NBANDS(wlc) > 1) || IS_SINGLEBAND_5G(wlc->deviceid)) &&
	    wlc->bandstate[BAND_5G_INDEX]->mimo_cap_40) {
		chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_UPPER;
		wlc_chanspec_list(wlc, list, chanspec);
		chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_LOWER;
		wlc_chanspec_list(wlc, list, chanspec);
	}

#ifdef WL11N
	/* restore 40 MHz cap */
	wlc->band->mimo_cap_40 = saved_cap_40;
	if (NBANDS(wlc) > 1)
		wlc->bandstate[OTHERBANDUNIT(wlc)]->mimo_cap_40 = saved_db_cap_40;

	if (li_mimo != NULL) {
		wlc_cm->bandstate[wlc->band->bandunit].locale_flags = saved_locale_flags;
		if ((NBANDS(wlc) > 1))
			wlc_cm->bandstate[OTHERBANDUNIT(wlc)].locale_flags = saved_db_locale_flags;
	}
#endif /* WL11N */

	/* Restore the locales if switched */
	if (locale != NULL) {
		bcopy(&saved_valid_channels,
			&wlc->cmi->bandstate[wlc->band->bandunit].valid_channels,
			sizeof(chanvec_t));
		if ((NBANDS(wlc) > 1))
			bcopy(&saved_db_valid_channels,
			      &wlc->cmi->bandstate[OTHERBANDUNIT(wlc)].valid_channels,
			      sizeof(chanvec_t));
	}
}

/* query the channel list given a country and a regulatory class */
uint8
wlc_rclass_get_channel_list(wlc_cm_info_t *cmi, char *abbrev, uint8 rclass,
	bool bw20, wl_uint32_list_t *list)
{
	const rcinfo_t *rcinfo = NULL;
	uint8 ch2g_start = 0, ch2g_end = 0;
	int i;

	if (wlc_us_ccode(abbrev)) {
		if (rclass == WLC_REGCLASS_USA_2G_20MHZ) {
			ch2g_start = 1;
			ch2g_end = 11;
		}
#ifdef BAND5G
		else
			rcinfo = &rcinfo_us_20;
#endif
	} else if (wlc_japan_ccode(abbrev)) {
		if (rclass == WLC_REGCLASS_JPN_2G_20MHZ) {
			ch2g_start = 1;
			ch2g_end = 13;
		}
		else if (rclass == WLC_REGCLASS_JPN_2G_20MHZ_CH14) {
			ch2g_start = 14;
			ch2g_end = 14;
		}
#ifdef BAND5G
		else
			rcinfo = &rcinfo_jp_20;
#endif
	} else {
		if (rclass == WLC_REGCLASS_EUR_2G_20MHZ) {
			ch2g_start = 1;
			ch2g_end = 11;
		}
#ifdef BAND5G
		else
			rcinfo = &rcinfo_eu_20;
#endif
	}

	list->count = 0;
	if (rcinfo == NULL) {
		for (i = ch2g_start; i <= ch2g_end; i ++)
			list->element[list->count ++] = i;
	}
	else {
		for (i = 0; i < rcinfo->len; i ++) {
			if (rclass == rcinfo->rctbl[i].rclass)
				list->element[list->count ++] = rcinfo->rctbl[i].chan;
		}
	}

	return (uint8)list->count;
}

/* Return true if the channel is a valid channel that is radar sensitive
 * in the current country/locale
 */
bool
wlc_radar_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
#ifdef BAND5G     /* RADAR */
	uint channel = CHSPEC_CHANNEL(chspec);
	const chanvec_t *radar_channels;

	/* The radar_channels chanvec may be a superset of valid channels,
	 * so be sure to check for a valid channel first.
	 */

	if (!chspec || !wlc_valid_chanspec_db(wlc_cm, chspec)) {
		return FALSE;
	}

	if (CHSPEC_IS5G(chspec)) {
		radar_channels = wlc_cm->bandstate[BAND_5G_INDEX].radar_channels;

		if (CHSPEC_IS40(chspec)) {
			if (isset(radar_channels->vec, LOWER_20_SB(channel)) ||
			    isset(radar_channels->vec, UPPER_20_SB(channel)))
				return TRUE;
		} else if (isset(radar_channels->vec, channel)) {
			return TRUE;
		}
	}
#endif	/* BAND5G */
	return FALSE;
}

/* Return true if the channel is a valid channel that is radar sensitive
 * in the current country/locale
 */
bool
wlc_restricted_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
	uint channel = CHSPEC_CHANNEL(chspec);
	chanvec_t *restricted_channels;

	/* The restriced_channels chanvec may be a superset of valid channels,
	 * so be sure to check for a valid channel first.
	 */

	if (!chspec || !wlc_valid_chanspec_db(wlc_cm, chspec)) {
		return FALSE;
	}

	restricted_channels = &wlc_cm->restricted_channels;

	if (CHSPEC_IS40(chspec)) {
		if (isset(restricted_channels->vec, LOWER_20_SB(channel)) ||
		    isset(restricted_channels->vec, UPPER_20_SB(channel)))
			return TRUE;
	} else if (isset(restricted_channels->vec, channel)) {
		return TRUE;
	}

	return FALSE;
}

void
wlc_clr_restricted_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
	if (CHSPEC_IS40_UNCOND(chspec)) {
		clrbit(wlc_cm->restricted_channels.vec, LOWER_20_SB(CHSPEC_CHANNEL(chspec)));
		clrbit(wlc_cm->restricted_channels.vec, UPPER_20_SB(CHSPEC_CHANNEL(chspec)));
	} else
		clrbit(wlc_cm->restricted_channels.vec, CHSPEC_CHANNEL(chspec));

	wlc_upd_restricted_chanspec_flag(wlc_cm);
}

static void
wlc_upd_restricted_chanspec_flag(wlc_cm_info_t *wlc_cm)
{
	uint j;

	for (j = 0; j < (int)sizeof(chanvec_t); j++)
		if (wlc_cm->restricted_channels.vec[j]) {
			wlc_cm->has_restricted_ch = TRUE;
			return;
		}

	wlc_cm->has_restricted_ch = FALSE;
}

bool
wlc_has_restricted_chanspec(wlc_cm_info_t *wlc_cm)
{
	return wlc_cm->has_restricted_ch;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
#define QDB_FRAC(x)	(x) / WLC_TXPWR_DB_FACTOR, fraction[(x) % WLC_TXPWR_DB_FACTOR]
int
wlc_channel_dump_locale(void *handle, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = (wlc_info_t*)handle;
	txppr_t txpwr;
	char max_cck_str[32];
	int chan, i;
	int restricted;
	int radar = 0;
	int max_cck, max_ofdm;
	int max_ht20 = 0, max_ht40 = 0;
	char fraction[4][4] = {"  ", ".25", ".5", ".75"};
	char flagstr[64];
	const bcm_bit_desc_t fc_flags[] = {
		{WLC_EIRP, "EIRP"},
		{WLC_DFS_TPC, "DFS/TPC"},
		{WLC_NO_OFDM, "No OFDM"},
		{WLC_NO_40MHZ, "No 40MHz"},
		{WLC_NO_MIMO, "No MIMO"},
		{WLC_RADAR_TYPE_EU, "EU_RADAR"},
		{0, NULL}
	};
	uint8 rclist[MAXRCLISTSIZE], rclen;
	chanspec_t chanspec;
	int quiet;

	bcm_bprintf(b, "srom_ccode \"%s\" srom_regrev %u\n",
	            wlc->cmi->srom_ccode, wlc->cmi->srom_regrev);

	if (NBANDS(wlc) > 1) {
		bcm_format_flags(fc_flags, wlc->cmi->bandstate[BAND_2G_INDEX].locale_flags,
			flagstr, 64);
		bcm_bprintf(b, "2G Flags: %s\n", flagstr);
		bcm_format_flags(fc_flags, wlc->cmi->bandstate[BAND_5G_INDEX].locale_flags,
			flagstr, 64);
		bcm_bprintf(b, "5G Flags: %s\n", flagstr);
	} else {
		bcm_format_flags(fc_flags, wlc->cmi->bandstate[wlc->band->bandunit].locale_flags,
			flagstr, 64);
		bcm_bprintf(b, "%dG Flags: %s\n", BAND_2G(wlc->band->bandtype)?2:5, flagstr);
	}

	if (N_ENAB(wlc->pub))
		bcm_bprintf(b, "  Ch Rdr/reS max        HT 20/40\n");
	else
		bcm_bprintf(b, "  Ch Rdr/reS max\n");

	for (chan = 0; chan < MAXCHANNEL; chan++) {
		chanspec = CH20MHZ_CHSPEC(chan);
		if (!wlc_valid_chanspec_db(wlc->cmi, chanspec)) {
			chanspec = CH40MHZ_CHSPEC(chan, WL_CHANSPEC_CTL_SB_LOWER);
			if (!wlc_valid_chanspec_db(wlc->cmi, chanspec))
				continue;
		}

		radar = wlc_radar_chanspec(wlc->cmi, chanspec);
		restricted = wlc_restricted_chanspec(wlc->cmi, chanspec);
		quiet = wlc_quiet_chanspec(wlc->cmi, chanspec);

		wlc_channel_reg_limits(wlc->cmi, chanspec, &txpwr);

		max_cck = txpwr.cck[0];
		max_ofdm = txpwr.ofdm[0];
#ifdef WL11N
		max_ht20 = txpwr.u20.n.cdd[0];
		max_ht40 = txpwr.u40.n.cdd[0];
#endif /* WL11N */

		if (CHSPEC_IS2G(chanspec))
			snprintf(max_cck_str, sizeof(max_cck_str),
			         "%2d%s/", QDB_FRAC(max_cck));
		else
			strncpy(max_cck_str, "     ", sizeof(max_cck_str));

		if (N_ENAB(wlc->pub))
			bcm_bprintf(b, "%s%3d %s%s%s     %s%2d%s  %2d%s/%2d%s\n",
			            (CHSPEC_IS40(chanspec)?">":" "), chan,
			            (radar ? "R" : " "), (restricted ? "S" : " "),
			            (quiet ? "Q" : " "),
			            max_cck_str, QDB_FRAC(max_ofdm),
			            QDB_FRAC(max_ht20), QDB_FRAC(max_ht40));
		else
			bcm_bprintf(b, "%s%3d %s%s%s     %s%2d%s\n",
			            (CHSPEC_IS40(chanspec)?">":" "), chan,
			            (radar ? "R" : " "), (restricted ? "S" : " "),
			            (quiet ? "Q" : " "),
			            max_cck_str, QDB_FRAC(max_ofdm));
	}

	bzero(rclist, MAXRCLISTSIZE);
	chanspec = wlc->pub->associated ?
	        wlc->home_chanspec : WLC_BAND_PI_RADIO_CHANSPEC;
	rclen = wlc_get_regclass_list(wlc->cmi, rclist, MAXRCLISTSIZE, chanspec, FALSE);
	if (rclen) {
		bcm_bprintf(b, "supported regulatory class:\n");
		for (i = 0; i < rclen; i++)
			bcm_bprintf(b, "%d ", rclist[i]);
		bcm_bprintf(b, "\n");
	}

	bcm_bprintf(b, "has_restricted_ch %s\n", wlc->cmi->has_restricted_ch ? "TRUE" : "FALSE");

#if HTCONF
	{
	struct wlc_channel_txchain_limits *lim;

	lim = &wlc->cmi->bandstate[BAND_2G_INDEX].chain_limits;
	bcm_bprintf(b, "chain limits 2g:");
	for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++)
		bcm_bprintf(b, " %2d%s", QDB_FRAC(lim->chain_limit[i]));
	bcm_bprintf(b, "\n");

	lim = &wlc->cmi->bandstate[BAND_5G_INDEX].chain_limits;
	bcm_bprintf(b, "chain limits 5g:");
	for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++)
		bcm_bprintf(b, " %2d%s", QDB_FRAC(lim->chain_limit[i]));
	bcm_bprintf(b, "\n");
	}
#endif /* HTCONF */

	return 0;
}

#endif /* BCMDBG || BCMDBG_DUMP */

#ifndef INT8_MIN
#define INT8_MIN 0x80
#endif
#ifndef INT8_MAX
#define INT8_MAX 0x7F
#endif

/* Perform an element by element min of txppr structs a and b, and
 * store the result in a.
 */
static void
wlc_channel_txpwr_vec_combine_min(txppr_t *a, txppr_t *b)
{
	uint i;

	for (i = 0; i < sizeof(txppr_t); i++)
		((uint8*)a)[i] = MIN(((uint8*)a)[i], ((uint8*)b)[i]);
}

static void
wlc_channel_margin_summary_mapfn(void *context, uint8 *a, uint8 *b)
{
	uint8 margin;
	uint8 *pmin = (uint8*)context;

	if (*a > *b)
		margin = *a - *b;
	else
		margin = 0;

	*pmin = MIN(*pmin, margin);
}

static void
wlc_channel_max_summary_mapfn(void *context, uint8 *a, uint8 *ignore)
{
	uint8 *pmax = (uint8*)context;

	*pmax = MAX(*pmax, *a);
}

/* Map the given function with its context value over the two
 * uint8 vectors
 */
static void
wlc_channel_map_uint8_vec_binary(wlc_channel_mapfn_t fn, void* context, uint len,
	uint8 *vec_a, uint8 *vec_b)
{
	uint i;

	for (i = 0; i < len; i++)
		(fn)(context, vec_a + i, vec_b + i);
}

/* Map the given function with its context value over the power targets
 * appropriate for the given band and bandwidth in two txppr structs.
 * If the band is 2G, DSSS/CCK rates will be included.
 * If the bandwidth is 20MHz, only 20MHz targets are included.
 * If the bandwidth is 40MHz, both 40MHz and 20in40 targets are included.
 */ 
static void
wlc_channel_map_txppr_binary(wlc_channel_mapfn_t fn, void* context, uint bandtype, uint bw,
	txppr_t *a, txppr_t *b)
{

/* macro for the typical 8 rates in a group (OFDM, MCS0-7, 8-15, 16-23) */
#define MAP_GROUP(member) \
	wlc_channel_map_uint8_vec_binary(fn, context, 8, a->member, b->member)

	if (bw == WL_CHANSPEC_BW_20) {
		if (bandtype == WL_CHANSPEC_BAND_2G) {
			wlc_channel_map_uint8_vec_binary(fn, context, WL_NUM_RATES_CCK,
			                                 a->cck, b->cck);
		}

		MAP_GROUP(ofdm);
	}

#ifdef WL11N
	/* map over 20MHz rates for 20MHz channels */
	if (bw == WL_CHANSPEC_BW_20) {
		if (bandtype == WL_CHANSPEC_BAND_2G) {
			wlc_channel_map_uint8_vec_binary(fn, context, WL_NUM_RATES_CCK,
			                                 a->cck_cdd.s1x2, b->cck_cdd.s1x2);
			wlc_channel_map_uint8_vec_binary(fn, context, WL_NUM_RATES_CCK,
			                                 a->cck_cdd.s1x3, b->cck_cdd.s1x3);
		}

		MAP_GROUP(ofdm_cdd);

		MAP_GROUP(u20.ht.s1x1);
		MAP_GROUP(u20.ht.s1x2);
		MAP_GROUP(ht.u20s1x3);

		MAP_GROUP(u20.ht.s2x2);
		MAP_GROUP(ht.u20s2x3);

		MAP_GROUP(u20.ht.s3x3);

	} else
	/* map over 40MHz and 20in40 rates for 40MHz channels */
	{
		MAP_GROUP(ofdm_40);
		MAP_GROUP(ofdm_40_cdd);

		MAP_GROUP(u40.ht.s1x1);
		MAP_GROUP(u40.ht.s1x2);
		MAP_GROUP(ht.u40s1x3);

		MAP_GROUP(u40.ht.s2x2);
		MAP_GROUP(ht.u40s2x3);

		MAP_GROUP(u40.ht.s3x3);

		/* 20in40 legacy */
		if (bandtype == WL_CHANSPEC_BAND_2G) {
			wlc_channel_map_uint8_vec_binary(fn, context, WL_NUM_RATES_CCK,
			                                 a->cck_20ul, b->cck_20ul);
			wlc_channel_map_uint8_vec_binary(fn, context, WL_NUM_RATES_CCK,
			                                 a->cck_20ul_cdd.s1x2,
			                                 b->cck_20ul_cdd.s1x2);
			wlc_channel_map_uint8_vec_binary(fn, context, WL_NUM_RATES_CCK,
			                                 a->cck_20ul_cdd.s1x3,
			                                 b->cck_20ul_cdd.s1x3);
		}
		MAP_GROUP(ofdm_20ul);
		MAP_GROUP(ofdm_20ul_cdd);

		/* 20in40 HT */
		MAP_GROUP(ht20ul.s1x1);
		MAP_GROUP(ht20ul.s1x2);
		MAP_GROUP(ht20ul.s2x2);
		MAP_GROUP(ht20ul.s3x3);
		MAP_GROUP(ht.ul20s1x3);
		MAP_GROUP(ht.ul20s2x3);

		/* MCS 32 oddball */
		wlc_channel_map_uint8_vec_binary(fn, context, 1,
		                                 &a->mcs32, &b->mcs32);
	}
#endif /* WL11N */
}

/* calculate the offset from each per-rate power target in txpwr to the supplied
 * limit (or zero if txpwr[x] is less than limit[x]), and return the smallest
 * offset of relevant rates for bandtype/bw.
 */
static uint8
wlc_channel_txpwr_margin(txppr_t *txpwr, txppr_t *limit, uint bandtype, uint bw)
{
	uint8 margin = 0xff;

	wlc_channel_map_txppr_binary(wlc_channel_margin_summary_mapfn, &margin,
	                             bandtype, bw, txpwr, limit);

	return margin;
}

/* calculate the max power target of relevant rates for bandtype/bw */
static uint8
wlc_channel_txpwr_max(txppr_t *txpwr, uint bandtype, uint bw)
{
	uint8 pwr_max = 0;

	wlc_channel_map_txppr_binary(wlc_channel_max_summary_mapfn, &pwr_max,
	                             bandtype, bw, txpwr, NULL);

	return pwr_max;
}

struct txp_range {
	int start;
	int end;
};

static const struct txp_range txp_ranges_2g_20[] = {
	{0, 51},
	{153, 168},
	{201, 208},
	{-1, -1}
};

static const struct txp_range txp_ranges_2g_40[] = {
	{52, 152},
	{169, 200},
	{209, 216},
	{-1, -1}
};

static const struct txp_range txp_ranges_5g_20[] = {
	{4, 51},
	{153, 168},
	{-1, -1}
};

static const struct txp_range txp_ranges_5g_40[] = {
	{52, 100},
	{105, 152},
	{169, 200},
	{-1, -1}
};

/* return a txppr_t struct with the phy srom limits for the given channel */
static void
wlc_channel_srom_limits(wlc_cm_info_t *wlc_cm, chanspec_t chanspec,
	txppr_t *srommin, txppr_t *srommax)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	wlc_phy_t *pi = wlc->band->pi;
	const struct txp_range *txp_ranges;
	int range_idx, start, end, txp_rate_idx;
	uint8 min_srom, max_srom;
	uint channel = wf_chspec_ctlchan(chanspec);

	if (srommin != NULL)
		memset(srommin, 0, sizeof(txppr_t));
	if (srommax != NULL)
		memset(srommax, 0, sizeof(txppr_t));

	if (!WLCISHTPHY(wlc_cm->wlc->band))
		return;

	if (CHSPEC_IS2G(chanspec)) {
		if (CHSPEC_IS20(chanspec))
			txp_ranges = txp_ranges_2g_20;
		else
			txp_ranges = txp_ranges_2g_40;
	} else {
		if (CHSPEC_IS20(chanspec))
			txp_ranges = txp_ranges_5g_20;
		else
			txp_ranges = txp_ranges_5g_40;
	}

	for (range_idx = 0; txp_ranges[range_idx].start != -1; range_idx++) {
		start = txp_ranges[range_idx].start;
		end = txp_ranges[range_idx].end;

		for (txp_rate_idx = start; txp_rate_idx <= end; txp_rate_idx++) {
			wlc_phy_txpower_sromlimit(pi, channel, &min_srom, &max_srom, txp_rate_idx);

			if (srommin != NULL)
				((uint8*)srommin)[txp_rate_idx] = min_srom;
			if (srommax != NULL)
				((uint8*)srommax)[txp_rate_idx] = max_srom;
		}
	}
}

/* Set a per-chain power limit for the given band 
 * Per-chain offsets will be used to make sure the max target power does not exceed 
 * the per-chain power limit
 */
int
wlc_channel_band_chain_limit(wlc_cm_info_t *wlc_cm, uint bandtype,
                             struct wlc_channel_txchain_limits *lim)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	txppr_t txpwr;
	int bandunit = (bandtype == WLC_BAND_2G) ? BAND_2G_INDEX : BAND_5G_INDEX;

	if (!WLCISHTPHY(wlc_cm->wlc->band))
		return BCME_UNSUPPORTED;

	wlc_cm->bandstate[bandunit].chain_limits = *lim;

	if (CHSPEC_WLCBANDUNIT(wlc->chanspec) != bandunit)
		return 0;

	/* update the current tx chain offset if we just updated this band's limits */
	wlc_channel_txpower_limits(wlc_cm, &txpwr);
	wlc_channel_update_txchain_offsets(wlc_cm, &txpwr);

	return 0;
}

/* update the per-chain tx power offset given the current power targets to implement
 * the correct per-chain tx power limit
 */
static int
wlc_channel_update_txchain_offsets(wlc_cm_info_t *wlc_cm, txppr_t *txpwr)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	struct wlc_channel_txchain_limits *lim;
	wl_txchain_pwr_offsets_t offsets;
	chanspec_t chanspec;
	txppr_t srompwr;
	int i, err;
	int max_pwr;
	int band, bw;
	int limits_present = FALSE;
	uint8 delta, margin, err_margin;
#ifdef BCMDBG
	char chanbuf[CHANSPEC_STR_LEN];
#endif

	if (!WLCISHTPHY(wlc->band))
		return BCME_UNSUPPORTED;

	chanspec = wlc->chanspec;
	band = CHSPEC_BAND(chanspec);
	bw = CHSPEC_BW(chanspec);

	/* initialze the offsets to a default of no offset */
	memset(&offsets, 0, sizeof(wl_txchain_pwr_offsets_t));

	lim = &wlc_cm->bandstate[CHSPEC_WLCBANDUNIT(chanspec)].chain_limits;

	/* see if there are any chain limits specified */
	for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++) {
		if (lim->chain_limit[i] < WLC_TXPWR_MAX) {
			limits_present = TRUE;
			break;
		}
	}

	/* if there are no limits, we do not need to do any calculations */
	if (limits_present) {

		/* find the max power target for this channel and impose
		 * a txpwr delta per chain to meet the specified chain limits
		 * Bound the delta by the tx power margin  
		 */

		/* get the srom min powers */
		wlc_channel_srom_limits(wlc_cm, wlc->chanspec, &srompwr, NULL);

		/* find the dB margin we can use to adjust tx power */
		margin = wlc_channel_txpwr_margin(txpwr, &srompwr, band, bw);

		/* reduce the margin by the error margin 1.5dB backoff */
		err_margin = 6;	/* 1.5 dB in qdBm */
		margin = (margin >= err_margin) ? margin - err_margin : 0;

		/* get the srom max powers */
		wlc_channel_srom_limits(wlc_cm, wlc->chanspec, NULL, &srompwr);

		/* combine the srom limits with the given regulatory limits
		 * to find the actual channel max
		 */
		wlc_channel_txpwr_vec_combine_min(&srompwr, txpwr);

		max_pwr = (int)wlc_channel_txpwr_max(&srompwr, band, bw);

		WL_NONE(("wl%d: %s: channel %s max_pwr %d margin %d\n",
		         wlc->pub->unit, __FUNCTION__,
		         wf_chspec_ntoa(wlc->chanspec, chanbuf), max_pwr, margin));

		/* for each chain, calculate an offset that keeps the max tx power target
		 * no greater than the chain limit
		 */
		for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++) {
			WL_NONE(("wl%d: %s: chain_limit[%d] %d",
			         wlc->pub->unit, __FUNCTION__,
			         i, lim->chain_limit[i]));
			if (lim->chain_limit[i] < max_pwr) {
				delta = max_pwr - lim->chain_limit[i];

				WL_NONE((" desired delta -%u lim delta -%u",
				         delta, MIN(delta, margin)));

				/* limit to the margin allowed for our adjustmets */
				delta = MIN(delta, margin);

				offsets.offset[i] = -delta;
			}
			WL_NONE(("\n"));
		}
	} else {
		WL_NONE(("wl%d: %s skipping limit calculation since limits are MAX\n",
		         wlc->pub->unit, __FUNCTION__));
	}

	err = wlc_iovar_op(wlc, "txchain_pwr_offset", NULL, 0,
	                   &offsets, sizeof(wl_txchain_pwr_offsets_t), IOV_SET, NULL);
	if (err) {
		WL_ERROR(("wl%d: txchain_pwr_offset failed: error %d\n",
		          wlc->pub->unit, err));
	}

	return err;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int
wlc_channel_dump_reg_ppr(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cm = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cm->wlc;
	txppr_t txpwr;
	char chanbuf[CHANSPEC_STR_LEN];

	wlc_channel_reg_limits(wlc_cm, wlc->chanspec, &txpwr);

	bcm_bprintf(b, "Regulatory Limits for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, &txpwr, WLCISHTPHY(wlc->band));

	return 0;
}

/* dump of regulatory power with local constraint factored in for the current channel */
static int
wlc_channel_dump_reg_local_ppr(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cm = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cm->wlc;
	txppr_t txpwr;
	char chanbuf[CHANSPEC_STR_LEN];

	wlc_channel_txpower_limits(wlc_cm, &txpwr);

	bcm_bprintf(b, "Regulatory Limits with constraint for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, &txpwr, WLCISHTPHY(wlc->band));

	return 0;
}

/* dump of srom per-rate max/min values for the current channel */
static int
wlc_channel_dump_srom_ppr(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cm = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cm->wlc;
	txppr_t srompwr;
	char chanbuf[CHANSPEC_STR_LEN];

	wlc_channel_srom_limits(wlc_cm, wlc->chanspec, NULL, &srompwr);

	bcm_bprintf(b, "PHY/SROM Max Limits for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, &srompwr, WLCISHTPHY(wlc->band));

	wlc_channel_srom_limits(wlc_cm, wlc->chanspec, &srompwr, NULL);

	bcm_bprintf(b, "PHY/SROM Min Limits for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, &srompwr, WLCISHTPHY(wlc->band));

	return 0;
}

static void
wlc_channel_margin_calc_mapfn(void *ignore, uint8 *a, uint8 *b)
{
	if (*a > *b)
		*a = *a - *b;
	else
		*a = 0;
}

/* dumps dB margin between a rate an the lowest allowable power target, and
 * summarize the min of the margins for the current channel
 */
static int
wlc_channel_dump_margin(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cm = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cm->wlc;
	txppr_t txpwr, srommin;
	chanspec_t chanspec;
	int band, bw;
	uint8 margin;
	char chanbuf[CHANSPEC_STR_LEN];

	chanspec = wlc->chanspec;
	band = CHSPEC_BAND(chanspec);
	bw = CHSPEC_BW(chanspec);

	memset(&txpwr, 0, sizeof(txppr_t));
	memset(&srommin, 0, sizeof(txppr_t));

	wlc_channel_txpower_limits(wlc_cm, &txpwr);

	/* get the srom min powers */
	wlc_channel_srom_limits(wlc_cm, wlc->chanspec, &srommin, NULL);

	/* find the dB margin we can use to adjust tx power */
	margin = wlc_channel_txpwr_margin(&txpwr, &srommin, band, bw);

	/* calulate the per-rate margins */
	wlc_channel_map_txppr_binary(wlc_channel_margin_calc_mapfn, NULL,
	                             band, bw, &txpwr, &srommin);

	bcm_bprintf(b, "Power margin for channel %s, min = %u\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf), margin);
	wlc_channel_dump_txppr(b, &txpwr, WLCISHTPHY(wlc->band));

	return 0;
}
#endif /* BCMDBG || BCMDBG_DUMP */
