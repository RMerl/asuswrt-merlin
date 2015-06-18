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


int
ej_get_leases_array(int eid, webs_t wp, int argc, char_t **argv)
{
	char *leaselist = NULL, *leaselistptr;
	char hostname[16], duration[9], ip[40], mac[18];
	int ret=0;

	killall("dnsmasq", SIGUSR2);
	sleep(1);

	leaselist = read_whole_file("/var/lib/misc/dnsmasq.leases");
	if (!leaselist)
		return websWrite(wp, "leasearray = [];\n");

	ret += websWrite(wp, "leasearray= [");
	leaselistptr = leaselist;

	while ((leaselistptr < leaselist+strlen(leaselist)-2) && (sscanf(leaselistptr,"%8s %17s %15s %15s %*s", duration, mac, ip, hostname) == 4)) {
		ret += websWrite(wp, "[\"%s\", \"%s\", \"%s\", \"%s\"],\n", duration, mac, ip, hostname);
		leaselistptr = strstr(leaselistptr,"\n")+1;
	}
	ret += websWrite(wp, "[]];\n");
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
			strncpy(desc2, desc, sizeof(desc2));

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
