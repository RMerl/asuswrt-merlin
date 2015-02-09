/******************************************************************************
 *
 * Copyright(c) 2003 - 2010 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *****************************************************************************/
/*
 * Please use this file (iwl-3945.h) for driver implementation definitions.
 * Please use iwl-3945-commands.h for uCode API definitions.
 * Please use iwl-3945-hw.h for hardware-related definitions.
 */

#ifndef __iwl_3945_h__
#define __iwl_3945_h__

#include <linux/pci.h> /* for struct pci_device_id */
#include <linux/kernel.h>
#include <net/ieee80211_radiotap.h>

/* Hardware specific file defines the PCI IDs table for that hardware module */
extern const struct pci_device_id iwl3945_hw_card_ids[];

#include "iwl-csr.h"
#include "iwl-prph.h"
#include "iwl-fh.h"
#include "iwl-3945-hw.h"
#include "iwl-debug.h"
#include "iwl-power.h"
#include "iwl-dev.h"
#include "iwl-led.h"

/* Highest firmware API version supported */
#define IWL3945_UCODE_API_MAX 2

/* Lowest firmware API version supported */
#define IWL3945_UCODE_API_MIN 1

#define IWL3945_FW_PRE	"iwlwifi-3945-"
#define _IWL3945_MODULE_FIRMWARE(api) IWL3945_FW_PRE #api ".ucode"
#define IWL3945_MODULE_FIRMWARE(api) _IWL3945_MODULE_FIRMWARE(api)

/* Default noise level to report when noise measurement is not available.
 *   This may be because we're:
 *   1)  Not associated (4965, no beacon statistics being sent to driver)
 *   2)  Scanning (noise measurement does not apply to associated channel)
 *   3)  Receiving CCK (3945 delivers noise info only for OFDM frames)
 * Use default noise value of -127 ... this is below the range of measurable
 *   Rx dBm for either 3945 or 4965, so it can indicate "unmeasurable" to user.
 *   Also, -127 works better than 0 when averaging frames with/without
 *   noise info (e.g. averaging might be done in app); measured dBm values are
 *   always negative ... using a negative value as the default keeps all
 *   averages within an s8's (used in some apps) range of negative values. */
#define IWL_NOISE_MEAS_NOT_AVAILABLE (-127)

/* Module parameters accessible from iwl-*.c */
extern struct iwl_mod_params iwl3945_mod_params;

struct iwl3945_rate_scale_data {
	u64 data;
	s32 success_counter;
	s32 success_ratio;
	s32 counter;
	s32 average_tpt;
	unsigned long stamp;
};

struct iwl3945_rs_sta {
	spinlock_t lock;
	struct iwl_priv *priv;
	s32 *expected_tpt;
	unsigned long last_partial_flush;
	unsigned long last_flush;
	u32 flush_time;
	u32 last_tx_packets;
	u32 tx_packets;
	u8 tgg;
	u8 flush_pending;
	u8 start_rate;
	struct timer_list rate_scale_flush;
	struct iwl3945_rate_scale_data win[IWL_RATE_COUNT_3945];
#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *rs_sta_dbgfs_stats_table_file;
#endif

	/* used to be in sta_info */
	int last_txrate_idx;
};


/*
 * The common struct MUST be first because it is shared between
 * 3945 and agn!
 */
struct iwl3945_sta_priv {
	struct iwl_station_priv_common common;
	struct iwl3945_rs_sta rs_sta;
};

enum iwl3945_antenna {
	IWL_ANTENNA_DIVERSITY,
	IWL_ANTENNA_MAIN,
	IWL_ANTENNA_AUX
};

/*
 * RTS threshold here is total size [2347] minus 4 FCS bytes
 * Per spec:
 *   a value of 0 means RTS on all data/management packets
 *   a value > max MSDU size means no RTS
 * else RTS for data/management frames where MPDU is larger
 *   than RTS value.
 */
#define DEFAULT_RTS_THRESHOLD     2347U
#define MIN_RTS_THRESHOLD         0U
#define MAX_RTS_THRESHOLD         2347U
#define MAX_MSDU_SIZE		  2304U
#define MAX_MPDU_SIZE		  2346U
#define DEFAULT_BEACON_INTERVAL   100U
#define	DEFAULT_SHORT_RETRY_LIMIT 7U
#define	DEFAULT_LONG_RETRY_LIMIT  4U

#include "iwl-agn-rs.h"

#define IWL_TX_FIFO_AC0	0
#define IWL_TX_FIFO_AC1	1
#define IWL_TX_FIFO_AC2	2
#define IWL_TX_FIFO_AC3	3
#define IWL_TX_FIFO_HCCA_1	5
#define IWL_TX_FIFO_HCCA_2	6
#define IWL_TX_FIFO_NONE	7

#define IEEE80211_DATA_LEN              2304
#define IEEE80211_4ADDR_LEN             30
#define IEEE80211_HLEN                  (IEEE80211_4ADDR_LEN)
#define IEEE80211_FRAME_LEN             (IEEE80211_DATA_LEN + IEEE80211_HLEN)

struct iwl3945_frame {
	union {
		struct ieee80211_hdr frame;
		struct iwl3945_tx_beacon_cmd beacon;
		u8 raw[IEEE80211_FRAME_LEN];
		u8 cmd[360];
	} u;
	struct list_head list;
};

#define SEQ_TO_SN(seq) (((seq) & IEEE80211_SCTL_SEQ) >> 4)
#define SN_TO_SEQ(ssn) (((ssn) << 4) & IEEE80211_SCTL_SEQ)
#define MAX_SN ((IEEE80211_SCTL_SEQ) >> 4)

#define SUP_RATE_11A_MAX_NUM_CHANNELS  8
#define SUP_RATE_11B_MAX_NUM_CHANNELS  4
#define SUP_RATE_11G_MAX_NUM_CHANNELS  12

#define IWL_SUPPORTED_RATES_IE_LEN         8

#define SCAN_INTERVAL 100

#define MAX_TID_COUNT        9

#define IWL_INVALID_RATE     0xFF
#define IWL_INVALID_VALUE    -1

#define STA_PS_STATUS_WAKE             0
#define STA_PS_STATUS_SLEEP            1

struct iwl3945_ibss_seq {
	u8 mac[ETH_ALEN];
	u16 seq_num;
	u16 frag_num;
	unsigned long packet_time;
	struct list_head list;
};

#define IWL_RX_HDR(x) ((struct iwl3945_rx_frame_hdr *)(\
		       x->u.rx_frame.stats.payload + \
		       x->u.rx_frame.stats.phy_count))
#define IWL_RX_END(x) ((struct iwl3945_rx_frame_end *)(\
		       IWL_RX_HDR(x)->payload + \
		       le16_to_cpu(IWL_RX_HDR(x)->len)))
#define IWL_RX_STATS(x) (&x->u.rx_frame.stats)
#define IWL_RX_DATA(x) (IWL_RX_HDR(x)->payload)


/******************************************************************************
 *
 * Functions implemented in iwl-base.c which are forward declared here
 * for use by iwl-*.c
 *
 *****************************************************************************/
extern int iwl3945_calc_db_from_ratio(int sig_ratio);
extern void iwl3945_rx_replenish(void *data);
extern void iwl3945_rx_queue_reset(struct iwl_priv *priv, struct iwl_rx_queue *rxq);
extern unsigned int iwl3945_fill_beacon_frame(struct iwl_priv *priv,
					struct ieee80211_hdr *hdr,int left);
extern int iwl3945_dump_nic_event_log(struct iwl_priv *priv, bool full_log,
				       char **buf, bool display);
extern void iwl3945_dump_nic_error_log(struct iwl_priv *priv);

extern void iwl3945_hw_rx_handler_setup(struct iwl_priv *priv);
extern void iwl3945_hw_setup_deferred_work(struct iwl_priv *priv);
extern void iwl3945_hw_cancel_deferred_work(struct iwl_priv *priv);
extern int iwl3945_hw_rxq_stop(struct iwl_priv *priv);
extern int iwl3945_hw_set_hw_params(struct iwl_priv *priv);
extern int iwl3945_hw_nic_init(struct iwl_priv *priv);
extern int iwl3945_hw_nic_stop_master(struct iwl_priv *priv);
extern void iwl3945_hw_txq_ctx_free(struct iwl_priv *priv);
extern void iwl3945_hw_txq_ctx_stop(struct iwl_priv *priv);
extern int iwl3945_hw_nic_reset(struct iwl_priv *priv);
extern int iwl3945_hw_txq_attach_buf_to_tfd(struct iwl_priv *priv,
					    struct iwl_tx_queue *txq,
					    dma_addr_t addr, u16 len,
					    u8 reset, u8 pad);
extern void iwl3945_hw_txq_free_tfd(struct iwl_priv *priv,
				    struct iwl_tx_queue *txq);
extern int iwl3945_hw_get_temperature(struct iwl_priv *priv);
extern int iwl3945_hw_tx_queue_init(struct iwl_priv *priv,
				struct iwl_tx_queue *txq);
extern unsigned int iwl3945_hw_get_beacon_cmd(struct iwl_priv *priv,
				 struct iwl3945_frame *frame, u8 rate);
void iwl3945_hw_build_tx_cmd_rate(struct iwl_priv *priv,
				  struct iwl_device_cmd *cmd,
				  struct ieee80211_tx_info *info,
				  struct ieee80211_hdr *hdr,
				  int sta_id, int tx_id);
extern int iwl3945_hw_reg_send_txpower(struct iwl_priv *priv);
extern int iwl3945_hw_reg_set_txpower(struct iwl_priv *priv, s8 power);
extern void iwl3945_hw_rx_statistics(struct iwl_priv *priv,
				 struct iwl_rx_mem_buffer *rxb);
void iwl3945_reply_statistics(struct iwl_priv *priv,
			      struct iwl_rx_mem_buffer *rxb);
extern void iwl3945_disable_events(struct iwl_priv *priv);
extern int iwl4965_get_temperature(const struct iwl_priv *priv);
extern void iwl3945_post_associate(struct iwl_priv *priv,
				   struct ieee80211_vif *vif);
extern void iwl3945_config_ap(struct iwl_priv *priv,
			      struct ieee80211_vif *vif);

/**
 * iwl3945_hw_find_station - Find station id for a given BSSID
 * @bssid: MAC address of station ID to find
 *
 * NOTE:  This should not be hardware specific but the code has
 * not yet been merged into a single common layer for managing the
 * station tables.
 */
extern u8 iwl3945_hw_find_station(struct iwl_priv *priv, const u8 *bssid);

/*
 * Forward declare iwl-3945.c functions for iwl-base.c
 */
extern __le32 iwl3945_get_antenna_flags(const struct iwl_priv *priv);
extern int iwl3945_init_hw_rate_table(struct iwl_priv *priv);
extern void iwl3945_reg_txpower_periodic(struct iwl_priv *priv);
extern int iwl3945_txpower_set_from_eeprom(struct iwl_priv *priv);

extern const struct iwl_channel_info *iwl3945_get_channel_info(
	const struct iwl_priv *priv, enum ieee80211_band band, u16 channel);

extern int iwl3945_rs_next_rate(struct iwl_priv *priv, int rate);

/* scanning */
void iwl3945_request_scan(struct iwl_priv *priv, struct ieee80211_vif *vif);

/* Requires full declaration of iwl_priv before including */
#include "iwl-io.h"

#endif
