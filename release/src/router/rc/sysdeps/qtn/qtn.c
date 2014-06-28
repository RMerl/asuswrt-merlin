#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bcmnvram.h>
#include <shutils.h>

#include "qcsapi_output.h"
#include "qcsapi_rpc_common/client/find_host_addr.h"

#include "qcsapi.h"
#include "qcsapi_rpc/client/qcsapi_rpc_client.h"
#include "qcsapi_rpc/generated/qcsapi_rpc.h"
#include "qcsapi_driver.h"
#include "call_qcsapi.h"

#define MAX_RETRY_TIMES 30

static int s_c_rpc_use_udp = 0;

static int lock_qtn_apscan = -1;

extern int isValidCountryCode(const char *Ccode);
extern int isValidRegrev(char *regrev);
extern int isValidMacAddr(const char* mac);
extern int file_lock(char *tag);
extern void file_unlock(int lockfd);
extern void char_to_ascii(const char *output, const char *input);

char cmd[32];
#if 0
void inc_mac(char *mac, int plus);
#endif
int rpc_qcsapi_init()
{
	const char *host;
	CLIENT *clnt;
	int retry = 0;

	/* setup RPC based on udp protocol */
	do {
		// remove due to ATE command output format
		// fprintf(stderr, "#%d attempt to create RPC connection\n", retry + 1);

		host = client_qcsapi_find_host_addr(0, NULL);
		if (!host) {
			fprintf(stderr, "Cannot find the host\n");
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

int
setCountryCode_5G_qtn(const char *cc)
{
	int ret;
	char value[20] = {0};

	if( cc==NULL || !isValidCountryCode(cc) )
		return 0;

	ret = rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_update_parameter("ccode_5g", cc);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_get_parameter("ccode_5g", value, sizeof(value));
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}

	sprintf(cmd, "asuscfe1:ccode=%s", cc);
	eval("nvram", "set", cmd );
	puts(nvram_safe_get("1:ccode"));

	return 1;
}

int
getCountryCode_5G_qtn(void)
{
	puts(nvram_safe_get("1:ccode"));

	return 0;
}

int setRegrev_5G_qtn(const char *regrev)
{
	int ret;
	char value[20] = {0};

	if( regrev==NULL || !isValidRegrev(regrev) )
		return 0;

	ret = rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_update_parameter("regrev_5g", regrev);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_get_parameter("regrev_5g", value, sizeof(value));
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}

	memset(cmd, 0, 32);
	sprintf(cmd, "asuscfe1:regrev=%s", regrev);
	eval("nvram", "set", cmd );
	puts(nvram_safe_get("1:regrev"));
	return 1;
}

int
getRegrev_5G_qtn(void)
{
	puts(nvram_safe_get("1:regrev"));
	return 0;
}

int setMAC_5G_qtn(const char *mac)
{
	int ret;
	char cmd_l[64];
	char value[20] = {0};

	if( mac==NULL || !isValidMacAddr(mac) )
		return 0;

	ret = rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_update_parameter("ethaddr", mac);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
#if 0
	inc_mac(mac, 1);
#endif
	ret = qcsapi_bootcfg_update_parameter("wifiaddr", mac);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_get_parameter("ethaddr", value, sizeof(value));
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}

	memset(cmd_l, 0, 64);
	sprintf(cmd_l, "asuscfe1:macaddr=%s", mac);
	eval("nvram", "set", cmd_l );
	// puts(nvram_safe_get("1:macaddr"));

	puts(value);
	return 1;
}

int getMAC_5G_qtn(void)
{
	int ret;
	char value[20] = {0};

	ret = rpc_qcsapi_init();
	ret = qcsapi_bootcfg_get_parameter("ethaddr", value, sizeof(value));
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	puts(value);
	return 1;
}

//int start_wireless_qtn(void)
//{
//	int ret;
//
//	ret = rpc_qcsapi_init();
//
//	ret = qcsapi_wifi_set_SSID(WIFINAME,nvram_safe_get("wl1_ssid"));
//	if(!nvram_match("wl1_auth_mode_x", "open")){
//		rpc_qcsapi_set_beacon_type(nvram_safe_get("wl1_auth_mode_x"));
//		rpc_qcsapi_set_WPA_encryption_modes(nvram_safe_get("wl1_crypto"));
//		rpc_qcsapi_set_key_passphrase(nvram_safe_get("wl1_wpa_psk"));
//	}
//	return 1;
//}

int setAllLedOn_qtn(void)
{
	int ret;

	ret = rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_led_set(1, 1);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_wifi_run_script("set_test_mode", "lan4_led_ctrl on");
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	return 0;
}

int setAllLedOff_qtn(void)
{
	int ret;

	ret = rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_led_set(1, 0);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}

	ret = qcsapi_wifi_run_script("set_test_mode", "lan4_led_ctrl off");
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	return 0;
}

int Get_channel_list_qtn(int unit)
{
	int ret;
	string_1024 list_of_channels;
	char cur_ccode[20] = {0};

	ret = rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_wifi_get_regulatory_region("wifi0", cur_ccode);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_regulatory_get_list_regulatory_channels(cur_ccode, 20 /* bw */, list_of_channels);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	puts(list_of_channels);

	return 1;
}

int Get_ChannelList_5G_qtn(void)
{
	return Get_channel_list_qtn(1);
}

// format : [Band, SSID, channel, security, encryption, RSSI, MAC, 802.11xx, hidden]
void show_ap_properties(const qcsapi_unsigned_int index, const qcsapi_ap_properties *params, char *buff)
{
	int channel 	= params->ap_channel;
	int wpa_mask	= params->ap_protocol;
	int psk_mask	= params->ap_authentication_mode;
	int tkip_mask	= params->ap_encryption_modes;
	int proto 	= params->ap_80211_proto;
	int hidden;
	char band[8], ssid[256], security[32], auth[16], crypto[32], wmode[8];
	char mac[24];

	// MAC
	sprintf(&mac[0], "%02X:%02X:%02X:%02X:%02X:%02X", 
		params->ap_mac_addr[0],
		params->ap_mac_addr[1],
		params->ap_mac_addr[2],
		params->ap_mac_addr[3],
		params->ap_mac_addr[4],
		params->ap_mac_addr[5]
	);

	// Band
	if(channel > 15)
		strcpy(band, "5G");
	else
		strcpy(band, "2G");

	// SSID
	if(!strcmp(params->ap_name_SSID, ""))
		strcpy(ssid, "");
	else{	
		memset(ssid, 0, sizeof(ssid));
		char_to_ascii(ssid, params->ap_name_SSID);
	}

	// security and authentication : check wpa_mask and psk_mask
	// 	wpa_mask : 0x01 = WPA, 0x02 = WPA2
	// 	psk_mask : 0x01 = psk, 0x02 = enterprise
	if((wpa_mask == 0x1) && (psk_mask == 0x01)){
		strcpy(security, "WPA-Personal");
		strcpy(auth, "PSK");
	}
	else if((wpa_mask == 0x2) && (psk_mask == 0x01)){
		strcpy(security, "WPA2-Personal");
		strcpy(auth, "PSK");
	}
	else if((wpa_mask == 0x3) && (psk_mask == 0x01)){
		strcpy(security, "WPA-Auto-Personal");
		strcpy(auth, "PSK");
	}
	else if((wpa_mask == 0x0) && (psk_mask == 0x0)){
		strcpy(security, "Open System");
		strcpy(auth, "NONE");
	}
	else{
		strcpy(security, "");
		strcpy(auth, "");
	}
		
	// encryption : check tkip_mask
	// 	tkip_mask : 0x01 = tkip, 0x02 = aes, 0x03 = tkip+aes
	if(tkip_mask == 0x01)
		strcpy(crypto, "TKIP");
	else if(tkip_mask == 0x02)
		strcpy(crypto, "AES");
	else if(tkip_mask == 0x03)
		strcpy(crypto, "TKIP+AES");
	else
		strcpy(crypto, "NONE");

	// Wmode : b/a/an/bg/bgn
	// 0x01 : b
	// 0x02 : g
	// 0x04 : a
	// 0x08 : n
	if(proto == 0x01)
		strcpy(wmode, "b");
	else if(proto == 0x04)
		strcpy(wmode, "a");
	else if(proto == 0x0C)
		strcpy(wmode, "an");
	else if(proto == 0x03)
		strcpy(wmode, "bg");
	else if(proto == 0x0B)
		strcpy(wmode, "bgn");
	else if(proto == 0x1C)
		strcpy(wmode, "ac");
	else{
		strcpy(wmode, "");
		fprintf(stderr, "[%s][%d]dp: [%d]\n", __FUNCTION__, __LINE__, proto);
	}

	// hidden SSID : if get MAC but not get SSID, it should be a hidden SSID
	if((&mac[0] != NULL) && !strcmp(params->ap_name_SSID, ""))
		hidden = 1;
	else if((&mac[0] != NULL) && !strcmp(params->ap_name_SSID, ""))
		hidden = 0;
	else
		hidden = 0;

#if 0
	dbg("band=%s,SSID=%s,channel=%d,security=%s,crypto=%s,RSSI=%d,MAC=%s,wmode=%s,hidden=%d\n", 
		band, ssid, params->ap_channel, security, crypto, params->ap_RSSI, &mac[0], wmode, hidden);
#endif

	sprintf(buff, "\"%s\",\"%s\",\"%d\",\"%s\",\"%s\",\"%d\",\"%s\",\"%s\",\"%d\"", 
		band, ssid, params->ap_channel, security, crypto, params->ap_RSSI, &mac[0], wmode, hidden);
}

int wlcscan_core_qtn(char *ofile, char *ifname)
{
	int i;
	int scanstatus = -1;
	uint32_t count;
	qcsapi_ap_properties	params;
	char buff[256];
	FILE *fp_apscan;
	int ret;

	ret = rpc_qcsapi_init();
	if (ret < 0) {
		fprintf(stderr, "[%s][%d]rpc_qcsapi_init error\n", __FUNCTION__, __LINE__);
		return -1;
	}
	/* clean APSCAN_INFO */
	lock_qtn_apscan = file_lock("sitesurvey");
	if((fp_apscan = fopen(ofile, "a")) != NULL){
		fclose(fp_apscan);
	}
	file_unlock(lock_qtn_apscan);
	
	// start scan AP
	if(qcsapi_wifi_start_scan(ifname)){
		dbg("fail to start AP scan\n");
		return 0;
	}

	// loop for check scan status
	while(1){
		if(qcsapi_wifi_get_scan_status(ifname, &scanstatus) < 0){
			dbg("scan error occurs\n");
			return 0;
		}
		
		// if scanstatus = 0 , no scan is running
		if(scanstatus == 0) break;
		else{
			dbg("scan is running...\n");
			sleep(1);
		}
	}

	// check AP scan
	if(qcsapi_wifi_get_results_AP_scan(ifname, &count) < 0){
		dbg("fail to get AP scan results, ifname=%s, count=%d\n", ifname, (int)count);
		return 0;
	}

	if((int)count > 0){
		if((fp_apscan = fopen(ofile, "a")) == NULL){
			dbg("fail to write to [%s]\n", ofile);
			return 0;
		}
		else{
			// for loop
			for(i = 0; i < (int)count; i++){
				// get properties of AP
				if(!qcsapi_wifi_get_properties_AP(ifname, (uint32_t)i, &params)){
					show_ap_properties((uint32_t)i, &params, buff);
					fprintf(fp_apscan, "%s", buff);
				}
				else{
					dbg("fail to get AP properties\n");
					fclose(fp_apscan);
				}

				if (i == (int)count - 1){
					fprintf(fp_apscan, "\n");
				}else{
					fprintf(fp_apscan, "\n");
				}
			}
			// for loop
		}
	}
	fclose(fp_apscan);
	return 1;
}

