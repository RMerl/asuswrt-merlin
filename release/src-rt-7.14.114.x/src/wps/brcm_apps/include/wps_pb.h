/*
 * WPS push button
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_pb.h 383928 2013-02-08 04:15:06Z $
 */

#ifndef __WPS_PB_H__
#define __WPS_PB_H__

#include <wps.h>

#define PBC_OVERLAP_CNT			2
#define WPS_PB_SELECTING_MAX_TIMEOUT	10	/* second */

typedef struct {
	unsigned char	mac[6];
	unsigned int	last_time;
	unsigned char	uuid[16];
} PBC_STA_INFO;

enum {
	WPS_PB_STATE_INIT = 0,
	WPS_PB_STATE_CONFIRM,
	WPS_PB_STATE_SELECTING
} WPS_PB_STATE_T;

int wps_pb_check_pushtime(unsigned long time);
void wps_pb_update_pushtime(char *mac, uint8 *uuid);
void wps_pb_get_uuids(uint8 *buf, int len);
void wps_pb_clear_sta(unsigned char *mac);
wps_hndl_t *wps_pb_check(char *buf, int *buflen);
int wps_pb_find_pbc_ap(char * bssid, char *ssid, uint8 *wsec);
int wps_pb_init();
int wps_pb_deinit();
void wps_pb_reset();
void wps_pb_timeout(int session_opened);
int wps_pb_state_reset();

#endif	/* __WPS_PB_H__ */
