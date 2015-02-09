/* IEEE 802.11 SoftMAC layer
 * Copyright (c) 2005 Andrea Merello <andreamrl@tiscali.it>
 *
 * Mostly extracted from the rtl8180-sa2400 driver for the
 * in-kernel generic ieee802.11 stack.
 *
 * Few lines might be stolen from other part of the ieee80211
 * stack. Copyright who own it's copyright
 *
 * WPA code stolen from the ipw2200 driver.
 * Copyright who own it's copyright.
 *
 * released under the GPL
 */


#include "ieee80211.h"

#include <linux/random.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#ifdef ENABLE_DOT11D
#include "dot11d.h"
#endif

u8 rsn_authen_cipher_suite[16][4] = {
	{0x00,0x0F,0xAC,0x00}, //Use group key, //Reserved
	{0x00,0x0F,0xAC,0x01}, //WEP-40         //RSNA default
	{0x00,0x0F,0xAC,0x02}, //TKIP           //NONE		//{used just as default}
	{0x00,0x0F,0xAC,0x03}, //WRAP-historical
	{0x00,0x0F,0xAC,0x04}, //CCMP
	{0x00,0x0F,0xAC,0x05}, //WEP-104
};

short ieee80211_is_54g(struct ieee80211_network net)
{
	return ((net.rates_ex_len > 0) || (net.rates_len > 4));
}

short ieee80211_is_shortslot(struct ieee80211_network net)
{
	return (net.capability & WLAN_CAPABILITY_SHORT_SLOT);
}

/* returns the total length needed for pleacing the RATE MFIE
 * tag and the EXTENDED RATE MFIE tag if needed.
 * It encludes two bytes per tag for the tag itself and its len
 */
unsigned int ieee80211_MFIE_rate_len(struct ieee80211_device *ieee)
{
	unsigned int rate_len = 0;

	if (ieee->modulation & IEEE80211_CCK_MODULATION)
		rate_len = IEEE80211_CCK_RATE_LEN + 2;

	if (ieee->modulation & IEEE80211_OFDM_MODULATION)

		rate_len += IEEE80211_OFDM_RATE_LEN + 2;

	return rate_len;
}

/* pleace the MFIE rate, tag to the memory (double) poined.
 * Then it updates the pointer so that
 * it points after the new MFIE tag added.
 */
void ieee80211_MFIE_Brate(struct ieee80211_device *ieee, u8 **tag_p)
{
	u8 *tag = *tag_p;

	if (ieee->modulation & IEEE80211_CCK_MODULATION){
		*tag++ = MFIE_TYPE_RATES;
		*tag++ = 4;
		*tag++ = IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_1MB;
		*tag++ = IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_2MB;
		*tag++ = IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_5MB;
		*tag++ = IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_11MB;
	}

	/* We may add an option for custom rates that specific HW might support */
	*tag_p = tag;
}

void ieee80211_MFIE_Grate(struct ieee80211_device *ieee, u8 **tag_p)
{
	u8 *tag = *tag_p;

		if (ieee->modulation & IEEE80211_OFDM_MODULATION){

		*tag++ = MFIE_TYPE_RATES_EX;
		*tag++ = 8;
		*tag++ = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_6MB;
		*tag++ = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_9MB;
		*tag++ = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_12MB;
		*tag++ = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_18MB;
		*tag++ = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_24MB;
		*tag++ = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_36MB;
		*tag++ = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_48MB;
		*tag++ = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_54MB;

	}

	/* We may add an option for custom rates that specific HW might support */
	*tag_p = tag;
}


void ieee80211_WMM_Info(struct ieee80211_device *ieee, u8 **tag_p) {
	u8 *tag = *tag_p;

	*tag++ = MFIE_TYPE_GENERIC; //0
	*tag++ = 7;
	*tag++ = 0x00;
	*tag++ = 0x50;
	*tag++ = 0xf2;
	*tag++ = 0x02;//5
	*tag++ = 0x00;
	*tag++ = 0x01;
#ifdef SUPPORT_USPD
	if(ieee->current_network.wmm_info & 0x80) {
		*tag++ = 0x0f|MAX_SP_Len;
	} else {
		*tag++ = MAX_SP_Len;
	}
#else
	*tag++ = MAX_SP_Len;
#endif
	*tag_p = tag;
}

#ifdef THOMAS_TURBO
void ieee80211_TURBO_Info(struct ieee80211_device *ieee, u8 **tag_p) {
	u8 *tag = *tag_p;

	*tag++ = MFIE_TYPE_GENERIC; //0
	*tag++ = 7;
	*tag++ = 0x00;
	*tag++ = 0xe0;
	*tag++ = 0x4c;
	*tag++ = 0x01;//5
	*tag++ = 0x02;
	*tag++ = 0x11;
	*tag++ = 0x00;

	*tag_p = tag;
	printk(KERN_ALERT "This is enable turbo mode IE process\n");
}
#endif

void enqueue_mgmt(struct ieee80211_device *ieee, struct sk_buff *skb)
{
	int nh;
	nh = (ieee->mgmt_queue_head +1) % MGMT_QUEUE_NUM;

/*
 * if the queue is full but we have newer frames then
 * just overwrites the oldest.
 *
 * if (nh == ieee->mgmt_queue_tail)
 *		return -1;
 */
	ieee->mgmt_queue_head = nh;
	ieee->mgmt_queue_ring[nh] = skb;

	//return 0;
}

struct sk_buff *dequeue_mgmt(struct ieee80211_device *ieee)
{
	struct sk_buff *ret;

	if(ieee->mgmt_queue_tail == ieee->mgmt_queue_head)
		return NULL;

	ret = ieee->mgmt_queue_ring[ieee->mgmt_queue_tail];

	ieee->mgmt_queue_tail =
		(ieee->mgmt_queue_tail+1) % MGMT_QUEUE_NUM;

	return ret;
}

void init_mgmt_queue(struct ieee80211_device *ieee)
{
	ieee->mgmt_queue_tail = ieee->mgmt_queue_head = 0;
}

u8 MgntQuery_MgntFrameTxRate(struct ieee80211_device *ieee)
{
	PRT_HIGH_THROUGHPUT      pHTInfo = ieee->pHTInfo;
	u8 rate;

	// 2008/01/25 MH For broadcom, MGNT frame set as OFDM 6M.
	if(pHTInfo->IOTAction & HT_IOT_ACT_MGNT_USE_CCK_6M)
		rate = 0x0c;
	else
		rate = ieee->basic_rate & 0x7f;

	if(rate == 0){
		// 2005.01.26, by rcnjko.
		if(ieee->mode == IEEE_A||
		   ieee->mode== IEEE_N_5G||
		   (ieee->mode== IEEE_N_24G&&!pHTInfo->bCurSuppCCK))
			rate = 0x0c;
		else
			rate = 0x02;
	}

	/*
	// Data rate of ProbeReq is already decided. Annie, 2005-03-31
	if( pMgntInfo->bScanInProgress || (pMgntInfo->bDualModeScanStep!=0) )
	{
	if(pMgntInfo->dot11CurrentWirelessMode==WIRELESS_MODE_A)
	rate = 0x0c;
	else
	rate = 0x02;
	}
	 */
	return rate;
}


void ieee80211_sta_wakeup(struct ieee80211_device *ieee, short nl);

inline void softmac_mgmt_xmit(struct sk_buff *skb, struct ieee80211_device *ieee)
{
	unsigned long flags;
	short single = ieee->softmac_features & IEEE_SOFTMAC_SINGLE_QUEUE;
	struct ieee80211_hdr_3addr  *header=
		(struct ieee80211_hdr_3addr  *) skb->data;

	cb_desc *tcb_desc = (cb_desc *)(skb->cb + 8);
	spin_lock_irqsave(&ieee->lock, flags);

	/* called with 2nd param 0, no mgmt lock required */
	ieee80211_sta_wakeup(ieee,0);

	tcb_desc->queue_index = MGNT_QUEUE;
	tcb_desc->data_rate = MgntQuery_MgntFrameTxRate(ieee);
	tcb_desc->RATRIndex = 7;
	tcb_desc->bTxDisableRateFallBack = 1;
	tcb_desc->bTxUseDriverAssingedRate = 1;

	if(single){
		if(ieee->queue_stop){
			enqueue_mgmt(ieee,skb);
		}else{
			header->seq_ctl = cpu_to_le16(ieee->seq_ctrl[0]<<4);

			if (ieee->seq_ctrl[0] == 0xFFF)
				ieee->seq_ctrl[0] = 0;
			else
				ieee->seq_ctrl[0]++;

			/* avoid watchdog triggers */
			ieee->dev->trans_start = jiffies;
			ieee->softmac_data_hard_start_xmit(skb,ieee->dev,ieee->basic_rate);
			//dev_kfree_skb_any(skb);//edit by thomas
		}

		spin_unlock_irqrestore(&ieee->lock, flags);
	}else{
		spin_unlock_irqrestore(&ieee->lock, flags);
		spin_lock_irqsave(&ieee->mgmt_tx_lock, flags);

		header->seq_ctl = cpu_to_le16(ieee->seq_ctrl[0] << 4);

		if (ieee->seq_ctrl[0] == 0xFFF)
			ieee->seq_ctrl[0] = 0;
		else
			ieee->seq_ctrl[0]++;

		/* check wether the managed packet queued greater than 5 */
		if(!ieee->check_nic_enough_desc(ieee->dev,tcb_desc->queue_index)||\
				(skb_queue_len(&ieee->skb_waitQ[tcb_desc->queue_index]) != 0)||\
				(ieee->queue_stop) ) {
			/* insert the skb packet to the management queue */
			/* as for the completion function, it does not need
			 * to check it any more.
			 * */
			printk("%s():insert to waitqueue!\n",__FUNCTION__);
			skb_queue_tail(&ieee->skb_waitQ[tcb_desc->queue_index], skb);
		} else {
			//printk("TX packet!\n");
			ieee->softmac_hard_start_xmit(skb,ieee->dev);
			//dev_kfree_skb_any(skb);//edit by thomas
		}
		spin_unlock_irqrestore(&ieee->mgmt_tx_lock, flags);
	}
}

inline void softmac_ps_mgmt_xmit(struct sk_buff *skb, struct ieee80211_device *ieee)
{

	short single = ieee->softmac_features & IEEE_SOFTMAC_SINGLE_QUEUE;
	struct ieee80211_hdr_3addr  *header =
		(struct ieee80211_hdr_3addr  *) skb->data;


	if(single){

		header->seq_ctl = cpu_to_le16(ieee->seq_ctrl[0] << 4);

		if (ieee->seq_ctrl[0] == 0xFFF)
			ieee->seq_ctrl[0] = 0;
		else
			ieee->seq_ctrl[0]++;

		/* avoid watchdog triggers */
		ieee->dev->trans_start = jiffies;
		ieee->softmac_data_hard_start_xmit(skb,ieee->dev,ieee->basic_rate);

	}else{

		header->seq_ctl = cpu_to_le16(ieee->seq_ctrl[0] << 4);

		if (ieee->seq_ctrl[0] == 0xFFF)
			ieee->seq_ctrl[0] = 0;
		else
			ieee->seq_ctrl[0]++;

		ieee->softmac_hard_start_xmit(skb,ieee->dev);

	}
	//dev_kfree_skb_any(skb);//edit by thomas
}

inline struct sk_buff *ieee80211_probe_req(struct ieee80211_device *ieee)
{
	unsigned int len,rate_len;
	u8 *tag;
	struct sk_buff *skb;
	struct ieee80211_probe_request *req;

	len = ieee->current_network.ssid_len;

	rate_len = ieee80211_MFIE_rate_len(ieee);

	skb = dev_alloc_skb(sizeof(struct ieee80211_probe_request) +
			    2 + len + rate_len + ieee->tx_headroom);
	if (!skb)
		return NULL;

	skb_reserve(skb, ieee->tx_headroom);

	req = (struct ieee80211_probe_request *) skb_put(skb,sizeof(struct ieee80211_probe_request));
	req->header.frame_ctl = cpu_to_le16(IEEE80211_STYPE_PROBE_REQ);
	req->header.duration_id = 0;

	memset(req->header.addr1, 0xff, ETH_ALEN);
	memcpy(req->header.addr2, ieee->dev->dev_addr, ETH_ALEN);
	memset(req->header.addr3, 0xff, ETH_ALEN);

	tag = (u8 *) skb_put(skb,len+2+rate_len);

	*tag++ = MFIE_TYPE_SSID;
	*tag++ = len;
	memcpy(tag, ieee->current_network.ssid, len);
	tag += len;

	ieee80211_MFIE_Brate(ieee,&tag);
	ieee80211_MFIE_Grate(ieee,&tag);
	return skb;
}

struct sk_buff *ieee80211_get_beacon_(struct ieee80211_device *ieee);
void ieee80211_send_beacon(struct ieee80211_device *ieee)
{
	struct sk_buff *skb;
	if(!ieee->ieee_up)
		return;
	//unsigned long flags;
	skb = ieee80211_get_beacon_(ieee);

	if (skb){
		softmac_mgmt_xmit(skb, ieee);
		ieee->softmac_stats.tx_beacons++;
		//dev_kfree_skb_any(skb);//edit by thomas
	}
//	ieee->beacon_timer.expires = jiffies +
//		(MSECS( ieee->current_network.beacon_interval -5));

	//spin_lock_irqsave(&ieee->beacon_lock,flags);
	if(ieee->beacon_txing && ieee->ieee_up){
//		if(!timer_pending(&ieee->beacon_timer))
//			add_timer(&ieee->beacon_timer);
		mod_timer(&ieee->beacon_timer,jiffies+(MSECS(ieee->current_network.beacon_interval-5)));
	}
	//spin_unlock_irqrestore(&ieee->beacon_lock,flags);
}


void ieee80211_send_beacon_cb(unsigned long _ieee)
{
	struct ieee80211_device *ieee =
		(struct ieee80211_device *) _ieee;
	unsigned long flags;

	spin_lock_irqsave(&ieee->beacon_lock, flags);
	ieee80211_send_beacon(ieee);
	spin_unlock_irqrestore(&ieee->beacon_lock, flags);
}


void ieee80211_send_probe(struct ieee80211_device *ieee)
{
	struct sk_buff *skb;

	skb = ieee80211_probe_req(ieee);
	if (skb){
		softmac_mgmt_xmit(skb, ieee);
		ieee->softmac_stats.tx_probe_rq++;
		//dev_kfree_skb_any(skb);//edit by thomas
	}
}

void ieee80211_send_probe_requests(struct ieee80211_device *ieee)
{
	if (ieee->active_scan && (ieee->softmac_features & IEEE_SOFTMAC_PROBERQ)){
		ieee80211_send_probe(ieee);
		ieee80211_send_probe(ieee);
	}
}

/* this performs syncro scan blocking the caller until all channels
 * in the allowed channel map has been checked.
 */
void ieee80211_softmac_scan_syncro(struct ieee80211_device *ieee)
{
	short ch = 0;
#ifdef ENABLE_DOT11D
	u8 channel_map[MAX_CHANNEL_NUMBER+1];
	memcpy(channel_map, GET_DOT11D_INFO(ieee)->channel_map, MAX_CHANNEL_NUMBER+1);
#endif
	down(&ieee->scan_sem);

	while(1)
	{

		do{
			ch++;
			if (ch > MAX_CHANNEL_NUMBER)
				goto out; /* scan completed */
#ifdef ENABLE_DOT11D
		}while(!channel_map[ch]);
#else
		}while(!ieee->channel_map[ch]);
#endif

		/* this fuction can be called in two situations
		 * 1- We have switched to ad-hoc mode and we are
		 *    performing a complete syncro scan before conclude
		 *    there are no interesting cell and to create a
		 *    new one. In this case the link state is
		 *    IEEE80211_NOLINK until we found an interesting cell.
		 *    If so the ieee8021_new_net, called by the RX path
		 *    will set the state to IEEE80211_LINKED, so we stop
		 *    scanning
		 * 2- We are linked and the root uses run iwlist scan.
		 *    So we switch to IEEE80211_LINKED_SCANNING to remember
		 *    that we are still logically linked (not interested in
		 *    new network events, despite for updating the net list,
		 *    but we are temporarly 'unlinked' as the driver shall
		 *    not filter RX frames and the channel is changing.
		 * So the only situation in witch are interested is to check
		 * if the state become LINKED because of the #1 situation
		 */

		if (ieee->state == IEEE80211_LINKED)
			goto out;
		ieee->set_chan(ieee->dev, ch);
#ifdef ENABLE_DOT11D
		if(channel_map[ch] == 1)
#endif
		ieee80211_send_probe_requests(ieee);

		/* this prevent excessive time wait when we
		 * need to wait for a syncro scan to end..
		 */
		if(ieee->state < IEEE80211_LINKED)
			;
		else
		if (ieee->sync_scan_hurryup)
			goto out;


		msleep_interruptible_rsl(IEEE80211_SOFTMAC_SCAN_TIME);

	}
out:
	if(ieee->state < IEEE80211_LINKED){
		ieee->actscanning = false;
		up(&ieee->scan_sem);
	}
	else{
	ieee->sync_scan_hurryup = 0;
#ifdef ENABLE_DOT11D
	if(IS_DOT11D_ENABLE(ieee))
		DOT11D_ScanComplete(ieee);
#endif
	up(&ieee->scan_sem);
}
}


void ieee80211_softmac_scan_wq(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work, struct delayed_work, work);
	struct ieee80211_device *ieee = container_of(dwork, struct ieee80211_device, softmac_scan_wq);
	static short watchdog = 0;
#ifdef ENABLE_DOT11D
	u8 channel_map[MAX_CHANNEL_NUMBER+1];
	memcpy(channel_map, GET_DOT11D_INFO(ieee)->channel_map, MAX_CHANNEL_NUMBER+1);
#endif
	if(!ieee->ieee_up)
		return;
	down(&ieee->scan_sem);
	do{
		ieee->current_network.channel =
			(ieee->current_network.channel + 1) % MAX_CHANNEL_NUMBER;
		if (watchdog++ > MAX_CHANNEL_NUMBER)
		{
		//if current channel is not in channel map, set to default channel.
		#ifdef ENABLE_DOT11D
			if (!channel_map[ieee->current_network.channel]);
		#else
			if (!ieee->channel_map[ieee->current_network.channel]);
		#endif
				ieee->current_network.channel = 6;
				goto out; /* no good chans */
		}
#ifdef ENABLE_DOT11D
	}while(!channel_map[ieee->current_network.channel]);
#else
	}while(!ieee->channel_map[ieee->current_network.channel]);
#endif
	if (ieee->scanning == 0 )
		goto out;
	ieee->set_chan(ieee->dev, ieee->current_network.channel);
#ifdef ENABLE_DOT11D
	if(channel_map[ieee->current_network.channel] == 1)
#endif
	ieee80211_send_probe_requests(ieee);


	queue_delayed_work(ieee->wq, &ieee->softmac_scan_wq, IEEE80211_SOFTMAC_SCAN_TIME);

	up(&ieee->scan_sem);
	return;
out:
#ifdef ENABLE_DOT11D
	if(IS_DOT11D_ENABLE(ieee))
		DOT11D_ScanComplete(ieee);
#endif
	ieee->actscanning = false;
	watchdog = 0;
	ieee->scanning = 0;
	up(&ieee->scan_sem);
}



void ieee80211_beacons_start(struct ieee80211_device *ieee)
{
	unsigned long flags;
	spin_lock_irqsave(&ieee->beacon_lock,flags);

	ieee->beacon_txing = 1;
	ieee80211_send_beacon(ieee);

	spin_unlock_irqrestore(&ieee->beacon_lock,flags);
}

void ieee80211_beacons_stop(struct ieee80211_device *ieee)
{
	unsigned long flags;

	spin_lock_irqsave(&ieee->beacon_lock,flags);

	ieee->beacon_txing = 0;
	del_timer_sync(&ieee->beacon_timer);

	spin_unlock_irqrestore(&ieee->beacon_lock,flags);

}


void ieee80211_stop_send_beacons(struct ieee80211_device *ieee)
{
	if(ieee->stop_send_beacons)
		ieee->stop_send_beacons(ieee->dev);
	if (ieee->softmac_features & IEEE_SOFTMAC_BEACONS)
		ieee80211_beacons_stop(ieee);
}


void ieee80211_start_send_beacons(struct ieee80211_device *ieee)
{
	if(ieee->start_send_beacons)
		ieee->start_send_beacons(ieee->dev,ieee->basic_rate);
	if(ieee->softmac_features & IEEE_SOFTMAC_BEACONS)
		ieee80211_beacons_start(ieee);
}


void ieee80211_softmac_stop_scan(struct ieee80211_device *ieee)
{
//	unsigned long flags;

	//ieee->sync_scan_hurryup = 1;

	down(&ieee->scan_sem);
//	spin_lock_irqsave(&ieee->lock, flags);

	if (ieee->scanning == 1){
		ieee->scanning = 0;

		cancel_delayed_work(&ieee->softmac_scan_wq);
	}

//	spin_unlock_irqrestore(&ieee->lock, flags);
	up(&ieee->scan_sem);
}

void ieee80211_stop_scan(struct ieee80211_device *ieee)
{
	if (ieee->softmac_features & IEEE_SOFTMAC_SCAN)
		ieee80211_softmac_stop_scan(ieee);
	else
		ieee->stop_scan(ieee->dev);
}

/* called with ieee->lock held */
void ieee80211_start_scan(struct ieee80211_device *ieee)
{
#ifdef ENABLE_DOT11D
	if(IS_DOT11D_ENABLE(ieee) )
	{
		if(IS_COUNTRY_IE_VALID(ieee))
		{
			RESET_CIE_WATCHDOG(ieee);
		}
	}
#endif
	if (ieee->softmac_features & IEEE_SOFTMAC_SCAN){
		if (ieee->scanning == 0){
			ieee->scanning = 1;
			queue_delayed_work(ieee->wq, &ieee->softmac_scan_wq, 0);
		}
	}else
		ieee->start_scan(ieee->dev);

}

/* called with wx_sem held */
void ieee80211_start_scan_syncro(struct ieee80211_device *ieee)
{
#ifdef ENABLE_DOT11D
	if(IS_DOT11D_ENABLE(ieee) )
	{
		if(IS_COUNTRY_IE_VALID(ieee))
		{
			RESET_CIE_WATCHDOG(ieee);
		}
	}
#endif
	ieee->sync_scan_hurryup = 0;
	if (ieee->softmac_features & IEEE_SOFTMAC_SCAN)
		ieee80211_softmac_scan_syncro(ieee);
	else
		ieee->scan_syncro(ieee->dev);

}

inline struct sk_buff *ieee80211_authentication_req(struct ieee80211_network *beacon,
	struct ieee80211_device *ieee, int challengelen)
{
	struct sk_buff *skb;
	struct ieee80211_authentication *auth;
	int len = sizeof(struct ieee80211_authentication) + challengelen + ieee->tx_headroom;


	skb = dev_alloc_skb(len);
	if (!skb) return NULL;

	skb_reserve(skb, ieee->tx_headroom);
	auth = (struct ieee80211_authentication *)
		skb_put(skb, sizeof(struct ieee80211_authentication));

	auth->header.frame_ctl = IEEE80211_STYPE_AUTH;
	if (challengelen) auth->header.frame_ctl |= IEEE80211_FCTL_WEP;

	auth->header.duration_id = 0x013a;

	memcpy(auth->header.addr1, beacon->bssid, ETH_ALEN);
	memcpy(auth->header.addr2, ieee->dev->dev_addr, ETH_ALEN);
	memcpy(auth->header.addr3, beacon->bssid, ETH_ALEN);

	//auth->algorithm = ieee->open_wep ? WLAN_AUTH_OPEN : WLAN_AUTH_SHARED_KEY;
	if(ieee->auth_mode == 0)
		auth->algorithm = WLAN_AUTH_OPEN;
	else if(ieee->auth_mode == 1)
		auth->algorithm = WLAN_AUTH_SHARED_KEY;
	else if(ieee->auth_mode == 2)
		auth->algorithm = WLAN_AUTH_OPEN;//0x80;
	printk("=================>%s():auth->algorithm is %d\n",__FUNCTION__,auth->algorithm);
	auth->transaction = cpu_to_le16(ieee->associate_seq);
	ieee->associate_seq++;

	auth->status = cpu_to_le16(WLAN_STATUS_SUCCESS);

	return skb;

}


static struct sk_buff* ieee80211_probe_resp(struct ieee80211_device *ieee, u8 *dest)
{
	u8 *tag;
	int beacon_size;
	struct ieee80211_probe_response *beacon_buf;
	struct sk_buff *skb = NULL;
	int encrypt;
	int atim_len,erp_len;
	struct ieee80211_crypt_data* crypt;

	char *ssid = ieee->current_network.ssid;
	int ssid_len = ieee->current_network.ssid_len;
	int rate_len = ieee->current_network.rates_len+2;
	int rate_ex_len = ieee->current_network.rates_ex_len;
	int wpa_ie_len = ieee->wpa_ie_len;
	u8 erpinfo_content = 0;

	u8* tmp_ht_cap_buf;
	u8 tmp_ht_cap_len=0;
	u8* tmp_ht_info_buf;
	u8 tmp_ht_info_len=0;
	PRT_HIGH_THROUGHPUT	pHTInfo = ieee->pHTInfo;
	u8* tmp_generic_ie_buf=NULL;
	u8 tmp_generic_ie_len=0;

	if(rate_ex_len > 0) rate_ex_len+=2;

	if(ieee->current_network.capability & WLAN_CAPABILITY_IBSS)
		atim_len = 4;
	else
		atim_len = 0;

	if(ieee80211_is_54g(ieee->current_network))
		erp_len = 3;
	else
		erp_len = 0;


	crypt = ieee->crypt[ieee->tx_keyidx];


	encrypt = ieee->host_encrypt && crypt && crypt->ops &&
		((0 == strcmp(crypt->ops->name, "WEP") || wpa_ie_len));
	//HT ralated element
	tmp_ht_cap_buf =(u8*) &(ieee->pHTInfo->SelfHTCap);
	tmp_ht_cap_len = sizeof(ieee->pHTInfo->SelfHTCap);
	tmp_ht_info_buf =(u8*) &(ieee->pHTInfo->SelfHTInfo);
	tmp_ht_info_len = sizeof(ieee->pHTInfo->SelfHTInfo);
	HTConstructCapabilityElement(ieee, tmp_ht_cap_buf, &tmp_ht_cap_len,encrypt);
	HTConstructInfoElement(ieee,tmp_ht_info_buf,&tmp_ht_info_len, encrypt);


	if(pHTInfo->bRegRT2RTAggregation)
	{
		tmp_generic_ie_buf = ieee->pHTInfo->szRT2RTAggBuffer;
		tmp_generic_ie_len = sizeof(ieee->pHTInfo->szRT2RTAggBuffer);
		HTConstructRT2RTAggElement(ieee, tmp_generic_ie_buf, &tmp_generic_ie_len);
	}
//	printk("===============>tmp_ht_cap_len is %d,tmp_ht_info_len is %d, tmp_generic_ie_len is %d\n",tmp_ht_cap_len,tmp_ht_info_len,tmp_generic_ie_len);
	beacon_size = sizeof(struct ieee80211_probe_response)+2+
		ssid_len
		+3 //channel
		+rate_len
		+rate_ex_len
		+atim_len
		+erp_len
		+wpa_ie_len
	//	+tmp_ht_cap_len
	//	+tmp_ht_info_len
	//	+tmp_generic_ie_len
//		+wmm_len+2
		+ieee->tx_headroom;
	skb = dev_alloc_skb(beacon_size);
	if (!skb)
		return NULL;
	skb_reserve(skb, ieee->tx_headroom);
	beacon_buf = (struct ieee80211_probe_response*) skb_put(skb, (beacon_size - ieee->tx_headroom));
	memcpy (beacon_buf->header.addr1, dest,ETH_ALEN);
	memcpy (beacon_buf->header.addr2, ieee->dev->dev_addr, ETH_ALEN);
	memcpy (beacon_buf->header.addr3, ieee->current_network.bssid, ETH_ALEN);

	beacon_buf->header.duration_id = 0;
	beacon_buf->beacon_interval =
		cpu_to_le16(ieee->current_network.beacon_interval);
	beacon_buf->capability =
		cpu_to_le16(ieee->current_network.capability & WLAN_CAPABILITY_IBSS);
	beacon_buf->capability |=
		cpu_to_le16(ieee->current_network.capability & WLAN_CAPABILITY_SHORT_PREAMBLE); //add short preamble here

	if(ieee->short_slot && (ieee->current_network.capability & WLAN_CAPABILITY_SHORT_SLOT))
		cpu_to_le16((beacon_buf->capability |= WLAN_CAPABILITY_SHORT_SLOT));

	crypt = ieee->crypt[ieee->tx_keyidx];
	if (encrypt)
		beacon_buf->capability |= cpu_to_le16(WLAN_CAPABILITY_PRIVACY);


	beacon_buf->header.frame_ctl = cpu_to_le16(IEEE80211_STYPE_PROBE_RESP);
	beacon_buf->info_element[0].id = MFIE_TYPE_SSID;
	beacon_buf->info_element[0].len = ssid_len;

	tag = (u8*) beacon_buf->info_element[0].data;

	memcpy(tag, ssid, ssid_len);

	tag += ssid_len;

	*(tag++) = MFIE_TYPE_RATES;
	*(tag++) = rate_len-2;
	memcpy(tag,ieee->current_network.rates,rate_len-2);
	tag+=rate_len-2;

	*(tag++) = MFIE_TYPE_DS_SET;
	*(tag++) = 1;
	*(tag++) = ieee->current_network.channel;

	if(atim_len){
	u16 val16;
		*(tag++) = MFIE_TYPE_IBSS_SET;
		*(tag++) = 2;
		/
	if (ieee->state == IEEE80211_ASSOCIATING_AUTHENTICATING){
		IEEE80211_DEBUG_MGMT("Authentication failed\n");
		ieee->softmac_stats.no_auth_rs++;
	}else{
		IEEE80211_DEBUG_MGMT("Association failed\n");
		ieee->softmac_stats.no_ass_rs++;
	}

	ieee->state = IEEE80211_ASSOCIATING_RETRY;

	queue_delayed_work(ieee->wq, &ieee->associate_retry_wq, \
			   IEEE80211_SOFTMAC_ASSOC_RETRY_TIME);

	spin_unlock_irqrestore(&ieee->lock, flags);
}

void ieee80211_associate_abort_cb(unsigned long dev)
{
	ieee80211_associate_abort((struct ieee80211_device *) dev);
}


void ieee80211_associate_step1(struct ieee80211_device *ieee)
{
	struct ieee80211_network *beacon = &ieee->current_network;
	struct sk_buff *skb;

	IEEE80211_DEBUG_MGMT("Stopping scan\n");

	ieee->softmac_stats.tx_auth_rq++;
	skb=ieee80211_authentication_req(beacon, ieee, 0);

	if (!skb)
		ieee80211_associate_abort(ieee);
	else{
		ieee->state = IEEE80211_ASSOCIATING_AUTHENTICATING ;
		IEEE80211_DEBUG_MGMT("Sending authentication request\n");
		//printk(KERN_WARNING "Sending authentication request\n");
		softmac_mgmt_xmit(skb, ieee);
		//BUGON when you try to add_timer twice, using mod_timer may be better, john0709
		if(!timer_pending(&ieee->associate_timer)){
			ieee->associate_timer.expires = jiffies + (HZ / 2);
			add_timer(&ieee->associate_timer);
		}
		//dev_kfree_skb_any(skb);//edit by thomas
	}
}

void ieee80211_auth_challenge(struct ieee80211_device *ieee, u8 *challenge, int chlen)
{
	u8 *c;
	struct sk_buff *skb;
	struct ieee80211_network *beacon = &ieee->current_network;
//	int hlen = sizeof(struct ieee80211_authentication);

	ieee->associate_seq++;
	ieee->softmac_stats.tx_auth_rq++;

	skb = ieee80211_authentication_req(beacon, ieee, chlen+2);
	if (!skb)
		ieee80211_associate_abort(ieee);
	else{
		c = skb_put(skb, chlen+2);
		*(c++) = MFIE_TYPE_CHALLENGE;
		*(c++) = chlen;
		memcpy(c, challenge, chlen);

		IEEE80211_DEBUG_MGMT("Sending authentication challenge response\n");

		ieee80211_encrypt_fragment(ieee, skb, sizeof(struct ieee80211_hdr_3addr  ));

		softmac_mgmt_xmit(skb, ieee);
		mod_timer(&ieee->associate_timer, jiffies + (HZ/2));
		//dev_kfree_skb_any(skb);//edit by thomas
	}
	kfree(challenge);
}

void ieee80211_associate_step2(struct ieee80211_device *ieee)
{
	struct sk_buff* skb;
	struct ieee80211_network *beacon = &ieee->current_network;

	del_timer_sync(&ieee->associate_timer);

	IEEE80211_DEBUG_MGMT("Sending association request\n");

	ieee->softmac_stats.tx_ass_rq++;
	skb=ieee80211_association_req(beacon, ieee);
	if (!skb)
		ieee80211_associate_abort(ieee);
	else{
		softmac_mgmt_xmit(skb, ieee);
		mod_timer(&ieee->associate_timer, jiffies + (HZ/2));
		//dev_kfree_skb_any(skb);//edit by thomas
	}
}
void ieee80211_associate_complete_wq(struct work_struct *work)
{
	struct ieee80211_device *ieee = container_of(work, struct ieee80211_device, associate_complete_wq);
	printk(KERN_INFO "Associated successfully\n");
	if(ieee80211_is_54g(ieee->current_network) &&
		(ieee->modulation & IEEE80211_OFDM_MODULATION)){

		ieee->rate = 108;
		printk(KERN_INFO"Using G rates:%d\n", ieee->rate);
	}else{
		ieee->rate = 22;
		printk(KERN_INFO"Using B rates:%d\n", ieee->rate);
	}
	if (ieee->pHTInfo->bCurrentHTSupport&&ieee->pHTInfo->bEnableHT)
	{
		printk("Successfully associated, ht enabled\n");
		HTOnAssocRsp(ieee);
	}
	else
	{
		printk("Successfully associated, ht not enabled(%d, %d)\n", ieee->pHTInfo->bCurrentHTSupport, ieee->pHTInfo->bEnableHT);
		memset(ieee->dot11HTOperationalRateSet, 0, 16);
		//HTSetConnectBwMode(ieee, HT_CHANNEL_WIDTH_20, HT_EXTCHNL_OFFSET_NO_EXT);
	}
	ieee->LinkDetectInfo.SlotNum = 2 * (1 + ieee->current_network.beacon_interval/500);
	// To prevent the immediately calling watch_dog after association.
	if(ieee->LinkDetectInfo.NumRecvBcnInPeriod==0||ieee->LinkDetectInfo.NumRecvDataInPeriod==0 )
	{
		ieee->LinkDetectInfo.NumRecvBcnInPeriod = 1;
		ieee->LinkDetectInfo.NumRecvDataInPeriod= 1;
	}
	ieee->link_change(ieee->dev);
	if(ieee->is_silent_reset == 0){
		printk("============>normal associate\n");
	notify_wx_assoc_event(ieee);
	}
	else if(ieee->is_silent_reset == 1)
	{
		printk("==================>silent reset associate\n");
		ieee->is_silent_reset = 0;
	}

	if (ieee->data_hard_resume)
		ieee->data_hard_resume(ieee->dev);
	netif_carrier_on(ieee->dev);
}

void ieee80211_associate_complete(struct ieee80211_device *ieee)
{
//	int i;
//	struct net_device* dev = ieee->dev;
	del_timer_sync(&ieee->associate_timer);

	ieee->state = IEEE80211_LINKED;
	//ieee->UpdateHalRATRTableHandler(dev, ieee->dot11HTOperationalRateSet);
	queue_work(ieee->wq, &ieee->associate_complete_wq);
}

void ieee80211_associate_procedure_wq(struct work_struct *work)
{
	struct ieee80211_device *ieee = container_of(work, struct ieee80211_device, associate_procedure_wq);
	ieee->sync_scan_hurryup = 1;
	down(&ieee->wx_sem);

	if (ieee->data_hard_stop)
		ieee->data_hard_stop(ieee->dev);

	ieee80211_stop_scan(ieee);
	printk("===>%s(), chan:%d\n", __FUNCTION__, ieee->current_network.channel);
	//ieee->set_chan(ieee->dev, ieee->current_network.channel);
	HTSetConnectBwMode(ieee, HT_CHANNEL_WIDTH_20, HT_EXTCHNL_OFFSET_NO_EXT);

	ieee->associate_seq = 1;
	ieee80211_associate_step1(ieee);

	up(&ieee->wx_sem);
}

inline void ieee80211_softmac_new_net(struct ieee80211_device *ieee, struct ieee80211_network *net)
{
	u8 tmp_ssid[IW_ESSID_MAX_SIZE+1];
	int tmp_ssid_len = 0;

	short apset,ssidset,ssidbroad,apmatch,ssidmatch;

	/* we are interested in new new only if we are not associated
	 * and we are not associating / authenticating
	 */
	if (ieee->state != IEEE80211_NOLINK)
		return;

	if ((ieee->iw_mode == IW_MODE_INFRA) && !(net->capability & WLAN_CAPABILITY_BSS))
		return;

	if ((ieee->iw_mode == IW_MODE_ADHOC) && !(net->capability & WLAN_CAPABILITY_IBSS))
		return;


	if (ieee->iw_mode == IW_MODE_INFRA || ieee->iw_mode == IW_MODE_ADHOC){
		/* if the user specified the AP MAC, we need also the essid
		 * This could be obtained by beacons or, if the network does not
		 * broadcast it, it can be put manually.
		 */
		apset = ieee->wap_set;//(memcmp(ieee->current_network.bssid, zero,ETH_ALEN)!=0 );
		ssidset = ieee->ssid_set;//ieee->current_network.ssid[0] != '\0';
		ssidbroad =  !(net->ssid_len == 0 || net->ssid[0]== '\0');
		apmatch = (memcmp(ieee->current_network.bssid, net->bssid, ETH_ALEN)==0);
		ssidmatch = (ieee->current_network.ssid_len == net->ssid_len)&&\
				(!strncmp(ieee->current_network.ssid, net->ssid, net->ssid_len));


		if (	/* if the user set the AP check if match.
			 * if the network does not broadcast essid we check the user supplyed ANY essid
			 * if the network does broadcast and the user does not set essid it is OK
			 * if the network does broadcast and the user did set essid chech if essid match
			 */
			( apset && apmatch &&
				((ssidset && ssidbroad && ssidmatch) || (ssidbroad && !ssidset) || (!ssidbroad && ssidset)) ) ||
			/* if the ap is not set, check that the user set the bssid
			 * and the network does bradcast and that those two bssid matches
			 */
			(!apset && ssidset && ssidbroad && ssidmatch)
			){
				/* if the essid is hidden replace it with the
				* essid provided by the user.
				*/
				if (!ssidbroad){
					strncpy(tmp_ssid, ieee->current_network.ssid, IW_ESSID_MAX_SIZE);
					tmp_ssid_len = ieee->current_network.ssid_len;
				}
				memcpy(&ieee->current_network, net, sizeof(struct ieee80211_network));

				if (!ssidbroad){
					strncpy(ieee->current_network.ssid, tmp_ssid, IW_ESSID_MAX_SIZE);
					ieee->current_network.ssid_len = tmp_ssid_len;
				}
				printk(KERN_INFO"Linking with %s,channel:%d, qos:%d, myHT:%d, networkHT:%d\n",ieee->current_network.ssid,ieee->current_network.channel, ieee->current_network.qos_data.supported, ieee->pHTInfo->bEnableHT, ieee->current_network.bssht.bdSupportHT);

				//ieee->pHTInfo->IOTAction = 0;
				HTResetIOTSetting(ieee->pHTInfo);
				if (ieee->iw_mode == IW_MODE_INFRA){
					/* Join the network for the first time */
					ieee->AsocRetryCount = 0;
					//for HT by amy 080514
					if((ieee->current_network.qos_data.supported == 1) &&
					  // (ieee->pHTInfo->bEnableHT && ieee->current_network.bssht.bdSupportHT))
					   ieee->current_network.bssht.bdSupportHT)
/*WB, 2008.09.09:bCurrentHTSupport and bEnableHT two flags are going to put together to check whether we are in HT now, so needn't to check bEnableHT flags here. That's is to say we will set to HT support whenever joined AP has the ability to support HT. And whether we are in HT or not, please check bCurrentHTSupport&&bEnableHT now please.*/
					{
					//	ieee->pHTInfo->bCurrentHTSupport = true;
						HTResetSelfAndSavePeerSetting(ieee, &(ieee->current_network));
					}
					else
					{
						ieee->pHTInfo->bCurrentHTSupport = false;
					}

					ieee->state = IEEE80211_ASSOCIATING;
					queue_work(ieee->wq, &ieee->associate_procedure_wq);
				}else{
					if(ieee80211_is_54g(ieee->current_network) &&
						(ieee->modulation & IEEE80211_OFDM_MODULATION)){
						ieee->rate = 108;
						ieee->SetWirelessMode(ieee->dev, IEEE_G);
						printk(KERN_INFO"Using G rates\n");
					}else{
						ieee->rate = 22;
						ieee->SetWirelessMode(ieee->dev, IEEE_B);
						printk(KERN_INFO"Using B rates\n");
					}
					memset(ieee->dot11HTOperationalRateSet, 0, 16);
					//HTSetConnectBwMode(ieee, HT_CHANNEL_WIDTH_20, HT_EXTCHNL_OFFSET_NO_EXT);
					ieee->state = IEEE80211_LINKED;
				}

		}
	}

}

void ieee80211_softmac_check_all_nets(struct ieee80211_device *ieee)
{
	unsigned long flags;
	struct ieee80211_network *target;

	spin_lock_irqsave(&ieee->lock, flags);

	list_for_each_entry(target, &ieee->network_list, list) {

		/* if the state become different that NOLINK means
		 * we had found what we are searching for
		 */

		if (ieee->state != IEEE80211_NOLINK)
			break;

		if (ieee->scan_age == 0 || time_after(target->last_scanned + ieee->scan_age, jiffies))
		ieee80211_softmac_new_net(ieee, target);
	}

	spin_unlock_irqrestore(&ieee->lock, flags);

}


static inline u16 auth_parse(struct sk_buff *skb, u8** challenge, int *chlen)
{
	struct ieee80211_authentication *a;
	u8 *t;
	if (skb->len <  (sizeof(struct ieee80211_authentication)-sizeof(struct ieee80211_info_element))){
		IEEE80211_DEBUG_MGMT("invalid len in auth resp: %d\n",skb->len);
		return 0xcafe;
	}
	*challenge = NULL;
	a = (struct ieee80211_authentication*) skb->data;
	if(skb->len > (sizeof(struct ieee80211_authentication) +3)){
		t = skb->data + sizeof(struct ieee80211_authentication);

		if(*(t++) == MFIE_TYPE_CHALLENGE){
			*chlen = *(t++);
			*challenge = kmemdup(t, *chlen, GFP_ATOMIC);
			if (!*challenge)
				return -ENOMEM;
		}
	}

	return cpu_to_le16(a->status);

}


int auth_rq_parse(struct sk_buff *skb,u8* dest)
{
	struct ieee80211_authentication *a;

	if (skb->len <  (sizeof(struct ieee80211_authentication)-sizeof(struct ieee80211_info_element))){
		IEEE80211_DEBUG_MGMT("invalid len in auth request: %d\n",skb->len);
		return -1;
	}
	a = (struct ieee80211_authentication*) skb->data;

	memcpy(dest,a->header.addr2, ETH_ALEN);

	if (le16_to_cpu(a->algorithm) != WLAN_AUTH_OPEN)
		return  WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG;

	return WLAN_STATUS_SUCCESS;
}

static short probe_rq_parse(struct ieee80211_device *ieee, struct sk_buff *skb, u8 *src)
{
	u8 *tag;
	u8 *skbend;
	u8 *ssid=NULL;
	u8 ssidlen = 0;

	struct ieee80211_hdr_3addr   *header =
		(struct ieee80211_hdr_3addr   *) skb->data;

	if (skb->len < sizeof (struct ieee80211_hdr_3addr  ))
		return -1; /* corrupted */

	memcpy(src,header->addr2, ETH_ALEN);

	skbend = (u8*)skb->data + skb->len;

	tag = skb->data + sizeof (struct ieee80211_hdr_3addr  );

	while (tag+1 < skbend){
		if (*tag == 0){
			ssid = tag+2;
			ssidlen = *(tag+1);
			break;
		}
		tag++; /* point to the len field */
		tag = tag + *(tag); /* point to the last data byte of the tag */
		tag++; /* point to the next tag */
	}

	//IEEE80211DMESG("Card MAC address is "MACSTR, MAC2STR(src));
	if (ssidlen == 0) return 1;

	if (!ssid) return 1; /* ssid not found in tagged param */
	return (!strncmp(ssid, ieee->current_network.ssid, ssidlen));

}

int assoc_rq_parse(struct sk_buff *skb,u8* dest)
{
	struct ieee80211_assoc_request_frame *a;

	if (skb->len < (sizeof(struct ieee80211_assoc_request_frame) -
		sizeof(struct ieee80211_info_element))) {

		IEEE80211_DEBUG_MGMT("invalid len in auth request:%d \n", skb->len);
		return -1;
	}

	a = (struct ieee80211_assoc_request_frame*) skb->data;

	memcpy(dest,a->header.addr2,ETH_ALEN);

	return 0;
}

static inline u16 assoc_parse(struct ieee80211_device *ieee, struct sk_buff *skb, int *aid)
{
	struct ieee80211_assoc_response_frame *response_head;
	u16 status_code;

	if (skb->len <  sizeof(struct ieee80211_assoc_response_frame)){
		IEEE80211_DEBUG_MGMT("invalid len in auth resp: %d\n", skb->len);
		return 0xcafe;
	}

	response_head = (struct ieee80211_assoc_response_frame*) skb->data;
	*aid = le16_to_cpu(response_head->aid) & 0x3fff;

	status_code = le16_to_cpu(response_head->status);
	if((status_code==WLAN_STATUS_ASSOC_DENIED_RATES || \
	   status_code==WLAN_STATUS_CAPS_UNSUPPORTED)&&
	   ((ieee->mode == IEEE_G) &&
	    (ieee->current_network.mode == IEEE_N_24G) &&
	    (ieee->AsocRetryCount++ < (RT_ASOC_RETRY_LIMIT-1)))) {
		 ieee->pHTInfo->IOTAction |= HT_IOT_ACT_PURE_N_MODE;
	}else {
		 ieee->AsocRetryCount = 0;
	}

	return le16_to_cpu(response_head->status);
}

static inline void
ieee80211_rx_probe_rq(struct ieee80211_device *ieee, struct sk_buff *skb)
{
	u8 dest[ETH_ALEN];

	//IEEE80211DMESG("Rx probe");
	ieee->softmac_stats.rx_probe_rq++;
	//DMESG("Dest is "MACSTR, MAC2STR(dest));
	if (probe_rq_parse(ieee, skb, dest)){
		//IEEE80211DMESG("Was for me!");
		ieee->softmac_stats.tx_probe_rs++;
		ieee80211_resp_to_probe(ieee, dest);
	}
}

static inline void
ieee80211_rx_auth_rq(struct ieee80211_device *ieee, struct sk_buff *skb)
{
	u8 dest[ETH_ALEN];
	int status;
	//IEEE80211DMESG("Rx probe");
	ieee->softmac_stats.rx_auth_rq++;

	status = auth_rq_parse(skb, dest);
	if (status != -1) {
		ieee80211_resp_to_auth(ieee, status, dest);
	}
	//DMESG("Dest is "MACSTR, MAC2STR(dest));

}

static inline void
ieee80211_rx_assoc_rq(struct ieee80211_device *ieee, struct sk_buff *skb)
{

	u8 dest[ETH_ALEN];
	//unsigned long flags;

	ieee->softmac_stats.rx_ass_rq++;
	if (assoc_rq_parse(skb,dest) != -1){
		ieee80211_resp_to_assoc_rq(ieee, dest);
	}

	printk(KERN_INFO"New client associated: %pM\n", dest);
}



void ieee80211_sta_ps_send_null_frame(struct ieee80211_device *ieee, short pwr)
{

	struct sk_buff *buf = ieee80211_null_func(ieee, pwr);

	if (buf)
		softmac_ps_mgmt_xmit(buf, ieee);

}


short ieee80211_sta_ps_sleep(struct ieee80211_device *ieee, u32 *time_h, u32 *time_l)
{
	int timeout = ieee->ps_timeout;
	u8 dtim;
	/*if(ieee->ps == IEEE80211_PS_DISABLED ||
		ieee->iw_mode != IW_MODE_INFRA ||
		ieee->state != IEEE80211_LINKED)

		return 0;
	*/
	dtim = ieee->current_network.dtim_data;
	//printk("DTIM\n");
	if(!(dtim & IEEE80211_DTIM_VALID))
		return 0;
	timeout = ieee->current_network.beacon_interval; //should we use ps_timeout value or beacon_interval
	//printk("VALID\n");
	ieee->current_network.dtim_data = IEEE80211_DTIM_INVALID;

	if(dtim & ((IEEE80211_DTIM_UCAST | IEEE80211_DTIM_MBCAST)& ieee->ps))
		return 2;

	if(!time_after(jiffies, ieee->dev->trans_start + MSECS(timeout)))
		return 0;

	if(!time_after(jiffies, ieee->last_rx_ps_time + MSECS(timeout)))
		return 0;

	if((ieee->softmac_features & IEEE_SOFTMAC_SINGLE_QUEUE ) &&
		(ieee->mgmt_queue_tail != ieee->mgmt_queue_head))
		return 0;

	if(time_l){
		*time_l = ieee->current_network.last_dtim_sta_time[0]
			+ (ieee->current_network.beacon_interval
			* ieee->current_network.dtim_period) * 1000;
	}

	if(time_h){
		*time_h = ieee->current_network.last_dtim_sta_time[1];
		if(time_l && *time_l < ieee->current_network.last_dtim_sta_time[0])
			*time_h += 1;
	}

	return 1;


}

inline void ieee80211_sta_ps(struct ieee80211_device *ieee)
{

	u32 th,tl;
	short sleep;

	unsigned long flags,flags2;

	spin_lock_irqsave(&ieee->lock, flags);

	if((ieee->ps == IEEE80211_PS_DISABLED ||
		ieee->iw_mode != IW_MODE_INFRA ||
		ieee->state != IEEE80211_LINKED)){

	//	#warning CHECK_LOCK_HERE
		spin_lock_irqsave(&ieee->mgmt_tx_lock, flags2);

		ieee80211_sta_wakeup(ieee, 1);

		spin_unlock_irqrestore(&ieee->mgmt_tx_lock, flags2);
	}

	sleep = ieee80211_sta_ps_sleep(ieee,&th, &tl);
	/* 2 wake, 1 sleep, 0 do nothing */
	if(sleep == 0)
		goto out;

	if(sleep == 1){

		if(ieee->sta_sleep == 1)
			ieee->enter_sleep_state(ieee->dev,th,tl);

		else if(ieee->sta_sleep == 0){
		//	printk("send null 1\n");
			spin_lock_irqsave(&ieee->mgmt_tx_lock, flags2);

			if(ieee->ps_is_queue_empty(ieee->dev)){


				ieee->sta_sleep = 2;

				ieee->ps_request_tx_ack(ieee->dev);

				ieee80211_sta_ps_send_null_frame(ieee,1);

				ieee->ps_th = th;
				ieee->ps_tl = tl;
			}
			spin_unlock_irqrestore(&ieee->mgmt_tx_lock, flags2);

		}


	}else if(sleep == 2){
//#warning CHECK_LOCK_HERE
		spin_lock_irqsave(&ieee->mgmt_tx_lock, flags2);

		ieee80211_sta_wakeup(ieee,1);

		spin_unlock_irqrestore(&ieee->mgmt_tx_lock, flags2);
	}

out:
	spin_unlock_irqrestore(&ieee->lock, flags);

}

void ieee80211_sta_wakeup(struct ieee80211_device *ieee, short nl)
{
	if(ieee->sta_sleep == 0){
		if(nl){
			printk("Warning: driver is probably failing to report TX ps error\n");
			ieee->ps_request_tx_ack(ieee->dev);
			ieee80211_sta_ps_send_null_frame(ieee, 0);
		}
		return;

	}

	if(ieee->sta_sleep == 1)
		ieee->sta_wake_up(ieee->dev);

	ieee->sta_sleep = 0;

	if(nl){
		ieee->ps_request_tx_ack(ieee->dev);
		ieee80211_sta_ps_send_null_frame(ieee, 0);
	}
}

void ieee80211_ps_tx_ack(struct ieee80211_device *ieee, short success)
{
	unsigned long flags,flags2;

	spin_lock_irqsave(&ieee->lock, flags);

	if(ieee->sta_sleep == 2){
		/* Null frame with PS bit set */
		if(success){
			ieee->sta_sleep = 1;
			ieee->enter_sleep_state(ieee->dev,ieee->ps_th,ieee->ps_tl);
		}
		/* if the card report not success we can't be sure the AP
		 * has not RXed so we can't assume the AP believe us awake
		 */
	}
	/* 21112005 - tx again null without PS bit if lost */
	else {

		if((ieee->sta_sleep == 0) && !success){
			spin_lock_irqsave(&ieee->mgmt_tx_lock, flags2);
			ieee80211_sta_ps_send_null_frame(ieee, 0);
			spin_unlock_irqrestore(&ieee->mgmt_tx_lock, flags2);
		}
	}
	spin_unlock_irqrestore(&ieee->lock, flags);
}
void ieee80211_process_action(struct ieee80211_device* ieee, struct sk_buff* skb)
{
	struct ieee80211_hdr* header = (struct ieee80211_hdr*)skb->data;
	u8* act = ieee80211_get_payload(header);
	u8 tmp = 0;
//	IEEE80211_DEBUG_DATA(IEEE80211_DL_DATA|IEEE80211_DL_BA, skb->data, skb->len);
	if (act == NULL)
	{
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "error to get payload of action frame\n");
		return;
	}
	tmp = *act;
	act ++;
	switch (tmp)
	{
		case ACT_CAT_BA:
			if (*act == ACT_ADDBAREQ)
			ieee80211_rx_ADDBAReq(ieee, skb);
			else if (*act == ACT_ADDBARSP)
			ieee80211_rx_ADDBARsp(ieee, skb);
			else if (*act == ACT_DELBA)
			ieee80211_rx_DELBA(ieee, skb);
			break;
		default:
//			if (net_ratelimit())
//			IEEE80211_DEBUG(IEEE80211_DL_BA, "unknown action frame(%d)\n", tmp);
			break;
	}
	return;

}
inline int
ieee80211_rx_frame_softmac(struct ieee80211_device *ieee, struct sk_buff *skb,
			struct ieee80211_rx_stats *rx_stats, u16 type,
			u16 stype)
{
	struct ieee80211_hdr_3addr *header = (struct ieee80211_hdr_3addr *) skb->data;
	u16 errcode;
	u8* challenge;
	int chlen=0;
	int aid;
	struct ieee80211_assoc_response_frame *assoc_resp;
//	struct ieee80211_info_element *info_element;
	bool bSupportNmode = true, bHalfSupportNmode = false; //default support N mode, disable halfNmode

	if(!ieee->proto_started)
		return 0;

	if(ieee->sta_sleep || (ieee->ps != IEEE80211_PS_DISABLED &&
		ieee->iw_mode == IW_MODE_INFRA &&
		ieee->state == IEEE80211_LINKED))

		tasklet_schedule(&ieee->ps_task);

	if(WLAN_FC_GET_STYPE(header->frame_ctl) != IEEE80211_STYPE_PROBE_RESP &&
		WLAN_FC_GET_STYPE(header->frame_ctl) != IEEE80211_STYPE_BEACON)
		ieee->last_rx_ps_time = jiffies;

	switch (WLAN_FC_GET_STYPE(header->frame_ctl)) {

		case IEEE80211_STYPE_ASSOC_RESP:
		case IEEE80211_STYPE_REASSOC_RESP:

			IEEE80211_DEBUG_MGMT("received [RE]ASSOCIATION RESPONSE (%d)\n",
					WLAN_FC_GET_STYPE(header->frame_ctl));
			if ((ieee->softmac_features & IEEE_SOFTMAC_ASSOCIATE) &&
				ieee->state == IEEE80211_ASSOCIATING_AUTHENTICATED &&
				ieee->iw_mode == IW_MODE_INFRA){
				struct ieee80211_network network_resp;
				struct ieee80211_network *network = &network_resp;

				if (0 == (errcode=assoc_parse(ieee,skb, &aid))){
					ieee->state=IEEE80211_LINKED;
					ieee->assoc_id = aid;
					ieee->softmac_stats.rx_ass_ok++;
					/* station support qos */
					/* Let the register setting defaultly with Legacy station */
					if(ieee->qos_support) {
						assoc_resp = (struct ieee80211_assoc_response_frame*)skb->data;
						memset(network, 0, sizeof(*network));
						if (ieee80211_parse_info_param(ieee,assoc_resp->info_element,\
									rx_stats->len - sizeof(*assoc_resp),\
									network,rx_stats)){
							return 1;
						}
						else
						{	//filling the PeerHTCap. //maybe not neccesary as we can get its info from current_network.
							memcpy(ieee->pHTInfo->PeerHTCapBuf, network->bssht.bdHTCapBuf, network->bssht.bdHTCapLen);
							memcpy(ieee->pHTInfo->PeerHTInfoBuf, network->bssht.bdHTInfoBuf, network->bssht.bdHTInfoLen);
						}
						if (ieee->handle_assoc_response != NULL)
							ieee->handle_assoc_response(ieee->dev, (struct ieee80211_assoc_response_frame*)header, network);
					}
					ieee80211_associate_complete(ieee);
				} else {
					/* aid could not been allocated */
					ieee->softmac_stats.rx_ass_err++;
					printk(
						"Association response status code 0x%x\n",
						errcode);
					IEEE80211_DEBUG_MGMT(
						"Association response status code 0x%x\n",
						errcode);
					if(ieee->AsocRetryCount < RT_ASOC_RETRY_LIMIT) {
						queue_work(ieee->wq, &ieee->associate_procedure_wq);
					} else {
						ieee80211_associate_abort(ieee);
					}
				}
			}
			break;

		case IEEE80211_STYPE_ASSOC_REQ:
		case IEEE80211_STYPE_REASSOC_REQ:

			if ((ieee->softmac_features & IEEE_SOFTMAC_ASSOCIATE) &&
				ieee->iw_mode == IW_MODE_MASTER)

				ieee80211_rx_assoc_rq(ieee, skb);
			break;

		case IEEE80211_STYPE_AUTH:

			if (ieee->softmac_features & IEEE_SOFTMAC_ASSOCIATE){
				if (ieee->state == IEEE80211_ASSOCIATING_AUTHENTICATING &&
				ieee->iw_mode == IW_MODE_INFRA){

						IEEE80211_DEBUG_MGMT("Received authentication response");

						if (0 == (errcode=auth_parse(skb, &challenge, &chlen))){
							if(ieee->open_wep || !challenge){
								ieee->state = IEEE80211_ASSOCIATING_AUTHENTICATED;
								ieee->softmac_stats.rx_auth_rs_ok++;
								if(!(ieee->pHTInfo->IOTAction&HT_IOT_ACT_PURE_N_MODE))
								{
									if (!ieee->GetNmodeSupportBySecCfg(ieee->dev))
									{
												// WEP or TKIP encryption
										if(IsHTHalfNmodeAPs(ieee))
										{
											bSupportNmode = true;
											bHalfSupportNmode = true;
										}
										else
										{
											bSupportNmode = false;
											bHalfSupportNmode = false;
										}
									printk("==========>to link with AP using SEC(%d, %d)", bSupportNmode, bHalfSupportNmode);
									}
								}
								/* Dummy wirless mode setting to avoid encryption issue */
								if(bSupportNmode) {
									//N mode setting
									ieee->SetWirelessMode(ieee->dev, \
											ieee->current_network.mode);
								}else{
									//b/g mode setting
									/*TODO*/
									ieee->SetWirelessMode(ieee->dev, IEEE_G);
								}

								if (ieee->current_network.mode == IEEE_N_24G && bHalfSupportNmode == true)
								{
									printk("===============>entern half N mode\n");
									ieee->bHalfWirelessN24GMode = true;
								}
								else
									ieee->bHalfWirelessN24GMode = false;

								ieee80211_associate_step2(ieee);
							}else{
								ieee80211_auth_challenge(ieee, challenge, chlen);
							}
						}else{
							ieee->softmac_stats.rx_auth_rs_err++;
							IEEE80211_DEBUG_MGMT("Authentication respose status code 0x%x",errcode);
							ieee80211_associate_abort(ieee);
						}

					}else if (ieee->iw_mode == IW_MODE_MASTER){
						ieee80211_rx_auth_rq(ieee, skb);
					}
				}
			break;

		case IEEE80211_STYPE_PROBE_REQ:

			if ((ieee->softmac_features & IEEE_SOFTMAC_PROBERS) &&
				((ieee->iw_mode == IW_MODE_ADHOC ||
				ieee->iw_mode == IW_MODE_MASTER) &&
				ieee->state == IEEE80211_LINKED)){
				ieee80211_rx_probe_rq(ieee, skb);
			}
			break;

		case IEEE80211_STYPE_DISASSOC:
		case IEEE80211_STYPE_DEAUTH:
			if ((ieee->softmac_features & IEEE_SOFTMAC_ASSOCIATE) &&
				ieee->state == IEEE80211_LINKED &&
				ieee->iw_mode == IW_MODE_INFRA){

				ieee->state = IEEE80211_ASSOCIATING;
				ieee->softmac_stats.reassoc++;

				notify_wx_assoc_event(ieee);
				//HTSetConnectBwMode(ieee, HT_CHANNEL_WIDTH_20, HT_EXTCHNL_OFFSET_NO_EXT);
				RemovePeerTS(ieee, header->addr2);
				queue_work(ieee->wq, &ieee->associate_procedure_wq);
			}
			break;
		case IEEE80211_STYPE_MANAGE_ACT:
			ieee80211_process_action(ieee,skb);
			break;
		default:
			return -1;
			break;
	}

	//dev_kfree_skb_any(skb);
	return 0;
}

/* following are for a simplier TX queue management.
 * Instead of using netif_[stop/wake]_queue the driver
 * will uses these two function (plus a reset one), that
 * will internally uses the kernel netif_* and takes
 * care of the ieee802.11 fragmentation.
 * So the driver receives a fragment per time and might
 * call the stop function when it want without take care
 * to have enought room to TX an entire packet.
 * This might be useful if each fragment need it's own
 * descriptor, thus just keep a total free memory > than
 * the max fragmentation treshold is not enought.. If the
 * ieee802.11 stack passed a TXB struct then you needed
 * to keep N free descriptors where
 * N = MAX_PACKET_SIZE / MIN_FRAG_TRESHOLD
 * In this way you need just one and the 802.11 stack
 * will take care of buffering fragments and pass them to
 * to the driver later, when it wakes the queue.
 */
void ieee80211_softmac_xmit(struct ieee80211_txb *txb, struct ieee80211_device *ieee)
{

	unsigned int queue_index = txb->queue_index;
	unsigned long flags;
	int  i;
	cb_desc *tcb_desc = NULL;

	spin_lock_irqsave(&ieee->lock,flags);

	/* called with 2nd parm 0, no tx mgmt lock required */
	ieee80211_sta_wakeup(ieee,0);

	/* update the tx status */
	ieee->stats.tx_bytes += txb->payload_size;
	ieee->stats.tx_packets++;
	tcb_desc = (cb_desc *)(txb->fragments[0]->cb + MAX_DEV_ADDR_SIZE);
	if(tcb_desc->bMulticast) {
		ieee->stats.multicast++;
	}
	/* if xmit available, just xmit it immediately, else just insert it to the wait queue */
	for(i = 0; i < txb->nr_frags; i++) {
#ifdef USB_TX_DRIVER_AGGREGATION_ENABLE
		if ((skb_queue_len(&ieee->skb_drv_aggQ[queue_index]) != 0) ||
#else
		if ((skb_queue_len(&ieee->skb_waitQ[queue_index]) != 0) ||
#endif
		(!ieee->check_nic_enough_desc(ieee->dev,queue_index))||\
		     (ieee->queue_stop)) {
			/* insert the skb packet to the wait queue */
			/* as for the completion function, it does not need
			 * to check it any more.
			 * */
			//printk("error:no descriptor left@queue_index %d\n", queue_index);
			//ieee80211_stop_queue(ieee);
#ifdef USB_TX_DRIVER_AGGREGATION_ENABLE
			skb_queue_tail(&ieee->skb_drv_aggQ[queue_index], txb->fragments[i]);
#else
			skb_queue_tail(&ieee->skb_waitQ[queue_index], txb->fragments[i]);
#endif
		}else{
			ieee->softmac_data_hard_start_xmit(
					txb->fragments[i],
					ieee->dev,ieee->rate);
			//ieee->stats.tx_packets++;
			//ieee->stats.tx_bytes += txb->fragments[i]->len;
			//ieee->dev->trans_start = jiffies;
		}
	}
	ieee80211_txb_free(txb);

//exit:
	spin_unlock_irqrestore(&ieee->lock,flags);

}

/* called with ieee->lock acquired */
void ieee80211_resume_tx(struct ieee80211_device *ieee)
{
	int i;
	for(i = ieee->tx_pending.frag; i < ieee->tx_pending.txb->nr_frags; i++) {

		if (ieee->queue_stop){
			ieee->tx_pending.frag = i;
			return;
		}else{

			ieee->softmac_data_hard_start_xmit(
				ieee->tx_pending.txb->fragments[i],
				ieee->dev,ieee->rate);
				//(i+1)<ieee->tx_pending.txb->nr_frags);
			ieee->stats.tx_packets++;
			ieee->dev->trans_start = jiffies;
		}
	}


	ieee80211_txb_free(ieee->tx_pending.txb);
	ieee->tx_pending.txb = NULL;
}


void ieee80211_reset_queue(struct ieee80211_device *ieee)
{
	unsigned long flags;

	spin_lock_irqsave(&ieee->lock,flags);
	init_mgmt_queue(ieee);
	if (ieee->tx_pending.txb){
		ieee80211_txb_free(ieee->tx_pending.txb);
		ieee->tx_pending.txb = NULL;
	}
	ieee->queue_stop = 0;
	spin_unlock_irqrestore(&ieee->lock,flags);

}

void ieee80211_wake_queue(struct ieee80211_device *ieee)
{

	unsigned long flags;
	struct sk_buff *skb;
	struct ieee80211_hdr_3addr  *header;

	spin_lock_irqsave(&ieee->lock,flags);
	if (! ieee->queue_stop) goto exit;

	ieee->queue_stop = 0;

	if(ieee->softmac_features & IEEE_SOFTMAC_SINGLE_QUEUE){
		while (!ieee->queue_stop && (skb = dequeue_mgmt(ieee))){

			header = (struct ieee80211_hdr_3addr  *) skb->data;

			header->seq_ctl = cpu_to_le16(ieee->seq_ctrl[0] << 4);

			if (ieee->seq_ctrl[0] == 0xFFF)
				ieee->seq_ctrl[0] = 0;
			else
				ieee->seq_ctrl[0]++;

			ieee->softmac_data_hard_start_xmit(skb,ieee->dev,ieee->basic_rate);
			//dev_kfree_skb_any(skb);//edit by thomas
		}
	}
	if (!ieee->queue_stop && ieee->tx_pending.txb)
		ieee80211_resume_tx(ieee);

	if (!ieee->queue_stop && netif_queue_stopped(ieee->dev)){
		ieee->softmac_stats.swtxawake++;
		netif_wake_queue(ieee->dev);
	}

exit :
	spin_unlock_irqrestore(&ieee->lock,flags);
}


void ieee80211_stop_queue(struct ieee80211_device *ieee)
{
	//unsigned long flags;
	//spin_lock_irqsave(&ieee->lock,flags);

	if (! netif_queue_stopped(ieee->dev)){
		netif_stop_queue(ieee->dev);
		ieee->softmac_stats.swtxstop++;
	}
	ieee->queue_stop = 1;
	//spin_unlock_irqrestore(&ieee->lock,flags);

}


inline void ieee80211_randomize_cell(struct ieee80211_device *ieee)
{

	get_random_bytes(ieee->current_network.bssid, ETH_ALEN);

	/* an IBSS cell address must have the two less significant
	 * bits of the first byte = 2
	 */
	ieee->current_network.bssid[0] &= ~0x01;
	ieee->current_network.bssid[0] |= 0x02;
}

/* called in user context only */
void ieee80211_start_master_bss(struct ieee80211_device *ieee)
{
	ieee->assoc_id = 1;

	if (ieee->current_network.ssid_len == 0){
		strncpy(ieee->current_network.ssid,
			IEEE80211_DEFAULT_TX_ESSID,
			IW_ESSID_MAX_SIZE);

		ieee->current_network.ssid_len = strlen(IEEE80211_DEFAULT_TX_ESSID);
		ieee->ssid_set = 1;
	}

	memcpy(ieee->current_network.bssid, ieee->dev->dev_addr, ETH_ALEN);

	ieee->set_chan(ieee->dev, ieee->current_network.channel);
	ieee->state = IEEE80211_LINKED;
	ieee->link_change(ieee->dev);
	notify_wx_assoc_event(ieee);

	if (ieee->data_hard_resume)
		ieee->data_hard_resume(ieee->dev);

	netif_carrier_on(ieee->dev);
}

void ieee80211_start_monitor_mode(struct ieee80211_device *ieee)
{
	if(ieee->raw_tx){

		if (ieee->data_hard_resume)
			ieee->data_hard_resume(ieee->dev);

		netif_carrier_on(ieee->dev);
	}
}
void ieee80211_start_ibss_wq(struct work_struct *work)
{

	struct delayed_work *dwork = container_of(work, struct delayed_work, work);
	struct ieee80211_device *ieee = container_of(dwork, struct ieee80211_device, start_ibss_wq);
	/* iwconfig mode ad-hoc will schedule this and return
	 * on the other hand this will block further iwconfig SET
	 * operations because of the wx_sem hold.
	 * Anyway some most set operations set a flag to speed-up
	 * (abort) this wq (when syncro scanning) before sleeping
	 * on the semaphore
	 */
	if(!ieee->proto_started){
		printk("==========oh driver down return\n");
		return;
	}
	down(&ieee->wx_sem);

	if (ieee->current_network.ssid_len == 0){
		strcpy(ieee->current_network.ssid,IEEE80211_DEFAULT_TX_ESSID);
		ieee->current_network.ssid_len = strlen(IEEE80211_DEFAULT_TX_ESSID);
		ieee->ssid_set = 1;
	}

	/* check if we have this cell in our network list */
	ieee80211_softmac_check_all_nets(ieee);


#ifdef ENABLE_DOT11D //if creating an ad-hoc, set its channel to 10 temporarily--this is the requirement for ASUS, not 11D, so disable 11d.
//	if((IS_DOT11D_ENABLE(ieee)) && (ieee->state == IEEE80211_NOLINK))
	if (ieee->state == IEEE80211_NOLINK)
		ieee->current_network.channel = 6;
#endif
	/* if not then the state is not linked. Maybe the user swithced to
	 * ad-hoc mode just after being in monitor mode, or just after
	 * being very few time in managed mode (so the card have had no
	 * time to scan all the chans..) or we have just run up the iface
	 * after setting ad-hoc mode. So we have to give another try..
	 * Here, in ibss mode, should be safe to do this without extra care
	 * (in bss mode we had to make sure no-one tryed to associate when
	 * we had just checked the ieee->state and we was going to start the
	 * scan) beacause in ibss mode the ieee80211_new_net function, when
	 * finds a good net, just set the ieee->state to IEEE80211_LINKED,
	 * so, at worst, we waste a bit of time to initiate an unneeded syncro
	 * scan, that will stop at the first round because it sees the state
	 * associated.
	 */
	if (ieee->state == IEEE80211_NOLINK)
		ieee80211_start_scan_syncro(ieee);

	/* the network definitively is not here.. create a new cell */
	if (ieee->state == IEEE80211_NOLINK){
		printk("creating new IBSS cell\n");
		if(!ieee->wap_set)
			ieee80211_randomize_cell(ieee);

		if(ieee->modulation & IEEE80211_CCK_MODULATION){

			ieee->current_network.rates_len = 4;

			ieee->current_network.rates[0] = IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_1MB;
			ieee->current_network.rates[1] = IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_2MB;
			ieee->current_network.rates[2] = IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_5MB;
			ieee->current_network.rates[3] = IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_11MB;

		}else
			ieee->current_network.rates_len = 0;

		if(ieee->modulation & IEEE80211_OFDM_MODULATION){
			ieee->current_network.rates_ex_len = 8;

			ieee->current_network.rates_ex[0] = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_6MB;
			ieee->current_network.rates_ex[1] = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_9MB;
			ieee->current_network.rates_ex[2] = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_12MB;
			ieee->current_network.rates_ex[3] = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_18MB;
			ieee->current_network.rates_ex[4] = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_24MB;
			ieee->current_network.rates_ex[5] = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_36MB;
			ieee->current_network.rates_ex[6] = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_48MB;
			ieee->current_network.rates_ex[7] = IEEE80211_BASIC_RATE_MASK | IEEE80211_OFDM_RATE_54MB;

			ieee->rate = 108;
		}else{
			ieee->current_network.rates_ex_len = 0;
			ieee->rate = 22;
		}

		// By default, WMM function will be disabled in IBSS mode
		ieee->current_network.QoS_Enable = 0;
		ieee->SetWirelessMode(ieee->dev, IEEE_G);
		ieee->current_network.atim_window = 0;
		ieee->current_network.capability = WLAN_CAPABILITY_IBSS;
		if(ieee->short_slot)
			ieee->current_network.capability |= WLAN_CAPABILITY_SHORT_SLOT;

	}

	ieee->state = IEEE80211_LINKED;

	ieee->set_chan(ieee->dev, ieee->current_network.channel);
	ieee->link_change(ieee->dev);

	notify_wx_assoc_event(ieee);

	ieee80211_start_send_beacons(ieee);

	if (ieee->data_hard_resume)
		ieee->data_hard_resume(ieee->dev);
	netif_carrier_on(ieee->dev);

	up(&ieee->wx_sem);
}

inline void ieee80211_start_ibss(struct ieee80211_device *ieee)
{
	queue_delayed_work(ieee->wq, &ieee->start_ibss_wq, 150);
}

/* this is called only in user context, with wx_sem held */
void ieee80211_start_bss(struct ieee80211_device *ieee)
{
	unsigned long flags;
#ifdef ENABLE_DOT11D
	//
	// Ref: 802.11d 11.1.3.3
	// STA shall not start a BSS unless properly formed Beacon frame including a Country IE.
	//
	if(IS_DOT11D_ENABLE(ieee) && !IS_COUNTRY_IE_VALID(ieee))
	{
		if(! ieee->bGlobalDomain)
		{
			return;
		}
	}
#endif
	/* check if we have already found the net we
	 * are interested in (if any).
	 * if not (we are disassociated and we are not
	 * in associating / authenticating phase) start the background scanning.
	 */
	ieee80211_softmac_check_all_nets(ieee);

	/* ensure no-one start an associating process (thus setting
	 * the ieee->state to ieee80211_ASSOCIATING) while we
	 * have just cheked it and we are going to enable scan.
	 * The ieee80211_new_net function is always called with
	 * lock held (from both ieee80211_softmac_check_all_nets and
	 * the rx path), so we cannot be in the middle of such function
	 */
	spin_lock_irqsave(&ieee->lock, flags);

	if (ieee->state == IEEE80211_NOLINK){
		ieee->actscanning = true;
		ieee80211_start_scan(ieee);
	}
	spin_unlock_irqrestore(&ieee->lock, flags);
}

/* called only in userspace context */
void ieee80211_disassociate(struct ieee80211_device *ieee)
{


	netif_carrier_off(ieee->dev);
	if (ieee->softmac_features & IEEE_SOFTMAC_TX_QUEUE)
			ieee80211_reset_queue(ieee);

	if (ieee->data_hard_stop)
			ieee->data_hard_stop(ieee->dev);
#ifdef ENABLE_DOT11D
	if(IS_DOT11D_ENABLE(ieee))
		Dot11d_Reset(ieee);
#endif
	ieee->state = IEEE80211_NOLINK;
	ieee->is_set_key = false;
	ieee->link_change(ieee->dev);
	//HTSetConnectBwMode(ieee, HT_CHANNEL_WIDTH_20, HT_EXTCHNL_OFFSET_NO_EXT);
	notify_wx_assoc_event(ieee);

}
void ieee80211_associate_retry_wq(struct work_struct *work)
{
        struct delayed_work *dwork = container_of(work, struct delayed_work, work);
        struct ieee80211_device *ieee = container_of(dwork, struct ieee80211_device, associate_retry_wq);
	unsigned long flags;

	down(&ieee->wx_sem);
	if(!ieee->proto_started)
		goto exit;

	if(ieee->state != IEEE80211_ASSOCIATING_RETRY)
		goto exit;

	/* until we do not set the state to IEEE80211_NOLINK
	* there are no possibility to have someone else trying
	* to start an association procdure (we get here with
	* ieee->state = IEEE80211_ASSOCIATING).
	* When we set the state to IEEE80211_NOLINK it is possible
	* that the RX path run an attempt to associate, but
	* both ieee80211_softmac_check_all_nets and the
	* RX path works with ieee->lock held so there are no
	* problems. If we are still disassociated then start a scan.
	* the lock here is necessary to ensure no one try to start
	* an association procedure when we have just checked the
	* state and we are going to start the scan.
	*/
	ieee->state = IEEE80211_NOLINK;

	ieee80211_softmac_check_all_nets(ieee);

	spin_lock_irqsave(&ieee->lock, flags);

	if(ieee->state == IEEE80211_NOLINK)
		ieee80211_start_scan(ieee);

	spin_unlock_irqrestore(&ieee->lock, flags);

exit:
	up(&ieee->wx_sem);
}

struct sk_buff *ieee80211_get_beacon_(struct ieee80211_device *ieee)
{
	u8 broadcast_addr[] = {0xff,0xff,0xff,0xff,0xff,0xff};

	struct sk_buff *skb;
	struct ieee80211_probe_response *b;

	skb = ieee80211_probe_resp(ieee, broadcast_addr);

	if (!skb)
		return NULL;

	b = (struct ieee80211_probe_response *) skb->data;
	b->header.frame_ctl = cpu_to_le16(IEEE80211_STYPE_BEACON);

	return skb;

}

struct sk_buff *ieee80211_get_beacon(struct ieee80211_device *ieee)
{
	struct sk_buff *skb;
	struct ieee80211_probe_response *b;

	skb = ieee80211_get_beacon_(ieee);
	if(!skb)
		return NULL;

	b = (struct ieee80211_probe_response *) skb->data;
	b->header.seq_ctl = cpu_to_le16(ieee->seq_ctrl[0] << 4);

	if (ieee->seq_ctrl[0] == 0xFFF)
		ieee->seq_ctrl[0] = 0;
	else
		ieee->seq_ctrl[0]++;

	return skb;
}

void ieee80211_softmac_stop_protocol(struct ieee80211_device *ieee)
{
	ieee->sync_scan_hurryup = 1;
	down(&ieee->wx_sem);
	ieee80211_stop_protocol(ieee);
	up(&ieee->wx_sem);
}


void ieee80211_stop_protocol(struct ieee80211_device *ieee)
{
	if (!ieee->proto_started)
		return;

	ieee->proto_started = 0;

	ieee80211_stop_send_beacons(ieee);
	del_timer_sync(&ieee->associate_timer);
	cancel_delayed_work(&ieee->associate_retry_wq);
	cancel_delayed_work(&ieee->start_ibss_wq);
	ieee80211_stop_scan(ieee);

	ieee80211_disassociate(ieee);
	RemoveAllTS(ieee); //added as we disconnect from the previous BSS, Remove all TS
}

void ieee80211_softmac_start_protocol(struct ieee80211_device *ieee)
{
	ieee->sync_scan_hurryup = 0;
	down(&ieee->wx_sem);
	ieee80211_start_protocol(ieee);
	up(&ieee->wx_sem);
}

void ieee80211_start_protocol(struct ieee80211_device *ieee)
{
	short ch = 0;
	int i = 0;
	if (ieee->proto_started)
		return;

	ieee->proto_started = 1;

	if (ieee->current_network.channel == 0){
		do{
			ch++;
			if (ch > MAX_CHANNEL_NUMBER)
				return; /* no channel found */
#ifdef ENABLE_DOT11D
		}while(!GET_DOT11D_INFO(ieee)->channel_map[ch]);
#else
		}while(!ieee->channel_map[ch]);
#endif
		ieee->current_network.channel = ch;
	}

	if (ieee->current_network.beacon_interval == 0)
		ieee->current_network.beacon_interval = 100;
//	printk("===>%s(), chan:%d\n", __FUNCTION__, ieee->current_network.channel);
//	ieee->set_chan(ieee->dev,ieee->current_network.channel);

	for(i = 0; i < 17; i++) {
	  ieee->last_rxseq_num[i] = -1;
	  ieee->last_rxfrag_num[i] = -1;
	  ieee->last_packet_time[i] = 0;
	}

	ieee->init_wmmparam_flag = 0;//reinitialize AC_xx_PARAM registers.


	/* if the user set the MAC of the ad-hoc cell and then
	 * switch to managed mode, shall we  make sure that association
	 * attempts does not fail just because the user provide the essid
	 * and the nic is still checking for the AP MAC ??
	 */
	if (ieee->iw_mode == IW_MODE_INFRA)
		ieee80211_start_bss(ieee);

	else if (ieee->iw_mode == IW_MODE_ADHOC)
		ieee80211_start_ibss(ieee);

	else if (ieee->iw_mode == IW_MODE_MASTER)
		ieee80211_start_master_bss(ieee);

	else if(ieee->iw_mode == IW_MODE_MONITOR)
		ieee80211_start_monitor_mode(ieee);
}


#define DRV_NAME  "Ieee80211"
void ieee80211_softmac_init(struct ieee80211_device *ieee)
{
	int i;
	memset(&ieee->current_network, 0, sizeof(struct ieee80211_network));

	ieee->state = IEEE80211_NOLINK;
	ieee->sync_scan_hurryup = 0;
	for(i = 0; i < 5; i++) {
	  ieee->seq_ctrl[i] = 0;
	}
#ifdef ENABLE_DOT11D
	ieee->pDot11dInfo = kzalloc(sizeof(RT_DOT11D_INFO), GFP_ATOMIC);
	if (!ieee->pDot11dInfo)
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "can't alloc memory for DOT11D\n");
#endif
	//added for  AP roaming
	ieee->LinkDetectInfo.SlotNum = 2;
	ieee->LinkDetectInfo.NumRecvBcnInPeriod=0;
	ieee->LinkDetectInfo.NumRecvDataInPeriod=0;

	ieee->assoc_id = 0;
	ieee->queue_stop = 0;
	ieee->scanning = 0;
	ieee->softmac_features = 0; //so IEEE2100-like driver are happy
	ieee->wap_set = 0;
	ieee->ssid_set = 0;
	ieee->proto_started = 0;
	ieee->basic_rate = IEEE80211_DEFAULT_BASIC_RATE;
	ieee->rate = 22;
	ieee->ps = IEEE80211_PS_DISABLED;
	ieee->sta_sleep = 0;
	ieee->Regdot11HTOperationalRateSet[0]= 0xff;//support MCS 0~7
	ieee->Regdot11HTOperationalRateSet[1]= 0xff;//support MCS 8~15
	ieee->Regdot11HTOperationalRateSet[4]= 0x01;
	//added by amy
	ieee->actscanning = false;
	ieee->beinretry = false;
	ieee->is_set_key = false;
	init_mgmt_queue(ieee);

	ieee->sta_edca_param[0] = 0x0000A403;
	ieee->sta_edca_param[1] = 0x0000A427;
	ieee->sta_edca_param[2] = 0x005E4342;
	ieee->sta_edca_param[3] = 0x002F3262;
	ieee->aggregation = true;
	ieee->enable_rx_imm_BA = 1;
	ieee->tx_pending.txb = NULL;

	init_timer(&ieee->associate_timer);
	ieee->associate_timer.data = (unsigned long)ieee;
	ieee->associate_timer.function = ieee80211_associate_abort_cb;

	init_timer(&ieee->beacon_timer);
	ieee->beacon_timer.data = (unsigned long) ieee;
	ieee->beacon_timer.function = ieee80211_send_beacon_cb;

#ifdef PF_SYNCTHREAD
	ieee->wq = create_workqueue(DRV_NAME,0);
#else
	ieee->wq = create_workqueue(DRV_NAME);
#endif

	INIT_DELAYED_WORK(&ieee->start_ibss_wq,ieee80211_start_ibss_wq);
	INIT_WORK(&ieee->associate_complete_wq, ieee80211_associate_complete_wq);
	INIT_WORK(&ieee->associate_procedure_wq, ieee80211_associate_procedure_wq);
	INIT_DELAYED_WORK(&ieee->softmac_scan_wq,ieee80211_softmac_scan_wq);
	INIT_DELAYED_WORK(&ieee->associate_retry_wq, ieee80211_associate_retry_wq);
	INIT_WORK(&ieee->wx_sync_scan_wq,ieee80211_wx_sync_scan_wq);


	sema_init(&ieee->wx_sem, 1);
	sema_init(&ieee->scan_sem, 1);

	spin_lock_init(&ieee->mgmt_tx_lock);
	spin_lock_init(&ieee->beacon_lock);

	tasklet_init(&ieee->ps_task,
	     (void(*)(unsigned long)) ieee80211_sta_ps,
	     (unsigned long)ieee);

}

void ieee80211_softmac_free(struct ieee80211_device *ieee)
{
	down(&ieee->wx_sem);
#ifdef ENABLE_DOT11D
	if(NULL != ieee->pDot11dInfo)
	{
		kfree(ieee->pDot11dInfo);
		ieee->pDot11dInfo = NULL;
	}
#endif
	del_timer_sync(&ieee->associate_timer);

	cancel_delayed_work(&ieee->associate_retry_wq);
	destroy_workqueue(ieee->wq);

	up(&ieee->wx_sem);
}

/********************************************************
 * Start of WPA code.                                   *
 * this is stolen from the ipw2200 driver               *
 ********************************************************/


static int ieee80211_wpa_enable(struct ieee80211_device *ieee, int value)
{
	/* This is called when wpa_supplicant loads and closes the driver
	 * interface. */
	printk("%s WPA\n",value ? "enabling" : "disabling");
	ieee->wpa_enabled = value;
	return 0;
}


void ieee80211_wpa_assoc_frame(struct ieee80211_device *ieee, char *wpa_ie, int wpa_ie_len)
{
	/* make sure WPA is enabled */
	ieee80211_wpa_enable(ieee, 1);

	ieee80211_disassociate(ieee);
}


static int ieee80211_wpa_mlme(struct ieee80211_device *ieee, int command, int reason)
{

	int ret = 0;

	switch (command) {
	case IEEE_MLME_STA_DEAUTH:
		// silently ignore
		break;

	case IEEE_MLME_STA_DISASSOC:
		ieee80211_disassociate(ieee);
		break;

	default:
		printk("Unknown MLME request: %d\n", command);
		ret = -EOPNOTSUPP;
	}

	return ret;
}


static int ieee80211_wpa_set_wpa_ie(struct ieee80211_device *ieee,
			      struct ieee_param *param, int plen)
{
	u8 *buf;

	if (param->u.wpa_ie.len > MAX_WPA_IE_LEN ||
	    (param->u.wpa_ie.len && param->u.wpa_ie.data == NULL))
		return -EINVAL;

	if (param->u.wpa_ie.len) {
		buf = kmemdup(param->u.wpa_ie.data, param->u.wpa_ie.len,
			      GFP_KERNEL);
		if (buf == NULL)
			return -ENOMEM;

		kfree(ieee->wpa_ie);
		ieee->wpa_ie = buf;
		ieee->wpa_ie_len = param->u.wpa_ie.len;
	} else {
		kfree(ieee->wpa_ie);
		ieee->wpa_ie = NULL;
		ieee->wpa_ie_len = 0;
	}

	ieee80211_wpa_assoc_frame(ieee, ieee->wpa_ie, ieee->wpa_ie_len);
	return 0;
}

#define AUTH_ALG_OPEN_SYSTEM			0x1
#define AUTH_ALG_SHARED_KEY			0x2

static int ieee80211_wpa_set_auth_algs(struct ieee80211_device *ieee, int value)
{

	struct ieee80211_security sec = {
		.flags = SEC_AUTH_MODE,
	};
	int ret = 0;

	if (value & AUTH_ALG_SHARED_KEY) {
		sec.auth_mode = WLAN_AUTH_SHARED_KEY;
		ieee->open_wep = 0;
		ieee->auth_mode = 1;
	} else if (value & AUTH_ALG_OPEN_SYSTEM){
		sec.auth_mode = WLAN_AUTH_OPEN;
		ieee->open_wep = 1;
		ieee->auth_mode = 0;
	}
	else if (value & IW_AUTH_ALG_LEAP){
		sec.auth_mode = WLAN_AUTH_LEAP;
		ieee->open_wep = 1;
		ieee->auth_mode = 2;
	}


	if (ieee->set_security)
		ieee->set_security(ieee->dev, &sec);
	//else
	//	ret = -EOPNOTSUPP;

	return ret;
}

static int ieee80211_wpa_set_param(struct ieee80211_device *ieee, u8 name, u32 value)
{
	int ret=0;
	unsigned long flags;

	switch (name) {
	case IEEE_PARAM_WPA_ENABLED:
		ret = ieee80211_wpa_enable(ieee, value);
		break;

	case IEEE_PARAM_TKIP_COUNTERMEASURES:
		ieee->tkip_countermeasures=value;
		break;

	case IEEE_PARAM_DROP_UNENCRYPTED: {
		/* HACK:
		 *
		 * wpa_supplicant calls set_wpa_enabled when the driver
		 * is loaded and unloaded, regardless of if WPA is being
		 * used.  No other calls are made which can be used to
		 * determine if encryption will be used or not prior to
		 * association being expected.  If encryption is not being
		 * used, drop_unencrypted is set to false, else true -- we
		 * can use this to determine if the CAP_PRIVACY_ON bit should
		 * be set.
		 */
		struct ieee80211_security sec = {
			.flags = SEC_ENABLED,
			.enabled = value,
		};
		ieee->drop_unencrypted = value;
		/* We only change SEC_LEVEL for open mode. Others
		 * are set by ipw_wpa_set_encryption.
		 */
		if (!value) {
			sec.flags |= SEC_LEVEL;
			sec.level = SEC_LEVEL_0;
		}
		else {
			sec.flags |= SEC_LEVEL;
			sec.level = SEC_LEVEL_1;
		}
		if (ieee->set_security)
			ieee->set_security(ieee->dev, &sec);
		break;
	}

	case IEEE_PARAM_PRIVACY_INVOKED:
		ieee->privacy_invoked=value;
		break;

	case IEEE_PARAM_AUTH_ALGS:
		ret = ieee80211_wpa_set_auth_algs(ieee, value);
		break;

	case IEEE_PARAM_IEEE_802_1X:
		ieee->ieee802_1x=value;
		break;
	case IEEE_PARAM_WPAX_SELECT:
		// added for WPA2 mixed mode
		spin_lock_irqsave(&ieee->wpax_suitlist_lock,flags);
		ieee->wpax_type_set = 1;
		ieee->wpax_type_notify = value;
		spin_unlock_irqrestore(&ieee->wpax_suitlist_lock,flags);
		break;

	default:
		printk("Unknown WPA param: %d\n",name);
		ret = -EOPNOTSUPP;
	}

	return ret;
}

/* implementation borrowed from hostap driver */

static int ieee80211_wpa_set_encryption(struct ieee80211_device *ieee,
				  struct ieee_param *param, int param_len)
{
	int ret = 0;

	struct ieee80211_crypto_ops *ops;
	struct ieee80211_crypt_data **crypt;

	struct ieee80211_security sec = {
		.flags = 0,
	};

	param->u.crypt.err = 0;
	param->u.crypt.alg[IEEE_CRYPT_ALG_NAME_LEN - 1] = '\0';

	if (param_len !=
	    (int) ((char *) param->u.crypt.key - (char *) param) +
	    param->u.crypt.key_len) {
		printk("Len mismatch %d, %d\n", param_len,
			       param->u.crypt.key_len);
		return -EINVAL;
	}
	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff) {
		if (param->u.crypt.idx >= WEP_KEYS)
			return -EINVAL;
		crypt = &ieee->crypt[param->u.crypt.idx];
	} else {
		return -EINVAL;
	}

	if (strcmp(param->u.crypt.alg, "none") == 0) {
		if (crypt) {
			sec.enabled = 0;
			//sec.encrypt = 0;
			sec.level = SEC_LEVEL_0;
			sec.flags |= SEC_ENABLED | SEC_LEVEL;
			ieee80211_crypt_delayed_deinit(ieee, crypt);
		}
		goto done;
	}
	sec.enabled = 1;
//	sec.encrypt = 1;
	sec.flags |= SEC_ENABLED;

	/* IPW HW cannot build TKIP MIC, host decryption still needed. */
	if (!(ieee->host_encrypt || ieee->host_decrypt) &&
	    strcmp(param->u.crypt.alg, "TKIP"))
		goto skip_host_crypt;

	ops = ieee80211_get_crypto_ops(param->u.crypt.alg);
	if (ops == NULL && strcmp(param->u.crypt.alg, "WEP") == 0) {
		request_module("ieee80211_crypt_wep");
		ops = ieee80211_get_crypto_ops(param->u.crypt.alg);
		//set WEP40 first, it will be modified according to WEP104 or WEP40 at other place
	} else if (ops == NULL && strcmp(param->u.crypt.alg, "TKIP") == 0) {
		request_module("ieee80211_crypt_tkip");
		ops = ieee80211_get_crypto_ops(param->u.crypt.alg);
	} else if (ops == NULL && strcmp(param->u.crypt.alg, "CCMP") == 0) {
		request_module("ieee80211_crypt_ccmp");
		ops = ieee80211_get_crypto_ops(param->u.crypt.alg);
	}
	if (ops == NULL) {
		printk("unknown crypto alg '%s'\n", param->u.crypt.alg);
		param->u.crypt.err = IEEE_CRYPT_ERR_UNKNOWN_ALG;
		ret = -EINVAL;
		goto done;
	}

	if (*crypt == NULL || (*crypt)->ops != ops) {
		struct ieee80211_crypt_data *new_crypt;

		ieee80211_crypt_delayed_deinit(ieee, crypt);

		new_crypt = kmalloc(sizeof(*new_crypt), GFP_KERNEL);
		if (new_crypt == NULL) {
			ret = -ENOMEM;
			goto done;
		}
		memset(new_crypt, 0, sizeof(struct ieee80211_crypt_data));
		new_crypt->ops = ops;
		if (new_crypt->ops && try_module_get(new_crypt->ops->owner))
			new_crypt->priv =
				new_crypt->ops->init(param->u.crypt.idx);

		if (new_crypt->priv == NULL) {
			kfree(new_crypt);
			param->u.crypt.err = IEEE_CRYPT_ERR_CRYPT_INIT_FAILED;
			ret = -EINVAL;
			goto done;
		}

		*crypt = new_crypt;
	}

	if (param->u.crypt.key_len > 0 && (*crypt)->ops->set_key &&
	    (*crypt)->ops->set_key(param->u.crypt.key,
				   param->u.crypt.key_len, param->u.crypt.seq,
				   (*crypt)->priv) < 0) {
		printk("key setting failed\n");
		param->u.crypt.err = IEEE_CRYPT_ERR_KEY_SET_FAILED;
		ret = -EINVAL;
		goto done;
	}

 skip_host_crypt:
	if (param->u.crypt.set_tx) {
		ieee->tx_keyidx = param->u.crypt.idx;
		sec.active_key = param->u.crypt.idx;
		sec.flags |= SEC_ACTIVE_KEY;
	} else
		sec.flags &= ~SEC_ACTIVE_KEY;

	if (param->u.crypt.alg != NULL) {
		memcpy(sec.keys[param->u.crypt.idx],
		       param->u.crypt.key,
		       param->u.crypt.key_len);
		sec.key_sizes[param->u.crypt.idx] = param->u.crypt.key_len;
		sec.flags |= (1 << param->u.crypt.idx);

		if (strcmp(param->u.crypt.alg, "WEP") == 0) {
			sec.flags |= SEC_LEVEL;
			sec.level = SEC_LEVEL_1;
		} else if (strcmp(param->u.crypt.alg, "TKIP") == 0) {
			sec.flags |= SEC_LEVEL;
			sec.level = SEC_LEVEL_2;
		} else if (strcmp(param->u.crypt.alg, "CCMP") == 0) {
			sec.flags |= SEC_LEVEL;
			sec.level = SEC_LEVEL_3;
		}
	}
 done:
	if (ieee->set_security)
		ieee->set_security(ieee->dev, &sec);

	/* Do not reset port if card is in Managed mode since resetting will
	 * generate new IEEE 802.11 authentication which may end up in looping
	 * with IEEE 802.1X.  If your hardware requires a reset after WEP
	 * configuration (for example... Prism2), implement the reset_port in
	 * the callbacks structures used to initialize the 802.11 stack. */
	if (ieee->reset_on_keychange &&
	    ieee->iw_mode != IW_MODE_INFRA &&
	    ieee->reset_port &&
	    ieee->reset_port(ieee->dev)) {
		printk("reset_port failed\n");
		param->u.crypt.err = IEEE_CRYPT_ERR_CARD_CONF_FAILED;
		return -EINVAL;
	}

	return ret;
}

inline struct sk_buff *ieee80211_disassociate_skb(
							struct ieee80211_network *beacon,
							struct ieee80211_device *ieee,
							u8	asRsn)
{
	struct sk_buff *skb;
	struct ieee80211_disassoc *disass;

	skb = dev_alloc_skb(sizeof(struct ieee80211_disassoc));
	if (!skb)
		return NULL;

	disass = (struct ieee80211_disassoc *) skb_put(skb,sizeof(struct ieee80211_disassoc));
	disass->header.frame_ctl = cpu_to_le16(IEEE80211_STYPE_DISASSOC);
	disass->header.duration_id = 0;

	memcpy(disass->header.addr1, beacon->bssid, ETH_ALEN);
	memcpy(disass->header.addr2, ieee->dev->dev_addr, ETH_ALEN);
	memcpy(disass->header.addr3, beacon->bssid, ETH_ALEN);

	disass->reason = asRsn;
	return skb;
}


void
SendDisassociation(
		struct ieee80211_device *ieee,
		u8* 					asSta,
		u8						asRsn
)
{
		struct ieee80211_network *beacon = &ieee->current_network;
		struct sk_buff *skb;
		skb = ieee80211_disassociate_skb(beacon,ieee,asRsn);
		if (skb){
				softmac_mgmt_xmit(skb, ieee);
				//dev_kfree_skb_any(skb);//edit by thomas
		}
}

int ieee80211_wpa_supplicant_ioctl(struct ieee80211_device *ieee, struct iw_point *p)
{
	struct ieee_param *param;
	int ret=0;

	down(&ieee->wx_sem);
	//IEEE_DEBUG_INFO("wpa_supplicant: len=%d\n", p->length);

	if (p->length < sizeof(struct ieee_param) || !p->pointer){
		ret = -EINVAL;
		goto out;
	}

	param = kmalloc(p->length, GFP_KERNEL);
	if (param == NULL){
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(param, p->pointer, p->length)) {
		kfree(param);
		ret = -EFAULT;
		goto out;
	}

	switch (param->cmd) {

	case IEEE_CMD_SET_WPA_PARAM:
		ret = ieee80211_wpa_set_param(ieee, param->u.wpa_param.name,
					param->u.wpa_param.value);
		break;

	case IEEE_CMD_SET_WPA_IE:
		ret = ieee80211_wpa_set_wpa_ie(ieee, param, p->length);
		break;

	case IEEE_CMD_SET_ENCRYPTION:
		ret = ieee80211_wpa_set_encryption(ieee, param, p->length);
		break;

	case IEEE_CMD_MLME:
		ret = ieee80211_wpa_mlme(ieee, param->u.mlme.command,
				   param->u.mlme.reason_code);
		break;

	default:
		printk("Unknown WPA supplicant request: %d\n",param->cmd);
		ret = -EOPNOTSUPP;
		break;
	}

	if (ret == 0 && copy_to_user(p->pointer, param, p->length))
		ret = -EFAULT;

	kfree(param);
out:
	up(&ieee->wx_sem);

	return ret;
}

void notify_wx_assoc_event(struct ieee80211_device *ieee)
{
	union iwreq_data wrqu;
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	if (ieee->state == IEEE80211_LINKED)
		memcpy(wrqu.ap_addr.sa_data, ieee->current_network.bssid, ETH_ALEN);
	else
		memset(wrqu.ap_addr.sa_data, 0, ETH_ALEN);
	wireless_send_event(ieee->dev, SIOCGIWAP, &wrqu, NULL);
}

EXPORT_SYMBOL(ieee80211_get_beacon);
EXPORT_SYMBOL(ieee80211_wake_queue);
EXPORT_SYMBOL(ieee80211_stop_queue);
EXPORT_SYMBOL(ieee80211_reset_queue);
EXPORT_SYMBOL(ieee80211_softmac_stop_protocol);
EXPORT_SYMBOL(ieee80211_softmac_start_protocol);
EXPORT_SYMBOL(ieee80211_is_shortslot);
EXPORT_SYMBOL(ieee80211_is_54g);
EXPORT_SYMBOL(ieee80211_wpa_supplicant_ioctl);
EXPORT_SYMBOL(ieee80211_ps_tx_ack);
EXPORT_SYMBOL(ieee80211_softmac_xmit);
EXPORT_SYMBOL(ieee80211_stop_send_beacons);
EXPORT_SYMBOL(notify_wx_assoc_event);
EXPORT_SYMBOL(SendDisassociation);
EXPORT_SYMBOL(ieee80211_disassociate);
EXPORT_SYMBOL(ieee80211_start_send_beacons);
EXPORT_SYMBOL(ieee80211_stop_scan);
EXPORT_SYMBOL(ieee80211_send_probe_requests);
EXPORT_SYMBOL(ieee80211_softmac_scan_syncro);
EXPORT_SYMBOL(ieee80211_start_scan_syncro);
//EXPORT_SYMBOL(ieee80211_sta_ps_send_null_frame);
