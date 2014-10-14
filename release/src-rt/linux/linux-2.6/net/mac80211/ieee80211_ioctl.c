/*
 * Copyright 2002-2005, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <asm/uaccess.h>

#include <net/mac80211.h>
#include "ieee80211_i.h"
#include "hostapd_ioctl.h"
#include "ieee80211_rate.h"
#include "wpa.h"
#include "aes_ccm.h"
#include "debugfs_key.h"

static int ieee80211_regdom = 0x10; /* FCC */
module_param(ieee80211_regdom, int, 0444);
MODULE_PARM_DESC(ieee80211_regdom, "IEEE 802.11 regulatory domain; 64=MKK");

/*
 * If firmware is upgraded by the vendor, additional channels can be used based
 * on the new Japanese regulatory rules. This is indicated by setting
 * ieee80211_japan_5ghz module parameter to one when loading the 80211 kernel
 * module.
 */
static int ieee80211_japan_5ghz /* = 0 */;
module_param(ieee80211_japan_5ghz, int, 0444);
MODULE_PARM_DESC(ieee80211_japan_5ghz, "Vendor-updated firmware for 5 GHz");

static void ieee80211_set_hw_encryption(struct net_device *dev,
					struct sta_info *sta, u8 addr[ETH_ALEN],
					struct ieee80211_key *key)
{
	struct ieee80211_key_conf *keyconf = NULL;
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);

	/* default to sw encryption; this will be cleared by low-level
	 * driver if the hw supports requested encryption */
	if (key)
		key->force_sw_encrypt = 1;

	if (key && local->ops->set_key &&
	    (keyconf = ieee80211_key_data2conf(local, key))) {
		if (local->ops->set_key(local_to_hw(local), SET_KEY, addr,
				       keyconf, sta ? sta->aid : 0)) {
			key->force_sw_encrypt = 1;
			key->hw_key_idx = HW_KEY_IDX_INVALID;
		} else {
			key->force_sw_encrypt =
				!!(keyconf->flags & IEEE80211_KEY_FORCE_SW_ENCRYPT);
			key->hw_key_idx =
				keyconf->hw_key_idx;

		}
	}
	kfree(keyconf);
}


static int ieee80211_set_encryption(struct net_device *dev, u8 *sta_addr,
				    int idx, int alg, int set_tx_key,
				    const u8 *_key, size_t key_len)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	int ret = 0;
	struct sta_info *sta;
	struct ieee80211_key *key, *old_key;
	int try_hwaccel = 1;
	struct ieee80211_key_conf *keyconf;
	struct ieee80211_sub_if_data *sdata;

	sdata = IEEE80211_DEV_TO_SUB_IF(dev);

	if (is_broadcast_ether_addr(sta_addr)) {
		sta = NULL;
		if (idx >= NUM_DEFAULT_KEYS) {
			printk(KERN_DEBUG "%s: set_encrypt - invalid idx=%d\n",
			       dev->name, idx);
			return -EINVAL;
		}
		key = sdata->keys[idx];

		/* TODO: consider adding hwaccel support for these; at least
		 * Atheros key cache should be able to handle this since AP is
		 * only transmitting frames with default keys. */
		/* FIX: hw key cache can be used when only one virtual
		 * STA is associated with each AP. If more than one STA
		 * is associated to the same AP, software encryption
		 * must be used. This should be done automatically
		 * based on configured station devices. For the time
		 * being, this can be only set at compile time. */
	} else {
		set_tx_key = 0;
		if (idx != 0) {
			printk(KERN_DEBUG "%s: set_encrypt - non-zero idx for "
			       "individual key\n", dev->name);
			return -EINVAL;
		}

		sta = sta_info_get(local, sta_addr);
		if (!sta) {
#ifdef CONFIG_MAC80211_VERBOSE_DEBUG
			printk(KERN_DEBUG "%s: set_encrypt - unknown addr "
			       MAC_FMT "\n",
			       dev->name, MAC_ARG(sta_addr));
#endif /* CONFIG_MAC80211_VERBOSE_DEBUG */

			return -ENOENT;
		}

		key = sta->key;
	}

	/* FIX:
	 * Cannot configure default hwaccel keys with WEP algorithm, if
	 * any of the virtual interfaces is using static WEP
	 * configuration because hwaccel would otherwise try to decrypt
	 * these frames.
	 *
	 * For now, just disable WEP hwaccel for broadcast when there is
	 * possibility of conflict with default keys. This can maybe later be
	 * optimized by using non-default keys (at least with Atheros ar521x).
	 */
	if (!sta && alg == ALG_WEP && !local->default_wep_only &&
	    sdata->type != IEEE80211_IF_TYPE_IBSS &&
	    sdata->type != IEEE80211_IF_TYPE_AP) {
		try_hwaccel = 0;
	}

	if (local->hw.flags & IEEE80211_HW_DEVICE_HIDES_WEP) {
		/* Software encryption cannot be used with devices that hide
		 * encryption from the host system, so always try to use
		 * hardware acceleration with such devices. */
		try_hwaccel = 1;
	}

	if ((local->hw.flags & IEEE80211_HW_NO_TKIP_WMM_HWACCEL) &&
	    alg == ALG_TKIP) {
		if (sta && (sta->flags & WLAN_STA_WME)) {
		/* Hardware does not support hwaccel with TKIP when using WMM.
		 */
			try_hwaccel = 0;
		}
		else if (sdata->type == IEEE80211_IF_TYPE_STA) {
			sta = sta_info_get(local, sdata->u.sta.bssid);
			if (sta) {
				if (sta->flags & WLAN_STA_WME) {
					try_hwaccel = 0;
				}
				sta_info_put(sta);
				sta = NULL;
			}
		}
	}

	if (alg == ALG_NONE) {
		keyconf = NULL;
		if (try_hwaccel && key &&
		    key->hw_key_idx != HW_KEY_IDX_INVALID &&
		    local->ops->set_key &&
		    (keyconf = ieee80211_key_data2conf(local, key)) != NULL &&
		    local->ops->set_key(local_to_hw(local), DISABLE_KEY,
				       sta_addr, keyconf, sta ? sta->aid : 0)) {
			printk(KERN_DEBUG "%s: set_encrypt - low-level disable"
			       " failed\n", dev->name);
			ret = -EINVAL;
		}
		kfree(keyconf);

		if (set_tx_key || sdata->default_key == key) {
			ieee80211_debugfs_key_remove_default(sdata);
			sdata->default_key = NULL;
		}
		ieee80211_debugfs_key_remove(key);
		if (sta)
			sta->key = NULL;
		else
			sdata->keys[idx] = NULL;
		ieee80211_key_free(key);
		key = NULL;
	} else {
		old_key = key;
		key = ieee80211_key_alloc(sta ? NULL : sdata, idx, key_len,
					  GFP_KERNEL);
		if (!key) {
			ret = -ENOMEM;
			goto err_out;
		}

		/* default to sw encryption; low-level driver sets these if the
		 * requested encryption is supported */
		key->hw_key_idx = HW_KEY_IDX_INVALID;
		key->force_sw_encrypt = 1;

		key->alg = alg;
		key->keyidx = idx;
		key->keylen = key_len;
		memcpy(key->key, _key, key_len);
		if (set_tx_key)
			key->default_tx_key = 1;

		if (alg == ALG_CCMP) {
			/* Initialize AES key state here as an optimization
			 * so that it does not need to be initialized for every
			 * packet. */
			key->u.ccmp.tfm = ieee80211_aes_key_setup_encrypt(
				key->key);
			if (!key->u.ccmp.tfm) {
				ret = -ENOMEM;
				goto err_free;
			}
		}

		if (set_tx_key || sdata->default_key == old_key) {
			ieee80211_debugfs_key_remove_default(sdata);
			sdata->default_key = NULL;
		}
		ieee80211_debugfs_key_remove(old_key);
		if (sta)
			sta->key = key;
		else
			sdata->keys[idx] = key;
		ieee80211_key_free(old_key);
		ieee80211_debugfs_key_add(local, key);
		if (sta)
			ieee80211_debugfs_key_sta_link(key, sta);

		if (try_hwaccel &&
		    (alg == ALG_WEP || alg == ALG_TKIP || alg == ALG_CCMP))
			ieee80211_set_hw_encryption(dev, sta, sta_addr, key);
	}

	if (set_tx_key || (!sta && !sdata->default_key && key)) {
		sdata->default_key = key;
		if (key)
			ieee80211_debugfs_key_add_default(sdata);

		if (local->ops->set_key_idx &&
		    local->ops->set_key_idx(local_to_hw(local), idx))
			printk(KERN_DEBUG "%s: failed to set TX key idx for "
			       "low-level driver\n", dev->name);
	}

	if (sta)
		sta_info_put(sta);

	return 0;

err_free:
	ieee80211_key_free(key);
err_out:
	if (sta)
		sta_info_put(sta);
	return ret;
}

static int ieee80211_ioctl_siwgenie(struct net_device *dev,
				    struct iw_request_info *info,
				    struct iw_point *data, char *extra)
{
	struct ieee80211_sub_if_data *sdata;
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);

	if (local->user_space_mlme)
		return -EOPNOTSUPP;

	sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	if (sdata->type == IEEE80211_IF_TYPE_STA ||
	    sdata->type == IEEE80211_IF_TYPE_IBSS) {
		int ret = ieee80211_sta_set_extra_ie(dev, extra, data->length);
		if (ret)
			return ret;
		sdata->u.sta.auto_bssid_sel = 0;
		ieee80211_sta_req_auth(dev, &sdata->u.sta);
		return 0;
	}

	if (sdata->type == IEEE80211_IF_TYPE_AP) {
		kfree(sdata->u.ap.generic_elem);
		sdata->u.ap.generic_elem = kmalloc(data->length, GFP_KERNEL);
		if (!sdata->u.ap.generic_elem)
			return -ENOMEM;
		memcpy(sdata->u.ap.generic_elem, extra, data->length);
		sdata->u.ap.generic_elem_len = data->length;
		return ieee80211_if_config(dev);
	}
	return -EOPNOTSUPP;
}

static int ieee80211_ioctl_set_radio_enabled(struct net_device *dev,
					     int val)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct ieee80211_conf *conf = &local->hw.conf;

	conf->radio_enabled = val;
	return ieee80211_hw_config(wdev_priv(dev->ieee80211_ptr));
}

static int ieee80211_ioctl_giwname(struct net_device *dev,
				   struct iw_request_info *info,
				   char *name, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);

	switch (local->hw.conf.phymode) {
	case MODE_IEEE80211A:
		strcpy(name, "IEEE 802.11a");
		break;
	case MODE_IEEE80211B:
		strcpy(name, "IEEE 802.11b");
		break;
	case MODE_IEEE80211G:
		strcpy(name, "IEEE 802.11g");
		break;
	case MODE_ATHEROS_TURBO:
		strcpy(name, "5GHz Turbo");
		break;
	default:
		strcpy(name, "IEEE 802.11");
		break;
	}

	return 0;
}


static int ieee80211_ioctl_giwrange(struct net_device *dev,
				 struct iw_request_info *info,
				 struct iw_point *data, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct iw_range *range = (struct iw_range *) extra;

	data->length = sizeof(struct iw_range);
	memset(range, 0, sizeof(struct iw_range));

	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 21;
	range->retry_capa = IW_RETRY_LIMIT;
	range->retry_flags = IW_RETRY_LIMIT;
	range->min_retry = 0;
	range->max_retry = 255;
	range->min_rts = 0;
	range->max_rts = 2347;
	range->min_frag = 256;
	range->max_frag = 2346;

	range->encoding_size[0] = 5;
	range->encoding_size[1] = 13;
	range->num_encoding_sizes = 2;
	range->max_encoding_tokens = NUM_DEFAULT_KEYS;

	range->max_qual.qual = local->hw.max_signal;
	range->max_qual.level = local->hw.max_rssi;
	range->max_qual.noise = local->hw.max_noise;
	range->max_qual.updated = local->wstats_flags;

	range->avg_qual.qual = local->hw.max_signal/2;
	range->avg_qual.level = 0;
	range->avg_qual.noise = 0;
	range->avg_qual.updated = local->wstats_flags;

	range->enc_capa = IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2 |
			  IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP;

	IW_EVENT_CAPA_SET_KERNEL(range->event_capa);
	IW_EVENT_CAPA_SET(range->event_capa, SIOCGIWTHRSPY);
	IW_EVENT_CAPA_SET(range->event_capa, SIOCGIWAP);
	IW_EVENT_CAPA_SET(range->event_capa, SIOCGIWSCAN);

	return 0;
}


struct ieee80211_channel_range {
	short start_freq;
	short end_freq;
	unsigned char power_level;
	unsigned char antenna_max;
};

static const struct ieee80211_channel_range ieee80211_fcc_channels[] = {
	{ 2412, 2462, 27, 6 } /* IEEE 802.11b/g, channels 1..11 */,
	{ 5180, 5240, 17, 6 } /* IEEE 802.11a, channels 36..48 */,
	{ 5260, 5320, 23, 6 } /* IEEE 802.11a, channels 52..64 */,
	{ 5745, 5825, 30, 6 } /* IEEE 802.11a, channels 149..165, outdoor */,
	{ 0 }
};

static const struct ieee80211_channel_range ieee80211_mkk_channels[] = {
	{ 2412, 2472, 20, 6 } /* IEEE 802.11b/g, channels 1..13 */,
	{ 5170, 5240, 20, 6 } /* IEEE 802.11a, channels 34..48 */,
	{ 5260, 5320, 20, 6 } /* IEEE 802.11a, channels 52..64 */,
	{ 0 }
};


static const struct ieee80211_channel_range *channel_range =
	ieee80211_fcc_channels;


static void ieee80211_unmask_channel(struct net_device *dev, int mode,
				     struct ieee80211_channel *chan)
{
	int i;

	chan->flag = 0;

	if (ieee80211_regdom == 64 &&
	    (mode == MODE_ATHEROS_TURBO || mode == MODE_ATHEROS_TURBOG)) {
		/* Do not allow Turbo modes in Japan. */
		return;
	}

	for (i = 0; channel_range[i].start_freq; i++) {
		const struct ieee80211_channel_range *r = &channel_range[i];
		if (r->start_freq <= chan->freq && r->end_freq >= chan->freq) {
			if (ieee80211_regdom == 64 && !ieee80211_japan_5ghz &&
			    chan->freq >= 5260 && chan->freq <= 5320) {
				/*
				 * Skip new channels in Japan since the
				 * firmware was not marked having been upgraded
				 * by the vendor.
				 */
				continue;
			}

			if (ieee80211_regdom == 0x10 &&
			    (chan->freq == 5190 || chan->freq == 5210 ||
			     chan->freq == 5230)) {
				    /* Skip MKK channels when in FCC domain. */
				    continue;
			}

			chan->flag |= IEEE80211_CHAN_W_SCAN |
				IEEE80211_CHAN_W_ACTIVE_SCAN |
				IEEE80211_CHAN_W_IBSS;
			chan->power_level = r->power_level;
			chan->antenna_max = r->antenna_max;

			if (ieee80211_regdom == 64 &&
			    (chan->freq == 5170 || chan->freq == 5190 ||
			     chan->freq == 5210 || chan->freq == 5230)) {
				/*
				 * New regulatory rules in Japan have backwards
				 * compatibility with old channels in 5.15-5.25
				 * GHz band, but the station is not allowed to
				 * use active scan on these old channels.
				 */
				chan->flag &= ~IEEE80211_CHAN_W_ACTIVE_SCAN;
			}

			if (ieee80211_regdom == 64 &&
			    (chan->freq == 5260 || chan->freq == 5280 ||
			     chan->freq == 5300 || chan->freq == 5320)) {
				/*
				 * IBSS is not allowed on 5.25-5.35 GHz band
				 * due to radar detection requirements.
				 */
				chan->flag &= ~IEEE80211_CHAN_W_IBSS;
			}

			break;
		}
	}
}


static int ieee80211_unmask_channels(struct net_device *dev)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct ieee80211_hw_mode *mode;
	int c;

	list_for_each_entry(mode, &local->modes_list, list) {
		for (c = 0; c < mode->num_channels; c++) {
			ieee80211_unmask_channel(dev, mode->mode,
						 &mode->channels[c]);
		}
	}
	return 0;
}


int ieee80211_init_client(struct net_device *dev)
{
	if (ieee80211_regdom == 0x40)
		channel_range = ieee80211_mkk_channels;
	ieee80211_unmask_channels(dev);
	return 0;
}


static int ieee80211_ioctl_siwmode(struct net_device *dev,
				   struct iw_request_info *info,
				   __u32 *mode, char *extra)
{
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	int type;

	if (sdata->type == IEEE80211_IF_TYPE_VLAN)
		return -EOPNOTSUPP;

	switch (*mode) {
	case IW_MODE_INFRA:
		type = IEEE80211_IF_TYPE_STA;
		break;
	case IW_MODE_ADHOC:
		type = IEEE80211_IF_TYPE_IBSS;
		break;
	case IW_MODE_MONITOR:
		type = IEEE80211_IF_TYPE_MNTR;
		break;
	default:
		return -EINVAL;
	}

	if (type == sdata->type)
		return 0;
	if (netif_running(dev))
		return -EBUSY;

	ieee80211_if_reinit(dev);
	ieee80211_if_set_type(dev, type);

	return 0;
}


static int ieee80211_ioctl_giwmode(struct net_device *dev,
				   struct iw_request_info *info,
				   __u32 *mode, char *extra)
{
	struct ieee80211_sub_if_data *sdata;

	sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	switch (sdata->type) {
	case IEEE80211_IF_TYPE_AP:
		*mode = IW_MODE_MASTER;
		break;
	case IEEE80211_IF_TYPE_STA:
		*mode = IW_MODE_INFRA;
		break;
	case IEEE80211_IF_TYPE_IBSS:
		*mode = IW_MODE_ADHOC;
		break;
	case IEEE80211_IF_TYPE_MNTR:
		*mode = IW_MODE_MONITOR;
		break;
	case IEEE80211_IF_TYPE_WDS:
		*mode = IW_MODE_REPEAT;
		break;
	case IEEE80211_IF_TYPE_VLAN:
		*mode = IW_MODE_SECOND;		/* FIXME */
		break;
	default:
		*mode = IW_MODE_AUTO;
		break;
	}
	return 0;
}

int ieee80211_set_channel(struct ieee80211_local *local, int channel, int freq)
{
	struct ieee80211_hw_mode *mode;
	int c, set = 0;
	int ret = -EINVAL;

	list_for_each_entry(mode, &local->modes_list, list) {
		if (!(local->enabled_modes & (1 << mode->mode)))
			continue;
		for (c = 0; c < mode->num_channels; c++) {
			struct ieee80211_channel *chan = &mode->channels[c];
			if (chan->flag & IEEE80211_CHAN_W_SCAN &&
			    ((chan->chan == channel) || (chan->freq == freq))) {
				/* Use next_mode as the mode preference to
				 * resolve non-unique channel numbers. */
				if (set && mode->mode != local->next_mode)
					continue;

				local->oper_channel = chan;
				local->oper_hw_mode = mode;
				set++;
			}
		}
	}

	if (set) {
		if (local->sta_scanning)
			ret = 0;
		else
			ret = ieee80211_hw_config(local);

		rate_control_clear(local);
	}

	return ret;
}

static int ieee80211_ioctl_siwfreq(struct net_device *dev,
				   struct iw_request_info *info,
				   struct iw_freq *freq, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);

	if (sdata->type == IEEE80211_IF_TYPE_STA)
		sdata->u.sta.auto_channel_sel = 0;

	/* freq->e == 0: freq->m = channel; otherwise freq = m * 10^e */
	if (freq->e == 0) {
		if (freq->m < 0) {
			if (sdata->type == IEEE80211_IF_TYPE_STA)
				sdata->u.sta.auto_channel_sel = 1;
			return 0;
		} else
			return ieee80211_set_channel(local, freq->m, -1);
	} else {
		int i, div = 1000000;
		for (i = 0; i < freq->e; i++)
			div /= 10;
		if (div > 0)
			return ieee80211_set_channel(local, -1, freq->m / div);
		else
			return -EINVAL;
	}
}


static int ieee80211_ioctl_giwfreq(struct net_device *dev,
				   struct iw_request_info *info,
				   struct iw_freq *freq, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);

	/* TODO: in station mode (Managed/Ad-hoc) might need to poll low-level
	 * driver for the current channel with firmware-based management */

	freq->m = local->hw.conf.freq;
	freq->e = 6;

	return 0;
}


static int ieee80211_ioctl_siwessid(struct net_device *dev,
				    struct iw_request_info *info,
				    struct iw_point *data, char *ssid)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct ieee80211_sub_if_data *sdata;
	size_t len = data->length;

	/* iwconfig uses nul termination in SSID.. */
	if (len > 0 && ssid[len - 1] == '\0')
		len--;

	sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	if (sdata->type == IEEE80211_IF_TYPE_STA ||
	    sdata->type == IEEE80211_IF_TYPE_IBSS) {
		int ret;
		if (local->user_space_mlme) {
			if (len > IEEE80211_MAX_SSID_LEN)
				return -EINVAL;
			memcpy(sdata->u.sta.ssid, ssid, len);
			sdata->u.sta.ssid_len = len;
			return 0;
		}
		sdata->u.sta.auto_ssid_sel = !data->flags;
		ret = ieee80211_sta_set_ssid(dev, ssid, len);
		if (ret)
			return ret;
		ieee80211_sta_req_auth(dev, &sdata->u.sta);
		return 0;
	}

	if (sdata->type == IEEE80211_IF_TYPE_AP) {
		memcpy(sdata->u.ap.ssid, ssid, len);
		memset(sdata->u.ap.ssid + len, 0,
		       IEEE80211_MAX_SSID_LEN - len);
		sdata->u.ap.ssid_len = len;
		return ieee80211_if_config(dev);
	}
	return -EOPNOTSUPP;
}


static int ieee80211_ioctl_giwessid(struct net_device *dev,
				    struct iw_request_info *info,
				    struct iw_point *data, char *ssid)
{
	size_t len;

	struct ieee80211_sub_if_data *sdata;
	sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	if (sdata->type == IEEE80211_IF_TYPE_STA ||
	    sdata->type == IEEE80211_IF_TYPE_IBSS) {
		int res = ieee80211_sta_get_ssid(dev, ssid, &len);
		if (res == 0) {
			data->length = len;
			data->flags = 1;
		} else
			data->flags = 0;
		return res;
	}

	if (sdata->type == IEEE80211_IF_TYPE_AP) {
		len = sdata->u.ap.ssid_len;
		if (len > IW_ESSID_MAX_SIZE)
			len = IW_ESSID_MAX_SIZE;
		memcpy(ssid, sdata->u.ap.ssid, len);
		data->length = len;
		data->flags = 1;
		return 0;
	}
	return -EOPNOTSUPP;
}


static int ieee80211_ioctl_siwap(struct net_device *dev,
				 struct iw_request_info *info,
				 struct sockaddr *ap_addr, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct ieee80211_sub_if_data *sdata;

	sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	if (sdata->type == IEEE80211_IF_TYPE_STA ||
	    sdata->type == IEEE80211_IF_TYPE_IBSS) {
		int ret;
		if (local->user_space_mlme) {
			memcpy(sdata->u.sta.bssid, (u8 *) &ap_addr->sa_data,
			       ETH_ALEN);
			return 0;
		}
		if (is_zero_ether_addr((u8 *) &ap_addr->sa_data)) {
			sdata->u.sta.auto_bssid_sel = 1;
			sdata->u.sta.auto_channel_sel = 1;
		} else if (is_broadcast_ether_addr((u8 *) &ap_addr->sa_data))
			sdata->u.sta.auto_bssid_sel = 1;
		else
			sdata->u.sta.auto_bssid_sel = 0;
		ret = ieee80211_sta_set_bssid(dev, (u8 *) &ap_addr->sa_data);
		if (ret)
			return ret;
		ieee80211_sta_req_auth(dev, &sdata->u.sta);
		return 0;
	} else if (sdata->type == IEEE80211_IF_TYPE_WDS) {
		if (memcmp(sdata->u.wds.remote_addr, (u8 *) &ap_addr->sa_data,
			   ETH_ALEN) == 0)
			return 0;
		return ieee80211_if_update_wds(dev, (u8 *) &ap_addr->sa_data);
	}

	return -EOPNOTSUPP;
}


static int ieee80211_ioctl_giwap(struct net_device *dev,
				 struct iw_request_info *info,
				 struct sockaddr *ap_addr, char *extra)
{
	struct ieee80211_sub_if_data *sdata;

	sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	if (sdata->type == IEEE80211_IF_TYPE_STA ||
	    sdata->type == IEEE80211_IF_TYPE_IBSS) {
		ap_addr->sa_family = ARPHRD_ETHER;
		memcpy(&ap_addr->sa_data, sdata->u.sta.bssid, ETH_ALEN);
		return 0;
	} else if (sdata->type == IEEE80211_IF_TYPE_WDS) {
		ap_addr->sa_family = ARPHRD_ETHER;
		memcpy(&ap_addr->sa_data, sdata->u.wds.remote_addr, ETH_ALEN);
		return 0;
	}

	return -EOPNOTSUPP;
}


static int ieee80211_ioctl_siwscan(struct net_device *dev,
				   struct iw_request_info *info,
				   struct iw_point *data, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	u8 *ssid = NULL;
	size_t ssid_len = 0;

	if (!netif_running(dev))
		return -ENETDOWN;

	if (local->scan_flags & IEEE80211_SCAN_MATCH_SSID) {
		if (sdata->type == IEEE80211_IF_TYPE_STA ||
		    sdata->type == IEEE80211_IF_TYPE_IBSS) {
			ssid = sdata->u.sta.ssid;
			ssid_len = sdata->u.sta.ssid_len;
		} else if (sdata->type == IEEE80211_IF_TYPE_AP) {
			ssid = sdata->u.ap.ssid;
			ssid_len = sdata->u.ap.ssid_len;
		} else
			return -EINVAL;
	}
	return ieee80211_sta_req_scan(dev, ssid, ssid_len);
}


static int ieee80211_ioctl_giwscan(struct net_device *dev,
				   struct iw_request_info *info,
				   struct iw_point *data, char *extra)
{
	int res;
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	if (local->sta_scanning)
		return -EAGAIN;
	res = ieee80211_sta_scan_results(dev, extra, data->length);
	if (res >= 0) {
		data->length = res;
		return 0;
	}
	data->length = 0;
	return res;
}


static int ieee80211_ioctl_siwrts(struct net_device *dev,
				  struct iw_request_info *info,
				  struct iw_param *rts, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);

	if (rts->disabled)
		local->rts_threshold = IEEE80211_MAX_RTS_THRESHOLD;
	else if (rts->value < 0 || rts->value > IEEE80211_MAX_RTS_THRESHOLD)
		return -EINVAL;
	else
		local->rts_threshold = rts->value;

	/* If the wlan card performs RTS/CTS in hardware/firmware,
	 * configure it here */

	if (local->ops->set_rts_threshold)
		local->ops->set_rts_threshold(local_to_hw(local),
					     local->rts_threshold);

	return 0;
}

static int ieee80211_ioctl_giwrts(struct net_device *dev,
				  struct iw_request_info *info,
				  struct iw_param *rts, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);

	rts->value = local->rts_threshold;
	rts->disabled = (rts->value >= IEEE80211_MAX_RTS_THRESHOLD);
	rts->fixed = 1;

	return 0;
}


static int ieee80211_ioctl_siwfrag(struct net_device *dev,
				   struct iw_request_info *info,
				   struct iw_param *frag, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);

	if (frag->disabled)
		local->fragmentation_threshold = IEEE80211_MAX_FRAG_THRESHOLD;
	else if (frag->value < 256 ||
		 frag->value > IEEE80211_MAX_FRAG_THRESHOLD)
		return -EINVAL;
	else {
		/* Fragment length must be even, so strip LSB. */
		local->fragmentation_threshold = frag->value & ~0x1;
	}

	/* If the wlan card performs fragmentation in hardware/firmware,
	 * configure it here */

	if (local->ops->set_frag_threshold)
		local->ops->set_frag_threshold(
			local_to_hw(local),
			local->fragmentation_threshold);

	return 0;
}

static int ieee80211_ioctl_giwfrag(struct net_device *dev,
				   struct iw_request_info *info,
				   struct iw_param *frag, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);

	frag->value = local->fragmentation_threshold;
	frag->disabled = (frag->value >= IEEE80211_MAX_RTS_THRESHOLD);
	frag->fixed = 1;

	return 0;
}


static int ieee80211_ioctl_siwretry(struct net_device *dev,
				    struct iw_request_info *info,
				    struct iw_param *retry, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);

	if (retry->disabled ||
	    (retry->flags & IW_RETRY_TYPE) != IW_RETRY_LIMIT)
		return -EINVAL;

	if (retry->flags & IW_RETRY_MAX)
		local->long_retry_limit = retry->value;
	else if (retry->flags & IW_RETRY_MIN)
		local->short_retry_limit = retry->value;
	else {
		local->long_retry_limit = retry->value;
		local->short_retry_limit = retry->value;
	}

	if (local->ops->set_retry_limit) {
		return local->ops->set_retry_limit(
			local_to_hw(local),
			local->short_retry_limit,
			local->long_retry_limit);
	}

	return 0;
}


static int ieee80211_ioctl_giwretry(struct net_device *dev,
				    struct iw_request_info *info,
				    struct iw_param *retry, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);

	retry->disabled = 0;
	if (retry->flags == 0 || retry->flags & IW_RETRY_MIN) {
		/* first return min value, iwconfig will ask max value
		 * later if needed */
		retry->flags |= IW_RETRY_LIMIT;
		retry->value = local->short_retry_limit;
		if (local->long_retry_limit != local->short_retry_limit)
			retry->flags |= IW_RETRY_MIN;
		return 0;
	}
	if (retry->flags & IW_RETRY_MAX) {
		retry->flags = IW_RETRY_LIMIT | IW_RETRY_MAX;
		retry->value = local->long_retry_limit;
	}

	return 0;
}

static int ieee80211_ioctl_clear_keys(struct net_device *dev)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct ieee80211_key_conf key;
	int i;
	u8 addr[ETH_ALEN];
	struct ieee80211_key_conf *keyconf;
	struct ieee80211_sub_if_data *sdata;
	struct sta_info *sta;

	memset(addr, 0xff, ETH_ALEN);
	read_lock(&local->sub_if_lock);
	list_for_each_entry(sdata, &local->sub_if_list, list) {
		for (i = 0; i < NUM_DEFAULT_KEYS; i++) {
			keyconf = NULL;
			if (sdata->keys[i] &&
			    !sdata->keys[i]->force_sw_encrypt &&
			    local->ops->set_key &&
			    (keyconf = ieee80211_key_data2conf(local,
							       sdata->keys[i])))
				local->ops->set_key(local_to_hw(local),
						   DISABLE_KEY, addr,
						   keyconf, 0);
			kfree(keyconf);
			ieee80211_key_free(sdata->keys[i]);
			sdata->keys[i] = NULL;
		}
		sdata->default_key = NULL;
	}
	read_unlock(&local->sub_if_lock);

	spin_lock_bh(&local->sta_lock);
	list_for_each_entry(sta, &local->sta_list, list) {
		keyconf = NULL;
		if (sta->key && !sta->key->force_sw_encrypt &&
		    local->ops->set_key &&
		    (keyconf = ieee80211_key_data2conf(local, sta->key)))
			local->ops->set_key(local_to_hw(local), DISABLE_KEY,
					   sta->addr, keyconf, sta->aid);
		kfree(keyconf);
		ieee80211_key_free(sta->key);
		sta->key = NULL;
	}
	spin_unlock_bh(&local->sta_lock);

	memset(&key, 0, sizeof(key));
	if (local->ops->set_key &&
		    local->ops->set_key(local_to_hw(local), REMOVE_ALL_KEYS,
				       NULL, &key, 0))
		printk(KERN_DEBUG "%s: failed to remove hwaccel keys\n",
		       dev->name);

	return 0;
}


static int
ieee80211_ioctl_force_unicast_rate(struct net_device *dev,
				   struct ieee80211_sub_if_data *sdata,
				   int rate)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct ieee80211_hw_mode *mode;
	int i;

	if (sdata->type != IEEE80211_IF_TYPE_AP)
		return -ENOENT;

	if (rate == 0) {
		sdata->u.ap.force_unicast_rateidx = -1;
		return 0;
	}

	mode = local->oper_hw_mode;
	for (i = 0; i < mode->num_rates; i++) {
		if (mode->rates[i].rate == rate) {
			sdata->u.ap.force_unicast_rateidx = i;
			return 0;
		}
	}
	return -EINVAL;
}


static int
ieee80211_ioctl_max_ratectrl_rate(struct net_device *dev,
				  struct ieee80211_sub_if_data *sdata,
				  int rate)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct ieee80211_hw_mode *mode;
	int i;

	if (sdata->type != IEEE80211_IF_TYPE_AP)
		return -ENOENT;

	if (rate == 0) {
		sdata->u.ap.max_ratectrl_rateidx = -1;
		return 0;
	}

	mode = local->oper_hw_mode;
	for (i = 0; i < mode->num_rates; i++) {
		if (mode->rates[i].rate == rate) {
			sdata->u.ap.max_ratectrl_rateidx = i;
			return 0;
		}
	}
	return -EINVAL;
}


static void ieee80211_key_enable_hwaccel(struct ieee80211_local *local,
					 struct ieee80211_key *key)
{
	struct ieee80211_key_conf *keyconf;
	u8 addr[ETH_ALEN];

	if (!key || key->alg != ALG_WEP || !key->force_sw_encrypt ||
	    (local->hw.flags & IEEE80211_HW_DEVICE_HIDES_WEP))
		return;

	memset(addr, 0xff, ETH_ALEN);
	keyconf = ieee80211_key_data2conf(local, key);
	if (keyconf && local->ops->set_key &&
	    local->ops->set_key(local_to_hw(local),
			       SET_KEY, addr, keyconf, 0) == 0) {
		key->force_sw_encrypt =
			!!(keyconf->flags & IEEE80211_KEY_FORCE_SW_ENCRYPT);
		key->hw_key_idx = keyconf->hw_key_idx;
	}
	kfree(keyconf);
}


static void ieee80211_key_disable_hwaccel(struct ieee80211_local *local,
					  struct ieee80211_key *key)
{
	struct ieee80211_key_conf *keyconf;
	u8 addr[ETH_ALEN];

	if (!key || key->alg != ALG_WEP || key->force_sw_encrypt ||
	    (local->hw.flags & IEEE80211_HW_DEVICE_HIDES_WEP))
		return;

	memset(addr, 0xff, ETH_ALEN);
	keyconf = ieee80211_key_data2conf(local, key);
	if (keyconf && local->ops->set_key)
		local->ops->set_key(local_to_hw(local), DISABLE_KEY,
				   addr, keyconf, 0);
	kfree(keyconf);
	key->force_sw_encrypt = 1;
}


static int ieee80211_ioctl_default_wep_only(struct ieee80211_local *local,
					    int value)
{
	int i;
	struct ieee80211_sub_if_data *sdata;

	local->default_wep_only = value;
	read_lock(&local->sub_if_lock);
	list_for_each_entry(sdata, &local->sub_if_list, list)
		for (i = 0; i < NUM_DEFAULT_KEYS; i++)
			if (value)
				ieee80211_key_enable_hwaccel(local,
							     sdata->keys[i]);
			else
				ieee80211_key_disable_hwaccel(local,
							      sdata->keys[i]);
	read_unlock(&local->sub_if_lock);

	return 0;
}


void ieee80211_update_default_wep_only(struct ieee80211_local *local)
{
	int i = 0;
	struct ieee80211_sub_if_data *sdata;

	read_lock(&local->sub_if_lock);
	list_for_each_entry(sdata, &local->sub_if_list, list) {

		if (sdata->dev == local->mdev)
			continue;

		/* If there is an AP interface then depend on userspace to
		   set default_wep_only correctly. */
		if (sdata->type == IEEE80211_IF_TYPE_AP) {
			read_unlock(&local->sub_if_lock);
			return;
		}

		i++;
	}

	read_unlock(&local->sub_if_lock);

	if (i <= 1)
		ieee80211_ioctl_default_wep_only(local, 1);
	else
		ieee80211_ioctl_default_wep_only(local, 0);
}


static int ieee80211_ioctl_prism2_param(struct net_device *dev,
					struct iw_request_info *info,
					void *wrqu, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct ieee80211_sub_if_data *sdata;
	int *i = (int *) extra;
	int param = *i;
	int value = *(i + 1);
	int ret = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	sdata = IEEE80211_DEV_TO_SUB_IF(dev);

	switch (param) {
	case PRISM2_PARAM_IEEE_802_1X:
		if (local->ops->set_ieee8021x)
			ret = local->ops->set_ieee8021x(local_to_hw(local),
							value);
		if (ret)
			printk(KERN_DEBUG "%s: failed to set IEEE 802.1X (%d) "
			       "for low-level driver\n", dev->name, value);
		else
			sdata->ieee802_1x = value;
		break;

	case PRISM2_PARAM_ANTSEL_TX:
		local->hw.conf.antenna_sel_tx = value;
		if (ieee80211_hw_config(local))
			ret = -EINVAL;
		break;

	case PRISM2_PARAM_ANTSEL_RX:
		local->hw.conf.antenna_sel_rx = value;
		if (ieee80211_hw_config(local))
			ret = -EINVAL;
		break;

	case PRISM2_PARAM_CTS_PROTECT_ERP_FRAMES:
		local->cts_protect_erp_frames = value;
		break;

	case PRISM2_PARAM_DROP_UNENCRYPTED:
		sdata->drop_unencrypted = value;
		break;

	case PRISM2_PARAM_PREAMBLE:
		local->short_preamble = value;
		break;

	case PRISM2_PARAM_STAT_TIME:
		if (!local->stat_time && value) {
			local->stat_timer.expires = jiffies + HZ * value / 100;
			add_timer(&local->stat_timer);
		} else if (local->stat_time && !value) {
			del_timer_sync(&local->stat_timer);
		}
		local->stat_time = value;
		break;
	case PRISM2_PARAM_SHORT_SLOT_TIME:
		if (value)
			local->hw.conf.flags |= IEEE80211_CONF_SHORT_SLOT_TIME;
		else
			local->hw.conf.flags &= ~IEEE80211_CONF_SHORT_SLOT_TIME;
		if (ieee80211_hw_config(local))
			ret = -EINVAL;
		break;

	case PRISM2_PARAM_NEXT_MODE:
		local->next_mode = value;
		break;

	case PRISM2_PARAM_CLEAR_KEYS:
		ret = ieee80211_ioctl_clear_keys(dev);
		break;

	case PRISM2_PARAM_RADIO_ENABLED:
		ret = ieee80211_ioctl_set_radio_enabled(dev, value);
		break;

	case PRISM2_PARAM_ANTENNA_MODE:
		local->hw.conf.antenna_mode = value;
		if (ieee80211_hw_config(local))
			ret = -EINVAL;
		break;

	case PRISM2_PARAM_STA_ANTENNA_SEL:
		local->sta_antenna_sel = value;
		break;

	case PRISM2_PARAM_FORCE_UNICAST_RATE:
		ret = ieee80211_ioctl_force_unicast_rate(dev, sdata, value);
		break;

	case PRISM2_PARAM_MAX_RATECTRL_RATE:
		ret = ieee80211_ioctl_max_ratectrl_rate(dev, sdata, value);
		break;

	case PRISM2_PARAM_RATE_CTRL_NUM_UP:
		local->rate_ctrl_num_up = value;
		break;

	case PRISM2_PARAM_RATE_CTRL_NUM_DOWN:
		local->rate_ctrl_num_down = value;
		break;

	case PRISM2_PARAM_TX_POWER_REDUCTION:
		if (value < 0)
			ret = -EINVAL;
		else
			local->hw.conf.tx_power_reduction = value;
		break;

	case PRISM2_PARAM_KEY_TX_RX_THRESHOLD:
		local->key_tx_rx_threshold = value;
		break;

	case PRISM2_PARAM_DEFAULT_WEP_ONLY:
		ret = ieee80211_ioctl_default_wep_only(local, value);
		break;

	case PRISM2_PARAM_WIFI_WME_NOACK_TEST:
		local->wifi_wme_noack_test = value;
		break;

	case PRISM2_PARAM_SCAN_FLAGS:
		local->scan_flags = value;
		break;

	case PRISM2_PARAM_MIXED_CELL:
		if (sdata->type != IEEE80211_IF_TYPE_STA &&
		    sdata->type != IEEE80211_IF_TYPE_IBSS)
			ret = -EINVAL;
		else
			sdata->u.sta.mixed_cell = !!value;
		break;

	case PRISM2_PARAM_HW_MODES:
		local->enabled_modes = value;
		break;

	case PRISM2_PARAM_CREATE_IBSS:
		if (sdata->type != IEEE80211_IF_TYPE_IBSS)
			ret = -EINVAL;
		else
			sdata->u.sta.create_ibss = !!value;
		break;
	case PRISM2_PARAM_WMM_ENABLED:
		if (sdata->type != IEEE80211_IF_TYPE_STA &&
		    sdata->type != IEEE80211_IF_TYPE_IBSS)
			ret = -EINVAL;
		else
			sdata->u.sta.wmm_enabled = !!value;
		break;
	case PRISM2_PARAM_RADAR_DETECT:
		local->hw.conf.radar_detect = value;
		break;
	case PRISM2_PARAM_SPECTRUM_MGMT:
		local->hw.conf.spect_mgmt = value;
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}


static int ieee80211_ioctl_get_prism2_param(struct net_device *dev,
					    struct iw_request_info *info,
					    void *wrqu, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct ieee80211_sub_if_data *sdata;
	int *param = (int *) extra;
	int ret = 0;

	sdata = IEEE80211_DEV_TO_SUB_IF(dev);

	switch (*param) {
	case PRISM2_PARAM_IEEE_802_1X:
		*param = sdata->ieee802_1x;
		break;

	case PRISM2_PARAM_ANTSEL_TX:
		*param = local->hw.conf.antenna_sel_tx;
		break;

	case PRISM2_PARAM_ANTSEL_RX:
		*param = local->hw.conf.antenna_sel_rx;
		break;

	case PRISM2_PARAM_CTS_PROTECT_ERP_FRAMES:
		*param = local->cts_protect_erp_frames;
		break;

	case PRISM2_PARAM_DROP_UNENCRYPTED:
		*param = sdata->drop_unencrypted;
		break;

	case PRISM2_PARAM_PREAMBLE:
		*param = local->short_preamble;
		break;

	case PRISM2_PARAM_STAT_TIME:
		*param = local->stat_time;
		break;
	case PRISM2_PARAM_SHORT_SLOT_TIME:
		*param = !!(local->hw.conf.flags & IEEE80211_CONF_SHORT_SLOT_TIME);
		break;

	case PRISM2_PARAM_NEXT_MODE:
		*param = local->next_mode;
		break;

	case PRISM2_PARAM_ANTENNA_MODE:
		*param = local->hw.conf.antenna_mode;
		break;

	case PRISM2_PARAM_STA_ANTENNA_SEL:
		*param = local->sta_antenna_sel;
		break;

	case PRISM2_PARAM_RATE_CTRL_NUM_UP:
		*param = local->rate_ctrl_num_up;
		break;

	case PRISM2_PARAM_RATE_CTRL_NUM_DOWN:
		*param = local->rate_ctrl_num_down;
		break;

	case PRISM2_PARAM_TX_POWER_REDUCTION:
		*param = local->hw.conf.tx_power_reduction;
		break;

	case PRISM2_PARAM_KEY_TX_RX_THRESHOLD:
		*param = local->key_tx_rx_threshold;
		break;

	case PRISM2_PARAM_DEFAULT_WEP_ONLY:
		*param = local->default_wep_only;
		break;

	case PRISM2_PARAM_WIFI_WME_NOACK_TEST:
		*param = local->wifi_wme_noack_test;
		break;

	case PRISM2_PARAM_SCAN_FLAGS:
		*param = local->scan_flags;
		break;

	case PRISM2_PARAM_HW_MODES:
		*param = local->enabled_modes;
		break;

	case PRISM2_PARAM_CREATE_IBSS:
		if (sdata->type != IEEE80211_IF_TYPE_IBSS)
			ret = -EINVAL;
		else
			*param = !!sdata->u.sta.create_ibss;
		break;

	case PRISM2_PARAM_MIXED_CELL:
		if (sdata->type != IEEE80211_IF_TYPE_STA &&
		    sdata->type != IEEE80211_IF_TYPE_IBSS)
			ret = -EINVAL;
		else
			*param = !!sdata->u.sta.mixed_cell;
		break;
	case PRISM2_PARAM_WMM_ENABLED:
		if (sdata->type != IEEE80211_IF_TYPE_STA &&
		    sdata->type != IEEE80211_IF_TYPE_IBSS)
			ret = -EINVAL;
		else
			*param = !!sdata->u.sta.wmm_enabled;
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}

static int ieee80211_ioctl_siwmlme(struct net_device *dev,
				   struct iw_request_info *info,
				   struct iw_point *data, char *extra)
{
	struct ieee80211_sub_if_data *sdata;
	struct iw_mlme *mlme = (struct iw_mlme *) extra;

	sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	if (sdata->type != IEEE80211_IF_TYPE_STA &&
	    sdata->type != IEEE80211_IF_TYPE_IBSS)
		return -EINVAL;

	switch (mlme->cmd) {
	case IW_MLME_DEAUTH:
		/* TODO: mlme->addr.sa_data */
		return ieee80211_sta_deauthenticate(dev, mlme->reason_code);
	case IW_MLME_DISASSOC:
		/* TODO: mlme->addr.sa_data */
		return ieee80211_sta_disassociate(dev, mlme->reason_code);
	default:
		return -EOPNOTSUPP;
	}
}


static int ieee80211_ioctl_siwencode(struct net_device *dev,
				     struct iw_request_info *info,
				     struct iw_point *erq, char *keybuf)
{
	struct ieee80211_sub_if_data *sdata;
	int idx, i, alg = ALG_WEP;
	u8 bcaddr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	sdata = IEEE80211_DEV_TO_SUB_IF(dev);

	idx = erq->flags & IW_ENCODE_INDEX;
	if (idx == 0) {
		if (sdata->default_key)
			for (i = 0; i < NUM_DEFAULT_KEYS; i++) {
				if (sdata->default_key == sdata->keys[i]) {
					idx = i;
					break;
				}
			}
	} else if (idx < 1 || idx > 4)
		return -EINVAL;
	else
		idx--;

	if (erq->flags & IW_ENCODE_DISABLED)
		alg = ALG_NONE;
	else if (erq->length == 0) {
		/* No key data - just set the default TX key index */
		if (sdata->default_key != sdata->keys[idx]) {
			ieee80211_debugfs_key_remove_default(sdata);
			sdata->default_key = sdata->keys[idx];
			if (sdata->default_key)
				ieee80211_debugfs_key_add_default(sdata);
		}
		return 0;
	}

	return ieee80211_set_encryption(
		dev, bcaddr,
		idx, alg,
		!sdata->default_key,
		keybuf, erq->length);
}


static int ieee80211_ioctl_giwencode(struct net_device *dev,
				     struct iw_request_info *info,
				     struct iw_point *erq, char *key)
{
	struct ieee80211_sub_if_data *sdata;
	int idx, i;

	sdata = IEEE80211_DEV_TO_SUB_IF(dev);

	idx = erq->flags & IW_ENCODE_INDEX;
	if (idx < 1 || idx > 4) {
		idx = -1;
		if (!sdata->default_key)
			idx = 0;
		else for (i = 0; i < NUM_DEFAULT_KEYS; i++) {
			if (sdata->default_key == sdata->keys[i]) {
				idx = i;
				break;
			}
		}
		if (idx < 0)
			return -EINVAL;
	} else
		idx--;

	erq->flags = idx + 1;

	if (!sdata->keys[idx]) {
		erq->length = 0;
		erq->flags |= IW_ENCODE_DISABLED;
		return 0;
	}

	memcpy(key, sdata->keys[idx]->key,
	       min((int)erq->length, sdata->keys[idx]->keylen));
	erq->length = sdata->keys[idx]->keylen;
	erq->flags |= IW_ENCODE_ENABLED;

	return 0;
}

static int ieee80211_ioctl_siwauth(struct net_device *dev,
				   struct iw_request_info *info,
				   struct iw_param *data, char *extra)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	int ret = 0;

	switch (data->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
	case IW_AUTH_CIPHER_PAIRWISE:
	case IW_AUTH_CIPHER_GROUP:
	case IW_AUTH_WPA_ENABLED:
	case IW_AUTH_RX_UNENCRYPTED_EAPOL:
		break;
	case IW_AUTH_KEY_MGMT:
		if (sdata->type != IEEE80211_IF_TYPE_STA)
			ret = -EINVAL;
		else {
			/*
			 * TODO: sdata->u.sta.key_mgmt does not match with WE18
			 * value completely; could consider modifying this to
			 * be closer to WE18. For now, this value is not really
			 * used for anything else than Privacy matching, so the
			 * current code here should be more or less OK.
			 */
			if (data->value & IW_AUTH_KEY_MGMT_802_1X) {
				sdata->u.sta.key_mgmt =
					IEEE80211_KEY_MGMT_WPA_EAP;
			} else if (data->value & IW_AUTH_KEY_MGMT_PSK) {
				sdata->u.sta.key_mgmt =
					IEEE80211_KEY_MGMT_WPA_PSK;
			} else {
				sdata->u.sta.key_mgmt =
					IEEE80211_KEY_MGMT_NONE;
			}
		}
		break;
	case IW_AUTH_80211_AUTH_ALG:
		if (sdata->type == IEEE80211_IF_TYPE_STA ||
		    sdata->type == IEEE80211_IF_TYPE_IBSS)
			sdata->u.sta.auth_algs = data->value;
		else
			ret = -EOPNOTSUPP;
		break;
	case IW_AUTH_PRIVACY_INVOKED:
		if (local->ops->set_privacy_invoked)
			ret = local->ops->set_privacy_invoked(
					local_to_hw(local), data->value);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}
	return ret;
}

/* Get wireless statistics.  Called by /proc/net/wireless and by SIOCGIWSTATS */
static struct iw_statistics *ieee80211_get_wireless_stats(struct net_device *dev)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);
	struct iw_statistics *wstats = &local->wstats;
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	struct sta_info *sta = NULL;

	if (sdata->type == IEEE80211_IF_TYPE_STA ||
	    sdata->type == IEEE80211_IF_TYPE_IBSS)
		sta = sta_info_get(local, sdata->u.sta.bssid);
	if (!sta) {
		wstats->discard.fragment = 0;
		wstats->discard.misc = 0;
		wstats->qual.qual = 0;
		wstats->qual.level = 0;
		wstats->qual.noise = 0;
		wstats->qual.updated = IW_QUAL_ALL_INVALID;
	} else {
		wstats->qual.level = sta->last_rssi;
		wstats->qual.qual = sta->last_signal;
		wstats->qual.noise = sta->last_noise;
		wstats->qual.updated = local->wstats_flags;
		sta_info_put(sta);
	}
	return wstats;
}

static int ieee80211_ioctl_giwauth(struct net_device *dev,
				   struct iw_request_info *info,
				   struct iw_param *data, char *extra)
{
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	int ret = 0;

	switch (data->flags & IW_AUTH_INDEX) {
	case IW_AUTH_80211_AUTH_ALG:
		if (sdata->type == IEEE80211_IF_TYPE_STA ||
		    sdata->type == IEEE80211_IF_TYPE_IBSS)
			data->value = sdata->u.sta.auth_algs;
		else
			ret = -EOPNOTSUPP;
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}
	return ret;
}


static int ieee80211_ioctl_siwencodeext(struct net_device *dev,
					struct iw_request_info *info,
					struct iw_point *erq, char *extra)
{
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	struct iw_encode_ext *ext = (struct iw_encode_ext *) extra;
	int alg, idx, i;

	switch (ext->alg) {
	case IW_ENCODE_ALG_NONE:
		alg = ALG_NONE;
		break;
	case IW_ENCODE_ALG_WEP:
		alg = ALG_WEP;
		break;
	case IW_ENCODE_ALG_TKIP:
		alg = ALG_TKIP;
		break;
	case IW_ENCODE_ALG_CCMP:
		alg = ALG_CCMP;
		break;
	default:
		return -EOPNOTSUPP;
	}

	if (erq->flags & IW_ENCODE_DISABLED)
		alg = ALG_NONE;

	idx = erq->flags & IW_ENCODE_INDEX;
	if (idx < 1 || idx > 4) {
		idx = -1;
		if (!sdata->default_key)
			idx = 0;
		else for (i = 0; i < NUM_DEFAULT_KEYS; i++) {
			if (sdata->default_key == sdata->keys[i]) {
				idx = i;
				break;
			}
		}
		if (idx < 0)
			return -EINVAL;
	} else
		idx--;

	return ieee80211_set_encryption(dev, ext->addr.sa_data, idx, alg,
					ext->ext_flags &
					IW_ENCODE_EXT_SET_TX_KEY,
					ext->key, ext->key_len);
}


static const struct iw_priv_args ieee80211_ioctl_priv[] = {
	{ PRISM2_IOCTL_PRISM2_PARAM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "param" },
	{ PRISM2_IOCTL_GET_PRISM2_PARAM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_param" },
};

/* Structures to export the Wireless Handlers */

static const iw_handler ieee80211_handler[] =
{
	(iw_handler) NULL,				/* SIOCSIWCOMMIT */
	(iw_handler) ieee80211_ioctl_giwname,		/* SIOCGIWNAME */
	(iw_handler) NULL,				/* SIOCSIWNWID */
	(iw_handler) NULL,				/* SIOCGIWNWID */
	(iw_handler) ieee80211_ioctl_siwfreq,		/* SIOCSIWFREQ */
	(iw_handler) ieee80211_ioctl_giwfreq,		/* SIOCGIWFREQ */
	(iw_handler) ieee80211_ioctl_siwmode,		/* SIOCSIWMODE */
	(iw_handler) ieee80211_ioctl_giwmode,		/* SIOCGIWMODE */
	(iw_handler) NULL,				/* SIOCSIWSENS */
	(iw_handler) NULL,				/* SIOCGIWSENS */
	(iw_handler) NULL /* not used */,		/* SIOCSIWRANGE */
	(iw_handler) ieee80211_ioctl_giwrange,		/* SIOCGIWRANGE */
	(iw_handler) NULL /* not used */,		/* SIOCSIWPRIV */
	(iw_handler) NULL /* kernel code */,		/* SIOCGIWPRIV */
	(iw_handler) NULL /* not used */,		/* SIOCSIWSTATS */
	(iw_handler) NULL /* kernel code */,		/* SIOCGIWSTATS */
	iw_handler_set_spy,				/* SIOCSIWSPY */
	iw_handler_get_spy,				/* SIOCGIWSPY */
	iw_handler_set_thrspy,				/* SIOCSIWTHRSPY */
	iw_handler_get_thrspy,				/* SIOCGIWTHRSPY */
	(iw_handler) ieee80211_ioctl_siwap,		/* SIOCSIWAP */
	(iw_handler) ieee80211_ioctl_giwap,		/* SIOCGIWAP */
	(iw_handler) ieee80211_ioctl_siwmlme,		/* SIOCSIWMLME */
	(iw_handler) NULL,				/* SIOCGIWAPLIST */
	(iw_handler) ieee80211_ioctl_siwscan,		/* SIOCSIWSCAN */
	(iw_handler) ieee80211_ioctl_giwscan,		/* SIOCGIWSCAN */
	(iw_handler) ieee80211_ioctl_siwessid,		/* SIOCSIWESSID */
	(iw_handler) ieee80211_ioctl_giwessid,		/* SIOCGIWESSID */
	(iw_handler) NULL,				/* SIOCSIWNICKN */
	(iw_handler) NULL,				/* SIOCGIWNICKN */
	(iw_handler) NULL,				/* -- hole -- */
	(iw_handler) NULL,				/* -- hole -- */
	(iw_handler) NULL,				/* SIOCSIWRATE */
	(iw_handler) NULL,				/* SIOCGIWRATE */
	(iw_handler) ieee80211_ioctl_siwrts,		/* SIOCSIWRTS */
	(iw_handler) ieee80211_ioctl_giwrts,		/* SIOCGIWRTS */
	(iw_handler) ieee80211_ioctl_siwfrag,		/* SIOCSIWFRAG */
	(iw_handler) ieee80211_ioctl_giwfrag,		/* SIOCGIWFRAG */
	(iw_handler) NULL,				/* SIOCSIWTXPOW */
	(iw_handler) NULL,				/* SIOCGIWTXPOW */
	(iw_handler) ieee80211_ioctl_siwretry,		/* SIOCSIWRETRY */
	(iw_handler) ieee80211_ioctl_giwretry,		/* SIOCGIWRETRY */
	(iw_handler) ieee80211_ioctl_siwencode,		/* SIOCSIWENCODE */
	(iw_handler) ieee80211_ioctl_giwencode,		/* SIOCGIWENCODE */
	(iw_handler) NULL,				/* SIOCSIWPOWER */
	(iw_handler) NULL,				/* SIOCGIWPOWER */
	(iw_handler) NULL,				/* -- hole -- */
	(iw_handler) NULL,				/* -- hole -- */
	(iw_handler) ieee80211_ioctl_siwgenie,		/* SIOCSIWGENIE */
	(iw_handler) NULL,				/* SIOCGIWGENIE */
	(iw_handler) ieee80211_ioctl_siwauth,		/* SIOCSIWAUTH */
	(iw_handler) ieee80211_ioctl_giwauth,		/* SIOCGIWAUTH */
	(iw_handler) ieee80211_ioctl_siwencodeext,	/* SIOCSIWENCODEEXT */
	(iw_handler) NULL,				/* SIOCGIWENCODEEXT */
	(iw_handler) NULL,				/* SIOCSIWPMKSA */
	(iw_handler) NULL,				/* -- hole -- */
};

static const iw_handler ieee80211_private_handler[] =
{							/* SIOCIWFIRSTPRIV + */
	(iw_handler) ieee80211_ioctl_prism2_param,	/* 0 */
	(iw_handler) ieee80211_ioctl_get_prism2_param,	/* 1 */
};

const struct iw_handler_def ieee80211_iw_handler_def =
{
	.num_standard	= ARRAY_SIZE(ieee80211_handler),
	.num_private	= ARRAY_SIZE(ieee80211_private_handler),
	.num_private_args = ARRAY_SIZE(ieee80211_ioctl_priv),
	.standard	= (iw_handler *) ieee80211_handler,
	.private	= (iw_handler *) ieee80211_private_handler,
	.private_args	= (struct iw_priv_args *) ieee80211_ioctl_priv,
	.get_wireless_stats = ieee80211_get_wireless_stats,
};
