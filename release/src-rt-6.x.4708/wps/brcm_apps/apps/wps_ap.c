/*
 * WPS AP thread (Platform independent portion)
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_ap.c 402957 2013-05-17 08:58:04Z $
 */

#include <stdio.h>
#include <unistd.h>

#ifdef __ECOS
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#endif

#include <netinet/in.h>
#include <net/if.h>

#include <bn.h>
#include <wps_dh.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <ap_ssr.h>
#include <wps_ap.h>
#include <sminfo.h>
#include <wpserror.h>
#include <portability.h>
#include <reg_proto.h>
#include <statemachine.h>

#include <tutrace.h>

#include <ctype.h>
#include <time.h>
#include <wps_wl.h>
#include <wpsapi.h>
#include <wlif_utils.h>
#include <shutils.h>
#include <wps_apapi.h>
#include <ap_eap_sm.h>
#ifdef WPS_UPNP_DEVICE
#include <ap_upnp_sm.h>
#endif
#include <wps.h>
#include <wps_ui.h>
#include <wps_upnp.h>
#include <wps_ie.h>
#include <wps_aplockdown.h>
#include <wps_pb.h>

#ifdef WPS_NFC_DEVICE
#include <wps_nfc.h>
#endif
static void wpsap_start_WPSReg(wpsap_wksp_t *ap_wksp);

#ifdef _TUDEBUGTRACE
static void
wpsap_dump_config(DevInfo *ap_devinfo)
{
	char *Pchar;
	unsigned char *Puchar;

	/* Mode */
	switch (ap_devinfo->sc_mode) {
	case SCMODE_AP_ENROLLEE:
		Pchar = "AP enrollee";
		break;
	case SCMODE_AP_REGISTRAR:
		Pchar = "Ap Proxy with built-in Registrar";
		break;

	case SCMODE_UNKNOWN:
	default:
		Pchar = "Unknown";
		break;
	}
	TUTRACE((TUTRACE_INFO, "Mode: [%s]\n", Pchar));

	/* Configure Mode */
	TUTRACE((TUTRACE_INFO, "AP %s configured\n",
		(ap_devinfo->scState == 2) ? "already" : "not"));

	/* UUID */
	Puchar = ap_devinfo->uuid;
	TUTRACE((TUTRACE_INFO, "UUID: [0x%02x%02x%02x%02x%02x%02x%02x%02x"
		"%02x%02x%02x%02x%02x%02x%02x%02x]\n",
		Puchar[0], Puchar[1], Puchar[2], Puchar[3], Puchar[4],
		Puchar[5], Puchar[6], Puchar[7], Puchar[8], Puchar[9],
		Puchar[10], Puchar[11], Puchar[12], Puchar[13], Puchar[14], Puchar[15]));

	/* Version */
	TUTRACE((TUTRACE_INFO, "WPS Version: [0x%02X]\n",
		ap_devinfo->version));

	/* Device Nake */
	TUTRACE((TUTRACE_INFO, "Device name: [%s]\n",
		ap_devinfo->deviceName));

	/* Device category */
	TUTRACE((TUTRACE_INFO, "Device category: [%d]\n",
		ap_devinfo->primDeviceCategory));

	/* Device sub category */
	TUTRACE((TUTRACE_INFO, "Device sub category: [%d]\n",
		ap_devinfo->primDeviceSubCategory));

	/* Device OUI */
	TUTRACE((TUTRACE_INFO, "Device OUI: [0x%08X]\n",
		ap_devinfo->primDeviceOui));

	/* Mac address */
	Puchar = ap_devinfo->macAddr;
	TUTRACE((TUTRACE_INFO, "Mac address: [%02x:%02x:%02x:%02x:%02x:%02x]\n",
		Puchar[0], Puchar[1], Puchar[2], Puchar[3], Puchar[4], Puchar[5]));

	/* MANUFACTURER */
	TUTRACE((TUTRACE_INFO, "Manufacturer name: [%s]\n",
		ap_devinfo->manufacturer));

	/* MODEL_NAME */
	TUTRACE((TUTRACE_INFO, "Model name: [%s]\n",
		ap_devinfo->modelName));

	/* MODEL_NUMBER */
	TUTRACE((TUTRACE_INFO, "Model number: [%s]\n",
		ap_devinfo->modelNumber));

	/* SERIAL_NUMBER */
	TUTRACE((TUTRACE_INFO, "Serial number: [%s]\n",
		ap_devinfo->serialNumber));

	/* CONFIG_METHODS */
	TUTRACE((TUTRACE_INFO, "Config methods: [0x%04X]\n",
		ap_devinfo->configMethods));

	/* AUTH_TYPE_FLAGS */
	TUTRACE((TUTRACE_INFO, "Auth type flags: [0x%04X]\n",
		ap_devinfo->authTypeFlags));

	/* ENCR_TYPE_FLAGS */
	TUTRACE((TUTRACE_INFO, "Encrypto type flags: [0x%04X]\n",
		ap_devinfo->encrTypeFlags));

	/* CONN_TYPE_FLAGS */
	TUTRACE((TUTRACE_INFO, "Conn type flags: [%d]\n",
		ap_devinfo->connTypeFlags));

	/* RF Band */
	TUTRACE((TUTRACE_INFO, "RF band: [%d]\n",
		ap_devinfo->rfBand));

	/* OS_VER */
	TUTRACE((TUTRACE_INFO, "OS version: [0x%08X]\n",
		ap_devinfo->osVersion));

	/* FEATURE_ID */
	TUTRACE((TUTRACE_INFO, "Feature ID: [0x%08X]\n",
		ap_devinfo->featureId));

	/* SSID */
	TUTRACE((TUTRACE_INFO, "SSID: [%s]\n", ap_devinfo->ssid));

	/* Auth */
	TUTRACE((TUTRACE_INFO, "Auth: [%d]\n", ap_devinfo->auth));

	/* KEY MGMT */
	TUTRACE((TUTRACE_INFO, "Key management: [%s]\n",
		(ap_devinfo->keyMgmt[0]) ? ap_devinfo->keyMgmt : "Open network"));

	/* Network key */
	TUTRACE((TUTRACE_INFO, "Network Key: [%s]\n", ap_devinfo->nwKey));

	/* wep key index */
	TUTRACE((TUTRACE_INFO, "Network Key Index: [%x]\n", ap_devinfo->wepKeyIdx));

	/* Crypto falgs */
	TUTRACE((TUTRACE_INFO, "Crypto flags: [%s]\n",
		((ap_devinfo->crypto & WPS_ENCRTYPE_TKIP) &&
		(ap_devinfo->crypto & WPS_ENCRTYPE_AES)) ? "TKIP|AES" :
		(ap_devinfo->crypto & WPS_ENCRTYPE_TKIP) ? "TKIP" :
		(ap_devinfo->crypto & WPS_ENCRTYPE_AES) ? "AES" : "None"));
}
#endif /* _TUDEBUGTRACE */

#ifdef WPS_ROUTER
int
wps_setWpsIE(void *bcmdev, uint8 scState, uint8 selRegistrar,
	uint16 devPwdId, uint16 selRegCfgMethods, uint8 *authorizedMacs_buf,
	int authorizedMacs_len)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)bcmdev;
	wpsap_wksp_t *wps_wksp = (wpsap_wksp_t *)g_mc->bcmwps;
	char *ifname;
	CTlvSsrIE ssrmsg;
	BufferObj *authorizedMacs_bufObj = NULL;
	uint8 data8;

	if (!wps_wksp)
		return -1;

	ifname = wps_wksp->ifname;

	TUTRACE((TUTRACE_INFO, "Add IE from %s\n", ifname));

	memset(&ssrmsg, 0, sizeof(CTlvSsrIE));

	/* setup default data to ssrmsg */
	ssrmsg.version.m_data = WPS_VERSION;
	ssrmsg.scState.m_data = scState;
	ssrmsg.selReg.m_data = selRegistrar;
	ssrmsg.devPwdId.m_data = devPwdId;
	ssrmsg.selRegCfgMethods.m_data = selRegCfgMethods;

	/* WSC 2.0 */
	data8 = g_mc->dev_info->version2;
	if (data8 >= WPS_VERSION2) {
		ssrmsg.version2.m_data = data8;

		/* Dserialize ssrmsg.authorizedMacs from authorizedMacs_buf */
		if (authorizedMacs_buf && authorizedMacs_len) {
			authorizedMacs_bufObj = buffobj_new();
			if (authorizedMacs_bufObj) {
				/* Serialize authorizedMacs_buf to authorizedMacs_bufObj */
				subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS,
					authorizedMacs_bufObj,
					authorizedMacs_buf,
					authorizedMacs_len);
				buffobj_Rewind(authorizedMacs_bufObj);
				/* De-serialize the authorizedMacs data to get the TLVs */
				subtlv_dserialize(&ssrmsg.authorizedMacs,
					WPS_WFA_SUBID_AUTHORIZED_MACS,
					authorizedMacs_bufObj, 0, 0);
			}
		}
	}

	wps_ie_set(ifname, &ssrmsg);

	/* Free authorizedMacs_bufObj */
	if (authorizedMacs_bufObj)
		buffobj_del(authorizedMacs_bufObj);

	return 0;
}
#else
int
wps_set_wps_ie(void *bcmdev, unsigned char *p_data, int length, unsigned int cmdtype)
{
	wpsap_wksp_t *wps_wksp = (wpsap_wksp_t *)bcmdev;
	char data8;
	char *ifname;
	WPSAPI_T *gp_mc;
	DevInfo *devinfo;

	if (!wps_wksp)
		return -1;

	ifname = wps_wksp->ifname;

	TUTRACE((TUTRACE_INFO, "Add IE from %s\n", ifname));
	wps_wl_set_wps_ie(ifname, p_data, length, cmdtype, OUITYPE_WPS);

	/* Set wep OUI if wep on */
	gp_mc = (WPSAPI_T *)wps_wksp->mc;
	devinfo = gp_mc->dev_info;

	if (devinfo->wep) {
		data8 = 0;
		wps_wl_set_wps_ie(ifname, &data8, 1, cmdtype, OUITYPE_PROVISION_STATIC_WEP);
	}
	else
		wps_wl_del_wps_ie(ifname, cmdtype, OUITYPE_PROVISION_STATIC_WEP);

	return 0;
}
#endif /* WPS_ROUTER */

static int
wpsap_process_msg(wpsap_wksp_t *wps_wksp, char *buf, uint32 len, uint32 msgtype)
{
	int send_len;
	char *sendBuf;
	uint32 retVal = WPS_CONT;

	switch (msgtype) {
	/* EAP */
	case TRANSPORT_TYPE_EAP:
		WPS_HexDumpAscii("<== From EAPOL", -1, buf, len);

		retVal = ap_eap_sm_process_sta_msg(buf, len);

		/* check return code to do more things */
		if (retVal == WPS_SEND_MSG_CONT ||
			retVal == WPS_SEND_MSG_SUCCESS ||
			retVal == WPS_SEND_MSG_ERROR ||
			retVal == WPS_ERR_REGISTRATION_PINFAIL ||
			retVal == WPS_ERR_ENROLLMENT_PINFAIL) {
			send_len = ap_eap_sm_get_msg_to_send(&sendBuf);
			if (send_len >= 0)
				ap_eap_sm_sendMsg(sendBuf, send_len);

			/* WPS_SUCCESS or WPS_MESSAGE_PROCESSING_ERROR case */
			if (retVal == WPS_SEND_MSG_SUCCESS ||
				retVal == WPS_SEND_MSG_ERROR ||
				retVal == WPS_ERR_REGISTRATION_PINFAIL ||
				retVal == WPS_ERR_ENROLLMENT_PINFAIL) {
				/* WSC 2.0, send deauthentication followed by EAP-Fail */
				ap_eap_sm_Failure(wps_wksp->b_wps_version2 ? 1 : 0);
			}

			/* over-write retVal */
			if (retVal == WPS_SEND_MSG_SUCCESS)
				retVal = WPS_SUCCESS;
			else if (retVal == WPS_SEND_MSG_ERROR)
				retVal = WPS_MESSAGE_PROCESSING_ERROR;
			else if (retVal == WPS_ERR_REGISTRATION_PINFAIL) {
				wps_ui_set_env("wps_pinfail", "1");
				wps_set_conf("wps_pinfail", "1");
			}
			else if (retVal == WPS_ERR_ENROLLMENT_PINFAIL)
				retVal = WPS_ERR_ENROLLMENT_PINFAIL;
			else
				retVal = WPS_CONT;
		}
		/* other error case */
		else if (retVal != WPS_CONT) {
			TUTRACE((TUTRACE_INFO,
				"Prevent EAP-Fail faster than EAP-Done for DTM 1.4 case 913.\n",
				retVal));
			WpsSleepMs(50);

			/* WSC 2.0, send deauthentication followed by EAP-Fail */
			ap_eap_sm_Failure(wps_wksp->b_wps_version2 ? 1 : 0);
		}
		break;

#ifdef WPS_UPNP_DEVICE
	/* UPNP */
	case TRANSPORT_TYPE_UPNP_DEV:
	{
		unsigned long now = (unsigned long)time(0);
		int type;

		wps_upnp_parse_msg(buf, len, NULL, &type, NULL);
		WPS_HexDumpAscii("<== From UPnP", type, buf, len);

		retVal = ap_upnp_sm_process_msg(buf, len, now);

		/* check return code to do more things */
		if (retVal == WPS_SEND_MSG_CONT ||
			retVal == WPS_SEND_MSG_SUCCESS ||
			retVal == WPS_SEND_MSG_ERROR ||
			retVal == WPS_ERR_ENROLLMENT_PINFAIL) {
			send_len = ap_upnp_sm_get_msg_to_send(&sendBuf);
			if (send_len >= 0)
				ap_upnp_sm_sendMsg(sendBuf, send_len);

			/* over-write retVal */
			if (retVal == WPS_SEND_MSG_SUCCESS)
				retVal = WPS_SUCCESS;
			else if (retVal == WPS_SEND_MSG_ERROR)
				retVal = WPS_MESSAGE_PROCESSING_ERROR;
			else if (retVal == WPS_ERR_ENROLLMENT_PINFAIL)
				retVal = WPS_ERR_ENROLLMENT_PINFAIL;
			else
				retVal = WPS_CONT;
		}
		break;
	}
#endif /* WPS_UPNP_DEVICE */
	default:
		break;
	}

	return retVal;
}

static int
wpsap_update_custom_cred(char *ssid, char *key, char *akm, char *crypto, int oob_addenr,
	bool b_wps_Version2)
{
	/*
	 * WSC 1.0 Old design : Because WSC 1.0 did not support Mix mode, in default
	 * we pick WPA2-PSK/AES up in Mix mode.  If NVRAM "wps_mixedmode" is "1"
	 * than change to pick WPA-PSK/TKIP up.
	 */

	/*
	 * WSC 2.0 support Mix mode and says "WPA-PSK and TKIP only allowed in
	 * Mix mode".  So in default we use Mix mode and if  NVRMA "wps_mixedmode"
	 * is "1" than change to pick WPA2-PSK/AES up.
	 */
	int mix_mode = 2;

	if (!strcmp(wps_safe_get_conf("wps_mixedmode"), "1"))
		mix_mode = 1;

	if (!strcmp(akm, "WPA-PSK WPA2-PSK")) {
		if (b_wps_Version2) {
				strcpy(akm, "WPA2-PSK");
		} else {
			if (mix_mode == 1)
				strcpy(akm, "WPA-PSK");
			else
				strcpy(akm, "WPA2-PSK");
		}
		TUTRACE((TUTRACE_INFO, "Update customized Key Mode : %s, ", akm));
	}

	if (!strcmp(crypto, "AES+TKIP")) {
		if (b_wps_Version2) {
				strcpy(crypto, "AES");
		} else {
			if (mix_mode == 1)
				strcpy(crypto, "TKIP");
			else
				strcpy(crypto, "AES");
		}
		TUTRACE((TUTRACE_INFO, "Update customized encrypt mode = %s\n", crypto));
	}

	if (oob_addenr) {
		char *p;

		/* get randomssid */
		if ((p = wps_get_conf("wps_randomssid"))) {
			strncpy(ssid, p, MAX_SSID_LEN);
			TUTRACE((TUTRACE_INFO, "Update customized SSID : %s\n", ssid));
		}

		/* get randomkey */
		if ((p = wps_get_conf("wps_randomkey")) && (strlen (p) >= 8)) {
			strncpy(key, p, SIZE_64_BYTES);
			TUTRACE((TUTRACE_INFO, "Update customized Key : %s\n", key));
		}

		/* Modify the crypto, if the station is legacy */
	}

	return 0;
}

#ifdef WFA_WPS_20_TESTBED
int
new_tlv_convert(uint8 *new_tlv_str, char *nattr_tlv, int *nattr_len)
{
	uchar *src, *dest;
	uchar val;
	int idx, len;
	char hexstr[3];

	/* reset first */
	*nattr_len = 0;

	if (!new_tlv_str || !nattr_tlv)
		return 0;

	/* Ensure in 2 characters long */
	len = strlen((char*)new_tlv_str);
	if (len % 2) {
		printf("Please specify all the data bytes for this TLV\n");
		return -1;
	}
	*nattr_len = (uint8) (len / 2);

	/* string to hex */
	src = new_tlv_str;
	dest = nattr_tlv;
	for (idx = 0; idx < len; idx++) {
		hexstr[0] = src[0];
		hexstr[1] = src[1];
		hexstr[2] = '\0';

		val = (uchar) strtoul(hexstr, NULL, 16);

		*dest++ = val;
		src += 2;
	}

	/* TODO, can add TLV parsing here */
	return 0;
}
#endif /* WFA_WPS_20_TESTBED */

static int
wpsap_readConfigure(wpsap_wksp_t *wps_wksp, DevInfo *ap_devinfo)
{
	char *dev_key = NULL;
	char *value, *next;
	int auth = 0;
	char ssid[SIZE_32_BYTES + 1] = {0};
	char psk[SIZE_64_BYTES + 1] = {0};
	char akmstr[32];
	char key[8];
	unsigned int akm = 0;
	unsigned int wsec = 0;
	int wep_index = 0;			/* wep key index */
	char *wep_key = NULL;			/* user-supplied wep key */
	char dev_akm[64] = {0};
	char dev_crypto[64] = {0};
	bool oob_addenr = 0;
	char prefix[] = "wlXXXXXXXXXX_";
	char tmp[100];

	/* TBD, is going to use osname only */
	sprintf(prefix, "%s_", wps_wksp->ifname);

	if (wps_wksp->sc_mode == SCMODE_UNKNOWN) {
		TUTRACE((TUTRACE_ERR, "Error getting wps config mode\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		wps_wksp->b_wps_version2 = true;
	else
		wps_wksp->b_wps_version2 = false;

	/* raw OOB config state (per-ESS) */
	sprintf(tmp, "ess%d_wps_oob", wps_wksp->ess_id);
	if (!strcmp(wps_safe_get_conf(tmp), "disabled") ||
	    !strcmp(wps_safe_get_conf("wps_oob_configured"), "1"))
		wps_wksp->config_state = WPS_SCSTATE_CONFIGURED;
	else
		wps_wksp->config_state = WPS_SCSTATE_UNCONFIGURED;

	/* Configure Mode sending to peer */
	/* According to WPS 2.0 section "Wi-Fi Simple Configuration State"
	 * Note: The Internal Registrar waits until successful completion of the protocol before
	 * applying the automatically generated credentials to avoid an accidental transition from
	 * "Not Configured" to "Configured" in the case that a neighboring device tries to run WSC
	 */
	ap_devinfo->scState = wps_wksp->config_state;

	/* Operating Mode */
	ap_devinfo->sc_mode = wps_wksp->sc_mode;
	ap_devinfo->b_ap = true;

	/* UUID */
	memcpy(ap_devinfo->uuid, wps_get_uuid(), SIZE_16_BYTES);

	/* Version */
	ap_devinfo->version = WPS_VERSION;

	/* Device Name */
	value = wps_get_conf("wps_device_name");
	if (!value) {
		TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in cfg file, <DEVICE_NAME>\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}
	strcpy(ap_devinfo->deviceName, value);

	/* Device category */
	ap_devinfo->primDeviceCategory = WPS_DEVICE_TYPE_CAT_NW_INFRA;

	/* Device OUI */
	ap_devinfo->primDeviceOui = 0x0050f204;

	/* Device sub category */
	ap_devinfo->primDeviceSubCategory = WPS_DEVICE_TYPE_SUB_CAT_NW_AP;

	/* Mac address */
	memcpy(ap_devinfo->macAddr, wps_wksp->mac_ap, SIZE_6_BYTES);

	/* MANUFACTURER */
	value = wps_get_conf("wps_mfstring");
	if (!value) {
		TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in cfg file, <MANUFACTURER>\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}
	wps_strncpy(ap_devinfo->manufacturer, value, sizeof(ap_devinfo->manufacturer));

	/* MODEL_NAME */
	value = wps_get_conf("wps_modelname");
	if (!value) {
		TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in cfg file, <MODEL_NAME>\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}
	wps_strncpy(ap_devinfo->modelName, value, sizeof(ap_devinfo->modelName));

	/* MODEL_NUMBER */
	value = wps_get_conf("wps_modelnum");
	if (!value) {
		TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in cfg file, <MODEL_NUMBER>\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}
	wps_strncpy(ap_devinfo->modelNumber, value, sizeof(ap_devinfo->modelNumber));

	/* SERIAL_NUMBER */
	value = wps_get_conf("boardnum");
	if (!value) {
		TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in cfg file, <BOARD_NUMBER>\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}
	wps_strncpy(ap_devinfo->serialNumber, value, sizeof(ap_devinfo->serialNumber));

	/* CONFIG_METHODS */
	if (wps_wksp->b_wps_version2) {
		/* WSC 2.0 */
		ap_devinfo->configMethods = (WPS_CONFMET_VIRT_PBC | WPS_CONFMET_PHY_PBC |
			WPS_CONFMET_VIRT_DISPLAY | WPS_CONFMET_PBC | WPS_CONFMET_DISPLAY);
	} else {
		ap_devinfo->configMethods = (WPS_CONFMET_PBC | WPS_CONFMET_DISPLAY);
	}

	value = wps_get_conf("wps_config_method");
	if (value)
		ap_devinfo->configMethods = (uint16)(strtoul(value, NULL, 16));

	/* AUTH_TYPE_FLAGS */
	/* WSC 2.0, WPS-PSK and SHARED are deprecated.
	 * When both the Registrar and the Enrollee are using protocol version 2.0
	 * or newer, this variable can use the value 0x0022 to indicate mixed mode
	 * operation (both WPA-Personal and WPA2-Personal enabled)
	 */
	if (wps_wksp->b_wps_version2) {
		ap_devinfo->authTypeFlags = (uint16)(WPS_AUTHTYPE_OPEN | WPS_AUTHTYPE_WPAPSK |
			WPS_AUTHTYPE_WPA2PSK);
	} else {
		ap_devinfo->authTypeFlags = (uint16)(WPS_AUTHTYPE_OPEN | WPS_AUTHTYPE_WPAPSK |
			WPS_AUTHTYPE_SHARED | WPS_AUTHTYPE_WPA2PSK);
	}

	/* ENCR_TYPE_FLAGS */
	/*
	 * WSC 2.0, deprecated WEP. TKIP can only be advertised on the AP when
	 * Mixed Mode is enabled (Encryption Type is 0x000c)
	 */
	if (wps_wksp->b_wps_version2) {
		ap_devinfo->encrTypeFlags = (uint16)(WPS_ENCRTYPE_NONE | WPS_ENCRTYPE_TKIP |
			WPS_ENCRTYPE_AES);
	} else {
		ap_devinfo->encrTypeFlags = (uint16)(WPS_ENCRTYPE_NONE | WPS_ENCRTYPE_WEP |
			WPS_ENCRTYPE_TKIP | WPS_ENCRTYPE_AES);
	}

	/* CONN_TYPE_FLAGS */
	ap_devinfo->connTypeFlags = WPS_CONNTYPE_ESS;

	/* RF Band */
	/* use ESS's rfBand which used in wps_upnp_create_device_info() instead of wireless */
	if (wps_wksp->pre_nonce) {
		sprintf(tmp, "ess%d_band", wps_wksp->ess_id);
		value = wps_safe_get_conf(tmp);
		ap_devinfo->rfBand = atoi(value);
	} else {
		sprintf(tmp, "%s_band", wps_wksp->ifname);
		value = wps_safe_get_conf(tmp);
		switch (atoi(value)) {
		case WLC_BAND_ALL:
			ap_devinfo->rfBand = WPS_RFBAND_24GHZ | WPS_RFBAND_50GHZ;
			break;
		case WLC_BAND_5G:
			ap_devinfo->rfBand = WPS_RFBAND_50GHZ;
			break;
		case WLC_BAND_2G:
		default:
			ap_devinfo->rfBand = WPS_RFBAND_24GHZ;
			break;
		}
	}

	/* OS_VER */
	ap_devinfo->osVersion = 0x80000000;

	/* FEATURE_ID */
	ap_devinfo->featureId = 0x80000000;

	/* Auth */
	if (!strcmp(strcat_r(prefix, "auth", tmp), "1"))
		auth = 1;

	ap_devinfo->auth = auth;

	/* If caller has specified credentials using them */
	if ((ap_devinfo->scState == WPS_SCSTATE_UNCONFIGURED) &&
	    (value = wps_ui_get_env("wps_ssid")) && strcmp(value, "") != 0) {

		/* SSID */
		value = wps_ui_get_env("wps_ssid");
		wps_strncpy(ssid, value, sizeof(ssid));

		/* AKM */
		value = wps_ui_get_env("wps_akm");
		foreach(akmstr, value, next) {
			if (!strcmp(akmstr, "psk"))
				akm |= WPA_AUTH_PSK;
			else if (!strcmp(akmstr, "psk2"))
				akm |= WPA2_AUTH_PSK;
			else {
				TUTRACE((TUTRACE_INFO, "Error in AKM\n"));
				return MC_ERR_CFGFILE_CONTENT;
			}
		}

		/* Crypto */
		if (akm) {
			value = wps_ui_get_env("wps_crypto");
			if (!strcmp(value, "aes"))
				wsec = AES_ENABLED;
			else if (!strcmp(value, "tkip+aes"))
				wsec = TKIP_ENABLED|AES_ENABLED;
			else {
				TUTRACE((TUTRACE_INFO, "Error in crypto\n"));
				return MC_ERR_CFGFILE_CONTENT;
			}

			/* Set PSK key */
			value = wps_ui_get_env("wps_psk");
			if (strlen(value) < 8 || strlen(value) > SIZE_64_BYTES) {
				TUTRACE((TUTRACE_INFO, "Error in crypto\n"));
				return MC_ERR_CFGFILE_CONTENT;
			}
			wps_strncpy(psk, value, sizeof(psk));
			dev_key = psk;
		}
	}
	else {

		/*
		 * Before check oob mode, we have to
		 * get ssid, akm, wep, crypto and mgmt key from config.
		 * because oob mode might change the settings.
		 */
		value = wps_safe_get_conf(strcat_r(prefix, "ssid", tmp));
		wps_strncpy(ssid, value, sizeof(ssid));

		value = wps_safe_get_conf(strcat_r(prefix, "akm", tmp));
		foreach(akmstr, value, next) {
			if (!strcmp(akmstr, "psk"))
				akm |= WPA_AUTH_PSK;

			if (!strcmp(akmstr, "psk2"))
				akm |= WPA2_AUTH_PSK;
		}

		value = wps_safe_get_conf(strcat_r(prefix, "wep", tmp));
		wsec = !strcmp(value, "enabled") ? WEP_ENABLED : 0;

		value = wps_safe_get_conf(strcat_r(prefix, "crypto", tmp));
		if (WPS_WLAKM_PSK(akm) || WPS_WLAKM_PSK2(akm)) {
			if (!strcmp(value, "tkip"))
				wsec |= TKIP_ENABLED;
			else if (!strcmp(value, "aes"))
				wsec |= AES_ENABLED;
			else if (!strcmp(value, "tkip+aes"))
				wsec |= TKIP_ENABLED|AES_ENABLED;

			/* Set PSK key */
			value = wps_safe_get_conf(strcat_r(prefix, "wpa_psk", tmp));
			wps_strncpy(psk, value, sizeof(psk));
		}

		if (wsec & WEP_ENABLED) {
			/* Key index */
			value = wps_safe_get_conf(strcat_r(prefix, "key", tmp));
			wep_index = (int)strtoul(value, NULL, 0);

			/* Key */
			sprintf(key, "key%s", value);
			wep_key = wps_safe_get_conf(strcat_r(prefix, key, tmp));
		}

		if (wps_wksp->config_state == WPS_SCSTATE_UNCONFIGURED &&
		    wps_wksp->sc_mode == SCMODE_AP_REGISTRAR) {
			oob_addenr = 1;
		}

		/* Caution: oob_addenr will over-write akm and wsec */
		if (oob_addenr) {
			/* Generate random ssid and key */
			if (wps_gen_ssid(ssid, sizeof(ssid),
				wps_get_conf("wps_random_ssid_prefix"),
				wps_safe_get_conf("eth1_hwaddr")) == FALSE ||
			    wps_gen_key(psk, sizeof(psk)) == FALSE)
				return WPS_ERR_SYSTEM;

			/* Open */
			auth = 0;

			/* PSK, PSK2 */
			akm = WPA_AUTH_PSK | WPA2_AUTH_PSK;
			wsec = AES_ENABLED;
		}

		/*
		 * Let the user have a chance to override the credential.
		 */
		if (WPS_WLAKM_BOTH(akm))
			strcpy(dev_akm, "WPA-PSK WPA2-PSK");
		else if (WPS_WLAKM_PSK(akm))
			strcpy(dev_akm, "WPA-PSK");
		else if (WPS_WLAKM_PSK2(akm))
			strcpy(dev_akm, "WPA2-PSK");
		else
			dev_akm[0] = 0;

		/* Encryption algorithm */
		if (WPS_WLENCR_BOTH(wsec))
			strcpy(dev_crypto, "AES+TKIP");
		else if (WPS_WLENCR_TKIP(wsec))
			strcpy(dev_crypto, "TKIP");
		else if (WPS_WLENCR_AES(wsec))
			strcpy(dev_crypto, "AES");
		else
			dev_crypto[0] = 0;

		/* Do customization, and check credentials again */
		wpsap_update_custom_cred(ssid, psk, dev_akm, dev_crypto, oob_addenr,
			wps_wksp->b_wps_version2);

		/* Parsing return akm and crypto */
		if (strlen(dev_akm)) {
			if (!strcmp(dev_akm, "WPA-PSK WPA2-PSK"))
				akm = WPA_AUTH_PSK | WPA2_AUTH_PSK;
			else if (!strcmp(dev_akm, "WPA-PSK"))
				akm = WPA_AUTH_PSK;
			else if (!strcmp(dev_akm, "WPA2-PSK"))
				akm = WPA2_AUTH_PSK;
		}
		if (strlen(dev_crypto)) {
			if (!strcmp(dev_crypto, "AES+TKIP"))
				wsec = AES_ENABLED | TKIP_ENABLED;
			else if (!strcmp(dev_crypto, "AES"))
				wsec = AES_ENABLED;
			else if (!strcmp(dev_crypto, "TKIP"))
				wsec = TKIP_ENABLED;
		}
	}

	/*
	 * After doing customized credentials modification,
	 * fill ssid, psk, akm and crypto to ap_deviceinfo
	 */
	strcpy(ap_devinfo->ssid, ssid);

	/* KEY MGMT */
	/* WSC 2.0, deprecated SHARED */
	if (auth) {
		strcpy(ap_devinfo->keyMgmt, "SHARED");
		if (wps_wksp->b_wps_version2 &&
		    (wps_wksp->config_state == WPS_SCSTATE_CONFIGURED)) {
			TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in configuration,"
				"Authentication type is Shared, violate WSC 2.0\n"));
			return MC_ERR_CFGFILE_CONTENT;
		}
	}
	else {
		if (WPS_WLAKM_BOTH(akm))
			strcpy(ap_devinfo->keyMgmt, "WPA-PSK WPA2-PSK");
		else if (WPS_WLAKM_PSK(akm))
			strcpy(ap_devinfo->keyMgmt, "WPA-PSK");
		else if (WPS_WLAKM_PSK2(akm))
			strcpy(ap_devinfo->keyMgmt, "WPA2-PSK");
		else
			ap_devinfo->keyMgmt[0] = 0;
	}

	/* WEP index */
	ap_devinfo->wep = (wsec & WEP_ENABLED) ? 1 : 0;

	/* AKM has higher priority than WEP */
	if (WPS_WLAKM_NONE(akm) && ap_devinfo->wep) {
		/* Get wps key index */
		ap_devinfo->wepKeyIdx = wep_index;

		/* Get wps key content */
		dev_key = wep_key;

		if (wps_wksp->b_wps_version2 &&
		    (wps_wksp->config_state == WPS_SCSTATE_CONFIGURED)) {
			TUTRACE((TUTRACE_INFO,
				"Error in configuration, WEP is enabled violate WSC 2.0\n"));
			return MC_ERR_CFGFILE_CONTENT;
		}
	}
	else if (!WPS_WLAKM_NONE(akm)) {
		dev_key = psk;

		if ((wsec & (AES_ENABLED | TKIP_ENABLED)) == 0) {
			TUTRACE((TUTRACE_INFO, "Error in crypto\n"));
			return MC_ERR_CFGFILE_CONTENT;
		}
		if (wps_wksp->b_wps_version2 &&
		    (wps_wksp->config_state == WPS_SCSTATE_CONFIGURED)) {
			/* WSC 2.0, WPS-PSK is allowed only in mix mode */
			if ((akm & WPA_AUTH_PSK) && !(akm & WPA2_AUTH_PSK)) {
				TUTRACE((TUTRACE_ERR, "Error in configuration,"
					" WPA-PSK enabled without WPA2-PSK,"
					" violate WSC 2.0\n"));
				return MC_ERR_CFGFILE_CONTENT;
			}
		}
	}

	/* Network key,  it may WEP key or Passphrase key */
	memset(ap_devinfo->nwKey, 0, SIZE_64_BYTES);
	if (dev_key)
		wps_strncpy(ap_devinfo->nwKey, dev_key, sizeof(ap_devinfo->nwKey));

	/* fill in prebuild enrollee nonce and private key */
	memset(&ap_devinfo->pre_nonce, 0, SIZE_128_BITS);
	if (wps_wksp->pre_nonce) {
		/* copy prebuild enrollee nonce */
		memcpy(ap_devinfo->pre_nonce, wps_wksp->pre_nonce, SIZE_128_BITS);
		ap_devinfo->flags |= DEVINFO_FLAG_PRE_NONCE;
		TUTRACE((TUTRACE_INFO, "wpsap is triggered by UPnP PMR message\n"));
	}

	if (wps_wksp->pre_privkey) {
		memcpy(ap_devinfo->pre_privkey, wps_wksp->pre_privkey, SIZE_PUB_KEY);
		ap_devinfo->flags |= DEVINFO_FLAG_PRE_PRIV_KEY;
	}

	/* Set crypto algorithm */
	ap_devinfo->crypto = 0;
	if (wsec & TKIP_ENABLED)
		ap_devinfo->crypto |= WPS_ENCRTYPE_TKIP;
	if (wsec & AES_ENABLED)
		ap_devinfo->crypto |= WPS_ENCRTYPE_AES;

	if (ap_devinfo->crypto == 0)
		ap_devinfo->crypto = WPS_ENCRTYPE_NONE;

	/* WSC 2.0, Set peer Mac address used for create M8ap/M8sta */
	memcpy(ap_devinfo->peerMacAddr, wps_wksp->mac_sta, SIZE_6_BYTES);

	/* Probe Request UUID-E lists */
	wps_pb_get_uuids(ap_devinfo->pbc_uuids, sizeof(ap_devinfo->pbc_uuids));

	/* WSC 2.0 */
	if (wps_wksp->b_wps_version2) {
		/* Version 2 */
		value = wps_get_conf("wps_version2_num");
		ap_devinfo->version2 = (uint8)(strtoul(value, NULL, 16));

		/* Setting Delay Time */
		ap_devinfo->settingsDelayTime = WPS_SETTING_DELAY_TIME_ROUTER;

		/* Request to Enroll */
		ap_devinfo->b_reqToEnroll = wps_wksp->b_reqToEnroll;

		/* Network Key shareable */
		ap_devinfo->b_nwKeyShareable = wps_wksp->b_nwKeyShareable;

		/* Update authorizedMacs */
		if (wps_wksp->sc_mode == SCMODE_AP_REGISTRAR &&
		    wps_wksp->authorizedMacs_len) {
			memcpy(ap_devinfo->authorizedMacs,
				wps_wksp->authorizedMacs,
				wps_wksp->authorizedMacs_len);
			ap_devinfo->authorizedMacs_len = wps_wksp->authorizedMacs_len;
		}
	}

#ifdef WFA_WPS_20_TESTBED
	/* New attribute */
	value = wps_safe_get_conf("wps_nattr");
	new_tlv_convert(value, ap_devinfo->nattr_tlv, &ap_devinfo->nattr_len);

	/* Do zero padding for testing purpose */
	if (strcmp(wps_safe_get_conf("wps_zpadding"), "1") == 0)
		ap_devinfo->b_zpadding = true;
	else
		ap_devinfo->b_zpadding = false;

	/* Multiple Credential Attributes for testing purpose */
	if (strcmp(wps_safe_get_conf("wps_mca"), "1") == 0)
		ap_devinfo->b_mca = true;
	else
		ap_devinfo->b_mca = false;

	/* Dummy SSID */
	strcpy(ap_devinfo->dummy_ssid, "DUMMY SSID");

	/* EAP Fragment threshold */
	value = wps_safe_get_conf("wps_eap_frag");
	wps_wksp->eap_frag_threshold = atoi(value);
#endif /* WFA_WPS_20_TESTBED */

	/* WPS M1 Pairing Auth */
	ap_devinfo->pairing_auth_str = wps_get_conf("wps_pairing_auth");

#ifdef _TUDEBUGTRACE
	wpsap_dump_config(ap_devinfo);
#endif
	return WPS_SUCCESS;
}

static int
wpsap_wksp_init(wpsap_wksp_t *ap_wksp)
{
	time_t now;
#ifdef WPS_UPNP_DEVICE
	char tmp[100];
	int ap_index;
#endif
	DevInfo ap_devinfo = {0};
	char null_mac[6] = {0};
	int ret;

	/* !!! shold be removed */
	wps_set_ifname(ap_wksp->ifname);

	TUTRACE((TUTRACE_INFO, "wpsap_wksp_init: init %s\n", ap_wksp->ifname));

	/* workspace init time */
	time(&now);
	ap_wksp->start_time = now;
	ap_wksp->touch_time = now;

	/* read device config here and pass to wps_init */
	if (wpsap_readConfigure(ap_wksp, &ap_devinfo) != WPS_SUCCESS) {
		TUTRACE((TUTRACE_INFO, "wpsap_readConfigure fail...\n"));
		return WPS_ERR_SYSTEM;
	}

	if ((ap_wksp->mc = (void*)wps_init(ap_wksp, &ap_devinfo)) == NULL) {
		TUTRACE((TUTRACE_INFO, "wps_init fail...\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	/* Get PIN */
	if (ap_wksp->sc_mode == SCMODE_AP_ENROLLEE) {
		char *dev_pin;
		uint16 devicepwid = 0;

		dev_pin = wps_safe_get_conf("wps_device_pin");
		TUTRACE((TUTRACE_INFO, "init ap pin = %s\n", dev_pin));

#ifdef WPS_NFC_DEVICE
		/* Leverage wps_stareg_ap_pin */
		if (strcmp(wps_ui_get_env("wps_stareg_ap_pin"), "NFC_PW") == 0) {
			dev_pin = wps_nfc_dev_pw_str();
			devicepwid = wps_nfc_pw_id();
		}
#endif
		ret = wpsap_start_enrollment_devpwid(ap_wksp->mc, dev_pin, devicepwid);
	}
	else {
		char *sta_pin;
		uint8 *pub_key_hash = NULL;
		uint16 devicepwid = 0;

		sta_pin = wps_ui_get_env("wps_sta_pin");
		TUTRACE((TUTRACE_INFO, "init sta pin = %s\n", sta_pin));

#ifdef WPS_NFC_DEVICE
		if (strcmp(sta_pin, "NFC_PW") == 0) {
			sta_pin = wps_nfc_dev_pw_str();
			devicepwid = wps_nfc_pw_id();
			pub_key_hash = wps_nfc_pub_key_hash();
		}
#endif
		ret = wpsap_start_registration_devpwid(ap_wksp->mc, sta_pin, pub_key_hash,
			devicepwid);
	}

	if (ret != WPS_SUCCESS)
		return ret;

	/* Start eap/upnp state machine */
	if (ap_eap_sm_init(ap_wksp->mc, ap_wksp->mac_sta, wps_eapol_parse_msg,
		wps_eapol_send_data, ap_wksp->eap_frag_threshold) != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "ap_eap_sm_init fail...\n"));
		return WPS_ERR_SYSTEM;
	}

	TUTRACE((TUTRACE_INFO, "ap_eap_sm_init successful...\n"));

#ifdef WPS_UPNP_DEVICE
	if ((ap_wksp->sc_mode == SCMODE_AP_ENROLLEE) ||
	    (ap_wksp->sc_mode == SCMODE_AP_REGISTRAR)) {
		sprintf(tmp, "ess%d_ap_index", ap_wksp->ess_id);
		ap_index = atoi(wps_safe_get_conf(tmp));

		if (ap_upnp_sm_init(ap_wksp->mc,
			(void *)wps_upnp_update_init_wlan_event,
			(void *)wps_upnp_update_wlan_event,
			(void *)wps_upnp_send_msg,
			(void *)wps_upnp_parse_msg,
			ap_index) != WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "ap_upnp_sm_init fail...\n"));
			return WPS_ERR_SYSTEM;
		}

		TUTRACE((TUTRACE_INFO, "ap_upnp_sm_init lan%d successful...\n", ap_index));
	}
#endif /* WPS_UPNP_DEVICE */

	/*
	 * Send out 'EAP-Request(Start) message', because we've already
	 * received 'EAP-Response/Identity' packet in monitor.
	 */
	if (memcmp(ap_wksp->mac_sta, null_mac, 6) != 0) {
		wpsap_start_WPSReg(ap_wksp);
	}

	TUTRACE((TUTRACE_INFO, "wpsap init done\n"));
	return WPS_SUCCESS;
}

static void
wpsap_wksp_deinit(wpsap_wksp_t *wps_wksp)
{
	if (wps_wksp) {
		ap_eap_sm_deinit();
#ifdef WPS_UPNP_DEVICE
		ap_upnp_sm_deinit();
#endif

		if (wps_wksp->mc) {
			wps_deinit(wps_wksp->mc);
		}

		free(wps_wksp);
	}

	return;
}

static int
wpsap_time_check(wpsap_wksp_t *wps_wksp, unsigned long now)
{
	int procstate = 0;
	int rc;

	/* check overall timeout first */
	if ((now < wps_wksp->start_time) ||
		((now - wps_wksp->start_time) > WPS_OVERALL_TIMEOUT)) {
		return WPS_RESULT_PROCESS_TIMEOUT;
	}

	if (now < wps_wksp->touch_time) {
		wps_wksp->touch_time = now;
	}

	/* check eap sm message timeout */
	if ((rc = ap_eap_sm_check_timer(now)) == EAP_TIMEOUT) {
		return WPS_RESULT_PROCESS_TIMEOUT;
	}

	switch (wps_wksp->sc_mode) {
	case SCMODE_AP_ENROLLEE:
		/* check enr state */
		procstate = wps_getenrState(wps_wksp->mc);
		if (procstate == 0)
			break;
		if (procstate != wps_wksp->wps_state) {
			/* state change, update the last time */
			wps_wksp->wps_state = procstate;
			wps_wksp->touch_time = now;
		}
		else {
			if (((now - wps_wksp->touch_time) > WPS_MSG_TIMEOUT) &&
				((procstate != MNONE && procstate != M2D) &&
				(procstate >= M2))) {
				WPSAPI_T *g_mc = wps_wksp->mc;
				EnrSM *e = g_mc->mp_enrSM;

				/* reset the SM */
				enr_sm_restartSM(e);

				wps_wksp->touch_time = now;
			}
		}
		break;

	case SCMODE_AP_REGISTRAR:
		/* check reg state */
		procstate = wps_getregState(wps_wksp->mc);
		if (procstate == 0)
			break;

		if (procstate != wps_wksp->wps_state) {
			/* state change, update the last time */
			wps_wksp->wps_state = procstate;
			wps_wksp->touch_time = now;
		}
		else {
			if (((now - wps_wksp->touch_time) > WPS_MSG_TIMEOUT) &&
				((procstate != MNONE && procstate != M2D) &&
				(procstate >= M2))) {
				return WPS_RESULT_PROCESS_TIMEOUT;
			}
		}

		/* reset the SM, we need to handle AP send M2D */
		if (rc == WPS_ERR_M2D_TIMEOUT && procstate == M2D) {
			WPSAPI_T *g_mc = wps_wksp->mc;
			RegSM *r = g_mc->mp_regSM;

			/* AP stay in pending when PBC pushed, if AP detected PBC overlap from
			 * the M1 UUID-E mismatch case, it will just send M2D instead M2 and stop
			 * WPS session immediately.  Just return  WPS_RESULT_PROCESS_TIMEOUT
			 * caller will handle it
			 */
			if (r->err_code == WPS_ERR_PBC_OVERLAP)
				return WPS_RESULT_PROCESS_TIMEOUT;

			reg_sm_restartsm(r);
		}
		break;
	default:
		break;
	}

	return WPS_CONT;
}

static int
wpsap_close_session(wpsap_wksp_t *wps_wksp, int opcode)
{
	WPSAPI_T *g_mc = wps_wksp->mc;
	int sc_mode = wps_wksp->sc_mode;
	int restart = 0;
	uint16 mgmt_data;
	char tmp[100];

	if (opcode == WPS_SUCCESS) {
		switch (sc_mode) {
		case SCMODE_AP_ENROLLEE:
		{
			/*
			 * AP (Enrollee) must have just completed initial setup
			 * p_encrSettings contain CTlvEsM8Ap
			 */
			EsM8Ap *p_tlvEncr;
			char *cp_data, *nwKey;
			CTlvNwKey *p_tlvKey;
			WpsEnrCred credential;

			/* Cleanup ap lockdown */
			wps_aplockdown_cleanup();

			memset(&credential, 0, sizeof(credential));

			TUTRACE((TUTRACE_INFO, "Entering SCMODE_AP_ENROLLEE\n"));

			p_tlvEncr = (EsM8Ap *)g_mc->mp_enrSM->mp_peerEncrSettings;
			if (!p_tlvEncr) {
				TUTRACE((TUTRACE_ERR, "Peer Encr Settings not exist\n"));
				break;
			}

			/* Fill in SSID */
			cp_data = (char *)(p_tlvEncr->ssid.m_data);
			credential.ssidLen = p_tlvEncr->ssid.tlvbase.m_len;
			strncpy(credential.ssid, cp_data, credential.ssidLen);
			credential.ssid[credential.ssidLen] = '\0';
			TUTRACE((TUTRACE_INFO, "Get SSID = %s\n", credential.ssid));

			/* Fill in KeyMgmt from the encrSettings TLV */
			mgmt_data = p_tlvEncr->authType.m_data;
			if (mgmt_data == WPS_AUTHTYPE_SHARED) {
				strcpy(credential.keyMgmt, "SHARED");
				if (wps_wksp->b_wps_version2) {
					/* WSC 2.0, deprecated "Shared" authentication */
					TUTRACE((TUTRACE_INFO, "Shared authentication is not "
						"acceptable by WSC 2.0\n"));
					break; /* Failed */
				}
			}
			else  if (mgmt_data == WPS_AUTHTYPE_WPAPSK) {
				strcpy(credential.keyMgmt, "WPA-PSK");
				if (wps_wksp->b_wps_version2) {
					/* WSC 2.0, deprecated "WPA-PSK" only */
					TUTRACE((TUTRACE_INFO, "WPA-PSK only is not acceptable "
						"by WSC 2.0, force to Mixed mode.\n"));
					/*
					 * WSC 2.0, section "9. Security Configuration Requirements"
					 * mentions "Registrars supporting WSC version 1.0 only,
					 * might configure an AP for WPA-Personal. In this scenario,
					 * the AP shall enable Mixed Mode instead"
					 */
					/* Test Plan 2.0 case 4.1.12 */
					strcpy(credential.keyMgmt, "WPA-PSK WPA2-PSK");
				}
			}
			else if (mgmt_data == WPS_AUTHTYPE_WPA2PSK)
				strcpy(credential.keyMgmt, "WPA2-PSK");
			else if (mgmt_data == (WPS_AUTHTYPE_WPAPSK
				| WPS_AUTHTYPE_WPA2PSK))
				strcpy(credential.keyMgmt, "WPA-PSK WPA2-PSK");
			else
				strcpy(credential.keyMgmt, "OPEN");

			TUTRACE((TUTRACE_INFO, "Get Key Mgmt type (%d) is %s\n", mgmt_data,
				credential.keyMgmt));

			/* Get the real cypher */
			credential.encrType = p_tlvEncr->encrType.m_data;
			if (wps_wksp->b_wps_version2) {
				if (credential.encrType == WPS_ENCRTYPE_TKIP) {
					/* WSC 2.0, deprecated "TKIP" only */
					if (strcmp(credential.keyMgmt, "WPA-PSK WPA2-PSK") == 0) {
						TUTRACE((TUTRACE_INFO, "TKIP only is not "
							"acceptable by WSC 2.0, force to "
							"Mixed mode.\n"));
						credential.encrType |= WPS_ENCRTYPE_AES;
					}
					else {
						/* Add in PF# 3 with Win 7 */
						if (strcmp(credential.keyMgmt, "WPA2-PSK") == 0) {
							strcpy(credential.keyMgmt,
								"WPA-PSK WPA2-PSK");
							TUTRACE((TUTRACE_INFO,
								"WPA2-PSK only, force to "
								"Mixed mode.\n"));
							TUTRACE((TUTRACE_INFO, "TKIP only is not "
								"acceptable by WSC 2.0, force to "
								"Mixed mode.\n"));
						}
						else {
							TUTRACE((TUTRACE_INFO, "TKIP only is not "
								"acceptable by WSC 2.0\n"));
							break; /* Failed */
						}
					}
				}
				else if (credential.encrType == WPS_ENCRTYPE_WEP) {
					/* WSC 2.0, deprecated "TKIP" only */
					TUTRACE((TUTRACE_INFO, "WEP is not acceptable "
					"by WSC 2.0\n"));
					break; /* Failed */
				}
			}
			TUTRACE((TUTRACE_INFO, "Get Encr type is %d\n", credential.encrType));

			/* Get wep key index if exist */
			credential.wepIndex = p_tlvEncr->wepIdx.m_data;
			TUTRACE((TUTRACE_INFO, "Get wep key index = %d\n", credential.wepIndex));
			if (credential.encrType == WPS_ENCRTYPE_WEP &&
			    (credential.wepIndex < 1 || credential.wepIndex > 4)) {
				TUTRACE((TUTRACE_ERR, "Peer wep key index range error\n"));
				break;
			}

			/* Fill in psk */
			p_tlvKey = (CTlvNwKey *)p_tlvEncr->nwKey;
			nwKey = (char *)p_tlvKey->m_data;
			TUTRACE((TUTRACE_INFO, "Save PSK info\n"));

			credential.nwKeyLen = p_tlvKey->tlvbase.m_len;
			memcpy(credential.nwKey, nwKey, credential.nwKeyLen);

			/* Apply settings to driver */
			restart = wps_set_wsec(wps_wksp->ess_id, wps_wksp->ifname,
				&credential, sc_mode);

			TUTRACE((TUTRACE_INFO, "wpsap_close_session: Done\n"));
			break;
		}

		case SCMODE_AP_REGISTRAR:
		{
			/* Ckear the wps error state when AP registrar success */
			wps_ui_set_env("wps_pinfail_state", "");
			wps_set_conf("wps_pinfail_state", "");

			if (wps_wksp->config_state == WPS_SCSTATE_UNCONFIGURED) {
				WpsEnrCred credential;
				DevInfo *devinfo = g_mc->dev_info;

				memset(&credential, 0, sizeof(credential));

				/* Fill in SSID */
				wps_strncpy(credential.ssid, devinfo->ssid,
					sizeof(credential.ssid));
				credential.ssidLen = strlen(credential.ssid);
				TUTRACE((TUTRACE_INFO, "ssid = \"%s\", ssid length = %d\n",
					credential.ssid, credential.ssidLen));

				/* Fill in Key */
				credential.nwKeyLen = strlen(devinfo->nwKey);
				memcpy(credential.nwKey, devinfo->nwKey, credential.nwKeyLen);
				TUTRACE((TUTRACE_INFO, "key = \"%s\", key length = %d\n",
					credential.nwKey, credential.nwKeyLen));

				/* Fill in KeyMgmt */
				wps_strncpy(credential.keyMgmt, devinfo->keyMgmt,
					sizeof(credential.keyMgmt));

				/*
				 * Set AES+TKIP in OOB mode, otherwise in WPS test plan 4.2.4
				 * the Broadcom legacy is not able to associate in TKIP
				 */
				credential.encrType = devinfo->crypto;
				credential.encrType |= WPS_ENCRTYPE_TKIP;
				credential.encrType |= WPS_ENCRTYPE_AES;

				TUTRACE((TUTRACE_INFO, "KeyMgmt = %s, encryptType = %x\n",
					credential.keyMgmt, credential.encrType));

				/* Apply settings to driver */
				restart = wps_set_wsec(wps_wksp->ess_id, wps_wksp->ifname,
					&credential, sc_mode);
			}

			ether_etoa(wps_wksp->mac_sta, tmp);
			wps_ui_set_env("wps_sta_mac", tmp);
			wps_set_conf("wps_sta_mac", tmp);

			/*
			 * According to WPS sepc 1.0h Section 10.3 page 79 middle,
			 * Remove enrollee's probe request when Registrar successfully
			 * runs the PBC method
			 */
			if (!strcmp(wps_ui_get_env("wps_sta_pin"), "00000000"))
				wps_pb_clear_sta(wps_wksp->mac_sta);

			break;
		}

		default:
			break;
		}

		wps_ui_set_env("wps_pinfail", "0");
		wps_set_conf("wps_pinfail", "0");

		if (restart)
			wps_wksp->return_code = WPS_RESULT_SUCCESS_RESTART;
		else
			wps_wksp->return_code = WPS_RESULT_SUCCESS;

		wps_setProcessStates(WPS_OK);
	}
	else {
		uint32 err_code = 0;

		if (sc_mode == SCMODE_AP_REGISTRAR)
			err_code = g_mc->mp_regSM->err_code;
		else if (sc_mode == SCMODE_AP_ENROLLEE)
			err_code = g_mc->mp_enrSM->err_code;

		if (err_code == WPS_ERR_PBC_OVERLAP) {
			wps_setProcessStates(WPS_PBCOVERLAP);
			wps_wksp->return_code = WPS_ERR_PBC_OVERLAP;
		} else {
			/* Don't say WPS ERROR when we failed at M2D NACK. The Win7 enrollee
			 * use two phases WPS mechanism to start WPS, the first phase start
			 * registration to retrieve AP's M1 and response M2D then NACK.
			 */
			if (err_code != WPS_M2D_NACK_CONT) {
				wps_setProcessStates(WPS_MSG_ERR);
				wps_wksp->return_code = WPS_RESULT_FAILURE;
			} else {
				wps_wksp->return_code = WPS_RESULT_ENROLLMENT_M2DFAIL;
			}
		}

		if (opcode == WPS_ERR_ENROLLMENT_PINFAIL)
			wps_wksp->return_code = WPS_RESULT_ENROLLMENT_PINFAIL;

		if (opcode == WPS_ERR_REGISTRATION_PINFAIL)
			wps_wksp->return_code = WPS_RESULT_REGISTRATION_PINFAIL;
	}

	return 1;
}

static void
wpsap_start_WPSReg(wpsap_wksp_t *ap_wksp)
{
	char *send_buf;
	int send_buf_len;
	int ret_val = WPS_CONT;

	ret_val = ap_eap_sm_startWPSReg(ap_wksp->mac_sta, ap_wksp->mac_ap);
	if (ret_val == WPS_CONT || ret_val == WPS_SEND_MSG_CONT) {
		/* check return code to do more things */
		if (ret_val == WPS_SEND_MSG_CONT) {
			send_buf_len = ap_eap_sm_get_msg_to_send(&send_buf);
			if (send_buf_len >= 0)
				ap_eap_sm_sendMsg(send_buf, send_buf_len);
		}
	}
}

static int
wpsap_process(wpsap_wksp_t *wps_wksp, char *buf, int len, int msgtype)
{
	int retVal = WPS_CONT;
	wps_wksp->return_code = WPS_CONT;

	retVal = wpsap_process_msg(wps_wksp, buf, len, msgtype);

	/* check the return code and close this session if needed */
	if ((retVal == WPS_SUCCESS) ||
	    (retVal == WPS_MESSAGE_PROCESSING_ERROR) ||
	    (retVal == WPS_ERR_REGISTRATION_PINFAIL) ||
	    (retVal == WPS_ERR_ENROLLMENT_PINFAIL)) {
		/* Close session */
		wpsap_close_session(wps_wksp, retVal);
	}

	return wps_wksp->return_code;
}

static int
wpsap_check_timeout(wpsap_wksp_t *wps_wksp)
{
	WPSAPI_T *g_mc = wps_wksp->mc;
	int sc_mode = wps_wksp->sc_mode;
	unsigned long now = (unsigned long)time(0);

	/*
	 * 1. check timer and do proper action when timeout happened,
	 * like restart wps or something else
	 */
	wps_wksp->return_code = wpsap_time_check(wps_wksp, now);

	if (wps_wksp->return_code == WPS_RESULT_PROCESS_TIMEOUT) {
		if (sc_mode == SCMODE_AP_REGISTRAR) {
			if (g_mc->mp_regSM->err_code == WPS_ERR_PBC_OVERLAP) {
				wps_setProcessStates(WPS_PBCOVERLAP);
				wps_wksp->return_code = WPS_ERR_PBC_OVERLAP;
			} else {
				wps_setProcessStates(WPS_TIMEOUT);
				if (wps_wksp->wps_state == M4 ||
				    wps_wksp->wps_state == M6) {
					wps_setPinFailInfo(wps_wksp->mac_sta,
						wps_ui_get_env("wps_sta_devname"),
						wps_wksp->wps_state == M4 ? "M4" : "M6");
				}
			}
		} else {
			wps_setProcessStates(WPS_TIMEOUT);
		}
	}

	return wps_wksp->return_code;
}

int
wpsap_open_session(wps_app_t *wps_app, int sc_mode, unsigned char *mac, unsigned char *mac_sta,
	char *osifname, char *enr_nonce, char *pre_privkey, uint8 *authorizedMacs,
	uint32 authorizedMacs_len, bool b_reqToEnroll, bool b_nwKeyShareable)
{
	char *wl_mode;

	char tmp[100];
	char *wlnames;
	char ifname[IFNAMSIZ];
	char *next = NULL;
	int i, imax;
	wpsap_wksp_t *ap_wksp;

	if (!osifname || !mac) {
		TUTRACE((TUTRACE_ERR, "Invalid parameter!!\n"));
		return WPS_ERR_OPEN_SESSION;
	}

	/* Save wireless interface name for UI */
	wps_ui_set_env("wps_ifname", osifname);

	sprintf(tmp, "%s_mode", osifname);
	wl_mode = wps_safe_get_conf(tmp);
	if (strcmp(wl_mode, "ap") != 0) {
		TUTRACE((TUTRACE_ERR, "Cannot init an STA interface!!\n"));
		return WPS_ERR_OPEN_SESSION;
	}

	/* Save maximum instance number, and probe if any wl interface */
	imax = wps_get_ess_num();
	for (i = 0; i < imax; i++) {
		sprintf(tmp, "ess%d_wlnames", i);
		wlnames = wps_safe_get_conf(tmp);

		foreach(ifname, wlnames, next) {
			if (!strcmp(ifname, osifname)) {
				goto found;
			}
		}
	}

found:
	if (i == imax) {
		TUTRACE((TUTRACE_ERR, "Can't find upnp instance for %s!!\n", osifname));
		return WPS_ERR_OPEN_SESSION;
	}

	/* Check mode */
	switch (sc_mode) {
	case SCMODE_AP_ENROLLEE:
		/* If AP is under aplockdown mode, disable all configuring to AP */
		if (wps_aplockdown_islocked()) {
			TUTRACE((TUTRACE_INFO, "AP in lock up state, deny AP configuring\n"));
			return WPS_ERR_OPEN_SESSION;
		}
	case SCMODE_AP_REGISTRAR:
		break;
	default:
		TUTRACE((TUTRACE_ERR, "invalid mode parameter(%d)\n", sc_mode));
		return WPS_ERR_OPEN_SESSION;
	}

	/* clear pending flag */
	wps_ui_clear_pending();
#ifdef WPS_UPNP_DEVICE
	wps_upnp_clear_ssr_timer();
#endif

	/* init workspace */
	if ((ap_wksp = (wpsap_wksp_t *)alloc_init(sizeof(wpsap_wksp_t))) == NULL) {
		TUTRACE((TUTRACE_INFO, "Can not allocate memory for wps structer...\n"));
		return WPS_ERR_OPEN_SESSION;
	}

	/* Save variables from ui env */
	ap_wksp->sc_mode = sc_mode;
	ap_wksp->ess_id = i;
	wps_strncpy(ap_wksp->ifname, osifname, sizeof(ap_wksp->ifname));
	memcpy(ap_wksp->mac_ap, mac, SIZE_6_BYTES);
	if (mac_sta)
		memcpy(ap_wksp->mac_sta, mac_sta, SIZE_6_BYTES);
	ap_wksp->pre_nonce = (unsigned char *)enr_nonce;
	ap_wksp->pre_privkey = (unsigned char *)pre_privkey;

	/* WSC 2.0 */
	if (authorizedMacs && authorizedMacs_len) {
		/* For now, only support one authorized mac */
		ap_wksp->authorizedMacs_len = authorizedMacs_len;
		memcpy(ap_wksp->authorizedMacs, authorizedMacs, authorizedMacs_len);
	}
	else {
		/* WSC 2.0 r44, add wildcard MAC when authorized mac not specified */
		ap_wksp->authorizedMacs_len = SIZE_MAC_ADDR;
		memcpy(ap_wksp->authorizedMacs, wildcard_authorizedMacs, SIZE_MAC_ADDR);
	}

	ap_wksp->b_wps_version2 = true; /* Will be updated in wpsap_readConfigure */
	ap_wksp->b_reqToEnroll = b_reqToEnroll;
	ap_wksp->b_nwKeyShareable = b_nwKeyShareable;
	ap_wksp->eap_frag_threshold = EAP_WPS_FRAG_MAX;

	/* update mode */
	TUTRACE((TUTRACE_INFO, "Start %s mode\n", WPS_SMODE2STR(sc_mode)));

	if (wpsap_wksp_init(ap_wksp) != WPS_SUCCESS) {
		wpsap_wksp_deinit(ap_wksp);
		return WPS_ERR_OPEN_SESSION;
	}

	/* Hook to wps_monitor */
	wps_app->wksp = ap_wksp;
	wps_app->sc_mode = sc_mode;
	wps_app->close = (int (*)(void *))wpsap_wksp_deinit;
	wps_app->process = (int (*)(void *, char *, int, int))wpsap_process;
	wps_app->check_timeout = (int (*)(void *))wpsap_check_timeout;

	return WPS_CONT;
}
