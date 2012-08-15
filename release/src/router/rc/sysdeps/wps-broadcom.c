/*
 * Miscellaneous services
 *
 * Copyright (C) 2009, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: services.c,v 1.100 2010/03/04 09:39:18 Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#include <rc.h>
#include <semaphore_mfp.h>
#include <shared.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <security_ipc.h>
#include <bcmutils.h>
#include <wlutils.h>
#include <wlscan.h>
#ifdef RTCONFIG_WPS
#include <wps_ui.h>
#endif

static int
set_wps_env(char *uibuf)
{
	int wps_fd = -1;
	struct sockaddr_in to;
	int sentBytes = 0;
	uint32 uilen = strlen(uibuf);

	//if (is_wps_enabled() == 0)
	//	return -1;

	if ((wps_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		goto exit;
	}
	
	/* send to WPS */
	to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	to.sin_family = AF_INET;
	to.sin_port = htons(WPS_UI_PORT);

	sentBytes = sendto(wps_fd, uibuf, uilen, 0, (struct sockaddr *) &to,
		sizeof(struct sockaddr_in));

	if (sentBytes != uilen) {
		goto exit;
	}

	/* Sleep 100 ms to make sure
	   WPS have received socket */
	usleep(100*1000);
	close(wps_fd);
	return 0;

exit:
	if (wps_fd >= 0)
		close(wps_fd);

	/* Show error message ?  */
	return -1;
}



/*
 * input variables:
 * 	nvram: wps_band: 
 * 	nvram: wps_action: WPS_UI_ACT_ADDENROLLEE/WPS_UI_ACT_CONFIGAe
 *	       (wps_method: (according to wps_sta_pin)
 *	nvram: wps_sta_pin:
 *	nvram: wps_version2: 
 *	nvram: wps_autho_sta_mac: 
 *
 * output variables:
 * 	wps_proc_status
 */

int 
start_wps_method(void)
{
	int wps_band;
	int wps_action;
	int wps_method;
	char *wps_sta_pin;
	char prefix[]="wlXXXXXX_", tmp[100];
	char buf[256] = "SET ";
	int len = 4;

	if(getpid()!=1) {
		notify_rc("start_wps_method");
		return 0;
	}

	wps_band = nvram_get_int("wps_band");
	snprintf(prefix, sizeof(prefix), "wl%d_", wps_band);
	wps_action = nvram_get_int("wps_action");
	wps_method = nvram_get_int("wps_method"); // useless
	wps_sta_pin = nvram_safe_get("wps_sta_pin");

	if(strlen(wps_sta_pin) && strcmp(wps_sta_pin, "00000000") && (wl_wpsPincheck(wps_sta_pin) == 0))
		len += sprintf(buf + len, "wps_method=%d ", WPS_UI_METHOD_PIN);	
	else
		len += sprintf(buf + len, "wps_method=%d ", WPS_UI_METHOD_PBC);
	
	if(nvram_match("wps_version2", "enabled") && strlen(nvram_safe_get("wps_autho_sta_mac")))
		len += sprintf(buf + len, "wps_autho_sta_mac=%s ", nvram_safe_get("wps_autho_sta_mac"));

	if(strlen(wps_sta_pin))
		len += sprintf(buf + len, "wps_sta_pin=%s ", wps_sta_pin);
	else
		len += sprintf(buf + len, "wps_sta_pin=00000000 ");

//	len += sprintf(buf + len, "wps_action=%d ", wps_action);
	len += sprintf(buf + len, "wps_action=%d ", WPS_UI_ACT_ADDENROLLEE);

	len += sprintf(buf + len, "wps_config_command=%d ", WPS_UI_CMD_START);

	nvram_set("wps_proc_status", "0");

	len += sprintf(buf + len, "wps_pbc_method=%d ", WPS_UI_PBC_SW);
	len += sprintf(buf + len, "wps_ifname=%s ", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));

	dbG("wps env buffer: %s\n", buf);

	return set_wps_env(buf);
}

int 
stop_wps_method(void)
{
	char buf[256] = "SET ";
	int len = 4;

	if(getpid()!=1) {
		notify_rc("stop_wps_method");
		return 0;
	}

	len += sprintf(buf + len, "wps_config_command=%d ", WPS_UI_CMD_STOP);
	len += sprintf(buf + len, "wps_action=%d ", WPS_UI_ACT_NONE);

	return set_wps_env(buf);
}

int is_wps_stopped(void)
{
	int status, ret = 1;
	status = nvram_get_int("wps_proc_status");

	switch (status) {
		case 1: /* WPS_ASSOCIATED */
			fprintf(stderr, "Processing WPS start...\n");
			ret = 0;
			break;
		case 2: /* WPS_OK */
		case 7: /* WPS_MSGDONE */
			fprintf(stderr, "WPS Success\n");
			break;
		case 3: /* WPS_MSG_ERR */
			fprintf(stderr, "WPS Fail due to message exange error!\n");
			break;
		case 4: /* WPS_TIMEOUT */
			fprintf(stderr, "WPS Fail due to time out!\n");
			break;
		default:
			ret = 0;
			break;
	}

	return ret;
	// TODO: handle enrollee 
}

#ifdef RTCONFIG_WIRELESSREPEATER
char *wlc_nvname(char *keyword) 
{
	return(wl_nvname(keyword, nvram_get_int("wlc_band"), -1));
}

static const unsigned char WPA_OUT_TYPE[] = { 0x00, 0x50, 0xf2, 1 };

int wpa_key_mgmt_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, WPA_AUTH_KEY_MGMT_UNSPEC_802_1X, WPA_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_IEEE8021X_;
	if (memcmp(s, WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X, WPA_SELECTOR_LEN) ==
	    0)
		return WPA_KEY_MGMT_PSK_;
	if (memcmp(s, WPA_AUTH_KEY_MGMT_NONE, WPA_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_WPA_NONE_;
	return 0;
}

int rsn_key_mgmt_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X, RSN_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_IEEE8021X2_;
	if (memcmp(s, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X, RSN_SELECTOR_LEN) ==
	    0)
		return WPA_KEY_MGMT_PSK2_;
	return 0;
}

int wpa_selector_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, WPA_CIPHER_SUITE_NONE, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_NONE_;
	if (memcmp(s, WPA_CIPHER_SUITE_WEP40, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP40_;
	if (memcmp(s, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_TKIP_;
	if (memcmp(s, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_CCMP_;
	if (memcmp(s, WPA_CIPHER_SUITE_WEP104, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP104_;
	return 0;
}

int rsn_selector_to_bitfield(const unsigned char *s)
{
	if (memcmp(s, RSN_CIPHER_SUITE_NONE, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_NONE_;
	if (memcmp(s, RSN_CIPHER_SUITE_WEP40, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP40_;
	if (memcmp(s, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_TKIP_;
	if (memcmp(s, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_CCMP_;
	if (memcmp(s, RSN_CIPHER_SUITE_WEP104, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP104_;
	return 0;
}

int wpa_parse_wpa_ie_wpa(const unsigned char *wpa_ie, size_t wpa_ie_len, struct wpa_ie_data *data)
{
	const struct wpa_ie_hdr *hdr;
	const unsigned char *pos;
	int left;
	int i, count;

	data->proto = WPA_PROTO_WPA_;
	data->pairwise_cipher = WPA_CIPHER_TKIP_;
	data->group_cipher = WPA_CIPHER_TKIP_;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X_;
	data->capabilities = 0;
	data->pmkid = NULL;
	data->num_pmkid = 0;

	if (wpa_ie_len == 0) {
		/* No WPA IE - fail silently */
		return -1;
	}

	if (wpa_ie_len < sizeof(struct wpa_ie_hdr)) {
//		fprintf(stderr, "ie len too short %lu", (unsigned long) wpa_ie_len);
		return -1;
	}

	hdr = (const struct wpa_ie_hdr *) wpa_ie;

	if (hdr->elem_id != DOT11_MNG_WPA_ID ||
	    hdr->len != wpa_ie_len - 2 ||
	    memcmp(&hdr->oui, WPA_OUI_TYPE_ARR, WPA_SELECTOR_LEN) != 0 ||
	    WPA_GET_LE16(hdr->version) != WPA_VERSION_) {
//		fprintf(stderr, "malformed ie or unknown version");
		return -1;
	}

	pos = (const unsigned char *) (hdr + 1);
	left = wpa_ie_len - sizeof(*hdr);

	if (left >= WPA_SELECTOR_LEN) {
		data->group_cipher = wpa_selector_to_bitfield(pos);
		pos += WPA_SELECTOR_LEN;
		left -= WPA_SELECTOR_LEN;
	} else if (left > 0) {
//		fprintf(stderr, "ie length mismatch, %u too much", left);
		return -1;
	}

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * WPA_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (pairwise), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= wpa_selector_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for key mgmt)");
		return -1;
	}

	if (left >= 2) {
		data->key_mgmt = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * WPA_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (key mgmt), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->key_mgmt |= wpa_key_mgmt_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for capabilities)");
		return -1;
	}

	if (left >= 2) {
		data->capabilities = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
	}

	if (left > 0) {
//		fprintf(stderr, "ie has %u trailing bytes", left);
		return -1;
	}

	return 0;
}

int wpa_parse_wpa_ie_rsn(const unsigned char *rsn_ie, size_t rsn_ie_len, struct wpa_ie_data *data)
{
	const struct rsn_ie_hdr *hdr;
	const unsigned char *pos;
	int left;
	int i, count;

	data->proto = WPA_PROTO_RSN_;
	data->pairwise_cipher = WPA_CIPHER_CCMP_;
	data->group_cipher = WPA_CIPHER_CCMP_;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X2_;
	data->capabilities = 0;
	data->pmkid = NULL;
	data->num_pmkid = 0;

	if (rsn_ie_len == 0) {
		/* No RSN IE - fail silently */
		return -1;
	}

	if (rsn_ie_len < sizeof(struct rsn_ie_hdr)) {
//		fprintf(stderr, "ie len too short %lu", (unsigned long) rsn_ie_len);
		return -1;
	}

	hdr = (const struct rsn_ie_hdr *) rsn_ie;

	if (hdr->elem_id != DOT11_MNG_RSN_ID ||
	    hdr->len != rsn_ie_len - 2 ||
	    WPA_GET_LE16(hdr->version) != RSN_VERSION_) {
//		fprintf(stderr, "malformed ie or unknown version");
		return -1;
	}

	pos = (const unsigned char *) (hdr + 1);
	left = rsn_ie_len - sizeof(*hdr);

	if (left >= RSN_SELECTOR_LEN) {
		data->group_cipher = rsn_selector_to_bitfield(pos);
		pos += RSN_SELECTOR_LEN;
		left -= RSN_SELECTOR_LEN;
	} else if (left > 0) {
//		fprintf(stderr, "ie length mismatch, %u too much", left);
		return -1;
	}

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * RSN_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (pairwise), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= rsn_selector_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for key mgmt)");
		return -1;
	}

	if (left >= 2) {
		data->key_mgmt = 0;
		count = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * RSN_SELECTOR_LEN) {
//			fprintf(stderr, "ie count botch (key mgmt), "
//				   "count %u left %u", count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->key_mgmt |= rsn_key_mgmt_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1) {
//		fprintf(stderr, "ie too short (for capabilities)");
		return -1;
	}

	if (left >= 2) {
		data->capabilities = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
	}

	if (left >= 2) {
		data->num_pmkid = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
		if (left < data->num_pmkid * PMKID_LEN) {
//			fprintf(stderr, "PMKID underflow "
//				   "(num_pmkid=%d left=%d)", data->num_pmkid, left);
			data->num_pmkid = 0;
		} else {
			data->pmkid = pos;
			pos += data->num_pmkid * PMKID_LEN;
			left -= data->num_pmkid * PMKID_LEN;
		}
	}

	if (left > 0) {
//		fprintf(stderr, "ie has %u trailing bytes - ignored", left);
	}

	return 0;
}

int wpa_parse_wpa_ie(const unsigned char *wpa_ie, size_t wpa_ie_len,
		     struct wpa_ie_data *data)
{
	if (wpa_ie_len >= 1 && wpa_ie[0] == DOT11_MNG_RSN_ID)
		return wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, data);
	else
		return wpa_parse_wpa_ie_wpa(wpa_ie, wpa_ie_len, data);
}

static const char * wpa_key_mgmt_txt(int key_mgmt, int proto)
{
	switch (key_mgmt) {
	case WPA_KEY_MGMT_IEEE8021X_:
/*
		return proto == WPA_PROTO_RSN_ ?
			"WPA2/IEEE 802.1X/EAP" : "WPA/IEEE 802.1X/EAP";
*/
		return "WPA-Enterprise";
	case WPA_KEY_MGMT_IEEE8021X2_:
		return "WPA2-Enterprise";
	case WPA_KEY_MGMT_PSK_:
/*
		return proto == WPA_PROTO_RSN_ ?
			"WPA2-PSK" : "WPA-PSK";
*/
		return "WPA-Personal";
	case WPA_KEY_MGMT_PSK2_:
		return "WPA2-Personal";
	case WPA_KEY_MGMT_NONE_:
		return "NONE";
	case WPA_KEY_MGMT_IEEE8021X_NO_WPA_:
//		return "IEEE 802.1X (no WPA)";
		return "IEEE 802.1X";
	default:
		return "Unknown";
	}
}

static const char * wpa_cipher_txt(int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_NONE_:
		return "NONE";
	case WPA_CIPHER_WEP40_:
		return "WEP-40";
	case WPA_CIPHER_WEP104_:
		return "WEP-104";
	case WPA_CIPHER_TKIP_:
		return "TKIP";
	case WPA_CIPHER_CCMP_:
//		return "CCMP";
		return "AES";
	case (WPA_CIPHER_TKIP_|WPA_CIPHER_CCMP_):
		return "TKIP+AES";
	default:
		return "Unknown";
	}
}

int wlcscan_core(char *ofile, char *wif)
{
	int ret, i, k, left, ht_extcha;
	int retval = 0, ap_count = 0, idx_same = -1, count = 0;
	unsigned char *bssidp;
	char *info_b;
	unsigned char rate;
	unsigned char bssid[6];
	char macstr[18];
	char ure_mac[18];
	char ssid_str[256];
	wl_scan_results_t *result;
	wl_bss_info_t *info;
	struct bss_ie_hdr *ie;
	NDIS_802_11_NETWORK_TYPE NetWorkType;
	struct maclist *authorized;
	int maclist_size;
	int max_sta_count = 128;
	int wl_authorized = 0;
	wl_scan_params_t *params;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + NUMCHANS * sizeof(uint16);
	FILE *fp;

	retval = 0;

	params = (wl_scan_params_t*)malloc(params_size);
	if (params == NULL)
		return retval;

	memset(params, 0, params_size);
	params->bss_type = DOT11_BSSTYPE_INFRASTRUCTURE;
	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
//	params->scan_type = -1;
	params->scan_type = DOT11_SCANTYPE_ACTIVE;
//	params->scan_type = DOT11_SCANTYPE_PASSIVE;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;

	while ((ret = wl_ioctl(wif, WLC_SCAN, params, params_size)) < 0 &&
				count++ < 2){
		fprintf(stderr, "[rc] set scan command failed, retry %d\n", count);
		sleep(1);
	}

	free(params);

	nvram_set("ap_selecting", "1");
	fprintf(stderr, "[rc] Please wait 4 seconds\n\n");
	sleep(4);
	nvram_set("ap_selecting", "0");

	if (ret == 0){
		count = 0;

		result = (wl_scan_results_t *)buf;
		result->buflen = WLC_IOCTL_MAXLEN - sizeof(result);

		while ((ret = wl_ioctl(wif, WLC_SCAN_RESULTS, result, WLC_IOCTL_MAXLEN)) < 0 && count++ < 2)
		{
			fprintf(stderr, "[rc] set scan results command failed, retry %d\n", count);
			sleep(1);
		}

		if (ret == 0)
		{
			info = &(result->bss_info[0]);
			info_b = (unsigned char *)info;
			
			for(i = 0; i < result->count; i++)
			{
				if (info->SSID_len > 32/* || info->SSID_len == 0*/)
					goto next_info;
				bssidp = (unsigned char *)&info->BSSID;
				sprintf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X",
										(unsigned char)bssidp[0],
										(unsigned char)bssidp[1],
										(unsigned char)bssidp[2],
										(unsigned char)bssidp[3],
										(unsigned char)bssidp[4],
										(unsigned char)bssidp[5]);

				idx_same = -1;
				for (k = 0; k < ap_count; k++){
					/* deal with old version of Broadcom Multiple SSID
						(share the same BSSID) */
					if(strcmp(apinfos[k].BSSID, macstr) == 0 &&
						strcmp(apinfos[k].SSID, info->SSID) == 0){
						idx_same = k;
						break;
					}
				}

				if (idx_same != -1)
				{
					if (info->RSSI >= -50)
						apinfos[idx_same].RSSI_Quality = 100;
					else if (info->RSSI >= -80)	// between -50 ~ -80dbm
						apinfos[idx_same].RSSI_Quality = (int)(24 + ((info->RSSI + 80) * 26)/10);
					else if (info->RSSI >= -90)	// between -80 ~ -90dbm
						apinfos[idx_same].RSSI_Quality = (int)(((info->RSSI + 90) * 26)/10);
					else					// < -84 dbm
						apinfos[idx_same].RSSI_Quality = 0;
				}
				else
				{
					strcpy(apinfos[ap_count].BSSID, macstr);
//					strcpy(apinfos[ap_count].SSID, info->SSID);
					memset(apinfos[ap_count].SSID, 0x0, 33);
					memcpy(apinfos[ap_count].SSID, info->SSID, info->SSID_len);
//					apinfos[ap_count].channel = ((wl_bss_info_107_t *) info)->channel;
					apinfos[ap_count].channel = info->chanspec;
					apinfos[ap_count].ctl_ch = info->ctl_ch;

					if (info->RSSI >= -50)
						apinfos[ap_count].RSSI_Quality = 100;
					else if (info->RSSI >= -80)	// between -50 ~ -80dbm
						apinfos[ap_count].RSSI_Quality = (int)(24 + ((info->RSSI + 80) * 26)/10);
					else if (info->RSSI >= -90)	// between -80 ~ -90dbm
						apinfos[ap_count].RSSI_Quality = (int)(((info->RSSI + 90) * 26)/10);
					else					// < -84 dbm
						apinfos[ap_count].RSSI_Quality = 0;

					if ((info->capability & 0x10) == 0x10)
						apinfos[ap_count].wep = 1;
					else
						apinfos[ap_count].wep = 0;
					apinfos[ap_count].wpa = 0;

/*
					unsigned char *RATESET = &info->rateset;
					for (k = 0; k < 18; k++)
						fprintf(stderr, "%02x ", (unsigned char)RATESET[k]);
					fprintf(stderr, "\n");
*/

					NetWorkType = Ndis802_11DS;
//					if (((wl_bss_info_107_t *) info)->channel <= 14)
					if ((uint8)info->chanspec <= 14)
					{
						for (k = 0; k < info->rateset.count; k++)
						{
							rate = info->rateset.rates[k] & 0x7f;	// Mask out basic rate set bit
							if ((rate == 2) || (rate == 4) || (rate == 11) || (rate == 22))
								continue;
							else
							{
								NetWorkType = Ndis802_11OFDM24;
								break;
							}	
						}
					}
					else
						NetWorkType = Ndis802_11OFDM5;

					if (info->n_cap)
					{
						if (NetWorkType == Ndis802_11OFDM5)
							NetWorkType = Ndis802_11OFDM5_N;
						else
							NetWorkType = Ndis802_11OFDM24_N;
					}

					apinfos[ap_count].NetworkType = NetWorkType;

					ap_count++;
				}

				ie = (struct bss_ie_hdr *) ((unsigned char *) info + sizeof(*info));
				for (left = info->ie_length; left > 0; // look for RSN IE first
					left -= (ie->len + 2), ie = (struct bss_ie_hdr *) ((unsigned char *) ie + 2 + ie->len)) 
				{
					if (ie->elem_id != DOT11_MNG_RSN_ID)
						continue;

					if (wpa_parse_wpa_ie(&ie->elem_id, ie->len + 2, &apinfos[ap_count - 1].wid) == 0)
					{
						apinfos[ap_count-1].wpa = 1;
						goto next_info;
					}
				}

				ie = (struct bss_ie_hdr *) ((unsigned char *) info + sizeof(*info));
				for (left = info->ie_length; left > 0; // then look for WPA IE
					left -= (ie->len + 2), ie = (struct bss_ie_hdr *) ((unsigned char *) ie + 2 + ie->len)) 
				{
					if (ie->elem_id != DOT11_MNG_WPA_ID)
						continue;

					if (wpa_parse_wpa_ie(&ie->elem_id, ie->len + 2, &apinfos[ap_count-1].wid) == 0)
					{
						apinfos[ap_count-1].wpa = 1;
						break;
					}
				}

next_info:
				info = (wl_bss_info_t *) ((unsigned char *) info + info->length);
			}
		}
	}

	/* Print scanning result to console */
	if (ap_count == 0){
		fprintf(stderr, "[wlc] No AP found!\n");
	}else{
		fprintf(stderr, "%-4s%-3s%-33s%-18s%-9s%-16s%-9s%8s%3s%3s\n",
				"idx", "CH", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode", "CC", "EC");
		for (k = 0; k < ap_count; k++)
		{
			fprintf(stderr, "%2d. ", k + 1);
			fprintf(stderr, "%2d ", apinfos[k].channel);
			fprintf(stderr, "%-33s", apinfos[k].SSID);
			fprintf(stderr, "%-18s", apinfos[k].BSSID);

			if (apinfos[k].wpa == 1)
				fprintf(stderr, "%-9s%-16s", wpa_cipher_txt(apinfos[k].wid.pairwise_cipher), wpa_key_mgmt_txt(apinfos[k].wid.key_mgmt, apinfos[k].wid.proto));
			else if (apinfos[k].wep == 1)
				fprintf(stderr, "WEP      Unknown         ");
			else
				fprintf(stderr, "NONE     Open System     ");
			fprintf(stderr, "%9d ", apinfos[k].RSSI_Quality);

			if (apinfos[k].NetworkType == Ndis802_11FH || apinfos[k].NetworkType == Ndis802_11DS)
				fprintf(stderr, "%-7s", "11b");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM5)
				fprintf(stderr, "%-7s", "11a");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM5_N)
				fprintf(stderr, "%-7s", "11a/n");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM24)
				fprintf(stderr, "%-7s", "11b/g");
			else if (apinfos[k].NetworkType == Ndis802_11OFDM24_N)
				fprintf(stderr, "%-7s", "11b/g/n");
			else
				fprintf(stderr, "%-7s", "unknown");

			fprintf(stderr, "%3d", apinfos[k].ctl_ch);

			if (	((apinfos[k].NetworkType == Ndis802_11OFDM5_N) ||
					 (apinfos[k].NetworkType == Ndis802_11OFDM24_N)) &&
					(apinfos[k].channel != apinfos[k].ctl_ch)	){
				if (apinfos[k].ctl_ch < apinfos[k].channel)
					ht_extcha = 1;
				else
					ht_extcha = 0;

				fprintf(stderr, "%3d", ht_extcha);
			}

			fprintf(stderr, "\n");
		}
	}

	ret = wl_ioctl(wif, WLC_GET_BSSID, bssid, sizeof(bssid));
	memset(ure_mac, 0x0, 18);
	if (!ret){
		if ( !(!bssid[0] && !bssid[1] && !bssid[2] && !bssid[3] && !bssid[4] && !bssid[5]) ){
			sprintf(ure_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
										(unsigned char)bssid[0],
										(unsigned char)bssid[1],
										(unsigned char)bssid[2],
										(unsigned char)bssid[3],
										(unsigned char)bssid[4],
										(unsigned char)bssid[5]);
		}
	}

	if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")){
		maclist_size = sizeof(authorized->count) + max_sta_count * sizeof(struct ether_addr);
		authorized = malloc(maclist_size);

		// query wl for authorized sta list
		strcpy((char*)authorized, "autho_sta_list");
		if (!wl_ioctl(wif, WLC_GET_VAR, authorized, maclist_size)){
			if (authorized->count > 0) wl_authorized = 1;
		}

		if (authorized) free(authorized);
	}

	/* Print scanning result to web format */
	if (ap_count > 0){
		/* write pid */
		spinlock_lock(SPINLOCK_SiteSurvey);
		if ((fp = fopen(ofile, "a")) == NULL){
			printf("[wlcscan] Output %s error\n", ofile);
		}else{
			for (i = 0; i < ap_count; i++){
				if(apinfos[i].channel < 0 ){
					fprintf(fp, "\"ERR_BNAD\",");
				}else if( apinfos[i].channel > 0 &&
							 apinfos[i].channel < 14){
					fprintf(fp, "\"2G\",");
				}else if( apinfos[i].channel > 14 &&
							 apinfos[i].channel < 166 ){
					fprintf(fp, "\"5G\",");
				}else{
					fprintf(fp, "\"ERR_BNAD\",");
				}

				if (strlen(apinfos[i].SSID) == 0){
					fprintf(fp, ",");
				}else{
					memset(ssid_str, 0, sizeof(ssid_str));
					char_to_ascii(ssid_str, apinfos[i].SSID);
					fprintf(fp, "\"%s\",", ssid_str);
				}

				fprintf(fp, "\"%d\",", apinfos[i].channel);

				if (apinfos[i].wpa == 1){
					if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X_)
						fprintf(fp, "\"%s\",", "WPA");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X2_)
						fprintf(fp, "\"%s\",", "WPA2");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_PSK_)
						fprintf(fp, "\"%s\",", "WPA-PSK");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_PSK2_)
						fprintf(fp, "\"%s\",", "WPA2-PSK");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_NONE_)
						fprintf(fp, "\"%s\",", "NONE");
					else if (apinfos[i].wid.key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA_)
						fprintf(fp, "\"%s\",", "IEEE 802.1X");
					else
						fprintf(fp, "\"%s\",", "Unknown");
				}else if (apinfos[i].wep == 1){
					fprintf(fp, "\"%s\",", "Unknown");
				}else{
					fprintf(fp, "\"%s\",", "Open System");
				}

				if (apinfos[i].wpa == 1){
					if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_NONE_)
						fprintf(fp, "\"%s\",", "NONE");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_WEP40_)
						fprintf(fp, "\"%s\",", "WEP");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_WEP104_)
						fprintf(fp, "\"%s\",", "WEP");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_TKIP_)
						fprintf(fp, "\"%s\",", "TKIP");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_CCMP_)
						fprintf(fp, "\"%s\",", "AES");
					else if (apinfos[i].wid.pairwise_cipher == WPA_CIPHER_TKIP_|WPA_CIPHER_CCMP_)
						fprintf(fp, "\"%s\",", "TKIP+AES");
					else
						fprintf(fp, "\"%\"s,", "Unknown");
				}else if (apinfos[i].wep == 1){
					fprintf(fp, "\"%s\",", "WEP");
				}else{
					fprintf(fp, "\"%s\",", "NONE");
				}

				fprintf(fp, "\"%d\",", apinfos[i].RSSI_Quality);
				fprintf(fp, "\"%s\",", apinfos[i].BSSID);

				if (apinfos[i].NetworkType == Ndis802_11FH || apinfos[i].NetworkType == Ndis802_11DS)
					fprintf(fp, "\"%s\",", "b");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM5)
					fprintf(fp, "\"%s\",", "a");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM5_N)
					fprintf(fp, "\"%s\",", "an");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM24)
					fprintf(fp, "\"%s\",", "bg");
				else if (apinfos[i].NetworkType == Ndis802_11OFDM24_N)
					fprintf(fp, "\"%s\",", "bgn");
				else
					fprintf(fp, "\"%s\",", "");

				if (strcmp(nvram_safe_get(wlc_nvname("ssid")), apinfos[i].SSID)){
					if (strcmp(apinfos[i].SSID, ""))
						fprintf(fp, "\"%s\"", "0");				// none
					else if (!strcmp(ure_mac, apinfos[i].BSSID)){
						// hidden AP (null SSID)
						if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")){
							if (wl_authorized){
								// in profile, connected
								fprintf(fp, "\"%s\"", "4");
							}else{
								// in profile, connecting
								fprintf(fp, "\"%s\"", "5");
							}
						}else{
							// in profile, connected
							fprintf(fp, "\"%s\"", "4");
						}
					}else{
						// hidden AP (null SSID)
						fprintf(fp, "\"%s\"", "0");				// none
					}
				}else if (!strcmp(nvram_safe_get(wlc_nvname("ssid")), apinfos[i].SSID)){
					if (!strlen(ure_mac)){
						// in profile, disconnected
						fprintf(fp, "\"%s\",", "1");
					}else if (!strcmp(ure_mac, apinfos[i].BSSID)){
						if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")){
							if (wl_authorized){
								// in profile, connected
								fprintf(fp, "\"%s\"", "2");
							}else{
								// in profile, connecting
								fprintf(fp, "\"%s\"", "3");
							}
						}else{
							// in profile, connected
							fprintf(fp, "\"%s\"", "2");
						}
					}else{
						fprintf(fp, "\"%s\"", "0");				// impossible...
					}
				}else{
					// wl0_ssid is empty
					fprintf(fp, "\"%s\"", "0");
				}

				if (i == ap_count - 1){
					fprintf(fp, "\n");
				}else{
					fprintf(fp, "\n");
				}
			}	/* for */
			fclose(fp);
		}
		spinlock_unlock(SPINLOCK_SiteSurvey);
	}	/* if */

	return retval;
}

/* 
 *  Return value:
 *  	2 = successfully connected to parent AP 
 */  
int get_wlc_status(char *wif)
{
	char ure_mac[18];
	unsigned char bssid[6];
	unsigned char SEND_NULLDATA[]={0x73, 0x65, 0x6e, 0x64, 0x5f, 0x6e, 0x75, 0x6c, 0x6c, 0x64, 0x61, 0x74, 0x61, 0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	struct maclist *authorized;
	int maclist_size;
	int max_sta_count = 128;
	int wl_authorized = 0;
	int wl_associated = 0;
	int wl_psk = 0;
	wlc_ssid_t wst = {0, ""};

	wl_psk = strstr(nvram_safe_get(wlc_nvname("akm")), "psk") ? 1 : 0;

	if (wl_ioctl(wif, WLC_GET_SSID, &wst, sizeof(wst))){
		fprintf(stderr, "[wlc] WLC_GET_SSID error\n");
		goto wl_ioctl_error;
	}

	memset(ure_mac, 0x0, 18);
	if (!wl_ioctl(wif, WLC_GET_BSSID, bssid, sizeof(bssid))){
		if ( !(!bssid[0] && !bssid[1] && !bssid[2] &&
				!bssid[3] && !bssid[4] && !bssid[5]) ){
			wl_associated = 1;
			sprintf(ure_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
					(unsigned char)bssid[0],
					(unsigned char)bssid[1],
					(unsigned char)bssid[2],
					(unsigned char)bssid[3],
					(unsigned char)bssid[4],
					(unsigned char)bssid[5]);
			memcpy(SEND_NULLDATA + 14, bssid, 6);
		}
	}else{
		fprintf(stderr, "[wlc] WLC_GET_BSSID error\n");
		goto wl_ioctl_error;
	}

	if (wl_psk){
		maclist_size = sizeof(authorized->count) +
							max_sta_count * sizeof(struct ether_addr);
		authorized = malloc(maclist_size);

		if (authorized){
			// query wl for authorized sta list
			strcpy((char*)authorized, "autho_sta_list");

			if (!wl_ioctl(wif, WLC_GET_VAR, authorized, maclist_size)){
				if (authorized->count > 0) wl_authorized = 1;
				free(authorized);
			}else{
				free(authorized);
				fprintf(stderr, "[wlc] Authorized failed\n");
				goto wl_ioctl_error;
			}
		}
	}

	if(!wl_associated){
		fprintf(stderr, "[wlc] not wl_associated\n");
	}

	fprintf(stderr, "[wlc] wl-associated [%d]\n", wl_associated);
	fprintf(stderr, "[wlc] %s\n", wst.SSID);
	fprintf(stderr, "[wlc] %s\n", nvram_safe_get(wlc_nvname("ssid")));
		
	if (wl_associated &&
		!strncmp(wst.SSID, nvram_safe_get(wlc_nvname("ssid")), wst.SSID_len)){
		if (wl_psk){
			if (wl_authorized){
				fprintf(stderr, "[wlc] wl_authorized\n");
				return 2;
			}else{
				fprintf(stderr, "[wlc] not wl_authorized\n");
				return 1;
			}
		}else{
			fprintf(stderr, "[wlc] wl_psk:[%d]\n", wl_psk);
			return 2;
		}
	}else{
		fprintf(stderr, "[wlc] Not associated\n");
		return 0;
	}

wl_ioctl_error:
	return 0;
}

// TODO: wlcconnect_main
//	wireless ap monitor to connect to ap
//	when wlc_list, then connect to it according to priority
int wlcconnect_core(void)
{
	int ret;
	unsigned int count;
	char word[256], *next;
	unsigned char SEND_NULLDATA[]={ 0x73, 0x65, 0x6e, 0x64,
					0x5f, 0x6e, 0x75, 0x6c,
					0x6c, 0x64, 0x61, 0x74,
					0x61, 0x00, 0xff, 0xff,
					0xff, 0xff, 0xff, 0xff};
	int unit;

	count = 0;
	unit=0;
	/* return WLC connection status */
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		// only one client in a system
		if(is_ure(unit)) {
			fprintf(stderr, "[rc] [%s] is URE mode\n", word);
			while(count<4){
				count++;
				// wl send_nulldata xx:xx:xx:xx:xx:xx
				wl_ioctl(word, WLC_SET_VAR, SEND_NULLDATA,
				sizeof(SEND_NULLDATA));
				sleep(1);
			}
			ret = get_wlc_status(word);
			fprintf(stderr, "[wlc] get_wlc_status:[%d]\n", ret);
		}else{
			fprintf(stderr, "[rc] [%s] is not URE mode\n", word);
		}
		unit++;
	}

	return ret;
}

#endif
