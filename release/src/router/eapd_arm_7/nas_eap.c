/*
 * Application-specific portion of EAPD
 * (NAS)
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: nas_eap.c 437632 2013-11-19 17:12:02Z $
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
#include <nas.h>
#include <security_ipc.h>
#include <bcmwpa.h>

/* mapping from WPA_CIPHER_XXXX to wsec */
#define NAS_APP_WPA_CIPHER2WSEC(cipher)	((cipher) == WPA_CIPHER_WEP_40 ? WEP_ENABLED : \
				(cipher) == WPA_CIPHER_WEP_104 ? WEP_ENABLED : \
				(cipher) == WPA_CIPHER_TKIP ? TKIP_ENABLED : \
				(cipher) == WPA_CIPHER_AES_CCM ? AES_ENABLED : \
				0)

static uint32
nas_app_wpa_akm2auth(uint32 akm)
{
	switch (akm) {
	case RSN_AKM_PSK:
		return WPA_AUTH_PSK;
	case RSN_AKM_UNSPECIFIED:
		return WPA_AUTH_UNSPECIFIED;
	case RSN_AKM_NONE:
	default:
		return WPA_AUTH_NONE;
	}
}

static uint32
nas_app_wpa2_akm2auth(uint32 akm)
{
	switch (akm) {
	case RSN_AKM_PSK:
		return WPA2_AUTH_PSK;
	case RSN_AKM_UNSPECIFIED:
		return WPA2_AUTH_UNSPECIFIED;
	case RSN_AKM_NONE:
	default:
		return WPA_AUTH_NONE;
	}
}

static int
nas_app_wpa_auth2mode(int auth)
{
	switch (auth) {
	case WPA_AUTH_PSK:
		return WPA_PSK;
	case WPA_AUTH_UNSPECIFIED:
		return WPA;
	case WPA2_AUTH_PSK:
		return WPA2_PSK;
	case WPA2_AUTH_UNSPECIFIED:
		return WPA2;
	case WPA_AUTH_DISABLED:
	default:
		return RADIUS;
	}
}

static bool
nas_app_is_wpa_ie(uint8 *ie, uint8 **tlvs, uint *tlvs_len)
{
	/* If the contents match the WPA_OUI and type=1 */
	if ((ie[TLV_LEN_OFF] > (WPA_OUI_LEN+1)) &&
	    !bcmp(&ie[TLV_BODY_OFF], WPA_OUI "\x01", WPA_OUI_LEN + 1)) {
		return TRUE;
	}

	/* point to the next ie */
	ie += ie[TLV_LEN_OFF] + TLV_HDR_LEN;
	/* calculate the length of the rest of the buffer */
	*tlvs_len -= (int)(ie - *tlvs);
	/* update the pointer to the start of the buffer */
	*tlvs = ie;

	return FALSE;
}

static bool
nas_app_is_wps_ie(uint8 *ie, uint8 **tlvs, uint *tlvs_len)
{
	/* If the contents match the WPA_OUI and type=4 */
	if ((ie[TLV_LEN_OFF] > (WPA_OUI_LEN+1)) &&
	    !bcmp(&ie[TLV_BODY_OFF], WPA_OUI "\x04", WPA_OUI_LEN + 1)) {
		return TRUE;
	}

	/* point to the next ie */
	ie += ie[TLV_LEN_OFF] + TLV_HDR_LEN;
	/* calculate the length of the rest of the buffer */
	*tlvs_len -= (int)(ie - *tlvs);
	/* update the pointer to the start of the buffer */
	*tlvs = ie;

	return FALSE;
}

static bcm_tlv_t *
nas_app_parse_tlvs(void *buf, int buflen, uint key)
{
	bcm_tlv_t *elt;
	int totlen;

	elt = (bcm_tlv_t*)buf;
	totlen = buflen;

	/* find tagged parameter */
	while (totlen >= 2) {
		int len = elt->len;

		/* validate remaining totlen */
		if ((elt->id == key) && (totlen >= (len + 2)))
			return (elt);

		elt = (bcm_tlv_t*)((uint8*)elt + (len + 2));
		totlen -= (len + 2);
	}

	return NULL;
}

static wpa_ie_fixed_t *
nas_app_find_wpaie(uint8 *parse, uint len)
{
	bcm_tlv_t *ie;

	while ((ie = nas_app_parse_tlvs(parse, len, DOT11_MNG_WPA_ID))) {
		if (nas_app_is_wpa_ie((uint8*)ie, &parse, &len)) {
			return (wpa_ie_fixed_t *)ie;
		}
	}
	return NULL;
}

static wpa_ie_fixed_t *
nas_app_find_wpsie(uint8 *data, uint data_len)
{
	uint8 *parse = data;
	uint len = data_len;
	bcm_tlv_t *ie;

	while ((ie = nas_app_parse_tlvs(parse, len, DOT11_MNG_WPA_ID))) {
		if (nas_app_is_wps_ie((uint8*)ie, &parse, &len)) {
			return (wpa_ie_fixed_t *)ie;
		}
	}
	return NULL;
}

/* decode WPA IE to retrieve supplicant wsec, auth mode, and pmk cached */
/* pmkc - 0:no pmkid in ie, -1:pmkid not found, 1:pmkid found */
static int
nas_app_parse_ie(uint8 *ie, int ie_len, uint32 *wsec, uint32 *mode)
{
	int len;
	wpa_suite_mcast_t *mcast = NULL;
	wpa_suite_ucast_t *ucast = NULL;
	wpa_suite_auth_key_mgmt_t *mgmt = NULL;
	uint8 *oui;

	uint16 count;
	uint32 m = 0;
	uint32 (*akm2auth)(uint32 akm) = NULL;
	wpa_ie_fixed_t *wpaie = NULL;
	uint8 *parse = ie;
	int parse_len = ie_len;


	/* Search WPA IE */
	wpaie = nas_app_find_wpaie(parse, parse_len);
	/* Search RSN IE */
	if (!wpaie)
		wpaie = (wpa_ie_fixed_t *)nas_app_parse_tlvs(ie, ie_len, DOT11_MNG_RSN_ID);

	/* No WPA or RSN IE */
	if (!wpaie)
		return -1;

	/* type specific header processing */
	switch (wpaie->tag) {
	case DOT11_MNG_RSN_ID: {
		wpa_rsn_ie_fixed_t *rsnie = (wpa_rsn_ie_fixed_t *)wpaie;
		if (rsnie->length < WPA_RSN_IE_TAG_FIXED_LEN) {
			EAPD_ERROR("invalid RSN IE header\n");
			return -1;
		}
		if (ltoh16_ua((uint8 *)&rsnie->version) != WPA2_VERSION) {
			EAPD_ERROR("unsupported RSN IE version\n");
			return -1;
		}
		mcast = (wpa_suite_mcast_t *)(rsnie + 1);
		len = ie_len - WPA_RSN_IE_FIXED_LEN;
		oui = (uint8*)WPA2_OUI;
		akm2auth = nas_app_wpa2_akm2auth;
		break;
	}
	case DOT11_MNG_WPA_ID: {
		if (wpaie->length < WPA_IE_TAG_FIXED_LEN ||
		    bcmp(wpaie->oui, WPA_OUI "\x01", WPA_IE_OUITYPE_LEN)) {
			EAPD_ERROR("invalid WPA IE header\n");
			return -1;
		}
		if (ltoh16_ua((uint8 *)&wpaie->version) != WPA_VERSION) {
			EAPD_ERROR("unsupported WPA IE version\n");
			return -1;
		}
		mcast = (wpa_suite_mcast_t *)(wpaie + 1);
		len = ie_len - WPA_IE_FIXED_LEN;
		oui = (uint8*)WPA_OUI;
		akm2auth = nas_app_wpa_akm2auth;
		break;
	}
	default:
		EAPD_ERROR("unsupported IE type\n");
		return -1;
	}


	/* init return values - no mcast cipher and no ucast cipher */
	if (wsec)
		*wsec = 0;
	if (mode)
		*mode = 0;

	/* Check for multicast suite */
	if (len >= WPA_SUITE_LEN) {
		if (!bcmp(mcast->oui, oui, DOT11_OUI_LEN)) {
			if (wsec)
				*wsec |= NAS_APP_WPA_CIPHER2WSEC(mcast->type);
		}
		len -= WPA_SUITE_LEN;
	}
	/* Check for unicast suite(s) */
	if (len >= WPA_IE_SUITE_COUNT_LEN) {
		ucast = (wpa_suite_ucast_t *)&mcast[1];
		count = ltoh16_ua((uint8 *)&ucast->count);
		len -= WPA_IE_SUITE_COUNT_LEN;
		if (count != 1) {
			EAPD_ERROR("# of unicast cipher suites %d\n", count);
			return -1;
		}
		if (!bcmp(ucast->list[0].oui, oui, DOT11_OUI_LEN)) {
			if (wsec)
				*wsec |= NAS_APP_WPA_CIPHER2WSEC(ucast->list[0].type);
		}
		len -= WPA_SUITE_LEN;
	}
	/* Check for auth key management suite(s) */
	if (len >= WPA_IE_SUITE_COUNT_LEN) {
		mgmt = (wpa_suite_auth_key_mgmt_t *)&ucast->list[1];
		count = ltoh16_ua((uint8 *)&mgmt->count);
		len -= WPA_IE_SUITE_COUNT_LEN;
		if (count != 1) {
			EAPD_ERROR("# of AKM suites %d\n", count);
			return -1;
		}
		if (!bcmp(mgmt->list[0].oui, oui, DOT11_OUI_LEN)) {
			m = nas_app_wpa_auth2mode(akm2auth(mgmt->list[0].type));
			if (mode)
				*mode = m;
		}
		len -= WPA_SUITE_LEN;
	}

	if (wsec) {
		EAPD_INFO("wsec 0x%x\n", *wsec);
	}

	if (mode) {
		EAPD_INFO("mode 0x%x\n", *mode);
	}

	return 0;
}

/* Receive eapol, preauth message from nas module
 */
void
nas_app_recv_handler(eapd_wksp_t *nwksp, char *wlifname, eapd_cb_t *from, uint8 *pData,
	int *pLen)
{
	eapol_header_t *eapol = (eapol_header_t*) pData;
	eap_header_t *eap;
	struct ether_addr *sta_ea;
	eapd_sta_t *sta;
	struct eapd_socket *Socket = NULL;

	if (!nwksp || !wlifname || !from || !pData) {
		EAPD_ERROR("Wrong argument...\n");
		return;
	}

	if (*pLen < EAPOL_HEADER_LEN)
		return; /* message too short */

	/* dispatch message to eapol, preauth and brcmevent */
	switch (ntohs(eapol->eth.ether_type)) {
		case ETHER_TYPE_802_1X:	/* eapol */
		case ETHER_TYPE_BRCM:	/* brcmevent */
			Socket = from->brcmSocket;
			break;
		case ETHER_TYPE_802_1X_PREAUTH: /* preauth */
			Socket = &from->preauthSocket;
			break;
	}

	/* send message data out. */
	sta_ea = (struct ether_addr*) eapol->eth.ether_dhost;
	sta = sta_lookup(nwksp, sta_ea, NULL, wlifname, EAPD_SEARCH_ONLY);

	/* monitor eapol packet */
	eap = (eap_header_t *) eapol->body;
	if ((sta) && (eapol->type == EAP_PACKET) &&
		(eap->code == EAP_FAILURE || eap->code == EAP_SUCCESS)) {
		sta_remove(nwksp, sta);
	}

	if (Socket != NULL) {
		eapd_message_send(nwksp, Socket, pData, *pLen);
	}
	else {
		EAPD_ERROR("Socket is not exist!\n");
	}
	return;
}

void
nas_app_set_eventmask(eapd_app_t *app)
{
	memset(app->bitvec, 0, sizeof(app->bitvec));

	setbit(app->bitvec, WLC_E_EAPOL_MSG);
	setbit(app->bitvec, WLC_E_LINK);
	setbit(app->bitvec, WLC_E_ASSOC_IND);
	setbit(app->bitvec, WLC_E_REASSOC_IND);
	setbit(app->bitvec, WLC_E_DISASSOC_IND);
	setbit(app->bitvec, WLC_E_DEAUTH_IND);
	setbit(app->bitvec, WLC_E_MIC_ERROR);
#ifdef WLWNM
	setbit(app->bitvec, WLC_E_WNM_STA_SLEEP);
#endif /* WLWNM */
	return;
}

int
nas_open_dif_sockets(eapd_wksp_t *nwksp, eapd_cb_t *cb)
{
#ifdef BCMWPA2
	eapd_preauth_socket_t *preauthSocket;
#endif

	EAPD_INFO("nas: init brcm interface %s \n", cb->ifname);
	cb->brcmSocket = eapd_add_brcm(nwksp, cb->ifname);
	if (!cb->brcmSocket)
		return -1;
	/* set this brcmSocket have NAS capability */
	cb->brcmSocket->flag |= EAPD_CAP_NAS;

#ifdef BCMWPA2
	/* open preauth socket */
	preauthSocket  = &cb->preauthSocket;
	memset(preauthSocket, 0, sizeof(eapd_preauth_socket_t));
	strcpy(preauthSocket->ifname, cb->ifname);
	EAPD_INFO("nas: init preauth interface %s \n", cb->ifname);
	if (eapd_preauth_open(nwksp, preauthSocket) < 0) {
		EAPD_ERROR("open preauth socket on %s error!!\n", preauthSocket->ifname);
		preauthSocket->drvSocket = -1;
	}
#endif

	return 0;
}

void
nas_close_dif_sockets(eapd_wksp_t *nwksp, eapd_cb_t *cb)
{
#ifdef BCMWPA2
	eapd_preauth_socket_t *preauthSocket;
#endif

	/* close brcm drvSocket */
	if (cb->brcmSocket) {
		EAPD_INFO("close nas brcmSocket %d\n", cb->brcmSocket->drvSocket);
		eapd_del_brcm(nwksp, cb->brcmSocket);
	}

#ifdef BCMWPA2
	/* close preauth drvSocket */
	preauthSocket = &cb->preauthSocket;
	if (preauthSocket->drvSocket >= 0) {
		EAPD_INFO("close nas preauthSocket %d\n", preauthSocket->drvSocket);
		eapd_preauth_close(preauthSocket->drvSocket);
		preauthSocket->drvSocket = -1;
	}
#endif
}

int
nas_app_init(eapd_wksp_t *nwksp)
{
	int reuse = 1;
	eapd_nas_t *nas;
	eapd_cb_t *cb;
	eapd_preauth_socket_t *preauthSocket;
	struct sockaddr_in addr;


	if (nwksp == NULL)
		return -1;

	nas = &nwksp->nas;
	nas->appSocket = -1;

	cb = nas->cb;
	if (cb == NULL) {
		EAPD_INFO("No any NAS application need to run.\n");
		return 0;
	}

	while (cb) {
		EAPD_INFO("nas: init brcm interface %s \n", cb->ifname);
		cb->brcmSocket = eapd_add_brcm(nwksp, cb->ifname);
		if (!cb->brcmSocket)
			return -1;
		/* set this brcmSocket have NAS capability */
		cb->brcmSocket->flag |= EAPD_CAP_NAS;

		/* open preauth socket */
		preauthSocket  = &cb->preauthSocket;
		memset(preauthSocket, 0, sizeof(eapd_preauth_socket_t));
		strcpy(preauthSocket->ifname, cb->ifname);
		EAPD_INFO("nas: init preauth interface %s \n", cb->ifname);
		if (eapd_preauth_open(nwksp, preauthSocket) < 0) {
			EAPD_ERROR("open preauth socket on %s error!!\n", preauthSocket->ifname);
			preauthSocket->drvSocket = -1;
		}
		cb = cb->next;
	}

	/* appSocket for nas */
	nas->appSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (nas->appSocket < 0) {
		EAPD_ERROR("UDP Open failed.\n");
		return -1;
	}
#if defined(__ECOS)
	if (setsockopt(nas->appSocket, SOL_SOCKET, SO_REUSEPORT, (char*)&reuse,
		sizeof(reuse)) < 0) {
		EAPD_ERROR("UDP setsockopt failed.\n");
		close(nas->appSocket);
		nas->appSocket = -1;
		return -1;
	}
#else
	if (setsockopt(nas->appSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse,
		sizeof(reuse)) < 0) {
		EAPD_ERROR("UDP setsockopt failed.\n");
		close(nas->appSocket);
		nas->appSocket = -1;
		return -1;
	}
#endif 

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(EAPD_WKSP_NAS_UDP_RPORT);
	if (bind(nas->appSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		EAPD_ERROR("UDP Bind failed, close nas appSocket %d\n", nas->appSocket);
		close(nas->appSocket);
		nas->appSocket = -1;
		return -1;
	}
	EAPD_INFO("NAS appSocket %d opened\n", nas->appSocket);

	return 0;
}

int
nas_app_deinit(eapd_wksp_t *nwksp)
{
	eapd_nas_t *nas;
	eapd_cb_t *cb, *tmp_cb;
	eapd_preauth_socket_t *preauthSocket;

	if (nwksp == NULL) {
		EAPD_ERROR("Wrong argument...\n");
		return -1;
	}

	nas = &nwksp->nas;
	cb = nas->cb;
	while (cb) {
		/* close  brcm drvSocket */
		if (cb->brcmSocket) {
			EAPD_INFO("close nas brcmSocket %d\n", cb->brcmSocket->drvSocket);
			eapd_del_brcm(nwksp, cb->brcmSocket);
		}

		/* close  preauth drvSocket */
		preauthSocket = &cb->preauthSocket;
		if (preauthSocket->drvSocket >= 0) {
			EAPD_INFO("close nas preauthSocket %d\n", preauthSocket->drvSocket);
			eapd_preauth_close(preauthSocket->drvSocket);
			preauthSocket->drvSocket = -1;
		}
		tmp_cb = cb;
		cb = cb->next;
		free(tmp_cb);
	}

	/* close  appSocke */
	if (nas->appSocket >= 0) {
		EAPD_INFO("close nas appSocket %d\n", nas->appSocket);
		close(nas->appSocket);
		nas->appSocket = -1;
	}

	return 0;
}

/* nas_sendup will send eapol, brcm and preauth packet */
int
nas_app_sendup(eapd_wksp_t *nwksp, uint8 *pData, int pLen, char *from)
{
	eapd_nas_t *nas;

	if (nwksp == NULL) {
		EAPD_ERROR("Wrong argument...\n");
		return -1;
	}

	nas = &nwksp->nas;
	if (nas->appSocket >= 0) {
		/* send to nas */
		int sentBytes = 0;
		struct sockaddr_in to;

		to.sin_addr.s_addr = inet_addr(EAPD_WKSP_UDP_ADDR);
		to.sin_family = AF_INET;
		to.sin_port = htons(EAPD_WKSP_NAS_UDP_SPORT);

		sentBytes = sendto(nas->appSocket, (char *)pData, pLen, 0,
			(struct sockaddr *)&to, sizeof(struct sockaddr_in));

		if (sentBytes != pLen) {
			EAPD_ERROR("UDP send failed; sentBytes = %d\n", sentBytes);
		}
		else {
			/* EAPD_ERROR("Send %d bytes to nas\n", sentBytes); */
		}
	}
	else {
		EAPD_ERROR("nas appSocket not created\n");
	}
	return 0;
}

#if EAPD_WKSP_AUTO_CONFIG
int
nas_app_enabled(char *name)
{
	char value[128], comb[32], akm[16], prefix[8];
	char akms[128], auth[32], wep[32], crypto[32], os_name[IFNAMSIZ];
	char *akmnext;
	uint wsec, nasm;
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
		EAPD_INFO("NAS:ignored interface %s. radio disabled\n", os_name);
		return 0;
	}
	/* ignore shared */
	eapd_safe_get_conf(value, sizeof(value), strcat_r(prefix, "auth", comb));
	if (atoi(value) == 1) {
		EAPD_INFO("NAS: ignored interface %s. Shared 802.11 auth\n", os_name);
		return 0;
	}
	/* ignore if BSS is disabled */
	eapd_safe_get_conf(value, sizeof(value), strcat_r(prefix, "bss_enabled", comb));
	if (atoi(value) == 0) {
		EAPD_INFO("NAS: ignored interface %s, %s is disabled \n", os_name, comb);
		return 0;
	}
	/* nas mode */
	eapd_safe_get_conf(auth, sizeof(auth), strcat_r(prefix, "auth_mode", comb));
	nasm = !strcmp(auth, "radius") ? RADIUS : 0;
	eapd_safe_get_conf(akms, sizeof(akms), strcat_r(prefix, "akm", comb));
	foreach(akm, akms, akmnext) {
		if (!strcmp(akm, "wpa"))
			nasm |= WPA;
		if (!strcmp(akm, "psk"))
			nasm |= WPA_PSK;
		if (!strcmp(akm, "wpa2"))
			nasm |= WPA2;
		if (!strcmp(akm, "psk2"))
			nasm |= WPA2_PSK;
	}
	if (!nasm) {
		EAPD_INFO("NAS:ignored interface %s. Invalid NAS mode\n", os_name);
		return 0;
	}
	/* wsec */
	eapd_safe_get_conf(wep, sizeof(wep), strcat_r(prefix, "wep", comb));
	wsec = !strcmp(wep, "enabled") ? WEP_ENABLED : 0;
	if (CHECK_NAS(nasm)) {
		eapd_safe_get_conf(crypto, sizeof(crypto), strcat_r(prefix, "crypto", comb));
		if (!strcmp(crypto, "tkip"))
			wsec |= TKIP_ENABLED;
		else if (!strcmp(crypto, "aes"))
			wsec |= AES_ENABLED;
		else if (!strcmp(crypto, "tkip+aes"))
			wsec |= TKIP_ENABLED|AES_ENABLED;
	}
	if (!wsec) {
		EAPD_INFO("%s: Ignored interface. Invalid WSEC\n", os_name);
		return 0;
	}

	/* if come to here return enabled */
	return 1;
}
#endif /* EAPD_WKSP_AUTO_CONFIG */

int
nas_app_handle_event(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from)
{
	int type;
	eapd_nas_t *nas;
	eapd_cb_t *cb;
	bcm_event_t *dpkt = (bcm_event_t *) pData;
	wl_event_msg_t *event = &(dpkt->event);
	uint8 *addr = (uint8 *)&(event->addr);
	uint8 *data = (uint8 *)(event + 1);
	uint16 len = ntoh32(event->datalen);
	uint32 mode, wsec;
	eapd_sta_t *sta;


	type = ntohl(event->event_type);

	nas = &nwksp->nas;
	cb = nas->cb;
	while (cb) {
		if (isset(nas->bitvec, type) && !strcmp(cb->ifname, from)) {
			/* We need to know is client in PSK mode */
			if (type == WLC_E_ASSOC_IND || type == WLC_E_REASSOC_IND ||
			    type == WLC_E_DISASSOC_IND || type == WLC_E_DEAUTH_IND) {
				sta = sta_lookup(nwksp, (struct ether_addr *)addr,
				        (struct ether_addr *) dpkt->eth.ether_dhost,
				        event->ifname, EAPD_SEARCH_ENTER);
				if (!sta) {
					EAPD_ERROR("no STA struct available\n");
					return -1;
				}

				if (type == WLC_E_DISASSOC_IND || type == WLC_E_DEAUTH_IND) {
					/* Reset the sta mode */
					sta->mode = EAPD_STA_MODE_UNKNOW;
				}
				else if (nas_app_find_wpsie(data, (uint)len)) {
					sta->mode = EAPD_STA_MODE_UNKNOW;

					/* Don't send those Indication to NAS, otherwise NAS may
					* initial a 4-ways handshake with Ralink STA
					*/
					break;
				}
				else if (!nas_app_parse_ie(data, len, &wsec, &mode) &&
					CHECK_PSK(mode)) {
					/* set cient in PSK mode */
					sta->mode = EAPD_STA_MODE_NAS_PSK;
				}
			}

			/* prepend ifname,  we reserved IFNAMSIZ length already */
			pData -= IFNAMSIZ;
			Len += IFNAMSIZ;
			memcpy(pData, event->ifname, IFNAMSIZ);

			EAPD_INFO("%s: send event %d to nas %s\n",
			          event->ifname, type, cb->ifname);

			/* send to nas use cb->ifname */
			nas_app_sendup(nwksp, pData, Len, cb->ifname);
			break;
		}
		cb = cb->next;
	}

	return 0;
}
