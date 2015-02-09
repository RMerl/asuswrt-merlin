/*
 * This file is part of wl1271
 *
 * Copyright (C) 2009 Nokia Corporation
 *
 * Contact: Luciano Coelho <luciano.coelho@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/gfp.h>

#include "wl1271.h"
#include "wl1271_acx.h"
#include "wl1271_reg.h"
#include "wl1271_rx.h"
#include "wl1271_io.h"

static u8 wl1271_rx_get_mem_block(struct wl1271_fw_status *status,
				  u32 drv_rx_counter)
{
	return le32_to_cpu(status->rx_pkt_descs[drv_rx_counter]) &
		RX_MEM_BLOCK_MASK;
}

static u32 wl1271_rx_get_buf_size(struct wl1271_fw_status *status,
				 u32 drv_rx_counter)
{
	return (le32_to_cpu(status->rx_pkt_descs[drv_rx_counter]) &
		RX_BUF_SIZE_MASK) >> RX_BUF_SIZE_SHIFT_DIV;
}

static void wl1271_rx_status(struct wl1271 *wl,
			     struct wl1271_rx_descriptor *desc,
			     struct ieee80211_rx_status *status,
			     u8 beacon)
{
	memset(status, 0, sizeof(struct ieee80211_rx_status));

	status->band = wl->band;
	status->rate_idx = wl1271_rate_to_idx(wl, desc->rate);

	status->signal = desc->rssi;

	wl->noise = desc->rssi - (desc->snr >> 1);

	status->freq = ieee80211_channel_to_frequency(desc->channel);

	if (desc->flags & WL1271_RX_DESC_ENCRYPT_MASK) {
		status->flag |= RX_FLAG_IV_STRIPPED | RX_FLAG_MMIC_STRIPPED;

		if (likely(!(desc->status & WL1271_RX_DESC_DECRYPT_FAIL)))
			status->flag |= RX_FLAG_DECRYPTED;
		if (unlikely(desc->status & WL1271_RX_DESC_MIC_FAIL))
			status->flag |= RX_FLAG_MMIC_ERROR;
	}
}

static void wl1271_rx_handle_data(struct wl1271 *wl, u32 length)
{
	struct ieee80211_rx_status rx_status;
	struct wl1271_rx_descriptor *desc;
	struct sk_buff *skb;
	u16 *fc;
	u8 *buf;
	u8 beacon = 0;

	if (unlikely(wl->state == WL1271_STATE_PLT))
		return;

	skb = __dev_alloc_skb(length, GFP_KERNEL);
	if (!skb) {
		wl1271_error("Couldn't allocate RX frame");
		return;
	}

	buf = skb_put(skb, length);
	wl1271_read(wl, WL1271_SLV_MEM_DATA, buf, length, true);

	/* the data read starts with the descriptor */
	desc = (struct wl1271_rx_descriptor *) buf;

	/* now we pull the descriptor out of the buffer */
	skb_pull(skb, sizeof(*desc));

	fc = (u16 *)skb->data;
	if ((*fc & IEEE80211_FCTL_STYPE) == IEEE80211_STYPE_BEACON)
		beacon = 1;

	wl1271_rx_status(wl, desc, &rx_status, beacon);

	wl1271_debug(DEBUG_RX, "rx skb 0x%p: %d B %s", skb, skb->len,
		     beacon ? "beacon" : "");

	skb_trim(skb, skb->len - desc->pad_len);

	memcpy(IEEE80211_SKB_RXCB(skb), &rx_status, sizeof(rx_status));
	ieee80211_rx_ni(wl->hw, skb);
}

void wl1271_rx(struct wl1271 *wl, struct wl1271_fw_status *status)
{
	struct wl1271_acx_mem_map *wl_mem_map = wl->target_mem_map;
	u32 buf_size;
	u32 fw_rx_counter  = status->fw_rx_counter & NUM_RX_PKT_DESC_MOD_MASK;
	u32 drv_rx_counter = wl->rx_counter & NUM_RX_PKT_DESC_MOD_MASK;
	u32 mem_block;

	while (drv_rx_counter != fw_rx_counter) {
		mem_block = wl1271_rx_get_mem_block(status, drv_rx_counter);
		buf_size = wl1271_rx_get_buf_size(status, drv_rx_counter);

		if (buf_size == 0) {
			wl1271_warning("received empty data");
			break;
		}

		wl->rx_mem_pool_addr.addr = (mem_block << 8) +
			le32_to_cpu(wl_mem_map->packet_memory_pool_start);
		wl->rx_mem_pool_addr.addr_extra =
			wl->rx_mem_pool_addr.addr + 4;

		/* Choose the block we want to read */
		wl1271_write(wl, WL1271_SLV_REG_DATA, &wl->rx_mem_pool_addr,
			     sizeof(wl->rx_mem_pool_addr), false);

		wl1271_rx_handle_data(wl, buf_size);

		wl->rx_counter++;
		drv_rx_counter = wl->rx_counter & NUM_RX_PKT_DESC_MOD_MASK;
	}

	wl1271_write32(wl, RX_DRIVER_COUNTER_ADDRESS, wl->rx_counter);
}
