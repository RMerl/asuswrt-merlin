/*
 * WPS IE share utility header file
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ie_utils.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _WPS_IE_UTILS_H_
#define _WPS_IE_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <typedefs.h>
#include <wpstypes.h>

#define IE_UTILS_BUILD_WPS_IE_FUNC_NUM	5

/* AuthorizedMacs octet structure */
typedef struct {
	int len;
	char macs[SIZE_MAC_ADDR * SIZE_AUTHORIZEDMACS_NUM];
} IE_UTILS_AUTHOMACS_OCTET;

/* Beacon parameters WPS IE structure */
typedef struct {
	uint8 version;
	uint8 scState;
	uint8 apLockdown;
	uint8 selReg;
	uint16 devPwdId;
	uint16 selRegCfgMethods;
	uint8 uuid_e[SIZE_UUID];
	uint8 rfBand;

	uint16 primDeviceCategory;
	uint32 primDeviceOui;
	uint16 primDeviceSubCategory;

	char deviceName[SIZE_32_BYTES + 1];

	/* WSC 2.0 */
	uint8 version2;
	IE_UTILS_AUTHOMACS_OCTET authorizedMacs;
} IE_UTILS_BEACON_PARAMS;

/* Probe Response parameters WPS IE structure */
typedef struct {
	uint8 version;
	uint8 scState;
	uint8 apLockdown;
	uint8 selReg;
	uint16 devPwdId;
	uint16 selRegCfgMethods;
	uint8 respType;
	uint8 uuid_e[SIZE_UUID];
	char manufacturer[SIZE_64_BYTES + 1];
	char modelName[SIZE_32_BYTES + 1];
	char modelNumber[SIZE_32_BYTES + 1];
	char serialNumber[SIZE_32_BYTES + 1];
	uint16 primDeviceCategory;
	uint32 primDeviceOui;
	uint16 primDeviceSubCategory;
	char deviceName[SIZE_32_BYTES + 1];
	uint16 configMethods;
	uint8 rfBand;

	/* WSC 2.0 */
	uint8 version2;
	IE_UTILS_AUTHOMACS_OCTET authorizedMacs;
} IE_UTILS_PROBERESP_PARAMS;

/* Associate Response parameters WPS IE structure */
typedef struct {
	uint8 version;
	uint8 respType;

	/* WSC 2.0 */
	uint8 version2;
} IE_UTILS_ASSOCRESP_PARAMS;

/* Probe Request parameters WPS IE structure */
typedef struct {
	uint8 version;
	uint8 reqType;
	uint16 configMethods;
	uint8 uuid[SIZE_UUID];
	uint16 primDeviceCategory;
	uint32 primDeviceOui;
	uint16 primDeviceSubCategory;
	uint8 rfBand;
	uint16 assocState;
	uint16 configError;
	uint16 devPwdId;

	uint16 reqDeviceCategory;
	uint32 reqDeviceOui;
	uint16 reqDeviceSubCategory;

	/* WSC 2.0 */
	uint8 version2;
	char manufacturer[SIZE_64_BYTES + 1];
	char modelName[SIZE_32_BYTES + 1];
	char modelNumber[SIZE_32_BYTES + 1];
	char deviceName[SIZE_32_BYTES + 1];
	uint8 reqToEnroll;
} IE_UTILS_PROBEREQ_PARAMS;

/* Associate Request WPS IE parameters structure */
typedef struct {
	uint8 version;
	uint8 reqType;

	/* WSC 2.0 */
	uint8 version2;
} IE_UTILS_ASSOCREQ_PARAMS;

/*
 * implemented in ie_utils.c
 */
uint32 ie_utils_build_beacon_IE(void *params, uint8 *buf, int *buflen);
uint32 ie_utils_build_proberesp_IE(void *params, uint8 *buf, int *buflen);
uint32 ie_utils_build_assocresp_IE(void *params, uint8 *buf, int *buflen);

uint32 ie_utils_build_probereq_IE(void *params, uint8 *buf, int *buflen);
uint32 ie_utils_build_assocreq_IE(void *params, uint8 *buf, int *buflen);

#ifdef __cplusplus
}
#endif

#endif /* _WPS_IE_UTILS_H_ */
