/*
 * WPS ui
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_ui.c 402970 2013-05-17 09:36:35Z $
 */

#include <stdio.h>
#include <tutrace.h>
#include <time.h>
#include <wps_wl.h>
#include <wps_ui.h>
#include <shutils.h>
#include <wlif_utils.h>
#include <wps_ap.h>
#include <wps_enr.h>
#include <security_ipc.h>
#include <wps.h>
#include <wps_pb.h>
#include <wps_led.h>
#include <wps_upnp.h>
#include <wps_ie.h>
#include <ap_ssr.h>
#ifdef WPS_NFC_DEVICE
#include <wps_nfc.h>
#endif

typedef struct {
	int wps_config_command;	/* Config */
	int wps_method;
	int wps_action;
	int wps_pbc_method;
	char wps_ifname[16];
	char wps_sta_pin[SIZE_64_BYTES];
	char wps_ssid[SIZE_32_BYTES+1];
	char wps_akm[SIZE_32_BYTES];
	char wps_crypto[SIZE_32_BYTES];
	char wps_psk[SIZE_64_BYTES+1];
	int wps_aplockdown;
	int wps_proc_status;	/* Status report */
	int wps_pinfail;
	char wps_pinfail_state[32];
	char wps_sta_mac[sizeof("00:00:00:00:00:00")];
	char wps_autho_sta_mac[sizeof("00:00:00:00:00:00")]; /* WSC 2.0 */
	char wps_sta_devname[32];
	char wps_enr_wsec[SIZE_32_BYTES];
	char wps_enr_ssid[SIZE_32_BYTES+1];
	char wps_enr_bssid[SIZE_32_BYTES];
	int wps_enr_scan;
#ifdef __CONFIG_WFI__
	char wps_device_pin[12]; /* For WiFi-Invite session PIN */
#endif /* __CONFIG_WFI__ */
	char wps_stareg_ap_pin[SIZE_64_BYTES];
	char wps_scstate[32];
#ifdef WPS_NFC_DEVICE
	int wps_nfc_dm_status;
	int wps_nfc_err_code;
#endif
} wps_ui_t;

static wps_hndl_t ui_hndl;
static int pending_flag = 0;
static unsigned long pending_time = 0;

#ifdef WPS_ADDCLIENT_WWTP
/* WSC 2.0,
 * One way to define the use of a particular PIN is that it should be valid for the whole
 * Walking Time period.  In other words, as long as the registration is not successful
 * and the process hasn't timed out and no new PIN has been entered, the AP should
 * accept to engage in a new registration process with the same PIN.
 * On some implementation, this is what happens on the enrollee side when it fails
 * registration with one of several AP and keep trying with all of them (actually,
 * I think we should implement this as well in our enrollee code).
 *
 * In our case, that would mean that :
 * - the AP should detect the NACK from the enrollee after failure on M4 (which I am
 * not sure it does) and close the session.
 * - as long as the PIN is valid, the AP should restart its state machine after failure,
 * keep the same PIN,  and answer an M1 with M2 instead of M2D.
 */
static unsigned long addclient_window_start = 0;
static bool wps_wer_override_active = FALSE;
#endif /* WPS_ADDCLIENT_WWTP */

static wps_ui_t s_wps_ui;
static wps_ui_t *wps_ui = &s_wps_ui;

char *
wps_ui_get_env(char *name)
{
	static char buf[32];

	if (!strcmp(name, "wps_config_command")) {
		sprintf(buf, "%d", wps_ui->wps_config_command);
		return buf;
	}

	if (!strcmp(name, "wps_action")) {
		sprintf(buf, "%d", wps_ui->wps_action);
		return buf;
	}

	if (!strcmp(name, "wps_method")) {
		sprintf(buf, "%d", wps_ui->wps_method);
		return buf;
	}

	if (!strcmp(name, "wps_pbc_method")) {
		sprintf(buf, "%d", wps_ui->wps_pbc_method);
		return buf;
	}

	if (!strcmp(name, "wps_ssid"))
		return wps_ui->wps_ssid;

	if (!strcmp(name, "wps_akm"))
		return wps_ui->wps_akm;

	if (!strcmp(name, "wps_crypto"))
		return wps_ui->wps_crypto;

	if (!strcmp(name, "wps_psk"))
		return wps_ui->wps_psk;

	if (!strcmp(name, "wps_sta_pin"))
		return wps_ui->wps_sta_pin;

	if (!strcmp(name, "wps_proc_status")) {
		sprintf(buf, "%d", wps_ui->wps_proc_status);
		return buf;
	}

	if (!strcmp(name, "wps_enr_wsec")) {
		return wps_ui->wps_enr_wsec;
	}

	if (!strcmp(name, "wps_enr_ssid")) {
		return wps_ui->wps_enr_ssid;
	}

	if (!strcmp(name, "wps_enr_bssid")) {
		return wps_ui->wps_enr_bssid;
	}

	if (!strcmp(name, "wps_enr_scan")) {
		sprintf(buf, "%d", wps_ui->wps_enr_scan);
		return buf;
	}

	if (!strcmp(name, "wps_stareg_ap_pin"))
		return wps_ui->wps_stareg_ap_pin;

	if (!strcmp(name, "wps_scstate"))
		return wps_ui->wps_scstate;

#ifdef __CONFIG_WFI__
	/* For WiFi-Invite session PIN */
	if (!strcmp(name, "wps_device_pin")) {
		return wps_ui->wps_device_pin;
	}
#endif /* __CONFIG_WFI__ */

	if (!strcmp(name, "wps_autho_sta_mac")) /* WSC 2.0 */
		return wps_ui->wps_autho_sta_mac;

	if (!strcmp(name, "wps_sta_devname"))
		return wps_ui->wps_sta_devname;

	if (!strcmp(name, "wps_pinfail_state"))
		return wps_ui->wps_pinfail_state;

#ifdef WPS_NFC_DEVICE
	if (!strcmp(name, "wps_nfc_dm_status")) {
		sprintf(buf, "%d", wps_ui->wps_nfc_dm_status);
		return buf;
	}

	if (!strcmp(name, "wps_nfc_err_code")) {
		sprintf(buf, "%d", wps_ui->wps_nfc_err_code);
		return buf;
	}
#endif

	return "";
}

void
wps_ui_set_env(char *name, char *value)
{
	if (!strcmp(name, "wps_ifname"))
		wps_strncpy(wps_ui->wps_ifname, value, sizeof(wps_ui->wps_ifname));
	else if (!strcmp(name, "wps_config_command"))
		wps_ui->wps_config_command = atoi(value);
	else if (!strcmp(name, "wps_action"))
		wps_ui->wps_action = atoi(value);
	else if (!strcmp(name, "wps_aplockdown"))
		wps_ui->wps_aplockdown = atoi(value);
	else if (!strcmp(name, "wps_proc_status"))
		wps_ui->wps_proc_status = atoi(value);
	else if (!strcmp(name, "wps_pinfail"))
		wps_ui->wps_pinfail = atoi(value);
	else if (!strcmp(name, "wps_pinfail_state"))
		wps_strncpy(wps_ui->wps_pinfail_state, value, sizeof(wps_ui->wps_pinfail_state));
	else if (!strcmp(name, "wps_sta_mac"))
		wps_strncpy(wps_ui->wps_sta_mac, value, sizeof(wps_ui->wps_sta_mac));
	else if (!strcmp(name, "wps_sta_devname"))
		wps_strncpy(wps_ui->wps_sta_devname, value, sizeof(wps_ui->wps_sta_devname));
	else if (!strcmp(name, "wps_sta_pin"))
		wps_strncpy(wps_ui->wps_sta_pin, value, sizeof(wps_ui->wps_sta_pin));
	else if (!strcmp(name, "wps_pbc_method"))
		wps_ui->wps_pbc_method = atoi(value);
#ifdef WPS_NFC_DEVICE
	else if (!strcmp(name, "wps_nfc_dm_status"))
		wps_ui->wps_nfc_dm_status = atoi(value);
	else if (!strcmp(name, "wps_nfc_err_code"))
		wps_ui->wps_nfc_err_code = atoi(value);
#endif

	return;
}

void
wps_ui_reset_env()
{
	/* Reset partial wps_ui environment variables */
	wps_ui->wps_config_command = WPS_UI_CMD_NONE;
	wps_ui->wps_action = WPS_UI_ACT_NONE;
	wps_ui->wps_pbc_method = WPS_UI_PBC_NONE;
}

int
wps_ui_is_pending()
{
	return pending_flag;
}

#ifdef BCMWPSAP
/* Start pending for any MAC  */
static void
wps_ui_start_pending(char *wps_ifname)
{
	unsigned long now;
	CTlvSsrIE ssrmsg;

	int i, imax, scb_num;
	char temp[32];
	char ifname[IFNAMSIZ];
	unsigned int wps_uuid[16];
	unsigned char uuid_R[16];
	unsigned char authorizedMacs[SIZE_MAC_ADDR] = {0};
	int authorizedMacs_len = 0;
	char authorizedMacs_buf[SIZE_MAC_ADDR * SIZE_AUTHORIZEDMACS_NUM] = {0};
	int authorizedMacs_buf_len = 0;
	BufferObj *authorizedMacs_bufObj = NULL, *uuid_R_bufObj = NULL;
	char *wlnames, *next, *ipaddr = NULL, *value;
	bool found = FALSE;
	bool b_wps_version2 = FALSE;
	bool b_do_pbc = FALSE;
	uint8 scState = WPS_SCSTATE_UNCONFIGURED;

	(void) time((time_t*)&now);

	if (strcmp(wps_ui->wps_sta_pin, "00000000") == 0)
		b_do_pbc = TRUE;

	/* Check push time only when we use PBC method */
	if (b_do_pbc && wps_pb_check_pushtime(now) == PBC_OVERLAP_CNT) {
		TUTRACE((TUTRACE_INFO, "%d PBC station found, ignored PBC!\n", PBC_OVERLAP_CNT));
		wps_ui->wps_config_command = WPS_UI_CMD_NONE;
		wps_setProcessStates(WPS_PBCOVERLAP);

		return;
	}

	/* WSC 2.0,  find ipaddr according to wps_ifname */
	imax = wps_get_ess_num();
	for (i = 0; i < imax; i++) {
		sprintf(temp, "ess%d_wlnames", i);
		wlnames = wps_safe_get_conf(temp);

		foreach(ifname, wlnames, next) {
			if (!strcmp(ifname, wps_ifname)) {
				found = TRUE;
				break;
			}
		}

		if (found) {
			/* Get ipaddr */
			sprintf(temp, "ess%d_ipaddr", i);
			ipaddr = wps_safe_get_conf(temp);

			/* According to WPS 2.0 section "Wi-Fi Simple Configuration State"
			 * Note: The Internal Registrar waits until successful completion of the
			 * protocol before applying the automatically generated credentials to
			 * avoid an accidental transition from "Not Configured" to "Configured"
			 * in the case that a neighboring device tries to run WSC
			 */
			/* Get builtin register scState */
			sprintf(temp, "ess%d_wps_oob", i);
			if (!strcmp(wps_safe_get_conf(temp), "disabled") ||
			    !strcmp(wps_safe_get_conf("wps_oob_configured"), "1"))
				scState = WPS_SCSTATE_CONFIGURED;
			else
				scState = WPS_SCSTATE_UNCONFIGURED;

			/* Get UUID, convert string to hex */
			value = wps_safe_get_conf("wps_uuid");
			sscanf(value,
				"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
				&wps_uuid[0], &wps_uuid[1], &wps_uuid[2], &wps_uuid[3],
				&wps_uuid[4], &wps_uuid[5], &wps_uuid[6], &wps_uuid[7],
				&wps_uuid[8], &wps_uuid[9], &wps_uuid[10], &wps_uuid[11],
				&wps_uuid[12], &wps_uuid[13], &wps_uuid[14], &wps_uuid[15]);
			for (i = 0; i < 16; i++)
				uuid_R[i] = (wps_uuid[i] & 0xff);

			break;
		}
	}

	if (!found || !ipaddr) {
		TUTRACE((TUTRACE_ERR, "Can't find ipaddr according to %s\n", wps_ifname));
		return;
	}

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		b_wps_version2 = TRUE;

	/* Set built-in registrar to be selected registrar */
	if (b_wps_version2) {
		if (strlen(wps_ui->wps_autho_sta_mac)) {
			/* For now, only support one authorized mac */
			authorizedMacs_len = SIZE_MAC_ADDR;
			ether_atoe(wps_ui->wps_autho_sta_mac, authorizedMacs);
		}
		else {
			/* WSC 2.0 r44, add wildcard MAC when authorized mac not specified */
			authorizedMacs_len = SIZE_MAC_ADDR;
			memcpy(authorizedMacs, wildcard_authorizedMacs, SIZE_MAC_ADDR);
		}

		/* Prepare authorizedMacs_Obj and uuid_R_Obj */
		authorizedMacs_bufObj = buffobj_new();
		if (!authorizedMacs_bufObj) {
			TUTRACE((TUTRACE_ERR, "Can't allocate authorizedMacs_bufObj\n"));
			return;
		}
		uuid_R_bufObj = buffobj_new();
		if (!uuid_R_bufObj) {
			TUTRACE((TUTRACE_ERR, "Can't allocate uuid_R_bufObj\n"));
			buffobj_del(authorizedMacs_bufObj);
			return;
		}
	}

	wps_ie_default_ssr_info(&ssrmsg, authorizedMacs, authorizedMacs_len,
		authorizedMacs_bufObj, uuid_R, uuid_R_bufObj, scState);

	/* WSC 2.0, add authorizedMACs when runs built-in registrar */
	ap_ssr_set_scb(ipaddr, &ssrmsg, NULL, now);

	if (b_wps_version2) {
		/*
		 * Get union (WSC 2.0 page 79, 8.4.1) of information received from all Registrars,
		 * use recently authorizedMACs list and update union attriabutes to ssrmsg
		 */
		scb_num = ap_ssr_get_union_attributes(&ssrmsg, authorizedMacs_buf,
			&authorizedMacs_buf_len);

		/*
		 * No any SSR exist, no any selreg is TRUE, set UPnP SSR time expired
		 * if it is activing
		 */
		if (scb_num == 0) {
			TUTRACE((TUTRACE_ERR, "No any SSR exist\n"));
			if (authorizedMacs_bufObj)
				buffobj_del(authorizedMacs_bufObj);
			if (uuid_R_bufObj)
				buffobj_del(uuid_R_bufObj);
			return;
		}

		/* Reset ssrmsg.authorizedMacs */
		ssrmsg.authorizedMacs.m_data = NULL;
		ssrmsg.authorizedMacs.subtlvbase.m_len = 0;

		/* Construct ssrmsg.authorizedMacs from authorizedMacs_buf */
		if (authorizedMacs_buf_len) {
			/* Re-use authorizedMacs_bufObj */
			buffobj_Rewind(authorizedMacs_bufObj);
			/* De-serialize the authorizedMacs data to get the TLVs */
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, authorizedMacs_bufObj,
				authorizedMacs_buf, authorizedMacs_buf_len);
			buffobj_Rewind(authorizedMacs_bufObj);
			subtlv_dserialize(&ssrmsg.authorizedMacs, WPS_WFA_SUBID_AUTHORIZED_MACS,
				authorizedMacs_bufObj, 0, 0);
		}
	}

	wps_ie_set(wps_ifname, &ssrmsg);

	if (authorizedMacs_bufObj)
		buffobj_del(authorizedMacs_bufObj);

	if (uuid_R_bufObj)
		buffobj_del(uuid_R_bufObj);

	pending_flag = 1;
	pending_time = now;

#ifdef WPS_ADDCLIENT_WWTP
	/* Change in PF #3 */
	if (addclient_window_start && wps_wer_override_active) {
		pending_time = addclient_window_start;
	}
	else {
		pending_time = now;
		addclient_window_start = now;
	}
#endif /* WPS_ADDCLIENT_WWTP */

	return;
}
#endif /* ifdef BCMWPSAP */

#ifdef WPS_ADDCLIENT_WWTP
int
wps_ui_is_SET_cmd(char *buf, int buflen)
{
	if (strncmp(buf, "SET ", 4) == 0)
		return 1;

	return 0;
}

void
wps_ui_wer_override_active(bool active)
{
	wps_wer_override_active = active;
}

void
wps_ui_close_addclient_window()
{
	addclient_window_start = 0;
	wps_ui_wer_override_active(FALSE);
}
#endif /* WPS_ADDCLIENT_WWTP */

void
wps_ui_clear_pending()
{
	pending_flag = 0;
	pending_time = 0;
}

int
wps_ui_pending_expire()
{
	unsigned long now;

	if (pending_flag && pending_time) {
		(void) time((time_t*)&now);

		if ((now < pending_time) || ((now - pending_time) > WPS_MAX_TIMEOUT)) {
#ifdef WPS_ADDCLIENT_WWTP
			wps_ui_close_addclient_window();
#endif
			return 1;
		}
	}

	return 0;
}

static void
wps_ui_dump(wps_ui_t *ui)
{
	char *cmd_str[] = {"None", "Start", "Stop",
			   "NFC Write Configuration", "NFC Read Configuration",
			   "NFC Write Password", "NFC Read Password",
			   "NFC Stop"};
	char *method_str[] = {"NONE", "PIN", "PBC", "NFC_PW"};
	char *action_str[] = {"NONE", "ENROLL", "CONFIGAP", "ADDENROLLEE",
			      "STA_CONFIGAP", "STA_GETAPCONFIG"};
	char *pbc_str[] = {"NONE", "HW", "SW"};

	printf("============ wps_ui_dump ============\n");
	printf("[%s]::\n", cmd_str[ui->wps_config_command]);
	printf("    Method:     %s\n", method_str[ui->wps_method]);
	printf("    Action:     %s\n", action_str[ui->wps_action]);
	printf("    PBC Method: %s\n", pbc_str[ui->wps_pbc_method]);
	printf("    Interface:  %s\n", ui->wps_ifname);
	printf("    STA PIN:    %s\n", ui->wps_sta_pin);
	printf("    SSID:       %s\n", ui->wps_ssid);
	printf("    AKM:        %s\n", ui->wps_akm);
	printf("    Crypto:     %s\n", ui->wps_crypto);
	printf("    PSK:        %s\n", ui->wps_psk);
	printf("    ENR WSEC:   %s\n", ui->wps_enr_wsec);
	printf("    ENR SSID:   %s\n", ui->wps_enr_ssid);
	printf("    ENR BSSID:  %s\n", ui->wps_enr_bssid);
	printf("    ENR SCAN:   %d\n", ui->wps_enr_scan);
#ifdef __CONFIG_WFI__
	printf("    WFI DEV PIN: %s\n", ui->wps_device_pin);
#endif /* __CONFIG_WFI__ */
	printf("    Autho MAC: %s\n", ui->wps_autho_sta_mac);
	printf("    STA REG AP PIN: %s\n", ui->wps_stareg_ap_pin);
	printf("    WPS State: %s\n", ui->wps_scstate);
	printf("=====================================\n");

	return;
}

#ifdef WPS_NFC_DEVICE
static DevInfo *
wps_ui_nfc_get_devinfo(DevInfo *ap_devinfo)
{
	bool b_wps_version2 = FALSE;
	char *value, *next, *dev_key = NULL, *wep_key = NULL;
	char psk[SIZE_64_BYTES + 1] = {0};
	char key[8], akmstr[32];
	int wep_index = 0;
	unsigned int akm = 0;
	unsigned int wsec = 0;
	char tmp[256], prefix[] = "wlXXXXXXXXXX_";

	if (!ap_devinfo)
		return NULL;

	memset(ap_devinfo, 0, sizeof(DevInfo));

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0) {
		b_wps_version2 = TRUE;
		value = wps_get_conf("wps_version2_num");
		ap_devinfo->version2 = (uint8)(strtoul(value, NULL, 16));
	}

	/* Prefix */
	snprintf(prefix, sizeof(prefix), "%s_", wps_ui->wps_ifname);

	/* If caller has specified credentials using them */
	if ((value = wps_ui_get_env("wps_ssid")) && strcmp(value, "") != 0) {
		/* SSID */
		value = wps_ui_get_env("wps_ssid");
		wps_strncpy(ap_devinfo->ssid, value, sizeof(ap_devinfo->ssid));

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
		/* SSID */
		value = wps_safe_get_conf(strcat_r(prefix, "ssid", tmp));
		wps_strncpy(ap_devinfo->ssid, value, sizeof(ap_devinfo->ssid));

		/* AKM */
		value = wps_safe_get_conf(strcat_r(prefix, "akm", tmp));
		foreach(akmstr, value, next) {
			if (!strcmp(akmstr, "psk"))
				akm |= WPA_AUTH_PSK;
			else if (!strcmp(akmstr, "psk2"))
				akm |= WPA2_AUTH_PSK;
			else {
				TUTRACE((TUTRACE_INFO, "Unknown AKM value\n"));
				return NULL;
			}
		}

		value = wps_safe_get_conf(strcat_r(prefix, "wep", tmp));
		wsec = !strcmp(value, "enabled") ? WEP_ENABLED : 0;

		/* Crypto */
		value = wps_safe_get_conf(strcat_r(prefix, "crypto", tmp));
		if (WPS_WLAKM_PSK(akm) || WPS_WLAKM_PSK2(akm)) {
			if (!strcmp(value, "tkip"))
				wsec |= TKIP_ENABLED;
			else if (!strcmp(value, "aes"))
				wsec |= AES_ENABLED;
			else if (!strcmp(value, "tkip+aes"))
				wsec |= TKIP_ENABLED|AES_ENABLED;
			else {
				TUTRACE((TUTRACE_INFO, "Unknown Crypto value\n"));
				return NULL;
			}

			/* Set PSK key */
			value = wps_safe_get_conf(strcat_r(prefix, "wpa_psk", tmp));
			wps_strncpy(psk, value, sizeof(psk));
		}
	}

	/* The MAC Address attribute contains the Enrollee's MAC address.
	 * To set this attribute, the Registrar needs prior knowledge of the Enrollee's MAC
	 * address, either through a partial run of the registration protocol
	 * (until M2D message) or through prior out-of-band communication as with the
	 * Connection Handover protocol. If a Registrar does not know the Enrollee's MAC
	 * address or does not want to link the Credential to a specific MAC address,
	 * the broadcast MAC address (all octets set to 255) can be used. The MAC Address
	 * attribute is encoded as a Wi-Fi Simple Configuration TLV with Type 0x1020 and
	 * Length 0x0006
	 */
	if (strlen(wps_ui->wps_sta_mac))
		ether_atoe(wps_ui->wps_sta_mac, ap_devinfo->peerMacAddr);
	else
		ether_atoe("FF:FF:FF:FF:FF:FF", ap_devinfo->peerMacAddr);

	if (wsec & WEP_ENABLED) {
		/* Key index */
		value = wps_safe_get_conf(strcat_r(prefix, "key", tmp));
		wep_index = (int)strtoul(value, NULL, 0);

		/* Key */
		sprintf(key, "key%s", value);
		wep_key = wps_safe_get_conf(strcat_r(prefix, key, tmp));
	}

	/* KEY MGMT */
	/* WSC 2.0, deprecated SHARED */
	if (!strcmp(strcat_r(prefix, "auth", tmp), "1")) {
		strcpy(ap_devinfo->keyMgmt, "SHARED");
		if (b_wps_version2) {
			TUTRACE((TUTRACE_INFO, "Write NFC Configuration: "
				"Authentication type is Shared, violate WSC 2.0\n"));
			return NULL;
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

		if (b_wps_version2) {
			TUTRACE((TUTRACE_INFO, "Write NFC Configuration: "
				"WEP is enabled violate WSC 2.0\n"));
			return NULL;
		}
	}
	else if (!WPS_WLAKM_NONE(akm)) {
		dev_key = psk;

		if ((wsec & (AES_ENABLED | TKIP_ENABLED)) == 0) {
			TUTRACE((TUTRACE_INFO, "Error in crypto\n"));
			return NULL;
		}
		if (b_wps_version2) {
			/* WSC 2.0, WPS-PSK is allowed only in mix mode */
			if ((akm & WPA_AUTH_PSK) && !(akm & WPA2_AUTH_PSK)) {
				TUTRACE((TUTRACE_ERR, "Write NFC Configuration: "
					" WPA-PSK enabled without WPA2-PSK,"
					" violate WSC 2.0\n"));
				return NULL;
			}
		}
	}

	/* Network key,  it may WEP key or Passphrase key */
	memset(ap_devinfo->nwKey, 0, SIZE_64_BYTES);
	if (dev_key)
		wps_strncpy(ap_devinfo->nwKey, dev_key, sizeof(ap_devinfo->nwKey));

	/* Set crypto algorithm */
	ap_devinfo->crypto = 0;
	if (wsec & TKIP_ENABLED)
		ap_devinfo->crypto |= WPS_ENCRTYPE_TKIP;
	if (wsec & AES_ENABLED)
		ap_devinfo->crypto |= WPS_ENCRTYPE_AES;

	if (ap_devinfo->crypto == 0)
		ap_devinfo->crypto = WPS_ENCRTYPE_NONE;

	return ap_devinfo;
}

static int
wps_ui_nfc_write_cfg()
{
	DevInfo ap_devinfo;

	if (wps_ui_nfc_get_devinfo(&ap_devinfo) == NULL)
		return NFC_ERROR;

	/* Write NFC Configuration */
	return wps_nfc_write_cfg(wps_ui->wps_ifname, &ap_devinfo);
}

static int
wps_ui_nfc_write_pw()
{
	uint8 hex_pin[SIZE_32_BYTES];
	DevInfo ap_devinfo = {0};

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		ap_devinfo.version2 = (uint8)(strtoul(wps_get_conf("wps_version2_num"), NULL, 16));

	wps_gen_oob_dev_pw(ap_devinfo.pre_privkey, ap_devinfo.pub_key_hash,
		&ap_devinfo.devPwdId, hex_pin, sizeof(hex_pin));

	/* Do Hex to String translation */
	if (!wps_hex2str(ap_devinfo.pin, sizeof(ap_devinfo.pin), hex_pin, sizeof(hex_pin))) {
		TUTRACE((TUTRACE_NFC, "Invalid parameters\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	/* Write NFC Password */
	return wps_nfc_write_pw(wps_ui->wps_ifname, &ap_devinfo);
}

static int
wps_ui_nfc_ho_selector()
{
	DevInfo ap_devinfo;

	if (wps_ui_nfc_get_devinfo(&ap_devinfo) == NULL)
		return NFC_ERROR;

	/* Start hand over selector with WPS configuration */
	return wps_nfc_ho_selector(wps_ui->wps_ifname, &ap_devinfo);
}

void
wps_ui_nfc_open_session()
{
	char uibuf[1024] = "SET ";
	int uilen = 4;

	/* AP Registration: Add Enrollee */
	/* AP Enrollment: Add Enrollee */
	/* STA Enrollment: Get AP Config */
	/* STA Registration: Get AP Config */
	uilen += sprintf(uibuf + uilen, "wps_config_command=%d ", WPS_UI_CMD_START);
	uilen += sprintf(uibuf + uilen, "wps_action=%d ", wps_ui->wps_action);
	uilen += sprintf(uibuf + uilen, "wps_method=%d ", WPS_UI_METHOD_PIN);
	uilen += sprintf(uibuf + uilen, "wps_pbc_method=\"%d\" ", wps_ui->wps_pbc_method);
	uilen += sprintf(uibuf + uilen, "wps_sta_pin=\"%s\" ", wps_ui->wps_sta_pin);
	uilen += sprintf(uibuf + uilen, "wps_stareg_ap_pin=\"%s\" ", wps_ui->wps_stareg_ap_pin);
	uilen += sprintf(uibuf + uilen, "wps_autho_sta_mac=%s ", wps_ui->wps_autho_sta_mac);
	uilen += sprintf(uibuf + uilen, "wps_ifname=%s ", wps_ui->wps_ifname);
	uilen += sprintf(uibuf + uilen, "wps_ssid=\"%s\" ", wps_ui->wps_ssid);
	uilen += sprintf(uibuf + uilen, "wps_akm=\"%s\" ", wps_ui->wps_akm);
	uilen += sprintf(uibuf + uilen, "wps_crypto=\"%s\" ", wps_ui->wps_crypto);
	uilen += sprintf(uibuf + uilen, "wps_psk=\"%s\" ", wps_ui->wps_psk);
	uilen += sprintf(uibuf + uilen, "wps_enr_wsec=\"%s\" ", wps_ui->wps_enr_wsec);
	uilen += sprintf(uibuf + uilen, "wps_enr_ssid=\"%s\" ", wps_ui->wps_enr_ssid);
	uilen += sprintf(uibuf + uilen, "wps_enr_bssid=\"%s\" ", wps_ui->wps_enr_bssid);
	uilen += sprintf(uibuf + uilen, "wps_enr_scan=\"%d\" ", wps_ui->wps_enr_scan);

	wps_osl_nfc_set_wps_env(&ui_hndl, uibuf, uilen+1);
	return;
}
#endif /* WPS_NFC_DEVICE */

static void
wps_ui_do_get()
{
#ifdef WPS_UPNP_DEVICE
	unsigned char wps_uuid[16];
#endif
	char buf[256];
	int len = 0;

	len += sprintf(buf + len, "wps_config_command=%d ", wps_ui->wps_config_command);
	len += sprintf(buf + len, "wps_action=%d ", wps_ui->wps_action);
	/* Add in PF #3 */
	len += sprintf(buf + len, "wps_method=%d ", wps_ui->wps_method);
	len += sprintf(buf + len, "wps_autho_sta_mac=%s ", wps_ui->wps_autho_sta_mac);
	len += sprintf(buf + len, "wps_ifname=%s ", wps_ui->wps_ifname);
#ifdef WPS_UPNP_DEVICE
	wps_upnp_device_uuid(wps_uuid);
	len += sprintf(buf + len,
		"wps_uuid=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ",
		wps_uuid[0], wps_uuid[1], wps_uuid[2], wps_uuid[3],
		wps_uuid[4], wps_uuid[5], wps_uuid[6], wps_uuid[7],
		wps_uuid[8], wps_uuid[9], wps_uuid[10], wps_uuid[11],
		wps_uuid[12], wps_uuid[13], wps_uuid[14], wps_uuid[15]);
#endif /* WPS_UPNP_DEVICE */
	len += sprintf(buf + len, "wps_aplockdown=%d ", wps_ui->wps_aplockdown);
	len += sprintf(buf + len, "wps_proc_status=%d ", wps_ui->wps_proc_status);
	len += sprintf(buf + len, "wps_pinfail=%d ", wps_ui->wps_pinfail);
	len += sprintf(buf + len, "wps_sta_mac=%s ", wps_ui->wps_sta_mac);
	len += sprintf(buf + len, "wps_sta_devname=\"%s\" ", wps_ui->wps_sta_devname);
#ifdef WPS_NFC_DEVICE
	len += sprintf(buf + len, "wps_nfc_dm_status=%d ", wps_ui->wps_nfc_dm_status);
	len += sprintf(buf + len, "wps_nfc_err_code=%d ", wps_ui->wps_nfc_err_code);
#endif
	wps_osl_send_uimsg(&ui_hndl, buf, len+1);
	return;
}


#define MAX_UI_ARGS	32

/* Note, the ui parser has limitation on processing quote.
 * It can only process string within two quotes at most
 */
static int
wps_ui_parse_env(char *buf, wps_ui_t *ui)
{
	char *argv[MAX_UI_ARGS] = {0};
	char *p;
	char *name, *value;
	int i;

	/* Seperate buf into argv[], we have to make sure at least one is empty */
	for (i = 0, p = buf; i < MAX_UI_ARGS-1; i++) {
		/* Eat white space */
		while (*p == ' ')
			p++;
		if (*p == 0)
			goto all_found;

		/* Save this item */
		argv[i] = p;

		 /* Search until space */
		while (*p != ' ' && *p) {
			/* Take care of doube quot */
			if (*p == '\"') {
				char *qs, *qe;

				qs = p;
				qe = strchr(p+1, '\"');
				if (qe == NULL) {
					printf("%s:%d, unbalanced quote string!",
						__func__, __LINE__);
					argv[i] = 0;
					goto all_found;
				}


				/* Null eneded quot string and do shift */
				*qe = '\0';
				memmove(qs, qs+1, (int)(qe-qs));

				p = qe+1;
				break;
			}

			p++;
		}

		if (*p)
			*p++ = '\0';
	}

all_found:
	for (i = 0; argv[i]; i++) {
		value = argv[i];
		name = strsep(&value, "=");
		if (name) {
			if (!strcmp(name, "wps_config_command"))
				ui->wps_config_command = atoi(value);
			else if (!strcmp(name, "wps_method"))
				ui->wps_method = atoi(value);
			else if (!strcmp(name, "wps_action"))
				ui->wps_action = atoi(value);
			else if (!strcmp(name, "wps_pbc_method"))
				ui->wps_pbc_method = atoi(value);
			else if (!strcmp(name, "wps_ifname"))
				wps_strncpy(ui->wps_ifname, value, sizeof(ui->wps_ifname));
			else if (!strcmp(name, "wps_sta_pin"))
				wps_strncpy(ui->wps_sta_pin, value, sizeof(ui->wps_sta_pin));
			else if (!strcmp(name, "wps_ssid"))
				wps_strncpy(ui->wps_ssid, value, sizeof(ui->wps_ssid));
			else if (!strcmp(name, "wps_akm"))
				wps_strncpy(ui->wps_akm, value, sizeof(ui->wps_akm));
			else if (!strcmp(name, "wps_crypto"))
				wps_strncpy(ui->wps_crypto, value, sizeof(ui->wps_crypto));
			else if (!strcmp(name, "wps_psk"))
				wps_strncpy(ui->wps_psk, value, sizeof(ui->wps_psk));
			else if (!strcmp(name, "wps_enr_wsec"))
				wps_strncpy(ui->wps_enr_wsec, value, sizeof(ui->wps_enr_wsec));
			else if (!strcmp(name, "wps_enr_ssid"))
				wps_strncpy(ui->wps_enr_ssid, value, sizeof(ui->wps_enr_ssid));
			else if (!strcmp(name, "wps_enr_bssid"))
				wps_strncpy(ui->wps_enr_bssid, value, sizeof(ui->wps_enr_bssid));
			else if (!strcmp(name, "wps_enr_scan"))
				ui->wps_enr_scan = atoi(value);
#ifdef __CONFIG_WFI__
			else if (!strcmp(name, "wps_device_pin"))
				strcpy(ui->wps_device_pin, value);
#endif /* __CONFIG_WFI__ */
			else if (!strcmp(name, "wps_autho_sta_mac")) /* WSC 2.0 */
				wps_strncpy(ui->wps_autho_sta_mac, value,
					sizeof(ui->wps_autho_sta_mac));
			else if (!strcmp(name, "wps_stareg_ap_pin"))
				wps_strncpy(ui->wps_stareg_ap_pin, value,
					sizeof(ui->wps_stareg_ap_pin));
			else if (!strcmp(name, "wps_scstate"))
				wps_strncpy(ui->wps_scstate, value, sizeof(ui->wps_scstate));
		}
	}

	return 0;
}

static int
wps_ui_do_set(char *buf)
{
	int ret = WPS_CONT;
	wps_ui_t a_ui;
	wps_ui_t *ui = &a_ui;
	wps_app_t *wps_app = get_wps_app();

	/* Process set command */
	memset(ui, 0, sizeof(wps_ui_t));
	if (wps_ui_parse_env(buf, ui) != 0)
		return WPS_CONT;

#ifdef _TUDEBUGTRACE
	wps_ui_dump(ui);
#endif

	/* user click STOPWPS button or pending expire */
	if (ui->wps_config_command == WPS_UI_CMD_STOP) {

#ifdef WPS_ADDCLIENT_WWTP
		/* stop add client 2 min. window */
		wps_ui_close_addclient_window();
		wps_close_addclient_window();
#endif /* WPS_ADDCLIENT_WWTP */

		/* state maching in progress, check stop or push button here */
		wps_close_session();
		wps_setProcessStates(WPS_INIT);

		/*
		  * set wps LAN all leds to normal and wps_pb_state
		  * when session timeout or user stop
		  */
		wps_pb_state_reset();
	}
#ifdef WPS_NFC_DEVICE
	else if (ui->wps_config_command == WPS_UI_CMD_NFC_STOP) {
		wps_nfc_stop(ui->wps_ifname);
		wps_close_session();
	}
	else if (ui->wps_config_command == WPS_UI_CMD_NFC_WR_CFG ||
		ui->wps_config_command == WPS_UI_CMD_NFC_RD_CFG ||
		ui->wps_config_command == WPS_UI_CMD_NFC_WR_PW ||
		ui->wps_config_command == WPS_UI_CMD_NFC_RD_PW ||
		ui->wps_config_command == WPS_UI_CMD_NFC_HO_S ||
		ui->wps_config_command == WPS_UI_CMD_NFC_HO_R) {
		/* Close current session if any */
		if (wps_app->wksp && !pending_flag)
			wps_close_session();

		/* Fine to accept set command */
		memcpy(wps_ui, ui, sizeof(*ui));

		if (wps_ui->wps_config_command == WPS_UI_CMD_NFC_WR_CFG)
			ret = wps_ui_nfc_write_cfg();
		else if (wps_ui->wps_config_command == WPS_UI_CMD_NFC_RD_CFG)
			ret = wps_nfc_read_cfg(wps_ui->wps_ifname);
		else if (wps_ui->wps_config_command == WPS_UI_CMD_NFC_WR_PW)
			ret = wps_ui_nfc_write_pw();
		else if (wps_ui->wps_config_command == WPS_UI_CMD_NFC_RD_PW)
			ret = wps_nfc_read_pw(wps_ui->wps_ifname);
		else if (wps_ui->wps_config_command == WPS_UI_CMD_NFC_HO_S)
			ret = wps_ui_nfc_ho_selector(wps_ui->wps_ifname);
		else if (wps_ui->wps_config_command == WPS_UI_CMD_NFC_HO_R)
			ret = wps_nfc_ho_requester(wps_ui->wps_ifname);
	}
#endif /* WPS_NFC_DEVICE */
	else {
		/* Add in PF #3 */
		if (ui->wps_config_command == WPS_UI_CMD_START &&
		    ui->wps_pbc_method == WPS_UI_PBC_SW &&
		    (wps_app->wksp || pending_flag)) {
			/* close session if session opend or in pending state */
			int old_action;

			old_action = ui->wps_action;
			wps_close_session();
			ui->wps_config_command = WPS_UI_CMD_START;
			ui->wps_action = old_action;
		}

		/* for WPS_AP_M2D */
		if ((!wps_app->wksp || WPS_IS_PROXY(wps_app->sc_mode)) &&
		    !pending_flag) {
			/* Fine to accept set command */
			memcpy(wps_ui, ui, sizeof(*ui));

			/* some button in UI is pushed */
			if (wps_ui->wps_config_command == WPS_UI_CMD_START) {
				wps_setProcessStates(WPS_INIT);

				/* if proxy in process, stop it */
				if (wps_app->wksp) {
					wps_close_session();
					wps_ui->wps_config_command = WPS_UI_CMD_START;
					wps_ui->wps_action = ui->wps_action;
				}

				if (wps_is_wps_sta(wps_ui->wps_ifname)) {
					/* set what interface you want */
#ifdef BCMWPSAPSTA
					/* set bss to up */
					wps_wl_bss_config(wps_safe_get_conf("wps_pbc_sta_ifname"),
						1);

					/* set the process status */
					wps_setProcessStates(WPS_ASSOCIATED);
					ret = wpssta_open_session(wps_app, wps_ui->wps_ifname);
					if (ret != WPS_CONT) {
						/* Normally it cause by associate time out */
						wps_setProcessStates(WPS_TIMEOUT);
					}
#endif /* BCMWPSAPSTA */

				}
#ifdef BCMWPSAP
				/* for wps_ap application */
				else {
					/* set the process status */
					wps_setProcessStates(WPS_ASSOCIATED);

					if (wps_ui->wps_action == WPS_UI_ACT_CONFIGAP) {
						/* TBD - Initial all relative nvram setting */
						char tmptr[] = "wlXXXXXXXXXX_hwaddr";
						char *macaddr;
						char *pre_privkey = NULL;

						sprintf(tmptr, "%s_hwaddr", wps_ui->wps_ifname);
						macaddr = wps_get_conf(tmptr);
						if (macaddr) {
							unsigned char ifmac[SIZE_6_BYTES];
							ether_atoe(macaddr, ifmac);
#ifdef WPS_NFC_DEVICE
							pre_privkey = (char *)wps_nfc_priv_key();
#endif

							/* WSC 2.0, Request to Enroll TRUE */
							ret = wpsap_open_session(wps_app,
								SCMODE_AP_ENROLLEE, ifmac, NULL,
								wps_ui->wps_ifname, NULL,
								pre_privkey, NULL, 0, 1, 0);
						}
						else {
							TUTRACE((TUTRACE_INFO, "can not find mac "
								"on %s\n", wps_ui->wps_ifname));
						}
					}
					else if (wps_ui->wps_action == WPS_UI_ACT_ADDENROLLEE) {
						/* Do nothing, wait for client connect */
						wps_ui_start_pending(wps_ui->wps_ifname);
					}
				}

#else
				TUTRACE((TUTRACE_INFO, "AP functionality not included.\n"));
#endif /* BCMWPSAP */
			}
		}
	}

	return ret;
}

int
wps_ui_process_msg(char *buf, int buflen)
{
	if (strncmp(buf, "GET", 3) == 0) {
		wps_ui_do_get();
	}

	if (strncmp(buf, "SET ", 4) == 0) {
		return wps_ui_do_set(buf+4);
	}

	return WPS_CONT;
}

int
wps_ui_init()
{
	/* Init eap_hndl */
	memset(&ui_hndl, 0, sizeof(ui_hndl));

	ui_hndl.type = WPS_RECEIVE_PKT_UI;
	ui_hndl.handle = wps_osl_ui_handle_init();
	if (ui_hndl.handle == -1)
		return -1;

	wps_hndl_add(&ui_hndl);
	return 0;
}

void
wps_ui_deinit()
{
	wps_hndl_del(&ui_hndl);
	wps_osl_ui_handle_deinit(ui_hndl.handle);
	return;
}
