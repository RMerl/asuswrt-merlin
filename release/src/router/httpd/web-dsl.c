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
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 */

#ifdef WEBS
#include <webs.h>
#include <uemf.h>
#include <ej.h>
#else /* !WEBS */
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
#include <assert.h>
#include <httpd.h>
#endif /* WEBS */
#include <typedefs.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <shutils.h>
#ifdef RTCONFIG_RALINK
#include <ralink.h>
#include <iwlib.h>
#include <stapriv.h>
#include <ethutils.h>
#endif
#include <shared.h>
#include <sys/mman.h>
#ifndef O_BINARY
#define O_BINARY 	0
#endif
#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif

#define wan_prefix(unit, prefix)	snprintf(prefix, sizeof(prefix), "wan%d_", unit)
//static char * rfctime(const time_t *timep);
//static char * reltime(unsigned int seconds);
void reltime(unsigned int seconds, char *buf);

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/klog.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <net/if_arp.h>

#include <dirent.h>

#define MAX_LINE_SIZE 512
// copy from web.c
static int dump_file(webs_t wp, char *filename)
{
	FILE *fp;
	char buf[MAX_LINE_SIZE];
	int ret=0;

	fp = fopen(filename, "r");
		
	if (fp==NULL) 
	{
		ret+=websWrite(wp, "");
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




/*
 * retreive and convert dsls values for specified dsl_unit 
 * Example: 
 * <% dsl_get_parameter(); %> for coping wan[n]_ to wan_
 */
int ej_dsl_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit, subunit;

	unit = nvram_get_int("dsl_unit");
	subunit = nvram_get_int("dsl_subunit");

	// handle generate cases first
	(void)copy_index_to_unindex("dsl_", unit, subunit);

	return (websWrite(wp,""));
}

int wanlink_hook_dsl(int eid, webs_t wp, int argc, char_t **argv){
// DSLTODO : dummy code
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

	status = 1;

	type = nvram_safe_get(nvram_safe_get("dsl_proto"));

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

/* TODO: implement WANX */
//	if (strcmp(wan_proto, "pppoe") == 0 ||
//	    strcmp(wan_proto, "pptp") == 0 ||
//	    strcmp(wan_proto, "l2tp") == 0) {
//		int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
//		xtype = (dhcpenable == 0) ? "static" :
//			(strcmp(wan_proto, "pppoe") == 0 && nvram_match(strcat_r(prefix, "vpndhcp", tmp), "0")) ? "" : /* zeroconf */
//			"dhcp";
//		xip = nvram_safe_get(strcat_r(prefix, "xipaddr", tmp));
//		xnetmask = nvram_safe_get(strcat_r(prefix, "xnetmask", tmp));
//		xgateway = nvram_safe_get(strcat_r(prefix, "xgateway", tmp));
//		xlease = nvram_get_int(strcat_r(prefix, "xlease", tmp));
//		if (xlease > 0)
//			xexpires = nvram_get_int(strcat_r(prefix, "xexpires", tmp)) - uptime();
//	}

	websWrite(wp, "function wanlink_xtype() { return '%s';}\n", xtype);
	websWrite(wp, "function wanlink_xipaddr() { return '%s';}\n", xip);
	websWrite(wp, "function wanlink_xnetmask() { return '%s';}\n", xnetmask);
	websWrite(wp, "function wanlink_xgateway() { return '%s';}\n", xgateway);
	websWrite(wp, "function wanlink_xdns() { return '%s';}\n", nvram_safe_get(strcat_r(prefix, "xdns", tmp)));
	websWrite(wp, "function wanlink_xlease() { return %d;}\n", xlease);
	websWrite(wp, "function wanlink_xexpires() { return %d;}\n", xexpires);

	return 0;
}

void
do_adsllog_cgi(char *path, FILE *stream)
{
	dump_file(stream, "/tmp/adsl/adsllog.log-1");
	dump_file(stream, "/tmp/adsl/adsllog.log");
	fputs("\r\n", stream); /* terminator */
	fputs("\r\n", stream); /* terminator */
}

int ej_get_isp_list(int eid, webs_t wp, int argc, char_t **argv){
	char *name;
	FILE* fpIsp;
	char bufIsp[512];

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	fpIsp = fopen(name,"r");
	if (fpIsp != NULL)
	{
	  // read out UTF-8 3 bytes header
	        fread(bufIsp,3,1,fpIsp);
		while(!feof(fpIsp))
		{
			char* ret_fgets;
			ret_fgets = fgets(bufIsp,sizeof(bufIsp),fpIsp);
			if (ret_fgets != NULL)
			{
				websWrite(wp, bufIsp);
				websWrite(wp, "\n");
			}
		}
		fclose(fpIsp);
	}
	return 0;
}

/*******************************************************************
* NAME: ej_get_isp_dhcp_opt_list
* AUTHOR: Andy Chiu
* CREATE DATE: 2016/01/20
* DESCRIPTION: return the ISP_DHCP_opt_List.txt
* INPUT:
* OUTPUT:
* RETURN:
* NOTE:
*******************************************************************/
int ej_get_isp_dhcp_opt_list(int eid, webs_t wp, int argc, char_t **argv){
	char *name;
	FILE* fpIsp;
	char bufIsp[512];

	fpIsp = fopen("/www/ISP_DHCP_Opt_List.txt","r");
	if (fpIsp != NULL)
	{
	  // read out UTF-8 3 bytes header
	        //fread(bufIsp,3,1,fpIsp);
		while(!feof(fpIsp))
		{
			char* ret_fgets;
			ret_fgets = fgets(bufIsp,sizeof(bufIsp),fpIsp);
			if (ret_fgets != NULL)
			{
				websWrite(wp, bufIsp);
				websWrite(wp, "\n");
			}
		}
		fclose(fpIsp);
	}
	return 0;
}

int ej_get_DSL_WAN_list(int eid, webs_t wp, int argc, char_t **argv){
	char buf[MAX_LINE_SIZE];
	char buf2[MAX_LINE_SIZE];
	char prefix[]="dslXXXXXX_", tmp[100];
	int unit;
	int j;
	int firstItem;

	if(nvram_match("dsltmp_transmode", "atm")) {
		char *display_items[] = {"dsl_enable", "dsl_vpi", "dsl_vci","dsl_proto", "dsl_encap", 
			"dsl_svc_cat", "dsl_pcr","dsl_scr","dsl_mbs",
#ifdef RTCONFIG_DSL_TCLINUX
			"dsl_dot1q", "dsl_vid", "dsl_dot1p",
#endif
			NULL};

		for ( unit = 0; unit<8; unit++ ) {
			snprintf(prefix, sizeof(prefix), "dsl%d_", unit);

			firstItem = 1;
			websWrite(wp, "[");
			for ( j = 0; display_items[j] != NULL; j++ ) {
				if(firstItem == 1) {
					firstItem = 0;
				}
				else {
					websWrite(wp, ", ");
				}
				strcpy(buf,nvram_safe_get(strcat_r(prefix, &display_items[j][4], tmp)));
				if (strcmp(buf,"")==0) {
					strcpy(buf2,"\"0\"");
				}
				else {
					sprintf(buf2,"\"%s\"",buf);
				}
				websWrite(wp, "%s", buf2);
			}
			if (unit != 7) {
				websWrite(wp, "], ");
			}
			else {
				websWrite(wp, "]");
			}
		}
	}
	else {	//ptm
		char *display_items[] = {"dsl_enable", "dsl_proto", "dsl_dot1q", "dsl_vid", "dsl_dot1p", NULL};

		for ( unit = 0; unit<8; unit++ ) {
			if(unit)
				snprintf(prefix, sizeof(prefix), "dsl8.%d_", unit);
			else
				snprintf(prefix, sizeof(prefix), "dsl8_");

			firstItem = 1;
			websWrite(wp, "[");
			for ( j = 0; display_items[j] != NULL; j++ ) {
				if(firstItem == 1) {
					firstItem = 0;
				}
				else {
					websWrite(wp, ", ");
				}
				strcpy(buf,nvram_safe_get(strcat_r(prefix, &display_items[j][4], tmp)));
				if (strcmp(buf,"")==0) {
					strcpy(buf2,"\"0\"");
				}
				else {
					sprintf(buf2,"\"%s\"",buf);
				}
				websWrite(wp, "%s", buf2);
			}
			if (unit != 7) {
				websWrite(wp, "], ");
			}
			else {
				websWrite(wp, "]");
			}
		}
	}

	return 0;
}

int update_dsl_iptv_variables()
{
	char *nv, *nvp, *b;
	char *vpi, *vci, *proto, *encap, *vid;
	int unit = 0;
	char prefix[] = "dslxxx_xxxxxxxx";
	char tmp[64];

	_dprintf("dsltmp_cfg_iptv_pvclist=%s\n", nvram_safe_get("dsltmp_cfg_iptv_pvclist"));

	nvp = nv = strdup(nvram_safe_get("dsltmp_cfg_iptv_pvclist"));
	while(nv && (b = strsep(&nvp, "<")) != NULL){
		if((vstrsep(b, ">", &vpi, &vci, &proto, &encap, &vid) != 5))
			continue;

		unit++;

		_dprintf("vpi=[%s], vci=[%s], proto=[%s], encap=[%s], vid=[%s]\n", vpi, vci, proto, encap, vid);

		if(nvram_match("dsltmp_transmode", "atm")) {
			snprintf(prefix, sizeof(prefix), "dsl%d_", unit);

			nvram_set(strcat_r(prefix, "vpi", tmp), vpi);
			nvram_set(strcat_r(prefix, "vci", tmp), vci);
			nvram_set(strcat_r(prefix, "encap", tmp), encap);
		}
		else {	//ptm
			snprintf(prefix, sizeof(prefix), "dsl8.%d_", unit);

			nvram_set("dslx_transmode", "ptm");
			notify_rc("restart_dsl_setting");
		}

		nvram_set(strcat_r(prefix, "enable", tmp), "1");

		if(!strcmp(proto, "0")) {
			nvram_set(strcat_r(prefix, "proto", tmp), "pppoe");
		}
		else if(!strcmp(proto, "1")) {
			nvram_set(strcat_r(prefix, "proto", tmp), "pppoa");
		}
		else if(!strcmp(proto, "2")) {
			nvram_set(strcat_r(prefix, "proto", tmp), "dhcp");
		}
		else if(!strcmp(proto, "3")) {
			nvram_set(strcat_r(prefix, "proto", tmp), "bridge");
		}
		else if(!strcmp(proto, "4")) {
			nvram_set(strcat_r(prefix, "proto", tmp), "ipoa");
		}
		else {
			nvram_set(strcat_r(prefix, "proto", tmp), "bridge");
		}

		if(strlen(vid)) {
			nvram_set(strcat_r(prefix, "dot1q", tmp), "1");
			nvram_set(strcat_r(prefix, "vid", tmp), vid);
		}
	}
	free(nv);

	if(unit) {
		if(nvram_match("switch_stb_x", "1")
			&& (nvram_get_int("dslx_config_num") > 1)
		) {
			nvram_set("dsltmp_qis_reboot", "0");
		}
		else {
			nvram_set("switch_stb_x", "1");
			nvram_set("wans_lanport", "4");
			nvram_set("dsltmp_qis_reboot", "1");
		}
	}
	else {
		if(nvram_match("switch_stb_x", "0"))  {
			nvram_set("dsltmp_qis_reboot", "0");
		}
		else {
			nvram_set("switch_stb_x", "0");
			nvram_set("dsl1_enable", "0");
			nvram_set("dsl2_enable", "0");
			nvram_set("dsl3_enable", "0");
			nvram_set("dsl4_enable", "0");
			nvram_set("dsl5_enable", "0");
			nvram_set("dsl6_enable", "0");
			nvram_set("dsl7_enable", "0");
#ifdef RTCONFIG_VDSL
			nvram_set("dsl8.1_enable", "0");
			nvram_set("dsl8.2_enable", "0");
			nvram_set("dsl8.3_enable", "0");
			nvram_set("dsl8.4_enable", "0");
			nvram_set("dsl8.5_enable", "0");
			nvram_set("dsl8.6_enable", "0");
			nvram_set("dsl8.7_enable", "0");
#endif
			nvram_set("dsltmp_qis_reboot", "1");
		}
	}

	return 0;
}
