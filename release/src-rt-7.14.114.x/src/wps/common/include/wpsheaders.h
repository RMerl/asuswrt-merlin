/*
 * WPS header
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpsheaders.h 290904 2011-10-20 15:18:57Z $
 */

#ifndef _WPS_HEADERS_
#define _WPS_HEADERS_

#include <wpstypes.h>

#include <wpstlvbase.h>

/* Include the following until we figure out where to put the beacons */
#include <reg_prototlv.h>

/* WSC 2.0, deprecated and set to hardcode 0x10 for backwords compatibility resons */
#define WPS_VERSION                0x10	/* Do not change it anymore */

/* WSC 2.0 */
#define WPS_VERSION2               0x20
#define WPS_SETTING_DELAY_TIME_ROUTER	10 /* seconds for embedded router */
#define WPS_SETTING_DELAY_TIME_LINUX	10 /* seconds for Host Linux STA */

/* Beacon Info */
typedef struct {
	CTlvVersion version;
	CTlvScState scState;
	CTlvAPSetupLocked apSetupLocked;
	CTlvSelRegistrar selRegistrar;
	CTlvDevicePwdId pwdId;
	CTlvSelRegCfgMethods selRegConfigMethods;
	CTlvUuid uuid;
	CTlvRfBand rfBand;

	CTlvVendorExt vendorExt;
	CSubTlvVersion2 version2; /* C: WSC 2.0 */
	CSubTlvAuthorizedMACs authorizedMACs; /* C: WSC 2.0 */
} WpsBeaconIE;

/* Probe Request Info */
typedef struct {
	CTlvVersion version;
	CTlvReqType reqType;
	CTlvConfigMethods confMethods;
	CTlvUuid uuid;
	CTlvPrimDeviceType primDevType;
	CTlvRfBand rfBand;
	CTlvAssocState assocState;
	CTlvConfigError confErr;
	CTlvDevicePwdId pwdId;
	CTlvManufacturer manufacturer; /* C: WSC 2.0 */
	CTlvModelName modelName; /* C: WSC 2.0 */
	CTlvModelNumber modelNumber; /* C: WSC 2.0 */
	CTlvDeviceName deviceName; /* C: WSC 2.0 */
	CTlvReqDeviceType reqDevType; /* O: WSC 2.0 */
	CTlvPortableDevice portableDevice;

	CTlvVendorExt vendorExt;
	CSubTlvVersion2 version2; /* C: WSC 2.0 */
	CSubTlvReqToEnr reqToEnr; /* O: WSC 2.0 */
} WpsProbreqIE;

/* Probe Response Info */
typedef struct {
	CTlvVersion version;
	CTlvScState scState;
	CTlvAPSetupLocked apSetupLocked;
	CTlvSelRegistrar selRegistrar;
	CTlvDevicePwdId pwdId;
	CTlvSelRegCfgMethods selRegConfigMethods;
	CTlvRespType respType;
	CTlvUuid uuid;
	CTlvManufacturer manuf;
	CTlvModelName modelName;
	CTlvModelNumber modelNumber;
	CTlvSerialNum serialNumber;
	CTlvPrimDeviceType primDevType;
	CTlvDeviceName devName;
	CTlvConfigMethods confMethods;
	CTlvRfBand rfBand;

	CTlvVendorExt vendorExt;
	CSubTlvVersion2 version2; /* C: WSC 2.0 */
	CSubTlvAuthorizedMACs authorizedMACs; /* C: WSC 2.0 */
} WpsProbrspIE;


#endif /* _WPS_HEADERS_ */
