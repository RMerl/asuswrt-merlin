/*
 * LMAC API definitions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Definitions subject to change without notice.
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
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
 * $Id: wllmacctl.h,v 13.31 2009-10-05 20:49:01 Exp $:
 */

#ifndef _wllmacctl_h_
#define	_wllmacctl_h_

#include <typedefs.h>
#include <proto/ethernet.h>
#include <proto/bcmeth.h>
#include <proto/bcmevent.h>
#include <proto/802.11.h>

/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */

/* Init/Configure response */

typedef struct {
	uint8	vec[32];	/* bitvec of channels */
} lmacchanvec_t;

#define LMAC_MAXMCS		80

typedef struct {
	uint8	mcs[(LMAC_MAXMCS / 8)];
} lmac_mcs_t;

typedef struct {
	uint32		htcaps;
	uint16		max_phy_rxrate;		/* Mbps */
	uint8		sup_chan_width;		/* 20MHz/40MHz */
	uint8		rx_stbc_cap;
	uint8		max_amsdulen;
	uint8		max_rx_ampdulen;
	uint8		ampdu_spacing;		/* time between mpdus in an ampdu */
	lmac_mcs_t	rxmcs_set;
	lmac_mcs_t	txmcs_set;
	uint8		pco_trans_time;
	uint8		sup_mcs_feedback;
	uint8		reserved[3];
} lmac_ht_cap_t;

/* LMAC HT CAP field specific definitions */

/* htcaps bits 13-31 reserved */
#define LMAC_HTCAP_RX_LDPC		0x00000001
#define LMAC_HTCAP_40MHz		0x00000002
#define LMAC_HTCAP_GF_FRAMES		0x00000004
#define LMAC_HTCAP_SGI_20MHz		0x00000008
#define LMAC_HTCAP_SGI_40MHz		0x00000010
#define LMAC_HTCAP_STBC_TX		0x00000020
#define LMAC_HTCAP_DELAYED_BA		0x00000040
#define LMAC_HTCAP_DSSSCCK_40MHz	0x00000080
#define LMAC_HTCAP_PSMP			0x00000100
#define LMAC_HTCAP_LSIG_TXOP		0x00000200
#define LMAC_HTCAP_PCO			0x00000400
#define LMAC_HTCAP_HTC_FIELD		0x00000800
#define LMAC_HTCAP_RD_RESP		0x00001000

/* sup_chan_width */
#define LMAC_SUP_20MHz_ONLY		0
#define LMAC_SUP_20MHz40MHz		1

/* rx_stbc_cap */
#define LMAC_RXSTBC_UNSUPPORTED		0
#define LMAC_RXSTBC_1_SSTREAM		1
#define LMAC_RXSTBC_1_2_SSTREAM		2
#define LMAC_RXSTBC_1_2_3_SSTREAM	3

/* max_amsdulen */
#define LMAC_MAXAMSDU_SIZE_3839		0
#define LMAC_MAXAMSDU_SIZE_7935		1

/* max_rx_ampdulen */
#define LMAC_RXMAXAMPDU_SIZE_8191	0
#define LMAC_RXMAXAMPDU_SIZE_16383	1
#define LMAC_RXMAXAMPDU_SIZE_32767	2
#define LMAC_RXMAXAMPDU_SIZE_65535	3

/* ampdu_spacing */
#define LMAC_SPACING_NORESTRICT		0
#define LMAC_SPACING_250ns		1
#define LMAC_SPACING_512ns		2
#define LMAC_SPACING_1000ns		3
#define LMAC_SPACING_2000ns		4
#define LMAC_SPACING_4000ns		5
#define LMAC_SPACING_8000ns		6
#define LMAC_SPACING_16000ns		7

/* pco_trans_time */
#define LMAC_PCO_TRANSITION_NONE	0
#define LMAC_PCO_TRANSITION_400us	1
#define LMAC_PCO_TRANSITION_1500us	2
#define LMAC_PCO_TRANSITION_5000us	3

/* sup_mcs_feedback */
#define LMAC_MCSFEEDBACK_NONE		0
#define LMAC_MCSFEEDBACK_RSVD		1
#define LMAC_MCSFEEDBACK_UNSOLICITED	2
#define LMAC_MCSFEEDBACK_ALL		3


#define LMAC_CAP_MAGIC		0xdeadbeef
#define LMAC_VERSION_BUF_SIZE	64

typedef struct wl_lmac_cap {
	uint32		magic;
	wl_rateset_t	hw_legacy_rateset;	/* nonHT hw rates */
	uint8		scan_ssid;
	uint8		bands;			/* bands (WLC_BAND_xx) */
	uint8		nmulticast;
	uint8		nratefallbackclasses;
	uint16		ssid_buffer_len;
	uint16		rxpacketflags;
	struct {
		int32	max_dBm;
		int32	min_dBm;
		uint32	step_dB;
	} txpower[3];
	uint32		capabilities;
	uint8		max_txbuffers[AC_COUNT];
	uint16		max_rxbuffers;
	uint8		max_ethtype_filters;
	uint8		max_udpport_filters;

	/* HT Capabilities */
	lmac_ht_cap_t	lmac_htcap;

	/* BROADCOM specific parts */
	uint32		radioid[2];
	lmacchanvec_t 	sup_chan[2];
	char		lmac_version[LMAC_VERSION_BUF_SIZE];
	uint8		rxhdr_size;
	uint8		rxhdr_hwrxoff;
	uint8		PAD[2];
	uint32		PAD[7];
	wlc_rev_info_t	lmac_rev_info;
	uint32		PAD[8];
} wl_lmac_cap_t;

/* LMAC capabilities */
#define LMAC_CAP_MAXRXLIFETIME_AC	0x00000001
#define LMAC_CAP_IBSS_PS		0x00000002
#define LMAC_CAP_TRUNCATE_TXPOLICY	0x00000004
#define LMAC_CAP_PREAMBLE_OVERRIDE	0x00000008
#define LMAC_CAP_TXPWR_SENDPKT		0x00000010
#define LMAC_CAP_EXPIRY_TXPACKET	0x00000020
#define LMAC_CAP_PROBEFOR_JOIN		0x00000040
#define LMAC_CAP_MAXLIFETIME_Q		0x00000080
#define LMAC_CAP_TXNOACK_Q		0x00000100
#define LMAC_CAP_BLOCKACK_Q		0x00000200
#define LMAC_CAP_DOT11SLOTTIME		0x00000400
#define LMAC_CAP_WMM_SA			0x00000800
#define LMAC_CAP_SAPSD			0x00001000
#define LMAC_CAP_RM			0x00002000
#define LMAC_CAP_LEGACY_PSPOLL_Q	0x00004000
#define LMAC_CAP_WEP128			0x00008000
#define LMAC_CAP_MOREDATA_ACK		0x00010000
#define LMAC_CAP_SCAN_MINMAX_TIME	0x00020000
#define LMAC_CAP_TXAUTORATE		0x00040000
#define LMAC_CAP_NOIVICV_IN_PKT		0x00080000
#define LMAC_CAP_HT			0x00100000
#define LMAC_CAP_WAPI			0x00200000
#define LMAC_CAP_DSPARAM_IN_PRB		0x00400000
#define LMAC_CAP_STA			0x80000000	/* (BRCM) STA functions included */

/* LMAC Join params */
#define WLC_LMAC_JOIN_MODE_IBSS		0
#define WLC_LMAC_JOIN_MODE_BSS		1

/* Note: order of fields is chosen carefully for proper alignment */
typedef struct wl_lmac_join_params {
	uint32			mode;
	uint32			beacon_interval;	/* TUs */
	uint16			atim_window;
	struct ether_addr	bssid;
	uint8			band;
	uint8			channel;
	uint8			preamble;
	uint8			SSID_len;
	uint8			SSID[32];
	uint8			probe_for_join;
	uint8			rsvd[3];
	wl_rateset_t		basic_rateset;
} wl_lmac_join_params_t;

typedef struct wl_lmac_bss_params {
	uint16			aid;
	uint8			dtim_interval;
} wl_lmac_bss_params_t;

typedef enum queue_id {
	Q_BE = 0,
	Q_BK,
	Q_VI,
	Q_VO,
	Q_HCCA
} queue_id_t;

typedef enum ps_scheme {
	PS_REGULAR = 0,
	PS_UAPSD,
	PS_LEGACY_PSPOLL,
	PS_SAPSD
} ps_scheme_t;

typedef enum ack_policy {
	ACKP_NORMAL = 0,
	ACKP_NOACK,
	ACKP_BLOCKACK
} ack_policy_t;

typedef struct wl_lmac_conf_q {
	queue_id_t 	qid;
	ack_policy_t	ack;
	ps_scheme_t	ps;
	uint8		pad;

	uint32		maxtxlifetime;	/* usec */
	struct {
		uint32	start_time;
		uint32	interval;
	} sapsd_conf;
	uint16		mediumtime;
} wl_lmac_conf_q_t;

typedef struct wl_lmac_conf_ac {
	uint16	cwmin[AC_COUNT];
	uint16	cwmax[AC_COUNT];
	uint8	aifs[AC_COUNT];
	uint16	txop[AC_COUNT];
	uint32	max_rxlifetime[AC_COUNT];
} wl_lmac_conf_ac_t;

#define TEMPLATE_BUFFER_LEN		(256 + 32)
#define TEMPLATE_BEACON_FRAME		0
#define TEMPLATE_PRBREQ_FRAME		1
#define TEMPLATE_NULLDATA_FRAME		2
#define TEMPLATE_PRBRESP_FRAME		3
#define TEMPLATE_QOSNULLDATA_FRAME	4
#define TEMPLATE_PSPOLL_FRAME		5
#define	TEMPLATE_COUNT			6	/* Must be last */

struct wl_lmac_frmtemplate {
	uint32	frm_type;
	uint32	rate;
	uint32	length;			/* Actual size of frm_data[] content only */
};

#define LMAC_FRMTEMPLATE_FIXED_SIZE	(sizeof(struct wl_lmac_frmtemplate))

#define BCNIE_FILTER_BIT0_MASK		0x01
#define BCNIE_FILTER_BIT1_MASK		0x02

typedef struct wl_lmac_bcnfilter {
	uint32	bcn_filter_enable;
	uint32	hostwake_bcn_count;
} wl_lmac_bcnfilter_t;

#define WLLMAC_BCNFILTER_MAX_IES	8
typedef struct wl_lmac_bcnie {
	uint8	id;
	uint8	mask;
	int16  offset;
	uint8	OUI[3];
	uint8	type;
	uint16	ver;
} wl_lmac_bcnie_t;

typedef struct wl_lmac_bcniefilter {
	uint32			nies;
	wl_lmac_bcnie_t		ies[WLLMAC_BCNFILTER_MAX_IES];
} wl_lmac_bcniefilter_t;

typedef struct wl_lmac_txrate_class {
	uint8		retry_54Mbps;
	uint8		retry_48Mbps;
	uint8		retry_36Mbps;
	uint8		retry_33Mbps;
	uint8		retry_24Mbps;
	uint8		retry_22Mbps;
	uint8		retry_18Mbps;
	uint8		retry_12Mbps;
	uint8		retry_11Mbps;
	uint8		retry_9Mbps;
	uint8		retry_6Mbps;
	uint8		retry_5p5Mbps;
	uint8		retry_2Mbps;
	uint8		retry_1Mbps;
	uint8		SRL;
	uint8		LRL;
	uint32		iflags;
} wl_lmac_txrate_class_t;

typedef struct wl_lmac_txrate_policy {
	uint32		nclasses;
	wl_lmac_txrate_class_t txrate_classes[1];
} wl_lmac_txrate_policy_t;

typedef struct wl_lmac_setchannel {
	uint8 		channel;
	uint8		txp_user_target[45];
	uint8		txp_limit[45];
	uint8		override;
	uint8		txpwr_percent;
} wl_lmac_set_channel_t;

typedef struct wl_lmac_addkey {
	uint8		index;
	uint8		reserved[3];
	wl_wsec_key_t	key;
} wl_lmac_addkey_t;

typedef struct wl_lmac_delkey {
	uint8		keyindex;
} wl_lmac_delkey_t;

typedef struct wl_bcmlmac_txdone {
	/* Length is D11_TXH_LEN(104),TXSTATUS_LEN(16),D11_PHY_HDR_LEN(6) */
	uint8		data[128];
} wl_bcmlmac_txdone_t;

#define LMAC_MODE_NORMAL	0
#define LMAC_MODE_EXTENDED	1	/* Expands format of txheader/rxheader/rxstatus/etc */

/* LMAC RX filter mode bits */
#define LMAC_PROMISC		0x00000001
#define LMAC_BSSIDFILTER	0x00000002
#define LMAC_MONITOR		0x80000000

/* Error codes that can be returned by LMAC */
#define LMAC_SUCCESS			0
#define LMAC_FAILURE			1
#define LMAC_RX_DECRYPT_FAILED		2
#define LMAC_RX_MICFAILURE		3
#define LMAC_SUCCESSXFER		4
#define	LMAC_PENDING			5
#define LMAC_TXQ_FULL			6
#define LMAC_EXCEED_RETRY		7
#define LMAC_LIFETIME_EXPIRED		8
#define LMAC_NOLINK			9
#define	LMAC_MAC_ERROR			10

/* LMAC events */
#define LMAC_EVENT_INITDONE		0
#define LMAC_EVENT_FATAL		1
#define LMAC_EVENT_BSSLOST		2
#define LMAC_EVENT_BSSREGAINED		3
#define LMAC_EVENT_RADARDETECT		4
#define LMAC_EVENT_LOWRSSI		5
#define LMAC_EVENT_SCANCOMPLETE		6
#define LMAC_EVENT_RMCOMPLETE		7
#define LMAC_EVENT_JOINCOMPLETE		8
#define LMAC_EVENT_PSCOMPLETE		9
#define LMAC_EVENT_TRACE		10
#define LMAC_EVENT_LAST			11	/* Must be last */

typedef struct wllmac_join_data {
	int32	max_powerlevel;
	int32	min_powerlevel;
} wllmac_join_data_t;

typedef struct wllmac_pscomplete_data {
	uint32 pmstate;
} wllmac_pscomplete_data_t;

typedef struct wllmac_scancomplete_data {
	uint32 pmstate;
} wllmac_scancomplete_data_t;

typedef struct wl_lmac_rmreq_params {
	int32		tx_power;
	uint32		channel;
	uint8		band;
	uint8		activation_delay;
	uint8		measurement_offset;
	uint8		nmeasures;
} wl_lmac_rmreq_params_t;

typedef struct wlc_lmac_rm_req {
	uint32		type;
	uint32		dur;
	uint32		resrved;
} wl_lmac_rm_req_t;

struct wlc_lmac_rm_bcn_measure {
	uint32		scan_mode;
};

#define LMAC_WAKEUP_BEACON			0
#define LMAC_WAKEUP_DTIMBEACON			1
#define LMAC_WAKEUP_NBEACONS			2
#define LMAC_WAKEUP_NDTIMBEACONS		3

#define LMAC_SLEEPMODE_WAKEUP			0
#define LMAC_SLEEPMODE_PDOWN			1
#define LMAC_SLEEPMODE_LPDOWN			2


#define WLLMAC_PF_DISABLE			0
#define WLLMAC_PF_MATCH_FORWARD			1
#define WLLMAC_PF_MATCH_DISCARD			2

#define WLLMAC_PFTYPE_ETHTYPE			1
#define WLLMAC_PFTYPE_ARPHOSTIP			2	/* not used for now */
#define WLLMAC_PFTYPE_BCAST_UDPPORT		3

typedef struct wllmac_pktfilter {
	uint8 pf_type;
	uint8 pf_flags;
	uint8 pf_nelements;
	uint8 reserved;
	/* actual pkt filter data starts here */
} wllmac_pktfilter_t;

#define WLLMAC_PKTFILTER_MINSIZE	sizeof(wllmac_pktfilter_t)

typedef struct wllmac_htcap {
	uint8			ht_supported;
	uint8			rx_stbc_cap;
	struct ether_addr       ibss_peer_mac;
	ht_cap_ie_t		ie;
} wllmac_htcap_t;

#define LMAC_HT_RXBCN_PRIMARY		0x00
#define LMAC_HT_RXBCN_ANY		0x01
#define LMAC_HT_RXBCN_SECONDARY		0x02

typedef struct wllmac_ht_secbcn {
	uint8			sec_bcn;
	uint8			reserved[3];
} wllmac_ht_secbcn_t;

typedef struct wllmac_ht_blockack {
	uint8	tx_tid_bitmap;
	uint8	rx_tid_bitmap;
	uint8	reserved[2];
} wllmac_ht_blockack_t;

#define WLLMAC_RATE_MCS				0x8000
#define WLLMAC_RATE_MASK			0x7fff

#define WLLMAC_RX_FLAG_RAMATCH			0x00000001
#define WLLMAC_RX_FLAG_MCASTMATCH		0x00000002
#define WLLMAC_RX_FLAG_BCASTMATCH		0x00000004
#define WLLMAC_RX_FLAG_BCNTIM_SET		0x00000008
#define WLLMAC_RX_FLAG_BCNVBMP_LOT		0x00000010
#define WLLMAC_RX_FLAG_SSID_MATCH		0x00000020
#define WLLMAC_RX_FLAG_BSSID_MATCH		0x00000040
#define WLLMAC_RX_FLAG_ENC_MASK			0x00038000
#define WLLMAC_RX_FLAG_MORE_RX			0x00040000
#define WLLMAC_RX_FLAG_MORE_MEASURE		0x00080000
#define WLLMAC_RX_FLAG_HTPACKET			0x00100000
#define WLLMAC_RX_FLAG_AMPDU			0x00200000
#define WLLMAC_RX_FLAG_STBC			0x00400000

#define WLLMAC_RX_FLAG_ENC_SHIFT		15
#define WLLMAC_RX_FLAG_ENC_NONE			(0 << WLLMAC_RX_FLAG_ENC_SHIFT)
#define WLLMAC_RX_FLAG_ENC_WAPI			(1 << WLLMAC_RX_FLAG_ENC_SHIFT)
#define WLLMAC_RX_FLAG_ENC_WEP			(2 << WLLMAC_RX_FLAG_ENC_SHIFT)
#define WLLMAC_RX_FLAG_ENC_TKIP			(4 << WLLMAC_RX_FLAG_ENC_SHIFT)
#define WLLMAC_RX_FLAG_ENC_AES			(6 << WLLMAC_RX_FLAG_ENC_SHIFT)


typedef struct wllmac_autorate_class {
	wl_rateset_t	legacy_rateset;
	lmac_mcs_t	mcs_rates;
	uint8		index;
	uint8		SRL;
	uint8		LRL;
	uint8		pad[3];
} wllmac_autorate_t;

#endif /* _wllmacctl_h_ */
