/*
 * State Machine
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: statemachine.h 295126 2011-11-09 13:52:10Z $
 */

#ifndef __STATEMACHINE_H__
#define __STATEMACHINE_H__


#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__) || defined(__ECOS)
	#define stringPrintf snprintf
#else
	#define stringPrintf _snprintf
#endif

#define WPS_PROTOCOL_TIMEOUT 120
#define WPS_MESSAGE_TIMEOUT 25


/* M2D Status values */
#define SM_AWAIT_M2     0
#define SM_RECVD_M2     1
#define SM_RECVD_M2D    2
#define SM_M2D_RESET    3

uint32 state_machine_send_ack(RegData *reg_info, BufferObj *outmsg, uint8 *regNonce);
uint32 state_machine_send_nack(RegData *reg_info, uint16 configError, BufferObj *outmsg,
	uint8 *regNonce);

typedef struct {
	RegData *reg_info;

	/* The peer's encrypted settings will be stored here */
	void *mp_peerEncrSettings;
	bool m_localSMEnabled;
	bool m_passThruEnabled;

	/* Temporary state variable */
	bool m_sentM2;
	void *g_mc;

	/* External Registrar variables */
	bool m_er_sentM2;
	uint8 m_er_nonce[SIZE_128_BITS];

	uint32 err_code; /* Real err code, initially used for RPROT_ERR_INCOMPATIBLE_WEP */
} RegSM;

RegSM *reg_sm_new(void *g_mc);
void reg_sm_delete(RegSM *r);
uint32 reg_sm_initsm(RegSM *r, DevInfo *p_enrolleeInfo, bool enableLocalSM, bool enablePassthru);
uint32 reg_sm_step(RegSM *r, uint32 msgLen, uint8 *p_msg, uint8 *outbuffer, uint32 *out_len);
uint32 reg_sm_get_devinfo_step(RegSM *r); /* brcm */
void reg_sm_restartsm(RegSM *r);
uint32 reg_sm_handleMessage(RegSM *r, BufferObj *msg, BufferObj *outmsg);

typedef struct {
	RegData *reg_info;

	uint32 m_m2dStatus;

	/* The peer's encrypted settings will be stored here */
	void *mp_peerEncrSettings;
	void *g_mc;

	uint32 err_code; /* Real err code, initially used for RPROT_ERR_INCOMPATIBLE_WEP */
} EnrSM;

uint32 enr_sm_step(EnrSM *e, uint32 msgLen, uint8 *p_msg, uint8 *outbuffer, uint32 *out_len);
EnrSM * enr_sm_new(void *g_mc);
void enr_sm_delete(EnrSM *e);
uint32 enr_sm_initializeSM(EnrSM *e, DevInfo *dev_info);
void enr_sm_restartSM(EnrSM *e);
uint32 enr_sm_handleMessage(EnrSM *e, BufferObj *msg, BufferObj *outmsg);

#ifdef __cplusplus
}
#endif


#endif /* __STATEMACHINE_H__ */
