/*
 * WPS WiFi Invite (WFI) header file
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_wfi.h 383928 2013-02-08 04:15:06Z $
 */

#ifndef _WPS_WFI_H_
#define _WPS_WFI_H_

#pragma pack(push, 1)
#define BWL_PRE_PACKED_STRUCT
#define BWL_POST_PACKED_STRUCT

typedef BWL_PRE_PACKED_STRUCT struct {
	uint8 vndr_id[3]; /* Vendor OUI: 00 11 3d */
	union {
		uint8 vndr_data[1]; /* Friendly name, variable length, non-null-terminated */
		uint8 type; /* Friendly name, variable length, non-null-terminated */
	};
} BWL_POST_PACKED_STRUCT wps_vndr_ext_t;
#define BRCM_VENDOR_ID			"\x00\x11\x3d"

typedef BWL_PRE_PACKED_STRUCT struct {
	uint8 type;        /* type of this IE = WFI_IE_TYPE. Will be defined in wlioctl.h */
	uint8 version;     /* WFI version = 1 */
	uint8 cap;         /* WFI capabilities. HOTSPOT = 0x1, Bits 1-7 : Reserved */
	uint32 pin;        /* PIN to be used */
	uint8 mac_addr[6]; /* MAC address of the STA requesting WFI (ProbeReq) or
	                   *  MAC address of the STA for whome the WFI is destined (ProbeRsp).
	                   */
	uint8 fname_len;   /* Length of the BDD friendly name (of STA in case of ProbeReq,
	                   *  of AP in case of ProbeRsp).
	                   */
	uint8 fname[1];    /* BDD Friendly name, variable length, non-null-terminated */
} BWL_POST_PACKED_STRUCT brcm_wfi_ie_t;
#pragma pack(pop)

#define WPS_WFI_VENDOR_EXT_LEN		(sizeof(brcm_wfi_ie_t) + sizeof(wps_vndr_ext_t) + 32)

#define WPS_WFI_TYPE				1
#define WPS_WFI_HOTSPOT			0x01	/* Hotspot mode when WPS is not required */

#if defined(BCMWPSAP)
/*
*  WiFi Invite Common code, implemented in wps_wfi.c:
*/

extern int wps_wfi_process_prb_req(uint8 *pcWPSIE,
	int siIELen,
	uint8 *pcAddr,
	char *pcIFname);
/*
*  Return < 0 if error; >= 0 if OK.
*  pcWPSIE: Input, WPS IE content.
*  siIELen: Input, WPS IE total length.
*  pcAddr: Input, MAC Address of the STA sending ProbeReq.
*  pcIFname: Input, Interface where the ProbeReq comes from.
*/

extern int wps_wfi_process_sta_ind(uint8 *ptData,
	int siLen,
	uint8 *ptAddr,
	uint8 *ptIFName,
	int biJoin);
/*
*  Return < 0 if error; >= 0 if OK.
*  ptData: Input, event data.
*  siLen: Input, event data total length.
*  pcAddr: Input, MAC Address of the STA sending ProbeReq.
*  pcIFname: Input, Interface where the ProbeReq comes from.
*  biJoin: 0: leaving event; !0: joining event.
*/

extern int wps_wfi_init(void);
/*
*  Initialize WFI feature.
*  Return < 0 if error; >= 0 if OK.
*/

extern int wps_wfi_rejected(void);
/*
*  Call when EAP-NAK is received.
*  Return < 0 if error; >= 0 if OK.
*/

extern int wps_wfi_cleanup(void);
/*
*  Deinitialize WFI feature.
*  Return < 0 if error; >= 0 if OK.
*/

extern void wps_wfi_check(void);
/*
*  Check the WFI operation.
*/
#endif /* BCMWPSAP */

#endif /* _WPS_WFI_H_ */
