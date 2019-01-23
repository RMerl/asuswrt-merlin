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
 * Network services
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <dirent.h>
#include <linux/sockios.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <wlutils.h>
#include <rc.h>
#include <bcmutils.h>
#include <bcmparams.h>
#include <net/route.h>
#include <stdarg.h>

#include <linux/types.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>

#ifdef RTCONFIG_USB
#include <disk_io_tools.h>
#endif

#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif

#ifdef RTCONFIG_QCA
#include <qca.h>
#endif


#define	MAX_MAC_NUM	16
static int mac_num;
static char mac_clone[MAX_MAC_NUM][18];

void convert_wan_nvram(char *prefix, int unit);

#if defined(DSL_N55U) || defined(DSL_N55U_B)
int classATargetTable[]={
	1,
	14,
	27,
	36,
	39,
	42,
	49,
	58,
	59,
	60,
	61,
	101,
	103,
	106,
	110,
	111,
	112,
	113,
	114,
	115,
	116,
	117,
	118,
	119,
	120,
	121,
	122,
	123,
	124,
	125,
	126,
	175,
	180,
	182,
	183,
	202,
	203,
	210,
	211,
	218,
	219,
	220,
	221,
	222,
	223
};

int isTargetArea()
{
	int i;
	char *ip = get_wanip();
	int prefixA = inet_network(ip) >> 24;
_dprintf("==>%s ip: %s, prefix: %d\n", __func__, ip, prefixA);
	for(i=0; i<sizeof(classATargetTable); i++) {
		if( prefixA == classATargetTable[i] )
			return 1;
	}
	return 0;
}
#endif

#define WAN0_ROUTE_TABLE 100
#define WAN1_ROUTE_TABLE 200

#define TEMP_ROUTE_FILE "/tmp/route_main"

int copy_routes(int table){
	char cmd[2048];
	char *route_buf, *follow_info, line[1024];
	int len;

	if(table <= 0){
		snprintf(cmd, 2048, "ip route flush table %d", WAN0_ROUTE_TABLE);
		system(cmd);

		snprintf(cmd, 2048, "ip route flush table %d", WAN1_ROUTE_TABLE);
		system(cmd);

		return 0;
	}

	snprintf(cmd, 2048, "ip route list table main > %s", TEMP_ROUTE_FILE);
	system(cmd);

	route_buf = read_whole_file(TEMP_ROUTE_FILE);
	if(route_buf == NULL || strlen(route_buf) <= 0)
		return -1;

	follow_info = route_buf;
	while(get_line_from_buffer(follow_info, line, 1024) != NULL) {
		follow_info += strlen(line);

		if(strncmp(line, "default", 7) == 0 || isspace(line[0]))
			continue;

		len = strlen(line);
		line[len-2] = 0;

		snprintf(cmd, 2048, "ip route add table %d %s", table, line);
		system(cmd);
	}

	free(route_buf);

	return 0;
}

/*
 * the priority of routing rules:
 * pref 20100: user's routes.
 * pref 20200: from wan's ip, from wan's DNS.
 * pref 20300: ISP's routes.
 * pref 20400: to wan's gateway, to wan's DNS.
 */
int add_multi_routes(void)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char wan_proto[32];
	char wan_ip[32], wan_gate[32];
	char cmd[2048];
	char wan_multi_if[WAN_UNIT_MAX][32], wan_multi_gate[WAN_UNIT_MAX][32];
#ifdef RTCONFIG_DUALWAN
	int gate_num = 0, wan_weight, table;
	char cmd2[2048], *ptr;
	char wan_dns[1024];
	char wan_multi_ip[WAN_UNIT_MAX][32];
	char word[64], *next;
	char *nv, *nvp, *b;
#endif
	int debug = nvram_get_int("routes_debug");
	int lock;
	lock = file_lock("mt_routes");

	// clean the rules of routing table and re-build them then.
	system("ip rule flush");
	system("ip rule add from all lookup main pref 32766");
	system("ip rule add from all lookup default pref 32767");

	// clean multi route tables and re-build them then.
	copy_routes(0);

	memset(wan_multi_if, 0, sizeof(char)*WAN_UNIT_MAX*32);
	memset(wan_multi_gate, 0, sizeof(char)*WAN_UNIT_MAX*32);
#ifdef RTCONFIG_DUALWAN
	memset(wan_multi_ip, 0, sizeof(char)*WAN_UNIT_MAX*32);
#endif

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){ // Multipath
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		strncpy(wan_multi_if[unit], get_wan_ifname(unit), 32);
		strncpy(wan_ip, nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)), 32);
		strncpy(wan_gate, nvram_safe_get(strcat_r(prefix, "gateway", tmp)), 32);

		// when wan_down().
		if(!is_wan_connect(unit))
			continue;

		if(strlen(wan_gate) <= 0 || !strcmp(wan_gate, "0.0.0.0"))
			continue;

		if(strlen(wan_ip) <= 0 || !strcmp(wan_ip, "0.0.0.0"))
			continue;

#ifdef RTCONFIG_DUALWAN
		++gate_num;

		strcpy(wan_multi_ip[unit], wan_ip);
#endif
		strcpy(wan_multi_gate[unit], wan_gate);
	}

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){ // Multipath
		if(unit != wan_primary_ifunit()
#ifdef RTCONFIG_DUALWAN
				&& !nvram_match("wans_mode", "lb")
#endif
				)
			continue;

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		strncpy(wan_proto, nvram_safe_get(strcat_r(prefix, "proto", tmp)), 32);

		// when wan_down().
		if(!is_wan_connect(unit))
			continue;

#ifdef RTCONFIG_DUALWAN
		if(strlen(wan_multi_gate[unit]) <= 0 || !strcmp(wan_multi_gate[unit], "0.0.0.0"))
			continue;

		if(nvram_match("wans_mode", "lb") && gate_num > 1){
			if(strlen(wan_multi_ip[unit]) <= 0 || !strcmp(wan_multi_ip[unit], "0.0.0.0"))
				continue;

			if(unit == WAN_UNIT_SECOND)
				table = WAN1_ROUTE_TABLE;
			else
				table = WAN0_ROUTE_TABLE;

			// set the rules of wan[X]'s ip and gateway for multi routing tables.
			snprintf(cmd2, 2048, "ip rule del pref 20200 from %s table %d 2>/dev/null", wan_multi_ip[unit], table);
if(debug) printf("test 1. cmd2=%s.\n", cmd2);
			system(cmd2);

			snprintf(cmd2, 2048, "ip rule add pref 20200 from %s table %d", wan_multi_ip[unit], table);
if(debug) printf("test 2. cmd2=%s.\n", cmd2);
			system(cmd2);

			snprintf(cmd2, 2048, "ip rule del pref 20400 to %s table %d 2>/dev/null", wan_multi_gate[unit], table);
if(debug) printf("test 3. cmd2=%s.\n", cmd2);
			system(cmd2);

			snprintf(cmd2, 2048, "ip rule add pref 20400 to %s table %d", wan_multi_gate[unit], table);
if(debug) printf("test 4. cmd2=%s.\n", cmd2);
			system(cmd2);

			// set the routes for multi routing tables.
			copy_routes(table);

			snprintf(cmd2, 2048, "ip route replace %s dev %s proto kernel table %d", wan_multi_gate[unit], wan_multi_if[unit], table);
if(debug) printf("test 5. cmd2=%s.\n", cmd2);
			system(cmd2);

			snprintf(cmd2, 2048, "ip route replace default via %s dev %s table %d", wan_multi_gate[unit], wan_multi_if[unit], table);
if(debug) printf("test 6. cmd2=%s.\n", cmd2);
			system(cmd2);

			if(!strcmp(wan_proto, "pptp") || !strcmp(wan_proto, "l2tp")){
				snprintf(cmd2, 2048, "ip route del %s dev %s table %d 2>/dev/null", wan_multi_gate[unit], wan_multi_if[unit], table);
if(debug) printf("test 7. cmd2=%s.\n", cmd2);
				system(cmd2);
			}

			// set the static routing rules.
			if(nvram_match("wans_routing_enable", "1")){
				char *rfrom, *rto, *rtable_str;
				int rtable;

				nvp = nv = strdup(nvram_safe_get("wans_routing_rulelist"));
				while(nv && (b = strsep(&nvp, "<")) != NULL){
					if((vstrsep(b, ">", &rfrom, &rto, &rtable_str) != 3))
						continue;

					if(!strcmp(rfrom, "all") && !strcmp(rfrom, rto))
						continue;

					rtable = atoi(rtable_str);

					if(rtable == WAN_UNIT_FIRST || rtable == WAN0_ROUTE_TABLE)
						rtable = WAN0_ROUTE_TABLE;
					else if(rtable == WAN_UNIT_SECOND || rtable == WAN1_ROUTE_TABLE)
						rtable = WAN1_ROUTE_TABLE;
					else // incorrect table.
						continue;

					if(rtable == table){
						snprintf(cmd2, 2048, "ip rule del pref 20100 from %s to %s table %d 2>/dev/null", rfrom, rto, rtable);
if(debug) printf("test 8. cmd2=%s.\n", cmd2);
						system(cmd2);

						snprintf(cmd2, 2048, "ip rule add pref 20100 from %s to %s table %d", rfrom, rto, rtable);
if(debug) printf("test 9. cmd2=%s.\n", cmd2);
						system(cmd2);
					}
	 			}
				free(nv);
			}
		}
		else
		{
			snprintf(cmd, 2048, "ip route replace %s dev %s proto kernel", wan_multi_gate[unit], wan_multi_if[unit]);
if(debug) printf("test 10. cmd=%s.\n", cmd);
			system(cmd);

			// set the default gateway.
			snprintf(cmd, 2048, "ip route replace default via %s dev %s", wan_multi_gate[unit], wan_multi_if[unit]);
if(debug) printf("test 11. cmd=%s.\n", cmd);
			system(cmd);

			if(!strcmp(wan_proto, "pptp") || !strcmp(wan_proto, "l2tp")){
				snprintf(cmd, 2048, "ip route del %s dev %s 2>/dev/null", wan_multi_gate[unit], wan_multi_if[unit]);
if(debug) printf("test 12. cmd=%s.\n", cmd);
				system(cmd);
			}
		}

		if(!nvram_match("wans_mode", "lb") || gate_num <= 1)
			break;
#else
		snprintf(cmd, 2048, "ip route replace %s dev %s proto kernel", wan_multi_gate[unit], wan_multi_if[unit]);
if(debug) printf("test 10. cmd=%s.\n", cmd);
		system(cmd);

		// set the default gateway.
		snprintf(cmd, 2048, "ip route replace default via %s dev %s", wan_multi_gate[unit], wan_multi_if[unit]);
if(debug) printf("test 11. cmd=%s.\n", cmd);
		system(cmd);

		if(!strcmp(wan_proto, "pptp") || !strcmp(wan_proto, "l2tp")){
			snprintf(cmd, 2048, "ip route del %s dev %s 2>/dev/null", wan_multi_gate[unit], wan_multi_if[unit]);
if(debug) printf("test 12. cmd=%s.\n", cmd);
			system(cmd);
		}
#endif // RTCONFIG_DUALWAN
	}

#ifdef RTCONFIG_DUALWAN
	// set the multi default gateway.
	if(nvram_match("wans_mode", "lb") && gate_num > 1){
		memset(cmd, 0, 2048);
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);

			if(unit == WAN_UNIT_SECOND)
				table = WAN1_ROUTE_TABLE;
			else
				table = WAN0_ROUTE_TABLE;

			// move the gateway via VPN+DHCP from the main routing table to the correct one.
			strcpy(wan_gate, nvram_safe_get(strcat_r(prefix, "xgateway", tmp)));
			if(strlen(wan_gate) > 0 && strcmp(wan_gate, "0.0.0.0") && strcmp(wan_gate, wan_multi_gate[unit])){
				snprintf(cmd2, 2048, "ip route del default via %s dev %s 2>/dev/null", wan_gate, get_wanx_ifname(unit));
if(debug) printf("test 13. cmd2=%s.\n", cmd2);
				system(cmd2);

				snprintf(cmd2, 2048, "ip route replace default via %s dev %s table %d metric 1", wan_gate, get_wanx_ifname(unit), table);
if(debug) printf("test 14. cmd2=%s.\n", cmd2);
				system(cmd2);
			}

			// set the routing rules of DNS via VPN+DHCP.
			strncpy(wan_dns, nvram_safe_get(strcat_r(prefix, "xdns", tmp)), 32);
			if(strlen(wan_dns) > 0){
				// set the rules for the DNS servers.
				foreach(word, wan_dns, next) {
					snprintf(cmd2, 2048, "ip rule del pref 20200 from %s table %d 2>/dev/null", word, table);
if(debug) printf("test 15. cmd2=%s.\n", cmd2);
					system(cmd2);
					snprintf(cmd2, 2048, "ip rule add pref 20200 from %s table %d", word, table);
if(debug) printf("test 16. cmd2=%s.\n", cmd2);
					system(cmd2);

					snprintf(cmd2, 2048, "ip rule del pref 20400 to %s table %d 2>/dev/null", word, table);
if(debug) printf("test 17. cmd2=%s.\n", cmd2);
					system(cmd2);
					snprintf(cmd2, 2048, "ip rule add pref 20400 to %s table %d", word, table);
if(debug) printf("test 18. cmd2=%s.\n", cmd2);
					system(cmd2);
				}
			}

			// set the routing rules of DNS.
			strncpy(wan_dns, nvram_safe_get(strcat_r(prefix, "dns", tmp)), 32);
			if(strlen(wan_dns) > 0){
				// set the rules for the DNS servers.
				foreach(word, wan_dns, next) {
					snprintf(cmd2, 2048, "ip rule del pref 20200 from %s table %d 2>/dev/null", word, table);
if(debug) printf("test 19. cmd2=%s.\n", cmd2);
					system(cmd2);
					snprintf(cmd2, 2048, "ip rule add pref 20200 from %s table %d", word, table);
if(debug) printf("test 20. cmd2=%s.\n", cmd2);
					system(cmd2);

					snprintf(cmd2, 2048, "ip rule del pref 20400 to %s table %d 2>/dev/null", word, table);
if(debug) printf("test 21. cmd2=%s.\n", cmd2);
					system(cmd2);
					snprintf(cmd2, 2048, "ip rule add pref 20400 to %s table %d", word, table);
if(debug) printf("test 22. cmd2=%s.\n", cmd2);
					system(cmd2);
				}
			}

			// set the default gateways with weights in the main routing table.
			nvp = nv = strdup(nvram_safe_get("wans_lb_ratio"));
			int i = 0;
			b = NULL;
			while(nv && (b = strsep(&nvp, ":")) != NULL){
				if(i == unit)
					break;

				++i;
			}

			if(!b)
				continue;

			wan_weight = atoi(b);
			if(wan_weight > 0 && strlen(wan_multi_gate[unit]) > 0){
				if(strlen(cmd) == 0)
					strcpy(cmd, "ip route replace default");

				ptr = cmd+strlen(cmd);
				sprintf(ptr, " nexthop via %s dev %s weight %d", wan_multi_gate[unit], wan_multi_if[unit], wan_weight);
			}

#if ! defined(CONFIG_BCMWL5)
			{
				/* set same mark as iptables balance */
				snprintf(cmd2, sizeof(cmd2), "ip rule add fwmark 0x%x/0x%x table %d", IPTABLES_MARK_LB_SET(unit), IPTABLES_MARK_LB_MASK, table);
				system(cmd2);
			}
#endif	/* ! CONFIG_BCMWL5 */

			if(nv)
				free(nv);
		}

		if(strlen(cmd) > 0){
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				snprintf(cmd2, 2048, "ip route replace %s dev %s", wan_multi_gate[unit], wan_multi_if[unit]);
if(debug) printf("test 23. cmd2=%s.\n", cmd2);
				system(cmd2);
			}

if(debug) printf("test 24. cmd=%s.\n", cmd);
			system(cmd);

			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				snprintf(prefix, sizeof(prefix), "wan%d_", unit);
				strncpy(wan_proto, nvram_safe_get(strcat_r(prefix, "proto", tmp)), 32);
				if(!strcmp(wan_proto, "pptp") || !strcmp(wan_proto, "l2tp")){
					snprintf(cmd2, 2048, "ip route del %s dev %s 2>/dev/null", wan_multi_gate[unit], wan_multi_if[unit]);
if(debug) printf("test 25. cmd2=%s.\n", cmd2);
					system(cmd2);
				}
			}
		}
	}
#endif

if(debug) printf("test 26. route flush cache.\n");
	system("ip route flush cache");

	file_unlock(lock);
	return 0;
}

int
add_routes(char *prefix, char *var, char *ifname)
{
	char word[80], *next;
	char *ipaddr, *netmask, *gateway, *metric;
	char tmp[100];

	foreach(word, nvram_safe_get(strcat_r(prefix, var, tmp)), next) {

		netmask = word;
		ipaddr = strsep(&netmask, ":");
		if (!ipaddr || !netmask)
			continue;
		gateway = netmask;
		netmask = strsep(&gateway, ":");
		if (!netmask || !gateway)
			continue;
		metric = gateway;
		gateway = strsep(&metric, ":");
		if (!gateway || !metric)
			continue;

		/* Incorrect, empty and 0.0.0.0
		 * probably need to allow empty gateway to set on-link route */
		if (inet_addr_(gateway) == INADDR_ANY)
			gateway = nvram_safe_get(strcat_r(prefix, "xgateway", tmp));

		route_add(ifname, atoi(metric) + 1, ipaddr, gateway, netmask);
	}

	return 0;
}

static void
add_dhcp_routes(char *prefix, char *ifname, int metric)
{
	char *routes, *tmp;
	char nvname[sizeof("wanXXXXXXXXXX_routesXXX")];
	char *ipaddr, *gateway;
	char netmask[] = "255.255.255.255";
	struct in_addr mask;
	int netsize;

	if (nvram_get_int("dr_enable_x") == 0)
		return;

	/* classful static routes */
	routes = strdup(nvram_safe_get(strcat_r(prefix, "routes", nvname)));
	for (tmp = routes; tmp && *tmp; ) {
		ipaddr  = strsep(&tmp, "/");
		gateway = strsep(&tmp, " ");
		if (gateway && inet_addr(ipaddr) != INADDR_ANY)
			route_add(ifname, metric + 1, ipaddr, gateway, netmask);
	}
	free(routes);

	/* ms claseless static routes */
	routes = strdup(nvram_safe_get(strcat_r(prefix, "routes_ms", nvname)));
	for (tmp = routes; tmp && *tmp; ) {
		ipaddr  = strsep(&tmp, "/");
		netsize = atoi(strsep(&tmp, " "));
		gateway = strsep(&tmp, " ");
		if (gateway && netsize > 0 && netsize <= 32 && inet_addr(ipaddr) != INADDR_ANY) {
			mask.s_addr = htonl(0xffffffff << (32 - netsize));
			strcpy(netmask, inet_ntoa(mask));
			route_add(ifname, metric + 1, ipaddr, gateway, netmask);
		}
	}
	free(routes);

	/* rfc3442 classless static routes */
	routes = strdup(nvram_safe_get(strcat_r(prefix, "routes_rfc", nvname)));
	for (tmp = routes; tmp && *tmp; ) {
		ipaddr  = strsep(&tmp, "/");
		netsize = atoi(strsep(&tmp, " "));
		gateway = strsep(&tmp, " ");
		if (gateway && netsize > 0 && netsize <= 32 && inet_addr(ipaddr) != INADDR_ANY) {
			mask.s_addr = htonl(0xffffffff << (32 - netsize));
			strcpy(netmask, inet_ntoa(mask));
			route_add(ifname, metric + 1, ipaddr, gateway, netmask);
		}
	}
	free(routes);
}

int
del_routes(char *prefix, char *var, char *ifname)
{
	char word[80], *next;
	char *ipaddr, *netmask, *gateway, *metric;
	char tmp[100];

	foreach(word, nvram_safe_get(strcat_r(prefix, var, tmp)), next) {
		_dprintf("%s: %s\n", __FUNCTION__, word);

		netmask = word;
		ipaddr = strsep(&netmask, ":");
		if (!ipaddr || !netmask)
			continue;
		gateway = netmask;
		netmask = strsep(&gateway, ":");
		if (!netmask || !gateway)
			continue;

		metric = gateway;
		gateway = strsep(&metric, ":");
		if (!gateway || !metric)
			continue;

		if (inet_addr_(gateway) == INADDR_ANY) 	// oleg patch
			gateway = nvram_safe_get("wan0_xgateway");

		route_del(ifname, atoi(metric) + 1, ipaddr, gateway, netmask);
	}

	return 0;
}

#if 0
#ifdef RTCONFIG_IPV6
void
stop_ecmh(void)
{
	if (pids("ecmh"))
	{
		killall_tk("ecmh");
		sleep(1);
	}
}

void
start_ecmh(const char *wan_ifname)
{
	int service = get_ipv6_service();

	stop_ecmh();

	if (!wan_ifname || (strlen(wan_ifname) <= 0))
		return;

	if (!nvram_get_int("mr_enable_x"))
		return;

	switch (service) {
	case IPV6_NATIVE_DHCP:
	case IPV6_MANUAL:
#ifdef RTCONFIG_6RELAYD
	case IPV6_PASSTHROUGH:
#endif
		eval("/bin/ecmh", "-u", nvram_safe_get("http_username"), "-i", (char*)wan_ifname);
		break;
	}
}
#endif
#endif

void
stop_igmpproxy()
{
	if (pids("udpxy"))
		killall_tk("udpxy");
	if (pids("igmpproxy"))
		killall_tk("igmpproxy");
}

void	// oleg patch , add
start_igmpproxy(char *wan_ifname)
{
	FILE *fp;
	static char *igmpproxy_conf = "/tmp/igmpproxy.conf";
	char *altnet;

#ifdef RTCONFIG_DSL
#ifdef RTCONFIG_DUALWAN
	if ( nvram_match("wan0_ifname", wan_ifname)
		&& get_dualwan_primary() == WANS_DUALWAN_IF_DSL) {
		if (nvram_get_int("dslx_config_num") > 1)
			wan_ifname = "br1";
	}
#else
	if (nvram_get_int("dslx_config_num") > 1)
		wan_ifname = "br1";
#endif
#endif

#ifdef RTCONFIG_MULTICAST_IPTV
	if (nvram_get_int("switch_stb_x") > 6 &&
	    nvram_match("switch_wantag", "movistar") &&
	    !nvram_match("iptv_ifname", wan_ifname))
		return;
#endif

	stop_igmpproxy();

	if (nvram_get_int("udpxy_enable_x")) {
		_dprintf("start udpxy [%s]\n", wan_ifname);
		eval("/usr/sbin/udpxy",
			"-m", wan_ifname,
			"-p", nvram_safe_get("udpxy_enable_x"),
			"-B", "65536",
			"-c", nvram_safe_get("udpxy_clients"),
			"-a", nvram_get("lan_ifname") ? : "br0");
	}

	if (!nvram_get_int("mr_enable_x"))
		return;

	_dprintf("start igmpproxy [%s]\n", wan_ifname);

	if ((fp = fopen(igmpproxy_conf, "w")) == NULL) {
		perror(igmpproxy_conf);
		return;
	}

	altnet = nvram_safe_get("mr_altnet_x");

	if (nvram_get_int("mr_qleave_x"))
		fprintf(fp, "quickleave\n\n");
	fprintf(fp,
		"phyint %s upstream ratelimit 0 threshold 1 altnet %s\n"
		"phyint %s downstream ratelimit 0 threshold 1\n",
		wan_ifname, *altnet ? altnet : "0.0.0.0/0",
		nvram_get("lan_ifname") ? : "br0");

	append_custom_config("igmpproxy.conf", fp);
	fclose(fp);
	use_custom_config("igmpproxy.conf", igmpproxy_conf);
	run_postconf("igmpproxy", igmpproxy_conf);

	eval("/usr/sbin/igmpproxy", igmpproxy_conf);
}

int
wan_prefix(char *ifname, char *prefix)
{
	int unit;

	if ((unit = wan_ifunit(ifname)) < 0 &&
	    (unit = wanx_ifunit(ifname)) < 0)
		return -1;

	sprintf(prefix, "wan%d_", unit);

	return unit;
}

static int
add_wan_routes(char *wan_ifname)
{
	char prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	return add_routes(prefix, "route", wan_ifname);
}

static int
del_wan_routes(char *wan_ifname)
{
	char prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
#if 0
		return -1;
#else
		sprintf(prefix, "wan%d_", WAN_UNIT_FIRST);
#endif

	return del_routes(prefix, "route", wan_ifname);
}

#ifdef QOS
int enable_qos()
{
#if defined (W7_LOGO) || defined (WIFI_LOGO)
	return 0;
#endif
	int qos_userspec_app_en = 0;
	int rulenum = nvram_get_int("qos_rulenum_x"), idx_class = 0;

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

	if (	(nvram_match("qos_tos_prio", "1") ||
		 nvram_match("qos_pshack_prio", "1") ||
		 nvram_match("qos_service_enable", "1") ||
		 nvram_match("qos_shortpkt_prio", "1")	) ||
		(!nvram_match("qos_manual_ubw","0") && *nvram_safe_get("qos_manual_ubw")) ||
		(rulenum && qos_userspec_app_en)
	)
	{
		fprintf(stderr, "found QoS rulues\n");
		return 1;
	}
	else
	{
		fprintf(stderr, "no QoS rulues\n");
		return 0;
	}
}
#endif

/*
 * (1) wan[x]_ipaddr_x/wan[x]_netmask_x/wan[x]_gateway_x/...:
 *    static ip or ip get from dhcp
 *
 * (2) wan[x]_xipaddr/wan[x]_xnetmaskwan[x]_xgateway/...:
 *    ip get from dhcp when proto = l2tp/pptp/pppoe
 *
 * (3) wan[x]_ipaddr/wan[x]_netmask/wan[x]_gateway/...:
 *    always keeps the latest updated ip/netmask/gateway in system
 *      static: it is the same as (1)
 *      dhcp:
 *      	- before getting ip from dhcp server, it is 0.0.0.0
 *      	- after getting ip from dhcp server, it is updated
 *      l2tp/pptp/pppoe with static ip:
 *      	- before getting ip from vpn server, it is the same as (1)
 *      	- after getting ip from vpn server, it is the one from vpn server
 *      l2tp/pptp/pppoe with dhcp ip:
 *      	- before getting ip from dhcp server, it is 0.0.0.0
 *      	- before getting ip from vpn server, it is the one from vpn server
 */

void update_wan_state(char *prefix, int state, int reason)
{
	char tmp[100], tmp1[100], *ptr;
	int unit = -1;

	_dprintf("%s(%s, %d, %d)\n", __FUNCTION__, prefix, state, reason);

	if (strncmp(prefix, "wan", 3) == 0)
		unit = atoi(prefix + 3);

	nvram_set_int(strcat_r(prefix, "state_t", tmp), state);
	if(state == WAN_STATE_CONNECTED)
		nvram_set_int(strcat_r(prefix, "sbstate_t", tmp), WAN_STOPPED_REASON_NONE);
	else
		nvram_set_int(strcat_r(prefix, "sbstate_t", tmp), reason);

	// 20110610, reset auxstate each time state is changed
	nvram_set_int(strcat_r(prefix, "auxstate_t", tmp), 0);

	if (state == WAN_STATE_INITIALIZING)
	{
		nvram_set(strcat_r(prefix, "proto_t", tmp), nvram_safe_get(strcat_r(prefix, "proto", tmp1)));

		/* reset wanX_* variables */
		if (!nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp))) {
			nvram_set(strcat_r(prefix, "ipaddr", tmp), nvram_safe_get(strcat_r(prefix, "ipaddr_x", tmp1)));
			nvram_set(strcat_r(prefix, "netmask", tmp), nvram_safe_get(strcat_r(prefix, "netmask_x", tmp1)));
			nvram_set(strcat_r(prefix, "gateway", tmp), nvram_safe_get(strcat_r(prefix, "gateway_x", tmp1)));
		}
		else {
			nvram_set(strcat_r(prefix, "ipaddr", tmp), "0.0.0.0");
			nvram_set(strcat_r(prefix, "netmask", tmp), "0.0.0.0");
			nvram_set(strcat_r(prefix, "gateway", tmp), "0.0.0.0");
		}
		nvram_unset(strcat_r(prefix, "domain", tmp));
		nvram_unset(strcat_r(prefix, "lease", tmp));
		nvram_unset(strcat_r(prefix, "expires", tmp));
		nvram_unset(strcat_r(prefix, "routes", tmp));
		nvram_unset(strcat_r(prefix, "routes_ms", tmp));
		nvram_unset(strcat_r(prefix, "routes_rfc", tmp));

		/* reset wanX_x* VPN variables */
		nvram_set(strcat_r(prefix, "xipaddr", tmp), nvram_safe_get(strcat_r(prefix, "ipaddr", tmp1)));
		nvram_set(strcat_r(prefix, "xnetmask", tmp), nvram_safe_get(strcat_r(prefix, "netmask", tmp1)));
		nvram_set(strcat_r(prefix, "xgateway", tmp), nvram_safe_get(strcat_r(prefix, "gateway", tmp1)));
		nvram_unset(strcat_r(prefix, "xdomain", tmp));
		nvram_unset(strcat_r(prefix, "xlease", tmp));
		nvram_unset(strcat_r(prefix, "xexpires", tmp));
		nvram_unset(strcat_r(prefix, "xroutes", tmp));
		nvram_unset(strcat_r(prefix, "xroutes_ms", tmp));
		nvram_unset(strcat_r(prefix, "xroutes_rfc", tmp));

		/* reset wanX* dns variables */
		strcpy(tmp1, "");
		ptr = nvram_safe_get(strcat_r(prefix, "dns1_x", tmp));
		if (*ptr && inet_addr_(ptr) != INADDR_ANY)
			sprintf(tmp1, "%s", ptr);
		ptr = nvram_safe_get(strcat_r(prefix, "dns2_x", tmp));
		if (*ptr && inet_addr_(ptr) != INADDR_ANY)
			sprintf(tmp1 + strlen(tmp1), "%s%s", *tmp1 ? " " : "", ptr);

		/* reset wanX_dns */
		ptr = !nvram_get_int(strcat_r(prefix, "dnsenable_x", tmp)) ? tmp1 : "";
		nvram_set(strcat_r(prefix, "dns", tmp), ptr);

		/* reset wanX_xdns VPN */
		ptr = !nvram_get_int(strcat_r(prefix, "dnsenable_x", tmp)) ? tmp1 : "";
		nvram_set(strcat_r(prefix, "xdns", tmp), ptr);

#ifdef RTCONFIG_IPV6
		nvram_set(strcat_r(prefix, "6rd_ip4size", tmp), "");
		nvram_set(strcat_r(prefix, "6rd_router", tmp), "");
		nvram_set(strcat_r(prefix, "6rd_prefix", tmp), "");
		nvram_set(strcat_r(prefix, "6rd_prefixlen", tmp), "");
#endif
#ifdef RTCONFIG_TR069
//		nvram_unset(strcat_r(prefix, "tr_acs_url", tmp));
//		nvram_unset(strcat_r(prefix, "tr_pvgcode", tmp));
#endif
	}
#if 0
	else if (state == WAN_STATE_STOPPED) {
		// Save Stopped Reason
		// keep ip info if it is stopped from connected
		nvram_set_int(strcat_r(prefix, "sbstate_t", tmp), reason);
	}
#endif
	else if(state == WAN_STATE_STOPPING) {
		snprintf(tmp, sizeof(tmp), "/var/run/ppp-wan%d.status", unit);
		unlink(tmp);
	}
        else if (state == WAN_STATE_CONNECTED) {
		sprintf(tmp,"%c",prefix[3]);
                run_custom_script("wan-start", tmp);
        }

#if defined(RTCONFIG_WANRED_LED)
	switch (state) {
	case WAN_STATE_INITIALIZING:
	case WAN_STATE_STOPPED:
	case WAN_STATE_CONNECTING:
	case WAN_STATE_STOPPING:
	case WAN_STATE_CONNECTED:
		/* update WAN LED(s) as soon as possible. */
		update_wan_leds(unit);
	}
#endif
}

#ifdef RTCONFIG_IPV6
// for one ipv6 in current stage
void update_wan6_state(char *prefix, int state, int reason)
{
	char tmp[100];

	_dprintf("%s(%s, %d, %d)\n", __FUNCTION__, prefix, state, reason);

	nvram_set_int(strcat_r(prefix, "state_t", tmp), state);
	nvram_set_int(strcat_r(prefix, "sbstate_t", tmp), 0);

	if (state == WAN_STATE_INITIALIZING)
	{
	}
	else if (state == WAN_STATE_STOPPED) {
		// Save Stopped Reason
		// keep ip info if it is stopped from connected
		nvram_set_int(strcat_r(prefix, "sbstate_t", tmp), reason);
	}

}
#endif

// IPOA test case
// 111.235.232.137 (gateway)
// 111.235.232.138 (ip)
// 255.255.255.252 (netmask)

// cat /proc/net/arp
// arp -na

#ifdef RTCONFIG_DSL
static int start_ipoa()
{
	char tc_mac[32];
	char ip_addr[32];
	char ip_mask[32];
	char ip_gateway[32];
	int try_cnt;
	FILE* fp_dsl_mac;
	FILE* fp_log;

	int NeighborIpNum;
	int i;
	int NeiBaseIpNum;
	int LastIpNum;
	int NetMaskLastIpNum;
	char NeighborIpPrefix[32];
	int ip_addr_dot_cnt;
	char CmdBuf[128];

	// mac address is adsl mac
	for (try_cnt = 0; try_cnt < 10; try_cnt++)
	{
		fp_dsl_mac = fopen("/tmp/adsl/tc_mac.txt","r");
		if (fp_dsl_mac != NULL)
		{
			fgets(tc_mac,sizeof(tc_mac),fp_dsl_mac);
			fclose(fp_dsl_mac);
			break;
		}
		usleep(1000*1000);
	}

#ifdef RTCONFIG_DUALWAN
	if (get_dualwan_secondary()==WANS_DUALWAN_IF_DSL)
	{
		strcpy(ip_gateway, nvram_safe_get("wan1_gateway"));
		strcpy(ip_addr, nvram_safe_get("wan1_ipaddr"));
		strcpy(ip_mask, nvram_safe_get("wan1_netmask"));
	}
	else
	{
		strcpy(ip_gateway, nvram_safe_get("wan0_gateway"));
		strcpy(ip_addr, nvram_safe_get("wan0_ipaddr"));
		strcpy(ip_mask, nvram_safe_get("wan0_netmask"));
	}
#else
	strcpy(ip_gateway, nvram_safe_get("wan0_gateway"));
	strcpy(ip_addr, nvram_safe_get("wan0_ipaddr"));
	strcpy(ip_mask, nvram_safe_get("wan0_netmask"));
#endif

	// we only support maximum 256 neighbor host
	if (strncmp("255.255.255",ip_mask,11) != 0)
	{
		fp_log = fopen("/tmp/adsl/ipoa_too_many_neighbors.txt","w");
		fputs("ErrorMsg",fp_log);
		fclose(fp_log);
		return -1;
	}

//
// do not send arp to neighborhood and gateway
//

	ip_addr_dot_cnt = 0;
	for (i=0; i<sizeof(NeighborIpPrefix); i++)
	{
		if (ip_addr[i] == '.')
		{
			ip_addr_dot_cnt++;
			if (ip_addr_dot_cnt >= 3) break;
		}
		NeighborIpPrefix[i]=ip_addr[i];
	}
	NeighborIpPrefix[i] = 0;

	LastIpNum = atoi(&ip_addr[i+1]);
	NetMaskLastIpNum = atoi(&ip_mask[12]);
	NeighborIpNum = ((~NetMaskLastIpNum) + 1)&0xff;
	NeiBaseIpNum = LastIpNum & NetMaskLastIpNum;

	//
	// add gateway host
	//
#ifdef RTCONFIG_DSL_TCLINUX
	eval("arp","-i",nvram_safe_get("wan0_ifname"),"-a",ip_gateway,"-s",tc_mac);
#else
	eval("arp","-i","br0","-a",ip_gateway,"-s",tc_mac);
#endif

	// add neighbor hosts
	for (i=0; i<NeighborIpNum; i++)
	{
		sprintf(CmdBuf,"%s.%d",NeighborIpPrefix,i+NeiBaseIpNum);
#ifdef RTCONFIG_DSL_TCLINUX
		eval("arp","-i",nvram_safe_get("wan0_ifname"),"-a",CmdBuf,"-s",tc_mac);
#else
		eval("arp","-i","br0","-a",CmdBuf,"-s",tc_mac);
#endif
	}

	return 0;
}

static int stop_ipoa()
{
	char ip_addr[32];
	char ip_mask[32];
	char ip_gateway[32];
	FILE* fp_log;

	int NeighborIpNum;
	int i;
	int NeiBaseIpNum;
	int LastIpNum;
	int NetMaskLastIpNum;
	char NeighborIpPrefix[32];
	int ip_addr_dot_cnt;
	char CmdBuf[128];

#ifdef RTCONFIG_DUALWAN
		if (get_dualwan_secondary()==WANS_DUALWAN_IF_DSL)
		{
			strcpy(ip_gateway, nvram_safe_get("wan1_gateway"));
			strcpy(ip_addr, nvram_safe_get("wan1_ipaddr"));
			strcpy(ip_mask, nvram_safe_get("wan1_netmask"));
		}
		else
		{
			strcpy(ip_gateway, nvram_safe_get("wan0_gateway"));
			strcpy(ip_addr, nvram_safe_get("wan0_ipaddr"));
			strcpy(ip_mask, nvram_safe_get("wan0_netmask"));
		}
#else
		strcpy(ip_gateway, nvram_safe_get("wan0_gateway"));
		strcpy(ip_addr, nvram_safe_get("wan0_ipaddr"));
		strcpy(ip_mask, nvram_safe_get("wan0_netmask"));
#endif

	// we only support maximum 256 neighbor host
	if (strncmp("255.255.255",ip_mask,11) != 0)
	{
		fp_log = fopen("/tmp/adsl/ipoa_too_many_neighbors.txt","w");
		fputs("ErrorMsg",fp_log);
		fclose(fp_log);
		return -1;
	}

	//
	// do not send arp to neighborhood and gateway
	//

	ip_addr_dot_cnt = 0;
	for (i=0; i<sizeof(NeighborIpPrefix); i++)
	{
		if (ip_addr[i] == '.')
		{
			ip_addr_dot_cnt++;
			if (ip_addr_dot_cnt >= 3) break;
		}
		NeighborIpPrefix[i]=ip_addr[i];
	}
	NeighborIpPrefix[i] = 0;

	LastIpNum = atoi(&ip_addr[i+1]);
	NetMaskLastIpNum = atoi(&ip_mask[12]);
	NeighborIpNum = ((~NetMaskLastIpNum) + 1)&0xff;
	NeiBaseIpNum = LastIpNum & NetMaskLastIpNum;

	//
	// delete gateway host
	//
	eval("arp","-d",ip_gateway);

	// delete neighbor hosts
	for (i=0; i<NeighborIpNum; i++)
	{
		sprintf(CmdBuf,"%s.%d",NeighborIpPrefix,i+NeiBaseIpNum);
		eval("arp","-d",CmdBuf);
	}

	return 0;
}
#endif

#ifdef RTCONFIG_DUALWAN
void check_wan_nvram(void)
{
	if(nvram_match("wan1_proto", "")) nvram_set("wan1_proto", "dhcp");
}
#endif

#ifdef DSL_AC68U	//Andy Chiu, 2015/09/15
#include "interface.h"
extern int cur_ewan_vid;

int check_wan_if(int unit)
{
	//check wan mode.
	dbG("unit=%d\n", unit);
	if(get_wans_dualwan()&WANSCAP_LAN)
	{
		//check vid
		int new_vid;

		if(nvram_match("ewan_dot1q", "1"))
			new_vid = nvram_get_int("ewan_vid");
		else
			new_vid = 4;
		dbG("cur_vid=%d, new_vid=%d\n", cur_ewan_vid, new_vid);
		
		if(new_vid != cur_ewan_vid)
		{
			//update port info
			char buf[32], wan[32];
			const int ports[] = { 0, 1, 2, 3, 4, 5 };
			int wan1cfg = nvram_get_int("wans_lanport") + WAN1PORT1 - 1;
			snprintf(buf, sizeof(buf), "vlan%dports", cur_ewan_vid);
			nvram_unset(buf);
			switch_gen_config(wan, ports, wan1cfg, 1, "t");
			snprintf(buf, sizeof(buf), "vlan%dports", new_vid);
			nvram_set(buf, wan);

			//update hwname info
			snprintf(buf, sizeof(buf), "vlan%dhwname", cur_ewan_vid);
			nvram_unset(buf);
			snprintf(buf, sizeof(buf), "vlan%dhwname", new_vid);
			nvram_set(buf, "et0");

			//generate vlan interfaces
			char cur_vif[16], new_vif[16];
			sprintf(cur_vif, "vlan%d", cur_ewan_vid);
			sprintf(new_vif, "vlan%d", new_vid);

			dbG("cur_vif=%s, new_vif=%s\n", cur_vif, new_vif);
			
			char word[256], *next, tmp[256];
			//replace old vlan interface by new vlan interface
			memset(tmp, 0, 256);
			foreach(word, nvram_safe_get("wandevs"), next)
			{
				sprintf(tmp, "%s %s", tmp, (!strcmp(word, cur_vif))? new_vif: word);
			}
			dbG("wandevs=%s\n", tmp);
			nvram_set("wandevs", tmp);

			//replace wan_phy
			memset(tmp, 0, 256);
			foreach(word, nvram_safe_get("wan_ifnames"), next)
			{
				sprintf(tmp, "%s %s", tmp, (!strcmp(word, cur_vif))? new_vif: word);
			}
			dbG("wan_ifnames=%s\n", tmp);
			nvram_set("wan_ifnames", tmp);

			//check all wan unit.
			int unit;
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
			{
				snprintf(buf, sizeof(buf), "wan%d_ifname", unit);
				if(nvram_match(buf, cur_vif))
				{
					dbG("%s=%s\n", buf, new_vif);
					nvram_set(buf, new_vif);
				}

				sprintf(buf, "wan%d_gw_ifname", unit);
				if(nvram_match(buf, cur_vif))
				{
					dbG("%s=%s\n", buf, new_vif);
					nvram_set(buf, new_vif);
				}
			}

			//remove old vlan		
			eval("ifconfig", cur_vif, "down");
			eval("vconfig", "rem", cur_vif);

			//set new vlan
			eval("vconfig", "set_name_type", "VLAN_PLUS_VID_NO_PAD");
			snprintf(buf, sizeof(buf), "%d", new_vid);
			eval("vconfig", "add", "eth0", buf);
			eval("ifconfig", new_vif, "up");
			cur_ewan_vid = new_vid;
		}
		dbG("Set switch\n");
		config_switch_dsl_set_lan();
	}
	return 0;
}
#endif 

void
start_wan_if(int unit)
{
#ifdef RTCONFIG_DUALWAN
	int wan_type;
	int pppoerelay_unit;
#endif
	char *wan_ifname;
	char *wan_proto;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char eabuf[32];
	int s;
	struct ifreq ifr;
	pid_t pid;
#ifdef RTCONFIG_USB_MODEM
	char usb_node[32], port_path[8];
	char nvram_name[32];
	int i = 0;
	char modem_type[32];
#ifdef RTCONFIG_USB_BECEEM
	unsigned int uvid, upid;
#endif
#ifdef SET_USB_MODEM_MTU_ETH
	int modem_mtu;
#endif
#endif
	int mtu;
	struct vlan_ioctl_args ifv;

	TRACE_PT("unit=%d.\n", unit);
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	/* variable should exist? */
	if (nvram_match(strcat_r(prefix, "enable", tmp), "0")) {
		update_wan_state(prefix, WAN_STATE_DISABLED, 0);
		return;
	}
#ifdef RTCONFIG_USB_MODEM
	else if(nvram_get_int("usb_modem_act_scanning") != 0){
_dprintf("start_wan_if: USB modem is scanning...\n");
		update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_USBSCAN);
		return;
	}
#endif
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	// had detected the DATA limit before.
	else if(nvram_get_int(strcat_r(prefix, "sbstate_t", tmp)) == WAN_STOPPED_REASON_DATALIMIT){
		TRACE_PT("start_wan_if: Data limit was detected and skip the start_wan_if().\n");
		return;
	}
#endif

	update_wan_state(prefix, WAN_STATE_INITIALIZING, 0);

#ifdef RTCONFIG_DUALWAN
	wan_type = get_dualwan_by_unit(unit);
#endif

#ifdef RTCONFIG_DUALWAN
	if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
		if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
			check_wan_nvram();
	}
#endif

#ifdef RTCONFIG_USB_MODEM
	if (dualwan_unit__usbif(unit)) {
		FILE *fp;

		if(is_usb_modem_ready() != 1){
			TRACE_PT("No USB Modem!\n");
			return;
		}

		TRACE_PT("3g begin.\n");
		if(nvram_match("g3err_pin", "1")){
			TRACE_PT("3g end: PIN error previously!\n");
			update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_PINCODE_ERR);
			return;
		}

		update_wan_state(prefix, WAN_STATE_CONNECTING, 0);

		snprintf(modem_type, 32, "%s", nvram_safe_get("usb_modem_act_type"));
		if(strlen(modem_type) <= 0){
			eval("/usr/sbin/find_modem_type.sh");
			snprintf(modem_type, 32, "%s", nvram_safe_get("usb_modem_act_type"));
		}

		if(!strcmp(modem_type, "tty") || !strcmp(modem_type, "qmi") || !strcmp(modem_type, "mbim") || !strcmp(modem_type, "gobi")){
			if(!strcmp(nvram_safe_get("usb_modem_act_int"), "")){
				if(!strcmp(modem_type, "qmi")){	// e.q. Huawei E398.
					TRACE_PT("Sleep 3 seconds to wait modem nodes.\n");
					sleep(3);
				}
				else{
					TRACE_PT("Sleep 2 seconds to wait modem nodes.\n");
					sleep(2);
				}
			}

			// find the modem node at every start_wan_if() to avoid the incorrct one sometimes.
			eval("/usr/sbin/find_modem_node.sh");
		}

		if(nvram_get_int("usb_modem_act_reset") == 1){
			// need to execute find_modem_xxx.sh again.
			TRACE_PT("3g end: Reseting the modem...\n");
			return;
		}

		/* Stop pppd */
		stop_pppd(unit);

		/* Stop dhcp client */
		stop_udhcpc(unit);

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
		unsigned long long rx, tx;
		unsigned long long total, limit;

		rx = strtoull(nvram_safe_get("modem_bytes_rx"), NULL, 10);
		tx = strtoull(nvram_safe_get("modem_bytes_tx"), NULL, 10);
		limit = strtoull(nvram_safe_get("modem_bytes_data_limit"), NULL, 10);

		total = rx+tx;

		if(limit > 0 && total >= limit){
			TRACE_PT("3g end: Data limit was set: limit %llu, now %llu.\n", limit, total);
			update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_DATALIMIT);
			return;
		}
#endif

		if(nvram_get_int("stop_conn_3g") == 1){
			write_3g_ppp_conf();
		}
		else if(strcmp(modem_type, "wimax")){
			char *modem_argv[] = {"/usr/sbin/modem_enable.sh", NULL};
			char *ptr;
			int sim_state;

			_eval(modem_argv, ">>/tmp/usb.log", 0, NULL);

			if(nvram_match("g3err_pin", "1")){
				TRACE_PT("3g end: PIN error!\n");
				update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_PINCODE_ERR);
				return;
			}

			ptr = nvram_safe_get("usb_modem_act_sim");
			if(strlen(ptr) > 0){
				sim_state = atoi(ptr);
				if(sim_state == 2 || sim_state == 3){
					TRACE_PT("3g end: Need to input PIN or PUK.\n");
					update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_PINCODE_ERR);
					return;
				}
				else if(sim_state != 1){
					TRACE_PT("3g end: SIM isn't ready.\n");
					return;
				}
			}
		}

		if((!strcmp(modem_type, "tty") || !strcmp(modem_type, "mbim"))
				&& write_3g_ppp_conf()
				&& (fp = fopen(PPP_CONF_FOR_3G, "r")) != NULL){
			fclose(fp);

			// run as ppp proto.
			nvram_set(strcat_r(prefix, "proto", tmp), "pppoe");
#ifndef RTCONFIG_DUALWAN
			nvram_set(strcat_r(prefix, "dhcpenable_x", tmp), "1");
			nvram_set(strcat_r(prefix, "vpndhcp", tmp), "0");
			nvram_set(strcat_r(prefix, "dnsenable_x", tmp), "1");
#endif

			char *pppd_argv[] = { "/usr/sbin/pppd", "call", "3g", "nochecktime", NULL};

			if(nvram_get_int("stop_conn_3g") != 1)
				_eval(pppd_argv, NULL, 0, NULL);
			else
				TRACE_PT("stop_conn_3g was set.\n");
		}
		// RNDIS, QMI interface: usbX, Beceem interface: usbbcm -> ethX, gct(mad)wimax: wimaxX.
		else{
			wan_ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

			if(strlen(wan_ifname) <= 0){
#ifdef RTCONFIG_USB_BECEEM
				snprintf(usb_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
				if(strlen(usb_node) <= 0)
					return;

				if(get_path_by_node(usb_node, port_path, 8) != NULL){
					snprintf(nvram_name, 32, "usb_path%s", port_path);
					TRACE_PT("RNDIS/Beceem: start_wan_if.\n");

					if(!strcmp(nvram_safe_get(nvram_name), "modem")){
						snprintf(nvram_name, 32, "usb_path%s_vid", port_path);
						uvid = strtoul(nvram_safe_get(nvram_name), NULL, 16);
						snprintf(nvram_name, 32, "usb_path%s_pid", port_path);
						upid = strtoul(nvram_safe_get(nvram_name), NULL, 16);

						if(is_samsung_dongle(1, uvid, upid)){
							modprobe("tun");
							sleep(1);

							xstart("madwimax");
						}
						else if(is_gct_dongle(1, uvid, upid)){
							modprobe("tun");
							sleep(1);

							write_gct_conf();

							xstart("gctwimax", "-C", WIMAX_CONF);
						}
					}
				}
#endif

				return;
			}

#ifdef SET_USB_MODEM_MTU_ETH
			modem_mtu = nvram_get_int("modem_mtu");
#endif
			ifr.ifr_flags = 0;
			for (i = 0; i < 3; i++) {
				int flags, mtu = 0;

				if (_ifconfig_get(wan_ifname, &flags, NULL, NULL, NULL, &mtu) != 0) {
					TRACE_PT("Couldn't read the flags of %s!\n", wan_ifname);
					update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
					return;
				}
				ifr.ifr_flags = flags;
#ifdef SET_USB_MODEM_MTU_ETH
				mtu = (modem_mtu >= 576 && modem_mtu < mtu) ? modem_mtu : 0;
				if ((flags & IFF_UP) && !mtu)
					break;
				_ifconfig(wan_ifname, flags | IFUP, NULL, NULL, NULL, mtu);
				if ((flags & IFF_UP))
					break;
#else
				if ((flags & IFF_UP))
					break;
				ifconfig(wan_ifname, flags | IFUP, NULL, NULL);
#endif
				TRACE_PT("%s: wait %s be up, %d second...!\n", __FUNCTION__, wan_ifname, i);
				sleep(1);
			};

			if (!(ifr.ifr_flags & IFF_UP)) {
				TRACE_PT("Interface %s couldn't be up!\n", wan_ifname);
				update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
				return;
			}

			// run as dhcp proto.
			nvram_set(strcat_r(prefix, "proto", tmp), "dhcp");
			nvram_set(strcat_r(prefix, "dhcpenable_x", tmp), "1");
			nvram_set(strcat_r(prefix, "dnsenable_x", tmp), "1");

			// Android phone, RNDIS, QMI interface, Gobi.
			if(!strncmp(wan_ifname, "usb", 3)){
				if(nvram_get_int("stop_conn_3g") != 1){
#ifdef RTCONFIG_TCPDUMP
					char *tcpdump_argv[] = { "/usr/sbin/tcpdump", "-i", wan_ifname, "-nnXw", "/tmp/udhcpc.pcap", NULL};
					_eval(tcpdump_argv, NULL, 0, &pid);
					sleep(2);
#endif
					start_udhcpc(wan_ifname, unit, &pid);
				}
				else
					TRACE_PT("stop_conn_3g was set.\n");
			}
			// Beceem dongle, ASIX USB to RJ45 converter, Huawei E353.
			else if(!strncmp(wan_ifname, "eth", 3)){
#ifdef RTCONFIG_USB_BECEEM
				write_beceem_conf(wan_ifname);
#endif

				if(nvram_get_int("stop_conn_3g") != 1){
					snprintf(usb_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
					if(strlen(usb_node) <= 0)
						return;

					if(get_path_by_node(usb_node, port_path, 8) == NULL)
						return;

					snprintf(nvram_name, 32, "usb_path%s_act", port_path);

					if(!strcmp(nvram_safe_get(nvram_name), wan_ifname))
						start_udhcpc(wan_ifname, unit, &pid);

#ifdef RTCONFIG_USB_BECEEM
					if(strlen(nvram_safe_get(nvram_name)) <= 0){
						char buf[128];

						snprintf(buf, 128, "wimaxd -c %s", WIMAX_CONF);
						TRACE_PT("%s: cmd=%s.\n", __FUNCTION__, buf);
						system(buf);
						sleep(3);

						TRACE_PT("%s: cmd=wimaxc search.\n", __FUNCTION__);
						system("wimaxc search");
						TRACE_PT("%s: sleep 10 seconds.\n", __FUNCTION__);
						sleep(10);

						TRACE_PT("%s: cmd=wimaxc connect.\n", __FUNCTION__);
						system("wimaxc connect");
					}
#endif
				}
				else
					TRACE_PT("stop_conn_3g was set.\n");
			}
#ifdef RTCONFIG_USB_BECEEM
			else if(!strncmp(wan_ifname, "wimax", 5)){
				if(nvram_get_int("stop_conn_3g") != 1)
					start_udhcpc(wan_ifname, unit, &pid);
				else
					TRACE_PT("stop_conn_3g was set.\n");
			}
#endif
		}

		TRACE_PT("3g end.\n");
		return;
	}
	else
#endif
	if (dualwan_unit__nonusbif(unit)) {
		convert_wan_nvram(prefix, unit);

		/* make sure the connection exists and is enabled */
		wan_ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
		if(strlen(wan_ifname) <= 0)
			return;

		wan_proto = nvram_get(strcat_r(prefix, "proto", tmp));
		if (!wan_proto || !strcmp(wan_proto, "disabled")) {
			update_wan_state(prefix, WAN_STATE_DISABLED, 0);
			return;
		}

		/* Set i/f hardware address before bringing it up */
		if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
			update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
			return;
		}

		strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);

		/* Since WAN interface may be already turned up (by vlan.c),
			if WAN hardware address is specified (and different than the current one),
			we need to make it down for synchronizing hwaddr. */
		if (ioctl(s, SIOCGIFHWADDR, &ifr)) {
			close(s);
			update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
			return;
		}

#ifdef RTCONFIG_IPV6
/* TODO: rewrite enabling/disabling IPv6 */
		switch (get_ipv6_service_by_unit(unit)) {
		case IPV6_NATIVE_DHCP:
		case IPV6_MANUAL:
#ifdef RTCONFIG_6RELAYD
		case IPV6_PASSTHROUGH:
#endif
			if (strcmp(wan_proto, "dhcp") != 0 && strcmp(wan_proto, "static") != 0 &&
			    nvram_match(ipv6_nvname("ipv6_ifdev"), "ppp")) {
				disable_ipv6(wan_ifname);
#if !defined(RTCONFIG_BCMARM) && !defined(RTCONFIG_RALINK) && !defined(RTCONFIG_QCA)
				if (with_ipv6_linklocal_addr(wan_ifname))
					doSystem("ip -6 addr flush dev %s scope link", wan_ifname);
#endif
			} else
				enable_ipv6(wan_ifname);
			break;
		}
#endif
		ether_atoe((const char *) nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)), (unsigned char *) eabuf);
		if ((bcmp(eabuf, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN)))
		{
			/* current hardware address is different than user specified */
			ifconfig(wan_ifname, 0, NULL, NULL);
		}
#ifdef RTCONFIG_IPV6
/* TODO: rewrite syncing MAC with enabled IPv6 */
#ifdef RTCONFIG_RALINK
		switch (get_ipv6_service_by_unit(unit)) {
		case IPV6_NATIVE_DHCP:
		case IPV6_MANUAL:
#ifdef RTCONFIG_6RELAYD
		case IPV6_PASSTHROUGH:
#endif
			if (strcmp(wan_proto, "dhcp") != 0 && strcmp(wan_proto, "static") != 0 &&
			    nvram_match(ipv6_nvname("ipv6_ifdev"), "eth") &&
			    !with_ipv6_linklocal_addr(wan_ifname)) {
				ifconfig(wan_ifname, 0, NULL, NULL);
				break;
			}
			/* fall through */
		default:
			if (with_ipv6_linklocal_addr(wan_ifname))
				ifconfig(wan_ifname, 0, NULL, NULL);
			break;
		}
#endif
#endif
		/* Configure i/f only once, specially for wireless i/f shared by multiple connections */
		if (ioctl(s, SIOCGIFFLAGS, &ifr)) {
			close(s);
			update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
			return;
		}

		if (!(ifr.ifr_flags & IFF_UP)) {
			fprintf(stderr, "** wan_ifname: %s is NOT UP\n", wan_ifname);

			/* Sync connection nvram address and i/f hardware address */
			memset(ifr.ifr_hwaddr.sa_data, 0, ETHER_ADDR_LEN);

			if (nvram_match(strcat_r(prefix, "hwaddr", tmp), "") ||
			    !ether_atoe((const char *) nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)), (unsigned char *) ifr.ifr_hwaddr.sa_data) ||
			    !memcmp(ifr.ifr_hwaddr.sa_data, "\0\0\0\0\0\0", ETHER_ADDR_LEN)) {
				if (ioctl(s, SIOCGIFHWADDR, &ifr)) {
					fprintf(stderr, "ioctl fail. continue\n");
					close(s);
					update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
					return;
				}
				nvram_set(strcat_r(prefix, "hwaddr", tmp), ether_etoa((unsigned char *) ifr.ifr_hwaddr.sa_data, eabuf));
			}
			else {
				ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
				ioctl(s, SIOCSIFHWADDR, &ifr);
			}

			/* Bring up i/f */
			ifconfig(wan_ifname, IFUP, NULL, NULL);
		}
		close(s);

		/* Set initial QoS mode again now that WAN port is ready. */
#ifdef CONFIG_BCMWL5
		set_et_qos_mode();
#endif

#if defined(RTCONFIG_DUALWAN)
		pppoerelay_unit = wan_primary_ifunit();
		if (nvram_match("wans_mode", "lb") && get_nr_wan_unit() > 1)
			pppoerelay_unit = nvram_get_int("pppoerelay_unit");
		if (unit == pppoerelay_unit)
			start_pppoe_relay(wan_ifname);
#else
		if (unit == wan_primary_ifunit())
			start_pppoe_relay(wan_ifname);
#endif

		enable_ip_forward();

		update_wan_state(prefix, WAN_STATE_CONNECTING, 0);

		/*
		 * Configure PPPoE connection. The PPPoE client will run
		 * ip-up/ip-down scripts upon link's connect/disconnect.
		 */
		if (strcmp(wan_proto, "pppoe") == 0 ||
		    strcmp(wan_proto, "pptp") == 0 ||
		    strcmp(wan_proto, "l2tp") == 0) 	// oleg patch
		{
			char *ipaddr = nvram_safe_get(strcat_r(prefix, "xipaddr", tmp));
			char *netmask = nvram_safe_get(strcat_r(prefix, "xnetmask", tmp));
			int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
			int demand = nvram_get_int(strcat_r(prefix, "pppoe_idletime", tmp)) &&
					strcmp(wan_proto, "l2tp");	/* L2TP does not support idling */

			/* update demand option */
			nvram_set_int(strcat_r(prefix, "pppoe_demand", tmp), demand);

			if (dhcpenable == 0 &&
			    inet_equal(ipaddr, netmask,
				       nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"))) {
				update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_INVALID_IPADDR);
				return;
			}

			/* Bring up WAN interface */
			ifconfig(wan_ifname, IFUP, ipaddr, netmask);

			/* Increase WAN interface's MTU to allow pppoe MTU/MRU over 1492 (with 8 byte overhead) */
			if (strcmp(wan_proto, "pppoe") == 0) {
				/* Compute maximum required MTU by taking the maximum of the pppoe MRU and MTU values */
				int mru = nvram_get_int(strcat_r(prefix, "pppoe_mru", tmp));
				mtu = nvram_get_int(strcat_r(prefix, "pppoe_mtu", tmp));
				if (mru > mtu)
					mtu = mru;
				if (mtu > 1492) {
					if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
						/* First set parent device if vlan was configured */
						strncpy(ifv.device1, wan_ifname, IFNAMSIZ);
						ifv.cmd = GET_VLAN_REALDEV_NAME_CMD;
						if (ioctl(s, SIOCGIFVLAN, &ifv) >= 0) {
							strncpy(ifr.ifr_name, ifv.u.device2, IFNAMSIZ);
							ifr.ifr_mtu = mtu + 8;
							if (ioctl(s, SIOCSIFMTU, &ifr)) {
								perror(wan_ifname);
								logmessage("start_wan_if()", "Error setting MTU on %s to %d", ifv.u.device2, mtu);
							}
						}

						/* Set WAN device */
						strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);
						ifr.ifr_mtu = mtu + 8;
						if (ioctl(s, SIOCSIFMTU, &ifr)) {
							perror(wan_ifname);
							logmessage("start_wan_if()", "Error setting MTU on %s to %d", wan_ifname, mtu);
						}
						close(s);
					}
				}
			}

			/* launch dhcp client and wait for lease forawhile */
			if (dhcpenable) {
				start_udhcpc(wan_ifname, unit,
					(strcmp(wan_proto, "pppoe") == 0) ? &pid : NULL);
			} else {
				char *gateway = nvram_safe_get(strcat_r(prefix, "xgateway", tmp));

				/* start firewall */
// TODO: handle different lan_ifname
				start_firewall(unit, 0);

				/* setup static wan routes via physical device */
				add_routes(prefix, "mroute", wan_ifname);

				/* and set default route if specified with metric 1 */
				if (inet_addr_(gateway) != INADDR_ANY &&
				    !nvram_match(strcat_r(prefix, "heartbeat_x", tmp), "")) {
					in_addr_t mask = inet_addr(netmask);

					/* the gateway is out of the local network */
					if ((inet_addr(gateway) & mask) != (inet_addr(ipaddr) & mask))
						route_add(wan_ifname, 2, gateway, NULL, "255.255.255.255");

					/* default route via default gateway */
					route_add(wan_ifname, 2, "0.0.0.0", gateway, "0.0.0.0");
				}

				/* update resolv.conf */
				update_resolvconf();

				/* start multicast router on Static+VPN physical interface */
				if (unit == wan_primary_ifunit())
					start_igmpproxy(wan_ifname);
			}

			/* launch pppoe client daemon */
			start_pppd(unit);

			/* ppp interface name is referenced from this point
			 * after pppd start before ip-pre-up it will be empty */
			wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));

			/* Pretend that the WAN interface is up */
			if (demand) {
				int timeout = 5;

				/* Wait for pppx to be created */
				while (timeout--) {
					/* ppp interface name is re-referenced from this point */
					wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));
					if (*wan_ifname && ifconfig(wan_ifname, IFUP, NULL, NULL) == 0)
						break;
					_dprintf("%s: wait interface %s up at %d seconds...\n", __FUNCTION__, wan_ifname, timeout);
					sleep(1);
				}

				if (!*wan_ifname) {
					_dprintf("%s: no interface of wan_unit %d.\n", __FUNCTION__, unit);
					return;
				}

				/* Retrieve IP info */
				if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
					update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
					return;
				}
				strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);

				/* Set temporary IP address */
				if (ioctl(s, SIOCGIFADDR, &ifr))
					perror(wan_ifname);
				nvram_set(strcat_r(prefix, "ipaddr", tmp), inet_ntoa(sin_addr(&ifr.ifr_addr)));
				nvram_set(strcat_r(prefix, "netmask", tmp), "255.255.255.255");

				/* Set temporary P-t-P address */
				if (ioctl(s, SIOCGIFDSTADDR, &ifr))
					perror(wan_ifname);
				nvram_set(strcat_r(prefix, "gateway", tmp), inet_ntoa(sin_addr(&ifr.ifr_dstaddr)));

				close(s);

				/*
				 * Preset routes so that traffic can be sent to proper pppx even before
				 * the link is brought up.
				 */
				preset_wan_routes(wan_ifname);

				/* Trigger it up to obtain PPP DNS early */
				start_demand_ppp(unit, 0);
			}
		}
		/*
		 * Configure DHCP connection. The DHCP client will run
		 * 'udhcpc bound'/'udhcpc deconfig' upon finishing IP address
		 * renew and release.
		 */
		else if (strcmp(wan_proto, "dhcp") == 0)
		{
			/* Bring up WAN interface */
			dbG("ifup:%s\n", wan_ifname);
			ifconfig(wan_ifname, IFUP, NULL, NULL);

			/* MTU */
			if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
				mtu = nvram_get_int(strcat_r(prefix, "mtu", tmp));
				if (mtu < 576)
					mtu = 576;

				if (mtu > 9000)
					mtu = 9000;	// Limit to a sane value

				ifr.ifr_mtu = mtu;
				strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);
				if (ioctl(s, SIOCSIFMTU, &ifr)) {
					perror(wan_ifname);
					logmessage("start_wan_if()","Error setting MTU on %s to %d", wan_ifname, mtu);
				}
				close(s);
			}

			/* Start pre-authenticator */
			dbG("start auth:%d\n", unit);
			start_auth(unit, 0);

#ifdef RTCONFIG_DSL
			nvram_set(strcat_r(prefix, "clientid_type", tmp), nvram_safe_get("dslx_dhcp_clientid_type"));
			nvram_set(strcat_r(prefix, "clientid", tmp), nvram_safe_get("dslx_dhcp_clientid"));
			nvram_set(strcat_r(prefix, "vendorid", tmp), nvram_safe_get("dslx_dhcp_vendorid"));
#endif
			/* Start dhcp daemon */
			dbG("start udhcpc:%s, %d\n", wan_ifname, unit);
			start_udhcpc(wan_ifname, unit, &pid);
		}
		/* Configure static IP connection. */
		else if (strcmp(wan_proto, "static") == 0)
		{
			if (inet_equal(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)), nvram_safe_get(strcat_r(prefix, "netmask", tmp)),
				       nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"))) {
				update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_INVALID_IPADDR);
				return;
			}

			/* Assign static IP address to i/f */
			ifconfig(wan_ifname, IFUP,
					nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
					nvram_safe_get(strcat_r(prefix, "netmask", tmp)));

			/* Start pre-authenticator */
			start_auth(unit, 0);

#ifdef RTCONFIG_DSL
#ifdef RTCONFIG_DUALWAN
			if (wan_type == WANS_DUALWAN_IF_DSL)
#endif
			(
				if (nvram_match("dsl0_proto", "ipoa"))
					start_ipoa();
#endif

			/* MTU */
			if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
				mtu = nvram_get_int(strcat_r(prefix, "mtu", tmp));
				if (mtu < 576)
					mtu = 576;

				if (mtu > 9000)
					mtu = 9000;     // Limit to a sane value

				ifr.ifr_mtu = mtu;
				strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);
				if (ioctl(s, SIOCSIFMTU, &ifr)) {
					perror(wan_ifname);
					logmessage("start_wan_if()","Error setting MTU on %s to %d", wan_ifname, mtu);
				}
				close(s);
			}

			/* We are done configuration */
			wan_up(wan_ifname);
		}
	}
	else
#ifdef RTCONFIG_DUALWAN
		_dprintf("%s(): Cound't find the type(%d) of unit(%d)!!!\n", __FUNCTION__, wan_type, unit);
#else
		_dprintf("%s(): Cound't find the wan(%d)!!!\n", __FUNCTION__, unit);
#endif

	_dprintf("%s(): End.\n", __FUNCTION__);
}

void
stop_wan_if(int unit)
{
#if defined(RTCONFIG_DSL) && defined(RTCONFIG_DUALWAN)
	int wan_type;
#endif
	char *wan_ifname;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char *wan_proto, active_proto[32];
#if defined(RTCONFIG_USB_BECEEM)
	int i;
#endif
#ifdef RTCONFIG_USB_BECEEM
	unsigned int uvid, upid;
#endif
	int end_wan_sbstate = WAN_STOPPED_REASON_NONE;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	if(nvram_get_int(strcat_r(prefix, "sbstate_t", tmp)) == WAN_STOPPED_REASON_DATALIMIT)
		end_wan_sbstate = WAN_STOPPED_REASON_DATALIMIT;
#endif

	update_wan_state(prefix, WAN_STATE_STOPPING, end_wan_sbstate);

	/* Backup active wan_proto for later restore, if it have been updated by ui */
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	strncpy(active_proto, wan_proto, sizeof(active_proto));

	/* Set previous wan_proto as active */
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto_t", tmp));
	if (*wan_proto && strcmp(active_proto, wan_proto) != 0)
	{
		stop_iQos(); // clean all tc rules
		_dprintf("%s %sproto_t=%s\n", __FUNCTION__, prefix, wan_proto);
		nvram_set(strcat_r(prefix, "proto", tmp), wan_proto);
		nvram_unset(strcat_r(prefix, "proto_t", tmp));
	}

	// Handel for each interface
	if(unit == wan_primary_ifunit()){
		killall_tk("stats");
		killall_tk("ntpclient");

		/* Shutdown and kill all possible tasks */
#if 0
		killall_tk("ip-up");
		killall_tk("ip-down");
		killall_tk("ip-pre-up");
#ifdef RTCONFIG_IPV6
		killall_tk("ipv6-up");
		killall_tk("ipv6-down");
#endif
		killall_tk("auth-fail");
#endif

#ifdef RTCONFIG_MULTICAST_IPTV
		if (nvram_get_int("switch_stb_x") > 6 &&
		    nvram_match("switch_wantag", "movistar") && unit == WAN_UNIT_IPTV)
#endif
		stop_igmpproxy();
	}

#ifdef RTCONFIG_VPNC
	/* Stop VPN client */
	stop_vpnc();
#endif

	/* Stop l2tp */
	if (strcmp(wan_proto, "l2tp") == 0) {
		kill_pidfile_tk("/var/run/l2tpd.pid");
		usleep(1000*10000);
	}

	/* Stop pppd */
	stop_pppd(unit);

	/* Stop post-authenticator */
	stop_auth(unit, 1);

	/* Stop dhcp client */
	stop_udhcpc(unit);

	/* Stop pre-authenticator */
	stop_auth(unit, 0);

	/* Bring down WAN interfaces */
	// Does it have to?
	wan_ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
#ifdef RTCONFIG_USB_MODEM
	if(strncmp(wan_ifname, "/dev/tty", 8))
#endif
	{
		if(strlen(wan_ifname) > 0){
#ifdef RTCONFIG_SOC_IPQ40XX
			if (strcmp(wan_ifname, "eth0") == 0)
				ifconfig(wan_ifname, IFUP, "0.0.0.0", NULL);
			else
#endif
			ifconfig(wan_ifname, 0, NULL, NULL);
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
			if(!strncmp(wan_ifname, "eth", 3) || !strncmp(wan_ifname, "vlan", 4))
				ifconfig(wan_ifname, IFUP, "0.0.0.0", NULL);
#endif
		}
	}

#ifdef RTCONFIG_DSL
#ifdef RTCONFIG_DUALWAN
	wan_type = get_dualwan_by_unit(unit);
	if(wan_type == WANS_DUALWAN_IF_DSL)
#endif
		if (nvram_match("dsl0_proto", "ipoa"))
		{
			stop_ipoa();
		}
#endif

#if defined(RTCONFIG_USB_MODEM)
	if (dualwan_unit__usbif(unit)) {
#if defined(RTCONFIG_USB_BECEEM)
		if(is_usb_modem_ready() == 1){
			if(pids("wimaxd"))
				eval("wimaxc", "disconnect");
		}

		if(pids("wimaxd")){
			killall("wimaxd", SIGTERM);
			killall("wimaxd", SIGUSR1);
		}

		uvid = atoi(nvram_safe_get("usb_modem_act_vid"));
		upid = atoi(nvram_safe_get("usb_modem_act_pid"));

		if(is_samsung_dongle(1, uvid, upid)){
			i = 0;
			while(i < 3){
				if(pids("madwimax")){
					killall_tk("madwimax");
					sleep(1);

					++i;
				}
				else
					break;
			}

			modprobe_r("tun");

			nvram_set(strcat_r(prefix, "ifname", tmp), "");
		}
		else if(is_gct_dongle(1, uvid, upid)){
			i = 0;
			while(i < 3){
				if(pids("gctwimax")){
					killall_tk("gctwimax");
					sleep(1);

					++i;
				}
				else
					break;
			}
			unlink(WIMAX_CONF);

			modprobe_r("tun");

			nvram_set(strcat_r(prefix, "ifname", tmp), "");
		}
#endif	/* RTCONFIG_USB_BECEEM */

#ifdef RTCONFIG_INTERNAL_GOBI
		char *const modem_argv[] = {"/usr/sbin/modem_stop.sh", NULL};

		_eval(modem_argv, ">>/tmp/usb.log", 0, NULL);
#endif
	}

	if(dualwan_unit__usbif(unit))
		update_wan_state(prefix, WAN_STATE_INITIALIZING, end_wan_sbstate);
	else
#endif // RTCONFIG_USB_MODEM
		update_wan_state(prefix, WAN_STATE_STOPPED, end_wan_sbstate);

	// wait for release finished ?
	if (!g_reboot)
		sleep(2);

	/* Restore active wan_proto value */
	_dprintf("%s %sproto=%s\n", __FUNCTION__, prefix, active_proto);
	nvram_set(strcat_r(prefix, "proto", tmp), active_proto);
}

int update_resolvconf(void)
{
	FILE *fp, *fp_servers;
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char *wan_dns, *wan_domain, *next;
	char domain[64], *next_domain;
	int unit, lock;
#ifdef RTCONFIG_YANDEXDNS
	int yadns_mode = nvram_get_int("yadns_enable_x") ? nvram_get_int("yadns_mode") : YADNS_DISABLED;
#endif
#ifdef RTCONFIG_OPENVPN
	int dnsstrict = 0;
#endif

	lock = file_lock("resolv");

	if (!(fp = fopen("/tmp/resolv.conf", "w+"))) {
		perror("/tmp/resolv.conf");
		goto error;
	}
	if (!(fp_servers = fopen("/tmp/resolv.dnsmasq", "w+"))) {
		perror("/tmp/resolv.dnsmasq");
		fclose(fp);
		goto error;
	}

	/* Add DNS from VPN clients, others if non-exclusive */
#ifdef RTCONFIG_OPENVPN
	dnsstrict = write_vpn_resolv(fp);
	// If dns not set to exclusive
	if (dnsstrict != 3)
#endif
	{
		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++) {
		char *wan_xdns, *wan_xdomain;

		/* TODO: Skip unused wans
			if (wan disabled or inactive and it's not ppp with active man)
				continue */

			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			wan_dns = nvram_safe_get(strcat_r(prefix, "dns", tmp));
			wan_xdns = nvram_safe_get(strcat_r(prefix, "xdns", tmp));

			if (!*wan_dns && !*wan_xdns)
				continue;

		foreach(tmp, (*wan_dns ? wan_dns : wan_xdns), next)
			fprintf(fp, "nameserver %s\n", tmp);

		wan_domain = nvram_safe_get(strcat_r(prefix, "domain", tmp));
		foreach(tmp, wan_dns, next) {
			foreach(domain, wan_domain, next_domain)
				fprintf(fp_servers, "server=/%s/%s\n", domain, tmp);
#ifdef RTCONFIG_YANDEXDNS
			if (yadns_mode != YADNS_DISABLED)
				fprintf(fp_servers, "server=/%s/%s\n", "local", tmp);
#endif
		}
		wan_xdomain = nvram_safe_get(strcat_r(prefix, "xdomain", tmp));
		foreach(tmp, wan_xdns, next) {
			int new = (find_word(wan_dns, tmp) == NULL);
			foreach(domain, wan_xdomain, next_domain) {
				if (new || find_word(wan_domain, domain) == NULL)
					fprintf(fp_servers, "server=/%s/%s\n", domain, tmp);
			}
#ifdef RTCONFIG_YANDEXDNS
			if (yadns_mode != YADNS_DISABLED && new)
				fprintf(fp_servers, "server=/%s/%s\n", "local", tmp);
#endif
		}
	}
#ifdef RTCONFIG_OPENVPN
	}
#endif
#ifdef RTCONFIG_YANDEXDNS
	if (yadns_mode != YADNS_DISABLED) {
		char *server[2];
		int count = get_yandex_dns(AF_INET, yadns_mode, server, sizeof(server)/sizeof(server[0]));
		for (unit = 0; unit < count; unit++) {
			fprintf(fp_servers, "server=%s\n", server[unit]);
			fprintf(fp_servers, "server=%s#%u\n", server[unit], YADNS_DNSPORT);
		}
	}
#endif

#ifdef RTCONFIG_IPV6
	if (ipv6_enabled() && is_routing_enabled()) {
		char dnsbuf[INET6_ADDRSTRLEN*3 + 3];
		struct in6_addr addr;

	/* TODO: Skip unconnected wan */

		switch (get_ipv6_service()) {
		case IPV6_NATIVE_DHCP:
#ifdef RTCONFIG_6RELAYD
		case IPV6_PASSTHROUGH:
#endif
			if (nvram_get_int(ipv6_nvname("ipv6_dnsenable"))) {
				wan_dns = nvram_safe_get(ipv6_nvname("ipv6_get_dns"));
				wan_domain = nvram_safe_get(ipv6_nvname("ipv6_get_domain"));
				break;
			}
			/* fall through */
		default:
			next = strcpy(dnsbuf, "");
			for (unit = 1; unit <= 3; unit++) {
				snprintf(tmp, sizeof(tmp), "ipv6_dns%d", unit);
				next += snprintf(next, sizeof(dnsbuf) + dnsbuf - next,
					    *dnsbuf ? " %s" : "%s", nvram_safe_get(ipv6_nvname(tmp)));
			}
			wan_dns = dnsbuf;
			wan_domain = "";
		}

		foreach(tmp, wan_dns, next) {
			if (inet_pton(AF_INET6, tmp, &addr) <= 0)
				continue;
			foreach(domain, wan_domain, next_domain)
				fprintf(fp_servers, "server=/%s/%s\n", domain, tmp);
#ifdef RTCONFIG_YANDEXDNS
			if (yadns_mode != YADNS_DISABLED) {
				fprintf(fp_servers, "server=/%s/%s\n", "local", tmp);
				continue;
			}
#endif
			fprintf(fp_servers, "server=%s\n", tmp);
		}

#ifdef RTCONFIG_YANDEXDNS
		if (yadns_mode != YADNS_DISABLED) {
			char *server[2];
			int count = get_yandex_dns(AF_INET6, yadns_mode, server, sizeof(server)/sizeof(server[0]));
			for (unit = 0; unit < count; unit++) {
				fprintf(fp_servers, "server=%s\n", server[unit]);
				fprintf(fp_servers, "server=%s#%u\n", server[unit], YADNS_DNSPORT);
			}
		}
#endif
	}
#endif

	fclose(fp);
	fclose(fp_servers);
	file_unlock(lock);

#ifdef RTCONFIG_OPENVPN
	if (dnsstrict == 2)
		start_dnsmasq();	// add strict-order
	else
#endif
		reload_dnsmasq();

	return 0;

error:
	file_unlock(lock);
	return -1;
}

#ifdef RTCONFIG_IPV6
void wan6_up(const char *wan_ifname)
{
	char addr6[INET6_ADDRSTRLEN + 4];
	struct in_addr addr4;
	struct in6_addr addr;
	char *gateway;
	int mtu, service = get_ipv6_service();

	if (!wan_ifname || (strlen(wan_ifname) <= 0) ||
		(service == IPV6_DISABLED))
		return;

	switch (service) {
	case IPV6_NATIVE_DHCP:
#ifdef RTCONFIG_6RELAYD
	case IPV6_PASSTHROUGH:
#endif
		ipv6_sysconf(wan_ifname, "accept_ra", 1);
		ipv6_sysconf(wan_ifname, "forwarding", 0);
		break;
	case IPV6_MANUAL:
		ipv6_sysconf(wan_ifname, "accept_ra", 0);
		ipv6_sysconf(wan_ifname, "forwarding", 1);
		break;
	case IPV6_6RD:
		update_6rd_info();
		break;
	}

	set_intf_ipv6_dad(wan_ifname, 0, 1);

	switch (service) {
#ifdef RTCONFIG_6RELAYD
	case IPV6_PASSTHROUGH:
		start_6relayd();
		/* fall through */
#endif
	case IPV6_NATIVE_DHCP:
		start_dhcp6c();
		break;
	case IPV6_MANUAL:
		if (nvram_match(ipv6_nvname("ipv6_ipaddr"), (char*)ipv6_router_address(NULL))) {
			dbG("WAN IPv6 address is the same as LAN IPv6 address!\n");
			break;
		}
		snprintf(addr6, sizeof(addr6), "%s/%d", nvram_safe_get(ipv6_nvname("ipv6_ipaddr")), nvram_get_int(ipv6_nvname("ipv6_prefix_len_wan")));
		eval("ip", "-6", "addr", "add", addr6, "dev", (char *)wan_ifname);
		eval("ip", "-6", "route", "del", "::/0");

		gateway = nvram_safe_get(ipv6_nvname("ipv6_gateway"));
		if (gateway && gateway) {
			eval("ip", "-6", "route", "add", gateway, "dev", (char *)wan_ifname, "metric", "1");
			eval("ip", "-6", "route", "add", "::/0", "via", gateway, "dev", (char *)wan_ifname, "metric", "1");
		} else	eval("ip", "-6", "route", "add", "::/0", "dev", (char *)wan_ifname, "metric", "1");

		/* propagate ipv6 mtu */
		mtu = ipv6_getconf(wan_ifname, "mtu");
		if (mtu)
			ipv6_sysconf(nvram_safe_get("lan_ifname"), "mtu", mtu);
		/* workaround to update ndp entry for now */
		char *ping6_argv[] = {"ping6", "-c", "2", "-I", (char *)wan_ifname, "ff02::1", NULL};
		char *ping6_argv2[] = {"ping6", "-c", "2", nvram_safe_get(ipv6_nvname("ipv6_gateway")), NULL};
		pid_t pid;
		_eval(ping6_argv, NULL, 0, &pid);
		_eval(ping6_argv2, NULL, 0, &pid);

		break;
	case IPV6_6TO4:
	case IPV6_6IN4:
	case IPV6_6RD:
		stop_ipv6_tunnel();
		if (service == IPV6_6TO4) {
			int prefixlen = 16;
			int mask4size = 0;

			/* prefix */
			addr4.s_addr = 0;
			memset(&addr, 0, sizeof(addr));
			inet_aton(get_wanip(), &addr4);
			addr.s6_addr16[0] = htons(0x2002);
			prefixlen = ipv6_mapaddr4(&addr, prefixlen, &addr4, mask4size);
			//addr4.s_addr = htonl(0x00000001);
			//prefixlen = ipv6_mapaddr4(&addr, prefixlen, &addr4, (32 - 16));
			inet_ntop(AF_INET6, &addr, addr6, sizeof(addr6));
			nvram_set(ipv6_nvname("ipv6_prefix"), addr6);
			nvram_set_int(ipv6_nvname("ipv6_prefix_length"), prefixlen);

			/* address */
			addr.s6_addr16[7] |= htons(0x0001);
			inet_ntop(AF_INET6, &addr, addr6, sizeof(addr6));
			nvram_set(ipv6_nvname("ipv6_rtr_addr"), addr6);
		}
		else if (service == IPV6_6RD) {
			int prefixlen = nvram_get_int(ipv6_nvname("ipv6_6rd_prefixlen"));
			int masklen = nvram_get_int(ipv6_nvname("ipv6_6rd_ip4size"));

			/* prefix */
			addr4.s_addr = 0;
			memset(&addr, 0, sizeof(addr));
			inet_aton(get_wanip(), &addr4);
			inet_pton(AF_INET6, nvram_safe_get(ipv6_nvname("ipv6_6rd_prefix")), &addr);
			prefixlen = ipv6_mapaddr4(&addr, prefixlen, &addr4, masklen);
			//addr4.s_addr = htonl(0x00000001);
			//prefixlen = ipv6_mapaddr4(&addr, prefixlen, &addr4, (32 - 1));
			inet_ntop(AF_INET6, &addr, addr6, sizeof(addr6));
			nvram_set(ipv6_nvname("ipv6_prefix"), addr6);
			nvram_set_int(ipv6_nvname("ipv6_prefix_length"), prefixlen);

			/* address */
			addr.s6_addr16[7] |= htons(0x0001);
			inet_ntop(AF_INET6, &addr, addr6, sizeof(addr6));
			nvram_set(ipv6_nvname("ipv6_rtr_addr"), addr6);
		}
		start_ipv6_tunnel();

		/* propagate ipv6 mtu */
		mtu = ipv6_getconf(wan_ifname, "mtu");
		if (mtu)
			ipv6_sysconf(nvram_safe_get("lan_ifname"), "mtu", mtu);
		// FIXME: give it a few seconds for DAD completion
		sleep(2);
		break;
	}

#if 0
	start_ecmh(wan_ifname);
#endif
}

void wan6_down(const char *wan_ifname)
{
	set_intf_ipv6_dad(wan_ifname, 0, 0);
#if 0
	stop_ecmh();
#endif
#ifdef RTCONFIG_6RELAYD
	stop_6relayd();
#endif
	stop_dhcp6c();
	stop_ipv6_tunnel();

	update_resolvconf();
}

void start_wan6(void)
{
	// call wan6_up directly
	char tmp[100];
	char prefix[] = "wanXXXXXXXXXX_";
	char *wan_proto;

	snprintf(prefix, sizeof(prefix), "wan%d_", wan_primary_ifunit_ipv6());
	wan_proto = nvram_get(strcat_r(prefix, "proto", tmp));

	switch (get_ipv6_service()) {
	case IPV6_NATIVE_DHCP:
	case IPV6_MANUAL:
#ifdef RTCONFIG_6RELAYD
	case IPV6_PASSTHROUGH:
#endif
		if (strcmp(wan_proto, "dhcp") != 0 && strcmp(wan_proto, "static") != 0 &&
		    nvram_match(ipv6_nvname("ipv6_ifdev"), "ppp"))
			break;
		/* fall though */
	default:
		wan6_up(get_wan6face());
		break;
	}
}

void stop_wan6(void)
{
	// call wan6_down directly
	wan6_down(get_wan6face());
}

#endif

/**
 * Append netdev to bled or remove netdev from bled.
 * @action:	append or move
 * 	0:	remove
 *  otherwise:	append
 * @wan_unit:
 * @wan_ifname:
 */
static void adjust_netdev_if_of_wan_bled(int action, int wan_unit, char *wan_ifname)
{
#if defined(RTCONFIG_BLINK_LED)
	char *wan_gpio = "led_wan_gpio";
	int (*func)(const char *led_gpio, const char *ifname);

	if (wan_unit < 0 || wan_unit >= WAN_UNIT_MAX || !wan_ifname)
		return;

	if (action)
		func = append_netdev_bled_if;
	else
		func = remove_netdev_bled_if;
#if defined(RTCONFIG_WANPORT2)
	if (wan_unit == 1)
		wan_gpio = "led_wan2_gpio";
#endif
	if (dualwan_unit__usbif(wan_unit)) {
		func(wan_gpio, wan_ifname);
		return;
	}

	if (!(nvram_get_int("boardflags") & 0x100))
		return;

#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2) || \
    defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2)
	/* Nothing to do. */
#else
	if (get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_LAN) {
		func(wan_gpio, wan_ifname);
	}
#endif
#endif
}

void
wan_up(char *wan_ifname)	// oleg patch, replace
{
	char tmp[100];
	char prefix[] = "wanXXXXXXXXXX_";
	char prefix_x[] = "wanXXXXXXXXX_";
	char *wan_proto, *gateway;
	int wan_unit;

	_dprintf("%s(%s)\n", __FUNCTION__, wan_ifname);

	/* Figure out nvram variable name prefix for this i/f */
	if ((wan_unit = wan_ifunit(wan_ifname)) < 0)
	{
		/* called for dhcp+ppp */
		if ((wan_unit = wanx_ifunit(wan_ifname)) < 0)
			return;

		snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);
		snprintf(prefix_x, sizeof(prefix_x), "wan%d_x", wan_unit);

#ifdef RTCONFIG_IPV6
		wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
		if (wan_unit == wan_primary_ifunit_ipv6()) {
			switch (get_ipv6_service()) {
			case IPV6_NATIVE_DHCP:
			case IPV6_MANUAL:
#ifdef RTCONFIG_6RELAYD
			case IPV6_PASSTHROUGH:
#endif
				if (strcmp(wan_proto, "dhcp") != 0 && strcmp(wan_proto, "static") != 0 &&
				    nvram_match(ipv6_nvname("ipv6_ifdev"), "ppp"))
					break;
				/* fall through */
			default:
				wan6_up(get_wan6face());
				break;
			}
		}
#endif

		start_firewall(wan_unit, 0);

		/* setup static wan routes via physical device */
		add_routes(prefix, "mroute", wan_ifname);

		/* and one supplied via DHCP */
		add_dhcp_routes(prefix_x, wan_ifname, 1);

		/* and default route with metric 1 */
		gateway = nvram_safe_get(strcat_r(prefix_x, "gateway", tmp));
		if (inet_addr_(gateway) != INADDR_ANY) {
			char word[100], *next;
			in_addr_t addr = inet_addr(nvram_safe_get(strcat_r(prefix_x, "ipaddr", tmp)));
			in_addr_t mask = inet_addr(nvram_safe_get(strcat_r(prefix_x, "netmask", tmp)));

			/* the gateway is out of the local network */
			if ((inet_addr(gateway) & mask) != (addr & mask))
				route_add(wan_ifname, 2, gateway, NULL, "255.255.255.255");

			/* default route via default gateway */
			route_add(wan_ifname, 2, "0.0.0.0", gateway, "0.0.0.0");

			/* ... and to dns servers as well for demand ppp to work */
			if (nvram_get_int(strcat_r(prefix, "dnsenable_x", tmp))) {
				foreach(word, nvram_safe_get(strcat_r(prefix_x, "dns", tmp)), next) {
					if ((inet_addr(word) != inet_addr(gateway)) &&
					    (inet_addr(word) & mask) != (addr & mask))
						route_add(wan_ifname, 2, word, gateway, "255.255.255.255");
				}
			}
		}

		update_resolvconf();

		/* start multicast router on DHCP+VPN physical interface */
		if (wan_unit == wan_primary_ifunit())
			start_igmpproxy(wan_ifname);

		return;
	}

	snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	gateway = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
	if (inet_addr_(gateway) == INADDR_ANY)
		gateway = NULL;

	/* Set default route to gateway if specified */
	if (strcmp(wan_proto, "dhcp") == 0 || strcmp(wan_proto, "static") == 0) {
		/* the gateway is in the local network */
		if (gateway &&
		    inet_addr_(gateway) != inet_addr_(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)))) {
#ifdef RTCONFIG_MULTICAST_IPTV
			/* Rawny: delete gateway route in IPTV(movistar) case to enable QUAGGA */
			if (nvram_get_int("switch_stb_x") > 6 &&
			    !nvram_match("switch_wantag", "movistar"))
#endif
			route_add(wan_ifname, 0, gateway, NULL, "255.255.255.255");
		}
		/* replaced with add_multi_routes()
		route_add(wan_ifname, 0, "0.0.0.0", gateway, "0.0.0.0"); */
	}

	/* hack: avoid routing cycles, when both peer and server has the same IP */
	if (strcmp(wan_proto, "pptp") == 0 || strcmp(wan_proto, "l2tp") == 0) {
		/* delete gateway route as it's no longer needed */
		if (gateway)
			route_del(wan_ifname, 0, gateway, "0.0.0.0", "255.255.255.255");
	}

	/* Install interface dependent static routes */
	add_wan_routes(wan_ifname);

	nvram_set(strcat_r(prefix, "gw_ifname", tmp), wan_ifname);

	/* setup static wan routes via physical device */
	if (strcmp(wan_proto, "dhcp") == 0 || strcmp(wan_proto, "static") == 0) {
		nvram_set(strcat_r(prefix, "xgateway", tmp), gateway ? : "0.0.0.0");
		add_routes(prefix, "mroute", wan_ifname);
	}

	/* and one supplied via DHCP */
	if (strcmp(wan_proto, "dhcp") == 0)
		add_dhcp_routes(prefix, wan_ifname, 0);

#ifdef RTCONFIG_IPV6
	if (wan_unit == wan_primary_ifunit_ipv6()) {
		switch (get_ipv6_service()) {
		case IPV6_NATIVE_DHCP:
		case IPV6_MANUAL:
#ifdef RTCONFIG_6RELAYD
		case IPV6_PASSTHROUGH:
#endif
			if (strcmp(wan_proto, "dhcp") != 0 && strcmp(wan_proto, "static") != 0 &&
			    nvram_match(ipv6_nvname("ipv6_ifdev"), "ppp"))
				break;
			/* fall through */
		default:
			wan6_up(get_wan6face());
			break;
		}
	}
#endif

#ifdef RTCONFIG_MULTICAST_IPTV
	if (nvram_get_int("switch_stb_x") > 6 &&
	    nvram_match("iptv_ifname", wan_ifname)) {
		if (nvram_match("switch_wantag", "maxis_fiber_iptv"))
			route_add(wan_ifname, 0, "172.17.90.1", NULL, "255.255.255.255");
		start_igmpproxy(wan_ifname);
	}
#ifdef RTCONFIG_QUAGGA
	if( (wan_unit == WAN_UNIT_IPTV) || (wan_unit == WAN_UNIT_VOIP) ){
		stop_quagga();
		start_quagga();
	}
#endif
#endif

#if defined(RTCONFIG_QCA) || \
    (defined(RTCONFIG_RALINK) && !defined(RTCONFIG_DSL) && !defined(RTN13U))
	reinit_hwnat(wan_unit);
#endif

	ctrl_wan_gro(wan_unit, nvram_get_int("qca_gro"));

#if defined(DSL_N55U) || defined(DSL_N55U_B)
	if(nvram_match("wl0_country_code", "GB")) {
		if(isTargetArea()) {
			system("ATE Set_RegulationDomain_2G SG");
			//system("ATE Set_RegulationDomain_5G SG");
		}
	}
#endif

	/* Set connected state */
	update_wan_state(prefix, WAN_STATE_CONNECTED, 0);

	// TODO: handle different lan_ifname?
	start_firewall(wan_unit, 0);
	//start_firewall(wan_ifname, nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
	//	nvram_safe_get("lan_ifname"), nvram_safe_get("lan_ipaddr"));

	/* Start post-authenticator */
	start_auth(wan_unit, 1);

	/* Add dns servers to resolv.conf */
	update_resolvconf();

	/* default route via default gateway */
	add_multi_routes();

#ifdef RTCONFIG_USB_MODEM
#ifdef RTCONFIG_INTERNAL_GOBI
	if(dualwan_unit__usbif(wan_unit)){
		nvram_set("freeze_duck", "5");
		eval("/usr/sbin/modem_status.sh", "rate");
		eval("/usr/sbin/modem_status.sh", "band");
		eval("/usr/sbin/modem_status.sh", "operation");
		eval("/usr/sbin/modem_status.sh", "provider");
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
		eval("/usr/sbin/modem_status.sh", "get_dataset");
		eval("/usr/sbin/modem_status.sh", "bytes");
#endif

		char start_sec[32], *str = file2str("/proc/uptime");
		unsigned int up = atoi(str);

		free(str);
		snprintf(start_sec, 32, "%u", up);
		nvram_set("usb_modem_act_startsec", start_sec);
	}
#endif
#endif

#ifdef RTCONFIG_OPENVPN
	stop_vpn_all();
#endif

#if 0 // unsure changes
	if(wan_unit == wan_primary_ifunit()
#ifdef RTCONFIG_DUALWAN
					|| nvram_match("wans_mode", "lb")
#endif
			){
		if(do_dns_detect(wan_unit))
			nvram_set_int("link_internet", 2);
		else
			nvram_set_int("link_internet", 1);
	}
#endif

#if !defined(RTCONFIG_MULTIWAN_CFG)
	if (wan_unit != wan_primary_ifunit())
	{
#ifdef RTCONFIG_OPENVPN
		start_vpn_eas();
#endif
		return;
	}
#endif
	/* start multicast router when not VPN */
	if (strcmp(wan_proto, "dhcp") == 0 ||
	    strcmp(wan_proto, "static") == 0)
		start_igmpproxy(wan_ifname);

	stop_upnp();
	start_upnp();

	/* Sync time */
	refresh_ntpc();

#ifdef RTCONFIG_OPENVPN
	start_vpn_eas();
#endif

	stop_ddns();
	start_ddns();

#ifdef RTCONFIG_VPNC
	if((nvram_match("vpnc_proto", "pptp") || nvram_match("vpnc_proto", "l2tp")) && nvram_match("vpnc_auto_conn", "1"))
		start_vpnc();
#endif
#ifdef RTCONFIG_IPSEC
	rc_ipsec_config_init();
#endif
#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD)
	if (nvram_get_int("pptpd_enable")) {
		stop_pptpd();
		start_pptpd();
	}
#endif


	adjust_netdev_if_of_wan_bled(1, wan_unit, wan_ifname);

#ifdef RTCONFIG_BWDPI
	int debug = nvram_get_int("bwdpi_debug");
	int enabled = check_bwdpi_nvram_setting();

	if(debug) dbg("[wan up] enabled= %d\n", enabled);

	if(enabled){
		_dprintf("[%s] do dpi engine service ... \n", __FUNCTION__);
		// if Adaptive QoS or AiProtection is enabled
		int count = 0;
		int val = 0;
		while(count < 3){
			sleep(1);
			val = found_default_route(0);
			count++;
			if((val == 1) || (count == 3)) break;
		}

		if(debug) dbg("[wan up] found_default_route result: %d\n", val);

		if(val){
			// if restart_wan_if, remove dpi engine related
			if(f_exists("/dev/detector") || f_exists("/dev/idpfw")){
				_dprintf("[%s] stop dpi engine service\n", __FUNCTION__);
				stop_dpi_engine_service(0);
			}
			_dprintf("[%s] start dpi engine service\n", __FUNCTION__);
			set_codel_patch();
			start_dpi_engine_service();
			start_firewall(wan_unit, 0);
		}

		if(nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") != 1){
			_dprintf("[wan up] tradtional qos or bandwidth limiter start\n");
			start_iQos();
		}
	}
	else{
		if(nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") != 1){
			_dprintf("[wan up] tradtional qos or bandwidth limiter start\n");
			start_iQos();
		}
	}
#else
	start_iQos();
#endif

#if !defined(RTCONFIG_MULTIWAN_CFG)
	/* FIXME: Protect below code from 2-nd WAN temporarilly. */
	if (wan_unit == wan_primary_ifunit()) {
#endif

#ifdef RTCONFIG_TR069
	start_tr();
#endif

#ifdef RTCONFIG_GETREALIP
#ifdef RTCONFIG_DUALWAN
	if(nvram_invmatch("wans_mode", "lb"))
#endif
	{
		char *getip[] = {"getrealip.sh", NULL};
		pid_t pid;

		//_eval(getip, ">>/tmp/log.txt", 0, &pid);
		_eval(getip, ">>/dev/null", 0, &pid);
	}
#endif

#ifdef RTCONFIG_TCPDUMP
	eval("killall", "tcpdump");
#endif

#if !defined(RTCONFIG_MULTIWAN_CFG)
	}
#endif

_dprintf("%s(%s): done.\n", __FUNCTION__, wan_ifname);
}

void
wan_down(char *wan_ifname)
{
	int wan_unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char *wan_proto, *gateway;
	int end_wan_sbstate = WAN_STOPPED_REASON_NONE;

	_dprintf("%s(%s)\n", __FUNCTION__, wan_ifname);

	/* Skip physical interface of VPN connections */
	if ((wan_unit = wan_ifunit(wan_ifname)) < 0)
		return;

	/* Figure out nvram variable name prefix for this i/f */
	if(wan_prefix(wan_ifname, prefix) < 0)
		return;

	_dprintf("%s(%s): %s.\n", __FUNCTION__, wan_ifname, nvram_safe_get(strcat_r(prefix, "dns", tmp)));

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	if(nvram_get_int(strcat_r(prefix, "sbstate_t", tmp)) == WAN_STOPPED_REASON_DATALIMIT)
		end_wan_sbstate = WAN_STOPPED_REASON_DATALIMIT;
#endif

#ifdef RTCONFIG_INTERNAL_GOBI
	if(dualwan_unit__usbif(wan_unit)){
		nvram_unset("usb_modem_act_tx");
		nvram_unset("usb_modem_act_rx");
		nvram_unset("usb_modem_act_band");
		nvram_unset("usb_modem_act_operation");
		nvram_unset("usb_modem_act_provider");

		nvram_unset("usb_modem_act_startsec");
	}
#endif

	adjust_netdev_if_of_wan_bled(0, wan_unit, wan_ifname);

	/* Stop post-authenticator */
	stop_auth(wan_unit, 1);

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if (wan_unit == wan_primary_ifunit()) {
		/* Stop multicast router when not VPN */
		if (strcmp(wan_proto, "dhcp") == 0 ||
		    strcmp(wan_proto, "static") == 0) {
#ifdef RTCONFIG_MULTICAST_IPTV
			if (nvram_get_int("switch_stb_x") > 6 &&
			    !nvram_match("switch_wantag", "movistar"))
#endif
			stop_igmpproxy();
		}

		/* Remove default route to gateway if specified */
		gateway = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
		if (inet_addr_(gateway) == INADDR_ANY)
			gateway = NULL;
		route_del(wan_ifname, 0, "0.0.0.0", gateway, "0.0.0.0");
	}

	/* Remove interface dependent static routes */
	del_wan_routes(wan_ifname);

	if (strcmp(wan_proto, "static") == 0)
		ifconfig(wan_ifname, IFUP, NULL, NULL);

	update_wan_state(prefix, WAN_STATE_DISCONNECTED, end_wan_sbstate);

	/* Update resolv.conf
	 * Leave as is if no dns servers left for demand to work */
	if (*nvram_safe_get(strcat_r(prefix, "xdns", tmp)))
		nvram_unset(strcat_r(prefix, "dns", tmp));
	update_resolvconf();

#ifdef RTCONFIG_DUALWAN
	if(nvram_match("wans_mode", "lb"))
		add_multi_routes();
#endif

#ifdef RTCONFIG_GETREALIP
#ifdef RTCONFIG_DUALWAN
	if(nvram_invmatch("wans_mode", "lb"))
#endif
	{
		nvram_set(strcat_r(prefix, "realip_state", tmp), "0");
		nvram_set(strcat_r(prefix, "realip_ip", tmp), "");
	}
#endif
}

int
wan_ifunit(char *wan_ifname)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	if ((unit = ppp_ifunit(wan_ifname)) >= 0) {
		return unit;
	} else {
		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++) {
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			if (nvram_match(strcat_r(prefix, "ifname", tmp), wan_ifname) &&
			    (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") ||
			     nvram_match(strcat_r(prefix, "proto", tmp), "static")))
				return unit;
		}
#ifdef RTCONFIG_MULTICAST_IPTV
		if (nvram_get_int("switch_stb_x") > 6) {
			for (unit = WAN_UNIT_IPTV; unit < WAN_UNIT_MULTICAST_IPTV_MAX; unit++) {
				snprintf(prefix, sizeof(prefix), "wan%d_", unit);
				if (nvram_match(strcat_r(prefix, "ifname", tmp), wan_ifname) &&
				    (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") ||
				     nvram_match(strcat_r(prefix, "proto", tmp), "static")))
					return unit;
			}
		}
#endif
	}

	return -1;
}

int
wanx_ifunit(char *wan_ifname)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit ++) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		if (nvram_match(strcat_r(prefix, "ifname", tmp), wan_ifname) &&
		    (nvram_match(strcat_r(prefix, "proto", tmp), "pppoe") ||
		     nvram_match(strcat_r(prefix, "proto", tmp), "pptp") ||
		     nvram_match(strcat_r(prefix, "proto", tmp), "l2tp")))
			return unit;
	}
#ifdef RTCONFIG_MULTICAST_IPTV
	if (nvram_get_int("switch_stb_x") > 6) {
		for (unit = WAN_UNIT_IPTV; unit < WAN_UNIT_MULTICAST_IPTV_MAX; unit++) {
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			if (nvram_match(strcat_r(prefix, "ifname", tmp), wan_ifname) &&
			    (nvram_match(strcat_r(prefix, "proto", tmp), "pppoe") ||
			     nvram_match(strcat_r(prefix, "proto", tmp), "pptp") ||
			     nvram_match(strcat_r(prefix, "proto", tmp), "l2tp")))
				return unit;
		}
	}
#endif
	return -1;
}

int
preset_wan_routes(char *wan_ifname)
{
	int unit = -1;

	if((unit = wan_ifunit(wan_ifname)) < 0)
		if((unit = wanx_ifunit(wan_ifname)) < 0)
			return -1;

	/* Set default route to gateway if specified */
	if(unit == wan_primary_ifunit())
		route_add(wan_ifname, 0, "0.0.0.0", "0.0.0.0", "0.0.0.0");

	/* Install interface dependent static routes */
	add_wan_routes(wan_ifname);
	return 0;
}

char *
get_lan_ipaddr()
{
	int s;
	struct ifreq ifr;
	struct sockaddr_in *inaddr;
	struct in_addr ip_addr;

	/* Retrieve IP info */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
#if 0
		return strdup("0.0.0.0");
#else
	{
		memset(&ip_addr, 0x0, sizeof(ip_addr));
		return inet_ntoa(ip_addr);
	}
#endif

	strncpy(ifr.ifr_name, "br0", IFNAMSIZ);
	inaddr = (struct sockaddr_in *)&ifr.ifr_addr;
	inet_aton("0.0.0.0", &inaddr->sin_addr);

	/* Get IP address */
	ioctl(s, SIOCGIFADDR, &ifr);
	close(s);

	ip_addr = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
//	fprintf(stderr, "current LAN IP address: %s\n", inet_ntoa(ip_addr));
	return inet_ntoa(ip_addr);
}

int
ppp0_as_default_route()
{
	int i, n, found;
	FILE *f;
	unsigned int dest, mask;
	char buf[256], device[256];

	n = 0;
	found = 0;
	mask = 0;
	device[0] = '\0';

	if ((f = fopen("/proc/net/route", "r")) != NULL)
	{
		while (fgets(buf, sizeof(buf), f) != NULL)
		{
			if (++n == 1 && strncmp(buf, "Iface", 5) == 0)
				continue;

			i = sscanf(buf, "%255s %x %*s %*s %*s %*s %*s %x",
						device, &dest, &mask);

			if (i != 3)
				break;

			if (device[0] != '\0' && dest == 0 && mask == 0)
			{
				found = 1;
				break;
			}
		}

		fclose(f);

		if (found && !strcmp("ppp0", device))
			return 1;
		else
			return 0;
	}

	return 0;
}

int
found_default_route(int wan_unit)
{
	int i, n, found;
	FILE *f;
	unsigned int dest, mask;
	char buf[256], device[256];
	char *wanif;

	if(wan_unit != wan_primary_ifunit())
		return 1;

	n = 0;
	found = 0;
	mask = 0;
	device[0] = '\0';

	if ((f = fopen("/proc/net/route", "r")) != NULL)
	{
		while (fgets(buf, sizeof(buf), f) != NULL)
		{
			if (++n == 1 && strncmp(buf, "Iface", 5) == 0)
				continue;

				i = sscanf(buf, "%255s %x %*s %*s %*s %*s %*s %x",
					device, &dest, &mask);

			if (i != 3)
			{
				break;
			}

			if (device[0] != '\0' && dest == 0 && mask == 0)
			{
				wanif = get_wan_ifname(wan_unit);
				if (!strcmp(wanif, device))
				{
					found = 1;
					break;
				}
			}
		}

		fclose(f);

		if (found)
		{
			return 1;
		}
	}

	_dprintf("\nNO default route!!!\n");

	return 0;
}

long print_num_of_connections()
{
	char buf[256];
	char entries[16], others[256];
	long num_of_entries;

	FILE *fp = fopen("/proc/net/stat/nf_conntrack", "r");
	if (!fp) {
		fprintf(stderr, "no connection!\n");
		return 0;
	}

	fgets(buf, 256, fp);
	fgets(buf, 256, fp);
	fclose(fp);

	memset(entries, 0x0, 16);
	sscanf(buf, "%s %s", entries, others);
	num_of_entries = strtoul(entries, NULL, 16);

	fprintf(stderr, "connection count: %ld\n", num_of_entries);
	return num_of_entries;
}

void
start_wan(void)
{
#ifdef RTCONFIG_DUALWAN
	int unit;
#endif

#ifdef RTCONFIG_DSL
	dsl_configure(1);
#endif

	if (!is_routing_enabled())
		return;

	/* Create links */
	mkdir("/tmp/ppp", 0777);
	mkdir("/tmp/ppp/peers", 0777);
	symlink("/sbin/rc", "/tmp/ppp/ip-up");
	symlink("/sbin/rc", "/tmp/ppp/ip-down");
	symlink("/sbin/rc", "/tmp/ppp/ip-pre-up");
#ifdef RTCONFIG_IPV6
	symlink("/sbin/rc", "/tmp/ppp/ipv6-up");
	symlink("/sbin/rc", "/tmp/ppp/ipv6-down");
	symlink("/sbin/rc", "/tmp/dhcp6c");
#endif
	symlink("/sbin/rc", "/tmp/ppp/auth-fail");
#ifdef RTCONFIG_VPNC
	symlink("/sbin/rc", "/tmp/ppp/vpnc-ip-up");
	symlink("/sbin/rc", "/tmp/ppp/vpnc-ip-down");
	symlink("/sbin/rc", "/tmp/ppp/vpnc-ip-pre-up");
	symlink("/sbin/rc", "/tmp/ppp/vpnc-auth-fail");
#endif
	symlink("/sbin/rc", "/tmp/udhcpc");
	symlink("/sbin/rc", "/tmp/zcip");
#ifdef RTCONFIG_EAPOL
	symlink("/sbin/rc", "/tmp/wpa_cli");
#endif
//	symlink("/dev/null", "/tmp/ppp/connect-errors");

	reinit_hwnat(-1);

#ifdef RTCONFIG_DUALWAN
	/* Start each configured and enabled wan connection and its undelying i/f */
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
		if(dualwan_unit__nonusbif(unit)){
			if(unit == wan_primary_ifunit() || nvram_match("wans_mode", "lb")){
				_dprintf("%s: start_wan_if(%d)!\n", __FUNCTION__, unit);
				start_wan_if(unit);
			}
		}
	}

#ifdef RTCONFIG_MULTICAST_IPTV
	/* Start each configured and enabled wan connection and its undelying i/f */
	if (nvram_get_int("switch_stb_x") > 6) {
		for (unit = WAN_UNIT_IPTV; unit < WAN_UNIT_MULTICAST_IPTV_MAX; ++unit) {
			_dprintf("%s: start_wan_if(%d)!\n", __FUNCTION__, unit);
			start_wan_if(unit);
		}
	}
#endif
#else
	_dprintf("%s: start_wan_if(%d)!\n", __FUNCTION__, WAN_UNIT_FIRST);
	start_wan_if(WAN_UNIT_FIRST);

#ifdef RTCONFIG_USB_MODEM
	if(is_usb_modem_ready() == 1 && nvram_get_int("success_start_service") == 1){
		_dprintf("%s: start_wan_if(%d)!\n", __FUNCTION__, WAN_UNIT_SECOND);
		start_wan_if(WAN_UNIT_SECOND);
	}
#endif
#endif

#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(2,6,36)
	f_write_string("/proc/sys/net/bridge/bridge-nf-call-iptables", "0", 0, 0);
	f_write_string("/proc/sys/net/bridge/bridge-nf-call-ip6tables", "0", 0, 0);
#endif

	/* Report stats */
	if (*nvram_safe_get("stats_server")) {
		char *stats_argv[] = { "stats", nvram_safe_get("stats_server"), NULL };
		_eval(stats_argv, NULL, 5, NULL);
	}
}

void
stop_wan(void)
{
	int unit;

	if (!is_routing_enabled())
		return;

#ifdef RTCONFIG_RALINK
	if (module_loaded("hw_nat"))
	{
		modprobe_r("hw_nat");
		if (!g_reboot)
			sleep(1);
	}
#endif

#ifdef RTCONFIG_IPV6
	stop_wan6();
#endif

	/* Start each configured and enabled wan connection and its undelying i/f */
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
		stop_wan_if(unit);

#ifdef RTCONFIG_OPENVPN
	stop_vpn_eas();
#endif

#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD)
	if (nvram_get_int("pptpd_enable"))
		stop_pptpd();
#endif

	/* Remove dynamically created links */
#ifdef RTCONFIG_EAPOL
	unlink("/tmp/wpa_cli");
#endif
	unlink("/tmp/udhcpc");
	unlink("/tmp/zcip");
	unlink("/tmp/ppp/ip-up");
	unlink("/tmp/ppp/ip-down");
	unlink("/tmp/ppp/ip-pre-up");
#ifdef RTCONFIG_IPV6
	unlink("/tmp/ppp/ipv6-up");
	unlink("/tmp/ppp/ipv6-down");
	unlink("/tmp/dhcp6c");
#endif
	unlink("/tmp/ppp/auth-fail");
#ifdef RTCONFIG_VPNC
	unlink("/tmp/ppp/vpnc-ip-up");
	unlink("/tmp/ppp/vpnc-ip-down");
	unlink("/tmp/ppp/vpnc-ip-pre-up");
	unlink("/tmp/ppp/vpnc-auth-fail");
#endif
	rmdir("/tmp/ppp");
}

void convert_wan_nvram(char *prefix, int unit)
{
	int mac_clone = 0;
	char tmp[100];
	char macbuf[32];
#if defined(CONFIG_BCMWL5) && defined(RTCONFIG_RGMII_BRCM5301X)
	char hwaddr_5g[18];
#endif

	_dprintf("%s(%s)\n", __FUNCTION__, prefix);

	// setup hwaddr
	strcpy(macbuf, nvram_safe_get(strcat_r(prefix, "hwaddr_x", tmp)));
	if (strlen(macbuf)!=0 && strcasecmp(macbuf, "FF:FF:FF:FF:FF:FF")){
		mac_clone = 1;
		nvram_set(strcat_r(prefix, "hwaddr", tmp), macbuf);
		logmessage("wan", "mac clone: [%s] == [%s]\n", tmp, macbuf);
	}
#if defined(CONFIG_BCMWL5) && defined(RTCONFIG_RGMII_BRCM5301X)
	else{
		/* QTN */
		if(strcmp(prefix, "wan1_") == 0){
			strcpy(hwaddr_5g, get_wan_mac_name());
			inc_mac(hwaddr_5g, 7);
			nvram_set(strcat_r(prefix, "hwaddr", tmp), hwaddr_5g);
			logmessage("wan", "[%s] == [%s]\n", tmp, hwaddr_5g);
		}else{
			nvram_set(strcat_r(prefix, "hwaddr", tmp), nvram_safe_get("lan_hwaddr"));
			logmessage("wan", "[%s] == [%s]\n", tmp, nvram_safe_get("lan_hwaddr"));
		}
	}
#else
	else nvram_set(strcat_r(prefix, "hwaddr", tmp), get_wan_hwaddr());
#endif /* CONFIG_BCMWL5 && RTCONFIG_RGMII_BRCM5301X */

#if defined(RTCONFIG_DUALWAN)
	if (!mac_clone && unit > 0) {
		unsigned char eabuf[ETHER_ADDR_LEN];
		char macaddr[32];

		/* Don't use same MAC address on all WANx interfaces. */
		ether_atoe(nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)), eabuf);
		eabuf[ETHER_ADDR_LEN - 1] += unit;
		ether_etoa(eabuf, macaddr);
		nvram_set(strcat_r(prefix, "hwaddr", tmp), macaddr);
	}
#endif

#ifdef RTCONFIG_MULTICAST_IPTV
	if (nvram_get_int("switch_stb_x") > 6 &&
	    unit > 9) {
		unsigned char ea[6];
		ether_atoe(nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)), ea);
		ea[5] = (ea[5] & 0xf0) | ((ea[5] + unit - 9) & 0x0f);
		ether_etoa(ea, macbuf);
		nvram_set(strcat_r(prefix, "hwaddr", tmp), macbuf);
	}
#endif
	// sync proto
	if (nvram_match(strcat_r(prefix, "proto", tmp), "static"))
		nvram_set_int(strcat_r(prefix, "dhcpenable_x", tmp), 0);
	// backlink unit for ppp
	nvram_set_int(strcat_r(prefix, "unit", tmp), unit);
}

void dumparptable()
{
	char buf[256];
	char ip_entry[32], hw_type[8], flags[8], hw_address[32], mask[32], device[8];
	char macbuf[32];

	FILE *fp = fopen("/proc/net/arp", "r");
	if (!fp) {
		fprintf(stderr, "no proc fs mounted!\n");
		return;
	}

	mac_num = 0;

	while (fgets(buf, 256, fp) && (mac_num < MAX_MAC_NUM - 2)) {
		sscanf(buf, "%s %s %s %s %s %s", ip_entry, hw_type, flags, hw_address, mask, device);

		if (!strcmp(device, "br0") && strlen(hw_address)!=0)
		{
			strcpy(mac_clone[mac_num++], hw_address);
		}
	}
	fclose(fp);

	strcpy(macbuf, nvram_safe_get("wan0_hwaddr_x"));

	// try pre-set mac
	if (strlen(macbuf)!=0 && strcasecmp(macbuf, "FF:FF:FF:FF:FF:FF"))
		strcpy(mac_clone[mac_num++], macbuf);

	// try original mac
	strcpy(mac_clone[mac_num++], get_lan_hwaddr());

	if (mac_num)
	{
		fprintf(stderr, "num of mac: %d\n", mac_num);
		int i;
		for (i = 0; i < mac_num; i++)
			fprintf(stderr, "mac to clone: %s\n", mac_clone[i]);
	}
}

#ifdef RTCONFIG_QCA_PLC_UTILS
#define PLCTOOL_M	"/tmp/plctool-m"
int autodet_plc_main(int argc, char *argv[]){
	FILE *fd;
	char buf[64], *ptr1, *ptr2;
	char ifname[8], plc_mac[18], resp_mac[18];
	int found = 0, idx = 0, chk = 0, cnt = 0;

	memset(ifname, 0, sizeof(ifname));
	strcpy(ifname, nvram_safe_get("lan_ifname"));
	memset(plc_mac, 0, sizeof(plc_mac));
	strcpy(plc_mac, nvram_safe_get("plc_macaddr"));
	memset(resp_mac, 0, sizeof(resp_mac));

	sprintf(buf, "/usr/local/bin/plctool -i %s -m -e > %s", ifname, PLCTOOL_M);
	system(buf);

	if ((fd = fopen(PLCTOOL_M, "r")) < 0) {
		_dprintf("%s: Can't open %s\n", __func__, PLCTOOL_M);
		return -1;
	}

	// parse content
	while (fgets(buf, sizeof(buf), fd) != NULL) {
		// check response MAC
		if (!found && (ptr1 = strstr(buf, ifname))) {
			ptr2 = strstr(buf, " Found ");
			if (ptr2) {
				idx++;
				ptr1 += strlen(ifname) + 1;
				strncpy(resp_mac, ptr1, 17);
				if (!strncmp(resp_mac, plc_mac, 17))
					found = 1;
				continue;
			}
		}

		// check amount of member
		if (strstr(buf, "network->TEI")) {
			idx--;
			if (idx == 0) {
				cnt++;
				chk = 1;
			} else
				chk = 0;
			continue;
		}
		if (chk && (ptr1 = strstr(buf, "network->STATIONS"))) {
			ptr1 += strlen("network->STATIONS") + 3;
			cnt += atoi(ptr1);
		}
	}

	fclose(fd);
	unlink(PLCTOOL_M);
	nvram_set_int("autodet_plc_state" , cnt);

	return 0;
}	
#endif

int autodet_main(int argc, char *argv[]){
	int i;
	int unit;
	char prefix[]="wanXXXXXX_", tmp[100];
	char prefix2[]="autodetXXXXXX_", tmp2[100];
	char hwaddr_x[32];
	int status;

	f_write_string("/tmp/detect_wrong.log", "", 0, 0);
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
		if(get_dualwan_by_unit(unit) != WANS_DUALWAN_IF_WAN && get_dualwan_by_unit(unit) != WANS_DUALWAN_IF_LAN)
			continue;

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		if(unit == WAN_UNIT_FIRST)
			snprintf(prefix2, sizeof(prefix2), "autodet_");
		else
			snprintf(prefix2, sizeof(prefix2), "autodet%d_", unit);

		nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_INITIALIZING);
		nvram_set_int(strcat_r(prefix2, "auxstate", tmp2), AUTODET_STATE_INITIALIZING);

		// it shouldnot happen, because it is only called in default mode
		if(!nvram_match(strcat_r(prefix, "proto", tmp), "dhcp")){
			status = discover_all(unit);
			if(status == -1)
				nvram_set_int(strcat_r(prefix2, "auxstate", tmp2), AUTODET_STATE_FINISHED_NOLINK);
			else if(status == 2)
				nvram_set_int(strcat_r(prefix2, "auxstate", tmp2), AUTODET_STATE_FINISHED_WITHPPPOE);
			else
				nvram_set_int(strcat_r(prefix2, "auxstate", tmp2), AUTODET_STATE_FINISHED_OK);

			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_NODHCP);
			continue;
		}

		// TODO: need every unit of WAN to do the autodet?
		if(!get_wanports_status(unit)){
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_NOLINK);
			continue;
		}

		if(get_wan_state(unit) == WAN_STATE_CONNECTED){
			i = nvram_get_int(strcat_r(prefix, "lease", tmp));

			if(i < 60 && is_private_subnet(strcat_r(prefix, "ipaddr", tmp))){
				sleep(i);
			}
			//else{
			//	nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_OK);
			//	continue;
			//}
		}

		status = discover_all(unit);

		// check for pppoe status only,
		if(get_wan_state(unit) == WAN_STATE_CONNECTED){
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_OK);
			if(status == 2)
				nvram_set_int(strcat_r(prefix2, "auxstate", tmp2), AUTODET_STATE_FINISHED_WITHPPPOE);
			continue;
		}
		else if(status == 2){
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_WITHPPPOE);
			continue;
		}
		else if(is_ip_conflict(unit)){
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_OK);
			continue;
		}

		nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_CHECKING);

		dumparptable();

		// backup hwaddr_x
		strcpy(hwaddr_x, nvram_safe_get(strcat_r(prefix, "hwaddr_x", tmp)));
		//nvram_set(strcat_r(prefix, "hwaddr_x", tmp), "");

		char *ptr = nvram_safe_get(strcat_r(prefix2, "waitsec", tmp2));
		int waitsec = 0;

		if(ptr == NULL || strlen(ptr) <= 0)
			waitsec = 5;
		else
			waitsec = atoi(ptr);

		i = 0;
		while(i < mac_num && (!is_wan_connect(unit) && !is_ip_conflict(unit))){
			if(!(nvram_match("wl0_country_code", "SG")) && 
			   strncmp(nvram_safe_get("territory_code"), "SG", 2) != 0){ // Singpore do not auto clone
				_dprintf("try clone %s\n", mac_clone[i]);
				nvram_set(strcat_r(prefix, "hwaddr_x", tmp), mac_clone[i]);
			}
			char buf[32];
			snprintf(buf, 32, "restart_wan_if %d", unit);
			notify_rc_and_wait(buf);
			_dprintf("%s: wait a IP during %d seconds...\n", __FUNCTION__, waitsec);
			int count = 0;
			while(count < waitsec && (!is_wan_connect(unit) && !is_ip_conflict(unit))){
				sleep(1);

				++count;
			}
			++i;
		}

		if(i == mac_num){
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_FAIL);
			// restore hwaddr_x
			nvram_set(strcat_r(prefix, "hwaddr_x", tmp), hwaddr_x);
		}
		else if(i == mac_num-1){ // OK in original mac
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_OK);
		}
		else{ // OK in cloned mac
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_OK);
		}
		nvram_commit();
	}

	return 0;
}
