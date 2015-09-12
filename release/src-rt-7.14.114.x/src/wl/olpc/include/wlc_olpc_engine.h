/*
 * Common OS-independent driver header for open-loop power calibration engine.
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_olpc_engine.h,v 1.70 2012/01/28 04:59:00 Exp $
 */
#ifndef _wlc_olpc_engine_h_
#define _wlc_olpc_engine_h_
#ifdef WLOLPC
/* get open loop pwr ctl rate limit for current channel */
#define WLC_OLPC_NO_LIMIT	0
#define WLC_OLPC_SISO_LIMIT	1

/* Event handlers */
/* EVENT: channel changes */
extern int wlc_olpc_eng_hdl_chan_update(wlc_olpc_eng_info_t *olpc);

/* EVENT: txchain changes */
extern int wlc_olpc_eng_hdl_txchain_update(wlc_olpc_eng_info_t *olpc);

/* EVENT: ccrev changes - wipe out saved channel data and try to cal on current channel */
extern int wlc_olpc_eng_reset(wlc_olpc_eng_info_t *olpc);

/* API: Force new open loop phy cal on current channel (call after channel change done) */
/* Will only calibrate if assoc is present */
extern int wlc_olpc_eng_recal(wlc_olpc_eng_info_t *olpc);

/* API: Module-system interfaces */
/* module attach */
extern wlc_olpc_eng_info_t * wlc_olpc_eng_attach(wlc_info_t *wlc);

/* module detach, up, down */
extern void wlc_olpc_eng_detach(wlc_olpc_eng_info_t *olpc);

/* return true if a calibration is in progress */
extern bool wlc_olpc_eng_has_active_cal(wlc_olpc_eng_info_t *olpc);

extern bool wlc_olpc_eng_tx_cal_pkts(wlc_olpc_eng_info_t *olpc);

/* get coremask of calibrated cores for current channel; -1 if not on olpc channel */
extern int wlc_olpc_get_cal_state(wlc_olpc_eng_info_t *olpc);

/* true iff sending calibration pkts */
#define WLC_OLPC_TX_CAL_PKTS(olpc) (wlc_olpc_eng_tx_cal_pkts((olpc)))
#endif /* WLOLPC */
#endif /* _wlc_olpc_engine_h_ */
