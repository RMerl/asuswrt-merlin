/*
 * WPS device infomation
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_devinfo.h 399455 2013-04-30 07:07:27Z $
 */

#ifndef __WPS_DEVICE_INFO_H__
#define __WPS_DEVICE_INFO_H__

#include <wps_dh.h>

/* data structure to hold Enrollee and Registrar information */
typedef struct {
	uint8   version;
	uint8   uuid[SIZE_16_BYTES];
	uint8   macAddr[SIZE_6_BYTES];
	char    deviceName[SIZE_32_BYTES+1];
	uint16  primDeviceCategory;
	uint32  primDeviceOui;
	uint16  primDeviceSubCategory;
	uint16  authTypeFlags;
	uint16  encrTypeFlags;
	uint8   connTypeFlags;
	uint16  configMethods;
	uint8   scState;
	bool    selRegistrar;
	char    manufacturer[SIZE_64_BYTES+1];
	char    modelName[SIZE_32_BYTES+1];
	char    modelNumber[SIZE_32_BYTES+1];
	char    serialNumber[SIZE_32_BYTES+1];
	uint8   rfBand;
	uint32  osVersion;
	uint32  featureId;
	uint16  assocState;
	uint16  devPwdId;
	uint16  configError;
	bool    b_ap;
	char    ssid[SIZE_SSID_LENGTH];
	char    keyMgmt[SIZE_20_BYTES+1];
	char    nwKey[SIZE_64_BYTES+1];
	uint16  auth;
	uint16  wep;
	uint16  wepKeyIdx;
	uint16  crypto;
	uint16  reqDeviceCategory;
	uint32  reqDeviceOui;
	uint16  reqDeviceSubCategory;
	uint8   version2;
	uint8   settingsDelayTime;
	bool    b_reqToEnroll;
	bool    b_nwKeyShareable;
#ifdef WFA_WPS_20_TESTBED
	char    dummy_ssid[SIZE_SSID_LENGTH];
	bool    b_zpadding;
	bool    b_mca;
	int     nattr_len;
	char    nattr_tlv[SIZE_128_BYTES];
#endif /* WFA_WPS_20_TESTBED */
	int     authorizedMacs_len;
	char    authorizedMacs[SIZE_MAC_ADDR * SIZE_AUTHORIZEDMACS_NUM];

	uint8   transport_uuid[SIZE_16_BYTES]; /* Used for WCN-NET VP */

	char    *pairing_auth_str;

	/* Run time data */
	int     sc_mode;
	bool    configap;
	char    pin[SIZE_64_BYTES+1]; /* String format */
	int     flags;
	DH      *DHSecret;
	uint8   pre_nonce[SIZE_128_BITS];
	uint8   pre_privkey[SIZE_PUB_KEY];
	uint8   peerMacAddr[SIZE_6_BYTES];
	uint8   pbc_uuids[SIZE_16_BYTES * 2];
	uint8   pub_key_hash[SIZE_160_BITS];

	void    *mp_tlvEsM7Ap;
	void    *mp_tlvEsM7Sta;
	void    *mp_tlvEsM8Ap;
	void    *mp_tlvEsM8Sta;
} DevInfo;

typedef enum {
	WPS_WL_AKM_NONE = 0,
	WPS_WL_AKM_PSK,
	WPS_WL_AKM_PSK2,
	WPS_WL_AKM_BOTH
} WPS_WL_AKM_E;

#define DEVINFO_FLAG_PRE_NONCE		0x1
#define DEVINFO_FLAG_PRE_PRIV_KEY	0x2

/* Functions */
DevInfo *devinfo_new();
void devinfo_delete(DevInfo *dev_inf);

uint16 devinfo_getKeyMgmtType(DevInfo *inf);

#endif /* __WPS_DEVICE_INFO_H__ */
