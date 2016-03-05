#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bcmnvram.h>
#include <shutils.h>

#include "shared.h"
#include "web-qtn.h"
#include "net80211/ieee80211_dfs_reentry.h"

#ifdef RTCONFIG_JFFS2ND_BACKUP
#include <sys/mount.h>
#include <sys/statfs.h>
#endif

static int lock_qtn_apscan = -1;

extern int isValidCountryCode(const char *Ccode);
extern int isValidRegrev(char *regrev);
extern int isValidMacAddr(const char* mac);
extern int file_lock(char *tag);
extern void file_unlock(int lockfd);
extern void char_to_ascii(const char *output, const char *input);

#define	WIFINAME	"wifi0"
#if 0
void inc_mac(char *mac, int plus);
#endif

struct txpower_ac_qtn_s {
	uint16 min;
	uint16 max;
	uint8 pwr;
};

static const struct txpower_ac_qtn_s txpower_list_qtn_rtac87u[] = {
#if !defined(RTCONFIG_RALINK)
	/* 1 ~ 25% */
	{ 1, 25, 14},
	/* 26 ~ 50% */
	{ 26, 50, 17},
	/* 51 ~ 75% */
	{ 51, 75, 20},
	/* 76 ~ 100% */
	{ 76, 100, 23},
#endif	/* !RTCONFIG_RALINK */
	{ 0, 0, 0x0}
};

int
setCountryCode_5G_qtn(const char *cc)
{
	int ret;
	char value[20] = {0};

	if( cc==NULL || !isValidCountryCode(cc) )
		return 0;

	if (!rpc_qtn_ready()) {
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

	if (!factory_debug_raw()) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}

	ATE_BRCM_SET("1:ccode", cc);
	puts(nvram_safe_get("1:ccode"));

	return 1;
}

int
getCountryCode_5G_qtn(void)
{
	puts(cfe_nvram_safe_get_raw("1:ccode"));

	return 0;
}

int setRegrev_5G_qtn(const char *regrev)
{
	int ret;
	char value[20] = {0};

	if( regrev==NULL || !isValidRegrev((char *)regrev) )
		return 0;

	if (!rpc_qtn_ready()) {
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

	if (!factory_debug_raw()) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}

	ATE_BRCM_SET("1:regrev", regrev);
	puts(nvram_safe_get("1:regrev"));
	return 1;
}

int
getRegrev_5G_qtn(void)
{
	puts(cfe_nvram_safe_get_raw("1:regrev"));
	return 0;
}

int setMAC_5G_qtn(const char *mac)
{
	int ret;
	char value[20] = {0};

	if( mac==NULL || !isValidMacAddr(mac) )
		return 0;

	if (!rpc_qtn_ready())
	{
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

	if (!factory_debug_raw()) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}

	ATE_BRCM_SET("1:macaddr", mac);
	// puts(nvram_safe_get("1:macaddr"));

	puts(value);
	return 1;
}

int getMAC_5G_qtn(void)
{
	int ret;
	char value[20] = {0};

	if (!rpc_qtn_ready()) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_bootcfg_get_parameter("ethaddr", value, sizeof(value));
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	puts(value);
	return 1;
}

int setAllLedOn_qtn(void)
{
	int ret;

	if (!rpc_qtn_ready()) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_led_set(1, 1);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_wifi_run_script("router_command.sh", "lan4_led_ctrl on");
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	return 0;
}

int setAllLedOff_qtn(void)
{
	int ret;

	if (!rpc_qtn_ready()) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_wifi_run_script("router_command.sh", "wifi_led_off");
	if (ret < 0) {
		fprintf(stderr, "[led] router_command.sh: wifi_led_off error\n");
		return -1;
	}

	ret = qcsapi_led_set(1, 0);
	if (ret < 0) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}

	ret = qcsapi_wifi_run_script("router_command.sh", "lan4_led_ctrl off");
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

	if (!rpc_qtn_ready()) {
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

	if (!rpc_qtn_ready()) {
		dbG("5 GHz radio is not ready\n");
		return -1;
	}

	logmessage("wlcscan", "start wlcscan scan\n");

	/* clean APSCAN_INFO */
	lock_qtn_apscan = file_lock("sitesurvey");
	if((fp_apscan = fopen(ofile, "a")) != NULL){
		fclose(fp_apscan);
	}
	file_unlock(lock_qtn_apscan);
	
	// start scan AP
	// if(qcsapi_wifi_start_scan(ifname)){
	if(qcsapi_wifi_start_scan_ext(ifname, IEEE80211_PICK_ALL | IEEE80211_PICK_NOPICK_BG)){
		dbg("fail to start AP scan\n");
		return 0;
	}
	fprintf(stderr, "ok to start AP scan\n");

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

int GetPhyStatus_qtn(void)
{
	int ret;

	if (!rpc_qtn_ready()) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_wifi_run_script("router_command.sh", "get_eth_1000m");
	if (ret < 0) {
		ret = qcsapi_wifi_run_script("router_command.sh", "get_eth_100m");
		if (ret < 0) {
			ret = qcsapi_wifi_run_script("router_command.sh", "get_eth_10m");
			if (ret < 0) {
				// fprintf(stderr, "ATE command error\n");
				return 0;
			}else{
				return 10;
			}
		}else{
			return 100;
		}
		return -1;
	}else{
		return 1000;
	}
	return 0;
}

int start_ap_qtn(void)
{
	int ret;
	int			 qcsapi_retval;
	unsigned int		 iter;
	char ssid[65];

	if (!rpc_qtn_ready()) {
		dbG("5 GHz radio is not ready\n");
		return -1;
	}

	logmessage("start_ap", "AP is running...");

#if 0
	qcsapi_retval = qcsapi_wifi_reload_in_mode(WIFINAME, qcsapi_access_point);

	if (qcsapi_retval >= 0) {
		fprintf(stderr, "reload to AP mode successfuly\n" );
	} else {
		fprintf(stderr, "reload to AP mode failed\n" );
	}
#endif
	sprintf(ssid, "%s", nvram_safe_get("wl1_ssid"));
	ret = qcsapi_wifi_set_SSID(WIFINAME, ssid);

	// check security
	char auth[8];
	char crypto[16];
	char beacon[] = "WPAand11i";
	char encryption[] = "TKIPandAESEncryption";
	char key[65];
	uint32_t index = 0;

	strncpy(auth, nvram_safe_get("wl1_auth_mode_x"), sizeof(auth));
	strncpy(crypto, nvram_safe_get("wl1_crypto"), sizeof(crypto));
	strncpy(key, nvram_safe_get("wl1_wpa_psk"), sizeof(key));

	if(!strcmp(auth, "psk2") && !strcmp(crypto, "aes")){
		memcpy(beacon, "11i", strlen("11i") + 1);
		memcpy(encryption, "AESEncryption", strlen("AESEncryption") + 1);
	}
	else if(!strcmp(auth, "pskpsk2") && !strcmp(crypto, "aes") ){
		memcpy(beacon, "WPAand11i", strlen("WPAand11i") + 1);
		memcpy(encryption, "AESEncryption", strlen("AESEncryption") + 1);
	}
	else if(!strcmp(auth, "pskpsk2") && !strcmp(crypto, "tkip+aes") ){
		memcpy(beacon, "WPAand11i", strlen("WPAand11i") + 1);
		memcpy(encryption, "TKIPandAESEncryption", strlen("TKIPandAESEncryption") + 1);
	}
	else{
		logmessage("start_ap", "No security in use\n");
		memcpy(beacon, "Basic", strlen("Basic") + 1);
	}

	logmessage("start_ap", "ssid=%s, auth=%s, crypto=%s, encryption=%s, key=%s\n", ssid, auth, crypto, encryption, key);
	if(!strcmp(auth, "open")){
		if(qcsapi_wifi_set_WPA_authentication_mode(WIFINAME, "NONE") < 0)
			logmessage("start_ap", "fail to setup a open-none ap\n");
		if(qcsapi_wifi_set_beacon_type(WIFINAME, beacon) < 0)
			logmessage("start_ap", "fail to setup beacon type in ap\n");
	}
	else{
		if(qcsapi_wifi_set_beacon_type(WIFINAME, beacon) < 0)
			logmessage("start_ap", "fail to setup beacon type in ap\n");
		if(qcsapi_wifi_set_WPA_authentication_mode(WIFINAME, "PSKAuthentication") < 0)
			logmessage("start_ap", "fail to setup authentiocation type in ap\n");
		if(qcsapi_wifi_set_key_passphrase(WIFINAME, index, key) < 0)
			logmessage("start_ap", "fail to set key in ap\n");
		if(qcsapi_wifi_set_WPA_encryption_modes(WIFINAME, encryption) < 0)
			logmessage("start_ap", "fail to set encryption mode in ap\n");
	}

	logmessage("start_ap", "start_ap done!\n");

	return 1;
}

int start_psta_qtn(void)
{
	static qcsapi_SSID	 array_ssids[10 /* MAX_SSID_LIST_SIZE */];
	int			 qcsapi_retval;
	unsigned int		 iter;
	qcsapi_unsigned_int	 sizeof_list = 2 /* DEFAULT_SSID_LIST_SIZE */ ;
	char			*list_ssids[10 /* MAX_SSID_LIST_SIZE */ + 1];
	int ret;

	if (!rpc_qtn_ready()) {
		dbG("5 GHz radio is not ready\n");
		return -1;
	}

	logmessage("start_psta", "media bridge is running...");

	qcsapi_retval = qcsapi_wifi_reload_in_mode(WIFINAME, qcsapi_station);

	if (qcsapi_retval >= 0) {
		fprintf(stderr, "reload to STA mode successfuly\n" );
	} else {
		fprintf(stderr, "reload to STA mode failed\n" );
	}

	for (iter = 0; iter < sizeof_list; iter++) {
		list_ssids[iter] = array_ssids[iter];
		*(list_ssids[iter]) = '\0';
	}

	qcsapi_retval = qcsapi_SSID_get_SSID_list(WIFINAME, sizeof_list, &list_ssids[0]);
	if (qcsapi_retval >= 0) {
		for (iter = 0; iter < sizeof_list; iter++) {
			if ((list_ssids[iter] == NULL) || strlen(list_ssids[iter]) < 1) {
				break;
			}
			fprintf(stderr, "remove [%s]\n", list_ssids[iter]);
			qcsapi_SSID_remove_SSID(WIFINAME, array_ssids[iter]);
		}
	}

	// verify ssid, if not exists, create new one
	char ssid[33];
	strncpy(ssid, nvram_safe_get("wlc_ssid"), sizeof(ssid));
	logmessage("start_psta", "verify ssid [%s]", ssid);
	if(qcsapi_SSID_verify_SSID(WIFINAME, ssid) < 0){
		logmessage("start_psta", "Not such SSID in sta mode\n");
		if(qcsapi_SSID_create_SSID(WIFINAME, ssid) < 0)
			logmessage("start_psta", "fail to create SSID in sta mode\n");
	}

	// check security
	char auth[8];
	char crypto[16];
	char beacon[] = "WPAand11i";
	char encryption[] = "TKIPandAESEncryption";
	char key[65];
	uint32_t index = 0;

	strncpy(auth, nvram_safe_get("wlc_auth_mode"), sizeof(auth));
	strncpy(crypto, nvram_safe_get("wlc_crypto"), sizeof(crypto));
	strncpy(key, nvram_safe_get("wlc_wpa_psk"), sizeof(key));

	if(!strcmp(auth, "psk2") && !strcmp(crypto, "aes")){
		memcpy(beacon, "11i", strlen("11i") + 1);
		memcpy(encryption, "AESEncryption", strlen("AESEncryption") + 1);
	}
	else if(!strcmp(auth, "pskpsk2") && !strcmp(crypto, "aes") ){
		memcpy(beacon, "WPAand11i", strlen("WPAand11i") + 1);
		memcpy(encryption, "AESEncryption", strlen("AESEncryption") + 1);
	}
	else if(!strcmp(auth, "pskpsk2") && !strcmp(crypto, "tkip+aes") ){
		memcpy(beacon, "WPAand11i", strlen("WPAand11i") + 1);
		memcpy(encryption, "TKIPandAESEncryption", strlen("TKIPandAESEncryption") + 1);
	}
	else{
		logmessage("start_psta", "not support such authentication & encryption\n");
	}

	logmessage("start_psta", "ssid=%s, auth=%s, crypto=%s, encryption=%s, key=%s\n", ssid, auth, crypto, encryption, key);
	if(!strcmp(auth, "open")){
		if(qcsapi_SSID_set_authentication_mode(WIFINAME, ssid, "NONE") < 0)
			logmessage("start_psta", "fail to setup a open-none sta\n");
	}
	else{
		if(qcsapi_SSID_set_protocol(WIFINAME, ssid, beacon) < 0)
			logmessage("start_psta", "fail to setup protocol in sta\n");
		if(qcsapi_SSID_set_authentication_mode(WIFINAME, ssid, "PSKAuthentication") < 0)
			logmessage("start_psta", "fail to setup authentiocation type in sta\n");
		if(qcsapi_SSID_set_key_passphrase(WIFINAME, ssid, index, key) < 0)
			logmessage("start_psta", "fail to set key in sta\n");
	}

	// eval("wpa_cli", "reconfigure");
	ret = qcsapi_wifi_run_script("router_command.sh", "wpa_cli_reconfigure");
	if (ret < 0) {
		fprintf(stderr, "[psta] router_command.sh: wpa_cli_reconfigure error\n");
		return -1;
	}

	logmessage("start_psta", "start_psta done!\n");

	return 1;
}

int start_nodfs_scan_qtn(void)
{
	int		 qcsapi_retval;
	int pick_flags = 0;

	logmessage("dfs", "start dfs scan\n");

	pick_flags = IEEE80211_PICK_CLEAREST;
	pick_flags |= IEEE80211_PICK_NONDFS;

	if (!rpc_qtn_ready()) {
		dbG("5 GHz radio is not ready\n");
		return -1;
	}
	qcsapi_retval = qcsapi_wifi_start_scan_ext(WIFINAME, pick_flags);
	if (qcsapi_retval >= 0) {
		logmessage("nodfs_scan", "complete");
	}else{
		logmessage("nodfs_scan", "scan not complete");
	}

	return 1;
}

int enable_qtn_telnetsrv(int enable_flag)
{
	int ret;

	if (!rpc_qtn_ready()) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	if(enable_flag == 0){
		nvram_set("QTNTELNETSRV", "0");
		ret = qcsapi_wifi_run_script("router_command.sh", "enable_telnet_srv 0");
	}else{
		nvram_set("QTNTELNETSRV", "1");
		ret = qcsapi_wifi_run_script("router_command.sh", "enable_telnet_srv 1");
	}
	if (ret < 0) {
		fprintf(stderr, "[ate] set telnet server error\n");
		return -1;
	}
	nvram_commit();
	return 0;
}

int getstatus_qtn_telnetsrv(void)
{
	if(nvram_get_int("QTNTELNETSRV") == 1)
		puts("1");
	else
		puts("0");

	return 0;
}

int del_qtn_cal_files(void)
{
	int ret;

	if (!rpc_qtn_ready()) {
		fprintf(stderr, "ATE command error\n");
		return -1;
	}
	ret = qcsapi_wifi_run_script("router_command.sh", "del_cal_files");
	if (ret < 0) {
		fprintf(stderr, "[ate] delete calibration files error\n");
		return -1;
	}
	return 0;
}

int get_tx_power_qtn(void)
{
	const struct txpower_ac_qtn_s *p_to_table;
	int txpower = 80;

	p_to_table = &txpower_list_qtn_rtac87u[0];
	txpower = nvram_get_int("wl1_txpower");

	for (p_to_table; p_to_table->min != 0; ++p_to_table) {
		if (txpower >= p_to_table->min && txpower <= p_to_table->max) {
			dbG("txpoewr between: min:[%d] to max:[%d]\n", p_to_table->min, p_to_table->max);
			return p_to_table->pwr;
		}
	}

	if ( p_to_table->min == 0 )
		dbG("no correct power offset!\n");

	/* default max power */
	return 23;
}

typedef uint16 chanspec_t;
extern uint8 wf_chspec_ctlchan(chanspec_t chspec);
extern chanspec_t wf_chspec_aton(const char *a);

void fix_script_err(char *orig_str, char *new_str)
{
	unsigned i = 0, j = 0;
	unsigned int str_len = 0;
	str_len = strlen(orig_str);

	for ( i = 0; i < str_len; i++ ){
		if(orig_str[i] == '$' ||
			orig_str[i] == '`' ||
			orig_str[i] == '"' ||
			orig_str[i] == '\\'){
			new_str[j] = '\\';
			new_str[j+1] = orig_str[i];
			j = j + 2;
		}else{
			new_str[j] = orig_str[i];
			j++;
		}
	}
}

int gen_stateless_conf(void)
{
	int ret;
	FILE *fp;

	int l_len;
	// check security
	char auth[8];
	char crypto[16];
	char beacon[] = "WPAand11i";
	char encryption[] = "TKIPandAESEncryption";
	char key[130];
	char tmpkey[130];
	char ssid[66];
	char tmpssid[66];
	char region[5] = {0};
	int channel = wf_chspec_ctlchan(wf_chspec_aton(nvram_safe_get("wl1_chanspec")));
	int bw = atoi(nvram_safe_get("wl1_bw"));
	uint32_t index = 0;

	sprintf(ssid, "%s", nvram_safe_get("wl1_ssid"));
	memset(tmpssid, 0, sizeof(tmpssid));
	fix_script_err(ssid, tmpssid);
	strncpy(ssid, tmpssid, sizeof(ssid));

	sprintf(region, "%s", nvram_safe_get("wl1_country_code"));
	if(strlen(region) == 0)
		sprintf(region, "%s", nvram_safe_get("1:ccode"));
	dbg("[stateless] channel:[%d]\n", channel);
	dbg("[stateless] bw:[%d]\n", bw);

	fp = fopen("/tmp/stateless_slave_config", "w");

	if(nvram_get_int("sw_mode") == SW_MODE_AP &&
		nvram_get_int("wlc_psta") == 1 &&
		nvram_get_int("wlc_band") == 1){
		/* media bridge mode */
		fprintf(fp, "wifi0_mode=sta\n");

		strncpy(auth, nvram_safe_get("wlc_auth_mode"), sizeof(auth));
		strncpy(crypto, nvram_safe_get("wlc_crypto"), sizeof(crypto));
		strncpy(key, nvram_safe_get("wlc_wpa_psk"), sizeof(key));
		memset(tmpkey, 0, sizeof(tmpkey));
		fix_script_err(key, tmpkey);
		strncpy(key, tmpkey, sizeof(key));

		strncpy(ssid, nvram_safe_get("wlc_ssid"), sizeof(ssid));
		memset(tmpssid, 0, sizeof(tmpssid));
		fix_script_err(ssid, tmpssid);
		strncpy(ssid, tmpssid, sizeof(ssid));
		fprintf(fp, "wifi0_SSID=\"%s\"\n", ssid);

		logmessage("start_psta", "ssid=%s, auth=%s, crypto=%s, encryption=%s, key=%s\n", ssid, auth, crypto, encryption, key);

		/* convert security from nvram to qtn */
		if(!strcmp(auth, "psk2") && !strcmp(crypto, "aes")){
			fprintf(fp, "wifi0_auth_mode=PSKAuthentication\n");
			fprintf(fp, "wifi0_beacon=11i\n");
			fprintf(fp, "wifi0_encryption=AESEncryption\n");
			fprintf(fp, "wifi0_passphrase=\"%s\"\n", key);
		}
		else if(!strcmp(auth, "pskpsk2") && !strcmp(crypto, "aes") ){
			fprintf(fp, "wifi0_auth_mode=PSKAuthentication\n");
			fprintf(fp, "wifi0_beacon=WPAand11i\n");
			fprintf(fp, "wifi0_encryption=AESEncryption\n");
			fprintf(fp, "wifi0_passphrase=\"%s\"\n", key);
		}
		else if(!strcmp(auth, "pskpsk2") && !strcmp(crypto, "tkip+aes") ){
			fprintf(fp, "wifi0_auth_mode=PSKAuthentication\n");
			fprintf(fp, "wifi0_beacon=WPAand11i\n");
			fprintf(fp, "wifi0_encryption=TKIPandAESEncryption\n");
			fprintf(fp, "wifi0_passphrase=\"%s\"\n", key);
		}
		else{
			logmessage("start_psta", "No security in use\n");
			fprintf(fp, "wifi0_auth_mode=NONE\n");
			fprintf(fp, "wifi0_beacon=Basic\n");
		}

		/* auto channel for media bridge mode */
		channel = 0;
	}else{
		/* not media bridge mode */
		fprintf(fp, "wifi0_mode=ap\n");

		strncpy(auth, nvram_safe_get("wl1_auth_mode_x"), sizeof(auth));
		strncpy(crypto, nvram_safe_get("wl1_crypto"), sizeof(crypto));
		strncpy(key, nvram_safe_get("wl1_wpa_psk"), sizeof(key));
		memset(tmpkey, 0, sizeof(tmpkey));
		fix_script_err(key, tmpkey);
		strncpy(key, tmpkey, sizeof(key));


		strncpy(ssid, nvram_safe_get("wl1_ssid"), sizeof(ssid));
		memset(tmpssid, 0, sizeof(tmpssid));
		fix_script_err(ssid, tmpssid);
		strncpy(ssid, tmpssid, sizeof(ssid));
		fprintf(fp, "wifi0_SSID=\"%s\"\n", ssid);

		if(!strcmp(auth, "psk2") && !strcmp(crypto, "aes")){
			fprintf(fp, "wifi0_auth_mode=PSKAuthentication\n");
			fprintf(fp, "wifi0_beacon=11i\n");
			fprintf(fp, "wifi0_encryption=AESEncryption\n");
			fprintf(fp, "wifi0_passphrase=\"%s\"\n", key);
		}
		else if(!strcmp(auth, "pskpsk2") && !strcmp(crypto, "aes") ){
			fprintf(fp, "wifi0_auth_mode=PSKAuthentication\n");
			fprintf(fp, "wifi0_beacon=WPAand11i\n");
			fprintf(fp, "wifi0_encryption=AESEncryption\n");
			fprintf(fp, "wifi0_passphrase=\"%s\"\n", key);
		}
		else if(!strcmp(auth, "pskpsk2") && !strcmp(crypto, "tkip+aes") ){
			fprintf(fp, "wifi0_auth_mode=PSKAuthentication\n");
			fprintf(fp, "wifi0_beacon=WPAand11i\n");
			fprintf(fp, "wifi0_encryption=TKIPandAESEncryption\n");
			fprintf(fp, "wifi0_passphrase=\"%s\"\n", key);
		}
		else{
			logmessage("start_ap", "No security in use\n");
			fprintf(fp, "wifi0_beacon=Basic\n");
		}
	}

	for( l_len = 0 ; l_len < strlen(region); l_len++){
		region[l_len] = tolower(region[l_len]);
	}
	fprintf(fp, "wifi0_region=%s\n", region);
	// nvram_set("wl1_country_code", nvram_safe_get("1:ccode"));
	fprintf(fp, "wifi0_vht=1\n");
	if(bw==1) fprintf(fp, "wifi0_bw=20\n");
	else if(bw==2) fprintf(fp, "wifi0_bw=40\n");
	else if(bw==3) fprintf(fp, "wifi0_bw=80\n");
	else fprintf(fp, "wifi0_bw=80\n");

	/* if media bridge mode, always auto channel */
	fprintf(fp, "wifi0_channel=%d\n", channel);
	fprintf(fp, "wifi0_pwr=%d\n", get_tx_power_qtn());
	if(nvram_get_int("wl1_itxbf") == 1 || nvram_get_int("wl1_txbf") == 1){
		fprintf(fp, "wifi0_bf=1\n");
	}else{
		fprintf(fp, "wifi0_bf=0\n");
	}
	if(nvram_get_int("wl1_mumimo") == 1){
		fprintf(fp, "wifi0_mu=1\n");
	}else{
		fprintf(fp, "wifi0_mu=0\n");
	}
	fprintf(fp, "wifi0_staticip=1\n");
	fprintf(fp, "slave_ipaddr=\"192.168.1.111/16\"\n");
	fprintf(fp, "server_ipaddr=\"%s\"\n", nvram_safe_get("QTN_RPC_SERVER"));
	fprintf(fp, "client_ipaddr=\"%s\"\n", nvram_safe_get("QTN_RPC_CLIENT"));

	if(nvram_match("wl1.1_lanaccess", "off") && !nvram_match("wl1.1_lanaccess", ""))
		fprintf(fp, "wifi1_lanaccess=off\n");
	else
		fprintf(fp, "wifi1_lanaccess=on\n");

	if(nvram_match("wl1.2_lanaccess", "off") && !nvram_match("wl1.2_lanaccess", ""))
		fprintf(fp, "wifi2_lanaccess=off\n");
	else
		fprintf(fp, "wifi2_lanaccess=on\n");

	if(nvram_match("wl1.3_lanaccess", "off") && !nvram_match("wl1.3_lanaccess", ""))
		fprintf(fp, "wifi3_lanaccess=off\n");
	else
		fprintf(fp, "wifi3_lanaccess=on\n");

	fclose(fp);

	return 1;
}

void rpc_parse_nvram_from_httpd(int unit, int subunit);

int runtime_config_qtn(int unit, int subunit)
{
	int ret;

	if (!rpc_qtn_ready()) {
		dbG("qcsapi error\n");
		return -1;
	}
	if ( unit == 1 && subunit == -1 ){
#if 0
		dbG("Global QTN settings\n");
		if(nvram_get_int("wl1_itxbf") == 1 || nvram_get_int("wl1_txbf") == 1){
			dbG("[bf] set_bf_on\n");
			qcsapi_wifi_run_script("router_command.sh", "set_bf_on");
			ret = qcsapi_config_update_parameter(WIFINAME, "bf", "1");
			if (ret < 0) dbG("qcsapi error\n");
		}else{
			dbG("[bf] set_bf_off\n");
			qcsapi_wifi_run_script("router_command.sh", "set_bf_off");
			ret = qcsapi_config_update_parameter(WIFINAME, "bf", "0");
			if (ret < 0) dbG("qcsapi error\n");
		}
#endif
		gen_stateless_conf();
	}
	rpc_parse_nvram_from_httpd(unit, subunit);
	return 1;
}

/* start: 169.254.39.1, 0x127fea9 */
/* end: 169.254.39.254, 0xfe27fea9 */
int gen_rpc_qcsapi_ip(void)
{
	int i;
	unsigned int j;
	unsigned char *hwaddr;
	char hwaddr_5g[18];
	struct ifreq ifr;
	struct in_addr start, addr;
	int hw_len;
	FILE *fp_qcsapi_conf;

	/* BRCM */
	ether_atoe(nvram_safe_get("lan_hwaddr"), (unsigned char *)&ifr.ifr_hwaddr.sa_data);
	for (j = 0, i = 0; i < 6; i++){
		j += ifr.ifr_hwaddr.sa_data[i] + (j << 6) + (j << 16) - j;
	}
	start.s_addr = htonl(ntohl(0x127fea9 /* start */) +
		((j + 0 /* c->addr_epoch */) % (1 + ntohl(0xfe27fea9 /* end */) - ntohl(0x127fea9 /* start */))));
	nvram_set("QTN_RPC_CLIENT", inet_ntoa(start));

	/* QTN */
	strcpy(hwaddr_5g, nvram_safe_get("lan_hwaddr"));
	inc_mac(hwaddr_5g, 4);
	ether_atoe(hwaddr_5g, (unsigned char *)&ifr.ifr_hwaddr.sa_data);
	for (j = 0, i = 0; i < 6; i++){
		j += ifr.ifr_hwaddr.sa_data[i] + (j << 6) + (j << 16) - j;
	}
	start.s_addr = htonl(ntohl(0x127fea9 /* start */) +
		((j + 0 /* c->addr_epoch */) % (1 + ntohl(0xfe27fea9 /* end */) - ntohl(0x127fea9 /* start */))));
	nvram_set("QTN_RPC_SERVER", inet_ntoa(start));
	if ((fp_qcsapi_conf = fopen("/etc/qcsapi_target_ip.conf", "w")) == NULL){
		logmessage("qcsapi", "write qcsapi conf error");
	}else{
		fprintf(fp_qcsapi_conf, "%s", nvram_safe_get("QTN_RPC_SERVER"));
		fclose(fp_qcsapi_conf);
		logmessage("qcsapi", "write qcsapi conf ok");
	}

#if 0
	do_ping_detect(); /* refer wanduck.c */
#endif
}

#if defined(RTCONFIG_JFFS2ND_BACKUP)
#define JFFS_NAME	"jffs2"
#define SECOND_JFFS2_PARTITION  "asus"
#define SECOND_JFFS2_PATH	"/asus_jffs"
void check_2nd_jffs(void)
{
	char s[256];
	int size;
	int part;
	struct statfs sf;

	_dprintf("2nd jffs2: %s\n", SECOND_JFFS2_PARTITION);

	if (!mtd_getinfo(SECOND_JFFS2_PARTITION, &part, &size)) {
		_dprintf("Can not get 2nd jffs2 information!");
		return;
	}
	mount_2nd_jffs2();

	if(access("/asus_jffs/bootcfg.tgz", R_OK ) != -1 ) {
		logmessage("qtn", "bootcfg.tgz exists");
		system("rm -f /tmp/bootcfg.tgz");
	} else {
		logmessage("qtn", "bootcfg.tgz does not exist");
		sprintf(s, MTD_BLKDEV(%d), part);
		umount("/asus_jffs");
		if (mount(s, SECOND_JFFS2_PATH , JFFS_NAME, MS_NOATIME, "") != 0) {
			logmessage("qtn", "cannot store bootcfg.tgz");
		}else{
			system("cp /tmp/bootcfg.tgz /asus_jffs");
			system("rm -f /tmp/bootcfg.tgz");
			logmessage("qtn", "backup bootcfg.tgz ok");
		}
	}

	if (umount(SECOND_JFFS2_PATH)){
		dbG("umount asus_jffs failed\n");
	}else{
		dbG("umount asus_jffs ok\n");
	}

	// format_mount_2nd_jffs2();
}
#endif
