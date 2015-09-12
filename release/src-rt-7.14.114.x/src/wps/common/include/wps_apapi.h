/*
 * WPS Registratar API
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_apapi.h 383928 2013-02-08 04:15:06Z $
 */

#ifndef _WPS_AP_API_H_
#define _WPS_AP_API_H_

#include <typedefs.h>
#include <info.h>


typedef enum {
	SEARCH_ONLY,
	SEARCH_ENTER
} sta_lookup_mode_t;

/* ap eap state machine states */
typedef enum {
	INIT = 0,
	EAPOL_START_SENT,
	EAP_START_SEND,
	PROCESSING_PROTOCOL,
	REG_SUCCESSFUL,
	REG_FAILED,
	EAP_TIMEOUT,
	EAP_FAILURED
} ap_eap_state_t;

typedef enum {
	WPS_PROC_STATE_INIT = 0,
	WPS_PROC_STATE_PROCESSING,
	WPS_PROC_STATE_SUCCESS,
	WPS_STATUS_MSGERR,
	WPS_STATUS_TIMEOUT
} WPS_PROC_STATE_E;

#define EAP_MAX_RESEND_COUNT 5

int wps_get_mode(void *mc_dev);
int wps_processMsg(void *mc_dev, void *inBuffer, uint32 in_len, void *outBuffer, uint32 *out_len,
	TRANSPORT_TYPE m_transportType);
uint32 wpsap_start_enrollment(void *mc_dev, char *ap_pin);
uint32 wpsap_start_enrollment_devpwid(void *mc_dev, char *ap_pin, uint16 devicepwid);
uint32 wpsap_start_registration(void *mc_dev, char *sta_pin);
uint32 wpsap_start_registration_devpwid(void *mc_dev, char *sta_pin, uint8 *pub_key_hash,
	uint16 devicepwid);
unsigned char * wps_get_mac_income(void *mc_dev);
unsigned char *wps_get_mac(void *mc_dev);
uint8 wps_get_version2(void *mc_dev);
bool ap_api_is_recvd_m2d(void *mc_dev);

#endif /* _WPS_AP_API_H_ */
