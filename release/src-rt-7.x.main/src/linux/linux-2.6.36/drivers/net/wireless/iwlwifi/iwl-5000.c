/******************************************************************************
 *
 * Copyright(c) 2007 - 2010 Intel Corporation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <net/mac80211.h>
#include <linux/etherdevice.h>
#include <asm/unaligned.h>

#include "iwl-eeprom.h"
#include "iwl-dev.h"
#include "iwl-core.h"
#include "iwl-io.h"
#include "iwl-sta.h"
#include "iwl-helpers.h"
#include "iwl-agn.h"
#include "iwl-agn-led.h"
#include "iwl-agn-hw.h"
#include "iwl-5000-hw.h"
#include "iwl-agn-debugfs.h"

/* Highest firmware API version supported */
#define IWL5000_UCODE_API_MAX 2
#define IWL5150_UCODE_API_MAX 2

/* Lowest firmware API version supported */
#define IWL5000_UCODE_API_MIN 1
#define IWL5150_UCODE_API_MIN 1

#define IWL5000_FW_PRE "iwlwifi-5000-"
#define _IWL5000_MODULE_FIRMWARE(api) IWL5000_FW_PRE #api ".ucode"
#define IWL5000_MODULE_FIRMWARE(api) _IWL5000_MODULE_FIRMWARE(api)

#define IWL5150_FW_PRE "iwlwifi-5150-"
#define _IWL5150_MODULE_FIRMWARE(api) IWL5150_FW_PRE #api ".ucode"
#define IWL5150_MODULE_FIRMWARE(api) _IWL5150_MODULE_FIRMWARE(api)

/* NIC configuration for 5000 series */
static void iwl5000_nic_config(struct iwl_priv *priv)
{
	unsigned long flags;
	u16 radio_cfg;

	spin_lock_irqsave(&priv->lock, flags);

	radio_cfg = iwl_eeprom_query16(priv, EEPROM_RADIO_CONFIG);

	/* write radio config values to register */
	if (EEPROM_RF_CFG_TYPE_MSK(radio_cfg) < EEPROM_RF_CONFIG_TYPE_MAX)
		iwl_set_bit(priv, CSR_HW_IF_CONFIG_REG,
			    EEPROM_RF_CFG_TYPE_MSK(radio_cfg) |
			    EEPROM_RF_CFG_STEP_MSK(radio_cfg) |
			    EEPROM_RF_CFG_DASH_MSK(radio_cfg));

	/* set CSR_HW_CONFIG_REG for uCode use */
	iwl_set_bit(priv, CSR_HW_IF_CONFIG_REG,
		    CSR_HW_IF_CONFIG_REG_BIT_RADIO_SI |
		    CSR_HW_IF_CONFIG_REG_BIT_MAC_SI);

	/* W/A : NIC is stuck in a reset state after Early PCIe power off
	 * (PCIe power is lost before PERST# is asserted),
	 * causing ME FW to lose ownership and not being able to obtain it back.
	 */
	iwl_set_bits_mask_prph(priv, APMG_PS_CTRL_REG,
				APMG_PS_CTRL_EARLY_PWR_OFF_RESET_DIS,
				~APMG_PS_CTRL_EARLY_PWR_OFF_RESET_DIS);


	spin_unlock_irqrestore(&priv->lock, flags);
}

static struct iwl_sensitivity_ranges iwl5000_sensitivity = {
	.min_nrg_cck = 95,
	.max_nrg_cck = 0, /* not used, set to 0 */
	.auto_corr_min_ofdm = 90,
	.auto_corr_min_ofdm_mrc = 170,
	.auto_corr_min_ofdm_x1 = 120,
	.auto_corr_min_ofdm_mrc_x1 = 240,

	.auto_corr_max_ofdm = 120,
	.auto_corr_max_ofdm_mrc = 210,
	.auto_corr_max_ofdm_x1 = 120,
	.auto_corr_max_ofdm_mrc_x1 = 240,

	.auto_corr_min_cck = 125,
	.auto_corr_max_cck = 200,
	.auto_corr_min_cck_mrc = 170,
	.auto_corr_max_cck_mrc = 400,
	.nrg_th_cck = 95,
	.nrg_th_ofdm = 95,

	.barker_corr_th_min = 190,
	.barker_corr_th_min_mrc = 390,
	.nrg_th_cca = 62,
};

static struct iwl_sensitivity_ranges iwl5150_sensitivity = {
	.min_nrg_cck = 95,
	.max_nrg_cck = 0, /* not used, set to 0 */
	.auto_corr_min_ofdm = 90,
	.auto_corr_min_ofdm_mrc = 170,
	.auto_corr_min_ofdm_x1 = 105,
	.auto_corr_min_ofdm_mrc_x1 = 220,

	.auto_corr_max_ofdm = 120,
	.auto_corr_max_ofdm_mrc = 210,
	/* max = min for performance bug in 5150 DSP */
	.auto_corr_max_ofdm_x1 = 105,
	.auto_corr_max_ofdm_mrc_x1 = 220,

	.auto_corr_min_cck = 125,
	.auto_corr_max_cck = 200,
	.auto_corr_min_cck_mrc = 170,
	.auto_corr_max_cck_mrc = 400,
	.nrg_th_cck = 95,
	.nrg_th_ofdm = 95,

	.barker_corr_th_min = 190,
	.barker_corr_th_min_mrc = 390,
	.nrg_th_cca = 62,
};

static void iwl5150_set_ct_threshold(struct iwl_priv *priv)
{
	const s32 volt2temp_coef = IWL_5150_VOLTAGE_TO_TEMPERATURE_COEFF;
	s32 threshold = (s32)CELSIUS_TO_KELVIN(CT_KILL_THRESHOLD_LEGACY) -
			iwl_temp_calib_to_offset(priv);

	priv->hw_params.ct_kill_threshold = threshold * volt2temp_coef;
}

static void iwl5000_set_ct_threshold(struct iwl_priv *priv)
{
	/* want Celsius */
	priv->hw_params.ct_kill_threshold = CT_KILL_THRESHOLD_LEGACY;
}

static int iwl5000_hw_set_hw_params(struct iwl_priv *priv)
{
	if (priv->cfg->mod_params->num_of_queues >= IWL_MIN_NUM_QUEUES &&
	    priv->cfg->mod_params->num_of_queues <= IWLAGN_NUM_QUEUES)
		priv->cfg->num_of_queues =
			priv->cfg->mod_params->num_of_queues;

	priv->hw_params.max_txq_num = priv->cfg->num_of_queues;
	priv->hw_params.dma_chnl_num = FH50_TCSR_CHNL_NUM;
	priv->hw_params.scd_bc_tbls_size =
			priv->cfg->num_of_queues *
			sizeof(struct iwlagn_scd_bc_tbl);
	priv->hw_params.tfd_size = sizeof(struct iwl_tfd);
	priv->hw_params.max_stations = IWLAGN_STATION_COUNT;
	priv->hw_params.bcast_sta_id = IWLAGN_BROADCAST_ID;

	priv->hw_params.max_data_size = IWLAGN_RTC_DATA_SIZE;
	priv->hw_params.max_inst_size = IWLAGN_RTC_INST_SIZE;

	priv->hw_params.max_bsm_size = 0;
	priv->hw_params.ht40_channel =  BIT(IEEE80211_BAND_2GHZ) |
					BIT(IEEE80211_BAND_5GHZ);
	priv->hw_params.rx_wrt_ptr_reg = FH_RSCSR_CHNL0_WPTR;

	priv->hw_params.tx_chains_num = num_of_ant(priv->cfg->valid_tx_ant);
	priv->hw_params.rx_chains_num = num_of_ant(priv->cfg->valid_rx_ant);
	priv->hw_params.valid_tx_ant = priv->cfg->valid_tx_ant;
	priv->hw_params.valid_rx_ant = priv->cfg->valid_rx_ant;

	if (priv->cfg->ops->lib->temp_ops.set_ct_kill)
		priv->cfg->ops->lib->temp_ops.set_ct_kill(priv);

	/* Set initial sensitivity parameters */
	/* Set initial calibration set */
	priv->hw_params.sens = &iwl5000_sensitivity;
	priv->hw_params.calib_init_cfg =
		BIT(IWL_CALIB_XTAL)		|
		BIT(IWL_CALIB_LO)		|
		BIT(IWL_CALIB_TX_IQ)		|
		BIT(IWL_CALIB_TX_IQ_PERD)	|
		BIT(IWL_CALIB_BASE_BAND);

	priv->hw_params.beacon_time_tsf_bits = IWLAGN_EXT_BEACON_TIME_POS;

	return 0;
}

static int iwl5150_hw_set_hw_params(struct iwl_priv *priv)
{
	if (priv->cfg->mod_params->num_of_queues >= IWL_MIN_NUM_QUEUES &&
	    priv->cfg->mod_params->num_of_queues <= IWLAGN_NUM_QUEUES)
		priv->cfg->num_of_queues =
			priv->cfg->mod_params->num_of_queues;

	priv->hw_params.max_txq_num = priv->cfg->num_of_queues;
	priv->hw_params.dma_chnl_num = FH50_TCSR_CHNL_NUM;
	priv->hw_params.scd_bc_tbls_size =
			priv->cfg->num_of_queues *
			sizeof(struct iwlagn_scd_bc_tbl);
	priv->hw_params.tfd_size = sizeof(struct iwl_tfd);
	priv->hw_params.max_stations = IWLAGN_STATION_COUNT;
	priv->hw_params.bcast_sta_id = IWLAGN_BROADCAST_ID;

	priv->hw_params.max_data_size = IWLAGN_RTC_DATA_SIZE;
	priv->hw_params.max_inst_size = IWLAGN_RTC_INST_SIZE;

	priv->hw_params.max_bsm_size = 0;
	priv->hw_params.ht40_channel =  BIT(IEEE80211_BAND_2GHZ) |
					BIT(IEEE80211_BAND_5GHZ);
	priv->hw_params.rx_wrt_ptr_reg = FH_RSCSR_CHNL0_WPTR;

	priv->hw_params.tx_chains_num = num_of_ant(priv->cfg->valid_tx_ant);
	priv->hw_params.rx_chains_num = num_of_ant(priv->cfg->valid_rx_ant);
	priv->hw_params.valid_tx_ant = priv->cfg->valid_tx_ant;
	priv->hw_params.valid_rx_ant = priv->cfg->valid_rx_ant;

	if (priv->cfg->ops->lib->temp_ops.set_ct_kill)
		priv->cfg->ops->lib->temp_ops.set_ct_kill(priv);

	/* Set initial sensitivity parameters */
	/* Set initial calibration set */
	priv->hw_params.sens = &iwl5150_sensitivity;
	priv->hw_params.calib_init_cfg =
		BIT(IWL_CALIB_LO)		|
		BIT(IWL_CALIB_TX_IQ)		|
		BIT(IWL_CALIB_BASE_BAND);
	if (priv->cfg->need_dc_calib)
		priv->hw_params.calib_init_cfg |= BIT(IWL_CALIB_DC);

	priv->hw_params.beacon_time_tsf_bits = IWLAGN_EXT_BEACON_TIME_POS;

	return 0;
}

static void iwl5150_temperature(struct iwl_priv *priv)
{
	u32 vt = 0;
	s32 offset =  iwl_temp_calib_to_offset(priv);

	vt = le32_to_cpu(priv->_agn.statistics.general.common.temperature);
	vt = vt / IWL_5150_VOLTAGE_TO_TEMPERATURE_COEFF + offset;
	/* now vt hold the temperature in Kelvin */
	priv->temperature = KELVIN_TO_CELSIUS(vt);
	iwl_tt_handler(priv);
}

static int iwl5000_hw_channel_switch(struct iwl_priv *priv,
				     struct ieee80211_channel_switch *ch_switch)
{
	struct iwl5000_channel_switch_cmd cmd;
	const struct iwl_channel_info *ch_info;
	u32 switch_time_in_usec, ucode_switch_time;
	u16 ch;
	u32 tsf_low;
	u8 switch_count;
	u16 beacon_interval = le16_to_cpu(priv->rxon_timing.beacon_interval);
	struct ieee80211_vif *vif = priv->vif;
	struct iwl_host_cmd hcmd = {
		.id = REPLY_CHANNEL_SWITCH,
		.len = sizeof(cmd),
		.flags = CMD_SYNC,
		.data = &cmd,
	};

	cmd.band = priv->band == IEEE80211_BAND_2GHZ;
	ch = ieee80211_frequency_to_channel(ch_switch->channel->center_freq);
	IWL_DEBUG_11H(priv, "channel switch from %d to %d\n",
		priv->active_rxon.channel, ch);
	cmd.channel = cpu_to_le16(ch);
	cmd.rxon_flags = priv->staging_rxon.flags;
	cmd.rxon_filter_flags = priv->staging_rxon.filter_flags;
	switch_count = ch_switch->count;
	tsf_low = ch_switch->timestamp & 0x0ffffffff;
	/*
	 * calculate the ucode channel switch time
	 * adding TSF as one of the factor for when to switch
	 */
	if ((priv->ucode_beacon_time > tsf_low) && beacon_interval) {
		if (switch_count > ((priv->ucode_beacon_time - tsf_low) /
		    beacon_interval)) {
			switch_count -= (priv->ucode_beacon_time -
				tsf_low) / beacon_interval;
		} else
			switch_count = 0;
	}
	if (switch_count <= 1)
		cmd.switch_time = cpu_to_le32(priv->ucode_beacon_time);
	else {
		switch_time_in_usec =
			vif->bss_conf.beacon_int * switch_count * TIME_UNIT;
		ucode_switch_time = iwl_usecs_to_beacons(priv,
							 switch_time_in_usec,
							 beacon_interval);
		cmd.switch_time = iwl_add_beacon_time(priv,
						      priv->ucode_beacon_time,
						      ucode_switch_time,
						      beacon_interval);
	}
	IWL_DEBUG_11H(priv, "uCode time for the switch is 0x%x\n",
		      cmd.switch_time);
	ch_info = iwl_get_channel_info(priv, priv->band, ch);
	if (ch_info)
		cmd.expect_beacon = is_channel_radar(ch_info);
	else {
		IWL_ERR(priv, "invalid channel switch from %u to %u\n",
			priv->active_rxon.channel, ch);
		return -EFAULT;
	}
	priv->switch_rxon.channel = cmd.channel;
	priv->switch_rxon.switch_in_progress = true;

	return iwl_send_cmd_sync(priv, &hcmd);
}

static struct iwl_lib_ops iwl5000_lib = {
	.set_hw_params = iwl5000_hw_set_hw_params,
	.txq_update_byte_cnt_tbl = iwlagn_txq_update_byte_cnt_tbl,
	.txq_inval_byte_cnt_tbl = iwlagn_txq_inval_byte_cnt_tbl,
	.txq_set_sched = iwlagn_txq_set_sched,
	.txq_agg_enable = iwlagn_txq_agg_enable,
	.txq_agg_disable = iwlagn_txq_agg_disable,
	.txq_attach_buf_to_tfd = iwl_hw_txq_attach_buf_to_tfd,
	.txq_free_tfd = iwl_hw_txq_free_tfd,
	.txq_init = iwl_hw_tx_queue_init,
	.rx_handler_setup = iwlagn_rx_handler_setup,
	.setup_deferred_work = iwlagn_setup_deferred_work,
	.is_valid_rtc_data_addr = iwlagn_hw_valid_rtc_data_addr,
	.dump_nic_event_log = iwl_dump_nic_event_log,
	.dump_nic_error_log = iwl_dump_nic_error_log,
	.dump_csr = iwl_dump_csr,
	.dump_fh = iwl_dump_fh,
	.load_ucode = iwlagn_load_ucode,
	.init_alive_start = iwlagn_init_alive_start,
	.alive_notify = iwlagn_alive_notify,
	.send_tx_power = iwlagn_send_tx_power,
	.update_chain_flags = iwl_update_chain_flags,
	.set_channel_switch = iwl5000_hw_channel_switch,
	.apm_ops = {
		.init = iwl_apm_init,
		.stop = iwl_apm_stop,
		.config = iwl5000_nic_config,
		.set_pwr_src = iwl_set_pwr_src,
	},
	.eeprom_ops = {
		.regulatory_bands = {
			EEPROM_REG_BAND_1_CHANNELS,
			EEPROM_REG_BAND_2_CHANNELS,
			EEPROM_REG_BAND_3_CHANNELS,
			EEPROM_REG_BAND_4_CHANNELS,
			EEPROM_REG_BAND_5_CHANNELS,
			EEPROM_REG_BAND_24_HT40_CHANNELS,
			EEPROM_REG_BAND_52_HT40_CHANNELS
		},
		.verify_signature  = iwlcore_eeprom_verify_signature,
		.acquire_semaphore = iwlcore_eeprom_acquire_semaphore,
		.release_semaphore = iwlcore_eeprom_release_semaphore,
		.calib_version	= iwlagn_eeprom_calib_version,
		.query_addr = iwlagn_eeprom_query_addr,
	},
	.post_associate = iwl_post_associate,
	.isr = iwl_isr_ict,
	.config_ap = iwl_config_ap,
	.temp_ops = {
		.temperature = iwlagn_temperature,
		.set_ct_kill = iwl5000_set_ct_threshold,
	 },
	.manage_ibss_station = iwlagn_manage_ibss_station,
	.update_bcast_station = iwl_update_bcast_station,
	.debugfs_ops = {
		.rx_stats_read = iwl_ucode_rx_stats_read,
		.tx_stats_read = iwl_ucode_tx_stats_read,
		.general_stats_read = iwl_ucode_general_stats_read,
		.bt_stats_read = iwl_ucode_bt_stats_read,
	},
	.recover_from_tx_stall = iwl_bg_monitor_recover,
	.check_plcp_health = iwl_good_plcp_health,
	.check_ack_health = iwl_good_ack_health,
	.txfifo_flush = iwlagn_txfifo_flush,
	.dev_txfifo_flush = iwlagn_dev_txfifo_flush,
};

static struct iwl_lib_ops iwl5150_lib = {
	.set_hw_params = iwl5150_hw_set_hw_params,
	.txq_update_byte_cnt_tbl = iwlagn_txq_update_byte_cnt_tbl,
	.txq_inval_byte_cnt_tbl = iwlagn_txq_inval_byte_cnt_tbl,
	.txq_set_sched = iwlagn_txq_set_sched,
	.txq_agg_enable = iwlagn_txq_agg_enable,
	.txq_agg_disable = iwlagn_txq_agg_disable,
	.txq_attach_buf_to_tfd = iwl_hw_txq_attach_buf_to_tfd,
	.txq_free_tfd = iwl_hw_txq_free_tfd,
	.txq_init = iwl_hw_tx_queue_init,
	.rx_handler_setup = iwlagn_rx_handler_setup,
	.setup_deferred_work = iwlagn_setup_deferred_work,
	.is_valid_rtc_data_addr = iwlagn_hw_valid_rtc_data_addr,
	.dump_nic_event_log = iwl_dump_nic_event_log,
	.dump_nic_error_log = iwl_dump_nic_error_log,
	.dump_csr = iwl_dump_csr,
	.load_ucode = iwlagn_load_ucode,
	.init_alive_start = iwlagn_init_alive_start,
	.alive_notify = iwlagn_alive_notify,
	.send_tx_power = iwlagn_send_tx_power,
	.update_chain_flags = iwl_update_chain_flags,
	.set_channel_switch = iwl5000_hw_channel_switch,
	.apm_ops = {
		.init = iwl_apm_init,
		.stop = iwl_apm_stop,
		.config = iwl5000_nic_config,
		.set_pwr_src = iwl_set_pwr_src,
	},
	.eeprom_ops = {
		.regulatory_bands = {
			EEPROM_REG_BAND_1_CHANNELS,
			EEPROM_REG_BAND_2_CHANNELS,
			EEPROM_REG_BAND_3_CHANNELS,
			EEPROM_REG_BAND_4_CHANNELS,
			EEPROM_REG_BAND_5_CHANNELS,
			EEPROM_REG_BAND_24_HT40_CHANNELS,
			EEPROM_REG_BAND_52_HT40_CHANNELS
		},
		.verify_signature  = iwlcore_eeprom_verify_signature,
		.acquire_semaphore = iwlcore_eeprom_acquire_semaphore,
		.release_semaphore = iwlcore_eeprom_release_semaphore,
		.calib_version	= iwlagn_eeprom_calib_version,
		.query_addr = iwlagn_eeprom_query_addr,
	},
	.post_associate = iwl_post_associate,
	.isr = iwl_isr_ict,
	.config_ap = iwl_config_ap,
	.temp_ops = {
		.temperature = iwl5150_temperature,
		.set_ct_kill = iwl5150_set_ct_threshold,
	 },
	.manage_ibss_station = iwlagn_manage_ibss_station,
	.update_bcast_station = iwl_update_bcast_station,
	.debugfs_ops = {
		.rx_stats_read = iwl_ucode_rx_stats_read,
		.tx_stats_read = iwl_ucode_tx_stats_read,
		.general_stats_read = iwl_ucode_general_stats_read,
	},
	.recover_from_tx_stall = iwl_bg_monitor_recover,
	.check_plcp_health = iwl_good_plcp_health,
	.check_ack_health = iwl_good_ack_health,
	.txfifo_flush = iwlagn_txfifo_flush,
	.dev_txfifo_flush = iwlagn_dev_txfifo_flush,
};

static const struct iwl_ops iwl5000_ops = {
	.lib = &iwl5000_lib,
	.hcmd = &iwlagn_hcmd,
	.utils = &iwlagn_hcmd_utils,
	.led = &iwlagn_led_ops,
};

static const struct iwl_ops iwl5150_ops = {
	.lib = &iwl5150_lib,
	.hcmd = &iwlagn_hcmd,
	.utils = &iwlagn_hcmd_utils,
	.led = &iwlagn_led_ops,
};

struct iwl_cfg iwl5300_agn_cfg = {
	.name = "Intel(R) Ultimate N WiFi Link 5300 AGN",
	.fw_name_pre = IWL5000_FW_PRE,
	.ucode_api_max = IWL5000_UCODE_API_MAX,
	.ucode_api_min = IWL5000_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G|IWL_SKU_N,
	.ops = &iwl5000_ops,
	.eeprom_size = IWLAGN_EEPROM_IMG_SIZE,
	.eeprom_ver = EEPROM_5000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.num_of_queues = IWLAGN_NUM_QUEUES,
	.num_of_ampdu_queues = IWLAGN_NUM_AMPDU_QUEUES,
	.mod_params = &iwlagn_mod_params,
	.valid_tx_ant = ANT_ABC,
	.valid_rx_ant = ANT_ABC,
	.pll_cfg_val = CSR50_ANA_PLL_CFG_VAL,
	.set_l0s = true,
	.use_bsm = false,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.use_rts_for_aggregation = true, /* use rts/cts protection */
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.plcp_delta_threshold = IWL_MAX_PLCP_ERR_LONG_THRESHOLD_DEF,
	.chain_noise_scale = 1000,
	.monitor_recover_period = IWL_LONG_MONITORING_PERIOD,
	.max_event_log_size = 512,
	.ucode_tracing = true,
	.sensitivity_calib_by_driver = true,
	.chain_noise_calib_by_driver = true,
};

struct iwl_cfg iwl5100_bgn_cfg = {
	.name = "Intel(R) WiFi Link 5100 BGN",
	.fw_name_pre = IWL5000_FW_PRE,
	.ucode_api_max = IWL5000_UCODE_API_MAX,
	.ucode_api_min = IWL5000_UCODE_API_MIN,
	.sku = IWL_SKU_G|IWL_SKU_N,
	.ops = &iwl5000_ops,
	.eeprom_size = IWLAGN_EEPROM_IMG_SIZE,
	.eeprom_ver = EEPROM_5000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.num_of_queues = IWLAGN_NUM_QUEUES,
	.num_of_ampdu_queues = IWLAGN_NUM_AMPDU_QUEUES,
	.mod_params = &iwlagn_mod_params,
	.valid_tx_ant = ANT_B,
	.valid_rx_ant = ANT_AB,
	.pll_cfg_val = CSR50_ANA_PLL_CFG_VAL,
	.set_l0s = true,
	.use_bsm = false,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.use_rts_for_aggregation = true, /* use rts/cts protection */
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.plcp_delta_threshold = IWL_MAX_PLCP_ERR_LONG_THRESHOLD_DEF,
	.chain_noise_scale = 1000,
	.monitor_recover_period = IWL_LONG_MONITORING_PERIOD,
	.max_event_log_size = 512,
	.ucode_tracing = true,
	.sensitivity_calib_by_driver = true,
	.chain_noise_calib_by_driver = true,
};

struct iwl_cfg iwl5100_abg_cfg = {
	.name = "Intel(R) WiFi Link 5100 ABG",
	.fw_name_pre = IWL5000_FW_PRE,
	.ucode_api_max = IWL5000_UCODE_API_MAX,
	.ucode_api_min = IWL5000_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G,
	.ops = &iwl5000_ops,
	.eeprom_size = IWLAGN_EEPROM_IMG_SIZE,
	.eeprom_ver = EEPROM_5000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.num_of_queues = IWLAGN_NUM_QUEUES,
	.num_of_ampdu_queues = IWLAGN_NUM_AMPDU_QUEUES,
	.mod_params = &iwlagn_mod_params,
	.valid_tx_ant = ANT_B,
	.valid_rx_ant = ANT_AB,
	.pll_cfg_val = CSR50_ANA_PLL_CFG_VAL,
	.set_l0s = true,
	.use_bsm = false,
	.led_compensation = 51,
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.plcp_delta_threshold = IWL_MAX_PLCP_ERR_LONG_THRESHOLD_DEF,
	.chain_noise_scale = 1000,
	.monitor_recover_period = IWL_LONG_MONITORING_PERIOD,
	.max_event_log_size = 512,
	.ucode_tracing = true,
	.sensitivity_calib_by_driver = true,
	.chain_noise_calib_by_driver = true,
};

struct iwl_cfg iwl5100_agn_cfg = {
	.name = "Intel(R) WiFi Link 5100 AGN",
	.fw_name_pre = IWL5000_FW_PRE,
	.ucode_api_max = IWL5000_UCODE_API_MAX,
	.ucode_api_min = IWL5000_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G|IWL_SKU_N,
	.ops = &iwl5000_ops,
	.eeprom_size = IWLAGN_EEPROM_IMG_SIZE,
	.eeprom_ver = EEPROM_5000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.num_of_queues = IWLAGN_NUM_QUEUES,
	.num_of_ampdu_queues = IWLAGN_NUM_AMPDU_QUEUES,
	.mod_params = &iwlagn_mod_params,
	.valid_tx_ant = ANT_B,
	.valid_rx_ant = ANT_AB,
	.pll_cfg_val = CSR50_ANA_PLL_CFG_VAL,
	.set_l0s = true,
	.use_bsm = false,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.use_rts_for_aggregation = true, /* use rts/cts protection */
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.plcp_delta_threshold = IWL_MAX_PLCP_ERR_LONG_THRESHOLD_DEF,
	.chain_noise_scale = 1000,
	.monitor_recover_period = IWL_LONG_MONITORING_PERIOD,
	.max_event_log_size = 512,
	.ucode_tracing = true,
	.sensitivity_calib_by_driver = true,
	.chain_noise_calib_by_driver = true,
};

struct iwl_cfg iwl5350_agn_cfg = {
	.name = "Intel(R) WiMAX/WiFi Link 5350 AGN",
	.fw_name_pre = IWL5000_FW_PRE,
	.ucode_api_max = IWL5000_UCODE_API_MAX,
	.ucode_api_min = IWL5000_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G|IWL_SKU_N,
	.ops = &iwl5000_ops,
	.eeprom_size = IWLAGN_EEPROM_IMG_SIZE,
	.eeprom_ver = EEPROM_5050_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5050_TX_POWER_VERSION,
	.num_of_queues = IWLAGN_NUM_QUEUES,
	.num_of_ampdu_queues = IWLAGN_NUM_AMPDU_QUEUES,
	.mod_params = &iwlagn_mod_params,
	.valid_tx_ant = ANT_ABC,
	.valid_rx_ant = ANT_ABC,
	.pll_cfg_val = CSR50_ANA_PLL_CFG_VAL,
	.set_l0s = true,
	.use_bsm = false,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.use_rts_for_aggregation = true, /* use rts/cts protection */
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.plcp_delta_threshold = IWL_MAX_PLCP_ERR_LONG_THRESHOLD_DEF,
	.chain_noise_scale = 1000,
	.monitor_recover_period = IWL_LONG_MONITORING_PERIOD,
	.max_event_log_size = 512,
	.ucode_tracing = true,
	.sensitivity_calib_by_driver = true,
	.chain_noise_calib_by_driver = true,
};

struct iwl_cfg iwl5150_agn_cfg = {
	.name = "Intel(R) WiMAX/WiFi Link 5150 AGN",
	.fw_name_pre = IWL5150_FW_PRE,
	.ucode_api_max = IWL5150_UCODE_API_MAX,
	.ucode_api_min = IWL5150_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G|IWL_SKU_N,
	.ops = &iwl5150_ops,
	.eeprom_size = IWLAGN_EEPROM_IMG_SIZE,
	.eeprom_ver = EEPROM_5050_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5050_TX_POWER_VERSION,
	.num_of_queues = IWLAGN_NUM_QUEUES,
	.num_of_ampdu_queues = IWLAGN_NUM_AMPDU_QUEUES,
	.mod_params = &iwlagn_mod_params,
	.valid_tx_ant = ANT_A,
	.valid_rx_ant = ANT_AB,
	.pll_cfg_val = CSR50_ANA_PLL_CFG_VAL,
	.set_l0s = true,
	.use_bsm = false,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.use_rts_for_aggregation = true, /* use rts/cts protection */
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.plcp_delta_threshold = IWL_MAX_PLCP_ERR_LONG_THRESHOLD_DEF,
	.chain_noise_scale = 1000,
	.monitor_recover_period = IWL_LONG_MONITORING_PERIOD,
	.max_event_log_size = 512,
	.ucode_tracing = true,
	.sensitivity_calib_by_driver = true,
	.chain_noise_calib_by_driver = true,
	.need_dc_calib = true,
};

struct iwl_cfg iwl5150_abg_cfg = {
	.name = "Intel(R) WiMAX/WiFi Link 5150 ABG",
	.fw_name_pre = IWL5150_FW_PRE,
	.ucode_api_max = IWL5150_UCODE_API_MAX,
	.ucode_api_min = IWL5150_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G,
	.ops = &iwl5150_ops,
	.eeprom_size = IWLAGN_EEPROM_IMG_SIZE,
	.eeprom_ver = EEPROM_5050_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5050_TX_POWER_VERSION,
	.num_of_queues = IWLAGN_NUM_QUEUES,
	.num_of_ampdu_queues = IWLAGN_NUM_AMPDU_QUEUES,
	.mod_params = &iwlagn_mod_params,
	.valid_tx_ant = ANT_A,
	.valid_rx_ant = ANT_AB,
	.pll_cfg_val = CSR50_ANA_PLL_CFG_VAL,
	.set_l0s = true,
	.use_bsm = false,
	.led_compensation = 51,
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.plcp_delta_threshold = IWL_MAX_PLCP_ERR_LONG_THRESHOLD_DEF,
	.chain_noise_scale = 1000,
	.monitor_recover_period = IWL_LONG_MONITORING_PERIOD,
	.max_event_log_size = 512,
	.ucode_tracing = true,
	.sensitivity_calib_by_driver = true,
	.chain_noise_calib_by_driver = true,
	.need_dc_calib = true,
};

MODULE_FIRMWARE(IWL5000_MODULE_FIRMWARE(IWL5000_UCODE_API_MAX));
MODULE_FIRMWARE(IWL5150_MODULE_FIRMWARE(IWL5150_UCODE_API_MAX));
