/*
 * WPS eap
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_eap.c 383924 2013-02-08 04:14:39Z $
 */

#include <stdio.h>
#include <time.h>

#include <bn.h>
#include <wps_dh.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <sminfo.h>
#include <reg_proto.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef __ECOS
#include <sys/socket.h>
#endif
#include <net/if.h>

#include <bcmparams.h>
#include <wlutils.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <proto/eap.h>
#include <wps_wl.h>
#include <tutrace.h>

#include <bcmutils.h>
#include <ap_ssr.h>
#include <wps_ap.h>
#include <security_ipc.h>
#include <wps_ui.h>
#include <statemachine.h>
#include <wpsapi.h>
#include <wps_pb.h>
#include <wps.h>
#include <wps_eap.h>
#include <shutils.h>

#ifdef __CONFIG_WFI__
#include <wps_wfi.h>
#endif /* __CONFIG_WFI__ */

#ifdef WPS_UPNP_DEVICE
#include <wps_upnp.h>
#endif

/* under m_xxx in mbuf.h */

#define WPS_IE_BUF_LEN	VNDR_IE_MAX_LEN * 8	/* 2048 */

struct eap_user_list {
	unsigned char identity[32];
	unsigned int identity_len;
};
static struct eap_user_list  eapuserlist[2] = {
	{"WFA-SimpleConfig-Enrollee-1-0", 29},
	{"WFA-SimpleConfig-Registrar-1-0", 30}
};

#ifdef WPS_UPNP_DEVICE
typedef struct preb_req_sta_scb_s {
	unsigned char	mac[SIZE_MAC_ADDR];
	unsigned long	time;
	struct preb_req_sta_scb_s *next;
} PREB_REQ_STA_SCB;
static PREB_REQ_STA_SCB *preb_req_scb = NULL;
#endif

static wps_hndl_t eap_hndl;


static uint32
wps_eap_get_id(char *buf)
{
	eapol_header_t *eapol = (eapol_header_t *)buf;
	eap_header_t *eap = (eap_header_t *) eapol->body;
	int i;

	if (eap->type != EAP_IDENTITY)
		return WPS_EAP_ID_NONE;

	for (i = 0; i < 2; i++) {
		if (memcmp((char *)eap->data, eapuserlist[i].identity,
			eapuserlist[i].identity_len) == 0) {
			return i;
		}
	}

	return WPS_EAP_ID_NONE;
}

static int
wps_eap_find_ess_id(char *ifname)
{
	int i;
	int imax = wps_get_ess_num();
	char name[IFNAMSIZ];
	char *wlnames, *next = NULL, tmp[100];

	/* Find ess */
	for (i = 0; i < imax; i++) {
		sprintf(tmp, "ess%d_wlnames", i);
		wlnames = wps_safe_get_conf(tmp);

		foreach(name, wlnames, next) {
			if (!strcmp(name, ifname)) {
				goto found;
			}
		}
	}
found:
	if (i == imax)
		return -1; /* Not Found */
	else
		return i; /* Found */
}

#ifdef WPS_UPNP_DEVICE
static int
wps_eap_update_prebreq_time(unsigned char *mac)
{
	int updated = 0;
	unsigned long now;
	PREB_REQ_STA_SCB *scb = preb_req_scb;

	(void) time((time_t*)&now);

	while (scb) {
		if (memcmp(mac, scb->mac, SIZE_MAC_ADDR) == 0)
			break; /* Found */
		scb = scb->next;
	}

	/* Not found, new one */
	if (scb == NULL) {
		if ((scb = malloc(sizeof(PREB_REQ_STA_SCB))) == NULL)
			return updated;
		memset(scb, 0, sizeof(PREB_REQ_STA_SCB));
		/* Append to head */
		scb->next = preb_req_scb;
		preb_req_scb = scb;
		/* Copy ipaddr string */
		memcpy(scb->mac, mac, SIZE_MAC_ADDR);
		scb->time = now;
		updated = 1;
	} else {
		/* Check time in second */
		if (now > scb->time) {
			scb->time = now;
			updated = 1;
		}
	}

	return updated;
}
#endif /* WPS_UPNP_DEVICE */

static uint32
wps_eap_parse_prob_reqIE(unsigned char *mac, unsigned char *p_data, uint32 len,
	char *ifname)
{
	uint32 tlv_err = 0;
	WpsProbreqIE prReqIE;
	BufferObj *bufObj = buffobj_new();
#ifdef WPS_UPNP_DEVICE
	int ess_id;
	wps_app_t *wps_app = get_wps_app();

	/* Forward probe request to External Registart only when session not open yet. */
	if (wps_app->wksp == 0 && wps_eap_update_prebreq_time(mac)) {
		if ((ess_id = wps_eap_find_ess_id(ifname)) != -1)
			wps_upnp_forward_preb_req(ess_id, mac, (char *)p_data, len);
	}
#endif /* WPS_UPNP_DEVICE */

	/* De-serialize this IE to get to the data */
	if (bufObj == NULL) {
		TUTRACE((TUTRACE_ERR, "wps_eap_parse_prob_reqIE: buffobj_new fail\n"));
		return -1;
	}

	buffobj_dserial(bufObj, p_data, len);

	tlv_err |= tlv_dserialize(&prReqIE.version, WPS_ID_VERSION, bufObj, 0, 0);
	if (tlv_err)
		goto err_out;

	tlv_err |= tlv_dserialize(&prReqIE.reqType, WPS_ID_REQ_TYPE, bufObj, 0, 0);
	if (tlv_err)
		goto err_out;

	tlv_err |=  tlv_dserialize(&prReqIE.confMethods, WPS_ID_CONFIG_METHODS, bufObj, 0, 0);
	if (tlv_err)
		goto err_out;

	if (WPS_ID_UUID_E == buffobj_NextType(bufObj)) {
		tlv_err |= tlv_dserialize(&prReqIE.uuid, WPS_ID_UUID_E, bufObj, SIZE_16_BYTES, 0);
		if (tlv_err)
			goto err_out;
	}
	else if (WPS_ID_UUID_R == buffobj_NextType(bufObj)) {
		tlv_err |=  tlv_dserialize(&prReqIE.uuid, WPS_ID_UUID_R, bufObj, SIZE_16_BYTES, 0);
		if (tlv_err)
			goto err_out;
	}

	/* Primary Device Type is a complex TLV - handle differently */
	tlv_err |= tlv_primDeviceTypeParse(&prReqIE.primDevType, bufObj);

err_out:
	/* if any error, bail  */
	if (tlv_err) {
		if (bufObj)
			buffobj_del(bufObj);
		return -1;
	}

	tlv_dserialize(&prReqIE.rfBand, WPS_ID_RF_BAND, bufObj, 0, 0);
	tlv_dserialize(&prReqIE.assocState, WPS_ID_ASSOC_STATE, bufObj, 0, 0);
	tlv_dserialize(&prReqIE.confErr, WPS_ID_CONFIG_ERROR, bufObj, 0, 0);
	tlv_dserialize(&prReqIE.pwdId, WPS_ID_DEVICE_PWD_ID, bufObj, 0, 0);

	if (prReqIE.pwdId.m_data == WPS_DEVICEPWDID_PUSH_BTN) {
		TUTRACE((TUTRACE_ERR, "\n\nPush Button sta detect\n\n"));
		wps_pb_update_pushtime(mac, prReqIE.uuid.m_data);
	}

	buffobj_del(bufObj);

	return 0;
}

/* PBC Overlapped detection */
int
wps_eap_process_msg(char *buf, int buflen)
{
	int ret = WPS_CONT;
	int eapid = WPS_EAP_ID_NONE;
	struct ether_header *eth;
	eapol_header_t *eapol;
	eap_header_t *eap;
	char ifname[IFNAMSIZ] = {0};
	int mode = -1;
#ifdef BCMWPSAP
	CTlvSsrIE ssrmsg;
	char authorizedMacs[SIZE_MAC_ADDR * SIZE_AUTHORIZEDMACS_NUM] = {0};
	int authorizedMacs_len = 0;
#endif
	wps_app_t *wps_app = get_wps_app();
	wpsap_wksp_t *wksp = (wpsap_wksp_t *)wps_app->wksp;

	/* Retreive wireless ifname of eap packets */
	strncpy(ifname, eap_hndl.ifname, sizeof(ifname));

	/* check is it bcmevent? */
	eth = (struct ether_header *) buf;
	if (ntohs(eth->ether_type) == ETHER_TYPE_BRCM) {
		bcm_event_t *pvt_data = (bcm_event_t *)buf;
		wl_event_msg_t *event = &(pvt_data->event);
		uint8 *addr = (uint8 *)&(event->addr);
		int totlen = ntohl(event->datalen);
		uint32 event_type = ntohl(event->event_type);
		uint8 *ie = (uint8 *)(event + 1) + DOT11_MGMT_HDR_LEN;
		bcm_tlv_t *elt = (bcm_tlv_t *)ie;
		int wps_iebuf_len;
		unsigned char wps_iebuf[WPS_IE_BUF_LEN];

		if (ntohs(pvt_data->bcm_hdr.usr_subtype) != BCMILCP_BCM_SUBTYPE_EVENT)
			return WPS_CONT;

		if (event_type == WLC_E_PROBREQ_MSG) {
			/*
			 * WPS 2.0, test plan 4.2.9 Construct a WPS IE from Probe request.
			 * It may need to reassemble from multiple WPS IEs
			 */
			memset(wps_iebuf, 0, sizeof(wps_iebuf));
			wps_iebuf_len = 0;

			while (totlen >= 2) {
				int eltlen = elt->len;

				/* validate remaining totlen */
				if ((elt->id == 0xdd) && (totlen >= (eltlen + 2)) &&
				    (elt->len >= WPA_IE_OUITYPE_LEN) &&
				    !bcmp(elt->data, WPA_OUI"\x04", WPA_IE_OUITYPE_LEN)) {

					/* comment out for now as it is flooding the screen and
					 * makes debugging difficult it would be nice to have
					 * debug levels to enable/disable some of the messages.
					 */
					/* TUTRACE((TUTRACE_INFO, "found WPS probe request \n")); */

					/* Keep constructing WPS IE if we have enough buf */
					if (wps_iebuf_len + elt->len - 4 <= WPS_IE_BUF_LEN) {
						memcpy(&wps_iebuf[wps_iebuf_len], &elt->data[4],
							elt->len - 4);
						wps_iebuf_len += elt->len - 4;
					}
				}
				elt = (bcm_tlv_t*)((uint8*)elt + (eltlen + 2));
				ie = (uint8 *)elt;
				totlen -= (eltlen + 2);
			}

			/* WPS IEs exist */
			if (wps_iebuf_len) {
				wps_eap_parse_prob_reqIE(addr, wps_iebuf, wps_iebuf_len, ifname);
#if defined(BCMWPSAP) && defined(__CONFIG_WFI__)
				wps_wfi_process_prb_req(wps_iebuf, wps_iebuf_len, addr,
					event->ifname);
#endif /* BCMWPSAP && __CONFIG_WFI__ */
			}
			else {
				/* No WPS IEs */
#if defined(BCMWPSAP) && defined(__CONFIG_WFI__)
				/* do wps_wfi_del_sta */
				wps_wfi_process_prb_req(NULL, 0, addr, event->ifname);
#endif /* BCMWPSAP && __CONFIG_WFI__ */
			}

			return ret;
		}
#if defined(BCMWPSAP) && defined(__CONFIG_WFI__)
		else if ((event_type == WLC_E_ASSOC_IND) || (event_type == WLC_E_REASSOC_IND)) {
			wps_wfi_process_sta_ind(ie, totlen, addr, event->ifname, TRUE);
			return ret;
		}
		else if ((event_type == WLC_E_DISASSOC_IND) ||	(event_type == WLC_E_DEAUTH_IND)) {
			wps_wfi_process_sta_ind(ie, totlen, addr, event->ifname, FALSE);
			return ret;
		}
#endif /* BCMWPSAP && __CONFIG_WFI__ */
		else {
			TUTRACE((TUTRACE_ERR, "%s: recved wl event type %d\n",
				pvt_data->event.ifname, event_type));
			return ret;
		}
	}

	/* ETHER_TYPE_802_1X packets */

	/* if is not EAP IDENTITY and there is no existing WPS running, exit   */
	eapol = (eapol_header_t *)buf;
	eap = (eap_header_t *)eapol->body;

	if ((eapid = wps_eap_get_id(buf)) == WPS_EAP_ID_NONE && !wksp) {
#if defined(BCMWPSAP) && defined(__CONFIG_WFI__)
		if (eap->code == EAP_RESPONSE && eap->type == EAP_NAK)
			wps_wfi_rejected();
#endif /* BCMWPSAP && __CONFIG_WFI__ */
		return ret;
	}

#ifdef WPS_NFC_DEVICE
	/* Check if NFC state is idle */
	if (!wps_nfc_is_idle())
		return ret;
#endif

	/* Indicate station associated */
	if (eapid != WPS_EAP_ID_NONE) {
		wps_setProcessStates(WPS_ASSOCIATED);
	}

	if (wksp) {
		int other_sta_coming = 0;

		if (wps_app->sc_mode != SCMODE_STA_ENROLLEE &&
		    wps_app->sc_mode != SCMODE_STA_REGISTRAR) {
			/*
			  * Ingnore other eap packet from other interfaces,
			  * if the ap session is opened.
			  */
			if (strcmp(ifname, wksp->ifname) != 0) {
				TUTRACE((TUTRACE_ERR, "Expect EAP from %s, rather than %s\n",
					wksp->ifname, ifname));
				return WPS_CONT;
			}

			/* Check eap source mac */
			if (memcmp(wksp->mac_sta, eapol->eth.ether_shost, 6) != 0) {
#ifdef _TUDEBUGTRACE
				char tmp[64], tmp1[64];
#endif
				/* Update the first sta registrar mac if the mac_sta is empty,
				 * SCMODE_AP_ENROLLEE trigger by GUI
				 */
				if (eapid == WPS_EAP_ID_REGISTRAR &&
				    memcmp(wksp->mac_sta, "\0\0\0\0\0\0", 6) == 0) {
					memcpy(wksp->mac_sta, eapol->eth.ether_shost, 6);
				} else {
					TUTRACE((TUTRACE_ERR,
						"Expect EAP from %s, rather than %s\n",
						ether_etoa(wksp->mac_sta, tmp),
						ether_etoa(eapol->eth.ether_shost, tmp1)));
					other_sta_coming = 1;
				}
			}
		}
#ifdef BCMWPSAP
		if (eapid == WPS_EAP_ID_REGISTRAR &&
		    WPS_IS_PROXY(wps_app->sc_mode) &&
		    wps_getenrState(wksp->mc) == MSTART) {
			/* WAR, special patch for DTM 1.4 test case 934 */
#ifdef _TUDEBUGTRACE
			int enr_status = wps_getenrState(wksp->mc);
			int reg_status = wps_getregState(wksp->mc);
			TUTRACE((TUTRACE_ERR, "enr_status = %d, reg_status = %d\n", enr_status,
				reg_status));
#endif
			wps_close_session();
		}
		else if (other_sta_coming) {
			/* Ingore other mac */
			return WPS_CONT;
		}
		else
#endif /* BCMWPSAP */
		{
			return (*wps_app->process)(wps_app->wksp, buf, buflen, TRANSPORT_TYPE_EAP);
		}
	}

	/* This packet might be enrollee or registrar, start wps with suit command */
	if (eapid == WPS_EAP_ID_ENROLLEE) {
		if (atoi(wps_ui_get_env("wps_config_command")) == WPS_UI_CMD_START) {
			/* received EAP enrollee packet and UI button was pushed */
		}
		else {
			/* clear station pin for AP/Router response M2D */
			wps_ui_set_env("wps_sta_pin", "");
		}

		mode = SCMODE_AP_REGISTRAR;

		TUTRACE((TUTRACE_ERR, "eapid = WPS_EAP_ID_ENROLLEE mode = %s\n",
			WPS_SMODE2STR(mode)));
	}
	/*
	 * Only start enrollee when we are not doing built-in reg,
	 * Special patch for DTM 1.4 test case 913
	 */
	else if (eapid == WPS_EAP_ID_REGISTRAR &&
		(!wps_ui_is_pending() || !strcmp(wps_safe_get_conf("wps_wer_override"), "1"))) {
		/* Allow or deny Wireless External Registrar get or configure AP through AP PIN */
		if (strcmp(wps_safe_get_conf("wps_wer_mode"), "deny") == 0) {
			TUTRACE((TUTRACE_INFO, "Deny Wireless ER get or configure AP\n"));
			return ret;
		}

		/* receive EAP packet and no buttion in UI pushed */
		mode = SCMODE_AP_ENROLLEE;

		/* WAR: 1 second delay for Win7 cannot display PBC icon problem
		 * As we know we add PBC support in the M1 config method to hack Win7 cannot
		 * display issue.  But it seems has other factory effect this issue.
		 * We guess Win7 use two threads to handle the Probe Req/Resp and WPS message,
		 * and it seems has AP info database race condition problem between two threads.
		 * So, here we dealy 1 second to start WPS in SCMODE_AP_ENROLLEE.
		 */
		WpsSleep(1);

		TUTRACE((TUTRACE_ERR, "eapid = WPS_EAP_ID_REGISTRAR mode = SCMODE_AP_ENROLLEE\n"));
	}
	else {
		return ret;
	}

#ifdef BCMWPSAP
	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0) {
		/*
		 * Get union (WSC 2.0 page 79, 8.4.1) of information received from all Registrars,
		 * use recently authorizedMACs list and update union attriabutes to ssrmsg
		 */
		ap_ssr_get_union_attributes(&ssrmsg, authorizedMacs, &authorizedMacs_len);
	}

	ret = wpsap_open_session(wps_app, mode, eapol->eth.ether_dhost, eapol->eth.ether_shost,
		ifname, NULL, NULL, authorizedMacs, authorizedMacs_len, 0, 0);
#endif /* BCMWPSAP */
	return ret;
}

/* WPS eap init */
int
wps_eap_get_handle()
{
	return eap_hndl.handle;
}

int
wps_eap_init()
{
	/* Init eap_hndl */
	memset(&eap_hndl, 0, sizeof(eap_hndl));

	eap_hndl.type = WPS_RECEIVE_PKT_EAP;
	eap_hndl.handle = wps_osl_eap_handle_init();
	if (eap_hndl.handle == -1)
		return -1;

	wps_hndl_add(&eap_hndl);
	return 0;
}

void
wps_eap_deinit()
{
#ifdef WPS_UPNP_DEVICE
	PREB_REQ_STA_SCB *scb;
#endif
	wps_hndl_del(&eap_hndl);
	wps_osl_eap_handle_deinit(eap_hndl.handle);

#ifdef WPS_UPNP_DEVICE
	/* Free prebe request scb list */
	while (preb_req_scb) {
		scb = preb_req_scb;
		preb_req_scb = preb_req_scb->next;
		free(scb);
	}
#endif
	return;
}
