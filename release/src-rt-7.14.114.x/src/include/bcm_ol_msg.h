/*
 * Broadcom 802.11 Message infra (pcie<-> CR4) used for RX offloads
 *
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bcm_ol_msg.h chandrum $
 */

#ifndef _BCM_OL_MSG_H_
#define _BCM_OL_MSG_H_

#include <epivers.h>
#include <typedefs.h>
#include <wlc_utils.h>
#ifdef BCM_OL_DEV
#include <proto/ethernet.h>
#include <bcmcrypto/tkhash.h>
#include <proto/802.11.h>
#endif
#include <proto/eapol.h>
#ifdef WLOFFLD
#include <wlc_key.h>
#endif
#include <bcmdevs.h>
#include <proto/bcmip.h>
#include <proto/bcmipv6.h>
#include <proto/802.1d.h>
#include <wlc_phy_hal.h>
#include <bcmwpa.h>

#define OLMSG_RW_MAX_ENTRIES		2

/* Dongle indecies */
#define OLMSG_READ_DONGLE_INDEX		0
#define OLMSG_WRITE_DONGLE_INDEX	1

/* Host indecies */
#define OLMSG_READ_HOST_INDEX		1
#define OLMSG_WRITE_HOST_INDEX		0
#define OLMSG_BUF_SZ			0x10000 /* 64k */
#define OLMSG_HOST_BUF_SZ		0x7800 /* 30k */
#define OLMSG_DGL_BUF_SZ		0x7800 /* 30k */

/* Maximum IE id for non vendor specific IE */
#define OLMSG_BCN_MAX_IE		222
#define MAX_VNDR_IE			50 /* Needs to be looked into */
#define MAX_IE_LENGTH_BUF		2048
#define MAX_STAT_ENTRIES		16
#define NEXT_STAT(x)			((x + 1) & ((MAX_STAT_ENTRIES) - 1))
#define IEMASK_SZ			CEIL((OLMSG_BCN_MAX_IE+1), 8)
#define DEFAULT_KEYS			4
#define NUMRXIVS			5

#define MARKER_BEGIN	0xA5A5A5A5
#define MARKER_END		~MARKER_BEGIN

#define MAX_OL_EVENTS			16
#define	MAX_OL_SCAN_CONFIG		9
#define	MAX_OL_SCAN_BSS			5

#define RSSI_THRESHOLD_2G_DEF		-80
#define RSSI_THRESHOLD_5G_DEF		-75

/* Events among various offload modules */
enum {
	BCM_OL_E_WOWL_START,
	BCM_OL_E_WOWL_COMPLETE,
	BCM_OL_E_TIME_SINCE_BCN,
	BCM_OL_E_BCN_LOSS,
	BCM_OL_E_DEAUTH,
	BCM_OL_E_DISASSOC,
	BCM_OL_E_RETROGRADE_TSF,
	BCM_OL_E_RADIO_HW_DISABLED,
	BCM_OL_E_PME_ASSERTED,
	BCM_OL_E_UNASSOC,
	BCM_OL_E_SCAN_BEGIN,
	BCM_OL_E_SCAN_END,
	BCM_OL_E_PREFSSID_FOUND,
	BCM_OL_E_CSA,
	BCM_OL_E_MAX
};

enum {
	BCM_OL_UNUSED,	/* 0 */
	BCM_OL_BEACON_ENABLE,
	BCM_OL_BEACON_DISABLE,
	BCM_OL_RSSI_INIT,
	BCM_OL_LOW_RSSI_NOTIFICATION,
	BCM_OL_ARP_ENABLE,
	BCM_OL_ARP_SETIP,
	BCM_OL_ARP_DISABLE,
	BCM_OL_ND_ENABLE,
	BCM_OL_ND_SETIP,
	BCM_OL_ND_DISABLE,
	BCM_OL_PKT_FILTER_ENABLE,
	BCM_OL_PKT_FILTER_ADD,
	BCM_OL_PKT_FILTER_DISABLE,
	BCM_OL_WOWL_ENABLE_START,
	BCM_OL_WOWL_ENABLE_COMPLETE,
	BCM_OL_WOWL_ENABLE_COMPLETED,
	BCM_OL_ARM_TX,
	BCM_OL_ARM_TX_DONE,
	BCM_OL_RESET,
	BCM_OL_FIFODEL,
	BCM_OL_MSG_TEST,
	BCM_OL_MSG_IE_NOTIFICATION_FLAG,
	BCM_OL_MSG_IE_NOTIFICATION,
	BCM_OL_SCAN_ENAB,
	BCM_OL_SCAN,
	BCM_OL_SCAN_RESULTS,
	BCM_OL_SCAN_CONFIG,
	BCM_OL_SCAN_BSS,
	BCM_OL_SCAN_QUIET,
	BCM_OL_SCAN_VALID2G,
	BCM_OL_SCAN_VALID5G,
	BCM_OL_SCAN_CHANSPECS,
	BCM_OL_SCAN_BSSID,
	BCM_OL_MACADDR,
	BCM_OL_SCAN_TXRXCHAIN,
	BCM_OL_SCAN_COUNTRY,
	BCM_OL_SCAN_PARAMS,
	BCM_OL_SSIDS,
	BCM_OL_PREFSSIDS,
	BCM_OL_PFN_LIST,
	BCM_OL_PFN_ADD,
	BCM_OL_PFN_DEL,
	BCM_OL_ULP,
	BCM_OL_CURPWR,
	BCM_OL_SARLIMIT,
	BCM_OL_TXCORE,
	BCM_OL_SCAN_DUMP,
	BCM_OL_MSGLEVEL,
	BCM_OL_MSGLEVEL2,
	BCM_OL_DMA_DUMP,
	BCM_OL_BCNS_PROMISC,
	BCM_OL_SETCHANNEL,
	BCM_OL_L2KEEPALIVE_ENABLE,
	BCM_OL_GTK_UPD,
	BCM_OL_GTK_ENABLE,
	BCM_OL_LTR,
	BCM_OL_TCP_KEEP_CONN,
	BCM_OL_TCP_KEEP_TIMERS,
	BCM_OL_EL_START,
	BCM_OL_EL_SEND_REPORT,
	BCM_OL_EL_REPORT,
	BCM_OL_PANIC,
	BCM_OL_CONS,
	BCM_OL_MSG_MAX /* Keep this last */
};

/* L2 Keepalive feature flags */
enum {
	BCM_OL_KEEPALIVE_RX_SILENT_DISCARD = 1<<0,
	BCM_OL_KEEPALIVE_PERIODIC_TX = 1<<1,
	BCM_OL_KEEPALIVE_PERIODIC_TX_QOS = 1<<2
};

#include <packed_section_start.h>

typedef BWL_PRE_PACKED_STRUCT struct ol_tkip_info {
	uint16		phase1[TKHASH_P1_KEY_SIZE/sizeof(uint16)];	/* tkhash phase1 result */
	uint8		PAD[2];
} BWL_POST_PACKED_STRUCT ol_tkip_info_t;

typedef BWL_PRE_PACKED_STRUCT struct iv {
	uint32		hi;	/* upper 32 bits of IV */
	uint16		lo;	/* lower 16 bits of IV */
	uint16		PAD;
} BWL_POST_PACKED_STRUCT iv_t;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_arp_stats_t {
	struct ipv4_addr src_ip;
	struct ipv4_addr dest_ip;
	uint8 suppressed;
	uint8 is_request;
	uint8 resp_sent;
	uint8 armtx;
} BWL_POST_PACKED_STRUCT olmsg_arp_stats;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_nd_stats_t {
	struct ipv6_addr dest_ip;
	uint8 suppressed;
	uint8 is_request;
	uint8 resp_sent;
	uint8 armtx;
} BWL_POST_PACKED_STRUCT olmsg_nd_stats;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_pkt_filter_stats_t {
	uint8	suppressed;
	uint8	pkt_filtered;
	uint8	matched_pattern;
	uint8	matched_magic;
	uint32	pattern_id;
} BWL_POST_PACKED_STRUCT olmsg_pkt_filter_stats;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_txpkt_status_t {
	uint32	tottxpkt;
	uint32	datafrm;
	uint32	nullfrm;
	uint32	pspoll;
	uint32	probereq;
	uint32	txacked;
	uint32	txsupressed;
} BWL_POST_PACKED_STRUCT olmsg_txpkt_status;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_rxpkt_status_t {
	uint32	totrxpkt;
	uint32	mgmtfrm;
	uint32	datafrm;
	uint32	scanprocessfrm;
	uint32	unassfrmdrop;
	uint32	badfcs;
	uint32	badrxchan;
	uint32	badfrmlen;
} BWL_POST_PACKED_STRUCT olmsg_rxpkt_status;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_rssi_t {
	uint8	rssi;			/* This is where we will keep caluculated RSSI Average */
	uint8	noise_avg;		/* This is where we will keep caluculated Noise Average */
	uint8	rxpwr[WL_RSSI_ANT_MAX];	/* 8 bit value for multiple Antennas */
	uint8	PAD[2];
} BWL_POST_PACKED_STRUCT olmsg_rssi;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_dump_stats_info_t {
	uint32 	rxoe_bcncnt;
	uint32 	rxoe_bcntohost;
	uint32 	rxoe_bcndelcnt;
	uint32 	rxoe_obssidcnt;
	uint32 	rxoe_capchangedcnt;
	uint32 	rxoe_bcnintchangedcnt;
	uint32	rxoe_bcnlosscnt;
	uint32	rxoe_tx_stopcnt;
	uint16	rxoe_iechanged[OLMSG_BCN_MAX_IE];
	uint16	rxoe_arp_statidx;
	uint16	PAD;
	uint32 	rxoe_totalarpcnt;
	uint32	rxoe_arpcnt;	/* arp received for US */
	uint32 	rxoe_arpsupresscnt;
	uint32 	rxoe_arpresponsecnt;
	olmsg_arp_stats rxoe_arp_stats[MAX_STAT_ENTRIES];
	uint16 	rxoe_nd_statidx;
	uint16	PAD;
	uint32 	rxoe_totalndcnt;
	uint32	rxoe_nscnt;
	uint32 	rxoe_nssupresscnt;
	uint32 	rxoe_nsresponsecnt;
	olmsg_nd_stats	rxoe_nd_stats[MAX_STAT_ENTRIES];
	uint16	rxoe_pkt_filter_statidx;
	uint16	PAD;
	uint32 	rxoe_total_pkt_filter_cnt;
	uint32 	rxoe_total_matching_pattern_cnt;
	uint32 	rxoe_total_matching_magic_cnt;
	uint32 	rxoe_pkt_filter_supresscnt;
	olmsg_pkt_filter_stats rxoe_pkt_filter_stats[MAX_STAT_ENTRIES];
	olmsg_rxpkt_status	rxoe_rxpktcnt;
	olmsg_txpkt_status	rxoe_txpktcnt;
} BWL_POST_PACKED_STRUCT olmsg_dump_stats;

typedef BWL_PRE_PACKED_STRUCT struct vndriemask_info_t {
	union {
		struct ouidata {
		uint8	id[3];
		uint8	type;
		} b;
		uint32  mask;
	} oui;
} BWL_POST_PACKED_STRUCT vndriemask_info;

/* Read/Write Context */
typedef BWL_PRE_PACKED_STRUCT struct {
	uint32	offset;
	uint32	size;
	uint32	rx;
	uint32	wx;
} BWL_POST_PACKED_STRUCT volatile olmsg_buf_info;

/* TBD: Should be a  packed structure */
typedef BWL_PRE_PACKED_STRUCT struct olmsg_header_t {
	uint32 type;
	uint32 seq;
	uint32 len;
} BWL_POST_PACKED_STRUCT olmsg_header;


typedef BWL_PRE_PACKED_STRUCT struct olmsg_test_t {
	olmsg_header hdr;
	uint32 data;
} BWL_POST_PACKED_STRUCT olmsg_test;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_bmac_up_t {
	olmsg_header hdr;
} BWL_POST_PACKED_STRUCT olmsg_bmac_up;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_reset_t {
	olmsg_header hdr;
} BWL_POST_PACKED_STRUCT olmsg_reset;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_fifodel_t {
	olmsg_header hdr;
	uint8 enable;
} BWL_POST_PACKED_STRUCT olmsg_fifodel;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_bcn_enable_t {
	olmsg_header    hdr;
	/* Deferral count to inform ucode */
	uint32	defcnt;

	/* BSSID beacon length */
	uint32	bcn_length;
	/* BSSID to support per interface */
	struct  ether_addr      BSSID;          /* Associated with BSSID */
	struct  ether_addr      cur_etheraddr;  /* Current Ethernet Address */

	/* beacon interval  */
	uint16	bi; /* units are Kusec */

	/* Beacon capability */
	uint16 capability;

	/* Control channel */
	uint32	ctrl_channel;

	/* association aid */
	uint16	aid;

	uint8 	frame_del;

	uint8	iemask[IEMASK_SZ];

	vndriemask_info	vndriemask[MAX_VNDR_IE];

	uint32	iedatalen;

	uint8	iedata[1];			/* Elements */
} BWL_POST_PACKED_STRUCT olmsg_bcn_enable;


typedef BWL_PRE_PACKED_STRUCT struct olmsg_bcn_disable_t {
	olmsg_header hdr;
	struct  ether_addr      BSSID;          /* Associated with BSSID */
} BWL_POST_PACKED_STRUCT olmsg_bcn_disable;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_rssi_init_t {
	olmsg_header hdr;
	uint8	enabled;		/* Flag to indicate RSSI enabled */
	int8	low_threshold;		/* Low RSSI Threshold value */
	int8	roam_off;		/* Low RSSI roam enabled */
	uint8	mode;			/* Mode: MAX, MIN, AVG */
	uint8	phyrxchain;		/* Antenna chain */
	int8	phy_rssi_gain_error[WL_RSSI_ANT_MAX]; /* RSSI gain error */
	uint16	current_temp;		/* current temperature */
	uint16	raw_tempsense;		/* temperature from ROM */
	uint16	radio_chanspec;		/* Radio channel spec */
} BWL_POST_PACKED_STRUCT olmsg_rssi_init;

typedef BWL_PRE_PACKED_STRUCT struct ol_sec_igtk_info_t {
	uint16 id;	/* IGTK key id */
	uint16 ipn_hi;	/* key IPN */
	uint32 ipn_lo;	/* key IPN */
	uint16 key_len;
	uint8  key[BIP_KEY_SIZE];
} BWL_POST_PACKED_STRUCT ol_sec_igtk_info;

typedef BWL_PRE_PACKED_STRUCT struct ol_sec_info_t {
	uint8		idx;		/* key index in wsec_keys array */
	uint8		id;		/* key ID [0-3] */
	uint8		algo;		/* CRYPTO_ALGO_AES_CCM, CRYPTO_ALGO_WEP128, etc */
	uint8 		algo_hw;	/* cache for hw register */
	int8		iv_len;		/* IV length */
	int8 		icv_len;
	int8 		PAD[2];
	uint32		len;
	iv_t		txiv;		/* Tx IV */
	uint8		rcmta;		/* rcmta entry index, same as idx by default */
	uint8		PAD[3];
	uint8		data[DOT11_MAX_KEY_SIZE];	/* key data */
	iv_t		rxiv[NUMRXIVS];		/* Rx IV (one per TID) */
	uint8		tkip_tx_left[4];	/* not-mic'd bytes */
	uint8		tkip_tx_lefts;	/* # of not-mic'd bytes */
	uint8		PAD;
	uint16		tkip_tx_offset;	/* frag offset in frame */
	ol_tkip_info_t	tkip_tx;
	ol_tkip_info_t	tkip_rx;	/* tkip receive state */
	uint8		iv_initialized;
	uint8 		aes_mode;	/* cache for hw register */
	uint16		flags;		/* misc flags */
	uint32		tkip_rx_iv32;	/* upper 32 bits of rx IV used to calc phase1 */
	uint8		tkip_rx_ividx;	/* index of rxiv above iv32 belongs to */
	uint8		PAD;
	struct ether_addr 	ea;		/* per station */

} BWL_POST_PACKED_STRUCT ol_sec_info;

typedef BWL_PRE_PACKED_STRUCT struct ol_tx_info_t {
	uint32			wsec;
	ol_sec_info 		key;
	ol_sec_info 		defaultkeys[DEFAULT_KEYS];
	ol_sec_igtk_info	igtk;
	uint8			qos;
	uint8			hwmic;
	uint16			rate;
	uint16			chanspec;
	struct  ether_addr	BSSID;          /* Associated with BSSID */
	struct  ether_addr	cur_etheraddr;  /* Current Ethernet Address */
	uint32			aid;		/* association aid */
	uint16			PhyTxControlWord_0;
	uint16			PhyTxControlWord_1;
	uint16			PhyTxControlWord_2;
	uint32			key_rot_indx;
	uint8			replay_counter[EAPOL_KEY_REPLAY_LEN];
} BWL_POST_PACKED_STRUCT ol_tx_info;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_arp_enable_t {
	olmsg_header		hdr;
	struct ether_addr	host_mac;
	struct ipv4_addr	host_ip;
	uint32			wsec;
	ol_sec_info 		defaultkeys[DEFAULT_KEYS];
} BWL_POST_PACKED_STRUCT olmsg_arp_enable;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_arp_disable_t {
	olmsg_header hdr;
} BWL_POST_PACKED_STRUCT olmsg_arp_disable;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_arp_setip_t {
	olmsg_header 		hdr;
	struct	ipv4_addr	host_ip;
} BWL_POST_PACKED_STRUCT olmsg_arp_setip;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_tcp_keep_conn_t {
	olmsg_header 		hdr;
	wl_mtcpkeep_alive_conn_pkt_t	tcp_keepalive_conn;
} BWL_POST_PACKED_STRUCT olmsg_tcp_keep_conn;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_tcp_keep_timers_t {
	olmsg_header 		hdr;
	wl_mtcpkeep_alive_timers_pkt_t	tcp_keepalive_timers;
} BWL_POST_PACKED_STRUCT olmsg_tcp_keep_timers;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_nd_enable_t {
	olmsg_header 		hdr;
	struct ether_addr	host_mac;
	struct ipv6_addr	host_ip;
	uint32			wsec;
	ol_sec_info 		defaultkeys[DEFAULT_KEYS];
} BWL_POST_PACKED_STRUCT olmsg_nd_enable;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_nd_disable_t {
	olmsg_header		hdr;
} BWL_POST_PACKED_STRUCT olmsg_nd_disable;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_nd_setip_t {
	olmsg_header 		hdr;
	struct ipv6_addr	host_ip;
} BWL_POST_PACKED_STRUCT olmsg_nd_setip;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_armtx_t {
	olmsg_header hdr;
	uint8 TX;
    ol_tx_info txinfo;
} BWL_POST_PACKED_STRUCT olmsg_armtx;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_armtxdone_t {
	olmsg_header 	hdr;
	ol_tx_info 	txinfo;
} BWL_POST_PACKED_STRUCT olmsg_armtxdone;

/* Add IE NOTIFICATION STRUCT HERE */
typedef BWL_PRE_PACKED_STRUCT struct olmsg_ie_notification_enable_t {
	olmsg_header            hdr;            /* Message Header */
	struct  ether_addr      BSSID;          /* Associated with BSSID */
	struct  ether_addr      cur_etheraddr;  /* Current Ethernet Address */
	uint32                  id;             /* IE Mask for standard IE */
	uint32                  enable;         /* IE Mask enable/disable flag */

} BWL_POST_PACKED_STRUCT olmsg_ie_notification_enable;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_groupkeyupd_t {
	olmsg_header 	hdr;
	ol_sec_info 	defaultkeys[DEFAULT_KEYS];
} BWL_POST_PACKED_STRUCT olmsg_groupkeyupd;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_scan32_t {
	olmsg_header hdr;
	uint32 count;
	uint32 list[10];
} BWL_POST_PACKED_STRUCT olmsg_scan32;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_scanchspec_t {
	olmsg_header hdr;
	uint32 count;
	uint32 list[33];
} BWL_POST_PACKED_STRUCT olmsg_scanchspec;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_chanvec_t {
	olmsg_header hdr;
	chanvec_t chanvec;
} BWL_POST_PACKED_STRUCT olmsg_chanvec;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_country_t {
	olmsg_header hdr;
	char country[WLC_CNTRY_BUF_SZ];
} BWL_POST_PACKED_STRUCT olmsg_country;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_addr_t {
	olmsg_header hdr;
	struct ether_addr addr;
} BWL_POST_PACKED_STRUCT olmsg_addr;

#define MAX_SSID_CNT		16
typedef BWL_PRE_PACKED_STRUCT struct olmsg_ssids_t {
	olmsg_header hdr;
	wlc_ssid_t ssid[1];
} BWL_POST_PACKED_STRUCT olmsg_ssids;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_pfn_t {
	olmsg_header hdr;
	pfn_olmsg_params params;
} BWL_POST_PACKED_STRUCT olmsg_pfn;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_scanparams_t {
	olmsg_header hdr;
	scanol_params_t params;
} BWL_POST_PACKED_STRUCT olmsg_scanparams;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_pkt_filter_enable_t {
	olmsg_header		hdr;
	struct ether_addr	host_mac;
} BWL_POST_PACKED_STRUCT olmsg_pkt_filter_enable;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_pkt_filter_add_t {
	olmsg_header		hdr;
	wl_pkt_filter_t         pkt_filter;
} BWL_POST_PACKED_STRUCT olmsg_pkt_filter_add;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_pkt_filter_disable_t {
	olmsg_header hdr;
} BWL_POST_PACKED_STRUCT olmsg_pkt_filter_disable;

/*
 * Message from host to ARM: Notification of start of WoWL enable.
 * Other messages can be sent from host after this message, but those
 * messages cannot require a handshake message from ARM after WoWL is
 * enabled.
 */

/* WOWL cfg data passed to ARM by host driver */
typedef BWL_PRE_PACKED_STRUCT struct wowl_cfg {
	uint8		wowl_enabled;   /* TRUE iff WoWL is enabled by host driver */
	uint32		wowl_flags;     /* WL_WOWL_Xxx flags defined in wlioctl.h */
	uint32		wowl_test;      /* Wowl test: seconds to sleep before waking */
	uint32		bcn_loss;       /* Threshold for bcn loss before waking host */
	uint32		associated;     /* Whether we are entering wowl in assoc mode */
	uint32		PM;             /* PM mode for wowl */
} BWL_POST_PACKED_STRUCT wowl_cfg_t;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_wowl_enable_start_t {
	olmsg_header	hdr;
	wowl_cfg_t	wowl_cfg;
} BWL_POST_PACKED_STRUCT olmsg_wowl_enable_start;

/*
 * Message from host to ARM: Notification of end of WoWL enable.
 * This must be the last host message when WoWL is being enabled.
 */
typedef BWL_PRE_PACKED_STRUCT struct olmsg_wowl_enable_complete_t {
	olmsg_header	hdr;
} BWL_POST_PACKED_STRUCT olmsg_wowl_enable_complete;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_l2keepalive_enable {
	olmsg_header	hdr;
	uint16	period_ms;
	uint8	prio;
	uint8	flags;
} BWL_POST_PACKED_STRUCT olmsg_l2keepalive_enable_t;

typedef BWL_PRE_PACKED_STRUCT struct rsn_rekey_params_t {
	uint8	kck[WPA_MIC_KEY_LEN];
	uint8	kek[WPA_ENCR_KEY_LEN];
	uint8	replay_counter[EAPOL_KEY_REPLAY_LEN];	/* replay counter */
} BWL_POST_PACKED_STRUCT rsn_rekey_params;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_gtk_enable_t {
	olmsg_header	hdr;
	uint16 		WPA_auth;
	rsn_rekey_params	rekey;
	uint32 		flags;
	uint16		m_seckindxalgo_blk;
	uint		m_seckindxalgo_blk_sz;
	uint		seckeys; /* 54 key table shm address */
	uint		tkmickeys; /* 12 TKIP MIC key table shm address */
	int 		tx_tkmic_offset;
	int 		rx_tkmic_offset;
	uint32		igtk_enabled;
} BWL_POST_PACKED_STRUCT olmsg_gtk_enable;

/*
 * Message from ARM to host: Notification of completion of WoWL enable.
 * This notification informs the host that the ARM is now ready to operate
 * in a low-power state and will not access host memory.
 */
typedef BWL_PRE_PACKED_STRUCT struct olmsg_wowl_enable_completed_t {
	olmsg_header	hdr;
} BWL_POST_PACKED_STRUCT olmsg_wowl_enable_completed;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_low_rssi_notification_t {
	olmsg_header	hdr;
	int8		rssi;
} BWL_POST_PACKED_STRUCT olmsg_low_rssi_notification;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_eventlog_notification_t {
	olmsg_header	hdr;
	int8		num;
	int8		totalnum;
	int16		buflen;
	int8		buf[0];
} BWL_POST_PACKED_STRUCT olmsg_eventlog_notification;

#ifdef WL_LTR
typedef BWL_PRE_PACKED_STRUCT struct olmsg_ltr_t {
	olmsg_header	hdr;
	uint8 ltr_sw_en;		/* Set/clear to turn on/off LTR support in driver */
	uint8 ltr_hw_en;		/* Set/clear if LTR is enabled/disabled in hw */
	uint32 active_lat;		/* LTR active latency */
	uint32 active_idle_lat;	/* LTR active idle latency */
	uint32 sleep_lat;		/* LTR sleep latency */
	uint32 hi_wm;			/* FIFO watermark for LTR active transition */
	uint32 lo_wm;			/* FIFO watermark for LTR sleep transition */
} BWL_POST_PACKED_STRUCT olmsg_ltr;
#endif /* WL_LTR */

/*
 * The following structures define the format of the shared memory between
 * the ARM and the host driver.
 */
#ifndef ETHER_MAX_DATA
#define ETHER_MAX_DATA		    1500
#endif

#define MAX_WAKE_PACKET_BYTES	    (DOT11_A3_HDR_LEN +			    \
				     DOT11_QOS_LEN +			    \
				     sizeof(struct dot11_llc_snap_header) + \
				     ETHER_MAX_DATA)

/* WOWL info returned by ARM firmware to host driver */
typedef BWL_PRE_PACKED_STRUCT struct wowl_wake_pkt_info {
	uint32		pattern_id;		/* ID of pattern that packet matched */
	uint32		original_packet_size;	/* Original size of packet */
	uint32		phy_type;
	uint32		channel;
	uint32		rate;
	uint32		rssi;
} BWL_POST_PACKED_STRUCT wowl_wake_info_t;

typedef BWL_PRE_PACKED_STRUCT struct scan_wake_pkt_info {
	wlc_ssid_t		ssid;			/* ssid that matched */
	uint16			capability;		/* Capability information */
	chanspec_t		chanspec;		/* Capability information */
	uint32			rssi;
	struct rsn_parms	wpa;
	struct rsn_parms	wpa2;
} BWL_POST_PACKED_STRUCT scan_wake_pkt_info_t;

typedef BWL_PRE_PACKED_STRUCT struct wowl_host_info {
	uint32		wake_reason;	/* WL_WOWL_Xxx value */
	uint8		eventlog[MAX_OL_EVENTS];
	uint32		eventidx;
	union {
		wowl_wake_info_t	pattern_pkt_info;
		scan_wake_pkt_info_t	scan_pkt_info;
	} u;
	uint32          pkt_len;
	uchar           pkt[ETHER_MAX_DATA];
	olmsg_armtxdone	wake_tx_info;	/* Tx done settings returned to the host */
} BWL_POST_PACKED_STRUCT wowl_host_info_t;

#define MDNS_DBASE_SZ	4096
typedef BWL_PRE_PACKED_STRUCT struct {
	uint32		    marker_begin;
	uint32		    msgbufaddr_low;
	uint32		    msgbufaddr_high;
	uint32		    msgbuf_sz;
	uint32		    console_addr;
	/* flag[] is boolean array; and turn is an integer */
	uint32		    flag[2];
	uint32		    turn;
	uint32		    vars_size;
	uint8		    vars[MAXSZ_NVRAM_VARS];
	uint32		    mdns_len;
	uint8		    mdns_dbase[MDNS_DBASE_SZ];
	uint32		    dngl_watchdog_cntr;
	olmsg_rssi	    rssi_info;
	uint32		    eventlog_addr;
	olmsg_dump_stats    stats;
	wowl_host_info_t    wowl_host_info;
	uint32		    marker_end;
} BWL_POST_PACKED_STRUCT volatile olmsg_shared_info_t;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_curpwr_t {
	olmsg_header hdr;
	uint8 curpwr[MAXCHANNEL];
} BWL_POST_PACKED_STRUCT olmsg_curpwr;

typedef BWL_PRE_PACKED_STRUCT struct olmsg_sarlimit_t {
	olmsg_header hdr;
	sar_limit_t sarlimit;
} BWL_POST_PACKED_STRUCT olmsg_sarlimit;

#define CMDLINESZ	80

typedef BWL_PRE_PACKED_STRUCT struct olmsg_ol_conscmd_t {
	olmsg_header hdr;
	uint8 cmdline[CMDLINESZ];
} BWL_POST_PACKED_STRUCT olmsg_ol_conscmd;

/* Message buffer start addreses is written at the end of
 * ARM memroy, 32 bytes additional.
 */
#define sharedsz		(sizeof(olmsg_shared_info_t) + 32)
#define OLMSG_SHARED_INFO_SZ	ROUNDUP(sharedsz, sizeof(uint32))

/* Modify below if the size changes */
#define OLMSG_SHARED_INFO_SZ_NUM  (11936)
#include <packed_section_end.h>

typedef struct olmsg_info_t {
	uchar	*msg_buff;
	uint32	len;
	olmsg_buf_info *write;
	olmsg_buf_info *read;
	uint32	next_seq;
} olmsg_info;

#ifdef BCM_OL_DEV
/* This needs to be moved to private place */
extern olmsg_shared_info_t *ppcie_shared;
#define RXOEINC(a)	(ppcie_shared->stats.a++)
#define RXOEINCCNTR() (ppcie_shared->dngl_watchdog_cntr++)
#define RXOEINCIE(a, i) ((ppcie_shared->stats.a[i])++)

#define RXOEINC_N(a, n) (a = ((a + 1) & ((n) - 1)))
#define RXOEADDARPENTRY(entry) ({ uint8 i = ppcie_shared->stats.rxoe_arp_statidx; \
	bcopy(&entry, \
		(void *)&ppcie_shared->stats.rxoe_arp_stats[i], \
		sizeof(olmsg_arp_stats)); \
	RXOEINC_N(ppcie_shared->stats.rxoe_arp_statidx, MAX_STAT_ENTRIES); })
#define RXOEADDNDENTRY(entry) ({ uint8 i = ppcie_shared->stats.rxoe_nd_statidx; \
	bcopy(&entry, \
		(void *)&ppcie_shared->stats.rxoe_nd_stats[i], \
		sizeof(olmsg_nd_stats)); \
	RXOEINC_N(ppcie_shared->stats.rxoe_nd_statidx, MAX_STAT_ENTRIES); })
#define RXOEADD_PKT_FILTER_ENTRY(entry) ({ uint8 i = ppcie_shared->stats.rxoe_pkt_filter_statidx; \
	bcopy(&entry, \
		(void *)&ppcie_shared->stats.rxoe_pkt_filter_stats[i], \
		sizeof(olmsg_pkt_filter_stats)); \
	RXOEINC_N(ppcie_shared->stats.rxoe_pkt_filter_statidx, MAX_STAT_ENTRIES); })
#define RXOEUPDREPLAYCNT(rcnt)({bcopy((uint8 *)(rcnt), \
	(uint8 *)(ppcie_shared->wowl_host_info.wake_tx_info.txinfo.replay_counter), \
	EAPOL_KEY_REPLAY_LEN);})
#define RXOEUPDTXIV(l, h) ({ppcie_shared->wowl_host_info.wake_tx_info.txinfo.key.txiv.hi = h; \
	ppcie_shared->wowl_host_info.wake_tx_info.txinfo.key.txiv.lo = l;})
#define RXOEUPDRXIV_UC(l, h, prio) \
	({ppcie_shared->wowl_host_info.wake_tx_info.txinfo.key.rxiv[prio].hi = h; \
	ppcie_shared->wowl_host_info.wake_tx_info.txinfo.key.rxiv[prio].lo = l;})
#define RXOEUPDRXIV_BC(l, h, prio, keyid) \
	({ppcie_shared->wowl_host_info.wake_tx_info.txinfo.defaultkeys[keyid].rxiv[prio].hi = h; \
	ppcie_shared->wowl_host_info.wake_tx_info.txinfo.defaultkeys[keyid].rxiv[prio].lo = l;})
#define RXOEUPDTXPH1(p1) ({bcopy((uint8 *)(p1), \
	(uint8 *)(ppcie_shared->wowl_host_info.wake_tx_info.txinfo.key.tkip_tx.phase1), \
	TKHASH_P1_KEY_SIZE);})
#define RXOEUPDRXPH1_UC(p1) ({bcopy((uint8 *)(p1), \
	(uint8 *)(ppcie_shared->wowl_host_info.wake_tx_info.txinfo.key.tkip_rx.phase1), \
	TKHASH_P1_KEY_SIZE);})
#define RXOEUPDRXPH1_BC(p1, keyid) ({bcopy((uint8 *)(p1), \
	(uint8 *)\
	(ppcie_shared->wowl_host_info.wake_tx_info.txinfo.defaultkeys[keyid].tkip_rx.phase1), \
	TKHASH_P1_KEY_SIZE);})
#define RXOEUPDTXINFO(tinfo) (bcopy((uint8 *)(tinfo), \
	(uint8 *)(&ppcie_shared->wowl_host_info.wake_tx_info.txinfo), \
	sizeof(ol_tx_info)));
#endif	/* BCM_OL_DEV */
extern int
bcm_olmsg_create(uchar *buf, uint32 len);

/* Initialize message buffer */
extern int
bcm_olmsg_init(olmsg_info *ol_info, uchar *buf, uint32 len, uint8 rx, uint8 wx);

extern void
bcm_olmsg_deinit(olmsg_info *ol_info);

/* Copies the next message to be read into buf
	Updates the read pointer
	returns size of the message
	Pass NULL to retrieve the size of the message
 */
extern int
bcm_olmsg_getnext(olmsg_info *ol_info, char *buf, uint32 size);

/* same as bcm_olmsg_getnext, except that read pointer it not updated
 */
int
bcm_olmsg_peeknext(olmsg_info *ol_info, char *buf, uint32 size);


/* Writes the message to the shared buffer
	Updates the write pointer
	returns TRUE/FALSE depending on the availability of the space
	Pass NULL to retrieve the size of the message
 */
extern bool
bcm_olmsg_write(olmsg_info *ol_info, char *buf, uint32 size);

/*
 * Returns free space of the message buffer
 */
extern uint32
bcm_olmsg_avail(olmsg_info *ol_info);

extern bool
bcm_olmsg_is_writebuf_full(olmsg_info *ol_info);

extern bool
bcm_olmsg_is_writebuf_empty(olmsg_info *ol_info);

extern int
bcm_olmsg_writemsg(olmsg_info *ol, uchar *buf, uint16 len);

extern uint32
bcm_olmsg_bytes_to_read(olmsg_info *ol_info);

extern bool
bcm_olmsg_is_readbuf_empty(olmsg_info *ol_info);


extern uint32
bcm_olmsg_peekbytes(olmsg_info *ol, uchar *dst, uint32 len);

extern uint32
bcm_olmsg_readbytes(olmsg_info *ol, uchar *dst, uint32 len);

extern uint16
bcm_olmsg_peekmsg_len(olmsg_info *ol);

extern uint16
bcm_olmsg_readmsg(olmsg_info *ol, uchar *buf, uint16 len);

extern void
bcm_olmsg_dump_msg(olmsg_info *ol, olmsg_header *hdr);

extern void
bcm_olmsg_dump_record(olmsg_info *ol);

#endif /* _BCM_OL_MSG_H_ */
