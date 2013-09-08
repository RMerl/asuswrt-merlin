/*
 * WPS Station specific (Platform independent portion)
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
 *
 */

#include <stdio.h>
#include <unistd.h>

#if defined(__ECOS)
#include <proto/ethernet.h>
#endif

#include <ctype.h>
#include <wpserror.h>
#include <portability.h>
#include <reg_prototlv.h>
#include <wps_wl.h>
#include <wps_enrapi.h>
#include <wps_sta.h>
#include <wps_enr.h>
#include <wps.h>
#include <wps_ui.h>
#include <wps_enr_osl.h>
#include <wps_version.h>
#include <wps_staeapsm.h>
#include <wpscommon.h>
#include <bcmutils.h>
#include <shutils.h>
#include <wlif_utils.h>
#include <tutrace.h>
#include <wlioctl.h>
#include <wps_pb.h>

#ifdef WPS_NFC_DEVICE
#include <wps_nfc.h>
#endif

#if !defined(MOD_VERSION_STR)
#error "wps_version.h doesn't exist !"
#endif

static char def_pin[9] = "12345670\0";
static unsigned long start_time;
static char *pin = def_pin; /* set pin to default */
static uint8 bssid[6];
static char ssid[SIZE_SSID_LENGTH] = "ASUS\0";
static uint8 wsec = 1; /* by default, assume wep is ON */
static uint8 enroll_again = false;
static bool b_wps_version2 = false;
static uint8 empty_mac[SIZE_MAC_ADDR] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned short oob_devpwid = 0;
static uint8 *pub_key_hash = NULL;
static uint8 *pre_privkey = NULL;

#define WPS_ASSOC_STATE_INIT		0
#define WPS_ASSOC_STATE_SCANNING	1
#define WPS_ASSOC_STATE_FINDINGAP	2
#define WPS_ASSOC_STATE_ASSOCIATING	3
#define WPS_ASSOC_STATE_ASSOCIATED	4
static char assoc_state = WPS_ASSOC_STATE_INIT;
static unsigned long assoc_state_time;

typedef struct {
	int 	sc_mode;
	bool 	configap;
	int 	ess_id;
} wpssta_wksp_t;

static uint32 wpssta_enr_config_init();
static uint32 wpssta_reg_config_init(wpssta_wksp_t *wksp, char *ifname, char *bssid, char oob);
static int wpssta_display_aplist(wps_ap_list_info_t *aplist);
static int wpssta_do_protocol_again(wpssta_wksp_t *wksp);


/* do join network without waiting association */
static void
wpssta_do_join(uint8 again)
{
	enroll_again = again;

	leave_network();
	WpsSleep(1);
	wpsenr_osl_proc_states(WPS_ASSOCIATING);
	join_network_with_bssid(ssid, wsec, bssid);
	assoc_state = WPS_ASSOC_STATE_ASSOCIATING;
	assoc_state_time = get_current_time();
}

static int
wpssta_wksp_deinit(void *sta_wksp)
{
	wps_cleanup();
	if (sta_wksp)
		free(sta_wksp);
	return 0;
}

/*
 * Name        : wpsenr_wksp_mainloop
 * Description : Main loop point for the WPS stack
 * Arguments   : wpsenr_param_t *param - argument set
 * Return type : int
 */
static wpssta_wksp_t *
wpssta_wksp_init(char *ifname)
{
	wpssta_wksp_t *sta_wksp = NULL;
	int pbc = WPS_UI_PBC_SW;
	char start_ok = false;
	wps_ap_list_info_t *wpsaplist;
	char scan = false;
	char *val, *next;
	char op[6] = {0};
	int oob = 0;
	int i, imax;
	int wps_action;
	char *env_ssid = NULL;
	char *env_sec = NULL;
	char *env_bssid = NULL;
	char *env_pin = NULL;
#ifdef __CONFIG_WFI__
	char *ui_env_pin = NULL;
#endif /* __CONFIG_WFI__ */
	char tmp[100];
	char *wlnames, name[256];

	TUTRACE((TUTRACE_INFO, "*********************************************\n"));
	TUTRACE((TUTRACE_INFO, "WPS - Enrollee App Broacom Corp.\n"));
	TUTRACE((TUTRACE_INFO, "Version: %s\n", MOD_VERSION_STR));
	TUTRACE((TUTRACE_INFO, "*********************************************\n"));

	/* we need to specify the if name before anything else */
	if (!ifname) {
		TUTRACE((TUTRACE_INFO, "no ifname exist!! return\n"));
		return 0;
	}

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		b_wps_version2 = true;

	wps_set_ifname(ifname);
	wps_osl_set_ifname(ifname);

	/* reset assoc_state in INIT state */
	assoc_state = WPS_ASSOC_STATE_INIT;

	/* reset enroll_again */
	enroll_again = false;

	/* reset variables */
	oob_devpwid = 0;
	pub_key_hash = NULL;
	pre_privkey = NULL;

	/* Check whether scan needed */
	val = wps_ui_get_env("wps_enr_scan");
	if (val)
		scan = atoi(val);

	/* if scan requested : display and exit */
	if (scan) {
		/* do scan and wait the scan results */
		do_wps_scan();
		while (get_wps_scan_results() == NULL)
			WpsSleep(1);

		/* use scan result to create ap list */
		wpsaplist = create_aplist();
		if (wpsaplist) {
			wpssta_display_aplist(wpsaplist);
			wps_get_aplist(wpsaplist, wpsaplist);

			TUTRACE((TUTRACE_INFO, "WPS Enabled AP list :\n"));
			wpssta_display_aplist(wpsaplist);
		}
		goto exit;
	}

	/* init workspace */
	if ((sta_wksp = (wpssta_wksp_t *)alloc_init(sizeof(wpssta_wksp_t))) == NULL) {
		TUTRACE((TUTRACE_INFO, "Can not allocate memory for wps workspace...\n"));
		return NULL;
	}
	memset(sta_wksp, 0, sizeof(wpssta_wksp_t));
	wps_action = atoi(wps_ui_get_env("wps_action"));

	/* Setup STA action */
	if (wps_action == WPS_UI_ACT_STA_CONFIGAP || wps_action == WPS_UI_ACT_STA_GETAPCONFIG) {
		sta_wksp->sc_mode = SCMODE_STA_REGISTRAR;
		if (wps_action == WPS_UI_ACT_STA_CONFIGAP)
			sta_wksp->configap = true;
	}
	else
		sta_wksp->sc_mode = SCMODE_STA_ENROLLEE;

	val = wps_ui_get_env("wps_pbc_method");
	if (val)
		pbc = atoi(val);

	/* Save maximum instance number, and probe if any wl interface */
	imax = wps_get_ess_num();
	for (i = 0; i < imax; i++) {
		sprintf(tmp, "ess%d_wlnames", i);
		wlnames = wps_safe_get_conf(tmp);

		foreach(name, wlnames, next) {
			if (!strcmp(name, ifname)) {
				sta_wksp->ess_id = i;
				goto found;
			}
		}
	}
	goto exit;

found:
	/* Retrieve ENV */
	if (pbc ==  WPS_UI_PBC_HW) {
		strcat(op, "pb");
	}
	else {
		/* SW PBC */
		if (atoi(wps_ui_get_env("wps_method")) == WPS_UI_METHOD_PBC) {
			strcat(op, "pb");
		}
		else { /* PIN */
			strcat(op, "pin");

#ifdef WPS_NFC_DEVICE
			if (strcmp(wps_ui_get_env("wps_sta_pin"), "NFC_PW") == 0) {
				env_pin = wps_nfc_dev_pw_str();
				oob_devpwid = wps_nfc_pw_id();
				pre_privkey = wps_nfc_priv_key();
			}
			else
#endif
			{
				env_pin = wps_get_conf("wps_device_pin");
			}

			env_sec = wps_ui_get_env("wps_enr_wsec");
			if (env_sec[0] != 0) {
				wsec = atoi(env_sec);
			}

			env_ssid = wps_ui_get_env("wps_enr_ssid");
			if (env_ssid[0] == 0) {
				TUTRACE((TUTRACE_ERR,
					"\n\nPlease specify ssid or use pbc method\n\n"));
				goto exit;
			}
			wps_strncpy(ssid, env_ssid, sizeof(ssid));

			env_bssid = wps_ui_get_env("wps_enr_bssid");
			if (strlen(env_bssid)) {
				/*
				 * WARNING : this "bssid" is used only to create an 802.1X socket.
				 *
				 * Normally, it should be the bssid of the AP we will associate to.
				 *
				 * Setting this manually means that we might be proceeding to
				 * eapol exchange with a different AP than the one we are
				 * associated to, which might work ... or not.
				 *
				 * When implementing an application, one might want to enforce
				 * association with the AP with that particular BSSID. In case of
				 * multiple AP on the ESS, this might not be stable with roaming
				 * enabled.
				 */
				ether_atoe(env_bssid, bssid);
			}
#ifdef __CONFIG_WFI__
			/* For WiFi-Invite session PIN */
			ui_env_pin = wps_ui_get_env("wps_device_pin");
			if (ui_env_pin[0] != '\0')
				env_pin = ui_env_pin;
#endif /* __CONFIG_WFI__ */

			if (sta_wksp->sc_mode == SCMODE_STA_REGISTRAR) {
				env_pin = wps_ui_get_env("wps_stareg_ap_pin");
#ifdef WPS_NFC_DEVICE
				if (strcmp(env_pin, "NFC_PW") == 0) {
					env_pin = wps_nfc_dev_pw_str();
					oob_devpwid = wps_nfc_pw_id();
					pub_key_hash = wps_nfc_pub_key_hash();
				}
				else
#endif
				{
					if (wps_validate_pin(env_pin) == FALSE) {
						TUTRACE((TUTRACE_INFO, "Not a valid PIN [%s]\n",
							env_pin ? (char *)env_pin : ""));
						goto exit;
					}
				}

				sprintf(tmp, "ess%d_wps_oob", sta_wksp->ess_id);
				val = wps_safe_get_conf(tmp);
				if (strcmp(val, "enabled") == 0)
					oob = 1;
			}

			/* If we want to get AP config and the AP is unconfigured,
			 * configure the AP directly
			*/
			if (sta_wksp->sc_mode == SCMODE_STA_REGISTRAR &&
			    sta_wksp->configap == false) {
				val = wps_ui_get_env("wps_scstate");
				if (strcmp(val, "unconfigured") == 0) {
					sta_wksp->configap = true;
					TUTRACE((TUTRACE_INFO, "AP-%s is unconfigure, "
						"using our security settings to configre it.\n",
						env_ssid));
				}
			}
		}
	}

	TUTRACE((TUTRACE_INFO,
		"pbc = %s, wpsenr param: ifname = %s, mode= %s, op = %s, sec = %s, "
		"ssid = %s, bssid = %s, pin = %s, oob = %s\n",
		(pbc == 1? "HW_PBC": "SW_PBC"),
		ifname,
		(sta_wksp->sc_mode == SCMODE_STA_REGISTRAR) ? "STA_REGISTRAR" : "STA_ENROLLEE",
		op,
		(env_sec? (char *)env_sec : "NULL"),
		(env_ssid? (char *)env_ssid : "NULL"),
		(env_bssid? (char *)env_bssid : "NULL"),
		((env_pin && oob_devpwid == 0)? (char *)env_pin : "NULL"),
		(oob == 1? "Enabled": "Disabled")));

	/*
	 * setup device configuration for WPS
	 * needs to be done before eventual scan for PBC.
	 */
	if (sta_wksp->sc_mode == SCMODE_STA_REGISTRAR) {
		if (wpssta_reg_config_init(sta_wksp, ifname, bssid, oob) != WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "wpssta_reg_config_init failed, exit.\n"));
			goto exit;
		}
	}
	else {
		if (wpssta_enr_config_init() != WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "wpssta_enr_config_init failed, exit.\n"));
			goto exit;
		}
	}

	/* if ssid specified, use it */
	if (!strcmp(op, "pin")) {
		pin = env_pin;
		if (!pin) {
			pin = def_pin;
			TUTRACE((TUTRACE_ERR,
				"\n\nStation Pin not specified, use default Pin %s\n\n",
				def_pin));
		}
		start_ok = true;
		/* WSC 2.0,  Test Plan 5.1.1 step 8 must add wps ie to probe request */
		if (b_wps_version2)
			add_wps_ie(NULL, 0, 0, b_wps_version2);
	}
	else {
		pin = NULL;
		wpsenr_osl_proc_states(WPS_FIND_PBC_AP);

		/* add wps ie to probe  */
		add_wps_ie(NULL, 0, TRUE, b_wps_version2);
		do_wps_scan();
		assoc_state_time = get_current_time();
		assoc_state = WPS_ASSOC_STATE_SCANNING;

		start_ok = false;
	}

	/* start WPS two minutes period at Finding a PBC AP or Associating with AP */
	start_time = get_current_time();

	if (start_ok) {
		/* clear current security setting */
		wpsenr_osl_clear_wsec();

		/*
		 * join. If user_bssid is specified, it might not
		 * match the actual associated AP.
		 * An implementation might want to make sure
		 * it associates to the same bssid.
		 * There might be problems with roaming.
		 */
		wpssta_do_join(false);
		return sta_wksp;
	}
	else if (assoc_state == WPS_ASSOC_STATE_SCANNING) {
		return sta_wksp;
	}

exit:
	wpssta_wksp_deinit(sta_wksp);
	return NULL;
}

static int
wpssta_wksp_start(wpssta_wksp_t *sta_wksp)
{
	char *val;
	char user_bssid = false;
	uint32 len;
	uint band_num, active_band;
	int pbc = WPS_UI_PBC_SW;
	unsigned char *env_bssid = NULL;
	uint8 assoc_bssid[6];


	val = wps_ui_get_env("wps_pbc_method");
	if (val)
		pbc = atoi(val);

	/* Retrieve user specified bssid ENV */
	if (pbc !=  WPS_UI_PBC_HW) {
		env_bssid = wps_ui_get_env("wps_enr_bssid");
		if (env_bssid)
			user_bssid = true;
	}

	/* update specific RF band */
	wps_get_bands(&band_num, &active_band);
	if (active_band == WLC_BAND_5G)
		active_band = WPS_RFBAND_50GHZ;
	else if (active_band == WLC_BAND_2G)
		active_band = WPS_RFBAND_24GHZ;
	else
		active_band = WPS_RFBAND_24GHZ;
	wps_update_RFBand((uint8)active_band);


	/*
	 * We always retrieve bssid from associated AP. If driver support join network
	 * with bssid, the retrieved bssid should be same as user specified. Otherwise
	 * there might be problems with roaming.
	 */

	/* If user_bssid not defined, use associated AP's */
	if (wps_get_bssid(assoc_bssid)) {
		TUTRACE((TUTRACE_ERR, "Can not get [%s] BSSID, Quit....\n", ssid));
		goto err_exit;
	}

	/* check associated bssid consistent with user specified */
	if (user_bssid && memcmp(assoc_bssid, bssid, 6) != 0) {
		/* warning to user */
		TUTRACE((TUTRACE_INFO, "User specified BSSID %02x:%02x:%02x:%02x:%02x:%02x, but "
			"connect to %02x:%02x:%02x:%02x:%02x:%02x\n",
			bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
			assoc_bssid[0], assoc_bssid[1], assoc_bssid[2], assoc_bssid[3],
			assoc_bssid[4], assoc_bssid[5]));
		memcpy(bssid, assoc_bssid, 6);
	}

	/* setup raw 802.1X socket with "bssid" destination  */
	if (wps_osl_init(bssid) != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "Initializing 802.1x raw socket failed. \n"));
		goto err_exit;
	}

	wpsenr_osl_proc_states(WPS_ASSOCIATED); /* START ENROLLING */

	if (sta_wksp->sc_mode == SCMODE_STA_REGISTRAR) {
		TUTRACE((TUTRACE_INFO,
			"Start registration for BSSID: %02x:%02x:%02x:%02x:%02x:%02x [Mode:%s]\n",
			bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
			(sta_wksp->configap == true) ? " config AP":" get AP config"));

		if (wpssta_start_registration_devpwid(pin, pub_key_hash, oob_devpwid, start_time)
			!= WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "Start registration failed!\n"));
			goto err_exit;
		}
	}
	else {
		TUTRACE((TUTRACE_INFO,
			"Start enrollment for BSSID: %02x:%02x:%02x:%02x:%02x:%02x\n", bssid[0],
			bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]));

		if (wpssta_start_enrollment_devpwid(pin, oob_devpwid, start_time)
			!= WPS_SUCCESS) {
			TUTRACE((TUTRACE_INFO, "Start enrollment failed!\n"));
			goto err_exit;
		}
	}

	/*
	 * start the process by sending the eapol start . Created from the
	 * Enrollee SM Initialize.
	 */
	len = wps_get_msg_to_send(&val, start_time);
	if (val) {
		send_eapol_packet(val, len);
		TUTRACE((TUTRACE_INFO, "Send EAPOL-Start\n"));
	}
	else {
		/* this means the system is not initialized */
		return WPS_ERR_NOT_INITIALIZED;
	}

	return WPS_SUCCESS;

err_exit: /* Do cleanup */
	wpsenr_osl_restore_wsec();
	return WPS_ERR_SYSTEM;
}

static void
wpssta_print_credential(WpsEnrCred *credential, char *title)
{
	char keystr[65];

	if (title)
		TUTRACE((TUTRACE_INFO, "\n%s\n", title));

	TUTRACE((TUTRACE_INFO, "SSID = %s\n", credential->ssid));
	TUTRACE((TUTRACE_INFO, "Key Mgmt type is %s\n", credential->keyMgmt));
	strncpy(keystr, credential->nwKey, credential->nwKeyLen);
	keystr[credential->nwKeyLen] = 0;
	TUTRACE((TUTRACE_INFO, "Key : %s\n", keystr));
	if (credential->encrType == 0) {
		TUTRACE((TUTRACE_INFO, "Encryption : NONE\n"));
	}
	else {
		if (credential->encrType & ENCRYPT_WEP) {
			TUTRACE((TUTRACE_INFO, "Encryption :  WEP\n"));
			TUTRACE((TUTRACE_INFO, "WEP Index: %d\n", credential->wepIndex));
		}
		if (credential->encrType & ENCRYPT_TKIP)
			TUTRACE((TUTRACE_INFO, "Encryption :  TKIP\n"));
		if (credential->encrType & ENCRYPT_AES)
			TUTRACE((TUTRACE_INFO, "Encryption :  AES\n"));
	}
}

static void
wpssta_filter_credential(WpsEnrCred *credential)
{
	int encrType_mixed = (ENCRYPT_TKIP | ENCRYPT_AES);

	/* Always use WPA2PSK when both WPAPSK and WPA2PSK enabled */
	if (strstr(credential->keyMgmt, "WPA-PSK") &&
	    strstr(credential->keyMgmt, "WPA2-PSK")) {
		strcpy(credential->keyMgmt, "WPA2-PSK");
		TUTRACE((TUTRACE_INFO, "Key Mgmt type is mixed-mode, pick WPA2-PSK up\n"));
	}

	/* Always use AES when encryptoin type in mixed-mode */
	if ((credential->encrType & encrType_mixed) == encrType_mixed) {
		credential->encrType &= ~ENCRYPT_TKIP;
		TUTRACE((TUTRACE_INFO, "Encryption is mixed-mode, pick AES up\n"));
	}
}

static int
wpssta_success(wpssta_wksp_t *sta_wksp)
{
	int retVal = WPS_RESULT_SUCCESS, ssidlen = 0;
	char ssid[SIZE_SSID_LENGTH];
	WpsEnrCred *credential = (WpsEnrCred *)malloc(sizeof(WpsEnrCred));

	/* Remove WPS IE before doing 4-way handshake */
	rem_wps_ie(NULL, 0, VNDR_IE_PRBREQ_FLAG);
	if (b_wps_version2)
		rem_wps_ie(NULL, 0, VNDR_IE_ASSOCREQ_FLAG);

	if (credential == NULL) {
		TUTRACE((TUTRACE_INFO, "memory allocate failed!!\n"));
		wpsenr_osl_proc_states(WPS_MSG_ERR); /* FAILED */
		return WPS_RESULT_FAILURE;
	}

	TUTRACE((TUTRACE_INFO, "WPS Protocol SUCCEEDED !!\n"));

	/* get credentials */
	if (sta_wksp->sc_mode == SCMODE_STA_REGISTRAR) {
		if (sta_wksp->configap == true)
			wpssta_get_reg_M8credentials(credential);
		else
			wpssta_get_reg_M7credentials(credential);
	}
	else {
		wps_get_ssid(ssid, &ssidlen);
		wpssta_get_credentials(credential, ssid, ssidlen);
	}

	wpssta_print_credential(credential, "Current AP Credential");

	wpssta_filter_credential(credential);

	wpsenr_osl_proc_states(WPS_OK); /* SUCCEEDED */

	if (wpsenr_osl_set_wsec(sta_wksp->ess_id, credential, sta_wksp->sc_mode))
		retVal  = WPS_RESULT_SUCCESS_RESTART;
	else
		retVal  = WPS_RESULT_SUCCESS;

	/* free memory */
	free(credential);

	/* WPS 2.0. test 5.1.1, leave network before do_wpa_psk with new security */
	WpsSleepMs(500);
	leave_network();

	return retVal;
}

static int
wpssta_process(wpssta_wksp_t *sta_wksp, char *buf, int len, int msgtype)
{
	uint32 buf_len = len;
	char *sendBuf;
	int last_recv_msg;
	char msg_type;
	unsigned long now = get_current_time();
	uint32 retVal;
	int state;
#ifdef _TUDEBUGTRACE
	char *assoc_state_name[] = {"init", "scanning", "findingap", "associating", "associated"};
#endif

	/* associate state checking */
	if (assoc_state != WPS_ASSOC_STATE_ASSOCIATED) {
		TUTRACE((TUTRACE_INFO, "In assoc state %s, ignore this packet\n",
			assoc_state_name[(int)assoc_state]));
		return WPS_CONT; /* ingore it */
	}

	/* eapol packet validation */
	retVal = wpsenr_eapol_validate(buf, &buf_len);
	if (retVal == WPS_SUCCESS) {
		/* Show receive message */
		msg_type = wps_get_msg_type(buf, buf_len);
		TUTRACE((TUTRACE_INFO, "Receive EAP-Request%s\n",
			wps_get_msg_string((int)msg_type)));

		/* process ap message */
		retVal = wps_process_ap_msg(buf, buf_len);

		/* check return code to do more things */
		if (retVal == WPS_SEND_MSG_CONT ||
		    retVal == WPS_SEND_MSG_SUCCESS ||
		    retVal == WPS_SEND_MSG_ERROR ||
		    retVal == WPS_ERR_ENROLLMENT_PINFAIL) {
			len = wps_get_eapol_msg_to_send(&sendBuf, now);
			if (sendBuf) {
				msg_type = wps_get_msg_type(sendBuf, len);

				send_eapol_packet(sendBuf, len);
				TUTRACE((TUTRACE_INFO, "Send EAP-Response%s\n",
					wps_get_msg_string((int)msg_type)));
			}

			if (retVal == WPS_ERR_ENROLLMENT_PINFAIL)
				retVal = WPS_SEND_MSG_ERROR;

			/*
			 * sleep a short time for driver to send last WPS DONE message,
			 * otherwise doing leave_network before do_wpa_psk in
			 * enroll_device() may cause driver to drop the last WPS DONE
			 * message if it not transmit.
			 */
			if (retVal == WPS_SEND_MSG_SUCCESS ||
			    retVal == WPS_SEND_MSG_ERROR)
				WpsSleepMs(2);

			/* over-write retVal */
			if (retVal == WPS_SEND_MSG_SUCCESS)
				retVal = WPS_SUCCESS;
			else if (retVal == WPS_SEND_MSG_ERROR)
				retVal = REG_FAILURE;
			else
				retVal = WPS_CONT;
		}
		else if (retVal == EAP_FAILURE) {
			/* we received an eap failure from registrar */
			/*
			 * check if this is coming AFTER the protocol passed the M2
			 * mark or is the end of the discovery after M2D.
			 */
			last_recv_msg = wps_get_recv_msg_id();
			TUTRACE((TUTRACE_INFO,
				"Received eap failure, last recv msg EAP-Request%s\n",
				wps_get_msg_string(last_recv_msg)));
			if (last_recv_msg > WPS_ID_MESSAGE_M2D) {
				retVal = REG_FAILURE;
			}
			else {
				/* do join again */
				wpssta_do_join(true);
				retVal = WPS_CONT;
			}
		}
		/* special case, without doing wps_eap_create_pkt */
		else if (retVal == WPS_SEND_MSG_IDRESP) {
			len = wps_get_msg_to_send(&sendBuf, now);
			if (sendBuf) {
				send_eapol_packet(sendBuf, len);
				TUTRACE((TUTRACE_INFO, "Send EAP-Response / Identity\n"));
			}
			retVal = WPS_CONT;
		}
		/* Re-transmit last sent message, because we receive a re-transmit packet */
		else if (retVal == WPS_SEND_RET_MSG_CONT) {
			len = wps_get_retrans_msg_to_send(&sendBuf, now, &msg_type);
			if (sendBuf) {
				state = wps_get_eap_state();

				if (state == EAPOL_START_SENT)
					TUTRACE((TUTRACE_INFO, "Re-Send EAPOL-Start\n"));
				else if (state == EAP_IDENTITY_SENT)
					TUTRACE((TUTRACE_INFO,
						"Re-Send EAP-Response / Identity\n"));
				else
					TUTRACE((TUTRACE_INFO, "Re-Send EAP-Response%s\n",
						wps_get_msg_string((int)msg_type)));

				send_eapol_packet(sendBuf, len);
			}

			retVal = WPS_CONT;
		}
		else if (retVal == WPS_SEND_FRAG_CONT ||
			retVal == WPS_SEND_FRAG_ACK_CONT) {
			len = wps_get_frag_msg_to_send(&sendBuf, now);
			if (sendBuf) {
				if (retVal == WPS_SEND_FRAG_CONT)
					TUTRACE((TUTRACE_INFO, "Send EAP-Response(FRAG)\n"));
				else
					TUTRACE((TUTRACE_INFO, "Send EAP-Response(FRAG_ACK)\n"));

				send_eapol_packet(sendBuf, len);
			}
			retVal = WPS_CONT;
		}

		/*
		 * exits with either success, failure or indication that
		 *  the registrar has not started its end of the protocol yet.
		 */
		if (retVal == WPS_SUCCESS) {
			retVal = wpssta_success(sta_wksp);
		}
		else if (retVal != WPS_CONT) {
			TUTRACE((TUTRACE_INFO, "WPS Protocol FAILED. CODE = %x\n", retVal));
			wpsenr_osl_proc_states(WPS_MSG_ERR); /* FAILED */
			retVal = WPS_RESULT_FAILURE;
		}

		/* Failure, restore original wireless settings */
		if (retVal != WPS_RESULT_SUCCESS_RESTART &&
		    retVal != WPS_RESULT_SUCCESS &&
		    retVal != WPS_CONT) {
			wpsenr_osl_restore_wsec();
		}
	}

	return retVal;
}

static int
wpssta_check_timeout(wpssta_wksp_t *sta_wksp)
{
	int state, find_pbc;
	int last_sent_msg;
	uint8 msg_type;
	char *sendBuf;
	uint32 sendBufLen;
	uint32 retVal = WPS_CONT;
	unsigned long now = get_current_time();
	uint8 assoc_bssid[6];


	if (now > start_time + 120) {
		TUTRACE((TUTRACE_INFO, "Overall protocol timeout \n"));
		switch (assoc_state) {
		case WPS_ASSOC_STATE_SCANNING:
		case WPS_ASSOC_STATE_FINDINGAP:
			rem_wps_ie(NULL, 0, VNDR_IE_PRBREQ_FLAG);
			if (b_wps_version2)
				rem_wps_ie(NULL, 0, VNDR_IE_ASSOCREQ_FLAG);
			break;

		default:
			break;
		}

		wpsenr_osl_proc_states(WPS_TIMEOUT);
		return REG_FAILURE;
	}

	/* continue to find pbc ap or check associating status */
	switch (assoc_state) {
	case WPS_ASSOC_STATE_SCANNING:
		/* keep checking scan results for 10 second and issue scan again */
		if ((now - assoc_state_time) > 10 /* WPS_SCAN_MAX_WAIT_SEC */) {
			do_wps_scan();
			assoc_state_time = get_current_time();
		}
		else if (get_wps_scan_results() != NULL) {
			/* got scan results, check to find pbc ap state */
			assoc_state = WPS_ASSOC_STATE_FINDINGAP;
		}
		/* no need to do wps_eap_check_timer  */
		return WPS_CONT;

	case WPS_ASSOC_STATE_FINDINGAP:
		/* find pbc ap */
		find_pbc = wps_pb_find_pbc_ap((char *)bssid, (char *)ssid, &wsec);
		if (find_pbc == PBC_OVERLAP || find_pbc == PBC_TIMEOUT) {
			/* PBC_TIMEOUT should not happen anymore */
			rem_wps_ie(NULL, 0, VNDR_IE_PRBREQ_FLAG);
			if (b_wps_version2)
				rem_wps_ie(NULL, 0, VNDR_IE_ASSOCREQ_FLAG);

			if (find_pbc == PBC_OVERLAP)
				wpsenr_osl_proc_states(WPS_PBCOVERLAP);
			else
				wpsenr_osl_proc_states(WPS_TIMEOUT);

			return REG_FAILURE;
		}

		if (find_pbc == PBC_FOUND_OK) {
			wpssta_do_join(FALSE);
			rem_wps_ie(NULL, 0, VNDR_IE_PRBREQ_FLAG);
			if (b_wps_version2)
				rem_wps_ie(NULL, 0, VNDR_IE_ASSOCREQ_FLAG);
		}
		else {
			/* do scan again */
			do_wps_scan();
			assoc_state = WPS_ASSOC_STATE_SCANNING;
			assoc_state_time = get_current_time();
		}
		/* no need to do wps_eap_check_timer  */
		return WPS_CONT;

	case WPS_ASSOC_STATE_ASSOCIATING:
		/* keep checking bssid status for 10 second and issue join again */
		if ((now - assoc_state_time) > 10 /* WPS_ENR_SCAN_JOIN_MAX_WAIT_SEC */) {
			join_network_with_bssid(ssid, wsec, bssid);
			assoc_state_time = get_current_time();
		}
		else if (wps_get_bssid(assoc_bssid) == 0) {
			/* associated with AP */
			assoc_state = WPS_ASSOC_STATE_ASSOCIATED;

			/* start wps */
			if (enroll_again) {
				if (wpssta_do_protocol_again(sta_wksp) != 0)
					return REG_FAILURE;
			}
			else if (wpssta_wksp_start(sta_wksp) != WPS_SUCCESS)
				return WPS_ERR_OPEN_SESSION;
		}
		/* no need to do wps_eap_check_timer  */
		return WPS_CONT;

	default:
		break;
	}

	/* check eap receive timer. It might be time to re-transmit */
	/*
	 * Do we need this API ? We could just count how many times
	 * we re-transmit right here.
	 */
	retVal = wps_eap_check_timer(now);
	if (retVal == WPS_SEND_RET_MSG_CONT) {
		sendBufLen = wps_get_retrans_msg_to_send(&sendBuf, now, &msg_type);
		if (sendBuf) {
			state = wps_get_eap_state();

			if (state == EAPOL_START_SENT)
				TUTRACE((TUTRACE_INFO, "Re-Send EAPOL-Start\n"));
			else if (state == EAP_IDENTITY_SENT)
				TUTRACE((TUTRACE_INFO, "Re-Send EAP-Response / Identity\n"));
			else
				TUTRACE((TUTRACE_INFO, "Re-Send EAP-Response%s\n",
					wps_get_msg_string((int)msg_type)));

			send_eapol_packet(sendBuf, sendBufLen);
		}
		retVal = WPS_CONT;
	}
	/* re-transmission count exceeded, give up */
	else if (retVal == EAP_TIMEOUT) {
		char last_recv_msg = wps_get_recv_msg_id();

		retVal = WPS_CONT;
		if (last_recv_msg == WPS_ID_MESSAGE_M2D) {
			TUTRACE((TUTRACE_INFO, "M2D Wait timeout, again.\n"));

			/* do join again */
			wpssta_do_join(true);
		}
		else if (last_recv_msg > WPS_ID_MESSAGE_M2D) {
			last_sent_msg = wps_get_sent_msg_id();
			TUTRACE((TUTRACE_INFO, "Timeout, give up. Last recv/sent msg "
				"[EAP-Response%s/EAP-Request%s]\n",
				wps_get_msg_string(last_recv_msg),
				wps_get_msg_string(last_sent_msg)));
			retVal = REG_FAILURE;
		}
		else {
			TUTRACE((TUTRACE_INFO, "Re-transmission count exceeded, again\n"));
			/* do join again */
			wpssta_do_join(true);
		}

		/* Failure, restore original wireless settings */
		if (retVal != WPS_CONT)
			wpsenr_osl_restore_wsec();
	}
	else if (retVal == WPS_SUCCESS) {
		TUTRACE((TUTRACE_INFO, "EAP-Failure not received in 10 seconds, "
			"assume WPS Success!\n"));
		retVal = wpssta_success(sta_wksp);
	}

	return retVal;
}

/*
 *  Let customer have a chance to modify credential
 */
static int
wpssta_update_custom_cred(char *ssid, char *key, char *akm, char *crypto, int oob,
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

	if (oob) {
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

/*
 * Fill up the device info and pass it to WPS.
 * This will need to be tailored to specific platforms (read from a file,
 * nvram ...)
 */

static uint32
wpssta_reg_config_init(wpssta_wksp_t *sta_wksp, char *ifname, char *bssid, char oob)
{
	DevInfo info;
	char *value, *next;
	int auth = 0;
	char mac[6];
	char ssid[MAX_SSID_LEN + 1] = {0};
	char psk[MAX_USER_KEY_LEN + 1] = {0};
	char akmstr[32];
	char key[8];
	unsigned int akm = 0;
	unsigned int wsec = 0;
	int wep_index = 0;			/* wep key index */
	char *wep_key = NULL;			/* user-supplied wep key */
	char dev_akm[64] = {0};
	char dev_crypto[64] = {0};
	char prefix[] = "wlXXXXXXXXXX_";
	char tmp[100];

	/* TBD, is going to use osname only */
	sprintf(prefix, "%s_", ifname);

	/* fill in device specific info. */
	memset((char *)(&info), 0, sizeof(info));

	info.version = WPS_VERSION;

	/* MAC addr */
	wps_osl_get_mac(mac);
	memcpy(info.macAddr, mac, 6);

	memcpy(info.uuid, wps_get_uuid(), SIZE_16_BYTES);
	if ((value = wps_get_conf("wps_sta_device_name")) == NULL)
		value = "ASUSTeK Registrar";
	wps_strncpy(info.deviceName, value, sizeof(info.deviceName));
	info.primDeviceCategory = WPS_DEVICE_TYPE_CAT_NW_INFRA;
	info.primDeviceOui = 0x0050F204;
	info.primDeviceSubCategory = WPS_DEVICE_TYPE_SUB_CAT_NW_GATEWAY;
	strcpy(info.manufacturer, "ASUSTeK");
	strcpy(info.modelName, "WPS Wireless Registrar");
	strcpy(info.modelNumber, "1234");
	strcpy(info.serialNumber, "5678");

	if (b_wps_version2) {
		info.configMethods = (WPS_CONFMET_VIRT_PBC | WPS_CONFMET_PHY_PBC |
			WPS_CONFMET_VIRT_DISPLAY);
	}
	else {
		info.configMethods = WPS_CONFMET_PBC | WPS_CONFMET_DISPLAY;
	}

	/* WSC 2.0, WPS-PSK and SHARED are deprecated.
	 * When both the Registrar and the Enrollee are using protocol version 2.0
	 * or newer, this variable can use the value 0x0022 to indicate mixed mode
	 * operation (both WPA-Personal and WPA2-Personal enabled)
	 */
	if (b_wps_version2) {
		info.authTypeFlags = (uint16)(WPS_AUTHTYPE_OPEN | WPS_AUTHTYPE_WPAPSK |
			WPS_AUTHTYPE_WPA2PSK);
	}
	else {
		info.authTypeFlags = (uint16)(WPS_AUTHTYPE_OPEN | WPS_AUTHTYPE_WPAPSK |
			WPS_AUTHTYPE_SHARED | WPS_AUTHTYPE_WPA2PSK);
	}

	/* ENCR_TYPE_FLAGS */
	/*
	 * WSC 2.0, deprecated WEP. TKIP can only be advertised on the AP when
	 * Mixed Mode is enabled (Encryption Type is 0x000c)
	 */
	if (b_wps_version2) {
		info.encrTypeFlags = (uint16)(WPS_ENCRTYPE_NONE | WPS_ENCRTYPE_TKIP |
			WPS_ENCRTYPE_AES);
	}
	else {
		info.encrTypeFlags = (uint16)(WPS_ENCRTYPE_NONE | WPS_ENCRTYPE_WEP |
			WPS_ENCRTYPE_TKIP | WPS_ENCRTYPE_AES);
	}

	info.connTypeFlags = WPS_CONNTYPE_ESS;
	info.rfBand = WPS_RFBAND_24GHZ;
	info.osVersion = 0x80000000;
	info.featureId = 0x80000000;
	/* WSC 2.0 */
	if (b_wps_version2) {
		value = wps_get_conf("wps_version2_num");
		info.version2 = (uint8)(strtoul(value, NULL, 16));
		info.settingsDelayTime = WPS_SETTING_DELAY_TIME_ROUTER;
		info.b_reqToEnroll = FALSE;
		info.b_nwKeyShareable = FALSE;
	}

	if (!sta_wksp->configap) {
		/* We don't care about our settings. All we want is get ap credential,
		 * so, empty sta credential is okay.  The state machine sends NACK,
		 * when M7AP received.
		 */
		return wpssta_reg_init(&info, NULL, NULL);
	}

	/* Want to config AP with the STA registrar's crendentials */
	if ((value = wps_ui_get_env("wps_ssid")) && strcmp(value, "") != 0) {
		/* SSID */
		value = wps_ui_get_env("wps_ssid");
		strncpy(ssid, value, MAX_SSID_LEN);

		/* AKM */
		value = wps_ui_get_env("wps_akm");
		foreach(akmstr, value, next) {
			if (!strcmp(akmstr, "psk"))
				akm |= WPA_AUTH_PSK;
			if (!strcmp(akmstr, "psk2"))
				akm |= WPA2_AUTH_PSK;
		}
		switch (akm) {
		case 0:
		case WPA2_AUTH_PSK:
		case (WPA_AUTH_PSK | WPA2_AUTH_PSK):
			break;
		default:
			TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in AKM\n"));
			return MC_ERR_CFGFILE_CONTENT;
		}

		/* Crypto */
		if (akm) {
			value = wps_ui_get_env("wps_crypto");
			if (!strcmp(value, "aes"))
				wsec = AES_ENABLED;
			else if (!strcmp(value, "tkip+aes"))
				wsec = TKIP_ENABLED|AES_ENABLED;
			else {
				TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in crypto\n"));
				return MC_ERR_CFGFILE_CONTENT;
			}

			/* Set PSK key */
			value = wps_ui_get_env("wps_psk");
			strncpy(psk, value, MAX_USER_KEY_LEN);
			psk[MAX_USER_KEY_LEN] = 0;
		}
	}
	else {
		/*
		 * Before check oob mode, we have to
		 * get ssid, akm, wep, crypto and mgmt key from config.
		 * because oob mode might change the settings.
		 */
		value = wps_safe_get_conf(strcat_r(prefix, "ssid", tmp));
		strncpy(ssid, value, MAX_SSID_LEN);

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
			strncpy(psk, value, MAX_USER_KEY_LEN);
			psk[MAX_USER_KEY_LEN] = 0;
		}

		if (wsec & WEP_ENABLED) {
			/* Key index */
			value = wps_safe_get_conf(strcat_r(prefix, "key", tmp));
			wep_index = (int)strtoul(value, NULL, 0);

			/* Key */
			sprintf(key, "key%s", value);
			wep_key = wps_safe_get_conf(strcat_r(prefix, key, tmp));
		}

		/* Caution: wps_oob will over-write akm and wsec */
		if (oob) {
			/* Generate random ssid and key */
			if (wps_gen_ssid(ssid, sizeof(ssid),
				wps_get_conf("wps_random_ssid_prefix"),
				wps_safe_get_conf("wl0_hwaddr")) == FALSE ||
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
		wpssta_update_custom_cred(ssid, psk, dev_akm, dev_crypto, oob,
			b_wps_version2);

		/* Parsing return amk and crypto */
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
	strcpy(info.ssid, ssid);

	/* KEY MGMT */
	/* WSC 2.0, deprecated SHARED */
	if (auth) {
		strcpy(info.keyMgmt, "SHARED");
		if (b_wps_version2 && !oob) {
			TUTRACE((TUTRACE_INFO,
				"wpssta_readConfigure: Error in configuration,"
				"Authentication type is Shared, violate WSC 2.0\n"));
			return MC_ERR_CFGFILE_CONTENT;
		}
	}
	else {
		if (WPS_WLAKM_BOTH(akm))
			strcpy(info.keyMgmt, "WPA-PSK WPA2-PSK");
		else if (WPS_WLAKM_PSK(akm))
			strcpy(info.keyMgmt, "WPA-PSK");
		else if (WPS_WLAKM_PSK2(akm))
			strcpy(info.keyMgmt, "WPA2-PSK");
		else
			info.keyMgmt[0] = 0;
	}

	/* WEP index */
	info.wep = (wsec & WEP_ENABLED) ? 1 : 0;

	/* Set crypto algorithm */
	info.crypto = 0;
	if (wsec & TKIP_ENABLED)
		info.crypto |= WPS_ENCRTYPE_TKIP;
	if (wsec & AES_ENABLED)
		info.crypto |= WPS_ENCRTYPE_AES;

	if (info.crypto == 0)
		info.crypto = WPS_ENCRTYPE_TKIP;

	/* WSC 2.0 */
	if (b_wps_version2) {			/* Version 2 */
		value = wps_get_conf("wps_version2_num");
		info.version2 = (uint8)(strtoul(value, NULL, 16));

		/* Setting Delay Time */
		info.settingsDelayTime = WPS_SETTING_DELAY_TIME_ROUTER;
	}

	return wpssta_reg_init(&info, psk, bssid);
}

/*
 * Fill up the device info and pass it to WPS.
 * This will need to be tailored to specific platforms (read from a file,
 * nvram ...)
 */

static uint32
wpssta_enr_config_init()
{
	DevInfo info;
	unsigned char mac[6];
	char *value;
	char uuid[16] = {0x22, 0x21, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0xa, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

	/* fill in device specific info. The way this information is stored is app specific */
	/* Would be good to document all of these ...  */

	memset((char *)(&info), 0, sizeof(info));
	info.version = WPS_VERSION;

	/* MAC addr */
	wps_osl_get_mac(mac);
	memcpy(info.macAddr, mac, 6);

	memcpy(info.uuid, uuid, 16);
	if ((value = wps_get_conf("wps_sta_device_name")) == NULL)
		value = "ASUSTeK Client";
	wps_strncpy(info.deviceName, value, sizeof(info.deviceName));
	info.primDeviceCategory = WPS_DEVICE_TYPE_CAT_NW_INFRA;
	info.primDeviceOui = 0x0050F204;
	info.primDeviceSubCategory = WPS_DEVICE_TYPE_SUB_CAT_NW_GATEWAY;
	strcpy(info.manufacturer, "ASUSTeK");
	strcpy(info.modelName, "WPS Wireless Client");
	strcpy(info.modelNumber, "1234");
	strcpy(info.serialNumber, "5678");

	if (b_wps_version2) {
		info.configMethods = (WPS_CONFMET_VIRT_PBC | WPS_CONFMET_PHY_PBC |
			WPS_CONFMET_VIRT_DISPLAY);
	} else {
		info.configMethods = WPS_CONFMET_PBC | WPS_CONFMET_DISPLAY;
	}

	/* WSC 2.0, WPS-PSK and SHARED are deprecated.
	 * When both the Registrar and the Enrollee are using protocol version 2.0
	 * or newer, this variable can use the value 0x0022 to indicate mixed mode
	 * operation (both WPA-Personal and WPA2-Personal enabled)
	 */
	if (b_wps_version2) {
		info.authTypeFlags = (uint16)(WPS_AUTHTYPE_OPEN | WPS_AUTHTYPE_WPAPSK |
			WPS_AUTHTYPE_WPA2PSK);
	} else {
		info.authTypeFlags = (uint16)(WPS_AUTHTYPE_OPEN | WPS_AUTHTYPE_WPAPSK |
			WPS_AUTHTYPE_SHARED | WPS_AUTHTYPE_WPA2PSK);
	}

	/* ENCR_TYPE_FLAGS */
	/*
	 * WSC 2.0, deprecated WEP. TKIP can only be advertised on the AP when
	 * Mixed Mode is enabled (Encryption Type is 0x000c)
	 */
	if (b_wps_version2) {
		info.encrTypeFlags = (uint16)(WPS_ENCRTYPE_NONE | WPS_ENCRTYPE_TKIP |
			WPS_ENCRTYPE_AES);
	} else {
		info.encrTypeFlags = (uint16)(WPS_ENCRTYPE_NONE | WPS_ENCRTYPE_WEP |
			WPS_ENCRTYPE_TKIP | WPS_ENCRTYPE_AES);
	}

	info.connTypeFlags = WPS_CONNTYPE_ESS;
	info.rfBand = WPS_RFBAND_24GHZ;
	info.osVersion = 0x80000000;
	info.featureId = 0x80000000;

	/* WSC 2.0 */
	if (b_wps_version2) {
		value = wps_get_conf("wps_version2_num");
		info.version2 = (uint8)(strtoul(value, NULL, 16));
		info.settingsDelayTime = WPS_SETTING_DELAY_TIME_LINUX;
		info.b_reqToEnroll = TRUE;
		info.b_nwKeyShareable = FALSE;
	}

#ifdef WPS_NFC_DEVICE
	if (pre_privkey) {
		memcpy(info.pre_privkey, pre_privkey, SIZE_PUB_KEY);
		info.flags |= DEVINFO_FLAG_PRE_PRIV_KEY;
	}
#endif

	return wpssta_enr_init(&info);
}

static int
wpssta_do_protocol_again(wpssta_wksp_t *sta_wksp)
{
	char *data;
	int len;
	unsigned long current_time = get_current_time();
	uint32 ret;

	/* do enrollement/registaration again */
	if (sta_wksp->sc_mode == SCMODE_STA_REGISTRAR) {
		ret = wpssta_start_registration_devpwid(pin, pub_key_hash, oob_devpwid,
			current_time);
	}
	else {
		ret = wpssta_start_enrollment_devpwid(pin, oob_devpwid, current_time);
	}

	if (ret != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "Start enrollment failed!\n"));
		return -1;
	}

	/*
	 * start the process by sending the eapol start . Created from the
	 * Enrollee SM Initialize.
	 */
	len = wps_get_msg_to_send(&data, current_time);
	if (data) {
		send_eapol_packet(data, len);
		printf("Send EAPOL-Start\n");
	}
	else {
		/* this means the system is not initialized */
		TUTRACE((TUTRACE_ERR, "Can not get EAPOL-Start to send, Quit...\n"));
		return -1;
	}

	return 0;

}

int
wpssta_display_aplist(wps_ap_list_info_t *ap)
{
	int i = 0, j;
	uint8 *mac;
#ifdef _TUDEBUGTRACE
	char eastr[ETHER_ADDR_STR_LEN];
#endif
	if (!ap)
		return 0;

	TUTRACE((TUTRACE_INFO, "-------------------------------------\n"));
	while (ap->used == TRUE) {
		mac = ap->authorizedMACs;
		for (j = 0; j < 5; j++) {
			if (memcmp(mac, empty_mac, SIZE_MAC_ADDR) == 0)
				break;
			mac += SIZE_MAC_ADDR;
		}
		TUTRACE((TUTRACE_INFO,
			" %d :  SSID:%s  BSSID:%s  Channel:%d  %s %s AuthroizedMACs: %s %s\n",
			i, ap->ssid, ether_etoa(ap->BSSID, eastr), ap->channel, ap->wep?"WEP":"",
			(b_wps_version2 && ap->version2 >= WPS_VERSION2)?"V2":"",
			ether_etoa(mac, eastr),
			(ap->scstate == WPS_SCSTATE_CONFIGURED)?"Configured":"Unconfigured"));

		ap++;
		i++;
	}

	TUTRACE((TUTRACE_INFO, "-------------------------------------\n"));
	return 0;
}

int
wpssta_open_session(wps_app_t *wps_app, char* ifname)
{
	wpssta_wksp_t *sta_wksp = NULL;

	TUTRACE((TUTRACE_INFO, "Start Enroll\n"));

	sta_wksp = wpssta_wksp_init(ifname);
	if (!sta_wksp) {
		/* means open session failed */
		return WPS_ERR_OPEN_SESSION;
	}

	/* update mode */
	wps_app->wksp = sta_wksp;
	wps_app->sc_mode = sta_wksp->sc_mode;
	wps_app->close = (int (*)(void *))wpssta_wksp_deinit;
	wps_app->process = (int (*)(void *, char *, int, int))wpssta_process;
	wps_app->check_timeout = (int (*)(void *))wpssta_check_timeout;

	return WPS_CONT;
}
