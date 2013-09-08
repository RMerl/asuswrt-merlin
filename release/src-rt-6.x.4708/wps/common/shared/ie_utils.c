/*
 * WPS IE share utility
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ie_utils.c 274160 2011-07-28 10:52:54Z $
 */

#include <wpserror.h>
#include <reg_prototlv.h>
#include <wpsheaders.h>
#include <tutrace.h>
#include <ie_utils.h>


/* For AP */
uint32
ie_utils_build_beacon_IE(void *params, uint8 *buf, int *buflen)
{
	uint32 ret = WPS_SUCCESS;
	CTlvPrimDeviceType tlvPrimDeviceType;
	CTlvVendorExt vendorExt;
	BufferObj *bufObj = NULL, *vendorExt_bufObj = NULL;
	IE_UTILS_BEACON_PARAMS *beacon = (IE_UTILS_BEACON_PARAMS *)params;


	TUTRACE((TUTRACE_INFO, "ie_utils_build_beacon_IE\n"));

	if (beacon == NULL || buf == NULL || *buflen == 0) {
		TUTRACE((TUTRACE_ERR, "ie_utils_build_beacon_IE: Invalid arguments\n"));
		return WPS_ERR_SYSTEM;
	}

	if ((bufObj = buffobj_new()) == NULL) {
		TUTRACE((TUTRACE_ERR, "ie_utils_build_beacon_IE: Out of memory\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	/* Version */
	tlv_serialize(WPS_ID_VERSION, bufObj, &beacon->version, WPS_ID_VERSION_S);

	/* Simple Config State */
	tlv_serialize(WPS_ID_SC_STATE, bufObj, &beacon->scState, WPS_ID_SC_STATE_S);

	/* AP Setup Locked - Optional */
	if (beacon->apLockdown)
		tlv_serialize(WPS_ID_AP_SETUP_LOCKED, bufObj, &beacon->apLockdown,
			WPS_ID_AP_SETUP_LOCKED_S);

	/* Selected Registrar - Optional */
	if (beacon->selReg) {
		tlv_serialize(WPS_ID_SEL_REGISTRAR, bufObj, &beacon->selReg,
			WPS_ID_SEL_REGISTRAR_S);
		tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &beacon->devPwdId,
			WPS_ID_DEVICE_PWD_ID_S);
		tlv_serialize(WPS_ID_SEL_REG_CFG_METHODS, bufObj, &beacon->selRegCfgMethods,
			WPS_ID_SEL_REG_CFG_METHODS_S);
	}

	/* UUID and RF_BAND for dualband */
	if (beacon->rfBand == (WPS_RFBAND_24GHZ | WPS_RFBAND_50GHZ)) {
		tlv_serialize(WPS_ID_UUID_E, bufObj, beacon->uuid_e, SIZE_UUID);
		tlv_serialize(WPS_ID_RF_BAND, bufObj, &beacon->rfBand, WPS_ID_RF_BAND_S);
	}

	/* WSC 2.0,  support WPS V2 or not */
	if (beacon->version2 >= WPS_VERSION2) {
		/* Prepare vendor extension buffer object */
		if ((vendorExt_bufObj = buffobj_new()) == NULL) {
			TUTRACE((TUTRACE_ERR, "ie_utils_build_beacon_IE: Out of memory\n"));
			ret = WPS_ERR_OUTOFMEMORY;
			goto error;
		}

		/* WSC 2.0 */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&beacon->version2, WPS_WFA_SUBID_VERSION2_S);

		/* AuthorizedMACs */
		if (beacon->authorizedMacs.len) {
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, vendorExt_bufObj,
				beacon->authorizedMacs.macs, beacon->authorizedMacs.len);
		}

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, bufObj);
	}

	/*
	 * Primary Device Type
	 * This is a complex TLV, so will be handled differently
	 */
	if (beacon->primDeviceCategory) {
		tlvPrimDeviceType.categoryId = beacon->primDeviceCategory;
		tlvPrimDeviceType.oui = beacon->primDeviceOui;
		tlvPrimDeviceType.subCategoryId = beacon->primDeviceSubCategory;
		tlv_primDeviceTypeWrite(&tlvPrimDeviceType, bufObj);
	}

	/* Device Name */
	if (strlen(beacon->deviceName)) {
		tlv_serialize(WPS_ID_DEVICE_NAME, bufObj, beacon->deviceName,
			strlen(beacon->deviceName));
	}

	/* Check copy back avaliable buf length */
	if (*buflen < buffobj_Length(bufObj)) {
		TUTRACE((TUTRACE_ERR, "ie_utils_build_beacon_IE: In buffer len too short\n"));
		ret = WPS_ERR_SYSTEM;
		goto error;
	}

	/* Copy back data and length */
	*buflen = buffobj_Length(bufObj);
	memcpy(buf, buffobj_GetBuf(bufObj), buffobj_Length(bufObj));

error:
	/* Free local vendorExt_bufObj */
	if (vendorExt_bufObj)
		buffobj_del(vendorExt_bufObj);

	/* Free local bufObj */
	if (bufObj)
		buffobj_del(bufObj);

	return ret;
}

uint32
ie_utils_build_proberesp_IE(void *params, uint8 *buf, int *buflen)
{
	uint32 ret = WPS_SUCCESS;
	CTlvPrimDeviceType tlvPrimDeviceType;
	CTlvVendorExt vendorExt;
	BufferObj *bufObj = NULL, *vendorExt_bufObj = NULL;
	IE_UTILS_PROBERESP_PARAMS *preboresp = (IE_UTILS_PROBERESP_PARAMS *)params;


	TUTRACE((TUTRACE_INFO, "ie_utils_build_proberesp_IE\n"));

	if (preboresp == NULL || buf == NULL || *buflen == 0) {
		TUTRACE((TUTRACE_ERR, "ie_utils_build_proberesp_IE: Invalid arguments\n"));
		return WPS_ERR_SYSTEM;
	}

	if ((bufObj = buffobj_new()) == NULL) {
		TUTRACE((TUTRACE_ERR, "ie_utils_build_proberesp_IE: Out of memory\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	/* Version */
	tlv_serialize(WPS_ID_VERSION, bufObj, &preboresp->version, WPS_ID_VERSION_S);

	/* Simple Config State */
	tlv_serialize(WPS_ID_SC_STATE, bufObj, &preboresp->scState, WPS_ID_SC_STATE_S);

	/* AP Setup Locked - optional if false */
	if (preboresp->apLockdown)
		tlv_serialize(WPS_ID_AP_SETUP_LOCKED, bufObj, &preboresp->apLockdown,
			WPS_ID_AP_SETUP_LOCKED_S);

	/* Selected Registrar - Optional */
	if (preboresp->selReg) {
		tlv_serialize(WPS_ID_SEL_REGISTRAR, bufObj, &preboresp->selReg,
			WPS_ID_SEL_REGISTRAR_S);
		tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &preboresp->devPwdId,
			WPS_ID_DEVICE_PWD_ID_S);
		tlv_serialize(WPS_ID_SEL_REG_CFG_METHODS, bufObj, &preboresp->selRegCfgMethods,
			WPS_ID_SEL_REG_CFG_METHODS_S);
	}

	/* Response Type */
	tlv_serialize(WPS_ID_RESP_TYPE, bufObj, &preboresp->respType, WPS_ID_RESP_TYPE_S);

	/* UUID-E */
	tlv_serialize(preboresp->respType == WPS_MSGTYPE_REGISTRAR ? WPS_ID_UUID_R : WPS_ID_UUID_E,
		bufObj, preboresp->uuid_e, SIZE_UUID);

	/* Manufacturer */
	tlv_serialize(WPS_ID_MANUFACTURER, bufObj, preboresp->manufacturer,
		strlen(preboresp->manufacturer));

	/* Model Name */
	tlv_serialize(WPS_ID_MODEL_NAME, bufObj, preboresp->modelName,
		strlen(preboresp->modelName));

	/* Model Number */
	tlv_serialize(WPS_ID_MODEL_NUMBER, bufObj, preboresp->modelNumber,
		strlen(preboresp->modelNumber));

	/* Serial Number */
	tlv_serialize(WPS_ID_SERIAL_NUM, bufObj, preboresp->serialNumber,
		strlen(preboresp->serialNumber));

	/* Primary Device Type */
	/* This is a complex TLV, so will be handled differently */
	tlvPrimDeviceType.categoryId = preboresp->primDeviceCategory;
	tlvPrimDeviceType.oui = preboresp->primDeviceOui;
	tlvPrimDeviceType.subCategoryId = preboresp->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&tlvPrimDeviceType, bufObj);

	/* Device Name */
	tlv_serialize(WPS_ID_DEVICE_NAME, bufObj, preboresp->deviceName,
		strlen(preboresp->deviceName));


	/* Config Methods */
	tlv_serialize(WPS_ID_CONFIG_METHODS, bufObj, &preboresp->configMethods,
		WPS_ID_CONFIG_METHODS_S);

	/* RF Bands - optional */
	tlv_serialize(WPS_ID_RF_BAND, bufObj, &preboresp->rfBand, WPS_ID_RF_BAND_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (preboresp->version2) {
		/* Prepare vendor extension buffer object */
		if ((vendorExt_bufObj = buffobj_new()) == NULL) {
			TUTRACE((TUTRACE_ERR, "ie_utils_build_proberesp_IE: Out of memory\n"));
			ret = WPS_ERR_OUTOFMEMORY;
			goto error;
		}

		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&preboresp->version2, WPS_WFA_SUBID_VERSION2_S);

		/* AuthorizedMACs */
		if (preboresp->authorizedMacs.len)
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, vendorExt_bufObj,
				preboresp->authorizedMacs.macs, preboresp->authorizedMacs.len);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, bufObj);
	}

	/* Check copy back avaliable buf length */
	if (*buflen < buffobj_Length(bufObj)) {
		TUTRACE((TUTRACE_ERR, "ie_utils_build_proberesp_IE: In buffer len too short\n"));
		ret = WPS_ERR_SYSTEM;
		goto error;
	}

	/* Copy back data and length */
	*buflen = buffobj_Length(bufObj);
	memcpy(buf, buffobj_GetBuf(bufObj), buffobj_Length(bufObj));

error:
	/* Free local vendorExt_bufObj */
	if (vendorExt_bufObj)
		buffobj_del(vendorExt_bufObj);

	/* Free local bufObj */
	if (bufObj)
		buffobj_del(bufObj);

	return ret;
}

uint32
ie_utils_build_assocresp_IE(void *params, uint8 *buf, int *buflen)
{
	uint32 ret = WPS_SUCCESS;
	CTlvVendorExt vendorExt;
	BufferObj *bufObj = NULL, *vendorExt_bufObj = NULL;
	IE_UTILS_ASSOCRESP_PARAMS *assocresp = (IE_UTILS_ASSOCRESP_PARAMS *)params;


	TUTRACE((TUTRACE_INFO, "ie_utils_build_assocresp_IE\n"));

	if (assocresp == NULL || buf == NULL || *buflen == 0) {
		TUTRACE((TUTRACE_ERR, "ie_utils_build_assocresp_IE: Invalid arguments\n"));
		return WPS_ERR_SYSTEM;
	}

	if ((bufObj = buffobj_new()) == NULL) {
		TUTRACE((TUTRACE_ERR, "ie_utils_build_assocresp_IE: Out of memory\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	if ((vendorExt_bufObj = buffobj_new()) == NULL) {
		buffobj_del(bufObj);
		TUTRACE((TUTRACE_ERR, "ie_utils_build_assocresp_IE: Out of memory\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	/* Version */
	tlv_serialize(WPS_ID_VERSION, bufObj, &assocresp->version, WPS_ID_VERSION_S);

	/* Response Type */
	tlv_serialize(WPS_ID_RESP_TYPE, bufObj, &assocresp->respType, WPS_ID_RESP_TYPE_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
		&assocresp->version2, WPS_WFA_SUBID_VERSION2_S);

	/* Serialize subelemetns to Vendor Extension */
	vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
	vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
	vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
	tlv_vendorExtWrite(&vendorExt, bufObj);

	/* Check copy back avaliable buf length */
	if (*buflen < buffobj_Length(bufObj)) {
		ret = WPS_ERR_SYSTEM;
		TUTRACE((TUTRACE_ERR, "ie_utils_build_assocresp_IE: In buffer len too short\n"));
		goto error;
	}

	/* Copy back data and length */
	*buflen = buffobj_Length(bufObj);
	memcpy(buf, buffobj_GetBuf(bufObj), buffobj_Length(bufObj));

error:
	/* Free local vendorExt_bufObj */
	if (vendorExt_bufObj)
		buffobj_del(vendorExt_bufObj);

	/* Free local bufObj */
	if (bufObj)
		buffobj_del(bufObj);

	return ret;
}

/* For STA */
uint32
ie_utils_build_probereq_IE(void *params, uint8 *buf, int *buflen)
{
	uint32 ret = WPS_SUCCESS;
	CTlvPrimDeviceType tlvPrimDeviceType;
	CTlvReqDeviceType tlvReqDeviceType;
	CTlvVendorExt vendorExt;
	BufferObj *bufObj = NULL, *vendorExt_bufObj = NULL;
	IE_UTILS_PROBEREQ_PARAMS *probereq = (IE_UTILS_PROBEREQ_PARAMS *)params;


	TUTRACE((TUTRACE_INFO, "ie_utils_build_probereq_IE\n"));

	if (probereq == NULL || buf == NULL || *buflen == 0) {
		TUTRACE((TUTRACE_ERR, "ie_utils_build_probereq_IE: Invalid arguments\n"));
		return WPS_ERR_SYSTEM;
	}

	if ((bufObj = buffobj_new()) == NULL) {
		TUTRACE((TUTRACE_ERR, "ie_utils_build_probereq_IE: Out of memory\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	/* Version */
	tlv_serialize(WPS_ID_VERSION, bufObj, &probereq->version, WPS_ID_VERSION_S);

	/* Request Type */
	tlv_serialize(WPS_ID_REQ_TYPE, bufObj, &probereq->reqType, WPS_ID_REQ_TYPE_S);

	/* Config Methods */
	tlv_serialize(WPS_ID_CONFIG_METHODS, bufObj, &probereq->configMethods,
		WPS_ID_CONFIG_METHODS_S);

	/* UUID-(E or R) */
	tlv_serialize(probereq->reqType == WPS_MSGTYPE_REGISTRAR ? WPS_ID_UUID_R : WPS_ID_UUID_E,
		bufObj, probereq->uuid, SIZE_UUID);

	/*
	 * Primary Device Type
	 * This is a complex TLV, so will be handled differently
	 */
	tlvPrimDeviceType.categoryId = probereq->primDeviceCategory;
	tlvPrimDeviceType.oui = probereq->primDeviceOui;
	tlvPrimDeviceType.subCategoryId = probereq->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&tlvPrimDeviceType, bufObj);

	/* RF Bands */
	tlv_serialize(WPS_ID_RF_BAND, bufObj, &probereq->rfBand, WPS_ID_RF_BAND_S);

	/* Association State */
	tlv_serialize(WPS_ID_ASSOC_STATE, bufObj, &probereq->assocState, WPS_ID_ASSOC_STATE_S);

	/* Configuration Error */
	tlv_serialize(WPS_ID_CONFIG_ERROR, bufObj, &probereq->configError, WPS_ID_CONFIG_ERROR_S);

	/* Device Password ID */
	tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &probereq->devPwdId, WPS_ID_DEVICE_PWD_ID_S);

	/* WSC 2.0 */
	if (probereq->version2 >= WPS_VERSION2) {
		/* Prepare vendor extension buffer object */
		if ((vendorExt_bufObj = buffobj_new()) == NULL) {
			ret = WPS_ERR_OUTOFMEMORY;
			TUTRACE((TUTRACE_ERR, "ie_utils_build_probereq_IE: Out of memory\n"));
			goto error;
		}

		/* Manufacturer */
		tlv_serialize(WPS_ID_MANUFACTURER, bufObj, probereq->manufacturer,
			strlen(probereq->manufacturer));

		/* Model Name */
		tlv_serialize(WPS_ID_MODEL_NAME, bufObj, probereq->modelName,
			strlen(probereq->modelName));

		/* Model Number */
		tlv_serialize(WPS_ID_MODEL_NUMBER, bufObj, probereq->modelNumber,
			strlen(probereq->modelNumber));

		/* Device Name */
		tlv_serialize(WPS_ID_DEVICE_NAME, bufObj, probereq->deviceName,
			strlen(probereq->deviceName));

		/* Add WFA Vendor Extension subelements */
		/* Version2 subelement */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj, &probereq->version2,
			WPS_WFA_SUBID_VERSION2_S);

		/* Request to Enroll subelement - optional */
		if (probereq->reqToEnroll)
			subtlv_serialize(WPS_WFA_SUBID_REQ_TO_ENROLL, vendorExt_bufObj,
				&probereq->reqToEnroll, WPS_WFA_SUBID_REQ_TO_ENROLL_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, bufObj);

		/*
		 * Request Device Type
		 * This is a complex TLV, so will be handled differently
		 */
		if (probereq->reqDeviceCategory) {
			tlvReqDeviceType.categoryId = probereq->reqDeviceCategory;
			tlvReqDeviceType.oui = probereq->reqDeviceOui;
			tlvReqDeviceType.subCategoryId = probereq->reqDeviceSubCategory;
			tlv_reqDeviceTypeWrite(&tlvReqDeviceType, bufObj);
		}
	}

	/* Check copy back avaliable buf length */
	if (*buflen < buffobj_Length(bufObj)) {
		ret = WPS_ERR_SYSTEM;
		TUTRACE((TUTRACE_ERR, "ie_utils_build_probereq_IE: In buffer len too short\n"));
		goto error;
	}

	/* Copy back data and length */
	*buflen = buffobj_Length(bufObj);
	memcpy(buf, buffobj_GetBuf(bufObj), buffobj_Length(bufObj));

error:
	/* Free local vendorExt_bufObj */
	if (vendorExt_bufObj)
		buffobj_del(vendorExt_bufObj);

	/* Free local bufObj */
	if (bufObj)
		buffobj_del(bufObj);

	return ret;
}

uint32
ie_utils_build_assocreq_IE(void *params, uint8 *buf, int *buflen)
{
	uint32 ret = WPS_SUCCESS;
	CTlvVendorExt vendorExt;
	BufferObj *bufObj = NULL, *vendorExt_bufObj = NULL;
	IE_UTILS_ASSOCREQ_PARAMS *assocreq = (IE_UTILS_ASSOCREQ_PARAMS *)params;


	TUTRACE((TUTRACE_INFO, "ie_utils_build_assocreq_IE\n"));

	if (assocreq == NULL || buf == NULL || *buflen == 0) {
		TUTRACE((TUTRACE_ERR, "ie_utils_build_probereq_IE: Invalid arguments\n"));
		return WPS_ERR_SYSTEM;
	}

	if ((bufObj = buffobj_new()) == NULL) {
		TUTRACE((TUTRACE_ERR, "ie_utils_build_assocreq_IE: Out of memory\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	if ((vendorExt_bufObj = buffobj_new()) == NULL) {
		buffobj_del(bufObj);
		TUTRACE((TUTRACE_ERR, "ie_utils_build_assocreq_IE: Out of memory\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	/* Version */
	tlv_serialize(WPS_ID_VERSION, bufObj, &assocreq->version, WPS_ID_VERSION_S);

	/* Request Type */
	tlv_serialize(WPS_ID_REQ_TYPE, bufObj, &assocreq->reqType, WPS_ID_REQ_TYPE_S);

	/* Add WFA Vendor Extension subelements */
	/* Version2 subelement */
	subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj, &assocreq->version2,
		WPS_WFA_SUBID_VERSION2_S);

	/* Serialize subelemetns to Vendor Extension */
	vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
	vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
	vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
	tlv_vendorExtWrite(&vendorExt, bufObj);

	/* Check copy back avaliable buf length */
	if (*buflen < buffobj_Length(bufObj)) {
		ret = WPS_ERR_SYSTEM;
		TUTRACE((TUTRACE_ERR, "ie_utils_build_assocreq_IE: In buffer len too short\n"));
		goto error;
	}

	/* Copy back data and length */
	*buflen = buffobj_Length(bufObj);
	memcpy(buf, buffobj_GetBuf(bufObj), buffobj_Length(bufObj));

error:
	/* Free local vendorExt_bufObj */
	if (vendorExt_bufObj)
		buffobj_del(vendorExt_bufObj);

	/* Free local bufObj */
	if (bufObj)
		buffobj_del(bufObj);

	return ret;
}
