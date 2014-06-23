/*
 * dcs.c
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dcs.c 398227 2013-04-23 22:34:03Z $
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <assert.h>
#include <typedefs.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <bcmtimer.h>
#include <bcmendian.h>
#include <shutils.h>
#include <bcmendian.h>
#include <bcmwifi_channels.h>
#include <wlioctl.h>
#include <wlutils.h>

#include "acsd_svr.h"

int
dcs_parse_actframe(dot11_action_wifi_vendor_specific_t *actfrm, wl_bcmdcs_data_t *dcs_data)
{
	uint8 cat;
	uint32 reason;
	chanspec_t chanspec;

	if (!actfrm)
		return ACSD_ERR_NO_FRM;

	cat = actfrm->category;
	ACSD_INFO("recved action frames, category: %d\n", actfrm->category);
	if (cat != DOT11_ACTION_CAT_VS)
		return ACSD_ERR_NOT_ACTVS;

	if ((actfrm->OUI[0] != BCM_ACTION_OUI_BYTE0) ||
		(actfrm->OUI[1] != BCM_ACTION_OUI_BYTE1) ||
		(actfrm->OUI[2] != BCM_ACTION_OUI_BYTE2) ||
		(actfrm->type != BCM_ACTION_RFAWARE)) {
		ACSD_INFO("recved VS action frame, but not DCS request\n");
		return ACSD_ERR_NOT_DCSREQ;
	}

	bcopy(&actfrm->data[0], &reason, sizeof(uint32));
	dcs_data->reason = ltoh32(reason);
	bcopy(&actfrm->data[4], (uint8*)&chanspec, sizeof(chanspec_t));
	dcs_data->chspec = ltoh16(chanspec);

	ACSD_INFO("dcs_data: reason: %d, chanspec: 0x%4x\n", dcs_data->reason,
		(uint16)dcs_data->chspec);

	return ACSD_OK;
}

int
dcs_handle_request(char* ifname, wl_bcmdcs_data_t *dcs_data,
	uint8 mode, uint8 count, uint8 csa_mode)
{
	wl_chan_switch_t csa;
	int err = ACSD_OK;

	ACSD_INFO("ifname: %s, reason: %d, chanspec: 0x%x, csa:%x\n",
		ifname, dcs_data->reason, dcs_data->chspec, csa_mode);

	csa.mode = mode;
	csa.count = count;
	csa.chspec = dcs_data->chspec;
	csa.reg = 0;
	csa.frame_type = csa_mode;

	err = wl_iovar_set(ifname, "csa", &csa, sizeof(wl_chan_switch_t));

	return err;
}
