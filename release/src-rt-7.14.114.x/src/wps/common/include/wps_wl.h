/*
 * WPS wireless related
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_wl.h 383928 2013-02-08 04:15:06Z $
 */
#ifndef _WPS_WL_H_
#define _WPS_WL_H_

#include <typedefs.h>
#include <proto/ethernet.h>
#include <proto/wpa.h>
#include <wlioctl.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <info.h>

#define WPS_IE_TYPE_SET_BEACON_IE		1
#define WPS_IE_TYPE_SET_PROBE_REQUEST_IE	2
#define WPS_IE_TYPE_SET_PROBE_RESPONSE_IE	3
#define WPS_IE_TYPE_SET_ASSOC_REQUEST_IE	4
#define WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE	5

#define OUITYPE_WPS				4
#define OUITYPE_PROVISION_STATIC_WEP		5

#define WPS_WLAKM_BOTH(akm) ((akm & WPA_AUTH_PSK) && (akm & WPA2_AUTH_PSK))
#define WPS_WLAKM_PSK2(akm) ((akm & WPA2_AUTH_PSK))
#define WPS_WLAKM_PSK(akm) ((akm & WPA_AUTH_PSK))
#define WPS_WLAKM_NONE(akm) (!(WPS_WLAKM_BOTH(akm) | WPS_WLAKM_PSK2(akm) | WPS_WLAKM_PSK(akm)))

#define WPS_WLENCR_BOTH(wsec) ((wsec & TKIP_ENABLED) && (wsec & AES_ENABLED))
#define WPS_WLENCR_TKIP(wsec) (wsec & TKIP_ENABLED)
#define WPS_WLENCR_AES(wsec) (wsec & AES_ENABLED)

/*
 * implemented in wps_linux.c
 */
int wps_set_wsec(int ess_id, char *ifname, void *credential, int mode);
#ifndef WPS_ROUTER
int wps_set_wps_ie(void *bcmdev, unsigned char *p_data, int length, unsigned int cmdtype);
#endif /* !WPS_ROUTER */
int wps_ioctl(char *ifname, int cmd, void *buf, int len);

/* OS dependent EAP APIs */
int wps_set_ifname(char *ifname);
uint32 wps_eapol_send_data(char *dataBuffer, uint32 dataLen);
char* wps_eapol_parse_msg(char *msg, int msg_len, int *len);

/*
 * implemented in wps_wl.c
 */
int wps_deauthenticate(unsigned char *bssid, unsigned char *sta_mac, int reason);

int wps_wl_deauthenticate(char *wl_if, unsigned char *sta_mac, int reason);
int wps_wl_del_wps_ie(char *wl_if, unsigned int cmdtype, unsigned char ouitype);
int wps_wl_set_wps_ie(char *wl_if, unsigned char *p_data, int length, unsigned int cmdtype,
	unsigned char ouitype);
int wps_wl_open_wps_window(char *ifname);
int wps_wl_close_wps_window(char *ifname);
int wps_wl_get_maclist(char *ifname, char *buf, int count);
#ifdef BCMWPSAPSTA
int wps_wl_bss_config(char *ifname, int enabled);
#endif
extern char *ether_etoa(const unsigned char *e, char *a);

#endif /* _WPS_WL_H_ */
