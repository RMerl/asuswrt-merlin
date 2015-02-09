/******************************************************************************
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2010 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/etherdevice.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <net/mac80211.h>

#include "iwl-eeprom.h"
#include "iwl-dev.h"
#include "iwl-debug.h"
#include "iwl-core.h"
#include "iwl-io.h"
#include "iwl-power.h"
#include "iwl-sta.h"
#include "iwl-helpers.h"


MODULE_DESCRIPTION("iwl core");
MODULE_VERSION(IWLWIFI_VERSION);
MODULE_AUTHOR(DRV_COPYRIGHT " " DRV_AUTHOR);
MODULE_LICENSE("GPL");

/*
 * set bt_coex_active to true, uCode will do kill/defer
 * every time the priority line is asserted (BT is sending signals on the
 * priority line in the PCIx).
 * set bt_coex_active to false, uCode will ignore the BT activity and
 * perform the normal operation
 *
 * User might experience transmit issue on some platform due to WiFi/BT
 * co-exist problem. The possible behaviors are:
 *   Able to scan and finding all the available AP
 *   Not able to associate with any AP
 * On those platforms, WiFi communication can be restored by set
 * "bt_coex_active" module parameter to "false"
 *
 * default: bt_coex_active = true (BT_COEX_ENABLE)
 */
static bool bt_coex_active = true;
module_param(bt_coex_active, bool, S_IRUGO);
MODULE_PARM_DESC(bt_coex_active, "enable wifi/bluetooth co-exist");

#define IWL_DECLARE_RATE_INFO(r, s, ip, in, rp, rn, pp, np)    \
	[IWL_RATE_##r##M_INDEX] = { IWL_RATE_##r##M_PLCP,      \
				    IWL_RATE_SISO_##s##M_PLCP, \
				    IWL_RATE_MIMO2_##s##M_PLCP,\
				    IWL_RATE_MIMO3_##s##M_PLCP,\
				    IWL_RATE_##r##M_IEEE,      \
				    IWL_RATE_##ip##M_INDEX,    \
				    IWL_RATE_##in##M_INDEX,    \
				    IWL_RATE_##rp##M_INDEX,    \
				    IWL_RATE_##rn##M_INDEX,    \
				    IWL_RATE_##pp##M_INDEX,    \
				    IWL_RATE_##np##M_INDEX }

u32 iwl_debug_level;
EXPORT_SYMBOL(iwl_debug_level);

/*
 * Parameter order:
 *   rate, ht rate, prev rate, next rate, prev tgg rate, next tgg rate
 *
 * If there isn't a valid next or previous rate then INV is used which
 * maps to IWL_RATE_INVALID
 *
 */
const struct iwl_rate_info iwl_rates[IWL_RATE_COUNT] = {
	IWL_DECLARE_RATE_INFO(1, INV, INV, 2, INV, 2, INV, 2),    /*  1mbps */
	IWL_DECLARE_RATE_INFO(2, INV, 1, 5, 1, 5, 1, 5),          /*  2mbps */
	IWL_DECLARE_RATE_INFO(5, INV, 2, 6, 2, 11, 2, 11),        /*5.5mbps */
	IWL_DECLARE_RATE_INFO(11, INV, 9, 12, 9, 12, 5, 18),      /* 11mbps */
	IWL_DECLARE_RATE_INFO(6, 6, 5, 9, 5, 11, 5, 11),        /*  6mbps */
	IWL_DECLARE_RATE_INFO(9, 6, 6, 11, 6, 11, 5, 11),       /*  9mbps */
	IWL_DECLARE_RATE_INFO(12, 12, 11, 18, 11, 18, 11, 18),   /* 12mbps */
	IWL_DECLARE_RATE_INFO(18, 18, 12, 24, 12, 24, 11, 24),   /* 18mbps */
	IWL_DECLARE_RATE_INFO(24, 24, 18, 36, 18, 36, 18, 36),   /* 24mbps */
	IWL_DECLARE_RATE_INFO(36, 36, 24, 48, 24, 48, 24, 48),   /* 36mbps */
	IWL_DECLARE_RATE_INFO(48, 48, 36, 54, 36, 54, 36, 54),   /* 48mbps */
	IWL_DECLARE_RATE_INFO(54, 54, 48, INV, 48, INV, 48, INV),/* 54mbps */
	IWL_DECLARE_RATE_INFO(60, 60, 48, INV, 48, INV, 48, INV),/* 60mbps */
};
EXPORT_SYMBOL(iwl_rates);

int iwl_hwrate_to_plcp_idx(u32 rate_n_flags)
{
	int idx = 0;

	/* HT rate format */
	if (rate_n_flags & RATE_MCS_HT_MSK) {
		idx = (rate_n_flags & 0xff);

		if (idx >= IWL_RATE_MIMO3_6M_PLCP)
			idx = idx - IWL_RATE_MIMO3_6M_PLCP;
		else if (idx >= IWL_RATE_MIMO2_6M_PLCP)
			idx = idx - IWL_RATE_MIMO2_6M_PLCP;

		idx += IWL_FIRST_OFDM_RATE;
		/* skip 9M not supported in ht*/
		if (idx >= IWL_RATE_9M_INDEX)
			idx += 1;
		if ((idx >= IWL_FIRST_OFDM_RATE) && (idx <= IWL_LAST_OFDM_RATE))
			return idx;

	/* legacy rate format, search for match in table */
	} else {
		for (idx = 0; idx < ARRAY_SIZE(iwl_rates); idx++)
			if (iwl_rates[idx].plcp == (rate_n_flags & 0xFF))
				return idx;
	}

	return -1;
}
EXPORT_SYMBOL(iwl_hwrate_to_plcp_idx);

u8 iwl_toggle_tx_ant(struct iwl_priv *priv, u8 ant, u8 valid)
{
	int i;
	u8 ind = ant;

	for (i = 0; i < RATE_ANT_NUM - 1; i++) {
		ind = (ind + 1) < RATE_ANT_NUM ?  ind + 1 : 0;
		if (valid & BIT(ind))
			return ind;
	}
	return ant;
}
EXPORT_SYMBOL(iwl_toggle_tx_ant);

const u8 iwl_bcast_addr[ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
EXPORT_SYMBOL(iwl_bcast_addr);


/* This function both allocates and initializes hw and priv. */
struct ieee80211_hw *iwl_alloc_all(struct iwl_cfg *cfg,
		struct ieee80211_ops *hw_ops)
{
	struct iwl_priv *priv;

	/* mac80211 allocates memory for this device instance, including
	 *   space for this driver's private structure */
	struct ieee80211_hw *hw =
		ieee80211_alloc_hw(sizeof(struct iwl_priv), hw_ops);
	if (hw == NULL) {
		pr_err("%s: Can not allocate network device\n",
		       cfg->name);
		goto out;
	}

	priv = hw->priv;
	priv->hw = hw;

out:
	return hw;
}
EXPORT_SYMBOL(iwl_alloc_all);

void iwl_hw_detect(struct iwl_priv *priv)
{
	priv->hw_rev = _iwl_read32(priv, CSR_HW_REV);
	priv->hw_wa_rev = _iwl_read32(priv, CSR_HW_REV_WA_REG);
	pci_read_config_byte(priv->pci_dev, PCI_REVISION_ID, &priv->rev_id);
}
EXPORT_SYMBOL(iwl_hw_detect);

/*
 * QoS  support
*/
static void iwl_update_qos(struct iwl_priv *priv)
{
	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	priv->qos_data.def_qos_parm.qos_flags = 0;

	if (priv->qos_data.qos_active)
		priv->qos_data.def_qos_parm.qos_flags |=
			QOS_PARAM_FLG_UPDATE_EDCA_MSK;

	if (priv->current_ht_config.is_ht)
		priv->qos_data.def_qos_parm.qos_flags |= QOS_PARAM_FLG_TGN_MSK;

	IWL_DEBUG_QOS(priv, "send QoS cmd with Qos active=%d FLAGS=0x%X\n",
		      priv->qos_data.qos_active,
		      priv->qos_data.def_qos_parm.qos_flags);

	iwl_send_cmd_pdu_async(priv, REPLY_QOS_PARAM,
			       sizeof(struct iwl_qosparam_cmd),
			       &priv->qos_data.def_qos_parm, NULL);
}

#define MAX_BIT_RATE_40_MHZ 150 /* Mbps */
#define MAX_BIT_RATE_20_MHZ 72 /* Mbps */
static void iwlcore_init_ht_hw_capab(const struct iwl_priv *priv,
			      struct ieee80211_sta_ht_cap *ht_info,
			      enum ieee80211_band band)
{
	u16 max_bit_rate = 0;
	u8 rx_chains_num = priv->hw_params.rx_chains_num;
	u8 tx_chains_num = priv->hw_params.tx_chains_num;

	ht_info->cap = 0;
	memset(&ht_info->mcs, 0, sizeof(ht_info->mcs));

	ht_info->ht_supported = true;

	if (priv->cfg->ht_greenfield_support)
		ht_info->cap |= IEEE80211_HT_CAP_GRN_FLD;
	ht_info->cap |= IEEE80211_HT_CAP_SGI_20;
	max_bit_rate = MAX_BIT_RATE_20_MHZ;
	if (priv->hw_params.ht40_channel & BIT(band)) {
		ht_info->cap |= IEEE80211_HT_CAP_SUP_WIDTH_20_40;
		ht_info->cap |= IEEE80211_HT_CAP_SGI_40;
		ht_info->mcs.rx_mask[4] = 0x01;
		max_bit_rate = MAX_BIT_RATE_40_MHZ;
	}

	if (priv->cfg->mod_params->amsdu_size_8K)
		ht_info->cap |= IEEE80211_HT_CAP_MAX_AMSDU;

	ht_info->ampdu_factor = CFG_HT_RX_AMPDU_FACTOR_DEF;
	ht_info->ampdu_density = CFG_HT_MPDU_DENSITY_DEF;

	ht_info->mcs.rx_mask[0] = 0xFF;
	if (rx_chains_num >= 2)
		ht_info->mcs.rx_mask[1] = 0xFF;
	if (rx_chains_num >= 3)
		ht_info->mcs.rx_mask[2] = 0xFF;

	/* Highest supported Rx data rate */
	max_bit_rate *= rx_chains_num;
	WARN_ON(max_bit_rate & ~IEEE80211_HT_MCS_RX_HIGHEST_MASK);
	ht_info->mcs.rx_highest = cpu_to_le16(max_bit_rate);

	/* Tx MCS capabilities */
	ht_info->mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
	if (tx_chains_num != rx_chains_num) {
		ht_info->mcs.tx_params |= IEEE80211_HT_MCS_TX_RX_DIFF;
		ht_info->mcs.tx_params |= ((tx_chains_num - 1) <<
				IEEE80211_HT_MCS_TX_MAX_STREAMS_SHIFT);
	}
}

/**
 * iwlcore_init_geos - Initialize mac80211's geo/channel info based from eeprom
 */
int iwlcore_init_geos(struct iwl_priv *priv)
{
	struct iwl_channel_info *ch;
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *channels;
	struct ieee80211_channel *geo_ch;
	struct ieee80211_rate *rates;
	int i = 0;

	if (priv->bands[IEEE80211_BAND_2GHZ].n_bitrates ||
	    priv->bands[IEEE80211_BAND_5GHZ].n_bitrates) {
		IWL_DEBUG_INFO(priv, "Geography modes already initialized.\n");
		set_bit(STATUS_GEO_CONFIGURED, &priv->status);
		return 0;
	}

	channels = kzalloc(sizeof(struct ieee80211_channel) *
			   priv->channel_count, GFP_KERNEL);
	if (!channels)
		return -ENOMEM;

	rates = kzalloc((sizeof(struct ieee80211_rate) * IWL_RATE_COUNT_LEGACY),
			GFP_KERNEL);
	if (!rates) {
		kfree(channels);
		return -ENOMEM;
	}

	/* 5.2GHz channels start after the 2.4GHz channels */
	sband = &priv->bands[IEEE80211_BAND_5GHZ];
	sband->channels = &channels[ARRAY_SIZE(iwl_eeprom_band_1)];
	/* just OFDM */
	sband->bitrates = &rates[IWL_FIRST_OFDM_RATE];
	sband->n_bitrates = IWL_RATE_COUNT_LEGACY - IWL_FIRST_OFDM_RATE;

	if (priv->cfg->sku & IWL_SKU_N)
		iwlcore_init_ht_hw_capab(priv, &sband->ht_cap,
					 IEEE80211_BAND_5GHZ);

	sband = &priv->bands[IEEE80211_BAND_2GHZ];
	sband->channels = channels;
	/* OFDM & CCK */
	sband->bitrates = rates;
	sband->n_bitrates = IWL_RATE_COUNT_LEGACY;

	if (priv->cfg->sku & IWL_SKU_N)
		iwlcore_init_ht_hw_capab(priv, &sband->ht_cap,
					 IEEE80211_BAND_2GHZ);

	priv->ieee_channels = channels;
	priv->ieee_rates = rates;

	for (i = 0;  i < priv->channel_count; i++) {
		ch = &priv->channel_info[i];

		if (!is_channel_valid(ch))
			continue;

		if (is_channel_a_band(ch))
			sband =  &priv->bands[IEEE80211_BAND_5GHZ];
		else
			sband =  &priv->bands[IEEE80211_BAND_2GHZ];

		geo_ch = &sband->channels[sband->n_channels++];

		geo_ch->center_freq =
				ieee80211_channel_to_frequency(ch->channel);
		geo_ch->max_power = ch->max_power_avg;
		geo_ch->max_antenna_gain = 0xff;
		geo_ch->hw_value = ch->channel;

		if (is_channel_valid(ch)) {
			if (!(ch->flags & EEPROM_CHANNEL_IBSS))
				geo_ch->flags |= IEEE80211_CHAN_NO_IBSS;

			if (!(ch->flags & EEPROM_CHANNEL_ACTIVE))
				geo_ch->flags |= IEEE80211_CHAN_PASSIVE_SCAN;

			if (ch->flags & EEPROM_CHANNEL_RADAR)
				geo_ch->flags |= IEEE80211_CHAN_RADAR;

			geo_ch->flags |= ch->ht40_extension_channel;

			if (ch->max_power_avg > priv->tx_power_device_lmt)
				priv->tx_power_device_lmt = ch->max_power_avg;
		} else {
			geo_ch->flags |= IEEE80211_CHAN_DISABLED;
		}

		IWL_DEBUG_INFO(priv, "Channel %d Freq=%d[%sGHz] %s flag=0x%X\n",
				ch->channel, geo_ch->center_freq,
				is_channel_a_band(ch) ?  "5.2" : "2.4",
				geo_ch->flags & IEEE80211_CHAN_DISABLED ?
				"restricted" : "valid",
				 geo_ch->flags);
	}

	if ((priv->bands[IEEE80211_BAND_5GHZ].n_channels == 0) &&
	     priv->cfg->sku & IWL_SKU_A) {
		IWL_INFO(priv, "Incorrectly detected BG card as ABG. "
			"Please send your PCI ID 0x%04X:0x%04X to maintainer.\n",
			   priv->pci_dev->device,
			   priv->pci_dev->subsystem_device);
		priv->cfg->sku &= ~IWL_SKU_A;
	}

	IWL_INFO(priv, "Tunable channels: %d 802.11bg, %d 802.11a channels\n",
		   priv->bands[IEEE80211_BAND_2GHZ].n_channels,
		   priv->bands[IEEE80211_BAND_5GHZ].n_channels);

	set_bit(STATUS_GEO_CONFIGURED, &priv->status);

	return 0;
}
EXPORT_SYMBOL(iwlcore_init_geos);

/*
 * iwlcore_free_geos - undo allocations in iwlcore_init_geos
 */
void iwlcore_free_geos(struct iwl_priv *priv)
{
	kfree(priv->ieee_channels);
	kfree(priv->ieee_rates);
	clear_bit(STATUS_GEO_CONFIGURED, &priv->status);
}
EXPORT_SYMBOL(iwlcore_free_geos);

/*
 *  iwlcore_tx_cmd_protection: Set rts/cts. 3945 and 4965 only share this
 *  function.
 */
void iwlcore_tx_cmd_protection(struct iwl_priv *priv,
			       struct ieee80211_tx_info *info,
			       __le16 fc, __le32 *tx_flags)
{
	if (info->control.rates[0].flags & IEEE80211_TX_RC_USE_RTS_CTS) {
		*tx_flags |= TX_CMD_FLG_RTS_MSK;
		*tx_flags &= ~TX_CMD_FLG_CTS_MSK;
		*tx_flags |= TX_CMD_FLG_FULL_TXOP_PROT_MSK;

		if (!ieee80211_is_mgmt(fc))
			return;

		switch (fc & cpu_to_le16(IEEE80211_FCTL_STYPE)) {
		case cpu_to_le16(IEEE80211_STYPE_AUTH):
		case cpu_to_le16(IEEE80211_STYPE_DEAUTH):
		case cpu_to_le16(IEEE80211_STYPE_ASSOC_REQ):
		case cpu_to_le16(IEEE80211_STYPE_REASSOC_REQ):
			*tx_flags &= ~TX_CMD_FLG_RTS_MSK;
			*tx_flags |= TX_CMD_FLG_CTS_MSK;
			break;
		}
	} else if (info->control.rates[0].flags & IEEE80211_TX_RC_USE_CTS_PROTECT) {
		*tx_flags &= ~TX_CMD_FLG_RTS_MSK;
		*tx_flags |= TX_CMD_FLG_CTS_MSK;
		*tx_flags |= TX_CMD_FLG_FULL_TXOP_PROT_MSK;
	}
}
EXPORT_SYMBOL(iwlcore_tx_cmd_protection);


static bool is_single_rx_stream(struct iwl_priv *priv)
{
	return priv->current_ht_config.smps == IEEE80211_SMPS_STATIC ||
	       priv->current_ht_config.single_chain_sufficient;
}

static u8 iwl_is_channel_extension(struct iwl_priv *priv,
				   enum ieee80211_band band,
				   u16 channel, u8 extension_chan_offset)
{
	const struct iwl_channel_info *ch_info;

	ch_info = iwl_get_channel_info(priv, band, channel);
	if (!is_channel_valid(ch_info))
		return 0;

	if (extension_chan_offset == IEEE80211_HT_PARAM_CHA_SEC_ABOVE)
		return !(ch_info->ht40_extension_channel &
					IEEE80211_CHAN_NO_HT40PLUS);
	else if (extension_chan_offset == IEEE80211_HT_PARAM_CHA_SEC_BELOW)
		return !(ch_info->ht40_extension_channel &
					IEEE80211_CHAN_NO_HT40MINUS);

	return 0;
}

u8 iwl_is_ht40_tx_allowed(struct iwl_priv *priv,
			 struct ieee80211_sta_ht_cap *sta_ht_inf)
{
	struct iwl_ht_config *ht_conf = &priv->current_ht_config;

	if (!ht_conf->is_ht || !ht_conf->is_40mhz)
		return 0;

	/* We do not check for IEEE80211_HT_CAP_SUP_WIDTH_20_40
	 * the bit will not set if it is pure 40MHz case
	 */
	if (sta_ht_inf) {
		if (!sta_ht_inf->ht_supported)
			return 0;
	}
#ifdef CONFIG_IWLWIFI_DEBUGFS
	if (priv->disable_ht40)
		return 0;
#endif
	return iwl_is_channel_extension(priv, priv->band,
			le16_to_cpu(priv->staging_rxon.channel),
			ht_conf->extension_chan_offset);
}
EXPORT_SYMBOL(iwl_is_ht40_tx_allowed);

static u16 iwl_adjust_beacon_interval(u16 beacon_val, u16 max_beacon_val)
{
	u16 new_val = 0;
	u16 beacon_factor = 0;

	beacon_factor = (beacon_val + max_beacon_val) / max_beacon_val;
	new_val = beacon_val / beacon_factor;

	if (!new_val)
		new_val = max_beacon_val;

	return new_val;
}

void iwl_setup_rxon_timing(struct iwl_priv *priv, struct ieee80211_vif *vif)
{
	u64 tsf;
	s32 interval_tm, rem;
	unsigned long flags;
	struct ieee80211_conf *conf = NULL;
	u16 beacon_int;

	conf = ieee80211_get_hw_conf(priv->hw);

	spin_lock_irqsave(&priv->lock, flags);
	priv->rxon_timing.timestamp = cpu_to_le64(priv->timestamp);
	priv->rxon_timing.listen_interval = cpu_to_le16(conf->listen_interval);

	beacon_int = vif->bss_conf.beacon_int;

	if (vif->type == NL80211_IFTYPE_ADHOC) {
		/* TODO: we need to get atim_window from upper stack
		 * for now we set to 0 */
		priv->rxon_timing.atim_window = 0;
	} else {
		priv->rxon_timing.atim_window = 0;
	}

	beacon_int = iwl_adjust_beacon_interval(beacon_int,
				priv->hw_params.max_beacon_itrvl * TIME_UNIT);
	priv->rxon_timing.beacon_interval = cpu_to_le16(beacon_int);

	tsf = priv->timestamp; /* tsf is modifed by do_div: copy it */
	interval_tm = beacon_int * TIME_UNIT;
	rem = do_div(tsf, interval_tm);
	priv->rxon_timing.beacon_init_val = cpu_to_le32(interval_tm - rem);

	spin_unlock_irqrestore(&priv->lock, flags);
	IWL_DEBUG_ASSOC(priv,
			"beacon interval %d beacon timer %d beacon tim %d\n",
			le16_to_cpu(priv->rxon_timing.beacon_interval),
			le32_to_cpu(priv->rxon_timing.beacon_init_val),
			le16_to_cpu(priv->rxon_timing.atim_window));
}
EXPORT_SYMBOL(iwl_setup_rxon_timing);

void iwl_set_rxon_hwcrypto(struct iwl_priv *priv, int hw_decrypt)
{
	struct iwl_rxon_cmd *rxon = &priv->staging_rxon;

	if (hw_decrypt)
		rxon->filter_flags &= ~RXON_FILTER_DIS_DECRYPT_MSK;
	else
		rxon->filter_flags |= RXON_FILTER_DIS_DECRYPT_MSK;

}
EXPORT_SYMBOL(iwl_set_rxon_hwcrypto);

/**
 * iwl_check_rxon_cmd - validate RXON structure is valid
 *
 * NOTE:  This is really only useful during development and can eventually
 * be #ifdef'd out once the driver is stable and folks aren't actively
 * making changes
 */
int iwl_check_rxon_cmd(struct iwl_priv *priv)
{
	int error = 0;
	int counter = 1;
	struct iwl_rxon_cmd *rxon = &priv->staging_rxon;

	if (rxon->flags & RXON_FLG_BAND_24G_MSK) {
		error |= le32_to_cpu(rxon->flags &
				(RXON_FLG_TGJ_NARROW_BAND_MSK |
				 RXON_FLG_RADAR_DETECT_MSK));
		if (error)
			IWL_WARN(priv, "check 24G fields %d | %d\n",
				    counter++, error);
	} else {
		error |= (rxon->flags & RXON_FLG_SHORT_SLOT_MSK) ?
				0 : le32_to_cpu(RXON_FLG_SHORT_SLOT_MSK);
		if (error)
			IWL_WARN(priv, "check 52 fields %d | %d\n",
				    counter++, error);
		error |= le32_to_cpu(rxon->flags & RXON_FLG_CCK_MSK);
		if (error)
			IWL_WARN(priv, "check 52 CCK %d | %d\n",
				    counter++, error);
	}
	error |= (rxon->node_addr[0] | rxon->bssid_addr[0]) & 0x1;
	if (error)
		IWL_WARN(priv, "check mac addr %d | %d\n", counter++, error);

	/* make sure basic rates 6Mbps and 1Mbps are supported */
	error |= (((rxon->ofdm_basic_rates & IWL_RATE_6M_MASK) == 0) &&
		  ((rxon->cck_basic_rates & IWL_RATE_1M_MASK) == 0));
	if (error)
		IWL_WARN(priv, "check basic rate %d | %d\n", counter++, error);

	error |= (le16_to_cpu(rxon->assoc_id) > 2007);
	if (error)
		IWL_WARN(priv, "check assoc id %d | %d\n", counter++, error);

	error |= ((rxon->flags & (RXON_FLG_CCK_MSK | RXON_FLG_SHORT_SLOT_MSK))
			== (RXON_FLG_CCK_MSK | RXON_FLG_SHORT_SLOT_MSK));
	if (error)
		IWL_WARN(priv, "check CCK and short slot %d | %d\n",
			    counter++, error);

	error |= ((rxon->flags & (RXON_FLG_CCK_MSK | RXON_FLG_AUTO_DETECT_MSK))
			== (RXON_FLG_CCK_MSK | RXON_FLG_AUTO_DETECT_MSK));
	if (error)
		IWL_WARN(priv, "check CCK & auto detect %d | %d\n",
			    counter++, error);

	error |= ((rxon->flags & (RXON_FLG_AUTO_DETECT_MSK |
			RXON_FLG_TGG_PROTECT_MSK)) == RXON_FLG_TGG_PROTECT_MSK);
	if (error)
		IWL_WARN(priv, "check TGG and auto detect %d | %d\n",
			    counter++, error);

	if (error)
		IWL_WARN(priv, "Tuning to channel %d\n",
			    le16_to_cpu(rxon->channel));

	if (error) {
		IWL_ERR(priv, "Not a valid iwl_rxon_assoc_cmd field values\n");
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(iwl_check_rxon_cmd);

/**
 * iwl_full_rxon_required - check if full RXON (vs RXON_ASSOC) cmd is needed
 * @priv: staging_rxon is compared to active_rxon
 *
 * If the RXON structure is changing enough to require a new tune,
 * or is clearing the RXON_FILTER_ASSOC_MSK, then return 1 to indicate that
 * a new tune (full RXON command, rather than RXON_ASSOC cmd) is required.
 */
int iwl_full_rxon_required(struct iwl_priv *priv)
{

	/* These items are only settable from the full RXON command */
	if (!(iwl_is_associated(priv)) ||
	    compare_ether_addr(priv->staging_rxon.bssid_addr,
			       priv->active_rxon.bssid_addr) ||
	    compare_ether_addr(priv->staging_rxon.node_addr,
			       priv->active_rxon.node_addr) ||
	    compare_ether_addr(priv->staging_rxon.wlap_bssid_addr,
			       priv->active_rxon.wlap_bssid_addr) ||
	    (priv->staging_rxon.dev_type != priv->active_rxon.dev_type) ||
	    (priv->staging_rxon.channel != priv->active_rxon.channel) ||
	    (priv->staging_rxon.air_propagation !=
	     priv->active_rxon.air_propagation) ||
	    (priv->staging_rxon.ofdm_ht_single_stream_basic_rates !=
	     priv->active_rxon.ofdm_ht_single_stream_basic_rates) ||
	    (priv->staging_rxon.ofdm_ht_dual_stream_basic_rates !=
	     priv->active_rxon.ofdm_ht_dual_stream_basic_rates) ||
	    (priv->staging_rxon.ofdm_ht_triple_stream_basic_rates !=
	     priv->active_rxon.ofdm_ht_triple_stream_basic_rates) ||
	    (priv->staging_rxon.assoc_id != priv->active_rxon.assoc_id))
		return 1;

	/* flags, filter_flags, ofdm_basic_rates, and cck_basic_rates can
	 * be updated with the RXON_ASSOC command -- however only some
	 * flag transitions are allowed using RXON_ASSOC */

	/* Check if we are not switching bands */
	if ((priv->staging_rxon.flags & RXON_FLG_BAND_24G_MSK) !=
	    (priv->active_rxon.flags & RXON_FLG_BAND_24G_MSK))
		return 1;

	/* Check if we are switching association toggle */
	if ((priv->staging_rxon.filter_flags & RXON_FILTER_ASSOC_MSK) !=
		(priv->active_rxon.filter_flags & RXON_FILTER_ASSOC_MSK))
		return 1;

	return 0;
}
EXPORT_SYMBOL(iwl_full_rxon_required);

u8 iwl_rate_get_lowest_plcp(struct iwl_priv *priv)
{
	/*
	 * Assign the lowest rate -- should really get this from
	 * the beacon skb from mac80211.
	 */
	if (priv->staging_rxon.flags & RXON_FLG_BAND_24G_MSK)
		return IWL_RATE_1M_PLCP;
	else
		return IWL_RATE_6M_PLCP;
}
EXPORT_SYMBOL(iwl_rate_get_lowest_plcp);

void iwl_set_rxon_ht(struct iwl_priv *priv, struct iwl_ht_config *ht_conf)
{
	struct iwl_rxon_cmd *rxon = &priv->staging_rxon;

	if (!ht_conf->is_ht) {
		rxon->flags &= ~(RXON_FLG_CHANNEL_MODE_MSK |
			RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK |
			RXON_FLG_HT40_PROT_MSK |
			RXON_FLG_HT_PROT_MSK);
		return;
	}

	rxon->flags |= cpu_to_le32(ht_conf->ht_protection << RXON_FLG_HT_OPERATING_MODE_POS);

	/* Set up channel bandwidth:
	 * 20 MHz only, 20/40 mixed or pure 40 if ht40 ok */
	/* clear the HT channel mode before set the mode */
	rxon->flags &= ~(RXON_FLG_CHANNEL_MODE_MSK |
			 RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK);
	if (iwl_is_ht40_tx_allowed(priv, NULL)) {
		/* pure ht40 */
		if (ht_conf->ht_protection == IEEE80211_HT_OP_MODE_PROTECTION_20MHZ) {
			rxon->flags |= RXON_FLG_CHANNEL_MODE_PURE_40;
			/* Note: control channel is opposite of extension channel */
			switch (ht_conf->extension_chan_offset) {
			case IEEE80211_HT_PARAM_CHA_SEC_ABOVE:
				rxon->flags &= ~RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK;
				break;
			case IEEE80211_HT_PARAM_CHA_SEC_BELOW:
				rxon->flags |= RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK;
				break;
			}
		} else {
			/* Note: control channel is opposite of extension channel */
			switch (ht_conf->extension_chan_offset) {
			case IEEE80211_HT_PARAM_CHA_SEC_ABOVE:
				rxon->flags &= ~(RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK);
				rxon->flags |= RXON_FLG_CHANNEL_MODE_MIXED;
				break;
			case IEEE80211_HT_PARAM_CHA_SEC_BELOW:
				rxon->flags |= RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK;
				rxon->flags |= RXON_FLG_CHANNEL_MODE_MIXED;
				break;
			case IEEE80211_HT_PARAM_CHA_SEC_NONE:
			default:
				/* channel location only valid if in Mixed mode */
				IWL_ERR(priv, "invalid extension channel offset\n");
				break;
			}
		}
	} else {
		rxon->flags |= RXON_FLG_CHANNEL_MODE_LEGACY;
	}

	if (priv->cfg->ops->hcmd->set_rxon_chain)
		priv->cfg->ops->hcmd->set_rxon_chain(priv);

	IWL_DEBUG_ASSOC(priv, "rxon flags 0x%X operation mode :0x%X "
			"extension channel offset 0x%x\n",
			le32_to_cpu(rxon->flags), ht_conf->ht_protection,
			ht_conf->extension_chan_offset);
}
EXPORT_SYMBOL(iwl_set_rxon_ht);

#define IWL_NUM_RX_CHAINS_MULTIPLE	3
#define IWL_NUM_RX_CHAINS_SINGLE	2
#define IWL_NUM_IDLE_CHAINS_DUAL	2
#define IWL_NUM_IDLE_CHAINS_SINGLE	1

/*
 * Determine how many receiver/antenna chains to use.
 *
 * More provides better reception via diversity.  Fewer saves power
 * at the expense of throughput, but only when not in powersave to
 * start with.
 *
 * MIMO (dual stream) requires at least 2, but works better with 3.
 * This does not determine *which* chains to use, just how many.
 */
static int iwl_get_active_rx_chain_count(struct iwl_priv *priv)
{
	/* # of Rx chains to use when expecting MIMO. */
	if (is_single_rx_stream(priv))
		return IWL_NUM_RX_CHAINS_SINGLE;
	else
		return IWL_NUM_RX_CHAINS_MULTIPLE;
}

/*
 * When we are in power saving mode, unless device support spatial
 * multiplexing power save, use the active count for rx chain count.
 */
static int iwl_get_idle_rx_chain_count(struct iwl_priv *priv, int active_cnt)
{
	/* # Rx chains when idling, depending on SMPS mode */
	switch (priv->current_ht_config.smps) {
	case IEEE80211_SMPS_STATIC:
	case IEEE80211_SMPS_DYNAMIC:
		return IWL_NUM_IDLE_CHAINS_SINGLE;
	case IEEE80211_SMPS_OFF:
		return active_cnt;
	default:
		WARN(1, "invalid SMPS mode %d",
		     priv->current_ht_config.smps);
		return active_cnt;
	}
}

/* up to 4 chains */
static u8 iwl_count_chain_bitmap(u32 chain_bitmap)
{
	u8 res;
	res = (chain_bitmap & BIT(0)) >> 0;
	res += (chain_bitmap & BIT(1)) >> 1;
	res += (chain_bitmap & BIT(2)) >> 2;
	res += (chain_bitmap & BIT(3)) >> 3;
	return res;
}

/**
 * iwl_set_rxon_chain - Set up Rx chain usage in "staging" RXON image
 *
 * Selects how many and which Rx receivers/antennas/chains to use.
 * This should not be used for scan command ... it puts data in wrong place.
 */
void iwl_set_rxon_chain(struct iwl_priv *priv)
{
	bool is_single = is_single_rx_stream(priv);
	bool is_cam = !test_bit(STATUS_POWER_PMI, &priv->status);
	u8 idle_rx_cnt, active_rx_cnt, valid_rx_cnt;
	u32 active_chains;
	u16 rx_chain;

	/* Tell uCode which antennas are actually connected.
	 * Before first association, we assume all antennas are connected.
	 * Just after first association, iwl_chain_noise_calibration()
	 *    checks which antennas actually *are* connected. */
	 if (priv->chain_noise_data.active_chains)
		active_chains = priv->chain_noise_data.active_chains;
	else
		active_chains = priv->hw_params.valid_rx_ant;

	rx_chain = active_chains << RXON_RX_CHAIN_VALID_POS;

	/* How many receivers should we use? */
	active_rx_cnt = iwl_get_active_rx_chain_count(priv);
	idle_rx_cnt = iwl_get_idle_rx_chain_count(priv, active_rx_cnt);


	/* correct rx chain count according hw settings
	 * and chain noise calibration
	 */
	valid_rx_cnt = iwl_count_chain_bitmap(active_chains);
	if (valid_rx_cnt < active_rx_cnt)
		active_rx_cnt = valid_rx_cnt;

	if (valid_rx_cnt < idle_rx_cnt)
		idle_rx_cnt = valid_rx_cnt;

	rx_chain |= active_rx_cnt << RXON_RX_CHAIN_MIMO_CNT_POS;
	rx_chain |= idle_rx_cnt  << RXON_RX_CHAIN_CNT_POS;

	priv->staging_rxon.rx_chain = cpu_to_le16(rx_chain);

	if (!is_single && (active_rx_cnt >= IWL_NUM_RX_CHAINS_SINGLE) && is_cam)
		priv->staging_rxon.rx_chain |= RXON_RX_CHAIN_MIMO_FORCE_MSK;
	else
		priv->staging_rxon.rx_chain &= ~RXON_RX_CHAIN_MIMO_FORCE_MSK;

	IWL_DEBUG_ASSOC(priv, "rx_chain=0x%X active=%d idle=%d\n",
			priv->staging_rxon.rx_chain,
			active_rx_cnt, idle_rx_cnt);

	WARN_ON(active_rx_cnt == 0 || idle_rx_cnt == 0 ||
		active_rx_cnt < idle_rx_cnt);
}
EXPORT_SYMBOL(iwl_set_rxon_chain);

/* Return valid channel */
u8 iwl_get_single_channel_number(struct iwl_priv *priv,
				  enum ieee80211_band band)
{
	const struct iwl_channel_info *ch_info;
	int i;
	u8 channel = 0;

	/* only scan single channel, good enough to reset the RF */
	/* pick the first valid not in-use channel */
	if (band == IEEE80211_BAND_5GHZ) {
		for (i = 14; i < priv->channel_count; i++) {
			if (priv->channel_info[i].channel !=
			    le16_to_cpu(priv->staging_rxon.channel)) {
				channel = priv->channel_info[i].channel;
				ch_info = iwl_get_channel_info(priv,
					band, channel);
				if (is_channel_valid(ch_info))
					break;
			}
		}
	} else {
		for (i = 0; i < 14; i++) {
			if (priv->channel_info[i].channel !=
			    le16_to_cpu(priv->staging_rxon.channel)) {
					channel =
						priv->channel_info[i].channel;
					ch_info = iwl_get_channel_info(priv,
						band, channel);
					if (is_channel_valid(ch_info))
						break;
			}
		}
	}

	return channel;
}
EXPORT_SYMBOL(iwl_get_single_channel_number);

/**
 * iwl_set_rxon_channel - Set the phymode and channel values in staging RXON
 * @phymode: MODE_IEEE80211A sets to 5.2GHz; all else set to 2.4GHz
 * @channel: Any channel valid for the requested phymode

 * In addition to setting the staging RXON, priv->phymode is also set.
 *
 * NOTE:  Does not commit to the hardware; it sets appropriate bit fields
 * in the staging RXON flag structure based on the phymode
 */
int iwl_set_rxon_channel(struct iwl_priv *priv, struct ieee80211_channel *ch)
{
	enum ieee80211_band band = ch->band;
	u16 channel = ieee80211_frequency_to_channel(ch->center_freq);

	if (!iwl_get_channel_info(priv, band, channel)) {
		IWL_DEBUG_INFO(priv, "Could not set channel to %d [%d]\n",
			       channel, band);
		return -EINVAL;
	}

	if ((le16_to_cpu(priv->staging_rxon.channel) == channel) &&
	    (priv->band == band))
		return 0;

	priv->staging_rxon.channel = cpu_to_le16(channel);
	if (band == IEEE80211_BAND_5GHZ)
		priv->staging_rxon.flags &= ~RXON_FLG_BAND_24G_MSK;
	else
		priv->staging_rxon.flags |= RXON_FLG_BAND_24G_MSK;

	priv->band = band;

	IWL_DEBUG_INFO(priv, "Staging channel set to %d [%d]\n", channel, band);

	return 0;
}
EXPORT_SYMBOL(iwl_set_rxon_channel);

void iwl_set_flags_for_band(struct iwl_priv *priv,
			    enum ieee80211_band band,
			    struct ieee80211_vif *vif)
{
	if (band == IEEE80211_BAND_5GHZ) {
		priv->staging_rxon.flags &=
		    ~(RXON_FLG_BAND_24G_MSK | RXON_FLG_AUTO_DETECT_MSK
		      | RXON_FLG_CCK_MSK);
		priv->staging_rxon.flags |= RXON_FLG_SHORT_SLOT_MSK;
	} else {
		/* Copied from iwl_post_associate() */
		if (vif && vif->bss_conf.use_short_slot)
			priv->staging_rxon.flags |= RXON_FLG_SHORT_SLOT_MSK;
		else
			priv->staging_rxon.flags &= ~RXON_FLG_SHORT_SLOT_MSK;

		priv->staging_rxon.flags |= RXON_FLG_BAND_24G_MSK;
		priv->staging_rxon.flags |= RXON_FLG_AUTO_DETECT_MSK;
		priv->staging_rxon.flags &= ~RXON_FLG_CCK_MSK;
	}
}
EXPORT_SYMBOL(iwl_set_flags_for_band);

/*
 * initialize rxon structure with default values from eeprom
 */
void iwl_connection_init_rx_config(struct iwl_priv *priv,
				   struct ieee80211_vif *vif)
{
	const struct iwl_channel_info *ch_info;
	enum nl80211_iftype type = NL80211_IFTYPE_STATION;

	if (vif)
		type = vif->type;

	memset(&priv->staging_rxon, 0, sizeof(priv->staging_rxon));

	switch (type) {
	case NL80211_IFTYPE_AP:
		priv->staging_rxon.dev_type = RXON_DEV_TYPE_AP;
		break;

	case NL80211_IFTYPE_STATION:
		priv->staging_rxon.dev_type = RXON_DEV_TYPE_ESS;
		priv->staging_rxon.filter_flags = RXON_FILTER_ACCEPT_GRP_MSK;
		break;

	case NL80211_IFTYPE_ADHOC:
		priv->staging_rxon.dev_type = RXON_DEV_TYPE_IBSS;
		priv->staging_rxon.flags = RXON_FLG_SHORT_PREAMBLE_MSK;
		priv->staging_rxon.filter_flags = RXON_FILTER_BCON_AWARE_MSK |
						  RXON_FILTER_ACCEPT_GRP_MSK;
		break;

	default:
		IWL_ERR(priv, "Unsupported interface type %d\n", type);
		break;
	}


	ch_info = iwl_get_channel_info(priv, priv->band,
				       le16_to_cpu(priv->active_rxon.channel));

	if (!ch_info)
		ch_info = &priv->channel_info[0];

	priv->staging_rxon.channel = cpu_to_le16(ch_info->channel);
	priv->band = ch_info->band;

	iwl_set_flags_for_band(priv, priv->band, vif);

	priv->staging_rxon.ofdm_basic_rates =
	    (IWL_OFDM_RATES_MASK >> IWL_FIRST_OFDM_RATE) & 0xFF;
	priv->staging_rxon.cck_basic_rates =
	    (IWL_CCK_RATES_MASK >> IWL_FIRST_CCK_RATE) & 0xF;

	/* clear both MIX and PURE40 mode flag */
	priv->staging_rxon.flags &= ~(RXON_FLG_CHANNEL_MODE_MIXED |
					RXON_FLG_CHANNEL_MODE_PURE_40);

	if (vif)
		memcpy(priv->staging_rxon.node_addr, vif->addr, ETH_ALEN);

	priv->staging_rxon.ofdm_ht_single_stream_basic_rates = 0xff;
	priv->staging_rxon.ofdm_ht_dual_stream_basic_rates = 0xff;
	priv->staging_rxon.ofdm_ht_triple_stream_basic_rates = 0xff;
}
EXPORT_SYMBOL(iwl_connection_init_rx_config);

void iwl_set_rate(struct iwl_priv *priv)
{
	const struct ieee80211_supported_band *hw = NULL;
	struct ieee80211_rate *rate;
	int i;

	hw = iwl_get_hw_mode(priv, priv->band);
	if (!hw) {
		IWL_ERR(priv, "Failed to set rate: unable to get hw mode\n");
		return;
	}

	priv->active_rate = 0;

	for (i = 0; i < hw->n_bitrates; i++) {
		rate = &(hw->bitrates[i]);
		if (rate->hw_value < IWL_RATE_COUNT_LEGACY)
			priv->active_rate |= (1 << rate->hw_value);
	}

	IWL_DEBUG_RATE(priv, "Set active_rate = %0x\n", priv->active_rate);

	priv->staging_rxon.cck_basic_rates =
	    (IWL_CCK_BASIC_RATES_MASK >> IWL_FIRST_CCK_RATE) & 0xF;

	priv->staging_rxon.ofdm_basic_rates =
	   (IWL_OFDM_BASIC_RATES_MASK >> IWL_FIRST_OFDM_RATE) & 0xFF;
}
EXPORT_SYMBOL(iwl_set_rate);

void iwl_chswitch_done(struct iwl_priv *priv, bool is_success)
{
	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	if (priv->switch_rxon.switch_in_progress) {
		ieee80211_chswitch_done(priv->vif, is_success);
		mutex_lock(&priv->mutex);
		priv->switch_rxon.switch_in_progress = false;
		mutex_unlock(&priv->mutex);
	}
}
EXPORT_SYMBOL(iwl_chswitch_done);

void iwl_rx_csa(struct iwl_priv *priv, struct iwl_rx_mem_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	struct iwl_rxon_cmd *rxon = (void *)&priv->active_rxon;
	struct iwl_csa_notification *csa = &(pkt->u.csa_notif);

	if (priv->switch_rxon.switch_in_progress) {
		if (!le32_to_cpu(csa->status) &&
		    (csa->channel == priv->switch_rxon.channel)) {
			rxon->channel = csa->channel;
			priv->staging_rxon.channel = csa->channel;
			IWL_DEBUG_11H(priv, "CSA notif: channel %d\n",
			      le16_to_cpu(csa->channel));
			iwl_chswitch_done(priv, true);
		} else {
			IWL_ERR(priv, "CSA notif (fail) : channel %d\n",
			      le16_to_cpu(csa->channel));
			iwl_chswitch_done(priv, false);
		}
	}
}
EXPORT_SYMBOL(iwl_rx_csa);

#ifdef CONFIG_IWLWIFI_DEBUG
void iwl_print_rx_config_cmd(struct iwl_priv *priv)
{
	struct iwl_rxon_cmd *rxon = &priv->staging_rxon;

	IWL_DEBUG_RADIO(priv, "RX CONFIG:\n");
	iwl_print_hex_dump(priv, IWL_DL_RADIO, (u8 *) rxon, sizeof(*rxon));
	IWL_DEBUG_RADIO(priv, "u16 channel: 0x%x\n", le16_to_cpu(rxon->channel));
	IWL_DEBUG_RADIO(priv, "u32 flags: 0x%08X\n", le32_to_cpu(rxon->flags));
	IWL_DEBUG_RADIO(priv, "u32 filter_flags: 0x%08x\n",
			le32_to_cpu(rxon->filter_flags));
	IWL_DEBUG_RADIO(priv, "u8 dev_type: 0x%x\n", rxon->dev_type);
	IWL_DEBUG_RADIO(priv, "u8 ofdm_basic_rates: 0x%02x\n",
			rxon->ofdm_basic_rates);
	IWL_DEBUG_RADIO(priv, "u8 cck_basic_rates: 0x%02x\n", rxon->cck_basic_rates);
	IWL_DEBUG_RADIO(priv, "u8[6] node_addr: %pM\n", rxon->node_addr);
	IWL_DEBUG_RADIO(priv, "u8[6] bssid_addr: %pM\n", rxon->bssid_addr);
	IWL_DEBUG_RADIO(priv, "u16 assoc_id: 0x%x\n", le16_to_cpu(rxon->assoc_id));
}
EXPORT_SYMBOL(iwl_print_rx_config_cmd);
#endif
/**
 * iwl_irq_handle_error - called for HW or SW error interrupt from card
 */
void iwl_irq_handle_error(struct iwl_priv *priv)
{
	/* Set the FW error flag -- cleared on iwl_down */
	set_bit(STATUS_FW_ERROR, &priv->status);

	/* Cancel currently queued command. */
	clear_bit(STATUS_HCMD_ACTIVE, &priv->status);

	IWL_ERR(priv, "Loaded firmware version: %s\n",
		priv->hw->wiphy->fw_version);

	priv->cfg->ops->lib->dump_nic_error_log(priv);
	if (priv->cfg->ops->lib->dump_csr)
		priv->cfg->ops->lib->dump_csr(priv);
	if (priv->cfg->ops->lib->dump_fh)
		priv->cfg->ops->lib->dump_fh(priv, NULL, false);
	priv->cfg->ops->lib->dump_nic_event_log(priv, false, NULL, false);
#ifdef CONFIG_IWLWIFI_DEBUG
	if (iwl_get_debug_level(priv) & IWL_DL_FW_ERRORS)
		iwl_print_rx_config_cmd(priv);
#endif

	wake_up_interruptible(&priv->wait_command_queue);

	/* Keep the restart process from trying to send host
	 * commands by clearing the INIT status bit */
	clear_bit(STATUS_READY, &priv->status);

	if (!test_bit(STATUS_EXIT_PENDING, &priv->status)) {
		IWL_DEBUG(priv, IWL_DL_FW_ERRORS,
			  "Restarting adapter due to uCode error.\n");

		if (priv->cfg->mod_params->restart_fw)
			queue_work(priv->workqueue, &priv->restart);
	}
}
EXPORT_SYMBOL(iwl_irq_handle_error);

static int iwl_apm_stop_master(struct iwl_priv *priv)
{
	int ret = 0;

	/* stop device's busmaster DMA activity */
	iwl_set_bit(priv, CSR_RESET, CSR_RESET_REG_FLAG_STOP_MASTER);

	ret = iwl_poll_bit(priv, CSR_RESET, CSR_RESET_REG_FLAG_MASTER_DISABLED,
			CSR_RESET_REG_FLAG_MASTER_DISABLED, 100);
	if (ret)
		IWL_WARN(priv, "Master Disable Timed Out, 100 usec\n");

	IWL_DEBUG_INFO(priv, "stop master\n");

	return ret;
}

void iwl_apm_stop(struct iwl_priv *priv)
{
	IWL_DEBUG_INFO(priv, "Stop card, put in low power state\n");

	/* Stop device's DMA activity */
	iwl_apm_stop_master(priv);

	/* Reset the entire device */
	iwl_set_bit(priv, CSR_RESET, CSR_RESET_REG_FLAG_SW_RESET);

	udelay(10);

	/*
	 * Clear "initialization complete" bit to move adapter from
	 * D0A* (powered-up Active) --> D0U* (Uninitialized) state.
	 */
	iwl_clear_bit(priv, CSR_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_INIT_DONE);
}
EXPORT_SYMBOL(iwl_apm_stop);


/*
 * Start up NIC's basic functionality after it has been reset
 * (e.g. after platform boot, or shutdown via iwl_apm_stop())
 * NOTE:  This does not load uCode nor start the embedded processor
 */
int iwl_apm_init(struct iwl_priv *priv)
{
	int ret = 0;
	u16 lctl;

	IWL_DEBUG_INFO(priv, "Init card's basic functions\n");

	/*
	 * Use "set_bit" below rather than "write", to preserve any hardware
	 * bits already set by default after reset.
	 */

	/* Disable L0S exit timer (platform NMI Work/Around) */
	iwl_set_bit(priv, CSR_GIO_CHICKEN_BITS,
			  CSR_GIO_CHICKEN_BITS_REG_BIT_DIS_L0S_EXIT_TIMER);

	/*
	 * Disable L0s without affecting L1;
	 *  don't wait for ICH L0s (ICH bug W/A)
	 */
	iwl_set_bit(priv, CSR_GIO_CHICKEN_BITS,
			  CSR_GIO_CHICKEN_BITS_REG_BIT_L1A_NO_L0S_RX);

	/* Set FH wait threshold to maximum (HW error during stress W/A) */
	iwl_set_bit(priv, CSR_DBG_HPET_MEM_REG, CSR_DBG_HPET_MEM_REG_VAL);

	/*
	 * Enable HAP INTA (interrupt from management bus) to
	 * wake device's PCI Express link L1a -> L0s
	 * NOTE:  This is no-op for 3945 (non-existant bit)
	 */
	iwl_set_bit(priv, CSR_HW_IF_CONFIG_REG,
				    CSR_HW_IF_CONFIG_REG_BIT_HAP_WAKE_L1A);

	/*
	 * HW bug W/A for instability in PCIe bus L0->L0S->L1 transition.
	 * Check if BIOS (or OS) enabled L1-ASPM on this device.
	 * If so (likely), disable L0S, so device moves directly L0->L1;
	 *    costs negligible amount of power savings.
	 * If not (unlikely), enable L0S, so there is at least some
	 *    power savings, even without L1.
	 */
	if (priv->cfg->set_l0s) {
		lctl = iwl_pcie_link_ctl(priv);
		if ((lctl & PCI_CFG_LINK_CTRL_VAL_L1_EN) ==
					PCI_CFG_LINK_CTRL_VAL_L1_EN) {
			/* L1-ASPM enabled; disable(!) L0S  */
			iwl_set_bit(priv, CSR_GIO_REG,
					CSR_GIO_REG_VAL_L0S_ENABLED);
			IWL_DEBUG_POWER(priv, "L1 Enabled; Disabling L0S\n");
		} else {
			/* L1-ASPM disabled; enable(!) L0S */
			iwl_clear_bit(priv, CSR_GIO_REG,
					CSR_GIO_REG_VAL_L0S_ENABLED);
			IWL_DEBUG_POWER(priv, "L1 Disabled; Enabling L0S\n");
		}
	}

	/* Configure analog phase-lock-loop before activating to D0A */
	if (priv->cfg->pll_cfg_val)
		iwl_set_bit(priv, CSR_ANA_PLL_CFG, priv->cfg->pll_cfg_val);

	/*
	 * Set "initialization complete" bit to move adapter from
	 * D0U* --> D0A* (powered-up active) state.
	 */
	iwl_set_bit(priv, CSR_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_INIT_DONE);

	/*
	 * Wait for clock stabilization; once stabilized, access to
	 * device-internal resources is supported, e.g. iwl_write_prph()
	 * and accesses to uCode SRAM.
	 */
	ret = iwl_poll_bit(priv, CSR_GP_CNTRL,
			CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
			CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY, 25000);
	if (ret < 0) {
		IWL_DEBUG_INFO(priv, "Failed to init the card\n");
		goto out;
	}

	/*
	 * Enable DMA and BSM (if used) clocks, wait for them to stabilize.
	 * BSM (Boostrap State Machine) is only in 3945 and 4965;
	 * later devices (i.e. 5000 and later) have non-volatile SRAM,
	 * and don't need BSM to restore data after power-saving sleep.
	 *
	 * Write to "CLK_EN_REG"; "1" bits enable clocks, while "0" bits
	 * do not disable clocks.  This preserves any hardware bits already
	 * set by default in "CLK_CTRL_REG" after reset.
	 */
	if (priv->cfg->use_bsm)
		iwl_write_prph(priv, APMG_CLK_EN_REG,
			APMG_CLK_VAL_DMA_CLK_RQT | APMG_CLK_VAL_BSM_CLK_RQT);
	else
		iwl_write_prph(priv, APMG_CLK_EN_REG,
			APMG_CLK_VAL_DMA_CLK_RQT);
	udelay(20);

	/* Disable L1-Active */
	iwl_set_bits_prph(priv, APMG_PCIDEV_STT_REG,
			  APMG_PCIDEV_STT_VAL_L1_ACT_DIS);

out:
	return ret;
}
EXPORT_SYMBOL(iwl_apm_init);


int iwl_set_hw_params(struct iwl_priv *priv)
{
	priv->hw_params.max_rxq_size = RX_QUEUE_SIZE;
	priv->hw_params.max_rxq_log = RX_QUEUE_SIZE_LOG;
	if (priv->cfg->mod_params->amsdu_size_8K)
		priv->hw_params.rx_page_order = get_order(IWL_RX_BUF_SIZE_8K);
	else
		priv->hw_params.rx_page_order = get_order(IWL_RX_BUF_SIZE_4K);

	priv->hw_params.max_beacon_itrvl = IWL_MAX_UCODE_BEACON_INTERVAL;

	if (priv->cfg->mod_params->disable_11n)
		priv->cfg->sku &= ~IWL_SKU_N;

	/* Device-specific setup */
	return priv->cfg->ops->lib->set_hw_params(priv);
}
EXPORT_SYMBOL(iwl_set_hw_params);

int iwl_set_tx_power(struct iwl_priv *priv, s8 tx_power, bool force)
{
	int ret = 0;
	s8 prev_tx_power = priv->tx_power_user_lmt;

	if (tx_power < IWLAGN_TX_POWER_TARGET_POWER_MIN) {
		IWL_WARN(priv,
			 "Requested user TXPOWER %d below lower limit %d.\n",
			 tx_power,
			 IWLAGN_TX_POWER_TARGET_POWER_MIN);
		return -EINVAL;
	}

	if (tx_power > priv->tx_power_device_lmt) {
		IWL_WARN(priv,
			"Requested user TXPOWER %d above upper limit %d.\n",
			 tx_power, priv->tx_power_device_lmt);
		return -EINVAL;
	}

	if (priv->tx_power_user_lmt != tx_power)
		force = true;

	/* if nic is not up don't send command */
	if (iwl_is_ready_rf(priv)) {
		priv->tx_power_user_lmt = tx_power;
		if (force && priv->cfg->ops->lib->send_tx_power)
			ret = priv->cfg->ops->lib->send_tx_power(priv);
		else if (!priv->cfg->ops->lib->send_tx_power)
			ret = -EOPNOTSUPP;
		/*
		 * if fail to set tx_power, restore the orig. tx power
		 */
		if (ret)
			priv->tx_power_user_lmt = prev_tx_power;
	}

	/*
	 * Even this is an async host command, the command
	 * will always report success from uCode
	 * So once driver can placing the command into the queue
	 * successfully, driver can use priv->tx_power_user_lmt
	 * to reflect the current tx power
	 */
	return ret;
}
EXPORT_SYMBOL(iwl_set_tx_power);

irqreturn_t iwl_isr_legacy(int irq, void *data)
{
	struct iwl_priv *priv = data;
	u32 inta, inta_mask;
	u32 inta_fh;
	unsigned long flags;
	if (!priv)
		return IRQ_NONE;

	spin_lock_irqsave(&priv->lock, flags);

	/* Disable (but don't clear!) interrupts here to avoid
	 *    back-to-back ISRs and sporadic interrupts from our NIC.
	 * If we have something to service, the tasklet will re-enable ints.
	 * If we *don't* have something, we'll re-enable before leaving here. */
	inta_mask = iwl_read32(priv, CSR_INT_MASK);  /* just for debug */
	iwl_write32(priv, CSR_INT_MASK, 0x00000000);

	/* Discover which interrupts are active/pending */
	inta = iwl_read32(priv, CSR_INT);
	inta_fh = iwl_read32(priv, CSR_FH_INT_STATUS);

	/* Ignore interrupt if there's nothing in NIC to service.
	 * This may be due to IRQ shared with another device,
	 * or due to sporadic interrupts thrown from our NIC. */
	if (!inta && !inta_fh) {
		IWL_DEBUG_ISR(priv, "Ignore interrupt, inta == 0, inta_fh == 0\n");
		goto none;
	}

	if ((inta == 0xFFFFFFFF) || ((inta & 0xFFFFFFF0) == 0xa5a5a5a0)) {
		/* Hardware disappeared. It might have already raised
		 * an interrupt */
		IWL_WARN(priv, "HARDWARE GONE?? INTA == 0x%08x\n", inta);
		goto unplugged;
	}

	IWL_DEBUG_ISR(priv, "ISR inta 0x%08x, enabled 0x%08x, fh 0x%08x\n",
		      inta, inta_mask, inta_fh);

	inta &= ~CSR_INT_BIT_SCD;

	/* iwl_irq_tasklet() will service interrupts and re-enable them */
	if (likely(inta || inta_fh))
		tasklet_schedule(&priv->irq_tasklet);

 unplugged:
	spin_unlock_irqrestore(&priv->lock, flags);
	return IRQ_HANDLED;

 none:
	/* re-enable interrupts here since we don't have anything to service. */
	/* only Re-enable if diabled by irq */
	if (test_bit(STATUS_INT_ENABLED, &priv->status))
		iwl_enable_interrupts(priv);
	spin_unlock_irqrestore(&priv->lock, flags);
	return IRQ_NONE;
}
EXPORT_SYMBOL(iwl_isr_legacy);

void iwl_send_bt_config(struct iwl_priv *priv)
{
	struct iwl_bt_cmd bt_cmd = {
		.lead_time = BT_LEAD_TIME_DEF,
		.max_kill = BT_MAX_KILL_DEF,
		.kill_ack_mask = 0,
		.kill_cts_mask = 0,
	};

	if (!bt_coex_active)
		bt_cmd.flags = BT_COEX_DISABLE;
	else
		bt_cmd.flags = BT_COEX_ENABLE;

	IWL_DEBUG_INFO(priv, "BT coex %s\n",
		(bt_cmd.flags == BT_COEX_DISABLE) ? "disable" : "active");

	if (iwl_send_cmd_pdu(priv, REPLY_BT_CONFIG,
			     sizeof(struct iwl_bt_cmd), &bt_cmd))
		IWL_ERR(priv, "failed to send BT Coex Config\n");
}
EXPORT_SYMBOL(iwl_send_bt_config);

int iwl_send_statistics_request(struct iwl_priv *priv, u8 flags, bool clear)
{
	struct iwl_statistics_cmd statistics_cmd = {
		.configuration_flags =
			clear ? IWL_STATS_CONF_CLEAR_STATS : 0,
	};

	if (flags & CMD_ASYNC)
		return iwl_send_cmd_pdu_async(priv, REPLY_STATISTICS_CMD,
					       sizeof(struct iwl_statistics_cmd),
					       &statistics_cmd, NULL);
	else
		return iwl_send_cmd_pdu(priv, REPLY_STATISTICS_CMD,
					sizeof(struct iwl_statistics_cmd),
					&statistics_cmd);
}
EXPORT_SYMBOL(iwl_send_statistics_request);

void iwl_rf_kill_ct_config(struct iwl_priv *priv)
{
	struct iwl_ct_kill_config cmd;
	struct iwl_ct_kill_throttling_config adv_cmd;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&priv->lock, flags);
	iwl_write32(priv, CSR_UCODE_DRV_GP1_CLR,
		    CSR_UCODE_DRV_GP1_REG_BIT_CT_KILL_EXIT);
	spin_unlock_irqrestore(&priv->lock, flags);
	priv->thermal_throttle.ct_kill_toggle = false;

	if (priv->cfg->support_ct_kill_exit) {
		adv_cmd.critical_temperature_enter =
			cpu_to_le32(priv->hw_params.ct_kill_threshold);
		adv_cmd.critical_temperature_exit =
			cpu_to_le32(priv->hw_params.ct_kill_exit_threshold);

		ret = iwl_send_cmd_pdu(priv, REPLY_CT_KILL_CONFIG_CMD,
				       sizeof(adv_cmd), &adv_cmd);
		if (ret)
			IWL_ERR(priv, "REPLY_CT_KILL_CONFIG_CMD failed\n");
		else
			IWL_DEBUG_INFO(priv, "REPLY_CT_KILL_CONFIG_CMD "
					"succeeded, "
					"critical temperature enter is %d,"
					"exit is %d\n",
				       priv->hw_params.ct_kill_threshold,
				       priv->hw_params.ct_kill_exit_threshold);
	} else {
		cmd.critical_temperature_R =
			cpu_to_le32(priv->hw_params.ct_kill_threshold);

		ret = iwl_send_cmd_pdu(priv, REPLY_CT_KILL_CONFIG_CMD,
				       sizeof(cmd), &cmd);
		if (ret)
			IWL_ERR(priv, "REPLY_CT_KILL_CONFIG_CMD failed\n");
		else
			IWL_DEBUG_INFO(priv, "REPLY_CT_KILL_CONFIG_CMD "
					"succeeded, "
					"critical temperature is %d\n",
					priv->hw_params.ct_kill_threshold);
	}
}
EXPORT_SYMBOL(iwl_rf_kill_ct_config);


/*
 * CARD_STATE_CMD
 *
 * Use: Sets the device's internal card state to enable, disable, or halt
 *
 * When in the 'enable' state the card operates as normal.
 * When in the 'disable' state, the card enters into a low power mode.
 * When in the 'halt' state, the card is shut down and must be fully
 * restarted to come back on.
 */
int iwl_send_card_state(struct iwl_priv *priv, u32 flags, u8 meta_flag)
{
	struct iwl_host_cmd cmd = {
		.id = REPLY_CARD_STATE_CMD,
		.len = sizeof(u32),
		.data = &flags,
		.flags = meta_flag,
	};

	return iwl_send_cmd(priv, &cmd);
}

void iwl_rx_pm_sleep_notif(struct iwl_priv *priv,
			   struct iwl_rx_mem_buffer *rxb)
{
#ifdef CONFIG_IWLWIFI_DEBUG
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	struct iwl_sleep_notification *sleep = &(pkt->u.sleep_notif);
	IWL_DEBUG_RX(priv, "sleep mode: %d, src: %d\n",
		     sleep->pm_sleep_mode, sleep->pm_wakeup_src);
#endif
}
EXPORT_SYMBOL(iwl_rx_pm_sleep_notif);

void iwl_rx_pm_debug_statistics_notif(struct iwl_priv *priv,
				      struct iwl_rx_mem_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	u32 len = le32_to_cpu(pkt->len_n_flags) & FH_RSCSR_FRAME_SIZE_MSK;
	IWL_DEBUG_RADIO(priv, "Dumping %d bytes of unhandled "
			"notification for %s:\n", len,
			get_cmd_string(pkt->hdr.cmd));
	iwl_print_hex_dump(priv, IWL_DL_RADIO, pkt->u.raw, len);
}
EXPORT_SYMBOL(iwl_rx_pm_debug_statistics_notif);

void iwl_rx_reply_error(struct iwl_priv *priv,
			struct iwl_rx_mem_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);

	IWL_ERR(priv, "Error Reply type 0x%08X cmd %s (0x%02X) "
		"seq 0x%04X ser 0x%08X\n",
		le32_to_cpu(pkt->u.err_resp.error_type),
		get_cmd_string(pkt->u.err_resp.cmd_id),
		pkt->u.err_resp.cmd_id,
		le16_to_cpu(pkt->u.err_resp.bad_cmd_seq_num),
		le32_to_cpu(pkt->u.err_resp.error_info));
}
EXPORT_SYMBOL(iwl_rx_reply_error);

void iwl_clear_isr_stats(struct iwl_priv *priv)
{
	memset(&priv->isr_stats, 0, sizeof(priv->isr_stats));
}

int iwl_mac_conf_tx(struct ieee80211_hw *hw, u16 queue,
			   const struct ieee80211_tx_queue_params *params)
{
	struct iwl_priv *priv = hw->priv;
	unsigned long flags;
	int q;

	IWL_DEBUG_MAC80211(priv, "enter\n");

	if (!iwl_is_ready_rf(priv)) {
		IWL_DEBUG_MAC80211(priv, "leave - RF not ready\n");
		return -EIO;
	}

	if (queue >= AC_NUM) {
		IWL_DEBUG_MAC80211(priv, "leave - queue >= AC_NUM %d\n", queue);
		return 0;
	}

	q = AC_NUM - 1 - queue;

	spin_lock_irqsave(&priv->lock, flags);

	priv->qos_data.def_qos_parm.ac[q].cw_min = cpu_to_le16(params->cw_min);
	priv->qos_data.def_qos_parm.ac[q].cw_max = cpu_to_le16(params->cw_max);
	priv->qos_data.def_qos_parm.ac[q].aifsn = params->aifs;
	priv->qos_data.def_qos_parm.ac[q].edca_txop =
			cpu_to_le16((params->txop * 32));

	priv->qos_data.def_qos_parm.ac[q].reserved1 = 0;

	spin_unlock_irqrestore(&priv->lock, flags);

	IWL_DEBUG_MAC80211(priv, "leave\n");
	return 0;
}
EXPORT_SYMBOL(iwl_mac_conf_tx);

static void iwl_ht_conf(struct iwl_priv *priv,
			struct ieee80211_vif *vif)
{
	struct iwl_ht_config *ht_conf = &priv->current_ht_config;
	struct ieee80211_sta *sta;
	struct ieee80211_bss_conf *bss_conf = &vif->bss_conf;

	IWL_DEBUG_MAC80211(priv, "enter:\n");

	if (!ht_conf->is_ht)
		return;

	ht_conf->ht_protection =
		bss_conf->ht_operation_mode & IEEE80211_HT_OP_MODE_PROTECTION;
	ht_conf->non_GF_STA_present =
		!!(bss_conf->ht_operation_mode & IEEE80211_HT_OP_MODE_NON_GF_STA_PRSNT);

	ht_conf->single_chain_sufficient = false;

	switch (vif->type) {
	case NL80211_IFTYPE_STATION:
		rcu_read_lock();
		sta = ieee80211_find_sta(vif, bss_conf->bssid);
		if (sta) {
			struct ieee80211_sta_ht_cap *ht_cap = &sta->ht_cap;
			int maxstreams;

			maxstreams = (ht_cap->mcs.tx_params &
				      IEEE80211_HT_MCS_TX_MAX_STREAMS_MASK)
					>> IEEE80211_HT_MCS_TX_MAX_STREAMS_SHIFT;
			maxstreams += 1;

			if ((ht_cap->mcs.rx_mask[1] == 0) &&
			    (ht_cap->mcs.rx_mask[2] == 0))
				ht_conf->single_chain_sufficient = true;
			if (maxstreams <= 1)
				ht_conf->single_chain_sufficient = true;
		} else {
			/*
			 * If at all, this can only happen through a race
			 * when the AP disconnects us while we're still
			 * setting up the connection, in that case mac80211
			 * will soon tell us about that.
			 */
			ht_conf->single_chain_sufficient = true;
		}
		rcu_read_unlock();
		break;
	case NL80211_IFTYPE_ADHOC:
		ht_conf->single_chain_sufficient = true;
		break;
	default:
		break;
	}

	IWL_DEBUG_MAC80211(priv, "leave\n");
}

static inline void iwl_set_no_assoc(struct iwl_priv *priv)
{
	iwl_led_disassociate(priv);
	/*
	 * inform the ucode that there is no longer an
	 * association and that no more packets should be
	 * sent
	 */
	priv->staging_rxon.filter_flags &=
		~RXON_FILTER_ASSOC_MSK;
	priv->staging_rxon.assoc_id = 0;
	iwlcore_commit_rxon(priv);
}

static int iwl_mac_beacon_update(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	struct iwl_priv *priv = hw->priv;
	unsigned long flags;
	__le64 timestamp;

	IWL_DEBUG_MAC80211(priv, "enter\n");

	if (!iwl_is_ready_rf(priv)) {
		IWL_DEBUG_MAC80211(priv, "leave - RF not ready\n");
		return -EIO;
	}

	spin_lock_irqsave(&priv->lock, flags);

	if (priv->ibss_beacon)
		dev_kfree_skb(priv->ibss_beacon);

	priv->ibss_beacon = skb;

	timestamp = ((struct ieee80211_mgmt *)skb->data)->u.beacon.timestamp;
	priv->timestamp = le64_to_cpu(timestamp);

	IWL_DEBUG_MAC80211(priv, "leave\n");
	spin_unlock_irqrestore(&priv->lock, flags);

	priv->cfg->ops->lib->post_associate(priv, priv->vif);

	return 0;
}

void iwl_bss_info_changed(struct ieee80211_hw *hw,
			  struct ieee80211_vif *vif,
			  struct ieee80211_bss_conf *bss_conf,
			  u32 changes)
{
	struct iwl_priv *priv = hw->priv;
	int ret;

	IWL_DEBUG_MAC80211(priv, "changes = 0x%X\n", changes);

	if (!iwl_is_alive(priv))
		return;

	mutex_lock(&priv->mutex);

	if (changes & BSS_CHANGED_QOS) {
		unsigned long flags;

		spin_lock_irqsave(&priv->lock, flags);
		priv->qos_data.qos_active = bss_conf->qos;
		iwl_update_qos(priv);
		spin_unlock_irqrestore(&priv->lock, flags);
	}

	if (changes & BSS_CHANGED_BEACON && vif->type == NL80211_IFTYPE_AP) {
		dev_kfree_skb(priv->ibss_beacon);
		priv->ibss_beacon = ieee80211_beacon_get(hw, vif);
	}

	if (changes & BSS_CHANGED_BEACON_INT) {
		/* TODO: in AP mode, do something to make this take effect */
	}

	if (changes & BSS_CHANGED_BSSID) {
		IWL_DEBUG_MAC80211(priv, "BSSID %pM\n", bss_conf->bssid);

		/*
		 * If there is currently a HW scan going on in the
		 * background then we need to cancel it else the RXON
		 * below/in post_associate will fail.
		 */
		if (iwl_scan_cancel_timeout(priv, 100)) {
			IWL_WARN(priv, "Aborted scan still in progress after 100ms\n");
			IWL_DEBUG_MAC80211(priv, "leaving - scan abort failed.\n");
			mutex_unlock(&priv->mutex);
			return;
		}

		/* mac80211 only sets assoc when in STATION mode */
		if (vif->type == NL80211_IFTYPE_ADHOC || bss_conf->assoc) {
			memcpy(priv->staging_rxon.bssid_addr,
			       bss_conf->bssid, ETH_ALEN);

			/* currently needed in a few places */
			memcpy(priv->bssid, bss_conf->bssid, ETH_ALEN);
		} else {
			priv->staging_rxon.filter_flags &=
				~RXON_FILTER_ASSOC_MSK;
		}

	}

	/*
	 * This needs to be after setting the BSSID in case
	 * mac80211 decides to do both changes at once because
	 * it will invoke post_associate.
	 */
	if (vif->type == NL80211_IFTYPE_ADHOC &&
	    changes & BSS_CHANGED_BEACON) {
		struct sk_buff *beacon = ieee80211_beacon_get(hw, vif);

		if (beacon)
			iwl_mac_beacon_update(hw, beacon);
	}

	if (changes & BSS_CHANGED_ERP_PREAMBLE) {
		IWL_DEBUG_MAC80211(priv, "ERP_PREAMBLE %d\n",
				   bss_conf->use_short_preamble);
		if (bss_conf->use_short_preamble)
			priv->staging_rxon.flags |= RXON_FLG_SHORT_PREAMBLE_MSK;
		else
			priv->staging_rxon.flags &= ~RXON_FLG_SHORT_PREAMBLE_MSK;
	}

	if (changes & BSS_CHANGED_ERP_CTS_PROT) {
		IWL_DEBUG_MAC80211(priv, "ERP_CTS %d\n", bss_conf->use_cts_prot);
		if (bss_conf->use_cts_prot && (priv->band != IEEE80211_BAND_5GHZ))
			priv->staging_rxon.flags |= RXON_FLG_TGG_PROTECT_MSK;
		else
			priv->staging_rxon.flags &= ~RXON_FLG_TGG_PROTECT_MSK;
		if (bss_conf->use_cts_prot)
			priv->staging_rxon.flags |= RXON_FLG_SELF_CTS_EN;
		else
			priv->staging_rxon.flags &= ~RXON_FLG_SELF_CTS_EN;
	}

	if (changes & BSS_CHANGED_BASIC_RATES) {
	}

	if (changes & BSS_CHANGED_HT) {
		iwl_ht_conf(priv, vif);

		if (priv->cfg->ops->hcmd->set_rxon_chain)
			priv->cfg->ops->hcmd->set_rxon_chain(priv);
	}

	if (changes & BSS_CHANGED_ASSOC) {
		IWL_DEBUG_MAC80211(priv, "ASSOC %d\n", bss_conf->assoc);
		if (bss_conf->assoc) {
			priv->timestamp = bss_conf->timestamp;

			iwl_led_associate(priv);

			if (!iwl_is_rfkill(priv))
				priv->cfg->ops->lib->post_associate(priv, vif);
		} else
			iwl_set_no_assoc(priv);
	}

	if (changes && iwl_is_associated(priv) && bss_conf->aid) {
		IWL_DEBUG_MAC80211(priv, "Changes (%#x) while associated\n",
				   changes);
		ret = iwl_send_rxon_assoc(priv);
		if (!ret) {
			/* Sync active_rxon with latest change. */
			memcpy((void *)&priv->active_rxon,
				&priv->staging_rxon,
				sizeof(struct iwl_rxon_cmd));
		}
	}

	if (changes & BSS_CHANGED_BEACON_ENABLED) {
		if (vif->bss_conf.enable_beacon) {
			memcpy(priv->staging_rxon.bssid_addr,
			       bss_conf->bssid, ETH_ALEN);
			memcpy(priv->bssid, bss_conf->bssid, ETH_ALEN);
			iwlcore_config_ap(priv, vif);
		} else
			iwl_set_no_assoc(priv);
	}

	if (changes & BSS_CHANGED_IBSS) {
		ret = priv->cfg->ops->lib->manage_ibss_station(priv, vif,
							bss_conf->ibss_joined);
		if (ret)
			IWL_ERR(priv, "failed to %s IBSS station %pM\n",
				bss_conf->ibss_joined ? "add" : "remove",
				bss_conf->bssid);
	}

	mutex_unlock(&priv->mutex);

	IWL_DEBUG_MAC80211(priv, "leave\n");
}
EXPORT_SYMBOL(iwl_bss_info_changed);

static int iwl_set_mode(struct iwl_priv *priv, struct ieee80211_vif *vif)
{
	iwl_connection_init_rx_config(priv, vif);

	if (priv->cfg->ops->hcmd->set_rxon_chain)
		priv->cfg->ops->hcmd->set_rxon_chain(priv);

	return iwlcore_commit_rxon(priv);
}

int iwl_mac_add_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct iwl_priv *priv = hw->priv;
	int err = 0;

	IWL_DEBUG_MAC80211(priv, "enter: type %d, addr %pM\n",
			   vif->type, vif->addr);

	mutex_lock(&priv->mutex);

	if (WARN_ON(!iwl_is_ready_rf(priv))) {
		err = -EINVAL;
		goto out;
	}

	if (priv->vif) {
		IWL_DEBUG_MAC80211(priv, "leave - vif != NULL\n");
		err = -EOPNOTSUPP;
		goto out;
	}

	priv->vif = vif;
	priv->iw_mode = vif->type;

	err = iwl_set_mode(priv, vif);
	if (err)
		goto out_err;

	goto out;

 out_err:
	priv->vif = NULL;
	priv->iw_mode = NL80211_IFTYPE_STATION;
 out:
	mutex_unlock(&priv->mutex);

	IWL_DEBUG_MAC80211(priv, "leave\n");
	return err;
}
EXPORT_SYMBOL(iwl_mac_add_interface);

void iwl_mac_remove_interface(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif)
{
	struct iwl_priv *priv = hw->priv;
	bool scan_completed = false;

	IWL_DEBUG_MAC80211(priv, "enter\n");

	mutex_lock(&priv->mutex);

	if (iwl_is_ready_rf(priv)) {
		iwl_scan_cancel_timeout(priv, 100);
		priv->staging_rxon.filter_flags &= ~RXON_FILTER_ASSOC_MSK;
		iwlcore_commit_rxon(priv);
	}
	if (priv->vif == vif) {
		priv->vif = NULL;
		if (priv->scan_vif == vif) {
			scan_completed = true;
			priv->scan_vif = NULL;
			priv->scan_request = NULL;
		}
		memset(priv->bssid, 0, ETH_ALEN);
	}
	mutex_unlock(&priv->mutex);

	if (scan_completed)
		ieee80211_scan_completed(priv->hw, true);

	IWL_DEBUG_MAC80211(priv, "leave\n");

}
EXPORT_SYMBOL(iwl_mac_remove_interface);

/**
 * iwl_mac_config - mac80211 config callback
 */
int iwl_mac_config(struct ieee80211_hw *hw, u32 changed)
{
	struct iwl_priv *priv = hw->priv;
	const struct iwl_channel_info *ch_info;
	struct ieee80211_conf *conf = &hw->conf;
	struct iwl_ht_config *ht_conf = &priv->current_ht_config;
	unsigned long flags = 0;
	int ret = 0;
	u16 ch;
	int scan_active = 0;

	mutex_lock(&priv->mutex);

	IWL_DEBUG_MAC80211(priv, "enter to channel %d changed 0x%X\n",
					conf->channel->hw_value, changed);

	if (unlikely(!priv->cfg->mod_params->disable_hw_scan &&
			test_bit(STATUS_SCANNING, &priv->status))) {
		scan_active = 1;
		IWL_DEBUG_MAC80211(priv, "leave - scanning\n");
	}

	if (changed & (IEEE80211_CONF_CHANGE_SMPS |
		       IEEE80211_CONF_CHANGE_CHANNEL)) {
		/* mac80211 uses static for non-HT which is what we want */
		priv->current_ht_config.smps = conf->smps_mode;

		/*
		 * Recalculate chain counts.
		 *
		 * If monitor mode is enabled then mac80211 will
		 * set up the SM PS mode to OFF if an HT channel is
		 * configured.
		 */
		if (priv->cfg->ops->hcmd->set_rxon_chain)
			priv->cfg->ops->hcmd->set_rxon_chain(priv);
	}

	/* during scanning mac80211 will delay channel setting until
	 * scan finish with changed = 0
	 */
	if (!changed || (changed & IEEE80211_CONF_CHANGE_CHANNEL)) {
		if (scan_active)
			goto set_ch_out;

		ch = ieee80211_frequency_to_channel(conf->channel->center_freq);
		ch_info = iwl_get_channel_info(priv, conf->channel->band, ch);
		if (!is_channel_valid(ch_info)) {
			IWL_DEBUG_MAC80211(priv, "leave - invalid channel\n");
			ret = -EINVAL;
			goto set_ch_out;
		}

		spin_lock_irqsave(&priv->lock, flags);

		/* Configure HT40 channels */
		ht_conf->is_ht = conf_is_ht(conf);
		if (ht_conf->is_ht) {
			if (conf_is_ht40_minus(conf)) {
				ht_conf->extension_chan_offset =
					IEEE80211_HT_PARAM_CHA_SEC_BELOW;
				ht_conf->is_40mhz = true;
			} else if (conf_is_ht40_plus(conf)) {
				ht_conf->extension_chan_offset =
					IEEE80211_HT_PARAM_CHA_SEC_ABOVE;
				ht_conf->is_40mhz = true;
			} else {
				ht_conf->extension_chan_offset =
					IEEE80211_HT_PARAM_CHA_SEC_NONE;
				ht_conf->is_40mhz = false;
			}
		} else
			ht_conf->is_40mhz = false;
		/* Default to no protection. Protection mode will later be set
		 * from BSS config in iwl_ht_conf */
		ht_conf->ht_protection = IEEE80211_HT_OP_MODE_PROTECTION_NONE;

		/* if we are switching from ht to 2.4 clear flags
		 * from any ht related info since 2.4 does not
		 * support ht */
		if ((le16_to_cpu(priv->staging_rxon.channel) != ch))
			priv->staging_rxon.flags = 0;

		iwl_set_rxon_channel(priv, conf->channel);
		iwl_set_rxon_ht(priv, ht_conf);

		iwl_set_flags_for_band(priv, conf->channel->band, priv->vif);
		spin_unlock_irqrestore(&priv->lock, flags);

		if (priv->cfg->ops->lib->update_bcast_station)
			ret = priv->cfg->ops->lib->update_bcast_station(priv);

 set_ch_out:
		/* The list of supported rates and rate mask can be different
		 * for each band; since the band may have changed, reset
		 * the rate mask to what mac80211 lists */
		iwl_set_rate(priv);
	}

	if (changed & (IEEE80211_CONF_CHANGE_PS |
			IEEE80211_CONF_CHANGE_IDLE)) {
		ret = iwl_power_update_mode(priv, false);
		if (ret)
			IWL_DEBUG_MAC80211(priv, "Error setting sleep level\n");
	}

	if (changed & IEEE80211_CONF_CHANGE_POWER) {
		IWL_DEBUG_MAC80211(priv, "TX Power old=%d new=%d\n",
			priv->tx_power_user_lmt, conf->power_level);

		iwl_set_tx_power(priv, conf->power_level, false);
	}

	if (!iwl_is_ready(priv)) {
		IWL_DEBUG_MAC80211(priv, "leave - not ready\n");
		goto out;
	}

	if (scan_active)
		goto out;

	if (memcmp(&priv->active_rxon,
		   &priv->staging_rxon, sizeof(priv->staging_rxon)))
		iwlcore_commit_rxon(priv);
	else
		IWL_DEBUG_INFO(priv, "Not re-sending same RXON configuration.\n");


out:
	IWL_DEBUG_MAC80211(priv, "leave\n");
	mutex_unlock(&priv->mutex);
	return ret;
}
EXPORT_SYMBOL(iwl_mac_config);

void iwl_mac_reset_tsf(struct ieee80211_hw *hw)
{
	struct iwl_priv *priv = hw->priv;
	unsigned long flags;

	mutex_lock(&priv->mutex);
	IWL_DEBUG_MAC80211(priv, "enter\n");

	spin_lock_irqsave(&priv->lock, flags);
	memset(&priv->current_ht_config, 0, sizeof(struct iwl_ht_config));
	spin_unlock_irqrestore(&priv->lock, flags);

	spin_lock_irqsave(&priv->lock, flags);

	/* new association get rid of ibss beacon skb */
	if (priv->ibss_beacon)
		dev_kfree_skb(priv->ibss_beacon);

	priv->ibss_beacon = NULL;

	priv->timestamp = 0;

	spin_unlock_irqrestore(&priv->lock, flags);

	if (!iwl_is_ready_rf(priv)) {
		IWL_DEBUG_MAC80211(priv, "leave - not ready\n");
		mutex_unlock(&priv->mutex);
		return;
	}

	/* we are restarting association process
	 * clear RXON_FILTER_ASSOC_MSK bit
	 */
	iwl_scan_cancel_timeout(priv, 100);
	priv->staging_rxon.filter_flags &= ~RXON_FILTER_ASSOC_MSK;
	iwlcore_commit_rxon(priv);

	iwl_set_rate(priv);

	mutex_unlock(&priv->mutex);

	IWL_DEBUG_MAC80211(priv, "leave\n");
}
EXPORT_SYMBOL(iwl_mac_reset_tsf);

int iwl_alloc_txq_mem(struct iwl_priv *priv)
{
	if (!priv->txq)
		priv->txq = kzalloc(
			sizeof(struct iwl_tx_queue) * priv->cfg->num_of_queues,
			GFP_KERNEL);
	if (!priv->txq) {
		IWL_ERR(priv, "Not enough memory for txq\n");
		return -ENOMEM;
	}
	return 0;
}
EXPORT_SYMBOL(iwl_alloc_txq_mem);

void iwl_free_txq_mem(struct iwl_priv *priv)
{
	kfree(priv->txq);
	priv->txq = NULL;
}
EXPORT_SYMBOL(iwl_free_txq_mem);

#ifdef CONFIG_IWLWIFI_DEBUGFS

#define IWL_TRAFFIC_DUMP_SIZE	(IWL_TRAFFIC_ENTRY_SIZE * IWL_TRAFFIC_ENTRIES)

void iwl_reset_traffic_log(struct iwl_priv *priv)
{
	priv->tx_traffic_idx = 0;
	priv->rx_traffic_idx = 0;
	if (priv->tx_traffic)
		memset(priv->tx_traffic, 0, IWL_TRAFFIC_DUMP_SIZE);
	if (priv->rx_traffic)
		memset(priv->rx_traffic, 0, IWL_TRAFFIC_DUMP_SIZE);
}

int iwl_alloc_traffic_mem(struct iwl_priv *priv)
{
	u32 traffic_size = IWL_TRAFFIC_DUMP_SIZE;

	if (iwl_debug_level & IWL_DL_TX) {
		if (!priv->tx_traffic) {
			priv->tx_traffic =
				kzalloc(traffic_size, GFP_KERNEL);
			if (!priv->tx_traffic)
				return -ENOMEM;
		}
	}
	if (iwl_debug_level & IWL_DL_RX) {
		if (!priv->rx_traffic) {
			priv->rx_traffic =
				kzalloc(traffic_size, GFP_KERNEL);
			if (!priv->rx_traffic)
				return -ENOMEM;
		}
	}
	iwl_reset_traffic_log(priv);
	return 0;
}
EXPORT_SYMBOL(iwl_alloc_traffic_mem);

void iwl_free_traffic_mem(struct iwl_priv *priv)
{
	kfree(priv->tx_traffic);
	priv->tx_traffic = NULL;

	kfree(priv->rx_traffic);
	priv->rx_traffic = NULL;
}
EXPORT_SYMBOL(iwl_free_traffic_mem);

void iwl_dbg_log_tx_data_frame(struct iwl_priv *priv,
		      u16 length, struct ieee80211_hdr *header)
{
	__le16 fc;
	u16 len;

	if (likely(!(iwl_debug_level & IWL_DL_TX)))
		return;

	if (!priv->tx_traffic)
		return;

	fc = header->frame_control;
	if (ieee80211_is_data(fc)) {
		len = (length > IWL_TRAFFIC_ENTRY_SIZE)
		       ? IWL_TRAFFIC_ENTRY_SIZE : length;
		memcpy((priv->tx_traffic +
		       (priv->tx_traffic_idx * IWL_TRAFFIC_ENTRY_SIZE)),
		       header, len);
		priv->tx_traffic_idx =
			(priv->tx_traffic_idx + 1) % IWL_TRAFFIC_ENTRIES;
	}
}
EXPORT_SYMBOL(iwl_dbg_log_tx_data_frame);

void iwl_dbg_log_rx_data_frame(struct iwl_priv *priv,
		      u16 length, struct ieee80211_hdr *header)
{
	__le16 fc;
	u16 len;

	if (likely(!(iwl_debug_level & IWL_DL_RX)))
		return;

	if (!priv->rx_traffic)
		return;

	fc = header->frame_control;
	if (ieee80211_is_data(fc)) {
		len = (length > IWL_TRAFFIC_ENTRY_SIZE)
		       ? IWL_TRAFFIC_ENTRY_SIZE : length;
		memcpy((priv->rx_traffic +
		       (priv->rx_traffic_idx * IWL_TRAFFIC_ENTRY_SIZE)),
		       header, len);
		priv->rx_traffic_idx =
			(priv->rx_traffic_idx + 1) % IWL_TRAFFIC_ENTRIES;
	}
}
EXPORT_SYMBOL(iwl_dbg_log_rx_data_frame);

const char *get_mgmt_string(int cmd)
{
	switch (cmd) {
		IWL_CMD(MANAGEMENT_ASSOC_REQ);
		IWL_CMD(MANAGEMENT_ASSOC_RESP);
		IWL_CMD(MANAGEMENT_REASSOC_REQ);
		IWL_CMD(MANAGEMENT_REASSOC_RESP);
		IWL_CMD(MANAGEMENT_PROBE_REQ);
		IWL_CMD(MANAGEMENT_PROBE_RESP);
		IWL_CMD(MANAGEMENT_BEACON);
		IWL_CMD(MANAGEMENT_ATIM);
		IWL_CMD(MANAGEMENT_DISASSOC);
		IWL_CMD(MANAGEMENT_AUTH);
		IWL_CMD(MANAGEMENT_DEAUTH);
		IWL_CMD(MANAGEMENT_ACTION);
	default:
		return "UNKNOWN";

	}
}

const char *get_ctrl_string(int cmd)
{
	switch (cmd) {
		IWL_CMD(CONTROL_BACK_REQ);
		IWL_CMD(CONTROL_BACK);
		IWL_CMD(CONTROL_PSPOLL);
		IWL_CMD(CONTROL_RTS);
		IWL_CMD(CONTROL_CTS);
		IWL_CMD(CONTROL_ACK);
		IWL_CMD(CONTROL_CFEND);
		IWL_CMD(CONTROL_CFENDACK);
	default:
		return "UNKNOWN";

	}
}

void iwl_clear_traffic_stats(struct iwl_priv *priv)
{
	memset(&priv->tx_stats, 0, sizeof(struct traffic_stats));
	memset(&priv->rx_stats, 0, sizeof(struct traffic_stats));
	priv->led_tpt = 0;
}

/*
 * if CONFIG_IWLWIFI_DEBUGFS defined, iwl_update_stats function will
 * record all the MGMT, CTRL and DATA pkt for both TX and Rx pass.
 * Use debugFs to display the rx/rx_statistics
 * if CONFIG_IWLWIFI_DEBUGFS not being defined, then no MGMT and CTRL
 * information will be recorded, but DATA pkt still will be recorded
 * for the reason of iwl_led.c need to control the led blinking based on
 * number of tx and rx data.
 *
 */
void iwl_update_stats(struct iwl_priv *priv, bool is_tx, __le16 fc, u16 len)
{
	struct traffic_stats	*stats;

	if (is_tx)
		stats = &priv->tx_stats;
	else
		stats = &priv->rx_stats;

	if (ieee80211_is_mgmt(fc)) {
		switch (fc & cpu_to_le16(IEEE80211_FCTL_STYPE)) {
		case cpu_to_le16(IEEE80211_STYPE_ASSOC_REQ):
			stats->mgmt[MANAGEMENT_ASSOC_REQ]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_ASSOC_RESP):
			stats->mgmt[MANAGEMENT_ASSOC_RESP]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_REASSOC_REQ):
			stats->mgmt[MANAGEMENT_REASSOC_REQ]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_REASSOC_RESP):
			stats->mgmt[MANAGEMENT_REASSOC_RESP]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_PROBE_REQ):
			stats->mgmt[MANAGEMENT_PROBE_REQ]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_PROBE_RESP):
			stats->mgmt[MANAGEMENT_PROBE_RESP]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_BEACON):
			stats->mgmt[MANAGEMENT_BEACON]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_ATIM):
			stats->mgmt[MANAGEMENT_ATIM]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_DISASSOC):
			stats->mgmt[MANAGEMENT_DISASSOC]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_AUTH):
			stats->mgmt[MANAGEMENT_AUTH]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_DEAUTH):
			stats->mgmt[MANAGEMENT_DEAUTH]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_ACTION):
			stats->mgmt[MANAGEMENT_ACTION]++;
			break;
		}
	} else if (ieee80211_is_ctl(fc)) {
		switch (fc & cpu_to_le16(IEEE80211_FCTL_STYPE)) {
		case cpu_to_le16(IEEE80211_STYPE_BACK_REQ):
			stats->ctrl[CONTROL_BACK_REQ]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_BACK):
			stats->ctrl[CONTROL_BACK]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_PSPOLL):
			stats->ctrl[CONTROL_PSPOLL]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_RTS):
			stats->ctrl[CONTROL_RTS]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_CTS):
			stats->ctrl[CONTROL_CTS]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_ACK):
			stats->ctrl[CONTROL_ACK]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_CFEND):
			stats->ctrl[CONTROL_CFEND]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_CFENDACK):
			stats->ctrl[CONTROL_CFENDACK]++;
			break;
		}
	} else {
		/* data */
		stats->data_cnt++;
		stats->data_bytes += len;
	}
	iwl_leds_background(priv);
}
EXPORT_SYMBOL(iwl_update_stats);
#endif

static const char *get_csr_string(int cmd)
{
	switch (cmd) {
		IWL_CMD(CSR_HW_IF_CONFIG_REG);
		IWL_CMD(CSR_INT_COALESCING);
		IWL_CMD(CSR_INT);
		IWL_CMD(CSR_INT_MASK);
		IWL_CMD(CSR_FH_INT_STATUS);
		IWL_CMD(CSR_GPIO_IN);
		IWL_CMD(CSR_RESET);
		IWL_CMD(CSR_GP_CNTRL);
		IWL_CMD(CSR_HW_REV);
		IWL_CMD(CSR_EEPROM_REG);
		IWL_CMD(CSR_EEPROM_GP);
		IWL_CMD(CSR_OTP_GP_REG);
		IWL_CMD(CSR_GIO_REG);
		IWL_CMD(CSR_GP_UCODE_REG);
		IWL_CMD(CSR_GP_DRIVER_REG);
		IWL_CMD(CSR_UCODE_DRV_GP1);
		IWL_CMD(CSR_UCODE_DRV_GP2);
		IWL_CMD(CSR_LED_REG);
		IWL_CMD(CSR_DRAM_INT_TBL_REG);
		IWL_CMD(CSR_GIO_CHICKEN_BITS);
		IWL_CMD(CSR_ANA_PLL_CFG);
		IWL_CMD(CSR_HW_REV_WA_REG);
		IWL_CMD(CSR_DBG_HPET_MEM_REG);
	default:
		return "UNKNOWN";

	}
}

void iwl_dump_csr(struct iwl_priv *priv)
{
	int i;
	u32 csr_tbl[] = {
		CSR_HW_IF_CONFIG_REG,
		CSR_INT_COALESCING,
		CSR_INT,
		CSR_INT_MASK,
		CSR_FH_INT_STATUS,
		CSR_GPIO_IN,
		CSR_RESET,
		CSR_GP_CNTRL,
		CSR_HW_REV,
		CSR_EEPROM_REG,
		CSR_EEPROM_GP,
		CSR_OTP_GP_REG,
		CSR_GIO_REG,
		CSR_GP_UCODE_REG,
		CSR_GP_DRIVER_REG,
		CSR_UCODE_DRV_GP1,
		CSR_UCODE_DRV_GP2,
		CSR_LED_REG,
		CSR_DRAM_INT_TBL_REG,
		CSR_GIO_CHICKEN_BITS,
		CSR_ANA_PLL_CFG,
		CSR_HW_REV_WA_REG,
		CSR_DBG_HPET_MEM_REG
	};
	IWL_ERR(priv, "CSR values:\n");
	IWL_ERR(priv, "(2nd byte of CSR_INT_COALESCING is "
		"CSR_INT_PERIODIC_REG)\n");
	for (i = 0; i <  ARRAY_SIZE(csr_tbl); i++) {
		IWL_ERR(priv, "  %25s: 0X%08x\n",
			get_csr_string(csr_tbl[i]),
			iwl_read32(priv, csr_tbl[i]));
	}
}
EXPORT_SYMBOL(iwl_dump_csr);

static const char *get_fh_string(int cmd)
{
	switch (cmd) {
		IWL_CMD(FH_RSCSR_CHNL0_STTS_WPTR_REG);
		IWL_CMD(FH_RSCSR_CHNL0_RBDCB_BASE_REG);
		IWL_CMD(FH_RSCSR_CHNL0_WPTR);
		IWL_CMD(FH_MEM_RCSR_CHNL0_CONFIG_REG);
		IWL_CMD(FH_MEM_RSSR_SHARED_CTRL_REG);
		IWL_CMD(FH_MEM_RSSR_RX_STATUS_REG);
		IWL_CMD(FH_MEM_RSSR_RX_ENABLE_ERR_IRQ2DRV);
		IWL_CMD(FH_TSSR_TX_STATUS_REG);
		IWL_CMD(FH_TSSR_TX_ERROR_REG);
	default:
		return "UNKNOWN";

	}
}

int iwl_dump_fh(struct iwl_priv *priv, char **buf, bool display)
{
	int i;
#ifdef CONFIG_IWLWIFI_DEBUG
	int pos = 0;
	size_t bufsz = 0;
#endif
	u32 fh_tbl[] = {
		FH_RSCSR_CHNL0_STTS_WPTR_REG,
		FH_RSCSR_CHNL0_RBDCB_BASE_REG,
		FH_RSCSR_CHNL0_WPTR,
		FH_MEM_RCSR_CHNL0_CONFIG_REG,
		FH_MEM_RSSR_SHARED_CTRL_REG,
		FH_MEM_RSSR_RX_STATUS_REG,
		FH_MEM_RSSR_RX_ENABLE_ERR_IRQ2DRV,
		FH_TSSR_TX_STATUS_REG,
		FH_TSSR_TX_ERROR_REG
	};
#ifdef CONFIG_IWLWIFI_DEBUG
	if (display) {
		bufsz = ARRAY_SIZE(fh_tbl) * 48 + 40;
		*buf = kmalloc(bufsz, GFP_KERNEL);
		if (!*buf)
			return -ENOMEM;
		pos += scnprintf(*buf + pos, bufsz - pos,
				"FH register values:\n");
		for (i = 0; i < ARRAY_SIZE(fh_tbl); i++) {
			pos += scnprintf(*buf + pos, bufsz - pos,
				"  %34s: 0X%08x\n",
				get_fh_string(fh_tbl[i]),
				iwl_read_direct32(priv, fh_tbl[i]));
		}
		return pos;
	}
#endif
	IWL_ERR(priv, "FH register values:\n");
	for (i = 0; i <  ARRAY_SIZE(fh_tbl); i++) {
		IWL_ERR(priv, "  %34s: 0X%08x\n",
			get_fh_string(fh_tbl[i]),
			iwl_read_direct32(priv, fh_tbl[i]));
	}
	return 0;
}
EXPORT_SYMBOL(iwl_dump_fh);

static void iwl_force_rf_reset(struct iwl_priv *priv)
{
	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	if (!iwl_is_associated(priv)) {
		IWL_DEBUG_SCAN(priv, "force reset rejected: not associated\n");
		return;
	}
	/*
	 * There is no easy and better way to force reset the radio,
	 * the only known method is switching channel which will force to
	 * reset and tune the radio.
	 * Use internal short scan (single channel) operation to should
	 * achieve this objective.
	 * Driver should reset the radio when number of consecutive missed
	 * beacon, or any other uCode error condition detected.
	 */
	IWL_DEBUG_INFO(priv, "perform radio reset.\n");
	iwl_internal_short_hw_scan(priv);
}


int iwl_force_reset(struct iwl_priv *priv, int mode, bool external)
{
	struct iwl_force_reset *force_reset;

	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return -EINVAL;

	if (test_bit(STATUS_SCANNING, &priv->status)) {
		IWL_DEBUG_INFO(priv, "scan in progress.\n");
		return -EINVAL;
	}

	if (mode >= IWL_MAX_FORCE_RESET) {
		IWL_DEBUG_INFO(priv, "invalid reset request.\n");
		return -EINVAL;
	}
	force_reset = &priv->force_reset[mode];
	force_reset->reset_request_count++;
	if (!external) {
		if (force_reset->last_force_reset_jiffies &&
		    time_after(force_reset->last_force_reset_jiffies +
		    force_reset->reset_duration, jiffies)) {
			IWL_DEBUG_INFO(priv, "force reset rejected\n");
			force_reset->reset_reject_count++;
			return -EAGAIN;
		}
	}
	force_reset->reset_success_count++;
	force_reset->last_force_reset_jiffies = jiffies;
	IWL_DEBUG_INFO(priv, "perform force reset (%d)\n", mode);
	switch (mode) {
	case IWL_RF_RESET:
		iwl_force_rf_reset(priv);
		break;
	case IWL_FW_RESET:
		/*
		 * if the request is from external(ex: debugfs),
		 * then always perform the request in regardless the module
		 * parameter setting
		 * if the request is from internal (uCode error or driver
		 * detect failure), then fw_restart module parameter
		 * need to be check before performing firmware reload
		 */
		if (!external && !priv->cfg->mod_params->restart_fw) {
			IWL_DEBUG_INFO(priv, "Cancel firmware reload based on "
				       "module parameter setting\n");
			break;
		}
		IWL_ERR(priv, "On demand firmware reload\n");
		/* Set the FW error flag -- cleared on iwl_down */
		set_bit(STATUS_FW_ERROR, &priv->status);
		wake_up_interruptible(&priv->wait_command_queue);
		/*
		 * Keep the restart process from trying to send host
		 * commands by clearing the INIT status bit
		 */
		clear_bit(STATUS_READY, &priv->status);
		queue_work(priv->workqueue, &priv->restart);
		break;
	}
	return 0;
}
EXPORT_SYMBOL(iwl_force_reset);

/**
 * iwl_bg_monitor_recover - Timer callback to check for stuck queue and recover
 *
 * During normal condition (no queue is stuck), the timer is continually set to
 * execute every monitor_recover_period milliseconds after the last timer
 * expired.  When the queue read_ptr is at the same place, the timer is
 * shorten to 100mSecs.  This is
 *      1) to reduce the chance that the read_ptr may wrap around (not stuck)
 *      2) to detect the stuck queues quicker before the station and AP can
 *      disassociate each other.
 *
 * This function monitors all the tx queues and recover from it if any
 * of the queues are stuck.
 * 1. It first check the cmd queue for stuck conditions.  If it is stuck,
 *      it will recover by resetting the firmware and return.
 * 2. Then, it checks for station association.  If it associates it will check
 *      other queues.  If any queue is stuck, it will recover by resetting
 *      the firmware.
 * Note: It the number of times the queue read_ptr to be at the same place to
 *      be MAX_REPEAT+1 in order to consider to be stuck.
 */
/*
 * The maximum number of times the read pointer of the tx queue at the
 * same place without considering to be stuck.
 */
#define MAX_REPEAT      (2)
static int iwl_check_stuck_queue(struct iwl_priv *priv, int cnt)
{
	struct iwl_tx_queue *txq;
	struct iwl_queue *q;

	txq = &priv->txq[cnt];
	q = &txq->q;
	/* queue is empty, skip */
	if (q->read_ptr != q->write_ptr) {
		if (q->read_ptr == q->last_read_ptr) {
			/* a queue has not been read from last time */
			if (q->repeat_same_read_ptr > MAX_REPEAT) {
				IWL_ERR(priv,
					"queue %d stuck %d time. Fw reload.\n",
					q->id, q->repeat_same_read_ptr);
				q->repeat_same_read_ptr = 0;
				iwl_force_reset(priv, IWL_FW_RESET, false);
			} else {
				q->repeat_same_read_ptr++;
				IWL_DEBUG_RADIO(priv,
						"queue %d, not read %d time\n",
						q->id,
						q->repeat_same_read_ptr);
				mod_timer(&priv->monitor_recover, jiffies +
					msecs_to_jiffies(IWL_ONE_HUNDRED_MSECS));
			}
			return 1;
		} else {
			q->last_read_ptr = q->read_ptr;
			q->repeat_same_read_ptr = 0;
		}
	}
	return 0;
}

void iwl_bg_monitor_recover(unsigned long data)
{
	struct iwl_priv *priv = (struct iwl_priv *)data;
	int cnt;

	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	/* monitor and check for stuck cmd queue */
	if (iwl_check_stuck_queue(priv, IWL_CMD_QUEUE_NUM))
		return;

	/* monitor and check for other stuck queues */
	if (iwl_is_associated(priv)) {
		for (cnt = 0; cnt < priv->hw_params.max_txq_num; cnt++) {
			/* skip as we already checked the command queue */
			if (cnt == IWL_CMD_QUEUE_NUM)
				continue;
			if (iwl_check_stuck_queue(priv, cnt))
				return;
		}
	}
	/*
	 * Reschedule the timer to occur in
	 * priv->cfg->monitor_recover_period
	 */
	mod_timer(&priv->monitor_recover,
		jiffies + msecs_to_jiffies(priv->cfg->monitor_recover_period));
}
EXPORT_SYMBOL(iwl_bg_monitor_recover);


/*
 * extended beacon time format
 * time in usec will be changed into a 32-bit value in extended:internal format
 * the extended part is the beacon counts
 * the internal part is the time in usec within one beacon interval
 */
u32 iwl_usecs_to_beacons(struct iwl_priv *priv, u32 usec, u32 beacon_interval)
{
	u32 quot;
	u32 rem;
	u32 interval = beacon_interval * TIME_UNIT;

	if (!interval || !usec)
		return 0;

	quot = (usec / interval) &
		(iwl_beacon_time_mask_high(priv,
		priv->hw_params.beacon_time_tsf_bits) >>
		priv->hw_params.beacon_time_tsf_bits);
	rem = (usec % interval) & iwl_beacon_time_mask_low(priv,
				   priv->hw_params.beacon_time_tsf_bits);

	return (quot << priv->hw_params.beacon_time_tsf_bits) + rem;
}
EXPORT_SYMBOL(iwl_usecs_to_beacons);

/* base is usually what we get from ucode with each received frame,
 * the same as HW timer counter counting down
 */
__le32 iwl_add_beacon_time(struct iwl_priv *priv, u32 base,
			   u32 addon, u32 beacon_interval)
{
	u32 base_low = base & iwl_beacon_time_mask_low(priv,
					priv->hw_params.beacon_time_tsf_bits);
	u32 addon_low = addon & iwl_beacon_time_mask_low(priv,
					priv->hw_params.beacon_time_tsf_bits);
	u32 interval = beacon_interval * TIME_UNIT;
	u32 res = (base & iwl_beacon_time_mask_high(priv,
				priv->hw_params.beacon_time_tsf_bits)) +
				(addon & iwl_beacon_time_mask_high(priv,
				priv->hw_params.beacon_time_tsf_bits));

	if (base_low > addon_low)
		res += base_low - addon_low;
	else if (base_low < addon_low) {
		res += interval + base_low - addon_low;
		res += (1 << priv->hw_params.beacon_time_tsf_bits);
	} else
		res += (1 << priv->hw_params.beacon_time_tsf_bits);

	return cpu_to_le32(res);
}
EXPORT_SYMBOL(iwl_add_beacon_time);

#ifdef CONFIG_PM

int iwl_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct iwl_priv *priv = pci_get_drvdata(pdev);

	/*
	 * This function is called when system goes into suspend state
	 * mac80211 will call iwl_mac_stop() from the mac80211 suspend function
	 * first but since iwl_mac_stop() has no knowledge of who the caller is,
	 * it will not call apm_ops.stop() to stop the DMA operation.
	 * Calling apm_ops.stop here to make sure we stop the DMA.
	 */
	priv->cfg->ops->lib->apm_ops.stop(priv);

	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, PCI_D3hot);

	return 0;
}
EXPORT_SYMBOL(iwl_pci_suspend);

int iwl_pci_resume(struct pci_dev *pdev)
{
	struct iwl_priv *priv = pci_get_drvdata(pdev);
	int ret;
	bool hw_rfkill = false;

	/*
	 * We disable the RETRY_TIMEOUT register (0x41) to keep
	 * PCI Tx retries from interfering with C3 CPU state.
	 */
	pci_write_config_byte(pdev, PCI_CFG_RETRY_TIMEOUT, 0x00);

	pci_set_power_state(pdev, PCI_D0);
	ret = pci_enable_device(pdev);
	if (ret)
		return ret;
	pci_restore_state(pdev);
	iwl_enable_interrupts(priv);

	if (!(iwl_read32(priv, CSR_GP_CNTRL) &
				CSR_GP_CNTRL_REG_FLAG_HW_RF_KILL_SW))
		hw_rfkill = true;

	if (hw_rfkill)
		set_bit(STATUS_RF_KILL_HW, &priv->status);
	else
		clear_bit(STATUS_RF_KILL_HW, &priv->status);

	wiphy_rfkill_set_hw_state(priv->hw->wiphy, hw_rfkill);

	return 0;
}
EXPORT_SYMBOL(iwl_pci_resume);

#endif /* CONFIG_PM */
