/*
 * Application-specific portion of EAPD
 * (DCS)
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dcs_eap.c 398228 2013-04-23 22:34:08Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <proto/eap.h>
#include <bcmendian.h>
#include <wlutils.h>
#include <eapd.h>
#include <shutils.h>
#include <UdpLib.h>
#include <security_ipc.h>

void
dcs_app_recv_handler(eapd_wksp_t *nwksp, eapd_cb_t *from, uint8 *pData,
	int *pLen)
{

}

void
dcs_app_set_eventmask(eapd_app_t *app)
{
	memset(app->bitvec, 0, sizeof(app->bitvec));

	setbit(app->bitvec, WLC_E_DCS_REQUEST);
	setbit(app->bitvec, WLC_E_SCAN_COMPLETE);
	setbit(app->bitvec, WLC_E_PKTDELAY_IND);
	setbit(app->bitvec, WLC_E_TXFAIL_THRESH);
	return;
}

int
dcs_app_init(eapd_wksp_t *nwksp)
{
	int reuse = 1;
	eapd_dcs_t *dcs;
	eapd_cb_t *cb;
	struct sockaddr_in addr;


	if (nwksp == NULL)
		return -1;

	dcs = &nwksp->dcs;
	dcs->appSocket = -1;

	cb = dcs->cb;
	if (cb == NULL) {
		EAPD_INFO("No any DCS application need to run.\n");
		return 0;
	}

	while (cb) {
		EAPD_INFO("dcs: init brcm interface %s \n", cb->ifname);
		cb->brcmSocket = eapd_add_brcm(nwksp, cb->ifname);
		if (!cb->brcmSocket)
			return -1;
		/* set this brcmSocket have DCS capability */
		cb->brcmSocket->flag |= EAPD_CAP_DCS;

		cb = cb->next;
	}

	/* appSocket for dcs */
	dcs->appSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (dcs->appSocket < 0) {
		EAPD_ERROR("UDP Open failed.\n");
		return -1;
	}
#if defined(__ECOS)
	if (setsockopt(dcs->appSocket, SOL_SOCKET, SO_REUSEPORT, (char*)&reuse,
		sizeof(reuse)) < 0) {
		EAPD_ERROR("UDP setsockopt failed.\n");
		close(dcs->appSocket);
		dcs->appSocket = -1;
		return -1;
	}
#else
	if (setsockopt(dcs->appSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse,
		sizeof(reuse)) < 0) {
		EAPD_ERROR("UDP setsockopt failed.\n");
		close(dcs->appSocket);
		dcs->appSocket = -1;
		return -1;
	}
#endif 

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(EAPD_WKSP_DCS_UDP_RPORT);
	if (bind(dcs->appSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		EAPD_ERROR("UDP Bind failed, close dcs appSocket %d\n", dcs->appSocket);
		close(dcs->appSocket);
		dcs->appSocket = -1;
		return -1;
	}
	EAPD_INFO("DCS appSocket %d opened\n", dcs->appSocket);

	return 0;
}

int
dcs_app_deinit(eapd_wksp_t *nwksp)
{
	eapd_dcs_t *dcs;
	eapd_cb_t *cb, *tmp_cb;

	if (nwksp == NULL) {
		EAPD_ERROR("Wrong argument...\n");
		return -1;
	}

	dcs = &nwksp->dcs;
	cb = dcs->cb;
	while (cb) {
		/* close  brcm drvSocket */
		if (cb->brcmSocket) {
			EAPD_INFO("close dcs brcmSocket %d\n", cb->brcmSocket->drvSocket);
			eapd_del_brcm(nwksp, cb->brcmSocket);
		}

		tmp_cb = cb;
		cb = cb->next;
		free(tmp_cb);
	}

	/* close  appSocke */
	if (dcs->appSocket >= 0) {
		EAPD_INFO("close dcs appSocket %d\n", dcs->appSocket);
		close(dcs->appSocket);
		dcs->appSocket = -1;
	}

	return 0;
}

int
dcs_app_sendup(eapd_wksp_t *nwksp, uint8 *pData, int pLen, char *from)
{
	eapd_dcs_t *dcs;

	if (nwksp == NULL) {
		EAPD_ERROR("Wrong argument...\n");
		return -1;
	}

	dcs = &nwksp->dcs;
	if (dcs->appSocket >= 0) {
		/* send to dcs */
		int sentBytes = 0;
		struct sockaddr_in to;

		to.sin_addr.s_addr = inet_addr(EAPD_WKSP_UDP_ADDR);
		to.sin_family = AF_INET;
		to.sin_port = htons(EAPD_WKSP_DCS_UDP_SPORT);

		sentBytes = sendto(dcs->appSocket, pData, pLen, 0,
			(struct sockaddr *)&to, sizeof(struct sockaddr_in));

		if (sentBytes != pLen) {
			EAPD_ERROR("UDP send failed; sentBytes = %d\n", sentBytes);
		}
		else {
			/* EAPD_ERROR("Send %d bytes to dcs\n", sentBytes); */
		}
	}
	else {
		EAPD_ERROR("dcs appSocket not created\n");
	}
	return 0;
}

#if EAPD_WKSP_AUTO_CONFIG
int
dcs_app_enabled(char *name)
{
	char value[128], comb[32],  prefix[8];
	char os_name[IFNAMSIZ];
	int unit;

	memset(os_name, 0, sizeof(os_name));

	if (nvifname_to_osifname(name, os_name, sizeof(os_name)))
		return 0;
	if (wl_probe(os_name) ||
		wl_ioctl(os_name, WLC_GET_INSTANCE, &unit, sizeof(unit)))
		return 0;
	if (osifname_to_nvifname(name, prefix, sizeof(prefix)))
		return 0;

	strcat(prefix, "_");
	/* ignore if disabled */
	eapd_safe_get_conf(value, sizeof(value), strcat_r(prefix, "radio", comb));
	if (atoi(value) == 0) {
		EAPD_INFO("DCS:ignored interface %s. radio disabled\n", os_name);
		return 0;
	}

	/* ignore if BSS is disabled */
	eapd_safe_get_conf(value, sizeof(value), strcat_r(prefix, "bss_enabled", comb));
	if (atoi(value) == 0) {
		EAPD_INFO("DCS: ignored interface %s, %s is disabled \n", os_name, comb);
		return 0;
	}

	/* if come to here return enabled */
	return 1;
}
#endif /* EAPD_WKSP_AUTO_CONFIG */

int
dcs_app_handle_event(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from)
{
	int type;
	eapd_dcs_t *dcs;
	eapd_cb_t *cb;
	bcm_event_t *dpkt = (bcm_event_t *) pData;
	wl_event_msg_t *event = &(dpkt->event);

	type = ntohl(event->event_type);

	dcs = &nwksp->dcs;
	cb = dcs->cb;
	while (cb) {
		if (isset(dcs->bitvec, type) &&
			!strcmp(cb->ifname, from)) {

			/* prepend ifname,  we reserved IFNAMSIZ length already */
			pData -= IFNAMSIZ;
			Len += IFNAMSIZ;
			memcpy(pData, event->ifname, IFNAMSIZ);

			/* send to dcs use cb->ifname */
			dcs_app_sendup(nwksp, pData, Len, cb->ifname);
			break;
		}
		cb = cb->next;
	}

	return 0;
}
