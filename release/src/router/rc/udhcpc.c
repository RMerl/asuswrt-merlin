/*
 * udhcpc scripts
 *
 * Copyright (C) 2009, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: udhcpc.c,v 1.27 2009/12/02 20:06:40 Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include <bcmnvram.h>
#include <shutils.h>
#include <rc.h>

/* Zeroconf support if DHCP fails */
#undef DHCP_ZEROCONF

static int
expires(char *wan_ifname, unsigned int in)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int unit;
	time_t now;
	FILE *fp;

	if ((unit = wan_prefix(wan_ifname, prefix)) < 0)
		return -1;
	if (wan_ifunit(wan_ifname) < 0)
		snprintf(prefix, sizeof(prefix), "wan%d_x", unit);

	nvram_set_int(strcat_r(prefix, "expires", tmp), (unsigned int) uptime() + in);

	snprintf(tmp, sizeof(tmp), "/tmp/udhcpc%d.expires", unit); 
	if ((fp = fopen(tmp, "w")) == NULL) {
		perror(tmp);
		return errno;
	}
	time(&now);
	fprintf(fp, "%d", (unsigned int) now + in);
	fclose(fp);

	return 0;
}

/* 
 * deconfig: This argument is used when udhcpc starts, and when a
 * leases is lost. The script should put the interface in an up, but
 * deconfigured state.
*/
static int
deconfig(int zcip)
{
	char *wan_ifname = safe_getenv("interface");
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int unit = wan_ifunit(wan_ifname);

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;
	if ((unit < 0) &&
	    (nvram_match(strcat_r(prefix, "proto", tmp), "l2tp") ||
	     nvram_match(strcat_r(prefix, "proto", tmp), "pptp"))) {
		logmessage(zcip ? "zcip client" : "dhcp client", "skipping resetting IP address to 0.0.0.0");
	} else
		ifconfig(wan_ifname, IFUP, "0.0.0.0", NULL);

	expires(wan_ifname, 0);
	wan_down(wan_ifname);

	/* Skip physical VPN subinterface */
	if (!(unit < 0))
		update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_DHCP_DECONFIG);

	_dprintf("udhcpc:: %s done\n", __FUNCTION__);
	return 0;
}

/*
 * bound: This argument is used when udhcpc moves from an unbound, to
 * a bound state. All of the paramaters are set in enviromental
 * variables, The script should configure the interface, and set any
 * other relavent parameters (default gateway, dns server, etc).
*/
static int
bound(void)
{
	char *wan_ifname = safe_getenv("interface");
	char *value;
	char tmp[100], tmp2[100], prefix[] = "wanXXXXXXXXXX_";
	char wanprefix[] = "wanXXXXXXXXXX_";
	char route[sizeof("255.255.255.255/255")];
	int unit, ifunit;
	int changed = 0;
	int gateway = 0;

#ifdef DHCP_ZEROCONF
	killall("zcip", SIGTERM);
#endif
	/* Figure out nvram variable name prefix for this i/f */
	if ((ifunit = wan_prefix(wan_ifname, wanprefix)) < 0)
		return -1;
	if ((unit = wan_ifunit(wan_ifname)) < 0)
		snprintf(prefix, sizeof(prefix), "wan%d_x", ifunit);
	else	snprintf(prefix, sizeof(prefix), "wan%d_", ifunit);

	if ((value = getenv("ip"))) {
		changed = nvram_invmatch(strcat_r(prefix, "ipaddr", tmp), trim_r(value));
		nvram_set(strcat_r(prefix, "ipaddr", tmp), trim_r(value));
	}
	if ((value = getenv("subnet")))
		nvram_set(strcat_r(prefix, "netmask", tmp), trim_r(value));
	if ((value = getenv("router"))) {
		gateway = 1;
		nvram_set(strcat_r(prefix, "gateway", tmp), trim_r(value));
		memset(tmp2, 0, 100);
		strcpy(tmp2, trim_r(value));
	}
	if(nvram_match(strcat_r(wanprefix, "dnsenable_x", tmp), "1")){
		if((value = getenv("dns")))
			nvram_set(strcat_r(prefix, "dns", tmp), trim_r(value));
		else if(gateway) // ex: android phone. The gateway is the DNS server.
			nvram_set(strcat_r(prefix, "dns", tmp), tmp2);
	}
	if ((value = getenv("wins")))
		nvram_set(strcat_r(prefix, "wins", tmp), trim_r(value));
	//if ((value = getenv("hostname")))
	//	sethostname(value, strlen(value) + 1);
	if ((value = getenv("domain")))
		nvram_set(strcat_r(prefix, "domain", tmp), trim_r(value));
	if ((value = getenv("lease"))) {
		unsigned int lease = atoi(value);
		nvram_set_int(strcat_r(prefix, "lease", tmp), lease);
		expires(wan_ifname, lease);
	}

	/* classful static routes */
	nvram_set(strcat_r(prefix, "routes", tmp), getenv("routes"));
	/* ms classless static routes */
	nvram_set(strcat_r(prefix, "routes_ms", tmp), getenv("msstaticroutes"));
	/* rfc3442 classless static routes */
	nvram_set(strcat_r(prefix, "routes_rfc", tmp), getenv("staticroutes"));

	/* rfc3442 could contain gateway */
	if (!gateway) {
		foreach(route, nvram_safe_get(strcat_r(prefix, "routes_rfc", tmp)), value) {
			if (gateway) {
				nvram_set(strcat_r(prefix, "gateway", tmp), route);
				break;
			} else
				gateway = (strcmp(route, "0.0.0.0/0") == 0);
		}
	}

#ifdef RTCONFIG_IPV6
	if ((value = getenv("ip6rd")) &&
	    (get_ipv6_service() == IPV6_6RD && nvram_match("ipv6_6rd_dhcp", "1"))) {
		char *ptr, *values[4];
		int i;

		ptr = value = strdup(value);
		for (i = 0; value && i < 4; i++)
			values[i] = strsep(&value, " ");
		if (i == 4) {
			nvram_set(strcat_r(prefix, "6rd_ip4size", tmp), values[0]);
			nvram_set(strcat_r(prefix, "6rd_prefixlen", tmp), values[1]);
			nvram_set(strcat_r(prefix, "6rd_prefix", tmp), values[2]);
			nvram_set(strcat_r(prefix, "6rd_router", tmp), values[3]);
		}
		free(ptr);
	}
#endif

	// check if the ipaddr is safe to apply
	// only handle one lan instance so far
	// update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_INVALID_IPADDR)
/* TODO: remake it as macro */
	if ((inet_network(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp))) &
	     inet_network(nvram_safe_get(strcat_r(prefix, "netmask", tmp)))) ==
	    (inet_network(nvram_safe_get("lan_ipaddr")) &
	     inet_network(nvram_safe_get("lan_netmask")))) {
		update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_INVALID_IPADDR);
		return 0;
	}

	/* Clean nat conntrack for this interface,
	 * but skip physical VPN subinterface for PPTP/L2TP */
	if (changed && !(unit < 0 &&
	    (nvram_match(strcat_r(wanprefix, "proto", tmp), "l2tp") ||
	     nvram_match(strcat_r(wanprefix, "proto", tmp), "pptp"))))
		ifconfig(wan_ifname, IFUP, "0.0.0.0", NULL);
	ifconfig(wan_ifname, IFUP,
		 nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
		 nvram_safe_get(strcat_r(prefix, "netmask", tmp)));

	wan_up(wan_ifname);

	logmessage("dhcp client", "bound %s via %s during %d seconds.",
		nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
		nvram_safe_get(strcat_r(prefix, "gateway", tmp)),
		nvram_get_int(strcat_r(prefix, "lease", tmp))
		);

	_dprintf("udhcpc:: %s done\n", __FUNCTION__);
	return 0;
}

/*
 * renew: This argument is used when a DHCP lease is renewed. All of
 * the paramaters are set in enviromental variables. This argument is
 * used when the interface is already configured, so the IP address,
 * will not change, however, the other DHCP paramaters, such as the
 * default gateway, subnet mask, and dns server may change.
 */
static int
renew(void)
{
	char *wan_ifname = safe_getenv("interface");
	char *value;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char wanprefix[] = "wanXXXXXXXXXX_";
	int unit, ifunit;
	int changed = 0;

#ifdef DHCP_ZEROCONF
	killall("zcip", SIGTERM);
#endif
	/* Figure out nvram variable name prefix for this i/f */
	if ((ifunit = wan_prefix(wan_ifname, wanprefix)) < 0)
		return -1;
	if ((unit = wan_ifunit(wan_ifname)) < 0)
		snprintf(prefix, sizeof(prefix), "wan%d_x", ifunit);
	else	snprintf(prefix, sizeof(prefix), "wan%d_", ifunit);

	if ((value = getenv("subnet")) == NULL ||
	    nvram_invmatch(strcat_r(prefix, "netmask", tmp), trim_r(value)))
		return bound();
	if ((value = getenv("router")) == NULL ||
	    nvram_invmatch(strcat_r(prefix, "gateway", tmp), trim_r(value)))
		return bound();

	if ((value = getenv("dns")) &&
	    nvram_match(strcat_r(wanprefix, "dnsenable_x", tmp), "1")) {
		changed = nvram_invmatch(strcat_r(prefix, "dns", tmp), trim_r(value));
		nvram_set(strcat_r(prefix, "dns", tmp), trim_r(value));
	}
	if ((value = getenv("wins")))
		nvram_set(strcat_r(prefix, "wins", tmp), trim_r(value));
	//if ((value = getenv("hostname")))
	//	sethostname(value, strlen(value) + 1);
	if ((value = getenv("domain")))
		nvram_set(strcat_r(prefix, "domain", tmp), trim_r(value));
	if ((value = getenv("lease"))) {
		unsigned int lease = atoi(value);
		nvram_set_int(strcat_r(prefix, "lease", tmp), lease);
		expires(wan_ifname, lease);
	}

	/* Update actual DNS or delayed for DHCP+PPP */
	if (changed) {
#ifdef OVERWRITE_DNS
		update_resolvconf();
#else
		add_ns(wan_ifname);
#endif
	}

	/* Update connected state and DNS for WEB UI,
	 * but skip physical VPN subinterface */
	if (changed && !(unit < 0))
		update_wan_state(wanprefix, WAN_STATE_CONNECTED, 0);

	_dprintf("udhcpc:: %s done\n", __FUNCTION__);
	return 0;
}

#ifdef DHCP_ZEROCONF
static int
leasefail(void)
{
	char *wan_ifname = safe_getenv("interface");
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char wanprefix[] = "wanXXXXXXXXXX_";
	int unit, ifunit;

	/* Figure out nvram variable name prefix for this i/f */
	if ((ifunit = wan_prefix(wan_ifname, wanprefix)) < 0)
		return -1;
	if ((unit = wan_ifunit(wan_ifname)) < 0)
		snprintf(prefix, sizeof(prefix), "wan%d_x", ifunit);
	else	snprintf(prefix, sizeof(prefix), "wan%d_", ifunit);

	if ((inet_network(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp))) &
	     inet_network(nvram_safe_get(strcat_r(prefix, "netmask", tmp)))) ==
	     inet_network("169.254.0.0"))
		return 0;

	return start_zcip(wan_ifname);
}
#endif

int
udhcpc_wan(int argc, char **argv)
{
	_dprintf("%s:: %s\n", __FUNCTION__, argv[1]);

	run_custom_script("dhcpc-event", argv[1]);

	if (!argv[1])
		return EINVAL;
	else if (strstr(argv[1], "deconfig"))
		return deconfig(0);
	else if (strstr(argv[1], "bound"))
		return bound();
	else if (strstr(argv[1], "renew"))
		return renew();
#ifdef DHCP_ZEROCONF
	else if (strstr(argv[1], "leasefail"))
		return leasefail();
#endif
/*	else if (strstr(argv[1], "nak")) */

	return 0;
}

int
start_udhcpc(char *wan_ifname, int unit, pid_t *ppid)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char pid[sizeof("/var/run/udhcpcXXXXXXXXXX.pid")];
	char clientid[sizeof("61:") + (32+32+1)*2];
	char *value;
	char *dhcp_argv[] = { "udhcpc",
		"-i", wan_ifname,
		"-p", (snprintf(pid, sizeof(pid), "/var/run/udhcpc%d.pid", unit), pid),
		"-s", "/tmp/udhcpc",
		"-t", "2",	/* Two DISC packets max */
		"-T", "5",	/* 5 seconds between packets */
		"-A", "120",	/* Wait 120 seconds before trying again */
		NULL,		/* -b */
		NULL, NULL,	/* -H wan_hostname */
		NULL,		/* -Oroutes */
		NULL,		/* -Ostaticroutes */
		NULL,		/* -Omsstaticroutes */
#ifdef __CONFIG_IPV6__
		NULL,		/* -Oip6rd rfc */
		NULL,		/* -Oip6rd comcast */
#endif
#ifdef RTCONFIG_DSL
		NULL, NULL,	/* -x 61:wan_clientid */
#endif
		NULL, NULL,	/* -x 61:wan_clientid (non-DSL) */
		NULL};
	int index = 7;		/* first NULL */
	int dr_enable;

	/* Use unit */
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	if (ppid == NULL)
		dhcp_argv[index++] = "-b";

	value = nvram_safe_get(strcat_r(prefix, "hostname", tmp));
	if (*value && is_valid_hostname(value)) {
		dhcp_argv[index++] = "-H";
		dhcp_argv[index++] = value;
	}

	/* 0: disable, 1: MS routes, 2: RFC routes, 3: Both */
	dr_enable = nvram_get_int("dr_enable_x");
	if (dr_enable != 0) {
		dhcp_argv[index++] = "-O33";		/* routes */
		if (dr_enable & (1 << 0))
			dhcp_argv[index++] = "-O249";   /* "msstaticroutes" */
		if (dr_enable & (1 << 1))
			dhcp_argv[index++] = "-O121";	/* "staticroutes" */
	}

#ifdef RTCONFIG_IPV6
	if (get_ipv6_service() == IPV6_6RD && nvram_match("ipv6_6rd_dhcp", "1")) {
		dhcp_argv[index++] = "-O212";		/* ip6rd rfc */
		dhcp_argv[index++] = "-O150";		/* ip6rd comcast */
	}
#endif

#ifdef RTCONFIG_DSL
	value = nvram_safe_get(strcat_r(prefix, "clientid", tmp));
	if (*value) {
		char *ptr = clientid;
		ptr += sprintf(ptr, "61:");
		while (*value && (ptr - clientid) < sizeof(clientid) - 2)
			ptr += sprintf(ptr, "%02x", *value++);
		dhcp_argv[index++] = "-x";
		dhcp_argv[index++] = clientid;
	}
#endif

	value = nvram_safe_get(strcat_r(prefix,"dhcpc_options",tmp));
	if (*value) {
		char *ptr = clientid;
		ptr += sprintf(ptr, "61:");
		while (*value && (ptr - clientid) < sizeof(clientid) - 2)
			ptr += sprintf(ptr, "%02x", *value++);
		dhcp_argv[index++] = "-x";
		dhcp_argv[index++] = clientid;
	}
	return _eval(dhcp_argv, NULL, 0, ppid);
}

/*
 * config: This argument is used when zcip moves to configured state.
 * All of the paramaters are set in enviromental variables, the script
 * should configure the interface.
*/
static int
config(void)
{
	char *wan_ifname = safe_getenv("interface");
	char *value;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char wanprefix[] = "wanXXXXXXXXXX_";
	int unit, ifunit;
	int changed = 0;

	/* Figure out nvram variable name prefix for this i/f */
	if ((ifunit = wan_prefix(wan_ifname, wanprefix)) < 0)
		return -1;
	if ((unit = wan_ifunit(wan_ifname)) < 0)
		snprintf(prefix, sizeof(prefix), "wan%d_x", ifunit);
	else	snprintf(prefix, sizeof(prefix), "wan%d_", ifunit);

	if ((value = getenv("ip"))) {
		changed = nvram_invmatch(strcat_r(prefix, "ipaddr", tmp), trim_r(value));
		nvram_set(strcat_r(prefix, "ipaddr", tmp), trim_r(value));
	}
	nvram_set(strcat_r(prefix, "netmask", tmp), "255.255.0.0");
	nvram_set(strcat_r(prefix, "gateway", tmp), "");
	nvram_set(strcat_r(prefix, "dns", tmp), "");
	//nvram_set(strcat_r(prefix, "wins", tmp), "");
	//nvram_set(strcat_r(prefix, "domain", tmp), "");
#ifdef DHCP_ZEROCONF
	nvram_unset(strcat_r(prefix, "lease", tmp));
	nvram_unset(strcat_r(prefix, "expires", tmp));
#endif
	/* Clean nat conntrack for this interface,
	 * but skip physical VPN subinterface for PPTP/L2TP */
	if (changed && !(unit < 0 &&
	    (nvram_match(strcat_r(wanprefix, "proto", tmp), "l2tp") ||
	     nvram_match(strcat_r(wanprefix, "proto", tmp), "pptp"))))
		ifconfig(wan_ifname, IFUP, "0.0.0.0", NULL);
	ifconfig(wan_ifname, IFUP,
		 nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
		 nvram_safe_get(strcat_r(prefix, "netmask", tmp)));

	wan_up(wan_ifname);

	logmessage("zcip client", "configured %s",
		nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)));

	_dprintf("zcip:: %s done\n", __FUNCTION__);
	return 0;
}

int
zcip_wan(int argc, char **argv)
{
	_dprintf("%s:: %s\n", __FUNCTION__, argv[1]);

        run_custom_script("zcip-event", argv[1]);

	if (!argv[1])
		return EINVAL;
	else if (strstr(argv[1], "deconfig"))
		return deconfig(1);
	else if (strstr(argv[1], "config"))
		return config();
/*	else if (strstr(argv[1], "init")) */

	return 0;
}

int
start_zcip(char *wan_ifname)
{
	char *zcip_argv[] = { "zcip", "-q", wan_ifname, "/tmp/zcip", NULL };

	return _eval(zcip_argv, NULL, 0, NULL);
}

static int
expires_lan(char *lan_ifname, unsigned int in)
{
	time_t now;
	FILE *fp;
	char tmp[100];

	time(&now);
	snprintf(tmp, sizeof(tmp), "/tmp/udhcpc-%s.expires", lan_ifname); 
	if (!(fp = fopen(tmp, "w"))) {
		perror(tmp);
		return errno;
	}
	fprintf(fp, "%d", (unsigned int) now + in);
	fclose(fp);
	return 0;
}

/* 
 * deconfig: This argument is used when udhcpc starts, and when a
 * leases is lost. The script should put the interface in an up, but
 * deconfigured state.
*/
static int
deconfig_lan(void)
{
	char *lan_ifname = safe_getenv("interface");

	//ifconfig(lan_ifname, IFUP, "0.0.0.0", NULL);
_dprintf("%s: IFUP.\n", __FUNCTION__);
	if(nvram_match("lan_proto", "static"))
		ifconfig(lan_ifname, IFUP, nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));
	else
		ifconfig(lan_ifname, IFUP, nvram_default_get("lan_ipaddr"), nvram_default_get("lan_netmask"));

	expires_lan(lan_ifname, 0);

	lan_down(lan_ifname);

	_dprintf("done\n");
	return 0;
}

/*
 * bound: This argument is used when udhcpc moves from an unbound, to
 * a bound state. All of the paramaters are set in enviromental
 * variables, The script should configure the interface, and set any
 * other relavent parameters (default gateway, dns server, etc).
*/
static int
bound_lan(void)
{
	char *lan_ifname = safe_getenv("interface");
	char *value;

	if ((value = getenv("ip")))
		nvram_set("lan_ipaddr", trim_r(value));
	if ((value = getenv("subnet")))
		nvram_set("lan_netmask", trim_r(value));
	if ((value = getenv("router")))
		nvram_set("lan_gateway", trim_r(value));
	if ((value = getenv("lease"))) {
		nvram_set("lan_lease", trim_r(value));
		expires_lan(lan_ifname, atoi(value));
	}
	if ((value = getenv("dns")))
		nvram_set("lan_dns", trim_r(value));

_dprintf("%s: IFUP.\n", __FUNCTION__);
#ifdef RTCONFIG_WIRELESSREPEATER
	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_mode") == 0){
		update_lan_state(LAN_STATE_CONNECTED, 0);
		_dprintf("done\n");
		return 0;
	}
#endif

	ifconfig(lan_ifname, IFUP, nvram_safe_get("lan_ipaddr"),
		nvram_safe_get("lan_netmask"));

	lan_up(lan_ifname);

	_dprintf("done\n");
	return 0;
}

/*
 * renew: This argument is used when a DHCP lease is renewed. All of
 * the paramaters are set in enviromental variables. This argument is
 * used when the interface is already configured, so the IP address,
 * will not change, however, the other DHCP paramaters, such as the
 * default gateway, subnet mask, and dns server may change.
 */
static int
renew_lan(void)
{
	bound_lan();

	_dprintf("done\n");
	return 0;
}

/* dhcp client script entry for LAN (AP) */
int
udhcpc_lan(int argc, char **argv)
{
        run_custom_script("dhcpc-event", argv[1]);

	if (!argv[1])
		return EINVAL;
	else if (strstr(argv[1], "deconfig"))
		return deconfig_lan();
	else if (strstr(argv[1], "bound"))
		return bound_lan();
	else if (strstr(argv[1], "renew"))
		return renew_lan();
	else
		return EINVAL;
}

// -----------------------------------------------------------------------------

// copy env to nvram
// returns 1 if new/changed, 0 if not changed/no env
static int env2nv(char *env, char *nv)
{
	char *value;
	if ((value = getenv(env)) != NULL) {
		if (!nvram_match(nv, trim_r(value))) {
			nvram_set(nv, trim_r(value));
			return 1;
		}
	}
	return 0;
}

#ifdef RTCONFIG_IPV6
int dhcp6c_state_main(int argc, char **argv)
{
	char prefix[INET6_ADDRSTRLEN + 1];
	struct in6_addr addr;
	int i, r;

	TRACE_PT("begin\n");

	if (!wait_action_idle(10)) return 1;

	nvram_set("ipv6_rtr_addr", getifaddr(nvram_safe_get("lan_ifname"), AF_INET6, 0));

	if (argv[1]) nvram_set("ipv6_gw_addr", argv[1]);

	// extract prefix from configured IPv6 address
	if (inet_pton(AF_INET6, nvram_safe_get("ipv6_rtr_addr"), &addr) > 0) {
		r = nvram_get_int("ipv6_prefix_length") ? : 64;
		for (r = 128 - r, i = 15; r > 0; r -= 8) {
			if (r >= 8)
				addr.s6_addr[i--] = 0;
			else
				addr.s6_addr[i--] &= (0xff << r);
		}
		inet_ntop(AF_INET6, &addr, prefix, sizeof(prefix));
		nvram_set("ipv6_prefix", prefix);
	}

	if (env2nv("new_domain_name_servers", "ipv6_get_dns")) {
		TRACE_PT("ipv6_get_dns=%s\n", nvram_safe_get("ipv6_get_dns"));
#ifdef OVERWRITE_DNS
		update_resolvconf();
#else
		add_ns(NULL);
#endif
	}

	// (re)start radvd and httpd
	start_radvd();
	start_httpd();

	TRACE_PT("end\n");
	return 0;
}

static unsigned long pow(int m, int n)
{
	if (m == 1)
		return n;
	else if (m == 0)
		return 1;
	return pow(m - 1, n) * n ;
}

void start_dhcp6c(void)
{
	FILE *f;
	int prefix_len;
	char *wan6face = get_wan6face();
	char *argv[] = { "dhcp6c", "-T", "LL", NULL, NULL, NULL };
	int argc;
	char iaid_str[9] = {0};
	int i, j;
	unsigned long iaid = 0;
	char *mac;
	char *lan_ifname = nvram_safe_get("lan_ifname");

	TRACE_PT("begin\n");

	// Check if turned on
	if (get_ipv6_service() != IPV6_NATIVE_DHCP) return;

	if (!wan6face || (strlen(wan6face) <= 0)) return;

	prefix_len = 64 - (nvram_get_int("ipv6_prefix_length") ? : 64);
	if (prefix_len < 0)
		prefix_len = 0;

	// generate IAID from last 7 digits of WAN MAC addresss
	mac = nvram_safe_get("wan0_hwaddr");
	for (i = 7, j = 0; i < 17; i++) {
		if (mac[i] != 0x3A) { // skip :
			iaid_str[j] = mac[i];
			j++;
		}
	}
	for (i = 0; i < 7; i++) {
		unsigned long var = 0;
		if (iaid_str[i] >= 0x30 && iaid_str[i] <= 0x39) //0-9
			var = iaid_str[i] - 0x30;
		else if (iaid_str[i] >= 0x41 && iaid_str[i] <= 0x46) //A-F
			var = 10 + (iaid_str[i] - 0x41);
		else if (iaid_str[i] >= 0x61 && iaid_str[i] <= 0x66) //a-f
			var = 10 + (iaid_str[i] - 0x61);
		var = var * pow(6 - i, 16);
		iaid += var;
	}

	nvram_set("ipv6_get_dns", "");
	nvram_set("ipv6_rtr_addr", "");
	nvram_set("ipv6_prefix", "");
	nvram_set("ipv6_gw_addr", "");

	// Create dhcp6c.conf
	if ((f = fopen("/etc/dhcp6c.conf", "w"))) {
		fprintf(f,
			"interface %s {\n"
			" send ia-pd %d;\n"
			" send ia-na %d;\n"
			" send rapid-commit;\n"
			" request domain-name-servers;\n"
			" script \"/sbin/dhcp6c-state\";\n"
			"};\n"
			"id-assoc pd %d {\n"
			" prefix-interface %s {\n"
			"  sla-id 1;\n"
			"  sla-len %d;\n"
			" };\n"
			"};\n"
			"id-assoc na %d { };\n",
			wan6face,
			iaid,
			iaid,
			iaid,
			lan_ifname,
			prefix_len,
			iaid);
		fclose(f);
	}

	argc = 3;
	if (nvram_get_int("ipv6_debug"))
		argv[argc++] = "-D";
	argv[argc++] = wan6face;
	argv[argc] = NULL;
	_eval(argv, NULL, 0, NULL);

	TRACE_PT("end\n");
}

void stop_dhcp6c(void)
{
	TRACE_PT("begin\n");

	char *wan6face = get_wan6face();
	char *lan_ifname = nvram_safe_get("lan_ifname");

	if (!pids("dhcp6c")) return;

	killall("dhcp6c-event", SIGTERM);
	killall_tk("dhcp6c");

	eval("ip", "-6", "addr", "flush", "scope", "global", "dev", lan_ifname);
	eval("ip", "-6", "route", "flush", "root", "2000::/3", "dev", wan6face);
	eval("ip", "-6", "neigh", "flush", "dev", lan_ifname);

	TRACE_PT("end\n");
}
#endif	// RTCONFIG_IPV6
