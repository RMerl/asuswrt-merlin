/*
 *
 *  State Transaction:
 *  0. STOP
 *  1. CONNECTING_PROFILE
 * 	a. scanning
 *  2. CONNECTING_ONE
 *  3. SCANNING
 *  4. CONNECTED
 *  5. BEING_AP
 * 
 *  CONNECTING_PROFILE --> CONNECTED --> SCANNING ---|
 *                     \-> BEING_AP -/               |                 
 *                                                   |
 *                     CONNECTED <- CONNECTING_ONE <--
 *  CONNECTED <- CONNECT_PROFILE <-/
 *  BEING_AP <-/
 *
 *  Environment
 *  1. Power on and connect to existed LAN
 *  2. Power on and connect to existed WLAN
 *  3. Power on and no LAN/WLAN is connected to 
 *
 */

/* 
 *
 *  State Transaction:
 *  0. STOP
 *  1. CONNECTING_PROFILE
 *  2. CONNECTING_ONE
 *  3. SCANNING
 *  4. CONNECTED
 *  5. BEING_AP
 * 
 *  CONNECTING_PROFILE --> CONNECTED --> SCANNING ---|
 *		     \-> BEING_AP -/	       |		 
 *						   |
 *		     CONNECTED <- CONNECTING_ONE <--
 *  CONNECTED <- CONNECT_PROFILE <-/
 *  BEING_AP <-/
 *
 *  Environment
 *  1. Power on and connect to existed LAN
 *  2. Power on and connect to existed WLAN
 *  3. Power on and no LAN/WLAN is connected to 
 *
 */
						
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/vfs.h>	/* get disk type */
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <bcmnvram.h>
#include <shutils.h>
//typedef unsigned char   bool;
#include <wlutils.h>
#include <unistd.h>		// eric++
#include <dirent.h>		// eric++
#include <syslog.h>
#include "iboxcom.h"
#include <shared.h>

#define SIOCETHTOOL 0x8946
#define MAX_PROFILE_NUMBER 32
#define MAX_SITE_NUMBER 24

enum 
{
	STA_STATE_STOP = 0,
	STA_STATE_CONNECTING_PROFILE,
	STA_STATE_CONNECTING_ONE,
	STA_STATE_SCANNING_PROFILE,
	STA_STATE_SCANNING,
	STA_STATE_CONNECTED,
	STA_STATE_BEING_AP
} STA_STATE;

#define STA_ISTIMEOUT(t) ((unsigned long)(now-sta_timer)>=t)
#define STA_STATE_TIMEOUT_CONNECTING 10 // 10 sec
#define STA_STATE_TIMEOUT_SCANNING 30 //30 sec
#define STA_STATE_TIMEOUT_SCANNING_PROFILE 1 //1 sec

int sta_state = STA_STATE_STOP;
int sta_profile = 0;
int sta_scan = 0;
time_t sta_timer = 0;
time_t now;
	
char pdubuf_res[INFO_PDU_LENGTH];

PKT_GET_INFO_STA stainfo_g;
int sites_g_count=0;
SITES sites_g[MAX_SITE_NUMBER];
int profiles_g_count=0;
PROFILES profiles_g[MAX_PROFILE_NUMBER];
int scan_g_type;
int scan_g_mode;

#ifdef WCLIENT
int is_lan_connected(char *lan_if)
{
	int fd, err, ret;
	struct ifreq ifr;
	struct ethtool_cmd ecmd;

	/* check if udhcpc get IP from DHCP server */
	_dprintf("check lan\n");	
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, lan_if);
	fd=socket(AF_INET, SOCK_DGRAM, 0);
	if (fd<0) return 0;
	ecmd.cmd = ETHTOOL_GSET;
	ifr.ifr_data = (caddr_t)&ecmd;
	err = ioctl(fd, SIOCETHTOOL, &ifr);
	if (err==0)
	{
		_dprintf("lan connect: %x\n", ecmd.speed);
		ret = ecmd.speed;
	}
	else ret = 0;

	close(fd);
	return ret;
}

void sta_info_init(void)
{
	int i;

     	stainfo_g.mode = STATION_MODE_INFRA;
	stainfo_g.chan = 1;
	strcpy(stainfo_g.ssid, "WirelessHD");
	stainfo_g.rate = 0;
	stainfo_g.wep = 0;
	stainfo_g.wepkeylen = 0;
	stainfo_g.wepkeyactive = 0;
	stainfo_g.sharedkeyauth = 0;
	stainfo_g.brgmacclone = 0;
	stainfo_g.preamble = 0;	
	stainfo_g.profileCount = 0;

	// initial sites info
	wl_read_profile();
	stainfo_g.profileCount = profiles_g_count;

	/* Check if client mode is disabled */
	if (nvram_invmatch("wlp_clientmode_x", "0") && profiles_g_count!=0)
	{	
		//int pref = atoi(nvram_safe_get("wlp_pref_x"));
		//if (pref < profiles_g_count)
		//	sta_start_connecting_one(&profiles_g[pref]);
		//else sta_start_connecting_profile(0);
		if (nvram_match("wlp_clientmode_x", "2")) 
			nvram_set("wl0_mode", "wet");
		else nvram_set("wl0_mode", "sta");
		system("wlconfig eth2");
		sta_start_scanning_profile(0);
	}
	else if (nvram_match("wlp_beap_x", "1")) sta_start_being_ap();
	else sta_stop();
}	

void sta_connect(PROFILES *profile)
{
	int i;

	_dprintf("ssid: %s\n", profile->ssid);
     	stainfo_g.mode = profile->mode;
	stainfo_g.chan = profile->chan;
	strncpy(stainfo_g.ssid, profile->ssid, 32);
	stainfo_g.rate = profile->rate;
	stainfo_g.wep = profile->wep;
	stainfo_g.wepkeylen = profile->wepkeylen;
	stainfo_g.wepkeyactive = profile->wepkeyactive;
	stainfo_g.sharedkeyauth = profile->sharedkeyauth;
	stainfo_g.brgmacclone = profile->brgmacclone;
	stainfo_g.preamble = profile->preamble;	

	wl_connect(profile);
}

void sta_stop()
{
	system("wl radio off");
	sta_state = STA_STATE_STOP;
}

void sta_start_scanning()
{
	
	if (sta_state==STA_STATE_BEING_AP)
	{			
		stop_dhcpd();
		if (nvram_match("wlp_clientmode_x", "2")) 
			nvram_set("wl0_mode", "wet");
		else nvram_set("wl0_mode", "sta");
		system("wlconfig eth2");
	}

	wl_scan();
	time(&sta_timer);
	sta_state = STA_STATE_SCANNING;
	
	sta_status_report(sta_state, 0);
}


void sta_start_connecting_profile(int is_next)
{	
	if (is_next) sta_profile++;
	else sta_profile = 0;

	if (profiles_g_count == 0 || sta_profile>=profiles_g_count)
	{
		if (nvram_match("wlp_beap_x", "1"))
		{
			sta_start_being_ap();
			return;			
		}
		sta_profile=0;
	}
	
	_dprintf("connect to profile : %d\n", sta_profile);
	sta_connect(&profiles_g[sta_profile]);
	time(&sta_timer);
	sta_state = STA_STATE_CONNECTING_PROFILE;

	sta_status_report(sta_state, 0);
}


void sta_start_scanning_profile(int is_next)
{
	int i, j, cand=-1, cand_rssi=-248, pref;
	char ssid[33], ssids[33];
	
	ssid[32] = ssids[32] = 0;

	if (is_next) sta_scan++;
	else sta_scan = 0;

	pref = atoi(nvram_safe_get("wlp_pref_x"));
		
	// scan each entry in profiles actively
	for (i=0;i<profiles_g_count;i++)
	{
		memcpy(ssid, profiles_g[i].ssid, 32);
		ssid[32] = 0;
		_dprintf("find: %s %d %d\n", ssid, profiles_g[i].wep, profiles_g[i].wepkeylen);
		doSystem("wl scan %s", ssid);
		sleep(1);
		wl_scan_results();

		for (j=0;j<sites_g_count;j++)
		{
			memcpy(ssids, sites_g[j].SSID, 32);

			_dprintf("scaned: %s %d\n", ssids, sites_g[j].RSSI);
			
			if (!strcmp(ssid, ssids) && 
				sites_g[j].RSSI>cand_rssi &&
				cand != pref)
			{
				cand = i;
				cand_rssi = sites_g[j].RSSI;
				break;
			}
		}
	}	

	if (cand==-1)
	{
		for (i=0;i<profiles_g_count;i++)
		{
			if (profiles_g[i].mode == STATION_MODE_ADHOC)
			{
				cand = i;
				break;
			}	
		}
	}

	if (cand!=-1)
	{	
		_dprintf("connect to one : %d\n", cand);	
		sta_start_connecting_one(&profiles_g[cand]);
	}
	else if (sta_scan<3) // totally, 3 chances to try
	{
		_dprintf("try : %d\n", sta_scan);	
		time(&sta_timer);
		sta_state = STA_STATE_SCANNING_PROFILE;
		sta_status_report(sta_state, 0);
	}
	else if (nvram_match("wlp_beap_x", "1")) sta_start_being_ap();
	else sta_start_scanning();
}



void sta_start_connecting_one(PROFILES *profile)
{
	sta_connect(profile);
	time(&sta_timer);
	sta_state = STA_STATE_CONNECTING_ONE;


	sta_status_report(sta_state, 0);
}

void sta_start_connected()
{
	// DHCP renew;
	FILE *fp;
	int pid;

	fp = fopen("/var/run/udhcpc.pid", "r");

	if (fp!=NULL)
	{
		fscanf(fp, "%d", &pid);
		_dprintf("release to %d\n", pid);
		kill(SIGUSR2, pid);
		sleep(1);
		_dprintf("renew to %d\n", pid);
		kill(SIGUSR1, pid);
		fclose(fp);
	}
	sta_state = STA_STATE_CONNECTED;

	sta_status_report(sta_state, 0);
}

void sta_start_being_ap()
{
	PROFILES profile;
	pid_t pid;
	char *nas_cmd[]={"nas", "/tmp/nas.conf", NULL};

	//printf("set as a AP");
	/* Become an AP with SSID = Shared Name */
	memset(&profile, 0, sizeof(profile));
	profile.mode = STATION_MODE_AP;
	strcpy(profile.ssid, nvram_safe_get("wl0_ssid")); 

	//printf("ssid: %s\n", profile->ssid);
     	stainfo_g.mode = profile.mode;
	stainfo_g.chan = profile.chan;
	strncpy(stainfo_g.ssid, profile.ssid, 32);
	stainfo_g.rate = profile.rate;
	stainfo_g.wep = profile.wep;
	stainfo_g.wepkeylen = profile.wepkeylen;
	stainfo_g.wepkeyactive = profile.wepkeyactive;
	stainfo_g.sharedkeyauth = profile.sharedkeyauth;
	stainfo_g.brgmacclone = profile.brgmacclone;
	stainfo_g.preamble = profile.preamble;	

	if (nvram_match("wl_wdsmode_x", "1")) nvram_set("wl0_mode", "wds");
	else nvram_set("wl0_mode", "ap");
	//system("wlconf eth2 up");
	system("wlconfig eth2");
	system("killall nas");
	_eval(nas_cmd, NULL, 0, &pid);

	stop_dhcpd();

	if (!is_dhcpd_exist()) 
	{
		_dprintf("start your own dhcp server\n");
		start_dhcpd();
	}
	sta_state = STA_STATE_BEING_AP;
	sta_status_report(sta_state, 0);
}

int sta_status()
{
	if (sta_state==STA_STATE_BEING_AP) return 0;
	else return (wl_status());
}

void sta_status_check()
{
	time(&now);
	
	if (sta_state==STA_STATE_STOP)
	{
		// do nothing
	}
	else if (sta_state==STA_STATE_CONNECTING_PROFILE)
	{
		if (wl_status()) // have connected to one AP
		{
			sta_start_connected();
		}
		else if (STA_ISTIMEOUT(STA_STATE_TIMEOUT_CONNECTING))
		{
			sta_start_connecting_profile(1);
		}
	}
	else if (sta_state==STA_STATE_CONNECTING_ONE)
	{
		if (wl_status())
		{
			sta_start_connected();
		}
		else if (STA_ISTIMEOUT(STA_STATE_TIMEOUT_CONNECTING))
		{
			if (nvram_match("wlp_beap_x", "1"))
			{
				sta_start_being_ap();
				return;			
			}
			else sta_start_scanning_profile(0);
		}
	}
	else if (sta_state==STA_STATE_SCANNING)
	{
		if (STA_ISTIMEOUT(STA_STATE_TIMEOUT_SCANNING))
		{
			sta_start_scanning_profile(0);
		}
	}
	else if (sta_state==STA_STATE_SCANNING_PROFILE)
	{
		if (STA_ISTIMEOUT(STA_STATE_TIMEOUT_SCANNING))
		{
			sta_start_scanning_profile(1); //Try again
		}
	}
	else if (sta_state==STA_STATE_CONNECTED)
	{
		if (!wl_status())
		{
			sta_start_scanning_profile(0);
		}
	}
	else if (sta_state==STA_STATE_BEING_AP)
	{
		// do nothing now
	}
}

void sta_status_report(int stastate, int refresh)
{

	#define MAXBUF 8192
	FILE *fp;	
	int		result, i;
	int		cmd;
	unsigned char	buf[MAXBUF];

	//printf("report\n");

	if (refresh)
	{
		if (sta_state==STA_STATE_BEING_AP)
			stastate=STA_STATE_BEING_AP;
		else return;
	}

	fp=fopen("/tmp/wlan11b.log", "w+");

	if (fp==NULL) 
	{
		_dprintf("can not open file\n");
		return;
	}

	if (stastate==STA_STATE_STOP)
	{
		fprintf(fp, "Wireless Function is disabled\n");
	}
	else if (stastate==STA_STATE_BEING_AP)
	{
		char *auth[] = {"Open System or Shared Key", 
				"Shared Key",
				"WPA-PSK",
				"WPA",
				"Radius"};
		char *wep[] = {"None", "WEP-64", "WEP-128"};
		char *wpa[] = {"TKIP", "AES"};
		int authidx, encidx;


		fprintf(fp, "Mode: AP\n");
		fprintf(fp, "SSID: %s\n", nvram_safe_get("wl0_ssid"));

		authidx = atoi(nvram_safe_get("wl_authmode_x"));
		encidx = atoi(nvram_safe_get("wl_weptype_x"));

		if (authidx>=5) authidx=0;
		if (encidx>=3) encidx=0;

		fprintf(fp, "Authentication: %s\n", auth[authidx]);

		if (authidx==1)
		{	
			if (authidx<0||encidx>1) encidx=0;
			fprintf(fp, "Encryption: %s\n", wep[encidx+1]);
		}	
		else if (authidx==2)
		{
			if (authidx<0||encidx>1) encidx=0;
			fprintf(fp, "Encryption: %s\n", wpa[encidx]);
		}
		else if (authidx==3) 
		{
			if (authidx<0||encidx>2) encidx=0;
			fprintf(fp, "Encryption: %s\n", wep[encidx]);
		}
		else
		{
			if (authidx<0||encidx>2) encidx=0;
			fprintf(fp, "Encryption: %s\n", wep[encidx]);
		}

		fprintf(fp, "Association List\n");
		fprintf(fp, "------------------------------------\n");

		cmd = WLC_GET_ASSOCLIST;
		*(unsigned int *)buf = 1000;		//query 1000 MAC address

		if ( (result = wl_ioctl("eth2", cmd, buf, MAXBUF)) != 0 ) {
			_dprintf("Fail: cmd=0x%02x (%d)\n", cmd, result);
		}
		else 
		{
			unsigned int	count = *(unsigned int *)buf;
			unsigned int 	j;

			j=0;

			for ( i=0; i< count*6; i++ ) 
			{
				if ((i%6)==0)
				{
					fprintf(fp, "STA%2x:", j++);
				}

				fprintf(fp, "%02x", i, buf[sizeof(unsigned int)+i]);
				if ( (i % 6)==5 )
					fprintf(fp, "\n");
				else
					fprintf(fp, ":");
			}
		}

	}
	else
	{
		if (stastate==STA_STATE_CONNECTED || 
			stastate==STA_STATE_CONNECTING_ONE ||
			stastate==STA_STATE_CONNECTING_PROFILE)
		{
			if (stainfo_g.mode==0)
			{	
				fprintf(fp, "Mode: Infra Structure(STA)\n");
			}
			else
			{
				fprintf(fp, "Mode: Adhoc(STA)\n");
			}

			if (stastate == STA_STATE_CONNECTED)
			 	fprintf(fp, "Status: Associated\n");
			else fprintf(fp, "Status: Connecting\n");
			
			fprintf(fp, "SSID  : %s\n", stainfo_g.ssid); 

			if (stainfo_g.sharedkeyauth)
			{
				fprintf(fp, "Authenication: Shared Key\n");
			}
			else
			{
				fprintf(fp, "Authenication: Open System\n");
			}

			if (stainfo_g.wep == STA_ENCRYPTION_ENABLE)
			{
				if (stainfo_g.wepkeylen==STA_ENCRYPTION_TYPE_WEP128)
				{	
					fprintf(fp, "Encryption: WEP 128 bits\n");
				}
				else
				{
					fprintf(fp, "Encryption: WEP 64 bits\n");
				}
			}
			else
			{	
				fprintf(fp, "Encryption: None\n");
			}
		}
		else if (sta_state==STA_STATE_SCANNING ||
			  sta_state==STA_STATE_SCANNING_PROFILE)
		{
			fprintf(fp, "Mode: Station\n");
			fprintf(fp, "Status: Scanning\n");
		}
	}
	fclose(fp);
}
#endif

#ifdef BTN_SETUP

enum BTNSETUP_STATE
{
	BTNSETUP_NONE=0,
	BTNSETUP_DETECT,
	BTNSETUP_START,
	BTNSETUP_DATAEXCHANGE,
	BTNSETUP_FINISH
};

int is_setup_mode(int mode)
{
	if (atoi(nvram_safe_get("bs_mode"))==mode) return 1;
	else return 0;
}

int bs_get_setting(PKT_SET_INFO_GW_QUICK *setting)
{
	FILE *fp=NULL;
	int ret=0;

	fp = fopen("/tmp/WSetting", "r");

	if (fp && (fread(setting, 1, sizeof(PKT_SET_INFO_GW_QUICK), fp)==sizeof(PKT_SET_INFO_GW_QUICK)))	
	{
		ret=1;
	}

	if (fp) fclose(fp);
	return ret;
}

int bs_put_setting(PKT_SET_INFO_GW_QUICK *setting)
{
	FILE *fp=NULL;
	int ret=0;

	fp = fopen("/tmp/CSetting", "w");

	if (fp && (fwrite(setting, 1, sizeof(PKT_SET_INFO_GW_QUICK), fp)==sizeof(PKT_SET_INFO_GW_QUICK)))	
	{
		ret=1;
	}

	if (fp) fclose(fp);
	return ret;
}
#endif

int
kill_pidfile_s(char *pidfile, int sig)	// copy from rc/common_ex.c
{
	FILE *fp = fopen(pidfile, "r");
	char buf[256];
	extern errno;

	if (fp && fgets(buf, sizeof(buf), fp)) {
		pid_t pid = strtoul(buf, NULL, 0);
		fclose(fp);
		return kill(pid, sig);
  	} else
		return errno;
}

extern char ssid_g[];
extern char netmask_g[];
extern char productid_g[];
extern char firmver_g[];
extern char mac[];

int
get_ftype(char *type)	/* get disk type */
{
	struct statfs fsbuf;
	long f_type;
	double free_size;
	char *mass_path = nvram_safe_get("usb_mnt_first_path");
	//if (!mass_path)
	//	mass_path = "/media/AiDisk_a1";

	if (statfs(mass_path, &fsbuf))
	{
		perror("infosvr: statfs fail");
		return -1;
	}

	f_type = fsbuf.f_type;
	free_size = (double)((double)((double)fsbuf.f_bfree * fsbuf.f_bsize)/(1024*1024));
	_dprintf("f_type is %x\n", f_type);	// tmp test
	sprintf(type, "%x", f_type);
	return free_size;
}

char *get_lan_netmask()
{
	int fd;
	struct ifreq ifr;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	char *lan_ifname = nvram_safe_get("lan_ifname");

	/* IPv4 netmask */
	ifr.ifr_addr.sa_family = AF_INET;

	strncpy(ifr.ifr_name, strlen(lan_ifname) ? lan_ifname : "br0", IFNAMSIZ-1);
	ioctl(fd, SIOCGIFNETMASK, &ifr);
	close(fd);

	return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}

char *processPacket(int sockfd, char *pdubuf)
{
    IBOX_COMM_PKT_HDR	*phdr;
    IBOX_COMM_PKT_HDR_EX *phdr_ex;
    IBOX_COMM_PKT_RES_EX *phdr_res;
    PKT_GET_INFO *ginfo;
//    PKT_GET_INFO_STA *stainfo;
//    PKT_GET_INFO_EX1 *cmd;					
//    PKT_GET_INFO_EX1 *res;
//    PKT_GET_INFO_SITES *res_sites;
//    PKT_GET_INFO_PROFILE *cmd_profiles, *res_profiles;

#ifdef WAVESERVER	// eric++
    int fail = 0;
    pid_t pid;
    DIR *dir;
    int fd, ret, bytes;
    unsigned char tmp_buf[15];	// /proc/XXXXXX
    WS_INFO_T *wsinfo;
#endif
//#ifdef WL700G
    STORAGE_INFO_T *st;
//#endif
//    int i;
    char ftype[8], prinfo[128];	/* get disk type */
    int free_space;
#ifdef RTCONFIG_WIRELESSREPEATER
    char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
#endif

    phdr = (IBOX_COMM_PKT_HDR *)pdubuf;  
    phdr_res = (IBOX_COMM_PKT_RES_EX *)pdubuf_res;
    
//    fprintf(stderr,"Get: %x %x %x\n", phdr->ServiceID, phdr->PacketType, phdr->OpCode);

    if (phdr->ServiceID==NET_SERVICE_ID_IBOX_INFO &&
	phdr->PacketType==NET_PACKET_TYPE_CMD)
    {	    
	if (phdr->OpCode!=NET_CMD_ID_GETINFO && phdr->OpCode!=NET_CMD_ID_GETINFO_MANU&&
	    phdr_res->OpCode==phdr->OpCode &&
	    phdr_res->Info==phdr->Info)
	{	
		// if transaction id is equal to the transaction id of the last response message, just re-send message again;
		return pdubuf_res;
	}	
	
	phdr_res->ServiceID=NET_SERVICE_ID_IBOX_INFO;
	phdr_res->PacketType=NET_PACKET_TYPE_RES;
	phdr_res->OpCode=phdr->OpCode;

	if (phdr->OpCode!=NET_CMD_ID_GETINFO && phdr->OpCode!=NET_CMD_ID_GETINFO_MANU)
	{
		phdr_ex = (IBOX_COMM_PKT_HDR_EX *)pdubuf;	
		
		// Check Mac Address
		if (memcpy(phdr_ex->MacAddress, mac, 6)==0)
		{
			_dprintf("Mac Error %2x%2x%2x%2x%2x%2x\n",
				(unsigned char)phdr_ex->MacAddress[0],
				(unsigned char)phdr_ex->MacAddress[1],
				(unsigned char)phdr_ex->MacAddress[2],
				(unsigned char)phdr_ex->MacAddress[3],
				(unsigned char)phdr_ex->MacAddress[4],
				(unsigned char)phdr_ex->MacAddress[5]
				);
			return NULL;
		}
		
		// Check Password
		//if (strcmp(phdr_ex->Password, "admin")!=0)
		//{
		//	phdr_res->OpCode = phdr->OpCode | NET_RES_ERR_PASSWORD;
		//	_dprintf("Password Error %s\n", phdr_ex->Password);	
		//	return NULL;
		//}
		phdr_res->Info = phdr_ex->Info;
		memcpy(phdr_res->MacAddress, phdr_ex->MacAddress, 6);
	}

	switch(phdr->OpCode)
	{
		case NET_CMD_ID_GETINFO_MANU:
//		case NET_CMD_ID_GETINFO:
		     _dprintf("NET CMD GETINFO_MANU\n");	// tmp test
		     ginfo=(PKT_GET_INFO *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES));
		     memset(ginfo, 0, sizeof(ginfo));
#if 0
#ifdef PRNINFO
		     readPrnID(ginfo->PrinterInfo);
#else
		     memset(ginfo->PrinterInfo, 0, sizeof(ginfo->PrinterInfo));
#endif
#else
			if (strlen(nvram_safe_get("u2ec_mfg")) && strlen(nvram_safe_get("u2ec_device")))
			{
				if (strstr(nvram_safe_get("u2ec_device"), nvram_safe_get("u2ec_mfg")))
					sprintf(ginfo->PrinterInfo, "%s", nvram_safe_get("u2ec_device"));
				else
					sprintf(ginfo->PrinterInfo, "%s %s", nvram_safe_get("u2ec_mfg"), nvram_safe_get("u2ec_device"));
			}
#endif
		     /* get disk type */
#ifdef RTCONFIG_WIRELESSREPEATER
			if (nvram_get_int("sw_mode") == SW_MODE_REPEATER)
			{
				snprintf(prefix, sizeof(prefix), "wl%d.1_", nvram_get_int("wlc_band"));
				strcpy(ssid_g, nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
			}
			else
#endif
			strcpy(ssid_g, nvram_safe_get("wl0_ssid"));
		     strcpy(productid_g, get_productid());
		     strcpy(ginfo->SSID, ssid_g);
		     strcpy(ginfo->NetMask, get_lan_netmask());
		     strcpy(ginfo->ProductID, productid_g);	// disable for tmp
		     strcpy(ginfo->FirmwareVersion, firmver_g);	// disable for tmp
		     memcpy(ginfo->MacAddress, mac, 6);
#ifdef WCLIENT
		     ginfo->OperationMode = OPERATION_MODE_WB;
		     ginfo->Regulation = 0xff;
#endif
	#ifdef WAVESERVER
	     st = (STORAGE_INFO_T *) (pdubuf_res + sizeof (IBOX_COMM_PKT_RES) + sizeof (PKT_GET_INFO) + sizeof(WS_INFO_T));
	#else
	     st = (STORAGE_INFO_T *) (pdubuf_res + sizeof (IBOX_COMM_PKT_RES) + sizeof (PKT_GET_INFO));
	#endif
	//printf("get storage status(1)\n");	// tmp test
		     getStorageStatus(st);
//#endif /* #ifdef WL700G */
 		     /* Disable  WSC functions for MFG test*/
		     nvram_set("asus_mfg", "1");
/*
		     nvram_set("wsc_config_state", "1");
		     system("killall wsccmd");
		     system("killall wsc");
		     system("killall upnp");
		     system("killall ntp");
	 	     system("killall ntpclient");
		     system("killall lld2d");
*/
		     sendInfo(sockfd, pdubuf_res);
		     return pdubuf_res;

		case NET_CMD_ID_GETINFO:
			_dprintf("NET CMD GETINFO\n");
		     ginfo=(PKT_GET_INFO *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES));
		     memset(ginfo, 0, sizeof(ginfo));
#if 0
#ifdef PRNINFO
    		     readPrnID(ginfo->PrinterInfo);
#else
		     memset(ginfo->PrinterInfo, 0, sizeof(ginfo->PrinterInfo));
#endif
#else
			if (strlen(nvram_safe_get("u2ec_mfg")) && strlen(nvram_safe_get("u2ec_device")))
			{
				if (strstr(nvram_safe_get("u2ec_device"), nvram_safe_get("u2ec_mfg")))
					sprintf(ginfo->PrinterInfo, "%s", nvram_safe_get("u2ec_device"));
				else
					sprintf(ginfo->PrinterInfo, "%s %s", nvram_safe_get("u2ec_mfg"), nvram_safe_get("u2ec_device"));
			}
#endif
#ifdef RTCONFIG_WIRELESSREPEATER
			if (nvram_get_int("sw_mode") == SW_MODE_REPEATER)
			{
				snprintf(prefix, sizeof(prefix), "wl%d.1_", nvram_get_int("wlc_band"));
				strcpy(ssid_g, nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
			}
			else
#endif
			strcpy(ssid_g, nvram_safe_get("wl0_ssid"));
		     strcpy(productid_g, get_productid());
   		     strcpy(ginfo->SSID, ssid_g);
		     strcpy(ginfo->NetMask, get_lan_netmask());
		     strcpy(ginfo->ProductID, productid_g);	// disable for tmp
		     strcpy(ginfo->FirmwareVersion, firmver_g); // disable for tmp
		     memcpy(ginfo->MacAddress, mac, 6);
#ifdef WCLIENT
		     ginfo->OperationMode = OPERATION_MODE_WB;
		     ginfo->Regulation = 0xff;
#endif

#ifdef WAVESERVER    // eric++
	     	     // search /tmp/waveserver and get information
	     	     wsinfo = (WS_INFO_T*) (pdubuf_res + sizeof (IBOX_COMM_PKT_RES) + sizeof (PKT_GET_INFO));

	     	     fd = open (WS_INFO_FILENAME, O_RDONLY);
	     	     if (fd != -1)	{
				bytes = sizeof (WS_INFO_T);
				while (bytes > 0)	{
			    	ret = read (fd, wsinfo, bytes); 
		    		if (ret > 0)			{ bytes -= ret;		} 
					else if (ret < 0)		{ fail++; break;	} 
					else if (ret == 0)		{ fail++; break;	}
		 		} /* while () */
			} else {
				fail++;
			}

			if (fail == 0 && bytes == 0)	{
				ret = read (fd, &pid, sizeof (pid_t));
				if (ret == sizeof (pid_t))	{
			    	sprintf (tmp_buf, "/proc/%d", pid);
			    	dir = opendir (tmp_buf);
			    	if (dir == NULL)	{	// file exist, but the process had been killed
		    		    fail++;
				    }
				    closedir (dir);
				} else {	// file not found or error occurred
					fail++;
				}
	   		}

			if (fail != 0)	{
		    		memset (wsinfo, 0, sizeof (WS_INFO_T));
			}
#endif /* #ifdef WAVESERVER */
	#ifdef WAVESERVER
	     		st = (STORAGE_INFO_T *) (pdubuf_res + sizeof (IBOX_COMM_PKT_RES) + sizeof (PKT_GET_INFO) + sizeof(WS_INFO_T));
	#else
	     		st = (STORAGE_INFO_T *) (pdubuf_res + sizeof (IBOX_COMM_PKT_RES) + sizeof (PKT_GET_INFO));
	#endif
		  	getStorageStatus(st);
			sendInfo(sockfd, pdubuf_res);
			return pdubuf_res;		     	

		case NET_CMD_ID_GETINFO_EX2:
		     _dprintf("\n we got case GETINFO_EX2\n");	// tmp test
		     ginfo=(PKT_GET_INFO *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES));
		     memset(ginfo, 0, sizeof(ginfo));
		     memset(ginfo->PrinterInfo, 0, sizeof(ginfo->PrinterInfo));

		     /* get disk type */
		     memset(ftype, 0, sizeof(ftype));
		     memset(prinfo, 0, sizeof(prinfo));
		     free_space = get_ftype(ftype);
		     _dprintf("get ftpye is %s, free = %d\n", ftype, free_space);
		     sprintf(prinfo, "%s:%d!$", ftype, free_space);
		     memcpy(ginfo->PrinterInfo, prinfo, strlen(prinfo));

		     sendInfo(sockfd, pdubuf_res);
		     return pdubuf_res;		     	
#ifdef WCLIENT	
		case NET_CMD_ID_GETINFO_EX:
		     cmd = (PKT_GET_INFO_EX1 *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));	
		     res = (PKT_GET_INFO_EX1 *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES_EX));
		     stainfo = (PKT_GET_INFO_STA *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES_EX)+sizeof(PKT_GET_INFO_EX1));
				
		     if (cmd->FieldID!=FIELD_GENERAL_CURRENT_STA) return NULL;
		     res->FieldCount=1;
		     res->FieldID=FIELD_GENERAL_CURRENT_STA;

		     memcpy(stainfo, &stainfo_g, sizeof(stainfo_g));
		     stainfo->connectionStatus = sta_status();
		     _dprintf("GetSTA: %s\n", stainfo->ssid);
			
		     sendInfo(sockfd, pdubuf_res);
		     return pdubuf_res;
		case NET_CMD_ID_SETINFO:				
		     cmd=(PKT_GET_INFO_EX1 *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));			
		     stainfo = (PKT_GET_INFO_STA *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX)+sizeof(PKT_GET_INFO_EX1));
		     _dprintf("SSID: %s\n", stainfo->ssid);
		     sendInfo(sockfd, pdubuf_res);
		     return pdubuf_res;
		case NET_CMD_ID_GETINFO_SITES:				
		     cmd=(PKT_GET_INFO_EX1 *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));	
		     res_sites = (PKT_GET_INFO_SITES *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES_EX));
		     sta_start_scanning();
		     //sta_count_reset();
		     sleep(2);	
		     wl_scan_results();

		     _dprintf("Get INFO %d\n", sites_g_count);

		     res_sites->Count = sites_g_count;
	
		     for (i=0;i<res_sites->Count&&i<MAX_SITE_NUMBER;i++)
		     {
			 memcpy(&res_sites->Sites[i%8], &sites_g[i], sizeof(SITES));				 
			 if (i%8==0&&i!=0)
			 {
			 	res_sites->Index = i/8;
		     		sendInfo(sockfd, pdubuf_res);
			 } 	
		     }
		     if (i%8!=0)
		     {
			 res_sites->Index = i/8;
		     	 sendInfo(sockfd, pdubuf_res);
			 _dprintf("Send:%d %d\n", res_sites->Index, res_sites->Count);
		     }	
		     return pdubuf_res;
		case NET_CMD_ID_GETINFO_PROF:		     
		     cmd_profiles = (PKT_GET_INFO_PROFILE *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));	
		     res_profiles = (PKT_GET_INFO_PROFILE *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES_EX));
		     res_profiles->StartIndex = cmd_profiles->StartIndex;
		     res_profiles->Count = cmd_profiles->Count;
		     for (i=0;i<cmd_profiles->Count;i++)
		     {
			 memcpy(&res_profiles->p.Profiles[i], &profiles_g[cmd_profiles->StartIndex+i], sizeof(PROFILES));
		     }
		     sendInfo(sockfd, pdubuf_res);
		     return pdubuf_res;
		case NET_CMD_ID_SETINFO_PROF:
		     cmd_profiles =(PKT_GET_INFO_PROFILE *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));	
		     res_profiles = (PKT_GET_INFO_SITES *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES_EX));

		     if (cmd_profiles->Count==0)
		     {
			 if (cmd_profiles->StartIndex == 0xff) // Save to file
			 {
			 }
			 else 
			 {
				 scan_g_type = cmd_profiles->p.ProfileControl.ButtonType;
				 scan_g_mode = cmd_profiles->p.ProfileControl.ButtonMode;
				 stainfo_g.profileCount = cmd_profiles->p.ProfileControl.ProfileCount;

				 sta_start_connecting_one(&profiles_g[cmd_profiles->StartIndex]);
				 //printf("set: %d %d\n", cmd_profiles->StartIndex, stainfo_g.profileCount);
				
				 profiles_g_count = stainfo_g.profileCount;
				 wl_write_profile();				 
			 }
		     }		
		     else
		     {	 	
		     	 for (i=0;i<cmd_profiles->Count;i++)
		     	 {
				memcpy(&profiles_g[cmd_profiles->StartIndex + i], &cmd_profiles->p.Profiles[i], sizeof(PROFILES));
		     	 }
		     }	 	
		     sendInfo(sockfd, pdubuf_res);
		     return pdubuf_res;
#endif
		case NET_CMD_ID_MANU_CMD:
		{
		     #define MAXSYSCMD 256
		     char cmdstr[MAXSYSCMD];
		     PKT_SYSCMD *syscmd;
		     PKT_SYSCMD_RES *syscmd_res;
		     FILE *fp;

printf("1. NET_CMD_ID_MANU_CMD:\n");
_dprintf("2. NET_CMD_ID_MANU_CMD:\n");
fprintf(stderr, "3. NET_CMD_ID_MANU_CMD:\n");

		     syscmd = (PKT_SYSCMD *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));
		     syscmd_res = (PKT_SYSCMD_RES *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES_EX));

		     if (syscmd->len>=MAXSYSCMD) syscmd->len=MAXSYSCMD;
		     syscmd->cmd[syscmd->len]=0;
		     syscmd->len=strlen(syscmd->cmd);
		     fprintf(stderr,"system cmd: %d %s\n", syscmd->len, syscmd->cmd);
#if 0
		     if (!strcmp(syscmd->cmd, "GetAliveUsbDeviceInfo"))
		     {
		     	// response format: "USB device name","current status of the device","IP of cccupied user"
			if (nvram_match("u2ec_device", ""))
				sprintf(syscmd_res->res, "\"%s\",\"%s\"", "", "");
			else if (nvram_invmatch("u2ec_busyip", ""))
//				sprintf(syscmd_res->res, "\"%s\",\"%s\",\"%s\"", nvram_safe_get("u2ec_device"), "Busy", nvram_safe_get("u2ec_busyip"));
				sprintf(syscmd_res->res, "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"", ssid_g, productid_g, nvram_safe_get("u2ec_device"), "Busy", nvram_safe_get("u2ec_busyip"));
			else
//				sprintf(syscmd_res->res, "\"%s\",\"%s\"", nvram_safe_get("u2ec_device"), "Idle");
				sprintf(syscmd_res->res, "\"%s\",\"%s\",\"%s\",\"%s\"", ssid_g, productid_g, nvram_safe_get("u2ec_device"), "Idle");
			syscmd_res->len = strlen(syscmd_res->res);
			_dprintf("%d %s\n", syscmd_res->len, syscmd_res->res);
//			kill_pidfile_s("/var/run/usdsvr_broadcast.pid", SIGUSR1);
			sendInfo(sockfd, pdubuf_res);
		     }
		     else if (!strncmp(syscmd->cmd, "ClientPostMsg ", 14))
		     {
			char userip[16];
		     	char usermsg[256];
		     	char *p;
		     	int flag_invalid = 0;

			/* format: ClientPostMsg"Occupied User IP","ClientMsg" */
			memset(userip, 0x0, 16);
			memset(usermsg, 0x0, 256);
			strncpy(userip, syscmd->cmd + 15, 16);	// skip "ClientPostMsg \""

			if (p = strchr(userip, '"'))
				*p = '\0';
			else			// invalid format
			{
				flag_invalid = 1;
				strcpy(userip, "255.255.255.255");
			}

			if (!flag_invalid)	// skip "ClientPostMsg\"Occupied User IP\",\""
			{
				strcpy(usermsg, syscmd->cmd + 15 + strlen(userip) + 3);
				if (strlen(usermsg) > 1)
					usermsg[strlen(usermsg) - 1] = '\0';
			}
			else
				strcpy(usermsg, "invalid message format!");

			if (!strcmp(userip, "255.255.255.255"))
			{
				nvram_set("u2ec_msg_broadcast", usermsg);
				kill_pidfile_s("/var/run/usdsvr_broadcast.pid", SIGUSR2);
			}
			else
			{
				nvram_set("u2ec_clntip_unicast", userip);
				nvram_set("u2ec_msg_unicast", usermsg);
				kill_pidfile_s("/var/run/usdsvr_unicast.pid", SIGTSTP);
			}

//			strcpy(syscmd_res->res, usermsg);
//		     	syscmd_res->len = strlen(syscmd_res->res);
//		     	_dprintf("client ip: %s\n", userip);
//			_dprintf("%d %s\n", syscmd_res->len, syscmd_res->res);
//			sendInfo(sockfd, pdubuf_res);
		     }
		     else
#endif
		     {
			sprintf(cmdstr, "%s > /tmp/syscmd.out", syscmd->cmd);
			system(cmdstr);

			fprintf(stderr,"rund: %s\n", cmdstr);
			fp = fopen("/tmp/syscmd.out", "r");

			if (fp!=NULL)
			{
				syscmd_res->len = fread(syscmd_res->res, 1, sizeof(syscmd_res->res), fp);
				fclose(fp);
			}
			else syscmd_res->len=0;

			fprintf(stderr,"%d %s\n", syscmd_res->len, syscmd_res->res);
			/* repeat 3 times for MFG by Yen*/
			sendInfo(sockfd, pdubuf_res);
			sendInfo(sockfd, pdubuf_res);
			sendInfo(sockfd, pdubuf_res);
			if(strstr(syscmd->cmd, "Commit")) {
				int x=0;
				while(x<7) {
					sendInfo(sockfd, pdubuf_res);
					x++;
				}
			}
			/* end of MFG */
			fprintf(stderr,"\nSendInfo 3 times!\n");
		     }
	 	     return pdubuf_res;
		}
#ifdef BTN_SETUP // This option can not co-exist with WCLIENT
		case NET_CMD_ID_SETKEY_EX:
		{
		     #define MAXSYSCMD 256
		     char cmdstr[MAXSYSCMD];
		     PKT_SET_INFO_GW_QUICK_KEY *pkey;
		     PKT_SET_INFO_GW_QUICK_KEY *pkey_res;

		     if (!is_setup_mode(BTNSETUP_START)) return NULL;
		     pkey=(PKT_SET_INFO_GW_QUICK_KEY *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));
		     pkey_res = (PKT_SET_INFO_GW_QUICK_KEY *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES_EX));

#ifdef ENCRYPTION
		     if (pkey->KeyLen==0) return NULL;

#else
		     pkey_res->KeyLen=0;
#endif
		     sprintf(cmdstr, "%2x%2x%2x%2x%2x%2x\n",
				(unsigned char)phdr_ex->MacAddress[0],
				(unsigned char)phdr_ex->MacAddress[1],
				(unsigned char)phdr_ex->MacAddress[2],
				(unsigned char)phdr_ex->MacAddress[3],
				(unsigned char)phdr_ex->MacAddress[4],
				(unsigned char)phdr_ex->MacAddress[5]
				);
		     nvram_set("bs_mac", cmdstr);
		     sprintf(cmdstr, "Set MAC %s", cmdstr);
		     syslog(LOG_NOTICE, cmdstr);
		     sendInfo(sockfd, pdubuf_res);
		     return pdubuf_res;
		}
		case NET_CMD_ID_QUICKGW_EX:
		{
		     #define MAXSYSCMD 64
		     char cmdstr[MAXSYSCMD];
		     PKT_SET_INFO_GW_QUICK *gwquick;
		     PKT_SET_INFO_GW_QUICK *gwquick_res;

		     if (!is_setup_mode(BTNSETUP_DATAEXCHANGE)) return NULL;
		     gwquick=(PKT_SET_INFO_GW_QUICK *)(pdubuf+sizeof(IBOX_COMM_PKT_HDR_EX));
		     gwquick_res = (PKT_SET_INFO_GW_QUICK *)(pdubuf_res+sizeof(IBOX_COMM_PKT_RES_EX));

#ifdef ENCRYPTION
		     if (gwquick->QuickFlag&QFCAP_ENCRYPTION)
		     {
			   // DECRYPTION first
		     }
		     else return NULL	
#endif

		     bs_put_setting(gwquick);

		     if (!(gwquick->QuickFlag&QFCAP_WIRELESS)) // get setting
		     	 bs_get_setting(gwquick_res);
		     else memcpy(gwquick_res, gwquick, sizeof(PKT_SET_INFO_GW_QUICK));

#ifdef ENCRYPTION
		     // ENCRYPTION first
		     gwquick_res->QuickFlag = (QFCAP_ENCRYPTION|QFCAP_WIRELESS);
#else
		     gwquick_res->QuickFlag = (QFCAP_WIRELESS);
#endif	
		     sendInfo(sockfd, pdubuf_res);
		     return pdubuf_res;
		}
#endif
		default:
			return NULL;	
	}
    }
    return NULL;
}
