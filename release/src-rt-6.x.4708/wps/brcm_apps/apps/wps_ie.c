/*
 * WPS IE
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_ie.c 402957 2013-05-17 08:58:04Z $
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <bn.h>
#include <wps_dh.h>
#include <wpsheaders.h>
#include <wpscommon.h>
#include <sminfo.h>
#ifdef __ECOS
#include <sys/socket.h>
#endif
#include <net/if.h>
#include <wps_wl.h>
#include <tutrace.h>
#include <shutils.h>
#include <wps_upnp.h>
#include <wps_ui.h>
#include <wps_aplockdown.h>
#include <wps_ie.h>

/* under m_xxx in mbuf.h */

#ifdef __CONFIG_WFI__
static int
wps_vendor_ext_write_matched(BufferObj *bufObj, char *osname, int siType);
#endif /* __CONFIG_WFI__ */

#ifdef WFA_WPS_20_TESTBED
extern int new_tlv_convert(uint8 *new_tlv_str, char *nattr_tlv, int *nattr_len);
#endif

static int
wps_set_assoc_rsp_IE(int ess_id, char *ifname)
{
	uint32 ret = 0;
	char prefix[] = "wlXXXXXXXXXX_";
	char *value;
	BufferObj *bufObj, *vendorExt_bufObj;
	uint8 respType = WPS_MSGTYPE_AP_WLAN_MGR;
	uint8 version = WPS_VERSION;
	uint8 version2;
	CTlvVendorExt vendorExt;


	(void)ess_id;

	bufObj = buffobj_new();
	if (bufObj == NULL)
		return 0;

	vendorExt_bufObj = buffobj_new();
	if (vendorExt_bufObj == NULL) {
		buffobj_del(bufObj);
		return 0;
	}

	snprintf(prefix, sizeof(prefix), "%s_", ifname);

	/* Create the IE */
	/* Version */
	tlv_serialize(WPS_ID_VERSION, bufObj, &version, WPS_ID_VERSION_S);

	/* Response Type */
	tlv_serialize(WPS_ID_RESP_TYPE, bufObj, &respType, WPS_ID_RESP_TYPE_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	value = wps_get_conf("wps_version2_num");
	version2 = (uint8)(strtoul(value, NULL, 16));
	subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
		&version2, WPS_WFA_SUBID_VERSION2_S);

	/* Serialize subelemetns to Vendor Extension */
	vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
	vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
	vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
	tlv_vendorExtWrite(&vendorExt, bufObj);

	buffobj_del(vendorExt_bufObj);

#ifdef WFA_WPS_20_TESTBED
{
	int nattr_len;
	char nattr_tlv[SIZE_128_BYTES];

	/* New unknown attribute */
	value = wps_safe_get_conf("wps_nattr");
	new_tlv_convert(value, nattr_tlv, &nattr_len);
	if (nattr_len)
		buffobj_Append(bufObj, nattr_len, nattr_tlv);
}
#endif /* WFA_WPS_20_TESTBED */

	/* Other... */

	/* Send a pointer to the serialized data to Transport */
	ret = wps_wl_set_wps_ie(ifname, bufObj->pBase, bufObj->m_dataLength,
		WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE, OUITYPE_WPS);
	if (ret != 0) {
		TUTRACE((TUTRACE_ERR, "set assocrsp IE on %s failed, response type %d\n",
			ifname, respType));
	}
	else {
		TUTRACE((TUTRACE_ERR, "set assocrsp IE on %s successful, response type %d\n",
			ifname, respType));
	}

	/* bufObj will be destroyed when we exit this function */
	buffobj_del(bufObj);

	return ret;
}

static int
wps_set_prb_rsp_IE(int ess_id, char *ifname, CTlvSsrIE *ssrmsg)
{
	uint32 ret = 0;
	char prefix[] = "wlXXXXXXXXXX_";
	BufferObj *bufObj, *vendorExt_bufObj;
	uint16 data16;
	uint8 *p_data8;
	uint8 data8;
	char *pc_data;
	char tmp[100];
	uint8 respType = WPS_MSGTYPE_AP_WLAN_MGR;
	uint16 primDeviceCategory = 6;
	uint32 primDeviceOui = 0x0050F204;
	uint16 primDeviceSubCategory = 1;
	CTlvPrimDeviceType tlvPrimDeviceType;
	uint8 version = ssrmsg ? ssrmsg->version.m_data : WPS_VERSION;
	uint8 version2, scState;
	uint8 selreg = ssrmsg ? ssrmsg->selReg.m_data : 0;
	bool b_wps_version2 = FALSE;
	CTlvVendorExt vendorExt;


	bufObj = buffobj_new();
	if (bufObj == NULL)
		return 0;

	snprintf(prefix, sizeof(prefix), "%s_", ifname);

	/* Create the IE */
	/* Version */
	tlv_serialize(WPS_ID_VERSION, bufObj, &version, WPS_ID_VERSION_S);

	/* Simple Config State */
	if (ssrmsg) {
		scState = ssrmsg->scState.m_data;
	}
	else {
		/* According to WPS 2.0 section "Wi-Fi Simple Configuration State"
		 * Note: The Internal Registrar waits until successful completion of the protocol
		 * before applying the automatically generated credentials to avoid an accidental
		 * transition from "Not Configured" to "Configured" in the case that a
		 * neighboring device tries to run WSC
		 */
		sprintf(tmp, "ess%d_wps_oob", ess_id);
		if (!strcmp(wps_safe_get_conf(tmp), "disabled") ||
		    !strcmp(wps_safe_get_conf("wps_oob_configured"), "1"))
			scState = WPS_SCSTATE_CONFIGURED;
		else
			scState = WPS_SCSTATE_UNCONFIGURED;
	}

	tlv_serialize(WPS_ID_SC_STATE, bufObj, &scState, WPS_ID_SC_STATE_S);

	/* AP Setup Locked - optional if false */
	if (wps_aplockdown_islocked()) {
		data8 = 0x1;
		tlv_serialize(WPS_ID_AP_SETUP_LOCKED, bufObj, &data8, WPS_ID_AP_SETUP_LOCKED_S);
	}

	/* optional if true */
	if (selreg) {
		/* Selected Registrar */
		tlv_serialize(WPS_ID_SEL_REGISTRAR, bufObj, &selreg, WPS_ID_SEL_REGISTRAR_S);
		/* Device Password ID */
		tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj,
			&ssrmsg->devPwdId.m_data, WPS_ID_DEVICE_PWD_ID_S);
		/* Selected Registrar Config Methods */
		tlv_serialize(WPS_ID_SEL_REG_CFG_METHODS, bufObj,
			&ssrmsg->selRegCfgMethods.m_data, WPS_ID_SEL_REG_CFG_METHODS_S);
	}

	/* Response Type */
	tlv_serialize(WPS_ID_RESP_TYPE, bufObj, &respType, WPS_ID_RESP_TYPE_S);

	/* UUID */
	p_data8 = wps_get_uuid();
	tlv_serialize(WPS_ID_UUID_E, bufObj, p_data8, SIZE_UUID);

	/* Manufacturer */
	pc_data = wps_safe_get_conf("wps_mfstring");
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_MANUFACTURER, bufObj, pc_data, data16);

	/* Model Name */
	pc_data = wps_safe_get_conf("wps_modelname");
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_MODEL_NAME, bufObj, pc_data, data16);

	/* Model Number */
	pc_data = wps_safe_get_conf("wps_modelnum");
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_MODEL_NUMBER, bufObj, pc_data, data16);

	/* Serial Number */
	pc_data = wps_safe_get_conf("boardnum");
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_SERIAL_NUM, bufObj, pc_data, data16);

	/* Primary Device Type */
	/* This is a complex TLV, so will be handled differently */
	tlvPrimDeviceType.categoryId = primDeviceCategory;
	tlvPrimDeviceType.oui = primDeviceOui;
	tlvPrimDeviceType.subCategoryId = primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&tlvPrimDeviceType, bufObj);

	/* Device Name */
	pc_data = wps_safe_get_conf("wps_device_name");
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_DEVICE_NAME, bufObj, pc_data, data16);

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		b_wps_version2 = TRUE;

	/* Config Methods */
	if (b_wps_version2) {
		/* WSC 2.0 */
		data16 = (WPS_CONFMET_VIRT_PBC | WPS_CONFMET_PHY_PBC | WPS_CONFMET_VIRT_DISPLAY);
	} else {
		data16 = (WPS_CONFMET_PBC | WPS_CONFMET_DISPLAY);
	}

	pc_data = wps_get_conf("wps_config_method");
	if (pc_data)
		data16 = (uint16)(strtoul(pc_data, NULL, 16));

	/*
	 * 1.In WPS Test Plan 2.0.3, case 4.1.2 step 4. APUT cannot use push button to add ER,
	 * remove PBC in WPS 2.0 anyway.
	 * 2.DMT PBC testing check the PBC method from CONFIG_METHODS field (for now, DMT
	 * testing only avaliable on WPS 1.0
	 */
	if (b_wps_version2)
		data16 &= ~(WPS_CONFMET_VIRT_PBC | WPS_CONFMET_PHY_PBC);
	else {
		/* WPS 1.0 */
		if (scState == WPS_SCSTATE_UNCONFIGURED)
			data16 &= ~WPS_CONFMET_PBC;
	}
	tlv_serialize(WPS_ID_CONFIG_METHODS, bufObj, &data16, WPS_ID_CONFIG_METHODS_S);

	/* RF Bands - optional */
	sprintf(tmp, "ess%d_band", ess_id);
	pc_data = wps_safe_get_conf(tmp);
	data8 = atoi(pc_data);
	tlv_serialize(WPS_ID_RF_BAND, bufObj, &data8, WPS_ID_RF_BAND_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (b_wps_version2) {
		vendorExt_bufObj = buffobj_new();
		if (vendorExt_bufObj == NULL) {
			buffobj_del(bufObj);
			return 0;
		}

		pc_data = wps_get_conf("wps_version2_num");
		version2 = (uint8)(strtoul(pc_data, NULL, 16));

		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&version2, WPS_WFA_SUBID_VERSION2_S);

		/* AuthorizedMACs */
		if (ssrmsg && ssrmsg->authorizedMacs.subtlvbase.m_len) {
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, vendorExt_bufObj,
				ssrmsg->authorizedMacs.m_data,
				ssrmsg->authorizedMacs.subtlvbase.m_len);
		}

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, bufObj);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
{
		char *value;
		int nattr_len;
		char nattr_tlv[SIZE_128_BYTES];

		/* New unknown attribute */
		value = wps_safe_get_conf("wps_nattr");
		new_tlv_convert(value, nattr_tlv, &nattr_len);
		if (nattr_len)
			buffobj_Append(bufObj, nattr_len, nattr_tlv);}
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Other... */
#ifdef __CONFIG_WFI__
	wps_vendor_ext_write_matched(bufObj, ifname, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE);
#endif /* __CONFIG_WFI__ */

	/* Send a pointer to the serialized data to Transport */
	ret = wps_wl_set_wps_ie(ifname, bufObj->pBase, bufObj->m_dataLength,
		WPS_IE_TYPE_SET_PROBE_RESPONSE_IE, OUITYPE_WPS);
	if (ret != 0) {
		TUTRACE((TUTRACE_ERR, "set probrsp IE on %s failed, selreg is %s\n",
			ifname, selreg ? "TRUE" : "FALSE"));
	}
	else {
		TUTRACE((TUTRACE_ERR, "set probrsp IE on %s successful, selreg is %s\n",
			ifname, selreg ? "TRUE" : "FALSE"));
	}

	/* bufObj will be destroyed when we exit this function */
	buffobj_del(bufObj);

	if (!strcmp(wps_safe_get_conf(strcat_r(prefix, "wep", tmp)), "enabled")) {
		/* set STATIC WEP key OUI IE to frame */
		data8 = 0;
		ret = wps_wl_set_wps_ie(ifname, &data8, 1,
			WPS_IE_TYPE_SET_PROBE_RESPONSE_IE, OUITYPE_PROVISION_STATIC_WEP);
		if (ret != 0)
			TUTRACE((TUTRACE_ERR, "set STATIC WEP to probrsp IE on %s failed\n",
				ifname));
		else
			TUTRACE((TUTRACE_ERR, "set STATIC WEP to probrsp IE on %s success\n",
				ifname));
	}
	else
		ret = wps_wl_del_wps_ie(ifname,
			WPS_IE_TYPE_SET_PROBE_RESPONSE_IE, OUITYPE_PROVISION_STATIC_WEP);

	return ret;
}

static int
wps_set_beacon_IE(int ess_id, char *ifname, CTlvSsrIE *ssrmsg)
{
	uint32 ret;
	char prefix[] = "wlXXXXXXXXXX_";
	uint8 data8;
	uint8 *p_data8;
	char *pc_data;
	char tmp[100];
	uint8 version = ssrmsg ? ssrmsg->version.m_data : WPS_VERSION;
	uint8 version2;
	uint8 selreg = ssrmsg ? ssrmsg->selReg.m_data : 0;
	bool b_wps_version2 = FALSE;
	CTlvVendorExt vendorExt;

	BufferObj *bufObj, *vendorExt_bufObj;


	bufObj = buffobj_new();
	if (bufObj == NULL)
		return 0;

	snprintf(prefix, sizeof(prefix), "%s_", ifname);

	/* Create the IE */
	/* Version */
	tlv_serialize(WPS_ID_VERSION, bufObj, &version, WPS_ID_VERSION_S);

	/* Simple Config State */
	if (ssrmsg) {
		data8 = ssrmsg->scState.m_data;
	}
	else {
		/* According to WPS 2.0 section "Wi-Fi Simple Configuration State"
		 * Note: The Internal Registrar waits until successful completion of the protocol
		 * before applying the automatically generated credentials to avoid an accidental
		 * transition from "Not Configured" to "Configured" in the case that a
		 * neighboring device tries to run WSC
		 */
		sprintf(tmp, "ess%d_wps_oob", ess_id);
		if (!strcmp(wps_safe_get_conf(tmp), "disabled") ||
		    !strcmp(wps_safe_get_conf("wps_oob_configured"), "1"))
			data8 = WPS_SCSTATE_CONFIGURED;
		else
			data8 = WPS_SCSTATE_UNCONFIGURED;
	}

	tlv_serialize(WPS_ID_SC_STATE, bufObj, &data8, WPS_ID_SC_STATE_S);

	/* AP Setup Locked - optional if false */
	if (wps_aplockdown_islocked()) {
		data8 = 0x1;
		tlv_serialize(WPS_ID_AP_SETUP_LOCKED, bufObj, &data8, WPS_ID_AP_SETUP_LOCKED_S);
	}

	/* Selected Registrar - optional if false */
	if (selreg) {
		tlv_serialize(WPS_ID_SEL_REGISTRAR, bufObj, &ssrmsg->selReg.m_data,
			WPS_ID_SEL_REGISTRAR_S);
		tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj,
			&ssrmsg->devPwdId.m_data, WPS_ID_DEVICE_PWD_ID_S);
		tlv_serialize(WPS_ID_SEL_REG_CFG_METHODS, bufObj,
			&ssrmsg->selRegCfgMethods.m_data, WPS_ID_SEL_REG_CFG_METHODS_S);
	}

	/* UUID and RF_BAND for dualband */
	sprintf(tmp, "ess%d_band", ess_id);
	pc_data = wps_safe_get_conf(tmp);
	data8 = atoi(pc_data);
	if (data8 == (WPS_RFBAND_24GHZ | WPS_RFBAND_50GHZ)) {
		p_data8 = wps_get_uuid();
		tlv_serialize(WPS_ID_UUID_E, bufObj, p_data8, 16);
		tlv_serialize(WPS_ID_RF_BAND, bufObj, &data8, WPS_ID_RF_BAND_S);
	}

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		b_wps_version2 = TRUE;

	if (b_wps_version2) {
		vendorExt_bufObj = buffobj_new();
		if (vendorExt_bufObj == NULL) {
			buffobj_del(bufObj);
			return 0;
		}

		/* WSC 2.0 */
		pc_data = wps_get_conf("wps_version2_num");
		version2 = (uint8)(strtoul(pc_data, NULL, 16));

		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&version2, WPS_WFA_SUBID_VERSION2_S);

		/* AuthorizedMACs */
		if (ssrmsg && ssrmsg->authorizedMacs.subtlvbase.m_len) {
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, vendorExt_bufObj,
				ssrmsg->authorizedMacs.m_data,
				ssrmsg->authorizedMacs.subtlvbase.m_len);
		}

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, bufObj);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
{
		char *value;
		int nattr_len;
		char nattr_tlv[SIZE_128_BYTES];

		/* New unknown attribute */
		value = wps_safe_get_conf("wps_nattr");
		new_tlv_convert(value, nattr_tlv, &nattr_len);
		if (nattr_len)
			buffobj_Append(bufObj, nattr_len, nattr_tlv);}
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Other... */
#ifdef __CONFIG_WFI__
	wps_vendor_ext_write_matched(bufObj, ifname, WPS_IE_TYPE_SET_BEACON_IE);
#endif /* __CONFIG_WFI__ */

	/* Send a pointer to the serialized data to Transport */
	ret = wps_wl_set_wps_ie(ifname, bufObj->pBase, bufObj->m_dataLength,
		WPS_IE_TYPE_SET_BEACON_IE, OUITYPE_WPS);
	if (ret != 0) {
		TUTRACE((TUTRACE_ERR, "set beacon IE on %s failed, selreg is %s\n",
			ifname, selreg ? "TRUE" : "FALSE"));
	}
	else {
		TUTRACE((TUTRACE_ERR, "set beacon IE on %s successful, selreg is %s\n",
			ifname, selreg ? "TRUE" : "FALSE"));
	}

	/* bufObj will be destroyed when we exit this function */
	buffobj_del(bufObj);

	if (!strcmp(wps_safe_get_conf(strcat_r(prefix, "wep", tmp)), "enabled")) {
		/* set STATIC WEP key OUI IE to frame */
		data8 = 0;
		ret |= wps_wl_set_wps_ie(ifname, &data8, 1,
			WPS_IE_TYPE_SET_BEACON_IE, OUITYPE_PROVISION_STATIC_WEP);
		if (ret != 0)
			TUTRACE((TUTRACE_ERR, "set STATIC WEP to probrsp IE on %s failed\n",
				ifname));
		else
			TUTRACE((TUTRACE_ERR, "set STATIC WEP to probrsp IE on %s success\n",
				ifname));
	}
	else
		ret = wps_wl_del_wps_ie(ifname,
			WPS_IE_TYPE_SET_BEACON_IE, OUITYPE_PROVISION_STATIC_WEP);

	return ret;

}

/* Get default select registrar information */
int
wps_ie_default_ssr_info(CTlvSsrIE *ssrmsg, unsigned char *authorizedMacs,
	int authorizedMacs_len, BufferObj *authorizedMacs_Obj, unsigned char *wps_uuid,
	BufferObj *uuid_R_Obj, uint8 scState)
{
	uint16 devpwd_id;
	char *pc_data;
	uint16 data16;
	bool b_wps_version2 = FALSE;

	if (!ssrmsg) {
		TUTRACE((TUTRACE_ERR, "get default ssr buffer NULL!!\n"));
		return -1;
	}

	if (!strcmp(wps_ui_get_env("wps_sta_pin"), "00000000"))
		devpwd_id = WPS_DEVICEPWDID_PUSH_BTN;
	else
		devpwd_id = WPS_DEVICEPWDID_DEFAULT;

	/* setup default data to ssrmsg */
	ssrmsg->version.m_data = WPS_VERSION;
	ssrmsg->scState.m_data = scState;	/* Builtin register */
	ssrmsg->selReg.m_data = 1; /* TRUE */
	ssrmsg->devPwdId.m_data = devpwd_id;

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		b_wps_version2 = TRUE;

	if (b_wps_version2)
		data16 = (WPS_CONFMET_VIRT_PBC | WPS_CONFMET_PHY_PBC | WPS_CONFMET_VIRT_DISPLAY);
	else
		data16 = (WPS_CONFMET_DISPLAY | WPS_CONFMET_PBC);

	pc_data = wps_get_conf("wps_config_method");
	if (pc_data)
		data16 = (uint16)(strtoul(pc_data, NULL, 16));
	ssrmsg->selRegCfgMethods.m_data = data16;

	/* Reset ssrmsg->authorizedMacs and uuid_R */
	ssrmsg->authorizedMacs.m_data = NULL;
	ssrmsg->authorizedMacs.subtlvbase.m_len = 0;
	ssrmsg->uuid_R.m_data = NULL;
	ssrmsg->uuid_R.tlvbase.m_len = 0;

	if (b_wps_version2) {
		pc_data = wps_get_conf("wps_version2_num");
		ssrmsg->version2.m_data = (uint8)(strtoul(pc_data, NULL, 16));

		/* Dserialize ssrmsg->authorizedMacs from authorizedMacs */
		if (authorizedMacs && authorizedMacs_len && authorizedMacs_Obj) {
			/* Serialize the authorizedMacs to authorizedMacs_Obj */
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, authorizedMacs_Obj,
				authorizedMacs, authorizedMacs_len);
			buffobj_Rewind(authorizedMacs_Obj);
			/* De-serialize the authorizedMacs data to get the TLVs */
			subtlv_dserialize(&ssrmsg->authorizedMacs, WPS_WFA_SUBID_AUTHORIZED_MACS,
				authorizedMacs_Obj, 0, 0);
		}

		/* Dserialize ssrmsg->uuid_R from wps_uuid */
		if (wps_uuid && uuid_R_Obj) {
			/* Serialize the uuid_R to uuid_R_Obj */
			tlv_serialize(WPS_ID_UUID_R, uuid_R_Obj, wps_uuid, SIZE_UUID);
			buffobj_Rewind(uuid_R_Obj);
			/* De-serialize the uuid_R data to get the TLVs */
			tlv_dserialize(&ssrmsg->uuid_R, WPS_ID_UUID_R, uuid_R_Obj, 0, 0);
		}
	}

	return 0;
}

static void
add_ie(int ess_id, char *ifname, CTlvSsrIE *ssrmsg)
{
	bool b_wps_version2 = FALSE;

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		b_wps_version2 = TRUE;

	/* Apply ie to probe response and beacon */
	wps_set_beacon_IE(ess_id, ifname, ssrmsg);
	wps_set_prb_rsp_IE(ess_id, ifname, ssrmsg);

	if (b_wps_version2) {
		/* WSC 2.0 */
		wps_set_assoc_rsp_IE(ess_id, ifname);
	}

	wps_wl_open_wps_window(ifname);
}

static void
del_ie(char *ifname)
{
	bool b_wps_version2 = FALSE;

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		b_wps_version2 = TRUE;

	/* WPS IE */
	wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_BEACON_IE,
		OUITYPE_WPS);
	wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE,
		OUITYPE_WPS);

	if (b_wps_version2) {
		/* WSC 2.0 */
		wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE,
			OUITYPE_WPS);
	}

	/* Provision Static WEP IE */
	wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_BEACON_IE,
		OUITYPE_PROVISION_STATIC_WEP);
	wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE,
		OUITYPE_PROVISION_STATIC_WEP);
	wps_wl_close_wps_window(ifname);
}

void
wps_ie_set(char *wps_ifname, CTlvSsrIE *ssrmsg)
{
	char ifname[IFNAMSIZ];
	char *next = NULL;

	int i, imax;
	int matched = -1;
	int matched_band = -1;
	char tmp[100];
	char *wlnames, *wl_mode;
	int band_flag;
	int band;

	/* Search matched ess with the wps_ifname */
	imax = wps_get_ess_num();
	if (wps_ifname) {
		for (i = 0; i < imax; i++) {
			sprintf(tmp, "ess%d_wlnames", i);
			wlnames = wps_safe_get_conf(tmp);
			if (strlen(wlnames) == 0)
				continue;

			foreach(ifname, wlnames, next) {
				if (strcmp(ifname, wps_ifname) == 0) {
					sprintf(tmp, "%s_band", ifname);
					matched_band = atoi(wps_safe_get_conf(tmp));
					matched = i;
					goto found;
				}
			}
		}
	}

found:
	for (i = 0; i < imax; i++) {
		sprintf(tmp, "ess%d_wlnames", i);
		wlnames = wps_safe_get_conf(tmp);
		if (strlen(wlnames) == 0)
			continue;

		band_flag = 0;
		foreach(ifname, wlnames, next) {
			sprintf(tmp, "%s_mode", ifname);
			wl_mode = wps_safe_get_conf(tmp);
			if (strcmp(wl_mode, "ap") != 0)
				continue; /* Only for AP mode */

			sprintf(tmp, "%s_band", ifname);
			band = atoi(wps_safe_get_conf(tmp));
			/*
			  * 1. If wps_ifname is null, set to all the wl interfaces.
			  * 2. Set ie to the exact matched wl interface if wps_ifname is given.
			  * 3. For each band, at most one wl interface is able to set the ssrmsg.
			  */
			if (wps_ifname == NULL || strcmp(wps_ifname, ifname) == 0 ||
			    ((i == matched) && (band != matched_band) && !(band_flag & band))) {
				/* Set ssrmsg to expected wl interface */
				add_ie(i, ifname, ssrmsg);
				band_flag |= band;
			}
			else {
				del_ie(ifname);
			}
		}
	}

	return;
}


void
wps_ie_clear()
{
	char ifname[IFNAMSIZ];
	char *next = NULL;
	int i, imax;
	char tmp[100];
	char *wlnames, *wl_mode;
	bool b_wps_version2 = FALSE;

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		b_wps_version2 = TRUE;

	imax = wps_get_ess_num();
	for (i = 0; i < imax; i++) {
		sprintf(tmp, "ess%d_wlnames", i);
		wlnames = wps_safe_get_conf(tmp);
		if (strlen(wlnames) == 0)
			continue;

		foreach(ifname, wlnames, next) {
			sprintf(tmp, "%s_mode", ifname);
			wl_mode = wps_safe_get_conf(tmp);
			if (strcmp(wl_mode, "ap") != 0)
				continue; /* Only for AP mode */

			wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_BEACON_IE,
				OUITYPE_WPS);
			wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE,
				OUITYPE_WPS);
			if (b_wps_version2) {
				/* WSC 2.0 */
				wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE,
				OUITYPE_WPS);
			}

			wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_BEACON_IE,
				OUITYPE_PROVISION_STATIC_WEP);
			wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE,
				OUITYPE_PROVISION_STATIC_WEP);
			wps_wl_close_wps_window(ifname);
		}
	}

	return;
}

#ifdef __CONFIG_WFI__
/*
*  Generic Vendor Extension Support:
*/
typedef struct _wps_vndr_ext_obj_t {
	struct _wps_vndr_ext_obj_t *ptNext;
	char acPrefix[IFNAMSIZ + 1];
	char acOSName[IFNAMSIZ + 1];
	int siType; /* WPS_IE_TYPE_SET_BEACON_IE or WPS_IE_TYPE_SET_PROBE_RESPONSE_IE */
	int siSize; /* Buffer Size */
	int siLen; /* Data Length */
	char *pcData; /* Data Pointer to end of this struct if siSize > 0 during alloc */
}	wps_vndr_ext_obj_t;
#define WPSM_VNDR_EXT_MODE_INACTIVE		0x80000000

static wps_vndr_ext_obj_t *ptVEList = NULL;

/*
*  Write all vendor exts with the matching type to the bufObj for a particular osname.
*/
static int
wps_vendor_ext_write_matched(BufferObj *bufObj, char *osname, int siType)
{
	int siTotal = 0;
	wps_vndr_ext_obj_t *ptObj;

	for (ptObj = ptVEList; ptObj; ptObj = ptObj->ptNext) {
		if (ptObj->siType != siType || ptObj->pcData == NULL || ptObj->siLen <= 0 ||
			strcmp(ptObj->acOSName, osname) != 0)
			continue;
		tlv_serialize(WPS_ID_VENDOR_EXT, bufObj, ptObj->pcData, ptObj->siLen);
		siTotal += ptObj->siLen;
	}

	return siTotal;
}

int
wps_vndr_ext_obj_free(void *ptToFree)
{
	wps_vndr_ext_obj_t *ptObj, *ptPrev;

	if (ptToFree == NULL)
		return 0;

	for (ptPrev = NULL, ptObj = ptVEList; ptObj; ptObj = ptObj->ptNext) {
		if (ptObj == (wps_vndr_ext_obj_t *)ptToFree) {
			if (ptPrev)
				ptPrev->ptNext = ptObj->ptNext;
			else
				ptVEList = ptObj->ptNext;
			if (ptObj->pcData && (ptObj->pcData !=
				((char *)&ptObj->pcData + sizeof(ptObj->pcData))))
				free(ptObj->pcData);
			free(ptObj);
			return 1;
		}
		ptPrev = ptObj;
	}
	return -1;
}

void *
wps_vndr_ext_obj_alloc(int siBufferSize, char *pcOSName)
{
	wps_vndr_ext_obj_t *ptObj;
	char prefix[IFNAMSIZ];

	if (siBufferSize > WPSM_VNDR_EXT_MAX_DATA_80211)
		return NULL;

	if (siBufferSize < 0)
		siBufferSize = WPSM_VNDR_EXT_MAX_DATA_80211;

	ptObj = (wps_vndr_ext_obj_t *)malloc((sizeof(*ptObj) + siBufferSize));
	if (ptObj == NULL)
		return NULL;

	memset(ptObj, 0, sizeof(*ptObj) + siBufferSize);

	/* nvram prefix */
	snprintf(prefix, sizeof(prefix), "%s_", pcOSName);
	strncpy(ptObj->acPrefix, prefix, IFNAMSIZ);

	strncpy(ptObj->acOSName, pcOSName, IFNAMSIZ);
	ptObj->siSize = siBufferSize;
	if (siBufferSize == 0)
		ptObj->pcData = NULL;
	else
		ptObj->pcData = (char *)&ptObj->pcData + sizeof(ptObj->pcData);

	ptObj->ptNext = ptVEList;
	ptVEList = ptObj;

	return ptObj;
}

int
wps_vndr_ext_obj_copy(void *ptVEObj, char *pcData, int siLen)
{
	wps_vndr_ext_obj_t *ptObj = (wps_vndr_ext_obj_t *)ptVEObj;

	if (siLen > ptObj->siSize)
		return -1;

	ptObj->siLen = siLen;
	memcpy(ptObj->pcData, pcData, ptObj->siLen);
	return ptObj->siLen;
}

int
wps_vndr_ext_obj_set_mode(void *ptVEObj, int siType, int boolActivate)
{
	int siTotalEss, siEssID;
	char *pcEssWLs, acNVName[64];
	wps_vndr_ext_obj_t *ptObj = (wps_vndr_ext_obj_t *)ptVEObj;

	if (siType != WPS_IE_TYPE_SET_BEACON_IE && siType != WPS_IE_TYPE_SET_PROBE_RESPONSE_IE)
		return -1;

	siTotalEss = wps_get_ess_num();
	for (siEssID = 0; siEssID < siTotalEss; siEssID++) {
		snprintf(acNVName, sizeof(acNVName), "ess%d_wlnames", siEssID);
		pcEssWLs = wps_get_conf(acNVName);
		if (pcEssWLs == NULL || *pcEssWLs == '\0')
			continue;
		if (strstr(pcEssWLs, ptObj->acOSName))
			break;
	}
	if (siEssID >= siTotalEss)
		return -2;

	ptObj->siType = siType;
	if (boolActivate == FALSE)
		ptObj->siType |= WPSM_VNDR_EXT_MODE_INACTIVE;
	else
		ptObj->siType &= ~WPSM_VNDR_EXT_MODE_INACTIVE;

	/* It doesn't work any more, need to revisit. */
	if (siType == WPS_IE_TYPE_SET_PROBE_RESPONSE_IE)
		wps_set_prb_rsp_IE(siEssID, ptObj->acOSName, FALSE);
	else if (siType == WPS_IE_TYPE_SET_BEACON_IE)
		wps_set_beacon_IE(siEssID, ptObj->acOSName, FALSE);

	return 1;
}
#endif /* __CONFIG_WFI__ */
