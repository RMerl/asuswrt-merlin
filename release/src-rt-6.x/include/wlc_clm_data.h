/*
 * CLM Data structure definitions
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_clm_data.h 4652 2012-01-17 20:37:44Z $
 */

#ifndef _WLC_CLM_DATA_H_
#define _WLC_CLM_DATA_H_

#include <bcmwifi_rates.h>


#define CLMATTACHDATA(_data)	__attribute__ ((__section__ (".clmdataini2." #_data))) _data

extern const struct clm_data_header clm_header;
extern const struct clm_data_header clm_inc_header;

/*
***************************
* CLM DATA BLOB CONSTANTS *
***************************
*/

/* CLM header tag that designates the presence of CLM data (useful for incremental CLM data) */
#define CLM_HEADER_TAG "CLM DATA"

/* CLM apps version that indicates the data is vanilla Broadcom, direct from Regulatory */
#define CLM_APPS_VERSION_NONE_TAG "Broadcom-0.0"

/** Constants, related to CLM data BLOB */
enum clm_data_const {
	/* Special index values used in BLOB */

	CLM_RANGE_ALL_CHANNELS	   = 0xFF,	/* Channel range ID that designates all channels */
	CLM_RESTRICTED_SET_NONE	   = 0xFF,	/* Restricted set that covers all channels */

	/* Locale index that designates absence of locale. If all locales of region are absent
	   region considered to be deleted from incremental BLOB
	*/
	CLM_LOC_NONE			   = 0x3FF,

	/* Locale index that designates that region in incremental data uses the same locale as
	   correspondent base locale
	*/
	CLM_LOC_SAME			   = 0x3FE,

	/* Locale index marks region as deleted (used in incremental data) */
	CLM_LOC_DELETED			   = 0x3FD,

	/* Region revision that marks mapping, deleted in incremental aggregation */
	CLM_DELETED_MAPPING		   = 0xFFu,

	/* Value for 'num' field that marks item as deleted (used in incremental data for
	   clm_advertised_cc_set and clm_aggregate_cc_set structures)
	*/
	CLM_DELETED_NUM			   =   -1,

	/* Base for subchannel rules indices */
	CLM_SUB_CHAN_IDX_BASE	   = 1,

	/* Indices of locale types in vectors of per-type locale definitions */

	/* Index of 2.4GHz base locales in vectors of locale definitions */
	CLM_LOC_IDX_BASE_2G		   = 0,

	/* Index of 5GHz base locales in vectors of locale definitions */
	CLM_LOC_IDX_BASE_5G		   = 1,

	/* Index of 2.4GHz HT locales in vectors of locale definitions */
	CLM_LOC_IDX_HT_2G		   = 2,

	/* Index of 5GHz HT locales in vectors of locale definitions */
	CLM_LOC_IDX_HT_5G		   = 3,

	/* Number of locale type indices (length of locale definition vectors) */
	CLM_LOC_IDX_NUM			   = 4,

	/* Indices	of various data bytes in parts of locale definition */

	/* Index of locale valid channel index in locale definition header */
	CLM_LOC_DSC_CHANNELS_IDX   = 0,

	/* Index of locale flags in base locale definition header */
	CLM_LOC_DSC_FLAGS_IDX	   = 1,

	/* Index of restricted set index byte in base locale definition header */
	CLM_LOC_DSC_RESTRICTED_IDX = 2,

	/* Index of transmission (in quarter of dBm) or public power (in dBm)
	   in public or transmission record respectively
	*/
	CLM_LOC_DSC_POWER_IDX	   = 0,

	/* Index of channel range index in public or transmission record */
	CLM_LOC_DSC_RANGE_IDX	   = 1,

	/* Index of rates set index in transmission record */
	CLM_LOC_DSC_RATE_IDX	   = 2,

	/* Length of base locale definition header */
	CLM_LOC_DSC_BASE_HDR_LEN   = 3,

	/* Length of public power record */
	CLM_LOC_DSC_PUB_REC_LEN	   = 2,

	/* Length of transmission power records' header */
	CLM_LOC_DSC_TX_REC_HDR_LEN = 2,

	/* Length of transmission power record */
	CLM_LOC_DSC_TX_REC_LEN	   = 3,


	/* Flags used in registry */

	/* Country (region) record with 10 bit locale indices and flags are used */
	CLM_REGISTRY_FLAG_COUNTRY_10_FL	 = 0x00000001,

    /* BLOB header contains SW APPS version field */
    CLM_REGISTRY_FLAG_APPS_VERSION   = 0x00000002,

    /* BLOB contains subchannel rules */
    CLM_REGISTRY_FLAG_SUB_CHAN_RULES = 0x00000004
};

#define CLM_FORMAT_VERSION_MAJOR 7 /* Major version number of CLM data format */
#define CLM_FORMAT_VERSION_MINOR 2 /* Minor version number of CLM data format */

/** Flags and flag masks used in BLOB's byte fields */
enum clm_data_flags {

	/* Base locale flags */

	CLM_DATA_FLAG_DFS_NONE	 = 0x00,	/* General DFS rules */
	CLM_DATA_FLAG_DFS_EU	 = 0x01,	/* EU DFS rules */
	CLM_DATA_FLAG_DFS_US	 = 0x02,	/* US (FCC) DFS rules */
	CLM_DATA_FLAG_DFS_MASK	 = 0x03,	/* Mask of DFS field */

	CLM_DATA_FLAG_FILTWAR1	 = 0x04,	/* FiltWAR1 flag from CLM XML */

	/* Transmission power record flags */

	CLM_DATA_FLAG_WIDTH_20	 = 0x00,	/* 20MHz channel width */
	CLM_DATA_FLAG_WIDTH_40	 = 0x01,	/* 40MHz channel width */
	CLM_DATA_FLAG_WIDTH_80	 = 0x08,	/* 80MHz channel width */
	CLM_DATA_FLAG_WIDTH_MASK = 0x09,	/* Mask of (noncontiguous!) channel width field */

	CLM_DATA_FLAG_MEAS_COND	 = 0x00,	/* TX power specified as conducted limit */
	CLM_DATA_FLAG_MEAS_EIRP	 = 0x02,	/* TX power specified as EIRP limit */
	CLM_DATA_FLAG_MEAS_MASK	 = 0x02,	/* Mask of TX power limit type field */

	CLM_DATA_FLAG_MORE = 0x04,	/* Nonlast transmission power record in locale */

	/* Region flags */

	CLM_DATA_FLAG_REG_SC_RULES_MASK = 0x07,	/* Subchannel rules index */
	CLM_DATA_FLAG_REG_NO_BF         = 0x08,	/* Beamforming disabled */

	/* Subchannel rules bandwidth flags (bandwidth bitmask) == 1<<clm_bandwidth_t */

	CLM_DATA_FLAG_SC_RULE_BW_20 = 0x01,	/* Use 20MHz limits */
	CLM_DATA_FLAG_SC_RULE_BW_40 = 0x02,	/* Use 40MHz limits */
	CLM_DATA_FLAG_SC_RULE_BW_80 = 0x04,	/* Use 80MHz limits */
};

/** Subchannel identifiers == clm_limits_type-1 */
typedef enum clm_data_sub_chan_id {
	CLM_DATA_SUB_CHAN_L,
	CLM_DATA_SUB_CHAN_U,
	CLM_DATA_SUB_CHAN_LL,
	CLM_DATA_SUB_CHAN_LU,
	CLM_DATA_SUB_CHAN_UL,
	CLM_DATA_SUB_CHAN_UU,
	CLM_DATA_SUB_CHAN_NUM,
	CLM_DATA_SUB_CHAN_MAX_40 = CLM_DATA_SUB_CHAN_U + 1,
	CLM_DATA_SUB_CHAN_MAX_80 = CLM_DATA_SUB_CHAN_NUM
} clm_data_sub_chan_id_t;

/*
****************************
* CLM DATA BLOB STRUCTURES *
****************************
*/

/** Descriptor of channel comb
 * Channel comb is a sequence of evenly spaced channel numbers
 */
typedef struct clm_channel_comb {
	unsigned char first_channel;	/* First channel number */
	unsigned char last_channel;		/* Last channel number */
	unsigned char stride;			/* Distance between channel numbers in sequence */
} clm_channel_comb_t;

/** Descriptor of set of channel combs */
typedef struct clm_channel_comb_set {
	int num;							/* Number of combs in set */
	const struct clm_channel_comb *set; /* Address of combs' vector */
} clm_channel_comb_set_t;

/** Channel range descriptor */
typedef struct clm_channel_range {
	unsigned char start;	/* Number of first channel */
	unsigned char end;		/* Number of last channel */
} clm_channel_range_t;

/** Subchannel rules descriptor for channel range */
typedef struct clm_sub_chan_channel_rules {
	/** Channel range idx */
	unsigned char chan_range;

	/* Subchannel rules (sets of CLM_DATA_FLAG_SC_RULE_BW_ bits) */
	unsigned char sub_chan_rules[CLM_DATA_SUB_CHAN_NUM];
} clm_sub_chan_channel_rules_t;

/** Subchannel rules descriptor for region */
typedef struct clm_sub_chan_region_rules {
	/** Number of channel-range-level rules */
	int num;

	/** Array of channel-range-level rules */
	const clm_sub_chan_channel_rules_t *channel_rules;
} clm_sub_chan_region_rules_t;

/** Set of region-level subchannel rules */
typedef struct clm_sub_chan_rules_set {
	/** Number of region-level subchannel rules */
	int num;

	/** Array of region-level subchannel rules */
	const clm_sub_chan_region_rules_t *region_rules;
} clm_sub_chan_rules_set_t;

/** Region identifier */
typedef struct clm_cc_rev {
	char cc[2]; /* Region country code */
	unsigned char rev;	/* Region revison */
} clm_cc_rev_t;

/** Legacy region descriptor: 8-bit locale indices, no flags */
typedef struct clm_country_rev_definition_t {
	struct clm_cc_rev cc_rev;				/* Region identifier */
	unsigned char locales[CLM_LOC_IDX_NUM]; /* Indices of region locales' descriptors */
} clm_country_rev_definition_t;

/** Contemporary region descriptor, uses 10-bit locale indices, has flags */
typedef struct clm_country_rev_definition10_fl {
	struct clm_cc_rev cc_rev;				/* Region identifier */
	unsigned char locales[CLM_LOC_IDX_NUM]; /* Indices of region locales' descriptors */
	unsigned char hi_bits;					/* Higher bits of locale indices */
    unsigned char flags;					/* Region flags */
} clm_country_rev_definition10_fl_t;

/** Set of region descriptors */
typedef struct clm_country_rev_definition_set {
	int num;	  /* Number of region descriptors in set */
	const void *set; /* Vector of region descriptors */
} clm_country_rev_definition_set_t;

/** Region alias descriptor */
typedef struct clm_advertised_cc {
	char cc[2];			/* Aliased (effective) country codes */
	int num_aliases;	/* Number of region identifiers */
	const struct clm_cc_rev *aliases;	/* Vector of region identifiers */
} clm_advertised_cc_t;

/** Set of alias descriptors */
typedef struct clm_advertised_cc_set {
	int num;			/* Number of descriptors in set */
	const struct clm_advertised_cc *set;	/* Vector of alias descriptors */
} clm_advertised_cc_set_t;

/** Aggregation descriptor */
typedef struct clm_aggregate_cc {
	struct clm_cc_rev def_reg;	/* Default region identifier */
	int num_regions;	/* Number of region mappings in aggregation */
	const struct clm_cc_rev *regions;	/* Vector of aggregation's region mappings */
} clm_aggregate_cc_t;

/** Set of aggregation descriptors */
typedef struct clm_aggregate_cc_set {
	int num;			/* Number of aggregation dexcriptors */
	const struct clm_aggregate_cc *set; /* Vector of aggregation descriptors */
} clm_aggregate_cc_set_t;

/** Registry (TOC) of CLM data structures, obtained in BLOB */
typedef struct clm_data_registry {
	/** Valid 20MHz channels of 2.4GHz band */
	const struct clm_channel_comb_set *valid_channels_2g_20m;

	/** Valid 20MHz channels of 5GHz band  */
	const struct clm_channel_comb_set *valid_channels_5g_20m;

	/** Vector of channel range descriptors */
	const struct clm_channel_range *channel_ranges;

	/** Sequence of byte strings that encode restricted sets */
	const unsigned char *restricted_channels;

	/** Sequence of byte strings that encode locales' valid channels sets */
	const unsigned char *locale_valid_channels;

	/** Sequence of byte strings that encode rate sets */
	const unsigned char *locale_rate_sets;

	/** Byte string sequences that encode locale definitions */
	const unsigned char *locales[CLM_LOC_IDX_NUM];

	/** Address of region definitions set descriptor */
	const struct clm_country_rev_definition_set	 *countries;

	/** Address of alias definitions set descriptor */
	const struct clm_advertised_cc_set *advertised_ccs;

	/** Address of aggregation definitions set descriptor */
	const struct clm_aggregate_cc_set *aggregates;

	/** Flags */
	int flags;

	/** Address of subchannel rules set descriptor or 0 */
	const struct clm_sub_chan_rules_set *sub_chan_rules;
} clm_registry_t;

/** CLM data BLOB header */
typedef struct clm_data_header {

	/* CLM data header tag */
	char header_tag[10];

	/* CLM BLOB format version major number */
	short format_major;

	/* CLM BLOB format version minor number */
	short format_minor;

	/* CLM data set version string */
	char clm_version[20];

	/* CLM compiler version string */
	char compiler_version[10];

	/* Pointer to self (for relocation compensation) */
	const struct clm_data_header *self_pointer;

	/* CLM BLOB data registry */
	const struct clm_data_registry *data;

	/* CLM compiler version string */
	char generator_version[30];

	/* SW apps version string */
	char apps_version[20];
} clm_data_header_t;

#endif /* _WLC_CLM_DATA_H_ */
