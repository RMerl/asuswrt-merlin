/*
 * This file is part of wl1271
 *
 * Copyright (C) 1998-2009 Texas Instruments. All rights reserved.
 * Copyright (C) 2008-2010 Nokia Corporation
 *
 * Contact: Luciano Coelho <luciano.coelho@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __WL1271_ACX_H__
#define __WL1271_ACX_H__

#include "wl1271.h"
#include "wl1271_cmd.h"

/*************************************************************************

    Host Interrupt Register (WiLink -> Host)

**************************************************************************/
/* HW Initiated interrupt Watchdog timer expiration */
#define WL1271_ACX_INTR_WATCHDOG           BIT(0)
/* Init sequence is done (masked interrupt, detection through polling only ) */
#define WL1271_ACX_INTR_INIT_COMPLETE      BIT(1)
/* Event was entered to Event MBOX #A*/
#define WL1271_ACX_INTR_EVENT_A            BIT(2)
/* Event was entered to Event MBOX #B*/
#define WL1271_ACX_INTR_EVENT_B            BIT(3)
/* Command processing completion*/
#define WL1271_ACX_INTR_CMD_COMPLETE       BIT(4)
/* Signaling the host on HW wakeup */
#define WL1271_ACX_INTR_HW_AVAILABLE       BIT(5)
/* The MISC bit is used for aggregation of RX, TxComplete and TX rate update */
#define WL1271_ACX_INTR_DATA               BIT(6)
/* Trace meassge on MBOX #A */
#define WL1271_ACX_INTR_TRACE_A            BIT(7)
/* Trace meassge on MBOX #B */
#define WL1271_ACX_INTR_TRACE_B            BIT(8)

#define WL1271_ACX_INTR_ALL		   0xFFFFFFFF
#define WL1271_ACX_ALL_EVENTS_VECTOR       (WL1271_ACX_INTR_WATCHDOG      | \
					    WL1271_ACX_INTR_INIT_COMPLETE | \
					    WL1271_ACX_INTR_EVENT_A       | \
					    WL1271_ACX_INTR_EVENT_B       | \
					    WL1271_ACX_INTR_CMD_COMPLETE  | \
					    WL1271_ACX_INTR_HW_AVAILABLE  | \
					    WL1271_ACX_INTR_DATA)

#define WL1271_INTR_MASK                   (WL1271_ACX_INTR_EVENT_A      | \
					    WL1271_ACX_INTR_EVENT_B      | \
					    WL1271_ACX_INTR_HW_AVAILABLE | \
					    WL1271_ACX_INTR_DATA)

/* Target's information element */
struct acx_header {
	struct wl1271_cmd_header cmd;

	/* acx (or information element) header */
	__le16 id;

	/* payload length (not including headers */
	__le16 len;
} __packed;

struct acx_error_counter {
	struct acx_header header;

	/* The number of PLCP errors since the last time this */
	/* information element was interrogated. This field is */
	/* automatically cleared when it is interrogated.*/
	__le32 PLCP_error;

	/* The number of FCS errors since the last time this */
	/* information element was interrogated. This field is */
	/* automatically cleared when it is interrogated.*/
	__le32 FCS_error;

	/* The number of MPDUs without PLCP header errors received*/
	/* since the last time this information element was interrogated. */
	/* This field is automatically cleared when it is interrogated.*/
	__le32 valid_frame;

	/* the number of missed sequence numbers in the squentially */
	/* values of frames seq numbers */
	__le32 seq_num_miss;
} __packed;

struct acx_revision {
	struct acx_header header;

	/*
	 * The WiLink firmware version, an ASCII string x.x.x.x,
	 * that uniquely identifies the current firmware.
	 * The left most digit is incremented each time a
	 * significant change is made to the firmware, such as
	 * code redesign or new platform support.
	 * The second digit is incremented when major enhancements
	 * are added or major fixes are made.
	 * The third digit is incremented for each GA release.
	 * The fourth digit is incremented for each build.
	 * The first two digits identify a firmware release version,
	 * in other words, a unique set of features.
	 * The first three digits identify a GA release.
	 */
	char fw_version[20];

	/*
	 * This 4 byte field specifies the WiLink hardware version.
	 * bits 0  - 15: Reserved.
	 * bits 16 - 23: Version ID - The WiLink version ID
	 *              (1 = first spin, 2 = second spin, and so on).
	 * bits 24 - 31: Chip ID - The WiLink chip ID.
	 */
	__le32 hw_version;
} __packed;

enum wl1271_psm_mode {
	/* Active mode */
	WL1271_PSM_CAM = 0,

	/* Power save mode */
	WL1271_PSM_PS = 1,

	/* Extreme low power */
	WL1271_PSM_ELP = 2,
};

struct acx_sleep_auth {
	struct acx_header header;

	/* The sleep level authorization of the device. */
	/* 0 - Always active*/
	/* 1 - Power down mode: light / fast sleep*/
	/* 2 - ELP mode: Deep / Max sleep*/
	u8  sleep_auth;
	u8  padding[3];
} __packed;

enum {
	HOSTIF_PCI_MASTER_HOST_INDIRECT,
	HOSTIF_PCI_MASTER_HOST_DIRECT,
	HOSTIF_SLAVE,
	HOSTIF_PKT_RING,
	HOSTIF_DONTCARE = 0xFF
};

#define DEFAULT_UCAST_PRIORITY          0
#define DEFAULT_RX_Q_PRIORITY           0
#define DEFAULT_NUM_STATIONS            1
#define DEFAULT_RXQ_PRIORITY            0 /* low 0 .. 15 high  */
#define DEFAULT_RXQ_TYPE                0x07    /* All frames, Data/Ctrl/Mgmt */
#define TRACE_BUFFER_MAX_SIZE           256

#define  DP_RX_PACKET_RING_CHUNK_SIZE 1600
#define  DP_TX_PACKET_RING_CHUNK_SIZE 1600
#define  DP_RX_PACKET_RING_CHUNK_NUM 2
#define  DP_TX_PACKET_RING_CHUNK_NUM 2
#define  DP_TX_COMPLETE_TIME_OUT 20

#define TX_MSDU_LIFETIME_MIN       0
#define TX_MSDU_LIFETIME_MAX       3000
#define TX_MSDU_LIFETIME_DEF       512
#define RX_MSDU_LIFETIME_MIN       0
#define RX_MSDU_LIFETIME_MAX       0xFFFFFFFF
#define RX_MSDU_LIFETIME_DEF       512000

struct acx_rx_msdu_lifetime {
	struct acx_header header;

	/*
	 * The maximum amount of time, in TU, before the
	 * firmware discards the MSDU.
	 */
	__le32 lifetime;
} __packed;

/*
 * RX Config Options Table
 * Bit		Definition
 * ===		==========
 * 31:14		Reserved
 * 13		Copy RX Status - when set, write three receive status words
 *		to top of rx'd MPDUs.
 *		When cleared, do not write three status words (added rev 1.5)
 * 12		Reserved
 * 11		RX Complete upon FCS error - when set, give rx complete
 *		interrupt for FCS errors, after the rx filtering, e.g. unicast
 *		frames not to us with FCS error will not generate an interrupt.
 * 10		SSID Filter Enable - When set, the WiLink discards all beacon,
 *	        probe request, and probe response frames with an SSID that does
 *		not match the SSID specified by the host in the START/JOIN
 *		command.
 *		When clear, the WiLink receives frames with any SSID.
 * 9		Broadcast Filter Enable - When set, the WiLink discards all
 *		broadcast frames. When clear, the WiLink receives all received
 *		broadcast frames.
 * 8:6		Reserved
 * 5		BSSID Filter Enable - When set, the WiLink discards any frames
 *		with a BSSID that does not match the BSSID specified by the
 *		host.
 *		When clear, the WiLink receives frames from any BSSID.
 * 4		MAC Addr Filter - When set, the WiLink discards any frames
 *		with a destination address that does not match the MAC address
 *		of the adaptor.
 *		When clear, the WiLink receives frames destined to any MAC
 *		address.
 * 3		Promiscuous - When set, the WiLink receives all valid frames
 *		(i.e., all frames that pass the FCS check).
 *		When clear, only frames that pass the other filters specified
 *		are received.
 * 2		FCS - When set, the WiLink includes the FCS with the received
 *		frame.
 *		When cleared, the FCS is discarded.
 * 1		PLCP header - When set, write all data from baseband to frame
 *		buffer including PHY header.
 * 0		Reserved - Always equal to 0.
 *
 * RX Filter Options Table
 * Bit		Definition
 * ===		==========
 * 31:12		Reserved - Always equal to 0.
 * 11		Association - When set, the WiLink receives all association
 *		related frames (association request/response, reassocation
 *		request/response, and disassociation). When clear, these frames
 *		are discarded.
 * 10		Auth/De auth - When set, the WiLink receives all authentication
 *		and de-authentication frames. When clear, these frames are
 *		discarded.
 * 9		Beacon - When set, the WiLink receives all beacon frames.
 *		When clear, these frames are discarded.
 * 8		Contention Free - When set, the WiLink receives all contention
 *		free frames.
 *		When clear, these frames are discarded.
 * 7		Control - When set, the WiLink receives all control frames.
 *		When clear, these frames are discarded.
 * 6		Data - When set, the WiLink receives all data frames.
 *		When clear, these frames are discarded.
 * 5		FCS Error - When set, the WiLink receives frames that have FCS
 *		errors.
 *		When clear, these frames are discarded.
 * 4		Management - When set, the WiLink receives all management
 *		frames.
 *		When clear, these frames are discarded.
 * 3		Probe Request - When set, the WiLink receives all probe request
 *		frames.
 *		When clear, these frames are discarded.
 * 2		Probe Response - When set, the WiLink receives all probe
 *		response frames.
 *		When clear, these frames are discarded.
 * 1		RTS/CTS/ACK - When set, the WiLink receives all RTS, CTS and ACK
 *		frames.
 *		When clear, these frames are discarded.
 * 0		Rsvd Type/Sub Type - When set, the WiLink receives all frames
 *		that have reserved frame types and sub types as defined by the
 *		802.11 specification.
 *		When clear, these frames are discarded.
 */
struct acx_rx_config {
	struct acx_header header;

	__le32 config_options;
	__le32 filter_options;
} __packed;

struct acx_packet_detection {
	struct acx_header header;

	__le32 threshold;
} __packed;


enum acx_slot_type {
	SLOT_TIME_LONG = 0,
	SLOT_TIME_SHORT = 1,
	DEFAULT_SLOT_TIME = SLOT_TIME_SHORT,
	MAX_SLOT_TIMES = 0xFF
};

#define STATION_WONE_INDEX 0

struct acx_slot {
	struct acx_header header;

	u8 wone_index; /* Reserved */
	u8 slot_time;
	u8 reserved[6];
} __packed;


#define ACX_MC_ADDRESS_GROUP_MAX	(8)
#define ADDRESS_GROUP_MAX_LEN	        (ETH_ALEN * ACX_MC_ADDRESS_GROUP_MAX)

struct acx_dot11_grp_addr_tbl {
	struct acx_header header;

	u8 enabled;
	u8 num_groups;
	u8 pad[2];
	u8 mac_table[ADDRESS_GROUP_MAX_LEN];
} __packed;

struct acx_rx_timeout {
	struct acx_header header;

	__le16 ps_poll_timeout;
	__le16 upsd_timeout;
} __packed;

struct acx_rts_threshold {
	struct acx_header header;

	__le16 threshold;
	u8 pad[2];
} __packed;

struct acx_beacon_filter_option {
	struct acx_header header;

	u8 enable;

	/*
	 * The number of beacons without the unicast TIM
	 * bit set that the firmware buffers before
	 * signaling the host about ready frames.
	 * When set to 0 and the filter is enabled, beacons
	 * without the unicast TIM bit set are dropped.
	 */
	u8 max_num_beacons;
	u8 pad[2];
} __packed;

/*
 * ACXBeaconFilterEntry (not 221)
 * Byte Offset     Size (Bytes)    Definition
 * ===========     ============    ==========
 * 0               1               IE identifier
 * 1               1               Treatment bit mask
 *
 * ACXBeaconFilterEntry (221)
 * Byte Offset     Size (Bytes)    Definition
 * ===========     ============    ==========
 * 0               1               IE identifier
 * 1               1               Treatment bit mask
 * 2               3               OUI
 * 5               1               Type
 * 6               2               Version
 *
 *
 * Treatment bit mask - The information element handling:
 * bit 0 - The information element is compared and transferred
 * in case of change.
 * bit 1 - The information element is transferred to the host
 * with each appearance or disappearance.
 * Note that both bits can be set at the same time.
 */
#define	BEACON_FILTER_TABLE_MAX_IE_NUM		       (32)
#define BEACON_FILTER_TABLE_MAX_VENDOR_SPECIFIC_IE_NUM (6)
#define BEACON_FILTER_TABLE_IE_ENTRY_SIZE	       (2)
#define BEACON_FILTER_TABLE_EXTRA_VENDOR_SPECIFIC_IE_SIZE (6)
#define BEACON_FILTER_TABLE_MAX_SIZE ((BEACON_FILTER_TABLE_MAX_IE_NUM * \
			    BEACON_FILTER_TABLE_IE_ENTRY_SIZE) + \
			   (BEACON_FILTER_TABLE_MAX_VENDOR_SPECIFIC_IE_NUM * \
			    BEACON_FILTER_TABLE_EXTRA_VENDOR_SPECIFIC_IE_SIZE))

struct acx_beacon_filter_ie_table {
	struct acx_header header;

	u8 num_ie;
	u8 pad[3];
	u8 table[BEACON_FILTER_TABLE_MAX_SIZE];
} __packed;

struct acx_conn_monit_params {
       struct acx_header header;

       __le32 synch_fail_thold; /* number of beacons missed */
       __le32 bss_lose_timeout; /* number of TU's from synch fail */
} __packed;

struct acx_bt_wlan_coex {
	struct acx_header header;

	u8 enable;
	u8 pad[3];
} __packed;

struct acx_bt_wlan_coex_param {
	struct acx_header header;

	__le32 params[CONF_SG_PARAMS_MAX];
	u8 param_idx;
	u8 padding[3];
} __packed;

struct acx_dco_itrim_params {
	struct acx_header header;

	u8 enable;
	u8 padding[3];
	__le32 timeout;
} __packed;

struct acx_energy_detection {
	struct acx_header header;

	/* The RX Clear Channel Assessment threshold in the PHY */
	__le16 rx_cca_threshold;
	u8 tx_energy_detection;
	u8 pad;
} __packed;

struct acx_beacon_broadcast {
	struct acx_header header;

	__le16 beacon_rx_timeout;
	__le16 broadcast_timeout;

	/* Enables receiving of broadcast packets in PS mode */
	u8 rx_broadcast_in_ps;

	/* Consecutive PS Poll failures before updating the host */
	u8 ps_poll_threshold;
	u8 pad[2];
} __packed;

struct acx_event_mask {
	struct acx_header header;

	__le32 event_mask;
	__le32 high_event_mask; /* Unused */
} __packed;

#define CFG_RX_FCS		BIT(2)
#define CFG_RX_ALL_GOOD		BIT(3)
#define CFG_UNI_FILTER_EN	BIT(4)
#define CFG_BSSID_FILTER_EN	BIT(5)
#define CFG_MC_FILTER_EN	BIT(6)
#define CFG_MC_ADDR0_EN		BIT(7)
#define CFG_MC_ADDR1_EN		BIT(8)
#define CFG_BC_REJECT_EN	BIT(9)
#define CFG_SSID_FILTER_EN	BIT(10)
#define CFG_RX_INT_FCS_ERROR	BIT(11)
#define CFG_RX_INT_ENCRYPTED	BIT(12)
#define CFG_RX_WR_RX_STATUS	BIT(13)
#define CFG_RX_FILTER_NULTI	BIT(14)
#define CFG_RX_RESERVE		BIT(15)
#define CFG_RX_TIMESTAMP_TSF	BIT(16)

#define CFG_RX_RSV_EN		BIT(0)
#define CFG_RX_RCTS_ACK		BIT(1)
#define CFG_RX_PRSP_EN		BIT(2)
#define CFG_RX_PREQ_EN		BIT(3)
#define CFG_RX_MGMT_EN		BIT(4)
#define CFG_RX_FCS_ERROR	BIT(5)
#define CFG_RX_DATA_EN		BIT(6)
#define CFG_RX_CTL_EN		BIT(7)
#define CFG_RX_CF_EN		BIT(8)
#define CFG_RX_BCN_EN		BIT(9)
#define CFG_RX_AUTH_EN		BIT(10)
#define CFG_RX_ASSOC_EN		BIT(11)

#define SCAN_PASSIVE		BIT(0)
#define SCAN_5GHZ_BAND		BIT(1)
#define SCAN_TRIGGERED		BIT(2)
#define SCAN_PRIORITY_HIGH	BIT(3)

/* When set, disable HW encryption */
#define DF_ENCRYPTION_DISABLE      0x01
#define DF_SNIFF_MODE_ENABLE       0x80

struct acx_feature_config {
	struct acx_header header;

	__le32 options;
	__le32 data_flow_options;
} __packed;

struct acx_current_tx_power {
	struct acx_header header;

	u8  current_tx_power;
	u8  padding[3];
} __packed;

struct acx_wake_up_condition {
	struct acx_header header;

	u8 wake_up_event; /* Only one bit can be set */
	u8 listen_interval;
	u8 pad[2];
} __packed;

struct acx_aid {
	struct acx_header header;

	/*
	 * To be set when associated with an AP.
	 */
	__le16 aid;
	u8 pad[2];
} __packed;

enum acx_preamble_type {
	ACX_PREAMBLE_LONG = 0,
	ACX_PREAMBLE_SHORT = 1
};

struct acx_preamble {
	struct acx_header header;

	/*
	 * When set, the WiLink transmits the frames with a short preamble and
	 * when cleared, the WiLink transmits the frames with a long preamble.
	 */
	u8 preamble;
	u8 padding[3];
} __packed;

enum acx_ctsprotect_type {
	CTSPROTECT_DISABLE = 0,
	CTSPROTECT_ENABLE = 1
};

struct acx_ctsprotect {
	struct acx_header header;
	u8 ctsprotect;
	u8 padding[3];
} __packed;

struct acx_tx_statistics {
	__le32 internal_desc_overflow;
}  __packed;

struct acx_rx_statistics {
	__le32 out_of_mem;
	__le32 hdr_overflow;
	__le32 hw_stuck;
	__le32 dropped;
	__le32 fcs_err;
	__le32 xfr_hint_trig;
	__le32 path_reset;
	__le32 reset_counter;
} __packed;

struct acx_dma_statistics {
	__le32 rx_requested;
	__le32 rx_errors;
	__le32 tx_requested;
	__le32 tx_errors;
}  __packed;

struct acx_isr_statistics {
	/* host command complete */
	__le32 cmd_cmplt;

	/* fiqisr() */
	__le32 fiqs;

	/* (INT_STS_ND & INT_TRIG_RX_HEADER) */
	__le32 rx_headers;

	/* (INT_STS_ND & INT_TRIG_RX_CMPLT) */
	__le32 rx_completes;

	/* (INT_STS_ND & INT_TRIG_NO_RX_BUF) */
	__le32 rx_mem_overflow;

	/* (INT_STS_ND & INT_TRIG_S_RX_RDY) */
	__le32 rx_rdys;

	/* irqisr() */
	__le32 irqs;

	/* (INT_STS_ND & INT_TRIG_TX_PROC) */
	__le32 tx_procs;

	/* (INT_STS_ND & INT_TRIG_DECRYPT_DONE) */
	__le32 decrypt_done;

	/* (INT_STS_ND & INT_TRIG_DMA0) */
	__le32 dma0_done;

	/* (INT_STS_ND & INT_TRIG_DMA1) */
	__le32 dma1_done;

	/* (INT_STS_ND & INT_TRIG_TX_EXC_CMPLT) */
	__le32 tx_exch_complete;

	/* (INT_STS_ND & INT_TRIG_COMMAND) */
	__le32 commands;

	/* (INT_STS_ND & INT_TRIG_RX_PROC) */
	__le32 rx_procs;

	/* (INT_STS_ND & INT_TRIG_PM_802) */
	__le32 hw_pm_mode_changes;

	/* (INT_STS_ND & INT_TRIG_ACKNOWLEDGE) */
	__le32 host_acknowledges;

	/* (INT_STS_ND & INT_TRIG_PM_PCI) */
	__le32 pci_pm;

	/* (INT_STS_ND & INT_TRIG_ACM_WAKEUP) */
	__le32 wakeups;

	/* (INT_STS_ND & INT_TRIG_LOW_RSSI) */
	__le32 low_rssi;
} __packed;

struct acx_wep_statistics {
	/* WEP address keys configured */
	__le32 addr_key_count;

	/* default keys configured */
	__le32 default_key_count;

	__le32 reserved;

	/* number of times that WEP key not found on lookup */
	__le32 key_not_found;

	/* number of times that WEP key decryption failed */
	__le32 decrypt_fail;

	/* WEP packets decrypted */
	__le32 packets;

	/* WEP decrypt interrupts */
	__le32 interrupt;
} __packed;

#define ACX_MISSED_BEACONS_SPREAD 10

struct acx_pwr_statistics {
	/* the amount of enters into power save mode (both PD & ELP) */
	__le32 ps_enter;

	/* the amount of enters into ELP mode */
	__le32 elp_enter;

	/* the amount of missing beacon interrupts to the host */
	__le32 missing_bcns;

	/* the amount of wake on host-access times */
	__le32 wake_on_host;

	/* the amount of wake on timer-expire */
	__le32 wake_on_timer_exp;

	/* the number of packets that were transmitted with PS bit set */
	__le32 tx_with_ps;

	/* the number of packets that were transmitted with PS bit clear */
	__le32 tx_without_ps;

	/* the number of received beacons */
	__le32 rcvd_beacons;

	/* the number of entering into PowerOn (power save off) */
	__le32 power_save_off;

	/* the number of entries into power save mode */
	__le16 enable_ps;

	/*
	 * the number of exits from power save, not including failed PS
	 * transitions
	 */
	__le16 disable_ps;

	/*
	 * the number of times the TSF counter was adjusted because
	 * of drift
	 */
	__le32 fix_tsf_ps;

	/* Gives statistics about the spread continuous missed beacons.
	 * The 16 LSB are dedicated for the PS mode.
	 * The 16 MSB are dedicated for the PS mode.
	 * cont_miss_bcns_spread[0] - single missed beacon.
	 * cont_miss_bcns_spread[1] - two continuous missed beacons.
	 * cont_miss_bcns_spread[2] - three continuous missed beacons.
	 * ...
	 * cont_miss_bcns_spread[9] - ten and more continuous missed beacons.
	*/
	__le32 cont_miss_bcns_spread[ACX_MISSED_BEACONS_SPREAD];

	/* the number of beacons in awake mode */
	__le32 rcvd_awake_beacons;
} __packed;

struct acx_mic_statistics {
	__le32 rx_pkts;
	__le32 calc_failure;
} __packed;

struct acx_aes_statistics {
	__le32 encrypt_fail;
	__le32 decrypt_fail;
	__le32 encrypt_packets;
	__le32 decrypt_packets;
	__le32 encrypt_interrupt;
	__le32 decrypt_interrupt;
} __packed;

struct acx_event_statistics {
	__le32 heart_beat;
	__le32 calibration;
	__le32 rx_mismatch;
	__le32 rx_mem_empty;
	__le32 rx_pool;
	__le32 oom_late;
	__le32 phy_transmit_error;
	__le32 tx_stuck;
} __packed;

struct acx_ps_statistics {
	__le32 pspoll_timeouts;
	__le32 upsd_timeouts;
	__le32 upsd_max_sptime;
	__le32 upsd_max_apturn;
	__le32 pspoll_max_apturn;
	__le32 pspoll_utilization;
	__le32 upsd_utilization;
} __packed;

struct acx_rxpipe_statistics {
	__le32 rx_prep_beacon_drop;
	__le32 descr_host_int_trig_rx_data;
	__le32 beacon_buffer_thres_host_int_trig_rx_data;
	__le32 missed_beacon_host_int_trig_rx_data;
	__le32 tx_xfr_host_int_trig_rx_data;
} __packed;

struct acx_statistics {
	struct acx_header header;

	struct acx_tx_statistics tx;
	struct acx_rx_statistics rx;
	struct acx_dma_statistics dma;
	struct acx_isr_statistics isr;
	struct acx_wep_statistics wep;
	struct acx_pwr_statistics pwr;
	struct acx_aes_statistics aes;
	struct acx_mic_statistics mic;
	struct acx_event_statistics event;
	struct acx_ps_statistics ps;
	struct acx_rxpipe_statistics rxpipe;
} __packed;

struct acx_rate_class {
	__le32 enabled_rates;
	u8 short_retry_limit;
	u8 long_retry_limit;
	u8 aflags;
	u8 reserved;
};

#define ACX_TX_BASIC_RATE      0
#define ACX_TX_AP_FULL_RATE    1
#define ACX_TX_RATE_POLICY_CNT 2
struct acx_rate_policy {
	struct acx_header header;

	__le32 rate_class_cnt;
	struct acx_rate_class rate_class[CONF_TX_MAX_RATE_CLASSES];
} __packed;

struct acx_ac_cfg {
	struct acx_header header;
	u8 ac;
	u8 cw_min;
	__le16 cw_max;
	u8 aifsn;
	u8 reserved;
	__le16 tx_op_limit;
} __packed;

struct acx_tid_config {
	struct acx_header header;
	u8 queue_id;
	u8 channel_type;
	u8 tsid;
	u8 ps_scheme;
	u8 ack_policy;
	u8 padding[3];
	__le32 apsd_conf[2];
} __packed;

struct acx_frag_threshold {
	struct acx_header header;
	__le16 frag_threshold;
	u8 padding[2];
} __packed;

struct acx_tx_config_options {
	struct acx_header header;
	__le16 tx_compl_timeout;     /* msec */
	__le16 tx_compl_threshold;   /* number of packets */
} __packed;

#define ACX_RX_MEM_BLOCKS     70
#define ACX_TX_MIN_MEM_BLOCKS 40
#define ACX_TX_DESCRIPTORS    32
#define ACX_NUM_SSID_PROFILES 1

struct wl1271_acx_config_memory {
	struct acx_header header;

	u8 rx_mem_block_num;
	u8 tx_min_mem_block_num;
	u8 num_stations;
	u8 num_ssid_profiles;
	__le32 total_tx_descriptors;
} __packed;

struct wl1271_acx_mem_map {
	struct acx_header header;

	__le32 code_start;
	__le32 code_end;

	__le32 wep_defkey_start;
	__le32 wep_defkey_end;

	__le32 sta_table_start;
	__le32 sta_table_end;

	__le32 packet_template_start;
	__le32 packet_template_end;

	/* Address of the TX result interface (control block) */
	__le32 tx_result;
	__le32 tx_result_queue_start;

	__le32 queue_memory_start;
	__le32 queue_memory_end;

	__le32 packet_memory_pool_start;
	__le32 packet_memory_pool_end;

	__le32 debug_buffer1_start;
	__le32 debug_buffer1_end;

	__le32 debug_buffer2_start;
	__le32 debug_buffer2_end;

	/* Number of blocks FW allocated for TX packets */
	__le32 num_tx_mem_blocks;

	/* Number of blocks FW allocated for RX packets */
	__le32 num_rx_mem_blocks;

	/* the following 4 fields are valid in SLAVE mode only */
	u8 *tx_cbuf;
	u8 *rx_cbuf;
	__le32 rx_ctrl;
	__le32 tx_ctrl;
} __packed;

struct wl1271_acx_rx_config_opt {
	struct acx_header header;

	__le16 mblk_threshold;
	__le16 threshold;
	__le16 timeout;
	u8 queue_type;
	u8 reserved;
} __packed;


struct wl1271_acx_bet_enable {
	struct acx_header header;

	u8 enable;
	u8 max_consecutive;
	u8 padding[2];
} __packed;

#define ACX_IPV4_VERSION 4
#define ACX_IPV6_VERSION 6
#define ACX_IPV4_ADDR_SIZE 4
struct wl1271_acx_arp_filter {
	struct acx_header header;
	u8 version;         /* ACX_IPV4_VERSION, ACX_IPV6_VERSION */
	u8 enable;          /* 1 to enable ARP filtering, 0 to disable */
	u8 padding[2];
	u8 address[16];     /* The configured device IP address - all ARP
			       requests directed to this IP address will pass
			       through. For IPv4, the first four bytes are
			       used. */
} __packed;

struct wl1271_acx_pm_config {
	struct acx_header header;

	__le32 host_clk_settling_time;
	u8 host_fast_wakeup_support;
	u8 padding[3];
} __packed;

struct wl1271_acx_keep_alive_mode {
	struct acx_header header;

	u8 enabled;
	u8 padding[3];
} __packed;

enum {
	ACX_KEEP_ALIVE_NO_TX = 0,
	ACX_KEEP_ALIVE_PERIOD_ONLY
};

enum {
	ACX_KEEP_ALIVE_TPL_INVALID = 0,
	ACX_KEEP_ALIVE_TPL_VALID
};

struct wl1271_acx_keep_alive_config {
	struct acx_header header;

	__le32 period;
	u8 index;
	u8 tpl_validation;
	u8 trigger;
	u8 padding;
} __packed;

enum {
	WL1271_ACX_TRIG_TYPE_LEVEL = 0,
	WL1271_ACX_TRIG_TYPE_EDGE,
};

enum {
	WL1271_ACX_TRIG_DIR_LOW = 0,
	WL1271_ACX_TRIG_DIR_HIGH,
	WL1271_ACX_TRIG_DIR_BIDIR,
};

enum {
	WL1271_ACX_TRIG_ENABLE = 1,
	WL1271_ACX_TRIG_DISABLE,
};

enum {
	WL1271_ACX_TRIG_METRIC_RSSI_BEACON = 0,
	WL1271_ACX_TRIG_METRIC_RSSI_DATA,
	WL1271_ACX_TRIG_METRIC_SNR_BEACON,
	WL1271_ACX_TRIG_METRIC_SNR_DATA,
};

enum {
	WL1271_ACX_TRIG_IDX_RSSI = 0,
	WL1271_ACX_TRIG_COUNT = 8,
};

struct wl1271_acx_rssi_snr_trigger {
	struct acx_header header;

	__le16 threshold;
	__le16 pacing; /* 0 - 60000 ms */
	u8 metric;
	u8 type;
	u8 dir;
	u8 hysteresis;
	u8 index;
	u8 enable;
	u8 padding[2];
};

struct wl1271_acx_rssi_snr_avg_weights {
	struct acx_header header;

	u8 rssi_beacon;
	u8 rssi_data;
	u8 snr_beacon;
	u8 snr_data;
};

struct wl1271_acx_fw_tsf_information {
	struct acx_header header;

	__le32 current_tsf_high;
	__le32 current_tsf_low;
	__le32 last_bttt_high;
	__le32 last_tbtt_low;
	u8 last_dtim_count;
	u8 padding[3];
} __packed;

enum {
	ACX_WAKE_UP_CONDITIONS      = 0x0002,
	ACX_MEM_CFG                 = 0x0003,
	ACX_SLOT                    = 0x0004,
	ACX_AC_CFG                  = 0x0007,
	ACX_MEM_MAP                 = 0x0008,
	ACX_AID                     = 0x000A,
	/* ACX_FW_REV is missing in the ref driver, but seems to work */
	ACX_FW_REV                  = 0x000D,
	ACX_MEDIUM_USAGE            = 0x000F,
	ACX_RX_CFG                  = 0x0010,
	ACX_TX_QUEUE_CFG            = 0x0011,
	ACX_STATISTICS              = 0x0013, /* Debug API */
	ACX_PWR_CONSUMPTION_STATISTICS = 0x0014,
	ACX_FEATURE_CFG             = 0x0015,
	ACX_TID_CFG                 = 0x001A,
	ACX_PS_RX_STREAMING         = 0x001B,
	ACX_BEACON_FILTER_OPT       = 0x001F,
	ACX_NOISE_HIST              = 0x0021,
	ACX_HDK_VERSION             = 0x0022, /* ??? */
	ACX_PD_THRESHOLD            = 0x0023,
	ACX_TX_CONFIG_OPT           = 0x0024,
	ACX_CCA_THRESHOLD           = 0x0025,
	ACX_EVENT_MBOX_MASK         = 0x0026,
	ACX_CONN_MONIT_PARAMS       = 0x002D,
	ACX_CONS_TX_FAILURE         = 0x002F,
	ACX_BCN_DTIM_OPTIONS        = 0x0031,
	ACX_SG_ENABLE               = 0x0032,
	ACX_SG_CFG                  = 0x0033,
	ACX_BEACON_FILTER_TABLE     = 0x0038,
	ACX_ARP_IP_FILTER           = 0x0039,
	ACX_ROAMING_STATISTICS_TBL  = 0x003B,
	ACX_RATE_POLICY             = 0x003D,
	ACX_CTS_PROTECTION          = 0x003E,
	ACX_SLEEP_AUTH              = 0x003F,
	ACX_PREAMBLE_TYPE	    = 0x0040,
	ACX_ERROR_CNT               = 0x0041,
	ACX_IBSS_FILTER		    = 0x0044,
	ACX_SERVICE_PERIOD_TIMEOUT  = 0x0045,
	ACX_TSF_INFO                = 0x0046,
	ACX_CONFIG_PS_WMM           = 0x0049,
	ACX_ENABLE_RX_DATA_FILTER   = 0x004A,
	ACX_SET_RX_DATA_FILTER      = 0x004B,
	ACX_GET_DATA_FILTER_STATISTICS = 0x004C,
	ACX_RX_CONFIG_OPT           = 0x004E,
	ACX_FRAG_CFG                = 0x004F,
	ACX_BET_ENABLE              = 0x0050,
	ACX_RSSI_SNR_TRIGGER        = 0x0051,
	ACX_RSSI_SNR_WEIGHTS        = 0x0052,
	ACX_KEEP_ALIVE_MODE         = 0x0053,
	ACX_SET_KEEP_ALIVE_CONFIG   = 0x0054,
	ACX_BA_SESSION_RESPONDER_POLICY = 0x0055,
	ACX_BA_SESSION_INITIATOR_POLICY = 0x0056,
	ACX_PEER_HT_CAP             = 0x0057,
	ACX_HT_BSS_OPERATION        = 0x0058,
	ACX_COEX_ACTIVITY           = 0x0059,
	ACX_SET_SMART_REFLEX_DEBUG  = 0x005A,
	ACX_SET_DCO_ITRIM_PARAMS    = 0x0061,
	DOT11_RX_MSDU_LIFE_TIME     = 0x1004,
	DOT11_CUR_TX_PWR            = 0x100D,
	DOT11_RX_DOT11_MODE         = 0x1012,
	DOT11_RTS_THRESHOLD         = 0x1013,
	DOT11_GROUP_ADDRESS_TBL     = 0x1014,
	ACX_PM_CONFIG               = 0x1016,

	MAX_DOT11_IE = DOT11_GROUP_ADDRESS_TBL,

	MAX_IE = 0xFFFF
};


int wl1271_acx_wake_up_conditions(struct wl1271 *wl);
int wl1271_acx_sleep_auth(struct wl1271 *wl, u8 sleep_auth);
int wl1271_acx_fw_version(struct wl1271 *wl, char *buf, size_t len);
int wl1271_acx_tx_power(struct wl1271 *wl, int power);
int wl1271_acx_feature_cfg(struct wl1271 *wl);
int wl1271_acx_mem_map(struct wl1271 *wl,
		       struct acx_header *mem_map, size_t len);
int wl1271_acx_rx_msdu_life_time(struct wl1271 *wl);
int wl1271_acx_rx_config(struct wl1271 *wl, u32 config, u32 filter);
int wl1271_acx_pd_threshold(struct wl1271 *wl);
int wl1271_acx_slot(struct wl1271 *wl, enum acx_slot_type slot_time);
int wl1271_acx_group_address_tbl(struct wl1271 *wl, bool enable,
				 void *mc_list, u32 mc_list_len);
int wl1271_acx_service_period_timeout(struct wl1271 *wl);
int wl1271_acx_rts_threshold(struct wl1271 *wl, u16 rts_threshold);
int wl1271_acx_dco_itrim_params(struct wl1271 *wl);
int wl1271_acx_beacon_filter_opt(struct wl1271 *wl, bool enable_filter);
int wl1271_acx_beacon_filter_table(struct wl1271 *wl);
int wl1271_acx_conn_monit_params(struct wl1271 *wl, bool enable);
int wl1271_acx_sg_enable(struct wl1271 *wl, bool enable);
int wl1271_acx_sg_cfg(struct wl1271 *wl);
int wl1271_acx_cca_threshold(struct wl1271 *wl);
int wl1271_acx_bcn_dtim_options(struct wl1271 *wl);
int wl1271_acx_aid(struct wl1271 *wl, u16 aid);
int wl1271_acx_event_mbox_mask(struct wl1271 *wl, u32 event_mask);
int wl1271_acx_set_preamble(struct wl1271 *wl, enum acx_preamble_type preamble);
int wl1271_acx_cts_protect(struct wl1271 *wl,
			   enum acx_ctsprotect_type ctsprotect);
int wl1271_acx_statistics(struct wl1271 *wl, struct acx_statistics *stats);
int wl1271_acx_rate_policies(struct wl1271 *wl);
int wl1271_acx_ac_cfg(struct wl1271 *wl, u8 ac, u8 cw_min, u16 cw_max,
		      u8 aifsn, u16 txop);
int wl1271_acx_tid_cfg(struct wl1271 *wl, u8 queue_id, u8 channel_type,
		       u8 tsid, u8 ps_scheme, u8 ack_policy,
		       u32 apsd_conf0, u32 apsd_conf1);
int wl1271_acx_frag_threshold(struct wl1271 *wl);
int wl1271_acx_tx_config_options(struct wl1271 *wl);
int wl1271_acx_mem_cfg(struct wl1271 *wl);
int wl1271_acx_init_mem_config(struct wl1271 *wl);
int wl1271_acx_init_rx_interrupt(struct wl1271 *wl);
int wl1271_acx_smart_reflex(struct wl1271 *wl);
int wl1271_acx_bet_enable(struct wl1271 *wl, bool enable);
int wl1271_acx_arp_ip_filter(struct wl1271 *wl, bool enable, __be32 address);
int wl1271_acx_pm_config(struct wl1271 *wl);
int wl1271_acx_keep_alive_mode(struct wl1271 *wl, bool enable);
int wl1271_acx_keep_alive_config(struct wl1271 *wl, u8 index, u8 tpl_valid);
int wl1271_acx_rssi_snr_trigger(struct wl1271 *wl, bool enable,
				s16 thold, u8 hyst);
int wl1271_acx_rssi_snr_avg_weights(struct wl1271 *wl);
int wl1271_acx_tsf_info(struct wl1271 *wl, u64 *mactime);

#endif /* __WL1271_ACX_H__ */
