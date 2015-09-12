/*
 * Open Loop phy calibration SW module for
 * Broadcom 802.11bang Networking Device Driver
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_olpc_engine.c 377976 2013-01-23 20:57:10Z $
 */

#include <wlc_cfg.h>
#ifdef WLOLPC
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <pcicfg.h>
#include <siutils.h>
#include <bcmendian.h>
#include <nicpci.h>
#include <wlioctl.h>
#include <pcie_core.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_bmac.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <wl_dbg.h>

#include <wlc_olpc_engine.h>
#include <wlc_channel.h>
#include <wlc_stf.h>
#include <wlc_scan.h>
#include <wlc_rm.h>
#include <wlc_phy_int.h>
#include <wlc_pcb.h>

#define WLC_OLPC_DEF_NPKTS 16
#define WL_OLPC_PKT_LEN (sizeof(struct dot11_header)+1)

#if defined(BCMDBG) || defined(WLTEST)
#define OLPC_MSG_LVL_IMPORTANT 1
#define OLPC_MSG_LVL_CHATTY 2
#define OLPC_MSG_LVL_ENTRY 4
#define WL_OLPC(olpc, args) if (olpc && (olpc->msglevel & \
	OLPC_MSG_LVL_IMPORTANT) != 0) WL_PRINT(args)
#define WL_OLPC_DBG(olpc, args) if (olpc && (olpc->msglevel & \
	OLPC_MSG_LVL_CHATTY) != 0) WL_PRINT(args)
#define WL_OLPC_ENTRY(olpc, args) if (olpc && (olpc->msglevel & \
		OLPC_MSG_LVL_ENTRY) != 0) WL_PRINT(args)
#else
#define WL_OLPC(olpc, args)
#define WL_OLPC_DBG(olpc, args)
#define WL_OLPC_ENTRY(olpc, args)
#endif /* defined(BCMDBG) || defined(WLTEST) */
/* max num of cores supported */
#define WL_OLPC_MAX_NUM_CORES 3
/* max valueof core mask with one core is 0x4 */
#define WL_OLPC_MAX_CORE 4
#define WL_OLPC_ANT_TO_CIDX(core) ((core == WL_OLPC_MAX_CORE) ? \
	(WL_OLPC_MAX_NUM_CORES-1) : (core - 1))

/* Need to shave 1.5 (6 coz units are .25 dBm) off ppr power to comp TSSI pwr */
#define WL_OLPC_PPR_TO_TSSI_PWR(pwr) (pwr - 6)

#define WLC_TO_CHANSPEC(wlc) (wlc->chanspec)
#define WLC_OLPC_INVALID_TXCHAIN 0xff

#if defined(BCMDBG) || defined(WLTEST)
#define WL_OLPC_IOVARS_ENAB 1
#else
#define WL_OLPC_IOVARS_ENAB 0
#endif /* BCMDBG || WLTEST */

#if WL_OLPC_IOVARS_ENAB
enum {
	IOV_OLPC = 0,
	IOV_OLPC_CAL_ST,
	IOV_OLPC_FORCE_CAL,
	IOV_OLPC_MSG_LVL,
	IOV_OLPC_CHAN,
	IOV_OLPC_CAL_PKTS,
	IOV_OLPC_CHAN_DBG,
	IOV_OLPC_LAST
};
#endif /* WL_OLPC_IOVARS_ENAB */

static const bcm_iovar_t olpc_iovars[] = {
#if WL_OLPC_IOVARS_ENAB
	{"olpc", IOV_OLPC, 0, IOVT_UINT32, 0},
	{"olpc_cal_mask", IOV_OLPC_CAL_ST, IOVF_SET_UP | IOVF_GET_UP, IOVT_UINT32, 0},
	{"olpc_cal_force", IOV_OLPC_FORCE_CAL, IOVF_GET_UP, IOVT_UINT32, 0},
	{"olpc_msg_lvl", IOV_OLPC_MSG_LVL, 0, IOVT_UINT32, 0},
	{"olpc_chan", IOV_OLPC_CHAN, IOVF_GET_UP, IOVT_UINT32, 0},
	{"olpc_cal_pkts_mask", IOV_OLPC_CAL_PKTS, IOVF_GET_UP, IOVT_UINT32, 0},
	{"olpc_chan_dbg", IOV_OLPC_CHAN_DBG, 0, IOVT_UINT32, 0},
#endif /* WL_OLPC_IOVARS_ENAB */
	{NULL, 0, 0, 0, 0}
};

typedef struct wlc_olpc_eng_chan {
	struct wlc_olpc_eng_chan *next;
	chanspec_t cspec; /* chanspec, containing chan number the struct refers to */
	uint8 pkts_sent[WL_OLPC_MAX_NUM_CORES];
	uint8 cores_cal; /* which cores have been calibrated */
	uint8 cores_cal_active; /* which cores have active cal */
	uint8 cores_cal_to_cmplt; /* cores we just calibrated */
	uint8 cal_pkts_outstanding; /* pkts outstanding on the chan */
#if WL_OLPC_IOVARS_ENAB
	uint8 cores_cal_pkts_sent; /* mask of cal pkts sent  and txstatus cmplted */
	bool dbg_mode; /* TRUE: ignore cal cache b4 telling phy cal is done; FALSE:  don't ignore */
#endif /* WL_OLPC_IOVARS_ENAB */
} wlc_olpc_eng_chan_t;

struct wlc_olpc_eng_info_t {
	wlc_info_t *wlc;
	/* status info */
	wlc_olpc_eng_chan_t *chan_list;
	bool up;
	bool tx_cal_pkts;
	bool updn; /* bsscfg updown cb registered */

	/* configuration */
	uint8 npkts; /* number of pkts used to calibrate per core */
	uint8 num_hw_cores; /* save this value to avoid repetitive calls BITSCNT */

	/* txcore mgmt */
	bool restore_perrate_stf_state;
	wlc_stf_txchain_st stf_saved_perrate;

	/* OLPC PPR cache */
	uint8 last_txchain_chk; /* txchain used for last ppr query */
	chanspec_t last_chan_chk; /* chanspec used for last ppr query */
	bool last_chan_olpc_state; /* cache result of ppr query */

	/* track our current channel - detect channel changes */
	wlc_olpc_eng_chan_t* cur_chan;

	uint8 msglevel;
	bool assoc_notif_reg; /* bsscfg assoc state chg notif reg */
};

/* static functions and internal defines */
#define WLC_TO_CHANSPEC(wlc) (wlc->chanspec)

/* use own mac addr for src and dest */
#define WLC_MACADDR(wlc) (char*)(wlc->pub->cur_etheraddr.octet)

static bool
wlc_olpc_chan_needs_cal(wlc_olpc_eng_info_t *olpc, wlc_olpc_eng_chan_t* chan);

static uint8
wlc_olpc_chan_num_cores_cal_needed(wlc_info_t *wlc, wlc_olpc_eng_chan_t* chan);

static int
wlc_olpc_send_dummy_pkts(wlc_info_t *wlc,
	wlc_olpc_eng_chan_t* chan, uint8 npkts);

static int wlc_olpc_eng_recal_ex(wlc_info_t *wlc, uint8 npkts);

static int wlc_olpc_stf_override(wlc_info_t *wlc);

static void wlc_olpc_stf_perrate_changed(wlc_info_t *wlc);

/* EVENT: one calibration packet just completed tx */
static void wlc_olpc_eng_pkt_complete(wlc_info_t *wlc, void *pkt, uint txs);

static wlc_olpc_eng_chan_t*
wlc_olpc_get_chan(wlc_info_t *wlc, chanspec_t cspec, int *err);

static wlc_olpc_eng_chan_t*
wlc_olpc_get_chan_ex(wlc_info_t *wlc, chanspec_t cspec, int *err, bool create);

static INLINE bool
wlc_olpc_chan_needs_olpc(wlc_olpc_eng_info_t *olpc, chanspec_t cspec);

static void wlc_olpc_chan_terminate_active_cal(wlc_info_t *wlc,
	wlc_olpc_eng_chan_t* cur_chan);

static int wlc_olpc_eng_terminate_cal(wlc_info_t *wlc);

static bool wlc_olpc_chan_has_active_cal(wlc_olpc_eng_info_t *olpc,
	wlc_olpc_eng_chan_t* chan);
static int wlc_olpc_stf_override_revert(wlc_info_t *wlc);

static bool wlc_olpc_eng_ready(wlc_olpc_eng_info_t *olpc_info);

static int wlc_olpc_eng_hdl_chan_update_ex(wlc_info_t *wlc, uint8 npkts, bool force_cal);

static int wlc_olpc_eng_up(void *hdl);

static int wlc_olpc_eng_down(void *hdl);

static void wlc_olpc_bsscfg_updn(void *ctx, bsscfg_up_down_event_data_t *evt);
static void
wlc_olpc_bsscfg_assoc_state_notif(void *arg, bss_assoc_state_data_t *notif_data);

#if defined(BCMDBG) || defined(WLTEST)
static void wlc_olpc_dump_channels(wlc_olpc_eng_info_t *olpc_info, struct bcmstrbuf *b);
static int wlc_dump_olpc(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* defined(BCMDBG) || defined(WLTEST) */

static int wlc_olpc_get_min_2ss3ss_sdm_pwr(wlc_olpc_eng_info_t *olpc_info,
	ppr_t *txpwr, chanspec_t channel);

static bool wlc_olpc_in_cal_prohibit_state(wlc_info_t *wlc);

static int wlc_olpc_chan_rm(wlc_olpc_eng_info_t *olpc,
	wlc_olpc_eng_chan_t* chan);

static bool
wlc_olpc_find_other_up_on_chan(wlc_info_t *wlc, wlc_bsscfg_t *exclude);

/* check if wlc is in state which prohibits calibration */
static bool wlc_olpc_in_cal_prohibit_state(wlc_info_t *wlc)
{
	return (SCAN_IN_PROGRESS(wlc->scan) || WLC_RM_IN_PROGRESS(wlc));
}

int
wlc_olpc_get_cal_state(wlc_olpc_eng_info_t *olpc)
{
	int err;
	int cores_cal = -1;
	wlc_info_t *wlc	= olpc->wlc;
	wlc_olpc_eng_chan_t* chan_info = NULL;
	chanspec_t cspec = wlc->chanspec;

	if (wlc_olpc_chan_needs_olpc(olpc, wlc->chanspec)) {
		chan_info = wlc_olpc_get_chan_ex(wlc, cspec, &err, FALSE);
		if (chan_info) {
			cores_cal = chan_info->cores_cal;
		} else {
			cores_cal = 0;
		}
	}

	return cores_cal;
}

static void
wlc_olpc_bsscfg_assoc_state_notif(void *arg, bss_assoc_state_data_t *notif_data)
{
	if (notif_data->state == AS_JOIN_ADOPT) {
		wlc_olpc_eng_info_t *olpc = (wlc_olpc_eng_info_t *)arg;

		WL_OLPC_DBG(olpc,
			("%s: JOIN_ADOPT state\n", __FUNCTION__));
		wlc_olpc_eng_hdl_chan_update(olpc);
	}
}

static void wlc_olpc_bsscfg_updn(void *ctx, bsscfg_up_down_event_data_t *evt)
{
	/* seeing a bsscfg up or down */
	wlc_olpc_eng_info_t *olpc = (wlc_olpc_eng_info_t *)ctx;
	wlc_info_t* wlc;
	wlc_olpc_eng_chan_t* chan;
	chanspec_t cspec;
	int err;
	ASSERT(ctx != NULL);
	ASSERT(evt != NULL);
	WL_OLPC_ENTRY(olpc, ("Entry:%s\n", __FUNCTION__));

	wlc = olpc->wlc;
	/* if any bsscfg just went up, then force calibrate on channel */
	if (evt->up) {
		WL_OLPC_ENTRY(olpc, ("%s: cfg (%p) up on chan now\n", __FUNCTION__, evt->bsscfg));
		wlc_olpc_eng_hdl_chan_update_ex(olpc->wlc, olpc->npkts, TRUE);
	} else if (!wlc_olpc_find_other_up_on_chan(olpc->wlc, evt->bsscfg)) {
		WL_OLPC_ENTRY(olpc, ("No cfg up on chan now:%s\n", __FUNCTION__));
		/* going down -- free unused channel info */
		if (evt->bsscfg->current_bss) {
			cspec = evt->bsscfg->current_bss->chanspec;
			chan = wlc_olpc_get_chan_ex(wlc, cspec, &err, FALSE);
			if (chan) {
				WL_OLPC_ENTRY(olpc, ("FREEing chan struct:%s\n", __FUNCTION__));
				wlc_olpc_chan_rm(olpc, chan);
			}
		}
	}
	WL_OLPC_ENTRY(olpc, ("Exit:%s\n", __FUNCTION__));

}

static void
wlc_olpc_chan_terminate_active_cal(wlc_info_t *wlc, wlc_olpc_eng_chan_t* cur_chan)
{
	wlc_olpc_stf_override_revert(wlc);
	bzero(cur_chan->pkts_sent, sizeof(cur_chan->pkts_sent));
	cur_chan->cores_cal_active = 0;
	/* if we hadn't yet saved cal result, forget we calibrated */
	cur_chan->cores_cal &= ~cur_chan->cores_cal_to_cmplt;
	cur_chan->cores_cal_to_cmplt = 0;
#if WL_OLPC_IOVARS_ENAB
	cur_chan->cores_cal_pkts_sent = 0;
#endif /* WL_OLPC_IOVARS_ENAB */
}

static int
wlc_olpc_eng_terminate_cal(wlc_info_t *wlc)
{
	struct wlc_olpc_eng_info_t* olpc_info = wlc->olpc_info;

	/* if on olpc chan + [wlc chanspec changed or we're in scan/rm] + an active calibration */
	if (olpc_info->cur_chan &&
		(olpc_info->cur_chan->cspec != wlc->chanspec ||
		wlc_olpc_in_cal_prohibit_state(wlc)) &&
		wlc_olpc_chan_has_active_cal(wlc->olpc_info, olpc_info->cur_chan)) {
		wlc_olpc_chan_terminate_active_cal(wlc, olpc_info->cur_chan);
	}
	return BCME_OK;
}

/* return TRUE iff txchain has cores that need calibrating */
static bool
wlc_olpc_chan_needs_cal(wlc_olpc_eng_info_t *olpc, wlc_olpc_eng_chan_t* chan)
{
	/* if cores not calibrated nor calibrating (~(chan->cores_cal | chan->cores_cal_active)) */
	/* that are in txchain (& olpc->wlc->stf->txchain), then we need some calibration */
	return (~(chan->cores_cal | chan->cores_cal_active) & olpc->wlc->stf->txchain) != 0;
}

static bool
wlc_olpc_chan_has_active_cal(wlc_olpc_eng_info_t *olpc, wlc_olpc_eng_chan_t* chan)
{
	return ((chan->cores_cal_active) != 0);
}

static void
wlc_olpc_stf_perrate_changed(wlc_info_t *wlc)
{
	if (wlc->olpc_info) {
		WL_OLPC_DBG(wlc->olpc_info, ("prerrate chged not restore\n"));
		wlc->olpc_info->restore_perrate_stf_state = FALSE;
	}
}

bool
wlc_olpc_eng_has_active_cal(wlc_olpc_eng_info_t *olpc)
{
	if (!wlc_olpc_eng_ready(olpc)) {
		return FALSE;
	}

	return (olpc->cur_chan && wlc_olpc_chan_has_active_cal(olpc, olpc->cur_chan));
}

static int
wlc_olpc_stf_override(wlc_info_t *wlc)
{
	int err = BCME_OK;
	uint8 tgt_chains = wlc->stf->txchain;

	if ((wlc->stf->txcore_override[OFDM_IDX] == tgt_chains &&
		wlc->stf->txcore_override[CCK_IDX] == tgt_chains)) {
		/* NO-OP */
		return err;
	}
	if (!wlc->olpc_info->restore_perrate_stf_state) {
		wlc_stf_txchain_get_perrate_state(wlc, &(wlc->olpc_info->stf_saved_perrate),
			wlc_olpc_stf_perrate_changed);
	}

	wlc->olpc_info->restore_perrate_stf_state = TRUE;

	/* set value to have all cores on */
	/* we may use OFDM or CCK for cal pkts */
	WL_OLPC_DBG(wlc->olpc_info, ("%s: txcore_override current[ofdm,cck]=[%x,%x] override=%d\n",
		__FUNCTION__, wlc->stf->txcore_override[OFDM_IDX],
		wlc->stf->txcore_override[CCK_IDX],
		tgt_chains));
	wlc->stf->txcore_override[OFDM_IDX] = tgt_chains;
	wlc->stf->txcore_override[CCK_IDX] = tgt_chains;

	wlc_stf_spatial_policy_set(wlc, wlc->stf->spatialpolicy);

	return err;
}

/* called when calibration is done */
static int
wlc_olpc_stf_override_perrate_revert(wlc_info_t *wlc)
{
	if (wlc->olpc_info->restore_perrate_stf_state) {
		wlc->olpc_info->restore_perrate_stf_state = FALSE;
		wlc_stf_txchain_restore_perrate_state(wlc, &(wlc->olpc_info->stf_saved_perrate));
	}
	return BCME_OK;
}

/* called when calibration is done - if stf saved state still valid, restore it */
static int
wlc_olpc_stf_override_revert(wlc_info_t *wlc)
{
	/* restore override on txcore */
	wlc_olpc_stf_override_perrate_revert(wlc);
	/* txchain is not changed */
	return BCME_OK;
}

#define WL_OLPC_BW_ITER_HAS_NEXT(txbw) (txbw != WL_TX_BW_ALL)

static wl_tx_bw_t
wlc_olpc_bw_iter_next(wl_tx_bw_t txbw)
{
	wl_tx_bw_t ret;

	switch (txbw) {
		case WL_TX_BW_40:
			ret = WL_TX_BW_20IN40;
		break;
		case WL_TX_BW_80:
			ret = WL_TX_BW_40IN80;
		break;
		case WL_TX_BW_40IN80:
			ret = WL_TX_BW_20IN80;
		break;
		case WL_TX_BW_160:
			ret = WL_TX_BW_80IN160;
		break;
		case WL_TX_BW_80IN160:
			ret = WL_TX_BW_40IN160;
		break;
		case WL_TX_BW_40IN160:
			ret = WL_TX_BW_20IN160;
		break;
		case WL_TX_BW_20IN40:
		case WL_TX_BW_20IN80:
		case WL_TX_BW_20:
		case WL_TX_BW_20IN160:
			ret = WL_TX_BW_ALL;
		break;
		default:
			ASSERT(0);
			ret = WL_TX_BW_ALL;
		break;
	}
	return ret;
}

static int
wlc_olpc_get_min_2ss3ss_sdm_pwr(wlc_olpc_eng_info_t *olpc,
	ppr_t *txpwr, chanspec_t channel)
{
	int8 sdmmin;
	int min_txpwr_limit = 0xffff;
	int err = 0;
	uint txchains = WLC_BITSCNT(olpc->wlc->stf->txchain);
	wl_tx_bw_t txbw = PPR_CHSPEC_BW(channel);

	while (WL_OLPC_BW_ITER_HAS_NEXT(txbw)) {
		/* get min of 2x2 */
		if (txchains > 1) {
			err = ppr_get_vht_mcs_min(txpwr, txbw,
				WL_TX_NSS_2, WL_TX_MODE_NONE, WL_TX_CHAINS_2, &sdmmin);

			if (err == BCME_OK) {
				WL_OLPC_ENTRY(olpc, ("wl%d:%s: nss2 min=%d sdmmin=%d\n",
					olpc->wlc->pub->unit, __FUNCTION__,
					min_txpwr_limit, sdmmin));

				if (sdmmin != WL_RATE_DISABLED) {
					min_txpwr_limit = MIN(min_txpwr_limit, sdmmin);
				}
			}
		}
		/* get min of 3x3 */
		if (txchains > 2) {
			err = ppr_get_vht_mcs_min(txpwr, txbw,
				WL_TX_NSS_3, WL_TX_MODE_NONE, WL_TX_CHAINS_3, &sdmmin);
			if (err == BCME_OK) {
				WL_OLPC_ENTRY(olpc, ("wl%d:%s: nss3 min=%d sdmmin=%d\n",
					olpc->wlc->pub->unit, __FUNCTION__,
					min_txpwr_limit, sdmmin));
				if (sdmmin != WL_RATE_DISABLED) {
					min_txpwr_limit = MIN(min_txpwr_limit, sdmmin);
				}
			}
		}
		txbw = wlc_olpc_bw_iter_next(txbw);
	}

	return min_txpwr_limit;
}

/* use ppr to find min tgt power (2ss/3ss sdm) in .25dBm units */
static int
wlc_olpc_get_min_tgt_pwr(wlc_olpc_eng_info_t *olpc, chanspec_t channel)
{
	wlc_info_t* wlc = olpc->wlc;
	int cur_min = 0xFFFF;
	wlc_phy_t *pi = wlc->band->pi;
	ppr_t *txpwr;
	ppr_t *srommax;
	int8 min_srom;
	if ((txpwr = ppr_create(wlc->pub->osh, PPR_CHSPEC_BW(channel))) == NULL) {
		return WL_RATE_DISABLED;
	}
	if ((srommax = ppr_create(wlc->pub->osh, PPR_CHSPEC_BW(channel))) == NULL) {
		ppr_delete(wlc->pub->osh, txpwr);
		return WL_RATE_DISABLED;
	}
	/* use the control channel to get the regulatory limits and srom max/min */
	wlc_channel_reg_limits(wlc->cmi, channel, txpwr);

	wlc_phy_txpower_sromlimit(pi, channel, (uint8*)&min_srom, srommax, 0);

	/* bound the regulatory limit by srom min/max */
	ppr_apply_vector_ceiling(txpwr, srommax);

	/* apply min and  then disable powers below min */
	ppr_apply_min(txpwr, min_srom);
	ppr_force_disabled(txpwr, min_srom);

	WL_NONE(("min_srom %d\n", min_srom));

	cur_min = wlc_olpc_get_min_2ss3ss_sdm_pwr(olpc, txpwr, channel);

	ppr_delete(wlc->pub->osh, srommax);
	ppr_delete(wlc->pub->osh, txpwr);

	return (int)(cur_min);
}

/* is channel one that needs open loop phy cal? */
static INLINE bool
wlc_olpc_chan_needs_olpc(wlc_olpc_eng_info_t *olpc, chanspec_t chan)
{
	int pwr, tssi_thresh;

	if (olpc && olpc->wlc->band && olpc->wlc->band->pi) {
		if (olpc->last_chan_chk != chan ||
			olpc->last_txchain_chk != olpc->wlc->stf->txchain) {
			pwr = wlc_olpc_get_min_tgt_pwr(olpc, chan);

			if (pwr == WL_RATE_DISABLED) {
				/* assume not need olpc */
				WL_ERROR(("%s: min pwr lookup failed -- assume not olpc\n",
					__FUNCTION__));
				olpc->last_chan_olpc_state = FALSE;
			} else {
				/* adjust by -1.5dBm to reconcile ppr and tssi */
				pwr = WL_OLPC_PPR_TO_TSSI_PWR(pwr);
				tssi_thresh = wlc_phy_tssivisible_thresh(olpc->wlc->band->pi);
				WL_OLPC_DBG(olpc, ("chan=%d mintgtpwr=%d tssithresh=%d\n",
					chan, pwr, tssi_thresh));
				/* this channel needs open loop pwr cal iff the below is true */
				olpc->last_chan_olpc_state = (pwr < tssi_thresh);
			}
			olpc->last_chan_chk = chan;
			olpc->last_txchain_chk = olpc->wlc->stf->txchain;
		}
		return olpc->last_chan_olpc_state;
	} else {
		WL_REGULATORY(("%s: needs olpc FALSE/skip due to null phy info\n",
			__FUNCTION__));
	}
	return FALSE;
}

static void
wlc_olpc_chan_init(wlc_olpc_eng_chan_t* chan, chanspec_t cspec)
{
	chan->pkts_sent[0] = 0;
	chan->pkts_sent[1] = 0;
	chan->pkts_sent[2] = 0;

	/* chanspec, containing chan number the struct refers to */
	chan->cspec = cspec;
	chan->next = NULL;
	chan->cores_cal = 0;
	chan->cores_cal_active = 0;
	chan->cores_cal_to_cmplt = 0;
	chan->cal_pkts_outstanding = 0;
#if WL_OLPC_IOVARS_ENAB
	chan->cores_cal_pkts_sent = 0;
	chan->dbg_mode = FALSE;
#endif /* WL_OLPC_IOVARS_ENAB */
}

static int
wlc_olpc_chan_rm(wlc_olpc_eng_info_t *olpc,
	wlc_olpc_eng_chan_t* chan)
{
	int err = BCME_OK;
	wlc_olpc_eng_chan_t *iter = olpc->chan_list;
	wlc_olpc_eng_chan_t *trail_iter = NULL;
	ASSERT(chan);
	WL_OLPC_ENTRY(olpc, ("Entry:%s\n", __FUNCTION__));

	while (iter != NULL) {
		if (iter == chan) {
			break;
		}
		trail_iter = iter;
		iter = iter->next;
	}
	if (iter) {
		/* cut from list */
		if (trail_iter) {
			trail_iter->next = iter->next;
		} else {
			olpc->chan_list = iter->next;
		}
		/* cut from olpc */
		if (olpc->cur_chan == iter) {
			olpc->cur_chan = NULL;
		}
		/* free memory */
		MFREE(olpc->wlc->osh, iter, sizeof(wlc_olpc_eng_chan_t));
	} else {
		/* attempt to free failed */
		ASSERT(iter);
		err = BCME_ERROR;
	}
	WL_OLPC_ENTRY(olpc, ("Exit:%s\n", __FUNCTION__));

	return err;
}

/*
* cspec - chanspec to search for
* err - to return error values
* create - TRUE to create if not found; FALSE otherwise
* return pointer to channel info structure; NULL if not found/not created
*/
static wlc_olpc_eng_chan_t*
wlc_olpc_get_chan_ex(wlc_info_t *wlc, chanspec_t cspec, int *err, bool create)
{
	wlc_olpc_eng_chan_t* chan = NULL;
	*err = BCME_OK;

	chan = wlc->olpc_info->chan_list;
	/* find cspec in list */
	while (chan) {
		/* get chan struct through which comparison? */
		if (chan->cspec == cspec) {
			return chan;
		}
		chan = chan->next;
	}
	if (!create) {
		return NULL;
	}
	/* create new channel on demand */
	chan = MALLOC(wlc->osh, sizeof(wlc_olpc_eng_chan_t));
	if (chan) {
		/* init and insert into list */
		wlc_olpc_chan_init(chan, cspec);
		chan->next = wlc->olpc_info->chan_list;
		wlc->olpc_info->chan_list = chan;
	} else {
		*err = BCME_NOMEM;
	}

	return chan;
}

/* get olpc chan from list - create it if not there */
static wlc_olpc_eng_chan_t*
wlc_olpc_get_chan(wlc_info_t *wlc, chanspec_t cspec, int *err)
{
	return wlc_olpc_get_chan_ex(wlc, cspec, err, TRUE);
}

int
wlc_olpc_eng_hdl_txchain_update(wlc_olpc_eng_info_t *olpc)
{
	WL_OLPC_ENTRY(olpc, ("%s\n", __FUNCTION__));
	if (olpc->restore_perrate_stf_state &&
		!wlc_stf_saved_state_is_consistent(olpc->wlc, &olpc->stf_saved_perrate))
	{
		/* our saved txcore_override may no longer be valid, so don't restore */
		/* logic in stf prevents txchain and txcore_override from colliding */
		/* if saved state is 0 (clear override), */
		/* then restore unless txcore_override changes */
		olpc->restore_perrate_stf_state = FALSE;
	}
	return wlc_olpc_eng_hdl_chan_update(olpc);
}

int
wlc_olpc_eng_reset(wlc_olpc_eng_info_t *olpc)
{
	/* clear internal state and process new info */
	/* equivalent to down followed by up */
	if (wlc_olpc_eng_ready(olpc)) {
		WL_OLPC_DBG(olpc, ("%s - exec down/up\n", __FUNCTION__));
		wlc_olpc_eng_down((void*)olpc);
		wlc_olpc_eng_up((void*)olpc);
	}
	/* else wait until up and then process */
	return BCME_OK;
}

int
wlc_olpc_eng_hdl_chan_update(wlc_olpc_eng_info_t *olpc)
{
	if (!olpc) {
		return BCME_OK;
	}
	WL_OLPC_ENTRY(olpc, ("%s\n", __FUNCTION__));

	return wlc_olpc_eng_hdl_chan_update_ex(olpc->wlc, olpc->npkts, FALSE);
}

/* Is there a cfg up on our channel? (Ignoring exclude parm which may be NULL) */
static bool
wlc_olpc_find_other_up_on_chan(wlc_info_t *wlc, wlc_bsscfg_t *exclude)
{
	int idx;
	wlc_bsscfg_t *cfg;

	FOREACH_BSS(wlc, idx, cfg) {
		if (cfg->up && cfg != exclude) {
			if (cfg->current_bss &&
				cfg->current_bss->chanspec ==
				wlc->chanspec) {
				return TRUE;
			}
			WL_OLPC_ENTRY(wlc->olpc_info,
				("%s: unmatch bsscfg(%p) up%d curbss%p cspec%x\nwlc->cspec=%x\n",
				__FUNCTION__, cfg,
				cfg->up, cfg->current_bss,
				cfg->current_bss ? cfg->current_bss->chanspec : -1,
				wlc->chanspec));
		}
	}
	return FALSE;
}

/* kick-off open loop phy cal */
static int
wlc_olpc_eng_hdl_chan_update_ex(wlc_info_t *wlc, uint8 npkts, bool force_cal)
{
	int err = BCME_OK;
	chanspec_t cspec = WLC_TO_CHANSPEC(wlc);
	bool olpc_chan = FALSE;
	wlc_olpc_eng_chan_t* chan_info = NULL;
	wlc_olpc_eng_info_t* olpc_info = wlc->olpc_info;

	if (!wlc_olpc_eng_ready(olpc_info)) {
		WL_NONE(("wl%d:%s: olpc module not up\n", wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}
	WL_OLPC_ENTRY(olpc_info, ("%s\n", __FUNCTION__));

	/* check for any condition that may cause us to terminate our current calibration */
	/* and terminate it if needed */
	wlc_olpc_eng_terminate_cal(wlc);

	/* do nothing if scan or rm is running */
	if (wlc_olpc_in_cal_prohibit_state(wlc)) {
		olpc_info->cur_chan = NULL;
		goto exit;
	}

	olpc_chan = wlc_olpc_chan_needs_olpc(olpc_info, wlc->chanspec);
	WL_OLPC_DBG(olpc_info, ("%s: chan=%x home=%x olpc_chan=%d\n", __FUNCTION__, wlc->chanspec,
		wlc->home_chanspec, olpc_chan));

	if (olpc_chan) {
		/* phytx ctl word power offset needs to be set */
		chan_info = wlc_olpc_get_chan(wlc, cspec, &err);
		olpc_info->cur_chan = chan_info;

		/* pretend not calibrated */
		if (chan_info && force_cal) {
			olpc_info->cur_chan->cores_cal = 0;
#if WL_OLPC_IOVARS_ENAB
			olpc_info->cur_chan->cores_cal_pkts_sent = 0;
#endif /* WL_OLPC_IOVARS_ENAB */
		}


		/* if null here, there was an out of mem condition */
		if (!chan_info) {
			err = BCME_NOMEM;
			WL_OLPC(olpc_info, ("%s: chan info not found\n", __FUNCTION__));

			goto exit;
		} else if (!wlc_olpc_chan_needs_cal(olpc_info, chan_info)) {
			/* no cal needed */
			WL_OLPC_DBG(olpc_info, ("%s: no cal needed at chan update notify\n",
				__FUNCTION__));
			goto exit;
		} else {
			/* cal needed -- limit all rates for now; force implies cfg up on channel */
			if (!force_cal && !wlc_olpc_find_other_up_on_chan(wlc, NULL)) {
				WL_OLPC_DBG(olpc_info, ("%s: NO BSS FOUND UP - exitting\n",
					__FUNCTION__));
				/* avoid calibrations when no cfg up, as cal caching not avail */
				goto exit;
			}

			WL_OLPC_DBG(olpc_info, ("%s: calibration needed\n",
				__FUNCTION__));
		}
		/* now kick off cal/recal */
		WL_OLPC_DBG(olpc_info, ("%s: calibration needed, starting recal\n", __FUNCTION__));
		err = wlc_olpc_eng_recal_ex(wlc, npkts);
	} else {
		if (olpc_info->cur_chan) {
			wlc_olpc_stf_override_revert(wlc);
			olpc_info->cur_chan = NULL;
		}
	}

exit:
	return err;
}

static int
wlc_olpc_eng_recal_ex(wlc_info_t *wlc, uint8 npkts)
{
	int err = BCME_OK;
	wlc_olpc_eng_chan_t* chan_info = wlc->olpc_info->cur_chan;
	if (chan_info) {
		WL_OLPC(wlc->olpc_info, ("%s - send dummies\n", __FUNCTION__));
		err = wlc_olpc_send_dummy_pkts(wlc, chan_info, npkts);
	} else {
		WL_OLPC(wlc->olpc_info,
			("%s: chan info NULL - no cal\n", __FUNCTION__));
	}
	WL_NONE(("%s - end\n", __FUNCTION__));

	return err;
}

/* kick off new open loop phy cal */
int
wlc_olpc_eng_recal(wlc_olpc_eng_info_t *olpc)
{
	WL_OLPC_ENTRY(olpc, ("%s\n", __FUNCTION__));

	/* begin new cal if we can */
	return wlc_olpc_eng_hdl_chan_update_ex(olpc->wlc, olpc->npkts, TRUE);
}

static void
wlc_olpc_modify_pkt(wlc_info_t *wlc, void* p, uint8 core_idx)
{
	/* no ACK */
	wlc_pkt_set_ack(wlc, p, FALSE);

	/* Which core? */
	wlc_pkt_set_core(wlc, p, core_idx);

	/* pwr offset 0 */
	wlc_pkt_set_txpwr_offset(wlc, p, 0);
}

/* totally bogus -- d11 hdr only + tx hdrs */
static void *
wlc_olpc_get_pkt(wlc_info_t *wlc, uint ac, uint* fifo)
{
	int buflen = (TXOFF + WL_OLPC_PKT_LEN);
	void* p = NULL;
	osl_t *osh = wlc->osh;
	const char* macaddr = NULL;
	struct dot11_header *hdr = NULL;

	if ((p = PKTGET(osh, buflen, TRUE)) == NULL) {
		WL_ERROR(("wl%d: %s: pktget error for len %d \n",
			wlc->pub->unit, __FUNCTION__, buflen));
		goto fatal;
	}
	macaddr = WLC_MACADDR(wlc);

	WL_NONE(("pkt manip\n"));
	/* reserve TXOFF bytes of headroom */
	PKTPULL(osh, p, TXOFF);
	PKTSETLEN(osh, p, WL_OLPC_PKT_LEN);

	WL_NONE(("d11_hdr\n"));
	hdr = (struct dot11_header*)PKTDATA(osh, p);
	bzero((char*)hdr, WL_OLPC_PKT_LEN);
	hdr->fc = htol16(FC_DATA);
	hdr->durid = 0;
	bcopy((const char*)macaddr, (char*)&(hdr->a1.octet), ETHER_ADDR_LEN);
	bcopy((const char*)macaddr, (char*)&(hdr->a2.octet), ETHER_ADDR_LEN);
	bcopy((const char*)macaddr, (char*)&(hdr->a3.octet), ETHER_ADDR_LEN);
	hdr->seq = 0;
	WL_NONE(("prep raw 80211\n"));
	wlc->olpc_info->tx_cal_pkts = TRUE;
	/* frameid returned here -- ignore for now -- may speed up using this */
	(void)wlc_prep80211_raw(wlc, NULL, ac, TRUE, p, fifo);
	wlc->olpc_info->tx_cal_pkts = FALSE;

	return p;
fatal:
	return (NULL);
}

/* process one pkt send complete */
static void
wlc_olpc_eng_pkt_complete(wlc_info_t *wlc, void *pkt, uint txs)
{
	chanspec_t chanspec;

	wlc_olpc_eng_chan_t* olpc_chan = NULL;
	wlc_txh_info_t tx_info;
	int err;
	uint8 coreMask;
	uint8 cidx = 0;

	wlc_get_txh_info(wlc, pkt, &tx_info);

	/* one calibration packet was finished */
	/* look at packet header to find - channel, antenna, etc. */
	chanspec = wlc_txh_get_chanspec(wlc, &tx_info);
	olpc_chan = wlc_olpc_get_chan_ex(wlc, chanspec, &err, FALSE);
	WL_OLPC_ENTRY(wlc->olpc_info, ("%s\n", __FUNCTION__));

	if (!olpc_chan || err != BCME_OK) {
		WL_OLPC_DBG(wlc->olpc_info, ("%s: err: NO-OP chanspec=%x\n",
			__FUNCTION__, chanspec));
		return;
	}
	if (olpc_chan->cal_pkts_outstanding) {
		olpc_chan->cal_pkts_outstanding--;
	}
	if (olpc_chan->cores_cal_active == 0) {
		WL_OLPC_DBG(wlc->olpc_info, ("%s: NO-OP (no cal was active) chanspec=%x\n",
			__FUNCTION__, chanspec));
		goto pkt_complete_done;
	}

	WL_OLPC_DBG(wlc->olpc_info, ("%s: entry status=%d\n", __FUNCTION__, txs));

	/* get core number */
	coreMask = (tx_info.PhyTxControlWord0 & D11AC_PHY_TXC_CORE_MASK) >>
		(D11AC_PHY_TXC_CORE_SHIFT);
	cidx = WL_OLPC_ANT_TO_CIDX(coreMask);

	WL_OLPC_DBG(wlc->olpc_info, ("%s: coreNum=%x\n", __FUNCTION__, coreMask));

	/* decrement counters */
	if (olpc_chan->pkts_sent[cidx]) {
		olpc_chan->pkts_sent[cidx]--;
	} else {
		WL_NONE(("wl%d: %s: tried decrementing counter of 0, idx=%d\n",
			wlc->pub->unit, __FUNCTION__, WL_OLPC_ANT_TO_CIDX(coreMask)));
	}
	/* if done on core, update info */
	if (olpc_chan->pkts_sent[cidx] == 0) {
		olpc_chan->cores_cal_active &= ~coreMask;
		olpc_chan->cores_cal |= coreMask;
		olpc_chan->cores_cal_to_cmplt |= coreMask;
#if WL_OLPC_IOVARS_ENAB
		olpc_chan->cores_cal_pkts_sent |= coreMask;
#endif /* WL_OLPC_IOVARS_ENAB */

		WL_OLPC(wlc->olpc_info, ("%s: exit: open loop phy CAL done mask=%x!\n",
			__FUNCTION__, coreMask));
		WL_OLPC(wlc->olpc_info, ("%s: exit: open loop phy CAL done done=%x active=%x!\n",
			__FUNCTION__, olpc_chan->cores_cal, olpc_chan->cores_cal_active));
	}
#if WL_OLPC_IOVARS_ENAB
	if (olpc_chan->cores_cal == wlc->stf->hw_txchain) {
		WL_OLPC(wlc->olpc_info, ("%s: exit: open loop phy CAL done for all chains!\n",
			__FUNCTION__));
	}
#endif /* WLTEST || BCMDBG */
	if (olpc_chan->cores_cal_active == 0) {
		WL_OLPC(wlc->olpc_info, ("%s: no more cores w/ cal active!\n", __FUNCTION__));
		/* cache calibration results so following ops don't mess it up */
#if (defined(PHYCAL_CACHING) || defined(WLMCHAN) || defined(WL_MODESW))
#ifndef WLC_HIGH_ONLY
		if (!wlc_phy_get_chanctx((phy_info_t *)wlc->band->pi, wlc->chanspec))
		  wlc_phy_create_chanctx(wlc->band->pi, wlc->chanspec);
#endif /* WLC_HIGH_ONLY */
		wlc_phy_cal_cache((wlc_phy_t *)wlc->band->pi);
#endif /* PHYCAL_CACHING || WLMCHAN */

		/* execute these for now, coz cal is over */
		wlc_olpc_stf_override_revert(wlc);

#ifdef WLTXPWR_CACHE
		wlc_phy_txpwr_cache_invalidate(wlc_phy_get_txpwr_cache(wlc->band->pi));
#endif	/* WLTXPWR_CACHE */
#ifdef WLC_HIGH_ONLY
		wlc_bmac_phy_txpwr_cache_invalidate(wlc->hw);
#endif		/* restore cal cache to what it was prior to doing txpwr limit */
#if (defined(PHYCAL_CACHING) || defined(WLMCHAN))
		if ((err = wlc_phy_cal_cache_return((wlc_phy_t *)wlc->band->pi)) != BCME_OK) {
			WL_ERROR(("wl%d:%s: error from wlc_phy_cal_cache_restore=%d\n",
				wlc->pub->unit, __FUNCTION__, err));
			/* mark as not calibrated - calibration values hosed */
			olpc_chan->cores_cal = 0;
		}
		if (err == BCME_OK ||
#if WL_OLPC_IOVARS_ENAB
			olpc_chan->dbg_mode ||
#endif /* WL_OLPC_IOVARS_ENAB */
			FALSE) {
			/* inform the phy we are calibrated */
			/* if dbg mode is set, we ignore cal cache errors and tell the phy */
			/* to use dbg storage for cal result */
			wlc_phy_update_olpc_cal((wlc_phy_t *)wlc->band->pi, TRUE,
#if WL_OLPC_IOVARS_ENAB
				olpc_chan->dbg_mode);
#else
				FALSE);
#endif /* WL_OLPC_IOVARS_ENAB */
		}
#endif /* PHYCAL_CACHING || WLMCHAN */
		/* recalc ppr targets - this also resets pwr control so we need next line */
		wlc_channel_update_txpwr_limit(wlc);

		olpc_chan->cores_cal_to_cmplt = 0;
	}
pkt_complete_done:
	/* if reached the last outstanding pkt, then cal if needed */
	if (olpc_chan->cal_pkts_outstanding == 0) {
		wlc_olpc_eng_hdl_chan_update(wlc->olpc_info);
	}
}

static bool
wlc_olpc_eng_ready(wlc_olpc_eng_info_t *olpc_info)
{
	if (!olpc_info || !olpc_info->up) {
		return FALSE;
	}
	return TRUE;
}

bool
wlc_olpc_eng_tx_cal_pkts(wlc_olpc_eng_info_t *olpc)
{
	return (olpc && olpc->tx_cal_pkts);
}

/* return number of cores needing cal */
static uint8
wlc_olpc_chan_num_cores_cal_needed(wlc_info_t *wlc, wlc_olpc_eng_chan_t* chan)
{
	uint8 cores = wlc->olpc_info->num_hw_cores;
	uint8 core_idx = 0;
	uint8 needed = 0;
	for (; core_idx < cores; core_idx++) {
		/* if chain is off or calibration done/in progress then skip */
		if ((wlc->stf->txchain & (1 << core_idx)) == 0 ||
			((chan->cores_cal | chan->cores_cal_active) & (1 << core_idx)) != 0) {
			continue; /* not needed */
		}
		needed++;
	}
	return needed;
}

/* return BCME_OK if all npkts * num_cores get sent out */
static int
wlc_olpc_send_dummy_pkts(wlc_info_t *wlc, wlc_olpc_eng_chan_t* chan, uint8 npkts)
{
	int err = BCME_OK;
	void *pkt = NULL;
	uint8 cores, core_idx = 0;
	uint8 pktnum;
	uint ac = AC_VI;
	wlc_txh_info_t tx_info;
	uint fifo = 0;
	ASSERT(wlc->stf);
	cores = wlc->olpc_info->num_hw_cores;
	if (!chan) {
		WL_ERROR(("wl%d: %s: null channel - not sending\n", wlc->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}
	/* if send pkts would exceed npkts * tot cores, then wait for cal pkts to all return */
	if (chan->cal_pkts_outstanding +
		(wlc_olpc_chan_num_cores_cal_needed(wlc, chan) * npkts) >
		(npkts * cores)) {
		/* wait enough pkts completed, then calibrate */
		return BCME_OK;
	}
	if ((err = wlc_olpc_stf_override(wlc)) != BCME_OK) {
		WL_ERROR(("%s: abort olpc cal; err=%d\n", __FUNCTION__, err));
		return err;
	}

	for (; core_idx < cores; core_idx++) {
		/* if chain is off or calibration done/in progress then skip */
		if ((wlc->stf->txchain & (1 << core_idx)) == 0 ||
			((chan->cores_cal | chan->cores_cal_active) & (1 << core_idx)) != 0) {
			/* skip this one - already calibrated/calibrating or txchain not on */
			WL_OLPC(wlc->olpc_info,
				("%s: skip core %d for calibrating. txchain=%x\n",
				__FUNCTION__, core_idx, wlc->stf->txchain));
			continue;
		}

		for (pktnum = 0; pktnum < npkts; pktnum++) {
			WL_NONE(("%s: getting test frame\n", __FUNCTION__));

			pkt = wlc_olpc_get_pkt(wlc, ac, &fifo);
			if (pkt == NULL) {
				WL_OLPC_DBG(wlc->olpc_info, ("wl%d: %s: null pkt - not sending\n",
					wlc->pub->unit,
					__FUNCTION__));
				err = BCME_NOMEM;
				break;
			}
			/* modify tx headers, make sure it is no ack and on the right antenna */
			WL_NONE(("%s: modify pkt\n", __FUNCTION__));
			wlc_olpc_modify_pkt(wlc, pkt, core_idx);
			/* done modifying - now register for pcb */
			WLF2_PCB1_REG(pkt, WLF2_PCB1_OLPC);

			WL_NONE(("%s: send pkt\n", __FUNCTION__));

			/* avoid get_txh_info if can reuse */
			if (pktnum == 0) {
				wlc_get_txh_info(wlc, pkt, &tx_info);
			} else {
				int tsoHdrSize;
				d11actxh_t* vhtHdr = NULL;
				tsoHdrSize = wlc_pkt_get_vht_hdr(wlc, pkt, &vhtHdr);
				tx_info.tsoHdrSize = tsoHdrSize;
				tx_info.tsoHdrPtr = (void*)((tsoHdrSize != 0) ?
				PKTDATA(wlc->osh, pkt) : NULL);
				tx_info.hdrPtr = (wlc_txd_t *)(PKTDATA(wlc->osh, pkt) + tsoHdrSize);
				tx_info.hdrSize = D11AC_TXH_LEN;
				tx_info.d11HdrPtr = ((uint8 *)tx_info.hdrPtr) + D11AC_TXH_LEN;
				tx_info.TxFrameID = vhtHdr->PktInfo.TxFrameID;
				tx_info.MacTxControlLow = vhtHdr->PktInfo.MacTxControlLow;
				tx_info.MacTxControlHigh = vhtHdr->PktInfo.MacTxControlHigh;
				tx_info.plcpPtr = (vhtHdr->RateInfo[0].plcp);
			}
			chan->pkts_sent[core_idx]++;
			WL_NONE(("olpc fifo=%d prio=%d\n", fifo, PKTPRIO(pkt)));
			chan->cal_pkts_outstanding++;
			wlc_txfifo(wlc, fifo, pkt, &tx_info, TRUE, 1);
		}
		/* successful here, so modify cal_active variable */
		chan->cores_cal_active |= (1 << core_idx);
	}
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: err - cal not done\n",
			wlc->pub->unit, __FUNCTION__));
		if (pkt) {
			PKTFREE(wlc->osh, pkt, TRUE);
		}
	}
	return err;
}

static int
wlc_olpc_doiovar(void *context, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int alen, int vsize, struct wlc_if *wlcif)
{
	int err = BCME_UNSUPPORTED;
#if WL_OLPC_IOVARS_ENAB
	wlc_olpc_eng_info_t *olpc = (wlc_olpc_eng_info_t *)context;
	wlc_info_t *wlc = olpc->wlc;

	wlc_olpc_eng_chan_t* chan = NULL;
	bool is_olpc_chan = FALSE;
	bool is_scanning = FALSE;

	int32 int_val = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	err = BCME_OK;

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;
	WL_OLPC_ENTRY(olpc, ("%s\n", __FUNCTION__));
	if (olpc->up) {
		is_olpc_chan = wlc_olpc_chan_needs_olpc(olpc, wlc->chanspec);
		is_scanning = wlc_olpc_in_cal_prohibit_state(wlc);
		if (is_olpc_chan) {
			chan = wlc_olpc_get_chan_ex(wlc, wlc->chanspec, &err, !is_scanning);
		}
	}
	WL_OLPC_ENTRY(olpc, ("%s: chan=%p; is_scanning=%d\n", __FUNCTION__, chan, is_scanning));

	switch (actionid) {
		case IOV_SVAL(IOV_OLPC):
			wlc->pub->_olpc = bool_val;
			break;
		case IOV_GVAL(IOV_OLPC):
			*ret_int_ptr = OLPC_ENAB(wlc);
			break;
		case IOV_GVAL(IOV_OLPC_CAL_PKTS):
			if (is_scanning) {
				err = BCME_BUSY;
			} else if (!is_olpc_chan || !chan) {
				WL_OLPC(olpc,
					("%s: OLPC_CAL_PKTS: not valid olpc chan (%x)\n",
					__FUNCTION__, wlc->chanspec));
				err = BCME_BADCHAN;
			} else {
				*ret_int_ptr = chan->cores_cal_pkts_sent;
			}
			break;
		case IOV_SVAL(IOV_OLPC_CAL_ST):
			if (is_scanning) {
				err = BCME_BUSY;
			} else if (!is_olpc_chan || !chan) {
				WL_OLPC(olpc,
					("%s: SET_CAL_STATE: not valid olpc chanspec (%x)\n",
					__FUNCTION__, wlc->chanspec));
				err = BCME_BADCHAN;
			} else {
				chan->cores_cal = (uint8)int_val;
			}
			break;
		case IOV_GVAL(IOV_OLPC_CAL_ST):
			if (is_scanning) {
				err = BCME_BUSY;
			} else if (!is_olpc_chan || !chan) {
				WL_OLPC(olpc,
					("%s: GET_CAL_STATE: not valid olpc chan (%x)\n",
					__FUNCTION__, wlc->chanspec));
				err = BCME_BADCHAN;
			} else {
				*ret_int_ptr = chan->cores_cal;
			}
			break;
		case IOV_GVAL(IOV_OLPC_FORCE_CAL):
			if (is_scanning) {
				err = BCME_BUSY;
				*ret_int_ptr = -1;
				goto finish_iovar;
			}
			if (!chan) {
				/* always force, even if not olpc channel */
				chan = wlc_olpc_get_chan_ex(wlc, wlc->chanspec, &err, TRUE);
			}
			if (chan) {
				chan->cores_cal = 0;
				chan->cores_cal_active = 0;
				wlc->olpc_info->cur_chan = chan;
				wlc_olpc_eng_recal_ex(wlc, olpc->npkts);
				*ret_int_ptr = wlc_olpc_chan_has_active_cal(olpc, chan);
			} else {
				WL_ERROR(("%s: Couldn't create chan struct\n", __FUNCTION__));
				err = BCME_NOMEM;
				*ret_int_ptr = -1;
			}
			break;
		case IOV_SVAL(IOV_OLPC_MSG_LVL):
			olpc->msglevel = (uint8)int_val;
			break;
		case IOV_GVAL(IOV_OLPC_MSG_LVL):
			*ret_int_ptr = olpc->msglevel;
			break;
		case IOV_GVAL(IOV_OLPC_CHAN):
			*ret_int_ptr =
				wlc_olpc_chan_needs_olpc(olpc, wlc->chanspec) ? 1 : 0;
			break;
		case IOV_SVAL(IOV_OLPC_CHAN_DBG):
			if (is_scanning) {
				err = BCME_BUSY;
			} else if (!is_olpc_chan) {
				WL_OLPC(olpc,
					("wl%d:%s: OLPC_CHAN_DBG: not valid olpc chan (%x)\n",
					wlc->pub->unit, __FUNCTION__, wlc->chanspec));
				err = BCME_BADCHAN;
			} else if (chan) {
				chan->dbg_mode = bool_val;
			} else {
				WL_ERROR(("wl%d:%s: out of memory for SVAL(OLPC_CHAN_DBG)\n",
					wlc->pub->unit, __FUNCTION__));
				err = BCME_NOMEM;
			}
			break;
		case IOV_GVAL(IOV_OLPC_CHAN_DBG):
			if (is_scanning) {
				err = BCME_BUSY;
			} else if (!is_olpc_chan) {
				WL_OLPC(olpc,
					("wl%d:%s: OLPC_CHAN_DBG: not valid olpc chan (%x)\n",
					wlc->pub->unit, __FUNCTION__, wlc->chanspec));
				err = BCME_BADCHAN;
			} else if (chan) {
				*ret_int_ptr = chan->dbg_mode;
			} else {
				WL_ERROR(("wl%d:%s: out of memory for GVAL(OLPC_CHAN_DBG)\n",
					wlc->pub->unit, __FUNCTION__));
				err = BCME_NOMEM;
			}
			break;

		default:
			err = BCME_UNSUPPORTED;
			break;
	}
finish_iovar:
#endif /* WL_OLPC_IOVARS_ENAB */
	return err;
}

#if defined(BCMDBG) || defined(WLTEST)
static void
wlc_olpc_dump_channels(wlc_olpc_eng_info_t *olpc, struct bcmstrbuf *b)
{
	wlc_olpc_eng_chan_t* cur_chan = olpc->chan_list;

	while (cur_chan) {
		bcm_bprintf(b, "ptr=%p chan=%x pkts[%d,%d,%d]\n"
		"core_cal=%x core_cal_active=%x core_cal_need_cmplt=%x cal_pkts_outst=%d\n",
			cur_chan, cur_chan->cspec, cur_chan->pkts_sent[0], cur_chan->pkts_sent[1],
			cur_chan->pkts_sent[2], cur_chan->cores_cal, cur_chan->cores_cal_active,
			cur_chan->cores_cal_to_cmplt, cur_chan->cal_pkts_outstanding);
		cur_chan = cur_chan->next;
	}
}

static int
wlc_dump_olpc(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_olpc_eng_info_t *olpc = wlc->olpc_info;
	if (!olpc) {
		bcm_bprintf(b, "olpc NULL (not attached)!\n");
		return 0;
	}
	bcm_bprintf(b, "up=%d txing_cal=%d npkts=%d", olpc->up, olpc->tx_cal_pkts, olpc->npkts);
	bcm_bprintf(b, "\nnum_hw_cores=%d restr_txcr=%d saved_txcr=%x", olpc->num_hw_cores,
		olpc->restore_perrate_stf_state, olpc->stf_saved_perrate);
	bcm_bprintf(b, "\nPPR: last_txchain_chk=%x last_chan_chk=%x last_chan_olpc_state=%d\n"
		"cur_chan=%p\n",
		olpc->last_txchain_chk, olpc->last_chan_chk, olpc->last_chan_olpc_state,
		olpc->cur_chan);

	wlc_olpc_dump_channels(olpc, b);
	return 0;
}
#endif /* defined(BCMDBG) || defined(WLTEST) */

/* module attach */
wlc_olpc_eng_info_t*
BCMATTACHFN(wlc_olpc_eng_attach)(wlc_info_t *wlc)
{
	int err;
	wlc_olpc_eng_info_t *olpc_info = NULL;
	WL_NONE(("%s\n", __FUNCTION__));
	if (!wlc) {
		WL_ERROR(("%s - null wlc\n", __FUNCTION__));
		goto fail;
	}
	if ((olpc_info = (wlc_olpc_eng_info_t *)MALLOC(wlc->osh, sizeof(wlc_olpc_eng_info_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	bzero(olpc_info, sizeof(wlc_olpc_eng_info_t));
	olpc_info->wlc = wlc;
	olpc_info->chan_list = NULL;
	olpc_info->npkts = WLC_OLPC_DEF_NPKTS;
	olpc_info->up = FALSE;
	olpc_info->last_chan_olpc_state = FALSE;
	olpc_info->last_chan_chk = INVCHANSPEC;
	olpc_info->last_txchain_chk = WLC_OLPC_INVALID_TXCHAIN;

	olpc_info->restore_perrate_stf_state = FALSE;
	olpc_info->cur_chan = NULL;
	olpc_info->tx_cal_pkts = FALSE;
	olpc_info->updn = FALSE;
	olpc_info->assoc_notif_reg = FALSE;
	olpc_info->msglevel = 0;

	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, olpc_iovars, "olpc", olpc_info, wlc_olpc_doiovar,
	                        NULL, wlc_olpc_eng_up, wlc_olpc_eng_down)) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	WL_NONE(("%s - end\n", __FUNCTION__));
	/* bsscfg up/down callback */
	if (wlc_bsscfg_updown_register(wlc, wlc_olpc_bsscfg_updn, olpc_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	olpc_info->updn = TRUE;
	if (wlc_bss_assoc_state_register(wlc,
		wlc_olpc_bsscfg_assoc_state_notif, olpc_info) !=
		BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_assoc_state_notif_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
	}
	olpc_info->assoc_notif_reg = TRUE;

#if defined(BCMDBG) || defined(WLTEST)
	wlc_dump_register(wlc->pub, "olpc", (dump_fn_t)wlc_dump_olpc, (void *)wlc);
#endif /* defined(BCMDBG) || defined(WLTEST) */
	err = wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_OLPC, wlc_olpc_eng_pkt_complete);
	if (err != BCME_OK) {
		WL_ERROR(("%s: wlc_pcb_fn_set err=%d\n", __FUNCTION__, err));
		goto fail;
	}

	return olpc_info;
fail:
	wlc_olpc_eng_detach(olpc_info);
	return NULL;
}

/* go through and free all chan info */
static void
wlc_olpc_free_chans(struct wlc_olpc_eng_info_t *olpc_info)
{
	wlc_olpc_eng_chan_t* cur_chan = olpc_info->chan_list;
	wlc_info_t *wlc = olpc_info->wlc;

	while (cur_chan) {
		cur_chan = cur_chan->next;
		MFREE(wlc->osh, olpc_info->chan_list, sizeof(wlc_olpc_eng_chan_t));
		olpc_info->chan_list = cur_chan;
	}
	/* no longer a valid field */
	olpc_info->cur_chan = NULL;
}

/* module detach, up, down */
void
BCMATTACHFN(wlc_olpc_eng_detach)(struct wlc_olpc_eng_info_t *olpc_info)
{
	wlc_info_t *wlc;

	if (olpc_info == NULL) {
		return;
	}
	wlc = olpc_info->wlc;
	wlc_olpc_free_chans(olpc_info);
	wlc_module_unregister(wlc->pub, "olpc", olpc_info);
	if (olpc_info->updn) {
		wlc_bsscfg_updown_unregister(olpc_info->wlc, wlc_olpc_bsscfg_updn, olpc_info);
	}
	if (olpc_info->assoc_notif_reg) {
		wlc_bss_assoc_state_unregister(wlc,
			wlc_olpc_bsscfg_assoc_state_notif, olpc_info);
	}
	MFREE(wlc->osh, olpc_info, sizeof(struct wlc_olpc_eng_info_t));
}

static int
wlc_olpc_eng_up(void *hdl)
{
	struct wlc_olpc_eng_info_t *olpc_info;
	olpc_info = (struct wlc_olpc_eng_info_t *)hdl;
	if (olpc_info->up) {
		return BCME_OK;
	}
	olpc_info->num_hw_cores = (uint8)WLC_BITSCNT(olpc_info->wlc->stf->hw_txchain);

	olpc_info->up = TRUE;
	wlc_olpc_eng_hdl_chan_update(olpc_info);

	return BCME_OK;
}

static int
wlc_olpc_eng_down(void *hdl)
{
	struct wlc_olpc_eng_info_t *olpc_info;
	olpc_info = (struct wlc_olpc_eng_info_t *)hdl;
	if (!olpc_info->up) {
		return BCME_OK;
	}
	olpc_info->up = FALSE;
	/* clear ppr query cache */
	olpc_info->last_chan_olpc_state = FALSE;
	olpc_info->last_chan_chk = INVCHANSPEC;

	/* clear cal info */
	wlc_olpc_free_chans(olpc_info);
	return BCME_OK;
}
#endif /* WLOLPC */
