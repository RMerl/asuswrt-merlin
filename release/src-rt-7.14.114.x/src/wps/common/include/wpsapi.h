/*
 * WPS API
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpsapi.h 323636 2012-03-26 10:29:39Z $
 */

#ifndef _WPSAPI_
#define _WPSAPI_

#ifdef __cplusplus
extern "C" {
#endif

#include <reg_protomsg.h>
#include <wps_devinfo.h>

typedef struct {
	void *bcmwps;

	RegSM *mp_regSM;
	EnrSM *mp_enrSM;

	bool mb_initialized;
	bool mb_stackStarted;

	DevInfo *dev_info;

	bool b_UpnpDevGetDeviceInfo;
} WPSAPI_T;

void * wps_init(void *bcmwps, DevInfo *ap_devinfo);
uint32 wps_deinit(WPSAPI_T *g_mc);

uint32 wps_ProcessBeaconIE(char *ssid, uint8 *macAddr, uint8 *p_data, uint32 len);
uint32 wps_ProcessProbeReqIE(uint8 *macAddr, uint8 *p_data, uint32 len);
uint32 wps_ProcessProbeRespIE(uint8 *macAddr, uint8 *p_data, uint32 len);

int wps_getenrState(void *mc_dev);
int wps_getregState(void *mc_dev);

uint32 wps_sendStartMessage(void *bcmdev, TRANSPORT_TYPE trType);
int wps_get_upnpDevSSR(WPSAPI_T *g_mc, void *p_cbData, uint32 length, CTlvSsrIE *ssrmsg);
int wps_upnpDevSSR(WPSAPI_T *g_mc, CTlvSsrIE *ssrmsg);

int wps_getProcessStates();
void wps_setProcessStates(int state);
void wps_setStaDevName(unsigned char *str);
void wps_setPinFailInfo(uint8 *mac, char *name, char *state);

#ifdef __cplusplus
}
#endif


#endif /* _WPSAPI_ */
