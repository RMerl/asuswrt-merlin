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
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: common_ex.c,v 1.3 2007/03/29 06:02:23 shinjung Exp $
 */

#include "rc.h"

#include <stdlib.h>
#include <stdio.h>
#include <net/if_arp.h>
#include <time.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <stdarg.h>
#include <arpa/inet.h>	// oleg patch
#include <string.h>	// oleg patch
#include <bcmdevs.h>
#include <wlutils.h>
#include <dirent.h>
#include <sys/wait.h>
#include <shared.h>

#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif

#ifdef RTCONFIG_QCA
#include <qca.h>
#endif

#include <mtd.h>

void update_lan_status(int);

in_addr_t inet_addr_(const char *cp)
{
	struct in_addr a;

	if (!inet_aton(cp, &a))
		return INADDR_ANY;
	else
		return a.s_addr;
}

inline int inet_equal(char *addr1, char *mask1, char *addr2, char *mask2)
{
	return ((inet_network(addr1) & inet_network(mask1)) ==
		(inet_network(addr2) & inet_network(mask2)));
}

/* remove space in the end of string */
char *trim_r(char *str)
{
	int i;

	if (!str || !*str)
		return str;
	
	i = strlen(str) - 1;
	while ((i >= 0) && (str[i] == ' ' || str[i] == '\n' || str[i] == '\r'))
		str[i--] = 0;

	return str;
}

/* convert mac address format from XXXXXXXXXXXX to XX:XX:XX:XX:XX:XX */
char *conv_mac(char *mac, char *buf)
{
	int i, j;

	if (strlen(mac)==0) 
	{
		buf[0] = 0;
	}
	else
	{
		j=0;	
		for (i=0; i<12; i++)
		{		
			if (i!=0&&i%2==0) buf[j++] = ':';
			buf[j++] = mac[i];
		}
		buf[j] = 0;	// oleg patch
	}
	//buf[j] = 0;

	_dprintf("mac: %s\n", buf);

	return (buf);
}

// 2010.09 James. {
/* convert mac address format from XX:XX:XX:XX:XX:XX to XXXXXXXXXXXX */
char *conv_mac2(char *mac, char *buf)
{
	int i,j;

	if(strlen(mac) != 17)
		buf[0] = 0;
	else{
		for(i = 0, j = 0; i < 17; ++i){
			if(i%3 != 2){
				buf[j] = mac[i];
				++j;
			}

			buf[j] = 0;
		}
	}

	return(buf);
}


/* convert mac address format from XXXXXXXXXXXX to XX:XX:XX:XX:XX:XX */
char *mac_conv(char *mac_name, int idx, char *buf)
{
	char *mac, name[32];
	int i, j;

	if (idx!=-1)	
		sprintf(name, "%s%d", mac_name, idx);
	else sprintf(name, "%s", mac_name);

	mac = nvram_safe_get(name);

	if (strlen(mac)==0) 
	{
		buf[0] = 0;
	}
	else
	{
		j=0;	
		for (i=0; i<12; i++)
		{		
			if (i!=0&&i%2==0) buf[j++] = ':';
			buf[j++] = mac[i];
		}
		buf[j] = 0;	// oleg patch
	}
	//buf[j] = 0;

	_dprintf("mac: %s\n", buf);

	return (buf);
}

// 2010.09 James. {
/* convert mac address format from XX:XX:XX:XX:XX:XX to XXXXXXXXXXXX */
char *mac_conv2(char *mac_name, int idx, char *buf)
{
	char *mac, name[32];
	int i, j;

	if(idx != -1)	
		sprintf(name, "%s%d", mac_name, idx);
	else
		sprintf(name, "%s", mac_name);

	mac = nvram_safe_get(name);

	if(strlen(mac) == 0 || strlen(mac) != 17)
		buf[0] = 0;
	else{
		for(i = 0, j = 0; i < 17; ++i){
			if(i%3 != 2){
				buf[j] = mac[i];
				++j;
			}

			buf[j] = 0;
		}
	}

	return(buf);
}
// 2010.09 James. }

//#if 0
void wan_netmask_check(void)
{
	unsigned int ip, gw, nm, lip, lnm;

	if (nvram_match("wan0_proto", "static") ||
	    //nvram_match("wan0_proto", "pptp"))
	    nvram_match("wan0_proto", "pptp") || nvram_match("wan0_proto", "l2tp"))	// oleg patch
	{
		ip = inet_addr(nvram_safe_get("wan_ipaddr"));
		gw = inet_addr(nvram_safe_get("wan_gateway"));
		nm = inet_addr(nvram_safe_get("wan_netmask"));

		lip = inet_addr(nvram_safe_get("lan_ipaddr"));
		lnm = inet_addr(nvram_safe_get("lan_netmask"));

		_dprintf("ip : %x %x %x\n", ip, gw, nm);

		if (ip==0x0 && (nvram_match("wan0_proto", "pptp") || nvram_match("wan0_proto", "l2tp")))	// oleg patch
			return;

		if (ip==0x0 || (ip&lnm)==(lip&lnm))
		{
			nvram_set("wan_ipaddr", "1.1.1.1");
			nvram_set("wan_netmask", "255.0.0.0");	
			nvram_set("wan0_ipaddr", nvram_safe_get("wan_ipaddr"));
			nvram_set("wan0_netmask", nvram_safe_get("wan_netmask"));
		}

		// check netmask here
		if (gw==0 || gw==0xffffffff || (ip&nm)==(gw&nm))
		{
			nvram_set("wan0_netmask", nvram_safe_get("wan_netmask"));
		}
		else
		{		
			for (nm=0xffffffff;nm!=0;nm=(nm>>8))
			{
				if ((ip&nm)==(gw&nm)) break;
			}

			_dprintf("nm: %x\n", nm);

			if (nm==0xffffffff) nvram_set("wan0_netmask", "255.255.255.255");
			else if (nm==0xffffff) nvram_set("wan0_netmask", "255.255.255.0");
			else if (nm==0xffff) nvram_set("wan0_netmask", "255.255.0.0");
			else if (nm==0xff) nvram_set("wan0_netmask", "255.0.0.0");
			else nvram_set("wan0_netmask", "0.0.0.0");
		}

		nvram_set("wanx_ipaddr", nvram_safe_get("wan0_ipaddr"));	// oleg patch, he suggests to mark the following 3 lines
		nvram_set("wanx_netmask", nvram_safe_get("wan0_netmask"));
		nvram_set("wanx_gateway", nvram_safe_get("wan0_gateway"));
	}
}

void init_switch_mode()
{
//	ra_gpio_init();						// init for switch mode retrieval
//	sw_mode_check();					// save switch mode into nvram name sw_mode
//	nvram_set("sw_mode_ex", nvram_safe_get("sw_mode"));	// save working switch mode into nvram name sw_mode_ex

	if (!nvram_get("sw_mode"))
		nvram_set("sw_mode", "1");
	nvram_set("sw_mode_ex", nvram_safe_get("sw_mode"));

	if (nvram_match("sw_mode_ex", "1"))			// Gateway mode
	{
		nvram_set("wan_nat_x", "1");
		nvram_set("wan_route_x", "IP_Routed");
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
		nvram_set("wlc_psta", "0");
		nvram_set("wl0_bss_enabled", "1");
		nvram_set("wl1_bss_enabled", "1");
#endif
#endif
		nvram_set("ure_disable", "1");
	}
	else if (nvram_match("sw_mode_ex", "4"))		// Router mode
	{
		nvram_set("wan_nat_x", "0");
		nvram_set("wan_route_x", "IP_Routed");
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
		nvram_set("wlc_psta", "0");
		nvram_set("wl0_bss_enabled", "1");
		nvram_set("wl1_bss_enabled", "1");
#endif
#endif
		nvram_set("ure_disable", "1");
	}
#ifdef RTCONFIG_WIRELESSREPEATER
	else if (nvram_match("sw_mode_ex", "2"))		// Repeater mode
	{
		nvram_set("wan_nat_x", "0");
		nvram_set("wan_route_x", "IP_Bridged");
		
		nvram_set("wl0_vifs", "wl0.1");
		
#ifdef RTCONFIG_RGMII_BRCM5301X
		nvram_set("wl0.1_hwaddr", nvram_safe_get("lan_hwaddr"));
#else
		nvram_set("wl0.1_hwaddr", nvram_safe_get("et0macaddr"));
#endif
#ifdef RTCONFIG_BCMWL6
		if (nvram_match("wl0_phytype", "v"))
			nvram_set("wl0_bw_cap", "7");
		else
			nvram_set("wl0_bw_cap", "3");
#else
		nvram_set("wl0_nbw_cap", "1");
		nvram_set("wl0_nbw", "40");
#endif
		
		nvram_set("wl_mode", "wet");
		nvram_set("wl0_mode", "wet");
		nvram_set("wl_ure", "1");
		nvram_set("wl0_ure", "1");
		nvram_set("ure_disable", "0");
		
		nvram_set("wl0.1_bss_enabled", "1");
		
		nvram_set("wl0.1_auth_mode", "none");
		nvram_set("wl0.1_wme", nvram_safe_get("wl_wme"));
		nvram_set("wl0.1_wme_bss_disable", nvram_safe_get("wl_wme_bss_disable"));
		nvram_set("wl0.1_wmf_bss_enable", nvram_safe_get("wl_wmf_bss_enable"));
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
		nvram_set("wlc_psta", "0");
#endif
#endif
	}
#endif	/* RTCONFIG_WIRELESSREPEATER */
	else if (nvram_match("sw_mode_ex", "3"))		// AP mode
	{
		nvram_set("wan_nat_x", "0");
		nvram_set("wan_route_x", "IP_Bridged");
		nvram_set("ure_disable", "1");
	}
	else
	{
		nvram_set("sw_mode", "1");
		nvram_set("sw_mode_ex", "1");
		nvram_set("wan_nat_x", "1");
		nvram_set("wan_route_x", "IP_Routed");
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
		nvram_set("wlc_psta", "0");
		nvram_set("wl0_bss_enabled", "1");
		nvram_set("wl1_bss_enabled", "1");
#endif
#endif
		nvram_set("ure_disable", "1");
	}
}

/*
 * wanmessage
 *
 */
void wanmessage(char *fmt, ...)
{
	va_list args;
	char buf[512];

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	nvram_set("wan_reason_t", buf);
	va_end(args);
}

int pppstatus(void)
{
	FILE *fp;
	char sline[128], buf[128], *p;

	if ((fp = fopen("/tmp/wanstatus.log", "r")) && fgets(sline, sizeof(sline), fp))
	{
		fcntl(fileno(fp), F_SETFL, fcntl(fileno(fp), F_GETFL) | O_NONBLOCK);
		p = strstr(sline, ",");
		strcpy(buf, p+1);
	}
	else
	{
		strcpy(buf, "unknown reason");
	}

	if(fp) fclose(fp);

	if(strstr(buf, "No response from ISP.")) return WAN_STOPPED_REASON_PPP_NO_ACTIVITY;
	else if(strstr(buf, "Failed to authenticate ourselves to peer")) return WAN_STOPPED_REASON_PPP_AUTH_FAIL;
	else if(strstr(buf, "Terminating connection due to lack of activity")) return WAN_STOPPED_REASON_PPP_LACK_ACTIVITY;
	else return WAN_STOPPED_REASON_NONE;
}

void usage_exit(const char *cmd, const char *help)
{
	fprintf(stderr, "Usage: %s %s\n", cmd, help);
	exit(1);
}

#if 0 // replaced by #define in rc.h
int modprobe(const char *mod)
{
#if 1
	return eval("modprobe", "-s", (char *)mod);
#else
	int r = eval("modprobe", "-s", (char *)mod);
	cprintf("modprobe %s = %d\n", mod, r);
	return r;
#endif
}
#endif // 0

int modprobe_r(const char *mod)
{
#if 1
	return eval("modprobe", "-r", (char *)mod);
#else
	int r = eval("modprobe", "-r", (char *)mod);
	cprintf("modprobe -r %s = %d\n", mod, r);
	return r;
#endif
}

#ifndef ct_modprobe
#ifdef LINUX26
#define ct_modprobe(mod, args...) ({ \
		modprobe("nf_conntrack_"mod, ## args); \
		modprobe("nf_nat_"mod); \
})
#else
#define ct_modprobe(mod, args...) ({ \
		modprobe("ip_conntrack_"mod, ## args); \
		modprobe("ip_nat_"mod, ## args); \
})
#endif
#endif

#ifndef ct_modprobe_r
#ifdef LINUX26
#define ct_modprobe_r(mod) ({ \
	modprobe_r("nf_nat_"mod); \
	modprobe_r("nf_conntrack_"mod); \
})
#else
#define ct_modprobe_r(mod) ({ \
	modprobe_r("ip_nat_"mod); \
	modprobe_r("ip_conntrack_"mod); \
})
#endif
#endif

/* 
 * The various child job starting functions:
 * _eval()
 *	Start the child. If ppid param is NULL, wait until the child exits.
 *	Otherwise, store the child's pid in ppid and return immediately.
 * eval()
 *	Call _eval with a NULL ppid, to wait for the child to exit.
 * xstart()
 *	Call _eval with a garbage ppid (to not wait), then return.
 * runuserfile
 *	Execute each executable in a directory that has the specified extention.
 *	Call _eval with a ppid (to not wait), then check every second for the child's pid.
 *	After wtime seconds or when the child has exited, return.
 *	If any such filename has an '&' character in it, then do *not* wait at
 *	all for the child to exit, regardless of the wtime.
 */

int _xstart(const char *cmd, ...)
{
	va_list ap;
	char *argv[16];
	int argc;
	int pid;

	argv[0] = (char *)cmd;
	argc = 1;
	va_start(ap, cmd);
	while ((argv[argc++] = va_arg(ap, char *)) != NULL) {
		//
	}
	va_end(ap);

	return _eval(argv, NULL, 0, &pid);
}

static int endswith(const char *str, char *cmp)
{
	int cmp_len, str_len, i;

	cmp_len = strlen(cmp);
	str_len = strlen(str);
	if (cmp_len > str_len)
		return 0;
	for (i = 0; i < cmp_len; i++) {
		if (str[(str_len - 1) - i] != cmp[(cmp_len - 1) - i])
			return 0;
	}
	return 1;
}

static void execute_with_maxwait(char *const argv[], int wtime)
{
	pid_t pid;

	if (_eval(argv, NULL, 0, &pid) != 0)
		pid = -1;
	else {
		while (wtime-- > 0) {
			waitpid(pid, NULL, WNOHANG);	/* Reap the zombie if it has terminated. */
			if (kill(pid, 0) != 0) break;
			sleep(1);
		}
		_dprintf("%s killdon: errno: %d pid %d\n", argv[0], errno, pid);
	}
}

/* This is a bit ugly. Why didn't they allow another parameter to filter???? */
static char *filter_extension;
static int endswith_filter(const struct dirent *entry)
{
	return endswith(entry->d_name, filter_extension);
}

/* If the filename has an '&' character in it, don't wait at all. */
void run_userfile(char *folder, char *extension, const char *arg1, int wtime)
{
	unsigned char buf[PATH_MAX + 1];
	char *argv[] = { (char *)buf, (char *)arg1, NULL };
	struct dirent **namelist;
	int i, n;

	/* Do them in sorted order. */
	filter_extension = extension;
	n = scandir(folder, &namelist, endswith_filter, alphasort);
	if (n >= 0) {
		for (i = 0; i < n; ++i) {
			sprintf((char *) buf, "%s/%s", folder, namelist[i]->d_name);
			execute_with_maxwait(argv,
				strchr(namelist[i]->d_name, '&') ? 0 : wtime);
			free(namelist[i]);
		}
		free(namelist);
	}
}

/* Run user-supplied script(s), with 1 argument.
 * Return when the script(s) have finished,
 * or after wtime seconds, even if they aren't finished.
 *
 * Extract NAME from nvram variable named as "script_NAME".
 *
 * The sole exception to the nvram item naming rule is sesx.
 * That one is "sesx_script" rather than "script_sesx", due
 * to historical accident.
 *
 * The other exception is time-scheduled commands.
 * These have names that start with "sch_".
 * No directories are searched for corresponding user scripts.
 *
 * Execute in this order:
 *	nvram item: nv (run as a /bin/sh script)
 *		(unless nv starts with a dot)
 *	All files with a suffix of ".NAME" in these directories:
 *	/etc/config/
 *	/jffs/etc/config/
 *	/opt/etc/config/
 *	/mmc/etc/config/
 *	/tmp/config/
 */
/*
At this time, the names/events are:
   (Unless otherwise noted, there are no parameters.  Otherwise, one parameter).
   sesx		SES/AOSS Button custom script.  Param: ??
   brau		"bridge/auto" button pushed.  Param: mode (bridge/auto/etc)
   fire		When firewall service has been started or re-started.
   shut		At system shutdown, just before wan/lan/usb/etc. are stopped.
   init		At system startup, just before wan/lan/usb/etc. are started.
		The root filesystem and /jffs are mounted, but not any USB devices.
   usbmount	After an auto-mounted USB drive is mounted.
   usbumount	Before an auto-mounted USB drive is unmounted.
   usbhotplug	When any USB device is attached or removed.
   wanup	After WAN has come up.
   autostop	When a USB partition gets un-mounted.  Param: the mount-point (directory).
		If unmounted from the GUI, the directory is still mounted and accessible.
		If the USB drive was unplugged, it is still mounted but not accessible.

User scripts -- no directories are searched.  One parameter.
   autorun	When a USB disk partition gets auto-mounted. Param: the mount-point (directory).
		But not if the partition was already mounted.
		Only the files in that directory will be run.
*/
void run_nvscript(const char *nv, const char *arg1, int wtime)
{
	FILE *f;
	char *script;
	char s[PATH_MAX + 1];
	char *argv[] = { s, (char *)arg1, NULL };
	int check_dirs = 1;

	if (nv[0] == '.') {
		strcpy(s, nv);
	}
	else {
		script = nvram_get(nv);

		if ((script) && (*script != 0)) {
			sprintf(s, "/tmp/%s.sh", nv);
			if ((f = fopen(s, "w")) != NULL) {
				fputs("#!/bin/sh\n", f);
				fputs(script, f);
				fputs("\n", f);
				fclose(f);
				chmod(s, 0700);
				chdir("/tmp");

				_dprintf("Running: '%s %s'\n", argv[0], argv[1]? argv[1]: "");
				execute_with_maxwait(argv, wtime);
				chdir("/");
			}
		}

		sprintf(s, ".%s", nv);
		if (strncmp("sch_c", nv, 5) == 0) {
			check_dirs = 0;
		}
		else if (strncmp("sesx_", nv, 5) == 0) {
			s[5] = 0;
		}
		else if (strncmp("script_", nv, 7) == 0) {
			strcpy(&s[1], &nv[7]);
		}
	}

	if (nvram_match("userfiles_disable", "1")) {
		// backdoor to disable user scripts execution
		check_dirs = 0;
	}

	if ((check_dirs) && strcmp(s, ".") != 0) {
		_dprintf("checking for user scripts: '%s'\n", s);
		run_userfile("/etc/config", s, arg1, wtime);
		run_userfile("/jffs/etc/config", s, arg1, wtime);
		run_userfile("/opt/etc/config", s, arg1, wtime);
		run_userfile("/mmc/etc/config", s, arg1, wtime);
		run_userfile("/tmp/config", s, arg1, wtime);
	}
}

static void write_ct_timeout(const char *type, const char *name, unsigned int val)
{
	unsigned char buf[128];
	char v[16];

	sprintf((char *) buf, "/proc/sys/net/ipv4/netfilter/ip_conntrack_%s_timeout%s%s",
		type, (name && name[0]) ? "_" : "", name ? name : "");
	sprintf(v, "%u", val);

	f_write_string((const char *) buf, v, 0, 0);
}

#ifndef write_tcp_timeout
#define write_tcp_timeout(name, val) write_ct_timeout("tcp", name, val)
#endif

#ifndef write_udp_timeout
#define write_udp_timeout(name, val) write_ct_timeout("udp", name, val)
#endif

static unsigned int read_ct_timeout(const char *type, const char *name)
{
	unsigned char buf[128];
	unsigned int val = 0;
	char v[16];

	sprintf((char *) buf, "/proc/sys/net/ipv4/netfilter/ip_conntrack_%s_timeout%s%s",
		type, (name && name[0]) ? "_" : "", name ? name : "");
	if (f_read_string((const char *) buf, v, sizeof(v)) > 0)
		val = atoi(v);

	return val;
}

#ifndef read_tcp_timeout
#define read_tcp_timeout(name) read_ct_timeout("tcp", name)
#endif

#ifndef read_udp_timeout
#define read_udp_timeout(name) read_ct_timeout("udp", name)
#endif


void setup_ftp_conntrack(int port)
{
	char ports[32];

	if(port>0&&port!=21)
		sprintf(ports, "ports=21,%d", port);
	else sprintf(ports, "ports=21");

	if(!nvram_match("ftp_ports", ports))
	{
		ct_modprobe_r("ftp");
		ct_modprobe("ftp", ports);
		nvram_set("ftp_ports", ports);
	}
}

void setup_udp_timeout(int connflag)
{
	unsigned int v[10];
	const char *p;
	char buf[70];

	if (connflag
#ifdef RTCONFIG_WIRELESSREPEATER
			&& nvram_get_int("sw_mode")!=SW_MODE_REPEATER
#endif
	) {
		p = nvram_safe_get("ct_udp_timeout");
		if (sscanf(p, "%u%u", &v[0], &v[1]) == 2) {
			write_udp_timeout(NULL, v[0]);
			write_udp_timeout("stream", v[1]);
		}
		else {
			v[0] = read_udp_timeout(NULL);
			v[1] = read_udp_timeout("stream");
			sprintf(buf, "%u %u", v[0], v[1]);
			nvram_set("ct_udp_timeout", buf);
		}
	}
	else {
		write_udp_timeout(NULL, 1);
		write_udp_timeout("stream", 6);
	}
}

int scan_icmp_unreplied_conntrack()
{
	FILE *fp = fopen( "/proc/net/nf_conntrack", "r" );
	char buff[1024], ipv[16], ipv_num[16], protocol[16], protocol_num[16];
	int found = 0;

	if (fp == NULL) {
		perror( "openning /proc/net/nf_conntrack" );
		return -1;
	}

	while (fgets(buff, sizeof(buff), fp) != NULL) {
		sscanf(buff, "%s %s %s %s", ipv, ipv_num, protocol, protocol_num);

		if (memcmp(protocol, "icmp", 4) || !strstr(buff, "UNREPLIED")) continue;

//		dbG("\n%s %s %s %s\n", ipv, ipv_num, protocol, protocol_num);

		found++;
		break;
	}

	fclose(fp);

//	if (!found)
//		dbG("No matching conntrack found\n");

	return found;
}

void setup_ct_timeout(int connflag)
{
	unsigned int v[10];
	const char *p;
	char buf[70];
	int i;

	if (connflag
#ifdef RTCONFIG_WIRELESSREPEATER
			&& nvram_get_int("sw_mode")!=SW_MODE_REPEATER
#endif
	) {
		p = nvram_safe_get("ct_timeout");
		if (sscanf(p, "%u%u", &v[0], &v[1]) == 2) {
//			write_ct_timeout("generic", NULL, v[0]);
			write_ct_timeout("icmp", NULL, v[1]);
		}
		else {
			v[0] = read_ct_timeout("generic", NULL);
			v[1] = read_ct_timeout("icmp", NULL);

			sprintf(buf, "%u %u", v[0], v[1]);
			nvram_set("ct_timeout", buf);
		}
	}
	else {
		for (i = 0; i < 3; i++)
		{
			if (scan_icmp_unreplied_conntrack() > 0)
			{
//				write_ct_timeout("generic", NULL, 0);
				write_ct_timeout("icmp", NULL, 0);
				sleep(2);
			}
		}
	}
}

void setup_conntrack(void)
{
	unsigned int v[10];
	const char *p;
	char buf[70];
	int i;

	p = nvram_safe_get("ct_tcp_timeout");
	if (sscanf(p, "%u%u%u%u%u%u%u%u%u%u",
		&v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9]) == 10) {	// lightly verify
		write_tcp_timeout("established", v[1]);
		write_tcp_timeout("syn_sent", v[2]);
		write_tcp_timeout("syn_recv", v[3]);
		write_tcp_timeout("fin_wait", v[4]);
		write_tcp_timeout("time_wait", v[5]);
		write_tcp_timeout("close", v[6]);
		write_tcp_timeout("close_wait", v[7]);
		write_tcp_timeout("last_ack", v[8]);
	}
	else {
		v[1] = read_tcp_timeout("established");
		v[2] = read_tcp_timeout("syn_sent");
		v[3] = read_tcp_timeout("syn_recv");
		v[4] = read_tcp_timeout("fin_wait");
		v[5] = read_tcp_timeout("time_wait");
		v[6] = read_tcp_timeout("close");
		v[7] = read_tcp_timeout("close_wait");
		v[8] = read_tcp_timeout("last_ack");
		sprintf(buf, "0 %u %u %u %u %u %u %u %u 0",
			v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8]);
		nvram_set("ct_tcp_timeout", buf);
	}

	setup_udp_timeout(FALSE);

	p = nvram_safe_get("ct_timeout");
	if (sscanf(p, "%u%u", &v[0], &v[1]) == 2) {
//		write_ct_timeout("generic", NULL, v[0]);
		write_ct_timeout("icmp", NULL, v[1]);
	}
	else {
		v[0] = read_ct_timeout("generic", NULL);
		v[1] = read_ct_timeout("icmp", NULL);
		sprintf(buf, "%u %u", v[0], v[1]);
		nvram_set("ct_timeout", buf);
	}

#ifdef LINUX26
	p = nvram_safe_get("ct_hashsize");
	i = atoi(p);
	if (i >= 127) {
		f_write_string("/sys/module/nf_conntrack/parameters/hashsize", p, 0, 0);
	}
	else if (f_read_string("/sys/module/nf_conntrack/parameters/hashsize", buf, sizeof(buf)) > 0) {
		if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';
		if (atoi(buf) > 0) nvram_set("ct_hashsize", buf);
	}
#endif
#ifdef LINUX26
	p = nvram_safe_get("ct_max");
	i = atoi(p);
	if (i >= 128) {
		f_write_string("/proc/sys/net/nf_conntrack_max", p, 0, 0);
	}
	else if (f_read_string("/proc/sys/net/nf_conntrack_max", buf, sizeof(buf)) > 0) {
		if (atoi(buf) > 0) nvram_set("ct_max", buf);
	}
#else
	p = nvram_safe_get("ct_max");
	i = atoi(p);
	if (i >= 128) {
		f_write_string("/proc/sys/net/ipv4/netfilter/ip_conntrack_max", p, 0, 0);
	}
	else if (f_read_string("/proc/sys/net/ipv4/netfilter/ip_conntrack_max", buf, sizeof(buf)) > 0) {
		if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';
		if (atoi(buf) > 0) nvram_set("ct_max", buf);
	}
#endif
#if 0
	if (!nvram_match("nf_rtsp", "0")) {
		ct_modprobe("rtsp");
	}
	else {
		ct_modprobe_r("rtsp");
	}

	if (!nvram_match("nf_h323", "0")) {
		ct_modprobe("h323");
	}
	else {
		ct_modprobe_r("h323");
	}

#ifdef LINUX26
	if (!nvram_match("nf_sip", "0")) {
		ct_modprobe("sip");
	}
	else {
		ct_modprobe_r("sip");
	}
#endif
#endif
	// !!TB - FTP Server
#ifdef RTCONFIG_FTP
	i = nvram_get_int("ftp_port");
	if (nvram_match("ftp_enable", "1") && (i > 0) && (i != 21))
	{
		char ports[32];

		sprintf(ports, "ports=21,%d", i);
		ct_modprobe("ftp", ports);
	}
	else 
#endif
	if (!nvram_match("nf_ftp", "0")
#ifdef RTCONFIG_FTP
		|| nvram_match("ftp_enable", "1")	// !!TB - FTP Server
#endif
		) {
		ct_modprobe("ftp");
	}
	else {
		ct_modprobe_r("ftp");
	}

	if (!nvram_match("nf_pptp", "0")) {
		ct_modprobe("proto_gre");
		ct_modprobe("pptp");
	}
	else {
		ct_modprobe_r("pptp");
		ct_modprobe_r("proto_gre");
	}
}

void setup_pt_conntrack(void)
{
	if (!nvram_match("fw_pt_rtsp", "0")) {
		ct_modprobe("rtsp", "ports=554,8554");
	}
	else {
		ct_modprobe_r("rtsp");
	}

	if (!nvram_match("fw_pt_h323", "0")) {
		ct_modprobe("h323");
	}
	else {
		ct_modprobe_r("h323");
	}

#ifdef LINUX26
	if (!nvram_match("fw_pt_sip", "0")) {
		ct_modprobe("sip");
	}
	else {
		ct_modprobe_r("sip");
	}
#endif
}

void remove_conntrack(void)
{
	ct_modprobe_r("pptp");
	ct_modprobe_r("ftp");
	ct_modprobe_r("rtsp");
	ct_modprobe_r("h323");
#ifdef LINUX26
	ct_modprobe_r("sip");
#endif
}

void inc_mac(char *mac, int plus)
{
	unsigned char m[6];
	int i;

	for (i = 0; i < 6; i++)
		m[i] = (unsigned char) strtol(mac + (3 * i), (char **)NULL, 16);
	while (plus != 0) {
		for (i = 5; i >= 3; --i) {
			m[i] += (plus < 0) ? -1 : 1;
			if (m[i] != 0) break;	// continue if rolled over
		}
		plus += (plus < 0) ? 1 : -1;
	}
	sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
		m[0], m[1], m[2], m[3], m[4], m[5]);
}

void set_mac(const char *ifname, const char *nvname, int plus)
{
	int sfd;
	struct ifreq ifr;
	int up;
	int j;

	if ((sfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		_dprintf("%s: %s %d\n", ifname, __FUNCTION__, __LINE__);
		return;
	}

	strcpy(ifr.ifr_name, ifname);

	up = 0;
	if (ioctl(sfd, SIOCGIFFLAGS, &ifr) == 0) {
		if ((up = ifr.ifr_flags & IFF_UP) != 0) {
			ifr.ifr_flags &= ~IFF_UP;
			if (ioctl(sfd, SIOCSIFFLAGS, &ifr) != 0) {
				_dprintf("%s: %s %d\n", ifname, __FUNCTION__, __LINE__);
			}
		}
	}
	else {
		_dprintf("%s: %s %d\n", ifname, __FUNCTION__, __LINE__);
	}

	if (!ether_atoe(nvram_safe_get(nvname), (unsigned char *)&ifr.ifr_hwaddr.sa_data)) {
#ifdef RTCONFIG_RGMII_BRCM5301X
		if (!ether_atoe(nvram_safe_get("lan_hwaddr"), (unsigned char *)&ifr.ifr_hwaddr.sa_data)) {
#else
		if (!ether_atoe(nvram_safe_get("et0macaddr"), (unsigned char *)&ifr.ifr_hwaddr.sa_data)) {
#endif

			// goofy et0macaddr, make something up
#ifdef RTCONFIG_RGMII_BRCM5301X
			nvram_set("lan_hwaddr", "00:01:23:45:67:89");
#else
			nvram_set("et0macaddr", "00:01:23:45:67:89");
#endif
			ifr.ifr_hwaddr.sa_data[0] = 0;
			ifr.ifr_hwaddr.sa_data[1] = 0x01;
			ifr.ifr_hwaddr.sa_data[2] = 0x23;
			ifr.ifr_hwaddr.sa_data[3] = 0x45;
			ifr.ifr_hwaddr.sa_data[4] = 0x67;
			ifr.ifr_hwaddr.sa_data[5] = 0x89;
		}

		while (plus-- > 0) {
			for (j = 5; j >= 3; --j) {
				ifr.ifr_hwaddr.sa_data[j]++;
				if (ifr.ifr_hwaddr.sa_data[j] != 0) break;	// continue if rolled over
			}
		}
	}

	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	if (ioctl(sfd, SIOCSIFHWADDR, &ifr) == -1) {
		_dprintf("Error setting %s address\n", ifname);
	}

	if (up) {
		if (ioctl(sfd, SIOCGIFFLAGS, &ifr) == 0) {
			ifr.ifr_flags |= IFF_UP|IFF_RUNNING;
			if (ioctl(sfd, SIOCSIFFLAGS, &ifr) == -1) {
				_dprintf("%s: %s %d\n", ifname, __FUNCTION__, __LINE__);
			}
		}
		else {
			_dprintf("%s: %s %d\n", ifname, __FUNCTION__, __LINE__);
		}
	}

	close(sfd);
}

/*
const char *default_wanif(void)
{
	return ((strtoul(nvram_safe_get("boardflags"), NULL, 0) & BFL_ENETVLAN) ||
		(check_hw_type() == HW_BCM4712)) ? "vlan1" : "eth1";
}
*/

/*
const char *default_wlif(void)
{
	switch (check_hw_type()) {
	case HW_BCM4702:
	case HW_BCM4704_BCM5325F:
	case HW_BCM4704_BCM5325F_EWC:
		return "eth2";
	}
	return "eth1";

}
*/

void simple_unlock(const char *name)
{
	char fn[256];

	snprintf(fn, sizeof(fn), "/var/lock/%s.lock", name);
	f_write(fn, NULL, 0, 0, 0600);
}

void simple_lock(const char *name)
{
	int n;
	char fn[256];

	n = 5 + (getpid() % 10);
	snprintf(fn, sizeof(fn), "/var/lock/%s.lock", name);
	while (unlink(fn) != 0) {
		if (--n == 0) {
			syslog(LOG_DEBUG, "Breaking %s", fn);
			break;
		}
		sleep(1);
	}
}

void killall_tk(const char *name)
{
	int n;

	if (killall(name, SIGTERM) == 0) {
		n = 10;
		while ((killall(name, 0) == 0) && (n-- > 0)) {
			_dprintf("%s: waiting name=%s n=%d\n", __FUNCTION__, name, n);
			usleep(100 * 1000);
		}
		if (n < 0) {
			n = 10;
			while ((killall(name, SIGKILL) == 0) && (n-- > 0)) {
				_dprintf("%s: SIGKILL name=%s n=%d\n", __FUNCTION__, name, n);
				usleep(100 * 1000);
			}
		}
	}
}

void kill_pidfile_tk(const char *pidfile)
{
	FILE *fp;
	char buf[256];
	pid_t pid = 0;
	int n;

	if ((fp = fopen(pidfile, "r")) != NULL) {
		if (fgets(buf, sizeof(buf), fp) != NULL)
			pid = strtoul(buf, NULL, 0);
		fclose(fp);
	}

	if (pid > 1 && kill(pid, SIGTERM) == 0) {
		n = 10;
		while ((kill(pid, 0) == 0) && (n-- > 0)) {
			_dprintf("%s: waiting pid=%d n=%d\n", __FUNCTION__, pid, n);
			usleep(100 * 1000);
		}
		if (n < 0) {
			n = 10;
			while ((kill(pid, SIGKILL) == 0) && (n-- > 0)) {
				_dprintf("%s: SIGKILL pid=%d n=%d\n", __FUNCTION__, pid, n);
				usleep(100 * 1000);
			}
		}
	}
}

long fappend(FILE *out, const char *fname)
{
	FILE *in;
	char buf[1024];
	int n;
	long r;

	if ((in = fopen(fname, "r")) == NULL) return -1;
	r = 0;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) {
			r = -1;
			break;
		}
		else {
			r += n;
		}
	}
	fclose(in);
	return r;
}

long fappend_file(const char *path, const char *fname)
{
	FILE *f;
	int r = -1;

	if (f_exists(fname) && (f = fopen(path, "a")) != NULL) {
		r = fappend(f, fname);
		fclose(f);
	}
	return r;
}

/* uclibc accepts any of following:
 * [STD][Offset][DST], where
 * [STD] ~ any [a-zA-Z]
 * [DST] ~ any [a-zA-Z] */
#define CONVERT_TZ_TO_GMT_DST
#ifdef CONVERT_TZ_TO_GMT_DST
int gettzoffset(char *tzstr, char *tzstr1, int size1)
{
	char offstr[32];
	char *tzptr = tzstr;
	char *offptr = offstr;
	int ret = 0;
	int dst = 0;

	memset(offstr, 0, sizeof(offstr));
	for ( ; *tzptr; tzptr++) {
		if (*tzptr=='-'||*tzptr=='+'||*tzptr==':'||isdigit(*tzptr)) {
			*offptr++ = *tzptr;
			ret = 1;
		} else if (ret) {
			dst = isalpha(*tzptr);
			break;
		}
	}

	if (ret)
		snprintf(tzstr1, size1, "GMT%s%s", offstr, dst ? "DST" : "");
	return ret;
}
#endif

void time_zone_x_mapping(void)
{
	FILE *fp;
	char tmpstr[32];
	char *ptr;

	/* pre mapping */
	if (nvram_match("time_zone", "KST-9KDT"))
		nvram_set("time_zone", "UCT-9_1");
	else if (nvram_match("time_zone", "RFT-9RFTDST"))
		nvram_set("time_zone", "UCT-9_2");
	else if (nvram_match("time_zone", "UTC-2DST_1"))	/*Minsk*/
		nvram_set("time_zone", "UTC-3_3");
	else if (nvram_match("time_zone", "UTC-4_2"))		/*Moscow, St. Petersburg*/
		nvram_set("time_zone", "UTC-3_4");
	else if (nvram_match("time_zone", "UTC-4_3"))		/*Volgograd*/
		nvram_set("time_zone", "UTC-3_5");
	else if (nvram_match("time_zone", "UTC-6_1"))		/*Yekaterinburg*/
		nvram_set("time_zone", "UTC-5_1");
	else if (nvram_match("time_zone", "UTC-7_1"))		/*Novosibirsk*/
		nvram_set("time_zone", "UTC-6_2");
	else if (nvram_match("time_zone", "CST-8_2"))		/*Krasnoyarsk*/
		nvram_set("time_zone", "CST-7_2");
	else if (nvram_match("time_zone", "UTC-9_2"))		/*Irkutsk*/
		nvram_set("time_zone", "UTC-8_1");
	else if (nvram_match("time_zone", "UTC-10_3"))		/*Yakutsk*/
		nvram_set("time_zone", "UTC-9_3");
	else if (nvram_match("time_zone", "UTC-11_2"))		/*Vladivostok*/
		nvram_set("time_zone", "UTC-10_4");
	else if (nvram_match("time_zone", "UTC-12_1"))          /*Magadan*/
		nvram_set("time_zone", "UTC-10_5");

	snprintf(tmpstr, sizeof(tmpstr), "%s", nvram_safe_get("time_zone"));
	/* replace . with : */
	while ((ptr=strchr(tmpstr, '.'))!=NULL) *ptr = ':';
	/* remove *_? */
	while ((ptr=strchr(tmpstr, '_'))!=NULL) *ptr = 0x0;

	/* check time_zone_dst for daylight saving */
	if (nvram_get_int("time_zone_dst"))
		sprintf(tmpstr, "%s,%s", tmpstr, nvram_safe_get("time_zone_dstoff"));
#ifdef CONVERT_TZ_TO_GMT_DST
	else	gettzoffset(tmpstr, tmpstr, sizeof(tmpstr));
#endif

	nvram_set("time_zone_x", tmpstr);

	/* special mapping */
	if (nvram_match("time_zone", "JST"))
		nvram_set("time_zone_x", "UCT-9");
#if 0
	else if (nvram_match("time_zone", "TST-10TDT"))
		nvram_set("time_zone_x", "UCT-10");
	else if (nvram_match("time_zone", "CST-9:30CDT"))
		nvram_set("time_zone_x", "UCT-9:30");
#endif

	if ((fp = fopen("/etc/TZ", "w")) != NULL) {
		fprintf(fp, "%s\n", tmpstr);
		fclose(fp);
	}
}

/* Get the timezone from NVRAM and set the timezone in the kernel
 * and export the TZ variable 
 */
void
setup_timezone(void)
{
#ifndef RC_BUILDTIME
#define RC_BUILDTIME	1438387200	// Aug 1 00:00:00 GMT 2015
#endif
	time_t now;
	struct tm gm, local;
	struct timezone tz;
	struct timeval tv = { RC_BUILDTIME, 0 };
	struct timeval *tvp = NULL;

	/* Export TZ variable for the time libraries to 
	 * use.
	 */
	time_zone_x_mapping();
	setenv("TZ", nvram_get("time_zone_x"), 1);

	/* Update kernel timezone */
	time(&now);
	gmtime_r(&now, &gm);
	localtime_r(&now, &local);
	gm.tm_isdst = local.tm_isdst;
	tz.tz_minuteswest = (mktime(&gm) - mktime(&local)) / 60;

	/* Setup sane start time */
	if (now < RC_BUILDTIME) {
		struct sysinfo info;

		sysinfo(&info);
		tv.tv_sec += info.uptime;
		tvp = &tv;
	}

	settimeofday(tvp, &tz);
}

int
is_invalid_char_for_hostname(char c)
{
	int ret = 0;

	if (c <= 0x2c)				/* SPACE !"#$%&'()*+, */
		ret = 1;
	else if (c >= 0x2e && c <= 0x2f)	/* ./ */
		ret = 1;
	else if (c >= 0x3a && c <= 0x40)	/* :;<=>?@ */
		ret = 1;
#if 0
	else if (c >= 0x5b && c <= 0x60)	/* [\]^_ */
		ret = 1;
#else	/* allow '_' */
	else if (c >= 0x5b && c <= 0x5e)	/* [\]^ */
		ret = 1;
	else if (c == 0x60)			/* ` */
		ret = 1;
#endif
	else if (c >= 0x7b)			/* {|}~ DEL */
		ret = 1;
#if 0
	printf("%c (0x%02x) is %svalid for hostname\n", c, c, (ret == 0) ? "  " : "in");
#endif
	return ret;
}

int
is_valid_hostname(const char *name)
{
	int len, i;

	if (!name)
		return 0;

	len = strlen(name);
	for (i = 0; i < len ; i++) {
		if (is_invalid_char_for_hostname(name[i])) {
			len = 0;
			break;
		}
	}
#if 0
	printf("%s is %svalid for hostname\n", name, len ? "" : "in");
#endif
	return len;
}

int get_meminfo_item(const char *name)
{
	FILE *fp;
	char memdata[256] = {0};
	int mem = 0;

	if (!name || *name == '\0')
		return -1;

	if ((fp = fopen("/proc/meminfo", "r")) != NULL) {
		/* get one memory parameter specified by the name */
		while (fgets(memdata, 255, fp) != NULL) {
			if (strstr(memdata, name) != NULL) {
				sscanf(memdata, "%*s %d kB", &mem);
				break;
			}
		}
		fclose(fp);
	}

	return mem;
}

#ifdef RTCONFIG_SHP
void restart_lfp()
{
	char v[32];

	if(nvram_get_int("lfp_disable")==0) {
		sprintf(v, "%x", inet_addr(nvram_safe_get("lan_ipaddr")));
		f_write_string("/proc/net/lfpctrl", v, 0, 0);
	}
	else {
		f_write_string("/proc/net/lfpctrl", "", 0, 0);
	}
}
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
int setup_dnsmq(int mode)
{
	char v[32];
	char tmp[32];

	if(mode) {
		// setup ebtables
		eval("ebtables", "-F");
		eval("ebtables", "-t", "broute", "-F");
		eval("ebtables", "-t", "broute", "-I", "BROUTING", "-d", "00:E0:11:22:33:44", "-j", "redirect", "--redirect-target", "DROP");
	}
	else {
		eval("ebtables", "-F");
		eval("ebtables", "-t", "broute", "-F");
		eval("ebtables", "-I", "FORWARD", "-i", "eth1", "-j", "DROP");
	}	
	
	eval("iptables", "-t", "nat", "-F", "PREROUTING");
	eval("iptables", "-t", "nat", "-I", "PREROUTING", "-p", "udp", "--dport", "53", "-j", "DNAT", "--to-destination", strcat_r(nvram_safe_get("lan_ipaddr"), ":18018", tmp));

	if(mode) {
		eval("iptables", "-t", "nat", "-I", "PREROUTING", "-p", "tcp", "-d", "10.0.0.1", "--dport", "80", "-j", "DNAT", "--to-destination", strcat_r(nvram_safe_get("lan_ipaddr"), ":80", tmp));
	
		//sprintf(v, "%x my.%s", inet_addr("10.0.0.1"), get_productid());
		sprintf(v, "%x %s", inet_addr(nvram_safe_get("lan_ipaddr")), DUT_DOMAIN_NAME);
		f_write_string("/proc/net/dnsmqctrl", v, 0, 0);
	}
	else {
		// setup ebtables and iptables
		eval("iptables", "-t", "nat", "-I", "PREROUTING", "-p", "tcp", "-d", "10.0.0.1", "--dport", "80", "-j", "DNAT", "--to-destination", strcat_r(nvram_safe_get("lan_ipaddr"), ":18017", tmp));
	
		f_write_string("/proc/net/dnsmqctrl", "", 0, 0);
	}

	return 0;
}
#endif


int
is_invalid_char_for_volname(char c)
{
	int ret = 0;

	if (c < 0x20)
		ret = 1;
#if 0
	else if (c >= 0x21 && c <= 0x2c)
		ret = 1;
#else	/* allow '+' */
	else if (c >= 0x21 && c <= 0x2a)	/* !"#$%&'()* */
		ret = 1;
	else if (c == 0x2c)			/* , */
		ret = 1;
#endif
	else if (c >= 0x2e && c <= 0x2f)
		ret = 1;
	else if (c >= 0x3a && c <= 0x40)
		ret = 1;
	else if (c >= 0x5b && c <= 0x5e)
		ret = 1;
	else if (c == 0x60)
		ret = 1;
	else if (c >= 0x7b)
		ret = 1;
	return ret;
}

int
is_valid_volname(const char *name)
{
	int len, i;

	if (!name)
		return 0;

	len = strlen(name);
	for (i = 0; i < len ; i++) {
		if (is_invalid_char_for_volname(name[i])) {
			len = 0;
			break;
		}
	}
	return len;
}


void stop_if_misc(void)
{
	DIR *dir;
	struct dirent *dirent;
	struct ifreq ifr;
	int sfd;

	if ((dir = opendir("/proc/sys/net/ipv4/conf")) != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
				continue;

			if (strcmp(dirent->d_name, "all") &&
				strcmp(dirent->d_name, "default") &&
				strcmp(dirent->d_name, "lo") &&
				strncmp(dirent->d_name, "br", 2) &&
				strncmp(dirent->d_name, "eth", 3) &&
				strncmp(dirent->d_name, "ra", 2) &&
				!((sfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0))
			{
				strcpy(ifr.ifr_name, dirent->d_name);
				if (!ioctl(sfd, SIOCGIFFLAGS, &ifr) && (ifr.ifr_flags & IFF_UP))
					ifconfig(ifr.ifr_name, 0, NULL, NULL);

				close(sfd);
			}

		}
		closedir(dir);
	}
}

int mssid_mac_validate(const char *macaddr)
{
	unsigned char mac_binary[6];
	unsigned long long macvalue;
	char macbuf[13];

	if (!macaddr || !strlen(macaddr))
		return 0;

	ether_atoe(macaddr, mac_binary);
	sprintf(macbuf, "%02X%02X%02X%02X%02X%02X",
		mac_binary[0],
		mac_binary[1],
		mac_binary[2],
		mac_binary[3],
		mac_binary[4],
		mac_binary[5]);
	macvalue = strtoll(macbuf, (char **) NULL, 16);
	if (macvalue % 4)
		return 0;
	else
		return 1;
}
