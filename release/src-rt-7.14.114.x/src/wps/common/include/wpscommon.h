/*
 * WPS Common
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscommon.h 383928 2013-02-08 04:15:06Z $
 */

#ifndef _WPS_COMMON_
#define _WPS_COMMON_

#include <wpstypes.h>
#include <portability.h>
#include <wps_utils.h>


/* Config methods */
#define WPS_CONFMET_USBA            0x0001	/* Deprecated in WSC 2.0 */
#define WPS_CONFMET_ETHERNET        0x0002	/* Deprecated in WSC 2.0 */
#define WPS_CONFMET_LABEL           0x0004
#define WPS_CONFMET_DISPLAY         0x0008
#define WPS_CONFMET_EXT_NFC_TOK     0x0010
#define WPS_CONFMET_INT_NFC_TOK     0x0020
#define WPS_CONFMET_NFC_INTF        0x0040
#define WPS_CONFMET_PBC             0x0080
#define WPS_CONFMET_KEYPAD          0x0100
/* WSC 2.0 */
#define WPS_CONFMET_VIRT_PBC        0x0280
#define WPS_CONFMET_PHY_PBC         0x0480
#define WPS_CONFMET_VIRT_DISPLAY    0x2008
#define WPS_CONFMET_PHY_DISPLAY     0x4008


#define REGISTRAR_ID_STRING        "WFA-SimpleConfig-Registrar-1-0"
#define ENROLLEE_ID_STRING        "WFA-SimpleConfig-Enrollee-1-0"

#define BUF_SIZE_64_BITS    8
#define BUF_SIZE_128_BITS   16
#define BUF_SIZE_160_BITS   20
#define BUF_SIZE_256_BITS   32
#define BUF_SIZE_512_BITS   64
#define BUF_SIZE_1024_BITS  128
#define BUF_SIZE_1536_BITS  192
#define NVRAM_BUFSIZE	100

#define PERSONALIZATION_STRING  "Wi-Fi Easy and Secure Key Derivation"
#define PRF_DIGEST_SIZE         BUF_SIZE_256_BITS
#define KDF_KEY_BITS            640

#define WPS_RESULT_SUCCESS_RESTART			100
#define WPS_RESULT_SUCCESS					101
#define WPS_RESULT_PROCESS_TIMEOUT			102
#define WPS_RESULT_USR_BREAK				103
#define WPS_RESULT_FAILURE					104
#define WPS_RESULT_ENROLLMENT_PINFAIL			105
#define WPS_RESULT_REGISTRATION_PINFAIL			106
#define WPS_RESULT_ENROLLMENT_M2DFAIL			107

typedef enum {
	SCMODE_UNKNOWN = 0,
	SCMODE_STA_ENROLLEE,
	SCMODE_STA_REGISTRAR,
	SCMODE_AP_ENROLLEE,
	SCMODE_AP_REGISTRAR
} WPS_SCMODE;

typedef enum {
	WPS_INIT = 0,
	WPS_ASSOCIATED,
	WPS_OK,
	WPS_MSG_ERR,
	WPS_TIMEOUT,
	WPS_SENDM2,
	WPS_SENDM7,
	WPS_MSGDONE,
	WPS_PBCOVERLAP,
	WPS_FIND_PBC_AP,
	WPS_ASSOCIATING,
	WPS_NFC_WR_CFG,
	WPS_NFC_WR_PW,
	WPS_NFC_WR_CPLT,
	WPS_NFC_RD_CFG,
	WPS_NFC_RD_PW,
	WPS_NFC_RD_CPLT,
	WPS_NFC_HO_S,
	WPS_NFC_HO_R,
	WPS_NFC_HO_NDEF,
	WPS_NFC_HO_CPLT,
	WPS_NFC_OP_ERROR,
	WPS_NFC_OP_STOP,
	WPS_NFC_OP_TO,
	WPS_NFC_FM,
	WPS_NFC_FM_CPLT
} WPS_SCSTATE;

typedef enum {
	TRANSPORT_TYPE_UFD = 1,
	TRANSPORT_TYPE_EAP,
	TRANSPORT_TYPE_WLAN,
	TRANSPORT_TYPE_NFC,
	TRANSPORT_TYPE_BT,
	TRANSPORT_TYPE_IP,
	TRANSPORT_TYPE_UPNP_CP,
	TRANSPORT_TYPE_UPNP_DEV,
	TRANSPORT_TYPE_MAX /* insert new transport types before TRANSPORT_TYPE_MAX */
} TRANSPORT_TYPE;

#define SM_FAILURE    0
#define SM_SUCCESS    1
#define SM_SET_PASSWD 2
#define WPS_WLAN_EVENT_TYPE_PROBE_REQ_FRAME	1
#define WPS_WLAN_EVENT_TYPE_EAP_FRAME		2

#define WPSM_WPSA_PORT		40000 + (1 << 6)
#define WPSAP_WPSM_PORT		40000 + (1 << 7)
#define WPS_LOOPBACK_ADDR		"127.0.0.1"

extern void RAND_bytes(unsigned char *buf, int num);
bool wps_getUpnpDevGetDeviceInfo(void *mc_dev);
void wps_setUpnpDevGetDeviceInfo(void *mc_dev, bool value);

#endif /* _WPS_COMMON_ */
