/*
 * Copyright 2002-2005, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
 * Copyright 2006-2007	Jiri Benc <jbenc@suse.cz>
 * Copyright 2008-2010	Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <net/mac80211.h>
#include "ieee80211_i.h"
#include "rate.h"
#include "mesh.h"
#include "led.h"


void ieee80211_tx_status_irqsafe(struct ieee80211_hw *hw,
				 struct sk_buff *skb)
{
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	int tmp;

	skb->pkt_type = IEEE80211_TX_STATUS_MSG;
	skb_queue_tail(info->flags & IEEE80211_TX_CTL_REQ_TX_STATUS ?
		       &local->skb_queue : &local->skb_queue_unreliable, skb);
	tmp = skb_queue_len(&local->skb_queue) +
		skb_queue_len(&local->skb_queue_unreliable);
	while (tmp > IEEE80211_IRQSAFE_QUEUE_LIMIT &&
	       (skb = skb_dequeue(&local->skb_queue_unreliable))) {
		dev_kfree_skb_irq(skb);
		tmp--;
		I802_DEBUG_INC(local->tx_status_drop);
	}
	tasklet_schedule(&local->tasklet);
}
EXPORT_SYMBOL(ieee80211_tx_status_irqsafe);

static void ieee80211_handle_filtered_frame(struct ieee80211_local *local,
					    struct sta_info *sta,
					    struct sk_buff *skb)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);

	/*
	 * This skb 'survived' a round-trip through the driver, and
	 * hopefully the driver didn't mangle it too badly. However,
	 * we can definitely not rely on the control information
	 * being correct. Clear it so we don't get junk there, and
	 * indicate that it needs new processing, but must not be
	 * modified/encrypted again.
	 */
	memset(&info->control, 0, sizeof(info->control));

	info->control.jiffies = jiffies;
	info->control.vif = &sta->sdata->vif;
	info->flags |= IEEE80211_TX_INTFL_NEED_TXPROCESSING |
		       IEEE80211_TX_INTFL_RETRANSMISSION;
	info->flags &= ~IEEE80211_TX_TEMPORARY_FLAGS;

	sta->tx_filtered_count++;

	/*
	 * Clear the TX filter mask for this STA when sending the next
	 * packet. If the STA went to power save mode, this will happen
	 * when it wakes up for the next time.
	 */
	set_sta_flags(sta, WLAN_STA_CLEAR_PS_FILT);

	/*
	 * This code races in the following way:
	 *
	 *  (1) STA sends frame indicating it will go to sleep and does so
	 *  (2) hardware/firmware adds STA to filter list, passes frame up
	 *  (3) hardware/firmware processes TX fifo and suppresses a frame
	 *  (4) we get TX status before having processed the frame and
	 *	knowing that the STA has gone to sleep.
	 *
	 * This is actually quite unlikely even when both those events are
	 * processed from interrupts coming in quickly after one another or
	 * even at the same time because we queue both TX status events and
	 * RX frames to be processed by a tasklet and process them in the
	 * same order that they were received or TX status last. Hence, there
	 * is no race as long as the frame RX is processed before the next TX
	 * status, which drivers can ensure, see below.
	 *
	 * Note that this can only happen if the hardware or firmware can
	 * actually add STAs to the filter list, if this is done by the
	 * driver in response to set_tim() (which will only reduce the race
	 * this whole filtering tries to solve, not completely solve it)
	 * this situation cannot happen.
	 *
	 * To completely solve this race drivers need to make sure that they
	 *  (a) don't mix the irq-safe/not irq-safe TX status/RX processing
	 *	functions and
	 *  (b) always process RX events before TX status events if ordering
	 *      can be unknown, for example with different interrupt status
	 *	bits.
	 */
	if (test_sta_flags(sta, WLAN_STA_PS_STA) &&
	    skb_queue_len(&sta->tx_filtered) < STA_MAX_TX_BUFFER) {
		skb_queue_tail(&sta->tx_filtered, skb);
		return;
	}

	if (!test_sta_flags(sta, WLAN_STA_PS_STA) &&
	    !(info->flags & IEEE80211_TX_INTFL_RETRIED)) {
		/* Software retry the packet once */
		info->flags |= IEEE80211_TX_INTFL_RETRIED;
		ieee80211_add_pending_skb(local, skb);
		return;
	}

#ifdef CONFIG_MAC80211_VERBOSE_DEBUG
	if (net_ratelimit())
		printk(KERN_DEBUG "%s: dropped TX filtered frame, "
		       "queue_len=%d PS=%d @%lu\n",
		       wiphy_name(local->hw.wiphy),
		       skb_queue_len(&sta->tx_filtered),
		       !!test_sta_flags(sta, WLAN_STA_PS_STA), jiffies);
#endif
	dev_kfree_skb(skb);
}

static void ieee80211_frame_acked(struct sta_info *sta, struct sk_buff *skb)
{
	struct ieee80211_mgmt *mgmt = (void *) skb->data;
	struct ieee80211_local *local = sta->local;
	struct ieee80211_sub_if_data *sdata = sta->sdata;

	if (ieee80211_is_action(mgmt->frame_control) &&
	    sdata->vif.type == NL80211_IFTYPE_STATION &&
	    mgmt->u.action.category == WLAN_CATEGORY_HT &&
	    mgmt->u.action.u.ht_smps.action == WLAN_HT_ACTION_SMPS) {
		/*
		 * This update looks racy, but isn't -- if we come
		 * here we've definitely got a station that we're
		 * talking to, and on a managed interface that can
		 * only be the AP. And the only other place updating
		 * this variable is before we're associated.
		 */
		switch (mgmt->u.action.u.ht_smps.smps_control) {
		case WLAN_HT_SMPS_CONTROL_DYNAMIC:
			sta->sdata->u.mgd.ap_smps = IEEE80211_SMPS_DYNAMIC;
			break;
		case WLAN_HT_SMPS_CONTROL_STATIC:
			sta->sdata->u.mgd.ap_smps = IEEE80211_SMPS_STATIC;
			break;
		case WLAN_HT_SMPS_CONTROL_DISABLED:
		default: /* shouldn't happen since we don't send that */
			sta->sdata->u.mgd.ap_smps = IEEE80211_SMPS_OFF;
			break;
		}

		ieee80211_queue_work(&local->hw, &local->recalc_smps);
	}
}

void ieee80211_tx_status(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	struct sk_buff *skb2;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	u16 frag, type;
	__le16 fc;
	struct ieee80211_supported_band *sband;
	struct ieee80211_tx_status_rtap_hdr *rthdr;
	struct ieee80211_sub_if_data *sdata;
	struct net_device *prev_dev = NULL;
	struct sta_info *sta, *tmp;
	int retry_count = -1, i;
	int rates_idx = -1;
	bool send_to_cooked;

	for (i = 0; i < IEEE80211_TX_MAX_RATES; i++) {
		/* the HW cannot have attempted that rate */
		if (i >= hw->max_rates) {
			info->status.rates[i].idx = -1;
			info->status.rates[i].count = 0;
		} else if (info->status.rates[i].idx >= 0) {
			rates_idx = i;
		}

		retry_count += info->status.rates[i].count;
	}
	if (retry_count < 0)
		retry_count = 0;

	rcu_read_lock();

	sband = local->hw.wiphy->bands[info->band];
	fc = hdr->frame_control;

	for_each_sta_info(local, hdr->addr1, sta, tmp) {
		/* skip wrong virtual interface */
		if (memcmp(hdr->addr2, sta->sdata->vif.addr, ETH_ALEN))
			continue;

		if (!(info->flags & IEEE80211_TX_STAT_ACK) &&
		    test_sta_flags(sta, WLAN_STA_PS_STA)) {
			/*
			 * The STA is in power save mode, so assume
			 * that this TX packet failed because of that.
			 */
			ieee80211_handle_filtered_frame(local, sta, skb);
			rcu_read_unlock();
			return;
		}

		if ((local->hw.flags & IEEE80211_HW_HAS_RATE_CONTROL) &&
		    (rates_idx != -1))
			sta->last_tx_rate = info->status.rates[rates_idx];

		if ((info->flags & IEEE80211_TX_STAT_AMPDU_NO_BACK) &&
		    (ieee80211_is_data_qos(fc))) {
			u16 tid, ssn;
			u8 *qc;

			qc = ieee80211_get_qos_ctl(hdr);
			tid = qc[0] & 0xf;
			ssn = ((le16_to_cpu(hdr->seq_ctrl) + 0x10)
						& IEEE80211_SCTL_SEQ);
			ieee80211_send_bar(sta->sdata, hdr->addr1,
					   tid, ssn);
		}

		if (info->flags & IEEE80211_TX_STAT_TX_FILTERED) {
			ieee80211_handle_filtered_frame(local, sta, skb);
			rcu_read_unlock();
			return;
		} else {
			if (!(info->flags & IEEE80211_TX_STAT_ACK))
				sta->tx_retry_failed++;
			sta->tx_retry_count += retry_count;
		}

		rate_control_tx_status(local, sband, sta, skb);
		if (ieee80211_vif_is_mesh(&sta->sdata->vif))
			ieee80211s_update_metric(local, sta, skb);

		if (!(info->flags & IEEE80211_TX_CTL_INJECTED) &&
		    (info->flags & IEEE80211_TX_STAT_ACK))
			ieee80211_frame_acked(sta, skb);
	}

	rcu_read_unlock();

	ieee80211_led_tx(local, 0);

	/* SNMP counters
	 * Fragments are passed to low-level drivers as separate skbs, so these
	 * are actually fragments, not frames. Update frame counters only for
	 * the first fragment of the frame. */

	frag = le16_to_cpu(hdr->seq_ctrl) & IEEE80211_SCTL_FRAG;
	type = le16_to_cpu(hdr->frame_control) & IEEE80211_FCTL_FTYPE;

	if (info->flags & IEEE80211_TX_STAT_ACK) {
		if (frag == 0) {
			local->dot11TransmittedFrameCount++;
			if (is_multicast_ether_addr(hdr->addr1))
				local->dot11MulticastTransmittedFrameCount++;
			if (retry_count > 0)
				local->dot11RetryCount++;
			if (retry_count > 1)
				local->dot11MultipleRetryCount++;
		}

		/* This counter shall be incremented for an acknowledged MPDU
		 * with an individual address in the address 1 field or an MPDU
		 * with a multicast address in the address 1 field of type Data
		 * or Management. */
		if (!is_multicast_ether_addr(hdr->addr1) ||
		    type == IEEE80211_FTYPE_DATA ||
		    type == IEEE80211_FTYPE_MGMT)
			local->dot11TransmittedFragmentCount++;
	} else {
		if (frag == 0)
			local->dot11FailedCount++;
	}

	if (ieee80211_is_nullfunc(fc) && ieee80211_has_pm(fc) &&
	    (local->hw.flags & IEEE80211_HW_REPORTS_TX_ACK_STATUS) &&
	    !(info->flags & IEEE80211_TX_CTL_INJECTED) &&
	    local->ps_sdata && !(local->scanning)) {
		if (info->flags & IEEE80211_TX_STAT_ACK) {
			local->ps_sdata->u.mgd.flags |=
					IEEE80211_STA_NULLFUNC_ACKED;
			ieee80211_queue_work(&local->hw,
					&local->dynamic_ps_enable_work);
		} else
			mod_timer(&local->dynamic_ps_timer, jiffies +
					msecs_to_jiffies(10));
	}

	if (info->flags & IEEE80211_TX_INTFL_NL80211_FRAME_TX)
		cfg80211_action_tx_status(
			skb->dev, (unsigned long) skb, skb->data, skb->len,
			!!(info->flags & IEEE80211_TX_STAT_ACK), GFP_ATOMIC);

	/* this was a transmitted frame, but now we want to reuse it */
	skb_orphan(skb);

	/* Need to make a copy before skb->cb gets cleared */
	send_to_cooked = !!(info->flags & IEEE80211_TX_CTL_INJECTED) ||
			(type != IEEE80211_FTYPE_DATA);

	/*
	 * This is a bit racy but we can avoid a lot of work
	 * with this test...
	 */
	if (!local->monitors && (!send_to_cooked || !local->cooked_mntrs)) {
		dev_kfree_skb(skb);
		return;
	}

	/* send frame to monitor interfaces now */

	if (skb_headroom(skb) < sizeof(*rthdr)) {
		printk(KERN_ERR "ieee80211_tx_status: headroom too small\n");
		dev_kfree_skb(skb);
		return;
	}

	rthdr = (struct ieee80211_tx_status_rtap_hdr *)
				skb_push(skb, sizeof(*rthdr));

	memset(rthdr, 0, sizeof(*rthdr));
	rthdr->hdr.it_len = cpu_to_le16(sizeof(*rthdr));
	rthdr->hdr.it_present =
		cpu_to_le32((1 << IEEE80211_RADIOTAP_TX_FLAGS) |
			    (1 << IEEE80211_RADIOTAP_DATA_RETRIES) |
			    (1 << IEEE80211_RADIOTAP_RATE));

	if (!(info->flags & IEEE80211_TX_STAT_ACK) &&
	    !is_multicast_ether_addr(hdr->addr1))
		rthdr->tx_flags |= cpu_to_le16(IEEE80211_RADIOTAP_F_TX_FAIL);

	if ((info->status.rates[0].flags & IEEE80211_TX_RC_USE_RTS_CTS) ||
	    (info->status.rates[0].flags & IEEE80211_TX_RC_USE_CTS_PROTECT))
		rthdr->tx_flags |= cpu_to_le16(IEEE80211_RADIOTAP_F_TX_CTS);
	else if (info->status.rates[0].flags & IEEE80211_TX_RC_USE_RTS_CTS)
		rthdr->tx_flags |= cpu_to_le16(IEEE80211_RADIOTAP_F_TX_RTS);
	if (info->status.rates[0].idx >= 0 &&
	    !(info->status.rates[0].flags & IEEE80211_TX_RC_MCS))
		rthdr->rate = sband->bitrates[
				info->status.rates[0].idx].bitrate / 5;

	/* for now report the total retry_count */
	rthdr->data_retries = retry_count;

	skb_set_mac_header(skb, 0);
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = htons(ETH_P_802_2);
	memset(skb->cb, 0, sizeof(skb->cb));

	rcu_read_lock();
	list_for_each_entry_rcu(sdata, &local->interfaces, list) {
		if (sdata->vif.type == NL80211_IFTYPE_MONITOR) {
			if (!ieee80211_sdata_running(sdata))
				continue;

			if ((sdata->u.mntr_flags & MONITOR_FLAG_COOK_FRAMES) &&
			    !send_to_cooked)
				continue;

			if (prev_dev) {
				skb2 = skb_clone(skb, GFP_ATOMIC);
				if (skb2) {
					skb2->dev = prev_dev;
					netif_rx(skb2);
				}
			}

			prev_dev = sdata->dev;
		}
	}
	if (prev_dev) {
		skb->dev = prev_dev;
		netif_rx(skb);
		skb = NULL;
	}
	rcu_read_unlock();
	dev_kfree_skb(skb);
}
EXPORT_SYMBOL(ieee80211_tx_status);
