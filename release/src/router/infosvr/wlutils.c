#ifdef WCLIENT	
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <shutils.h>
#include <wlutils.h>
#include <bcmnvram.h>
//#include <bcmutils.h>
#include <iboxcom.h>
#include <wlstatus.h>

#define WIF "eth2"

extern int wl_ioctl(char *name, int request, void *buf, int len);
extern SITES sites_g[];
extern int sites_g_count;
extern PROFILES profiles_g[];
extern int profiles_g_count;
extern int scan_g_mode;
extern int scan_g_type;

uint8 buf[4096];

time_t scantime=0;
int wl_scan_disabled=0;

int wl_scan(char *ssid)
{
	int ret, unit;
	time_t now;
	
	time(&now);
	
	//printf("scan %x %x %x\n", (unsigned int)scantime, (unsigned int)now, (unsigned int)(now-scantime));
	
	if ((unsigned int )(now-scantime) > 10 && !wl_scan_disabled)
	{
		//printf("scan actually\n");
		unit = 0;
		ret=wl_ioctl(WIF, WLC_SET_AP, &unit, sizeof(unit));
		ret=wl_ioctl(WIF, WLC_SCAN, &unit, sizeof(unit));
		memcpy(&scantime, &now, sizeof(time_t));
	}

	return ret;
}

int wl_scan_results(void)
{
	int unit, ret, i, j;
	wl_scan_results_t *result;
	wl_bss_info_t *info;
	char *info_b;
	unsigned char *ie;

	result = (wl_scan_results_t *)buf;
	result->buflen=sizeof(buf);
	result->version=WL_BSS_INFO_VERSION;
	ret=wl_ioctl(WIF, WLC_SCAN_RESULTS, result, sizeof(buf));

	if (ret==0)
	{
		//printf("count: %x %x %d %d\n", result->count, result->version, sizeof(wl_bss_info_t), sizeof(wl_bss_info106_t));
		info = &(result->bss_info[0]);
		info_b = (unsigned char *)info;

		for (i=0;i<512;i++)
			if (info_b[i] == 107) printf("i=%d\n", i);

		for (i=0;i<result->count;i++)
		{
			//printf("VER	: %d\n", info->version);
			//printf("Ssid	: %s\n", info->SSID);
			//printf("Channel	: %d\n", info->channel);
			//printf("RSSI	: %d\n", info->RSSI);
			//printf("Noise	: %d\n", info->phy_noise);
			//printf("Cap	: %x\n", info->capability);
			//printf("IE:	: %d %d\n", sizeof(wl_bss_info_t), info->ie_length);

			memcpy(sites_g[i].SSID, info->SSID, 32);
			sites_g[i].Type = 0;
		 	sites_g[i].Channel = info->channel;
			sites_g[i].Wep = 0;
			sites_g[i].Status = 0;
			memcpy(sites_g[i].BSSID, &info->BSSID, 6);
			sites_g[i].RSSI = info->RSSI;


			ie = &info->ie_length + 1;
			j=0;

			while (j<info->ie_length)
			{
				//printf("IE: %x %x\n", (unsigned char)ie[j], ie[j+1]);
				if (ie[j] == 0xdd) 
				{
					//printf("WEP\n");
					sites_g[i].Wep = 1;
				}	

				j+=ie[j+1] + 2;

			}
			//assign to profile			
			
			info_b += (sizeof(wl_bss_info_t));

			if ((info->ie_length%4)) 
			{				
				info_b+=info->ie_length+4-(info->ie_length%4);
			}
			else info_b+=info->ie_length;

			info = (wl_bss_info_t *)info_b;
		}
		sites_g_count = result->count;
		return (sites_g_count);
	}
	return 0;
}

int wl_isap(void)
{
	int val;

	wl_ioctl(WIF, WLC_GET_AP, &val, sizeof(val));

	return val;
}

int wl_status(PROFILES *profile)
{
	int unit, ret, i;
	char bssid[32];
	char bssinfobuf[2000];
	wl_bss_info_t *info;
	int val;

	// Get bssid
	ret=wl_ioctl(WIF, WLC_GET_BSSID, bssid, sizeof(bssid));
	//wl_scan_results();

	if (ret==0 && !(bssid[0]==0&&bssid[1]==0&&bssid[2]==0
		&&bssid[3]==0&&bssid[4]==0&&bssid[5]==0)) 	
	{
		//printf("Found\n");

		ret=wl_ioctl(WIF, WLC_GET_BSS_INFO, bssinfobuf, sizeof(bssinfobuf));
		info = (wl_bss_info_t *)bssinfobuf;

		//if (ret==0) printf("Connect to : %s\n", info->SSID);
		//else printf("Connect to unknown\n");
		return 1;
	}
	return 0;
}

void wl_connect(PROFILES *profile)
{
	wsec_key_t key;
	char *keystr, hex[3];
	unsigned char *data=key.data;
	wlc_ssid_t ssid;
	int val, i;

	if (profile->mode == STATION_MODE_AP)
	{
		// Set driver to AP mode	
		val = 1;
		wl_ioctl(WIF, WLC_SET_AP, &val, sizeof(val));

		// Set to Infra
		val = 1;
		wl_ioctl(WIF, WLC_SET_INFRA, &val, sizeof(val));
	}
	else
	{
		// Set driver to Sta mode	
		val = 0;
		wl_ioctl(WIF, WLC_SET_AP, &val, sizeof(val));

		// Set station mode
		if (profile->mode == STATION_MODE_INFRA) val = 1;
		else val=0;
		wl_ioctl(WIF, WLC_SET_INFRA, &val, sizeof(val));

		if (nvram_invmatch("wlp_clientmode_x", "2")) val=0;		
		else val=1;
		wl_ioctl(WIF, WLC_SET_WET, &val, sizeof(val));
	}

	// Set Authentication mode
	val = profile->sharedkeyauth;
	//printf("auth: %d\n", val);
	wl_ioctl(WIF, WLC_SET_AUTH, &val, sizeof(val));

	// Set WEP enable
	if (profile->wep == STA_ENCRYPTION_ENABLE) 
	{	
		//printf("wep: %d %d\n", profile->wep, profile->wepkeylen);
		// Set WEP key 
		for (i=0;i<4;i++)
		{
			memset(&key, 0, sizeof(wsec_key_t));
			
			key.index = i;

			if (i==0) keystr = profile->wepkey1;
			else if (i==1) keystr = profile->wepkey2;
			else if (i==2) keystr = profile->wepkey3;
			else if (i==3) keystr = profile->wepkey4;

			if (profile->wepkeylen==STA_ENCRYPTION_TYPE_WEP64)
			{
				key.algo = CRYPTO_ALGO_WEP1;
				key.len=5;
			}
			else 
			{
				key.algo = CRYPTO_ALGO_WEP128;
				key.len=13;
			}

			strncpy(key.data, keystr, key.len);
	
			//printf("%x %x %x\n", key.data[0], key.data[1], key.len);
			if (i==profile->wepkeyactive) key.flags=WSEC_PRIMARY_KEY;
			wl_ioctl(WIF, WLC_SET_KEY, &key, sizeof(key));
		}
		//val = WEP_ENABLED;
		val = 1;
	}
	else val = 0;

	//wl_ioctl(WIF, WLC_SET_WSEC, &val, sizeof(val)); For AP?
	wl_ioctl(WIF, WLC_SET_WEP, &val, sizeof(val));


	memset(&ssid, 0, sizeof(ssid));
	ssid.SSID_len = strlen(profile->ssid);
	strncpy(ssid.SSID, profile->ssid, ssid.SSID_len);
	wl_ioctl(WIF, WLC_SET_SSID, &ssid, sizeof(ssid));
}

void wl_join(PROFILES *profile)
{
	int unit, ret, i;
	wl_scan_results_t *result;
	wl_bss_info_t *info;

	// get scan result

	// check if ssid in this list, if yes, connect it

	// if ssid is not in the list, connect any one profile
	wl_connect(profile);
}

void wl_disassoc(void)
{

}


char *nvram_get_str(char *name, int index)
{
	char tmpname[32], tmpval[64];

	if (index!=-1)
		sprintf(tmpname, "%s%d", name, index);
	else sprintf(tmpname, "%s", name);

	return (nvram_safe_get(tmpname));
}

int nvram_get_val(char *name, int index)
{
	char tmpname[32], tmpval[64];

	if (index!=-1)
		sprintf(tmpname, "%s%d", name, index);
	else sprintf(tmpname, "%s", name);

	return (atoi(nvram_safe_get(tmpname)));
}


void wl_read_profile(void)
{
	int i, weptype;

	profiles_g_count = nvram_get_val("wlp_num", -1);
	scan_g_type = nvram_get_val("wlp_scan_type", -1);
	scan_g_mode = nvram_get_val("wlp_scan_mode", -1);

	printf("count: %d\n", profiles_g_count);

	for (i=0;i<profiles_g_count;i++)
	{
		if (nvram_get_val("wlp_nettype", i)==0)
			profiles_g[i].mode = STATION_MODE_INFRA;
		else profiles_g[i].mode = STATION_MODE_ADHOC;

		profiles_g[i].chan = nvram_get_val("wlp_channel", i);

		strcpy(profiles_g[i].ssid, nvram_get_str("wlp_ssid", i));

		profiles_g[i].rate = nvram_get_val("wlp_rate", i);
		profiles_g[i].sharedkeyauth = (nvram_get_val("wlp_sharedkeyauth", i)? 1:0);
		weptype = nvram_get_val("wlp_weptype", i);

		if (profiles_g[i].sharedkeyauth)
		{
			if (weptype==1)
			{
				profiles_g[i].wep = STA_ENCRYPTION_ENABLE;
				profiles_g[i].wepkeylen = STA_ENCRYPTION_TYPE_WEP128;
			}
			else
			{
				profiles_g[i].wep = STA_ENCRYPTION_ENABLE;
				profiles_g[i].wepkeylen = STA_ENCRYPTION_TYPE_WEP64;
			}	
		}
		else
		{	
			if (weptype==1)
			{
				profiles_g[i].wep = STA_ENCRYPTION_ENABLE;
				profiles_g[i].wepkeylen = STA_ENCRYPTION_TYPE_WEP64;
			}
			else if (weptype==2)
			{
				profiles_g[i].wep = STA_ENCRYPTION_ENABLE;
				profiles_g[i].wepkeylen = STA_ENCRYPTION_TYPE_WEP128;
			}
			else
			{
				profiles_g[i].wep = STA_ENCRYPTION_DISABLE;
				//profiles_g[i].wepkeylen =
			}
		}


		if (profiles_g[i].wepkeylen == STA_ENCRYPTION_TYPE_WEP64) 
		{
			weptype = ENCRYPTION_WEP64;

			PackKey(weptype, profiles_g[i].wepkey1, 
			nvram_get_str("wlp_wepkey1", i),
			nvram_get_str("wlp_wepkey2", i),
			nvram_get_str("wlp_wepkey3", i),
			nvram_get_str("wlp_wepkey4", i));
		}
		else if (profiles_g[i].wepkeylen == STA_ENCRYPTION_TYPE_WEP128) 
		{
			weptype = ENCRYPTION_WEP128;

			PackKey(weptype, profiles_g[i].wepkey1, 
			nvram_get_str("wlp_wepkey1", i),
			nvram_get_str("wlp_wepkey2", i),
			nvram_get_str("wlp_wepkey3", i),
			nvram_get_str("wlp_wepkey4", i));

		}

		// nvram 0~1
		profiles_g[i].wepkeyactive = nvram_get_val("wlp_wepkeyactive", i) - 1;

		profiles_g[i].brgmacclone = nvram_get_val("wlp_brgmacclone", i);

		profiles_g[i].preamble = nvram_get_val("wlp_preamble", i);

		//strcpy(profiles_g[i].name, nvram_get_str("wlp_name", i));
		//profiles_g[i].ipaddr = nvram_get_val("wlp_ipaddr", i);
		//profiles_g[i].netmask = nvram_get_val("wlp_netmask", i);
		//profiles_g[i].gateway = nvram_get_val("wlp_gateway", i);
		//profiles_g[i].dhcp = nvram_get_val("wlp_dhcp", i);
	}
}


void nvram_set_str(char *name, char *str, int index)
{
	char tmpname[32], tmpval[64];

	if (index!=-1)
		sprintf(tmpname, "%s%d", name, index);
	else sprintf(tmpname, "%s", name);

	nvram_set(tmpname, str);
}

void nvram_set_val(char *name, int val, int index)
{
	char tmpname[32], tmpval[64];

	if (index!=-1)
		sprintf(tmpname, "%s%d", name, index);
	else sprintf(tmpname, "%s", name);

	sprintf(tmpval, "%d", val);

	nvram_set(tmpname, tmpval);
}

void wl_write_profile(void)
{
	int i, weptype, weplen;
	char key[4][36];

	printf("write count : %d\n", profiles_g_count);
	for (i=0;i<profiles_g_count;i++)
	{
		if (profiles_g[i].mode==STATION_MODE_ADHOC)
			nvram_set_val("wlp_nettype", 1, i);
		else nvram_set_val("wlp_nettype", 0, i);
		nvram_set_val("wlp_channel", profiles_g[i].chan, i);
		nvram_set_str("wlp_ssid", profiles_g[i].ssid, i);
		nvram_set_val("wlp_rate", profiles_g[i].rate, i);
		nvram_set_val("wlp_sharedkeyauth", profiles_g[i].sharedkeyauth, i);

		if (profiles_g[i].sharedkeyauth)
		{
		    if (profiles_g[i].wep==STA_ENCRYPTION_ENABLE)
		    {
			if (profiles_g[i].wepkeylen == STA_ENCRYPTION_TYPE_WEP128) nvram_set_val("wlp_weptype", 1, i);
			else nvram_set_val("wlp_weptype", 0, i);
		    }			
		    else nvram_set_val("wlp_weptype", 0, i);
		}
		else
		{
		    if (profiles_g[i].wep==STA_ENCRYPTION_ENABLE)
		    {
			if (profiles_g[i].wepkeylen == STA_ENCRYPTION_TYPE_WEP128) nvram_set_val("wlp_weptype", 2, i);
			else nvram_set_val("wlp_weptype", 1, i);
		    }			
		    else nvram_set_val("wlp_weptype", 0, i);
		}

		if (profiles_g[i].wepkeylen == STA_ENCRYPTION_TYPE_WEP64) 
		{
			weptype = ENCRYPTION_WEP64;
			weplen = 5;
			
			UnpackKey(weptype, profiles_g[i].wepkey1, key[0], key[1], key[2], key[3]);

		}
		else if (profiles_g[i].wepkeylen == STA_ENCRYPTION_TYPE_WEP128)
		{
			weptype = ENCRYPTION_WEP128;
			weplen = 13;
			
			UnpackKey(weptype, profiles_g[i].wepkey1, key[0], key[1], key[2], key[3]);
		}
		else weplen = 0;

		key[0][weplen*2] = 0;
		key[1][weplen*2] = 0;
		key[2][weplen*2] = 0;
		key[3][weplen*2] = 0;

		nvram_set_str("wlp_wepkey1", key[0], i);
		nvram_set_str("wlp_wepkey2", key[1], i);
		nvram_set_str("wlp_wepkey3", key[2], i);
		nvram_set_str("wlp_wepkey4", key[3], i);

		nvram_set_val("wlp_wepkeyactive", profiles_g[i].wepkeyactive+1, i);

		nvram_set_val("wlp_brgmacclone", profiles_g[i].brgmacclone, i);
		nvram_set_val("wlp_preamble", profiles_g[i].preamble, i);
		
		//nvram_set_str("wlp_name", profiles_g[i].name, i);
		//nvram_set_val("wlp_ipaddr", profiles_g[i].ipaddr, i);
		//nvram_set_val("wlp_netmask", profiles_g[i].netmask, i);
		//nvram_set_val("wlp_gateway", profiles_g[i].gateway, i);
		//nvram_set_val("wlp_dhcp", profiles_g[i].dhcp, i);
	}

	nvram_set_val("wlp_num", profiles_g_count, -1);
	nvram_set_val("wlp_scan_type", scan_g_type, -1);
	nvram_set_val("wlp_scan_mode", scan_g_mode, -1);	
	nvram_commit();
}

void check_tools(void)
{
	char cmd[64];
	
	strcpy(cmd, nvram_safe_get("st_tool_t"));

	printf("checking %s\n", cmd);

	if (strlen(cmd)>0)
	{
		nvram_set("st_toolstatus_t", "proceeding");
		nvram_set("st_tool_t", "");
		system(cmd);
	}
}
#endif
