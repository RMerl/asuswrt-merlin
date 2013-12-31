/*
 * Custom OID/ioctl definitions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Definitions subject to change without notice.
 *
 * Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
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
 * $Id: wlioctl.h 427697 2013-10-04 09:42:19Z $
 */

#ifndef _wlioctl_h_
#define	_wlioctl_h_

#include <typedefs.h>
#include <proto/ethernet.h>
#include <proto/bcmeth.h>
#include <proto/bcmip.h>
#include <proto/bcmevent.h>
#include <proto/802.11.h>
#include <bcmwifi_channels.h>
#include <bcmwifi_rates.h>

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
	cdc_ioctl_t 	msg;
	uint		data_len;
#ifndef OLYMPIC_RWL
	char            intf_name[INTF_NAME_SIZ];
#endif
} rem_ioctl_t;
#define REMOTE_SIZE	sizeof(rem_ioctl_t)
#ifdef EFI
#define BCMWL_IOCTL_GUID \
	{0xB4910A35, 0x88C5, 0x4328, { 0x90, 0x08, 0x9F, 0xB2, 0x00, 0x00, 0x0, 0x0 } }
#endif /* EFI */

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

/* bss_info_cap_t flags */
#define WL_BSS_FLAGS_FROM_BEACON	0x01	/* bss_info derived from beacon */
#define WL_BSS_FLAGS_FROM_CACHE		0x02	/* bss_info collected from cache */
#define WL_BSS_FLAGS_RSSI_ONCHANNEL 0x04 /* rssi info was received on channel (vs offchannel) */
#define WL_BSS_FLAGS_HS20			0x08    /* hotspot 2.0 capable */

/* bssinfo flag for nbss_cap */
#define VHT_BI_SGI_80MHZ			0x00000100
#define VHT_BI_80MHZ			    0x00000200
#define VHT_BI_160MHZ			    0x00000400
#define VHT_BI_8080MHZ			    0x00000800

#ifndef LINUX_POSTMOGRIFY_REMOVAL

typedef struct wl_bsscfg {
	uint32	wsec;
	uint32	WPA_auth;
	uint32	wsec_index;
	uint32	associated;
	uint32	BSS;
	uint32	phytest_on;
	struct ether_addr	prev_BSSID;
	struct ether_addr	BSSID;
} wl_bsscfg_t;

typedef struct wl_bss_config {
	uint32	atim_window;
	uint32	beacon_period;
	uint32	chanspec;
} wl_bss_config_t;

#define WL_BSS_USER_RADAR_CHAN_SELECT	0x1	/* User application will randomply select
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
	uchar		SSID[32];
} wlc_ssid_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL

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

#define WL_BSSTYPE_INFRA 1
#define WL_BSSTYPE_INDEP 0
#define WL_BSSTYPE_ANY   2

/* Bitmask for scan_type */
#define WL_SCANFLAGS_PASSIVE	0x01	/* force passive scan */
#define WL_SCANFLAGS_RESERVED	0x02	/* Reserved */
#define WL_SCANFLAGS_PROHIBITED	0x04	/* allow scanning prohibited channels */
#define WL_SCANFLAGS_OFFCHAN	0x08	/* allow scanning/reporting off-channel APs */
#define WL_SCANFLAGS_HOTSPOT	0x10	/* automatic ANQP to hotspot APs */

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

/* masks for channel and ssid count */
#define WL_SCAN_PARAMS_COUNT_MASK 0x0000ffff
#define WL_SCAN_PARAMS_NSSID_SHIFT 16

#define WL_SCAN_ACTION_START      1
#define WL_SCAN_ACTION_CONTINUE   2
#define WL_SCAN_ACTION_ABORT      3

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

/* wl_iscan_results status values */
#define WL_SCAN_RESULTS_SUCCESS	0
#define WL_SCAN_RESULTS_PARTIAL	1
#define WL_SCAN_RESULTS_PENDING	2
#define WL_SCAN_RESULTS_ABORTED	3
#define WL_SCAN_RESULTS_NO_MEM  4

/* Used in EXT_STA */
#define DNGL_RXCTXT_SIZE	45

#if defined(SIMPLE_ISCAN)
#define ISCAN_RETRY_CNT   5
#define ISCAN_STATE_IDLE   0
#define ISCAN_STATE_SCANING 1
#define ISCAN_STATE_PENDING 2

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

#define SCANOL_ENABLED			(1 << 0)
#define SCANOL_BCAST_SSID		(1 << 1)
#define SCANOL_NOTIFY_BCAST_SSID	(1 << 2)
#define SCANOL_RESULTS_PER_CYCLE	(1 << 3)
#define SCANOL_PARAMS_VERSION	1

/* scan times in milliseconds */
#define SCANOL_HOME_TIME		45	/* for home channel processing */
#define SCANOL_ASSOC_TIME		20	/* dwell on a channel while associated */
#define SCANOL_UNASSOC_TIME		40	/* dwell on a channel while unassociated */
#define SCANOL_PASSIVE_TIME		110	/* listen on a channelfor passive scan */
#define SCANOL_AWAY_LIMIT		100	/* max time to be away from home channel */
#define SCANOL_IDLE_REST_TIME		40
#define SCANOL_IDLE_REST_MULTIPLIER	0
#define SCANOL_ACTIVE_REST_TIME		20
#define SCANOL_ACTIVE_REST_MULTIPLIER	0
#define SCANOL_CYCLE_IDLE_REST_TIME	300000	/* Idle Rest Time between Scan Cycle (msec) */
#define SCANOL_CYCLE_IDLE_REST_MULTIPLIER	0	/* Idle Rest Time Multiplier */
#define SCANOL_CYCLE_ACTIVE_REST_TIME	200
#define SCANOL_CYCLE_ACTIVE_REST_MULTIPLIER	0
#define SCANOL_MAX_REST_TIME		3600000	/* max rest time between scan cycle (msec) */
#define SCANOL_CYCLE_DEFAULT		0	/* default for Max Scan Cycle, 0 = forever */
#define SCANOL_CYCLE_MAX		864000	/* Max Scan Cycle */
						/* 10 sec/scan cycle => 100 days */
#define SCANOL_NPROBES			2	/* for Active scan; send n probes on each channel */
#define SCANOL_NPROBES_MAX		5	/* for Active scan; send n probes on each channel */
#define SCANOL_SCAN_START_DLY		10	/* delay start of offload scan (sec) */
#define SCANOL_SCAN_START_DLY_MAX	240	/* delay start of offload scan (sec) */
#define SCANOL_MULTIPLIER_MAX		10	/* Max Multiplier */
#define SCANOL_UNASSOC_TIME_MAX		100	/* max dwell on a channel while unassociated */
#define SCANOL_PASSIVE_TIME_MAX		500	/* max listen on a channel for passive scan */
#define SCANOL_SSID_MAX			16	/* max supported preferred SSID */

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
	uint16 bssid_cnt;               /* 0: just use chanspec_num,
					* otherwise count of chanspecs in chanspec_list
					* AND paired bssids following chanspec_list
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

/* All builds use the new 11ac ratespec/chanspec */
#undef  D11AC_IOTYPES
#define D11AC_IOTYPES

#ifndef D11AC_IOTYPES

/* defines used by the nrate iovar */
#define NRATE_MCS_INUSE	0x00000080	/* MSC in use,indicates b0-6 holds an mcs */
#define NRATE_RATE_MASK 0x0000007f	/* rate/mcs value */
#define NRATE_STF_MASK	0x0000ff00	/* stf mode mask: siso, cdd, stbc, sdm */
#define NRATE_STF_SHIFT	8			/* stf mode shift */
#define NRATE_OVERRIDE	0x80000000	/* bit indicates override both rate & mode */
#define NRATE_OVERRIDE_MCS_ONLY 0x40000000 /* bit indicate to override mcs only */
#define NRATE_SGI_MASK  0x00800000      /* sgi mode */
#define NRATE_SGI_SHIFT 23              /* sgi mode */
#define NRATE_LDPC_CODING 0x00400000    /* bit indicates adv coding in use */
#define NRATE_LDPC_SHIFT 22             /* ldpc shift */

#define NRATE_STF_SISO	0		/* stf mode SISO */
#define NRATE_STF_CDD	1		/* stf mode CDD */
#define NRATE_STF_STBC	2		/* stf mode STBC */
#define NRATE_STF_SDM	3		/* stf mode SDM */

#else /* D11AC_IOTYPES */

/* WL_RSPEC defines for rate information */
#define WL_RSPEC_RATE_MASK      0x000000FF      /* rate or HT MCS value */
#define WL_RSPEC_VHT_MCS_MASK   0x0000000F      /* VHT MCS value */
#define WL_RSPEC_VHT_NSS_MASK   0x000000F0      /* VHT Nss value */
#define WL_RSPEC_VHT_NSS_SHIFT  4               /* VHT Nss value shift */
#define WL_RSPEC_TXEXP_MASK     0x00000300
#define WL_RSPEC_TXEXP_SHIFT    8
#define WL_RSPEC_BW_MASK        0x00070000      /* bandwidth mask */
#define WL_RSPEC_BW_SHIFT       16              /* bandwidth shift */
#define WL_RSPEC_STBC           0x00100000      /* STBC encoding, Nsts = 2 x Nss */
#define WL_RSPEC_TXBF           0x00200000      /* bit indicates TXBF mode */
#define WL_RSPEC_LDPC           0x00400000      /* bit indicates adv coding in use */
#define WL_RSPEC_SGI            0x00800000      /* Short GI mode */
#define WL_RSPEC_ENCODING_MASK  0x03000000      /* Encoding of Rate/MCS field */
#define WL_RSPEC_OVERRIDE_RATE  0x40000000      /* bit indicate to override mcs only */
#define WL_RSPEC_OVERRIDE_MODE  0x80000000      /* bit indicates override both rate & mode */

/* WL_RSPEC_ENCODING field defs */
#define WL_RSPEC_ENCODE_RATE    0x00000000      /* Legacy rate is stored in RSPEC_RATE_MASK */
#define WL_RSPEC_ENCODE_HT      0x01000000      /* HT MCS is stored in RSPEC_RATE_MASK */
#define WL_RSPEC_ENCODE_VHT     0x02000000      /* VHT MCS and Nss is stored in RSPEC_RATE_MASK */

/* WL_RSPEC_BW field defs */
#define WL_RSPEC_BW_UNSPECIFIED 0
#define WL_RSPEC_BW_20MHZ       0x00010000
#define WL_RSPEC_BW_40MHZ       0x00020000
#define WL_RSPEC_BW_80MHZ       0x00030000
#define WL_RSPEC_BW_160MHZ      0x00040000

/* Legacy defines for the nrate iovar */
#define OLD_NRATE_MCS_INUSE         0x00000080 /* MSC in use,indicates b0-6 holds an mcs */
#define OLD_NRATE_RATE_MASK         0x0000007f /* rate/mcs value */
#define OLD_NRATE_STF_MASK          0x0000ff00 /* stf mode mask: siso, cdd, stbc, sdm */
#define OLD_NRATE_STF_SHIFT         8          /* stf mode shift */
#define OLD_NRATE_OVERRIDE          0x80000000 /* bit indicates override both rate & mode */
#define OLD_NRATE_OVERRIDE_MCS_ONLY 0x40000000 /* bit indicate to override mcs only */
#define OLD_NRATE_SGI               0x00800000 /* sgi mode */
#define OLD_NRATE_LDPC_CODING       0x00400000 /* bit indicates adv coding in use */

#define OLD_NRATE_STF_SISO	0		/* stf mode SISO */
#define OLD_NRATE_STF_CDD	1		/* stf mode CDD */
#define OLD_NRATE_STF_STBC	2		/* stf mode STBC */
#define OLD_NRATE_STF_SDM	3		/* stf mode SDM */

#endif /* D11AC_IOTYPES */

#define ANTENNA_NUM_1	1		/* total number of antennas to be used */
#define ANTENNA_NUM_2	2
#define ANTENNA_NUM_3	3
#define ANTENNA_NUM_4	4

#define ANT_SELCFG_AUTO		0x80	/* bit indicates antenna sel AUTO */
#define ANT_SELCFG_MASK		0x33	/* antenna configuration mask */
#define ANT_SELCFG_MAX		4	/* max number of antenna configurations */
#define ANT_SELCFG_TX_UNICAST	0	/* unicast tx antenna configuration */
#define ANT_SELCFG_RX_UNICAST	1	/* unicast rx antenna configuration */
#define ANT_SELCFG_TX_DEF	2	/* default tx antenna configuration */
#define ANT_SELCFG_RX_DEF	3	/* default rx antenna configuration */

#define MAX_STREAMS_SUPPORTED	4	/* max number of streams supported */

typedef struct {
	uint8 ant_config[ANT_SELCFG_MAX];	/* antenna configuration */
	uint8 num_antcfg;	/* number of available antenna configurations */
} wlc_antselcfg_t;

#define HIGHEST_SINGLE_STREAM_MCS	7 /* MCS values greater than this enable multiple streams */

#define MAX_CCA_CHANNELS 38	/* Max number of 20 Mhz wide channels */
#define MAX_CCA_SECS     60	/* CCA keeps this many seconds history */

#define IBSS_MED        15	/* Mediom in-bss congestion percentage */
#define IBSS_HI         25	/* Hi in-bss congestion percentage */
#define OBSS_MED        12
#define OBSS_HI         25
#define INTERFER_MED    5
#define INTERFER_HI     10

#define  CCA_FLAG_2G_ONLY		0x01	/* Return a channel from 2.4 Ghz band */
#define  CCA_FLAG_5G_ONLY		0x02	/* Return a channel from 2.4 Ghz band */
#define  CCA_FLAG_IGNORE_DURATION	0x04	/* Ignore dwell time for each channel */
#define  CCA_FLAGS_PREFER_1_6_11	0x10
#define  CCA_FLAG_IGNORE_INTERFER 	0x20 /* do not exlude channel based on interfer level */

#define CCA_ERRNO_BAND 		1	/* After filtering for band pref, no choices left */
#define CCA_ERRNO_DURATION	2	/* After filtering for duration, no choices left */
#define CCA_ERRNO_PREF_CHAN	3	/* After filtering for chan pref, no choices left */
#define CCA_ERRNO_INTERFER	4	/* After filtering for interference, no choices left */
#define CCA_ERRNO_TOO_FEW	5	/* Only 1 channel was input */

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
	uint8 num_secs;		/* How many secs worth of data */
	cca_congest_t  secs[1];	/* Data */
} cca_congest_channel_req_t;

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

/* interference source detection and identification mode */
#define ITFR_MODE_DISABLE	0	/* disable feature */
#define ITFR_MODE_MANUAL_ENABLE	1	/* enable manual detection */
#define ITFR_MODE_AUTO_ENABLE	2	/* enable auto detection */

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

/* bit definitions for flags in interference source report */
#define ITFR_INTERFERENCED	1	/* interference detected */
#define ITFR_HOME_CHANNEL	2	/* home channel has interference */
#define ITFR_NOISY_ENVIRONMENT	4	/* noisy environemnt so feature stopped */

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

#define WL_NUM_RPI_BINS		8
#define WL_RM_TYPE_BASIC	1
#define WL_RM_TYPE_CCA		2
#define WL_RM_TYPE_RPI		3

#define WL_RM_FLAG_PARALLEL	(1<<0)

#define WL_RM_FLAG_LATE		(1<<1)
#define WL_RM_FLAG_INCAPABLE	(1<<2)
#define WL_RM_FLAG_REFUSED	(1<<3)

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


#if defined(BCMSUP_PSK)
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

/* Enumerate crypto algorithms */
#define	CRYPTO_ALGO_OFF			0
#define	CRYPTO_ALGO_WEP1		1
#define	CRYPTO_ALGO_TKIP		2
#define	CRYPTO_ALGO_WEP128		3
#define CRYPTO_ALGO_AES_CCM		4
#define CRYPTO_ALGO_AES_OCB_MSDU	5
#define CRYPTO_ALGO_AES_OCB_MPDU	6
#define CRYPTO_ALGO_NALG		7
#define CRYPTO_ALGO_PMK			12	/* for 802.1x supp to set PMK before 4-way */
#define CRYPTO_ALGO_BIP			13  /* 802.11w BIP (aes cmac) */

#define WSEC_GEN_MIC_ERROR	0x0001
#define WSEC_GEN_REPLAY		0x0002
#define WSEC_GEN_ICV_ERROR	0x0004
#define WSEC_GEN_MFP_ACT_ERROR	0x0008
#define WSEC_GEN_MFP_DISASSOC_ERROR	0x0010
#define WSEC_GEN_MFP_DEAUTH_ERROR	0x0020

#define WL_SOFT_KEY	(1 << 0)	/* Indicates this key is using soft encrypt */
#define WL_PRIMARY_KEY	(1 << 1)	/* Indicates this key is the primary (ie tx) key */
#define WL_KF_RES_4	(1 << 4)	/* Reserved for backward compat */
#define WL_KF_RES_5	(1 << 5)	/* Reserved for backward compat */
#define WL_IBSS_PEER_GROUP_KEY	(1 << 6)	/* Indicates a group key for a IBSS PEER */

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

/* wireless security bitvec */
#define WEP_ENABLED		0x0001
#define TKIP_ENABLED		0x0002
#define AES_ENABLED		0x0004
#define WSEC_SWFLAG		0x0008
#define SES_OW_ENABLED		0x0040	/* to go into transition mode without setting wep */

/* wsec macros for operating on the above definitions */
#define WSEC_WEP_ENABLED(wsec)	((wsec) & WEP_ENABLED)
#define WSEC_TKIP_ENABLED(wsec)	((wsec) & TKIP_ENABLED)
#define WSEC_AES_ENABLED(wsec)	((wsec) & AES_ENABLED)

#define WSEC_ENABLED(wsec)	((wsec) & (WEP_ENABLED | TKIP_ENABLED | AES_ENABLED))
#define WSEC_SES_OW_ENABLED(wsec)	((wsec) & SES_OW_ENABLED)

#define MFP_CAPABLE		0x0200
#define MFP_REQUIRED	0x0400
#define MFP_SHA256		0x0800 /* a special configuration for STA for WIFI test tool */

/* WPA authentication mode bitvec */
#define WPA_AUTH_DISABLED	0x0000	/* Legacy (i.e., non-WPA) */
#define WPA_AUTH_NONE		0x0001	/* none (IBSS) */
#define WPA_AUTH_UNSPECIFIED	0x0002	/* over 802.1x */
#define WPA_AUTH_PSK		0x0004	/* Pre-shared key */
/* #define WPA_AUTH_8021X 0x0020 */	/* 802.1x, reserved */
#define WPA2_AUTH_UNSPECIFIED	0x0040	/* over 802.1x */
#define WPA2_AUTH_PSK		0x0080	/* Pre-shared key */
#define BRCM_AUTH_PSK           0x0100  /* BRCM specific PSK */
#define BRCM_AUTH_DPT		0x0200	/* DPT PSK without group keys */
#define WPA2_AUTH_MFP           0x1000  /* MFP (11w) in contrast to CCX */
#define WPA2_AUTH_TPK		0x2000 	/* TDLS Peer Key */
#define WPA2_AUTH_FT		0x4000 	/* Fast Transition. */
#define WPA_AUTH_PFN_ANY	0xffffffff	/* for PFN, match only ssid */

/* pmkid */
#define	MAXPMKID		16

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

#ifndef LINUX_POSTMOGRIFY_REMOVAL
typedef struct wl_assoc_info {
	uint32		req_len;
	uint32		resp_len;
	uint32		flags;
	struct dot11_assoc_req req;
	struct ether_addr reassoc_bssid; /* used in reassoc's */
	struct dot11_assoc_resp resp;
} wl_assoc_info_t;

/* flags */
#define WLC_ASSOC_REQ_IS_REASSOC 0x01 /* assoc req was actually a reassoc */

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

/* similar cis (srom or otp) struct [iovar: may not be aligned] */
typedef struct {
	uint32	source;		/* cis source */
	uint32	byteoff;	/* byte offset */
	uint32	nbytes;		/* number of bytes */
	/* data follows here */
} cis_rw_t;

#define WLC_CIS_DEFAULT	0	/* built-in default */
#define WLC_CIS_SROM	1	/* source is sprom */
#define WLC_CIS_OTP	2	/* source is otp */

/* R_REG and W_REG struct passed through ioctl */
typedef struct {
	uint32	byteoff;	/* byte offset of the field in d11regs_t */
	uint32	val;		/* read/write value of the field */
	uint32	size;		/* sizeof the field */
	uint	band;		/* band (optional) */
} rw_reg_t;

/* Structure used by GET/SET_ATTEN ioctls - it controls power in b/g-band */
/* PCL - Power Control Loop */
/* current gain setting is replaced by user input */
#define WL_ATTEN_APP_INPUT_PCL_OFF	0	/* turn off PCL, apply supplied input */
#define WL_ATTEN_PCL_ON			1	/* turn on PCL */
/* current gain setting is maintained */
#define WL_ATTEN_PCL_OFF		2	/* turn off PCL. */

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

/* defines used by poweridx iovar - it controls power in a-band */
/* current gain setting is maintained */
#define WL_PWRIDX_PCL_OFF	-2	/* turn off PCL.  */
#define WL_PWRIDX_PCL_ON	-1	/* turn on PCL */
#define WL_PWRIDX_LOWER_LIMIT	-2	/* lower limit */
#define WL_PWRIDX_UPPER_LIMIT	63	/* upper limit */
/* value >= 0 causes
 *	- input to be set to that value
 *	- PCL to be off
 */

/* Used to get specific link/ac parameters */
typedef struct {
	int ac;
	uint8 val;
	struct ether_addr ea;
} link_val_t;

#define BCM_MAC_STATUS_INDICATION	(0x40010200L)


#define WL_STA_ANT_MAX		4		/* max possible rx antennas */

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
	uint32			tx_pkts;	/* # of packets transmitted */
	uint32			tx_failures;	/* # of packets failed */
	uint32			rx_ucast_pkts;	/* # of unicast packets received */
	uint32			rx_mcast_pkts;	/* # of multicast packets received */
	uint32			tx_rate;	/* Rate of last successful tx frame */
	uint32			rx_rate;	/* Rate of last successful rx frame */
	uint32			rx_decrypt_succeeds;	/* # of packet decrypted successfully */
	uint32			rx_decrypt_failures;	/* # of packet decrypted unsuccessfully */
	uint32			tx_tot_pkts;	/* # of tx pkts (ucast + mcast) */
	uint32 			rx_tot_pkts;	/* # of data packets recvd (uni + mcast) */
	uint32			tx_mcast_pkts;	/* # of mcast pkts txed */
	uint64			tx_tot_bytes;	/* data bytes txed (ucast + mcast) */
	uint64			rx_tot_bytes;	/* data bytes recvd (ucast + mcast) */
	uint64			tx_ucast_bytes;	/* data bytes txed (ucast) */
	uint64			tx_mcast_bytes;	/* # data bytes txed (mcast) */
	uint64			rx_ucast_bytes;	/* data bytes recvd (ucast) */
	uint64			rx_mcast_bytes;	/* data bytes recvd (mcast) */
	int8			rssi[WL_STA_ANT_MAX];	/* average rssi per antenna of data
						         * frames
						         */
	int8			nf[WL_STA_ANT_MAX];	/* per antenna noise floor */
	uint16			aid;		/* association ID */
	uint16			ht_capabilities;	/* advertised ht caps */
	uint16			vht_flags;		/* converted vht flags */
	uint32			tx_pkts_retried;		/* # of frames where a retry was
						                 *   necessary
						                 */
	uint32			tx_pkts_retry_exhausted; /* # of frames where a retry was
						          * exhausted
						          */
	int8			rx_lastpkt_rssi[WL_STA_ANT_MAX]; /* Per antenna RSSI of last
						                  * received data frame
						                  */
} sta_info_t;

#define WL_OLD_STAINFO_SIZE	OFFSETOF(sta_info_t, tx_tot_pkts)

#define WL_STA_VER		4

#define WL_STA_AID(a)		((a) &~ 0xc000)

/* Flags for sta_info_t indicating properties of STA */
#define WL_STA_BRCM		0x00000001	/* Running a Broadcom driver */
#define WL_STA_WME		0x00000002	/* WMM association */
#define WL_STA_NONERP		0x00000004	/* No ERP */
#define WL_STA_AUTHE		0x00000008	/* Authenticated */
#define WL_STA_ASSOC		0x00000010	/* Associated */
#define WL_STA_AUTHO		0x00000020	/* Authorized */
#define WL_STA_WDS		0x00000040	/* Wireless Distribution System */
#define WL_STA_WDS_LINKUP	0x00000080	/* WDS traffic/probes flowing properly */
#define WL_STA_PS		0x00000100	/* STA is in power save mode from AP's viewpoint */
#define WL_STA_APSD_BE		0x00000200	/* APSD delv/trigger for AC_BE is default enabled */
#define WL_STA_APSD_BK		0x00000400	/* APSD delv/trigger for AC_BK is default enabled */
#define WL_STA_APSD_VI		0x00000800	/* APSD delv/trigger for AC_VI is default enabled */
#define WL_STA_APSD_VO		0x00001000	/* APSD delv/trigger for AC_VO is default enabled */
#define WL_STA_N_CAP		0x00002000	/* STA 802.11n capable */
#define WL_STA_SCBSTATS		0x00004000	/* Per STA debug stats */
#define WL_STA_AMPDU_CAP	0x00008000	/* STA AMPDU capable */
#define WL_STA_AMSDU_CAP	0x00010000	/* STA AMSDU capable */
#define WL_STA_MIMO_PS		0x00020000	/* mimo ps mode is enabled */
#define WL_STA_MIMO_RTS		0x00040000	/* send rts in mimo ps mode */
#define WL_STA_RIFS_CAP		0x00080000	/* rifs enabled */
#define WL_STA_VHT_CAP		0x00100000	/* STA VHT(11ac) capable */

#define WL_WDS_LINKUP		WL_STA_WDS_LINKUP	/* deprecated */
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

#define	WLC_NUMRATES	16	/* max # of rates in a rateset */

typedef struct wlc_rateset {
	uint	count;			/* number of rates in rates[] */
	uint8	rates[WLC_NUMRATES];	/* rates in 500kbps units w/hi bit set if basic */
	uint8	htphy_membership;	/* HT PHY Membership */
	uint8	mcs[MCSSET_LEN];	/* supported mcs index bit map */
	uint16  vht_mcsmap;		/* supported vht mcs nss bit map */
} wlc_rateset_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL
/* STA HT cap fields */
#define WL_STA_CAP_LDPC_CODING		0x0001	/* Support for rx of LDPC coded pkts */
#define WL_STA_CAP_40MHZ		0x0002  /* FALSE:20Mhz, TRUE:20/40MHZ supported */
#define WL_STA_CAP_MIMO_PS_MASK		0x000C  /* Mimo PS mask */
#define WL_STA_CAP_MIMO_PS_SHIFT	0x0002	/* Mimo PS shift */
#define WL_STA_CAP_MIMO_PS_OFF		0x0003	/* Mimo PS, no restriction */
#define WL_STA_CAP_MIMO_PS_RTS		0x0001	/* Mimo PS, send RTS/CTS around MIMO frames */
#define WL_STA_CAP_MIMO_PS_ON		0x0000	/* Mimo PS, MIMO disallowed */
#define WL_STA_CAP_GF			0x0010	/* Greenfield preamble support */
#define WL_STA_CAP_SHORT_GI_20		0x0020	/* 20MHZ short guard interval support */
#define WL_STA_CAP_SHORT_GI_40		0x0040	/* 40Mhz short guard interval support */
#define WL_STA_CAP_TX_STBC		0x0080	/* Tx STBC support */
#define WL_STA_CAP_RX_STBC_MASK		0x0300	/* Rx STBC mask */
#define WL_STA_CAP_RX_STBC_SHIFT	8	/* Rx STBC shift */
#define WL_STA_CAP_DELAYED_BA		0x0400	/* delayed BA support */
#define WL_STA_CAP_MAX_AMSDU		0x0800	/* Max AMSDU size in bytes , 0=3839, 1=7935 */
#define WL_STA_CAP_DSSS_CCK		0x1000	/* DSSS/CCK supported by the BSS */
#define WL_STA_CAP_PSMP			0x2000	/* Power Save Multi Poll support */
#define WL_STA_CAP_40MHZ_INTOLERANT 	0x4000	/* 40MHz Intolerant */
#define WL_STA_CAP_LSIG_TXOP		0x8000	/* L-SIG TXOP protection support */

#define WL_STA_CAP_RX_STBC_NO		0x0	/* no rx STBC support */
#define WL_STA_CAP_RX_STBC_ONE_STREAM	0x1	/* rx STBC support of 1 spatial stream */
#define WL_STA_CAP_RX_STBC_TWO_STREAM	0x2	/* rx STBC support of 1-2 spatial streams */
#define WL_STA_CAP_RX_STBC_THREE_STREAM	0x3	/* rx STBC support of 1-3 spatial streams */

/* scb vht flags */
#define WL_STA_VHT_LDPCCAP	0x0001
#define WL_STA_SGI80       	0x0002
#define WL_STA_SGI160		0x0004
#define WL_STA_VHT_TX_STBCCAP	0x0008
#define WL_STA_VHT_RX_STBCCAP	0x0010
#define WL_STA_SU_BEAMFORMER	0x0020
#define WL_STA_SU_BEAMFORMEE	0x0040
#define WL_STA_MU_BEAMFORMER	0x0080
#define WL_STA_MU_BEAMFORMEE	0x0100
#define WL_STA_VHT_TXOP_PS	0x0200
#define WL_STA_HTC_VHT_CAP	0x0400

/* Values for TX Filter override mode */
#define WLC_TXFILTER_OVERRIDE_DISABLED  0
#define WLC_TXFILTER_OVERRIDE_ENABLED   1

#endif /* LINUX_POSTMOGRIFY_REMOVAL */

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
struct maclist {
	uint count;			/* number of MAC addresses */
	struct ether_addr ea[1];	/* variable length array of MAC addresses */
};

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

/* Values for TX Filter override mode */
#define WLC_TXFILTER_OVERRIDE_DISABLED  0
#define WLC_TXFILTER_OVERRIDE_ENABLED   1

#define WL_IOCTL_ACTION_GET				0x0
#define WL_IOCTL_ACTION_SET				0x1
#define WL_IOCTL_ACTION_OVL_IDX_MASK	0x1e
#define WL_IOCTL_ACTION_OVL_RSV			0x20
#define WL_IOCTL_ACTION_OVL				0x40
#define WL_IOCTL_ACTION_MASK			0x7e
#define WL_IOCTL_ACTION_OVL_SHIFT		1

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

#ifndef LINUX_POSTMOGRIFY_REMOVAL

/* reference to wl_ioctl_t struct used by usermode driver */
#define ioctl_subtype	set		/* subtype param */
#define ioctl_pid	used		/* pid param */
#define ioctl_status	needed		/* status param */

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
/* magic pattern used for mismatch driver and wl */
#define WL_TXFIFO_SZ_MAGIC	0xa5a5

/* Transfer info about an IOVar from the driver */
/* Max supported IOV name size in bytes, + 1 for nul termination */
#define WLC_IOV_NAME_LEN 30
typedef struct wlc_iov_trx_s {
	uint8 module;
	uint8 type;
	char name[WLC_IOV_NAME_LEN];
} wlc_iov_trx_t;

/* check this magic number */
#define WLC_IOCTL_MAGIC		0x14e46c77

/* bump this number if you change the ioctl interface */
#ifdef D11AC_IOTYPES
#define WLC_IOCTL_VERSION	2
#define WLC_IOCTL_VERSION_LEGACY_IOTYPES	1
#else
#define WLC_IOCTL_VERSION	1
#endif /* D11AC_IOTYPES */
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

#define	WLC_IOCTL_MAXLEN		8192	/* max length ioctl buffer required */
#define	WLC_IOCTL_SMLEN			256	/* "small" length ioctl buffer required */
#define WLC_IOCTL_MEDLEN		1536    /* "med" length ioctl buffer required */
#ifdef WLC_HIGH_ONLY
#define WLC_SAMPLECOLLECT_MAXLEN	1024	/* limit sample size for bmac  */
#define WLC_SAMPLECOLLECT_MAXLEN_LCN40  1024
#else
#if defined(LCNCONF) || defined(LCN40CONF)
#define WLC_SAMPLECOLLECT_MAXLEN	8192	/* Max Sample Collect buffer */
#else
#define WLC_SAMPLECOLLECT_MAXLEN	10240	/* Max Sample Collect buffer for two cores */
#endif
#define WLC_SAMPLECOLLECT_MAXLEN_LCN40  8192
#endif /* WLC_HIGH_ONLY */

/* common ioctl definitions */
#define WLC_GET_MAGIC				0
#define WLC_GET_VERSION				1
#define WLC_UP					2
#define WLC_DOWN				3
#define WLC_GET_LOOP				4
#define WLC_SET_LOOP				5
#define WLC_DUMP				6
#define WLC_GET_MSGLEVEL			7
#define WLC_SET_MSGLEVEL			8
#define WLC_GET_PROMISC				9
#define WLC_SET_PROMISC				10
/* #define WLC_OVERLAY_IOCTL			11 */ /* not supported */
#define WLC_GET_RATE				12
#define WLC_GET_MAX_RATE			13
#define WLC_GET_INSTANCE			14
/* #define WLC_GET_FRAG				15 */ /* no longer supported */
/* #define WLC_SET_FRAG				16 */ /* no longer supported */
/* #define WLC_GET_RTS				17 */ /* no longer supported */
/* #define WLC_SET_RTS				18 */ /* no longer supported */
#define WLC_GET_INFRA				19
#define WLC_SET_INFRA				20
#define WLC_GET_AUTH				21
#define WLC_SET_AUTH				22
#define WLC_GET_BSSID				23
#define WLC_SET_BSSID				24
#define WLC_GET_SSID				25
#define WLC_SET_SSID				26
#define WLC_RESTART				27
#define WLC_TERMINATED             		28
/* #define WLC_DUMP_SCB				28 */ /* no longer supported */
#define WLC_GET_CHANNEL				29
#define WLC_SET_CHANNEL				30
#define WLC_GET_SRL				31
#define WLC_SET_SRL				32
#define WLC_GET_LRL				33
#define WLC_SET_LRL				34
#define WLC_GET_PLCPHDR				35
#define WLC_SET_PLCPHDR				36
#define WLC_GET_RADIO				37
#define WLC_SET_RADIO				38
#define WLC_GET_PHYTYPE				39
#define WLC_DUMP_RATE				40
#define WLC_SET_RATE_PARAMS			41
#define WLC_GET_FIXRATE				42
#define WLC_SET_FIXRATE				43
/* #define WLC_GET_WEP				42 */ /* no longer supported */
/* #define WLC_SET_WEP				43 */ /* no longer supported */
#define WLC_GET_KEY				44
#define WLC_SET_KEY				45
#define WLC_GET_REGULATORY			46
#define WLC_SET_REGULATORY			47
#define WLC_GET_PASSIVE_SCAN			48
#define WLC_SET_PASSIVE_SCAN			49
#define WLC_SCAN				50
#define WLC_SCAN_RESULTS			51
#define WLC_DISASSOC				52
#define WLC_REASSOC				53
#define WLC_GET_ROAM_TRIGGER			54
#define WLC_SET_ROAM_TRIGGER			55
#define WLC_GET_ROAM_DELTA			56
#define WLC_SET_ROAM_DELTA			57
#define WLC_GET_ROAM_SCAN_PERIOD		58
#define WLC_SET_ROAM_SCAN_PERIOD		59
#define WLC_EVM					60	/* diag */
#define WLC_GET_TXANT				61
#define WLC_SET_TXANT				62
#define WLC_GET_ANTDIV				63
#define WLC_SET_ANTDIV				64
/* #define WLC_GET_TXPWR			65 */ /* no longer supported */
/* #define WLC_SET_TXPWR			66 */ /* no longer supported */
#define WLC_GET_CLOSED				67
#define WLC_SET_CLOSED				68
#define WLC_GET_MACLIST				69
#define WLC_SET_MACLIST				70
#define WLC_GET_RATESET				71
#define WLC_SET_RATESET				72
/* #define WLC_GET_LOCALE			73 */ /* no longer supported */
#define WLC_LONGTRAIN				74
#define WLC_GET_BCNPRD				75
#define WLC_SET_BCNPRD				76
#define WLC_GET_DTIMPRD				77
#define WLC_SET_DTIMPRD				78
#define WLC_GET_SROM				79
#define WLC_SET_SROM				80
#define WLC_GET_WEP_RESTRICT			81
#define WLC_SET_WEP_RESTRICT			82
#define WLC_GET_COUNTRY				83
#define WLC_SET_COUNTRY				84
#define WLC_GET_PM				85
#define WLC_SET_PM				86
#define WLC_GET_WAKE				87
#define WLC_SET_WAKE				88
/* #define WLC_GET_D11CNTS			89 */ /* -> "counters" iovar */
#define WLC_GET_FORCELINK			90	/* ndis only */
#define WLC_SET_FORCELINK			91	/* ndis only */
#define WLC_FREQ_ACCURACY			92	/* diag */
#define WLC_CARRIER_SUPPRESS			93	/* diag */
#define WLC_GET_PHYREG				94
#define WLC_SET_PHYREG				95
#define WLC_GET_RADIOREG			96
#define WLC_SET_RADIOREG			97
#define WLC_GET_REVINFO				98
#define WLC_GET_UCANTDIV			99
#define WLC_SET_UCANTDIV			100
#define WLC_R_REG				101
#define WLC_W_REG				102
/* #define WLC_DIAG_LOOPBACK			103	old tray diag */
/* #define WLC_RESET_D11CNTS			104 */ /* -> "reset_d11cnts" iovar */
#define WLC_GET_MACMODE				105
#define WLC_SET_MACMODE				106
#define WLC_GET_MONITOR				107
#define WLC_SET_MONITOR				108
#define WLC_GET_GMODE				109
#define WLC_SET_GMODE				110
#define WLC_GET_LEGACY_ERP			111
#define WLC_SET_LEGACY_ERP			112
#define WLC_GET_RX_ANT				113
#define WLC_GET_CURR_RATESET			114	/* current rateset */
#define WLC_GET_SCANSUPPRESS			115
#define WLC_SET_SCANSUPPRESS			116
#define WLC_GET_AP				117
#define WLC_SET_AP				118
#define WLC_GET_EAP_RESTRICT			119
#define WLC_SET_EAP_RESTRICT			120
#define WLC_SCB_AUTHORIZE			121
#define WLC_SCB_DEAUTHORIZE			122
#define WLC_GET_WDSLIST				123
#define WLC_SET_WDSLIST				124
#define WLC_GET_ATIM				125
#define WLC_SET_ATIM				126
#define WLC_GET_RSSI				127
#define WLC_GET_PHYANTDIV			128
#define WLC_SET_PHYANTDIV			129
#define WLC_AP_RX_ONLY				130
#define WLC_GET_TX_PATH_PWR			131
#define WLC_SET_TX_PATH_PWR			132
#define WLC_GET_WSEC				133
#define WLC_SET_WSEC				134
#define WLC_GET_PHY_NOISE			135
#define WLC_GET_BSS_INFO			136
#define WLC_GET_PKTCNTS				137
#define WLC_GET_LAZYWDS				138
#define WLC_SET_LAZYWDS				139
#define WLC_GET_BANDLIST			140

#ifndef LINUX_POSTMOGRIFY_REMOVAL
#define WLC_GET_BAND				141
#define WLC_SET_BAND				142
#define WLC_SCB_DEAUTHENTICATE			143
#define WLC_GET_SHORTSLOT			144
#define WLC_GET_SHORTSLOT_OVERRIDE		145
#define WLC_SET_SHORTSLOT_OVERRIDE		146
#define WLC_GET_SHORTSLOT_RESTRICT		147
#define WLC_SET_SHORTSLOT_RESTRICT		148
#define WLC_GET_GMODE_PROTECTION		149
#define WLC_GET_GMODE_PROTECTION_OVERRIDE	150
#define WLC_SET_GMODE_PROTECTION_OVERRIDE	151
#define WLC_UPGRADE				152
/* #define WLC_GET_MRATE			153 */ /* no longer supported */
/* #define WLC_SET_MRATE			154 */ /* no longer supported */
#define WLC_GET_IGNORE_BCNS			155
#define WLC_SET_IGNORE_BCNS			156
#define WLC_GET_SCB_TIMEOUT			157
#define WLC_SET_SCB_TIMEOUT			158
#define WLC_GET_ASSOCLIST			159
#define WLC_GET_CLK				160
#define WLC_SET_CLK				161
#define WLC_GET_UP				162
#define WLC_OUT					163
#define WLC_GET_WPA_AUTH			164
#define WLC_SET_WPA_AUTH			165
#define WLC_GET_UCFLAGS				166
#define WLC_SET_UCFLAGS				167
#define WLC_GET_PWRIDX				168
#define WLC_SET_PWRIDX				169
#define WLC_GET_TSSI				170
#define WLC_GET_SUP_RATESET_OVERRIDE		171
#define WLC_SET_SUP_RATESET_OVERRIDE		172
/* #define WLC_SET_FAST_TIMER			173 */ /* no longer supported */
/* #define WLC_GET_FAST_TIMER			174 */ /* no longer supported */
/* #define WLC_SET_SLOW_TIMER			175 */ /* no longer supported */
/* #define WLC_GET_SLOW_TIMER			176 */ /* no longer supported */
/* #define WLC_DUMP_PHYREGS			177 */ /* no longer supported */
#define WLC_GET_PROTECTION_CONTROL		178
#define WLC_SET_PROTECTION_CONTROL		179
#endif /* LINUX_POSTMOGRIFY_REMOVAL  */
#define WLC_GET_PHYLIST				180
#ifndef LINUX_POSTMOGRIFY_REMOVAL
#define WLC_ENCRYPT_STRENGTH			181	/* ndis only */
#define WLC_DECRYPT_STATUS			182	/* ndis only */
#define WLC_GET_KEY_SEQ				183
#define WLC_GET_SCAN_CHANNEL_TIME		184
#define WLC_SET_SCAN_CHANNEL_TIME		185
#define WLC_GET_SCAN_UNASSOC_TIME		186
#define WLC_SET_SCAN_UNASSOC_TIME		187
#define WLC_GET_SCAN_HOME_TIME			188
#define WLC_SET_SCAN_HOME_TIME			189
#define WLC_GET_SCAN_NPROBES			190
#define WLC_SET_SCAN_NPROBES			191
#define WLC_GET_PRB_RESP_TIMEOUT		192
#define WLC_SET_PRB_RESP_TIMEOUT		193
#define WLC_GET_ATTEN				194
#define WLC_SET_ATTEN				195
#define WLC_GET_SHMEM				196	/* diag */
#define WLC_SET_SHMEM				197	/* diag */
/* #define WLC_GET_GMODE_PROTECTION_CTS		198 */ /* no longer supported */
/* #define WLC_SET_GMODE_PROTECTION_CTS		199 */ /* no longer supported */
#define WLC_SET_WSEC_TEST			200
#endif /* LINUX_POSTMOGRIFY_REMOVAL */
#define WLC_SCB_DEAUTHENTICATE_FOR_REASON	201
#ifndef LINUX_POSTMOGRIFY_REMOVAL
#define WLC_TKIP_COUNTERMEASURES		202
#define WLC_GET_PIOMODE				203
#define WLC_SET_PIOMODE				204
#define WLC_SET_ASSOC_PREFER			205
#define WLC_GET_ASSOC_PREFER			206
#define WLC_SET_ROAM_PREFER			207
#define WLC_GET_ROAM_PREFER			208
#define WLC_SET_LED				209
#define WLC_GET_LED				210
#define WLC_GET_INTERFERENCE_MODE		211
#define WLC_SET_INTERFERENCE_MODE		212
#define WLC_GET_CHANNEL_QA			213
#define WLC_START_CHANNEL_QA			214
#define WLC_GET_CHANNEL_SEL			215
#define WLC_START_CHANNEL_SEL			216
#endif /* LINUX_POSTMOGRIFY_REMOVAL */
#define WLC_GET_VALID_CHANNELS			217
#define WLC_GET_FAKEFRAG			218
#define WLC_SET_FAKEFRAG			219
#define WLC_GET_PWROUT_PERCENTAGE		220
#define WLC_SET_PWROUT_PERCENTAGE		221
#define WLC_SET_BAD_FRAME_PREEMPT		222
#define WLC_GET_BAD_FRAME_PREEMPT		223
#define WLC_SET_LEAP_LIST			224
#define WLC_GET_LEAP_LIST			225
#define WLC_GET_CWMIN				226
#define WLC_SET_CWMIN				227
#define WLC_GET_CWMAX				228
#define WLC_SET_CWMAX				229
#define WLC_GET_WET				230
#define WLC_SET_WET				231
#define WLC_GET_PUB				232
/* #define WLC_SET_GLACIAL_TIMER		233 */ /* no longer supported */
/* #define WLC_GET_GLACIAL_TIMER		234 */ /* no longer supported */
#define WLC_GET_KEY_PRIMARY			235
#define WLC_SET_KEY_PRIMARY			236

#ifndef LINUX_POSTMOGRIFY_REMOVAL

/* #define WLC_DUMP_RADIOREGS			237 */ /* no longer supported */
#define WLC_GET_ACI_ARGS			238
#define WLC_SET_ACI_ARGS			239
#define WLC_UNSET_CALLBACK			240
#define WLC_SET_CALLBACK			241
#define WLC_GET_RADAR				242
#define WLC_SET_RADAR				243
#define WLC_SET_SPECT_MANAGMENT			244
#define WLC_GET_SPECT_MANAGMENT			245
#define WLC_WDS_GET_REMOTE_HWADDR		246	/* handled in wl_linux.c/wl_vx.c */
#define WLC_WDS_GET_WPA_SUP			247
#define WLC_SET_CS_SCAN_TIMER			248
#define WLC_GET_CS_SCAN_TIMER			249
#define WLC_MEASURE_REQUEST			250
#define WLC_INIT				251
#define WLC_SEND_QUIET				252
#define WLC_KEEPALIVE			253
#define WLC_SEND_PWR_CONSTRAINT			254
#define WLC_UPGRADE_STATUS			255
#define WLC_CURRENT_PWR				256
#define WLC_GET_SCAN_PASSIVE_TIME		257
#define WLC_SET_SCAN_PASSIVE_TIME		258
#define WLC_LEGACY_LINK_BEHAVIOR		259
#define WLC_GET_CHANNELS_IN_COUNTRY		260
#define WLC_GET_COUNTRY_LIST			261
#endif /* LINUX_POSTMOGRIFY_REMOVAL */
#define WLC_GET_VAR				262	/* get value of named variable */
#define WLC_SET_VAR				263	/* set named variable to value */
#ifndef LINUX_POSTMOGRIFY_REMOVAL
#define WLC_NVRAM_GET				264	/* deprecated */
#define WLC_NVRAM_SET				265
#define WLC_NVRAM_DUMP				266
#define WLC_REBOOT				267
#define WLC_SET_WSEC_PMK			268
#define WLC_GET_AUTH_MODE			269
#define WLC_SET_AUTH_MODE			270
#define WLC_GET_WAKEENTRY			271
#define WLC_SET_WAKEENTRY			272
#define WLC_NDCONFIG_ITEM			273	/* currently handled in wl_oid.c */
#define WLC_NVOTPW				274
#define WLC_OTPW				275
#define WLC_IOV_BLOCK_GET			276
#define WLC_IOV_MODULES_GET			277
#define WLC_SOFT_RESET				278
#define WLC_GET_ALLOW_MODE			279
#define WLC_SET_ALLOW_MODE			280
#define WLC_GET_DESIRED_BSSID			281
#define WLC_SET_DESIRED_BSSID			282
#define	WLC_DISASSOC_MYAP			283
#define WLC_GET_NBANDS				284	/* for Dongle EXT_STA support */
#define WLC_GET_BANDSTATES			285	/* for Dongle EXT_STA support */
#define WLC_GET_WLC_BSS_INFO			286	/* for Dongle EXT_STA support */
#define WLC_GET_ASSOC_INFO			287	/* for Dongle EXT_STA support */
#define WLC_GET_OID_PHY				288	/* for Dongle EXT_STA support */
#define WLC_SET_OID_PHY				289	/* for Dongle EXT_STA support */
#define WLC_SET_ASSOC_TIME			290	/* for Dongle EXT_STA support */
#define WLC_GET_DESIRED_SSID			291	/* for Dongle EXT_STA support */
#define WLC_GET_CHANSPEC			292	/* for Dongle EXT_STA support */
#define WLC_GET_ASSOC_STATE			293	/* for Dongle EXT_STA support */
#define WLC_SET_PHY_STATE			294	/* for Dongle EXT_STA support */
#define WLC_GET_SCAN_PENDING			295	/* for Dongle EXT_STA support */
#define WLC_GET_SCANREQ_PENDING			296	/* for Dongle EXT_STA support */
#define WLC_GET_PREV_ROAM_REASON		297	/* for Dongle EXT_STA support */
#define WLC_SET_PREV_ROAM_REASON		298	/* for Dongle EXT_STA support */
#define WLC_GET_BANDSTATES_PI			299	/* for Dongle EXT_STA support */
#define WLC_GET_PHY_STATE			300	/* for Dongle EXT_STA support */
#define WLC_GET_BSS_WPA_RSN			301	/* for Dongle EXT_STA support */
#define WLC_GET_BSS_WPA2_RSN			302	/* for Dongle EXT_STA support */
#define WLC_GET_BSS_BCN_TS			303	/* for Dongle EXT_STA support */
#define WLC_GET_INT_DISASSOC			304	/* for Dongle EXT_STA support */
#define WLC_SET_NUM_PEERS			305     /* for Dongle EXT_STA support */
#define WLC_GET_NUM_BSS				306	/* for Dongle EXT_STA support */
#define WLC_PHY_SAMPLE_COLLECT			307	/* phy sample collect mode */
/* #define WLC_UM_PRIV				308 */	/* Deprecated: usermode driver */
#define WLC_GET_CMD				309
/* #define WLC_LAST				310 */	/* Never used - can be reused */
#define WLC_SET_INTERFERENCE_OVERRIDE_MODE	311	/* set inter mode override */
#define WLC_GET_INTERFERENCE_OVERRIDE_MODE	312	/* get inter mode override */
/* #define WLC_GET_WAI_RESTRICT			313 */	/* for WAPI, deprecated use iovar instead */
/* #define WLC_SET_WAI_RESTRICT			314 */	/* for WAPI, deprecated use iovar instead */
/* #define WLC_SET_WAI_REKEY			315 */	/* for WAPI, deprecated use iovar instead */
#define WLC_SET_NAT_CONFIG			316	/* for configuring NAT filter driver */
#define WLC_GET_NAT_STATE			317
#define WLC_GET_TXBF_RATESET			318
#define WLC_SET_TXBF_RATESET			319
#define WLC_SCAN_CQ				320
#define WLC_GET_RSSI_QDB			321 /* qdB portion of the RSSI */

#define WLC_LAST				322

#ifndef EPICTRL_COOKIE
#define EPICTRL_COOKIE		0xABADCEDE
#endif

/* vx wlc ioctl's offset */
#define CMN_IOCTL_OFF 0x180

/*
 * custom OID support
 *
 * 0xFF - implementation specific OID
 * 0xE4 - first byte of Broadcom PCI vendor ID
 * 0x14 - second byte of Broadcom PCI vendor ID
 * 0xXX - the custom OID number
 */

/* begin 0x1f values beyond the start of the ET driver range. */
#define WL_OID_BASE		0xFFE41420

/* NDIS overrides */
#define OID_WL_GETINSTANCE	(WL_OID_BASE + WLC_GET_INSTANCE)
#define OID_WL_GET_FORCELINK	(WL_OID_BASE + WLC_GET_FORCELINK)
#define OID_WL_SET_FORCELINK	(WL_OID_BASE + WLC_SET_FORCELINK)
#define	OID_WL_ENCRYPT_STRENGTH	(WL_OID_BASE + WLC_ENCRYPT_STRENGTH)
#define OID_WL_DECRYPT_STATUS	(WL_OID_BASE + WLC_DECRYPT_STATUS)
#define OID_LEGACY_LINK_BEHAVIOR (WL_OID_BASE + WLC_LEGACY_LINK_BEHAVIOR)
#define OID_WL_NDCONFIG_ITEM	(WL_OID_BASE + WLC_NDCONFIG_ITEM)

/* EXT_STA Dongle suuport */
#define OID_STA_CHANSPEC	(WL_OID_BASE + WLC_GET_CHANSPEC)
#define OID_STA_NBANDS		(WL_OID_BASE + WLC_GET_NBANDS)
#define OID_STA_GET_PHY		(WL_OID_BASE + WLC_GET_OID_PHY)
#define OID_STA_SET_PHY		(WL_OID_BASE + WLC_SET_OID_PHY)
#define OID_STA_ASSOC_TIME	(WL_OID_BASE + WLC_SET_ASSOC_TIME)
#define OID_STA_DESIRED_SSID	(WL_OID_BASE + WLC_GET_DESIRED_SSID)
#define OID_STA_SET_PHY_STATE	(WL_OID_BASE + WLC_SET_PHY_STATE)
#define OID_STA_SCAN_PENDING	(WL_OID_BASE + WLC_GET_SCAN_PENDING)
#define OID_STA_SCANREQ_PENDING (WL_OID_BASE + WLC_GET_SCANREQ_PENDING)
#define OID_STA_GET_ROAM_REASON (WL_OID_BASE + WLC_GET_PREV_ROAM_REASON)
#define OID_STA_SET_ROAM_REASON (WL_OID_BASE + WLC_SET_PREV_ROAM_REASON)
#define OID_STA_GET_PHY_STATE	(WL_OID_BASE + WLC_GET_PHY_STATE)
#define OID_STA_INT_DISASSOC	(WL_OID_BASE + WLC_GET_INT_DISASSOC)
#define OID_STA_SET_NUM_PEERS	(WL_OID_BASE + WLC_SET_NUM_PEERS)
#define OID_STA_GET_NUM_BSS	(WL_OID_BASE + WLC_GET_NUM_BSS)

/* NAT filter driver support */
#define OID_NAT_SET_CONFIG	(WL_OID_BASE + WLC_SET_NAT_CONFIG)
#define OID_NAT_GET_STATE	(WL_OID_BASE + WLC_GET_NAT_STATE)

#define WL_DECRYPT_STATUS_SUCCESS	1
#define WL_DECRYPT_STATUS_FAILURE	2
#define WL_DECRYPT_STATUS_UNKNOWN	3

/* allows user-mode app to poll the status of USB image upgrade */
#define WLC_UPGRADE_SUCCESS			0
#define WLC_UPGRADE_PENDING			1

#ifdef CONFIG_USBRNDIS_RETAIL
/* struct passed in for WLC_NDCONFIG_ITEM */
typedef struct {
	char *name;
	void *param;
} ndconfig_item_t;
#endif


/* WLC_GET_AUTH, WLC_SET_AUTH values */
#define WL_AUTH_OPEN_SYSTEM		0	/* d11 open authentication */
#define WL_AUTH_SHARED_KEY		1	/* d11 shared authentication */
#define WL_AUTH_OPEN_SHARED     	2   /* try open, then shared if open failed w/rc 13 */
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

/* Bit masks for radio disabled status - returned by WL_GET_RADIO */
#define WL_RADIO_SW_DISABLE		(1<<0)
#define WL_RADIO_HW_DISABLE		(1<<1)
#define WL_RADIO_MPC_DISABLE		(1<<2)
#define WL_RADIO_COUNTRY_DISABLE	(1<<3)	/* some countries don't support any channel */

#define	WL_SPURAVOID_OFF	0
#define	WL_SPURAVOID_ON1	1
#define	WL_SPURAVOID_ON2	2


/* Override bit for WLC_SET_TXPWR.  if set, ignore other level limits */
#define WL_TXPWR_OVERRIDE	(1U<<31)
#define WL_TXPWR_NEG   (1U<<30)

#ifndef LINUX_POSTMOGRIFY_REMOVAL
#define WL_PHY_PAVARS_LEN	32	/* Phy type, Band range, chain, a1[0], b0[0], b1[0] ... */

#define WL_PHY_PAVARS2_NUM	3	/* a1, b0, b1 */
#define WL_PHY_PAVAR_VER	1	/* pavars version */
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

/* a large TX Power as an init value to factor out of MIN() calculations,
 * keep low enough to fit in an int8, units are .25 dBm
 */
#define WLC_TXPWR_MAX		(127)	/* ~32 dBm = 1,500 mW */

/* "diag" iovar argument and error code */
#define WL_DIAG_INTERRUPT			1	/* d11 loopback interrupt test */
#define WL_DIAG_LOOPBACK			2	/* d11 loopback data test */
#define WL_DIAG_MEMORY				3	/* d11 memory test */
#define WL_DIAG_LED				4	/* LED test */
#define WL_DIAG_REG				5	/* d11/phy register test */
#define WL_DIAG_SROM				6	/* srom read/crc test */
#define WL_DIAG_DMA				7	/* DMA test */
#define WL_DIAG_LOOPBACK_EXT			8	/* enhenced d11 loopback data test */

#define WL_DIAGERR_SUCCESS			0
#define WL_DIAGERR_FAIL_TO_RUN			1	/* unable to run requested diag */
#define WL_DIAGERR_NOT_SUPPORTED		2	/* diag requested is not supported */
#define WL_DIAGERR_INTERRUPT_FAIL		3	/* loopback interrupt test failed */
#define WL_DIAGERR_LOOPBACK_FAIL		4	/* loopback data test failed */
#define WL_DIAGERR_SROM_FAIL			5	/* srom read failed */
#define WL_DIAGERR_SROM_BADCRC			6	/* srom crc failed */
#define WL_DIAGERR_REG_FAIL			7	/* d11/phy register test failed */
#define WL_DIAGERR_MEMORY_FAIL			8	/* d11 memory test failed */
#define WL_DIAGERR_NOMEM			9	/* diag test failed due to no memory */
#define WL_DIAGERR_DMA_FAIL			10	/* DMA test failed */

#define WL_DIAGERR_MEMORY_TIMEOUT		11	/* d11 memory test didn't finish in time */
#define WL_DIAGERR_MEMORY_BADPATTERN		12	/* d11 memory test result in bad pattern */

/* band types */
#define	WLC_BAND_AUTO		0	/* auto-select */
#define	WLC_BAND_5G		1	/* 5 Ghz */
#define	WLC_BAND_2G		2	/* 2.4 Ghz */
#define	WLC_BAND_ALL		3	/* all bands */

/* band range returned by band_range iovar */
#define WL_CHAN_FREQ_RANGE_2G      0
#define WL_CHAN_FREQ_RANGE_5GL     1
#define WL_CHAN_FREQ_RANGE_5GM     2
#define WL_CHAN_FREQ_RANGE_5GH     3

#define WL_CHAN_FREQ_RANGE_5GLL_5BAND    4
#define WL_CHAN_FREQ_RANGE_5GLH_5BAND    5
#define WL_CHAN_FREQ_RANGE_5GML_5BAND    6
#define WL_CHAN_FREQ_RANGE_5GMH_5BAND    7
#define WL_CHAN_FREQ_RANGE_5GH_5BAND     8

#define WL_CHAN_FREQ_RANGE_5G_BAND0     1
#define WL_CHAN_FREQ_RANGE_5G_BAND1     2
#define WL_CHAN_FREQ_RANGE_5G_BAND2     3
#define WL_CHAN_FREQ_RANGE_5G_BAND3     4

#define WL_CHAN_FREQ_RANGE_5G_4BAND    	5
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

/* phy types (returned by WLC_GET_PHYTPE) */
#define	WLC_PHY_TYPE_A		0
#define	WLC_PHY_TYPE_B		1
#define	WLC_PHY_TYPE_G		2
#define	WLC_PHY_TYPE_N		4
#define	WLC_PHY_TYPE_LP		5
#define	WLC_PHY_TYPE_SSN	6
#define	WLC_PHY_TYPE_HT		7
#define	WLC_PHY_TYPE_LCN	8
#define	WLC_PHY_TYPE_LCN40	10
#define WLC_PHY_TYPE_AC		11
#define	WLC_PHY_TYPE_NULL	0xf

/* Values for PM */
#define PM_OFF	0
#define PM_MAX	1
#define PM_FAST 2
#define PM_FORCE_OFF 3 		/* use this bit to force PM off even bt is active */

#ifndef LINUX_POSTMOGRIFY_REMOVAL
/* MAC list modes */
#define WLC_MACMODE_DISABLED	0	/* MAC list disabled */
#define WLC_MACMODE_DENY	1	/* Deny specified (i.e. allow unspecified) */
#define WLC_MACMODE_ALLOW	2	/* Allow specified (i.e. deny unspecified) */

/*
 * 54g modes (basic bits may still be overridden)
 *
 * GMODE_LEGACY_B			Rateset: 1b, 2b, 5.5, 11
 *					Preamble: Long
 *					Shortslot: Off
 * GMODE_AUTO				Rateset: 1b, 2b, 5.5b, 11b, 18, 24, 36, 54
 *					Extended Rateset: 6, 9, 12, 48
 *					Preamble: Long
 *					Shortslot: Auto
 * GMODE_ONLY				Rateset: 1b, 2b, 5.5b, 11b, 18, 24b, 36, 54
 *					Extended Rateset: 6b, 9, 12b, 48
 *					Preamble: Short required
 *					Shortslot: Auto
 * GMODE_B_DEFERRED			Rateset: 1b, 2b, 5.5b, 11b, 18, 24, 36, 54
 *					Extended Rateset: 6, 9, 12, 48
 *					Preamble: Long
 *					Shortslot: On
 * GMODE_PERFORMANCE			Rateset: 1b, 2b, 5.5b, 6b, 9, 11b, 12b, 18, 24b, 36, 48, 54
 *					Preamble: Short required
 *					Shortslot: On and required
 * GMODE_LRS				Rateset: 1b, 2b, 5.5b, 11b
 *					Extended Rateset: 6, 9, 12, 18, 24, 36, 48, 54
 *					Preamble: Long
 *					Shortslot: Auto
 */
#define GMODE_LEGACY_B		0
#define GMODE_AUTO		1
#define GMODE_ONLY		2
#define GMODE_B_DEFERRED	3
#define GMODE_PERFORMANCE	4
#define GMODE_LRS		5
#define GMODE_MAX		6

/* values for PLCPHdr_override */
#define WLC_PLCP_AUTO	-1
#define WLC_PLCP_SHORT	0
#define WLC_PLCP_LONG	1

/* values for g_protection_override and n_protection_override */
#define WLC_PROTECTION_AUTO		-1
#define WLC_PROTECTION_OFF		0
#define WLC_PROTECTION_ON		1
#define WLC_PROTECTION_MMHDR_ONLY	2
#define WLC_PROTECTION_CTS_ONLY		3

/* values for g_protection_control and n_protection_control */
#define WLC_PROTECTION_CTL_OFF		0
#define WLC_PROTECTION_CTL_LOCAL	1
#define WLC_PROTECTION_CTL_OVERLAP	2

/* values for n_protection */
#define WLC_N_PROTECTION_OFF		0
#define WLC_N_PROTECTION_OPTIONAL	1
#define WLC_N_PROTECTION_20IN40		2
#define WLC_N_PROTECTION_MIXEDMODE	3

/* values for n_preamble_type */
#define WLC_N_PREAMBLE_MIXEDMODE	0
#define WLC_N_PREAMBLE_GF		1
#define WLC_N_PREAMBLE_GF_BRCM          2

/* values for band specific 40MHz capabilities (deprecated) */
#define WLC_N_BW_20ALL			0
#define WLC_N_BW_40ALL			1
#define WLC_N_BW_20IN2G_40IN5G		2

#define WLC_BW_20MHZ_BIT		(1<<0)
#define WLC_BW_40MHZ_BIT		(1<<1)
#define WLC_BW_80MHZ_BIT		(1<<2)

/* Bandwidth capabilities */
#define WLC_BW_CAP_20MHZ		(WLC_BW_20MHZ_BIT)
#define WLC_BW_CAP_40MHZ		(WLC_BW_40MHZ_BIT|WLC_BW_20MHZ_BIT)
#define WLC_BW_CAP_80MHZ		(WLC_BW_80MHZ_BIT|WLC_BW_40MHZ_BIT|WLC_BW_20MHZ_BIT)
#define WLC_BW_CAP_UNRESTRICTED		0xFF

#define WL_BW_CAP_20MHZ(bw_cap)	(((bw_cap) & WLC_BW_20MHZ_BIT) ? TRUE : FALSE)
#define WL_BW_CAP_40MHZ(bw_cap)	(((bw_cap) & WLC_BW_40MHZ_BIT) ? TRUE : FALSE)
#define WL_BW_CAP_80MHZ(bw_cap)	(((bw_cap) & WLC_BW_80MHZ_BIT) ? TRUE : FALSE)

/* values to force tx/rx chain */
#define WLC_N_TXRX_CHAIN0		0
#define WLC_N_TXRX_CHAIN1		1

/* bitflags for SGI support (sgi_rx iovar) */
#define WLC_N_SGI_20			0x01
#define WLC_N_SGI_40			0x02
#define WLC_VHT_SGI_80			0x04

/* when sgi_tx==WLC_SGI_ALL, bypass rate selection, enable sgi for all mcs */
#define WLC_SGI_ALL				0x02

#define LISTEN_INTERVAL			10
/* interference mitigation options */
#define	INTERFERE_OVRRIDE_OFF	-1	/* interference override off */
#define	INTERFERE_NONE	0	/* off */
#define	NON_WLAN	1	/* foreign/non 802.11 interference, no auto detect */
#define	WLAN_MANUAL	2	/* ACI: no auto detection */
#define	WLAN_AUTO	3	/* ACI: auto detect */
#define	WLAN_AUTO_W_NOISE	4	/* ACI: auto - detect and non 802.11 interference */
#define AUTO_ACTIVE	(1 << 7) /* Auto is currently active */

/* interfernece mode bit-masks (ACPHY) */
#define ACPHY_ACI_GLITCHBASED_DESENSE 1   /* bit 0 */
#define ACPHY_ACI_HWACI_PKTGAINLMT 2      /* bit 1 */
#define ACPHY_ACI_W2NB_PKTGAINLMT 4       /* bit 2 */
#define ACPHY_ACI_MAX_MODE 7

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

#define TRIGGER_NOW				0
#define TRIGGER_CRS				0x01
#define TRIGGER_CRSDEASSERT			0x02
#define TRIGGER_GOODFCS				0x04
#define TRIGGER_BADFCS				0x08
#define TRIGGER_BADPLCP				0x10
#define TRIGGER_CRSGLITCH			0x20
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
	bool downsamp;
	bool be_deaf;
	bool agc;		/* loop from init gain and going down */
	bool filter;		/* override high pass corners to lowest */
	/* add'l version 2 fields */
	uint8 trigger_state;
	uint8 module_sel1;
	uint8 module_sel2;
	uint16 nsamps;
	int bitStart;
	uint32 gpioCapMask;
} wl_samplecollect_args_t;

#define	WL_SAMPLEDATA_HEADER_TYPE	1
#define WL_SAMPLEDATA_HEADER_SIZE	80	/* sample collect header size (bytes) */
#define	WL_SAMPLEDATA_TYPE		2
#define	WL_SAMPLEDATA_SEQ		0xff	/* sequence # */
#define	WL_SAMPLEDATA_MORE_DATA		0x100	/* more data mask */
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

/* wl_radar_args_t */
typedef struct {
	int npulses; 	/* required number of pulses at n * t_int */
	int ncontig; 	/* required number of pulses at t_int */
	int min_pw; 	/* minimum pulse width (20 MHz clocks) */
	int max_pw; 	/* maximum pulse width (20 MHz clocks) */
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
} wl_radar_thr_t;

#define WL_RADAR_THR_VERSION	2
#define WL_THRESHOLD_LO_BAND	70	/* range from 5250MHz - 5350MHz */

/* radar iovar SET defines */
#define WL_RADAR_DETECTOR_OFF		0	/* radar detector off */
#define WL_RADAR_DETECTOR_ON		1	/* radar detector on */
#define WL_RADAR_SIMULATED		2	/* force radar detector to declare
						 * detection once
						 */
#define WL_RSSI_ANT_VERSION	1	/* current version of wl_rssi_ant_t */
#define WL_ANT_RX_MAX		2	/* max 2 receive antennas */
#define WL_ANT_HT_RX_MAX	3	/* max 3 receive antennas/cores */
#define WL_ANT_IDX_1		0	/* antenna index 1 */
#define WL_ANT_IDX_2		1	/* antenna index 2 */

#ifndef WL_RSSI_ANT_MAX
#define WL_RSSI_ANT_MAX		4	/* max possible rx antennas */
#elif WL_RSSI_ANT_MAX != 4
#error "WL_RSSI_ANT_MAX does not match"
#endif

/* RSSI per antenna */
typedef struct {
	uint32	version;		/* version field */
	uint32	count;			/* number of valid antenna rssi */
	int8 rssi_ant[WL_RSSI_ANT_MAX];	/* rssi per antenna */
} wl_rssi_ant_t;

/* dfs_status iovar-related defines */

/* cac - channel availability check,
 * ism - in-service monitoring
 * csa - channel switching announcement
 */

/* cac state values */
#define WL_DFS_CACSTATE_IDLE		0	/* state for operating in non-radar channel */
#define	WL_DFS_CACSTATE_PREISM_CAC	1	/* CAC in progress */
#define WL_DFS_CACSTATE_ISM		2	/* ISM in progress */
#define WL_DFS_CACSTATE_CSA		3	/* csa */
#define WL_DFS_CACSTATE_POSTISM_CAC	4	/* ISM CAC */
#define WL_DFS_CACSTATE_PREISM_OOC	5	/* PREISM OOC */
#define WL_DFS_CACSTATE_POSTISM_OOC	6	/* POSTISM OOC */
#define WL_DFS_CACSTATES		7	/* this many states exist */

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
#define WL_NUM_RATES_CCK			4 /* 1, 2, 5.5, 11 Mbps */
#define WL_NUM_RATES_OFDM			8 /* 6, 9, 12, 18, 24, 36, 48, 54 Mbps SISO/CDD */
#define WL_NUM_RATES_MCS_1STREAM	8 /* MCS 0-7 1-stream rates - SISO/CDD/STBC/MCS */
#define WL_NUM_RATES_EXTRA_VHT		2 /* Additional VHT 11AC rates */
#define WL_NUM_RATES_VHT			10
#define WL_NUM_RATES_MCS32			1

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
#define TX_POWER_T_VERSION	44

/* Defines used with channel_bandwidth for curpower */
#define WL_BW_20MHZ 		0
#define WL_BW_40MHZ 		1
#define WL_BW_80MHZ 		2
#define WL_BW_160MHZ 		3

/* tx_power_t.flags bits */
#define WL_TX_POWER_F_ENABLED	1
#define WL_TX_POWER_F_HW		2
#define WL_TX_POWER_F_MIMO		4
#define WL_TX_POWER_F_SISO		8
#define WL_TX_POWER_F_HT		0x10
#define WL_TX_POWER_F_VHT		0x20
#define WL_TX_POWER_F_OPENLOOP		0x40

typedef struct tx_inst_power {
	uint8 txpwr_est_Pout[2];			/* Latest estimate for 2.4 and 5 Ghz */
	uint8 txpwr_est_Pout_gofdm;			/* Pwr estimate for 2.4 OFDM */
} tx_inst_power_t;

#define WL_NUM_TXCHAIN_MAX	4
typedef struct wl_txchain_pwr_offsets {
	int8 offset[WL_NUM_TXCHAIN_MAX];	/* quarter dBm signed offset for each chain */
} wl_txchain_pwr_offsets_t;

/* 802.11h measurement types */
#define WLC_MEASURE_TPC			1
#define WLC_MEASURE_CHANNEL_BASIC	2
#define WLC_MEASURE_CHANNEL_CCA		3
#define WLC_MEASURE_CHANNEL_RPI		4

/* regulatory enforcement levels */
#define SPECT_MNGMT_OFF			0		/* both 11h and 11d disabled */
#define SPECT_MNGMT_LOOSE_11H		1		/* allow non-11h APs in scan lists */
#define SPECT_MNGMT_STRICT_11H		2		/* prune out non-11h APs from scan list */
#define SPECT_MNGMT_STRICT_11D		3		/* switch to 802.11D mode */
/* SPECT_MNGMT_LOOSE_11H_D - same as SPECT_MNGMT_LOOSE with the exception that Country IE
 * adoption is done regardless of capability spectrum_management
 */
#define SPECT_MNGMT_LOOSE_11H_D		4		/* operation defined above */

#define WL_CHAN_VALID_HW	(1 << 0)	/* valid with current HW */
#define WL_CHAN_VALID_SW	(1 << 1)	/* valid with current country setting */
#define WL_CHAN_BAND_5G		(1 << 2)	/* 5GHz-band channel */
#define WL_CHAN_RADAR		(1 << 3)	/* radar sensitive  channel */
#define WL_CHAN_INACTIVE	(1 << 4)	/* temporarily inactive due to radar */
#define WL_CHAN_PASSIVE		(1 << 5)	/* channel is in passive mode */
#define WL_CHAN_RESTRICTED	(1 << 6)	/* restricted use channel */

/* BTC mode used by "btc_mode" iovar */
#define	WL_BTC_DISABLE		0	/* disable BT coexistence */
#define WL_BTC_FULLTDM      1	/* full TDM COEX */
#define WL_BTC_ENABLE       1	/* full TDM COEX to maintain backward compatiblity */
#define WL_BTC_PREMPT      2    /* full TDM COEX with preemption */
#define WL_BTC_LITE        3	/* light weight coex for large isolation platform */
#define WL_BTC_PARALLEL		4   /* BT and WLAN run in parallel with separate antenna  */
#define WL_BTC_HYBRID		5   /* hybrid coex, only ack is allowed to transmit in BT slot */
#define WL_BTC_DEFAULT		8	/* set the default mode for the device */
#define WL_INF_BTC_DISABLE      0
#define WL_INF_BTC_ENABLE       1
#define WL_INF_BTC_AUTO         3

/* BTC wire used by "btc_wire" iovar */
#define	WL_BTC_DEFWIRE		0	/* use default wire setting */
#define WL_BTC_2WIRE		2	/* use 2-wire BTC */
#define WL_BTC_3WIRE		3	/* use 3-wire BTC */
#define WL_BTC_4WIRE		4	/* use 4-wire BTC */

/* BTC flags: BTC configuration that can be set by host */
#define WL_BTC_FLAG_PREMPT               (1 << 0)
#define WL_BTC_FLAG_BT_DEF               (1 << 1)
#define WL_BTC_FLAG_ACTIVE_PROT          (1 << 2)
#define WL_BTC_FLAG_SIM_RSP              (1 << 3)
#define WL_BTC_FLAG_PS_PROTECT           (1 << 4)
#define WL_BTC_FLAG_SIM_TX_LP	         (1 << 5)
#define WL_BTC_FLAG_ECI                  (1 << 6)
#define WL_BTC_FLAG_LIGHT                (1 << 7)
#define WL_BTC_FLAG_PARALLEL             (1 << 8)

/* Message levels */
#define WL_ERROR_VAL		0x00000001
#define WL_TRACE_VAL		0x00000002
#define WL_PRHDRS_VAL		0x00000004
#define WL_PRPKT_VAL		0x00000008
#define WL_INFORM_VAL		0x00000010
#define WL_TMP_VAL		0x00000020
#define WL_OID_VAL		0x00000040
#define WL_RATE_VAL		0x00000080
#define WL_ASSOC_VAL		0x00000100
#define WL_PRUSR_VAL		0x00000200
#define WL_PS_VAL		0x00000400
#define WL_TXPWR_VAL		0x00000800	/* retired in TOT on 6/10/2009 */
#define WL_PORT_VAL		0x00001000
#define WL_DUAL_VAL		0x00002000
#define WL_WSEC_VAL		0x00004000
#define WL_WSEC_DUMP_VAL	0x00008000
#define WL_LOG_VAL		0x00010000
#define WL_NRSSI_VAL		0x00020000	/* retired in TOT on 6/10/2009 */
#define WL_LOFT_VAL		0x00040000	/* retired in TOT on 6/10/2009 */
#define WL_REGULATORY_VAL	0x00080000
#define WL_TAF_VAL		0x00100000
#define WL_RADAR_VAL		0x00200000	/* retired in TOT on 6/10/2009 */
#define WL_MPC_VAL		0x00400000
#define WL_APSTA_VAL		0x00800000
#define WL_DFS_VAL		0x01000000
#define WL_BA_VAL		0x02000000	/* retired in TOT on 6/14/2010 */
#define WL_ACI_VAL		0x04000000
#define WL_MBSS_VAL		0x04000000
#define WL_CAC_VAL		0x08000000
#define WL_AMSDU_VAL		0x10000000
#define WL_AMPDU_VAL		0x20000000
#define WL_FFPLD_VAL		0x40000000

/* wl_msg_level is full. For new bits take the next one and AND with
 * wl_msg_level2 in wl_dbg.h
 */
#define WL_DPT_VAL 		0x00000001
#define WL_SCAN_VAL		0x00000002
#define WL_WOWL_VAL		0x00000004
#define WL_COEX_VAL		0x00000008
#define WL_RTDC_VAL		0x00000010
#define WL_PROTO_VAL		0x00000020
#define WL_BTA_VAL		0x00000040
#define WL_CHANINT_VAL		0x00000080
#define WL_WMF_VAL		0x00000100
#define WL_P2P_VAL		0x00000200
#define WL_ITFR_VAL		0x00000400
#define WL_MCHAN_VAL		0x00000800
#define WL_TDLS_VAL		0x00001000
#define WL_MCNX_VAL		0x00002000
#define WL_PROT_VAL		0x00004000
#define WL_PSTA_VAL		0x00008000
#define WL_TSO_VAL		0x00010000
#define WL_TRF_MGMT_VAL		0x00020000
#define WL_LPC_VAL	        0x00040000
#define WL_L2FILTER_VAL		0x00080000
#define WL_TXBF_VAL		0x00100000
#define WL_P2PO_VAL		0x00200000
/* Space reserved for conflicting message levels from trunk */
#define WL_WNM_VAL		0x04000000
#define WL_AWDL_VAL		0x08000000
#define WL_PWRSEL_VAL		0x10000000
#define WL_NET_DETECT_VAL	0x20000000
#define WL_PCIE_VAL		0x40000000

/* use top-bit for WL_TIME_STAMP_VAL because this is a modifier
 * rather than a message-type of its own
 */
#define WL_TIMESTAMP_VAL        0x80000000

/* max # of leds supported by GPIO (gpio pin# == led index#) */
#define	WL_LED_NUMGPIO		32	/* gpio 0-31 */

/* led per-pin behaviors */
#define	WL_LED_OFF		0		/* always off */
#define	WL_LED_ON		1		/* always on */
#define	WL_LED_ACTIVITY		2		/* activity */
#define	WL_LED_RADIO		3		/* radio enabled */
#define	WL_LED_ARADIO		4		/* 5  Ghz radio enabled */
#define	WL_LED_BRADIO		5		/* 2.4Ghz radio enabled */
#define	WL_LED_BGMODE		6		/* on if gmode, off if bmode */
#define	WL_LED_WI1		7
#define	WL_LED_WI2		8
#define	WL_LED_WI3		9
#define	WL_LED_ASSOC		10		/* associated state indicator */
#define	WL_LED_INACTIVE		11		/* null behavior (clears default behavior) */
#define	WL_LED_ASSOCACT		12		/* on when associated; blink fast for activity */
#define WL_LED_WI4		13
#define WL_LED_WI5		14
#define	WL_LED_BLINKSLOW	15		/* blink slow */
#define	WL_LED_BLINKMED		16		/* blink med */
#define	WL_LED_BLINKFAST	17		/* blink fast */
#define	WL_LED_BLINKCUSTOM	18		/* blink custom */
#define	WL_LED_BLINKPERIODIC	19		/* blink periodic (custom 1000ms / off 400ms) */
#define WL_LED_ASSOC_WITH_SEC 	20		/* when connected with security */
						/* keep on for 300 sec */
#define WL_LED_START_OFF 	21		/* off upon boot, could be turned on later */
#define WL_LED_WI6		22
#define WL_LED_WI7		23
#define WL_LED_WI8		24
#define	WL_LED_NUMBEHAVIOR	25

/* led behavior numeric value format */
#define	WL_LED_BEH_MASK		0x7f		/* behavior mask */
#define	WL_LED_AL_MASK		0x80		/* activelow (polarity) bit */

/* maximum channels returned by the get valid channels iovar */
#define WL_NUMCHANNELS		64

/* max number of chanspecs (used by the iovar to calc. buf space) */
#define WL_NUMCHANSPECS 110

/* WDS link local endpoint WPA role */
#define WL_WDS_WPA_ROLE_AUTH	0	/* authenticator */
#define WL_WDS_WPA_ROLE_SUP	1	/* supplicant */
#define WL_WDS_WPA_ROLE_AUTO	255	/* auto, based on mac addr value */

/* number of bytes needed to define a 128-bit mask for MAC event reporting */
#define WL_EVENTING_MASK_LEN	((WLC_E_LAST + 7) / 8)

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
 * offset 1: +ve RSSI boost balue in dB
 */

/* join preference types */
#define WL_JOIN_PREF_RSSI	1	/* by RSSI */
#define WL_JOIN_PREF_WPA	2	/* by akm and ciphers */
#define WL_JOIN_PREF_BAND	3	/* by 802.11 band */
#define WL_JOIN_PREF_RSSI_DELTA	4	/* by 802.11 band only if RSSI delta condition matches */
#define WL_JOIN_PREF_TRANS_PREF	5	/* defined by requesting AP */

/* band preference */
#define WLJP_BAND_ASSOC_PREF	255	/* use what WLC_SET_ASSOC_PREFER ioctl specifies */

/* any multicast cipher suite */
#define WL_WPA_ACP_MCS_ANY	"\x00\x00\x00\x00"

struct tsinfo_arg {
	uint8 octets[3];
};
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

#define	NFIFO			6	/* # tx/rx fifopairs */

#define	WL_CNT_T_VERSION	9	/* current version of wl_cnt_t struct */

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
	uint32  rx108mbps; 	/* packets rx at 108mbps */
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
	uint32 	cso_normal;	/* hw cso hdr for normal process */
	uint32	chained;	/* number of frames chained */
	uint32	chainedsz1;	/* number of chain size 1 frames */
	uint32	unchained;	/* number of frames not chained */
	uint32	maxchainsz;	/* max chain size so far */
	uint32	currchainsz;	/* current chain size */
	uint32	rxdrop20s;	/* drop secondary cnt */
	uint32	pciereset;	/* Secondary Bus Reset issued by driver */
	uint32	cfgrestore;	/* configspace restore by driver */

	uint32	rxdma_frame;	/* count for rx dma */
	uint32	rxdma_inactivity;	/* cleared when rxdma handler is serviced or increased in watchdog */
	uint32	rxdma_stuck;	/* count for rx stuck */
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
	uint32  rx108mbps; 	/* packets rx at 108mbps */
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

#define WL_WME_CNT_VERSION	1	/* current version of wl_wme_cnt_t */

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

#ifndef ETHER_MAX_DATA
#define ETHER_MAX_DATA	1500
#endif /* ETHER_MAX_DATA */

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

/* structure for identifying ea/tid for sending addba/delba */
struct ampdu_ea_tid {
	struct ether_addr ea;		/* Station address */
	uint8 tid;			/* tid */
};
/* structure for identifying retry/tid for retry_limit_tid/rr_retry_limit_tid */
struct ampdu_retry_tid {
	uint8 tid;	/* tid */
	uint8 retry;	/* retry value */
};

/* Different discovery modes for dpt */
#define	DPT_DISCOVERY_MANUAL	0x01	/* manual discovery mode */
#define	DPT_DISCOVERY_AUTO	0x02	/* auto discovery mode */
#define	DPT_DISCOVERY_SCAN	0x04	/* scan-based discovery mode */

/* different path selection values */
#define DPT_PATHSEL_AUTO	0	/* auto mode for path selection */
#define DPT_PATHSEL_DIRECT	1	/* always use direct DPT path */
#define DPT_PATHSEL_APPATH	2	/* always use AP path */

/* different ops for deny list */
#define DPT_DENY_LIST_ADD 	1	/* add to dpt deny list */
#define DPT_DENY_LIST_REMOVE 	2	/* remove from dpt deny list */

/* different ops for manual end point */
#define DPT_MANUAL_EP_CREATE	1	/* create manual dpt endpoint */
#define DPT_MANUAL_EP_MODIFY	2	/* modify manual dpt endpoint */
#define DPT_MANUAL_EP_DELETE	3	/* delete manual dpt endpoint */

/* structure for dpt iovars */
typedef struct dpt_iovar {
	struct ether_addr ea;		/* Station address */
	uint8 mode;			/* mode: depends on iovar */
	uint32 pad;			/* future */
} dpt_iovar_t;

/* flags to indicate DPT status */
#define	DPT_STATUS_ACTIVE	0x01	/* link active (though may be suspended) */
#define	DPT_STATUS_AES		0x02	/* link secured through AES encryption */
#define	DPT_STATUS_FAILED	0x04	/* DPT link failed */

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
/* different ops for manual end point */
#define TDLS_MANUAL_EP_CREATE	1	/* create manual dpt endpoint */
#define TDLS_MANUAL_EP_MODIFY	2	/* modify manual dpt endpoint */
#define TDLS_MANUAL_EP_DELETE	3	/* delete manual dpt endpoint */
#define TDLS_MANUAL_EP_PM		4	/*  put dpt endpoint in PM mode */
#define TDLS_MANUAL_EP_WAKE		5	/* wake up dpt endpoint from PM */
#define TDLS_MANUAL_EP_DISCOVERY	6	/* discover if endpoint is TDLS capable */
#define TDLS_MANUAL_EP_CHSW		7	/* channel switch */
#define TDLS_MANUAL_EP_WFD_TPQ	8	/* WiFi-Display Tunneled Probe reQuest */

/* structure for tdls iovars */
typedef struct tdls_iovar {
	struct ether_addr ea;		/* Station address */
	uint8 mode;			/* mode: depends on iovar */
	chanspec_t chanspec;
	uint32 pad;			/* future */
} tdls_iovar_t;

/* modes */
#define TDLS_WFD_IE_TX 			0
#define TDLS_WFD_IE_RX 			1
#define TDLS_WFD_PROBE_IE_TX	2
#define TDLS_WFD_PROBE_IE_RX	3
#define TDLS_WFD_IE_SIZE 		512
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

/* define for flag */
#define TSPEC_PENDING		0	/* TSPEC pending */
#define TSPEC_ACCEPTED		1	/* TSPEC accepted */
#define TSPEC_REJECTED		2	/* TSPEC rejected */
#define TSPEC_UNKNOWN		3	/* TSPEC unknown */
#define TSPEC_STATUS_MASK	7	/* TSPEC status mask */


/* Software feature flag defines used by wlfeatureflag */
#ifdef WLAFTERBURNER
#define WL_SWFL_ABBFL       0x0001 /* Allow Afterburner on systems w/o hardware BFL */
#define WL_SWFL_ABENCORE    0x0002 /* Allow AB on non-4318E chips */
#endif /* WLAFTERBURNER */
#define WL_SWFL_NOHWRADIO	0x0004
#define WL_SWFL_FLOWCONTROL     0x0008 /* Enable backpressure to OS stack */
#define WL_SWFL_WLBSSSORT	0x0010 /* Per-port supports sorting of BSS */

#define WL_LIFETIME_MAX 0xFFFF /* Max value in ms */

/* Packet lifetime configuration per ac */
typedef struct wl_lifetime {
	uint32 ac;	        /* access class */
	uint32 lifetime;    /* Packet lifetime value in ms */
} wl_lifetime_t;

#define CSA_BROADCAST_ACTION_FRAME	0	/* csa broadcast action frame */
#define CSA_UNICAST_ACTION_FRAME	  1 /* csa unicast action frame */

/* Channel Switch Announcement param */
typedef struct wl_chan_switch {
	uint8 mode;		/* value 0 or 1 */
	uint8 count;		/* count # of beacons before switching */
	chanspec_t chspec;	/* chanspec */
	uint8 reg;		/* regulatory class */
	uint8 frame_type;		/* csa frame type, unicast or broadcast */
} wl_chan_switch_t;

/* Roaming trigger definitions for WLC_SET_ROAM_TRIGGER.
 *
 * (-100 < value < 0)   value is used directly as a roaming trigger in dBm
 * (0 <= value) value specifies a logical roaming trigger level from
 *                      the list below
 *
 * WLC_GET_ROAM_TRIGGER always returns roaming trigger value in dBm, never
 * the logical roam trigger value.
 */
#define WLC_ROAM_TRIGGER_DEFAULT	0 /* default roaming trigger */
#define WLC_ROAM_TRIGGER_BANDWIDTH	1 /* optimize for bandwidth roaming trigger */
#define WLC_ROAM_TRIGGER_DISTANCE	2 /* optimize for distance roaming trigger */
#define WLC_ROAM_TRIGGER_AUTO		3 /* auto-detect environment */
#define WLC_ROAM_TRIGGER_MAX_VALUE	3 /* max. valid value */

#define WLC_ROAM_NEVER_ROAM_TRIGGER	(-100) /* Avoid Roaming by setting a large value */

/* Preferred Network Offload (PNO, formerly PFN) defines */
#define WPA_AUTH_PFN_ANY	0xffffffff	/* for PFN, match only ssid */

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

#define SORT_CRITERIA_BIT		0
#define AUTO_NET_SWITCH_BIT		1
#define ENABLE_BKGRD_SCAN_BIT	2
#define IMMEDIATE_SCAN_BIT		3
#define	AUTO_CONNECT_BIT		4
#define	ENABLE_BD_SCAN_BIT		5
#define ENABLE_ADAPTSCAN_BIT	6
#define IMMEDIATE_EVENT_BIT		8
#define SUPPRESS_SSID_BIT		9
#define ENABLE_NET_OFFLOAD_BIT		10

#define SORT_CRITERIA_MASK		0x0001
#define AUTO_NET_SWITCH_MASK	0x0002
#define ENABLE_BKGRD_SCAN_MASK	0x0004
#define IMMEDIATE_SCAN_MASK		0x0008
#define	AUTO_CONNECT_MASK		0x0010

#define ENABLE_BD_SCAN_MASK		0x0020
#define ENABLE_ADAPTSCAN_MASK	0x00c0
#define IMMEDIATE_EVENT_MASK	0x0100
#define SUPPRESS_SSID_MASK	0x0200
#define ENABLE_NET_OFFLOAD_MASK	0x0400

#define PFN_VERSION				2
#define PFN_SCANRESULT_VERSION	1
#define MAX_PFN_LIST_COUNT	16

#define PFN_COMPLETE			1
#define PFN_INCOMPLETE			0

#define DEFAULT_BESTN			2
#define DEFAULT_MSCAN			0
#define DEFAULT_REPEAT			10
#define DEFAULT_EXP				2

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

typedef struct wl_pfn_scanresults {
	uint32 version;
	uint32 status;
	uint32 count;
	wl_pfn_net_info_t netinfo[1];
} wl_pfn_scanresults_t;

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
#define WL_PFN_SUPPRESSFOUND_MASK	0x08
#define WL_PFN_SUPPRESSLOST_MASK	0x10
#define WL_PFN_RSSI_MASK			0xff00
#define WL_PFN_RSSI_SHIFT			8

typedef struct wl_pfn_cfg {
	uint32				reporttype;
	int32				channel_num;
	uint16				channel_list[WL_NUMCHANNELS];
} wl_pfn_cfg_t;
#define WL_PFN_REPORT_ALLNET    0
#define WL_PFN_REPORT_SSIDNET   1
#define WL_PFN_REPORT_BSSIDNET  2

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

#define WL_PFN_HIDDEN_BIT		2
#define PNO_SCAN_MAX_FW			508*1000	/* max time scan time in msec */
#define PNO_SCAN_MAX_FW_SEC		PNO_SCAN_MAX_FW/1000 /* max time scan time in SEC */
#define PNO_SCAN_MIN_FW_SEC		10			/* min time scan time in SEC */
#define WL_PFN_HIDDEN_MASK		0x4

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

/* TCP Checksum Offload defines */
#define TOE_TX_CSUM_OL		0x00000001
#define TOE_RX_CSUM_OL		0x00000002

#ifndef LINUX_POSTMOGRIFY_REMOVAL
/* TCP Checksum Offload error injection for testing */
#define TOE_ERRTEST_TX_CSUM	0x00000001
#define TOE_ERRTEST_RX_CSUM	0x00000002
#define TOE_ERRTEST_RX_CSUM2	0x00000004

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

/* ARP Offload feature flags for arp_ol iovar */
#define ARP_OL_AGENT		0x00000001
#define ARP_OL_SNOOP		0x00000002
#define ARP_OL_HOST_AUTO_REPLY	0x00000004
#define ARP_OL_PEER_AUTO_REPLY	0x00000008

/* ARP Offload error injection */
#define ARP_ERRTEST_REPLY_PEER	0x1
#define ARP_ERRTEST_REPLY_HOST	0x2

#define ARP_MULTIHOMING_MAX	8	/* Maximum local host IP addresses */
#define ND_MULTIHOMING_MAX 10	/* Maximum local host IP addresses */

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
	uint16	len_bytes;	/* Size of packet to transmit (0 for null packet) */
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
	struct ether_addr 	addr;
	struct ether_addr 	top_master;
	uint8	dist_top;		/* Distance from Top */
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
	uint8 	type; 		/* 0x08 AWDL */
	uint8	version;
	uint8 	sub_type; 	/* Sub type */
	uint8	rsvd; 		/* Reserved */
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
	int8 master_sync_rssi_thld;
	int8 slave_sync_rssi_thld;
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
	struct ether_addr addr;
	uint32 flags;	/* Flags to indicate various states */
} BWL_POST_PACKED_STRUCT awdl_peer_op_node_t;

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
	uint32 	aw_dur;
	uint32	debug;
	uint32  txsupr;
	uint32	afrxdrop;
	uint32  awdrop;
	uint32  noawchansw;
	uint32  rx80211;
	uint32  peeropdrop;
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

typedef struct awdl_pw_opmode {
	struct ether_addr top_master;	/* Peer mac addr */
	uint8 mode; /* 0 - normal; 1 - fast mode */
} awdl_pw_opmode_t;

/* i/f request */
typedef struct wl_awdl_if {
	int32 cfg_idx;
	int32 up;
	struct ether_addr if_addr;
	struct ether_addr bssid;
} wl_awdl_if_t;

typedef struct _aw_start {
	uint8 role;
	struct ether_addr 	master;
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

#define SYNC_ROLE_SLAVE			0
#define SYNC_ROLE_NE_MASTER		1	/* Non-election master */
#define SYNC_ROLE_MASTER		2

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
 * Dongle pattern matching filter.
 */
/* AOAC wake event flag */
#define WAKE_EVENT_NLO_DISCOVERY_BIT 		1
#define WAKE_EVENT_AP_ASSOCIATION_LOST_BIT	2
#define WAKE_EVENT_GTK_HANDSHAKE_ERROR_BIT 4
#define WAKE_EVENT_4WAY_HANDSHAKE_REQUEST_BIT 8

#define MAX_NUM_WOL_PATTERN	16 /* LOGO requirements min 16 */
#define MAX_WAKE_PACKET_CACHE_BYTES 128 /* Maximum cached wake packet */

typedef struct pm_wake_packet {
	uint32	status;		/* Is the wake reason a packet (if all the other field's valid) */
	uint32	pattern_id;	/* Pattern ID that matched */
	uint32	original_packet_size;
	uint32	saved_packet_size;
	uchar	packet[MAX_WAKE_PACKET_CACHE_BYTES];
} pm_wake_packet_t;

/* Packet filter operation mode */
/* True: 1; False: 0 */
#define PKT_FILTER_MODE_FORWARD_ON_MATCH      		1
/* Enable and disable pkt_filter as a whole */
#define PKT_FILTER_MODE_DISABLE					2
/* Cache first matched rx pkt(be queried by host later) */
#define PKT_FILTER_MODE_PKT_CACHE_ON_MATCH    		4
/* If pkt_filter is enabled and no filter is set, don't forward anything */
#define PKT_FILTER_MODE_PKT_FORWARD_OFF_DEFAULT 8

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

/* Base offset values */
#define WL_PKT_FILTER_BASE_PKT   0
#define WL_PKT_FILTER_BASE_END   1
#define WL_PKT_FILTER_BASE_D11_H 2 /* May be removed */
#define WL_PKT_FILTER_BASE_D11_D 3 /* May be removed */
#define WL_PKT_FILTER_BASE_ETH_H 4
#define WL_PKT_FILTER_BASE_ETH_D 5
#define WL_PKT_FILTER_BASE_ARP_H 6
#define WL_PKT_FILTER_BASE_ARP_D 7 /* May be removed */
#define WL_PKT_FILTER_BASE_IP4_H 8
#define WL_PKT_FILTER_BASE_IP4_D 9
#define WL_PKT_FILTER_BASE_IP6_H 10
#define WL_PKT_FILTER_BASE_IP6_D 11
#define WL_PKT_FILTER_BASE_TCP_H 12
#define WL_PKT_FILTER_BASE_TCP_D 13 /* May be removed */
#define WL_PKT_FILTER_BASE_UDP_H 14
#define WL_PKT_FILTER_BASE_UDP_D 15
#define WL_PKT_FILTER_BASE_IP6_P 16
#define WL_PKT_FILTER_BASE_COUNT 17 /* May be removed */

/* String mapping for bases that may be used by applications or debug */
#define WL_PKT_FILTER_BASE_NAMES \
	{ "START", WL_PKT_FILTER_BASE_PKT },   \
	{ "END",   WL_PKT_FILTER_BASE_END },   \
	{ "ETH_H", WL_PKT_FILTER_BASE_ETH_H }, \
	{ "ETH_D", WL_PKT_FILTER_BASE_ETH_D }, \
	{ "D11_H", WL_PKT_FILTER_BASE_D11_H }, \
	{ "D11_D", WL_PKT_FILTER_BASE_D11_D }, \
	{ "ARP_H", WL_PKT_FILTER_BASE_ARP_H }, \
	{ "IP4_H", WL_PKT_FILTER_BASE_IP4_H }, \
	{ "IP4_D", WL_PKT_FILTER_BASE_IP4_D }, \
	{ "IP6_H", WL_PKT_FILTER_BASE_IP6_H }, \
	{ "IP6_D", WL_PKT_FILTER_BASE_IP6_D }, \
	{ "IP6_P", WL_PKT_FILTER_BASE_IP6_P }, \
	{ "TCP_H", WL_PKT_FILTER_BASE_TCP_H }, \
	{ "TCP_D", WL_PKT_FILTER_BASE_TCP_D }, \
	{ "UDP_H", WL_PKT_FILTER_BASE_UDP_H }, \
	{ "UDP_D", WL_PKT_FILTER_BASE_UDP_D }

/* Flags for a pattern list element */
#define WL_PKT_FILTER_MFLAG_NEG 0x0001

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

/*
 * Packet engine interface
 */

#define WL_PKTENG_PER_TX_START			0x01
#define WL_PKTENG_PER_TX_STOP			0x02
#define WL_PKTENG_PER_RX_START			0x04
#define WL_PKTENG_PER_RX_WITH_ACK_START 	0x05
#define WL_PKTENG_PER_TX_WITH_ACK_START 	0x06
#define WL_PKTENG_PER_RX_STOP			0x08
#define WL_PKTENG_PER_MASK			0xff

#define WL_PKTENG_SYNCHRONOUS			0x100	/* synchronous flag */

#define WL_PKTENG_MAXPKTSZ				16384	/* max pktsz limit for pkteng */

typedef struct wl_pkteng {
	uint32 flags;
	uint32 delay;			/* Inter-packet delay */
	uint32 nframes;			/* Number of frames */
	uint32 length;			/* Packet length */
	uint8  seqno;			/* Enable/disable sequence no. */
	struct ether_addr dest;		/* Destination address */
	struct ether_addr src;		/* Source address */
} wl_pkteng_t;

#define NUM_80211b_RATES	4
#define NUM_80211ag_RATES	8
#define NUM_80211n_RATES	32
#define NUM_80211_RATES		(NUM_80211b_RATES+NUM_80211ag_RATES+NUM_80211n_RATES)
typedef struct wl_pkteng_stats {
	uint32 lostfrmcnt;		/* RX PER test: no of frames lost (skip seqno) */
	int32 rssi;			/* RSSI */
	int32 snr;			/* signal to noise ratio */
	uint16 rxpktcnt[NUM_80211_RATES+1];
	uint8 rssi_qdb; /* qdB portion of the computed rssi */
} wl_pkteng_stats_t;

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

/*
 * WOWL capability/override settings
 */
#define WL_WOWL_MAGIC           (1 << 0)    /* Wakeup on Magic packet */
#define WL_WOWL_NET             (1 << 1)    /* Wakeup on Netpattern */
#define WL_WOWL_DIS             (1 << 2)    /* Wakeup on loss-of-link due to Disassoc/Deauth */
#define WL_WOWL_RETR            (1 << 3)    /* Wakeup on retrograde TSF */
#define WL_WOWL_BCN             (1 << 4)    /* Wakeup on loss of beacon */
#define WL_WOWL_TST             (1 << 5)    /* Wakeup after test */
#define WL_WOWL_M1              (1 << 6)    /* Wakeup after PTK refresh */
#define WL_WOWL_EAPID           (1 << 7)    /* Wakeup after receipt of EAP-Identity Req */
#define WL_WOWL_PME_GPIO        (1 << 8)    /* Wakeind via PME(0) or GPIO(1) */
#define WL_WOWL_NEEDTKIP1       (1 << 9)    /* need tkip phase 1 key to be updated by the driver */
#define WL_WOWL_GTK_FAILURE     (1 << 10)   /* enable wakeup if GTK fails */
#define WL_WOWL_EXTMAGPAT       (1 << 11)   /* support extended magic packets */
#define WL_WOWL_ARPOFFLOAD      (1 << 12)   /* support ARP/NS/keepalive offloading */
#define WL_WOWL_WPA2            (1 << 13)   /* read protocol version for EAPOL frames */
#define WL_WOWL_KEYROT          (1 << 14)   /* If the bit is set, use key rotaton */
#define WL_WOWL_BCAST           (1 << 15)   /* If the bit is set, frm received was bcast frame */
#define WL_WOWL_SCANOL          (1 << 16)   /* If the bit is set, scan offload is enabled */
#define WL_WOWL_NOT_USED        (1 << 17)   /* Currently unused, use this bit next */
#define WL_WOWL_MDNS_CONFLICT   (1 << 18)   /* Wakeup on mDNS Conflict Resolution */
#define WL_WOWL_MDNS_SERVICE    (1 << 19)   /* Wakeup on mDNS Service Connect */
#define WL_WOWL_TCPKEEP         (1 << 20)   /* tcp keepalive either timed out or got data */
#define WL_WOWL_FW_HALT         (1 << 21)   /* Firmware died in wowl mode */
#define WL_WOWL_ENAB_HWRADIO    (1 << 22)   /* Enable detection of radio button changes */


#define MAGIC_PKT_MINLEN	102    /* Magic pkt min length is 6 * 0xFF + 16 * ETHER_ADDR_LEN */
#define MAGIC_PKT_NUM_MAC_ADDRS	16

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


#if defined(DSLCPE_DELAY)
#define WL_DELAYMODE_DEFER  0   /* defer by scheduler's choice, make this driver default */
#define WL_DELAYMODE_FORCE  1   /* force, this is driver default */
#define WL_DELAYMODE_AUTO   2   /* defer if no sta associated, force if sta associated */
#endif

/* Overlap BSS Scan parameters default, minimum, maximum */
#define WLC_OBSS_SCAN_PASSIVE_DWELL_DEFAULT		20	/* unit TU */
#define WLC_OBSS_SCAN_PASSIVE_DWELL_MIN			5	/* unit TU */
#define WLC_OBSS_SCAN_PASSIVE_DWELL_MAX			1000	/* unit TU */
#define WLC_OBSS_SCAN_ACTIVE_DWELL_DEFAULT		10	/* unit TU */
#define WLC_OBSS_SCAN_ACTIVE_DWELL_MIN			10	/* unit TU */
#define WLC_OBSS_SCAN_ACTIVE_DWELL_MAX			1000	/* unit TU */
#define WLC_OBSS_SCAN_WIDTHSCAN_INTERVAL_DEFAULT	300	/* unit Sec */
#define WLC_OBSS_SCAN_WIDTHSCAN_INTERVAL_MIN		10	/* unit Sec */
#define WLC_OBSS_SCAN_WIDTHSCAN_INTERVAL_MAX		900	/* unit Sec */
#define WLC_OBSS_SCAN_CHANWIDTH_TRANSITION_DLY_DEFAULT	5
#define WLC_OBSS_SCAN_CHANWIDTH_TRANSITION_DLY_MIN	5
#define WLC_OBSS_SCAN_CHANWIDTH_TRANSITION_DLY_MAX	100
#define WLC_OBSS_SCAN_PASSIVE_TOTAL_PER_CHANNEL_DEFAULT	200	/* unit TU */
#define WLC_OBSS_SCAN_PASSIVE_TOTAL_PER_CHANNEL_MIN	200	/* unit TU */
#define WLC_OBSS_SCAN_PASSIVE_TOTAL_PER_CHANNEL_MAX	10000	/* unit TU */
#define WLC_OBSS_SCAN_ACTIVE_TOTAL_PER_CHANNEL_DEFAULT	20	/* unit TU */
#define WLC_OBSS_SCAN_ACTIVE_TOTAL_PER_CHANNEL_MIN	20	/* unit TU */
#define WLC_OBSS_SCAN_ACTIVE_TOTAL_PER_CHANNEL_MAX	10000	/* unit TU */
#define WLC_OBSS_SCAN_ACTIVITY_THRESHOLD_DEFAULT	25	/* unit percent */
#define WLC_OBSS_SCAN_ACTIVITY_THRESHOLD_MIN		0	/* unit percent */
#define WLC_OBSS_SCAN_ACTIVITY_THRESHOLD_MAX		100	/* unit percent */

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
#define WL_MIN_NUM_OBSS_SCAN_ARG 7	/* minimum number of arguments required for OBSS Scan */

#define WL_COEX_INFO_MASK		0x07
#define WL_COEX_INFO_REQ		0x01
#define	WL_COEX_40MHZ_INTOLERANT	0x02
#define	WL_COEX_WIDTH20			0x04

#define	WLC_RSSI_INVALID	 0	/* invalid RSSI value */

#define MAX_RSSI_LEVELS 8

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

typedef struct wl_action_obss_coex_req {
	uint8 info;
	uint8 num;
	uint8 ch_list[1];
} wl_action_obss_coex_req_t;


/* IOVar parameter block for small MAC address array with type indicator */
#define WL_IOV_MAC_PARAM_LEN  4

#define WL_IOV_PKTQ_LOG_PRECS 16

typedef struct {
	uint32 num_addrs;
	char   addr_type[WL_IOV_MAC_PARAM_LEN];
	struct ether_addr ea[WL_IOV_MAC_PARAM_LEN];
} wl_iov_mac_params_t;

/* This is extra info that follows wl_iov_mac_params_t */
typedef struct {
	uint32 addr_info[WL_IOV_MAC_PARAM_LEN];
} wl_iov_mac_extra_params_t;

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
	uint32 airtime;        /* RESERVED - zero until pktq_stats V5 is committed. */
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
	iov_bs_data_record_t	structure_record[];	/* 0 - structure_count records */
} BWL_POST_PACKED_STRUCT iov_bs_data_struct_t;

/* Bitmask of options that can be passed in to the iovar. */
enum {
	SCB_BS_DATA_FLAG_NO_RESET = (1<<0)	/* Do not clear the counters after reading */
};
/*
 * SCB_BS_DATA iovar definitions end.
 */

/* **** EXTLOG **** */
#define EXTLOG_CUR_VER		0x0100

#define MAX_ARGSTR_LEN		18 /* At least big enough for storing ETHER_ADDR_STR_LEN */

/* log modules (bitmap) */
#define LOG_MODULE_COMMON	0x0001
#define LOG_MODULE_ASSOC	0x0002
#define LOG_MODULE_EVENT	0x0004
#define LOG_MODULE_MAX		3			/* Update when adding module */

/* log levels */
#define WL_LOG_LEVEL_DISABLE	0
#define WL_LOG_LEVEL_ERR	1
#define WL_LOG_LEVEL_WARN	2
#define WL_LOG_LEVEL_INFO	3
#define WL_LOG_LEVEL_MAX	WL_LOG_LEVEL_INFO	/* Update when adding level */

/* flag */
#define LOG_FLAG_EVENT		1

/* log arg_type */
#define LOG_ARGTYPE_NULL	0
#define LOG_ARGTYPE_STR		1	/* %s */
#define LOG_ARGTYPE_INT		2	/* %d */
#define LOG_ARGTYPE_INT_STR	3	/* %d...%s */
#define LOG_ARGTYPE_STR_INT	4	/* %s...%d */

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

#define OVERLAY_IDX_MASK		0x000000ff
#define OVERLAY_IDX_SHIFT		0
#define OVERLAY_FLAGS_MASK		0xffffff00
#define OVERLAY_FLAGS_SHIFT		8
/* overlay written to device memory immediately after loading the base image */
#define OVERLAY_FLAG_POSTLOAD	0x100
/* defer overlay download until the device responds w/WLC_E_OVL_DOWNLOAD event */
#define OVERLAY_FLAG_DEFER_DL	0x200
/* overlay downloaded prior to the host going to sleep */
#define OVERLAY_FLAG_PRESLEEP	0x400

#define OVERLAY_DOWNLOAD_CHUNKSIZE	1024
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

/* require strict packing */
#include <packed_section_start.h>

#ifndef LINUX_POSTMOGRIFY_REMOVAL

/* Structures and constants used for "vndr_ie" IOVar interface */
#define VNDR_IE_CMD_LEN		4	/* length of the set command string:
					 * "add", "del" (+ NUL)
					 */

/* 802.11 Mgmt Packet flags */
#define VNDR_IE_BEACON_FLAG	0x1
#define VNDR_IE_PRBRSP_FLAG	0x2
#define VNDR_IE_ASSOCRSP_FLAG	0x4
#define VNDR_IE_AUTHRSP_FLAG	0x8
#define VNDR_IE_PRBREQ_FLAG	0x10
#define VNDR_IE_ASSOCREQ_FLAG	0x20
#define VNDR_IE_IWAPID_FLAG	0x40 /* vendor IE in IW advertisement protocol ID field */
#define VNDR_IE_CUSTOM_FLAG	0x100 /* allow custom IE id */

#if defined(WLP2P)
/* P2P Action Frames flags (spec ordered) */
#define VNDR_IE_GONREQ_FLAG     0x001000
#define VNDR_IE_GONRSP_FLAG     0x002000
#define VNDR_IE_GONCFM_FLAG     0x004000
#define VNDR_IE_INVREQ_FLAG     0x008000
#define VNDR_IE_INVRSP_FLAG     0x010000
#define VNDR_IE_DISREQ_FLAG     0x020000
#define VNDR_IE_DISRSP_FLAG     0x040000
#define VNDR_IE_PRDREQ_FLAG     0x080000
#define VNDR_IE_PRDRSP_FLAG     0x100000

#define VNDR_IE_P2PAF_SHIFT	12
#endif /* WLP2P */

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
	uint32 board_limit_len;		/* length of board limit buffer */
	uint32 target_len;		/* length of target power buffer */
	int8 SARLIMIT[MAX_STREAMS_SUPPORTED];
	uint8  pprdata[1];		/* ppr serialization buffer */
} BWL_POST_PACKED_STRUCT tx_pwr_rpt_t;

#define TXPWR_TARGET_VERSION  0
typedef BWL_PRE_PACKED_STRUCT struct {
	int32 version;		/* version number */
	chanspec_t chanspec;	/* txpwr report for this channel */
	int8 txpwr[WL_STA_ANT_MAX]; /* Max tx target power, in qdb */
	uint8 rf_cores;		/* count of RF Cores being reported */
} BWL_POST_PACKED_STRUCT txpwr_target_max_t;

/* fbt_cap: FBT assoc / reassoc modes. */
#define WLC_FBT_CAP_DRV_4WAY_AND_REASSOC  1 /* Driver 4-way handshake & reassoc (WLFBT). */

#endif /* LINUX_POSTMOGRIFY_REMOVAL */

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


/* channel interference measurement (chanim) related defines */

/* chanim mode */
#define CHANIM_DISABLE	0	/* disabled */
#define CHANIM_DETECT	1	/* detection only */
#define CHANIM_EXT		2 	/* external state machine */
#define CHANIM_ACT		3	/* full internal state machine, detect + act */
#define CHANIM_MODE_MAX 4

/* define for apcs reason code */
#define APCS_INIT		0
#define APCS_IOCTL		1
#define APCS_CHANIM		2
#define APCS_CSTIMER		3
#define APCS_BTA		4
#define APCS_TXDLY		5
#define APCS_NONACSD		6
#define APCS_DFS_REENTRY	7
#define APCS_TXFAIL		8
#define APCS_MAX		9

/* number of ACS record entries */
#define CHANIM_ACS_RECORD			10

/* CHANIM */
#define CCASTATS_TXDUR  0
#define CCASTATS_INBSS  1
#define CCASTATS_OBSS   2
#define CCASTATS_NOCTG  3
#define CCASTATS_NOPKT  4
#define CCASTATS_DOZE   5
#define CCASTATS_TXOP	6
#define CCASTATS_GDTXDUR        7
#define CCASTATS_BDTXDUR        8
#define CCASTATS_MAX    9

/* chanim acs record */
typedef struct {
	bool valid;
	uint8 trigger;
	chanspec_t selected_chspc;
	int8 bgnoise;
	uint32 glitch_cnt;
	uint8 ccastats;
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
	uint8 chan_idle;		/* normalized as 0~100 */
} chanim_stats_t;

#define WL_CHANIM_STATS_VERSION 1
#define WL_CHANIM_COUNT_ALL	0xff
#define WL_CHANIM_COUNT_ONE	0x1

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

/* ap tpc modes */
#define	AP_TPC_OFF		0
#define	AP_TPC_BSS_PWR		1	/* BSS power control */
#define AP_TPC_AP_PWR		2	/* AP power control */
#define	AP_TPC_AP_BSS_PWR	3	/* Both AP and BSS power control */
#define AP_TPC_MAX_LINK_MARGIN	127

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

/* reuse two number in the sc/rc space */
#define	SMFS_CODE_MALFORMED 0xFFFE
#define SMFS_CODE_IGNORED 	0xFFFD

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

/* state */
#define WL_P2P_DISC_ST_SCAN	0
#define WL_P2P_DISC_ST_LISTEN	1
#define WL_P2P_DISC_ST_SEARCH	2

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

/* i/f type */
#define WL_P2P_IF_CLIENT	0
#define WL_P2P_IF_GO		1
#define WL_P2P_IF_DYNBCN_GO	2
#define WL_P2P_IF_DEV		3

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

/* count */
#define WL_P2P_SCHED_RSVD	0
#define WL_P2P_SCHED_REPEAT	255	/* anything > 255 will be treated as 255 */

typedef struct wl_p2p_sched {
	uint8 type;	/* see schedule type */
	uint8 action;	/* see schedule action */
	uint8 option;	/* see schedule option */
	wl_p2p_sched_desc_t desc[1];
} wl_p2p_sched_t;
#define WL_P2P_SCHED_FIXED_LEN		3

/* schedule type */
#define WL_P2P_SCHED_TYPE_ABS		0	/* Scheduled Absence */
#define WL_P2P_SCHED_TYPE_REQ_ABS	1	/* Requested Absence */

/* schedule action during absence periods (for WL_P2P_SCHED_ABS type) */
#define WL_P2P_SCHED_ACTION_NONE	0	/* no action */
#define WL_P2P_SCHED_ACTION_DOZE	1	/* doze */
/* schedule option - WL_P2P_SCHED_TYPE_REQ_ABS */
#define WL_P2P_SCHED_ACTION_GOOFF	2	/* turn off GO beacon/prbrsp functions */
/* schedule option - WL_P2P_SCHED_TYPE_XXX */
#define WL_P2P_SCHED_ACTION_RESET	255	/* reset */

/* schedule option - WL_P2P_SCHED_TYPE_ABS */
#define WL_P2P_SCHED_OPTION_NORMAL	0	/* normal start/interval/duration/count */
#define WL_P2P_SCHED_OPTION_BCNPCT	1	/* percentage of beacon interval */
/* schedule option - WL_P2P_SCHED_TYPE_REQ_ABS */
#define WL_P2P_SCHED_OPTION_TSFOFS	2	/* normal start/internal/duration/count with
						 * start being an offset of the 'current' TSF
						 */

/* feature flags */
#define WL_P2P_FEAT_GO_CSA	(1 << 0)	/* GO moves with the STA using CSA method */
#define WL_P2P_FEAT_GO_NOLEGACY	(1 << 1)	/* GO does not probe respond to non-p2p probe
						 * requests
						 */
#define WL_P2P_FEAT_RESTRICT_DEV_RESP (1 << 2)	/* Restrict p2p dev interface from responding */

/* RFAWARE def */
#define BCM_ACTION_RFAWARE		0x77
#define BCM_ACTION_RFAWARE_DCS  0x01

/* DCS reason code define */
#define BCM_DCS_IOVAR		0x1
#define BCM_DCS_UNKNOWN		0xFF

typedef struct wl_bcmdcs_data {
	uint reason;
	chanspec_t chspec;
} wl_bcmdcs_data_t;

/* n-mode support capability */
/* 2x2 includes both 1x1 & 2x2 devices
 * reserved #define 2 for future when we want to separate 1x1 & 2x2 and
 * control it independently
 */
#define WL_11N_2x2			1
#define WL_11N_3x3			3
#define WL_11N_4x4			4

/* define 11n feature disable flags */
#define WLFEATURE_DISABLE_11N		0x00000001
#define WLFEATURE_DISABLE_11N_STBC_TX	0x00000002
#define WLFEATURE_DISABLE_11N_STBC_RX	0x00000004
#define WLFEATURE_DISABLE_11N_SGI_TX	0x00000008
#define WLFEATURE_DISABLE_11N_SGI_RX	0x00000010
#define WLFEATURE_DISABLE_11N_AMPDU_TX	0x00000020
#define WLFEATURE_DISABLE_11N_AMPDU_RX	0x00000040
#define WLFEATURE_DISABLE_11N_GF	0x00000080

/* Proxy STA modes */
#define PSTA_MODE_DISABLED		0
#define PSTA_MODE_PROXY			1
#define PSTA_MODE_REPEATER		2


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

/* op code in nat_cfg */
#define NAT_OP_ENABLE		1	/* enable NAT on given interface */
#define NAT_OP_DISABLE		2	/* disable NAT on given interface */
#define NAT_OP_DISABLE_ALL	3	/* disable NAT on all interfaces */

/* NAT state */
#define NAT_STATE_ENABLED	1	/* NAT is enabled */
#define NAT_STATE_DISABLED	2	/* NAT is disabled */

typedef struct {
	int state;	/* NAT state returned */
} nat_state_t;

#ifdef PROP_TXSTATUS
/* Bit definitions for tlv iovar */
/*
 * enable RSSI signals:
 * WLFC_CTL_TYPE_RSSI
 */
#define WLFC_FLAGS_RSSI_SIGNALS                         1

/* enable (if/mac_open, if/mac_close,, mac_add, mac_del) signals:
 *
 * WLFC_CTL_TYPE_MAC_OPEN
 * WLFC_CTL_TYPE_MAC_CLOSE
 *
 * WLFC_CTL_TYPE_INTERFACE_OPEN
 * WLFC_CTL_TYPE_INTERFACE_CLOSE
 *
 * WLFC_CTL_TYPE_MACDESC_ADD
 * WLFC_CTL_TYPE_MACDESC_DEL
 *
 */
#define WLFC_FLAGS_XONXOFF_SIGNALS                      2

/* enable (status, fifo_credit, mac_credit) signals
 * WLFC_CTL_TYPE_MAC_REQUEST_CREDIT
 * WLFC_CTL_TYPE_TXSTATUS
 * WLFC_CTL_TYPE_FIFO_CREDITBACK
 */
#define WLFC_FLAGS_CREDIT_STATUS_SIGNALS        4

#define WLFC_FLAGS_HOST_PROPTXSTATUS_ACTIVE     8
#define WLFC_FLAGS_PSQ_GENERATIONFSM_ENABLE     16
#define WLFC_FLAGS_PSQ_ZERO_BUFFER_ENABLE	32
#endif /* PROP_TXSTATUS */

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

#define CHANNEL_5G_LOW_START	36	/* 5G low (36..48) CDD enable/disable bit mask */
#define CHANNEL_5G_MID_START	52	/* 5G mid (52..64) CDD enable/disable bit mask */
#define CHANNEL_5G_HIGH_START	100	/* 5G high (100..140) CDD enable/disable bit mask */
#define CHANNEL_5G_UPPER_START	149	/* 5G upper (149..161) CDD enable/disable bit mask */

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

/* IOVAR "mempool" parameter. Used to retrieve a list of memory pool statistics. */
typedef struct wl_mempool_stats {
	int	num;		/* Number of memory pools */
	bcm_mp_stats_t s[1];	/* Variable array of memory pool stats. */
} wl_mempool_stats_t;

/* Network Offload Engine */
#define NWOE_OL_ENABLE		0x00000001

typedef struct {
	uint32 ipaddr;
	uint32 ipaddr_netmask;
	uint32 ipaddr_gateway;
} nwoe_ifconfig_t;

/*
 * Traffic management structures/defines.
 */

/* Traffic management bandwidth parameters */
#define TRF_MGMT_MAX_PRIORITIES                 3

#define TRF_MGMT_FLAG_ADD_DSCP                  0x0001  /* Add DSCP to IP TOS field */
#define TRF_MGMT_FLAG_DISABLE_SHAPING           0x0002  /* Don't shape traffic */
#define TRF_MGMT_FLAG_MANAGE_LOCAL_TRAFFIC      0x0008  /* Manage traffic over our local subnet */
#define TRF_MGMT_FLAG_FILTER_ON_MACADDR         0x0010  /* filter on MAC address */
#define TRF_MGMT_FLAG_NO_RX                     0x0020  /* do not apply fiters to rx packets */

#define TRF_FILTER_MAC_ADDR              0x0001 /* L2 filter use dst mac address for filtering */
#define TRF_FILTER_IP_ADDR               0x0002 /* L3 filter use ip ddress for filtering */
#define TRF_FILTER_L4                    0x0004 /* L4 filter use tcp/udp for filtering */
#define TRF_FILTER_DWM                   0x0008 /* L3 filter use DSCP for filtering */
#define TRF_FILTER_FAVORED               0x0010 /* Tag the packet FAVORED */

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

/* WNM/NPS subfeatures mask */
#define WL_WNM_BSSTRANS		0x00000001
#define WL_WNM_PROXYARP		0x00000002
#define WL_WNM_MAXIDLE		0x00000004
#define WL_WNM_TIMBC		0x00000008
#define WL_WNM_TFS		0x00000010
#define WL_WNM_SLEEP		0x00000020
#define WL_WNM_DMS		0x00000040
#define WL_WNM_FMS		0x00000080
#define WL_WNM_NOTIF		0x00000100
#define WL_WNM_MAX		0x00000200

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
	uint8 tfs_id;			/* ID of a specific set (existing or new), or 0 for all */
	uint8 actcode;			/* Action code for this filter set */
	uint8 tclas_proc;		/* TCLAS processing operator for this filter set */
} wl_tfs_set_t;

enum {
	TFS_TERM_SEND_NOW = 1,		/* Send the TFS frame immediatly */
	TFS_TERM_DELETE = 2		/* Also delete set internally */
};

typedef struct wl_tfs_term {
	uint8 flags;			/* Bitfield of WL_TFS_TERM_xxx */
	uint8 tfs_id;			/* ID of a specific set (existing), or 0 for all */
} wl_tfs_term_t;


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
	DMS_STATUS_ERROR = 8		/* Non compliant AP behvior */
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

#define WL_TIMBC_STATUS_AP_UNKNOWN	255	/* AP status for internal use only */

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

/* Definitions for Reliable Multicast */
#define WL_RELMCAST_MAX_CLIENT		32
#define WL_RELMCAST_FLAG_INBLACKLIST	1
#define WL_RELMCAST_FLAG_ACTIVEACKER	2
#define WL_RELMCAST_FLAG_RELMCAST	4

#define WL_RELMCAST_VER			1

typedef struct wl_relmcast_client {
	uint8 flag;
	int16 rssi;
	struct ether_addr addr;
} wl_relmcast_client_t;

typedef struct wl_relmcast_st {
	uint8 ver;
	uint8 num;
	wl_relmcast_client_t clients[WL_RELMCAST_MAX_CLIENT];
} wl_relmcast_status_t;

/* structures for proximity detection device role */
#define WL_PROXD_MODE_DISABLE	0
#define WL_PROXD_MODE_NEUTRAL	1
#define WL_PROXD_MODE_INITIATOR	2
#define WL_PROXD_MODE_TARGET	3
#define WL_PROXD_RANDOM_WAKEUP	0x8000

typedef struct wl_proxd_iovar {
	uint16	method;		/* Proxmity Detection method */
	uint16	mode;		/* Mode (neutral, initiator, target) */
} wl_proxd_iovar_t;

/* structures for proximity detection parameters */
typedef struct wl_proxd_params_rssi_method {
	chanspec_t	chanspec;	/* chanspec for home channel */
	uint16		interval;	/* interval between neighbor finding attempts (in TU) */
	uint16		duration;	/* duration of neighbor finding attempts (in ms) */
	int16		rssi_thresh;	/* RSSI threshold (in dBm) */
	int16		tx_power;	/* tx power of Proximity Detection frames (in dBm) */
	uint16		tx_rate;	/* tx rate of Proximity Detection frames
					 * (in 500kbps units)
					 */
	uint16		timeout;	/* state machine wait timeout of the frames (in ms) */
	uint16		maxconvergtmo;	/* max wait converge timeout (in ms) */
} wl_proxd_params_rssi_method_t;

typedef struct wl_proxd_params_iovar {
	uint16	method;			/* Proxmity Detection method */
	union {
		wl_proxd_params_rssi_method_t rssi_params;
	} u;				/* Method specific optional parameters */
} wl_proxd_params_iovar_t;

enum {
	RSSI_REASON_UNKNOW,
	RSSI_REASON_LOWRSSI,
	RSSI_REASON_NSYC,
	RSSI_REASON_TIMEOUT
};

enum {
	RSSI_STATE_POLL,
	RSSI_STATE_TPAIRING,
	RSSI_STATE_IPAIRING,
	RSSI_STATE_THANDSHAKE,
	RSSI_STATE_IHANDSHAKE,
	RSSI_STATE_CONFIRMED,
	RSSI_STATE_PIPELINE,
	RSSI_STATE_NEGMODE,
	RSSI_STATE_MONITOR,
	RSSI_STATE_LAST
};

typedef struct wl_proxd_status_iovar {
	uint8			mode;
	uint8			peermode;
	uint8			state;
	uint8			reason;
	uint32			txcnt;
	uint32			rxcnt;
	struct ether_addr	peer;
	int16			hi_rssi;
	int16			low_rssi;
} wl_proxd_status_iovar_t;

#ifdef NET_DETECT

#define NET_DETECT_MAX_WAKE_DATA_SIZE	2048
#define NET_DETECT_MAX_PROFILES		16
#define NET_DETECT_MAX_CHANNELS		50

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

typedef struct wlc_l2keepalive_ol_params {
	uint8 	flags;
	uint8	prio;
	uint16	period_ms;
} wlc_l2keepalive_ol_params_t;

typedef struct wlc_dwds_config {
	uint32		enable;
	uint32		mode; /* STA/AP interface */
	struct ether_addr ea;
} wlc_dwds_config_t;

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
	STAMON_CFG_CMD_ADD = 1,
	STAMON_CFG_CMD_ENB = 2,
	STAMON_CFG_CMD_DSB = 3
} wl_stamon_cfg_cmd_type_t;

typedef struct wlc_stamon_sta_config {
	wl_stamon_cfg_cmd_type_t cmd; /* 0 - delete, 1 - add */
	struct ether_addr ea;
} wlc_stamon_sta_config_t;

/* monitor_promisc_level bits */
#define WL_MONPROMISC_PROMISC 0x0001
#define WL_MONPROMISC_CTRL 0x0002
#define WL_MONPROMISC_FCS 0x0004

typedef struct wl_taf_sta {
	struct ether_addr ea;	/* STA MAC or null for default */
	uint16 pad;
	uint32 rules_enabled;	/* Bitfield of rules to enable */
	uint32 rule_user_val;	/* Parameter for rule User */
} wl_taf_sta_t;

#define WL_TAF_RULE_AID_BIT	0x01
#define WL_TAF_RULE_RSSI_BIT	0x02
#define WL_TAF_RULE_RATE_BIT	0x04
#define WL_TAF_RULE_PRIO_BIT	0x08
#define WL_TAF_RULE_SSID_BIT	0x10
#define WL_TAF_RULE_USER_BIT	0x20

/* WDS net interface types */
#define WL_WDSIFTYPE_NONE  0x0 /* The interface type is neither WDS nor DWDS. */
#define WL_WDSIFTYPE_WDS   0x1 /* The interface is WDS type. */
#define WL_WDSIFTYPE_DWDS  0x2 /* The interface is DWDS type. */

typedef struct {
	uint32 config;	/* MODE: AUTO (-1), Disable (0), Enable (1) */
	uint32 status;	/* Current state: Disabled (0), Enabled (1) */
} wl_config_t;

#endif /* _wlioctl_h_ */
