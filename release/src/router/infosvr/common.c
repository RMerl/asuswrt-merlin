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

char *processPacket(int sockfd, char *pdubuf)
{
    return NULL;
}
