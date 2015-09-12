/*
 * WPS station
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_sta.h 297187 2011-11-18 04:09:28Z $
 */
#ifndef __WPS_STA_H__
#define __WPS_STA_H__

#define WPS_MAX_AP_SCAN_LIST_LEN 50
#define PBC_WALK_TIME 120

wps_ap_list_info_t *wps_get_ap_list(void);
wps_ap_list_info_t *create_aplist(void);
int add_wps_ie(unsigned char *p_data, int length, bool pbc, bool b_wps_version2);
int rem_wps_ie(unsigned char *p_data, int length, unsigned int pktflag);
int join_network(char* ssid, uint32 wsec);
int leave_network(void);
int wps_get_bssid(char *bssid);
int wps_get_ssid(char *ssid, int *len);
int wps_get_bands(uint *band_num, uint *active_band);
int do_wpa_psk(WpsEnrCred* credential);
int join_network_with_bssid(char* ssid, uint32 wsec, char *bssid);
int do_wps_scan(void);
char* get_wps_scan_results(void);

#ifdef WFA_WPS_20_TESTBED
int set_wps_ie_frag_threshold(int threshold);
int set_update_partial_ie(uint8 *updie_str, unsigned int pktflag);
#endif /* WFA_WPS_20_TESTBED */

bool wps_wl_init(void *caller_ctx, void *callback);
void wps_wl_deinit();
wps_ap_list_info_t *wps_wl_surveying(bool b_pbc, bool b_v2, bool b_add_wpsie);
bool wps_wl_join(uint8 *bssid, char *ssid, uint8 wep);


#endif /* __WPS_STA_H__ */
