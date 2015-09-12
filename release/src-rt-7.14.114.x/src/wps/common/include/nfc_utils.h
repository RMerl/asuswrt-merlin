/*
 * WPS NFC share utility header file
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: $
 */

#ifndef _WPS_NFC_UTILS_H_
#define _WPS_NFC_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <wps_devinfo.h>

/*
 * implemented in nfc_utils.c
 */
uint32 nfc_utils_build_cfg(DevInfo *info, uint8 *buf, uint *buflen);
uint32 nfc_utils_build_pw(DevInfo *info, uint8 *buf, uint *buflen);
uint32 nfc_utils_parse_cfg(WpsEnrCred *cred, uint8 wsp_version2, uint8 *buf, uint buflen);
uint32 nfc_utils_parse_pw(WpsOobDevPw *devpw, uint8 wsp_version2, uint8 *buf, uint buflen);

#ifdef __cplusplus
}
#endif

#endif /* _WPS_NFC_UTILS_H_ */
