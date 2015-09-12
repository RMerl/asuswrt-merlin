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
#include <wlutils.h>

#include <httpd.h>

#include <shutils.h>
#include <shared.h>

#include "web-qtn.h"

#define	MAX_RETRY_TIMES	30
#define	MAX_TOTAL_TIME	120
#define	WIFINAME	"wifi0"

static int s_c_rpc_use_udp = 0;
#if 0	/* remove */
static int qtn_qcsapi_init = 0;
#endif
static int qtn_init = 0;

extern uint8 wf_chspec_ctlchan(chanspec_t chspec);
extern chanspec_t wf_chspec_aton(const char *a);
extern char *wl_vifname_qtn(int unit, int subunit);

#if 1	/* replaced by raw Ethernet RPC */
int rpc_qcsapi_init(int verbose)
{
	// const char *host;
	char host[18];
	CLIENT *clnt;
	int retry = 0;
	time_t start_time = uptime();

	/* setup RPC based on udp protocol */
	do {
		if (verbose)
		dbG("#%d attempt to create RPC connection\n", retry + 1);

		// host = client_qcsapi_find_host_addr(0, NULL);
		strcpy(host, nvram_safe_get("QTN_RPC_SERVER"));
		if (!strlen(host)) {
			if (verbose)
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
#if 0	/* remove */
			qtn_qcsapi_init = 1;
#endif
			return 0;
		}
	} while ((retry++ < MAX_RETRY_TIMES) && ((uptime() - start_time) < MAX_TOTAL_TIME));

	return -1;
}
#else
int rpc_qcsapi_init(int verbose)
{
	// const char *host;
	char host[18];
	CLIENT *clnt;
	int retry = 0;
	time_t start_time = uptime();
	char hwaddr_5g[18];
	uint8_t dst_mac[ETH_HLEN];

	/* setup RPC based on udp protocol */
	do {
		if (verbose)
		dbG("#%d attempt to create RPC connection\n", retry + 1);

		// host = client_qcsapi_find_host_addr(0, NULL);
		if (str_to_mac(nvram_safe_get("1:macaddr"), dst_mac) < 0) {
			dbG("QRPC: Wrong destination MAC address format. "
				"Use the following format: XX:XX:XX:XX:XX:XX\n");
			sleep(1);
			continue;
		}

		clnt = qrpc_clnt_raw_create(QCSAPI_PROG, QCSAPI_VERS, "br0", dst_mac);

		if (clnt == NULL) {
			clnt_pcreateerror("QRPC: ");
			sleep(1);
			continue;
		} else {
			client_qcsapi_set_rpcclient(clnt);
			qtn_qcsapi_init = 1;
			return 0;
		}
	} while ((retry++ < MAX_RETRY_TIMES) && ((uptime() - start_time) < MAX_TOTAL_TIME));

	return -1;
}
#endif

int rpc_qtn_ready()
{
	int ret, qtn_ready;
	int lock;

	qtn_ready = nvram_get_int("qtn_ready");

	lock = file_lock("qtn");

	if (qtn_ready && !qtn_init)
	{
		ret = rpc_qcsapi_init(0);
		if (ret < 0){
			qtn_ready = 0;
			dbG("rpc_qcsapi_init error, return: %d\n", ret);
		}else
		{
			ret = qcsapi_init();
			if (ret < 0){
				qtn_ready = 0;
				dbG("Qcsapi qcsapi_init error, return: %d\n", ret);
			}else
				qtn_init = 1;
		}
	}

	file_unlock(lock);

	nvram_set("wl1_country_code", nvram_safe_get("1:ccode"));
	return qtn_ready;
}

int rpc_qcsapi_set_SSID(const char *ifname, const char *ssid)
{
	int ret;

	if (!rpc_qtn_ready())
		return -1;

	ret = qcsapi_wifi_set_SSID(ifname, ssid);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_SSID %s error, return: %d\n", ifname, ret);
		return ret;
	}
	dbG("%s as: %s\n", ifname, ssid);

	return 0;
}

int rpc_qcsapi_set_SSID_broadcast(const char *ifname, const char *option)
{
	int ret;
	int OPTION = 1 - atoi(option);

	if (!rpc_qtn_ready())
		return -1;

	ret = qcsapi_wifi_set_option(ifname, qcsapi_SSID_broadcast, OPTION);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_option::SSID_broadcast %s error, return: %d\n", ifname, ret);
		return ret;
	}
	dbG("%s as: %s\n", ifname, OPTION ? "TRUE" : "FALSE");

	return 0;
}

int rpc_qcsapi_set_vht(const char *mode)
{
	int ret;
	int VHT;

	if (!rpc_qtn_ready())
		return -1;

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
	dbG("%s as: %s\n", WIFINAME, VHT ? "11ac" : "11n");

	return 0;
}

int rpc_qcsapi_set_bw(const char *bw)
{
	int ret;
	int BW = 20;

	if (!rpc_qtn_ready())
		return -1;

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
	dbG("%s as: %d MHz\n", WIFINAME, BW);

	return 0;
}

int rpc_qcsapi_set_channel(const char *chspec_buf)
{
	int ret;
	int channel = 0;
	char str_ch[] = "149";

	if (!rpc_qtn_ready())
		return -1;

	channel = wf_chspec_ctlchan(wf_chspec_aton(chspec_buf));

	ret = qcsapi_wifi_set_channel(WIFINAME, channel);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_channel %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}
	dbG("%s as: %d\n", WIFINAME, channel);

	snprintf(str_ch, sizeof(str_ch), "%d", channel);
	ret = qcsapi_config_update_parameter(WIFINAME, "channel", str_ch);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_config_update_parameter %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}
	dbG("update wireless_conf.txt %s as: %s\n", WIFINAME, str_ch);

	return 0;
}

int rpc_qcsapi_set_beacon_type(const char *ifname, const char *auth_mode)
{
	int ret;
	char *p_new_beacon = NULL;

	if (!rpc_qtn_ready())
		return -1;

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
	dbG("%s as: %s\n", ifname, p_new_beacon);

	if (p_new_beacon) free(p_new_beacon);

	return 0;
}

int rpc_qcsapi_set_WPA_encryption_modes(const char *ifname, const char *crypto)
{
	int ret;
	string_32 encryption_modes;

	if (!rpc_qtn_ready())
		return -1;

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
	dbG("%s as: %s\n", ifname, encryption_modes);

	return 0;
}

int rpc_qcsapi_set_key_passphrase(const char *ifname, const char *wpa_psk)
{
	int ret;

	if (!rpc_qtn_ready())
		return -1;

	ret = qcsapi_wifi_set_key_passphrase(ifname, 0, wpa_psk);
	if (ret < 0) {
		dbG("%s set_key_passphrase error, return: %d\n", ifname, ret);

		ret = qcsapi_wifi_set_pre_shared_key(ifname, 0, wpa_psk);
		if (ret < 0)
			dbG("%s set_pre_shared_key error, return: %d\n", ifname, ret);

		return ret;
	}
	dbG("%s as: %s\n", ifname, wpa_psk);

	return 0;
}

int rpc_qcsapi_set_dtim(const char *dtim)
{
	int ret;
	int DTIM = atoi(dtim);

	if (!rpc_qtn_ready())
		return -1;

	ret = qcsapi_wifi_set_dtim(WIFINAME, DTIM);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_dtim %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}
	dbG("%s as: %d\n", WIFINAME, DTIM);

	return 0;
}

int rpc_qcsapi_set_beacon_interval(const char *beacon_interval)
{
	int ret;
	int BCN = atoi(beacon_interval);

	if (!rpc_qtn_ready())
		return -1;

	ret = qcsapi_wifi_set_beacon_interval(WIFINAME, BCN);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_beacon_interval %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}
	dbG("%s as: %d\n", WIFINAME, BCN);

	return 0;
}

int rpc_qcsapi_set_mac_address_filtering(const char *ifname, const char *mac_address_filtering)
{
	int ret;
	qcsapi_mac_address_filtering MAF;
	qcsapi_mac_address_filtering orig_mac_address_filtering;

	if (!rpc_qtn_ready())
		return -1;

	ret = rpc_qcsapi_get_mac_address_filtering(ifname, &orig_mac_address_filtering);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_mac_address_filtering %s error, return: %d\n", ifname, ret);
		return ret;
	}
	dbG("%s: %d\n", ifname, orig_mac_address_filtering);

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
	dbG("%s as: %d (%s)\n", ifname, MAF, mac_address_filtering);

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

	if (!rpc_qtn_ready())
		return;

	ret = rpc_qcsapi_set_mac_address_filtering(WIFINAME, mac_address_filtering);
	if (ret < 0) {
		dbG("rpc_qcsapi_set_mac_address_filtering %s error, return: %d\n", WIFINAME, ret);
	}

	if (nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_band"))
		return;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

	ether_atoe(macaddr, peer_address);
	ret = qcsapi_wds_set_psk(ifname, peer_address, wpa_psk);
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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

	ret = qcsapi_wifi_get_channel(WIFINAME, p_channel);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_channel %s error, return: %d\n", WIFINAME, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_get_snr(void)
{
	int ret;
	int snr;

	if (!rpc_qtn_ready())
		return -1;

	ret = qcsapi_wifi_get_avg_snr(WIFINAME, &snr);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_avg_snr %s error, return: %d\n", WIFINAME, ret);
		return 0;
	}

	return snr;
}

int rpc_qcsapi_get_channel_list(string_1024* list_of_channels)
{
	int ret;

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return -1;

	ret = qcsapi_wps_registrar_report_pin(ifname, wps_pin);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wps_registrar_report_pin %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

#if 0	/* remove */
int rpc_qcsapi_restore_default_config(int flag)
{
	int ret;

	if (!qtn_qcsapi_init) {
		ret = rpc_qcsapi_init(1);
		if (ret < 0){
			dbG("[restore_default] rpc_qcsapi_init error, return: %d\n", ret);
			return -1;
		}
	}

	ret = qcsapi_restore_default_config(flag);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_restore_default_config error, return: %d\n", ret);
		return ret;
	}
	dbG("QTN restore default config successfully\n");

	qtn_qcsapi_init = 0;

	return 0;
}
#endif

int rpc_qcsapi_bootcfg_commit(void)
{
	int ret;

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return;

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

void rpc_set_radio(int unit, int subunit, int on)
{
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
	int ret;
	char interface_status = 0;
	qcsapi_mac_addr wl_macaddr;
	char macbuf[13], macaddr_str[18];
	unsigned long long macvalue;
	unsigned char *macp;

	if (subunit > 0)
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
	else
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	if (subunit > 0)
	{
		ret = qcsapi_interface_get_status(wl_vifname_qtn(unit, subunit), &interface_status);
//		if (ret < 0)
//			dbG("Qcsapi qcsapi_interface_get_status %s error, return: %d\n", wl_vifname_qtn(unit, subunit), ret);

		if (on)
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
				dbG("Qcsapi qcsapi_wifi_create_bss %s error, return: %d\n",
					wl_vifname_qtn(unit, subunit), ret);

				return;
			}

			ret = rpc_qcsapi_set_SSID(wl_vifname_qtn(unit, subunit), nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
			if (ret < 0)
				dbG("rpc_qcsapi_set_SSID %s error, return: %d\n",
					wl_vifname_qtn(unit, subunit), ret);

			ret = rpc_qcsapi_set_SSID_broadcast(wl_vifname_qtn(unit, subunit), nvram_safe_get(strcat_r(prefix, "closed", tmp)));
			if (ret < 0)
				dbG("rpc_qcsapi_set_SSID_broadcast %s error, return: %d\n",
					wl_vifname_qtn(unit, subunit), ret);

			ret = rpc_qcsapi_set_beacon_type(wl_vifname_qtn(unit, subunit), nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp)));
			if (ret < 0)
				dbG("rpc_qcsapi_set_beacon_type %s error, return: %d\n",
					wl_vifname_qtn(unit, subunit), ret);

			ret = rpc_qcsapi_set_WPA_encryption_modes(wl_vifname_qtn(unit, subunit), nvram_safe_get(strcat_r(prefix, "crypto", tmp)));
			if (ret < 0)
				dbG("rpc_qcsapi_set_WPA_encryption_modes %s error, return: %d\n",
					wl_vifname_qtn(unit, subunit), ret);

			ret = rpc_qcsapi_set_key_passphrase(wl_vifname_qtn(unit, subunit), nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
			if (ret < 0)
				dbG("rpc_qcsapi_set_key_passphrase %s error, return: %d\n",
					wl_vifname_qtn(unit, subunit), ret);
		}
		else
		{
			ret = qcsapi_wifi_remove_bss(wl_vifname_qtn(unit, subunit));
			if (ret < 0)
				dbG("Qcsapi qcsapi_wifi_remove_bss %s error, return: %d\n",
					wl_vifname_qtn(unit, subunit), ret);
		}
	}
	else {
		ret = qcsapi_wifi_rfenable((qcsapi_unsigned_int) on);
		if (ret < 0)
			dbG("Qcsapi qcsapi_wifi_rfenable %s, return: %d\n", WIFINAME, ret);
	}
}

int rpc_update_ap_isolate(const char *ifname, const int isolate)
{
	int ret;

	if(!rpc_qtn_ready())
		return -1;

	qcsapi_wifi_rfenable((qcsapi_unsigned_int) 0 /* off */);
	ret = qcsapi_wifi_set_ap_isolate(ifname, isolate);
	if(ret < 0){
		dbG("qcsapi_wifi_set_ap_isolate %s error, return: %d\n", ifname, ret);
		return ret;
	}else{
		dbG("qcsapi_wifi_set_ap_isolate OK\n");
	}
	if(nvram_get_int("wl1_radio") == 1)
		qcsapi_wifi_rfenable((qcsapi_unsigned_int) 1 /* on */);

	return 0;
}

void rpc_parse_nvram(const char *name, const char *value)
{
#if 0
	if (!rpc_qtn_ready())
		return;

	if (!strcmp(name, "wl1_ssid"))
		rpc_qcsapi_set_SSID(WIFINAME, value);
	else if (!strcmp(name, "wl1_closed"))
		rpc_qcsapi_set_SSID_broadcast(WIFINAME, value);
	else if (!strcmp(name, "wl1_nmode_x"))
		rpc_qcsapi_set_vht(value);
	else if (!strcmp(name, "wl1_bw"))
		rpc_qcsapi_set_bw(value);
	else if (!strcmp(name, "wl1_chanspec"))
		rpc_qcsapi_set_channel(value);
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
	else if (!strcmp(name, "wl1_radio"))
		rpc_set_radio(1, 0, nvram_get_int(name));
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
	else if (!strcmp(name, "wl1_ap_isolate"))
		rpc_update_ap_isolate(WIFINAME, atoi(value));
	else if (!strncmp(name, "wl1.", 4))
		rpc_update_mbss(name, value);

//	rpc_show_config();
#else
	return;	/* move to restart_wireless */
#endif
}

int rpc_qcsapi_set_wlmaclist(const char *ifname)
{
	int ret;
	qcsapi_mac_address_filtering mac_address_filtering;
	char list_mac_addresses[1024];
	char *m = NULL;
	char *p, *pp;

	if (!rpc_qtn_ready())
		return -1;

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

	if (!rpc_qtn_ready())
		return;

	ret = rpc_qcsapi_set_wlmaclist(WIFINAME);
	if (ret < 0)
		dbG("rpc_qcsapi_set_wlmaclist %s error, return: %d\n", WIFINAME, ret);

	if (nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_band"))
                return;

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

	if (!rpc_qtn_ready())
		return;

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

	if (!rpc_qtn_ready())
		return;

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

	if (!rpc_qtn_ready())
		return;

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

        if (nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_band"))
                return;

	if (!rpc_qtn_ready())
		return;

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

int get_wl_channel_list_5g_by_bw(string_1024 list_of_channels, int bw)
{
	int ret;
	int retval = 0;
	char tmp[256];
	char *p;
	int i = 0;;
	char cur_ccode[20] = {0};

	sprintf(tmp, "[\"%d\"]", 0);

	if (!rpc_qtn_ready()){
		snprintf(tmp, sizeof(tmp), "");
		goto ERROR;
	}

	// ret = qcsapi_wifi_get_list_channels(WIFINAME, (char *) &list_of_channels);
	ret = qcsapi_wifi_get_regulatory_region(WIFINAME, cur_ccode);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_regulatory_region %s error, return: %d\n", WIFINAME, ret);
		goto ERROR;
	}

	if(strcmp(cur_ccode, "eu")==0 || strcmp(cur_ccode, "jp")==0){
		ret = qcsapi_regulatory_get_list_regulatory_channels(cur_ccode, 40 /* bw */, list_of_channels);
	}else{
		ret = qcsapi_regulatory_get_list_regulatory_channels(cur_ccode, bw, list_of_channels);
	}
	if (ret < 0) {
		dbG("Qcsapi qcsapi_regulatory_get_list_regulatory_channels %s error, return: %d\n", WIFINAME, ret);
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
	/* list_of_channels = 1024, tmp = 256 */
	sprintf(list_of_channels, "%s", tmp);
	return 0;
}

int ej_wl_channel_list_5g_20m(int eid, webs_t wp, int argc, char_t **argv)
{
	static char list_20m[1024] = {0};
	int retval;

	if(strlen(list_20m) == 0) retval = get_wl_channel_list_5g_by_bw(list_20m, 20);
	retval = websWrite(wp, "%s", list_20m);
	return retval;
}

int ej_wl_channel_list_5g_40m(int eid, webs_t wp, int argc, char_t **argv)
{
	static char list_40m[1024] = {0};
	int retval;

	if(strlen(list_40m) == 0) get_wl_channel_list_5g_by_bw(list_40m, 40);
	retval = websWrite(wp, "%s", list_40m);
	return retval;
}

int ej_wl_channel_list_5g_80m(int eid, webs_t wp, int argc, char_t **argv)
{
	static char list_80m[1024] = {0};
	int retval;

	if(strlen(list_80m) == 0) get_wl_channel_list_5g_by_bw(list_80m, 80);
	retval = websWrite(wp, "%s", list_80m);
	return retval;
}

int
ej_wl_channel_list_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	static char list_80m[1024] = {0};
	int retval;

	if(strlen(list_80m) == 0) get_wl_channel_list_5g_by_bw(list_80m, 80);
	retval = websWrite(wp, "%s", list_80m);
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

	if (!rpc_qtn_ready())
	{
		retval += websWrite(wp, "[]");
		return retval;
	}

	ret = qcsapi_wifi_start_scan_ext(WIFINAME, IEEE80211_PICK_ALL | IEEE80211_PICK_NOPICK_BG);
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

static int
ej_wl_sta_list_qtn(int eid, webs_t wp, int argc, char_t **argv, const char *ifname)
{
	int ret, retval = 0, firstRow = 1;;
	qcsapi_unsigned_int association_count = 0;
	qcsapi_mac_addr sta_address;
	int i, rssi, index = -1, unit = 1;
	char prefix[] = "wlXXXXXXXXXX_", tmp[128];

	if (!rpc_qtn_ready())
		return retval;

	int from_app = 0;
	char *name_t = NULL;

	if (strArgs(argc, argv, "%s", &name_t) < 1) {
		//_dprintf("name_t = NULL\n");
	} else if (!strncmp(name_t, "appobj", 6))
		from_app = 1;

	sscanf(ifname, "wifi%d", &index);
	if (index == -1) return retval;
	else if (index == 0)
		sprintf(prefix, "wl%d_", unit);
	else
		sprintf(prefix, "wl%d.%d_", unit, index);

	ret = qcsapi_wifi_get_count_associations(ifname, &association_count);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_count_associations %s error, return: %d\n", ifname, ret);
		return retval;
	} else {
		for (i = 0; i < association_count; ++i) {
			rssi = 0;
			ret = qcsapi_wifi_get_associated_device_mac_addr(ifname, i, (uint8_t *) &sta_address);
			if (ret < 0) {
				dbG("Qcsapi qcsapi_wifi_get_associated_device_mac_addr %s error, return: %d\n", ifname, ret);
				return retval;
			} else {
				if (firstRow == 1)
					firstRow = 0;
				else
				retval += websWrite(wp, ", ");
				if(from_app == 0)
					retval += websWrite(wp, "[");

				retval += websWrite(wp, "\"%s\"", wl_ether_etoa((struct ether_addr *) &sta_address));
				if(from_app == 1){
					retval += websWrite(wp, ":{");
					retval += websWrite(wp, "\"isWL\":");
				}
				if(from_app == 0)
					retval += websWrite(wp, ", \"%s\"", "Yes");
				else
					retval += websWrite(wp, "\"%s\"", "Yes");
				if(from_app == 0)
					retval += websWrite(wp, ", \"%s\"", !(nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open")) ? "Yes" : "No");
				if(from_app == 1){
					ret += websWrite(wp, ",\"rssi\":");
				}
				ret= qcsapi_wifi_get_rssi_in_dbm_per_association(ifname, i, &rssi);
				if (ret < 0)
					dbG("Qcsapi qcsapi_wifi_get_rssi_in_dbm_per_association %s error, return: %d\n", ifname, ret);
				if(from_app == 0)
					retval += websWrite(wp, ", \"%d\"", rssi);
				else
					retval += websWrite(wp, "\"%d\"", rssi);
				if(from_app == 0)
					retval += websWrite(wp, "]");
				else
					retval += websWrite(wp, "}");
			}
		}
	}

	return retval;
}

#ifdef RTCONFIG_STAINFO
static int
ej_wl_stainfo_list_qtn(int eid, webs_t wp, int argc, char_t **argv, const char *ifname)
{
	int ret, retval = 0, firstRow = 1;;
	qcsapi_unsigned_int association_count = 0;
	qcsapi_mac_addr sta_address;
	int i, rssi, index = -1, unit = 1;
	char prefix[] = "wlXXXXXXXXXX_", tmp[128];
	qcsapi_unsigned_int tx_phy_rate, rx_phy_rate, time_associated;
	int hr, min, sec;

	if (!rpc_qtn_ready())
		return retval;

	sscanf(ifname, "wifi%d", &index);
	if (index == -1) return retval;
	else if (index == 0)
		sprintf(prefix, "wl%d_", unit);
	else
		sprintf(prefix, "wl%d.%d_", unit, index);

	ret = qcsapi_wifi_get_count_associations(ifname, &association_count);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_count_associations %s error, return: %d\n", ifname, ret);
		return retval;
	} else {
		for (i = 0; i < association_count; ++i) {
			rssi = 0;
			ret = qcsapi_wifi_get_associated_device_mac_addr(ifname, i, (uint8_t *) &sta_address);
			if (ret < 0) {
				dbG("Qcsapi qcsapi_wifi_get_associated_device_mac_addr %s error, return: %d\n", ifname, ret);
				return retval;
			} else {
				if (firstRow == 1)
					firstRow = 0;
				else
				retval += websWrite(wp, ", ");
				retval += websWrite(wp, "[");

				retval += websWrite(wp, "\"%s\"", wl_ether_etoa((struct ether_addr *) &sta_address));

				tx_phy_rate = rx_phy_rate = time_associated = 0;

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

				retval += websWrite(wp, ", \"%d\"", tx_phy_rate);
				retval += websWrite(wp, ", \"%d\"", rx_phy_rate);
				retval += websWrite(wp, ", \"%02d:%02d:%02d\"", hr, min, sec);

				retval += websWrite(wp, "]");
			}
		}
	}

	return retval;
}
#endif

int
ej_wl_sta_list_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0, retval = 0, ret_t = 0;
	int i, unit = 1;
	char prefix[] = "wlXXXXXXXXXX_", tmp[128];

	if (!rpc_qtn_ready())
		return retval;

	ret += ej_wl_sta_list_qtn(eid, wp, argc, argv, WIFINAME);

        if (nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_band"))
                return ret;

	for (i = 1; i < 4; i++) {
		sprintf(prefix, "wl%d.%d_", unit, i);
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1")){
			if (ret_t != ret)
				retval += websWrite(wp, ", ");
			ret += ej_wl_sta_list_qtn(eid, wp, argc, argv, wl_vifname_qtn(unit, i));
			ret_t = ret;
		}
	}

	return retval;
}

#ifdef RTCONFIG_STAINFO
int
ej_wl_stainfo_list_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0, retval = 0, ret_t = 0;
	int i, unit = 1;
	char prefix[] = "wlXXXXXXXXXX_", tmp[128];

	if (!rpc_qtn_ready())
		return retval;

	ret += ej_wl_stainfo_list_qtn(eid, wp, argc, argv, WIFINAME);

        if (nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_band"))
                return ret;

	for (i = 1; i < 4; i++) {
		sprintf(prefix, "wl%d.%d_", unit, i);
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1")){
			if (ret_t != ret)
				retval += websWrite(wp, ", ");
			ret += ej_wl_stainfo_list_qtn(eid, wp, argc, argv, wl_vifname_qtn(unit, i));
			ret_t = ret;
		}
	}

	return retval;
}
#endif

int
wl_status_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret, retval = 0;
	qcsapi_SSID ssid;
	qcsapi_unsigned_int channel;
	qcsapi_unsigned_int bw;
	char chspec_str[8];
	int is_nctrlsb_lower;

	if (!rpc_qtn_ready())
		return 0;

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

	retval += websWrite(wp, "SNR: %d dB\t", rpc_qcsapi_get_snr());

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

	if (!rpc_qtn_ready())
		return retval;

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

	if (!rpc_qtn_ready())
		return ret;

	ret += websWrite(wp, "\n");
	ret += websWrite(wp, "Stations  (flags: A=Associated, U=Authenticated)\n");
	ret += websWrite(wp, "----------------------------------------\n");

	ret += websWrite(wp, "%-18s%-16s%-16s%-8s%-15s%-10s%-5s\n",
			     "MAC", "IP Address", "Name", "  RSSI", "  Rx/Tx Rate", "Connected", "Flags");

	ret += ej_wl_status_qtn(eid, wp, argc, argv, WIFINAME);

        if (nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_band"))
                return ret;

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

	if (!rpc_qtn_ready())
		return "5 GHz radio is not ready";

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

	if (!rpc_qtn_ready())
		return "";

	wps_configured_state[0] = 0;
	ret = rpc_qcsapi_wps_get_configured_state(WIFINAME, wps_configured_state, sizeof(wps_configured_state));
	if (ret < 0)
		dbG("rpc_qcsapi_wps_get_configured_state %s error, return: %d\n", WIFINAME, ret);

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

	if (!rpc_qtn_ready())
		return "";

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

	if (!rpc_qtn_ready())
		return "";

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

static int psta_auth = 0;

int
ej_wl_auth_psta_qtn(int eid, webs_t wp, int argc, char_t **argv)
{
	char *ifname = "wifi0";
	char ssid[33], auth[33];
	int psta_debug = 0;
	int retval = 0, psta = 0;
	char wlc_ssid[33];

	strcpy(wlc_ssid, nvram_safe_get("wlc_ssid"));
	memset(ssid, 0, sizeof(ssid));
	memset(auth, 0, sizeof(auth));

	if (nvram_match("psta_debug", "1"))
		psta_debug = 1;

	if (!rpc_qtn_ready()){
		goto ERROR;
	}

	qcsapi_wifi_get_SSID(WIFINAME, ssid);

	if(!strcmp(ssid, wlc_ssid))
	{
		if (psta_debug) dbg("connected: ssid %s\n", ssid);
		psta = 1;
		psta_auth = 0;
	}
	else
	{
		qcsapi_SSID_get_authentication_mode(ifname, wlc_ssid, auth);
		if (psta_debug) dbg("get_auth : ssid=%s, auth=%s\n", wlc_ssid, auth);

		if (!strcmp(auth, "PSKAuthentication") || !strcmp(auth, "EAPAuthentication"))
		{
			if (psta_debug) dbg("authorization failed\n");
			psta = 2;
			psta_auth = 1;
		}
		else
		{
			if (psta_debug) dbg("disconnected\n");
			psta = 0;
			psta_auth = 0;
		}
	}

	retval += websWrite(wp, "wlc_state=%d;", psta);
	retval += websWrite(wp, "wlc_state_auth=%d;", psta_auth);

ERROR:
	return retval;
}

int
wl_status_5g_array(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret, retval = 0;
	qcsapi_SSID ssid;
	char ssidtmp[sizeof(qcsapi_SSID)*2];
	qcsapi_unsigned_int channel;
	qcsapi_unsigned_int bw;
	char chspec_str[8];
	int is_nctrlsb_lower;

	if (!rpc_qtn_ready())
		return 0;

	memset(&ssid, 0, sizeof(qcsapi_SSID));
	ret = rpc_qcsapi_get_SSID(WIFINAME, &ssid);
	if (ret < 0) {
		ret += websWrite(wp, "\"\",\"\",\"\",\"\",\"\",\"\",");
		dbG("rpc_qcsapi_get_SSID %s error, return: %d\n", WIFINAME, ret);
	}

	if (str_escape_quotes(ssidtmp, ssid, sizeof(ssidtmp)) == 0 )
		strncpy(ssidtmp, ssid, sizeof(ssidtmp));

	retval += websWrite(wp, "\"%s\",", ssidtmp);

	int rssi_by_chain[4], rssi;
	qcsapi_wifi_get_rssi_by_chain(WIFINAME, 0, &rssi_by_chain[0]);
	qcsapi_wifi_get_rssi_by_chain(WIFINAME, 1, &rssi_by_chain[1]);
	qcsapi_wifi_get_rssi_by_chain(WIFINAME, 2, &rssi_by_chain[2]);
	qcsapi_wifi_get_rssi_by_chain(WIFINAME, 3, &rssi_by_chain[3]);
	rssi = (rssi_by_chain[0] + rssi_by_chain[1] + rssi_by_chain[2] + rssi_by_chain[3]) / 4;

	retval += websWrite(wp, "\"%d\",", 0);

	retval += websWrite(wp, "\"%d\",", rpc_qcsapi_get_snr());

	retval += websWrite(wp, "\"%d\",", rssi);

	ret = rpc_qcsapi_get_channel(&channel);
	if (ret < 0) {
		ret += websWrite(wp, "\"\",\"\",");
		dbG("rpc_qcsapi_get_channel error, return: %d\n", ret);
	}
	ret = rpc_qcsapi_get_bw(&bw);
	if (ret < 0) {
		ret += websWrite(wp, "\"\",\"\",");
		dbG("rpc_qcsapi_get_bw error, return: %d\n", ret);
	}

	if (bw == 80)
		sprintf(chspec_str, "%d/%d", channel, bw);
	else if (bw == 40)
	{
		is_nctrlsb_lower = ((channel == 36) || (channel == 44) || (channel == 52) || (channel == 60) || (channel == 100) || (channel == 108) || (channel == 116) || (channel == 124) || (channel == 132) || (channel == 149) || (channel == 157));
		sprintf(chspec_str, "%d%c", channel, is_nctrlsb_lower ? 'l': 'u');
	}
	else
		sprintf(chspec_str, "%d", channel);

	retval += websWrite(wp, "\"%s\",", chspec_str);

	qcsapi_mac_addr wl_mac_addr;
	ret = rpc_qcsapi_interface_get_mac_addr(WIFINAME, &wl_mac_addr);
	if (ret < 0) {
		ret += websWrite(wp, "\"\",");
		dbG("rpc_qcsapi_interface_get_mac_addr %s error, return: %d\n", WIFINAME, ret);
	}
	retval += websWrite(wp, "\"%s\",", wl_ether_etoa((struct ether_addr *) &wl_mac_addr));
	return retval;
}

static int
ej_wl_status_qtn_array(int eid, webs_t wp, int argc, char_t **argv, const char *ifname, int guest)
{
	int ret, retval = 0, i, rssi;
	qcsapi_unsigned_int association_count = 0, tx_phy_rate, rx_phy_rate, time_associated;
	qcsapi_mac_addr sta_address;
	int hr, min, sec;
	char *arplist = NULL, *arplistptr;
	char *leaselist = NULL, *leaselistptr;
	int found;
	char ipentry[40], macentry[18];
	char hostnameentry[32], tmp[16];

	if (!rpc_qtn_ready())
		return retval;

	ret = qcsapi_wifi_get_count_associations(ifname, &association_count);
	if (ret >= 0) {
		for (i = 0; i < association_count; ++i) {
			rssi = tx_phy_rate = rx_phy_rate = time_associated = 0;

			ret = qcsapi_wifi_get_associated_device_mac_addr(ifname, i, (uint8_t *) &sta_address);
			if (ret < 0) {
				dbG("Qcsapi qcsapi_wifi_get_associated_device_mac_addr %s error, return: %d\n", ifname, ret);
			}

			ret= qcsapi_wifi_get_rssi_in_dbm_per_association(ifname, i, &rssi);
			if (ret < 0) {
				dbG("Qcsapi qcsapi_wifi_get_rssi_in_dbm_per_association %s error, return: %d\n", ifname, ret);
			}

			ret = qcsapi_wifi_get_tx_phy_rate_per_association(ifname, i, &tx_phy_rate);
			if (ret < 0) {
				dbG("Qcsapi qcsapi_wifi_get_tx_phy_rate_per_association %s error, return: %d\n", ifname, ret);
			}

			ret = qcsapi_wifi_get_rx_phy_rate_per_association(ifname, i, &rx_phy_rate);
			if (ret < 0) {
				dbG("Qcsapi qcsapi_wifi_get_rx_phy_rate_per_association %s error, return: %d\n", ifname, ret);
			}

			ret = qcsapi_wifi_get_time_associated_per_association(ifname, i, &time_associated);
			if (ret < 0) {
				dbG("Qcsapi qcsapi_wifi_get_time_associated_per_association %s error, return: %d\n", ifname, ret);
			}

			hr = time_associated / 3600;
			min = (time_associated % 3600) / 60;
			sec = time_associated - hr * 3600 - min * 60;

			retval += websWrite(wp, "[\"%s\",", wl_ether_etoa((struct ether_addr *) &sta_address));

			/* Obtain mac + IP list */
			arplist = read_whole_file("/proc/net/arp");

			/* Obtain lease list - we still need the arp list for
			   cases where a device uses a static IP rather than DHCP */
			leaselist = read_whole_file("/var/lib/misc/dnsmasq.leases");

			found = 0;
			if (arplist) {
				arplistptr = arplist;

				while ((arplistptr < arplist+strlen(arplist)-2) && (sscanf(arplistptr,"%15s %*s %*s %17s",ipentry,macentry) == 2)) {
					if (upper_strcmp(macentry, wl_ether_etoa((struct ether_addr *) &sta_address)) == 0) {
						found = 1;
						break;
					} else {
						arplistptr = strstr(arplistptr,"\n")+1;
					}
				}

				if (found || !leaselist) {
					retval += websWrite(wp, "\"%s\",", (found ? ipentry : ""));
				}
			} else {
				retval += websWrite(wp, "\"<unknown>\",");
			}

			// Retrieve hostname from dnsmasq leases
			if (leaselist) {
				leaselistptr = leaselist;

				while ((leaselistptr < leaselist+strlen(leaselist)-2) && (sscanf(leaselistptr,"%*s %17s %15s %15s %*s", macentry, ipentry, tmp) == 3)) {
					if (upper_strcmp(macentry,  wl_ether_etoa((struct ether_addr *) &sta_address)) == 0) {
						found += 2;
						break;
					} else {
						leaselistptr = strstr(leaselistptr,"\n")+1;
					}
				}
				if ((found) && (str_escape_quotes(hostnameentry, tmp, sizeof(hostnameentry)) == 0 ))
					strncpy(hostnameentry, tmp, sizeof(hostnameentry));

				if (found == 0) {
					// Not in arplist nor in leaselist
					retval += websWrite(wp, "\"<not found>\",\"<not found>\",");
				} else if (found == 1) {
					// Only in arplist (static IP)
					retval += websWrite(wp, "\"<not found>\",");
				} else if (found == 2) {
					// Only in leaselist (dynamic IP that has not communicated with router for a while)
					retval += websWrite(wp, "\"%s\", \"%s\",", ipentry, hostnameentry);
				} else if (found == 3) {
					// In both arplist and leaselist (dynamic IP)
					retval += websWrite(wp, "\"%s\",", hostnameentry);
				}
			} else {
				retval += websWrite(wp, "\"<unknown>\",");
			}

			retval += websWrite(wp, "\"%d\",", rssi);
			retval += websWrite(wp, "\"%d\",\"%d\",", tx_phy_rate, rx_phy_rate);
			retval += websWrite(wp, "\"%3d:%02d:%02d\",", hr, min, sec);
			retval += websWrite(wp, "\"A%s%s\",", !nvram_match("wl1_auth_mode_x", "open") ? "U" : "", (guest ? "G" : ""));
			retval += websWrite(wp, "],");
		}
	}

	if (arplist) free(arplist);
	if (leaselist) free(leaselist);
	return retval;
}

int
ej_wl_status_5g_array(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0;
	int i, unit = 1;
	char prefix[] = "wlXXXXXXXXXX_", tmp[128];

	if (!rpc_qtn_ready()) {
		ret += websWrite(wp, "-1");
		return ret;
	}
	ret += ej_wl_status_qtn_array(eid, wp, argc, argv, WIFINAME, 0);
	if (nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_band")) {
		ret += websWrite(wp, "-1");
		return ret;
	}

	for (i = 1; i < 4; i++) {
		sprintf(prefix, "wl%d.%d_", unit, i);
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
			ret += ej_wl_status_qtn_array(eid, wp, argc, argv, wl_vifname_qtn(unit, i), 1);
	}

	return ret;
}

