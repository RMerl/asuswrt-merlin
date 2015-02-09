/*
 * Copyright 2002-2005, Instant802 Networks, Inc.
 * Copyright 2005, Devicescape Software, Inc.
 * Copyright 2006-2007	Jiri Benc <jbenc@suse.cz>
 * Copyright 2007-2010	Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IEEE80211_I_H
#define IEEE80211_I_H

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/if_ether.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/etherdevice.h>
#include <net/ieee80211_radiotap.h>
#include <net/cfg80211.h>
#include <net/mac80211.h>
#include "key.h"
#include "sta_info.h"

struct ieee80211_local;

/* Maximum number of broadcast/multicast frames to buffer when some of the
 * associated stations are using power saving. */
#define AP_MAX_BC_BUFFER 128

/* Maximum number of frames buffered to all STAs, including multicast frames.
 * Note: increasing this limit increases the potential memory requirement. Each
 * frame can be up to about 2 kB long. */
#define TOTAL_MAX_TX_BUFFER 512

/* Required encryption head and tailroom */
#define IEEE80211_ENCRYPT_HEADROOM 8
#define IEEE80211_ENCRYPT_TAILROOM 18

/* IEEE 802.11 (Ch. 9.5 Defragmentation) requires support for concurrent
 * reception of at least three fragmented frames. This limit can be increased
 * by changing this define, at the cost of slower frame reassembly and
 * increased memory use (about 2 kB of RAM per entry). */
#define IEEE80211_FRAGMENT_MAX 4

/*
 * Time after which we ignore scan results and no longer report/use
 * them in any way.
 */
#define IEEE80211_SCAN_RESULT_EXPIRE (10 * HZ)

#define TU_TO_EXP_TIME(x)	(jiffies + usecs_to_jiffies((x) * 1024))

#define IEEE80211_DEFAULT_UAPSD_QUEUES \
	(IEEE80211_WMM_IE_STA_QOSINFO_AC_BK |	\
	 IEEE80211_WMM_IE_STA_QOSINFO_AC_BE |	\
	 IEEE80211_WMM_IE_STA_QOSINFO_AC_VI |	\
	 IEEE80211_WMM_IE_STA_QOSINFO_AC_VO)

#define IEEE80211_DEFAULT_MAX_SP_LEN		\
	IEEE80211_WMM_IE_STA_QOSINFO_SP_ALL

struct ieee80211_fragment_entry {
	unsigned long first_frag_time;
	unsigned int seq;
	unsigned int rx_queue;
	unsigned int last_frag;
	unsigned int extra_len;
	struct sk_buff_head skb_list;
	int ccmp; /* Whether fragments were encrypted with CCMP */
	u8 last_pn[6]; /* PN of the last fragment if CCMP was used */
};


struct ieee80211_bss {
	/* don't want to look up all the time */
	size_t ssid_len;
	u8 ssid[IEEE80211_MAX_SSID_LEN];

	u8 dtim_period;

	bool wmm_used;
	bool uapsd_supported;

	unsigned long last_probe_resp;

#ifdef CONFIG_MAC80211_MESH
	u8 *mesh_id;
	size_t mesh_id_len;
	u8 *mesh_cfg;
#endif

#define IEEE80211_MAX_SUPP_RATES 32
	u8 supp_rates[IEEE80211_MAX_SUPP_RATES];
	size_t supp_rates_len;

	/*
	 * During assocation, we save an ERP value from a probe response so
	 * that we can feed ERP info to the driver when handling the
	 * association completes. these fields probably won't be up-to-date
	 * otherwise, you probably don't want to use them.
	 */
	bool has_erp_value;
	u8 erp_value;
};

static inline u8 *bss_mesh_cfg(struct ieee80211_bss *bss)
{
#ifdef CONFIG_MAC80211_MESH
	return bss->mesh_cfg;
#endif
	return NULL;
}

static inline u8 *bss_mesh_id(struct ieee80211_bss *bss)
{
#ifdef CONFIG_MAC80211_MESH
	return bss->mesh_id;
#endif
	return NULL;
}

static inline u8 bss_mesh_id_len(struct ieee80211_bss *bss)
{
#ifdef CONFIG_MAC80211_MESH
	return bss->mesh_id_len;
#endif
	return 0;
}


typedef unsigned __bitwise__ ieee80211_tx_result;
#define TX_CONTINUE	((__force ieee80211_tx_result) 0u)
#define TX_DROP		((__force ieee80211_tx_result) 1u)
#define TX_QUEUED	((__force ieee80211_tx_result) 2u)

#define IEEE80211_TX_FRAGMENTED		BIT(0)
#define IEEE80211_TX_UNICAST		BIT(1)
#define IEEE80211_TX_PS_BUFFERED	BIT(2)

struct ieee80211_tx_data {
	struct sk_buff *skb;
	struct ieee80211_local *local;
	struct ieee80211_sub_if_data *sdata;
	struct sta_info *sta;
	struct ieee80211_key *key;

	struct ieee80211_channel *channel;

	u16 ethertype;
	unsigned int flags;
};


typedef unsigned __bitwise__ ieee80211_rx_result;
#define RX_CONTINUE		((__force ieee80211_rx_result) 0u)
#define RX_DROP_UNUSABLE	((__force ieee80211_rx_result) 1u)
#define RX_DROP_MONITOR		((__force ieee80211_rx_result) 2u)
#define RX_QUEUED		((__force ieee80211_rx_result) 3u)

#define IEEE80211_RX_IN_SCAN		BIT(0)
/* frame is destined to interface currently processed (incl. multicast frames) */
#define IEEE80211_RX_RA_MATCH		BIT(1)
#define IEEE80211_RX_AMSDU		BIT(2)
#define IEEE80211_RX_FRAGMENTED		BIT(3)
/* only add flags here that do not change with subframes of an aMPDU */

struct ieee80211_rx_data {
	struct sk_buff *skb;
	struct ieee80211_local *local;
	struct ieee80211_sub_if_data *sdata;
	struct sta_info *sta;
	struct ieee80211_key *key;

	unsigned int flags;
	int queue;
	u32 tkip_iv32;
	u16 tkip_iv16;
};

struct beacon_data {
	u8 *head, *tail;
	int head_len, tail_len;
	int dtim_period;
};

struct ieee80211_if_ap {
	struct beacon_data *beacon;

	struct list_head vlans;

	/* yes, this looks ugly, but guarantees that we can later use
	 * bitmap_empty :)
	 * NB: don't touch this bitmap, use sta_info_{set,clear}_tim_bit */
	u8 tim[sizeof(unsigned long) * BITS_TO_LONGS(IEEE80211_MAX_AID + 1)];
	struct sk_buff_head ps_bc_buf;
	atomic_t num_sta_ps; /* number of stations in PS mode */
	int dtim_count;
};

struct ieee80211_if_wds {
	struct sta_info *sta;
	u8 remote_addr[ETH_ALEN];
};

struct ieee80211_if_vlan {
	struct list_head list;

	/* used for all tx if the VLAN is configured to 4-addr mode */
	struct sta_info *sta;
};

struct mesh_stats {
	__u32 fwded_mcast;		/* Mesh forwarded multicast frames */
	__u32 fwded_unicast;		/* Mesh forwarded unicast frames */
	__u32 fwded_frames;		/* Mesh total forwarded frames */
	__u32 dropped_frames_ttl;	/* Not transmitted since mesh_ttl == 0*/
	__u32 dropped_frames_no_route;	/* Not transmitted, no route found */
	atomic_t estab_plinks;
};

#define PREQ_Q_F_START		0x1
#define PREQ_Q_F_REFRESH	0x2
struct mesh_preq_queue {
	struct list_head list;
	u8 dst[ETH_ALEN];
	u8 flags;
};

enum ieee80211_work_type {
	IEEE80211_WORK_ABORT,
	IEEE80211_WORK_DIRECT_PROBE,
	IEEE80211_WORK_AUTH,
	IEEE80211_WORK_ASSOC_BEACON_WAIT,
	IEEE80211_WORK_ASSOC,
	IEEE80211_WORK_REMAIN_ON_CHANNEL,
};

/**
 * enum work_done_result - indicates what to do after work was done
 *
 * @WORK_DONE_DESTROY: This work item is no longer needed, destroy.
 * @WORK_DONE_REQUEUE: This work item was reset to be reused, and
 *	should be requeued.
 */
enum work_done_result {
	WORK_DONE_DESTROY,
	WORK_DONE_REQUEUE,
};

struct ieee80211_work {
	struct list_head list;

	struct rcu_head rcu_head;

	struct ieee80211_sub_if_data *sdata;

	enum work_done_result (*done)(struct ieee80211_work *wk,
				      struct sk_buff *skb);

	struct ieee80211_channel *chan;
	enum nl80211_channel_type chan_type;

	unsigned long timeout;
	enum ieee80211_work_type type;

	u8 filter_ta[ETH_ALEN];

	bool started;

	union {
		struct {
			int tries;
			u16 algorithm, transaction;
			u8 ssid[IEEE80211_MAX_SSID_LEN];
			u8 ssid_len;
			u8 key[WLAN_KEY_LEN_WEP104];
			u8 key_len, key_idx;
			bool privacy;
		} probe_auth;
		struct {
			struct cfg80211_bss *bss;
			const u8 *supp_rates;
			const u8 *ht_information_ie;
			enum ieee80211_smps_mode smps;
			int tries;
			u16 capability;
			u8 prev_bssid[ETH_ALEN];
			u8 ssid[IEEE80211_MAX_SSID_LEN];
			u8 ssid_len;
			u8 supp_rates_len;
			bool wmm_used, use_11n, uapsd_used;
		} assoc;
		struct {
			u32 duration;
		} remain;
	};

	int ie_len;
	/* must be last */
	u8 ie[0];
};

/* flags used in struct ieee80211_if_managed.flags */
enum ieee80211_sta_flags {
	IEEE80211_STA_BEACON_POLL	= BIT(0),
	IEEE80211_STA_CONNECTION_POLL	= BIT(1),
	IEEE80211_STA_CONTROL_PORT	= BIT(2),
	IEEE80211_STA_DISABLE_11N	= BIT(4),
	IEEE80211_STA_CSA_RECEIVED	= BIT(5),
	IEEE80211_STA_MFP_ENABLED	= BIT(6),
	IEEE80211_STA_UAPSD_ENABLED	= BIT(7),
	IEEE80211_STA_NULLFUNC_ACKED	= BIT(8),
	IEEE80211_STA_RESET_SIGNAL_AVE	= BIT(9),
};

struct ieee80211_if_managed {
	struct timer_list timer;
	struct timer_list conn_mon_timer;
	struct timer_list bcn_mon_timer;
	struct timer_list chswitch_timer;
	struct work_struct monitor_work;
	struct work_struct chswitch_work;
	struct work_struct beacon_connection_loss_work;

	unsigned long probe_timeout;
	int probe_send_count;

	struct mutex mtx;
	struct cfg80211_bss *associated;

	u8 bssid[ETH_ALEN];

	u16 aid;

	unsigned long timers_running; /* used for quiesce/restart */
	bool powersave; /* powersave requested for this iface */
	enum ieee80211_smps_mode req_smps, /* requested smps mode */
				 ap_smps; /* smps mode AP thinks we're in */

	unsigned int flags;

	u32 beacon_crc;

	enum {
		IEEE80211_MFP_DISABLED,
		IEEE80211_MFP_OPTIONAL,
		IEEE80211_MFP_REQUIRED
	} mfp; /* management frame protection */

	int wmm_last_param_set;

	u8 use_4addr;

	/* Signal strength from the last Beacon frame in the current BSS. */
	int last_beacon_signal;

	/*
	 * Weighted average of the signal strength from Beacon frames in the
	 * current BSS. This is in units of 1/16 of the signal unit to maintain
	 * accuracy and to speed up calculations, i.e., the value need to be
	 * divided by 16 to get the actual value.
	 */
	int ave_beacon_signal;

	/*
	 * Last Beacon frame signal strength average (ave_beacon_signal / 16)
	 * that triggered a cqm event. 0 indicates that no event has been
	 * generated for the current association.
	 */
	int last_cqm_event_signal;
};

struct ieee80211_if_ibss {
	struct timer_list timer;

	struct mutex mtx;

	unsigned long last_scan_completed;

	u32 basic_rates;

	bool timer_running;

	bool fixed_bssid;
	bool fixed_channel;
	bool privacy;

	u8 bssid[ETH_ALEN];
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 ssid_len, ie_len;
	u8 *ie;
	struct ieee80211_channel *channel;

	unsigned long ibss_join_req;
	/* probe response/beacon for IBSS */
	struct sk_buff *presp, *skb;

	enum {
		IEEE80211_IBSS_MLME_SEARCH,
		IEEE80211_IBSS_MLME_JOINED,
	} state;
};

struct ieee80211_if_mesh {
	struct timer_list housekeeping_timer;
	struct timer_list mesh_path_timer;
	struct timer_list mesh_path_root_timer;

	unsigned long timers_running;

	unsigned long wrkq_flags;

	u8 mesh_id[IEEE80211_MAX_MESH_ID_LEN];
	size_t mesh_id_len;
	/* Active Path Selection Protocol Identifier */
	u8 mesh_pp_id;
	/* Active Path Selection Metric Identifier */
	u8 mesh_pm_id;
	/* Congestion Control Mode Identifier */
	u8 mesh_cc_id;
	/* Synchronization Protocol Identifier */
	u8 mesh_sp_id;
	/* Authentication Protocol Identifier */
	u8 mesh_auth_id;
	/* Local mesh Sequence Number */
	u32 sn;
	/* Last used PREQ ID */
	u32 preq_id;
	atomic_t mpaths;
	/* Timestamp of last SN update */
	unsigned long last_sn_update;
	/* Timestamp of last SN sent */
	unsigned long last_preq;
	struct mesh_rmc *rmc;
	spinlock_t mesh_preq_queue_lock;
	struct mesh_preq_queue preq_queue;
	int preq_queue_len;
	struct mesh_stats mshstats;
	struct mesh_config mshcfg;
	u32 mesh_seqnum;
	bool accepting_plinks;
};

#ifdef CONFIG_MAC80211_MESH
#define IEEE80211_IFSTA_MESH_CTR_INC(msh, name)	\
	do { (msh)->mshstats.name++; } while (0)
#else
#define IEEE80211_IFSTA_MESH_CTR_INC(msh, name) \
	do { } while (0)
#endif

/**
 * enum ieee80211_sub_if_data_flags - virtual interface flags
 *
 * @IEEE80211_SDATA_ALLMULTI: interface wants all multicast packets
 * @IEEE80211_SDATA_PROMISC: interface is promisc
 * @IEEE80211_SDATA_OPERATING_GMODE: operating in G-only mode
 * @IEEE80211_SDATA_DONT_BRIDGE_PACKETS: bridge packets between
 *	associated stations and deliver multicast frames both
 *	back to wireless media and to the local net stack.
 */
enum ieee80211_sub_if_data_flags {
	IEEE80211_SDATA_ALLMULTI		= BIT(0),
	IEEE80211_SDATA_PROMISC			= BIT(1),
	IEEE80211_SDATA_OPERATING_GMODE		= BIT(2),
	IEEE80211_SDATA_DONT_BRIDGE_PACKETS	= BIT(3),
};

struct ieee80211_sub_if_data {
	struct list_head list;

	struct wireless_dev wdev;

	/* keys */
	struct list_head key_list;

	struct net_device *dev;
	struct ieee80211_local *local;

	unsigned int flags;

	int drop_unencrypted;

	char name[IFNAMSIZ];

	/*
	 * keep track of whether the HT opmode (stored in
	 * vif.bss_info.ht_operation_mode) is valid.
	 */
	bool ht_opmode_valid;

	/* Fragment table for host-based reassembly */
	struct ieee80211_fragment_entry	fragments[IEEE80211_FRAGMENT_MAX];
	unsigned int fragment_next;

#define NUM_DEFAULT_KEYS 4
#define NUM_DEFAULT_MGMT_KEYS 2
	struct ieee80211_key *keys[NUM_DEFAULT_KEYS + NUM_DEFAULT_MGMT_KEYS];
	struct ieee80211_key *default_key;
	struct ieee80211_key *default_mgmt_key;

	u16 sequence_number;

	struct work_struct work;
	struct sk_buff_head skb_queue;

	bool arp_filter_state;

	/*
	 * AP this belongs to: self in AP mode and
	 * corresponding AP in VLAN mode, NULL for
	 * all others (might be needed later in IBSS)
	 */
	struct ieee80211_if_ap *bss;

	/* bitmap of allowed (non-MCS) rate indexes for rate control */
	u32 rc_rateidx_mask[IEEE80211_NUM_BANDS];

	union {
		struct ieee80211_if_ap ap;
		struct ieee80211_if_wds wds;
		struct ieee80211_if_vlan vlan;
		struct ieee80211_if_managed mgd;
		struct ieee80211_if_ibss ibss;
#ifdef CONFIG_MAC80211_MESH
		struct ieee80211_if_mesh mesh;
#endif
		u32 mntr_flags;
	} u;

#ifdef CONFIG_MAC80211_DEBUGFS
	struct {
		struct dentry *dir;
		struct dentry *default_key;
		struct dentry *default_mgmt_key;
	} debugfs;
#endif
	/* must be last, dynamically sized area in this! */
	struct ieee80211_vif vif;
};

static inline
struct ieee80211_sub_if_data *vif_to_sdata(struct ieee80211_vif *p)
{
	return container_of(p, struct ieee80211_sub_if_data, vif);
}

static inline void
ieee80211_sdata_set_mesh_id(struct ieee80211_sub_if_data *sdata,
			    u8 mesh_id_len, u8 *mesh_id)
{
#ifdef CONFIG_MAC80211_MESH
	struct ieee80211_if_mesh *ifmsh = &sdata->u.mesh;
	ifmsh->mesh_id_len = mesh_id_len;
	memcpy(ifmsh->mesh_id, mesh_id, mesh_id_len);
#else
	WARN_ON(1);
#endif
}

enum sdata_queue_type {
	IEEE80211_SDATA_QUEUE_TYPE_FRAME	= 0,
	IEEE80211_SDATA_QUEUE_AGG_START		= 1,
	IEEE80211_SDATA_QUEUE_AGG_STOP		= 2,
};

enum {
	IEEE80211_RX_MSG	= 1,
	IEEE80211_TX_STATUS_MSG	= 2,
};

enum queue_stop_reason {
	IEEE80211_QUEUE_STOP_REASON_DRIVER,
	IEEE80211_QUEUE_STOP_REASON_PS,
	IEEE80211_QUEUE_STOP_REASON_CSA,
	IEEE80211_QUEUE_STOP_REASON_AGGREGATION,
	IEEE80211_QUEUE_STOP_REASON_SUSPEND,
	IEEE80211_QUEUE_STOP_REASON_SKB_ADD,
};

/**
 * mac80211 scan flags - currently active scan mode
 *
 * @SCAN_SW_SCANNING: We're currently in the process of scanning but may as
 *	well be on the operating channel
 * @SCAN_HW_SCANNING: The hardware is scanning for us, we have no way to
 *	determine if we are on the operating channel or not
 * @SCAN_OFF_CHANNEL: We're off our operating channel for scanning,
 *	gets only set in conjunction with SCAN_SW_SCANNING
 */
enum {
	SCAN_SW_SCANNING,
	SCAN_HW_SCANNING,
	SCAN_OFF_CHANNEL,
};

/**
 * enum mac80211_scan_state - scan state machine states
 *
 * @SCAN_DECISION: Main entry point to the scan state machine, this state
 *	determines if we should keep on scanning or switch back to the
 *	operating channel
 * @SCAN_SET_CHANNEL: Set the next channel to be scanned
 * @SCAN_SEND_PROBE: Send probe requests and wait for probe responses
 * @SCAN_LEAVE_OPER_CHANNEL: Leave the operating channel, notify the AP
 *	about us leaving the channel and stop all associated STA interfaces
 * @SCAN_ENTER_OPER_CHANNEL: Enter the operating channel again, notify the
 *	AP about us being back and restart all associated STA interfaces
 */
enum mac80211_scan_state {
	SCAN_DECISION,
	SCAN_SET_CHANNEL,
	SCAN_SEND_PROBE,
	SCAN_LEAVE_OPER_CHANNEL,
	SCAN_ENTER_OPER_CHANNEL,
};

struct ieee80211_local {
	/* embed the driver visible part.
	 * don't cast (use the static inlines below), but we keep
	 * it first anyway so they become a no-op */
	struct ieee80211_hw hw;

	const struct ieee80211_ops *ops;

	/*
	 * work stuff, potentially off-channel (in the future)
	 */
	struct mutex work_mtx;
	struct list_head work_list;
	struct timer_list work_timer;
	struct work_struct work_work;
	struct sk_buff_head work_skb_queue;

	/*
	 * private workqueue to mac80211. mac80211 makes this accessible
	 * via ieee80211_queue_work()
	 */
	struct workqueue_struct *workqueue;

	unsigned long queue_stop_reasons[IEEE80211_MAX_QUEUES];
	/* also used to protect ampdu_ac_queue and amdpu_ac_stop_refcnt */
	spinlock_t queue_stop_reason_lock;

	int open_count;
	int monitors, cooked_mntrs;
	/* number of interfaces with corresponding FIF_ flags */
	int fif_fcsfail, fif_plcpfail, fif_control, fif_other_bss, fif_pspoll;
	unsigned int filter_flags; /* FIF_* */

	/* protects the aggregated multicast list and filter calls */
	spinlock_t filter_lock;

	/* used for uploading changed mc list */
	struct work_struct reconfig_filter;

	/* used to reconfigure hardware SM PS */
	struct work_struct recalc_smps;

	/* aggregated multicast list */
	struct netdev_hw_addr_list mc_list;

	bool tim_in_locked_section; /* see ieee80211_beacon_get() */

	/*
	 * suspended is true if we finished all the suspend _and_ we have
	 * not yet come up from resume. This is to be used by mac80211
	 * to ensure driver sanity during suspend and mac80211's own
	 * sanity. It can eventually be used for WoW as well.
	 */
	bool suspended;

	/*
	 * Resuming is true while suspended, but when we're reprogramming the
	 * hardware -- at that time it's allowed to use ieee80211_queue_work()
	 * again even though some other parts of the stack are still suspended
	 * and we still drop received frames to avoid waking the stack.
	 */
	bool resuming;

	/*
	 * quiescing is true during the suspend process _only_ to
	 * ease timer cancelling etc.
	 */
	bool quiescing;

	/* device is started */
	bool started;

	int tx_headroom; /* required headroom for hardware/radiotap */

	/* Tasklet and skb queue to process calls from IRQ mode. All frames
	 * added to skb_queue will be processed, but frames in
	 * skb_queue_unreliable may be dropped if the total length of these
	 * queues increases over the limit. */
#define IEEE80211_IRQSAFE_QUEUE_LIMIT 128
	struct tasklet_struct tasklet;
	struct sk_buff_head skb_queue;
	struct sk_buff_head skb_queue_unreliable;

	/* Station data */
	/*
	 * The mutex only protects the list and counter,
	 * reads are done in RCU.
	 * Additionally, the lock protects the hash table,
	 * the pending list and each BSS's TIM bitmap.
	 */
	struct mutex sta_mtx;
	spinlock_t sta_lock;
	unsigned long num_sta;
	struct list_head sta_list, sta_pending_list;
	struct sta_info *sta_hash[STA_HASH_SIZE];
	struct timer_list sta_cleanup;
	struct work_struct sta_finish_work;
	int sta_generation;

	struct sk_buff_head pending[IEEE80211_MAX_QUEUES];
	struct tasklet_struct tx_pending_tasklet;

	atomic_t agg_queue_stop[IEEE80211_MAX_QUEUES];

	/* number of interfaces with corresponding IFF_ flags */
	atomic_t iff_allmultis, iff_promiscs;

	struct rate_control_ref *rate_ctrl;

	struct crypto_blkcipher *wep_tx_tfm;
	struct crypto_blkcipher *wep_rx_tfm;
	u32 wep_iv;

	/* see iface.c */
	struct list_head interfaces;
	struct mutex iflist_mtx;

	/*
	 * Key mutex, protects sdata's key_list and sta_info's
	 * key pointers (write access, they're RCU.)
	 */
	struct mutex key_mtx;


	/* Scanning and BSS list */
	struct mutex scan_mtx;
	unsigned long scanning;
	struct cfg80211_ssid scan_ssid;
	struct cfg80211_scan_request *int_scan_req;
	struct cfg80211_scan_request *scan_req, *hw_scan_req;
	struct ieee80211_channel *scan_channel;
	enum ieee80211_band hw_scan_band;
	int scan_channel_idx;
	int scan_ies_len;

	unsigned long leave_oper_channel_time;
	enum mac80211_scan_state next_scan_state;
	struct delayed_work scan_work;
	struct ieee80211_sub_if_data *scan_sdata;
	enum nl80211_channel_type _oper_channel_type;
	struct ieee80211_channel *oper_channel, *csa_channel;

	/* Temporary remain-on-channel for off-channel operations */
	struct ieee80211_channel *tmp_channel;
	enum nl80211_channel_type tmp_channel_type;

	/* SNMP counters */
	/* dot11CountersTable */
	u32 dot11TransmittedFragmentCount;
	u32 dot11MulticastTransmittedFrameCount;
	u32 dot11FailedCount;
	u32 dot11RetryCount;
	u32 dot11MultipleRetryCount;
	u32 dot11FrameDuplicateCount;
	u32 dot11ReceivedFragmentCount;
	u32 dot11MulticastReceivedFrameCount;
	u32 dot11TransmittedFrameCount;

#ifdef CONFIG_MAC80211_LEDS
	int tx_led_counter, rx_led_counter;
	struct led_trigger *tx_led, *rx_led, *assoc_led, *radio_led;
	char tx_led_name[32], rx_led_name[32],
	     assoc_led_name[32], radio_led_name[32];
#endif

#ifdef CONFIG_MAC80211_DEBUG_COUNTERS
	/* TX/RX handler statistics */
	unsigned int tx_handlers_drop;
	unsigned int tx_handlers_queued;
	unsigned int tx_handlers_drop_unencrypted;
	unsigned int tx_handlers_drop_fragment;
	unsigned int tx_handlers_drop_wep;
	unsigned int tx_handlers_drop_not_assoc;
	unsigned int tx_handlers_drop_unauth_port;
	unsigned int rx_handlers_drop;
	unsigned int rx_handlers_queued;
	unsigned int rx_handlers_drop_nullfunc;
	unsigned int rx_handlers_drop_defrag;
	unsigned int rx_handlers_drop_short;
	unsigned int rx_handlers_drop_passive_scan;
	unsigned int tx_expand_skb_head;
	unsigned int tx_expand_skb_head_cloned;
	unsigned int rx_expand_skb_head;
	unsigned int rx_expand_skb_head2;
	unsigned int rx_handlers_fragments;
	unsigned int tx_status_drop;
#define I802_DEBUG_INC(c) (c)++
#else /* CONFIG_MAC80211_DEBUG_COUNTERS */
#define I802_DEBUG_INC(c) do { } while (0)
#endif /* CONFIG_MAC80211_DEBUG_COUNTERS */


	int total_ps_buffered; /* total number of all buffered unicast and
				* multicast packets for power saving stations
				*/
	int wifi_wme_noack_test;
	unsigned int wmm_acm; /* bit field of ACM bits (BIT(802.1D tag)) */

	/*
	 * Bitmask of enabled u-apsd queues,
	 * IEEE80211_WMM_IE_STA_QOSINFO_AC_BE & co. Needs a new association
	 * to take effect.
	 */
	unsigned int uapsd_queues;

	/*
	 * Maximum number of buffered frames AP can deliver during a
	 * service period, IEEE80211_WMM_IE_STA_QOSINFO_SP_ALL or similar.
	 * Needs a new association to take effect.
	 */
	unsigned int uapsd_max_sp_len;

	bool pspolling;
	bool offchannel_ps_enabled;
	/*
	 * PS can only be enabled when we have exactly one managed
	 * interface (and monitors) in PS, this then points there.
	 */
	struct ieee80211_sub_if_data *ps_sdata;
	struct work_struct dynamic_ps_enable_work;
	struct work_struct dynamic_ps_disable_work;
	struct timer_list dynamic_ps_timer;
	struct notifier_block network_latency_notifier;
	struct notifier_block ifa_notifier;

	/*
	 * The dynamic ps timeout configured from user space via WEXT -
	 * this will override whatever chosen by mac80211 internally.
	 */
	int dynamic_ps_forced_timeout;
	int dynamic_ps_user_timeout;
	bool disable_dynamic_ps;

	int user_power_level; /* in dBm */
	int power_constr_level; /* in dBm */

	enum ieee80211_smps_mode smps_mode;

	struct work_struct restart_work;

#ifdef CONFIG_MAC80211_DEBUGFS
	struct local_debugfsdentries {
		struct dentry *rcdir;
		struct dentry *stations;
		struct dentry *keys;
	} debugfs;
#endif
};

static inline struct ieee80211_sub_if_data *
IEEE80211_DEV_TO_SUB_IF(struct net_device *dev)
{
	return netdev_priv(dev);
}

/* this struct represents 802.11n's RA/TID combination */
struct ieee80211_ra_tid {
	u8 ra[ETH_ALEN];
	u16 tid;
};

/* Parsed Information Elements */
struct ieee802_11_elems {
	u8 *ie_start;
	size_t total_len;

	/* pointers to IEs */
	u8 *ssid;
	u8 *supp_rates;
	u8 *fh_params;
	u8 *ds_params;
	u8 *cf_params;
	struct ieee80211_tim_ie *tim;
	u8 *ibss_params;
	u8 *challenge;
	u8 *wpa;
	u8 *rsn;
	u8 *erp_info;
	u8 *ext_supp_rates;
	u8 *wmm_info;
	u8 *wmm_param;
	struct ieee80211_ht_cap *ht_cap_elem;
	struct ieee80211_ht_info *ht_info_elem;
	struct ieee80211_meshconf_ie *mesh_config;
	u8 *mesh_id;
	u8 *peer_link;
	u8 *preq;
	u8 *prep;
	u8 *perr;
	struct ieee80211_rann_ie *rann;
	u8 *ch_switch_elem;
	u8 *country_elem;
	u8 *pwr_constr_elem;
	u8 *quiet_elem; 	/* first quite element */
	u8 *timeout_int;

	/* length of them, respectively */
	u8 ssid_len;
	u8 supp_rates_len;
	u8 fh_params_len;
	u8 ds_params_len;
	u8 cf_params_len;
	u8 tim_len;
	u8 ibss_params_len;
	u8 challenge_len;
	u8 wpa_len;
	u8 rsn_len;
	u8 erp_info_len;
	u8 ext_supp_rates_len;
	u8 wmm_info_len;
	u8 wmm_param_len;
	u8 mesh_id_len;
	u8 peer_link_len;
	u8 preq_len;
	u8 prep_len;
	u8 perr_len;
	u8 ch_switch_elem_len;
	u8 country_elem_len;
	u8 pwr_constr_elem_len;
	u8 quiet_elem_len;
	u8 num_of_quiet_elem;	/* can be more the one */
	u8 timeout_int_len;
};

static inline struct ieee80211_local *hw_to_local(
	struct ieee80211_hw *hw)
{
	return container_of(hw, struct ieee80211_local, hw);
}

static inline struct ieee80211_hw *local_to_hw(
	struct ieee80211_local *local)
{
	return &local->hw;
}


static inline int ieee80211_bssid_match(const u8 *raddr, const u8 *addr)
{
	return compare_ether_addr(raddr, addr) == 0 ||
	       is_broadcast_ether_addr(raddr);
}


int ieee80211_hw_config(struct ieee80211_local *local, u32 changed);
void ieee80211_tx_set_protected(struct ieee80211_tx_data *tx);
void ieee80211_bss_info_change_notify(struct ieee80211_sub_if_data *sdata,
				      u32 changed);
void ieee80211_configure_filter(struct ieee80211_local *local);
u32 ieee80211_reset_erp_info(struct ieee80211_sub_if_data *sdata);

extern bool ieee80211_disable_40mhz_24ghz;

/* STA code */
void ieee80211_sta_setup_sdata(struct ieee80211_sub_if_data *sdata);
int ieee80211_mgd_auth(struct ieee80211_sub_if_data *sdata,
		       struct cfg80211_auth_request *req);
int ieee80211_mgd_assoc(struct ieee80211_sub_if_data *sdata,
			struct cfg80211_assoc_request *req);
int ieee80211_mgd_deauth(struct ieee80211_sub_if_data *sdata,
			 struct cfg80211_deauth_request *req,
			 void *cookie);
int ieee80211_mgd_disassoc(struct ieee80211_sub_if_data *sdata,
			   struct cfg80211_disassoc_request *req,
			   void *cookie);
void ieee80211_send_pspoll(struct ieee80211_local *local,
			   struct ieee80211_sub_if_data *sdata);
void ieee80211_recalc_ps(struct ieee80211_local *local, s32 latency);
int ieee80211_max_network_latency(struct notifier_block *nb,
				  unsigned long data, void *dummy);
int ieee80211_set_arp_filter(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_process_chanswitch(struct ieee80211_sub_if_data *sdata,
				      struct ieee80211_channel_sw_ie *sw_elem,
				      struct ieee80211_bss *bss,
				      u64 timestamp);
void ieee80211_sta_quiesce(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_restart(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_rx_queued_mgmt(struct ieee80211_sub_if_data *sdata,
				  struct sk_buff *skb);
void ieee80211_sta_reset_beacon_monitor(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_reset_conn_monitor(struct ieee80211_sub_if_data *sdata);

/* IBSS code */
void ieee80211_ibss_notify_scan_completed(struct ieee80211_local *local);
void ieee80211_ibss_setup_sdata(struct ieee80211_sub_if_data *sdata);
struct sta_info *ieee80211_ibss_add_sta(struct ieee80211_sub_if_data *sdata,
					u8 *bssid, u8 *addr, u32 supp_rates,
					gfp_t gfp);
int ieee80211_ibss_join(struct ieee80211_sub_if_data *sdata,
			struct cfg80211_ibss_params *params);
int ieee80211_ibss_leave(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_quiesce(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_restart(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_rx_queued_mgmt(struct ieee80211_sub_if_data *sdata,
				   struct sk_buff *skb);

/* mesh code */
void ieee80211_mesh_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_mesh_rx_queued_mgmt(struct ieee80211_sub_if_data *sdata,
				   struct sk_buff *skb);

/* scan/BSS handling */
void ieee80211_scan_work(struct work_struct *work);
int ieee80211_request_internal_scan(struct ieee80211_sub_if_data *sdata,
				    const u8 *ssid, u8 ssid_len,
				    struct ieee80211_channel *chan);
int ieee80211_request_scan(struct ieee80211_sub_if_data *sdata,
			   struct cfg80211_scan_request *req);
void ieee80211_scan_cancel(struct ieee80211_local *local);
ieee80211_rx_result
ieee80211_scan_rx(struct ieee80211_sub_if_data *sdata, struct sk_buff *skb);

void ieee80211_mlme_notify_scan_completed(struct ieee80211_local *local);
struct ieee80211_bss *
ieee80211_bss_info_update(struct ieee80211_local *local,
			  struct ieee80211_rx_status *rx_status,
			  struct ieee80211_mgmt *mgmt,
			  size_t len,
			  struct ieee802_11_elems *elems,
			  struct ieee80211_channel *channel,
			  bool beacon);
struct ieee80211_bss *
ieee80211_rx_bss_get(struct ieee80211_local *local, u8 *bssid, int freq,
		     u8 *ssid, u8 ssid_len);
void ieee80211_rx_bss_put(struct ieee80211_local *local,
			  struct ieee80211_bss *bss);

/* off-channel helpers */
void ieee80211_offchannel_stop_beaconing(struct ieee80211_local *local);
void ieee80211_offchannel_stop_station(struct ieee80211_local *local);
void ieee80211_offchannel_return(struct ieee80211_local *local,
				 bool enable_beaconing);

/* interface handling */
int ieee80211_iface_init(void);
void ieee80211_iface_exit(void);
int ieee80211_if_add(struct ieee80211_local *local, const char *name,
		     struct net_device **new_dev, enum nl80211_iftype type,
		     struct vif_params *params);
int ieee80211_if_change_type(struct ieee80211_sub_if_data *sdata,
			     enum nl80211_iftype type);
void ieee80211_if_remove(struct ieee80211_sub_if_data *sdata);
void ieee80211_remove_interfaces(struct ieee80211_local *local);
u32 __ieee80211_recalc_idle(struct ieee80211_local *local);
void ieee80211_recalc_idle(struct ieee80211_local *local);

static inline bool ieee80211_sdata_running(struct ieee80211_sub_if_data *sdata)
{
	return netif_running(sdata->dev);
}

/* tx handling */
void ieee80211_clear_tx_pending(struct ieee80211_local *local);
void ieee80211_tx_pending(unsigned long data);
netdev_tx_t ieee80211_monitor_start_xmit(struct sk_buff *skb,
					 struct net_device *dev);
netdev_tx_t ieee80211_subif_start_xmit(struct sk_buff *skb,
				       struct net_device *dev);

/*
 * radiotap header for status frames
 */
struct ieee80211_tx_status_rtap_hdr {
	struct ieee80211_radiotap_header hdr;
	u8 rate;
	u8 padding_for_rate;
	__le16 tx_flags;
	u8 data_retries;
} __packed;


/* HT */
void ieee80211_ht_cap_ie_to_sta_ht_cap(struct ieee80211_supported_band *sband,
				       struct ieee80211_ht_cap *ht_cap_ie,
				       struct ieee80211_sta_ht_cap *ht_cap);
void ieee80211_send_bar(struct ieee80211_sub_if_data *sdata, u8 *ra, u16 tid, u16 ssn);
void ieee80211_send_delba(struct ieee80211_sub_if_data *sdata,
			  const u8 *da, u16 tid,
			  u16 initiator, u16 reason_code);
int ieee80211_send_smps_action(struct ieee80211_sub_if_data *sdata,
			       enum ieee80211_smps_mode smps, const u8 *da,
			       const u8 *bssid);

void ___ieee80211_stop_rx_ba_session(struct sta_info *sta, u16 tid,
				     u16 initiator, u16 reason);
void __ieee80211_stop_rx_ba_session(struct sta_info *sta, u16 tid,
				    u16 initiator, u16 reason);
void ieee80211_sta_tear_down_BA_sessions(struct sta_info *sta);
void ieee80211_process_delba(struct ieee80211_sub_if_data *sdata,
			     struct sta_info *sta,
			     struct ieee80211_mgmt *mgmt, size_t len);
void ieee80211_process_addba_resp(struct ieee80211_local *local,
				  struct sta_info *sta,
				  struct ieee80211_mgmt *mgmt,
				  size_t len);
void ieee80211_process_addba_request(struct ieee80211_local *local,
				     struct sta_info *sta,
				     struct ieee80211_mgmt *mgmt,
				     size_t len);

int __ieee80211_stop_tx_ba_session(struct sta_info *sta, u16 tid,
				   enum ieee80211_back_parties initiator);
int ___ieee80211_stop_tx_ba_session(struct sta_info *sta, u16 tid,
				    enum ieee80211_back_parties initiator);
void ieee80211_start_tx_ba_cb(struct ieee80211_vif *vif, u8 *ra, u16 tid);
void ieee80211_stop_tx_ba_cb(struct ieee80211_vif *vif, u8 *ra, u8 tid);
void ieee80211_ba_session_work(struct work_struct *work);
void ieee80211_tx_ba_session_handle_start(struct sta_info *sta, int tid);

/* Spectrum management */
void ieee80211_process_measurement_req(struct ieee80211_sub_if_data *sdata,
				       struct ieee80211_mgmt *mgmt,
				       size_t len);

/* Suspend/resume and hw reconfiguration */
int ieee80211_reconfig(struct ieee80211_local *local);
void ieee80211_stop_device(struct ieee80211_local *local);

#ifdef CONFIG_PM
int __ieee80211_suspend(struct ieee80211_hw *hw);

static inline int __ieee80211_resume(struct ieee80211_hw *hw)
{
	return ieee80211_reconfig(hw_to_local(hw));
}
#else
static inline int __ieee80211_suspend(struct ieee80211_hw *hw)
{
	return 0;
}

static inline int __ieee80211_resume(struct ieee80211_hw *hw)
{
	return 0;
}
#endif

/* utility functions/constants */
extern void *mac80211_wiphy_privid; /* for wiphy privid */
u8 *ieee80211_get_bssid(struct ieee80211_hdr *hdr, size_t len,
			enum nl80211_iftype type);
int ieee80211_frame_duration(struct ieee80211_local *local, size_t len,
			     int rate, int erp, int short_preamble);
void mac80211_ev_michael_mic_failure(struct ieee80211_sub_if_data *sdata, int keyidx,
				     struct ieee80211_hdr *hdr, const u8 *tsc,
				     gfp_t gfp);
void ieee80211_set_wmm_default(struct ieee80211_sub_if_data *sdata);
void ieee80211_tx_skb(struct ieee80211_sub_if_data *sdata, struct sk_buff *skb);
void ieee802_11_parse_elems(u8 *start, size_t len,
			    struct ieee802_11_elems *elems);
u32 ieee802_11_parse_elems_crc(u8 *start, size_t len,
			       struct ieee802_11_elems *elems,
			       u64 filter, u32 crc);
u32 ieee80211_mandatory_rates(struct ieee80211_local *local,
			      enum ieee80211_band band);

void ieee80211_dynamic_ps_enable_work(struct work_struct *work);
void ieee80211_dynamic_ps_disable_work(struct work_struct *work);
void ieee80211_dynamic_ps_timer(unsigned long data);
void ieee80211_send_nullfunc(struct ieee80211_local *local,
			     struct ieee80211_sub_if_data *sdata,
			     int powersave);
void ieee80211_sta_rx_notify(struct ieee80211_sub_if_data *sdata,
			     struct ieee80211_hdr *hdr);
void ieee80211_beacon_connection_loss_work(struct work_struct *work);

void ieee80211_wake_queues_by_reason(struct ieee80211_hw *hw,
				     enum queue_stop_reason reason);
void ieee80211_stop_queues_by_reason(struct ieee80211_hw *hw,
				     enum queue_stop_reason reason);
void ieee80211_wake_queue_by_reason(struct ieee80211_hw *hw, int queue,
				    enum queue_stop_reason reason);
void ieee80211_stop_queue_by_reason(struct ieee80211_hw *hw, int queue,
				    enum queue_stop_reason reason);
void ieee80211_add_pending_skb(struct ieee80211_local *local,
			       struct sk_buff *skb);
int ieee80211_add_pending_skbs(struct ieee80211_local *local,
			       struct sk_buff_head *skbs);

void ieee80211_send_auth(struct ieee80211_sub_if_data *sdata,
			 u16 transaction, u16 auth_alg,
			 u8 *extra, size_t extra_len, const u8 *bssid,
			 const u8 *key, u8 key_len, u8 key_idx);
int ieee80211_build_preq_ies(struct ieee80211_local *local, u8 *buffer,
			     const u8 *ie, size_t ie_len,
			     enum ieee80211_band band);
void ieee80211_send_probe_req(struct ieee80211_sub_if_data *sdata, u8 *dst,
			      const u8 *ssid, size_t ssid_len,
			      const u8 *ie, size_t ie_len);

void ieee80211_sta_def_wmm_params(struct ieee80211_sub_if_data *sdata,
				  const size_t supp_rates_len,
				  const u8 *supp_rates);
u32 ieee80211_sta_get_rates(struct ieee80211_local *local,
			    struct ieee802_11_elems *elems,
			    enum ieee80211_band band);
int __ieee80211_request_smps(struct ieee80211_sub_if_data *sdata,
			     enum ieee80211_smps_mode smps_mode);
void ieee80211_recalc_smps(struct ieee80211_local *local,
			   struct ieee80211_sub_if_data *forsdata);

size_t ieee80211_ie_split(const u8 *ies, size_t ielen,
			  const u8 *ids, int n_ids, size_t offset);
size_t ieee80211_ie_split_vendor(const u8 *ies, size_t ielen, size_t offset);

/* internal work items */
void ieee80211_work_init(struct ieee80211_local *local);
void ieee80211_add_work(struct ieee80211_work *wk);
void free_work(struct ieee80211_work *wk);
void ieee80211_work_purge(struct ieee80211_sub_if_data *sdata);
ieee80211_rx_result ieee80211_work_rx_mgmt(struct ieee80211_sub_if_data *sdata,
					   struct sk_buff *skb);
int ieee80211_wk_remain_on_channel(struct ieee80211_sub_if_data *sdata,
				   struct ieee80211_channel *chan,
				   enum nl80211_channel_type channel_type,
				   unsigned int duration, u64 *cookie);
int ieee80211_wk_cancel_remain_on_channel(
	struct ieee80211_sub_if_data *sdata, u64 cookie);

/* channel management */
enum ieee80211_chan_mode {
	CHAN_MODE_UNDEFINED,
	CHAN_MODE_HOPPING,
	CHAN_MODE_FIXED,
};

enum ieee80211_chan_mode
ieee80211_get_channel_mode(struct ieee80211_local *local,
			   struct ieee80211_sub_if_data *ignore);
bool ieee80211_set_channel_type(struct ieee80211_local *local,
				struct ieee80211_sub_if_data *sdata,
				enum nl80211_channel_type chantype);

#ifdef CONFIG_MAC80211_NOINLINE
#define debug_noinline noinline
#else
#define debug_noinline
#endif

#endif /* IEEE80211_I_H */
