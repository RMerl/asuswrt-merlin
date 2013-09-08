/*
 * Broadcom WPS module (for libupnp), WFADevice.h
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: WFADevice.h 270398 2011-07-04 06:31:51Z $
 */

#ifndef __WFADEVICE_H__
#define __WFADEVICE_H__

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */


typedef	struct wfa_net_s {
	char *lan_ifname;
} WFA_NET;

/*
 * Function protocol type
 */
int wfa_SetSelectedRegistrar(UPNP_CONTEXT *context, UPNP_TLV *NewMessage);
int wfa_PutMessage(UPNP_CONTEXT *context, UPNP_TLV *NewInMessage, UPNP_TLV *NewOutMessage);
int wfa_GetDeviceInfo(UPNP_CONTEXT *context, UPNP_TLV *NewDeviceInfo);
int wfa_PutWLANResponse(UPNP_CONTEXT *context, UPNP_TLV *NewMessage);

/* << TABLE BEGIN */
/*
 * WARNNING: DON'T MODIFY THE FOLLOWING TABLES
 * AND DON'T REMOVE TAG :
 *          "<< TABLE BEGIN"
 *          ">> TABLE END"
 */

extern UPNP_DEVICE WFADevice;

int WFADevice_open(UPNP_CONTEXT *context);
int WFADevice_close(UPNP_CONTEXT *context);
int WFADevice_timeout(UPNP_CONTEXT *context, time_t now);
int WFADevice_notify(UPNP_CONTEXT *context, UPNP_SERVICE *service,
	UPNP_SUBSCRIBER *subscriber, int notify);
int WFADevice_scbrchk(UPNP_CONTEXT *context, UPNP_SERVICE *service,
	UPNP_SUBSCRIBER *subscriber, struct in_addr ipaddr, unsigned short port, char *uri);
/* >> TABLE END */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WFADEVICE_H__ */
