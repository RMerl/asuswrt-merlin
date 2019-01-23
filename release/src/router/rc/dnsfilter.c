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
 *
 * Copyright 2014-2017 Eric Sauvageau.
 *
 */

#include <rc.h>
#include <net/ethernet.h>


#ifdef RTCONFIG_DNSFILTER

int get_dns_filter(int proto, int mode, char **server);


// ARG: server must be an array of two pointers, each pointing to an array of chars
int get_dns_filter(int proto, int mode, char **server)
{
	int count = 0;
	char *server_table[13][2] = {
		{ "", "" },			/* 0: Unfiltered */
		{ "208.67.222.222", "" },	/* 1: OpenDNS */
		{ "199.85.126.10", "" },	/* 2: Norton Connect Safe A (Security) */
		{ "199.85.126.20", "" },	/* 3: Norton Connect Safe B (Security + Adult) */
		{ "199.85.126.30", "" },	/* 4: Norton Connect Safe C (Sec. + Adult + Violence */
		{ "77.88.8.88", "" },		/* 5: Secure Mode safe.dns.yandex.ru */
		{ "77.88.8.7", "" },		/* 6: Family Mode family.dns.yandex.ru */
		{ "208.67.222.123", "" },	/* 7: OpenDNS Family Shield */
		{ nvram_safe_get("dnsfilter_custom1"), "" },		/* 8: Custom1 */
		{ nvram_safe_get("dnsfilter_custom2"), "" },		/* 9: Custom2 */
		{ nvram_safe_get("dnsfilter_custom3"), "" },		/* 10: Custom3 */
		{ nvram_safe_get("dhcp_dns1_x"), "" },			/* 11: Router */
		{ "8.26.56.26", "" }		/* 12: Comodo Secure DNS */
        };
#ifdef RTCONFIG_IPV6
	char *server6_table[][2] = {
		{"", ""},		/* 0: Unfiltered */
		{"", ""},		/* 1: OpenDNS */
		{"", ""},		/* 2: Norton Connect Safe A (Security) */
		{"", ""},		/* 3: Norton Connect Safe B (Security + Adult) */
		{"", ""},		/* 4: Norton Connect Safe C (Sec. + Adult + Violence */
		{"2a02:6b8::feed:bad","2a02:6b8:0:1::feed:bad"},		/* 5: Secure Mode safe.dns.yandex.ru */
		{"2a02:6b8::feed:a11","2a02:6b8:0:1::feed:a11"},		/* 6: Family Mode family.dns.yandex.ru */
		{"", ""},		/* 7: OpenDNS Family Shield */
		{"", ""},		/* 8: Custom1 - not supported yet */
		{"", ""},		/* 9: Custom2 - not supported yet */
		{"", ""},		/* 10: Custom3 - not supported yet */
		{nvram_safe_get("dhcp_dns1_x"), ""},		/* 11: Router */
		{"", ""}		/* 12: Comodo Secure DNS */
        };
#endif

// Initialize
	server[0] = server_table[0][0];
	server[1] = server_table[0][1];

	if (mode >= (sizeof(server_table)/sizeof(server_table[0]))) mode = 0;

#ifdef RTCONFIG_IPV6
	if (proto == AF_INET6) {
		server[0] = server6_table[mode][0];
		server[1] = server6_table[mode][1];
	} else
#endif
	{
		server[0] = server_table[mode][0];
		server[1] = server_table[mode][1];
	}

// Ensure that custom and DHCP-provided DNS do contain something
	if (((mode == 8) || (mode == 9) || (mode == 10) || (mode == 11)) && (!strlen(server[0])) && (proto == AF_INET)) {
		server[0] = nvram_safe_get("lan_ipaddr");
	}

// Report how many non-empty server we are returning
	if (strlen(server[0])) count++;
	if (strlen(server[1])) count++;
	return count;
}


void dnsfilter_settings(FILE *fp, char *lan_ip) {
	char *name, *mac, *mode, *server[2];
	unsigned char ea[ETHER_ADDR_LEN];
	char *nv, *nvp, *rule;
	char lan_class[32];
	int dnsmode;

	if (nvram_get_int("dnsfilter_enable_x")) {
		/* Reroute all DNS requests from LAN */
		ip2class(lan_ip, nvram_safe_get("lan_netmask"), lan_class);
		fprintf(fp, "-A PREROUTING -s %s -p udp -m udp --dport 53 -j DNSFILTER\n"
			    "-A PREROUTING -s %s -p tcp -m tcp --dport 53 -j DNSFILTER\n",
			lan_class, lan_class);

		/* Protection level per client */
		nv = nvp = strdup(nvram_safe_get("dnsfilter_rulelist"));
		while (nv && (rule = strsep(&nvp, "<")) != NULL) {
			if (vstrsep(rule, ">", &name, &mac, &mode) != 3)
				continue;
			if (!*mac || !*mode || !ether_atoe(mac, ea))
				continue;
			dnsmode = atoi(mode);
			if (dnsmode == 0) {
				fprintf(fp,
					"-A DNSFILTER -m mac --mac-source %s -j RETURN\n",
					mac);
			} else if (get_dns_filter(AF_INET, dnsmode, server)) {
					fprintf(fp,"-A DNSFILTER -m mac --mac-source %s -j DNAT --to-destination %s\n",
						mac, server[0]);
			}
		}

		/* Send other queries to the default server */
		dnsmode = nvram_get_int("dnsfilter_mode");
		if ((dnsmode) && get_dns_filter(AF_INET, dnsmode, server)) {
			fprintf(fp, "-A DNSFILTER -j DNAT --to-destination %s\n", server[0]);
		}
	}
}


#ifdef RTCONFIG_IPV6
void dnsfilter6_settings(FILE *fp, char *lan_if, char *lan_ip) {
	char *nv, *nvp, *rule;
	char *name, *mac, *mode, *server[2];
	unsigned char ea[ETHER_ADDR_LEN];
	int dnsmode, count;

	fprintf(fp, "-A INPUT -i %s -p udp -m udp --dport 53 -j DNSFILTERI\n"
		    "-A INPUT -i %s -p tcp -m tcp --dport 53 -j DNSFILTERI\n"
		    "-A FORWARD -i %s -p udp -m udp --dport 53 -j DNSFILTERF\n"
		    "-A FORWARD -i %s -p tcp -m tcp --dport 53 -j DNSFILTERF\n",
		lan_if, lan_if, lan_if, lan_if);

	nv = nvp = strdup(nvram_safe_get("dnsfilter_rulelist"));
	while (nv && (rule = strsep(&nvp, "<")) != NULL) {
		if (vstrsep(rule, ">", &name, &mac, &mode) != 3)
			continue;
		dnsmode = atoi(mode);
		if (!*mac || !ether_atoe(mac, ea))
			continue;
		if (dnsmode == 0) {	// Unfiltered
			fprintf(fp, "-A DNSFILTERI -m mac --mac-source %s -j ACCEPT\n"
				    "-A DNSFILTERF -m mac --mac-source %s -j ACCEPT\n", 
					mac, mac);
		} else {	// Filtered
			count = get_dns_filter(AF_INET6, dnsmode, server);
			fprintf(fp, "-A DNSFILTERI -m mac --mac-source %s -j DROP\n", mac);
			if (count) {
				fprintf(fp, "-A DNSFILTERF -m mac --mac-source %s -d %s -j ACCEPT\n", mac, server[0]);
			}
			if (count == 2) {
				fprintf(fp, "-A DNSFILTERF -m mac --mac-source %s -d %s -j ACCEPT\n", mac, server[1]);
			}
		}
	}
	free(nv);

	dnsmode = nvram_get_int("dnsfilter_mode");
	if (dnsmode) {
		/* Allow other queries to the default server, and drop the rest */
		count = get_dns_filter(AF_INET6, dnsmode, server);
		if (count) {
			fprintf(fp, "-A DNSFILTERI -d %s -j ACCEPT\n"
				    "-A DNSFILTERF -d %s -j ACCEPT\n",
				server[0], server[0]);
		}
		if (count == 2) {
			fprintf(fp, "-A DNSFILTERI -d %s -j ACCEPT\n"
				    "-A DNSFILTERF -d %s -j ACCEPT\n",
				server[1], server[1]);
		}
		fprintf(fp, "-A DNSFILTERI -j %s\n"
			    "-A DNSFILTERF -j DROP\n",
		            (dnsmode == 11 ? "ACCEPT" : "DROP"));
	}
}


void dnsfilter_setup_dnsmasq(FILE *fp) {

	unsigned char ea[ETHER_ADDR_LEN];
	char *name, *mac, *mode, *enable, *server[2];
	char *nv, *nvp, *b;
	int count, dnsmode, defmode;

	defmode = nvram_get_int("dnsfilter_mode");

	for (dnsmode = 1; dnsmode < 13; dnsmode++) {
		if (dnsmode == defmode)
			continue;
		count = get_dns_filter(AF_INET6, dnsmode, server);
		if (count == 0)
			continue;
		fprintf(fp, "dhcp-option=dnsf%u,option6:23,[%s]", dnsmode, server[0]);
		if (count == 2)
			fprintf(fp, ",[%s]", server[1]);
		fprintf(fp, "\n");
	}

	/* DNS server per client */
	nv = nvp = strdup(nvram_safe_get("dnsfilter_rulelist"));
	while (nv && (b = strsep(&nvp, "<")) != NULL) {
		if (vstrsep(b, ">", &name, &mac, &mode, &enable) < 3)
			continue;
		if (enable && atoi(enable) == 0)
			continue;
		if (!*mac || !*mode || !ether_atoe(mac, ea))
			continue;
		dnsmode = atoi(mode);
		/* Skip unfiltered, default, or non-IPv6 capable levels */
		if ((dnsmode == 0) || (dnsmode == defmode) || (get_dns_filter(AF_INET6, dnsmode, server) == 0))
			continue;
		fprintf(fp, "dhcp-host=%s,set:dnsf%u\n", mac, dnsmode);
	}
	free(nv);
}
#endif	// RTCONFIG_IPV6
#endif	// RTCONFIG_DNSFILTER
