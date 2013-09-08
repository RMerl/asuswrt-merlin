/*
 * WPS AP UPnP state machin
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ap_upnp_sm.c 295630 2011-11-11 04:45:47Z $
 */
#ifdef WPS_UPNP_DEVICE

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) || defined(__ECOS)
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef __ECOS
#include <net/if.h>
#include <sys/param.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#endif /* __linux__ || __ECOS */

#ifdef WIN32
#include <windows.h>
#endif /* WIN32 */

#include <tutrace.h>
#include <wpscommon.h>
#include <wpsheaders.h>

#include <wps_apapi.h>
#include <sminfo.h>
#include <reg_proto.h>
#include <reg_prototlv.h>
#include <statemachine.h>
#include <wpsapi.h>
#include <ap_upnp_sm.h>
#include <ap_eap_sm.h>
#include <ap_ssr.h>

static void ap_upnp_sm_init_notify_wlan_event(void);
static UPNP_WPS_AP_STATE s_apUpnpState;
static UPNP_WPS_AP_STATE *apUpnpState = NULL;

/* WSC 2.0 */
static uint8 apUpnp_wps_version2 = 0;

static void
ap_upnp_sm_init_notify_wlan_event(void)
{
	unsigned char *macaddr = wps_get_mac(apUpnpState->mc_dev);

	if (apUpnpState->init_wlan_event)
		(*apUpnpState->init_wlan_event)(apUpnpState->if_instance, (char *)macaddr, 1);

	return;
}

uint32
ap_upnp_sm_init(void *mc_dev, void * init_wlan_event, void *update_wlan_event,
	void *send_data, void *parse_msg, int instance)
{
	unsigned char *mac;

	/* Save interface name */
	mac = wps_get_mac_income(mc_dev);
	if  (!mac)
		return WPS_ERR_SYSTEM;

	memset(&s_apUpnpState, 0, sizeof(UPNP_WPS_AP_STATE));
	apUpnpState = &s_apUpnpState;

	apUpnpState->if_instance = instance;

	apUpnpState->mc_dev = mc_dev;

	apUpnpState->m_waitForGetDevInfoResp = false;
	apUpnpState->m_waitForPutMessageResp = false;
	if (init_wlan_event)
		apUpnpState->init_wlan_event =
			(void (*)(int, char *, int))init_wlan_event;

	if (update_wlan_event)
		apUpnpState->update_wlan_event =
			(void (*)(int, unsigned char *, char *, int, int, char))update_wlan_event;

	if (send_data)
		apUpnpState->send_data = (uint32 (*)(int, char *, uint32, int))send_data;

	if (parse_msg)
		apUpnpState->parse_msg = (char* (*)(char *, int, int *, int *, char *))parse_msg;

	/* WSC 2.0 */
	apUpnp_wps_version2 = wps_get_version2(mc_dev);

	/* Notify WLAN Evetn */
	ap_upnp_sm_init_notify_wlan_event();

	return WPS_SUCCESS;
}

uint32
ap_upnp_sm_deinit()
{
	if (!apUpnpState) {
		TUTRACE((TUTRACE_ERR, "Not initialized; Returning\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	return WPS_SUCCESS;
}

uint32
ap_upnp_sm_sendMsg(char *dataBuffer, uint32 dataLen)
{
	unsigned char *macaddr = wps_get_mac(apUpnpState->mc_dev);

	TUTRACE((TUTRACE_INFO, "In %s buffer Length = %d\n", __FUNCTION__, dataLen));

	if ((!dataBuffer) || (! dataLen)) {
		TUTRACE((TUTRACE_ERR, "Invalid Parameters\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	if (apUpnpState->m_waitForGetDevInfoResp) {
		/* Send message back to UPnP thread with GDIR type. */
		if (apUpnpState->send_data)
			(*apUpnpState->send_data)(apUpnpState->if_instance, dataBuffer, dataLen,
				UPNP_WPS_TYPE_GDIR);

		apUpnpState->m_waitForGetDevInfoResp = false;
	}
	else if (apUpnpState->m_waitForPutMessageResp) {
		/* Send message back to UPnP thread. */
		if (apUpnpState->send_data)
			(*apUpnpState->send_data)(apUpnpState->if_instance, dataBuffer, dataLen,
				UPNP_WPS_TYPE_PMR);

		apUpnpState->m_waitForPutMessageResp = false;
	}
	else {
		/* Send message back to UPnP thread. */
		if (apUpnpState->update_wlan_event)
			(*apUpnpState->update_wlan_event)(apUpnpState->if_instance, macaddr,
				dataBuffer, dataLen, 0, WPS_WLAN_EVENT_TYPE_EAP_FRAME);
	}

	return WPS_SUCCESS;
}

/* process message from WFADevice */
uint32
ap_upnp_sm_process_msg(char *msg, int msg_len, unsigned long time)
{
	char *data = NULL;
	int len = 0, type = 0, ret = WPS_CONT;
	char ssr_client[sizeof("255.255.255.255")] = {0};
	CTlvSsrIE ssrmsg;
	int scb_num;
	char authorizedMacs_buf[SIZE_MAC_ADDR * SIZE_AUTHORIZEDMACS_NUM] = {0};
	int authorizedMacs_len = 0;
	BufferObj *authorizedMacs_bufObj = NULL;


	/* Check initialization */
	if (apUpnpState == 0)
		return WPS_MESSAGE_PROCESSING_ERROR;

	if (apUpnpState->parse_msg)
		data = (*apUpnpState->parse_msg)(msg, msg_len, &len, &type, ssr_client);
	if (!data) {
		TUTRACE((TUTRACE_ERR,  "Missing data in message\n"));
		return WPS_ERR_GENERIC;
	}

	TUTRACE((TUTRACE_INFO, "In %s buffer Length = %d\n", __FUNCTION__, len));

	switch (type) {
		case UPNP_WPS_TYPE_SSR:
			TUTRACE((TUTRACE_INFO, "Set Selected Registrar received; "
				"InvokeCallback\n"));
			break;

		case UPNP_WPS_TYPE_PMR:
			TUTRACE((TUTRACE_INFO, "Wait For Put Message Resp received; "
				"InvokeCallback\n"));
			apUpnpState->m_waitForPutMessageResp = true;
			break;

		case UPNP_WPS_TYPE_GDIR:
			TUTRACE((TUTRACE_INFO, "Wait For Get DevInfo Resp received; "
				"InvokeCallback\n"));

			/* Update WPS state */
			wps_setUpnpDevGetDeviceInfo(apUpnpState->mc_dev, true);

			apUpnpState->m_waitForGetDevInfoResp = true;
			break;

		case UPNP_WPS_TYPE_PWR:
			TUTRACE((TUTRACE_INFO, "Put WLAN Response received; InvokeCallback\n"));
			break;

		/* WSC 2.0 */
		case UPNP_WPS_TYPE_DISCONNECT:
			TUTRACE((TUTRACE_INFO, "Subscriber unreachable DISCONNECT\n"));
			break;

		default:
			TUTRACE((TUTRACE_INFO, "Type Unknown; continuing..\n"));
			return ret;
	}

	/* call callback */
	if (type == UPNP_WPS_TYPE_SSR) {
		memset(&ssrmsg, 0, sizeof(CTlvSsrIE));

		/* Get SSR information for proxy mode */
		ret = wps_get_upnpDevSSR(apUpnpState->mc_dev, (void *)data, len, &ssrmsg);
		if (ret == WPS_SUCCESS) {
			/* Set SSR information to scb list */
			ap_ssr_set_scb(ssr_client, &ssrmsg, NULL, time);

			/*
			* delete authorizedMacs possible contents which allocated
			* in wps_get_upnpDevSSR
			*/
			subtlv_delete(&ssrmsg.authorizedMacs, 1);

			if (apUpnp_wps_version2) {
				/*
				 * Get union (WSC 2.0 page 79, 8.4.1) of information received
				 * from all Registrars, use recently authorizedMACs list and update
				 * union attriabutes to ssrmsg
				 */
				ap_ssr_get_union_attributes(&ssrmsg, authorizedMacs_buf,
					&authorizedMacs_len);

				/* Reset ssrmsg.authorizedMacs */
				ssrmsg.authorizedMacs.m_data = NULL;
				ssrmsg.authorizedMacs.subtlvbase.m_len = 0;

				/* Dserialize ssrmsg.authorizedMacs from authorizedMacs_buf */
				if (authorizedMacs_len) {
					authorizedMacs_bufObj = buffobj_new();
					if (authorizedMacs_bufObj) {
						/*
						 * Serialize authorizedMacs_buf to
						 * authorizedMacs_bufObj
						 */
						subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS,
							authorizedMacs_bufObj,
							authorizedMacs_buf,
							authorizedMacs_len);
						buffobj_Rewind(authorizedMacs_bufObj);
						/*
						 * De-serialize the authorizedMacs data
						 * to get the TLVs
						 */
						subtlv_dserialize(&ssrmsg.authorizedMacs,
							WPS_WFA_SUBID_AUTHORIZED_MACS,
							authorizedMacs_bufObj, 0, 0);
					}
				}
			}

			/* Process upnpDevSSR */
			ret = wps_upnpDevSSR(apUpnpState->mc_dev, &ssrmsg);

			/* Free authorizedMacs_bufObj */
			if (authorizedMacs_bufObj)
				buffobj_del(authorizedMacs_bufObj);
		}
	}
	else if (type == UPNP_WPS_TYPE_DISCONNECT) {
		if (apUpnp_wps_version2 && ap_ssr_get_scb_num()) {
			memset(&ssrmsg, 0, sizeof(CTlvSsrIE));

			/* Get this scb info first */
			ap_ssr_get_scb_info(ssr_client, &ssrmsg);

			/* Set selReg FALSE to remove this scb */
			ssrmsg.selReg.m_data = 0;
			ap_ssr_set_scb(ssr_client, &ssrmsg, NULL, time);

			/* Get union attributes */
			scb_num = ap_ssr_get_union_attributes(&ssrmsg, authorizedMacs_buf,
				&authorizedMacs_len);

			/* If scb_num is not 0 means some ER is doing SSR TRUE */
			if (scb_num) {
				/* Use newest scb's information */
				ap_ssr_get_recent_scb_info(&ssrmsg);
			}

			/* Reset ssrmsg.authorizedMacs */
			ssrmsg.authorizedMacs.m_data = NULL;
			ssrmsg.authorizedMacs.subtlvbase.m_len = 0;

			/* Dserialize ssrmsg.authorizedMacs from authorizedMacs_buf */
			if (authorizedMacs_len) {
				authorizedMacs_bufObj = buffobj_new();
				if (authorizedMacs_bufObj) {
					/*
					 * Serialize authorizedMacs_buf to
					 * authorizedMacs_bufObj
					 */
					subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS,
						authorizedMacs_bufObj,
						authorizedMacs_buf,
						authorizedMacs_len);
					buffobj_Rewind(authorizedMacs_bufObj);
					/*
					 * De-serialize the authorizedMacs data
					 * to get the TLVs
					 */
					subtlv_dserialize(&ssrmsg.authorizedMacs,
						WPS_WFA_SUBID_AUTHORIZED_MACS,
						authorizedMacs_bufObj, 0, 0);
				}
			}

			/* Process upnpDevSSR */
			ret = wps_upnpDevSSR(apUpnpState->mc_dev, &ssrmsg);

			/* Free authorizedMacs_bufObj */
			if (authorizedMacs_bufObj)
				buffobj_del(authorizedMacs_bufObj);
		}
	}
	else {
		/* reset msg_len */
		apUpnpState->msg_len = sizeof(apUpnpState->msg_to_send);
		ret = wps_processMsg(apUpnpState->mc_dev, (void *)data, len,
			apUpnpState->msg_to_send, (uint32 *)&apUpnpState->msg_len,
			TRANSPORT_TYPE_UPNP_DEV);
	}

	return ret;
}

int
ap_upnp_sm_get_msg_to_send(char **data)
{
	if (!apUpnpState) {
		*data = NULL;
		return -1;
	}

	*data = apUpnpState->msg_to_send;
	return apUpnpState->msg_len;
}
#endif /* WPS_UPNP_DEVICE */
