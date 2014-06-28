/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * ASUS Home Gateway Reference Design
 * Web Page Configuration Support Routines
 *
 * Copyright 2014, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of ASUSTeK Inc..
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bcmnvram.h>
#include <wlioctl.h>

#include <httpd.h>

#include <shutils.h>
#include <shared.h>

#include "web-qtn.h"

#define MAX_RETRY_TIMES 30
#define WIFINAME "wifi0"

static int s_c_rpc_use_udp = 0;

extern uint8 wf_chspec_ctlchan(chanspec_t chspec);
extern chanspec_t wf_chspec_aton(const char *a);
extern char *wl_ether_etoa(const struct ether_addr *n);
extern char *wl_vifname_qtn(int unit, int subunit);

int rpc_qcsapi_init()
{
	const char *host;
	CLIENT *clnt;
	int retry = 0;

	nvram_set("111", "222");
	/* setup RPC based on udp protocol */
	do {
		dbG("#%d attempt to create RPC connection\n", retry + 1);

		host = client_qcsapi_find_host_addr(0, NULL);
		if (!host) {
			dbG("Cannot find the host\n");
			sleep(1);
			continue;
		}

		if (!s_c_rpc_use_udp) {
			clnt = clnt_create(host, QCSAPI_PROG, QCSAPI_VERS, "tcp");
		} else {
			clnt = clnt_create(host, QCSAPI_PROG, QCSAPI_VERS, "udp");
		}

		if (clnt == NULL) {
			clnt_pcreateerror(host);
			sleep(1);
			continue;
		} else {
			client_qcsapi_set_rpcclient(clnt);
			return 0;
		}
	} while (retry++ < MAX_RETRY_TIMES);

	return -1;
}

int rpc_qcsapi_set_SSID(const char *ifname, const char *ssid)
{
	int ret;

	ret = qcsapi_wifi_set_SSID(ifname, ssid);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_SSID %s error, return: %d\n", ifname, ret);
		return ret;
	}
	dbG("Set SSID of interface %s as: %s\n", ifname, ssid);

	return 0;
}

int rpc_qcsapi_set_SSID_broadcast(const char *ifname, const char *option)
{
	int ret;
	int OPTION = 1 - atoi(option);

	ret = qcsapi_wifi_set_option(ifname, qcsapi_SSID_broadcast, OPTION);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_option::SSID_broadcast %s error, return: %d\n", ifname, ret);
		return ret;
	}
	dbG("Set Broadcast SSID of interface %s as: %s\n", ifname, OPTION ? "TRUE" : "FALSE");

	return 0;
}

int rpc_qcsapi_set_vht(const char *mode)
{
	int ret;
	int VHT;

	switch (atoi(mode))
	{
		case 0:
			VHT = 1;
			break;
		default:
			VHT = 0;
			break;
	}

	ret = qcsapi_wifi_set_vht(WIFINAME, VHT);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_vht %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}
	dbG("Set wireless mode of interface %s as: %s\n", WIFINAME, VHT ? "11ac" : "11n");

	return 0;
}

int rpc_qcsapi_set_bw(const char *bw)
{
	int ret;
	int BW = 20;

	switch (atoi(bw))
	{
		case 1:
			BW = 20;
			break;
		case 2:
			BW = 40;
			break;
		case 0:
		case 3:
			BW = 80;
			break;
	}

	ret = qcsapi_wifi_set_bw(WIFINAME, BW);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_bw %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}
	dbG("Set bw of interface %s as: %d MHz\n", WIFINAME, BW);

	return 0;
}

int rpc_qcsapi_set_channel(const char *chspec_buf)
{
	int ret;
	int channel = 0;

	channel = wf_chspec_ctlchan(wf_chspec_aton(chspec_buf));

	ret = qcsapi_wifi_set_channel(WIFINAME, channel);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_channel %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}
	dbG("Set channel of interface %s as: %d\n", WIFINAME, channel);

	return 0;
}

int rpc_qcsapi_set_beacon_type(const char *ifname, const char *auth_mode)
{
	int ret;
	char *p_new_beacon = NULL;

	if (!strcmp(auth_mode, "open"))
		p_new_beacon = strdup("Basic");
	else if (!strcmp(auth_mode, "psk"))
		p_new_beacon = strdup("WPA");
	else if (!strcmp(auth_mode, "psk2"))
		p_new_beacon = strdup("11i");
	else if (!strcmp(auth_mode, "pskpsk2"))
		p_new_beacon = strdup("WPAand11i");
	else
		p_new_beacon = strdup("Basic");

	ret = qcsapi_wifi_set_beacon_type(ifname, p_new_beacon);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_beacon_type %s error, return: %d\n", ifname, ret);
		return ret;
	}
	dbG("Set beacon type of interface %s as: %s\n", ifname, p_new_beacon);

	if (p_new_beacon) free(p_new_beacon);

	return 0;
}

int rpc_qcsapi_set_WPA_encryption_modes(const char *ifname, const char *crypto)
{
	int ret;
	string_32 encryption_modes;

	if (!strcmp(crypto, "tkip"))
		strcpy(encryption_modes, "TKIPEncryption");
	else if (!strcmp(crypto, "aes"))
		strcpy(encryption_modes, "AESEncryption");
	else if (!strcmp(crypto, "tkip+aes"))
		strcpy(encryption_modes, "TKIPandAESEncryption");
	else
		strcpy(encryption_modes, "AESEncryption");

	ret = qcsapi_wifi_set_WPA_encryption_modes(ifname, encryption_modes);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_WPA_encryption_modes %s error, return: %d\n", ifname, ret);
		return ret;
	}
	dbG("Set WPA encryption mode of interface %s as: %s\n", ifname, encryption_modes);

	return 0;
}

int rpc_qcsapi_set_key_passphrase(const char *ifname, const char *wpa_psk)
{
	int ret;

	ret = qcsapi_wifi_set_key_passphrase(ifname, 0, wpa_psk);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_key_passphrase %s error, return: %d\n", ifname, ret);

		ret = qcsapi_wifi_set_pre_shared_key(ifname, 0, wpa_psk);
		if (ret < 0)
			dbG("Qcsapi qcsapi_wifi_set_pre_shared_key %s error, return: %d\n", ifname, ret);

		return ret;
	}
	dbG("Set WPA preshared key of interface %s as: %s\n", ifname, wpa_psk);

	return 0;
}

int rpc_qcsapi_set_dtim(const char *dtim)
{
	int ret;
	int DTIM = atoi(dtim);

	ret = qcsapi_wifi_set_dtim(WIFINAME, DTIM);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_dtim %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}
	dbG("Set dtim of interface %s as: %d\n", WIFINAME, DTIM);

	return 0;
}

int rpc_qcsapi_set_beacon_interval(const char *beacon_interval)
{
	int ret;
	int BCN = atoi(beacon_interval);

	ret = qcsapi_wifi_set_beacon_interval(WIFINAME, BCN);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_beacon_interval %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}
	dbG("Set beacon_interval of interface %s as: %d\n", WIFINAME, BCN);

	return 0;
}

int rpc_qcsapi_set_mac_address_filtering(const char *ifname, const char *mac_address_filtering)
{
	int ret;
	qcsapi_mac_address_filtering MAF;
	qcsapi_mac_address_filtering orig_mac_address_filtering;

	ret = rpc_qcsapi_get_mac_address_filtering(ifname, &orig_mac_address_filtering);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_mac_address_filtering %s error, return: %d\n", ifname, ret);
		return ret;
	}
	dbG("Original mac_address_filtering setting of interface %s: %d\n", ifname, orig_mac_address_filtering);

	if (!strcmp(mac_address_filtering, "disabled"))
		MAF = qcsapi_disable_mac_address_filtering;
	else if (!strcmp(mac_address_filtering, "deny"))
		MAF = qcsapi_accept_mac_address_unless_denied;
	else if (!strcmp(mac_address_filtering, "allow"))
		MAF = qcsapi_deny_mac_address_unless_authorized;
	else
		MAF = qcsapi_disable_mac_address_filtering;

	ret = qcsapi_wifi_set_mac_address_filtering(ifname, MAF);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_mac_address_filtering %s error, return: %d\n", ifname, ret);
		return ret;
	}
	dbG("Set mac_address_filtering of interface %s as: %d (%s)\n", ifname, MAF, mac_address_filtering);

	if ((MAF != orig_mac_address_filtering) &&
		(MAF != qcsapi_disable_mac_address_filtering))
		rpc_qcsapi_set_wlmaclist(ifname);

	return 0;
}

void rpc_update_macmode(const char *mac_address_filtering)
{
	int ret;
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
	int i, unit = 1;

	ret = rpc_qcsapi_set_mac_address_filtering(WIFINAME, mac_address_filtering);
	if (ret < 0) {
		dbG("rpc_qcsapi_set_mac_address_filtering %s error, return: %d\n", WIFINAME, ret);
	}

	for (i = 1; i < 4; i++)
	{
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, i);

		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
		{
			ret = rpc_qcsapi_set_mac_address_filtering(wl_vifname_qtn(unit, i), mac_address_filtering);
			if (ret < 0)
				dbG("rpc_qcsapi_set_mac_address_filtering %s error, return: %d\n", wl_vifname_qtn(unit, i), ret);
		}
	}
}

int rpc_qcsapi_authorize_mac_address(const char *ifname, const char *macaddr)
{
	int ret;
	qcsapi_mac_addr address_to_authorize;

	ether_atoe(macaddr, address_to_authorize);
	ret = qcsapi_wifi_authorize_mac_address(ifname, address_to_authorize);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_authorize_mac_address %s error, return: %d\n", ifname, ret);
		return ret;
	}
//	dbG("authorize MAC addresss of interface %s: %s\n", ifname, macaddr);

	return 0;
}

int rpc_qcsapi_deny_mac_address(const char *ifname, const char *macaddr)
{
	int ret;
	qcsapi_mac_addr address_to_deny;

	ether_atoe(macaddr, address_to_deny);
	ret = qcsapi_wifi_deny_mac_address(ifname, address_to_deny);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_deny_mac_address %s error, return: %d\n", ifname, ret);
		return ret;
	}
//	dbG("deny MAC addresss of interface %s: %s\n", ifname, macaddr);

	return 0;
}

int rpc_qcsapi_wds_set_psk(const char *ifname, const char *macaddr, const char *wpa_psk)
{
	int ret;
	qcsapi_mac_addr peer_address;

	ether_atoe(macaddr, peer_address);
	ret = qcsapi_wds_set_psk(WIFINAME, peer_address, wpa_psk);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wds_set_psk %s error, return: %d\n", ifname, ret);
		return ret;
	}
	dbG("remove WDS Peer of interface %s: %s\n", ifname, macaddr);

	return 0;
}

int rpc_qcsapi_get_SSID(const char *ifname, qcsapi_SSID *ssid)
{
	int ret;

	ret = qcsapi_wifi_get_SSID(ifname, (char *) ssid);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_SSID %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_SSID_broadcast(const char *ifname, int *p_current_option)
{
	int ret;

	ret = qcsapi_wifi_get_option(ifname, qcsapi_SSID_broadcast, p_current_option);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_option::SSID_broadcast %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_vht(qcsapi_unsigned_int *vht)
{
	int ret;

	ret = qcsapi_wifi_get_vht(WIFINAME, vht);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_vht %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_bw(qcsapi_unsigned_int *p_bw)
{
	int ret;

	ret = qcsapi_wifi_get_bw(WIFINAME, p_bw);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_bw %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_channel(qcsapi_unsigned_int *p_channel)
{
	int ret;

	ret = qcsapi_wifi_get_channel(WIFINAME, p_channel);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_channel %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_channel_list(string_1024* list_of_channels)
{
	int ret;

	ret = qcsapi_wifi_get_list_channels(WIFINAME, *list_of_channels);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_list_channels %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_beacon_type(const char *ifname, char *p_current_beacon)
{
	int ret;

	ret = qcsapi_wifi_get_beacon_type(ifname, p_current_beacon);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_beacon_type %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_WPA_encryption_modes(const char *ifname, char *p_current_encryption_mode)
{
	int ret;

	ret = qcsapi_wifi_get_WPA_encryption_modes(ifname, p_current_encryption_mode);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_WPA_encryption_modes %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_key_passphrase(const char *ifname, char *p_current_key_passphrase)
{
	int ret;

	ret = qcsapi_wifi_get_key_passphrase(ifname, 0, p_current_key_passphrase);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_key_passphrase %s error, return: %d\n", ifname, ret);

		ret = qcsapi_wifi_get_pre_shared_key(ifname, 0, p_current_key_passphrase);
		if (ret < 0)
			dbG("Qcsapi qcsapi_wifi_get_pre_shared_key %s error, return: %d\n", ifname, ret);

		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_dtim(qcsapi_unsigned_int *p_dtim)
{
	int ret;

	ret = qcsapi_wifi_get_dtim(WIFINAME, p_dtim);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_dtim %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_beacon_interval(qcsapi_unsigned_int *p_beacon_interval)
{
	int ret;

	ret = qcsapi_wifi_get_beacon_interval(WIFINAME, p_beacon_interval);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_beacon_interval %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_mac_address_filtering(const char* ifname, qcsapi_mac_address_filtering *p_mac_address_filtering)
{
	int ret;

	ret = qcsapi_wifi_get_mac_address_filtering(ifname, p_mac_address_filtering);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_mac_address_filtering %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_authorized_mac_addresses(const char *ifname, char *list_mac_addresses, const unsigned int sizeof_list)
{
	int ret;

	ret = qcsapi_wifi_get_authorized_mac_addresses(ifname, list_mac_addresses, sizeof_list);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_authorized_mac_addresses %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_denied_mac_addresses(const char *ifname, char *list_mac_addresses, const unsigned int sizeof_list)
{
	int ret;

	ret = qcsapi_wifi_get_denied_mac_addresses(ifname, list_mac_addresses, sizeof_list);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_denied_mac_addresses %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_interface_get_mac_addr(const char *ifname, qcsapi_mac_addr *current_mac_addr)
{
	int ret;

	ret = qcsapi_interface_get_mac_addr(ifname, (uint8_t *) current_mac_addr);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_interface_get_mac_addr %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_wps_get_state(const char *ifname, char *wps_state, const qcsapi_unsigned_int max_len)
{
	int ret;

	ret = qcsapi_wps_get_state(ifname, wps_state, max_len);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wps_get_state %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_wifi_disable_wps(const char *ifname, int disable_wps)
{
	int ret;

	ret = qcsapi_wifi_disable_wps(ifname, disable_wps);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_disable_wps %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_wps_get_ap_pin(const char *ifname, char *wps_pin, int force_regenerate)
{
	int ret;

	ret = qcsapi_wps_get_ap_pin(ifname, wps_pin, force_regenerate);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wps_get_ap_pin %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_wps_get_configured_state(const char *ifname, char *wps_state, const qcsapi_unsigned_int max_len)
{
	int ret;

	ret = qcsapi_wps_get_configured_state(ifname, wps_state, max_len);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wps_get_configured_state %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_wps_cancel(const char *ifname)
{
	int ret;

	ret = qcsapi_wps_cancel(ifname);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wps_cancel %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_wps_registrar_report_button_press(const char *ifname)
{
	int ret;

	ret = qcsapi_wps_registrar_report_button_press(ifname);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wps_registrar_report_button_press %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_wps_registrar_report_pin(const char *ifname, const char *wps_pin)
{
	int ret;

	ret = qcsapi_wps_registrar_report_pin(ifname, wps_pin);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wps_registrar_report_pin %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_restore_default_config(int flag)
{
	int ret;

	ret = qcsapi_restore_default_config(flag);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_restore_default_config error, return: %d\n", ret);
		return ret;
	}
	dbG("QTN restore default config successfully\n");

	return 0;
}

int rpc_qcsapi_bootcfg_commit(void)
{
	int ret;

	ret = qcsapi_bootcfg_commit();
	if (ret < 0) {
		dbG("Qcsapi qcsapi_bootcfg_commit error, return: %d\n", ret);
		return ret;
	}
	dbG("QTN commit bootcfg successfully\n");

	return 0;
}

void rpc_show_config(void)
{
	int ret;
	char mac_address_filtering_mode[][8] = {"Disable", "Reject", "Accept"};

	qcsapi_SSID ssid;
	ret = rpc_qcsapi_get_SSID(WIFINAME, &ssid);
	if (ret < 0)
		dbG("rpc_qcsapi_get_SSID %s error, return: %d\n", WIFINAME, ret);
	else
		dbG("current SSID of interface %s: %s\n", WIFINAME, ssid);

	int current_option;
	ret = rpc_qcsapi_get_SSID_broadcast(WIFINAME, &current_option);
	if (ret < 0)
		dbG("rpc_qcsapi_get_SSID_broadcast %s error, return: %d\n", WIFINAME, ret);
	else
		dbG("current SSID broadcast option of interface %s: %s\n", WIFINAME, current_option ? "TRUE" : "FALSE");

	qcsapi_unsigned_int vht;
	ret = rpc_qcsapi_get_vht(&vht);
	if (ret < 0)
		dbG("rpc_qcsapi_get_vht error, return: %d\n", ret);
	else
		dbG("current wireless mode: %s\n", (unsigned int) vht ? "11ac" : "11n");

	qcsapi_unsigned_int bw;
	ret = rpc_qcsapi_get_bw(&bw);
	if (ret < 0)
		dbG("rpc_qcsapi_get_bw error, return: %d\n", ret);
	else
		dbG("current channel bandwidth: %d MHz\n", bw);

	qcsapi_unsigned_int channel;
	ret = rpc_qcsapi_get_channel(&channel);
	if (ret < 0)
		dbG("rpc_qcsapi_get_channel error, return: %d\n", ret);
	else
		dbG("current channel: %d\n", channel);

	string_1024 list_of_channels;
	ret = rpc_qcsapi_get_channel_list(&list_of_channels);
	if (ret < 0)
		dbG("rpc_qcsapi_get_channel_list error, return: %d\n", ret);
	else
		dbG("current channel list: %s\n", list_of_channels);

	string_16 current_beacon_type;
	ret = rpc_qcsapi_get_beacon_type(WIFINAME, (char *) &current_beacon_type);
	if (ret < 0)
		dbG("rpc_qcsapi_get_beacon_type %s error, return: %d\n", WIFINAME, ret);
	else
		dbG("current beacon type of interface %s: %s\n", WIFINAME, current_beacon_type);

	string_32 encryption_mode;
	ret = rpc_qcsapi_get_WPA_encryption_modes(WIFINAME, (char *) &encryption_mode);
	if (ret < 0)
		dbG("rpc_qcsapi_get_WPA_encryption_modes %s error, return: %d\n", WIFINAME, ret);
	else
		dbG("current WPA encryption mode of interface %s: %s\n", WIFINAME, encryption_mode);

	string_64 key_passphrase;
	ret = rpc_qcsapi_get_key_passphrase(WIFINAME, (char *) &key_passphrase);
	if (ret < 0)
		dbG("rpc_qcsapi_get_key_passphrase %s error, return: %d\n", WIFINAME, ret);
	else
		dbG("current WPA preshared key of interface %s: %s\n", WIFINAME, key_passphrase);

	qcsapi_unsigned_int dtim;
	ret = rpc_qcsapi_get_dtim(&dtim);
	if (ret < 0)
		dbG("rpc_qcsapi_get_dtim error, return: %d\n", ret);
	else
		dbG("current DTIM interval: %d\n", dtim);

	qcsapi_unsigned_int beacon_interval;
	ret = rpc_qcsapi_get_beacon_interval(&beacon_interval);
	if (ret < 0)
		dbG("rpc_qcsapi_get_beacon_interval error, return: %d\n", ret);
	else
		dbG("current Beacon interval: %d\n", beacon_interval);

	qcsapi_mac_address_filtering mac_address_filtering;
	ret = rpc_qcsapi_get_mac_address_filtering(WIFINAME, &mac_address_filtering);
	if (ret < 0)
		dbG("rpc_qcsapi_get_mac_address_filtering %s error, return: %d\n", WIFINAME, ret);
	else
		dbG("current MAC filter mode of interface %s: %s\n", WIFINAME, mac_address_filtering_mode[mac_address_filtering]);
}

void rpc_parse_nvram(const char *name, const char *value)
{
	if (!strcmp(name, "wl1_ssid"))
		rpc_qcsapi_set_SSID(WIFINAME, value);
	else if (!strcmp(name, "wl1_closed"))
		rpc_qcsapi_set_SSID_broadcast(WIFINAME, value);
	else if (!strcmp(name, "wl1_nmode_x"))
		rpc_qcsapi_set_vht(value);
	else if (!strcmp(name, "wl1_bw"))
		rpc_qcsapi_set_bw(value);
	else if (!strcmp(name, "wl1_chanspec"))
	{
		rpc_qcsapi_set_channel(value);
		rpc_qcsapi_set_channel(value);
	}
	else if (!strcmp(name, "wl1_auth_mode_x"))
		rpc_qcsapi_set_beacon_type(WIFINAME, value);
	else if (!strcmp(name, "wl1_crypto"))
		rpc_qcsapi_set_WPA_encryption_modes(WIFINAME, value);
	else if (!strcmp(name, "wl1_wpa_psk"))
		rpc_qcsapi_set_key_passphrase(WIFINAME, value);
	else if (!strcmp(name, "wl1_dtim"))
		rpc_qcsapi_set_dtim(value);
	else if (!strcmp(name, "wl1_bcn"))
		rpc_qcsapi_set_beacon_interval(value);
	else if (!strcmp(name, "wl1_macmode"))
		rpc_update_macmode(value);
	else if (!strcmp(name, "wl1_maclist_x"))
		rpc_update_wlmaclist();
	else if (!strcmp(name, "wl1_mode_x"))
		rpc_update_wdslist();
	else if (!strcmp(name, "wl1_wdslist"))
		rpc_update_wdslist();
	else if (!strcmp(name, "wl1_wds_psk"))
		rpc_update_wds_psk(value);
	else if (!strncmp(name, "wl1.", 4))
		rpc_update_mbss(name, value);

//	rpc_show_config();
}

int rpc_qcsapi_set_wlmaclist(const char *ifname)
{
	int ret;
	qcsapi_mac_address_filtering mac_address_filtering;
	char list_mac_addresses[1024];
	char *m = NULL;
	char *p, *pp;

	ret = rpc_qcsapi_get_mac_address_filtering(ifname, &mac_address_filtering);
	if (ret < 0)
	{
		dbG("rpc_qcsapi_get_mac_address_filtering %s error, return: %d\n", ifname, ret);
		return ret;
	}
	else
	{
		if (mac_address_filtering == qcsapi_accept_mac_address_unless_denied)
		{
			ret = qcsapi_wifi_clear_mac_address_filters(ifname);
			if (ret < 0)
			{
				dbG("Qcsapi qcsapi_wifi_clear_mac_address_filters %s, error, return: %d\n", ifname, ret);
				return ret;
			}

			pp = p = strdup(nvram_safe_get("wl1_maclist_x"));
			if (pp) {
				while ((m = strsep(&p, "<")) != NULL) {
					if (!strlen(m)) continue;
					ret = rpc_qcsapi_deny_mac_address(ifname, m);
					if (ret < 0)
						dbG("rpc_qcsapi_deny_mac_address %s error, return: %d\n", ifname, ret);
				}
				free(pp);
			}

			ret = rpc_qcsapi_get_denied_mac_addresses(ifname, list_mac_addresses, sizeof(list_mac_addresses));
			if (ret < 0)
				dbG("rpc_qcsapi_get_denied_mac_addresses %s error, return: %d\n", ifname, ret);
			else
				dbG("current denied MAC addresses of interface %s: %s\n", ifname, list_mac_addresses);
		}
		else if (mac_address_filtering == qcsapi_deny_mac_address_unless_authorized)
		{
			ret = qcsapi_wifi_clear_mac_address_filters(ifname);
			if (ret < 0)
			{
				dbG("Qcsapi qcsapi_wifi_clear_mac_address_filters %s error, return: %d\n", ifname, ret);
				return ret;
			}

			pp = p = strdup(nvram_safe_get("wl1_maclist_x"));
			if (pp) {
				while ((m = strsep(&p, "<")) != NULL) {
					if (!strlen(m)) continue;
					ret = rpc_qcsapi_authorize_mac_address(ifname, m);
					if (ret < 0)
						dbG("rpc_qcsapi_authorize_mac_address %s error, return: %d\n", ifname, ret);
				}
				free(pp);
			}

			ret = rpc_qcsapi_get_authorized_mac_addresses(ifname, list_mac_addresses, sizeof(list_mac_addresses));
			if (ret < 0)
				dbG("rpc_qcsapi_get_authorized_mac_addresses %s error, return: %d\n", ifname, ret);
			else
				dbG("current authorized MAC addresses of interface %s: %s\n", ifname, list_mac_addresses);
		}
	}

	return ret;
}

void rpc_update_wlmaclist(void)
{
	int ret;
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
	int i, unit = 1;

	ret = rpc_qcsapi_set_wlmaclist(WIFINAME);
	if (ret < 0)
		dbG("rpc_qcsapi_set_wlmaclist %s error, return: %d\n", WIFINAME, ret);

	for (i = 1; i < 4; i++)
	{
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, i);

		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
		{
			ret = rpc_qcsapi_set_wlmaclist(wl_vifname_qtn(unit, i));
			if (ret < 0)
				dbG("rpc_qcsapi_set_wlmaclist %s error, return: %d\n", wl_vifname_qtn(unit, i), ret);
		}
	}
}

void rpc_update_wdslist()
{
	int ret, i;
	qcsapi_mac_addr peer_address;
	char *m = NULL;
	char *p, *pp;

	for (i = 0; i < 8; i++)
	{
		ret = qcsapi_wds_get_peer_address(WIFINAME, 0, (uint8_t *) &peer_address);
		if (ret < 0)
		{
			if (ret == -19) break;	// No such device
			dbG("Qcsapi qcsapi_wds_get_peer_address %s error, return: %d\n", WIFINAME, ret);
		}
		else
		{
//			dbG("current WDS peer index 0 addresse: %s\n", wl_ether_etoa((struct ether_addr *) &peer_address));
			ret = qcsapi_wds_remove_peer(WIFINAME, peer_address);
			if (ret < 0)
				dbG("Qcsapi qcsapi_wds_remove_peer %s error, return: %d\n", WIFINAME, ret);
		}
	}

	if (nvram_match("wl1_mode_x", "0"))
		return;

	pp = p = strdup(nvram_safe_get("wl1_wdslist"));
	if (pp) {
		while ((m = strsep(&p, "<")) != NULL) {
			if (!strlen(m)) continue;

			ether_atoe(m, peer_address);
			ret = qcsapi_wds_add_peer(WIFINAME, peer_address);
			if (ret < 0)
				dbG("Qcsapi qcsapi_wds_add_peer %s error, return: %d\n", WIFINAME, ret);
			else
			{
				ret = rpc_qcsapi_wds_set_psk(WIFINAME, m, nvram_safe_get("wl1_wds_psk"));
				if (ret < 0)
					dbG("Qcsapi rpc_qcsapi_wds_set_psk %s error, return: %d\n", WIFINAME, ret);
			}
		}
		free(pp);
	}

	for (i = 0; i < 8; i++)
	{
		ret = qcsapi_wds_get_peer_address(WIFINAME, i, (uint8_t *) &peer_address);
		if (ret < 0)
		{
			if (ret == -19) break;	// No such device
			dbG("Qcsapi qcsapi_wds_get_peer_address %s error, return: %d\n", WIFINAME, ret);
		}
		else
			dbG("current WDS peer index 0 addresse: %s\n", wl_ether_etoa((struct ether_addr *) &peer_address));
	}
}

void rpc_update_wds_psk(const char *wds_psk)
{
	int ret, i;
	qcsapi_mac_addr peer_address;

	if (nvram_match("wl1_mode_x", "0"))
		return;

	for (i = 0; i < 8; i++)
	{
		ret = qcsapi_wds_get_peer_address(WIFINAME, i, (uint8_t *) &peer_address);
		if (ret < 0)
		{
			if (ret == -19) break;	// No such device
			dbG("Qcsapi qcsapi_wds_get_peer_address %s error, return: %d\n", WIFINAME, ret);
		}
		else
		{
//			dbG("current WDS peer index 0 addresse: %s\n", wl_ether_etoa((struct ether_addr *) &peer_address));
			ret = rpc_qcsapi_wds_set_psk(WIFINAME, wl_ether_etoa((struct ether_addr *) &peer_address), wds_psk);
			if (ret < 0)
				dbG("rpc_qcsapi_wds_set_psk %s error, return: %d\n", WIFINAME, ret);
		}
	}
}

#define SET_SSID	0x01
#define SET_CLOSED	0x02
#define SET_AUTH	0x04
#define	SET_CRYPTO	0x08
#define	SET_WPAPSK	0x10
#define	SET_MACMODE	0x20
#define SET_ALL		0x3F

static void rpc_reload_mbss(int unit, int subunit, const char *name_mbss)
{
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
	unsigned char set_type = 0;
	int ret;
	char *auth_mode;

	if (subunit > 0)
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
	else
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	if (!strcmp(name_mbss, "ssid"))
		set_type = SET_SSID;
	else if (!strcmp(name_mbss, "closed"))
		set_type = SET_CLOSED;
	else if (!strcmp(name_mbss, "auth_mode_x"))
		set_type = SET_AUTH;
	else if (!strcmp(name_mbss, "crypto"))
		set_type = SET_CRYPTO;
	else if (!strcmp(name_mbss, "wpa_psk"))
		set_type = SET_WPAPSK;
	else if (!strcmp(name_mbss, "macmode"))
		set_type = SET_MACMODE;
	else if (!strcmp(name_mbss, "all"))
		set_type = SET_ALL;

	if (set_type & SET_SSID)
	{
		ret = rpc_qcsapi_set_SSID(wl_vifname_qtn(unit, subunit), nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
		if (ret < 0)
			dbG("rpc_qcsapi_set_SSID %s error, return: %d\n",
				wl_vifname_qtn(unit, subunit), ret);
	}

	if (set_type & SET_CLOSED)
	{
		ret = rpc_qcsapi_set_SSID_broadcast(wl_vifname_qtn(unit, subunit), nvram_safe_get(strcat_r(prefix, "closed", tmp)));
		if (ret < 0)
			dbG("rpc_qcsapi_set_SSID_broadcast %s error, return: %d\n",
				wl_vifname_qtn(unit, subunit), ret);
	}

	if (set_type & SET_AUTH)
	{
		ret = rpc_qcsapi_set_beacon_type(wl_vifname_qtn(unit, subunit), nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp)));
		if (ret < 0)
			dbG("rpc_qcsapi_set_beacon_type %s error, return: %d\n",
				wl_vifname_qtn(unit, subunit), ret);
	}

	if (set_type & SET_CRYPTO)
	{
		auth_mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
		if (!strcmp(auth_mode, "psk")  ||
		    !strcmp(auth_mode, "psk2") ||
		    !strcmp(auth_mode, "pskpsk2"))
		{
			ret = rpc_qcsapi_set_WPA_encryption_modes(wl_vifname_qtn(unit, subunit), nvram_safe_get(strcat_r(prefix, "crypto", tmp)));
			if (ret < 0)
				dbG("rpc_qcsapi_set_WPA_encryption_modes %s error, return: %d\n",
					wl_vifname_qtn(unit, subunit), ret);
		}
	}

	if (set_type & SET_WPAPSK)
	{
		auth_mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
		if (!strcmp(auth_mode, "psk")  ||
		    !strcmp(auth_mode, "psk2") ||
		    !strcmp(auth_mode, "pskpsk2"))
		{
			ret = rpc_qcsapi_set_key_passphrase(wl_vifname_qtn(unit, subunit), nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
			if (ret < 0)
				dbG("rpc_qcsapi_set_key_passphrase %s error, return: %d\n",
					wl_vifname_qtn(unit, subunit), ret);
		}
	}

	if (set_type & SET_MACMODE)
	{
		ret = rpc_qcsapi_set_mac_address_filtering(wl_vifname_qtn(unit, subunit), nvram_safe_get(strcat_r(prefix, "macmode", tmp)));
		if (ret < 0)
		{
			dbG("rpc_qcsapi_set_mac_address_filtering %s error, return: %d\n",
				wl_vifname_qtn(unit, subunit), ret);

			return;
		}
		else
			rpc_qcsapi_set_wlmaclist(wl_vifname_qtn(unit, subunit));
	}
}

void rpc_update_mbss(const char* name, const char *value)
{
	int ret, unit, subunit;
	char name_mbss[32];
	char interface_status = 0;
	qcsapi_mac_addr wl_macaddr;
	char macbuf[13], macaddr_str[18];
	unsigned long long macvalue;
	unsigned char *macp;

	if (sscanf(name, "wl%d.%d_%s", &unit, &subunit, name_mbss) != 3)
		return;

	if ((subunit < 1) || (subunit > 3))
		return;

	ret = qcsapi_interface_get_status(wl_vifname_qtn(unit, subunit), &interface_status);
//	if (ret < 0)
//		dbG("qcsapi_interface_get_status %s error, return: %d\n", wl_vifname_qtn(unit, subunit), ret);

	if (!strcmp(name_mbss, "bss_enabled"))
	{
		if (nvram_match(name, "1"))
		{
			if (interface_status)
			{
				dbG("vif %s has existed already\n", wl_vifname_qtn(unit, subunit));

				return;
			}

			memset(&wl_macaddr, 0, sizeof(wl_macaddr));
			ret = qcsapi_interface_get_mac_addr(WIFINAME, (uint8_t *) wl_macaddr);
			if (ret < 0)
				dbG("Qcsapi qcsapi_interface_get_mac_addr %s error, return: %d\n", WIFINAME, ret);

			sprintf(macbuf, "%02X%02X%02X%02X%02X%02X",
				wl_macaddr[0],
				wl_macaddr[1],
				wl_macaddr[2],
				wl_macaddr[3],
				wl_macaddr[4],
				wl_macaddr[5]);
			macvalue = strtoll(macbuf, (char **) NULL, 16);
			macvalue += subunit;
			macp = (unsigned char*) &macvalue;
			memset(macaddr_str, 0, sizeof(macaddr_str));
			sprintf(macaddr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
				*(macp+5),
				*(macp+4),
				*(macp+3),
				*(macp+2),
				*(macp+1),
				*(macp+0));
			ether_atoe(macaddr_str, wl_macaddr);

			ret = qcsapi_wifi_create_bss(wl_vifname_qtn(unit, subunit), wl_macaddr);
			if (ret < 0)
			{
				dbG("Qcsapi qcsapi_wifi_create_bss %s error, return: %d\n", wl_vifname_qtn(unit, subunit), ret);

				return;
			}
			else
				dbG("Qcsapi qcsapi_wifi_create_bss %s successfully\n", wl_vifname_qtn(unit, subunit));

			rpc_reload_mbss(unit, subunit, "all");
		}
		else
		{
			ret = qcsapi_wifi_remove_bss(wl_vifname_qtn(unit, subunit));
			if (ret < 0)
				dbG("Qcsapi qcsapi_wifi_remove_bss %s error, return: %d\n", wl_vifname_qtn(unit, subunit), ret);
			else
				dbG("Qcsapi qcsapi_wifi_remove_bss %s successfully\n", wl_vifname_qtn(unit, subunit));
		}
	}
	else
		rpc_reload_mbss(unit, subunit, name_mbss);
}

int
ej_wl_channel_list_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret;
	int retval = 0;
	char tmp[256];
	string_1024 list_of_channels;
	char *p;
	int i = 0;;

	sprintf(tmp, "[\"%d\"]", 0);

	ret = qcsapi_wifi_get_list_channels(WIFINAME, (char *) &list_of_channels);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_list_channels %s error, return: %d\n", WIFINAME, ret);
		goto ERROR;
	}

	p = strtok((char *) list_of_channels, ",");
	while (p)
	{
		if (i == 0)
			sprintf(tmp, "[\"%s\"", (char *) p);
		else
			sprintf(tmp,  "%s, \"%s\"", tmp, (char *) p);

		p = strtok(NULL, ",");
		i++;
	}

	if (i)
		sprintf(tmp,  "%s]", tmp);

ERROR:
	retval += websWrite(wp, "%s", tmp);
	return retval;
}

int
ej_wl_scan_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	char ssid_str[128];
	char macstr[18];
	int retval = 0;
	int ret, i, j;
	unsigned int ap_count = 0;
	qcsapi_ap_properties ap_current;

	ret = qcsapi_wifi_start_scan(WIFINAME);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_start_scan %s error, return: %d\n", WIFINAME, ret);
		retval += websWrite(wp, "[]");
		return retval;
	}

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < i+1; j++)
			dbg(".");
		sleep(1);
		dbg("\n");
	}

	//Get the scaned AP count
	ret = qcsapi_wifi_get_results_AP_scan(WIFINAME, &ap_count);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_results_AP_scan %s error, return: %d\n", WIFINAME, ret);
		retval += websWrite(wp, "[]");
		return retval;
	}
	if (ap_count == 0) {
		dbG("Scaned ap count is 0\n");
		retval += websWrite(wp, "[]");
		return retval;
	}

	dbg("%-4s%-33s%-18s\n\n", "Ch", "SSID", "BSSID");

	retval += websWrite(wp, "[");

	for (i = 0; i < ap_count; i++) {
		ret = qcsapi_wifi_get_properties_AP(WIFINAME, i, &ap_current);
		if (ret < 0) {
			dbG("Qcsapi qcsapi_wifi_get_properties_AP %s error, return: %d\n", WIFINAME, ret);
			goto END;
		}

		memset(ssid_str, 0, sizeof(ssid_str));
		char_to_ascii(ssid_str, ap_current.ap_name_SSID);

		sprintf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X",
			ap_current.ap_mac_addr[0],
			ap_current.ap_mac_addr[1],
			ap_current.ap_mac_addr[2],
			ap_current.ap_mac_addr[3],
			ap_current.ap_mac_addr[4],
			ap_current.ap_mac_addr[5]);

		dbg("%-4d%-33s%-18s\n",
			ap_current.ap_channel,
			ap_current.ap_name_SSID,
			macstr
		);

		if (!i)
			retval += websWrite(wp, "[\"%s\", \"%s\"]", ssid_str, macstr);
		else
			retval += websWrite(wp, ", [\"%s\", \"%s\"]", ssid_str, macstr);
	}

	dbg("\n");
END:
	retval += websWrite(wp, "]");
	return retval;
}

int
ej_wl_sta_list_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret, retval = 0;
	qcsapi_unsigned_int association_count = 0;
	qcsapi_mac_addr sta_address;
	int i, firstRow = 1;

	ret = qcsapi_wifi_get_count_associations(WIFINAME, &association_count);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_count_associations %s error, return: %d\n", WIFINAME, ret);
		return retval;
	} else {
		for (i = 0; i < association_count; ++i) {
			ret = qcsapi_wifi_get_associated_device_mac_addr(WIFINAME, i, (uint8_t *) &sta_address);
			if (ret < 0) {
				dbG("Qcsapi qcsapi_wifi_get_associated_device_mac_addr %s error, return: %d\n", WIFINAME, ret);
				return retval;
			} else {
				if (firstRow == 1)
					firstRow = 0;
				else
					retval += websWrite(wp, ", ");

				retval += websWrite(wp, "[");

				retval += websWrite(wp, "\"%s\"", wl_ether_etoa((struct ether_addr *) &sta_address));
				retval += websWrite(wp, ", \"%s\"", "Yes");
				retval += websWrite(wp, ", \"%s\"", !nvram_match("wl1_auth_mode_x", "open") ? "Yes" : "No");

				retval += websWrite(wp, "]");
			}
		}
	}

	return retval;
}

int
wl_status_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret, retval = 0;
	qcsapi_SSID ssid;
	qcsapi_unsigned_int channel;
	qcsapi_unsigned_int bw;
	char chspec_str[8];
	int is_nctrlsb_lower;

	memset(&ssid, 0, sizeof(qcsapi_SSID));
	ret = rpc_qcsapi_get_SSID(WIFINAME, &ssid);
	if (ret < 0)
		dbG("rpc_qcsapi_get_SSID %s error, return: %d\n", WIFINAME, ret);
	retval += websWrite(wp, "SSID: \"%s\"\n", ssid);

	int rssi_by_chain[4], rssi;
	qcsapi_wifi_get_rssi_by_chain(WIFINAME, 0, &rssi_by_chain[0]);
	qcsapi_wifi_get_rssi_by_chain(WIFINAME, 1, &rssi_by_chain[1]);
	qcsapi_wifi_get_rssi_by_chain(WIFINAME, 2, &rssi_by_chain[2]);
	qcsapi_wifi_get_rssi_by_chain(WIFINAME, 3, &rssi_by_chain[3]);
	rssi = (rssi_by_chain[0] + rssi_by_chain[1] + rssi_by_chain[2] + rssi_by_chain[3]) / 4;

	retval += websWrite(wp, "RSSI: %d dBm\t", 0);

	retval += websWrite(wp, "SNR: %d dB\t", 0);

	retval += websWrite(wp, "noise: %d dBm\t", rssi);

	ret = rpc_qcsapi_get_channel(&channel);
	if (ret < 0)
		dbG("rpc_qcsapi_get_channel error, return: %d\n", ret);

	ret = rpc_qcsapi_get_bw(&bw);
	if (ret < 0)
		dbG("rpc_qcsapi_get_bw error, return: %d\n", ret);

	if (bw == 80)
		sprintf(chspec_str, "%d/%d", channel, bw);
	else if (bw == 40)
	{
		is_nctrlsb_lower = ((channel == 36) || (channel == 44) || (channel == 52) || (channel == 60) || (channel == 100) || (channel == 108) || (channel == 116) || (channel == 124) || (channel == 132) || (channel == 149) || (channel == 157));
		sprintf(chspec_str, "%d%c", channel, is_nctrlsb_lower ? 'l': 'u');
	}
	else
		sprintf(chspec_str, "%d", channel);

	retval += websWrite(wp, "Channel: %s\n", chspec_str);

	qcsapi_mac_addr wl_mac_addr;
	ret = rpc_qcsapi_interface_get_mac_addr(WIFINAME, &wl_mac_addr);
	if (ret < 0)
		dbG("rpc_qcsapi_interface_get_mac_addr %s error, return: %d\n", WIFINAME, ret);

	retval += websWrite(wp, "BSSID: %s\t", wl_ether_etoa((struct ether_addr *) &wl_mac_addr));

	retval += websWrite(wp, "Supported Rates: [ 6(b) 9 12(b) 18 24(b) 36 48 54 ]\n");

	retval += websWrite(wp, "\n");

	return retval;
}

static int
ej_wl_status_qtn(int eid, webs_t wp, int argc, char_t **argv, const char *ifname)
{
	int ret, retval = 0, i, rssi;
	qcsapi_unsigned_int association_count = 0, tx_phy_rate, rx_phy_rate, time_associated;
	qcsapi_mac_addr sta_address;
	int hr, min, sec;

	ret = qcsapi_wifi_get_count_associations(ifname, &association_count);
	if (ret >= 0) {
		for (i = 0; i < association_count; ++i) {
			rssi = tx_phy_rate = rx_phy_rate = time_associated = 0;

			ret = qcsapi_wifi_get_associated_device_mac_addr(ifname, i, (uint8_t *) &sta_address);
			if (ret < 0)
				dbG("Qcsapi qcsapi_wifi_get_associated_device_mac_addr %s error, return: %d\n", ifname, ret);

			ret= qcsapi_wifi_get_rssi_in_dbm_per_association(ifname, i, &rssi);
			if (ret < 0)
				dbG("Qcsapi qcsapi_wifi_get_rssi_in_dbm_per_association %s error, return: %d\n", ifname, ret);

			ret = qcsapi_wifi_get_tx_phy_rate_per_association(ifname, i, &tx_phy_rate);
			if (ret < 0)
				dbG("Qcsapi qcsapi_wifi_get_tx_phy_rate_per_association %s error, return: %d\n", ifname, ret);

			ret = qcsapi_wifi_get_rx_phy_rate_per_association(ifname, i, &rx_phy_rate);
			if (ret < 0)
				dbG("Qcsapi qcsapi_wifi_get_rx_phy_rate_per_association %s error, return: %d\n", ifname, ret);

			ret = qcsapi_wifi_get_time_associated_per_association(ifname, i, &time_associated);
			if (ret < 0)
				dbG("Qcsapi qcsapi_wifi_get_time_associated_per_association %s error, return: %d\n", ifname, ret);

			hr = time_associated / 3600;
			min = (time_associated % 3600) / 60;
			sec = time_associated - hr * 3600 - min * 60;

			retval += websWrite(wp, "%s ", wl_ether_etoa((struct ether_addr *) &sta_address));

			retval += websWrite(wp, "%-11s%-11s", "Yes", !nvram_match("wl1_auth_mode_x", "open") ? "Yes" : "No");

			retval += websWrite(wp, "%4ddBm ", rssi);

			retval += websWrite(wp, "%6dM ", tx_phy_rate);

			retval += websWrite(wp, "%6dM ", rx_phy_rate);

			retval += websWrite(wp, "%02d:%02d:%02d", hr, min, sec);

			retval += websWrite(wp, "\n");
		}
	}

	return retval;
}

int
ej_wl_status_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0;
	int i, unit = 1;
	char prefix[] = "wlXXXXXXXXXX_", tmp[128];

	ret += websWrite(wp, "\n");
	ret += websWrite(wp, "Stations List                           \n");
	ret += websWrite(wp, "----------------------------------------\n");

	ret += websWrite(wp, "%-18s%-11s%-11s%-8s%-8s%-8s%-12s\n",
			     "MAC", "Associated", "Authorized", "   RSSI", "Tx rate", "Rx rate", "Connect Time");

	ret += ej_wl_status_qtn(eid, wp, argc, argv, WIFINAME);

	for (i = 1; i < 4; i++) {
		sprintf(prefix, "wl%d.%d_", unit, i);
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
			ret += ej_wl_status_qtn(eid, wp, argc, argv, wl_vifname_qtn(unit, i));
	}

	return ret;
}

char *
getWscStatusStr_qtn()
{
	int ret, state = -1;
	char wps_state[32], state_str[32];

	ret = rpc_qcsapi_wps_get_state(WIFINAME, wps_state, sizeof(wps_state));
	if (ret < 0)
		dbG("rpc_qcsapi_wps_get_state %s error, return: %d\n", WIFINAME, ret);
	else if (sscanf(wps_state, "%d %s", &state, state_str) != 2)
		dbG("prase wps state error!\n");

	switch (state) {
	case 0: /* WPS_INITIAL */
		return "initialization";
		break;
	case 1: /* WPS_START */
		return "Start WPS Process";
		break;
	case 2: /* WPS_SUCCESS */
		return "Success";
		break;
	case 3: /* WPS_ERROR */
		return "Fail due to WPS message exchange error!";
		break;
	case 4: /* WPS_TIMEOUT */
		return "Fail due to WPS time out!";
		break;
	case 5: /* WPS_OVERLAP */
		return "Fail due to PBC session overlap!";
		break;
	default:
		if (nvram_match("wps_enable", "1"))
			return "Idle";
		else
			return "Not used";
		break;
	}
}

char *
get_WPSConfiguredStr_qtn()
{
	int ret;
	char wps_configured_state[32];

	wps_configured_state[0] = 0;
	ret = rpc_qcsapi_wps_get_configured_state(WIFINAME, wps_configured_state, sizeof(wps_configured_state));
	if (ret < 0)
		dbG("rpc_qcsapi_wps_get_configured_state %s error, return: %d\n", WIFINAME, ret);
	else
		dbG("wps_configured_state: %s (%d)\n", wps_configured_state, strlen(wps_configured_state));

	if (!strcmp(wps_configured_state, "configured"))
		return "Yes";
	else
		return "No";
}

char *
getWPSAuthMode_qtn()
{
	string_16 current_beacon_type;
	int ret;

	memset(&current_beacon_type, 0, sizeof(current_beacon_type));
	ret = rpc_qcsapi_get_beacon_type(WIFINAME, (char *) &current_beacon_type);
	if (ret < 0)
		dbG("rpc_qcsapi_get_beacon_type %s error, return: %d\n", WIFINAME, ret);

	if (!strcmp(current_beacon_type, "Basic"))
		return "Open System";
	else if (!strcmp(current_beacon_type, "WPA"))
		return "WPA-Personal";
	else if (!strcmp(current_beacon_type, "11i"))
		return "WPA2-Personal";
	else if (!strcmp(current_beacon_type, "WPAand11i"))
		return "WPA-Auto-Personal";
	else
		return "Open System";
}

char *
getWPSEncrypType_qtn()
{
	string_32 encryption_mode;
	int ret;

	memset(&encryption_mode, 0, sizeof(encryption_mode));
	ret = rpc_qcsapi_get_WPA_encryption_modes(WIFINAME, (char *) &encryption_mode);
	if (ret < 0)
		dbG("rpc_qcsapi_get_WPA_encryption_modes %s error, return: %d\n", WIFINAME, ret);

	if (!strcmp(encryption_mode, "TKIPEncryption"))
		return "TKIP";
	else if (!strcmp(encryption_mode, "AESEncryption"))
		return "AES";
	else if (!strcmp(encryption_mode, "TKIPandAESEncryption"))
		return "TKIP+AES";
	else
		return "AES";
}