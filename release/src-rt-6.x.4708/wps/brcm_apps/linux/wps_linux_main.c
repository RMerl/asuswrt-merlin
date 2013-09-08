/*
 * WPS main (platform dependent portion)
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_linux_main.c 404730 2013-05-29 06:26:20Z $
 */
#include <stdio.h>
#include <linux/types.h>
#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <typedefs.h>
#include <linux/sockios.h>
typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;
#include <linux/ethtool.h>

#include <bn.h>
#include <wps_dh.h>
#include <md5.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <bcmnvram.h>
#include <wpstypes.h>
#include <sminfo.h>
#include <wps_apapi.h>

#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <wlutils.h>
#include <tutrace.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <shutils.h>
#ifdef WPS_UPNP_DEVICE
#include <ap_upnp_sm.h>
#endif
#include <wlif_utils.h>
#include <bcmparams.h>
#include <security_ipc.h>
#include <wps_ui.h>
#include <wps.h>
#include <wps_wl.h>

#define WPSM_PID_FILE_PATH	"/tmp/wps_monitor.pid"

extern void RAND_linux_init();

char wps_conf_buf[1024 * 4];
char *wps_conf_buf_ptr;
static int wps_conf_num;
static char *wps_conf_list[256];


void
wps_osl_restart_wl()
{
	/* Wait for M8/DONE packet completed */
	WpsSleep(1);

#if 0
	/* restart all process */
	system("kill -1 1");
#else
        /* restart wps only */
        system("restart_wireless");
#endif
}

int
wps_getProcessStates()
{
	int value;

	value = atoi(nvram_safe_get("wps_proc_status"));
	switch (value) {
	case WPS_UI_INIT:		value = WPS_INIT;		break;
	case WPS_UI_ASSOCIATED:		value = WPS_ASSOCIATED;		break;
	case WPS_UI_OK:			value = WPS_OK;			break;
	case WPS_UI_MSG_ERR:		value = WPS_MSG_ERR;		break;
	case WPS_UI_TIMEOUT:		value = WPS_TIMEOUT;		break;
	case WPS_UI_SENDM2:		value = WPS_SENDM2;		break;
	case WPS_UI_SENDM7:		value = WPS_SENDM7;		break;
	case WPS_UI_MSGDONE:		value = WPS_MSGDONE;		break;
	case WPS_UI_PBCOVERLAP:		value = WPS_PBCOVERLAP;		break;
	case WPS_UI_FIND_PBC_AP:	value = WPS_FIND_PBC_AP;	break;
	case WPS_UI_ASSOCIATING:	value = WPS_ASSOCIATING;	break;
	case WPS_UI_NFC_WR_CFG:		value = WPS_NFC_WR_CFG;		break;
	case WPS_UI_NFC_WR_PW:		value = WPS_NFC_WR_PW;		break;
	case WPS_UI_NFC_WR_CPLT:	value = WPS_NFC_WR_CPLT;	break;
	case WPS_UI_NFC_RD_CFG:		value = WPS_NFC_RD_CFG;		break;
	case WPS_UI_NFC_RD_PW:		value = WPS_NFC_RD_PW;		break;
	case WPS_UI_NFC_RD_CPLT:	value = WPS_NFC_RD_CPLT;	break;
	case WPS_UI_NFC_HO_S:		value = WPS_NFC_HO_S;		break;
	case WPS_UI_NFC_HO_R:		value = WPS_NFC_HO_R;		break;
	case WPS_UI_NFC_HO_NDEF:	value = WPS_NFC_HO_NDEF;	break;
	case WPS_UI_NFC_HO_CPLT:	value = WPS_NFC_HO_CPLT;	break;
	case WPS_UI_NFC_OP_ERROR:	value = WPS_NFC_OP_ERROR;	break;
	case WPS_UI_NFC_OP_STOP:	value = WPS_NFC_OP_STOP;	break;
	case WPS_UI_NFC_OP_TO:		value = WPS_NFC_OP_TO;		break;
	case WPS_UI_NFC_FM:		value = WPS_NFC_FM;		break;
	case WPS_UI_NFC_FM_CPLT:	value = WPS_NFC_FM_CPLT;	break;
	default:
		TUTRACE((TUTRACE_ERR, "Unknow nv process state %d\n", value));
		value = WPS_INIT;
	}

	return value;
}

void
wps_setProcessStates(int state)
{
	int value;
	char buf[32];

	switch (state) {
	case WPS_INIT:		value = WPS_UI_INIT;		break;
	case WPS_ASSOCIATED:	value = WPS_UI_ASSOCIATED;	break;
	case WPS_OK:		value = WPS_UI_OK;		break;
	case WPS_MSG_ERR:	value = WPS_UI_MSG_ERR;		break;
	case WPS_TIMEOUT:	value = WPS_UI_TIMEOUT;		break;
	case WPS_SENDM2:	value = WPS_UI_SENDM2;		break;
	case WPS_SENDM7:	value = WPS_UI_SENDM7;		break;
	case WPS_MSGDONE:	value = WPS_UI_MSGDONE;		break;
	case WPS_PBCOVERLAP:	value = WPS_UI_PBCOVERLAP;	break;
	case WPS_FIND_PBC_AP:	value = WPS_UI_FIND_PBC_AP;	break;
	case WPS_ASSOCIATING:	value = WPS_UI_ASSOCIATING;	break;
	case WPS_NFC_WR_CFG:	value = WPS_UI_NFC_WR_CFG;	break;
	case WPS_NFC_WR_PW:	value = WPS_UI_NFC_WR_PW;	break;
	case WPS_NFC_WR_CPLT:	value = WPS_UI_NFC_WR_CPLT;	break;
	case WPS_NFC_RD_CFG:	value = WPS_UI_NFC_RD_CFG;	break;
	case WPS_NFC_RD_PW:	value = WPS_UI_NFC_RD_PW;	break;
	case WPS_NFC_RD_CPLT:	value = WPS_UI_NFC_RD_CPLT;	break;
	case WPS_NFC_HO_S:	value = WPS_UI_NFC_HO_S;	break;
	case WPS_NFC_HO_R:	value = WPS_UI_NFC_HO_R;	break;
	case WPS_NFC_HO_NDEF:	value = WPS_UI_NFC_HO_NDEF;	break;
	case WPS_NFC_HO_CPLT:	value = WPS_UI_NFC_HO_CPLT;	break;
	case WPS_NFC_OP_ERROR:	value = WPS_UI_NFC_OP_ERROR;	break;
	case WPS_NFC_OP_STOP:	value = WPS_UI_NFC_OP_STOP;	break;
	case WPS_NFC_OP_TO:	value = WPS_UI_NFC_OP_TO;	break;
	case WPS_NFC_FM:	value = WPS_UI_NFC_FM;		break;
	case WPS_NFC_FM_CPLT:	value = WPS_UI_NFC_FM_CPLT;	break;
	default:
		TUTRACE((TUTRACE_ERR, "Unknow process state %d\n", state));
		return;
	}
	sprintf(buf, "%d", value);
	nvram_set("wps_proc_status", buf);

#ifdef WPS_NFC_DEVICE
	/* Add external NFC Token bit */
	if (state == WPS_NFC_WR_CPLT) {
		value = strtoul(nvram_safe_get("wps_config_method"), NULL, 16);

		if (!(value & WPS_CONFMET_EXT_NFC_TOK)) {
			value |= WPS_CONFMET_EXT_NFC_TOK;
			sprintf(buf, "%d", value);
			nvram_set("wps_config_method", buf);
		}
	}
#endif /* WPS_NFC_DEVICE */
	return;
}

void
wps_setStaDevName(unsigned char *str)
{
	if (str) {
		wps_ui_set_env("wps_sta_devname", str);
		nvram_set("wps_sta_devname", str);
	}

	return;
}

void
wps_setPinFailInfo(uint8 *mac, char *name, char *state)
{
	char buf[32] = {0};

	if (mac)
		nvram_set("wps_pinfail_mac", ether_etoa(mac, buf));
	else
		nvram_set("wps_pinfail_mac", "");

	if (name)
		nvram_set("wps_pinfail_name", name);
	else
		nvram_set("wps_pinfail_name", "");

	if (state) {
		wps_ui_set_env("wps_pinfail_state", state);
		nvram_set("wps_pinfail_state", state);
	}
	else {
		wps_ui_set_env("wps_pinfail_state", "");
		nvram_set("wps_pinfail_state", "");
	}
}

void
wps_setWPSSuccessMode(int state)
{
	char buf[32];

	sprintf(buf, "%d", state);
	nvram_set("wps_success_mode", buf);
	return;
}

static int
findstr(char *s, char *t)
{
	int len_t = strlen(t);
	int len_s = strlen(s);
	int i;

	if (len_t > len_s)
		return 0;
	for (i = 0; i <= (len_s - len_t); i++) {
		if (*(s+i) == *t)
			if (strstr(s+i, t))
				return 1;
	}

	return 0;
}

static int
print_hex(char *str, const void *bytes, int len)
{
	int i;
	char *p = str;
	const uint8 *src = (const uint8*)bytes;

	for (i = 0; i < len; i++) {
		p += snprintf(p, 3, "%02X", *src);
		src++;
	}
	return (int)(p - str);
}

static int
set_wep_key(char *wl_keyi, char *key, int key_len)
{
	char keystr[WEP128_KEY_HEX_SIZE+1] = {0};


	switch (key_len) {
	case WEP1_KEY_SIZE:
	case WEP128_KEY_SIZE:
		print_hex(keystr, key, key_len);
		break;
	case WEP1_KEY_HEX_SIZE:
	case WEP128_KEY_HEX_SIZE:
		memcpy(keystr, key, key_len);
		break;
	default:
		TUTRACE((TUTRACE_ERR, "Wrong Key length %d\n", key_len));
		return -1;
	}

	wps_osl_set_conf(wl_keyi, keystr);

	return 0;
}

static int
set_wsec(char *ifname, void *credential, int mode)
{
	char tmp[128];
	unsigned char psk_mode = 0;
	WpsEnrCred *cred = (WpsEnrCred *)credential;
	char prefix[] = "wlXXXXXXXXXX_";
	bool b_wps_version2 = false;
	char *value;

	value = nvram_get("wps_version2");
	if (value && !strcmp(value, "enabled"))
		b_wps_version2 = true;

	/* empty credential check */
	if (cred->ssidLen == 0) {
		TUTRACE((TUTRACE_INFO, "Ignore apply new credential because ssid is empty\n"));
		return 0;
	}

	TUTRACE((TUTRACE_INFO,
		"nvram set key = %s keyMgmt = %s ssid = %s(b_configured)\n",
		cred->nwKey, cred->keyMgmt, cred->ssid));

	/* convert os name to wl name */
	if (osifname_to_nvifname(ifname, prefix, sizeof(prefix)) != 0) {
		TUTRACE((TUTRACE_INFO, "Convert to nvname failed\n"));
		return 0;
	}
	strcat(prefix, "_");

	/* Check credential */
	if (findstr(cred->keyMgmt, "WPA-PSK"))
		psk_mode |= 1;
	if (findstr(cred->keyMgmt, "WPA2-PSK"))
		psk_mode |= 2;

	/* for version 2, force psk2 if psk1 is on */
	if (b_wps_version2 && (psk_mode & 1)) {
		psk_mode |= 2;
	}

	switch (psk_mode) {
	case 1:
		wps_osl_set_conf(strcat_r(prefix, "akm", tmp), "psk ");
		wps_osl_set_conf(strcat_r(prefix, "auth_mode_x", tmp), "psk");
		break;
	case 2:
		wps_osl_set_conf(strcat_r(prefix, "akm", tmp), "psk2 ");
		wps_osl_set_conf(strcat_r(prefix, "auth_mode_x", tmp), "psk2");
		break;
	case 3:
		wps_osl_set_conf(strcat_r(prefix, "akm", tmp), "psk psk2 ");
		wps_osl_set_conf(strcat_r(prefix, "auth_mode_x", tmp), "pskpsk2");
		break;
	default:
		wps_osl_set_conf(strcat_r(prefix, "akm", tmp), "");
		wps_osl_set_conf(strcat_r(prefix, "auth_mode_x", tmp), "open");
		break;
	}

	if (findstr(cred->keyMgmt, "SHARED"))
	{
		wps_osl_set_conf(strcat_r(prefix, "auth", tmp), "1");
		wps_osl_set_conf(strcat_r(prefix, "auth_mode_x", tmp), "shared");
	}
	else
		wps_osl_set_conf(strcat_r(prefix, "auth", tmp), "0");

	/* set SSID */
	wps_osl_set_conf(strcat_r(prefix, "ssid", tmp), cred->ssid);
	if (psk_mode)
	{
		wps_osl_set_conf(strcat_r(prefix, "wep", tmp), "disabled");
		wps_osl_set_conf(strcat_r(prefix, "wep_x", tmp), "0");
	}
	/* for version 2, force aes if tkip is on */
	if (b_wps_version2 && (cred->encrType & WPS_ENCRTYPE_TKIP)) {
		cred->encrType |= WPS_ENCRTYPE_AES;
	}

	/* set Encr type */
	if (cred->encrType == WPS_ENCRTYPE_NONE)
	{
		wps_osl_set_conf(strcat_r(prefix, "wep", tmp), "disabled");
		wps_osl_set_conf(strcat_r(prefix, "wep_x", tmp), "0");
	}
	else if (cred->encrType == WPS_ENCRTYPE_WEP)
		wps_osl_set_conf(strcat_r(prefix, "wep", tmp), "enabled");
	else if (cred->encrType == WPS_ENCRTYPE_TKIP)
		wps_osl_set_conf(strcat_r(prefix, "crypto", tmp), "tkip");
	else if (cred->encrType == WPS_ENCRTYPE_AES)
		wps_osl_set_conf(strcat_r(prefix, "crypto", tmp), "aes");
	else if (cred->encrType == (WPS_ENCRTYPE_TKIP | WPS_ENCRTYPE_AES))
		wps_osl_set_conf(strcat_r(prefix, "crypto", tmp), "tkip+aes");
	else
		wps_osl_set_conf(strcat_r(prefix, "crypto", tmp), "tkip");

	if (cred->encrType == WPS_ENCRTYPE_WEP) {
		char buf[16] = {0};
		sprintf(buf, "%d", cred->wepIndex);
		wps_osl_set_conf(strcat_r(prefix, "key", tmp), buf);
		sprintf(buf, "key%d", cred->wepIndex);
		set_wep_key(strcat_r(prefix, buf, tmp), cred->nwKey, cred->nwKeyLen);
                if ((cred->nwKeyLen == 5) || (cred->nwKeyLen == 10))
                        wps_osl_set_conf(strcat_r(prefix, "wep_x", tmp), "1");
                else
                        wps_osl_set_conf(strcat_r(prefix, "wep_x", tmp), "2");
	}
	else {
		/* set key */
		if (cred->nwKeyLen < 64) {
			wps_osl_set_conf(strcat_r(prefix, "wpa_psk", tmp), cred->nwKey);
		}
		else {
			char temp_key[65] = {0};
			memcpy(temp_key, cred->nwKey, 64);
			temp_key[64] = 0;
			wps_osl_set_conf(strcat_r(prefix, "wpa_psk", tmp), temp_key);
		}
	}

	/* Disable nmode for WEP and TKIP for TGN spec */
	switch (cred->encrType) {
	case WPS_ENCRTYPE_WEP:
	case WPS_ENCRTYPE_TKIP:
		wps_osl_set_conf(strcat_r(prefix, "nmode", tmp), "0");
		break;
	default:
		wps_osl_set_conf(strcat_r(prefix, "nmode", tmp), "-1");
		break;
	}

	nvram_set("w_Setting", "1");

	return 1;
}

int
wps_set_wsec(int ess_id, char *wps_ifname, void *credential, int mode)
{
	char prefix[] = "lanXXXXXXXXX_";
	int instance;
	char *value;
	char *oob = NULL;

	char tmp[100];
	char *wlnames = wps_ifname;
	char *next = NULL;
	char ifname[IFNAMSIZ];

	if (wps_ifname == NULL || credential == NULL) {
		TUTRACE((TUTRACE_ERR, "wps_set_wsec: Invaild argument\n"));
		return -1;
	}
#ifdef BCMWPSAPSTA
	/* only support on URE WRE mode */
	snprintf(tmp, sizeof(tmp), "%s_ure_mode", wps_ifname);
	value = wps_safe_get_conf(tmp);
	if (strcmp(value, "wre") == 0 &&
		strcmp(wps_safe_get_conf("wps_pbc_apsta"), "enabled") == 0 &&
		strcmp(wps_ifname, wps_safe_get_conf("wps_pbc_sta_ifname")) == 0) {

		char *ap_ifname = wps_safe_get_conf("wps_pbc_ap_ifname");

		/* Set wsec to ap interface */

		value = wps_get_conf("wps_pbc_ap_index");
		if (value == NULL) {
			TUTRACE((TUTRACE_ERR, "wps_set_wsec: Cannot get UPnP instance\n"));
			return -1;
		}

		instance = atoi(value);
		if (instance == 0)
			strcpy(prefix, "lan_");
		else
			sprintf(prefix, "lan%d_", instance);
		wps_osl_set_conf(strcat_r(prefix, "wps_oob", tmp),
			"disabled");

		wps_osl_set_conf(strcat_r(prefix, "wps_reg", tmp),
			"enabled");
		TUTRACE((TUTRACE_INFO, "built-in registrar enabled\n"));

		wlnames = ap_ifname;
		TUTRACE((TUTRACE_INFO, "WPS STA (%s) has finished WPS process "
			"and now is setting security to AP (%s).\n", wps_ifname, ap_ifname));
	}
	else
#endif /* BCMWPSAPSTA */
	if (!wps_is_wps_sta(wps_ifname)) {
		sprintf(tmp, "ess%d_ap_index", ess_id);
		value = wps_get_conf(tmp);
		if (value == NULL) {
			TUTRACE((TUTRACE_ERR, "wps_set_wsec: Cannot get UPnP instance\n"));
			return -1;
		}

		instance = atoi(value);
		if (instance == 0)
			strcpy(prefix, "lan_");
		else
			sprintf(prefix, "lan%d_", instance);

		/* Default OOB set to enabled */
		oob = nvram_get(strcat_r(prefix, "wps_oob", tmp));
		if (!oob)
			oob = "enabled";

		/* Check wether OOB is true */
		if (!strcmp(oob, "enabled")) {
			/* OOB mode, apply to all wl if */
			sprintf(tmp, "ess%d_wlnames", ess_id);
			wlnames = wps_safe_get_conf(tmp);

			TUTRACE((TUTRACE_INFO,
				"wps_set_wsec: OOB set config\n"));
		}

		/* Set OOB and built-in reg (Per-ESS) */
		if (mode == SCMODE_AP_REGISTRAR) {
			wps_osl_set_conf(strcat_r(prefix, "wps_oob", tmp),
				"disabled");
			TUTRACE((TUTRACE_INFO, "OOB state configed\n"));

			wps_osl_set_conf(strcat_r(prefix, "wps_reg", tmp),
				"enabled");
			TUTRACE((TUTRACE_INFO, "built-in registrar enabled\n"));
		} else if (mode == SCMODE_AP_ENROLLEE) {
			wps_osl_set_conf(strcat_r(prefix, "wps_oob", tmp),
				"disabled");
			TUTRACE((TUTRACE_INFO, "OOB state configed\n"));
		}
	}
	else {
		/* STA ess interfaces */
		if (osifname_to_nvifname(wlnames, prefix, sizeof(prefix)) == 0) {
			strcat(prefix, "_");
			wps_osl_set_conf(strcat_r(prefix, "wps_oob", tmp), "disabled");
		}
		else {
			TUTRACE((TUTRACE_INFO, "Clear OOB: convert to nvname failed!\n"));
		}
	}

	/* Apply the wsec */
	foreach(ifname, wlnames, next) {
		set_wsec(ifname, credential, mode);
		TUTRACE((TUTRACE_INFO, "wps_set_wsec: Set config to %s\n", ifname));
	}

	/*
	 * Do commit
	 */
	nvram_commit();

	TUTRACE((TUTRACE_INFO, "wps: wsec configuraiton set completed\n"));
	return 1;
}

int
wps_osl_set_conf(char *config_name, char *config_string)
{
	if (config_name && config_string) {
		fprintf(stderr, "WPS nvram set %s = %s\n", config_name, config_string);

		TUTRACE((TUTRACE_INFO,	"wps_osl_set_conf: Set %s = %s\n",
			config_name, config_string));
		nvram_set(config_name, config_string);
	}

	return 0;
}

static char *
print_conf(char *os_ifname, char *nv_ifname, char *name, char *value)
{
	char os_prefix[] = "wlXXXXXXXXXX_";
	char nv_prefix[] = "wlXXXXXXXXXX_";
	char tmp[100];

	/* Set up os prefix */
	if (os_ifname)
		snprintf(os_prefix, sizeof(os_prefix), "%s_", os_ifname);
	else
		os_prefix[0] = 0;

	/* Set up nv prefix */
	if (nv_ifname)
		snprintf(nv_prefix, sizeof(nv_prefix), "%s_", nv_ifname);
	else
		nv_prefix[0] = 0;

	/* If value is null, get from NVRAM. */
	if (value == NULL) {
		value = nvram_get(strcat_r(nv_prefix, name, tmp));
		if (!value)
			return NULL;
	}

	wps_conf_buf_ptr += sprintf(wps_conf_buf_ptr, "%s%s=%s\n",
		os_prefix, name, value);
	TUTRACE((TUTRACE_INFO, "%s%s=%s\n", os_prefix, name, value));
	return value;
}

static unsigned char*
create_device_uuid_string(unsigned char *uuid_string)
{
	unsigned char mac[6] = {0};
	char deviceType[] = "urn:schemas-wifialliance-org:device:WFADevice";
	char *lanmac_str;
	MD5_CTX mdContext;
	unsigned char uuid[16];

	lanmac_str = nvram_get("lan_hwaddr");
	if (lanmac_str)
		ether_atoe(lanmac_str, mac);

	/* Generate hash */
	MD5Init(&mdContext);
	MD5Update(&mdContext, mac, 6);
	MD5Update(&mdContext, deviceType, strlen(deviceType));
	MD5Final(uuid, &mdContext);

	sprintf(uuid_string, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

	return uuid_string;
}

static void
wps_osl_validate_default()
{
	int dirty = 0;
	unsigned short config_method, config_method_org, config_method_v1;
	char *value, tmp[128], devPwd[9];
	bool b_wps_version2 = false;


	/* Validate wps_device_pin existance,  embedded nvram can override it */
#if 0
	value = nvram_get("wps_device_pin");
	if (!value || strcmp(value, "12345670") == 0) {
		/* Generate a random device's PIN */
		if (wps_gen_pin(devPwd, sizeof(devPwd)) == WPS_SUCCESS)
			nvram_set("wps_device_pin", devPwd);
		else {
			TUTRACE((TUTRACE_ERR, "Generate new random AP PIN failed\n"));
			nvram_set("wps_device_pin", "12345670");
		}
		dirty++;
	}
#endif
	/* WSC 2.0 support */
	value = nvram_get("wps_version2");
	if (value && !strcmp(value, "enabled"))
		b_wps_version2 = true;

	/* Set wps_config_method default value */
	value = nvram_get("wps_config_method");
	if (!value) {
		if (b_wps_version2) {
			config_method = (WPS_CONFMET_VIRT_PBC | WPS_CONFMET_PHY_PBC |
				WPS_CONFMET_VIRT_DISPLAY | WPS_CONFMET_PBC |
				WPS_CONFMET_DISPLAY);
		}
		else {
			/* For DTM 1.1 test */
			/* config_method = (WPS_CONFMET_PBC | WPS_CONFMET_DISPLAY); */
			config_method = (WPS_CONFMET_PBC | WPS_CONFMET_LABEL);
		}

#ifdef WPS_NFC_DEVICE
			config_method |= WPS_CONFMET_NFC_INTF;
#endif

		sprintf(tmp, "0x%x", config_method);
		nvram_set("wps_config_method", tmp);
		dirty++;
	}
	else {
		config_method = strtoul(value, NULL, 16);
		if (b_wps_version2) {
			config_method_org = config_method;

			/* We expect that add virtual PBC if PBC supported,
			 * add virtual DISPLAY if DISPLAY supported.
			 */
			if (config_method & WPS_CONFMET_PBC)
				config_method |= WPS_CONFMET_VIRT_PBC;
			if (config_method & WPS_CONFMET_DISPLAY)
				config_method |= WPS_CONFMET_VIRT_DISPLAY;

			if (config_method != config_method_org) {
				sprintf(tmp, "0x%x", config_method);
				nvram_set("wps_config_method", tmp);
				dirty++;
			}
		}
		else {
			/* Is it compatible with V1 */
			config_method_v1 = config_method;
			config_method_v1 &= ~(0x200 | 0x400 | 0x2000 | 0x4000);

			if (config_method != config_method_v1) {
				sprintf(tmp, "0x%x", config_method_v1);
				nvram_set("wps_config_method", tmp);
				dirty++;
			}
		}
	}

	/* Commit */
	if (dirty)
		nvram_commit();
}

static bool
is_wps_pbc_apsta_enabled(int ess_num, char *ap_ifname, char *sta_ifname)
{
	if (strcmp(nvram_safe_get("wps_pbc_apsta"), "enabled") != 0)
		return FALSE;

	if (ess_num == 2 &&
	    nvram_match("ure_disable", "0") &&
	    strcmp(ap_ifname, "") != 0 &&
	    strcmp(sta_ifname, "") != 0) {
		TUTRACE((TUTRACE_INFO, "%s : wps_pbc_apsta enabled, ap ifname = %s\n",
			nvram_match("router_disable", "0")? "TR":"URE", ap_ifname));
		TUTRACE((TUTRACE_INFO, "%s : wps_pbc_apsta enabled, sta ifname = %s\n",
			nvram_match("router_disable", "0")? "TR":"URE", sta_ifname));
		return  TRUE;
	}

	return FALSE;
}

void
wps_osl_build_conf()
{
	int i, unit;
	char *value;
	char *item, *p, *next;
#ifdef WPS_UPNP_DEVICE
	char *ifname;
	char lan_ifname[IFNAMSIZ];
#endif
	char *ifnames;
	char wlnames[256];
	char stanames[256];
	char prefix[] = "wlXXXXXXXXXX_";
	char name[IFNAMSIZ], os_name[IFNAMSIZ], wl_name[IFNAMSIZ];
	char ess_name[IFNAMSIZ], lan_name[IFNAMSIZ];
	char tmp[100];
	int bandtype = 0;
	int band = 0;
	char key[8];
	int ess_id = 0;
	int ap_index;
	char *wl_mode;
	char *wl_radio, *wl_bss_enabled;
	unsigned char uuid_string[36];
	char wps_pbc_ap_ifname[IFNAMSIZ] = {0};
	char wps_pbc_sta_ifname[IFNAMSIZ] = {0};
	int wps_pbc_ap_index = 0;


	/* Init print pointer */
	wps_conf_buf_ptr = wps_conf_buf;

	/* Start config print */
	print_conf(NULL, NULL, "wps_aplockdown_forceon", NULL);
	print_conf(NULL, NULL, "wps_lock_start_cnt", NULL);
	print_conf(NULL, NULL, "wps_lock_forever_cnt", NULL);
	print_conf(NULL, NULL, "wps_device_pin", NULL);
	print_conf(NULL, NULL, "wps_mixedmode", NULL);
	print_conf(NULL, NULL, "wps_random_ssid_prefix", NULL);
	print_conf(NULL, NULL, "wps_randomssid", NULL);
	print_conf(NULL, NULL, "wps_randomkey", NULL);
	print_conf(NULL, NULL, "wps_device_name", NULL);
	print_conf(NULL, NULL, "wps_mfstring", NULL);
	print_conf(NULL, NULL, "wps_modelname", NULL);
	print_conf(NULL, NULL, "wps_modelnum", NULL);
	print_conf(NULL, NULL, "boardnum", NULL);
	print_conf(NULL, NULL, "wps_config_method", NULL);
	print_conf(NULL, NULL, "wps_wer_mode", NULL);
	print_conf(NULL, NULL, "wps_wer_override", NULL);
	print_conf(NULL, NULL, "wps_oob_configured", NULL);

#ifdef WPS_NFC_DEVICE
	print_conf(NULL, NULL, "nfc_sleep1", NULL);
	print_conf(NULL, NULL, "nfc_sleep2", NULL);
#endif

#ifdef WPS_UPNP_DEVICE
	print_conf(NULL, NULL, "wfa_port", NULL);
	print_conf(NULL, NULL, "wfa_adv_time", NULL);
#endif
	/* Support WSC 2.0 or not */
	print_conf(NULL, NULL, "wps_version2", NULL);
	if (print_conf(NULL, NULL, "wps_version2_num", NULL) == NULL) {
		snprintf(tmp, sizeof(tmp), "0x%x", WPS_VERSION2);
		print_conf(NULL, NULL, "wps_version2_num", tmp);
	}

	/* For testing purpose */
#ifdef WFA_WPS_20_TESTBED
	/* WPS IE fragment threshold */
	print_conf(NULL, NULL, "wps_ie_frag", NULL);
	/* WPS EAP fragment threshold */
	print_conf(NULL, NULL, "wps_eap_frag", NULL);
	/* New attribute */
	print_conf(NULL, NULL, "wps_nattr", NULL);
	/* Disable Null-Termination */
	print_conf(NULL, NULL, "wps_zpadding", NULL);
	/* Multiple Credential Attribute */
	print_conf(NULL, NULL, "wps_mca", NULL);
	/*
	 * Update partial embedded WPS IE in beacon /probe req /probe resp /
	 * associate req and associate resp
	 */
	print_conf(NULL, NULL, "wps_beacon", NULL);
	print_conf(NULL, NULL, "wps_prbreq", NULL);
	print_conf(NULL, NULL, "wps_prbrsp", NULL);
	print_conf(NULL, NULL, "wps_assocreq", NULL);
	print_conf(NULL, NULL, "wps_assocrsp", NULL);
#endif /* WFA_WPS_20_TESTBED */

	/* Set device UUID */
	print_conf(NULL, NULL, "wps_uuid", create_device_uuid_string(uuid_string));
	/* Set WPS M1 Pairing */
	print_conf(NULL, NULL, "wps_pairing_auth", NULL);
	print_conf(NULL, NULL, "wps_sta_device_name", NULL);

	ess_id = 0;
	/* (LAN) for UPnP and push button */
	for (i = 0; i < 256; i ++) {
		/* Taking care of LAN interface names */
		if (i == 0) {
			strcpy(name, "lan_ifnames");
			strcpy(lan_name, "lan");
		}
		else {
			sprintf(name, "lan%d_ifnames", i);
			sprintf(lan_name, "lan%d", i);
		}

		ifnames = nvram_get(name);
		if (!ifnames)
			continue;

#ifdef WPS_UPNP_DEVICE
		if (i == 0)
			strcpy(lan_ifname, "lan_ifname");
		else
			sprintf(lan_ifname, "lan%d_ifname", i);

		ifname = nvram_get(lan_ifname);
		if (!ifname)
			continue;
#endif /* WPS_UPNP_DEVICE */

		/* Search for wl_name in ess */
		ap_index = -1;
		sprintf(ess_name, "ess%d", ess_id);

		memset(wlnames, 0, sizeof(wlnames));
		memset(stanames, 0, sizeof(stanames));
		band = 0;

		foreach(name, ifnames, next) {
			if (nvifname_to_osifname(name, os_name, sizeof(os_name)) < 0)
				continue;
			if (wl_probe(os_name) ||
				wl_ioctl(os_name, WLC_GET_INSTANCE, &unit, sizeof(unit)))
				continue;

			/* Convert eth name to wl name */
			if (osifname_to_nvifname(name, wl_name, sizeof(wl_name)) != 0)
				continue;

			/* Ignore radio or bss is disabled */
			snprintf(tmp, sizeof(tmp), "wl%d_radio", unit);
			wl_radio = nvram_safe_get(tmp);
			if (strcmp(wl_radio, "1") != 0)
				continue;

			snprintf(tmp, sizeof(tmp), "%s_bss_enabled", wl_name);
			wl_bss_enabled = nvram_safe_get(tmp);
			if (strcmp(wl_bss_enabled, "1") != 0) {
				/* In URE WRE mode, the STA interface always join WPS */
				snprintf(tmp, sizeof(tmp), "%s_ure_mode", wl_name);
				value = nvram_safe_get(tmp);
				if (strcmp(value, "wre") != 0)
					continue;
			}
			print_conf(os_name, wl_name, "ure_mode", NULL);

			/* Get configured wireless address */
			snprintf(prefix, sizeof(prefix), "%s_", wl_name);

			value = nvram_get(strcat_r(prefix, "wps_mode", tmp));
			if (strcmp(value, "disabled") == 0)
				continue;

			/* If the radio is disabled, skip it */
			if (!print_conf(os_name, wl_name, "hwaddr", NULL))
				continue;

			/* Print wl specific variables */
			wl_mode = nvram_get(strcat_r(prefix, "mode", tmp));
			print_conf(os_name, wl_name, "mode", wl_mode);

			print_conf(os_name, wl_name, "wep", NULL);
			print_conf(os_name, wl_name, "auth", NULL);
			print_conf(os_name, wl_name, "ssid", NULL);
			print_conf(os_name, wl_name, "akm", NULL);
			print_conf(os_name, wl_name, "crypto", NULL);
			print_conf(os_name, wl_name, "wpa_psk", NULL);

			value = print_conf(os_name, wl_name, "key", NULL);
			if (value) {
				sprintf(key, "key%s", value);
				print_conf(os_name, wl_name, key, NULL);
			}

			/* Set only for AP mode */
			if (strcmp(wl_mode, "ap") == 0) {
				/* Get per lan RF bands */
				wl_ioctl(os_name, WLC_GET_BAND, &bandtype, sizeof(bandtype));
				if (bandtype == WLC_BAND_AUTO) {
					int list[3];
					int j;

					wl_ioctl(os_name, WLC_GET_BANDLIST, list, sizeof(list));
					if (list[0] > 2)
						list[0] = 2;

					bandtype = 0;
					for (j = 0; j < list[0]; j++) {
						switch (list[j]) {
						case WLC_BAND_5G:
						case WLC_BAND_2G:
							bandtype |= list[j];
							break;
						default:
							break;
						}
					}
				}

				sprintf(tmp, "%d", bandtype);
				print_conf(os_name, wl_name, "band", tmp);

				/* Aggregage band type */
				band |= bandtype;

				TUTRACE((TUTRACE_INFO, "%s(%s) bandtype = %d\n",
					os_name, wl_name, bandtype));

				/* Save apsta AP ifname */
				strcpy(wps_pbc_ap_ifname, os_name);
				wps_pbc_ap_index = i;

#ifdef __CONFIG_WFI__
				/* Get Wifi-Invite configuration for this (virtual) interface */
				print_conf(os_name, wl_name, "wfi_enable", NULL);
				print_conf(os_name, wl_name, "wfi_pinmode", NULL);
#endif	/* __CONFIG_WFI__ */
				/* We got an AP interface, need UPnP attached */
				ap_index = i;

				/* Save wlnames of this lan */
				if (strlen(wlnames))
					strcat(wlnames, " ");

				strcat(wlnames, os_name);
			}
			/* Set only for STA mode */
			else {
				/* Save apsta STA ifname */
				strcpy(wps_pbc_sta_ifname, os_name);

				/* Save stanames of this lan */
				if (strlen(stanames))
					strcat(stanames, " ");

				strcat(stanames, os_name);
			}
		}

		/* Setup ESS for AP */
		if (strlen(wlnames)) {
			print_conf(ess_name, NULL, "wlnames", wlnames);
			print_conf(ess_name, lan_name, "wps_reg", NULL);
			print_conf(ess_name, lan_name, "wps_oob", NULL);

			/* LED id */
			sprintf(tmp, "%d", ess_id);
			print_conf(ess_name, NULL, "led_id", tmp);

			if (band) {
				/* ALL RF bands supported */
				switch (band) {
				case WLC_BAND_ALL:
					bandtype = WPS_RFBAND_24GHZ | WPS_RFBAND_50GHZ;
					break;
				case WLC_BAND_5G:
					bandtype = WPS_RFBAND_50GHZ;
					break;
				case WLC_BAND_2G:
				default:
					bandtype = WPS_RFBAND_24GHZ;
					break;
				}
				sprintf(tmp, "%d", bandtype);
				print_conf(ess_name, NULL, "band", tmp);
			}

			if (ap_index != -1) {
				sprintf(tmp, "%d", ap_index);
				print_conf(ess_name, NULL, "ap_index", tmp);
#ifdef WPS_UPNP_DEVICE
				print_conf(ess_name, NULL, "ifname", ifname);
#endif
			}

			/* WSC 2.0, for built-in Registrar add AuthorizedMACs */
			print_conf(ess_name, lan_name, "ipaddr", NULL);

			ess_id++;
		}

		/* Setup ESS for URE STA */
		foreach(name, stanames, next) {
			/* Convert eth name to wl name */
			if (osifname_to_nvifname(name, wl_name, sizeof(wl_name)) != 0)
				continue;

			sprintf(ess_name, "ess%d", ess_id);
			print_conf(ess_name, NULL, "wlnames", name);
			print_conf(ess_name, wl_name, "wps_oob", NULL);

			/* LED id */
			sprintf(tmp, "%d", ess_id);
			print_conf(ess_name, NULL, "led_id", tmp);

			/* WSC 2.0, for built-in Registrar add AuthorizedMACs */
			print_conf(ess_name, lan_name, "ipaddr", NULL);

			ess_id++;
		}
	}

	/* (WAN) for push button */
	if (!nvram_match("router_disable", "1")) {
		for (i = 0; i < 256; i ++) {
			/* Taking care of WAN interface names */
			if (i == 0)
				strcpy(name, "wan_ifnames");
			else
				sprintf(name, "wan%d_ifnames", i);

			ifnames = nvram_get(name);
			if (!ifnames)
				continue;

			/* Search for wl_name in it */
			memset(wlnames, 0, sizeof(wlnames));
			foreach(name, ifnames, next) {
				if (nvifname_to_osifname(name, os_name, sizeof(os_name)) < 0)
					continue;
				if (wl_probe(os_name) ||
					wl_ioctl(os_name, WLC_GET_INSTANCE, &unit, sizeof(unit)))
					continue;

				/* Convert eth name to wl name */
				if (osifname_to_nvifname(name, wl_name, sizeof(wl_name)) != 0)
					continue;

				/* Ignore radio or bss is disabled */
				snprintf(tmp, sizeof(tmp), "wl%d_radio", unit);
				wl_radio = nvram_safe_get(tmp);

				snprintf(tmp, sizeof(tmp), "%s_bss_enabled", wl_name);
				wl_bss_enabled = nvram_safe_get(tmp);
				if (strcmp(wl_radio, "1") != 0 || strcmp(wl_bss_enabled, "1") != 0)
					continue;

				/* Get configured wireless address */
				snprintf(prefix, sizeof(prefix), "%s_", wl_name);

				value = nvram_get(strcat_r(prefix, "wps_mode", tmp));
				if (strcmp(value, "disabled") == 0)
					continue;

				if (!print_conf(os_name, wl_name, "hwaddr", NULL))
					continue;

				/* Print wl specific variables */
				print_conf(os_name, wl_name, "mode", NULL);

				print_conf(os_name, wl_name, "wep", NULL);
				print_conf(os_name, wl_name, "auth", NULL);
				print_conf(os_name, wl_name, "ssid", NULL);
				print_conf(os_name, wl_name, "akm", NULL);
				print_conf(os_name, wl_name, "crypto", NULL);
				print_conf(os_name, wl_name, "wpa_psk", NULL);

				value = print_conf(os_name, wl_name, "key", NULL);
				if (value) {
					sprintf(key, "key%s", value);
					print_conf(os_name, wl_name, key, NULL);
				}

				/* Save apsta STA ifname */
				strcpy(wps_pbc_sta_ifname, os_name);

				/* Set ESS instance prefix */
				sprintf(ess_name, "ess%d", ess_id);
				print_conf(ess_name, NULL, "wlnames", os_name);

				/* LED id */
				sprintf(tmp, "%d", ess_id);
				print_conf(ess_name, NULL, "led_id", tmp);
				print_conf(ess_name, wl_name, "wps_oob", NULL);

				ess_id++;
			}
		}
	}

#ifdef BCMWPSAPSTA
	if (is_wps_pbc_apsta_enabled(ess_id, wps_pbc_ap_ifname, wps_pbc_sta_ifname)) {
		print_conf(NULL, NULL, "wps_pbc_apsta", "enabled");
		print_conf(NULL, NULL, "wps_pbc_ap_ifname", wps_pbc_ap_ifname);
		print_conf(NULL, NULL, "wps_pbc_sta_ifname", wps_pbc_sta_ifname);
		sprintf(tmp, "%d", wps_pbc_ap_index);
		print_conf(NULL, NULL, "wps_pbc_ap_index", tmp);
	}
#endif /* BCMWPSAPSTA */

	/* Save maximum instance */
	sprintf(tmp, "%d", ess_id);
	print_conf(NULL, NULL, "wps_ess_num", tmp);

	/* Seperate wps_conf_buf into wps_conf_list[] */
	wps_conf_num = 0;
	for (item = wps_conf_buf, p = item; item && item[0]; item = next, p = 0) {
		/* Get next token */
		strtok_r(p, "\n", &next);
		wps_conf_list[wps_conf_num++] = item;
	}
}


/*
 * Name        : main
 * Description : Main entry point for the WPS monitor
 * Arguments   : int argc, char *argv[] - command line parameters
 * Return type : int
 */
int
main(int argc, char* argv[])
{
	FILE *pidfile;
	int flag;
	char *var;
	unsigned int wps_msglevel;

	/* Show usage */
	if (argc > 1) {
		fprintf(stderr, "Usage: wps_monitor\n");
		return -1;
	}

	/*
	 * Check whether this process is running
	 */
	if ((pidfile = fopen(WPSM_PID_FILE_PATH, "r"))) {
		fprintf(stderr, "%s: wps_monitor has been started\n", __FILE__);

		fclose(pidfile);
		return -1;
	}

	/* Cread pid file */
	if ((pidfile = fopen(WPSM_PID_FILE_PATH, "w"))) {
		fprintf(pidfile, "%d\n", getpid());
		fclose(pidfile);
	}
	else {
		perror("pidfile");
		exit(errno);
	}

	/* establish a handler to handle SIGTERM. */
	signal(SIGTERM, wps_stophandler);
	signal(SIGUSR1, wps_restarthandler);

	RAND_linux_init();

	/* Default settings validation */
	wps_osl_validate_default();

	wps_osl_build_conf();

	/* Set denug message level */
	var = nvram_get("wps_msglevel");
	if (var) {
		wps_msglevel = strtoul(var, NULL, 16);
		fprintf(stderr, "WPS: set msglevel to 0x%x\n", wps_msglevel);
		wps_tutrace_set_msglevel(wps_msglevel);
	}

	/* Enter main loop */
	flag = wps_mainloop(wps_conf_num, wps_conf_list);

	if (flag & WPSM_WKSP_FLAG_SET_RESTART)
		nvram_set("wps_restart", "1");

	/* need restart wireless */
	if (flag & WPSM_WKSP_FLAG_RESTART_WL)
		wps_osl_restart_wl();

	/* Destroy pid file */
	unlink(WPSM_PID_FILE_PATH);

	return 0;
}
