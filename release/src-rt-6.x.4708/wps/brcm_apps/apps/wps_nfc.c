/*
 * WPS nfc
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

#include <net/if.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <portability.h>
#include <tutrace.h>
#include <wps.h>
#include <wps_enrapi.h>
#include <nfc_utils.h>
#include <wps_ui.h>
#include <wps_nfc.h>

#include <shutils.h>
#include <wps_wl.h>

/* NSA */
#include <nsa_api.h>
#include <app_mgt.h>
#include <app_generic.h>
#include <app_nsa_utils.h>
#include <nsa_ndef_utils.h>

#define WPS_NFC_PRINT_CREDENTIAL
#define WPS_NFC_MAX_TIMEOUT	120	/* seconds */

typedef struct {
	char ifname[16];
	char dev_pw_str[SIZE_64_BYTES+1];
	uint8 version2;
	uint8 pre_privkey[SIZE_PUB_KEY];
	uint8 buffer[2048];
	bool config_state;
	uint32 cmd_status;
	uint32 conn_cbstatus;
	int ess_id;
	int cback_retcode;
	uint32 buffer_len;
	WPS_SCSTATE state;
	WpsOobDevPw oob_dev_pw;
	WpsEnrCred cred;

/* For debug only */
	int nfc_sleep1;
	int nfc_sleep2;
} wps_nfc_t;

static wps_hndl_t nfc_hndl;

static wps_nfc_t s_wps_nfc;
static wps_nfc_t *wps_nfc = &s_wps_nfc;
static time_t wps_nfc_active_time = 0;

/* Command */
#define CMD_WRING		0x0001
#define CMD_RDING		0x0002
#define CMD_HOING		0x0003
#define CMD_FMING		0x0004
#define CMD_MASK		0x000F

/* Command result */
#define CMD_RESULT_CPLT		0x0010
#define CMD_RESULT_STOP		0x0020
#define CMD_RESULT_TO		0x0030
#define CMD_RESULT_MASK		0x00F0

#define WPS_NFC_CMD_RESULT_SET(wps_nfc, res)	((wps_nfc)->cmd_status = \
						 (((wps_nfc)->cmd_status & ~CMD_RESULT_MASK) | \
						 ((res) & CMD_RESULT_MASK)))
#define WPS_NFC_CMD_HAVE_RESULT(wps_nfc)	((wps_nfc)->cmd_status & CMD_RESULT_MASK)


#define IS_IDLE(wps_nfc)	(((wps_nfc)->cmd_status == 0) || WPS_NFC_CMD_HAVE_RESULT(wps_nfc))
#define IS_WRING(wps_nfc)	((wps_nfc)->cmd_status == CMD_WRING)
#define IS_RDING(wps_nfc)	((wps_nfc)->cmd_status == CMD_RDING)
#define IS_HOING(wps_nfc)	((wps_nfc)->cmd_status == CMD_HOING)
#define IS_FMING(wps_nfc)	((wps_nfc)->cmd_status == CMD_FMING)

/* Also clear con_cbstatus and buffer_len in  WPS_NFC_CMD_SET */
#define WPS_NFC_CMD_SET(wps_nfc, cmd)				\
	do {							\
		(wps_nfc)->cmd_status = (cmd) & CMD_MASK;	\
		(wps_nfc)->conn_cbstatus = 0;			\
		(wps_nfc)->buffer_len = 0;			\
		(void)time((time_t*)&wps_nfc_active_time);	\
	} while (0)

#define OP_ERROR()				\
	do {					\
		ret = NFC_ERROR; 		\
		state = WPS_NFC_OP_ERROR;	\
		goto err;			\
	} while (0)

#define WPS_NFC_UI_SET_ENV(name, value)				\
	do {							\
		char tmp[32];					\
		snprintf(tmp, sizeof(tmp), "%d", value);	\
		wps_ui_set_env(name, tmp);			\
	} while (0)

extern void wps_setProcessStates(int state);
void wps_nfc_done_cback(tNSA_CONN_EVT event, int status);

static int
_wps_nfc_find_ess(char *ifname)
{
	int i, imax;
	char tmp[] = "essXXXXXXXXXX_wlnames";
	char name[IFNAMSIZ];
	char *next = NULL, *wlnames;

	if (ifname == NULL)
		return -1;

	/* Apply settings to driver */
	imax = wps_get_ess_num();
	for (i = 0; i < imax; i++) {
		sprintf(tmp, "ess%d_wlnames", i);
		wlnames = wps_safe_get_conf(tmp);

		foreach(name, wlnames, next) {
			if (!strcmp(name, ifname)) {
				goto found;
			}
		}
	}

found:
	if (i == imax)
		i = -1;

	return i;
}

static int
_wps_nfc_ifname_upd(char *ifname)
{
	char tmp[100];

	if (ifname) {
		strncpy(wps_nfc->ifname, ifname, sizeof(wps_nfc->ifname));
		wps_nfc->ess_id = _wps_nfc_find_ess(wps_nfc->ifname);
		if (wps_nfc->ess_id == -1) {
			TUTRACE((TUTRACE_NFC, "Can't find LAN instance for %s!!\n",
				wps_nfc->ifname));
			return -1;
		}

		sprintf(tmp, "ess%d_wps_oob", wps_nfc->ess_id);
		if (!strcmp(wps_safe_get_conf(tmp), "disabled") ||
		    !strcmp(wps_safe_get_conf("wps_oob_configured"), "1"))
			wps_nfc->config_state = WPS_SCSTATE_CONFIGURED;
		else
			wps_nfc->config_state = WPS_SCSTATE_UNCONFIGURED;
	}

	return 0;
}

static void
_wps_nfc_print_data(void *data, unsigned int type, char *title)
{
#ifdef WPS_NFC_PRINT_CREDENTIAL
	if (title)
		TUTRACE((TUTRACE_NFC, "\n%s\n", title));

	if (type == WPS_NFC_RD_CFG) {
		WpsEnrCred *cred = (WpsEnrCred *)data;
		char keystr[65];

		TUTRACE((TUTRACE_NFC, "SSID = %s\n", cred->ssid));
		TUTRACE((TUTRACE_NFC, "Key Mgmt type is %s\n", cred->keyMgmt));
		strncpy(keystr, cred->nwKey, cred->nwKeyLen);
		keystr[cred->nwKeyLen] = 0;
		TUTRACE((TUTRACE_NFC, "Key : %s\n", keystr));
		if (cred->encrType == 0) {
			TUTRACE((TUTRACE_NFC, "Encryption : NONE\n"));
		}
		else {
			if (cred->encrType & ENCRYPT_WEP) {
				TUTRACE((TUTRACE_NFC, "Encryption :  WEP\n"));
				TUTRACE((TUTRACE_NFC, "WEP Index: %d\n", cred->wepIndex));
			}
			if (cred->encrType & ENCRYPT_TKIP)
				TUTRACE((TUTRACE_NFC, "Encryption :  TKIP\n"));
			if (cred->encrType & ENCRYPT_AES)
				TUTRACE((TUTRACE_NFC, "Encryption :  AES\n"));
		}
	}
	else if (type == WPS_NFC_RD_PW) {
		WpsOobDevPw *p_oobDevPw = (WpsOobDevPw *)data;

		WPS_HexDumpAscii("Public Key Hash Data:", -1, p_oobDevPw->pub_key_hash,
			sizeof(p_oobDevPw->pub_key_hash));
		printf("Password ID: 0x%x\n", p_oobDevPw->devPwdId);
		WPS_HexDumpAscii("Device Password: HEX Format", -1, (uint8 *)&p_oobDevPw->pin,
			p_oobDevPw->pin_len);
	}
#endif /* WPS_NFC_PRINT_CREDENTIAL */
}

static int
_wps_nfc_stop()
{
	int ret = 0;

	/* Cmd */
	if (IS_WRING(wps_nfc)) {
		TUTRACE((TUTRACE_NFC, "WPS NFC: Write Stop\n"));
	}
	else if (IS_RDING(wps_nfc)) {
		TUTRACE((TUTRACE_NFC, "WPS NFC: Read Stop\n"));
	}
	else if (IS_HOING(wps_nfc)) {
		TUTRACE((TUTRACE_NFC, "WPS NFC: HandOver Stop\n"));
	}
	else {
		TUTRACE((TUTRACE_NFC, "WPS NFC: Unknown command %d\n", wps_nfc->cmd_status));
		ret = -1;
		goto err;
	}

	if (IS_HOING(wps_nfc))
		ret = app_nsa_cho_stop();
	else
		ret = app_nsa_rw_stop();

	if (ret) {
		TUTRACE((TUTRACE_NFC, "NDA RW Stop failed\n"));
	}

err:
	return ret;
}

static int
_wps_nfc_parse_ndef(WPS_SCSTATE type, uint8 *data, unsigned int data_len)
{
	int restart, ret = WPS_CONT;
	WpsEnrCred cred;

	if (type == WPS_NFC_RD_CFG)
		ret = nfc_utils_parse_cfg(&cred, wps_nfc->version2, data, data_len);
	else
		ret = nfc_utils_parse_pw(&wps_nfc->oob_dev_pw, wps_nfc->version2, data, data_len);

	if (ret != WPS_SUCCESS)
		goto error;

	if (type == WPS_NFC_RD_PW) {
		/* Do Hex to String translation */
		if (!wps_hex2str(wps_nfc->dev_pw_str, sizeof(wps_nfc->dev_pw_str),
			wps_nfc->oob_dev_pw.pin, wps_nfc->oob_dev_pw.pin_len)) {
			ret = WPS_ERR_INVALID_PARAMETERS;
			goto error;
		}
	}

	_wps_nfc_print_data((type == WPS_NFC_RD_CFG) ?
		(void*)&cred : (void*)&wps_nfc->oob_dev_pw, type, "WPS Token Data");

	if (type == WPS_NFC_RD_PW)
		return NFC_RD_PW_SUCCESS;

	/* Apply settings to driver only for WPS_NFC_RD_CFG */
	restart = wps_set_wsec(wps_nfc->ess_id, wps_nfc->ifname, &cred,
		wps_is_wps_sta(wps_nfc->ifname) ? SCMODE_STA_ENROLLEE : SCMODE_AP_ENROLLEE);
	if (restart)
		ret = NFC_RD_CFG_SUCCESS_RESTART;
	else
		ret = NFC_RD_CFG_SUCCESS;

error:

	return ret;
}

static void
_wps_nfc_rw_cplt_cback(tNSA_STATUS status)
{
	if (!IS_WRING(wps_nfc) && !IS_RDING(wps_nfc) &&
	    TRUE)
		return;

	/* RW/Format not complete, we need to wait NSA_RW_STOP_EVT before
	 * we do others, so here we just update the state and keep continue.
	 * Just update the cnn_cbstate
	 */
	wps_nfc->conn_cbstatus = status;
}

void wps_nfc_dm_cback(tNSA_CONN_EVT event, int status)
{
	int dm_status;

	/* Just update the dm_status */
	if (event == NSA_DM_ENABLED_EVT) {
		if (status == 0)
			dm_status = WPS_UI_NFC_STATUS_INITED;
		else
			dm_status = status;

		WPS_NFC_UI_SET_ENV("wps_nfc_dm_status", dm_status);
	}
}

void
wps_nfc_conn_cback(tNSA_CONN_EVT event, tNSA_STATUS status)
{
	if (IS_IDLE(wps_nfc))
		return;

	/* Filter what we are interesting */
	switch (event) {
	case NSA_READ_CPLT_EVT:
	case NSA_WRITE_CPLT_EVT:
	case NSA_FORMAT_CPLT_EVT:
		_wps_nfc_rw_cplt_cback(status);
		break;

	case NSA_DEACTIVATED_EVT:
		/* We use NSA_DEACTIVATED_EVT as done/stop even for CHO */
		if (!IS_HOING(wps_nfc))
			return;

		wps_nfc_done_cback(event, status);
		break;

	default:
		return;
	}
}

void
wps_nfc_ndef_cback(UINT8 *ndef, UINT32 ndef_len)
{
	/* Specially parse "ChoSendSelectSuccess" for CHO selector */
	if (ndef_len == sizeof("ChoSendSelectSuccess") &&
	    !strncmp((char *)ndef, "ChoSendSelectSuccess", ndef_len)) {
		strncpy((char *)wps_nfc->buffer, (char *)ndef, ndef_len);
		wps_nfc->buffer_len = ndef_len;
		goto out;
	}

	/* Just parse NDEF to our local buffer */
	wps_nfc->buffer_len = ParseNDefData_WSC(ndef, ndef_len, wps_nfc->buffer,
		sizeof(wps_nfc->buffer));

out:
	/* Update states if we are in CHO */
	if (IS_HOING(wps_nfc)) {
		wps_setProcessStates(WPS_NFC_HO_NDEF);
	}
}

/* Process message from NFC (nfcapp_wps) application */
void
wps_nfc_done_cback(tNSA_CONN_EVT event, int status)
{
	int ret = WPS_CONT;
	int restart = 0;
	int sc_mode = wps_is_wps_sta(wps_nfc->ifname) ? SCMODE_STA_ENROLLEE : SCMODE_AP_ENROLLEE;

	TUTRACE((TUTRACE_NFC, "WPS NFC: Receive event %d status %d from NFC\n", event, status));

	if (IS_IDLE(wps_nfc))
		goto out;

	if (IS_WRING(wps_nfc)) {
		/* Write complete */
		WPS_NFC_CMD_RESULT_SET(wps_nfc, CMD_RESULT_CPLT);
		if (status != 0 || wps_nfc->conn_cbstatus != 0) {
			wps_nfc->state = WPS_NFC_OP_ERROR;
			wps_setProcessStates(WPS_NFC_OP_ERROR);
			WPS_NFC_UI_SET_ENV("wps_nfc_err_code",
				wps_nfc->conn_cbstatus ? wps_nfc->conn_cbstatus : status);
			TUTRACE((TUTRACE_NFC, "WPS NFC: Write failed, statue %d "
				"conn_cbstatus %d\n", status, wps_nfc->conn_cbstatus));
			ret = NFC_ERROR;
			goto out;
		}

		/* Write NFC password successful */
		if (wps_nfc->state == WPS_NFC_WR_PW)
			ret = NFC_WR_PW_SUCCESS;
		else {
			/* Apply settings to driver if we are in OOB */
			if (wps_nfc->config_state == WPS_SCSTATE_UNCONFIGURED) {
				restart = wps_set_wsec(wps_nfc->ess_id, wps_nfc->ifname,
					&wps_nfc->cred, sc_mode);
			}
			ret = restart ? NFC_WR_CFG_SUCCESS_RESTART : NFC_WR_CFG_SUCCESS;
		}

		wps_nfc->state = WPS_NFC_WR_CPLT;
		wps_setProcessStates(WPS_NFC_WR_CPLT);
	}
	else if (IS_RDING(wps_nfc)) {
		/* Read complete */
		WPS_NFC_CMD_RESULT_SET(wps_nfc, CMD_RESULT_CPLT);
		if (status != 0 || wps_nfc->conn_cbstatus != 0 || wps_nfc->buffer_len == 0) {
			wps_nfc->state = WPS_NFC_OP_ERROR;
			wps_setProcessStates(WPS_NFC_OP_ERROR);
			WPS_NFC_UI_SET_ENV("wps_nfc_err_code",
				wps_nfc->conn_cbstatus ? wps_nfc->conn_cbstatus : status);
			TUTRACE((TUTRACE_NFC, "WPS NFC: Read failed, statue %d "
				"conn_cbstatus %d wps_nfc->buffer_len %d\n",
				status, wps_nfc->conn_cbstatus));
			ret = NFC_ERROR;
			goto out;
		}

		/* Parse NDEF */
		ret = _wps_nfc_parse_ndef(wps_nfc->state, wps_nfc->buffer, wps_nfc->buffer_len);

		/* Set process states */
		if (ret != NFC_RD_CFG_SUCCESS && ret != NFC_RD_CFG_SUCCESS_RESTART &&
		    ret != NFC_RD_PW_SUCCESS) {
			wps_nfc->state = WPS_NFC_OP_ERROR;
			wps_setProcessStates(WPS_NFC_OP_ERROR);
			WPS_NFC_UI_SET_ENV("wps_nfc_err_code", 0);
			ret = NFC_ERROR;
		}
		else {
			wps_nfc->state = WPS_NFC_RD_CPLT;
			wps_setProcessStates(WPS_NFC_RD_CPLT);
		}
	}
	else if (IS_HOING(wps_nfc)) {

		/* Stop CHO */
		_wps_nfc_stop();

		/* CHO complete */
		WPS_NFC_CMD_RESULT_SET(wps_nfc, CMD_RESULT_CPLT);

		if (status != 0 || wps_nfc->buffer_len == 0) {
			wps_nfc->state = WPS_NFC_OP_ERROR;
			wps_setProcessStates(WPS_NFC_OP_ERROR);
			WPS_NFC_UI_SET_ENV("wps_nfc_err_code",
				wps_nfc->conn_cbstatus ? wps_nfc->conn_cbstatus : status);
			TUTRACE((TUTRACE_NFC, "WPS NFC: CHO failed, statue %d "
				"wps_nfc->buffer_len %d\n", status, wps_nfc->buffer_len));
			ret = NFC_ERROR;
			goto out;
		}

		/* CHO-R */
		if (wps_nfc->state == WPS_NFC_HO_R) {
			ret = _wps_nfc_parse_ndef(WPS_NFC_RD_CFG, wps_nfc->buffer,
				wps_nfc->buffer_len);

			/* Set process states */
			if (ret != NFC_RD_CFG_SUCCESS && ret != NFC_RD_CFG_SUCCESS_RESTART) {
				wps_nfc->state = WPS_NFC_OP_ERROR;
				wps_setProcessStates(WPS_NFC_OP_ERROR);
				WPS_NFC_UI_SET_ENV("wps_nfc_err_code", 0);
				ret = NFC_ERROR;
			}
			else {
				wps_nfc->state = WPS_NFC_HO_CPLT;
				wps_setProcessStates(WPS_NFC_HO_CPLT);
			}
		} /* CHO-S */
		else if (wps_nfc->state == WPS_NFC_HO_S) {
			WPS_HexDumpAscii("WPS_NFC_HO_R", -1, wps_nfc->buffer, wps_nfc->buffer_len);
			/* Set process states */
			wps_nfc->state = WPS_NFC_HO_CPLT;
			wps_setProcessStates(WPS_NFC_HO_CPLT);
			/* Apply settings to driver if we are in OOB */
			if (wps_nfc->config_state == WPS_SCSTATE_UNCONFIGURED) {
				restart = wps_set_wsec(wps_nfc->ess_id, wps_nfc->ifname,
					&wps_nfc->cred, sc_mode);
			}
			ret = restart ? NFC_WR_CFG_SUCCESS_RESTART : NFC_WR_CFG_SUCCESS;
		}
		else {
			ret = NFC_ERROR;
			WPS_NFC_UI_SET_ENV("wps_nfc_err_code", 0);
		}
	}
	else {
		TUTRACE((TUTRACE_NFC, "Unknown state %d\n", wps_nfc->state));
	}

out:
	wps_nfc->cback_retcode = ret;

	return;
}

/* Request write NFC WPS Configuration */
int
wps_nfc_write_cfg(char *ifname, DevInfo *info)
{
	int ret = WPS_CONT;
	int err_code = 0;
	uint32 len;
	WPS_SCSTATE state = WPS_NFC_WR_CFG;

	if (!IS_IDLE(wps_nfc)) {
		TUTRACE((TUTRACE_NFC, "cmd_status = %d, not idle\n", wps_nfc->cmd_status));
		OP_ERROR();
	}

	len = sizeof(wps_nfc->buffer);
	if (nfc_utils_build_cfg(info, wps_nfc->buffer, &len) != WPS_SUCCESS) {
		OP_ERROR();
	}

	/* Save the credential in local in case we need to do wps_set_wsec */
	nfc_utils_parse_cfg(&wps_nfc->cred, wps_nfc->version2, wps_nfc->buffer, len);

	if (_wps_nfc_ifname_upd(ifname) == -1) {
		OP_ERROR();
	}

	TUTRACE((TUTRACE_NFC, "WPS NFC: Write Configuration\n"));
	err_code = app_nsa_rw_write_wps_with_payload(wps_nfc->buffer, len);
	if (err_code) {
		TUTRACE((TUTRACE_NFC, "NSA RW Write failed\n"));
		OP_ERROR();
	}

	WPS_NFC_CMD_SET(wps_nfc, CMD_WRING);

err:
	wps_nfc->state = state;
	wps_setProcessStates(wps_nfc->state);
	WPS_NFC_UI_SET_ENV("wps_nfc_err_code", err_code);

	return ret;
}

/* Request read NFC WPS Configuration */
int
wps_nfc_read_cfg(char *ifname)
{
	int ret = WPS_CONT;
	int err_code = 0;
	WPS_SCSTATE state = WPS_NFC_RD_CFG;

	if (!IS_IDLE(wps_nfc)) {
		TUTRACE((TUTRACE_NFC, "cmd_status = %d, not idle\n", wps_nfc->cmd_status));
		OP_ERROR();
	}

	if (_wps_nfc_ifname_upd(ifname) == -1) {
		OP_ERROR();
	}

	TUTRACE((TUTRACE_NFC, "WPS NFC: Read Configuration\n"));
	err_code = app_nsa_rw_read_wps();
	if (err_code) {
		TUTRACE((TUTRACE_NFC, "NSA RW Read Configuration failed\n"));
		OP_ERROR();
	}

	WPS_NFC_CMD_SET(wps_nfc, CMD_RDING);

err:
	wps_nfc->state = state;
	wps_setProcessStates(wps_nfc->state);
	WPS_NFC_UI_SET_ENV("wps_nfc_err_code", err_code);

	return ret;
}

/* Construct the WPS Password payload */
int
wps_nfc_write_pw(char *ifname, DevInfo *info)
{
	int ret = WPS_CONT;
	int err_code = 0;
	uint32 len;
	WPS_SCSTATE state = WPS_NFC_WR_PW;

	if (!IS_IDLE(wps_nfc)) {
		TUTRACE((TUTRACE_NFC, "cmd_status = %d, not idle\n", wps_nfc->cmd_status));
		OP_ERROR();
	}

	len = sizeof(wps_nfc->buffer);
	if (nfc_utils_build_pw(info, wps_nfc->buffer, &len) != WPS_SUCCESS) {
		OP_ERROR();
	}

	/* Save the OOB device password */
	memcpy(wps_nfc->pre_privkey, info->pre_privkey, SIZE_PUB_KEY);
	memcpy(wps_nfc->oob_dev_pw.pub_key_hash, info->pub_key_hash, SIZE_160_BITS);
	wps_nfc->oob_dev_pw.devPwdId = info->devPwdId;

	/* Do String to Hex translation */
	wps_strncpy(wps_nfc->dev_pw_str, info->pin, sizeof(wps_nfc->dev_pw_str));
	wps_nfc->oob_dev_pw.pin_len = wps_str2hex(wps_nfc->oob_dev_pw.pin,
		sizeof(wps_nfc->oob_dev_pw.pin), info->pin);
	if (wps_nfc->oob_dev_pw.pin_len == 0) {
		TUTRACE((TUTRACE_NFC, "NSA RW invalid parameters\n"));
		OP_ERROR();
	}

	if (_wps_nfc_ifname_upd(ifname) == -1) {
		OP_ERROR();
	}

	TUTRACE((TUTRACE_NFC, "WPS NFC: Write Password\n"));
	err_code = app_nsa_rw_write_wps_with_payload(wps_nfc->buffer, len);
	if (err_code) {
		TUTRACE((TUTRACE_NFC, "NSA RW Write failed\n"));
		OP_ERROR();
	}

	WPS_NFC_CMD_SET(wps_nfc, CMD_WRING);

err:
	wps_nfc->state = state;
	wps_setProcessStates(wps_nfc->state);
	WPS_NFC_UI_SET_ENV("wps_nfc_err_code", err_code);

	return ret;
}

/* Request read NFC WPS Password */
int
wps_nfc_read_pw(char *ifname)
{
	int ret = WPS_CONT;
	int err_code = 0;
	WPS_SCSTATE state = WPS_NFC_RD_PW;

	if (!IS_IDLE(wps_nfc)) {
		TUTRACE((TUTRACE_NFC, "cmd_status = %d, not idle\n", wps_nfc->cmd_status));
		OP_ERROR();
	}

	if (_wps_nfc_ifname_upd(ifname) == -1) {
		OP_ERROR();
	}

	TUTRACE((TUTRACE_NFC, "WPS NFC: Read Password\n"));
	err_code = app_nsa_rw_read_wps();
	if (err_code) {
		TUTRACE((TUTRACE_NFC, "NSA RW Read Password failed\n"));
		OP_ERROR();
	}

	WPS_NFC_CMD_SET(wps_nfc, CMD_RDING);

err:
	wps_nfc->state = state;
	wps_setProcessStates(wps_nfc->state);
	WPS_NFC_UI_SET_ENV("wps_nfc_err_code", err_code);

	return ret;
}

/* NFC WPS Hand over selector */
int
wps_nfc_ho_selector(char *ifname, DevInfo *info)
{
	int ret = WPS_CONT;
	int err_code = 0;
	uint32 len;
	WPS_SCSTATE state = WPS_NFC_HO_S;

	if (!IS_IDLE(wps_nfc)) {
		TUTRACE((TUTRACE_NFC, "cmd_status = %d, not idle\n", wps_nfc->cmd_status));
		OP_ERROR();
	}

	len = sizeof(wps_nfc->buffer);
	if (nfc_utils_build_cfg(info, wps_nfc->buffer, &len) != WPS_SUCCESS) {
		OP_ERROR();
	}

	/* Save the credential in local in case we need to do wps_set_wsec */
	nfc_utils_parse_cfg(&wps_nfc->cred, wps_nfc->version2, wps_nfc->buffer, len);

	if (_wps_nfc_ifname_upd(ifname) == -1) {
		OP_ERROR();
	}

	TUTRACE((TUTRACE_NFC, "WPS NFC: HO selector write configuration\n"));
	err_code = app_nsa_cho_start_wps_with_payload(TRUE, wps_nfc->buffer, len);
	if (err_code) {
		TUTRACE((TUTRACE_NFC, "NSA CHO Selector failed\n"));
		OP_ERROR();
	}

	WPS_NFC_CMD_SET(wps_nfc, CMD_HOING);

err:
	wps_nfc->state = state;
	wps_setProcessStates(wps_nfc->state);
	WPS_NFC_UI_SET_ENV("wps_nfc_err_code", err_code);

	return ret;
}

/* NFC WPS Hand over requester */
int
wps_nfc_ho_requester(char *ifname)
{
	int ret = WPS_CONT;
	int err_code = 0;
	WPS_SCSTATE state = WPS_NFC_HO_R;

	if (!IS_IDLE(wps_nfc)) {
		TUTRACE((TUTRACE_NFC, "cmd_status = %d, not idle\n", wps_nfc->cmd_status));
		OP_ERROR();
	}

	if (_wps_nfc_ifname_upd(ifname) == -1) {
		OP_ERROR();
	}

	TUTRACE((TUTRACE_NFC, "WPS NFC: HO requester read configuration\n"));
	if (app_nsa_cho_start_wps_with_payload(FALSE, NULL, 0)) {
		TUTRACE((TUTRACE_NFC, "NSA CHO start failed\n"));
		OP_ERROR();
	}

	err_code = app_nsa_cho_send_wps();
	if (err_code) {
		TUTRACE((TUTRACE_NFC, "NSA CHO Requester failed\n"));
		OP_ERROR();
	}

	WPS_NFC_CMD_SET(wps_nfc, CMD_HOING);

err:
	wps_nfc->state = state;
	wps_setProcessStates(wps_nfc->state);
	WPS_NFC_UI_SET_ENV("wps_nfc_err_code", err_code);

	return ret;
}

/* Request stop current NFC operation */
int
wps_nfc_stop(char *ifname)
{
	int ret = WPS_CONT;
	int err_code = 0;
	WPS_SCSTATE state = WPS_NFC_OP_STOP;

	if (IS_IDLE(wps_nfc)) {
		TUTRACE((TUTRACE_NFC, "cmd_status = %d, idle\n", wps_nfc->cmd_status));
		OP_ERROR();
	}

	if (_wps_nfc_ifname_upd(ifname) == -1) {
		OP_ERROR();
	}

	TUTRACE((TUTRACE_NFC, "WPS NFC: Stop\n"));
	err_code = _wps_nfc_stop(FALSE, NULL, 0);
	if (err_code) {
		OP_ERROR();
	}

	WPS_NFC_CMD_RESULT_SET(wps_nfc, CMD_RESULT_STOP);

err:
	wps_nfc->state = state;
	wps_setProcessStates(wps_nfc->state);
	WPS_NFC_UI_SET_ENV("wps_nfc_err_code", err_code);

	return ret;
}


uint8 *
wps_nfc_priv_key()
{
	return wps_nfc->pre_privkey;
}

uint8 *
wps_nfc_pub_key_hash()
{
	return wps_nfc->oob_dev_pw.pub_key_hash;
}

unsigned short
wps_nfc_pw_id()
{
	return wps_nfc->oob_dev_pw.devPwdId;
}

char *
wps_nfc_dev_pw_str()
{
	return wps_nfc->dev_pw_str;
}

bool
wps_nfc_is_idle()
{
	if (IS_IDLE(wps_nfc))
		return TRUE;
	else
		return FALSE;
}

/*
 *
 * Function         main
 *
 * Description      This is the main Custom Management callback function.
 *
 * Parameters
 *
 * Returns          void
 *
 */
BOOLEAN
app_nsa_mgt_custom_cback(tNSA_MGT_EVT event, tNSA_MGT_MSG *p_data)
{
	switch (event) {
	case NSA_MGT_STATUS_EVT:
		break;

	case NSA_MGT_DISCONNECT_EVT:
		/* Connection with the Server lost => close application */
		TUTRACE((TUTRACE_NFC, "Disconnect with NFC server, reason:%d",
			p_data->disconnect.reason));
		break;
	}

	TUTRACE((TUTRACE_NFC, " app_nsa_mgt_custom_cback event %d", event));

	return FALSE;
}

int
wps_nfc_init()
{
	char *value;

	/* Init control structure */
	memset(&s_wps_nfc, 0, sizeof(s_wps_nfc));
	s_wps_nfc.cback_retcode = WPS_CONT;
	WPS_NFC_UI_SET_ENV("wps_nfc_dm_status", WPS_UI_NFC_STATUS_INITING);
	WPS_NFC_UI_SET_ENV("wps_nfc_err_code", 0);

	/* Init a dummy nfc_hndl */
	memset(&nfc_hndl, 0, sizeof(nfc_hndl));
	nfc_hndl.type = WPS_RECEIVE_PKT_NFC;

	/* WPS 2.0 number */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		wps_nfc->version2 = (uint8)(strtoul(wps_get_conf("wps_version2_num"), NULL, 16));

	wps_nfc->nfc_sleep1 = 2;
	value = wps_get_conf("nfc_sleep1");
	if (value)
		wps_nfc->nfc_sleep1 = atoi(value);

	wps_nfc->nfc_sleep2 = 2;
	value = wps_get_conf("nfc_sleep2");
	if (value)
		wps_nfc->nfc_sleep2 = atoi(value);

	/* Must wait a short time for nsa_server ready */
	if (wps_nfc->nfc_sleep1)
		WpsSleep(wps_nfc->nfc_sleep1);

	app_nsa_gen_init();

	/* Open NSA connection */
	if (app_mgt_open("/tmp/", app_nsa_mgt_custom_cback, FALSE)) {
		TUTRACE((TUTRACE_NFC, "Couldn't open successfully"));
		return -1;
	}

	if (wps_nfc->nfc_sleep2)
		WpsSleep(wps_nfc->nfc_sleep2);

	if (app_nsa_dm_enable_wps()) {
		TUTRACE((TUTRACE_NFC, "Couldn't enable device manager"));
	}

	return 0;
}

void
wps_nfc_deinit()
{
	/* Terminate the profile */
	app_nsa_end();

	/* Close the BSA connection */
	app_mgt_close();

	return;
}

int
wps_nfc_process_msg(char *nfcmsg, int nfcmsg_len)
{
	int ret;

	/* Get */
	ret = wps_nfc->cback_retcode;

	/* Reset */
	wps_nfc->cback_retcode = WPS_CONT;

	return ret;
}

wps_hndl_t *
wps_nfc_check(char *buf, int *buflen)
{
	if (wps_nfc->cback_retcode == WPS_CONT)
		return NULL;

	/* Just return empty dummy nfc handle */
	return &nfc_hndl;
}

void
wps_nfc_timeout()
{
	time_t curr_time;

	if (IS_IDLE(wps_nfc) || IS_FMING(wps_nfc))
		return;

	if (IS_WRING(wps_nfc) || IS_RDING(wps_nfc) ||
	    IS_HOING(wps_nfc)) {
		(void)time((time_t*)&curr_time);
		if (curr_time < wps_nfc_active_time ||
		    (curr_time - wps_nfc_active_time) > WPS_NFC_MAX_TIMEOUT) {
			TUTRACE((TUTRACE_NFC, "WPS NFC: %d seconds timeout!\n",
				WPS_NFC_MAX_TIMEOUT));
			_wps_nfc_stop();
			WPS_NFC_CMD_RESULT_SET(wps_nfc, CMD_RESULT_TO);
			wps_nfc->state = WPS_NFC_OP_TO;
			wps_setProcessStates(WPS_NFC_OP_TO);
			wps_ui_reset_env();
		}
	}

	return;
}
