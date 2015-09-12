/*
 * Portability
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: portability.h 383928 2013-02-08 04:15:06Z $
 */

#ifndef _WPS_PORTAB_
#define _WPS_PORTAB_

#include <wpstypes.h>

#ifndef __cplusplus
#include "stdio.h"
#include "stdlib.h"
#include "typedefs.h"

#ifdef true
#undef true
#endif
#define true 1

#ifdef false
#undef false
#endif
#define false 0

char * alloc_init(int size);
#define new(a) (a *)alloc_init(sizeof(a))
#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif

#include "string.h"

/* Byte swapping functions. To be implemented by application. */
uint32 WpsHtonl(uint32 intlong);
uint16 WpsHtons(uint16 intshort);
uint32 WpsHtonlPtr(uint8 * in, uint8 * out);
uint16 WpsHtonsPtr(uint8 * in, uint8 * out);
uint32 WpsNtohl(uint8* intlong);
uint16 WpsNtohs(uint8 * intshort);
void WpsSleep(uint32 seconds);
void WpsSleepMs(uint32 ms);

typedef struct {
	char ssid[SIZE_SSID_LENGTH];
	uint32 ssidLen;
	char keyMgmt[SIZE_20_BYTES+1];
	char nwKey[SIZE_64_BYTES+1];
	uint32 nwKeyLen;
	uint32 encrType;
	uint16 wepIndex;
	bool nwKeyShareable;
} WpsEnrCred;

typedef struct {
	uint8  pub_key_hash[SIZE_160_BITS];
	uint16 devPwdId;
	uint8  pin[SIZE_32_BYTES];
	int    pin_len;
} WpsOobDevPw;


bool wps_isSRPBC(IN uint8 *p_data, IN uint32 len);
bool wps_isWPSS(IN uint8 *p_data, IN uint32 len);
bool wl_is_wps_ie(uint8 **wpaie, uint8 **tlvs, uint *tlvs_len);
bool is_wps_ies(uint8* cp, uint len);
bool is_SelectReg_PBC(uint8* cp, uint len);
bool is_ConfiguredState(uint8* cp, uint len);
uint8 *wps_parse_tlvs(uint8 *tlv_buf, int buflen, uint key);
int set_mac_address(char *mac_string, char *mac_bin);
void wps_set_reg_result(uint8 val);
bool wps_isSELR(IN uint8 *p_data, IN uint32 len);

bool wps_isVersion2(uint8 *p_data, uint32 len, uint8 *version2, uint8 *macs);
bool is_wpsVersion2(uint8* cp, uint len, uint8 *version2, uint8 *macs);
bool wps_isAuthorizedMAC(IN uint8 *p_data, IN uint32 len, IN uint8 *mac);
bool is_AuthorizedMAC(uint8* cp, uint len, uint8 *mac);

#ifdef __cplusplus
}
#endif

#endif /* _WPS_PORTAB_ */
