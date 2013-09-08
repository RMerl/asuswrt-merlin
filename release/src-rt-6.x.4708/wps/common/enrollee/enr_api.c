/*
 * Enrollee API
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: enr_api.c 383924 2013-02-08 04:14:39Z $
 */

#ifdef WIN32
#include <stdio.h>
#else
#include <string.h>
#endif /* WIN32 */

#include <bn.h>
#include <wps_dh.h>

#include <portability.h>
#include <wpsheaders.h>
#include <wpscommon.h>
#include <sminfo.h>
#include <wpserror.h>
#include <reg_protomsg.h>
#include <reg_proto.h>
#include <reg_prototlv.h>
#include <statemachine.h>
#include <tutrace.h>

#include <info.h>
#include <wpsapi.h>
#include <wps_staeapsm.h>
#include <wps_enrapi.h>
#include <wps_sta.h>
#include <wlioctl.h>

#define WPS_IE_BUF_LEN	VNDR_IE_MAX_LEN * 8	/* 2048 */

static WPSAPI_T *g_mc = NULL;
static uint8 enr_api_wps_iebuf[WPS_IE_BUF_LEN];
static uint32 wpssta_start();
static uint32 wpssta_createM8AP(WPSAPI_T *g_mc);

static WPSAPI_T *
wpssta_init(DevInfo *user_info)
{
	DevInfo *dev_info;

	g_mc = (WPSAPI_T *)alloc_init(sizeof(*g_mc));
	if (!g_mc) {
		TUTRACE((TUTRACE_ERR, "wpssta_init::malloc failed\n"));
		return NULL;
	}

	if ((g_mc->dev_info = devinfo_new()) == NULL)
		goto error;

	/* Setup device info */
	dev_info = g_mc->dev_info;

	memcpy(dev_info, user_info, sizeof(*dev_info));
	dev_info->scState = WPS_SCSTATE_UNCONFIGURED;
	dev_info->b_ap = false;

	/* Prebuild DH key */
	if (dev_info->flags & DEVINFO_FLAG_PRE_PRIV_KEY) {
		if (reg_proto_generate_prebuild_dhkeypair(&dev_info->DHSecret,
			dev_info->pre_privkey) != WPS_SUCCESS)
			goto error;
	} else if (reg_proto_generate_dhkeypair(&dev_info->DHSecret) != WPS_SUCCESS)
		goto error;

	return g_mc;

error:
	TUTRACE((TUTRACE_ERR, "wpssta_init::Init failed\n"));
	if (g_mc) {
		wps_cleanup();
	}

	return NULL;
}


uint32
wpssta_enr_init(DevInfo *user_info)
{
	if (wpssta_init(user_info) == NULL)
		return WPS_ERR_OUTOFMEMORY;

	g_mc->dev_info->sc_mode = SCMODE_STA_ENROLLEE;

	g_mc->mb_initialized = true;

	return WPS_SUCCESS;
}

uint32
wpssta_reg_init(DevInfo *user_info, char *nwKey, char *bssid)
{
	if (wpssta_init(user_info) == NULL)
		return WPS_ERR_OUTOFMEMORY;

	/* update nwKey */
	if (nwKey)
		wps_strncpy(g_mc->dev_info->nwKey, nwKey, sizeof(g_mc->dev_info->nwKey));

	if (bssid)
		memcpy(g_mc->dev_info->peerMacAddr, bssid, SIZE_6_BYTES);

	/* Update sc mode */
	g_mc->dev_info->sc_mode = SCMODE_STA_REGISTRAR;
	if (strlen(g_mc->dev_info->ssid))
		g_mc->dev_info->configap = true;
	else
		g_mc->dev_info->configap = false;

	g_mc->mb_initialized = true;

	return WPS_SUCCESS;
}


/* for enrollee only */
bool
wps_is_wep_incompatible(bool role_reg)
{
	if (g_mc == NULL) {
		TUTRACE((TUTRACE_ERR, "enrApi: not initialization\n"));
		return FALSE;
	}

	if (role_reg) {
		if (g_mc->mp_regSM == NULL) {
			TUTRACE((TUTRACE_ERR, "enrApi: Registrar not initialization\n"));
			return FALSE;
		}

		if (g_mc->mp_regSM->err_code == RPROT_ERR_INCOMPATIBLE_WEP)
			return TRUE;
	}
	else {
		if (g_mc->mp_enrSM == NULL) {
			TUTRACE((TUTRACE_ERR, "enrApi: Enrollee not initialization\n"));
			return FALSE;
		}

		if (g_mc->mp_enrSM->err_code == RPROT_ERR_INCOMPATIBLE_WEP)
			return TRUE;
	}

	return FALSE;
}

/*
 * Name        : DeInit
 * Description : Deinitialize member variables
 * Arguments   : none
 * Return type : uint32
 */
void
wps_cleanup()
{
	if (g_mc) {
		/* Delete enrollee/registrar state machines */
		enr_sm_delete(g_mc->mp_enrSM);
		reg_sm_delete(g_mc->mp_regSM);

		/* Delete local device info */
		devinfo_delete(g_mc->dev_info);

		/* Free context */
		free(g_mc);
		g_mc = NULL;
	}
	TUTRACE((TUTRACE_INFO, "wps_cleanup::Done!\n"));
}

uint32
wpssta_start_enrollment(char *dev_pin, unsigned long time)
{
	DevInfo *dev_info = g_mc->dev_info;
	int devicepwid;

	/* Update device pin */
	if (!dev_pin)
		dev_pin = "00000000";

	if (strcmp(dev_pin, "00000000") == 0)
		devicepwid = WPS_DEVICEPWDID_PUSH_BTN;
	else
		devicepwid = WPS_DEVICEPWDID_DEFAULT;

	dev_info->devPwdId = devicepwid;
	wps_strncpy(dev_info->pin, dev_pin, sizeof(dev_info->pin));

	/* Start protocol */
	return wpssta_start();
}

uint32
wpssta_start_enrollment_devpwid(char *dev_pin, uint16 devicepwid, unsigned long time)
{
	DevInfo *dev_info = g_mc->dev_info;

	/* PBC */
	if (!dev_pin)
		dev_pin = "00000000";

	if (devicepwid == 0) {
		if (strcmp(dev_pin, "00000000") == 0)
			devicepwid = WPS_DEVICEPWDID_PUSH_BTN;
		else
			devicepwid = WPS_DEVICEPWDID_DEFAULT;
	}

	dev_info->devPwdId = devicepwid;
	wps_strncpy(dev_info->pin, dev_pin, sizeof(dev_info->pin));

	/* Start protocol */
	return wpssta_start();
}
uint32
wpssta_start_registration(char *ap_pin, unsigned long time)
{
	DevInfo *dev_info = g_mc->dev_info;

	/* Update AP pin */
	if (!ap_pin) {
		TUTRACE((TUTRACE_ERR, "AP PIN is null, expecting one\n"));
		return WPS_ERR_SYSTEM;
	}

	dev_info->devPwdId = WPS_DEVICEPWDID_DEFAULT;
	wps_strncpy(dev_info->pin, ap_pin, sizeof(dev_info->pin));

	/* Start protocol */
	return wpssta_start();
}

/* ap_pin must String format */
uint32
wpssta_start_registration_devpwid(char *ap_pin, uint8 *pub_key_hash, uint16 devicepwid,
	unsigned long time)
{
	DevInfo *dev_info = g_mc->dev_info;

	/* Update AP pin */
	if (!ap_pin) {
		TUTRACE((TUTRACE_ERR, "AP PIN is null, expecting one\n"));
		return WPS_ERR_SYSTEM;
	}

	if (devicepwid == 0)
		devicepwid = WPS_DEVICEPWDID_DEFAULT;

	if (pub_key_hash)
		memcpy(dev_info->pub_key_hash, pub_key_hash, sizeof(dev_info->pub_key_hash));

	dev_info->devPwdId = devicepwid;
	wps_strncpy(dev_info->pin, ap_pin, sizeof(dev_info->pin));

	/* Start protocol */
	return wpssta_start();
}

void
wpssta_get_credentials(WpsEnrCred* credential, const char *ssid, int len)
{
	EsM8Sta *p_tlvEncr = (EsM8Sta *)(g_mc->mp_enrSM->mp_peerEncrSettings);
	WPS_SSLIST *itr = p_tlvEncr->credential;
	CTlvCredential *p_tlvCred = (CTlvCredential *)itr;
	char *cp_data;
	uint16 data16;
	uint16 mgmt_data;

	if (len > 0 && (itr && itr->next)) {
		/* try to find same ssid */
		for (; (p_tlvCred = (CTlvCredential *)itr); itr = itr->next) {
			if (p_tlvCred->ssid.tlvbase.m_len == len &&
				!strncmp((char*)p_tlvCred->ssid.m_data, ssid, len))
				break;
		}

		/* no anyone match, use first one */
		if (p_tlvCred == NULL) {
			p_tlvCred = (CTlvCredential *)p_tlvEncr->credential;
		}
	}

	if (p_tlvCred == NULL)
		return;


	/* Fill in SSID */
	cp_data = (char *)(p_tlvCred->ssid.m_data);
	data16 = credential->ssidLen = p_tlvCred->ssid.tlvbase.m_len;
	strncpy(credential->ssid, cp_data, data16);
	credential->ssid[data16] = '\0';

	/* Fill in keyMgmt */
	mgmt_data = p_tlvCred->authType.m_data;

	if (mgmt_data == WPS_AUTHTYPE_SHARED) {
		strncpy(credential->keyMgmt, "SHARED", 6);
		credential->keyMgmt[6] = '\0';
	}
	else  if (mgmt_data == WPS_AUTHTYPE_WPAPSK) {
		strncpy(credential->keyMgmt, "WPA-PSK", 7);
		credential->keyMgmt[7] = '\0';
	}
	else if (mgmt_data == WPS_AUTHTYPE_WPA2PSK) {
		strncpy(credential->keyMgmt, "WPA2-PSK", 8);
		credential->keyMgmt[8] = '\0';
	}
	else if (mgmt_data == (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)) {
		strncpy(credential->keyMgmt, "WPA-PSK WPA2-PSK", 16);
		credential->keyMgmt[16] = '\0';
	}
	else {
		strncpy(credential->keyMgmt, "OPEN", 4);
		credential->keyMgmt[4] = '\0';
	}

	/* get the real cypher */
	mgmt_data = p_tlvCred->encrType.m_data;
	TUTRACE((TUTRACE_INFO, "Get Encr type is %d\n", mgmt_data));

	credential->encrType = mgmt_data;

	/* Fill in WEP index, no matter it's WEP type or not */
	credential->wepIndex = p_tlvCred->WEPKeyIndex.m_data;

	/* Fill in PSK */
	data16 = p_tlvCred->nwKey.tlvbase.m_len;
	memset(credential->nwKey, 0, SIZE_64_BYTES);
	memcpy(credential->nwKey, p_tlvCred->nwKey.m_data, data16);
	/* we have to set key length here */
	credential->nwKeyLen = data16;

	/* WSC 2.0,  nwKeyShareable */
	credential->nwKeyShareable = p_tlvCred->nwKeyShareable.m_data;

}

void
wpssta_get_reg_M7credentials(WpsEnrCred* credential)
{
	EsM7Ap *p_tlvEncr = (EsM7Ap *)(g_mc->mp_regSM->mp_peerEncrSettings);
	CTlvNwKey *nwKey;
	char *cp_data;
	uint16 data16;
	uint16 mgmt_data;
	uint16 m7keystr_len, offset;

	if (!p_tlvEncr) {
		TUTRACE((TUTRACE_INFO, "wps_get_reg_M7credentials: No peer Encrypt settings\n"));
		return;
	}
	TUTRACE((TUTRACE_INFO, "wps_get_reg_M7credentials: p_tlvEncr=0x%p\n", p_tlvEncr));

	/* Fill in SSID */
	cp_data = (char *)(p_tlvEncr->ssid.m_data);
	data16 = credential->ssidLen = p_tlvEncr->ssid.tlvbase.m_len;
	strncpy(credential->ssid, cp_data, data16);
	credential->ssid[data16] = '\0';

	/* Fill in keyMgmt */
	mgmt_data = p_tlvEncr->authType.m_data;

	if (mgmt_data == WPS_AUTHTYPE_SHARED) {
		strncpy(credential->keyMgmt, "SHARED", 6);
		credential->keyMgmt[6] = '\0';
	}
	else  if (mgmt_data == WPS_AUTHTYPE_WPAPSK) {
		strncpy(credential->keyMgmt, "WPA-PSK", 7);
		credential->keyMgmt[7] = '\0';
	}
	else if (mgmt_data == WPS_AUTHTYPE_WPA2PSK) {
		strncpy(credential->keyMgmt, "WPA2-PSK", 8);
		credential->keyMgmt[8] = '\0';
	}
	else if (mgmt_data == (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)) {
		strncpy(credential->keyMgmt, "WPA-PSK WPA2-PSK", 16);
		credential->keyMgmt[16] = '\0';
	}
	else {
		strncpy(credential->keyMgmt, "OPEN", 4);
		credential->keyMgmt[4] = '\0';
	}

	/* get the real cypher */

	mgmt_data = p_tlvEncr->encrType.m_data;
	TUTRACE((TUTRACE_INFO, "Get Encr type is %d\n", mgmt_data));

	credential->encrType = mgmt_data;

	/* Fill in WEP index, no matter it's WEP type or not */
	credential->wepIndex = p_tlvEncr->wepIdx.m_data;

	TUTRACE((TUTRACE_INFO, "wps_get_reg_credentials: p_tlvEncr->nwKey=0x%p\n",
		p_tlvEncr->nwKey));
	/* Fill in PSK */
	nwKey = (CTlvNwKey *)p_tlvEncr->nwKey;
	TUTRACE((TUTRACE_INFO, "wps_get_reg_credentials: nwKey=0x%p\n", nwKey));

	data16 = nwKey->tlvbase.m_len;
	memset(credential->nwKey, 0, SIZE_64_BYTES);

	if (nwKey->m_data && nwKey->tlvbase.m_len) {
		memcpy(credential->nwKey, nwKey->m_data, data16);

		m7keystr_len = (uint16)strlen(nwKey->m_data);
		if (m7keystr_len < data16) {
			for (offset = m7keystr_len; offset < data16; offset++) {
				if (nwKey->m_data[offset] != 0)
					break;
			}

			if (offset == data16) {
				/* Adjust nwKeyLen */
				nwKey->tlvbase.m_len = m7keystr_len;
				data16 = m7keystr_len;
			}
		}
	}

	/* we have to set key length here */
	credential->nwKeyLen = data16;
}

void
wpssta_get_reg_M8credentials(WpsEnrCred* credential)
{
	EsM8Ap *p_tlvEncr = (EsM8Ap *)
		(g_mc->mp_regSM->reg_info->dev_info->mp_tlvEsM8Ap);
	CTlvNwKey *nwKey;
	char *cp_data;
	uint16 data16;
	uint16 mgmt_data;

	if (!p_tlvEncr)
		return;

	/* Fill in SSID */
	cp_data = (char *)(p_tlvEncr->ssid.m_data);
	data16 = credential->ssidLen = p_tlvEncr->ssid.tlvbase.m_len;
	strncpy(credential->ssid, cp_data, data16);
	credential->ssid[data16] = '\0';

	/* Fill in keyMgmt */
	mgmt_data = p_tlvEncr->authType.m_data;

	if (mgmt_data == WPS_AUTHTYPE_SHARED) {
		strncpy(credential->keyMgmt, "SHARED", 6);
		credential->keyMgmt[6] = '\0';
	}
	else  if (mgmt_data == WPS_AUTHTYPE_WPAPSK) {
		strncpy(credential->keyMgmt, "WPA-PSK", 7);
		credential->keyMgmt[7] = '\0';
	}
	else if (mgmt_data == WPS_AUTHTYPE_WPA2PSK) {
		strncpy(credential->keyMgmt, "WPA2-PSK", 8);
		credential->keyMgmt[8] = '\0';
	}
	else if (mgmt_data == (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)) {
		strncpy(credential->keyMgmt, "WPA-PSK WPA2-PSK", 16);
		credential->keyMgmt[16] = '\0';
	}
	else {
		strncpy(credential->keyMgmt, "OPEN", 4);
		credential->keyMgmt[4] = '\0';
	}

	/* get the real cypher */
	mgmt_data = p_tlvEncr->encrType.m_data;
	TUTRACE((TUTRACE_INFO, "Get Encr type is %d\n", mgmt_data));

	credential->encrType = mgmt_data;

	/* Fill in WEP index, no matter it's WEP type or not */
	credential->wepIndex = p_tlvEncr->wepIdx.m_data;

	TUTRACE((TUTRACE_INFO, "wps_get_reg_credentials: p_tlvEncr->nwKey=0x%p\n",
		p_tlvEncr->nwKey));

	/* Fill in PSK */
	nwKey = (CTlvNwKey *)p_tlvEncr->nwKey;
	TUTRACE((TUTRACE_INFO, "wps_get_reg_credentials: nwKey=0x%p\n", nwKey));

	data16 = nwKey->tlvbase.m_len;
	memset(credential->nwKey, 0, SIZE_64_BYTES);

	if (nwKey->m_data && nwKey->tlvbase.m_len)
		memcpy(credential->nwKey, nwKey->m_data, data16);

	/* we have to set key length here */
	credential->nwKeyLen = data16;
}

bool
wps_validateChecksum(IN unsigned long int PIN)
{
	unsigned long int accum = 0;
	accum += 3 * ((PIN / 10000000) % 10);
	accum += 1 * ((PIN / 1000000) % 10);
	accum += 3 * ((PIN / 100000) % 10);
	accum += 1 * ((PIN / 10000) % 10);
	accum += 3 * ((PIN / 1000) % 10);
	accum += 1 * ((PIN / 100) % 10);
	accum += 3 * ((PIN / 10) % 10);
	accum += 1 * ((PIN / 1) % 10);

	return ((accum % 10) == 0);
}

static uint32
wpssta_createM8AP(WPSAPI_T *g_mc)
{
	char *cp_data;
	uint16 data16;
	uint32 nwKeyLen = 0;
	uint8 *p_macAddr;
	CTlvNwKey *p_tlvKey;
	uint8 wep_exist = 0;
	DevInfo *info = g_mc->dev_info;
	EsM8Ap *es;

	/*
	 * Create the Encrypted Settings TLV for AP config
	 * Also store this info locally so it can be used
	 * when registration completes for reconfiguration.
	 */
	if (info->mp_tlvEsM8Ap) {
		reg_msg_es_del(info->mp_tlvEsM8Ap, 0);
	}
	info->mp_tlvEsM8Ap = (EsM8Ap *)reg_msg_es_new(ES_TYPE_M8AP);
	if (!info->mp_tlvEsM8Ap)
		return WPS_ERR_SYSTEM;

	es = info->mp_tlvEsM8Ap;

	/* Use the SSID from the config file */
	cp_data = info->ssid;
	data16 = strlen(cp_data);
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	if (info->b_zpadding)
		data16 ++;
#endif /* WFA_WPS_20_TESTBED */
	tlv_set(&es->ssid, WPS_ID_SSID, (uint8 *)cp_data, data16);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: ssid=%s\n", cp_data));

	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: do REAL sec config here...\n"));

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
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: Auth type = 0x%x...\n", data16));

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
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: encr type = 0x%x...\n", data16));

	if (data16 == WPS_ENCRTYPE_WEP)
		wep_exist = 1;

	/* nwKey */
	p_tlvKey = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
	if (!p_tlvKey)
		return WPS_ERR_SYSTEM;

	cp_data = info->nwKey;
	nwKeyLen = strlen(cp_data);

#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	if (info->b_zpadding && cp_data &&
	    nwKeyLen < SIZE_64_BYTES && strlen(cp_data) == nwKeyLen)
		nwKeyLen ++;
#endif /* WFA_WPS_20_TESTBED */
	tlv_set(p_tlvKey, WPS_ID_NW_KEY, cp_data, nwKeyLen);
	if (!wps_sslist_add(&es->nwKey, p_tlvKey)) {
		tlv_delete(p_tlvKey, 0);
		return WPS_ERR_SYSTEM;
	}

	/* macAddr (Enrollee AP's MAC) */
	p_macAddr = info->peerMacAddr;
	data16 = SIZE_MAC_ADDR;
	tlv_set(&es->macAddr, WPS_ID_MAC_ADDR, p_macAddr, data16);

	/* WepKeyIdx */
	if (wep_exist)
		tlv_set(&es->wepIdx, WPS_ID_WEP_TRANSMIT_KEY,
			UINT2PTR(info->wepKeyIdx), 0);

	return WPS_SUCCESS;
}

/* Start the WPS regstration protocol state machine */
static uint32
wpssta_start()
{
	DevInfo *dev_info = g_mc->dev_info;
	uint32 ret = WPS_SUCCESS;


	/* Check if the protocol is started */
	if (!g_mc->mb_initialized) {
		TUTRACE((TUTRACE_INFO, "wpssta_start: Not initialized\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	if (g_mc->mb_stackStarted) {
		TUTRACE((TUTRACE_INFO, "wpssta_start: Already started. Return from M2D\n"));
		/*
		 * Restart Stack to make enrollee or registrar SM restart,
		 * otherwise the enrollee or registrar SM will stay in old state and handle
		 * new message.
		 */
		enr_sm_delete(g_mc->mp_enrSM);
		reg_sm_delete(g_mc->mp_regSM);

		g_mc->mb_stackStarted = false;
	}

	/* Enable state machine */
	switch (g_mc->dev_info->sc_mode) {
	case SCMODE_STA_ENROLLEE:
		TUTRACE((TUTRACE_INFO, "wpssta_start: SCMODE_STA_ENROLLEE enter\n"));
		/* Instantiate the Enrollee SM */
		g_mc->mp_enrSM = enr_sm_new(g_mc);
		if (!g_mc->mp_enrSM) {
			TUTRACE((TUTRACE_ERR,
				"wpssta_start: Could not allocate g_mc->mp_enrSM\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		if (enr_sm_initializeSM(g_mc->mp_enrSM, dev_info) != WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "wpssta_start:Can't init enr_sm\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		/* Start the EAP transport */
		wps_eap_sta_init(NULL, g_mc->mp_enrSM, SCMODE_STA_ENROLLEE);
		TUTRACE((TUTRACE_INFO, "wpssta_start: Exit\n"));
		break;

	case SCMODE_STA_REGISTRAR:
		TUTRACE((TUTRACE_INFO, "wpssta_start: SCMODE_STA_REGISTRAR enter\n"));

		/* Create M8AP */
		ret = wpssta_createM8AP(g_mc);
		if (ret != WPS_SUCCESS)
			return ret;

		/* Instantiate the Registrar SM */
		g_mc->mp_regSM = reg_sm_new(g_mc);
		if (!g_mc->mp_regSM) {
			TUTRACE((TUTRACE_ERR,
				"wpssta_start: Could not allocate g_mc->mp_regSM\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		/* Initialize the Registrar SM */
		if (reg_sm_initsm(g_mc->mp_regSM, dev_info, true, false) != WPS_SUCCESS)
			return WPS_ERR_OUTOFMEMORY;

		/* Fix transportType to EAP */
		g_mc->mp_regSM->reg_info->transportType = TRANSPORT_TYPE_EAP;

		/* Start the EAP transport */
		wps_eap_sta_init(NULL, g_mc->mp_regSM, SCMODE_STA_REGISTRAR);
		break;

	default:
		return WPS_ERR_SYSTEM;
	}

	g_mc->mb_stackStarted = true;

	TUTRACE((TUTRACE_INFO, "wpssta_start: Started\n"));
	return WPS_SUCCESS;
}

/*
 * creates and copy a WPS IE in buff argument.
 * Lenght of the IE is store in buff[0].
 * buff should be a 256 bytes buffer.
 */
uint32
wps_build_probe_req_ie(uint8 *buff, int buflen,  uint8 reqType,
	uint16 assocState, uint16 configError, uint16 passwId)
{
	uint32 ret = WPS_SUCCESS;
	BufferObj *bufObj, *vendorExt_bufObj;
	uint16 data16;
	uint8 data8, *p_data8;
	char *pc_data;
	CTlvPrimDeviceType tlvPrimDeviceType;
	CTlvVendorExt vendorExt;
	DevInfo *info = g_mc->dev_info;

	if (!buff)
		return WPS_ERR_SYSTEM;

	/*  fill in the WPA-WPS OUI  */
	buff[0] = 4;
	buff[1] = 0x00;
	buff[2] = 0x50;
	buff[3] = 0xf2;
	buff[4] = 0x04;

	/*  use oa BufferObj for the rest */
	bufObj = buffobj_setbuf(buff+5, buflen-5);
	if (!bufObj)
		return WPS_ERR_SYSTEM;

	/* Create the IE */
	/* Version */
	data8 = WPS_VERSION;
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Request Type */
	tlv_serialize(WPS_ID_REQ_TYPE, bufObj, &reqType, WPS_ID_REQ_TYPE_S);

	/* Config Methods */
	data16 = info->configMethods;
	tlv_serialize(WPS_ID_CONFIG_METHODS, bufObj, &data16, WPS_ID_CONFIG_METHODS_S);

	/* UUID */
	p_data8 = info->uuid;
	if (WPS_MSGTYPE_REGISTRAR == reqType)
		data16 = WPS_ID_UUID_R;
	else
		data16 = WPS_ID_UUID_E;
	tlv_serialize(data16, bufObj, p_data8, SIZE_UUID);

	/*
	 * Primary Device Type
	 *This is a complex TLV, so will be handled differently
	 */
	tlvPrimDeviceType.categoryId = info->primDeviceCategory;
	tlvPrimDeviceType.oui = info->primDeviceOui;
	tlvPrimDeviceType.subCategoryId = info->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&tlvPrimDeviceType, bufObj);

	/* RF Bands */
	data8 = info->rfBand;
	tlv_serialize(WPS_ID_RF_BAND, bufObj, &data8, WPS_ID_RF_BAND_S);

	/* Association State */
	tlv_serialize(WPS_ID_ASSOC_STATE, bufObj, &assocState, WPS_ID_ASSOC_STATE_S);

	/* Configuration Error */
	tlv_serialize(WPS_ID_CONFIG_ERROR, bufObj, &configError, WPS_ID_CONFIG_ERROR_S);

	/* Device Password ID */
	data16 = passwId;
	tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &data16, WPS_ID_DEVICE_PWD_ID_S);

	/* WSC 2.0 */
	data8 = info->version2;
	if (data8 >= WPS_VERSION2) {
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

		/* Device Name */
		pc_data = info->deviceName;
		data16 = strlen(pc_data);
		tlv_serialize(WPS_ID_DEVICE_NAME, bufObj, pc_data, data16);

		/* Add WFA Vendor Extension subelements */
		vendorExt_bufObj = buffobj_new();
		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		/* Version2 subelement */
		data8 = info->version2;
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj, &data8,
			WPS_WFA_SUBID_VERSION2_S);

		/* Request to Enroll subelement - optional */
		if (info->b_reqToEnroll) {
			data8 = 1;
			subtlv_serialize(WPS_WFA_SUBID_REQ_TO_ENROLL, vendorExt_bufObj, &data8,
				WPS_WFA_SUBID_REQ_TO_ENROLL_S);
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
			buffobj_Append(bufObj, data16, (uint8*)pc_data);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Portable Device - optional */

	/* other Vendor Extension - optional */

	/* store length */
	buff[0] += bufObj->m_dataLength;

	buffobj_del(bufObj);

	return ret;
}

/*
 * creates and copy a WPS IE in buff argument.
 * Lenght of the IE is store in buff[0].
 * buff should be a 256 bytes buffer.
 */
uint32
wps_build_assoc_req_ie(uint8 *buff, int buflen,  uint8 reqType)
{
	uint32 ret = WPS_SUCCESS;
	BufferObj *bufObj, *vendorExt_bufObj;
	uint8 data8;
	CTlvVendorExt vendorExt;
	DevInfo *info = g_mc->dev_info;


	if (!buff)
		return WPS_ERR_SYSTEM;

	/*  fill in the WPA-WPS OUI  */
	buff[0] = 4;
	buff[1] = 0x00;
	buff[2] = 0x50;
	buff[3] = 0xf2;
	buff[4] = 0x04;

	/*  use oa BufferObj for the rest */
	bufObj = buffobj_setbuf(buff+5, buflen-5);
	if (!bufObj)
		return WPS_ERR_SYSTEM;

	/* Create the IE */
	/* Version */
	data8 = WPS_VERSION;
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Request Type */
	tlv_serialize(WPS_ID_REQ_TYPE, bufObj, &reqType, WPS_ID_REQ_TYPE_S);

	/* Add WFA Vendor Extension subelements */
	vendorExt_bufObj = buffobj_new();
	if (vendorExt_bufObj == NULL)
		return WPS_ERR_OUTOFMEMORY;

	/* Version2 subelement */
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
		buffobj_Append(bufObj, data16, (uint8*)pc_data);
}
#endif /* WFA_WPS_20_TESTBED */

	/* Portable Device - optional */

	/* Other Vendor Extension - optional */

	/* store length */
	buff[0] += bufObj->m_dataLength;

	buffobj_del(bufObj);

	return ret;
}

uint32
wps_build_pbc_proberq(uint8 *buff, int len)
{
	return wps_build_probe_req_ie(buff, len, 0, 0, 0, WPS_DEVICEPWDID_PUSH_BTN);
}

uint32
wps_build_def_proberq(uint8 *buff, int len)
{
	return wps_build_probe_req_ie(buff, len, 0, 0, 0, WPS_DEVICEPWDID_DEFAULT);
}

/* WSC 2.0 */
uint32
wps_build_def_assocrq(uint8 *buff, int len)
{
	return wps_build_assoc_req_ie(buff, len, 0);
}

uint32
wps_computeChecksum(IN unsigned long int PIN)
{
	unsigned long int accum = 0;
	int digit;

	PIN *= 10;
	accum += 3 * ((PIN / 10000000) % 10);
	accum += 1 * ((PIN / 1000000) % 10);
	accum += 3 * ((PIN / 100000) % 10);
	accum += 1 * ((PIN / 10000) % 10);
	accum += 3 * ((PIN / 1000) % 10);
	accum += 1 * ((PIN / 100) % 10);
	accum += 3 * ((PIN / 10) % 10);

	digit = (accum % 10);
	return (10 - digit) % 10;
}

uint32
wps_generatePin(char *c_devPwd, int buf_len, IN bool b_display)
{
	BufferObj *bo_devPwd;
	uint8 *devPwd = NULL;
	uint32 val = 0;
	uint32 checksum = 0;
	char local_pwd[20];

	/*
	 * buffer size needs to big enough to hold 8 digits plus the string terminition
	 * character '\0'
	*/
	if (buf_len < 9)
		return WPS_ERR_BUFFER_TOO_SMALL;

	bo_devPwd = buffobj_new();
	if (!bo_devPwd)
		return WPS_ERR_SYSTEM;

	/* Use the GeneratePSK() method in RegProtocol to help with this */
	buffobj_Reset(bo_devPwd);
	if (WPS_SUCCESS != reg_proto_generate_psk(8, bo_devPwd)) {
		TUTRACE((TUTRACE_ERR,
			"MC_GenerateDevPwd: Could not generate enrDevPwd\n"));
		buffobj_del(bo_devPwd);
		return WPS_ERR_SYSTEM;
	}
	devPwd = bo_devPwd->pBase;
	sprintf(local_pwd, "%08u", *(uint32 *)devPwd);

	/* Compute the checksum */
	local_pwd[7] = '\0';
	val = strtoul(local_pwd, NULL, 10);
	if (val == 0) {
		TUTRACE((TUTRACE_ERR, "MC_GenerateDevPwd: DevPwd is not numeric\n"));
		return WPS_ERR_SYSTEM;
	}
	checksum = wps_computeChecksum(val);
	val = val*10 + checksum;
	sprintf(local_pwd, "%08u", val); /* make sure 8 characters are displayed */
	local_pwd[8] = '\0';
	TUTRACE((TUTRACE_ERR, "MC::GenerateDevPwd: wps_device_pin = %s\n", local_pwd));
	buffobj_del(bo_devPwd);

	if (b_display)
		/* Display this pwd */
		TUTRACE((TUTRACE_ERR, "DEVICE PIN: %s\n", local_pwd));

	/* Output result */
	strncpy(c_devPwd, local_pwd, 8);
	c_devPwd[8] = '\0';

	return WPS_SUCCESS;
}

bool
wps_isSRPBC(IN uint8 *p_data, IN uint32 len)
{
	WpsProbrspIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = buffobj_new();

	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	tlv_set(&probrspIE.selRegistrar, WPS_ID_SEL_REGISTRAR, (void *)0, 0);
	tlv_set(&probrspIE.pwdId, WPS_ID_DEVICE_PWD_ID, (void *)0, 0);

	if (!tlv_find_dserialize(&probrspIE.selRegistrar, WPS_ID_SEL_REGISTRAR, bufObj, 0, 0) &&
	    probrspIE.selRegistrar.m_data == TRUE)
		tlv_find_dserialize(&probrspIE.pwdId, WPS_ID_DEVICE_PWD_ID, bufObj, 0, 0);

	buffobj_del(bufObj);
	if (probrspIE.pwdId.m_data == 0x04)
		return TRUE;
	else
		return FALSE;
}

/*
 * Return the state of WPS IE attribute "Select Registrar" indicating if the user has
 * recently activated a registrar to add an Enrollee.
*/
bool
wps_isSELR(IN uint8 *p_data, IN uint32 len)
{
	WpsBeaconIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = buffobj_new();

	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	tlv_set(&probrspIE.selRegistrar, WPS_ID_SEL_REGISTRAR, (void *)0, 0);

	tlv_find_dserialize(&probrspIE.selRegistrar, WPS_ID_SEL_REGISTRAR, bufObj, 0, 0);

	buffobj_del(bufObj);

	if (probrspIE.selRegistrar.m_data == TRUE)
		return TRUE;
	else
		return FALSE;
}

bool
wps_isWPSS(IN uint8 *p_data, IN uint32 len)
{
	WpsProbrspIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = buffobj_new();

	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	tlv_dserialize(&probrspIE.version, WPS_ID_VERSION, bufObj, 0, 0);
	tlv_dserialize(&probrspIE.scState, WPS_ID_SC_STATE, bufObj, 0, 0);

	buffobj_del(bufObj);
	if (probrspIE.scState.m_data == WPS_SCSTATE_CONFIGURED)
		return TRUE; /* configured */
	else
		return FALSE; /* unconfigured */
}

/* WSC 2.0 */
bool
wps_isVersion2(uint8 *p_data, uint32 len, uint8 *version2, uint8 *macs)
{
	bool isv2 = FALSE;
	WpsProbrspIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = NULL;
	BufferObj *vendorExt_bufObj = NULL;


	bufObj = buffobj_new();
	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	/* Initial set */
	tlv_vendorExt_set(&probrspIE.vendorExt, NULL, NULL, 0);
	subtlv_set(&probrspIE.version2, WPS_WFA_SUBID_VERSION2, (void *)0, 0);
	subtlv_set(&probrspIE.authorizedMACs, WPS_WFA_SUBID_AUTHORIZED_MACS, (void *)0, 0);

	/* Check subelement in WFA Vendor Extension */
	if (tlv_find_vendorExtParse(&probrspIE.vendorExt, bufObj, (uint8 *)WFA_VENDOR_EXT_ID) != 0)
		goto no_wfa_vendorExt;

	/* Deserialize subelement */
	vendorExt_bufObj = buffobj_new();
	if (!vendorExt_bufObj) {
		buffobj_del(bufObj);
		return FALSE;
	}

	buffobj_dserial(vendorExt_bufObj, probrspIE.vendorExt.vendorData,
		probrspIE.vendorExt.dataLength);

	/* Get Version2 and AuthorizedMACs subelement */
	if (!subtlv_dserialize(&probrspIE.version2, WPS_WFA_SUBID_VERSION2,
		vendorExt_bufObj, 0, 0) && probrspIE.version2.m_data >= WPS_VERSION2) {
		/* Get AuthorizedMACs list */
		subtlv_find_dserialize(&probrspIE.authorizedMACs, WPS_WFA_SUBID_AUTHORIZED_MACS,
			vendorExt_bufObj,  0, 0);
	}

no_wfa_vendorExt:
	/* Copy AuthorizedMACs list back */
	if (version2)
		*version2 = probrspIE.version2.m_data;

	if (probrspIE.authorizedMACs.subtlvbase.m_len && macs) {
		memcpy(macs, probrspIE.authorizedMACs.m_data,
			probrspIE.authorizedMACs.subtlvbase.m_len);
		isv2 = TRUE;
	}

	if (vendorExt_bufObj)
		buffobj_del(vendorExt_bufObj);

	buffobj_del(bufObj);

	return isv2;
}

/* Is mac in AuthorizedMACs list */
bool
wps_isAuthorizedMAC(IN uint8 *p_data, IN uint32 len, IN uint8 *mac)
{
	int i;
	bool found = FALSE;
	uint8 *amac;
	WpsProbrspIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = NULL;
	BufferObj *vendorExt_bufObj = NULL;


	bufObj = buffobj_new();
	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	/* Initial set */
	tlv_vendorExt_set(&probrspIE.vendorExt, NULL, NULL, 0);
	subtlv_set(&probrspIE.version2, WPS_WFA_SUBID_VERSION2, (void *)0, 0);
	subtlv_set(&probrspIE.authorizedMACs, WPS_WFA_SUBID_AUTHORIZED_MACS, (void *)0, 0);

	/* Check subelement in WFA Vendor Extension */
	if (tlv_find_vendorExtParse(&probrspIE.vendorExt, bufObj, (uint8 *)WFA_VENDOR_EXT_ID) != 0)
		goto no_wfa_vendorExt;

	/* Deserialize subelement */
	vendorExt_bufObj = buffobj_new();
	if (!vendorExt_bufObj) {
		buffobj_del(bufObj);
		return FALSE;
	}

	buffobj_dserial(vendorExt_bufObj, probrspIE.vendorExt.vendorData,
		probrspIE.vendorExt.dataLength);

	/* Get Version2 and AuthorizedMACs subelement */
	if (!subtlv_dserialize(&probrspIE.version2, WPS_WFA_SUBID_VERSION2,
		vendorExt_bufObj, 0, 0) && probrspIE.version2.m_data >= WPS_VERSION2) {
		/* Get AuthorizedMACs list */
		subtlv_find_dserialize(&probrspIE.authorizedMACs, WPS_WFA_SUBID_AUTHORIZED_MACS,
			vendorExt_bufObj,  0, 0);
	}

no_wfa_vendorExt:
	/* Check Mac in AuthorizedMACs list */
	if (probrspIE.authorizedMACs.subtlvbase.m_len >= SIZE_MAC_ADDR && mac) {
		for (i = 0; i < probrspIE.authorizedMACs.subtlvbase.m_len; i += SIZE_MAC_ADDR) {
			amac = &probrspIE.authorizedMACs.m_data[i];
			if (memcmp(amac, mac, SIZE_MAC_ADDR) == 0) {
				found = TRUE;
				break;
			}
		}
	}

	if (vendorExt_bufObj)
		buffobj_del(vendorExt_bufObj);

	buffobj_del(bufObj);

	return found;
}

bool is_wps_ies(uint8* cp, uint len)
{
	uint8 *parse = cp;
	uint parse_len = len;
	uint8 *wpaie;

	while ((wpaie = wps_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID)))
		if (wl_is_wps_ie(&wpaie, &parse, &parse_len))
			break;
	if (wpaie)
		return TRUE;
	else
		return FALSE;
}

uint8 *wps_get_wps_iebuf(uint8* cp, uint len, uint *wps_iebuf_len)
{
	uint8 *parse = cp;
	uint parse_len = len;
	uint8 *wpaie, wps_ie_len;
	uint wps_iebuf_curr_len = 0;

	while ((wpaie = wps_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID))) {
		if (wl_is_wps_ie(&wpaie, &parse, &parse_len)) {
			/* Skip abnormal IE */
			wps_ie_len = wpaie[1];

			if (wps_ie_len < parse_len &&
			    (wps_iebuf_curr_len + wps_ie_len <= WPS_IE_BUF_LEN - 4)) {
				memcpy(&enr_api_wps_iebuf[wps_iebuf_curr_len], &wpaie[6],
					(wps_ie_len - 4)); /* ignore 00:50:F2:04 */
				wps_iebuf_curr_len += (wps_ie_len - 4);
			}
			/* point to the next ie */
			parse = wpaie + wps_ie_len + 2;
			parse_len = len - (parse - cp);
		}
	}

	if (wps_iebuf_len)
		*wps_iebuf_len = wps_iebuf_curr_len;

	if (wps_iebuf_curr_len)
		return enr_api_wps_iebuf;
	else
		return NULL;
}

bool is_SelectReg_PBC(uint8* cp, uint len)
{
	uint8 *wps_iebuf;
	uint wps_iebuf_len = 0;

	wps_iebuf = wps_get_wps_iebuf(cp, len, &wps_iebuf_len);
	if (wps_iebuf)
		return wps_isSRPBC(wps_iebuf, wps_iebuf_len);

	return FALSE;
}

bool is_AuthorizedMAC(uint8* cp, uint len, uint8 *mac)
{
	uint8 *wps_iebuf;
	uint wps_iebuf_len = 0;

	wps_iebuf = wps_get_wps_iebuf(cp, len, &wps_iebuf_len);
	if (wps_iebuf)
		return wps_isAuthorizedMAC(wps_iebuf, wps_iebuf_len, mac);

	return FALSE;
}

bool is_ConfiguredState(uint8* cp, uint len)
{
	uint8 *wps_iebuf;
	uint wps_iebuf_len = 0;

	wps_iebuf = wps_get_wps_iebuf(cp, len, &wps_iebuf_len);
	if (wps_iebuf)
		return wps_isWPSS(wps_iebuf, wps_iebuf_len);

	return FALSE;
}

/* WSC 2.0, The WPS IE may fragment */
bool is_wpsVersion2(uint8* cp, uint len, uint8 *version2, uint8 *macs)
{
	uint8 *wps_iebuf;
	uint wps_iebuf_len = 0;

	wps_iebuf = wps_get_wps_iebuf(cp, len, &wps_iebuf_len);
	if (wps_iebuf)
		return wps_isVersion2(wps_iebuf, wps_iebuf_len, version2, macs);

	return FALSE;
}

/* Is this body of this tlvs entry a WPA entry? If */
/* not update the tlvs buffer pointer/length */
bool wl_is_wps_ie(uint8 **wpaie, uint8 **tlvs, uint *tlvs_len)
{
	uint8 *ie = *wpaie;

	/* If the contents match the WPA_OUI and type=1 */
	if ((ie[1] >= 6) && !memcmp(&ie[2], WPA_OUI "\x04", 4)) {
		return TRUE;
	}

	/* point to the next ie */
	ie += ie[1] + 2;
	/* calculate the length of the rest of the buffer */
	*tlvs_len -= (int)(ie - *tlvs);
	/* update the pointer to the start of the buffer */
	*tlvs = ie;

	return FALSE;
}

/*
 * Traverse a string of 1-byte tag/1-byte length/variable-length value
 * triples, returning a pointer to the substring whose first element
 * matches tag
 */
uint8 *
wps_parse_tlvs(uint8 *tlv_buf, int buflen, uint key)
{
	uint8 *cp;
	int totlen;

	cp = tlv_buf;
	totlen = buflen;

	/* find tagged parameter */
	while (totlen >= 2) {
		uint tag;
		int len;

		tag = *cp;
		len = *(cp +1);

		/* validate remaining totlen */
		if ((tag == key) && (totlen >= (len + 2)))
			return (cp);

		cp += (len + 2);
		totlen -= (len + 2);
	}

	return NULL;
}

/*
 * filter wps enabled AP list. list_in and list_out can be the same.
 * return the number of WPS APs.
 */
int
wps_get_aplist(wps_ap_list_info_t *list_in, wps_ap_list_info_t *list_out)
{
	wps_ap_list_info_t *ap_in = &list_in[0];
	wps_ap_list_info_t *ap_out = &list_out[0];
	int i = 0, wps_apcount = 0;

	while (ap_in->used == TRUE && i < WPS_MAX_AP_SCAN_LIST_LEN) {
		if (true == is_wps_ies(ap_in->ie_buf, ap_in->ie_buflen)) {
			if (true == is_ConfiguredState(ap_in->ie_buf, ap_in->ie_buflen))
				ap_in->scstate = WPS_SCSTATE_CONFIGURED;
			/* WSC 2.0 */
			is_wpsVersion2(ap_in->ie_buf, ap_in->ie_buflen,
				&ap_in->version2, ap_in->authorizedMACs);
			memcpy(ap_out, ap_in, sizeof(wps_ap_list_info_t));
			wps_apcount++;
			ap_out = &list_out[wps_apcount];
		}
		i++;
		ap_in = &list_in[i];
	}

	/* in case of on-the-spot filtering, make sure we stop the list  */
	if (wps_apcount < WPS_MAX_AP_SCAN_LIST_LEN)
		ap_out->used = 0;

	return wps_apcount;
}

int
wps_get_pin_aplist(wps_ap_list_info_t *list_in, wps_ap_list_info_t *list_out)
{
	wps_ap_list_info_t *ap_in = &list_in[0];
	wps_ap_list_info_t *ap_out = &list_out[0];
	int i = 0, wps_apcount = 0;

	while (ap_in->used == TRUE && i < WPS_MAX_AP_SCAN_LIST_LEN) {
		if (true == is_ConfiguredState(ap_in->ie_buf, ap_in->ie_buflen) &&
		    true == wps_get_select_reg(ap_in) &&
		    false == is_SelectReg_PBC(ap_in->ie_buf, ap_in->ie_buflen)) {
			ap_in->scstate = WPS_SCSTATE_CONFIGURED;
			/* WSC 2.0 */
			is_wpsVersion2(ap_in->ie_buf, ap_in->ie_buflen,
				&ap_in->version2, ap_in->authorizedMACs);

			memcpy(ap_out, ap_in, sizeof(wps_ap_list_info_t));
			wps_apcount++;
			ap_out = &list_out[wps_apcount];
		}
		i++;
		ap_in = &list_in[i];
	}

	/* in case of on-the-spot filtering, make sure we stop the list  */
	if (wps_apcount < WPS_MAX_AP_SCAN_LIST_LEN)
		ap_out->used = 0;

	return wps_apcount;
}

static bool
wps_get_probrsp_uuid(uint8* cp, uint len, uint8 *uuid)
{
	bool ret = FALSE;
	uint8 *wps_iebuf;
	uint wps_iebuf_len = 0;
	CTlvUuid tlv_uuid;
	BufferObj *bufObj;

	if (cp == NULL || len == 0 || uuid == NULL)
		return FALSE;

	wps_iebuf = wps_get_wps_iebuf(cp, len, &wps_iebuf_len);
	if (wps_iebuf == NULL)
		return FALSE;

	if ((bufObj = buffobj_new()) == NULL)
		return FALSE;

	buffobj_dserial(bufObj, wps_iebuf, wps_iebuf_len);

	if (tlv_find_dserialize(&tlv_uuid, WPS_ID_UUID_E, bufObj, SIZE_UUID, 0) == 0) {
		memcpy(uuid, tlv_uuid.m_data, tlv_uuid.tlvbase.m_len);
		ret = TRUE;
	}

	buffobj_del(bufObj);
	return ret;
}

int
wps_get_pbc_ap(wps_ap_list_info_t *list_in, char *bssid, char *ssid, uint8 *wep,
	unsigned long time, char start)
{
	wps_ap_list_info_t *ap_in = &list_in[0];
	int pbc_found = 0;
	int i = 0;
	int band_2G = 0, band_5G = 0;
	uint8 uuid_2G[SIZE_UUID] = {0}, uuid_5G[SIZE_UUID] = {0};
	static unsigned long start_time;

	if (start)
		start_time = time;
	if (time  > start_time + PBC_WALK_TIME)
		return PBC_TIMEOUT;

	while (ap_in->used == TRUE && i < WPS_MAX_AP_SCAN_LIST_LEN) {
		if (true == is_SelectReg_PBC(ap_in->ie_buf, ap_in->ie_buflen)) {
			if (ap_in->band == WPS_RFBAND_50GHZ) {
				wps_get_probrsp_uuid(ap_in->ie_buf, ap_in->ie_buflen, uuid_5G);
				band_5G++;
			}
			else if (ap_in->band == WPS_RFBAND_24GHZ) {
				wps_get_probrsp_uuid(ap_in->ie_buf, ap_in->ie_buflen, uuid_2G);
				band_2G++;
			}
			TUTRACE((TUTRACE_INFO, "2.4G = %d, 5G = %d, band = 0x%x\n",
				band_2G, band_5G, ap_in->band));
			if (pbc_found) {
				if ((band_5G > 1) || (band_2G > 1))
					return PBC_OVERLAP;
				else if (band_5G == 1 && band_2G == 1 &&
					memcmp(uuid_2G, uuid_5G, SIZE_UUID))
					return PBC_OVERLAP;
			}
			pbc_found++;
			memcpy(bssid, ap_in->BSSID, 6);
			strcpy(ssid, (char *)ap_in->ssid);
			*wep = ap_in->wep;
		}

		i++;
		ap_in = &list_in[i];
	}

	if (!pbc_found)
		return PBC_NOT_FOUND;

	return PBC_FOUND_OK;
}

int
wps_get_amac_ap(wps_ap_list_info_t *list_in, uint8 *mac, char wildcard, char *bssid,
	char *ssid, uint8 *wep, unsigned long time, char start)
{
	int i = 0;
	uint8 wc_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; /* wildcard mac */
	static unsigned long amac_start_time;
	wps_ap_list_info_t *ap_in = &list_in[0];

	if (start)
		amac_start_time = time;
	if (time  > amac_start_time + PBC_WALK_TIME)
		return AUTHO_MAC_TIMEOUT;

	while (ap_in->used && i < WPS_MAX_AP_SCAN_LIST_LEN) {
		if (is_AuthorizedMAC(ap_in->ie_buf, ap_in->ie_buflen, mac)) {
			memcpy(bssid, ap_in->BSSID, 6);
			strcpy(ssid, (char *)ap_in->ssid);
			*wep = ap_in->wep;

			/* Method? */
			if (is_SelectReg_PBC(ap_in->ie_buf, ap_in->ie_buflen))
				return AUTHO_MAC_PBC_FOUND;

			return AUTHO_MAC_PIN_FOUND;
		}
		else if (wildcard && is_AuthorizedMAC(ap_in->ie_buf, ap_in->ie_buflen, wc_mac)) {
			memcpy(bssid, ap_in->BSSID, 6);
			strcpy(ssid, (char *)ap_in->ssid);
			*wep = ap_in->wep;

			/* Method? */
			if (is_SelectReg_PBC(ap_in->ie_buf, ap_in->ie_buflen))
				return AUTHO_MAC_WC_PBC_FOUND;

			return AUTHO_MAC_WC_PIN_FOUND;
		}

		i++;
		ap_in = &list_in[i];
	}

	return AUTHO_MAC_NOT_FOUND;
}

/*
 * Return whether a registrar is activated or not and the information is carried in WPS IE
 */
/* WSC 2.0, The WPS IE may fragment */
bool
wps_get_select_reg(const wps_ap_list_info_t *ap_in)
{
	uint8 *wps_iebuf;
	uint wps_iebuf_len = 0;

	wps_iebuf = wps_get_wps_iebuf(ap_in->ie_buf, ap_in->ie_buflen, &wps_iebuf_len);
	if (wps_iebuf)
		return wps_isSELR(wps_iebuf, wps_iebuf_len);

	return FALSE;
}

uint32
wps_update_RFBand(uint8 rfBand)
{
	g_mc->dev_info->rfBand = rfBand;
	return WPS_SUCCESS;
}

#ifdef WFA_WPS_20_TESTBED
/* NOTE: ie[0] is NOT a length */
uint32
wps_update_partial_ie(uint8 *buff, int buflen, uint8 *ie, uint8 ie_len,
	uint8 *updie, uint8 updie_len)
{
	uint32 ret = WPS_SUCCESS;
	BufferObj *bufObj = NULL, *ie_bufObj = NULL, *updie_bufObj = NULL;
	uint16 theType, m_len;
	int tlvtype;
	TlvObj_uint8 ie_tlv_uint8, updie_tlv_uint8;
	TlvObj_uint16 ie_tlv_uint16, updie_tlv_uint16;
	TlvObj_uint32 ie_tlv_uint32, updie_tlv_uint32;
	TlvObj_ptru ie_tlv_uint8_ptr, updie_tlv_uint8_ptr;
	TlvObj_ptr ie_tlv_char_ptr, updie_tlv_char_ptr;
	CTlvPrimDeviceType ie_primDeviceType, updie_primDeviceType;
	void *data_v;

	if (!buff || !ie || !updie)
		return WPS_ERR_SYSTEM;

	/* De-serialize the embedded ie data to get the TLVs */
	memcpy(enr_api_wps_iebuf, ie, ie_len);
	ie_bufObj = buffobj_new();
	if (!ie_bufObj)
		return WPS_ERR_SYSTEM;
	buffobj_dserial(ie_bufObj, enr_api_wps_iebuf, ie_len);

	/* De-serialize the update partial ie data to get the TLVs */
	updie_bufObj = buffobj_new();
	if (!updie_bufObj) {
		ret = WPS_ERR_SYSTEM;
		goto error;
	}
	buffobj_dserial(updie_bufObj, updie, updie_len);

	/*  fill in the WPA-WPS OUI  */
	buff[0] = 4;
	buff[1] = 0x00;
	buff[2] = 0x50;
	buff[3] = 0xf2;
	buff[4] = 0x04;

	/*  bufObj is final output */
	bufObj = buffobj_setbuf(buff+5, buflen-5);
	if (!bufObj) {
		ret = WPS_ERR_SYSTEM;
		goto error;
	}

	/* Add each TLVs */
	while ((theType = buffobj_NextType(ie_bufObj))) {
		/* Check update ie */
		tlvtype = tlv_gettype(theType);
		switch (tlvtype) {
		case TLV_UINT8:
			if (tlv_dserialize(&ie_tlv_uint8, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = &ie_tlv_uint8.m_data;
			m_len = ie_tlv_uint8.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_uint8, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = &updie_tlv_uint8.m_data;
				m_len = updie_tlv_uint8.tlvbase.m_len;
			}
			break;
		case TLV_UINT16:
			if (tlv_dserialize(&ie_tlv_uint16, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = &ie_tlv_uint16.m_data;
			m_len = ie_tlv_uint16.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_uint16, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = &updie_tlv_uint16.m_data;
				m_len = updie_tlv_uint16.tlvbase.m_len;
			}
			break;
		case TLV_UINT32:
			if (tlv_dserialize(&ie_tlv_uint32, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = &ie_tlv_uint32.m_data;
			m_len = ie_tlv_uint32.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_uint32, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = &updie_tlv_uint32.m_data;
				m_len = updie_tlv_uint32.tlvbase.m_len;
			}
			break;
		case TLV_UINT8_PTR:
			if (tlv_dserialize(&ie_tlv_uint8_ptr, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = ie_tlv_uint8_ptr.m_data;
			m_len = ie_tlv_uint8_ptr.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_uint8_ptr, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = updie_tlv_uint8_ptr.m_data;
				m_len = updie_tlv_uint8_ptr.tlvbase.m_len;
			}
			break;
		case TLV_CHAR_PTR:
			if (tlv_dserialize(&ie_tlv_char_ptr, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = ie_tlv_char_ptr.m_data;
			m_len = ie_tlv_char_ptr.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_char_ptr, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = updie_tlv_char_ptr.m_data;
				m_len = updie_tlv_char_ptr.tlvbase.m_len;
			}
			break;
		default:
			if (theType == WPS_ID_PRIM_DEV_TYPE) {
				if (tlv_primDeviceTypeParse(&ie_primDeviceType,	ie_bufObj) != 0) {
					ret = WPS_ERR_SYSTEM;
					goto error;
				}

				/* Check update one */
				if (tlv_find_primDeviceTypeParse(&updie_primDeviceType,
					updie_bufObj) == 0) {
					/* Use update ie */
					tlv_primDeviceTypeWrite(&updie_primDeviceType, bufObj);
				}
				else {
					/* Use embedded one */
					tlv_primDeviceTypeWrite(&ie_primDeviceType, bufObj);
				}
				continue;
			}

			ret = WPS_ERR_SYSTEM;
			goto error;
		}

		tlv_serialize(theType, bufObj, data_v, m_len);
	}

	/* store length */
	buff[0] += bufObj->m_dataLength;

error:
	if (bufObj)
		buffobj_del(bufObj);
	if (ie_bufObj)
		buffobj_del(ie_bufObj);
	if (updie_bufObj)
		buffobj_del(updie_bufObj);

	return ret;
}
#endif /* WFA_WPS_20_TESTBED */
