/*
 * Application-specific portion of EAPD
 * (SES)
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ses_eap.c 241182 2011-02-17 21:50:03Z $
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

#define SES_NV_INTERFACE	"ses_interface"

void
ses_app_recv_handler(eapd_wksp_t *nwksp, char *wlifname, eapd_cb_t *from,
	uint8 *pData, int *pLen)
{
	eapol_header_t *eapol = (eapol_header_t*) pData;
	eap_header_t *eap;
	eapd_sta_t *sta;
	struct ether_addr *sta_ea;

	if (!nwksp || !wlifname || !from || !pData) {
		EAPD_ERROR("Wrong arguments...\n");
		return;
	}

	if (*pLen < EAPOL_HEADER_LEN)
		return; /* message too short */

	/* send message data out. */
	sta_ea = (struct ether_addr*) eapol->eth.ether_dhost;
	sta = sta_lookup(nwksp, sta_ea, NULL, wlifname, EAPD_SEARCH_ONLY);

	/* monitor eapol packet */
	eap = (eap_header_t *) eapol->body;
	if ((sta) && (eapol->type == EAP_PACKET) &&
		(eap->code == EAP_FAILURE || eap->code == EAP_SUCCESS)) {
		sta_remove(nwksp, sta);
	}

	eapd_message_send(nwksp, from->brcmSocket, (uint8*) eapol, *pLen);

	return;
}

void
ses_app_set_eventmask(eapd_app_t *app)
{
	memset(app->bitvec, 0, sizeof(app->bitvec));

	setbit(app->bitvec, WLC_E_EAPOL_MSG);
	return;
}

int
ses_app_init(eapd_wksp_t *nwksp)
{
	int reuse = 1;
	eapd_ses_t *ses;
	eapd_cb_t *cb;
	struct sockaddr_in addr;


	if (nwksp == NULL)
		return -1;

	ses = &nwksp->ses;
	ses->appSocket = -1;

	cb = ses->cb;
	if (cb == NULL) {
		EAPD_INFO("No any SES application need to run\n");
		return 0;
	}

	while (cb) {
		EAPD_INFO("ses: init brcm interface %s \n", cb->ifname);
		cb->brcmSocket = eapd_add_brcm(nwksp, cb->ifname);
		if (!cb->brcmSocket)
			return -1;
		/* set this brcmSocket have SES capability */
		cb->brcmSocket->flag |= EAPD_CAP_SES;

		cb = cb->next;
	}

	/* appSocket for ses */
	ses->appSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (ses->appSocket < 0) {
		EAPD_ERROR("UDP Open failed.\n");
		return -1;
	}
#if defined(__ECOS)
	if (setsockopt(ses->appSocket, SOL_SOCKET, SO_REUSEPORT, (char*)&reuse,
		sizeof(reuse)) < 0) {
		EAPD_ERROR("UDP setsockopt failed.\n");
		close(ses->appSocket);
		ses->appSocket = -1;
		return -1;
	}
#else
	if (setsockopt(ses->appSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse,
		sizeof(reuse)) < 0) {
		EAPD_ERROR("UDP setsockopt failed.\n");
		close(ses->appSocket);
		ses->appSocket = -1;
		return -1;
	}
#endif 

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(EAPD_WKSP_SES_UDP_RPORT);
	if (bind(ses->appSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		EAPD_ERROR("UDP Bind failed, close ses appSocket %d\n", ses->appSocket);
		close(ses->appSocket);
		ses->appSocket = -1;
		return -1;
	}
	EAPD_INFO("SES appSocket %d opened\n", ses->appSocket);

	return 0;
}

int
ses_app_deinit(eapd_wksp_t *nwksp)
{
	eapd_ses_t *ses;
	eapd_cb_t *cb, *tmp_cb;

	if (nwksp == NULL) {
		EAPD_ERROR("Wrong argument...\n");
		return -1;
	}

	ses = &nwksp->ses;
	cb = ses->cb;
	while (cb) {
		if (cb->brcmSocket) {
			EAPD_INFO("close ses brcm drvSocket %d\n", cb->brcmSocket->drvSocket);
			eapd_del_brcm(nwksp, cb->brcmSocket);
		}

		tmp_cb = cb;
		cb = cb->next;
		free(tmp_cb);
	}

	/* close  appSocket */
	if (ses->appSocket >= 0) {
		EAPD_INFO("close ses appSocket %d\n", ses->appSocket);
		close(ses->appSocket);
		ses->appSocket = -1;
	}

	return 0;
}

int
ses_app_sendup(eapd_wksp_t *nwksp, uint8 *pData, int pLen, char *from)
{
	eapd_ses_t *ses;

	if (nwksp == NULL) {
		EAPD_ERROR("Wrong argument...\n");
		return -1;
	}

	ses = &nwksp->ses;
	if (ses->appSocket >= 0) {
		/* send to ses */
		int sentBytes = 0;
		struct sockaddr_in to;

		to.sin_addr.s_addr = inet_addr(EAPD_WKSP_UDP_ADDR);
		to.sin_family = AF_INET;
		to.sin_port = htons(EAPD_WKSP_SES_UDP_SPORT);

		sentBytes = sendto(ses->appSocket, (char *)pData, pLen, 0,
			(struct sockaddr *)&to, sizeof(struct sockaddr_in));

		if (sentBytes != pLen) {
			EAPD_ERROR("UDP send failed; sentBytes = %d\n", sentBytes);
		}
		else {
			/* EAPD_INFO("Send %d bytes to ses\n", sentBytes); */
		}
	}
	else {
		EAPD_ERROR("ses appSocket not created\n");
	}
	return 0;
}

#if EAPD_WKSP_AUTO_CONFIG
int
ses_app_enabled(char *name)
{
	char value[128];
	int unit;

	/* ses service not enabled */
	eapd_safe_get_conf(value, sizeof(value), "ses_enable");
	if (strcmp(value, "1") != 0)
		return 0;

	eapd_safe_get_conf(value, sizeof(value), SES_NV_INTERFACE);
	if (!strcmp(value, "")) {
		if (wl_probe(name))
			return 0;
		snprintf(value, sizeof(value), "%s", name);
	}

	if ((wl_probe(value) != 0) ||
		(wl_ioctl(value, WLC_GET_INSTANCE, &unit, sizeof(unit)) != 0)) {
		EAPD_ERROR("ERROR: Invalid default wireless interface %s\n", value);
		return 0;
	}

	return 1;
}
#endif /* EAPD_WKSP_AUTO_CONFIG */

int
ses_app_handle_event(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from)
{
	int type;
	eapd_ses_t *ses;
	eapd_cb_t *cb;
	bcm_event_t *dpkt = (bcm_event_t *) pData;
	wl_event_msg_t *event;

	event = &(dpkt->event);
	type = ntohl(event->event_type);

	ses = &nwksp->ses;
	cb = ses->cb;
	while (cb) {
		if (isset(ses->bitvec, type) && !strcmp(cb->ifname, from)) {
			/* prepend ifname,  we reserved IFNAMSIZ length already */
			pData -= IFNAMSIZ;
			Len += IFNAMSIZ;
			memcpy(pData, event->ifname, IFNAMSIZ);

			/* send to ses use cb->ifname */
			ses_app_sendup(nwksp, pData, Len, cb->ifname);
			break;
		}
		cb = cb->next;
	}

	return 0;
}
