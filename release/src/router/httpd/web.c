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
/*
 * ASUS Home Gateway Reference Design
 * Web Page Configuration Support Routines
 *
 * Copyright 2001, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of ASUSTeK Inc..
 *
 * $Id: web_ex.c,v 1.4 2007/04/09 12:01:50 shinjung Exp $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <httpd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/klog.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <dirent.h>
#include <proto/ethernet.h>   //add by Viz 2010.08
#include <net/route.h>
#include <sys/ioctl.h>
#ifndef RTCONFIG_BCMARM
#include <math.h>	//for ceil()
#endif

#include <typedefs.h>
#include <bcmutils.h>
#include <shutils.h>
#include <bcmnvram.h>
#include <bcmnvram_f.h>
#include <common.h>
#include <shared.h>
#include <rtstate.h>
#include "iptraffic.h"
#include <sys/sysinfo.h>

#ifdef RTCONFIG_FANCTRL
#include <wlutils.h>
#endif

#ifdef RTCONFIG_DSL
#include <web-dsl.h>
#include <web-dsl-upg.h>
#endif

#ifdef RTCONFIG_USB
#include <usb_info.h>
#include <disk_io_tools.h>
#include <disk_initial.h>
#include <disk_share.h>

#include <apps.h>
#else
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
#endif
#include "initial_web_hook.h"
//#endif

//#ifdef RTCONFIG_OPENVPN
#include "openvpn_options.h"
//#endif

#include <net/if.h>
#include <linux/sockios.h>
#include "../networkmap/networkmap.h"
//#include <networkmap.h> //2011.03 Yau add for new networkmap
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sysinfo.h>

#include "sysinfo.h"
#include "data_arrays.h"

#ifdef RTCONFIG_QTN
#include "web-qtn.h"
#endif

#ifdef RTCONFIG_BWDPI
#include "bwdpi.h"
#include "sqlite3.h"
#include "bwdpi_sqlite.h"
#endif

#ifdef RTCONFIG_TRAFFIC_CONTROL
#include "traffic_control.h"
#endif

#include <json.h>

#ifdef RTCONFIG_HTTPS
extern int do_ssl;
extern int ssl_stream_fd;
#endif

extern int ej_wl_sta_list_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_sta_list_5g(int eid, webs_t wp, int argc, char_t **argv);
#ifndef RTCONFIG_QTN
extern int ej_wl_sta_list_5g_2(int eid, webs_t wp, int argc, char_t **argv);
#endif
#ifdef RTCONFIG_STAINFO
extern int ej_wl_stainfo_list_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_stainfo_list_5g(int eid, webs_t wp, int argc, char_t **argv);
#ifndef RTCONFIG_QTN
extern int ej_wl_stainfo_list_5g_2(int eid, webs_t wp, int argc, char_t **argv);
#endif
#endif
extern int ej_wl_auth_list(int eid, webs_t wp, int argc, char_t **argv);
#ifdef CONFIG_BCMWL5
extern int ej_wl_control_channel(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_extent_channel(int eid, webs_t wp, int argc, char_t **argv);
#endif

#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
extern int ej_SiteSurvey(int eid, webs_t wp, int argc, char_t **argv);
#endif

extern int ej_wl_scan_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_scan_5g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_scan_5g_2(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_channel_list_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_channel_list_5g(int eid, webs_t wp, int argc, char_t **argv);
#ifdef RTCONFIG_QTN
extern int ej_wl_channel_list_5g_20m(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_channel_list_5g_40m(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_channel_list_5g_80m(int eid, webs_t wp, int argc, char_t **argv);
#endif
extern int ej_wl_channel_list_5g_2(int eid, webs_t wp, int argc, char_t **argv);
#ifdef CONFIG_BCMWL5
extern int ej_wl_chanspecs_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_chanspecs_5g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_chanspecs_5g_2(int eid, webs_t wp, int argc, char_t **argv);
#endif
#if defined(CONFIG_BCMWL5) || (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) || defined(RTCONFIG_QCA)
extern int ej_wl_rate_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_rate_5g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_rate_5g_2(int eid, webs_t wp, int argc, char_t **argv);
#endif
#ifdef RTCONFIG_PROXYSTA
int ej_wl_auth_psta(int eid, webs_t wp, int argc, char_t **argv);
#endif

#ifdef RTCONFIG_IPV6
extern int ej_lan_ipv6_network(int eid, webs_t wp, int argc, char_t **argv);
#endif

extern int ej_get_default_reboot_time(int eid, webs_t wp, int argc, char_t **argv);

static int b64_decode( const char* str, unsigned char* space, int size );

extern void send_login_page(int fromapp_flag, int error_status);
extern void __send_login_page(int fromapp_flag, int error_status);

extern char *get_cgi_json(char *name, json_object *root);

void add_asus_token(char *token);

#define wan_prefix(unit, prefix)	snprintf(prefix, sizeof(prefix), "wan%d_", unit)

#define nvram_default_safe_get(name) (nvram_default_get(name) ? : "")

/*
#define csprintf(fmt, args...) do{\
	FILE *cp = fopen("/dev/console", "w");\
	if (cp) {\
		fprintf(cp, fmt, ## args);\
		fclose(cp);\
	}\
}while (0)
*/
// 2008.08 magic }

#include <sys/mman.h>
typedef uint32_t __u32; //2008.08 magic
#ifndef	O_BINARY		/* should be define'd on __WIN32__ */
#define O_BINARY	0
#endif
#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif

/* #define sys_upgrade(image) eval("mtd-write", "-i", image, "-d", "linux"); */
#define sys_upload(image) eval("nvram", "restore", image)
#define sys_download(file) eval("nvram", "save", file)
#define sys_default() notify_rc("resetdefault"); //   eval("mtd-erase", "-d", "nvram")
#define sys_reboot() notify_rc("reboot");

#define PROFILE_HEADER 	"HDR1"
#ifdef RTCONFIG_DSL
#define PROFILE_HEADER_NEW	"N55U"
#else
#if RTCONFIG_QCA
#define PROFILE_HEADER_NEW	"AC55U"
#else
#define PROFILE_HEADER_NEW	"HDR2"
#endif
#endif
#define IH_MAGIC	0x27051956	/* Image Magic Number		*/

int count_sddev_mountpoint();

#define Ralink_WPS	1 //2009.01 magic

char ibuf2[8192];

//static int ezc_error = 0;

#define ACTION_UPGRADE_OK   0
#define ACTION_UPGRADE_FAIL 1

int action;

char *serviceId;
#define MAX_GROUP_ITEM 10
#define MAX_GROUP_COUNT 300
#define MAX_LINE_SIZE 512
char *groupItem[MAX_GROUP_ITEM];
char urlcache[128];
char *next_host;
int delMap[MAX_GROUP_COUNT];
char SystemCmd[128];
char UserID[32]="";
char UserPass[32]="";
char ProductID[32]="";
extern int redirect;
extern int change_passwd;	// 2008.08 magic
extern int reget_passwd;	// 2008.08 magic
extern int skip_auth;
extern char host_name[64];
extern char user_agent[1024];
extern char referer_host[64];
extern unsigned int login_ip_tmp;

extern time_t login_timestamp; // the timestamp of the logined ip
extern time_t login_timestamp_tmp; // the timestamp of the current session.
extern time_t last_login_timestamp; // the timestamp of the current session.
extern unsigned int login_try;
extern unsigned int MAX_login;

#ifdef RTCONFIG_JFFS2USERICON
#define JFFS_USERICON		"/jffs/usericon/"
#endif

static void insert_hook_func(webs_t wp, char *fname, char *param)
{
	if (!wp || !fname) {
		_dprintf("%s: invalid parameter (%p, %p, %p)\n", __func__, wp, fname, param);
		return;
	}

	if (!param)
		param = "";

	websWrite(wp, "<script>\n");
	websWrite(wp, "%s(%s);\n", fname, param);
	websWrite(wp, "</script>\n");
}

char *
rfctime(const time_t *timep)
{
	static char s[201];
	struct tm tm;

	//it suppose to be convert after applying
	//time_zone_x_mapping();
	setenv("TZ", nvram_safe_get_x("", "time_zone_x"), 1);
	memcpy(&tm, localtime(timep), sizeof(struct tm));
	strftime(s, 200, "%a, %d %b %Y %H:%M:%S %z", &tm);
	return s;
}

void
reltime(unsigned int seconds, char *cs)
{
#ifdef SHOWALL
	int days=0, hours=0, minutes=0;

	if (seconds > 60*60*24) {
		days = seconds / (60*60*24);
		seconds %= 60*60*24;
	}
	if (seconds > 60*60) {
		hours = seconds / (60*60);
		seconds %= 60*60;
	}
	if (seconds > 60) {
		minutes = seconds / 60;
		seconds %= 60;
	}
	sprintf(cs, "%d days, %d hours, %d minutes, %d seconds", days, hours, minutes, seconds);
#else
	sprintf(cs, "%d secs", seconds);
#endif
}

/******************************************************************************/
/*
 *	Redirect the user to another webs page
 */

//2008.08 magic{
void websRedirect(webs_t wp, char_t *url)
{
	websWrite(wp, T("<html><head>\r\n"));

	if(strchr(url, '>') || strchr(url, '<'))
	{
		websWrite(wp,"<script>parent.location.href='/index.asp';</script>\n");
	}
	else
	{
#ifdef RTCONFIG_HTTPS
		if(do_ssl){
			//websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=https://%s/%s\">\r\n"), gethost(), url);
			websWrite(wp,"<script>parent.location.href='/%s';</script>\n",url);
		}else
#endif	
		{
			//websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=http://%s/%s\">\r\n"), gethost(), url);
			websWrite(wp,"<script>parent.location.href='/%s';</script>\n",url);
		}
	}

	websWrite(wp, T("<meta http-equiv=\"Content-Type\" content=\"text/html\">\r\n"));
	websWrite(wp, T("</head></html>\r\n"));

	websDone(wp, 200);
}

void websRedirect_iframe(webs_t wp, char_t *url)
{
        websWrite(wp, T("<html><head>\r\n"));

        if(strchr(url, '>') || strchr(url, '<'))
        {
                websWrite(wp,"<script>parent.location.href='/index.asp';</script>\n");
        }
        else
        {
#ifdef RTCONFIG_HTTPS
                if(do_ssl){
                        websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=https://%s/%s\">\r\n"), gethost(), url);
                }else
#endif
                {
			websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=http://%s/%s\">\r\n"), gethost(), url);
                }
        }

        websWrite(wp, T("<meta http-equiv=\"Content-Type\" content=\"text/html\">\r\n"));
        websWrite(wp, T("</head></html>\r\n"));

        websDone(wp, 200);
}
//2008.08 magic}

void sys_script(char *name)
{

     char scmd[64];

     sprintf(scmd, "/tmp/%s", name);
     //printf("run %s %d %s\n", name, strlen(name), scmd);	// tmp test

     //handle special scirpt first
     if (strcmp(name,"syscmd.sh")==0)
     {
	   if (strcmp(SystemCmd, "")!=0)
	   {
		snprintf(SystemCmd, sizeof(SystemCmd), "%s > /tmp/syscmd.log 2>&1 && echo 'XU6J03M6' >> /tmp/syscmd.log &\n", SystemCmd);	// oleg patch
	   	system(SystemCmd);
		strcpy(SystemCmd,""); // Ensure we don't re-execute it again
	   }
	   else
	   {
	   	f_write_string("/tmp/syscmd.log", "", 0, 0);

	   }
     }
//#ifdef U2EC
     else if (strcmp(name,"eject-usb.sh")==0)
     {
		eval("rmstorage");
     }
#ifdef ASUS_DDNS //2007.03.22 Yau add
     else if (strcmp(name,"ddnsclient")==0)
     {
		notify_rc("restart_ddns");
     }
     else if (strstr(name,"asusddns_register") != NULL)
     {
		notify_rc("asusddns_reg_domain");
     }
#endif
     else if (strcmp(name, "leases.sh")==0 || strcmp(name, "dleases.sh")==0)
     {
		// do nothing
     }
     else if (strstr(scmd, " ") == 0) // no parameter, run script with eval
     {
		eval(scmd);
     }
     else system(scmd);
}

void websScan(char_t *str)
{
	unsigned int i, flag;
	char_t *v1, *v2, *v3, *sp;
	char_t groupid[64];
	char_t value[MAX_LINE_SIZE];
	char_t name[MAX_LINE_SIZE];

	v1 = strchr(str, '?');

	i = 0;
	flag = 0;

	while (v1!=NULL)
	{
	    v2 = strchr(v1+1, '=');
	    v3 = strchr(v1+1, '&');

// 2008.08 magic {
		if (v2 == NULL)
			break;
// 2008.08 magic }

	    if (v3!=NULL)
	    {
	       strncpy(value, v2+1, v3-v2-1);
	       value[v3-v2-1] = 0;
	    }
	    else
	    {
	       strcpy(value, v2+1);
	    }

	    strncpy(name, v1+1, v2-v1-1);
	    name[v2-v1-1] = 0;
	    /*printf("Value: %s %s\n", name, value);*/

	    if (v2 != NULL && ((sp = strchr(v1+1, ' ')) == NULL || (sp > v2)))
	    {
	       if (flag && strncmp(v1+1, groupid, strlen(groupid))==0)
	       {
		   delMap[i] = atoi(value);
		   /*printf("Del Scan : %x\n", delMap[i]);*/
		   if (delMap[i]==-1)  break;
		   i++;
	       }
	       else if (strncmp(v1+1,"group_id", 8)==0)
	       {
		   sprintf(groupid, "%s_s", value);
		   flag = 1;
	       }
	    }
	    v1 = strchr(v1+1, '&');
	}
	delMap[i] = -1;
	return;
}


void websApply(webs_t wp, char_t *url)
{
#ifdef TRANSLATE_ON_FLY
	do_ej (url, wp);
	websDone (wp, 200);
#else   // define TRANSLATE_ON_FLY

     FILE *fp;
     char buf[MAX_LINE_SIZE];

     fp = fopen(url, "r");

     if (fp==NULL) return;

     while (fgets(buf, sizeof(buf), fp))
     {
	websWrite(wp, buf);
     }

     websDone(wp, 200);
     fclose(fp);
#endif
}


/*
 * Example:
 * lan_ipaddr=192.168.1.1
 * <% nvram_get("lan_ipaddr"); %> produces "192.168.1.1"
 * <% nvram_get("undefined"); %> produces ""
 */
static int
ej_nvram_get(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name, *c;
	int ret = 0;
//	char sid_dummy = "",

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (strcmp(name, "modem_spn") == 0 && !nvram_invmatch(name, ""))
		name = "modem_isp";

	for (c = nvram_safe_get(name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	return ret;
}


/* This will return properly encoded HTML entities - required
   for retrieving stored certs/keys.
*/

static int
ej_nvram_clean_get(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name, *c;
	int ret = 0;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	for (c = nvram_safe_get(name); *c; c++) {
		if (isprint(*c) &&
			*c != '"' && *c != '&' && *c != '<' && *c != '>')
				ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d;", *c);
	}

	return ret;
}


static int
ej_nvram_default_get(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name, *c;
	int ret = 0;
//	char sid_dummy = "",

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	for (c = nvram_default_safe_get(name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	return ret;
}

/*
 * Example:
 * lan_ipaddr=192.168.1.1
 * <% nvram_get_x("lan_ipaddr"); %> produces "192.168.1.1"
 * <% nvram_get_x("undefined"); %> produces ""
 */
static int
ej_nvram_get_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *c;
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s", &sid, &name) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	for (c = nvram_safe_get_x(sid, name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	return ret;
}

#ifdef ASUS_DDNS

static int
ej_nvram_get_ddns(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *c;
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s", &sid, &name) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	for (c = nvram_safe_get_x(sid, name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}
	if (strcmp(name,"ddns_return_code")==0) {
		if(!nvram_match("ddns_return_code", "ddns_query")) {
			nvram_set("ddns_return_code","");
		}
	}

	return ret;
}
#endif
/*
 * Example:
 * lan_ipaddr=192.168.1.1
 * <% nvram_get_x("lan_ipaddr"); %> produces "192.168.1.1"
 * <% nvram_get_x("undefined"); %> produces ""
 */
static int
ej_nvram_get_f(int eid, webs_t wp, int argc, char_t **argv)
{
	char *file, *field, *c, buf[64];
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s", &file, &field) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	strcpy(buf, nvram_safe_get_f(file, field));
	for (c = buf; *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	return ret;
}

static int
ej_nvram_show_chinese_char(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name, *c;
	int ret = 0;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	for (c = nvram_safe_get(name); *c; c++) {
		ret += websWrite(wp, "%c", *c);
	}

	return ret;
}

/*
 * Example:
 * wan_proto=dhcp
 * <% nvram_match("wan_proto", "dhcp", "selected"); %> produces "selected"
 * <% nvram_match("wan_proto", "static", "selected"); %> does not produce
 */
static int
ej_nvram_match(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name, *match, *output;

	if (ejArgs(argc, argv, "%s %s %s", &name, &match, &output) < 3) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (nvram_match(name, match))
	{
		return websWrite(wp, output);
	}

	return 0;
}

/*
 * Example:
 * wan_proto=dhcp
 * <% nvram_match("wan_proto", "dhcp", "selected"); %> produces "selected"
 * <% nvram_match("wan_proto", "static", "selected"); %> does not produce
 */
static int
ej_nvram_match_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *match, *output;

	if (ejArgs(argc, argv, "%s %s %s %s", &sid, &name, &match, &output) < 4) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (nvram_match_x(sid, name, match))
	{
		return websWrite(wp, output);
	}

	return 0;
}

static int
ej_nvram_double_match(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name, *match, *output;
	char *name2, *match2;

	if (ejArgs(argc, argv, "%s %s %s %s %s", &name, &match, &name2, &match2, &output) < 5) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (nvram_match(name, match) && nvram_match(name2, match2))
	{
		return websWrite(wp, output);
	}

	return 0;
}

static int
ej_nvram_double_match_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *match, *output;
	char *sid2, *name2, *match2;

	if (ejArgs(argc, argv, "%s %s %s %s %s %s %s", &sid, &name, &match, &sid2, &name2, &match2, &output) < 7) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (nvram_match_x(sid, name, match) && nvram_match_x(sid2, name2, match2))
	{
		return websWrite(wp, output);
	}

	return 0;
}

/*
 * Example:
 * wan_proto=dhcp
 * <% nvram_match("wan_proto", "dhcp", "selected"); %> produces "selected"
 * <% nvram_match("wan_proto", "static", "selected"); %> does not produce
 */
static int
ej_nvram_match_both_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *match, *output, *output_not;

	if (ejArgs(argc, argv, "%s %s %s %s %s", &sid, &name, &match, &output, &output_not) < 5)
	{
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (nvram_match_x(sid, name, match))
	{
		return websWrite(wp, output);
	}
	else
	{
		return websWrite(wp, output_not);
	}
}

/*
 * Example:
 * lan_ipaddr=192.168.1.1 192.168.39.248
 * <% nvram_get_list("lan_ipaddr", 0); %> produces "192.168.1.1"
 * <% nvram_get_list("lan_ipaddr", 1); %> produces "192.168.39.248"
 */
static int
ej_nvram_get_list_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name;
	int which;
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s %d", &sid, &name, &which) < 3) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	ret += websWrite(wp, nvram_get_list_x(sid, name, which));
	return ret;
}

/*
 * Example:
 * lan_ipaddr=192.168.1.1 192.168.39.248
 * <% nvram_get_list("lan_ipaddr", 0); %> produces "192.168.1.1"
 * <% nvram_get_list("lan_ipaddr", 1); %> produces "192.168.39.248"
 */
static int
ej_nvram_get_buf_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name;
	int which;

	if (ejArgs(argc, argv, "%s %s %d", &sid, &name, &which) < 3) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	return 0;
}


/*
 * Example:
 * wan_proto=dhcp;dns
 * <% nvram_match_list("wan_proto", "dhcp", "selected", 0); %> produces "selected"
 * <% nvram_match_list("wan_proto", "static", "selected", 1); %> does not produce
 */
static int
ej_nvram_match_list_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *match, *output;
	int which;

	if (ejArgs(argc, argv, "%s %s %s %s %d", &sid, &name, &match, &output, &which) < 5) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (nvram_match_list_x(sid, name, match, which))
		return websWrite(wp, output);
	else
		return 0;
}

static int
ej_select_channel(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, chstr[32];
	int ret = 0;
	int idx = 0, channel;
	char *value = nvram_safe_get("wl0_country_code");
	char *channel_s = nvram_safe_get("wl_channel");

	if (ejArgs(argc, argv, "%s", &sid) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	channel = (channel_s == NULL)? 0 : atoi(channel_s);

	for (idx = 0; idx < 12; idx++)
	{
		if (idx == 0)
			strcpy(chstr, "Auto");
		else
			sprintf(chstr, "%d", idx);
		ret += websWrite(wp, "<option value=\"%d\" %s>%s</option>", idx, (idx == channel)? "selected" : "", chstr);
	}

	if (    strcasecmp(value, "CA") && strcasecmp(value, "CO") && strcasecmp(value, "DO") &&
		strcasecmp(value, "GT") && strcasecmp(value, "MX") && strcasecmp(value, "NO") &&
		strcasecmp(value, "PA") && strcasecmp(value, "PR") && strcasecmp(value, "TW") &&
		strcasecmp(value, "US") && strcasecmp(value, "UZ") )
	{
		for (idx = 12; idx < 14; idx++)
		{
			sprintf(chstr, "%d", idx);
			ret += websWrite(wp, "<option value=\"%d\" %s>%s</option>", idx, (idx == channel)? "selected" : "", chstr);
		}
	}

	if ((strcmp(value, "") == 0) || (strcasecmp(value, "DB") == 0)/* || (strcasecmp(value, "JP") == 0)*/)
		ret += websWrite(wp, "<option value=\"14\" %s>14</option>", (14 == channel)? "selected" : "");

	return ret;
}

static int
ej_nvram_char_to_ascii(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name;
	char tmp[MAX_LINE_SIZE];
	char *buf = tmp, *str;
	int ret;

	if (ejArgs(argc, argv, "%s %s", &sid, &name) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	str = nvram_safe_get_x(sid, name);

	/* each char expands to %XX at max */
	ret = strlen(str) * sizeof(char)*3 + sizeof(char);
	if (ret > sizeof(tmp)) {
		buf = (char *)malloc(ret);
		if (buf == NULL) {
			csprintf("No memory.\n");
			return 0;
		}
	}

	char_to_ascii_safe(buf, str, ret);
	ret = websWrite(wp, "%s", buf);

	if (buf != tmp)
		free(buf);

	return ret;
}



/* Report sys up time */
static int
ej_uptime(int eid, webs_t wp, int argc, char_t **argv)
{

//	FILE *fp;
	char buf[MAX_LINE_SIZE];
//	unsigned long uptime;
	int ret;
	char *str = file2str("/proc/uptime");
	time_t tm;

	time(&tm);
	sprintf(buf, rfctime(&tm));

	if (str) {
		unsigned int up = atoi(str);
		free(str);
		char lease_buf[128];
		memset(lease_buf, 0, sizeof(lease_buf));
		reltime(up, lease_buf);
		sprintf(buf, "%s(%s since boot)", buf, lease_buf);
	}

	ret = websWrite(wp, buf);
	return ret;
}

static int
ej_sysuptime(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret=0;
	char *str = file2str("/proc/uptime");

	if (str) {
		unsigned int up = atoi(str);
		free(str);

		char lease_buf[128];
		memset(lease_buf, 0, sizeof(lease_buf));
		reltime(up, lease_buf);
		ret = websWrite(wp, "%s since boot", lease_buf);
	}

	return ret;
}

struct lease_t {
	unsigned char chaddr[16];
	u_int32_t yiaddr;
	u_int32_t expires;
	char hostname[64];
};


static int
ej_ddnsinfo(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret;

	ret = websWrite(wp, "[\"%s\", \"%s\", \"%s\", \"%s\"]",
		nvram_safe_get("ddns_enable_x"),
		nvram_safe_get("ddns_server_x"),
		nvram_safe_get("ddns_hostname_x"),
		nvram_safe_get("ddns_return_code")
		);

	return ret;
}

int
websWriteCh(webs_t wp, char *ch, int count)
{
   int i, ret;

   ret = 0;
   for (i=0; i<count; i++)
      ret+=websWrite(wp, "%s", ch);
   return (ret);
}

static int dump_file(webs_t wp, char *filename)
{
	FILE *fp;
	char buf[MAX_LINE_SIZE];
	int ret=0;

	fp = fopen(filename, "r");

	if (fp==NULL)
	{
		ret+=websWrite(wp, "%s", "");
		return (ret);
	}

	ret = 0;

	while (fgets(buf, MAX_LINE_SIZE, fp)!=NULL)
	{
	    int len;
	    len = strlen(buf); // fgets() would fill the '\0' at the last character in buffer.
	    ret += websWriteData(wp, buf, len);
	}

	fclose(fp);

	return (ret);
}

extern int wl_wps_info(int eid, webs_t wp, int argc, char_t **argv, int unit);

static int
ej_dump(int eid, webs_t wp, int argc, char_t **argv)
{
//	FILE *fp;
//	char buf[MAX_LINE_SIZE];
	char filename[PATH_MAX], path[PATH_MAX];
	char *file,*script;
	int ret;

	if (ejArgs(argc, argv, "%s %s", &file, &script) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	//csprintf("Script : %s, File: %s\n", script, file);

	// run scrip first to update some status
	if (strcmp(script,"")!=0) sys_script(script);

	if (strcmp(file, "wlan11b.log")==0)
		return (ej_wl_status(eid, wp, 0, NULL, 0));	/* FIXME */
	else if (strcmp(file, "wlan11b_2g.log")==0)
		return (ej_wl_status_2g(eid, wp, 0, NULL));
#if 0
	else if (strcmp(file, "leases.log")==0)
		return (ej_lan_leases(eid, wp, 0, NULL));
#ifdef RTCONFIG_IPV6
	else if (strcmp(file, "ipv6_network.log")==0)
		return (ej_lan_ipv6_network(eid, wp, 0, NULL));
#endif
	else if (strcmp(file, "iptable.log")==0)
		return (get_nat_vserver_table(eid, wp, 0, NULL));
	else if (strcmp(file, "route.log")==0)
		return (ej_route_table(eid, wp, 0, NULL));
#endif
	else if (strcmp(file, "wps_info.log")==0)
	{
#ifndef RTAC3200
		if (nvram_match("wps_band", "0"))
			return (ej_wps_info_2g(eid, wp, 0, NULL));
		else
			return (ej_wps_info(eid, wp, 0, NULL));
#else
		return wl_wps_info(eid, wp, argc, argv, nvram_get_int("wps_band"));
#endif
	}
#if 0
	else if (strcmp(file, "apselect.log")==0)
		return (ej_getSiteSurvey(eid, wp, 0, NULL));
	else if (strcmp(file, "apscan")==0)
		return (ej_SiteSurvey(eid, wp, 0, NULL));
	else if (strcmp(file, "urelease")==0)
		return (ej_urelease(eid, wp, 0, NULL));
#endif

	ret = 0;

	strcpy(path, get_logfile_path());
	if (strcmp(file, "syslog.log")==0)
	{
		sprintf(filename, "%s/%s-1", path, file);
		ret += dump_file(wp, filename);
		sprintf(filename, "%s/%s", path, file);
		ret += dump_file(wp, filename);
	}
//#ifdef RTCONFIG_CLOUDSYNC
	else if(!strcmp(file, "cloudsync.log")){
		sprintf(filename, "/tmp/smartsync/.logs/system.log");
		ret += dump_file(wp, filename);
		sprintf(filename, "/tmp/%s", file);
		ret += dump_file(wp, filename);
	}
	else if(!strcmp(file, "clouddisk.log")){
		sprintf(filename, "/tmp/lighttpd/syslog.log");
		ret += dump_file(wp, filename);
		sprintf(filename, "/tmp/%s", file);
		ret += dump_file(wp, filename);
	}
//#endif
#ifdef RTCONFIG_OPENVPN
	else if(!strcmp(file, "openvpn_connected")){
		int unit = nvram_get_int("vpn_server_unit");
		parse_openvpn_status(unit);
		sprintf(filename, "/etc/openvpn/server%d/client_status", unit);
		ret += dump_file(wp, filename);
	}
#endif
#ifdef RTCONFIG_PUSH_EMAIL
#ifdef RTCONFIG_DSL
	else if(!strcmp(file, "fb_fail_content")){
		sprintf(filename, "/tmp/xdslissuestracking");
		if(check_if_file_exist(filename)) {
			eval("sed", "-i", "/PIN Code:/d", filename);
			eval("sed", "-i", "/MAC Address:/d", filename);
			eval("sed", "-i", "/E-mail:/d", filename);
			eval("sed", "-i", "/Download Master:/d", filename);
			eval("sed", "-i", "/Cloud Disk:/d", filename);
			eval("sed", "-i", "/Smart Access:/d", filename);
			eval("sed", "-i", "/Smart Sync:/d", filename);
			eval("sed", "-i", "/Guest Network 1\\/2\\/3 \\(.*\\):/d", filename);
			eval("sed", "-i", "/Current connected Clients:/d", filename);
			eval("sed", "-i", "/CC\\(.*\\)\\/CC\\(.*\\)\\/TC:/d", filename);
			eval("sed", "-i", "/regrev\\(.*\\)\\/regrev\\(.*\\):/d", filename);
			ret += dump_file(wp, filename);
			unlink(filename);
		}
		else {
			char buf[4096] = {0};
			snprintf(buf, 4095,
				"Your ISP / Internet Service Provider: %s\n"
				"Name of the Subscribed Plan/Service/Package:  %s\n"
				"DSL service performance option: %s\n"
				"Firmware version: %s.%s_%s\n"
				"DSL Firmware version: %s\n"
				"&nbsp;\n"
				"Comments / Suggestions:\n"
				"%s\n"
				, nvram_safe_get("fb_ISP")
				, nvram_safe_get("fb_Subscribed_Info")
				, nvram_safe_get("fb_availability")
				, nvram_safe_get("firmver"), nvram_safe_get("buildno"), nvram_safe_get("extendno")
				, nvram_safe_get("dsllog_fwver")
				, nvram_safe_get("fb_comment")
				);
			ret += websWrite(wp, buf);
		}
	}
#else /* RTCONFIG_DSL */
	else if(!strcmp(file, "fb_fail_content")){
		sprintf(filename, "/tmp/xdslissuestracking");
		if(check_if_file_exist(filename)) {
			eval("sed", "-i", "/PIN Code:/d", filename);
			eval("sed", "-i", "/MAC Address:/d", filename);
			eval("sed", "-i", "/E-mail:/d", filename);
			eval("sed", "-i", "/Download Master:/d", filename);
			eval("sed", "-i", "/Cloud Disk:/d", filename);
			eval("sed", "-i", "/Smart Access:/d", filename);
			eval("sed", "-i", "/Smart Sync:/d", filename);
			eval("sed", "-i", "/Guest Network 1\\/2\\/3 \\(.*\\):/d", filename);
			eval("sed", "-i", "/Current connected Clients:/d", filename);
			eval("sed", "-i", "/CC\\(.*\\)\\/CC\\(.*\\)\\/TC:/d", filename);
			eval("sed", "-i", "/regrev\\(.*\\)\\/regrev\\(.*\\):/d", filename);
			ret += dump_file(wp, filename);
			unlink(filename);
		}
		else {
			char buf[4096] = {0};
			snprintf(buf, 4095,
				"Feedback problem type: %s\n"
				"Feedback problem description:  %s\n"
				"Firmware version: %s.%s_%s\n"
				"&nbsp;\n"
				"Comments / Suggestions:\n"
				"%s\n"
				, nvram_safe_get("fb_ptype")
				, nvram_safe_get("fb_pdesc")
				, nvram_safe_get("firmver"), nvram_safe_get("buildno"), nvram_safe_get("extendno")
				, nvram_safe_get("fb_comment")
				);
			ret += websWrite(wp, buf);
		}
	}
#endif /* RTCONFIG_DSL */
#endif /* RTCONFIG_PUSH_EMAIL */
	else {
		sprintf(filename, "/tmp/%s", file);
		ret += dump_file(wp, filename);
	}

	return ret;
}

static int
ej_load(int eid, webs_t wp, int argc, char_t **argv)
{
	char *script;

	if (ejArgs(argc, argv, "%s", &script) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	sys_script(script);
	return (websWrite(wp,"%s",""));
}

/*
 * retreive and convert wl values for specified wl_unit
 * Example:
 * <% wl_get_parameter(); %> for coping wl[n]_ to wl_
 */

static int
ej_wl_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit, subunit;

	unit = nvram_get_int("wl_unit");
	subunit = nvram_get_int("wl_subunit");

	// handle generate cases first
	(void)copy_index_to_unindex("wl_", unit, subunit);

	return (websWrite(wp,"%s",""));
}

int webWriteNvram(webs_t wp, char *name)
{
	char *c;
	int ret = 0;

	for (c = nvram_safe_get(name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>' && *c != '\\' && *c != '%')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "%%%02X", *c);
	}

	return ret;
}

int webWriteNvram2(webs_t wp, char *name)
{
	char *c;
	int ret = 0;

	for (c = nvram_safe_get(name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>' && *c != '\\' && *c != '%')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	return ret;
}

/*
 * retreive guest network releated wl values
 */

static int
ej_wl_get_guestnetwork(int eid, webs_t wp, int argc, char_t **argv)
{
	char word2[128], tmp[128], *next2;
	char *unitname;
	char prefix[32];
	int  unit, subunit;
	int ret = 0;

	if (ejArgs(argc, argv, "%s", &unitname) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	unit = atoi(unitname);
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	ret += websWrite(wp, "[");

	subunit = 0;
	foreach(word2, nvram_safe_get(strcat_r(prefix, "vifnames", tmp)), next2) {
		if(subunit>0) websWrite(wp, ", ");

		subunit++;

		ret += websWrite(wp, "[\"");
		ret += webWriteNvram(wp, strcat_r(word2, "_bss_enabled", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_ssid", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_auth_mode_x", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_crypto", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_wpa_psk", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_wep_x", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_key", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_key1", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_key2", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_key3", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_key4", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_expire", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_lanaccess", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_expire_tmp", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_macmode", tmp));	// gn_array[][14]
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_mbss", tmp));	// gn_array[][15]
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram2(wp, strcat_r(word2, "_maclist_x", tmp));	// gn_array[][16]
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram2(wp, strcat_r(word2, "_phrase_x", tmp));	// gn_array[][17]
		ret += websWrite(wp, "\"]");
	}
	ret += websWrite(wp, "]");
	return ret;
}


/*
 * retreive and convert wan values for specified wan_unit
 * Example:
 * <% wan_get_parameter(); %> for coping wan[n]_ to wan_
 */

static int
ej_wan_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit;

	unit = nvram_get_int("wan_unit");

	// handle generate cases first
	(void)copy_index_to_unindex("wan_", unit, -1);

	return (websWrite(wp,"%s",""));
}


/*

 * retreive and convert lan values for specified lan_unit
 * Example:
 * <% lan_get_parameter(); %> for coping lan[n]_ to lan_
 */

static int
ej_lan_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit;

	unit = nvram_get_int("lan_unit");

	// handle generate cases first
	(void)copy_index_to_unindex("lan_", unit, -1);

	return (websWrite(wp,"%s",""));
}

#ifdef RTCONFIG_OPENVPN
static int
ej_vpn_server_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit;

	unit = nvram_get_int("vpn_server_unit");
	// handle generate cases first
	(void)copy_index_to_unindex("vpn_server_", unit, -1);

	return (websWrite(wp,"%s",""));
}

static int
ej_vpn_client_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit;

	unit = nvram_get_int("vpn_client_unit");
	// handle generate cases first
	(void)copy_index_to_unindex("vpn_client_", unit, -1);

	return (websWrite(wp,"%s",""));
}
static int 
ej_vpn_crt_server(int eid, webs_t wp, int argc, char **argv) {
	char buf[4000];
	char file_name[32];
	int idx = 0;
	
	for (idx = 1; idx < 3; idx++) {
		char *c;

		//vpn_crt_server_ca
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_server%d_ca", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
				websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		//vpn_crt_server_crt
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_server%d_crt", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
				websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		//vpn_crt_server_key
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_server%d_key", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
				websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		//vpn_crt_server_dh
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_server%d_dh", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
				websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		//vpn_crt_server_crl
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_server%d_crl", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
				websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		//vpn_crt_server_static
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_server%d_static", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
				websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		//vpn_crt_server_extra
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_server%d_extra", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
			websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		websWrite(wp, "\n");
	}
	return 0;
}
static int 
ej_vpn_crt_client(int eid, webs_t wp, int argc, char **argv) {
	char buf[4000];
	char file_name[32];
	int idx = 0;
	
	for (idx = 1; idx < 6; idx++) {
		char *c;

		//vpn_crt_client_ca
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_client%d_ca", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
				websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		//vpn_crt_client_crt
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_client%d_crt", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
				websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		//vpn_crt_client_key
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_client%d_key", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
				websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		//vpn_crt_client_static
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_client%d_static", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
				websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		//vpn_crt_client_crl
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_client%d_crl", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
				websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		//vpn_crt_client_extra
		memset(buf, 0, sizeof(buf));
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "vpn_crt_client%d_extra", idx);
		get_parsed_crt(file_name, buf, sizeof(buf));
		websWrite(wp, "%s=['", file_name);
		for (c = buf; *c; c++) {
			if (isprint(*c) &&
				*c != '"' && *c != '&' && *c != '<' && *c != '>')
					websWrite(wp, "%c", *c);
			else
				websWrite(wp, "&#%d", *c);
		}
		websWrite(wp, "'];\n");

		websWrite(wp, "\n");
	}
	return 0;
}
#endif


//2008.08 magic {
// Largest POST will be the OpenVPN server page with keys:
// 7 key/certs, each with up to 3500 characters, for a potential
// size of 25KB.  Add other fields + policy lists, 40KB should be safe enough.

static char post_buf[40000] = { 0 };
static char post_buf_backup[40000] = { 0 };
static char post_json_buf[65535] = { 0 };

static void do_html_post_and_get(char *url, FILE *stream, int len, char *boundary){
	char *query = NULL;

	init_cgi(NULL);

	memset(post_buf, 0, sizeof(post_buf));
	memset(post_buf_backup, 0, sizeof(post_buf));
	memset(post_json_buf, 0, sizeof(post_json_buf));

	if (fgets(post_buf, MIN(len+1, sizeof(post_buf)), stream)){
		len -= strlen(post_buf);

		while (len--)
			(void)fgetc(stream);
	}

	sprintf(post_json_buf, "%s", post_buf);
	query = url;
	strsep(&query, "?");

	if (query && strlen(query) > 0){
		if (strlen(post_buf) > 0)
			sprintf(post_buf_backup, "?%s&%s", post_buf, query);
		else
			sprintf(post_buf_backup, "?%s", query);

		sprintf(post_buf, "%s", post_buf_backup+1);
	}
	else if (strlen(post_buf) > 0)
		sprintf(post_buf_backup, "?%s", post_buf);

	//websScan(post_buf_backup);
	init_cgi(post_buf);
}

extern struct nvram_tuple router_defaults[];
extern struct nvram_tuple router_state_defaults[];

// used for handling multiple instance
// copy nvram from, ex wl0_ to wl_
// prefix is wl_, wan_, or other function with multiple instance
// [prefix]_unit and [prefix]_subunit must exist
void copy_index_to_unindex(char *prefix, int unit, int subunit)
{
	struct nvram_tuple *t;
	char *value;
	char name[64], unitname[64], unitptr[32];
	char tmp[64], unitprefix[32];

	// check if unit exist
	if(unit == -1) return;
	snprintf(unitptr, sizeof(unitptr), "%sunit", prefix);
	if((value=nvram_get(unitptr))==NULL) return;

	strncpy(tmp, prefix, sizeof(tmp));
	tmp[strlen(prefix)-1]=0;
	if(subunit==-1||subunit==0)
		snprintf(unitprefix, sizeof(unitprefix), "%s%d_", tmp, unit);
	else snprintf(unitprefix, sizeof(unitprefix), "%s%d.%d_", tmp, unit, subunit);

	/* go through each nvram value */
	for (t = router_defaults; t->name; t++)
	{
		memset(name, 0, 64);
		sprintf(name, "%s", t->name);

		// exception here
		if(strcmp(name, unitptr)==0) continue;
		if(!strcmp(name, "wan_primary")) continue;

		if(!strncmp(name, prefix, strlen(prefix)))
		{
			(void)strcat_r(unitprefix, &name[strlen(prefix)], unitname);

			if((value=nvram_get(unitname))!=NULL)
			{
				nvram_set(name, value);
			}
		}
	}
}

#ifdef RTCONFIG_DUALWAN
void save_index_to_interface(){//Cherry Cho added for exchanging settings of dualwan in 2014/10/20.
	int i;
	char word[80], *next;
	char tmp[64], prefix[32], unitprefix[32], name[64], unitname[64];
	char *wans_dualwan = nvram_get("wans_dualwan");		
	char *wan_prefix = "wan_";
	struct nvram_tuple *t;
	char *value;	

	i = 0;
	foreach(word, wans_dualwan, next) {
		memset(prefix, 0, sizeof(prefix));
		memset(unitprefix, 0, sizeof(unitprefix));		
		snprintf(prefix, sizeof(prefix), "%s_", word);	
		snprintf(unitprefix, sizeof(unitprefix), "wan%d_", i);				

		/* go through each nvram value */					
		for (t = router_defaults; t->name; t++)
		{
			memset(name, 0, 64);
			sprintf(name, "%s", t->name);

			if(!strcmp(name, "wan_unit")) continue;
			if(!strcmp(name, "wan_primary")) continue;

			if(!strncmp(name, wan_prefix, strlen(wan_prefix))){
				memset(tmp, 0, sizeof(tmp));
				memset(unitname, 0, sizeof(unitname));
				(void)strcat_r(prefix, name, tmp);	
				(void)strcat_r(unitprefix, &name[strlen(wan_prefix)], unitname);
				if((value = nvram_get(unitname)) != NULL){
					nvram_set(tmp, value);
				}	
			}
		}
		i++;
	}	
}


void save_interface_to_index(char *wans_dualwan){
	int i;
	char word[80], *next;
	char tmp[64], prefix[32], unitprefix[32], name[64], unitname[64];	
	char *wan_prefix = "wan_";
	struct nvram_tuple *t;
	char *value;	

	i = 0;
	foreach(word, wans_dualwan, next) {
		memset(prefix, 0, sizeof(prefix));
		memset(unitprefix, 0, sizeof(unitprefix));		
		snprintf(prefix, sizeof(prefix), "%s_", word);	
		snprintf(unitprefix, sizeof(unitprefix), "wan%d_", i);				

		/* go through each nvram value */					
		for (t = router_defaults; t->name; t++)
		{
			memset(name, 0, 64);
			sprintf(name, "%s", t->name);

			if(!strcmp(name, "wan_unit")) continue;
			if(!strcmp(name, "wan_primary")) continue;

			if(!strncmp(name, wan_prefix, strlen(wan_prefix))){
				memset(tmp, 0, sizeof(tmp));
				memset(unitname, 0, sizeof(unitname));
				(void)strcat_r(prefix, name, tmp);	
				(void)strcat_r(unitprefix, &name[strlen(wan_prefix)], unitname);
				if((value = nvram_get(tmp)) != NULL){
					nvram_set(unitname, value);
				}	
			}
		}
		i++;	
	}
}	
#endif

#ifdef RTCONFIG_JFFS2USERICON
void handle_upload_icon(char *value) {
	char *mac, *uploadicon;
	char filename[32];
	memset(filename, 0, 32);
	
	//Check folder exist or not
	if(!check_if_dir_exist(JFFS_USERICON))
		mkdir(JFFS_USERICON, 0755);

	if((vstrsep(value, ">", &mac, &uploadicon) == 2)) {	
		sprintf(filename, "/jffs/usericon/%s.log", mac);
		
		//Delete exist file
		if(check_if_file_exist(filename)) {
			unlink(filename);
		}
		//If upload icon string is not noupload, then write to file.
		if(strcmp(uploadicon, "noupload")) {
			FILE *fp;
			if((fp = fopen(filename, "w")) != NULL) {
				fprintf(fp, "%s", uploadicon);
				fclose(fp);
			}
		}
	}
}
void del_upload_icon(char *value) {
	char *buf, *g, *p;
	char filename[32];
	memset(filename, 0, 32);

	g = buf = strdup(value);
	while (buf) {
		if ((p = strsep(&g, ">")) == NULL) break;

		if(strcmp(p, "")) {
			sprintf(filename, "/jffs/usericon/%s.log", p);
			//Delete exist file
			if(check_if_file_exist(filename)) {
				unlink(filename);
			}
		}
	}
	free(buf);
}
#endif

#define NVRAM_MODIFIED_BIT		1
#define NVRAM_MODIFIED_WL_BIT		2
#ifdef RTCONFIG_QTN
#define NVRAM_MODIFIED_WL_QTN_BIT	4
#endif

int validate_instance(webs_t wp, char *name, json_object *root)
{
	char prefix[32], word[100], tmp[100], *next, *value;
	char prefix1[32], word1[100], *next1;
	int i=0; /*, j=0;*/
	int found = 0;

	// handle instance for wlx, wanx, lanx
	if(strncmp(name, "wl", 2)==0) {
		foreach(word, nvram_safe_get("wl_ifnames"), next) {
			sprintf(prefix, "wl%d_", i++);
			value = get_cgi_json(strcat_r(prefix, name+3, tmp),root);
			if(!value) {
				// find variable with subunit
				foreach(word1, nvram_safe_get(strcat_r(prefix, "vifnames", tmp)), next1) {
					sprintf(prefix1, "%s_", word1);
					value = get_cgi_json(strcat_r(prefix1, name+3, tmp),root);
					//printf("find %s\n", tmp);

					if(value) break;
				}
			}

			if(value && strcmp(nvram_safe_get(tmp), value)) {
				//printf("instance value %s=%s\n", tmp, value);
				dbG("nvram set %s = %s\n", tmp, value);
				nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT|NVRAM_MODIFIED_WL_BIT;
#ifdef RTCONFIG_QTN
				if (!strncmp(tmp, "wl1", 3))
				{
					if (rpc_qtn_ready())
					{
						rpc_parse_nvram(tmp, value);
						found |= NVRAM_MODIFIED_WL_QTN_BIT;
					}
				}
#endif
			}
		}
	}
	else if(strncmp(name, "wan", 3)==0) {
		foreach(word, nvram_safe_get("wan_ifnames"), next) {
			sprintf(prefix, "wan%d_", i++);
			value = get_cgi_json(strcat_r(prefix, name+4, tmp),root);
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				dbG("nvram set %s = %s\n", tmp, value);
				nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT;
			}
		}
	}
#ifdef RTCONFIG_DSL
	else if(strncmp(name, "dsl", 3)==0) {
		for(i=0;i<8;i++) {
			sprintf(prefix, "dsl%d_", i++);
			value = get_cgi_json(strcat_r(prefix, name+4, tmp),root);
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				dbG("nvram set %s = %s\n", tmp, value);
				nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT;
			}
		}
#ifdef RTCONFIG_VDSL
		for(i=0;i<8;i++) {
			if(i)
				snprintf(prefix, sizeof(prefix), "dsl8.%d_", i);
			else
				snprintf(prefix, sizeof(prefix), "dsl8_");
			value = get_cgi_json(strcat_r(prefix, name+4, tmp),root);
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				dbG("nvram set %s = %s\n", tmp, value);
				nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT;
			}
		}
#endif
	}
#endif
	else if(strncmp(name, "lan", 3)==0) {
	}
// This seems to create default values for each unit.
// Is it really necessary?  lan_ does not seem to use it.
#ifdef RTCONFIG_OPENVPN
	else if(strncmp(name, "vpn_server_", 11)==0) {
		for(i=1;i<3;i++) {
			sprintf(prefix, "vpn_server%d_", i);
			value = get_cgi_json(strcat_r(prefix, name+11, tmp),root);
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				dbG("nvram set %s = %s\n", tmp, value);
				nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT;
			}
		}
	}
	else if(strncmp(name, "vpn_client_", 11)==0) {
		for(i=1;i<6;i++) {
			sprintf(prefix, "vpn_client%d_", i);
			value = get_cgi_json(strcat_r(prefix, name+11, tmp),root);
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				dbG("nvram set %s = %s\n", tmp, value);
				nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT;
			}
		}
	}
#endif

	return found;
}

static int validate_apply(webs_t wp, json_object *root) {
	struct nvram_tuple *t;
	char *value;
	char name[64];
	char tmp[3500], prefix[32];
	int unit=-1, subunit=-1;
	int nvram_modified = 0;
	int nvram_modified_wl = 0;
	int acc_modified = 0;
	int ret;
#ifdef RTCONFIG_USB
#if defined(RTCONFIG_USB_MODEM) && (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
	int got_modem_data = 0;
#endif
	char orig_acc[128], modified_acc[128], modified_pass[128];

	memset(orig_acc, 0, 128);
	memset(modified_acc, 0, 128);
	memset(modified_pass, 0, 128);
#endif

	/* go through each nvram value */
	for (t = router_defaults; t->name; t++)
	{
		snprintf(name, sizeof(name), t->name);

		value = get_cgi_json(name, root);

		if(!value) {
			if((ret=validate_instance(wp, name,root))) {
				if(ret&NVRAM_MODIFIED_BIT) nvram_modified = 1;
				if(ret&NVRAM_MODIFIED_WL_BIT) nvram_modified_wl = 1;
			}
		}
		else {
#ifdef RTCONFIG_JFFS2USERICON
			if(strcmp(name, "custom_usericon"))
#endif
			_dprintf("value %s=%s\n", name, value);

			// unit nvram should be in fron of each apply,
			// seems not a good design

			if(!strcmp(name, "wl_unit")
					|| !strcmp(name, "wan_unit")
					|| !strcmp(name, "lan_unit")
#ifdef RTCONFIG_DSL
					|| !strcmp(name, "dsl_unit")
#endif
#ifdef RTCONFIG_OPENVPN
					|| !strcmp(name, "vpn_server_unit")
					|| !strcmp(name, "vpn_client_unit")
#endif
					) {
				unit = atoi(value);
				if(unit != nvram_get_int(name)) {
					nvram_set_int(name, unit);
					nvram_modified=1;
				}
			}
			else if(!strcmp(name, "wl_subunit")) {
				subunit = atoi(value);
				if(subunit!=nvram_get_int(name)) {
					nvram_set_int(name, subunit);
					nvram_modified=1;
				}
			}
			else if(!strncmp(name, "wl_", 3) && unit != -1) {
				// convert wl_ to wl[unit], only when wl_unit is parsed
				if(subunit==-1||subunit==0)
					snprintf(prefix, sizeof(prefix), "wl%d_", unit);
				else snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
				(void)strcat_r(prefix, name+3, tmp);
				if(strcmp(nvram_safe_get(tmp), value))
				{
					nvram_set(tmp, value);
					nvram_modified = 1;
					nvram_modified_wl = 1;
					_dprintf("set %s=%s\n", tmp, value);

#ifdef RTCONFIG_QTN
					if (unit == 1)
					{
						if (rpc_qtn_ready())
						{
							rpc_parse_nvram(tmp, value);
						}
					}
#endif
				}
			}
			else if(!strncmp(name, "wan_", 4) && unit != -1) {
				snprintf(prefix, sizeof(prefix), "wan%d_", unit);
				(void)strcat_r(prefix, name+4, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}					
			}
			else if(!strncmp(name, "lan_", 4) && unit != -1) {
				snprintf(prefix, sizeof(prefix), "lan%d_", unit);
				(void)strcat_r(prefix, name+4, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
#ifdef RTCONFIG_DSL
			else if(!strcmp(name, "dsl_subunit")) {
				subunit = atoi(value);
				if(subunit!=nvram_get_int(name)) {
					nvram_set_int(name, subunit);
					nvram_modified=1;
				}
			}
			else if(!strncmp(name, "dsl_", 4) && unit != -1) {
				if(subunit==-1||subunit==0)
					snprintf(prefix, sizeof(prefix), "dsl%d_", unit);
				else
					snprintf(prefix, sizeof(prefix), "dsl%d.%d_", unit, subunit);
				(void)strcat_r(prefix, name+4, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
#endif
#ifdef RTCONFIG_OPENVPN
			else if(!strncmp(name, "vpn_server_", 11) && unit!=-1) {
				snprintf(prefix, sizeof(prefix), "vpn_server%d_", unit);
				(void)strcat_r(prefix, name+11, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
			else if(!strncmp(name, "vpn_client_", 11) && unit!=-1) {
				snprintf(prefix, sizeof(prefix), "vpn_client%d_", unit);
				(void)strcat_r(prefix, name+11, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
			else if(!strncmp(name, "vpn_crt", 7)) {
				nvram_set(name, value);			// save to nvram
				get_parsed_crt(name, tmp, sizeof (tmp));// then migrate to jffs
				_dprintf("set %s=%s'n", name, value);
			}
#endif
			else if(!strncmp(name, "sshd_", 5)) {
				write_encoded_crt(name, value);
				nvram_modified = 1;
				_dprintf("set %s=%s\n", name, value);
			}
#ifdef RTCONFIG_DISK_MONITOR
			else if(!strncmp(name, "diskmon_", 8)) {
				snprintf(prefix, sizeof(prefix), "usb_path%s_diskmon_", nvram_safe_get("diskmon_usbport"));
				(void)strcat_r(prefix, name+8, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
#endif
#ifdef RTCONFIG_JFFS2USERICON
			else if(!strcmp(name, "custom_usericon")) {
				(void)handle_upload_icon(value);
				nvram_set(name, "");
				nvram_modified = 1;
			}
			else if(!strcmp(name, "custom_usericon_del")) {
				(void)del_upload_icon(value);
				nvram_set(name, "");
				nvram_modified = 1;
			}
#endif
			// TODO: add other multiple instance handle here
			else if(strcmp(nvram_safe_get(name), value)) {

				// the flag is set only when username or password is changed
				if(!strcmp(t->name, "http_username")
						|| !strcmp(t->name, "http_passwd")){
#ifdef RTCONFIG_USB
					if(!strcmp(t->name, "http_username")){
						strncpy(orig_acc, nvram_safe_get(name), 128);
						strncpy(modified_acc, value, 128);
					}
					else if(!strcmp(t->name, "http_passwd"))
						strncpy(modified_pass, value, 128);

#endif

					acc_modified = 1;
					change_passwd = 1;
				}

#ifdef RTCONFIG_HTTPS
				if(!strcmp(name, "PM_SMTP_AUTH_PASS")){
					_dprintf("PM_SMTP_AUTH_PASS match\n");
					char pw_tmp[256];
					memset(pw_tmp, 0, 256);
					strncpy(pw_tmp, value,256);
					strncpy(value, (char *) pwenc(pw_tmp),256);
				}
#endif

#ifdef RTCONFIG_DUALWAN//Cherry Cho added for exchanging settings of dualwan in 2014/10/20.
				if(!strcmp(name, "wans_dualwan")){
					save_index_to_interface();
					save_interface_to_index(value);
				}
#endif

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
				if(!strcmp(name, "modem_bytes_data_cycle") || !strcmp(name, "modem_bytes_data_limit") || !strcmp(name, "modem_bytes_data_warning")){
					notify_rc("restart_set_dataset");
				}
#endif				

				nvram_set(name, value);
				nvram_modified = 1;
				_dprintf("set %s=%s\n", name, value);

#if defined(RTCONFIG_USB_MODEM) && (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
				if(!strncmp(name, "modem_bytes_data", 16)){
					got_modem_data = 1;
				}
#endif
			}
		}
	}

	if(acc_modified){
		// ugly solution?
		notify_rc("chpass");

#ifdef RTCONFIG_USB
		if(strlen(orig_acc) <= 0)
			strncpy(orig_acc, nvram_safe_get("http_username"), 128);
		if(strlen(modified_pass) <= 0)
			strncpy(modified_pass, nvram_safe_get("http_passwd"), 128);

		if(strlen(modified_acc) <= 0)
			mod_account(orig_acc, NULL, modified_pass);
		else
			mod_account(orig_acc, modified_acc, modified_pass);

		notify_rc_and_wait("restart_ftpsamba");
#endif
	}

#if defined(RTCONFIG_USB_MODEM) && (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
	if(got_modem_data){
		eval("modem_status.sh", "set_dataset");
	}
#endif

	/* go through each temp nvram value */
	/* but not support instance now */
	for (t = router_state_defaults; t->name; t++)
	{
		// skip some unhandled variables
		if(strcmp(t->name, "rc_service")==0)
			continue;

		memset(name, 0, 64);
		sprintf(name, "%s", t->name);

		value = websGetVar(wp, name, NULL);

		if(value) {
			if(strcmp(nvram_safe_get(name), value)) {
				nvram_set(name, value);
			}
		}
	}

	if(nvram_modified)
	{
#ifdef RTCONFIG_TR069
		if(pids("tr069")) {
			_dprintf("value change from web!\n");
			eval("sendtocli", "http://127.0.0.1:1234/web/value/change", "\"name=change\"");
		}
#endif
		// TODO: is it necessary to separate the different?
		if(nvram_match("x_Setting", "0")){
			nvram_set("x_Setting", "1");
			if(nvram_match("productid", "4G-AC55U") && nvram_match("wans_mode", "lb")){//Cherry Cho added in 2014/10/03.
				char *current_page;
				current_page = websGetVar(wp, "current_page", NULL);
				if(!strstr(current_page, "QIS_"))
					nvram_set("wans_mode", "fo");
			}
		}

		if (nvram_modified_wl)
			nvram_set("w_Setting", "1");
		nvram_commit();
	}

	return nvram_modified;
}

bool isIP(char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
}

bool isMAC(const char* mac) {
    int i = 0;
    int s = 0;

    while (*mac) {
       if (isxdigit(*mac)) {
          i++;
       }
       else if (*mac == ':') {
          if (i == 0 || i / 2 - 1 != s)
            break;
          ++s;
       }
       else {
           s = -1;
       }
       ++mac;
    }
    return (i == 12 && s == 5);
}

bool isNumber(const char*s) {
   char* e = NULL;
   (void) strtol(s, &e, 0);
   return e != NULL && *e == (char)0;
}

#ifdef RTCONFIG_ROG
#define ApiMaxNumRegister 32
#define ApiMaxNumPortforward 32
#define ApiMaxNumQos 32
static int ej_set_variables(int eid, webs_t wp, int argc, char_t **argv) {
	char *apiName;
	char *apiAction;
	char *webVar_1;
	char *webVar_2;
	char *webVar_3;
	char *webVar_4;
	char *webVar_5;
	char *webVar_6;
	int retStatus = 0;
	char retList[65536]={0};
	char *buf, *g, *p;
	char nvramTmp[4096]={0}, strTmp[4096]={0};
	int iCurrentListNum = 0;

	apiName = websGetVar(wp, "apiname", "");
	apiAction = websGetVar(wp, "action", "");
	_dprintf("[httpd] api.asp: apiname->%s, apiAction->%s\n", apiName, apiAction);

	if(!strcmp(apiName, "register")){
		char *name, *mac, *group, *type, *callback, *keeparp;

		if(!strcmp(apiAction, "add")){
			webVar_1 = websGetVar(wp, "name", "");
			webVar_2 = websGetVar(wp, "mac", "");
			webVar_3 = websGetVar(wp, "group", "");
			webVar_4 = websGetVar(wp, "type", "");
			webVar_5 = websGetVar(wp, "callback", "");
			webVar_6 = websGetVar(wp, "keeparp", "");

			if(!strcmp(webVar_1, "") || !strcmp(webVar_2, "") ||
			   !strcmp(webVar_3, "") || !strcmp(webVar_4, "") ){
				retStatus = 3;
			}
			else if(strlen(webVar_1) > 32){
				retStatus = 3;
			}
			else if(!isMAC(webVar_2)){
				retStatus = 3;
			}
			else if(!isNumber(webVar_3)){
				retStatus = 3;
			}
			else if(!isNumber(webVar_4)){
				retStatus = 3;
			}
                        else if(!isNumber(webVar_5)){
                                retStatus = 3;
                        }
                        else if(!isNumber(webVar_6)){
                                retStatus = 3;
                        }
			else{
				g = buf = strdup(nvram_safe_get("custom_clientlist"));
				while (buf) {
					if ((p = strsep(&g, "<")) == NULL) break;

					iCurrentListNum++;

					if((vstrsep(p, ">", &name, &mac, &group, &type, &callback, &keeparp)) != 6) continue;

					if(!strcmp(webVar_2, mac)){
						retStatus = 1;
						break;
					}
				}
				free(buf);

				// if this current list already is the maximum, can't be added.
				if((iCurrentListNum-1) == ApiMaxNumRegister)
					retStatus = 4;

				if(retStatus == 0){
					strcat(nvramTmp, "<");
					strcat(nvramTmp, webVar_1);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_2);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_3);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_4);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_5);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_6);
					strcat(nvramTmp, nvram_safe_get("custom_clientlist"));

					nvram_set("custom_clientlist", nvramTmp);
					nvram_commit();
				}
			}
		}
		else if(!strcmp(apiAction, "del")){
			webVar_1 = websGetVar(wp, "mac", "");

			if(!strcmp(webVar_1, "")){
				retStatus = 3;
			}
			else{
				retStatus = 2;
				g = buf = strdup(nvram_safe_get("custom_clientlist"));
				while (buf) {
					if ((p = strsep(&g, "<")) == NULL) break;
					if((vstrsep(p, ">", &name, &mac, &group, &type, &callback, &keeparp)) != 6) continue;

					if(!strcmp(webVar_1, mac)){
						retStatus = 0;
						continue;
					}
					else{
						strcat(nvramTmp, "<");
						strcat(nvramTmp, name);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, mac);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, group);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, type);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, callback);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, keeparp);
					}
				}
				free(buf);

				if(retStatus == 0){
					nvram_set("custom_clientlist", nvramTmp);
					nvram_commit();
				}
			}
		}
		else if(!strcmp(apiAction, "list")){
			g = buf = strdup(nvram_safe_get("custom_clientlist"));
			strcat(retList, "<list>\n");
			while (buf) {
				if ((p = strsep(&g, "<")) == NULL) break;
				if((vstrsep(p, ">", &name, &mac, &group, &type, &callback, &keeparp)) != 6) continue;

				strcat(retList, "<device>\n");
				sprintf(strTmp, "<name>%s</name>\n", name);
				strcat(retList, strTmp);
				sprintf(strTmp, "<mac>%s</mac>\n", mac);
				strcat(retList, strTmp);
				sprintf(strTmp, "<group>%s</group>\n", group);
				strcat(retList, strTmp);
				sprintf(strTmp, "<type>%s</type>\n", type);
				strcat(retList, strTmp);
				sprintf(strTmp, "<callback>%s</callback>\n", callback);
				strcat(retList, strTmp);
				sprintf(strTmp, "<keeparp>%s</keeparp>\n", keeparp);
				strcat(retList, strTmp);
				strcat(retList, "</device>\n");
			}
			free(buf);
			strcat(retList, "</list>\n");
		}
		else{
			retStatus = 3;
		}
	}
	else if(!strcmp(apiName, "portforward")){
		char *name, *rport, *lip, *lport, *proto;

		if(!strcmp(apiAction, "enable")){
			if(nvram_match("vts_enable_x", "0")){
				nvram_set("vts_enable_x", "1");
				nvram_commit();
				notify_rc("restart_firewall");
			}
		}
		else if(!strcmp(apiAction, "disable")){
			if(nvram_match("vts_enable_x", "1")){
				nvram_set("vts_enable_x", "0");
				nvram_commit();
				notify_rc("restart_firewall");			
			}
		}
		else if(!strcmp(apiAction, "add")){
			webVar_1 = websGetVar(wp, "name", "");
			webVar_2 = websGetVar(wp, "rport", "");
			webVar_3 = websGetVar(wp, "lip", "");
			webVar_4 = websGetVar(wp, "lport", "");
			webVar_5 = websGetVar(wp, "proto", "");

			if(!strcmp(webVar_1, "") || !strcmp(webVar_2, "") || !strcmp(webVar_3, "") ||
			   !strcmp(webVar_4, "") || !strcmp(webVar_5, "")){
				retStatus = 3;
			}
			else if(strlen(webVar_1) > 32){
				retStatus = 3;
			}
			else if(!isNumber(webVar_2)){
				retStatus = 3;
			}
			else if(!isIP(webVar_3)){
				retStatus = 3;
			}
			else if(!isNumber(webVar_4)){
				retStatus = 3;
			}
			else if(strcmp(webVar_5, "TCP") && strcmp(webVar_5, "UDP") &&
					strcmp(webVar_5, "BOTH") && strcmp(webVar_5, "OTHER")){
				retStatus = 3;
			}
			else{
				g = buf = strdup(nvram_safe_get("vts_rulelist"));
				while (buf) {
					if ((p = strsep(&g, "<")) == NULL) break;

					iCurrentListNum++;

					if((vstrsep(p, ">", &name, &rport, &lip, &lport, &proto)) != 5) continue;

					if(!strcmp(webVar_1, name) && !strcmp(webVar_2, rport) &&
					   !strcmp(webVar_3, lip) && !strcmp(webVar_4, lport) &&
					   !strcmp(webVar_5, proto)
					){
						retStatus = 1;
						break;
					}
				}
				free(buf);

				// if this current list already is the maximum, can't be added.
				if((iCurrentListNum-1) == ApiMaxNumPortforward)
					retStatus = 4;

				if(retStatus == 0){
					strcat(nvramTmp, "<");
					strcat(nvramTmp, webVar_1);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_2);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_3);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_4);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_5);
					strcat(nvramTmp, nvram_safe_get("vts_rulelist"));

					nvram_set("vts_rulelist", nvramTmp);
					nvram_commit();
					notify_rc("restart_firewall"); // need to reboot if ctf is enabled.
				}
			}
		}
		else if(!strcmp(apiAction, "del")){
			webVar_1 = websGetVar(wp, "name", "");
			webVar_2 = websGetVar(wp, "rport", "");
			webVar_3 = websGetVar(wp, "lip", "");
			webVar_4 = websGetVar(wp, "lport", "");
			webVar_5 = websGetVar(wp, "proto", "");

			if(!strcmp(webVar_1, "") && !strcmp(webVar_2, "") && !strcmp(webVar_3, "") &&
			   !strcmp(webVar_4, "") && !strcmp(webVar_5, "")){
				retStatus = 3;
			}
			else{
				retStatus = 2;
				g = buf = strdup(nvram_safe_get("vts_rulelist"));
				while (buf) {
					if ((p = strsep(&g, "<")) == NULL) break;
					if((vstrsep(p, ">", &name, &rport, &lip, &lport, &proto)) != 5) continue;

					if(!strcmp(webVar_1, name) && !strcmp(webVar_2, rport) &&
					   !strcmp(webVar_3, lip) && !strcmp(webVar_4, lport) &&
					   !strcmp(webVar_5, proto)
					){
						retStatus = 0;
						continue;
					}
					else{
						strcat(nvramTmp, "<");
						strcat(nvramTmp, name);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, rport);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, lip);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, lport);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, proto);
					}
				}
				free(buf);

				if(retStatus == 0){
					nvram_set("vts_rulelist", nvramTmp);
					nvram_commit();
					notify_rc("restart_firewall");
				}
			}
		}
		else if(!strcmp(apiAction, "list")){
			g = buf = strdup(nvram_safe_get("vts_rulelist"));
			strcat(retList, "<list>\n");
			while (buf) {
				if ((p = strsep(&g, "<")) == NULL) break;
				if((vstrsep(p, ">", &name, &rport, &lip, &lport, &proto)) != 5) continue;

				strcat(retList, "<item>\n");
				sprintf(strTmp, "<name>%s</name>\n", name);
				strcat(retList, strTmp);
				sprintf(strTmp, "<rport>%s</rport>\n", rport);
				strcat(retList, strTmp);
				sprintf(strTmp, "<lip>%s</lip>\n", lip);
				strcat(retList, strTmp);
				sprintf(strTmp, "<lport>%s</lport>\n", lport);
				strcat(retList, strTmp);
				sprintf(strTmp, "<proto>%s</proto>\n", proto);
				strcat(retList, strTmp);
				strcat(retList, "</item>\n");
			}
			free(buf);

			strcat(retList, "</list>\n");
		}
		else{
			retStatus = 3;
		}
	}
	else if(!strcmp(apiName, "qos")){
		char *desc, *addr, *port, *prio, *transferred, *proto;

		if(!strcmp(apiAction, "enable")){
			webVar_1 = websGetVar(wp, "upbw", "");
			webVar_2 = websGetVar(wp, "downbw", "");

			if(!strcmp(webVar_1, "") || !isNumber(webVar_1)){
				retStatus = 3;
			}
			else if(!strcmp(webVar_2, "") || !isNumber(webVar_2)){
				retStatus = 3;
			}
			else{
				nvram_set("qos_obw", webVar_1);
				nvram_set("qos_ibw", webVar_2);
				if(nvram_match("qos_enable", "1")){
					nvram_commit();
					notify_rc("restart_qos");
				}
				else{
					nvram_set("qos_enable", "1");
					nvram_commit();
					sys_reboot();
				}
			}
		}
		else if(!strcmp(apiAction, "disable")){
			if(nvram_match("qos_enable", "1")){
				nvram_set("qos_enable", "0");
				nvram_commit();
				sys_reboot();
			}
		}
		else if(!strcmp(apiAction, "add")){
			webVar_1 = websGetVar(wp, "name", "");
			webVar_2 = websGetVar(wp, "src", "");
			webVar_3 = websGetVar(wp, "dstport", "");
			webVar_4 = websGetVar(wp, "proto", "");
			webVar_5 = websGetVar(wp, "transferred", "");
			webVar_6 = websGetVar(wp, "prio", "");

			if(!strcmp(webVar_1, "") || !strcmp(webVar_6, "")){
				retStatus = 3;
			}
			else if(!strcmp(webVar_2, "") && !strcmp(webVar_3, "") &&
					!strcmp(webVar_4, "") && !strcmp(webVar_5, "")){
				retStatus = 3;
			}
			else if(strlen(webVar_1) > 32){
				retStatus = 3;
			}
			else if(strcmp(webVar_2, "") && (!isMAC(webVar_2) && !isIP(webVar_2))){
				retStatus = 3;
			}
			else if(strcmp(webVar_4, "") &&
					(strcmp(webVar_4, "tcp") && strcmp(webVar_4, "udp") &&
					 strcmp(webVar_4, "tcp/udp") && strcmp(webVar_4, "any"))) {
				retStatus = 3;
			}
			else if(strcmp(webVar_6, "0") && strcmp(webVar_6, "1") &&
					strcmp(webVar_6, "2") && strcmp(webVar_6, "3") && strcmp(webVar_6, "4")){
				retStatus = 3;
			}
			else{
				g = buf = strdup(nvram_safe_get("qos_rulelist"));
				while (buf) {
					if ((p = strsep(&g, "<")) == NULL) break;

					iCurrentListNum++;

					if((vstrsep(p, ">", &desc, &addr, &port, &proto, &transferred, &prio)) != 6) continue;

					if(!strcmp(webVar_1, desc) && !strcmp(webVar_2, addr) &&
					   !strcmp(webVar_3, port) && !strcmp(webVar_4, proto) &&
					   !strcmp(webVar_5, transferred) && !strcmp(webVar_6, prio)
					){
						retStatus = 1;
						break;
					}
				}
				free(buf);

				// if this current list already is the maximum, can't be added.
				if((iCurrentListNum-1) == ApiMaxNumQos)
					retStatus = 4;

				if(retStatus == 0){
					strcat(nvramTmp, "<");
					strcat(nvramTmp, webVar_1);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_2);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_3);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_4);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_5);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_6);
					strcat(nvramTmp, nvram_safe_get("qos_rulelist"));

					nvram_set("qos_rulelist", nvramTmp);
					nvram_commit();
					notify_rc("restart_qos"); // need to reboot if ctf is enabled.
				}
			}
		}
		else if(!strcmp(apiAction, "del")){
			webVar_1 = websGetVar(wp, "name", "");
			webVar_2 = websGetVar(wp, "src", "");
			webVar_3 = websGetVar(wp, "dstport", "");
			webVar_4 = websGetVar(wp, "proto", "");
			webVar_5 = websGetVar(wp, "transferred", "");
			webVar_6 = websGetVar(wp, "prio", "");

			if(!strcmp(webVar_1, "") && !strcmp(webVar_2, "") && !strcmp(webVar_3, "") &&
			   !strcmp(webVar_4, "") && !strcmp(webVar_5, "") && !strcmp(webVar_6, "")){
				retStatus = 3;
			}
			else{
				retStatus = 2;
				g = buf = strdup(nvram_safe_get("qos_rulelist"));
				while (buf) {
					if ((p = strsep(&g, "<")) == NULL) break;
					if((vstrsep(p, ">", &desc, &addr, &port, &proto, &transferred, &prio)) != 6) continue;

					if(!strcmp(webVar_1, desc) && !strcmp(webVar_2, addr) &&
					   !strcmp(webVar_3, port) && !strcmp(webVar_4, proto) &&
					   !strcmp(webVar_5, transferred) && !strcmp(webVar_6, prio)
					){
						retStatus = 0;
						continue;
					}
					else{
						strcat(nvramTmp, "<");
						strcat(nvramTmp, desc);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, addr);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, port);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, proto);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, transferred);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, prio);
					}
				}
				free(buf);

				if(retStatus == 0){
					nvram_set("qos_rulelist", nvramTmp);
					nvram_commit();
					notify_rc("restart_qos"); // need to reboot if ctf is enabled.
				}
			}
		}
		else if(!strcmp(apiAction, "list")){
			g = buf = strdup(nvram_safe_get("qos_rulelist"));
			strcat(retList, "<list>\n");

			sprintf(strTmp, "<enable>%s</enable>\n", nvram_get("qos_enable"));
			strcat(retList, strTmp);
			sprintf(strTmp, "<upbw>%s</upbw>\n", nvram_get("qos_obw"));
			strcat(retList, strTmp);
			sprintf(strTmp, "<downbw>%s</downbw>\n", nvram_get("qos_ibw"));
			strcat(retList, strTmp);

			while (buf) {
				if ((p = strsep(&g, "<")) == NULL) break;
				if((vstrsep(p, ">", &desc, &addr, &port, &proto, &transferred, &prio)) != 6) continue;

				strcat(retList, "<item>\n");
				sprintf(strTmp, "<name>%s</name>\n", desc);
				strcat(retList, strTmp);
				sprintf(strTmp, "<src>%s</src>\n", addr);
				strcat(retList, strTmp);
				sprintf(strTmp, "<dstport>%s</dstport>\n", port);
				strcat(retList, strTmp);
				sprintf(strTmp, "<proto>%s</proto>\n", proto);
				strcat(retList, strTmp);
				sprintf(strTmp, "<transferred>%s</transferred>\n", transferred);
				strcat(retList, strTmp);
				sprintf(strTmp, "<prio>%s</prio>\n", prio);
				strcat(retList, strTmp);
				strcat(retList, "</item>\n");
			}
			free(buf);

			strcat(retList, "</list>\n");
		}
		else{
				retStatus = 3;
		}
	}
	else{
		retStatus = 3;
	}

	websWrite(wp, "<status>%d</status>\n", retStatus);
	if(!strcmp(apiAction, "list"))
		websWrite(wp, "%s", retList);
	return 0;
}
#endif

static int ej_update_variables(int eid, webs_t wp, int argc, char_t **argv) {
	char *action_mode;
	char *action_script;
	char *action_wait;
	char *wan_unit = websGetVar(wp, "wan_unit", "0");
	char notify_cmd[128];
	int do_apply;

	// assign control variables
	action_mode = websGetVar(wp, "action_mode", "");
	action_script = websGetVar(wp, "action_script", "restart_net");
	action_wait = websGetVar(wp, "action_wait", "5");

	_dprintf("update_variables: [%s] [%s] [%s]\n", action_mode, action_script, action_wait);

	if ((do_apply = !strcmp(action_mode, "apply")) ||
	    !strcmp(action_mode, "apply_new"))
	{
		int has_modify;
		if (!(has_modify = validate_apply(wp, NULL))) {
			websWrite(wp, "<script>no_changes_and_no_committing();</script>\n");
		}
		else {
			websWrite(wp, "<script>done_committing();</script>\n");
		}
		if(do_apply || has_modify) {
#ifdef RTCONFIG_QTN
			/* early stop wps for QTN */
			if (strcmp(action_script, "restart_wireless") == 0
			  ||strcmp(action_script, "restart_net") == 0)
			{
#if 0
				if (rpc_qtn_ready())
				{
					rpc_qcsapi_wifi_disable_wps(WIFINAME, 1);

					if (nvram_get_int("wps_enable")){
						rpc_qcsapi_wifi_disable_wps(WIFINAME, !nvram_get_int("wps_enable"));
						qcsapi_wps_set_ap_pin(WIFINAME, nvram_safe_get("wps_device_pin"));
					}

				}
#endif
			}
#endif
			if (strlen(action_script) > 0) {
				char *p1, *p2;
				if(!strcmp(action_script, "restart_wtfd")){
					_dprintf("send SIGHUP to wtfd!");
					killall("wtfd", SIGHUP);
				}
				else{
					memset(notify_cmd, 0, sizeof(notify_cmd));
					if((p1 = strstr(action_script, "_wan_if")))
					{
						p1 += 7;
						strncpy(notify_cmd, action_script, p1 - action_script);
						p2 = notify_cmd + strlen(notify_cmd);
						sprintf(p2, " %s%s", wan_unit, p1);
					}
					else
						strncpy(notify_cmd, action_script, 128);

					if(strcmp(action_script, "saveNvram"))
					{
						if(!strcmp(action_script, "QisFinish")){
							skip_auth = 0;
						}else{
							nvram_set("freeze_duck", "15");
							notify_rc(notify_cmd);
						}
					}
				}
			}
#if defined(RTCONFIG_RALINK) ||  defined(RTCONFIG_QCA)
			if (!strcmp(action_script, "restart_wireless") || !strcmp(action_script, "restart_net")) {
				char *rc_support = nvram_safe_get("rc_support");
				if (find_word(rc_support, "2.4G") && find_word(rc_support, "5G"))
					websWrite(wp, "<script>restart_needed_time(%d);</script>\n", atoi(action_wait) + 20);
				else
					websWrite(wp, "<script>restart_needed_time(%d);</script>\n", atoi(action_wait) + 5);
			}
			else
#endif
#if defined(RTCONFIG_RALINK)
			if (!strcmp(action_script, "restart_net_and_phy")) {
				char *rc_support = nvram_safe_get("rc_support");
				if (find_word(rc_support, "2.4G") && find_word(rc_support, "5G"))
					websWrite(wp, "<script>restart_needed_time(%d);</script>\n", atoi(action_wait) + 10);
				else
					websWrite(wp, "<script>restart_needed_time(%d);</script>\n", atoi(action_wait) + 5);
			}
			else
#endif			
			websWrite(wp, "<script>restart_needed_time(%d);</script>\n", atoi(action_wait));
		}
	}
	return 0;
}

static int convert_asus_variables(int eid, webs_t wp, int argc, char_t **argv) {
	return 0;
}

static int asus_nvram_commit(int eid, webs_t wp, int argc, char_t **argv) {
	return 0;
}

static int ej_notify_services(int eid, webs_t wp, int argc, char_t **argv) {
	return 0;
}

char *Ch_conv(char *proto_name, int idx)
{
	char *proto;
	char qos_name_x[32];
	sprintf(qos_name_x, "%s%d", proto_name, idx);
	if (nvram_match(qos_name_x,""))
	{
		return NULL;
	}
	else
	{
		proto=nvram_get(qos_name_x);
		return proto;
	}
}

static int enable_hwnat()
{
	int qos_userspec_app_en = 0;
	int rulenum = nvram_get_int("qos_rulenum_x"), idx_class = 0;
	int ret = 0;
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char wan_proto[16];

#if defined (W7_LOGO) || defined (WIFI_LOGO)
		return 0;
#endif

	if(nvram_invmatch("sw_mode_ex", "1"))
		return 0;

	unit = 0;
	wan_prefix(unit, prefix);
	memset(wan_proto, 0, 16);
	strcpy(wan_proto, nvram_safe_get(strcat_r(prefix, "proto", tmp)));

	if(!strcmp(wan_proto, "pptp") || !strcmp(wan_proto, "l2tp"))
		return 0;

	/* Add class for User specify, 10:20(high), 10:40(middle), 10:60(low)*/
	if (rulenum) {
		for (idx_class=0; idx_class < rulenum; idx_class++)
		{
			if (atoi(Ch_conv("qos_prio_x", idx_class)) == 1)
			{
				qos_userspec_app_en = 1;
				break;
			}
			else if (atoi(Ch_conv("qos_prio_x", idx_class)) == 6)
			{
				qos_userspec_app_en = 1;
				break;
			}
		}
	}
/*
	if (nvram_match("mr_enable_x", "1"))
		ret += 1;

	if (	nvram_match("qos_tos_prio", "1") ||
		nvram_match("qos_pshack_prio", "1") ||
		nvram_match("qos_service_enable", "1") ||
		nvram_match("qos_shortpkt_prio", "1")	)
		ret += 2;
*/

	if (	(nvram_invmatch("qos_manual_ubw","0") && nvram_invmatch("qos_manual_ubw","")) ||
		(rulenum && qos_userspec_app_en)	)
		ret = 1;

	return ret;
}

static int check_hwnat(int eid, webs_t wp, int argc, char_t **argv){
	if(!enable_hwnat())
		websWrite(wp, "0");
	else
		websWrite(wp, "1");

	return 0;
}

static int wanstate_hook(int eid, webs_t wp, int argc, char_t **argv){
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;

	/* current unit */
#ifdef RTCONFIG_DUALWAN
	if(nvram_match("wans_mode", "lb"))
		unit = WAN_UNIT_FIRST;
	else
#endif
		unit = wan_primary_ifunit();
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "wanstate = %d;\n", wan_state);
	websWrite(wp, "wansbstate = %d;\n", wan_sbstate);
	websWrite(wp, "wanauxstate = %d;\n", wan_auxstate);

	return 0;
}

static int dual_wanstate_hook(int eid, webs_t wp, int argc, char_t **argv){
#ifdef RTCONFIG_DUALWAN
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;

	unit = WAN_UNIT_FIRST;
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "first_wanstate = %d;\n", wan_state);
	websWrite(wp, "first_wansbstate = %d;\n", wan_sbstate);
	websWrite(wp, "first_wanauxstate = %d;\n", wan_auxstate);

	memset(prefix, 0, sizeof(prefix));	
	unit = WAN_UNIT_SECOND;
	wan_prefix(unit, prefix);

	memset(tmp, 0, 100);
	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "second_wanstate = %d;\n", wan_state);
	websWrite(wp, "second_wansbstate = %d;\n", wan_sbstate);
	websWrite(wp, "second_wanauxstate = %d;\n", wan_auxstate);
#else
	websWrite(wp, "first_wanstate = -1;\n");
	websWrite(wp, "first_wansbstate = -1;\n");
	websWrite(wp, "first_wanauxstate = -1;\n");
	websWrite(wp, "second_wanstate = -1;\n");
	websWrite(wp, "second_wansbstate = -1;\n");
	websWrite(wp, "second_wanauxstate = -1;\n");
#endif

	return 0;
}

static int ajax_dualwanstate_hook(int eid, webs_t wp, int argc, char_t **argv){
#ifdef RTCONFIG_DUALWAN
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;

	unit = WAN_UNIT_FIRST;
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "<first_wan>%d</first_wan>\n", wan_state);
	websWrite(wp, "<first_wan>%d</first_wan>\n", wan_sbstate);
	websWrite(wp, "<first_wan>%d</first_wan>\n", wan_auxstate);

	memset(prefix, 0, sizeof(prefix));	
	unit = WAN_UNIT_SECOND;
	wan_prefix(unit, prefix);

	memset(tmp, 0, 100);
	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "<second_wan>%d</second_wan>\n", wan_state);
	websWrite(wp, "<second_wan>%d</second_wan>\n", wan_sbstate);
	websWrite(wp, "<second_wan>%d</second_wan>\n", wan_auxstate);
#else
	websWrite(wp, "<first_wan>-1</first_wan>\n");
	websWrite(wp, "<first_wan>-1</first_wan>\n");
	websWrite(wp, "<first_wan>-1</first_wan>\n");
	websWrite(wp, "<second_wan>-1</second_wan>\n");
	websWrite(wp, "<second_wan>-1</second_wan>\n");
	websWrite(wp, "<second_wan>-1</second_wan>\n");
#endif

	return 0;
}


static int ajax_wanstate_hook(int eid, webs_t wp, int argc, char_t **argv){
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;

	/* current unit */
#ifdef RTCONFIG_DUALWAN
	if(nvram_match("wans_mode", "lb"))
		unit = WAN_UNIT_FIRST;
	else
#endif
		unit = wan_primary_ifunit();
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "<wan>%d</wan>\n", wan_state);
	websWrite(wp, "<wan>%d</wan>\n", wan_sbstate);
	websWrite(wp, "<wan>%d</wan>\n", wan_auxstate);

	return 0;
}

static int secondary_ajax_wanstate_hook(int eid, webs_t wp, int argc, char_t **argv){
#ifdef RTCONFIG_DUALWAN
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;

	/* current unit */
	unit = WAN_UNIT_SECOND;
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "<secondary_wan>%d</secondary_wan>\n", wan_state);
	websWrite(wp, "<secondary_wan>%d</secondary_wan>\n", wan_sbstate);
	websWrite(wp, "<secondary_wan>%d</secondary_wan>\n", wan_auxstate);
#else
	websWrite(wp, "<secondary_wan>-1</secondary_wan>\n");
	websWrite(wp, "<secondary_wan>-1</secondary_wan>\n");
	websWrite(wp, "<secondary_wan>-1</secondary_wan>\n");
#endif

	return 0;
}

static int wanlink_hook(int eid, webs_t wp, int argc, char_t **argv){
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;
	int unit, status = 0;
	char *statusstr[2] = { "Disconnected", "Connected" };
	char *wan_proto, *type;
	char *ip = "0.0.0.0";
	char *netmask = "0.0.0.0";
	char *gateway = "0.0.0.0";
	unsigned int lease = 0, expires = 0;
	char *xtype = "";
	char *xip = "0.0.0.0";
	char *xnetmask = "0.0.0.0";
	char *xgateway = "0.0.0.0";
	unsigned int xlease = 0, xexpires = 0;
	char *name = NULL;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		//_dprintf("name = NULL\n");
	}

	/* current unit */
#ifdef RTCONFIG_DUALWAN
	if(nvram_match("wans_mode", "lb"))
		unit = WAN_UNIT_FIRST;
	else
#endif
	unit = wan_primary_ifunit(); //Paul add 2013/7/24, get current working wan unit

	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if (dualwan_unit__usbif(unit)) {
		if(wan_state == WAN_STATE_CONNECTED){
			status = 1;
		}
		else{
			status = 0;
		}
	}
	else if(wan_state == WAN_STATE_DISABLED){
		status = 0;
	}
// DSLTODO, need a better integration
#ifdef RTCONFIG_DSL
	// if dualwan & enable lan port as wan
	// it always report disconnected
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#else
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#endif
	else if(wan_auxstate == WAN_AUXSTATE_NO_INTERNET_ACTIVITY&&(nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOINTERNET)) {
		status = 0;
	}
	else if(!strcmp(wan_proto, "pppoe")
			|| !strcmp(wan_proto, "pptp")
			|| !strcmp(wan_proto, "l2tp")
			)
	{
		if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_STOPPED && wan_sbstate != WAN_STOPPED_REASON_PPP_LACK_ACTIVITY){
			status = 0;
		}
		else{
			status = 1;
		}
	}
	else{
		//if(wan_state == WAN_STATE_STOPPED && wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR){
		if(wan_state == WAN_STATE_STOPPED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else {
			// treat short lease time as disconnected
			if(!strcmp(wan_proto, "dhcp") &&
			  nvram_get_int(strcat_r(prefix, "lease", tmp)) <= 60 &&
			  is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)))
			) {
				status = 0;
			}
			else {
				status = 1;
			}
		}
	}

#ifdef RTCONFIG_USB
	if (dualwan_unit__usbif(unit))
		type = "USB Modem";
	else
#endif
		type = wan_proto;

	if(status != 0){
		ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
		netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
		gateway = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
		lease = nvram_get_int(strcat_r(prefix, "lease", tmp));
		if (lease > 0)
			expires = nvram_get_int(strcat_r(prefix, "expires", tmp)) - uptime();
	}

	if(name == NULL){
		websWrite(wp, "function wanlink_status() { return %d;}\n", status);
		websWrite(wp, "function wanlink_statusstr() { return '%s';}\n", statusstr[status]);
		websWrite(wp, "function wanlink_type() { return '%s';}\n", type);
		websWrite(wp, "function wanlink_ipaddr() { return '%s';}\n", ip);
		websWrite(wp, "function wanlink_netmask() { return '%s';}\n", netmask);
		websWrite(wp, "function wanlink_gateway() { return '%s';}\n", gateway);
		websWrite(wp, "function wanlink_dns() { return '%s';}\n", nvram_safe_get(strcat_r(prefix, "dns", tmp)));
		websWrite(wp, "function wanlink_lease() { return %d;}\n", lease);
		websWrite(wp, "function wanlink_expires() { return %d;}\n", expires);
		websWrite(wp, "function is_private_subnet() { return '%d';}\n", is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp))));
	}else if(!strcmp(name,"status"))
		websWrite(wp, "%d", status);
	else if(!strcmp(name,"statusstr"))
		websWrite(wp, "%s", statusstr[status]);
	else if(!strcmp(name,"type"))
		websWrite(wp, "%s", type);
	else if(!strcmp(name,"ipaddr"))
		websWrite(wp, "%s", ip);
	else if(!strcmp(name,"netmask"))
		websWrite(wp, "%s", netmask);
	else if(!strcmp(name,"gateway"))
		websWrite(wp, "%s", gateway);
	else if(!strcmp(name,"dns"))
		websWrite(wp, "%s", nvram_safe_get(strcat_r(prefix, "dns", tmp)));
	else if(!strcmp(name,"lease"))
		websWrite(wp, "%d", lease);
	else if(!strcmp(name,"expires"))
		websWrite(wp, "%d", expires);
	else if(!strcmp(name,"private_subnet"))
		websWrite(wp, "%d", is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp))));

	if (strcmp(wan_proto, "pppoe") == 0 ||
	    strcmp(wan_proto, "pptp") == 0 ||
	    strcmp(wan_proto, "l2tp") == 0) {
		int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
		xtype = (dhcpenable == 0) ? "static" :
			(strcmp(wan_proto, "pppoe") == 0 && nvram_match(strcat_r(prefix, "vpndhcp", tmp), "0")) ? "" : /* zeroconf */
			"dhcp";
		xip = nvram_safe_get(strcat_r(prefix, "xipaddr", tmp));
		xnetmask = nvram_safe_get(strcat_r(prefix, "xnetmask", tmp));
		xgateway = nvram_safe_get(strcat_r(prefix, "xgateway", tmp));
		xlease = nvram_get_int(strcat_r(prefix, "xlease", tmp));
		if (xlease > 0)
			xexpires = nvram_get_int(strcat_r(prefix, "xexpires", tmp)) - uptime();
	}

	if(name == NULL){
		websWrite(wp, "function wanlink_xtype() { return '%s';}\n", xtype);
		websWrite(wp, "function wanlink_xipaddr() { return '%s';}\n", xip);
		websWrite(wp, "function wanlink_xnetmask() { return '%s';}\n", xnetmask);
		websWrite(wp, "function wanlink_xgateway() { return '%s';}\n", xgateway);
		websWrite(wp, "function wanlink_xdns() { return '%s';}\n", nvram_safe_get(strcat_r(prefix, "xdns", tmp)));
		websWrite(wp, "function wanlink_xlease() { return %d;}\n", xlease);
		websWrite(wp, "function wanlink_xexpires() { return %d;}\n", xexpires);
	}else if(!strcmp(name,"xtype"))
		websWrite(wp, "%s", xtype);
	else if(!strcmp(name,"xipaddr"))
		websWrite(wp, "%s", xip);
	else if(!strcmp(name,"xnetmask"))
		websWrite(wp, "%s", xnetmask);
	else if(!strcmp(name,"xgateway"))
		websWrite(wp, "%s", xgateway);
	else if(!strcmp(name,"xdns"))
		websWrite(wp, "%s", nvram_safe_get(strcat_r(prefix, "xdns", tmp)));
	else if(!strcmp(name,"xlease"))
		websWrite(wp, "%d", xlease);
	else if(!strcmp(name,"xexpires"))
		websWrite(wp, "%d", xexpires);

	return 0;
}

static int first_wanlink_hook(int eid, webs_t wp, int argc, char_t **argv){
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;
	int unit, status = 0;
	char *statusstr[2] = { "Disconnected", "Connected" };
	char *wan_proto, *type;
	char *ip = "0.0.0.0";
	char *netmask = "0.0.0.0";
	char *gateway = "0.0.0.0";
	unsigned int lease = 0, expires = 0;
	char *xtype = "";
	char *xip = "0.0.0.0";
	char *xnetmask = "0.0.0.0";
	char *xgateway = "0.0.0.0";
	unsigned int xlease = 0, xexpires = 0;

	unit = WAN_UNIT_FIRST;
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if (dualwan_unit__usbif(unit)) {
		if(wan_state == WAN_STATE_CONNECTED){
			status = 1;
		}
		else{
			status = 0;
		}
	}
	else if(wan_state == WAN_STATE_DISABLED){
		status = 0;
	}
// DSLTODO, need a better integration
#ifdef RTCONFIG_DSL
	// if dualwan & enable lan port as wan
	// it always report disconnected
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#else
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#endif
	else if(wan_auxstate == WAN_AUXSTATE_NO_INTERNET_ACTIVITY&&(nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOINTERNET)) {
		status = 0;
	}
	else if(!strcmp(wan_proto, "pppoe")
			|| !strcmp(wan_proto, "pptp")
			|| !strcmp(wan_proto, "l2tp")
			)
	{
		if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_STOPPED && wan_sbstate != WAN_STOPPED_REASON_PPP_LACK_ACTIVITY){
			status = 0;
		}
		else{
			status = 1;
		}
	}
	else{
		//if(wan_state == WAN_STATE_STOPPED && wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR){
		if(wan_state == WAN_STATE_STOPPED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else {
			// treat short lease time as disconnected
			if(!strcmp(wan_proto, "dhcp") &&
			  nvram_get_int(strcat_r(prefix, "lease", tmp)) <= 60 &&
			  is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)))
			) {
				status = 0;
			}
			else {
				status = 1;
			}
		}
	}

#ifdef RTCONFIG_USB
	if (dualwan_unit__usbif(unit))
		type = "USB Modem";
	else
#endif
		type = wan_proto;

	if(status != 0){
		ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
		netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
		gateway = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
		lease = nvram_get_int(strcat_r(prefix, "lease", tmp));
		if (lease > 0)
			expires = nvram_get_int(strcat_r(prefix, "expires", tmp)) - uptime();
	}

	websWrite(wp, "function first_wanlink_status() { return %d;}\n", status);
	websWrite(wp, "function first_wanlink_statusstr() { return '%s';}\n", statusstr[status]);
	websWrite(wp, "function first_wanlink_type() { return '%s';}\n", type);
	websWrite(wp, "function first_wanlink_ipaddr() { return '%s';}\n", ip);
	websWrite(wp, "function first_wanlink_netmask() { return '%s';}\n", netmask);
	websWrite(wp, "function first_wanlink_gateway() { return '%s';}\n", gateway);
	websWrite(wp, "function first_wanlink_dns() { return '%s';}\n", nvram_safe_get(strcat_r(prefix, "dns", tmp)));
	websWrite(wp, "function first_wanlink_lease() { return %d;}\n", lease);
	websWrite(wp, "function first_wanlink_expires() { return %d;}\n", expires);

	if (strcmp(wan_proto, "pppoe") == 0 ||
	    strcmp(wan_proto, "pptp") == 0 ||
	    strcmp(wan_proto, "l2tp") == 0) {
		int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
		xtype = (dhcpenable == 0) ? "static" :
			(strcmp(wan_proto, "pppoe") == 0 && nvram_match(strcat_r(prefix, "vpndhcp", tmp), "0")) ? "" : /* zeroconf */
			"dhcp";
		xip = nvram_safe_get(strcat_r(prefix, "xipaddr", tmp));
		xnetmask = nvram_safe_get(strcat_r(prefix, "xnetmask", tmp));
		xgateway = nvram_safe_get(strcat_r(prefix, "xgateway", tmp));
		xlease = nvram_get_int(strcat_r(prefix, "xlease", tmp));
		if (xlease > 0)
			xexpires = nvram_get_int(strcat_r(prefix, "xexpires", tmp)) - uptime();
	}

	websWrite(wp, "function first_wanlink_xtype() { return '%s';}\n", xtype);
	websWrite(wp, "function first_wanlink_xipaddr() { return '%s';}\n", xip);
	websWrite(wp, "function first_wanlink_xnetmask() { return '%s';}\n", xnetmask);
	websWrite(wp, "function first_wanlink_xgateway() { return '%s';}\n", xgateway);
	websWrite(wp, "function first_wanlink_xdns() { return '%s';}\n", nvram_safe_get(strcat_r(prefix, "xdns", tmp)));
	websWrite(wp, "function first_wanlink_xlease() { return %d;}\n", xlease);
	websWrite(wp, "function first_wanlink_xexpires() { return %d;}\n", xexpires);

	return 0;
}

static int secondary_wanlink_hook(int eid, webs_t wp, int argc, char_t **argv){
#ifdef RTCONFIG_DUALWAN
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;
	int unit, status = 0;
	char *statusstr[2] = { "Disconnected", "Connected" };
	char *wan_proto, *type;
	char *ip = "0.0.0.0";
	char *netmask = "0.0.0.0";
	char *gateway = "0.0.0.0";
	unsigned int lease = 0, expires = 0;
	char *xtype = "";
	char *xip = "0.0.0.0";
	char *xnetmask = "0.0.0.0";
	char *xgateway = "0.0.0.0";
	unsigned int xlease = 0, xexpires = 0;

	/* current unit */
	unit = WAN_UNIT_SECOND;
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	if (dualwan_unit__usbif(unit)) {
		if(wan_state == WAN_STATE_CONNECTED){
			status = 1;
		}
		else{
			status = 0;
		}
	}
	else if(wan_state == WAN_STATE_DISABLED){
		status = 0;
	}
// DSLTODO, need a better integration
#ifdef RTCONFIG_DSL
	// if dualwan & enable lan port as wan
	// it always report disconnected
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#else
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#endif
	else if(wan_auxstate == WAN_AUXSTATE_NO_INTERNET_ACTIVITY&&(nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOINTERNET)) {
		status = 0;
	}
	else if(!strcmp(wan_proto, "pppoe")
			|| !strcmp(wan_proto, "pptp")
			|| !strcmp(wan_proto, "l2tp")
			)
	{
		if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_STOPPED && wan_sbstate != WAN_STOPPED_REASON_PPP_LACK_ACTIVITY){
			status = 0;
		}
		else{
			status = 1;
		}
	}
	else{
		if(wan_state == WAN_STATE_STOPPED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else {
			// treat short lease time as disconnected
			if(!strcmp(wan_proto, "dhcp") &&
			  nvram_get_int(strcat_r(prefix, "lease", tmp)) <= 60 &&
			  is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)))
			) {
				status = 0;
			}
			else {
				status = 1;
			}
		}
	}

	if (dualwan_unit__usbif(unit))
		type = "USB Modem";
	else
		type = wan_proto;

	if(status != 0){
		ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
		netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
		gateway = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
		lease = nvram_get_int(strcat_r(prefix, "lease", tmp));
		if (lease > 0)
			expires = nvram_get_int(strcat_r(prefix, "expires", tmp)) - uptime();
	}

	websWrite(wp, "function secondary_wanlink_status() { return %d;}\n", status);
	websWrite(wp, "function secondary_wanlink_statusstr() { return '%s';}\n", statusstr[status]);
	websWrite(wp, "function secondary_wanlink_type() { return '%s';}\n", type);
	websWrite(wp, "function secondary_wanlink_ipaddr() { return '%s';}\n", ip);
	websWrite(wp, "function secondary_wanlink_netmask() { return '%s';}\n", netmask);
	websWrite(wp, "function secondary_wanlink_gateway() { return '%s';}\n", gateway);
	websWrite(wp, "function secondary_wanlink_dns() { return '%s';}\n", nvram_safe_get(strcat_r(prefix, "dns", tmp)));
	websWrite(wp, "function secondary_wanlink_lease() { return %d;}\n", lease);
	websWrite(wp, "function secondary_wanlink_expires() { return %d;}\n", expires);

	if (strcmp(wan_proto, "pppoe") == 0 ||
	    strcmp(wan_proto, "pptp") == 0 ||
	    strcmp(wan_proto, "l2tp") == 0) {
		int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
		xtype = (dhcpenable == 0) ? "static" :
			(strcmp(wan_proto, "pppoe") == 0 && nvram_match(strcat_r(prefix, "vpndhcp", tmp), "0")) ? "" : /* zeroconf */
			"dhcp";
		xip = nvram_safe_get(strcat_r(prefix, "xipaddr", tmp));
		xnetmask = nvram_safe_get(strcat_r(prefix, "xnetmask", tmp));
		xgateway = nvram_safe_get(strcat_r(prefix, "xgateway", tmp));
		xlease = nvram_get_int(strcat_r(prefix, "xlease", tmp));
		if (xlease > 0)
			xexpires = nvram_get_int(strcat_r(prefix, "xexpires", tmp)) - uptime();
	}

	websWrite(wp, "function secondary_wanlink_xtype() { return '%s';}\n", xtype);
	websWrite(wp, "function secondary_wanlink_xipaddr() { return '%s';}\n", xip);
	websWrite(wp, "function secondary_wanlink_xnetmask() { return '%s';}\n", xnetmask);
	websWrite(wp, "function secondary_wanlink_xgateway() { return '%s';}\n", xgateway);
	websWrite(wp, "function secondary_wanlink_xdns() { return '%s';}\n", nvram_safe_get(strcat_r(prefix, "xdns", tmp)));
	websWrite(wp, "function secondary_wanlink_xlease() { return %d;}\n", xlease);
	websWrite(wp, "function secondary_wanlink_xexpires() { return %d;}\n", xexpires);
#else
	websWrite(wp, "function secondary_wanlink_status() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_statusstr() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_type() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_ipaddr() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_netmask() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_gateway() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_dns() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_lease() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_expires() { return -1;}\n");

	websWrite(wp, "function secondary_wanlink_xtype() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_xipaddr() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_xnetmask() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_xgateway() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_xdns() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_xlease() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_xexpires() { return -1;}\n");
#endif
	return 0;
}

static int wan_action_hook(int eid, webs_t wp, int argc, char_t **argv){
	char *action;
	int needed_seconds = 0;
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char wan_enable[16];

	unit = wan_primary_ifunit();
	wan_prefix(unit, prefix);
	memset(wan_enable, 0, 16);
	strcpy(wan_enable, strcat_r(prefix, "enable", tmp));

	// assign control variables
	action = websGetVar(wp, "wanaction", "");
	if (strlen(action) <= 0){
		fprintf(stderr, "No connect action in wan_action_hook!\n");
		return -1;
	}

	fprintf(stderr, "wan action: %s\n", action);

	// TODO: multiple interface
	if(!strcmp(action, "Connect")){
		nvram_set_int(wan_enable, 0);
		nvram_set("freeze_duck", "15");
		notify_rc("start_wan");
	}
	else if (!strcmp(action, "Disconnect")){
		nvram_set_int(wan_enable, 1);
		nvram_set("freeze_duck", "10");
		notify_rc("stop_wan");
	}

	websWrite(wp, "<script>restart_needed_time(%d);</script>\n", needed_seconds);

	return 0;
}

static int get_wan_unit_hook(int eid, webs_t wp, int argc, char_t **argv){
	int unit;

	unit = wan_primary_ifunit();

	websWrite(wp, "%d", unit);

	return 0;
}

static int wanlink_state_hook(int eid, webs_t wp, int argc, char_t **argv){

	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;
	int unit, status = 0;
	char *statusstr[2] = { "Disconnected", "Connected" };
	char *wan_proto, *type;
	char *ip = "0.0.0.0";
	char *netmask = "0.0.0.0";
	char *gateway = "0.0.0.0";
	unsigned int lease = 0, expires = 0;
	char *xtype = "";
	char *xip = "0.0.0.0";
	char *xnetmask = "0.0.0.0";
	char *xgateway = "0.0.0.0";
	unsigned int xlease = 0, xexpires = 0;
	char *name = NULL;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		//_dprintf("name = NULL\n");
	}

	/* current unit */
#ifdef RTCONFIG_DUALWAN
	if(nvram_match("wans_mode", "lb"))
		unit = WAN_UNIT_FIRST;
	else
#endif
		unit = wan_primary_ifunit();
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "\"wanstate\":\"%d\",\n", wan_state);
	websWrite(wp, "\"wansbstate\":\"%d\",\n", wan_sbstate);
	websWrite(wp, "\"wanauxstate\":\"%d\",\n", wan_auxstate);
	websWrite(wp, "\"autodet_state\":\"%d\",\n", nvram_get_int("autodet_state"));
	websWrite(wp, "\"autodet_auxstate\":\"%d\",\n", nvram_get_int("autodet_auxstate"));

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if (dualwan_unit__usbif(unit)) {
		if(wan_state == WAN_STATE_CONNECTED){
			status = 1;
		}
		else{
			status = 0;
		}
	}
	else if(wan_state == WAN_STATE_DISABLED){
		status = 0;
	}
// DSLTODO, need a better integration
#ifdef RTCONFIG_DSL
	// if dualwan & enable lan port as wan
	// it always report disconnected
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#else
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#endif
	else if(wan_auxstate == WAN_AUXSTATE_NO_INTERNET_ACTIVITY&&(nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOINTERNET)) {
		status = 0;
	}
	else if(!strcmp(wan_proto, "pppoe")
			|| !strcmp(wan_proto, "pptp")
			|| !strcmp(wan_proto, "l2tp")
			)
	{
		if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_STOPPED && wan_sbstate != WAN_STOPPED_REASON_PPP_LACK_ACTIVITY){
			status = 0;
		}
		else{
			status = 1;
		}
	}
	else{
		//if(wan_state == WAN_STATE_STOPPED && wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR){
		if(wan_state == WAN_STATE_STOPPED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else {
			// treat short lease time as disconnected
			if(!strcmp(wan_proto, "dhcp") &&
			  nvram_get_int(strcat_r(prefix, "lease", tmp)) <= 60 &&
			  is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)))
			) {
				status = 0;
			}
			else {
				status = 1;
			}
		}
	}

#ifdef RTCONFIG_USB
	if (dualwan_unit__usbif(unit))
		type = "USB Modem";
	else
#endif
		type = wan_proto;

	if(status != 0){
		ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
		netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
		gateway = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
		lease = nvram_get_int(strcat_r(prefix, "lease", tmp));
		if (lease > 0)
			expires = nvram_get_int(strcat_r(prefix, "expires", tmp)) - uptime();
	}

	if(!strcmp(name,"appobj")){
		websWrite(wp, "\"wanlink_status\":\"%d\",\n", status);
		websWrite(wp, "\"wanlink_statusstr\":\"%s\",\n", statusstr[status]);
		websWrite(wp, "\"wanlink_type\":\"%s\",\n", type);
		websWrite(wp, "\"wanlink_ipaddr\":\"%s\",\n", ip);
		websWrite(wp, "\"wanlink_netmask\":\"%s\",\n", netmask);
		websWrite(wp, "\"wanlink_gateway\":\"%s\",\n", gateway);
		websWrite(wp, "\"wanlink_dns\":\"%s\",\n", nvram_safe_get(strcat_r(prefix, "dns", tmp)));
		websWrite(wp, "\"wanlink_lease\":\"%d\",\n", lease);
		websWrite(wp, "\"wanlink_expires\":\"%d\",\n", expires);
		websWrite(wp, "\"is_private_subnet\":\"%d\",\n", is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp))));
	}else if(!strcmp(name,"status"))
		websWrite(wp, "%d", status);
	else if(!strcmp(name,"statusstr"))
		websWrite(wp, "%s", statusstr[status]);
	else if(!strcmp(name,"type"))
		websWrite(wp, "%s", type);
	else if(!strcmp(name,"ipaddr"))
		websWrite(wp, "%s", ip);
	else if(!strcmp(name,"netmask"))
		websWrite(wp, "%s", netmask);
	else if(!strcmp(name,"gateway"))
		websWrite(wp, "%s", gateway);
	else if(!strcmp(name,"dns"))
		websWrite(wp, "%s", nvram_safe_get(strcat_r(prefix, "dns", tmp)));
	else if(!strcmp(name,"lease"))
		websWrite(wp, "%d", lease);
	else if(!strcmp(name,"expires"))
		websWrite(wp, "%d", expires);
	else if(!strcmp(name,"private_subnet"))
		websWrite(wp, "%d", is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp))));

	if (strcmp(wan_proto, "pppoe") == 0 ||
	    strcmp(wan_proto, "pptp") == 0 ||
	    strcmp(wan_proto, "l2tp") == 0) {
		int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
		xtype = (dhcpenable == 0) ? "static" :
			(strcmp(wan_proto, "pppoe") == 0 && nvram_match(strcat_r(prefix, "vpndhcp", tmp), "0")) ? "" : /* zeroconf */
			"dhcp";
		xip = nvram_safe_get(strcat_r(prefix, "xipaddr", tmp));
		xnetmask = nvram_safe_get(strcat_r(prefix, "xnetmask", tmp));
		xgateway = nvram_safe_get(strcat_r(prefix, "xgateway", tmp));
		xlease = nvram_get_int(strcat_r(prefix, "xlease", tmp));
		if (xlease > 0)
			xexpires = nvram_get_int(strcat_r(prefix, "xexpires", tmp)) - uptime();
	}

	if(!strcmp(name,"appobj")){
		websWrite(wp, "\"wanlink_xtype\":\"%s\",\n", xtype);
		websWrite(wp, "\"wanlink_xipaddr\":\"%s\",\n", xip);
		websWrite(wp, "\"wanlink_xnetmask\":\"%s\",\n", xnetmask);
		websWrite(wp, "\"wanlink_xgateway\":\"%s\",\n", xgateway);
		websWrite(wp, "\"wanlink_xdns\":\"%s\",\n", nvram_safe_get(strcat_r(prefix, "xdns", tmp)));
		websWrite(wp, "\"wanlink_xlease\":\"%d\",\n", xlease);
		websWrite(wp, "\"wanlink_xexpires\":\"%d\"\n", xexpires);
	}else if(!strcmp(name,"xtype"))
		websWrite(wp, "%s", xtype);
	else if(!strcmp(name,"xipaddr"))
		websWrite(wp, "%s", xip);
	else if(!strcmp(name,"xnetmask"))
		websWrite(wp, "%s", xnetmask);
	else if(!strcmp(name,"xgateway"))
		websWrite(wp, "%s", xgateway);
	else if(!strcmp(name,"xdns"))
		websWrite(wp, "%s", nvram_safe_get(strcat_r(prefix, "xdns", tmp)));
	else if(!strcmp(name,"xlease"))
		websWrite(wp, "%d", xlease);
	else if(!strcmp(name,"xexpires"))
		websWrite(wp, "%d", xexpires);

	return 0;
}

static int ej_get_ascii_parameter(int eid, webs_t wp, int argc, char_t **argv){
	char tmp[MAX_LINE_SIZE];
	char *buf = tmp, *str;
	int ret = 0;

	if (argc < 1){
		websError(wp, 400,
			"get_parameter() used with no arguments, but at least one "
			"argument is required to specify the parameter name\n");
		return -1;
	}

	str = websGetVar(wp, argv[0], "");

	/* each char expands to %XX at max */
	ret = strlen(str) * sizeof(char)*3 + sizeof(char);
	if (ret > sizeof(tmp)) {
		buf = (char *)malloc(ret);
		if (buf == NULL) {
			csprintf("No memory.\n");
			return 0;
		}
	}

	char_to_ascii_safe(buf, str, ret);
	ret = websWrite(wp, "%s", buf);

	if (buf != tmp)
		free(buf);

	return ret;
}

static int ej_get_parameter(int eid, webs_t wp, int argc, char_t **argv){
	char *c;
	bool last_was_escaped;
	int ret = 0;

	if (argc < 1){
		websError(wp, 400,
				"get_parameter() used with no arguments, but at least one "
				"argument is required to specify the parameter name\n");
		return -1;
	}

	last_was_escaped = FALSE;

	//char *value = websGetVar(wp, argv[0], "");
	//websWrite(wp, "%s", value);
	for (c = websGetVar(wp, argv[0], ""); *c; c++){
		if (isprint((int)*c) &&
			*c != '"' && *c != '&' && *c != '<' && *c != '>' && *c != '\\' &&  *c != '(' && *c != ')' &&
			((!last_was_escaped) || !isdigit(*c)))
		{
			ret += websWrite(wp, "%c", *c);
			last_was_escaped = FALSE;
		}
		else if ((*c & 0x80) != 0)
		{
			ret += websWrite(wp, "%c", *c);
			last_was_escaped = FALSE;
		}
		else
		{
			ret += websWrite(wp, " ");
			//ret += websWrite(wp, "&#%d", *c);
			//last_was_escaped = TRUE;
		}
	}

	return ret;
}

unsigned int getpeerip(webs_t wp){
	int fd, ret;
	struct sockaddr peer;
	socklen_t peerlen = sizeof(struct sockaddr);
	struct sockaddr_in *sa;

	fd = fileno((FILE *)wp);
	ret = getpeername(fd, (struct sockaddr *)&peer, &peerlen);
	sa = (struct sockaddr_in *)&peer;

	if (!ret){
//		csprintf("peer: %x\n", sa->sin_addr.s_addr);
		return (unsigned int)sa->sin_addr.s_addr;
	}
	else{
		csprintf("error: %d %d \n", ret, errno);
		return 0;
	}
}

extern long uptime(void);

static int login_state_hook(int eid, webs_t wp, int argc, char_t **argv){
	unsigned int ip, login_ip, login_port;
	char ip_str[16], login_ip_str[16];
	time_t login_timestamp_t;
	struct in_addr now_ip_addr, login_ip_addr;
	time_t now;
	const int MAX = 80;
	const int VALUELEN = 18;
	char buffer[MAX], values[6][VALUELEN];

	ip = getpeerip(wp);
	//csprintf("ip = %u\n",ip);

	now_ip_addr.s_addr = ip;
	memset(ip_str, 0, 16);
	strcpy(ip_str, inet_ntoa(now_ip_addr));
//	time(&now);
	now = uptime();

	login_ip = (unsigned int)atoll(nvram_safe_get("login_ip"));
	login_ip_addr.s_addr = login_ip;
	memset(login_ip_str, 0, 16);
	strcpy(login_ip_str, inet_ntoa(login_ip_addr));
//	login_timestamp = (unsigned long)atol(nvram_safe_get("login_timestamp"));
	login_timestamp_t = strtoul(nvram_safe_get("login_timestamp"), NULL, 10);
	login_port = (unsigned int)atol(nvram_safe_get("login_port"));

	FILE *fp = fopen("/proc/net/arp", "r");
	if (fp){
		memset(buffer, 0, MAX);
		memset(values, 0, 6*VALUELEN);

		while (fgets(buffer, MAX, fp)){
			if (strstr(buffer, "br0") && !strstr(buffer, "00:00:00:00:00:00")){
				if (sscanf(buffer, "%s%s%s%s%s%s", values[0], values[1], values[2], values[3], values[4], values[5]) == 6){
					if (!strcmp(values[0], ip_str)){
						break;
					}
				}

				memset(values, 0, 6*VALUELEN);
			}

			memset(buffer, 0, MAX);
		}

		fclose(fp);
	}

	if (ip != 0 && login_ip == ip && login_port != 0 && login_port == http_port) {
		websWrite(wp, "function is_logined() { return 1; }\n");
		websWrite(wp, "function login_ip_dec() { return '%u'; }\n", login_ip);
		websWrite(wp, "function login_ip_str() { return '%s'; }\n", login_ip_str);
		websWrite(wp, "function login_ip_str_now() { return '%s'; }\n", ip_str);

		if (values[3] != NULL)
			websWrite(wp, "function login_mac_str() { return '%s'; }\n", values[3]);
		else
			websWrite(wp, "function login_mac_str() { return ''; }\n");
//		time(&login_timestamp);
		login_timestamp_t = uptime();
	}
	else{
		websWrite(wp, "function is_logined() { return 0; }\n");
		websWrite(wp, "function login_ip_dec() { return '%u'; }\n", login_ip);

		if ((unsigned long)(now-login_timestamp_t) > 60)	//one minitues
			websWrite(wp, "function login_ip_str() { return '0.0.0.0'; }\n");
		else
			websWrite(wp, "function login_ip_str() { return '%s'; }\n", login_ip_str);

		websWrite(wp, "function login_ip_str_now() { return '%s'; }\n", ip_str);

		if (values[3] != NULL)
			websWrite(wp, "function login_mac_str() { return '%s'; }\n", values[3]);
		else
			websWrite(wp, "function login_mac_str() { return ''; }\n");
	}

	return 0;
}
#ifdef RTCONFIG_FANCTRL
static int get_fanctrl_info(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0;
	unsigned int *temp_24 = NULL;
	unsigned int *temp_50 = NULL;
	char buf[WLC_IOCTL_SMLEN];
	char buf2[WLC_IOCTL_SMLEN];

	strcpy(buf, "phy_tempsense");
	strcpy(buf2, "phy_tempsense");

	if ((ret = wl_ioctl("eth1", WLC_GET_VAR, buf, sizeof(buf))))
		return ret;

	if ((ret = wl_ioctl("eth2", WLC_GET_VAR, buf2, sizeof(buf2))))
		return ret;

	temp_24 = (unsigned int *)buf;
	temp_50 = (unsigned int *)buf2;
//	dbG("phy_tempsense 2.4G: %d, 5G: %d\n", *temp_24, *temp_50);

	ret += websWrite(wp, "[\"%d\", \"%d\", \"%d\", \"%s\"]", button_pressed(BTN_FAN), *temp_24, *temp_50, nvram_safe_get("fanctrl_dutycycle_ex"));

	return ret;
}
#endif

#ifdef RTCONFIG_BCMARM
static int get_cpu_temperature(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	int temperature = -1;

	if ((fp = fopen("/proc/dmu/temperature", "r")) != NULL) {
		if (fscanf(fp, "%*s %*s %*s %d", &temperature) != 1)
			temperature = -1;
		fclose(fp);
	}

	return websWrite(wp, "%d", temperature);
}
#endif

static int get_machine_name(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0;
	struct utsname utsn;

	uname(&utsn);

	ret += websWrite(wp, "%s", utsn.machine);

	return ret;
}

int ej_dhcp_leases(int eid, webs_t wp, int argc, char_t **argv)
{
	return 0;
}

int
ej_dhcpLeaseInfo(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	struct in_addr addr4;
	struct in6_addr addr6;
	char line[256];
	char *hwaddr, *ipaddr, *name, *next;
	unsigned int expires;
	int ret = 0;

	if (!nvram_get_int("dhcp_enable_x") || !nvram_match("sw_mode", "1"))
		return ret;

	/* Read leases file */
	if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r")))
		return ret;

	while ((next = fgets(line, sizeof(line), fp)) != NULL) {
		/* line should start from numeric value */
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		hwaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		name = strsep(&next, " ") ? : "";

		if (inet_pton(AF_INET6, ipaddr, &addr6) != 0) {
			/* skip ipv6 leases, thay have no hwaddr, but client id */
			// hwaddr = next ? : "";
			continue;
		} else if (inet_pton(AF_INET, ipaddr, &addr4) == 0)
			continue;

		ret += websWrite(wp,
			"<client>\n"
			    "<mac>value=%s</mac>\n"
			    "<hostname>value=%s</hostname>\n"
			"</client>\n", hwaddr, name);
	}
	fclose(fp);

	return ret;
}

int
ej_dhcpLeaseMacList(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	struct in_addr addr4;
	struct in6_addr addr6;
	char line[256];
	char *hwaddr, *ipaddr, *name, *next;
	unsigned int expires;
	int ret = 0;
	int name_len;
	char tmp[MAX_LINE_SIZE];
	char *buf = tmp;

	if (!nvram_get_int("dhcp_enable_x") || !nvram_match("sw_mode", "1")){
		ret += websWrite(wp, "[['', '']]");
		return ret;
	}

	/* Read leases file */
	if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r"))){
		ret += websWrite(wp, "[['', '']]");
		return ret;
	}

	ret += websWrite(wp, "[");
	while ((next = fgets(line, sizeof(line), fp)) != NULL) {
		/* line should start from numeric value */
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		hwaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		name = strsep(&next, " ") ? : "";

		if (inet_pton(AF_INET6, ipaddr, &addr6) != 0) {
			/* skip ipv6 leases, thay have no hwaddr, but client id */
			// hwaddr = next ? : "";
			continue;
		} else if (inet_pton(AF_INET, ipaddr, &addr4) == 0)
			continue;

		/* each char expands to %XX at max */
		name_len = strlen(name) * sizeof(char)*3 + sizeof(char);

		if (name_len > sizeof(tmp)) {
			buf = (char *)malloc(name_len);
			if (buf == NULL) {
				csprintf("No memory.\n");
				return 0;
			}
		}

		char_to_ascii_safe(buf, name, name_len);

		ret += websWrite(wp,"['%s', '%s'],", hwaddr, buf);
	}
	ret += websWrite(wp, "['','']]");
	fclose(fp);

	return ret;
}

#if 0
int
ej_lan_leases(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	struct in_addr addr4;
	struct in6_addr addr6;
	char line[256], timestr[sizeof("999:59:59")];
	char *hwaddr, *ipaddr, *name, *next;
	unsigned int expires;
	int ret = 0;

	ret += websWrite(wp, "%-32s %-15s %-17s %-9s\n",
		"Hostname", "IP Address", "MAC Address", "Expires");

	if (!nvram_get_int("dhcp_enable_x"))
		return ret;

	/* Refresh lease file to get actual expire time */
/*	nvram_set("flush_dhcp_lease", "1"); */
	killall("dnsmasq", SIGUSR2);
	sleep (1);
/*	while(nvram_match("flush_dhcp_lease", "1"))
		sleep(1); */

	/* Read leases file */
	if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r")))
		return ret;

	while ((next = fgets(line, sizeof(line), fp)) != NULL) {
		/* line should start from numeric value */
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		hwaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		name = strsep(&next, " ") ? : "";

		if (strlen(name) > 32)
		{
			strcpy(name + 29, "...");
			name[32] = '\0';
		}

		if (inet_pton(AF_INET6, ipaddr, &addr6) != 0) {
			/* skip ipv6 leases, thay have no hwaddr, but client id */
			// hwaddr = next ? : "";
			continue;
		} else if (inet_pton(AF_INET, ipaddr, &addr4) == 0)
			continue;

		if (expires) {
			snprintf(timestr, sizeof(timestr), "%u:%02u:%02u",
			    expires / 3600,
			    expires % 3600 / 60,
			    expires % 60);
		}

		ret += websWrite(wp, "%-32s %-15s %-17s %-9s\n",
			name, ipaddr, hwaddr,
			expires ? timestr : "Static");
	}
	fclose(fp);

	return ret;
}
#endif

int
ej_IP_dhcpLeaseInfo(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	struct in_addr addr4;
	struct in6_addr addr6;
	char line[256];
	char *hwaddr, *ipaddr, *name, *next;
	unsigned int expires;
	int ret = 0;

	if (!nvram_get_int("dhcp_enable_x") || !nvram_match("sw_mode", "1"))
		return (ret + websWrite(wp, "\"\""));

	/* Read leases file */
	if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r")))
		return (ret + websWrite(wp, "\"\""));

	ret += websWrite(wp, "[");
	while ((next = fgets(line, sizeof(line), fp)) != NULL) {
		/* line should start from numeric value */
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		hwaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		name = strsep(&next, " ") ? : "";

		if (inet_pton(AF_INET6, ipaddr, &addr6) != 0) {
			/* skip ipv6 leases, thay have no hwaddr, but client id */
			// hwaddr = next ? : "";
			continue;
		} else if (inet_pton(AF_INET, ipaddr, &addr4) == 0)
			continue;

		ret += websWrite(wp,"['%s', '%s'],", ipaddr, name);
	}
	ret += websWrite(wp, "['', '']];");
	fclose(fp);

	return ret;
}

#ifdef RTCONFIG_IPV6
#define DHCP_LEASE_FILE         "/var/lib/misc/dnsmasq.leases"
#define IPV6_CLIENT_NEIGH	"/tmp/ipv6_neigh"
#define IPV6_CLIENT_INFO	"/tmp/ipv6_client_info"
#define	IPV6_CLIENT_LIST	"/tmp/ipv6_client_list"
#define	MAC			1
#define	HOSTNAME		2
#define	IPV6_ADDRESS		3
#define BUFSIZE			8192

static int compare_back(FILE *fp, int current_line, char *buffer);
static int check_mac_previous(char *mac);
static char *value(FILE *fp, int line, int token);
static void find_hostname_by_mac(char *mac, char *hostname);
static int total_lines = 0;

/* Init File and clear the content */
void init_file(char *file)
{
	FILE *fp;

	if ((fp = fopen(file ,"w")) == NULL) {
		_dprintf("can't open %s: %s", file,
			strerror(errno));
	}

	fclose(fp);
}

void save_file(const char *file, const char *fmt, ...)
{
	char buf[BUFSIZE];
	va_list args;
	FILE *fp;

	if ((fp = fopen(file ,"a")) == NULL) {
		_dprintf("can't open %s: %s", file,
			strerror(errno));
	}

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	va_start(args, fmt);
	fprintf(fp, "%s", buf);
	va_end(args);

	fclose(fp);
}

static char *get_stok(char *str, char *dest, char delimiter)
{
	char *p;

	p = strchr(str, delimiter);
	if (p) {
		if (p == str)
			*dest = '\0';
		else
			strncpy(dest, str, p-str);

		p++;
	} else
		strcpy(dest, str);

	return p;
}

static char *value(FILE *fp, int line, int token)
{
	int i;
	static char temp[BUFSIZE], buffer[BUFSIZE];
	char *ptr;
	int temp_len;

	fseek(fp, 0, SEEK_SET);
	for(i = 0; i < line; i++) {
		memset(temp, 0, sizeof(temp));
		fgets(temp, sizeof(temp), fp);
		temp_len = strlen(temp);
		if (temp_len && temp[temp_len-1] == '\n')
			temp[temp_len-1] = '\0';
	}
	memset(buffer, 0, sizeof(buffer));
	switch (token) {
		case HOSTNAME:
			get_stok(temp, buffer, ' ');
			break;
		case MAC:
			ptr = get_stok(temp, buffer, ' ');
			if (ptr)
				get_stok(ptr, buffer, ' ');
			break;
		case IPV6_ADDRESS:
			ptr = get_stok(temp, buffer, ' ');
			if (ptr) {
				ptr = get_stok(ptr, buffer, ' ');
				if (ptr)
					ptr = get_stok(ptr, buffer, ' ');
			}
			break;
		default:
			_dprintf("error option\n");
			strcpy(buffer, "ERROR");
			break;
	}

	return buffer;
}

static int check_mac_previous(char *mac)
{
	FILE *fp;
	char temp[BUFSIZE];
	memset(temp, 0, sizeof(temp));

	if ((fp = fopen(IPV6_CLIENT_LIST, "r")) == NULL)
	{
		_dprintf("can't open %s: %s", IPV6_CLIENT_LIST,
			strerror(errno));

		return 0;
	}

	while (fgets(temp, BUFSIZE, fp)) {
		if (strstr(temp, mac)) {
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);

	return 0;
}

static int compare_back(FILE *fp, int current_line, char *buffer)
{
	int i = 0;
	char mac[32], compare_mac[32];

	buffer[strlen(buffer) -1] = '\0';
	strcpy(mac, value(fp, current_line, MAC));

	if (check_mac_previous(mac))
		return 0;

	for(i = 0; i<(total_lines - current_line); i++) {
		strcpy(compare_mac, value(fp, current_line + 1 + i, MAC));
		if (strcmp(mac, compare_mac) == 0) {
			strcat(buffer, ",");
			strcat(buffer, value(fp, current_line + 1 + i, IPV6_ADDRESS));
		}
	}
	save_file(IPV6_CLIENT_LIST, "%s\n", buffer);

	return 0;
}

static void find_hostname_by_mac(char *mac, char *hostname)
{
	FILE *fp;
	unsigned int expires;
	char *macaddr, *ipaddr, *host_name, *next;
	char line[256];

	if ((fp = fopen(DHCP_LEASE_FILE, "r")) == NULL)
	{
		_dprintf("can't open %s: %s", DHCP_LEASE_FILE,
			strerror(errno));

		goto END;
	}

	while ((next = fgets(line, sizeof(line), fp)) != NULL)
	{
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		macaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		host_name = strsep(&next, " ") ? : "";

		if (strncasecmp(macaddr, mac, 17) == 0) {
			fclose(fp);
			strcpy(hostname, host_name);
			return;
		}

		memset(macaddr, 0, sizeof(macaddr));
		memset(ipaddr, 0, sizeof(ipaddr));
		memset(host_name, 0, sizeof(host_name));
	}
	fclose(fp);
END:
	strcpy(hostname, "<unknown>");
}

void get_ipv6_client_info()
{
	FILE *fp;
	char buffer[128], ipv6_addr[128], mac[32];
	char *ptr_end, hostname[64];
	doSystem("ip -f inet6 neigh show dev %s > %s", nvram_safe_get("lan_ifname"), IPV6_CLIENT_NEIGH);
	usleep(1000);

	if ((fp = fopen(IPV6_CLIENT_NEIGH, "r")) == NULL)
	{
		_dprintf("can't open %s: %s", IPV6_CLIENT_NEIGH,
			strerror(errno));

		return;
	}

	init_file(IPV6_CLIENT_INFO);
	while (fgets(buffer, 128, fp)) {
		int temp_len = strlen(buffer);
		if (temp_len && buffer[temp_len-1] == '\n')
			buffer[temp_len-1] = '\0';
		if ((ptr_end = strstr(buffer, "lladdr")))
		{
			ptr_end = ptr_end - 1;
			memset(ipv6_addr, 0, sizeof(ipv6_addr));
			strncpy(ipv6_addr, buffer, ptr_end - buffer);
			ptr_end = ptr_end + 8;
			memset(mac, 0, sizeof(mac));
			strncpy(mac, ptr_end, 17);
			find_hostname_by_mac(mac, hostname);
			if ( (ipv6_addr[0] == '2' || ipv6_addr[0] == '3')
				&& ipv6_addr[0] != ':' && ipv6_addr[1] != ':'
				&& ipv6_addr[2] != ':' && ipv6_addr[3] != ':')
				save_file(IPV6_CLIENT_INFO, "%s %s %s\n", hostname, mac, ipv6_addr);
		}

		memset(buffer, 0, sizeof(buffer));
	}
	fclose(fp);
}

void get_ipv6_client_list(void)
{
	FILE *fp;
	int line_index = 1;
	char temp[BUFSIZE];
	memset(temp, 0, sizeof(temp));
	init_file(IPV6_CLIENT_LIST);

	if ((fp = fopen(IPV6_CLIENT_INFO, "r")) == NULL)
	{
		_dprintf("can't open %s: %s", IPV6_CLIENT_INFO,
			strerror(errno));

		return;
	}

	total_lines = 0;
	while (fgets(temp, BUFSIZE, fp))
		total_lines++;
	fseek(fp, 0, SEEK_SET);
	memset(temp, 0, sizeof(temp));

	while (fgets(temp, BUFSIZE, fp)) {
		compare_back(fp, line_index, temp);
		value(fp, line_index, MAC);
		line_index++;
	}
	fclose(fp);

	line_index = 1;
}
#if 0
static int ipv6_client_numbers(void)
{
	FILE *fp;
	int numbers = 0;
	char temp[BUFSIZE];

	if ((fp = fopen(IPV6_CLIENT_LIST, "r")) == NULL)
	{
		_dprintf("can't open %s: %s", IPV6_CLIENT_LIST,
			strerror(errno));

		return 0;
	}

	while (fgets(temp, BUFSIZE, fp))
		numbers++;
	fclose(fp);

	return numbers;
}
#endif

#if 0
int
ej_lan_ipv6_network(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	char buf[64+32+8192+1];
	char *hostname, *macaddr, ipaddrs[8192+1];
	char ipv6_dns_str[1024];
	char *wan_type, *wan_dns, *p;
	int service, i, ret = 0;

	if (!(ipv6_enabled() && is_routing_enabled())) {
		ret += websWrite(wp, "IPv6 disabled\n");
		return ret;
	}

	service = get_ipv6_service();
	switch (service) {
	case IPV6_NATIVE_DHCP:
		wan_type = "Native with DHCP-PD"; break;
	case IPV6_6TO4:
		wan_type = "Tunnel 6to4"; break;
	case IPV6_6IN4:
		wan_type = "Tunnel 6in4"; break;
	case IPV6_6RD:
		wan_type = "Tunnel 6rd"; break;
	case IPV6_MANUAL:
		wan_type = "Static"; break;
	default:
		wan_type = "Disabled"; break;
	}

	ret += websWrite(wp, "%30s: %s\n", "IPv6 Connection Type", wan_type);
	ret += websWrite(wp, "%30s: %s\n", "WAN IPv6 Address",
			 getifaddr((char *) get_wan6face(), AF_INET6, GIF_PREFIXLEN) ? : nvram_safe_get("ipv6_rtr_addr"));
	ret += websWrite(wp, "%30s: %s\n", "WAN IPv6 Gateway",
			 ipv6_gateway_address() ? : "");
	ret += websWrite(wp, "%30s: %s/%d\n", "LAN IPv6 Address",
			 nvram_safe_get("ipv6_rtr_addr"), nvram_get_int("ipv6_prefix_length"));
	ret += websWrite(wp, "%30s: %s\n", "LAN IPv6 Link-Local Address",
			 getifaddr(nvram_safe_get("lan_ifname"), AF_INET6, GIF_LINKLOCAL | GIF_PREFIXLEN) ? : "");
	if (service == IPV6_NATIVE_DHCP) {
		ret += websWrite(wp, "%30s: %s\n", "DHCP-PD",
				 nvram_get_int("ipv6_dhcp_pd") ? "Enabled" : "Disabled");
	}
	ret += websWrite(wp, "%30s: %s/%d\n", "LAN IPv6 Prefix",
			 nvram_safe_get("ipv6_prefix"), nvram_get_int("ipv6_prefix_length"));

	if (service == IPV6_NATIVE_DHCP &&
	    nvram_get_int("ipv6_dnsenable")) {
		wan_dns = nvram_safe_get("ipv6_get_dns");
	} else {
		char nvname[sizeof("ipv6_dnsXXX")];
		char *next = ipv6_dns_str;

		ipv6_dns_str[0] = '\0';
		for (i = 1; i <= 3; i++) {
			snprintf(nvname, sizeof(nvname), "ipv6_dns%d", i);
			wan_dns = nvram_safe_get(nvname);
			if (*wan_dns)
				next += sprintf(next, *ipv6_dns_str ? " %s" : "%s", wan_dns);
		}
		wan_dns = ipv6_dns_str;
	}
	ret += websWrite(wp, "%30s: %s\n", "DNS Address", wan_dns);

	ret += websWrite(wp, "\n\nIPv6 LAN Devices List\n");
	ret += websWrite(wp, "-------------------------------------------------------------------\n");
	ret += websWrite(wp, "%-32s %-17s %-39s\n",
			 "Hostname", "MAC Address", "IPv6 Address");

	/* Refresh lease file to get actual expire time */
	killall("dnsmasq", SIGUSR2);
	usleep(100 * 1000);

	get_ipv6_client_info();
	get_ipv6_client_list();

	if ((fp = fopen(IPV6_CLIENT_LIST, "r")) == NULL) {
		_dprintf("can't open %s: %s", IPV6_CLIENT_LIST, strerror(errno));
		return ret;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char *ptr = buf;

		ptr = strsep(&ptr, "\n");
		hostname = strsep(&ptr, " ");
		macaddr = strsep(&ptr, " ");
		if (!macaddr || *macaddr == '\0' ||
		    !ptr || *ptr == '\0')
			continue;

		if (strlen(hostname) > 32)
			sprintf(hostname + 29, "...");

		ipaddrs[0] = '\0';
		p = ipaddrs;
		while (ptr && *ptr) {
			char *next = strsep(&ptr, ",\n");
			if (next && *next)
				p += snprintf(p, sizeof(ipaddrs) + ipaddrs - p, "%s%s", *ipaddrs ? ", " : "", next);
		}

		ret += websWrite(wp, "%-32s %-17s %-39s\n",
				 hostname, macaddr, ipaddrs);
	}
	fclose(fp);

	return ret;
}
#endif

#if 0 /* temporary till httpd route table redo */
static void INET6_displayroutes(webs_t wp)
{
	char addr6[128], *naddr6;
	/* In addr6x, we store both 40-byte ':'-delimited ipv6 addresses.
	 * We read the non-delimited strings into the tail of the buffer
	 * using fscanf and then modify the buffer by shifting forward
	 * while inserting ':'s and the nul terminator for the first string.
	 * Hence the strings are at addr6x and addr6x+40.  This generates
	 * _much_ less code than the previous (upstream) approach. */
	char addr6x[80];
	char iface[16], flags[16];
	int iflags, metric, refcnt, use, prefix_len, slen;
	struct sockaddr_in6 snaddr6;
	int ret = 0;

	FILE *fp = fopen("/proc/net/ipv6_route", "r");

	ret += websWrite(wp, "\nIPv6 routing table\n%-44s%-40s"
			  "Flags Metric Ref    Use Iface\n",
			  "Destination", "Next Hop");

	while (1) {
		int r;
		r = fscanf(fp, "%32s%x%*s%x%32s%x%x%x%x%s\n",
				addr6x+14, &prefix_len, &slen, addr6x+40+7,
				&metric, &use, &refcnt, &iflags, iface);
		if (r != 9) {
			if ((r < 0) && feof(fp)) { /* EOF with no (nonspace) chars read. */
				break;
			}
 ERROR:
			perror("fscanf");
			return;
		}

		/* Do the addr6x shift-and-insert changes to ':'-delimit addresses.
		 * For now, always do this to validate the proc route format, even
		 * if the interface is down. */
		{
			int i = 0;
			char *p = addr6x+14;

			do {
				if (!*p) {
					if (i == 40) { /* nul terminator for 1st address? */
						addr6x[39] = 0;	/* Fixup... need 0 instead of ':'. */
						++p;	/* Skip and continue. */
						continue;
					}
					goto ERROR;
				}
				addr6x[i++] = *p++;
				if (!((i+1) % 5)) {
					addr6x[i++] = ':';
				}
			} while (i < 40+28+7);
		}

		if (!(iflags & RTF_UP)) { /* Skip interfaces that are down. */
			continue;
		}

		ipv6_set_flags(flags, (iflags & IPV6_MASK));

		r = 0;
		do {
			inet_pton(AF_INET6, addr6x + r,
					  (struct sockaddr *) &snaddr6.sin6_addr);
			snaddr6.sin6_family = AF_INET6;
			naddr6 = INET6_rresolve((struct sockaddr_in6 *) &snaddr6,
						   0x0fff /* Apparently, upstream never resolves. */
						   );

			if (!r) {			/* 1st pass */
				snprintf(addr6, sizeof(addr6), "%s/%d", naddr6, prefix_len);
				r += 40;
				free(naddr6);
			} else {			/* 2nd pass */
				/* Print the info. */
				ret += websWrite(wp, "%-43s %-39s %-5s %-6d %-2d %7d %-8s\n",
						addr6, naddr6, flags, metric, refcnt, use, iface);
				free(naddr6);
				break;
			}
		} while (1);
	}

	return;
}
#endif
#endif

#if 0
static int ipv4_route_table(webs_t wp)
{
	FILE *fp;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char buf[256], *dev, *sflags, *str;
	struct in_addr dest, gateway, mask;
	int flags, ref, use, metric;
	int unit, ret = 0;

	ret += websWrite(wp, "%-16s%-16s%-16s%s\n",
			 "Destination", "Gateway", "Genmask",
			 "Flags Metric Ref    Use Iface");

	fp = fopen("/proc/net/route", "r");
	if (fp == NULL)
		return 0;

	while ((str = fgets(buf, sizeof(buf), fp)) != NULL) {
		dev = strsep(&str, " \t");
		if (!str || dev == str)
			continue;
		if (sscanf(str, "%x%x%x%d%u%d%x", &dest.s_addr, &gateway.s_addr,
			   &flags, &ref, &use, &metric, &mask.s_addr) != 7)
			continue;

		/* Parse flags, reuse buf */
		sflags = str;
		if (flags & RTF_UP)
			*str++ = 'U';
		if (flags & RTF_GATEWAY)
			*str++ = 'G';
		if (flags & RTF_HOST)
			*str++ = 'H';
		*str++ = '\0';

		/* Skip interfaces here */
		if (strcmp(dev, "lo") == 0)
			continue;

		/* Replace known interfaces with LAN/WAN/MAN */
		if (nvram_match("lan_ifname", dev)) /* br0, wl0, etc */
			dev = "LAN";
		else {
			/* Tricky, it's better to move wan_ifunit/wanx_ifunit to shared instead */
			for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++) {
				wan_prefix(unit, prefix);
				if (nvram_match(strcat_r(prefix, "pppoe_ifname", tmp), dev)) {
					dev = "WAN";
					break;
				}
				if (nvram_match(strcat_r(prefix, "ifname", tmp), dev)) {
					char *wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
					dev = (strcmp(wan_proto, "dhcp") == 0 ||
					       strcmp(wan_proto, "static") == 0 ) ? "WAN" : "MAN";
					break;
				}
			}
		}

		ret += websWrite(wp, "%-16s", dest.s_addr == INADDR_ANY ? "default" : inet_ntoa(dest));
		ret += websWrite(wp, "%-16s", gateway.s_addr == INADDR_ANY ? "*" : inet_ntoa(gateway));
		ret += websWrite(wp, "%-16s%-6s%-6d %-2d %7d %s\n",
				 inet_ntoa(mask), sflags, metric, ref, use, dev);
	}
	fclose(fp);

	return ret;
}
#endif

#if 0
int
ej_route_table(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret;
#ifdef RTCONFIG_IPV6
#if 1 /* temporary till httpd route table redo */
	FILE *fp;
	char buf[256];
	unsigned int fl = 0;
	int found = 0;
#endif
#endif

	ret = ipv4_route_table(wp);

#ifdef RTCONFIG_IPV6
	if (get_ipv6_service() != IPV6_DISABLED) {

#if 1 /* temporary till httpd route table redo */
	if ((fp = fopen("/proc/net/if_inet6", "r")) == (FILE*)0)
		return ret;

	while (fgets(buf, 256, fp) != NULL)
	{
		if(strstr(buf, "br0") == (char*) 0)
			continue;

		if (sscanf(buf, "%*s %*02x %*02x %02x", &fl) != 1)
			continue;

		if ((fl & 0xF0) == 0x20)
		{
			/* Link-Local Address is ready */
			found = 1;
			break;
		}
	}
	fclose(fp);

	if (found)
		INET6_displayroutes(wp);
#endif
	}
#endif

	return ret;
}
#endif

static int ej_get_arp_table(int eid, webs_t wp, int argc, char_t **argv){
	const int MAX = 80;
	const int FIELD_NUM = 6;
	const int VALUELEN = 18;
	char buffer[MAX], values[FIELD_NUM][VALUELEN];
	int num, firstRow;

	FILE *fp = fopen("/proc/net/arp", "r");
	if (fp){
		memset(buffer, 0, MAX);
		memset(values, 0, FIELD_NUM*VALUELEN);

		firstRow = 1;
		while (fgets(buffer, MAX, fp)){
			if (strstr(buffer, "br0") && !strstr(buffer, "00:00:00:00:00:00")){
				if (firstRow == 1)
					firstRow = 0;
				else
					websWrite(wp, ", ");

				if ((num = sscanf(buffer, "%s%s%s%s%s%s", values[0], values[1], values[2], values[3], values[4], values[5])) == FIELD_NUM){
					websWrite(wp, "[\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"]", values[0], values[1], values[2], values[3], values[4], values[5]);
				}

				memset(values, 0, FIELD_NUM*VALUELEN);
			}

			memset(buffer, 0, MAX);
		}

		fclose(fp);
	}

	return 0;
}

#if defined(CONFIG_BCMWL5) || (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) || defined(RTCONFIG_QCA)
static int ej_get_ap_info(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	char buf[MAX_LINE_SIZE];
	int ret_l = 0;
	unsigned int i_l, len_l;
	int i;
	int lock;

	// get info from file generated by wlc_scan
	if(nvram_get_int("wlc_scan_state")!=WLCSCAN_STATE_FINISHED){
		ret_l += websWrite(wp, "[]");
		return ret_l;
	}
	lock = file_lock("sitesurvey");
	fp = fopen("/tmp/apscan_info.txt", "r");
	if( fp == NULL ){
		csprintf("[httpd] open apscan_info.txt error\n");
		ret_l += websWrite(wp, "[]");
		return ret_l;
	}else{
		//ret_l += websWrite(wp, "[");
		i = 0;
		while (fgets(buf, MAX_LINE_SIZE, fp)){
			if(i>0) ret_l += websWrite(wp, ",");
			len_l = strlen(buf);
			i_l = 0;
			while (i_l < len_l + 1){
				if(buf[i_l] == '\n') buf[i_l] = '\0';
				i_l++;
			}
			ret_l += websWrite(wp, "[");
			ret_l += websWrite(wp, "%s", buf);
			ret_l += websWrite(wp, "]");
			memset(buf, 0, MAX_LINE_SIZE);
			i++;
		}
		//ret_l += websWrite(wp, "];");	/* insert empty for last record */
	}
	fclose(fp);
	file_unlock(lock);
	return ret_l;

}
#endif

//2011.03 Yau add for new networkmap
static int ej_get_client_detail_info(int eid, webs_t wp, int argc, char_t **argv){
	int i, shm_client_info_id;
	void *shared_client_info=(void *) 0;
	char output_buf[256], dev_name[16];
	P_CLIENT_DETAIL_INFO_TABLE p_client_info_tab;
	int lock;
	char devname[LINE_SIZE], character;
	int j, len;

	lock = file_lock("networkmap");
	shm_client_info_id = shmget((key_t)1001, sizeof(CLIENT_DETAIL_INFO_TABLE), 0666|IPC_CREAT);
	if (shm_client_info_id == -1){
	    fprintf(stderr,"shmget failed\n");
	    file_unlock(lock);
	    return 0;
	}

	shared_client_info = shmat(shm_client_info_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr,"shmat failed\n");
		file_unlock(lock);
		return 0;
	}

	p_client_info_tab = (P_CLIENT_DETAIL_INFO_TABLE)shared_client_info;
	for(i=0; i<p_client_info_tab->ip_mac_num; i++) {
		if(strcmp(p_client_info_tab->user_define[i], ""))
			strlcpy(dev_name, p_client_info_tab->user_define[i], sizeof (dev_name));
		else
			strlcpy(dev_name, p_client_info_tab->device_name[i], sizeof (dev_name));

		memset(output_buf, 0, 256);
		memset(devname, 0, LINE_SIZE);

	    if(p_client_info_tab->exist[i]==1) {
		len = strlen( (char *) p_client_info_tab->device_name[i]);
		for (j=0; (j < len) && (j < LINE_SIZE-1); j++) {
			character = dev_name[j];
			if ((isalnum(character)) || (character == ' ') || (character == '-') || (character == '_'))
				devname[j] = character;
			else
				devname[j] = ' ';
		}

#ifdef RTCONFIG_BWDPI
		sprintf(output_buf, "<%d>%s>%d.%d.%d.%d>%02X:%02X:%02X:%02X:%02X:%02X>%d>%d>%d>%s>%s>%s>%s>%s",
#else
		sprintf(output_buf, "<%d>%s>%d.%d.%d.%d>%02X:%02X:%02X:%02X:%02X:%02X>%d>%d>%d>%s",
#endif
		p_client_info_tab->type[i],
		devname,
		p_client_info_tab->ip_addr[i][0],p_client_info_tab->ip_addr[i][1],
		p_client_info_tab->ip_addr[i][2],p_client_info_tab->ip_addr[i][3],
		p_client_info_tab->mac_addr[i][0],p_client_info_tab->mac_addr[i][1],
		p_client_info_tab->mac_addr[i][2],p_client_info_tab->mac_addr[i][3],
		p_client_info_tab->mac_addr[i][4],p_client_info_tab->mac_addr[i][5],
		p_client_info_tab->http[i],
		p_client_info_tab->printer[i],
		p_client_info_tab->itune[i],
                p_client_info_tab->apple_model[i]
#ifdef RTCONFIG_BWDPI
		,p_client_info_tab->bwdpi_host[i]
		,p_client_info_tab->bwdpi_vendor[i]
		,p_client_info_tab->bwdpi_type[i]
		,p_client_info_tab->bwdpi_device[i]
#endif
		);
		websWrite(wp, output_buf);
//		if(i < p_client_info_tab->ip_mac_num-1)
//			websWrite(wp, ",");
	    }
	}
	shmdt(shared_client_info);
	file_unlock(lock);

	return 0;
}

// for detect static IP's client.
static int ej_get_static_client(int eid, webs_t wp, int argc, char_t **argv){
	FILE *fp = fopen("/tmp/static_ip.inf", "r");
	char buf[1024], *head, *tail, field[1024];
	int len, i, first_client, first_field;

	if (fp == NULL){
		csprintf("Don't detect static clients!\n");
		return 0;
	}

	memset(buf, 0, 1024);

	first_client = 1;
	while (fgets(buf, 1024, fp)){
		if (first_client == 1)
			first_client = 0;
		else
			websWrite(wp, ", ");

		len = strlen(buf);
		buf[len-1] = ',';
		head = buf;
		first_field = 1;
		for (i = 0; i < 7; ++i){
			tail = strchr(head, ',');
			if (tail != NULL){
				memset(field, 0, 1024);
				strncpy(field, head, (tail-head));
			}

			if (first_field == 1){
				first_field = 0;
				websWrite(wp, "[");
			}
			else
				websWrite(wp, ", ");

			if (strlen(field) > 0)
				websWrite(wp, "\"%s\"", field);
			else
				websWrite(wp, "null");

			//if (tail+1 != NULL)
				head = tail+1;

			if (i == 6)
				websWrite(wp, "]");
		}

		memset(buf, 0, 1024);
	}

	fclose(fp);
	return 0;
}

static int yadns_servers_hook(int eid, webs_t wp, int argc, char_t **argv)
{
#ifdef RTCONFIG_YANDEXDNS
	int yadns_mode = nvram_get_int("yadns_enable_x") ? nvram_get_int("yadns_mode") : YADNS_DISABLED;
	char *server[2];
	int i, count;

	if (yadns_mode != YADNS_DISABLED) {
		count = get_yandex_dns(AF_INET, yadns_mode, server, sizeof(server)/sizeof(server[0]));
		for (i = 0; i < count; i++)
			websWrite(wp, i ? ",\"%s\"" : "\"%s\"", server[i]);
	}
#endif
	return 0;
}

static int yadns_clients_hook(int eid, webs_t wp, int argc, char_t **argv)
{
#ifdef RTCONFIG_YANDEXDNS
	char *name, *mac, *mode, *enable;
	char *nv, *nvp, *b;
	int i, dnsmode, clients[YADNS_COUNT];

	memset(&clients, 0, sizeof(clients));

	if (nvram_get_int("yadns_enable_x")) {
		nv = nvp = strdup(nvram_safe_get("yadns_rulelist"));
		while (nv && (b = strsep(&nvp, "<")) != NULL) {
			if (vstrsep(b, ">", &name, &mac, &mode, &enable) < 3)
				continue;
			if (enable && atoi(enable) == 0)
				continue;
			if (!*mac || !*mode)
				continue;
			dnsmode = atoi(mode);
			/* Skip incorrect levels */
			if (dnsmode < YADNS_FIRST || dnsmode >= YADNS_COUNT)
				continue;
			clients[dnsmode]++;
		}
		free(nv);
	}

	for (i = YADNS_FIRST; i < YADNS_COUNT; i++)
		websWrite(wp, (i == YADNS_FIRST) ? "%d" : ",%d", clients[i]);
#endif
	return 0;
}

static int ej_get_changed_status(int eid, webs_t wp, int argc, char_t **argv){
	char *arp_info = read_whole_file("/proc/net/arp");
#ifdef RTCONFIG_USB
	char *disk_info = read_whole_file(PARTITION_FILE);
	char *mount_info = read_whole_file(MOUNT_FILE);
#endif
	u32 arp_info_len, disk_info_len, mount_info_len;
//	u32 arp_change, disk_change;

	//printf("get changed status\n");	// tmp test

	if (arp_info != NULL){
		arp_info_len = strlen(arp_info);
		free(arp_info);
	}
	else
		arp_info_len = 0;

#ifdef RTCONFIG_USB
	if (disk_info != NULL){
		disk_info_len = strlen(disk_info);
		free(disk_info);
	}
	else
		disk_info_len = 0;

	if (mount_info != NULL){
		mount_info_len = strlen(mount_info);
		free(mount_info);
	}
	else
		mount_info_len = 0;
#endif

	websWrite(wp, "function get_client_status_changed(){\n");
	websWrite(wp, "    return %u;\n", arp_info_len);
	websWrite(wp, "}\n\n");

#ifdef RTCONFIG_USB
	websWrite(wp, "function get_disk_status_changed(){\n");
	websWrite(wp, "    return %u;\n", disk_info_len);
	websWrite(wp, "}\n\n");

	websWrite(wp, "function get_mount_status_changed(){\n");
	websWrite(wp, "    return %u;\n", mount_info_len);
	websWrite(wp, "}\n\n");
#endif
	return 0;
}

#ifdef RTCONFIG_USB
static int ej_show_usb_path(int eid, webs_t wp, int argc, char_t **argv){
	DIR *bus_usb;
	struct dirent *interface;
	char usb_port[8], port_path[8], *ptr;
	int port_num, port_order, hub_order;
	char all_usb_path[MAX_USB_PORT][MAX_USB_HUB_PORT][3][16]; // MAX USB hub port number is 6.
	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
	int port_set, got_port, got_hub;

	if((bus_usb = opendir(USB_DEVICE_PATH)) == NULL)
		return -1;

	memset(all_usb_path, 0, MAX_USB_PORT*MAX_USB_HUB_PORT*3*16);

	while((interface = readdir(bus_usb)) != NULL){
		if(interface->d_name[0] == '.')
			continue;

		if(!isdigit(interface->d_name[0]))
			continue;

		if(strchr(interface->d_name, ':'))
			continue;

		if(get_usb_port_by_string(interface->d_name, usb_port, 8) == NULL)
			continue;

		port_num = get_usb_port_number(usb_port);
		port_order = port_num-1;
		if((ptr = strchr(interface->d_name+strlen(usb_port), '.')) != NULL)
			hub_order = atoi(ptr+1);
		else
			hub_order = 0;

		if(get_path_by_node(interface->d_name, port_path, 8) == NULL)
			continue;

		strncpy(all_usb_path[port_order][hub_order][0], port_path, 16);
		snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);
		strncpy(all_usb_path[port_order][hub_order][1], nvram_safe_get(prefix), 16);
		strncpy(all_usb_path[port_order][hub_order][2], nvram_safe_get(strcat_r(prefix, "_speed", tmp)), 16);
	}
	closedir(bus_usb);

	port_set = got_port = got_hub = 0;

	websWrite(wp, "[");
	for(port_order = 0; port_order < MAX_USB_PORT; ++port_order){
		got_hub = 0;
		for(hub_order = 0; hub_order < MAX_USB_HUB_PORT; ++hub_order){
			if(strlen(all_usb_path[port_order][hub_order][1]) > 0){
				if(!port_set){ // port range.
					port_set = 1;

					if(!got_port)
						got_port = 1;
					else
						websWrite(wp, ", ");

					websWrite(wp, "[");
				}

				if(!got_hub)
					got_hub = 1;
				else
					websWrite(wp, ", ");

				websWrite(wp, "[\"%s\", \"%s\", \"%s\"]", all_usb_path[port_order][hub_order][0], all_usb_path[port_order][hub_order][1], all_usb_path[port_order][hub_order][2]);
			}
		}

		if(port_set){ // port range.
			port_set = 0;
			websWrite(wp, "]");
		}
	}
	websWrite(wp, "]");

	return 0;
}

static int ej_disk_pool_mapping_info(int eid, webs_t wp, int argc, char_t **argv){
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	int first;
	char *Ptr;

	disks_info = read_disk_data();
	if (disks_info == NULL){
		websWrite(wp, "%s", initial_disk_pool_mapping_info());
		return -1;
	}

	websWrite(wp, "function total_disk_sizes(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%llu\"", follow_disk->size_in_kilobytes);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function disk_interface_names(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"usb\"");
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_names(){\n");
	websWrite(wp, "    return [");

	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			if (follow_partition->mount_point == NULL){
				websWrite(wp, "\"%s\"", follow_partition->device);
				continue;
			}

			Ptr = rindex(follow_partition->mount_point, '/');
			if (Ptr == NULL){
				websWrite(wp, "\"unknown\"");
				continue;
			}

			++Ptr;
			websWrite(wp, "\"%s\"", Ptr);
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_devices(){\n");
	websWrite(wp, "    return [");

	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "\"%s\"", follow_partition->device);
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_types(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			if (follow_partition->mount_point == NULL){
				websWrite(wp, "\"unknown\"");
				continue;
			}

			websWrite(wp, "\"%s\"", follow_partition->file_system);
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_mirror_counts(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "0");
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_status(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			if (follow_partition->mount_point == NULL){
				websWrite(wp, "\"unmounted\"");
				continue;
			}

			//if (strcmp(follow_partition->file_system, "ntfs") == 0)
			//	websWrite(wp, "\"ro\"");
			//else
			websWrite(wp, "\"rw\"");
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_kilobytes(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "%llu", follow_partition->size_in_kilobytes);
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_encryption_password_is_missing(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "\"no\"");
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_kilobytes_in_use(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "%llu", follow_partition->used_kilobytes);
		}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	u64 disk_used_kilobytes;

	websWrite(wp, "function disk_usage(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		disk_used_kilobytes = 0;
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next)
			disk_used_kilobytes += follow_partition->size_in_kilobytes;

		websWrite(wp, "%llu", disk_used_kilobytes);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	disk_info_t *follow_disk2;
	u32 disk_num, pool_num;
	websWrite(wp, "function per_pane_pool_usage_kilobytes(pool_num, disk_num){\n");
	for (follow_disk = disks_info, pool_num = 0; follow_disk != NULL; follow_disk = follow_disk->next){
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next, ++pool_num){
			websWrite(wp, "    if (pool_num == %d){\n", pool_num);
			for (follow_disk2 = disks_info, disk_num = 0; follow_disk2 != NULL; follow_disk2 = follow_disk2->next, ++disk_num){
				websWrite(wp, "	if (disk_num == %d) {\n", disk_num);

				//if (strcmp(follow_disk2->tag, follow_disk->tag) == 0)
				if (follow_disk2->major == follow_disk->major && follow_disk2->minor == follow_disk->minor)
					websWrite(wp, "	    return [%llu];\n", follow_partition->size_in_kilobytes);
				else
					websWrite(wp, "	    return [0];\n");

				websWrite(wp, "	}\n");
			}
			websWrite(wp, "    }\n");
		}
	}
	websWrite(wp, "}\n\n");
	free_disk_data(&disks_info);

	return 0;
}

static int ej_available_disk_names_and_sizes(int eid, webs_t wp, int argc, char_t **argv){
	disk_info_t *disks_info, *follow_disk;
	int first;
	char ascii_tag[PATH_MAX], ascii_vendor[PATH_MAX], ascii_model[PATH_MAX];

	websWrite(wp, "function available_disks(){ return [];}\n\n");
	websWrite(wp, "function available_disk_sizes(){ return [];}\n\n");
	websWrite(wp, "function claimed_disks(){ return [];}\n\n");
	websWrite(wp, "function claimed_disk_interface_names(){ return [];}\n\n");
	websWrite(wp, "function claimed_disk_model_info(){ return [];}\n\n");
	websWrite(wp, "function claimed_disk_total_size(){ return [];}\n\n");
	websWrite(wp, "function claimed_disk_total_mounted_number(){ return [];}\n\n");
	websWrite(wp, "function blank_disks(){ return [];}\n\n");
	websWrite(wp, "function blank_disk_interface_names(){ return [];}\n\n");
	websWrite(wp, "function blank_disk_model_info(){ return [];}\n\n");
	websWrite(wp, "function blank_disk_total_size(){ return [];}\n\n");
	websWrite(wp, "function blank_disk_total_mounted_number(){ return [];}\n\n");

	disks_info = read_disk_data();
	if (disks_info == NULL){
		websWrite(wp, "%s", initial_available_disk_names_and_sizes());
		return -1;
	}

	/* show name of the foreign disks */
	websWrite(wp, "function foreign_disks(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		memset(ascii_tag, 0, PATH_MAX);
		char_to_ascii_safe(ascii_tag, follow_disk->tag, PATH_MAX);
		websWrite(wp, "\"%s\"", ascii_tag);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show interface of the foreign disks */
	websWrite(wp, "function foreign_disk_interface_names(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

//		websWrite(wp, "\"USB\"");
		websWrite(wp, "\"%s\"", follow_disk->port);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show model info of the foreign disks */
	websWrite(wp, "function foreign_disk_model_info(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"");

		if (follow_disk->vendor != NULL){
			memset(ascii_vendor, 0, PATH_MAX);
			char_to_ascii_safe(ascii_vendor, follow_disk->vendor, PATH_MAX);
			websWrite(wp, "%s", ascii_vendor);
		}
		if (follow_disk->model != NULL){
			if (follow_disk->vendor != NULL)
				websWrite(wp, " ");

			memset(ascii_model, 0, PATH_MAX);
			char_to_ascii_safe(ascii_model, follow_disk->model, PATH_MAX);
			websWrite(wp, "%s", ascii_model);
		}
		websWrite(wp, "\"");
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show total_size of the foreign disks */
	websWrite(wp, "function foreign_disk_total_size(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%llu\"", follow_disk->size_in_kilobytes);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show total number of the partitions in this foreign disk */
	websWrite(wp, "function foreign_disk_pool_number(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%u\"", follow_disk->partition_number);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show total number of the mounted partitions in this foreign disk */
	websWrite(wp, "function foreign_disk_total_mounted_number(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%u\"", follow_disk->mounted_number);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	free_disk_data(&disks_info);

	return 0;
}

static int ej_get_printer_info(int eid, webs_t wp, int argc, char_t **argv){
	int printer_num, got_printer;
	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
	char printer_array[MAX_USB_PRINTER_NUM][4][64];
	char usb_node[32];
	char port_path[8];

	memset(printer_array, 0, MAX_USB_PRINTER_NUM*4*64);

	for(printer_num = 0, got_printer = 0; printer_num < MAX_USB_PRINTER_NUM; ++printer_num){
		snprintf(prefix, sizeof(prefix), "usb_path_lp%d", printer_num);
		memset(usb_node, 0, 32);
		strncpy(usb_node, nvram_safe_get(prefix), 32);

		if(strlen(usb_node) > 0){
			if(get_path_by_node(usb_node, port_path, 8) != NULL){
				snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);

				strncpy(printer_array[got_printer][0], nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp)), 64);
				strncpy(printer_array[got_printer][1], nvram_safe_get(strcat_r(prefix, "_product", tmp)), 64);
				strncpy(printer_array[got_printer][2], nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 64);
				strncpy(printer_array[got_printer][3], port_path, 64);

				++got_printer;
			}
		}
	}

	websWrite(wp, "function printer_manufacturers(){\n");
	websWrite(wp, "    return [");

	for(printer_num = 0; printer_num < got_printer; ++printer_num){
		if(printer_num != 0)
			websWrite(wp, ", ");

		if(strlen(printer_array[printer_num][0]) > 0)
			websWrite(wp, "\"%s\"", printer_array[printer_num][0]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function printer_models(){\n");
	websWrite(wp, "    return [");

	for(printer_num = 0; printer_num < got_printer; ++printer_num){
		if(printer_num != 0)
			websWrite(wp, ", ");

		if(strlen(printer_array[printer_num][1]) > 0)
			websWrite(wp, "\"%s\"", printer_array[printer_num][1]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function printer_serialn(){\n");
	websWrite(wp, "    return [");

	for(printer_num = 0; printer_num < got_printer; ++printer_num){
		if(printer_num != 0)
			websWrite(wp, ", ");

		if(strlen(printer_array[printer_num][2]) > 0)
			websWrite(wp, "\"%s\"", printer_array[printer_num][2]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function printer_pool(){\n");
	websWrite(wp, "    return [");

	for(printer_num = 0; printer_num < got_printer; ++printer_num){
		if(printer_num != 0)
			websWrite(wp, ", ");

		if(strlen(printer_array[printer_num][3]) > 0)
			websWrite(wp, "\"%s\"", printer_array[printer_num][3]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	return 0;
}

static long old_uptime = 0;

static int ej_get_modem_info(int eid, webs_t wp, int argc, char_t **argv){
	int i, j, got_modem;
	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
#ifdef RT4GAC55U
	char modem_array[MAX_USB_PORT*MAX_USB_HUB_PORT][6][64];
#else
	char modem_array[MAX_USB_PORT*MAX_USB_HUB_PORT][4][64];
#endif
	char port_path[8];
	char act_node[32], act_port_path[8];
	long now;
#ifdef RT4GAC55U
	char *cmd_sig[] = {"/usr/sbin/modem_status.sh", "signal", NULL};
	char *cmd_op[] = {"/usr/sbin/modem_status.sh", "operation", NULL};
	int pid;
#endif

	snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
		memset(act_port_path, 0, 8);
		old_uptime = 0;
	}
	else{
		now = uptime();
		if(!old_uptime || now-old_uptime >= 60){
#ifdef RT4GAC55U
_dprintf("httpd: run modem_status.sh.\n");
#if 0
			_eval(cmd_sig, NULL, 0, &pid);
			_eval(cmd_op, NULL, 0, &pid);
#else
			eval("/usr/sbin/modem_status.sh", "signal");
			eval("/usr/sbin/modem_status.sh", "operation");
#endif
#endif
			old_uptime = now;
		}
	}

#ifdef RT4GAC55U
	memset(modem_array, 0, MAX_USB_PORT*MAX_USB_HUB_PORT*6*64);
#else
	memset(modem_array, 0, MAX_USB_PORT*MAX_USB_HUB_PORT*4*64);
#endif

	got_modem = 0;
	for(i = 1; i <= MAX_USB_PORT; ++i){
		snprintf(prefix, sizeof(prefix), "usb_path%d", i);
		if(!strcmp(nvram_safe_get(prefix), "modem")){
			snprintf(port_path, 8, "%d", i);

			strncpy(modem_array[got_modem][0], nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp)), 64);
			strncpy(modem_array[got_modem][1], nvram_safe_get(strcat_r(prefix, "_product", tmp)), 64);
			strncpy(modem_array[got_modem][2], nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 64);
			strncpy(modem_array[got_modem][3], port_path, 64);
#ifdef RT4GAC55U
			if(!strcmp(port_path, act_port_path)){
				strncpy(modem_array[got_modem][4], nvram_safe_get("usb_modem_act_signal"), 64);
				strncpy(modem_array[got_modem][5], nvram_safe_get("usb_modem_act_operation"), 64);
			}
#endif

			++got_modem;
		}
		else{
			for(j = 1; j <= MAX_USB_HUB_PORT; ++j){
				snprintf(prefix, sizeof(prefix), "usb_path%d.%d", i, j);

				if(!strcmp(nvram_safe_get(prefix), "modem")){
					snprintf(port_path, 8, "%d.%d", i, j);

					strncpy(modem_array[got_modem][0], nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp)), 64);
					strncpy(modem_array[got_modem][1], nvram_safe_get(strcat_r(prefix, "_product", tmp)), 64);
					strncpy(modem_array[got_modem][2], nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 64);
					strncpy(modem_array[got_modem][3], port_path, 64);
#ifdef RT4GAC55U
					if(!strcmp(port_path, act_port_path)){
						strncpy(modem_array[got_modem][4], nvram_safe_get("usb_modem_act_signal"), 64);
						strncpy(modem_array[got_modem][5], nvram_safe_get("usb_modem_act_operation"), 64);
					}
#endif

					++got_modem;
				}
			}
		}
	}

	websWrite(wp, "function modem_manufacturers(){\n");
	websWrite(wp, "    return [");

	for(i = 0; i < got_modem; ++i){
		if(i != 0)
			websWrite(wp, ", ");

		if(strlen(modem_array[i][0]) > 0)
			websWrite(wp, "\"%s\"", modem_array[i][0]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function modem_models(){\n");
	websWrite(wp, "    return [");

	for(i = 0; i < got_modem; ++i){
		if(i != 0)
			websWrite(wp, ", ");

		if(strlen(modem_array[i][1]) > 0)
			websWrite(wp, "\"%s\"", modem_array[i][1]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function modem_serialn(){\n");
	websWrite(wp, "    return [");

	for(i = 0; i < got_modem; ++i){
		if(i != 0)
			websWrite(wp, ", ");

		if(strlen(modem_array[i][2]) > 0)
			websWrite(wp, "\"%s\"", modem_array[i][2]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function modem_pool(){\n");
	websWrite(wp, "    return [");

	for(i = 0; i < got_modem; ++i){
		if(i != 0)
			websWrite(wp, ", ");

		if(strlen(modem_array[i][3]) > 0)
			websWrite(wp, "\"%s\"", modem_array[i][3]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

#ifdef RT4GAC55U
	websWrite(wp, "function modem_signal(){\n");
	websWrite(wp, "    return [");

	for(i = 0; i < got_modem; ++i){
		if(i != 0)
			websWrite(wp, ", ");

		if(strlen(modem_array[i][4]) > 0)
			websWrite(wp, "\"%s\"", modem_array[i][4]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function modem_operation(){\n");
	websWrite(wp, "    return [");

	for(i = 0; i < got_modem; ++i){
		if(i != 0)
			websWrite(wp, ", ");

		if(strlen(modem_array[i][5]) > 0)
			websWrite(wp, "\"%s\"", modem_array[i][5]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");
#endif

	return 0;
}
#if 0
static int modem_simstatus_hook(int eid, webs_t wp, int argc, char_t **argv){//Cherry Cho added in 2014/9/4.
#ifdef RT4GAC55U
	char act_node[32], act_port_path[8];
	char *cmd_simsignal[] = {"modem_status.sh", "signal", NULL};
	char *cmd_simop[] = {"modem_status.sh", "operation", NULL};
	char *cmd_simbytes[] = {"modem_status.sh", "bytes", NULL};	
	float rx_Gbytes, tx_Gbytes;
	int pid2, pid3, pid4;

	snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
		return 0;
	}

	_eval(cmd_simsignal, NULL, 0, &pid2);
	_eval(cmd_simop, NULL, 0, &pid3);
	_eval(cmd_simbytes, NULL, 0, &pid4);	
#endif
	return 0;
}

static int ej_check_modem_sim(int eid, webs_t wp, int argc, char_t **argv){
	char act_node[32], act_port_path[8];
	int status;

	snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
		return 0;
	}

	status = nvram_get_int("usb_modem_act_sim");
	websWrite(wp, "%d", status);

	return 0;
}
#endif
static int ej_get_isp_scan_results(int eid, webs_t wp, int argc, char_t **argv){
#ifdef RT4GAC55U
	char file_name[MAX_LINE_SIZE];
	int ret = 0;

	memset(file_name, 0, MAX_LINE_SIZE);
	sprintf(file_name, "%s", nvram_safe_get("modem_roaming_scanlist"));
	if(strlen(file_name) >= 0)
		ret = dump_file(wp, file_name);

	return ret;
#endif
	return 0;
}

static int ej_get_simact_result(int eid, webs_t wp, int argc, char_t **argv){
#ifdef RT4GAC55U
	char act_node[32], act_port_path[8];
	FILE *fp;
	char buf[256];
	int len = 0;

	snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
		return 0;
	}

	if ((fp = fopen("/tmp/modem_action.ret", "r")) != NULL) {
		while(fgets(buf, sizeof(buf), fp) != NULL){
			len = strlen(buf) - 1;
			if(len > 0){
				if(buf[len] == '\n' || buf[len] == '\r')
					buf[len] = '\0';
				websWrite(wp, buf);
				break;
			}
		}
		fclose(fp);
	}	
#endif
	return 0;
}

static int ej_modemuptime(int eid, webs_t wp, int argc, char_t **argv){
	int ret = 0;
	unsigned int now, start = atoi(nvram_safe_get("usb_modem_act_startsec"));
	char *str;

	if(start <= 0){
		ret = websWrite(wp, "0");
		return ret;
	}

	str = file2str("/proc/uptime");
	if(!str){
		ret = websWrite(wp, "0");
		return ret;
	}

	now = atoi(str);
	free(str);

	ret = websWrite(wp, "%u", (now-start));

	return ret;
}

#else
static int ej_show_usb_path(int eid, webs_t wp, int argc, char_t **argv){
	websWrite(wp, "[]");
	return 0;
}

int ej_apps_fsck_ret(int eid, webs_t wp, int argc, char **argv){
	websWrite(wp, "[]");
	return 0;
}
#endif

int ej_shown_language_css(int eid, webs_t wp, int argc, char **argv){
	struct language_table *pLang = NULL;
	char lang[4];
	int len;
#ifdef RTCONFIG_AUTODICT
	unsigned char header[3] = { 0xef, 0xbb, 0xbf };
	FILE *fp = fopen("Lang_Hdr.txt", "r");
#else
	FILE *fp = fopen("Lang_Hdr", "r");
#endif
	char buffer[1024], key[30], target[30];
	char *follow_info, *follow_info_end;
	int offset = 0;

	if (fp == NULL){
		fprintf(stderr, "No English dictionary!\n");
		return 0;
	}

	memset(lang, 0, 4);
	strcpy(lang, nvram_safe_get("preferred_lang"));
	websWrite(wp, "<li><dl><a href=\"#\"><dt id=\"selected_lang\"></dt></a>\\n");
	while (1) {
		memset(buffer, 0, sizeof(buffer));
		if ((follow_info = fgets(buffer, sizeof(buffer), fp)) != NULL){
#ifdef RTCONFIG_AUTODICT
			if (memcmp(buffer, header, 3) == 0) offset = 3;
#endif
			if (strncmp(follow_info+offset, "LANG_", 5))    // 5 = strlen("LANG_")
				continue;

			follow_info += 5;
			follow_info_end = strstr(follow_info, "=");
			len = follow_info_end-follow_info;
			memset(key, 0, sizeof(key));
			strncpy(key, follow_info, len);

			for (pLang = language_tables; pLang->Lang != NULL; ++pLang){
				if (strcmp(key, pLang->Target_Lang))
					continue;
				follow_info = follow_info_end+1;
				follow_info_end = strstr(follow_info, "\n");
				len = follow_info_end-follow_info;
				memset(target, 0, sizeof(target));
				strncpy(target, follow_info, len);
				if (check_lang_support(key) && strcmp(key,lang))
					websWrite(wp, "<dd><a onclick=\"submit_language(this)\" id=\"%s\">%s</a></dd>\\n", key, target);
				break;
			}
		}
		else
			break;

	}
	websWrite(wp, "</dl></li>\\n");
	fclose(fp);

	return 0;
}

//andi
char*
send_action(char *ftp_url,int port)
{
        char str[1024]={0};
        char buf[1024]={0};
	int my_fd;
	if ((my_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            return NULL;
        }

        struct sockaddr_in their_addr; /* connector's address information */
        bzero(&(their_addr), sizeof(their_addr)); /* zero the rest of the struct */
        their_addr.sin_family = AF_INET; /* host byte order */
        their_addr.sin_port = htons(port); /* short, network byte order */
        their_addr.sin_addr.s_addr = INADDR_ANY;
        //their_addr.sin_addr.s_addr = ((struct in_addr *)(he->h_addr))->s_addr;
        bzero(&(their_addr.sin_zero), sizeof(their_addr.sin_zero)); /* zero the rest of the struct */

        if (connect(my_fd, (struct sockaddr *)&their_addr,sizeof(struct sockaddr)) == -1) {
            perror("connect");
	    close(my_fd);
            return NULL;
        }
        sprintf(str,"refresh@%s",ftp_url);
	_dprintf("socket:%s\n",str);
        if (send(my_fd, str, strlen(str), 0) == -1) {
            perror("send");
	    close(my_fd);
            return NULL;
        }
	
	int len;
	while ((len = recv(my_fd, buf, 1024, 0))) {
        	//_dprintf("BUF:%s\n",buf);
		close(my_fd);
		return strdup(buf);
    	}
	
        close(my_fd);
	return NULL;
}

//andi
static int
ftpServerTree_cgi(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
		char_t *url, char_t *path, char_t *query)
{
        char *ftp_url;
        ftp_url = websGetVar(wp, "path","");
    	_dprintf("URL:%s\n",ftp_url);
	
        char *buf = send_action(ftp_url,3568);
	_dprintf("BUF:%s\n",buf);
	if(buf == NULL)
	{
		websWrite(wp,"NULL");
		return 0;
	}
	else
		websWrite(wp,buf);	
	
        return 0;
}

#ifdef  __CONFIG_NORTON__
/* Trigger an NGA LiveUpdate (linux/netbsd - no support for ECOS) */
static int
nga_update(void)
{
	char *str = NULL;
	int pid;

	if ((str = file2str("/var/run/bootstrap.pid"))) {
		pid = atoi(str);
		free(str);
		return kill(pid, SIGHUP);
	}

	return -1;
}
#endif /* __CONFIG_NORTON__ */

static int
apply_cgi(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
		char_t *url, char_t *path, char_t *query)
{
	char *action_mode;
	char *action_para;
	char *current_url;
	char command[32];
	int i=0, j=0, len=0;
	
	struct json_object *root=NULL;

	if(!strcmp(url, "applyapp.cgi")){
		//_dprintf("post_buf = %s\n",post_buf);
               
		root = json_tokener_parse(post_buf);

		if (!root) {
			//return 0; /* Aicloud app can not use JSON format */
		}
	}
	action_mode = get_cgi_json("action_mode", root);
	current_url = get_cgi_json("current_page", root);

	_dprintf("apply: %s %s\n", action_mode, current_url);

	if (!strcmp(action_mode, "apply")) {
		if (!validate_apply(wp,root)) {
			websWrite(wp, "NOT MODIFIED\n");
		}
		else {
			websWrite(wp, "MODIFIED\n");
		}

		action_para = get_cgi_json("rc_service",root);

		if(action_para && strlen(action_para) > 0) {
			notify_rc(action_para);
		}
		websWrite(wp, "RUN SERVICE\n");
	}
	else if (!strcmp(action_mode," Refresh "))
	{
		char *system_cmd;
		system_cmd = get_cgi_json("SystemCmd",root);
		len = strlen(system_cmd);

		for(i=0;i<len;i++){
			if (isalnum(system_cmd[i]) != 0 || system_cmd[i] == ':' || system_cmd[i] == '-' || system_cmd[i] == '_' || system_cmd[i] == '.' || isspace(system_cmd[i]) != 0)
				j++;
			else{
				_dprintf("[httpd] Invalid SystemCmd!\n");
				strcpy(SystemCmd, "");	

				json_object_put(root);
				websRedirect_iframe(wp, current_url);

				return 0;
			}				
		}
		if(!strcmp(current_url, "Main_Netstat_Content.asp") && (
			strncasecmp(system_cmd, "netstat", 7) == 0
		)){
			strncpy(SystemCmd, system_cmd, sizeof(SystemCmd));
		}
		else if(!strcmp(current_url, "Main_Analysis_Content.asp") && (
			   strncasecmp(system_cmd, "ping", 4) == 0
			|| strncasecmp(system_cmd, "traceroute", 10) == 0
			|| strncasecmp(system_cmd, "nslookup", 8) == 0
		)){
			strncpy(SystemCmd, system_cmd, sizeof(SystemCmd));
		}
		else if(!strcmp(current_url, "Main_WOL_Content.asp") && (
			strncasecmp(system_cmd, "ether-wake", 10) == 0
		)){
			strncpy(SystemCmd, system_cmd, sizeof(SystemCmd));
			sys_script("syscmd.sh");        // Immediately run it
		}
		else if(!strcmp(current_url, "Main_AdmStatus_Content.asp"))
		{
			if(strncasecmp(system_cmd, "run_telnetd", 11) == 0){
				strncpy(SystemCmd, system_cmd, sizeof(SystemCmd));
				sys_script("syscmd.sh");
			}else if(strncasecmp(system_cmd, "run_infosvr", 11) == 0){
				nvram_set("ateCommand_flag", "1");
			}else if(strncasecmp(system_cmd, "set_factory_mode", 16) == 0){
				strncpy(SystemCmd, system_cmd, sizeof(SystemCmd));
				sys_script("syscmd.sh");
			}
		}
		else if(!strcmp(current_url, "Main_ConnStatus_Content.asp") && (
			strncasecmp(system_cmd, "netstat-nat", 11) == 0
		)){
			strncpy(SystemCmd, system_cmd, sizeof(SystemCmd));
		}
		else{
			_dprintf("[httpd] Invalid SystemCmd!\n");
			strcpy(SystemCmd, "");
		}
		json_object_put(root);
		websRedirect_iframe(wp, current_url);
		return 0;
	}
	else if (!strcmp(action_mode," Clear "))
	{
		unlink(get_syslog_fname(1));
		unlink(get_syslog_fname(0));
		websRedirect(wp, current_url);
		json_object_put(root);
		return 0;
	}
	else if (!strcmp(action_mode, " Restart ")||!strcmp(action_mode, "reboot"))
	{
		websApply(wp, "Restarting.asp");
		nvram_set("freeze_duck", "15");
		shutdown(fileno(wp), SHUT_RDWR);
		sys_reboot();
		json_object_put(root);
		return (0);
	}
	else if (!strcmp(action_mode, "Restore")||!strcmp(action_mode, "restore"))
	{
		int offset = 10;
#ifdef RTCONFIG_RALINK
		if (get_model() == MODEL_RTN65U)
			offset = 15;
#endif

		/* Stop USB application prior to counting reboot_time.
		 * Don't stop 3G/4G here.  If yes and end-user connect to
		 * administrative page through 3G/4G, he/she can't see Restarting.asp
		 */
		if (!notify_rc_and_wait_2min("stop_app"))
			_dprintf("%s: send stop_app rc_service fail!\n", __func__);

		/* Enlarge reboot_time temporarily. */
		nvram_set_int("reboot_time", nvram_get_int("reboot_time") + offset);

		eval("/sbin/ejusb", "-1", "0");

		nvram_set("lan_ipaddr", "192.168.1.1");
		websApply(wp, "Restarting.asp");
		shutdown(fileno(wp), SHUT_RDWR);
		nvram_set("restore_defaults", "1");
		nvram_set("freeze_duck", "15");
		sys_default();
		json_object_put(root);
		return (0);
	}
	else if (!strcmp(action_mode, "logout")) // but, every one can reset it by this call
	{
		http_logout(0, "cgi_logout", 0);
		websRedirect(wp, "Nologin.asp");
		json_object_put(root);
		return (0);
	}
	else if (!strcmp(action_mode, "change_wl_unit"))
	{
		action_para = get_cgi_json("wl_unit",root);

		if(action_para)
			nvram_set("wl_unit", action_para);

		action_para = get_cgi_json("wl_subunit",root);

		if(action_para)
			nvram_set("wl_subunit", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "change_wps_unit"))
	{
		action_para = get_cgi_json("wps_band",root);
		if(action_para)
			nvram_set("wps_band", action_para);
#if defined(RTCONFIG_WPSMULTIBAND)
		if ((action_para = get_cgi_json("wps_multiband",root)))
			nvram_set("wps_multiband", action_para);
#endif

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "wps_apply"))
	{
		action_para = get_cgi_json("wps_band",root);
		if(action_para)
			nvram_set("wps_band", action_para);
		else goto wps_finish;

		action_para = get_cgi_json("wps_enable",root);
		if(action_para)
			nvram_set("wps_enable", action_para);
		else goto wps_finish;

		action_para = get_cgi_json("wps_sta_pin",root);
		if(action_para)
			nvram_set("wps_sta_pin", action_para);
		else goto wps_finish;
#if defined(RTCONFIG_WPSMULTIBAND)
		if ((action_para = get_cgi_json("wps_multiband",root)))
			nvram_set("wps_multiband", action_para);
#endif

		notify_rc("start_wps_method");

wps_finish:
		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "wps_reset"))
	{
		action_para = get_cgi_json("wps_band",root);
		if(action_para)
			nvram_set("wps_band", action_para);
#if defined(RTCONFIG_WPSMULTIBAND)
		if ((action_para = get_cgi_json("wps_multiband",root)))
			nvram_set("wps_multiband", action_para);
#endif

		notify_rc("reset_wps");

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "change_wan_unit"))
	{	
		action_para = get_cgi_json("wan_unit", root);

		if(action_para)
			nvram_set("wan_unit", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "change_dslx_transmode"))
	{
		action_para = get_cgi_json("dsltmp_transmode", root);

		if(action_para)
			nvram_set("dsltmp_transmode", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "change_lan_unit"))
	{
		action_para = get_cgi_json("lan_unit",root);

		if(action_para)
			nvram_set("lan_unit", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "refresh_networkmap"))
	{
		nvram_set("client_info_tmp", "");
		nvram_set("refresh_networkmap", "1");
		nvram_set("nmp_client_tmp", "");//Yaudbg

		doSystem("killall -%d networkmap", SIGUSR1);
#ifdef RTCONFIG_JFFS2USERICON
		notify_rc("start_lltdc");
#endif
		notify_rc("start_miniupnpc");

		websRedirect(wp, current_url);
	}
        else if (!strcmp(action_mode, "update_client_list"))
        {
                action_para = get_cgi_json("client_info_tmp", root);
                if(action_para)
                        nvram_set("client_info_tmp", action_para);

                doSystem("killall -%d networkmap", SIGUSR1);

		websDone(wp, 200);
                //websRedirect(wp, current_url);
        }
	else if (!strcmp(action_mode, "restore_module"))
	{
		action_para = get_cgi_json("module_prefix",root);
		if(action_para) {
			sprintf(command, "restore %s", action_para);
			notify_rc(command);
		}
		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "mfp_requeue")){
		unsigned int login_ip = (unsigned int)atoll(nvram_safe_get("login_ip"));

		if (login_ip == 0x100007f || login_ip == 0x0)
			nvram_set("mfp_ip_requeue", "");
		else{
			struct in_addr addr;

			addr.s_addr = login_ip;
			nvram_set("mfp_ip_requeue", inet_ntoa(addr));
		}

		int u2ec_fifo = open("/var/u2ec_fifo", O_WRONLY|O_NONBLOCK);

		write(u2ec_fifo, "q", 1);
		close(u2ec_fifo);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "mfp_monopolize")){
		unsigned int login_ip = (unsigned int)atoll(nvram_safe_get("login_ip"));

		//printf("[httpd] run mfp monopolize\n");	// tmp test
		if (login_ip==0x100007f || login_ip==0x0)
			nvram_set("mfp_ip_monopoly", "");
		else
		{
			struct in_addr addr;
			addr.s_addr=login_ip;
			nvram_set("mfp_ip_monopoly", inet_ntoa(addr));
		}
		int u2ec_fifo = open("/var/u2ec_fifo", O_WRONLY|O_NONBLOCK);
		write(u2ec_fifo, "m", 1);
		close(u2ec_fifo);

		websRedirect(wp, current_url);
	}
#ifdef ASUS_DDNS //2007.03.22 Yau add
	else if (!strcmp(action_mode, "ddnsclient"))
	{
		notify_rc("restart_ddns");

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "ddns_hostname_check"))
	{
		notify_rc("ddns_hostname_check");

		websRedirect(wp, current_url);
	}
#endif
#ifdef RTCONFIG_TRAFFIC_METER
	else if (!strcmp(action_mode, "reset_traffic_meter"))
	{
		printf("@@@ RESET Traffic Meter!!!\n");
		doSystem("killall -%d wanduck", SIGTSTP);
/*
		if (!validate_apply(wp)) {
			websWrite(wp, "NOT MODIFIED\n");
		}
		else {
			websWrite(wp, "MODIFIED\n");
		}
*/
		websRedirect(wp, current_url);
	}
#endif
//#ifdef RTCONFIG_CLOUDSYNC // get share link from lighttpd. Jerry5 added 2012.11.08
	else if (!strcmp(action_mode, "get_sharelink"))
	{
		FILE *fp;
		char buf[256];
		pid_t pid = 0;

		action_para = get_cgi_json("share_link_param", root);
		if(action_para){
			nvram_set("share_link_param", action_para);
			nvram_set("share_link_result", "");
		}

		action_para = get_cgi_json("share_link_host", root);
		if(action_para){
			nvram_set("share_link_host", action_para);
			nvram_commit();
		}

		if ((fp = fopen("/tmp/lighttpd/lighttpd.pid", "r")) != NULL) {
			if (fgets(buf, sizeof(buf), fp) != NULL)
		   	pid = strtoul(buf, NULL, 0);
			fclose(fp);
			if (pid > 1 && kill(pid, SIGUSR2) == 0) {
				printf("[HTTPD] Signaling lighttpd OK!\n");
			}
			else{
				printf("[HTTPD] Signaling lighttpd FAIL!\n");
			}
		}
	}
//#endif
#ifdef RTCONFIG_OPENVPN
        else if (!strcmp(action_mode, "change_vpn_server_unit"))
        {
		action_para = get_cgi_json("vpn_server_unit", root);

                if(action_para)
                        nvram_set("vpn_server_unit", action_para);

		action_para = websGetVar(wp, "VPNServer_mode", "");

		if(action_para)
			nvram_set("VPNServer_mode", action_para);

		websRedirect(wp, current_url);
        }
        else if (!strcmp(action_mode, "change_vpn_client_unit"))
        {
		action_para = get_cgi_json("vpn_client_unit", root);

                if(action_para)
                        nvram_set("vpn_client_unit", action_para);

                websRedirect(wp, current_url);
        }
#endif
#ifdef  __CONFIG_NORTON__
	/* Trigger an NGA LiveUpdate */
	else if (!strcmp(action_mode, "NGAUpdate"))
		websWrite(wp, "Invoking LiveUpdate...");
		if (nga_update())
			websWrite(wp, "error<br>");
		else
			websWrite(wp, "done<br>");
	}
#endif /* __CONFIG_NORTON__ */
#ifdef RT4GAC55U	
	else if (!strcmp(action_mode, "scan_isp"))
	{
		char act_node[32], act_port_path[8];

		snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
		if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
			json_object_put(root);
			return 0;
		}

		notify_rc("start_modemscan");
	}
	else if (!strcmp(action_mode, "start_lockpin") || !strcmp(action_mode, "stop_lockpin"))
	{
		char act_node[32], act_port_path[8];
		char *pincode;

		pincode = get_cgi_json("sim_pincode", root);

		snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
		if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
			json_object_put(root);
			return 0;
		}

		sprintf(command, "%s %s", action_mode, pincode);
		notify_rc(command);
	}
	else if (!strcmp(action_mode, "start_pwdpin"))
	{
		char act_node[32], act_port_path[8];
		char *pincode, *newpin;

		pincode = get_cgi_json("sim_pincode", root);
		newpin = get_cgi_json("sim_newpin", root);

		snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
		if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
			json_object_put(root);
			return 0;
		}

		sprintf(command, "%s %s %s", action_mode, pincode, newpin);
		notify_rc(command);
	}
	else if (!strcmp(action_mode, "start_simpin"))
	{
		char act_node[32], act_port_path[8];
		char *pincode, *save_pin, *g3err_pin, *wan_unit;
		int save_nvram = 0;

		pincode = get_cgi_json("sim_pincode", root);
		save_pin = get_cgi_json("save_pin", root);
		g3err_pin = get_cgi_json("g3err_pin", root);
		wan_unit = get_cgi_json("wan_unit", root);

		snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
		if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
			json_object_put(root);
			return 0;
		}

		nvram_set("g3err_pin", g3err_pin);

		if(!strcmp(save_pin, "1")){
			nvram_set("modem_pincode", pincode);
			save_nvram = 1;
		}
		else if(strcmp(nvram_safe_get("modem_pincode"),"") && !strcmp(save_pin, "0")){
			nvram_set("modem_pincode", "");
			save_nvram = 1;
		}

		sprintf(command, "%s %s", action_mode, pincode);
		notify_rc(command);

		if(save_nvram)
			nvram_commit();		
	}	
	else if (!strcmp(action_mode, "start_simpuk"))
	{
		char act_node[32], act_port_path[8];
		char *puk, *newpin, *g3err_pin, *wan_unit;

		puk = get_cgi_json("sim_puk", root);
		newpin = get_cgi_json("sim_newpin", root);
		g3err_pin = get_cgi_json("g3err_pin", root);
		wan_unit = get_cgi_json("wan_unit", root);
		
		snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
		if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
			json_object_put(root);
			return 0;
		}

		nvram_set("g3err_pin", g3err_pin);

		sprintf(command, "%s %s %s", action_mode, puk, newpin);
		notify_rc(command);
	}	
	else if (!strcmp(action_mode, "restart_simauth"))
	{
		char act_node[32], act_port_path[8];

		snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
		if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
			json_object_put(root);
			return 0;
		}
			
		notify_rc(action_mode);
	}
	else if (!strcmp(action_mode, "start_simdetect"))
	{
		char *simdetect;

		simdetect = get_cgi_json("simdetect", root);
		sprintf(command, "%s %s", action_mode, simdetect);
		notify_rc(command);
		websApply(wp, "Restarting.asp");
		nvram_set("freeze_duck", "15");
		shutdown(fileno(wp), SHUT_RDWR);		
		sys_reboot();
		json_object_put(root);
		return 0;
	}	
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)	
	else if (!strcmp(action_mode, "restart_resetcount"))
	{
		notify_rc(action_mode);
	}
	else if (!strcmp(action_mode, "restart_sim_del"))
	{
		char *sim_order;

		sim_order = get_cgi_json("sim_order", root);

		sprintf(command, "%s %s", action_mode, sim_order);
		notify_rc(command);
	}
#endif		
#endif
#ifdef RTCONFIG_TRAFFIC_CONTROL
	else if (!strcmp(action_mode, "traffic_resetcount"))
	{
		char *ifname = get_cgi_json("interface", root);
		char ifmap[IFNAME_MAX];	// ifname after mapping

		memset(ifmap, 0, sizeof(ifmap));
		ifname_mapping(ifname, ifmap);
		doSystem("traffic_control -w");	// write database
		doSystem("rm -f /jffs/traffic_control/%s/traffic.db", ifmap);	// delete file
		doSystem("echo -n 0 > /jffs/traffic_control/%s/tmp", ifmap);	// reset current traffic
	}
#endif
#ifdef RTCONFIG_WTFAST
	else if (!strcmp(action_mode, "wtfast_logout")){
		char *wtf_rulelist = get_cgi_json("wtf_rulelist", root);
		nvram_set("wtf_rulelist", wtf_rulelist);
		nvram_set("wtf_username", "");
		nvram_set("wtf_passwd", "");
		nvram_set("wtf_account_type", "");
		nvram_set("wtf_max_clients", "");
		killall("wtfd", SIGHUP);
		_dprintf("httpd: wtfast_logout\n");
	} 
	else if (!strcmp(action_mode, "wtfast_login")){
		char *wtf_username, *wtf_passwd, *wtf_account_type, *wtf_max_clients;

		wtf_username = get_cgi_json("wtf_username", root);
		wtf_passwd = get_cgi_json("wtf_passwd", root);
		wtf_account_type = get_cgi_json("wtf_account_type", root);
		wtf_max_clients = get_cgi_json("wtf_max_clients", root);
		
		nvram_set("wtf_username", wtf_username);
		nvram_set("wtf_passwd", wtf_passwd);
		nvram_set("wtf_account_type", wtf_account_type);
		nvram_set("wtf_max_clients", wtf_max_clients);
		killall("wtfd", SIGHUP);
		_dprintf("httpd: wtfast_login\n");
 	}
#endif 	
#ifdef RTCONFIG_DISK_MONITOR
	else if (!strcmp(action_mode, "change_diskmon_unit"))
	{
		action_para = get_cgi_json("diskmon_usbport", root);

		if(action_para)
			nvram_set("diskmon_usbport", action_para);
	}
#endif
	json_object_put(root);
	return 1;
}



static void
do_auth(char *userid, char *passwd, char *realm)
{
//	time_t tm;

	if (strcmp(ProductID,"")==0)
	{
		strcpy(ProductID, get_productid());
	}
	if (strcmp(UserID,"")==0 || reget_passwd == 1)
	{
	   	strcpy(UserID, nvram_safe_get("http_username"));
	}
// 2008.08 magic {
	if (strcmp(UserPass, "") == 0 || reget_passwd == 1)
	{
// 2008.08 magic }
		strcpy(UserPass, nvram_safe_get("http_passwd"));
	}

	reget_passwd = 0;

	strncpy(userid, UserID, AUTH_MAX);

	if (!is_auth())
	{
		strcpy(passwd, "");
	}
	else
	{
		strncpy(passwd, UserPass, AUTH_MAX);
	}
	strncpy(realm, ProductID, AUTH_MAX);
}

//andi
static void
do_ftpServerTree_cgi(char *url, FILE *stream)
{
    ftpServerTree_cgi(stream, NULL, NULL, 0, url, NULL, NULL);
}

static void
do_apply_cgi(char *url, FILE *stream)
{
    apply_cgi(stream, NULL, NULL, 0, url, NULL, NULL);
}

/* Look for unquoted character within a string */
char *
unqstrstr_t(char *haystack, char *needle)
{
	char *cur;
	int q;

	for (cur = haystack, q = 0;
	     cur < &haystack[strlen(haystack)] && !(!q && !strncmp(needle, cur, strlen(needle)));
	     cur++) {
		if (*cur == '"')
			q ? q-- : q++;
	}
	return (cur < &haystack[strlen(haystack)]) ? cur : NULL;
}

char *
get_arg_t(char *args, char **next)
{
	char *arg, *end;

	/* Parse out arg, ... */
	if (!(end = unqstrstr_t(args, ","))) {
		end = args + strlen(args);
		*next = NULL;
	} else
		*next = end + 1;

	/* Skip whitespace and quotation marks on either end of arg */
	for (arg = args; isspace((int)*arg) || *arg == '"'; arg++);
	for (*end-- = '\0'; isspace((int)*end) || *end == '"'; end--)
		*end = '\0';

	return arg;
}

#ifdef TRANSLATE_ON_FLY
static int refresh_title_asp = 0;

static void
do_lang_cgi(char *url, FILE *stream)
{
	if (refresh_title_asp)  {
		// Request refreshing pages from browser.
		websHeader(stream);
		websWrite(stream, "<head></head><title>REDIRECT TO INDEX.ASP</title>");

		// The text between <body> and </body> content may be rendered in Opera browser.
		websWrite(stream, "<body onLoad='if (navigator.appVersion.indexOf(\"Firefox\")!=-1||navigator.appName == \"Netscape\"){top.location=\"index.asp\";}else{top.location.reload(true);}'></body>");
		websFooter(stream);
		websDone(stream, 200);
	} else {
		// Send redirect-page if and only if refresh_title_asp is true.
		// If we do not send Title.asp, Firefox reload web-pages again and again.
		// This trick had been deprecated due to compatibility issue with Netscape and Mozilla browser.
		websRedirect(stream, "Title.asp");
	}
}


static void
do_lang_post(char *url, FILE *stream, int len, char *boundary)
{
	int c;
	char *p, *p1;
	char orig_lang[4], new_lang[4];

	if (url == NULL)
		return;

	p = strstr (url, "preferred_lang_menu");
	if (p == NULL)
		return;
	memset (new_lang, 0, sizeof (new_lang));
	strncpy (new_lang, p + strlen ("preferred_lang_menu") + 1, 2);

	memset (orig_lang, 0, sizeof (orig_lang));
	p1 = nvram_safe_get_x ("", "preferred_lang");
	if (p1[0] != '\0') {
		strncpy (orig_lang, p1, 2);
	} else {
		strncpy (orig_lang, "EN", 2);
	}

	// read remain data
#if 0
	if (feof (stream)) {
		while ((c = fgetc(stream) != EOF)) {
			;	// fall through
		}
	}
#else
	char buf[1024];
	while ((c = fread(buf, 1, sizeof(buf), stream)) > 0)
		;		// fall through
#endif

	cprintf ("lang: %s --> %s\n", orig_lang, new_lang);
	refresh_title_asp = 0;
	if (strcmp (orig_lang, new_lang) != 0 || is_firsttime ()) {
		// If language setting is different or first change language
		nvram_set_x ("", "preferred_lang", new_lang);
		if (is_firsttime ())    {
			cprintf ("set x_Setting --> 1\n");
			nvram_set("x_Setting", "1");
		}
		cprintf ("!!!!!!!!!Commit new language settings.\n");
		refresh_title_asp = 1;
		nvram_commit();
	}
}
#endif // TRANSLATE_ON_FLY



#define SWAP_LONG(x) \
	((__u32)( \
		(((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
		(((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
		(((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
		(((__u32)(x) & (__u32)0xff000000UL) >> 24) ))

int upgrade_err;
int stop_upgrade_once = 0;

static void
do_upgrade_post(char *url, FILE *stream, int len, char *boundary)
{
	#define MAX_VERSION_LEN 64
	char upload_fifo[64] = "/tmp/linux.trx";
	FILE *fifo = NULL;
	char buf[4096];
	int count, ch/*, ver_chk = 0*/;
	int cnt;
	long filelen;
	int offset;
	struct sysinfo si;
	upgrade_err=1;
	/* workaround to RAM disk space issue */
	stop_upgrade_once = 0;
	nvram_set_int("upgrade_fw_status", FW_INIT);
	f_write_string("/tmp/detect_wrong.log", "", 0, 0);
	f_write_string("/tmp/usb.log", "", 0, 0);
#if defined(RTCONFIG_SMALL_FW_UPDATE)
	eval("/sbin/ejusb", "-1", "0");
	notify_rc("stop_upgrade");
	stop_upgrade_once = 1;
	sleep(10);
	/* Mount 16M ram disk to avoid out of memory */
	system("mkdir /tmp/mytmpfs");
	system("mount -t tmpfs -o size=16M,nr_inodes=10k,mode=700 tmpfs /tmp/mytmpfs");
	snprintf(upload_fifo, sizeof(upload_fifo), "/tmp/mytmpfs/linux.trx");
#endif

	/* Look for our part */
	while (len > 0)
	{
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream))
		{
			goto err;
		}

		len -= strlen(buf);

		if (!strncasecmp(buf, "Content-Disposition:", 20) && strstr(buf, "name=\"file\""))
			break;
	}

	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream))
		{
			goto err;
		}
		len -= strlen(buf);
		if (!strcmp(buf, "\n") || !strcmp(buf, "\r\n"))
		{
			break;
		}
	}

#define BYTE_TO_KB(b) ((b >> 10) + ((b & 0x2ff)?1:0))
	free_caches(FREE_MEM_PAGE, 5, BYTE_TO_KB(len));

	if (!(fifo = fopen(upload_fifo, "a+"))) goto err;

#if !defined(RTCONFIG_SMALL_FW_UPDATE)
	sysinfo(&si);
	/* available tmpfs size is half of free RAM */
	if ((si.freeram * si.mem_unit)/2 < len)
	{
		eval("/sbin/ejusb", "-1", "0");
		notify_rc("stop_upgrade");
		stop_upgrade_once = 1;
	}
#endif

	filelen = len;
	cnt = 0;
	offset = 0;

	/* Pipe the rest to the FIFO */
	while (len>0 && filelen>0)
	{

#ifdef RTCONFIG_HTTPS
		//_dprintf("[httpd] SSL for upgrade!\n"); // tmp test
		if(do_ssl){
			//_dprintf("[httpd] ssl_stream_fd : %d\n", ssl_stream_fd); // tmp test
			if (waitfor(ssl_stream_fd, 3) <= 0)
				break;
		}
		else{
			if (waitfor (fileno(stream), 10) <= 0)
			{
				break;
			}
		}
#else
		//_dprintf("[httpd] NO SSL for upgrade!\n"); // tmp test
		if (waitfor (fileno(stream), 10) <= 0)
		{
			break;
		}
#endif

		count = fread(buf + offset, 1, MIN(len, sizeof(buf)-offset), stream);

		if(count <= 0)
			goto err;

		len -= count;

		if(cnt==0) {
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
#define HEADER_LEN (64)
#else
#define HEADER_LEN (8)
#endif
			if(count + offset < HEADER_LEN)
			{
				offset += count;
				continue;
			}

			count += offset;
			offset = 0;
			_dprintf("read from stream: %d\n", count);
			cnt++;
			if(!check_imageheader(buf, &filelen)) {
				goto err;
			}
		}
		filelen-=count;
		fwrite(buf, 1, count, fifo);
	}

	/* Slurp anything remaining in the request */
	while (len-- > 0)
	{
		if((ch = fgetc(stream)) == EOF)
			break;

		if (filelen>0)
		{
			fwrite(&ch, 1, 1, fifo);
			filelen--;
		}
	}
	fclose(fifo);
	fifo = NULL;
#ifdef RTCONFIG_DSL

	int ret_val_sep;
#ifdef RTCONFIG_RALINK
	ret_val_sep = separate_tc_fw_from_trx();	//TODO: merge truncated_trx

	// should router update tc fw?
	if (ret_val_sep)
	{
		if(check_tc_firmware_crc()) /* return 0 when pass */
			goto err;
	}
	//TODO: if merge truncated_trx() to separate_tc_fw_from_trx()
	//then all use check_imagefile(upload_fifo)
#else
	ret_val_sep = separate_tc_fw_from_trx(upload_fifo);

	// should router update tc fw?
	if (ret_val_sep)
	{
		if(check_tc_firmware_crc()) /* return 0 when pass */
			goto err;
		nvram_set_int("reboot_time", nvram_get_int("reboot_time")+100);
	}

	if(!check_imagefile(upload_fifo)) /* 0: illegal image; 1: legal image */
		goto err;
#endif

#else
#ifdef RTAC68U
	if (nvram_match("bl_version", "2.1.2.2") || nvram_match("bl_version", "2.1.2.6")) {
		unlink("/tmp/linux.trx");
		eval("/usr/sbin/webs_update.sh");

		if (nvram_get_int("webs_state_update") &&
		    !nvram_get_int("webs_state_error") &&
		    strlen(nvram_safe_get("webs_state_info"))) {
			_dprintf("retrieve firmware information\n");

			if (!nvram_get_int("webs_state_flag"))
			{
				_dprintf("no need to upgrade firmware\n");
				goto err;
			}

			eval("/usr/sbin/webs_upgrade.sh");

			if (nvram_get_int("webs_state_error"))
			{
				_dprintf("error execute upgrade script\n");
				goto err;
			}

			nvram_set("restore_defaults", "1");
			system("nvram erase");

		} else _dprintf("could not retrieve firmware information!\n");
	}
#endif
	if(!check_imagefile(upload_fifo)) /* 0: illegal image; 1: legal image */
		goto err;
#endif
	upgrade_err = 0;

err:
	nvram_set_int("upgrade_fw_status", FW_UPLOADING_ERROR);
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		if((ch = fgetc(stream)) == EOF)
			break;
}

static void
do_upgrade_cgi(char *url, FILE *stream)
{
	_dprintf("## [httpd] do upgrade cgi upgrade_err(%d)\n", upgrade_err);	// tmp test
	/* Reboot if successful */

	if (upgrade_err == 0)
	{
#ifdef RTCONFIG_DSL
#ifdef RTCONFIG_RALINK
		int ret_val_trunc;
		ret_val_trunc = truncate_trx();
		printf("truncate_trx ret=%d\n",ret_val_trunc);
		if (ret_val_trunc)
		{
			int ret_val_comp;

			do_upgrade_adsldrv();
			ret_val_comp = compare_linux_image();
			printf("compare_linux_image ret=%d\n",ret_val_comp);
			if (ret_val_comp == 0)
			{
				// same trx
				unlink("/tmp/linux.trx");
			}
			else
			{
				// different trx
				// it will call rc_service automatically for firmware upgrading
			}
		}
#endif
#endif
#if !defined(RTCONFIG_SMALL_FW_UPDATE)
		if (!stop_upgrade_once){
			eval("/sbin/ejusb", "-1", "0");
			notify_rc("stop_upgrade");
			stop_upgrade_once = 1;
		}
#endif
		int etry = 3, err = 0;

#if (defined(PLN12) || defined(PLAC56))
		set_wifiled(6);
#endif
		websApply(stream, "Updating.asp");
		shutdown(fileno(stream), SHUT_RDWR);
		while(etry-- && (err = notify_rc_after_period_wait("start_upgrade", 60)))
		{
			_dprintf("%s, try agn upgrade...%d/3, err=%d\n", __FUNCTION__, etry, err);
			notify_rc_after_period_wait("stop_upgrade", 10);
			stop_upgrade_once = 1;
		}
	}
	else
	{
		if(stop_upgrade_once != 0){
			nvram_set_int("upgrade_fw_status", FW_WRITING_ERROR);
			websApply(stream, "UpdateError_reboot.asp");
			unlink("/tmp/linux.trx");
			sys_reboot();
		}else{
			nvram_set_int("upgrade_fw_status", FW_WRITING_ERROR);
			websApply(stream, "UpdateError.asp");
			unlink("/tmp/linux.trx");
		}
	}
}

static void
do_upload_post(char *url, FILE *stream, int len, char *boundary)
{
	#define MAX_VERSION_LEN 64
	char upload_fifo[] = "/tmp/settings_u.prf";
	FILE *fifo = NULL;
	char buf[1024];
	int count, ret = EINVAL, ch;
	int /*eno, */cnt;
	long filelen, *filelenptr;
	char /*version[MAX_VERSION_LEN], */cmpHeader;
	int offset;

	/* Look for our part */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream)) {
			goto err;
		}

		len -= strlen(buf);

		if (!strncasecmp(buf, "Content-Disposition:", 20)
				&& strstr(buf, "name=\"file\""))
			break;
	}

	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream)) {
			goto err;
		}

		len -= strlen(buf);
		if (!strcmp(buf, "\n") || !strcmp(buf, "\r\n")) {
			break;
		}
	}

	if (!(fifo = fopen(upload_fifo, "a+")))
		goto err;

	filelen = len;
	cnt = 0;
	offset = 0;

	/* Pipe the rest to the FIFO */
	cprintf("Upgrading %d\n", len);
	cmpHeader = 0;

	while (len > 0 && filelen > 0) {
#ifdef RTCONFIG_HTTPS
		//_dprintf("[httpd] SSL for upload!\n"); // tmp test
		if(do_ssl){
			//_dprintf("[httpd] ssl_stream_fd : %d\n", ssl_stream_fd); // tmp test
			if (waitfor(ssl_stream_fd, 3) <= 0)
				break;
		}
		else{
			if (waitfor (fileno(stream), 10) <= 0)
			{
				break;
			}
		}
#else
		//_dprintf("[httpd] NO SSL for upload!\n"); // tmp test
		if (waitfor (fileno(stream), 10) <= 0)
		{
			break;
		}
#endif
		count = fread(buf + offset, 1, MIN(len, sizeof(buf)-offset), stream);
		if(count <= 0)
			goto err;

		len -= count;

		if (cnt == 0)
		{
			if(count + offset < 8)
			{
				offset += count;
				continue;
			}
			count += offset;
			offset = 0;

			if (!strncmp(buf, PROFILE_HEADER, 4))
			{
				filelenptr = (long*)(buf + 4);
				filelen = *filelenptr;

			}
			else if (!strncmp(buf, PROFILE_HEADER_NEW, 4))
			{
				filelenptr = (long*)(buf + 4);
				filelen = *filelenptr;
				filelen = filelen & 0xffffff;

			}
			else
			{
				goto err;
			}

			cmpHeader = 1;
			++cnt;
		}

		filelen -= count;
		fwrite(buf, 1, count, fifo);
	}

	if (!cmpHeader)
		goto err;

	/* Slurp anything remaining in the request */
	while (len-- > 0) {
		ch = fgetc(stream);

		if (filelen > 0) {
			fwrite(&ch, 1, 1, fifo);
			--filelen;
		}
	}

	ret = 0;

	fseek(fifo, 0, SEEK_END);
	fclose(fifo);
	fifo = NULL;
	/*printf("done\n");*/

err:
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		if((ch = fgetc(stream)) == EOF)
			break;

	fcntl(fileno(stream), F_SETOWN, -ret);
}

static void
do_upload_cgi(char *url, FILE *stream)
{
	int ret;

#ifdef RTCONFIG_HTTPS
	if(do_ssl)
		ret = fcntl(ssl_stream_fd , F_GETOWN, 0);
	else
#endif
	ret = fcntl(fileno(stream), F_GETOWN, 0);

	/* Reboot if successful */
	if (ret == 0)
	{
		websApply(stream, "Uploading.asp");
#ifdef RTCONFIG_HTTPS
	if(do_ssl)
		shutdown(ssl_stream_fd, SHUT_RDWR);
	else
#endif
		shutdown(fileno(stream), SHUT_RDWR);
		sys_upload("/tmp/settings_u.prf");
		nvram_commit();
		sys_reboot();
	}
	else
	{
		websApply(stream, "UploadError.asp");
	   	//unlink("/tmp/settings_u.prf");
	}
}

#ifdef RTCONFIG_OPENVPN

#define VPN_CLIENT_UPLOAD	"/tmp/openvpn_file"

static void
do_vpnupload_post(char *url, FILE *stream, int len, char *boundary)
{
	char upload_fifo[] = VPN_CLIENT_UPLOAD;
	FILE *fifo = NULL;
	int ret = EINVAL, ch;
	int offset;
	char *name, *value, *p;

	memset(post_buf, 0, sizeof(post_buf));
	nvram_set("vpn_upload_type", "");
	nvram_set("vpn_upload_unit", "");

	/* Look for our part */
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}

		len -= strlen(post_buf);

		if (!strncasecmp(post_buf, "Content-Disposition:", 20)) {
			if(strstr(post_buf, "name=\"file\""))
				break;
			else if(strstr(post_buf, "name=\"")) {
				offset = strlen(post_buf);
				fgets(post_buf+offset, MIN(len + 1, sizeof(post_buf)-offset), stream);
				len -= strlen(post_buf) - offset;
				offset = strlen(post_buf);
				fgets(post_buf+offset, MIN(len + 1, sizeof(post_buf)-offset), stream);
				len -= strlen(post_buf) - offset;
				p = post_buf;
				name = strstr(p, "\"") + 1;
				p = strstr(name, "\"");
				strcpy(p++, "\0");
				value = strstr(p, "\r\n\r\n") + 4;
				p = strstr(value, "\r");
				strcpy(p, "\0");
				//printf("%s=%s\n", name, value);
				nvram_set(name, value);
			}
		}
	}

	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}

		len -= strlen(post_buf);
		if (!strcmp(post_buf, "\n") || !strcmp(post_buf, "\r\n")) {
			break;
		}
	}

	if (!(fifo = fopen(upload_fifo, "w")))
		goto err;

	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}
		len -= strlen(post_buf);

		if(boundary) {
			if (strstr(post_buf, boundary))
				break;
		}

		fputs(post_buf, fifo);
	}

	ret = 0;

	fclose(fifo);
	fifo = NULL;
	/*printf("done\n");*/

err:
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		if((ch = fgetc(stream)) == EOF)
			break;

	fcntl(fileno(stream), F_SETOWN, -ret);
}

#ifdef RTCONFIG_QTN  //RT-AC87U
static void
do_qtn_diagnostics(char *url, FILE *stream)
{
	char qtn_rpc_client[20] = {0};

	unlink("/tmp/diagnostics_done");
	nvram_set("qtn_diagnostics", "1");
	printf("Do diagnostics\n");
	memset(qtn_rpc_client, 0, sizeof(qtn_rpc_client));
	snprintf(qtn_rpc_client, sizeof(qtn_rpc_client), "%s", nvram_safe_get("QTN_RPC_CLIENT"));
	eval("qcsapi_sockrpc", "run_script", "router_command.sh", "diagnostics", qtn_rpc_client);
	while(access("/tmp/diagnostics_done", R_OK ) == -1 ) {
		printf("run_script.log does not exist, wait\n");
		sleep(5);
	}
	do_file("/tmp/run_script.log", stream);
	unlink("/tmp/diagnostics_done");
	nvram_unset("qtn_diagnostics");
}
#endif

static void
do_vpnupload_cgi(char *url, FILE *stream)
{
	int ret, state;
	char *filetype = nvram_safe_get("vpn_upload_type");
	char *unit = nvram_safe_get("vpn_upload_unit");
	char nv[32] = {0};

	if(!filetype || !unit) {
		unlink(VPN_CLIENT_UPLOAD);
		return;
	}

#ifdef RTCONFIG_HTTPS
	if(do_ssl)
		ret = fcntl(ssl_stream_fd , F_GETOWN, 0);
	else
#endif
	ret = fcntl(fileno(stream), F_GETOWN, 0);

	if (ret == 0)
	{
		//websApply(stream, "OvpnChecking.asp");

		if(!strcmp(filetype, "ovpn")) {
			reset_client_setting(atoi(unit));
			ret = read_config_file(VPN_CLIENT_UPLOAD, atoi(unit));
			nvram_set_int("vpn_upload_state", ret);
			nvram_commit();
		}
		else if(!strcmp(filetype, "ca")) {
			sprintf(nv, "vpn_crt_client%s_ca", unit);
			set_crt_parsed(nv, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_CA_CERT));
		}
		else if(!strcmp(filetype, "cert")) {
			sprintf(nv, "vpn_crt_client%s_crt", unit);
			set_crt_parsed(nv, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_CERT));
		}
		else if(!strcmp(filetype, "key")) {
			sprintf(nv, "vpn_crt_client%s_key", unit);
			set_crt_parsed(nv, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_KEY));
		}
		else if(!strcmp(filetype, "static")) {
			sprintf(nv, "vpn_crt_client%s_static", unit);
			set_crt_parsed(nv, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_STATIC));
		}
		else if(!strcmp(filetype, "ccrl")) {
			sprintf(nv, "vpn_crt_client%s_crl", unit);
			set_crt_parsed(nv, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_CRL));
		}
		else if(!strcmp(filetype, "scrl")) {
			sprintf(nv, "vpn_crt_server%s_crl", unit);
			set_crt_parsed(nv, VPN_CLIENT_UPLOAD);
		}
		else if(!strcmp(filetype, "extra")) {
			sprintf(nv, "vpn_crt_client%s_extra", unit);
			set_crt_parsed(nv, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_EXTRA));
		}
	}
	else
	{
		//websApply(stream, "OvpnError.asp");
	}
	unlink(VPN_CLIENT_UPLOAD);
}
#endif	//RTCONFIG_OPENVPN

// Viz 2010.08
static void
do_update_cgi(char *url, FILE *stream)
{
	struct ej_handler *handler;
	const char *pattern;
	int argc;
	char *argv[16];
	char s[32];

	if ((pattern = get_cgi("output")) != NULL) {
		for (handler = &ej_handlers[0]; handler->pattern; handler++) {
			if (strcmp(handler->pattern, pattern) == 0) {
				for (argc = 0; argc < 16; ++argc) {
					sprintf(s, "arg%d", argc);
					if ((argv[argc] = (char *)get_cgi(s)) == NULL) break;
				}
				handler->output(0, stream, argc, argv);
				break;
			}
		}
	}
}

//Traffic Monitor
void wo_bwmbackup(char *url, webs_t wp)
{
	static const char *hfn = "/var/lib/misc/rstats-history.gz";
	struct stat st;
	time_t t;
	int i;

	if (stat(hfn, &st) == 0) {
		t = st.st_mtime;
		sleep(1);
	}
	else {
		t = 0;
	}
	killall("rstats", SIGHUP);
	for (i = 10; i > 0; --i) {
		if ((stat(hfn, &st) == 0) && (st.st_mtime != t)) break;
		sleep(1);
	}
	if (i == 0) {
		//send_error(500, "Bad Request", (char*) 0, "Internal server error." );
		return;
	}
	//send_headers(200, NULL, mime_binary, 0);
	do_f((char *)hfn, wp);
}
// end Viz ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

#if 0
void wo_iptbackup(char *url, webs_t wp)
{
	static const char *ifn = "/var/lib/misc/cstats-history.gz";
	struct stat st;
	time_t t;
	int i;

	if (stat(ifn, &st) == 0) {
		t = st.st_mtime;
		sleep(1);
	}
	else {
		t = 0;
	}
	killall("cstats", SIGHUP);
	for (i = 20; i > 0; --i) { // may take a long time for gzip to complete
		if ((stat(ifn, &st) == 0) && (st.st_mtime != t)) break;
		sleep(1);
	}
	if (i == 0) {
//		send_error(500, NULL, NULL);
		return;
	}
//	send_header(200, NULL, mime_binary, 0);
	do_f((char *)ifn, wp);
}
#endif

static int ej_iptmon(int eid, webs_t wp, int argc, char **argv) {

	char comma;
	char sa[256];
	FILE *a;
	char *exclude;
	char *include;

	char ip[INET6_ADDRSTRLEN];

	int64_t tx, rx;

	exclude = nvram_safe_get("cstats_exclude");
	include = nvram_safe_get("cstats_include");

	websWrite(wp, "\n\niptmon={");
	comma = ' ';

	char wholenetstatsline = 1;

	if ((a = fopen("/proc/net/ipt_account/lan", "r")) == NULL) return 0;

	if (!wholenetstatsline)
		fgets(sa, sizeof(sa), a); // network

	while (fgets(sa, sizeof(sa), a)) {
		if(sscanf(sa, 
			"ip = %s bytes_src = %llu %*u %*u %*u %*u packets_src = %*u %*u %*u %*u %*u bytes_dst = %llu %*u %*u %*u %*u packets_dst = %*u %*u %*u %*u %*u time = %*u",
			ip, &tx, &rx) != 3 ) continue;
		if (find_word(exclude, ip)) {
			wholenetstatsline = 0;
			continue;
		}

		if (((find_word(include, ip)) || (wholenetstatsline == 1)) || ((nvram_get_int("cstats_all")) && ((rx > 0) || (tx > 0)) )) {
//		if ((find_word(include, ip)) || (wholenetstatsline == 1)) {
//		if ((tx > 0) || (rx > 0) || (wholenetstatsline == 1)) {
//		if ((tx > 0) || (rx > 0)) {
			websWrite(wp,"%c'%s':{rx:0x%llx,tx:0x%llx}", comma, ip, rx, tx);
			comma = ',';
		}
		wholenetstatsline = 0;
	}
	fclose(a);
	websWrite(wp,"};\n");

	return 0;
}

static int ej_ipt_bandwidth(int eid, webs_t wp, int argc, char **argv)
{
	char *name;
	int sig;

	if ((nvram_get_int("cstats_enable") == 1) && (argc == 1)) {
		if (strcmp(argv[0], "speed") == 0) {
			sig = SIGUSR1;
			name = "/var/spool/cstats-speed.js";
		}
		else {
			sig = SIGUSR2;
			name = "/var/spool/cstats-history.js";
		}
		unlink(name);
		killall("cstats", sig);
		f_wait_exists(name, 5);
		do_f(name, wp);
		unlink(name);
	}
	return 0;
}

static int ej_iptraffic(int eid, webs_t wp, int argc, char **argv) {
	char comma;
	char sa[256];
	FILE *a;
	char ip[INET_ADDRSTRLEN];

	char *exclude;

	int64_t tx_bytes, rx_bytes;
	unsigned long tp_tcp, rp_tcp;
	unsigned long tp_udp, rp_udp;
	unsigned long tp_icmp, rp_icmp;
	unsigned int ct_tcp, ct_udp;

	exclude = nvram_safe_get("cstats_exclude");

	Node tmp;
	Node *ptr;

	iptraffic_conntrack_init();

	if ((a = fopen("/proc/net/ipt_account/lan", "r")) == NULL) return 0;

        websWrite(wp, "\n\niptraffic=[");
        comma = ' ';

	fgets(sa, sizeof(sa), a); // network
	while (fgets(sa, sizeof(sa), a)) {
		if(sscanf(sa, 
			"ip = %s bytes_src = %llu %*u %*u %*u %*u packets_src = %*u %lu %lu %lu %*u bytes_dst = %llu %*u %*u %*u %*u packets_dst = %*u %lu %lu %lu %*u time = %*u",
			ip, &tx_bytes, &tp_tcp, &tp_udp, &tp_icmp, &rx_bytes, &rp_tcp, &rp_udp, &rp_icmp) != 9 ) continue;
		if (find_word(exclude, ip)) continue ;
		if ((tx_bytes > 0) || (rx_bytes > 0)){
			strncpy(tmp.ipaddr, ip, INET_ADDRSTRLEN);
			ptr = TREE_FIND(&tree, _Node, linkage, &tmp);
			if (!ptr) {
				ct_tcp = 0;
				ct_udp = 0;
			} else {
				ct_tcp = ptr->tcp_conn;
				ct_udp = ptr->udp_conn;
			}
			websWrite(wp, "%c['%s', %llu, %llu, %lu, %lu, %lu, %lu, %lu, %lu, %u, %u]", 
						comma, ip, rx_bytes, tx_bytes, rp_tcp, tp_tcp, rp_udp, tp_udp, rp_icmp, tp_icmp, ct_tcp, ct_udp);
			comma = ',';
		}
	}
	fclose(a);
	websWrite(wp, "];\n");

	TREE_FORWARD_APPLY(&tree, _Node, linkage, Node_housekeeping, NULL);
	TREE_INIT(&tree, Node_compare);

	return 0;
}

void iptraffic_conntrack_init() {
	unsigned int a_time, a_proto;
	char a_src[INET_ADDRSTRLEN];
	char a_dst[INET_ADDRSTRLEN];
	char b_src[INET_ADDRSTRLEN];
	char b_dst[INET_ADDRSTRLEN];

	char sa[256];
	char sb[256];
	FILE *a;
	char *p;
	int x;

	Node tmp;
	Node *ptr;

	unsigned long rip;
	unsigned long lan;
	unsigned long mask;

	if (strcmp(nvram_safe_get("lan_ifname"), "") != 0) {
		rip = inet_addr(nvram_safe_get("lan_ipaddr"));
		mask = inet_addr(nvram_safe_get("lan_netmask"));
		lan = rip & mask;
//		_dprintf("rip[%d]=%lu\n", br, rip[br]);
//		_dprintf("mask[%d]=%lu\n", br, mask[br]);
//		_dprintf("lan[%d]=%lu\n", br, lan[br]);
	} else {
		mask = 0;
		rip = 0;
		lan = 0;
	}

	const char conntrack[] = "/proc/net/ip_conntrack";

	if ((a = fopen(conntrack, "r")) == NULL) return;

//	ctvbuf(a);	// if possible, read in one go

	while (fgets(sa, sizeof(sa), a)) {
		if (sscanf(sa, "%*s %u %u", &a_proto, &a_time) != 2) continue;

		if ((a_proto != 6) && (a_proto != 17)) continue;

		if ((p = strstr(sa, "src=")) == NULL) continue;
		if (sscanf(p, "src=%s dst=%s %n", a_src, a_dst, &x) != 2) continue;
		p += x;

		if ((p = strstr(p, "src=")) == NULL) continue;
		if (sscanf(p, "src=%s dst=%s", b_src, b_dst) != 2) continue;

		snprintf(sb, sizeof(sb), "%s %s %s %s", a_src, a_dst, b_src, b_dst);
		remove_dups(sb, sizeof(sb));

		char ipaddr[INET_ADDRSTRLEN], *next = NULL;

		foreach(ipaddr, sb, next) {
			if ((mask == 0) || ((inet_addr(ipaddr) & mask) != lan)) continue;

			strncpy(tmp.ipaddr, ipaddr, INET_ADDRSTRLEN);
			ptr = TREE_FIND(&tree, _Node, linkage, &tmp);

			if (!ptr) {
				_dprintf("%s: new ip: %s\n", __FUNCTION__, ipaddr);
				TREE_INSERT(&tree, _Node, linkage, Node_new(ipaddr));
				ptr = TREE_FIND(&tree, _Node, linkage, &tmp);
			}
			if (a_proto == 6) ++ptr->tcp_conn;
			if (a_proto == 17) ++ptr->udp_conn;
		}
	}
	fclose(a);
//	Tree_info();
}

void ctvbuf(FILE *f) {
	int n;
	struct sysinfo si;
//	meminfo_t mem;

#if 1
	const char *p;

	if ((p = nvram_get("ct_max")) != NULL) {
		n = atoi(p);
		if (n == 0) n = 2048;
		else if (n < 1024) n = 1024;
		else if (n > 10240) n = 10240;
	}
	else {
		n = 2048;
	}
#else
	char s[64];

	if (f_read_string("/proc/sys/net/ipv4/ip_conntrack_max", s, sizeof(s)) > 0) n = atoi(s);
		else n = 1024;
	if (n < 1024) n = 1024;
		else if (n > 10240) n = 10240;
#endif

	n *= 170;	// avg tested

//	get_memory(&mem);
//	if (mem.maxfreeram < (n + (64 * 1024))) n = mem.maxfreeram - (64 * 1024);

	sysinfo(&si);
	if (si.freeram < (n + (64 * 1024))) n = si.freeram - (64 * 1024);

//	cprintf("free: %dK, buffer: %dK\n", si.freeram / 1024, n / 1024);

	if (n > 4096) {
//		n =
		setvbuf(f, NULL, _IOFBF, n);
//		cprintf("setvbuf = %d\n", n);
	}
}

static void
prf_file(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query)
{
	char *ddns_flag;
	char *ddns_mac;
	char ddns_hostname_tmp[65];
	char model_name;
	
	model_name = get_model();
#ifdef RTCONFIG_RGMII_BRCM5301X
	ddns_mac = nvram_get("et1macaddr");
#else	
	if(model_name == MODEL_RTN56U){
		ddns_mac = nvram_get("et1macaddr");
	}
	else{
		ddns_mac = nvram_get("et0macaddr");	
	}
#endif	
	ddns_flag = websGetVar(wp, "path", "");
	if(strcmp(ddns_flag, "0") == 0){
		strncpy(ddns_hostname_tmp, nvram_get("ddns_hostname_x"), sizeof(ddns_hostname_tmp));
		nvram_set("ddns_transfer", "");
		nvram_set("ddns_hostname_x", "");
	}
	else{
		nvram_set("ddns_transfer", ddns_mac);
	}

	nvram_unset("nmp_client_list");
	nvram_unset("asus_device_list");
	nvram_commit();
	sys_download("/tmp/settings");

	if(strcmp(ddns_flag, "0") == 0){
		nvram_set("ddns_hostname_x", ddns_hostname_tmp);
		nvram_commit();
	}

	do_file("/tmp/settings", wp);
}

static void
do_prf_file(char *url, FILE *stream)
{
    prf_file(stream, NULL, NULL, 0, url, NULL, NULL);
}

static void
do_uploadIconFile_file(char *url, FILE *stream)
{
        system("tar cvf /tmp/IconFile.tar /jffs/usericon /tmp/upnpicon");
        do_file("/tmp/IconFile.tar", stream);
        unlink("/tmp/IconFile.tar");
}

static void
do_networkmap_file(char *url, FILE *stream)
{
	system("nvram get nmp_client_list > /tmp/nmp_client_list.log");
	system("nvram get asus_device_list > /tmp/asus_dev_list.log");
	system("tar cvf /tmp/networkmap.tar /tmp/upnpc_xml.log /tmp/upnp.log /tmp/smb.log /tmp/syslog.log \
		/tmp/nmp_client_list.log /tmp/asus_dev_list.log  /tmp/mDNSNetMonitor.log /jffs/usericon /tmp/upnpicon");
	do_file("/tmp/networkmap.tar", stream);
	unlink("/tmp/networkmap.tar");
}

static void
do_upnp_file(char *url, FILE *stream)
{
        do_file("/tmp/upnp.log", stream);
}

static void
do_upnpc_xml_file(char *url, FILE *stream)
{
        do_file("/tmp/upnpc_xml.log", stream);
}

static void
do_dnsnet_file(char *url, FILE *stream)
{
        do_file("/tmp/mDNSNetMonitor.log", stream);
}

static void
do_prf_ovpn_file(char *url, FILE *stream)
{
	nvram_commit();
	do_file(url, stream);
}

#ifdef RTCONFIG_DSL_TCLINUX
static void
do_diag_log_file(char *url, FILE *stream)
{
	char path[128];
	snprintf(path, sizeof(path), "%s/%s", nvram_safe_get("dslx_diag_log_path"), url);
	//_dprintf("Get log file %s\n", path);
	do_file(path, stream);
}
#endif

// 2010.09 James. {
static char no_cache_IE7[] =
"X-UA-Compatible: IE=EmulateIE7\r\n"
"Cache-Control: no-cache\r\n"
"Pragma: no-cache\r\n"
"Expires: 0"
;
// 2010.09 James. }

static char no_cache[] =
"Cache-Control: no-cache\r\n"
"Pragma: no-cache\r\n"
"Expires: 0"
;

static char syslog_txt[] =
"Content-Disposition: attachment;\r\n"
"filename=syslog.txt"
;

static char cache_object[] =
"Cache-Control: max-age=300"
;

#ifdef RTCONFIG_USB_MODEM
static char modemlog_txt[] =
"Content-Disposition: attachment;\r\n"
"filename=modemlog.txt"
;

static void
do_modemlog_cgi(char *path, FILE *stream)
{
	char *cmd[] = {"/usr/sbin/3ginfo.sh", NULL};

	unlink("/tmp/3ginfo.txt");
	_eval(cmd, ">/tmp/3ginfo.txt", 0, NULL);

	dump_file(stream, get_modemlog_fname());
	fputs("\r\n", stream); /* terminator */
	fputs("\r\n", stream); /* terminator */
}
#endif

static void
do_log_cgi(char *path, FILE *stream)
{
	dump_file(stream, get_syslog_fname(1));
	dump_file(stream, get_syslog_fname(0));
	fputs("\r\n", stream); /* terminator */
	fputs("\r\n", stream); /* terminator */
}

#ifdef RTCONFIG_FINDASUS
static int
findasus_cgi(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
		char_t *url, char_t *path, char_t *query)
{
	char *action_mode;
	char *action_para;
	char *current_url;

	action_mode = websGetVar(wp, "action_mode","");
	current_url = websGetVar(wp, "current_page", "");
	_dprintf("apply: %s %s\n", action_mode, current_url);

	if (!strcmp(action_mode, "refresh_networkmap"))
	{
		printf("@@@ Signal to networkmap!!!\n");
		doSystem("killall -%d networkmap", SIGUSR1);

		websRedirect(wp, current_url);
	}
	return 1;
}


static void
do_findasus_cgi(char *url, FILE *stream)
{
    findasus_cgi(stream, NULL, NULL, 0, url, NULL, NULL);
}
#endif


/* Base-64 decoding.  This represents binary data as printable ASCII
** characters.  Three 8-bit binary bytes are turned into four 6-bit
** values, like so:
**
**   [11111111]  [22222222]  [33333333]
**
**   [111111] [112222] [222233] [333333]
**
** Then the 6-bit values are represented using the characters "A-Za-z0-9+/".
*/

static int b64_decode_table[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
    };

/* Do base-64 decoding on a string.  Ignore any non-base64 bytes.
** Return the actual number of bytes generated.  The decoded size will
** be at most 3/4 the size of the encoded, and may be smaller if there
** are padding characters (blanks, newlines).
*/
static int
b64_decode( const char* str, unsigned char* space, int size )
{
    const char* cp;
    int space_idx, phase;
    int d, prev_d=0;
    unsigned char c;

    space_idx = 0;
    phase = 0;
    for ( cp = str; *cp != '\0'; ++cp )
	{
	d = b64_decode_table[(int)*cp];
	if ( d != -1 )
	    {
	    switch ( phase )
		{
		case 0:
		++phase;
		break;
		case 1:
		c = ( ( prev_d << 2 ) | ( ( d & 0x30 ) >> 4 ) );
		if ( space_idx < size )
		    space[space_idx++] = c;
		++phase;
		break;
		case 2:
		c = ( ( ( prev_d & 0xf ) << 4 ) | ( ( d & 0x3c ) >> 2 ) );
		if ( space_idx < size )
		    space[space_idx++] = c;
		++phase;
		break;
		case 3:
		c = ( ( ( prev_d & 0x03 ) << 6 ) | d );
		if ( space_idx < size )
		    space[space_idx++] = c;
		phase = 0;
		break;
		}
	    prev_d = d;
	    }
	}
    return space_idx;
}

asus_token_t* create_list(char *token)
{
	char login_timestr[32];
	time_t now;

	struct in_addr login_ip_addr;
	char *login_ip_str;

	login_ip_addr.s_addr = login_ip_tmp;
	login_ip_str = inet_ntoa(login_ip_addr);

	now = uptime();

	memset(login_timestr, 0, 32);
	sprintf(login_timestr, "%lu", now);

	asus_token_t *ptr;
	ptr = (asus_token_t*)malloc(sizeof(asus_token_t));
	if(NULL == ptr)
	{
	        printf("\n Node creation failed \n");
	        return NULL;
	}
	strncpy(ptr->useragent, user_agent, 1024);
	strncpy(ptr->token, token, 32);
	strncpy(ptr->ipaddr, login_ip_str, 16);
	strncpy(ptr->login_timestampstr, login_timestr, 32);
	strncpy(ptr->host, host_name, 64);
	ptr->next = NULL;

    head = curr = ptr;
    return ptr;
}

asus_token_t* add_token_to_list(char *token, int add_to_end)
{
	if(NULL == head)
	{
		return (create_list(token));
	}

	asus_token_t *ptr = (asus_token_t *)malloc(sizeof(asus_token_t));
	if(NULL == ptr)
	{
		_dprintf("\n Node creation failed \n");
		return NULL;
	}
	char login_timestr[32];
	time_t now;

	struct in_addr login_ip_addr;
	char *login_ip_str;

	login_ip_addr.s_addr = login_ip_tmp;
	login_ip_str = inet_ntoa(login_ip_addr);

	now = uptime();

	memset(login_timestr, 0, 32);
	sprintf(login_timestr, "%lu", now);

	strncpy(ptr->useragent, user_agent, 1024);
	strncpy(ptr->token, token, 32);
	strncpy(ptr->ipaddr, login_ip_str, 16);
	strncpy(ptr->login_timestampstr, login_timestr, 32);
	strncpy(ptr->host, host_name, 64);
	ptr->next = NULL;

	if(add_to_end == 1)
	{
		curr->next = ptr;
        	curr = ptr;
  	}
    	else
    	{
        	ptr->next = head;
		head = ptr;
    	}
	return ptr;
}

int get_token_list_length(void){ 

	asus_token_t *p = head;

	int count=0;    

	while(p!=NULL){      
		count++;    
		p=p->next;    
	}  
     
	return count;  
}  

asus_token_t* search_timeout_in_list(asus_token_t **prev, int fromapp_flag)
{
	asus_token_t *ptr = head;
	asus_token_t *tmp = NULL;
	char *cp = NULL;
	int found = 0;

	time_t now = 0;
	
	now = uptime();

	while(ptr != NULL)
	{
		cp = strtok(ptr->useragent, "-");

		if(!cp)
		{
			found = 1;
			break;
		}

		if((unsigned long)(now-atol(ptr->login_timestampstr)) > 60 && strcmp( cp, "asusrouter") != 0)
		{
			found = 1;
			break;
       		}else if((unsigned long)(now-atol(ptr->login_timestampstr)) > 6000 && strcmp( cp, "asusrouter") == 0)
		{
			found = 1;
			break;
	        }else if(fromapp_flag == 0 && strcmp(cp, "asusrouter") != 0)
		{
			found = 1;
			break;
       		}else
        	{
			tmp = ptr;
			ptr = ptr->next;
        	}
	}

	if(found == 1)
	{
		if(prev)
		*prev = tmp;
		return ptr;
	}
	else
	{
		return NULL;
	}
}

int check_token_timeout_in_list(void)
{
	int i;
	int list_len = get_token_list_length();
	char *cp = strtok(user_agent, "-");
	int fromapp_flag = 0;

	if(cp != NULL && strcmp( cp, "asusrouter") == 0)
		fromapp_flag = 1;

	for(i=0; i < list_len; i++){
		asus_token_t *prev = NULL;
		asus_token_t *del = NULL;
		del = search_timeout_in_list(&prev, fromapp_flag);

		if(del == NULL)
		{
			return -1;
		}
		else
 		{
        		if(prev != NULL)
			prev->next = del->next;
	
			if(del == curr)
		        {
        		    curr = prev;
        		}
        		if(del == head)
        		{
        		    head = del->next;
        		}
		}
		free(del);
		del = NULL;
   	}
	return 0;

}

int check_login_in_list(void)
{
	asus_token_t *prev = NULL;
	asus_token_t *del = NULL;

	char *cp1 = strtok(user_agent, "-");
	int fromapp_flag = 0;

	if(strcmp( cp1, "asusrouter") == 0)
	fromapp_flag = 1;

	del = search_timeout_in_list(&prev, fromapp_flag);
	if(del == NULL)
	{
		return -1;
	}
	else
		{
       		if(prev != NULL)
		prev->next = del->next;

		if(del == curr)
	        {
       		    curr = prev;
       		}
       		if(del == head)
       		{
       		    head = del->next;
       		}
	}
	free(del);
	del = NULL;
	return 0;
}

void print_list(void)
{
    asus_token_t *ptr = head;

    _dprintf("\n -------Printing list Start------- \n");
    while(ptr != NULL)
    {
	_dprintf("%s\n",ptr->useragent);
        _dprintf("%s\n",ptr->token);
	_dprintf("%s\n",ptr->ipaddr);
	_dprintf("%s\n",ptr->login_timestampstr);
	_dprintf("%s\n",ptr->host);
        ptr = ptr->next;
    }
    _dprintf("\n -------Printing list End------- \n");

    return;
}

void add_asus_token(char *token){
	//print_list();
	int ret;
	ret = check_token_timeout_in_list();

	add_token_to_list(token, 1);

	//print_list();
}

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
static int
login_cgi(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
		char_t *url, char_t *path, char_t *query)
{
	char *authorization_t;
	char authinfo[500];
	char* authpass;
	int l;
	char asus_token[32];
	char *next_page=NULL;

	char *cp1 = strtok(user_agent, "-");
	int fromapp_flag = 0;

	if(strcmp( cp1, "asusrouter") == 0)
		fromapp_flag = 1;

	next_page = websGetVar(wp, "next_page", "");

	authorization_t = websGetVar(wp, "login_authorization","");
	/* Decode it. */
	l = b64_decode( &(authorization_t[0]), (unsigned char*) authinfo, sizeof(authinfo) );
	authinfo[l] = '\0';

	authpass = strchr( authinfo, ':' );
	if ( authpass == (char*) 0 ) {
		websRedirect(wp, "Main_Login.asp");
		return 0;
	}
	*authpass++ = '\0';

	time_t now;
	char timebuf[100];
	now = time( (time_t*) 0 );

	time_t dt;
	struct in_addr temp_ip_addr;
	char *temp_ip_str;

	login_timestamp_tmp = uptime();
	dt = login_timestamp_tmp - last_login_timestamp;
	if(last_login_timestamp != 0 && dt > 60){
		login_try = 0;
		last_login_timestamp = 0;
	}
	if (MAX_login <= DEFAULT_LOGIN_MAX_NUM){
		MAX_login = DEFAULT_LOGIN_MAX_NUM;
	}
	if(login_try >= MAX_login){
		temp_ip_addr.s_addr = login_ip_tmp;
		temp_ip_str = inet_ntoa(temp_ip_addr);

		if(login_try%MAX_login == 0){
			logmessage("httpd login lock", "Detect abnormal logins at %d times. The newest one was from %s.", login_try, temp_ip_str);
		}
		__send_login_page(fromapp_flag, LOGINLOCK);
		return LOGINLOCK;
	}

	websWrite(wp,"%s %d %s\r\n", PROTOCOL, 200, "OK" );
	websWrite(wp,"Server: %s\r\n", SERVER_NAME );
        if (fromapp_flag == 1){
		websWrite(wp, "Cache-Control: no-store\r\n");	
		websWrite(wp, "Pragma: no-cache\r\n");
		websWrite(wp,"AiHOMEAPILevel: %d\r\n", EXTEND_AIHOME_API_LEVEL );
		websWrite(wp,"Httpd_AiHome_Ver: %d\r\n", EXTEND_HTTPD_AIHOME_VER );
	}
	(void) strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
	websWrite(wp,"Date: %s\r\n", timebuf );
	if (fromapp_flag == 0){
		websWrite(wp,"Content-Type: %s\r\n", "text/html");	
	}else{
		websWrite(wp,"Content-Type: %s\r\n", "application/json;charset=UTF-8");
	}

	/* Is this the right user and password? */
	if ( strcmp( nvram_safe_get("http_username"), authinfo ) == 0 && strcmp( nvram_safe_get("http_passwd"), authpass ) == 0)
	{
		if (fromapp_flag == 0){
			login_try = 0;
			last_login_timestamp = 0;
			memset(referer_host, 0, sizeof(referer_host));
			if(strncmp(DUT_DOMAIN_NAME, host_name, strlen(DUT_DOMAIN_NAME))==0){
				strcpy(referer_host, nvram_safe_get("lan_ipaddr"));
			}else
				snprintf(referer_host,sizeof(host_name),"%s",host_name);
		}
		strncpy(asus_token, generate_token(), sizeof(asus_token));
		add_asus_token(asus_token);

		websWrite(wp,"Set-Cookie: asus_token=%s; HttpOnly;\r\n",asus_token);
		websWrite(wp,"Connection: close\r\n" );
		websWrite(wp,"\r\n" );
		if (fromapp_flag == 0){
			websWrite(wp,"<HTML><HEAD>\n" );
			if(next_page == NULL)
				websWrite(wp,"<script>parent.location.href='/index.asp';</script>\n");
			else
				websWrite(wp,"<script>parent.location.href='/%s';</script>\n", next_page);

			websWrite(wp,"</HEAD></HTML>\n" );
		}else{		
			websWrite(wp,"{\n" );
			websWrite(wp,"\"asus_token\":\"%s\"\n", asus_token);
			websWrite(wp,"}\n" );			
		}
		return 1;
	}else{
		websWrite(wp,"Connection: close\r\n" );
		websWrite(wp,"\r\n" );
		if(fromapp_flag == 1){
			websWrite(wp, "{\n\"error_status\":\"%d\"\n}\n", ACCOUNTFAIL);
		}else{
			login_try++;
			last_login_timestamp = login_timestamp_tmp;
			websWrite(wp,"<HTML><HEAD>\n" );
			websWrite(wp,"<script>parent.location.href='/Main_Login.asp?error_status=%d';</script>\n",ACCOUNTFAIL);
			websWrite(wp,"</HEAD></HTML>\n" );	
		}
		return 0;
	}
}

static void
do_login_cgi(char *url, FILE *stream)
{
    login_cgi(stream, NULL, NULL, 0, url, NULL, NULL);
}


static void
app_call(char *func, FILE *stream)
{
	char *args, *end, *next;
	int argc;
	char * argv[16]={NULL};
	int app_method_hit = 0;
	struct ej_handler *handler;

	/* Parse out ( args ) */
	if (!(args = strchr(func, '(')))
		return;
	if (!(end = unqstrstr_t(func, ")")))
		return;
	*args++ = *end = '\0';

	/* Set up argv list */
	for (argc = 0; argc < 16 && args && *args; argc++, args = next) {
		if (!(argv[argc] = get_arg_t(args, &next)))
			break;
	}
	//_dprintf("app_call:argv[0] = %s\n",argv[0]);
	
	if (!argv[0] || strcmp(argv[0], "appobj") != 0){
		websWrite(stream,"\"" );
	}else{
		websWrite(stream,"{" );
	}

	/* Call handler */
	for (handler = &ej_handlers[0]; handler->pattern; handler++) {
//		if (strncmp(handler->pattern, func, strlen(handler->pattern)) == 0)
		if (strcmp(handler->pattern, func) == 0){
			handler->output(0, stream, argc, argv);
			app_method_hit = 1;
		}
	}
	if (app_method_hit == 0)
		websWrite(stream,"Not Support");
	
	if (!argv[0] || strcmp(argv[0], "appobj") != 0)
		websWrite(stream,"\"" );
	else
		websWrite(stream,"}" );
}

static void
do_appGet_cgi(char *url, FILE *stream)
{
	char *pattern;
	int firstRow=1;

	pattern = websGetVar(wp, "hook","");

	char *pattern_t = strtok(pattern, ";");

	websWrite(stream,"{\n" );

	while (pattern_t != NULL){
		if (firstRow == 1)
			firstRow = 0;
		else
			websWrite(stream, ",\n");

		websWrite(stream,"\"%s\":", pattern_t);

		app_call(pattern_t, stream);

		pattern_t = strtok(NULL, ";");
	}

	websWrite(stream,"\n}\n" );
}


static void
do_appGet_image_path_cgi(char *url, FILE *stream)
{

	websWrite(stream,"{\n" );

	websWrite(stream, "\"IMAGE_MODEL_PRODUCT\":\"%s\",\n", IMAGE_MODEL_PRODUCT);
	websWrite(stream, "\"IMAGE_WANUNPLUG\":\"%s\",\n", IMAGE_WANUNPLUG);
	websWrite(stream, "\"IMAGE_ROUTER_MODE\":\"%s\",\n", IMAGE_ROUTER_MODE);
	websWrite(stream, "\"IMAGE_REPEATER_MODE\":\"%s\",\n", IMAGE_REPEATER_MODE);
	websWrite(stream, "\"IMAGE_AP_MODE\":\"%s\",\n", IMAGE_AP_MODE);
	websWrite(stream, "\"IMAGE_MEDIA_BRIDGE_MODE\":\"%s\"\n", IMAGE_MEDIA_BRIDGE_MODE);

	websWrite(stream,"\n}\n" );
}

static void
do_qis_default(char *url, FILE *stream)
{
	char *flag;
	flag = websGetVar(wp, "flag","");
	if(!strcmp(flag, "sitesurvey"))
		websRedirect(stream, "QIS_wizard.htm?flag=sitesurvey");
	else
		websRedirect(stream, "QIS_wizard.htm?flag=welcome");
}

//2008.08 magic{
struct mime_handler mime_handlers[] = {
	{ "Main_Login.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "Nologin.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "error_page.htm*", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "blocking.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "gotoHomePage.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "ure_success.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "ureip.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "remote.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "js/jquery.js", "text/javascript", no_cache_IE7, NULL, do_file, NULL }, // 2010.09 James.
	{ "require/require.min.js", "text/javascript", no_cache_IE7, NULL, do_file, NULL },
	{ "httpd_check.xml", "text/xml", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "httpd_check.json", "application/json", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "findasus.json", "application/json", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "get_webdavInfo.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "appGet_image_path.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_appGet_image_path_cgi, NULL },
	{ "login.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_login_cgi, NULL },
	{ "update_clients.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "manifest.appcache", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "offline.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "wcdma_list.js", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "help_content.js", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "httpd_check.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "manifest.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "update_cloudstatus.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "update_applist.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "update_appstate.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
#ifdef RTCONFIG_FINDASUS
	{ "findasus.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_findasus_cgi, NULL },
	{ "find_device.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
#endif
	{ "**.xml", "text/xml", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "**.htm*", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "**.asp*", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "**.appcache", "text/cache-manifest", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },

#ifdef RTCONFIG_DSL_TCLINUX
	{ "TCC.log.*", "application/octet-stream", NULL, NULL, do_diag_log_file, do_auth },
#endif
	{ "**.gz", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.tgz", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.zip", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.ipk", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.css", "text/css", cache_object, NULL, do_file, NULL },
	{ "**.png", "image/png", cache_object, NULL, do_file, NULL },
	{ "**.gif", "image/gif", cache_object, NULL, do_file, NULL },
	{ "**.jpg", "image/jpeg", cache_object, NULL, do_file, NULL },
	// Viz 2010.08
	{ "**.svg", "image/svg+xml", NULL, NULL, do_file, NULL },
	{ "**.swf", "application/x-shockwave-flash", NULL, NULL, do_file, NULL  },
	{ "**.htc", "text/x-component", NULL, NULL, do_file, NULL  },
	// end Viz
#ifdef TRANSLATE_ON_FLY
	/* Only general.js and quick.js are need to translate. (reduce translation time) */
	{ "general.js|quick.js",  "text/javascript", no_cache_IE7, NULL, do_ej, do_auth },
#endif //TRANSLATE_ON_FLY

	{ "**.js",  "text/javascript", no_cache_IE7, NULL, do_ej, do_auth },
	{ "**.cab", "text/txt", NULL, NULL, do_file, do_auth },
	{ "**.CFG", "application/force-download", NULL, do_html_post_and_get, do_prf_file, do_auth },
	{ "uploadIconFile.tar", "application/force-download", NULL, NULL, do_uploadIconFile_file, do_auth },
	{ "networkmap.tar", "application/force-download", NULL, NULL, do_networkmap_file, do_auth },
	{ "upnp.log", "application/force-download", NULL, NULL, do_upnp_file, do_auth },
	{ "upnpc_xml.log", "application/force-download", NULL, NULL, do_upnpc_xml_file, do_auth },
	{ "mDNSNetMonitor.log", "application/force-download", NULL, NULL, do_dnsnet_file, do_auth },
	{ "ftpServerTree.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_ftpServerTree_cgi, do_auth },//andi
	{ "**.ovpn", "application/force-download", NULL, NULL, do_prf_ovpn_file, do_auth },
	{ "QIS_default.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_qis_default, do_auth },
	{ "apply.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_apply_cgi, do_auth },
	{ "applyapp.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_apply_cgi, do_auth },
	{ "appGet.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_appGet_cgi, do_auth },
	{ "upgrade.cgi*", "text/html", no_cache_IE7, do_upgrade_post, do_upgrade_cgi, do_auth},
	{ "upload.cgi*", "text/html", no_cache_IE7, do_upload_post, do_upload_cgi, do_auth },
	{ "syslog.txt*", "application/force-download", syslog_txt, do_html_post_and_get, do_log_cgi, do_auth },
#ifdef RTCONFIG_QTN  //RT-AC87U
	{ "tmp/qtn_diagnostics.cgi*", "application/force-download", NULL, NULL, do_qtn_diagnostics, do_auth },
#endif
#ifdef RTCONFIG_USB_MODEM
	{ "modemlog.txt*", "application/force-download", modemlog_txt, do_html_post_and_get, do_modemlog_cgi, do_auth },
#endif
#ifdef RTCONFIG_TCPDUMP
	{ "udhcpc.pcap*", "application/force-download", NULL, NULL, do_file, NULL },
#endif
#ifdef RTCONFIG_DSL
	{ "dsllog.cgi*", "text/txt", no_cache_IE7, do_html_post_and_get, do_adsllog_cgi, do_auth },
#endif
	// Viz 2010.08 vvvvv
	{ "update.cgi*", "text/javascript", no_cache_IE7, do_html_post_and_get, do_update_cgi, do_auth }, // jerry5
	{ "bwm/*.gz", NULL, no_cache, do_html_post_and_get, wo_bwmbackup, do_auth }, // jerry5
	// end Viz  ^^^^^^^^
	{ "**.pac", "application/x-ns-proxy-autoconfig", NULL, NULL, do_file, NULL },
	{ "wpad.dat", "application/x-ns-proxy-autoconfig", NULL, NULL, do_file, NULL },
#ifdef TRANSLATE_ON_FLY
	{ "change_lang.cgi*", "text/html", no_cache_IE7, do_lang_post, do_lang_cgi, do_auth },
#endif //TRANSLATE_ON_FLY
#ifdef RTCONFIG_OPENVPN
	{ "vpnupload.cgi*", "text/html", no_cache_IE7, do_vpnupload_post, do_vpnupload_cgi, do_auth },
#endif
	{ NULL, NULL, NULL, NULL, NULL, NULL }
};

// some should be removed
struct except_mime_handler except_mime_handlers[] = {
	{ "QIS_default.cgi", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "images/New_ui/login_bg.png", MIME_EXCEPTION_MAINPAGE},
	{ "images/New_ui/icon_titleName.png", MIME_EXCEPTION_MAINPAGE},
#if 0
	{ "QIS_*", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "qis/*", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "*.css", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "state.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "popup.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "general.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "help.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "help_content.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "validator.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "form.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "alttxt.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "start_autodet.asp", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
#ifdef RTCONFIG_QCA_PLC_UTILS
	{ "start_plcdet.asp", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
#endif	
	{ "start_dsl_autodet.asp", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "start_apply.htm", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "start_apply2.htm", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "apply.cgi", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "setting_lan.htm", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "status.asp", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "automac.asp", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "detecWAN.asp", MIME_EXCEPTION_NORESETTIME},
	{ "detecWAN2.asp", MIME_EXCEPTION_NORESETTIME},
	{ "WPS_info.xml", MIME_EXCEPTION_NORESETTIME},
	{ "WAN_info.asp", MIME_EXCEPTION_NOAUTH_ALL|MIME_EXCEPTION_NORESETTIME},
	{ "result_of_get_changed_status.asp", MIME_EXCEPTION_NORESETTIME},
	{ "result_of_get_changed_status_QIS.asp", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "result_of_detect_client.asp", MIME_EXCEPTION_NORESETTIME},
	{ "detect_firmware.asp", MIME_EXCEPTION_NOAUTH_ALL},
	{ "Nologin.asp", MIME_EXCEPTION_NOAUTH_ALL},
	{ "alertImg.gif", MIME_EXCEPTION_NOAUTH_ALL},
	{ "error_page.htm", MIME_EXCEPTION_NOAUTH_ALL},
	{ "jquery.js", MIME_EXCEPTION_NOAUTH_ALL},
	{ "require/require.min.js", MIME_EXCEPTION_NOAUTH_ALL},
	{ "gotoHomePage.htm", MIME_EXCEPTION_NOAUTH_ALL},
	{ "update_appstate.asp", MIME_EXCEPTION_NOAUTH_ALL},
	{ "update_applist.asp", MIME_EXCEPTION_NOAUTH_ALL},
	{ "update_cloudstatus.asp", MIME_EXCEPTION_NOAUTH_ALL},
	{ "upload.cgi", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "Uploading.asp", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "UploadError.asp", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "blocking.asp", MIME_EXCEPTION_NOAUTH_ALL},
	{ "images/New_ui/tm_logo_1.png", MIME_EXCEPTION_NOAUTH_ALL},
	{ "*.gz", MIME_EXCEPTION_NOAUTH_ALL},
	{ "*.tgz", MIME_EXCEPTION_NOAUTH_ALL},
	{ "*.zip", MIME_EXCEPTION_NOAUTH_ALL},
	{ "*.ipk", MIME_EXCEPTION_NOAUTH_ALL},
#ifdef RTCONFIG_FINDASUS
	{ "find_device.asp", MIME_EXCEPTION_NOAUTH_ALL},
#endif
	{ "Main_Login.asp", MIME_EXCEPTION_NOAUTH_ALL},
	{ "manifest.appcache", MIME_EXCEPTION_NOAUTH_ALL},
	{ "manifest.asp", MIME_EXCEPTION_NOAUTH_ALL},
	{ "offline.htm", MIME_EXCEPTION_NOAUTH_ALL},
	{ "httpd_check.htm", MIME_EXCEPTION_NOAUTH_ALL},
	{ "httpd_check.json", MIME_EXCEPTION_NOAUTH_ALL},
	{ "findasus.json", MIME_EXCEPTION_NOAUTH_ALL},
	{ "help_content.js", MIME_EXCEPTION_NOAUTH_ALL},
	{ "wcdma_list.js", MIME_EXCEPTION_NOAUTH_ALL},
	{ "update_clients.asp", MIME_EXCEPTION_NOAUTH_ALL},
#endif
	{ NULL, 0 }
};

// some should be referer
struct mime_referer mime_referers[] = {
	{ "start_apply.htm", CHECK_REFERER},
	{ "start_apply2.htm", CHECK_REFERER},
	{ "api.asp", CHECK_REFERER},
	{ "applyapp.cgi", CHECK_REFERER},
	{ "apply.cgi", CHECK_REFERER},
	{ "upgrade.cgi", CHECK_REFERER},
	{ "upload.cgi", CHECK_REFERER},
	{ "dsllog.cgi", CHECK_REFERER},
	{ "update.cgi", CHECK_REFERER},
	{ "vpnupload.cgi", CHECK_REFERER},
	{ "findasus.cgi", CHECK_REFERER},
	{ "ftpServerTree.cgi", CHECK_REFERER},
	{ NULL, 0 }
};

//2008.08 magic}
#ifdef RTCONFIG_USB
int ej_get_AiDisk_status(int eid, webs_t wp, int argc, char **argv){
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	int sh_num;
	char **folder_list = NULL;
	int first_pool, result, i;

	websWrite(wp, "function get_cifs_status(){\n");
	//websWrite(wp, "    return %d;\n", nvram_get_int("samba_running"));
	websWrite(wp, "    return %d;\n", nvram_get_int("enable_samba"));
	websWrite(wp, "}\n\n");

	websWrite(wp, "function get_ftp_status(){\n");
	//websWrite(wp, "    return %d;\n", nvram_get_int("ftp_running"));
	websWrite(wp, "    return %d;\n", nvram_get_int("enable_ftp"));
	websWrite(wp, "}\n\n");

#ifdef RTCONFIG_WEBDAV_PENDING
	websWrite(wp, "function get_webdav_status(){\n");
	//websWrite(wp, "    return %d;\n", nvram_get_int("ftp_running"));
	websWrite(wp, "    return %d;\n", nvram_get_int("enable_webdav"));
	websWrite(wp, "}\n\n");
#endif

	websWrite(wp, "function get_dms_status(){\n");
	websWrite(wp, "    return %d;\n", pids("ushare"));
	websWrite(wp, "}\n\n");

	websWrite(wp, "function get_share_management_status(protocol){\n");
	websWrite(wp, "    if (protocol == \"cifs\")\n");
	websWrite(wp, "	return %d;\n", (nvram_get("st_samba_force_mode") == NULL && nvram_get_int("st_samba_mode") == 1)?4:nvram_get_int("st_samba_mode"));
	websWrite(wp, "    else if (protocol == \"ftp\")\n");
	websWrite(wp, "	return %d;\n", (nvram_get("st_ftp_force_mode") == NULL && nvram_get_int("st_ftp_mode") == 1)?2:nvram_get_int("st_ftp_mode"));
#ifdef RTCONFIG_WEBDAV_PENDING
	websWrite(wp, "    else if (protocol == \"webdav\")\n");
	websWrite(wp, "	return %d;\n", nvram_get_int("st_webdav_mode"));
#endif
	websWrite(wp, "    else\n");
	websWrite(wp, "	return -1;\n");
	websWrite(wp, "}\n\n");

	disks_info = read_disk_data();
	if (disks_info == NULL){
		websWrite(wp, "function get_sharedfolder_in_pool(poolName){}\n");
		return -1;
	}
	first_pool = 1;
	websWrite(wp, "function get_sharedfolder_in_pool(poolName){\n");
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0){
				websWrite(wp, "    ");

				if (first_pool == 1)
					first_pool = 0;
				else
					websWrite(wp, "else ");

#ifdef OLD_AIDISK
				websWrite(wp, "if (poolName == \"%s\"){\n", rindex(follow_partition->mount_point, '/')+1);
#else
				websWrite(wp, "if (poolName == \"%s\"){\n", follow_partition->device);
#endif
				websWrite(wp, "	return [\"\"");

				result = get_all_folder(follow_partition->mount_point, &sh_num, &folder_list);
				if (result < 0){
					websWrite(wp, "];\n");
					websWrite(wp, "    }\n");

					printf("get_AiDisk_status: Can't get the folder list in \"%s\".\n", follow_partition->mount_point);

					free_2_dimension_list(&sh_num, &folder_list);

					continue;
				}

				for (i = 0; i < sh_num; ++i){
					websWrite(wp, ", ");

					websWrite(wp, "\"%s\"", folder_list[i]);
#if 0
//	tmp test
					printf("[httpd] chk folder list[%s]:\n", folder_list[i]);
					for (j=0; j<strlen(folder_list[i]); ++j)
					{
						printf("[%x] ", folder_list[i][j]);
					}
					printf("\nlen:(%d)\n", strlen(folder_list[i]));
//	tmp test ~
#endif

				}

				websWrite(wp, "];\n");
				websWrite(wp, "    }\n");
			}
		}

	websWrite(wp, "}\n\n");

	if (disks_info != NULL){
		free_2_dimension_list(&sh_num, &folder_list);
		free_disk_data(&disks_info);
	}

	return 0;
}

int ej_get_all_accounts(int eid, webs_t wp, int argc, char **argv){
	int acc_num;
	char **account_list = NULL;
	int result, i, first;
	char ascii_user[64];

	if ((result = get_account_list(&acc_num, &account_list)) < 0){
		printf("Failed to get the account list!\n");
		return -1;
	}

	first = 1;
	for (i = 0; i < acc_num; ++i){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		memset(ascii_user, 0, 64);
		char_to_ascii_safe(ascii_user, account_list[i], 64);

		websWrite(wp, "\"%s\"", ascii_user);
	}

	free_2_dimension_list(&acc_num, &account_list);
	return 0;
}

int
count_sddev_mountpoint()
{
	FILE *procpt;
	char line[PATH_MAX], devname[32], mpname[32], system_type[10], mount_mode[PATH_MAX];
	int dummy1, dummy2, count = 0;

	if((procpt = fopen(MOUNT_FILE, "r")) != NULL){
		while(fgets(line, sizeof(line), procpt)){
			if(sscanf(line, "%s %s %s %s %d %d", devname, mpname, system_type, mount_mode, &dummy1, &dummy2) != 6)
				continue;

			if(strstr(devname, "/dev/sd"))
				count++;
		}
	}

	if(procpt)
		fclose(procpt);

	return count;
}

static int notify_rc_for_nas(char *cmd)
{
	notify_rc_and_wait(cmd);
	return 0;
}

static int ej_safely_remove_disk(int eid, webs_t wp, int argc, char_t **argv){
	int result;
	char *disk_port = websGetVar(wp, "disk", "");
//	disk_info_t *disks_info = NULL, *follow_disk = NULL;
//	int disk_num = 0;
	int part_num = 0;
	char *fn = "safely_remove_disk_error";

	csprintf("disk_port = %s\n", disk_port);

	if(!strcmp(disk_port, "all")){
		result = eval("/sbin/ejusb", "-1", "0");
	}
	else{
		result = eval("/sbin/ejusb", disk_port, "0");
	}

	if (result != 0){
		insert_hook_func(wp, fn, "alert_msg.Action9");
		return -1;
	}

/*
	disks_info = read_disk_data();
	for(follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next, ++disk_num)
		;
	free_disk_data(&disks_info);
	csprintf("disk_num = %d\n", disk_num);
*/

	part_num = count_sddev_mountpoint();
	csprintf("part_num = %d\n", part_num);

//	if (disk_num > 1)
	if (part_num > 0)
	{
		result = eval("/sbin/check_proc_mounts_parts");
		result = notify_rc_for_nas("restart_nasapps");
	}
	else
	{
		result = notify_rc_for_nas("stop_nasapps");
	}

	insert_hook_func(wp, "safely_remove_disk_success", "");

	return 0;
}

int ej_get_permissions_of_account(int eid, webs_t wp, int argc, char **argv){
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	int acc_num = 0, sh_num = 0;
	char *account, **account_list = NULL, **folder_list;
	int samba_right, ftp_right;
#ifdef RTCONFIG_WEBDAV_PENDING
	int webdav_right;
#endif
	int result, i, j;
	int first_pool, first_account;
	char ascii_user[64];

	disks_info = read_disk_data();
	if (disks_info == NULL){
		websWrite(wp, "function get_account_permissions_in_pool(account, pool){return [];}\n");
		return -1;
	}

	result = get_account_list(&acc_num, &account_list);
	if (result < 0){
		printf("1. Can't get the account list.\n");
		free_2_dimension_list(&acc_num, &account_list);
		free_disk_data(&disks_info);
	}

	websWrite(wp, "function get_account_permissions_in_pool(account, pool){\n");

	first_account = 1;
	for (i = -1; i < acc_num; ++i){
		websWrite(wp, "    ");
		if (first_account == 1)
			first_account = 0;
		else
			websWrite(wp, "else ");

		if(i == -1){ // share mode.
			account = NULL;

			websWrite(wp, "if (account == null){\n");
		}
		else{
			account = account_list[i];

			memset(ascii_user, 0, 64);
			char_to_ascii_safe(ascii_user, account, 64);

			websWrite(wp, "if (account == \"%s\"){\n", ascii_user);
		}

		first_pool = 1;
		for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
			for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
				if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0){
					websWrite(wp, "	");
					if (first_pool == 1)
						first_pool = 0;
					else
						websWrite(wp, "else ");

#ifdef OLD_AIDISK
					websWrite(wp, "if (pool == \"%s\"){\n", rindex(follow_partition->mount_point, '/')+1);
#else
					websWrite(wp, "if (pool == \"%s\"){\n", follow_partition->device);
#endif

					websWrite(wp, "	    return [");

					result = get_all_folder(follow_partition->mount_point, &sh_num, &folder_list);

					// Pool's permission.
					samba_right = get_permission(account, follow_partition->mount_point, NULL, "cifs");
					if (samba_right < 0 || samba_right > 3){
						printf("Can't get the CIFS permission abount the pool: %s!\n", follow_partition->device);

						if(account == NULL || !strcmp(account, nvram_safe_get("http_username")))
							samba_right = DEFAULT_SAMBA_RIGHT;
						else
							samba_right = 0;
					}

					ftp_right = get_permission(account, follow_partition->mount_point, NULL, "ftp");
					if (ftp_right < 0 || ftp_right > 3){
						printf("Can't get the FTP permission abount the pool: %s!\n", follow_partition->device);

						if(account == NULL || !strcmp(account, nvram_safe_get("http_username")))
							ftp_right = DEFAULT_FTP_RIGHT;
						else
							ftp_right = 0;
					}

#ifdef RTCONFIG_WEBDAV_PENDING
					webdav_right = get_permission(account, follow_partition->mount_point, NULL, "webdav");
					if (webdav_right < 0 || webdav_right > 3){
						printf("Can't get the WEBDAV  permission abount the pool: %s!\n", follow_partition->device);

						if(account == NULL || !strcmp(account, nvram_safe_get("http_username")))
							webdav_right = DEFAULT_WEBDAV_RIGHT;
						else
							webdav_right = 0;
					}
#endif

#ifdef RTCONFIG_WEBDAV_PENDING
					websWrite(wp, "[\"\", %d, %d, %d]", samba_right, ftp_right, webdav_right);
#else
					websWrite(wp, "[\"\", %d, %d]", samba_right, ftp_right);
#endif
					if (result == 0 && sh_num > 0)
						websWrite(wp, ",\n");

					if (result != 0){
						websWrite(wp, "];\n");
						websWrite(wp, "	}\n");

						printf("get_permissions_of_account1: Can't get all folders in \"%s\".\n", follow_partition->mount_point);

						free_2_dimension_list(&sh_num, &folder_list);

						continue;
					}

					// Folder's permission.
					for (j = 0; j < sh_num; ++j){
						samba_right = get_permission(account, follow_partition->mount_point, folder_list[j], "cifs");
						ftp_right = get_permission(account, follow_partition->mount_point, folder_list[j], "ftp");
#ifdef RTCONFIG_WEBDAV_PENDING
						webdav_right = get_permission(account, follow_partition->mount_point, folder_list[j], "webdav");
#endif

						if (samba_right < 0 || samba_right > 3){
							printf("Can't get the CIFS permission abount \"%s\"!\n", folder_list[j]);

							if(account == NULL || !strcmp(account, nvram_safe_get("http_username")))
								samba_right = DEFAULT_SAMBA_RIGHT;
							else
								samba_right = 0;
						}

						if (ftp_right < 0 || ftp_right > 3){
							printf("Can't get the FTP permission abount \"%s\"!\n", folder_list[j]);

							if(account == NULL || !strcmp(account, nvram_safe_get("http_username")))
								ftp_right = DEFAULT_FTP_RIGHT;
							else
								ftp_right = 0;
						}

#ifdef RTCONFIG_WEBDAV_PENDING
						if (webdav_right < 0 || webdav_right > 3){
							printf("Can't get the WEBDAV permission abount \"%s\"!\n", folder_list[j]);

							if(account == NULL || !strcmp(account, nvram_safe_get("http_username")))
								webdav_right = DEFAULT_WEBDAV_RIGHT;
							else
								webdav_right = 0;
						}
#endif

#ifdef RTCONFIG_WEBDAV_PENDING
						websWrite(wp, "		    [\"%s\", %d, %d, %d]", folder_list[j], samba_right, ftp_right, webdav_right);
#else
						websWrite(wp, "		    [\"%s\", %d, %d]", folder_list[j], samba_right, ftp_right);
#endif

						if (j != sh_num-1)
							websWrite(wp, ",\n");
					}
					free_2_dimension_list(&sh_num, &folder_list);

					websWrite(wp, "];\n");
					websWrite(wp, "	}\n");
				}
			}
		}

		websWrite(wp, "    }\n");

	}
	free_2_dimension_list(&acc_num, &account_list);

	websWrite(wp, "}\n\n");

	if (disks_info != NULL)
		free_disk_data(&disks_info);

	return 0;
}

int ej_get_folder_tree(int eid, webs_t wp, int argc, char **argv){
	char *layer_order = websGetVar(wp, "layer_order", "");
	char *follow_info, *follow_info_end, backup;
	int layer = 0, first;
	int disk_count, partition_count, folder_count;
	int disk_order = -1, partition_order = -1;
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;

	if (strlen(layer_order) <= 0){
		printf("No input \"layer_order\"!\n");
		return -1;
	}

	follow_info = index(layer_order, '_');
	while (follow_info != NULL && *follow_info != 0){
		++layer;
		++follow_info;
		if (*follow_info == 0)
			break;
		follow_info_end = follow_info;
		while (*follow_info_end != 0 && isdigit(*follow_info_end))
			++follow_info_end;
		backup = *follow_info_end;
		*follow_info_end = 0;

		if (layer == 1)
			disk_order = atoi(follow_info);
		else if (layer == 2)
			partition_order = atoi(follow_info);
		else{
			*follow_info_end = backup;
			printf("Input \"%s\" is incorrect!\n", layer_order);
			return -1;
		}

		*follow_info_end = backup;
		follow_info = follow_info_end;
	}

	disks_info = read_disk_data();
	if (disks_info == NULL){
		printf("Can't read the information of disks.\n");
		return -1;
	}

	first = 1;
	disk_count = 0;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next, ++disk_count){
		partition_count = 0;
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next, ++partition_count){
			if (layer != 0 && follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0){
				int i;
				char **folder_list;
				int result;
				result = get_all_folder(follow_partition->mount_point, &folder_count, &folder_list);
				if (result < 0){
					printf("get_disk_tree: Can't get the folder list in \"%s\".\n", follow_partition->mount_point);

					folder_count = 0;
				}

				if (layer == 2 && disk_count == disk_order && partition_count == partition_order){
					for (i = 0; i < folder_count; ++i){
						if (first == 1)
							first = 0;
						else
							websWrite(wp, ", ");

						websWrite(wp, "\"%s#%u#0\"", folder_list[i], i);
					}
				}
				else if (layer == 1 && disk_count == disk_order){
					if (first == 1)
						first = 0;
					else
						websWrite(wp, ", ");

					follow_info = rindex(follow_partition->mount_point, '/');
					websWrite(wp, "\"%s#%u#%u\"", follow_info+1, partition_count, folder_count);
				}

				free_2_dimension_list(&folder_count, &folder_list);
			}
		}
		if (layer == 0){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "'%s#%u#%u'", follow_disk->tag, disk_count, partition_count);
		}

		if (layer > 0 && disk_count == disk_order)
			break;
	}

	free_disk_data(&disks_info);

	return 0;
}

int ej_get_share_tree(int eid, webs_t wp, int argc, char **argv){
	char *layer_order = websGetVar(wp, "layer_order", "");
	char *follow_info, *follow_info_end, backup;
	int layer = 0, first;
	int disk_count, partition_count, share_count;
	int disk_order = -1, partition_order = -1;
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;

	if (strlen(layer_order) <= 0){
		printf("No input \"layer_order\"!\n");
		return -1;
	}

	follow_info = index(layer_order, '_');
	while (follow_info != NULL && *follow_info != 0){
		++layer;
		++follow_info;
		if (*follow_info == 0)
			break;
		follow_info_end = follow_info;
		while (*follow_info_end != 0 && isdigit(*follow_info_end))
			++follow_info_end;
		backup = *follow_info_end;
		*follow_info_end = 0;

		if (layer == 1)
			disk_order = atoi(follow_info);
		else if (layer == 2)
			partition_order = atoi(follow_info);
		else{
			*follow_info_end = backup;
			printf("Input \"%s\" is incorrect!\n", layer_order);
			return -1;
		}

		*follow_info_end = backup;
		follow_info = follow_info_end;
	}

	disks_info = read_disk_data();
	if (disks_info == NULL){
		printf("Can't read the information of disks.\n");
		return -1;
	}

	first = 1;
	disk_count = 0;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next, ++disk_count){
		partition_count = 0;
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next, ++partition_count){
			if (layer != 0 && follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0){
				int i;
				char **share_list;
				int result;
				result = get_folder_list(follow_partition->mount_point, &share_count, &share_list);
				if (result < 0){
					printf("get_disk_tree: Can't get the share list in \"%s\".\n", follow_partition->mount_point);

					share_count = 0;
				}

				if (layer == 2 && disk_count == disk_order && partition_count == partition_order){
					for (i = 0; i < share_count; ++i){
						if (first == 1)
							first = 0;
						else
							websWrite(wp, ", ");

						websWrite(wp, "\"%s#%u#0\"", share_list[i], i);
					}
				}
				else if (layer == 1 && disk_count == disk_order){
					if (first == 1)
						first = 0;
					else
						websWrite(wp, ", ");

					follow_info = rindex(follow_partition->mount_point, '/');
					websWrite(wp, "\"%s#%u#%u\"", follow_info+1, partition_count, share_count);
				}

				free_2_dimension_list(&share_count, &share_list);
			}
		}
		if (layer == 0){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "'%s#%u#%u'", follow_disk->tag, disk_count, partition_count);
		}

		if (layer > 0 && disk_count == disk_order)
			break;
	}

	free_disk_data(&disks_info);

	return 0;
}

void not_ej_initial_folder_var_file()
{
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;

	disks_info = read_disk_data();
	if (disks_info == NULL)
		return;

	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next)
			if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0) {
				initial_folder_list(follow_partition->mount_point);
//				initial_all_var_file(follow_partition->mount_point);
			}

	free_disk_data(&disks_info);
}

int ej_initial_folder_var_file(int eid, webs_t wp, int argc, char **argv)
{
//	not_ej_initial_folder_var_file();
	return 0;
}

int ej_set_share_mode(int eid, webs_t wp, int argc, char **argv){

	struct json_object *root=NULL;
	root = json_tokener_parse(post_buf);

	int samba_mode = nvram_get_int("st_samba_mode");
	int samba_force_mode = nvram_get_int("st_samba_force_mode");
	int ftp_mode = nvram_get_int("st_ftp_mode");
	int ftp_force_mode = nvram_get_int("st_ftp_force_mode");
#ifdef RTCONFIG_WEBDAV_PENDING
	int webdav_mode = nvram_get_int("st_webdav_mode");
#endif
	char *dummyShareway = get_cgi_json("dummyShareway", root);
	char *protocol = get_cgi_json("protocol", root);
	char *mode = get_cgi_json("mode", root);
	int result;
	char *fn = "set_share_mode_error";

	if (!dummyShareway || strlen(dummyShareway) == 0){
		nvram_set("dummyShareway", "0");
	}else{
		nvram_set("dummyShareway", dummyShareway);
	}

	if (strlen(protocol) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input1");
		json_object_put(root);
		return -1;
	}
	if (strlen(mode) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input3");
		json_object_put(root);
		return -1;
	}
	if (!strcmp(mode, "share")){
		if (!strcmp(protocol, "cifs")){
			if((samba_mode == 1 || samba_mode == 3) && samba_force_mode == 1)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_samba_mode", "1");	// for test
		}
		else if (!strcmp(protocol, "ftp")){
			if (ftp_mode == 1 && ftp_force_mode == 1)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_ftp_mode", "1");
		}
#ifdef RTCONFIG_WEBDAV_PENDING
		else if (!strcmp(protocol, "webdav")){
			if (webdav_mode == 1)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_webdav_mode", "1");
		}
#endif
		else{
			insert_hook_func(wp, fn, "alert_msg.Input2");
			json_object_put(root);
			return -1;
		}
	}
	else if (!strcmp(mode, "account")){
		if (!strcmp(protocol, "cifs")){
			if((samba_mode == 2 || samba_mode == 4) && samba_force_mode == 2)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_samba_mode", "4");
		}
		else if (!strcmp(protocol, "ftp")){
			if(ftp_mode == 2 && ftp_force_mode == 2)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_ftp_mode", "2");
		}
#ifdef RTCONFIG_WEBDAV_PENDING
		else if (!strcmp(protocol, "webdav")){
			if (webdav_mode == 2)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_webdav_mode", "2");
		}
#endif
		else {
			insert_hook_func(wp, fn, "alert_msg.Input2");
			json_object_put(root);
			return -1;
		}
	}
	else{
		insert_hook_func(wp, fn, "alert_msg.Input4");
		json_object_put(root);
		return -1;
	}

	nvram_commit();

	not_ej_initial_folder_var_file();

	if (!strcmp(protocol, "cifs")) {
		result = notify_rc_for_nas("restart_samba_force");
	}
	else if (!strcmp(protocol, "ftp")) {
		result = notify_rc_for_nas("restart_ftpd_force");
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if (!strcmp(protocol, "webdav")) {
		result = notify_rc_for_nas("restart_webdav");
	}
#endif
	else {
		insert_hook_func(wp, fn, "alert_msg.Input2");
		json_object_put(root);
		return -1;
	}

	if (result != 0){
		insert_hook_func(wp, fn, "alert_msg.Action8");
		json_object_put(root);
		return -1;
	}

SET_SHARE_MODE_SUCCESS:
	insert_hook_func(wp, "set_share_mode_success", "");
	json_object_put(root);
	return 0;
}


int ej_modify_sharedfolder(int eid, webs_t wp, int argc, char **argv){
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char *new_folder = websGetVar(wp, "new_folder", "");
	char mount_path[PATH_MAX];
	char *fn = "modify_sharedfolder_error";

	if (strlen(pool) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input7");
		return -1;
	}
	if (strlen(folder) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input9");
		return -1;
	}
	if (strlen(new_folder) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input17");
		return -1;
	}
	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		insert_hook_func(wp, fn, "alert_msg.System1");
		return -1;
	}

	if (mod_folder(mount_path, folder, new_folder) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action7");
		return -1;
	}

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action7");
		return -1;
	}

	insert_hook_func(wp, "modify_sharedfolder_success", "");

	return 0;
}

int ej_delete_sharedfolder(int eid, webs_t wp, int argc, char **argv){
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char mount_path[PATH_MAX];
	char *fn = "delete_sharedfolder_error";

	if (strlen(pool) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input7");
		return -1;
	}
	if (strlen(folder) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input9");
		return -1;
	}

	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		insert_hook_func(wp, fn, "alert_msg.System1");
		return -1;
	}
	if (del_folder(mount_path, folder) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action6");
		return -1;
	}

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action6");
		return -1;
	}

	insert_hook_func(wp, "delete_sharedfolder_success", "");

	return 0;
}

int ej_create_sharedfolder(int eid, webs_t wp, int argc, char **argv){
	char *account = websGetVar(wp, "account", NULL);
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char mount_path[PATH_MAX];
	char *fn = "create_sharedfolder_error";

	if (strlen(pool) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input7");
		return -1;
	}
	if (strlen(folder) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input9");
		return -1;
	}

	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		fprintf(stderr, "Can't get the mount_path of %s.\n", pool);

		insert_hook_func(wp, fn, "alert_msg.System1");
		return -1;
	}
	if (add_folder(account, mount_path, folder) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action5");
		return -1;
	}

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action5");
		return -1;
	}
	insert_hook_func(wp, "create_sharedfolder_success", "");

	return 0;
}

int ej_set_AiDisk_status(int eid, webs_t wp, int argc, char **argv){
	char *protocol = websGetVar(wp, "protocol", "");
	char *flag = websGetVar(wp, "flag", "");
	int result = 0;
	char *fn = "set_AiDisk_status_error";

	if (strlen(protocol) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input1");
		return -1;
	}
	if (strlen(flag) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input18");
		return -1;
	}
	if (!strcmp(protocol, "cifs")){
		if (!strcmp(flag, "on")){
			nvram_set("enable_samba", "1");
			nvram_commit();
			result = notify_rc_for_nas("restart_samba");
		}
		else if (!strcmp(flag, "off")){
			nvram_set("enable_samba", "0");
			nvram_commit();
			if (!pids("smbd"))
				goto SET_AIDISK_STATUS_SUCCESS;

			result = notify_rc_for_nas("stop_samba");
		}
		else{
			insert_hook_func(wp, fn, "alert_msg.Input19");
			return -1;
		}
	}
	else if (!strcmp(protocol, "ftp")){
		if (!strcmp(flag, "on")){
			nvram_set("enable_ftp", "1");
			nvram_commit();
			result = notify_rc_for_nas("restart_ftpd");
		}
		else if (!strcmp(flag, "off")){
			nvram_set("enable_ftp", "0");
			nvram_commit();
			if (!pids("vsftpd"))
				goto SET_AIDISK_STATUS_SUCCESS;

			result = notify_rc_for_nas("stop_ftpd");
		}
		else{
			insert_hook_func(wp, fn, "alert_msg.Input19");
			return -1;
		}
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if (!strcmp(protocol, "webdav")){
		if (!strcmp(flag, "on")){
			nvram_set("enable_webdav", "1");
			nvram_commit();
			result = notify_rc_for_nas("restart_webdav");
		}
		else if (!strcmp(flag, "off")){
			nvram_set("enable_webdav", "0");
			nvram_commit();
			if (!pids("vsftpd"))
				goto SET_AIDISK_STATUS_SUCCESS;

			result = notify_rc_for_nas("stop_webdav");
		}
		else{
			insert_hook_func(wp, fn, "alert_msg.Input19");
			return -1;
		}
	}
#endif
	else{
		insert_hook_func(wp, fn, "alert_msg.Input2");
		return -1;
	}

	if (result != 0){
		insert_hook_func(wp, fn, "alert_msg.Action8");
		return -1;
	}

SET_AIDISK_STATUS_SUCCESS:
	//insert_hook_func(wp, "set_AiDisk_status_success", "");
	insert_hook_func(wp, "parent.resultOfSwitchAppStatus", "");

	return 0;
}

#ifdef RTCONFIG_WEBDAV

#define DEFAULT_WEBDAVPROXY_RIGHT 0

int add_webdav_account(char *account)
{
	char *nv, *nvp, *b;
	char new[256];
	char *acc, *right;
	int i, found;

	nv = nvp = strdup(nvram_safe_get("acc_webdavproxy"));

	if(nv) {
		i = found = 0;
		while ((b = strsep(&nvp, "<")) != NULL) {
			if((vstrsep(b, ">", &acc, &right) != 2)) continue;

			if(strcmp(acc, account)==0) {
				found = 1;
				break;
			}
			i++;
		}
		free(nv);

		if(!found) {
			if(i==0) sprintf(new, "%s>%d", account, DEFAULT_WEBDAVPROXY_RIGHT);
			else sprintf(new, "%s<%s>%d", nvram_safe_get("acc_webdavproxy"), account, DEFAULT_WEBDAVPROXY_RIGHT);

			nvram_set("acc_webdavproxy", new);
		}
	}

	return 0;
}


int del_webdav_account(char *account)
{
	char *nv, *nvp, *b;
	char new[256];
	char *acc, *right;
	int i;

	nv = nvp = strdup(nvram_safe_get("acc_webdavproxy"));

	if(nv) {
		i = 0;

		while ((b = strsep(&nvp, "<")) != NULL) {
			if((vstrsep(b, ">", &acc, &right) != 2)) continue;

			if(strcmp(acc, account)!=0) {
				if(i==0) sprintf(new, "%s>%s", acc, right);
				else sprintf(new, "%s<%s>%s", new, acc, right);
				i++;
			}
		}
		free(nv);

		if(i) nvram_set("acc_webdavproxy", new);
	}
	return 0;
}


int mod_webdav_account(char *account, char *newaccount)
{
	char *nv, *nvp, *b;
	char new[256];
	char *acc, *right;
	int i;

	nv = nvp = strdup(nvram_safe_get("acc_webdavproxy"));

	if(nv) {
		i = 0;

		while ((b = strsep(&nvp, "<")) != NULL) {
			if((vstrsep(b, ">", &acc, &right) != 2)) continue;

			if(strcmp(acc, account)!=0) {
				if(i==0) sprintf(new, "%s>%s", acc, right);
				else sprintf(new, "%s<%s>%s", new, acc, right);
			}
			else {
				if(i==0) sprintf(new, "%s>%s", newaccount, right);
				else sprintf(new, "%s<%s>%s", new, newaccount, right);
			}
			i++;
		}
		free(nv);

		if(i) nvram_set("acc_webdavproxy", new);
	}
	return 0;
}

#endif

int ej_modify_account(int eid, webs_t wp, int argc, char **argv){
	char *account = websGetVar(wp, "account", "");
	char *new_account = websGetVar(wp, "new_account", "");
	char *new_password = websGetVar(wp, "new_password", "");
	char *fn = "modify_account_error";

	if (strlen(account) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input5");
		return -1;
	}
	if (strlen(new_account) <= 0 && strlen(new_password) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input16");
		return -1;
	}

	if (mod_account(account, new_account, new_password) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action4");
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if(mod_webdav_account(account, new_account)<0) {
		insert_hook_func(wp, fn, "alert_msg.Action4");
		return -1;
	}
#endif

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action4");
		return -1;
	}

#ifdef RTCONFIG_WEBDAV_PENDING
	if(notify_rc_for_nas("restart_webdav") != 0) {
		insert_hook_func(wp, fn, "alert_msg.Action4");
		return -1;
	}
#endif

	insert_hook_func(wp, "modify_account_success", "");

	return 0;
}

int ej_delete_account(int eid, webs_t wp, int argc, char **argv){
	char *account = websGetVar(wp, "account", "");
	char *fn = "delete_account_error";

	if (strlen(account) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input5");
		return -1;
	}

	not_ej_initial_folder_var_file();

	if (del_account(account) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action3");
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if(del_webdav_account(account)<0) {
		insert_hook_func(wp, fn, "alert_msg.Action3");
		return -1;
	}
#endif

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action3");
		return -1;
	}

#ifdef RTCONFIG_WEBDAV_PENDING
	if(notify_rc_for_nas("restart_webdav") != 0) {
		insert_hook_func(wp, fn, "alert_msg.Action3");
		return -1;
	}
#endif

	insert_hook_func(wp, "delete_account_success", "");

	return 0;
}

int ej_initial_account(int eid, webs_t wp, int argc, char **argv){
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	char *command;
	int len;
	char *fn = "initial_account_error";

	nvram_set("acc_num", "0");
	nvram_set("acc_list", "");
	nvram_commit();

	disks_info = read_disk_data();
	if (disks_info == NULL){
		insert_hook_func(wp, fn, "alert_msg.System2");
		return -1;
	}

	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next)
			if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0){
				len = strlen("rm -f ")+strlen(follow_partition->mount_point)+strlen("/.__*");
				command = (char *)malloc(sizeof(char)*(len+1));
				if (command == NULL){
					insert_hook_func(wp, fn, "alert_msg.System1");
					return -1;
				}
				sprintf(command, "rm -f %s/.__*", follow_partition->mount_point);
				command[len] = 0;

				system(command);
				free(command);

				initial_folder_list(follow_partition->mount_point);
				initial_all_var_file(follow_partition->mount_point);
			}

	free_disk_data(&disks_info);

#if 0
	if (add_account(nvram_safe_get("http_username"), nvram_safe_get("http_passwd")) < 0)
#else
	// there are file_lock(), file_unlock() in add_account().
	// They would let the buffer of nvram_safe_get() be confused.
	char buf1[64], buf2[64];
	memset(buf1, 0, 64);
	memset(buf2, 0, 64);
	strcpy(buf1, nvram_safe_get("http_username"));
	strcpy(buf2, nvram_safe_get("http_passwd"));

	if(add_account(buf1, buf2) < 0)
#endif
	{
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if(add_webdav_account(nvram_safe_get("http_username"))<0) {
		insert_hook_func(wp, "init_account_error", "alert_msg.Action2");
		return -1;
	}
#endif

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}

#ifdef RTCONFIG_WEBDAV_PENDING
	if(notify_rc_for_nas("restart_webdav") != 0) {
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}
#endif
	insert_hook_func(wp, "initial_account_success", "");

	return 0;
}

int ej_create_account(int eid, webs_t wp, int argc, char **argv){
	char *account = websGetVar(wp, "account", "");
	char *password = websGetVar(wp, "password", "");
	char *fn = "create_account_error";

	if (strlen(account) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input5");
		return -1;
	}
	if (strlen(password) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input14");
		return -1;
	}

	not_ej_initial_folder_var_file();

	if (add_account(account, password) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if(add_webdav_account(account) < 0) {
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}
#endif

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}

#ifdef RTCONFIG_WEBDAV_PENDING
	if(notify_rc_for_nas("restart_webdav") != 0) {
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}
#endif

	insert_hook_func(wp, "create_account_success", "");
	return 0;
}

int ej_set_account_permission(int eid, webs_t wp, int argc, char **argv){
	char mount_path[PATH_MAX];
	char *ascii_user = websGetVar(wp, "account", NULL);
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", NULL);
	char *protocol = websGetVar(wp, "protocol", "");
	char *permission = websGetVar(wp, "permission", "");
#ifdef RTCONFIG_WEBDAV_PENDING
	char *webdavproxy = websGetVar(wp, "acc_webdavproxy", "");
#endif
	int right;
	char char_user[64];
	char *fn = "set_account_permission_error";

	memset(char_user, 0, 64);
	ascii_to_char_safe(char_user, ascii_user, 64);

	if (test_if_exist_account(char_user) != 1){
		insert_hook_func(wp, fn, "alert_msg.Input6");
		return -1;
	}

	if (strlen(pool) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input7");
		return -1;
	}

	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		fprintf(stderr, "Can't get the mount_path of %s.\n", pool);

		insert_hook_func(wp, fn, "alert_msg.System1");
		return -1;
	}

	if (strlen(protocol) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input1");
		return -1;
	}
	if (strcmp(protocol, "cifs") && strcmp(protocol, "ftp") && strcmp(protocol, "dms")
#ifdef RTCONFIG_WEBDAV_PENDING
&& strcmp(protocol, "webdav")
#endif
){
		insert_hook_func(wp, fn, "alert_msg.Input2");
		return -1;
	}

	if (strlen(permission) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input12");
		return -1;
	}
	right = atoi(permission);
	if (right < 0 || right > 3){
		insert_hook_func(wp, fn, "alert_msg.Input13");
		return -1;
	}

	if (set_permission(char_user, mount_path, folder, protocol, right) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action1");
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else {
		logmessage("wedavproxy right", "%s %s %s %s %d %s", char_user, mount_path, folder, protocol, right, webdavproxy);
		// modify permission for webdav proxy
		nvram_set("acc_webdavproxy", webdavproxy);
	}
#endif

#ifdef RTCONFIG_WEBDAV_PENDING
	if(strcmp(protocol, "webdav")==0) {
		if(notify_rc_for_nas("restart_webdav") != 0) {
			insert_hook_func(wp, fn, "alert_msg.Action1");
			return -1;
		}
	}
	else
#endif
	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action1");
		return -1;
	}

	insert_hook_func(wp, "set_account_permission_success", "");

	return 0;
}

int ej_set_account_all_folder_permission(int eid, webs_t wp, int argc, char **argv)
{
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	int i, result, sh_num;
	char **folder_list;
	char *ascii_user = websGetVar(wp, "account", NULL);
	char *protocol = websGetVar(wp, "protocol", "");
	char *permission = websGetVar(wp, "permission", "");
#ifdef RTCONFIG_WEBDAV_PENDING
	char *webdavproxy = websGetVar(wp, "acc_webdavproxy", "");
#endif
	int right;
	char char_user[64];
	char *fn = "set_account_all_folder_permission_error";

	memset(char_user, 0, 64);
	ascii_to_char_safe(char_user, ascii_user, 64);

	if (test_if_exist_account(char_user) != 1){
		insert_hook_func(wp, fn, "alert_msg.Input6");
		return -1;
	}

	if (strlen(protocol) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input1");
		return -1;
	}
	if (strcmp(protocol, "cifs") && strcmp(protocol, "ftp") && strcmp(protocol, "dms")
#ifdef RTCONFIG_WEBDAV_PENDING
&& strcmp(protocol, "webdav")
#endif
){
		insert_hook_func(wp, fn, "alert_msg.Input2");
		return -1;
	}

	if (strlen(permission) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input12");
		return -1;
	}
	right = atoi(permission);
	if (right < 0 || right > 3){
		insert_hook_func(wp, fn, "alert_msg.Input13");
		return -1;
	}


	disks_info = read_disk_data();
	if (disks_info == NULL){
		insert_hook_func(wp, fn, "alert_msg.System2");
		return -1;
	}

	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (follow_partition->mount_point == NULL || strlen(follow_partition->mount_point) <= 0)
				continue;

			result = get_all_folder(follow_partition->mount_point, &sh_num, &folder_list);
			if (result != 0) {
				insert_hook_func(wp, fn, "alert_msg.Action7");
				free_2_dimension_list(&sh_num, &folder_list);
				return -1;
			}
			for (i = 0; i < sh_num; ++i) {
				if (set_permission(char_user, follow_partition->mount_point, folder_list[i], protocol, right) < 0){
					insert_hook_func(wp, fn, "alert_msg.Action1");
					free_2_dimension_list(&sh_num, &folder_list);
					return -1;
				}
#ifdef RTCONFIG_WEBDAV_PENDING
#error FIXME
				else {
					logmessage("wedavproxy right", "%s %s %s %s %d %s", char_user, mount_path, folder, protocol, right, webdavproxy);
					// modify permission for webdav proxy
					nvram_set("acc_webdavproxy", webdavproxy);
				}
#endif
			}
			free_2_dimension_list(&sh_num, &folder_list);
		}
	}

	free_disk_data(&disks_info);

#ifdef RTCONFIG_WEBDAV_PENDING
	if(strcmp(protocol, "webdav")==0) {
		if(notify_rc_for_nas("restart_webdav") != 0) {
			insert_hook_func(wp, fn, "alert_msg.Action1");
			return -1;
		}
	}
	else
#endif
	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action1");
		return -1;
	}

	insert_hook_func(wp, "set_account_all_folder_permission_success", "");

	return 0;
}

int ej_apps_fsck_ret(int eid, webs_t wp, int argc, char **argv)
{
#ifdef RTCONFIG_DISK_MONITOR
	disk_info_t *disk_list, *disk_info;
	partition_info_t *partition_info;
	FILE *fp;

	disk_list = read_disk_data();
	if(disk_list == NULL){
		websWrite(wp, "[]");
		return -1;
	}

	websWrite(wp, "[");
	for(disk_info = disk_list; disk_info != NULL; disk_info = disk_info->next){
		for(partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next){
			websWrite(wp, "[\"%s\", ", partition_info->device);

#define MAX_ERROR_CODE 3
			int error_code, got_code;
			char file_name[32];

			for(error_code = 0, got_code = 0; error_code <= MAX_ERROR_CODE; ++error_code){
				memset(file_name, 0, 32);
				sprintf(file_name, "/tmp/fsck_ret/%s.%d", partition_info->device, error_code);

				if((fp = fopen(file_name, "r")) != NULL){
					fclose(fp);
					websWrite(wp, "\"%d\"", error_code);
					got_code = 1;
					break;
				}
			}

			if(!got_code)
				websWrite(wp, "\"\"");

			websWrite(wp, "]%s", (partition_info->next)?", ":"");
		}

		websWrite(wp, "%s", (disk_info->next)?", ":"");
	}
	websWrite(wp, "]");

	free_disk_data(&disk_list);

	return 0;
#endif

	websWrite(wp, "[]");
	return 0;
}

#ifdef RTCONFIG_DISK_MONITOR
int ej_apps_fsck_log(int eid, webs_t wp, int argc, char **argv)
{
	disk_info_t *disk_list, *disk_info;
	partition_info_t *partition_info;
	char file_name[32], d_port[16], *d_dot;
	char *port_path = websGetVar(wp, "diskmon_usbport", "-1");
	int ret, all_disk;

	disk_list = read_disk_data();
	if(disk_list == NULL){
		return -1;
	}

	all_disk = (atoi(port_path) == -1)? 1 : 0;
	for(disk_info = disk_list; disk_info != NULL; disk_info = disk_info->next){
		/* If hub port number is not specified in port_path,
		 * don't compare it with hub port number in disk_info->port.
		 */
		strlcpy(d_port, disk_info->port, sizeof(d_port));
		if (!strchr(port_path, '.') && d_dot)
			*d_dot = '\0';
		if (!all_disk && strcmp(d_port, port_path))
			continue;

		for(partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next){
			memset(file_name, 0, 32);
			sprintf(file_name, "/tmp/fsck_ret/%s.log", partition_info->device);
			ret = dump_file(wp, file_name);

			if(ret)
				websWrite(wp, "\n\n");
		}
	}

	free_disk_data(&disk_list);

	return 0;
}
#endif

// argv[0] = "all" or NULL: show all lists, "asus": only show the list of ASUS, "others": show other lists.
// apps_info: ["Name", "Version", "New Version", "Installed", "Enabled", "Source", "URL", "Description", "Depends", "Optional Utility", "New Optional Utility", "Help Path", "New File name"].
int ej_apps_info(int eid, webs_t wp, int argc, char **argv)
{
	apps_info_t *follow_apps_info, *apps_info_list;
	char *name;

	if (ejArgs(argc, argv, "%s", &name) < 1)
		name = APP_OWNER_ALL;
	if (strcmp(name, APP_OWNER_ALL) != 0 &&
	    strcmp(name, APP_OWNER_ASUS) != 0 &&
	    strcmp(name, APP_OWNER_OTHERS) != 0) {
		websWrite(wp, "[]");
		return 0;
	}

	apps_info_list = follow_apps_info = get_apps_list(name);
	websWrite(wp, "[");
	while (follow_apps_info != NULL) {
		websWrite(wp, "[");
		websWrite(wp, "\"%s\", ", follow_apps_info->name ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->version ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->new_version ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->installed ? : "" /* Why not FIELD_NO? */);
		websWrite(wp, "\"%s\", ", follow_apps_info->enabled ? : FIELD_YES);
		websWrite(wp, "\"%s\", ", follow_apps_info->source ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->url ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->description ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->depends ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->optional_utility ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->new_optional_utility ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->help_path ? : "");
		websWrite(wp, "\"%s\"",   follow_apps_info->new_file_name ? : "");

		follow_apps_info = follow_apps_info->next;
		websWrite(wp, "]%s\n", follow_apps_info ? "," : "");
	}
	websWrite(wp, "]");
	free_apps_list(&apps_info_list);

	return 0;
}

int ej_apps_state_info(int eid, webs_t wp, int argc, char **argv){
	char *cmd[] = {"chk_app_state", NULL};
	int pid;

	_eval(cmd, NULL, 0, &pid);

	websWrite(wp, "apps_dev = \"%s\";", nvram_safe_get("apps_dev"));
	websWrite(wp, "apps_mounted_path = \"%s\";", nvram_safe_get("apps_mounted_path"));

	websWrite(wp, "apps_state_upgrade = \"%s\";", nvram_safe_get("apps_state_upgrade"));
	websWrite(wp, "apps_state_update = \"%s\";", nvram_safe_get("apps_state_update"));
	websWrite(wp, "apps_state_remove = \"%s\";", nvram_safe_get("apps_state_remove"));
	websWrite(wp, "apps_state_enable = \"%s\";", nvram_safe_get("apps_state_enable"));
	websWrite(wp, "apps_state_switch = \"%s\";", nvram_safe_get("apps_state_switch"));
	websWrite(wp, "apps_state_autorun = \"%s\";", nvram_safe_get("apps_state_autorun"));
	websWrite(wp, "apps_state_install = \"%s\";", nvram_safe_get("apps_state_install"));
	websWrite(wp, "apps_state_error = \"%s\";", nvram_safe_get("apps_state_error"));

	websWrite(wp, "apps_download_file = \"%s\";", nvram_safe_get("apps_download_file"));
	websWrite(wp, "apps_download_percent = \"%s\";", nvram_safe_get("apps_download_percent"));

	websWrite(wp, "apps_depend_do = \"%s\";", nvram_safe_get("apps_depend_do"));
	websWrite(wp, "apps_depend_action = \"%s\";", nvram_safe_get("apps_depend_action"));
	websWrite(wp, "apps_depend_action_target = \"%s\";", nvram_safe_get("apps_depend_action_target"));

	return 0;
}

int ej_apps_action(int eid, webs_t wp, int argc, char **argv){
	char *apps_action = websGetVar(wp, "apps_action", "");
	char *apps_name = websGetVar(wp, "apps_name", "");
	char *apps_flag = websGetVar(wp, "apps_flag", "");
	char command[128];

	if(strlen(apps_action) <= 0)
		return 0;

	nvram_set("apps_state_action", apps_action);

	memset(command, 0, sizeof(command));

	if(!strcmp(apps_action, "install")){
		if(strlen(apps_name) <= 0 || strlen(apps_flag) <= 0)
			return 0;

		snprintf(command, sizeof(command), "start_apps_install %s %s", apps_name, apps_flag);
	}
	else if(!strcmp(apps_action, "stop")){
		snprintf(command, sizeof(command), "start_apps_stop");
	}
	else if(!strcmp(apps_action, "update")){
		snprintf(command, sizeof(command), "start_apps_update");
	}
	else if(!strcmp(apps_action, "upgrade")){
		if(strlen(apps_name) <= 0)
			return 0;

		snprintf(command, sizeof(command), "start_apps_upgrade %s", apps_name);
	}
	else if(!strcmp(apps_action, "remove")){
		if(strlen(apps_name) <= 0)
			return 0;

		snprintf(command, sizeof(command), "start_apps_remove %s", apps_name);
	}
	else if(!strcmp(apps_action, "enable")){
		if(strlen(apps_name) <= 0 || strlen(apps_flag) <= 0)
			return 0;

		if(strcmp(apps_flag, "yes") && strcmp(apps_flag, "no"))
			return 0;

		snprintf(command, sizeof(command), "start_apps_enable %s %s", apps_name, apps_flag);
	}
	else if(!strcmp(apps_action, "switch")){
		if(strlen(apps_name) <= 0 || strlen(apps_flag) <= 0)
			return 0;

		snprintf(command, sizeof(command), "start_apps_switch %s %s", apps_name, apps_flag);
	}
	else if(!strcmp(apps_action, "cancel")){
		snprintf(command, sizeof(command), "start_apps_cancel");
	}

	if(strlen(command) > 0)
		notify_rc(command);

	return 0;
}

#ifdef RTCONFIG_MEDIA_SERVER
// dms_info: ["Scanning"] or [ "" ]

int ej_dms_info(int eid, webs_t wp, int argc, char **argv){
	char *dms_dbcwd;
	char dms_scanfile[PATH_MAX], dms_status[32];
	FILE *fp;

	dms_dbcwd = nvram_safe_get("dms_dbcwd");

	strcpy(dms_status, "");

	if(nvram_get_int("dms_enable") && strlen(dms_dbcwd))
	{
		sprintf(dms_scanfile, "%s/scantag", dms_dbcwd);

		fp = fopen(dms_scanfile, "r");

		if(fp) {
			strcpy(dms_status, "Scanning");
			fclose(fp);
		}
	}

	websWrite(wp, "[\"%s\"]", dms_status);

	return 0;
}
#endif

//#ifdef RTCONFIG_CLOUDSYNC
static char *convert_cloudsync_status(const char *status_code){
	if(!strcmp(status_code, "STATUS:70"))
		return "INITIAL";
	else if(!strcmp(status_code, "STATUS:71"))
		return "SYNC";
	else if(!strcmp(status_code, "STATUS:72"))
		return "DOWNUP";
	else if(!strcmp(status_code, "STATUS:73"))
		return "UPLOAD";
	else if(!strcmp(status_code, "STATUS:74"))
		return "DOWNLOAD";
	else if(!strcmp(status_code, "STATUS:75"))
		return "STOP";
	else if(!strcmp(status_code, "STATUS:77"))
		return "INPUT CAPTCHA";
	else
		return "ERROR";
}

// cloud_sync = "content of nvram: cloud_sync"
// cloud_status = "string of status"
// cloud_obj = "handled object"
// cloud_msg = "error message"
int ej_cloud_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/smartsync/.logs/asuswebstorage", "r");
	char line[PATH_MAX], buf[PATH_MAX];
	int line_num;
	char status[16], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX];

	websWrite(wp, "cloud_sync=\"%s\";\n", nvram_safe_get("cloud_sync"));

	if(fp == NULL){
		websWrite(wp, "cloud_status=\"ERROR\";\n");
		websWrite(wp, "cloud_obj=\"\";\n");
		websWrite(wp, "cloud_msg=\"\";\n");
		return 0;
	}

	memset(status, 0, 16);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		switch(line_num){
			case 1:
				strncpy(status, convert_cloudsync_status(line), 16);
				break;
			case 2:
				memset(buf, 0, PATH_MAX);
				char_to_ascii(buf, line);
				strcpy(mounted_path, buf);
				break;
			case 3:
				// memset(buf, 0, PATH_MAX);
				// char_to_ascii(buf, line);
				// strcpy(target_obj, buf);
				strcpy(target_obj, line); // support Chinese
				break;
			case 4:
				strcpy(error_msg, line);
				break;
		}

		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "cloud_status=\"ERROR\";\n");
		websWrite(wp, "cloud_obj=\"\";\n");
		websWrite(wp, "cloud_msg=\"\";\n");
	}
	else{
		websWrite(wp, "cloud_status=\"%s\";\n", status);
		websWrite(wp, "cloud_obj=\"%s\";\n", target_obj);
		websWrite(wp, "cloud_msg=\"%s\";\n", error_msg);
	}

	return 0;
}

//Viz add to get partial string 2012.11.13
void substr(char *dest, const char* src, unsigned int start, unsigned int cnt) {
	strncpy(dest, src + start, cnt);
	dest[cnt] = 0;
}

//use for UI to avoid variable 'cloud_sync' JavaScript error, Jieming added at 2012.09.11
int ej_UI_cloud_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/smartsync/.logs/asuswebstorage", "r");
	char line[PATH_MAX], buf[PATH_MAX], dest[PATH_MAX];
	int line_num;
	char status[16], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX], full_capa[PATH_MAX], used_capa[PATH_MAX], captcha_url[PATH_MAX];

	if(fp == NULL){
		websWrite(wp, "cloud_status=\"WAITING\";\n"); //gauss change status fromm 'ERROR' to 'WAITING' 2014.11.4
		websWrite(wp, "cloud_obj=\"\";\n");
		websWrite(wp, "cloud_msg=\"\";\n");
		websWrite(wp, "cloud_fullcapa=\"\";\n");
		websWrite(wp, "cloud_usedcapa=\"\";\n");
		websWrite(wp, "CAPTCHA_URL=\"\";\n");
		return 0;
	}

	memset(status, 0, 16);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);
	memset(full_capa, 0, PATH_MAX);
	memset(used_capa, 0, PATH_MAX);
	memset(captcha_url, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		if(strstr(line, "STATUS") != NULL){
			strncpy(status, convert_cloudsync_status(line), 16);
		}
		else if(strstr(line, "MOUNT_PATH") != NULL){
			memset(buf, 0, PATH_MAX);
			substr(dest, line, 11, PATH_MAX-11);
			char_to_ascii(buf, dest);
			strcpy(mounted_path, buf);
		}
		else if(strstr(line, "FILENAME") != NULL){
			substr(dest, line, 9, PATH_MAX-9);
			strcpy(target_obj, dest); // support Chinese
		}
		else if(strstr(line, "ERR_MSG") != NULL){
			substr(dest, line, 8, PATH_MAX-8);
			strcpy(error_msg, dest);
		}
		else if(strstr(line, "TOTAL_SPACE") != NULL){
			substr(dest, line, 12, PATH_MAX-12);
			strcpy(full_capa, dest);
		}
		else if(strstr(line, "USED_SPACE") != NULL){
			substr(dest, line, 11, PATH_MAX-11);
			strcpy(used_capa, dest);
		}
		else if(strstr(line, "CAPTCHA_URL") != NULL){
			substr(dest, line, 12, PATH_MAX-12);
			strcpy(captcha_url, dest);
		}

		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "cloud_status=\"ERROR\";\n");
		websWrite(wp, "cloud_obj=\"\";\n");
		websWrite(wp, "cloud_msg=\"\";\n");
		websWrite(wp, "cloud_fullcapa=\"\";\n");
		websWrite(wp, "cloud_usedcapa=\"\";\n");
		websWrite(wp, "CAPTCHA_URL=\"\";\n");
	}
	else{
		websWrite(wp, "cloud_status=\"%s\";\n", status);
		websWrite(wp, "cloud_obj=\"%s\";\n", target_obj);
		if(!strcmp(status,"SYNC"))
		   strncpy(error_msg,"Sync has been completed",PATH_MAX);
		else if(!strcmp(status,"INITIAL"))
		   strncpy(error_msg,"Verifying",PATH_MAX);	
		websWrite(wp, "cloud_msg=\"%s\";\n", error_msg);
		websWrite(wp, "cloud_fullcapa=\"%s\";\n", full_capa);
		websWrite(wp, "cloud_usedcapa=\"%s\";\n", used_capa);
		websWrite(wp, "CAPTCHA_URL=\"%s\";\n", captcha_url);
	}

	return 0;
}

int ej_UI_cloud_dropbox_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/smartsync/.logs/dropbox", "r");
	char line[PATH_MAX], buf[PATH_MAX], dest[PATH_MAX];
	int line_num;
	char status[16], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX], full_capa[PATH_MAX], used_capa[PATH_MAX], rule_num[PATH_MAX];

	if(fp == NULL){
		websWrite(wp, "cloud_dropbox_status=\"WAITING\";\n"); //gauss change status fromm 'ERROR' to 'WAITING' 2014.11.4
		websWrite(wp, "cloud_dropbox_obj=\"\";\n");
		websWrite(wp, "cloud_dropbox_msg=\"\";\n");
		websWrite(wp, "cloud_dropbox_fullcapa=\"\";\n");
		websWrite(wp, "cloud_dropbox_usedcapa=\"\";\n");
		websWrite(wp, "cloud_dropbox_rule_num=\"\";\n");
		return 0;
	}

	memset(status, 0, 16);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);
	memset(full_capa, 0, PATH_MAX);
	memset(used_capa, 0, PATH_MAX);
	memset(rule_num, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		if(strstr(line, "STATUS") != NULL){
			strncpy(status, convert_cloudsync_status(line), 16);
		}
		else if(strstr(line, "MOUNT_PATH") != NULL){
			memset(buf, 0, PATH_MAX);
			substr(dest, line, 11, PATH_MAX);
			char_to_ascii(buf, dest);
			strcpy(mounted_path, buf);
		}
		else if(strstr(line, "FILENAME") != NULL){
			substr(dest, line, 9, PATH_MAX);
			strcpy(target_obj, dest); // support Chinese
		}
		else if(strstr(line, "ERR_MSG") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(error_msg, dest);
		}
		else if(strstr(line, "TOTAL_SPACE") != NULL){
			substr(dest, line, 12, PATH_MAX);
			strcpy(full_capa, dest);
		}
		else if(strstr(line, "USED_SPACE") != NULL){
			substr(dest, line, 11, PATH_MAX);
			strcpy(used_capa, dest);
		}
		else if(strstr(line, "RULENUM") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(rule_num, dest);
		}
		
		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "cloud_dropbox_status=\"ERROR\";\n");
		websWrite(wp, "cloud_dropbox_obj=\"\";\n");
		websWrite(wp, "cloud_dropbox_msg=\"\";\n");
		websWrite(wp, "cloud_dropbox_fullcapa=\"\";\n");
		websWrite(wp, "cloud_dropbox_usedcapa=\"\";\n");
		websWrite(wp, "cloud_dropbox_rule_num=\"\";\n");
	}
	else{
		websWrite(wp, "cloud_dropbox_status=\"%s\";\n", status);
		websWrite(wp, "cloud_dropbox_obj=\"%s\";\n", target_obj);
		if(!strcmp(status,"SYNC"))
		   strncpy(error_msg,"Sync has been completed",PATH_MAX);
		else if(!strcmp(status,"INITIAL"))
		   strncpy(error_msg,"Verifying",PATH_MAX);
		websWrite(wp, "cloud_dropbox_msg=\"%s\";\n", error_msg);
		websWrite(wp, "cloud_dropbox_fullcapa=\"%s\";\n", full_capa);
		websWrite(wp, "cloud_dropbox_usedcapa=\"%s\";\n", used_capa);
		websWrite(wp, "cloud_dropbox_rule_num=\"%s\";\n", rule_num);
	}

	return 0;
}

int ej_UI_cloud_ftpclient_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/smartsync/.logs/ftpclient", "r");
	char line[PATH_MAX], buf[PATH_MAX], dest[PATH_MAX];
	int line_num;
	char status[16], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX], full_capa[PATH_MAX], used_capa[PATH_MAX], rule_num[PATH_MAX];

	if(fp == NULL){
		websWrite(wp, "cloud_ftpclient_status=\"WAITING\";\n"); //gauss change status fromm 'ERROR' to 'WAITING' 2014.11.4
		websWrite(wp, "cloud_ftpclient_obj=\"\";\n");
		websWrite(wp, "cloud_ftpclient_msg=\"\";\n");
		websWrite(wp, "cloud_ftpclient_fullcapa=\"\";\n");
		websWrite(wp, "cloud_ftpclient_usedcapa=\"\";\n");
		websWrite(wp, "cloud_ftpclient_rule_num=\"\";\n");
		return 0;
	}

	memset(status, 0, 16);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);
	memset(full_capa, 0, PATH_MAX);
	memset(used_capa, 0, PATH_MAX);
	memset(rule_num, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		if(strstr(line, "STATUS") != NULL){
			strncpy(status, convert_cloudsync_status(line), 16);
		}
		else if(strstr(line, "MOUNT_PATH") != NULL){
			memset(buf, 0, PATH_MAX);
			substr(dest, line, 11, PATH_MAX);
			char_to_ascii(buf, dest);
			strcpy(mounted_path, buf);
		}
		else if(strstr(line, "FILENAME") != NULL){
			substr(dest, line, 9, PATH_MAX);
			strcpy(target_obj, dest); // support Chinese
		}
		else if(strstr(line, "ERR_MSG") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(error_msg, dest);
		}
		else if(strstr(line, "TOTAL_SPACE") != NULL){
			substr(dest, line, 12, PATH_MAX);
			strcpy(full_capa, dest);
		}
		else if(strstr(line, "USED_SPACE") != NULL){
			substr(dest, line, 11, PATH_MAX);
			strcpy(used_capa, dest);
		}
		else if(strstr(line, "RULENUM") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(rule_num, dest);
		}
		
		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "cloud_ftpclient_status=\"ERROR\";\n");
		websWrite(wp, "cloud_ftpclient_obj=\"\";\n");
		websWrite(wp, "cloud_ftpclient_msg=\"\";\n");
		websWrite(wp, "cloud_ftpclient_fullcapa=\"\";\n");
		websWrite(wp, "cloud_ftpclient_usedcapa=\"\";\n");
		websWrite(wp, "cloud_ftpclient_rule_num=\"\";\n");
	}
	else{
		websWrite(wp, "cloud_ftpclient_status=\"%s\";\n", status);
		websWrite(wp, "cloud_ftpclient_obj=\"%s\";\n", target_obj);
		if(!strcmp(status,"SYNC"))
		   strncpy(error_msg,"Sync has been completed",PATH_MAX);
		else if(!strcmp(status,"INITIAL"))
		   strncpy(error_msg,"Verifying",PATH_MAX);	
		websWrite(wp, "cloud_ftpclient_msg=\"%s\";\n", error_msg);
		websWrite(wp, "cloud_ftpclient_fullcapa=\"%s\";\n", full_capa);
		websWrite(wp, "cloud_ftpclient_usedcapa=\"%s\";\n", used_capa);
		websWrite(wp, "cloud_ftpclient_rule_num=\"%s\";\n", rule_num);
	}

	return 0;
}

int ej_UI_cloud_sambaclient_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/smartsync/.logs/sambaclient", "r");
	char line[PATH_MAX], buf[PATH_MAX], dest[PATH_MAX];
	int line_num;
	char status[16], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX], full_capa[PATH_MAX], used_capa[PATH_MAX], rule_num[PATH_MAX];

	if(fp == NULL){
		websWrite(wp, "cloud_sambaclient_status=\"WAITING\";\n"); //gauss change status fromm 'ERROR' to 'WAITING' 2014.11.4
		websWrite(wp, "cloud_sambaclient_obj=\"\";\n");
		websWrite(wp, "cloud_sambaclient_msg=\"\";\n");
		websWrite(wp, "cloud_sambaclient_fullcapa=\"\";\n");
		websWrite(wp, "cloud_sambaclient_usedcapa=\"\";\n");
		websWrite(wp, "cloud_sambaclient_rule_num=\"\";\n");
		return 0;
	}

	memset(status, 0, 16);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);
	memset(full_capa, 0, PATH_MAX);
	memset(used_capa, 0, PATH_MAX);
	memset(rule_num, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		if(strstr(line, "STATUS") != NULL){
			strncpy(status, convert_cloudsync_status(line), 16);
		}
		else if(strstr(line, "MOUNT_PATH") != NULL){
			memset(buf, 0, PATH_MAX);
			substr(dest, line, 11, PATH_MAX);
			char_to_ascii(buf, dest);
			strcpy(mounted_path, buf);
		}
		else if(strstr(line, "FILENAME") != NULL){
			substr(dest, line, 9, PATH_MAX);
			strcpy(target_obj, dest); // support Chinese
		}
		else if(strstr(line, "ERR_MSG") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(error_msg, dest);
		}
		else if(strstr(line, "TOTAL_SPACE") != NULL){
			substr(dest, line, 12, PATH_MAX);
			strcpy(full_capa, dest);
		}
		else if(strstr(line, "USED_SPACE") != NULL){
			substr(dest, line, 11, PATH_MAX);
			strcpy(used_capa, dest);
		}
		else if(strstr(line, "RULENUM") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(rule_num, dest);
		}
		
		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "cloud_sambaclient_status=\"ERROR\";\n");
		websWrite(wp, "cloud_sambaclient_obj=\"\";\n");
		websWrite(wp, "cloud_sambaclient_msg=\"\";\n");
		websWrite(wp, "cloud_sambaclient_fullcapa=\"\";\n");
		websWrite(wp, "cloud_sambaclient_usedcapa=\"\";\n");
		websWrite(wp, "cloud_sambaclient_rule_num=\"\";\n");
	}
	else{
		websWrite(wp, "cloud_sambaclient_status=\"%s\";\n", status);
		websWrite(wp, "cloud_sambaclient_obj=\"%s\";\n", target_obj);
		if(!strcmp(status,"SYNC"))
		   strncpy(error_msg,"Sync has been completed",PATH_MAX);
		else if(!strcmp(status,"INITIAL"))
		   strncpy(error_msg,"Verifying",PATH_MAX);		
		websWrite(wp, "cloud_sambaclient_msg=\"%s\";\n", error_msg);
		websWrite(wp, "cloud_sambaclient_fullcapa=\"%s\";\n", full_capa);
		websWrite(wp, "cloud_sambaclient_usedcapa=\"%s\";\n", used_capa);
		websWrite(wp, "cloud_sambaclient_rule_num=\"%s\";\n", rule_num);
	}

	return 0;
}

int ej_UI_rs_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/Cloud/log/WebDAV", "r");
	char line[PATH_MAX], buf[PATH_MAX], dest[PATH_MAX];
	int line_num;
	char rulenum[PATH_MAX], status[16], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX], full_capa[PATH_MAX], used_capa[PATH_MAX];

	if(fp == NULL){
		websWrite(wp, "rs_rulenum=\"\";\n");
		websWrite(wp, "rs_status=\"WAITING\";\n"); //gauss change status fromm 'ERROR' to 'WAITING' 2014.11.4
		websWrite(wp, "rs_obj=\"\";\n");
		websWrite(wp, "rs_msg=\"\";\n");
		websWrite(wp, "rs_fullcapa=\"\";\n");
		websWrite(wp, "rs_usedcapa=\"\";\n");
		return 0;
	}

	memset(status, 0, 16);
	memset(rulenum, 0, PATH_MAX);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);
	memset(full_capa, 0, PATH_MAX);
	memset(used_capa, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		if(strstr(line, "STATUS") != NULL){
			strncpy(status, convert_cloudsync_status(line), 16);
		}
		else if(strstr(line, "RULENUM") != NULL){
			substr(dest, line, 8, PATH_MAX-8);
			strcpy(rulenum, dest);
		}
		else if(strstr(line, "MOUNT_PATH") != NULL){
			memset(buf, 0, PATH_MAX);
			substr(dest, line, 11, PATH_MAX-11);
			char_to_ascii(buf, dest);
			strcpy(mounted_path, buf);
		}
		else if(strstr(line, "FILENAME") != NULL){
			substr(dest, line, 9, PATH_MAX-9);
			strcpy(target_obj, dest); // support Chinese
		}
		else if(strstr(line, "ERR_MSG") != NULL){
			substr(dest, line, 8, PATH_MAX-8);
			strcpy(error_msg, dest);
		}
		else if(strstr(line, "TOTAL_SPACE") != NULL){
			substr(dest, line, 12, PATH_MAX-12);
			strcpy(full_capa, dest);
		}
		else if(strstr(line, "USED_SPACE") != NULL){
			substr(dest, line, 11, PATH_MAX-11);
			strcpy(used_capa, dest);
		}

		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "rs_rulenum=\"\";\n");
		websWrite(wp, "rs_status=\"ERROR\";\n");
		websWrite(wp, "rs_obj=\"\";\n");
		websWrite(wp, "rs_msg=\"\";\n");
		websWrite(wp, "rs_fullcapa=\"\";\n");
		websWrite(wp, "rs_usedcapa=\"\";\n");
	}
	else{
		websWrite(wp, "rs_rulenum=\"%s\";\n", rulenum);
		websWrite(wp, "rs_status=\"%s\";\n", status);
		websWrite(wp, "rs_obj=\"%s\";\n", target_obj);
		if(!strcmp(status,"SYNC"))
		   strncpy(error_msg,"Sync has been completed",PATH_MAX);
		else if(!strcmp(status,"INITIAL"))
		   strncpy(error_msg,"Verifying",PATH_MAX);		
		websWrite(wp, "rs_msg=\"%s\";\n", error_msg);
		websWrite(wp, "rs_fullcapa=\"%s\";\n", full_capa);
		websWrite(wp, "rs_usedcapa=\"%s\";\n", used_capa);
	}

	return 0;
}

int ej_webdavInfo(int eid, webs_t wp, int argc, char **argv) {
	websWrite(wp, "// pktInfo=['PrinterInfo','SSID','NetMask','ProductID','FWVersion','OPMode','MACAddr','Regulation'];\n");
	websWrite(wp, "pktInfo=['','%s',", nvram_safe_get("wl0_ssid"));
	websWrite(wp, "'%s',", nvram_safe_get("lan_netmask"));
	websWrite(wp, "'%s',", nvram_safe_get("productid"));
	websWrite(wp, "'%s.%s',", nvram_safe_get("firmver"), nvram_safe_get("buildno"));
	websWrite(wp, "'%s',", nvram_safe_get("sw_mode"));
#if defined(RTCONFIG_RGMII_BRCM5301X) || defined(RTCONFIG_QCA)
	websWrite(wp, "'%s',", nvram_safe_get("lan_hwaddr"));
#else
	websWrite(wp, "'%s',", nvram_safe_get("et0macaddr"));
#endif
	websWrite(wp, "''];\n");

	websWrite(wp, "// webdavInfo=['Webdav','HTTPType','HTTPPort','DDNS','HostName','WAN0IPAddr','','xSetting','HTTPSPort'];\n");
	websWrite(wp, "webdavInfo=['%s',", nvram_safe_get("enable_webdav"));
	websWrite(wp, "'%s',", nvram_safe_get("st_webdav_mode"));
	websWrite(wp, "'%s',", nvram_safe_get("webdav_http_port"));
	websWrite(wp, "'%s',", nvram_safe_get("ddns_enable_x"));
	websWrite(wp, "'%s',", nvram_safe_get("ddns_hostname_x"));
	websWrite(wp, "'%s',", nvram_safe_get("wan0_ipaddr"));
	websWrite(wp, "'%s',", nvram_safe_get(""));
	websWrite(wp, "'%s',", nvram_safe_get("x_Setting"));
	websWrite(wp, "'%s',", nvram_safe_get("webdav_https_port"));
#ifdef RTCONFIG_WEBDAV
 	websWrite(wp, "'1'");
#else
	if(check_if_file_exist("/opt/etc/init.d/S50aicloud")) websWrite(wp, "'1'");
	else websWrite(wp, "'0'");
#endif
	websWrite(wp, "];\n");

	return 0;
}
//#endif
#endif

// 2010.09 James. {
int start_autodet(int eid, webs_t wp, int argc, char **argv) {
	nvram_set("autodet_state", "");
	notify_rc_after_period_wait("start_autodet", 0);
	return 0;
}
#ifdef RTCONFIG_QCA_PLC_UTILS
int start_plcdet(int eid, webs_t wp, int argc, char **argv) {
	nvram_set("autodet_plc_state", "");
	notify_rc_after_period_wait("start_plcdet", 0);
	return 0;
}
#endif
#if defined(CONFIG_BCMWL5) || (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) || defined(RTCONFIG_QCA)
int start_wlcscan(int eid, webs_t wp, int argc, char **argv) {
	notify_rc("start_wlcscan");
	return 0;
}
#endif
int setting_lan(int eid, webs_t wp, int argc, char **argv){
	char lan_ipaddr_t[16];
	char lan_netmask_t[16];
	unsigned int lan_ip_num;
	unsigned int lan_mask_num;
	unsigned int lan_subnet;
	char wan_ipaddr_t[16];
	char wan_netmask_t[16];
	unsigned int wan_ip_num;
	unsigned int wan_mask_num;
	unsigned int wan_subnet;
	const unsigned int MAX_SUBNET = 3232300800U;
	const unsigned int MIN_LAN_IP = 3232235521U;
	struct in_addr addr;
	unsigned int new_lan_ip_num;
	unsigned int new_dhcp_start_num;
	unsigned int new_dhcp_end_num;
	char new_lan_ip_str[16];
	char new_dhcp_start_str[16];
	char new_dhcp_end_str[16];
	char tmp_lan[100], prefix_lan[] = "lanXXXXXXXXXX_";
	int unit;
	char tmp_wan[100], prefix_wan[] = "wanXXXXXXXXXX_";

	snprintf(prefix_lan, sizeof(prefix_lan), "lan_");

	memset(lan_ipaddr_t, 0, 16);
	strcpy(lan_ipaddr_t, nvram_safe_get(strcat_r(prefix_lan, "ipaddr", tmp_lan)));
	memset(&addr, 0, sizeof(addr));
	lan_ip_num = ntohl(inet_aton(lan_ipaddr_t, &addr));
	lan_ip_num = ntohl(addr.s_addr);
	memset(lan_netmask_t, 0, 16);
	strcpy(lan_netmask_t, nvram_safe_get(strcat_r(prefix_lan, "netmask", tmp_lan)));
	memset(&addr, 0, sizeof(addr));
	lan_mask_num = ntohl(inet_aton(lan_netmask_t, &addr));
	lan_mask_num = ntohl(addr.s_addr);
	lan_subnet = lan_ip_num&lan_mask_num;
_dprintf("http: get lan_subnet=%x!\n", lan_subnet);

	if ((unit = get_primaryif_dualwan_unit()) < 0) {
_dprintf("http: Can't get the WAN's unit!\n");
		websWrite(wp, "0");
		return 0;
	}

	if (!dualwan_unit__usbif(unit)) {
		wan_prefix(unit, prefix_wan);

		memset(wan_ipaddr_t, 0, 16);
		strcpy(wan_ipaddr_t, nvram_safe_get(strcat_r(prefix_lan, "ipaddr", tmp_wan)));
		memset(&addr, 0, sizeof(addr));
		wan_ip_num = ntohl(inet_aton(wan_ipaddr_t, &addr));
		wan_ip_num = ntohl(addr.s_addr);
		memset(wan_netmask_t, 0, 16);
		strcpy(wan_netmask_t, nvram_safe_get(strcat_r(prefix_lan, "netmask", tmp_wan)));
		memset(&addr, 0, sizeof(addr));
		wan_mask_num = ntohl(inet_aton(wan_netmask_t, &addr));
		wan_mask_num = ntohl(addr.s_addr);
		wan_subnet = wan_ip_num&wan_mask_num;
_dprintf("http: get wan_subnet=%x!\n", wan_subnet);

		if(lan_subnet != wan_subnet){
_dprintf("http: The subnets of WAN and LAN aren't the same already.!\n");
			websWrite(wp, "0");
			return 0;
		}
	}

	if(lan_subnet >= MAX_SUBNET)
		new_lan_ip_num = MIN_LAN_IP;
	else
		new_lan_ip_num = lan_ip_num+(~lan_mask_num)+1;

	new_dhcp_start_num = new_lan_ip_num+1;
	new_dhcp_end_num = new_lan_ip_num+(~inet_network(lan_netmask_t))-2;
_dprintf("%u, %u, %u.\n", new_lan_ip_num, new_dhcp_start_num, new_dhcp_end_num);
	memset(&addr, 0, sizeof(addr));
	addr.s_addr = htonl(new_lan_ip_num);
	memset(new_lan_ip_str, 0, 16);
	strcpy(new_lan_ip_str, inet_ntoa(addr));
	memset(&addr, 0, sizeof(addr));
	addr.s_addr = htonl(new_dhcp_start_num);
	memset(new_dhcp_start_str, 0, 16);
	strcpy(new_dhcp_start_str, inet_ntoa(addr));
	memset(&addr, 0, sizeof(addr));
	addr.s_addr = htonl(new_dhcp_end_num);
	memset(new_dhcp_end_str, 0, 16);
	strcpy(new_dhcp_end_str, inet_ntoa(addr));
_dprintf("%s, %s, %s.\n", new_lan_ip_str, new_dhcp_start_str, new_dhcp_end_str);

	nvram_set(strcat_r(prefix_lan, "ipaddr", tmp_lan), new_lan_ip_str);
	nvram_set(strcat_r(prefix_lan, "ipaddr_rt", tmp_lan), new_lan_ip_str); // Sync to lan_ipaddr_rt, added by jerry5.
	nvram_set("dhcp_start", new_dhcp_start_str);
	nvram_set("dhcp_end", new_dhcp_end_str);

	websWrite(wp, "1");

	nvram_commit();

	nvram_set("freeze_duck", "15");
	notify_rc("restart_net_and_phy");

	return 0;
}
// 2010.09 James. }

// qos svg support 2010.08 Viz vvvvvvvvvvvv
void asp_ctcount(webs_t wp, int argc, char_t **argv)
{
	static const char *states[10] = {
		"NONE", "ESTABLISHED", "SYN_SENT", "SYN_RECV", "FIN_WAIT",
		"TIME_WAIT", "CLOSE", "CLOSE_WAIT", "LAST_ACK", "LISTEN" };
	int count[13];	// tcp(10) + udp(2) + total(1) = 13 / max classes = 10
	FILE *f;
	char s[512];
	char *p;
	int i;
	int n;
	int mode;
	unsigned long rip;
	unsigned long lan;
	unsigned long mask;
	int ret=0;

	if (argc != 1) return;
	mode = atoi(argv[0]);

	memset(count, 0, sizeof(count));

	  if ((f = fopen("/proc/net/ip_conntrack", "r")) != NULL) {
		// ctvbuf(f);	// if possible, read in one go

		if (nvram_match("t_hidelr", "1")) {
			mask = inet_addr(nvram_safe_get("lan_netmask"));
			rip = inet_addr(nvram_safe_get("lan_ipaddr"));
			lan = rip & mask;
		}
		else {
			rip = lan = mask = 0;
		}

		while (fgets(s, sizeof(s), f)) {
			if (rip != 0) {
				// src=x.x.x.x dst=x.x.x.x	// DIR_ORIGINAL
				if ((p = strstr(s + 14, "src=")) == NULL) continue;
				if ((inet_addr(p + 4) & mask) == lan) {
					if ((p = strstr(p + 13, "dst=")) == NULL) continue;
					if (inet_addr(p + 4) == rip) continue;
				}
			}

			if (mode == 0) {
				// count connections per state
				if (strncmp(s, "tcp", 3) == 0) {
					for (i = 9; i >= 0; --i) {
						if (strstr(s, states[i]) != NULL) {
							count[i]++;
							break;
						}
					}
				}
				else if (strncmp(s, "udp", 3) == 0) {
					if (strstr(s, "[UNREPLIED]") != NULL) {
						count[10]++;
					}
					else if (strstr(s, "[ASSURED]") != NULL) {
						count[11]++;
					}
				}
				count[12]++;
			}
			else {
				// count connections per mark
				if ((p = strstr(s, " mark=")) != NULL) {
					n = atoi(p + 6) & 0xFF;
					if (n <= 10) count[n]++;
				}
			}
		}

		fclose(f);
	}

	if (mode == 0) {
		p = s;
		for (i = 0; i < 12; ++i) {
			p += sprintf(p, ",%d", count[i]);
		}
		ret += websWrite(wp, "\nconntrack = [%d%s];\n", count[12], s);
	}
	else {
		p = s;
		for (i = 1; i < 11; ++i) {
			p += sprintf(p, ",%d", count[i]);
		}
		ret += websWrite(wp, "\nnfmarks = [%d%s];\n", count[0], s);
	}
}

int ej_qos_packet(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *f;
	char s[256];
	unsigned long rates[10];
	unsigned long u;
	char *e;
	int n;
	char comma;
	char *a[1];
	int ret=0, unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char wan_ifname[16];

	unit = wan_primary_ifunit();
	wan_prefix(unit, prefix);
	memset(wan_ifname, 0, 16);
	strcpy(wan_ifname, strcat_r(prefix, "ifname", tmp));

	a[0] = "1";
	asp_ctcount(wp, 1, a);

	memset(rates, 0, sizeof(rates));
	sprintf(s, "tc -s class ls dev %s", nvram_safe_get(wan_ifname));
	if ((f = popen(s, "r")) != NULL) {
		n = 1;
		while (fgets(s, sizeof(s), f)) {
			if (strncmp(s, "class htb 1:", 12) == 0) {
				n = atoi(s + 12);
			}
			else if (strncmp(s, " rate ", 6) == 0) {
				if ((n % 10) == 0) {
					n /= 10;
					if ((n >= 1) && (n <= 10)) {
						u = strtoul(s + 6, &e, 10);
						if (*e == 'K') u *= 1000;
							else if (*e == 'M') u *= 1000 * 1000;
						rates[n - 1] = u;
						n = 1;
					}
				}
			}
		}
		pclose(f);
	}

	comma = ' ';
	ret += websWrite(wp, "\nqrates = [0,");
	for (n = 0; n < 10; ++n) {
		ret += websWrite(wp, "%c%lu", comma, rates[n]);
		comma = ',';
	}
	ret += websWrite(wp, "];");
	return 0;
}

int ej_ctdump(int eid, webs_t wp, int argc, char **argv)
{
	FILE *f;
	char s[512];
	char *p, *q;
	int mark;
	int findmark;
	unsigned int proto;
	unsigned int time;
	char src[16];
	char dst[16];
	char sport[16];
	char dport[16];
	unsigned long rip;
	unsigned long lan;
	unsigned long mask;
	char comma;
	int ret=0;

	if (argc != 1) return 0;

	findmark = atoi(argv[0]);

	mask = inet_addr(nvram_safe_get("lan_netmask"));
	rip = inet_addr(nvram_safe_get("lan_ipaddr"));
	lan = rip & mask;
	if (nvram_match("t_hidelr", "0")) rip = 0;	// hide lan -> router?

	ret += websWrite(wp, "\nctdump = [");
	comma = ' ';
	if ((f = fopen("/proc/net/ip_conntrack", "r")) != NULL) {
		//ctvbuf(f);
		while (fgets(s, sizeof(s), f)) {
			if ((p = strstr(s, " mark=")) == NULL) continue;
			if ((mark = (atoi(p + 6) & 0xFF)) > 10) mark = 0;
			if ((findmark != -1) && (mark != findmark)) continue;

			if (sscanf(s, "%*s %u %u", &proto, &time) != 2) continue;

			if ((p = strstr(s + 14, "src=")) == NULL) continue;		// DIR_ORIGINAL
			if ((inet_addr(p + 4) & mask) != lan) {
				// make sure we're seeing int---ext if possible
				if ((p = strstr(p + 41, "src=")) == NULL) continue;	// DIR_REPLY
			}
			else if (rip != 0) {
				if ((q = strstr(p + 13, "dst=")) == NULL) continue;
//				cprintf("%lx=%lx\n", inet_addr(q + 4), rip);
				if (inet_addr(q + 4) == rip) continue;
			}

			if ((proto == 6) || (proto == 17)) {
				if (sscanf(p + 4, "%s dst=%s sport=%s dport=%s", src, dst, sport, dport) != 4) continue;
			}
			else {
				if (sscanf(p + 4, "%s dst=%s", src, dst) != 2) continue;
				sport[0] = 0;
				dport[0] = 0;
			}
			ret += websWrite(wp, "%c[%u,%u,'%s','%s','%s','%s',%d]", comma, proto, time, src, dst, sport, dport, mark);
			comma = ',';
		}
	}
	ret += websWrite(wp, "];\n");
	return 0;
}

void ej_cgi_get(int eid, webs_t wp, int argc, char **argv)
{
	const char *v;
	int i;
	int ret;

	for (i = 0; i < argc; ++i) {
		v = get_cgi(argv[i]);
		if (v) ret += websWrite(wp, v);
	}
}


// traffic monitor
static int ej_netdev(int eid, webs_t wp, int argc, char_t **argv)
{
 FILE * fp;
  char buf[256];
  unsigned long rx, tx;
  unsigned long rx2, tx2;
#ifdef RTCONFIG_LACP
  unsigned long rx_lacp1, tx_lacp1;
  unsigned long rx2_lacp1, tx2_lacp1;
  unsigned long rx_lacp2, tx_lacp2;
  unsigned long rx2_lacp2, tx2_lacp2;
#endif
  unsigned long wl0_all_rx = 0, wl0_all_tx = 0;
  unsigned long wl1_all_rx = 0, wl1_all_tx = 0;
  char *p;
  char *ifname;
  char ifname_desc[12], ifname_desc2[12];
#ifdef RTCONFIG_LACP
  char ifname_desc2_lacp1[12];
  char ifname_desc2_lacp2[12];
#endif
  char comma;
  int wl0_valid = 0, wl1_valid = 0;
  int ret=0;
  char *name = NULL;
  int from_app = 0;
 #ifdef RTCONFIG_QTN  //RT-AC87U
	qcsapi_unsigned_int l_counter_value;
#endif

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		//_dprintf("name = NULL\n");
	}else if(!strncmp(name, "appobj", 6))
		from_app = 1;

  if(from_app == 0)
     ret += websWrite(wp, "\nnetdev = {\n");

  if ((fp = fopen("/proc/net/dev", "r")) != NULL) {
		fgets(buf, sizeof(buf), fp);
		fgets(buf, sizeof(buf), fp);
		comma = ' ';
			while (fgets(buf, sizeof(buf), fp)) {
				if ((p = strchr(buf, ':')) == NULL) continue;
				*p = 0;
				if ((ifname = strrchr(buf, ' ')) == NULL) ifname = buf;
			   		else ++ifname;
	  	   		if (sscanf(p + 1, "%lu%*u%*u%*u%*u%*u%*u%*u%lu", &rx, &tx) != 2) continue;

#ifdef RTCONFIG_BCM5301X_TRAFFIC_MONITOR
				/* WAN1, WAN2, LAN */
				if(strncmp(ifname, "vlan", 4)==0){
					traffic_wanlan(ifname, &rx, &tx);
#ifdef RTCONFIG_LACP
					if(nvram_get_int("lacp_enabled") == 1 &&
							strcmp(ifname, "vlan1") == 0){
						//traffic_trunk(1, &rx_lacp1, &tx_lacp1);
						netdev_calc("lacp1", "LACP1",
								&rx_lacp1, &tx_lacp1,
								ifname_desc2_lacp1,
								&rx2_lacp1, &tx2_lacp1);

						//traffic_trunk(2, &rx_lacp2, &tx_lacp2);
						netdev_calc("lacp2", "LACP2",
								&rx_lacp2, &tx_lacp2,
								ifname_desc2_lacp2,
								&rx2_lacp2, &tx2_lacp2);
					}
#endif
#ifdef RTCONFIG_QTN  //RT-AC87U
					if (nvram_contains_word("lan_ifnames", ifname)){
						if (rpc_qtn_ready()) {
							qcsapi_interface_get_counter("eth1_1", qcsapi_total_bytes_received,
																		&l_counter_value);
							rx += l_counter_value;
							qcsapi_interface_get_counter("eth1_1", qcsapi_total_bytes_sent,
																		&l_counter_value);
							tx += l_counter_value;
						}
					}
#endif
				}
				if(nvram_match("wans_dualwan", "wan none")){
					if(strcmp(ifname, "eth0")==0){
						traffic_wanlan(WAN0DEV, &rx, &tx);
					}
				}
#endif	/* RTCONFIG_BCM5301X_TRAFFIC_MONITOR */
				if (!netdev_calc(ifname, ifname_desc, &rx, &tx, ifname_desc2, &rx2, &tx2)) continue;

loopagain:
				if (!strncmp(ifname_desc, "WIRELESS0", 9)) {
					wl0_valid = 1;
					wl0_all_rx += rx;
					wl0_all_tx += tx;
				} else if (!strncmp(ifname_desc, "WIRELESS1", 9)) {
					wl1_valid = 1;
					wl1_all_rx += rx;
					wl1_all_tx += tx;
				} else {
					if(from_app == 0){
						ret += websWrite(wp, "%c'%s':{rx:0x%lx,tx:0x%lx}\n", comma, ifname_desc, rx, tx);
					}else{
						ret += websWrite(wp, "%c\"%s_rx\":\"0x%lx\",\"%s_tx\":\"0x%lx\"", comma, ifname_desc, rx, ifname_desc, tx);
					}
						comma = ',';
				}
				if(strlen(ifname_desc2)) {
					strcpy(ifname_desc, ifname_desc2);
					rx = rx2;
					tx = tx2;
					strcpy(ifname_desc2, "");
					goto loopagain;
				}
			}
#ifdef RTCONFIG_QTN  //RT-AC87U
			if (rpc_qtn_ready()) {
				qcsapi_interface_get_counter(WIFINAME, qcsapi_total_bytes_received, &l_counter_value);
				wl1_all_rx = l_counter_value;
				qcsapi_interface_get_counter(WIFINAME, qcsapi_total_bytes_sent, &l_counter_value);
				wl1_all_tx = l_counter_value;
				wl1_valid = 1;
			}
#endif		
			

			if (wl0_valid) {
				if(from_app == 0){
					ret += websWrite(wp, "%c'%s':{rx:0x%lx,tx:0x%lx}\n", comma, "WIRELESS0", wl0_all_rx, wl0_all_tx);
				}else{
					ret += websWrite(wp, "%c\"%s_rx\":\"0x%lx\",\"%s_tx\":\"0x%lx\"", comma, "WIRELESS0", wl0_all_rx, "WIRELESS0", wl0_all_tx);
				}
				comma = ',';
			}
			if (wl1_valid) {
				if(from_app == 0){
					ret += websWrite(wp, "%c'%s':{rx:0x%lx,tx:0x%lx}\n", comma, "WIRELESS1", wl1_all_rx, wl1_all_tx);
				}else{
					ret += websWrite(wp, "%c\"%s_rx\":\"0x%lx\",\"%s_tx\":\"0x%lx\"", comma, "WIRELESS1", wl1_all_rx, "WIRELESS1",wl1_all_tx);
				}
				comma = ',';
			}

#ifdef RTCONFIG_LACP
	if(nvram_get_int("lacp_enabled") == 1){
		if(from_app == 0){
			ret += websWrite(wp, "%c'%s':{rx:0x%lx,tx:0x%lx}\n", comma, "LACP1", rx_lacp1, tx_lacp1);
			ret += websWrite(wp, "%c'%s':{rx:0x%lx,tx:0x%lx}\n", comma, "LACP2", rx_lacp2, tx_lacp2);
		}
	}
#endif

		fclose(fp);
			if(from_app == 0)
				ret += websWrite(wp, "}");
		
  }
  	return 0;
  }

int ej_bandwidth(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name;
	int sig;

	if (strcmp(argv[0], "speed") == 0) {
		sig = SIGUSR1;
		name = "/var/spool/rstats-speed.js";
	}
	else {
		sig = SIGUSR2;
		name = "/var/spool/rstats-history.js";
	}
	unlink(name);
	killall("rstats", sig);
//	eval("killall", sig, "rstats");
	f_wait_exists(name, 5);
	do_f(name, wp);
	unlink(name);
	return 0;
}

//Ren.B
#ifdef RTCONFIG_DSL

// 2014.02 Viz {
int start_dsl_autodet(int eid, webs_t wp, int argc, char **argv) {
	notify_rc("start_dsl_autodet");
	return 0;
}
// }

int ej_spectrum(int eid, webs_t wp, int argc, char_t **argv)
{
	int sig;

	if(nvram_match("spectrum_hook_is_running", "1"))
	{
		//on running status, skip.
		return 0;
	}

	sig = SIGUSR1;
	system("/usr/sbin/check_spectrum.sh"); //check if spectrum is running.
	sleep(1);

	killall("spectrum", sig);

	return 0;

}

int ej_getAnnexMode(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *logFile = fopen( "/tmp/adsl/adsllog.log", "r" );
	char buf[256] = {0};
	char *ptr = NULL;

	if( !logFile )
	{
		printf("Error: adsllog.log does not exist.\n");
		websWrite(wp, "Error: adsllog.log does not exist.");
		return -1;
	}
	while( fgets(buf, sizeof(buf), logFile) )
	{
		if( (ptr=strstr(buf, "Annex Mode :")) != NULL )
		{
			ptr += strlen("Annex Mode :")+1;
			if(strstr(ptr, "Annex A"))
			{
				websWrite(wp, "Annex A" );
			}
			else if(strstr(ptr, "Annex B"))
			{
				websWrite(wp, "Annex B" );
			}
			else
			{
				websWrite(wp, "Error : Unknown Annex Mode" );
			}
			break;
		}
	}

	fclose(logFile);
	return 0;
}

int ej_getADSLToneAmount(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *logFile = fopen( "/tmp/adsl/adsllog.log", "r" );
	char buf[256] = {0};
	char *ptr = NULL;
	int mode = 5;
	int tones = 512;

	if( !logFile )
	{
		printf("Error: adsllog.log does not exist.\n");
		websWrite(wp, "%d", tones);
		return -1;
	}
	while( fgets(buf, sizeof(buf), logFile) )
	{
		if( (ptr=strstr(buf, "Modulation :")) != NULL )
		{
			ptr += strlen("Annex Mode :")+1;
			mode = atoi(ptr);
			break;
		}
	}

	switch(mode)
	{
		case 0:
		case 1:
		case 2:
			tones = 256;
			break;
		case 3:
		case 4:
			tones = 512;
			break;
		default:
			tones = 512;
	}

	fclose(logFile);
	websWrite(wp, "%d", tones);
	return 0;
}

int show_file_content(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name;
	char buffer[256];
	FILE *fp = NULL;
	int ret = 0;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	fp = fopen(name, "r");
	if(!fp)
	{
		ret += websWrite(wp, "");
		return -2;
	}
	while (fgets(buffer, sizeof(buffer), fp))
	{
		ret += websWrite(wp, "%s", buffer);
	}

	fclose(fp);
	return ret;
}
#endif
//Ren.E

int ej_backup_nvram(int eid, webs_t wp, int argc, char_t **argv)
{
	char *list;
	char *p, *k;
	const char *v;

	if ((argc != 1) || ((list = strdup(argv[0])) == NULL)) return 0;
	websWrite(wp, "\nnvram = {\n");
	p = list;
	while ((k = strsep(&p, ",")) != NULL) {
		if (*k == 0) continue;
		v = nvram_get(k);
		if (!v) {
			v = "";
		}
		websWrite(wp, "\t%s: '", k);
		websWrite(wp, v);
//		web_puts((p == NULL) ? "'\n" : "',\n");
		websWrite(wp, "',\n");
	}
	free(list);
	websWrite(wp, "\thttp_id: '");
	websWrite(wp, nvram_safe_get("http_id"));
	websWrite(wp, "'};\n");
//	web_puts("};\n");
	return 0;
}
// end svg support by Viz ^^^^^^^^^^^^^^^^^^^^

static int
ej_select_list(int eid, webs_t wp, int argc, char_t **argv)
{
	char *id;
	int ret = 0;
	char out[64], idxstr[12], tmpstr[64], tmpstr1[64];
	int i, curr, hit, noneFlag;
	char *ref1, *ref2, *refnum;

	if (ejArgs(argc, argv, "%s", &id) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (strcmp(id, "Storage_x_SharedPath")==0)
	{
		ref1 = "sh_path_x";
		ref2 = "sh_path";
		refnum = "sh_num";
		curr = nvram_get_int(ref1);
		sprintf(idxstr, "%d", curr);
		strcpy(tmpstr1, nvram_get(strcat_r(ref2, idxstr, tmpstr)));
		sprintf(out, "%s", tmpstr1);
		ret += websWrite(wp, out);
		return ret;
	}
	else if (strncmp(id, "Storage_x_AccUser", 17)==0)
	{
		sprintf(tmpstr, "sh_accuser_x%s", id + 17);
		ref2 = "acc_username";
		refnum = "acc_num";

		curr = nvram_get_int(tmpstr);
		noneFlag =1;
	}
	else if (strcmp(id, "Storage_x_AccAny")==0)
	{
		 ret = websWrite(wp, "<option value=\"Guest\">Guest</option>");
		 return ret;
	}
	else if (strcmp(id, "Storage_AllUser_List")==0)
	{

		strcpy(out, "<option value=\"Guest\">Guest</option>");
		ret += websWrite(wp, out);

		for (i=0;i<nvram_get_int("acc_num");i++)
		{
			 sprintf(idxstr, "%d", i);
			 strcpy(tmpstr1, nvram_get(strcat_r("acc_username", idxstr, tmpstr)));
			 sprintf(out, "<option value=\"%s\">%s</option>", tmpstr1, tmpstr1);
			 ret += websWrite(wp, out);
		}
		return ret;
	}
	else
	{
		 return ret;
	}

	hit = 0;

	for (i=0;i<nvram_get_int(refnum);i++)
	{
		sprintf(idxstr, "%d", i);
		strcpy(tmpstr1, nvram_get(strcat_r(ref2, idxstr, tmpstr)));
		sprintf(out, "<option value=\"%d\"", i);

		if (i==curr)
		{
			hit = 1;
			sprintf(out, "%s selected", out);
		}
		sprintf(out,"%s>%s</option>", out, tmpstr1);

		ret += websWrite(wp, out);
	}

	if (noneFlag)
	{
		cprintf("hit : %d\n", hit);
		if (!hit) sprintf(out, "<option value=\"99\" selected>None</option>");
		else sprintf(out, "<option value=\"99\">None</option>");

		ret += websWrite(wp, out);
	}
	return ret;
}

static int
ej_radio_status(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;

	retval += websWrite(wp, "radio_2=%d;\nradio_5=%d;", get_radio(0,0), get_radio(1,0));
	return retval;
}

static int
ej_memory_usage(int eid, webs_t wp, int argc, char_t **argv){
	unsigned long total, used, mfree/*, shared, buffers, cached*/;
	char buf[80];
	char *name = NULL;
	int from_app = 0;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		//_dprintf("name = NULL\n");
	}else if(!strncmp(name, "appobj", 6))
		from_app = 1;

	FILE *fp; 
	
	fp = fopen("/proc/meminfo", "r");
	
	if(fp == NULL)
		return -1;
		
	fscanf(fp, "MemTotal: %lu %s\n", &total, buf);	
	fscanf(fp, "MemFree: %lu %s\n", &mfree, buf);	
	used = total - mfree;
	fclose(fp);
	if(from_app == 0){
		websWrite(wp, "<mem_info>\n");	
		websWrite(wp, "<total>%lu</total>\n", total);	
		websWrite(wp, "<free>%lu</free>\n", mfree);	
		websWrite(wp, "<used>%lu</used>\n", used);	
		websWrite(wp, "</mem_info>\n");	
	}else{
		websWrite(wp, "\"mem_total\":\"%lu\",\"mem_free\":\"%lu\",\"mem_used\":\"%lu\"", total, mfree, used);	
	}
	return 0;
}

static int
ej_cpu_usage(int eid, webs_t wp, int argc, char_t **argv){
	unsigned long total, user, nice, system, idle, io, irq, softirq;
	char name[10];	
	char *name_t = NULL;
	int from_app = 0;

	if (ejArgs(argc, argv, "%s", &name_t) < 1) {
//		_dprintf("name_t = NULL\n");
	}else if(!strncmp(name_t, "appobj", 6))
		from_app = 1;

	FILE *fp; 
	fp = fopen("/proc/stat", "r");
	int i = 0, firstRow=1;

	if(fp == NULL)
		return -1;
	if(from_app == 0){
		websWrite(wp, "<cpu_info>\n");	
	}
	while(fscanf(fp, "%s %lu %lu %lu %lu %lu %lu %lu \n", name, &user, &nice, &system, &idle, &io, &irq, &softirq) != EOF){
		if(strncmp(name, "cpu", 3) == 0){
			if(i == 0){
				i++;
				continue;
			}
			
			total = user + nice + system + idle + io + irq + softirq;
			if(from_app == 0){		
				websWrite(wp, "<cpu>\n");	
				websWrite(wp, "<total>%lu</total>\n", total);	
				websWrite(wp, "<usage>%lu</usage>\n", total - idle);	
				websWrite(wp, "</cpu>\n");	
			}else{
				if (firstRow == 1)
					firstRow = 0;
				else
					websWrite(wp, ",");

				websWrite(wp, "\"cpu%d_total\":\"%lu\",\"cpu%d_usage\":\"%lu\"", i, total, i, total - idle);
			}
			i++;
		}	
	}
	
	fclose(fp);
	if(from_app == 0)
		websWrite(wp, "</cpu_info>\n");	
	return 0;
}

static int
ej_cpu_core_num(int eid, webs_t wp, int argc, char_t **argv){
	char buf[MAX_LINE_SIZE];
	FILE *fp;
	int count = 0;
	fp = fopen("/proc/cpuinfo", "r");

	if(fp == NULL) return -1;

	while(fgets(buf, MAX_LINE_SIZE, fp)!=NULL){
		if(strncmp(buf, "processor", 9) == 0){
			count++;
		}
	}

	fclose(fp);
	if(count == 0){		//for Braomcom ARM single core
		count = 1;
	}

	websWrite(wp, "%d", count);
	return 0;

}

static int
ej_check_acpw(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	char result[32];

	strcpy(result, "0");

	if(!strcmp(nvram_default_get("http_username"), nvram_safe_get("http_username")) && !strcmp(nvram_default_get("http_passwd"), nvram_safe_get("http_passwd")))
		strcpy(result, "1");

	retval += websWrite(wp, result);

	return retval;
}

static int
ej_check_acorpw(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	char result[32];

	strcpy(result, "0");

	if(!strcmp(nvram_default_get("http_username"), nvram_safe_get("http_username")) || !strcmp(nvram_default_get("http_passwd"), nvram_safe_get("http_passwd")))
		strcpy(result, "1");

	retval += websWrite(wp, result);

	return retval;
}

#ifdef RTCONFIG_BWDPI
static int
ej_bwdpi_history(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	char *hwaddr;
	char *page;

	// get real-time traffic of someone.
	hwaddr = websGetVar(wp, "client", "");

	// get which page for listing
	page = websGetVar(wp, "page", "");
	
	//_dprintf("[httpd] history: hwaddr=%s, page=%s.\n", hwaddr, page);
	get_web_hook(hwaddr, page, &retval, wp);

	return retval;
}


/*
	get_bwdpi_hook(type, mode, name, dura, date)

	mode : traffic / traffic_wan / app / client_apps / client_web
	name : NULL / appname / clientname
	dura : realtime / month / week / day
	date : NULL / day
*/

static int
ej_bwdpi_status(int eid, webs_t wp, int argc, char_t **argv)
{
	char *mode, *name, *dura, *date;
	int retval = 0;

	if (ejArgs(argc, argv, "%s %s %s %s", &mode, &name, &dura, &date) < 4) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	// get real-time traffic of someone.
	name = websGetVar(wp, "client", "");
	
	//_dprintf("[httpd] bwdpi: mode=%s, name=%s, dura=%s, date=%s.\n", mode, name, dura, date);
	get_traffic_hook(mode, name, dura, date, &retval, wp);

	return retval;
}

static int
ej_bwdpi_device(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	char *hwaddr;

	// get device info of someone.
	hwaddr = websGetVar(wp, "client", "");
	
	//_dprintf("[httpd] history: hwaddr=%s.\n", hwaddr);
	get_device_hook(hwaddr, &retval, wp);

	return retval;
}

static int
ej_bwdpi_redirect_page_status(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	char *cat_id;	
	int catid;

	// get device info of someone.
	cat_id = websGetVar(wp, "cat_id", "");
	catid = atoi(cat_id);
	redirect_page_status(catid, &retval, wp);

	return retval;
}

static int
ej_bwdpi_appStat(int eid, webs_t wp, int argc, char_t **argv)
{
	char *client, *mode, *dura, *date;
	int retval = 0;
	
	client = websGetVar(wp, "client", "");
	mode = websGetVar(wp, "mode", "");
	dura = websGetVar(wp, "dura", "");
	date = websGetVar(wp, "date", "");

	// 0: app, 1: mac
	sqlite_Stat_hook(0, client, mode, dura, date, &retval, wp);
	
	return retval;
}

static int
ej_bwdpi_wanStat(int eid, webs_t wp, int argc, char_t **argv)
{
	char *client, *mode, *dura, *date;
	int retval = 0;
	
	client = websGetVar(wp, "client", "");
	mode = websGetVar(wp, "mode", "");
	dura = websGetVar(wp, "dura", "");
	date = websGetVar(wp, "date", "");

	// 0: app, 1: mac
	sqlite_Stat_hook(1, client, mode, dura, date, &retval, wp);
	
	return retval;
}

static int
ej_bwdpi_engine_status(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	
	retval += websWrite(wp, "{");
	retval += websWrite(wp, "\"DpiEngine\":%d", check_bwdpi_nvram_setting());
	retval += websWrite(wp, "}");

	return retval;
}
#else
static int
ej_bwdpi_engine_status(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	
	retval += websWrite(wp, "{");
	retval += websWrite(wp, "}");

	return retval;
}
#endif

#ifdef RTCONFIG_TRAFFIC_CONTROL
static int
ej_traffic_control_wanStat(int eid, webs_t wp, int argc, char_t **argv)
{
	char *ifname, *start, *end, *unit;
	int retval = 0;
	
	ifname = websGetVar(wp, "ifname", "");
	start = websGetVar(wp, "start", "");
	end = websGetVar(wp, "end", "");
	unit = websGetVar(wp, "unit", "");

	traffic_control_hook(ifname, start, end, unit, &retval, wp);
	
	return retval;
}
#endif

static int
ej_wl_nband_info(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit = 0, ret = 0, firstRow = 1;
	char *band, word[256], *next;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	ret += websWrite(wp, "[");
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if (firstRow == 1)
			firstRow = 0;
		else
			ret += websWrite(wp, ", ");

		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		band = nvram_safe_get(strcat_r(prefix, "nband", tmp));

			ret += websWrite(wp, "'%s'", band);

		unit++;
	}
	ret += websWrite(wp, "]");
	return ret;
}

#ifdef RTCONFIG_GEOIP
static int
ej_geoiplookup(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char *wanip;

	unit = wan_primary_ifunit();
	wan_prefix(unit, prefix);
	wanip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));

	return websWrite(wp, "%s", geoiplookup_by_ip(wanip));
}
#endif

#ifdef RTCONFIG_JFFS2USERICON
static int 
ej_get_upload_icon(int eid, webs_t wp, int argc, char **argv) {
	char *client_mac = websGetVar(wp, "clientmac", "");
	char *client_mac_tmp = NULL;
	int from_app = 0;

	if (ejArgs(argc, argv, "%s", &client_mac_tmp) < 1) {
		//_dprintf("name = NULL\n");
	}else if(!strcmp(client_mac, "")){
		client_mac = client_mac_tmp;
		from_app = 1;
	}

	if(strcmp(client_mac, "")) {
		char file_name[32];
		memset(file_name, 0, 32);

		//Check folder exist or not
		if(!check_if_dir_exist(JFFS_USERICON))
			mkdir(JFFS_USERICON, 0755);

		//Write upload icon value
		sprintf(file_name, "/jffs/usericon/%s.log", client_mac);
		if(check_if_file_exist(file_name)) {
			dump_file(wp, file_name);
		}
		else {
			websWrite(wp, "NoIcon");
		}
	}
	else {
		websWrite(wp, "NoIcon");
	}
	return 0;
}
static int 
ej_get_upload_icon_count_list(int eid, webs_t wp, int argc, char **argv) {
	int file_count = 0;
	DIR * dirp;
	struct dirent * entry;
	char allMacList[1500];
	memset(allMacList, 0, 1500);
	char *name = NULL;
	int from_app = 0;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		//_dprintf("name = NULL\n");
	}else if(!strncmp(name, "appobj", 6))
		from_app = 1;

	//Check folder exist or not
	if(!check_if_dir_exist(JFFS_USERICON))
		mkdir(JFFS_USERICON, 0755);

	//Write /jffs/usericon/ file count and list
	dirp = opendir(JFFS_USERICON); /* There should be error handling after this */
	while ((entry = readdir(dirp)) != NULL) {
		if (entry->d_type == DT_REG) { /* If the entry is a regular file */
			strcat(allMacList, entry->d_name);
			strcat(allMacList, ">");
			file_count++;
		}
	}
	closedir(dirp);
	if(from_app == 0){
		websWrite(wp, "upload_icon_count=\"%d\";\n", file_count);
		websWrite(wp, "upload_icon_list=\"%s\";\n", allMacList);
	}else{
		websWrite(wp, "\"upload_icon_count\":\"%d\",\n", file_count);
		websWrite(wp, "\"upload_icon_list\":\"%s\"\n", allMacList);
	}

	return 0;
}
#endif

static int 
ej_findasus(int eid, webs_t wp, int argc, char **argv) {
	char *buf, *g, *p;
	char strTmp[4096]={0}, retList[65536]={0};
	char *type, *name, *ip, *mac, *netmask, *tmp1, *tmp2, *ssid, *submask;
	int isfirst=0;

	eval("asusdiscovery");	//find asus device

	g = buf = strdup(nvram_safe_get("asus_device_list"));
	strcat(retList, "[\n");
   	if(strcmp(buf, "") != 0){
		while (buf) {
			if ((p = strsep(&g, "<")) == NULL) break;
	
			if((vstrsep(p, ">",&type ,&name, &ip, &mac, &netmask, &tmp1, &tmp2, &ssid, &submask)) != 9) continue;

			if(isfirst == 0){
				isfirst = 1;
			}else{
				strcat(retList, ",\n");
			}

			strcat(retList, "{");
			sprintf(strTmp, "modelName:\"%s\",", name);
			strcat(retList, strTmp);
			sprintf(strTmp, "ssid:\"%s\",", ssid);
			strcat(retList, strTmp);
			sprintf(strTmp, "ipAddr:\"%s\"", ip);
			strcat(retList, strTmp);
			strcat(retList, "}");
		}
	}else{
		sprintf(strTmp, "{modelName:\"%s\",ssid:\"%s\",ipAddr:\"%s\"}", nvram_safe_get("productid"), nvram_safe_get("wl_ssid"), nvram_safe_get("lan_ipaddr"));
		strcat(retList, strTmp);
	}
	free(buf);
	strcat(retList, "\n]");
	return websWrite(wp, "%s", retList);
}

#ifdef RTCONFIG_WTFAST
static int
ej_wtfast_status(int eid, webs_t wp, int argc, char **argv) {
	char wtfast_status[4096] = {0};
	char tmp[1024] = {0};
	char val[512] = {0};

	strcat(wtfast_status, "{");

	sprintf(tmp, "\"eMail\":\"%s\",", nvram_get("wtf_username"));
	strcat(wtfast_status, tmp);

	sprintf(tmp, "\"Account_Type\":\"%s\",", nvram_get("wtf_account_type"));
	strcat(wtfast_status, tmp);

	sprintf(tmp, "\"Max_Computers\": %d,", nvram_get_int("wtf_max_clients"));
	strcat(wtfast_status, tmp);

	memset(val, 0, 512);
	sprintf(tmp, "\"Server_List\":[],");
	strcat(wtfast_status, tmp);

	sprintf(tmp, "\"Days_Left\": %d,", 0);
	strcat(wtfast_status, tmp);

	sprintf(tmp, "\"Login_status\": %d,", nvram_get_int("wtf_login"));
	strcat(wtfast_status, tmp);

	sprintf(tmp, "\"Session_Hash\": \"\",");
	strcat(wtfast_status, tmp);

	strcat(wtfast_status, "}");

	return websWrite(wp, "%s", wtfast_status);
}
#endif

static int
ej_check_ftp_samba_anonymous(int eid, webs_t wp, int argc, char **argv){

	char *name = NULL;
	int samba_mode=0, ftp_mode=0, ret=0;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		//_dprintf("name = NULL\n");
		ret = websWrite(wp, "Not support");
		return ret;
	}

	if(!strcmp(name,"cifs")){
		if((nvram_get("st_samba_force_mode") == NULL && nvram_get_int("st_samba_mode") == 1)){
			samba_mode = 4;
		}else{
			samba_mode = nvram_get_int("st_samba_mode");
		}
		if(samba_mode == 2 || samba_mode == 4){
			ret = websWrite(wp, "1");
		}else{
			ret = websWrite(wp, "0");
		}
	}else if(!strcmp(name,"ftp")){
		if((nvram_get("st_ftp_force_mode") == NULL && nvram_get_int("st_ftp_mode") == 1)){
			ftp_mode = 2;
		}else{
			ftp_mode = nvram_get_int("st_ftp_mode");
		}
		if(ftp_mode == 2){
			ret = websWrite(wp, "1");
		}else{
			ret = websWrite(wp, "0");
		}
	}
	return ret;
}

char* reverse_str( char *str )
{
  int i, n;
  char c;

  n = strlen( str );
  for( i=0; i<n/2; i++ )
  {
    c = str[i];
    str[i] = str[n-i-1];
    str[n-i-1] = c;
  }

  return str;
}

static int
ej_check_passwd_strength(int eid, webs_t wp, int argc, char **argv){

	int ret=0;
	char *name = NULL;
	if (ejArgs(argc, argv, "%s", &name) < 1) {
		ret = websWrite(wp, "Not support");
		//_dprintf("name = NULL\n");
		return ret;		
	}

	int unit = 0;
	int nScore_total=0, nScore=0;
	char *pwd = NULL, word[256]={0}, *next = NULL;
	char tmp[128]={0}, prefix[] = "wlXXXXXXXXXX_";
	int nLength=0, nConsecAlphaUC=0, nConsecCharType=0, nAlphaUC=0, nConsecAlphaLC=0, nAlphaLC=0, nMidChar=0, nConsecNumber=0, nNumber=0, nConsecSymbol=0, nSymbol=0, nRepChar=0, nUnqChar=0, nSeqAlpha=0, nSeqNumber=0, nSeqChar = 0, nSeqSymbol=0;
	int nMultMidChar=2, nMultLength=4, nMultNumber=4, nMultSymbol=6, nMultConsecAlphaUC=2, nMultConsecAlphaLC=2, nMultConsecNumber=2, nMultSeqAlpha=3, nMultSeqNumber=3, nMultSeqSymbol=3;
	int a=0, b=0, s=0, x=0;
	int nTmpAlphaUC = -1, nTmpAlphaLC = -1, nTmpNumber = -1, nTmpSymbol = -1;
	double nRepInc = 0.0;
	char sAlphas[] = "abcdefghijklmnopqrstuvwxyz";
	char sNumerics[] = "01234567890";
	char sSymbols[] = "~!@#$%^&*()_+";
	char pwd_s[128] = {0};
	char *pwd_st=NULL, *arrPwd=NULL;
	char *auth_mode=NULL;
	char sFwd[4], sFwd_t[4], sRev[4];
	if(!strcmp(name,"wl_key")){
		foreach (word, nvram_safe_get("wl_ifnames"), next) {
			snprintf(prefix, sizeof(prefix), "wl%d_", unit);
			pwd = nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp));
			auth_mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
			nLength=0; nConsecAlphaUC=0; nConsecCharType=0; nAlphaUC=0; nConsecAlphaLC=0; nAlphaLC=0; nMidChar=0; nConsecNumber=0; nNumber=0; nConsecSymbol=0; nSymbol=0; nRepChar=0; nUnqChar=0; nSeqAlpha=0; nSeqNumber=0; nSeqChar = 0; nSeqSymbol=0;
			nTmpAlphaUC = -1; nTmpAlphaLC = -1; nTmpNumber = -1; nTmpSymbol = -1;
		   if(pwd != NULL && (!strcmp(auth_mode,"psk2") || !strcmp(auth_mode,"pskpsk2") || !strcmp(auth_mode,"wpa2") || !strcmp(auth_mode,"wpawpa2"))){
			nScore=0;
			nLength=0;
			pwd_st = pwd;
			arrPwd = pwd;
			nLength = strlen(pwd);
			nScore = nLength * nMultLength;

			/* Main calculation for strength: 
					Loop through password to check for Symbol, Numeric, Lowercase and Uppercase pattern matches */
			for (a=0; a <nLength; a++){
				if(isupper(arrPwd[a])){
					if(nTmpAlphaUC != -1){
						if((nTmpAlphaUC + 1) == a){
							nConsecAlphaUC++;
							nConsecCharType++;
						}
					}
					nTmpAlphaUC = a;
					nAlphaUC++;
				}
				else if(islower(arrPwd[a])){
					if(nTmpAlphaLC != -1){
						if((nTmpAlphaLC + 1) == a){
							nConsecAlphaLC++;
							nConsecCharType++;
						}
					}
					nTmpAlphaLC = a;
					nAlphaLC++;
				}
				else if(isdigit(arrPwd[a])){
					if(a > 0 && a < (nLength - 1)){
						nMidChar++;
					}
					if(nTmpNumber != -1){
						if((nTmpNumber + 1) == a){
							nConsecNumber++;
							nConsecCharType++;
						}
					}
					nTmpNumber = a;
					nNumber++;
				}
				else if(!isalnum(arrPwd[a])){
					if(a > 0 && a < (nLength - 1))
					{
						nMidChar++;
					}
					if(nTmpSymbol != -1){
						if((nTmpSymbol + 1) == a){
							nConsecSymbol++;
							nConsecCharType++;
						}
					}
					nTmpSymbol = a;
					nSymbol++;
				}

				/* Internal loop through password to check for repeat characters */
				int bCharExists = 0;
				for (b=0; b < nLength; b++){
					if (arrPwd[a] == arrPwd[b] && a != b){ /* repeat character exists */
						bCharExists = 1;
						/* 
						Calculate icrement deduction based on proximity to identical characters
						Deduction is incremented each time a new match is discovered
						Deduction amount is based on total password length divided by the
						difference of distance between currently selected match
						*/
						nRepInc += abs(nLength/(b-a));
					}
				}
				if (bCharExists == 1) { 
					nRepChar++; 
					nUnqChar = nLength-nRepChar;
					nRepInc = (nUnqChar > 0) ? ceil(nRepInc/(double)nUnqChar) : ceil(nRepInc); 
				}
			}

			for(x = 0; x < nLength; x++){
				pwd_s[x]= tolower(*pwd_st); 
				pwd_st++;
			}

			/* Check for sequential alpha string patterns (forward and reverse) */
			for (s=0; s < 23; s++){
				memset(sFwd, 0, sizeof(sFwd));
				memset(sFwd_t, 0, sizeof(sFwd_t));
				memset(sRev, 0, sizeof(sRev));
				if(sAlphas+s != '\0'){
					strncpy(sFwd, sAlphas+s, 3);
					strncpy(sFwd_t, sFwd, 3);
				}
				strcpy(sRev, reverse_str(sFwd));
		
				if(strstr(pwd_s, sFwd_t) != NULL || strstr(pwd_s, sRev) != NULL){
					nSeqAlpha++;
					nSeqChar++;
				}
			}
			/* Check for sequential numeric string patterns (forward and reverse) */
			for (s=0; s < 8; s++) {
				memset(sFwd, 0, sizeof(sFwd));
				memset(sFwd_t, 0, sizeof(sFwd_t));
				memset(sRev, 0, sizeof(sRev));
				if(sNumerics+s != '\0'){
					strncpy(sFwd, sNumerics+s, 3);
					strncpy(sFwd_t, sFwd, 3);
				}
				strcpy(sRev, reverse_str(sFwd));
				if(strstr(pwd_s, sFwd_t) != NULL || strstr(pwd_s, sRev) != NULL){
					nSeqNumber++;
					nSeqChar++;
				}
			}
			/* Check for sequential symbol string patterns (forward and reverse) */
			for (s=0; s < 8; s++) {
				memset(sFwd, 0, sizeof(sFwd));
				memset(sFwd_t, 0, sizeof(sFwd_t));
				memset(sRev, 0, sizeof(sRev));
				if(sSymbols+s != '\0'){
					strncpy(sFwd, sSymbols+s, 3);
					strncpy(sFwd_t, sFwd, 3);
				}
				strcpy(sRev, reverse_str(sFwd));
				if(strstr(pwd_s, sFwd_t) != NULL || strstr(pwd_s, sRev) != NULL){
					nSeqSymbol++;
					nSeqChar++;
				}
			}
			/* Modify overall score value based on usage vs requirements */
		
			/* General point assignment */
			if (nAlphaUC > 0 && nAlphaUC < nLength){	
				nScore = nScore + ((nLength - nAlphaUC) * 2);
			}
			if (nAlphaLC > 0 && nAlphaLC < nLength){	
				nScore = nScore + ((nLength - nAlphaLC) * 2); 
			}
			if (nNumber > 0 && nNumber < nLength){	
				nScore = nScore + (nNumber * nMultNumber);
			}
			if (nSymbol > 0){	
				nScore = nScore + (nSymbol * nMultSymbol);
			}
			if (nMidChar > 0){	
				nScore = nScore + (nMidChar * nMultMidChar);
			}		
			/* Point deductions for poor practices */
			if ((nAlphaLC > 0 || nAlphaUC > 0) && nSymbol == 0 && nNumber == 0) {  // Only Letters
				nScore = nScore - nLength;
			}
			if (nAlphaLC == 0 && nAlphaUC == 0 && nSymbol == 0 && nNumber > 0) {  // Only Numbers
				nScore = nScore - nLength; 
			}
			if (nRepChar > 0) {  // Same character exists more than once
				nScore = nScore - nRepInc;
			}
			if (nConsecAlphaUC > 0) {  // Consecutive Uppercase Letters exist
			nScore = nScore - (nConsecAlphaUC * nMultConsecAlphaUC); 
			}
			if (nConsecAlphaLC > 0) {  // Consecutive Lowercase Letters exist
				nScore = nScore - (nConsecAlphaLC * nMultConsecAlphaLC); 
			}
			if (nConsecNumber > 0) {  // Consecutive Numbers exist
				nScore = nScore - (nConsecNumber * nMultConsecNumber);  
			}
			if (nSeqAlpha > 0) {  // Sequential alpha strings exist (3 characters or more)
				nScore = nScore - (nSeqAlpha * nMultSeqAlpha); 
			}
			if (nSeqNumber > 0) {  // Sequential numeric strings exist (3 characters or more)
				nScore = nScore - (nSeqNumber * nMultSeqNumber); 
			}
			if (nSeqSymbol > 0) {  // Sequential symbol strings exist (3 characters or more)
				nScore = nScore - (nSeqSymbol * nMultSeqSymbol); 
			}

			/* Determine complexity based on overall score */
			if (nScore > 100)
			{
				nScore = 100;
			}else if(nScore < 0)
			{
				nScore = 0;
			}
			nScore_total = nScore + nScore_total;
		   }else{
			nScore = 0;
		   }
		   unit++;
		}	
	}
	websWrite(wp, "%d", (int)(nScore_total/unit));
	return ret;
}

static int
ej_check_wireless_encryption(int eid, webs_t wp, int argc, char **argv){

	int ret=0, unit=0;
	char *auth_mode=NULL;
	char word[256]={0}, *next = NULL;
	char tmp[128]={0}, prefix[] = "wlXXXXXXXXXX_";

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		auth_mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
		if(!strcmp(auth_mode,"psk2") || !strcmp(auth_mode,"pskpsk2") || !strcmp(auth_mode,"wpa2") || !strcmp(auth_mode,"wpawpa2")){
			ret = 1;					
		}else{
			return websWrite(wp, "0");
		}
		unit++;
	}
	return websWrite(wp, "1");
}

static int
check_macrepeat(char *maclist,char *mac){
	int total=0;
	while ( strstr(maclist,mac) != NULL ) 
   	{
		maclist += strlen(maclist);
		total++;
	}
	return total;
}

static int
ej_get_clientlist(int eid, webs_t wp, int argc, char **argv){

	int i, shm_client_info_id;
	void *shared_client_info=(void *) 0;
	char output_buf[2048];
	char maclist_buf[4096]=",\"maclist\":";
	char mac_buf[32];
	char *brackets_h = "[";
	char *brackets_d = "]";
	char *dot = ",";
	char ipaddr[16];
	P_CLIENT_DETAIL_INFO_TABLE p_client_info_tab;
	int lock;
	char devname[LINE_SIZE], character;
	int j, len;
	int first_mac=1, first_info=1;

	lock = file_lock("networkmap");
	shm_client_info_id = shmget((key_t)1001, sizeof(CLIENT_DETAIL_INFO_TABLE), 0666|IPC_CREAT);
	if (shm_client_info_id == -1){
	    fprintf(stderr,"shmget failed\n");
	    file_unlock(lock);
	    return 0;
	}

	shared_client_info = shmat(shm_client_info_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr,"shmat failed\n");
		file_unlock(lock);
		return 0;
	}

	p_client_info_tab = (P_CLIENT_DETAIL_INFO_TABLE)shared_client_info;
	for(i=0; i<p_client_info_tab->ip_mac_num; i++) {
		memset(output_buf, 0, 2048);
		memset(ipaddr, 0, 16);
		memset(mac_buf, 0, 32);
		memset(devname, 0, LINE_SIZE);

	    if(p_client_info_tab->exist[i]==1) {
		len = strlen( (char *) p_client_info_tab->device_name[i]);
		for (j=0; (j < len) && (j < LINE_SIZE-1); j++) {
			character = p_client_info_tab->device_name[i][j];
			if ((isalnum(character)) || (character == ' ') || (character == '-') || (character == '_'))
				devname[j] = character;
			else
				devname[j] = ' ';
		}
		sprintf(ipaddr, "%d.%d.%d.%d", p_client_info_tab->ip_addr[i][0],p_client_info_tab->ip_addr[i][1],
		p_client_info_tab->ip_addr[i][2],p_client_info_tab->ip_addr[i][3]);

		sprintf(mac_buf, "\"%02X:%02X:%02X:%02X:%02X:%02X\"",
		p_client_info_tab->mac_addr[i][0],p_client_info_tab->mac_addr[i][1],
		p_client_info_tab->mac_addr[i][2],p_client_info_tab->mac_addr[i][3],
		p_client_info_tab->mac_addr[i][4],p_client_info_tab->mac_addr[i][5]
		);
		if(first_mac == 1){
			first_mac = 0;
			strcat(maclist_buf,brackets_h);
		}else{
			strcat(maclist_buf,dot);
		}
		strcat(maclist_buf,mac_buf);

		if(first_info == 1){
			first_info = 0;			
		}else{
			websWrite(wp, ",\n");
		}

		sprintf(output_buf, "%s:{\"type\":\"%d\",\"name\":\"%s\",\"ip\":\"%s\",\"mac\":%s,\"from\":\"networkmapd\",\"macRepeat\":\"%d\",\"isGateway\":\"%s\",\"isWebServer\":\"%d\",\"isPrinter\":\"%d\",\"isITunes\":\"%d\",\"isOnline\":\"true\"}",
		mac_buf,
		p_client_info_tab->type[i],
		devname,
		ipaddr,
		mac_buf,
		check_macrepeat(maclist_buf, mac_buf),
		!strcmp(nvram_safe_get("lan_ipaddr"), ipaddr) ? "true" : "false",
		p_client_info_tab->http[i],
		p_client_info_tab->printer[i],
		p_client_info_tab->itune[i]
		);
		websWrite(wp, output_buf);

	    }
	}
	shmdt(shared_client_info);
	file_unlock(lock);
	strcat(maclist_buf,brackets_d);
	websWrite(wp, maclist_buf);
	return 0;
}

static int
ej_check_asus_model(int eid, webs_t wp, int argc, char **argv)
{
#ifdef RTCONFIG_HTTPS
	if (check_model_name())
		return websWrite(wp, "1");
	else
		return websWrite(wp, "0");
#else
	return websWrite(wp, "1");
#endif
}

struct ej_handler ej_handlers[] = {
	{ "nvram_get", ej_nvram_get},
	{ "nvram_default_get", ej_nvram_default_get},
	{ "nvram_match", ej_nvram_match},
	{ "nvram_double_match", ej_nvram_double_match},
	{ "nvram_show_chinese_char", ej_nvram_show_chinese_char}, // 2012.05 Jieming add to show chinese char in mediaserver.asp
	// the follwoing will be removed
	{ "nvram_get_x", ej_nvram_get_x},
	{ "nvram_get_f", ej_nvram_get_f},
	{ "nvram_get_list_x", ej_nvram_get_list_x},
	{ "nvram_get_buf_x", ej_nvram_get_buf_x},
	{ "nvram_match_x", ej_nvram_match_x},
	{ "nvram_double_match_x", ej_nvram_double_match_x},
	{ "nvram_match_both_x", ej_nvram_match_both_x},
	{ "nvram_match_list_x", ej_nvram_match_list_x},
	{ "select_channel", ej_select_channel},
	{ "uptime", ej_uptime},
	{ "sysuptime", ej_sysuptime},
	{ "nvram_dump", ej_dump},
	{ "load_script", ej_load},
	{ "select_list", ej_select_list},
	{ "dhcpLeaseInfo", ej_dhcpLeaseInfo},
	{ "dhcpLeaseMacList", ej_dhcpLeaseMacList},
	{ "IP_dhcpLeaseInfo", ej_IP_dhcpLeaseInfo},
//tomato qosvvvvvvvvvvv 2010.08 Viz
	{ "qrate", ej_qos_packet},
	{ "ctdump", ej_ctdump},
	{ "netdev", ej_netdev},

	{ "iptraffic", ej_iptraffic},
	{ "iptmon", ej_iptmon},
	{ "ipt_bandwidth", ej_ipt_bandwidth},

	{ "bandwidth", ej_bandwidth},
#ifdef RTCONFIG_DSL
	{ "spectrum", ej_spectrum}, //Ren
	{ "get_annexmode", ej_getAnnexMode}, //Ren
	{ "get_adsltoneamount", ej_getADSLToneAmount}, //Ren
	{ "show_file_content", show_file_content}, //Ren
#endif
	{ "backup_nvram", ej_backup_nvram},
//tomato qos^^^^^^^^^^^^ end Viz
	{ "wl_get_parameter", ej_wl_get_parameter},
	{ "wl_get_guestnetwork", ej_wl_get_guestnetwork},
	{ "wan_get_parameter", ej_wan_get_parameter},
	{ "lan_get_parameter", ej_lan_get_parameter},
#ifdef RTCONFIG_DSL
	{ "dsl_get_parameter", ej_dsl_get_parameter},
#endif

#ifdef ASUS_DDNS //2007.03.27 Yau add
	{ "nvram_get_ddns", ej_nvram_get_ddns},
	{ "nvram_char_to_ascii", ej_nvram_char_to_ascii},
#endif
//2008.08 magic{
	{ "update_variables", ej_update_variables},
#ifdef RTCONFIG_ROG
	{ "set_variables", ej_set_variables},
#endif
	{ "convert_asus_variables", convert_asus_variables},
	{ "asus_nvram_commit", asus_nvram_commit},
	{ "notify_services", ej_notify_services},
	{ "wanstate", wanstate_hook},
	{ "dual_wanstate", dual_wanstate_hook},
	{ "ajax_dualwanstate", ajax_dualwanstate_hook},	
	{ "ajax_wanstate", ajax_wanstate_hook},
	{ "secondary_ajax_wanstate", secondary_ajax_wanstate_hook},
#ifdef RTCONFIG_DSL
	{ "wanlink_dsl", wanlink_hook_dsl},
#endif
	{ "wanlink", wanlink_hook},
	{ "first_wanlink", first_wanlink_hook},
	{ "secondary_wanlink", secondary_wanlink_hook},
	{ "wanlink_state", wanlink_state_hook},
	{ "wan_action", wan_action_hook},
	{ "get_wan_unit", get_wan_unit_hook},
	{ "check_hwnat", check_hwnat},
	{ "get_parameter", ej_get_parameter},
	{ "get_ascii_parameter", ej_get_ascii_parameter},
	{ "login_state_hook", login_state_hook},
#ifdef RTCONFIG_FANCTRL
	{ "get_fanctrl_info", get_fanctrl_info},
#endif
#ifdef RTCONFIG_BCMARM
	{ "get_cpu_temperature", get_cpu_temperature},
#endif
	{ "get_machine_name" , get_machine_name},
	{ "dhcp_leases", ej_dhcp_leases},
	{ "get_arp_table", ej_get_arp_table},
	{ "get_client_detail_info", ej_get_client_detail_info},//2011.03 Yau add for new networkmap
	{ "get_static_client", ej_get_static_client},
	{ "yadns_servers", yadns_servers_hook},
	{ "yadns_clients", yadns_clients_hook},
	{ "get_changed_status", ej_get_changed_status},
	{ "shown_language_css", ej_shown_language_css},
	{ "memory_usage", ej_memory_usage},
	{ "cpu_usage", ej_cpu_usage},
	{ "cpu_core_num", ej_cpu_core_num},
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
#ifdef RTCONFIG_WIRELESSWAN
	{ "sitesurvey", ej_SiteSurvey},
#endif
#endif
#if defined(CONFIG_BCMWL5) || (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) || defined(RTCONFIG_QCA)
	{ "get_ap_info", ej_get_ap_info},
#endif
	{ "ddns_info", ej_ddnsinfo},
	{ "show_usb_path", ej_show_usb_path},
	{ "apps_fsck_ret", ej_apps_fsck_ret},
#ifdef RTCONFIG_USB
	{ "disk_pool_mapping_info", ej_disk_pool_mapping_info},
	{ "available_disk_names_and_sizes", ej_available_disk_names_and_sizes},
	{ "get_printer_info", ej_get_printer_info},
	{ "get_modem_info", ej_get_modem_info},
	{ "get_isp_scan_results", ej_get_isp_scan_results},
	{ "get_simact_result", ej_get_simact_result},
	{ "get_modemuptime", ej_modemuptime},
	{ "get_AiDisk_status", ej_get_AiDisk_status},
	{ "set_AiDisk_status", ej_set_AiDisk_status},
	{ "get_all_accounts", ej_get_all_accounts},
	{ "safely_remove_disk", ej_safely_remove_disk},
	{ "get_permissions_of_account", ej_get_permissions_of_account},
	{ "set_account_permission", ej_set_account_permission},
	{ "set_account_all_folder_permission", ej_set_account_all_folder_permission},
	{ "get_folder_tree", ej_get_folder_tree},
	{ "get_share_tree", ej_get_share_tree},
	{ "initial_account", ej_initial_account},
	{ "create_account", ej_create_account},	/*no ccc*/
	{ "delete_account", ej_delete_account}, /*n*/
	{ "modify_account", ej_modify_account}, /*n*/
	{ "create_sharedfolder", ej_create_sharedfolder},	/*y*/
	{ "delete_sharedfolder", ej_delete_sharedfolder},	/*y*/
	{ "modify_sharedfolder", ej_modify_sharedfolder},	/* no ccc*/
	{ "set_share_mode", ej_set_share_mode},
	{ "initial_folder_var_file", ej_initial_folder_var_file},
#ifdef RTCONFIG_DISK_MONITOR
	{ "apps_fsck_log", ej_apps_fsck_log},
#endif
	{ "apps_info", ej_apps_info},
	{ "apps_state_info", ej_apps_state_info},
	{ "apps_action", ej_apps_action},
#ifdef RTCONFIG_MEDIA_SERVER
	{ "dms_info", ej_dms_info},
#endif
//#ifdef RTCONFIG_CLOUDSYNC
	{ "cloud_status", ej_cloud_status},
	{ "UI_cloud_status", ej_UI_cloud_status},
	{ "UI_cloud_dropbox_status", ej_UI_cloud_dropbox_status},
	{ "UI_cloud_ftpclient_status", ej_UI_cloud_ftpclient_status},
	{ "UI_cloud_sambaclient_status", ej_UI_cloud_sambaclient_status},
	{ "UI_rs_status", ej_UI_rs_status},
	{ "getWebdavInfo", ej_webdavInfo},
//#endif
#endif
	{ "start_autodet", start_autodet},
#ifdef RTCONFIG_QCA_PLC_UTILS
	{ "start_plcdet", start_plcdet},
#endif	
#if defined(CONFIG_BCMWL5) || (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) || defined(RTCONFIG_QCA)
	{ "start_wlcscan", start_wlcscan},
#endif
	{ "setting_lan", setting_lan},

	// system or solution dependant part start from here
	{ "wl_sta_list_2g", ej_wl_sta_list_2g},
	{ "wl_sta_list_5g", ej_wl_sta_list_5g},
#ifdef CONFIG_BCMWL5
#ifndef RTCONFIG_QTN
	{ "wl_sta_list_5g_2", ej_wl_sta_list_5g_2},
#endif
#endif
#ifdef RTCONFIG_STAINFO
	{ "wl_stainfo_list_2g", ej_wl_stainfo_list_2g},
	{ "wl_stainfo_list_5g", ej_wl_stainfo_list_5g},
#ifndef RTCONFIG_QTN
	{ "wl_stainfo_list_5g_2", ej_wl_stainfo_list_5g_2},
#endif
#endif
	{ "wl_auth_list", ej_wl_auth_list},
#ifdef CONFIG_BCMWL5
	{ "wl_control_channel", ej_wl_control_channel},
	{ "wl_extent_channel", ej_wl_extent_channel},
#endif
#ifdef RTCONFIG_DSL
	{ "start_dsl_autodet", start_dsl_autodet},
	{ "get_isp_list", ej_get_isp_list},
	{ "get_DSL_WAN_list", ej_get_DSL_WAN_list},
#endif
	{ "wl_scan_2g", ej_wl_scan_2g},
	{ "wl_scan_5g", ej_wl_scan_5g},
#ifdef CONFIG_BCMWL5
	{ "wl_scan_5g_2", ej_wl_scan_5g_2},
#endif
	{ "channel_list_2g", ej_wl_channel_list_2g},
	{ "channel_list_5g", ej_wl_channel_list_5g},
#ifdef RTCONFIG_QTN
	{ "channel_list_5g_20m", ej_wl_channel_list_5g_20m},
	{ "channel_list_5g_40m", ej_wl_channel_list_5g_40m},
	{ "channel_list_5g_80m", ej_wl_channel_list_5g_80m},
#else
	{ "channel_list_5g_20m", ej_wl_channel_list_5g},
	{ "channel_list_5g_40m", ej_wl_channel_list_5g},
	{ "channel_list_5g_80m", ej_wl_channel_list_5g},
#endif
#ifdef CONFIG_BCMWL5
	{ "channel_list_5g_2", ej_wl_channel_list_5g_2},
	{ "chanspecs_2g", ej_wl_chanspecs_2g},
	{ "chanspecs_5g", ej_wl_chanspecs_5g},
	{ "chanspecs_5g_2", ej_wl_chanspecs_5g_2},
#endif
#if defined(CONFIG_BCMWL5) || (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) || defined(RTCONFIG_QCA)
	{ "wl_rate_2g", ej_wl_rate_2g},
	{ "wl_rate_5g", ej_wl_rate_5g},
#ifdef CONFIG_BCMWL5
	{ "wl_rate_5g_2", ej_wl_rate_5g_2},
#endif
#endif
#ifdef RTCONFIG_PROXYSTA
	{ "wlc_psta_state", ej_wl_auth_psta},
#endif
	{ "get_default_reboot_time", ej_get_default_reboot_time},
	{ "sysinfo",  ej_show_sysinfo},
#ifdef RTCONFIG_IPV6
#ifdef RTCONFIG_IGD2
	{ "ipv6_pinholes",  ej_ipv6_pinhole_array},
#endif
	{ "get_ipv6net_array", ej_lan_ipv6_network_array},
#endif
	{ "get_leases_array", ej_get_leases_array},
	{ "get_vserver_array", ej_get_vserver_array},
	{ "get_upnp_array", ej_get_upnp_array},
	{ "get_route_array", ej_get_route_array},
#ifdef RTCONFIG_BCMWL6
	{ "get_wl_status", ej_wl_status_2g_array},
#endif
#ifdef RTCONFIG_OPENVPN
	{ "vpn_server_get_parameter", ej_vpn_server_get_parameter},
	{ "vpn_client_get_parameter", ej_vpn_client_get_parameter},
	{ "vpn_crt_server", ej_vpn_crt_server},
	{ "vpn_crt_client", ej_vpn_crt_client},
#endif
	{ "nvram_clean_get", ej_nvram_clean_get},
	{ "radio_status", ej_radio_status},
	{ "check_acpw", ej_check_acpw},
	{ "check_acorpw", ej_check_acorpw},
	{ "check_ftp_samba_anonymous", ej_check_ftp_samba_anonymous},
	{ "check_passwd_strength", ej_check_passwd_strength},
	{ "check_wireless_encryption", ej_check_wireless_encryption},
	{ "get_clientlist", ej_get_clientlist},
#ifdef RTCONFIG_BWDPI
	{ "bwdpi_status", ej_bwdpi_status},
	{ "bwdpi_history", ej_bwdpi_history},
	{ "bwdpi_device_info", ej_bwdpi_device},
	{ "bwdpi_redirect_info", ej_bwdpi_redirect_page_status},
	{ "bwdpi_appStat", ej_bwdpi_appStat},
	{ "bwdpi_wanStat", ej_bwdpi_wanStat},
	{ "bwdpi_engine_status", ej_bwdpi_engine_status},
#else
	{ "bwdpi_engine_status", ej_bwdpi_engine_status},
#endif
#ifdef RTCONFIG_TRAFFIC_CONTROL
	{ "traffic_control_wanStat", ej_traffic_control_wanStat},
#endif
	{ "wl_nband_info", ej_wl_nband_info},
#ifdef RTCONFIG_GEOIP
	{ "geoiplookup", ej_geoiplookup},
#endif
#ifdef RTCONFIG_JFFS2USERICON
	{ "get_upload_icon", ej_get_upload_icon},
	{ "get_upload_icon_count_list", ej_get_upload_icon_count_list},
#endif
	{ "findasus", ej_findasus},
#ifdef RTCONFIG_WTFAST	
	{ "wtfast_status", ej_wtfast_status },
#endif	
	{ "check_asus_model", ej_check_asus_model},
	{ NULL, NULL }
};

void websSetVer(void)
{
	FILE *fp;
	unsigned long *imagelen;
	char dataPtr[4];
	char verPtr[64];
	char productid[13];
	char fwver[12];

	strcpy(productid, "WLHDD");
	strcpy(fwver, "0.1.0.1");

	if ((fp = fopen("/dev/mtd3", "rb"))!=NULL)
	{
		if (fseek(fp, 4, SEEK_SET)!=0) goto write_ver;
		fread(dataPtr, 1, 4, fp);
		imagelen = (unsigned long *)dataPtr;

		cprintf("image len %x\n", *imagelen);
		if (fseek(fp, *imagelen - 64, SEEK_SET)!=0) goto write_ver;
		cprintf("seek\n");
		if (!fread(verPtr, 1, 64, fp)) goto write_ver;
		cprintf("ver %x %x", verPtr[0], verPtr[1]);
		strncpy(productid, verPtr + 4, 12);
		productid[12] = 0;
		sprintf(fwver, "%d.%d.%d.%d", verPtr[0], verPtr[1], verPtr[2], verPtr[3]);

		cprintf("get fw ver: %s\n", productid);
		fclose(fp);
	}
write_ver:
	nvram_set_f("general.log", "productid", productid);
	nvram_set_f("general.log", "firmver", fwver);
}

#if 0
int
get_nat_vserver_table(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	char *nat_argv[] = {"iptables", "-t", "nat", "-nxL", NULL};
	char line[256], tmp[256];
	char target[16], proto[16];
	char src[sizeof("255.255.255.255")];
	char dst[sizeof("255.255.255.255")];
	char *range, *host, *port, *ptr, *val;
	int ret = 0;
	char chain[16];

	/* dump nat table including VSERVER and VUPNP chains */
	_eval(nat_argv, ">/tmp/vserver.log", 10, NULL);

	ret += websWrite(wp,
#ifdef NATSRC_SUPPORT
		"Source          "
#endif
		"Destination     Proto. Port range  Redirect to     Local port  Chain\n");
	/*	 255.255.255.255 other  65535:65535 255.255.255.255 65535:65535 VUPNP*/

	fp = fopen("/tmp/vserver.log", "r");
	if (fp == NULL)
		return ret;

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		_dprintf("HTTPD: %s\n", line);

		// If it's a chain definition then store it for following rules
		if (!strncmp(line, "Chain",  5)){
			if (sscanf(line, "%*s%*[ \t]%15s%*[ \t]%*s", chain) == 1)
				continue;
		}
		tmp[0] = '\0';
		if (sscanf(line,
		    "%15s%*[ \t]"		// target
		    "%15s%*[ \t]"		// prot
		    "%*s%*[ \t]"		// opt
		    "%15[^/]/%*d%*[ \t]"	// source
		    "%15[^/]/%*d%*[ \t]"	// destination
		    "%255[^\n]",		// options
		    target, proto, src, dst, tmp) < 4) continue;

		/* TODO: add port trigger, portmap, etc support */
		if (strcmp(target, "DNAT") != 0)
			continue;

		/* Don't list DNS redirections  from DNSFilter */
		if (strcmp(chain, "DNSFILTER") ==0)
			continue;

		/* uppercase proto */
		for (ptr = proto; *ptr; ptr++)
			*ptr = toupper(*ptr);
#ifdef NATSRC_SUPPORT
		/* parse source */
		if (strcmp(src, "0.0.0.0") == 0)
			strcpy(src, "ALL");
#endif
		/* parse destination */
		if (strcmp(dst, "0.0.0.0") == 0)
			strcpy(dst, "ALL");

		/* parse options */
		port = host = range = "";
		ptr = tmp;
		while ((val = strsep(&ptr, " ")) != NULL) {
			if (strncmp(val, "dpt:", 4) == 0)
				range = val + 4;
			else if (strncmp(val, "dpts:", 5) == 0)
				range = val + 5;
			else if (strncmp(val, "to:", 3) == 0) {
				port = host = val + 3;
				strsep(&port, ":");
			}
		}

		ret += websWrite(wp,
#ifdef NATSRC_SUPPORT
			"%-15s "
#endif
			"%-15s %-6s %-11s %-15s %-11s %-15s\n",
#ifdef NATSRC_SUPPORT
			src,
#endif
			dst, proto, range, host, port ? : range, chain);
	}
	fclose(fp);
	unlink("/tmp/vserver.log");

	return ret;
}
#endif

/* remove space in the end of string */
char *trim_r(char *str)
{
	int i;

	i = strlen(str);

	while (i >= 1)
	{
		if (*(str+i-1) == ' ' || *(str+i-1) == 0x0a || *(str+i-1) == 0x0d)
			*(str+i-1)=0x0;
		else
			break;

		i--;
	}

	return (str);
}

int
ej_get_default_reboot_time(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;

	retval += websWrite(wp, nvram_safe_get("reboot_time"));
	return retval;
}

int is_wlif_up(const char *ifname)
{
	struct ifreq ifr;
	int skfd;

	if (!ifname)
		return -1;

	skfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (skfd == -1)
	{
		perror("socket");
		return -1;
	}

	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
	{
		perror("ioctl");
		close(skfd);
		return -1;
	}
	close(skfd);

	if (ifr.ifr_flags & IFF_UP)
		return 1;
	else
		return 0;
}


// Replace CRLF with ">", as the nvram backup encoder can't handle
// ASCII values below 32.
void write_encoded_crt(char *name, char *value){
	int len, i, j;
	char tmp[3500];

	if (!value) return;

	len = strlen(value);
	// Safeguard against buffer overrun
	if (len > (sizeof(tmp) - 1)) len = sizeof(tmp) - 1;

	i = 0;
	for (j=0; (j < len); j++) {
		if (value[j] == '\n') tmp[i++] = '>';
		else if (value[j] != '\r') tmp[i++] = value[j];
	}

	tmp[i] = '\0';
	nvram_set(name, tmp);
}
