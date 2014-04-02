/*
 * WPS OSL include
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_osl.h 279189 2011-08-23 10:14:21Z $
 */
#ifndef __WPS_OSL_H__
#define __WPS_OSL_H__

#include <typedefs.h>
#include <security_ipc.h>

int wps_osl_set_conf(char *config_name, char *config_string);
int wps_osl_arp_get(char *client_ip, unsigned char mac[6]);

/* WPS EAP osl functions */
int wps_osl_eap_handle_init();
void wps_osl_eap_handle_deinit(int handle);
int wps_eap_get_handle();

/* WPS UI osl functions */
int wps_osl_ui_handle_init();
void wps_osl_ui_handle_deinit(int handle);
int wps_osl_send_uimsg(wps_hndl_t *hndl, char *buf, int len);

/* Handle all packets */
wps_hndl_t *wps_osl_wait_for_all_packets(char *buf, unsigned int *buflen, wps_hndl_t *hndl_list);

void WPS_HexDumpAscii(const char *title, int upnp_type, const unsigned char *buf,
	unsigned int len);

#endif /* __WPS_OSL_H__ */
