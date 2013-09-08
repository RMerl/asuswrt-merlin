/*
 * Broadcom WPS module (for libupnp), WFADevice.c
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: WFADevice.c 270398 2011-07-04 06:31:51Z $
 */
#include <upnp.h>
#include <WFADevice.h>
#include <wps_upnp.h>

extern UPNP_SCBRCHAIN * get_subscriber_chain(UPNP_CONTEXT *context, UPNP_SERVICE *service);

static char libupnp_wfa_msg[WPS_EAPD_READ_MAX_LEN];

/* Perform the SetSelectedRegistrar action */
int
wfa_SetSelectedRegistrar(UPNP_CONTEXT *context,	UPNP_TLV *NewMessage)
{
	int rc;
	UPNP_WPS_CMD *cmd = (UPNP_WPS_CMD *)libupnp_wfa_msg;

	/* Cmd */
	cmd->type = UPNP_WPS_TYPE_SSR;
	strcpy((char *)cmd->dst_addr, inet_ntoa(context->dst_addr.sin_addr));
	cmd->length = NewMessage->len;

	/* Data */
	memcpy(cmd->data, NewMessage->val.str, NewMessage->len);

	/* Direct call wps_libupnp_ProcessMsg */
	rc = wps_libupnp_ProcessMsg(context->focus_ifp->ifname, libupnp_wfa_msg,
		UPNP_WPS_CMD_SIZE + NewMessage->len);
	return 0;
}

/* Perform the PutMessage action */
int
wfa_PutMessage(UPNP_CONTEXT *context, UPNP_TLV *NewInMessage, UPNP_TLV *NewOutMessage)
{
	int rc;
	char *info = "";
	int info_len = 0;
	UPNP_WPS_CMD *cmd = (UPNP_WPS_CMD *)libupnp_wfa_msg;

	/* Cmd */
	cmd->type = UPNP_WPS_TYPE_PMR;
	strcpy((char *)cmd->dst_addr, inet_ntoa(context->dst_addr.sin_addr));
	cmd->length = NewInMessage->len;

	/* Data */
	memcpy(cmd->data, NewInMessage->val.str, NewInMessage->len);

	/* Direct call wps_libupnp_ProcessMsg */
	rc = wps_libupnp_ProcessMsg(context->focus_ifp->ifname, libupnp_wfa_msg,
		UPNP_WPS_CMD_SIZE + NewInMessage->len);

	/* Always check the out messge len */
	info_len = wps_libupnp_GetOutMsgLen(context->focus_ifp->ifname);
	if (info_len > 0 &&
	    (info = wps_libupnp_GetOutMsg(context->focus_ifp->ifname)) == NULL) {
		info = "";
		info_len = 0;
	}

	upnp_tlv_set_bin(NewOutMessage, (int)info, info_len);
	return 0;
}

/* Perform the GetDeviceInfo action */
int
wfa_GetDeviceInfo(UPNP_CONTEXT *context, UPNP_TLV *NewDeviceInfo)
{
	int rc;
	char *info = "";
	int info_len = 0;
	UPNP_WPS_CMD *cmd = (UPNP_WPS_CMD *)libupnp_wfa_msg;

	/* Cmd */
	cmd->type = UPNP_WPS_TYPE_GDIR;
	strcpy((char *)cmd->dst_addr, inet_ntoa(context->dst_addr.sin_addr));
	cmd->length = 0;

	/* Direct call wps_libupnp_ProcessMsg */
	rc = wps_libupnp_ProcessMsg(context->focus_ifp->ifname, libupnp_wfa_msg,
		UPNP_WPS_CMD_SIZE);

	/* Always check the out messge len */
	info_len = wps_libupnp_GetOutMsgLen(context->focus_ifp->ifname);
	if (info_len > 0 &&
	    (info = wps_libupnp_GetOutMsg(context->focus_ifp->ifname)) == NULL) {
		info = "";
		info_len = 0;
	}

	upnp_tlv_set_bin(NewDeviceInfo, (int)info, info_len);
	return 0;
}

/* Perform the PutWLANResponse action */
int
wfa_PutWLANResponse(UPNP_CONTEXT *context, UPNP_TLV *NewMessage)
{
	int rc;
	UPNP_WPS_CMD *cmd = (UPNP_WPS_CMD *)libupnp_wfa_msg;

	/* Cmd */
	cmd->type = UPNP_WPS_TYPE_PWR;
	strcpy((char *)cmd->dst_addr, inet_ntoa(context->dst_addr.sin_addr));
	cmd->length = NewMessage->len;

	/* Data */
	memcpy(cmd->data, NewMessage->val.str, NewMessage->len);

	/* Direct call wps_libupnp_ProcessMsg */
	rc = wps_libupnp_ProcessMsg(context->focus_ifp->ifname, libupnp_wfa_msg,
		UPNP_WPS_CMD_SIZE + NewMessage->len);
	return 0;
}

/* Close wfa */
static void
wfa_free(UPNP_CONTEXT *context)
{
	return;
}

/* Open wfa */
int
wfa_init(UPNP_CONTEXT *context)
{
	return 0;
}

static bool
is_duplicate_subscriber(UPNP_CONTEXT *context, UPNP_SERVICE *service,
	UPNP_SCBRCHAIN *scbrchain, UPNP_SUBSCRIBER *disconnected_subscriber)
{
	int count = 0;
	UPNP_SUBSCRIBER *subscriber;

	subscriber = scbrchain->subscriberlist;
	while (subscriber) {
		if (subscriber->ipaddr.s_addr == disconnected_subscriber->ipaddr.s_addr)
			count++;
		subscriber = subscriber->next;
	}

	if (count >= 2)
		return TRUE;
	else
		return FALSE;
}

/*
 * WARNNING: PLEASE IMPLEMENT YOUR CODES AFTER 
 *          "<< USER CODE START >>"
 * AND DON'T REMOVE TAG :
 *          "<< AUTO GENERATED FUNCTION: "
 *          ">> AUTO GENERATED FUNCTION"
 *          "<< USER CODE START >>"
 */

/* << AUTO GENERATED FUNCTION: WFADevice_open() */
int
WFADevice_open(UPNP_CONTEXT *context)
{
	/* << USER CODE START >> */
	int retries;

	/* Setup default gena connect retries for WFA */
	retries = WFADevice.gena_connect_retries;
	if (retries < 1 || retries > 5)
		retries = 3;

	WFADevice.gena_connect_retries = retries;

	if (wfa_init(context) != 0)
		return -1;

	return 0;
}
/* >> AUTO GENERATED FUNCTION */

/* << AUTO GENERATED FUNCTION: WFADevice_close() */
int
WFADevice_close(UPNP_CONTEXT *context)
{
	/* << USER CODE START >> */
	wfa_free(context);
	return 0;
}
/* >> AUTO GENERATED FUNCTION */

/* << AUTO GENERATED FUNCTION: WFADevice_timeout() */
int
WFADevice_timeout(UPNP_CONTEXT *context, time_t now)
{
	/* << USER CODE START >> */
	return 0;
}
/* >> AUTO GENERATED FUNCTION */

/* << AUTO GENERATED FUNCTION: WFADevice_notify() */
int
WFADevice_notify(UPNP_CONTEXT *context, UPNP_SERVICE *service,
	UPNP_SUBSCRIBER *subscriber, int notify)
{
	/* << USER CODE START >> */
	int rc;
	UPNP_SCBRCHAIN	*scbrchain;
	UPNP_WPS_CMD *cmd = (UPNP_WPS_CMD *)libupnp_wfa_msg;

	if (notify == DEVICE_NOTIFY_CONNECT_FAILED) {
		return 0;

		scbrchain = get_subscriber_chain(context, service);

		/* Qualify duplicate subscriber */
		if (scbrchain && subscriber &&
		    is_duplicate_subscriber(context, service, scbrchain, subscriber)) {
			/* Just delete disconnected subscriber  */
			delete_subscriber(scbrchain, subscriber);
			return 0;
		}

		/* Cmd */
		cmd->type = UPNP_WPS_TYPE_DISCONNECT;
		strcpy((char *)cmd->dst_addr, inet_ntoa(context->dst_addr.sin_addr));
		cmd->length = 0;

		/* Direct call wps_libupnp_ProcessMsg */
		rc = wps_libupnp_ProcessMsg(context->focus_ifp->ifname, libupnp_wfa_msg,
			UPNP_WPS_CMD_SIZE);

		/* Delete disconnected subscriber */
		if (scbrchain && subscriber)
			delete_subscriber(scbrchain, subscriber);

		return 0;
	}

	return 0;
}
/* >> AUTO GENERATED FUNCTION */

/* << AUTO GENERATED FUNCTION: WFADevice_scbrchk() */
int
WFADevice_scbrchk(UPNP_CONTEXT *context, UPNP_SERVICE *service,
	UPNP_SUBSCRIBER *subscriber, struct in_addr ipaddr, unsigned short port, char *uri)
{
	/* << USER CODE START >> */

	if (subscriber->ipaddr.s_addr == ipaddr.s_addr && subscriber->port == port)
		return 1;

	return 0;
}
/* >> AUTO GENERATED FUNCTION */
