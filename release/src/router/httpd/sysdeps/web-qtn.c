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
#include <unistd.h>

#include <httpd.h>

#include <shutils.h>

#include "qcsapi_output.h"
#include "qcsapi_rpc_common/client/find_host_addr.h"

#include "qcsapi.h"
#include "qcsapi_rpc/client/qcsapi_rpc_client.h"
#include "qcsapi_rpc/generated/qcsapi_rpc.h"
#include "qcsapi_driver.h"
#include "call_qcsapi.h"

#define MAX_RETRY_TIMES 30
#define WIFINAME "wifi0"

static int s_c_rpc_use_udp = 0;

int c_rpc_qcsapi_init()
{
	const char *host;
	CLIENT *clnt;
	int retry = 0;

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

int rpc_qcsapi_set_SSID(char *ssid)
{
	int ret;

	ret = qcsapi_wifi_set_SSID(WIFINAME, ssid);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_SSID error, return: %d\n", ret);
		return -1;
	}
	dbG("Set SSID as: %s\n", ssid);

	return 0;
}

int rpc_qcsapi_set_SSID_broadcast(char *option)
{
	int ret;
	int OPTION = 1 - atoi(option);

	ret = qcsapi_wifi_set_option(WIFINAME, qcsapi_SSID_broadcast, OPTION);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_option::SSID_broadcast error, return: %d\n", ret);
		return -1;
	}
	dbG("Set Broadcast SSID as: %s\n", OPTION ? "True" : "False");

	return 0;
}

int rpc_qcsapi_set_vht(char *mode)
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
		dbG("Qcsapi qcsapi_wifi_set_vht error, return: %d\n", ret);
		return -1;
	}
	dbG("Set wireless mode as: %s\n", VHT ? "11ac" : "11n");

	return 0;
}

int rpc_qcsapi_set_bw(char *bw)
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
		dbG("Qcsapi qcsapi_wifi_set_bw error, return: %d\n", ret);
		return -1;
	}
	dbG("Set bw as: %d MHz\n", BW);

	return 0;
}

int rpc_qcsapi_set_channel(char *chspec_buf)
{
	int ret;
	int channel = 0;

	channel = wf_chspec_ctlchan(wf_chspec_aton(chspec_buf));

	ret = qcsapi_wifi_set_channel(WIFINAME, channel);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_channel error, return: %d\n", ret);
		return -1;
	}
	dbG("Set channel as: %d\n", channel);

	return 0;
}

int rpc_qcsapi_set_beacon_type(char *auth_mode)
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

	ret = qcsapi_wifi_set_beacon_type(WIFINAME, p_new_beacon);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_beacon_type error, return: %d\n", ret);
		return -1;
	}
	dbG("Set beacon type as: %s\n", p_new_beacon);

	if (p_new_beacon) free(p_new_beacon);

	return 0;
}

int rpc_qcsapi_set_WPA_encryption_modes(char *crypto)
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

	ret = qcsapi_wifi_set_WPA_encryption_modes(WIFINAME, encryption_modes);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_WPA_encryption_modes error, return: %d\n", ret);
		return -1;
	}
	dbG("Set WPA encryption mode as: %s\n", encryption_modes);

	return 0;
}

int rpc_qcsapi_set_key_passphrase(char *wpa_psk)
{
	int ret;

	ret = qcsapi_wifi_set_key_passphrase(WIFINAME, 0, wpa_psk);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_key_passphrase error, return: %d\n", ret);
		return -1;
	}
	dbG("Set WPA preshared key as: %s\n", wpa_psk);

	return 0;
}

int rpc_qcsapi_set_dtim(char *dtim)
{
	int ret;
	int DTIM = atoi(dtim);

	ret = qcsapi_wifi_set_dtim(WIFINAME, DTIM);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_dtim error, return: %d\n", ret);
		return -1;
	}
	dbG("Set dtim as: %d\n", DTIM);

	return 0;
}

int rpc_qcsapi_set_beacon_interval(char *beacon_interval)
{
	int ret;
	int BCN = atoi(beacon_interval);

	ret = qcsapi_wifi_set_beacon_interval(WIFINAME, BCN);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_set_beacon_interval error, return: %d\n", ret);
		return -1;
	}
	dbG("Set beacon_interval as: %d\n", BCN);

	return 0;
}

int rpc_qcsapi_get_SSID(qcsapi_SSID *ssid)
{
	int ret;

	ret = qcsapi_wifi_get_SSID(WIFINAME, (char *) ssid);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_bw error, return: %d\n", ret);
		return -1;
	}

	return 0;
}

int rpc_qcsapi_get_SSID_broadcast(int *p_current_option)
{
	int ret;

	ret = qcsapi_wifi_get_option(WIFINAME, qcsapi_SSID_broadcast, p_current_option);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_option::SSID_broadcast error, return: %d\n", ret);
		return -1;
	}

	return 0;
}

int rpc_qcsapi_get_vht(qcsapi_unsigned_int *vht)
{
	int ret;

	ret = qcsapi_wifi_get_vht(WIFINAME, vht);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_vht error, return: %d\n", ret);
		return -1;
	}

	return 0;
}

int rpc_qcsapi_get_bw(qcsapi_unsigned_int *p_bw)
{
	int ret;

	ret = qcsapi_wifi_get_bw(WIFINAME, p_bw);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_bw error, return: %d\n", ret);
		return -1;
	}

	return 0;
}

int rpc_qcsapi_get_channel(qcsapi_unsigned_int *p_channel)
{
	int ret;

	ret = qcsapi_wifi_get_channel(WIFINAME, p_channel);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_channel error, return: %d\n", ret);
		return -1;
	}

	return 0;
}

int rpc_qcsapi_get_channel_list(string_1024* list_of_channels)
{
	int ret;

	ret = qcsapi_wifi_get_list_channels(WIFINAME, *list_of_channels);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_list_channels error, return: %d\n", ret);
		return -1;
	}

	return 0;
}

int rpc_qcsapi_get_beacon_type(char *p_current_beacon)
{
	int ret;

	ret = qcsapi_wifi_get_beacon_type(WIFINAME, p_current_beacon);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_beacon_type error, return: %d\n", ret);
		return -1;
	}

	return 0;
}

int rpc_qcsapi_get_WPA_encryption_modes(char *p_current_encryption_mode)
{
	int ret;

	ret = qcsapi_wifi_get_WPA_encryption_modes(WIFINAME, p_current_encryption_mode);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_WPA_encryption_modes error, return: %d\n", ret);
		return -1;
	}

	return 0;
}

int rpc_qcsapi_get_key_passphrase(char *p_current_key_passphrase)
{
	int ret;

	ret = qcsapi_wifi_get_key_passphrase(WIFINAME, 0, p_current_key_passphrase);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_key_passphrase error, return: %d\n", ret);
		return -1;
	}

	return 0;
}

int rpc_qcsapi_get_dtim(qcsapi_unsigned_int *p_dtim)
{
	int ret;

	ret = qcsapi_wifi_get_dtim(WIFINAME, p_dtim);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_dtim error, return: %d\n", ret);
		return -1;
	}

	return 0;
}

int rpc_qcsapi_get_beacon_interval(qcsapi_unsigned_int *p_beacon_interval)
{
	int ret;

	ret = qcsapi_wifi_get_beacon_interval(WIFINAME, p_beacon_interval);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_get_beacon_interval error, return: %d\n", ret);
		return -1;
	}

	return 0;
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
		dbG("Qcsapi qcsapi_wifi_get_list_channels error, return: %d\n", ret);
		goto ERROR;
	}

	p = strtok((char *) list_of_channels, ",");
	while (p)
	{
		if (i == 0)
			sprintf(tmp, "[\"%s\"", (char *)p);
		else
			sprintf(tmp,  "%s, \"%s\"", tmp, (char *)p);

		p = strtok(NULL, ",");
		i++;
	}

	if (i)
		sprintf(tmp,  "%s]", tmp);

ERROR:
	retval += websWrite(wp, "%s", tmp);
	return retval;
}

int rpc_qcsapi_restore_default_config(int flag)
{
	int ret;

	ret = qcsapi_restore_default_config(flag);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_restore_default_config error, return: %d\n", ret);
		return -1;
	}
	dbG("QTN restore default config successfully\n");

	return 0;
}