/*
 * WPS nfc
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

#ifndef __WPS_NFC_H__
#define __WPS_NFC_H__

#include <wps_devinfo.h>

int wps_nfc_init();
void wps_nfc_deinit();
int wps_nfc_process_msg(char *nfcmsg, int nfcmsg_len);
wps_hndl_t *wps_nfc_check(char *nfcmsg, int *nfcmsg_len);
int wps_nfc_write_cfg(char *ifname, DevInfo *info);
int wps_nfc_read_cfg(char *ifname);
int wps_nfc_write_pw(char *ifname, DevInfo *info);
int wps_nfc_read_pw(char *ifname);
int wps_nfc_ho_selector(char *ifname, DevInfo *info);
int wps_nfc_ho_requester(char *ifname);

int wps_nfc_stop(char *ifname);

uint8* wps_nfc_priv_key();
uint8* wps_nfc_pub_key_hash();
unsigned short wps_nfc_pw_id();
char* wps_nfc_dev_pw_str();
bool wps_nfc_is_idle();

#endif	/* __WPS_EAP_H__ */
