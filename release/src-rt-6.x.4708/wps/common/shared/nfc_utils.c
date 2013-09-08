/*
 * WPS NFC share utility
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: $
 */
#include <wpsheaders.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <sminfo.h>
#include <reg_proto.h>
#include <reg_prototlv.h>
#include <tutrace.h>
#include <nfc_utils.h>

uint32
nfc_utils_build_cfg(DevInfo *info, uint8 *buf, uint *buflen)
{
	char *p_nwKey, *cp_data;
	uint8 *p_macAddr, wep_exist = 0, version = WPS_VERSION;
	uint16 data16;
	uint32 ret = WPS_SUCCESS, nwKeyLen = 0;
	BufferObj *bufObj = NULL, *vendorExt_bufObj = NULL;
	CTlvVendorExt vendorExt;
	CTlvCredential *p_tlvCred = NULL;

	TUTRACE((TUTRACE_NFC, "Build NFC Configuration\n"));

	if (info == NULL || buf == NULL || *buflen == 0) {
		TUTRACE((TUTRACE_NFC, "nfc_utils_build_cfg: Invalid arguments\n"));
		return WPS_ERR_SYSTEM;
	}

	if ((bufObj = buffobj_new()) == NULL) {
		TUTRACE((TUTRACE_NFC, "nfc_utils_build_cfg: Out of memory\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	/* Version */
	tlv_serialize(WPS_ID_VERSION, bufObj, &version, WPS_ID_VERSION_S);

	/* Credential */
	if ((p_tlvCred = (CTlvCredential *)malloc(sizeof(CTlvCredential))) == NULL) {
		TUTRACE((TUTRACE_NFC, "nfc_utils_build_cfg: Out of memory\n"));
		ret = WPS_ERR_OUTOFMEMORY;
		goto error;
	}
	memset(p_tlvCred, 0, sizeof(CTlvCredential));

	/* Credential items */
	/* nwIndex */
	tlv_set(&p_tlvCred->nwIndex, WPS_ID_NW_INDEX, (void *)1, 0);

	/* ssid */
	cp_data = info->ssid;
	data16 = strlen(cp_data);
	tlv_set(&p_tlvCred->ssid, WPS_ID_SSID, cp_data, data16);

	/* auth */
	if (info->auth)
		data16 = WPS_AUTHTYPE_SHARED;
	else if (devinfo_getKeyMgmtType(info) == WPS_WL_AKM_PSK)
		data16 = WPS_AUTHTYPE_WPAPSK;
	else if (devinfo_getKeyMgmtType(info) == WPS_WL_AKM_PSK2)
		data16 = WPS_AUTHTYPE_WPA2PSK;
	else if (devinfo_getKeyMgmtType(info) == WPS_WL_AKM_BOTH)
		data16 = WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK;
	else
		data16 = WPS_AUTHTYPE_OPEN;
	tlv_set(&p_tlvCred->authType, WPS_ID_AUTH_TYPE, UINT2PTR(data16), 0);

	/* encrType */
	if (info->auth)
		data16 = WPS_ENCRTYPE_WEP;
	else if (data16 == WPS_AUTHTYPE_OPEN) {
		if (info->wep)
			data16 = WPS_ENCRTYPE_WEP;
		else
			data16 = WPS_ENCRTYPE_NONE;
	}
	else
		data16 = info->crypto;
	tlv_set(&p_tlvCred->encrType, WPS_ID_ENCR_TYPE, UINT2PTR(data16), 0);

	if (data16 == WPS_ENCRTYPE_WEP)
		wep_exist = 1;

	/* nwKeyIndex
	 * WSC 2.0, "Network Key Index" deprecated - only included by WSC 1.0 devices.
	 * Ignored by WSC 2.0 or newer devices.
	 */
	if (info->version2 < WPS_VERSION2) {
		tlv_set(&p_tlvCred->nwKeyIndex, WPS_ID_NW_KEY_INDEX, (void *)1, 0);
	}

	/* nwKey */
	p_nwKey = info->nwKey;
	nwKeyLen = strlen(info->nwKey);
	tlv_set(&p_tlvCred->nwKey, WPS_ID_NW_KEY, p_nwKey, nwKeyLen);

	/* enrollee's mac */
	p_macAddr = info->peerMacAddr;
	data16 = SIZE_MAC_ADDR;
	tlv_set(&p_tlvCred->macAddr, WPS_ID_MAC_ADDR, p_macAddr, data16);

	/* WepKeyIdx */
	if (wep_exist)
		tlv_set(&p_tlvCred->WEPKeyIndex, WPS_ID_WEP_TRANSMIT_KEY,
			UINT2PTR(info->wepKeyIdx), 0);

	/* WSC 2.0, WFA "Network Key Shareable" subelement */
	if (info->version2 >= WPS_VERSION2) {
		/* Always shareable */
		data16 = 1;
		subtlv_set(&p_tlvCred->nwKeyShareable, WPS_WFA_SUBID_NW_KEY_SHAREABLE,
			UINT2PTR(data16), 0);
	}
	tlv_credentialWrite(p_tlvCred, bufObj);

	/* Version2 */
	if (info->version2 >= WPS_VERSION2) {
		if ((vendorExt_bufObj = buffobj_new()) == NULL) {
			TUTRACE((TUTRACE_NFC, "nfc_utils_build_cfg: Out of memory\n"));
			ret = WPS_ERR_OUTOFMEMORY;
			goto error;
		}

		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&info->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, bufObj);
	}

	/* Check copy back avaliable buf length */
	if (*buflen < buffobj_Length(bufObj)) {
		TUTRACE((TUTRACE_NFC, "nfc_utils_build_cfg: In buffer len too short\n"));
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

	/* Free local p_tlvCred */
	if (p_tlvCred)
		tlv_credentialDelete(p_tlvCred, 0);

	/* Free local bufObj */
	if (bufObj)
		buffobj_del(bufObj);

	return ret;
}

uint32
nfc_utils_build_pw(DevInfo *info, uint8 *buf, uint *buflen)
{
	uint8 version = WPS_VERSION;
	uint32 ret = WPS_SUCCESS;
	BufferObj *bufObj = NULL, *vendorExt_bufObj = NULL;
	CTlvVendorExt vendorExt;
	CTlvOobDevPwd oobDevPwd;
	unsigned char hex_pin[SIZE_32_BYTES];
	int hex_pin_len;

	TUTRACE((TUTRACE_NFC, "Build NFC Password\n"));

	if (info == NULL || buf == NULL || *buflen == 0) {
		TUTRACE((TUTRACE_NFC, "nfc_utils_build_pw: Invalid arguments\n"));
		return WPS_ERR_SYSTEM;
	}

	if ((bufObj = buffobj_new()) == NULL) {
		TUTRACE((TUTRACE_NFC, "nfc_utils_build_pw: Out of memory\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	/* Version */
	tlv_serialize(WPS_ID_VERSION, bufObj, &version, WPS_ID_VERSION_S);

	/* OOB Device Password */
	oobDevPwd.publicKeyHash = info->pub_key_hash;
	oobDevPwd.pwdId = info->devPwdId;

	/* Do String to Hex translation */
	hex_pin_len = wps_str2hex(hex_pin, sizeof(hex_pin), info->pin);
	if (hex_pin_len == 0) {
		TUTRACE((TUTRACE_NFC, "nfc_utils_build_pw: invalid parameters\n"));
		ret = WPS_ERR_INVALID_PARAMETERS;
		goto error;
	}

	oobDevPwd.ip_devPwd = hex_pin;
	oobDevPwd.devPwdLength = hex_pin_len;
	tlv_oobDevPwdWrite(&oobDevPwd, bufObj);

	/* Version2 */
	if (info->version2 >= WPS_VERSION2) {
		if ((vendorExt_bufObj = buffobj_new()) == NULL) {
			TUTRACE((TUTRACE_NFC, "nfc_utils_build_pw: Out of memory\n"));
			ret = WPS_ERR_OUTOFMEMORY;
			goto error;
		}

		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&info->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, bufObj);
	}

	/* Check copy back avaliable buf length */
	if (*buflen < buffobj_Length(bufObj)) {
		TUTRACE((TUTRACE_NFC, "nfc_utils_build_cfg: In buffer len too short\n"));
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
nfc_utils_parse_cfg(WpsEnrCred *cred, uint8 wsp_version2, uint8 *buf, uint buflen)
{
	uint32 ret = WPS_SUCCESS;
	char *cp_data;
	uint16 data16;
	BufferObj *bufObj = NULL;
	BufferObj *vendorExt_bufObj = NULL;
	CTlvVersion version;
	CSubTlvVersion2 version2;
	CTlvVendorExt vendorExt;
	CTlvCredential *p_tlvCred = NULL;

	/* Sanity check */
	if (cred == NULL || buf == NULL || buflen == 0) {
		TUTRACE((TUTRACE_NFC, "Pase WPS CFG: Invalid parameters!\n"));
		ret = WPS_ERR_INVALID_PARAMETERS;
		goto error;
	}

	if (buflen < 7) {
		TUTRACE((TUTRACE_NFC, "Pase WPS CFG: buf length too short!\n"));
		ret = WPS_ERR_INVALID_PARAMETERS;
		goto error;
	}

	if (buf[5] != 0x10 || buf[6] != 0x0E) {
		TUTRACE((TUTRACE_NFC, "Pase WPS CFG: Not a Configuration Token!\n"));
		ret = WPS_ERR_INVALID_PARAMETERS;
		goto error;
	}

	/* Dserial raw data to bufObj */
	bufObj = buffobj_new();
	if (bufObj == NULL) {
		TUTRACE((TUTRACE_NFC, "Pase WPS CFG: Malloc fail!\n"));
		ret = WPS_ERR_OUTOFMEMORY;
		goto error;
	}

	WPS_HexDumpAscii("NFC_UTILS_PARSE_CFG", -1, buf, buflen);

	buffobj_dserial(bufObj, buf, buflen);

	/* Version 1 */
	if (tlv_dserialize(&version, WPS_ID_VERSION, bufObj, 0, 0) != 0) {
		TUTRACE((TUTRACE_NFC, "Pase WPS CFG: TLV missing\n"));
		ret = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Must be 0x10, See WSC2.0 Clause 7.9 Version Negotiation */
	if (version.m_data != WPS_VERSION) {
		TUTRACE((TUTRACE_NFC, "Pase WPS CFG: Message version should always set to 1\n"));
		ret = RPROT_ERR_INCOMPATIBLE;
		goto error;
	}

	/* Configuration */
	/* Parse Credential */
	p_tlvCred = (CTlvCredential *)malloc(sizeof(CTlvCredential));
	if (!p_tlvCred) {
		TUTRACE((TUTRACE_NFC, "Pase WPS CFG: Malloc fail!\n"));
		ret = WPS_ERR_OUTOFMEMORY;
		goto error;
	}
	memset(p_tlvCred, 0, sizeof(CTlvCredential));
	tlv_credentialParse(p_tlvCred, bufObj, true);

	/* Save CTlvCredential to WpsEnrCred */
	/* Fill in SSID */
	cp_data = (char *)(p_tlvCred->ssid.m_data);
	data16 = cred->ssidLen = p_tlvCred->ssid.tlvbase.m_len;
	strncpy(cred->ssid, cp_data, data16);
	cred->ssid[data16] = '\0';

	/* Fill in keyMgmt */
	if (p_tlvCred->authType.m_data == WPS_AUTHTYPE_SHARED) {
		strncpy(cred->keyMgmt, "SHARED", 6);
		cred->keyMgmt[6] = '\0';
	}
	else  if (p_tlvCred->authType.m_data == WPS_AUTHTYPE_WPAPSK) {
		strncpy(cred->keyMgmt, "WPA-PSK", 7);
		cred->keyMgmt[7] = '\0';
	}
	else if (p_tlvCred->authType.m_data == WPS_AUTHTYPE_WPA2PSK) {
		strncpy(cred->keyMgmt, "WPA2-PSK", 8);
		cred->keyMgmt[8] = '\0';
	}
	else if (p_tlvCred->authType.m_data == (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)) {
		strncpy(cred->keyMgmt, "WPA-PSK WPA2-PSK", 16);
		cred->keyMgmt[16] = '\0';
	}
	else {
		strncpy(cred->keyMgmt, "OPEN", 4);
		cred->keyMgmt[4] = '\0';
	}

	/* get the real cypher */
	cred->encrType = p_tlvCred->encrType.m_data;

	/* Fill in WEP index, no matter it's WEP type or not */
	cred->wepIndex = p_tlvCred->WEPKeyIndex.m_data;

	/* Fill in PSK */
	data16 = p_tlvCred->nwKey.tlvbase.m_len;
	memset(cred->nwKey, 0, SIZE_64_BYTES);
	memcpy(cred->nwKey, p_tlvCred->nwKey.m_data, data16);
	/* we have to set key length here */
	cred->nwKeyLen = data16;

	/* WSC 2.0,  nwKeyShareable */
	cred->nwKeyShareable = p_tlvCred->nwKeyShareable.m_data;

	/* Version2, here you don't is peer WSC 2.0 or not */
	if (wsp_version2 >= WPS_VERSION2) {
		if (tlv_find_vendorExtParse(&vendorExt, bufObj, (uint8 *)WFA_VENDOR_EXT_ID) != 0) {
			TUTRACE((TUTRACE_NFC, "Pase WPS CFG: Cannot find vendor extension\n"));
			/* ret = RPROT_ERR_INCOMPATIBLE; */
			goto error;
		}

		/* Deserialize subelement */
		if ((vendorExt_bufObj = buffobj_new()) == NULL) {
			TUTRACE((TUTRACE_NFC, "Pase WPS CFG: Fail to allocate "
				"vendor extension buffer, Out of memory\n"));

			ret = WPS_ERR_OUTOFMEMORY;
			goto error;
		}

		buffobj_dserial(vendorExt_bufObj, vendorExt.vendorData,
			vendorExt.dataLength);

		/* Get Version2 subelement */
		if ((buffobj_NextSubId(vendorExt_bufObj) != WPS_WFA_SUBID_VERSION2) ||
		    (subtlv_dserialize(&version2, WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
		     0, 0) != 0)) {
			TUTRACE((TUTRACE_NFC, "Pase WPS CFG: Cannot get Version2\n"));
			/* ret = RPROT_ERR_INCOMPATIBLE; */
			goto error;
		}

		if (version2.m_data < WPS_VERSION2) {
			TUTRACE((TUTRACE_NFC, "Pase WPS NDEF: Invalid Version2 number\n"));
			ret = RPROT_ERR_INCOMPATIBLE;
			goto error;
		}
	}

error:
	/* free local alloc pointers */
	if (vendorExt_bufObj)
		buffobj_del(vendorExt_bufObj);

	if (p_tlvCred)
		tlv_credentialDelete(p_tlvCred, 0);

	if (bufObj)
		buffobj_del(bufObj);

	return ret;
}

uint32
nfc_utils_parse_pw(WpsOobDevPw *devpw, uint8 wsp_version2, uint8 *buf, uint buflen)
{
	uint32 ret = WPS_SUCCESS;
	char *cp_data;
	uint16 data16;
	BufferObj *bufObj = NULL;
	BufferObj *vendorExt_bufObj = NULL;
	CTlvVersion version;
	CSubTlvVersion2 version2;
	CTlvVendorExt vendorExt;
	CTlvOobDevPwd *p_oobDevPwd = NULL;

	/* Sanity check */
	if (devpw == NULL || buf == NULL || buflen == 0) {
		TUTRACE((TUTRACE_NFC, "Pase WPS PW: Invalid parameters!\n"));
		ret = WPS_ERR_INVALID_PARAMETERS;
		goto error;
	}

	if (buflen < 7) {
		TUTRACE((TUTRACE_NFC, "Pase WPS PW: buf length too short!\n"));
		ret = WPS_ERR_INVALID_PARAMETERS;
		goto error;
	}

	if (buf[5] != 0x10 || buf[6] != 0x2C) {
		TUTRACE((TUTRACE_NFC, "Pase WPS PW: Not a Password Token!\n"));
		ret = WPS_ERR_INVALID_PARAMETERS;
		goto error;
	}

	/* Dserial raw data to bufObj */
	bufObj = buffobj_new();
	if (bufObj == NULL) {
		TUTRACE((TUTRACE_NFC, "Pase WPS PW: Malloc fail!\n"));
		ret = WPS_ERR_OUTOFMEMORY;
		goto error;
	}

	WPS_HexDumpAscii("NFC_UTILS_PARSE_PW", -1, buf, buflen);

	buffobj_dserial(bufObj, buf, buflen);

	/* Version 1 */
	if (tlv_dserialize(&version, WPS_ID_VERSION, bufObj, 0, 0) != 0) {
		TUTRACE((TUTRACE_NFC, "Pase WPS PW: TLV missing\n"));
		ret = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Must be 0x10, See WSC2.0 Clause 7.9 Version Negotiation */
	if (version.m_data != WPS_VERSION) {
		TUTRACE((TUTRACE_NFC, "Pase WPS PW: Message version should always set to 1\n"));
		ret = RPROT_ERR_INCOMPATIBLE;
		goto error;
	}

	/* OOB Device Password */
	p_oobDevPwd = (CTlvOobDevPwd *)malloc(sizeof(CTlvOobDevPwd));
	if (!p_oobDevPwd) {
		TUTRACE((TUTRACE_NFC, "Pase WPS PW: Malloc fail!\n"));
		ret = WPS_ERR_OUTOFMEMORY;
		goto error;
	}
	memset(p_oobDevPwd, 0, sizeof(CTlvOobDevPwd));
	if (tlv_oobDevPwdParse(p_oobDevPwd, bufObj) != 0) {
		ret = WPS_ERR_SYSTEM;
		goto error;
	}

	/* Save CTlvOobDevPwd to WpsOobDevPw */
	memcpy(devpw->pub_key_hash, p_oobDevPwd->publicKeyHash, SIZE_160_BITS);
	devpw->devPwdId = p_oobDevPwd->pwdId;
	memcpy(devpw->pin, p_oobDevPwd->ip_devPwd, p_oobDevPwd->devPwdLength);
	devpw->pin_len = p_oobDevPwd->devPwdLength;

	/* Version2, here you don't is peer WSC 2.0 or not */
	if (wsp_version2 >= WPS_VERSION2) {
		if (tlv_find_vendorExtParse(&vendorExt, bufObj, (uint8 *)WFA_VENDOR_EXT_ID) != 0) {
			TUTRACE((TUTRACE_NFC, "Pase WPS PW: Cannot find vendor extension\n"));
			/* ret = RPROT_ERR_INCOMPATIBLE; */
			goto error;
		}

		/* Deserialize subelement */
		if ((vendorExt_bufObj = buffobj_new()) == NULL) {
			TUTRACE((TUTRACE_NFC, "Pase WPS PW: Fail to allocate "
				"vendor extension buffer, Out of memory\n"));

			ret = WPS_ERR_OUTOFMEMORY;
			goto error;
		}

		buffobj_dserial(vendorExt_bufObj, vendorExt.vendorData,
			vendorExt.dataLength);

		/* Get Version2 subelement */
		if ((buffobj_NextSubId(vendorExt_bufObj) != WPS_WFA_SUBID_VERSION2) ||
		    (subtlv_dserialize(&version2, WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
		     0, 0) != 0)) {
			TUTRACE((TUTRACE_NFC, "Pase WPS PW: Cannot get Version2\n"));
			/* ret = RPROT_ERR_INCOMPATIBLE; */
			goto error;
		}

		if (version2.m_data < WPS_VERSION2) {
			TUTRACE((TUTRACE_NFC, "Pase WPS PW: Invalid Version2 number\n"));
			ret = RPROT_ERR_INCOMPATIBLE;
			goto error;
		}
	}

error:
	/* free local alloc pointers */
	if (vendorExt_bufObj)
		buffobj_del(vendorExt_bufObj);

	if (p_oobDevPwd)
		free(p_oobDevPwd);

	if (bufObj)
		buffobj_del(bufObj);

	return ret;
}
