/*
 * WPS Enrollee API
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_enrapi.h 383928 2013-02-08 04:15:06Z $
 */

#ifndef __WPS_ENROLLEE_API_H__
#define __WPS_ENROLLEE_API_H__

#include <wps_devinfo.h>

/* eap state machine states */
enum {
	INIT = 0,
	EAPOL_START_SENT,
	EAP_IDENTITY_SENT,
	PROCESSING_PROTOCOL,
	REG_SUCCESSFUL,
	REG_FAILED,
	EAP_TIMEOUT,
	EAP_FAILURE
};

/* returned to callers of process_registrar_message or process_timers */
enum {
	INTERNAL_ERROR = 0, /* internal processing erro (eap not initialized ..)  */
	REG_SUCCESS, /* registration completed */
	REG_FAILURE /* registration failed  */
};

enum {
	PBC_NOT_FOUND = 0,
	PBC_FOUND_OK,
	PBC_OVERLAP,
	PBC_TIMEOUT
};

/* AuthorizedMAC */
enum {
	AUTHO_MAC_NOT_FOUND = 0,
	AUTHO_MAC_PIN_FOUND,
	AUTHO_MAC_PBC_FOUND,
	AUTHO_MAC_WC_PIN_FOUND,
	AUTHO_MAC_WC_PBC_FOUND,
	AUTHO_MAC_TIMEOUT
};

#define ENCRYPT_NONE 1
#define ENCRYPT_WEP 2
#define ENCRYPT_TKIP 4
#define ENCRYPT_AES 8
typedef struct wps_ap_list_info {
	bool        used;
	uint8       ssid[SIZE_SSID_LENGTH];
	uint8       ssidLen;
	uint8       BSSID[6];
	uint8       *ie_buf;
	uint32      ie_buflen;
	uint8       channel;
	uint16      band;
	uint8       wep;
	uint8       scstate;
	uint8       version2; /* WSC 2.0 support */
	uint8       authorizedMACs[SIZE_MAC_ADDR * 5]; /* WSC 2.0 authorizedMACS */
} wps_ap_list_info_t;

uint32 wpssta_enr_init(DevInfo *user_info);
uint32 wpssta_reg_init(DevInfo *user_info, char *nwKey, char *bssid);
bool wps_is_wep_incompatible(bool role_reg);
void wps_cleanup(void);
uint32 wpssta_start_enrollment(char *dev_pin, unsigned long time);
uint32 wpssta_start_enrollment_devpwid(char *dev_pin, uint16 devicepwid, unsigned long time);
uint32 wpssta_start_registration(char *ap_pin, unsigned long time);
uint32 wpssta_start_registration_devpwid(char *ap_pin, uint8 *pub_key_hash, uint16 devicepwid,
	unsigned long time);
void wpssta_get_credentials(WpsEnrCred* credential, const char *ssid, int len);
void wpssta_get_reg_M7credentials(WpsEnrCred* credential);
void wpssta_get_reg_M8credentials(WpsEnrCred* credential);
int wps_process_ap_msg(char *eapol_msg, int len);

int wps_get_msg_to_send(char **data, uint32 time);
int wps_get_eapol_msg_to_send(char **data, uint32 time);
int wps_get_retrans_msg_to_send(char **data, uint32 time, char *mtype);
int wps_get_frag_msg_to_send(char **data, uint32 time);

int wps_eap_check_timer(uint32 time);
int wps_get_aplist(wps_ap_list_info_t *list_in, wps_ap_list_info_t *list_out);
int wps_get_pin_aplist(wps_ap_list_info_t *list_in, wps_ap_list_info_t *list_out);
int wps_get_pbc_ap(wps_ap_list_info_t *list_in, char *bssid, char *ssid, uint8 *wep,
	unsigned long time, char start);
int wps_get_amac_ap(wps_ap_list_info_t *list_in, uint8 *mac, char wildcard, char *bssid,
	char *ssid, uint8 *wep, unsigned long time, char start);
uint32 wps_build_pbc_proberq(uint8 *buff, int len);
uint32 wps_eap_send_msg(char * dataBuffer, uint32 dataLen);
int wps_get_sent_msg_id(void);
bool wps_get_select_reg(const wps_ap_list_info_t *ap_in);

char* wps_get_msg_string(int mtype);
int wps_get_recv_msg_id(void);
int wps_get_eap_state(void);
char wps_get_msg_type(char *eapol_msg, int len);
bool wps_validateChecksum(IN unsigned long int PIN);
uint32 wps_update_RFBand(uint8 rfBand);

/* WSC 2.0 */
uint32 wps_build_def_proberq(uint8 *buff, int len);
uint32 wps_build_def_assocrq(uint8 *buff, int len);

#ifdef WFA_WPS_20_TESTBED
uint32 wps_update_partial_ie(uint8 *buff, int buflen, uint8 *ie, uint8 ie_len,
	uint8 *updie, uint8 updie_len);
int sta_eap_sm_set_eap_frag_threshold(int eap_frag_threshold);
#endif /* WFA_WPS_20_TESTBED */

#endif /* __WPS_ENROLLEE_API_H__ */
