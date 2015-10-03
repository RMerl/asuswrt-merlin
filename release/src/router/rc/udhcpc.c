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
#include <net/ethernet.h>

#include <bcmnvram.h>
#include <shutils.h>
#include <rc.h>

/* Support for Domain Search List */
#undef DHCP_RFC3397

/* returns: pointer to dest
 * dest size must be lagre enough to accept n bytes from
   src in hex representation plus one \0 byte */
static char *
bin2hex(char *dest, size_t size, const void *src, size_t n)
{
	unsigned char *sptr = (unsigned char *) src;
	unsigned char *send = sptr + n;
	char *dptr = dest;

	while (sptr < send && size > 2) {
		n = snprintf(dptr, size, "%02x", *sptr++);
		dptr += n;
		size -= n;
	}
	return dest;
}

#ifdef RTCONFIG_TR069
/* returns:
 *  NULL on NULL value or alloc error
 *  binary representation of hex value on success
 * if size if specified, returns actual size of parsed value
 * note: value is alloced and needs to be free() */
static char *
hex2bin(const char *value, size_t *size)
{
	char buf[3], *src, *tmp;
	unsigned char *dst, *ptr;

	if (!(src = (char *) value) ||
	    !(dst = calloc(1, strlen(src)/2 + 1)))
		return NULL;

	memset(buf, 0, sizeof(buf));
	for (ptr = dst; src[0] && src[1]; ptr++) {
		buf[0] = *src++;
		buf[1] = *src++;
		*ptr = strtoul(buf, &tmp, 16);
		if (tmp == buf || *tmp)
			break;
	}
	if (size)
		*size = ptr - dst;

	return dst;
}
#endif

/* set nvram to value
 * returns:
 *  0 if not changed
 *  1 if new/changed/removed */
static int
nvram_set_check(const char *name, const char *value)
{
	char *nvalue = nvram_get(name);

	if (nvalue == value || strcmp(nvalue ? : "", value ? : "") == 0)
		return 0;

	nvram_set(name, value);
	return 1;
}

/* set nvram to env value
 * returns:
 *  0 if not changed
 *  1 if new/changed/removed */
static int
nvram_set_env(const char *name, const char *env)
{
	char *evalue = getenv(env);

	if (evalue)
		evalue = trim_r(evalue);

	return nvram_set_check(name, evalue);
}

struct viopt_hdr {
	unsigned int entnum;
	unsigned char len;
	unsigned char data[0];
} __attribute__ ((__packed__));

struct opt_hdr {
	unsigned char id;
	unsigned char len;
	unsigned char data[0];
} __attribute__ ((__packed__));

#ifdef RTCONFIG_TR069
#ifdef RTCONFIG_TR181
static struct viopt_hdr *
viopt_get(const void *buf, size_t size, unsigned int entnum)
{
	struct viopt_hdr *opt;
	unsigned char *ptr = (unsigned char *) buf;
	unsigned char *end = (unsigned char *) buf + size;

	while (ptr + sizeof(*opt) <= end) {
		opt = (struct viopt_hdr *) ptr;
		if ((ptr += sizeof(*opt) + opt->len) > end)
			break;
		if (opt->entnum == entnum)
			return opt;
	}

	return NULL;
}
#endif

static struct opt_hdr *
opt_get(const void *buf, size_t size, unsigned char id)
{
	struct opt_hdr *opt;
	unsigned char *ptr = (unsigned char *) buf;
	unsigned char *end = (unsigned char *) buf + size;

	while (ptr + sizeof(*opt) <= end) {
		opt = (struct opt_hdr *) ptr;
		if (opt->id == 0) {
			ptr++;
			continue;
		} else if (opt->id == 255)
			break;
		if ((ptr += sizeof(*opt) + opt->len) > end)
			break;
		if (opt->id == id)
			return opt;
	}

	return NULL;
}

static char
*stropt(const struct opt_hdr *opt, char *buf)
{
	strncpy(buf, opt->data, opt->len);
	buf[opt->len] = '\0';
	return buf;
}

#ifdef RTCONFIG_TR181
static int
opt_add(const void *buf, size_t size, unsigned char id, void *data, unsigned char len)
{
	struct opt_hdr *opt = (struct opt_hdr *) buf;

	if (size >= sizeof(*opt) + len) {
		opt->id = id;
		opt->len = len;
		memcpy(opt->data, data, len);
		return sizeof(*opt) + len;
	}

	return 0;
}
#endif
#endif

static int
expires(char *wan_ifname, unsigned int in)
{
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
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
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
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
	char *value, *gateway;
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char wanprefix[sizeof("wanXXXXXXXXXX_")];
	int unit, ifunit;
	int changed = 0;
#ifdef RTCONFIG_TR069
	int size;
#endif

	/* Figure out nvram variable name prefix for this i/f */
	if ((ifunit = wan_prefix(wan_ifname, wanprefix)) < 0)
		return -1;
	if ((unit = wan_ifunit(wan_ifname)) < 0)
		snprintf(prefix, sizeof(prefix), "wan%d_x", ifunit);
	else	snprintf(prefix, sizeof(prefix), "wan%d_", ifunit);

	/* Stop zcip to avoid races */
	stop_zcip(ifunit);

	changed += nvram_set_env(strcat_r(prefix, "ipaddr", tmp), "ip");
#if defined(RTCONFIG_USB_MODEM) && defined(RT4GAC55U)
	if (get_dualwan_by_unit(ifunit) == WANS_DUALWAN_IF_USB &&
	    nvram_match("usb_modem_act_type", "gobi")) {
		changed += nvram_set_check(strcat_r(prefix, "netmask", tmp), "255.255.255.255");
		if ((gateway = getenv("ip")))
			nvram_set(strcat_r(prefix, "gateway", tmp), trim_r(gateway));
	} else
#endif
{
	changed += nvram_set_env(strcat_r(prefix, "netmask", tmp), "subnet");
	if ((gateway = getenv("router")))
		nvram_set(strcat_r(prefix, "gateway", tmp), trim_r(gateway));
}

	if (nvram_get_int(strcat_r(wanprefix, "dnsenable_x", tmp))) {
		/* ex: android phone, the gateway is the DNS server. */
		if ((value = getenv("dns") ? : getenv("router")))
			nvram_set(strcat_r(prefix, "dns", tmp), trim_r(value));
#ifdef DHCP_RFC3397
		if ((value = getenv("search")) && *value) {
			char *domain, *result;
			if ((domain = getenv("domain")) && *domain &&
			    find_word(value, trim_r(domain)) == NULL) {
				result = alloca(strlen(domain) + strlen(value) + 2);
				sprintf(result, "%s %s", domain, value);
				value = result;
			}
			nvram_set(strcat_r(prefix, "domain", tmp), trim_r(value));
		} else
#endif
		nvram_set_env(strcat_r(prefix, "domain", tmp), "domain");
	}
	if ((value = getenv("wins")))
		nvram_set(strcat_r(prefix, "wins", tmp), trim_r(value));
	//if ((value = getenv("hostname")))
	//	sethostname(value, strlen(value) + 1);
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

#ifdef RTCONFIG_IPV6
	if ((value = getenv("ip6rd")) &&
	    (get_ipv6_service() == IPV6_6RD && nvram_match("ipv6_6rd_dhcp", "1"))) {
		char *ptr, *values[4];
		int i;

		ptr = value = strdup(value);
		for (i = 0; value && i < 4; i++)
			values[i] = strsep(&value, " ");
		if (i == 4) {
			nvram_set(strcat_r(wanprefix, "6rd_ip4size", tmp), values[0]);
			nvram_set(strcat_r(wanprefix, "6rd_prefixlen", tmp), values[1]);
			nvram_set(strcat_r(wanprefix, "6rd_prefix", tmp), values[2]);
			nvram_set(strcat_r(wanprefix, "6rd_router", tmp), values[3]);
		}
		free(ptr);
	}
#endif

#ifdef RTCONFIG_TR069
	if ((value = getenv("opt43")) && nvram_get_int("tr_discovery") &&
	    (value = hex2bin(value, &size))) {
		struct opt_hdr *opt;
		char buf[256], *url = NULL;
		if ((opt = opt_get(value, size, 1)) &&
		    strstr(stropt(opt, buf), "://") > buf)
			url = buf;
		else if (strstr(value, "://") > value)
			url = trim_r(value);
		if (url && *url) {
			nvram_set("tr_acs_url", url);
			nvram_set_int("tr_enable", 1);
		}
		//if ((opt = opt_get(value, size, 1)))
		//	nvram_set(strcat_r(wanprefix, "tr_acs_url", tmp), stropt(opt, buf));
		//if ((opt = opt_get(value, size, 2)))
		//	nvram_set(strcat_r(wanprefix, "tr_pvgcode", tmp), stropt(opt, buf));
		free(value);
	}
#ifdef RTCONFIG_TR181
	nvram_unset("vivso");
	if ((value = hex2bin(getenv("opt125"), &size))) {
		struct viopt_hdr *viopt;
		struct opt_hdr *oui, *serial, *class;
		if ((viopt = viopt_get(value, size, htonl(3561))) &&
		    (oui = opt_get(viopt->data, viopt->len, 4)) &&
		    (serial = opt_get(viopt->data, viopt->len, 5))) {
			char vivso[6 + 64 + 64 + 3];
			char *ptr = vivso;
			char *end = ptr + sizeof(vivso);
			ptr += snprintf(ptr, end - ptr, "%s,", stropt(oui, tmp));
			ptr += snprintf(ptr, end - ptr, "%s,", stropt(serial, tmp));
			if ((class = opt_get(viopt->data, viopt->len, 6)))
				ptr += snprintf(ptr, end - ptr, "%s", stropt(class, tmp));
			nvram_set("vivso", vivso);
		}
		free(value);
	}
#endif
#endif

	// check if the ipaddr is safe to apply
	// only handle one lan instance so far
	// update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_INVALID_IPADDR)
	if (inet_equal(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)), nvram_safe_get(strcat_r(prefix, "netmask", tmp)),
		       nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"))) {
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
	char *value, *gateway;
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char wanprefix[sizeof("wanXXXXXXXXXX_")];
	int unit, ifunit;
	int changed = 0;

	/* Figure out nvram variable name prefix for this i/f */
	if ((ifunit = wan_prefix(wan_ifname, wanprefix)) < 0)
		return -1;
	if ((unit = wan_ifunit(wan_ifname)) < 0)
		snprintf(prefix, sizeof(prefix), "wan%d_x", ifunit);
	else	snprintf(prefix, sizeof(prefix), "wan%d_", ifunit);

	if ((value = getenv("ip")) == NULL ||
	    !nvram_match(strcat_r(prefix, "ipaddr", tmp), trim_r(value)))
		return bound();
#if defined(RTCONFIG_USB_MODEM) && defined(RT4GAC55U)
	if (get_dualwan_by_unit(ifunit) == WANS_DUALWAN_IF_USB &&
	    nvram_match("usb_modem_act_type", "gobi")) {
		if (!nvram_match(strcat_r(prefix, "netmask", tmp), "255.255.255.255"))
			return bound();
		if ((gateway = getenv("ip")) == NULL ||
		    !nvram_match(strcat_r(prefix, "gateway", tmp), trim_r(gateway)))
			return bound();
	} else
#endif
{
	if ((value = getenv("subnet")) == NULL ||
	    !nvram_match(strcat_r(prefix, "netmask", tmp), trim_r(value)))
		return bound();
	if ((gateway = getenv("router")) == NULL ||
	    !nvram_match(strcat_r(prefix, "gateway", tmp), trim_r(gateway)))
		return bound();
}

	if (nvram_get_int(strcat_r(wanprefix, "dnsenable_x", tmp))) {
		/* ex: android phone, the gateway is the DNS server. */
		if ((value = getenv("dns") ? : getenv("router"))) {
			changed += !nvram_match(strcat_r(prefix, "dns", tmp), trim_r(value));
			nvram_set(strcat_r(prefix, "dns", tmp), trim_r(value));
		}
#ifdef DHCP_RFC3397
		if ((value = getenv("search")) && *value) {
			char *domain, *result;
			if ((domain = getenv("domain")) && *domain &&
			    find_word(value, trim_r(domain)) == NULL) {
				result = alloca(strlen(domain) + strlen(value) + 2);
				sprintf(result, "%s %s", domain, value);
				value = result;
			}
			changed += !nvram_match(strcat_r(prefix, "domain", tmp), trim_r(value));
			nvram_set(strcat_r(prefix, "domain", tmp), trim_r(value));
		} else
#endif
		changed += nvram_set_env(strcat_r(prefix, "domain", tmp), "domain");
	}
	if ((value = getenv("wins")))
		nvram_set(strcat_r(prefix, "wins", tmp), trim_r(value));
	//if ((value = getenv("hostname")))
	//	sethostname(value, strlen(value) + 1);
	if ((value = getenv("lease"))) {
		unsigned int lease = atoi(value);
		nvram_set_int(strcat_r(prefix, "lease", tmp), lease);
		expires(wan_ifname, lease);
	}

	/* Update actual DNS or delayed for DHCP+PPP */
	if (changed)
		update_resolvconf();

	/* Update connected state and DNS for WEB UI,
	 * but skip physical VPN subinterface */
	if (changed && !(unit < 0))
		update_wan_state(wanprefix, WAN_STATE_CONNECTED, 0);

	_dprintf("udhcpc:: %s done\n", __FUNCTION__);
	return 0;
}

static int
leasefail(void)
{
	char *wan_ifname = safe_getenv("interface");
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char pid[sizeof("/var/run/zcipXXXXXXXXXX.pid")];
	int unit;

	/* Figure out nvram variable name prefix for this i/f */
	if ((unit = wan_prefix(wan_ifname, prefix)) < 0)
		return -1;

	/* Start zcip for pppoe only */
	if (!nvram_match(strcat_r(prefix, "proto", tmp), "pppoe"))
		return 0;

	snprintf(pid, sizeof(pid), "/var/run/zcip%d.pid", unit);
	if (kill_pidfile_s(pid, 0) == 0)
		return 0;

	return start_zcip(wan_ifname, unit, NULL);
}

int
udhcpc_wan(int argc, char **argv)
{
	_dprintf("%s:: %s\n", __FUNCTION__, argv[1] ? : "");

	run_custom_script("dhcpc-event", argv[1]);

	if (!argv[1])
		return EINVAL;
	else if (strstr(argv[1], "deconfig"))
		return deconfig(0);
	else if (strstr(argv[1], "bound"))
		return bound();
	else if (strstr(argv[1], "renew"))
		return renew();
	else if (strstr(argv[1], "leasefail"))
		return leasefail();
/*	else if (strstr(argv[1], "nak")) */

	return 0;
}

int
start_udhcpc(char *wan_ifname, int unit, pid_t *ppid)
{
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char pid[sizeof("/var/run/udhcpcXXXXXXXXXX.pid")];
	char clientid[sizeof("61:") + (32+32+1)*2];
#if defined(RTCONFIG_TR069) && defined(RTCONFIG_TR181)
	unsigned char optbuf[sizeof(struct viopt_hdr) + 128];
	unsigned char hwaddr[6];
	char vivopts[sizeof("125:") + sizeof(optbuf)*2];
#endif
	char *value;
	char *dhcp_argv[] = { "/sbin/udhcpc",
		"-i", wan_ifname,
		"-p", (snprintf(pid, sizeof(pid), "/var/run/udhcpc%d.pid", unit), pid),
		"-s", "/tmp/udhcpc",
		NULL,		/* -t2 */
		NULL,		/* -T5 */
		NULL,		/* -A120 */
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
#ifdef RTCONFIG_TR069
		NULL, NULL,	/* -V dslforum.org */
		NULL,		/* -O43 */
#ifdef RTCONFIG_TR181
		NULL, NULL,	/* -x 125:vivopts */
#endif
#endif
		NULL, NULL,	/* -x 61:wan_clientid (non-DSL) */
		NULL };
	int index = 7;		/* first NULL */
	int dr_enable;

	/* Use unit */
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	/* Stop zcip to avoid races */
	stop_zcip(unit);

	/* Skip dhcp and start zcip for pppoe, if desired */
	if (nvram_match(strcat_r(prefix, "proto", tmp), "pppoe") &&
	    nvram_match(strcat_r(prefix, "vpndhcp", tmp), "0"))
		return start_zcip(wan_ifname, unit, ppid);

	if (nvram_get_int("dhcpc_mode") == 0) {
		/* 2 discover packets max (default 3 discover packets) */
		dhcp_argv[index++] = "-t2";
		/* 5 seconds between packets (default 3 seconds) */
		dhcp_argv[index++] = "-T5";
		/* Wait 160 seconds before trying again (default 20 seconds) */
		/* set to 160 to accomodate new timings enforced by Charter cable */
		dhcp_argv[index++] = "-A160";
	}

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
			dhcp_argv[index++] = "-O249";	/* "msstaticroutes" */
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
		int len = snprintf(clientid, sizeof(clientid), "61:");
		bin2hex(clientid + len, sizeof(clientid) - len, value, strlen(value));
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

#ifdef RTCONFIG_TR069
	if (!nvram_invmatch("tr_acs_url", ""))
		nvram_set_int("tr_discovery", 1);
	if (nvram_get_int("tr_enable") && nvram_get_int("tr_discovery")) {
		dhcp_argv[index++] = "-V";
		dhcp_argv[index++] = "dslforum.org";
		dhcp_argv[index++] = "-O43";
	}
#ifdef RTCONFIG_TR181
	if (ether_atoe(get_lan_hwaddr(), hwaddr)) {
		struct viopt_hdr *viopt = (struct viopt_hdr *) optbuf;
		unsigned char *ptr = viopt->data;
		unsigned char *end = optbuf + sizeof(optbuf);
		int len;

		/* OUI */
		len = snprintf(tmp, sizeof(tmp), "%02X%02X%02X", hwaddr[0], hwaddr[1], hwaddr[2]);
		ptr += opt_add(ptr, end - ptr, 1, tmp, len);
		/* Serial */
		len = snprintf(tmp, sizeof(tmp), "%02X%02X%02X%02X%02X%02X",
			       hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
		ptr += opt_add(ptr, end - ptr, 2, tmp, len);
		/* Class */
		len = snprintf(tmp, sizeof(tmp), "%s", nvram_safe_get("productid"));
		ptr += opt_add(ptr, end - ptr, 3, tmp, len);

		viopt->entnum = htonl(3561);
		viopt->len = ptr - viopt->data;

		len = snprintf(vivopts, sizeof(vivopts), "125:");
		bin2hex(vivopts + len, sizeof(vivopts) - len, viopt, sizeof(*viopt) + viopt->len);

		dhcp_argv[index++] = "-x";
		dhcp_argv[index++] = vivopts;
	}
#endif
#endif

	return _eval(dhcp_argv, NULL, 0, ppid);
}

void
stop_udhcpc(int unit)
{
	char pid[sizeof("/var/run/udhcpcXXXXXXXXXX.pid")];

	/* Stop zcip before udhcpc to avoid races */
	stop_zcip(unit);

	/* Stop all instances */
	if (unit < 0) {
		killall_tk("udhcpc");
		return;
	}

	/* Stop unit instance only */
	snprintf(pid, sizeof(pid), "/var/run/udhcpc%d.pid", unit);
	if (kill_pidfile_s(pid, SIGUSR2) == 0) {
		usleep(10000);
		kill_pidfile_s(pid, SIGTERM);
	}
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
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char wanprefix[sizeof("wanXXXXXXXXXX_")];
	int unit, ifunit;
	int changed = 0;

	/* Figure out nvram variable name prefix for this i/f */
	if ((ifunit = wan_prefix(wan_ifname, wanprefix)) < 0)
		return -1;
	if ((unit = wan_ifunit(wan_ifname)) < 0)
		snprintf(prefix, sizeof(prefix), "wan%d_x", ifunit);
	else	snprintf(prefix, sizeof(prefix), "wan%d_", ifunit);

	if ((value = getenv("ip"))) {
		changed = !nvram_match(strcat_r(prefix, "ipaddr", tmp), trim_r(value));
		nvram_set(strcat_r(prefix, "ipaddr", tmp), trim_r(value));
	}
	nvram_set(strcat_r(prefix, "netmask", tmp), "255.255.0.0");
	nvram_set(strcat_r(prefix, "gateway", tmp), "");
	if (nvram_get_int(strcat_r(wanprefix, "dnsenable_x", tmp)))
		nvram_set(strcat_r(prefix, "dns", tmp), "");
	nvram_unset(strcat_r(prefix, "wins", tmp));
	nvram_unset(strcat_r(prefix, "domain", tmp));
	nvram_unset(strcat_r(prefix, "lease", tmp));
	nvram_unset(strcat_r(prefix, "expires", tmp));

	nvram_unset(strcat_r(prefix, "routes", tmp));
	nvram_unset(strcat_r(prefix, "routes_ms", tmp));
	nvram_unset(strcat_r(prefix, "routes_rfc", tmp));

#ifdef RTCONFIG_TR069
//	nvram_unset(strcat_r(prefix, "tr_acs_url", tmp));
//	nvram_unset(strcat_r(prefix, "tr_pvgcode", tmp));
#ifdef RTCONFIG_TR181
	nvram_unset("vivso");
#endif
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
	_dprintf("%s:: %s\n", __FUNCTION__, argv[1] ? : "");

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
start_zcip(char *wan_ifname, int unit, pid_t *ppid)
{
	char pid[sizeof("/var/run/zcipXXXXXXXXXX.pid")];
	char *zcip_argv[] = { "/sbin/zcip",
		"-p", (snprintf(pid, sizeof(pid), "/var/run/zcip%d.pid", unit), pid),
		wan_ifname,
		"/tmp/zcip",
		NULL };

	return _eval(zcip_argv, NULL, 0, ppid);
}

void
stop_zcip(int unit)
{
	char pid[sizeof("/var/run/zcipXXXXXXXXXX.pid")];

	/* Stop all instances */
	if (unit < 0) {
		killall_tk("zcip");
		return;
	}

	/* Stop unit instance only */
	snprintf(pid, sizeof(pid), "/var/run/zcip%d.pid", unit);
	kill_pidfile_s(pid, SIGTERM);
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
#ifdef RTCONFIG_DHCP_OVERRIDE
	if (nvram_get_int("sw_mode") == SW_MODE_AP)
		;
	else
#endif
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
#if defined(RTCONFIG_TR069) && defined(RTCONFIG_TR181)
	char tmp[100];
	int size;
#endif

	if ((value = getenv("ip"))) {
		/* restart httpd after lan_ipaddr udpating through lan dhcp client */
		if (!nvram_match("lan_ipaddr", trim_r(value))) {
			stop_httpd();
			start_httpd();
		}
		nvram_set("lan_ipaddr", trim_r(value));
	}
	if ((value = getenv("subnet")))
		nvram_set("lan_netmask", trim_r(value));
	if ((value = getenv("router")))
		nvram_set("lan_gateway", trim_r(value));
	if ((value = getenv("lease"))) {
		nvram_set("lan_lease", trim_r(value));
		expires_lan(lan_ifname, atoi(value));
	}
	if (nvram_get_int("lan_dnsenable_x") && (value = getenv("dns")))
		nvram_set("lan_dns", trim_r(value));

#if defined(RTCONFIG_TR069) && defined(RTCONFIG_TR181)
	nvram_unset("vivso");
	if ((value = hex2bin(getenv("opt125"), &size))) {
		struct viopt_hdr *viopt;
		struct opt_hdr *oui, *serial, *class;
		if ((viopt = viopt_get(value, size, htonl(3561))) &&
		    (oui = opt_get(viopt->data, viopt->len, 4)) &&
		    (serial = opt_get(viopt->data, viopt->len, 5))) {
			char vivso[6 + 64 + 64 + 3];
			char *ptr = vivso;
			char *end = ptr + sizeof(vivso);
			ptr += snprintf(ptr, end - ptr, "%s,", stropt(oui, tmp));
			ptr += snprintf(ptr, end - ptr, "%s,", stropt(serial, tmp));
			if ((class = opt_get(viopt->data, viopt->len, 6)))
				ptr += snprintf(ptr, end - ptr, "%s", stropt(class, tmp));
			nvram_set("vivso", vivso);
		}
		free(value);
	}
#endif

_dprintf("%s: IFUP.\n", __FUNCTION__);
#ifdef RTCONFIG_WIRELESSREPEATER
	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_mode") == 0){
		update_lan_state(LAN_STATE_CONNECTED, 0);
		_dprintf("done\n");
		return 0;
	}
#endif

#ifdef RTCONFIG_DHCP_OVERRIDE
	if (nvram_get_int("sw_mode") == SW_MODE_AP && nvram_match("dnsqmode", "2")) {
		nvram_set("dnsqmode", "1");
		restart_dnsmasq(1);
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
	_dprintf("%s:: %s\n", __FUNCTION__, argv[1] ? : "");

        run_custom_script("dhcpc-event", argv[1]);

	if (!argv[1])
		return EINVAL;
	else if (strstr(argv[1], "deconfig"))
		return deconfig_lan();
	else if (strstr(argv[1], "bound"))
		return bound_lan();
	else if (strstr(argv[1], "renew"))
		return renew_lan();
/*	else if (strstr(argv[1], "leasefail")) */
/*	else if (strstr(argv[1], "nak")) */

	return EINVAL;
}

// -----------------------------------------------------------------------------

#ifdef RTCONFIG_IPV6
static int
deconfig6(char *wan_ifname)
{
	char *lan_ifname = nvram_safe_get("lan_ifname");

	if (nvram_invmatch("ipv6_wan_addr", "")) {
		eval("ip", "-6", "addr", "del", nvram_safe_get("ipv6_wan_addr"), "dev", wan_ifname);
		nvram_set("ipv6_wan_addr", "");
	}

	if (nvram_get_int("ipv6_dhcp_pd")) {
		if (nvram_invmatch("ipv6_prefix", "") ||
		    nvram_get_int("ipv6_prefix_length") != 0) {
			eval("ip", "-6", "addr", "flush", "scope", "global", "dev", lan_ifname);
			nvram_set("ipv6_rtr_addr", "");
			nvram_set("ipv6_prefix", "");
			nvram_set("ipv6_prefix_length", "");
		}
	}

	if (nvram_invmatch("ipv6_get_dns", "") ||
	    nvram_invmatch("ipv6_get_domain", "")) {
		nvram_set("ipv6_get_dns", "");
		nvram_set("ipv6_get_domain", "");
		if (nvram_get_int("ipv6_dnsenable"))
			update_resolvconf();
	}

	return 0;
}

static int
bound6(char *wan_ifname, int bound)
{
	char *lan_ifname = nvram_safe_get("lan_ifname");
	struct in6_addr range;
	char addr[INET6_ADDRSTRLEN + 1], *value;
	char tmp[100], *next;
	int wanaddr_changed, prefix_changed, dns_changed;
	int size, start, end, intval;

	value = safe_getenv("RA_HOPLIMIT");
	if (*value && (intval = atoi(value)))
		ipv6_sysconf(wan_ifname, "hop_limit", intval);

	value = safe_getenv("RA_MTU");
	if (*value && (intval = atoi(value)) && intval < ifconfig_mtu(wan_ifname, 0)) {
		ipv6_sysconf(wan_ifname, "mtu", intval);
		ipv6_sysconf(lan_ifname, "mtu", intval);
	} else if ((intval = ipv6_getconf(wan_ifname, "mtu")))
		ipv6_sysconf(lan_ifname, "mtu", intval);

	value = safe_getenv("ADDRESSES");
	if (*value) {
		foreach(tmp, value, next) {
			char *ptr = tmp;
			value = strsep(&ptr,",");
			break; /* only first address at the moment */
		}
	}
	wanaddr_changed = !nvram_match("ipv6_wan_addr", value);
	if (wanaddr_changed) {
		if (nvram_invmatch("ipv6_wan_addr", ""))
			eval("ip", "-6", "addr", "del", nvram_safe_get("ipv6_wan_addr"), "dev", wan_ifname);
		nvram_set("ipv6_wan_addr", value);
	}
	if (*value)
		eval("ip", "-6", "addr", "add", value, "dev", wan_ifname);

	prefix_changed = 0;
	if (nvram_get_int("ipv6_dhcp_pd")) {
		value = safe_getenv("PREFIXES");
		if (*value) {
			foreach(tmp, value, next) {
				char *ptr = tmp;
				value = strsep(&ptr,",");
				break; /* only first prefix at the moment */
			}
		}
		if (sscanf(value, "%[^/]/%d", addr, &size) != 2)
			goto skip;

		prefix_changed = (!nvram_match("ipv6_prefix", addr) ||
				  nvram_get_int("ipv6_prefix_length") != size);
		if (prefix_changed) {
			eval("ip", "-6", "addr", "flush", "scope", "global", "dev", lan_ifname);
			nvram_set("ipv6_rtr_addr", "");
			nvram_set("ipv6_prefix", addr);
			nvram_set_int("ipv6_prefix_length", size);
		}
		if (*addr)
			add_ip6_lanaddr();

		if (prefix_changed && nvram_get_int("ipv6_autoconf_type")) {
			/* TODO: rework WEB UI to specify ranges without prefix
			 * TODO: add size checking, now range takes all of 16 bit */
			start = (inet_pton(AF_INET6, nvram_safe_get("ipv6_dhcp_start"), &range) > 0) ?
			    ntohs(range.s6_addr16[7]) : 0x1000;
			end = (inet_pton(AF_INET6, nvram_safe_get("ipv6_dhcp_end"), &range) > 0) ?
			    ntohs(range.s6_addr16[7]) : 0x2000;

			value = nvram_safe_get("ipv6_prefix");
			inet_pton(AF_INET6, *value ? value : "::", &range);

			range.s6_addr16[7] = (start < end) ? htons(start) : htons(end);
			inet_ntop(AF_INET6, &range, addr, sizeof(addr));
			nvram_set("ipv6_dhcp_start", addr);
			range.s6_addr16[7] = (start < end) ? htons(end) : htons(start);
			inet_ntop(AF_INET6, &range, addr, sizeof(addr));
			nvram_set("ipv6_dhcp_end", addr);
		}
	}
skip:

	if (*safe_getenv("RDNSS")) {
		dns_changed = nvram_set_env("ipv6_get_dns", "RDNSS");
		dns_changed += nvram_set_env("ipv6_get_domain", "DOMAINS");
	} else {
		dns_changed = nvram_set_env("ipv6_get_dns", "RA_DNS");
		dns_changed += nvram_set_env("ipv6_get_domain", "RA_DOMAINS");
	}
	if (dns_changed && nvram_get_int("ipv6_dnsenable"))
		update_resolvconf();

	if (bound == 1 || wanaddr_changed || prefix_changed) {
		char *address = nvram_safe_get("ipv6_wan_addr");
		char *prefix = nvram_safe_get("ipv6_prefix");

		if (*prefix) {
			snprintf(addr, sizeof(addr), "%s/%d", prefix, nvram_get_int("ipv6_prefix_length"));
			prefix = addr;
		}
		logmessage("dhcp6 client", "%s %s%s%s%s%s",
			bound ? "bound" : "informed",
			*address ? "address " : "", address, *address ? ", " : "",
			*prefix ? "prefix " : "", prefix);
#ifdef RTCONFIG_IGD2
		notify_rc("restart_upnp");
#endif
	}

	return 0;
}

static int
ra_updated6(char *wan_ifname)
{
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char *value;
	int dns_changed, intval;

	value = safe_getenv("RA_HOPLIMIT");
	if (*value && (intval = atoi(value)))
		ipv6_sysconf(wan_ifname, "hop_limit", intval);

	value = safe_getenv("RA_MTU");
	if (*value && (intval = atoi(value)) && intval < ifconfig_mtu(wan_ifname, 0)) {
		ipv6_sysconf(wan_ifname, "mtu", intval);
		ipv6_sysconf(lan_ifname, "mtu", intval);
	}

	if (*safe_getenv("RDNSS")) {
		dns_changed = nvram_set_env("ipv6_get_dns", "RDNSS");
		dns_changed += nvram_set_env("ipv6_get_domain", "DOMAINS");
	} else {
		dns_changed = nvram_set_env("ipv6_get_dns", "RA_DNS");
		dns_changed += nvram_set_env("ipv6_get_domain", "RA_DOMAINS");
	}
	if (dns_changed && nvram_get_int("ipv6_dnsenable"))
		update_resolvconf();

	return 0;
}

int dhcp6c_wan(int argc, char **argv)
{

	if (argv[2]) run_custom_script("dhcpc-event", argv[2]);

	if (!argv[1] || !argv[2])
		return EINVAL;
	else if (strcmp(argv[2], "started") == 0 ||
		 strcmp(argv[2], "stopped") == 0 ||
		 strcmp(argv[2], "unbound") == 0)
		return deconfig6(argv[1]);
	else if (strcmp(argv[2], "bound") == 0)
		return bound6(argv[1], 1);
	else if (strcmp(argv[2], "updated") == 0 ||
		 strcmp(argv[2], "rebound") == 0)
		return bound6(argv[1], 2);
	else if (strcmp(argv[2], "informed") == 0)
		return bound6(argv[1], 0);
	else if (strcmp(argv[2], "ra-updated") == 0)
		return ra_updated6(argv[2]);

	return 0;
}

int
start_dhcp6c(void)
{
	char *wan_ifname = (char *)get_wan6face();
	char *dhcp6c_argv[] = { "odhcp6c",
#ifndef RTCONFIG_BCMARM
		"-f",
#else
		"-df",
#endif
		"-R",
		"-s", "/tmp/dhcp6c",
		"-N", "try",
		NULL, NULL,	/* -c duid */
		NULL, NULL,	/* -FP len:iaidhex */
		NULL, NULL,	/* -rdns -rdomain */
		NULL, NULL, 	/* -rsolmaxrt -r infmaxrt */
		NULL,		/* -v */
		NULL,		/* interface */
		NULL };
	int index = 7;
	unsigned long iaid = 0;
	struct {
		uint16_t type;
		uint16_t hwtype;
		unsigned char ea[ETHER_ADDR_LEN];
	} __attribute__ ((__packed__)) duid;
	char duid_arg[sizeof(duid)*2+1];
	char prefix_arg[sizeof("128:xxxxxxxx")];
	int i;
#ifndef RTCONFIG_BCMARM
	pid_t pid;
#endif

	/* Check if enabled */
	if (get_ipv6_service() != IPV6_NATIVE_DHCP)
		return 0;

	if (!wan_ifname || *wan_ifname == '\0')
		return -1;

	stop_dhcp6c();

	if (ether_atoe(nvram_safe_get("wan0_hwaddr"), duid.ea)) {
		/* Generate DUID-LL */
		duid.type = htons(3);	/* DUID-LL */
		duid.hwtype = htons(1);	/* Ethernet */

		/* Generate IAID from the last 7 digits of WAN MAC */
		iaid =	((unsigned long)(duid.ea[3] & 0x0f) << 16) |
			((unsigned long)(duid.ea[4]) << 8) |
			((unsigned long)(duid.ea[5]));

		bin2hex(duid_arg, sizeof(duid_arg), &duid, sizeof(duid));

		dhcp6c_argv[index++] = "-c";
		dhcp6c_argv[index++] = duid_arg;
	}

	if (nvram_get_int("ipv6_dhcp_pd")) {
		i = 64 - (nvram_get_int("ipv6_prefix_length") ? : 64);
		if (i < 0)
			i = 0;
		snprintf(prefix_arg, sizeof(prefix_arg), iaid ? "%d:%lx" : "%d", i, iaid);

		dhcp6c_argv[index++] = "-FP";
		dhcp6c_argv[index++] = prefix_arg;
	}

	if (nvram_get_int("ipv6_dnsenable")) {
		dhcp6c_argv[index++] = "-r23";	/* dns */
		dhcp6c_argv[index++] = "-r24";	/* domain */
	}
	dhcp6c_argv[index++] = "-r82";	/* sol_max_rt */
	dhcp6c_argv[index++] = "-r83";	/* inf_max_rt */

	if (nvram_get_int("ipv6_debug"))
		dhcp6c_argv[index++] = "-v";

	dhcp6c_argv[index++] = wan_ifname;

#ifndef RTCONFIG_BCMARM
	return _eval(dhcp6c_argv, NULL, 0, &pid);
#else
	return _eval(dhcp6c_argv, NULL, 0, NULL);
#endif
}

void stop_dhcp6c(void)
{
	killall_tk("odhcp6c");
}
#endif // RTCONFIG_IPV6
