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
#include <asm/byteorder.h>

char pdubuf_res[INFO_PDU_LENGTH];

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

#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
int is_psta(int unit)
{
	if (unit < 0) return 0;

	if ((nvram_get_int("sw_mode") == SW_MODE_AP) &&
		(nvram_get_int("wlc_psta") == 1) &&
		(nvram_get_int("wlc_band") == unit))
		return 1;

	return 0;
}
#endif
#endif

char *processPacket(int sockfd, char *pdubuf, unsigned short cli_port)
{
    unsigned int realOPCode;
    IBOX_COMM_PKT_HDR	*phdr;
    IBOX_COMM_PKT_HDR_EX *phdr_ex;
    IBOX_COMM_PKT_RES_EX *phdr_res;
    PKT_GET_INFO *ginfo;

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
#if defined(RTCONFIG_WIRELESSREPEATER) || defined(RTCONFIG_PROXYSTA)
    char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
#endif
    unsigned short send_port = cli_port;

    phdr = (IBOX_COMM_PKT_HDR *)pdubuf;  
    phdr_res = (IBOX_COMM_PKT_RES_EX *)pdubuf_res;
    
    realOPCode = __constant_cpu_to_le16(phdr->OpCode); //translate endian

    if (phdr->ServiceID==NET_SERVICE_ID_IBOX_INFO &&
	phdr->PacketType==NET_PACKET_TYPE_CMD)
    {	    
	if (realOPCode!=NET_CMD_ID_GETINFO && realOPCode!=NET_CMD_ID_GETINFO_MANU&&
	    phdr_res->OpCode==phdr->OpCode &&
	    phdr_res->Info==phdr->Info)
	{	
		// if transaction id is equal to the transaction id of the last response message, just re-send message again;
		return pdubuf_res;
	}	
	
	phdr_res->ServiceID=NET_SERVICE_ID_IBOX_INFO;
	phdr_res->PacketType=NET_PACKET_TYPE_RES;
	phdr_res->OpCode=phdr->OpCode;

	if (realOPCode!=NET_CMD_ID_GETINFO && realOPCode!=NET_CMD_ID_GETINFO_MANU)
	{
		phdr_ex = (IBOX_COMM_PKT_HDR_EX *)pdubuf;	
		
		// Check Mac Address
		if (memcmp(phdr_ex->MacAddress, mac, 6)==0)
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

	switch(realOPCode)
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
#ifdef RTCONFIG_WIRELESSREPEATER
			if (nvram_get_int("sw_mode") == SW_MODE_REPEATER)
			{
				snprintf(prefix, sizeof(prefix), "wl%d.1_", nvram_get_int("wlc_band"));
				strncpy(ssid_g, nvram_safe_get(strcat_r(prefix, "ssid", tmp)), 32);
			}
			else
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
			if (is_psta(0) || is_psta(1))
			{
				snprintf(prefix, sizeof(prefix), "wl%d_", 1 - nvram_get_int("wlc_band"));
				strncpy(ssid_g, nvram_safe_get(strcat_r(prefix, "ssid", tmp)), 32);
			}
			else
#endif
#endif
		     strncpy(ssid_g, nvram_safe_get("wl0_ssid"), 32);
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
		     sendInfo(sockfd, pdubuf_res, send_port);
		     return pdubuf_res;

		case NET_CMD_ID_GETINFO:
			//_dprintf("NET CMD GETINFO\n");
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
				strncpy(ssid_g, nvram_safe_get(strcat_r(prefix, "ssid", tmp)), 32);
			}
			else
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
			if (is_psta(0) || is_psta(1))
			{
				snprintf(prefix, sizeof(prefix), "wl%d_", 1 - nvram_get_int("wlc_band"));
				strncpy(ssid_g, nvram_safe_get(strcat_r(prefix, "ssid", tmp)), 32);
			}
			else
#endif
#endif
		     strncpy(ssid_g, nvram_safe_get("wl0_ssid"), 32);
   		     strcpy(ginfo->SSID, ssid_g);
		     strcpy(ginfo->NetMask, get_lan_netmask());
		     strcpy(ginfo->ProductID, productid_g);	// disable for tmp
		     strcpy(ginfo->FirmwareVersion, firmver_g); // disable for tmp
		     memcpy(ginfo->MacAddress, mac, 6);
		     ginfo->OperationMode = (char)nvram_get_int("sw_mode");

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
			sendInfo(sockfd, pdubuf_res,send_port);
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

		     sendInfo(sockfd, pdubuf_res, send_port);
		     return pdubuf_res;

#if 0 // Vulenrable code
		case NET_CMD_ID_MANU_CMD:
		{
		     if (!nvram_match("ateCommand_flag", "1")) return NULL;
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

		     if (__le16_to_cpu(syscmd->len) >= MAXSYSCMD) syscmd->len = __cpu_to_le16(MAXSYSCMD);
		     syscmd->cmd[__le16_to_cpu(syscmd->len)]=0;
		     syscmd->len=__cpu_to_le16(strlen(syscmd->cmd));
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
			sendInfo(sockfd, pdubuf_res, send_port);
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
//			sendInfo(sockfd, pdubuf_res, send_port);
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
				syscmd_res->len = __cpu_to_le16(fread(syscmd_res->res, 1, sizeof(syscmd_res->res), fp));
				fclose(fp);
				unlink("/tmp/syscmd.out");
			}
			else syscmd_res->len=0;

			fprintf(stderr,"%d %s\n", __le16_to_cpu(syscmd_res->len), syscmd_res->res);
			/* repeat 3 times for MFG by Yen*/
			sendInfo(sockfd, pdubuf_res, send_port);
			sendInfo(sockfd, pdubuf_res, send_port);
			sendInfo(sockfd, pdubuf_res, send_port);
			if(strstr(syscmd->cmd, "Commit")) {
				int x=0;
				while(x<7) {
					sendInfo(sockfd, pdubuf_res, send_port);
					x++;
				}
			}
			/* end of MFG */
			fprintf(stderr,"\nSendInfo 3 times!\n");
		     }
	 	     return pdubuf_res;
		}
#endif // Disable vulnerable code
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
		     sendInfo(sockfd, pdubuf_res, send_port);
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
		     sendInfo(sockfd, pdubuf_res, send_port);
		     return pdubuf_res;
		}
#endif
		default:
			return NULL;	
	}
    }
    return NULL;
}
