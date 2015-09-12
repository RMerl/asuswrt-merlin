/*
 * Broadcom WPS Set Selected Registrar
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ap_ssr.h 383928 2013-02-08 04:15:06Z $
 */

#ifndef __AP_SSR_H__
#define __AP_SSR_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

/* WSC 2.0 */
/* Find scb type */
#define WPS_SSR_SCB_FIND_ANY		0
#define WPS_SSR_SCB_FIND_IPADDR		1
#define WPS_SSR_SCB_FIND_AUTHORIED_MAC	2
#define WPS_SSR_SCB_FIND_UUID_R		3
#define WPS_SSR_SCB_FIND_VERSION1	4

#define WPS_SSR_SCB_SEARCH_ONLY		1
#define WPS_SSR_SCB_ENTER		2


typedef struct wps_ssr_scb_s {
	unsigned char version; /* V1 */
	unsigned char version2; /* V2 */
	unsigned char scState;
	unsigned long upd_time;
	char ipaddr[16]; /* string format */
	char wps_ifname[IFNAMSIZ];
	int authorizedMacs_len;
	char authorizedMacs[SIZE_MAC_ADDR * SIZE_AUTHORIZEDMACS_NUM]; /* <= 30 B */
	char uuid_R[SIZE_UUID]; /* unique identifier for registrar = 16 B */
	unsigned short selRegCfgMethods;
	unsigned short devPwdId;
	struct wps_ssr_scb_s *next;
} WPS_SSR_SCB;

static const char wildcard_authorizedMacs[SIZE_MAC_ADDR] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/*
 * UPnP command between WPS and WFA device
 */

/* Functions */
uint32 ap_ssr_init();
uint32 ap_ssr_deinit();
int ap_ssr_set_scb(char *ipaddr, CTlvSsrIE *ssrmsg, char *wps_ifname, unsigned long upd_time);
int ap_ssr_get_scb_num();
CTlvSsrIE *ap_ssr_get_scb_info(char *ipaddr, CTlvSsrIE *ssrmsg);
CTlvSsrIE *ap_ssr_get_recent_scb_info(CTlvSsrIE *ssrmsg);

int ap_ssr_get_authorizedMacs(char *authorizedMacs_buf);
int ap_ssr_get_union_attributes(CTlvSsrIE *ssrmsg, char *authorizedMacs_buf,
	int *authorizedMacs_len);
char *ap_ssr_get_ipaddr(char* uuid_r, char *enroll_mac);
char *ap_ssr_get_wps_ifname(char *ipaddr, char *wps_ifname);
void ap_ssr_free_all_scb();

#ifdef _TUDEBUGTRACE
void ap_ssr_dump_all_scb(char *title);
#endif

#ifdef __cplusplus
}
#endif

#endif	/* __AP_SSR_H__ */
