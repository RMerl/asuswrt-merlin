/*
 * WPS Enrollee OS layer
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_enr_osl.h 345516 2012-07-18 07:56:06Z $
 */

#ifndef __OSL_H__
#define __OSL_H__

/* Opaque types */
struct _wps_credentials;

int wps_osl_get_ifname(char *ifname);
int wps_osl_set_ifname(char *ifname);
int wps_osl_get_mac(uint8 *mac);
int wps_osl_init(char *);
void wps_osl_deinit(void);
void wps_osl_cleanup(void);
unsigned long get_current_time(void);
uint32 wpsenr_eapol_validate(char* buf, uint32* len);
uint32 wait_for_eapol_packet(char* buf, uint32* len, uint32 timeout);
uint32 send_eapol_packet(char *packet, uint32 len);

uint32 poll_eapol_packet(char *buf, uint32 *len);
bool wps_osl_create_profile(const struct _wps_credentials *credentials);


#endif /* __OSL_H__ */
