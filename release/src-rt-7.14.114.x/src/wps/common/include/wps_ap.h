/*
 * WPS AP
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_ap.h 295985 2011-11-13 02:57:31Z $
 */

#ifndef __WPS_AP_H__
#define __WPS_AP_H__


#define WPS_OVERALL_TIMEOUT	140 /* 120 + 20 for VISTA testing tolerance */
#define WPS_MSG_TIMEOUT	30

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

typedef struct {
	int sc_mode;
	int ess_id;
	char ifname[IFNAMSIZ];
	unsigned char mac_ap[6];
	unsigned char mac_sta[6];
	unsigned char *pre_nonce;
	unsigned char *pre_privkey;

	bool config_state;

	void *mc;			/* state machine context */

	unsigned long pkt_count;
	unsigned long pkt_count_prv;

	int wps_state;			/* state machine operating state */
	unsigned long start_time;	/* workspace init time */
	unsigned long touch_time;	/* workspace latest operating time */

	int return_code;

	/* WSC 2.0 */
	bool b_wps_version2;
	bool b_reqToEnroll;
	bool b_nwKeyShareable;
	uint32 authorizedMacs_len;
	uint8 authorizedMacs[SIZE_MAC_ADDR * SIZE_AUTHORIZEDMACS_NUM];
	int eap_frag_threshold;
} wpsap_wksp_t;

/*
 * implemented in wps_ap.c
 */
uint32 wpsap_osl_eapol_send_data(char *dataBuffer, uint32 dataLen);
char* wpsap_osl_eapol_parse_msg(char *msg, int msg_len, int *len);


#endif /* __WPS_AP_H__ */
