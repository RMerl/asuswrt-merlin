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
#include <arpa/inet.h>
#include <httpd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <dirent.h>

#include <typedefs.h>
#include <bcmutils.h>
#include <shutils.h>
#include <bcmnvram.h>
#include <bcmnvram_f.h>
#include <common.h>
#include <shared.h>
#include <rtstate.h>
#include <wlioctl.h>

#include <wlutils.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <linux/version.h>

#include "data_arrays.h"
#include "httpd.h"


int
ej_get_leases_array(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	struct in_addr addr4;
	struct in6_addr addr6;
	char line[256];
	char *hwaddr, *ipaddr, *name, *next;
	unsigned int expires;
	int ret=0;

	killall("dnsmasq", SIGUSR2);
	sleep(1);

	if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r")))
		return websWrite(wp, "leasearray = [];\n");

	ret += websWrite(wp, "leasearray= [");

	while ((next = fgets(line, sizeof(line), fp)) != NULL) {
		/* line should start with a numeric value */
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		hwaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		name = strsep(&next, " ") ? : "";

		if (strlen(name) > 32) {
			strcpy(name + 29, "...");
			name[32] = '\0';
		}

		if (inet_pton(AF_INET6, ipaddr, &addr6) != 0) {
			/* skip ipv6 leases, they have no hwaddr, but client id */
			// hwaddr = next ? : "";
			continue;

		} else if (inet_pton(AF_INET, ipaddr, &addr4) == 0)
			continue;

		ret += websWrite(wp, "[\"%d\", \"%s\", \"%s\", \"%s\"],\n", expires, hwaddr, ipaddr, name);
	}
	ret += websWrite(wp, "[]];\n");
	fclose(fp);

	return ret;
}


#ifdef RTCONFIG_IPV6
#ifdef RTCONFIG_IGD2
int
ej_ipv6_pinhole_array(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	char *ipt_argv[] = {"ip6tables", "-nxL", "UPNP", NULL};
	char line[256], tmp[256];
	char target[16], proto[16];
	char src[45];
	char dst[45];
	char *sport, *dport, *ptr, *val;
	int ret = 0;

	ret += websWrite(wp, "var pinholes = ");

        if (!(ipv6_enabled() && is_routing_enabled())) {
                ret += websWrite(wp, "[];\n");
                return ret;
        }

	_eval(ipt_argv, ">/tmp/pinhole.log", 10, NULL);

	fp = fopen("/tmp/pinhole.log", "r");
	if (fp == NULL) {
		ret += websWrite(wp, "[];\n");
		return ret;
	}

	ret += websWrite(wp, "[");

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		tmp[0] = '\0';
		if (sscanf(line,
		    "%15s%*[ \t]"		// target
		    "%15s%*[ \t]"		// prot
		    "%44[^/]/%*d%*[ \t]"	// source
		    "%44[^/]/%*d%*[ \t]"	// destination
		    "%255[^\n]",		// options
		    target, proto, src, dst, tmp) < 5) continue;

		if (strcmp(target, "ACCEPT")) continue;

		/* uppercase proto */
		for (ptr = proto; *ptr; ptr++)
			*ptr = toupper(*ptr);

		/* parse source */
		if (strcmp(src, "::") == 0)
			strcpy(src, "ALL");

		/* parse destination */
		if (strcmp(dst, "::") == 0)
			strcpy(dst, "ALL");

		/* parse options */
		sport = dport = "";
		ptr = tmp;
		while ((val = strsep(&ptr, " ")) != NULL) {
			if (strncmp(val, "dpt:", 4) == 0)
				dport = val + 4;
			if (strncmp(val, "spt:", 4) == 0)
				sport = val + 4;
			else if (strncmp(val, "dpts:", 5) == 0)
				dport = val + 5;
			else if (strncmp(val, "spts:", 5) == 0)
				sport = val + 5;
		}

		ret += websWrite(wp,
			"[\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"],\n",
			src, sport, dst, dport, proto);
	}
	ret += websWrite(wp, "[]];\n");

	fclose(fp);
	unlink("/tmp/pinhole.log");

	return ret;

}
#endif
#endif


int
ej_get_upnp_array(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	char proto[4], eport[6], iaddr[sizeof("255.255.255.255")], iport[6], timestamp[15], desc[200], desc2[256];
	int ret=0;
	char line[256];

	ret += websWrite(wp, "var upnparray = [");

	fp = fopen("/var/lib/misc/upnp.leases", "r");
	if (fp == NULL) {
		ret += websWrite(wp, "[]];\n");
		return ret;
	}

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		if (sscanf(line,
			"%3[^:]:"
			"%5[^:]:"
			"%15[^:]:"
			"%5[^:]:"
			"%14[^:]:"
			"%200[^\n]",
			proto, eport, iaddr, iport, timestamp, desc) < 6) continue;

		if (str_escape_quotes(desc2, desc, sizeof(desc2)) == 0)
			strlcpy(desc2, desc, sizeof(desc2));

		ret += websWrite(wp, "[\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"],\n",
			proto, eport, iaddr, iport, timestamp, desc2);
	}

	fclose(fp);

	ret += websWrite(wp, "[]];\n");
	return ret;

}

int
ej_get_vserver_array(int eid, webs_t wp, int argc, char_t **argv)
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

	ret += websWrite(wp, "var vserverarray = [");

	fp = fopen("/tmp/vserver.log", "r");

	if (fp == NULL) {
                ret += websWrite(wp, "[]];\n");
                return ret;
        }

	while (fgets(line, sizeof(line), fp) != NULL)
	{

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

		/* Don't list DNS redirections  from DNSFilter or UPNP */
		if ((strcmp(chain, "DNSFILTER") == 0) || (strcmp(chain, "VUPNP") == 0))
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

		ret += websWrite(wp, "["
#ifdef NATSRC_SUPPORT
			"\"%s\", "
#endif
			"\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"],\n",
#ifdef NATSRC_SUPPORT
			src,
#endif
			dst, proto, range, host, port ? : range, chain);
	}
	fclose(fp);
	unlink("/tmp/vserver.log");

        ret += websWrite(wp, "[]];\n");
	return ret;
}



static int ipv4_route_table_array(webs_t wp)
{
	FILE *fp;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char buf[256], *dev, *sflags, *str;
	struct in_addr dest, gateway, mask;
	int flags, ref, use, metric;
	int unit, ret = 0;

	fp = fopen("/proc/net/route", "r");
	if (fp == NULL) {
		ret += websWrite(wp, "[]];\n");
		return ret;
	}

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
				snprintf(prefix, sizeof(prefix), "wan%d_", unit);
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

		ret += websWrite(wp, "[\"%s\",",  dest.s_addr == INADDR_ANY ? "default" : inet_ntoa(dest));
		ret += websWrite(wp, "\"%s\", ", gateway.s_addr == INADDR_ANY ? "*" : inet_ntoa(gateway));
		ret += websWrite(wp, "\"%s\", \"%s\", \"%d\", \"%d\", \"%d\", \"%s\"],\n",
                       inet_ntoa(mask), sflags, metric, ref, use, dev);

	}
	fclose(fp);

	return ret;
}


#ifdef RTCONFIG_IPV6
int
INET6_displayroutes_array(webs_t wp)
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
			return ret;
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
				ret += websWrite(wp, "[\"%3s\", \"%s\", \"%s\", \"%d\", \"%d\", \"%d\", \"%s\"],\n",
						addr6, naddr6, flags, metric, refcnt, use, iface);
				free(naddr6);
				break;
			}
		} while (1);
	}

	return ret;;
}
#endif


int
ej_get_route_array(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0;
#ifdef RTCONFIG_IPV6
	FILE *fp;
	char buf[256];
	unsigned int fl = 0;
	int found = 0;
#endif

	ret += websWrite(wp, "var routearray = [");
	ret += ipv4_route_table_array(wp);
	ret += websWrite(wp, "[]];\n");

	ret += websWrite(wp, "var routev6array = [");
#ifdef RTCONFIG_IPV6
	if (get_ipv6_service() != IPV6_DISABLED) {

	if ((fp = fopen("/proc/net/if_inet6", "r")) == (FILE*)0) {
		ret += websWrite(wp, "[]];\n");
		return ret;
	}
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
		INET6_displayroutes_array(wp);
	}
#endif

	ret += websWrite(wp, "[]];\n");
	return ret;
}

#ifdef RTCONFIG_IPV6
int
ej_lan_ipv6_network_array(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	char buf[64+32+8192+1];
	char *hostname, *macaddr, ipaddrs[8192+1];
	char ipv6_dns_str[1024];
	char *wan_type, *wan_dns, *p;
	int service, i, ret = 0;

	ret += websWrite(wp, "var ipv6cfgarray = [");

	if (!(ipv6_enabled() && is_routing_enabled())) {
		ret += websWrite(wp, "[]];\n");
		ret += websWrite(wp, "var ipv6clientarray = [");
		ret += websWrite(wp, "[]];\n");
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

	ret += websWrite(wp, "[\"IPv6 Connection Type\",\"%s\"],", wan_type);

	ret += websWrite(wp, "[\"WAN IPv6 Address\",\"%s\"],",
			 getifaddr((char *) get_wan6face(), AF_INET6, GIF_PREFIXLEN) ? : nvram_safe_get("ipv6_rtr_addr"));


	ret += websWrite(wp, "[\"WAN IPv6 Gateway\",\"%s\"],",
			 ipv6_gateway_address() ? : "");

	ret += websWrite(wp, "[\"LAN IPv6 Address\",\"%s/%d\"],",
			 nvram_safe_get("ipv6_rtr_addr"), nvram_get_int("ipv6_prefix_length"));

	ret += websWrite(wp, "[\"LAN IPv6 Link-Local Address\",\"%s\"],",
			 getifaddr(nvram_safe_get("lan_ifname"), AF_INET6, GIF_LINKLOCAL | GIF_PREFIXLEN) ? : "");

	if (service == IPV6_NATIVE_DHCP) {
		ret += websWrite(wp, "[\"DHCP-PD\",\"%s\"],",
			 nvram_get_int("ipv6_dhcp_pd") ? "Enabled" : "Disabled");
	}

	ret += websWrite(wp, "[\"LAN IPv6 Prefix\",\"%s/%d\"],",
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

	ret += websWrite(wp, "[\"DNS Address\",\"%s\"],", wan_dns);
	ret += websWrite(wp, "[]];\n");

	ret += websWrite(wp, "var ipv6clientarray = [");

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

		ret += websWrite(wp, "[\"%s\", \"%s\", \"%s\"],",
				 hostname, macaddr, ipaddrs);
	}
	fclose(fp);

	ret += websWrite(wp, "[]];\n");
	return ret;
}
#endif

