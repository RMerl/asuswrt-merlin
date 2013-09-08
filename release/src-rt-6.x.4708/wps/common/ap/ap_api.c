/*
 * WPS AP API
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ap_api.c 395438 2013-04-08 02:01:22Z $
 */

#if !defined(__linux__) && !defined(__ECOS) && !defined(TARGETOS_nucleus) && \
	!defined(_MACOSX_)&& !defined(TARGETOS_symbian)
#include <stdio.h>
#include <windows.h>
#endif

#if defined(__linux__) || defined(__ECOS)
#include <string.h>
#include <ctype.h>
#endif

#include <bn.h>
#include <wps_dh.h>

#include <portability.h>
#include <wpsheaders.h>
#include <wpscommon.h>
#include <sminfo.h>
#include <wpserror.h>
#include <reg_protomsg.h>
#include <reg_proto.h>
#include <statemachine.h>
#include <tutrace.h>

#include <wps_wl.h>
#include <wps_apapi.h>
#include <wpsapi.h>
#include <ap_eap_sm.h>
#ifdef WPS_UPNP_DEVICE
#include <ap_upnp_sm.h>
#endif
#include <ap_ssr.h>

#ifdef __ECOS
#include <sys/socket.h>
#endif


/* under m_xxx in mbuf.h */

int wps_setWpsIE(void *bcmdev, uint8 scState, uint8 selRegistrar,
	uint16 devPwdId, uint16 selRegCfgMethods, uint8 *authorizedMacs_buf,
	int authorizedMacs_len);
#ifndef WPS_ROUTER
static uint32 wps_setBeaconIE(WPSAPI_T *g_mc, bool b_configured,
	bool b_selRegistrar, uint16 devPwdId, uint16 selRegCfgMethods,
	uint8 *authorizedMacs_buf, int authorizedMacs_len);
static uint32 wps_setProbeRespIE(WPSAPI_T *g_mc, uint8 respType, uint8 scState,
	bool b_selRegistrar, uint16 devPwdId, uint16 selRegCfgMethods,
	uint8 *authorizedMacs_buf, int authorizedMacs_len);
#endif /* !WPS_ROUTER */
static uint32 wpsap_createM7AP(WPSAPI_T *g_mc);
static uint32 wpsap_createM8Sta(WPSAPI_T *g_mc);
static uint32 wps_sendMsg(void *mcdev, TRANSPORT_TYPE trType, char * dataBuffer, uint32 dataLen);

char* wps_msg_type_str(int msgType)
{
	switch (msgType) {
	case 0x4:
		return "M1";
	case 0x5:
		return "M2";
	case 0x6:
		return "M2D";
	case 0x7:
		return "M3";
	case 0x8:
		return "M4";
	case 0x9:
		return "M5";
	case 0xa:
		return "M6";
	case 0xb:
		return "M7";
	case 0xc:
		return "M8";
	case 0xd:
		return "ACK";
	case 0xe:
		return "NACK";
	case 0xf:
		return "DONE";
	default:
		return "UNKNOWN";
	}
}

void *
wps_init(void *bcmwps, DevInfo *ap_devinfo)
{
	WPSAPI_T *gp_mc;
	DevInfo *dev_info;

	gp_mc = (WPSAPI_T *)alloc_init(sizeof(*gp_mc));
	if (!gp_mc) {
		TUTRACE((TUTRACE_INFO, "wps_init::malloc failed!\n"));
		return 0;
	}

	gp_mc->dev_info = devinfo_new();
	if (gp_mc->dev_info == NULL)
		goto error;

	/* copy user provided DevInfo to mp_deviceInfo */
	dev_info = gp_mc->dev_info;
	memcpy(dev_info, ap_devinfo, sizeof(DevInfo));

	/* copy prebuild enrollee noce and private key */
	if (dev_info->flags & DEVINFO_FLAG_PRE_PRIV_KEY) {
		if (reg_proto_generate_prebuild_dhkeypair(
			&dev_info->DHSecret, dev_info->pre_privkey) != WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "wps_init::prebuild_dhkeypair failed!\n"));
			goto error;
		}
	}
	else {
		if (reg_proto_generate_dhkeypair(&dev_info->DHSecret) != WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "wps_init::gen dhkeypair failed!\n"));
			goto error;
		}
	}

	gp_mc->mb_initialized = true;
	TUTRACE((TUTRACE_INFO, "wps_init::Done!\n"));

	/* Everything's initialized ok */
	gp_mc->bcmwps = bcmwps;

	return (void *)gp_mc;

error:
	TUTRACE((TUTRACE_ERR, "wps_init::Init failed\n"));
	if (gp_mc) {
		wps_deinit(gp_mc);
	}

	return 0;
}

/*
 * Name        : DeInit
 * Description : Deinitialize member variables
 * Arguments   : none
 * Return type : uint32
 */
uint32
wps_deinit(WPSAPI_T *g_mc)
{
	if (g_mc) {
		/* Delete enrollee/registrar state machines */
		enr_sm_delete(g_mc->mp_enrSM);
		reg_sm_delete(g_mc->mp_regSM);

		/* Delete local device info */
		devinfo_delete(g_mc->dev_info);

		/* Free context */
		free(g_mc);
	}

	TUTRACE((TUTRACE_INFO, "wps_deinit::Done!\n"));

	return WPS_SUCCESS;
}

/*
 * Name        : StartStack
 * Description : Once stack is initialized, start operations.
 * Arguments   : none
 * Return type : uint32 - result of the operation
 */
static uint32
wpsap_start(WPSAPI_T *g_mc, char *pin)
{
	DevInfo *dev_info = g_mc->dev_info;
	uint32 ret = WPS_SUCCESS;

	if (!g_mc->mb_initialized) {
		TUTRACE((TUTRACE_INFO, "wpsap_startStack: Not initialized\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	if (g_mc->mb_stackStarted) {
		TUTRACE((TUTRACE_INFO, "wpsap_startStack: Already started\n"));
		return MC_ERR_STACK_ALREADY_STARTED;
	}

	/* Now perform mode-specific startup */
	switch (dev_info->sc_mode) {
	case SCMODE_AP_ENROLLEE:
		TUTRACE((TUTRACE_INFO, "wpsap_start: SCMODE_AP_ENROLLEE enter\n"));
		/* Cread esm7ap to tell the registrar our settings */
		ret = wpsap_createM7AP(g_mc);
		if (ret != WPS_SUCCESS)
			break;

		/* Instantiate the Enrollee SM */
		g_mc->mp_enrSM = enr_sm_new(g_mc);
		if (!g_mc->mp_enrSM) {
			TUTRACE((TUTRACE_ERR,
				"wpsap_start: Could not allocate g_mc->mp_enrSM\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		/* Call in to the SM */
		if (enr_sm_initializeSM(g_mc->mp_enrSM, dev_info) != WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "Can't init enr_sm\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		/* Send the right IEs down to the transport */
		/* WSC 2.0, no authorizedMacs */
		wps_setWpsIE(g_mc,
			dev_info->scState,
			false,					/* Not a selected registrar */
			WPS_DEVICEPWDID_DEFAULT,
			dev_info->configMethods,
			NULL,
			0);
		break;

	case SCMODE_AP_REGISTRAR:
		TUTRACE((TUTRACE_INFO, "wpsap_start: SCMODE_AP_REGISTRAR enter\n"));

		/* Create the encrypted settings TLV for STA */
		ret = wpsap_createM8Sta(g_mc);
		if (ret != WPS_SUCCESS)
			break;

		/* Instantiate the Registrar SM */
		g_mc->mp_regSM = reg_sm_new(g_mc);
		if (!g_mc->mp_regSM) {
			TUTRACE((TUTRACE_ERR,
				"wpsap_start: Could not allocate g_mc->mp_regSM\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		/* Initialize the Registrar SM */
		if (reg_sm_initsm(g_mc->mp_regSM, dev_info, true, true) != WPS_SUCCESS)
			return WPS_ERR_OUTOFMEMORY;

		if (strlen(pin)) {
			TUTRACE((TUTRACE_ERR, "Using pwd from app\n"));

			/* Send the right IEs down to the transport */
			/* WSC 2.0 AuthorizedMacs */
			wps_setWpsIE(g_mc,
				dev_info->scState,
				true, /* Pin entered, this device is a selected registrar */
				dev_info->devPwdId,
				dev_info->configMethods,
				(uint8 *)dev_info->authorizedMacs,
				dev_info->authorizedMacs_len);
		}
		break;

	default:
		TUTRACE((TUTRACE_ERR, "MC_SwitchModeOn: Unknown mode\n"));
		return WPS_ERR_GENERIC;
	}

	g_mc->mb_stackStarted = true;

	TUTRACE((TUTRACE_INFO, "wpsap_startStack: Done!\n"));
	return WPS_SUCCESS;
}

#ifndef WPS_ROUTER
/*
 * Name        : SetBeaconIE
 * Description : Push Beacon WPS IE information to transport
 * Arguments   : IN bool b_configured - is the AP configured?
 * IN bool b_selRegistrar - is this flag set?
 * IN uint16 devPwdId - valid if b_selRegistrar is true
 * IN uint16 selRegCfgMethods - valid if b_selRegistrar is true
 * Return type : uint32 - result of the operation
 */
static uint32
wps_setBeaconIE(WPSAPI_T *g_mc, IN bool b_configured, IN bool b_selRegistrar,
	IN uint16 devPwdId, IN uint16 selRegCfgMethods, uint8 *authorizedMacs_buf,
	int authorizedMacs_len)
{
	uint32 ret;
	uint8 data8;
	uint8 *p_data8;
	CTlvVendorExt vendorExt;
	DevInfo *info = g_mc->dev_info;

	BufferObj *bufObj, *vendorExt_bufObj;

	bufObj = buffobj_new();
	if (!bufObj)
		return WPS_ERR_SYSTEM;

	/* Version */
	data8 = WPS_VERSION;
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Simple Config State */
	if (b_configured)
		data8 = WPS_SCSTATE_CONFIGURED;
	else
		data8 = WPS_SCSTATE_UNCONFIGURED;
	tlv_serialize(WPS_ID_SC_STATE, bufObj, &data8, WPS_ID_SC_STATE_S);

	/*
	 * AP Setup Locked - optional if false.  If implemented and TRUE, include this attribute.
	 * CTlvAPSetupLocked(WPS_ID_AP_SETUP_LOCKED, bufObj, &b_APSetupLocked);
	 *
	 * Selected Registrar - optional
	 * Add this TLV only if b_selRegistrar is true
	 */
	if (b_selRegistrar) {
		tlv_serialize(WPS_ID_SEL_REGISTRAR, bufObj, &b_selRegistrar,
			WPS_ID_SEL_REGISTRAR_S);
		/* Add in other related params as well Device Password ID */
		tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &devPwdId, WPS_ID_DEVICE_PWD_ID_S);
		/* Selected Registrar Config Methods */
		tlv_serialize(WPS_ID_SEL_REG_CFG_METHODS, bufObj,
			&selRegCfgMethods, WPS_ID_SEL_REG_CFG_METHODS_S);
		/* Enrollee UUID removed */
	}

	data8 = info->rfBand;
	/* dual band, it's become require */
	if (data8 == 3) {
		p_data8 = info->uuid;
		tlv_serialize(WPS_ID_UUID_E, bufObj, p_data8, SIZE_UUID);
		tlv_serialize(WPS_ID_RF_BAND, bufObj, &data8, WPS_ID_RF_BAND_S);
	}

	/* WSC 2.0,  support WPS V2 or not */
	data8 = info->version2;
	if (data8 >= WPS_VERSION2) {
		vendorExt_bufObj = buffobj_new();
		if (vendorExt_bufObj == NULL) {
			buffobj_del(bufObj);
			return WPS_ERR_SYSTEM;
		}

		/* WSC 2.0 */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&data8, WPS_WFA_SUBID_VERSION2_S);
		/* AuthorizedMACs */
		if (authorizedMacs_buf && authorizedMacs_len) {
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, vendorExt_bufObj,
				authorizedMacs_buf, authorizedMacs_len);
		}

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, bufObj);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
{
	char *pc_data;
	uint16 data16;

	/* New unknown attribute */
	pc_data = info->nattr_tlv;
	data16 = info->nattr_len;
	if (data16 && pc_data)
		buffobj_Append(bufObj, data16, pc_data);
}
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Send a pointer to the serialized data to Transport */
	if (0 != wps_set_wps_ie(g_mc->bcmwps, bufObj->pBase, bufObj->m_dataLength,
		1 /* WPS_IE_TYPE_SET_BEACON_IE */ )) {
		TUTRACE((TUTRACE_ERR, "MC_SetBeaconIE: call to trans->SetBeaconIE() failed\n"));
		ret = WPS_ERR_GENERIC;
	}
	else {
		TUTRACE((TUTRACE_ERR, "MC_SetBeaconIE: call to trans->SetBeaconIE() ok\n"));
		ret = WPS_SUCCESS;
	}

	buffobj_del(bufObj);
	return ret;
}

/*
 * Name        : SetProbeRespIE
 * Description : Push WPS Probe Resp WPS IE information to transport
 * Arguments   :
 * Return type : uint32 - result of the operation
 */
static uint32
wps_setProbeRespIE(WPSAPI_T *g_mc, IN uint8 respType, IN uint8 scState,
	IN bool b_selRegistrar, IN uint16 devPwdId, IN uint16 selRegCfgMethods,
	uint8 *authorizedMacs_buf, int authorizedMacs_len)
{
	uint32 ret;
	BufferObj *bufObj, *vendorExt_bufObj;
	uint16 data16;
	uint8 data8, *p_data8;
	char *pc_data;
	CTlvPrimDeviceType tlvPrimDeviceType;
	CTlvVendorExt vendorExt;
	DevInfo *info = g_mc->dev_info;

	bufObj = buffobj_new();
	if (!bufObj)
		return WPS_ERR_SYSTEM;

	/* Version */
	data8 = WPS_VERSION;
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Simple Config State */
	tlv_serialize(WPS_ID_SC_STATE, bufObj, &scState, WPS_ID_SC_STATE_S);

	/* AP Setup Locked - optional if false.  If implemented and TRUE, include this attribute. */
	/*
	 * tlv_serialize(WPS_ID_AP_SETUP_LOCKED, bufObj, &b_APSetupLocked,
	 * WPS_ID_AP_SETUP_LOCKED_S);
	 */

	/* Selected Registrar */
	tlv_serialize(WPS_ID_SEL_REGISTRAR, bufObj, &b_selRegistrar, WPS_ID_SEL_REGISTRAR_S);

	/*
	 * Selected Registrar Config Methods - optional, required if b_selRegistrar
	 * is true
	 * Device Password ID - optional, required if b_selRegistrar is true
	 * Enrollee UUID - optional, required if b_selRegistrar is true
	 */
	if (b_selRegistrar) {
		/* Device Password ID */
		tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &devPwdId, WPS_ID_DEVICE_PWD_ID_S);
		/* Selected Registrar Config Methods */
		tlv_serialize(WPS_ID_SEL_REG_CFG_METHODS, bufObj, &selRegCfgMethods,
			WPS_ID_SEL_REG_CFG_METHODS_S);
		/* Per 1.0b spec, removed Enrollee UUID */
	}

	/* Response Type */
	tlv_serialize(WPS_ID_RESP_TYPE, bufObj, &respType, WPS_ID_RESP_TYPE_S);

	p_data8 = info->uuid;
	tlv_serialize(WPS_ID_UUID_E, bufObj, p_data8, SIZE_UUID);

	/* Manufacturer */
	pc_data = info->manufacturer;
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_MANUFACTURER, bufObj, pc_data, data16);

	/* Model Name */
	pc_data = info->modelName;
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_MODEL_NAME, bufObj, pc_data, data16);

	/* Model Number */
	pc_data = info->modelNumber;
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_MODEL_NUMBER, bufObj, pc_data, data16);

	/* Serial Number */
	pc_data = info->serialNumber;
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_SERIAL_NUM, bufObj, pc_data, data16);

	/*
	 * Primary Device Type
	 * This is a complex TLV, so will be handled differently
	 */
	tlvPrimDeviceType.categoryId = info->primDeviceCategory;
	tlvPrimDeviceType.oui = info->primDeviceOui;
	tlvPrimDeviceType.subCategoryId = info->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&tlvPrimDeviceType, bufObj);

	/* Device Name */
	pc_data = info->deviceName;
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_DEVICE_NAME, bufObj, pc_data, data16);

	/* Config Methods */
	/*
	* 1: In WPS Test Plan 2.0.3, case 4.1.2 step4. APUT cannot use push button to add ER,
	* 2: In P2P Test Plan 4.2.1, PBC bit is checked to be disabled in WPS2.0.
	* 3: DMT PBC testing check the PBC method from CONFIG_METHODS field (for now, DMT
	* testing only avaliable on WPS 1.0.
	* Remove PBC in WPS 2.0 anyway.
	*/
	data16 = info->configMethods;
	data8 = info->version2;
	if (data8 >= WPS_VERSION2)
		data16 &= ~(WPS_CONFMET_VIRT_PBC | WPS_CONFMET_PHY_PBC);
	else
	{
		/* WPS 1.0 */
		if (scState == WPS_SCSTATE_UNCONFIGURED)
		{
			data16 &= ~WPS_CONFMET_PBC;
		}
	}
	tlv_serialize(WPS_ID_CONFIG_METHODS, bufObj, &data16, WPS_ID_CONFIG_METHODS_S);

	/* RF Bands - optional */
	data8 = info->rfBand;
	if (data8 == 3)
		tlv_serialize(WPS_ID_RF_BAND, bufObj, &data8, WPS_ID_RF_BAND_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	data8 = info->version2;
	if (data8 >= WPS_VERSION2) {
		vendorExt_bufObj = buffobj_new();
		if (vendorExt_bufObj == NULL) {
			buffobj_del(bufObj);
			return WPS_ERR_SYSTEM;
		}

		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&data8, WPS_WFA_SUBID_VERSION2_S);

		/* AuthorizedMACs */
		if (authorizedMacs_buf && authorizedMacs_len) {
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, vendorExt_bufObj,
				authorizedMacs_buf, authorizedMacs_len);
		}

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, bufObj);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		pc_data = info->nattr_tlv;
		data16 = info->nattr_len;
		if (data16 && pc_data)
			buffobj_Append(bufObj, data16, pc_data);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Send a pointer to the serialized data to Transport */
	if (0 != wps_set_wps_ie(g_mc->bcmwps, bufObj->pBase, bufObj->m_dataLength,
		WPS_IE_TYPE_SET_PROBE_RESPONSE_IE)) {
		TUTRACE((TUTRACE_ERR, "wps_setProbeRespIE: call wps_set_wps_ie() failed\n"));
		ret = WPS_ERR_GENERIC;
	}
	else {
		TUTRACE((TUTRACE_ERR, "wps_setProbeRespIE: call wps_set_wps_ie() ok\n"));
		ret = WPS_SUCCESS;
	}

	/* bufObj will be destroyed when we exit this function */
	buffobj_del(bufObj);
	return ret;
}

static uint32
wps_setAssocRespIE(WPSAPI_T *g_mc, IN uint8 respType)
{
	uint32 ret;
	uint8 data8;
	CTlvVendorExt vendorExt;
	BufferObj *bufObj, *vendorExt_bufObj;
	DevInfo *info = g_mc->dev_info;

	bufObj = buffobj_new();
	if (!bufObj)
		return WPS_ERR_SYSTEM;

	vendorExt_bufObj = buffobj_new();
	if (vendorExt_bufObj == NULL) {
		buffobj_del(bufObj);
		return WPS_ERR_SYSTEM;
	}

	/* Version */
	data8 = WPS_VERSION;
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Response Type */
	tlv_serialize(WPS_ID_RESP_TYPE, bufObj, &respType, WPS_ID_RESP_TYPE_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	data8 = info->version2;
	subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj, &data8,
		WPS_WFA_SUBID_VERSION2_S);

	/* Serialize subelemetns to Vendor Extension */
	vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
	vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
	vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
	tlv_vendorExtWrite(&vendorExt, bufObj);

	buffobj_del(vendorExt_bufObj);

#ifdef WFA_WPS_20_TESTBED
{
	char *pc_data;
	uint16 data16;

	/* New unknown attribute */
	pc_data = info->nattr_tlv;
	data16 = info->nattr_len;
	if (data16 && pc_data)
		buffobj_Append(bufObj, data16, pc_data);
}
#endif /* WFA_WPS_20_TESTBED */
	/* Send a pointer to the serialized data to Transport */
	if (0 != wps_set_wps_ie(g_mc->bcmwps, bufObj->pBase, bufObj->m_dataLength,
		WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE)) {
		TUTRACE((TUTRACE_ERR, "wps_setAssocRespIE: call wps_set_wps_ie() failed\n"));
		ret = WPS_ERR_GENERIC;
	}
	else {
		TUTRACE((TUTRACE_ERR, "wps_setAssocRespIE: call wps_set_wps_ie() ok\n"));
		ret = WPS_SUCCESS;
	}

	/* bufObj will be destroyed when we exit this function */
	buffobj_del(bufObj);
	return ret;
}

int
wps_setWpsIE(void *bcmdev, uint8 scState, uint8 selRegistrar,
	uint16 devPwdId, uint16 selRegCfgMethods, uint8 *authorizedMacs_buf,
	int authorizedMacs_len)
{
	uint8 data8;
	WPSAPI_T *g_mc = (WPSAPI_T *)bcmdev;


	/* Enable beacon IE */
	wps_setBeaconIE(g_mc,
		scState == WPS_SCSTATE_CONFIGURED,
		selRegistrar,
		devPwdId,
		selRegCfgMethods,
		authorizedMacs_buf,
		authorizedMacs_len);

	/* Enable probe response IE */
	wps_setProbeRespIE(g_mc,
		WPS_MSGTYPE_AP_WLAN_MGR,
		scState,
		selRegistrar,
		devPwdId,
		selRegCfgMethods,
		authorizedMacs_buf,
		authorizedMacs_len);

	/* Enable associate response IE */
	data8 = g_mc->dev_info->version2;
	if (data8 >= WPS_VERSION2)
		wps_setAssocRespIE(g_mc, WPS_MSGTYPE_AP_WLAN_MGR);

	return 0;
}
#endif	/* !WPS_ROUTER */

static uint32
wpsap_createM7AP(WPSAPI_T *g_mc)
{
	char *cp_data;
	uint16 data16;
	uint8 *p_macAddr;
	CTlvNwKey *p_tlvKey;
	char *cp_psk;
	DevInfo *info = g_mc->dev_info;
	EsM7Ap *es;

	/*
	 * AP wants to be configured/managed
	 * Let the Enrollee SM know, so that it can be initialized
	 *
	 * Create an Encrypted Settings TLV to be passed to the SM,
	 * so that the SM can pass it to the Registrar in M7
	 * Also store this TLV locally, so we can delete it once the
	 * registration completes. The SM will not delete it.
	 */
	if (info->mp_tlvEsM7Ap) {
		TUTRACE((TUTRACE_ERR, "wpsap_createM7AP: info->mp_tlvEsM7Ap exist!"));
		reg_msg_es_del(info->mp_tlvEsM7Ap, 0);
	}

	info->mp_tlvEsM7Ap = (EsM7Ap *)reg_msg_es_new(ES_TYPE_M7AP);
	if (!info->mp_tlvEsM7Ap)
		return WPS_ERR_SYSTEM;

	es = info->mp_tlvEsM7Ap;

	/* SSID */
	cp_data = info->ssid;
	data16 = strlen(cp_data);
	tlv_set(&es->ssid, WPS_ID_SSID, cp_data, data16);

	/* MAC addr:  should really get this from the NIC driver */
	p_macAddr = info->macAddr;
	data16 = SIZE_MAC_ADDR;
	tlv_set(&es->macAddr, WPS_ID_MAC_ADDR, p_macAddr, data16);

	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM7Ap: "
		"do REAL sec config here...\n"));
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

	tlv_set(&es->authType, WPS_ID_AUTH_TYPE, UINT2PTR(data16), 0);

	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM7Ap: "
		"Auth type = %x...\n", data16));

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

	tlv_set(&es->encrType, WPS_ID_ENCR_TYPE, UINT2PTR(data16), 0);

	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM7Ap: "
		"encr type = %x...\n", data16));

	if (data16 != WPS_ENCRTYPE_NONE) {
		char *dev_key = NULL;
		uint32	KeyLen;

		if (data16 == WPS_ENCRTYPE_WEP)
			tlv_set(&es->wepIdx, WPS_ID_WEP_TRANSMIT_KEY,
				UINT2PTR(info->wepKeyIdx), 0);

		dev_key = info->nwKey;
		KeyLen = strlen(dev_key);

		/* nwKey */
		p_tlvKey = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
		if (!p_tlvKey)
			return WPS_ERR_SYSTEM;

		if (tlv_allocate(p_tlvKey, WPS_ID_NW_KEY, KeyLen) == -1) {
			tlv_delete(p_tlvKey, 0);
			return WPS_ERR_SYSTEM;
		}

		cp_psk = p_tlvKey->m_data;
		memset(cp_psk, 0, KeyLen);

		memcpy(cp_psk, dev_key, KeyLen);
#ifdef BCMWPS_DEBUG_PRINT_KEY
		TUTRACE((TUTRACE_INFO,	"InitiateRegistration_NW_KEY get from AP is key "
			"%p %s\n",  p_tlvKey, cp_psk));
#endif
	}
	else {
		/* nwKey */
		p_tlvKey = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
		if (!p_tlvKey)
			return WPS_ERR_SYSTEM;

		if (tlv_allocate(p_tlvKey, WPS_ID_NW_KEY, 0) == -1) {
			tlv_delete(p_tlvKey, 0);
			return WPS_ERR_SYSTEM;
		}
	}
	if (!wps_sslist_add(&es->nwKey, p_tlvKey)) {
		tlv_delete(p_tlvKey, 0);
		return WPS_ERR_SYSTEM;
	}

	return WPS_SUCCESS;
}

static uint32
wpsap_createM8Sta(WPSAPI_T *g_mc)
{
	char *cp_data;
	uint16 data16;
	CTlvCredential *p_tlvCred;
	uint32 nwKeyLen = 0;
	char *p_nwKey;
	uint8 *p_macAddr;
	uint8 wep_exist = 0;
	uint8 data8;
#ifdef WFA_WPS_20_TESTBED
#define DUMMY_CRED_NUM 1
	int dummy_cred_remaining = DUMMY_CRED_NUM;
	bool dummy1_cred_wep[DUMMY_CRED_NUM] = {false, false};
#endif /* WFA_WPS_20_TESTBED */
	DevInfo *info = g_mc->dev_info;
	EsM8Sta *es;

	/*
	 * Create the Encrypted Settings TLV for STA config
	 * We will also need to delete this blob eventually, as the
	 * SM will not delete it.
	 */
	if (info->mp_tlvEsM8Sta) {
		TUTRACE((TUTRACE_ERR, "wpsap_createM8Sta: info->mp_tlvEsM8Sta exist!"));
		reg_msg_es_del(info->mp_tlvEsM8Sta, 0);
	}
	info->mp_tlvEsM8Sta = (EsM8Sta *)reg_msg_es_new(ES_TYPE_M8STA);
	if (!info->mp_tlvEsM8Sta)
		return WPS_ERR_SYSTEM;

	es = info->mp_tlvEsM8Sta;

#ifdef WFA_WPS_20_TESTBED
MSS:
#endif
	/* credential */
	p_tlvCred = (CTlvCredential *)malloc(sizeof(CTlvCredential));
	if (!p_tlvCred)
		return WPS_ERR_SYSTEM;

	memset(p_tlvCred, 0, sizeof(CTlvCredential));

	/* Fill in credential items */
	/* nwIndex */
	tlv_set(&p_tlvCred->nwIndex, WPS_ID_NW_INDEX, (void *)1, 0);
	/* ssid */
	cp_data = info->ssid;
	data16 = strlen(cp_data);
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	if (info->b_zpadding)
		data16 ++;

	if (dummy_cred_remaining && info->b_mca) {
		cp_data = info->dummy_ssid;
		data16 = strlen(cp_data);
		if (info->b_zpadding)
			data16 ++;
	}
#endif /* WFA_WPS_20_TESTBED */
	tlv_set(&p_tlvCred->ssid, WPS_ID_SSID, cp_data, data16);

	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: "
		"do REAL sec config here...\n"));

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

#ifdef WFA_WPS_20_TESTBED
	if (dummy_cred_remaining && info->b_mca) {
		if (dummy1_cred_wep[dummy_cred_remaining-1] == true) {
			/* force dummy cred to wep mode */
			data16 = WPS_AUTHTYPE_OPEN;
		}
	}
#endif
	tlv_set(&p_tlvCred->authType, WPS_ID_AUTH_TYPE, UINT2PTR(data16), 0);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: "
	         "Auth type = 0x%x...\n", data16));

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

#ifdef WFA_WPS_20_TESTBED
	if (dummy_cred_remaining && info->b_mca) {
		if (dummy1_cred_wep[dummy_cred_remaining-1] == true) {
			/* force dummy cred to wep mode */
			data16 = WPS_ENCRTYPE_WEP;
		}
	}
#endif
	tlv_set(&p_tlvCred->encrType, WPS_ID_ENCR_TYPE, UINT2PTR(data16), 0);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: "
		"encr type = 0x%x...\n", data16));
	if (data16 == WPS_ENCRTYPE_WEP)
		wep_exist = 1;

	/* nwKeyIndex */
	/*
	 * WSC 2.0, "Network Key Index" deprecated - only included by WSC 1.0 devices.
	 * Ignored by WSC 2.0 or newer devices.
	 */
	data8 = info->version2;
	if (data8 < WPS_VERSION2) {
		tlv_set(&p_tlvCred->nwKeyIndex, WPS_ID_NW_KEY_INDEX, (void *)1, 0);
	}
	/* nwKey */
	p_nwKey = info->nwKey;
	nwKeyLen = strlen(p_nwKey);
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	if (info->b_zpadding && p_nwKey &&
	    nwKeyLen < SIZE_64_BYTES && strlen(p_nwKey) == nwKeyLen)
		nwKeyLen ++;
#endif /* WFA_WPS_20_TESTBED */
	tlv_set(&p_tlvCred->nwKey, WPS_ID_NW_KEY, p_nwKey, nwKeyLen);

	/* macAddr, Enrollee Station's MAC */
	p_macAddr = info->peerMacAddr;
	data16 = SIZE_MAC_ADDR;
	tlv_set(&p_tlvCred->macAddr, WPS_ID_MAC_ADDR,
	        (uint8 *)p_macAddr, data16);

	/* WepKeyIdx */
	if (wep_exist)
		tlv_set(&p_tlvCred->WEPKeyIndex, WPS_ID_WEP_TRANSMIT_KEY,
			UINT2PTR(info->wepKeyIdx), 0);

	/* WSC 2.0, WFA "Network Key Shareable" subelement */
	data8 = info->version2;
	if (data8 >= WPS_VERSION2 && info->b_nwKeyShareable) {
		data16 = 1;
		subtlv_set(&p_tlvCred->nwKeyShareable, WPS_WFA_SUBID_NW_KEY_SHAREABLE,
			UINT2PTR(data16), 0);
		TUTRACE((TUTRACE_ERR, "wps_createM8Sta: Network Key Shareable is TRUE\n"));
	}

	if (!wps_sslist_add(&es->credential, p_tlvCred)) {
		tlv_credentialDelete(p_tlvCred, 0);
		return WPS_ERR_SYSTEM;
	}

#ifdef WFA_WPS_20_TESTBED
	/*
	 * Multiple Security Settings for testing purpose
	 * First credential is using dummy SSID, the second one is good for station.
	 */
	if (dummy_cred_remaining && info->b_mca) {
		dummy_cred_remaining--;
		goto MSS;
	}
#endif /* WFA_WPS_20_TESTBED */

	return WPS_SUCCESS;
}

/*
* NOTE: The caller MUST call tlv_delete(ssrmsg->authorizedMacs, 1) to free memory which
* allocated in this fcuntion.
*/
int
wps_get_upnpDevSSR(WPSAPI_T *g_mc, void *p_cbData, uint32 length, CTlvSsrIE *ssrmsg)
{
	int ret = WPS_CONT;
	uint8 data8;
	DevInfo *info = g_mc->dev_info;

	TUTRACE((TUTRACE_INFO, "wps_get_upnpDevSSR\n"));

	if (info->sc_mode == SCMODE_AP_REGISTRAR) {
		BufferObj *vendorExt_bufObj = NULL;
		BufferObj *wlidcardMac_bufObj = NULL;

		/* De-serialize the data to get the TLVs */
		BufferObj *bufObj = buffobj_new();
		if (!bufObj)
			return WPS_ERR_SYSTEM;

		buffobj_dserial(bufObj, (uint8 *)p_cbData, length);

		memset(ssrmsg, 0, sizeof(CTlvSsrIE));

		/* Version */
		tlv_dserialize(&ssrmsg->version, WPS_ID_VERSION, bufObj, 0, 0);
		/* Selected Registrar */
		tlv_dserialize(&ssrmsg->selReg, WPS_ID_SEL_REGISTRAR, bufObj, 0, 0);
		/* Device Password ID */
		tlv_dserialize(&ssrmsg->devPwdId, WPS_ID_DEVICE_PWD_ID, bufObj, 0, 0);
		/* Selected Registrar Config Methods */
		tlv_dserialize(&ssrmsg->selRegCfgMethods, WPS_ID_SEL_REG_CFG_METHODS,
			bufObj, 0, 0);

		/* WSC 2.0 */
		data8 = info->version2;
		if (data8 >= WPS_VERSION2) {
			/* Check subelement in WFA Vendor Extension */
			if (tlv_find_vendorExtParse(&ssrmsg->vendorExt, bufObj,
				(uint8 *)WFA_VENDOR_EXT_ID) != 0)
				goto add_wildcard_mac;

			/* Deserialize subelement */
			vendorExt_bufObj = buffobj_new();
			if (!vendorExt_bufObj) {
				buffobj_del(bufObj);
				return -1;
			}

			buffobj_dserial(vendorExt_bufObj, ssrmsg->vendorExt.vendorData,
				ssrmsg->vendorExt.dataLength);

			/* Get Version2 and AuthorizedMACs subelement */
			if (subtlv_dserialize(&ssrmsg->version2, WPS_WFA_SUBID_VERSION2,
				vendorExt_bufObj, 0, 0) == 0) {
				/* AuthorizedMACs, <= 30B */
				subtlv_dserialize(&ssrmsg->authorizedMacs,
					WPS_WFA_SUBID_AUTHORIZED_MACS,
					vendorExt_bufObj, 0, 0);
			}

			/* Add wildcard MAC when authorized mac not specified */
			if (ssrmsg->authorizedMacs.subtlvbase.m_len == 0) {
add_wildcard_mac:
				/*
				 * If the External Registrar is WSC version 1.0 it
				 * will not have included an AuthorizedMACs subelement.
				 * In this case the AP shall add the wildcard MAC Address
				 * (FF:FF:FF:FF:FF:FF) in an AuthorizedMACs subelement in
				 * Beacon and Probe Response frames
				 */
				wlidcardMac_bufObj = buffobj_new();
				if (!wlidcardMac_bufObj) {
					buffobj_del(vendorExt_bufObj);
					buffobj_del(bufObj);
					return -1;
				}

				/* Serialize the wildcard_authorizedMacs to wlidcardMac_Obj */
				subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, wlidcardMac_bufObj,
					(char *)wildcard_authorizedMacs, SIZE_MAC_ADDR);
				buffobj_Rewind(wlidcardMac_bufObj);
				/*
				 * De-serialize the wlidcardMac_Obj data to get the TLVs
				 * Do allocation, because wlidcardMac_bufObj will be freed here but
				 * ssrmsg->authorizedMacs return and used by caller.
				 */
				subtlv_dserialize(&ssrmsg->authorizedMacs,
					WPS_WFA_SUBID_AUTHORIZED_MACS,
					wlidcardMac_bufObj, 0, 1); /* Need to free it */
			}

			/* Get UUID-R after WFA-Vendor Extension (wpa_supplicant use this way) */
			tlv_find_dserialize(&ssrmsg->uuid_R, WPS_ID_UUID_R, bufObj, 0, 0);
		}

		/* Other... */

		if (bufObj)
			buffobj_del(bufObj);

		if (wlidcardMac_bufObj)
			buffobj_del(wlidcardMac_bufObj);

		if (vendorExt_bufObj)
			buffobj_del(vendorExt_bufObj);

		ret = WPS_SUCCESS;
	}

	return ret;
}

int
wps_upnpDevSSR(WPSAPI_T *g_mc, CTlvSsrIE *ssrmsg)
{
	WPS_SCMODE sc_mode = g_mc->dev_info->sc_mode;
	uint8 scState = g_mc->dev_info->scState;

	TUTRACE((TUTRACE_INFO, "MC_Recd CB_TRUPNP_DEV_SSR\n"));

	/*
	 * Added to support SetSelectedRegistrar
	 * If we are an AP+Proxy, then add the SelectedRegistrar TLV
	 * to the WPS IE in the Beacon
	 * This call will fail if the right WLAN drivers are not used.
	 */
	if (sc_mode == SCMODE_AP_REGISTRAR) {
		wps_setWpsIE(g_mc,
			scState,
			ssrmsg->selReg.m_data,
			ssrmsg->devPwdId.m_data,
			ssrmsg->selRegCfgMethods.m_data,
			ssrmsg->authorizedMacs.m_data,
			ssrmsg->authorizedMacs.subtlvbase.m_len);
	}

	return WPS_CONT;
}

uint32
wps_enrUpnpGetDeviceInfoCheck(EnrSM *e, void *inbuffer, uint32 in_len,
	void *outbuffer, uint32 *out_len)
{
	if (!inbuffer || !in_len) {
		if (false == e->reg_info->initialized) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Not yet initialized.\n"));
			return WPS_ERR_NOT_INITIALIZED;
		}

		if (START != e->reg_info->e_smState) {
			TUTRACE((TUTRACE_INFO, "\n======e_lastMsgSent != M1, "
				"Step GetDeviceInfo=%d ======\n",
				wps_getUpnpDevGetDeviceInfo(e->g_mc)));
			if (wps_getUpnpDevGetDeviceInfo(e->g_mc)) {
				wps_setUpnpDevGetDeviceInfo(e->g_mc, false);

				/* copy to msg_to_send buffer */
				if (*out_len < e->reg_info->outMsg->m_dataLength) {
					e->reg_info->e_smState = FAILURE;
					TUTRACE((TUTRACE_ERR, "output message buffer to small\n"));
					return WPS_MESSAGE_PROCESSING_ERROR;
				}
				memcpy(outbuffer, (char *)e->reg_info->outMsg->pBase,
					e->reg_info->outMsg->m_dataLength);
				*out_len = e->reg_info->outMsg->m_dataLength;
				return WPS_SEND_MSG_CONT;
			}
			return WPS_SUCCESS;
		}
	}

	return WPS_CONT;
}

/* Add for PF3 */
static uint32
wps_regUpnpERFilter(RegSM *r, BufferObj *msg, uint32 msgType)
{
	uint32 err;


	/*
	 * If AP have received M2 form one External Registrar,
	 * AP must ignore forwarding messages from other
	 * External Registrars.
	 */
	if (r->m_er_sentM2 == false) {
		if (msgType == WPS_ID_MESSAGE_M2) {
			/* Save R-Nonce */
			err = reg_proto_get_nonce(r->m_er_nonce, msg, WPS_ID_REGISTRAR_NONCE);
			if (err != WPS_SUCCESS) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Get R-Nonce error: %d\n", err));
			}
			else {
				r->m_er_sentM2 = true;
			}
		}
	}
	else {
		/* Filter UPnP to EAP messages by R-Nonce */
		err = reg_proto_check_nonce(r->m_er_nonce, msg, WPS_ID_REGISTRAR_NONCE);
		if (err == RPROT_ERR_NONCE_MISMATCH)
			return WPS_CONT;
	}

	return WPS_SUCCESS;
}

uint32
wps_regUpnpForwardingCheck(RegSM *r, void *inbuffer, uint32 in_len,
	void *outbuffer, uint32 *out_len)
{
	uint32 msgType = 0, err = WPS_SUCCESS, retVal = WPS_CONT;
	TRANSPORT_TYPE trType;
	BufferObj *inMsg = NULL, *outMsg = NULL;

	if (false == r->reg_info->initialized) {
		TUTRACE((TUTRACE_ERR, "REGSM: Not yet initialized.\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	/* Irrespective of whether the local registrar is enabled or whether we're
	 * using an external registrar, we need to send a WPS_Start over EAP to
	 * kickstart the protocol.
	 * If we send a WPS_START msg, we don't need to do anything else i.e.
	 * invoke the local registrar or pass the message over UPnP
	 */
	if ((!in_len) && (START == r->reg_info->e_smState) &&
		((TRANSPORT_TYPE_EAP == r->reg_info->transportType) ||
		(TRANSPORT_TYPE_UPNP_CP == r->reg_info->transportType))) {
		err = wps_sendStartMessage(r->g_mc, r->reg_info->transportType);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: SendStartMessage failed. Err = %d\n", err));
		}
		return WPS_CONT;
	}

	/* Now send the message over UPnP. Else, if the message has come over UPnP
	 * send it over EAP.
	 * if(m_passThruEnabled)
	 */
	/* temporarily disable forwarding to/from UPnP if we've already sent M2
	 * this is a stop-gap measure to prevent multiple registrars from cluttering
	 * up the EAP session.
	 */
	if ((r->m_passThruEnabled) && (!r->m_sentM2) &&
	    wps_get_mode(r->g_mc) == SCMODE_AP_REGISTRAR) {

		if ((inMsg = buffobj_new()) == NULL)
			return WPS_ERR_SYSTEM;

		if (in_len) {
			buffobj_dserial(inMsg, inbuffer, in_len);
			err = reg_proto_get_msg_type(&msgType, inMsg);
		}

		switch (r->reg_info->transportType) {
		case TRANSPORT_TYPE_EAP:
			/* EAP to UPnP:
			 * if add client was performed by AP, skip this forward
			 * if (getBuiltInStart())
			 * break;
			 */
			if (r->reg_info->dev_info->b_ap)
				trType = TRANSPORT_TYPE_UPNP_DEV;
			else
				trType = TRANSPORT_TYPE_UPNP_CP;

			/* recvd from EAP, send over UPnP */
			if (msgType != WPS_ID_MESSAGE_ACK) {
				TUTRACE((TUTRACE_INFO, "REGSM: Forwarding message from "
					"EAP to UPnP.\n"));
				TUTRACE((TUTRACE_INFO, "\n###EAP to UPnP msgType=%x (%s)"
					" ###\n", msgType, wps_msg_type_str(msgType)));
				err = wps_sendMsg(r->g_mc, trType, (char *)inbuffer, in_len);
			}

			/* Now check to see if the EAP message is a WPS_NACK.  If so,
			 * terminate the EAP connection with an EAP-Fail.
			 * if (msgType == WPS_ID_MESSAGE_NACK && WPS_SUCCESS == err) {
			 */
			if (msgType == WPS_ID_MESSAGE_NACK) {
				TUTRACE((TUTRACE_ERR, "REGSM: Notifying MC of failure.\n"));

				/* reset the SM */
				reg_sm_restartsm(r);
				retVal = WPS_MESSAGE_PROCESSING_ERROR;
				break;
			}

			if ((WPS_SUCCESS == err) && (msgType == WPS_ID_MESSAGE_DONE)) {
				TUTRACE((TUTRACE_INFO, "REGSM: Send EAP fail when "
					"DONE or ACK (type=%x).\n", msgType));

				retVal = WPS_SUCCESS;
				break;
			}

			break;

		default:
			/* UPnP to EAP: */
			TUTRACE((TUTRACE_INFO, "REGSM: Forwarding message from UPnP to EAP.\n"));

			/* recvd over UPNP, send it out over EAP */
			TUTRACE((TUTRACE_INFO, "\n======== GetDeviceInfo=%d  =======\n",
				wps_getUpnpDevGetDeviceInfo(r->g_mc)));
			if (in_len) {
				TUTRACE((TUTRACE_INFO, "\n###UPnP to EAP msgType=%x (%s) ###\n",
					msgType, wps_msg_type_str(msgType)));

				if (WPS_SUCCESS == err && msgType == WPS_ID_MESSAGE_M2D) {
					TUTRACE((TUTRACE_INFO, "\n ==received M2D ==\n"));
				}
			}

			if (wps_getUpnpDevGetDeviceInfo(r->g_mc)) {
				outMsg = buffobj_setbuf(outbuffer, *out_len);
				if (!outMsg) {
					retVal = WPS_ERR_SYSTEM;
					break;
				}

				wps_setUpnpDevGetDeviceInfo(r->g_mc, false);

				err = reg_proto_create_devinforsp(r->reg_info, outMsg);
				if (WPS_SUCCESS != err) {
					TUTRACE((TUTRACE_ERR, "BuildMessageM1: %d\n", err));
				}
				else {
					/* Now send the message to the transport */
					err = wps_sendMsg(r->g_mc, r->reg_info->transportType,
						(char *)outMsg->pBase, outMsg->m_dataLength);
				}

				if (WPS_SUCCESS != err) {
					r->reg_info->e_smState = FAILURE;
					TUTRACE((TUTRACE_ERR, "ENRSM: TrWrite generated an "
						"error: %d\n", err));
				}
			}

			if (in_len) {
				if (wps_regUpnpERFilter(r, inMsg, msgType) == WPS_CONT)
					break;

				err = wps_sendMsg(r->g_mc, TRANSPORT_TYPE_EAP,
					(char *)inbuffer, in_len);
			}

			break;
		}
	}

	if (inMsg)
		buffobj_del(inMsg);
	if (outMsg)
		buffobj_del(outMsg);

	return retVal;
}

int
wps_processMsg(void *mc_dev, void *inbuffer, uint32 in_len, void *outbuffer, uint32 *out_len,
	TRANSPORT_TYPE m_transportType)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;
	RegData *regInfo;
	EnrSM *e;
	int ret = WPS_CONT;

	if (!g_mc)
		return WPS_MESSAGE_PROCESSING_ERROR;

	if (wps_get_mode(g_mc) == SCMODE_AP_ENROLLEE) {
		if (g_mc->mp_enrSM) {
			g_mc->mp_enrSM->reg_info->transportType = m_transportType;

			/* Pre-process upnp */
			ret  = wps_enrUpnpGetDeviceInfoCheck(g_mc->mp_enrSM, inbuffer,
				in_len, outbuffer, out_len);
			if (ret != WPS_CONT) {
				if (ret == WPS_SUCCESS)
					return WPS_CONT;
				else
					return ret;
			}

			/* Hacking,
			 * set regInfo's e_lastMsgSent to M1,  m_m2dStatus to SM_AWAIT_M2 and
			 * regInfo's e_smState to CONTINUE,
			 * and create M1 to get regInfo's outMsg which needed in M2 process
			 */
			e = g_mc->mp_enrSM;
			regInfo = e->reg_info;

			if (START == regInfo->e_smState &&
			    (regInfo->dev_info->flags & DEVINFO_FLAG_PRE_NONCE)) {
				uint32 msgType = 0, err;
				BufferObj *inMsg, *outMsg;

				inMsg = buffobj_new();
				if (!inMsg)
					return WPS_ERR_SYSTEM;

				buffobj_dserial(inMsg, inbuffer, in_len);
				err = reg_proto_get_msg_type(&msgType, inMsg);
				buffobj_del(inMsg);

				if (WPS_SUCCESS == err && WPS_ID_MESSAGE_M2 == msgType) {
					outMsg = buffobj_setbuf((uint8 *)outbuffer, *out_len);
					if (!outMsg)
						return WPS_ERR_SYSTEM;

					memcpy(regInfo->enrolleeNonce,
						regInfo->dev_info->pre_nonce,
						SIZE_128_BITS);

					e->reg_info->e_lastMsgSent = M1;
					/* Set the m2dstatus. */
					e->m_m2dStatus = SM_AWAIT_M2;
					/* set the message state to CONTINUE */
					e->reg_info->e_smState = CONTINUE;

					reg_proto_create_m1(regInfo, outMsg);

					buffobj_del(outMsg);
				}
			}

			ret = enr_sm_step(g_mc->mp_enrSM, in_len, ((uint8 *)inbuffer),
				((uint8 *)outbuffer), out_len);
		}
	}
	else {
		if (g_mc->mp_regSM) {
			g_mc->mp_regSM->reg_info->transportType = m_transportType;

			/* Pre-process upnp */
			ret  = wps_regUpnpForwardingCheck(g_mc->mp_regSM, inbuffer,
				in_len, outbuffer, out_len);
			if (ret != WPS_CONT) {
				return ret;
			}

			ret = reg_sm_step(g_mc->mp_regSM, in_len, ((uint8 *)inbuffer),
				((uint8 *)outbuffer), out_len);
		}
	}

	return ret;
}

int
wps_getenrState(void  *mc_dev)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;
	if (g_mc->mp_enrSM)
		return g_mc->mp_enrSM->reg_info->e_lastMsgRecd;
	else
		return 0;
}

int
wps_getregState(void  *mc_dev)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;
	if (g_mc->mp_regSM)
		return g_mc->mp_regSM->reg_info->e_lastMsgSent;
	else
		return 0;
}

void
wps_setUpnpDevGetDeviceInfo(void  *mc_dev, bool value)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;

	if (g_mc)
	{
		g_mc->b_UpnpDevGetDeviceInfo = value;
	}

	return;
}

bool
wps_getUpnpDevGetDeviceInfo(void  *mc_dev)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;

	if (g_mc)
	{
		return g_mc->b_UpnpDevGetDeviceInfo;
	}

	return false;
}

static uint32
wps_sendMsg(void *mcdev, TRANSPORT_TYPE trType, char * dataBuffer, uint32 dataLen)
{
	uint32 retVal = WPS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "In wps_sendMsg\n"));

	if (trType < 1 || trType >= TRANSPORT_TYPE_MAX) {
		TUTRACE((TUTRACE_ERR, "Transport Type is not within the "
			"accepted range\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	switch (trType) {
	case TRANSPORT_TYPE_EAP:
		retVal = ap_eap_sm_sendMsg(dataBuffer, dataLen);
		break;
#ifdef WPS_UPNP_DEVICE
	case TRANSPORT_TYPE_UPNP_DEV:
		retVal = ap_upnp_sm_sendMsg(dataBuffer, dataLen);
		break;
#endif /* WPS_UPNP_DEVICE */
	default:
		break;
	}

	if (retVal != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR,  "WriteData for "
			"trType %d failed.\n", trType));
	}

	return retVal;
}

uint32
wps_sendStartMessage(void *mcdev, TRANSPORT_TYPE trType)
{
	if (trType == TRANSPORT_TYPE_EAP) {
		return (ap_eap_sm_sendWPSStart());
	}
	else {
		TUTRACE((TUTRACE_ERR, "Transport Type invalid\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	/* return WPS_SUCCESS;	unreachable */
}

uint32
wpsap_start_enrollment(void *mc_dev, char *ap_pin)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;
	DevInfo *dev_info = g_mc->dev_info;

	if (!ap_pin || !strlen(ap_pin))
		return WPS_ERR_INVALID_PARAMETERS;

	/* AP enrollee, cannot use push button */
	dev_info->devPwdId = WPS_DEVICEPWDID_DEFAULT;
	wps_strncpy(dev_info->pin, ap_pin, sizeof(dev_info->pin));

	/* Start ap protocol */
	return wpsap_start(g_mc, ap_pin);
}

uint32
wpsap_start_enrollment_devpwid(void *mc_dev, char *ap_pin, uint16 devicepwid)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;
	DevInfo *dev_info = g_mc->dev_info;

	if (!ap_pin || !strlen(ap_pin))
		return WPS_ERR_INVALID_PARAMETERS;

	/* AP enrollee, cannot use push button */
	if (devicepwid == 0)
		dev_info->devPwdId = WPS_DEVICEPWDID_DEFAULT;

	dev_info->devPwdId = devicepwid;
	wps_strncpy(dev_info->pin, ap_pin, sizeof(dev_info->pin));

	/* Start ap protocol */
	return wpsap_start(g_mc, ap_pin);
}

uint32
wpsap_start_registration(void *mc_dev, char *sta_pin)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;
	DevInfo *dev_info = g_mc->dev_info;
	int devicepwid;

	if (!sta_pin)
		return WPS_ERR_INVALID_PARAMETERS;

	if (strcmp(sta_pin, "00000000") == 0)
		devicepwid = WPS_DEVICEPWDID_PUSH_BTN;
	else
		devicepwid = WPS_DEVICEPWDID_DEFAULT;

	/* Save pin */
	dev_info->devPwdId = devicepwid;
	wps_strncpy(dev_info->pin, sta_pin, sizeof(dev_info->pin));

	/* Start ap protocol */
	return wpsap_start(g_mc, sta_pin);
}

/* sta_pin must String format */
uint32
wpsap_start_registration_devpwid(void *mc_dev, char *sta_pin, uint8 *pub_key_hash,
	uint16 devicepwid)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;
	DevInfo *dev_info = g_mc->dev_info;

	if (!sta_pin)
		return WPS_ERR_INVALID_PARAMETERS;

	if (devicepwid == 0) {
		if (strcmp(sta_pin, "00000000") == 0)
			devicepwid = WPS_DEVICEPWDID_PUSH_BTN;
		else
			devicepwid = WPS_DEVICEPWDID_DEFAULT;
	}

	if (pub_key_hash)
		memcpy(dev_info->pub_key_hash, pub_key_hash, sizeof(dev_info->pub_key_hash));

	dev_info->devPwdId = devicepwid;
	wps_strncpy(dev_info->pin, sta_pin, sizeof(dev_info->pin));

	/* Start ap protocol */
	return wpsap_start(g_mc, sta_pin);
}

int
wps_get_mode(void *mc_dev)
{
	WPSAPI_T *mc = (WPSAPI_T *)mc_dev;
	if (mc)
		return mc->dev_info->sc_mode;
	else
		return SCMODE_UNKNOWN;
}

/* Get Enrollee's MAC address */
unsigned char *
wps_get_mac(void *mc_dev)
{
	int mode;

	if (!mc_dev)
		return NULL;

	mode = wps_get_mode(mc_dev);

	return ap_eap_sm_getMac(mode);
}

unsigned char *
wps_get_mac_income(void *mc_dev)
{
	WPSAPI_T *mc = (WPSAPI_T *)mc_dev;
	if (!mc_dev)
		return NULL;

	return mc->dev_info->macAddr;
}

/* WSC 2.0, return 0: support WSC 1.0 only otherwise return WSC V2 version */
uint8
wps_get_version2(void *mc_dev)
{
	WPSAPI_T *mc = (WPSAPI_T *)mc_dev;
	uint8 data8;

	if (mc) {
		data8 = mc->dev_info->version2;
		if (data8 >= WPS_VERSION2)
			return data8;
	}

	return 0;
}

bool
ap_api_is_recvd_m2d(void *mc_dev)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;

	if (!mc_dev || SCMODE_AP_ENROLLEE != wps_get_mode(mc_dev) ||
	    !g_mc->mp_enrSM)
	    return FALSE;

	return (g_mc->mp_enrSM->m_m2dStatus == SM_RECVD_M2D);
}
