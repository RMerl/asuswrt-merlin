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

#include <net/if.h>
#include <linux/sockios.h>
#include <semaphore_mfp.h>
#include <networkmap.h> //2011.03 Yau add for new networkmap
#include <sys/ipc.h>
#include <sys/shm.h>

#include "sysinfo.h"

#ifdef RTCONFIG_HTTPS
extern int do_ssl;
extern int ssl_stream_fd;
#endif

extern int ej_wl_auth_list(int eid, webs_t wp, int argc, char_t **argv);
#ifdef CONFIG_BCMWL5
extern int ej_wl_control_channel(int eid, webs_t wp, int argc, char_t **argv);

#endif

#ifndef RTCONFIG_RALINK
extern int ej_SiteSurvey(int eid, webs_t wp, int argc, char_t **argv);
#endif

extern int ej_wl_scan_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_scan_5g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_channel_list_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_channel_list_5g(int eid, webs_t wp, int argc, char_t **argv);
#ifdef RTCONFIG_PROXYSTA
int ej_wl_auth_psta(int eid, webs_t wp, int argc, char_t **argv);
#endif

extern int ej_get_default_reboot_time(int eid, webs_t wp, int argc, char_t **argv);

#define wan_prefix(unit, prefix)	snprintf(prefix, sizeof(prefix), "wan%d_", unit)
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
#include <image.h>
#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif

#include <syslog.h>
#define logs(s) syslog(LOG_NOTICE, s)

/* #define sys_upgrade(image) eval("mtd-write", "-i", image, "-d", "linux"); */
#define sys_upload(image) eval("nvram", "restore", image)
#define sys_download(file) eval("nvram", "save", file)
#define sys_default() notify_rc("resetdefault"); //   eval("mtd-erase", "-d", "nvram")
#define sys_reboot() notify_rc("reboot");


#define PROFILE_HEADER 	"HDR1"
#ifdef RTCONFIG_DSL
#define PROFILE_HEADER_NEW      "N55U"
#else
#define PROFILE_HEADER_NEW      "HDR2"
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
char SystemCmd[64];
char UserID[32]="";
char UserPass[32]="";
char ProductID[32]="";
extern int redirect;
extern int change_passwd;	// 2008.08 magic
extern int reget_passwd;	// 2008.08 magic
extern char *host_name;

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
	//printf("Redirect to : %s\n", url);	
	websWrite(wp, T("<html><head>\r\n"));
#ifdef RTCONFIG_HTTPS
	if(do_ssl) 
		websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=https://%s/%s\">\r\n"), gethost(), url);
	else
#endif
	websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=http://%s/%s\">\r\n"), gethost(), url);
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
		sprintf(SystemCmd, "%s > /tmp/syscmd.log 2>&1\n", SystemCmd);	// oleg patch
	   	system(SystemCmd);
		strcpy(SystemCmd,""); // Ensure we don't re-execute it again
	   }
	   else
	   {
	   	system("echo None > /tmp/syscmd.log\n");
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
		doSystem("killall -%d udhcpd", SIGUSR1);
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
	char *sid, *name, *c;
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

	for (c = nvram_default_get(name); *c; c++) {
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

/* Dump leases in <tr><td>MAC</td><td>IP</td><td>expires</td></tr> format */
static int
ej_dumpleases(int eid, webs_t wp, char *lease_file)
{
	FILE *fp;
	struct lease_t lease;
	int i;
	struct in_addr addr;
	unsigned long expires;
	int ret = 0;

	ret +=websWrite(wp,"Mac Address       IP Address      Lease Time\n");	
	
	/* Parse leases file */
	if (!(fp = fopen(lease_file, "r"))) 
	{
		//websWrite(wp, "No leases\n");
		return -1;
	}

	while (fread(&lease, sizeof(lease), 1, fp)) {
		//ret += websWrite(wp, "%s", format);
		for (i = 0; i < 6; i++) {
			ret += websWrite(wp, "%02x", lease.chaddr[i]);
			if (i != 5) ret += websWrite(wp, ":");
		}
		addr.s_addr = lease.yiaddr;
		ret += websWrite(wp, " %s", inet_ntoa(addr));
		ret += websWriteCh(wp," ", 16-strlen(inet_ntoa(addr)));
		expires = ntohl(lease.expires);
		
		if (expires==0xffffffff)	
			ret += websWrite(wp, "manual");
		else if (!expires)
			ret += websWrite(wp, "expired");
		else {
			if (expires > 60*60*24) {
				ret += websWrite(wp, "%ld days, ", expires / (60*60*24));
				expires %= 60*60*24;
			}
			if (expires > 60*60) {
				ret += websWrite(wp, "%ld hours, ", expires / (60*60));
				expires %= 60*60;
			}
			if (expires > 60) {
				ret += websWrite(wp, "%ld minutes, ", expires / 60);
				expires %= 60;
			}
			ret += websWrite(wp, "%ld seconds", expires);
		}
		ret += websWrite(wp, "\n");
	}
	fclose(fp);

	return ret;
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
	    ret += websWrite(wp, buf);
	}
 
	fclose(fp);		
	
	return (ret);
}

static int
ej_dump(int eid, webs_t wp, int argc, char_t **argv)
{	
//	FILE *fp;
//	char buf[MAX_LINE_SIZE];
	char filename[32];
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
	else if (strcmp(file, "leases.log")==0) 
		return (ej_lan_leases(eid, wp, 0, NULL));
	else if (strcmp(file, "iptable.log")==0) 
		return (get_nat_vserver_table(eid, wp, 0, NULL));
	else if (strcmp(file, "route.log")==0)
		return (ej_route_table(eid, wp, 0, NULL));
	else if (strcmp(file, "wps_info.log")==0)
	{
		if (nvram_match("wps_band", "0"))
			return (ej_wps_info_2g(eid, wp, 0, NULL));
		else
			return (ej_wps_info(eid, wp, 0, NULL));
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

	if (strcmp(file, "syslog.log")==0)
	{
		sprintf(filename, "/tmp/%s-1", file);
		ret+=dump_file(wp, filename); 
		sprintf(filename, "/tmp/%s", file);
		ret+=dump_file(wp, filename);
	}
#ifdef RTCONFIG_CLOUDSYNC
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
#endif
	else {
		sprintf(filename, "/tmp/%s", file);
		ret+=dump_file(wp, filename);
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
		    *c != '"' && *c != '&' && *c != '<' && *c != '>' && *c != '\\')
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

static int
ej_vpn_server_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit;

	unit = nvram_get_int("vpn_server_unit");
	// handle generate cases first
	(void)copy_index_to_unindex("vpn_server_", unit, -1);

	return (websWrite(wp,""));
}

static int
ej_vpn_client_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
        int unit;

        unit = nvram_get_int("vpn_client_unit");
        // handle generate cases first
        (void)copy_index_to_unindex("vpn_client_", unit, -1);

        return (websWrite(wp,""));
}



//2008.08 magic {
static char post_buf[30000] = { 0 };
static char post_buf_backup[30000] = { 0 };

static void do_html_post_and_get(char *url, FILE *stream, int len, char *boundary){
	char *query = NULL;
	
	init_cgi(NULL);
	
	memset(post_buf, 0, 30000);
	memset(post_buf_backup, 0, 30000);
	
	if (fgets(post_buf, MIN(len+1, sizeof(post_buf)), stream)){
		len -= strlen(post_buf);
		
		while (len--)
			(void)fgetc(stream);
	}
	
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

static char *error_msg_console = NULL, *error_msg = NULL;

static char *get_msg_from_dict(char *lang, const char *const msg_name){
#define MAX_FILE_LENGTH 512
	char current_dir[MAX_FILE_LENGTH];
	char dict_file[MAX_FILE_LENGTH], *dict_info;
	char *follow_info, *follow_info_end, *target;
	int len;
	
	if (lang == NULL || strlen(lang) == 0)
		lang = "EN";
	
	memset(dict_file, 0, MAX_FILE_LENGTH);
	memset(current_dir, 0, MAX_FILE_LENGTH);
	getcwd(current_dir, MAX_FILE_LENGTH);
	sprintf(dict_file, "%s/%s.dict", current_dir, lang);
	
	dict_info = read_whole_file(dict_file);
	if (dict_info == NULL){
		csprintf("No dictionary file, \"%s\".\n", dict_file);
		return NULL;
	}
	
	follow_info = strstr(dict_info, msg_name);
	if (follow_info == NULL){
		csprintf("No \"%s\" in the dictionary file.\n", msg_name);
		free(dict_info);
		return NULL;
	}
	
	follow_info += strlen(msg_name)+strlen("=");
	follow_info_end = follow_info;
	while (*follow_info_end != 0 && *follow_info_end != '\n')
		++follow_info_end;
	
	len = follow_info_end-follow_info;
	
	target = (char *)malloc(sizeof(char)*(len+1));
	if (target == NULL){
		csprintf("No memory \"target\".\n");
		free(dict_info);
		return NULL;
	}
	if (len > 0)
		strncpy(target, follow_info, len);
	target[len] = 0;
	
	free(dict_info);
	return target;
}

static void show_error_msg(const char *const msg_num){
	char msg_name[32];
	
	memset(msg_name, 0, 32);
	sprintf(msg_name, "ALERT_OF_ERROR_%s", msg_num);
	
	error_msg_console = get_msg_from_dict("", msg_name);
	error_msg = get_msg_from_dict(nvram_safe_get("preferred_lang"), msg_name);
	logmessage("httpd", "%s %s", msg_num, error_msg);	
	return;
}

static void clean_error_msg(){
	if (error_msg_console != NULL)
		free(error_msg_console);
	
	if (error_msg != NULL)
		free(error_msg);
	
	return;
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
				nvram_set(name, value);
		}
	}
}

#define NVRAM_MODIFIED_BIT 	1
#define NVRAM_MODIFIED_WL_BIT 	2

int validate_instance(webs_t wp, char *name)
{
	char prefix[32], word[100], tmp[100], *next, *value;
	char prefix1[32], word1[100], *next1;
	int i=0; /*, j=0;*/
	int found = 0;

	// handle instance for wlx, wanx, lanx
	if(strncmp(name, "wl", 2)==0) {
		foreach(word, nvram_safe_get("wl_ifnames"), next) {
			sprintf(prefix, "wl%d_", i++);
			value = websGetVar(wp, strcat_r(prefix, name+3, tmp), NULL);
			if(!value) {
				// find variable with subunit
				foreach(word1, nvram_safe_get(strcat_r(prefix, "vifnames", tmp)), next1) {
					sprintf(prefix1, "%s_", word1);
					value = websGetVar(wp, strcat_r(prefix1, name+3, tmp), NULL);
					//printf("find %s\n", tmp);

					if(value) break;
				}
			}
		
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				//printf("instance value %s=%s\n", tmp, value);
				nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT|NVRAM_MODIFIED_WL_BIT;
			}
		}
	} 
	else if(strncmp(name, "wan", 3)==0) {
		foreach(word, nvram_safe_get("wan_ifnames"), next) {
			sprintf(prefix, "wan%d_", i++);
			value = websGetVar(wp, strcat_r(prefix, name+4, tmp), NULL);
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT;
			}
		}
	}
#ifdef RTCONFIG_DSL
	else if(strncmp(name, "dsl", 3)==0) {
		for(i=0;i<8;i++) {
			sprintf(prefix, "dsl%d_", i++);
			value = websGetVar(wp, strcat_r(prefix, name+4, tmp), NULL);
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT;
			}
		}
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
                        value = websGetVar(wp, strcat_r(prefix, name+11, tmp), NULL);
                        if(value && strcmp(nvram_safe_get(tmp), value)) {
                                nvram_set(tmp, value);
                                found = NVRAM_MODIFIED_BIT;
                        }
                }
        }
       else if(strncmp(name, "vpn_client_", 11)==0) {
                for(i=1;i<3;i++) {
                        sprintf(prefix, "vpn_client%d_", i);
                        value = websGetVar(wp, strcat_r(prefix, name+11, tmp), NULL);
                        if(value && strcmp(nvram_safe_get(tmp), value)) {
                                nvram_set(tmp, value);
                                found = NVRAM_MODIFIED_BIT;
                        }
                }
        }
#endif

	return found;
}

static int validate_apply(webs_t wp) {
	struct nvram_tuple *t;
	char *value;
	char name[64];
	char buff[100], tmp[100], prefix[32];
	int unit=-1, subunit=-1;
	int nvram_modified = 0;
	int nvram_modified_wl = 0;
	int ret;
#ifdef RTCONFIG_USB
	int acc_modified = 0;
	char orig_acc[128], modified_acc[128], modified_pass[128];

	memset(orig_acc, 0, 128);
	memset(modified_acc, 0, 128);
	memset(modified_pass, 0, 128);
#endif

	/* go through each nvram value */
	for (t = router_defaults; t->name; t++)
	{
		memset(name, 0, 64);
		sprintf(name, "%s", t->name);
	
		value = websGetVar(wp, name, NULL);

		if(!value) {
			if((ret=validate_instance(wp, name))) {
				if(ret&NVRAM_MODIFIED_BIT) nvram_modified = 1;
				if(ret&NVRAM_MODIFIED_WL_BIT) nvram_modified_wl = 1;
			}
		}
		else {
			_dprintf("value %s=%s\n", name, value);

#ifdef RTCONFIG_USB
			memset(buff, 0, 100);
			strncpy(buff, nvram_safe_get(name), 100);
#endif

			// unit nvram should be in fron of each apply,
			// seems not a good design

			if(!strcmp(name, "wl_unit") ||
			   !strcmp(name, "wan_unit") ||
			   !strcmp(name, "lan_unit")
#ifdef RTCONFIG_DSL
			|| !strcmp(name, "dsl_unit")
#endif
#ifdef RTCONFIG_OPENVPN
                        || !strcmp(name, "vpn_server_unit")
			|| !strcmp(name, "vpn_client_unit")
#endif

) {
				unit=atoi(value);
				if(unit!=nvram_get_int(name)) {
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
			else if(!strncmp(name, "wl_", 3) && unit!=-1) {		
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
				}
			}
			else if(!strncmp(name, "wan_", 4) && unit!=-1) {		
				snprintf(prefix, sizeof(prefix), "wan%d_", unit);
				(void)strcat_r(prefix, name+4, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
			else if(!strncmp(name, "lan_", 4) && unit!=-1) {
				snprintf(prefix, sizeof(prefix), "lan%d_", unit);
				(void)strcat_r(prefix, name+4, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
#ifdef RTCONFIG_DSL
			else if(!strncmp(name, "dsl_", 4) && unit!=-1) {
				snprintf(prefix, sizeof(prefix), "dsl%d_", unit);
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
#endif
			// TODO: add other multiple instance handle here
			else if(strcmp(buff, value)) {
				nvram_set(name, value);
				nvram_modified = 1;

				// the flag is set only when username or password is changed
				if(!strcmp(t->name, "http_username")
						|| !strcmp(t->name, "http_passwd")){
#ifdef RTCONFIG_USB
					if(!strcmp(t->name, "http_username")){
						strncpy(orig_acc, buff, 128);
						strncpy(modified_acc, value, 128);
					}
					else if(!strcmp(t->name, "http_passwd"))
						strncpy(modified_pass, value, 128);
					
					acc_modified = 1;
#endif

					change_passwd = 1;
					// ugly solution?
					notify_rc("chpass");
				}
			}
		}
	}

#ifdef RTCONFIG_USB
	if(acc_modified){
		if(strlen(orig_acc) <= 0)
			strncpy(orig_acc, nvram_safe_get("http_username"), 128);
		if(strlen(modified_pass) <= 0)
			strncpy(modified_pass, nvram_safe_get("http_passwd"), 128);

		if(strlen(modified_acc) <= 0)
			mod_account(orig_acc, NULL, modified_pass);
		else
			mod_account(orig_acc, modified_acc, modified_pass);

		notify_rc_and_wait("restart_ftpsamba");
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
		// TODO: is it necessary to separate the different?
		if(nvram_match("x_Setting", "0")){
			nvram_set("x_Setting", "1");
		}
		if (nvram_modified_wl)
			nvram_set("w_Setting", "1");
		nvram_commit();
	}
   
	return nvram_modified;
}


static int ej_update_variables(int eid, webs_t wp, int argc, char_t **argv) {
	char *action_mode;
	char *action_script;
	char *action_wait;
	char *wan_unit = websGetVar(wp, "wan_unit", "0");
	char notify_cmd[128];
	
	// assign control variables
	action_mode = websGetVar(wp, "action_mode", "");
	action_script = websGetVar(wp, "action_script", "restart_net");
	action_wait = websGetVar(wp, "action_wait", "5");

	_dprintf("update_variables: [%s] [%s]\n", action_mode, action_script);

	if (!strcmp(action_mode, "apply") || 
	    !strcmp(action_mode, " Apply ")) // for backwork compatibility 
	{
		if (!validate_apply(wp)) {
			websWrite(wp, "<script>no_changes_and_no_committing();</script>\n");
		}
		else {
			websWrite(wp, "<script>done_committing();</script>\n");
		}

		if (strlen(action_script) > 0) {
			memset(notify_cmd, 0, 32);
			if(strstr(action_script, "_wan_if"))
				sprintf(notify_cmd, "%s %s", action_script, wan_unit);
			else
				strncpy(notify_cmd, action_script, 128);

			notify_rc(notify_cmd);
		}

		websWrite(wp, "<script>restart_needed_time(%d);</script>\n", atoi(action_wait));
	}
	return 0;		
}

static int convert_asus_variables(int eid, webs_t wp, int argc, char_t **argv)
{
	
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
	int rulenum = atoi(nvram_safe_get("qos_rulenum_x")), idx_class = 0;
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
//	printf("####[hook] check_hwnat####");
	if(!enable_hwnat())
		websWrite(wp, "0");
	else
		websWrite(wp, "1");

	return 0;
}

void logmessage(char *logheader, char *fmt, ...)
{
  va_list args;
  char buf[512];

  va_start(args, fmt);

  vsnprintf(buf, sizeof(buf), fmt, args);
  openlog(logheader, 0, 0);
  syslog(0, buf);
  closelog();
  va_end(args);
}

static int wanstate_hook(int eid, webs_t wp, int argc, char_t **argv){
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;

	/* current unit */
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

static int ajax_wanstate_hook(int eid, webs_t wp, int argc, char_t **argv){
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;

	/* current unit */
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

	/* current unit */
	unit = wan_primary_ifunit();

printf("httpd: unit: %d\n", unit);

	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if(unit == 1)
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
		else if(wan_state == WAN_STATE_STOPPED){
			status = 0;
		}
		else{
			status = 1;
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
		if(wan_state == WAN_STATE_STOPPED && wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR){
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

	if(unit == 1)
		type = "USB Modem";
	else // dhcp
		type = wan_proto;

	if(status != 0){
		ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
		netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
		gateway = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
		lease = nvram_get_int(strcat_r(prefix, "lease", tmp));
		if (lease > 0)
			expires = nvram_get_int(strcat_r(prefix, "expires", tmp)) - uptime();
	}

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

	if (strcmp(wan_proto, "pppoe") == 0 ||
	    strcmp(wan_proto, "pptp") == 0 ||
	    strcmp(wan_proto, "l2tp") == 0) {
		int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
#if 1 /* TODO: tmporary change! remove after WEB UI support */
		if (strcmp(wan_proto, "pppoe") == 0 &&
		    dhcpenable && nvram_match(strcat_r(prefix, "vpndhcp", tmp), "0"))
			dhcpenable = 2;
#endif /* TODO: tmporary change! remove after WEB UI support */

		if (dhcpenable == 0)
			xtype = "static";
		else if (dhcpenable != 2 || strcmp(wan_proto, "pppoe") != 0)
			xtype = "dhcp";
		xip = nvram_safe_get(strcat_r(prefix, "xipaddr", tmp));
		xnetmask = nvram_safe_get(strcat_r(prefix, "xnetmask", tmp));
		xgateway = nvram_safe_get(strcat_r(prefix, "xgateway", tmp));
		xlease = nvram_get_int(strcat_r(prefix, "xlease", tmp));
		if (xlease > 0)
			xexpires = nvram_get_int(strcat_r(prefix, "xexpires", tmp)) - uptime();
	}

	websWrite(wp, "function wanlink_xtype() { return '%s';}\n", xtype);
	websWrite(wp, "function wanlink_xipaddr() { return '%s';}\n", xip);
	websWrite(wp, "function wanlink_xnetmask() { return '%s';}\n", xnetmask);
	websWrite(wp, "function wanlink_xgateway() { return '%s';}\n", xgateway);
	websWrite(wp, "function wanlink_xdns() { return '%s';}\n", nvram_safe_get(strcat_r(prefix, "xdns", tmp)));
	websWrite(wp, "function wanlink_xlease() { return %d;}\n", xlease);
	websWrite(wp, "function wanlink_xexpires() { return %d;}\n", xexpires);

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
	unit = 1;
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if(unit == 1)
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
		else if(wan_state == WAN_STATE_STOPPED){
			status = 0;
		}
		else{
			status = 1;
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
		if(wan_state == WAN_STATE_STOPPED && wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR){
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

	if(unit == 1)
		type = "USB Modem";
	else // dhcp
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
	websWrite(wp, "function is_private_subnet() { return %d;}\n", is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp))));

	if (strcmp(wan_proto, "pppoe") == 0 ||
	    strcmp(wan_proto, "pptp") == 0 ||
	    strcmp(wan_proto, "l2tp") == 0) {
		int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
#if 1 /* TODO: tmporary change! remove after WEB UI support */
		if (strcmp(wan_proto, "pppoe") == 0 &&
		    dhcpenable && nvram_match(strcat_r(prefix, "vpndhcp", tmp), "0"))
			dhcpenable = 2;
#endif /* TODO: tmporary change! remove after WEB UI support */

		if (dhcpenable == 0)
			xtype = "static";
		else if (dhcpenable != 2 || strcmp(wan_proto, "pppoe") != 0)
			xtype = "dhcp";
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
	websWrite(wp, "function is_private_subnet() { return -1;}\n");

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
		notify_rc("start_wan");
	}
	else if (!strcmp(action, "Disconnect")){
		nvram_set_int(wan_enable, 1);
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

static int ej_get_parameter(int eid, webs_t wp, int argc, char_t **argv){
//	char *c;
	bool last_was_escaped;
	int ret = 0;
	
	if (argc < 1){
		websError(wp, 400,
				"get_parameter() used with no arguments, but at least one "
				"argument is required to specify the parameter name\n");
		return -1;
	}
	
	last_was_escaped = FALSE;
	
	char *value = websGetVar(wp, argv[0], "");
	websWrite(wp, "%s", value);//*/
	/*for (c = websGetVar(wp, argv[0], ""); *c; c++){
		if (isprint((int)*c) &&
			*c != '"' && *c != '&' && *c != '<' && *c != '>' && *c != '\\' &&
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
			ret += websWrite(wp, "&#%d", *c);
			last_was_escaped = TRUE;
		}
	}//*/
	
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
//		csprintf("peer: %x\n", sa->sin_addr.s_addr);	// J++
		return (unsigned int)sa->sin_addr.s_addr;
	}
	else{
		csprintf("error: %d %d \n", ret, errno);
		return 0;
	}
}

extern long uptime(void);

static int login_state_hook(int eid, webs_t wp, int argc, char_t **argv){
	unsigned int ip, login_ip;
	char ip_str[16], login_ip_str[16];
	time_t login_timestamp;
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
	login_timestamp = strtoul(nvram_safe_get("login_timestamp"), NULL, 10);
	
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
	
	if (ip != 0 && login_ip == ip){
		websWrite(wp, "function is_logined() { return 1; }\n");
		websWrite(wp, "function login_ip_dec() { return '%u'; }\n", login_ip);
		websWrite(wp, "function login_ip_str() { return '%s'; }\n", login_ip_str);
		websWrite(wp, "function login_ip_str_now() { return '%s'; }\n", ip_str);
		
		if (values[3] != NULL)
			websWrite(wp, "function login_mac_str() { return '%s'; }\n", values[3]);
		else
			websWrite(wp, "function login_mac_str() { return ''; }\n");
//		time(&login_timestamp);
		login_timestamp = uptime();
	}
	else{
		websWrite(wp, "function is_logined() { return 0; }\n");
		websWrite(wp, "function login_ip_dec() { return '%u'; }\n", login_ip);
		
		if ((unsigned long)(now-login_timestamp) > 60)	//one minitues
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


int ej_dhcp_leases(int eid, webs_t wp, int argc, char_t **argv)
{
#ifndef DNSMASQ
	FILE *fp = NULL;
	struct lease_t lease;
	struct in_addr addr;
	unsigned long expires = 0;
	int i, firstRow;
	char str[80], buf[80];

	doSystem("killall -%d udhcpd", SIGUSR1);	

	/* Write out leases file */
	if (!(fp = fopen("/tmp/udhcpd-br0.leases", "r")))
		return 0;
	
	firstRow = 1;
	while (fread(&lease, sizeof(lease), 1, fp)){	
		/* Do not display reserved leases */
		if (ETHER_ISNULLADDR(lease.chaddr))
			continue;
		
		if (firstRow == 1)
			firstRow = 0;
		else
			websWrite(wp, ", ");
		websWrite(wp, "[");
		
		memset(str, 0, 80);
		memset(buf, 0, 80);
		sprintf(buf, "%-16s", lease.hostname);
		sscanf(buf, "%s", str);
		websWrite(wp, "\"%s\", ", str);
		
		websWrite(wp, "\"");
		for (i = 0; i < 6; ++i){
			websWrite(wp, "%02X", lease.chaddr[i]);
			if (i != 5)
				websWrite(wp, ":");
		}
		websWrite(wp, "\", ");
		
		addr.s_addr = lease.yiaddr;
		memset(str, 0, 80);
		memset(buf, 0, 80);
		sprintf(buf, "%-15s", inet_ntoa(addr));
		sscanf(buf, "%s", str);
		websWrite(wp, "\"%s\", ", str);
		expires = ntohl(lease.expires);
		
		if (expires == 0xffffffff)
			websWrite(wp, "\"Manual\"");
		else if (!expires)
			websWrite(wp, "\"Expired\"");
		else{
			char lease_buf[128];
			memset(lease_buf, 0, sizeof(lease_buf));
			memset(str, 0, 80);
			memset(buf, 0, 80);
			reltime(expires, lease_buf);
			sprintf(buf, "%-15s", lease_buf);
			sscanf(buf, "%s", str);
			websWrite(wp, "\"%s\"", str);
		}
		
		websWrite(wp, "]");
	}
	
	fclose(fp);
#endif
	return 0;
}

#ifdef DNSMASQ
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
ej_lan_leases(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	struct in_addr addr4;
	struct in6_addr addr6;
	char line[256], timestr[sizeof("999:59:59")];
	char *hwaddr, *ipaddr, *name, *next;
	unsigned int expires;
	int ret = 0;

	ret += websWrite(wp, "%-9s %-17s %-15s %s\n",
		"Expires", "MAC Address", "IP Address", "Host name");

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
		ret += websWrite(wp, "%-9s %-17s %-15s %s\n",
			expires ? timestr : "Static",
			hwaddr, ipaddr, name);
	}
	fclose(fp);

	return ret;
}

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
                return ret;

        /* Read leases file */
        if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r")))
                return ret;

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
#else
int
ej_dhcpLeaseInfo(int eid, webs_t wp, int argc, char_t **argv)
{
	return "";
}

int
ej_IP_dhcpLeaseInfo(int eid, webs_t wp, int argc, char_t **argv)
{
        return "[['', '']]";
}

/* Dump leases in <tr><td>hostname</td><td>MAC</td><td>IP</td><td>expires</td></tr> format */
int
ej_lan_leases(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp = NULL;
	struct lease_t lease;
	int i;
	struct in_addr addr;
	unsigned long expires = 0;
	int ret = 0;
	char lease_buf[128];
	
	memset(lease_buf, 0, sizeof(lease_buf));

_dprintf("^^^^ dproxy:: ej_lan_leases ^^^^\n");
	ret += websWrite(wp, "Host Name       Mac Address       IP Address	  Lease Time\n");

	/* Write out leases file */
	if (!(fp = fopen("/tmp/udhcpd-br0.leases", "r")))
		return ret;

	i = 0;
	while (fread(&lease, sizeof(lease), 1, fp)) {
		/* Do not display reserved leases */
		if (ETHER_ISNULLADDR(lease.chaddr))
			continue;
	
		ret += websWrite(wp, "%-16s", lease.hostname);
		for (i = 0; i < 6; ++i){
			ret += websWrite(wp, "%02X", lease.chaddr[i]);
			if (i != 5) ret += websWrite(wp, ":");
		}

		addr.s_addr = lease.yiaddr;
		ret += websWrite(wp, " %-15s ", inet_ntoa(addr));
		expires = ntohl(lease.expires);
		
		if (expires == 0xffffffff) ret += websWrite(wp, "Manual\n");
		else if (!expires) ret += websWrite(wp, "Expired\n");
		else {
			reltime(expires, lease_buf);
			ret += websWrite(wp, "%s\n", lease_buf);
		}
	}	
	fclose(fp);

#ifdef GUEST_ACCOUNT
	if(nvram_invmatch("wl_guest_enable", "1")) return ret;

	/* Write out leases file */
	if (!(fp = fopen("/tmp/udhcpd-br1.leases", "r")))
		return ret;

	while (fread(&lease, sizeof(lease), 1, fp)) {
		/* Do not display reserved leases */
		if (ETHER_ISNULLADDR(lease.chaddr))
			continue;

		//printf("lease: %s %d\n", lease.hostname, strlen(lease.hostname));
		ret += websWrite(wp, "%-16s", lease.hostname);
		for (i = 0; i < 6; i++) {
			ret += websWrite(wp, "%02X", lease.chaddr[i]);
			if (i != 5) ret += websWrite(wp, ":");
		}
		addr.s_addr = lease.yiaddr;
		ret += websWrite(wp, " %-15s ", inet_ntoa(addr));
		expires = ntohl(lease.expires);

		if (expires==0xffffffff) ret += websWrite(wp, "Manual\n");
		else if (!expires) ret += websWrite(wp, "Expired\n");
		else ret += websWrite(wp, "%s\n", reltime(expires));
	}
	fclose(fp);
#endif

		return ret;
}
#endif

#ifdef RTCONFIG_IPV6
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

int
ej_route_table(int eid, webs_t wp, int argc, char_t **argv)
{
	char buff[256];
	int  nl = 0;
	struct in_addr dest;
	struct in_addr gw;
	struct in_addr mask;
	int flgs, ref, use, metric, ret=0;
	char flags[4];
	unsigned long int d,g,m;
	char sdest[16], sgw[16], *iface;
	FILE *fp;
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char buf[256];
	unsigned int fl = 0;
	int found = 0;

	ret += websWrite(wp, "Destination     Gateway         Genmask         Flags Metric Ref    Use Iface\n");

	if (!(fp = fopen("/proc/net/route", "r"))) return 0;

	while (fgets(buff, sizeof(buff), fp) != NULL ) 
	{
		if (nl) 
		{
			int ifl = 0;
			while (buff[ifl]!=' ' && buff[ifl]!='\t' && buff[ifl]!='\0')
				ifl++;
			buff[ifl]=0;    /* interface */
			if (sscanf(buff+ifl+1, "%lx%lx%d%d%d%d%lx",
			   &d, &g, &flgs, &ref, &use, &metric, &m)!=7) {
				//error_msg_and_die( "Unsuported kernel route format\n");
				//continue;
			}

			ifl = 0;	/* parse flags */
			if (flgs&1)
				flags[ifl++]='U';
			if (flgs&2)
				flags[ifl++]='G';
			if (flgs&4)
				flags[ifl++]='H';
			flags[ifl]=0;
			dest.s_addr = d;
			gw.s_addr   = g;
			mask.s_addr = m;
			strcpy(sdest,  (dest.s_addr==0 ? "default" :
					inet_ntoa(dest)));
			strcpy(sgw,    (gw.s_addr==0   ? "*"       :
					inet_ntoa(gw)));

			/* Skip interfaces here */
			if (strstr(buff, "lo"))
				continue;

			/* If unknown, just expose interface name */
			iface = buff;
			if (nvram_match("lan_ifname", buff)) /* br0, wl0, etc */
				iface = "LAN";
			else
			/* Tricky, it's better to move wan_ifunit/wanx_ifunit to shared instead */
			for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++) {
				wan_prefix(unit, prefix);
				if (nvram_match(strcat_r(prefix, "pppoe_ifname", tmp), buff)) {
					iface = "WAN";
					break;
				}
				if (nvram_match(strcat_r(prefix, "ifname", tmp), buff)) {
					char *wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
					iface = (strcmp(wan_proto, "dhcp") == 0 ||
						 strcmp(wan_proto, "static") == 0 ) ? "WAN" : "MAN";
					break;
				}
			}

			ret += websWrite(wp, "%-16s%-16s%-16s%-6s%-6d %-2d %7d %s\n",
			sdest, sgw,
			inet_ntoa(mask),
			flags, metric, ref, use, iface);
		}
		nl++;
	}
	fclose(fp);

#ifdef RTCONFIG_IPV6
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

	return ret;
}

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

#ifdef RTCONFIG_WIRELESSREPEATER
static int ej_get_ap_info(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	char buf[MAX_LINE_SIZE];
	int ret_l = 0;
	unsigned int i_l, len_l;
	int i;

	// get info from file generated by wlc_scan
	if(nvram_get_int("wlc_scan_state")!=WLCSCAN_STATE_FINISHED){
		ret_l += websWrite(wp, "[]");
		return ret_l;
	}
	spinlock_lock(SPINLOCK_SiteSurvey);
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
	spinlock_unlock(SPINLOCK_SiteSurvey);
	return ret_l;
}
#endif

//2011.03 Yau add for new networkmap
static int ej_get_client_detail_info(int eid, webs_t wp, int argc, char_t **argv){
	int i, shm_client_info_id;
	void *shared_client_info=(void *) 0;
	char output_buf[256];
	char devname[LINE_SIZE], character;
	int j, len;

	P_CLIENT_DETAIL_INFO_TABLE p_client_info_tab;

	spinlock_lock(SPINLOCK_Networkmap);
	shm_client_info_id = shmget((key_t)1001, sizeof(CLIENT_DETAIL_INFO_TABLE), 0666|IPC_CREAT);
	if (shm_client_info_id == -1){
	    fprintf(stderr,"shmget failed\n");
	    spinlock_unlock(SPINLOCK_Networkmap);
	    return 0;
	}

	shared_client_info = shmat(shm_client_info_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr,"shmat failed\n");
		spinlock_unlock(SPINLOCK_Networkmap);
		return 0;
	}

	p_client_info_tab = (P_CLIENT_DETAIL_INFO_TABLE)shared_client_info;
	for(i=0; i<p_client_info_tab->ip_mac_num; i++) {
		memset(output_buf, 0, 256);
		memset(devname, 0, LINE_SIZE);

		len = strlen( p_client_info_tab->device_name[i]);
		for (j=0; (j < len) && (j < LINE_SIZE-1); j++) {
			character = p_client_info_tab->device_name[i][j];
			if ((isalnum(character)) || (character == ' ') || (character == '-') || (character == '_'))
				devname[j] = character;
			else
				devname[j] = ' ';
		}

		sprintf(output_buf, "<%d>%s>%d.%d.%d.%d>%02X:%02X:%02X:%02X:%02X:%02X>%d>%d>%d",
		p_client_info_tab->type[i],
		devname,
		p_client_info_tab->ip_addr[i][0],p_client_info_tab->ip_addr[i][1],
		p_client_info_tab->ip_addr[i][2],p_client_info_tab->ip_addr[i][3],
		p_client_info_tab->mac_addr[i][0],p_client_info_tab->mac_addr[i][1],
		p_client_info_tab->mac_addr[i][2],p_client_info_tab->mac_addr[i][3],
		p_client_info_tab->mac_addr[i][4],p_client_info_tab->mac_addr[i][5],
		p_client_info_tab->http[i],
		p_client_info_tab->printer[i],
		p_client_info_tab->itune[i]
		);
		websWrite(wp, output_buf);
		if(i < p_client_info_tab->ip_mac_num-1)
			websWrite(wp, ",");
	}
	spinlock_unlock(SPINLOCK_Networkmap);

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


static int ej_get_changed_status(int eid, webs_t wp, int argc, char_t **argv){
	char *arp_info = read_whole_file("/proc/net/arp");
#ifdef RTCONFIG_USB
	char *disk_info = read_whole_file(PARTITION_FILE); 
	char *mount_info = read_whole_file("/proc/mounts"); 
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
			if (follow_partition->mount_point != NULL){
				for (follow_disk2 = disks_info, disk_num = 0; follow_disk2 != NULL; follow_disk2 = follow_disk2->next, ++disk_num){
					websWrite(wp, "	if (disk_num == %d) {\n", disk_num);

					//if (strcmp(follow_disk2->tag, follow_disk->tag) == 0)
					if (follow_disk2->major == follow_disk->major && follow_disk2->minor == follow_disk->minor)
						websWrite(wp, "	    return [%llu];\n", follow_partition->size_in_kilobytes);
					else
						websWrite(wp, "	    return [0];\n");

					websWrite(wp, "	}\n");
				}
			}
			else
				websWrite(wp, "	return [0];\n");
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
	/* show total number of the mounted partition in this foreign disk */
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

#if 0
static int ej_get_printer_info(int eid, webs_t wp, int argc, char_t **argv){
	FILE *lpfp;
	char manufacturer[100], models[100], serialnos[100], pool[100];
	char buf[500];
	char *lpparam, *value, *v1 = NULL;
	
	if (!(lpfp = fopen("/proc/usblp/usblpid", "r"))){
		strcpy(manufacturer, "");
		strcpy(models, "");
		strcpy(serialnos, "");
		strcpy(pool, "");
	}
	else{
		while (fgets(buf, 500, lpfp)){
			value = &buf[0];
			lpparam = strsep(&value, "=")?:&buf[0];
			
			if (value){
				v1 = strchr(value, '\n');
				*v1 = '\0';
				
				if (!strcmp(lpparam, "Manufacturer"))
					sprintf(manufacturer, "\"%s\"", value);
				else if (!strcmp(lpparam, "Model"))
					sprintf(models, "\"%s\"", value );
			}
		}
		
		/* compatible for WL700gE platform */
		strcpy(pool, "VirtualPool");
		fclose(lpfp);
	}
	
	websWrite(wp, "function printer_manufacturers() {\n return [%s];\n}\n", manufacturer);
	websWrite(wp, "function printer_models() {\n return [%s];\n}\n", models);
	websWrite(wp, "function printer_serialn() {\n return [%s];\n}\n", "");
	websWrite(wp, "function printer_pool() {\n return [\"%s\"];\n}\n", pool);
	
	return 0;
}
#else
#define MAX_PRINTER_NUM 2

static int ej_get_printer_info(int eid, webs_t wp, int argc, char_t **argv){
	int port_num = 0, first;
	char tmp[64], prefix[] = "usb_pathX";
	char printer_array[2][5][64];

	for(port_num = 1; port_num <= MAX_PRINTER_NUM; ++port_num){
		snprintf(prefix, sizeof(prefix), "usb_path%d", port_num);

		memset(printer_array[port_num-1][0], 0, 64);
		memset(printer_array[port_num-1][1], 0, 64);
		memset(printer_array[port_num-1][2], 0, 64);
		memset(printer_array[port_num-1][3], 0, 64);
		memset(printer_array[port_num-1][4], 0, 64);

		if(nvram_match(prefix, "printer")){
			strncpy(printer_array[port_num-1][0], "printer", 64);
			strncpy(printer_array[port_num-1][1], nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp)), 64);
			strncpy(printer_array[port_num-1][2], nvram_safe_get(strcat_r(prefix, "_product", tmp)), 64);
			strncpy(printer_array[port_num-1][3], nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 64);
			if(!strcmp(printer_array[port_num-1][3], nvram_safe_get("u2ec_serial")))
				strcpy(printer_array[port_num-1][4], "VirtualPool");
			else
				strcpy(printer_array[port_num-1][4], "");
		}
	}

	websWrite(wp, "function printer_manufacturers(){\n");
	websWrite(wp, "    return [");

	first = 1;
	for(port_num = 1; port_num <= MAX_PRINTER_NUM; ++port_num){
		if(strlen(printer_array[port_num-1][0]) > 0)
			websWrite(wp, "\"%s\"", printer_array[port_num-1][1]);
		else
			websWrite(wp, "\"\"");
		if(first){
			--first;
			websWrite(wp, ", ");
		}
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function printer_models(){\n");
	websWrite(wp, "    return [");

	first = 1;
	for(port_num = 1; port_num <= MAX_PRINTER_NUM; ++port_num){
		if(strlen(printer_array[port_num-1][0]) > 0)
			websWrite(wp, "\"%s\"", printer_array[port_num-1][2]);
		else
			websWrite(wp, "\"\"");
		if(first){
			--first;
			websWrite(wp, ", ");
		}
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function printer_serialn(){\n");
	websWrite(wp, "    return [");

	first = 1;
	for(port_num = 1; port_num <= MAX_PRINTER_NUM; ++port_num){
		if(strlen(printer_array[port_num-1][0]) > 0)
			websWrite(wp, "\"%s\"", printer_array[port_num-1][3]);
		else
			websWrite(wp, "\"\"");
		if(first){
			--first;
			websWrite(wp, ", ");
		}
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function printer_pool(){\n");
	websWrite(wp, "    return [");

	first = 1;
	for(port_num = 1; port_num <= MAX_PRINTER_NUM; ++port_num){
		if(strlen(printer_array[port_num-1][0]) > 0)
			websWrite(wp, "\"%s\"", printer_array[port_num-1][4]);
		else
			websWrite(wp, "\"\"");
		if(first){
			--first;
			websWrite(wp, ", ");
		}
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	return 0;
}
#endif
#endif

int ej_shown_time(int eid, webs_t wp, int argc, char **argv){
	time_t t1;
	
	time(&t1);
	
	websWrite(wp, "%e", t1);
	
	return 0;
}

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
	char buffer[1024], key[16], target[16];
	char *follow_info, *follow_info_end;
	int offset = 0;

	if (fp == NULL){
		fprintf(stderr, "No English dictionary!\n");
		return 0;
	}

	memset(lang, 0, 4);
	strcpy(lang, nvram_safe_get("preferred_lang"));
	websWrite(wp, "<li><dl><dt id=\"selected_lang\"></dt>\\n");
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

				if(strcmp(key,lang))
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

int ej_shown_language_option(int eid, webs_t wp, int argc, char **argv){
	struct language_table *pLang = NULL;
	char lang[4];
	int len;
#ifdef RTCONFIG_AUTODICT
	FILE *fp = fopen("Lang_Hdr.txt", "r");
#else
	FILE *fp = fopen("Lang_Hdr", "r");
#endif
	char buffer[1024], key[16], target[16];
	char *follow_info, *follow_info_end;

	if (fp == NULL){
		fprintf(stderr, "No English dictionary!\n");
		return 0;
	}

#ifdef RTCONFIG_AUTODICT
	// skip <feff>
	fread(key, 1, 3, fp);
#endif

	memset(lang, 0, 4);
	strcpy(lang, nvram_safe_get("preferred_lang"));

	while (1) {
		memset(buffer, 0, sizeof(buffer));
		if ((follow_info = fgets(buffer, sizeof(buffer), fp)) != NULL){
			if (strncmp(follow_info, "LANG_", 5))    // 5 = strlen("LANG_")
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

				if (!strcmp(key, lang))
					websWrite(wp, "<option value=\"%s\" selected>%s</option>\\n", key, target);
				else
					websWrite(wp, "<option value=\"%s\">%s</option>\\n", key, target);

				break;
			}
		}
		else
			break;
	}
	fclose(fp);

	return 0;
}

static int
apply_cgi(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
		char_t *url, char_t *path, char_t *query)
{
	char *action_mode;
	char *action_para;
	char *current_url;
	char command[32];

	action_mode = websGetVar(wp, "action_mode","");	
	current_url = websGetVar(wp, "current_page", "");	
	_dprintf("apply: %s %s\n", action_mode, current_url);
	
	if (!strcmp(action_mode, "apply")) {
		if (!validate_apply(wp)) {
			websWrite(wp, "NOT MODIFIED\n");
		}
		else {
			websWrite(wp, "MODIFIED\n");
		}

		action_para = websGetVar(wp, "rc_service","");

		if(action_para && strlen(action_para) > 0) {
			notify_rc(action_para);
		}
		websWrite(wp, "RUN SERVICE\n");
	}
	else if (!strcmp(action_mode," Refresh "))
	{
		strcpy(SystemCmd, websGetVar(wp,"SystemCmd",""));
		//csprintf("1. SystemCmd: %s.\n", SystemCmd);
		websRedirect(wp, current_url);
		return 0;
	}
	else if (!strcmp(action_mode," Clear "))
	{
		unlink("/tmp/syslog.log");
		websRedirect(wp, current_url);
		return 0;
	}
	else if (!strcmp(action_mode, " Restart ")||!strcmp(action_mode, "reboot"))
	{
		websApply(wp, "Restarting.asp");
		shutdown(fileno(wp), SHUT_RDWR);
		sys_reboot();
		return (0);
	}
	else if (!strcmp(action_mode, "Restore")||!strcmp(action_mode, "restore"))
	{
		websApply(wp, "Restarting.asp");
		shutdown(fileno(wp), SHUT_RDWR);
#ifdef RTCONFIG_DSL
		system("adslate sysdefault"); /* Paul add 2012/8/7 */
#endif
		sys_default();
		return (0);
	}	
	else if (!strcmp(action_mode, "logout")) // but, every one can reset it by this call
	{
		http_logout(0);
		websRedirect(wp, "Nologin.asp");
		return (0);
	}	
	else if (!strcmp(action_mode, "change_wl_unit"))
	{
		action_para = websGetVar(wp, "wl_unit","");

		if(action_para) 
			nvram_set("wl_unit", action_para);

		action_para = websGetVar(wp, "wl_subunit","");

		if(action_para) 
			nvram_set("wl_subunit", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "change_wps_unit"))
	{
		action_para = websGetVar(wp, "wps_band","");

		if(action_para) 
			nvram_set("wps_band", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "wps_apply"))
	{
		action_para = websGetVar(wp, "wps_band","");
		if(action_para) 
			nvram_set("wps_band", action_para);
		else goto wps_finish;
		
		action_para = websGetVar(wp, "wps_enable","");
		if(action_para) 
			nvram_set("wps_enable", action_para);
		else goto wps_finish;

		action_para = websGetVar(wp, "wps_sta_pin","");
		if(action_para) 
			nvram_set("wps_sta_pin", action_para);
		else goto wps_finish;

		notify_rc("start_wps_method");

wps_finish:
		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "wps_reset"))
	{
		action_para = websGetVar(wp, "wps_band","");

		if(action_para) 
			nvram_set("wps_band", action_para);

		notify_rc("reset_wps");

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "change_wan_unit"))
	{
		action_para = websGetVar(wp, "wan_unit", "");

		if(action_para) 
			nvram_set("wan_unit", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "change_lan_unit"))
	{
		action_para = websGetVar(wp, "lan_unit","");

		if(action_para) 
			nvram_set("lan_unit", action_para);

		websRedirect(wp, current_url);
	}	
	else if (!strcmp(action_mode, "refresh_networkmap"))
	{
		printf("@@@ Signal to networkmap!!!\n");
		doSystem("killall -%d networkmap", SIGUSR1);
	
		websRedirect(wp, current_url);
	}		
	else if (!strcmp(action_mode, "restore_module"))
	{
		action_para = websGetVar(wp, "module_prefix","");
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
        else if (!strcmp(action_mode, "change_vpn_server_unit"))
        {
                action_para = websGetVar(wp, "vpn_server_unit", "");

                if(action_para)
                        nvram_set("vpn_server_unit", action_para);

                websRedirect(wp, current_url);
        }
        else if (!strcmp(action_mode, "change_vpn_client_unit"))
        {
                action_para = websGetVar(wp, "vpn_client_unit", "");

                if(action_para)
                        nvram_set("vpn_client_unit", action_para);

                websRedirect(wp, current_url);
        }
	return 1;
}



static void
do_auth(char *userid, char *passwd, char *realm)
{	
//      time_t tm;

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

static void
do_apply_cgi(char *url, FILE *stream)
{
    apply_cgi(stream, NULL, NULL, 0, url, NULL, NULL);
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
	if (p1[0] != '\0')      {
		strncpy (orig_lang, p1, 2);
	} else {
		strncpy (orig_lang, "EN", 2);
	}

	// read remain data
#if 0
	if (feof (stream))      {
		while ((c = fgetc(stream) != EOF))      {
			;       // fall through
		}
	}
#else
	char buf[1024];
	while ((c = fread(buf, 1, sizeof(buf), stream)) > 0)
		;		// fall through
#endif

	cprintf ("lang: %s --> %s\n", orig_lang, new_lang);
	refresh_title_asp = 0;
	if (strcmp (orig_lang, new_lang) != 0 || is_firsttime ())       {
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

static void
do_upgrade_post(char *url, FILE *stream, int len, char *boundary)
{
	#define MAX_VERSION_LEN 64
	char upload_fifo[] = "/tmp/linux.trx";
	FILE *fifo = NULL;
	char buf[4096];
	int count, ch/*, ver_chk = 0*/;
	int cnt;
	long filelen;
	
	upgrade_err=1;

	notify_rc("stop_upgrade");
	
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

	free_caches(FREE_MEM_PAGE, 5, len);

	if (!(fifo = fopen(upload_fifo, "a+"))) goto err;

	filelen = len;
	cnt = 0;

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

		count = fread(buf, 1, MIN(len, sizeof(buf)), stream);

		// pretend we can get enough info at first read
		if(cnt==0) {
			_dprintf("read from stream: %d\n", count);
			cnt++;
			if(!check_imageheader(buf, &filelen)) {	
				len-=count;
				filelen-=count;
				goto err;
			}
		}
		len-=count;
		filelen-=count;
		fwrite(buf, 1, count, fifo);
	}

	/* Slurp anything remaining in the request */
	while (len-- > 0)
	{
		ch = fgetc(stream);

		if (filelen>0)
		{
			fwrite(&ch, 1, 1, fifo);
			filelen--;
		}
	}	
	fclose(fifo);
	fifo = NULL;

#ifdef RTCONFIG_DSL
// unclear logic
//	if(dsl_check_imagefile_str(upload_fifo))
//	{
		// 1 = valid
		// 0 = invalid		
		int ret_val_sep;
		ret_val_sep = separate_tc_fw_from_trx();
		// should router update tc fw?
		if (ret_val_sep)
		{
			if(!check_tc_firmware_crc())
				upgrade_err = 0;			
		}
//	}
#else		
	if(check_imagefile(upload_fifo))
		upgrade_err = 0;
		
#endif 
err:
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		ch = fgetc(stream);
}

static void
do_upgrade_cgi(char *url, FILE *stream)
{	
	printf("## [httpd] do upgrade cgi\n");	// tmp test
	/* Reboot if successful */
	
	if (upgrade_err == 0)
	{
#ifdef RTCONFIG_DSL
		int ret_val_trunc;
#endif
		websApply(stream, "Updating.asp");
#ifdef RTCONFIG_DSL
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
		shutdown(fileno(stream), SHUT_RDWR);
	}	
	else    
	{
	   	websApply(stream, "UpdateError.asp");
	   	unlink("/tmp/linux.trx");
	}   	
	notify_rc_after_period_wait("start_upgrade", 60);
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
		count = fread(buf, 1, MIN(len, sizeof(buf)), stream);

		if (cnt == 0 && count > 8) {
			if (!strncmp(buf, PROFILE_HEADER, 4))
			{
				filelenptr = (buf + 4);
				filelen = *filelenptr;

			}
			else if (!strncmp(buf, PROFILE_HEADER_NEW, 4))
			{
				filelenptr = (buf + 4);
				filelen = *filelenptr;
				filelen = filelen & 0xffffff;

			}
			else
			{
				len -= count;
				goto err;
			}

			cmpHeader = 1;
			++cnt;
		}

		filelen -= count;
		len -= count;

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
		ch = fgetc(stream);

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

void ej_iptmon(int eid, webs_t wp, int argc, char **argv) {

	char comma;
	char sa[256];
	FILE *a;
	char *exclude;
	char *include;

	char ip[INET6_ADDRSTRLEN];

	unsigned long tx, rx;

	exclude = nvram_safe_get("cstats_exclude");
	include = nvram_safe_get("cstats_include");

	websWrite(wp, "\n\niptmon={");
	comma = ' ';

	char wholenetstatsline = 1;

	if ((a = fopen("/proc/net/ipt_account/lan", "r")) == NULL) return;

	if (!wholenetstatsline)
		fgets(sa, sizeof(sa), a); // network

	while (fgets(sa, sizeof(sa), a)) {
		if(sscanf(sa, 
			"ip = %s bytes_src = %lu %*u %*u %*u %*u packets_src = %*u %*u %*u %*u %*u bytes_dst = %lu %*u %*u %*u %*u packets_dst = %*u %*u %*u %*u %*u time = %*u",
			ip, &tx, &rx) != 3 ) continue;

		if (find_word(exclude, ip)) {
			wholenetstatsline = 0;
			continue;
		}

		if (((find_word(include, ip)) || (wholenetstatsline == 1)) || ((nvram_get_int("cstats_all")) && ((rx > 0) || (tx > 0)) )) {
//		if ((find_word(include, ip)) || (wholenetstatsline == 1)) {
//		if ((tx > 0) || (rx > 0) || (wholenetstatsline == 1)) {
//		if ((tx > 0) || (rx > 0)) {
			websWrite(wp,"%c'%s':{rx:0x%lx,tx:0x%lx}", comma, ip, rx, tx);
			comma = ',';
		}
		wholenetstatsline = 0;
	}
	fclose(a);
	websWrite(wp,"};\n");
}

void ej_ipt_bandwidth(int eid, webs_t wp, int argc, char **argv)
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
}

void ej_iptraffic(int eid, webs_t wp, int argc, char **argv) {
	char comma;
	char sa[256];
	FILE *a;
	char ip[INET_ADDRSTRLEN];

	char *exclude;

	unsigned long tx_bytes, rx_bytes;
	unsigned long tp_tcp, rp_tcp;
	unsigned long tp_udp, rp_udp;
	unsigned long tp_icmp, rp_icmp;
	unsigned int ct_tcp, ct_udp;

	exclude = nvram_safe_get("cstats_exclude");

	Node tmp;
	Node *ptr;

	iptraffic_conntrack_init();

	if ((a = fopen("/proc/net/ipt_account/lan", "r")) == NULL) return;

        websWrite(wp, "\n\niptraffic=[");
        comma = ' ';

	fgets(sa, sizeof(sa), a); // network
	while (fgets(sa, sizeof(sa), a)) {
		if(sscanf(sa, 
			"ip = %s bytes_src = %lu %*u %*u %*u %*u packets_src = %*u %lu %lu %lu %*u bytes_dst = %lu %*u %*u %*u %*u packets_dst = %*u %lu %lu %lu %*u time = %*u",
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
			websWrite(wp, "%c['%s', %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %u, %u]", 
						comma, ip, rx_bytes, tx_bytes, rp_tcp, tp_tcp, rp_udp, tp_udp, rp_icmp, tp_icmp, ct_tcp, ct_udp);
			comma = ',';
		}
	}
	fclose(a);
	websWrite(wp, "];\n");

	TREE_FORWARD_APPLY(&tree, _Node, linkage, Node_housekeeping, NULL);
	TREE_INIT(&tree, Node_compare);
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
do_prf_file(char *url, FILE *stream)
{
printf("here\n");
	nvram_commit();
	sys_download("/tmp/settings");
	do_file(url, stream);
	//unlink("/tmp/settings");
}


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

static void 
do_log_cgi(char *path, FILE *stream)
{
	dump_file(stream, "/tmp/syslog.log");
	fputs("\r\n", stream); /* terminator */
	fputs("\r\n", stream); /* terminator */
}


//2008.08 magic{
struct mime_handler mime_handlers[] = {
	{ "Nologin.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "error_page.htm*", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "gotoHomePage.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "ure_success.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "ureip.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "remote.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "jquery.js", "text/javascript", no_cache_IE7, NULL, do_file, NULL }, // 2010.09 James.
	{ "httpd_check.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "**.htm*", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "**.asp*", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	
	{ "**.gz", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.tgz", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.zip", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.ipk", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.css", "text/css", NULL, NULL, do_file, NULL },
	{ "**.png", "image/png", NULL, NULL, do_file, NULL },
	{ "**.gif", "image/gif", NULL, NULL, do_file, NULL },
	{ "**.jpg", "image/jpeg", NULL, NULL, do_file, NULL },
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
	{ "**.CFG", "application/force-download", NULL, NULL, do_prf_file, do_auth },
	
	{ "apply.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_apply_cgi, do_auth },
	{ "applyapp.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_apply_cgi, do_auth },
	{ "upgrade.cgi*", "text/html", no_cache_IE7, do_upgrade_post, do_upgrade_cgi, do_auth},
	{ "upload.cgi*", "text/html", no_cache_IE7, do_upload_post, do_upload_cgi, do_auth },
 	{ "syslog.cgi*", "application/force-download", no_cache_IE7, do_html_post_and_get, do_log_cgi, do_auth },
#ifdef RTCONFIG_DSL
 	{ "dsllog.cgi*", "text/txt", no_cache_IE7, do_html_post_and_get, do_adsllog_cgi, do_auth },
#endif
	// Viz 2010.08 vvvvv  
	{ "update.cgi*", "text/javascript", no_cache_IE7, do_html_post_and_get, do_update_cgi, do_auth }, // jerry5 
	{ "bwm/*.gz", NULL, no_cache, do_html_post_and_get, wo_bwmbackup, do_auth }, // jerry5
	// end Viz  ^^^^^^^^  
	{ "ipt/*.gz", NULL, no_cache, do_html_post_and_get, wo_iptbackup, do_auth },
#ifdef TRANSLATE_ON_FLY
	{ "change_lang.cgi*", "text/html", no_cache_IE7, do_lang_post, do_lang_cgi, do_auth },
#endif //TRANSLATE_ON_FLY
	{ NULL, NULL, NULL, NULL, NULL, NULL }
};

// some should be removed
struct except_mime_handler except_mime_handlers[] = {
	{ "QIS_*", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},	
	{ "qis/*", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},	
	{ "*.css", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "ajax.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "state.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "detect.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "popup.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "general.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "help.js", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "start_autodet.asp", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "start_apply.htm", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "start_apply2.htm", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "setting_lan.htm", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "httpd_check.htm", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "status.asp", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "automac.asp", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "detecWAN.asp", MIME_EXCEPTION_NORESETTIME}, 
	{ "detecWAN2.asp", MIME_EXCEPTION_NORESETTIME},
	{ "WPS_info.asp", MIME_EXCEPTION_NORESETTIME},
	{ "WAN_info.asp", MIME_EXCEPTION_NOAUTH_ALL|MIME_EXCEPTION_NORESETTIME},
	{ "result_of_get_changed_status.asp", MIME_EXCEPTION_NORESETTIME},
	{ "result_of_get_changed_status_QIS.asp", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "result_of_detect_client.asp", MIME_EXCEPTION_NORESETTIME},
	{ "Nologin.asp", MIME_EXCEPTION_NOAUTH_ALL},
	{ "alertImg.gif", MIME_EXCEPTION_NOAUTH_ALL},
	{ "error_page.htm", MIME_EXCEPTION_NOAUTH_ALL},
	{ "jquery.js", MIME_EXCEPTION_NOAUTH_ALL},
	{ "gotoHomePage.htm", MIME_EXCEPTION_NOAUTH_ALL},
	{ "update_appstate.asp", MIME_EXCEPTION_NOAUTH_ALL},
	{ "update_cloudstatus.asp", MIME_EXCEPTION_NOAUTH_ALL},
	{ "get_webdavInfo.asp", MIME_EXCEPTION_NOAUTH_ALL},
	{ "*.gz", MIME_EXCEPTION_NOAUTH_ALL},
	{ "*.tgz", MIME_EXCEPTION_NOAUTH_ALL},
	{ "*.zip", MIME_EXCEPTION_NOAUTH_ALL},
	{ "*.ipk", MIME_EXCEPTION_NOAUTH_ALL},
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
	//websWrite(wp, "    return %d;\n", atoi(nvram_safe_get("samba_running")));
	websWrite(wp, "    return %d;\n", atoi(nvram_safe_get("enable_samba")));
	websWrite(wp, "}\n\n");

	websWrite(wp, "function get_ftp_status(){\n");
	//websWrite(wp, "    return %d;\n", atoi(nvram_safe_get("ftp_running")));
	websWrite(wp, "    return %d;\n", atoi(nvram_safe_get("enable_ftp")));
	websWrite(wp, "}\n\n");

#ifdef RTCONFIG_WEBDAV_PENDING
	websWrite(wp, "function get_webdav_status(){\n");
	//websWrite(wp, "    return %d;\n", atoi(nvram_safe_get("ftp_running")));
	websWrite(wp, "    return %d;\n", atoi(nvram_safe_get("enable_webdav")));
	websWrite(wp, "}\n\n");
#endif

	websWrite(wp, "function get_dms_status(){\n");
	websWrite(wp, "    return %d;\n", pids("ushare"));
	websWrite(wp, "}\n\n");

	websWrite(wp, "function get_share_management_status(protocol){\n");
	websWrite(wp, "    if (protocol == \"cifs\")\n");
	websWrite(wp, "	return %d;\n", atoi(nvram_safe_get("st_samba_mode")));
	websWrite(wp, "    else if (protocol == \"ftp\")\n");
	websWrite(wp, "	return %d;\n", atoi(nvram_safe_get("st_ftp_mode")));
#ifdef RTCONFIG_WEBDAV_PENDING
	websWrite(wp, "    else if (protocol == \"webdav\")\n");
	websWrite(wp, "	return %d;\n", atoi(nvram_safe_get("st_webdav_mode")));
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
	char line[256], devname[32], mpname[32], system_type[10], mount_mode[96];
	int dummy1, dummy2, count = 0;
															       
	if ((procpt = fopen("/proc/mounts", "r")) != NULL)
	while (fgets(line, sizeof(line), procpt))
	{
		if (sscanf(line, "%s %s %s %s %d %d", devname, mpname, system_type, mount_mode, &dummy1, &dummy2) != 6)
			continue;
			
		if (strstr(devname, "/dev/sd"))
			count++;
	}

	if (procpt)
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

	csprintf("disk_port = %s\n", disk_port);

	result = eval("/sbin/ejusb", disk_port);

	if (result != 0){
		show_error_msg("Action9");

		websWrite(wp, "<script>\n");
		websWrite(wp, "safely_remove_disk_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		clean_error_msg();
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

	websWrite(wp, "<script>\n");
	websWrite(wp, "safely_remove_disk_success(\'%s\');\n", error_msg);
	websWrite(wp, "</script>\n");

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

void not_ej_initial_folder_var_file()						// J++
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

int ej_initial_folder_var_file(int eid, webs_t wp, int argc, char **argv)	// J++
{
//	not_ej_initial_folder_var_file();
	return 0;
}

int ej_set_share_mode(int eid, webs_t wp, int argc, char **argv){
	int samba_mode = atoi(nvram_safe_get("st_samba_mode"));
	int ftp_mode = atoi(nvram_safe_get("st_ftp_mode"));
#ifdef RTCONFIG_WEBDAV_PENDING
	int webdav_mode = atoi(nvram_safe_get("st_webdav_mode"));
#endif	
	char *dummyShareway = websGetVar(wp, "dummyShareway", "");
	char *protocol = websGetVar(wp, "protocol", "");
	char *mode = websGetVar(wp, "mode", "");
	int result;

	if (strlen(dummyShareway) > 0)
		nvram_set("dummyShareway", dummyShareway);
	else
		nvram_set("dummyShareway", "0");

	if (strlen(protocol) <= 0){
		show_error_msg("Input1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(mode) <= 0){
		show_error_msg("Input3");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (!strcmp(mode, "share")){
		if (!strcmp(protocol, "cifs")){
			if (samba_mode == 1 || samba_mode == 3)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_samba_mode", "1");	// for test
		}
		else if (!strcmp(protocol, "ftp")){
			if (ftp_mode == 1)
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
			show_error_msg("Input2");

			websWrite(wp, "<script>\n");
			websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
			websWrite(wp, "</script>\n");

			clean_error_msg();
			return -1;
		}
	}
	else if (!strcmp(mode, "account")){
		if (!strcmp(protocol, "cifs")){
			if (samba_mode == 2 || samba_mode == 4)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_samba_mode", "4");
		}
		else if (!strcmp(protocol, "ftp")){
			if (ftp_mode == 2)
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
			show_error_msg("Input2");

			websWrite(wp, "<script>\n");
			websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
			websWrite(wp, "</script>\n");

			clean_error_msg();
			return -1;
		}
	}
	else{
		show_error_msg("Input4");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	nvram_commit();

	not_ej_initial_folder_var_file();	// J++

	if (!strcmp(protocol, "cifs")) {
		result = notify_rc_for_nas("restart_samba");
	}
	else if (!strcmp(protocol, "ftp")) {
		result = notify_rc_for_nas("restart_ftpd");
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if (!strcmp(protocol, "webdav")) {
		result = notify_rc_for_nas("restart_webdav");
	}
#endif
	else {
		show_error_msg("Input2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (result != 0){
		show_error_msg("Action8");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

SET_SHARE_MODE_SUCCESS:
	websWrite(wp, "<script>\n");
	websWrite(wp, "set_share_mode_success();\n");
	websWrite(wp, "</script>\n");

	return 0;
}


int ej_modify_sharedfolder(int eid, webs_t wp, int argc, char **argv){
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char *new_folder = websGetVar(wp, "new_folder", "");
	char mount_path[PATH_MAX];

	if (strlen(pool) <= 0){
		show_error_msg("Input7");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(folder) <= 0){
		show_error_msg("Input9");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(new_folder) <= 0){
		show_error_msg("Input17");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		show_error_msg("System1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (mod_folder(mount_path, folder, new_folder) < 0){
		show_error_msg("Action7");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		show_error_msg("Action7");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	websWrite(wp, "<script>\n");
	websWrite(wp, "modify_sharedfolder_success();\n");
	websWrite(wp, "</script>\n");

	return 0;
}

int ej_delete_sharedfolder(int eid, webs_t wp, int argc, char **argv){
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char mount_path[PATH_MAX];

	if (strlen(pool) <= 0){
		show_error_msg("Input7");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(folder) <= 0){
		show_error_msg("Input9");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		show_error_msg("System1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (del_folder(mount_path, folder) < 0){
		show_error_msg("Action6");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		show_error_msg("Action6");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	websWrite(wp, "<script>\n");
	websWrite(wp, "delete_sharedfolder_success();\n");
	websWrite(wp, "</script>\n");

	return 0;
}

int ej_create_sharedfolder(int eid, webs_t wp, int argc, char **argv){
	char *account = websGetVar(wp, "account", NULL);
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char mount_path[PATH_MAX];

	if (strlen(pool) <= 0){
		show_error_msg("Input7");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(folder) <= 0){
		show_error_msg("Input9");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		fprintf(stderr, "Can't get the mount_path of %s.\n", pool);

		show_error_msg("System1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (add_folder(account, mount_path, folder) < 0){
		show_error_msg("Action5");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	
	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		show_error_msg("Action5");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	websWrite(wp, "<script>\n");
	websWrite(wp, "create_sharedfolder_success();\n");
	websWrite(wp, "</script>\n");

	return 0;
}

int ej_set_AiDisk_status(int eid, webs_t wp, int argc, char **argv){
	char *protocol = websGetVar(wp, "protocol", "");
	char *flag = websGetVar(wp, "flag", "");
	int result = 0;

	if (strlen(protocol) <= 0){
		show_error_msg("Input1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(flag) <= 0){
		show_error_msg("Input18");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
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
			show_error_msg("Input19");

			websWrite(wp, "<script>\n");
			websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
			websWrite(wp, "</script>\n");

			clean_error_msg();
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
			show_error_msg("Input19");

			websWrite(wp, "<script>\n");
			websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
			websWrite(wp, "</script>\n");

			clean_error_msg();
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
			show_error_msg("Input19");

			websWrite(wp, "<script>\n");
			websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
			websWrite(wp, "</script>\n");

			clean_error_msg();
			return -1;
		}
	}
#endif
	else{
		show_error_msg("Input2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (result != 0){
		show_error_msg("Action8");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

SET_AIDISK_STATUS_SUCCESS:
	websWrite(wp, "<script>\n");
	//websWrite(wp, "set_AiDisk_status_success();\n");
	websWrite(wp, "parent.resultOfSwitchAppStatus();\n");
	websWrite(wp, "</script>\n");

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

	if (strlen(account) <= 0){
		show_error_msg("Input5");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(new_account) <= 0 && strlen(new_password) <= 0){
		show_error_msg("Input16");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (mod_account(account, new_account, new_password) < 0){
		show_error_msg("Action4");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if(mod_webdav_account(account, new_account)<0) {
		show_error_msg("Action4");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;	
	}
#endif

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		show_error_msg("Action4");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

#ifdef RTCONFIG_WEBDAV_PENDING
	if(notify_rc_for_nas("restart_webdav") != 0) {
		show_error_msg("Action4");

		websWrite(wp, "<script>\n");
		websWrite(wp, "mod_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
#endif

	websWrite(wp, "<script>\n");
	websWrite(wp, "modify_account_success();\n");
	websWrite(wp, "</script>\n");

	return 0;
}

int ej_delete_account(int eid, webs_t wp, int argc, char **argv){
	char *account = websGetVar(wp, "account", "");

	if (strlen(account) <= 0){
		show_error_msg("Input5");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	not_ej_initial_folder_var_file();	// J++

	if (del_account(account) < 0){
		show_error_msg("Action3");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if(del_webdav_account(account)<0) {
		show_error_msg("Action3");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;	
	}
#endif

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		show_error_msg("Action3");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

#ifdef RTCONFIG_WEBDAV_PENDING
	if(notify_rc_for_nas("restart_webdav") != 0) {
		show_error_msg("Action3");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
#endif

	websWrite(wp, "<script>\n");
	websWrite(wp, "delete_account_success();\n");
	websWrite(wp, "</script>\n");

	return 0;
}

int ej_initial_account(int eid, webs_t wp, int argc, char **argv){
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	char *command;
	int len, result;

	nvram_set("acc_num", "0");
	nvram_set("acc_list", "");
	nvram_commit();

	disks_info = read_disk_data();
	if (disks_info == NULL){
		show_error_msg("System2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "initial_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next)
			if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0){
				len = strlen("rm -f ")+strlen(follow_partition->mount_point)+strlen("/.__*");
				command = (char *)malloc(sizeof(char)*(len+1));
				if (command == NULL){
					show_error_msg("System1");

					websWrite(wp, "<script>\n");
					websWrite(wp, "initial_account_error(\'%s\');\n", error_msg);
					websWrite(wp, "</script>\n");

					clean_error_msg();
					return -1;
				}
				sprintf(command, "rm -f %s/.__*", follow_partition->mount_point);
				command[len] = 0;

				result = system(command);
				free(command);

				initial_folder_list(follow_partition->mount_point);
				initial_all_var_file(follow_partition->mount_point);
			}

	free_disk_data(&disks_info);

#if 0
	if (add_account(nvram_safe_get("http_username"), nvram_safe_get("http_passwd")) < 0)
#else
	// there are spinlock_lock(), spinlock_unlock() in add_account().
	// They would let the buffer of nvram_safe_get() be confused.
	char buf1[64], buf2[64];
	memset(buf1, 0, 64);
	memset(buf2, 0, 64);
	strcpy(buf1, nvram_safe_get("http_username"));
	strcpy(buf2, nvram_safe_get("http_passwd"));

	if(add_account(buf1, buf2) < 0)
#endif
	{
		show_error_msg("Action2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "initial_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if(add_webdav_account(nvram_safe_get("http_username"))<0) {
		show_error_msg("Action2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "init_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;	
	}
#endif

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		show_error_msg("Action2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "initial_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

#ifdef RTCONFIG_WEBDAV_PENDING
	if(notify_rc_for_nas("restart_webdav") != 0) {
		show_error_msg("Action2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "initial_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
#endif
	websWrite(wp, "<script>\n");
	websWrite(wp, "initial_account_success();\n");
	websWrite(wp, "</script>\n");

	return 0;
}

int ej_create_account(int eid, webs_t wp, int argc, char **argv){
	char *account = websGetVar(wp, "account", "");
	char *password = websGetVar(wp, "password", "");

	if (strlen(account) <= 0){
		show_error_msg("Input5");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(password) <= 0){
		show_error_msg("Input14");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	not_ej_initial_folder_var_file();	// J++

	if (add_account(account, password) < 0){
		show_error_msg("Action2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if(add_webdav_account(account) < 0) {
		show_error_msg("Action2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;	
	}
#endif

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		show_error_msg("Action2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

#ifdef RTCONFIG_WEBDAV_PENDING
	if(notify_rc_for_nas("restart_webdav") != 0) {
		show_error_msg("Action2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
#endif

	websWrite(wp, "<script>\n");
	websWrite(wp, "create_account_success();\n");
	websWrite(wp, "</script>\n");
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

	memset(char_user, 0, 64);
	ascii_to_char_safe(char_user, ascii_user, 64);

	if (test_if_exist_account(char_user) != 1){
		show_error_msg("Input6");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (strlen(pool) <= 0){
		show_error_msg("Input7");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		fprintf(stderr, "Can't get the mount_path of %s.\n", pool);

		show_error_msg("System1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (strlen(protocol) <= 0){
		show_error_msg("Input1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strcmp(protocol, "cifs") && strcmp(protocol, "ftp") && strcmp(protocol, "dms") 
#ifdef RTCONFIG_WEBDAV_PENDING
&& strcmp(protocol, "webdav")
#endif
){
		show_error_msg("Input2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (strlen(permission) <= 0){
		show_error_msg("Input12");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	right = atoi(permission);
	if (right < 0 || right > 3){
		show_error_msg("Input13");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (set_permission(char_user, mount_path, folder, protocol, right) < 0){
		show_error_msg("Action1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
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
			show_error_msg("Action1");

			websWrite(wp, "<script>\n");
			websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
			websWrite(wp, "</script>\n");

			clean_error_msg();
			return -1;
		}
	}
	else 
#endif
	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		show_error_msg("Action1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	websWrite(wp, "<script>\n");
	websWrite(wp, "set_account_permission_success();\n");
	websWrite(wp, "</script>\n");

	return 0;
}

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

		sprintf(command, "start_apps_install %s %s", apps_name, apps_flag);
	}
	else if(!strcmp(apps_action, "stop")){
		sprintf(command, "start_apps_stop");
	}
	else if(!strcmp(apps_action, "update")){
		sprintf(command, "start_apps_update");
	}
	else if(!strcmp(apps_action, "upgrade")){
		if(strlen(apps_name) <= 0)
			return 0;

		sprintf(command, "start_apps_upgrade %s", apps_name);
	}
	else if(!strcmp(apps_action, "remove")){
		if(strlen(apps_name) <= 0)
			return 0;

		sprintf(command, "start_apps_remove %s", apps_name);
	}
	else if(!strcmp(apps_action, "enable")){
		if(strlen(apps_name) <= 0 || strlen(apps_flag) <= 0)
			return 0;

		if(strcmp(apps_flag, "yes") && strcmp(apps_flag, "no"))
			return 0;

		sprintf(command, "start_apps_enable %s %s", apps_name, apps_flag);
	}
	else if(!strcmp(apps_action, "switch")){
		if(strlen(apps_name) <= 0 || strlen(apps_flag) <= 0)
			return 0;

		sprintf(command, "start_apps_switch %s %s", apps_name, apps_flag);
	}
	else if(!strcmp(apps_action, "cancel")){
		sprintf(command, "start_apps_cancel");
	}

	if(strlen(command) > 0)
		notify_rc(command);

	return 0;
}

#ifdef RTCONFIG_MEDIA_SERVER
// dms_info: ["Scanning"] or [ "" ]

int ej_dms_info(int eid, webs_t wp, int argc, char **argv){
	char *dms_dbcwd;
	char dms_scanfile[64], dms_status[32];
	FILE *fp;

	dms_dbcwd = nvram_safe_get("dms_dbcwd");

	strcpy(dms_status, "");

	if(strlen(dms_dbcwd)) 
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

#ifdef RTCONFIG_CLOUDSYNC
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

//use for UI to avoid variable 'cloud_sync' JavaScript error, Jieming added at 2012.09.11
int ej_UI_cloud_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/smartsync/.logs/asuswebstorage", "r");
	char line[PATH_MAX], buf[PATH_MAX];
	int line_num;
	char status[16], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX];

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

		if(strstr(line, "STATUS") != NULL){
			strncpy(status, convert_cloudsync_status(line), 16);
		}
		else if(strstr(line, "MOUNT_PATH") != NULL){
			memset(buf, 0, PATH_MAX);
			char_to_ascii(buf, line);
			strcpy(mounted_path, buf);
		}
		else if(strstr(line, "FILENAME") != NULL){
			strcpy(target_obj, line); // support Chinese
			break;
		}
		else if(strstr(line, "ERR_MSG") != NULL){
			strcpy(error_msg, line);
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

int ej_webdavInfo(int eid, webs_t wp, int argc, char **argv) {
	websWrite(wp, "// pktInfo=['PrinterInfo','SSID','NetMask','ProductID','FWVersion','OPMode','MACAddr','Regulation'];\n");
	websWrite(wp, "pktInfo=['','%s',", nvram_safe_get("wl0_ssid"));
	websWrite(wp, "'%s',", nvram_safe_get("lan_netmask"));
	websWrite(wp, "'%s',", nvram_safe_get("productid"));
	websWrite(wp, "'%s.%s',", nvram_safe_get("firmver"), nvram_safe_get("buildno"));
	websWrite(wp, "'%s',", nvram_safe_get("sw_mode"));
	websWrite(wp, "'%s',", nvram_safe_get("et0macaddr"));
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
        websWrite(wp, "'%s'", nvram_safe_get("webdav_https_port"));
        websWrite(wp, "];\n");

	return 0;
}
#endif
#endif

// 2010.09 James. {
int start_autodet(int eid, webs_t wp, int argc, char **argv) {
	notify_rc("start_autodet");
	return 0;
}

#ifdef RTCONFIG_WIRELESSREPEATER
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
dbg("http: get lan_subnet=%x!\n", lan_subnet);
	
	unit = wan_primary_ifunit();
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
dbg("http: get wan_subnet=%x!\n", wan_subnet);
	
	if(lan_subnet != wan_subnet){
dbg("http: WAN's subnet and LAN's are the same.!\n");
		websWrite(wp, "0");
		return 0;
	}
	
	if(lan_subnet >= MAX_SUBNET)
		new_lan_ip_num = MIN_LAN_IP;
	else
		new_lan_ip_num = lan_ip_num+(~lan_mask_num)+1;
	
	new_dhcp_start_num = new_lan_ip_num+1;
	new_dhcp_end_num = new_lan_ip_num+(~inet_network(lan_netmask_t))-2;
dbg("%u, %u, %u.\n", new_lan_ip_num, new_dhcp_start_num, new_dhcp_end_num);
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
dbg("%s, %s, %s.\n", new_lan_ip_str, new_dhcp_start_str, new_dhcp_end_str);
	
	nvram_set(strcat_r(prefix_lan, "ipaddr", tmp_lan), new_lan_ip_str);
	nvram_set(strcat_r(prefix_lan, "ipaddr_rt", tmp_lan), new_lan_ip_str); // Sync to lan_ipaddr_rt, added by jerry5.
	nvram_set("dhcp_start", new_dhcp_start_str);
	nvram_set("dhcp_end", new_dhcp_end_str);
	
	websWrite(wp, "1");
	
	nvram_commit();
	
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
  char *p;
  char *ifname;
  char ifname_desc[12], ifname_desc2[12];
  char comma;
  int ret=0;

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
				if (!netdev_calc(ifname, ifname_desc, &rx, &tx, ifname_desc2, &rx2, &tx2)) continue;


loopagain: 
				ret += websWrite(wp, "%c'%s':{rx:0x%lx,tx:0x%lx}", comma, ifname_desc, rx, tx);
				comma = ',';
				ret += websWrite(wp, "\n");

				if(strlen(ifname_desc2)) {
					strcpy(ifname_desc, ifname_desc2);
					rx = rx2;
					tx = tx2;
					strcpy(ifname_desc2, "");
					goto loopagain;
				}
			}

		fclose(fp);
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
		curr = atoi(nvram_get(ref1));
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

		curr = atoi(nvram_get(tmpstr));
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

		for (i=0;i<atoi(nvram_get("acc_num"));i++)
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
 
	for (i=0;i<atoi(nvram_get(refnum));i++)
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
	{ "IP_dhcpLeaseInfo", ej_IP_dhcpLeaseInfo},
//tomato qosvvvvvvvvvvv 2010.08 Viz
	{ "qrate", ej_qos_packet},
	{ "ctdump", ej_ctdump},
	{ "netdev", ej_netdev},

	{ "iptraffic", ej_iptraffic},
	{ "iptmon", ej_iptmon},
	{ "ipt_bandwidth", ej_ipt_bandwidth},

	{ "bandwidth", ej_bandwidth},
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
	{ "convert_asus_variables", convert_asus_variables},
	{ "asus_nvram_commit", asus_nvram_commit},
	{ "notify_services", ej_notify_services},
	{ "wanstate", wanstate_hook},
	{ "ajax_wanstate", ajax_wanstate_hook},
#ifdef RTCONFIG_DSL
	{ "wanlink_dsl", wanlink_hook_dsl},
#endif
	{ "wanlink", wanlink_hook},
	{ "secondary_wanlink", secondary_wanlink_hook},
	{ "wan_action", wan_action_hook},
	{ "get_wan_unit", get_wan_unit_hook},
	{ "check_hwnat", check_hwnat}, 
	{ "get_parameter", ej_get_parameter},
	{ "login_state_hook", login_state_hook},
#ifdef RTCONFIG_FANCTRL
	{ "get_fanctrl_info", get_fanctrl_info},
#endif
	{ "dumpleases", ej_dumpleases},
	{ "dhcp_leases", ej_dhcp_leases},
	{ "get_arp_table", ej_get_arp_table},
	{ "get_client_detail_info", ej_get_client_detail_info},//2011.03 Yau add for new networkmap
	{ "get_static_client", ej_get_static_client},
	{ "get_changed_status", ej_get_changed_status},
	{ "shown_time", ej_shown_time},
	{ "shown_language_option", ej_shown_language_option},
	{ "shown_language_css", ej_shown_language_css},
#ifndef RTCONFIG_RALINK
#ifdef RTCONFIG_WIRELESSWAN
	{ "sitesurvey", ej_SiteSurvey},
#endif
#endif
#ifdef RTCONFIG_WIRELESSREPEATER
	{ "get_ap_info", ej_get_ap_info},
#endif
	{ "ddns_info", ej_ddnsinfo},
#ifdef RTCONFIG_USB
	{ "disk_pool_mapping_info", ej_disk_pool_mapping_info},
	{ "available_disk_names_and_sizes", ej_available_disk_names_and_sizes},
	{ "get_printer_info", ej_get_printer_info},
	{ "get_AiDisk_status", ej_get_AiDisk_status},
	{ "set_AiDisk_status", ej_set_AiDisk_status},
	{ "get_all_accounts", ej_get_all_accounts},
	{ "safely_remove_disk", ej_safely_remove_disk},
	{ "get_permissions_of_account", ej_get_permissions_of_account},
	{ "set_account_permission", ej_set_account_permission},
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
	{ "initial_folder_var_file", ej_initial_folder_var_file},	/* J++ */
	{ "apps_info", ej_apps_info},
	{ "apps_action", ej_apps_action},
#ifdef RTCONFIG_MEDIA_SERVER
	{ "dms_info", ej_dms_info},
#endif
#ifdef RTCONFIG_CLOUDSYNC
	{ "cloud_status", ej_cloud_status},
	{ "UI_cloud_status", ej_UI_cloud_status},
	{ "getWebdavInfo", ej_webdavInfo},
#endif
#endif

	{ "start_autodet", start_autodet},
#ifdef RTCONFIG_WIRELESSREPEATER
	{ "start_wlcscan", start_wlcscan},
#endif
	{ "setting_lan", setting_lan},

	// system or solution dependant part start from here
	{ "wl_auth_list", ej_wl_auth_list},
#ifdef CONFIG_BCMWL5
	{ "wl_control_channel", ej_wl_control_channel},
#endif
#ifdef RTCONFIG_DSL
	{ "get_isp_list", ej_get_isp_list},
	{ "get_DSL_WAN_list", ej_get_DSL_WAN_list},
#endif
	{ "wl_scan_2g", ej_wl_scan_2g},
	{ "wl_scan_5g", ej_wl_scan_5g},
	{ "channel_list_2g", ej_wl_channel_list_2g},
	{ "channel_list_5g", ej_wl_channel_list_5g},
#ifdef RTCONFIG_PROXYSTA
	{ "wlc_psta_state", ej_wl_auth_psta},
#endif	
	{ "get_default_reboot_time", ej_get_default_reboot_time},
	{ "sysinfo",  ej_show_sysinfo},
#ifdef RTCONFIG_OPENVPN
	{ "vpn_server_get_parameter", ej_vpn_server_get_parameter},
	{ "vpn_client_get_parameter", ej_vpn_client_get_parameter},
#endif
	{ "nvram_clean_get", ej_nvram_clean_get},
	{ "radio_status", ej_radio_status},
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

#ifdef DLM
void
umount_disc_parts(int usb_port)
{
	eval("ejusb");
}
#endif

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

	/* dump nat table including VSERVER and VUPNP chains */
	_eval(nat_argv, ">/tmp/vserver.log", 10, NULL);

	ret += websWrite(wp,
#ifdef NATSRC_SUPPORT
		"Source          "
#endif
		"Destination     Proto. Port range  Redirect to     Local port\n");
	/*	 255.255.255.255 other  65535:65535 255.255.255.255 65535:65535 */

	fp = fopen("/tmp/vserver.log", "r");
	if (fp == NULL)
		return ret;

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		_dprintf("HTTPD: %s\n", line);

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
			"%-15s %-6s %-11s %-15s %-11s\n",
#ifdef NATSRC_SUPPORT
			src,
#endif
			dst, proto, range, host, port ? : range);
	}
	fclose(fp);
	unlink("/tmp/vserver.log");

	return ret;
}

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
