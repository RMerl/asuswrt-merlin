/*
 *
 * Custom OID/ioctl definitions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Definitions subject to change without notice.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id: wlioctl.h 479485 2014-05-21 08:56:48Z $
 */

#ifndef _wlioctl_h_
#define	_wlioctl_h_

#include <typedefs.h>
#include <proto/ethernet.h>
#include <proto/bcmeth.h>
#include <proto/bcmip.h>
#include <proto/bcmevent.h>
#include <proto/802.11.h>
#include <proto/802.1d.h>
#include <bcmwifi_channels.h>
#include <bcmwifi_rates.h>
#include <devctrl_if/wlioctl_defs.h>

#ifndef LINUX_POSTMOGRIFY_REMOVAL
#include <bcm_mpool_pub.h>
#include <bcmcdc.h>
#endif /* LINUX_POSTMOGRIFY_REMOVAL */


#ifdef __NetBSD__
/* NetBSD 2.0 does not have SIOCDEVPRIVATE. */
#define SIOCDEVPRIVATE	_IOWR('i', 139, struct ifreq)
#endif

#ifndef LINUX_POSTMOGRIFY_REMOVAL

#ifndef INTF_NAME_SIZ
#define INTF_NAME_SIZ	16
#endif

/* Used to send ioctls over the transport pipe */
typedef struct remote_ioctl {
	cdc_ioctl_t	msg;
	uint32		data_len;
	char           intf_name[INTF_NAME_SIZ];
} rem_ioctl_t;
#define REMOTE_SIZE	sizeof(rem_ioctl_t)

typedef struct {
	uint32 num;
	chanspec_t list[1];
} chanspec_list_t;

/* DFS Forced param */
typedef struct wl_dfs_forced_params {
	chanspec_t chspec;
	uint16 version;
	chanspec_list_t chspec_list;
} wl_dfs_forced_t;

#define DFS_PREFCHANLIST_VER 0x01
#define WL_CHSPEC_LIST_FIXED_SIZE	OFFSETOF(chanspec_list_t, list)
#define WL_DFS_FORCED_PARAMS_FIXED_SIZE \
	(WL_CHSPEC_LIST_FIXED_SIZE + OFFSETOF(wl_dfs_forced_t, chspec_list))
#define WL_DFS_FORCED_PARAMS_MAX_SIZE \
	WL_DFS_FORCED_PARAMS_FIXED_SIZE + (WL_NUMCHANNELS * sizeof(chanspec_t))

/* association decision information */
typedef struct {
	bool		assoc_approved;		/* (re)association approved */
	uint16		reject_reason;		/* reason code for rejecting association */
	struct		ether_addr   da;
#if defined(NDIS) && (NDISVER >= 0x0620)
	LARGE_INTEGER	sys_time;		/* current system time */
#else
	int64		sys_time;		/* current system time */
#endif
} assoc_decision_t;

#define ACTION_FRAME_SIZE 1800

typedef struct wl_action_frame {
	struct ether_addr 	da;
	uint16 			len;
	uint32 			packetId;
	uint8			data[ACTION_FRAME_SIZE];
} wl_action_frame_t;

#define WL_WIFI_ACTION_FRAME_SIZE sizeof(struct wl_action_frame)

typedef struct ssid_info
{
	uint8		ssid_len;	/* the length of SSID */
	uint8		ssid[32];	/* SSID string */
} ssid_info_t;

typedef struct wl_af_params {
	uint32 			channel;
	int32 			dwell_time;
	struct ether_addr 	BSSID;
	wl_action_frame_t	action_frame;
} wl_af_params_t;

#define WL_WIFI_AF_PARAMS_SIZE sizeof(struct wl_af_params)

#define MFP_TEST_FLAG_NORMAL	0
#define MFP_TEST_FLAG_ANY_KEY	1
typedef struct wl_sa_query {
	uint32			flag;
	uint8 			action;
	uint16 			id;
	struct ether_addr 	da;
} wl_sa_query_t;

#endif /*  LINUX_POSTMOGRIFY_REMOVAL */

/* require default structure packing */
#define BWL_DEFAULT_PACKING
#include <packed_section_start.h>

typedef BWL_PRE_PACKED_STRUCT struct si_serial_init_param {
	uint8 interf; /* Uart interface number. 0- UART0, 1-UART1 etc. */
	uint32 baud_rate; /* Baud Rate */
} BWL_POST_PACKED_STRUCT si_serial_init_param_t;



#ifndef LINUX_POSTMOGRIFY_REMOVAL
/* Legacy structure to help keep backward compatible wl tool and tray app */

#define	LEGACY_WL_BSS_INFO_VERSION	107	/* older version of wl_bss_info struct */

typedef struct wl_bss_info_107 {
	uint32		version;		/* version field */
	uint32		length;			/* byte length of data in this record,
						 * starting at version and including IEs
						 */
	struct ether_addr BSSID;
	uint16		beacon_period;		/* units are Kusec */
	uint16		capability;		/* Capability information */
	uint8		SSID_len;
	uint8		SSID[32];
	struct {
		uint	count;			/* # rates in this set */
		uint8	rates[16];		/* rates in 500kbps units w/hi bit set if basic */
	} rateset;				/* supported rates */
	uint8		channel;		/* Channel no. */
	uint16		atim_window;		/* units are Kusec */
	uint8		dtim_period;		/* DTIM period */
	int16		RSSI;			/* receive signal strength (in dBm) */
	int8		phy_noise;		/* noise (in dBm) */
	uint32		ie_length;		/* byte length of Information Elements */
	/* variable length Information Elements */
} wl_bss_info_107_t;

/*
 * Per-BSS information structure.
 */

#define	LEGACY2_WL_BSS_INFO_VERSION	108		/* old version of wl_bss_info struct */

/* BSS info structure
 * Applications MUST CHECK ie_offset field and length field to access IEs and
 * next bss_info structure in a vector (in wl_scan_results_t)
 */
typedef struct wl_bss_info_108 {
	uint32		version;		/* version field */
	uint32		length;			/* byte length of data in this record,
						 * starting at version and including IEs
						 */
	struct ether_addr BSSID;
	uint16		beacon_period;		/* units are Kusec */
	uint16		capability;		/* Capability information */
	uint8		SSID_len;
	uint8		SSID[32];
	struct {
		uint	count;			/* # rates in this set */
		uint8	rates[16];		/* rates in 500kbps units w/hi bit set if basic */
	} rateset;				/* supported rates */
	chanspec_t	chanspec;		/* chanspec for bss */
	uint16		atim_window;		/* units are Kusec */
	uint8		dtim_period;		/* DTIM period */
	int16		RSSI;			/* receive signal strength (in dBm) */
	int8		phy_noise;		/* noise (in dBm) */

	uint8		n_cap;			/* BSS is 802.11N Capable */
	uint32		nbss_cap;		/* 802.11N BSS Capabilities (based on HT_CAP_*) */
	uint8		ctl_ch;			/* 802.11N BSS control channel number */
	uint32		reserved32[1];		/* Reserved for expansion of BSS properties */
	uint8		flags;			/* flags */
	uint8		reserved[3];		/* Reserved for expansion of BSS properties */
	uint8		basic_mcs[MCSSET_LEN];	/* 802.11N BSS required MCS set */

	uint16		ie_offset;		/* offset at which IEs start, from beginning */
	uint32		ie_length;		/* byte length of Information Elements */
	/* Add new fields here */
	/* variable length Information Elements */
} wl_bss_info_108_t;

#endif /* LINUX_POSTMOGRIFY_REMOVAL */

#define	WL_BSS_INFO_VERSION	109		/* current version of wl_bss_info struct */

/* BSS info structure
 * Applications MUST CHECK ie_offset field and length field to access IEs and
 * next bss_info structure in a vector (in wl_scan_results_t)
 */
typedef struct wl_bss_info {
	uint32		version;		/* version field */
	uint32		length;			/* byte length of data in this record,
						 * starting at version and including IEs
						 */
	struct ether_addr BSSID;
	uint16		beacon_period;		/* units are Kusec */
	uint16		capability;		/* Capability information */
	uint8		SSID_len;
	uint8		SSID[32];
	struct {
		uint	count;			/* # rates in this set */
		uint8	rates[16];		/* rates in 500kbps units w/hi bit set if basic */
	} rateset;				/* supported rates */
	chanspec_t	chanspec;		/* chanspec for bss */
	uint16		atim_window;		/* units are Kusec */
	uint8		dtim_period;		/* DTIM period */
	int16		RSSI;			/* receive signal strength (in dBm) */
	int8		phy_noise;		/* noise (in dBm) */

	uint8		n_cap;			/* BSS is 802.11N Capable */
	uint32		nbss_cap;		/* 802.11N+AC BSS Capabilities */
	uint8		ctl_ch;			/* 802.11N BSS control channel number */
	uint8		padding1[3];		/* explicit struct alignment padding */
	uint16		vht_rxmcsmap;	/* VHT rx mcs map (802.11ac IE, VHT_CAP_MCS_MAP_*) */
	uint16		vht_txmcsmap;	/* VHT tx mcs map (802.11ac IE, VHT_CAP_MCS_MAP_*) */
	uint8		flags;			/* flags */
	uint8		vht_cap;		/* BSS is vht capable */
	uint8		reserved[2];		/* Reserved for expansion of BSS properties */
	uint8		basic_mcs[MCSSET_LEN];	/* 802.11N BSS required MCS set */

	uint16		ie_offset;		/* offset at which IEs start, from beginning */
	uint32		ie_length;		/* byte length of Information Elements */
	int16		SNR;			/* average SNR of during frame reception */
	/* Add new fields here */
	/* variable length Information Elements */
} wl_bss_info_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL

typedef struct wl_bsscfg {
	uint32  bsscfg_idx;
	uint32  wsec;
	uint32  WPA_auth;
	uint32  wsec_index;
	uint32  associated;
	uint32  BSS;
	uint32  phytest_on;
	struct ether_addr   prev_BSSID;
	struct ether_addr   BSSID;
	uint32  targetbss_wpa2_flags;
	uint32 assoc_type;
	uint32 assoc_state;
} wl_bsscfg_t;

typedef struct wl_if_add {
	uint32  bsscfg_flags;
	uint32  if_flags;
	uint32  ap;
	struct ether_addr   mac_addr;
} wl_if_add_t;

typedef struct wl_bss_config {
	uint32	atim_window;
	uint32	beacon_period;
	uint32	chanspec;
} wl_bss_config_t;

#define WL_BSS_USER_RADAR_CHAN_SELECT	0x1	/* User application will randomly select
						 * radar channel.
						 */

#define DLOAD_HANDLER_VER			1	/* Downloader version */
#define DLOAD_FLAG_VER_MASK		0xf000	/* Downloader version mask */
#define DLOAD_FLAG_VER_SHIFT	12	/* Downloader version shift */

#define DL_CRC_NOT_INUSE 			0x0001

/* generic download types & flags */
enum {
	DL_TYPE_UCODE = 1,
	DL_TYPE_CLM = 2
};

/* ucode type values */
enum {
	UCODE_FW,
	INIT_VALS,
	BS_INIT_VALS
};

struct wl_dload_data {
	uint16 flag;
	uint16 dload_type;
	uint32 len;
	uint32 crc;
	uint8  data[1];
};
typedef struct wl_dload_data wl_dload_data_t;

struct wl_ucode_info {
	uint32 ucode_type;
	uint32 num_chunks;
	uint32 chunk_len;
	uint32 chunk_num;
	uint8  data_chunk[1];
};
typedef struct wl_ucode_info wl_ucode_info_t;

struct wl_clm_dload_info {
	uint32 ds_id;
	uint32 clm_total_len;
	uint32 num_chunks;
	uint32 chunk_len;
	uint32 chunk_offset;
	uint8  data_chunk[1];
};
typedef struct wl_clm_dload_info wl_clm_dload_info_t;

#endif /* LINUX_POSTMOGRIFY_REMOVAL */

typedef struct wlc_ssid {
	uint32		SSID_len;
	uchar		SSID[DOT11_MAX_SSID_LEN];
} wlc_ssid_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL
/* USB speed values */
enum {
	USB_UNKNOWN_SPEED = 0,
	USB_LOW_SPEED = 1,
	USB_FULL_SPEED = 2,
	USB_HIGH_SPEED = 3,
	USB_SUPER_SPEED = 4
};

typedef struct wlc_usbspeed_band_info {
	uint16	band;
	uint16	speed;
} wlc_usbspeed_band_info_t;

#define MAX_PREFERRED_AP_NUM     5
typedef struct wlc_fastssidinfo {
	uint32				SSID_channel[MAX_PREFERRED_AP_NUM];
	wlc_ssid_t		SSID_info[MAX_PREFERRED_AP_NUM];
} wlc_fastssidinfo_t;

typedef BWL_PRE_PACKED_STRUCT struct wnm_url {
	uint8   len;
	uint8   data[1];
} BWL_POST_PACKED_STRUCT wnm_url_t;

typedef struct chan_scandata {
	uint8		txpower;
	uint8		pad;
	chanspec_t	channel;	/* Channel num, bw, ctrl_sb and band */
	uint32		channel_mintime;
	uint32		channel_maxtime;
} chan_scandata_t;

typedef enum wl_scan_type {
	EXTDSCAN_FOREGROUND_SCAN,
	EXTDSCAN_BACKGROUND_SCAN,
	EXTDSCAN_FORCEDBACKGROUND_SCAN
} wl_scan_type_t;

#define WLC_EXTDSCAN_MAX_SSID		5

typedef struct wl_extdscan_params {
	int8 		nprobes;		/* 0, passive, otherwise active */
	int8    	split_scan;		/* split scan */
	int8		band;			/* band */
	int8		pad;
	wlc_ssid_t 	ssid[WLC_EXTDSCAN_MAX_SSID]; /* ssid list */
	uint32		tx_rate;		/* in 500ksec units */
	wl_scan_type_t	scan_type;		/* enum */
	int32 		channel_num;
	chan_scandata_t channel_list[1];	/* list of chandata structs */
} wl_extdscan_params_t;

#define WL_EXTDSCAN_PARAMS_FIXED_SIZE 	(sizeof(wl_extdscan_params_t) - sizeof(chan_scandata_t))

#define WL_SCAN_PARAMS_SSID_MAX 	10

typedef struct wl_scan_params {
	wlc_ssid_t ssid;		/* default: {0, ""} */
	struct ether_addr bssid;	/* default: bcast */
	int8 bss_type;			/* default: any,
					 * DOT11_BSSTYPE_ANY/INFRASTRUCTURE/INDEPENDENT
					 */
	uint8 scan_type;		/* flags, 0 use default */
	int32 nprobes;			/* -1 use default, number of probes per channel */
	int32 active_time;		/* -1 use default, dwell time per channel for
					 * active scanning
					 */
	int32 passive_time;		/* -1 use default, dwell time per channel
					 * for passive scanning
					 */
	int32 home_time;		/* -1 use default, dwell time for the home channel
					 * between channel scans
					 */
	int32 channel_num;		/* count of channels and ssids that follow
					 *
					 * low half is count of channels in channel_list, 0
					 * means default (use all available channels)
					 *
					 * high half is entries in wlc_ssid_t array that
					 * follows channel_list, aligned for int32 (4 bytes)
					 * meaning an odd channel count implies a 2-byte pad
					 * between end of channel_list and first ssid
					 *
					 * if ssid count is zero, single ssid in the fixed
					 * parameter portion is assumed, otherwise ssid in
					 * the fixed portion is ignored
					 */
	uint16 channel_list[1];		/* list of chanspecs */
} wl_scan_params_t;

/* size of wl_scan_params not including variable length array */
#define WL_SCAN_PARAMS_FIXED_SIZE 64

#define ISCAN_REQ_VERSION 1

/* incremental scan struct */
typedef struct wl_iscan_params {
	uint32 version;
	uint16 action;
	uint16 scan_duration;
	wl_scan_params_t params;
} wl_iscan_params_t;

/* 3 fields + size of wl_scan_params, not including variable length array */
#define WL_ISCAN_PARAMS_FIXED_SIZE (OFFSETOF(wl_iscan_params_t, params) + sizeof(wlc_ssid_t))
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

typedef struct wl_scan_results {
	uint32 buflen;
	uint32 version;
	uint32 count;
	wl_bss_info_t bss_info[1];
} wl_scan_results_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL
/* size of wl_scan_results not including variable length array */
#define WL_SCAN_RESULTS_FIXED_SIZE (sizeof(wl_scan_results_t) - sizeof(wl_bss_info_t))

#if defined(SIMPLE_ISCAN)
/* the buf lengh can be WLC_IOCTL_MAXLEN (8K) to reduce iteration */
#define WLC_IW_ISCAN_MAXLEN   2048
typedef struct iscan_buf {
	struct iscan_buf * next;
	char   iscan_buf[WLC_IW_ISCAN_MAXLEN];
} iscan_buf_t;
#endif /* SIMPLE_ISCAN */

#define ESCAN_REQ_VERSION 1

typedef struct wl_escan_params {
	uint32 version;
	uint16 action;
	uint16 sync_id;
	wl_scan_params_t params;
} wl_escan_params_t;

#define WL_ESCAN_PARAMS_FIXED_SIZE (OFFSETOF(wl_escan_params_t, params) + sizeof(wlc_ssid_t))

typedef struct wl_escan_result {
	uint32 buflen;
	uint32 version;
	uint16 sync_id;
	uint16 bss_count;
	wl_bss_info_t bss_info[1];
} wl_escan_result_t;

#define WL_ESCAN_RESULTS_FIXED_SIZE (sizeof(wl_escan_result_t) - sizeof(wl_bss_info_t))

/* incremental scan results struct */
typedef struct wl_iscan_results {
	uint32 status;
	wl_scan_results_t results;
} wl_iscan_results_t;

/* size of wl_iscan_results not including variable length array */
#define WL_ISCAN_RESULTS_FIXED_SIZE \
	(WL_SCAN_RESULTS_FIXED_SIZE + OFFSETOF(wl_iscan_results_t, results))

#define SCANOL_PARAMS_VERSION	1

typedef struct scanol_params {
	uint32 version;
	uint32 flags;	/* offload scanning flags */
	int32 active_time;	/* -1 use default, dwell time per channel for active scanning */
	int32 passive_time;	/* -1 use default, dwell time per channel for passive scanning */
	int32 idle_rest_time;	/* -1 use default, time idle between scan cycle */
	int32 idle_rest_time_multiplier;
	int32 active_rest_time;
	int32 active_rest_time_multiplier;
	int32 scan_cycle_idle_rest_time;
	int32 scan_cycle_idle_rest_multiplier;
	int32 scan_cycle_active_rest_time;
	int32 scan_cycle_active_rest_multiplier;
	int32 max_rest_time;
	int32 max_scan_cycles;
	int32 nprobes;		/* -1 use default, number of probes per channel */
	int32 scan_start_delay;
	uint32 nchannels;
	uint32 ssid_count;
	wlc_ssid_t ssidlist[1];
} scanol_params_t;

typedef struct wl_probe_params {
	wlc_ssid_t ssid;
	struct ether_addr bssid;
	struct ether_addr mac;
} wl_probe_params_t;
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

#define WL_MAXRATES_IN_SET		16	/* max # of rates in a rateset */
typedef struct wl_rateset {
	uint32	count;			/* # rates in this set */
	uint8	rates[WL_MAXRATES_IN_SET];	/* rates in 500kbps units w/hi bit set if basic */
} wl_rateset_t;

typedef struct wl_rateset_args {
	uint32	count;			/* # rates in this set */
	uint8	rates[WL_MAXRATES_IN_SET];	/* rates in 500kbps units w/hi bit set if basic */
	uint8   mcs[MCSSET_LEN];        /* supported mcs index bit map */
	uint16 vht_mcs[VHT_CAP_MCS_MAP_NSS_MAX]; /* supported mcs index bit map per nss */
} wl_rateset_args_t;

#define TXBF_RATE_MCS_ALL		4
#define TXBF_RATE_VHT_ALL		4
#define TXBF_RATE_OFDM_ALL		8

typedef struct wl_txbf_rateset {
	uint8	txbf_rate_mcs[TXBF_RATE_MCS_ALL];	/* one for each stream */
	uint8	txbf_rate_mcs_bcm[TXBF_RATE_MCS_ALL];	/* one for each stream */
	uint16	txbf_rate_vht[TXBF_RATE_VHT_ALL];	/* one for each stream */
	uint16	txbf_rate_vht_bcm[TXBF_RATE_VHT_ALL];	/* one for each stream */
	uint8	txbf_rate_ofdm[TXBF_RATE_OFDM_ALL];	/* bitmap of ofdm rates that enables txbf */
	uint8	txbf_rate_ofdm_bcm[TXBF_RATE_OFDM_ALL]; /* bitmap of ofdm rates that enables txbf */
	uint8	txbf_rate_ofdm_cnt;
	uint8	txbf_rate_ofdm_cnt_bcm;
} wl_txbf_rateset_t;

#define OFDM_RATE_MASK			0x0000007f
typedef uint8 ofdm_rates_t;

typedef struct wl_rates_info {
	wl_rateset_t rs_tgt;
	uint32 phy_type;
	int32 bandtype;
	uint8 cck_only;
	uint8 rate_mask;
	uint8 mcsallow;
	uint8 bw;
	uint8 txstreams;
} wl_rates_info_t;

/* uint32 list */
typedef struct wl_uint32_list {
	/* in - # of elements, out - # of entries */
	uint32 count;
	/* variable length uint32 list */
	uint32 element[1];
} wl_uint32_list_t;

/* used for association with a specific BSSID and chanspec list */
typedef struct wl_assoc_params {
	struct ether_addr bssid;	/* 00:00:00:00:00:00: broadcast scan */
	uint16 bssid_cnt;		/* 0: use chanspec_num, and the single bssid,
					* otherwise count of chanspecs in chanspec_list
					* AND paired bssids following chanspec_list
					* also, chanspec_num has to be set to zero
					* for bssid list to be used
					*/
	int32 chanspec_num;		/* 0: all available channels,
					* otherwise count of chanspecs in chanspec_list
					*/
	chanspec_t chanspec_list[1];	/* list of chanspecs */
} wl_assoc_params_t;

#define WL_ASSOC_PARAMS_FIXED_SIZE 	OFFSETOF(wl_assoc_params_t, chanspec_list)

/* used for reassociation/roam to a specific BSSID and channel */
typedef wl_assoc_params_t wl_reassoc_params_t;
#define WL_REASSOC_PARAMS_FIXED_SIZE	WL_ASSOC_PARAMS_FIXED_SIZE

/* used for association to a specific BSSID and channel */
typedef wl_assoc_params_t wl_join_assoc_params_t;
#define WL_JOIN_ASSOC_PARAMS_FIXED_SIZE	WL_ASSOC_PARAMS_FIXED_SIZE

/* used for join with or without a specific bssid and channel list */
typedef struct wl_join_params {
	wlc_ssid_t ssid;
	wl_assoc_params_t params;	/* optional field, but it must include the fixed portion
					 * of the wl_assoc_params_t struct when it does present.
					 */
} wl_join_params_t;

#ifndef  LINUX_POSTMOGRIFY_REMOVAL
#define WL_JOIN_PARAMS_FIXED_SIZE 	(OFFSETOF(wl_join_params_t, params) + \
					 WL_ASSOC_PARAMS_FIXED_SIZE)
/* scan params for extended join */
typedef struct wl_join_scan_params {
	uint8 scan_type;		/* 0 use default, active or passive scan */
	int32 nprobes;			/* -1 use default, number of probes per channel */
	int32 active_time;		/* -1 use default, dwell time per channel for
					 * active scanning
					 */
	int32 passive_time;		/* -1 use default, dwell time per channel
					 * for passive scanning
					 */
	int32 home_time;		/* -1 use default, dwell time for the home channel
					 * between channel scans
					 */
} wl_join_scan_params_t;

/* extended join params */
typedef struct wl_extjoin_params {
	wlc_ssid_t ssid;		/* {0, ""}: wildcard scan */
	wl_join_scan_params_t scan;
	wl_join_assoc_params_t assoc;	/* optional field, but it must include the fixed portion
					 * of the wl_join_assoc_params_t struct when it does
					 * present.
					 */
} wl_extjoin_params_t;
#define WL_EXTJOIN_PARAMS_FIXED_SIZE 	(OFFSETOF(wl_extjoin_params_t, assoc) + \
					 WL_JOIN_ASSOC_PARAMS_FIXED_SIZE)

#define ANT_SELCFG_MAX		4	/* max number of antenna configurations */
#define MAX_STREAMS_SUPPORTED	4	/* max number of streams supported */
typedef struct {
	uint8 ant_config[ANT_SELCFG_MAX];	/* antenna configuration */
	uint8 num_antcfg;	/* number of available antenna configurations */
} wlc_antselcfg_t;

typedef struct {
	uint32 duration;	/* millisecs spent sampling this channel */
	uint32 congest_ibss;	/* millisecs in our bss (presumably this traffic will */
				/*  move if cur bss moves channels) */
	uint32 congest_obss;	/* traffic not in our bss */
	uint32 interference;	/* millisecs detecting a non 802.11 interferer. */
	uint32 timestamp;	/* second timestamp */
} cca_congest_t;

typedef struct {
	chanspec_t chanspec;	/* Which channel? */
	uint16 num_secs;	/* How many secs worth of data */
	cca_congest_t  secs[1];	/* Data */
} cca_congest_channel_req_t;

typedef struct {
	uint32 duration;	/* millisecs spent sampling this channel */
	uint32 congest;		/* millisecs detecting busy CCA */
	uint32 timestamp;	/* second timestamp */
} cca_congest_simple_t;

typedef struct {
	uint16 status;
	uint16 id;
	chanspec_t chanspec;	/* Which channel? */
	uint16 len;
	union {
		cca_congest_simple_t  cca_busy;	/* CCA busy */
		int noise;			/* noise floor */
	};
} cca_chan_qual_event_t;

#if defined(BCMDBG)
typedef struct {
	uint32 msrmnt_time; /* Time for Measurement (msec) */
	uint32 msrmnt_done; /* flag set when measurement complete */
	char buf[1];
} cca_stats_n_flags;

typedef struct {
	uint32 msrmnt_query; /* host to driver query for measurement done */
	uint32 time_req;	/* Time required for measurement */
} cca_msrmnt_query;
#endif

/* interference sources */
enum interference_source {
	ITFR_NONE = 0,		/* interference */
	ITFR_PHONE,		/* wireless phone */
	ITFR_VIDEO_CAMERA,	/* wireless video camera */
	ITFR_MICROWAVE_OVEN,	/* microwave oven */
	ITFR_BABY_MONITOR,	/* wireless baby monitor */
	ITFR_BLUETOOTH,		/* bluetooth */
	ITFR_VIDEO_CAMERA_OR_BABY_MONITOR,	/* wireless camera or baby monitor */
	ITFR_BLUETOOTH_OR_BABY_MONITOR,	/* bluetooth or baby monitor */
	ITFR_VIDEO_CAMERA_OR_PHONE,	/* video camera or phone */
	ITFR_UNIDENTIFIED	/* interference from unidentified source */
};

/* structure for interference source report */
typedef struct {
	uint32 flags;	/* flags.  bit definitions below */
	uint32 source;	/* last detected interference source */
	uint32 timestamp;	/* second timestamp on interferenced flag change */
} interference_source_rep_t;
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

#define WLC_CNTRY_BUF_SZ	4		/* Country string is 3 bytes + NUL */

#ifndef LINUX_POSTMOGRIFY_REMOVAL

typedef struct wl_country {
	char country_abbrev[WLC_CNTRY_BUF_SZ];	/* nul-terminated country code used in
						 * the Country IE
						 */
	int32 rev;				/* revision specifier for ccode
						 * on set, -1 indicates unspecified.
						 * on get, rev >= 0
						 */
	char ccode[WLC_CNTRY_BUF_SZ];		/* nul-terminated built-in country code.
						 * variable length, but fixed size in
						 * struct allows simple allocation for
						 * expected country strings <= 3 chars.
						 */
} wl_country_t;

#define CCODE_INFO_VERSION 1

typedef enum wl_ccode_role {
	WLC_CCODE_ROLE_ACTIVE = 0,
	WLC_CCODE_ROLE_HOST,
	WLC_CCODE_ROLE_80211D_ASSOC,
	WLC_CCODE_ROLE_80211D_SCAN,
	WLC_CCODE_ROLE_DEFAULT,
	WLC_CCODE_LAST
} wl_ccode_role_t;
#define WLC_NUM_CCODE_INFO WLC_CCODE_LAST

typedef struct wl_ccode_entry {
	uint16 reserved;
	uint8 band;
	uint8 role;
	char	ccode[WLC_CNTRY_BUF_SZ];
} wl_ccode_entry_t;

typedef struct wl_ccode_info {
	uint16 version;
	uint16 count;   /* Number of ccodes entries in the set */
	wl_ccode_entry_t ccodelist[1];
} wl_ccode_info_t;
#define WL_CCODE_INFO_FIXED_LEN	OFFSETOF(wl_ccode_info_t, ccodelist)

typedef struct wl_channels_in_country {
	uint32 buflen;
	uint32 band;
	char country_abbrev[WLC_CNTRY_BUF_SZ];
	uint32 count;
	uint32 channel[1];
} wl_channels_in_country_t;

typedef struct wl_country_list {
	uint32 buflen;
	uint32 band_set;
	uint32 band;
	uint32 count;
	char country_abbrev[1];
} wl_country_list_t;

typedef struct wl_rm_req_elt {
	int8	type;
	int8	flags;
	chanspec_t	chanspec;
	uint32	token;		/* token for this measurement */
	uint32	tsf_h;		/* TSF high 32-bits of Measurement start time */
	uint32	tsf_l;		/* TSF low 32-bits */
	uint32	dur;		/* TUs */
} wl_rm_req_elt_t;

typedef struct wl_rm_req {
	uint32	token;		/* overall measurement set token */
	uint32	count;		/* number of measurement requests */
	void	*cb;		/* completion callback function: may be NULL */
	void	*cb_arg;	/* arg to completion callback function */
	wl_rm_req_elt_t	req[1];	/* variable length block of requests */
} wl_rm_req_t;
#define WL_RM_REQ_FIXED_LEN	OFFSETOF(wl_rm_req_t, req)

typedef struct wl_rm_rep_elt {
	int8	type;
	int8	flags;
	chanspec_t	chanspec;
	uint32	token;		/* token for this measurement */
	uint32	tsf_h;		/* TSF high 32-bits of Measurement start time */
	uint32	tsf_l;		/* TSF low 32-bits */
	uint32	dur;		/* TUs */
	uint32	len;		/* byte length of data block */
	uint8	data[1];	/* variable length data block */
} wl_rm_rep_elt_t;
#define WL_RM_REP_ELT_FIXED_LEN	24	/* length excluding data block */

#define WL_RPI_REP_BIN_NUM 8
typedef struct wl_rm_rpi_rep {
	uint8	rpi[WL_RPI_REP_BIN_NUM];
	int8	rpi_max[WL_RPI_REP_BIN_NUM];
} wl_rm_rpi_rep_t;

typedef struct wl_rm_rep {
	uint32	token;		/* overall measurement set token */
	uint32	len;		/* length of measurement report block */
	wl_rm_rep_elt_t	rep[1];	/* variable length block of reports */
} wl_rm_rep_t;
#define WL_RM_REP_FIXED_LEN	8


#if defined(BCMSUP_PSK) || defined(BCMDONGLEHOST)
typedef enum sup_auth_status {
	/* Basic supplicant authentication states */
	WLC_SUP_DISCONNECTED = 0,
	WLC_SUP_CONNECTING,
	WLC_SUP_IDREQUIRED,
	WLC_SUP_AUTHENTICATING,
	WLC_SUP_AUTHENTICATED,
	WLC_SUP_KEYXCHANGE,
	WLC_SUP_KEYED,
	WLC_SUP_TIMEOUT,
	WLC_SUP_LAST_BASIC_STATE,

	/* Extended supplicant authentication states */
	/* Waiting to receive handshake msg M1 */
	WLC_SUP_KEYXCHANGE_WAIT_M1 = WLC_SUP_AUTHENTICATED,
	/* Preparing to send handshake msg M2 */
	WLC_SUP_KEYXCHANGE_PREP_M2 = WLC_SUP_KEYXCHANGE,
	/* Waiting to receive handshake msg M3 */
	WLC_SUP_KEYXCHANGE_WAIT_M3 = WLC_SUP_LAST_BASIC_STATE,
	WLC_SUP_KEYXCHANGE_PREP_M4,	/* Preparing to send handshake msg M4 */
	WLC_SUP_KEYXCHANGE_WAIT_G1,	/* Waiting to receive handshake msg G1 */
	WLC_SUP_KEYXCHANGE_PREP_G2	/* Preparing to send handshake msg G2 */
} sup_auth_status_t;
#endif 
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

typedef struct wl_wsec_key {
	uint32		index;		/* key index */
	uint32		len;		/* key length */
	uint8		data[DOT11_MAX_KEY_SIZE];	/* key data */
	uint32		pad_1[18];
	uint32		algo;		/* CRYPTO_ALGO_AES_CCM, CRYPTO_ALGO_WEP128, etc */
	uint32		flags;		/* misc flags */
	uint32		pad_2[2];
	int		pad_3;
	int		iv_initialized;	/* has IV been initialized already? */
	int		pad_4;
	/* Rx IV */
	struct {
		uint32	hi;		/* upper 32 bits of IV */
		uint16	lo;		/* lower 16 bits of IV */
	} rxiv;
	uint32		pad_5[2];
	struct ether_addr ea;		/* per station */
} wl_wsec_key_t;

#define WSEC_MIN_PSK_LEN	8
#define WSEC_MAX_PSK_LEN	64

/* Flag for key material needing passhash'ing */
#define WSEC_PASSPHRASE		(1<<0)

/* receptacle for WLC_SET_WSEC_PMK parameter */
typedef struct {
	ushort	key_len;		/* octets in key material */
	ushort	flags;			/* key handling qualification */
	uint8	key[WSEC_MAX_PSK_LEN];	/* PMK material */
} wsec_pmk_t;

#define MAXPMKID 16
typedef struct _pmkid {
	struct ether_addr	BSSID;
	uint8			PMKID[WPA2_PMKID_LEN];
} pmkid_t;

typedef struct _pmkid_list {
	uint32	npmkid;
	pmkid_t	pmkid[1];
} pmkid_list_t;

typedef struct _pmkid_cand {
	struct ether_addr	BSSID;
	uint8			preauth;
} pmkid_cand_t;

typedef struct _pmkid_cand_list {
	uint32	npmkid_cand;
	pmkid_cand_t	pmkid_cand[1];
} pmkid_cand_list_t;

#define WL_STA_ANT_MAX		4	/* max possible rx antennas */

#ifndef LINUX_POSTMOGRIFY_REMOVAL
typedef struct wl_assoc_info {
	uint32		req_len;
	uint32		resp_len;
	uint32		flags;
	struct dot11_assoc_req req;
	struct ether_addr reassoc_bssid; /* used in reassoc's */
	struct dot11_assoc_resp resp;
} wl_assoc_info_t;

typedef struct wl_led_info {
	uint32      index;      /* led index */
	uint32      behavior;
	uint8       activehi;
} wl_led_info_t;


/* srom read/write struct passed through ioctl */
typedef struct {
	uint	byteoff;	/* byte offset */
	uint	nbytes;		/* number of bytes */
	uint16	buf[1];
} srom_rw_t;

#define CISH_FLAG_PCIECIS	(1 << 15)	/* write CIS format bit for PCIe CIS */
/* similar cis (srom or otp) struct [iovar: may not be aligned] */
typedef struct {
	uint16	source;		/* cis source */
	uint16	flags;		/* flags */
	uint32	byteoff;	/* byte offset */
	uint32	nbytes;		/* number of bytes */
	/* data follows here */
} cis_rw_t;

/* R_REG and W_REG struct passed through ioctl */
typedef struct {
	uint32	byteoff;	/* byte offset of the field in d11regs_t */
	uint32	val;		/* read/write value of the field */
	uint32	size;		/* sizeof the field */
	uint	band;		/* band (optional) */
} rw_reg_t;

/* Structure used by GET/SET_ATTEN ioctls - it controls power in b/g-band */
/* PCL - Power Control Loop */
typedef struct {
	uint16	auto_ctrl;	/* WL_ATTEN_XX */
	uint16	bb;		/* Baseband attenuation */
	uint16	radio;		/* Radio attenuation */
	uint16	txctl1;		/* Radio TX_CTL1 value */
} atten_t;

/* Per-AC retry parameters */
struct wme_tx_params_s {
	uint8  short_retry;
	uint8  short_fallback;
	uint8  long_retry;
	uint8  long_fallback;
	uint16 max_rate;  /* In units of 512 Kbps */
};

typedef struct wme_tx_params_s wme_tx_params_t;

#define WL_WME_TX_PARAMS_IO_BYTES (sizeof(wme_tx_params_t) * AC_COUNT)

typedef struct wl_plc_nodelist {
	uint32 count;			/* Number of nodes */
	struct _node {
		struct ether_addr ea;	/* Node ether address */
		uint32 node_type;	/* Node type */
		uint32 cost;		/* PLC affinity */
	} node[1];
} wl_plc_nodelist_t;

typedef struct wl_plc_params {
	uint32	cmd;			/* Command */
	uint8	plc_failover;		/* PLC failover control/status */
	struct	ether_addr node_ea;	/* Node ether address */
	uint32	cost;			/* Link cost or mac cost */
} wl_plc_params_t;

/* Used to get specific link/ac parameters */
typedef struct {
	int32 ac;
	uint8 val;
	struct ether_addr ea;
} link_val_t;



typedef struct {
	uint16			ver;		/* version of this struct */
	uint16			len;		/* length in bytes of this structure */
	uint16			cap;		/* sta's advertised capabilities */
	uint32			flags;		/* flags defined below */
	uint32			idle;		/* time since data pkt rx'd from sta */
	struct ether_addr	ea;		/* Station address */
	wl_rateset_t		rateset;	/* rateset in use */
	uint32			in;		/* seconds elapsed since associated */
	uint32			listen_interval_inms; /* Min Listen interval in ms for this STA */
	uint32			tx_pkts;	/* # of user packets transmitted (unicast) */
	uint32			tx_failures;	/* # of user packets failed */
	uint32			rx_ucast_pkts;	/* # of unicast packets received */
	uint32			rx_mcast_pkts;	/* # of multicast packets received */
	uint32			tx_rate;	/* Rate used by last tx frame */
	uint32			rx_rate;	/* Rate of last successful rx frame */
	uint32			rx_decrypt_succeeds;	/* # of packet decrypted successfully */
	uint32			rx_decrypt_failures;	/* # of packet decrypted unsuccessfully */
	uint32			tx_tot_pkts;	/* # of user tx pkts (ucast + mcast) */
	uint32			rx_tot_pkts;	/* # of data packets recvd (uni + mcast) */
	uint32			tx_mcast_pkts;	/* # of mcast pkts txed */
	uint64			tx_tot_bytes;	/* data bytes txed (ucast + mcast) */
	uint64			rx_tot_bytes;	/* data bytes recvd (ucast + mcast) */
	uint64			tx_ucast_bytes;	/* data bytes txed (ucast) */
	uint64			tx_mcast_bytes;	/* # data bytes txed (mcast) */
	uint64			rx_ucast_bytes;	/* data bytes recvd (ucast) */
	uint64			rx_mcast_bytes;	/* data bytes recvd (mcast) */
	int8			rssi[WL_STA_ANT_MAX];	/* per antenna rssi */
	int8			nf[WL_STA_ANT_MAX];	/* per antenna noise floor */
	uint16			aid;			/* association ID */
	uint16			ht_capabilities;	/* advertised ht caps */
	uint16			vht_flags;		/* converted vht flags */
	uint32			tx_pkts_retry_cnt;	/* # of frames where a retry was
							 * necessary (obsolete)
							 */
	uint32			tx_pkts_retry_exhausted; /* # of user frames where a retry
							  * was exhausted
							  */
	int8			rx_lastpkt_rssi[WL_STA_ANT_MAX]; /* Per antenna RSSI of last
								  * received data frame.
								  */
	/* TX WLAN retry/failure statistics:
	 * Separated for host requested frames and WLAN locally generated frames.
	 * Include unicast frame only where the retries/failures can be counted.
	 */
	uint32			tx_pkts_total;		/* # user frames sent successfully */
	uint32			tx_pkts_retries;	/* # user frames retries */
	uint32			tx_pkts_fw_total;	/* # FW generated sent successfully */
	uint32			tx_pkts_fw_retries;	/* # retries for FW generated frames */
	uint32			tx_pkts_fw_retry_exhausted;	/* # FW generated where a retry
								 * was exhausted
								 */
	uint32			rx_pkts_retried;	/* # rx with retry bit set */
	uint32			tx_rate_fallback;	/* lowest fallback TX rate */
} sta_info_t;

#define WL_OLD_STAINFO_SIZE	OFFSETOF(sta_info_t, tx_tot_pkts)

#define WL_STA_VER		4

#endif /* LINUX_POSTMOGRIFY_REMOVAL */

#define	WLC_NUMRATES	16	/* max # of rates in a rateset */

typedef struct wlc_rateset {
	uint32	count;			/* number of rates in rates[] */
	uint8	rates[WLC_NUMRATES];	/* rates in 500kbps units w/hi bit set if basic */
	uint8	htphy_membership;	/* HT PHY Membership */
	uint8	mcs[MCSSET_LEN];	/* supported mcs index bit map */
	uint16  vht_mcsmap;		/* supported vht mcs nss bit map */
} wlc_rateset_t;

/* Used to get specific STA parameters */
typedef struct {
	uint32	val;
	struct ether_addr ea;
} scb_val_t;

/* Used by iovar versions of some ioctls, i.e. WLC_SCB_AUTHORIZE et al */
typedef struct {
	uint32 code;
	scb_val_t ioctl_args;
} authops_t;

/* channel encoding */
typedef struct channel_info {
	int hw_channel;
	int target_channel;
	int scan_channel;
} channel_info_t;

/* For ioctls that take a list of MAC addresses */
typedef struct maclist {
	uint count;			/* number of MAC addresses */
	struct ether_addr ea[1];	/* variable length array of MAC addresses */
} maclist_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL
/* get pkt count struct passed through ioctl */
typedef struct get_pktcnt {
	uint rx_good_pkt;
	uint rx_bad_pkt;
	uint tx_good_pkt;
	uint tx_bad_pkt;
	uint rx_ocast_good_pkt; /* unicast packets destined for others */
} get_pktcnt_t;

/* NINTENDO2 */
#define LQ_IDX_MIN              0
#define LQ_IDX_MAX              1
#define LQ_IDX_AVG              2
#define LQ_IDX_SUM              2
#define LQ_IDX_LAST             3
#define LQ_STOP_MONITOR         0
#define LQ_START_MONITOR        1

/* Get averages RSSI, Rx PHY rate and SNR values */
typedef struct {
	int rssi[LQ_IDX_LAST];  /* Array to keep min, max, avg rssi */
	int snr[LQ_IDX_LAST];   /* Array to keep min, max, avg snr */
	int isvalid;            /* Flag indicating whether above data is valid */
} wl_lq_t; /* Link Quality */

typedef enum wl_wakeup_reason_type {
	LCD_ON = 1,
	LCD_OFF,
	DRC1_WAKE,
	DRC2_WAKE,
	REASON_LAST
} wl_wr_type_t;

typedef struct {
/* Unique filter id */
	uint32	id;

/* stores the reason for the last wake up */
	uint8	reason;
} wl_wr_t;

/* Get MAC specific rate histogram command */
typedef struct {
	struct	ether_addr ea;	/* MAC Address */
	uint8	ac_cat;	/* Access Category */
	uint8	num_pkts;	/* Number of packet entries to be averaged */
} wl_mac_ratehisto_cmd_t;	/* MAC Specific Rate Histogram command */

/* Get MAC rate histogram response */
typedef struct {
	uint32	rate[DOT11_RATE_MAX + 1];	/* Rates */
	uint32	mcs[WL_RATESET_SZ_HT_MCS * WL_TX_CHAINS_MAX];	/* MCS counts */
	uint32	vht[WL_RATESET_SZ_VHT_MCS][WL_TX_CHAINS_MAX];	/* VHT counts */
	uint32	tsf_timer[2][2];	/* Start and End time for 8bytes value */
} wl_mac_ratehisto_res_t;	/* MAC Specific Rate Histogram Response */

#endif /* LINUX_POSTMOGRIFY_REMOVAL */

/* Linux network driver ioctl encoding */
typedef struct wl_ioctl {
	uint cmd;	/* common ioctl definition */
	void *buf;	/* pointer to user buffer */
	uint len;	/* length of user buffer */
	uint8 set;		/* 1=set IOCTL; 0=query IOCTL */
	uint used;	/* bytes read or written (optional) */
	uint needed;	/* bytes needed (optional) */
} wl_ioctl_t;

#define WL_NUM_RATES_CCK			4 /* 1, 2, 5.5, 11 Mbps */
#define WL_NUM_RATES_OFDM			8 /* 6, 9, 12, 18, 24, 36, 48, 54 Mbps SISO/CDD */
#define WL_NUM_RATES_MCS_1STREAM	8 /* MCS 0-7 1-stream rates - SISO/CDD/STBC/MCS */
#define WL_NUM_RATES_EXTRA_VHT		2 /* Additional VHT 11AC rates */
#define WL_NUM_RATES_VHT			10
#define WL_NUM_RATES_MCS32			1

#ifndef LINUX_POSTMOGRIFY_REMOVAL

/*
 * Structure for passing hardware and software
 * revision info up from the driver.
 */
typedef struct wlc_rev_info {
	uint		vendorid;	/* PCI vendor id */
	uint		deviceid;	/* device id of chip */
	uint		radiorev;	/* radio revision */
	uint		chiprev;	/* chip revision */
	uint		corerev;	/* core revision */
	uint		boardid;	/* board identifier (usu. PCI sub-device id) */
	uint		boardvendor;	/* board vendor (usu. PCI sub-vendor id) */
	uint		boardrev;	/* board revision */
	uint		driverrev;	/* driver version */
	uint		ucoderev;	/* microcode version */
	uint		bus;		/* bus type */
	uint		chipnum;	/* chip number */
	uint		phytype;	/* phy type */
	uint		phyrev;		/* phy revision */
	uint		anarev;		/* anacore rev */
	uint		chippkg;	/* chip package info */
	uint		nvramrev;	/* nvram revision number */
} wlc_rev_info_t;

#define WL_REV_INFO_LEGACY_LENGTH	48

#define WL_BRAND_MAX 10
typedef struct wl_instance_info {
	uint instance;
	char brand[WL_BRAND_MAX];
} wl_instance_info_t;

/* structure to change size of tx fifo */
typedef struct wl_txfifo_sz {
	uint16	magic;
	uint16	fifo;
	uint16	size;
} wl_txfifo_sz_t;

/* Transfer info about an IOVar from the driver */
/* Max supported IOV name size in bytes, + 1 for nul termination */
#define WLC_IOV_NAME_LEN 30
typedef struct wlc_iov_trx_s {
	uint8 module;
	uint8 type;
	char name[WLC_IOV_NAME_LEN];
} wlc_iov_trx_t;

/* bump this number if you change the ioctl interface */
#define WLC_IOCTL_VERSION	2
#define WLC_IOCTL_VERSION_LEGACY_IOTYPES	1

#ifdef CONFIG_USBRNDIS_RETAIL
/* struct passed in for WLC_NDCONFIG_ITEM */
typedef struct {
	char *name;
	void *param;
} ndconfig_item_t;
#endif


#define WL_PHY_PAVARS_LEN	32	/* Phy type, Band range, chain, a1[0], b0[0], b1[0] ... */

#define WL_PHY_PAVAR_VER	1	/* pavars version */
#define WL_PHY_PAVARS2_NUM	3	/* a1, b0, b1 */
typedef struct wl_pavars2 {
	uint16 ver;		/* version of this struct */
	uint16 len;		/* len of this structure */
	uint16 inuse;		/* driver return 1 for a1,b0,b1 in current band range */
	uint16 phy_type;	/* phy type */
	uint16 bandrange;
	uint16 chain;
	uint16 inpa[WL_PHY_PAVARS2_NUM];	/* phy pavars for one band range */
} wl_pavars2_t;

typedef struct wl_po {
	uint16	phy_type;	/* Phy type */
	uint16	band;
	uint16	cckpo;
	uint32	ofdmpo;
	uint16	mcspo[8];
} wl_po_t;

#define WL_NUM_RPCALVARS 5	/* number of rpcal vars */

typedef struct wl_rpcal {
	uint16 value;
	uint16 update;
} wl_rpcal_t;

typedef struct wl_aci_args {
	int enter_aci_thresh; /* Trigger level to start detecting ACI */
	int exit_aci_thresh; /* Trigger level to exit ACI mode */
	int usec_spin; /* microsecs to delay between rssi samples */
	int glitch_delay; /* interval between ACI scans when glitch count is consistently high */
	uint16 nphy_adcpwr_enter_thresh;	/* ADC power to enter ACI mitigation mode */
	uint16 nphy_adcpwr_exit_thresh;	/* ADC power to exit ACI mitigation mode */
	uint16 nphy_repeat_ctr;		/* Number of tries per channel to compute power */
	uint16 nphy_num_samples;	/* Number of samples to compute power on one channel */
	uint16 nphy_undetect_window_sz;	/* num of undetects to exit ACI Mitigation mode */
	uint16 nphy_b_energy_lo_aci;	/* low ACI power energy threshold for bphy */
	uint16 nphy_b_energy_md_aci;	/* mid ACI power energy threshold for bphy */
	uint16 nphy_b_energy_hi_aci;	/* high ACI power energy threshold for bphy */
	uint16 nphy_noise_noassoc_glitch_th_up; /* wl interference 4 */
	uint16 nphy_noise_noassoc_glitch_th_dn;
	uint16 nphy_noise_assoc_glitch_th_up;
	uint16 nphy_noise_assoc_glitch_th_dn;
	uint16 nphy_noise_assoc_aci_glitch_th_up;
	uint16 nphy_noise_assoc_aci_glitch_th_dn;
	uint16 nphy_noise_assoc_enter_th;
	uint16 nphy_noise_noassoc_enter_th;
	uint16 nphy_noise_assoc_rx_glitch_badplcp_enter_th;
	uint16 nphy_noise_noassoc_crsidx_incr;
	uint16 nphy_noise_assoc_crsidx_incr;
	uint16 nphy_noise_crsidx_decr;
} wl_aci_args_t;

#define WL_ACI_ARGS_LEGACY_LENGTH	16	/* bytes of pre NPHY aci args */
#define	WL_SAMPLECOLLECT_T_VERSION	2	/* version of wl_samplecollect_args_t struct */
typedef struct wl_samplecollect_args {
	/* version 0 fields */
	uint8 coll_us;
	int cores;
	/* add'l version 1 fields */
	uint16 version;     /* see definition of WL_SAMPLECOLLECT_T_VERSION */
	uint16 length;      /* length of entire structure */
	int8 trigger;
	uint16 timeout;
	uint16 mode;
	uint32 pre_dur;
	uint32 post_dur;
	uint8 gpio_sel;
	uint8 downsamp;
	uint8 be_deaf;
	uint8 agc;		/* loop from init gain and going down */
	uint8 filter;		/* override high pass corners to lowest */
	/* add'l version 2 fields */
	uint8 trigger_state;
	uint8 module_sel1;
	uint8 module_sel2;
	uint16 nsamps;
	int bitStart;
	uint32 gpioCapMask;
} wl_samplecollect_args_t;

#define	WL_SAMPLEDATA_T_VERSION		1	/* version of wl_samplecollect_args_t struct */
/* version for unpacked sample data, int16 {(I,Q),Core(0..N)} */
#define	WL_SAMPLEDATA_T_VERSION_SPEC_AN 2

typedef struct wl_sampledata {
	uint16 version;	/* structure version */
	uint16 size;	/* size of structure */
	uint16 tag;	/* Header/Data */
	uint16 length;	/* data length */
	uint32 flag;	/* bit def */
} wl_sampledata_t;


/* WL_OTA START */
/* OTA Test Status */
enum {
	WL_OTA_TEST_IDLE = 0,	/* Default Idle state */
	WL_OTA_TEST_ACTIVE = 1,	/* Test Running */
	WL_OTA_TEST_SUCCESS = 2,	/* Successfully Finished Test */
	WL_OTA_TEST_FAIL = 3	/* Test Failed in the Middle */
};
/* OTA SYNC Status */
enum {
	WL_OTA_SYNC_IDLE = 0,	/* Idle state */
	WL_OTA_SYNC_ACTIVE = 1,	/* Waiting for Sync */
	WL_OTA_SYNC_FAIL = 2	/* Sync pkt not recieved */
};

/* Various error states dut can get stuck during test */
enum {
	WL_OTA_SKIP_TEST_CAL_FAIL = 1,		/* Phy calibration failed */
	WL_OTA_SKIP_TEST_SYNCH_FAIL = 2,		/* Sync Packet not recieved */
	WL_OTA_SKIP_TEST_FILE_DWNLD_FAIL = 3,	/* Cmd flow file download failed */
	WL_OTA_SKIP_TEST_NO_TEST_FOUND = 4,	/* No test found in Flow file */
	WL_OTA_SKIP_TEST_WL_NOT_UP = 5,		/* WL UP failed */
	WL_OTA_SKIP_TEST_UNKNOWN_CALL		/* Unintentional scheduling on ota test */
};

/* Differentiator for ota_tx and ota_rx */
enum {
	WL_OTA_TEST_TX = 0,		/* ota_tx */
	WL_OTA_TEST_RX = 1,		/* ota_rx */
};

/* Catch 3 modes of operation: 20Mhz, 40Mhz, 20 in 40 Mhz */
enum {
	WL_OTA_TEST_BW_20MHZ = 1,		/* 20 Mhz operation */
	WL_OTA_TEST_BW_40MHZ = 2,		/* full 40Mhz operation */
	WL_OTA_TEST_BW_80MHZ = 3		/* full 80Mhz operation */
};

#define HT_MCS_INUSE	0x00000080	/* HT MCS in use,indicates b0-6 holds an mcs */
#define VHT_MCS_INUSE	0x00000100	/* VHT MCS in use,indicates b0-6 holds an mcs */
#define OTA_RATE_MASK 0x0000007f	/* rate/mcs value */
#define OTA_STF_SISO	0
#define OTA_STF_CDD		1


typedef struct ota_rate_info {
	uint8 rate_cnt;					/* Total number of rates */
	uint16 rate_val_mbps[WL_OTA_TEST_MAX_NUM_RATE];	/* array of rates from 1mbps to 130mbps */
							/* for legacy rates : ratein mbps * 2 */
							/* for HT rates : mcs index */
} ota_rate_info_t;

typedef struct ota_power_info {
	int8 pwr_ctrl_on;	/* power control on/off */
	int8 start_pwr;		/* starting power/index */
	int8 delta_pwr;		/* delta power/index */
	int8 end_pwr;		/* end power/index */
} ota_power_info_t;

typedef struct ota_packetengine {
	uint16 delay;           /* Inter-packet delay */
				/* for ota_tx, delay is tx ifs in micro seconds */
				/* for ota_rx, delay is wait time in milliseconds */
	uint16 nframes;         /* Number of frames */
	uint16 length;          /* Packet length */
} ota_packetengine_t;

/* Test info vector */
typedef struct wl_ota_test_args {
	uint8 cur_test;			/* test phase */
	uint8 chan;			/* channel */
	uint8 bw;			/* bandwidth */
	uint8 control_band;		/* control band */
	uint8 stf_mode;			/* stf mode */
	ota_rate_info_t rt_info;	/* Rate info */
	ota_packetengine_t pkteng;	/* packeteng info */
	uint8 txant;			/* tx antenna */
	uint8 rxant;			/* rx antenna */
	ota_power_info_t pwr_info;	/* power sweep info */
	uint8 wait_for_sync;		/* wait for sync or not */
	uint8 ldpc;
	uint8 sgi;
} wl_ota_test_args_t;

typedef struct wl_ota_test_vector {
	wl_ota_test_args_t test_arg[WL_OTA_TEST_MAX_NUM_SEQ];	/* Test argument struct */
	uint16 test_cnt;					/* Total no of test */
	uint8 file_dwnld_valid;					/* File successfully downloaded */
	uint8 sync_timeout;					/* sync packet timeout */
	int8 sync_fail_action;					/* sync fail action */
	struct ether_addr sync_mac;				/* macaddress for sync pkt */
	struct ether_addr tx_mac;				/* macaddress for tx */
	struct ether_addr rx_mac;				/* macaddress for rx */
	int8 loop_test;					/* dbg feature to loop the test */
} wl_ota_test_vector_t;


/* struct copied back form dongle to host to query the status */
typedef struct wl_ota_test_status {
	int16 cur_test_cnt;		/* test phase */
	int8 skip_test_reason;		/* skip test reasoin */
	wl_ota_test_args_t test_arg;	/* cur test arg details */
	uint16 test_cnt;		/* total no of test downloaded */
	uint8 file_dwnld_valid;		/* file successfully downloaded ? */
	uint8 sync_timeout;		/* sync timeout */
	int8 sync_fail_action;		/* sync fail action */
	struct ether_addr sync_mac;	/* macaddress for sync pkt */
	struct ether_addr tx_mac;	/* tx mac address */
	struct ether_addr rx_mac;	/* rx mac address */
	uint8  test_stage;		/* check the test status */
	int8 loop_test;		/* Debug feature to puts test enfine in a loop */
	uint8 sync_status;		/* sync status */
} wl_ota_test_status_t;

/* WL_OTA END */

/* wl_radar_args_t */
typedef struct {
	int npulses;	/* required number of pulses at n * t_int */
	int ncontig;	/* required number of pulses at t_int */
	int min_pw;	/* minimum pulse width (20 MHz clocks) */
	int max_pw;	/* maximum pulse width (20 MHz clocks) */
	uint16 thresh0;	/* Radar detection, thresh 0 */
	uint16 thresh1;	/* Radar detection, thresh 1 */
	uint16 blank;	/* Radar detection, blank control */
	uint16 fmdemodcfg;	/* Radar detection, fmdemod config */
	int npulses_lp;  /* Radar detection, minimum long pulses */
	int min_pw_lp; /* Minimum pulsewidth for long pulses */
	int max_pw_lp; /* Maximum pulsewidth for long pulses */
	int min_fm_lp; /* Minimum fm for long pulses */
	int max_span_lp;  /* Maximum deltat for long pulses */
	int min_deltat; /* Minimum spacing between pulses */
	int max_deltat; /* Maximum spacing between pulses */
	uint16 autocorr;	/* Radar detection, autocorr on or off */
	uint16 st_level_time;	/* Radar detection, start_timing level */
	uint16 t2_min; /* minimum clocks needed to remain in state 2 */
	uint32 version; /* version */
	uint32 fra_pulse_err;	/* sample error margin for detecting French radar pulsed */
	int npulses_fra;  /* Radar detection, minimum French pulses set */
	int npulses_stg2;  /* Radar detection, minimum staggered-2 pulses set */
	int npulses_stg3;  /* Radar detection, minimum staggered-3 pulses set */
	uint16 percal_mask;	/* defines which period cal is masked from radar detection */
	int quant;	/* quantization resolution to pulse positions */
	uint32 min_burst_intv_lp;	/* minimum burst to burst interval for bin3 radar */
	uint32 max_burst_intv_lp;	/* maximum burst to burst interval for bin3 radar */
	int nskip_rst_lp;	/* number of skipped pulses before resetting lp buffer */
	int max_pw_tol;	/* maximum tollerance allowed in detected pulse width for radar detection */
	uint16 feature_mask; /* 16-bit mask to specify enabled features */
} wl_radar_args_t;

#define WL_RADAR_ARGS_VERSION 2

typedef struct {
	uint32 version; /* version */
	uint16 thresh0_20_lo;	/* Radar detection, thresh 0 (range 5250-5350MHz) for BW 20MHz */
	uint16 thresh1_20_lo;	/* Radar detection, thresh 1 (range 5250-5350MHz) for BW 20MHz */
	uint16 thresh0_40_lo;	/* Radar detection, thresh 0 (range 5250-5350MHz) for BW 40MHz */
	uint16 thresh1_40_lo;	/* Radar detection, thresh 1 (range 5250-5350MHz) for BW 40MHz */
	uint16 thresh0_80_lo;	/* Radar detection, thresh 0 (range 5250-5350MHz) for BW 80MHz */
	uint16 thresh1_80_lo;	/* Radar detection, thresh 1 (range 5250-5350MHz) for BW 80MHz */
	uint16 thresh0_20_hi;	/* Radar detection, thresh 0 (range 5470-5725MHz) for BW 20MHz */
	uint16 thresh1_20_hi;	/* Radar detection, thresh 1 (range 5470-5725MHz) for BW 20MHz */
	uint16 thresh0_40_hi;	/* Radar detection, thresh 0 (range 5470-5725MHz) for BW 40MHz */
	uint16 thresh1_40_hi;	/* Radar detection, thresh 1 (range 5470-5725MHz) for BW 40MHz */
	uint16 thresh0_80_hi;	/* Radar detection, thresh 0 (range 5470-5725MHz) for BW 80MHz */
	uint16 thresh1_80_hi;	/* Radar detection, thresh 1 (range 5470-5725MHz) for BW 80MHz */
#ifdef WL11AC160
	uint16 thresh0_160_lo;	/* Radar detection, thresh 0 (range 5250-5350MHz) for BW 160MHz */
	uint16 thresh1_160_lo;	/* Radar detection, thresh 1 (range 5250-5350MHz) for BW 160MHz */
	uint16 thresh0_160_hi;	/* Radar detection, thresh 0 (range 5470-5725MHz) for BW 160MHz */
	uint16 thresh1_160_hi;	/* Radar detection, thresh 1 (range 5470-5725MHz) for BW 160MHz */
#endif /* WL11AC160 */
} wl_radar_thr_t;

#define WL_RADAR_THR_VERSION	2

/* RSSI per antenna */
typedef struct {
	uint32	version;		/* version field */
	uint32	count;			/* number of valid antenna rssi */
	int8 rssi_ant[WL_RSSI_ANT_MAX];	/* rssi per antenna */
} wl_rssi_ant_t;

/* data structure used in 'dfs_status' wl interface, which is used to query dfs status */
typedef struct {
	uint state;		/* noted by WL_DFS_CACSTATE_XX. */
	uint duration;		/* time spent in ms in state. */
	/* as dfs enters ISM state, it removes the operational channel from quiet channel
	 * list and notes the channel in channel_cleared. set to 0 if no channel is cleared
	 */
	chanspec_t chanspec_cleared;
	/* chanspec cleared used to be a uint, add another to uint16 to maintain size */
	uint16 pad;
} wl_dfs_status_t;

/* data structure used in 'radar_status' wl interface, which is use to query radar det status */
typedef struct {
	bool detected;
	int count;
	bool pretended;
	uint32 radartype;
	uint32 timenow;
	uint32 timefromL;
	int lp_csect_single;
	int detected_pulse_index;
	int nconsecq_pulses;
	chanspec_t ch;
	int pw[10];
	int intv[10];
	int fm[10];
} wl_radar_status_t;

#define NUM_PWRCTRL_RATES 12

typedef struct {
	uint8 txpwr_band_max[NUM_PWRCTRL_RATES];	/* User set target */
	uint8 txpwr_limit[NUM_PWRCTRL_RATES];		/* reg and local power limit */
	uint8 txpwr_local_max;				/* local max according to the AP */
	uint8 txpwr_local_constraint;			/* local constraint according to the AP */
	uint8 txpwr_chan_reg_max;			/* Regulatory max for this channel */
	uint8 txpwr_target[2][NUM_PWRCTRL_RATES];	/* Latest target for 2.4 and 5 Ghz */
	uint8 txpwr_est_Pout[2];			/* Latest estimate for 2.4 and 5 Ghz */
	uint8 txpwr_opo[NUM_PWRCTRL_RATES];		/* On G phy, OFDM power offset */
	uint8 txpwr_bphy_cck_max[NUM_PWRCTRL_RATES];	/* Max CCK power for this band (SROM) */
	uint8 txpwr_bphy_ofdm_max;			/* Max OFDM power for this band (SROM) */
	uint8 txpwr_aphy_max[NUM_PWRCTRL_RATES];	/* Max power for A band (SROM) */
	int8  txpwr_antgain[2];				/* Ant gain for each band - from SROM */
	uint8 txpwr_est_Pout_gofdm;			/* Pwr estimate for 2.4 OFDM */
} tx_power_legacy_t;

#define WL_TX_POWER_RATES_LEGACY    45
#define WL_TX_POWER_MCS20_FIRST         12
#define WL_TX_POWER_MCS20_NUM           16
#define WL_TX_POWER_MCS40_FIRST         28
#define WL_TX_POWER_MCS40_NUM           17

typedef struct {
	uint32 flags;
	chanspec_t chanspec;                 /* txpwr report for this channel */
	chanspec_t local_chanspec;           /* channel on which we are associated */
	uint8 local_max;                 /* local max according to the AP */
	uint8 local_constraint;              /* local constraint according to the AP */
	int8  antgain[2];                /* Ant gain for each band - from SROM */
	uint8 rf_cores;                  /* count of RF Cores being reported */
	uint8 est_Pout[4];                           /* Latest tx power out estimate per RF
							  * chain without adjustment
							  */
	uint8 est_Pout_cck;                          /* Latest CCK tx power out estimate */
	uint8 user_limit[WL_TX_POWER_RATES_LEGACY];  /* User limit */
	uint8 reg_limit[WL_TX_POWER_RATES_LEGACY];   /* Regulatory power limit */
	uint8 board_limit[WL_TX_POWER_RATES_LEGACY]; /* Max power board can support (SROM) */
	uint8 target[WL_TX_POWER_RATES_LEGACY];      /* Latest target power */
} tx_power_legacy2_t;

/* TX Power index defines */
#define WLC_NUM_RATES_CCK       WL_NUM_RATES_CCK
#define WLC_NUM_RATES_OFDM      WL_NUM_RATES_OFDM
#define WLC_NUM_RATES_MCS_1_STREAM  WL_NUM_RATES_MCS_1STREAM
#define WLC_NUM_RATES_MCS_2_STREAM  WL_NUM_RATES_MCS_1STREAM
#define WLC_NUM_RATES_MCS32     WL_NUM_RATES_MCS32
#define WL_TX_POWER_CCK_NUM     WL_NUM_RATES_CCK
#define WL_TX_POWER_OFDM_NUM        WL_NUM_RATES_OFDM
#define WL_TX_POWER_MCS_1_STREAM_NUM    WL_NUM_RATES_MCS_1STREAM
#define WL_TX_POWER_MCS_2_STREAM_NUM    WL_NUM_RATES_MCS_1STREAM
#define WL_TX_POWER_MCS_32_NUM      WL_NUM_RATES_MCS32

#define WL_NUM_2x2_ELEMENTS		4
#define WL_NUM_3x3_ELEMENTS		6

typedef struct {
	uint16 ver;				/* version of this struct */
	uint16 len;				/* length in bytes of this structure */
	uint32 flags;
	chanspec_t chanspec;			/* txpwr report for this channel */
	chanspec_t local_chanspec;		/* channel on which we are associated */
	uint32     buflen;			/* ppr buffer length */
	uint8      pprbuf[1];			/* Latest target power buffer */
} wl_txppr_t;

#define WL_TXPPR_VERSION	1
#define WL_TXPPR_LENGTH	(sizeof(wl_txppr_t))
#define TX_POWER_T_VERSION	45
/* number of ppr serialization buffers, it should be reg, board and target */
#define WL_TXPPR_SER_BUF_NUM	(3)

typedef struct chanspec_txpwr_max {
	chanspec_t chanspec;   /* chanspec */
	uint8 txpwr_max;       /* max txpwr in all the rates */
	uint8 padding;
} chanspec_txpwr_max_t;

typedef struct  wl_chanspec_txpwr_max {
	uint16 ver;			/* version of this struct */
	uint16 len;			/* length in bytes of this structure */
	uint32 count;		/* number of elements of (chanspec, txpwr_max) pair */
	chanspec_txpwr_max_t txpwr[1];	/* array of (chanspec, max_txpwr) pair */
} wl_chanspec_txpwr_max_t;

#define WL_CHANSPEC_TXPWR_MAX_VER	1
#define WL_CHANSPEC_TXPWR_MAX_LEN	(sizeof(wl_chanspec_txpwr_max_t))

typedef struct tx_inst_power {
	uint8 txpwr_est_Pout[2];			/* Latest estimate for 2.4 and 5 Ghz */
	uint8 txpwr_est_Pout_gofdm;			/* Pwr estimate for 2.4 OFDM */
} tx_inst_power_t;

#define WL_NUM_TXCHAIN_MAX	4
typedef struct wl_txchain_pwr_offsets {
	int8 offset[WL_NUM_TXCHAIN_MAX];	/* quarter dBm signed offset for each chain */
} wl_txchain_pwr_offsets_t;

/*
 * Join preference iovar value is an array of tuples. Each tuple has a one-byte type,
 * a one-byte length, and a variable length value.  RSSI type tuple must be present
 * in the array.
 *
 * Types are defined in "join preference types" section.
 *
 * Length is the value size in octets. It is reserved for WL_JOIN_PREF_WPA type tuple
 * and must be set to zero.
 *
 * Values are defined below.
 *
 * 1. RSSI - 2 octets
 * offset 0: reserved
 * offset 1: reserved
 *
 * 2. WPA - 2 + 12 * n octets (n is # tuples defined below)
 * offset 0: reserved
 * offset 1: # of tuples
 * offset 2: tuple 1
 * offset 14: tuple 2
 * ...
 * offset 2 + 12 * (n - 1) octets: tuple n
 *
 * struct wpa_cfg_tuple {
 *   uint8 akm[DOT11_OUI_LEN+1];     akm suite
 *   uint8 ucipher[DOT11_OUI_LEN+1]; unicast cipher suite
 *   uint8 mcipher[DOT11_OUI_LEN+1]; multicast cipher suite
 * };
 *
 * multicast cipher suite can be specified as a specific cipher suite or WL_WPA_ACP_MCS_ANY.
 *
 * 3. BAND - 2 octets
 * offset 0: reserved
 * offset 1: see "band preference" and "band types"
 *
 * 4. BAND RSSI - 2 octets
 * offset 0: band types
 * offset 1: +ve RSSI boost value in dB
 */

struct tsinfo_arg {
	uint8 octets[3];
};
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

#define RATE_CCK_1MBPS 0
#define RATE_CCK_2MBPS 1
#define RATE_CCK_5_5MBPS 2
#define RATE_CCK_11MBPS 3

#define RATE_LEGACY_OFDM_6MBPS 0
#define RATE_LEGACY_OFDM_9MBPS 1
#define RATE_LEGACY_OFDM_12MBPS 2
#define RATE_LEGACY_OFDM_18MBPS 3
#define RATE_LEGACY_OFDM_24MBPS 4
#define RATE_LEGACY_OFDM_36MBPS 5
#define RATE_LEGACY_OFDM_48MBPS 6
#define RATE_LEGACY_OFDM_54MBPS 7

#define WL_BSSTRANS_RSSI_RATE_MAP_VERSION 1

typedef struct wl_bsstrans_rssi {
	int8 rssi_2g;	/* RSSI in dbm for 2.4 G */
	int8 rssi_5g;	/* RSSI in dbm for 5G, unused for cck */
} wl_bsstrans_rssi_t;

#define RSSI_RATE_MAP_MAX_STREAMS 4	/* max streams supported */

/* RSSI to rate mapping, all 20Mhz, no SGI */
typedef struct wl_bsstrans_rssi_rate_map {
	uint16 ver;
	uint16 len; /* length of entire structure */
	wl_bsstrans_rssi_t cck[WL_NUM_RATES_CCK]; /* 2.4G only */
	wl_bsstrans_rssi_t ofdm[WL_NUM_RATES_OFDM]; /* 6 to 54mbps */
	wl_bsstrans_rssi_t phy_n[RSSI_RATE_MAP_MAX_STREAMS][WL_NUM_RATES_MCS_1STREAM]; /* MCS0-7 */
	wl_bsstrans_rssi_t phy_ac[RSSI_RATE_MAP_MAX_STREAMS][WL_NUM_RATES_VHT]; /* MCS0-9 */
} wl_bsstrans_rssi_rate_map_t;

#define WL_BSSTRANS_ROAMTHROTTLE_VERSION 1

/* Configure number of scans allowed per throttle period */
typedef struct wl_bsstrans_roamthrottle {
	uint16 ver;
	uint16 period;
	uint16 scans_allowed;
} wl_bsstrans_roamthrottle_t;

#define	NFIFO			6	/* # tx/rx fifopairs */
#define NREINITREASONCOUNT	8
#define REINITREASONIDX(_x)	(((_x) < NREINITREASONCOUNT) ? (_x) : 0)

#define	WL_CNT_T_VERSION	10	/* current version of wl_cnt_t struct */

typedef struct {
	uint16	version;	/* see definition of WL_CNT_T_VERSION */
	uint16	length;		/* length of entire structure */

	/* transmit stat counters */
	uint32	txframe;	/* tx data frames */
	uint32	txbyte;		/* tx data bytes */
	uint32	txretrans;	/* tx mac retransmits */
	uint32	txerror;	/* tx data errors (derived: sum of others) */
	uint32	txctl;		/* tx management frames */
	uint32	txprshort;	/* tx short preamble frames */
	uint32	txserr;		/* tx status errors */
	uint32	txnobuf;	/* tx out of buffers errors */
	uint32	txnoassoc;	/* tx discard because we're not associated */
	uint32	txrunt;		/* tx runt frames */
	uint32	txchit;		/* tx header cache hit (fastpath) */
	uint32	txcmiss;	/* tx header cache miss (slowpath) */

	/* transmit chip error counters */
	uint32	txuflo;		/* tx fifo underflows */
	uint32	txphyerr;	/* tx phy errors (indicated in tx status) */
	uint32	txphycrs;

	/* receive stat counters */
	uint32	rxframe;	/* rx data frames */
	uint32	rxbyte;		/* rx data bytes */
	uint32	rxerror;	/* rx data errors (derived: sum of others) */
	uint32	rxctl;		/* rx management frames */
	uint32	rxnobuf;	/* rx out of buffers errors */
	uint32	rxnondata;	/* rx non data frames in the data channel errors */
	uint32	rxbadds;	/* rx bad DS errors */
	uint32	rxbadcm;	/* rx bad control or management frames */
	uint32	rxfragerr;	/* rx fragmentation errors */
	uint32	rxrunt;		/* rx runt frames */
	uint32	rxgiant;	/* rx giant frames */
	uint32	rxnoscb;	/* rx no scb error */
	uint32	rxbadproto;	/* rx invalid frames */
	uint32	rxbadsrcmac;	/* rx frames with Invalid Src Mac */
	uint32	rxbadda;	/* rx frames tossed for invalid da */
	uint32	rxfilter;	/* rx frames filtered out */

	/* receive chip error counters */
	uint32	rxoflo;		/* rx fifo overflow errors */
	uint32	rxuflo[NFIFO];	/* rx dma descriptor underflow errors */

	uint32	d11cnt_txrts_off;	/* d11cnt txrts value when reset d11cnt */
	uint32	d11cnt_rxcrc_off;	/* d11cnt rxcrc value when reset d11cnt */
	uint32	d11cnt_txnocts_off;	/* d11cnt txnocts value when reset d11cnt */

	/* misc counters */
	uint32	dmade;		/* tx/rx dma descriptor errors */
	uint32	dmada;		/* tx/rx dma data errors */
	uint32	dmape;		/* tx/rx dma descriptor protocol errors */
	uint32	reset;		/* reset count */
	uint32	tbtt;		/* cnts the TBTT int's */
	uint32	txdmawar;
	uint32	pkt_callback_reg_fail;	/* callbacks register failure */

	/* MAC counters: 32-bit version of d11.h's macstat_t */
	uint32	txallfrm;	/* total number of frames sent, incl. Data, ACK, RTS, CTS,
				 * Control Management (includes retransmissions)
				 */
	uint32	txrtsfrm;	/* number of RTS sent out by the MAC */
	uint32	txctsfrm;	/* number of CTS sent out by the MAC */
	uint32	txackfrm;	/* number of ACK frames sent out */
	uint32	txdnlfrm;	/* Not used */
	uint32	txbcnfrm;	/* beacons transmitted */
	uint32	txfunfl[6];	/* per-fifo tx underflows */
	uint32	rxtoolate;	/* receive too late */
	uint32  txfbw;		/* transmit at fallback bw (dynamic bw) */
	uint32	txtplunfl;	/* Template underflows (mac was too slow to transmit ACK/CTS
				 * or BCN)
				 */
	uint32	txphyerror;	/* Transmit phy error, type of error is reported in tx-status for
				 * driver enqueued frames
				 */
	uint32	rxfrmtoolong;	/* Received frame longer than legal limit (2346 bytes) */
	uint32	rxfrmtooshrt;	/* Received frame did not contain enough bytes for its frame type */
	uint32	rxinvmachdr;	/* Either the protocol version != 0 or frame type not
				 * data/control/management
				 */
	uint32	rxbadfcs;	/* number of frames for which the CRC check failed in the MAC */
	uint32	rxbadplcp;	/* parity check of the PLCP header failed */
	uint32	rxcrsglitch;	/* PHY was able to correlate the preamble but not the header */
	uint32	rxstrt;		/* Number of received frames with a good PLCP
				 * (i.e. passing parity check)
				 */
	uint32	rxdfrmucastmbss; /* Number of received DATA frames with good FCS and matching RA */
	uint32	rxmfrmucastmbss; /* number of received mgmt frames with good FCS and matching RA */
	uint32	rxcfrmucast;	/* number of received CNTRL frames with good FCS and matching RA */
	uint32	rxrtsucast;	/* number of unicast RTS addressed to the MAC (good FCS) */
	uint32	rxctsucast;	/* number of unicast CTS addressed to the MAC (good FCS) */
	uint32	rxackucast;	/* number of ucast ACKS received (good FCS) */
	uint32	rxdfrmocast;	/* number of received DATA frames (good FCS and not matching RA) */
	uint32	rxmfrmocast;	/* number of received MGMT frames (good FCS and not matching RA) */
	uint32	rxcfrmocast;	/* number of received CNTRL frame (good FCS and not matching RA) */
	uint32	rxrtsocast;	/* number of received RTS not addressed to the MAC */
	uint32	rxctsocast;	/* number of received CTS not addressed to the MAC */
	uint32	rxdfrmmcast;	/* number of RX Data multicast frames received by the MAC */
	uint32	rxmfrmmcast;	/* number of RX Management multicast frames received by the MAC */
	uint32	rxcfrmmcast;	/* number of RX Control multicast frames received by the MAC
				 * (unlikely to see these)
				 */
	uint32	rxbeaconmbss;	/* beacons received from member of BSS */
	uint32	rxdfrmucastobss; /* number of unicast frames addressed to the MAC from
				  * other BSS (WDS FRAME)
				  */
	uint32	rxbeaconobss;	/* beacons received from other BSS */
	uint32	rxrsptmout;	/* Number of response timeouts for transmitted frames
				 * expecting a response
				 */
	uint32	bcntxcancl;	/* transmit beacons canceled due to receipt of beacon (IBSS) */
	uint32	rxf0ovfl;	/* Number of receive fifo 0 overflows */
	uint32	rxf1ovfl;	/* Number of receive fifo 1 overflows (obsolete) */
	uint32	rxf2ovfl;	/* Number of receive fifo 2 overflows (obsolete) */
	uint32	txsfovfl;	/* Number of transmit status fifo overflows (obsolete) */
	uint32	pmqovfl;	/* Number of PMQ overflows */
	uint32	rxcgprqfrm;	/* Number of received Probe requests that made it into
				 * the PRQ fifo
				 */
	uint32	rxcgprsqovfl;	/* Rx Probe Request Que overflow in the AP */
	uint32	txcgprsfail;	/* Tx Probe Response Fail. AP sent probe response but did
				 * not get ACK
				 */
	uint32	txcgprssuc;	/* Tx Probe Response Success (ACK was received) */
	uint32	prs_timeout;	/* Number of probe requests that were dropped from the PRQ
				 * fifo because a probe response could not be sent out within
				 * the time limit defined in M_PRS_MAXTIME
				 */
	uint32	rxnack;		/* obsolete */
	uint32	frmscons;	/* obsolete */
	uint32  txnack;		/* obsolete */
	uint32	rxback;		/* blockack rxcnt */
	uint32	txback;		/* blockack txcnt */

	/* 802.11 MIB counters, pp. 614 of 802.11 reaff doc. */
	uint32	txfrag;		/* dot11TransmittedFragmentCount */
	uint32	txmulti;	/* dot11MulticastTransmittedFrameCount */
	uint32	txfail;		/* dot11FailedCount */
	uint32	txretry;	/* dot11RetryCount */
	uint32	txretrie;	/* dot11MultipleRetryCount */
	uint32	rxdup;		/* dot11FrameduplicateCount */
	uint32	txrts;		/* dot11RTSSuccessCount */
	uint32	txnocts;	/* dot11RTSFailureCount */
	uint32	txnoack;	/* dot11ACKFailureCount */
	uint32	rxfrag;		/* dot11ReceivedFragmentCount */
	uint32	rxmulti;	/* dot11MulticastReceivedFrameCount */
	uint32	rxcrc;		/* dot11FCSErrorCount */
	uint32	txfrmsnt;	/* dot11TransmittedFrameCount (bogus MIB?) */
	uint32	rxundec;	/* dot11WEPUndecryptableCount */

	/* WPA2 counters (see rxundec for DecryptFailureCount) */
	uint32	tkipmicfaill;	/* TKIPLocalMICFailures */
	uint32	tkipcntrmsr;	/* TKIPCounterMeasuresInvoked */
	uint32	tkipreplay;	/* TKIPReplays */
	uint32	ccmpfmterr;	/* CCMPFormatErrors */
	uint32	ccmpreplay;	/* CCMPReplays */
	uint32	ccmpundec;	/* CCMPDecryptErrors */
	uint32	fourwayfail;	/* FourWayHandshakeFailures */
	uint32	wepundec;	/* dot11WEPUndecryptableCount */
	uint32	wepicverr;	/* dot11WEPICVErrorCount */
	uint32	decsuccess;	/* DecryptSuccessCount */
	uint32	tkipicverr;	/* TKIPICVErrorCount */
	uint32	wepexcluded;	/* dot11WEPExcludedCount */

	uint32	txchanrej;	/* Tx frames suppressed due to channel rejection */
	uint32	psmwds;		/* Count PSM watchdogs */
	uint32	phywatchdog;	/* Count Phy watchdogs (triggered by ucode) */

	/* MBSS counters, AP only */
	uint32	prq_entries_handled;	/* PRQ entries read in */
	uint32	prq_undirected_entries;	/*    which were bcast bss & ssid */
	uint32	prq_bad_entries;	/*    which could not be translated to info */
	uint32	atim_suppress_count;	/* TX suppressions on ATIM fifo */
	uint32	bcn_template_not_ready;	/* Template marked in use on send bcn ... */
	uint32	bcn_template_not_ready_done; /* ...but "DMA done" interrupt rcvd */
	uint32	late_tbtt_dpc;	/* TBTT DPC did not happen in time */

	/* per-rate receive stat counters */
	uint32  rx1mbps;	/* packets rx at 1Mbps */
	uint32  rx2mbps;	/* packets rx at 2Mbps */
	uint32  rx5mbps5;	/* packets rx at 5.5Mbps */
	uint32  rx6mbps;	/* packets rx at 6Mbps */
	uint32  rx9mbps;	/* packets rx at 9Mbps */
	uint32  rx11mbps;	/* packets rx at 11Mbps */
	uint32  rx12mbps;	/* packets rx at 12Mbps */
	uint32  rx18mbps;	/* packets rx at 18Mbps */
	uint32  rx24mbps;	/* packets rx at 24Mbps */
	uint32  rx36mbps;	/* packets rx at 36Mbps */
	uint32  rx48mbps;	/* packets rx at 48Mbps */
	uint32  rx54mbps;	/* packets rx at 54Mbps */
	uint32  rx108mbps;	/* packets rx at 108mbps */
	uint32  rx162mbps;	/* packets rx at 162mbps */
	uint32  rx216mbps;	/* packets rx at 216 mbps */
	uint32  rx270mbps;	/* packets rx at 270 mbps */
	uint32  rx324mbps;	/* packets rx at 324 mbps */
	uint32  rx378mbps;	/* packets rx at 378 mbps */
	uint32  rx432mbps;	/* packets rx at 432 mbps */
	uint32  rx486mbps;	/* packets rx at 486 mbps */
	uint32  rx540mbps;	/* packets rx at 540 mbps */

	/* pkteng rx frame stats */
	uint32	pktengrxducast; /* unicast frames rxed by the pkteng code */
	uint32	pktengrxdmcast; /* multicast frames rxed by the pkteng code */

	uint32	rfdisable;	/* count of radio disables */
	uint32	bphy_rxcrsglitch;	/* PHY count of bphy glitches */
	uint32  bphy_badplcp;

	uint32	txexptime;	/* Tx frames suppressed due to timer expiration */

	uint32	txmpdu_sgi;	/* count for sgi transmit */
	uint32	rxmpdu_sgi;	/* count for sgi received */
	uint32	txmpdu_stbc;	/* count for stbc transmit */
	uint32	rxmpdu_stbc;	/* count for stbc received */

	uint32	rxundec_mcst;	/* dot11WEPUndecryptableCount */

	/* WPA2 counters (see rxundec for DecryptFailureCount) */
	uint32	tkipmicfaill_mcst;	/* TKIPLocalMICFailures */
	uint32	tkipcntrmsr_mcst;	/* TKIPCounterMeasuresInvoked */
	uint32	tkipreplay_mcst;	/* TKIPReplays */
	uint32	ccmpfmterr_mcst;	/* CCMPFormatErrors */
	uint32	ccmpreplay_mcst;	/* CCMPReplays */
	uint32	ccmpundec_mcst;	/* CCMPDecryptErrors */
	uint32	fourwayfail_mcst;	/* FourWayHandshakeFailures */
	uint32	wepundec_mcst;	/* dot11WEPUndecryptableCount */
	uint32	wepicverr_mcst;	/* dot11WEPICVErrorCount */
	uint32	decsuccess_mcst;	/* DecryptSuccessCount */
	uint32	tkipicverr_mcst;	/* TKIPICVErrorCount */
	uint32	wepexcluded_mcst;	/* dot11WEPExcludedCount */

	uint32	dma_hang;	/* count for dma hang */
	uint32	reinit;		/* count for reinit */

	uint32  pstatxucast;	/* count of ucast frames xmitted on all psta assoc */
	uint32  pstatxnoassoc;	/* count of txnoassoc frames xmitted on all psta assoc */
	uint32  pstarxucast;	/* count of ucast frames received on all psta assoc */
	uint32  pstarxbcmc;	/* count of bcmc frames received on all psta */
	uint32  pstatxbcmc;	/* count of bcmc frames transmitted on all psta */

	uint32  cso_passthrough; /* hw cso required but passthrough */
	uint32	cso_normal;	/* hw cso hdr for normal process */
	uint32	chained;	/* number of frames chained */
	uint32	chainedsz1;	/* number of chain size 1 frames */
	uint32	unchained;	/* number of frames not chained */
	uint32	maxchainsz;	/* max chain size so far */
	uint32	currchainsz;	/* current chain size */
	uint32	rxdrop20s;	/* drop secondary cnt */
	uint32	pciereset;	/* Secondary Bus Reset issued by driver */
	uint32	cfgrestore;	/* configspace restore by driver */
	uint32	reinitreason[NREINITREASONCOUNT]; /* reinitreason counters; 0: Unknown reason */
	uint32  rxrtry;		/* num of received packets with retry bit on */
} wl_cnt_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL
typedef struct {
	uint16  version;    /* see definition of WL_CNT_T_VERSION */
	uint16  length;     /* length of entire structure */

	/* transmit stat counters */
	uint32  txframe;    /* tx data frames */
	uint32  txbyte;     /* tx data bytes */
	uint32  txretrans;  /* tx mac retransmits */
	uint32  txerror;    /* tx data errors (derived: sum of others) */
	uint32  txctl;      /* tx management frames */
	uint32  txprshort;  /* tx short preamble frames */
	uint32  txserr;     /* tx status errors */
	uint32  txnobuf;    /* tx out of buffers errors */
	uint32  txnoassoc;  /* tx discard because we're not associated */
	uint32  txrunt;     /* tx runt frames */
	uint32  txchit;     /* tx header cache hit (fastpath) */
	uint32  txcmiss;    /* tx header cache miss (slowpath) */

	/* transmit chip error counters */
	uint32  txuflo;     /* tx fifo underflows */
	uint32  txphyerr;   /* tx phy errors (indicated in tx status) */
	uint32  txphycrs;

	/* receive stat counters */
	uint32  rxframe;    /* rx data frames */
	uint32  rxbyte;     /* rx data bytes */
	uint32  rxerror;    /* rx data errors (derived: sum of others) */
	uint32  rxctl;      /* rx management frames */
	uint32  rxnobuf;    /* rx out of buffers errors */
	uint32  rxnondata;  /* rx non data frames in the data channel errors */
	uint32  rxbadds;    /* rx bad DS errors */
	uint32  rxbadcm;    /* rx bad control or management frames */
	uint32  rxfragerr;  /* rx fragmentation errors */
	uint32  rxrunt;     /* rx runt frames */
	uint32  rxgiant;    /* rx giant frames */
	uint32  rxnoscb;    /* rx no scb error */
	uint32  rxbadproto; /* rx invalid frames */
	uint32  rxbadsrcmac;    /* rx frames with Invalid Src Mac */
	uint32  rxbadda;    /* rx frames tossed for invalid da */
	uint32  rxfilter;   /* rx frames filtered out */

	/* receive chip error counters */
	uint32  rxoflo;     /* rx fifo overflow errors */
	uint32  rxuflo[NFIFO];  /* rx dma descriptor underflow errors */

	uint32  d11cnt_txrts_off;   /* d11cnt txrts value when reset d11cnt */
	uint32  d11cnt_rxcrc_off;   /* d11cnt rxcrc value when reset d11cnt */
	uint32  d11cnt_txnocts_off; /* d11cnt txnocts value when reset d11cnt */

	/* misc counters */
	uint32  dmade;      /* tx/rx dma descriptor errors */
	uint32  dmada;      /* tx/rx dma data errors */
	uint32  dmape;      /* tx/rx dma descriptor protocol errors */
	uint32  reset;      /* reset count */
	uint32  tbtt;       /* cnts the TBTT int's */
	uint32  txdmawar;
	uint32  pkt_callback_reg_fail;  /* callbacks register failure */

	/* MAC counters: 32-bit version of d11.h's macstat_t */
	uint32  txallfrm;   /* total number of frames sent, incl. Data, ACK, RTS, CTS,
			     * Control Management (includes retransmissions)
			     */
	uint32  txrtsfrm;   /* number of RTS sent out by the MAC */
	uint32  txctsfrm;   /* number of CTS sent out by the MAC */
	uint32  txackfrm;   /* number of ACK frames sent out */
	uint32  txdnlfrm;   /* Not used */
	uint32  txbcnfrm;   /* beacons transmitted */
	uint32  txfunfl[6]; /* per-fifo tx underflows */
	uint32	rxtoolate;	/* receive too late */
	uint32  txfbw;	    /* transmit at fallback bw (dynamic bw) */
	uint32  txtplunfl;  /* Template underflows (mac was too slow to transmit ACK/CTS
			     * or BCN)
			     */
	uint32  txphyerror; /* Transmit phy error, type of error is reported in tx-status for
			     * driver enqueued frames
			     */
	uint32  rxfrmtoolong;   /* Received frame longer than legal limit (2346 bytes) */
	uint32  rxfrmtooshrt;   /* Received frame did not contain enough bytes for its frame type */
	uint32  rxinvmachdr;    /* Either the protocol version != 0 or frame type not
				 * data/control/management
			   */
	uint32  rxbadfcs;   /* number of frames for which the CRC check failed in the MAC */
	uint32  rxbadplcp;  /* parity check of the PLCP header failed */
	uint32  rxcrsglitch;    /* PHY was able to correlate the preamble but not the header */
	uint32  rxstrt;     /* Number of received frames with a good PLCP
			     * (i.e. passing parity check)
			     */
	uint32  rxdfrmucastmbss; /* Number of received DATA frames with good FCS and matching RA */
	uint32  rxmfrmucastmbss; /* number of received mgmt frames with good FCS and matching RA */
	uint32  rxcfrmucast;    /* number of received CNTRL frames with good FCS and matching RA */
	uint32  rxrtsucast; /* number of unicast RTS addressed to the MAC (good FCS) */
	uint32  rxctsucast; /* number of unicast CTS addressed to the MAC (good FCS) */
	uint32  rxackucast; /* number of ucast ACKS received (good FCS) */
	uint32  rxdfrmocast;    /* number of received DATA frames (good FCS and not matching RA) */
	uint32  rxmfrmocast;    /* number of received MGMT frames (good FCS and not matching RA) */
	uint32  rxcfrmocast;    /* number of received CNTRL frame (good FCS and not matching RA) */
	uint32  rxrtsocast; /* number of received RTS not addressed to the MAC */
	uint32  rxctsocast; /* number of received CTS not addressed to the MAC */
	uint32  rxdfrmmcast;    /* number of RX Data multicast frames received by the MAC */
	uint32  rxmfrmmcast;    /* number of RX Management multicast frames received by the MAC */
	uint32  rxcfrmmcast;    /* number of RX Control multicast frames received by the MAC
				 * (unlikely to see these)
				 */
	uint32  rxbeaconmbss;   /* beacons received from member of BSS */
	uint32  rxdfrmucastobss; /* number of unicast frames addressed to the MAC from
				  * other BSS (WDS FRAME)
				  */
	uint32  rxbeaconobss;   /* beacons received from other BSS */
	uint32  rxrsptmout; /* Number of response timeouts for transmitted frames
			     * expecting a response
			     */
	uint32  bcntxcancl; /* transmit beacons canceled due to receipt of beacon (IBSS) */
	uint32  rxf0ovfl;   /* Number of receive fifo 0 overflows */
	uint32  rxf1ovfl;   /* Number of receive fifo 1 overflows (obsolete) */
	uint32  rxf2ovfl;   /* Number of receive fifo 2 overflows (obsolete) */
	uint32  txsfovfl;   /* Number of transmit status fifo overflows (obsolete) */
	uint32  pmqovfl;    /* Number of PMQ overflows */
	uint32  rxcgprqfrm; /* Number of received Probe requests that made it into
			     * the PRQ fifo
			     */
	uint32  rxcgprsqovfl;   /* Rx Probe Request Que overflow in the AP */
	uint32  txcgprsfail;    /* Tx Probe Response Fail. AP sent probe response but did
				 * not get ACK
				 */
	uint32  txcgprssuc; /* Tx Probe Response Success (ACK was received) */
	uint32  prs_timeout;    /* Number of probe requests that were dropped from the PRQ
				 * fifo because a probe response could not be sent out within
				 * the time limit defined in M_PRS_MAXTIME
				 */
	uint32  rxnack;
	uint32  frmscons;
	uint32  txnack;		/* obsolete */
	uint32	rxback;		/* blockack rxcnt */
	uint32	txback;		/* blockack txcnt */

	/* 802.11 MIB counters, pp. 614 of 802.11 reaff doc. */
	uint32  txfrag;     /* dot11TransmittedFragmentCount */
	uint32  txmulti;    /* dot11MulticastTransmittedFrameCount */
	uint32  txfail;     /* dot11FailedCount */
	uint32  txretry;    /* dot11RetryCount */
	uint32  txretrie;   /* dot11MultipleRetryCount */
	uint32  rxdup;      /* dot11FrameduplicateCount */
	uint32  txrts;      /* dot11RTSSuccessCount */
	uint32  txnocts;    /* dot11RTSFailureCount */
	uint32  txnoack;    /* dot11ACKFailureCount */
	uint32  rxfrag;     /* dot11ReceivedFragmentCount */
	uint32  rxmulti;    /* dot11MulticastReceivedFrameCount */
	uint32  rxcrc;      /* dot11FCSErrorCount */
	uint32  txfrmsnt;   /* dot11TransmittedFrameCount (bogus MIB?) */
	uint32  rxundec;    /* dot11WEPUndecryptableCount */

	/* WPA2 counters (see rxundec for DecryptFailureCount) */
	uint32  tkipmicfaill;   /* TKIPLocalMICFailures */
	uint32  tkipcntrmsr;    /* TKIPCounterMeasuresInvoked */
	uint32  tkipreplay; /* TKIPReplays */
	uint32  ccmpfmterr; /* CCMPFormatErrors */
	uint32  ccmpreplay; /* CCMPReplays */
	uint32  ccmpundec;  /* CCMPDecryptErrors */
	uint32  fourwayfail;    /* FourWayHandshakeFailures */
	uint32  wepundec;   /* dot11WEPUndecryptableCount */
	uint32  wepicverr;  /* dot11WEPICVErrorCount */
	uint32  decsuccess; /* DecryptSuccessCount */
	uint32  tkipicverr; /* TKIPICVErrorCount */
	uint32  wepexcluded;    /* dot11WEPExcludedCount */

	uint32  rxundec_mcst;   /* dot11WEPUndecryptableCount */

	/* WPA2 counters (see rxundec for DecryptFailureCount) */
	uint32  tkipmicfaill_mcst;  /* TKIPLocalMICFailures */
	uint32  tkipcntrmsr_mcst;   /* TKIPCounterMeasuresInvoked */
	uint32  tkipreplay_mcst;    /* TKIPReplays */
	uint32  ccmpfmterr_mcst;    /* CCMPFormatErrors */
	uint32  ccmpreplay_mcst;    /* CCMPReplays */
	uint32  ccmpundec_mcst; /* CCMPDecryptErrors */
	uint32  fourwayfail_mcst;   /* FourWayHandshakeFailures */
	uint32  wepundec_mcst;  /* dot11WEPUndecryptableCount */
	uint32  wepicverr_mcst; /* dot11WEPICVErrorCount */
	uint32  decsuccess_mcst;    /* DecryptSuccessCount */
	uint32  tkipicverr_mcst;    /* TKIPICVErrorCount */
	uint32  wepexcluded_mcst;   /* dot11WEPExcludedCount */

	uint32  txchanrej;  /* Tx frames suppressed due to channel rejection */
	uint32  txexptime;  /* Tx frames suppressed due to timer expiration */
	uint32  psmwds;     /* Count PSM watchdogs */
	uint32  phywatchdog;    /* Count Phy watchdogs (triggered by ucode) */

	/* MBSS counters, AP only */
	uint32  prq_entries_handled;    /* PRQ entries read in */
	uint32  prq_undirected_entries; /*    which were bcast bss & ssid */
	uint32  prq_bad_entries;    /*    which could not be translated to info */
	uint32  atim_suppress_count;    /* TX suppressions on ATIM fifo */
	uint32  bcn_template_not_ready; /* Template marked in use on send bcn ... */
	uint32  bcn_template_not_ready_done; /* ...but "DMA done" interrupt rcvd */
	uint32  late_tbtt_dpc;  /* TBTT DPC did not happen in time */

	/* per-rate receive stat counters */
	uint32  rx1mbps;    /* packets rx at 1Mbps */
	uint32  rx2mbps;    /* packets rx at 2Mbps */
	uint32  rx5mbps5;   /* packets rx at 5.5Mbps */
	uint32  rx6mbps;    /* packets rx at 6Mbps */
	uint32  rx9mbps;    /* packets rx at 9Mbps */
	uint32  rx11mbps;   /* packets rx at 11Mbps */
	uint32  rx12mbps;   /* packets rx at 12Mbps */
	uint32  rx18mbps;   /* packets rx at 18Mbps */
	uint32  rx24mbps;   /* packets rx at 24Mbps */
	uint32  rx36mbps;   /* packets rx at 36Mbps */
	uint32  rx48mbps;   /* packets rx at 48Mbps */
	uint32  rx54mbps;   /* packets rx at 54Mbps */
	uint32  rx108mbps;  /* packets rx at 108mbps */
	uint32  rx162mbps;  /* packets rx at 162mbps */
	uint32  rx216mbps;  /* packets rx at 216 mbps */
	uint32  rx270mbps;  /* packets rx at 270 mbps */
	uint32  rx324mbps;  /* packets rx at 324 mbps */
	uint32  rx378mbps;  /* packets rx at 378 mbps */
	uint32  rx432mbps;  /* packets rx at 432 mbps */
	uint32  rx486mbps;  /* packets rx at 486 mbps */
	uint32  rx540mbps;  /* packets rx at 540 mbps */

	/* pkteng rx frame stats */
	uint32  pktengrxducast; /* unicast frames rxed by the pkteng code */
	uint32  pktengrxdmcast; /* multicast frames rxed by the pkteng code */

	uint32  rfdisable;  /* count of radio disables */
	uint32  bphy_rxcrsglitch;   /* PHY count of bphy glitches */
	uint32  bphy_badplcp;

	uint32  txmpdu_sgi; /* count for sgi transmit */
	uint32  rxmpdu_sgi; /* count for sgi received */
	uint32  txmpdu_stbc;    /* count for stbc transmit */
	uint32  rxmpdu_stbc;    /* count for stbc received */

	uint32	rxdrop20s;	/* drop secondary cnt */

} wl_cnt_ver_six_t;

#define	WL_DELTA_STATS_T_VERSION	2	/* current version of wl_delta_stats_t struct */

typedef struct {
	uint16 version;     /* see definition of WL_DELTA_STATS_T_VERSION */
	uint16 length;      /* length of entire structure */

	/* transmit stat counters */
	uint32 txframe;     /* tx data frames */
	uint32 txbyte;      /* tx data bytes */
	uint32 txretrans;   /* tx mac retransmits */
	uint32 txfail;      /* tx failures */

	/* receive stat counters */
	uint32 rxframe;     /* rx data frames */
	uint32 rxbyte;      /* rx data bytes */

	/* per-rate receive stat counters */
	uint32  rx1mbps;	/* packets rx at 1Mbps */
	uint32  rx2mbps;	/* packets rx at 2Mbps */
	uint32  rx5mbps5;	/* packets rx at 5.5Mbps */
	uint32  rx6mbps;	/* packets rx at 6Mbps */
	uint32  rx9mbps;	/* packets rx at 9Mbps */
	uint32  rx11mbps;	/* packets rx at 11Mbps */
	uint32  rx12mbps;	/* packets rx at 12Mbps */
	uint32  rx18mbps;	/* packets rx at 18Mbps */
	uint32  rx24mbps;	/* packets rx at 24Mbps */
	uint32  rx36mbps;	/* packets rx at 36Mbps */
	uint32  rx48mbps;	/* packets rx at 48Mbps */
	uint32  rx54mbps;	/* packets rx at 54Mbps */
	uint32  rx108mbps;	/* packets rx at 108mbps */
	uint32  rx162mbps;	/* packets rx at 162mbps */
	uint32  rx216mbps;	/* packets rx at 216 mbps */
	uint32  rx270mbps;	/* packets rx at 270 mbps */
	uint32  rx324mbps;	/* packets rx at 324 mbps */
	uint32  rx378mbps;	/* packets rx at 378 mbps */
	uint32  rx432mbps;	/* packets rx at 432 mbps */
	uint32  rx486mbps;	/* packets rx at 486 mbps */
	uint32  rx540mbps;	/* packets rx at 540 mbps */

	/* phy stats */
	uint32 rxbadplcp;
	uint32 rxcrsglitch;
	uint32 bphy_rxcrsglitch;
	uint32 bphy_badplcp;

} wl_delta_stats_t;
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

typedef struct {
	uint32 packets;
	uint32 bytes;
} wl_traffic_stats_t;

typedef struct {
	uint16	version;	/* see definition of WL_WME_CNT_VERSION */
	uint16	length;		/* length of entire structure */

	wl_traffic_stats_t tx[AC_COUNT];	/* Packets transmitted */
	wl_traffic_stats_t tx_failed[AC_COUNT];	/* Packets dropped or failed to transmit */
	wl_traffic_stats_t rx[AC_COUNT];	/* Packets received */
	wl_traffic_stats_t rx_failed[AC_COUNT];	/* Packets failed to receive */

	wl_traffic_stats_t forward[AC_COUNT];	/* Packets forwarded by AP */

	wl_traffic_stats_t tx_expired[AC_COUNT];	/* packets dropped due to lifetime expiry */

} wl_wme_cnt_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL
struct wl_msglevel2 {
	uint32 low;
	uint32 high;
};

typedef struct wl_mkeep_alive_pkt {
	uint16	version; /* Version for mkeep_alive */
	uint16	length; /* length of fixed parameters in the structure */
	uint32	period_msec;
	uint16	len_bytes;
	uint8	keep_alive_id; /* 0 - 3 for N = 4 */
	uint8	data[1];
} wl_mkeep_alive_pkt_t;

#define WL_MKEEP_ALIVE_VERSION		1
#define WL_MKEEP_ALIVE_FIXED_LEN	OFFSETOF(wl_mkeep_alive_pkt_t, data)
#define WL_MKEEP_ALIVE_PRECISION	500

/* TCP Keep-Alive conn struct */
typedef struct wl_mtcpkeep_alive_conn_pkt {
	struct ether_addr saddr;		/* src mac address */
	struct ether_addr daddr;		/* dst mac address */
	struct ipv4_addr sipaddr;		/* source IP addr */
	struct ipv4_addr dipaddr;		/* dest IP addr */
	uint16 sport;				/* src port */
	uint16 dport;				/* dest port */
	uint32 seq;				/* seq number */
	uint32 ack;				/* ACK number */
	uint16 tcpwin;				/* TCP window */
} wl_mtcpkeep_alive_conn_pkt_t;

/* TCP Keep-Alive interval struct */
typedef struct wl_mtcpkeep_alive_timers_pkt {
	uint16 interval;		/* interval timer */
	uint16 retry_interval;		/* retry_interval timer */
	uint16 retry_count;		/* retry_count */
} wl_mtcpkeep_alive_timers_pkt_t;

typedef struct wake_info {
	uint32 wake_reason;
	uint32 wake_info_len;		/* size of packet */
	uchar  packet[1];
} wake_info_t;

typedef struct wake_pkt {
	uint32 wake_pkt_len;		/* size of packet */
	uchar  packet[1];
} wake_pkt_t;


#define WL_MTCPKEEP_ALIVE_VERSION		1

#ifdef WLBA

#define WLC_BA_CNT_VERSION  1   /* current version of wlc_ba_cnt_t */

/* block ack related stats */
typedef struct wlc_ba_cnt {
	uint16  version;    /* WLC_BA_CNT_VERSION */
	uint16  length;     /* length of entire structure */

	/* transmit stat counters */
	uint32 txpdu;       /* pdus sent */
	uint32 txsdu;       /* sdus sent */
	uint32 txfc;        /* tx side flow controlled packets */
	uint32 txfci;       /* tx side flow control initiated */
	uint32 txretrans;   /* retransmitted pdus */
	uint32 txbatimer;   /* ba resend due to timer */
	uint32 txdrop;      /* dropped packets */
	uint32 txaddbareq;  /* addba req sent */
	uint32 txaddbaresp; /* addba resp sent */
	uint32 txdelba;     /* delba sent */
	uint32 txba;        /* ba sent */
	uint32 txbar;       /* bar sent */
	uint32 txpad[4];    /* future */

	/* receive side counters */
	uint32 rxpdu;       /* pdus recd */
	uint32 rxqed;       /* pdus buffered before sending up */
	uint32 rxdup;       /* duplicate pdus */
	uint32 rxnobuf;     /* pdus discarded due to no buf */
	uint32 rxaddbareq;  /* addba req recd */
	uint32 rxaddbaresp; /* addba resp recd */
	uint32 rxdelba;     /* delba recd */
	uint32 rxba;        /* ba recd */
	uint32 rxbar;       /* bar recd */
	uint32 rxinvba;     /* invalid ba recd */
	uint32 rxbaholes;   /* ba recd with holes */
	uint32 rxunexp;     /* unexpected packets */
	uint32 rxpad[4];    /* future */
} wlc_ba_cnt_t;
#endif /* WLBA */

/* structure for per-tid ampdu control */
struct ampdu_tid_control {
	uint8 tid;			/* tid */
	uint8 enable;			/* enable/disable */
};

/* struct for ampdu tx aggregation control */
struct ampdu_cfg_txaggr {
	uint8 control_all_tid;	/* enable/disable for all tid */
	uint8 enable;			/* enable/disable */
	struct ampdu_tid_control control[NUMPRIO]; /* tid will be 0xff for not used element */
};

/* structure for identifying ea/tid for sending addba/delba */
struct ampdu_ea_tid {
	struct ether_addr ea;		/* Station address */
	uint8 tid;			/* tid */
	uint8 initiator;	/* 0 is recipient, 1 is originator */
};
/* structure for identifying retry/tid for retry_limit_tid/rr_retry_limit_tid */
struct ampdu_retry_tid {
	uint8 tid;	/* tid */
	uint8 retry;	/* retry value */
};

/* structure for dpt iovars */
typedef struct dpt_iovar {
	struct ether_addr ea;		/* Station address */
	uint8 mode;			/* mode: depends on iovar */
	uint32 pad;			/* future */
} dpt_iovar_t;

#define	DPT_FNAME_LEN		48	/* Max length of friendly name */

typedef struct dpt_status {
	uint8 status;			/* flags to indicate status */
	uint8 fnlen;			/* length of friendly name */
	uchar name[DPT_FNAME_LEN];	/* friendly name */
	uint32 rssi;			/* RSSI of the link */
	sta_info_t sta;			/* sta info */
} dpt_status_t;

/* structure for dpt list */
typedef struct dpt_list {
	uint32 num;			/* number of entries in struct */
	dpt_status_t status[1];		/* per station info */
} dpt_list_t;

/* structure for dpt friendly name */
typedef struct dpt_fname {
	uint8 len;			/* length of friendly name */
	uchar name[DPT_FNAME_LEN];	/* friendly name */
} dpt_fname_t;

#define BDD_FNAME_LEN       32  /* Max length of friendly name */
typedef struct bdd_fname {
	uint8 len;          /* length of friendly name */
	uchar name[BDD_FNAME_LEN];  /* friendly name */
} bdd_fname_t;

/* structure for addts arguments */
/* For ioctls that take a list of TSPEC */
struct tslist {
	int count;			/* number of tspecs */
	struct tsinfo_arg tsinfo[1];	/* variable length array of tsinfo */
};

#ifdef WLTDLS
/* structure for tdls iovars */
typedef struct tdls_iovar {
	struct ether_addr ea;		/* Station address */
	uint8 mode;			/* mode: depends on iovar */
	chanspec_t chanspec;
	uint32 pad;			/* future */
} tdls_iovar_t;

#define TDLS_WFD_IE_SIZE		512
/* structure for tdls wfd ie */
typedef struct tdls_wfd_ie_iovar {
	struct ether_addr ea;		/* Station address */
	uint8 mode;
	uint16 length;
	uint8 data[TDLS_WFD_IE_SIZE];
} tdls_wfd_ie_iovar_t;
#endif /* WLTDLS */

/* structure for addts/delts arguments */
typedef struct tspec_arg {
	uint16 version;			/* see definition of TSPEC_ARG_VERSION */
	uint16 length;			/* length of entire structure */
	uint flag;			/* bit field */
	/* TSPEC Arguments */
	struct tsinfo_arg tsinfo;	/* TS Info bit field */
	uint16 nom_msdu_size;		/* (Nominal or fixed) MSDU Size (bytes) */
	uint16 max_msdu_size;		/* Maximum MSDU Size (bytes) */
	uint min_srv_interval;		/* Minimum Service Interval (us) */
	uint max_srv_interval;		/* Maximum Service Interval (us) */
	uint inactivity_interval;	/* Inactivity Interval (us) */
	uint suspension_interval;	/* Suspension Interval (us) */
	uint srv_start_time;		/* Service Start Time (us) */
	uint min_data_rate;		/* Minimum Data Rate (bps) */
	uint mean_data_rate;		/* Mean Data Rate (bps) */
	uint peak_data_rate;		/* Peak Data Rate (bps) */
	uint max_burst_size;		/* Maximum Burst Size (bytes) */
	uint delay_bound;		/* Delay Bound (us) */
	uint min_phy_rate;		/* Minimum PHY Rate (bps) */
	uint16 surplus_bw;		/* Surplus Bandwidth Allowance (range 1.0 to 8.0) */
	uint16 medium_time;		/* Medium Time (32 us/s periods) */
	uint8 dialog_token;		/* dialog token */
} tspec_arg_t;

/* tspec arg for desired station */
typedef	struct tspec_per_sta_arg {
	struct ether_addr ea;
	struct tspec_arg ts;
} tspec_per_sta_arg_t;

/* structure for max bandwidth for each access category */
typedef	struct wme_max_bandwidth {
	uint32	ac[AC_COUNT];	/* max bandwidth for each access category */
} wme_max_bandwidth_t;

#define WL_WME_MBW_PARAMS_IO_BYTES (sizeof(wme_max_bandwidth_t))

/* current version of wl_tspec_arg_t struct */
#define	TSPEC_ARG_VERSION		2	/* current version of wl_tspec_arg_t struct */
#define TSPEC_ARG_LENGTH		55	/* argument length from tsinfo to medium_time */
#define TSPEC_DEFAULT_DIALOG_TOKEN	42	/* default dialog token */
#define TSPEC_DEFAULT_SBW_FACTOR	0x3000	/* default surplus bw */


#define WL_WOWL_KEEPALIVE_MAX_PACKET_SIZE  80
#define WLC_WOWL_MAX_KEEPALIVE	2

/* Packet lifetime configuration per ac */
typedef struct wl_lifetime {
	uint32 ac;	        /* access class */
	uint32 lifetime;    /* Packet lifetime value in ms */
} wl_lifetime_t;

/* Channel Switch Announcement param */
typedef struct wl_chan_switch {
	uint8 mode;		/* value 0 or 1 */
	uint8 count;		/* count # of beacons before switching */
	chanspec_t chspec;	/* chanspec */
	uint8 reg;		/* regulatory class */
	uint8 frame_type;		/* csa frame type, unicast or broadcast */
} wl_chan_switch_t;

/*
 * Preferred Network Offload (PNO, formerly PFN) defines
 */
enum {
	PFN_LIST_ORDER,
	PFN_RSSI
};

enum {
	DISABLE,
	ENABLE
};

enum {
	OFF_ADAPT,
	SMART_ADAPT,
	STRICT_ADAPT,
	SLOW_ADAPT
};

/* PFN network info structure */
typedef struct wl_pfn_subnet_info {
	struct ether_addr BSSID;
	uint8	channel; /* channel number only */
	uint8	SSID_len;
	uint8	SSID[32];
} wl_pfn_subnet_info_t;

typedef struct wl_pfn_net_info {
	wl_pfn_subnet_info_t pfnsubnet;
	int16	RSSI; /* receive signal strength (in dBm) */
	uint16	timestamp; /* age in seconds */
} wl_pfn_net_info_t;

typedef struct wl_pfn_lnet_info {
	wl_pfn_subnet_info_t pfnsubnet; /* BSSID + channel + SSID len + SSID */
	uint16	flags; /* partial scan, etc */
	int16	RSSI; /* receive signal strength (in dBm) */
	uint32	timestamp; /* age in miliseconds */
	uint16	rtt0; /* estimated distance to this AP in centimeters */
	uint16	rtt1; /* standard deviation of the distance to this AP in centimeters */
} wl_pfn_lnet_info_t;

typedef struct wl_pfn_lscanresults {
	uint32 version;
	uint32 status;
	uint32 count;
	wl_pfn_lnet_info_t netinfo[1];
} wl_pfn_lscanresults_t;

/* this is used to report on 1-* pfn scan results */
typedef struct wl_pfn_scanresults {
	uint32 version;
	uint32 status;
	uint32 count;
	wl_pfn_net_info_t netinfo[1];
} wl_pfn_scanresults_t;

typedef struct wl_pfn_net_info_bssid {
	struct ether_addr BSSID;
	uint8 channel;	/* channel number only */
	int8  RSSI;	/* receive signal strength (in dBm) */
	uint16 flags;	/* (e.g. partial scan, off channel) */
	uint16 timestamp; /* age in seconds */
} wl_pfn_net_info_bssid_t;

typedef struct wl_pfn_scanhist_bssid {
	uint32 version;
	uint32 status;
	uint32 count;
	wl_pfn_net_info_bssid_t netinfo[1];
} wl_pfn_scanhist_bssid_t;

/* used to report exactly one scan result */
/* plus reports detailed scan info in bss_info */
typedef struct wl_pfn_scanresult {
	uint32 version;
	uint32 status;
	uint32 count;
	wl_pfn_net_info_t netinfo;
	wl_bss_info_t bss_info;
} wl_pfn_scanresult_t;

/* PFN data structure */
typedef struct wl_pfn_param {
	int32 version;			/* PNO parameters version */
	int32 scan_freq;		/* Scan frequency */
	int32 lost_network_timeout;	/* Timeout in sec. to declare
								* discovered network as lost
								*/
	int16 flags;			/* Bit field to control features
							* of PFN such as sort criteria auto
							* enable switch and background scan
							*/
	int16 rssi_margin;		/* Margin to avoid jitter for choosing a
							* PFN based on RSSI sort criteria
							*/
	uint8 bestn; /* number of best networks in each scan */
	uint8 mscan; /* number of scans recorded */
	uint8 repeat; /* Minimum number of scan intervals
				     *before scan frequency changes in adaptive scan
				     */
	uint8 exp; /* Exponent of 2 for maximum scan interval */
#if !defined(WLC_PATCH) || !defined(BCM43362A2)
	int32 slow_freq; /* slow scan period */
#endif /* !WLC_PATCH || !BCM43362A2 */
} wl_pfn_param_t;

typedef struct wl_pfn_bssid {
	struct ether_addr  macaddr;
	/* Bit4: suppress_lost, Bit3: suppress_found */
	uint16             flags;
} wl_pfn_bssid_t;

typedef struct wl_pfn_cfg {
	uint32	reporttype;
	int32	channel_num;
	uint16	channel_list[WL_NUMCHANNELS];
	uint32	flags;
} wl_pfn_cfg_t;

typedef struct wl_pfn {
	wlc_ssid_t		ssid;			/* ssid name and its length */
	int32			flags;			/* bit2: hidden */
	int32			infra;			/* BSS Vs IBSS */
	int32			auth;			/* Open Vs Closed */
	int32			wpa_auth;		/* WPA type */
	int32			wsec;			/* wsec value */
} wl_pfn_t;

typedef struct wl_pfn_list {
	uint32		version;
	uint32		enabled;
	uint32		count;
	wl_pfn_t	pfn[1];
} wl_pfn_list_t;

typedef BWL_PRE_PACKED_STRUCT struct pfn_olmsg_params_t {
	wlc_ssid_t ssid;
	uint32	cipher_type;
	uint32	auth_type;
	uint8	channels[4];
} BWL_POST_PACKED_STRUCT pfn_olmsg_params;

/* Dynamic scan configuration for motion profiles */

#define WL_PFN_MPF_VERSION 1

/* Valid group IDs, may be expanded in the future */
#define WL_PFN_MPF_GROUP_SSID 0
#define WL_PFN_MPF_GROUP_BSSID 1
#define WL_PFN_MPF_MAX_GROUPS 2

/* Max number of MPF states supported in this time */
#define WL_PFN_MPF_STATES_MAX 4

/* Flags for the mpf-specific stuff */
#define WL_PFN_MPF_ADAPT_ON_BIT		0
#define WL_PFN_MPF_ADAPTSCAN_BIT	1

#define WL_PFN_MPF_ADAPT_ON_MASK	0x0001
#define WL_PFN_MPF_ADAPTSCAN_MASK 	0x0006

/* Per-state timing values */
typedef struct wl_pfn_mpf_state_params {
	int32  scan_freq;	/* Scan frequency (secs) */
	int32  lost_network_timeout; /* Timeout to declare net lost (secs) */
	int16  flags;		/* Space for flags: ADAPT etc */
	uint8  exp;		/* Exponent of 2 for max interval for SMART/STRICT_ADAPT */
	uint8  repeat;		/* Number of scans before changing adaptation level */
	int32  slow_freq;	/* Slow scan period for SLOW_ADAPT */
} wl_pfn_mpf_state_params_t;

typedef struct wl_pfn_mpf_param {
	uint16 version;		/* Structure version */
	uint16 groupid;		/* Group ID: 0 (SSID), 1 (BSSID), other: reserved */
	wl_pfn_mpf_state_params_t state[WL_PFN_MPF_STATES_MAX];
} wl_pfn_mpf_param_t;

/* Structure for setting pfn_override iovar */
typedef struct wl_pfn_override_param {
	uint16 version;         /* Structure version */
	uint16 start_offset;    /* Seconds from now to apply new params */
	uint16 duration;        /* Seconds to keep new params applied */
	uint16 reserved;
	wl_pfn_mpf_state_params_t override;
} wl_pfn_override_param_t;
#define WL_PFN_OVERRIDE_VERSION	1

/* To configure pfn_macaddr */
typedef struct wl_pfn_macaddr_cfg {
	uint8 version;
	uint8 reserved;
	struct ether_addr macaddr;
} wl_pfn_macaddr_cfg_t;
#define WL_PFN_MACADDR_CFG_VER 0

/*
 * End of PNO/PFN definitions
 */

/*
 * Definitions for base MPF configuration
 */

#define WL_MPF_VERSION 1
#define WL_MPF_MAX_BITS 3
#define WL_MPF_MAX_STATES (1 << WL_MPF_MAX_BITS)

#define WL_MPF_STATE_NAME_MAX 12

typedef struct wl_mpf_val {
	uint16 val;		/* Value of GPIO bits */
	uint16 state;		/* State identifier */
	char name[WL_MPF_STATE_NAME_MAX]; /* Optional name */
} wl_mpf_val_t;

typedef struct wl_mpf_map {
	uint16 version;
	uint16 type;
	uint16 mask;		/* Which GPIO bits to use */
	uint8  count;		/* Count of state/value mappings */
	uint8  PAD1;
	wl_mpf_val_t vals[WL_MPF_MAX_STATES];
} wl_mpf_map_t;

#define WL_MPF_STATE_AUTO (0xFFFF) /* (uint16)-1) */

typedef struct wl_mpf_state {
	uint16 version;
	uint16 type;
	uint16 state;		/* Get/Set */
	uint8 force;		/* 0 - auto (HW) state, 1 - forced state */
	char name[WL_MPF_STATE_NAME_MAX]; /* Get/Set: Optional/actual name */
} wl_mpf_state_t;

/*
 * End of MPF definitions
 */

#endif /* LINUX_POSTMOGRIFY_REMOVAL */

/* Service discovery */
typedef struct {
	uint8	transaction_id;	/* Transaction id */
	uint8	protocol;	/* Service protocol type */
	uint16	query_len;	/* Length of query */
	uint16	response_len;	/* Length of response */
	uint8	qrbuf[1];
} wl_p2po_qr_t;

typedef struct {
	uint16			period;			/* extended listen period */
	uint16			interval;		/* extended listen interval */
} wl_p2po_listen_t;

/* ANQP offload */

#define ANQPO_MAX_QUERY_SIZE		256
typedef struct {
	uint16 max_retransmit;		/* ~0 use default, max retransmit on no ACK from peer */
	uint16 response_timeout;	/* ~0 use default, msec to wait for resp after tx packet */
	uint16 max_comeback_delay;	/* ~0 use default, max comeback delay in resp else fail */
	uint16 max_retries;			/* ~0 use default, max retries on failure */
	uint16 query_len;			/* length of ANQP query */
	uint8 query_data[1];		/* ANQP encoded query (max ANQPO_MAX_QUERY_SIZE) */
} wl_anqpo_set_t;

typedef struct {
	uint16 channel;				/* channel of the peer */
	struct ether_addr addr;		/* addr of the peer */
} wl_anqpo_peer_t;

#define ANQPO_MAX_PEER_LIST			64
typedef struct {
	uint16 count;				/* number of peers in list */
	wl_anqpo_peer_t peer[1];	/* max ANQPO_MAX_PEER_LIST */
} wl_anqpo_peer_list_t;

#define ANQPO_MAX_IGNORE_SSID		64
typedef struct {
	bool is_clear;				/* set to clear list (not used on GET) */
	uint16 count;				/* number of SSID in list */
	wlc_ssid_t ssid[1];			/* max ANQPO_MAX_IGNORE_SSID */
} wl_anqpo_ignore_ssid_list_t;

#define ANQPO_MAX_IGNORE_BSSID		64
typedef struct {
	bool is_clear;				/* set to clear list (not used on GET) */
	uint16 count;				/* number of addr in list */
	struct ether_addr bssid[1];	/* max ANQPO_MAX_IGNORE_BSSID */
} wl_anqpo_ignore_bssid_list_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL

struct toe_ol_stats_t {
	/* Num of tx packets that don't need to be checksummed */
	uint32 tx_summed;

	/* Num of tx packets where checksum is filled by offload engine */
	uint32 tx_iph_fill;
	uint32 tx_tcp_fill;
	uint32 tx_udp_fill;
	uint32 tx_icmp_fill;

	/*  Num of rx packets where toe finds out if checksum is good or bad */
	uint32 rx_iph_good;
	uint32 rx_iph_bad;
	uint32 rx_tcp_good;
	uint32 rx_tcp_bad;
	uint32 rx_udp_good;
	uint32 rx_udp_bad;
	uint32 rx_icmp_good;
	uint32 rx_icmp_bad;

	/* Num of tx packets in which csum error is injected */
	uint32 tx_tcp_errinj;
	uint32 tx_udp_errinj;
	uint32 tx_icmp_errinj;

	/* Num of rx packets in which csum error is injected */
	uint32 rx_tcp_errinj;
	uint32 rx_udp_errinj;
	uint32 rx_icmp_errinj;
};

/* Arp offload statistic counts */
struct arp_ol_stats_t {
	uint32  host_ip_entries;	/* Host IP table addresses (more than one if multihomed) */
	uint32  host_ip_overflow;	/* Host IP table additions skipped due to overflow */

	uint32  arp_table_entries;	/* ARP table entries */
	uint32  arp_table_overflow;	/* ARP table additions skipped due to overflow */

	uint32  host_request;		/* ARP requests from host */
	uint32  host_reply;		/* ARP replies from host */
	uint32  host_service;		/* ARP requests from host serviced by ARP Agent */

	uint32  peer_request;		/* ARP requests received from network */
	uint32  peer_request_drop;	/* ARP requests from network that were dropped */
	uint32  peer_reply;		/* ARP replies received from network */
	uint32  peer_reply_drop;	/* ARP replies from network that were dropped */
	uint32  peer_service;		/* ARP request from host serviced by ARP Agent */
};

/* NS offload statistic counts */
struct nd_ol_stats_t {
	uint32  host_ip_entries;    /* Host IP table addresses (more than one if multihomed) */
	uint32  host_ip_overflow;   /* Host IP table additions skipped due to overflow */
	uint32  peer_request;       /* NS requests received from network */
	uint32  peer_request_drop;  /* NS requests from network that were dropped */
	uint32  peer_reply_drop;    /* NA replies from network that were dropped */
	uint32  peer_service;       /* NS request from host serviced by firmware */
};

/*
 * Keep-alive packet offloading.
 */

/* NAT keep-alive packets format: specifies the re-transmission period, the packet
 * length, and packet contents.
 */
typedef struct wl_keep_alive_pkt {
	uint32	period_msec;	/* Retransmission period (0 to disable packet re-transmits) */
	uint16	len_bytes;	/* Size of packet to transmit (0 to disable packet re-transmits) */
	uint8	data[1];	/* Variable length packet to transmit.  Contents should include
				 * entire ethernet packet (enet header, IP header, UDP header,
				 * and UDP payload) in network byte order.
				 */
} wl_keep_alive_pkt_t;

#define WL_KEEP_ALIVE_FIXED_LEN		OFFSETOF(wl_keep_alive_pkt_t, data)

typedef struct awdl_config_params {
	uint32	version;
	uint8	awdl_chan;		/* awdl channel */
	uint8	guard_time;		/* Guard Time */
	uint16	aw_period;		/* AW interval period */
	uint16  aw_cmn_length;		/* Radio on Time AW */
	uint16	action_frame_period;	/* awdl action frame period */
	uint16  awdl_pktlifetime;	/* max packet life time in msec for awdl action frames  */
	uint16  awdl_maxnomaster;	/* max master missing time */
	uint16  awdl_extcount;		/* Max extended period count for traffic  */
	uint16	aw_ext_length;		/* AW ext period */
	uint16	awdl_nmode;	        /* Operation mode of awdl interface; * 0 - Legacy mode
					 * 1 - 11n rate only   * 2 - 11n + ampdu rx/tx
					 */
	struct ether_addr ea;		/* destination bcast/mcast  address to which action frame
					 * need to be sent
					 */
} awdl_config_params_t;

typedef struct wl_awdl_action_frame {
	uint16	len_bytes;
	uint8	awdl_action_frame_data[1];
} wl_awdl_action_frame_t;

#define WL_AWDL_ACTION_FRAME_FIXED_LEN		OFFSETOF(wl_awdl_action_frame_t, awdl_sync_frame)

typedef struct awdl_peer_node {
	uint32	type_state;		/* Master, slave , etc.. */
	uint16	aw_counter;		/* avail window counter */
	int8	rssi;			/* rssi last af was received at */
	int8	last_rssi;		/* rssi in the last AF */
	uint16	tx_counter;
	uint16	tx_delay;		/* ts_hw - ts_fw */
	uint16	period_tu;
	uint16	aw_period;
	uint16	aw_cmn_length;
	uint16	aw_ext_length;
	uint32	self_metrics;		/* Election Metric */
	uint32	top_master_metrics;	/* Top Master Metric */
	struct ether_addr	addr;
	struct ether_addr	top_master;
	uint8	dist_top;		/* Distance from Top */

	uint8  has_private_election_params;
	struct ether_addr private_top_master;
	uint32 private_top_master_metric;
	uint32 private_election_ID;
	uint8  private_distance_from_top;
} awdl_peer_node_t;

typedef struct awdl_peer_table {
	uint16  version;
	uint16	len;
	uint8 peer_nodes[1];
} awdl_peer_table_t;

typedef struct awdl_af_hdr {
	struct ether_addr dst_mac;
	uint8 action_hdr[4]; /* Category + OUI[3] */
} awdl_af_hdr_t;

typedef struct awdl_oui {
	uint8 oui[3];	/* default: 0x00 0x17 0xf2 */
	uint8 oui_type; /* AWDL: 0x08 */
} awdl_oui_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_hdr {
	uint8	type;		/* 0x08 AWDL */
	uint8	version;
	uint8	sub_type;	/* Sub type */
	uint8	rsvd;		/* Reserved */
	uint32	phy_timestamp;	/* PHY Tx time */
	uint32	fw_timestamp;	/* Target Tx time */
} BWL_POST_PACKED_STRUCT awdl_hdr_t;

/* AWDL AF flags for awdl_oob_af iovar */
#define AWDL_OOB_AF_FILL_TSF_PARAMS			0x00000001
#define AWDL_OOB_AF_FILL_SYNC_PARAMS		0x00000002
#define AWDL_OOB_AF_FILL_ELECT_PARAMS		0x00000004
#define AWDL_OOB_AF_PARAMS_SIZE 38

typedef BWL_PRE_PACKED_STRUCT struct awdl_oob_af_params {
	struct ether_addr bssid;
	struct ether_addr dst_mac;
	uint32 channel;
	uint32 dwell_time;
	uint32 flags;
	uint32 pkt_lifetime;
	uint32 tx_rate;
	uint32 max_retries; /* for unicast frames only */
	uint16 payload_len;
	uint8  payload[1]; /* complete AF payload */
} BWL_POST_PACKED_STRUCT awdl_oob_af_params_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_oob_af_params_async {
	uint32 tx_time;     /* tsf time to transmit, in usec */
	uint16 tag;         /* packet tag */
	struct ether_addr bssid;
	struct ether_addr dst_mac;
	uint32 channel;
	uint32 dwell_time;
	uint32 flags;
	uint32 pkt_lifetime;
	uint32 tx_rate;
	uint32 max_retries; /* for unicast frames only */
	uint16 payload_len;
	uint8  payload[1]; /* complete AF payload */
} BWL_POST_PACKED_STRUCT awdl_oob_af_params_async_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_oob_af_params_auto {
	uint32 tx_chan_map; /* bitmap for the channels in the chan seq to transmit the af */
	uint32 tx_aws_offset; /* time to transmit from the aw start, in usec */
	struct ether_addr bssid;
	struct ether_addr dst_mac;
	uint32 channel;
	uint32 dwell_time;
	uint32 flags;
	uint32 pkt_lifetime;
	uint32 tx_rate;
	uint32 max_retries; /* for unicast frames only */
	uint16 payload_len;
	uint8  payload[1]; /* complete AF payload */
} BWL_POST_PACKED_STRUCT awdl_oob_af_params_auto_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_sync_params {
	uint8	type;			/* Type */
	uint16	param_len;		/* sync param length */
	uint8	tx_chan;		/* tx channel */
	uint16	tx_counter;		/* tx down counter */
	uint8	master_chan;		/* master home channel */
	uint8	guard_time;		/* Gaurd Time */
	uint16	aw_period;		/* AW period */
	uint16	action_frame_period;	/* awdl action frame period */
	uint16	awdl_flags;		/* AWDL Flags */
	uint16	aw_ext_length;		/* AW extention len */
	uint16	aw_cmn_length;		/* AW common len */
	uint16	aw_remaining;		/* Remaining AW length */
	uint8	min_ext;		/* Minimum Extention count */
	uint8	max_ext_multi;		/* Max multicast Extention count */
	uint8	max_ext_uni;		/* Max unicast Extention count */
	uint8	max_ext_af;		/* Max af Extention count */
	struct ether_addr current_master;	/* Current Master mac addr */
	uint8	presence_mode;		/* Presence mode */
	uint8	reserved;
	uint16	aw_counter;		/* AW seq# */
	uint16	ap_bcn_alignment_delta;	/* AP Beacon alignment delta  */
} BWL_POST_PACKED_STRUCT awdl_sync_params_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_channel_sequence {
	uint8	aw_seq_len;		/* AW seq length */
	uint8	aw_seq_enc;		/* AW seq encoding */
	uint8	aw_seq_duplicate_cnt;	/* AW seq dupilcate count */
	uint8	seq_step_cnt;		/* Seq spet count */
	uint16	seq_fill_chan;		/* channel to fill in; 0xffff repeat current channel */
	uint8	chan_sequence[1];	/* Variable list of channel Sequence */
} BWL_POST_PACKED_STRUCT awdl_channel_sequence_t;
#define WL_AWDL_CHAN_SEQ_FIXED_LEN   OFFSETOF(awdl_channel_sequence_t, chan_sequence)

typedef BWL_PRE_PACKED_STRUCT struct awdl_election_info {
	uint8	election_flags;	/* Election Flags */
	uint16	election_ID;	/* Election ID */
	uint32	self_metrics;
} BWL_POST_PACKED_STRUCT awdl_election_info_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_election_tree_info {
	uint8	election_flags;	/* Election Flags */
	uint16	election_ID;	/* Election ID */
	uint32	self_metrics;
	int8 close_sync_rssi_thld;
	int8 master_rssi_boost;
	int8 edge_sync_rssi_thld;
	int8 close_range_rssi_thld;
	int8 mid_range_rssi_thld;
	uint8 max_higher_masters_close_range;
	uint8 max_higher_masters_mid_range;
	uint8 max_tree_depth;
	/* read only */
	struct ether_addr top_master;	/* top Master mac addr */
	uint32 top_master_self_metric;
	uint8  current_tree_depth;

	uint8 edge_master_dwell_cnt;
	struct ether_addr private_top_master;	/* private top Master mac addr */
	uint32 private_top_master_metric;
	uint32 private_election_ID;
	uint8  private_distance_from_top;
} BWL_POST_PACKED_STRUCT awdl_election_tree_info_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_election_params_tlv {
	uint8	type;			/* Type */
	uint16	param_len;		/* Election param length */
	uint8	election_flags;	/* Election Flags */
	uint16	election_ID;	/* Election ID */
	uint8	dist_top;	/* Distance from Top */
	uint8	rsvd;		/* Reserved */
	struct ether_addr top_master;	/* Top Master mac addr */
	uint32	top_master_metrics;
	uint32	self_metrics;
	uint8	pad[2];		/* Padding  */
} BWL_POST_PACKED_STRUCT awdl_election_params_tlv_t;

typedef struct awdl_payload {
	uint32	len;		/* Payload length */
	uint8	payload[1];	/* Payload */
} awdl_payload_t;

typedef struct awdl_long_payload {
	uint8   long_psf_period;      /* transmit every long_psf_perios AWs */
	uint8   long_psf_tx_offset;   /* delay from aw_start */
	uint16	len;		          /* Payload length */
	uint8	payload[1];           /* Payload */
} BWL_POST_PACKED_STRUCT awdl_long_payload_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_opmode {
	uint8	mode;		/* 0 - Auto; 1 - Fixed */
	uint8	role;		/* 0 - slave; 1 - non-elect master; 2 - master */
	uint16	bcast_tu; /* Bcasting period(TU) for non-elect master */
	struct ether_addr master; /* Address of master to sync to */
	uint16	cur_bcast_tu;	/* Current Bcasting Period(TU) */
} BWL_PRE_PACKED_STRUCT awdl_opmode_t;
/* Values for awdl_opmode_t.role */
#define AWDL_ROLE_SLAVE		0
#define AWDL_ROLE_NE_MASTER	1
#define AWDL_ROLE_MASTER	2

typedef BWL_PRE_PACKED_STRUCT struct awdl_extcount {
	uint8	minExt;			/* Min extension count */
	uint8	maxExtMulti;	/* Max extension count for mcast packets */
	uint8	maxExtUni;		/* Max extension count for unicast packets */
	uint8	maxAfExt;			/* Max extension count */
} BWL_PRE_PACKED_STRUCT awdl_extcount_t;

#define AWDL_OPMODE_AUTO	0
#define AWDL_OPMODE_FIXED	1

/* peer add/del operation */
typedef struct awdl_peer_op {
	uint8 version;
	uint8 opcode;	/* see opcode definition */
	struct ether_addr addr;
	uint8 mode;
} awdl_peer_op_t;

/* peer op table */
typedef struct awdl_peer_op_tbl {
	uint16	len;		/* length */
	uint8	tbl[1];	/* Peer table */
} awdl_peer_op_tbl_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_peer_op_node {
	struct 	ether_addr addr;
	uint32 	flags;	/* Flags to indicate various states */
	uint16 	chanseq_len;
	uint8 	chanseq[1];
} BWL_POST_PACKED_STRUCT awdl_peer_op_node_t;

/* awdl_peer_op_node_t flags */
#define AWDL_PEER_NODE_OP_FLAG_HT        0x01
#define AWDL_PEER_NODE_OP_FLAG_AMPDU     0x02
#define AWDL_PEER_NODE_OP_FLAG_PM        0x04
#define AWDL_PEER_NODE_OP_FLAG_ABAND     0x08
#define AWDL_PEER_NODE_OP_FLAG_QOS       0x10
#define AWDL_PEER_NODE_OP_FLAG_AWDL      0x20
#define AWDL_PEER_OP_CUR_VER	0

/* AWDL related statistics */
typedef BWL_PRE_PACKED_STRUCT struct awdl_stats {
	uint32	afrx;
	uint32	aftx;
	uint32	datatx;
	uint32	datarx;
	uint32	txdrop;
	uint32	rxdrop;
	uint32	monrx;
	uint32	lostmaster;
	uint32	misalign;
	uint32	aws;
	uint32	aw_dur;
	uint32	debug;
	uint32  txsupr;
	uint32	afrxdrop;
	uint32  awdrop;
	uint32  noawchansw;
	uint32  rx80211;
	uint32  peeropdrop;
	uint16	chancal;
	uint16	nopreawint;
} BWL_POST_PACKED_STRUCT awdl_stats_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_uct_stats {
	uint32 aw_proc_in_aw_sched;
	uint32 aw_upd_in_pre_aw_proc;
	uint32 pre_aw_proc_in_aw_set;
	uint32 ignore_pre_aw_proc;
	uint32 miss_pre_aw_intr;
	uint32 aw_dur_zero;
	uint32 aw_sched;
	uint32 aw_proc;
	uint32 pre_aw_proc;
	uint32 not_init;
	uint32 null_awdl;
} BWL_POST_PACKED_STRUCT awdl_uct_stats_t;

/* peer opcode */
#define AWDL_PEER_OP_ADD	0
#define AWDL_PEER_OP_DEL	1
#define AWDL_PEER_OP_INFO	2
#define AWDL_PEER_OP_UPD	3


/* AWDL Piggy backed scan */
typedef struct wl_awdl_pscan_params {
	wlc_ssid_t ssid;		/* default: {0, ""} */
	struct ether_addr bssid;	/* default: bcast */
	uint8 scan_type;		/* active or passive, 0 use default */
	uint8 pad;			/* pad */
	int32 nprobes;			/* -1 use default, number of probes per channel */
	int32 aw_seq_num;		/* count AW sequence nunbers to be piggy backed for scan */
	int32 nssid;			/* count of ssid in list */
	int32 rsvd;			/* Reserved  */
	uint16 aw_counter_list[1];	/* This is a list contains in following order
					 *    - List aw seq numbers
					 *    - List of SSID's 4 byte aligned.
					 */
} wl_awdl_pscan_params_t;

typedef struct wl_pscan_params {
	uint32 version;
	uint16 action;		/* PSCAN action type: FW or Host initiated pscan or abort pscan */
	uint16 sync_id;
	wl_awdl_pscan_params_t params;
} wl_pscan_params_t;

#define WL_AWDL_PSCAN_PARAMS_FIXED_SIZE (OFFSETOF(wl_awdl_pscan_params_t, aw_counter_list))
#define WL_AWDL_MAX_NUM_AWSEQ	64
#define AWDL_PSCAN_REQ_VERSION	1


/* awdl pscan action type values */
#define AWDL_HOST_PSCAN		0	/* Host Initiated PSCAN */
#define AWDL_FW_PSCAN		1	/* Firmware Initiated PSCAN */
#define AWDL_ABORT_PSCAN	2	/* Abort any PSCAN */

/* "aftxmode" iovar values */
#define AWDL_AFTXMODE_AUTO	0	/* Send AF on AWDL channel best effort while outside AW */

/* --- Deprecated ---- */
#define AWDL_AFTXMODE_INFRA	1	/* Send AF on Infra channel while outside AW */
#define AWDL_AFTXMODE_CUR_CHAN	2	/* Send AF on Current channel while outside AW */
/* --- Deprecated ---- */

#define AWDL_AFTXMODE_SUPPRESS	3	/* Suppress AF Tx */
#define AWDL_AFTXMODE_SYNC_PREAW 4	/* Send AF on master channel/s always in pre AW time */
#define AWDL_AFTXMODE_LAST	4	/* Last AWDL_AFTXMODE_XXX */

typedef struct awdl_pw_opmode {
	struct ether_addr top_master;	/* Peer mac addr */
	uint8 mode; /* 0 - normal; 1 - fast mode */
} awdl_pw_opmode_t;

/* i/f request */
typedef struct wl_awdl_if2 {
	int32 cfg_idx;
	int32 up;
	struct ether_addr bssid;
	struct ether_addr if_addr;
} wl_awdl_if2_t;

typedef struct _aw_start {
	uint8 role;
	struct ether_addr	master;
	uint8	aw_seq_num;
} aw_start_t;

typedef struct _aw_extension_start {
	uint8 aw_ext_num;
} aw_extension_start_t;

typedef struct _awdl_peer_state {
	struct ether_addr peer;
	uint8	state;
} awdl_peer_state_t;
#define AWDL_PEER_STATE_OPEN	0
#define AWDL_PEER_STATE_CLOSE	1

typedef struct _awdl_sync_state_changed {
	uint8	new_role;
	struct ether_addr master;
} awdl_sync_state_changed_t;

typedef struct _awdl_sync_state {
	uint8	role;
	struct ether_addr master;
	uint32 continuous_election_enable;
} awdl_sync_state_t;

typedef struct _awdl_aw_ap_alignment {
	uint32	enabled;
	int32	offset;
	uint32	align_on_dtim;
} awdl_aw_ap_alignment_t;

typedef struct _awdl_peer_stats {
	uint32 version;
	struct ether_addr address;
	uint8 clear;
	int8 rssi;
	int8 avg_rssi;
	uint8 txRate;
	uint8 rxRate;
	uint32 numTx;
	uint32 numTxRetries;
	uint32 numTxFailures;
} awdl_peer_stats_t;

#define MAX_NUM_AWDL_KEYS 4
typedef struct _awdl_aes_key {
	uint32 version;
	int32 enable;
	struct ether_addr awdl_peer;
	uint8 keys[MAX_NUM_AWDL_KEYS][16];
} awdl_aes_key_t;

/*
 * awdl ranging
 * all the fields with multple bytes are in the little Endian order
 */

/* Bit defines for global flags */
#define AWDL_RANGING_ENABLE		(1<<0)	/* Global enable bit */
#define AWDL_RANGING_RESPOND		(1<<1)	/* Enable responding to peer's range req */
#define AWDL_RANGING_RANGED		(1<<2)	/* V2: Report to host if ranged as target */
typedef BWL_PRE_PACKED_STRUCT struct awdl_ranging_config {
	uint16 flags;
	uint8 sounding_count;		/* self initiated ranging: number of probes per peer */
	uint8 reserved;
	struct ether_addr allow_mac;
					/* peer initiated ranging: the allowed peer mac
					 * address, a unicast (for one peer) or
					 * a broadcast for all. Setting it to all zeros
					 * means responding to none,same as not setting
					 * the flag bit AWDL_RANGING_RESPOND
					 */
} BWL_POST_PACKED_STRUCT awdl_ranging_config_t;

/* list of peers for self initiated ranging */
/* Bit defines for per peer flags */
#define AWDL_RANGING_REPORT (1<<0)	/* V2: Enable reporting range to target */
typedef BWL_PRE_PACKED_STRUCT struct awdl_ranging_peer {
	chanspec_t ranging_chanspec; 	/* desired chanspec for this peer */
	uint16 flags; 			/* per peer flags, report or not */
	struct ether_addr ea; 		/* peer MAC address */
} BWL_POST_PACKED_STRUCT awdl_ranging_peer_t;
typedef BWL_PRE_PACKED_STRUCT struct awdl_ranging_list {
	uint8 count; 			/* number of MAC addresses */
	uint8 num_peers_done;		/* host set to 0, when read, shows number of peers
					 * completed, success or fail
					 */
	uint8 num_aws;			/* time period to do the ranging, specified in aws */
	awdl_ranging_peer_t rp[1];	/* variable length array of peers */
} BWL_POST_PACKED_STRUCT awdl_ranging_list_t;

/* ranging results, a list for self initiated ranging and one for peer initiated ranging */
/* There will be one structure for each peer */
#define AWDL_RANGING_STATUS_SUCCESS		1
#define AWDL_RANGING_STATUS_FAIL		2
#define AWDL_RANGING_STATUS_TIMEOUT		3
#define AWDL_RANGING_STATUS_ABORT		4 /* with partial results if sounding count > 0 */
typedef BWL_PRE_PACKED_STRUCT struct awdl_ranging_result {
	uint8 status; 			/* 1: Success, 2: Fail 3: Timeout 4: Aborted */
	uint8 sounding_count;		/* number of measurements completed (0 = failure) */
	struct ether_addr ea;		/* peer MAC address */
	chanspec_t ranging_chanspec;	/* Chanspec where the ranging was done */
	uint32 timestamp;		/* 32bits of the TSF timestamp ranging was completed at */
	uint32 distance;		/* mean distance in meters expressed as Q4 number.
					 * Only valid when sounding_count > 0. Examples:
					 * 0x08 = 0.5m
					 * 0x10 = 1m
					 * 0x18 = 1.5m
					 * set to 0xffffffff to indicate invalid number
					 */
	int32 rtt_var;			/* standard deviation in 10th of ns of RTTs measured.
					 * Only valid when sounding_count > 0
					 */
} BWL_POST_PACKED_STRUCT awdl_ranging_result_t;
#define AWDL_RANGING_TYPE_HOST	1
#define AWDL_RANGING_TYPE_PEER	2
typedef BWL_PRE_PACKED_STRUCT struct awdl_ranging_event_data {
	uint8 type;			/* 1: Result of host initiated ranging */
					/* V2: 2: Result of peer initiated ranging */
	uint8 reserved;
	uint8 success_count;		/* number of peers completed successfully */
	uint8 count;			/* number of peers in the list */
	awdl_ranging_result_t rr[1];	/* variable array of ranging peers */
} BWL_POST_PACKED_STRUCT awdl_ranging_event_data_t;

/*
 * Dongle pattern matching filter.
 */

#define MAX_WAKE_PACKET_CACHE_BYTES 128 /* Maximum cached wake packet */

#define MAX_WAKE_PACKET_BYTES	    (DOT11_A3_HDR_LEN +			    \
				     DOT11_QOS_LEN +			    \
				     sizeof(struct dot11_llc_snap_header) + \
				     ETHER_MAX_DATA)

typedef struct pm_wake_packet {
	uint32	status;		/* Is the wake reason a packet (if all the other field's valid) */
	uint32	pattern_id;	/* Pattern ID that matched */
	uint32	original_packet_size;
	uint32	saved_packet_size;
	uchar	packet[MAX_WAKE_PACKET_CACHE_BYTES];
} pm_wake_packet_t;

/* Packet filter types. Currently, only pattern matching is supported. */
typedef enum wl_pkt_filter_type {
	WL_PKT_FILTER_TYPE_PATTERN_MATCH=0,	/* Pattern matching filter */
	WL_PKT_FILTER_TYPE_MAGIC_PATTERN_MATCH=1, /* Magic packet match */
	WL_PKT_FILTER_TYPE_PATTERN_LIST_MATCH=2	/* A pattern list (match all to match filter) */
} wl_pkt_filter_type_t;

#define WL_PKT_FILTER_TYPE wl_pkt_filter_type_t

/* String mapping for types that may be used by applications or debug */
#define WL_PKT_FILTER_TYPE_NAMES \
	{ "PATTERN", WL_PKT_FILTER_TYPE_PATTERN_MATCH },       \
	{ "MAGIC",   WL_PKT_FILTER_TYPE_MAGIC_PATTERN_MATCH }, \
	{ "PATLIST", WL_PKT_FILTER_TYPE_PATTERN_LIST_MATCH }

/* Pattern matching filter. Specifies an offset within received packets to
 * start matching, the pattern to match, the size of the pattern, and a bitmask
 * that indicates which bits within the pattern should be matched.
 */
typedef struct wl_pkt_filter_pattern {
	uint32	offset;		/* Offset within received packet to start pattern matching.
				 * Offset '0' is the first byte of the ethernet header.
				 */
	uint32	size_bytes;	/* Size of the pattern.  Bitmask must be the same size. */
	uint8   mask_and_pattern[1]; /* Variable length mask and pattern data.  mask starts
				      * at offset 0.  Pattern immediately follows mask.
				      */
} wl_pkt_filter_pattern_t;

/* A pattern list is a numerically specified list of modified pattern structures. */
typedef struct wl_pkt_filter_pattern_listel {
	uint16 rel_offs;	/* Offset to begin match (relative to 'base' below) */
	uint16 base_offs;	/* Base for offset (defined below) */
	uint16 size_bytes;	/* Size of mask/pattern */
	uint16 match_flags;	/* Addition flags controlling the match */
	uint8  mask_and_data[1]; /* Variable length mask followed by data, each size_bytes */
} wl_pkt_filter_pattern_listel_t;

typedef struct wl_pkt_filter_pattern_list {
	uint8 list_cnt;		/* Number of elements in the list */
	uint8 PAD1[1];		/* Reserved (possible version: reserved) */
	uint16 totsize;		/* Total size of this pattern list (includes this struct) */
	wl_pkt_filter_pattern_listel_t patterns[1]; /* Variable number of list elements */
} wl_pkt_filter_pattern_list_t;

/* IOVAR "pkt_filter_add" parameter. Used to install packet filters. */
typedef struct wl_pkt_filter {
	uint32	id;		/* Unique filter id, specified by app. */
	uint32	type;		/* Filter type (WL_PKT_FILTER_TYPE_xxx). */
	uint32	negate_match;	/* Negate the result of filter matches */
	union {			/* Filter definitions */
		wl_pkt_filter_pattern_t pattern;	/* Pattern matching filter */
		wl_pkt_filter_pattern_list_t patlist; /* List of patterns to match */
	} u;
} wl_pkt_filter_t;

/* IOVAR "tcp_keep_set" parameter. Used to install tcp keep_alive stuff. */
typedef struct wl_tcp_keep_set {
	uint32	val1;
	uint32	val2;
} wl_tcp_keep_set_t;

#define WL_PKT_FILTER_FIXED_LEN		  OFFSETOF(wl_pkt_filter_t, u)
#define WL_PKT_FILTER_PATTERN_FIXED_LEN	  OFFSETOF(wl_pkt_filter_pattern_t, mask_and_pattern)
#define WL_PKT_FILTER_PATTERN_LIST_FIXED_LEN OFFSETOF(wl_pkt_filter_pattern_list_t, patterns)
#define WL_PKT_FILTER_PATTERN_LISTEL_FIXED_LEN	\
			OFFSETOF(wl_pkt_filter_pattern_listel_t, mask_and_data)

/* IOVAR "pkt_filter_enable" parameter. */
typedef struct wl_pkt_filter_enable {
	uint32	id;		/* Unique filter id */
	uint32	enable;		/* Enable/disable bool */
} wl_pkt_filter_enable_t;

/* IOVAR "pkt_filter_list" parameter. Used to retrieve a list of installed filters. */
typedef struct wl_pkt_filter_list {
	uint32	num;		/* Number of installed packet filters */
	wl_pkt_filter_t	filter[1];	/* Variable array of packet filters. */
} wl_pkt_filter_list_t;

#define WL_PKT_FILTER_LIST_FIXED_LEN	  OFFSETOF(wl_pkt_filter_list_t, filter)

/* IOVAR "pkt_filter_stats" parameter. Used to retrieve debug statistics. */
typedef struct wl_pkt_filter_stats {
	uint32	num_pkts_matched;	/* # filter matches for specified filter id */
	uint32	num_pkts_forwarded;	/* # packets fwded from dongle to host for all filters */
	uint32	num_pkts_discarded;	/* # packets discarded by dongle for all filters */
} wl_pkt_filter_stats_t;

/* IOVAR "pkt_filter_ports" parameter.  Configure TCP/UDP port filters. */
typedef struct wl_pkt_filter_ports {
	uint8 version;		/* Be proper */
	uint8 reserved;		/* Be really proper */
	uint16 count;		/* Number of ports following */
	/* End of fixed data */
	uint16 ports[1];	/* Placeholder for ports[<count>] */
} wl_pkt_filter_ports_t;

#define WL_PKT_FILTER_PORTS_FIXED_LEN	OFFSETOF(wl_pkt_filter_ports_t, ports)

#define WL_PKT_FILTER_PORTS_VERSION	0
#define WL_PKT_FILTER_PORTS_MAX		128

#define RSN_KCK_LENGTH 16
#define RSN_KEK_LENGTH 16
#define RSN_REPLAY_LEN 8
typedef struct _gtkrefresh {
	uchar	KCK[RSN_KCK_LENGTH];
	uchar	KEK[RSN_KEK_LENGTH];
	uchar	ReplayCounter[RSN_REPLAY_LEN];
} gtk_keyinfo_t, *pgtk_keyinfo_t;

/* Sequential Commands ioctl */
typedef struct wl_seq_cmd_ioctl {
	uint32 cmd;		/* common ioctl definition */
	uint32 len;		/* length of user buffer */
} wl_seq_cmd_ioctl_t;

#define WL_SEQ_CMD_ALIGN_BYTES	4

/* These are the set of get IOCTLs that should be allowed when using
 * IOCTL sequence commands. These are issued implicitly by wl.exe each time
 * it is invoked. We never want to buffer these, or else wl.exe will stop working.
 */
#define WL_SEQ_CMDS_GET_IOCTL_FILTER(cmd) \
	(((cmd) == WLC_GET_MAGIC)		|| \
	 ((cmd) == WLC_GET_VERSION)		|| \
	 ((cmd) == WLC_GET_AP)			|| \
	 ((cmd) == WLC_GET_INSTANCE))

typedef struct wl_pkteng {
	uint32 flags;
	uint32 delay;			/* Inter-packet delay */
	uint32 nframes;			/* Number of frames */
	uint32 length;			/* Packet length */
	uint8  seqno;			/* Enable/disable sequence no. */
	struct ether_addr dest;		/* Destination address */
	struct ether_addr src;		/* Source address */
} wl_pkteng_t;

typedef struct wl_pkteng_stats {
	uint32 lostfrmcnt;		/* RX PER test: no of frames lost (skip seqno) */
	int32 rssi;			/* RSSI */
	int32 snr;			/* signal to noise ratio */
	uint16 rxpktcnt[NUM_80211_RATES+1];
	uint8 rssi_qdb;			/* qdB portion of the computed rssi */
} wl_pkteng_stats_t;

typedef struct wl_txcal_params {
	wl_pkteng_t pkteng;
	uint8 gidx_start;
	int8 gidx_step;
	uint8 gidx_stop;
} wl_txcal_params_t;

#if !defined(BCMDONGLEHOST) || defined(WLTEST)
typedef struct wl_sslpnphy_papd_debug_data {
	uint8 psat_pwr;
	uint8 psat_indx;
	uint8 final_idx;
	uint8 start_idx;
	int32 min_phase;
	int32 voltage;
	int8 temperature;
} wl_sslpnphy_papd_debug_data_t;
typedef struct wl_sslpnphy_debug_data {
	int16 papdcompRe [64];
	int16 papdcompIm [64];
} wl_sslpnphy_debug_data_t;
typedef struct wl_sslpnphy_spbdump_data {
	uint16 tbl_length;
	int16 spbreal[256];
	int16 spbimg[256];
} wl_sslpnphy_spbdump_data_t;
typedef struct wl_sslpnphy_percal_debug_data {
	uint cur_idx;
	uint tx_drift;
	uint8 prev_cal_idx;
	uint percal_ctr;
	int nxt_cal_idx;
	uint force_1idxcal;
	uint onedxacl_req;
	int32 last_cal_volt;
	int8 last_cal_temp;
	uint vbat_ripple;
	uint exit_route;
	int32 volt_winner;
} wl_sslpnphy_percal_debug_data_t;
#endif 

typedef enum {
	wowl_pattern_type_bitmap = 0,
	wowl_pattern_type_arp,
	wowl_pattern_type_na
} wowl_pattern_type_t;

typedef struct wl_wowl_pattern {
	uint32		    masksize;		/* Size of the mask in #of bytes */
	uint32		    offset;		/* Pattern byte offset in packet */
	uint32		    patternoffset;	/* Offset of start of pattern in the structure */
	uint32		    patternsize;	/* Size of the pattern itself in #of bytes */
	uint32		    id;			/* id */
	uint32		    reasonsize;		/* Size of the wakeup reason code */
	wowl_pattern_type_t type;		/* Type of pattern */
	/* Mask follows the structure above */
	/* Pattern follows the mask is at 'patternoffset' from the start */
} wl_wowl_pattern_t;

typedef struct wl_wowl_pattern_list {
	uint			count;
	wl_wowl_pattern_t	pattern[1];
} wl_wowl_pattern_list_t;

typedef struct wl_wowl_wakeind {
	uint8	pci_wakeind;	/* Whether PCI PMECSR PMEStatus bit was set */
	uint32	ucode_wakeind;	/* What wakeup-event indication was set by ucode */
} wl_wowl_wakeind_t;

typedef struct {
	uint32		pktlen;		    /* size of packet */
	void		*sdu;
} tcp_keepalive_wake_pkt_infop_t;

/* per AC rate control related data structure */
typedef struct wl_txrate_class {
	uint8		init_rate;
	uint8		min_rate;
	uint8		max_rate;
} wl_txrate_class_t;

/* structure for Overlap BSS scan arguments */
typedef struct wl_obss_scan_arg {
	int16	passive_dwell;
	int16	active_dwell;
	int16	bss_widthscan_interval;
	int16	passive_total;
	int16	active_total;
	int16	chanwidth_transition_delay;
	int16	activity_threshold;
} wl_obss_scan_arg_t;

#define WL_OBSS_SCAN_PARAM_LEN	sizeof(wl_obss_scan_arg_t)

/* RSSI event notification configuration. */
typedef struct wl_rssi_event {
	uint32 rate_limit_msec;		/* # of events posted to application will be limited to
					 * one per specified period (0 to disable rate limit).
					 */
	uint8 num_rssi_levels;		/* Number of entries in rssi_levels[] below */
	int8 rssi_levels[MAX_RSSI_LEVELS];	/* Variable number of RSSI levels. An event
						 * will be posted each time the RSSI of received
						 * beacons/packets crosses a level.
						 */
} wl_rssi_event_t;

/* CCA based channel quality event configuration */
#define WL_CHAN_QUAL_CCA	0
#define WL_CHAN_QUAL_NF		1
#define WL_CHAN_QUAL_NF_LTE	2
#define WL_CHAN_QUAL_TOTAL	3

#define MAX_CHAN_QUAL_LEVELS	8

typedef struct wl_chan_qual_metric {
	uint8 id;				/* metric ID */
	uint8 num_levels;               	/* Number of entries in rssi_levels[] below */
	uint16 flags;
	int16 htol[MAX_CHAN_QUAL_LEVELS];	/* threshold level array: hi-to-lo */
	int16 ltoh[MAX_CHAN_QUAL_LEVELS];	/* threshold level array: lo-to-hi */
} wl_chan_qual_metric_t;

typedef struct wl_chan_qual_event {
	uint32 rate_limit_msec;		/* # of events posted to application will be limited to
					 * one per specified period (0 to disable rate limit).
					 */
	uint16 flags;
	uint16 num_metrics;
	wl_chan_qual_metric_t metric[WL_CHAN_QUAL_TOTAL];	/* metric array */
} wl_chan_qual_event_t;

typedef struct wl_action_obss_coex_req {
	uint8 info;
	uint8 num;
	uint8 ch_list[1];
} wl_action_obss_coex_req_t;


/* IOVar parameter block for small MAC address array with type indicator */
#define WL_IOV_MAC_PARAM_LEN  4

#define WL_IOV_PKTQ_LOG_PRECS 16

typedef BWL_PRE_PACKED_STRUCT struct {
	uint32 num_addrs;
	char   addr_type[WL_IOV_MAC_PARAM_LEN];
	struct ether_addr ea[WL_IOV_MAC_PARAM_LEN];
} BWL_POST_PACKED_STRUCT wl_iov_mac_params_t;

/* This is extra info that follows wl_iov_mac_params_t */
typedef BWL_PRE_PACKED_STRUCT struct {
	uint32 addr_info[WL_IOV_MAC_PARAM_LEN];
} BWL_POST_PACKED_STRUCT wl_iov_mac_extra_params_t;

/* Combined structure */
typedef struct {
	wl_iov_mac_params_t params;
	wl_iov_mac_extra_params_t extra_params;
} wl_iov_mac_full_params_t;

/* Parameter block for PKTQ_LOG statistics */
#define PKTQ_LOG_COUNTERS_V4 \
	/* packets requested to be stored */ \
	uint32 requested; \
	/* packets stored */ \
	uint32 stored; \
	/* packets saved, because a lowest priority queue has given away one packet */ \
	uint32 saved; \
	/* packets saved, because an older packet from the same queue has been dropped */ \
	uint32 selfsaved; \
	/* packets dropped, because pktq is full with higher precedence packets */ \
	uint32 full_dropped; \
	 /* packets dropped because pktq per that precedence is full */ \
	uint32 dropped; \
	/* packets dropped, in order to save one from a queue of a highest priority */ \
	uint32 sacrificed; \
	/* packets droped because of hardware/transmission error */ \
	uint32 busy; \
	/* packets re-sent because they were not received */ \
	uint32 retry; \
	/* packets retried again (ps pretend) prior to moving power save mode */ \
	uint32 ps_retry; \
	 /* suppressed packet count */ \
	uint32 suppress; \
	/* packets finally dropped after retry limit */ \
	uint32 retry_drop; \
	/* the high-water mark of the queue capacity for packets - goes to zero as queue fills */ \
	uint32 max_avail; \
	/* the high-water mark of the queue utilisation for packets - ('inverse' of max_avail) */ \
	uint32 max_used; \
	 /* the maximum capacity of the queue */ \
	uint32 queue_capacity; \
	/* count of rts attempts that failed to receive cts */ \
	uint32 rtsfail; \
	/* count of packets sent (acked) successfully */ \
	uint32 acked; \
	/* running total of phy rate of packets sent successfully */ \
	uint32 txrate_succ; \
	/* running total of phy 'main' rate */ \
	uint32 txrate_main; \
	/* actual data transferred successfully */ \
	uint32 throughput; \
	/* time difference since last pktq_stats */ \
	uint32 time_delta;

typedef struct {
	PKTQ_LOG_COUNTERS_V4
} pktq_log_counters_v04_t;

/* v5 is the same as V4 with extra parameter */
typedef struct {
	PKTQ_LOG_COUNTERS_V4
	/* cumulative time to transmit */
	uint32 airtime;
} pktq_log_counters_v05_t;

typedef struct {
	uint8                num_prec[WL_IOV_MAC_PARAM_LEN];
	pktq_log_counters_v04_t  counters[WL_IOV_MAC_PARAM_LEN][WL_IOV_PKTQ_LOG_PRECS];
	uint32               counter_info[WL_IOV_MAC_PARAM_LEN];
	uint32               pspretend_time_delta[WL_IOV_MAC_PARAM_LEN];
	char                 headings[1];
} pktq_log_format_v04_t;

typedef struct {
	uint8                num_prec[WL_IOV_MAC_PARAM_LEN];
	pktq_log_counters_v05_t  counters[WL_IOV_MAC_PARAM_LEN][WL_IOV_PKTQ_LOG_PRECS];
	uint32               counter_info[WL_IOV_MAC_PARAM_LEN];
	uint32               pspretend_time_delta[WL_IOV_MAC_PARAM_LEN];
	char                 headings[1];
} pktq_log_format_v05_t;


typedef struct {
	uint32               version;
	wl_iov_mac_params_t  params;
	union {
		pktq_log_format_v04_t v04;
		pktq_log_format_v05_t v05;
	} pktq_log;
} wl_iov_pktq_log_t;

/* PKTQ_LOG_AUTO, PKTQ_LOG_DEF_PREC flags introduced in v05, they are ignored by v04 */
#define PKTQ_LOG_AUTO     (1 << 31)
#define PKTQ_LOG_DEF_PREC (1 << 30)

/*
 * SCB_BS_DATA iovar definitions start.
 */
#define SCB_BS_DATA_STRUCT_VERSION	1

/* The actual counters maintained for each station */
typedef BWL_PRE_PACKED_STRUCT struct {
	/* The following counters are a subset of what pktq_stats provides per precedence. */
	uint32 retry;          /* packets re-sent because they were not received */
	uint32 retry_drop;     /* packets finally dropped after retry limit */
	uint32 rtsfail;        /* count of rts attempts that failed to receive cts */
	uint32 acked;          /* count of packets sent (acked) successfully */
	uint32 txrate_succ;    /* running total of phy rate of packets sent successfully */
	uint32 txrate_main;    /* running total of phy 'main' rate */
	uint32 throughput;     /* actual data transferred successfully */
	uint32 time_delta;     /* time difference since last pktq_stats */
	uint32 airtime;        /* cumulative total medium access delay in useconds */
} BWL_POST_PACKED_STRUCT iov_bs_data_counters_t;

/* The structure for individual station information. */
typedef BWL_PRE_PACKED_STRUCT struct {
	struct ether_addr	station_address;	/* The station MAC address */
	uint16			station_flags;		/* Bit mask of flags, for future use. */
	iov_bs_data_counters_t	station_counters;	/* The actual counter values */
} BWL_POST_PACKED_STRUCT iov_bs_data_record_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	uint16	structure_version;	/* Structure version number (for wl/wlu matching) */
	uint16	structure_count;	/* Number of iov_bs_data_record_t records following */
	iov_bs_data_record_t	structure_record[1];	/* 0 - structure_count records */
} BWL_POST_PACKED_STRUCT iov_bs_data_struct_t;

/* Bitmask of options that can be passed in to the iovar. */
enum {
	SCB_BS_DATA_FLAG_NO_RESET = (1<<0)	/* Do not clear the counters after reading */
};
/*
 * SCB_BS_DATA iovar definitions end.
 */

typedef struct wlc_extlog_cfg {
	int max_number;
	uint16 module;	/* bitmap */
	uint8 level;
	uint8 flag;
	uint16 version;
} wlc_extlog_cfg_t;

typedef struct log_record {
	uint32 time;
	uint16 module;
	uint16 id;
	uint8 level;
	uint8 sub_unit;
	uint8 seq_num;
	int32 arg;
	char str[MAX_ARGSTR_LEN];
} log_record_t;

typedef struct wlc_extlog_req {
	uint32 from_last;
	uint32 num;
} wlc_extlog_req_t;

typedef struct wlc_extlog_results {
	uint16 version;
	uint16 record_len;
	uint32 num;
	log_record_t logs[1];
} wlc_extlog_results_t;

typedef struct log_idstr {
	uint16	id;
	uint16	flag;
	uint8	arg_type;
	const char	*fmt_str;
} log_idstr_t;

#define FMTSTRF_USER		1

/* flat ID definitions
 * New definitions HAVE TO BE ADDED at the end of the table. Otherwise, it will
 * affect backward compatibility with pre-existing apps
 */
typedef enum {
	FMTSTR_DRIVER_UP_ID = 0,
	FMTSTR_DRIVER_DOWN_ID = 1,
	FMTSTR_SUSPEND_MAC_FAIL_ID = 2,
	FMTSTR_NO_PROGRESS_ID = 3,
	FMTSTR_RFDISABLE_ID = 4,
	FMTSTR_REG_PRINT_ID = 5,
	FMTSTR_EXPTIME_ID = 6,
	FMTSTR_JOIN_START_ID = 7,
	FMTSTR_JOIN_COMPLETE_ID = 8,
	FMTSTR_NO_NETWORKS_ID = 9,
	FMTSTR_SECURITY_MISMATCH_ID = 10,
	FMTSTR_RATE_MISMATCH_ID = 11,
	FMTSTR_AP_PRUNED_ID = 12,
	FMTSTR_KEY_INSERTED_ID = 13,
	FMTSTR_DEAUTH_ID = 14,
	FMTSTR_DISASSOC_ID = 15,
	FMTSTR_LINK_UP_ID = 16,
	FMTSTR_LINK_DOWN_ID = 17,
	FMTSTR_RADIO_HW_OFF_ID = 18,
	FMTSTR_RADIO_HW_ON_ID = 19,
	FMTSTR_EVENT_DESC_ID = 20,
	FMTSTR_PNP_SET_POWER_ID = 21,
	FMTSTR_RADIO_SW_OFF_ID = 22,
	FMTSTR_RADIO_SW_ON_ID = 23,
	FMTSTR_PWD_MISMATCH_ID = 24,
	FMTSTR_FATAL_ERROR_ID = 25,
	FMTSTR_AUTH_FAIL_ID = 26,
	FMTSTR_ASSOC_FAIL_ID = 27,
	FMTSTR_IBSS_FAIL_ID = 28,
	FMTSTR_EXTAP_FAIL_ID = 29,
	FMTSTR_MAX_ID
} log_fmtstr_id_t;

#ifdef DONGLEOVERLAYS
typedef struct {
	uint32 flags_idx;	/* lower 8 bits: overlay index; upper 24 bits: flags */
	uint32 offset;		/* offset into overlay region to write code */
	uint32 len;			/* overlay code len */
	/* overlay code follows this struct */
} wl_ioctl_overlay_t;
#endif /* DONGLEOVERLAYS */

#endif /* LINUX_POSTMOGRIFY_REMOVAL */

/* 11k Neighbor Report element */
typedef struct nbr_element {
	uint8 id;
	uint8 len;
	struct ether_addr bssid;
	uint32 bssid_info;
	uint8 reg;
	uint8 channel;
	uint8 phytype;
	uint8 pad;
} nbr_element_t;

/* no default structure packing */
#include <packed_section_end.h>

typedef struct keepalives_max_idle {
	uint16  keepalive_count;        /* nmbr of keepalives per bss_max_idle period */
	uint8   mkeepalive_index;       /* mkeepalive_index for keepalive frame to be used */
	uint8   PAD;    /* to align next field */
	uint16  max_interval;           /* seconds */
} keepalives_max_idle_t;

#define PM_IGNORE_BCMC_PROXY_ARP (1 << 0)
#define PM_IGNORE_BCMC_ALL_DMS_ACCEPTED (1 << 1)
/* require strict packing */
#include <packed_section_start.h>

/* ##### Power Stats section ##### */

#define WL_PWRSTATS_VERSION	2

/* Input structure for pwrstats IOVAR */
typedef BWL_PRE_PACKED_STRUCT struct wl_pwrstats_query {
	uint16 length;		/* Number of entries in type array. */
	uint16 type[1];		/* Types (tags) to retrieve.
				 * Length 0 (no types) means get all.
				 */
} BWL_POST_PACKED_STRUCT wl_pwrstats_query_t;

/* This structure is for version 2; version 1 will be deprecated in by FW */
typedef BWL_PRE_PACKED_STRUCT struct wl_pwrstats {
	uint16 version;		      /* Version = 2 is TLV format */
	uint16 length;		      /* Length of entire structure */
	uint8 data[1];		      /* TLV data, a series of structures,
				       * each starting with type and length.
				       *
				       * Padded as necessary so each section
				       * starts on a 4-byte boundary.
				       *
				       * Both type and len are uint16, but the
				       * upper nibble of length is reserved so
				       * valid len values are 0-4095.
				       */
} BWL_POST_PACKED_STRUCT wl_pwrstats_t;
#define WL_PWR_STATS_HDRLEN	OFFSETOF(wl_pwrstats_t, data)

/* Type values for the data section */
#define WL_PWRSTATS_TYPE_PHY		0 /* struct wl_pwr_phy_stats */
#define WL_PWRSTATS_TYPE_SCAN		1 /* struct wl_pwr_scan_stats */
/* Not ported to BISON yet for need of dependent usb infrastructure */
#define WL_PWRSTATS_TYPE_USB_HSIC	2 /* struct wl_pwr_usb_hsic_stats */
#define WL_PWRSTATS_TYPE_PM_AWAKE	3 /* struct wl_pwr_pm_awake_stats */
#define WL_PWRSTATS_TYPE_CONNECTION	4 /* struct wl_pwr_connect_stats; assoc and key-exch time */
#define WL_PWRSTATS_TYPE_AWDL		5 /* struct wl_pwr_awdl_stats; */
#define WL_PWRSTATS_TYPE_PCIE		6 /* struct wl_pwr_pcie_stats */

/* Bits for wake reasons */
#define WLC_PMD_WAKE_SET		0x1
#define WLC_PMD_PM_AWAKE_BCN		0x2
#define WLC_PMD_BTA_ACTIVE		0x4
#define WLC_PMD_SCAN_IN_PROGRESS	0x8
#define WLC_PMD_RM_IN_PROGRESS		0x10
#define WLC_PMD_AS_IN_PROGRESS		0x20
#define WLC_PMD_PM_PEND			0x40
#define WLC_PMD_PS_POLL			0x80
#define WLC_PMD_CHK_UNALIGN_TBTT	0x100
#define WLC_PMD_APSD_STA_UP		0x200
#define WLC_PMD_TX_PEND_WAR		0x400
#define WLC_PMD_GPTIMER_STAY_AWAKE	0x800
#define WLC_PMD_AWDL_AWAKE		0x1000
#define WLC_PMD_PM2_RADIO_SOFF_PEND	0x2000
#define WLC_PMD_NON_PRIM_STA_UP		0x4000
#define WLC_PMD_AP_UP			0x8000

typedef BWL_PRE_PACKED_STRUCT struct wlc_pm_debug {
	uint32 timestamp;	     /* timestamp in millisecond */
	uint32 reason;		     /* reason(s) for staying awake */
} BWL_POST_PACKED_STRUCT wlc_pm_debug_t;

/* Data sent as part of pwrstats IOVAR */
typedef BWL_PRE_PACKED_STRUCT struct pm_awake_data {
	uint32 curr_time;	/* ms */
	uint32 hw_macc;		/* HW maccontrol */
	uint32 sw_macc;		/* SW maccontrol */
	uint32 pm_dur;		/* Total sleep time in PM, usecs */
	uint32 mpc_dur;		/* Total sleep time in MPC, usecs */

	/* int32 drifts = remote - local; +ve drift => local-clk slow */
	int32 last_drift;	/* Most recent TSF drift from beacon */
	int32 min_drift;	/* Min TSF drift from beacon in magnitude */
	int32 max_drift;	/* Max TSF drift from beacon in magnitude */

	uint32 avg_drift;	/* Avg TSF drift from beacon */

	/* Wake history tracking */

	/* pmstate array (type wlc_pm_debug_t) start offset */
	uint16 pm_state_offset;
	/* pmstate number of array entries */
	uint16 pm_state_len;

	/* array (type uint32) start offset */
	uint16 pmd_event_wake_dur_offset;
	/* pmd_event_wake_dur number of array entries */
	uint16 pmd_event_wake_dur_len;

	uint32 drift_cnt;	/* Count of drift readings over which avg_drift was computed */
	uint8  pmwake_idx;	/* for stepping through pm_state */
	uint8  pad[3];
	uint32 frts_time;	/* Cumulative ms spent in frts since driver load */
} BWL_POST_PACKED_STRUCT pm_awake_data_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_pwr_pm_awake_stats {
	uint16 type;	     /* WL_PWRSTATS_TYPE_PM_AWAKE */
	uint16 len;	     /* Up to 4K-1, top 4 bits are reserved */

	pm_awake_data_t awake_data;
} BWL_POST_PACKED_STRUCT wl_pwr_pm_awake_stats_t;

/* Original bus structure is for HSIC */
typedef BWL_PRE_PACKED_STRUCT struct bus_metrics {
	uint32 suspend_ct;	/* suspend count */
	uint32 resume_ct;	/* resume count */
	uint32 disconnect_ct;	/* disconnect count */
	uint32 reconnect_ct;	/* reconnect count */
	uint32 active_dur;	/* msecs in bus, usecs for user */
	uint32 suspend_dur;	/* msecs in bus, usecs for user */
	uint32 disconnect_dur;	/* msecs in bus, usecs for user */
} BWL_POST_PACKED_STRUCT bus_metrics_t;

/* Bus interface info for USB/HSIC */
typedef BWL_PRE_PACKED_STRUCT struct wl_pwr_usb_hsic_stats {
	uint16 type;	     /* WL_PWRSTATS_TYPE_USB_HSIC */
	uint16 len;	     /* Up to 4K-1, top 4 bits are reserved */

	bus_metrics_t hsic;	/* stats from hsic bus driver */
} BWL_POST_PACKED_STRUCT wl_pwr_usb_hsic_stats_t;

typedef BWL_PRE_PACKED_STRUCT struct pcie_bus_metrics {
	uint32 d3_suspend_ct;	/* suspend count */
	uint32 d0_resume_ct;	/* resume count */
	uint32 perst_assrt_ct;	/* PERST# assert count */
	uint32 perst_deassrt_ct;	/* PERST# de-assert count */
	uint32 active_dur;	/* msecs */
	uint32 d3_suspend_dur;	/* msecs */
	uint32 perst_dur;	/* msecs */
	uint32 l0_cnt;		/* L0 entry count */
	uint32 l0_usecs;	/* L0 duration in usecs */
	uint32 l1_cnt;		/* L1 entry count */
	uint32 l1_usecs;	/* L1 duration in usecs */
	uint32 l1_1_cnt;	/* L1_1ss entry count */
	uint32 l1_1_usecs;	/* L1_1ss duration in usecs */
	uint32 l1_2_cnt;	/* L1_2ss entry count */
	uint32 l1_2_usecs;	/* L1_2ss duration in usecs */
	uint32 l2_cnt;		/* L2 entry count */
	uint32 l2_usecs;	/* L2 duration in usecs */
	uint32 timestamp;	/* Timestamp on when stats are collected */
	uint32 num_h2d_doorbell;	/* # of doorbell interrupts - h2d */
	uint32 num_d2h_doorbell;	/* # of doorbell interrupts - d2h */
	uint32 num_submissions; /* # of submissions */
	uint32 num_completions; /* # of completions */
	uint32 num_rxcmplt;	/* # of rx completions */
	uint32 num_rxcmplt_drbl;	/* of drbl interrupts for rx complt. */
	uint32 num_txstatus;	/* # of tx completions */
	uint32 num_txstatus_drbl;	/* of drbl interrupts for tx complt. */
} BWL_POST_PACKED_STRUCT pcie_bus_metrics_t;

/* Bus interface info for PCIE */
typedef BWL_PRE_PACKED_STRUCT struct wl_pwr_pcie_stats {
	uint16 type;	     /* WL_PWRSTATS_TYPE_PCIE */
	uint16 len;	     /* Up to 4K-1, top 4 bits are reserved */

	pcie_bus_metrics_t pcie;	/* stats from pcie bus driver */
} BWL_POST_PACKED_STRUCT wl_pwr_pcie_stats_t;

/* Scan information history per category */
typedef BWL_PRE_PACKED_STRUCT struct scan_data {
	uint32 count;		/* Number of scans performed */
	uint32 dur;		/* Total time (in us) used */
} BWL_POST_PACKED_STRUCT scan_data_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_pwr_scan_stats {
	uint16 type;	     /* WL_PWRSTATS_TYPE_SCAN */
	uint16 len;	     /* Up to 4K-1, top 4 bits are reserved */

	/* Scan history */
	scan_data_t user_scans;	  /* User-requested scans: (i/e/p)scan */
	scan_data_t assoc_scans;  /* Scans initiated by association requests */
	scan_data_t roam_scans;	  /* Scans initiated by the roam engine */
	scan_data_t pno_scans[8]; /* For future PNO bucketing (BSSID, SSID, etc) */
	scan_data_t other_scans;  /* Scan engine usage not assigned to the above */
} BWL_POST_PACKED_STRUCT wl_pwr_scan_stats_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_pwr_connect_stats {
	uint16 type;	     /* WL_PWRSTATS_TYPE_SCAN */
	uint16 len;	     /* Up to 4K-1, top 4 bits are reserved */

	/* Connection (Association + Key exchange) data */
	uint32 count;	/* Number of connections performed */
	uint32 dur;		/* Total time (in ms) used */
} BWL_POST_PACKED_STRUCT wl_pwr_connect_stats_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_pwr_phy_stats {
	uint16 type;	    /* WL_PWRSTATS_TYPE_PHY */
	uint16 len;	    /* Up to 4K-1, top 4 bits are reserved */
	uint32 tx_dur;	    /* TX Active duration in us */
	uint32 rx_dur;	    /* RX Active duration in us */
} BWL_POST_PACKED_STRUCT wl_pwr_phy_stats_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_pwr_awdl_stats {
	uint16 type;	    /* WL_PWRSTATS_TYPE_AWDL */
	uint16 len;	    /* Up to 4K-1, top 4 bits are reserved */
	uint32 tx_dur;	    /* AWDL TX Active duration in usec */
	uint32 rx_dur;	    /* AWDL RX Active duration in usec */
	uint32 aw_dur;	    /* AWDL AW duration in msec */
	uint32 awpscan_dur; /* AWDL pscans dur in msec */
} BWL_POST_PACKED_STRUCT wl_pwr_awdl_stats_t;

/* ##### End of Power Stats section ##### */

typedef BWL_PRE_PACKED_STRUCT struct wl_pfn_roam_thresh {
	uint32 pfn_alert_thresh;
	uint32 roam_alert_thresh;
} BWL_POST_PACKED_STRUCT wl_pfn_roam_thresh_t;

/* Reasons for wl_pmalert_t */
#define PM_DUR_EXCEEDED			(1<<0)
#define MPC_DUR_EXCEEDED		(1<<1)
#define ROAM_ALERT_THRESH_EXCEEDED	(1<<2)
#define PFN_ALERT_THRESH_EXCEEDED	(1<<3)
#define CONST_AWAKE_DUR_ALERT           (1<<4)
#define CONST_AWAKE_DUR_RECOVERY        (1<<5)

#define MIN_PM_ALERT_LEN 9

/* Data sent in EXCESS_PM_WAKE event */
#define WL_PM_ALERT_VERSION 3

#define MAX_P2P_BSS_DTIM_PRD 4

/* This structure is for version 3; version 2 will be deprecated in by FW */
typedef BWL_PRE_PACKED_STRUCT struct wl_pmalert {
	uint16 version;		/* Version = 3 is TLV format */
	uint16 length;		/* Length of entire structure */
	uint32 reasons;		/* reason(s) for pm_alert */
	uint8 data[1];		/* TLV data, a series of structures,
				 * each starting with type and length.
				 *
				 * Padded as necessary so each section
				 * starts on a 4-byte boundary.
				 *
				 * Both type and len are uint16, but the
				 * upper nibble of length is reserved so
				 * valid len values are 0-4095.
				*/
} BWL_POST_PACKED_STRUCT wl_pmalert_t;

/* Type values for the data section */
#define WL_PMALERT_FIXED	0 /* struct wl_pmalert_fixed_t, fixed fields */
#define WL_PMALERT_PMSTATE	1 /* struct wl_pmalert_pmstate_t, variable */
#define WL_PMALERT_EVENT_DUR	2 /* struct wl_pmalert_event_dur_t, variable */
#define WL_PMALERT_UCODE_DBG	3 /* struct wl_pmalert_ucode_dbg_t, variable */

typedef BWL_PRE_PACKED_STRUCT struct wl_pmalert_fixed {
	uint16 type;	     /* WL_PMALERT_FIXED */
	uint16 len;	     /* Up to 4K-1, top 4 bits are reserved */
	uint32 prev_stats_time;	/* msecs */
	uint32 curr_time;	/* ms */
	uint32 prev_pm_dur;	/* usecs */
	uint32 pm_dur;		/* Total sleep time in PM, usecs */
	uint32 prev_mpc_dur;	/* usecs */
	uint32 mpc_dur;		/* Total sleep time in MPC, usecs */
	uint32 hw_macc;		/* HW maccontrol */
	uint32 sw_macc;		/* SW maccontrol */

	/* int32 drifts = remote - local; +ve drift -> local-clk slow */
	int32 last_drift;	/* Most recent TSF drift from beacon */
	int32 min_drift;	/* Min TSF drift from beacon in magnitude */
	int32 max_drift;	/* Max TSF drift from beacon in magnitude */

	uint32 avg_drift;	/* Avg TSF drift from beacon */
	uint32 drift_cnt;	/* Count of drift readings over which avg_drift was computed */
	uint32 frts_time;	/* Cumulative ms spent in frts since driver load */
} BWL_POST_PACKED_STRUCT wl_pmalert_fixed_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_pmalert_pmstate {
	uint16 type;	     /* WL_PMALERT_PMSTATE */
	uint16 len;	     /* Up to 4K-1, top 4 bits are reserved */

	uint8 pmwake_idx;   /* for stepping through pm_state */
	uint8 pad[3];
	/* Array of pmstate; len of array is based on tlv len */
	wlc_pm_debug_t pmstate[1];
} BWL_POST_PACKED_STRUCT wl_pmalert_pmstate_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_pmalert_event_dur {
	uint16 type;	     /* WL_PMALERT_EVENT_DUR */
	uint16 len;	     /* Up to 4K-1, top 4 bits are reserved */

	/* Array of event_dur, len of array is based on tlv len */
	uint32 event_dur[1];
} BWL_POST_PACKED_STRUCT wl_pmalert_event_dur_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_pmalert_ucode_dbg {
	uint16 type;	     /* WL_PMALERT_UCODE_DBG */
	uint16 len;	     /* Up to 4K-1, top 4 bits are reserved */
	uint32 macctrl;
	uint16 m_p2p_hps;
	uint32 psm_brc;
	uint32 ifsstat;
	uint16 m_p2p_bss_dtim_prd[MAX_P2P_BSS_DTIM_PRD];
	uint32 psmdebug[20];
	uint32 phydebug[20];
} BWL_POST_PACKED_STRUCT wl_pmalert_ucode_dbg_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL

/* Structures and constants used for "vndr_ie" IOVar interface */
#define VNDR_IE_CMD_LEN		4	/* length of the set command string:
					 * "add", "del" (+ NUL)
					 */

#define VNDR_IE_INFO_HDR_LEN	(sizeof(uint32))

typedef BWL_PRE_PACKED_STRUCT struct {
	uint32 pktflag;			/* bitmask indicating which packet(s) contain this IE */
	vndr_ie_t vndr_ie_data;		/* vendor IE data */
} BWL_POST_PACKED_STRUCT vndr_ie_info_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	int iecount;			/* number of entries in the vndr_ie_list[] array */
	vndr_ie_info_t vndr_ie_list[1];	/* variable size list of vndr_ie_info_t structs */
} BWL_POST_PACKED_STRUCT vndr_ie_buf_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	char cmd[VNDR_IE_CMD_LEN];	/* vndr_ie IOVar set command : "add", "del" + NUL */
	vndr_ie_buf_t vndr_ie_buffer;	/* buffer containing Vendor IE list information */
} BWL_POST_PACKED_STRUCT vndr_ie_setbuf_t;

/* tag_ID/length/value_buffer tuple */
typedef BWL_PRE_PACKED_STRUCT struct {
	uint8	id;
	uint8	len;
	uint8	data[1];
} BWL_POST_PACKED_STRUCT tlv_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	uint32 pktflag;			/* bitmask indicating which packet(s) contain this IE */
	tlv_t ie_data;		/* IE data */
} BWL_POST_PACKED_STRUCT ie_info_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	int iecount;			/* number of entries in the ie_list[] array */
	ie_info_t ie_list[1];	/* variable size list of ie_info_t structs */
} BWL_POST_PACKED_STRUCT ie_buf_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	char cmd[VNDR_IE_CMD_LEN];	/* ie IOVar set command : "add" + NUL */
	ie_buf_t ie_buffer;	/* buffer containing IE list information */
} BWL_POST_PACKED_STRUCT ie_setbuf_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	uint32 pktflag;		/* bitmask indicating which packet(s) contain this IE */
	uint8 id;		/* IE type */
} BWL_POST_PACKED_STRUCT ie_getbuf_t;

/* structures used to define format of wps ie data from probe requests */
/* passed up to applications via iovar "prbreq_wpsie" */
typedef BWL_PRE_PACKED_STRUCT struct sta_prbreq_wps_ie_hdr {
	struct ether_addr staAddr;
	uint16 ieLen;
} BWL_POST_PACKED_STRUCT sta_prbreq_wps_ie_hdr_t;

typedef BWL_PRE_PACKED_STRUCT struct sta_prbreq_wps_ie_data {
	sta_prbreq_wps_ie_hdr_t hdr;
	uint8 ieData[1];
} BWL_POST_PACKED_STRUCT sta_prbreq_wps_ie_data_t;

typedef BWL_PRE_PACKED_STRUCT struct sta_prbreq_wps_ie_list {
	uint32 totLen;
	uint8 ieDataList[1];
} BWL_POST_PACKED_STRUCT sta_prbreq_wps_ie_list_t;


#ifdef WLMEDIA_TXFAILEVENT
typedef BWL_PRE_PACKED_STRUCT struct {
	char   dest[ETHER_ADDR_LEN]; /* destination MAC */
	uint8  prio;            /* Packet Priority */
	uint8  flags;           /* Flags           */
	uint32 tsf_l;           /* TSF timer low   */
	uint32 tsf_h;           /* TSF timer high  */
	uint16 rates;           /* Main Rates      */
	uint16 txstatus;        /* TX Status       */
} BWL_POST_PACKED_STRUCT txfailinfo_t;
#endif /* WLMEDIA_TXFAILEVENT */

typedef BWL_PRE_PACKED_STRUCT struct {
	uint32 flags;
	chanspec_t chanspec;			/* txpwr report for this channel */
	chanspec_t local_chanspec;		/* channel on which we are associated */
	uint8 local_max;			/* local max according to the AP */
	uint8 local_constraint;			/* local constraint according to the AP */
	int8  antgain[2];			/* Ant gain for each band - from SROM */
	uint8 rf_cores;				/* count of RF Cores being reported */
	uint8 est_Pout[4];			/* Latest tx power out estimate per RF chain */
	uint8 est_Pout_act[4]; /* Latest tx power out estimate per RF chain w/o adjustment */
	uint8 est_Pout_cck;			/* Latest CCK tx power out estimate */
	uint8 tx_power_max[4];		/* Maximum target power among all rates */
	uint tx_power_max_rate_ind[4];		/* Index of the rate with the max target power */
	int8 clm_limits[WL_NUMRATES];		/* regulatory limits - 20, 40 or 80MHz */
	int8 clm_limits_subchan1[WL_NUMRATES];	/* regulatory limits - 20in40 or 40in80 */
	int8 clm_limits_subchan2[WL_NUMRATES];	/* regulatory limits - 20in80MHz */
	int8 sar;					/* SAR limit for display by wl executable */
	int8 channel_bandwidth;		/* 20, 40 or 80 MHz bandwidth? */
	uint8 version;				/* Version of the data format wlu <--> driver */
	uint8 display_core;			/* Displayed curpower core */
	int8 target_offsets[4];		/* Target power offsets for current rate per core */
	uint32 last_tx_ratespec;	/* Ratespec for last transmition */
	uint   user_target;		/* user limit */
	uint32 ppr_len;		/* length of each ppr serialization buffer */
	uint32 pad;		/* pad for roml */
	int8 SARLIMIT[MAX_STREAMS_SUPPORTED];
	uint8  pprdata[1];		/* ppr serialization buffer */
} BWL_POST_PACKED_STRUCT tx_pwr_rpt_t;

#endif /* LINUX_POSTMOGRIFY_REMOVAL */

#define TXPWR_TARGET_VERSION  0
typedef BWL_PRE_PACKED_STRUCT struct {
	int32 version;		/* version number */
	chanspec_t chanspec;	/* txpwr report for this channel */
	int8 txpwr[WL_STA_ANT_MAX]; /* Max tx target power, in qdb */
	uint8 rf_cores;		/* count of RF Cores being reported */
} BWL_POST_PACKED_STRUCT txpwr_target_max_t;

#define BSS_PEER_INFO_PARAM_CUR_VER	0
/* Input structure for IOV_BSS_PEER_INFO */
typedef BWL_PRE_PACKED_STRUCT	struct {
	uint16			version;
	struct	ether_addr ea;	/* peer MAC address */
} BWL_POST_PACKED_STRUCT bss_peer_info_param_t;

#define BSS_PEER_INFO_CUR_VER		0

typedef BWL_PRE_PACKED_STRUCT struct {
	uint16			version;
	struct ether_addr	ea;
	int32			rssi;
	uint32			tx_rate;	/* current tx rate */
	uint32			rx_rate;	/* current rx rate */
	wl_rateset_t		rateset;	/* rateset in use */
	uint32			age;		/* age in seconds */
} BWL_POST_PACKED_STRUCT bss_peer_info_t;

#define BSS_PEER_LIST_INFO_CUR_VER	0

typedef BWL_PRE_PACKED_STRUCT struct {
	uint16			version;
	uint16			bss_peer_info_len;	/* length of bss_peer_info_t */
	uint32			count;			/* number of peer info */
	bss_peer_info_t		peer_info[1];		/* peer info */
} BWL_POST_PACKED_STRUCT bss_peer_list_info_t;

#define BSS_PEER_LIST_INFO_FIXED_LEN OFFSETOF(bss_peer_list_info_t, peer_info)

#define AIBSS_BCN_FORCE_CONFIG_VER_0	0

/* structure used to configure AIBSS beacon force xmit */
typedef BWL_PRE_PACKED_STRUCT struct {
	uint16  version;
	uint16	len;
	uint32 initial_min_bcn_dur;	/* dur in ms to check a bcn in bcn_flood period */
	uint32 min_bcn_dur;	/* dur in ms to check a bcn after bcn_flood period */
	uint32 bcn_flood_dur; /* Initial bcn xmit period in ms */
} BWL_POST_PACKED_STRUCT aibss_bcn_force_config_t;

#define AIBSS_TXFAIL_CONFIG_VER_0    0
/* structure used to configure aibss tx fail event */
typedef BWL_PRE_PACKED_STRUCT struct {
	uint16  version;
	uint16  len;
	uint32 bcn_timeout;     /* dur in seconds, send txfail if no bcn for this dur */
	uint32 max_tx_retry;     /* no of consecutive no acks to send txfail event */
} BWL_POST_PACKED_STRUCT aibss_txfail_config_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_aibss_if {
	uint16 version;
	uint16 len;
	uint32 flags;
	struct ether_addr addr;
	chanspec_t chspec;
} BWL_PRE_PACKED_STRUCT wl_aibss_if_t;

typedef BWL_PRE_PACKED_STRUCT struct wlc_ipfo_route_entry {
	struct ipv4_addr ip_addr;
	struct ether_addr nexthop;
} BWL_POST_PACKED_STRUCT wlc_ipfo_route_entry_t;

typedef BWL_PRE_PACKED_STRUCT struct wlc_ipfo_route_tbl {
	uint32 num_entry;
	wlc_ipfo_route_entry_t route_entry[1];
} BWL_POST_PACKED_STRUCT wlc_ipfo_route_tbl_t;

#define WL_IPFO_ROUTE_TBL_FIXED_LEN 4
#define WL_MAX_IPFO_ROUTE_TBL_ENTRY	64

/* no strict structure packing */
#include <packed_section_end.h>

#ifndef LINUX_POSTMOGRIFY_REMOVAL
	/* Global ASSERT Logging */
#define ASSERTLOG_CUR_VER	0x0100
#define MAX_ASSRTSTR_LEN	64

	typedef struct assert_record {
		uint32 time;
		uint8 seq_num;
		char str[MAX_ASSRTSTR_LEN];
	} assert_record_t;

	typedef struct assertlog_results {
		uint16 version;
		uint16 record_len;
		uint32 num;
		assert_record_t logs[1];
	} assertlog_results_t;

#define LOGRRC_FIX_LEN	8
#define IOBUF_ALLOWED_NUM_OF_LOGREC(type, len) ((len - LOGRRC_FIX_LEN)/sizeof(type))


	/* chanim acs record */
	typedef struct {
		bool valid;
		uint8 trigger;
		chanspec_t selected_chspc;
		int8 bgnoise;
		uint32 glitch_cnt;
		uint8 ccastats;
		uint8 chan_idle;
		uint timestamp;
	} chanim_acs_record_t;

	typedef struct {
		chanim_acs_record_t acs_record[CHANIM_ACS_RECORD];
		uint8 count;
		uint timestamp;
	} wl_acs_record_t;

	typedef struct chanim_stats {
		uint32 glitchcnt;               /* normalized as per second count */
		uint32 badplcp;                 /* normalized as per second count */
		uint8 ccastats[CCASTATS_MAX];   /* normalized as 0-255 */
		int8 bgnoise;			/* background noise level (in dBm) */
		chanspec_t chanspec;
		uint32 timestamp;
		uint32 bphy_glitchcnt;          /* normalized as per second count */
		uint32 bphy_badplcp;            /* normalized as per second count */
		uint8 chan_idle;                /* normalized as 0~255 */
	} chanim_stats_t;

#define WL_CHANIM_STATS_VERSION 2

typedef struct {
	uint32 buflen;
	uint32 version;
	uint32 count;
	chanim_stats_t stats[1];
} wl_chanim_stats_t;

#define WL_CHANIM_STATS_FIXED_LEN OFFSETOF(wl_chanim_stats_t, stats)

/* Noise measurement metrics. */
#define NOISE_MEASURE_KNOISE	0x1

/* scb probe parameter */
typedef struct {
	uint32 scb_timeout;
	uint32 scb_activity_time;
	uint32 scb_max_probe;
} wl_scb_probe_t;

/* structure/defines for selective mgmt frame (smf) stats support */

#define SMFS_VERSION 1
/* selected mgmt frame (smf) stats element */
typedef struct wl_smfs_elem {
	uint32 count;
	uint16 code;  /* SC or RC code */
} wl_smfs_elem_t;

typedef struct wl_smf_stats {
	uint32 version;
	uint16 length;	/* reserved for future usage */
	uint8 type;
	uint8 codetype;
	uint32 ignored_cnt;
	uint32 malformed_cnt;
	uint32 count_total; /* count included the interested group */
	wl_smfs_elem_t elem[1];
} wl_smf_stats_t;

#define WL_SMFSTATS_FIXED_LEN OFFSETOF(wl_smf_stats_t, elem);

enum {
	SMFS_CODETYPE_SC,
	SMFS_CODETYPE_RC
};

typedef enum smfs_type {
	SMFS_TYPE_AUTH,
	SMFS_TYPE_ASSOC,
	SMFS_TYPE_REASSOC,
	SMFS_TYPE_DISASSOC_TX,
	SMFS_TYPE_DISASSOC_RX,
	SMFS_TYPE_DEAUTH_TX,
	SMFS_TYPE_DEAUTH_RX,
	SMFS_TYPE_MAX
} smfs_type_t;

#ifdef PHYMON

#define PHYMON_VERSION 1

typedef struct wl_phycal_core_state {
	/* Tx IQ/LO calibration coeffs */
	int16 tx_iqlocal_a;
	int16 tx_iqlocal_b;
	int8 tx_iqlocal_ci;
	int8 tx_iqlocal_cq;
	int8 tx_iqlocal_di;
	int8 tx_iqlocal_dq;
	int8 tx_iqlocal_ei;
	int8 tx_iqlocal_eq;
	int8 tx_iqlocal_fi;
	int8 tx_iqlocal_fq;

	/* Rx IQ calibration coeffs */
	int16 rx_iqcal_a;
	int16 rx_iqcal_b;

	uint8 tx_iqlocal_pwridx; /* Tx Power Index for Tx IQ/LO calibration */
	uint32 papd_epsilon_table[64]; /* PAPD epsilon table */
	int16 papd_epsilon_offset; /* PAPD epsilon offset */
	uint8 curr_tx_pwrindex; /* Tx power index */
	int8 idle_tssi; /* Idle TSSI */
	int8 est_tx_pwr; /* Estimated Tx Power (dB) */
	int8 est_rx_pwr; /* Estimated Rx Power (dB) from RSSI */
	uint16 rx_gaininfo; /* Rx gain applied on last Rx pkt */
	uint16 init_gaincode; /* initgain required for ACI */
	int8 estirr_tx;
	int8 estirr_rx;

} wl_phycal_core_state_t;

typedef struct wl_phycal_state {
	int version;
	int8 num_phy_cores; /* number of cores */
	int8 curr_temperature; /* on-chip temperature sensor reading */
	chanspec_t chspec; /* channspec for this state */
	bool aci_state; /* ACI state: ON/OFF */
	uint16 crsminpower; /* crsminpower required for ACI */
	uint16 crsminpowerl; /* crsminpowerl required for ACI */
	uint16 crsminpoweru; /* crsminpoweru required for ACI */
	wl_phycal_core_state_t phycal_core[1];
} wl_phycal_state_t;

#define WL_PHYCAL_STAT_FIXED_LEN OFFSETOF(wl_phycal_state_t, phycal_core)
#endif /* PHYMON */

/* discovery state */
typedef struct wl_p2p_disc_st {
	uint8 state;	/* see state */
	chanspec_t chspec;	/* valid in listen state */
	uint16 dwell;	/* valid in listen state, in ms */
} wl_p2p_disc_st_t;

/* scan request */
typedef struct wl_p2p_scan {
	uint8 type;		/* 'S' for WLC_SCAN, 'E' for "escan" */
	uint8 reserved[3];
	/* scan or escan parms... */
} wl_p2p_scan_t;

/* i/f request */
typedef struct wl_p2p_if {
	struct ether_addr addr;
	uint8 type;	/* see i/f type */
	chanspec_t chspec;	/* for p2p_ifadd GO */
} wl_p2p_if_t;

/* i/f query */
typedef struct wl_p2p_ifq {
	uint bsscfgidx;
	char ifname[BCM_MSG_IFNAME_MAX];
} wl_p2p_ifq_t;

/* OppPS & CTWindow */
typedef struct wl_p2p_ops {
	uint8 ops;	/* 0: disable 1: enable */
	uint8 ctw;	/* >= 10 */
} wl_p2p_ops_t;

/* absence and presence request */
typedef struct wl_p2p_sched_desc {
	uint32 start;
	uint32 interval;
	uint32 duration;
	uint32 count;	/* see count */
} wl_p2p_sched_desc_t;

typedef struct wl_p2p_sched {
	uint8 type;	/* see schedule type */
	uint8 action;	/* see schedule action */
	uint8 option;	/* see schedule option */
	wl_p2p_sched_desc_t desc[1];
} wl_p2p_sched_t;

typedef struct wl_bcmdcs_data {
	uint reason;
	chanspec_t chspec;
} wl_bcmdcs_data_t;


/* NAT configuration */
typedef struct {
	uint32 ipaddr;		/* interface ip address */
	uint32 ipaddr_mask;	/* interface ip address mask */
	uint32 ipaddr_gateway;	/* gateway ip address */
	uint8 mac_gateway[6];	/* gateway mac address */
	uint32 ipaddr_dns;	/* DNS server ip address, valid only for public if */
	uint8 mac_dns[6];	/* DNS server mac address,  valid only for public if */
	uint8 GUID[38];		/* interface GUID */
} nat_if_info_t;

typedef struct {
	uint op;		/* operation code */
	bool pub_if;		/* set for public if, clear for private if */
	nat_if_info_t if_info;	/* interface info */
} nat_cfg_t;

typedef struct {
	int state;	/* NAT state returned */
} nat_state_t;


#define BTA_STATE_LOG_SZ	64

/* BTAMP Statemachine states */
enum {
	HCIReset = 1,
	HCIReadLocalAMPInfo,
	HCIReadLocalAMPASSOC,
	HCIWriteRemoteAMPASSOC,
	HCICreatePhysicalLink,
	HCIAcceptPhysicalLinkRequest,
	HCIDisconnectPhysicalLink,
	HCICreateLogicalLink,
	HCIAcceptLogicalLink,
	HCIDisconnectLogicalLink,
	HCILogicalLinkCancel,
	HCIAmpStateChange,
	HCIWriteLogicalLinkAcceptTimeout
};

typedef struct flush_txfifo {
	uint32 txfifobmp;
	uint32 hwtxfifoflush;
	struct ether_addr ea;
} flush_txfifo_t;

enum {
	SPATIAL_MODE_2G_IDX = 0,
	SPATIAL_MODE_5G_LOW_IDX,
	SPATIAL_MODE_5G_MID_IDX,
	SPATIAL_MODE_5G_HIGH_IDX,
	SPATIAL_MODE_5G_UPPER_IDX,
	SPATIAL_MODE_MAX_IDX
};

#define WLC_TXCORE_MAX	4	/* max number of txcore supports */
#define WLC_SUBBAND_MAX	4	/* max number of sub-band supports */
typedef struct {
	uint8	band2g[WLC_TXCORE_MAX];
	uint8	band5g[WLC_SUBBAND_MAX][WLC_TXCORE_MAX];
} sar_limit_t;

#define WLC_TXCAL_CORE_MAX 2	/* max number of txcore supports for txcal */
#define MAX_NUM_TXCAL_MEAS 128

typedef struct wl_txcal_meas {
	uint8 tssi[WLC_TXCAL_CORE_MAX][MAX_NUM_TXCAL_MEAS];
	int16 pwr[WLC_TXCAL_CORE_MAX][MAX_NUM_TXCAL_MEAS];
	uint8 valid_cnt;
} wl_txcal_meas_t;

typedef struct wl_txcal_power_tssi {
	uint8 set_core;
	uint8 channel;
	int16 pwr_start[WLC_TXCAL_CORE_MAX];
	uint8 num_entries[WLC_TXCAL_CORE_MAX];
	uint8 tssi[WLC_TXCAL_CORE_MAX][MAX_NUM_TXCAL_MEAS];
	bool gen_tbl;
} wl_txcal_power_tssi_t;

/* IOVAR "mempool" parameter. Used to retrieve a list of memory pool statistics. */
typedef struct wl_mempool_stats {
	int	num;		/* Number of memory pools */
	bcm_mp_stats_t s[1];	/* Variable array of memory pool stats. */
} wl_mempool_stats_t;

typedef struct {
	uint32 ipaddr;
	uint32 ipaddr_netmask;
	uint32 ipaddr_gateway;
} nwoe_ifconfig_t;

/* Traffic management priority classes */
typedef enum trf_mgmt_priority_class {
	trf_mgmt_priority_low           = 0,        /* Maps to 802.1p BK */
	trf_mgmt_priority_medium        = 1,        /* Maps to 802.1p BE */
	trf_mgmt_priority_high          = 2,        /* Maps to 802.1p VI */
	trf_mgmt_priority_nochange	= 3,	    /* do not update the priority */
	trf_mgmt_priority_invalid       = (trf_mgmt_priority_nochange + 1)
} trf_mgmt_priority_class_t;

/* Traffic management configuration parameters */
typedef struct trf_mgmt_config {
	uint32  trf_mgmt_enabled;                           /* 0 - disabled, 1 - enabled */
	uint32  flags;                                      /* See TRF_MGMT_FLAG_xxx defines */
	uint32  host_ip_addr;                               /* My IP address to determine subnet */
	uint32  host_subnet_mask;                           /* My subnet mask */
	uint32  downlink_bandwidth;                         /* In units of kbps */
	uint32  uplink_bandwidth;                           /* In units of kbps */
	uint32  min_tx_bandwidth[TRF_MGMT_MAX_PRIORITIES];  /* Minimum guaranteed tx bandwidth */
	uint32  min_rx_bandwidth[TRF_MGMT_MAX_PRIORITIES];  /* Minimum guaranteed rx bandwidth */
} trf_mgmt_config_t;

/* Traffic management filter */
typedef struct trf_mgmt_filter {
	struct ether_addr           dst_ether_addr;         /* His L2 address */
	uint32                      dst_ip_addr;            /* His IP address */
	uint16                      dst_port;               /* His L4 port */
	uint16                      src_port;               /* My L4 port */
	uint16                      prot;                   /* L4 protocol (only TCP or UDP) */
	uint16                      flags;                  /* TBD. For now, this must be zero. */
	trf_mgmt_priority_class_t   priority;               /* Priority for filtered packets */
	uint32                      dscp;                   /* DSCP */
} trf_mgmt_filter_t;

/* Traffic management filter list (variable length) */
typedef struct trf_mgmt_filter_list     {
	uint32              num_filters;
	trf_mgmt_filter_t   filter[1];
} trf_mgmt_filter_list_t;

/* Traffic management global info used for all queues */
typedef struct trf_mgmt_global_info {
	uint32  maximum_bytes_per_second;
	uint32  maximum_bytes_per_sampling_period;
	uint32  total_bytes_consumed_per_second;
	uint32  total_bytes_consumed_per_sampling_period;
	uint32  total_unused_bytes_per_sampling_period;
} trf_mgmt_global_info_t;

/* Traffic management shaping info per priority queue */
typedef struct trf_mgmt_shaping_info {
	uint32  gauranteed_bandwidth_percentage;
	uint32  guaranteed_bytes_per_second;
	uint32  guaranteed_bytes_per_sampling_period;
	uint32  num_bytes_produced_per_second;
	uint32  num_bytes_consumed_per_second;
	uint32  num_queued_packets;                         /* Number of packets in queue */
	uint32  num_queued_bytes;                           /* Number of bytes in queue */
} trf_mgmt_shaping_info_t;

/* Traffic management shaping info array */
typedef struct trf_mgmt_shaping_info_array {
	trf_mgmt_global_info_t   tx_global_shaping_info;
	trf_mgmt_shaping_info_t  tx_queue_shaping_info[TRF_MGMT_MAX_PRIORITIES];
	trf_mgmt_global_info_t   rx_global_shaping_info;
	trf_mgmt_shaping_info_t  rx_queue_shaping_info[TRF_MGMT_MAX_PRIORITIES];
} trf_mgmt_shaping_info_array_t;


/* Traffic management statistical counters */
typedef struct trf_mgmt_stats {
	uint32  num_processed_packets;      /* Number of packets processed */
	uint32  num_processed_bytes;        /* Number of bytes processed */
	uint32  num_discarded_packets;      /* Number of packets discarded from queue */
} trf_mgmt_stats_t;

/* Traffic management statisics array */
typedef struct trf_mgmt_stats_array {
	trf_mgmt_stats_t  tx_queue_stats[TRF_MGMT_MAX_PRIORITIES];
	trf_mgmt_stats_t  rx_queue_stats[TRF_MGMT_MAX_PRIORITIES];
} trf_mgmt_stats_array_t;

typedef struct powersel_params {
	/* LPC Params exposed via IOVAR */
	int32		tp_ratio_thresh;  /* Throughput ratio threshold */
	uint8		rate_stab_thresh; /* Thresh for rate stability based on nupd */
	uint8		pwr_stab_thresh; /* Number of successes before power step down */
	uint8		pwr_sel_exp_time; /* Time lapse for expiry of database */
} powersel_params_t;

typedef struct lpc_params {
	/* LPC Params exposed via IOVAR */
	uint8		rate_stab_thresh; /* Thresh for rate stability based on nupd */
	uint8		pwr_stab_thresh; /* Number of successes before power step down */
	uint8		lpc_exp_time; /* Time lapse for expiry of database */
	uint8		pwrup_slow_step; /* Step size for slow step up */
	uint8		pwrup_fast_step; /* Step size for fast step up */
	uint8		pwrdn_slow_step; /* Step size for slow step down */
} lpc_params_t;

/* tx pkt delay statistics */
#define	SCB_RETRY_SHORT_DEF	7	/* Default Short retry Limit */
#define WLPKTDLY_HIST_NBINS	16	/* number of bins used in the Delay histogram */

/* structure to store per-AC delay statistics */
typedef struct scb_delay_stats {
	uint32 txmpdu_lost;	/* number of MPDUs lost */
	uint32 txmpdu_cnt[SCB_RETRY_SHORT_DEF]; /* retry times histogram */
	uint32 delay_sum[SCB_RETRY_SHORT_DEF]; /* cumulative packet latency */
	uint32 delay_min;	/* minimum packet latency observed */
	uint32 delay_max;	/* maximum packet latency observed */
	uint32 delay_avg;	/* packet latency average */
	uint32 delay_hist[WLPKTDLY_HIST_NBINS];	/* delay histogram */
} scb_delay_stats_t;

/* structure for txdelay event */
typedef struct txdelay_event {
	uint8	status;
	int		rssi;
	chanim_stats_t		chanim_stats;
	scb_delay_stats_t	delay_stats[AC_COUNT];
} txdelay_event_t;

/* structure for txdelay parameters */
typedef struct txdelay_params {
	uint16	ratio;	/* Avg Txdelay Delta */
	uint8	cnt;	/* Sample cnt */
	uint8	period;	/* Sample period */
	uint8	tune;	/* Debug */
} txdelay_params_t;

enum {
	WNM_SERVICE_DMS = 1,
	WNM_SERVICE_FMS = 2,
	WNM_SERVICE_TFS = 3
};

/* Definitions for WNM/NPS TCLAS */
typedef struct wl_tclas {
	uint8 user_priority;
	uint8 fc_len;
	dot11_tclas_fc_t fc;
} wl_tclas_t;

#define WL_TCLAS_FIXED_SIZE	OFFSETOF(wl_tclas_t, fc)

typedef struct wl_tclas_list {
	uint32 num;
	wl_tclas_t tclas[1];
} wl_tclas_list_t;

/* Definitions for WNM/NPS Traffic Filter Service */
typedef struct wl_tfs_req {
	uint8 tfs_id;
	uint8 tfs_actcode;
	uint8 tfs_subelem_id;
	uint8 send;
} wl_tfs_req_t;

typedef struct wl_tfs_filter {
	uint8 status;			/* Status returned by the AP */
	uint8 tclas_proc;		/* TCLAS processing value (0:and, 1:or)  */
	uint8 tclas_cnt;		/* count of all wl_tclas_t in tclas array */
	uint8 tclas[1];			/* VLA of wl_tclas_t */
} wl_tfs_filter_t;
#define WL_TFS_FILTER_FIXED_SIZE	OFFSETOF(wl_tfs_filter_t, tclas)

typedef struct wl_tfs_fset {
	struct ether_addr ea;		/* Address of AP/STA involved with this filter set */
	uint8 tfs_id;			/* TFS ID field chosen by STA host */
	uint8 status;			/* Internal status TFS_STATUS_xxx */
	uint8 actcode;			/* Action code DOT11_TFS_ACTCODE_xxx */
	uint8 token;			/* Token used in last request frame */
	uint8 notify;			/* Notify frame sent/received because of this set */
	uint8 filter_cnt;		/* count of all wl_tfs_filter_t in filter array */
	uint8 filter[1];		/* VLA of wl_tfs_filter_t */
} wl_tfs_fset_t;
#define WL_TFS_FSET_FIXED_SIZE		OFFSETOF(wl_tfs_fset_t, filter)

enum {
	TFS_STATUS_DISABLED = 0,	/* TFS filter set disabled by user */
	TFS_STATUS_DISABLING = 1,	/* Empty request just sent to AP */
	TFS_STATUS_VALIDATED = 2,	/* Filter set validated by AP (but maybe not enabled!) */
	TFS_STATUS_VALIDATING = 3,	/* Filter set just sent to AP */
	TFS_STATUS_NOT_ASSOC = 4,	/* STA not associated */
	TFS_STATUS_NOT_SUPPORT = 5,	/* TFS not supported by AP */
	TFS_STATUS_DENIED = 6,		/* Filter set refused by AP (=> all sets are disabled!) */
};

typedef struct wl_tfs_status {
	uint8 fset_cnt;			/* count of all wl_tfs_fset_t in fset array */
	wl_tfs_fset_t fset[1];		/* VLA of wl_tfs_fset_t */
} wl_tfs_status_t;

typedef struct wl_tfs_set {
	uint8 send;			/* Immediatly register registered sets on AP side */
	uint8 tfs_id;			/* ID of a specific set (existing or new), or nul for all */
	uint8 actcode;			/* Action code for this filter set */
	uint8 tclas_proc;		/* TCLAS processing operator for this filter set */
} wl_tfs_set_t;

typedef struct wl_tfs_term {
	uint8 del;			/* Delete internal set once confirmation received */
	uint8 tfs_id;			/* ID of a specific set (existing), or nul for all */
} wl_tfs_term_t;


#define DMS_DEP_PROXY_ARP (1 << 0)

/* Definitions for WNM/NPS Directed Multicast Service */
enum {
	DMS_STATUS_DISABLED = 0,	/* DMS desc disabled by user */
	DMS_STATUS_ACCEPTED = 1,	/* Request accepted by AP */
	DMS_STATUS_NOT_ASSOC = 2,	/* STA not associated */
	DMS_STATUS_NOT_SUPPORT = 3,	/* DMS not supported by AP */
	DMS_STATUS_DENIED = 4,		/* Request denied by AP */
	DMS_STATUS_TERM = 5,		/* Request terminated by AP */
	DMS_STATUS_REMOVING = 6,	/* Remove request just sent */
	DMS_STATUS_ADDING = 7,		/* Add request just sent */
	DMS_STATUS_ERROR = 8,		/* Non compliant AP behvior */
	DMS_STATUS_IN_PROGRESS = 9, /* Request just sent */
	DMS_STATUS_REQ_MISMATCH = 10 /* Conditions for sending DMS req not met */
};

typedef struct wl_dms_desc {
	uint8 user_id;
	uint8 status;
	uint8 token;
	uint8 dms_id;
	uint8 tclas_proc;
	uint8 mac_len;		/* length of all ether_addr in data array, 0 if STA */
	uint8 tclas_len;	/* length of all wl_tclas_t in data array */
	uint8 data[1];		/* VLA of 'ether_addr' and 'wl_tclas_t' (in this order ) */
} wl_dms_desc_t;

#define WL_DMS_DESC_FIXED_SIZE	OFFSETOF(wl_dms_desc_t, data)

typedef struct wl_dms_status {
	uint32 cnt;
	wl_dms_desc_t desc[1];
} wl_dms_status_t;

typedef struct wl_dms_set {
	uint8 send;
	uint8 user_id;
	uint8 tclas_proc;
} wl_dms_set_t;

typedef struct wl_dms_term {
	uint8 del;
	uint8 user_id;
} wl_dms_term_t;

typedef struct wl_service_term {
	uint8 service;
	union {
		wl_dms_term_t dms;
	} u;
} wl_service_term_t;

/* Definitions for WNM/NPS BSS Transistion */
typedef struct wl_bsstrans_req {
	uint16 tbtt;			/* time of BSS to end of life, in unit of TBTT */
	uint16 dur;			/* time of BSS to keep off, in unit of minute */
	uint8 reqmode;			/* request mode of BSS transition request */
	uint8 unicast;			/* request by unicast or by broadcast */
} wl_bsstrans_req_t;

enum {
	BSSTRANS_RESP_AUTO = 0,		/* Currently equivalent to ENABLE */
	BSSTRANS_RESP_DISABLE = 1,	/* Never answer BSS Trans Req frames */
	BSSTRANS_RESP_ENABLE = 2,	/* Always answer Req frames with preset data */
	BSSTRANS_RESP_WAIT = 3,		/* Send ind, wait and/or send preset data (NOT IMPL) */
	BSSTRANS_RESP_IMMEDIATE = 4	/* After an ind, set data and send resp (NOT IMPL) */
};

typedef struct wl_bsstrans_resp {
	uint8 policy;
	uint8 status;
	uint8 delay;
	struct ether_addr target;
} wl_bsstrans_resp_t;

/* "wnm_bsstrans_policy" argument programs behavior after BSSTRANS Req reception.
 * BSS-Transition feature is used by multiple programs such as NPS-PF, VE-PF,
 * Band-steering, Hotspot 2.0 and customer requirements. Each PF and its test plan
 * mandates different behavior on receiving BSS-transition request. To accomodate
 * such divergent behaviors these policies have been created.
 */
enum {
	WL_BSSTRANS_POLICY_ROAM_ALWAYS = 0,	/* Roam (or disassociate) in all cases */
	WL_BSSTRANS_POLICY_ROAM_IF_MODE = 1,	/* Roam only if requested by Request Mode field */
	WL_BSSTRANS_POLICY_ROAM_IF_PREF = 2,	/* Roam only if Preferred BSS provided */
	WL_BSSTRANS_POLICY_WAIT = 3,		/* Wait for deauth and send Accepted status */
	WL_BSSTRANS_POLICY_PRODUCT = 4,		/* Policy for real product use cases (non-pf) */
};

/* Definitions for WNM/NPS TIM Broadcast */
typedef struct wl_timbc_offset {
	int16 offset;		/* offset in us */
	uint16 fix_intv;	/* override interval sent from STA */
	uint16 rate_override;	/* use rate override to send high rate TIM broadcast frame */
	uint8 tsf_present;	/* show timestamp in TIM broadcast frame */
} wl_timbc_offset_t;

typedef struct wl_timbc_set {
	uint8 interval;		/* Interval in DTIM wished or required. */
	uint8 flags;		/* Bitfield described below */
	uint16 rate_min;	/* Minimum rate required for High/Low TIM frames. Optionnal */
	uint16 rate_max;	/* Maximum rate required for High/Low TIM frames. Optionnal */
} wl_timbc_set_t;

enum {
	WL_TIMBC_SET_TSF_REQUIRED = 1,	/* Enable TIMBC only if TSF in TIM frames */
	WL_TIMBC_SET_NO_OVERRIDE = 2,	/* ... if AP does not override interval */
	WL_TIMBC_SET_PROXY_ARP = 4,	/* ... if AP support Proxy ARP */
	WL_TIMBC_SET_DMS_ACCEPTED = 8	/* ... if all DMS desc have been accepted */
};

typedef struct wl_timbc_status {
	uint8 status_sta;		/* Status from internal state machine (check below) */
	uint8 status_ap;		/* From AP response frame (check 8.4.2.86 from 802.11) */
	uint8 interval;
	uint8 pad;
	int32 offset;
	uint16 rate_high;
	uint16 rate_low;
} wl_timbc_status_t;

enum {
	WL_TIMBC_STATUS_DISABLE = 0,		/* TIMBC disabled by user */
	WL_TIMBC_STATUS_REQ_MISMATCH = 1,	/* AP settings do no match user requirements */
	WL_TIMBC_STATUS_NOT_ASSOC = 2,		/* STA not associated */
	WL_TIMBC_STATUS_NOT_SUPPORT = 3,	/* TIMBC not supported by AP */
	WL_TIMBC_STATUS_DENIED = 4,		/* Req to disable TIMBC sent to AP */
	WL_TIMBC_STATUS_ENABLE = 5		/* TIMBC enabled */
};

/* Definitions for PM2 Dynamic Fast Return To Sleep */
typedef struct wl_pm2_sleep_ret_ext {
	uint8 logic;			/* DFRTS logic: see WL_DFRTS_LOGIC_* below */
	uint16 low_ms;			/* Low FRTS timeout */
	uint16 high_ms;			/* High FRTS timeout */
	uint16 rx_pkts_threshold;	/* switching threshold: # rx pkts */
	uint16 tx_pkts_threshold;	/* switching threshold: # tx pkts */
	uint16 txrx_pkts_threshold;	/* switching threshold: # (tx+rx) pkts */
	uint32 rx_bytes_threshold;	/* switching threshold: # rx bytes */
	uint32 tx_bytes_threshold;	/* switching threshold: # tx bytes */
	uint32 txrx_bytes_threshold;	/* switching threshold: # (tx+rx) bytes */
} wl_pm2_sleep_ret_ext_t;

#define WL_DFRTS_LOGIC_OFF	0	/* Feature is disabled */
#define WL_DFRTS_LOGIC_OR	1	/* OR all non-zero threshold conditions */
#define WL_DFRTS_LOGIC_AND	2	/* AND all non-zero threshold conditions */

/* Values for the passive_on_restricted_mode iovar.  When set to non-zero, this iovar
 * disables automatic conversions of a channel from passively scanned to
 * actively scanned.  These values only have an effect for country codes such
 * as XZ where some 5 GHz channels are defined to be passively scanned.
 */
#define WL_PASSACTCONV_DISABLE_NONE	0	/* Enable permanent and temporary conversions */
#define WL_PASSACTCONV_DISABLE_ALL	1	/* Disable permanent and temporary conversions */
#define WL_PASSACTCONV_DISABLE_PERM	2	/* Disable only permanent conversions */

/* Definitions for Reliable Multicast */
#define WL_RMC_CNT_VERSION	   1
#define WL_RMC_TR_VERSION	   1
#define WL_RMC_MAX_CLIENT	   32
#define WL_RMC_FLAG_INBLACKLIST	   1
#define WL_RMC_FLAG_ACTIVEACKER	   2
#define WL_RMC_FLAG_RELMCAST	   4
#define WL_RMC_MAX_TABLE_ENTRY     4

#define WL_RMC_VER		   1
#define WL_RMC_INDEX_ACK_ALL       255
#define WL_RMC_NUM_OF_MC_STREAMS   4
#define WL_RMC_MAX_TRS_PER_GROUP   1
#define WL_RMC_MAX_TRS_IN_ACKALL   1
#define WL_RMC_ACK_MCAST0          0x02
#define WL_RMC_ACK_MCAST_ALL       0x01
#define WL_RMC_ACTF_TIME_MIN       300	 /* time in ms */
#define WL_RMC_ACTF_TIME_MAX       20000 /* time in ms */
#define WL_RMC_MAX_NUM_TRS	   32	 /* maximun transmitters allowed */
#define WL_RMC_ARTMO_MIN           350	 /* time in ms */
#define WL_RMC_ARTMO_MAX           40000	 /* time in ms */

enum rmc_opcodes {
	RELMCAST_ENTRY_OP_DISABLE = 0,   /* Disable multi-cast group */
	RELMCAST_ENTRY_OP_DELETE  = 1,   /* Delete multi-cast group */
	RELMCAST_ENTRY_OP_ENABLE  = 2,   /* Enable multi-cast group */
	RELMCAST_ENTRY_OP_ACK_ALL = 3    /* Enable ACK ALL bit in AMT */
};

/* RMC operational modes */
enum rmc_modes {
	WL_RMC_MODE_RECEIVER    = 0,	 /* Receiver mode by default */
	WL_RMC_MODE_TRANSMITTER = 1,	 /* Transmitter mode using wl ackreq */
	WL_RMC_MODE_INITIATOR   = 2	 /* Initiator mode using wl ackreq */
};

/* Each RMC mcast client info */
typedef struct wl_relmcast_client {
	uint8 flag;			/* status of client such as AR, R, or blacklisted */
	int16 rssi;			/* rssi value of RMC client */
	struct ether_addr addr;		/* mac address of RMC client */
} wl_relmcast_client_t;

/* RMC Counters */
typedef struct wl_rmc_cnts {
	uint16  version;		/* see definition of WL_CNT_T_VERSION */
	uint16  length;			/* length of entire structure */
	uint16	dupcnt;			/* counter for duplicate rmc MPDU */
	uint16	ackreq_err;		/* counter for wl ackreq error    */
	uint16	af_tx_err;		/* error count for action frame transmit   */
	uint16	null_tx_err;		/* error count for rmc null frame transmit */
	uint16	af_unicast_tx_err;	/* error count for rmc unicast frame transmit */
	uint16	mc_no_amt_slot;		/* No mcast AMT entry available */
	/* Unused. Keep for rom compatibility */
	uint16	mc_no_glb_slot;		/* No mcast entry available in global table */
	uint16	mc_not_mirrored;	/* mcast group is not mirrored */
	uint16	mc_existing_tr;		/* mcast group is already taken by transmitter */
	uint16	mc_exist_in_amt;	/* mcast group is already programmed in amt */
	/* Unused. Keep for rom compatibility */
	uint16	mc_not_exist_in_gbl;	/* mcast group is not in global table */
	uint16	mc_not_exist_in_amt;	/* mcast group is not in AMT table */
	uint16	mc_utilized;		/* mcast addressed is already taken */
	uint16	mc_taken_other_tr;	/* multi-cast addressed is already taken */
	uint32	rmc_rx_frames_mac;      /* no of mc frames received from mac */
	uint32	rmc_tx_frames_mac;      /* no of mc frames transmitted to mac */
	uint32	mc_null_ar_cnt;         /* no. of times NULL AR is received */
	uint32	mc_ar_role_selected;	/* no. of times took AR role */
	uint32	mc_ar_role_deleted;	/* no. of times AR role cancelled */
	uint32	mc_noacktimer_expired;  /* no. of times noack timer expired */
	uint16  mc_no_wl_clk;           /* no wl clk detected when trying to access amt */
	uint16  mc_tr_cnt_exceeded;     /* No of transmitters in the network exceeded */
} wl_rmc_cnts_t;

/* RMC Status */
typedef struct wl_relmcast_st {
	uint8         ver;		/* version of RMC */
	uint8         num;		/* number of clients detected by transmitter */
	wl_relmcast_client_t clients[WL_RMC_MAX_CLIENT];
	uint16        err;		/* error status (used in infra) */
	uint16        actf_time;	/* action frame time period */
} wl_relmcast_status_t;

/* Entry for each STA/node */
typedef struct wl_rmc_entry {
	/* operation on multi-cast entry such add,
	 * delete, ack-all
	 */
	int8    flag;
	struct ether_addr addr;		/* multi-cast group mac address */
} wl_rmc_entry_t;

/* RMC table */
typedef struct wl_rmc_entry_table {
	uint8   index;			/* index to a particular mac entry in table */
	uint8   opcode;			/* opcodes or operation on entry */
	wl_rmc_entry_t entry[WL_RMC_MAX_TABLE_ENTRY];
} wl_rmc_entry_table_t;

typedef struct wl_rmc_trans_elem {
	struct ether_addr tr_mac;	/* transmitter mac */
	struct ether_addr ar_mac;	/* ar mac */
	uint16 artmo;			/* AR timeout */
	uint8 amt_idx;			/* amt table entry */
	uint16 flag;			/* entry will be acked, not acked, programmed, full etc */
} wl_rmc_trans_elem_t;

/* RMC transmitters */
typedef struct wl_rmc_trans_in_network {
	uint8         ver;		/* version of RMC */
	uint8         num_tr;		/* number of transmitters in the network */
	wl_rmc_trans_elem_t trs[WL_RMC_MAX_NUM_TRS];
} wl_rmc_trans_in_network_t;

/* To update vendor specific ie for RMC */
typedef struct wl_rmc_vsie {
	uint8	oui[DOT11_OUI_LEN];
	uint16	payload;	/* IE Data Payload */
} wl_rmc_vsie_t;


/* structures  & defines for proximity detection  */
enum proxd_method {
	PROXD_UNDEFINED_METHOD = 0,
	PROXD_RSSI_METHOD = 1,
	PROXD_TOF_METHOD = 2
};

/* structures for proximity detection device role */
#define WL_PROXD_MODE_DISABLE	0
#define WL_PROXD_MODE_NEUTRAL	1
#define WL_PROXD_MODE_INITIATOR	2
#define WL_PROXD_MODE_TARGET	3

#define WL_PROXD_ACTION_STOP		0
#define WL_PROXD_ACTION_START		1

#define WL_PROXD_FLAG_TARGET_REPORT	0x1
#define WL_PROXD_FLAG_REPORT_FAILURE	0x2
#define WL_PROXD_FLAG_INITIATOR_REPORT	0x4
#define WL_PROXD_FLAG_NOCHANSWT		0x8
#define WL_PROXD_FLAG_NETRUAL		0x10

#define WL_PROXD_RANDOM_WAKEUP	0x8000

typedef struct wl_proxd_iovar {
	uint16	method;		/* Proxmity Detection method */
	uint16	mode;		/* Mode (neutral, initiator, target) */
} wl_proxd_iovar_t;

/*
 * structures for proximity detection parameters
 * consists of two parts, common and method specific params
 * common params should be placed at the beginning
 */

/* require strict packing */
#include <packed_section_start.h>

typedef	BWL_PRE_PACKED_STRUCT struct	wl_proxd_params_common	{
	chanspec_t	chanspec;	/* channel spec */
	int16		tx_power;	/* tx power of Proximity Detection(PD) frames (in dBm) */
	uint16		tx_rate;	/* tx rate of PD rames  (in 500kbps units) */
	uint16		timeout;	/* timeout value */
	uint16		interval;	/* interval between neighbor finding attempts (in TU) */
	uint16		duration;	/* duration of neighbor finding attempts (in ms) */
} BWL_POST_PACKED_STRUCT wl_proxd_params_common_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_proxd_params_rssi_method {
	chanspec_t	chanspec;	/* chanspec for home channel */
	int16		tx_power;	/* tx power of Proximity Detection frames (in dBm) */
	uint16		tx_rate;	/* tx rate of PD frames, 500kbps units */
	uint16		timeout;	/* state machine wait timeout of the frames (in ms) */
	uint16		interval;	/* interval between neighbor finding attempts (in TU) */
	uint16		duration;	/* duration of neighbor finding attempts (in ms) */
					/* method specific ones go after this line */
	int16		rssi_thresh;	/* RSSI threshold (in dBm) */
	uint16		maxconvergtmo;	/* max wait converge timeout (in ms) */
} wl_proxd_params_rssi_method_t;

#define Q1_NS			25	/* Q1 time units */

#define TOF_BW_NUM		3	/* number of bandwidth that the TOF can support */
enum tof_bw_index {
	TOF_BW_20MHZ_INDEX = 0,
	TOF_BW_40MHZ_INDEX = 1,
	TOF_BW_80MHZ_INDEX = 2
};

#define BANDWIDTH_BASE	20	/* base value of bandwidth */
#define TOF_BW_20MHZ    (BANDWIDTH_BASE << TOF_BW_20MHZ_INDEX)
#define TOF_BW_40MHZ    (BANDWIDTH_BASE << TOF_BW_40MHZ_INDEX)
#define TOF_BW_80MHZ    (BANDWIDTH_BASE << TOF_BW_80MHZ_INDEX)
#define TOF_BW_10MHZ    10

#define NFFT_BASE		64	/* base size of fft */
#define TOF_NFFT_20MHZ  (NFFT_BASE << TOF_BW_20MHZ_INDEX)
#define TOF_NFFT_40MHZ  (NFFT_BASE << TOF_BW_40MHZ_INDEX)
#define TOF_NFFT_80MHZ  (NFFT_BASE << TOF_BW_80MHZ_INDEX)

typedef BWL_PRE_PACKED_STRUCT struct wl_proxd_params_tof_method {
	chanspec_t	chanspec;	/* chanspec for home channel */
	int16		tx_power;	/* tx power of Proximity Detection(PD) frames (in dBm) */
	uint16		tx_rate;	/* tx rate of PD rames  (in 500kbps units) */
	uint16		timeout;	/* state machine wait timeout of the frames (in ms) */
	uint16		interval;	/* interval between neighbor finding attempts (in TU) */
	uint16		duration;	/* duration of neighbor finding attempts (in ms) */
	/* specific for the method go after this line */
	struct ether_addr tgt_mac;	/* target mac addr for TOF method */
	uint16		ftm_cnt;	/* number of ftm frames reqested by initiator */
	uint16		retry_cnt;	/* number of retransmit attampts for ftm frames */
	int16		vht_rate;	/* ht or vht rate */
	/* add more params required for other methods can be added here  */
} BWL_POST_PACKED_STRUCT wl_proxd_params_tof_method_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_proxd_params_tof_tune {
	uint32		Ki;			/* h/w delay K factor for initiator */
	uint32		Kt;			/* h/w delay K factor for target */
	int16		gdadj;			/* 0:none, 1:ground delay and threshold crossing */
	int16		vhtack;			/* enable/disable VHT ACK */
	int16		N_log2;			/* simple threshold crossing */
	int16		N_scale;		/* simple threshold crossing */
	int16		sw_adj;			/* enable sw assisted timestamp adjustment */
	int16		hw_adj;			/* enable hw assisted timestamp adjustment */
	int16		w_offset[TOF_BW_NUM];	/* offset of threshold crossing window(per BW) */
	int16		w_len[TOF_BW_NUM];	/* length of threshold crossing window(per BW) */
	int32		maxDT;			/* max time difference of T4/T1 or T3/T2 */
	int32		minDT;			/* min time difference of T4/T1 or T3/T2 */
	uint8		totalfrmcnt;	/* total count of transfered measurement frames */
	uint16		rsv_media;		/* reserve media value for TOF */
	uint32		flags;			/* flags */
} BWL_POST_PACKED_STRUCT wl_proxd_params_tof_tune_t;

typedef struct wl_proxd_params_iovar {
	uint16	method;			/* Proxmity Detection method */
	union {
		/* common params for pdsvc */
		wl_proxd_params_common_t	cmn_params;	/* common parameters */
		/*  method specific */
		wl_proxd_params_rssi_method_t	rssi_params;	/* RSSI method parameters */
		wl_proxd_params_tof_method_t	tof_params;	/* TOF meothod parameters */
		/* tune parameters */
		wl_proxd_params_tof_tune_t	tof_tune;	/* TOF tune parameters */
	} u;				/* Method specific optional parameters */
} wl_proxd_params_iovar_t;

#define PROXD_COLLECT_GET_STATUS	0
#define PROXD_COLLECT_SET_STATUS	1
#define PROXD_COLLECT_QUERY_HEADER	2
#define PROXD_COLLECT_QUERY_DATA	3
#define PROXD_COLLECT_QUERY_DEBUG	4
#define PROXD_COLLECT_REMOTE_REQUEST	5

typedef BWL_PRE_PACKED_STRUCT struct wl_proxd_collect_query {
	uint32		method;		/* method */
	uint8		request;	/* Query request. */
	uint8		status;		/* 0 -- disable, 1 -- enable collection, */
					/* 2 -- enable collection & debug */
	uint16		index;		/* The current frame index [0 to total_frames - 1]. */
	uint16		mode;		/* Initiator or Target */
	bool		busy;		/* tof sm is busy */
	bool		remote;		/* Remote collect data */
} BWL_POST_PACKED_STRUCT wl_proxd_collect_query_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_proxd_collect_header {
	uint16		total_frames;			/* The totral frames for this collect. */
	uint16		nfft;				/* nfft value */
	uint16		bandwidth;			/* bandwidth */
	uint16		channel;			/* channel number */
	uint32		chanspec;			/* channel spec */
	uint32		fpfactor;			/* avb timer value factor */
	uint16		fpfactor_shift;			/* avb timer value shift bits */
	int16		window_len[TOF_BW_NUM];		/* window length */
	int16		window_offset[TOF_BW_NUM];	/* window offset */
	int16		thresh_scale;			/* threshold scale */
	int16		thresh_log2;			/* threshold log2 value */
	int32		kvalue;				/* k value used */
	int32		distance;			/* distance calculated by fw */
	uint32		meanrtt;			/* mean of RTTs */
	uint32		modertt;			/* mode of RTTs */
	uint32		medianrtt;			/* median of RTTs */
	uint32		sdrtt;				/* standard deviation of RTTs */
	uint32		clkdivisor;			/* clock divisor */
	uint16		chipnum;			/* chip type */
	uint8		chiprev;			/* chip revision */
	uint8		phyver;				/* phy version */
	struct ether_addr	loaclMacAddr;		/* local mac address */
	struct ether_addr	remoteMacAddr;		/* remote mac address */
} BWL_POST_PACKED_STRUCT wl_proxd_collect_header_t;

#define RSSI_THRESHOLD_SIZE 16
#define MAX_IMP_RESP_SIZE 256

typedef BWL_PRE_PACKED_STRUCT struct wl_proxd_rssi_bias {
	int32		version;			/* version */
	int32		threshold[RSSI_THRESHOLD_SIZE];	/* threshold */
	int32		peak_offset;			/* peak offset */
	int32		bias;				/* rssi bias */
	int32		gd_delta;			/* GD - GD_ADJ */
	int32		imp_resp[MAX_IMP_RESP_SIZE];	/* (Hi*Hi)+(Hr*Hr) */
} BWL_POST_PACKED_STRUCT wl_proxd_rssi_bias_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_proxd_rssi_bias_avg {
	int32		avg_threshold[RSSI_THRESHOLD_SIZE];	/* avg threshold */
	int32		avg_peak_offset;			/* avg peak offset */
	int32		avg_rssi;				/* avg rssi */
	int32		avg_bias;				/* avg bias */
} BWL_POST_PACKED_STRUCT wl_proxd_rssi_bias_avg_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_proxd_collect_data {
	uint16		index;		/* The current frame index, from 1 to total_frames. */
	uint16		tof_cmd;	/* M_TOF_CMD      */
	uint16		tof_rsp;	/* M_TOF_RSP      */
	uint16		tof_avb_rxl;	/* M_TOF_AVB_RX_L */
	uint16		tof_avb_rxh;	/* M_TOF_AVB_RX_H */
	uint16		tof_avb_txl;	/* M_TOF_AVB_TX_L */
	uint16		tof_avb_txh;	/* M_TOF_AVB_TX_H */
	uint16		tof_id;		/* M_TOF_ID */
	uint16		tof_status0;	/* M_TOF_STATUS_0 */
	uint16		tof_status2;	/* M_TOF_STATUS_2 */
	uint16		tof_chsm0;	/* M_TOF_CHNSM_0 */
	int32		gd_adj_ns;	/* gound delay */
	int32		gd_h_adj_ns;	/* group delay + threshold crossing */
	int16		nfft;		/* number of sample channel data */
	uint16		type;		/* type: 0 channel table, 1 channel smoothing table */
	uint32		H[256];		/* channel table get by core0chanestTbl. */
#ifdef RSSI_REFINE
	wl_proxd_rssi_bias_t rssi_bias; /* RSSI refinement info */
#endif
} BWL_POST_PACKED_STRUCT wl_proxd_collect_data_t;

typedef BWL_PRE_PACKED_STRUCT struct wl_proxd_debug_data {
	uint8		count;		/* number of packets */
	uint8		stage;		/* state machone stage */
	uint8		received;	/* received or txed */
	uint8		paket_type;	/* packet type */
	uint8		category;	/* category field */
	uint8		action;		/* action field */
	uint8		token;		/* token number */
	uint8		follow_token;	/* following token number */
	uint16		index;		/* index of the packet */
	uint16		tof_cmd;	/* M_TOF_CMD */
	uint16		tof_rsp;	/* M_TOF_RSP */
	uint16		tof_avb_rxl;	/* M_TOF_AVB_RX_L */
	uint16		tof_avb_rxh;	/* M_TOF_AVB_RX_H */
	uint16		tof_avb_txl;	/* M_TOF_AVB_TX_L */
	uint16		tof_avb_txh;	/* M_TOF_AVB_TX_H */
	uint16		tof_id;		/* M_TOF_ID */
	uint16		tof_status0;	/* M_TOF_STATUS_0 */
	uint16		tof_status2;	/* M_TOF_STATUS_2 */
	uint16		tof_chsm0;	/* M_TOF_CHNSM_0 */
	uint16		tof_phyctl0;	/* M_TOF_PHYCTL0 */
	uint16		tof_phyctl1;	/* M_TOF_PHYCTL1 */
	uint16		tof_phyctl2;	/* M_TOF_PHYCTL2 */
	uint16		tof_lsig;	/* M_TOF_LSIG */
	uint16		tof_vhta0;	/* M_TOF_VHTA0 */
	uint16		tof_vhta1;	/* M_TOF_VHTA1 */
	uint16		tof_vhta2;	/* M_TOF_VHTA2 */
	uint16		tof_vhtb0;	/* M_TOF_VHTB0 */
	uint16		tof_vhtb1;	/* M_TOF_VHTB1 */
	uint16		tof_apmductl;	/* M_TOF_AMPDU_CTL */
	uint16		tof_apmdudlim;	/* M_TOF_AMPDU_DLIM */
	uint16		tof_apmdulen;	/* M_TOF_AMPDU_LEN */
} BWL_POST_PACKED_STRUCT wl_proxd_debug_data_t;

/* version of the wl_wsec_info structure */
#define WL_WSEC_INFO_VERSION 0x01

/* start enum value for BSS properties */
#define WL_WSEC_INFO_BSS_BASE 0x0100

/* size of len and type fields of wl_wsec_info_tlv_t struct */
#define WL_WSEC_INFO_TLV_HDR_LEN OFFSETOF(wl_wsec_info_tlv_t, data)

/* Allowed wl_wsec_info properties; not all of them may be supported. */
typedef enum {
	WL_WSEC_INFO_NONE = 0,
	WL_WSEC_INFO_MAX_KEYS = 1,
	WL_WSEC_INFO_NUM_KEYS = 2,
	WL_WSEC_INFO_NUM_HW_KEYS = 3,
	WL_WSEC_INFO_MAX_KEY_IDX = 4,
	WL_WSEC_INFO_NUM_REPLAY_CNTRS = 5,
	WL_WSEC_INFO_SUPPORTED_ALGOS = 6,
	WL_WSEC_INFO_MAX_KEY_LEN = 7,
	WL_WSEC_INFO_FLAGS = 8,
	/* add global/per-wlc properties above */
	WL_WSEC_INFO_BSS_FLAGS = (WL_WSEC_INFO_BSS_BASE + 1),
	WL_WSEC_INFO_BSS_WSEC = (WL_WSEC_INFO_BSS_BASE + 2),
	WL_WSEC_INFO_BSS_TX_KEY_ID = (WL_WSEC_INFO_BSS_BASE + 3),
	WL_WSEC_INFO_BSS_ALGO = (WL_WSEC_INFO_BSS_BASE + 4),
	WL_WSEC_INFO_BSS_KEY_LEN = (WL_WSEC_INFO_BSS_BASE + 5),
	/* add per-BSS properties above */
	WL_WSEC_INFO_MAX = 0xffff
} wl_wsec_info_type_t;

/* tlv used to return wl_wsec_info properties */
typedef struct {
	uint16 type;
	uint16 len;		/* data length */
	uint8 data[1];	/* data follows */
} wl_wsec_info_tlv_t;

/* input/output data type for wsec_info iovar */
typedef struct wl_wsec_info {
	uint8 version; /* structure version */
	uint8 pad[2];
	uint8 num_tlvs;
	wl_wsec_info_tlv_t tlvs[1]; /* tlv data follows */
} wl_wsec_info_t;

/* no default structure packing */
#include <packed_section_end.h>

enum rssi_reason {
	RSSI_REASON_UNKNOW = 0,
	RSSI_REASON_LOWRSSI = 1,
	RSSI_REASON_NSYC = 2,
	RSSI_REASON_TIMEOUT = 3
};

enum tof_reason {
	TOF_REASON_OK = 0,
	TOF_REASON_REQEND = 1,
	TOF_REASON_TIMEOUT = 2,
	TOF_REASON_NOACK = 3,
	TOF_REASON_INVALIDAVB = 4,
	TOF_REASON_INITIAL = 5,
	TOF_REASON_ABORT = 6
};

enum rssi_state {
	RSSI_STATE_POLL = 0,
	RSSI_STATE_TPAIRING = 1,
	RSSI_STATE_IPAIRING = 2,
	RSSI_STATE_THANDSHAKE = 3,
	RSSI_STATE_IHANDSHAKE = 4,
	RSSI_STATE_CONFIRMED = 5,
	RSSI_STATE_PIPELINE = 6,
	RSSI_STATE_NEGMODE = 7,
	RSSI_STATE_MONITOR = 8,
	RSSI_STATE_LAST = 9
};

enum tof_state {
	TOF_STATE_IDLE	 = 0,
	TOF_STATE_IWAITM = 1,
	TOF_STATE_TWAITM = 2,
	TOF_STATE_ILEGACY = 3,
	TOF_STATE_IWAITCL = 4,
	TOF_STATE_TWAITCL = 5,
	TOF_STATE_ICONFIRM = 6,
	TOF_STATE_IREPORT = 7
};

enum tof_mode_type {
	TOF_LEGACY_UNKNOWN	= 0,
	TOF_LEGACY_AP		= 1,
	TOF_NONLEGACY_AP	= 2
};

enum tof_way_type {
	TOF_TYPE_ONE_WAY = 0,
	TOF_TYPE_TWO_WAY = 1
};

enum tof_rate_type {
	TOF_FRAME_RATE_VHT = 0,
	TOF_FRAME_RATE_LEGACY = 1
};

#define TOF_ADJ_TYPE_NUM	2	/* number of assisted timestamp adjustment */
enum tof_adj_mode {
	TOF_ADJ_SOFTWARE = 0,
	TOF_ADJ_HARDWARE = 1
};

#define FRAME_TYPE_NUM		4	/* number of frame type */
enum frame_type {
	FRAME_TYPE_CCK	= 0,
	FRAME_TYPE_OFDM	= 1,
	FRAME_TYPE_11N	= 2,
	FRAME_TYPE_11AC	= 3
};

typedef struct wl_proxd_status_iovar {
	uint16			method;				/* method */
	uint8			mode;				/* mode */
	uint8			peermode;			/* peer mode */
	uint8			state;				/* state */
	uint8			reason;				/* reason code */
	uint32			distance;			/* distance */
	uint32			txcnt;				/* tx pkt counter */
	uint32			rxcnt;				/* rx pkt counter */
	struct ether_addr	peer;				/* peer mac address */
	int8			avg_rssi;			/* average rssi */
	int8			hi_rssi;			/* highest rssi */
	int8			low_rssi;			/* lowest rssi */
	uint32			dbgstatus;			/* debug status */
	uint16			frame_type_cnt[FRAME_TYPE_NUM];	/* frame types */
	uint16			adj_type_cnt[TOF_ADJ_TYPE_NUM];	/* adj types HW/SW */
} wl_proxd_status_iovar_t;

#ifdef NET_DETECT
typedef struct net_detect_adapter_features {
	bool	wowl_enabled;
	bool	net_detect_enabled;
	bool	nlo_enabled;
} net_detect_adapter_features_t;

typedef enum net_detect_bss_type {
	nd_bss_any = 0,
	nd_ibss,
	nd_ess
} net_detect_bss_type_t;

typedef struct net_detect_profile {
	wlc_ssid_t		ssid;
	net_detect_bss_type_t   bss_type;	/* Ignore for now since Phase 1 is only for ESS */
	uint32			cipher_type;	/* DOT11_CIPHER_ALGORITHM enumeration values */
	uint32			auth_type;	/* DOT11_AUTH_ALGORITHM enumeration values */
} net_detect_profile_t;

typedef struct net_detect_profile_list {
	uint32			num_nd_profiles;
	net_detect_profile_t	nd_profile[0];
} net_detect_profile_list_t;

typedef struct net_detect_config {
	bool			    nd_enabled;
	uint32			    scan_interval;
	uint32			    wait_period;
	bool			    wake_if_connected;
	bool			    wake_if_disconnected;
	net_detect_profile_list_t   nd_profile_list;
} net_detect_config_t;

typedef enum net_detect_wake_reason {
	nd_reason_unknown,
	nd_net_detected,
	nd_wowl_event,
	nd_ucode_error
} net_detect_wake_reason_t;

typedef struct net_detect_wake_data {
	net_detect_wake_reason_t    nd_wake_reason;
	uint32			    nd_wake_date_length;
	uint8			    nd_wake_data[0];	    /* Wake data (currently unused) */
} net_detect_wake_data_t;

#endif /* NET_DETECT */

#endif /* LINUX_POSTMOGRIFY_REMOVAL */

typedef struct bcnreq {
	uint8 bcn_mode;
	int dur;
	int channel;
	struct ether_addr da;
	uint16 random_int;
	wlc_ssid_t ssid;
	uint16 reps;
} bcnreq_t;

typedef struct rrmreq {
	struct ether_addr da;
	uint8 reg;
	uint8 chan;
	uint16 random_int;
	uint16 dur;
	uint16 reps;
} rrmreq_t;

typedef struct framereq {
	struct ether_addr da;
	uint8 reg;
	uint8 chan;
	uint16 random_int;
	uint16 dur;
	struct ether_addr ta;
	uint16 reps;
} framereq_t;

typedef struct statreq {
	struct ether_addr da;
	struct ether_addr peer;
	uint16 random_int;
	uint16 dur;
	uint8 group_id;
	uint16 reps;
} statreq_t;

#define WL_RRM_RPT_VER		0
#define WL_RRM_RPT_MAX_PAYLOAD	64
#define WL_RRM_RPT_MIN_PAYLOAD	7
#define WL_RRM_RPT_FALG_ERR	0
#define WL_RRM_RPT_FALG_OK	1
typedef struct {
	uint16 ver;		/* version */
	struct ether_addr addr;	/* STA MAC addr */
	uint32 timestamp;	/* timestamp of the report */
	uint16 flag;		/* flag */
	uint16 len;		/* length of payload data */
	unsigned char data[WL_RRM_RPT_MAX_PAYLOAD];
} statrpt_t;

typedef struct wlc_l2keepalive_ol_params {
	uint8	flags;
	uint8	prio;
	uint16	period_ms;
} wlc_l2keepalive_ol_params_t;

typedef struct wlc_dwds_config {
	uint32		enable;
	uint32		mode; /* STA/AP interface */
	struct ether_addr ea;
} wlc_dwds_config_t;

typedef struct wl_el_set_params_s {
	uint8 set;	/* Set number */
	uint32 size;	/* Size to make/expand */
} wl_el_set_params_t;

typedef struct wl_el_tag_params_s {
	uint16 tag;
	uint8 set;
	uint8 flags;
} wl_el_tag_params_t;

/* Video Traffic Interference Monitor config */
#define INTFER_VERSION		1
typedef struct wl_intfer_params {
	uint16 version;			/* version */
	uint8 period;			/* sample period */
	uint8 cnt;			/* sample cnt */
	uint8 txfail_thresh;	/* non-TCP txfail threshold */
	uint8 tcptxfail_thresh;	/* tcptxfail threshold */
} wl_intfer_params_t;

typedef struct wl_staprio_cfg {
	struct ether_addr ea;	/* mac addr */
	uint8 prio;		/* scb priority */
} wl_staprio_cfg_t;

typedef enum wl_stamon_cfg_cmd_type {
	STAMON_CFG_CMD_DEL = 0,
	STAMON_CFG_CMD_ADD = 1
} wl_stamon_cfg_cmd_type_t;

typedef struct wlc_stamon_sta_config {
	wl_stamon_cfg_cmd_type_t cmd; /* 0 - delete, 1 - add */
	struct ether_addr ea;
} wlc_stamon_sta_config_t;
#ifdef SR_DEBUG
typedef struct /* pmu_reg */{
	uint32  pmu_control;
	uint32  pmu_capabilities;
	uint32  pmu_status;
	uint32  res_state;
	uint32  res_pending;
	uint32  pmu_timer1;
	uint32  min_res_mask;
	uint32  max_res_mask;
	uint32  pmu_chipcontrol1[4];
	uint32  pmu_regcontrol[5];
	uint32  pmu_pllcontrol[5];
	uint32  pmu_rsrc_up_down_timer[31];
	uint32  rsrc_dep_mask[31];
} pmu_reg_t;
#endif /* pmu_reg */

typedef struct wl_taf_define {
	struct ether_addr ea;	/* STA MAC or 0xFF... */
	uint16 version;         /* version */
	uint32 sch;             /* method index */
	uint32 prio;            /* priority */
	uint32 misc;            /* used for return value */
	char   text[1];         /* used to pass and return ascii text */
} wl_taf_define_t;

/* WDS net interface types */
#define WL_WDSIFTYPE_NONE  0x0 /* The interface type is neither WDS nor DWDS. */
#define WL_WDSIFTYPE_WDS   0x1 /* The interface is WDS type. */
#define WL_WDSIFTYPE_DWDS  0x2 /* The interface is DWDS type. */

typedef struct wl_bssload_static {
	bool is_static;
	uint16 sta_count;
	uint8 chan_util;
	uint16 aac;
} wl_bssload_static_t;

/* Received Beacons lengths information */
#define WL_LAST_BCNS_INFO_FIXED_LEN		OFFSETOF(wlc_bcn_len_hist_t, bcnlen_ring)
typedef struct wlc_bcn_len_hist {
	uint16	ver;				/* version field */
	uint16	cur_index;			/* current pointed index in ring buffer */
	uint32	max_bcnlen;		/* Max beacon length received */
	uint32	min_bcnlen;		/* Min beacon length received */
	uint32	ringbuff_len;		/* Length of the ring buffer 'bcnlen_ring' */
	uint32	bcnlen_ring[1];	/* ring buffer storing received beacon lengths */
} wlc_bcn_len_hist_t;

#ifdef ATE_BUILD
/* Buffer of size WLC_SAMPLECOLLECT_MAXLEN (=10240 for 4345a0 ACPHY)
 * gets copied to this, multiple times
 */
typedef enum wl_gpaio_option {
	GPAIO_PMU_AFELDO,
	GPAIO_PMU_TXLDO,
	GPAIO_PMU_VCOLDO,
	GPAIO_PMU_LNALDO,
	GPAIO_PMU_ADCLDO,
	GPAIO_PMU_CLEAR
} wl_gpaio_option_t;
#endif /* ATE_BUILD */

/* IO Var Operations - the Value of iov_op In wlc_ap_doiovar */
typedef enum wlc_ap_iov_operation {
	WLC_AP_IOV_OP_DELETE                   = -1,
	WLC_AP_IOV_OP_DISABLE                  = 0,
	WLC_AP_IOV_OP_ENABLE                   = 1,
	WLC_AP_IOV_OP_MANUAL_AP_BSSCFG_CREATE  = 2,
	WLC_AP_IOV_OP_MANUAL_STA_BSSCFG_CREATE = 3
} wlc_ap_iov_oper_t;

typedef struct {
	uint32 config;	/* MODE: AUTO (-1), Disable (0), Enable (1) */
	uint32 status;	/* Current state: Disabled (0), Enabled (1) */
} wl_config_t;

/* Multiple roaming profile suport */
#define WL_MAX_ROAM_PROF_BRACKETS	4

#define WL_MAX_ROAM_PROF_VER	0

#define WL_ROAM_PROF_NONE	(0 << 0)
#define WL_ROAM_PROF_LAZY	(1 << 0)
#define WL_ROAM_PROF_NO_CI	(1 << 1)
#define WL_ROAM_PROF_SUSPEND	(1 << 2)
#define WL_ROAM_PROF_SYNC_DTIM	(1 << 6)
#define WL_ROAM_PROF_DEFAULT	(1 << 7)	/* backward compatible single default profile */

typedef struct wl_roam_prof {
	int8	roam_flags;		/* bit flags */
	int8	roam_trigger;		/* RSSI trigger level per profile/RSSI bracket */
	int8	rssi_lower;
	int8	roam_delta;
	int8	rssi_boost_thresh;	/* Min RSSI to qualify for RSSI boost */
	int8	rssi_boost_delta;	/* RSSI boost for AP in the other band */
	uint16	nfscan;			/* nuber of full scan to start with */
	uint16	fullscan_period;
	uint16	init_scan_period;
	uint16	backoff_multiplier;
	uint16	max_scan_period;
} wl_roam_prof_t;

typedef struct wl_roam_prof_band {
	uint32	band;			/* Must be just one band */
	uint16	ver;			/* version of this struct */
	uint16	len;			/* length in bytes of this structure */
	wl_roam_prof_t roam_prof[WL_MAX_ROAM_PROF_BRACKETS];
} wl_roam_prof_band_t;

/* LTE coex info (high driver part?) */
/* Analogue of HCI Set MWS Signaling cmd */
typedef struct {
	uint16	mws_rx_assert_offset;
	uint16	mws_rx_assert_jitter;
	uint16	mws_rx_deassert_offset;
	uint16	mws_rx_deassert_jitter;
	uint16	mws_tx_assert_offset;
	uint16	mws_tx_assert_jitter;
	uint16	mws_tx_deassert_offset;
	uint16	mws_tx_deassert_jitter;
	uint16	mws_pattern_assert_offset;
	uint16	mws_pattern_assert_jitter;
	uint16	mws_inact_dur_assert_offset;
	uint16	mws_inact_dur_assert_jitter;
	uint16	mws_scan_freq_assert_offset;
	uint16	mws_scan_freq_assert_jitter;
	uint16	mws_prio_assert_offset_req;
} wci2_config_t;

/* Analogue of HCI MWS Channel Params */
typedef struct {
	uint16	mws_rx_center_freq; /* MHz */
	uint16	mws_tx_center_freq;
	uint16	mws_rx_channel_bw;  /* KHz */
	uint16	mws_tx_channel_bw;
	uint8	mws_channel_en;
	uint8	mws_channel_type;   /* Don't care for WLAN? */
} mws_params_t;

/* MWS wci2 message */
typedef struct {
	uint8	mws_wci2_data; /* BT-SIG msg */
	uint16	mws_wci2_interval; /* Interval in us */
	uint16	mws_wci2_repeat; /* No of msgs to send */
} mws_wci2_msg_t;

/* require strict packing */
#include <packed_section_start.h>

/* Data returned by the bssload_report iovar.
 * This is also the WLC_E_BSS_LOAD event data.
 */
typedef BWL_PRE_PACKED_STRUCT struct wl_bssload {
	uint16 sta_count;		/* station count */
	uint16 aac;			/* available admission capacity */
	uint8 chan_util;		/* channel utilization */
} BWL_POST_PACKED_STRUCT wl_bssload_t;

/* Maximum number of configurable BSS Load levels.  The number of BSS Load
 * ranges is always 1 more than the number of configured levels.  eg. if
 * 3 levels of 10, 20, 30 are configured then this defines 4 load ranges:
 * 0-10, 11-20, 21-30, 31-255.  A WLC_E_BSS_LOAD event is generated each time
 * the utilization level crosses into another range, subject to the rate limit.
 */
#define MAX_BSSLOAD_LEVELS 8
#define MAX_BSSLOAD_RANGES (MAX_BSSLOAD_LEVELS + 1)

/* BSS Load event notification configuration. */
typedef struct wl_bssload_cfg {
	uint32 rate_limit_msec;	/* # of events posted to application will be limited to
				 * one per specified period (0 to disable rate limit).
				 */
	uint8 num_util_levels;	/* Number of entries in util_levels[] below */
	uint8 util_levels[MAX_BSSLOAD_LEVELS];
				/* Variable number of BSS Load utilization levels in
				 * low to high order.  An event will be posted each time
				 * a received beacon's BSS Load IE channel utilization
				 * value crosses a level.
				 */
} wl_bssload_cfg_t;

typedef enum event_msgs_ext_command {
	EVENTMSGS_NONE		=	0,
	EVENTMSGS_SET_BIT	=	1,
	EVENTMSGS_RESET_BIT	=	2,
	EVENTMSGS_SET_MASK	=	3
} event_msgs_ext_command_t;

#define EVENTMSGS_VER 0
#define EVENTMSGS_EXT_STRUCT_SIZE	OFFSETOF(eventmsgs_ext_t, mask[0])

/* len-	for SET it would be mask size from the application to the firmware */
/*		for GET it would be actual firmware mask size */
/* maxgetsize -	is only used for GET. indicate max mask size that the */
/*				application can read from the firmware */
typedef struct eventmsgs_ext
{
	uint8	ver;
	uint8	command;
	uint8	len;
	uint8	maxgetsize;
	uint8	mask[1];
} eventmsgs_ext_t;

/* no default structure packing */
#include <packed_section_end.h>

#define	WL_IF_STATS_T_VERSION	1	/* current version of wl_if_stats_t struct */

/* per interface counters */
typedef struct wl_if_stats {
	uint16	version;		/* version of the structure */
	uint16	length;			/* length of the entire structure */
	uint32	PAD;			/* padding */

	/* transmit stat counters */
	uint64	txframe;		/* tx data frames */
	uint64	txbyte;			/* tx data bytes */
	uint64	txerror;		/* tx data errors (derived: sum of others) */
	uint64  txnobuf;		/* tx out of buffer errors */
	uint64  txrunt;			/* tx runt frames */
	uint64  txfail;			/* tx failed frames */
	uint64	txretry;		/* tx retry frames */
	uint64	txretrie;		/* tx multiple retry frames */
	uint64	txfrmsnt;		/* tx sent frames */
	uint64	txmulti;		/* tx mulitcast sent frames */
	uint64	txfrag;			/* tx fragments sent */

	/* receive stat counters */
	uint64	rxframe;		/* rx data frames */
	uint64	rxbyte;			/* rx data bytes */
	uint64	rxerror;		/* rx data errors (derived: sum of others) */
	uint64	rxnobuf;		/* rx out of buffer errors */
	uint64  rxrunt;			/* rx runt frames */
	uint64  rxfragerr;		/* rx fragment errors */
	uint64	rxmulti;		/* rx multicast frames */
}
wl_if_stats_t;

typedef struct wl_band {
	uint16		bandtype;		/* WL_BAND_2G, WL_BAND_5G */
	uint16		bandunit;		/* bandstate[] index */
	uint16		phytype;		/* phytype */
	uint16		phyrev;
}
wl_band_t;

#define	WL_WLC_VERSION_T_VERSION 1 /* current version of wlc_version structure */

/* wlc interface version */
typedef struct wl_wlc_version {
	uint16	version;		/* version of the structure */
	uint16	length;			/* length of the entire structure */

	/* epi version numbers */
	uint16	epi_ver_major;		/* epi major version number */
	uint16	epi_ver_minor;		/* epi minor version number */
	uint16	epi_rc_num;		/* epi RC number */
	uint16	epi_incr_num;		/* epi increment number */

	/* wlc interface version numbers */
	uint16	wlc_ver_major;		/* wlc interface major version number */
	uint16	wlc_ver_minor;		/* wlc interface minor version number */
}
wl_wlc_version_t;

/* Version of WLC interface to be returned as a part of wl_wlc_version structure.
 * For the discussion related to versions update policy refer to
 * http://hwnbu-twiki.broadcom.com/bin/view/Mwgroup/WlShimAbstractionLayer
 * For now the policy is to increment WLC_VERSION_MAJOR each time
 * there is a change that involves both WLC layer and per-port layer.
 * WLC_VERSION_MINOR is currently not in use.
 */
#define WLC_VERSION_MAJOR	2
#define WLC_VERSION_MINOR	0

/* current version of WLC interface supported by WL layer */
#define WL_SUPPORTED_WLC_VER_MAJOR 3
#define WL_SUPPORTED_WLC_VER_MINOR 0


#endif /* _wlioctl_h_ */
