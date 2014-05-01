/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: ieee80211_ioctl.h 1856 2006-12-14 01:38:00Z scottr $
 */
#ifndef _NET80211_IEEE80211_IOCTL_H_
#define _NET80211_IEEE80211_IOCTL_H_

/*
 * IEEE 802.11 ioctls.
 */
#include "net80211/_ieee80211.h"
#include "net80211/ieee80211.h"
#include "net80211/ieee80211_qos.h"
#include "net80211/ieee80211_crypto.h"

/*
 * Per/node (station) statistics available when operating as an AP.
 */
struct ieee80211_nodestats {
	u_int32_t ns_rx_data;		/* rx data frames */
	u_int32_t ns_rx_mgmt;		/* rx management frames */
	u_int32_t ns_rx_ctrl;		/* rx control frames */
	u_int32_t ns_rx_ucast;		/* rx unicast frames */
	u_int32_t ns_rx_mcast;		/* rx multicast frames */
	u_int32_t ns_rx_bcast;		/* rx broadcast frames */
	u_int64_t ns_rx_bytes;		/* rx data count (bytes) */
	u_int64_t ns_rx_beacons;		/* rx beacon frames */
	u_int32_t ns_rx_proberesp;	/* rx probe response frames */

	u_int32_t ns_rx_dup;		/* rx discard because it's a dup */
	u_int32_t ns_rx_noprivacy;	/* rx w/ wep but privacy off */
	u_int32_t ns_rx_wepfail;		/* rx wep processing failed */
	u_int32_t ns_rx_demicfail;	/* rx demic failed */
	u_int32_t ns_rx_decap;		/* rx decapsulation failed */
	u_int32_t ns_rx_defrag;		/* rx defragmentation failed */
	u_int32_t ns_rx_disassoc;	/* rx disassociation */
	u_int32_t ns_rx_deauth;		/* rx deauthentication */
	u_int32_t ns_rx_decryptcrc;	/* rx decrypt failed on crc */
	u_int32_t ns_rx_unauth;		/* rx on unauthorized port */
	u_int32_t ns_rx_unencrypted;	/* rx unecrypted w/ privacy */

	u_int32_t ns_tx_data;		/* tx data frames */
	u_int32_t ns_tx_mgmt;		/* tx management frames */
	u_int32_t ns_tx_ucast;		/* tx unicast frames */
	u_int32_t ns_tx_mcast;		/* tx multicast frames */
	u_int32_t ns_tx_bcast;		/* tx broadcast frames */
	u_int64_t ns_tx_bytes;		/* tx data count (bytes) */
	u_int32_t ns_tx_probereq;	/* tx probe request frames */
	u_int32_t ns_tx_uapsd;		/* tx on uapsd queue */

	u_int32_t ns_tx_novlantag;	/* tx discard due to no tag */
	u_int32_t ns_tx_vlanmismatch;	/* tx discard due to of bad tag */
	u_int32_t ns_tx_unauth;		/* rx on unauthorized port */

	u_int32_t ns_tx_eosplost;	/* uapsd EOSP retried out */

	u_int32_t ns_ps_discard;		/* ps discard due to of age */

	u_int32_t ns_uapsd_triggers;	/* uapsd triggers */

	/* MIB-related state */
	u_int32_t ns_tx_assoc;		/* [re]associations */
	u_int32_t ns_tx_assoc_fail;	/* [re]association failures */
	u_int32_t ns_tx_auth;		/* [re]authentications */
	u_int32_t ns_tx_auth_fail;	/* [re]authentication failures*/
	u_int32_t ns_tx_deauth;		/* deauthentications */
	u_int32_t ns_tx_deauth_code;	/* last deauth reason */
	u_int32_t ns_tx_disassoc;	/* disassociations */
	u_int32_t ns_tx_disassoc_code;	/* last disassociation reason */
	u_int32_t ns_psq_drops;		/* power save queue drops */
	u_int32_t ns_rx_action;         /* rx action */
	u_int32_t ns_tx_action;
	/*
	 * Next few fields track the corresponding entry in struct net_device_stats,
	 * but here for each associated node
	 */
	u_int32_t ns_rx_errors;
	u_int32_t ns_tx_errors;
	u_int32_t ns_rx_dropped;
	u_int32_t ns_tx_dropped;

	u_int32_t ns_rx_fragment_pkts;
	u_int32_t ns_rx_vlan_pkts;

	u_int32_t ns_ap_isolation_dropped;
};

/*
 * Summary statistics.
 */
struct ieee80211_stats {
	u_int32_t is_rx_badversion;	/* rx frame with bad version */
	u_int32_t is_rx_tooshort;	/* rx frame too short */
	u_int32_t is_rx_wrongbss;	/* rx from wrong bssid */
	u_int32_t is_rx_dup;		/* rx discard due to it's a dup */
	u_int32_t is_rx_wrongdir;	/* rx w/ wrong direction */
	u_int32_t is_rx_mcastecho;	/* rx discard due to of mcast echo */
	u_int32_t is_rx_notassoc;	/* rx discard due to sta !assoc */
	u_int32_t is_rx_noprivacy;	/* rx w/ wep but privacy off */
	u_int32_t is_rx_unencrypted;	/* rx w/o wep and privacy on */
	u_int32_t is_rx_wepfail;		/* rx wep processing failed */
	u_int32_t is_rx_decap;		/* rx decapsulation failed */
	u_int32_t is_rx_mgtdiscard;	/* rx discard mgt frames */
	u_int32_t is_rx_ctl;		/* rx discard ctrl frames */
	u_int32_t is_rx_beacon;		/* rx beacon frames */
	u_int32_t is_rx_rstoobig;	/* rx rate set truncated */
	u_int32_t is_rx_elem_missing;	/* rx required element missing*/
	u_int32_t is_rx_elem_toobig;	/* rx element too big */
	u_int32_t is_rx_elem_toosmall;	/* rx element too small */
	u_int32_t is_rx_elem_unknown;	/* rx element unknown */
	u_int32_t is_rx_badchan;	/* rx frame w/ invalid chan */
	u_int32_t is_rx_chanmismatch;	/* rx frame chan mismatch */
	u_int32_t is_rx_nodealloc;	/* rx frame dropped */
	u_int32_t is_rx_ssidmismatch;	/* rx frame ssid mismatch  */
	u_int32_t is_rx_auth_unsupported;/* rx w/ unsupported auth alg */
	u_int32_t is_rx_auth_fail;	/* rx sta auth failure */
	u_int32_t is_rx_auth_countermeasures;/* rx auth discard due to CM */
	u_int32_t is_rx_assoc_bss;	/* rx assoc from wrong bssid */
	u_int32_t is_rx_assoc_notauth;	/* rx assoc w/o auth */
	u_int32_t is_rx_assoc_capmismatch;/* rx assoc w/ cap mismatch */
	u_int32_t is_rx_assoc_norate;	/* rx assoc w/ no rate match */
	u_int32_t is_rx_assoc_badwpaie;	/* rx assoc w/ bad WPA IE */
	u_int32_t is_rx_deauth;		/* rx deauthentication */
	u_int32_t is_rx_disassoc;	/* rx disassociation */
	u_int32_t is_rx_action;         /* rx action mgt */
	u_int32_t is_rx_badsubtype;	/* rx frame w/ unknown subtype*/
	u_int32_t is_rx_nobuf;		/* rx failed for lack of buf */
	u_int32_t is_rx_decryptcrc;	/* rx decrypt failed on crc */
	u_int32_t is_rx_ahdemo_mgt;	/* rx discard ahdemo mgt frame*/
	u_int32_t is_rx_bad_auth;	/* rx bad auth request */
	u_int32_t is_rx_unauth;		/* rx on unauthorized port */
	u_int32_t is_rx_badkeyid;	/* rx w/ incorrect keyid */
	u_int32_t is_rx_ccmpreplay;	/* rx seq# violation (CCMP) */
	u_int32_t is_rx_ccmpformat;	/* rx format bad (CCMP) */
	u_int32_t is_rx_ccmpmic;		/* rx MIC check failed (CCMP) */
	u_int32_t is_rx_tkipreplay;	/* rx seq# violation (TKIP) */
	u_int32_t is_rx_tkipformat;	/* rx format bad (TKIP) */
	u_int32_t is_rx_tkipmic;		/* rx MIC check failed (TKIP) */
	u_int32_t is_rx_tkipicv;		/* rx ICV check failed (TKIP) */
	u_int32_t is_rx_badcipher;	/* rx failed due to of key type */
	u_int32_t is_rx_nocipherctx;	/* rx failed due to key !setup */
	u_int32_t is_rx_acl;		/* rx discard due to of acl policy */
	u_int32_t is_rx_ffcnt;		/* rx fast frames */
	u_int32_t is_rx_badathtnl;   	/* driver key alloc failed */
	u_int32_t is_tx_nobuf;		/* tx failed for lack of buf */
	u_int32_t is_tx_nonode;		/* tx failed for no node */
	u_int32_t is_tx_unknownmgt;	/* tx of unknown mgt frame */
	u_int32_t is_tx_badcipher;	/* tx failed due to of key type */
	u_int32_t is_tx_nodefkey;	/* tx failed due to no defkey */
	u_int32_t is_tx_noheadroom;	/* tx failed due to no space */
	u_int32_t is_tx_ffokcnt;		/* tx fast frames sent success */
	u_int32_t is_tx_fferrcnt;	/* tx fast frames sent success */
	u_int32_t is_tx_unauth;		/* tx on unauthorized port */
	u_int32_t is_scan_active;	/* active scans started */
	u_int32_t is_scan_passive;	/* passive scans started */
	u_int32_t is_node_timeout;	/* nodes timed out inactivity */
	u_int32_t is_crypto_nomem;	/* no memory for crypto ctx */
	u_int32_t is_crypto_tkip;	/* tkip crypto done in s/w */
	u_int32_t is_crypto_tkipenmic;	/* tkip en-MIC done in s/w */
	u_int32_t is_crypto_tkipdemic;	/* tkip de-MIC done in s/w */
	u_int32_t is_crypto_tkipcm;	/* tkip counter measures */
	u_int32_t is_crypto_ccmp;	/* ccmp crypto done in s/w */
	u_int32_t is_crypto_wep;		/* wep crypto done in s/w */
	u_int32_t is_crypto_setkey_cipher;/* cipher rejected key */
	u_int32_t is_crypto_setkey_nokey;/* no key index for setkey */
	u_int32_t is_crypto_delkey;	/* driver key delete failed */
	u_int32_t is_crypto_badcipher;	/* unknown cipher */
	u_int32_t is_crypto_nocipher;	/* cipher not available */
	u_int32_t is_crypto_attachfail;	/* cipher attach failed */
	u_int32_t is_crypto_swfallback;	/* cipher fallback to s/w */
	u_int32_t is_crypto_keyfail;	/* driver key alloc failed */
	u_int32_t is_crypto_enmicfail;	/* en-MIC failed */
	u_int32_t is_ibss_capmismatch;	/* merge failed-cap mismatch */
	u_int32_t is_ibss_norate;	/* merge failed-rate mismatch */
	u_int32_t is_ps_unassoc;	/* ps-poll for unassoc. sta */
	u_int32_t is_ps_badaid;		/* ps-poll w/ incorrect aid */
	u_int32_t is_ps_qempty;		/* ps-poll w/ nothing to send */
	u_int32_t is_rx_assoc_nohtcap;	/* HT capabilities mismatch */
	u_int32_t is_rx_assoc_tkiphtreject; /* rx assoc requesting TKIP and HT capabilities */
	u_int32_t is_rx_assoc_toomany;	/* reach assoc limit */
	uint32_t is_rx_ps_unauth;	/* ps-poll for un-authenticated STA */
};

/*
 * Max size of optional information elements.  We artificially
 * constrain this; it's limited only by the max frame size (and
 * the max parameter size of the wireless extensions).
 */
#define	IEEE80211_MAX_OPT_IE	256

/*
 * WPA/RSN get/set key request.  Specify the key/cipher
 * type and whether the key is to be used for sending and/or
 * receiving.  The key index should be set only when working
 * with global keys (use IEEE80211_KEYIX_NONE for ``no index'').
 * Otherwise a unicast/pairwise key is specified by the bssid
 * (on a station) or mac address (on an ap).  They key length
 * must include any MIC key data; otherwise it should be no
 more than IEEE80211_KEYBUF_SIZE.
 */
struct ieee80211req_key {
	u_int8_t ik_type;		/* key/cipher type */
	u_int8_t ik_pad;
	u_int8_t ik_keyix;		/* key index */
	u_int8_t ik_keylen;		/* key length in bytes */
	u_int8_t ik_flags;
/* NB: IEEE80211_KEY_XMIT and IEEE80211_KEY_RECV defined elsewhere */
#define	IEEE80211_KEY_DEFAULT	0x80	/* default xmit key */
	u_int8_t ik_macaddr[IEEE80211_ADDR_LEN];
	u_int64_t ik_keyrsc;		/* key receive sequence counter */
	u_int64_t ik_keytsc;		/* key transmit sequence counter */
	u_int8_t ik_keydata[IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE];
};

/*
 * Delete a key either by index or address.  Set the index
 * to IEEE80211_KEYIX_NONE when deleting a unicast key.
 */
struct ieee80211req_del_key {
	u_int8_t idk_keyix;		/* key index */
	u_int8_t idk_macaddr[IEEE80211_ADDR_LEN];
};

/*
 * MLME state manipulation request.  IEEE80211_MLME_ASSOC
 * only makes sense when operating as a station.  The other
 * requests can be used when operating as a station or an
 * ap (to effect a station).
 */
struct ieee80211req_mlme {
	u_int8_t im_op;			/* operation to perform */
#define	IEEE80211_MLME_ASSOC		1	/* associate station */
#define	IEEE80211_MLME_DISASSOC		2	/* disassociate station */
#define	IEEE80211_MLME_DEAUTH		3	/* deauthenticate station */
#define	IEEE80211_MLME_AUTHORIZE	4	/* authorize station */
#define	IEEE80211_MLME_UNAUTHORIZE	5	/* unauthorize station */
#define IEEE80211_MLME_CLEAR_STATS	6	/* clear station statistic */
#define IEEE80211_MLME_DEBUG_CLEAR	7	/* remove the STA without deauthing (DEBUG ONLY) */
	u_int8_t im_ssid_len;		/* length of optional ssid */
	u_int16_t im_reason;		/* 802.11 reason code */
	u_int8_t im_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t im_ssid[IEEE80211_NWID_LEN];
};

struct ieee80211req_brcm {
	u_int8_t ib_op;				/* operation to perform */
#define IEEE80211REQ_BRCM_INFO        0       /* BRCM client information */
#define IEEE80211REQ_BRCM_PKT         1       /* BRCM pkt from ap to client */
	u_int8_t ib_macaddr[IEEE80211_ADDR_LEN];
	int ib_rssi;
	uint32_t ib_rxglitch;
	uint8_t *ib_pkt;
	int32_t ib_pkt_len;
};

#define IEEE80211REQ_SCS_REPORT_CHAN_NUM    32
struct ieee80211req_scs_currchan_rpt {
	uint8_t iscr_curchan;
	uint16_t iscr_cca_try;
	uint16_t iscr_cca_idle;
	uint16_t iscr_cca_busy;
	uint16_t iscr_cca_intf;
	uint16_t iscr_cca_tx;
	uint16_t iscr_tx_ms;
	uint16_t iscr_rx_ms;
	uint32_t iscr_pmbl;
};
struct ieee80211req_scs_ranking_rpt_chan {
	uint8_t isrc_chan;
	uint8_t isrc_dfs;
	uint8_t isrc_txpwr;
	int32_t isrc_metric;
	/* scs part */
	uint16_t isrc_cca_intf;
	uint32_t isrc_pmbl_ap;
	uint32_t isrc_pmbl_sta;
	/* initial channel selection part */
	unsigned int isrc_numbeacons;
	int isrc_cci;
	int isrc_aci;
};
struct ieee80211req_scs_ranking_rpt {
	uint8_t isr_num;
	struct ieee80211req_scs_ranking_rpt_chan isr_chans[IEEE80211REQ_SCS_REPORT_CHAN_NUM];
};

#define SCS_MAX_TXTIME_COMP_INDEX	8
#define SCS_MAX_RXTIME_COMP_INDEX	8
/*
 * Restrictions:
 *   this structure must be kept in sync with ieee80211_scs
 */
enum qscs_cfg_param_e {
	SCS_SMPL_DWELL_TIME = 0,
	SCS_SAMPLE_INTV,
	SCS_THRSHLD_SMPL_PKTNUM,
	SCS_THRSHLD_SMPL_AIRTIME,
	SCS_THRSHLD_ATTEN_INC,
	SCS_THRSHLD_DFS_REENTRY,
	SCS_THRSHLD_DFS_REENTRY_MINRATE,
	SCS_THRSHLD_DFS_REENTRY_INTF,
	SCS_THRSHLD_LOADED,
	SCS_THRSHLD_AGING_NOR,
	SCS_THRSHLD_AGING_DFSREENT,
	SCS_ENABLE,
	SCS_DEBUG_ENABLE,
	SCS_SMPL_ENABLE,
	SCS_REPORT_ONLY,
	SCS_CCA_IDLE_THRSHLD,
	SCS_CCA_INTF_HI_THRSHLD,
	SCS_CCA_INTF_LO_THRSHLD,
	SCS_CCA_INTF_RATIO,
	SCS_CCA_SAMPLE_DUR,
	SCS_CCA_INTF_SMTH_NOXP,
	SCS_CCA_INTF_SMTH_XPED,
	SCS_RSSI_SMTH_UP,
	SCS_RSSI_SMTH_DOWN,
	SCS_CHAN_MTRC_MRGN,
	SCS_ATTEN_ADJUST,
	SCS_PMBL_ERR_SMTH_FCTR,
	SCS_PMBL_ERR_RANGE,
	SCS_PMBL_ERR_MAPPED_INTF_RANGE,
	SCS_SP_WF,
	SCS_LP_WF,
	SCS_PMP_RPT_CCA_SMTH_FCTR,
	SCS_PMP_RX_TIME_SMTH_FCTR,
	SCS_PMP_TX_TIME_SMTH_FCTR,
	SCS_PMP_STATS_STABLE_PERCENT,
	SCS_PMP_STATS_STABLE_RANGE,
	SCS_PMP_STATS_CLEAR_INTERVAL,
	SCS_AS_RX_TIME_SMTH_FCTR,
	SCS_AS_TX_TIME_SMTH_FCTR,
	SCS_TX_TIME_COMPENSTATION_START,
	SCS_TX_TIME_COMPENSTATION_END = SCS_TX_TIME_COMPENSTATION_START+SCS_MAX_TXTIME_COMP_INDEX-1,
	SCS_RX_TIME_COMPENSTATION_START,
	SCS_RX_TIME_COMPENSTATION_END = SCS_RX_TIME_COMPENSTATION_START+SCS_MAX_RXTIME_COMP_INDEX-1,
	SCS_PARAM_MAX,
};

struct ieee80211req_scs_param_rpt {
	uint32_t cfg_param;
	uint32_t signed_param_flag;
};

struct ieee80211req_scs {
	uint32_t is_op;
#define IEEE80211REQ_SCS_ID_UNKNOWN               0
#define IEEE80211REQ_SCS_FLAG_GET                 0x80000000
#define IEEE80211REQ_SCS_GET_CURRCHAN_RPT         (IEEE80211REQ_SCS_FLAG_GET | 1)
#define IEEE80211REQ_SCS_GET_INIT_RANKING_RPT     (IEEE80211REQ_SCS_FLAG_GET | 2)
#define IEEE80211REQ_SCS_GET_RANKING_RPT          (IEEE80211REQ_SCS_FLAG_GET | 3)
#define IEEE80211REQ_SCS_GET_PARAM_RPT            (IEEE80211REQ_SCS_FLAG_GET | 4)
	uint32_t *is_status;                  /* SCS specific reason for ioctl failure */
#define IEEE80211REQ_SCS_RESULT_OK                    0
#define IEEE80211REQ_SCS_RESULT_SYSCALL_ERR           1
#define IEEE80211REQ_SCS_RESULT_SCS_DISABLED          2
#define IEEE80211REQ_SCS_RESULT_NO_VAP_RUNNING        3
#define IEEE80211REQ_SCS_RESULT_NOT_EVALUATED         4        /* channel ranking not evaluated */
#define IEEE80211REQ_SCS_RESULT_TMP_UNAVAILABLE       5        /* when channel switch or param change */
#define IEEE80211REQ_SCS_RESULT_APMODE_ONLY           6
#define IEEE80211REQ_SCS_RESULT_AUTOCHAN_DISABLED     7
	uint8_t *is_data;
	int32_t is_data_len;
};

struct ieeee80211_dscp2ac {
#define IP_DSCP_NUM				64
	uint8_t dscp[IP_DSCP_NUM];
	uint8_t list_len;
	uint8_t ac;
};
/* 
 * MAC ACL operations.
 */
enum {
	IEEE80211_MACCMD_POLICY_OPEN	= 0,	/* set policy: no ACL's */
	IEEE80211_MACCMD_POLICY_ALLOW	= 1,	/* set policy: allow traffic */
	IEEE80211_MACCMD_POLICY_DENY	= 2,	/* set policy: deny traffic */
	IEEE80211_MACCMD_FLUSH		= 3,	/* flush ACL database */
	IEEE80211_MACCMD_DETACH		= 4,	/* detach ACL policy */
};

/*
 * Set the active channel list.  Note this list is
 * intersected with the available channel list in
 * calculating the set of channels actually used in
 * scanning.
 */
struct ieee80211req_chanlist {
	u_int8_t ic_channels[IEEE80211_CHAN_BYTES];
};

/*
 * Get the active channel list info.
 */
struct ieee80211req_chaninfo {
	u_int ic_nchans;
	struct ieee80211_channel ic_chans[IEEE80211_CHAN_MAX];
};

/*
 * Retrieve the WPA/RSN information element for an associated station.
 */
struct ieee80211req_wpaie {
	u_int8_t	wpa_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t	wpa_ie[IEEE80211_MAX_OPT_IE];
	u_int8_t	rsn_ie[IEEE80211_MAX_OPT_IE];
	u_int8_t	wps_ie[IEEE80211_MAX_OPT_IE];
	u_int8_t	qtn_pairing_ie[IEEE80211_MAX_OPT_IE];
#define QTN_PAIRING_IE_EXIST 1
#define QTN_PAIRING_IE_ABSENT 0
	u_int8_t	has_pairing_ie;		/* Indicates whether Pairing IE exists in assoc req/resp */
};

/*
 * Retrieve per-node statistics.
 */
struct ieee80211req_sta_stats {
	union {
		/* NB: explicitly force 64-bit alignment */
		u_int8_t macaddr[IEEE80211_ADDR_LEN];
		u_int64_t pad;
	} is_u;
	struct ieee80211_nodestats is_stats;
};
/*
 * Retrieve STA Statistics(Radio measurement) information element for an associated station.
 */
struct ieee80211req_qtn_rmt_sta_stats {
	int status;
	struct ieee80211_ie_qtn_rm_sta_all rmt_sta_stats;
	struct ieee80211_ie_rm_sta_grp221	rmt_sta_stats_grp221;
};

struct ieee80211req_qtn_rmt_sta_stats_setpara {
	u_int32_t flags;
	u_int8_t macaddr[IEEE80211_ADDR_LEN];
};

struct ieee80211req_node_meas {
	uint8_t mac_addr[6];

	uint8_t type;
#define IOCTL_MEAS_TYPE_BASIC		0x0
#define IOCTL_MEAS_TYPE_CCA		0x1
#define IOCTL_MEAS_TYPE_RPI		0x2
#define IOCTL_MEAS_TYPE_CHAN_LOAD	0x3
#define IOCTL_MEAS_TYPE_NOISE_HIS	0x4
#define IOCTL_MEAS_TYPE_BEACON		0x5
#define IOCTL_MEAS_TYPE_FRAME		0x6
#define IOCTL_MEAS_TYPE_CAT		0x7
#define IOCTL_MEAS_TYPE_MUL_DIAG	0x8
#define IOCTL_MEAS_TYPE_LINK		0x9
#define IOCTL_MEAS_TYPE_NEIGHBOR	0xA

	struct _ioctl_basic {
		uint16_t start_offset_ms;
		uint16_t duration_ms;
		uint8_t channel;
	} ioctl_basic;
	struct _ioctl_cca {
		uint16_t start_offset_ms;
		uint16_t duration_ms;
		uint8_t channel;
	} ioctl_cca;
	struct _ioctl_rpi {
		uint16_t start_offset_ms;
		uint16_t duration_ms;
		uint8_t channel;
	} ioctl_rpi;
	struct _ioctl_chan_load {
		uint16_t duration_ms;
		uint8_t channel;
	} ioctl_chan_load;
	struct _ioctl_noise_his {
		uint16_t duration_ms;
		uint8_t channel;
	} ioctl_noise_his;
	struct _ioctl_beacon {
		uint8_t op_class;
		uint8_t channel;
		uint16_t duration_ms;
		uint8_t mode;
		uint8_t bssid[IEEE80211_ADDR_LEN];
	} ioctl_beacon;
	struct _ioctl_frame {
		uint8_t op_class;
		uint8_t channel;
		uint16_t duration_ms;
		uint8_t type;
		uint8_t mac_address[IEEE80211_ADDR_LEN];
	} ioctl_frame;
	struct _ioctl_tran_stream_cat {
		uint16_t duration_ms;
		uint8_t peer_sta[IEEE80211_ADDR_LEN];
		uint8_t tid;
		uint8_t bin0;
	} ioctl_tran_stream_cat;
	struct _ioctl_multicast_diag {
		uint16_t duration_ms;
		uint8_t group_mac[IEEE80211_ADDR_LEN];
	} ioctl_multicast_diag;
};

struct ieee80211req_node_tpc {
	u_int8_t	mac_addr[6];
};

struct ieee80211req_node_info {
	u_int8_t	req_type;
#define IOCTL_REQ_MEASUREMENT	0x0
#define IOCTL_REQ_TPC		0x1
	union {
		struct ieee80211req_node_meas	req_node_meas;
		struct ieee80211req_node_tpc	req_node_tpc;
	} u_req_info;
};

struct ieee80211_ioctl_neighbor_report_item {
	uint8_t bssid[IEEE80211_ADDR_LEN];
	uint32_t bssid_info;
	uint8_t operating_class;
	uint8_t channel;
	uint8_t phy_type;
};
#define IEEE80211_MAX_NEIGHBOR_REPORT_ITEM 3

struct ieee80211rep_node_meas_result {
	uint8_t	status;
#define IOCTL_MEAS_STATUS_SUCC		0
#define IOCTL_MEAS_STATUS_TIMEOUT	1
#define IOCTL_MEAS_STATUS_NODELEAVE	2
#define IOCTL_MEAS_STATUS_STOP		3

	uint8_t report_mode;
#define IOCTL_MEAS_REP_OK	(0)
#define IOCTL_MEAS_REP_LATE	(1 << 0)
#define IOCTL_MEAS_REP_INCAP	(1 << 1)
#define IOCTL_MEAS_REP_REFUSE	(1 << 2)
#define IOCTL_MEAS_REP_MASK	(0x07)

	union {
		uint8_t	basic;
		uint8_t	cca;
		uint8_t	rpi[8];
		uint8_t chan_load;
		struct {
			uint8_t antenna_id;
			uint8_t anpi;
			uint8_t ipi[11];
		} noise_his;
		struct {
			uint8_t reported_frame_info;
			uint8_t rcpi;
			uint8_t rsni;
			uint8_t bssid[IEEE80211_ADDR_LEN];
			uint8_t antenna_id;
			uint32_t parent_tsf;
		} beacon;
		struct {
			uint32_t sub_ele_report;
			uint8_t ta[IEEE80211_ADDR_LEN];
			uint8_t bssid[IEEE80211_ADDR_LEN];
			uint8_t phy_type;
			uint8_t avg_rcpi;
			uint8_t last_rsni;
			uint8_t last_rcpi;
			uint8_t antenna_id;
			uint16_t frame_count;
		} frame;
		struct {
			uint8_t reason;
			uint32_t tran_msdu_cnt;
			uint32_t msdu_discard_cnt;
			uint32_t msdu_fail_cnt;
			uint32_t msdu_mul_retry_cnt;
			uint32_t qos_lost_cnt;
			uint32_t avg_queue_delay;
			uint32_t avg_tran_delay;
			uint8_t bin0_range;
			uint32_t bins[6];
		} tran_stream_cat;
		struct {
			uint8_t reason;
			uint32_t mul_rec_msdu_cnt;
			uint16_t first_seq_num;
			uint16_t last_seq_num;
			uint16_t mul_rate;
		} multicast_diag;
		struct {
			struct {
				int8_t tx_power;
				int8_t link_margin;
			} tpc_report;
			uint8_t recv_antenna_id;
			uint8_t tran_antenna_id;
			uint8_t rcpi;
			uint8_t rsni;
		} link_measure;
		struct {
			uint8_t item_num;
			struct ieee80211_ioctl_neighbor_report_item item[IEEE80211_MAX_NEIGHBOR_REPORT_ITEM];
		} neighbor_report;
	} u_data;
};

struct ieee80211rep_node_tpc_result {
	uint8_t status;
	int8_t	tx_power;
	int8_t	link_margin;
};

union ieee80211rep_node_info {
	struct ieee80211rep_node_meas_result	meas_result;
	struct ieee80211rep_node_tpc_result	tpc_result;
};

/*
 * Station information block; the mac address is used
 * to retrieve other data like stats, unicast key, etc.
 */
struct ieee80211req_sta_info {
	u_int16_t isi_len;		/* length (mult of 4) */
	u_int16_t isi_freq;		/* MHz */
	u_int16_t isi_flags;		/* channel flags */
	u_int16_t isi_state;		/* state flags */
	u_int8_t isi_authmode;		/* authentication algorithm */
	u_int8_t isi_rssi;
	u_int16_t isi_capinfo;		/* capabilities */
	u_int8_t isi_athflags;		/* Atheros capabilities */
	u_int8_t isi_erp;		/* ERP element */
	u_int8_t isi_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t isi_nrates;		/* negotiated rates */
	u_int8_t isi_rates[IEEE80211_RATE_MAXSIZE];
	u_int8_t isi_txrate;		/* index to isi_rates[] */
	u_int16_t isi_ie_len;		/* IE length */
	u_int16_t isi_associd;		/* assoc response */
	u_int16_t isi_txpower;		/* current tx power */
	u_int16_t isi_vlan;		/* vlan tag */
	u_int16_t isi_txseqs[17];	/* seq to be transmitted */
	u_int16_t isi_rxseqs[17];	/* seq previous for qos frames*/
	u_int16_t isi_inact;		/* inactivity timer */
	u_int8_t isi_uapsd;		/* UAPSD queues */
	u_int8_t isi_opmode;		/* sta operating mode */
	u_int16_t isi_htcap;		/* HT capabilities */

	/* XXX frag state? */
	/* variable length IE data */
};

enum {
	IEEE80211_STA_OPMODE_NORMAL,
	IEEE80211_STA_OPMODE_XR
};

/*
 * Retrieve per-station information; to retrieve all
 * specify a mac address of ff:ff:ff:ff:ff:ff.
 */
struct ieee80211req_sta_req {
	union {
		/* NB: explicitly force 64-bit alignment */
		u_int8_t macaddr[IEEE80211_ADDR_LEN];
		u_int64_t pad;
	} is_u;
	struct ieee80211req_sta_info info[1];	/* variable length */
};

/*
 * Get/set per-station tx power cap.
 */
struct ieee80211req_sta_txpow {
	u_int8_t	it_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t	it_txpow;
};

/*
 * WME parameters are set and return using i_val and i_len.
 * i_val holds the value itself.  i_len specifies the AC
 * and, as appropriate, then high bit specifies whether the
 * operation is to be applied to the BSS or ourself.
 */
#define	IEEE80211_WMEPARAM_SELF	0x0000		/* parameter applies to self */
#define	IEEE80211_WMEPARAM_BSS	0x8000		/* parameter applies to BSS */
#define	IEEE80211_WMEPARAM_VAL	0x7fff		/* parameter value */

/*
 * Scan result data returned for IEEE80211_IOC_SCAN_RESULTS.
 */
struct ieee80211req_scan_result {
	u_int16_t isr_len;		/* length (mult of 4) */
	u_int16_t isr_freq;		/* MHz */
	u_int16_t isr_flags;		/* channel flags */
	u_int8_t isr_noise;
	u_int8_t isr_rssi;
	u_int8_t isr_intval;		/* beacon interval */
	u_int16_t isr_capinfo;		/* capabilities */
	u_int8_t isr_erp;		/* ERP element */
	u_int8_t isr_bssid[IEEE80211_ADDR_LEN];
	u_int8_t isr_nrates;
	u_int8_t isr_rates[IEEE80211_RATE_MAXSIZE];
	u_int8_t isr_ssid_len;		/* SSID length */
	u_int8_t isr_ie_len;		/* IE length */
	u_int8_t isr_pad[5];
	/* variable length SSID followed by IE data */
};

#define IEEE80211_MAX_ASSOC_HISTORY	32

struct ieee80211_assoc_history {
	uint8_t  ah_macaddr_table[IEEE80211_MAX_ASSOC_HISTORY][IEEE80211_ADDR_LEN];
	uint32_t ah_timestamp[IEEE80211_MAX_ASSOC_HISTORY];
};

/*
 * Channel switch history record.
 */
#define CSW_MAX_RECORDS_MAX 32
struct ieee80211req_csw_record {
	uint32_t cnt;
	int32_t index;
	uint32_t channel[CSW_MAX_RECORDS_MAX];
	uint32_t timestamp[CSW_MAX_RECORDS_MAX];
};

struct ieee80211req_radar_status {
	uint32_t channel;
	uint32_t flags;
	uint32_t ic_radardetected;
};

struct ieee80211req_disconn_info {
	uint32_t asso_sta_count;
	uint32_t disconn_count;
	uint32_t sequence;
	uint32_t up_time;
	uint32_t resetflag;
};

#define AP_SCAN_MAX_NUM_RATES 32
/* for qcsapi_get_results_AP_scan */
struct ieee80211_general_ap_scan_result {
	int32_t num_bitrates;
	int32_t bitrates[AP_SCAN_MAX_NUM_RATES];
	int32_t num_ap_results;
};

struct ieee80211_per_ap_scan_result {
	int8_t		ap_addr_mac[IEEE80211_ADDR_LEN];
	int8_t		ap_name_ssid[32 + 1];
	int32_t		ap_channel_ieee;
	int32_t		ap_rssi;
	int32_t		ap_flags;
	int32_t		ap_htcap;
	int32_t		ap_vhtcap;
	int32_t		ap_num_genies;
	int8_t		ap_ie_buf[0];	/* just to remind there might be WPA/RSN/WSC IEs right behind*/
};

#ifdef __FreeBSD__
/*
 * FreeBSD-style ioctls.
 */
/* the first member must be matched with struct ifreq */
struct ieee80211req {
	char i_name[IFNAMSIZ];	/* if_name, e.g. "wi0" */
	u_int16_t i_type;	/* req type */
	int16_t 	i_val;		/* Index or simple value */
	int16_t 	i_len;		/* Index or simple value */
	void *i_data;		/* Extra data */
};
#define	SIOCS80211		 _IOW('i', 234, struct ieee80211req)
#define	SIOCG80211		_IOWR('i', 235, struct ieee80211req)
#define	SIOCG80211STATS		_IOWR('i', 236, struct ifreq)
#define	SIOC80211IFCREATE	_IOWR('i', 237, struct ifreq)
#define	SIOC80211IFDESTROY	 _IOW('i', 238, struct ifreq)

#define IEEE80211_IOC_SSID		1
#define IEEE80211_IOC_NUMSSIDS		2
#define IEEE80211_IOC_WEP		3
#define 	IEEE80211_WEP_NOSUP		-1
#define 	IEEE80211_WEP_OFF		0
#define 	IEEE80211_WEP_ON		1
#define 	IEEE80211_WEP_MIXED		2
#define IEEE80211_IOC_WEPKEY		4
#define IEEE80211_IOC_NUMWEPKEYS	5
#define IEEE80211_IOC_WEPTXKEY		6
#define IEEE80211_IOC_AUTHMODE		7
#define IEEE80211_IOC_STATIONNAME	8
#define IEEE80211_IOC_CHANNEL		9
#define IEEE80211_IOC_POWERSAVE		10
#define 	IEEE80211_POWERSAVE_NOSUP	-1
#define 	IEEE80211_POWERSAVE_OFF		0
#define 	IEEE80211_POWERSAVE_CAM		1
#define 	IEEE80211_POWERSAVE_PSP		2
#define 	IEEE80211_POWERSAVE_PSP_CAM	3
#define 	IEEE80211_POWERSAVE_ON		IEEE80211_POWERSAVE_CAM
#define IEEE80211_IOC_POWERSAVESLEEP	11
#define	IEEE80211_IOC_RTSTHRESHOLD	12
#define IEEE80211_IOC_PROTMODE		13
#define 	IEEE80211_PROTMODE_OFF		0
#define 	IEEE80211_PROTMODE_CTS		1
#define 	IEEE80211_PROTMODE_RTSCTS	2
#define	IEEE80211_IOC_TXPOWER		14	/* global tx power limit */
#define	IEEE80211_IOC_BSSID		15
#define	IEEE80211_IOC_ROAMING		16	/* roaming mode */
#define	IEEE80211_IOC_PRIVACY		17	/* privacy invoked */
#define	IEEE80211_IOC_DROPUNENCRYPTED	18	/* discard unencrypted frames */
#define	IEEE80211_IOC_WPAKEY		19
#define	IEEE80211_IOC_DELKEY		20
#define	IEEE80211_IOC_MLME		21
#define	IEEE80211_IOC_OPTIE		22	/* optional info. element */
#define	IEEE80211_IOC_SCAN_REQ		23
#define	IEEE80211_IOC_SCAN_RESULTS	24
#define	IEEE80211_IOC_COUNTERMEASURES	25	/* WPA/TKIP countermeasures */
#define	IEEE80211_IOC_WPA		26	/* WPA mode (0,1,2) */
#define	IEEE80211_IOC_CHANLIST		27	/* channel list */
#define	IEEE80211_IOC_WME		28	/* WME mode (on, off) */
#define	IEEE80211_IOC_HIDESSID		29	/* hide SSID mode (on, off) */
#define IEEE80211_IOC_APBRIDGE		30	/* AP inter-sta bridging */
#define	IEEE80211_IOC_MCASTCIPHER	31	/* multicast/default cipher */
#define	IEEE80211_IOC_MCASTKEYLEN	32	/* multicast key length */
#define	IEEE80211_IOC_UCASTCIPHERS	33	/* unicast cipher suites */
#define	IEEE80211_IOC_UCASTCIPHER	34	/* unicast cipher */
#define	IEEE80211_IOC_UCASTKEYLEN	35	/* unicast key length */
#define	IEEE80211_IOC_DRIVER_CAPS	36	/* driver capabilities */
#define	IEEE80211_IOC_KEYMGTALGS	37	/* key management algorithms */
#define	IEEE80211_IOC_RSNCAPS		38	/* RSN capabilities */
#define	IEEE80211_IOC_WPAIE		39	/* WPA information element */
#define	IEEE80211_IOC_STA_STATS		40	/* per-station statistics */
#define	IEEE80211_IOC_MACCMD		41	/* MAC ACL operation */
#define	IEEE80211_IOC_TXPOWMAX		43	/* max tx power for channel */
#define	IEEE80211_IOC_STA_TXPOW		44	/* per-station tx power limit */
#define	IEEE80211_IOC_STA_INFO		45	/* station/neighbor info */
#define	IEEE80211_IOC_WME_CWMIN		46	/* WME: ECWmin */
#define	IEEE80211_IOC_WME_CWMAX		47	/* WME: ECWmax */
#define	IEEE80211_IOC_WME_AIFS		48	/* WME: AIFSN */
#define	IEEE80211_IOC_WME_TXOPLIMIT	49	/* WME: txops limit */
#define	IEEE80211_IOC_WME_ACM		50	/* WME: ACM (bss only) */
#define	IEEE80211_IOC_WME_ACKPOLICY	51	/* WME: ACK policy (!bss only)*/
#define	IEEE80211_IOC_DTIM_PERIOD	52	/* DTIM period (beacons) */
#define	IEEE80211_IOC_BEACON_INTERVAL	53	/* beacon interval (ms) */
#define	IEEE80211_IOC_ADDMAC		54	/* add sta to MAC ACL table */
#define	IEEE80211_IOC_DELMAC		55	/* del sta from MAC ACL table */
#define	IEEE80211_IOC_FF		56	/* ATH fast frames (on, off) */
#define	IEEE80211_IOC_TURBOP		57	/* ATH turbo' (on, off) */
#define	IEEE80211_IOC_APPIEBUF		58	/* IE in the management frame */
#define	IEEE80211_IOC_FILTERFRAME	59	/* management frame filter */

/*
 * Scan result data returned for IEEE80211_IOC_SCAN_RESULTS.
 */
struct ieee80211req_scan_result {
	u_int16_t isr_len;		/* length (mult of 4) */
	u_int16_t isr_freq;		/* MHz */
	u_int16_t isr_flags;		/* channel flags */
	u_int8_t isr_noise;
	u_int8_t isr_rssi;
	u_int8_t isr_intval;		/* beacon interval */
	u_int16_t isr_capinfo;		/* capabilities */
	u_int8_t isr_erp;		/* ERP element */
	u_int8_t isr_bssid[IEEE80211_ADDR_LEN];
	u_int8_t isr_nrates;
	u_int8_t isr_rates[IEEE80211_RATE_MAXSIZE];
	u_int8_t isr_ssid_len;		/* SSID length */
	u_int8_t isr_ie_len;		/* IE length */
	u_int8_t isr_pad[5];
	/* variable length SSID followed by IE data */
};

#endif /* __FreeBSD__ */

#if defined(__linux__) || defined(MUC_BUILD) || defined(DSP_BUILD)
/*
 * Wireless Extensions API, private ioctl interfaces.
 *
 * NB: Even-numbered ioctl numbers have set semantics and are privileged!
 *     (regardless of the incorrect comment in wireless.h!)
 */
#ifdef __KERNEL__
#include <linux/if.h>
#endif
#define	IEEE80211_IOCTL_SETPARAM	(SIOCIWFIRSTPRIV+0)
#define	IEEE80211_IOCTL_GETPARAM	(SIOCIWFIRSTPRIV+1)
#define	IEEE80211_IOCTL_SETMODE		(SIOCIWFIRSTPRIV+2)
#define	IEEE80211_IOCTL_GETMODE		(SIOCIWFIRSTPRIV+3)
#define	IEEE80211_IOCTL_SETWMMPARAMS	(SIOCIWFIRSTPRIV+4)
#define	IEEE80211_IOCTL_GETWMMPARAMS	(SIOCIWFIRSTPRIV+5)
#define	IEEE80211_IOCTL_SETCHANLIST	(SIOCIWFIRSTPRIV+6)
#define	IEEE80211_IOCTL_GETCHANLIST	(SIOCIWFIRSTPRIV+7)
#define	IEEE80211_IOCTL_CHANSWITCH	(SIOCIWFIRSTPRIV+8)
#define	IEEE80211_IOCTL_GET_APPIEBUF	(SIOCIWFIRSTPRIV+9)
#define	IEEE80211_IOCTL_SET_APPIEBUF	(SIOCIWFIRSTPRIV+10)
#define	IEEE80211_IOCTL_FILTERFRAME	(SIOCIWFIRSTPRIV+12)
#define	IEEE80211_IOCTL_GETCHANINFO	(SIOCIWFIRSTPRIV+13)
#define	IEEE80211_IOCTL_SETOPTIE	(SIOCIWFIRSTPRIV+14)
#define	IEEE80211_IOCTL_GETOPTIE	(SIOCIWFIRSTPRIV+15)
#define	IEEE80211_IOCTL_SETMLME		(SIOCIWFIRSTPRIV+16)
#define	IEEE80211_IOCTL_RADAR		(SIOCIWFIRSTPRIV+17)
#define	IEEE80211_IOCTL_SETKEY		(SIOCIWFIRSTPRIV+18)
#define	IEEE80211_IOCTL_POSTEVENT	(SIOCIWFIRSTPRIV+19)
#define	IEEE80211_IOCTL_DELKEY		(SIOCIWFIRSTPRIV+20)
#define	IEEE80211_IOCTL_TXEAPOL		(SIOCIWFIRSTPRIV+21)
#define	IEEE80211_IOCTL_ADDMAC		(SIOCIWFIRSTPRIV+22)
#define	IEEE80211_IOCTL_STARTCCA	(SIOCIWFIRSTPRIV+23)
#define	IEEE80211_IOCTL_DELMAC		(SIOCIWFIRSTPRIV+24)
#define IEEE80211_IOCTL_GETSTASTATISTIC	(SIOCIWFIRSTPRIV+25)
#define	IEEE80211_IOCTL_WDSADDMAC	(SIOCIWFIRSTPRIV+26)
#define	IEEE80211_IOCTL_WDSDELMAC	(SIOCIWFIRSTPRIV+28)
#define IEEE80211_IOCTL_GETBLOCK	(SIOCIWFIRSTPRIV+29)
#define	IEEE80211_IOCTL_KICKMAC		(SIOCIWFIRSTPRIV+30)
#define	IEEE80211_IOCTL_DFSACTSCAN	(SIOCIWFIRSTPRIV+31)

#define IEEE80211_AMPDU_MIN_DENSITY	0
#define IEEE80211_AMPDU_MAX_DENSITY	7

#define IEEE80211_CCE_PREV_CHAN_SHIFT	8

enum {
	IEEE80211_PARAM_TURBO		= 1,	/* turbo mode */
	IEEE80211_PARAM_MODE		= 2,	/* phy mode (11a, 11b, etc.) */
	IEEE80211_PARAM_AUTHMODE	= 3,	/* authentication mode */
	IEEE80211_PARAM_PROTMODE	= 4,	/* 802.11g protection */
	IEEE80211_PARAM_MCASTCIPHER	= 5,	/* multicast/default cipher */
	IEEE80211_PARAM_MCASTKEYLEN	= 6,	/* multicast key length */
	IEEE80211_PARAM_UCASTCIPHERS	= 7,	/* unicast cipher suites */
	IEEE80211_PARAM_UCASTCIPHER	= 8,	/* unicast cipher */
	IEEE80211_PARAM_UCASTKEYLEN	= 9,	/* unicast key length */
	IEEE80211_PARAM_WPA		= 10,	/* WPA mode (0,1,2) */
	IEEE80211_PARAM_ROAMING		= 12,	/* roaming mode */
	IEEE80211_PARAM_PRIVACY		= 13,	/* privacy invoked */
	IEEE80211_PARAM_COUNTERMEASURES	= 14,	/* WPA/TKIP countermeasures */
	IEEE80211_PARAM_DROPUNENCRYPTED	= 15,	/* discard unencrypted frames */
	IEEE80211_PARAM_DRIVER_CAPS	= 16,	/* driver capabilities */
	IEEE80211_PARAM_MACCMD		= 17,	/* MAC ACL operation */
	IEEE80211_PARAM_WMM		= 18,	/* WMM mode (on, off) */
	IEEE80211_PARAM_HIDESSID	= 19,	/* hide SSID mode (on, off) */
	IEEE80211_PARAM_APBRIDGE    	= 20,   /* AP inter-sta bridging */
	IEEE80211_PARAM_KEYMGTALGS	= 21,	/* key management algorithms */
	IEEE80211_PARAM_RSNCAPS		= 22,	/* RSN capabilities */
	IEEE80211_PARAM_INACT		= 23,	/* station inactivity timeout */
	IEEE80211_PARAM_INACT_AUTH	= 24,	/* station auth inact timeout */
	IEEE80211_PARAM_INACT_INIT	= 25,	/* station init inact timeout */
	IEEE80211_PARAM_ABOLT		= 26,	/* Atheros Adv. Capabilities */
	IEEE80211_PARAM_DTIM_PERIOD	= 28,	/* DTIM period (beacons) */
	IEEE80211_PARAM_BEACON_INTERVAL	= 29,	/* beacon interval (ms) */
	IEEE80211_PARAM_DOTH		= 30,	/* 11.h is on/off */
	IEEE80211_PARAM_PWRCONSTRAINT	= 31,	/* Current Channel Pwr Constraint */
	IEEE80211_PARAM_GENREASSOC	= 32,	/* Generate a reassociation request */
	IEEE80211_PARAM_COMPRESSION	= 33,	/* compression */
	IEEE80211_PARAM_FF		= 34,	/* fast frames support  */
	IEEE80211_PARAM_XR		= 35,	/* XR support */
	IEEE80211_PARAM_BURST		= 36,	/* burst mode */
	IEEE80211_PARAM_PUREG		= 37,	/* pure 11g (no 11b stations) */
	IEEE80211_PARAM_AR		= 38,	/* AR support */
	IEEE80211_PARAM_WDS		= 39,	/* Enable 4 address processing */
	IEEE80211_PARAM_BGSCAN		= 40,	/* bg scanning (on, off) */
	IEEE80211_PARAM_BGSCAN_IDLE	= 41,	/* bg scan idle threshold */
	IEEE80211_PARAM_BGSCAN_INTERVAL	= 42,	/* bg scan interval */
	IEEE80211_PARAM_MCAST_RATE	= 43,	/* Multicast Tx Rate */
	IEEE80211_PARAM_COVERAGE_CLASS	= 44,	/* coverage class */
	IEEE80211_PARAM_COUNTRY_IE	= 45,	/* enable country IE */
	IEEE80211_PARAM_SCANVALID	= 46,	/* scan cache valid threshold */
	IEEE80211_PARAM_ROAM_RSSI_11A	= 47,	/* rssi threshold in 11a */
	IEEE80211_PARAM_ROAM_RSSI_11B	= 48,	/* rssi threshold in 11b */
	IEEE80211_PARAM_ROAM_RSSI_11G	= 49,	/* rssi threshold in 11g */
	IEEE80211_PARAM_ROAM_RATE_11A	= 50,	/* tx rate threshold in 11a */
	IEEE80211_PARAM_ROAM_RATE_11B	= 51,	/* tx rate threshold in 11b */
	IEEE80211_PARAM_ROAM_RATE_11G	= 52,	/* tx rate threshold in 11g */
	IEEE80211_PARAM_UAPSDINFO	= 53,	/* value for qos info field */
	IEEE80211_PARAM_SLEEP		= 54,	/* force sleep/wake */
	IEEE80211_PARAM_QOSNULL		= 55,	/* force sleep/wake */
	IEEE80211_PARAM_PSPOLL		= 56,	/* force ps-poll generation (sta only) */
	IEEE80211_PARAM_EOSPDROP	= 57,	/* force uapsd EOSP drop (ap only) */
	IEEE80211_PARAM_MARKDFS		= 58,	/* mark a dfs interference channel when found */
	IEEE80211_PARAM_REGCLASS	= 59,	/* enable regclass ids in country IE */
	IEEE80211_PARAM_DROPUNENC_EAPOL	= 60,	/* drop unencrypted eapol frames */
 	IEEE80211_PARAM_SHPREAMBLE	= 61,	/* Short Preamble */
 	IEEE80211_PARAM_FIXED_TX_RATE = 62,	/* Set fixed TX rate          */
 	IEEE80211_PARAM_MIMOMODE = 63,		/* Select antenna to use      */
 	IEEE80211_PARAM_AGGREGATION	= 64,	/* Enable/disable aggregation */
	IEEE80211_PARAM_RETRY_COUNT = 65,	/* Set retry count            */
	IEEE80211_PARAM_VAP_DBG    = 66,		/* Set the VAP debug verbosity . */
	IEEE80211_PARAM_VCO_CALIB = 67,		/* Set VCO calibration */
	IEEE80211_PARAM_EXP_MAT_SEL = 68,	/* Select different exp mat */ 
	IEEE80211_PARAM_BW_SEL = 69,		/* Select BW */ 
	IEEE80211_PARAM_RG = 70,			/* Let software fill in the duration update*/ 
	IEEE80211_PARAM_BW_SEL_MUC = 71,	/* Let software fill in the duration update*/ 
	IEEE80211_PARAM_ACK_POLICY = 72,   	/* 1 for ACK, zero for no ACK */
	IEEE80211_PARAM_LEGACY_MODE = 73,  	/* 1 for legacy, zero for HT*/
	IEEE80211_PARAM_MAX_AGG_SUBFRM = 74,   	/* Maximum number if subframes to allow for aggregation */
	IEEE80211_PARAM_ADD_WDS_MAC = 75,  	/* Add MAC address for WDS peer */
	IEEE80211_PARAM_DEL_WDS_MAC = 76,  	/* Delete MAC address for WDS peer */
	IEEE80211_PARAM_TXBF_CTRL = 77,   	/* Control TX beamforming */
	IEEE80211_PARAM_TXBF_PERIOD = 78,  	/* Set TX beamforming period */
	IEEE80211_PARAM_BSSID = 79,  		/* Set BSSID */
	IEEE80211_PARAM_HTBA_SEQ_CTRL = 80, /* Control HT Block ACK */
	IEEE80211_PARAM_HTBA_SIZE_CTRL = 81, /* Control HT Block ACK */
	IEEE80211_PARAM_HTBA_TIME_CTRL = 82, /* Control HT Block ACK */
	IEEE80211_PARAM_HT_ADDBA = 83,   	/* ADDBA control */
	IEEE80211_PARAM_HT_DELBA = 84,  	/* DELBA control */
	IEEE80211_PARAM_CHANNEL_NOSCAN = 85, /* Disable the scanning for fixed channels */
	IEEE80211_PARAM_MUC_PROFILE = 86,  	/* Control MuC profiling */
	IEEE80211_PARAM_MUC_PHY_STATS = 87,  	/* Control MuC phy stats */
	IEEE80211_PARAM_MUC_SET_PARTNUM = 88,  	/* set muc part num for cal */
	IEEE80211_PARAM_ENABLE_GAIN_ADAPT = 89,  	/* turn on the anlg gain tuning */
	IEEE80211_PARAM_GET_RFCHIP_ID = 90,  	/* Get RF chip frequency id */
	IEEE80211_PARAM_GET_RFCHIP_VERID = 91,  	/* Get RF chip version id */
	IEEE80211_PARAM_ADD_WDS_MAC_DOWN = 92,  	/* Add MAC address for WDS downlink peer */
	IEEE80211_PARAM_SHORT_GI = 93,			/* Set to 1 for turning on SGI */
	IEEE80211_PARAM_LINK_LOSS = 94,			/* Set to 1 for turning on Link Loss feature */
	IEEE80211_PARAM_BCN_MISS_THR = 95,			/* Set to 0 for default value (50 Beacons). */
	IEEE80211_PARAM_FORCE_SMPS = 96,		/* Force the SMPS mode to transition the mode (STA) - includes
							 * sending out the ACTION frame to the AP.
							 */
	IEEE80211_PARAM_FORCEMICERROR = 97,	/* Force a MIC error - does loopback through the MUC back up to QDRV thence
						 * through the normal TKIP MIC error path.
						 */
	IEEE80211_PARAM_ENABLECOUNTERMEASURES = 98, /* Enable/disable countermeasures */
	IEEE80211_PARAM_IMPLICITBA = 99,	/* Set the implicit BA flags in the QIE */
	IEEE80211_PARAM_CLIENT_REMOVE = 100,	/* Remove clients but DON'T deauth them */
	IEEE80211_PARAM_SHOWMEM = 101,		/* If debug build for MALLOC/FREE, show the summary view */
	IEEE80211_PARAM_SCANSTATUS = 102,	/* Get scanning state */
	IEEE80211_PARAM_GLOBAL_BA_CONTROL = 103, /* Set the global BA flags */
	IEEE80211_PARAM_NO_SSID_ASSOC = 104,	/* Enable/disable associations without SSIDs */
	IEEE80211_PARAM_FIXED_SGI = 105,	/* Choose between node based SGI or fixed SGI */
	IEEE80211_PARAM_CONFIG_TXPOWER = 106,	/* configure TX power for a band (start chan to stop chan) */	
	IEEE80211_PARAM_SKB_LIST_MAX = 107,	/* Configure the max len of the skb list shared b/n drivers */
	IEEE80211_PARAM_VAP_STATS = 108,		/* Show VAP stats */
	IEEE80211_PARAM_RATE_CTRL_FLAGS = 109,  /* Configure flags to tweak rate control algorithm */
	IEEE80211_PARAM_LDPC = 110, /* Enabling/disabling LDPC */
	IEEE80211_PARAM_DFS_FAST_SWITCH = 111,  /* On detection of radar, select a non-DFS channel and switch immediately */
	IEEE80211_PARAM_11N_40_ONLY_MODE = 112, /* Support for 11n 40MHZ only mode */
	IEEE80211_PARAM_AMPDU_DENSITY = 113,	/* AMPDU DENSITY CONTROL */
	IEEE80211_PARAM_SCAN_NO_DFS = 114,	/* On detection of radar, avoid DFS channels; AP only */
	IEEE80211_PARAM_REGULATORY_REGION = 115, /* set the regulatory region */
	IEEE80211_PARAM_CONFIG_BB_INTR_DO_SRESET = 116, /* enable or disable sw reset for BB interrupt */
	IEEE80211_PARAM_CONFIG_MAC_INTR_DO_SRESET = 117, /* enable or disable sw reset for MAC interrupt */
	IEEE80211_PARAM_CONFIG_WDG_DO_SRESET = 118, /* enable or disable sw reset triggered by watchdog */
	IEEE80211_PARAM_TRIGGER_RESET = 119,	/* trigger reset for MAC/BB */
	IEEE80211_PARAM_INJECT_INVALID_FCS = 120, /* inject bad FCS to induce tx hang */
	IEEE80211_PARAM_CONFIG_WDG_SENSITIVITY = 121, /* higher value means less sensitive */
	IEEE80211_PARAM_SAMPLE_RATE = 122,	/* Set data sampling rate */
	IEEE80211_PARAM_MCS_CAP = 123,		/* Configure an MCS cap rate - for debugging */
	IEEE80211_PARAM_MAX_MGMT_FRAMES = 124,	/* Max number of mgmt frames not complete */
	IEEE80211_PARAM_MCS_ODD_EVEN = 125,	/* Configure the rate adapt algorithm to only use odd or even MCSs */
	IEEE80211_PARAM_BLACKLIST_GET = 126,	/* List blacklisted stations. */
	IEEE80211_PARAM_BA_MAX_WIN_SIZE = 128,  /* Maximum BA window size allowed on TX and RX */
	IEEE80211_PARAM_RESTRICTED_MODE = 129,	/* Enable or disable restricted mode */
	IEEE80211_PARAM_BB_MAC_RESET_MSGS = 130, /* Enable / disable display of BB amd MAC reset messages */
	IEEE80211_PARAM_PHY_STATS_MODE = 131,	/* Mode for get_phy_stats */
	IEEE80211_PARAM_BB_MAC_RESET_DONE_WAIT = 132, /* Set max wait for tx or rx before reset (secs) */
	IEEE80211_PARAM_MIN_DWELL_TIME_ACTIVE = 133,  /* min dwell time for an active channel */
	IEEE80211_PARAM_MIN_DWELL_TIME_PASSIVE = 134, /* min dwell time for a passive channel */
	IEEE80211_PARAM_MAX_DWELL_TIME_ACTIVE = 135,  /* max dwell time for an active channel */
	IEEE80211_PARAM_MAX_DWELL_TIME_PASSIVE = 136, /* max dwell time for a passive channel */
	IEEE80211_PARAM_TX_AGG_TIMEOUT = 137, /* Configure timeout for TX aggregation */
	IEEE80211_PARAM_LEGACY_RETRY_LIMIT = 138, /* Times to retry sending non-AMPDU packets (0-16) per rate */
	IEEE80211_PARAM_TRAINING_COUNT = 139,	/* Training count for rate retry algorithm (QoS NULL to STAs after assoc) */
	IEEE80211_PARAM_DYNAMIC_AC = 140,	/* Enable / disable dynamic 1 bit auto correlation algo */
	IEEE80211_PARAM_DUMP_TRIGGER = 141,	/* Request immediate dump */
	IEEE80211_PARAM_DUMP_TCM_FD = 142,	/* Dump TCM frame descriptors */
	IEEE80211_PARAM_RXCSR_ERR_ALLOW = 143,	/* allow or disallow errors packets passed to MuC */
	IEEE80211_PARAM_STOP_FLAGS = 144,	/* Alter flags where a debug halt would be performed on error conditions */
	IEEE80211_PARAM_CHECK_FLAGS = 145,	/* Alter flags for additional runtime checks */
	IEEE80211_PARAM_RX_CTRL_FILTER = 146,   /* Set the control packet filter on hal. */
	IEEE80211_PARAM_SCS = 147,		/* ACI/CCI Detection and Mitigation*/
	IEEE80211_PARAM_ALT_CHAN = 148,		/* set the chan to jump to if radar is detected */
	IEEE80211_PARAM_QTN_BCM_WAR = 149, /* Workaround for BCM receiver not accepting last aggr */
	IEEE80211_PARAM_GI_SELECT = 150,	/* Enable or disable dynamic GI selection */
	IEEE80211_PARAM_RADAR_NONOCCUPY_PERIOD = 151,	/* Specify non-occupancy period for radar */
	IEEE80211_PARAM_RADAR_NONOCCUPY_ACT_SCAN = 152,	/* non-occupancy expire scan/no-action */
	IEEE80211_PARAM_MC_LEGACY_RATE = 153, /* Legacy multicast rate table */
	IEEE80211_PARAM_LDPC_ALLOW_NON_QTN = 154, /* Allow non QTN nodes to use LDPC */
	IEEE80211_PARAM_IGMP_HYBRID = 155, /* Do the IGMP hybrid mode as suggested by Swisscom */
	IEEE80211_PARAM_BCST_4 = 156, /* Reliable (4 addr encapsulated) broadcast to all clients */
	IEEE80211_PARAM_AP_FWD_LNCB = 157, /* AP forward LNCB packets from the STA to other STAs */
	IEEE80211_PARAM_PPPC_SELECT = 158, /* Per packet power control */
	IEEE80211_PARAM_TEST_LNCB = 159, /* Test LNCB code - leaks, drops etc. */
	IEEE80211_PARAM_STBC = 160, /* Enabling/disabling STBC */
	IEEE80211_PARAM_RTS_CTS = 161, /* Enabling/disabling RTS-CTS */
	IEEE80211_PARAM_GET_DFS_CCE = 162,	/* Get most recent DFS Channel Change Event */
	IEEE80211_PARAM_GET_SCS_CCE = 163,	/* Get most recent SCS (ACI/CCI) Channel Change Event */
	IEEE80211_PARAM_GET_CH_INUSE = 164,	/* Enable printing of channels in Use at end of scan */
	IEEE80211_PARAM_RX_AGG_TIMEOUT = 165,	/* RX aggregation timeout value (ms) */
	IEEE80211_PARAM_FORCE_MUC_HALT = 166,	/* Force MUC halt debug code. */
	IEEE80211_PARAM_FORCE_ENABLE_TRIGGERS= 167,	/* Enable trace triggers */
	IEEE80211_PARAM_FORCE_MUC_TRACE = 168,	/* MuC trace force without halt */
	IEEE80211_PARAM_BK_BITMAP_MODE = 169,   /* back bit map mode set */
	IEEE80211_PARAM_PROBE_RES_RETRIES = 170,/* Controls probe response retries */
	IEEE80211_PARAM_MUC_FLAGS = 171,	/* MuC flags */
	IEEE80211_PARAM_HT_NSS_CAP = 172,	/* Set max spatial streams for HT mode */
	IEEE80211_PARAM_ASSOC_LIMIT = 173,	/* STA assoc limit */
	IEEE80211_PARAM_PWR_ADJUST_SCANCNT = 174,	/* Enable power Adjust if nearby stations don't associate */
	IEEE80211_PARAM_PWR_ADJUST = 175,	/* ioctl to adjust rx gain */
	IEEE80211_PARAM_PWR_ADJUST_AUTO = 176,	/* Enable auto RX gain adjust when associated */
	IEEE80211_PARAM_UNKNOWN_DEST_ARP = 177,	/* Send ARP requests for unknown destinations */
	IEEE80211_PARAM_UNKNOWN_DEST_FWD = 178,	/* Send unknown dest pkt to all bridge STAs */
	IEEE80211_PARAM_DBG_MODE_FLAGS = 179,	/* set/clear debug mode flags */
	IEEE80211_PARAM_ASSOC_HISTORY = 180,	/* record of remote nodes that have associated by MAC address */
	IEEE80211_PARAM_CSW_RECORD = 181,	/* get channel switch record data */
	IEEE80211_PARAM_RESTRICT_RTS = 182,     /* HW xretry failures before switching to RTS mode */
	IEEE80211_PARAM_RESTRICT_LIMIT = 183,   /* RTS xretry failures before starting restricted mode */
	IEEE80211_PARAM_AP_ISOLATE = 184,	/* set ap isolation mode */
	IEEE80211_PARAM_IOT_TWEAKS = 185,	/* mask to switch on / off IOT tweaks */
	IEEE80211_PARAM_SWRETRY_AGG_MAX = 186,	/* max sw retries for ampdus */
	IEEE80211_PARAM_SWRETRY_NOAGG_MAX = 187,/* max sw retries for non-agg mpdus */
	IEEE80211_PARAM_BSS_ASSOC_LIMIT = 188, /* STA assoc limit for a VAP */

	IEEE80211_PARAM_VSP_NOD_DEBUG = 190,	/* turn on/off NOD debugs for VSP */
	IEEE80211_PARAM_CCA_PRI = 191,		/* Primary CCA threshold */
	IEEE80211_PARAM_CCA_SEC = 192,		/* Secondary CCA threshold */
	IEEE80211_PARAM_DYN_AGG_TIMEOUT = 193,	/* Enable feature which try to prevent unnecessary waiting of aggregate before sending */
	IEEE80211_PARAM_HW_BONDING = 194,	/* For now, this IOCTL number need to be reserved for HW bonding option in V3X */
	IEEE80211_PARAM_PS_CMD = 195,		/* Command to enable, disable, etc probe select for matrices */
	IEEE80211_PARAM_PWR_SAVE = 196,		/* Power save parameter ctrl */
	IEEE80211_PARAM_DBG_FD = 197,		/* Debug FD alloc/free */
	IEEE80211_PARAM_DISCONN_CNT = 198,	/* get count of disconnection event */
	IEEE80211_PARAM_FAST_REASSOC = 199,	/* Do a fast reassociation */
	IEEE80211_PARAM_SIFS_TIMING = 200,	/* SIFS timing */
	IEEE80211_PARAM_TEST_TRAFFIC = 201,	/* Test Traffic start|stop control */
	IEEE80211_PARAM_TX_AMSDU = 202,		/* Disable/enable AMSDU and/or Adaptive AMSDU for transmission to Quantenna clients */
	IEEE80211_PARAM_SCS_DFS_REENTRY_REQUEST = 203,	/* DFS re-entry request from SCS */
	IEEE80211_PARAM_QCAT_STATE = 204,	/* QCAT state information */
	IEEE80211_PARAM_RALG_DBG = 205,		/* Rate adaptation debugging */
	IEEE80211_PARAM_PPPC_STEP = 206,	/* PPPC step size control */
	IEEE80211_PARAM_QTN_BGSCAN_DWELL_TIME_ACTIVE = 207,  /* Quantenna bgscan dwell time for an active channel */
	IEEE80211_PARAM_QTN_BGSCAN_DWELL_TIME_PASSIVE = 208, /* Quantenna bgscan dwell time for a passive channel */
	IEEE80211_PARAM_QTN_BGSCAN_DEBUG = 209,	/* Quantenna background scan debugging */
	IEEE80211_PARAM_CONFIG_REGULATORY_TXPOWER = 210,	/* configure regulatory TX power for a band (start chan to stop chan) */
	IEEE80211_PARAM_SINGLE_AGG_QUEUING = 211,	/* Queue only AMPDU fd at a time on a given tid till all sw retries are done */
	IEEE80211_PARAM_CSA_FLAG = 212,         /* Channel switch announcement flag */
	IEEE80211_PARAM_BR_IP_ADDR = 213,
	IEEE80211_PARAM_REMAP_QOS = 214,	/* Command to enable, disable, qos remap feature, asked by customer */
	IEEE80211_PARAM_DEF_MATRIX = 215,	/* Use default expansion matrices */
	IEEE80211_PARAM_SCS_CCA_INTF = 216,	/* CCA interference for a channel */
	IEEE80211_PARAM_CONFIG_TPC_INTERVAL = 217,	/* periodical tpc request interval */
	IEEE80211_PARAM_TPC_QUERY = 218,	/* enable or disable tpc request periodically */
	IEEE80211_PARAM_TPC = 219,		/* tpc feature enable/disable flag */
	IEEE80211_PARAM_CACSTATUS = 220,	/* Get CAC status */
	IEEE80211_PARAM_RTSTHRESHOLD = 221,	/* Get/Set RTS Threshold */
	IEEE80211_PARAM_BA_THROT = 222,         /* Manual BA throttling */
	IEEE80211_PARAM_TX_QUEUING_ALG = 223,	/* MuC TX queuing algorithm */
	IEEE80211_PARAM_BEACON_ALLOW = 224,	/* To en/disable beacon rx when associated as STA*/
	IEEE80211_PARAM_1BIT_PKT_DETECT = 225,  /* enable/disable 1bit pkt detection */
	IEEE80211_PARAM_WME_THROT = 226,	/* Manual WME throttling */
#ifdef TOPAZ_PLATFORM
	IEEE80211_PARAM_ENABLE_11AC = 227,	/* Enable-disable 11AC feature in Topaz */
	IEEE80211_PARAM_FIXED_11AC_TX_RATE = 228,	/* Set 11AC mcs */
#endif
	IEEE80211_PARAM_GENPCAP = 229,		/* WMAC tx/rx pcap ring buffer */
	IEEE80211_PARAM_CCA_DEBUG = 230,	/* Debug of CCA */
	IEEE80211_PARAM_STA_DFS	= 231,		/* Enable or disable station DFS */
	IEEE80211_PARAM_OCAC = 232,		/* Off-channel CAC */
	IEEE80211_PARAM_CCA_STATS_PERIOD = 233,	/* the period for updating CCA stats in MuC */
	IEEE80211_PARAM_AUC_TX = 234,		/* Control HW datapath transmission */
	IEEE80211_PARAM_RADAR_BW = 235,		/* Set radar filter mode */
	IEEE80211_PARAM_TDLS_DISC_INT = 236,	/* Set TDLS discovery interval */
	IEEE80211_PARAM_TDLS_PATH_SEL_WEIGHT = 237,	/* The weight of path selection algorithm, 0 means always to use TDLS link */
	IEEE80211_PARAM_DAC_DBG = 238,		/* dynamic ac debug */
	IEEE80211_PARAM_CARRIER_ID = 239,	/* Get/Set carrier ID */
	IEEE80211_PARAM_SWRETRY_SUSPEND_XMIT = 240,	/* Max sw retries when sending frames is suspended */
	IEEE80211_PARAM_DEACTIVE_CHAN_PRI = 241,/* Deactive channel as being used as primary channel */
	IEEE80211_PARAM_RESTRICT_RATE = 242,	/* Packets per second sent when in Tx restrict mode */
	IEEE80211_PARAM_AUC_RX_DBG = 243,	/* AuC rx debug command */
	IEEE80211_PARAM_RX_ACCELERATE = 244,	/* Enable/Disable Topaz MuC rx accelerate */
	IEEE80211_PARAM_RX_ACCEL_LOOKUP_SA = 245,	/* Enable/Disable lookup SA in FWT for rx accelerate */
	IEEE80211_PARAM_TX_MAXMPDU = 246,		/* Set Max MPDU size to be supported */
	IEEE80211_PARAM_SNIFFER_DST_IP_ADDR = 247,	/* Sniffer destination IP address (IPv4 multicast) */
	IEEE80211_PARAM_SNIFFER_CAPT_MAX = 248,		/* Sniffer max 802.11 frame bytes copied */
	IEEE80211_PARAM_SPECIFIC_SCAN = 249,	/* Just perform specific SSID scan */
	IEEE80211_PARAM_VLAN_CONFIG = 250,		/* Configure VAP into MBSS or Passthrough mode */
	IEEE80211_PARAM_TRAINING_START = 251,	/* restart rate training to a particular node */
	IEEE80211_PARAM_AUC_TX_DBG = 252,	/* AuC tx debug command */
	IEEE80211_PARAM_AC_INHERITANCE = 253,	/* promote AC_BE to use aggresive medium contention */
	IEEE80211_PARAM_NODE_OPMODE = 254,	/* Set bandwidth and NSS used for a particular node */
	IEEE80211_PARAM_TACMAP = 255,		/* Config TID AC and priority at TAC_MAP, debug only */
	IEEE80211_PARAM_VAP_PRI = 256,		/* Config priority for VAP, used for TID priority at TAC_MAP */
	IEEE80211_PARAM_AUC_QOS_SCH = 257,	/* Tune QoS scheduling in AuC */
	IEEE80211_PARAM_TXBF_IOT = 258,         /* turn on/off TxBF IOT to non QTN node */
	IEEE80211_PARAM_CONGEST_IDX = 259,	/* Current channel congestion index */
	IEEE80211_PARAM_SPEC_COUNTRY_CODE = 260,	/* Set courntry code for EU region */
	IEEE80211_PARAM_AC_Q2Q_INHERITANCE = 261,	/* promote AC_BE to use aggresive medium contention - Q2Q case */
	IEEE80211_PARAM_1SS_AMSDU_SUPPORT = 262,	/* Enable-Disable AMSDU support for 1SS devies - phone and tablets */
	IEEE80211_PARAM_VAP_PRI_WME = 263,	/* Automatic adjusting WME bss param based on VAP priority */
	IEEE80211_PARAM_MICHAEL_ERR_CNT = 264,	/* total number of TKIP MIC errors */
	IEEE80211_PARAM_DUMP_CONFIG_TXPOWER = 265,	/* Dump configured txpower for all channels */
	IEEE80211_PARAM_EMI_POWER_SWITCHING = 266,	/* Enable/Disable EMI power switching */
	IEEE80211_PARAM_CONFIG_BW_TXPOWER = 267,	/* Configure the TX powers different bandwidths */
	IEEE80211_PARAM_SCAN_CANCEL = 268,			/* Cancel any ongoing scanning */
	IEEE80211_PARAM_VHT_NSS_CAP = 269,	/* Set max spatial streams for VHT mode */
	IEEE80211_PARAM_FIXED_BW = 270,		/* Configure fixed tx bandwidth without changing BSS bandwidth */
	IEEE80211_PARAM_DYN_RTSCTS = 271,	/* Dynamic RTS/CTS paramters */
};

#define	SIOCG80211STATS			(SIOCDEVPRIVATE+2)
/* NB: require in+out parameters so cannot use wireless extensions, yech */
#define	IEEE80211_IOCTL_GETKEY		(SIOCDEVPRIVATE+3)
#define	IEEE80211_IOCTL_GETWPAIE	(SIOCDEVPRIVATE+4)
#define	IEEE80211_IOCTL_STA_STATS	(SIOCDEVPRIVATE+5)
#define	IEEE80211_IOCTL_STA_INFO	(SIOCDEVPRIVATE+6)
#define	SIOC80211IFCREATE		(SIOCDEVPRIVATE+7)
#define	SIOC80211IFDESTROY		(SIOCDEVPRIVATE+8)
#define	IEEE80211_IOCTL_SCAN_RESULTS	(SIOCDEVPRIVATE+9)
#define SIOCR80211STATS                 (SIOCDEVPRIVATE+0xA) /* This define always has to sync up with SIOCRDEVSTATS in /linux/sockios.h */
#define IEEE80211_IOCTL_GET_ASSOC_TBL	(SIOCDEVPRIVATE+0xB)
#define IEEE80211_IOCTL_GET_RATES	(SIOCDEVPRIVATE+0xC)
#define IEEE80211_IOCTL_EXT		(SIOCDEVPRIVATE+0xF) /* This command is used to support sub-ioctls */

/*
 * ioctl command IEEE80211_IOCTL_EXT is used to support sub-ioctls.
 * The following lists the sub-ioctl numbers
 *
 */
#define SIOCDEV_SUBIO_BASE		(0)
#define SIOCDEV_SUBIO_RST_QUEUE		(SIOCDEV_SUBIO_BASE + 1)
#define SIOCDEV_SUBIO_RADAR_STATUS	(SIOCDEV_SUBIO_BASE + 2)
#define SIOCDEV_SUBIO_GET_PHY_STATS	(SIOCDEV_SUBIO_BASE + 3)
#define SIOCDEV_SUBIO_DISCONN_INFO	(SIOCDEV_SUBIO_BASE + 4)
#define SIOCDEV_SUBIO_SET_BRCM_IOCTL	(SIOCDEV_SUBIO_BASE + 5)
#define SIOCDEV_SUBIO_SCS	        (SIOCDEV_SUBIO_BASE + 6)
#define SIOCDEV_SUBIO_SET_SOC_ADDR_IOCTL	(SIOCDEV_SUBIO_BASE + 7) /* Command to set the SOC addr of the STB to VAP for recording */
#define SIOCDEV_SUBIO_WAIT_SCAN_TIMEOUT	(SIOCDEV_SUBIO_BASE + 9)
#define SIOCDEV_SUBIO_AP_SCAN_RESULTS	(SIOCDEV_SUBIO_BASE + 10)
#define SIOCDEV_SUBIO_GET_11H_11K_NODE_INFO	(SIOCDEV_SUBIO_BASE + 11)
#define SIOCDEV_SUBIO_GET_DSCP2AC_MAP	(SIOCDEV_SUBIO_BASE + 12)
#define SIOCDEV_SUBIO_SET_DSCP2AC_MAP	(SIOCDEV_SUBIO_BASE + 13)
#define SIOCDEV_SUBIO_SET_MARK_DFS_CHAN	(SIOCDEV_SUBIO_BASE + 14)

struct ieee80211_clone_params {
	char icp_name[IFNAMSIZ];		/* device name */
	u_int16_t icp_opmode;			/* operating mode */
	u_int16_t icp_flags;			/* see below */
#define	IEEE80211_CLONE_BSSID	0x0001		/* allocate unique mac/bssid */
#define	IEEE80211_NO_STABEACONS	0x0002		/* Do not setup the station beacon timers */
};

/* APPIEBUF related definitions */

/* Management frame type to which application IE is added */
enum {
	IEEE80211_APPIE_FRAME_BEACON		= 0,
	IEEE80211_APPIE_FRAME_PROBE_REQ		= 1,
	IEEE80211_APPIE_FRAME_PROBE_RESP	= 2,
	IEEE80211_APPIE_FRAME_ASSOC_REQ		= 3,
	IEEE80211_APPIE_FRAME_ASSOC_RESP	= 4,
	IEEE80211_APPIE_NUM_OF_FRAME		= 5
};

struct ieee80211req_getset_appiebuf {
	u_int32_t	app_frmtype;		/* management frame type for which buffer is added */
	u_int32_t	app_buflen;		/* application-supplied buffer length */
#define F_QTN_IEEE80211_PAIRING_IE 0x1
	u_int8_t	flags;			/* flags here is used to check whether QTN pairing IE exists */
	u_int8_t	app_buf[0];		/* application-supplied IE(s) */
};

struct qtn_cca_args
{
	u_int32_t cca_channel;
	u_int32_t duration;
};

/* Flags ORed by application to set filter for receiving management frames */
enum {
	IEEE80211_FILTER_TYPE_BEACON		= 1<<0,
	IEEE80211_FILTER_TYPE_PROBE_REQ		= 1<<1,
	IEEE80211_FILTER_TYPE_PROBE_RESP	= 1<<2,
	IEEE80211_FILTER_TYPE_ASSOC_REQ		= 1<<3,
	IEEE80211_FILTER_TYPE_ASSOC_RESP	= 1<<4,
	IEEE80211_FILTER_TYPE_AUTH		= 1<<5,
	IEEE80211_FILTER_TYPE_DEAUTH		= 1<<6,
	IEEE80211_FILTER_TYPE_DISASSOC		= 1<<7,
	IEEE80211_FILTER_TYPE_ALL		= 0xFF	/* used to check the valid filter bits */
};

struct ieee80211req_set_filter {
	u_int32_t app_filterype;		/* management frame filter type */
};

/* Tx Restrict */
#define IEEE80211_TX_RESTRICT_RTS_MIN		2
#define IEEE80211_TX_RESTRICT_RTS_DEF		6
#define IEEE80211_TX_RESTRICT_LIMIT_MIN		2
#define IEEE80211_TX_RESTRICT_LIMIT_DEF		12
#define IEEE80211_TX_RESTRICT_RATE		5

/* Compatibility fix bitmap for various vendor peer */
#define VENDOR_FIX_BRCM_DHCP			0x01
#define VENDOR_FIX_BRCM_REPLACE_IGMP_SRCMAC	0x02
#define VENDOR_FIX_BRCM_REPLACE_IP_SRCMAC	0x04
#define VENDOR_FIX_BRCM_DROP_STA_IGMPQUERY	0x08

enum vendor_fix_idx {
	VENDOR_FIX_IDX_BRCM_DHCP = 1,
	VENDOR_FIX_IDX_BRCM_IGMP = 2,
	VENDOR_FIX_IDX_MAX = VENDOR_FIX_IDX_BRCM_IGMP,
};

#endif /* __linux__ */

#endif /* _NET80211_IEEE80211_IOCTL_H_ */
