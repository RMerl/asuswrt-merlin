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

#include <rc.h>
#ifdef RTCONFIG_QCA
#include <stdio.h>
#include <fcntl.h>		//      for restore175C() from Ralink src
#include <qca.h>
#include <asm/byteorder.h>
#include <bcmnvram.h>
//#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if_arp.h>
#include <shutils.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
//#include <linux/if.h>
#include <iwlib.h>
//#include <wps.h>
//#include <stapriv.h>
#include <shared.h>
#include "flash_mtd.h"
#include "ate.h"

#define MAX_FRW 64
#define MACSIZE 12

#define	DEFAULT_SSID_2G	"ASUS"
#define	DEFAULT_SSID_5G	"ASUS_5G"

#define RTKSWITCH_DEV  "/dev/rtkswitch"

#define LED_CONTROL(led, flag) ralink_gpio_write_bit(led, flag)

#ifdef RTCONFIG_WIRELESSREPEATER
char *wlc_nvname(char *keyword);
#endif

#if defined(RTCONFIG_QCA)|| defined(RTAC52U) || defined(RTAC51U)
#define VHT_SUPPORT		/* 11AC */
#endif

#if 0				//eric++
typedef struct _roaming_info {
	char mac[19];
	char rssi0[7];
	char rssi1[7];
	char rssi2[7];
	char dump;
} roam;

typedef struct _roam_sta {
	roam sta[128];
} roam_sta;

typedef struct {
	unsigned int link[5];
	unsigned int speed[5];
} phyState;
#endif //eric++

int g_wsc_configured = 0;
int g_isEnrollee[MAX_NR_WL_IF] = { 0, };

int getCountryRegion5G(const char *countryCode, int *warning);

char *get_wscd_pidfile(void)
{
	static char tmpstr[32] = "/var/run/wscd.pid.";

	sprintf(tmpstr, "/var/run/wscd.pid.%s",
		(!nvram_get_int("wps_band")) ? WIF_2G : WIF_5G);
	return tmpstr;
}

char *get_wscd_pidfile_band(int wps_band)
{
	static char tmpstr[32] = "/var/run/wscd.pid.";

	sprintf(tmpstr, "/var/run/wscd.pid.%s", (!wps_band) ? WIF_2G : WIF_5G);
	return tmpstr;
}

int get_wifname_num(char *name)
{
	if (strcmp(WIF_5G, name) == 0)
		return 1;
	else if (strcmp(WIF_2G, name) == 0)
		return 0;
	else
		return -1;
}

const char *get_wifname(int band)
{
	if (band)
		return WIF_5G;
	else
		return WIF_2G;
}

const char *get_wpsifname(void)
{
	int wps_band = nvram_get_int("wps_band");

	if (wps_band)
		return WIF_5G;
	else
		return WIF_2G;
}

#if 0
char *get_non_wpsifname()
{
	int wps_band = nvram_get_int("wps_band");

	if (wps_band)
		return WIF_2G;
	else
		return WIF_5G;
}
#endif

static unsigned char nibble_hex(char *c)
{
	int val;
	char tmpstr[3];

	tmpstr[2] = '\0';
	memcpy(tmpstr, c, 2);
	val = strtoul(tmpstr, NULL, 16);
	return val;
}

static int atoh(const char *a, unsigned char *e)
{
	char *c = (char *)a;
	int i = 0;

	memset(e, 0, MAX_FRW);
	for (i = 0; i < MAX_FRW; ++i, c += 3) {
		if (!isxdigit(*c) || !isxdigit(*(c + 1)) || isxdigit(*(c + 2)))	// should be "AA:BB:CC:DD:..."
			break;
		e[i] = (unsigned char)nibble_hex(c);
	}

	return i;
}

char *htoa(const unsigned char *e, char *a, int len)
{
	char *c = a;
	int i;

	for (i = 0; i < len; i++) {
		if (i)
			*c++ = ':';
		c += sprintf(c, "%02X", e[i] & 0xff);
	}
	return a;
}

int FREAD(unsigned int addr_sa, int len)
{
	unsigned char buffer[MAX_FRW];
	char buffer_h[128];
	memset(buffer, 0, sizeof(buffer));
	memset(buffer_h, 0, sizeof(buffer_h));

	if (FRead(buffer, addr_sa, len) < 0)
		dbg("FREAD: Out of scope\n");
	else {
		if (len > MAX_FRW)
			len = MAX_FRW;
		htoa(buffer, buffer_h, len);
		puts(buffer_h);
	}
	return 0;
}

/*
 * 	write str_hex to offset da
 *	console input:	FWRITE 0x45000 00:11:22:33:44:55:66:77
 *	console output:	00:11:22:33:44:55:66:77
 *
 */
int FWRITE(const char *da, const char *str_hex)
{
	unsigned char ee[MAX_FRW];
	unsigned int addr_da;
	int len;

	addr_da = strtoul(da, NULL, 16);
	if (addr_da && (len = atoh(str_hex, ee))) {
		FWrite(ee, addr_da, len);
		FREAD(addr_da, len);
	}
	return 0;
}

//End of new ATE Command
//Ren.B
int check_macmode(const char *str)
{

	if ((!str) || (!strcmp(str, "")) || (!strcmp(str, "disabled"))) {
		return 0;
	}

	if (strcmp(str, "allow") == 0) {
		return 1;
	}

	if (strcmp(str, "deny") == 0) {
		return 2;
	}
	return 0;
}

//Ren.E

//Ren.B
void gen_macmode(int mac_filter[], int band, char *prefix)
{
	char temp[128];

	sprintf(temp,"%smacmode", prefix);
	mac_filter[0] = check_macmode(nvram_get(temp));
	_dprintf("mac_filter[0] = %d\n", mac_filter[0]);
}

//Ren.E

static inline void __choose_mrate(char *prefix, int *mcast_phy, int *mcast_mcs, int *rate)
{
	int phy = 3, mcs = 7;	/* HTMIX 65/150Mbps */
	*rate=150000;
	char tmp[128];

	if (ipv6_enabled() && nvram_get_int("ipv6_radvd")) {
		if (!strncmp(prefix, "wl0", 3)) {
			phy = 2;
			mcs = 2;	/* 2G: OFDM 12Mbps */
			*rate=12000;
		} else {
			phy = 3;
			mcs = 1;	/* 5G: HTMIX 13/30Mbps */
			*rate=30000;
		}
	}

	if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "2") ||	/* legacy mode */
	    strstr(nvram_safe_get(strcat_r(prefix, "crypto", tmp)), "tkip")) {	/* tkip */
		/* In such case, choose OFDM instead of HTMIX */
		phy = 2;
		mcs = 4;	/* OFDM 24Mbps */
		*rate=24000;
	}

	*mcast_phy = phy;
	*mcast_mcs = mcs;
}

int bw40_channel_check(int band,char *ext)
{
   	int ch;
 	if(!band)
 	   	ch=nvram_get_int("wl0_channel");
	else
 	   	ch=nvram_get_int("wl1_channel");
	if(ch)
	{
	  if(!band) //2.4G	
	  {   
		if((ch==1) ||(ch==2) ||(ch==3)||(ch==4))
		{
			if(!strcmp(ext,"MINUS"))
		 	{
				dbG("stage 1: a  mismatch between %s mode and ch %d => fix mode\n",ext,ch);
				sprintf(ext,"PLUS");
			}	   
		  
		}	
		else if(ch>=8)
		{
			if(!strcmp(ext,"PLUS"))
		 	{
				dbG("stage 2: a  mismatch between %s mode and ch %d => fix mode\n",ext,ch);
				sprintf(ext,"MINUS");
			}	   
		}
		//ch5,6,7:both
	  }	
	  else //5G
	  {
		  if ((ch == 36) || (ch == 44) || (ch == 52) || (ch == 60) || (ch == 100)
		     || (ch == 108) ||(ch == 116) || (ch == 124) || (ch == 132) || (ch == 149) || (ch ==157))
		  {
			if(!strcmp(ext,"MINUS"))
		 	{
				dbG("stage 1: a  mismatch between %s mode and ch %d => fix mode\n",ext,ch);
				sprintf(ext,"PLUS");
			}	   
		  
		  }
		  else if ((ch == 40) || (ch == 48) || (ch == 56) || (ch == 64) || (ch == 104) || (ch == 112) ||
		         (ch == 120) || (ch == 128) || (ch == 136) || (ch == 153) ||(ch == 161))
	  	  {
		  
			if(!strcmp(ext,"PLUS"))
		 	{
				dbG("stage 2: a  mismatch between %s mode and ch %d => fix mode\n",ext,ch);
				sprintf(ext,"MINUS");
			}	   
		  
		  }
		 
	  }   

	}
	return 1; //pass
}   
   

#define MAX_NO_GUEST 3
int gen_ath_config(int band, int is_iNIC,int subnet)
{
#ifdef RTCONFIG_WIRELESSREPEATER
	FILE *fp4 = NULL;
	char path4[50];
#endif   
	FILE *fp,*fp2,*fp3;
	char *str = NULL;
	char *str2 = NULL;
	int i;
	char list[2048];
	char wds_mac[4][30];
	int wds_keyidx;
	char wds_key[50];
	int flag_8021x = 0;
	int warning = 0;
//	int ChannelNumMax_2G = 11;
	char tmp[128], prefix[] = "wlXXXXXXX_" ,athfix[]="athXXXX_",tmpfix[]="wlXXXXX_";
	char temp[128], prefix_mssid[] = "wlXXXXXXXXXX_mssid_";
	char tmpstr[128];
	int j;
	char *nv, *nvp, *b;
	int mcast_phy = 0, mcast_mcs = 0;
	int mac_filter[MAX_NO_MSSID];
	char t_mode[30],t_bw[10],t_ext[10],mode_cmd[100];
	int bg_prot,ban;
	int sw_mode = nvram_get_int("sw_mode");
	int wlc_band = nvram_get_int("wlc_band");
	int wpapsk;
	int val,rate,caps;
	char wif[10];
	char path1[50],path2[50],path3[50];
	int rep_mode;
	char *uuid = nvram_safe_get("uuid");
#ifdef RTCONFIG_QCA_TW_AUTO_BAND4
	unsigned char CC[3];
#endif	
	rep_mode=0;
	bg_prot=0;
	ban=0;
	memset(mode_cmd,0,sizeof(mode_cmd));
	memset(t_mode,0,sizeof(t_mode));
	memset(t_bw,0,sizeof(t_bw));
	memset(t_ext,0,sizeof(t_ext));
	
	if(subnet==0) //for ath0 & ath1
		sprintf(wif,"ath%d",band);
	else  //ath00x & ath10x, for guest network
		sprintf(wif,"ath%d0%d",band,subnet);

	sprintf(path1,"/etc/Wireless/conf/hostapd_%s.conf",wif);
	sprintf(path2,"/etc/Wireless/sh/postwifi_%s.sh",wif);
	sprintf(path3,"/etc/Wireless/sh/prewifi_%s.sh",wif);
	system("mkdir -p /etc/Wireless/conf");
	system("mkdir -p /etc/Wireless/sh");
	
	_dprintf("gen qca config\n");
	if (!(fp = fopen(path1, "w+")))
		return 0;
	if (!(fp2 = fopen(path2, "w+")))
		return 0;
	if (!(fp3 = fopen(path3, "w+")))
		return 0;

#ifdef RTCONFIG_WIRELESSREPEATER
	if (sw_mode == SW_MODE_REPEATER && wlc_band == band && nvram_invmatch("wlc_ssid", "")&& subnet==0)
	{   
	   	rep_mode=1;
		sprintf(path4,"/etc/Wireless/conf/wpa_supplicant-sta%d.conf",band);
		if (!(fp4 = fopen(path4, "w+")))
			return 0;
	}	
#endif	   


	fprintf(fp, "#The word of \"Default\" must not be removed\n");
	fprintf(fp, "#Default\n");

	if(subnet==0)
		snprintf(prefix, sizeof(prefix), "wl%d_", band);
	else
	{
	   	j=0;
	   	for(i=1;i<=MAX_NO_GUEST;i++)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);
			if (nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
			{   
			   	j++;
				if(j==subnet)
					snprintf(prefix, sizeof(prefix), "wl%d.%d_", band,i);
			}	

		}   
	}

#ifdef RTCONFIG_WIRELESSREPEATER
	if(rep_mode==1)
	    snprintf(prefix, sizeof(prefix), "wl%d.1_", band);

	if (sw_mode == SW_MODE_REPEATER && wlc_band == band && nvram_invmatch("wlc_ssid", "") && subnet ==0)
	{
		int flag_wep = 0;
		int p;	
	    	snprintf(prefix_mssid, sizeof(prefix_mssid), "wl%d_", band);
	// convert wlc_xxx to wlX_ according to wlc_band == band
		nvram_set("ure_disable", "0");
		
		nvram_set("wl_ssid", nvram_safe_get("wlc_ssid"));
		nvram_set(strcat_r(prefix_mssid, "ssid", tmp), nvram_safe_get("wlc_ssid"));
		nvram_set(strcat_r(prefix_mssid, "auth_mode_x", tmp), nvram_safe_get("wlc_auth_mode"));

		nvram_set(strcat_r(prefix_mssid, "wep_x", tmp), nvram_safe_get("wlc_wep"));

		nvram_set(strcat_r(prefix_mssid, "key", tmp), nvram_safe_get("wlc_key"));
		for(p = 1; p <= 4; p++)
		{   
			char prekey[16];
			snprintf(prekey, sizeof(prekey), "key%d", p);	
			if(nvram_get_int("wlc_key")==p)
				nvram_set(strcat_r(prefix_mssid, prekey, tmp), nvram_safe_get("wlc_wep_key"));
		}

		nvram_set(strcat_r(prefix_mssid, "crypto", tmp), nvram_safe_get("wlc_crypto"));
		nvram_set(strcat_r(prefix_mssid, "wpa_psk", tmp), nvram_safe_get("wlc_wpa_psk"));
		nvram_set(strcat_r(prefix_mssid, "bw", tmp), nvram_safe_get("wlc_nbw_cap"));


		fprintf(fp4,
		      "ctrl_interface=/var/run/wpa_supplicant-sta%d\n"
		      "network={\n"
		      		"ssid=\"%s\"\n",
		      band,nvram_safe_get(strcat_r(prefix_mssid, "ssid", tmp)));

		str = nvram_safe_get("wlc_auth_mode");
		if (str && strlen(str))
		{
			if (!strcmp(str, "open") && nvram_match("wlc_wep", "0"))
			{
				fprintf(fp4,"       key_mgmt=NONE\n");    //open/none	
			}
			else if (!strcmp(str, "open"))
			{
				flag_wep = 1;
				fprintf(fp4,"       key_mgmt=NONE\n"); //open 
				fprintf(fp4,"       auth_alg=OPEN\n");
			}
			else if (!strcmp(str, "shared"))
			{
				flag_wep = 1;
				fprintf(fp4,"       key_mgmt=NONE\n"); //shared
				fprintf(fp4,"       auth_alg=SHARED\n");
			}      
			else if (!strcmp(str, "psk") || !strcmp(str, "psk2"))
			{
			   	fprintf(fp4,"       key_mgmt=WPA-PSK\n"); 
#if 0
				fprintf(fp4,"       proto=RSN\n"); 
#else
				if (!strcmp(str, "psk"))
				 	fprintf(fp4,"       proto=WPA\n");  //wpapsk
				else
				 	fprintf(fp4,"       proto=RSN\n");  //wpa2psk
#endif
				//EncrypType
				if (nvram_match("wlc_crypto", "tkip"))
				{
					fprintf(fp4, "       pairwise=TKIP\n"); 
					fprintf(fp4, "       group=TKIP\n");
				}   
				else if (nvram_match("wlc_crypto", "aes")) 
				{
					fprintf(fp4, "       pairwise=CCMP TKIP\n");
					fprintf(fp4, "       group=CCMP TKIP\n");
				}

				//key
				fprintf(fp4, "       psk=\"%s\"\n",nvram_safe_get("wlc_wpa_psk"));
			}
			else
				fprintf(fp4,"       key_mgmt=NONE\n");    //open/none	
		}
		else
			fprintf(fp4,"       key_mgmt=NONE\n");   //open/none

		//EncrypType
		if (flag_wep)
		{
			for(p = 1 ; p <= 4; p++)
			{
				if(nvram_get_int("wlc_key")==p)
				{   
				   	if((strlen(nvram_safe_get("wlc_wep_key"))==5)||(strlen(nvram_safe_get("wlc_wep_key"))==13))
					{

						fprintf(fp4, "       wep_tx_keyidx=%d\n",p-1);
					   	fprintf(fp4, "       wep_key%d=\"%s\"\n",p-1,nvram_safe_get("wlc_wep_key"));
					 
					}	
					else if((strlen(nvram_safe_get("wlc_wep_key"))==10)||(strlen(nvram_safe_get("wlc_wep_key"))==26))
					{   
						fprintf(fp4, "       wep_tx_keyidx=%d\n",p-1);
					   	fprintf(fp4, "       wep_key%d=%s\n",p-1,nvram_safe_get("wlc_wep_key"));
					}
					else
					{   
						fprintf(fp4, "       wep_tx_keyidx=%d\n",p-1);
					   	fprintf(fp4, "       wep_key%d=0\n",p-1);
					}	

				}
			}  

		}
		fprintf(fp4, "}\n");
	}
#endif	/* RTCONFIG_WIRELESSREPEATER */


	fprintf(fp, "interface=%s\n",wif);

	fprintf(fp, "ctrl_interface=/var/run/hostapd\n");
	fprintf(fp, "dump_file=/tmp/hostapd.dump\n");
	fprintf(fp, "driver=atheros\n");
	fprintf(fp, "bridge=br0\n");
	fprintf(fp, "logger_syslog=-1\n");
	fprintf(fp, "logger_syslog_level=2\n");
	fprintf(fp, "logger_stdout=-1\n");
	fprintf(fp, "logger_stdout_level=2\n");
	fprintf(fp, "ctrl_interface_group=0\n");
	fprintf(fp, "max_num_sta=255\n");
	fprintf(fp, "macaddr_acl=0\n");
	fprintf(fp, "ignore_broadcast_ssid=0\n");

	
	fprintf(fp, "eapol_key_index_workaround=0\n");
	flag_8021x=0;

	str = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
	if (str && (!strcmp(str, "radius") || !strcmp(str, "wpa") 
	    ||!strcmp(str, "wpa2")|| !strcmp(str, "wpawpa2")))
	{
	        flag_8021x=1;
		fprintf(fp, "ieee8021x=1\n");
		fprintf(fp, "eap_server=0\n");
	}
     	else
        {	   
		fprintf(fp, "ieee8021x=0\n");
		fprintf(fp, "eap_server=1\n");
	}	


/*
	//CountryRegion
	if(subnet==0)
	{   
		str = nvram_safe_get(strcat_r(prefix, "country_code", tmp));
		if (str && strlen(str)) {
			int region;
			region = getCountryRegion2G(str);
			//printf(fp, "country_code=%d\n", region);
			if(region==5) //default DB=US
				fprintf(fp, "country_code=US\n");
			else
				fprintf(fp, "country_code=%s\n", str);
			ChannelNumMax_2G = getChannelNumMax2G(region);
		} else {
			warning = 1;
			//fprintf(fp, "country_code=%d\n", 5);
			fprintf(fp, "country_code=US\n");
		}
	}
*/	
/*	
	//CountryRegion for A band
	str = nvram_safe_get(strcat_r(prefix, "country_code", tmp));
	if (str && strlen(str)) {
		int region;
		region = getCountryRegion5G(str, &warning);
		fprintf(fp, "CountryRegionABand=%d\n", region);
	} else {
		warning = 3;
		fprintf(fp, "CountryRegionABand=%d\n", 7);
	}

	//CountryCode
	str = nvram_safe_get(strcat_r(prefix, "country_code", tmp));
	if (str && strlen(str)) {
		fprintf(fp, "CountryCode=%s\n", str);
	} else {
		warning = 4;
		fprintf(fp, "CountryCode=DB\n");
	}
*/
	//SSID Num. [MSSID Only]

	fprintf(fp,"ssid=%s\n",nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
	
	if(band)
		fprintf(fp, "wmm_enabled=1\n");

	//if(subnet==0)
	{ 

		snprintf(tmpfix, sizeof(tmpfix), "wl%d_", band);
		//Network Mode
		str = nvram_safe_get(strcat_r(tmpfix, "nmode_x", tmp));
		//5G
		if (band) {
			if (str && strlen(str)) {
				if (atoi(str) == 0)	// Auto
				{
#if defined(VHT_SUPPORT)
					sprintf(t_mode,"iwpriv %s mode 11ACV" ,wif);
#else
					fprintf(fp, "hw_mode=a\n");
#endif
				} else if (atoi(str) == 1)	// N + AC
				{
#if defined(VHT_SUPPORT)
					sprintf(t_mode,"iwpriv %s mode 11ACV" ,wif);
#else
					fprintf(fp, "hw_mode=a\n");
#endif
				} else if (atoi(str) == 2)	// Legacy, A & N mode
				{   
					sprintf(t_mode,"iwpriv %s mode 11NA" ,wif);
					//ban=1;
				}	
				else	// A,N
				{
#if defined(VHT_SUPPORT)
					sprintf(t_mode,"iwpriv %s mode 11ACV" ,wif);
#else
					fprintf(fp, "hw_mode=a\n");
#endif
				}
			} else {
				warning = 6;
				fprintf(fp, "hw_mode=a\n");
			}
			
		} 
		else //2.4G 
		{
			if (str && strlen(str)) {
				if (atoi(str) == 0)	// B,G,N
				{
					sprintf(t_mode,"iwpriv %s mode 11NG",wif);
			     		bg_prot=1;	
				}	
				else if (atoi(str) == 2)	// B,G
				{   
					sprintf(t_mode,"iwpriv %s mode 11G",wif);
				     	bg_prot=1;	
					ban=1;
				}	
				else if (atoi(str) == 1)	// N
					sprintf(t_mode,"iwpriv %s mode 11NG",wif);
				else	// B,G,N
				{
					sprintf(t_mode,"iwpriv %s mode 11NG",wif);
				     	bg_prot=1;	
				}   
			} else {
				warning = 7;
				sprintf(t_mode,"iwpriv %s 11NG",wif);
				bg_prot=1;	
			}
		}


	} //subnet

	   
	//fprintf(fp2,"ifconfig %s up\n",wif);
	fprintf(fp2,"iwpriv %s hide_ssid %d\n",wif,nvram_get_int(strcat_r(prefix, "closed", tmp)));
	if (!nvram_get_int(strcat_r(prefix, "closed", tmp))) {
		fprintf(fp2, "iwconfig %s essid \"%s\"\n", wif, nvram_get(strcat_r(prefix, "ssid", tmp)));
		fprintf(fp2, "ifconfig %s up\n", wif);
	}
	
	if(subnet==0 && rep_mode==0 )
	{   
		//BGProtection
		str = nvram_safe_get(strcat_r(prefix, "gmode_protection", tmp));
		if (str && strlen(str)) {
			if (!strcmp(str, "auto") && (bg_prot)) //only for 2.4G
				fprintf(fp2, "iwpriv ath%d protmode 1\n",band);
			else
				fprintf(fp2, "iwpriv ath%d protmode 0\n",band);
		} else {
			warning = 13;
			fprintf(fp2, "iwpriv ath%d protmode 0\n",band);
		}


		//TxPreamble
		str = nvram_safe_get(strcat_r(prefix, "plcphdr", tmp));
		if (str && strcmp(str, "long") == 0)
			fprintf(fp2, "iwpriv %s shpreamble 0\n",wif);
		else if (str && strcmp(str, "short") == 0)
			fprintf(fp2, "iwpriv %s shpreamble 1\n",wif);
		else
			fprintf(fp2, "iwpriv %s shpreamble 0\n",wif);


		//RTSThreshold  Default=2347
		str = nvram_safe_get(strcat_r(prefix, "rts", tmp));
		if (str && strlen(str))
			fprintf(fp2, "iwconfig %s rts %d\n",wif,atoi(str));
		else {
			warning = 14;
			fprintf(fp2, "iwconfig %s rts 2347\n", wif);
		}

		
		//DTIM Period
		str = nvram_safe_get(strcat_r(prefix, "dtim", tmp));
		if (str && strlen(str))
			fprintf(fp2, "iwpriv %s dtim_period %d\n", wif,atoi(str));
		else {
			warning = 11;
			fprintf(fp2, "iwpriv %s dtim_period 1\n", wif);
		}

		//BeaconPeriod
		str = nvram_safe_get(strcat_r(prefix, "bcn", tmp));
		if (str && strlen(str)) {
			if (atoi(str) > 1000 || atoi(str) < 20) {
				nvram_set(strcat_r(prefix, "bcn", tmp), "100");
				fprintf(fp2, "iwpriv %s bintval 100\n",wif);
			} else
				fprintf(fp2, "iwpriv %s bintval %d\n",wif,atoi(str));
		} else {
			warning = 10;
			fprintf(fp2, "iwpriv %s bintval 100\n",wif);
		}

		//APSDCapable
		str = nvram_safe_get(strcat_r(prefix, "wme_apsd", tmp));
		if (str && strlen(str))
			fprintf(fp2, "iwpriv %s uapsd %d\n", wif,strcmp(str, "off") ? 1 : 0);
		else {
			warning = 18;
			fprintf(fp2, "iwpriv %s uapsd 1\n",wif);
		}

		//TxBurst
		str = nvram_safe_get(strcat_r(prefix, "frameburst", tmp));
		if (str && strlen(str))
			fprintf(fp2, "iwpriv wifi%d burst %d\n", band,strcmp(str, "off") ? 1 : 0);
		else {
			warning = 16;
			fprintf(fp2, "iwpriv wifi%d burst 1\n",band);
		}

	}	


	if(!band) //2.4G
		   fprintf(fp2,"iwpriv %s ap_bridge %d\n",wif,nvram_get_int("wl0_ap_isolate")?0:1);
	else
		   fprintf(fp2,"iwpriv %s ap_bridge %d\n",wif,nvram_get_int("wl1_ap_isolate")?0:1);
	



	//AuthMode
	memset(tmpstr, 0x0, sizeof(tmpstr));


	str = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));

	if (str && strlen(str)) {
		if (!strcmp(str, "open")) 	
			fprintf(fp, "auth_algs=1\n");
		else if (!strcmp(str, "shared")) 
			fprintf(fp, "auth_algs=2\n");
		else  //wpa/wpa2/wpa-auto-enterprise:wpa/wpa2/wpawpa2 
		      //wpa/wpa2/wpa-auto-personal:psk/psk2/pskpsk2 
			fprintf(fp, "auth_algs=1\n");
	 } 
	 else 
	 {
			warning = 24;
			fprintf(fp, "auth_algs=1\n");
	 }		
	
	//EncrypType
	memset(tmpstr, 0x0, sizeof(tmpstr));


	str = nvram_safe_get(strcat_r(prefix, "wpa_gtk_rekey", tmp));
	if (str && strlen(str)) 
		fprintf(fp, "wpa_group_rekey=%d\n",atoi(str));


	sprintf(prefix_mssid, "%s", prefix);

	if ((nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "open")
		&& nvram_match(strcat_r(prefix_mssid, "wep_x", temp),"0")))
			fprintf(fp, "#wpa_pairwise=\n");
	else if ((nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "open")
		&&
		 nvram_invmatch(strcat_r(prefix_mssid, "wep_x", temp),"0"))
		||
		nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "shared")
		||
		nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp),"radius"))
	   	{
		   	//wep
		   	str = nvram_safe_get(strcat_r(prefix_mssid, "key", temp));
			sprintf(tmpstr, "%skey%s", prefix_mssid, str);
			fprintf(fp2,"iwconfig %s key [%s]\n",wif,str); //key index

			if(strlen(nvram_safe_get(tmpstr))==10 || strlen(nvram_safe_get(tmpstr))==26)
				fprintf(fp2,"iwconfig %s key %s\n",wif,nvram_safe_get(tmpstr));
			else if(strlen(nvram_safe_get(tmpstr))==5 || strlen(nvram_safe_get(tmpstr))==13)
		   	   	fprintf(fp2,"iwconfig %s key \"s:%s\"\n",wif,nvram_safe_get(tmpstr));
			else
				fprintf(fp, "#wpa_pairwise=\n");
		}	
		else if (nvram_match(strcat_r(prefix_mssid, "crypto", temp), "tkip"))
		{
		   	 if(flag_8021x)
			 	fprintf(fp, "wpa_key_mgmt=WPA-EAP\n");
			 else
			 	fprintf(fp, "wpa_key_mgmt=WPA-PSK\n");   
			 fprintf(fp, "wpa_strict_rekey=1\n");   
			 fprintf(fp, "eapol_version=2\n");   
			 fprintf(fp, "wpa_pairwise=TKIP\n");
		}	 
		else if (nvram_match(strcat_r(prefix_mssid, "crypto", temp), "aes")) 
		{
		   	 if(flag_8021x)
			 	fprintf(fp, "wpa_key_mgmt=WPA-EAP\n");
			 else
			 	fprintf(fp, "wpa_key_mgmt=WPA-PSK\n");   
			 fprintf(fp, "wpa_strict_rekey=1\n");   
			 fprintf(fp, "eapol_version=2\n");   
	   	   	fprintf(fp, "wpa_pairwise=CCMP\n");
		}
		else if (nvram_match(strcat_r(prefix_mssid, "crypto", temp), "tkip+aes")) 
		{
		   	 if(flag_8021x)
			 	fprintf(fp, "wpa_key_mgmt=WPA-EAP\n");
			 else
			 	fprintf(fp, "wpa_key_mgmt=WPA-PSK\n");   
			 fprintf(fp, "wpa_strict_rekey=1\n");   
			 fprintf(fp, "eapol_version=2\n");   
	  	   	 fprintf(fp, "wpa_pairwise=TKIP CCMP\n");
		}	
		else 
		{
			warning = 25;
			fprintf(fp, "#wpa_pairwise=\n");
		}
	
	wpapsk=0;
	if ((nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "open")
	     && nvram_match(strcat_r(prefix_mssid, "wep_x", temp),"0")))
			fprintf(fp, "wpa=0\n");
	else if ((nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "open")
		  &&
		  nvram_invmatch(strcat_r(prefix_mssid, "wep_x", temp), "0"))
	          ||
		  nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp),"shared")
		  ||
		nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp),"radius"))
			fprintf(fp, "wpa=0\n");
	else if (nvram_match(strcat_r(prefix_mssid, "crypto", temp), "tkip")) 
	{	
			wpapsk=1;   
			fprintf(fp, "wpa=1\n");
	}		
	else if (nvram_match(strcat_r(prefix_mssid, "crypto", temp), "aes")) 
	{		
   			wpapsk=2;	   
	 		fprintf(fp, "wpa=2\n");
	}		
	else if (nvram_match(strcat_r(prefix_mssid, "crypto", temp), "tkip+aes")) 
	{	
	   		wpapsk=3;
	 	  	fprintf(fp, "wpa=3\n");
	}		
	else 
	{
			warning = 25;
			fprintf(fp, "wpa=0\n");
	}
	
	if(wpapsk!=0)
	{   
	   	if(!flag_8021x)
		{   
			sprintf(tmpstr, "wpa_passphrase=%s\n",nvram_safe_get(strcat_r(prefix_mssid, "wpa_psk",temp)));
			fprintf(fp, "%s", tmpstr);
		}	
	}	

	//if(subnet==0)
	{   
		if(!ban)
		{   
			//HT_BW
			str = nvram_safe_get(strcat_r(tmpfix, "bw", tmp));
			if (sw_mode == SW_MODE_REPEATER && wlc_band == band)
				sprintf(t_bw,"HT40");	
			else if (atoi(str) > 0)
			{
	   			if(strstr(t_mode,"11ACV") && (atoi(str)==3 || atoi(str)==1)) //80 BW or auto BW
					sprintf(t_bw,"HT80");
				else	
				{   
					sprintf(t_bw,"HT40");	
					//ext ch
					str = nvram_safe_get(strcat_r(tmpfix, "nctrlsb", tmp));
					if(!strcmp(str,"lower"))
						sprintf(t_ext,"PLUS");
					else
						sprintf(t_ext,"MINUS");

					bw40_channel_check(band,t_ext); 
				}	
			}	
			else {
				//warning = 34;
				sprintf(t_bw,"HT20");	
			}
		}	

		sprintf(mode_cmd,"%s%s%s",t_mode,t_bw,t_ext);
		fprintf(fp3,"%s\n",mode_cmd);

		if(band) //only 5G
		{   
		   	fprintf(fp3,"iwpriv %s shortgi 1\n",wif);
			if(subnet==0)
	  			fprintf(fp3,"iwpriv wifi1 enable_ol_stats %d\n",nvram_get_int("traffic_5g")==1?1:0);
		}	
		
	}

	val=0;
	if(!band)
	  	val=nvram_get_int("wl0_channel");
	else
		val=nvram_get_int("wl1_channel");

	
#ifdef RTCONFIG_QCA_TW_AUTO_BAND4
	if(band) //5G, flush block-channel list
		fprintf(fp3, "wifitool %s block_acs_channel 0\n",wif);
#endif			
	if(val)
	{
		fprintf(fp3, "iwpriv wifi%d dcs_enable 0\n",band);	//not to scan and change to other channels
		fprintf(fp3, "iwconfig %s channel %d\n",wif,val);
	}
	else if(subnet==0)
	{	
#ifdef RTCONFIG_QCA_TW_AUTO_BAND4
		if(band) //5G
		{
			memset(CC, 0, sizeof(CC));
	        	FRead(CC, OFFSET_COUNTRY_CODE, 2);
	                if(!strcmp(CC,"TW"))//old NCC
				fprintf(fp3, "wifitool %s block_acs_channel 52,56,60,64\n",wif);
			//else if(!strcmp(CC,"AA"))//new NCC
			//	fprintf(fp3, "wifitool %s block_acs_channel 36,40,44,48\n",wif);
		}	
#endif		
	   	fprintf(fp3, "iwconfig %s channel auto\n",wif);
	}
	if(!band && strstr(t_mode, "11N") != NULL) //only 2.4G && N mode is used
		fprintf(fp3,"iwpriv %s disablecoext %d\n",wif,nvram_get_int(strcat_r(tmpfix, "bw", tmp))==2?1:0);	// when N mode is used

	if(rep_mode)
	   goto next;

	//AccessPolicy0
	gen_macmode(mac_filter, band, prefix);	//Ren
	str = nvram_safe_get(strcat_r(prefix, "macmode", tmp));
	if(subnet==0)
		snprintf(athfix, sizeof(athfix), "ath%d", band);
	else
		snprintf(athfix, sizeof(athfix), "ath%d0%d", band,subnet);

	if (str && strlen(str)) {
			fprintf(fp2, "iwpriv %s maccmd 3\n",athfix); //clear acl list
			fprintf(fp2, "iwpriv %s maccmd %d\n",athfix,mac_filter[0]);
	} else {
		warning = 47;
		fprintf(fp2, "iwpriv %s maccmd 0\n",athfix); //disable acl
	}

	list[0] = 0;
	list[1] = 0;
	snprintf(tmpfix, sizeof(tmpfix), "wl%d_", band);
	if (nvram_invmatch(strcat_r(prefix, "macmode", tmp), "disabled")) {
		nv = nvp =
		    strdup(nvram_safe_get(strcat_r(tmpfix, "maclist_x", tmp)));
		if (nv) {
			while ((b = strsep(&nvp, "<")) != NULL) {
				if (strlen(b) == 0)
					continue;
				fprintf(fp2,"iwpriv %s addmac %s\n",athfix,b);
			}
			free(nv);
		}
	}


	int WdsEnable=0;
        int WdsEncrypType=0;
	if(subnet==0)
	{   

		fprintf(fp2,"wlanconfig %s nawds mode 0\n",wif);
        	//WDS Enable
		if (sw_mode != SW_MODE_REPEATER
		    && !nvram_match(strcat_r(prefix, "mode_x", tmp), "0")) {
			//WdsEnable
			str = nvram_safe_get(strcat_r(prefix, "mode_x", tmp));
			if (str && strlen(str)) {
				if ((nvram_match
				     (strcat_r(prefix, "auth_mode_x", tmp), "open")
				     ||
				     (nvram_match
				      (strcat_r(prefix, "auth_mode_x", tmp), "psk2")
				      && nvram_match(strcat_r(prefix, "crypto", tmp),
						     "aes")))
				 ) {
					if (atoi(str) == 0)
						WdsEnable=0;
					else if (atoi(str) == 1)
						WdsEnable=2;
					else if (atoi(str) == 2) {
						if (nvram_match(strcat_r(prefix, "wdsapply_x", tmp), "0"))
						WdsEnable=4;
					else
					        WdsEnable=3;
					}
				 } else
					WdsEnable=0;
			} else {
				warning = 49;
				WdsEnable=0;
			}
			//WdsEncrypType
			if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open")
			    && nvram_match(strcat_r(prefix, "wep_x", tmp), "0"))
				     WdsEncrypType=0; //none
			else if (nvram_match
				 (strcat_r(prefix, "auth_mode_x", tmp), "open")
				 && nvram_invmatch(strcat_r(prefix, "wep_x", tmp), "0"))
				     WdsEncrypType=1; //wep
			else if (nvram_match
				 (strcat_r(prefix, "auth_mode_x", tmp), "psk2")
				 && nvram_match(strcat_r(prefix, "crypto", tmp), "aes"))
				     WdsEncrypType=2; //aes
			else
				     WdsEncrypType=0; //none

			i=0;
			
			wds_keyidx=0;
			memset(wds_key,0,sizeof(wds_key));
			memset(wds_mac,0,sizeof(wds_mac));
			if ((nvram_match(strcat_r(prefix, "mode_x", tmp), "1")
			     || (nvram_match(strcat_r(prefix, "mode_x", tmp), "2")
				 && nvram_match(strcat_r(prefix, "wdsapply_x", tmp),
						"1")))
		   	 &&
		  	  (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open")
		    	 ||
		   	  (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk2")
		      	&& nvram_match(strcat_r(prefix, "crypto", tmp), "aes")))
		   	 ) {
				nv = nvp =
				    strdup(nvram_safe_get
					   (strcat_r(prefix, "wdslist", tmp)));
				if (nv) {
					while ((b = strsep(&nvp, "<")) != NULL) {
						if (strlen(b) == 0)
							continue;
						sprintf(wds_mac[i],"%s",b);
						i++;
					}
					free(nv);
				}
			}

			if (nvram_match
	    		        (strcat_r(prefix, "auth_mode_x", tmp), "open")
				&& nvram_invmatch(strcat_r(prefix, "wep_x", tmp),"0")) {
			 	wds_keyidx=nvram_get_int(strcat_r(prefix, "key", tmp));		
//              sprintf(list, "wl%d_key%s", band, nvram_safe_get(strcat_r(prefix, "key", tmp)));
				str = strcat_r(prefix, "key", tmp);
				str2 = nvram_safe_get(str);
				sprintf(list, "%s%s", str, str2);
				sprintf(wds_key,"%s",nvram_safe_get(list));
			} else
			    if (nvram_match
				(strcat_r(prefix, "auth_mode_x", tmp), "psk2")
				&& nvram_match(strcat_r(prefix, "crypto", tmp),
					       "aes")) {
				wds_keyidx=nvram_get_int(strcat_r(prefix, "wpa_psk", tmp));
			    }
		

#if 0//debug
		dbg("wds_keyidx: %d\n",wds_keyidx);
		dbg("wds_key: %s\n",wds_key);
		dbg("wds_mac: %s | %s | %s |%s \n",wds_mac[0],wds_mac[1],wds_mac[2],wds_mac[3]);
#endif				

			//5G:0x97/0x77, 2.4G:0x07
			if(band)
			{   
			   	if(nvram_get_int("wl1_wds_vht")==1)
				{   
					if(strstr(mode_cmd,"VHT80")!=NULL)
				   		caps=0x97;
			   		else
					   	caps=0x77;
				}
				else	
				   	caps=0x3;
			}
			else
				caps=0x07;
	
			fprintf(fp2,"wlanconfig %s nawds override 1\n",wif);
			fprintf(fp2,"wlanconfig %s nawds defcaps %d\n",wif,caps);
			if(WdsEnable!=2 || nvram_match(strcat_r(prefix, "wdsapply_x", tmp),"1"))
				fprintf(fp2,"wlanconfig %s nawds mode %d\n",wif,(WdsEnable==2)?4:3);
			   
			for(i=0;i<4;i++)
				if(strlen(wds_mac[i]) && nvram_match(strcat_r(prefix, "wdsapply_x", tmp),"1"))
					fprintf(fp2,"wlanconfig %s nawds add-repeater %s 0\n",wif,wds_mac[i]);

			if(WdsEncrypType==0)
		      		dbg("WDS:open/none");
			else  if(WdsEncrypType==1)
			{   
		      		dbg("WDS:open/wep");
				fprintf(fp2,"iwconfig %s key [%d]\n",wif,wds_keyidx);
			
				if(strlen(wds_key)==10 || strlen(wds_key)==26)
					fprintf(fp2,"iwconfig %s key %s\n",wif,wds_key);
				else if(strlen(wds_key)==5 || strlen(wds_key)==13)
		  	   		fprintf(fp2,"iwconfig %s key \"s:%s\"\n",wif,wds_key);
			}	
		   	else
			      	dbg("WDS:unknown\n");
			fprintf(fp2,"iwpriv %s wds 1\n",wif);

		}
	}
	if(flag_8021x)
	{    
		//radius server
		if (!strcmp(nvram_safe_get(strcat_r(tmpfix, "radius_ipaddr", tmp)), ""))
			fprintf(fp, "auth_server_addr=169.254.1.1\n");
		else
			fprintf(fp, "auth_server_addr=%s\n",
				nvram_safe_get(strcat_r(tmpfix, "radius_ipaddr", tmp)));

		//radius port
		str = nvram_safe_get(strcat_r(tmpfix, "radius_port", tmp));
		if (str && strlen(str))
			fprintf(fp, "auth_server_port=%d\n", atoi(str));
		else {
			warning = 50;
			fprintf(fp, "auth_server_port=1812\n");
		}

		//radius key

		str = nvram_safe_get(strcat_r(tmpfix, "radius_key", tmp));
		if (str && strlen(str))
			fprintf(fp, "auth_server_shared_secret=%s\n",
				nvram_safe_get(strcat_r(tmpfix, "radius_key", tmp)));
		else
		   	fprintf(fp,"#auth_server_shared_secret=\n");
	}	


	//if(subnet==0) //for ath0 & ath1
	{
		//RadioOn
		str = nvram_safe_get(strcat_r(tmpfix, "radio", tmp));
		if (str && strlen(str))
			fprintf(fp2, "ifconfig wifi%d %s\n",band, atoi(str)?"up":"down");
		//igmp
		fprintf(fp2, "iwpriv %s mcastenhance %d\n",wif,
			nvram_get_int(strcat_r(tmpfix, "igs", tmp)) ? 2 : 0);
	
		i = nvram_get_int(strcat_r(tmpfix, "mrate_x", tmp));
next_mrate:
		switch (i++) {
			default:
			case 0:		/* Driver default setting: Disable, means automatic rate instead of fixed rate
				 * Please refer to #ifdef MCAST_RATE_SPECIFIC section in
				 * file linuxxxx/drivers/net/wireless/rtxxxx/common/mlme.c
				 */
				break;
			case 1:		/* Legacy CCK 1Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 1000\n",wif);
				mcast_phy = 1;
				break;
			case 2:		/* Legacy CCK 2Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 2000\n",wif);
				mcast_phy = 1;
				break;
			case 3:		/* Legacy CCK 5.5Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 5500\n",wif);
				mcast_phy = 1;
				break;
			case 4:		/* Legacy OFDM 6Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 6000\n",wif);
				break;
			case 5:		/* Legacy OFDM 9Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 9000\n",wif);
				break;
			case 6:		/* Legacy CCK 11Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 11000\n",wif);
				mcast_phy = 1;
				break;
			case 7:		/* Legacy OFDM 12Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 12000\n",wif);
				break;
			case 8:		/* Legacy OFDM 18Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 18000\n",wif);
				break;
			case 9:		/* Legacy OFDM 24Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 24000\n",wif);
				break;
			case 10:		/* Legacy OFDM 36Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 36000\n",wif);
				break;
			case 11:		/* Legacy OFDM 48Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 48000\n",wif);
				break;
			case 12:		/* Legacy OFDM 54Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 54000\n",wif);
				break;
			case 13:		/* HTMIX 130/300Mbps 2S */
				fprintf(fp2, "iwpriv %s mcast_rate 300000\n",wif);
				break;
			case 14:		/* HTMIX 6.5/15Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 15000\n",wif);
				break;
			case 15:		/* HTMIX 13/30Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 30000\n",wif);
				break;
			case 16:		/* HTMIX 19.5/45Mbps */
				fprintf(fp2, "iwpriv %s mcast_rate 45000\n",wif);
				break;
			case 17:		/* HTMIX 13/30Mbps 2S */
				fprintf(fp2, "iwpriv %s mcast_rate 30000\n",wif);
				break;
			case 18:		/* HTMIX 26/60Mbps 2S */
				fprintf(fp2, "iwpriv %s mcast_rate 60000\n",wif);
				break;
			case 19:		/* HTMIX 39/90Mbps 2S */
				fprintf(fp2, "iwpriv %s mcast_rate 90000\n",wif);
				break;
			case 20:
				/* Choose multicast rate base on mode, encryption type, and IPv6 is enabled or not. */
				__choose_mrate(tmpfix, &mcast_phy, &mcast_mcs, &rate);
				fprintf(fp2, "iwpriv %s mcast_rate %d\n",wif,rate);
				break;
			}
		/* No CCK for 5Ghz band */
		if (band && mcast_phy == 1)
			goto next_mrate;
	}

#ifdef RTCONFIG_WPS
	fprintf(fp, "# Wi-Fi Protected Setup (WPS)\n");

	if (!subnet && nvram_get_int("wps_enable")) {
		if (nvram_match("w_Setting", "0")) {
			fprintf(fp, "wps_state=1\n");
		} else {
			fprintf(fp, "wps_state=2\n");
			fprintf(fp, "ap_setup_locked=1\n");
		}
		if (uuid && strlen(uuid) == 36)
			fprintf(fp, "uuid=%s\n", uuid);
	} else {
		/* Turn off WPS on guest network. */
		fprintf(fp, "wps_state=0\n");
	}

	fprintf(fp, "wps_independent=1\n");
	fprintf(fp, "device_name=ASUS Router\n");
	fprintf(fp, "manufacturer=ASUSTek Computer Inc.\n");
	fprintf(fp, "model_name=%s\n", nvram_safe_get("productid"));
	fprintf(fp, "model_number=123\n");	/* FIXME */
	fprintf(fp, "serial_number=12345\n");	/* FIXME */
	fprintf(fp, "device_type=6-0050F204-1\n");
	fprintf(fp, "config_methods=push_button display virtual_display virtual_push_button physical_push_button label\n");
	fprintf(fp, "pbc_in_m1=1\n");
	fprintf(fp, "ap_pin=%s\n", nvram_safe_get("secret_code"));
	fprintf(fp, "upnp_iface=br0\n");
	fprintf(fp, "friendly_name=WPS Access Point\n");
	fprintf(fp, "manufacturer_url=http://www.asus.com\n");
	fprintf(fp, "model_description=ASUS Router\n");
	fprintf(fp, "model_url=http://www.asus.com\n");
	//fprintf(fp, "wps_rf_bands=ag\n"); /* according to spec */
	fprintf(fp, "wps_rf_bands=%c\n",band?'a':'g');
#endif
next:
	fclose(fp);
	fclose(fp2);
	fclose(fp3);
#ifdef RTCONFIG_WIRELESSREPEATER
	if (sw_mode == SW_MODE_REPEATER && wlc_band == band && nvram_invmatch("wlc_ssid", "")&& subnet==0)
		fclose(fp4);
#endif	
	sprintf(temp,"/etc/Wireless/sh/prewifi_%s.sh",wif);
	chmod(temp,0777);
	sprintf(temp,"/etc/Wireless/sh/postwifi_%s.sh",wif);
	chmod(temp,0777);
	return 0;
}

/************************ CONSTANTS & MACROS ************************/

/*
 * Constants fof WE-9->15
 */
#define IW15_MAX_FREQUENCIES	16
#define IW15_MAX_BITRATES	8
#define IW15_MAX_TXPOWER	8
#define IW15_MAX_ENCODING_SIZES	8
#define IW15_MAX_SPY		8
#define IW15_MAX_AP		8

/****************************** TYPES ******************************/

/*
 *	Struct iw_range up to WE-15
 */
struct iw15_range {
	__u32 throughput;
	__u32 min_nwid;
	__u32 max_nwid;
	__u16 num_channels;
	__u8 num_frequency;
	struct iw_freq freq[IW15_MAX_FREQUENCIES];
	__s32 sensitivity;
	struct iw_quality max_qual;
	__u8 num_bitrates;
	__s32 bitrate[IW15_MAX_BITRATES];
	__s32 min_rts;
	__s32 max_rts;
	__s32 min_frag;
	__s32 max_frag;
	__s32 min_pmp;
	__s32 max_pmp;
	__s32 min_pmt;
	__s32 max_pmt;
	__u16 pmp_flags;
	__u16 pmt_flags;
	__u16 pm_capa;
	__u16 encoding_size[IW15_MAX_ENCODING_SIZES];
	__u8 num_encoding_sizes;
	__u8 max_encoding_tokens;
	__u16 txpower_capa;
	__u8 num_txpower;
	__s32 txpower[IW15_MAX_TXPOWER];
	__u8 we_version_compiled;
	__u8 we_version_source;
	__u16 retry_capa;
	__u16 retry_flags;
	__u16 r_time_flags;
	__s32 min_retry;
	__s32 max_retry;
	__s32 min_r_time;
	__s32 max_r_time;
	struct iw_quality avg_qual;
};

/*
 * Union for all the versions of iwrange.
 * Fortunately, I mostly only add fields at the end, and big-bang
 * reorganisations are few.
 */
union iw_range_raw {
	struct iw15_range range15;	/* WE 9->15 */
	struct iw_range range;	/* WE 16->current */
};

/*
 * Offsets in iw_range struct
 */
#define iwr15_off(f)	( ((char *) &(((struct iw15_range *) NULL)->f)) - \
			  (char *) NULL)
#define iwr_off(f)	( ((char *) &(((struct iw_range *) NULL)->f)) - \
			  (char *) NULL)

/* Disable runtime version warning in ralink_get_range_info() */
int iw_ignore_version_sp = 0;

/*
int getSiteSurvey(int band, char *ofile)
=> TBD. implement it if we want to support media bridge or repeater mode
*/

int __need_to_start_wps_band(char *prefix)
{
	char *p, tmp[128];

	if (!prefix || *prefix == '\0')
		return 0;

	p = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
	if ((!strcmp(p, "open")
	     && !nvram_match(strcat_r(prefix, "wep_x", tmp), "0"))
	    || !strcmp(p, "shared") || !strcmp(p, "psk") || !strcmp(p, "wpa")
	    || !strcmp(p, "wpa2") || !strcmp(p, "wpawpa2")
	    || !strcmp(p, "radius")
	    || nvram_match(strcat_r(prefix, "radio", tmp), "0")
	    || !((nvram_get_int("sw_mode") == SW_MODE_ROUTER)
		 || (nvram_get_int("sw_mode") == SW_MODE_AP)))
		return 0;

	return 1;
}

int need_to_start_wps_band(int wps_band)
{
	int ret = 1;
	char prefix[] = "wlXXXXXXXXXX_";

	switch (wps_band) {
	case 0:		/* fall through */
	case 1:
		snprintf(prefix, sizeof(prefix), "wl%d_", wps_band);
		ret = __need_to_start_wps_band(prefix);
		break;
	default:
		ret = 0;
	}

	return ret;
	return 1;	/* FIXME */
}

int wps_pin(int pincode)
{
	int i;
	char word[256], *next, ifnames[128];
	int wps_band = nvram_get_int("wps_band"), multiband = get_wps_multiband();

	i = 0;
	strcpy(ifnames, nvram_safe_get("wl_ifnames"));
	foreach(word, ifnames, next) {
		if (i >= MAX_NR_WL_IF)
			break;
		if (!multiband && wps_band != i) {
			++i;
			continue;
		}

		if (!need_to_start_wps_band(i)) {
			++i;
			continue;
		}
//              dbg("WPS: PIN\n");

		if (pincode == 0) {
			;
		} else {
			doSystem("hostapd_cli -i%s wps_pin any %08d", get_wifname(i), pincode);
		}

		++i;
	}

	return 0;
}

static int __wps_pbc(const int multiband)
{
	int i;
	char word[256], *next, ifnames[128];
	int wps_band = nvram_get_int("wps_band");

	i = 0;
	strcpy(ifnames, nvram_safe_get("wl_ifnames"));
	foreach(word, ifnames, next) {
		if (i >= MAX_NR_WL_IF)
			break;
		if (!multiband && wps_band != i) {
			++i;
			continue;
		}

		if (!need_to_start_wps_band(i)) {
			++i;
			continue;
		}
//              dbg("WPS: PBC\n");
		g_isEnrollee[i] = 1;
		eval("hostapd_cli", "-i", (char*)get_wifname(i), "wps_pbc");
		eval("hostapd_cli", "-i", (char*)get_wifname(i), "wps_ap_pin", "disable");

		++i;
	}

	return 0;
}

int wps_pbc(void)
{
	return __wps_pbc(get_wps_multiband());
}

int wps_pbc_both(void)
{
#if defined(RTCONFIG_WPSMULTIBAND)
	return __wps_pbc(1);
#endif
}

extern void wl_default_wps(int unit);

void __wps_oob(const int multiband)
{
	int i, wps_band = nvram_get_int("wps_band");
	char word[256], *next;
	char ifnames[128];

	if (nvram_match("lan_ipaddr", ""))
		return;

	i = 0;
	strcpy(ifnames, nvram_safe_get("wl_ifnames"));
	foreach(word, ifnames, next) {
		if (i >= MAX_NR_WL_IF)
			break;
		if (!multiband && wps_band != i) {
			++i;
			continue;
		}

		nvram_set("w_Setting", "0");
		wl_default_wps(i);

		qca_wif_up(word);
		g_isEnrollee[i] = 0;

		++i;
	}

#ifdef RTCONFIG_TCODE
	restore_defaults_wifi(0);
#endif
	nvram_commit();

	gen_qca_wifi_cfgs();
}

void wps_oob(void)
{
	__wps_oob(get_wps_multiband());
}

void wps_oob_both(void)
{
#if defined(RTCONFIG_WPSMULTIBAND)
	__wps_oob(1);
#else
	wps_oob();
#endif /* RTCONFIG_WPSMULTIBAND */
}

void start_wsc(void)
{
	int i;
	char *wps_sta_pin = nvram_safe_get("wps_sta_pin");
	char word[256], *next, ifnames[128];
	int wps_band = nvram_get_int("wps_band"), multiband = get_wps_multiband();

	if (nvram_match("lan_ipaddr", ""))
		return;

	i = 0;
	strcpy(ifnames, nvram_safe_get("wl_ifnames"));
	foreach(word, ifnames, next) {
		if (i >= MAX_NR_WL_IF)
			break;
		if (!multiband && wps_band != i) {
			++i;
			continue;
		}

		if (!need_to_start_wps_band(i)) {
			++i;
			continue;
		}

		dbg("%s: start wsc(%d)\n", __func__, i);
		doSystem("hostapd_cli -i%s wps_cancel", get_wifname(i));	// WPS disabled

		if (strlen(wps_sta_pin) && strcmp(wps_sta_pin, "00000000")
		    && (wl_wpsPincheck(wps_sta_pin) == 0)) {
			dbg("WPS: PIN\n");	// PIN method
			g_isEnrollee[i] = 0;
			doSystem("hostapd_cli -i%s wps_pin any %s", get_wifname(i), wps_sta_pin);
		} else {
			dbg("WPS: PBC\n");	// PBC method
			g_isEnrollee[i] = 1;
			eval("hostapd_cli", "-i", (char*)get_wifname(i), "wps_pbc");
			eval("hostapd_cli", "-i", (char*)get_wifname(i), "wps_ap_pin", "disable");
		}

		++i;
	}
}

static void __stop_wsc(int multiband)
{
	int i;
	char word[256], *next, ifnames[128];
	int wps_band = nvram_get_int("wps_band");

	i = 0;
	strcpy(ifnames, nvram_safe_get("wl_ifnames"));
	foreach(word, ifnames, next) {
		if (i >= MAX_NR_WL_IF)
			break;
		if (!multiband && wps_band != i) {
			++i;
			continue;
		}
		if (!need_to_start_wps_band(i)) {
			++i;
			continue;
		}

		doSystem("hostapd_cli -i%s wps_cancel", get_wifname(i));	// WPS disabled

		++i;
	}
}

void stop_wsc(void)
{
	__stop_wsc(get_wps_multiband());
}

void stop_wsc_both(void)
{
#if defined(RTCONFIG_WPSMULTIBAND)
	__stop_wsc(1);
#endif
}

#ifdef RTCONFIG_WPS_ENROLLEE
void start_wsc_enrollee(void)
{
	int i;
	char word[256], *next, ifnames[128];
	char conf[64];
	FILE *fp;

	i = 0;
	strcpy(ifnames, nvram_safe_get("wl_ifnames"));
	foreach(word, ifnames, next) {
		if (i >= MAX_NR_WL_IF)
			break;

		dbg("%s: start wsc enrollee(%d)\n", __func__, i);

		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER 
				|| nvram_get_int("sw_mode") == SW_MODE_AP) {
			sprintf(conf, "/etc/Wireless/conf/wpa_supplicant-sta%d.conf", i);
			if ((fp = fopen(conf, "w+")) < 0) {
				_dprintf("%s: Can't open %s\n", __func__, conf);
				continue;
			}
			fprintf(fp, "ctrl_interface=/var/run/wpa_supplicant\n");
			fprintf(fp, "update_config=1\n");
			fclose(fp);

			doSystem("wlanconfig sta%d create wlandev wifi%d wlanmode sta nosbeacon", i, i);
			doSystem("ifconfig sta%d up", i);
			doSystem("wpa_supplicant -B -P /var/run/wifi-sta%d.pid -D athr -i sta%d -b br0 -c /etc/Wireless/conf/wpa_supplicant-sta%d.conf", i, i, i);
		}

		doSystem("wpa_cli -i sta%d wps_pbc", i);
		i++;
	}
}

void stop_wsc_enrollee(void)
{
	int i;
	char word[256], *next, ifnames[128];
	char fpath[32];

	i = 0;
	strcpy(ifnames, nvram_safe_get("wl_ifnames"));
	foreach(word, ifnames, next) {
		if (i >= MAX_NR_WL_IF)
			break;

		doSystem("wpa_cli -i sta%d wps_cancel", i);

		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER 
				|| nvram_get_int("sw_mode") == SW_MODE_AP) {
			sprintf(fpath, "/var/run/wifi-sta%d.pid", i);
			kill_pidfile_tk(fpath);
			unlink(fpath);
			sprintf(fpath, "/etc/Wireless/conf/wpa_supplicant-sta%d.conf", i);
			unlink(fpath);

			doSystem("ifconfig sta%d down", i);
			doSystem("wlanconfig sta%d destroy", i);
		}

		i++;
	}
}

#ifdef RTCONFIG_WIFI_CLONE
void wifi_clone(int unit)
{
	char buf[512];
	FILE *fp;
	int len;
	char *pt1, *pt2;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";

	sprintf(buf, "/etc/Wireless/conf/wpa_supplicant-sta%d.conf", unit);
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	fp = fopen(buf, "r");
	if (fp) {
		memset(buf, 0, sizeof(buf));
		len = fread(buf, 1, sizeof(buf), fp);
		pclose(fp);
		if (len > 1) {
			buf[len-1] = '\0';
			//SSID
			pt1 = strstr(buf, "ssid=\"");
			if (pt1) {
				pt2 = pt1 + strlen("ssid=\"");
				pt1 = strstr(pt2, "\"");
				if (pt1) {
					*pt1 = '\0';
					chomp(pt2);
					nvram_set(strcat_r(prefix, "ssid", tmp), pt2);
				}
			}
					nvram_set(strcat_r(prefix, "crypto", tmp), "aes");
			//PSK
			pt2 = pt1 + 1;
			pt1 = strstr(pt2, "psk=\"");
			if (pt1) {	//WPA2-PSK
				pt2 = pt1 + strlen("psk=\"");
				pt1 = strstr(pt2, "\"");
				if (pt1) {
					*pt1 = '\0';
					chomp(pt2);
					nvram_set(strcat_r(prefix, "wpa_psk", tmp), pt2);
					nvram_set(strcat_r(prefix, "auth_mode_x", tmp), "psk2");
					nvram_set(strcat_r(prefix, "crypto", tmp), "aes");
				}
			}
			else {		//OPEN
				nvram_set(strcat_r(prefix, "auth_mode_x", tmp), "open");
				nvram_set(strcat_r(prefix, "wep_x", tmp), "0");
			}
			nvram_set("x_Setting", "1");
			nvram_commit();
		}
	}
}
#endif

char *getWscStatus_enrollee(int unit)
{
	char buf[512];
	FILE *fp;
	int len;
	char *pt1, *pt2;

	sprintf(buf, "wpa_cli -i sta%d status", unit);
	fp = popen(buf, "r");
	if (fp) {
		memset(buf, 0, sizeof(buf));
		len = fread(buf, 1, sizeof(buf), fp);
		pclose(fp);
		if (len > 1) {
			buf[len-1] = '\0';
			pt1 = strstr(buf, "wpa_state=");
			if (pt1) {
				pt2 = pt1 + strlen("wpa_state=");
				pt1 = strstr(pt2, "address=");
				if (pt1) {
					*pt1 = '\0';
					chomp(pt2);
				}
				return pt2;
			}
		}
	}

	return "";
}
#endif

char *getWscStatus(int unit)
{
	char buf[512];
	FILE *fp;
	int len;
	char *pt1,*pt2;

	sprintf(buf, "hostapd_cli -i%s wps_get_status", get_wifname(unit));
	fp = popen(buf, "r");
	if (fp) {
		memset(buf, 0, sizeof(buf));
		len = fread(buf, 1, sizeof(buf), fp);
		pclose(fp);
		if (len > 1) {
			buf[len-1] = '\0';
			pt1 = strstr(buf, "Last WPS result: ");
			if (pt1) {
				pt2 = pt1 + strlen("Last WPS result: ");
				pt1 = strstr(pt2, "Peer Address: ");
				if (pt1) {
					*pt1 = '\0';
					chomp(pt2);
				}
				return pt2;
			}
		}
	}

	return "";	/* FIXME */
}

int stainfo(int band)
{
#if 0 //eric++
	char data[2048];
	struct iwreq wrq;

	memset(data, 0x00, 2048);
	wrq.u.data.length = 2048;
	wrq.u.data.pointer = (caddr_t) data;
	wrq.u.data.flags = ASUS_SUBCMD_GSTAINFO;

	if (wl_ioctl(get_wifname(band), RTPRIV_IOCTL_ASUSCMD, &wrq) < 0) {
		dbg("errors in getting STAINFO result\n");
		return 0;
	}

	if (wrq.u.data.length > 0) {
		puts(wrq.u.data.pointer);
	}

#endif //eric++
	return 0;
}

int getstat(int band)
{
#if 0 //eric++
	char data[4096];
	struct iwreq wrq;

	memset(data, 0x00, 4096);
	wrq.u.data.length = 4096;
	wrq.u.data.pointer = (caddr_t) data;
	wrq.u.data.flags = ASUS_SUBCMD_GSTAT;

	if (wl_ioctl(get_wifname(band), RTPRIV_IOCTL_ASUSCMD, &wrq) < 0) {
		dbg("errors in getting STAT result\n");
		return 0;
	}

	if (wrq.u.data.length > 0) {
		puts(wrq.u.data.pointer);
	}

#endif //eric++
	return 0;
}

int getrssi(int band)
{
#if 0 //eric++
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
#define RTPRIV_IOCTL_STATISTICS		(SIOCIWFIRSTPRIV + 0x09)
	char data[1024];
	struct iwreq wrq;

	memset(data, 0x00, sizeof(data));
	wrq.u.data.length = sizeof(data);
	wrq.u.data.pointer = (caddr_t) data;
	wrq.u.data.flags = 0;

	if (wl_ioctl(get_wifname(band), RTPRIV_IOCTL_STATISTICS, &wrq) < 0) {
		dbg("errors in getting STATISTICS result\n");
		return 0;
	}

	if (wrq.u.data.length > 0) {
		char *start;
		char rssi1[8], rssi2[8], rssi3[8];
		start = wrq.u.data.pointer;
		getRSSI(start, &start, rssi1);
		getRSSI(start, &start, rssi2);
		getRSSI(start, &start, rssi3);
		printf("%s,%s,%s\n", rssi1, rssi2, rssi3);
	}

	return 0;
#else /* !RTCONFIG_WLMODULE_RT3352_INIC_MII */
	char data[32];
	struct iwreq wrq;

	memset(data, 0x00, 32);
	wrq.u.data.length = 32;
	wrq.u.data.pointer = (caddr_t) data;
	wrq.u.data.flags = ASUS_SUBCMD_GRSSI;

	if (wl_ioctl(get_wifname(band), RTPRIV_IOCTL_ASUSCMD, &wrq) < 0) {
		dbg("errors in getting RSSI result\n");
		return 0;
	}

	if (wrq.u.data.length > 0) {
		puts(wrq.u.data.pointer);
	}

	return 0;
#endif /* RTCONFIG_WLMODULE_RT3352_INIC_MII */
#endif //eric++
	return 0;	/* FIXME */
}

void wsc_user_commit(void)
{
}


void Get_fail_log(char *buf, int size, unsigned int offset)
{
	struct FAIL_LOG fail_log, *log = &fail_log;
	char *p = buf;
	int x, y;

	memset(buf, 0, size);
	FRead((char *)&fail_log, offset, sizeof(fail_log));
	if (log->num == 0 || log->num > FAIL_LOG_MAX) {
		return;
	}
	for (x = 0; x < (FAIL_LOG_MAX >> 3); x++) {
		for (y = 0; log->bits[x] != 0 && y < 7; y++) {
			if (log->bits[x] & (1 << y)) {
				p += snprintf(p, size - (p - buf), "%d,",
					      (x << 3) + y);
			}
		}
	}
}


void ate_commit_bootlog(char *err_code)
{
	unsigned char fail_buffer[OFFSET_SERIAL_NUMBER - OFFSET_FAIL_RET];

	nvram_set("Ate_power_on_off_enable", err_code);
	nvram_commit();

	memset(fail_buffer, 0, sizeof(fail_buffer));
	strncpy(fail_buffer, err_code,
		OFFSET_FAIL_BOOT_LOG - OFFSET_FAIL_RET - 1);
	Gen_fail_log(nvram_get("Ate_reboot_log"),
		     nvram_get_int("Ate_boot_check"),
		     (struct FAIL_LOG *)&fail_buffer[OFFSET_FAIL_BOOT_LOG -
						     OFFSET_FAIL_RET]);
	Gen_fail_log(nvram_get("Ate_dev_log"), nvram_get_int("Ate_boot_check"),
		     (struct FAIL_LOG *)&fail_buffer[OFFSET_FAIL_DEV_LOG -
						     OFFSET_FAIL_RET]);

	FWrite(fail_buffer, OFFSET_FAIL_RET, sizeof(fail_buffer));
}
#endif  //RTCONFIG_QCA

#ifdef RTCONFIG_USER_LOW_RSSI
typedef struct _WLANCONFIG_LIST {
         char addr[18];
         unsigned int aid;
         unsigned int chan;
         char txrate[6];
         char rxrate[6];
         unsigned int rssi;
         unsigned int idle;
         unsigned int txseq;
         unsigned int rcseq;
         char caps[12];
         char acaps[10];
         char erp[7];
         char state_maxrate[20];
         char wps[4];
         char rsn[4];
         char wme[4];
         char mode[31];
} WLANCONFIG_LIST;

void rssi_check_unit(int unit)
{
	#define STA_LOW_RSSI_PATH "/tmp/low_rssi"
   	int rssi_th;
	FILE *fp;
	char line_buf[300],cmd[300],tmp[128]; // max 14x
	char prefix[] = "wlXXXXXXXXXX_";
	WLANCONFIG_LIST *result;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	if (!(rssi_th= nvram_get_int(strcat_r(prefix, "user_rssi", tmp))))
		return;

	result=malloc(sizeof(WLANCONFIG_LIST));
	memset(result, 0, sizeof(WLANCONFIG_LIST));
	doSystem("wlanconfig ath%d list > %s", unit, STA_LOW_RSSI_PATH);
	fp = fopen(STA_LOW_RSSI_PATH, "r");
		if (fp) {
			//fseek(fp, 131, SEEK_SET);	// ignore header
			fgets(line_buf, sizeof(line_buf), fp); // ignore header
			while ( fgets(line_buf, sizeof(line_buf), fp) ) {
				sscanf(line_buf, "%s%u%u%s%s%u%u%u%u%s%s%s%s%s%s%s%s", 
							result->addr, 
							&result->aid, 
							&result->chan, 
							result->txrate, 
							result->rxrate, 
							&result->rssi, 
							&result->idle, 
							&result->txseq, 
							&result->rcseq, 
							result->caps, 
							result->acaps, 
							result->erp, 
							result->state_maxrate, 
							result->wps, 
							result->rsn, 
							result->wme,
							result->mode);

#if 0
				dbg("[%s][%u][%u][%s][%s][%u][%u][%u][%u][%s][%s][%s][%s][%s][%s][%s]\n", 
					result->addr, 
					result->aid, 
					result->chan, 
					result->txrate, 
					result->rxrate, 
					result->rssi, 
					result->idle, 
					result->txseq, 
					result->rcseq, 
					result->caps, 
					result->acaps, 
					result->erp, 
					result->state_maxrate, 
					result->wps, 
					result->rsn, 
					result->wme);
#endif
				if(rssi_th>-result->rssi)
				{
				    	memset(cmd,0,sizeof(cmd));
					sprintf(cmd,"iwpriv ath%d kickmac %s",unit,result->addr);
					doSystem(cmd);
					dbg("=====>Roaming with ath%d:Disconnect Station: %s  RSSI: %d\n",unit,result->addr,-result->rssi);
				}   
			}
			free(result);
			fclose(fp);
			unlink(STA_LOW_RSSI_PATH);
		}
}
#endif

void platform_start_ate_mode(void)
{
	int model = get_model();

	switch (model) {
#if defined(RTAC55U) || defined(RTAC55UHP)
	case MODEL_RTAC55U:
	case MODEL_RTAC55UHP:
		gpio_dir(13, GPIO_DIR_OUT);	/* Configure 2G LED as GPIO */
#ifndef RTCONFIG_ATEUSB3_FORCE
		// this way is unstable
		if(nvram_get_int("usb_usb3") == 0) {
			eval("ejusb", "-1");
			modprobe_r(USBOHCI_MOD);
			modprobe_r(USB20_MOD);
#ifdef USB30_MOD
			modprobe_r(USB30_MOD);
#endif
			nvram_set("xhci_ports", "2-1");
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
			modprobe(USB20_MOD);
			modprobe(USBOHCI_MOD);
#ifdef USB30_MOD
			modprobe(USB30_MOD, "u3intf=1");
#endif
		}
#endif
		break;
#endif	/* RTAC55U | RTAC55UHP */

#ifdef RT4GAC55U
	case MODEL_RT4GAC55U:
		break;
#endif	/* RT4GAC55U */

	default:
		_dprintf("%s: model %d\n", __func__, model);
	}
}


#define target 9
char str[target][40]={"Address:","ESSID:","Frequency:","Quality=","Encryption key:","IE:","Authentication Suites","Pairwise Ciphers","phy_mode="};
int
getSiteSurvey(int band,char* ofile)
{
   	int apCount=0;
	char header[128];
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char cmd[300];
	FILE *fp,*ofp;
	char buf[target][200],set_flag[target];
	int i;
	char *pt1,*pt2;
	char a1[10],a2[10];
	char ssid_str[256];
	char ch[4],ssid[33],address[18],enc[9],auth[16],sig[9],wmode[8];
	int  lock;
	char ure_mac[18];
	int wl_authorized = 0;
//////
	int is_ready;
	char temp1[200];
	char prefix_header[]="Cell xx - Address:";
/////
	dbG("site survey...\n");
	lock = file_lock("sitesurvey");
	system("rm -f /tmp/apscan_wlist");
	snprintf(prefix, sizeof(prefix), "wl%d_", band);
	sprintf(cmd,"iwlist %s scanning >> /tmp/apscan_wlist",nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
	ifconfig(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), IFUP, NULL, NULL);
	system(cmd);
	file_unlock(lock);
	
	if((fp= fopen("/tmp/apscan_wlist", "r"))==NULL) 
	   return 0;
	
	memset(header, 0, sizeof(header));
	sprintf(header, "%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode");

	dbg("\n%s", header);

	if ((ofp = fopen(ofile, "a")) == NULL)
	{
	   fclose(fp);
	   return 0;
	}

	apCount=1;
	while(1)
	{
	   	is_ready=0;
		memset(set_flag,0,sizeof(set_flag));
		memset(buf,0,sizeof(buf));
		memset(temp1,0,sizeof(temp1));
		snprintf(prefix_header, sizeof(prefix_header), "Cell %02d - Address:",apCount);

  		if(feof(fp)) 
		   break;

		while(fgets(temp1,sizeof(temp1),fp))
		{
			if(strstr(temp1,prefix_header)!=NULL)
			{
				if(is_ready)
				{   
					fseek(fp,-sizeof(temp1), SEEK_CUR);   
					break;
				}
				else
			   	{	   
					is_ready=1;
					snprintf(prefix_header, sizeof(prefix_header),"Cell %02d - Address:",apCount+1);
				}	
			}
			if(is_ready)
	   		{		   
				for(i=0;i<target;i++)
				{
					if(strstr(temp1,str[i])!=NULL && set_flag[i]==0)
				  	{   
						set_flag[i]=1;
					     	memcpy(buf[i],temp1,sizeof(temp1));
						break;
					}		
				}
			}	

		}


		dbg("\napCount=%d\n",apCount);
		apCount++;

		//ch
	        pt1 = strstr(buf[2], "Channel ");	
		if(pt1)
		{

			pt2 = strstr(pt1,")");
		   	memset(ch,0,sizeof(ch));
			strncpy(ch,pt1+strlen("Channel "),pt2-pt1-strlen("Channel "));
		}   

		//ssid
	        pt1 = strstr(buf[1], "ESSID:");	
		if(pt1)
		{
		   	memset(ssid,0,sizeof(ssid));
			strncpy(ssid,pt1+strlen("ESSID:")+1,strlen(buf[1])-2-(pt1+strlen("ESSID:")+1-buf[1]));
		}   


		//bssid
	        pt1 = strstr(buf[0], "Address: ");	
		if(pt1)
		{
		   	memset(address,0,sizeof(address));
			strncpy(address,pt1+strlen("Address: "),strlen(buf[0])-(pt1+strlen("Address: ")-buf[0])-1);
		}   
	

		//enc
		pt1=strstr(buf[4],"Encryption key:");
		if(pt1)
		{   
			if(strstr(pt1+strlen("Encryption key:"),"on"))
			{
				pt2=strstr(buf[7],"Pairwise Ciphers");
				if(pt2)
	  			{
					if(strstr(pt2,"CCMP TKIP") || strstr(pt2,"TKIP CCMP"))
				   		sprintf(enc,"TKIP+AES");
					else if(strstr(pt2,"CCMP"))
					   	sprintf(enc,"AES");
					else
					   	sprintf(enc,"TKIP");
				}
				else
					sprintf(enc,"WEP");
			}   
			else
				sprintf(enc,"NONE");
		}


		//auth
		memset(auth,0,sizeof(auth));
		pt1=strstr(buf[5],"IE:");
		if(pt1 && strstr(buf[5],"Unknown")==NULL)
		{   			 
			if(strstr(pt1+strlen("IE:"),"WPA2")!=NULL)
		   		sprintf(auth,"WPA2-");
			else if(strstr(pt1+strlen("IE:"),"WPA")!=NULL) 
		   		sprintf(auth,"WPA-");
		
			pt2=strstr(buf[6],"Authentication Suites");
			if(pt2)
	  		{
				if(strstr(pt2+strlen("Authentication Suites"),"PSK")!=NULL)
			   		strcat(auth,"Personal");
				else //802.1x
				   	strcat(auth,"Enterprise");
			}
		}
		else
		   	sprintf(auth,"Open System");
		   		  
		//sig
	        pt1 = strstr(buf[3], "Quality=");	
		pt2 = strstr(pt1,"/");
		if(pt1 && pt2)
		{
			memset(sig,0,sizeof(sig));
			memset(a1,0,sizeof(a1));
			memset(a2,0,sizeof(a2));
			strncpy(a1,pt1+strlen("Quality="),pt2-pt1-strlen("Quality="));
			strncpy(a2,pt2+1,strstr(pt2," ")-(pt2+1));
			sprintf(sig,"%d",100*(atoi(a1)+6)/(atoi(a2)+6));

		}   

		//wmode
		memset(wmode,0,sizeof(wmode));
		pt1=strstr(buf[8],"phy_mode=");
		if(pt1)
		{   

			if((pt2=strstr(pt1+strlen("phy_mode="),"IEEE80211_MODE_11AC_VHT"))!=NULL)
		   	   	sprintf(wmode,"ac");
			else if((pt2=strstr(pt1+strlen("phy_mode="),"IEEE80211_MODE_11A"))!=NULL
			        || (pt2=strstr(pt1+strlen("phy_mode="),"IEEE80211_MODE_TURBO_A"))!=NULL)
				sprintf(wmode,"a");
			else if((pt2=strstr(pt1+strlen("phy_mode="),"IEEE80211_MODE_11B"))!=NULL)
				sprintf(wmode,"b");
			else if((pt2=strstr(pt1+strlen("phy_mode="),"IEEE80211_MODE_11G"))!=NULL
			        || (pt2=strstr(pt1+strlen("phy_mode="),"IEEE80211_MODE_TURBO_G"))!=NULL)
				sprintf(wmode,"bg");
			else if((pt2=strstr(pt1+strlen("phy_mode="),"IEEE80211_MODE_11NA"))!=NULL)
		   	   	sprintf(wmode,"an");
			else if(strstr(pt1+strlen("phy_mode="),"IEEE80211_MODE_11NG"))
		   	   	sprintf(wmode,"bgn");
		}
		else
		   	sprintf(wmode,"unknown");

#if 1
		dbg("%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n",ch,ssid,address,enc,auth,sig,wmode);
#endif	

//////
		if(atoi(ch)<0)
			fprintf(ofp, "\"ERR_BNAD\",");
		else if(atoi(ch)>0 && atoi(ch)<14)
			fprintf(ofp, "\"2G\",");
		else if(atoi(ch)>14 && atoi(ch)<166)
			fprintf(ofp, "\"5G\",");
		else
			fprintf(ofp, "\"ERR_BNAD\",");


		memset(ssid_str, 0, sizeof(ssid_str));
		char_to_ascii(ssid_str, trim_r(ssid));
		
		if(strlen(ssid)==0)
			fprintf(ofp, "\"\",");
		else
			fprintf(ofp, "\"%s\",", ssid_str);

		fprintf(ofp, "\"%d\",", atoi(ch));

		fprintf(ofp, "\"%s\",",auth);
		
		fprintf(ofp, "\"%s\",", enc); 

		fprintf(ofp, "\"%d\",", atoi(sig));

		fprintf(ofp, "\"%s\",", address);

		fprintf(ofp, "\"%s\",", wmode); 

#ifdef RTCONFIG_WIRELESSREPEATER		
		//memset(ure_mac, 0x0, 18);
		//sprintf(ure_mac, "%02X:%02X:%02X:%02X:%02X:%02X",xxxx);
		if (strcmp(nvram_safe_get(wlc_nvname("ssid")), ssid)){
			if (strcmp(ssid, ""))
				fprintf(ofp, "\"%s\"", "0");				// none
			else if (!strcmp(ure_mac, address)){
				// hidden AP (null SSID)
				if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")!=NULL){
					if (wl_authorized){
						// in profile, connected
						fprintf(ofp, "\"%s\"", "4");
					}else{
						// in profile, connecting
						fprintf(ofp, "\"%s\"", "5");
					}
				}else{
					// in profile, connected
					fprintf(ofp, "\"%s\"", "4");
				}
			}else{
				// hidden AP (null SSID)
				fprintf(ofp, "\"%s\"", "0");				// none
			}
		}else if (!strcmp(nvram_safe_get(wlc_nvname("ssid")), ssid)){
			if (!strlen(ure_mac)){
				// in profile, disconnected
				fprintf(ofp, "\"%s\",", "1");
			}else if (!strcmp(ure_mac, address)){
				if (strstr(nvram_safe_get(wlc_nvname("akm")), "psk")!=NULL){
					if (wl_authorized){
						// in profile, connected
						fprintf(ofp, "\"%s\"", "2");
					}else{
						// in profile, connecting
						fprintf(ofp, "\"%s\"", "3");
					}
				}else{
					// in profile, connected
					fprintf(ofp, "\"%s\"", "2");
				}
			}else{
				fprintf(ofp, "\"%s\"", "0");				// impossible...
			}
		}else{
			// wl0_ssid is empty
			fprintf(ofp, "\"%s\"", "0");
		}
#else
		fprintf(ofp, "\"%s\"", "0");      
#endif		
		fprintf(ofp, "\n");
		
//////

	}

	fclose(fp);
	fclose(ofp);
	return 1;
}   


#ifdef RTCONFIG_WIRELESSREPEATER
char *wlc_nvname(char *keyword)
{
	return(wl_nvname(keyword, nvram_get_int("wlc_band"), -1));
}
#endif

char *getStaMAC(void)
{
	char buf[512];
	FILE *fp;
	int len,unit;
	char *pt1,*pt2;
	unit=nvram_get_int("wlc_band");

	sprintf(buf, "ifconfig sta%d", unit);

	fp = popen(buf, "r");
	if (fp) {
		memset(buf, 0, sizeof(buf));
		len = fread(buf, 1, sizeof(buf), fp);
		pclose(fp);
		if (len > 1) {
			buf[len-1] = '\0';
			pt1 = strstr(buf, "HWaddr ");
			if (pt1) 
			{
				pt2 = pt1 + strlen("HWaddr ");
				chomp(pt2);
				return pt2;
			}
		}
	}
	return NULL;
}


unsigned int getPapState(int unit)
{
	char buf[8192];
	FILE *fp;
	int len;
	char *pt1, *pt2;
	sprintf(buf, "iwconfig sta%d", unit);
	fp = popen(buf, "r");
	if (fp) {
		memset(buf, 0, sizeof(buf));
		len = fread(buf, 1, sizeof(buf), fp);
		pclose(fp);
		if (len > 1) {
			buf[len-1] = '\0';
			pt1 = strstr(buf, "Access Point:");
			if (pt1) {
				pt2 = pt1 + strlen("Access Point:");
				pt1 = strstr(pt2, "Not-Associated");
				if (pt1) 
				{
					sprintf(buf, "ifconfig | grep sta%d", unit);
				     	fp = popen(buf, "r");
					if(fp)
				   	{
						 memset(buf, 0, sizeof(buf));
						 len = fread(buf, 1, sizeof(buf), fp);
						 pclose(fp);
						 if(len>=1)
						    return 0;
						 else
						    return 3;
					}	
				     	else	
				   		return 0; //init
				}	
				else
				   	return 2; //connect and auth ?????
					
				}
			}
		}
	
	return 3; // stop
}

// TODO: wlcconnect_main
// TODO: wlcconnect_main
//	wireless ap monitor to connect to ap
//	when wlc_list, then connect to it according to priority
#define FIND_CHANNEL_INTERVAL	15
int wlcconnect_core(void)
{
   int unit,ret;
   unit=nvram_get_int("wlc_band");
   ret=getPapState(unit);
   if(ret!=2) //connected
   	dbG("check..wlconnect=%d \n",ret);
   return ret;
}


int wlcscan_core(char *ofile, char *wif)
{
	int ret,count;

	count=0;

	while((ret=getSiteSurvey(get_wifname_num(wif),ofile)==0)&& count++ < 2)
	{
		 dbg("[rc] set scan results command failed, retry %d\n", count);
		 sleep(1);
	}   

	return 0;
}	

