/*
 * Registrataion state machine
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: reg_sm.c 399862 2013-05-02 03:28:30Z $
 */

#ifdef WIN32
#include <windows.h>
#endif

#include <string.h>

#include <bn.h>
#include <wps_dh.h>
#include <wps_sha256.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <portability.h>
#include <tutrace.h>
#include <sminfo.h>
#include <reg_protomsg.h>
#include <reg_proto.h>
#include <statemachine.h>
#include <info.h>
#include <wpsapi.h>

#ifdef WFA_WPS_20_TESTBED
#define STR_ATTR_LEN(str, max) \
	((uint16)((b_zpadding && strlen(str) < max) ? strlen(str) + 1 : strlen(str)))
#else /* !WFA_WPS_20_TESTBED */
#define STR_ATTR_LEN(str, max)	((uint16)(strlen(str)))
#endif /* WFA_WPS_20_TESTBED */

static uint32 reg_proto_create_m2(RegData *regInfo, BufferObj *msg);
static uint32 reg_proto_create_m2d(RegData *regInfo, BufferObj *msg);
static uint32 reg_proto_create_m4(RegData *regInfo, BufferObj *msg);
static uint32 reg_proto_create_m6(RegData *regInfo, BufferObj *msg);
static uint32 reg_proto_create_m8(RegData *regInfo, BufferObj *msg);

static uint32 reg_proto_process_m1(RegData *regInfo, BufferObj *msg);
static uint32 reg_proto_process_m3(RegData *regInfo, BufferObj *msg);
static uint32 reg_proto_process_m5(RegData *regInfo, BufferObj *msg);
static uint32 reg_proto_process_m7(RegData *regInfo, BufferObj *msg, void **encrSettings);
static uint32 reg_proto_process_done(RegData *regInfo, BufferObj *msg);

/*
 * Include the UPnP Microsostack header file so an AP can send an EAP_FAIL
 * right after sending M2D if there are no currently-subscribed external Registrars.
 */
static uint32 reg_sm_localRegStep(RegSM *r, uint32 msgLen, uint8 *p_msg,
	uint8 *outbuffer, uint32 *out_len);

static uint32 reg_proto_m7ap_EncrSettingsCheck(void *encrSettings);

RegSM *
reg_sm_new(void *g_mc)
{
	RegSM *r = (RegSM *)malloc(sizeof(RegSM));

	if (!r)
		return NULL;

	memset(r, 0, sizeof(RegSM));

	TUTRACE((TUTRACE_INFO, "Registrar constructor\n"));

	r->reg_info = state_machine_new();
	if (r->reg_info == NULL) {
		free(r);
		return NULL;
	}

	r->m_sentM2 = false;
	r->g_mc = g_mc;

	r->m_er_sentM2 = false;

	return r;
}

void
reg_sm_delete(RegSM *r)
{
	if (r) {
		if (r->mp_peerEncrSettings) {
			reg_msg_es_del(r->mp_peerEncrSettings, 0);
			r->mp_peerEncrSettings = NULL;
		}

		state_machine_delete(r->reg_info);

		TUTRACE((TUTRACE_INFO, "Free RegSM %p\n", r));
		free(r);
	}
}

uint32
reg_sm_initsm(RegSM *r, DevInfo *dev_info,
	bool enableLocalSM, bool enablePassthru)
{
	uint32 err;

	if ((!enableLocalSM) && (!enablePassthru)) {
		TUTRACE((TUTRACE_ERR, "REGSM: enableLocalSM and enablePassthru are "
			"both false.\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	/* if the device is an AP, passthru MUST be enabled */
	if ((dev_info->b_ap) && (!enablePassthru)) {
		TUTRACE((TUTRACE_ERR, "REGSM: AP Passthru not enabled.\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	r->m_localSMEnabled = enableLocalSM;
	r->m_passThruEnabled = enablePassthru;

	err = state_machine_init_sm(r->reg_info);
	if (WPS_SUCCESS != err)
		return err;

	r->reg_info->enrollee = NULL;
	r->reg_info->registrar = dev_info;
	r->reg_info->dev_info = dev_info;

	return WPS_SUCCESS;
}

/*
 * This method is used by the Proxy to send packets to
 * the various registrars. If the local registrar is
 * enabled, this method will call it using RegStepSM
 */
void
reg_sm_restartsm(RegSM *r)
{
	uint32 err;
	RegData *reg_info;
	DevInfo *dev_info;

	TUTRACE((TUTRACE_INFO, "SM: Restarting the State Machine\n"));

	/* first extract the all info we need to save */
	reg_info = r->reg_info;
	dev_info = reg_info->dev_info;

	err = state_machine_init_sm(reg_info);
	if (WPS_SUCCESS != err) {
		return;
	}

	r->reg_info->dev_info = dev_info;
	r->reg_info->registrar = dev_info;
	r->reg_info->initialized = true;

	r->m_sentM2 = false;
	r->m_er_sentM2 = false;
}

uint32
reg_sm_step(RegSM *r, uint32 msgLen, uint8 *p_msg, uint8 *outbuffer, uint32 *out_len)
{
	uint32 err = WPS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "REGSM: Entering Step.\n"));
	if (false == r->reg_info->initialized) {
		TUTRACE((TUTRACE_ERR, "REGSM: Not yet initialized.\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	/*
	 * Irrespective of whether the local registrar is enabled or whether we're
	 * using an external registrar, we need to send a WPS_Start over EAP to
	 * kickstart the protocol.
	 * If we send a WPS_START msg, we don't need to do anything else i.e.
	 * invoke the local registrar or pass the message over UPnP
	 */
	if ((!msgLen) && (START == r->reg_info->e_smState)&&
		((TRANSPORT_TYPE_EAP == r->reg_info->transportType) ||
		(TRANSPORT_TYPE_UPNP_CP == r->reg_info->transportType))) {
		/* The outside wps_regUpnpForwardingCheck already handle it for us */
		return WPS_CONT; /* no more processing for WPS_START */
	}

	/*
	 * Send this message to the local registrar only if it hasn't
	 * arrived from UPnP. This is needed on an AP with passthrough
	 * enabled so that the local registrar doesn't
	 * end up processing messages coming over UPnP from a remote registrar
	 */
	if ((r->m_localSMEnabled) && ((! r->m_passThruEnabled) ||
		((r->reg_info->transportType != TRANSPORT_TYPE_UPNP_DEV) &&
		(r->reg_info->transportType != TRANSPORT_TYPE_UPNP_CP)))) {
			TUTRACE((TUTRACE_INFO, "REGSM: Sending msg to local registrar.\n"));
			err = reg_sm_localRegStep(r, msgLen, p_msg, outbuffer, out_len);
			return err;
	}

	return WPS_CONT;
}

static uint32
reg_sm_localRegStep(RegSM *r, uint32 msgLen, uint8 *p_msg, uint8 *outbuffer, uint32 *out_len)

{
	uint32 err, retVal;
	BufferObj *inMsg = NULL, *outMsg = NULL;

	TUTRACE((TUTRACE_INFO, "REGSM: Entering RegSMStep.\n"));

	TUTRACE((TUTRACE_INFO, "REGSM: Recvd message of length %d\n", msgLen));

	if ((!p_msg || !msgLen) && (START != r->reg_info->e_smState)) {
		TUTRACE((TUTRACE_ERR, "REGSM: Wrong input parameters, smstep = %d\n",
			r->reg_info->e_smState));
		reg_sm_restartsm(r);
		return WPS_CONT;
	}

	if ((MNONE == r->reg_info->e_lastMsgSent) && (msgLen)) {
		/*
		 * We've received a msg from the enrollee, we don't want to send
		 * WPS_Start in this case
		 * Change last message sent from MNONE to MSTART
		 */
		r->reg_info->e_lastMsgSent = MSTART;
	}

	inMsg = buffobj_new();
	if (!inMsg)
		return WPS_ERR_OUTOFMEMORY;

	buffobj_dserial(inMsg, p_msg, msgLen);
	err = reg_proto_check_nonce(r->reg_info->registrarNonce,
		inMsg, WPS_ID_REGISTRAR_NONCE);

	/* make an exception for M1. It won't have the registrar nonce */
	if ((WPS_SUCCESS != err) && (RPROT_ERR_REQD_TLV_MISSING != err)) {
		TUTRACE((TUTRACE_INFO, "REGSM: Recvd message is not meant for"
				" this registrar. Ignoring...\n"));
		buffobj_del(inMsg);
		return WPS_CONT;
	}

	TUTRACE((TUTRACE_INFO, "REGSM: Calling HandleMessage...\n"));
	outMsg = buffobj_setbuf(outbuffer, *out_len);
	if (!outMsg) {
		buffobj_del(inMsg);
		return WPS_ERR_OUTOFMEMORY;
	}

	retVal = reg_sm_handleMessage(r, inMsg, outMsg);
	*out_len = buffobj_Length(outMsg);

	buffobj_del(inMsg);
	buffobj_del(outMsg);

	/* now check the state so we can act accordingly */
	switch (r->reg_info->e_smState) {
	case START:
	case CONTINUE:
		/* do nothing. */
		break;

	case RESTART:
		/* reset the SM */
		reg_sm_restartsm(r);
		break;

	case SUCCESS:
		TUTRACE((TUTRACE_ERR, "REGSM: Notifying MC of success.\n"));

		/* reset the SM */
		reg_sm_restartsm(r);

		if (retVal == WPS_SEND_MSG_CONT)
			return WPS_SEND_MSG_SUCCESS;
		else
			return WPS_SUCCESS;

	case FAILURE:
		TUTRACE((TUTRACE_ERR, "REGSM: Notifying MC of failure.\n"));

		/* reset the SM */
		reg_sm_restartsm(r);

		if (retVal == WPS_SEND_MSG_CONT)
			return WPS_SEND_MSG_ERROR;
		else
			return WPS_MESSAGE_PROCESSING_ERROR;

	default:
		break;
	}

	return retVal;
}

/* AP stay in pending when PBC pushed, if AP detected PBC overlap from
 * the M1 UUID-E mismatch case, it will just send M2D instead M2
 */
static uint32
_reg_sm_m1_pbc_overlap(RegData *regInfo)
{
	int uuid_cnt = 0, found = 0;
	int i, len;
	uint8 null_uuid[SIZE_16_BYTES] = {0};
	DevInfo *enrollee;

	/* According to WSC 2.0 specification sectin 11.3,
	 * Return WPS_ERR_OVERLAP if Probe Request UUID-E list is not empty
	 * and M1 UUID-E cannot be found in Probe Request UUID-E list
	 */
	if (regInfo->dev_info->devPwdId == WPS_DEVICEPWDID_PUSH_BTN) {
		len = sizeof(regInfo->dev_info->pbc_uuids);

		for (i = 0; i < len; i += SIZE_16_BYTES) {
			if (memcmp(&regInfo->dev_info->pbc_uuids[i], null_uuid,
				SIZE_16_BYTES))
				uuid_cnt++;
		}

		if (uuid_cnt == 0)
			return 0;

		if (uuid_cnt > 1) {
			TUTRACE((TUTRACE_ERR, "REGSM: PBC overlap detected (Multiple PBC)\n"));
			return 1;
		}

		/* There is a PBC station on the air */
		enrollee = regInfo->enrollee;
		for (i = 0; i < len; i += SIZE_16_BYTES) {
			if (!memcmp(&regInfo->dev_info->pbc_uuids[i], enrollee->uuid,
				SIZE_16_BYTES)) {
				found = 1;
				break;
			}
		}

		if (!found) {
			TUTRACE((TUTRACE_ERR, "REGSM: PBC overlap detected"
				" (UUID-E mismatched)\n"));
			return 1;
		}
	}

	return 0;
}

static char *_strsep(char **s, const char *ct)
{
	char *sbegin = *s;
	char *end;

	if (sbegin == NULL)
		return NULL;

	end = strpbrk(sbegin, ct);
	if (end)
		*end++ = '\0';
	*s = end;
	return sbegin;
}

/* Pass/Block: return 0/1 */
static uint32
_reg_sm_m1_pairing_auth(char *auth_str, TlvObj_ptr *device_name)
{
	char *entry, *next, *auth_str_dup = NULL;
	int len, pass = 0;

	if (!auth_str)
		return 0;

	if (!device_name || device_name->tlvbase.m_len == 0)
		return 1;

	auth_str_dup = malloc(strlen(auth_str)+1);
	if (!auth_str_dup) {
		TUTRACE((TUTRACE_ERR, "Memory allocate failed\n"));
		return 1;
	}
	strcpy(auth_str_dup, auth_str);
	next = auth_str_dup;

	entry = _strsep(&next, ";");
	while (entry) {
		len = strlen(entry);
		if (len == device_name->tlvbase.m_len &&
		    !strncmp(entry, device_name->m_data, len)) {
			pass = 1;
			goto done;
		}
		entry = _strsep(&next, ";");
	}

done:
	if (auth_str_dup)
		free(auth_str_dup);

	return pass ? 0 : 1;
}

uint32
reg_sm_handleMessage(RegSM *r, BufferObj *msg, BufferObj *outmsg)
{
	uint32 err, retVal = WPS_CONT;
	void *encrSettings = NULL;
	uint32 msgType, m1_pbc_overlap = 0;
	RegData *regInfo = r->reg_info;

	err = reg_proto_get_msg_type(&msgType, msg);
	if (WPS_SUCCESS != err) {
		TUTRACE((TUTRACE_ERR, "ENRSM: GetMsgType returned error: %d\n", err));
		goto out;
	}

	switch (r->reg_info->e_lastMsgSent) {
	case MSTART:
		if (WPS_ID_MESSAGE_M1 != msgType) {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected M1, received %d\n",
				msgType));
			TUTRACE((TUTRACE_INFO, "REGSM: Sending EAP FAIL\n"));
			/* upper application will send EAP FAIL and continue */
			retVal = WPS_SEND_FAIL_CONT;
			goto out;
		}

		/* Process M1 */
		err = reg_proto_process_m1(r->reg_info, msg);
		if (WPS_SUCCESS != err) {
			if (strlen(r->reg_info->dev_info->pin)) {
				retVal = err;
				TUTRACE((TUTRACE_ERR, "REGSM: Process M1 fail\n"));
			}
			else {
				TUTRACE((TUTRACE_ERR, "REGSM: Process M1 fail, continue\n"));
			}

			goto out;
		}
		r->reg_info->e_lastMsgRecd = M1;

		if (strlen(r->reg_info->dev_info->pin) &&
		    !(m1_pbc_overlap = _reg_sm_m1_pbc_overlap(r->reg_info))) {
			/* We don't send any encrypted settings currently */
			err = reg_proto_create_m2(r->reg_info, outmsg);
			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "REGSM: BuildMessageM2 err %x\n", err));
				/* upper application will send EAP FAIL and continue */
				retVal = WPS_SEND_FAIL_CONT;
				goto out;
			}
			r->reg_info->e_lastMsgSent = M2;

			/* Indicate M2 send out */
			wps_setProcessStates(WPS_SENDM2);

			/* temporary change */
			r->m_sentM2 = true;
			TUTRACE((TUTRACE_INFO, "REGSM: m_sentM2 = true !!\n"));
		}
		else {
			/* GUI still waiting for STA pin input.
			  * There is no password present, so send M2D
			  */
			err = reg_proto_create_m2d(r->reg_info, outmsg);
			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "REGSM: BuildMessageM2D err %x\n", err));
				/* upper application will send EAP FAIL and continue */
				retVal = WPS_SEND_FAIL_CONT;
				goto out;
			}
			r->reg_info->e_lastMsgSent = M2D;
			if (m1_pbc_overlap)
				r->err_code = WPS_ERR_PBC_OVERLAP;
		}

		/* Set state to CONTINUE */
		r->reg_info->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;

		/* For DTM WCN Wireless 463. M1-M2D Proxy Functionality */
		/* Delay send built-in registrar M2D */
		if (r->reg_info->e_lastMsgSent == M2D)
			retVal = WPS_AP_M2D_READY_CONT;
		break;

	case M2:
		if (WPS_ID_MESSAGE_M3 != msgType) {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected M3, received %d\n", msgType));
			goto out;
		}

		err = reg_proto_process_m3(r->reg_info, msg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Process M3 failed %d\n", msgType));
			/* Send a NACK to the Enrollee */
			retVal = state_machine_send_nack(r->reg_info, WPS_ERROR_DEVICE_BUSY,
				outmsg, NULL);
			goto out;
		}
		r->reg_info->e_lastMsgRecd = M3;

		err = reg_proto_create_m4(r->reg_info, outmsg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Build M4 failed %d\n", msgType));
			/* Send a NACK to the Enrollee */
			retVal = state_machine_send_nack(r->reg_info, WPS_ERROR_DEVICE_BUSY,
				outmsg, NULL);
			goto out;
		}
		r->reg_info->e_lastMsgSent = M4;

		/* set the message state to CONTINUE */
		r->reg_info->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M2D:
		if (WPS_ID_MESSAGE_ACK == msgType) {
			err = reg_proto_process_ack(r->reg_info, msg);
			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "REGSM: Expected ACK, received %d\n",
					msgType));
				goto out;
			}
		}
		else if (WPS_ID_MESSAGE_NACK == msgType) {
			uint16 configError = 0xffff;
			err = reg_proto_process_nack(r->reg_info, msg,	&configError);
			if (configError != WPS_ERROR_NO_ERROR) {
				TUTRACE((TUTRACE_ERR, "REGSM: Recvd NACK with config err: "
					"%d", configError));
			}
		}
		else {
			err = RPROT_ERR_WRONG_MSGTYPE;
		}

		r->reg_info->e_smState = CONTINUE;
		break;

	case M4:
		if (WPS_ID_MESSAGE_M5 != msgType) {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected M5 (0x09), "
					"received %0x\n", msgType));

			/*
			 * WPS spec 1.0h, in 6.10.4 external registrar should send
			 * NACK after receive NACK from AP (802.1X authenticator).
			 */
			if (!regInfo->registrar->b_ap && msgType == WPS_ID_MESSAGE_NACK) {
				uint16 configError = 0xffff;
				err = reg_proto_process_nack(r->reg_info, msg,
					&configError);
				if (configError != WPS_ERROR_NO_ERROR) {
					TUTRACE((TUTRACE_ERR, "REGSM: Recvd NACK"
						" with config err: %d", configError));
				}
				retVal = state_machine_send_nack(r->reg_info,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg, NULL);
			}

			goto pinfail;
		}

		err = reg_proto_process_m5(r->reg_info, msg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Processing M5 faile %d\n",
				msgType));
			/* Send a NACK to the Enrollee */
			if (err == RPROT_ERR_CRYPTO) {
				retVal = state_machine_send_nack(r->reg_info,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg, NULL);
				goto pinfail;
			}

			retVal = state_machine_send_nack(r->reg_info, WPS_ERROR_DEVICE_BUSY,
				outmsg, NULL);
			goto out;
		}
		r->reg_info->e_lastMsgRecd = M5;

		err = reg_proto_create_m6(r->reg_info, outmsg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Build M6 failed %d\n",
				msgType));
			/* Send a NACK to the Enrollee */
			retVal = state_machine_send_nack(r->reg_info,
				WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			goto out;
		}
		r->reg_info->e_lastMsgSent = M6;

		/* set the message state to CONTINUE */
		r->reg_info->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M6:
		if (WPS_ID_MESSAGE_M7 != msgType) {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected M7, received %d\n",
				msgType));

			/*
			 * WPS spec 1.0h, in 6.10.4 external registrar should send
			 * NACK after receive NACK from AP (802.1X authenticator).
			 */
			if (!regInfo->registrar->b_ap && msgType == WPS_ID_MESSAGE_NACK) {
				uint16 configError = 0xffff;
				err = reg_proto_process_nack(r->reg_info, msg,
					&configError);
				if (configError != WPS_ERROR_NO_ERROR) {
					TUTRACE((TUTRACE_ERR, "REGSM: Recvd NACK"
						" with config err: %d", configError));
				}
				retVal = state_machine_send_nack(r->reg_info,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg, NULL);
			}

			goto pinfail;
		}

		err = reg_proto_process_m7(r->reg_info, msg, &encrSettings);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Processing M7 failed %d\n",
				msgType));
			/* Send a NACK to the Enrollee */
			if (err == RPROT_ERR_CRYPTO) {
				retVal = state_machine_send_nack(r->reg_info,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg, NULL);

				goto pinfail;
			}

			retVal = state_machine_send_nack(r->reg_info, WPS_ERROR_DEVICE_BUSY,
				outmsg, NULL);
			goto out;
		}
		wps_setProcessStates(WPS_SENDM7);
		r->reg_info->e_lastMsgRecd = M7;
		r->mp_peerEncrSettings = encrSettings;

		/* Build message 8 with the appropriate encrypted settings */
		if (r->reg_info->enrollee->b_ap &&
		    r->reg_info->dev_info->configap == 0) {
			RegData *regInfo = r->reg_info;

			retVal = state_machine_send_nack(r->reg_info,
				WPS_ERROR_NO_ERROR, outmsg, NULL);

			if (regInfo->registrar->version2 >= WPS_VERSION2) {
				/* WSC 2.0, test case 4.1.10 */
				err = reg_proto_m7ap_EncrSettingsCheck(encrSettings);
				if (err != WPS_SUCCESS) {
					/* Record the real error code for further use */
					r->err_code = err;
					r->reg_info->e_smState = FAILURE;
					goto out;
				}
			}

			/* set the message state to success */
			r->reg_info->e_smState = SUCCESS;

			goto out;
		}

		err = reg_proto_create_m8(r->reg_info, outmsg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: build M8 failed %d\n",
				msgType));
			/* Send a NACK to the Enrollee */
			retVal = state_machine_send_nack(r->reg_info,
				WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			goto out;
		}
		r->reg_info->e_lastMsgSent = M8;

		/* set the message state to continue */
		r->reg_info->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M8:
		if (WPS_ID_MESSAGE_DONE != msgType) {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected DONE, received %d\n",
				msgType));
			/*
			 * WPS spec 1.0h, in 6.10.4 external registrar should send
			 * NACK after receive NACK from AP (802.1X authenticator).
			 */
			if (!regInfo->registrar->b_ap && msgType == WPS_ID_MESSAGE_NACK) {
				uint16 configError = 0xffff;
				err = reg_proto_process_nack(r->reg_info, msg,
					&configError);
				if (configError != WPS_ERROR_NO_ERROR) {
					TUTRACE((TUTRACE_ERR, "REGSM: Recvd NACK"
						" with config err: %d", configError));
				}
				retVal = state_machine_send_nack(r->reg_info, configError,
					outmsg, NULL);
			}

			r->reg_info->e_smState = FAILURE;

			goto out;
		}

		err = reg_proto_process_done(r->reg_info, msg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Processing DONE failed %d\n",
				msgType));
			goto out;
		}

		/* Indicate Done received */
		wps_setProcessStates(WPS_MSGDONE);

		if (r->reg_info->enrollee->b_ap &&
			(r->reg_info->transportType == TRANSPORT_TYPE_EAP)) {
			retVal = state_machine_send_ack(r->reg_info, outmsg, NULL);
		}

		/* set the message state to success */
		r->reg_info->e_smState = SUCCESS;
		break;

	default:
		if (WPS_ID_MESSAGE_NACK == msgType) {
			uint16 configError;
			err = reg_proto_process_nack(r->reg_info, msg, &configError);
			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "REGSM: ProcessMessageNack err %d\n",
					err));
				goto out;
			}
			TUTRACE((TUTRACE_ERR, "REGSM: Recvd NACK with err code: %d",
				configError));
			r->reg_info->e_smState = FAILURE;
		}
		else {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected M1, received %d\n",
				msgType));
			goto out;
		}
	}

out:
	return retVal;

pinfail:
	/* Recoder Pin error information */
	wps_setPinFailInfo(regInfo->enrollee->macAddr, regInfo->enrollee->deviceName,
		r->reg_info->e_lastMsgSent == M4 ? "M4" : "M6");

	return WPS_ERR_REGISTRATION_PINFAIL;
}

/* WSC 2.0 */
static uint32
reg_proto_m7ap_EncrSettingsCheck(void *encrSettings)
{
	EsM7Ap *esAP = (EsM7Ap *)encrSettings;

	if (!esAP)
		return WPS_ERR_INVALID_PARAMETERS;

	if (esAP->encrType.m_data == WPS_ENCRTYPE_WEP) {
		TUTRACE((TUTRACE_ERR, "Deprecated WEP encryption detected\n"));
		return RPROT_ERR_INCOMPATIBLE_WEP;
	}

	return WPS_SUCCESS;
}

static uint32
reg_proto_process_m1(RegData *regInfo, BufferObj *msg)
{
	WpsM1 m;
	int ret;
	uint32 tlv_err = 0;
	bool b_oob_devpw = false;
	DevInfo *enrollee;
	CTlvPrimDeviceType *prim;
#ifdef WPS_NFC_DEVICE
	uint8 DHKey[SIZE_256_BITS];
#endif

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM1: %d byte message\n",
		msg->m_dataLength));

	/* Allocate enrollee context */
	if (!regInfo->enrollee) {
		regInfo->enrollee = (DevInfo *)alloc_init(sizeof(DevInfo));
		if (!regInfo->enrollee)
			return WPS_ERR_OUTOFMEMORY;
	}

	enrollee = regInfo->enrollee;

	/* Init M1 */
	reg_msg_init(&m, WPS_ID_MESSAGE_M1);

	/* Check the version */
	if ((ret = reg_msg_version_check(WPS_ID_MESSAGE_M1, msg,
		&m.version, &m.msgType)) != WPS_SUCCESS)
		return ret;

	/* Add in PF#3, we should not trust what we receive. (Ralink issue) */
	tlv_err |= tlv_dserialize(&m.uuid, WPS_ID_UUID_E, msg, SIZE_UUID, 0);
	tlv_err |= tlv_dserialize(&m.macAddr, WPS_ID_MAC_ADDR, msg, SIZE_MAC_ADDR, 0);
	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);
	tlv_err |= tlv_dserialize(&m.publicKey, WPS_ID_PUBLIC_KEY, msg, SIZE_PUB_KEY, 0);
	tlv_err |= tlv_dserialize(&m.authTypeFlags, WPS_ID_AUTH_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.encrTypeFlags, WPS_ID_ENCR_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.connTypeFlags, WPS_ID_CONN_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.configMethods, WPS_ID_CONFIG_METHODS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.scState, WPS_ID_SC_STATE, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.manufacturer, WPS_ID_MANUFACTURER, msg, SIZE_64_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.modelName, WPS_ID_MODEL_NAME, msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.modelNumber, WPS_ID_MODEL_NUMBER, msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.serialNumber, WPS_ID_SERIAL_NUM, msg, SIZE_32_BYTES, 0);

	tlv_err |= tlv_primDeviceTypeParse(&m.primDeviceType, msg);
	tlv_err |= tlv_dserialize(&m.deviceName, WPS_ID_DEVICE_NAME, msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.rfBand, WPS_ID_RF_BAND, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.assocState, WPS_ID_ASSOC_STATE, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.devPwdId, WPS_ID_DEVICE_PWD_ID, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.configError, WPS_ID_CONFIG_ERROR, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.osVersion, WPS_ID_OS_VERSION, msg, 0, 0);
	if (tlv_err) {
		TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM1: Bad attributes\n"));
		return RPROT_ERR_INVALID_VALUE;
	}

	/* WPS M1 Pairing Auth */
	if (regInfo->registrar->pairing_auth_str &&
	    _reg_sm_m1_pairing_auth(regInfo->registrar->pairing_auth_str, &m.deviceName)) {
		TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM1: M1 Pairing Auth failed.\n"));
		return RPROT_ERR_ROGUE_SUSPECTED;
	}

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->registrar->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			BufferObj *vendorExt_bufObj = buffobj_new();

			/* Deserialize subelement */
			if (!vendorExt_bufObj) {
				TUTRACE((TUTRACE_ERR, "Fail to allocate vendor extension buffer,"
					"Out of memory\n"));
				return WPS_ERR_OUTOFMEMORY;
			}

			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* WSC 2.0, is next Version 2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);

			if (WPS_WFA_SUBID_REQ_TO_ENROLL == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.reqToEnr, WPS_WFA_SUBID_REQ_TO_ENROLL,
					vendorExt_bufObj, 0, 0);

			buffobj_del(vendorExt_bufObj);
		}
	}

	/* Save Enrollee's Informations */
	memcpy(enrollee->uuid, m.uuid.m_data, m.uuid.tlvbase.m_len);
	memcpy(enrollee->macAddr, m.macAddr.m_data, m.macAddr.tlvbase.m_len);
	enrollee->version2 = m.version2.m_data;

	memcpy(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len);

	/* Extract the peer's public key. */
	/* First store the raw public key (to be used for e/rhash computation) */
	memcpy(regInfo->pke, m.publicKey.m_data, SIZE_PUB_KEY);

#ifdef WPS_NFC_DEVICE
	if (regInfo->registrar->devPwdId & 0xFFF0) {
		/* Device Password ID check */
		if (regInfo->registrar->devPwdId != m.devPwdId.m_data) {
			TUTRACE((TUTRACE_ERR, "Device Password ID not matched, R:0x%x E:0x%x\n",
				regInfo->registrar->devPwdId, m.devPwdId.m_data));
			return RPROT_ERR_DEV_PW_ID_MISMATCH;
		}

		/* Public Key Hash check */
		if (SHA256(regInfo->pke, SIZE_PUB_KEY, DHKey) == NULL) {
			TUTRACE((TUTRACE_ERR, "Public key hash: SHA256 calculation failed\n"));
			return RPROT_ERR_CRYPTO;
		}

		if (memcmp(regInfo->dev_info->pub_key_hash, DHKey, SIZE_20_BYTES) != 0) {
			TUTRACE((TUTRACE_ERR, "Public key hash doesn't matched\n"));
			return RPROT_ERR_PUB_KEY_HASH_MISMATCH;
		}

		b_oob_devpw = true;
	}
#endif /* WPS_NFC_DEVICE */

	enrollee->authTypeFlags = m.authTypeFlags.m_data;
	enrollee->encrTypeFlags = m.encrTypeFlags.m_data;
	enrollee->connTypeFlags = m.connTypeFlags.m_data;
	enrollee->configMethods = m.configMethods.m_data;
	enrollee->scState = m.scState.m_data;
	tlv_strncpy(&m.manufacturer, enrollee->manufacturer, sizeof(enrollee->manufacturer));
	tlv_strncpy(&m.modelName, enrollee->modelName, sizeof(enrollee->modelName));
	tlv_strncpy(&m.modelNumber, enrollee->modelNumber, sizeof(enrollee->modelNumber));
	tlv_strncpy(&m.serialNumber, enrollee->serialNumber, sizeof(enrollee->serialNumber));

	/* Only take a CAT_NW_INFRA.SUB_CAT_NW_AP as an AP */
	enrollee->b_ap = false;	/* Assume to be just a external registrar */

	prim = &m.primDeviceType;
	if (prim->categoryId == WPS_DEVICE_TYPE_CAT_NW_INFRA &&
	    prim->subCategoryId == WPS_DEVICE_TYPE_SUB_CAT_NW_AP) {
		enrollee->b_ap = true;
	}

	enrollee->primDeviceCategory = prim->categoryId;
	enrollee->primDeviceOui = prim->oui;
	enrollee->primDeviceSubCategory = prim->subCategoryId;

	/* Next is to set Device Name */
	tlv_strncpy(&m.deviceName, enrollee->deviceName, sizeof(enrollee->deviceName));

	wps_setStaDevName((unsigned char*)enrollee->deviceName);
	TUTRACE((TUTRACE_INFO, "The device name is %s\n", enrollee->deviceName));

	/* Next is RF Band */
	enrollee->rfBand = m.rfBand.m_data;
	enrollee->assocState = m.assocState.m_data;

	/* Check Acceptable Device Password ID */
	switch (m.devPwdId.m_data) {
	case WPS_DEVICEPWDID_DEFAULT:
	case WPS_DEVICEPWDID_USER_SPEC:
	case WPS_DEVICEPWDID_PUSH_BTN:
	case WPS_DEVICEPWDID_REG_SPEC:
		break;
	case WPS_DEVICEPWDID_MACHINE_SPEC:
	case WPS_DEVICEPWDID_REKEY:
	default:
		if (!b_oob_devpw) {
			TUTRACE((TUTRACE_ERR, "Device Password ID not support\n"));
			return WPS_ERR_NOT_IMPLEMENTED;
		}
	}

	enrollee->devPwdId = m.devPwdId.m_data;

	/* Assign the rest to enrolle context */
	enrollee->configError = m.configError.m_data;
	enrollee->osVersion = m.osVersion.m_data;

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, buffobj_Length(msg), buffobj_GetBuf(msg));
	return WPS_SUCCESS;
}

static uint32
reg_proto_create_m2(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint32 err;
	uint8 secret[SIZE_PUB_KEY];
	int secretLen;
	uint8 DHKey[SIZE_256_BITS];
	uint8 kdk[SIZE_256_BITS];
	uint8 hmac[SIZE_256_BITS];
	BufferObj *esBuf;
	BufferObj *cipherText;
	BufferObj *iv;
	BufferObj *hmacData;
	CTlvPrimDeviceType primDev;
	uint8 reg_proto_version = WPS_VERSION;
	DevInfo *registrar = regInfo->registrar;
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	bool b_zpadding = registrar->b_zpadding;
#endif /* WFA_WPS_20_TESTBED */

	BufferObj *buf1, *buf2, *buf3, *buf1_dser, *buf2_dser;


	buf1 = buffobj_new();
	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	buf2 = buffobj_new();
	if (buf2 == NULL) {
		buffobj_del(buf1);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf3 = buffobj_new();
	if (buf3 == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf1_dser = buffobj_new();
	if (buf1_dser == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		buffobj_del(buf3);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf2_dser = buffobj_new();
	if (buf2_dser == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		buffobj_del(buf3);
		buffobj_del(buf1_dser);
		return WPS_ERR_OUTOFMEMORY;
	}

	/* First, generate or gather the required data */
	message = WPS_ID_MESSAGE_M2;

	/* Registrar nonce */
	RAND_bytes(regInfo->registrarNonce, SIZE_128_BITS);

	if (!registrar->DHSecret) {
		err = reg_proto_generate_dhkeypair(&registrar->DHSecret);
		if (err != WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "RPROTO: Cannot create DH Secret\n"));
			goto error;
		}
	}

	/* extract the DH public key */
	if (reg_proto_BN_bn2bin(registrar->DHSecret->pub_key, regInfo->pkr) != SIZE_PUB_KEY) {
		err =  RPROT_ERR_CRYPTO;
		goto error;
	}

	/* KDK generation */
	regInfo->DH_PubKey_Peer = BN_new();
	if (!regInfo->DH_PubKey_Peer) {
		TUTRACE((TUTRACE_ERR, "Create M2: Malloc failed for peer DH public key\n"));
		err = WPS_ERR_OUTOFMEMORY;
		goto error;
	}

	/* Finally, import the raw key into the bignum datastructure */
	if (BN_bin2bn(regInfo->pke, SIZE_PUB_KEY, regInfo->DH_PubKey_Peer) == NULL) {
		BN_free(regInfo->DH_PubKey_Peer);
		TUTRACE((TUTRACE_ERR, "Create M2: import raw key data to BN failed\n"));
		err = RPROT_ERR_CRYPTO;
		goto error;
	}

	/* 1. generate the DH shared secret */
	secretLen = DH_compute_key_bn(secret, regInfo->DH_PubKey_Peer, registrar->DHSecret);
	if (secretLen == -1) {
#ifdef EXTERNAL_OPENSSL
		/* bcmcrypto DH_compute_key_bn will free peer_key when error! */
		BN_free(regInfo->DH_PubKey_Peer);
#endif
		err = RPROT_ERR_CRYPTO;
		goto error;
	} else if (secretLen < SIZE_PUB_KEY) {
		/*
		* zero padding the DH shared key from beginning to 192 octests in order
		* to be compatible with WPA_SUPPLICANT
		*/
		int i, j = SIZE_PUB_KEY - secretLen;

		for (i = secretLen - 1; i >= 0; i--) {
			secret[i + j] = secret[i];
		}
		memset(secret, 0, j);
		secretLen = SIZE_PUB_KEY;
	}

	/* 2. compute the DHKey based on the DH secret */
	if (SHA256(secret, secretLen, DHKey) == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: SHA256 calculation failed\n"));
		err = RPROT_ERR_CRYPTO;
		goto error;
	}

	/* 3.Append the enrollee nonce(N1), enrollee mac and registrar nonce(N2) */
	buffobj_Append(buf2, SIZE_128_BITS, regInfo->enrolleeNonce);
	buffobj_Append(buf2, SIZE_MAC_ADDR, regInfo->enrollee->macAddr);
	buffobj_Append(buf2, SIZE_128_BITS, regInfo->registrarNonce);

	/* 4. now generate the KDK */
	hmac_sha256(DHKey, SIZE_256_BITS, buf2->pBase, buf2->m_dataLength, kdk, NULL);

	/* KDK generation */

	/* Derivation of AuthKey, KeyWrapKey and EMSK */
	/* 1. declare and initialize the appropriate buffer objects */
	buffobj_dserial(buf1_dser, kdk, SIZE_256_BITS);
	buffobj_dserial(buf2_dser, (uint8 *)PERSONALIZATION_STRING,
		(uint32)strlen(PERSONALIZATION_STRING));

	/* 2. call the key derivation function */
	reg_proto_derivekey(buf1_dser, buf2_dser, KDF_KEY_BITS, buf1);

	/* 3. split the key into the component keys and store them */
	buffobj_RewindLength(buf1, buf1->m_dataLength);
	buffobj_Append(regInfo->authKey, SIZE_256_BITS, buf1->pCurrent);
	buffobj_Advance(buf1, SIZE_256_BITS);

	buffobj_Append(regInfo->keyWrapKey, SIZE_128_BITS, buf1->pCurrent);
	buffobj_Advance(buf1, SIZE_128_BITS);

	buffobj_Append(regInfo->emsk, SIZE_256_BITS, buf1->pCurrent);

	/* Derivation of AuthKey, KeyWrapKey and EMSK */

	/* Encrypted settings */
	/* encrypted settings. */
	esBuf = buf1;
	cipherText = buf2;
	iv = buf3;

	buffobj_Reset(esBuf);
	buffobj_Reset(cipherText);
	buffobj_Reset(iv);

	/* Encrypted settings */

	/* start assembling the message */
	tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	TUTRACE((TUTRACE_INFO, "RPROTO: In BuildMessageM2:  after encrypt 2\n"));
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_UUID_R, msg, registrar->uuid, SIZE_UUID);
	tlv_serialize(WPS_ID_PUBLIC_KEY, msg, regInfo->pkr, SIZE_PUB_KEY);
	tlv_serialize(WPS_ID_AUTH_TYPE_FLAGS, msg, &registrar->authTypeFlags,
		WPS_ID_AUTH_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_ENCR_TYPE_FLAGS, msg, &registrar->encrTypeFlags,
		WPS_ID_ENCR_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONN_TYPE_FLAGS, msg, &registrar->connTypeFlags,
		WPS_ID_CONN_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONFIG_METHODS, msg, &registrar->configMethods,
		WPS_ID_CONFIG_METHODS_S);
	tlv_serialize(WPS_ID_MANUFACTURER, msg, registrar->manufacturer,
		STR_ATTR_LEN(registrar->manufacturer, SIZE_64_BYTES));
	tlv_serialize(WPS_ID_MODEL_NAME, msg, registrar->modelName,
		STR_ATTR_LEN(registrar->modelName, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_MODEL_NUMBER, msg, registrar->modelNumber,
		STR_ATTR_LEN(registrar->modelNumber, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_SERIAL_NUM, msg, registrar->serialNumber,
		STR_ATTR_LEN(registrar->serialNumber, SIZE_32_BYTES));
	primDev.categoryId = registrar->primDeviceCategory;
	primDev.oui = registrar->primDeviceOui;
	primDev.subCategoryId = registrar->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&primDev, msg);

	tlv_serialize(WPS_ID_DEVICE_NAME, msg, registrar->deviceName,
		STR_ATTR_LEN(registrar->deviceName, SIZE_32_BYTES));

	/* set real RF band */
	tlv_serialize(WPS_ID_RF_BAND, msg, &registrar->rfBand,
		WPS_ID_RF_BAND_S);
	tlv_serialize(WPS_ID_ASSOC_STATE, msg, &registrar->assocState,
		WPS_ID_ASSOC_STATE_S);
	tlv_serialize(WPS_ID_CONFIG_ERROR, msg, &registrar->configError,
		WPS_ID_CONFIG_ERROR_S);
	tlv_serialize(WPS_ID_DEVICE_PWD_ID, msg, &registrar->devPwdId,
		WPS_ID_DEVICE_PWD_ID_S);
	tlv_serialize(WPS_ID_OS_VERSION, msg, &registrar->osVersion,
		WPS_ID_OS_VERSION_S);
	/* Skip optional attributes */
	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (registrar->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		/* Reuse buf1 */
		BufferObj *vendorExt_bufObj = buf1;

		buffobj_Reset(vendorExt_bufObj);

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&registrar->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (registrar->nattr_len)
			buffobj_Append(msg, registrar->nattr_len,
				(uint8*)registrar->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Add support for Windows Rally Vertical Pairing */
	reg_proto_vendor_ext_vp(registrar, msg);

	/* Now calculate the hmac */
	hmacData = buf1;
	buffobj_Reset(hmacData);
	buffobj_Append(hmacData, buffobj_Length(regInfo->inMsg), buffobj_GetBuf(regInfo->inMsg));
	buffobj_Append(hmacData, buffobj_Length(msg), buffobj_GetBuf(msg));

	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		buffobj_GetBuf(hmacData), buffobj_Length(hmacData), hmac, NULL);

	tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);

	/* Store the outgoing message  */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, buffobj_Length(msg), buffobj_GetBuf(msg));

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM2 built: %d bytes\n",
		buffobj_Length(msg)));
	err =  WPS_SUCCESS;

error:
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

/* M2D generation */
static uint32
reg_proto_create_m2d(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint8 reg_proto_version = WPS_VERSION;
	CTlvPrimDeviceType primDev;
	DevInfo *registrar = regInfo->registrar;
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	bool b_zpadding = registrar->b_zpadding;
#endif /* WFA_WPS_20_TESTBED */

	/* First, generate or gather the required data */
	message = WPS_ID_MESSAGE_M2D;

	/* Registrar nonce */
	RAND_bytes(regInfo->registrarNonce, SIZE_128_BITS);

	/* start assembling the message */

	tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_UUID_R, msg, registrar->uuid, SIZE_UUID);
	tlv_serialize(WPS_ID_AUTH_TYPE_FLAGS, msg, &registrar->authTypeFlags,
		WPS_ID_AUTH_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_ENCR_TYPE_FLAGS, msg, &registrar->encrTypeFlags,
		WPS_ID_ENCR_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONN_TYPE_FLAGS, msg, &registrar->connTypeFlags,
		WPS_ID_CONN_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONFIG_METHODS, msg, &registrar->configMethods,
		WPS_ID_CONFIG_METHODS_S);
	tlv_serialize(WPS_ID_MANUFACTURER, msg, registrar->manufacturer,
		STR_ATTR_LEN(registrar->manufacturer, SIZE_64_BYTES));
	tlv_serialize(WPS_ID_MODEL_NAME, msg, registrar->modelName,
		STR_ATTR_LEN(registrar->modelName, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_MODEL_NUMBER, msg, registrar->modelNumber,
		STR_ATTR_LEN(registrar->modelNumber, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_SERIAL_NUM, msg, registrar->serialNumber,
		STR_ATTR_LEN(registrar->serialNumber, SIZE_32_BYTES));

	primDev.categoryId = registrar->primDeviceCategory;
	primDev.oui = registrar->primDeviceOui;
	primDev.subCategoryId = registrar->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&primDev, msg);

	tlv_serialize(WPS_ID_DEVICE_NAME, msg, registrar->deviceName,
		STR_ATTR_LEN(registrar->deviceName, SIZE_32_BYTES));

	/* set real RF band */
	tlv_serialize(WPS_ID_RF_BAND, msg, &registrar->rfBand, WPS_ID_RF_BAND_S);
	tlv_serialize(WPS_ID_ASSOC_STATE, msg, &registrar->assocState,
		WPS_ID_ASSOC_STATE_S);
	tlv_serialize(WPS_ID_CONFIG_ERROR, msg, &registrar->configError,
		WPS_ID_CONFIG_ERROR_S);

	tlv_serialize(WPS_ID_OS_VERSION, msg, &registrar->osVersion,
		WPS_ID_OS_VERSION_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (registrar->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&registrar->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (registrar->nattr_len)
			buffobj_Append(msg, registrar->nattr_len,
				(uint8*)registrar->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Store the outgoing message */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM2D built: %d bytes\n",
		msg->m_dataLength));

	return WPS_SUCCESS;
}

static uint32
reg_proto_process_m3(RegData *regInfo, BufferObj *msg)
{
	WpsM3 m;
	uint32 err;
	BufferObj *hmacData;
	BufferObj *buf1 = buffobj_new();


	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM3: %d byte message\n",
	         msg->m_dataLength));

	/*
	* First and foremost, check the version and message number.
	* Don't deserialize incompatible messages!
	*/
	reg_msg_init(&m, WPS_ID_MESSAGE_M3);

	if ((err = reg_msg_version_check(WPS_ID_MESSAGE_M3, msg,
		&m.version, &m.msgType)) != WPS_SUCCESS)
		goto error;

	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE,
		msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.eHash1, WPS_ID_E_HASH1, msg, SIZE_256_BITS, 0);
	tlv_dserialize(&m.eHash2, WPS_ID_E_HASH2, msg, SIZE_256_BITS, 0);

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->registrar->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			buffobj_Reset(vendorExt_bufObj);

			/* Deserialize subelement */
			if (!vendorExt_bufObj) {
				TUTRACE((TUTRACE_ERR, "Fail to allocate vendor extension buffer,"
					"Out of memory\n"));
				return WPS_ERR_OUTOFMEMORY;
			}

			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* WSC 2.0, is next Version 2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/* skip other optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs(msg->pCurrent+sizeof(uint16)));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR, msg, SIZE_64_BITS, 0);

	/* Now start validating the message */

	/* confirm the registrar nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		err =  RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/* HMAC validation */
	hmacData = buf1;
	buffobj_Reset(hmacData);
	/* append the last message sent */
	buffobj_Append(hmacData, regInfo->outMsg->m_dataLength, regInfo->outMsg->pBase);
	/* append the current message. Don't append the last TLV (auth) */
	buffobj_Append(hmacData,
		msg->m_dataLength-(sizeof(WpsTlvHdr)+m.authenticator.tlvbase.m_len),
		msg->pBase);
	if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed"));
		err =  RPROT_ERR_CRYPTO;
		goto error;
	}

	/* HMAC validation */

	/* Now copy the relevant data */
	memcpy(regInfo->eHash1, m.eHash1.m_data, SIZE_256_BITS);
	memcpy(regInfo->eHash2, m.eHash2.m_data, SIZE_256_BITS);

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);

	err = WPS_SUCCESS;

error:
	buffobj_del(buf1);
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM3 : got error %x", err));
	return err;
}

static uint32
reg_proto_create_m4(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint8 hashBuf[SIZE_256_BITS];
	uint32 err;
	uint8 *pwdPtr;
	int pwdLen;

	BufferObj *buf1, *buf2, *buf3;


	buf1 = buffobj_new();
	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	buf2 = buffobj_new();
	if (buf2 == NULL) {
		buffobj_del(buf1);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf3 = buffobj_new();
	if (buf3 == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		return WPS_ERR_OUTOFMEMORY;
	}

	/* First, generate or gather the required data */
	message = WPS_ID_MESSAGE_M4;

	/* PSK1 and PSK2 generation */
	pwdPtr = (uint8 *)regInfo->dev_info->pin;
	pwdLen = strlen(regInfo->dev_info->pin);

	/*
	 * Hash 1st half of passwd. If it is an odd length, the extra byte
	 * goes along with the first half
	 */
	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		pwdPtr, (pwdLen/2)+(pwdLen%2), hashBuf, NULL);

	/* copy first 128 bits into psk1; */
	memcpy(regInfo->psk1, hashBuf, SIZE_128_BITS);

	/* Hash 2nd half of passwd */
	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		pwdPtr+(pwdLen/2)+(pwdLen%2), pwdLen/2, hashBuf, NULL);

	/* copy first 128 bits into psk2; */
	memcpy(regInfo->psk2, hashBuf, SIZE_128_BITS);
	/* PSK1 and PSK2 generation */

	/* RHash1 and RHash2 generation */
	RAND_bytes(regInfo->rs1, SIZE_128_BITS);
	RAND_bytes(regInfo->rs2, SIZE_128_BITS);
	{
		BufferObj *rhashBuf = buf1;
		buffobj_Reset(rhashBuf);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->rs1);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->psk1);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pkr);
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			rhashBuf->pBase, rhashBuf->m_dataLength, hashBuf, NULL);
		memcpy(regInfo->rHash1, hashBuf, SIZE_256_BITS);

		buffobj_Reset(rhashBuf);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->rs2);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->psk2);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pkr);
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		            rhashBuf->pBase, rhashBuf->m_dataLength, hashBuf, NULL);
	}
	memcpy(regInfo->rHash2, hashBuf, SIZE_256_BITS);

	/* RHash1 and RHash2 generation */
	{
		/* encrypted settings. */
		BufferObj *encData = buf1;
		BufferObj *cipherText = buf2;
		BufferObj *iv = buf3;
		uint8 reg_proto_version = WPS_VERSION;
		TlvEsNonce rsNonce;
		CTlvEncrSettings encrSettings_tlv;

		buffobj_Reset(encData);
		buffobj_Reset(cipherText);
		buffobj_Reset(iv);

		tlv_set(&rsNonce.nonce, WPS_ID_R_SNONCE1, regInfo->rs1, SIZE_128_BITS);
		reg_msg_nonce_write(&rsNonce, encData, regInfo->authKey);

		reg_proto_encrypt_data(encData, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);

		/* Now assemble the message */
		tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
		tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
		tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce,
			SIZE_128_BITS);
		tlv_serialize(WPS_ID_R_HASH1, msg, regInfo->rHash1, SIZE_256_BITS);
		tlv_serialize(WPS_ID_R_HASH2, msg, regInfo->rHash2, SIZE_256_BITS);

		encrSettings_tlv.iv = iv->pBase;
		encrSettings_tlv.ip_encryptedData = cipherText->pBase;
		encrSettings_tlv.encrDataLength = cipherText->m_dataLength;
		tlv_encrSettingsWrite(&encrSettings_tlv, msg);
	}

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->registrar->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		/* Reuse buf1 */
		BufferObj *vendorExt_bufObj = buf1;

		buffobj_Reset(vendorExt_bufObj);

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->registrar->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->registrar->nattr_len)
			buffobj_Append(msg, regInfo->registrar->nattr_len,
				(uint8*)regInfo->registrar->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Calculate the hmac */
	{
		BufferObj *hmacData = buf1;
		uint8 hmac[SIZE_256_BITS];

		buffobj_Reset(hmacData);
		buffobj_Append(hmacData, regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
		buffobj_Append(hmacData, msg->m_dataLength, msg->pBase);

		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			hmacData->pBase, hmacData->m_dataLength, hmac, NULL);
		tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);
	}
	/* Store the outgoing message */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);

	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM4 built: %d bytes\n",
		msg->m_dataLength));

	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM4 : got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);

	return err;
}

static uint32
reg_proto_process_m5(RegData *regInfo, BufferObj *msg)
{
	WpsM5 m;
	uint32 err;

	BufferObj *buf1, *buf2, *buf3, *buf1_dser, *buf2_dser;


	buf1 = buffobj_new();
	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	buf2 = buffobj_new();
	if (buf2 == NULL) {
		buffobj_del(buf1);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf3 = buffobj_new();
	if (buf3 == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf1_dser = buffobj_new();
	if (buf1_dser == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		buffobj_del(buf3);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf2_dser = buffobj_new();
	if (buf2_dser == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		buffobj_del(buf3);
		buffobj_del(buf1_dser);
		return WPS_ERR_OUTOFMEMORY;
	}

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM5: %d byte message\n",
		msg->m_dataLength));

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	reg_msg_init(&m, WPS_ID_MESSAGE_M5);

	if ((err = reg_msg_version_check(WPS_ID_MESSAGE_M5, msg,
		&m.version, &m.msgType)) != WPS_SUCCESS)
		goto error;

	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);
	tlv_encrSettingsParse(&m.encrSettings, msg);

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->registrar->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			buffobj_Reset(vendorExt_bufObj);

			/* Deserialize subelement */
			if (!vendorExt_bufObj) {
				TUTRACE((TUTRACE_ERR, "Fail to allocate vendor extension buffer,"
					"Out of memory\n"));
				return WPS_ERR_OUTOFMEMORY;
			}

			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* WSC 2.0, is next Version 2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/* skip other optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs(msg->pCurrent+sizeof(uint16)));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL)
			err =  RPROT_ERR_REQD_TLV_MISSING;
	}

	tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR, msg, SIZE_64_BITS, 0);

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		err =  RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/* HMAC validation */
	{
		BufferObj *hmacData = buf1;
		buffobj_Reset(hmacData);
		/* append the last message sent */
		buffobj_Append(hmacData, regInfo->outMsg->m_dataLength, regInfo->outMsg->pBase);
		/* append the current message. Don't append the last TLV (auth) */
		buffobj_Append(hmacData,
			msg->m_dataLength-(sizeof(WpsTlvHdr)+m.authenticator.tlvbase.m_len),
			msg->pBase);

		if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed"));
			err =  RPROT_ERR_CRYPTO;
			goto error;
		}
	}

	/* HMAC validation */

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = buf2_dser;
		BufferObj *plainText = buf1;
		TlvEsNonce eNonce;

		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);

		reg_msg_nonce_parse(&eNonce, WPS_ID_E_SNONCE1, plainText, regInfo->authKey);

	/* extract encrypted settings */

	/* EHash1 validation */
	/* 1. Save ES1 */
		memcpy(regInfo->es1, eNonce.nonce.m_data, eNonce.nonce.tlvbase.m_len);
	}

	{
		/* 2. prepare the buffer */
		BufferObj *ehashBuf = buf1;
		uint8 hashBuf[SIZE_256_BITS];

		buffobj_Reset(ehashBuf);
		buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->es1);
		buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->psk1);
		buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pkr);

		/* 3. generate the mac */
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			ehashBuf->pBase, ehashBuf->m_dataLength, hashBuf, NULL);

		/* 4. compare the mac to ehash1 */
		if (memcmp(regInfo->eHash1, hashBuf, SIZE_256_BITS)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: ES1 hash doesn't match EHash1\n"));
			err =  RPROT_ERR_CRYPTO;
			goto error;
		}

		/* 5. Instead of steps 3 & 4, we could have called ValidateMac */
	}

	/* EHash1 validation */

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM5 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM5: got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

static uint32
reg_proto_create_m6(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint32 err;

	BufferObj *buf1, *buf2, *buf3;


	buf1 = buffobj_new();
	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	buf2 = buffobj_new();
	if (buf2 == NULL) {
		buffobj_del(buf1);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf3 = buffobj_new();
	if (buf3 == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		return WPS_ERR_OUTOFMEMORY;
	}

	/* First, generate or gather the required data */
	message = WPS_ID_MESSAGE_M6;

	/* encrypted settings. */
	{
		BufferObj *encData = buf1;
		BufferObj *cipherText = buf2;
		BufferObj *iv = buf3;
		uint8 reg_proto_version = WPS_VERSION;
		TlvEsNonce rsNonce;
		CTlvEncrSettings encrSettings_tlv;

		buffobj_Reset(encData);
		buffobj_Reset(cipherText);
		buffobj_Reset(iv);

		tlv_set(&rsNonce.nonce, WPS_ID_R_SNONCE2, regInfo->rs2, SIZE_128_BITS);
		reg_msg_nonce_write(&rsNonce, encData, regInfo->authKey);

		reg_proto_encrypt_data(encData, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);

		/* Now assemble the message */
		tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
		tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
		tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);

		encrSettings_tlv.iv = iv->pBase;
		encrSettings_tlv.ip_encryptedData = cipherText->pBase;
		encrSettings_tlv.encrDataLength = cipherText->m_dataLength;
		tlv_encrSettingsWrite(&encrSettings_tlv, msg);
	}

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->registrar->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		/* Reuse buf1 */
		BufferObj *vendorExt_bufObj = buf1;

		buffobj_Reset(vendorExt_bufObj);

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->registrar->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->registrar->nattr_len)
			buffobj_Append(msg, regInfo->registrar->nattr_len,
				(uint8*)regInfo->registrar->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Calculate the hmac */
	{
		BufferObj *hmacData = buf1;
		uint8 hmac[SIZE_256_BITS];

		buffobj_Reset(hmacData);
		buffobj_Append(hmacData, regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
		buffobj_Append(hmacData, msg->m_dataLength, msg->pBase);

		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			hmacData->pBase, hmacData->m_dataLength, hmac, NULL);
		tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);
	}

	/* Store the outgoing message */

	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM6 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;

	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM6 : got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);

	return err;
}

/*
 * Vic: make sure we don't override the settings of an already-configured AP.
 * This scenario is not yet supported, but if this code is modified to support
 * setting up an already-configured AP, the settings from M7 will need to be
 * carried across to the encrypted settings of M8.
 */
static uint32
reg_proto_process_m7(RegData *regInfo, BufferObj *msg, void **encrSettings)
{
	WpsM7 m;
	uint32 err;

	BufferObj *buf1, *buf1_dser, *buf2_dser;


	buf1 = buffobj_new();
	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	buf1_dser = buffobj_new();
	if (buf1_dser == NULL) {
		buffobj_del(buf1);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf2_dser = buffobj_new();
	if (buf2_dser == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf1_dser);
		return WPS_ERR_OUTOFMEMORY;
	}

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM7: %d byte message\n",
		msg->m_dataLength));

	/* First, deserialize (parse) the message. */

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	reg_msg_init(&m, WPS_ID_MESSAGE_M7);

	if ((err = reg_msg_version_check(WPS_ID_MESSAGE_M7, msg,
		&m.version, &m.msgType)) != WPS_SUCCESS)
		goto error;

	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);

	tlv_encrSettingsParse(&m.encrSettings, msg);

	if (WPS_ID_X509_CERT_REQ == buffobj_NextType(msg))
		tlv_dserialize(&m.x509CertReq, WPS_ID_X509_CERT_REQ, msg, 0, 0);

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->registrar->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			buffobj_Reset(vendorExt_bufObj);

			/* Deserialize subelement */
			if (!vendorExt_bufObj) {
				TUTRACE((TUTRACE_ERR, "Fail to allocate vendor extension buffer,"
					"Out of memory\n"));
				return WPS_ERR_OUTOFMEMORY;
			}

			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* WSC 2.0, is next Settings delay Time */
			if (WPS_WFA_SUBID_SETTINGS_DELAY_TIME ==
				buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.settingsDelayTime,
					WPS_WFA_SUBID_SETTINGS_DELAY_TIME,
					vendorExt_bufObj, 0, 0);

			/* WSC 2.0, is next Version 2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/* skip other optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs(msg->pCurrent+sizeof(uint16)));

		/* If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR, msg, SIZE_64_BITS, 0);

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		err =  RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/* HMAC validation */
	{
		BufferObj *hmacData = buf1;
		buffobj_Reset(hmacData);
		/* append the last message sent */
		buffobj_Append(hmacData, regInfo->outMsg->m_dataLength, regInfo->outMsg->pBase);
		/* append the current message. Don't append the last TLV (auth) */
		buffobj_Append(hmacData,
			msg->m_dataLength-(sizeof(WpsTlvHdr)+m.authenticator.tlvbase.m_len),
			msg->pBase);

		if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed"));
			err = RPROT_ERR_CRYPTO;
		}
	}

	/* HMAC validation */

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = buf2_dser;
		BufferObj *plainText = buf1;
		CTlvNonce eNonce;

		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);

		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);

		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);

		if (regInfo->enrollee->b_ap) {
			CTlvSsid ssid;

			/*
			 * If encrypted settings of M7 does not have SSID attribute,
			 * change enrollee->b_ap to false.
			 */
			if (tlv_find_dserialize(&ssid, WPS_ID_SSID, plainText, SIZE_32_BYTES, 0)
				!= 0)
				regInfo->enrollee->b_ap = false;
			/* rewind plainText buffer OBJ */
			buffobj_Rewind(plainText);
		}

		if (regInfo->enrollee->b_ap) {
			EsM7Ap *esAP = (EsM7Ap *)reg_msg_es_new(ES_TYPE_M7AP);

			if (esAP == NULL)
				goto error;

			err = reg_msg_m7ap_parse(esAP, plainText, regInfo->authKey, true);
			if (err != WPS_SUCCESS) {
				reg_msg_es_del(esAP, 0);
				goto error;
			}

			eNonce = esAP->nonce;
			*encrSettings = (void *)esAP;
		}
		else {
			EsM7Enr *esSTA = (EsM7Enr *)reg_msg_es_new(ES_TYPE_M7ENR);

			if (esSTA == NULL)
				goto error;

			err = reg_msg_m7enr_parse(esSTA, plainText, regInfo->authKey, true);
			if (err != WPS_SUCCESS) {
				reg_msg_es_del(esSTA, 0);
				goto error;
			}

			eNonce = esSTA->nonce;
			*encrSettings = (void *)esSTA;
		}

		/* extract encrypted settings */

		/* EHash2 validation */

		/* 1. Save ES2 */
		memcpy(regInfo->es2, eNonce.m_data, eNonce.tlvbase.m_len);
	}

	{
		BufferObj *ehashBuf = buf1;
		uint8 hashBuf[SIZE_256_BITS];

		/* 2. prepare the buffer */
		buffobj_Reset(ehashBuf);
		buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->es2);
		buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->psk2);
		buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pkr);

		/* 3. generate the mac */
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			ehashBuf->pBase, ehashBuf->m_dataLength, hashBuf, NULL);

		/* 4. compare the mac to ehash2 */
		if (memcmp(regInfo->eHash2, hashBuf, SIZE_256_BITS)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: ES2 hash doesn't match EHash2\n"));
			err =  RPROT_ERR_CRYPTO;
			goto error;
		}

		/* 5. Instead of steps 3 & 4, we could have called ValidateMac */
	}

	/* EHash1 validation */

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM7 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS) {
		*encrSettings = NULL;
		TUTRACE((TUTRACE_ERR, "ProcessMessageM7: got error %x", err));
	}

	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

static uint32
reg_proto_create_m8(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint32 err;

	BufferObj *buf1, *buf2, *buf3;


	buf1 = buffobj_new();
	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	buf2 = buffobj_new();
	if (buf2 == NULL) {
		buffobj_del(buf1);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf3 = buffobj_new();
	if (buf3 == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		return WPS_ERR_OUTOFMEMORY;
	}

	/* First, generate or gather the required data */
	message = WPS_ID_MESSAGE_M8;

	/* encrypted settings. */
	{
		BufferObj *esBuf = buf1;
		BufferObj *cipherText = buf2;
		BufferObj *iv = buf3;
		uint8 reg_proto_version = WPS_VERSION;
		CTlvEncrSettings encrSettings_tlv;

		buffobj_Reset(esBuf);
		buffobj_Reset(cipherText);
		buffobj_Reset(iv);

		if (regInfo->enrollee->b_ap) {
			EsM8Ap *apEs = (EsM8Ap *)regInfo->dev_info->mp_tlvEsM8Ap;

			if (!apEs) {
				TUTRACE((TUTRACE_ERR, "Encrypted settings settings are NULL\n"));
				err =  WPS_ERR_INVALID_PARAMETERS;
				goto error;
			}

			if (regInfo->registrar->version2 >= WPS_VERSION2) {
				/*
				 * WSC 2.0, (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)
				 * allowed only when both the Registrar and the Enrollee
				 * are using WSC 2.0 or newer.
				 * Use WPS_AUTHTYPE_WPA2PSK when enrollee is WSC 1.0 only.
				 */
				if (regInfo->enrollee->version2 < WPS_VERSION2)
					apEs->authType.m_data &= ~WPS_AUTHTYPE_WPAPSK;
			}
			reg_msg_m8ap_write(apEs, esBuf, regInfo->authKey,
				regInfo->registrar->version2 >= WPS_VERSION2 ? TRUE : FALSE);
		}
		else {
			WPS_SSLIST *itr;
			CTlvCredential *pCred;
			EsM8Sta *staEs = (EsM8Sta *)regInfo->dev_info->mp_tlvEsM8Sta;

			if (!staEs) {
				TUTRACE((TUTRACE_ERR, "Encrypted settings settings are NULL\n"));
				err =  WPS_ERR_INVALID_PARAMETERS;
				goto error;
			}

			if (regInfo->registrar->version2 >= WPS_VERSION2) {
				/*
				 * WSC 2.0, (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)
				 * allowed only when both the Registrar and the Enrollee
				 * are using WSC 2.0 or newer.
				 * Use WPS_AUTHTYPE_WPA2PSK when enrollee is WSC 1.0 only.
				 */
				if (regInfo->enrollee->version2 < WPS_VERSION2 &&
				    staEs->credential) {
					itr = staEs->credential;
					for (; (pCred = (CTlvCredential *)itr); itr = itr->next) {
						pCred->authType.m_data &= ~WPS_AUTHTYPE_WPAPSK;
					}
				}
			}
			reg_msg_m8sta_write_key(staEs, esBuf, regInfo->authKey);
		}

		/* Now encrypt the serialize Encrypted settings buffer */
		reg_proto_encrypt_data(esBuf, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);

		/* Now assemble the message */
		tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
		tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
		tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);

		encrSettings_tlv.iv = iv->pBase;
		encrSettings_tlv.ip_encryptedData = cipherText->pBase;
		encrSettings_tlv.encrDataLength = cipherText->m_dataLength;
		tlv_encrSettingsWrite(&encrSettings_tlv, msg);

		if (regInfo->x509Cert->m_dataLength) {
			tlv_serialize(WPS_ID_X509_CERT, msg, regInfo->x509Cert->pBase,
				regInfo->x509Cert->m_dataLength);
		}
	}

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->registrar->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		/* Reuse buf1 */
		BufferObj *vendorExt_bufObj = buf1;

		buffobj_Reset(vendorExt_bufObj);

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->registrar->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->registrar->nattr_len)
			buffobj_Append(msg, regInfo->registrar->nattr_len,
				(uint8*)regInfo->registrar->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Calculate the hmac */
	{
		BufferObj *hmacData = buf1;
		uint8 hmac[SIZE_256_BITS];

		buffobj_Reset(hmacData);
		buffobj_Append(hmacData,  regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
		buffobj_Append(hmacData,  msg->m_dataLength, msg->pBase);

		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			hmacData->pBase, hmacData->m_dataLength, hmac, NULL);

		tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);
	}

	/* Store the outgoing message */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM8 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM8 : got error %x\n", err));

	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);

	return err;
}

static uint32
reg_proto_process_done(RegData *regInfo, BufferObj *msg)
{
	WpsDone m;
	int err;

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageDone: %d byte message\n",
		msg->m_dataLength));

	reg_msg_init(&m, WPS_ID_MESSAGE_DONE);

	if ((err = reg_msg_version_check(WPS_ID_MESSAGE_DONE, msg,
		&m.version, &m.msgType)) != WPS_SUCCESS)
		return err;

	tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);

	/* Registrar process done message */
	if (regInfo->dev_info->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			BufferObj *vendorExt_bufObj = buffobj_new();

			if (vendorExt_bufObj == NULL)
				return WPS_ERR_OUTOFMEMORY;

			/* Deserialize subelement */
			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* Get Version2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
	}

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		return RPROT_ERR_NONCE_MISMATCH;
	}

	/* confirm the registrar nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		return RPROT_ERR_NONCE_MISMATCH;
	}

	return WPS_SUCCESS;
}

uint32
reg_proto_create_devinforsp(RegData *regInfo, BufferObj *msg)
{
	uint32 ret = WPS_SUCCESS;
	uint8 message;
	uint8 reg_proto_version = WPS_VERSION;
	CTlvPrimDeviceType primDev;
	DevInfo *registrar = regInfo->registrar;
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	bool b_zpadding = registrar->b_zpadding;
#endif /* WFA_WPS_20_TESTBED */

	/* First generate/gather all the required data. */
	message = WPS_ID_MESSAGE_M1;

	/* Enrollee nonce */
	RAND_bytes(regInfo->enrolleeNonce, SIZE_128_BITS);

	if (!registrar->DHSecret) {
		ret = reg_proto_generate_dhkeypair(&registrar->DHSecret);
		if (ret != WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "RPROTO: Cannot create DH Secret\n"));
			return ret;
		}
	}

	/* Extract the DH public key */
	if (reg_proto_BN_bn2bin(registrar->DHSecret->pub_key, regInfo->pke) != SIZE_PUB_KEY)
		return RPROT_ERR_CRYPTO;

	/* Now start composing the message */
	tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_UUID_E, msg, registrar->uuid, SIZE_UUID);
	tlv_serialize(WPS_ID_MAC_ADDR, msg, registrar->macAddr, SIZE_MAC_ADDR);
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_PUBLIC_KEY, msg, regInfo->pke, SIZE_PUB_KEY);
	tlv_serialize(WPS_ID_AUTH_TYPE_FLAGS, msg, &registrar->authTypeFlags,
		WPS_ID_AUTH_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_ENCR_TYPE_FLAGS, msg, &registrar->encrTypeFlags,
		WPS_ID_ENCR_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONN_TYPE_FLAGS, msg, &registrar->connTypeFlags,
		WPS_ID_CONN_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONFIG_METHODS, msg, &registrar->configMethods,
		WPS_ID_CONFIG_METHODS_S);
	tlv_serialize(WPS_ID_SC_STATE, msg, &registrar->scState,
		WPS_ID_SC_STATE_S);
	tlv_serialize(WPS_ID_MANUFACTURER, msg, registrar->manufacturer,
		STR_ATTR_LEN(registrar->manufacturer, SIZE_64_BYTES));
	tlv_serialize(WPS_ID_MODEL_NAME, msg, registrar->modelName,
		STR_ATTR_LEN(registrar->modelName, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_MODEL_NUMBER, msg, registrar->modelNumber,
		STR_ATTR_LEN(registrar->modelNumber, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_SERIAL_NUM, msg, registrar->serialNumber,
		STR_ATTR_LEN(registrar->serialNumber, SIZE_32_BYTES));

	primDev.categoryId = registrar->primDeviceCategory;
	primDev.oui = registrar->primDeviceOui;
	primDev.subCategoryId = registrar->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&primDev, msg);

	tlv_serialize(WPS_ID_DEVICE_NAME, msg, registrar->deviceName,
		STR_ATTR_LEN(registrar->deviceName, SIZE_32_BYTES));

	/* set real RF band */
	tlv_serialize(WPS_ID_RF_BAND, msg, &registrar->rfBand,
		WPS_ID_RF_BAND_S);
	tlv_serialize(WPS_ID_ASSOC_STATE, msg, &registrar->assocState,
		WPS_ID_ASSOC_STATE_S);
	tlv_serialize(WPS_ID_DEVICE_PWD_ID, msg, &registrar->devPwdId,
		WPS_ID_DEVICE_PWD_ID_S);
	tlv_serialize(WPS_ID_CONFIG_ERROR, msg, &registrar->configError,
		WPS_ID_CONFIG_ERROR_S);
	tlv_serialize(WPS_ID_OS_VERSION, msg, &registrar->osVersion,
		WPS_ID_OS_VERSION_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (registrar->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&registrar->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Add ReqToEnroll subelement to vendorExt_bufObj */
		if (registrar->b_reqToEnroll)
			subtlv_serialize(WPS_WFA_SUBID_REQ_TO_ENROLL, vendorExt_bufObj,
				&registrar->b_reqToEnroll,
				WPS_WFA_SUBID_REQ_TO_ENROLL_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (registrar->nattr_len)
			buffobj_Append(msg, registrar->nattr_len,
				(uint8*)registrar->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}


	/* skip other optional attributes */

	/* copy message to outMsg buffer */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, buffobj_Length(msg), buffobj_GetBuf(msg));
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM1 built: %d bytes\n", buffobj_Length(msg)));

	return WPS_SUCCESS;
}
