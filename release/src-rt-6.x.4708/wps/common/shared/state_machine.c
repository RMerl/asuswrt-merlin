/*
 * WPS State machine
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: state_machine.c 383924 2013-02-08 04:14:39Z $
 */

#include <bn.h>
#include <wps_dh.h>

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

static void
RegInfo_cleanup(RegData *reg_info)
{
	TUTRACE((TUTRACE_ERR, "RegData: Free elements in regData\n"));

	if (reg_info->enrollee &&
	    (reg_info->enrollee != reg_info->dev_info)) {
		devinfo_delete(reg_info->enrollee);
	}
	if (reg_info->registrar &&
	    (reg_info->registrar != reg_info->dev_info)) {
		devinfo_delete(reg_info->registrar);
	}

	buffobj_del(reg_info->authKey);
	buffobj_del(reg_info->keyWrapKey);
	buffobj_del(reg_info->emsk);
	buffobj_del(reg_info->x509csr);
	buffobj_del(reg_info->x509Cert);
	buffobj_del(reg_info->inMsg);
	buffobj_del(reg_info->outMsg);

	memset(reg_info, 0, sizeof(*reg_info));
}

/* public methods */
/*
 * Name        : CStateMachine
 * Description : Class constructor. Initialize member variables, set
 *                    callback function.
 * Arguments   : none
 * Return type : none
 */
RegData *
state_machine_new()
{
	RegData *reg_info;

	reg_info = (RegData *)alloc_init(sizeof(RegData));
	if (!reg_info)
		return NULL;

	memset(reg_info, 0, sizeof(*reg_info));
	return reg_info;
}

/*
 * Name        : ~CStateMachine
 * Description : Class destructor. Cleanup if necessary.
 * Arguments   : none
 * Return type : none
 */

void
state_machine_delete(RegData *reg_info)
{
	if (reg_info) {
		TUTRACE((TUTRACE_ERR, "RegData: Deleting regData\n"));

		RegInfo_cleanup(reg_info);
		free(reg_info);
	}
}

uint32
state_machine_init_sm(RegData *reg_info)
{

	RegInfo_cleanup(reg_info);

	/* Clear it */
	memset(reg_info, 0, sizeof(*reg_info));

	reg_info->e_smState = START;
	reg_info->e_lastMsgRecd = MNONE;
	reg_info->e_lastMsgSent = MNONE;
	reg_info->enrollee = NULL;
	reg_info->registrar = NULL;

	reg_info->authKey = buffobj_new();
	reg_info->keyWrapKey = buffobj_new();
	reg_info->emsk = buffobj_new();
	reg_info->x509csr = buffobj_new();
	reg_info->x509Cert = buffobj_new();
	reg_info->inMsg = buffobj_new();
	reg_info->outMsg = buffobj_new();

	if (!reg_info->authKey ||
	    !reg_info->keyWrapKey ||
	    !reg_info->emsk ||
	    !reg_info->x509csr ||
	    !reg_info->x509Cert ||
	    !reg_info->inMsg ||
	    !reg_info->outMsg) {
		return WPS_ERR_OUTOFMEMORY;
	}

	reg_info->initialized = true;

	return WPS_SUCCESS;
}

uint32
state_machine_send_ack(RegData *reg_info, BufferObj *outmsg, uint8 *regNonce)
{
	uint32 err;

	err = reg_proto_create_ack(reg_info, outmsg, regNonce);
	if (WPS_SUCCESS != err) {
		TUTRACE((TUTRACE_ERR, "SM: reg_proto_create_ack generated an "
			"error: %d\n", err));
		return err;
	}

	return WPS_SEND_MSG_CONT;
}

uint32
state_machine_send_nack(RegData *reg_info, uint16 configError, BufferObj *outmsg, uint8 *regNonce)
{
	uint32 err;

	err = reg_proto_create_nack(reg_info, outmsg, configError, regNonce);
	if (WPS_SUCCESS != err) {
		TUTRACE((TUTRACE_ERR, "SM: reg_proto_create_nack generated an "
			"error: %d\n", err));
		return err;
	}

	return WPS_SEND_MSG_CONT;
}
