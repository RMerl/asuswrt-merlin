/*
 * Copyright 2003-2005	Devicescape Software, Inc.
 * Copyright (c) 2006	Jiri Benc <jbenc@suse.cz>
 * Copyright 2007	Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/ieee80211.h>
#include "ieee80211_i.h"
#include "debugfs.h"
#include "debugfs_sta.h"
#include "sta_info.h"

/* sta attributtes */

#define STA_READ(name, buflen, field, format_string)			\
static ssize_t sta_ ##name## _read(struct file *file,			\
				   char __user *userbuf,		\
				   size_t count, loff_t *ppos)		\
{									\
	int res;							\
	struct sta_info *sta = file->private_data;			\
	char buf[buflen];						\
	res = scnprintf(buf, buflen, format_string, sta->field);	\
	return simple_read_from_buffer(userbuf, count, ppos, buf, res);	\
}
#define STA_READ_D(name, field) STA_READ(name, 20, field, "%d\n")
#define STA_READ_U(name, field) STA_READ(name, 20, field, "%u\n")
#define STA_READ_S(name, field) STA_READ(name, 20, field, "%s\n")

#define STA_OPS(name)							\
static const struct file_operations sta_ ##name## _ops = {		\
	.read = sta_##name##_read,					\
	.open = mac80211_open_file_generic,				\
}

#define STA_OPS_RW(name)						\
static const struct file_operations sta_ ##name## _ops = {		\
	.read = sta_##name##_read,					\
	.write = sta_##name##_write,					\
	.open = mac80211_open_file_generic,				\
}

#define STA_FILE(name, field, format)					\
		STA_READ_##format(name, field)				\
		STA_OPS(name)

STA_FILE(aid, sta.aid, D);
STA_FILE(dev, sdata->name, S);
STA_FILE(last_signal, last_signal, D);

static ssize_t sta_flags_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	char buf[100];
	struct sta_info *sta = file->private_data;
	u32 staflags = get_sta_flags(sta);
	int res = scnprintf(buf, sizeof(buf), "%s%s%s%s%s%s%s%s%s",
		staflags & WLAN_STA_AUTH ? "AUTH\n" : "",
		staflags & WLAN_STA_ASSOC ? "ASSOC\n" : "",
		staflags & WLAN_STA_PS_STA ? "PS (sta)\n" : "",
		staflags & WLAN_STA_PS_DRIVER ? "PS (driver)\n" : "",
		staflags & WLAN_STA_AUTHORIZED ? "AUTHORIZED\n" : "",
		staflags & WLAN_STA_SHORT_PREAMBLE ? "SHORT PREAMBLE\n" : "",
		staflags & WLAN_STA_WME ? "WME\n" : "",
		staflags & WLAN_STA_WDS ? "WDS\n" : "",
		staflags & WLAN_STA_MFP ? "MFP\n" : "");
	return simple_read_from_buffer(userbuf, count, ppos, buf, res);
}
STA_OPS(flags);

static ssize_t sta_num_ps_buf_frames_read(struct file *file,
					  char __user *userbuf,
					  size_t count, loff_t *ppos)
{
	char buf[20];
	struct sta_info *sta = file->private_data;
	int res = scnprintf(buf, sizeof(buf), "%u\n",
			    skb_queue_len(&sta->ps_tx_buf));
	return simple_read_from_buffer(userbuf, count, ppos, buf, res);
}
STA_OPS(num_ps_buf_frames);

static ssize_t sta_inactive_ms_read(struct file *file, char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	char buf[20];
	struct sta_info *sta = file->private_data;
	int res = scnprintf(buf, sizeof(buf), "%d\n",
			    jiffies_to_msecs(jiffies - sta->last_rx));
	return simple_read_from_buffer(userbuf, count, ppos, buf, res);
}
STA_OPS(inactive_ms);

static ssize_t sta_last_seq_ctrl_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	char buf[15*NUM_RX_DATA_QUEUES], *p = buf;
	int i;
	struct sta_info *sta = file->private_data;
	for (i = 0; i < NUM_RX_DATA_QUEUES; i++)
		p += scnprintf(p, sizeof(buf)+buf-p, "%x ",
			       le16_to_cpu(sta->last_seq_ctrl[i]));
	p += scnprintf(p, sizeof(buf)+buf-p, "\n");
	return simple_read_from_buffer(userbuf, count, ppos, buf, p - buf);
}
STA_OPS(last_seq_ctrl);

static ssize_t sta_agg_status_read(struct file *file, char __user *userbuf,
					size_t count, loff_t *ppos)
{
	char buf[71 + STA_TID_NUM * 40], *p = buf;
	int i;
	struct sta_info *sta = file->private_data;

	spin_lock_bh(&sta->lock);
	p += scnprintf(p, sizeof(buf) + buf - p, "next dialog_token: %#02x\n",
			sta->ampdu_mlme.dialog_token_allocator + 1);
	p += scnprintf(p, sizeof(buf) + buf - p,
		       "TID\t\tRX active\tDTKN\tSSN\t\tTX\tDTKN\tpending\n");
	for (i = 0; i < STA_TID_NUM; i++) {
		p += scnprintf(p, sizeof(buf) + buf - p, "%02d", i);
		p += scnprintf(p, sizeof(buf) + buf - p, "\t\t%x",
				!!sta->ampdu_mlme.tid_rx[i]);
		p += scnprintf(p, sizeof(buf) + buf - p, "\t%#.2x",
				sta->ampdu_mlme.tid_rx[i] ?
				sta->ampdu_mlme.tid_rx[i]->dialog_token : 0);
		p += scnprintf(p, sizeof(buf) + buf - p, "\t%#.3x",
				sta->ampdu_mlme.tid_rx[i] ?
				sta->ampdu_mlme.tid_rx[i]->ssn : 0);

		p += scnprintf(p, sizeof(buf) + buf - p, "\t\t%x",
				!!sta->ampdu_mlme.tid_tx[i]);
		p += scnprintf(p, sizeof(buf) + buf - p, "\t%#.2x",
				sta->ampdu_mlme.tid_tx[i] ?
				sta->ampdu_mlme.tid_tx[i]->dialog_token : 0);
		p += scnprintf(p, sizeof(buf) + buf - p, "\t%03d",
				sta->ampdu_mlme.tid_tx[i] ?
				skb_queue_len(&sta->ampdu_mlme.tid_tx[i]->pending) : 0);
		p += scnprintf(p, sizeof(buf) + buf - p, "\n");
	}
	spin_unlock_bh(&sta->lock);

	return simple_read_from_buffer(userbuf, count, ppos, buf, p - buf);
}

static ssize_t sta_agg_status_write(struct file *file, const char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	char _buf[12], *buf = _buf;
	struct sta_info *sta = file->private_data;
	bool start, tx;
	unsigned long tid;
	int ret;

	if (count > sizeof(_buf))
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[sizeof(_buf) - 1] = '\0';

	if (strncmp(buf, "tx ", 3) == 0) {
		buf += 3;
		tx = true;
	} else if (strncmp(buf, "rx ", 3) == 0) {
		buf += 3;
		tx = false;
	} else
		return -EINVAL;

	if (strncmp(buf, "start ", 6) == 0) {
		buf += 6;
		start = true;
		if (!tx)
			return -EINVAL;
	} else if (strncmp(buf, "stop ", 5) == 0) {
		buf += 5;
		start = false;
	} else
		return -EINVAL;

	tid = simple_strtoul(buf, NULL, 0);

	if (tid >= STA_TID_NUM)
		return -EINVAL;

	if (tx) {
		if (start)
			ret = ieee80211_start_tx_ba_session(&sta->sta, tid);
		else
			ret = ieee80211_stop_tx_ba_session(&sta->sta, tid);
	} else {
		__ieee80211_stop_rx_ba_session(sta, tid, WLAN_BACK_RECIPIENT, 3);
		ret = 0;
	}

	return ret ?: count;
}
STA_OPS_RW(agg_status);

static ssize_t sta_ht_capa_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
#define PRINT_HT_CAP(_cond, _str) \
	do { \
	if (_cond) \
			p += scnprintf(p, sizeof(buf)+buf-p, "\t" _str "\n"); \
	} while (0)
	char buf[512], *p = buf;
	int i;
	struct sta_info *sta = file->private_data;
	struct ieee80211_sta_ht_cap *htc = &sta->sta.ht_cap;

	p += scnprintf(p, sizeof(buf) + buf - p, "ht %ssupported\n",
			htc->ht_supported ? "" : "not ");
	if (htc->ht_supported) {
		p += scnprintf(p, sizeof(buf)+buf-p, "cap: %#.4x\n", htc->cap);

		PRINT_HT_CAP((htc->cap & BIT(0)), "RX LDPC");
		PRINT_HT_CAP((htc->cap & BIT(1)), "HT20/HT40");
		PRINT_HT_CAP(!(htc->cap & BIT(1)), "HT20");

		PRINT_HT_CAP(((htc->cap >> 2) & 0x3) == 0, "Static SM Power Save");
		PRINT_HT_CAP(((htc->cap >> 2) & 0x3) == 1, "Dynamic SM Power Save");
		PRINT_HT_CAP(((htc->cap >> 2) & 0x3) == 3, "SM Power Save disabled");

		PRINT_HT_CAP((htc->cap & BIT(4)), "RX Greenfield");
		PRINT_HT_CAP((htc->cap & BIT(5)), "RX HT20 SGI");
		PRINT_HT_CAP((htc->cap & BIT(6)), "RX HT40 SGI");
		PRINT_HT_CAP((htc->cap & BIT(7)), "TX STBC");

		PRINT_HT_CAP(((htc->cap >> 8) & 0x3) == 0, "No RX STBC");
		PRINT_HT_CAP(((htc->cap >> 8) & 0x3) == 1, "RX STBC 1-stream");
		PRINT_HT_CAP(((htc->cap >> 8) & 0x3) == 2, "RX STBC 2-streams");
		PRINT_HT_CAP(((htc->cap >> 8) & 0x3) == 3, "RX STBC 3-streams");

		PRINT_HT_CAP((htc->cap & BIT(10)), "HT Delayed Block Ack");

		PRINT_HT_CAP((htc->cap & BIT(11)), "Max AMSDU length: "
			     "3839 bytes");
		PRINT_HT_CAP(!(htc->cap & BIT(11)), "Max AMSDU length: "
			     "7935 bytes");

		/*
		 * For beacons and probe response this would mean the BSS
		 * does or does not allow the usage of DSSS/CCK HT40.
		 * Otherwise it means the STA does or does not use
		 * DSSS/CCK HT40.
		 */
		PRINT_HT_CAP((htc->cap & BIT(12)), "DSSS/CCK HT40");
		PRINT_HT_CAP(!(htc->cap & BIT(12)), "No DSSS/CCK HT40");

		/* BIT(13) is reserved */

		PRINT_HT_CAP((htc->cap & BIT(14)), "40 MHz Intolerant");

		PRINT_HT_CAP((htc->cap & BIT(15)), "L-SIG TXOP protection");

		p += scnprintf(p, sizeof(buf)+buf-p, "ampdu factor/density: %d/%d\n",
				htc->ampdu_factor, htc->ampdu_density);
		p += scnprintf(p, sizeof(buf)+buf-p, "MCS mask:");

		for (i = 0; i < IEEE80211_HT_MCS_MASK_LEN; i++)
			p += scnprintf(p, sizeof(buf)+buf-p, " %.2x",
					htc->mcs.rx_mask[i]);
		p += scnprintf(p, sizeof(buf)+buf-p, "\n");

		/* If not set this is meaningless */
		if (le16_to_cpu(htc->mcs.rx_highest)) {
			p += scnprintf(p, sizeof(buf)+buf-p,
				       "MCS rx highest: %d Mbps\n",
				       le16_to_cpu(htc->mcs.rx_highest));
		}

		p += scnprintf(p, sizeof(buf)+buf-p, "MCS tx params: %x\n",
				htc->mcs.tx_params);
	}

	return simple_read_from_buffer(userbuf, count, ppos, buf, p - buf);
}
STA_OPS(ht_capa);

#define DEBUGFS_ADD(name) \
	debugfs_create_file(#name, 0400, \
		sta->debugfs.dir, sta, &sta_ ##name## _ops);

#define DEBUGFS_ADD_COUNTER(name, field)				\
	if (sizeof(sta->field) == sizeof(u32))				\
		debugfs_create_u32(#name, 0400, sta->debugfs.dir,	\
			(u32 *) &sta->field);				\
	else								\
		debugfs_create_u64(#name, 0400, sta->debugfs.dir,	\
			(u64 *) &sta->field);

void ieee80211_sta_debugfs_add(struct sta_info *sta)
{
	struct dentry *stations_dir = sta->local->debugfs.stations;
	u8 mac[3*ETH_ALEN];

	sta->debugfs.add_has_run = true;

	if (!stations_dir)
		return;

	snprintf(mac, sizeof(mac), "%pM", sta->sta.addr);

	/*
	 * This might fail due to a race condition:
	 * When mac80211 unlinks a station, the debugfs entries
	 * remain, but it is already possible to link a new
	 * station with the same address which triggers adding
	 * it to debugfs; therefore, if the old station isn't
	 * destroyed quickly enough the old station's debugfs
	 * dir might still be around.
	 */
	sta->debugfs.dir = debugfs_create_dir(mac, stations_dir);
	if (!sta->debugfs.dir)
		return;

	DEBUGFS_ADD(flags);
	DEBUGFS_ADD(num_ps_buf_frames);
	DEBUGFS_ADD(inactive_ms);
	DEBUGFS_ADD(last_seq_ctrl);
	DEBUGFS_ADD(agg_status);
	DEBUGFS_ADD(dev);
	DEBUGFS_ADD(last_signal);
	DEBUGFS_ADD(ht_capa);

	DEBUGFS_ADD_COUNTER(rx_packets, rx_packets);
	DEBUGFS_ADD_COUNTER(tx_packets, tx_packets);
	DEBUGFS_ADD_COUNTER(rx_bytes, rx_bytes);
	DEBUGFS_ADD_COUNTER(tx_bytes, tx_bytes);
	DEBUGFS_ADD_COUNTER(rx_duplicates, num_duplicates);
	DEBUGFS_ADD_COUNTER(rx_fragments, rx_fragments);
	DEBUGFS_ADD_COUNTER(rx_dropped, rx_dropped);
	DEBUGFS_ADD_COUNTER(tx_fragments, tx_fragments);
	DEBUGFS_ADD_COUNTER(tx_filtered, tx_filtered_count);
	DEBUGFS_ADD_COUNTER(tx_retry_failed, tx_retry_failed);
	DEBUGFS_ADD_COUNTER(tx_retry_count, tx_retry_count);
	DEBUGFS_ADD_COUNTER(wep_weak_iv_count, wep_weak_iv_count);
}

void ieee80211_sta_debugfs_remove(struct sta_info *sta)
{
	debugfs_remove_recursive(sta->debugfs.dir);
	sta->debugfs.dir = NULL;
}
