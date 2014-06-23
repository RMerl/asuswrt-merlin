/*
 * Application-specific portion of EAPD
 * (WPS)
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_eap.c 241182 2011-02-17 21:50:03Z $
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
#include <wlutils.h>
#include <eapd.h>
#include <shutils.h>
#include <UdpLib.h>
#include <security_ipc.h>
#include <bcmconfig.h>

/* Receive message from wps module  */
void
wps_app_recv_handler(eapd_wksp_t *nwksp, char *wlifname, eapd_cb_t *from,
	uint8 *pData, int *pLen, struct ether_addr *ap_ea)
{
	eapol_header_t *eapol = (eapol_header_t*) pData;
	eap_header_t *eap;
	eapd_sta_t *sta;
	struct ether_addr *sta_ea;

	if (!nwksp || !wlifname || !from || !pData) {
		EAPD_ERROR("Wrong arguments...\n");
		return;
	}

	if (*pLen < EAPOL_HEADER_LEN) {
		EAPD_ERROR("Message too short...\n");
		return;
	}

	/* send message data out. */
	sta_ea = (struct ether_addr*) eapol->eth.ether_dhost;
	sta = sta_lookup(nwksp, sta_ea, NULL, wlifname, EAPD_SEARCH_ONLY);

	/* monitor eapol packet */
	if (eapol->type == EAPOL_START) {
		/* remove exit */
		if (sta)
			sta_remove(nwksp, sta);
		/* create new one */
		sta = sta_lookup(nwksp, sta_ea, ap_ea, wlifname, EAPD_SEARCH_ENTER);
		if (sta) {
			sta->mode = EAPD_STA_MODE_WPS_ENR;
		}
	}
	else {
		eap = (eap_header_t *) eapol->body;
		/* remove sta info when FAILURE or SUCCESS */
		if ((sta) && (eapol->type == EAP_PACKET) &&
			(eap->code == EAP_FAILURE || eap->code == EAP_SUCCESS)) {
			sta_remove(nwksp, sta);
		}
	}

	eapd_message_send(nwksp, from->brcmSocket, pData, *pLen);

	return;
}

void
wps_app_set_eventmask(eapd_app_t *app)
{
	memset(app->bitvec, 0, sizeof(app->bitvec));

	setbit(app->bitvec, WLC_E_EAPOL_MSG);
	setbit(app->bitvec, WLC_E_PROBREQ_MSG);
/*
*/
#ifdef __CONFIG_WFI__
	setbit(app->bitvec, WLC_E_ASSOC_IND);
	setbit(app->bitvec, WLC_E_REASSOC_IND);
	setbit(app->bitvec, WLC_E_DISASSOC_IND);
	setbit(app->bitvec, WLC_E_DEAUTH_IND);
#endif /* __CONFIG_WFI__ */
/*
*/
	return;
}

int
wps_app_init(eapd_wksp_t *nwksp)
{
	int reuse = 1;
	eapd_wps_t *wps;
	eapd_cb_t *cb;
	struct sockaddr_in addr;

	if (nwksp == NULL)
		return -1;

	wps = &nwksp->wps;
	wps->appSocket = -1;

	cb = wps->cb;
	if (cb == NULL) {
		EAPD_INFO("No any interface is running WPS !\n");
		return 0;
	}

	while (cb) {
		EAPD_INFO("init brcm interface %s \n", cb->ifname);
		cb->brcmSocket = eapd_add_brcm(nwksp, cb->ifname);
		if (!cb->brcmSocket)
			return -1;
		/* set this brcmSocket have WPS capability */
		cb->brcmSocket->flag |= EAPD_CAP_WPS;

		cb = cb->next;
	}

	/*
	 * appSocket for wps-monitor, wps-monitor handle all wps
	 * relative packets
	 */
	wps->appSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (wps->appSocket < 0) {
		EAPD_ERROR("UDP Open failed.\n");
		return -1;
	}
#if defined(__ECOS)
	if (setsockopt(wps->appSocket, SOL_SOCKET, SO_REUSEPORT, (char*)&reuse,
		sizeof(reuse)) < 0) {
		EAPD_ERROR("UDP setsockopt failed.\n");
		close(wps->appSocket);
		wps->appSocket = -1;
		return -1;
	}
#else
	if (setsockopt(wps->appSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse,
		sizeof(reuse)) < 0) {
		EAPD_ERROR("UDP setsockopt failed.\n");
		close(wps->appSocket);
		wps->appSocket = -1;
		return -1;
	}
#endif 

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(EAPD_WKSP_WPS_UDP_RPORT);
	if (bind(wps->appSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		EAPD_ERROR("UDP Bind failed, close wps appSocket %d\n", wps->appSocket);
		close(wps->appSocket);
		wps->appSocket = -1;
		return -1;
	}
	EAPD_INFO("WPS appSocket %d opened\n", wps->appSocket);

	return 0;
}

int
wps_app_deinit(eapd_wksp_t *nwksp)
{
	eapd_wps_t *wps;
	eapd_cb_t *cb, *tmp_cb;

	if (nwksp == NULL) {
		EAPD_ERROR("Wrong argument...\n");
		return -1;
	}
	wps = &nwksp->wps;
	cb = wps->cb;
	while (cb) {
		/* brcm drvSocket delete */
		if (cb->brcmSocket) {
			EAPD_INFO("close wps brcmSocket %d\n", cb->brcmSocket->drvSocket);
			eapd_del_brcm(nwksp, cb->brcmSocket);
		}

		tmp_cb = cb;
		cb = cb->next;
		free(tmp_cb);
	}

	/* close appSocket for wps-monitor */
	if (wps->appSocket >= 0) {
		EAPD_INFO("close wps m_appSocket %d\n", wps->appSocket);
		close(wps->appSocket);
		wps->appSocket = -1;
	}

	return 0;
}

int
wps_app_monitor_sendup(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from)
{
	eapd_wps_t *wps;

	if (nwksp == NULL) {
		EAPD_ERROR("Wrong argument...\n");
		return -1;
	}

	wps = &nwksp->wps;
	if (wps->appSocket >= 0) {
		/* send to wps-monitor */
		int sentBytes = 0;
		struct sockaddr_in to;

		to.sin_addr.s_addr = inet_addr(EAPD_WKSP_UDP_ADDR);
		to.sin_family = AF_INET;
		to.sin_port = htons(EAPD_WKSP_WPS_UDP_MPORT);

		sentBytes = sendto(wps->appSocket, (char *)pData, Len, 0,
			(struct sockaddr *)&to, sizeof(struct sockaddr_in));

		if (sentBytes != Len) {
			EAPD_ERROR("UDP send to wps-monitor on %s failed; sentBytes = %d\n",
				from, sentBytes);
		}
		else {
			/* EAPD_INFO("send %d bytes to wps-monitor on %s\n", sentBytes, from); */
		}
	}
	else {
		EAPD_ERROR("wps-monitor appSocket not created\n");
	}

	return 0;
}

#if EAPD_WKSP_AUTO_CONFIG
int
wps_app_enabled(char *name)
{
	char value[128], os_name[IFNAMSIZ], temp[32], prefix[8];
	int unit;

	if (nvifname_to_osifname(name, os_name, sizeof(os_name)) < 0)
		return 0;
	if (wl_probe(os_name) ||
		wl_ioctl(os_name, WLC_GET_INSTANCE, &unit, sizeof(unit)))
		return 0;
	/* Convert eth name to wl name */
	if (osifname_to_nvifname(name, prefix, sizeof(prefix)) != 0)
		return 0;

	strcat(prefix, "_");
	eapd_safe_get_conf(value, sizeof(value), strcat_r(prefix, "bss_enabled", temp));
	if (strcmp(value, "1"))
		return 0;

	eapd_safe_get_conf(value, sizeof(value), strcat_r(prefix, "wps_mode", temp));
	if (!strcmp(value, "enabled") || !strcmp(value, "enr_enabled"))
		return 1;

	return 0;
}
#endif /* EAPD_WKSP_AUTO_CONFIG */

int
wps_app_handle_event(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from)
{
	int type;
	eapd_wps_t *wps;
	eapd_cb_t *cb;
	bcm_event_t *dpkt = (bcm_event_t *) pData;
	wl_event_msg_t *event;

	event = &(dpkt->event);
	type = ntohl(event->event_type);

	wps = &nwksp->wps;
	cb = wps->cb;
	while (cb) {
		if (isset(wps->bitvec, type) && !strcmp(cb->ifname, from)) {
			/* prepend ifname,  we reserved IFNAMSIZ length already */
			pData -= IFNAMSIZ;
			Len += IFNAMSIZ;
			memcpy(pData, event->ifname, IFNAMSIZ);

			/* send to wps use cb->ifname */
			wps_app_monitor_sendup(nwksp, pData, Len, cb->ifname);
			break;
		}
		cb = cb->next;
	}

	return 0;
}
