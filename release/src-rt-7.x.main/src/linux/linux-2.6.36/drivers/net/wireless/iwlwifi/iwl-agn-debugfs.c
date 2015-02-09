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

#include "iwl-agn-debugfs.h"

static int iwl_statistics_flag(struct iwl_priv *priv, char *buf, int bufsz)
{
	int p = 0;
	u32 flag;

	if (priv->cfg->bt_statistics)
		flag = le32_to_cpu(priv->_agn.statistics_bt.flag);
	else
		flag = le32_to_cpu(priv->_agn.statistics.flag);

	p += scnprintf(buf + p, bufsz - p, "Statistics Flag(0x%X):\n", flag);
	if (flag & UCODE_STATISTICS_CLEAR_MSK)
		p += scnprintf(buf + p, bufsz - p,
		"\tStatistics have been cleared\n");
	p += scnprintf(buf + p, bufsz - p, "\tOperational Frequency: %s\n",
		(flag & UCODE_STATISTICS_FREQUENCY_MSK)
		? "2.4 GHz" : "5.2 GHz");
	p += scnprintf(buf + p, bufsz - p, "\tTGj Narrow Band: %s\n",
		(flag & UCODE_STATISTICS_NARROW_BAND_MSK)
		 ? "enabled" : "disabled");

	return p;
}

ssize_t iwl_ucode_rx_stats_read(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
  {
	struct iwl_priv *priv = file->private_data;
	int pos = 0;
	char *buf;
	int bufsz = sizeof(struct statistics_rx_phy) * 40 +
		    sizeof(struct statistics_rx_non_phy) * 40 +
		    sizeof(struct statistics_rx_ht_phy) * 40 + 400;
	ssize_t ret;
	struct statistics_rx_phy *ofdm, *accum_ofdm, *delta_ofdm, *max_ofdm;
	struct statistics_rx_phy *cck, *accum_cck, *delta_cck, *max_cck;
	struct statistics_rx_non_phy *general, *accum_general;
	struct statistics_rx_non_phy *delta_general, *max_general;
	struct statistics_rx_ht_phy *ht, *accum_ht, *delta_ht, *max_ht;

	if (!iwl_is_alive(priv))
		return -EAGAIN;

	buf = kzalloc(bufsz, GFP_KERNEL);
	if (!buf) {
		IWL_ERR(priv, "Can not allocate Buffer\n");
		return -ENOMEM;
	}

	/*
	 * the statistic information display here is based on
	 * the last statistics notification from uCode
	 * might not reflect the current uCode activity
	 */
	if (priv->cfg->bt_statistics) {
		ofdm = &priv->_agn.statistics_bt.rx.ofdm;
		cck = &priv->_agn.statistics_bt.rx.cck;
		general = &priv->_agn.statistics_bt.rx.general.common;
		ht = &priv->_agn.statistics_bt.rx.ofdm_ht;
		accum_ofdm = &priv->_agn.accum_statistics_bt.rx.ofdm;
		accum_cck = &priv->_agn.accum_statistics_bt.rx.cck;
		accum_general =
			&priv->_agn.accum_statistics_bt.rx.general.common;
		accum_ht = &priv->_agn.accum_statistics_bt.rx.ofdm_ht;
		delta_ofdm = &priv->_agn.delta_statistics_bt.rx.ofdm;
		delta_cck = &priv->_agn.delta_statistics_bt.rx.cck;
		delta_general =
			&priv->_agn.delta_statistics_bt.rx.general.common;
		delta_ht = &priv->_agn.delta_statistics_bt.rx.ofdm_ht;
		max_ofdm = &priv->_agn.max_delta_bt.rx.ofdm;
		max_cck = &priv->_agn.max_delta_bt.rx.cck;
		max_general = &priv->_agn.max_delta_bt.rx.general.common;
		max_ht = &priv->_agn.max_delta_bt.rx.ofdm_ht;
	} else {
		ofdm = &priv->_agn.statistics.rx.ofdm;
		cck = &priv->_agn.statistics.rx.cck;
		general = &priv->_agn.statistics.rx.general;
		ht = &priv->_agn.statistics.rx.ofdm_ht;
		accum_ofdm = &priv->_agn.accum_statistics.rx.ofdm;
		accum_cck = &priv->_agn.accum_statistics.rx.cck;
		accum_general = &priv->_agn.accum_statistics.rx.general;
		accum_ht = &priv->_agn.accum_statistics.rx.ofdm_ht;
		delta_ofdm = &priv->_agn.delta_statistics.rx.ofdm;
		delta_cck = &priv->_agn.delta_statistics.rx.cck;
		delta_general = &priv->_agn.delta_statistics.rx.general;
		delta_ht = &priv->_agn.delta_statistics.rx.ofdm_ht;
		max_ofdm = &priv->_agn.max_delta.rx.ofdm;
		max_cck = &priv->_agn.max_delta.rx.cck;
		max_general = &priv->_agn.max_delta.rx.general;
		max_ht = &priv->_agn.max_delta.rx.ofdm_ht;
	}

	pos += iwl_statistics_flag(priv, buf, bufsz);
	pos += scnprintf(buf + pos, bufsz - pos, "%-32s     current"
			 "acumulative       delta         max\n",
			 "Statistics_Rx - OFDM:");
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "ina_cnt:", le32_to_cpu(ofdm->ina_cnt),
			 accum_ofdm->ina_cnt,
			 delta_ofdm->ina_cnt, max_ofdm->ina_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "fina_cnt:",
			 le32_to_cpu(ofdm->fina_cnt), accum_ofdm->fina_cnt,
			 delta_ofdm->fina_cnt, max_ofdm->fina_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "plcp_err:",
			 le32_to_cpu(ofdm->plcp_err), accum_ofdm->plcp_err,
			 delta_ofdm->plcp_err, max_ofdm->plcp_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "crc32_err:",
			 le32_to_cpu(ofdm->crc32_err), accum_ofdm->crc32_err,
			 delta_ofdm->crc32_err, max_ofdm->crc32_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "overrun_err:",
			 le32_to_cpu(ofdm->overrun_err),
			 accum_ofdm->overrun_err, delta_ofdm->overrun_err,
			 max_ofdm->overrun_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "early_overrun_err:",
			 le32_to_cpu(ofdm->early_overrun_err),
			 accum_ofdm->early_overrun_err,
			 delta_ofdm->early_overrun_err,
			 max_ofdm->early_overrun_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "crc32_good:", le32_to_cpu(ofdm->crc32_good),
			 accum_ofdm->crc32_good, delta_ofdm->crc32_good,
			 max_ofdm->crc32_good);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "false_alarm_cnt:",
			 le32_to_cpu(ofdm->false_alarm_cnt),
			 accum_ofdm->false_alarm_cnt,
			 delta_ofdm->false_alarm_cnt,
			 max_ofdm->false_alarm_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "fina_sync_err_cnt:",
			 le32_to_cpu(ofdm->fina_sync_err_cnt),
			 accum_ofdm->fina_sync_err_cnt,
			 delta_ofdm->fina_sync_err_cnt,
			 max_ofdm->fina_sync_err_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "sfd_timeout:",
			 le32_to_cpu(ofdm->sfd_timeout),
			 accum_ofdm->sfd_timeout, delta_ofdm->sfd_timeout,
			 max_ofdm->sfd_timeout);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "fina_timeout:",
			 le32_to_cpu(ofdm->fina_timeout),
			 accum_ofdm->fina_timeout, delta_ofdm->fina_timeout,
			 max_ofdm->fina_timeout);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "unresponded_rts:",
			 le32_to_cpu(ofdm->unresponded_rts),
			 accum_ofdm->unresponded_rts,
			 delta_ofdm->unresponded_rts,
			 max_ofdm->unresponded_rts);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "rxe_frame_lmt_ovrun:",
			 le32_to_cpu(ofdm->rxe_frame_limit_overrun),
			 accum_ofdm->rxe_frame_limit_overrun,
			 delta_ofdm->rxe_frame_limit_overrun,
			 max_ofdm->rxe_frame_limit_overrun);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "sent_ack_cnt:",
			 le32_to_cpu(ofdm->sent_ack_cnt),
			 accum_ofdm->sent_ack_cnt, delta_ofdm->sent_ack_cnt,
			 max_ofdm->sent_ack_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "sent_cts_cnt:",
			 le32_to_cpu(ofdm->sent_cts_cnt),
			 accum_ofdm->sent_cts_cnt, delta_ofdm->sent_cts_cnt,
			 max_ofdm->sent_cts_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "sent_ba_rsp_cnt:",
			 le32_to_cpu(ofdm->sent_ba_rsp_cnt),
			 accum_ofdm->sent_ba_rsp_cnt,
			 delta_ofdm->sent_ba_rsp_cnt,
			 max_ofdm->sent_ba_rsp_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "dsp_self_kill:",
			 le32_to_cpu(ofdm->dsp_self_kill),
			 accum_ofdm->dsp_self_kill,
			 delta_ofdm->dsp_self_kill,
			 max_ofdm->dsp_self_kill);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "mh_format_err:",
			 le32_to_cpu(ofdm->mh_format_err),
			 accum_ofdm->mh_format_err,
			 delta_ofdm->mh_format_err,
			 max_ofdm->mh_format_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "re_acq_main_rssi_sum:",
			 le32_to_cpu(ofdm->re_acq_main_rssi_sum),
			 accum_ofdm->re_acq_main_rssi_sum,
			 delta_ofdm->re_acq_main_rssi_sum,
			 max_ofdm->re_acq_main_rssi_sum);

	pos += scnprintf(buf + pos, bufsz - pos, "%-32s     current"
			 "acumulative       delta         max\n",
			 "Statistics_Rx - CCK:");
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "ina_cnt:",
			 le32_to_cpu(cck->ina_cnt), accum_cck->ina_cnt,
			 delta_cck->ina_cnt, max_cck->ina_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "fina_cnt:",
			 le32_to_cpu(cck->fina_cnt), accum_cck->fina_cnt,
			 delta_cck->fina_cnt, max_cck->fina_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "plcp_err:",
			 le32_to_cpu(cck->plcp_err), accum_cck->plcp_err,
			 delta_cck->plcp_err, max_cck->plcp_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "crc32_err:",
			 le32_to_cpu(cck->crc32_err), accum_cck->crc32_err,
			 delta_cck->crc32_err, max_cck->crc32_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "overrun_err:",
			 le32_to_cpu(cck->overrun_err),
			 accum_cck->overrun_err, delta_cck->overrun_err,
			 max_cck->overrun_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "early_overrun_err:",
			 le32_to_cpu(cck->early_overrun_err),
			 accum_cck->early_overrun_err,
			 delta_cck->early_overrun_err,
			 max_cck->early_overrun_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "crc32_good:",
			 le32_to_cpu(cck->crc32_good), accum_cck->crc32_good,
			 delta_cck->crc32_good, max_cck->crc32_good);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "false_alarm_cnt:",
			 le32_to_cpu(cck->false_alarm_cnt),
			 accum_cck->false_alarm_cnt,
			 delta_cck->false_alarm_cnt, max_cck->false_alarm_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "fina_sync_err_cnt:",
			 le32_to_cpu(cck->fina_sync_err_cnt),
			 accum_cck->fina_sync_err_cnt,
			 delta_cck->fina_sync_err_cnt,
			 max_cck->fina_sync_err_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "sfd_timeout:",
			 le32_to_cpu(cck->sfd_timeout),
			 accum_cck->sfd_timeout, delta_cck->sfd_timeout,
			 max_cck->sfd_timeout);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "fina_timeout:",
			 le32_to_cpu(cck->fina_timeout),
			 accum_cck->fina_timeout, delta_cck->fina_timeout,
			 max_cck->fina_timeout);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "unresponded_rts:",
			 le32_to_cpu(cck->unresponded_rts),
			 accum_cck->unresponded_rts, delta_cck->unresponded_rts,
			 max_cck->unresponded_rts);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "rxe_frame_lmt_ovrun:",
			 le32_to_cpu(cck->rxe_frame_limit_overrun),
			 accum_cck->rxe_frame_limit_overrun,
			 delta_cck->rxe_frame_limit_overrun,
			 max_cck->rxe_frame_limit_overrun);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "sent_ack_cnt:",
			 le32_to_cpu(cck->sent_ack_cnt),
			 accum_cck->sent_ack_cnt, delta_cck->sent_ack_cnt,
			 max_cck->sent_ack_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "sent_cts_cnt:",
			 le32_to_cpu(cck->sent_cts_cnt),
			 accum_cck->sent_cts_cnt, delta_cck->sent_cts_cnt,
			 max_cck->sent_cts_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "sent_ba_rsp_cnt:",
			 le32_to_cpu(cck->sent_ba_rsp_cnt),
			 accum_cck->sent_ba_rsp_cnt,
			 delta_cck->sent_ba_rsp_cnt,
			 max_cck->sent_ba_rsp_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "dsp_self_kill:",
			 le32_to_cpu(cck->dsp_self_kill),
			 accum_cck->dsp_self_kill, delta_cck->dsp_self_kill,
			 max_cck->dsp_self_kill);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "mh_format_err:",
			 le32_to_cpu(cck->mh_format_err),
			 accum_cck->mh_format_err, delta_cck->mh_format_err,
			 max_cck->mh_format_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "re_acq_main_rssi_sum:",
			 le32_to_cpu(cck->re_acq_main_rssi_sum),
			 accum_cck->re_acq_main_rssi_sum,
			 delta_cck->re_acq_main_rssi_sum,
			 max_cck->re_acq_main_rssi_sum);

	pos += scnprintf(buf + pos, bufsz - pos, "%-32s     current"
			 "acumulative       delta         max\n",
			 "Statistics_Rx - GENERAL:");
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "bogus_cts:",
			 le32_to_cpu(general->bogus_cts),
			 accum_general->bogus_cts, delta_general->bogus_cts,
			 max_general->bogus_cts);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n", "bogus_ack:",
			 le32_to_cpu(general->bogus_ack),
			 accum_general->bogus_ack, delta_general->bogus_ack,
			 max_general->bogus_ack);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "non_bssid_frames:",
			 le32_to_cpu(general->non_bssid_frames),
			 accum_general->non_bssid_frames,
			 delta_general->non_bssid_frames,
			 max_general->non_bssid_frames);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "filtered_frames:",
			 le32_to_cpu(general->filtered_frames),
			 accum_general->filtered_frames,
			 delta_general->filtered_frames,
			 max_general->filtered_frames);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "non_channel_beacons:",
			 le32_to_cpu(general->non_channel_beacons),
			 accum_general->non_channel_beacons,
			 delta_general->non_channel_beacons,
			 max_general->non_channel_beacons);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "channel_beacons:",
			 le32_to_cpu(general->channel_beacons),
			 accum_general->channel_beacons,
			 delta_general->channel_beacons,
			 max_general->channel_beacons);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "num_missed_bcon:",
			 le32_to_cpu(general->num_missed_bcon),
			 accum_general->num_missed_bcon,
			 delta_general->num_missed_bcon,
			 max_general->num_missed_bcon);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "adc_rx_saturation_time:",
			 le32_to_cpu(general->adc_rx_saturation_time),
			 accum_general->adc_rx_saturation_time,
			 delta_general->adc_rx_saturation_time,
			 max_general->adc_rx_saturation_time);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "ina_detect_search_tm:",
			 le32_to_cpu(general->ina_detection_search_time),
			 accum_general->ina_detection_search_time,
			 delta_general->ina_detection_search_time,
			 max_general->ina_detection_search_time);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "beacon_silence_rssi_a:",
			 le32_to_cpu(general->beacon_silence_rssi_a),
			 accum_general->beacon_silence_rssi_a,
			 delta_general->beacon_silence_rssi_a,
			 max_general->beacon_silence_rssi_a);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "beacon_silence_rssi_b:",
			 le32_to_cpu(general->beacon_silence_rssi_b),
			 accum_general->beacon_silence_rssi_b,
			 delta_general->beacon_silence_rssi_b,
			 max_general->beacon_silence_rssi_b);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "beacon_silence_rssi_c:",
			 le32_to_cpu(general->beacon_silence_rssi_c),
			 accum_general->beacon_silence_rssi_c,
			 delta_general->beacon_silence_rssi_c,
			 max_general->beacon_silence_rssi_c);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "interference_data_flag:",
			 le32_to_cpu(general->interference_data_flag),
			 accum_general->interference_data_flag,
			 delta_general->interference_data_flag,
			 max_general->interference_data_flag);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "channel_load:",
			 le32_to_cpu(general->channel_load),
			 accum_general->channel_load,
			 delta_general->channel_load,
			 max_general->channel_load);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "dsp_false_alarms:",
			 le32_to_cpu(general->dsp_false_alarms),
			 accum_general->dsp_false_alarms,
			 delta_general->dsp_false_alarms,
			 max_general->dsp_false_alarms);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "beacon_rssi_a:",
			 le32_to_cpu(general->beacon_rssi_a),
			 accum_general->beacon_rssi_a,
			 delta_general->beacon_rssi_a,
			 max_general->beacon_rssi_a);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "beacon_rssi_b:",
			 le32_to_cpu(general->beacon_rssi_b),
			 accum_general->beacon_rssi_b,
			 delta_general->beacon_rssi_b,
			 max_general->beacon_rssi_b);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "beacon_rssi_c:",
			 le32_to_cpu(general->beacon_rssi_c),
			 accum_general->beacon_rssi_c,
			 delta_general->beacon_rssi_c,
			 max_general->beacon_rssi_c);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "beacon_energy_a:",
			 le32_to_cpu(general->beacon_energy_a),
			 accum_general->beacon_energy_a,
			 delta_general->beacon_energy_a,
			 max_general->beacon_energy_a);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "beacon_energy_b:",
			 le32_to_cpu(general->beacon_energy_b),
			 accum_general->beacon_energy_b,
			 delta_general->beacon_energy_b,
			 max_general->beacon_energy_b);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "beacon_energy_c:",
			 le32_to_cpu(general->beacon_energy_c),
			 accum_general->beacon_energy_c,
			 delta_general->beacon_energy_c,
			 max_general->beacon_energy_c);

	pos += scnprintf(buf + pos, bufsz - pos, "Statistics_Rx - OFDM_HT:\n");
	pos += scnprintf(buf + pos, bufsz - pos, "%-32s     current"
			 "acumulative       delta         max\n",
			 "Statistics_Rx - OFDM_HT:");
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "plcp_err:",
			 le32_to_cpu(ht->plcp_err), accum_ht->plcp_err,
			 delta_ht->plcp_err, max_ht->plcp_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "overrun_err:",
			 le32_to_cpu(ht->overrun_err), accum_ht->overrun_err,
			 delta_ht->overrun_err, max_ht->overrun_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "early_overrun_err:",
			 le32_to_cpu(ht->early_overrun_err),
			 accum_ht->early_overrun_err,
			 delta_ht->early_overrun_err,
			 max_ht->early_overrun_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "crc32_good:",
			 le32_to_cpu(ht->crc32_good), accum_ht->crc32_good,
			 delta_ht->crc32_good, max_ht->crc32_good);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "crc32_err:",
			 le32_to_cpu(ht->crc32_err), accum_ht->crc32_err,
			 delta_ht->crc32_err, max_ht->crc32_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "mh_format_err:",
			 le32_to_cpu(ht->mh_format_err),
			 accum_ht->mh_format_err,
			 delta_ht->mh_format_err, max_ht->mh_format_err);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg_crc32_good:",
			 le32_to_cpu(ht->agg_crc32_good),
			 accum_ht->agg_crc32_good,
			 delta_ht->agg_crc32_good, max_ht->agg_crc32_good);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg_mpdu_cnt:",
			 le32_to_cpu(ht->agg_mpdu_cnt),
			 accum_ht->agg_mpdu_cnt,
			 delta_ht->agg_mpdu_cnt, max_ht->agg_mpdu_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg_cnt:",
			 le32_to_cpu(ht->agg_cnt), accum_ht->agg_cnt,
			 delta_ht->agg_cnt, max_ht->agg_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "unsupport_mcs:",
			 le32_to_cpu(ht->unsupport_mcs),
			 accum_ht->unsupport_mcs,
			 delta_ht->unsupport_mcs, max_ht->unsupport_mcs);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, pos);
	kfree(buf);
	return ret;
}

ssize_t iwl_ucode_tx_stats_read(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct iwl_priv *priv = file->private_data;
	int pos = 0;
	char *buf;
	int bufsz = (sizeof(struct statistics_tx) * 48) + 250;
	ssize_t ret;
	struct statistics_tx *tx, *accum_tx, *delta_tx, *max_tx;

	if (!iwl_is_alive(priv))
		return -EAGAIN;

	buf = kzalloc(bufsz, GFP_KERNEL);
	if (!buf) {
		IWL_ERR(priv, "Can not allocate Buffer\n");
		return -ENOMEM;
	}

	/* the statistic information display here is based on
	  * the last statistics notification from uCode
	  * might not reflect the current uCode activity
	  */
	if (priv->cfg->bt_statistics) {
		tx = &priv->_agn.statistics_bt.tx;
		accum_tx = &priv->_agn.accum_statistics_bt.tx;
		delta_tx = &priv->_agn.delta_statistics_bt.tx;
		max_tx = &priv->_agn.max_delta_bt.tx;
	} else {
		tx = &priv->_agn.statistics.tx;
		accum_tx = &priv->_agn.accum_statistics.tx;
		delta_tx = &priv->_agn.delta_statistics.tx;
		max_tx = &priv->_agn.max_delta.tx;
	}

	pos += iwl_statistics_flag(priv, buf, bufsz);
	pos += scnprintf(buf + pos, bufsz - pos,  "%-32s     current"
			 "acumulative       delta         max\n",
			 "Statistics_Tx:");
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "preamble:",
			 le32_to_cpu(tx->preamble_cnt),
			 accum_tx->preamble_cnt,
			 delta_tx->preamble_cnt, max_tx->preamble_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "rx_detected_cnt:",
			 le32_to_cpu(tx->rx_detected_cnt),
			 accum_tx->rx_detected_cnt,
			 delta_tx->rx_detected_cnt, max_tx->rx_detected_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "bt_prio_defer_cnt:",
			 le32_to_cpu(tx->bt_prio_defer_cnt),
			 accum_tx->bt_prio_defer_cnt,
			 delta_tx->bt_prio_defer_cnt,
			 max_tx->bt_prio_defer_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "bt_prio_kill_cnt:",
			 le32_to_cpu(tx->bt_prio_kill_cnt),
			 accum_tx->bt_prio_kill_cnt,
			 delta_tx->bt_prio_kill_cnt,
			 max_tx->bt_prio_kill_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "few_bytes_cnt:",
			 le32_to_cpu(tx->few_bytes_cnt),
			 accum_tx->few_bytes_cnt,
			 delta_tx->few_bytes_cnt, max_tx->few_bytes_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "cts_timeout:",
			 le32_to_cpu(tx->cts_timeout), accum_tx->cts_timeout,
			 delta_tx->cts_timeout, max_tx->cts_timeout);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "ack_timeout:",
			 le32_to_cpu(tx->ack_timeout),
			 accum_tx->ack_timeout,
			 delta_tx->ack_timeout, max_tx->ack_timeout);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "expected_ack_cnt:",
			 le32_to_cpu(tx->expected_ack_cnt),
			 accum_tx->expected_ack_cnt,
			 delta_tx->expected_ack_cnt,
			 max_tx->expected_ack_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "actual_ack_cnt:",
			 le32_to_cpu(tx->actual_ack_cnt),
			 accum_tx->actual_ack_cnt,
			 delta_tx->actual_ack_cnt,
			 max_tx->actual_ack_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "dump_msdu_cnt:",
			 le32_to_cpu(tx->dump_msdu_cnt),
			 accum_tx->dump_msdu_cnt,
			 delta_tx->dump_msdu_cnt,
			 max_tx->dump_msdu_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "abort_nxt_frame_mismatch:",
			 le32_to_cpu(tx->burst_abort_next_frame_mismatch_cnt),
			 accum_tx->burst_abort_next_frame_mismatch_cnt,
			 delta_tx->burst_abort_next_frame_mismatch_cnt,
			 max_tx->burst_abort_next_frame_mismatch_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "abort_missing_nxt_frame:",
			 le32_to_cpu(tx->burst_abort_missing_next_frame_cnt),
			 accum_tx->burst_abort_missing_next_frame_cnt,
			 delta_tx->burst_abort_missing_next_frame_cnt,
			 max_tx->burst_abort_missing_next_frame_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "cts_timeout_collision:",
			 le32_to_cpu(tx->cts_timeout_collision),
			 accum_tx->cts_timeout_collision,
			 delta_tx->cts_timeout_collision,
			 max_tx->cts_timeout_collision);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "ack_ba_timeout_collision:",
			 le32_to_cpu(tx->ack_or_ba_timeout_collision),
			 accum_tx->ack_or_ba_timeout_collision,
			 delta_tx->ack_or_ba_timeout_collision,
			 max_tx->ack_or_ba_timeout_collision);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg ba_timeout:",
			 le32_to_cpu(tx->agg.ba_timeout),
			 accum_tx->agg.ba_timeout,
			 delta_tx->agg.ba_timeout,
			 max_tx->agg.ba_timeout);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg ba_resched_frames:",
			 le32_to_cpu(tx->agg.ba_reschedule_frames),
			 accum_tx->agg.ba_reschedule_frames,
			 delta_tx->agg.ba_reschedule_frames,
			 max_tx->agg.ba_reschedule_frames);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg scd_query_agg_frame:",
			 le32_to_cpu(tx->agg.scd_query_agg_frame_cnt),
			 accum_tx->agg.scd_query_agg_frame_cnt,
			 delta_tx->agg.scd_query_agg_frame_cnt,
			 max_tx->agg.scd_query_agg_frame_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg scd_query_no_agg:",
			 le32_to_cpu(tx->agg.scd_query_no_agg),
			 accum_tx->agg.scd_query_no_agg,
			 delta_tx->agg.scd_query_no_agg,
			 max_tx->agg.scd_query_no_agg);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg scd_query_agg:",
			 le32_to_cpu(tx->agg.scd_query_agg),
			 accum_tx->agg.scd_query_agg,
			 delta_tx->agg.scd_query_agg,
			 max_tx->agg.scd_query_agg);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg scd_query_mismatch:",
			 le32_to_cpu(tx->agg.scd_query_mismatch),
			 accum_tx->agg.scd_query_mismatch,
			 delta_tx->agg.scd_query_mismatch,
			 max_tx->agg.scd_query_mismatch);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg frame_not_ready:",
			 le32_to_cpu(tx->agg.frame_not_ready),
			 accum_tx->agg.frame_not_ready,
			 delta_tx->agg.frame_not_ready,
			 max_tx->agg.frame_not_ready);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg underrun:",
			 le32_to_cpu(tx->agg.underrun),
			 accum_tx->agg.underrun,
			 delta_tx->agg.underrun, max_tx->agg.underrun);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg bt_prio_kill:",
			 le32_to_cpu(tx->agg.bt_prio_kill),
			 accum_tx->agg.bt_prio_kill,
			 delta_tx->agg.bt_prio_kill,
			 max_tx->agg.bt_prio_kill);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "agg rx_ba_rsp_cnt:",
			 le32_to_cpu(tx->agg.rx_ba_rsp_cnt),
			 accum_tx->agg.rx_ba_rsp_cnt,
			 delta_tx->agg.rx_ba_rsp_cnt,
			 max_tx->agg.rx_ba_rsp_cnt);

	if (tx->tx_power.ant_a || tx->tx_power.ant_b || tx->tx_power.ant_c) {
		pos += scnprintf(buf + pos, bufsz - pos,
			"tx power: (1/2 dB step)\n");
		if ((priv->cfg->valid_tx_ant & ANT_A) && tx->tx_power.ant_a)
			pos += scnprintf(buf + pos, bufsz - pos,
					"\tantenna A: 0x%X\n",
					tx->tx_power.ant_a);
		if ((priv->cfg->valid_tx_ant & ANT_B) && tx->tx_power.ant_b)
			pos += scnprintf(buf + pos, bufsz - pos,
					"\tantenna B: 0x%X\n",
					tx->tx_power.ant_b);
		if ((priv->cfg->valid_tx_ant & ANT_C) && tx->tx_power.ant_c)
			pos += scnprintf(buf + pos, bufsz - pos,
					"\tantenna C: 0x%X\n",
					tx->tx_power.ant_c);
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, pos);
	kfree(buf);
	return ret;
}

ssize_t iwl_ucode_general_stats_read(struct file *file, char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct iwl_priv *priv = file->private_data;
	int pos = 0;
	char *buf;
	int bufsz = sizeof(struct statistics_general) * 10 + 300;
	ssize_t ret;
	struct statistics_general_common *general, *accum_general;
	struct statistics_general_common *delta_general, *max_general;
	struct statistics_dbg *dbg, *accum_dbg, *delta_dbg, *max_dbg;
	struct statistics_div *div, *accum_div, *delta_div, *max_div;

	if (!iwl_is_alive(priv))
		return -EAGAIN;

	buf = kzalloc(bufsz, GFP_KERNEL);
	if (!buf) {
		IWL_ERR(priv, "Can not allocate Buffer\n");
		return -ENOMEM;
	}

	/* the statistic information display here is based on
	  * the last statistics notification from uCode
	  * might not reflect the current uCode activity
	  */
	if (priv->cfg->bt_statistics) {
		general = &priv->_agn.statistics_bt.general.common;
		dbg = &priv->_agn.statistics_bt.general.common.dbg;
		div = &priv->_agn.statistics_bt.general.common.div;
		accum_general = &priv->_agn.accum_statistics_bt.general.common;
		accum_dbg = &priv->_agn.accum_statistics_bt.general.common.dbg;
		accum_div = &priv->_agn.accum_statistics_bt.general.common.div;
		delta_general = &priv->_agn.delta_statistics_bt.general.common;
		max_general = &priv->_agn.max_delta_bt.general.common;
		delta_dbg = &priv->_agn.delta_statistics_bt.general.common.dbg;
		max_dbg = &priv->_agn.max_delta_bt.general.common.dbg;
		delta_div = &priv->_agn.delta_statistics_bt.general.common.div;
		max_div = &priv->_agn.max_delta_bt.general.common.div;
	} else {
		general = &priv->_agn.statistics.general.common;
		dbg = &priv->_agn.statistics.general.common.dbg;
		div = &priv->_agn.statistics.general.common.div;
		accum_general = &priv->_agn.accum_statistics.general.common;
		accum_dbg = &priv->_agn.accum_statistics.general.common.dbg;
		accum_div = &priv->_agn.accum_statistics.general.common.div;
		delta_general = &priv->_agn.delta_statistics.general.common;
		max_general = &priv->_agn.max_delta.general.common;
		delta_dbg = &priv->_agn.delta_statistics.general.common.dbg;
		max_dbg = &priv->_agn.max_delta.general.common.dbg;
		delta_div = &priv->_agn.delta_statistics.general.common.div;
		max_div = &priv->_agn.max_delta.general.common.div;
	}

	pos += iwl_statistics_flag(priv, buf, bufsz);
	pos += scnprintf(buf + pos, bufsz - pos, "%-32s     current"
			 "acumulative       delta         max\n",
			 "Statistics_General:");
	pos += scnprintf(buf + pos, bufsz - pos, "  %-30s %10u\n",
			 "temperature:",
			 le32_to_cpu(general->temperature));
	pos += scnprintf(buf + pos, bufsz - pos, "  %-30s %10u\n",
			 "temperature_m:",
			 le32_to_cpu(general->temperature_m));
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "burst_check:",
			 le32_to_cpu(dbg->burst_check),
			 accum_dbg->burst_check,
			 delta_dbg->burst_check, max_dbg->burst_check);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "burst_count:",
			 le32_to_cpu(dbg->burst_count),
			 accum_dbg->burst_count,
			 delta_dbg->burst_count, max_dbg->burst_count);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "wait_for_silence_timeout_count:",
			 le32_to_cpu(dbg->wait_for_silence_timeout_cnt),
			 accum_dbg->wait_for_silence_timeout_cnt,
			 delta_dbg->wait_for_silence_timeout_cnt,
			 max_dbg->wait_for_silence_timeout_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "sleep_time:",
			 le32_to_cpu(general->sleep_time),
			 accum_general->sleep_time,
			 delta_general->sleep_time, max_general->sleep_time);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "slots_out:",
			 le32_to_cpu(general->slots_out),
			 accum_general->slots_out,
			 delta_general->slots_out, max_general->slots_out);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "slots_idle:",
			 le32_to_cpu(general->slots_idle),
			 accum_general->slots_idle,
			 delta_general->slots_idle, max_general->slots_idle);
	pos += scnprintf(buf + pos, bufsz - pos, "ttl_timestamp:\t\t\t%u\n",
			 le32_to_cpu(general->ttl_timestamp));
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "tx_on_a:",
			 le32_to_cpu(div->tx_on_a), accum_div->tx_on_a,
			 delta_div->tx_on_a, max_div->tx_on_a);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "tx_on_b:",
			 le32_to_cpu(div->tx_on_b), accum_div->tx_on_b,
			 delta_div->tx_on_b, max_div->tx_on_b);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "exec_time:",
			 le32_to_cpu(div->exec_time), accum_div->exec_time,
			 delta_div->exec_time, max_div->exec_time);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "probe_time:",
			 le32_to_cpu(div->probe_time), accum_div->probe_time,
			 delta_div->probe_time, max_div->probe_time);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "rx_enable_counter:",
			 le32_to_cpu(general->rx_enable_counter),
			 accum_general->rx_enable_counter,
			 delta_general->rx_enable_counter,
			 max_general->rx_enable_counter);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "  %-30s %10u  %10u  %10u  %10u\n",
			 "num_of_sos_states:",
			 le32_to_cpu(general->num_of_sos_states),
			 accum_general->num_of_sos_states,
			 delta_general->num_of_sos_states,
			 max_general->num_of_sos_states);
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, pos);
	kfree(buf);
	return ret;
}

ssize_t iwl_ucode_bt_stats_read(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct iwl_priv *priv = (struct iwl_priv *)file->private_data;
	int pos = 0;
	char *buf;
	int bufsz = (sizeof(struct statistics_bt_activity) * 24) + 200;
	ssize_t ret;
	struct statistics_bt_activity *bt, *accum_bt;

	if (!iwl_is_alive(priv))
		return -EAGAIN;

	/* make request to uCode to retrieve statistics information */
	mutex_lock(&priv->mutex);
	ret = iwl_send_statistics_request(priv, CMD_SYNC, false);
	mutex_unlock(&priv->mutex);

	if (ret) {
		IWL_ERR(priv,
			"Error sending statistics request: %zd\n", ret);
		return -EAGAIN;
	}
	buf = kzalloc(bufsz, GFP_KERNEL);
	if (!buf) {
		IWL_ERR(priv, "Can not allocate Buffer\n");
		return -ENOMEM;
	}

	/*
	 * the statistic information display here is based on
	 * the last statistics notification from uCode
	 * might not reflect the current uCode activity
	 */
	bt = &priv->_agn.statistics_bt.general.activity;
	accum_bt = &priv->_agn.accum_statistics_bt.general.activity;

	pos += iwl_statistics_flag(priv, buf, bufsz);
	pos += scnprintf(buf + pos, bufsz - pos, "Statistics_BT:\n");
	pos += scnprintf(buf + pos, bufsz - pos,
			"\t\t\tcurrent\t\t\taccumulative\n");
	pos += scnprintf(buf + pos, bufsz - pos,
			 "hi_priority_tx_req_cnt:\t\t%u\t\t\t%u\n",
			 le32_to_cpu(bt->hi_priority_tx_req_cnt),
			 accum_bt->hi_priority_tx_req_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "hi_priority_tx_denied_cnt:\t%u\t\t\t%u\n",
			 le32_to_cpu(bt->hi_priority_tx_denied_cnt),
			 accum_bt->hi_priority_tx_denied_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "lo_priority_tx_req_cnt:\t\t%u\t\t\t%u\n",
			 le32_to_cpu(bt->lo_priority_tx_req_cnt),
			 accum_bt->lo_priority_tx_req_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "lo_priority_tx_denied_cnt:\t%u\t\t\t%u\n",
			 le32_to_cpu(bt->lo_priority_tx_denied_cnt),
			 accum_bt->lo_priority_tx_denied_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "hi_priority_rx_req_cnt:\t\t%u\t\t\t%u\n",
			 le32_to_cpu(bt->hi_priority_rx_req_cnt),
			 accum_bt->hi_priority_rx_req_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "hi_priority_rx_denied_cnt:\t%u\t\t\t%u\n",
			 le32_to_cpu(bt->hi_priority_rx_denied_cnt),
			 accum_bt->hi_priority_rx_denied_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "lo_priority_rx_req_cnt:\t\t%u\t\t\t%u\n",
			 le32_to_cpu(bt->lo_priority_rx_req_cnt),
			 accum_bt->lo_priority_rx_req_cnt);
	pos += scnprintf(buf + pos, bufsz - pos,
			 "lo_priority_rx_denied_cnt:\t%u\t\t\t%u\n",
			 le32_to_cpu(bt->lo_priority_rx_denied_cnt),
			 accum_bt->lo_priority_rx_denied_cnt);

	pos += scnprintf(buf + pos, bufsz - pos,
			 "(rx)num_bt_kills:\t\t%u\t\t\t%u\n",
			 le32_to_cpu(priv->_agn.statistics_bt.rx.
				general.num_bt_kills),
			 priv->_agn.accum_statistics_bt.rx.
				general.num_bt_kills);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, pos);
	kfree(buf);
	return ret;
}
