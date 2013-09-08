/*
 * Enrollee state machine
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: enr_reg_sm.c 395438 2013-04-08 02:01:22Z $
 */

#ifdef WIN32
#include <windows.h>
#endif

#ifndef UNDER_CE

#ifdef __ECOS
#include <sys/types.h>
#include <sys/socket.h>
#endif

#if defined(__linux__) || defined(__ECOS)
#include <net/if.h>
#endif

#endif /* UNDER_CE */

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
#include <wps_staeapsm.h>
#include <info.h>
#include <wpsapi.h>
#include <wpscommon.h>

#ifdef WFA_WPS_20_TESTBED
#define STR_ATTR_LEN(str, max) \
	((uint16)((b_zpadding && strlen(str) < max) ? strlen(str) + 1 : strlen(str)))
#else /* !WFA_WPS_20_TESTBED */
#define STR_ATTR_LEN(str, max)	((uint16)(strlen(str)))
#endif /* WFA_WPS_20_TESTBED */

static uint32 reg_proto_create_m3(RegData *regInfo, BufferObj *msg);
static uint32 reg_proto_create_m5(RegData *regInfo, BufferObj *msg);
static uint32 reg_proto_create_m7(RegData *regInfo, BufferObj *msg);
static uint32 reg_proto_create_done(RegData *regInfo, BufferObj *msg);

static uint32 reg_proto_process_m2d(RegData *regInfo, BufferObj *msg, uint8 *regNonce);
static uint32 reg_proto_process_m2(RegData *regInfo, BufferObj *msg, void **encrSettings,
	uint8 *regNoce);
static uint32 reg_proto_process_m4(RegData *regInfo, BufferObj *msg);
static uint32 reg_proto_process_m6(RegData *regInfo, BufferObj *msg);
static uint32 reg_proto_process_m8(RegData *regInfo, BufferObj *msg, void **encrSettings);

EnrSM *
enr_sm_new(void *g_mc)
{
	EnrSM *e = (EnrSM *)malloc(sizeof(EnrSM));

	if (!e)
		return NULL;

	memset(e, 0, sizeof(EnrSM));

	TUTRACE((TUTRACE_INFO, "Enrollee constructor\n"));
	e->g_mc = g_mc;

	e->reg_info = state_machine_new();
	if (e->reg_info == NULL) {
		free(e);
		return NULL;
	}

	return e;
}

void
enr_sm_delete(EnrSM *e)
{
	if (e) {
		if (e->mp_peerEncrSettings) {
			reg_msg_es_del(e->mp_peerEncrSettings, 0);
			e->mp_peerEncrSettings = NULL;
		}

		state_machine_delete(e->reg_info);

		TUTRACE((TUTRACE_INFO, "Free EnrSM %p\n", e));
		free(e);
	}
}

uint32
enr_sm_initializeSM(EnrSM *e, DevInfo *dev_info)
{
	uint32 err;

	TUTRACE((TUTRACE_INFO, "Enrollee Initialize SM\n"));

	err = state_machine_init_sm(e->reg_info);
	if (err != WPS_SUCCESS) {
		return err;
	}

	e->reg_info->dev_info = dev_info;
	e->reg_info->enrollee = dev_info;
	e->reg_info->registrar = NULL;

	/* update enrolleeNonce with prebuild_enrollee nonce */
	if (dev_info->flags & DEVINFO_FLAG_PRE_NONCE) {
		memcpy(e->reg_info->enrolleeNonce, dev_info->pre_nonce, SIZE_128_BITS);
	}

	return WPS_SUCCESS;
}

void
enr_sm_restartSM(EnrSM *e)
{
	RegData *reg_info;
	DevInfo *dev_info;
	uint32 err;

	TUTRACE((TUTRACE_INFO, "SM: Restarting the State Machine\n"));

	/* first extract the all info we need to save */
	reg_info = e->reg_info;
	dev_info = reg_info->dev_info;

	/* next, reinitialize the base SM class */
	err = state_machine_init_sm(e->reg_info);
	if (WPS_SUCCESS != err) {
		return;
	}

	/* Finally, store the data back into the regData struct */
	e->reg_info->dev_info = dev_info;
	e->reg_info->enrollee = dev_info;
	e->reg_info->initialized = true;
}

uint32
enr_sm_step(EnrSM *e, uint32 msgLen, uint8 *p_msg, uint8 *outbuffer, uint32 *out_len)
{
	BufferObj *inMsg, *outMsg;
	uint32 retVal = WPS_CONT;

	TUTRACE((TUTRACE_INFO, "ENRSM: Entering Step, , length = %d\n", msgLen));

	if (false == e->reg_info->initialized) {
		TUTRACE((TUTRACE_ERR, "ENRSM: Not yet initialized.\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	if (START == e->reg_info->e_smState) {
		TUTRACE((TUTRACE_INFO, "ENRSM: Step 1\n"));
		/* No special processing here */
		inMsg = buffobj_new();
		if (!inMsg)
			return WPS_ERR_OUTOFMEMORY;
		outMsg = buffobj_setbuf(outbuffer, *out_len);
		if (!outMsg) {
			buffobj_del(inMsg);
			return WPS_ERR_OUTOFMEMORY;
		}
		buffobj_dserial(inMsg, p_msg, msgLen);

		retVal = enr_sm_handleMessage(e, inMsg, outMsg);
		*out_len = buffobj_Length(outMsg);

		TUTRACE((TUTRACE_INFO, "ENRSM: Step 2\n"));

		buffobj_del(inMsg);
		buffobj_del(outMsg);
	}
	else {
		/* do the regular processing */
		if (!p_msg || !msgLen) {
			/* Preferential treatment for UPnP */
			if (e->reg_info->e_lastMsgSent == M1) {
				/*
				 * If we have already sent M1 and we get here, assume that it is
				 * another request for M1 rather than an error.
				 * Send the bufferred M1 message
				 */
				TUTRACE((TUTRACE_INFO, "ENRSM: Got another request for M1. "
					"Resending the earlier M1\n"));

				/* copy buffered M1 to msg_to_send buffer */
				if (*out_len < e->reg_info->outMsg->m_dataLength) {
					e->reg_info->e_smState = FAILURE;
					TUTRACE((TUTRACE_ERR, "output message buffer to small\n"));
					return WPS_MESSAGE_PROCESSING_ERROR;
				}
				memcpy(outbuffer, (char *)e->reg_info->outMsg->pBase,
					e->reg_info->outMsg->m_dataLength);
				*out_len = buffobj_Length(e->reg_info->outMsg);
				return WPS_SEND_MSG_CONT;
			}
			return WPS_CONT;
		}

		inMsg = buffobj_new();
		if (!inMsg)
			return WPS_ERR_OUTOFMEMORY;
		outMsg = buffobj_setbuf(outbuffer, *out_len);
		if (!outMsg) {
			buffobj_del(inMsg);
			return WPS_ERR_OUTOFMEMORY;
		}

		buffobj_dserial(inMsg, p_msg, msgLen);

		retVal = enr_sm_handleMessage(e, inMsg, outMsg);
		*out_len = buffobj_Length(outMsg);

		buffobj_del(inMsg);
		buffobj_del(outMsg);
	}

	/* now check the state so we can act accordingly */
	switch (e->reg_info->e_smState) {
	case START:
	case CONTINUE:
		/* do nothing. */
		break;

	case SUCCESS:
		/* reset the SM */
		enr_sm_restartSM(e);

		if (retVal == WPS_SEND_MSG_CONT)
			return WPS_SEND_MSG_SUCCESS;
		else
			return WPS_SUCCESS;

	case FAILURE:
		/* reset the SM */
		enr_sm_restartSM(e);

		if (retVal == WPS_SEND_MSG_CONT)
			return WPS_SEND_MSG_ERROR;
		else
			return WPS_MESSAGE_PROCESSING_ERROR;

	default:
		break;
	}

	return retVal;
}


uint32
enr_sm_handleMessage(EnrSM *e, BufferObj *msg, BufferObj *outmsg)
{
	uint32 err, retVal = WPS_CONT;
	void *encrSettings = NULL;
	uint32 msgType = 0;
	uint16 configError = WPS_ERROR_MSG_TIMEOUT;
	RegData *regInfo = e->reg_info;
	uint8 regNonce[SIZE_128_BITS] = { 0 };

	err = reg_proto_get_msg_type(&msgType, msg);
	if (WPS_SUCCESS != err) {
		TUTRACE((TUTRACE_ERR, "ENRSM: Expected M1, received %d\n", msgType));
		/* For DTM testing */
		/* goto out; */
	}

	/* If we get a valid message, extract the message type received.  */
	if (MNONE != e->reg_info->e_lastMsgSent) {
		/*
		 * If this is a late-arriving M2D, ACK it.  Otherwise, ignore it.
		 * Should probably also NACK an M2 at this point.
		 */
		if ((SM_RECVD_M2 == e->m_m2dStatus) &&
			(msgType <= WPS_ID_MESSAGE_M2D)) {
			if (WPS_ID_MESSAGE_M2D == msgType) {
				reg_proto_get_nonce(regNonce, msg, WPS_ID_REGISTRAR_NONCE);
				retVal = state_machine_send_ack(e->reg_info, outmsg, regNonce);
				TUTRACE((TUTRACE_INFO, "ENRSM: Possible late M2D received.  "
					"Sending ACK.\n"));
				goto out;
			}
		}
	}

	switch (e->reg_info->e_lastMsgSent) {
	case MNONE:
		if (WPS_ID_MESSAGE_M2 == msgType || WPS_ID_MESSAGE_M4 == msgType ||
			WPS_ID_MESSAGE_M6 == msgType || WPS_ID_MESSAGE_M8 == msgType ||
			WPS_ID_MESSAGE_NACK == msgType) {
			TUTRACE((TUTRACE_ERR, "ENRSM: msgType error type=%d, "
				"stay silent \n", msgType));
			goto out;
		}

		err = reg_proto_create_m1(e->reg_info, outmsg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Expected M1, received %d\n", msgType));
			goto out;
		}
		e->reg_info->e_lastMsgSent = M1;

		/* Set the m2dstatus. */
		e->m_m2dStatus = SM_AWAIT_M2;

		/* set the message state to CONTINUE */
		e->reg_info->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M1:
		/* for DTM 1.1 test */
		if (e->m_m2dStatus == SM_RECVD_M2D && WPS_ID_MESSAGE_NACK == msgType) {
			err = reg_proto_process_nack(e->reg_info, msg, &configError);
			if (configError != WPS_ERROR_NO_ERROR) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Recvd NACK with config err: "
					"%x\n", configError));
			}

			/* Don't say WPS ERROR when we failed at M2D NACK. The Win7 enrollee
			 * use two phases WPS mechanism to start WPS, the first phase start
			 * registration to retrieve AP's M1 and response M2D then NACK.
			 */
			e->err_code = WPS_M2D_NACK_CONT;

			/* for DTM 1.3 push button test */
			retVal = WPS_MESSAGE_PROCESSING_ERROR;

			enr_sm_restartSM(e);
			goto out;
		}

		/* Check whether this is M2D */
		if (WPS_ID_MESSAGE_M2D == msgType) {
			err = reg_proto_process_m2d(e->reg_info, msg, regNonce);
			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Expected M2D, err %x\n", msgType));
				goto out;
			}

			/* Send NACK if enrollee is AP */
			if (regInfo->enrollee->b_ap) {
				/* Send an NACK to the registrar */
				/* for DTM1.1 test */
				retVal = state_machine_send_nack(e->reg_info,
					WPS_ERROR_NO_ERROR, outmsg, regNonce);
				if (WPS_SEND_MSG_CONT != retVal) {
					TUTRACE((TUTRACE_ERR, "ENRSM: SendNack err %x\n", msgType));
					goto out;
				}
			}
			else {
				/* Send an ACK to the registrar */
				retVal = state_machine_send_ack(e->reg_info, outmsg, regNonce);
				if (WPS_SEND_MSG_CONT != retVal) {
					TUTRACE((TUTRACE_ERR, "ENRSM: SendAck err %x\n", msgType));
					goto out;
				}
			}

			if (SM_AWAIT_M2 == e->m_m2dStatus) {
				TUTRACE((TUTRACE_INFO, "ENRSM: Starting M2D\n"));

				e->m_m2dStatus = SM_RECVD_M2D;

				/*
				 * Now, sleep for some time to allow other
				 * registrars to send M2 or M2D messages.
				 */
				WpsSleep(1);

				/* set the message state to CONTINUE */
				e->reg_info->e_smState = CONTINUE;
				goto out;
			}
			else {
				TUTRACE((TUTRACE_INFO, "ENRSM: Did not start M2D. "
					"status = %d\n", e->m_m2dStatus));
			}
			goto out; /* done processing for M2D, return */
		}
		/* message wasn't M2D, do processing for M2 */
		else {
			if (SM_M2D_RESET == e->m_m2dStatus) {
				/* a SM reset has been initiated. Don't process any M2s */
				goto out;
			}
			else {
				e->m_m2dStatus = SM_RECVD_M2;
			}

			err = reg_proto_process_m2(e->reg_info, msg, NULL, regNonce);
			if (WPS_SUCCESS != err) {
				if (err == RPROT_ERR_AUTH_ENC_FLAG) {
					TUTRACE((TUTRACE_ERR, "ENRSM: Check auth or encry flag "
						"error: SendNack! %x\n", err));
					retVal = state_machine_send_nack(e->reg_info,
						WPS_ERROR_NW_AUTH_FAIL, outmsg, regNonce);
					/* send msg_to_send message immedicate */
					enr_sm_restartSM(e);
					goto out;
				}
				else if (err == RPROT_ERR_MULTIPLE_M2) {
					TUTRACE((TUTRACE_ERR, "ENRSM: receive multiple M2 "
						"error: SendNack! %x\n", err));
					retVal = state_machine_send_nack(e->reg_info,
						WPS_ERROR_DEVICE_BUSY, outmsg, regNonce);
					goto out;
				} /* WSC 2.0 */
				else if (err == RPROT_ERR_ROGUE_SUSPECTED) {
					TUTRACE((TUTRACE_ERR, "ENRSM: receive rogue suspected MAC "
						"error: SendNack! %x\n", err));
					retVal = state_machine_send_nack(e->reg_info,
						WPS_ERROR_ROGUE_SUSPECTED, outmsg, regNonce);
					goto out;
				}
				else { /* for DTM1.1 test */
					TUTRACE((TUTRACE_ERR, "ENRSM: HMAC "
						"error: SendNack! %x\n", err));
					retVal = state_machine_send_nack(e->reg_info,
						WPS_ERROR_DECRYPT_CRC_FAIL, outmsg, regNonce);
					/* send msg_to_send message immedicate */
					enr_sm_restartSM(e);
					goto out;
				}
			}
			e->reg_info->e_lastMsgRecd = M2;

			err = reg_proto_create_m3(e->reg_info, outmsg);

			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Build M3 failed %x\n", err));
				retVal = state_machine_send_nack(e->reg_info,
					WPS_ERROR_DEVICE_BUSY, outmsg, regNonce);
				goto out;
			}
			e->reg_info->e_lastMsgSent = M3;

			/* set the message state to CONTINUE */
			e->reg_info->e_smState = CONTINUE;

			/* msg_to_send is ready */
			retVal = WPS_SEND_MSG_CONT;
		}
		break;

	case M3:
		err = reg_proto_process_m4(e->reg_info, msg);
		if (WPS_SUCCESS != err) {
			/* for DTM testing */
			if (WPS_ID_MESSAGE_M2 == msgType) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Receive multiple M2 "
					"error! %x\n", err));
				goto out;
			}

			TUTRACE((TUTRACE_ERR, "ENRSM: Process M4 failed %x\n", err));
			/* Send a NACK to the registrar */
			if (err == RPROT_ERR_CRYPTO) {
				retVal = state_machine_send_nack(e->reg_info,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg, NULL);
				retVal = WPS_ERR_ENROLLMENT_PINFAIL;
			} else if (err == RPROT_ERR_WRONG_MSGTYPE &&
				msgType == WPS_ID_MESSAGE_NACK) {
				/* set the message state to failure */
				e->reg_info->e_smState = FAILURE;
				retVal = WPS_MESSAGE_PROCESSING_ERROR;
				break;
			} else {
				retVal = state_machine_send_nack(e->reg_info,
					WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			}
			enr_sm_restartSM(e);
			goto out;
		}
		e->reg_info->e_lastMsgRecd = M4;

		err = reg_proto_create_m5(e->reg_info, outmsg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Build M5 failed %x\n", err));
			retVal = state_machine_send_nack(e->reg_info,
				WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			goto out;
		}
		e->reg_info->e_lastMsgSent = M5;

		/* set the message state to CONTINUE */
		e->reg_info->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M5:
		err = reg_proto_process_m6(e->reg_info, msg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Process M6 failed %x\n", err));
			/* Send a NACK to the registrar */
			if (err == RPROT_ERR_CRYPTO) {
				retVal = state_machine_send_nack(e->reg_info,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg, NULL);
				/* Add in PF #3 */
				retVal = WPS_ERR_ENROLLMENT_PINFAIL;
			} else if (err == RPROT_ERR_WRONG_MSGTYPE &&
				msgType == WPS_ID_MESSAGE_NACK) {
				/* set the message state to failure */
				e->reg_info->e_smState = FAILURE;
				retVal = WPS_MESSAGE_PROCESSING_ERROR;
				break;
			} else {
				retVal = state_machine_send_nack(e->reg_info,
					WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			}
			enr_sm_restartSM(e);
			goto out;
		}
		e->reg_info->e_lastMsgRecd = M6;

		/* Build message 7 with the appropriate encrypted settings */
		err = reg_proto_create_m7(e->reg_info, outmsg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Build M7 failed %x\n", err));
			retVal = state_machine_send_nack(e->reg_info,
				WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			goto out;
		}

		e->reg_info->e_lastMsgSent = M7;

		/* set the message state to CONTINUE */
		e->reg_info->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M7:
		/* for DTM 1.1 test, if enrollee is AP */
		if (regInfo->enrollee->b_ap && WPS_ID_MESSAGE_NACK == msgType) {
			err = reg_proto_process_nack(e->reg_info, msg, &configError);
			if (configError != WPS_ERROR_NO_ERROR) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Recvd NACK with config err: "
					"%d\n", configError));
			}

			/* set the message state to success */
			e->reg_info->e_smState = SUCCESS;
			retVal = WPS_SUCCESS;
			goto out;
		}

		err = reg_proto_process_m8(e->reg_info, msg, &encrSettings);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Process M8 failed %x\n", err));

			/* Record the real error code for further use */
			e->err_code = err;

			configError = WPS_ERROR_DEVICE_BUSY;

			if (err == RPROT_ERR_CRYPTO)
				configError = WPS_ERROR_DEV_PWD_AUTH_FAIL;
			if (err == RPROT_ERR_ROGUE_SUSPECTED)
				configError = WPS_ERROR_ROGUE_SUSPECTED;

			/* send fail in all case */
			retVal = state_machine_send_nack(e->reg_info, configError, outmsg, NULL);
			enr_sm_restartSM(e);
			goto out;
		}

		e->reg_info->e_lastMsgRecd = M8;
		e->mp_peerEncrSettings = encrSettings;

		/* Send a Done message */
		err = reg_proto_create_done(e->reg_info, outmsg);
		if (err != WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "ENRSM: reg_proto_create_done generated an "
			"error: %d\n", err));
			retVal = err;
		}
		else {
			retVal = WPS_SEND_MSG_CONT;
		}

		e->reg_info->e_lastMsgSent = DONE;

		/* Decide if we need to wait for an EAP-Failure
		 * Wait only if we're an AP AND we're running EAP
		 */
		if ((!e->reg_info->enrollee->b_ap) ||
			(e->reg_info->transportType != TRANSPORT_TYPE_EAP)) {
			/* case 1: We are not an AP and run an erollee process (enr_sm)
			 * case 2: Message from UPnP
			 */
			e->reg_info->e_smState = SUCCESS;
		}
		else {
			/* AP runs as an enrolle,
			 * Wait registrar send WPS ACK
			 * See WSC2.0 spec 5.2.1.
			 */
			e->reg_info->e_smState = CONTINUE;
		}
		break;

	case DONE:
		err = reg_proto_process_ack(e->reg_info, msg);
		if (RPROT_ERR_NONCE_MISMATCH == err) {
			 /* ignore nonce mismatches */
			e->reg_info->e_smState = CONTINUE;
		}
		else if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Expected M1, received %d\n", msgType));
			goto out;
		}

		/* Set the message state to SUCCESS, then
		 * EAP layer will send EAP-Failure
		 */
		e->reg_info->e_smState = SUCCESS;
		break;

	default:
		TUTRACE((TUTRACE_ERR, "ENRSM: Expected M1, received %d\n", msgType));
		goto out;
	}

out:

	return retVal;
}

/* WSC 2.0 */
int
reg_proto_MacCheck(DevInfo *enrollee, void *encrSettings)
{
	EsM8Ap *esAP;
	EsM8Sta *esSta;
	CTlvCredential *pCredential;
	WPS_SSLIST *itr;
	bool b_ap;
	uint8 *mac;

	if (!enrollee || !encrSettings)
		return WPS_ERR_INVALID_PARAMETERS;

	b_ap = enrollee->b_ap;
	mac = enrollee->macAddr;

	if (b_ap) {
		esAP = (EsM8Ap *)encrSettings;
		if (esAP->macAddr.m_data == NULL ||
		    memcmp(mac, esAP->macAddr.m_data, SIZE_MAC_ADDR) != 0) {
			TUTRACE((TUTRACE_ERR, "Rogue suspected\n"));
			TUTRACE((TUTRACE_ERR, "AP enrollee mac %02x:%02x:%02x:%02x:%02x:%02x\n",
				mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]));
			if (esAP->macAddr.m_data == NULL) {
				TUTRACE((TUTRACE_ERR, "No AP's mac address!."));
			}
			else {
				TUTRACE((TUTRACE_ERR, "AP's mac %02x:%02x:%02x:%02x:%02x:%02x\n",
					esAP->macAddr.m_data[0], esAP->macAddr.m_data[1],
					esAP->macAddr.m_data[2], esAP->macAddr.m_data[3],
					esAP->macAddr.m_data[4], esAP->macAddr.m_data[5]));
			}
			return RPROT_ERR_ROGUE_SUSPECTED;
		}
	}
	else {
		esSta = (EsM8Sta *)encrSettings;
		if (esSta->credential == NULL) {
			TUTRACE((TUTRACE_ERR, "Illegal credential\n"));
			return RPROT_ERR_ROGUE_SUSPECTED;
		}

		itr = esSta->credential;
		for (; (pCredential = (CTlvCredential *)itr); itr = itr->next) {
			if (pCredential->macAddr.m_data == NULL ||
			    memcmp(mac, pCredential->macAddr.m_data, SIZE_MAC_ADDR) != 0) {
				TUTRACE((TUTRACE_ERR, "Rogue suspected\n"));
				TUTRACE((TUTRACE_ERR, "STA enrollee mac "
					"%02x:%02x:%02x:%02x:%02x:%02x\n",
					mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]));
				if (pCredential->macAddr.m_data == NULL) {
					TUTRACE((TUTRACE_ERR, "No device's mac address!."));
				}
				else {
					TUTRACE((TUTRACE_ERR, "Device's mac "
						"%02x:%02x:%02x:%02x:%02x:%02x\n",
						pCredential->macAddr.m_data[0],
						pCredential->macAddr.m_data[1],
						pCredential->macAddr.m_data[2],
						pCredential->macAddr.m_data[3],
						pCredential->macAddr.m_data[4],
						pCredential->macAddr.m_data[5]));
				}
				return RPROT_ERR_ROGUE_SUSPECTED;
			}
		}
	}

	return WPS_SUCCESS;
}

/* WSC 2.0 */
int
reg_proto_EncrSettingsCheck(DevInfo *enrollee, void *encrSettings)
{
	EsM8Ap *esAP;
	EsM8Sta *esSta;
	CTlvCredential *pCredential;
	WPS_SSLIST *itr, *next;
	WPS_SSLIST *prev = NULL;
	bool b_ap;

	if (!enrollee || !encrSettings)
		return WPS_ERR_INVALID_PARAMETERS;

	b_ap = enrollee->b_ap;
	if (b_ap) {
		esAP = (EsM8Ap *)encrSettings;
		if (esAP->encrType.m_data == WPS_ENCRTYPE_WEP) {
			TUTRACE((TUTRACE_ERR, "Deprecated WEP encryption detected\n"));
			return RPROT_ERR_INCOMPATIBLE_WEP;
		}
	}
	else {
		esSta = (EsM8Sta *)encrSettings;
		if (esSta->credential == NULL) {
			TUTRACE((TUTRACE_ERR, "Illegal credential\n"));
			return RPROT_ERR_INCOMPATIBLE;
		}

		/* Remove WEP credential */
		itr = esSta->credential;
		while ((pCredential = (CTlvCredential *)itr)) {
			next = itr->next;
			if (pCredential->encrType.m_data == WPS_ENCRTYPE_WEP) {
				if (itr == esSta->credential)
					esSta->credential = next;
				else
					prev->next = next;

				TUTRACE((TUTRACE_ERR, "Deprecated a WEP encryption\n"));
				tlv_credentialDelete(pCredential, 0);
			}
			else {
				prev = itr;
			}
			itr = next;
		}

		if (esSta->credential == NULL) {
			TUTRACE((TUTRACE_ERR, "Deprecated WEP encryption detected\n"));
			return RPROT_ERR_INCOMPATIBLE_WEP;
		}
	}

	return WPS_SUCCESS;
}

uint32
reg_proto_create_m1(RegData *regInfo, BufferObj *msg)
{
	uint32 ret = WPS_SUCCESS;
	uint8 message;
	CTlvPrimDeviceType primDev;
	DevInfo *enrollee = regInfo->enrollee;
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	bool b_zpadding = enrollee->b_zpadding;
#endif /* WFA_WPS_20_TESTBED */
	uint16 configMethods;

	/* First generate/gather all the required data. */
	message = WPS_ID_MESSAGE_M1;

	/* Enrollee nonce */
	/*
	 * Hacking, do not generate new random enrollee nonce
	 * in case of we have prebuild enrollee nonce.
	 */
	if (regInfo->e_lastMsgSent == MNONE) {
		RAND_bytes(regInfo->enrolleeNonce, SIZE_128_BITS);
		TUTRACE((TUTRACE_ERR, "RPROTO: create enrolleeNonce\n"));
	}
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM1 \n"));

	/* It should not generate new key pair if we have prebuild enrollee nonce */
	if (!enrollee->DHSecret) {
		ret = reg_proto_generate_dhkeypair(&enrollee->DHSecret);
		if (ret != WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "RPROTO: Cannot create DH Secret\n"));
			return ret;
		}

		TUTRACE((TUTRACE_ERR, "RPROTO: create DH Secret\n"));
	}

	/* Extract the DH public key */
	if (reg_proto_BN_bn2bin(enrollee->DHSecret->pub_key, regInfo->pke) != SIZE_PUB_KEY)
		return RPROT_ERR_CRYPTO;

	/* Now start composing the message */
	tlv_serialize(WPS_ID_VERSION, msg, &enrollee->version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_UUID_E, msg, enrollee->uuid, SIZE_UUID);
	tlv_serialize(WPS_ID_MAC_ADDR, msg, enrollee->macAddr, SIZE_MAC_ADDR);
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_PUBLIC_KEY, msg, regInfo->pke, SIZE_PUB_KEY);
	tlv_serialize(WPS_ID_AUTH_TYPE_FLAGS, msg, &enrollee->authTypeFlags,
		WPS_ID_AUTH_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_ENCR_TYPE_FLAGS, msg, &enrollee->encrTypeFlags,
		WPS_ID_ENCR_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONN_TYPE_FLAGS, msg, &enrollee->connTypeFlags,
		WPS_ID_CONN_TYPE_FLAGS_S);
	/*
	 * For DTM1.5 /1.6 test case "WCN Wireless Compare Probe Response and M1 messages",
	 * Configuration Methods in M1 must matched in Probe Response
	 */
	configMethods = enrollee->configMethods;
	/*
	 * 1.In WPS Test Plan 2.0.3, case 4.1.2 step 4. APUT cannot use push button to add ER,
	 * here we make Config Methods in both Probe Resp and M1 consistently.
	 * 2.DMT PBC testing check the PBC method from CONFIG_METHODS field (for now, DMT
	 * testing only avaliable on WPS 1.0
	 */
	if (enrollee->b_ap) {
		if (enrollee->version2 >= WPS_VERSION2) {
			/* configMethods &= ~(WPS_CONFMET_VIRT_PBC | WPS_CONFMET_PHY_PBC); */
		}
		else {
			/* WPS 1.0 */
			if (enrollee->scState == WPS_SCSTATE_UNCONFIGURED)
				configMethods &= ~WPS_CONFMET_PBC;
		}
	}
	tlv_serialize(WPS_ID_CONFIG_METHODS, msg, &configMethods, WPS_ID_CONFIG_METHODS_S);
	tlv_serialize(WPS_ID_SC_STATE, msg, &enrollee->scState, WPS_ID_SC_STATE_S);
	tlv_serialize(WPS_ID_MANUFACTURER, msg, enrollee->manufacturer,
		STR_ATTR_LEN(enrollee->manufacturer, SIZE_64_BYTES));
	tlv_serialize(WPS_ID_MODEL_NAME, msg, enrollee->modelName,
		STR_ATTR_LEN(enrollee->modelName, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_MODEL_NUMBER, msg, enrollee->modelNumber,
		STR_ATTR_LEN(enrollee->modelNumber, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_SERIAL_NUM, msg, enrollee->serialNumber,
		STR_ATTR_LEN(enrollee->serialNumber, SIZE_32_BYTES));

	primDev.categoryId = enrollee->primDeviceCategory;
	primDev.oui = enrollee->primDeviceOui;
	primDev.subCategoryId = enrollee->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&primDev, msg);
	tlv_serialize(WPS_ID_DEVICE_NAME, msg, enrollee->deviceName,
		STR_ATTR_LEN(enrollee->deviceName, SIZE_32_BYTES));

	/* set real RF band */
	tlv_serialize(WPS_ID_RF_BAND, msg, &enrollee->rfBand, WPS_ID_RF_BAND_S);
	tlv_serialize(WPS_ID_ASSOC_STATE, msg, &enrollee->assocState,
		WPS_ID_ASSOC_STATE_S);

	tlv_serialize(WPS_ID_DEVICE_PWD_ID, msg, &enrollee->devPwdId,
		WPS_ID_DEVICE_PWD_ID_S);
	tlv_serialize(WPS_ID_CONFIG_ERROR, msg, &enrollee->configError,
		WPS_ID_CONFIG_ERROR_S);
	tlv_serialize(WPS_ID_OS_VERSION, msg, &enrollee->osVersion,
		WPS_ID_OS_VERSION_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (enrollee->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&enrollee->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Add reqToEnroll subelement to vendorExt_bufObj */
		if (enrollee->b_reqToEnroll)
			subtlv_serialize(WPS_WFA_SUBID_REQ_TO_ENROLL, vendorExt_bufObj,
				&enrollee->b_reqToEnroll,
				WPS_WFA_SUBID_REQ_TO_ENROLL_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (enrollee->nattr_len)
			buffobj_Append(msg, enrollee->nattr_len,
				(uint8 *)enrollee->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Add support for Windows Rally Vertical Pairing */
	reg_proto_vendor_ext_vp(enrollee, msg);

	/* skip other... optional attributes */

	/* copy message to outMsg buffer */
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM1 copying to output %d bytes\n",
		buffobj_Length(msg)));

	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, buffobj_Length(msg), buffobj_GetBuf(msg));

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM1 built: %d bytes\n",
		buffobj_Length(msg)));

	return WPS_SUCCESS;
}

static uint32
reg_proto_process_m2(RegData *regInfo, BufferObj *msg, void **encrSettings, uint8 *regNonce)
{
	WpsM2 m;
	uint8 *Pos;
	uint32 err, tlv_err = 0;
	uint8 DHKey[SIZE_256_BITS];
	BufferObj *buf1, *buf1_dser, *buf2_dser;
	uint8 secret[SIZE_PUB_KEY];
	int secretLen;
	BufferObj *kdkBuf;
	BufferObj *kdkData;
	BufferObj *pString;
	BufferObj *keys;
	BufferObj *hmacData;
	uint8 kdk[SIZE_256_BITS];
	uint8 null_uuid[SIZE_16_BYTES] = {0};
	DevInfo *registrar;

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

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM2: %d byte message\n",
		buffobj_Length(msg)));

	/* First, deserialize (parse) the message. */

	/* First and foremost, check the version and message number */
	reg_msg_init(&m, WPS_ID_MESSAGE_M2);

	if ((err = reg_msg_version_check(WPS_ID_MESSAGE_M2, msg,
		&m.version, &m.msgType)) != WPS_SUCCESS)
		goto error;

	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE,
		msg, SIZE_128_BITS, 0);
	tlv_err |= tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE,
		msg, SIZE_128_BITS, 0);
	tlv_err |= tlv_dserialize(&m.uuid, WPS_ID_UUID_R, msg, SIZE_UUID, 0);
	tlv_err |= tlv_dserialize(&m.publicKey, WPS_ID_PUBLIC_KEY, msg, SIZE_PUB_KEY, 0);
	tlv_err |= tlv_dserialize(&m.authTypeFlags, WPS_ID_AUTH_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.encrTypeFlags, WPS_ID_ENCR_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.connTypeFlags, WPS_ID_CONN_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.configMethods, WPS_ID_CONFIG_METHODS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.manufacturer, WPS_ID_MANUFACTURER,
		msg, SIZE_64_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.modelName, WPS_ID_MODEL_NAME, msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.modelNumber, WPS_ID_MODEL_NUMBER,
		msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.serialNumber, WPS_ID_SERIAL_NUM, msg, SIZE_32_BYTES, 0);

	tlv_err |= tlv_primDeviceTypeParse(&m.primDeviceType, msg);
	tlv_err |= tlv_dserialize(&m.deviceName, WPS_ID_DEVICE_NAME,
		msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.rfBand, WPS_ID_RF_BAND, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.assocState, WPS_ID_ASSOC_STATE, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.configError, WPS_ID_CONFIG_ERROR, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.devPwdId, WPS_ID_DEVICE_PWD_ID, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.osVersion, WPS_ID_OS_VERSION, msg, 0, 0);

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->enrollee->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			/* Deserialize subelement */
			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* WSC 2.0, is next Version 2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				tlv_err |= subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/*
	 * For DTM Error Handling Multiple M2 Testing.
	 * We provide the coming M2 R-NONCE in regNonce if any.
	 * It may used by reg_proto_create_nack() when multiple M2 error happen.
	 */
	memcpy(regNonce, m.registrarNonce.m_data, m.registrarNonce.tlvbase.m_len);

	TUTRACE((TUTRACE_INFO, "RPROTO: m2.authTypeFlags.m_data()=%x, authTypeFlags=%x, "
		"m2.encrTypeFlags.m_data()=%x, encrTypeFlags=%x  \n",
		m.authTypeFlags.m_data, regInfo->enrollee->authTypeFlags,
		m.encrTypeFlags.m_data, regInfo->enrollee->encrTypeFlags));

	if (((m.authTypeFlags.m_data & regInfo->enrollee->authTypeFlags) == 0) ||
		((m.encrTypeFlags.m_data & regInfo->enrollee->encrTypeFlags) == 0)) {
		err = RPROT_ERR_AUTH_ENC_FLAG;
		TUTRACE((TUTRACE_ERR, "RPROTO: M2 Auth/Encr type not acceptable\n"));
		goto error;
	}

	/* AP have received M2,it must ignor other M2 */
	if (regInfo->registrar &&
	    memcmp(regInfo->registrar->uuid, null_uuid, SIZE_16_BYTES) != 0 &&
	    memcmp(regInfo->registrar->uuid, m.uuid.m_data, m.uuid.tlvbase.m_len) != 0) {
		TUTRACE((TUTRACE_ERR, "\n***********  multiple M2 !!!! *********\n"));
		err = RPROT_ERR_MULTIPLE_M2;
		goto error;
	}

	/* for VISTA TEST -- */
	if (tlv_err) {
		err =  RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/*
	 * skip the other vendor extensions and any other optional TLVs until
	 * we get to the authenticator
	 */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* optional encrypted settings */
		if (WPS_ID_ENCR_SETTINGS == buffobj_NextType(msg) &&
			m.encrSettings.encrDataLength == 0) {
			/* only process the first encrypted settings attribute encountered */
			tlv_err |= tlv_encrSettingsParse(&m.encrSettings, msg);
			Pos = buffobj_Pos(msg);
		}
		else {
			/* advance past the TLV */
			Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
				WpsNtohs((buffobj_Pos(msg)+sizeof(uint16))));
		}

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err = RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_err |= tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR,
		msg, SIZE_64_BITS, 0);
	if (tlv_err) {
		err =  RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Now start processing the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err = RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/*
	  * to verify the hmac, we need to process the nonces, generate
	  * the DH secret, the KDK and finally the auth key
	  */
	memcpy(regInfo->registrarNonce, m.registrarNonce.m_data, m.registrarNonce.tlvbase.m_len);

	/*
	 * read the registrar's public key
	 * First store the raw public key (to be used for e/rhash computation)
	 */
	memcpy(regInfo->pkr, m.publicKey.m_data, SIZE_PUB_KEY);

	/* Next, allocate memory for the pub key */
	regInfo->DH_PubKey_Peer = BN_new();
	if (!regInfo->DH_PubKey_Peer) {
		err = WPS_ERR_OUTOFMEMORY;
		goto error;
	}

	/* Finally, import the raw key into the bignum datastructure */
	if (BN_bin2bn(regInfo->pkr, SIZE_PUB_KEY,
		regInfo->DH_PubKey_Peer) == NULL) {
		BN_free(regInfo->DH_PubKey_Peer);
		err =  RPROT_ERR_CRYPTO;
		goto error;
	}

	/* KDK generation */
	/* 1. generate the DH shared secret */
	secretLen = DH_compute_key_bn(secret, regInfo->DH_PubKey_Peer,
		regInfo->enrollee->DHSecret);
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

	/* 3. Append the enrollee nonce(N1), enrollee mac and registrar nonce(N2) */
	kdkData = buf1;
	buffobj_Reset(kdkData);
	buffobj_Append(kdkData, SIZE_128_BITS, regInfo->enrolleeNonce);
	buffobj_Append(kdkData, SIZE_MAC_ADDR, regInfo->enrollee->macAddr);
	buffobj_Append(kdkData, SIZE_128_BITS, regInfo->registrarNonce);

	/* 4. now generate the KDK */
	hmac_sha256(DHKey, SIZE_256_BITS,
		buffobj_GetBuf(kdkData), buffobj_Length(kdkData), kdk, NULL);

	/* KDK generation */
	/* Derivation of AuthKey, KeyWrapKey and EMSK */
	/* 1. declare and initialize the appropriate buffer objects */
	kdkBuf = buf1_dser;
	buffobj_dserial(kdkBuf, kdk, SIZE_256_BITS);
	pString = buf2_dser;
	buffobj_dserial(pString, (uint8 *)PERSONALIZATION_STRING,
		(uint32)strlen(PERSONALIZATION_STRING));
	keys = buf1;
	buffobj_Reset(keys);

	/* 2. call the key derivation function */
	reg_proto_derivekey(kdkBuf, pString, KDF_KEY_BITS, keys);

	/* 3. split the key into the component keys and store them */
	buffobj_RewindLength(keys, buffobj_Length(keys));
	buffobj_Append(regInfo->authKey, SIZE_256_BITS, buffobj_Pos(keys));
	buffobj_Advance(keys, SIZE_256_BITS);

	buffobj_Append(regInfo->keyWrapKey, SIZE_128_BITS, buffobj_Pos(keys));
	buffobj_Advance(keys, SIZE_128_BITS);

	buffobj_Append(regInfo->emsk, SIZE_256_BITS, buffobj_Pos(keys));
	/* Derivation of AuthKey, KeyWrapKey and EMSK */

	/* HMAC validation */
	hmacData = buf1;
	buffobj_Reset(hmacData);
	/* append the last message sent */
	buffobj_Append(hmacData, buffobj_Length(regInfo->outMsg), buffobj_GetBuf(regInfo->outMsg));

	/* append the current message. Don't append the last TLV (auth) */
	buffobj_Append(hmacData,
	        msg->m_dataLength -(sizeof(WpsTlvHdr)+ m.authenticator.tlvbase.m_len),
	        msg->pBase);

	if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed\n"));
		err = RPROT_ERR_CRYPTO;
		goto error;
	}
	/* HMAC validation */

	/* Save Registrar's Informations */
	/* Now we can proceed with copying out and processing the other data */
	if (!regInfo->registrar) {
		regInfo->registrar = (DevInfo *)alloc_init(sizeof(DevInfo));
		if (!regInfo->registrar) {
			err = WPS_ERR_OUTOFMEMORY;
			goto error;
		}
	}

	/* Ease the job */
	registrar = regInfo->registrar;

	memcpy(registrar->uuid, m.uuid.m_data, m.uuid.tlvbase.m_len);

	registrar->authTypeFlags = m.authTypeFlags.m_data;
	registrar->encrTypeFlags = m.encrTypeFlags.m_data;
	registrar->connTypeFlags = m.connTypeFlags.m_data;
	registrar->configMethods = m.configMethods.m_data;
	tlv_strncpy(&m.manufacturer, registrar->manufacturer, sizeof(registrar->manufacturer));
	tlv_strncpy(&m.modelName, registrar->modelName, sizeof(registrar->modelName));
	tlv_strncpy(&m.serialNumber, registrar->serialNumber, sizeof(registrar->serialNumber));
	registrar->primDeviceCategory = m.primDeviceType.categoryId;
	registrar->primDeviceOui = m.primDeviceType.oui;
	registrar->primDeviceSubCategory = m.primDeviceType.subCategoryId;
	tlv_strncpy(&m.deviceName.m_data, registrar->deviceName, sizeof(registrar->deviceName));
	registrar->rfBand = m.rfBand.m_data;
	registrar->assocState = m.assocState.m_data;
	registrar->configError = m.configError.m_data;
	registrar->devPwdId = m.devPwdId.m_data;
	registrar->osVersion = m.osVersion.m_data;
	/* WSC 2.0 */
	registrar->version2 = m.version2.m_data;

	/* extract encrypted settings */
	if (m.encrSettings.encrDataLength) {
		BufferObj *cipherText = buf1_dser;
		BufferObj *plainText = 0;
		BufferObj *iv = 0;
		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);
		iv = buf2_dser;
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		plainText = buf1;
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);

		/* Usually, only out of band method gets enrSettings at M2 stage */
		if (encrSettings) {
			if (regInfo->enrollee->b_ap) {
				EsM8Ap *esAP = (EsM8Ap *)reg_msg_es_new(ES_TYPE_M8AP);
				if (!esAP) {
					err = WPS_ERR_OUTOFMEMORY;
					goto error;
				}

				reg_msg_m8ap_parse(esAP, plainText, regInfo->authKey, true);
				*encrSettings = (void *)esAP;
			}
			else {
				EsM8Sta *esSta = (EsM8Sta *)reg_msg_es_new(ES_TYPE_M8STA);
				if (!esSta) {
					err = WPS_ERR_OUTOFMEMORY;
					goto error;
				}

				reg_msg_m8sta_parse(esSta, plainText, regInfo->authKey, true);
				*encrSettings = (void *)esSta;
			}
			/*
			 * WSC 2.0, check MAC attribute only when registrar
			 * and enrollee are both WSC 2.0
			 */
			if (regInfo->registrar->version2 >= WPS_VERSION2 &&
				reg_proto_MacCheck(regInfo->enrollee, *encrSettings)
					!= WPS_SUCCESS) {
				err = RPROT_ERR_ROGUE_SUSPECTED;
				goto error;
			}
		}
	}
	/* extract encrypted settings */

	/*
	 * now set the registrar's b_ap flag. If the local enrollee is an ap,
	 * the registrar shouldn't be one
	 */
	if (regInfo->enrollee->b_ap)
		regInfo->registrar->b_ap = true;

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;

error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM2 : got error %x\n", err));
	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

static uint32
reg_proto_process_m2d(RegData *regInfo, BufferObj *msg, uint8 *regNonce)
{
	WpsM2D m;
	uint32 err = WPS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM2D: %d byte message\n",
		msg->m_dataLength));

	/* First, deserialize (parse) the message. */

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	reg_msg_init(&m, WPS_ID_MESSAGE_M2D);

	if ((err = reg_msg_version_check(WPS_ID_MESSAGE_M2D, msg,
		&m.version, &m.msgType)) != WPS_SUCCESS)
		goto error;

	tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);

	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.uuid, WPS_ID_UUID_R, msg, SIZE_UUID, 0);
	/* No Public Key in M2D */
	tlv_dserialize(&m.authTypeFlags, WPS_ID_AUTH_TYPE_FLAGS, msg, 0, 0);
	tlv_dserialize(&m.encrTypeFlags, WPS_ID_ENCR_TYPE_FLAGS, msg, 0, 0);
	tlv_dserialize(&m.connTypeFlags, WPS_ID_CONN_TYPE_FLAGS, msg, 0, 0);
	tlv_dserialize(&m.configMethods, WPS_ID_CONFIG_METHODS, msg, 0, 0);
	tlv_dserialize(&m.manufacturer, WPS_ID_MANUFACTURER, msg, SIZE_64_BYTES, 0);
	tlv_dserialize(&m.modelName, WPS_ID_MODEL_NAME, msg, SIZE_32_BYTES, 0);
	tlv_dserialize(&m.modelNumber, WPS_ID_MODEL_NUMBER, msg, SIZE_32_BYTES, 0);
	tlv_dserialize(&m.serialNumber, WPS_ID_SERIAL_NUM, msg, SIZE_32_BYTES, 0);

	tlv_primDeviceTypeParse(&m.primDeviceType, msg);

	tlv_dserialize(&m.deviceName, WPS_ID_DEVICE_NAME, msg, SIZE_32_BYTES, 0);
	tlv_dserialize(&m.rfBand, WPS_ID_RF_BAND, msg, 0, 0);
	tlv_dserialize(&m.assocState, WPS_ID_ASSOC_STATE, msg, 0, 0);
	tlv_dserialize(&m.configError, WPS_ID_CONFIG_ERROR, msg, 0, 0);
	tlv_dserialize(&m.osVersion, WPS_ID_OS_VERSION, msg, 0, 0);

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->enrollee->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			BufferObj *vendorExt_bufObj = buffobj_new();

			/* Deserialize subelement */
			if (!vendorExt_bufObj) {
				err = WPS_ERR_OUTOFMEMORY;
				goto error;
			}

			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* WSC 2.0, is next Version 2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);

			buffobj_del(vendorExt_bufObj);
		}
	}

	/* ignore any other TLVs in the message */

	/* Now start processing the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err = RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/*
	 * For DTM Error Handling Multiple M2 Testing.
	 * We provide the coming M2D R-NONCE in regNonce if any.
	 * It may used by reg_proto_create_nack() when multiple M2 error happen.
	 */
	memcpy(regNonce, m.registrarNonce.m_data, m.registrarNonce.tlvbase.m_len);

error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM2D : got error %x", err));

	return err;
}

static uint32
reg_proto_create_m3(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint8 hashBuf[SIZE_256_BITS];
	uint32 err;
	BufferObj *hmacData;
	uint8 hmac[SIZE_256_BITS];
	BufferObj *ehashBuf;
	uint8 *pwdPtr = 0;
	int pwdLen;

	BufferObj *buf1 = buffobj_new();

	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	message = WPS_ID_MESSAGE_M3;

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

	/* EHash1 and EHash2 generation */
	RAND_bytes(regInfo->es1, SIZE_128_BITS);
	RAND_bytes(regInfo->es2, SIZE_128_BITS);

	ehashBuf = buf1;
	buffobj_Reset(ehashBuf);
	buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->es1);
	buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->psk1);
	buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pke);
	buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pkr);
	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		ehashBuf->pBase, ehashBuf->m_dataLength, hashBuf, NULL);

	memcpy(regInfo->eHash1, hashBuf, SIZE_256_BITS);

	buffobj_Reset(ehashBuf);
	buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->es2);
	buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->psk2);
	buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pke);
	buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pkr);
	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		ehashBuf->pBase, ehashBuf->m_dataLength, hashBuf, NULL);

	memcpy(regInfo->eHash2, hashBuf, SIZE_256_BITS);
	/* EHash1 and EHash2 generation */

	/* Now assemble the message */
	tlv_serialize(WPS_ID_VERSION, msg, &regInfo->enrollee->version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);

	tlv_serialize(WPS_ID_E_HASH1, msg, regInfo->eHash1, SIZE_256_BITS);
	tlv_serialize(WPS_ID_E_HASH2, msg, regInfo->eHash2, SIZE_256_BITS);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->enrollee->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		/* Reuse buf1 */
		BufferObj *vendorExt_bufObj = buf1;

		buffobj_Reset(vendorExt_bufObj);

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->enrollee->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->enrollee->nattr_len)
			buffobj_Append(msg, regInfo->enrollee->nattr_len,
				(uint8*)regInfo->enrollee->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Calculate the hmac */
	hmacData = buf1;
	buffobj_Reset(hmacData);
	buffobj_Append(hmacData, regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
	buffobj_Append(hmacData, msg->m_dataLength, msg->pBase);

	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		hmacData->pBase, hmacData->m_dataLength, hmac, NULL);
	tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);

	/* Store the outgoing message  */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM3 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;

	buffobj_del(buf1);
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM3 : got error %x", err));

	return err;
}

static uint32
reg_proto_process_m4(RegData *regInfo, BufferObj *msg)
{
	uint8 *Pos;
	WpsM4 m;
	uint32 err, tlv_err = 0;
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

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM4: %d byte message\n",
		msg->m_dataLength));
	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	reg_msg_init(&m, WPS_ID_MESSAGE_M4);

	if ((err = reg_msg_version_check(WPS_ID_MESSAGE_M4, msg,
		&m.version, &m.msgType)) != WPS_SUCCESS)
		goto error;

	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE,
		msg, SIZE_128_BITS, 0);

	tlv_err |= tlv_dserialize(&m.rHash1, WPS_ID_R_HASH1, msg, SIZE_256_BITS, 0);
	tlv_err |= tlv_dserialize(&m.rHash2, WPS_ID_R_HASH2, msg, SIZE_256_BITS, 0);

	tlv_err |= tlv_encrSettingsParse(&m.encrSettings, msg);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->enrollee->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			buffobj_Reset(vendorExt_bufObj);

			/* Deserialize subelement */
			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* Get Version2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				tlv_err |= subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/* skip the other optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* advance past the TLV */
		Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs((msg->pCurrent+sizeof(uint16))));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_err |= tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR,
		msg, SIZE_64_BITS, 0);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
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

	/* Now copy the relevant data */
	memcpy(regInfo->rHash1, m.rHash1.m_data, SIZE_256_BITS);
	memcpy(regInfo->rHash2, m.rHash2.m_data, SIZE_256_BITS);

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = 0;
		BufferObj *plainText = 0;
		TlvEsNonce rNonce;

		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
		                  m.encrSettings.encrDataLength);
		iv = buf2_dser;
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		plainText = buf1;
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);
		reg_msg_nonce_parse(&rNonce, WPS_ID_R_SNONCE1, plainText, regInfo->authKey);

		/* extract encrypted settings */

		/* RHash1 validation */
		/* 1. Save RS1 */
		memcpy(regInfo->rs1, rNonce.nonce.m_data, rNonce.nonce.tlvbase.m_len);
	}

	/* 2. prepare the buffer */
	{
		uint8 hashBuf[SIZE_256_BITS];
		BufferObj *rhashBuf = buf1;
		buffobj_Reset(rhashBuf);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->rs1);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->psk1);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pkr);

		/* 3. generate the mac */
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			rhashBuf->pBase, rhashBuf->m_dataLength, hashBuf, NULL);

		/* 4. compare the mac to rhash1 */
		if (memcmp(regInfo->rHash1, hashBuf, SIZE_256_BITS)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: RS1 hash doesn't match RHash1\n"));
			err =  RPROT_ERR_CRYPTO;
			goto error;
		}
	}
	/* 5. Instead of steps 3 & 4, we could have called ValidateMac */
	/* RHash1 validation */

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM4 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM4: got error %x\n", err));

	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

static uint32
reg_proto_create_m5(RegData *regInfo, BufferObj *msg)
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
	message = WPS_ID_MESSAGE_M5;

	/* encrypted settings. */
	{
		BufferObj *encData = buf1;
		BufferObj *cipherText = buf2;
		BufferObj *iv = buf3;
		TlvEsNonce esNonce;
		buffobj_Reset(encData);
		buffobj_Reset(cipherText);
		buffobj_Reset(iv);
		tlv_set(&esNonce.nonce, WPS_ID_E_SNONCE1, regInfo->es1, SIZE_128_BITS);
		reg_msg_nonce_write(&esNonce, encData, regInfo->authKey);

		reg_proto_encrypt_data(encData, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);

		/* Now assemble the message */
		tlv_serialize(WPS_ID_VERSION, msg, &regInfo->enrollee->version,
			WPS_ID_VERSION_S);
		tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
		tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);
		{
			CTlvEncrSettings encrSettings;
			encrSettings.iv = iv->pBase;
			encrSettings.ip_encryptedData = cipherText->pBase;
			encrSettings.encrDataLength = cipherText->m_dataLength;
			tlv_encrSettingsWrite(&encrSettings, msg);
		}
	}

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->enrollee->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL) {
			err = WPS_ERR_OUTOFMEMORY;
			goto error;
		}

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->enrollee->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->enrollee->nattr_len)
			buffobj_Append(msg, regInfo->enrollee->nattr_len,
				(uint8*)regInfo->enrollee->nattr_tlv);
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


	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM5 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;

error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM5 : got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);

	return err;
}

static uint32
reg_proto_process_m6(RegData *regInfo, BufferObj *msg)
{
	uint8 *Pos;
	WpsM6 m;
	uint32 err, tlv_err = 0;

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

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM6: %d byte message\n",
		msg->m_dataLength));

	/*
	 * First, deserialize (parse) the message.
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	reg_msg_init(&m, WPS_ID_MESSAGE_M6);

	if ((err = reg_msg_version_check(WPS_ID_MESSAGE_M6, msg,
		&m.version, &m.msgType)) != WPS_SUCCESS)
		goto error;

	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE,
		msg, SIZE_128_BITS, 0);

	tlv_err |= tlv_encrSettingsParse(&m.encrSettings, msg);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->enrollee->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			buffobj_Reset(vendorExt_bufObj);

			/* Deserialize subelement */
			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* Get Version2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				tlv_err |= subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/* skip the other optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* advance past the TLV */
		Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs((msg->pCurrent+sizeof(uint16))));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_err |= tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR,
		msg, SIZE_64_BITS, 0);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err = RPROT_ERR_NONCE_MISMATCH;
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
			goto error;
		}
	}
	/* HMAC validation */

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = 0;
		BufferObj *plainText = 0;
		TlvEsNonce rNonce;

		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);
		iv = buf2_dser;
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		plainText = buf1;
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);
		reg_msg_nonce_parse(&rNonce, WPS_ID_R_SNONCE2, plainText, regInfo->authKey);

		/* extract encrypted settings */

		/* RHash2 validation */
		/* 1. Save RS2 */
		memcpy(regInfo->rs2, rNonce.nonce.m_data, rNonce.nonce.tlvbase.m_len);
	}

	/* 2. prepare the buffer */
	{
		uint8 hashBuf[SIZE_256_BITS];

		BufferObj *rhashBuf = buf1;
		buffobj_Reset(rhashBuf);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->rs2);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->psk2);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pkr);

		/* 3. generate the mac */
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			rhashBuf->pBase, rhashBuf->m_dataLength, hashBuf, NULL);

		/* 4. compare the mac to rhash2 */
		if (memcmp(regInfo->rHash2, hashBuf, SIZE_256_BITS)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: RS2 hash doesn't match RHash2\n"));
			err = RPROT_ERR_CRYPTO;
			goto error;
		}

		/* 5. Instead of steps 3 & 4, we could have called ValidateMac */
		/* RHash2 validation */
	}

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);

	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM6 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM6: got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

static uint32
reg_proto_create_m7(RegData *regInfo, BufferObj *msg)
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
	message = WPS_ID_MESSAGE_M7;

	/* encrypted settings. */
	{
		EsM7Ap *apEs = 0;
		BufferObj *esBuf = buf1;
		BufferObj *cipherText = buf2;
		BufferObj *iv = buf3;
		buffobj_Reset(esBuf);
		buffobj_Reset(cipherText);
		buffobj_Reset(iv);

		if (regInfo->enrollee->b_ap) {
			if (!regInfo->dev_info->mp_tlvEsM7Ap) {
				TUTRACE((TUTRACE_ERR, "RPROTO: AP Encr settings are NULL\n"));
				err =  WPS_ERR_INVALID_PARAMETERS;
				goto error;
			}

			apEs = (EsM7Ap *)regInfo->dev_info->mp_tlvEsM7Ap;
			/* Set ES Nonce2 */
			tlv_set(&apEs->nonce, WPS_ID_E_SNONCE2, regInfo->es2, SIZE_128_BITS);

			if (regInfo->enrollee->version2 >= WPS_VERSION2) {
				/*
				 * WSC 2.0, (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)
				 * allowed only when both the Registrar and the Enrollee
				 * are using WSC 2.0 or newer.
				 * Use WPS_AUTHTYPE_WPA2PSK when enrollee is WSC 1.0 only.
				 */
				if (regInfo->registrar->version2 < WPS_VERSION2)
					apEs->authType.m_data &= ~WPS_AUTHTYPE_WPAPSK;
			}

			reg_msg_m7ap_write(apEs, esBuf, regInfo->authKey);
		}
		else {
			EsM7Enr *staEs;

			if (!regInfo->dev_info->mp_tlvEsM7Sta) {
				TUTRACE((TUTRACE_INFO, "RPROTO: NULL STA Encrypted settings."
					" Allocating memory...\n"));
				staEs = (EsM7Enr *)reg_msg_es_new(ES_TYPE_M7ENR);
				if (staEs == NULL) {
					err = WPS_ERR_OUTOFMEMORY;
					goto error;
				}

				/* Set ES Nonce2 */
				tlv_set(&staEs->nonce, WPS_ID_E_SNONCE2, regInfo->es2,
					SIZE_128_BITS);
				reg_msg_m7enr_write(staEs, esBuf, regInfo->authKey);

				/* Free local staEs */
				reg_msg_es_del(staEs, 0);
			}
			else {
				staEs = (EsM7Enr *)regInfo->dev_info->mp_tlvEsM7Sta;

				/* Set ES Nonce2 */
				tlv_set(&staEs->nonce, WPS_ID_E_SNONCE2, regInfo->es2,
					SIZE_128_BITS);
				reg_msg_m7enr_write(staEs, esBuf, regInfo->authKey);
			}
		}

		/* Now encrypt the serialize Encrypted settings buffer */

		reg_proto_encrypt_data(esBuf, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);

		/* Now assemble the message */
		tlv_serialize(WPS_ID_VERSION, msg, &regInfo->enrollee->version,
			WPS_ID_VERSION_S);
		tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
		tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);
		{
			CTlvEncrSettings settings;
			settings.iv = iv->pBase;
			settings.ip_encryptedData = cipherText->pBase;
			settings.encrDataLength = cipherText->m_dataLength;
			tlv_encrSettingsWrite(&settings, msg);
		}

		if (regInfo->x509csr->m_dataLength) {
			tlv_serialize(WPS_ID_X509_CERT_REQ, msg, regInfo->x509csr->pBase,
				regInfo->x509csr->m_dataLength);
		}
	}

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->enrollee->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL) {
			err = WPS_ERR_OUTOFMEMORY;
			goto error;
		}

		/* Add settingsDelayTime vendorExt_bufObj */
		if (regInfo->enrollee->settingsDelayTime) {
			subtlv_serialize(WPS_WFA_SUBID_SETTINGS_DELAY_TIME, vendorExt_bufObj,
				&regInfo->enrollee->settingsDelayTime,
				WPS_WFA_SUBID_SETTINGS_DELAY_TIME_S);
		}

		/* Add Version2 vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->enrollee->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->enrollee->nattr_len)
			buffobj_Append(msg, regInfo->enrollee->nattr_len,
				(uint8*)regInfo->enrollee->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Calculate the hmac */
	{
		uint8 hmac[SIZE_256_BITS];

		BufferObj *hmacData = buf1;
		buffobj_Reset(hmacData);
		buffobj_Append(hmacData, regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
		buffobj_Append(hmacData, msg->m_dataLength, msg->pBase);

		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			hmacData->pBase, hmacData->m_dataLength, hmac, NULL);

		tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);
	}

	/* Store the outgoing message  */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM7 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM7 : got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);

	return err;
}

static uint32
reg_proto_process_m8(RegData *regInfo, BufferObj *msg, void **encrSettings)
{
	uint8 *Pos;
	WpsM8 m;
	uint32 err, tlv_err = 0;

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

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM8: %d byte message\n",
		msg->m_dataLength));

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	reg_msg_init(&m, WPS_ID_MESSAGE_M8);

	if ((err = reg_msg_version_check(WPS_ID_MESSAGE_M8, msg,
		&m.version, &m.msgType)) != WPS_SUCCESS)
		goto error;

	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE,
		msg, SIZE_128_BITS, 0);

	tlv_err |= tlv_encrSettingsParse(&m.encrSettings, msg);

	if (WPS_ID_X509_CERT == buffobj_NextType(msg))
		tlv_err |= tlv_dserialize(&m.x509Cert, WPS_ID_X509_CERT, msg, 0, 0);

	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->enrollee->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			buffobj_Reset(vendorExt_bufObj);

			/* Deserialize subelement */
			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* Get Version2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				tlv_err |= subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/* skip all optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* advance past the TLV */
		Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs((msg->pCurrent+sizeof(uint16))));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_err |= tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR, msg, SIZE_64_BITS, 0);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err = RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/* HMAC validation */
	{
		BufferObj *hmacData = buf1;
		buffobj_Reset(hmacData);
		/* append the last message sent */
		buffobj_Append(hmacData,  regInfo->outMsg->m_dataLength, regInfo->outMsg->pBase);
		/* append the current message. Don't append the last TLV (auth) */
		buffobj_Append(hmacData,
		        msg->m_dataLength-(sizeof(WpsTlvHdr)+m.authenticator.tlvbase.m_len),
		        msg->pBase);

		if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed\n"));
			err = RPROT_ERR_CRYPTO;
			goto error;
		}
	}
	/* HMAC validation */

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = 0;
		BufferObj *plainText = 0;
		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);
		iv = buf2_dser;
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		plainText = buf1;
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);

		if (regInfo->enrollee->b_ap) {
			EsM8Ap *esAP = (EsM8Ap *)reg_msg_es_new(ES_TYPE_M8AP);
			if (esAP == NULL) {
				err = WPS_ERR_OUTOFMEMORY;
				goto error;
			}
			reg_msg_m8ap_parse(esAP, plainText, regInfo->authKey, true);
			*encrSettings = (void *)esAP;
		}
		else {
			EsM8Sta *esSta = (EsM8Sta *)reg_msg_es_new(ES_TYPE_M8STA);
			if (esSta == NULL) {
				err = WPS_ERR_OUTOFMEMORY;
				goto error;
			}
			reg_msg_m8sta_parse(esSta, plainText, regInfo->authKey, true);
			*encrSettings = (void *)esSta;
		}

		if (regInfo->enrollee->version2 >= WPS_VERSION2) {
			/*
			 * WSC 2.0, check MAC attribute only when registrar
			 * and enrollee are both WSC 2.0
			 */
			if (regInfo->registrar->version2 >= WPS_VERSION2 &&
				reg_proto_MacCheck(regInfo->enrollee, *encrSettings)
					!= WPS_SUCCESS) {
				err = RPROT_ERR_ROGUE_SUSPECTED;
				goto error;
			}

			/* WSC 2.0, test case 4.1.10 */
			err = reg_proto_EncrSettingsCheck(regInfo->enrollee, *encrSettings);
			if (err != WPS_SUCCESS)
				goto error;
		}
	}
	/* extract encrypted settings */

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM8 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM8: got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

uint32
reg_proto_create_ack(RegData *regInfo, BufferObj *msg, uint8 *regNonce)
{
	uint8 data8;
	uint8 *R_Nonce = regNonce ? regNonce : regInfo->registrarNonce;

	TUTRACE((TUTRACE_INFO, "RPROTO: In BuildMessageAck\n"));

	data8 = WPS_VERSION;
	tlv_serialize(WPS_ID_VERSION, msg, &data8, WPS_ID_VERSION_S);

	data8 = WPS_ID_MESSAGE_ACK;
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &data8, WPS_ID_MSG_TYPE_S);

	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, R_Nonce, SIZE_128_BITS);

	/* WSC 2.0 */
	if (regInfo->dev_info->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		data8 = regInfo->dev_info->version2;

		/* Add Version2 vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&data8, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->enrollee->nattr_len)
			buffobj_Append(msg, regInfo->enrollee->nattr_len,
				(uint8*)regInfo->enrollee->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageAck built: %d bytes\n",
		msg->m_dataLength));

	return WPS_SUCCESS;
}

uint32
reg_proto_process_ack(RegData *regInfo, BufferObj *msg)
{
	WpsACK m;
	int err;

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageAck: %d byte message\n",
		msg->m_dataLength));

	/* First, deserialize (parse) the message. */

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	reg_msg_init(&m, WPS_ID_MESSAGE_ACK);

	if ((err = reg_msg_version_check(WPS_ID_MESSAGE_ACK, msg,
		&m.version, &m.msgType)) != WPS_SUCCESS)
	    return err;

	tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);

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

			buffobj_del(vendorExt_bufObj);
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

	/* ignore any other TLVs */

	return WPS_SUCCESS;
}

uint32
reg_proto_create_nack(RegData *regInfo, BufferObj *msg, uint16 configError, uint8 *regNonce)
{
	uint8 data8;
	uint8 *R_Nonce = regNonce ? regNonce : regInfo->registrarNonce;

	TUTRACE((TUTRACE_INFO, "RPROTO: In BuildMessageNack\n"));

	data8 = WPS_VERSION;
	tlv_serialize(WPS_ID_VERSION, msg, &data8, WPS_ID_VERSION_S);

	data8 = WPS_ID_MESSAGE_NACK;
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &data8, WPS_ID_MSG_TYPE_S);

	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, R_Nonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_CONFIG_ERROR, msg, &configError, WPS_ID_CONFIG_ERROR_S);

	/* WSC 2.0 */
	if (regInfo->dev_info->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		data8 = regInfo->dev_info->version2;

		/* Add Version2 vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&data8, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelements to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->enrollee->nattr_len)
			buffobj_Append(msg, regInfo->enrollee->nattr_len,
				(uint8*)regInfo->enrollee->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	return WPS_SUCCESS;
}

uint32
reg_proto_process_nack(RegData *regInfo, BufferObj *msg, uint16 *configError)
{
	WpsNACK m;
	int err;

	/* First, deserialize (parse) the message. */

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageNack: %d byte message\n",
		msg->m_dataLength));

	reg_msg_init(&m, WPS_ID_MESSAGE_NACK);

	if ((err = reg_msg_version_check(WPS_ID_MESSAGE_NACK, msg,
		&m.version, &m.msgType)) != WPS_SUCCESS)
		return err;

	tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.configError, WPS_ID_CONFIG_ERROR, msg, 0, 0);

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

			buffobj_del(vendorExt_bufObj);
		}
	}

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		return  RPROT_ERR_NONCE_MISMATCH;
	}

	/* confirm the registrar nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		return  RPROT_ERR_NONCE_MISMATCH;
	}

	/* ignore any other TLVs */

	*configError = m.configError.m_data;

	return WPS_SUCCESS;
}

static uint32
reg_proto_create_done(RegData *regInfo, BufferObj *msg)
{
	uint8 data8;

	TUTRACE((TUTRACE_INFO, "RPROTO: In BuildMessageDone\n"));

	data8 = WPS_VERSION;
	tlv_serialize(WPS_ID_VERSION, msg, &data8, WPS_ID_VERSION_S);

	data8 = WPS_ID_MESSAGE_DONE;
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &data8, WPS_ID_MSG_TYPE_S);

	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);

	/* WSC 2.0 */
	if (regInfo->dev_info->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		data8 = regInfo->dev_info->version2;

		/* Add Version2 vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&data8, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->enrollee->nattr_len)
			buffobj_Append(msg, regInfo->enrollee->nattr_len,
				(uint8*)regInfo->enrollee->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageDone built: %d bytes\n",
		msg->m_dataLength));

	return WPS_SUCCESS;
}
