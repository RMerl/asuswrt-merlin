/*
 * CLM API functions.
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_clm.c 5832 2013-04-26 17:16:49Z $
 */

#include <wlc_cfg.h>
#include "wlc_clm.h"
#include "wlc_clm_data.h"
#include <bcmwifi_rates.h>
#include <typedefs.h>


#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>

/******************************
* MODULE MACROS AND CONSTANTS *
*******************************
*/

#define FORMAT_VERSION_MAJOR 11 /* BLOB format version major number */
#define FORMAT_VERSION_MINOR 0 /* BLOB format version minor number */
#define FORMAT_MIN_COMPAT_MAJOR 7 /* Minimum supported binary BLOB format's major version */

#if (FORMAT_VERSION_MAJOR != CLM_FORMAT_VERSION_MAJOR) || (FORMAT_VERSION_MINOR != \
	CLM_FORMAT_VERSION_MINOR)
#error CLM data format version mismatch between wlc_clm.c and wlc_clm_data.h
#endif

#ifndef NULL
	/** Null pointer */
	#define NULL 0
#endif

/** Boolean type definition for use inside this module */
typedef int MY_BOOL;

#ifndef TRUE
	/** TRUE for MY_BOOL */
	#define TRUE 1
#endif

#ifndef FALSE
	/** FALSE for MY_BOOL */
	#define FALSE 0
#endif

#ifndef OFFSETOF
	/** Offset of given field in given structure */
#ifdef _WIN64
	#define OFFSETOF(s, m) (unsigned long long)&(((s *)0)->m)
#else
	#define OFFSETOF(s, m) (unsigned long)&(((s *)0)->m)
#endif /* _WIN64 */
#endif /* OFFSETOF */

#ifndef ARRAYSIZE
	/** Number of elements in given array */
	#define ARRAYSIZE(x) (unsigned)(sizeof(x)/sizeof(x[0]))
#endif

#if WL_NUMRATES >= 178
	/** Defined if bcmwifi_rates.h contains TXBF rates */
	#define CLM_TXBF_RATES_SUPPORTED
#endif

/** CLM data source IDs */
typedef enum data_source_id {
	DS_INC,		/* Incremental CLM data. Placed first so we look there before base data. */
	DS_MAIN,	/* Main CLM data */
	DS_NUM		/* Number of CLM data source IDs */
} data_source_id_t;

/** Module constants */
enum clm_internal_const {
	/* Masks that define iterator contents */
	ITER_DS_MAIN  = 0x40000000, /* Pointed data is in main CLM data source */
	ITER_DS_INC	  = 0x00000000, /* Pointed data is in incremental CLM data source */
	ITER_DS_MASK  = 0x40000000, /* Mask of data source field of iterator */
	ITER_IDX_MASK = 0x3FFFFFFF, /* Mask of index field of iterator */
	NUM_MCS_MODS  = 8,	/* Number of MCS/OFDM rates, differing only by modulation */
	NUM_DSSS_MODS = 4,	/* Number of DSSS rates, differing only by modulation */
	CHAN_STRIDE   = 4,	/* Channel number stride at 20MHz */
	SUB_CHAN_PATH_COUNT_MASK = 0xF0, /* Mask of count field in subchannel path descriptor */
	SUB_CHAN_PATH_COUNT_OFFSET = 4,	/* Offset of count field in subchannel path descriptor */
	/* Prefill constant for power limits used in clm_limits() */
	UNSPECIFIED_POWER = CLM_DISABLED_POWER + 1,

	/* clm_country_locales::computed_flags: country flags taken from main data */
	COUNTRY_FLAGS_DS_MAIN = (unsigned char)DS_MAIN,
	/* clm_country_locales::computed_flags: country flags taken from incremental data */
	COUNTRY_FLAGS_DS_INC  = (unsigned char)DS_INC,
	/* clm_country_locales::computed_flags: mask for country flags source field */
	COUNTRY_FLAGS_DS_MASK = (unsigned char)(DS_NUM - 1)
};

/** Rate types */
enum clm_rate_type {
	RT_DSSS,	/* DSSS (802.11b) rate */
	RT_OFDM,	/* OFDM (802.11a/g) rate */
	RT_MCS,		/* MCS (802.11n) rate */
};

/** CLM data set descriptor */
typedef struct data_dsc {
	/** Pointer to registry (TOC) structure of CLM data */
	const clm_registry_t *data;

	/* Relocation factor of CLM DATA set: value that shall be added to pointer contained in
	   CLM data set to get a true pointer (e.g. 0 if data is not relocated)
	*/
	unsigned relocation;

	/** Valid channel comb sets (per band, per bandwidth) */
	clm_channel_comb_set_t valid_channel_combs[CLM_BAND_NUM][CLM_BW_NUM];

	/** True for 10 bit locale indices, false for 8 bit locale indices */
	MY_BOOL loc_idx10;

	/** True if region records have flags field */
	MY_BOOL has_region_flags;

	/** Length of region record in bytes */
	int country_rev_rec_len;

	/** Address of header for version strings */
	const clm_data_header_t *header;

	/** True if BLOB capable of containing 160MHz data */
	MY_BOOL has_160mhz;

	/** Per-bandwidth base addresses of channel range descriptors */
	const clm_channel_range_t *chan_ranges_bw[CLM_BW_NUM];

	/** Per-bandwidth base addresses of rate set definitions */
	const unsigned char *rate_sets_bw[CLM_BW_NUM];
} data_dsc_t;

/** Addresses of locale-related data */
typedef struct locale_data {
	/** Locale definition */
	const unsigned char *def_ptr;

	/** Per-bandwidth base addresses of channel range descriptors */
	const clm_channel_range_t * const *chan_ranges_bw;

	/** Per-bandwidth base addresses of rate set definitions */
	const unsigned char * const *rate_sets_bw;

	/** Base address of valid channel sets definitions */
	const unsigned char *valid_channels;

	/** Base address of restricted sets definitions */
	const unsigned char *restricted_channels;

	/** Per-bandwidth channel combs */
	const clm_channel_comb_set_t *combs[CLM_BW_NUM];

	/** 80MHz subchannel rules - deprecated! */
	clm_sub_chan_region_rules_80_t sub_chan_channel_rules_80;

	/** 160MHz subchannel rules - deprecated! */
	clm_sub_chan_region_rules_160_t sub_chan_channel_rules_160;

} locale_data_t;

/** Addresses of region (country)-related data */
typedef struct country_data {
	/** Per-bandwidth base addresses of channel range descriptors in data set this region
	 * defined in
	 */
	const clm_channel_range_t * const *chan_ranges_bw;

	/** 80MHz subchannel rules */
	clm_sub_chan_region_rules_80_t sub_chan_channel_rules_80;

	/** 160MHz subchannel rules */
	clm_sub_chan_region_rules_160_t sub_chan_channel_rules_160;
} country_data_t;


/** Descriptors of main and incremental data sources */
static data_dsc_t data_sources[] = {
	{ NULL, 0, {{{0, 0}}}, 0, 0, 0, NULL, 0, {NULL}, {NULL} },
	{ NULL, 0, {{{0, 0}}}, 0, 0, 0, NULL, 0, {NULL}, {NULL} }
};


/* Rate type table and related get/set macros */

#ifdef WL_CLM_RATE_TYPE_SPARSE
/* Legacy version */
static char rate_type[WL_NUMRATES];
#else
/** Rate type by rate index. Values are from enum clm_rate_type, compressed to 2-bits */
static char rate_type[(WL_NUMRATES + 3)/4];
#endif

/* Gives rate type for given rate index. Use of named constants make this expression too ugly */
#define RATE_TYPE(rate_idx)	\
	((get_rate_type()[(unsigned)(rate_idx) >> 2] >> (((rate_idx) & 3) << 1)) & 3)

/* Sets rate type for given rate index */
#define SET_RATE_TYPE(rate_idx, rt)                                               \
	get_rate_type()[(unsigned)(rate_idx) >> 2] &= ~(3 << (((rate_idx) & 3) << 1)); \
	get_rate_type()[(unsigned)(rate_idx) >> 2] |= rt << (((rate_idx) & 3) << 1)     \


/** Valid 40M channels of 2.4G band */
static const struct clm_channel_comb valid_channel_combs_2g_40m[] = {
	{  3,  11, 1}, /* 3 - 11 with step of 1 */
};

/** Set of 40M 2.4G valid channel combs */
static const struct clm_channel_comb_set valid_channel_2g_40m_set = {
	1, valid_channel_combs_2g_40m
};

/** Valid 40M channels of 5G band */
static const struct clm_channel_comb valid_channel_combs_5g_40m[] = {
	{ 38,  62, 8}, /* 38 - 62 with step of 8 */
	{102, 142, 8}, /* 102 - 142 with step of 8 */
	{151, 159, 8}, /* 151 - 159 with step of 8 */
};

/** Set of 40M 5G valid channel combs */
static const struct clm_channel_comb_set valid_channel_5g_40m_set = {
	3, valid_channel_combs_5g_40m
};

/** Valid 80M channels of 5G band */
static const struct clm_channel_comb valid_channel_combs_5g_80m[] = {
	{ 42,  58, 16}, /* 42 - 58 with step of 16 */
	{106, 138, 16}, /* 106 - 138 with step of 16 */
	{155, 155, 16}, /* 155 - 155 with step of 16 */
};

/** Set of 80M 5G valid channel combs */
static const struct clm_channel_comb_set valid_channel_5g_80m_set = {
	3, valid_channel_combs_5g_80m
};

/** Valid 160M channels of 5G band */
static const struct clm_channel_comb valid_channel_combs_5g_160m[] = {
	{ 50,  50, 32}, /* 50 - 50 with step of 32 */
	{114, 114, 32}, /* 114 - 114 with step of 32 */
};

/** Set of 160M 5G valid channel combs */
static const struct clm_channel_comb_set valid_channel_5g_160m_set = {
	3, valid_channel_combs_5g_160m
};


/* This is a hack to accomodate PHOENIX2 builds which don't know about the VHT rates */
#ifndef WL_RATESET_SZ_VHT_MCS
#define CLM_NO_VHT_RATES
#endif


/** Maps CLM_DATA_FLAG_WIDTH_...  to clm_bandwidth_t */
static const unsigned char bw_width_to_idx_non_ac[CLM_DATA_FLAG_WIDTH_MASK + 1] = {
	CLM_BW_20, CLM_BW_40, 0, 0, 0, 0, 0, 0, 0, 0
};

#ifdef WL11AC
static const unsigned char bw_width_to_idx_ac[CLM_DATA_FLAG_WIDTH_MASK + 1] = {
	CLM_BW_20, CLM_BW_40, 0, 0, 0, 0, 0, 0, CLM_BW_80, CLM_BW_160
};
#endif

static const unsigned char* bw_width_to_idx;

/** Maps limit types to descriptors of paths from main channel to correspondent subchannel
 * Each descriptor is a byte. High nibble contains number of steps in path, bits in low nibble
 * describe steps. 0 - select lower subchannel, 1 - select upper subchannel. Least significant
 * bit corresponds to last step
 */
static const unsigned char subchan_paths[] = {
	0x00, /* CHANNEL */
	0x10, /* SUBCHAN_L */
	0x11, /* SUBCHAN_U */
#ifdef WL11AC
	0x20, /* SUBCHAN_LL */
	0x21, /* SUBCHAN_LU */
	0x22, /* SUBCHAN_UL */
	0x23, /* SUBCHAN_UU */
	0x30, /* SUBCHAN_LLL */
	0x31, /* SUBCHAN_LLU */
	0x32, /* SUBCHAN_LUL */
	0x33, /* SUBCHAN_LUU */
	0x34, /* SUBCHAN_ULL */
	0x35, /* SUBCHAN_ULU */
	0x36, /* SUBCHAN_UUL */
	0x37, /* SUBCHAN_UUU */
#endif
};

/** Offsets of per-antenna power limits inside TX power record */
static const int antenna_power_offsets[] = {
	CLM_LOC_DSC_POWER_IDX, CLM_LOC_DSC_POWER1_IDX, CLM_LOC_DSC_POWER2_IDX
};


/****************************
* MODULE INTERNAL FUNCTIONS *
*****************************
*/

/** Local implementation of strcmp()
 * Implemented here because standard library might be unavailable
 * \param[in] str1 First zero-terminated string. Must be nonzero
 * \param[in] str2 Second zero-terminated string. Must be nonzero
 * \return <0 if first string is less than second, >0 if second string more than first,
	==0 if strings are equal
 */
static int my_strcmp(const char *str1, const char *str2)
{
	while (*str1 && *str2 && (*str1 == *str2)) {
		++str1;
		++str2;
	}
	return *str1 - *str2;
}

/** Local implementation of memset()
 * Implemented here because standard library might be unavailable
 * \param[in] to Buffer to fill
 * \param[in] c Character to fill with
 * \param[in] len Length of buffer to fill
 */
static void my_memset(void *to, char c, int len)
{
	char *t;
	for (t = (char *)to; len--; *t++ = c);
}


/** Returns value of field with given name in given (main of incremental) CLM data set
 * Interface to get_data that converts field name to offset of field inside CLM data registry
 * \param[in] ds_id CLM data set identifier
 * \param[in] field Name of field in struct clm_registry
 * \return Field value as (const void *) pointer. NULL if given data source was not set
 */
#define GET_DATA(ds_id, field) get_data(ds_id, OFFSETOF(clm_registry_t, field))


/* Accessor function to avoid data_sources structure from getting into ROM.
 * Don't have this function in ROM.
 */
static data_dsc_t *
get_data_sources(int ds_idx)
{
	return &data_sources[ds_idx];
}

/* Accessor function to avoid rate_type structure from getting into ROM.
 * Don't have this function in ROM.
 */
static char *
get_rate_type(void)
{
	return rate_type;
}

/** Returns value of field with given offset in given (main or incremental) CLM data set
 * \param[in] ds_idx CLM data set identifier
 * \param[in] field_offset Offset of field in struct clm_registry
 * \return Field value as (const void *) pointer. NULL if given data source was not set
 */
static const void *
get_data(int ds_idx, unsigned long field_offset)
{
	data_dsc_t *ds = get_data_sources(ds_idx);
	const uintptr paddr = (uintptr)ds->data;
	const char **pp = (const char **)(paddr + field_offset);

	return (ds->data && *pp) ? (*pp + ds->relocation) : NULL;
}

/** Converts given pointer value, fetched from some (possibly relocated) CLM data structure
	to its 'true' value
 * Note that GET_DATA() return values are already converted to 'true' values
 * \param[in] ds_idx Identifier of CLM data set that contained given pointer
 * \param[in] ptr Pointer to convert
 * \return 'True' (unrelocated) pointer value
 */
static const void *relocate_ptr(int ds_idx, const void *ptr)
{
	return ptr ? ((const char *)ptr + get_data_sources(ds_idx)->relocation) : NULL;
}

/** Retrieves CLM data set identifier and index, contained in given iterator
 * Applicable to previously 'packed' iterator. Not applicable to 'just initialized' iterators
 * \param[in] iter Iterator to unpack
 * \param[out] ds_id CLM data set identifier retrieved from iterator
 * \param[out] idx Index retrieved from iterator
 */
static void iter_unpack(int iter, data_source_id_t *ds_id, int *idx)
{
	--iter;
	if (ds_id) {
		*ds_id = ((iter & ITER_DS_MASK) == ITER_DS_MAIN) ? DS_MAIN : DS_INC;
	}
	if (idx) {
		*idx = iter & ITER_IDX_MASK;
	}
}

/** Creates (packs) iterator out of CLM data set identifier and index
 * \param[out] iter Resulted iterator
 * \param[in] ds_id CLM data set identifier to put to iterator
 * \param[in] idx Index Index to put to iterator
 */
static void iter_pack(int *iter, data_source_id_t ds_id, int idx)
{
	if (iter) {
		*iter = (((ds_id == DS_MAIN) ? ITER_DS_MAIN : ITER_DS_INC) |
			(idx & ITER_IDX_MASK)) + 1;
	}
}

/** Traversal of byte string sequence
 * \param[in] byte_string_seq Byte string sequence to traverse
 * \param[in] idx Index of string to find
 * \return Address of idx's string in a sequence
 */
static const unsigned char *get_byte_string(const unsigned char *byte_string_seq, int idx)
{
	while (idx--) {
		byte_string_seq += *byte_string_seq + 1;
	}
	return byte_string_seq;
}

/** Looks up byte string that contains locale definition, precomputes locale-related data
 * \param[in] locales Region locales
 * \param[in] loc_type Type of locale to retrieve (CLM_LOC_IDX_...)
 * \param[out] loc_data Locale-related data. If locale not found all fields are zeroed
 * \return TRUE in case of success, FALSE if region locale definitions structure contents is
	invalid
 */
static MY_BOOL get_loc_def(const clm_country_locales_t *locales, int loc_type,
	locale_data_t *loc_data)
{
	data_source_id_t ds_id;
	int idx, bw_idx;
	MY_BOOL is_base;
	clm_band_t band;
	const unsigned char *loc_def;
	const data_dsc_t *ds;

	my_memset(loc_data, 0, sizeof(*loc_data));

	switch (loc_type) {
	case CLM_LOC_IDX_BASE_2G:
		iter_unpack(locales->locale_2G, &ds_id, &idx);
		is_base = TRUE;
		band = CLM_BAND_2G;
		break;
	case CLM_LOC_IDX_BASE_5G:
		iter_unpack(locales->locale_5G, &ds_id, &idx);
		is_base = TRUE;
		band = CLM_BAND_5G;
		break;
	case CLM_LOC_IDX_HT_2G:
		iter_unpack(locales->locale_2G_HT, &ds_id, &idx);
		is_base = FALSE;
		band = CLM_BAND_2G;
		break;
	case CLM_LOC_IDX_HT_5G:
		iter_unpack(locales->locale_5G_HT, &ds_id, &idx);
		is_base = FALSE;
		band = CLM_BAND_5G;
		break;
	default:
		return FALSE;
	}
	if (idx == CLM_LOC_NONE) {
		return TRUE;
	}
	if (idx == CLM_LOC_SAME) {
		return FALSE;
	}
	ds = get_data_sources(ds_id);
	loc_def =  (const unsigned char *)relocate_ptr(ds_id, ds->data->locales[loc_type]);
	while (idx--) {
		int tx_rec_len;
		if (is_base) {
			loc_def += CLM_LOC_DSC_BASE_HDR_LEN;
			loc_def += 1 + CLM_LOC_DSC_PUB_REC_LEN * (int)(*loc_def);
		}
		for (;;) {
			tx_rec_len = CLM_LOC_DSC_TX_REC_LEN +
				((*loc_def &
				CLM_DATA_FLAG_PER_ANT_MASK) >> CLM_DATA_FLAG_PER_ANT_SHIFT);
			if (!(*loc_def++ & CLM_DATA_FLAG_MORE)) {
				break;
			}
			loc_def += 1 + tx_rec_len * (int)(*loc_def);
		}
		loc_def += 1 + tx_rec_len * (int)(*loc_def);
	}
	loc_data->def_ptr = loc_def;
	loc_data->chan_ranges_bw = ds->chan_ranges_bw;
	loc_data->rate_sets_bw = ds->rate_sets_bw;
	loc_data->valid_channels = (const unsigned char *)GET_DATA(ds_id, locale_valid_channels);
	loc_data->restricted_channels =
		(const unsigned char *)GET_DATA(ds_id, restricted_channels);
	for (bw_idx = 0; bw_idx < CLM_BW_NUM; ++bw_idx) {
		loc_data->combs[bw_idx] =
			&get_data_sources(ds_id)->valid_channel_combs[band][bw_idx];
	}
	return TRUE;
}

/** Tries to fill valid_channel_combs using given CLM data source
 * This function takes information about 20MHz channels from BLOB, 40MHz channels from
	valid_channel_...g_40m_set structures hardcoded in this module to save BLOB space
 * \param[in] ds_id Identifier of CLM data to get information from
 */
static void try_fill_valid_channel_combs(data_source_id_t ds_id)
{
	const clm_channel_comb_set_t *combs20[CLM_BAND_NUM];
	int band, bw;
	MY_BOOL empty = TRUE;
	if (!get_data_sources(ds_id)->data) {
		for (band = 0; band < CLM_BAND_NUM; ++band) {
			for (bw = 0; bw < CLM_BW_NUM; ++bw) {
				get_data_sources(ds_id)->valid_channel_combs[band][bw].num = 0;
			}
		}
		return;
	}
	combs20[CLM_BAND_2G] = (const clm_channel_comb_set_t*)GET_DATA(ds_id,
		valid_channels_2g_20m);
	combs20[CLM_BAND_5G] = (const clm_channel_comb_set_t*)GET_DATA(ds_id,
		valid_channels_5g_20m);
	for (band = 0; band < CLM_BAND_NUM; ++band) {
		if (combs20[band]->num) {
			empty = FALSE;
			break;
		}
	}
	if (empty && (ds_id == DS_INC)) {
		for (band = 0; band < CLM_BAND_NUM; ++band) {
			combs20[band] =
				&get_data_sources(DS_MAIN)->valid_channel_combs[band][CLM_BW_20];
		}
	}
	for (band = 0; band < CLM_BAND_NUM; ++band) {
		get_data_sources(ds_id)->valid_channel_combs[band][CLM_BW_20] = *combs20[band];
		if (!(empty && (ds_id == DS_INC))) {
			get_data_sources(ds_id)->valid_channel_combs[band][CLM_BW_20].set =
				(const clm_channel_comb_t *)relocate_ptr(ds_id, combs20[band]->set);
		}
	}
	get_data_sources(ds_id)->valid_channel_combs[CLM_BAND_2G][CLM_BW_40] =
		valid_channel_2g_40m_set;
	get_data_sources(ds_id)->valid_channel_combs[CLM_BAND_5G][CLM_BW_40] =
		valid_channel_5g_40m_set;
#ifdef WL11AC
	get_data_sources(ds_id)->valid_channel_combs[CLM_BAND_5G][CLM_BW_80] =
		valid_channel_5g_80m_set;
	get_data_sources(ds_id)->valid_channel_combs[CLM_BAND_5G][CLM_BW_160] =
		valid_channel_5g_160m_set;
#endif
}

/** In given comb set looks up for comb that contains given channel
 * \param[in] channel Channel to find comb for
 * \param[in] combs Comb set to find comb in
 * \return NULL or address of given comb
 */
static const clm_channel_comb_t *get_comb(int channel, const clm_channel_comb_set_t *combs)
{
	const clm_channel_comb_t *ret, *comb_end;
	for (ret = combs->set, comb_end = ret + combs->num; ret != comb_end; ++ret) {
		if ((channel >= ret->first_channel) && (channel <= ret->last_channel) &&
		   (((channel - ret->first_channel) % ret->stride) == 0))
		{
			return ret;
		}
	}
	return NULL;
}

/** Among combs whose first channel is greater than given looks up one with minimum first
	channel
 * \param[in] channel Channel to find comb for
 * \param[in] combs Comb set to find comb in
 * \return Address of found comb, NULL if all combs have first channel smaller than given
 */
static const clm_channel_comb_t *get_next_comb(int channel,
	const clm_channel_comb_set_t *combs)
{
	const clm_channel_comb_t *ret, *comb, *comb_end;
	for (ret = NULL, comb = combs->set, comb_end = comb+combs->num; comb != comb_end; ++comb) {
		if (comb->first_channel <= channel) {
			continue;
		}
		if (!ret || (comb->first_channel < ret->first_channel)) {
			ret = comb;
		}
	}
	return ret;
}

/** Fills channel set structure from given source
 * \param[out] channels Channel set to fill
 * \param[in] channel_defs Byte string sequence that contains channel set definitions
 * \param[in] def_idx Index of byte string that contains required channel set definition
 * \param[in] ranges Vector of channel ranges
 * \param[in] combs Set of combs of valid channel numbers
 */
static void get_channels(clm_channels_t *channels, const unsigned char *channel_defs,
	unsigned char def_idx, const clm_channel_range_t *ranges,
	const clm_channel_comb_set_t *combs)
{
	int num_ranges;

	if (!channels) {
		return;
	}
	my_memset(channels->bitvec, 0, sizeof(channels->bitvec));
	if (!channel_defs) {
		return;
	}
	if (def_idx == CLM_RESTRICTED_SET_NONE) {
		return;
	}
	channel_defs = get_byte_string(channel_defs, def_idx);
	num_ranges = *channel_defs++;
	while (num_ranges--) {
		unsigned char r = *channel_defs++;
		if (r == CLM_RANGE_ALL_CHANNELS) {
			const clm_channel_comb_t *comb = combs->set;
			int num_combs;
			for (num_combs = combs->num; num_combs--; ++comb) {
				int chan;
				for (chan = comb->first_channel; chan <= comb->last_channel;
					chan += comb->stride) {
					channels->bitvec[chan / 8] |=
						(unsigned char)(1 << (chan % 8));
				}
			}
		} else {
			int chan = ranges[r].start, end = ranges[r].end;
			const clm_channel_comb_t *comb = get_comb(chan, combs);
			if (!comb) {
				continue;
			}
			for (;;) {
				channels->bitvec[chan / 8] |= (unsigned char)(1 << (chan % 8));
				if (chan >= end) {
					break;
				}
				if (chan < comb->last_channel) {
					chan += comb->stride;
					continue;
				}
				comb = get_next_comb(chan, combs);
				if (!comb || (comb->first_channel > end)) {
					break;
				}
				chan = comb->first_channel;
			}
		}
	}
}

/** True if given channel belongs to given range and belong to comb that represent this range
 * \param[in] channel Channel in question
 * \param[in] range Range in question
 * \param[in] combs Comb set
 * \return True if given channel belongs to given range and belong to comb that represent
	this range
 */
static MY_BOOL channel_in_range(int channel, const clm_channel_range_t *range,
	const clm_channel_comb_set_t *combs)
{
	const clm_channel_comb_t *comb;
	if ((channel < range->start) || (channel > range->end)) {
		return FALSE;
	}
	comb = get_comb(range->start, combs);
	while (comb && (comb->last_channel < channel)) {
		comb = get_next_comb(comb->last_channel, combs);
	}
	return comb && (comb->first_channel <= channel) &&
		!((channel - comb->first_channel) % comb->stride);
}

/** Fills rate_type[] */
static void fill_rate_types(void)
{
	/* Rate range descriptor */
	static const struct {
		int start;             /* First rate in range */
		int length;            /* Number of rates in range */
		enum clm_rate_type rt; /* Rate type for range */
	} rate_ranges[] = {
		{0,                      WL_NUMRATES,        RT_MCS},  /* Prefill with default */
		{WL_RATE_1X1_DSSS_1,     WL_RATESET_SZ_DSSS, RT_DSSS},
		{WL_RATE_1X2_DSSS_1,     WL_RATESET_SZ_DSSS, RT_DSSS},
		{WL_RATE_1X3_DSSS_1,     WL_RATESET_SZ_DSSS, RT_DSSS},
		{WL_RATE_1X1_OFDM_6,     WL_RATESET_SZ_OFDM, RT_OFDM},
		{WL_RATE_1X2_CDD_OFDM_6, WL_RATESET_SZ_OFDM, RT_OFDM},
		{WL_RATE_1X3_CDD_OFDM_6, WL_RATESET_SZ_OFDM, RT_OFDM},
#ifdef CLM_TXBF_RATES_SUPPORTED
		{WL_RATE_1X2_TXBF_OFDM_6, WL_RATESET_SZ_OFDM, RT_OFDM},
		{WL_RATE_1X3_TXBF_OFDM_6, WL_RATESET_SZ_OFDM, RT_OFDM},
#endif
	};
	int range_idx;
	for (range_idx = 0; range_idx < (int)ARRAYSIZE(rate_ranges); ++range_idx) {
		int rate_idx = rate_ranges[range_idx].start;
		int end = rate_idx + rate_ranges[range_idx].length;
		enum clm_rate_type rt = rate_ranges[range_idx].rt;
		do {
			SET_RATE_TYPE(rate_idx, rt);
		} while (++rate_idx < end);
	}
}

/** Initializes given CLM data source
 * \param[in] header Header of CLM data
 * \param[in] ds_id Identifier of CLM data set
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if data address is zero or if
	CLM data tag is absent at given address or if major number of CLM data format version is
	not supported by CLM API
 */
static clm_result_t data_init(const clm_data_header_t *header, data_source_id_t ds_id)
{
	data_dsc_t *ds = get_data_sources(ds_id);
	if (header) {
		MY_BOOL has_registry_flags = TRUE;
		if (my_strcmp(header->header_tag, CLM_HEADER_TAG)) {
			return CLM_RESULT_ERR;
		}
		if ((header->format_major == 5) && (header->format_minor == 1)) {
			has_registry_flags = FALSE;
		} else if ((header->format_major > FORMAT_VERSION_MAJOR) ||
			(header->format_major < FORMAT_MIN_COMPAT_MAJOR))
		{
			return CLM_RESULT_ERR;
		}
		ds->relocation = (unsigned)((const char*)header -(const char*)header->self_pointer);
		ds->data = (const clm_registry_t*)relocate_ptr(ds_id, header->data);
		if (has_registry_flags) {
			int blob_num_rates = (ds->data->flags & CLM_REGISTRY_FLAG_NUM_RATES_MASK) >>
				CLM_REGISTRY_FLAG_NUM_RATES_SHIFT;
			if (blob_num_rates && (blob_num_rates != WL_NUMRATES)) {
				return CLM_RESULT_ERR;
			}
		}
		ds->header = header;
		ds->loc_idx10 = ds->has_region_flags =
		  has_registry_flags && (ds->data->flags & CLM_REGISTRY_FLAG_COUNTRY_10_FL);
		ds->country_rev_rec_len =
		  has_registry_flags && (ds->data->flags & CLM_REGISTRY_FLAG_COUNTRY_10_FL) ?
		  sizeof(clm_country_rev_definition10_fl_t) : sizeof(clm_country_rev_definition_t);
		ds->has_160mhz = has_registry_flags && (ds->data->flags & CLM_REGISTRY_FLAG_160MHZ);
		if (has_registry_flags && (ds->data->flags & CLM_REGISTRY_FLAG_PER_BW_RS)) {
			ds->chan_ranges_bw[CLM_BW_20] =
				(const clm_channel_range_t *)GET_DATA(ds_id, channel_ranges_20m);
			ds->rate_sets_bw[CLM_BW_20] =
				(const unsigned char *)GET_DATA(ds_id, locale_rate_sets_20m);
			ds->chan_ranges_bw[CLM_BW_40] =
				(const clm_channel_range_t *)GET_DATA(ds_id, channel_ranges_40m);
			ds->rate_sets_bw[CLM_BW_40] =
				(const unsigned char *)GET_DATA(ds_id, locale_rate_sets_40m);
	#ifdef WL11AC
			ds->chan_ranges_bw[CLM_BW_80] =
				(const clm_channel_range_t *)GET_DATA(ds_id, channel_ranges_80m);
			ds->rate_sets_bw[CLM_BW_80] =
				(const unsigned char *)GET_DATA(ds_id, locale_rate_sets_80m);
			ds->chan_ranges_bw[CLM_BW_160] =
				(const clm_channel_range_t *)GET_DATA(ds_id, channel_ranges_160m);
			ds->rate_sets_bw[CLM_BW_160] =
				(const unsigned char *)GET_DATA(ds_id, locale_rate_sets_160m);
	#endif
		} else {
			const clm_channel_range_t *channel_ranges =
				(const clm_channel_range_t *)GET_DATA(ds_id, channel_ranges_20m);
			const unsigned char *rate_sets =
				(const unsigned char *)GET_DATA(ds_id, locale_rate_sets_20m);
			int bw_idx;
			for (bw_idx = 0; bw_idx < CLM_BW_NUM; ++bw_idx) {
				ds->chan_ranges_bw[bw_idx] = channel_ranges;
				ds->rate_sets_bw[bw_idx] = rate_sets;
			}
		}
	} else {
		ds->relocation = 0;
		ds->data = NULL;
	}
	try_fill_valid_channel_combs(ds_id);
	if (ds_id == DS_MAIN) {
		try_fill_valid_channel_combs(DS_INC);
	}
	fill_rate_types();
	return CLM_RESULT_OK;
}

/** True if two given CC/Revs are equal
 * \param[in] cc_rev1 First CC/Rev
 * \param[in] cc_rev2 Second CC/Rev
 * \return True if two given CC/Revs are equal
 */
static MY_BOOL cc_rev_equal(const clm_cc_rev_t *cc_rev1, const clm_cc_rev_t *cc_rev2)
{
	return (cc_rev1->cc[0] == cc_rev2->cc[0]) && (cc_rev1->cc[1] == cc_rev2->cc[1]) &&
		(cc_rev1->rev == cc_rev2->rev);
}

/** True if two given CCs are equal
 * \param[in] cc1 First CC
 * \param[in] cc2 Second CC
 * \return
 */
static MY_BOOL cc_equal(const char *cc1, const char *cc2)
{
	return (cc1[0] == cc2[0]) && (cc1[1] == cc2[1]);
}

/** Copies CC
 * \param[out] to Destination CC
 * \param[in] from Source CC
 */
static void copy_cc(char *to, const char *from)
{
	to[0] = from[0];
	to[1] = from[1];
}

/** Translates old-style CC to new style
 * For now only "\0\0" -> "ww" translation is performed
 * \param[in,out] cc On input points to CC to be translated, on output points to translated CC
 */
static ccode_t ww = {'w', 'w'};
static void translate_cc(const char **cc)
{
	if (((*cc)[0] == 0) && ((*cc)[1] == 0)) {
		*cc = ww;
	}
}

/** Looks for given aggregation in given data set
 * \param[in] ds_id Identifier of CLM data set
 * \param[in] cc_rev Aggregation's default region CC/rev
 * \param[out] idx Optional output parameter - index of aggregation in set
 * \return Address of aggregation or NULL
 */
static const clm_aggregate_cc_t *get_aggregate(data_source_id_t ds_id,
	const clm_cc_rev_t *cc_rev, int *idx)
{
	const clm_aggregate_cc_set_t *aggregate_set =
		(const clm_aggregate_cc_set_t *)GET_DATA(ds_id, aggregates);
	const clm_aggregate_cc_t *ret, *start, *end;

	if (!aggregate_set || !aggregate_set->set) {
		return NULL;
	}
	start = ret =
		(const clm_aggregate_cc_t *)relocate_ptr(ds_id, aggregate_set->set);
	for (end = ret + aggregate_set->num;
		(ret < end) && !cc_rev_equal(&ret->def_reg, cc_rev);
		++ret);
	if (idx) {
		*idx = (int)(ret - start);
	}
	return (ret < end) ? ret : NULL;
}

/** Looks for mapping with given CC in given aggregation
 * \param[in] ds_id Identifier of CLM data set aggregation belongs to
 * \param[in] agg Aggregation to look in
 * \param[in] cc CC to look for
 * \return Address of found mapping or NULL
 */
static const clm_cc_rev_t *get_mapping(data_source_id_t ds_id, const clm_aggregate_cc_t *agg,
	const ccode_t cc)
{
	const clm_cc_rev_t *ret =
		agg ? (const clm_cc_rev_t *)relocate_ptr(ds_id, agg->regions) : NULL;
	const clm_cc_rev_t *end;
	if (!ret) {
		return NULL;
	}
	for (end = ret + agg->num_regions; (ret < end) && !cc_equal(cc, ret->cc); ++ret);
	return (ret < end) ? ret : NULL;
}

/** Reads locale index from region record
 * \param[in] ds_id Identifier of CLM data set region record belongs to
 * \param[in] country_definition Region definition record
 * \param[in] loc_type Locale type
 * \return Locale index or one of CLM_LOC_... special indices
 */
static int loc_idx(data_source_id_t ds_id,
	const clm_country_rev_definition10_fl_t *country_definition, int loc_type)
{
	int ret = country_definition->locales[loc_type];
	if (get_data_sources(ds_id)->loc_idx10) {
		ret |= ((int)country_definition->hi_bits << ((CLM_LOC_IDX_NUM-loc_type)*2)) & 0x300;
	} else {
		switch (ret) {
		case CLM_LOC_NONE & 0xFF:
			ret = CLM_LOC_NONE;
			break;
		case CLM_LOC_SAME & 0xFF:
			ret = CLM_LOC_SAME;
			break;
		case CLM_LOC_DELETED & 0xFF:
			ret = CLM_LOC_DELETED;
			break;
		}
	}
	return ret;
}

/** True if given country definition marked as deleted
 * \param[in] ds_id Identifier of CLM data set country definition belongs to
 * \param[in] country_definition Country definition structure
 * \return True if given country definition marked as deleted
 */
static MY_BOOL country_deleted(data_source_id_t ds_id,
	const clm_country_rev_definition10_fl_t *country_definition)
{
	return loc_idx(ds_id, country_definition, 0) == CLM_LOC_DELETED;
}

/** Returns country definition by index
 * \param ds_id Data set to look in
 * \param idx Country definition index
 * \return Country definition address, NULL if data set contains no countries
 */
static const clm_country_rev_definition10_fl_t *get_country_def_by_idx(data_source_id_t ds_id,
	int idx)
{
	const clm_country_rev_definition_set_t *country_set =
		(const clm_country_rev_definition_set_t *)GET_DATA(ds_id, countries);
	if (!country_set) {
		return NULL;
	}
	return (const clm_country_rev_definition10_fl_t *)
		((const char *)relocate_ptr(ds_id, country_set->set) +
		 idx*get_data_sources(ds_id)->country_rev_rec_len);
}

/** Looks up for definition of given country (region) in given CLM data set
 * \param ds_id Data set id to look in
 * \param cc_rev Region CC/rev to look for
 * \param idx Optional output parameter: index of found country definition
 * \return Address of country definition or NULL
 */
static const clm_country_rev_definition10_fl_t *get_country_def(data_source_id_t ds_id,
	const clm_cc_rev_t *cc_rev, int *idx)
{
	const clm_country_rev_definition_set_t *country_set =
		(const clm_country_rev_definition_set_t *)GET_DATA(ds_id, countries);
	const clm_country_rev_definition10_fl_t *ret = get_country_def_by_idx(ds_id, 0);
	int country_rec_len = get_data_sources(ds_id)->country_rev_rec_len;
	int i;
	if (!ret) {
		return NULL;
	}
	for (i = 0; i < country_set->num;
		++i, ret = (const clm_country_rev_definition10_fl_t *)
		((const char *)ret + country_rec_len))
	{
		if (cc_rev_equal(&ret->cc_rev, cc_rev)) {
			if (idx) {
				*idx = i;
			}
			return ret;
		}
	}
	return NULL;
}

#ifdef WL11AC
/** Finds subchannel rule for given main channel and fills channel table for it
 * \param[out] actual_table Table to fill. Includes channel numbers only for bandwidths
 *  included in subchannel rule
 * \param[in] full_table Full subchannel table to take channel numbers from
 * \param[in] limits_type Limits type (subchannel ID)
 * \param[in] channel Main channel
 * \param[in] ranges Channel ranges' table
 * \param[in] comb_set Comb set for main channel's bandwidth
 * \param[in] rules Subchannel rules
 * \param[in] num_rules Number of subchannel rules
 * \param[in] rule_len Length of subchannel rule
 */
static void fill_actual_subchan_table(unsigned char actual_table[CLM_BW_NUM],
	unsigned char full_table[CLM_BW_NUM], int limits_type, int channel,
	const clm_channel_range_t *ranges, const clm_channel_comb_set_t *comb_set,
	const void *rules, int num_rules, int rule_len)
{
	/* Rule pointer as character pointer (to simplify address arithmetic) */
	const unsigned char *r;
	/* Converting subchannel ID from clm_limits_type_t to clm_data_sub_chan_id_t */
	limits_type -= CLM_SUB_CHAN_IDX_BASE;
	/* Loop over subchannel rules rules */
	for (r = (const unsigned char *)rules; num_rules--; r += rule_len) {
		/* Did we find rule for range that contains given main channel? */
		if (channel_in_range(channel, ranges + r[CLM_SUB_CHAN_RANGE_IDX], comb_set)) {
			/* Rule found - now let's fill the table */

			/* Loop index, actual type is clm_bandwidth_t */
			int bw_idx;
			/* Subchannel rule (cell in 'Rules' spreadsheet) */
			unsigned char sub_chan_rule = r [CLM_SUB_CHAN_RULES_IDX + limits_type];
			/* Probing all possible bandwidths */
			for (bw_idx = 0; bw_idx < CLM_BW_NUM; ++bw_idx) {
				/* If bandwidth included to rule */
				if ((1 << bw_idx) & sub_chan_rule) {
					/* Copy channel number for this bandwidth from full table */
					actual_table[bw_idx] = full_table[bw_idx];
				}
			}
			return; /* All done, no need to look for more rules */
		}
	}
}
#endif /* WL11AC */

clm_result_t clm_init(const struct clm_data_header *header)
{
#ifdef WL11AC
	bw_width_to_idx = bw_width_to_idx_ac;
#else
	bw_width_to_idx = bw_width_to_idx_non_ac;
#endif
	return data_init(header, DS_MAIN);
}

clm_result_t clm_set_inc_data(const struct clm_data_header *header)
{
	return data_init(header, DS_INC);
}

clm_result_t clm_iter_init(int *iter)
{
	if (iter) {
		*iter = CLM_ITER_NULL;
		return CLM_RESULT_OK;
	}
	return CLM_RESULT_ERR;
}

clm_result_t clm_limits_params_init(struct clm_limits_params *params)
{
	if (!params) {
		return CLM_RESULT_ERR;
	}
	params->bw = CLM_BW_20;
	params->antenna_idx = 0;
	params->sar = 0x7F;
	return CLM_RESULT_OK;
}

clm_result_t clm_country_iter(clm_country_t *country, ccode_t cc, unsigned int *rev)
{
	data_source_id_t ds_id;
	int idx;
	clm_result_t ret = CLM_RESULT_OK;
	if (!country || !cc || !rev) {
		return CLM_RESULT_ERR;
	}
	if (*country == CLM_ITER_NULL) {
		ds_id = DS_INC;
		idx = 0;
	} else {
		iter_unpack(*country, &ds_id, &idx);
		++idx;
	}
	for (;;) {
		const clm_country_rev_definition_set_t *country_set =
			(const clm_country_rev_definition_set_t *)GET_DATA(ds_id, countries);
		const clm_country_rev_definition10_fl_t *country_definition;
		if (!country_set || (idx >= country_set->num)) {
			if (ds_id == DS_INC) {
				ds_id = DS_MAIN;
				idx = 0;
				continue;
			} else {
				ret = CLM_RESULT_NOT_FOUND;
				idx = country_set ? country_set->num : 0;
				break;
			}
		}
		country_definition = get_country_def_by_idx(ds_id, idx);
		if (country_deleted(ds_id, country_definition)) {
			++idx;
			continue;
		}
		if ((ds_id == DS_MAIN) && get_data_sources(DS_INC)->data) {
			const clm_country_rev_definition_set_t *inc_country_set =
				(const clm_country_rev_definition_set_t *)
				GET_DATA(DS_INC, countries);
			const clm_country_rev_definition10_fl_t *inc_country_definition =
				get_country_def_by_idx(DS_INC, 0);
			int country_rec_len = get_data_sources(DS_INC)->country_rev_rec_len;
			int i;
			for (i = 0; i < inc_country_set->num;
				++i, inc_country_definition =
				(const clm_country_rev_definition10_fl_t *)
				((const char *)inc_country_definition + country_rec_len))
			{
				if (cc_rev_equal(&country_definition->cc_rev,
					&inc_country_definition->cc_rev)) {
					break;
				}
			}
			if (i < inc_country_set->num) {
				++idx;
				continue;
			}
		}
		copy_cc(cc, country_definition->cc_rev.cc);
		*rev = country_definition->cc_rev.rev;
		break;
	}
	iter_pack(country, ds_id, idx);
	return ret;
}

clm_result_t clm_country_lookup(const ccode_t cc, unsigned int rev, clm_country_t *country)
{
	int ds_idx;
	clm_cc_rev_t cc_rev;
	if (!cc || !country) {
		return CLM_RESULT_ERR;
	}
	translate_cc(&cc);
	copy_cc(cc_rev.cc, cc);
	cc_rev.rev = (char)rev;
	for (ds_idx = 0; ds_idx < DS_NUM; ++ds_idx) {
		int idx;
		const clm_country_rev_definition10_fl_t *country_definition =
			get_country_def((data_source_id_t)ds_idx, &cc_rev, &idx);
		if (!country_definition) {
			continue;
		}
		if (country_deleted((data_source_id_t)ds_idx, country_definition)) {
			return CLM_RESULT_NOT_FOUND;
		}
		iter_pack(country, (data_source_id_t)ds_idx, idx);
		return CLM_RESULT_OK;
	}
	return CLM_RESULT_NOT_FOUND;
}

clm_result_t clm_country_def(const clm_country_t country, clm_country_locales_t *locales)
{
	struct locale_field_dsc {
		unsigned clm_offset;
		int data_idx;
	};
	static const struct locale_field_dsc fields[] = {
		{OFFSETOF(clm_country_locales_t, locale_2G),	CLM_LOC_IDX_BASE_2G},
		{OFFSETOF(clm_country_locales_t, locale_5G),	CLM_LOC_IDX_BASE_5G},
		{OFFSETOF(clm_country_locales_t, locale_2G_HT), CLM_LOC_IDX_HT_2G},
		{OFFSETOF(clm_country_locales_t, locale_5G_HT), CLM_LOC_IDX_HT_5G},
	};
	data_source_id_t ds_id;
	int idx, i;
	const clm_country_rev_definition_set_t *country_set;
	const clm_country_rev_definition10_fl_t *country_definition;
	const clm_country_rev_definition10_fl_t *main_country_definition = NULL;
	if (!locales) {
		return CLM_RESULT_ERR;
	}
	iter_unpack(country, &ds_id, &idx);
	country_set =
		(const clm_country_rev_definition_set_t *)GET_DATA(ds_id, countries);
	if (!country_set || ((unsigned)idx >= (unsigned)country_set->num)) {
		return CLM_RESULT_NOT_FOUND;
	}
	country_definition = get_country_def_by_idx(ds_id, idx);
	for (i = 0; i < (int)ARRAYSIZE(fields); ++i) {
		data_source_id_t locale_ds_id = ds_id;
		int locale_idx = loc_idx(locale_ds_id, country_definition, fields[i].data_idx);
		if (locale_idx == CLM_LOC_SAME) {
			if (!main_country_definition) {
				main_country_definition =
					get_country_def(DS_MAIN, &country_definition->cc_rev, NULL);
			}
			locale_ds_id = DS_MAIN;
			locale_idx = main_country_definition
			  ? loc_idx(locale_ds_id, main_country_definition, fields[i].data_idx)
			  : CLM_LOC_NONE;
		}
		iter_pack((int*)((char *)locales + fields[i].clm_offset), locale_ds_id, locale_idx);
	}
	locales->country_flags =
	  get_data_sources(ds_id)->has_region_flags ? country_definition->flags : 0;
	locales->computed_flags = (unsigned char)ds_id;
	return CLM_RESULT_OK;
}

clm_result_t clm_country_channels(const clm_country_locales_t *locales, clm_band_t band,
	clm_channels_t *valid_channels, clm_channels_t *restricted_channels)
{
	clm_channels_t dummy_valid_channels;
	locale_data_t loc_data;

	if (!locales || ((unsigned)band >= (unsigned)CLM_BAND_NUM)) {
		return CLM_RESULT_ERR;
	}
	if (!restricted_channels && !valid_channels) {
		return CLM_RESULT_OK;
	}
	if (!valid_channels) {
		valid_channels = &dummy_valid_channels;
	}
	if (!get_loc_def(locales, (band == CLM_BAND_2G) ? CLM_LOC_IDX_BASE_2G : CLM_LOC_IDX_BASE_5G,
		&loc_data))
	{
		return CLM_RESULT_ERR;
	}
	if (loc_data.def_ptr) {
		get_channels(valid_channels, loc_data.valid_channels,
			loc_data.def_ptr[CLM_LOC_DSC_CHANNELS_IDX],
			loc_data.chan_ranges_bw[CLM_BW_20],
			loc_data.combs[CLM_BW_20]);
		get_channels(restricted_channels, loc_data.restricted_channels,
			loc_data.def_ptr[CLM_LOC_DSC_RESTRICTED_IDX],
			loc_data.chan_ranges_bw[CLM_BW_20],
			loc_data.combs[CLM_BW_20]);
		if (restricted_channels) {
			int i;
			for (i = 0; i < (int)ARRAYSIZE(restricted_channels->bitvec); ++i) {
				restricted_channels->bitvec[i] &= valid_channels->bitvec[i];
			}
		}
	} else {
		get_channels(valid_channels, NULL, 0, NULL, NULL);
		get_channels(restricted_channels, NULL, 0, NULL, NULL);
	}
	return CLM_RESULT_OK;
}

clm_result_t clm_country_flags(const clm_country_locales_t *locales, clm_band_t band,
	unsigned long *ret_flags)
{
	int base_ht_idx;
	locale_data_t base_ht_loc_data[2];
	if (!locales || !ret_flags || ((unsigned)band >= (unsigned)CLM_BAND_NUM)) {
		return CLM_RESULT_ERR;
	}
	*ret_flags = (unsigned long)(CLM_FLAG_DFS_NONE | CLM_FLAG_NO_40MHZ | CLM_FLAG_NO_80MHZ |
		CLM_FLAG_NO_160MHZ | CLM_FLAG_NO_MIMO);
	if (!get_loc_def(locales, (band == CLM_BAND_2G) ? CLM_LOC_IDX_BASE_2G : CLM_LOC_IDX_BASE_5G,
		&(base_ht_loc_data[0])))
	{
		return CLM_RESULT_ERR;
	}
	if (base_ht_loc_data[0].def_ptr) {
		switch (base_ht_loc_data[0].def_ptr[CLM_LOC_DSC_FLAGS_IDX] &
			CLM_DATA_FLAG_DFS_MASK) {
		case CLM_DATA_FLAG_DFS_NONE:
			*ret_flags |= CLM_FLAG_DFS_NONE;
			break;
		case CLM_DATA_FLAG_DFS_EU:
			*ret_flags |= CLM_FLAG_DFS_EU;
			break;
		case CLM_DATA_FLAG_DFS_US:
			*ret_flags |= CLM_FLAG_DFS_US;
			break;
		}
		if (base_ht_loc_data[0].def_ptr[CLM_LOC_DSC_FLAGS_IDX] & CLM_DATA_FLAG_FILTWAR1) {
			*ret_flags |= CLM_FLAG_FILTWAR1;
		}
		base_ht_loc_data[0].def_ptr += CLM_LOC_DSC_BASE_HDR_LEN;
		base_ht_loc_data[0].def_ptr +=
			1 + CLM_LOC_DSC_PUB_REC_LEN * (int)*(base_ht_loc_data[0].def_ptr);
	}
	if (!get_loc_def(locales, (band == CLM_BAND_2G) ? CLM_LOC_IDX_HT_2G : CLM_LOC_IDX_HT_5G,
		&(base_ht_loc_data[1])))
	{
		return CLM_RESULT_ERR;
	}
	for (base_ht_idx = 0; base_ht_idx < (int)ARRAYSIZE(base_ht_loc_data); ++base_ht_idx) {
		unsigned char flags;
		const unsigned char *tx_rec = base_ht_loc_data[base_ht_idx].def_ptr;

		if (!tx_rec) {
			continue;
		}
		do {
			int num_rec, tx_rec_len;
			MY_BOOL eirp;
			unsigned char bw_idx;
			unsigned long bw_flag_mask = 0;

			flags = *tx_rec++;
			if ((flags & CLM_DATA_FLAG_WIDTH_MASK) == CLM_DATA_FLAG_WIDTH_40) {
				bw_flag_mask = CLM_FLAG_NO_40MHZ;
			} else if ((flags & CLM_DATA_FLAG_WIDTH_MASK) == CLM_DATA_FLAG_WIDTH_80) {
				bw_flag_mask = CLM_FLAG_NO_80MHZ;
			} else if ((flags & CLM_DATA_FLAG_WIDTH_MASK) == CLM_DATA_FLAG_WIDTH_160) {
				bw_flag_mask = CLM_FLAG_NO_160MHZ;
			}
			tx_rec_len = CLM_LOC_DSC_TX_REC_LEN + ((flags &
				CLM_DATA_FLAG_PER_ANT_MASK) >> CLM_DATA_FLAG_PER_ANT_SHIFT);
			if (tx_rec_len != CLM_LOC_DSC_TX_REC_LEN) {
				*ret_flags |= CLM_FLAG_PER_ANTENNA;
			}
			eirp = (flags & CLM_DATA_FLAG_MEAS_MASK) == CLM_DATA_FLAG_MEAS_EIRP;
			bw_idx = bw_width_to_idx[flags & CLM_DATA_FLAG_WIDTH_MASK];
			for (num_rec = (int)*tx_rec++; num_rec--; tx_rec += tx_rec_len) {
				const unsigned char *rates =
					get_byte_string(
					base_ht_loc_data[base_ht_idx].rate_sets_bw[bw_idx],
					tx_rec[CLM_LOC_DSC_RATE_IDX]);
				int num_rates = *rates++;
				/* Check for a non-disabled power before clearing NO_bw flag */
				if ((unsigned char)CLM_DISABLED_POWER ==
					(unsigned char)tx_rec[CLM_LOC_DSC_POWER_IDX]) {
					continue;
				}
				if (bw_flag_mask) {
					*ret_flags &= ~bw_flag_mask;
					bw_flag_mask = 0; /* clearing once should be enough */
				}
				while (num_rates--) {
					unsigned char rate_idx = *rates++;
					switch (RATE_TYPE(rate_idx)) {
					case RT_DSSS:
						if (eirp) {
							*ret_flags |= CLM_FLAG_HAS_DSSS_EIRP;
						}
						break;
					case RT_OFDM:
						if (eirp) {
							*ret_flags |= CLM_FLAG_HAS_OFDM_EIRP;
						}
						break;
					case RT_MCS:
						*ret_flags &= ~CLM_FLAG_NO_MIMO;
						break;
					}
				}
			}
		} while (flags & CLM_DATA_FLAG_MORE);
	}
	if (locales->country_flags & CLM_DATA_FLAG_REG_TXBF) {
		*ret_flags |= CLM_FLAG_TXBF;
	}
	if (locales->country_flags & CLM_DATA_FLAG_REG_DEF_FOR_CC) {
		*ret_flags |= CLM_FLAG_DEFAULT_FOR_CC;
	}
	return CLM_RESULT_OK;
}

clm_result_t clm_country_advertised_cc(const clm_country_t country, ccode_t advertised_cc)
{
	data_source_id_t ds_id;
	int idx, ds_idx;
	const clm_country_rev_definition_set_t *country_set;
	const clm_cc_rev_t *cc_rev;

	if (!advertised_cc) {
		return CLM_RESULT_ERR;
	}
	iter_unpack(country, &ds_id, &idx);
	country_set =
		(const clm_country_rev_definition_set_t *)GET_DATA(ds_id, countries);
	if (!country_set || ((unsigned)idx >= (unsigned)country_set->num)) {
		return CLM_RESULT_ERR;
	}
	cc_rev = &get_country_def_by_idx(ds_id, idx)->cc_rev;
	for (ds_idx = 0; ds_idx < DS_NUM; ++ds_idx) {
		const clm_advertised_cc_set_t *adv_cc_set =
			(const clm_advertised_cc_set_t *)GET_DATA(ds_idx, advertised_ccs);
		const clm_advertised_cc_t *adv_ccs =
			adv_cc_set
				? (const clm_advertised_cc_t *)relocate_ptr(ds_idx, adv_cc_set->set)
				: NULL;
		int adv_cc_idx;
		if (!adv_ccs) {
			continue;
		}
		for (adv_cc_idx = 0; adv_cc_idx < adv_cc_set->num; ++adv_cc_idx) {
			int num_aliases = adv_ccs[adv_cc_idx].num_aliases;
			int alias_idx;
			const clm_cc_rev_t *alias =	(const clm_cc_rev_t *)relocate_ptr(ds_idx,
				adv_ccs[adv_cc_idx].aliases);
			if (num_aliases == CLM_DELETED_NUM) {
				continue;
			}
			for (alias_idx = 0; alias_idx < num_aliases; ++alias_idx) {
				if (cc_rev_equal(alias + alias_idx, cc_rev)) {
					copy_cc(advertised_cc, adv_ccs[adv_cc_idx].cc);
					return CLM_RESULT_OK;
				}
			}
		}
	}
	copy_cc(advertised_cc, cc_rev->cc);
	return CLM_RESULT_OK;
}

#if !defined(WLC_CLMAPI_PRE7) || defined(BCMROMBUILD)

/** Precomputes country (region) related data
 * \param[in] locales Region locales
 * \param[out] loc_data Country-related data
 */
static void get_country_data(const clm_country_locales_t *locales, country_data_t *country_data)
{
	data_source_id_t ds_id =
		(data_source_id_t)(locales->computed_flags & COUNTRY_FLAGS_DS_MASK);
	const data_dsc_t *ds = get_data_sources(ds_id);
	my_memset(country_data, 0, sizeof(*country_data));
	country_data->chan_ranges_bw = ds->chan_ranges_bw;
	if (locales->country_flags & CLM_DATA_FLAG_REG_SC_RULES_MASK) {
		int rules_idx = (locales->country_flags & CLM_DATA_FLAG_REG_SC_RULES_MASK) -
			CLM_SUB_CHAN_IDX_BASE;
		const clm_sub_chan_rules_set_80_t *sub_chan_rules_set_80 =
			(const clm_sub_chan_rules_set_80_t *)GET_DATA(ds_id, sub_chan_rules_80);
		if (sub_chan_rules_set_80 && ((unsigned)rules_idx <
				(unsigned)sub_chan_rules_set_80->num)) {
			const clm_sub_chan_region_rules_80_t *sub_chan_region_rules =
				(const clm_sub_chan_region_rules_80_t*)relocate_ptr(ds_id,
				sub_chan_rules_set_80->region_rules) + rules_idx;
			country_data->sub_chan_channel_rules_80.channel_rules =
				(const clm_sub_chan_channel_rules_80_t *)relocate_ptr(ds_id,
				sub_chan_region_rules->channel_rules);
			country_data->sub_chan_channel_rules_80.num = sub_chan_region_rules->num;
		}
		if (ds->has_160mhz) {
			const clm_sub_chan_rules_set_160_t *sub_chan_rules_set_160 =
				(const clm_sub_chan_rules_set_160_t *)GET_DATA(ds_id,
				sub_chan_rules_160);
			if (sub_chan_rules_set_160 && ((unsigned)rules_idx <
					(unsigned)sub_chan_rules_set_160->num)) {
				const clm_sub_chan_region_rules_160_t *sub_chan_region_rules =
					(const clm_sub_chan_region_rules_160_t*)relocate_ptr(ds_id,
					sub_chan_rules_set_160->region_rules) + rules_idx;
				country_data->sub_chan_channel_rules_160.channel_rules =
					(const clm_sub_chan_channel_rules_160_t*)relocate_ptr(ds_id,
					sub_chan_region_rules->channel_rules);
				country_data->sub_chan_channel_rules_160.num =
					sub_chan_region_rules->num;
			}
		}
	}
}

/** Fills subchannel table that maps bandwidth to subchannel numbers
 * \param[out] subchannels Subchannel table being filled
 * \param[in] channel Main channel number
 * \param[in] bw Main channel bandwidth (actual type is clm_bandwidth_t)
 * \param[in] limits_type Limits type (subchannel ID)
 * \return TRUE if bandwidth/limits type combination is valid
 */
static MY_BOOL fill_full_subchan_table(unsigned char subchannels[CLM_BW_NUM], int channel,
	int bw, clm_limits_type_t limits_type)
{
	/* Descriptor of path to given subchannel */
	unsigned char path = subchan_paths[limits_type];

	/* Mask that corresponds to current step in path */
	unsigned char path_mask =
		1 << ((path & SUB_CHAN_PATH_COUNT_MASK) >> SUB_CHAN_PATH_COUNT_OFFSET);

	/* Channel number stride for current bandwidth */
	int stride = (CHAN_STRIDE << bw) / 2;

	/* Emptying the map */
	my_memset(subchannels, 0, sizeof(unsigned char) * CLM_BW_NUM);
	for (;;) {
		/* Setting channel number for current bandwidth */
		subchannels[bw--] = (unsigned char)channel;

		/* Rest will related to previous (halved) bandwidth, i.e. to subchannel */

		/* Is path over? */
		if ((path_mask >>= 1) == 0) {
			return TRUE; /* Yes - success */
		}
		/* Path not over but we passed through minimum bandwidth? */
		if (bw < 0) {
			return FALSE; /* Yes, failure */
		}

		/* Halving channel stride */
		stride >>= 1;

		/* Selecting subchannel number according to path */
		if (path & path_mask) {
			channel += stride;
		} else {
			channel -= stride;
		}
	}
}

/* Preliminary observations.
Every power in *limits is a minimum over values from several sources:
- For main (full) channel - over per-channel-range limits from limits that contain
	this channel. There can be legitimately more than one such limit: e.g. one EIRP
	and another Conducted (this was legal up to certain moment, not legal now but
	may become legal again in future)
- For subchannels it might be minimum over several enclosing channels (e.g. for
	20-in-80 it may be minimum over corresponding 20MHz main (full) channel and
	40MHz enclosing main (full) channel). Notice that all enclosing channels have
	different channel numbers (e.g. for 36LL it is 40MHz enclosing channel 38 and
	80MHz enclosing channel 42)
- 2.4GHz 20-in-40 channels also take power targets for DSSS rates from 20MHz
	channel data (even though other limits are taken from enclosing 40MHz channel)

So in general resulting limit is a minimum of up to 3 channels (one per
bandwidth) and these channels have different numbers!
'bw_to_chan' vector contains mapping from bandwidths to channel numbers.
Bandwidths not used in limit computation are mapped to 0.
20-in-40 DSSS case is served by 'channel_dsss' variable, that when nonzero
contains number of channel where from DSSS limits shall be taken.

'chan_offsets' mapping is initially computed to to derive all these channel
numbers from main channel number, main channel bandwidth and power limit type
(i.e. subchannel ID)  is computed. Also computation of chan_offsets is used
to determine if bandwidth/limits_type pair is valid.
*/
extern clm_result_t clm_limits(const clm_country_locales_t *locales, clm_band_t band,
	unsigned int channel, int ant_gain, clm_limits_type_t limits_type,
	const clm_limits_params_t *params, clm_power_limits_t *limits)
{
	/* Locale characteristics for base and HT locales of given band */
	locale_data_t base_ht_loc_data[2];

	/* Loop variable. Points first to base than to HT locale */
	int base_ht_idx;

	/* Channel for DSSS rates of 20-in-40 power targets (0 if irrelevant) */
	int channel_dsss = 0;

	/* Become true if at least one power target was found */
	MY_BOOL valid_channel = FALSE;

	/* Maps bandwidths to subchannel numbers.
	 * Leaves zeroes for subchannels of given limits_type
	 */
	unsigned char subchannels[CLM_BW_NUM];

	/* Country (region) precomputed data */
	country_data_t country_data;

	/* Simple validity check */
	if (!locales || !limits || ((unsigned)band >= (unsigned)CLM_BAND_NUM) || !params ||
		((unsigned)params->bw >= (unsigned)CLM_BW_NUM) ||
		((unsigned)limits_type >= CLM_LIMITS_TYPE_NUM) ||
		((unsigned)params->antenna_idx >= WL_TX_CHAINS_MAX))
	{
		return CLM_RESULT_ERR;
	}

	/* Fills bandwidth->channel number map */
	if (!fill_full_subchan_table(subchannels, channel, params->bw, limits_type)) {
		/** bandwidth/limits_type pair is invalid */
		return CLM_RESULT_ERR;
	}
	if ((band == CLM_BAND_2G) && (params->bw != CLM_BW_20) && subchannels[CLM_BW_20]) {
		/* 20-in-something, 2.4GHz. Channel to take DSSS limits from */
		channel_dsss = subchannels[CLM_BW_20];
	}
	my_memset(limits, (unsigned char)UNSPECIFIED_POWER, sizeof(limits->limit));

	/* Computing helper information on base locale for given bandwidth */
	if (!get_loc_def(locales, (band == CLM_BAND_2G) ? CLM_LOC_IDX_BASE_2G : CLM_LOC_IDX_BASE_5G,
		&(base_ht_loc_data[0])))
	{
		return CLM_RESULT_ERR;
	}
	/* Advance local definition pointer from regulatory info to power targets info */
	if (base_ht_loc_data[0].def_ptr) {
		base_ht_loc_data[0].def_ptr += CLM_LOC_DSC_BASE_HDR_LEN;
		base_ht_loc_data[0].def_ptr +=
			1 + CLM_LOC_DSC_PUB_REC_LEN * (int)*(base_ht_loc_data[0].def_ptr);
	}
	/* Helper information for base locale computed */

	/* Computing helper information for HT locale */
	if (!get_loc_def(locales, (band == CLM_BAND_2G) ? CLM_LOC_IDX_HT_2G : CLM_LOC_IDX_HT_5G,
		&(base_ht_loc_data[1])))
	{
		return CLM_RESULT_ERR;
	}
	/* Helper information for HT locale computed */

	/* Obtaining precomputed country data */
	get_country_data(locales, &country_data);

	/* For base then HT locales do */
	for (base_ht_idx = 0; base_ht_idx < (int)ARRAYSIZE(base_ht_loc_data); ++base_ht_idx) {
		/* Precomputed helper data for current locale */
		const locale_data_t *loc_data = base_ht_loc_data + base_ht_idx;

		/* Channel combs for given band - vector indexed by channel bandwidth */
		const clm_channel_comb_set_t *const* comb_sets = loc_data->combs;

		/* Transmission power records' sequence for current locale */
		const unsigned char *tx_rec = loc_data->def_ptr;

		/* CLM_DATA_FLAG_ flags for current subsequence of transmission power
		   records' sequence
		 */
		unsigned char flags;

		/* Same as subchannels, but only has nonzeroes for bandwidths, used by current
		 * subchannel rule
		 */
		unsigned char bw_to_chan[CLM_BW_NUM];

		/* No transmission power records - nothing to do for this locale */
		if (!tx_rec) {
			continue;
		}

		/* Now computing 'bw_to_chan' - bandwidth to channel map that determines which
		   channels will be used for limit computation
		 */
		/* Preset to 'no bandwidths' */
		my_memset(bw_to_chan, 0, sizeof(bw_to_chan));
		if (limits_type == CLM_LIMITS_TYPE_CHANNEL) {
			/* Main channel case - bandwidth specified as parameter, channel
			   specified as parameter
			 */
			bw_to_chan[params->bw] = (unsigned char)channel;
		} else if (params->bw == CLM_BW_40) {
			/* 20-in-40 - use predefined 'limit from 40MHz' rule */
			bw_to_chan[CLM_BW_40] = (unsigned char)channel;
#ifdef WL11AC
		} else if (params->bw == CLM_BW_80) {
			fill_actual_subchan_table(bw_to_chan, subchannels, limits_type, channel,
				country_data.chan_ranges_bw[params->bw], comb_sets[CLM_BW_80],
				country_data.sub_chan_channel_rules_80.channel_rules,
				country_data.sub_chan_channel_rules_80.num,
				sizeof(clm_sub_chan_channel_rules_80_t));
		} else if (params->bw == CLM_BW_160) {
			fill_actual_subchan_table(bw_to_chan, subchannels, limits_type, channel,
				country_data.chan_ranges_bw[params->bw], comb_sets[CLM_BW_160],
				country_data.sub_chan_channel_rules_160.channel_rules,
				country_data.sub_chan_channel_rules_160.num,
				sizeof(clm_sub_chan_channel_rules_160_t));
#endif /* WL11AC */
		}
		/* bw_to_chan computed */

		/* Loop over all transmission power subsequences */
		do {
			/* Number of records in subsequence */
			int num_rec;
			/* Bandwidth of records in subsequence */
			clm_bandwidth_t pg_bw;
			/* Channel combs for bandwidth used in subsequence */
			const clm_channel_comb_set_t *comb_set_for_bw;
			/* Channel number to look for bandwidth used in this subsequence */
			int channel_for_bw;
			/* Length of TX power records in current subsequence */
			int tx_rec_len;
			/* Index of TX power inside TX power record */
			int tx_power_idx;
			/* Base address for channel ranges for bandwidth used in this subsequence */
			const clm_channel_range_t *ranges;
			/* Sequence of rate sets' definition for bw used in this subsequence */
			const unsigned char *rate_sets;

			flags = *tx_rec++;
			pg_bw = (clm_bandwidth_t)bw_width_to_idx[flags & CLM_DATA_FLAG_WIDTH_MASK];
			comb_set_for_bw = comb_sets[pg_bw];
			channel_for_bw = bw_to_chan[pg_bw];
			tx_rec_len = CLM_LOC_DSC_TX_REC_LEN + ((flags &
				CLM_DATA_FLAG_PER_ANT_MASK) >> CLM_DATA_FLAG_PER_ANT_SHIFT);
			tx_power_idx = (tx_rec_len == CLM_LOC_DSC_TX_REC_LEN)
				? CLM_LOC_DSC_POWER_IDX
				: antenna_power_offsets[params->antenna_idx];
			ranges = loc_data->chan_ranges_bw[pg_bw];
			rate_sets = loc_data->rate_sets_bw[pg_bw];
			/* Loop over all records in subsequence */
			for (num_rec = (int)*tx_rec++; num_rec--; tx_rec += tx_rec_len)
			{
				/* Channel range for current transmission power record */
				const clm_channel_range_t *range = ranges +
					tx_rec[CLM_LOC_DSC_RANGE_IDX];
				/* Power target for current transmission power record */
				char qdbm;

				/* Per-antenna record without a limit for given antenna index? */
				if (tx_power_idx >= tx_rec_len) {
					/* At least check if chan is valid - return OK if it is */
					if (!valid_channel && channel_for_bw &&
						channel_in_range(channel_for_bw,
						range, comb_set_for_bw) &&
						((unsigned char)tx_rec[0] !=
						(unsigned char)CLM_DISABLED_POWER))
					{
						valid_channel = TRUE;
					}
					/* Skip the rest - no limit for given antenna index */
					continue;
				}
				qdbm = (char)tx_rec[tx_power_idx];
				if ((unsigned char)qdbm != (unsigned char)CLM_DISABLED_POWER) {
					if ((flags & CLM_DATA_FLAG_MEAS_MASK) ==
						CLM_DATA_FLAG_MEAS_EIRP) {
						qdbm -= (char)ant_gain;
					}
					/* Apply SAR limit */
					qdbm = (char)((qdbm > params->sar) ? params->sar : qdbm);
				}

				/* If this record related to channel for this bandwidth? */
				if (channel_for_bw &&
					channel_in_range(channel_for_bw, range, comb_set_for_bw))
				{
					/* Rate indices  for current records' rate set */
					const unsigned char *rates = get_byte_string(rate_sets,
						tx_rec[CLM_LOC_DSC_RATE_IDX]);
					int num_rates = *rates++;
					/* Loop over this tx power record's rate indices */
					while (num_rates--) {
						unsigned char rate_idx = *rates++;
						clm_power_t *pp = &limits->limit[rate_idx];
						/* Looking for minimum power */
						if ((!channel_dsss ||
							(RATE_TYPE(rate_idx) != RT_DSSS)) &&
							((*pp == (clm_power_t)
							(unsigned char)UNSPECIFIED_POWER) ||
							((*pp > qdbm) &&
							(*pp != (clm_power_t)
							(unsigned char)CLM_DISABLED_POWER))))
						{
							*pp = qdbm;
						}
					}
					if ((unsigned char)qdbm !=
						(unsigned char)CLM_DISABLED_POWER) {
						valid_channel = TRUE;
					}
				}
				/* If this rule related to 20-in-something DSSS channel? */
				if (channel_dsss && (pg_bw == CLM_BW_20) &&
				   channel_in_range(channel_dsss, range, comb_sets[CLM_BW_20]))
				{
					/* Same as above */
					const unsigned char *rates = get_byte_string(rate_sets,
						tx_rec[CLM_LOC_DSC_RATE_IDX]);
					int num_rates = *rates++;
					while (num_rates--) {
						unsigned char rate_idx = *rates++;
						clm_power_t *pp = &limits->limit[rate_idx];
						if ((RATE_TYPE(rate_idx) == RT_DSSS) &&
							((*pp == (clm_power_t)
							(unsigned char)UNSPECIFIED_POWER) ||
							((*pp > qdbm) &&
							(*pp !=	(clm_power_t)
							(unsigned char)CLM_DISABLED_POWER))))
						{
							*pp = qdbm;
						}
					}
				}
			}
		} while (flags & CLM_DATA_FLAG_MORE);
	}
	if (valid_channel) {
		/* Converting CLM_DISABLED_POWER and UNSPECIFIED_POWER to WL_RATE_DISABLED */
		clm_power_t *pp = limits->limit, *end = pp + ARRAYSIZE(limits->limit);
		do {
			if ((*pp == (clm_power_t)(unsigned char)CLM_DISABLED_POWER) ||
				(*pp == (clm_power_t)(unsigned char)UNSPECIFIED_POWER))
			{
				*pp = WL_RATE_DISABLED;
			}
		} while (++pp < end);
	}
	return valid_channel ? CLM_RESULT_OK : CLM_RESULT_NOT_FOUND;
}


/** Retrieves information about channels with valid power limits for locales of some region
 * \param[in] locales Country (region) locales' information
 * \param[out] valid_channels Valid 5GHz channels
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if `locales` is null or contains
	invalid information, CLM_RESULT_NOT_FOUND if channel has invalid value
 */
extern clm_result_t clm_valid_channels_5g(const clm_country_locales_t *locales,
	clm_channels_t *channels20, clm_channels_t *channels4080)
{
	/* Locale characteristics for base and HT locales of given band */
	locale_data_t base_ht_loc_data[2];
	/* Loop variable */
	int base_ht_idx;

	/* Check pointers' validity */
	if (!locales || !channels20 || !channels4080) {
		return CLM_RESULT_ERR;
	}
	/* Clearing output parameters */
	my_memset(channels20, 0, sizeof(*channels20));
	my_memset(channels4080, 0, sizeof(*channels4080));

	/* Computing helper information on base locale for given bandwidth */
	if (!get_loc_def(locales, CLM_LOC_IDX_BASE_5G, &(base_ht_loc_data[0])))
	{
		return CLM_RESULT_ERR;
	}
	/* Advance local definition pointer from regulatory info to power targets info */
	if (base_ht_loc_data[0].def_ptr) {
		base_ht_loc_data[0].def_ptr += CLM_LOC_DSC_BASE_HDR_LEN;
		base_ht_loc_data[0].def_ptr +=
			1 + CLM_LOC_DSC_PUB_REC_LEN * (int)*(base_ht_loc_data[0].def_ptr);
	}
	/* Helper information for base locale computed */

	/* Computing helper information for HT locale */
	if (!get_loc_def(locales, CLM_LOC_IDX_HT_5G, &(base_ht_loc_data[1])))
	{
		return CLM_RESULT_ERR;
	}
	/* Helper information for HT locale computed */

	/* For base then HT locales do */
	for (base_ht_idx = 0; base_ht_idx < (int)ARRAYSIZE(base_ht_loc_data); ++base_ht_idx) {
		/* Precomputed helper data for current locale */
		const locale_data_t *loc_data = base_ht_loc_data + base_ht_idx;

		/* Channel combs for given band - vector indexed by channel bandwidth */
		const clm_channel_comb_set_t *const* comb_sets = loc_data->combs;

		/* Transmission power records' sequence for current locale */
		const unsigned char *tx_rec = loc_data->def_ptr;

		/* CLM_DATA_FLAG_ flags for current subsequence of transmission power
		   records' sequence
		 */
		unsigned char flags;

		/* No transmission power records - nothing to do for this locale */
		if (!tx_rec) {
			continue;
		}

		/* Loop over all transmission power subsequences */
		do {
			/* Number of records in subsequence */
			int num_rec;
			/* Bandwidth of records in subsequence */
			clm_bandwidth_t pg_bw;
			/* Channel combs for bandwidth used in subsequence */
			const clm_channel_comb_set_t *comb_set_for_bw;
			/* Length of TX power records in current subsequence */
			int tx_rec_len;
			/* Vector of channel ranges' definition */
			const clm_channel_range_t *ranges;
			clm_channels_t *channels;

			flags = *tx_rec++;
			pg_bw = (clm_bandwidth_t)bw_width_to_idx[flags & CLM_DATA_FLAG_WIDTH_MASK];
			if (pg_bw == CLM_BW_20)
				channels = channels20;
			else
				channels = channels4080;

			ranges = loc_data->chan_ranges_bw[pg_bw];

			tx_rec_len = CLM_LOC_DSC_TX_REC_LEN + ((flags &
				CLM_DATA_FLAG_PER_ANT_MASK) >> CLM_DATA_FLAG_PER_ANT_SHIFT);
			/* Loop over all records in subsequence */

			comb_set_for_bw = comb_sets[pg_bw];

			for (num_rec = (int)*tx_rec++; num_rec--; tx_rec += tx_rec_len)
			{

				/* Channel range for current transmission power record */
				const clm_channel_range_t *range = ranges +
					tx_rec[CLM_LOC_DSC_RANGE_IDX];

				int num_combs;
				const clm_channel_comb_set_t *combs = loc_data->combs[pg_bw];
				const clm_channel_comb_t *comb = combs->set;

				/* Check for a non-disabled power before clearing NO_bw flag */
				if ((unsigned char)CLM_DISABLED_POWER ==
					(unsigned char)tx_rec[CLM_LOC_DSC_POWER_IDX]) {
					continue;
				}

				for (num_combs = comb_set_for_bw->num; num_combs--; ++comb) {
					int chan;
					for (chan = comb->first_channel; chan <= comb->last_channel;
						chan += comb->stride) {
						if (chan && channel_in_range(chan, range,
							comb_set_for_bw)) {
							channels->bitvec[chan / 8] |=
								(unsigned char)(1 << (chan % 8));
						}
					}
				}
			}
		} while (flags & CLM_DATA_FLAG_MORE);
	}
	return CLM_RESULT_OK;
}

#endif /* !WLC_CLMAPI_PRE7 || BCMROMBUILD */

clm_result_t clm_regulatory_limit(const clm_country_locales_t *locales, clm_band_t band,
	unsigned int channel, int *limit)
{
	int num_rec;
	locale_data_t loc_data;
	const unsigned char *pub_rec;
	const clm_channel_range_t *ranges;
	const clm_channel_comb_set_t *comb_set;
	if (!locales || ((unsigned)band >= (unsigned)CLM_BAND_NUM) || !limit) {
		return CLM_RESULT_ERR;
	}
	if (!get_loc_def(locales, (band == CLM_BAND_2G) ? CLM_LOC_IDX_BASE_2G : CLM_LOC_IDX_BASE_5G,
		&loc_data))
	{
		return CLM_RESULT_ERR;
	}
	if ((pub_rec = loc_data.def_ptr) == NULL) {
		return CLM_RESULT_ERR;
	}
	pub_rec += CLM_LOC_DSC_BASE_HDR_LEN;
	ranges = loc_data.chan_ranges_bw[CLM_BW_20];
	comb_set = loc_data.combs[CLM_BW_20];
	for (num_rec = *pub_rec++; num_rec--; pub_rec += CLM_LOC_DSC_PUB_REC_LEN) {
		if (channel_in_range(channel, ranges + pub_rec[CLM_LOC_DSC_RANGE_IDX], comb_set)) {
			*limit = (int)pub_rec[CLM_LOC_DSC_POWER_IDX];
			return CLM_RESULT_OK;
		}
	}
	return CLM_RESULT_NOT_FOUND;
}

clm_result_t clm_agg_country_iter(clm_agg_country_t *agg, ccode_t cc, unsigned int *rev)
{
	data_source_id_t ds_id;
	int idx;
	clm_result_t ret = CLM_RESULT_OK;
	if (!agg || !cc || !rev) {
		return CLM_RESULT_ERR;
	}
	if (*agg == CLM_ITER_NULL) {
		ds_id = DS_INC;
		idx = 0;
	} else {
		iter_unpack(*agg, &ds_id, &idx);
		++idx;
	}
	for (;;) {
		const clm_aggregate_cc_set_t *aggregate_set =
			(const clm_aggregate_cc_set_t *)GET_DATA(ds_id, aggregates);
		const clm_aggregate_cc_t *aggregate_definition;
		if (!aggregate_set || (idx >= aggregate_set->num)) {
			if (ds_id == DS_INC) {
				ds_id = DS_MAIN;
				idx = 0;
				continue;
			} else {
				ret = CLM_RESULT_NOT_FOUND;
				idx = aggregate_set ? aggregate_set->num : 0;
				break;
			}
		}
		aggregate_definition =
			(const clm_aggregate_cc_t *)relocate_ptr(ds_id, aggregate_set->set) + idx;
		if (aggregate_definition->num_regions == CLM_DELETED_NUM) {
			++idx;
			continue;
		}
		if ((ds_id == DS_MAIN) && get_aggregate(DS_INC,
			&aggregate_definition->def_reg, NULL)) {
			++idx;
			continue;
		}
		copy_cc(cc, aggregate_definition->def_reg.cc);
		*rev = aggregate_definition->def_reg.rev;
		break;
	}
	iter_pack(agg, ds_id, idx);
	return ret;
}

clm_result_t clm_agg_map_iter(const clm_agg_country_t agg, clm_agg_map_t *map, ccode_t cc,
	unsigned int *rev)
{
	data_source_id_t ds_id, mapping_ds_id;
	int agg_idx, mapping_idx;
	const clm_aggregate_cc_set_t *aggregate_set;
	const clm_aggregate_cc_t *aggregate;

	if (!map || !cc || !rev) {
		return CLM_RESULT_ERR;
	}
	iter_unpack(agg, &ds_id, &agg_idx);
	aggregate_set = (const clm_aggregate_cc_set_t *)GET_DATA(ds_id, aggregates);
	aggregate = (const clm_aggregate_cc_t *)relocate_ptr(ds_id, aggregate_set->set) +
		agg_idx;
	if (*map == CLM_ITER_NULL) {
		mapping_idx = 0;
		mapping_ds_id = ds_id;
	} else {
		iter_unpack(*map, &mapping_ds_id, &mapping_idx);
		++mapping_idx;
	}
	for (;;) {
		const clm_aggregate_cc_t *cur_agg =
			(mapping_ds_id == ds_id)
				? aggregate
				: get_aggregate(mapping_ds_id, &aggregate->def_reg, NULL);
		const clm_cc_rev_t *mapping = cur_agg ?
			((const clm_cc_rev_t *)relocate_ptr(mapping_ds_id, cur_agg->regions) +
			mapping_idx)
			: NULL;

		if (!mapping || (mapping_idx >= cur_agg->num_regions)) {
			if (mapping_ds_id == DS_MAIN) {
				iter_pack(map, mapping_ds_id, mapping_idx);
				return CLM_RESULT_NOT_FOUND;
			}
			mapping_ds_id = DS_MAIN;
			mapping_idx = 0;
			continue;
		}
		if (mapping->rev == CLM_DELETED_MAPPING) {
			++mapping_idx;
			continue;
		}
		if ((ds_id == DS_INC) && (mapping_ds_id == DS_MAIN) &&
			get_mapping(DS_INC, aggregate, mapping->cc)) {
			++mapping_idx;
			continue;
		}
		copy_cc(cc, mapping->cc);
		*rev = mapping->rev;
		break;
	}
	iter_pack(map, mapping_ds_id, mapping_idx);
	return CLM_RESULT_OK;
}

clm_result_t clm_agg_country_lookup(const ccode_t cc, unsigned int rev,
	clm_agg_country_t *agg)
{
	int ds_idx;
	clm_cc_rev_t cc_rev;
	if (!cc || !agg) {
		return CLM_RESULT_ERR;
	}
	translate_cc(&cc);
	copy_cc(cc_rev.cc, cc);
	cc_rev.rev = (char)rev;
	for (ds_idx = 0; ds_idx < DS_NUM; ds_idx++) {
		int agg_idx;
		const clm_aggregate_cc_t *agg_def =
			get_aggregate((data_source_id_t)ds_idx, &cc_rev, &agg_idx);
		if (agg_def) {
			if (agg_def->num_regions == CLM_DELETED_NUM) {
				return CLM_RESULT_NOT_FOUND;
			}
			iter_pack(agg, (data_source_id_t)ds_idx, agg_idx);
			return CLM_RESULT_OK;
		}
	}
	return CLM_RESULT_NOT_FOUND;
}

clm_result_t clm_agg_country_map_lookup(const clm_agg_country_t agg, const ccode_t target_cc,
	unsigned int *rev)
{
	data_source_id_t ds_id;
	int aggregate_idx;
	const clm_aggregate_cc_set_t *aggregate_set;
	const clm_aggregate_cc_t *aggregate;
	const clm_cc_rev_t *mapping;

	if (!target_cc || !rev) {
		return CLM_RESULT_ERR;
	}
	translate_cc(&target_cc);
	iter_unpack(agg, &ds_id, &aggregate_idx);
	aggregate_set = (const clm_aggregate_cc_set_t *)GET_DATA(ds_id, aggregates);
	aggregate = (const clm_aggregate_cc_t *)relocate_ptr(ds_id, aggregate_set->set) +
		aggregate_idx;
	for (;;) {
		mapping = get_mapping(ds_id, aggregate, target_cc);
		if (mapping) {
			if (mapping->rev == CLM_DELETED_MAPPING) {
				return CLM_RESULT_NOT_FOUND;
			}
			*rev = mapping->rev;
			return CLM_RESULT_OK;
		}
		if (ds_id == DS_MAIN) {
			return CLM_RESULT_NOT_FOUND;
		}
		ds_id = DS_MAIN;
		aggregate = get_aggregate(DS_MAIN, &aggregate->def_reg, NULL);
		if (!aggregate) {
			return CLM_RESULT_NOT_FOUND;
		}
	}
}


static const char* clm_get_app_version_string(data_source_id_t ds_id)
{
	const char* strptr = NULL;
	data_dsc_t *ds = get_data_sources(ds_id);
	const clm_registry_t *data = ds->data;
	const int flags = data->flags;

	if (flags & CLM_REGISTRY_FLAG_APPS_VERSION) {
		const clm_data_header_t *header = ds->header;

		if (my_strcmp(header->apps_version, CLM_APPS_VERSION_NONE_TAG)) {
			strptr = header->apps_version;
		}
	}
	return strptr;
}


const char* clm_get_base_app_version_string(void)
{
	return clm_get_app_version_string(DS_MAIN);
}


const char* clm_get_inc_app_version_string(void)
{
	return clm_get_app_version_string(DS_INC);
}
