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
#ifdef BTN_SETUP
/* * Man in Middle Attack
 *
 *             -Attacker<-
 *            /           \
 *           /             \
 *          V               \
 *         AP  <----x---- Client
 *  ==> the same as
 *         AP  <--------- Anonymous Client
 *  ==> solved by using in very short range
 *
 * * Key generation(Diffie-Hellman)
 *
 * AP 				
 *	p: random in 100 predefined primes		
 *	q: 5
 *	public_ap/private_ap: DH_generate_key(p, q, private)
 *
 * Client
 *	p: random in 100 predfined primses, rand in SSID
 *	q: 5
 *	public_client/private_client: DH_generate_key(p, q, private)
 *
 * * Process
 * 
 * (a1) Press Button for 3 seconds
 *
 * (a2) Generate Public Key by using:
 *      CreatePubPrivKey()
 *
 * (a3) Change SSID to
 *      ASUS_OTSx_zzz_iiii
 *      x : setting in default or not
 *      zzz : rand seed for primes number
 *      iiii : default ip, if no dhcp server is provided
 *
 * (c1) Survey AP with OTS....
 * (c2) Generate Public Key by using:
 *			CreatePubPrivKey()
 * (c3) Start Session one with PackSetPubKey
 *	<-----OTSInit(SetPubKey)---------
 * (a4) UnpackSetPubKey
 * (a5) For other connection, set into log
 *
 * (a6) PackSetPubKeyRes
 *	------OTSInitAck(SetPubKeyRes)-->
 * (c4) UnpackSetPubKeyAck
 * (c5) Close Session one socket
 *
 * (a7) CreateSharedKey()		(c6) CreateSharedKey
 * (a8) Set to WPA-PSK w/
 *			CreateSharedKey
 *
 * (c7) Start Session Two w/ PackSetInfoGWQuick
 *	<---- OTSExchange(SetInfoGWQuick)- 
 * (a9) UnpackSetInfoGWQuick
 * (a10) For other connection, set into log
 *
 * (a11-1) PackSetInfoGWQuickRes: 
 *				 Apply Setting with QuickFlag = None
 *
 *	----- OTSExchangeRes(SetInfoGWQuickRes) -> Client
 * (c8) UnpackSetInfoGWQuickRes
 *
 * (a11-2) PackSetInfoGWQuickRes
 *				 Response Setting with QuickFlag = Wireless
 *
 *	----- OTSExchangeRes(SetInfoGWQuickRes) -> Client
 * (c8) UnpackSetInfoGWQuickRes
 * (c9) close sesson two socket
 *
 * (a12) save setting and reboot
 *
 * * Timer
 * - 120 seconds, button is pressed and no action is performed.
 * - 20 seconds, button is pressed and OTSInit is sent
 *
 * * Functions
 * DH *DH_new();
 * int CreatePubPrivKey(DH *dh, int rand, char *pub, char *priv);
 * int CreateSharedKey(DH *dh, char *pub, char *shared);
 * int DH_free(DH *dh);
 *
 * Fully Support: ASUS cards, WZC
 * Alert to WZC(WPA) : Centrino or other cards in XP SP2
 * Alert to WZC(WEP) : Centrino or other cards in XP SP1
 * Alert to Ethernet : Other cards in 98/ME/.....
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <sys/mount.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <rc.h>
#include <syslog.h>
#include <iboxcom.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>

#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif

#ifdef RTCONFIG_QCA
#include <qca.h>
#endif

#define logs(fmt, arg...) //syslog(LOG_NOTICE, fmt, ##arg)
//#include <openssl/dh.h>
#include "crypto.c"
DH *dh;

typedef union {
    struct sockaddr sa;
    struct sockaddr_in sa_in;
} usockaddr;

#ifdef FULL_EZSETUP // Added by CHen-I 20080201
#define ENCRYPTION 1
#endif

#define BTNSETUP_INIT_TIMEOUT 		120	// 3 minute
#define BTNSETUP_EXCHANGE_TIMEOUT 	300    	// 3 minutes
#define MAX_DHKEY_LEN 192
#define WEP64_LEN 10
#define WEP128_LEN 26
#define WPAPSK_LEN 63

PKT_SET_INFO_GW_QUICK pkt;
unsigned char pubkey[MAX_DHKEY_LEN];
unsigned char cpubkey[MAX_DHKEY_LEN];
unsigned char sharedkey[MAX_DHKEY_LEN];
char sharedkeystr[MAX_DHKEY_LEN*2+1];
TEMP_WIRELESS *tw; // Point to sharedkeystr


char ssid[32+1];
int bs_mode;
time_t bs_time;
int bs_timeout;
int bs_auth;
int bs_encrypt;
//#define OTS_LOG 1
//#define OTS_SIMU 1

int rand_seed_by_time(void)
{
	time_t atime;
	static unsigned long rand_base = 0;

	time(&atime);
	srand(((unsigned long)atime + rand_base++) % ULONG_MAX);
	
	return rand();
}

#ifdef OTS_SIMU
int ots_simu(int stage)
{
	printf("ots check: %d %d\n", stage, nvram_get_int("ots_simu_stage"));
	if(stage==nvram_get_int("ots_simu_stage"))
	{
		nvram_set("ots_simu_stage", "");
		return 0;
        }
	return 1;
}
#endif

#ifdef OTS_LOG
char ots_buf[1024];

void ots_log(unsigned int flag, int save)
{
	if(flag==0) 
	{
		ots_buf[0]=0;
		nvram_set("ots_log", "0");
	}
	else
	{
		if (save>1)
			sprintf(ots_buf, "%s;[%02x]", nvram_safe_get("ots_log"), flag);
		else
			sprintf(ots_buf, "%s;%02x", nvram_safe_get("ots_log"), flag);
		nvram_set("ots_log", ots_buf);
	}

	if(save==1)
		nvram_commit();
}
#endif

int is_ots()
{
	char *ptr=nvram_safe_get("sharedkeystr");

	if(strlen(ptr)) return 1;
	else return 0;	
}

#ifdef ENCRYPTION
#define BLOCKLEN 16

void Encrypt(int klen, unsigned char *key, unsigned char *ptext, int tlen, unsigned char *ctext)
{
	unsigned char *pptr, *cptr;
	int i;

	i = 0;
	pptr = ptext;
	cptr = ctext;

	while(1)
	{
		aes_encrypt(klen, key, pptr, cptr);
		i+=16;
		if(i>=tlen) break;		
		pptr+=16;
		cptr+=16;
	}
}

void Decrypt(int klen, unsigned char *key, unsigned char *ptext, int tlen, unsigned char *ctext)
{
	unsigned char *pptr, *cptr;
	int i;

	i = 0;
	pptr = ptext;
	cptr = ctext;

	while(1)
	{
		aes_decrypt(klen, key, pptr, cptr);
		i+=16;
		if(i>=tlen) break;		
		pptr+=16;
		cptr+=16;
	}
}
#endif

// some utility
void nvram_set_ip(const char *nvram_name, DWORD ip)
{
	struct in_addr in;

	if (ip!=0)
	{
		in.s_addr = ip;
		nvram_set(nvram_name, (char *)inet_ntoa(in));
	}
	else nvram_set(nvram_name, "");
}

void nvram_set_str(const char *nvram_name, char *value, int size)
{
	char tmpbuf[256];

	tmpbuf[size] = 0;
	memcpy(tmpbuf, value, size);
	nvram_set(nvram_name, tmpbuf);
}

int btn_setup_get_setting(PKT_SET_INFO_GW_QUICK *pkt)	// WLAN 2.4G
{
	char tmpbuf[256];
	int ret=0;

	memset(pkt, 0, sizeof(pkt));

	pkt->QuickFlag=QFCAP_WIRELESS;
	strcpy(tmpbuf, nvram_safe_get("wl0_ssid"));
	memcpy(pkt->WSetting.SSID, tmpbuf, sizeof(pkt->WSetting.SSID));

	if(nvram_match("wl0_auth_mode_x", "open"))
	{
		pkt->WSetting.Auth=AUTHENTICATION_OPEN;
		pkt->WSetting.Encrypt=nvram_get_int("wl0_wep_x");
		if (pkt->WSetting.Encrypt>ENCRYPTION_DISABLE)
		{
			pkt->WSetting.DefaultKey = nvram_get_int("wl0_key");
			sprintf(tmpbuf, "wl0_key%d", pkt->WSetting.DefaultKey);
			strcpy(pkt->WSetting.Key, nvram_safe_get(tmpbuf));
		}
	}
	else if(nvram_match("wl0_auth_mode_x", "shared"))
	{
		pkt->WSetting.Auth=AUTHENTICATION_SHARED;
		pkt->WSetting.Encrypt=nvram_get_int("wl0_wep_x");
		if (pkt->WSetting.Encrypt>ENCRYPTION_DISABLE)
		{
			pkt->WSetting.DefaultKey = nvram_get_int("wl0_key");
			sprintf(tmpbuf, "wl0_key%d", pkt->WSetting.DefaultKey);
			strcpy(pkt->WSetting.Key, nvram_safe_get(tmpbuf));
		}	
	}
	else if(nvram_match("wl0_auth_mode_x", "psk") || nvram_match("wl0_auth_mode_x", "psk2") || nvram_match("wl0_auth_mode_x", "pskpsk2"))
	{
		if (nvram_match("wl0_auth_mode_x", "psk"))
			pkt->WSetting.Auth=AUTHENTICATION_WPA_PSK;
		else if (nvram_match("wl0_auth_mode_x", "psk2"))
			pkt->WSetting.Auth=AUTHENTICATION_WPA_PSK2;
		else if (nvram_match("wl0_auth_mode_x", "pskpsk2"))
			pkt->WSetting.Auth=AUTHENTICATION_WPA_AUTO;

		if (nvram_match("wl0_crypto", "tkip"))
			pkt->WSetting.Encrypt=ENCRYPTION_TKIP;
		else if (nvram_match("wl0_crypto", "aes"))
			pkt->WSetting.Encrypt=ENCRYPTION_AES;
		else
			pkt->WSetting.Encrypt=ENCRYPTION_TKIP_AES;

		strcpy(tmpbuf, nvram_safe_get("wl0_wpa_psk"));
		memcpy(pkt->WSetting.Key, tmpbuf, sizeof(pkt->WSetting.Key));
		pkt->WSetting.DefaultKey=DEFAULT_KEY_1;
	}
	else goto fail;	

	ret = 1;
fail:
	return ret;
}

int btn_setup_get_setting2(PKT_SET_INFO_GW_QUICK *pkt)	// WLAN 5G
{
	char tmpbuf[256];
	int ret=0;

	memset(pkt, 0, sizeof(pkt));

	pkt->QuickFlag=QFCAP_WIRELESS;
	strcpy(tmpbuf, nvram_safe_get("wl1_ssid"));
	memcpy(pkt->WSetting.SSID, tmpbuf, sizeof(pkt->WSetting.SSID));

	if(nvram_match("wl1_auth_mode_x", "open"))
	{
		pkt->WSetting.Auth=AUTHENTICATION_OPEN;
		pkt->WSetting.Encrypt=nvram_get_int("wl1_wep_x");
		if (pkt->WSetting.Encrypt>ENCRYPTION_DISABLE)
		{
			pkt->WSetting.DefaultKey = nvram_get_int("wl1_key");
			sprintf(tmpbuf, "wl1_key%d", pkt->WSetting.DefaultKey);
			strcpy(pkt->WSetting.Key, nvram_safe_get(tmpbuf));
		}
	}
	else if(nvram_match("wl1_auth_mode_x", "shared"))
	{
		pkt->WSetting.Auth=AUTHENTICATION_SHARED;
		pkt->WSetting.Encrypt=nvram_get_int("wl1_wep_x");
		if (pkt->WSetting.Encrypt>ENCRYPTION_DISABLE)
		{
			pkt->WSetting.DefaultKey = nvram_get_int("wl1_key");
			sprintf(tmpbuf, "wl1_key%d", pkt->WSetting.DefaultKey);
			strcpy(pkt->WSetting.Key, nvram_safe_get(tmpbuf));
		}	
	}
	else if(nvram_match("wl1_auth_mode_x", "psk") || nvram_match("wl1_auth_mode_x", "psk2") || nvram_match("wl1_auth_mode_x", "pskpsk2"))
	{
                if (nvram_match("wl1_auth_mode_x", "psk"))
                        pkt->WSetting.Auth=AUTHENTICATION_WPA_PSK;
                else if (nvram_match("wl1_auth_mode_x", "psk2"))
                        pkt->WSetting.Auth=AUTHENTICATION_WPA_PSK2;
                else if (nvram_match("wl1_auth_mode_x", "pskpsk2"))
                        pkt->WSetting.Auth=AUTHENTICATION_WPA_AUTO;

                if (nvram_match("wl1_crypto", "tkip"))
                        pkt->WSetting.Encrypt=ENCRYPTION_TKIP;
                else if (nvram_match("wl1_crypto", "aes"))
                        pkt->WSetting.Encrypt=ENCRYPTION_AES;
                else
                        pkt->WSetting.Encrypt=ENCRYPTION_TKIP_AES;

		strcpy(tmpbuf, nvram_safe_get("wl1_wpa_psk"));
		memcpy(pkt->WSetting.Key, tmpbuf, sizeof(pkt->WSetting.Key));
		pkt->WSetting.DefaultKey=DEFAULT_KEY_1;
	}
	else goto fail;	

	ret = 1;
fail:
	return ret;
}

void btn_setup_save_setting(PKT_SET_INFO_GW_QUICK *pkt)
{
	char tmpbuf[256];
	char sr_name[32];
	char sr_num[1];
        char idx = 0, idx1 = 0;
        char start_num;
        char end_num = 0;
	DWORD dhcp_tmp;


	
	if (pkt->QuickFlag&QFCAP_WIRELESS)
	{
	   printf("Wireless\n");
	   if (!(pkt->QuickFlag&QFCAP_GET))
	   {
		printf("Set\n");
		// assign automatic generate value
		if (pkt->WSetting.SSID[0]==0)
		{
			strncpy(pkt->WSetting.SSID, tw->u.WirelessStruct.SuggestSSID, sizeof(pkt->WSetting.SSID));
		}
		// assign automatic generate value
		if (pkt->WSetting.Key[0]==0)
		{
			strncpy(pkt->WSetting.Key, tw->u.WirelessStruct.SuggestKey, sizeof(pkt->WSetting.Key));
		}

		if (pkt->WSetting.Encrypt==ENCRYPTION_WEP64)
			pkt->WSetting.Key[WEP64_LEN] = 0;
		else if (pkt->WSetting.Encrypt==ENCRYPTION_WEP128)
			pkt->WSetting.Key[WEP128_LEN] = 0;

		// wireless setting
		// 1. ssid
		nvram_set_str("wl0_ssid", pkt->WSetting.SSID, sizeof(pkt->WSetting.SSID));
		nvram_set_str("wl1_ssid", pkt->WSetting.SSID, sizeof(pkt->WSetting.SSID));
		memset(tmpbuf, 0, sizeof(tmpbuf));

		if (pkt->WSetting.Auth==AUTHENTICATION_OPEN)
		{
			nvram_set("wl0_auth_mode_x", "open");
			nvram_set("wl1_auth_mode_x", "open");
			if (pkt->WSetting.Encrypt==ENCRYPTION_WEP64||
			    pkt->WSetting.Encrypt==ENCRYPTION_WEP128)
			{
				sprintf(tmpbuf, "%d", pkt->WSetting.Encrypt);
				nvram_set("wl0_wep_x", tmpbuf);
				nvram_set("wl1_wep_x", tmpbuf);

				if(pkt->WSetting.DefaultKey>DEFAULT_KEY_4||
				   pkt->WSetting.DefaultKey<DEFAULT_KEY_1) 
				   pkt->WSetting.DefaultKey=DEFAULT_KEY_1;

				if(pkt->WSetting.DefaultKey==DEFAULT_KEY_1)
				{
					nvram_set_str("wl0_key1", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
					nvram_set_str("wl1_key1", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
				}
				else if(pkt->WSetting.DefaultKey==DEFAULT_KEY_2)
				{
					nvram_set_str("wl0_key2", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
					nvram_set_str("wl1_key2", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
				}
				else if(pkt->WSetting.DefaultKey==DEFAULT_KEY_3)
				{
					nvram_set_str("wl0_key3", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
					nvram_set_str("wl1_key3", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
				}
				else if(pkt->WSetting.DefaultKey==DEFAULT_KEY_4)
				{
					nvram_set_str("wl0_key4", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
					nvram_set_str("wl1_key4", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
				}
				sprintf(tmpbuf,"%d", pkt->WSetting.DefaultKey);
				nvram_set("wl0_key", tmpbuf);
				nvram_set("wl1_key", tmpbuf);
			}
			else 
			{
				nvram_set("wl0_key", "1");
				nvram_set("wl0_key1", "");
				nvram_set("wl0_key2", "");
				nvram_set("wl0_key3", "");
				nvram_set("wl0_key4", "");

				nvram_set("wl1_key", "1");
				nvram_set("wl1_key1", "");
				nvram_set("wl1_key2", "");
				nvram_set("wl1_key3", "");
				nvram_set("wl1_key4", "");
			}
			nvram_set("wl0_wpa_psk","");/* Cherry Cho added for removing temporary key used by WSC in 2007/3/8. */
			nvram_set("wl1_wpa_psk","");
		}
		else if(pkt->WSetting.Auth==AUTHENTICATION_SHARED)
		{	
			nvram_set("wl0_auth_mode_x", "shared");
			nvram_set("wl1_auth_mode_x", "shared");
			if (pkt->WSetting.Encrypt==ENCRYPTION_WEP64 ||
			    pkt->WSetting.Encrypt==ENCRYPTION_WEP128)
			{
				sprintf(tmpbuf, "%d", pkt->WSetting.Encrypt);
				nvram_set("wl0_wep_x", tmpbuf);
				nvram_set("wl1_wep_x", tmpbuf);
				if(pkt->WSetting.DefaultKey>DEFAULT_KEY_4 ||
				   pkt->WSetting.DefaultKey<DEFAULT_KEY_1) 
				   pkt->WSetting.DefaultKey=DEFAULT_KEY_1;
				if(pkt->WSetting.DefaultKey==DEFAULT_KEY_1)
				{
					nvram_set_str("wl0_key1", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
					nvram_set_str("wl1_key1", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
				}
				else if(pkt->WSetting.DefaultKey==DEFAULT_KEY_2)
				{
					nvram_set_str("wl0_key2", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
					nvram_set_str("wl1_key2", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
				}
				else if(pkt->WSetting.DefaultKey==DEFAULT_KEY_3)
				{
					nvram_set_str("wl0_key3", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
					nvram_set_str("wl1_key3", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
				}
				else if(pkt->WSetting.DefaultKey==DEFAULT_KEY_4)
				{
					nvram_set_str("wl0_key4", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
					nvram_set_str("wl1_key4", pkt->WSetting.Key, sizeof(pkt->WSetting.Key));
				}
				else goto fail;
				sprintf(tmpbuf,"%d", pkt->WSetting.DefaultKey);
				nvram_set("wl0_key", tmpbuf);
				nvram_set("wl1_key", tmpbuf);
			}
			nvram_set("wl0_wpa_psk","");/* Cherry Cho added for removing temporary key used by WSC in 2007/3/8. */
			nvram_set("wl1_wpa_psk","");
		}
		else if((pkt->WSetting.Auth==AUTHENTICATION_WPA_PSK) || 
			(pkt->WSetting.Auth==AUTHENTICATION_WPA_PSK2)||
			(pkt->WSetting.Auth==AUTHENTICATION_WPA_AUTO))
		{
			if(pkt->WSetting.Auth==AUTHENTICATION_WPA_PSK)
			{
				nvram_set("wl0_auth_mode_x", "psk");
				nvram_set("wl1_auth_mode_x", "psk");
				nvram_set("wl0_crypto", "tkip");
				nvram_set("wl1_crypto", "tkip");
			}
			else if (pkt->WSetting.Auth==AUTHENTICATION_WPA_PSK2)
			{
				nvram_set("wl0_auth_mode_x", "psk2");
				nvram_set("wl1_auth_mode_x", "psk2");
				nvram_set("wl0_crypto", "aes");
				nvram_set("wl1_crypto", "aes");
			}
			else if (pkt->WSetting.Auth==AUTHENTICATION_WPA_AUTO)
			{
				nvram_set("wl0_auth_mode_x", "pskpsk2");
				nvram_set("wl1_auth_mode_x", "pskpsk2");

				nvram_set("wl0_crypto", "tkip+aes");
				nvram_set("wl1_crypto", "tkip+aes");
			}

			nvram_set("wl0_wep_x", "0");	
			nvram_set_str("wl0_wpa_psk", pkt->WSetting.Key, WPAPSK_LEN);
			nvram_set("wl0_key", "1");
			nvram_set("wl0_key1", "");
			nvram_set("wl0_key2", "");
			nvram_set("wl0_key3", "");
			nvram_set("wl0_key4", "");

			nvram_set("wl1_wep_x", "0");	
			nvram_set_str("wl1_wpa_psk", pkt->WSetting.Key, WPAPSK_LEN);
			nvram_set("wl1_key", "1");
			nvram_set("wl1_key1", "");
			nvram_set("wl1_key2", "");
			nvram_set("wl1_key3", "");
			nvram_set("wl1_key4", "");
		}
		else goto fail;

		nvram_set("x_Setting", "1");
		nvram_set("x_EZSetup", "1");
		nvram_set("wl0_wsc_config_state", "1");
		nvram_set("wl1_wsc_config_state", "1");
		nvram_commit();
	   }
	}		
	if (pkt->QuickFlag&QFCAP_ISP)
	{
		// ISP setting
		if(pkt->ISPSetting.ISPType==ISP_TYPE_DHCPCLIENT)
		{
			nvram_set("wan_proto", "dhcp");	
			nvram_set_str("wan_hostname", pkt->ISPSetting.HostName, sizeof(pkt->ISPSetting.HostName));
			nvram_set_str("wan_hwaddr_x", pkt->ISPSetting.MAC, sizeof(pkt->ISPSetting.MAC));
			nvram_set_str("wan_heartbeat_x", "", sizeof(pkt->ISPSetting.BPServer));
		}
		else if(pkt->ISPSetting.ISPType==ISP_TYPE_PPPOE)
		{
			nvram_set("wan_proto", "pppoe");
			nvram_set_str("wan_pppoe_username", pkt->ISPSetting.UserName, sizeof(pkt->ISPSetting.UserName));
			nvram_set_str("wan_pppoe_passwd", pkt->ISPSetting.Password, sizeof(pkt->ISPSetting.Password));
			/* for RU, James */
                        nvram_set_ip("wan_ipaddr", pkt->ISPSetting.IPAddr);
                        nvram_set_ip("wan_netmask", pkt->ISPSetting.Mask);
                        nvram_set_ip("wan_gateway", pkt->ISPSetting.Gateway);

	                nvram_set_str("wan_hostname", pkt->ISPSetting.HostName, sizeof(pkt->ISPSetting.HostName));
                        nvram_set_str("wan_hwaddr_x", pkt->ISPSetting.MAC, sizeof(pkt->ISPSetting.MAC));
                        nvram_set_str("wan_heartbeat_x", pkt->ISPSetting.BPServer, sizeof(pkt->ISPSetting.BPServer));
		}	
		else if(pkt->ISPSetting.ISPType==ISP_TYPE_PPTP)
		{
			nvram_set("wan_proto", "pptp");
			nvram_set_str("wan_pppoe_username", pkt->ISPSetting.UserName, sizeof(pkt->ISPSetting.UserName));
			nvram_set_str("wan_pppoe_passwd", pkt->ISPSetting.Password, sizeof(pkt->ISPSetting.Password));
			nvram_set_ip("wan_ipaddr", pkt->ISPSetting.IPAddr);
			nvram_set_ip("wan_netmask", pkt->ISPSetting.Mask);
			nvram_set_ip("wan_gateway", pkt->ISPSetting.Gateway);
			if (pkt->ISPSetting.PPTPOption == PPTP_OPTION_NOENCRYPT)
                                nvram_set("wan_pptp_options_x", "-mppc");
                        else if (pkt->ISPSetting.PPTPOption == PPTP_OPTION_MPPE40)
                                nvram_set("wan_pptp_options_x", "+mppe-40");
                        else if (pkt->ISPSetting.PPTPOption == PPTP_OPTION_MPPE56)
                                nvram_set("wan_pptp_options_x", "+mppe-56");
                        else if (pkt->ISPSetting.PPTPOption == PPTP_OPTION_MPPE128)
                                nvram_set("wan_pptp_options_x", "+mppe-128");
                        else
                                nvram_set("wan_pptp_options_x", "");

                        nvram_set_str("wan_hostname", pkt->ISPSetting.HostName, sizeof(pkt->ISPSetting.HostName));
                        nvram_set_str("wan_hwaddr_x", pkt->ISPSetting.MAC, sizeof(pkt->ISPSetting.MAC));
                        nvram_set_str("wan_heartbeat_x", pkt->ISPSetting.BPServer, sizeof(pkt->ISPSetting.BPServer));
                        //if ((pkt->ISPSetting.LAN_IPAddr) != 0)
                        //        nvram_set_ip("lan_ipaddr", pkt->ISPSetting.LAN_IPAddr);
			if ((pkt->ISPSetting.LAN_IPAddr) != 0)
			{
                                nvram_set_ip("lan_ipaddr", pkt->ISPSetting.LAN_IPAddr);
				dhcp_tmp = pkt->ISPSetting.LAN_IPAddr;
				dhcp_tmp = (dhcp_tmp&0x00ffffff);
				dhcp_tmp = (dhcp_tmp|0x02000000);
                                nvram_set_ip("dhcp_start", dhcp_tmp);
				dhcp_tmp =(dhcp_tmp|0xfe000000);
                                nvram_set_ip("dhcp_end", dhcp_tmp);
			}
		}
		else if(pkt->ISPSetting.ISPType==ISP_TYPE_STATICIP)
		{
			nvram_set("wan_proto", "static");
			nvram_set_ip("wan_ipaddr", pkt->ISPSetting.IPAddr);
			nvram_set_ip("wan_netmask", pkt->ISPSetting.Mask);
			nvram_set_ip("wan_gateway", pkt->ISPSetting.Gateway);
			nvram_set_str("wan_hostname", pkt->ISPSetting.HostName, sizeof(pkt->ISPSetting.HostName));
			nvram_set_str("wan_hwaddr_x", pkt->ISPSetting.MAC, sizeof(pkt->ISPSetting.MAC));
			nvram_set_str("wan_heartbeat_x", "", sizeof(pkt->ISPSetting.BPServer));
			if ((pkt->ISPSetting.LAN_IPAddr) != 0)
			{
                                nvram_set_ip("lan_ipaddr", pkt->ISPSetting.LAN_IPAddr);
				dhcp_tmp = pkt->ISPSetting.LAN_IPAddr;
				dhcp_tmp = (dhcp_tmp&0x00ffffff);
				dhcp_tmp = (dhcp_tmp|0x02000000);
                                nvram_set_ip("dhcp_start", dhcp_tmp);
				dhcp_tmp =(dhcp_tmp|0xfe000000);
                                nvram_set_ip("dhcp_end", dhcp_tmp);
			}
		}
		else if(pkt->ISPSetting.ISPType==ISP_TYPE_L2TP)
                {
                        nvram_set("wan_proto", "l2tp");
                        nvram_set_str("wan_pppoe_username", pkt->ISPSetting.UserName, sizeof(pkt->ISPSetting.UserName));
                        nvram_set_str("wan_pppoe_passwd", pkt->ISPSetting.Password, sizeof(pkt->ISPSetting.Password));
                        nvram_set_ip("wan_ipaddr", pkt->ISPSetting.IPAddr);
                        nvram_set_ip("wan_netmask", pkt->ISPSetting.Mask);
                        nvram_set_ip("wan_gateway", pkt->ISPSetting.Gateway);
                        nvram_set_str("wan_hostname", pkt->ISPSetting.HostName, sizeof(pkt->ISPSetting.HostName));
                        nvram_set_str("wan_hwaddr_x", pkt->ISPSetting.MAC, sizeof(pkt->ISPSetting.MAC));
                        nvram_set_str("wan_heartbeat_x", pkt->ISPSetting.BPServer, sizeof(pkt->ISPSetting.BPServer));
                }

//		printf("ISPType is %x\n",pkt->ISPSetting.ISPType);
//		printf("PacketNum is %x\n",pkt->ISPSetting.PacketNum);
//		printf("SRNum is %x\n",pkt->ISPSetting.SRNum);
			if(pkt->ISPSetting.IPAddr != 0)
				nvram_set("x_DHCPClient","0");

		if(pkt->ISPSetting.ISPType==ISP_TYPE_STATICIP ||
                   pkt->ISPSetting.ISPType==ISP_TYPE_PPTP ||
                   pkt->ISPSetting.ISPType==ISP_TYPE_PPPOE)
                {
                        if(pkt->ISPSetting.PacketNum == SR_PACKET_1 || pkt->ISPSetting.PacketNum == SR_PACKET_2)
                        {
                                if(pkt->ISPSetting.PacketNum == SR_PACKET_1)
                                {       start_num = 0;
                                       if(pkt->ISPSetting.SRNum<13)
                                                end_num = pkt->ISPSetting.SRNum;
                                        else
                                                end_num = 12;
					idx1 = 0;
                                }
                                if(pkt->ISPSetting.PacketNum == SR_PACKET_2)
                                {       start_num = 0;
                                        end_num = (pkt->ISPSetting.SRNum - 12);
					idx1 = 12;
				}

				
				sprintf(sr_num,"%d",pkt->ISPSetting.SRNum);
                                nvram_set("sr_num_x",sr_num);


//				printf("start_num is %x, end_num is %x\n",start_num, end_num);

                                for(idx=start_num;idx<end_num;idx++)
                                {
                                        memset(sr_name, 0, sizeof(sr_name));
                                        sprintf(sr_name,"%s%d", "sr_if_x", (idx+idx1));
                                        nvram_set_str(sr_name, "WAN", 3);
                                        sprintf(sr_name,"%s%d", "sr_ipaddr_x", (idx+idx1));
                                        nvram_set_ip(sr_name, pkt->ISPSetting.SR_IPAddr[(int)idx]);
                                        sprintf(sr_name,"%s%d", "sr_netmask_x", (idx+idx1));
                                        nvram_set_ip(sr_name, pkt->ISPSetting.SR_Mask[(int)idx]);
                                        sprintf(sr_name,"%s%d", "sr_gateway_x", (idx+idx1));
                                        nvram_set(sr_name, "0.0.0.0");
                                }
                        }      
                }


		if (pkt->ISPSetting.DNSServer1==0 && pkt->ISPSetting.DNSServer2==0)
		{
			nvram_set("wan_dnsenable_x", "1");
		}
		else
		{
			nvram_set("wan_dnsenable_x", "0");
			nvram_set_ip("wan_dns1_x", pkt->ISPSetting.DNSServer1);
			nvram_set_ip("wan_dns2_x", pkt->ISPSetting.DNSServer2);
		}

		if (pkt->ISPSetting.DHCPRoute == 0)
                        nvram_set("dr_enable_x", "0");
                else
                        nvram_set("dr_enable_x", "1");

                if (pkt->ISPSetting.MulticastRoute == 0)
                        nvram_set("mr_enable_x", "0");
                else
                        nvram_set("mr_enable_x", "1");

                if (pkt->ISPSetting.StaticRoute == 0)
                        nvram_set("sr_enable_x", "0");
                else
                        nvram_set("sr_enable_x", "1");

                if (pkt->ISPSetting.WANBridgePort == WAN_BRIDGE_NONE)
                        nvram_set("switch_stb_x", "0");
                else if (pkt->ISPSetting.WANBridgePort == WAN_BRIDGE_LAN1)
                        nvram_set("switch_stb_x", "1");
                else if (pkt->ISPSetting.WANBridgePort == WAN_BRIDGE_LAN2)
                        nvram_set("switch_stb_x", "2");
                else if (pkt->ISPSetting.WANBridgePort == WAN_BRIDGE_LAN3)
                        nvram_set("swtich_stb_x", "3");
                else if (pkt->ISPSetting.WANBridgePort == WAN_BRIDGE_LAN4)
                        nvram_set("switch_stb_x", "4");
                else if (pkt->ISPSetting.WANBridgePort == WAN_BRIDGE_LAN3LAN4)
                        nvram_set("switch_stb_x", "5");

		if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_AUTO)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "13");
		}
		else if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_1)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "1");
		}
		else if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_2)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "2");
		}
		else if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_5)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "3");
		}
		else if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_6)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "4");
		}
		else if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_9)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "5");
		}
		else if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_11)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "6");
		}
		else if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_12)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "7");
		}
		else if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_18)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "8");
		}
		else if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_24)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "9");
		}
		else if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_36)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "10");
		}
		else if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_48)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "11");
		}
		else if (pkt->ISPSetting.MulticastRate == MULTICAST_RATE_54)
		{
			nvram_set("wl0_mrate", "13");
			nvram_set("wl1_mrate", "12");
		}
/* 1023 update*/

	
		nvram_set("time_zone", pkt->ISPSetting.TimeZone);
		time_zone_x_mapping();
		setenv("TZ", nvram_safe_get("time_zone_x"), 1);
		nvram_set("x_Setting", "1");
	}		
	if (pkt->QuickFlag&QFCAP_FINISH)
	{
		nvram_set("sharedkeystr", "");	
	}
	else if(pkt->QuickFlag&QFCAP_REBOOT) 
	{
	   	nvram_set("sharedkeystr", sharedkeystr);
	}
	//convert_asus_values();
	//nvram_commit_safe();
fail:
	printf("fails\n");;

}


int OTSStart(int flag)
{
	// stop other service

	if (flag)
	{
		//stop_service_main(1);
		/* start_sdhcpd(); */
		strcpy(sharedkeystr, nvram_safe_get("sharedkeystr"));
		tw = (TEMP_WIRELESS *)sharedkeystr;
		nvram_set("sharedkeystr", "");
		nvram_commit();
		time(&bs_time);
		bs_mode=BTNSETUP_DATAEXCHANGE_EXTEND;
		bs_timeout = BTNSETUP_EXCHANGE_TIMEOUT;
	}
	else
	{
#ifdef FULL_EZSETUP
		stop_service_main(1);
		/* start_sdhcpd(); */

		BN_register_RAND(ots_rand);

		dh = NULL;
		dh = DH_init(p1536, 192, 5);
		if (!DH_generate_key(pubkey,dh)) goto err;

		/* Start button setup process */
		/* SSID : [ProductID]_OTS[Default]_[Prime]*/
		if (nvram_match("x_Setting", "1")) // not in default
			sprintf(ssid, "%s_OTS1", get_productid()); 
		else sprintf(ssid, "%s_OTS0", get_productid()); 

#if defined(RTCONFIG_HAS_5G)
		doSystem("iwpriv %s set AuthMode=OPEN", WIF_5G);
		doSystem("iwpriv %s set EncrypType=NONE", WIF_5G);
		doSystem("iwpriv %s set IEEE8021X=0", WIF_5G);
		doSystem("iwpriv %s set SSID=ASUS_OTS", WIF_5G);
#endif	/* RTCONFIG_HAS_5G */

		doSystem("iwpriv %s set AuthMode=OPEN", WIF2G);
		doSystem("iwpriv %s set EncrypType=NONE", WIF2G);
		doSystem("iwpriv %s set IEEE8021X=0", WIF2G);
		doSystem("iwpriv %s set SSID=ASUS_OTS", WIF2G);

		bs_mode = BTNSETUP_START;
#else
		bs_mode = BTNSETUP_DATAEXCHANGE;
#endif		
		bs_timeout = BTNSETUP_INIT_TIMEOUT;
		time(&bs_time);
	}
	return 1;

#ifdef FULL_EZSETUP
err:
#endif
	if (dh) 
	{
		DH_free(dh);
		dh=NULL;
	}
	return 0;
}

int
OTSExchange(int auth, int encrypt)
{
	int ret = 0;
	int i;
	char SSID[32+1];
	char Key[64+1];


//	printf("OTSExchange\n");


	if (auth==-1&&encrypt==-1) 
	{
//		printf("auth = %d and encrypt = %d\n", auth, encrypt);
		return ret;
	}

	

	// generate shared key
	if (!DH_compute_key(sharedkey, cpubkey, MAX_DHKEY_LEN, dh)) 
	{
//		 printf("dbg: DH_compute_key err\n");
		goto err;
	}

	sharedkeystr[0] = 0;
	for(i=0;i<MAX_DHKEY_LEN;i++)
	{
		 sprintf(sharedkeystr, "%s%02X", sharedkeystr, (unsigned char )sharedkey[i]);
	}

	tw = (TEMP_WIRELESS *) sharedkeystr;
	strncpy(SSID, tw->u.WirelessStruct.TempSSID, sizeof(SSID));
	SSID[32]=0;
	strncpy(Key, tw->u.WirelessStruct.TempKey, sizeof(Key));
	Key[64]=0;

//	printf("SSID = %s, KEY = %s\n", SSID, Key);
	
#ifdef ENCRYPTION
	// using layer 3 encryption
#else
	if(auth==AUTHENTICATION_SHARED)
		eval("wl", "auth", "shared");
	else if(auth==AUTHENTICATION_WPA_PSK)
		eval("wl", "auth", "psk");
	else
		eval("wl", "auth", "open");

	if (encrypt==ENCRYPTION_WEP64)
	{
		Key[WEP64_LEN]=0;
		eval("wl","wep", Key);
	}			
	else if (encrypt==ENCRYPTION_WEP128)
	{
		Key[WEP128_LEN]=0;
		eval("wl","wep", Key);
	}
	else if (encrypt==ENCRYPTION_TKIP)
	{
		Key[WPAPSK_LEN] = 0;
		eval("wl", "tkip", Key);
	}

	eval("wl","ssid", SSID);

#endif
	ret = 1;
	return ret;
err:
	if (dh) 
	{
		DH_free(dh);
		dh=NULL;
	}
	return ret;
}

static int
OTS_socket_init( usockaddr* usaP )
{
    int listen_fd;
    int i;

    memset( usaP, 0, sizeof(usockaddr) );
    usaP->sa.sa_family = AF_INET;
    usaP->sa_in.sin_addr.s_addr = htonl( INADDR_ANY );
    usaP->sa_in.sin_port = htons(OTSPORT);

    listen_fd = socket( usaP->sa.sa_family, SOCK_STREAM, 0 );

    if ( listen_fd < 0 )
    {
	perror( "socket" );
	return -1;
    }

    i = 1;
    if ( setsockopt( listen_fd, SOL_SOCKET, SO_REUSEADDR, (char*) &i, sizeof(i) ) < 0 )
	{
	perror( "setsockopt" );
	return -1;
	}
    if ( bind( listen_fd, &usaP->sa, sizeof(struct sockaddr_in) ) < 0 )
	{
	perror( "bind" );
	return -1;
	}
    if ( listen( listen_fd, 5) < 0 )
	{
	perror( "listen" );
	return -1;
	}
    return listen_fd;
}

void OTSFinish(int fd, int flag)
{
    	shutdown(fd, 2);
    	close(fd);

	//if (flag) kill(1, SIGHUP);
	//else kill(1, SIGTERM);
	kill(1, SIGTERM);
}

char pdubuf[INFO_PDU_LENGTH];	
char pdubuf_res[INFO_PDU_LENGTH];

static int 
waitsock(int fd, int sec, int usec)
{
	struct timeval tv;
	fd_set fdvar;
	int res;
	
	FD_ZERO(&fdvar);
	FD_SET(fd, &fdvar);
	tv.tv_sec = sec;
	tv.tv_usec = usec;
	res = select(fd+1, &fdvar, NULL, NULL, &tv);

	return res;
}

#define isWAN_DETECT_FILE "/tmp/tcpcheck_result_isWAN"
int iw_debug = 1;

int isWAN_detect()
{
	FILE *fp = NULL;
	char line[80], cmd[128];
	char *detect_host[] = {"8.8.8.8", "208.67.220.220", "208.67.222.222"};
	int i;

	if (iw_debug) fprintf(stderr, "## isWAN_detect: internet status ##\n");

	remove(isWAN_DETECT_FILE);
	i = rand_seed_by_time() % 3;
	snprintf(cmd, sizeof(cmd), "/usr/sbin/tcpcheck 5 %s:53 %s:53 >%s", detect_host[i], detect_host[(i+1)%3], isWAN_DETECT_FILE);
	if (iw_debug) fprintf(stderr, "cmd: %s\n", cmd);
	system(cmd);
	if (iw_debug)
	{
		snprintf(cmd, sizeof(cmd), "cat %s", isWAN_DETECT_FILE);
		system(cmd);
//		doSystem("cat %s", isWAN_DETECT_FILE);
	}

        if ((fp = fopen(isWAN_DETECT_FILE, "r")) != NULL)
        {
		while(1)
		{
			if ( fgets(line, sizeof(line), fp) != NULL )
			{
				if (strstr(line, "alive"))
				{
					if (iw_debug) fprintf(stderr, "isWAN_detect: got response!\n");
					fclose(fp);
					return 1;
				}
			}
			else
				break;
		}

		fclose(fp);
		if (iw_debug) fprintf(stderr, "isWAN_detect: no response!\n");
		return 0;
	}

	if (iw_debug) fprintf(stderr, "isWAN_detect: fopen %s error!\n", isWAN_DETECT_FILE);
	return 0;
}

int OTSPacketHandler(int sockfd)
{
    IBOX_COMM_PKT_HDR_EX *phdr;
    IBOX_COMM_PKT_RES_EX *phdr_res;
#ifdef ENCRYPTION
    char tmpbuf[INFO_PDU_LENGTH];
#endif
	
    int i, len;
    char *buf;

    if (waitsock(sockfd, 2, 0)<=0)
    {
	syslog(LOG_NOTICE, "Connect Timeout %x\n", bs_mode);
	close(sockfd); 
	return 0;
    }

    buf = pdubuf; 
    len = sizeof(pdubuf);
    /* Parse headers */

    while ((i=read(sockfd, buf, len))&&len>0)
    {
	len-=i;
	buf+=i;
    }

#ifdef DEBUG
    _dprintf("recv: %x\n", len);
    for(i=0;i<sizeof(pdubuf);i++)
    {
	if(i%16==0) _dprintf("\n");
	_dprintf("%02x ", (unsigned char)pdubuf[i]);
    }
#endif

#ifdef ENCRYPTION
    phdr = (IBOX_COMM_PKT_HDR_EX *)pdubuf;
    phdr_res = (IBOX_COMM_PKT_RES_EX *)pdubuf_res;
    
    if (bs_mode>BTNSETUP_START &&
       !( bs_mode==BTNSETUP_DATAEXCHANGE &&
	phdr->ServiceID==NET_SERVICE_ID_IBOX_INFO &&//Second Chance,2005/07/18
        phdr->PacketType==NET_PACKET_TYPE_CMD &&
        (phdr->OpCode==NET_CMD_ID_EZPROBE || phdr->OpCode==NET_CMD_ID_SETKEY_EX)
	)
) 
    {	
    	Decrypt(sizeof(tw->u.WirelessStruct.TempKey),
		tw->u.WirelessStruct.TempKey,
		pdubuf, INFO_PDU_LENGTH,
		tmpbuf);

    	phdr = (IBOX_COMM_PKT_HDR_EX *)tmpbuf;
    	phdr_res = (IBOX_COMM_PKT_RES_EX *)pdubuf_res;
    } 	
    else
    {
    	phdr = (IBOX_COMM_PKT_HDR_EX *)pdubuf;
    	phdr_res = (IBOX_COMM_PKT_RES_EX *)pdubuf_res;
    }
#else
    phdr = (IBOX_COMM_PKT_HDR_EX *)pdubuf;
    phdr_res = (IBOX_COMM_PKT_RES_EX *)pdubuf_res;
#endif

    //syslog(LOG_NOTICE, "Data Packet XXX %x %x %x %x\n", phdr->ServiceID, phdr->PacketType, bs_mode, phdr->OpCode);

    if (phdr->ServiceID==NET_SERVICE_ID_IBOX_INFO &&
        phdr->PacketType==NET_PACKET_TYPE_CMD)
    {	    	
	phdr_res->ServiceID=NET_SERVICE_ID_IBOX_INFO;
	phdr_res->PacketType=NET_PACKET_TYPE_RES;
	phdr_res->OpCode =phdr->OpCode;
	phdr_res->Info = phdr->Info;
	memcpy(phdr_res->MacAddress, phdr->MacAddress, sizeof(phdr_res->MacAddress));
#ifdef OTS_LOG
	if (phdr->OpCode!=NET_CMD_ID_EZPROBE)
		ots_log(phdr->OpCode+0x90, 0);
#endif
	//syslog(LOG_NOTICE, "Data Packet %x %x\n", bs_mode, phdr->OpCode);

//	printf("Data Packet %x %x\n", bs_mode, phdr->OpCode);

	switch(phdr->OpCode)
	{
		case NET_CMD_ID_EZPROBE:
		{
		     PKT_EZPROBE_INFO *ezprobe_res;
		     
		     ezprobe_res = (PKT_EZPROBE_INFO *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES_EX));		

		     ezprobe_res->isNotDefault = nvram_get_int("x_Setting") | nvram_get_int("wl0_wsc_config_state") | nvram_get_int("wl1_wsc_config_state"); // for EZSetup to coexist w/ WSC
		     ezprobe_res->isSetByOts = nvram_get_int("x_EZSetup");
//		     ezprobe_res->isWAN = is_phyconnected(nvram_safe_get("wan_ifname"));
		     ezprobe_res->isWAN = isWAN_detect();
		     ezprobe_res->isDHCP = 0;
		     ezprobe_res->isPPPOE = 0;

		     if (nvram_match("wl0_auth_mode_x", "shared"))
				ezprobe_res->Auth = AUTHENTICATION_SHARED;
		     else if (nvram_match("wl0_auth_mode_x", "pskpsk2"))
				ezprobe_res->Auth = AUTHENTICATION_WPA_AUTO;
		     else if (nvram_match("wl0_auth_mode_x", "psk"))
				ezprobe_res->Auth = AUTHENTICATION_WPA_PSK;
		     else if (nvram_match("wl0_auth_mode_x", "psk2"))
				ezprobe_res->Auth = AUTHENTICATION_WPA_PSK2;
		     else if (nvram_match("wl0_auth_mode_x", "wpa") || nvram_match("wl0_auth_mode_x", "wpa2") || nvram_match("wl0_auth_mode_x", "wpawpa2"))
				ezprobe_res->Auth = AUTHENTICATION_WPA;
		     else if (nvram_match("wl0_auth_mode_x", "radius"))
				ezprobe_res->Auth = AUTHENTICATION_8021X;
		     else ezprobe_res->Auth = AUTHENTICATION_OPEN;

		     if (nvram_match("wl0_macmode", "allow"))
				ezprobe_res->Acl = ACL_MODE_ACCEPT;
		     else if (nvram_match("wl0_macmode", "deny"))
				ezprobe_res->Acl = ACL_MODE_REJECT;
		     else ezprobe_res->Acl = ACL_MODE_DISABLE;

		     if (nvram_match("wl0_mode_x", "1"))
				ezprobe_res->Wds = WDS_MODE_WDS_ONLY;
		     else if (nvram_match("wl0_mode_x", "2"))
				ezprobe_res->Wds = WDS_MODE_HYBRID;
		     else ezprobe_res->Wds = WDS_MODE_AP_ONLY;

		     strcpy(ezprobe_res->ProductID, get_productid()); 
		     strcpy(ezprobe_res->FirmwareVersion, nvram_safe_get("firmver"));
		     time(&bs_time); // reset timer only
		     bs_auth=-1;
		     bs_encrypt=-1;

#ifdef OTS_SIMU
		     if(!ots_simu(1)) return INFO_PDU_LENGTH;	
#endif
		     send(sockfd, pdubuf_res, sizeof(pdubuf_res), 0);
		     return INFO_PDU_LENGTH;		  
		}	
		case NET_CMD_ID_SETKEY_EX:
		{
		     PKT_SET_INFO_GW_QUICK_KEY *pkey;
		     PKT_SET_INFO_GW_QUICK_KEY *pkey_res;

		     if (bs_mode!=BTNSETUP_START 
			&& bs_mode != BTNSETUP_DATAEXCHANGE // allow second change, 2005/07/18, Chen-I
			) 
		     {	
		     	  bs_auth=-1;
		     	  bs_encrypt=-1;
		  	  return 0;
                     }

		     pkey=(PKT_SET_INFO_GW_QUICK_KEY *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));
		     pkey_res = (PKT_SET_INFO_GW_QUICK_KEY *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES_EX));

		     if(pkey->KeyLen==0) return 0;
		     else memcpy(cpubkey, pkey->Key, MAX_DHKEY_LEN);

		     bs_mode = BTNSETUP_DATAEXCHANGE;
		     time(&bs_time);
		     bs_timeout=BTNSETUP_EXCHANGE_TIMEOUT;
		     bs_auth=pkey->Auth;
		     bs_encrypt=pkey->Encrypt;
		     pkey_res->Auth = pkey->Auth;
		     pkey_res->Encrypt = pkey->Encrypt;
		     pkey_res->KeyLen = MAX_DHKEY_LEN;
		     memcpy(pkey_res->Key, pubkey, MAX_DHKEY_LEN);
#ifdef OTS_SIMU
		     if(!ots_simu(2)) return INFO_PDU_LENGTH;
#endif
		     send(sockfd, pdubuf_res, sizeof(pdubuf_res), 0);
		     return INFO_PDU_LENGTH;
		}
		case NET_CMD_ID_QUICKGW_EX:
		{
		     PKT_SET_INFO_GW_QUICK *gwquick;
		     PKT_SET_INFO_GW_QUICK *gwquick_res;

		     if (bs_mode!=BTNSETUP_DATAEXCHANGE && bs_mode!=BTNSETUP_DATAEXCHANGE_EXTEND) 
		     {
			 return 0;
		     }
#ifdef ENCRYPTION
		     gwquick=(PKT_SET_INFO_GW_QUICK *)(tmpbuf+sizeof(IBOX_COMM_PKT_HDR_EX));
#else
		     gwquick=(PKT_SET_INFO_GW_QUICK *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));
#endif

		     gwquick_res = (PKT_SET_INFO_GW_QUICK *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES_EX));

//		     printf("Flag: %x\n", gwquick->QuickFlag);
		     btn_setup_save_setting(gwquick);

#ifdef OTS_LOG
	if (phdr->OpCode!=NET_CMD_ID_EZPROBE)
		ots_log(phdr->OpCode + gwquick->QuickFlag, 2);
#endif

		     if((gwquick->QuickFlag&QFCAP_WIRELESS)&&
			(gwquick->QuickFlag&QFCAP_GET)) // get setting
		     {	
			 btn_setup_get_setting(gwquick_res);
		     	 gwquick_res->QuickFlag = QFCAP_WIRELESS;
		     }
		     else if((gwquick->QuickFlag&QFCAP_WIRELESS2)&&
			(gwquick->QuickFlag&QFCAP_GET)) // get setting
		     {	
			 btn_setup_get_setting2(gwquick_res);
		     	 gwquick_res->QuickFlag = QFCAP_WIRELESS;
		     }	
		     else
		     {	
			 memcpy(gwquick_res, gwquick, sizeof(PKT_SET_INFO_GW_QUICK));
		     }	

		     if((gwquick->QuickFlag&QFCAP_FINISH)) // finish
		     {	
		     	 bs_mode = BTNSETUP_FINISH;
		     	 bs_timeout=0;
#ifdef OTS_SIMU
		     	 if(!ots_simu(3)) return INFO_PDU_LENGTH;
#endif
		     }
		     else if((gwquick->QuickFlag&QFCAP_REBOOT)) //reboot 
		     {
			 bs_mode = BTNSETUP_DATAEXCHANGE_FINISH;
		     	 bs_timeout=BTNSETUP_EXCHANGE_TIMEOUT;
#ifdef OTS_SIMU
		     	 if(!ots_simu(4)) return INFO_PDU_LENGTH;
#endif
		     }	
		     else
		     {
#ifdef OTS_SIMU
		     	 if(!ots_simu(5)) return INFO_PDU_LENGTH;
#endif
                     }
		     time(&bs_time);

#ifdef OTS_LOG
	if (phdr->OpCode!=NET_CMD_ID_EZPROBE)
		ots_log(phdr->OpCode + gwquick->QuickFlag, 3);
#endif


#ifdef ENCRYPTION
		     Encrypt(sizeof(tw->u.WirelessStruct.TempKey),
				tw->u.WirelessStruct.TempKey,
				pdubuf_res, INFO_PDU_LENGTH,
				tmpbuf);
		     i=send(sockfd, tmpbuf, INFO_PDU_LENGTH, 0);
 
#else
		     i=send(sockfd, pdubuf_res, INFO_PDU_LENGTH, 0);
#endif

#ifdef OTS_LOG
	 	     if(i>=0) ots_log(i, 4);
		     else ots_log(errno, 4);
#endif

		     return INFO_PDU_LENGTH;
		}
		default:
			return 0;
	}
    }
    return 0;
}

int 
start_ots(void)
{ 
	char *ots_argv[] = {"ots", NULL};
	pid_t pid;

	_eval(ots_argv, NULL, 0, &pid);
	return 0;
}

void
stop_ots(void)
{
	killall_tk("ots");
}

int 
ots_main(int argc, char *argv[])
{
	FILE *fp;
    	usockaddr usa;
    	int listen_fd;
    	int conn_fd;
    	socklen_t sz = sizeof(usa);
	time_t now;
	int flag=0;
	int ret;

	/* write pid */
	if ((fp=fopen("/var/run/ots.pid", "w"))!=NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	if (nvram_invmatch("sharedkeystr", "")) 
	{
//		printf("dbg: OST:1\n");
		OTSStart(1);
	}
	else 
	{
#ifdef OTS_LOG
		ots_log(0, 0);
#endif
//		printf("dbg: OST:0\n");
		OTSStart(0);
	}

	/* Initialize listen socket */
    	if ((listen_fd = OTS_socket_init(&usa)) < 0) {
		fprintf(stderr, "can't bind to any address\n" );
		return 0;
    	} 	

    	/* Loop forever handling requests */
    	for (;;) 
    	{
    		ret=waitsock(listen_fd, bs_timeout, 0);

#ifdef FULL_EZSETUP
		if (ret==0) goto finish;
		else if(ret<0) continue;
#else
		if (ret<=0) continue;
#endif

		if ((conn_fd = accept(listen_fd, &usa.sa, &sz)) < 0) {
			perror("accept");
#ifdef OTS_LOG
			ots_log(bs_mode+0x10, 0);
#endif
			continue;
			//return errno;
		}

		if (!OTSPacketHandler(conn_fd))
		{
			syslog(LOG_NOTICE, "Error Packets: %x %x %x", bs_mode, bs_auth, bs_encrypt);
		}
	        
		if (bs_mode==BTNSETUP_DATAEXCHANGE) 
		{
		      if(!OTSExchange(bs_auth, bs_encrypt)) 
		      {	
			  //continue;
		      }	
		}
		else if(bs_mode==BTNSETUP_DATAEXCHANGE_FINISH)
		{
		      sleep(2);
		      flag = 1;
		      goto finish;		
		}
		time(&now);
		if (bs_mode>=BTNSETUP_FINISH) goto finish;
#ifdef FULL_EZSETUP // Added by Chen-I 200802012
		if ((now-bs_time)>bs_timeout) goto finish;
#endif
    	}
finish:
	if (bs_mode==BTNSETUP_FINISH)
	{
#ifdef OTS_LOG
		ots_log(bs_mode + 0x30, 0);
#endif
		if (dh) 
		{
			DH_free(dh);
			dh=NULL;
		}
#ifdef FULL_EZSETUP
    		shutdown(listen_fd, 2);
    		close(listen_fd);
		nvram_set("bs_mode", "1");
		bs_mode=BTNSETUP_NONE;
		sleep(2);
		stop_wan();
		stop_dnsmasq(); /* stop_dhcpd(); */
		//convert_asus_values(1);
		//nvram_commit_safe();
		start_dnsmasq(); /* start_dhcpd(); */
		start_wan();
#else
		
		nvram_set("wl0_wsc_config_state", "1");
		nvram_set("wl1_wsc_config_state", "1");
		//convert_asus_values(1);
		//nvram_commit_safe();
		OTSFinish(listen_fd, flag);
#endif
	}
	else 
	{
#ifdef OTS_LOG
		ots_log(bs_mode + 0x40, 1);
#endif
		if (dh) 
		{
			DH_free(dh);
			dh=NULL;
		}

		//convert_asus_values(1);
		//nvram_commit_safe();
		OTSFinish(listen_fd, flag);
	}
    	return 1;
}
#endif

