/*
 * HT handling
 *
 * Copyright 2003, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright 2002-2005, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
 * Copyright 2006-2007	Jiri Benc <jbenc@suse.cz>
 * Copyright 2007, Michael Wu <flamingice@sourmilk.net>
 * Copyright 2007-2010, Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/ieee80211.h>
#include <net/mac80211.h>
#include "ieee80211_i.h"
#include "rate.h"

void ieee80211_ht_cap_ie_to_sta_ht_cap(struct ieee80211_supported_band *sband,
				       struct ieee80211_ht_cap *ht_cap_ie,
				       struct ieee80211_sta_ht_cap *ht_cap)
{
	u8 ampdu_info, tx_mcs_set_cap;
	int i, max_tx_streams;

	BUG_ON(!ht_cap);

	memset(ht_cap, 0, sizeof(*ht_cap));

	if (!ht_cap_ie || !sband->ht_cap.ht_supported)
		return;

	ht_cap->ht_supported = true;

	/*
	 * The bits listed in this expression should be
	 * the same for the peer and us, if the station
	 * advertises more then we can't use those thus
	 * we mask them out.
	 */
	ht_cap->cap = le16_to_cpu(ht_cap_ie->cap_info) &
		(sband->ht_cap.cap |
		 ~(IEEE80211_HT_CAP_LDPC_CODING |
		   IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
		   IEEE80211_HT_CAP_GRN_FLD |
		   IEEE80211_HT_CAP_SGI_20 |
		   IEEE80211_HT_CAP_SGI_40 |
		   IEEE80211_HT_CAP_DSSSCCK40));
	/*
	 * The STBC bits are asymmetric -- if we don't have
	 * TX then mask out the peer's RX and vice versa.
	 */
	if (!(sband->ht_cap.cap & IEEE80211_HT_CAP_TX_STBC))
		ht_cap->cap &= ~IEEE80211_HT_CAP_RX_STBC;
	if (!(sband->ht_cap.cap & IEEE80211_HT_CAP_RX_STBC))
		ht_cap->cap &= ~IEEE80211_HT_CAP_TX_STBC;

	ampdu_info = ht_cap_ie->ampdu_params_info;
	ht_cap->ampdu_factor =
		ampdu_info & IEEE80211_HT_AMPDU_PARM_FACTOR;
	ht_cap->ampdu_density =
		(ampdu_info & IEEE80211_HT_AMPDU_PARM_DENSITY) >> 2;

	/* own MCS TX capabilities */
	tx_mcs_set_cap = sband->ht_cap.mcs.tx_params;

	/* can we TX with MCS rates? */
	if (!(tx_mcs_set_cap & IEEE80211_HT_MCS_TX_DEFINED))
		return;

	/* Counting from 0, therefore +1 */
	if (tx_mcs_set_cap & IEEE80211_HT_MCS_TX_RX_DIFF)
		max_tx_streams =
			((tx_mcs_set_cap & IEEE80211_HT_MCS_TX_MAX_STREAMS_MASK)
				>> IEEE80211_HT_MCS_TX_MAX_STREAMS_SHIFT) + 1;
	else
		max_tx_streams = IEEE80211_HT_MCS_TX_MAX_STREAMS;

	/*
	 * 802.11n D5.0 20.3.5 / 20.6 says:
	 * - indices 0 to 7 and 32 are single spatial stream
	 * - 8 to 31 are multiple spatial streams using equal modulation
	 *   [8..15 for two streams, 16..23 for three and 24..31 for four]
	 * - remainder are multiple spatial streams using unequal modulation
	 */
	for (i = 0; i < max_tx_streams; i++)
		ht_cap->mcs.rx_mask[i] =
			sband->ht_cap.mcs.rx_mask[i] & ht_cap_ie->mcs.rx_mask[i];

	if (tx_mcs_set_cap & IEEE80211_HT_MCS_TX_UNEQUAL_MODULATION)
		for (i = IEEE80211_HT_MCS_UNEQUAL_MODULATION_START_BYTE;
		     i < IEEE80211_HT_MCS_MASK_LEN; i++)
			ht_cap->mcs.rx_mask[i] =
				sband->ht_cap.mcs.rx_mask[i] &
					ht_cap_ie->mcs.rx_mask[i];

	/* handle MCS rate 32 too */
	if (sband->ht_cap.mcs.rx_mask[32/8] & ht_cap_ie->mcs.rx_mask[32/8] & 1)
		ht_cap->mcs.rx_mask[32/8] |= 1;
}

void ieee80211_sta_tear_down_BA_sessions(struct sta_info *sta)
{
	int i;

	cancel_work_sync(&sta->ampdu_mlme.work);

	for (i = 0; i <  STA_TID_NUM; i++) {
		__ieee80211_stop_tx_ba_session(sta, i, WLAN_BACK_INITIATOR);
		__ieee80211_stop_rx_ba_session(sta, i, WLAN_BACK_RECIPIENT,
					       WLAN_REASON_QSTA_LEAVE_QBSS);
	}
}

void ieee80211_ba_session_work(struct work_struct *work)
{
	struct sta_info *sta =
		container_of(work, struct sta_info, ampdu_mlme.work);
	struct tid_ampdu_tx *tid_tx;
	int tid;

	/*
	 * When this flag is set, new sessions should be
	 * blocked, and existing sessions will be torn
	 * down by the code that set the flag, so this
	 * need not run.
	 */
	if (test_sta_flags(sta, WLAN_STA_BLOCK_BA))
		return;

	mutex_lock(&sta->ampdu_mlme.mtx);
	for (tid = 0; tid < STA_TID_NUM; tid++) {
		if (test_and_clear_bit(tid, sta->ampdu_mlme.tid_rx_timer_expired))
			___ieee80211_stop_rx_ba_session(
				sta, tid, WLAN_BACK_RECIPIENT,
				WLAN_REASON_QSTA_TIMEOUT);

		tid_tx = sta->ampdu_mlme.tid_tx[tid];
		if (!tid_tx)
			continue;

		if (test_bit(HT_AGG_STATE_WANT_START, &tid_tx->state))
			ieee80211_tx_ba_session_handle_start(sta, tid);
		else if (test_and_clear_bit(HT_AGG_STATE_WANT_STOP,
					    &tid_tx->state))
			___ieee80211_stop_tx_ba_session(sta, tid,
							WLAN_BACK_INITIATOR);
	}
	mutex_unlock(&sta->ampdu_mlme.mtx);
}

void ieee80211_send_delba(struct ieee80211_sub_if_data *sdata,
			  const u8 *da, u16 tid,
			  u16 initiator, u16 reason_code)
{
	struct ieee80211_local *local = sdata->local;
	struct sk_buff *skb;
	struct ieee80211_mgmt *mgmt;
	u16 params;

	skb = dev_alloc_skb(sizeof(*mgmt) + local->hw.extra_tx_headroom);

	if (!skb) {
		printk(KERN_ERR "%s: failed to allocate buffer "
					"for delba frame\n", sdata->name);
		return;
	}

	skb_reserve(skb, local->hw.extra_tx_headroom);
	mgmt = (struct ieee80211_mgmt *) skb_put(skb, 24);
	memset(mgmt, 0, 24);
	memcpy(mgmt->da, da, ETH_ALEN);
	memcpy(mgmt->sa, sdata->vif.addr, ETH_ALEN);
	if (sdata->vif.type == NL80211_IFTYPE_AP ||
	    sdata->vif.type == NL80211_IFTYPE_AP_VLAN)
		memcpy(mgmt->bssid, sdata->vif.addr, ETH_ALEN);
	else if (sdata->vif.type == NL80211_IFTYPE_STATION)
		memcpy(mgmt->bssid, sdata->u.mgd.bssid, ETH_ALEN);

	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
					  IEEE80211_STYPE_ACTION);

	skb_put(skb, 1 + sizeof(mgmt->u.action.u.delba));

	mgmt->u.action.category = WLAN_CATEGORY_BACK;
	mgmt->u.action.u.delba.action_code = WLAN_ACTION_DELBA;
	params = (u16)(initiator << 11); 	/* bit 11 initiator */
	params |= (u16)(tid << 12); 		/* bit 15:12 TID number */

	mgmt->u.action.u.delba.params = cpu_to_le16(params);
	mgmt->u.action.u.delba.reason_code = cpu_to_le16(reason_code);

	ieee80211_tx_skb(sdata, skb);
}

void ieee80211_process_delba(struct ieee80211_sub_if_data *sdata,
			     struct sta_info *sta,
			     struct ieee80211_mgmt *mgmt, size_t len)
{
	u16 tid, params;
	u16 initiator;

	params = le16_to_cpu(mgmt->u.action.u.delba.params);
	tid = (params & IEEE80211_DELBA_PARAM_TID_MASK) >> 12;
	initiator = (params & IEEE80211_DELBA_PARAM_INITIATOR_MASK) >> 11;

#ifdef CONFIG_MAC80211_HT_DEBUG
	if (net_ratelimit())
		printk(KERN_DEBUG "delba from %pM (%s) tid %d reason code %d\n",
			mgmt->sa, initiator ? "initiator" : "recipient", tid,
			le16_to_cpu(mgmt->u.action.u.delba.reason_code));
#endif /* CONFIG_MAC80211_HT_DEBUG */

	if (initiator == WLAN_BACK_INITIATOR)
		__ieee80211_stop_rx_ba_session(sta, tid, WLAN_BACK_INITIATOR, 0);
	else
		__ieee80211_stop_tx_ba_session(sta, tid, WLAN_BACK_RECIPIENT);
}

int ieee80211_send_smps_action(struct ieee80211_sub_if_data *sdata,
			       enum ieee80211_smps_mode smps, const u8 *da,
			       const u8 *bssid)
{
	struct ieee80211_local *local = sdata->local;
	struct sk_buff *skb;
	struct ieee80211_mgmt *action_frame;

	/* 27 = header + category + action + smps mode */
	skb = dev_alloc_skb(27 + local->hw.extra_tx_headroom);
	if (!skb)
		return -ENOMEM;

	skb_reserve(skb, local->hw.extra_tx_headroom);
	action_frame = (void *)skb_put(skb, 27);
	memcpy(action_frame->da, da, ETH_ALEN);
	memcpy(action_frame->sa, sdata->dev->dev_addr, ETH_ALEN);
	memcpy(action_frame->bssid, bssid, ETH_ALEN);
	action_frame->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
						  IEEE80211_STYPE_ACTION);
	action_frame->u.action.category = WLAN_CATEGORY_HT;
	action_frame->u.action.u.ht_smps.action = WLAN_HT_ACTION_SMPS;
	switch (smps) {
	case IEEE80211_SMPS_AUTOMATIC:
	case IEEE80211_SMPS_NUM_MODES:
		WARN_ON(1);
	case IEEE80211_SMPS_OFF:
		action_frame->u.action.u.ht_smps.smps_control =
				WLAN_HT_SMPS_CONTROL_DISABLED;
		break;
	case IEEE80211_SMPS_STATIC:
		action_frame->u.action.u.ht_smps.smps_control =
				WLAN_HT_SMPS_CONTROL_STATIC;
		break;
	case IEEE80211_SMPS_DYNAMIC:
		action_frame->u.action.u.ht_smps.smps_control =
				WLAN_HT_SMPS_CONTROL_DYNAMIC;
		break;
	}

	/* we'll do more on status of this frame */
	IEEE80211_SKB_CB(skb)->flags |= IEEE80211_TX_CTL_REQ_TX_STATUS;
	ieee80211_tx_skb(sdata, skb);

	return 0;
}
