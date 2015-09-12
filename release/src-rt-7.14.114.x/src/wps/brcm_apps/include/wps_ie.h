/*
 * WPS IE
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_ie.h 383928 2013-02-08 04:15:06Z $
 */

#ifndef __WPS_IE_H__
#define __WPS_IE_H__

int wps_ie_default_ssr_info(CTlvSsrIE *ssrmsg, unsigned char *authorizedMacs,
	int authorizedMacs_len, BufferObj *authorizedMacs_Obj, unsigned char *wps_uuid,
	BufferObj *uuid_R_Obj, uint8 scState);
void wps_ie_set(char *wps_ifname, CTlvSsrIE *ssrmsg);
void wps_ie_clear();

#include <bcmconfig.h>
#ifdef __CONFIG_WFI__
/*
*  Generic Vendor Extension Support:
*/
#define WPSM_VNDR_EXT_MAX_DATA_80211	(3 + 246)

int wps_vndr_ext_obj_free(void *ptToFree);
/*
*  Return: <0, the object not found; >= 0 OK.
*  ptToFree: pointer to vendor object.
*/

void *wps_vndr_ext_obj_alloc(int siBufferSize, char *pcOSName);
/*
*  Return the Vendor Extension Object Pointer allocated with buffer
*     size siBufferSize. NULL if BufferSize is too big or malloc error.
*  siBufferSize: < 0, the WPSM_VNDR_EXT_MAX_DATA_80211 will be allocated.
*  pcOSName: pointer to char that contains OS name of an interface
*     that this vendor object belongs to.
*/

int wps_vndr_ext_obj_copy(void *ptObj, char *pcData, int siLen);
/*
*  Return <0, Error; >0, number of bytes copied.
*  pcData: pointer to char array containing input data.
*  siLen: Number of data to copy.
*/

int wps_vndr_ext_obj_set_mode(void *ptObj, int siType, int boolActivate);
/*
*  Return < 0 if error; >=0 if OK.
*  siType: WPS_IE_TYPE_SET_BEACON_IE or WPS_IE_TYPE_SET_PROBE_RESPONSE_IE.
*  boolActivate: 0, set but do not activate it; 1, set and activate it.
*/
#endif /* __CONFIG_WFI__ */

#endif	/* __WPS_IE_H__ */
