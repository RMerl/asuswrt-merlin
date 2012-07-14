/*
 * API for accessing CLM data
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_clm.h 4645 2012-01-16 22:47:12Z $
 */

#ifndef _WLC_CLM_H_
#define _WLC_CLM_H_

#include <bcmwifi_rates.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/****************
* API CONSTANTS	*
*****************
*/

/** Module constants */
enum clm_const {
	/* Initial ('before begin') value for iterator. It is guaranteed that iterator
	 * 'pointing' at some valid object it is not equal to this value
	 */
	CLM_ITER_NULL	  = 0
};

/** Frequency band identifiers */
typedef enum clm_band {
	CLM_BAND_2G,	/* 2.4HGz band */
	CLM_BAND_5G,	/* 5GHz band */
	CLM_BAND_NUM	/* Number of band identifiers */
} clm_band_t;

/** Channel bandwidth identifiers */
typedef enum clm_bandwidth {
	CLM_BW_20,	/* 20MHz channel */
	CLM_BW_40,	/* 40MHz channel */
#ifdef WL11AC
	CLM_BW_80,	/* 80MHz channel */
#endif
	CLM_BW_NUM	/* Number of channel bandwidth identifiers */
} clm_bandwidth_t;

/** Return codes for API functions */
typedef enum clm_result {
	CLM_RESULT_OK		 = 0,	/* No error */
	CLM_RESULT_ERR		 = 1,	/* Invalid parameters */
	CLM_RESULT_NOT_FOUND = -1	/* Lookup failed (something was not found) */
} clm_result_t;

#if defined(WLC_CLMAPI_PRE7) && !defined(BCM4334A0SIM_4334B0) && !defined(BCMROMBUILD)
/** Which 20MHz channel to use as extension in 40MHz operation */
typedef enum clm_ext_chan {
	CLM_EXT_CHAN_LOWER = -1, /* Lower channel is extension, upper is control */
	CLM_EXT_CHAN_UPPER =  1, /* Upper channel is extension, lower is control */
	CLM_EXT_CHAN_NONE  =  0, /* Neither of the above (use for 20MHz operation) */
} clm_ext_chan_t;
#endif

/** Flags */
typedef enum clm_flags {
	/* DFS-related country (region) flags */
	CLM_FLAG_DFS_NONE		= 0x00000000, /* Common DFS rules */
	CLM_FLAG_DFS_EU			= 0x00000001, /* EU DFS rules */
	CLM_FLAG_DFS_US			= 0x00000002, /* US (FCC) DFS rules */
	CLM_FLAG_DFS_MASK		= 0x00000003, /* Mask of DFS-related flags */

	CLM_FLAG_FILTWAR1		= 0x00000004, /* FiltWAR1 flag from CLM XML */
	CLM_FLAG_NO_BF			= 0x00000008, /* No beamforming */

	/* DEBUGGING FLAGS (ALLOCATED AS TOP BITS) */
	CLM_FLAG_NO_80MHZ		= 0x80000000, /* No 80MHz channels */
	CLM_FLAG_NO_40MHZ		= 0x40000000, /* No 40MHz channels */
	CLM_FLAG_NO_MIMO		= 0x20000000, /* No MCS rates */
	CLM_FLAG_HAS_DSSS_EIRP	= 0x10000000, /* Has DSSS rates that use EIRP limits */
	CLM_FLAG_HAS_OFDM_EIRP	= 0x08000000, /* Has OFDM rates that use EIRP limits */
} clm_flags_t;

/** Type of limits to output in clm_limits() */
typedef enum clm_limits_type {
	CLM_LIMITS_TYPE_CHANNEL,  /* Limit for main channel */
	CLM_LIMITS_TYPE_SUBCHAN_L,	/* Limit for L-subchannel (20-in-40, 40-in-80) */
	CLM_LIMITS_TYPE_SUBCHAN_U,	/* Limit for U-subchannel (20-in-40, 40-in-80) */
#ifdef WL11AC
	CLM_LIMITS_TYPE_SUBCHAN_LL,	/* Limit for LL-subchannel (20-in-80) */
	CLM_LIMITS_TYPE_SUBCHAN_LU,	/* Limit for LU-subchannel (20-in-80) */
	CLM_LIMITS_TYPE_SUBCHAN_UL,	/* Limit for UL-subchannel (20-in-80) */
	CLM_LIMITS_TYPE_SUBCHAN_UU,	/* Limit for UU-subchannel (20-in-80) */
#endif
	CLM_LIMITS_TYPE_NUM
} clm_limits_type_t;


/*****************
* API DATA TYPES *
******************
*/

/** Country Code: a two byte code, usually ASCII
 * Note that 'worldwide' country code is now "ww", not "\0\0"
 */
typedef char ccode_t[2];

/** Channel set */
typedef struct clm_channels {
	unsigned char bitvec[25]; /* Bit vector, indexed by channel numbers */
} clm_channels_t;

/** Power in quarter of dBm units */
typedef signed char clm_power_t;

/** Per-TX-rate power limits */
typedef struct clm_power_limits {
	/** Per-rate limits (WL_RATE_DISABLED for disabled rates) */
	clm_power_t limit[WL_NUMRATES];
} clm_power_limits_t;

/* Iterators - tokens that represent various items in the CLM */
typedef int clm_country_t;		/* Country (region) definition */
typedef int clm_locale_t;		/* Locale definition */
typedef int clm_agg_country_t;	/* Aggregate definition */
typedef int clm_agg_map_t;		/* Definition of mapping inside aggregation */

/** Locales (transmission rules) for a country (region) */
typedef struct clm_country_locales {
	clm_locale_t locale_2G;			/* 2.4GHz base locale (802.11b/g SISO) */
	clm_locale_t locale_5G;			/* 5GHz base locale (802.11a SISO) */
	clm_locale_t locale_2G_HT;		/* 2.4GHz HT locale (802.11n and 802.11b/g MIMO) */
	clm_locale_t locale_5G_HT;		/* 5GHz HT locale (802.11n and 802.11a MIMO) */
	unsigned char country_flags;	/* Flags from country record */
} clm_country_locales_t;

/* forward declaration for CLM header data structure used in clm_init()
 * struct clm_data_header is defined in clm_data.h
 */
struct clm_data_header;


/***************
* API ROUTINES *
****************
*/

/** Provides main CLM data to the CLM access API
 * Must be called before any access APIs are used
 * \param[in] data Header of main CLM data. Only one main CLM data source may be set
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if given address is nonzero and
	CLM data tag is absent at given address or major number of CLM data format version is
	not supported by CLM API
 */
extern clm_result_t clm_init(const struct clm_data_header *data);

/** Provides incremental CLM data to the CLM access API
 * This call is optional
 * \param[in] data Header of incremental CLM data. No more than one incremental CLM data
	source may be set
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if given address is non zero and
	CLM data tag is absent at given address or major number of CLM data format version is
	not supported by CLM API
 */
extern clm_result_t clm_set_inc_data(const struct clm_data_header *data);

/** Initializes iterator before iteration via of clm_..._iter()
 * May be done manually - by assigning CLM_ITER_NULL
 * \param[out] iter Iterator to be initialized (type is clm_country_t, clm_locale_t,
	clm_agg_country_t, clm_agg_map_t)
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if null pointer passed
 */
extern clm_result_t clm_iter_init(int *iter);

/** Performs one iteration step over set of countries (regions)
 * Looks up first/next country (region)
 * \param[in,out] country Iterator token. Shall be initialized with clm_iter_init() before
	iteration begin. After successful call iterator token 'points' to same region as returned
	cc/rev
 * \param[out] cc Country (region) code
 * \param[out] rev Country (region) revision
 * \return CLM_RESULT_OK if first/next country was found, CLM_RESULT_ERR if any of passed
	pointers was null, CLM_RESULT_NOT_FOUND if first/next country was not found (iteration
	completed)
 */
extern clm_result_t clm_country_iter(clm_country_t *country, ccode_t cc, unsigned int *rev);


/** Looks up for country (region) with given country code and revision
 * \param[in] cc Country code to look for
 * \param[in] rev Country (region) revision to look for
 * \param[out] country Iterator that 'points' to found country
 * \return CLM_RESULT_OK if country was found, CLM_RESULT_ERR if any of passed pointers was
	null, CLM_RESULT_NOT_FOUND if required country was not found
 */
extern clm_result_t clm_country_lookup(const ccode_t cc, unsigned int rev,
	clm_country_t *country);

/** Retrieves locale definitions of given country (regions)
 * \param[in] country Iterator that 'points' to country (region) information
 * \param[out] locales Locales' information for given country
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if `locales` pointer is null,
	CLM_RESULT_NOT_FOUND if given country iterator does not point to valid country (region)
	definition
 */
extern clm_result_t clm_country_def(const clm_country_t country,
	clm_country_locales_t *locales);


/** Retrieves information about valid and restricted channels for locales of some region
 * \param[in] locales Country (region) locales' information
 * \param[in] band Frequency band
 * \param[out] valid_channels Valid 20MHz channels (optional parameter)
 * \param[out] restricted_channels Restricted channels (optional parameter)
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if `locales` pointer is null,
	or band ID is invalid or `locales` contents is invalid
 */
extern clm_result_t clm_country_channels(const clm_country_locales_t *locales,
	clm_band_t band, clm_channels_t *valid_channels, clm_channels_t *restricted_channels);


/** Retrieves flags associated with given country (region) for given band
 * \param[in] locales Country (region) locales' information
 * \param[in] band Frequency band
 * \param[out] flags Flags associated with given country (region) for given band
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if `locales` or `flags`pointer is
	null, or band ID is invalid or `locales` contents is invalid
 */
extern clm_result_t clm_country_flags(const clm_country_locales_t *locales, clm_band_t band,
	unsigned long *flags);


/** Retrieves advertised country code for country (region) pointed by given iterator
 * \param[in] country Iterator that points to country (region) in question
 * \param[out] advertised_cc Advertised CC for given region
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if `country` or `advertised_cc` is
	null, or if `country` not `points` t a valid country (region) definition
 */
extern clm_result_t clm_country_advertised_cc(const clm_country_t country, ccode_t advertised_cc);

#if defined(WLC_CLMAPI_PRE7) && !defined(BCM4334A0SIM_4334B0) && !defined(BCMROMBUILD)
/* This version required for ROM compatibility */
extern clm_result_t clm_limits(const clm_country_locales_t *locales, clm_band_t band,
	unsigned int channel, clm_bandwidth_t bw, int ant_gain, int sar,
	clm_ext_chan_t extension_channel, clm_power_limits_t *limits,
	clm_power_limits_t *bw20in40_limits);
#else
/** Retrieves the power limits on the given band/(sub)channel/bandwidth using the given
	antenna gain
 * \param[in] locales Country (region) locales' information
 * \param[in] band Frequency band
 * \param[in] channel Channel number (main channel if subchannel limits output is required)
 * \param[in] bw Channel bandwidth
 * \param[in] ant_gain Antenna gain in quarter dBm (used if limit is given in EIRP terms)
 * \param[in] sar SAR limit in quarter dBm
 * \param[in] limits_type Type of limits to output
 * \param[out] limits Limits for given above parameters
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if `locales` is null or contains
	invalid information, or if any other input parameter (except channel) has invalid value,
	CLM_RESULT_NOT_FOUND if channel has invalid value
 */
extern clm_result_t clm_limits(const clm_country_locales_t *locales, clm_band_t band,
	unsigned int channel, clm_bandwidth_t bw, int ant_gain, int sar,
	clm_limits_type_t limits_type, clm_power_limits_t *limits);
#endif /* WLC_CLMAPI_PRE7 && !BCM4334A0SIM_4334B0 && !BCMROMBUILD */


/** Retrieves maximum regulatory power for given channel
 * \param[in] locales Country (region) locales' information
 * \param[in] band Frequency band
 * \param[in] channel Channel number
 * \param[out] limit Regulatory power limit in dBm (!NOT qdBm!)
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if `locales` is null or contains
	invalid information, if `limit` is null or if any other input parameter (except channel)
	has invalid value, CLM_RESULT_NOT_FOUND if regulatory power limit not defined for given
	channel
 */
extern clm_result_t clm_regulatory_limit(const clm_country_locales_t *locales,
	clm_band_t band, unsigned int channel, int *limit);

/** Performs one iteration step over set of aggregations
 * Looks up first/next aggregation
 * \param[in,out] agg Iterator token. Shall be initialized with clm_iter_init() before
	iteration begin. After successful call iterator token 'points' to same aggregation as
	returned cc/rev
 * \param[out] cc Aggregation's default country (region) code
 * \param[out] rev Aggregation's default country (region) revision
 * \return CLM_RESULT_OK if first/next aggregation was found, CLM_RESULT_ERR if any of passed
	pointers was null, CLM_RESULT_NOT_FOUND if first/next aggregation was not found (iteration
	completed)
 */
extern clm_result_t clm_agg_country_iter(clm_agg_country_t *agg, ccode_t cc, unsigned int *rev);

/** Performs one iteration step over sequence of aggregation's mappings
 * Looks up first/next mapping
 * \param[in] agg Aggregation whose mappings are being iterated
 * \param[in,out] map Iterator token. Shall be initialized with clm_iter_init() before
	iteration begin. After successful call iterator token 'points' to same mapping as returned
	cc/rev
 * \param[out] cc Mapping's country code
 * \param[out] rev Mapping's region revision
 * \return CLM_RESULT_OK if first/next mapping was found, CLM_RESULT_ERR if any of passed
	pointers was null or if aggregation iterator does not 'point' to valid aggregation,
	CLM_RESULT_NOT_FOUND if first/next mapping was not found (iteration completed)
 */
extern clm_result_t clm_agg_map_iter(const clm_agg_country_t agg, clm_agg_map_t *map,
	ccode_t cc, unsigned int *rev);

/** Looks up for aggregation with given default country code and revision
 * \param[in] cc Default country code of aggregation being looked for
 * \param[in] rev Default region revision of aggregation being looked for
 * \param[out] agg Iterator that 'points' to found aggregation
 * \return CLM_RESULT_OK if aggregation was found, CLM_RESULT_ERR if any of passed pointers
	was null, CLM_RESULT_NOT_FOUND if required aggregation was not found
 */
extern clm_result_t clm_agg_country_lookup(const ccode_t cc, unsigned int rev,
	clm_agg_country_t *agg);

/** Looks up for mapping with given country code among mappings of given aggregation
 * \param[in] agg Aggregation whose mappings are being looked up
 * \param[in] target_cc Country code of mapping being looked up
 * \param[out] rev Country (region) revision of found mapping
 * \return CLM_RESULT_OK if mapping was found, CLM_RESULT_ERR if any of passed pointers
	was null or aggregation iterator does not 'point' to valid aggregation,
	CLM_RESULT_NOT_FOUND if required aggregation was not found
 */
extern clm_result_t clm_agg_country_map_lookup(const clm_agg_country_t agg,
	const ccode_t target_cc, unsigned int *rev);


/** Returns base data app version string
 * \return Pointer to version if it's present and not the vanilla string.
	NULL if version is not present or unchanged from default.
 */
extern const char* clm_get_base_app_version_string(void);


/** Returns incremental data app version string
 * \return Pointer to version if it's present and not the vanilla string.
	NULL if version is not present or unchanged from default.
 */
extern const char* clm_get_inc_app_version_string(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _WLC_CLM_H_ */
