/*
 * Miscellaneous services
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
 * $Id: services.c,v 1.100 2010/03/04 09:39:18 Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#include <rc.h>
#include <semaphore_mfp.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <sys/reboot.h>
#include <errno.h>
#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif
#include <shared.h>

#define sin_addr(s) (((struct sockaddr_in *)(s))->sin_addr)

// Pop an alarm to recheck pids in 500 msec.
static const struct itimerval pop_tv = { {0,0}, {0, 500 * 1000} };

// Pop an alarm to reap zombies. 
static const struct itimerval zombie_tv = { {0,0}, {307, 0} };

// -----------------------------------------------------------------------------

static const char dmhosts[] = "/etc/hosts.dnsmasq";
#ifndef OVERWRITE_DNS
static const char dmresolv[] = "/etc/resolv.conf";
#else
static const char dmresolv[] = "/tmp/resolv.conf";
#endif

static pid_t pid_dnsmasq = -1;


#ifdef BCMDBG
#include <assert.h>
#else
#define assert(a)
#endif

#define logs(s) syslog(LOG_NOTICE, s)

#if 0
static char
*make_var(char *prefix, int index, char *name)
{
	static char buf[100];

	assert(prefix);
	assert(name);

	if (index)
		snprintf(buf, sizeof(buf), "%s%d%s", prefix, index, name);
	else
		snprintf(buf, sizeof(buf), "%s%s", prefix, name);
	return buf;
}

static int is_wet(int idx, int unit, int subunit, void *param)
{
	return nvram_match(wl_nvname("mode", unit, subunit), "wet");
}
#endif

void create_passwd(void)
{
	char s[512];
	char *p;
	char salt[32];
	FILE *f;
	mode_t m;
	char *http_user;

#ifdef RTCONFIG_SAMBASRV	//!!TB
	char *smbd_user;
	
	create_custom_passwd();
#endif

	strcpy(salt, "$1$");
	f_read("/dev/urandom", s, 6);
	base64_encode(s, salt + 3, 6);
	salt[3 + 8] = 0;
	p = salt;
	while (*p) {
		if (*p == '+') *p = '.';
		++p;
	}
	if (((p = nvram_get("http_passwd")) == NULL) || (*p == 0)) p = "admin";

	if (((http_user = nvram_get("http_username")) == NULL) || (*http_user == 0)) http_user = "admin";

#ifdef RTCONFIG_SAMBASRV	//!!TB
	if (((smbd_user = nvram_get("smbd_user")) == NULL) || (*smbd_user == 0) || !strcmp(smbd_user, "root"))
		smbd_user = "nas";
#endif

	m = umask(0777);
	if ((f = fopen("/etc/shadow", "w")) != NULL) {
		p = crypt(p, salt);
		fprintf(f, "%s:%s:0:0:99999:7:0:0:\n"
				   "nobody:*:0:0:99999:7:0:0:\n", http_user, p);
#ifdef RTCONFIG_SAMBASRV	//!!TB
		fprintf(f, "%s:*:0:0:99999:7:0:0:\n", smbd_user);
#endif

		fappend(f, "/etc/shadow.custom");
		append_custom_config("shadow", f);
		fclose(f);
	}
	umask(m);
	chmod("/etc/shadow", 0600);

#ifdef RTCONFIG_SAMBASRV	//!!TB
	sprintf(s,
		"%s:x:0:0:%s:/root:/bin/sh\n"
		"%s:x:100:100:nas:/dev/null:/dev/null\n"
		"nobody:x:65534:65534:nobody:/dev/null:/dev/null\n",
		http_user,
		http_user,
		smbd_user);
#else	//!!TB
	sprintf(s,
		"%s:x:0:0:%s:/root:/bin/sh\n"
		"nobody:x:65534:65534:nobody:/dev/null:/dev/null\n",
		http_user,
		http_user);
#endif	//!!TB
	f_write_string("/etc/passwd", s, 0, 0644);
	fappend_file("/etc/passwd", "/etc/passwd.custom");

//	append_custom_config() - saves us from opening the file first
	fappend_file("/etc/passwd", "/jffs/configs/passwd.add");

	sprintf(s,
		"%s:*:0:\n"
#ifdef RTCONFIG_SAMBASRV	//!!TB
		"nas:*:100:\n"
#endif
		"nobody:*:65534:\n",
		http_user);
	f_write_string("/etc/gshadow", s, 0, 0644);
	fappend_file("/etc/gshadow", "/etc/gshadow.custom");
//      append_custom_config();
        fappend_file("/etc/gshadow", "/jffs/configs/gshadow.add");

	f_write_string("/etc/group",
		"root:x:0:\n"
#ifdef RTCONFIG_SAMBASRV	//!!TB
		"nas:x:100:\n"
#endif
		"nobody:x:65534:\n",
		0, 0644);
	fappend_file("/etc/group", "/etc/group.custom");
//      append_custom_config();
        fappend_file("/etc/group", "/jffs/configs/group.add");
}

void start_dnsmasq()
{
	FILE *fp;
	char *lan_ifname;
	char *lan_ipaddr;
	char *nv;
	int n;

	TRACE_PT("begin\n");

	if (getpid() != 1) {
		notify_rc("start_dnsmasq");
		return;
	}

	lan_ifname = nvram_safe_get("lan_ifname");
#ifdef RTCONFIG_WIRELESSREPEATER
	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_state") != WLC_STATE_CONNECTED){
		lan_ipaddr = nvram_default_get("lan_ipaddr");
	}
	else
#endif
	{
		lan_ipaddr = nvram_safe_get("lan_ipaddr");
	}

	/* write /etc/hosts */
	if ((fp = fopen("/etc/hosts", "w")) != NULL) {
		/* loclhost ipv4 */
		fprintf(fp, "127.0.0.1 localhost.localdomain localhost\n"
			    "%s www.asusnetwork.net\n", lan_ipaddr);
		/* productid/samba name */
		if (is_valid_hostname(nv = nvram_safe_get("computer_name")) ||
		    is_valid_hostname(nv = get_productid()))
			fprintf(fp, "%s %s.%s %s\n", lan_ipaddr,
				    nv, nvram_safe_get("lan_domain"), nv);
		/* lan hostname.domain hostname */
		if (nvram_invmatch("lan_hostname", "")) {
			fprintf(fp, "%s %s.%s %s\n", lan_ipaddr,
				    nvram_safe_get("lan_hostname"),
				    nvram_safe_get("lan_domain"),
				    nvram_safe_get("lan_hostname"));
		}
#ifdef RTCONFIG_IPV6
		if (ipv6_enabled()) {
			/* localhost ipv6 */
			fprintf(fp, "::1 localhost6.localdomain6 localhost6\n");
			/* lan6 hostname.domain hostname */
			nv = ipv6_router_address(NULL);
			if (*nv && nvram_invmatch("lan_hostname", "")) {
				fprintf(fp, "%s %s.%s %s\n", nv,
					    nvram_safe_get("lan_hostname"),
					    nvram_safe_get("lan_domain"),
					    nvram_safe_get("lan_hostname"));
			}
		}
#endif
		append_custom_config("hosts", fp);
		fclose(fp);
		use_custom_config("hosts", "/etc/hosts");
	} else
		perror("/etc/hosts");

	if (!is_routing_enabled()
#ifdef RTCONFIG_WIRELESSREPEATER
	 && nvram_get_int("sw_mode") != SW_MODE_REPEATER
#endif
	) return;

	// we still need dnsmasq in wet
	//if (foreach_wif(1, NULL, is_wet)) return;

	if ((fp = fopen("/etc/dnsmasq.conf", "w")) == NULL)
		return;

	fprintf(fp,
		"pid-file=/var/run/dnsmasq.pid\n"
		"user=nobody\n"
		"resolv-file=%s\n"		// the real stuff is here
		"no-poll\n"			// don't poll resolv file
		"interface=%s\n"		// dns & dhcp only on LAN interface
		"min-port=%u\n",		// min port used for random src port
		dmresolv,
		lan_ifname,
		nvram_get_int("dns_minport") ? : 4096);

	/* legacy: DNS servers */
	const dns_list_t *dns = get_dns();	// this always points to a static buffer
	for (n = 0 ; n < dns->count; ++n) {
		if (dns->dns[n].port != 53)
			fprintf(fp, "server=%s#%u\n", inet_ntoa(dns->dns[n].addr), dns->dns[n].port);
	}

	/* lan domain */
	nv = nvram_safe_get("lan_domain");
	if (*nv) {
		fprintf(fp, "domain=%s\n"
			    "expand-hosts\n", nv);	// expand hostnames in hosts file
	}

	/* caching */
	fprintf(fp, "no-negcache\n"
		    "cache-size=1500\n");

	if ((is_routing_enabled() && nvram_get_int("dhcp_enable_x"))
#ifdef RTCONFIG_WIRELESSREPEATER
	 || (nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_state") != WLC_STATE_CONNECTED)
#endif
	) {
		char *dhcp_start, *dhcp_end;
		int dhcp_lease;

#ifdef RTCONFIG_WIRELESSREPEATER
		if(nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_state") != WLC_STATE_CONNECTED){
			dhcp_start = nvram_default_get("dhcp_start");
			dhcp_end = nvram_default_get("dhcp_end");
			dhcp_lease = atoi(nvram_default_get("dhcp_lease"));
		}
		else
#endif
		{
			dhcp_start = nvram_safe_get("dhcp_start");
			dhcp_end = nvram_safe_get("dhcp_end");
			dhcp_lease = nvram_get_int("dhcp_lease");
		}

		if (dhcp_lease <= 0) dhcp_lease = 86400;

		/* LAN range */
		if (*dhcp_start && *dhcp_end) {
			fprintf(fp, "dhcp-range=lan,%s,%s,%s,%ds\n",
				dhcp_start, dhcp_end, nvram_safe_get("lan_netmask"), dhcp_lease);
		} else {
			/* compatibility */
			char lan[24];
			int start = nvram_get_int("dhcp_start");
			int count = nvram_get_int("dhcp_num");

			strlcpy(lan, lan_ipaddr, sizeof(lan));
			if ((nv = strrchr(lan, '.')) != NULL) *(nv + 1) = 0;

			fprintf(fp, "dhcp-range=lan,%s%d,%s%d,%s,%ds\n",
				lan, start, lan, start + count - 1, nvram_safe_get("lan_netmask"), dhcp_lease);
		}
		if ((n = nvram_get_int("dhcpd_lmax")) > 0)
			fprintf(fp, "dhcp-lease-max=%d\n", n);

		/* Faster for moving clients, if authoritative */
		if (nvram_get_int("dhcpd_auth") >= 0)
			fprintf(fp, "dhcp-authoritative\n");

		/* LAN Domain */
		nv = nvram_safe_get("lan_domain");
		if (*nv)
			fprintf(fp, "dhcp-option=lan,15,%s\n", nv);

		/* Gateway, if not set, force use lan ipaddr to avoid repeater issue */
		nv = nvram_safe_get("dhcp_gateway_x");
		nv = (*nv && inet_addr(nv)) ? nv : lan_ipaddr;
		fprintf(fp, "dhcp-option=lan,3,%s\n", nv);

		/* DNS server and additional router address */
		nv = nvram_safe_get("dhcp_dns1_x");
		if (*nv && inet_addr(nv))
			fprintf(fp, "dhcp-option=lan,6,%s,0.0.0.0\n", nv);

		/* WINS server */
		nv = nvram_safe_get("dhcp_wins_x");
		if (*nv && inet_addr(nv)) {
			fprintf(fp, "dhcp-option=lan,44,%s\n"
			/*	    "dhcp-option=lan,46,8\n"*/, nv);
		}
#ifdef RTCONFIG_SAMBASRV
		/* Samba will serve as a WINS server */
		else if (nvram_get_int("smbd_enable") && nvram_invmatch("lan_domain", "") && nvram_get_int("smbd_wins")) {
			fprintf(fp, "dhcp-option=lan,44,0.0.0.0\n"
			/*	    "dhcp-option=lan,46,8\n"*/);
		}
#endif
	} else
		fprintf(fp, "no-dhcp-interface=%s\n", lan_ifname);

	/* Static IP MAC binding */
	if (nvram_match("dhcp_static_x","1")) {
		fprintf(fp, "read-ethers\n");
		write_static_leases("/etc/ethers");
	}

	/* Listen to PPTPD clients */
	if (nvram_match("pptpd_enable","1"))
	fprintf(fp,"listen-address=%s,127.0.0.1\n",lan_ipaddr);

	/* Don't log DHCP queries */
	if (nvram_match("dhcpd_querylog","0"))
		fprintf(fp,"log-dhcp=none\n");

#ifdef RTCONFIG_OPENVPN
	write_vpn_dnsmasq_config(fp);
#endif

	append_custom_config("dnsmasq.conf",fp);

	fclose(fp);

	use_custom_config("dnsmasq.conf","/etc/dnsmasq.conf");

	eval("touch", "/tmp/resolv.conf");
	chmod("/tmp/resolv.conf", 0666);
#ifndef OVERWRITE_DNS
	unlink("/etc/resolv.conf");
	symlink("/tmp/resolv.conf", "/etc/resolv.conf");
#endif

	eval("dnsmasq", "--log-async");
	if (!nvram_contains_word("debug_norestart", "dnsmasq")) {
		pid_dnsmasq = -2;
	}

	TRACE_PT("end\n");
}

void stop_dnsmasq(void)
{
	TRACE_PT("begin\n");

	if (getpid() != 1) {
		notify_rc("stop_dnsmasq");
		return;
	}

	pid_dnsmasq = -1;

	killall_tk("dnsmasq");

	TRACE_PT("end\n");
}

void restart_dnsmasq(void)
{
	stop_dnsmasq();
	start_dnsmasq();
}

void clear_resolv(void)
{
	f_write(dmresolv, NULL, 0, 0, 0);	// blank
}

#ifdef RTCONFIG_IPV6
int write_ipv6_dns_servers(FILE *f, const char *prefix, char *dns, const char *suffix, int once)
{
	char p[INET6_ADDRSTRLEN + 1], *next = NULL;
	struct in6_addr addr;
	int cnt = 0;

	foreach(p, dns, next) {
		// verify that this is a valid IPv6 address
		if (inet_pton(AF_INET6, p, &addr) == 1) {
			fprintf(f, "%s%s%s", (once && cnt) ? "" : prefix, p, suffix);
			++cnt;
		}
	}

	return cnt;
}
#endif

#if 0
void dns_to_resolv(void)
{
	FILE *f;
	const dns_list_t *dns;
	int i;
	mode_t m;

	m = umask(022);	// 077 from pppoecd
	if ((f = fopen(dmresolv, "w")) != NULL) {
		// Check for VPN DNS entries
		if (!write_vpn_resolv(f)) {
			dns = get_dns();	// static buffer
			if (dns->count == 0) {
				// Put a pseudo DNS IP to trigger Connect On Demand
			}
			else {
				for (i = 0; i < dns->count; i++) {
					if (dns->dns[i].port == 53) {	// resolv.conf doesn't allow for an alternate port
						fprintf(f, "nameserver %s\n", inet_ntoa(dns->dns[i].addr));
					}
				}
			}
		}
		fclose(f);
	}
	umask(m);
}
#endif

// -----------------------------------------------------------------------------
#ifdef RTCONFIG_IPV6
static void add_ip6_lanaddr(void)
{
	char ip[INET6_ADDRSTRLEN + 4];
	char *p;

	p = ipv6_router_address(NULL);
	if (*p) {
		if (!nvram_invmatch("ipv6_rtr_addr", "")) nvram_set("ipv6_rtr_addr", p);

		snprintf(ip, sizeof(ip), "%s/%d", p, nvram_get_int("ipv6_prefix_length") ? : 64);
		eval("ip", "-6", "addr", "add", ip, "dev", nvram_safe_get("lan_ifname"));
	}
}

void start_ipv6_tunnel(void)
{
	char ip[INET6_ADDRSTRLEN + 4];
	char router[INET6_ADDRSTRLEN + 1];
	struct in_addr addr4;
	struct in6_addr addr;
	char *wanip, *mtu, *tun_dev, *gateway;
	int service;
	int prefixlen;

	// for one wan only now
	service = get_ipv6_service();
	tun_dev = get_wan6face();
	wanip = get_wanip();

	mtu = (nvram_get_int("ipv6_tun_mtu") > 0) ? nvram_safe_get("ipv6_tun_mtu") : "1480";
	modprobe("sit");

	eval("ip", "tunnel", "add", tun_dev, "mode", "sit",
		"remote", (service == IPV6_6IN4) ? nvram_safe_get("ipv6_tun_v4end") : "any",
		"local", wanip,
		"ttl", nvram_safe_get("ipv6_tun_ttl"));
	eval("ip", "link", "set", tun_dev, "mtu", mtu, "up");
	nvram_set("ipv6_ifname", tun_dev);

	switch (service) {
	case IPV6_6TO4: {
		int prefixlen = 16;
		int mask4size = 0;

		/* address */
		addr4.s_addr = 0;
		memset(&addr, 0, sizeof(addr));
		inet_aton(wanip, &addr4);
		addr.s6_addr16[0] = htons(0x2002);
		ipv6_mapaddr4(&addr, prefixlen, &addr4, mask4size);
		//addr.s6_addr16[7] |= htons(0x0001);
		inet_ntop(AF_INET6, &addr, ip, sizeof(ip));
		snprintf(ip, sizeof(ip), "%s/%d", ip, prefixlen);

		/* gateway */
		snprintf(router, sizeof(router), "::%s", nvram_safe_get("ipv6_relay"));
		gateway = router;

		add_ip6_lanaddr();
		break;
	}
	case IPV6_6RD: {
		int prefixlen = nvram_get_int("ipv6_6rd_prefixlen");
		int mask4size = nvram_get_int("ipv6_6rd_ip4size");
		char brprefix[sizeof("255.255.255.255/32")];

		/* 6rd domain */
		addr4.s_addr = 0;
		if (mask4size) {
			inet_aton(wanip, &addr4);
			addr4.s_addr &= htonl(0xffffffffUL << (32 - mask4size));
		} else	addr4.s_addr = 0;
		snprintf(ip, sizeof(ip), "%s/%d", nvram_safe_get("ipv6_6rd_prefix"), prefixlen);
		snprintf(brprefix, sizeof(brprefix), "%s/%d", inet_ntoa(addr4), mask4size);
		eval("ip", "tunnel", "6rd", "dev", tun_dev,
		     "6rd-prefix", ip, "6rd-relay_prefix", brprefix);

		/* address */
		addr4.s_addr = 0;
		memset(&addr, 0, sizeof(addr));
		inet_aton(wanip, &addr4);
		inet_pton(AF_INET6, nvram_safe_get("ipv6_6rd_prefix"), &addr);
		ipv6_mapaddr4(&addr, prefixlen, &addr4, mask4size);
		//addr.s6_addr16[7] |= htons(0x0001);
		inet_ntop(AF_INET6, &addr, ip, sizeof(ip));
		snprintf(ip, sizeof(ip), "%s/%d", ip, prefixlen);

		/* gateway */
		snprintf(router, sizeof(router), "::%s", nvram_safe_get("ipv6_6rd_router"));
		gateway = router;

		add_ip6_lanaddr();
		break;
	}
	default:
		/* address */
		snprintf(ip, sizeof(ip), "%s/%d",
			nvram_safe_get("ipv6_tun_addr"),
			nvram_get_int("ipv6_tun_addrlen") ? : 64);

		/* TODO: add & allow gateway */
		gateway = NULL;
	}

	eval("ip", "-6", "addr", "add", ip, "dev", tun_dev);
	if (gateway && *gateway)
		eval("ip", "-6", "route", "add", "::/0", "via", gateway, "dev", tun_dev, "metric", "1");
	else	eval("ip", "-6", "route", "add", "::/0", "dev", tun_dev, "metric", "1");
}

void stop_ipv6_tunnel(void)
{
	int service = get_ipv6_service();

	if (service == IPV6_6TO4 || service == IPV6_6RD || service == IPV6_6IN4) {
		eval("ip", "tunnel", "del", (char *)get_wan6face());
	}
	if (service == IPV6_6TO4 || service == IPV6_6RD) {
		// get rid of old IPv6 address from lan iface
		eval("ip", "-6", "addr", "flush", "dev", nvram_safe_get("lan_ifname"), "scope", "global");
		nvram_set("ipv6_rtr_addr", "");
		nvram_set("ipv6_prefix", "");
	}
	modprobe_r("sit");
}

void start_dhcp6s(void)
{
	FILE *fp;
	pid_t pid;
	char *p, ipv6_dns_str[1024] = "";
	char *dhcp6s_argv[] = { "dhcp6s",
		"-c", "/etc/dhcp6s.conf",
		nvram_safe_get("lan_ifname"),
		NULL,		/* -D */
		NULL };
	int index = 4;		/* first NULL */

	if (getpid() != 1) {
		notify_rc("start_dhcp6s");
		return;
	}

	if (!ipv6_enabled())
		return;

	/* create dhcp6s.conf */
	if ((fp = fopen("/etc/dhcp6s.conf", "w")) == NULL)
		return;

	/* avoid infinite/or stop working IPv6 DNS on clients */
	fprintf(fp, "option refreshtime %d;\n", 900); /* 15 minutes for now */

	if ((get_ipv6_service() == IPV6_NATIVE_DHCP) && nvram_match("ipv6_dnsenable", "1")) {
		p = nvram_invmatch("ipv6_get_dns", "") ?
			nvram_safe_get("ipv6_get_dns") :
			getifaddr(nvram_safe_get("lan_ifname"), AF_INET6, 1) ? : "";
	} else {
		char nvname[sizeof("ipv6_dnsXXX")];
		char *next = ipv6_dns_str;
		int i;

		for (i = 0; i < 3; i++) {
			snprintf(nvname, sizeof(nvname), "ipv6_dns%d", i + 1);
			p = nvram_safe_get(nvname);
			/* TODO: make validation ipv6 address */
			next += sprintf(next, *ipv6_dns_str ? " %s" : "%s", p);
		}
		p = ipv6_dns_str;
	}
	if (p && *p)
		fprintf(fp, "option domain-name-servers %s;\n", p);

	append_custom_config("dhcp6s.conf", fp);
	fclose(fp);

	use_custom_config("dhcp6s.conf", "/etc/dhcp6s.conf");

	if (nvram_get_int("ipv6_debug"))
		dhcp6s_argv[index++] = "-D";

	_eval(dhcp6s_argv, NULL, 0, &pid);
}

int
start_ipv6aide()
{
	char *ipv6aide_argv[] = {"ipv6aide", NULL};
	pid_t pid;

	if ((get_ipv6_service() == IPV6_NATIVE_DHCP) &&
		!nvram_match("ipv6_comcast", "0"))
		return _eval(ipv6aide_argv, NULL, 0, &pid);

	return 0;
}

int
stop_ipv6aide()
{
	if (pids("ipv6aide"))
		killall("ipv6aide", SIGTERM);

	return 0;
}

static pid_t pid_radvd = -1;

void start_radvd(void)
{
	FILE *f;
	char *prefix, *ip, *mtu;
	int do_dns, do_6to4, do_6rd;
	char *argv[] = { "radvd", NULL, NULL, NULL, NULL, NULL, NULL };
	int pid, argc, service, cnt;
	char *p = NULL;

	if (getpid() != 1) {
		notify_rc("start_radvd");
		return;
	}

	stop_radvd();

	stop_dhcp6s();
	start_dhcp6s();

	stop_ipv6aide();
	start_ipv6aide();

	if (ipv6_enabled() && nvram_get_int("ipv6_radvd")) {
		service = get_ipv6_service();
		do_6to4 = (service == IPV6_6TO4);
		do_6rd = (service == IPV6_6RD);
		mtu = NULL;

		switch (service) {
		case IPV6_NATIVE_DHCP:
			prefix = "::";
		case IPV6_6TO4:
		case IPV6_6IN4:
		case IPV6_6RD:
			mtu = (nvram_get_int("ipv6_tun_mtu") > 0) ? nvram_safe_get("ipv6_tun_mtu") : "1480";
			// fall through
		default:
			prefix = do_6to4 ? "0:0:0:1::" : nvram_safe_get("ipv6_prefix");
			break;
		}
		if (!(*prefix) || (strlen(prefix) <= 0)) prefix = "::";

		// Create radvd.conf
		if ((f = fopen("/etc/radvd.conf", "w")) == NULL) return;
#if 0
		ip = (char *)ipv6_router_address(NULL);
#else
		ip = getifaddr(nvram_safe_get("lan_ifname"), AF_INET6, 1) ? : "";
#endif
#if 0
		do_dns = (*ip);
#else
		do_dns = 0;
#endif
		fprintf(f,
			"interface %s\n"
			"{\n"
			" IgnoreIfMissing on;\n"
			" AdvSendAdvert on;\n"
#if 1
			" MinRtrAdvInterval 3;\n"
			" MaxRtrAdvInterval 10;\n"
#else
			" MaxRtrAdvInterval 60;\n"
#endif
			" AdvHomeAgentFlag off;\n"
			" AdvManagedFlag off;\n"
			" AdvOtherConfigFlag %s;\n"
			"%s%s%s"
			" prefix %s/64 \n"
			" {\n"
			"  AdvOnLink on;\n"
			"  AdvAutonomous on;\n"
			"%s"
			"%s%s%s"
			" };\n",
			nvram_safe_get("lan_ifname"),
#if 0
			(get_ipv6_service() != IPV6_NATIVE) && (get_ipv6_service() != IPV6_NATIVE_DHCP) ? "on" : "off",
#else
			"on",
#endif
			mtu ? " AdvLinkMTU " : "", mtu ? : "", mtu ? ";\n" : "",
			prefix,
			do_6to4 | do_6rd ? "  AdvValidLifetime 300;\n  AdvPreferredLifetime 120;\n" : "",
			do_6to4 ? "  Base6to4Interface " : "",
			do_6to4 ? get_wanface()  : "",
			do_6to4 ? ";\n" : "");

		if (do_dns) {
			fprintf(f, " RDNSS %s {};\n", ip);
		}
		else {
			char ipv6_dns_str[1024] = "";
			char nvname[sizeof("ipv6_dnsXXX")];
			char *next = ipv6_dns_str;
			int i;

			for (i = 0; i < 3; i++) {
				snprintf(nvname, sizeof(nvname), "ipv6_dns%d", i + 1);
				p = nvram_safe_get(nvname);
				/* TODO: make validation ipv6 address */
				next += sprintf(next, *ipv6_dns_str ? " %s" : "%s", p);
			}

			if ((get_ipv6_service() == IPV6_NATIVE_DHCP) && nvram_match("ipv6_dnsenable", "1"))
				p = nvram_safe_get("ipv6_get_dns");
			else
				p = ipv6_dns_str;

			cnt = write_ipv6_dns_servers(f, " RDNSS ", (char*) ((p && *p) ? p : ip), " ", 1);
			if (cnt) fprintf(f, "{};\n");
		}

		fprintf(f,
			"};\n");	// close "interface" section

		append_custom_config("radvd.conf", f);
		fclose(f);

		use_custom_config("radvd.conf", "/etc/radvd.conf");

		chmod("/etc/radvd.conf", 0400);

		f_write_string("/proc/sys/net/ipv6/conf/all/forwarding", "1", 0, 0);

		// Start radvd
		argc = 1;
		argv[argc++] = "-u";
		argv[argc++] = nvram_safe_get("http_username");
		if (nvram_get_int("ipv6_debug")) {
			argv[argc++] = "-d";
			argv[argc++] = "5";
		}
		argv[argc] = NULL;
		_eval(argv, NULL, 0, &pid);

		if (!nvram_contains_word("debug_norestart", "radvd")) {
			pid_radvd = -2;
		}
	}
}

void stop_radvd(void)
{
	if (getpid() != 1) {
		notify_rc("stop_radvd");
		return;
	}

	stop_dhcp6s();
	stop_ipv6aide();

	pid_radvd = -1;
	killall_tk("radvd");

	f_write_string("/proc/sys/net/ipv6/conf/all/forwarding", "0", 0, 0);
}

void stop_dhcp6s(void)
{
	if (getpid() != 1) {
		notify_rc("stop_dhcp6s");
		return;
	}

	killall_tk("dhcp6s");
}

void start_ipv6(void)
{
	int service;

	service = get_ipv6_service();

	if (service != IPV6_DISABLED)
		doSystem("echo 86400 > /proc/sys/net/ipv6/neigh/%s/gc_stale_time", nvram_safe_get("lan_ifname"));

	// Check if turned on
	switch (service) {
	case IPV6_6IN4:
		nvram_set("ipv6_rtr_addr", "");
	case IPV6_NATIVE:
	case IPV6_MANUAL:
		add_ip6_lanaddr();
		break;
	case IPV6_NATIVE_DHCP:
	case IPV6_6TO4:
	case IPV6_6RD:
	default:
		nvram_set("ipv6_rtr_addr", "");
		nvram_set("ipv6_prefix", "");
		break;
	}
}

void stop_ipv6(void)
{
	stop_ipv6_tunnel();
	stop_dhcp6c();
	eval("ip", "-6", "addr", "flush", "scope", "all");
	eval("ip", "-6", "route", "flush", "scope", "all");
	eval("ip", "-6", "neigh", "flush", "dev", nvram_safe_get("lan_ifname"));
}
#endif

// -----------------------------------------------------------------------------

int no_need_to_start_wps()
{
	int i, wps_band;
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
	char word[256], *next;

	if (!nvram_match("sw_mode", "1"))
		return 1;

	i = 0;
	wps_band = nvram_get_int("wps_band");

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if (i == wps_band) {
			snprintf(prefix, sizeof(prefix), "wl%d_", i);

			if (	/*(nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open") &&
					!nvram_match(strcat_r(prefix, "wep_x", tmp), "0")) ||*/
				nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "shared") ||
				/*(strstr(nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp)), "psk") &&
					nvram_match(strcat_r(prefix, "crypto", tmp), "tkip")) ||*/
				strstr(nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp)), "wpa") ||
                                nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius") ||
#if 0
				!get_radio(i, -1)
#else
				nvram_match(strcat_r(prefix, "radio", tmp), "0")
#endif
			)
				return 1;
		}

		i++;
	}

	return 0;
}

int 
wl_wpsPincheck(char *pin_string)
{
	unsigned long PIN = strtoul(pin_string, NULL, 10);
	unsigned long int accum = 0;
	unsigned int len = strlen(pin_string);

	if (len != 4 && len != 8)
		return 	-1;

	if (len == 8) {
		accum += 3 * ((PIN / 10000000) % 10);
		accum += 1 * ((PIN / 1000000) % 10);
		accum += 3 * ((PIN / 100000) % 10);
		accum += 1 * ((PIN / 10000) % 10);
		accum += 3 * ((PIN / 1000) % 10);
		accum += 1 * ((PIN / 100) % 10);
		accum += 3 * ((PIN / 10) % 10);
		accum += 1 * ((PIN / 1) % 10);

		if (0 == (accum % 10))
			return 0;
	}
	else if (len == 4)
		return 0;

	return -1;
}

int 
start_wps_pbc(int unit)
{
	if (!no_need_to_start_wps() && nvram_match("wps_enable", "0"))
	{
		nvram_set("wps_enable", "1");
#ifdef CONFIG_BCMWL5
#ifdef RTCONFIG_BCMWL6
#ifdef ACS_ONCE
		restart_wireless_acsd();
#else
		restart_wireless();
#endif
		int delay_count = 10;
		while ((delay_count-- > 0) && !nvram_match("wlready", "1"))
			sleep(1);
#else
		restart_wireless();
#endif
#else
		stop_wps();
		nvram_set_int("wps_band", unit);
		start_wps();
#endif
	}

	nvram_set_int("wps_band", unit);
	nvram_set("wps_sta_pin", "00000000");

	return start_wps_method();
}

int 
start_wps_pin(int unit)
{
	if(!strlen(nvram_safe_get("wps_sta_pin"))) return 0;

	if (wl_wpsPincheck(nvram_safe_get("wps_sta_pin"))) return 0;

	nvram_set_int("wps_band", unit);

	return start_wps_method();
}

int
start_wpsaide()
{
	char *wpsaide_argv[] = {"wpsaide", NULL};
	pid_t pid;

	return _eval(wpsaide_argv, NULL, 0, &pid);
}

int
stop_wpsaide()
{
	if (pids("wpsaide"))
		killall("wpsaide", SIGTERM);

	return 0;
}

int
start_wps(void)
{
#ifdef RTCONFIG_WPS
#ifdef CONFIG_BCMWL5
	char *wps_argv[] = {"/bin/wps_monitor", NULL};
	pid_t pid;
#endif
	if (no_need_to_start_wps())
		nvram_set("wps_enable", "0");

	if (nvram_match("wps_restart", "1")) {
		nvram_set("wps_restart", "0");
	}
	else {
		nvram_set("wps_restart", "0");
		nvram_set("wps_proc_status", "0");
	}

	nvram_set("wps_sta_pin", "00000000");
	if (nvram_match("w_Setting", "1"))
		nvram_set("lan_wps_oob", "disabled");
	else
		nvram_set("lan_wps_oob", "enabled");
#ifdef CONFIG_BCMWL5
	killall_tk("wps_monitor");
	killall_tk("wps_ap");
	killall_tk("wps_enr");
	unlink("/tmp/wps_monitor.pid");
#endif
	if (nvram_match("wps_enable", "1"))
	{
#ifdef CONFIG_BCMWL5
		nvram_set("wl_wps_mode", "enabled");
#endif
#ifdef CONFIG_BCMWL5
		_eval(wps_argv, NULL, 0, &pid);
#elif defined RTCONFIG_RALINK
		start_wsc_pin_enrollee();
		start_wpsfix();
		if (f_exists("/var/run/watchdog.pid"))
		{
			doSystem("iwpriv %s set WatchdogPid=`cat %s`", WIF_2G, "/var/run/watchdog.pid");
			doSystem("iwpriv %s set WatchdogPid=`cat %s`", WIF_5G, "/var/run/watchdog.pid");
		}
#endif
	}
#ifdef CONFIG_BCMWL5
	else
		nvram_set("wl_wps_mode", "disabled");
#endif
#else
	/* if we don't support WPS, make sure we unset any remaining wl_wps_mode */
	nvram_unset("wl_wps_mode");
#endif /* RTCONFIG_WPS */
	return 0;
}

int
stop_wps(void)
{
	int ret = 0;
#ifdef RTCONFIG_WPS
#ifdef CONFIG_BCMWL5
	killall_tk("wps_monitor");
	killall_tk("wps_ap");
#elif defined RTCONFIG_RALINK
	stop_wpsfix();
	stop_wsc();
#endif
#endif /* RTCONFIG_WPS */
   	return ret;
}

/* check for dual band case */
void
reset_wps(void)
{
#ifdef CONFIG_BCMWL5
//	int unit;
//	char prefix[]="wlXXXXXX_", tmp[100];

	stop_wps_method();
	
	stop_wps();

//	snprintf(prefix, sizeof(prefix), "wl%s_", nvram_safe_get("wps_band"));	
//	nvram_set(strcat_r(prefix, "wps_config_state", tmp), "0");

	nvram_set("w_Setting", "0");
//	start_wps();
	restart_wireless_wps();
#elif defined RTCONFIG_RALINK
	stop_wpsfix();
	wps_oob_both();
	start_wpsfix();
#endif
}

#ifdef CONFIG_BCMWL5
int
start_eapd(void)
{
	int ret = eval("/bin/eapd");

	return ret;
}

int
stop_eapd(void)
{
   	int ret = eval("killall","eapd");

   	return ret;
}
#endif

#if defined(BCM_DCS)
int
start_dcsd(void)
{
	int ret = eval("/usr/sbin/dcsd");

	return ret;
}

int
stop_dcsd(void)
{
   	int ret = eval("killall","dcsd");

   	return ret;
}
#endif /* BCM_DCS */


//2008.10 magic{
int start_networkmap(void)
{
	char *networkmap_argv[] = {"networkmap", NULL};
	pid_t pid;

	//if (!is_routing_enabled())
	//	return 0;

	_eval(networkmap_argv, NULL, 0, &pid);
	
	return 0;
}

//2008.10 magic}

void stop_networkmap()
{
	if (pids("networkmap"))
		killall_tk("networkmap");
}


// -----------------------------------------------------------------------------
#ifdef LINUX26

static pid_t pid_hotplug2 = -1;

void start_hotplug2()
{
	stop_hotplug2();

	f_write_string("/proc/sys/kernel/hotplug", "", FW_NEWLINE, 0);
	xstart("hotplug2", "--persistent", "--no-coldplug");
	// FIXME: Don't remember exactly why I put "sleep" here -
	// but it was not for a race with check_services()... - TB
	sleep(1);

	if (!nvram_contains_word("debug_norestart", "hotplug2")) {
		pid_hotplug2 = -2;
	}
}

void stop_hotplug2(void)
{
	pid_hotplug2 = -1;
	killall_tk("hotplug2");
}

#endif	/* LINUX26 */


void
stop_infosvr()
{
	if (pids("infosvr"))
		killall_tk("infosvr");
}

int 
start_infosvr()
{
	char *infosvr_argv[] = {"/usr/sbin/infosvr", "br0", NULL};
	pid_t pid;

	return _eval(infosvr_argv, NULL, 0, &pid);
}

#ifdef RTCONFIG_RALINK
int
exec_8021x_start(int band, int is_iNIC)
{
	char tmp[100], prefix[] = "wlXXXXXXX_";

	snprintf(prefix, sizeof(prefix), "wl%d_", band);

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa") ||
		nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa2") ||
		nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpawpa2") ||
		nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius"))
	{
		if (is_iNIC)
			return xstart("rtinicapd");
		else
			return xstart("rt2860apd");
	}
	else
		return 0;
}

int
start_8021x()
{
	char word[256], *next;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if (!strcmp(word, WIF_2G))
		{
			if (!strncmp(word, "rai", 3))   // iNIC
				exec_8021x_start(0, 1);
			else
				exec_8021x_start(0, 0);
		}
		else if (!strcmp(word, WIF_5G))
		{
			if (!strncmp(word, "rai", 3))   // iNIC
				exec_8021x_start(1, 1);
			else
				exec_8021x_start(1, 0);
		}
	}

	return 0;
}

int
exec_8021x_stop(int band, int is_iNIC)
{
	char tmp[100], prefix[] = "wlXXXXXXX_";

	snprintf(prefix, sizeof(prefix), "wl%d_", band);

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa") ||
		nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa2") ||
		nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpawpa2") ||
		nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius"))
	{
		if (is_iNIC)
			return killall("rtinicapd", SIGTERM);
		else
			return killall("rt2860apd", SIGTERM);
	}
	else
		return 0;
}

int
stop_8021x()
{
	char word[256], *next;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if (!strcmp(word, WIF_2G))
		{
			if (!strncmp(word, "rai", 3))   // iNIC
				exec_8021x_stop(0, 1);
			else
				exec_8021x_stop(0, 0);
		}
		else if (!strcmp(word, WIF_5G))
		{
			if (!strncmp(word, "rai", 3))   // iNIC
				exec_8021x_stop(1, 1);
			else
				exec_8021x_stop(1, 0);
		}
	}

	return 0;
}

int
start_wpsfix()
{
	char *wpsfix_argv[] = {"wpsfix", NULL};
	pid_t wfpid;

	return _eval(wpsfix_argv, NULL, 0, &wfpid);
}

int
stop_wpsfix()
{
	if (pids("wpsfix"))
		killall("wpsfix", SIGTERM);

	return 0;
}
#endif

void write_static_leases(char *file)
{
	FILE *fp;
	char *nv, *nvp, *b;
	char *mac, *ip;

	fp=fopen(file, "w");

	if (fp==NULL) return;

	nv = nvp = strdup(nvram_safe_get("dhcp_staticlist"));

	if(nv) {
	while ((b = strsep(&nvp, "<")) != NULL) {
		if((vstrsep(b, ">", &mac, &ip)!=2)) continue;
		if(strlen(mac)==0||strlen(ip)==0) continue;
		fprintf(fp, "%s %s\n", mac, ip);
	}
	free(nv);
	}			
	fclose(fp);
}

#ifndef RTCONFIG_DNSMASQ
int
start_dhcpd(void)
{
	FILE *fp;
	char *slease = "/tmp/udhcpd-br0.sleases";
#if 0
	int auto_dns = 0;
	int unit = wan_primary_ifunit();
	char prefix[] = "wanXXXXXXXXXX_";
	char word[256], *next, tmp[32];

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
#endif
	if (is_routing_enabled() && nvram_match("dhcp_enable_x", "1")) {
		_dprintf("starting udhcpd...\n");
	}
	else {
		_dprintf("skip running udhcpd...\n");
		return 0;
	}

	_dprintf("%s %s %s %s\n",
		nvram_safe_get("lan_ifname"),
		nvram_safe_get("dhcp_start"),
		nvram_safe_get("dhcp_end"),
		//nvram_safe_get("lan_lease"));
		nvram_safe_get("dhcp_lease"));

	if (!(fp = fopen("/tmp/udhcpd-br0.leases", "w"))) {
		perror("/tmp/udhcpd-br0.leases");
		return errno;
	}
	fclose(fp);

	// Write configuration file based on current information
	if (!(fp = fopen("/tmp/udhcpd.conf", "w"))) {
		perror("/tmp/udhcpd.conf");
		return errno;
	}

	//fprintf(fp, "pidfile /var/run/udhcpd-br0.pid\n");
	fprintf(fp, "start %s\n", nvram_safe_get("dhcp_start"));
	fprintf(fp, "end %s\n", nvram_safe_get("dhcp_end"));
	//fprintf(fp, "interface %s\n", nvram_safe_get("lan_ifname"));
	fprintf(fp, "interface br0\n");
	fprintf(fp, "remaining yes\n");
	fprintf(fp, "lease_file /tmp/udhcpd-br0.leases\n");
	fprintf(fp, "option subnet %s\n", nvram_safe_get("lan_netmask"));

	if (nvram_invmatch("dhcp_gateway_x",""))
		fprintf(fp, "option router %s\n", nvram_safe_get("dhcp_gateway_x"));	
	else	
		fprintf(fp, "option router %s\n", nvram_safe_get("lan_ipaddr"));	

	fprintf(fp, "option dns %s\n", nvram_safe_get("lan_ipaddr"));
	logmessage("dhcpd", "add option dns: %s", nvram_safe_get("lan_ipaddr"));

	if (nvram_invmatch("dhcp_dns1_x",""))
	{
		fprintf(fp, "option dns %s\n", nvram_safe_get("dhcp_dns1_x"));
		logmessage("dhcpd", "add option dns: %s", nvram_safe_get("dhcp_dns1_x"));
	}
#if 0
	if (!nvram_match(strcat_r(prefix, "proto", tmp), "static") && nvram_match(strcat_r(prefix, "dnsenable_x", tmp), "1"))
	{
		foreach(word, ((strlen(nvram_safe_get(strcat_r(prefix, "dns", tmp))) > 0) ? nvram_safe_get(tmp):
			nvram_safe_get(strcat_r(prefix, "xdns", tmp))), next)
		{
			auto_dns++;
			fprintf(fp, "option dns %s\n", word);
			logmessage("dhcpd", "add option dns: %s", word);
		}

		if (!auto_dns && (nvram_match("dhcp_dns1_x", "") || !nvram_match("dhcp_dns1_x", "8.8.8.8")))
		{
			fprintf(fp, "option dns 8.8.8.8\n");
			logmessage("dhcpd", "add option dns: %s", "8.8.8.8");
		}
	}
#endif
	fprintf(fp, "option lease %s\n", nvram_safe_get("dhcp_lease"));

	if (nvram_invmatch("dhcp_wins_x",""))		
		fprintf(fp, "option wins %s\n", nvram_safe_get("dhcp_wins_x"));		
	if (nvram_invmatch("lan_domain", ""))
		fprintf(fp, "option domain %s\n", nvram_safe_get("lan_domain"));
	fclose(fp);

	if (nvram_match("dhcp_static_x","1"))
	{	
		write_static_leases(slease);
		xstart("/usr/sbin/udhcpd", "/tmp/udhcpd.conf", "/tmp/udhcpd-br0.sleases");
	}
	else
	{
		xstart("/usr/sbin/udhcpd","/tmp/udhcpd.conf");
	}

	return 0;
}

void
stop_dhcpd(void)
{
	if (pids("udhcpd"))
	{
		kill_pidfile_s("/var/run/udhcpd.pid", SIGUSR1);
		kill_pidfile_s("/var/run/udhcpd.pid", SIGTERM);
	}
}

int
start_dns(void)
{
	FILE *fp;
	char word[256], *next;
	char *dns_list;
	int unit;
	char tmp1[32], prefix[] = "wanXXXXXXXXXX_";

	if (pids("dproxy"))
		return restart_dns();

	_dprintf("start_dns()\n");

	if (!is_routing_enabled())
		return 0;

	unit = wan_primary_ifunit();
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	/* Create resolv.conf with empty nameserver list */
	if (!(fp = fopen("/tmp/resolv.conf", "r")))
	{
		if (!(fp = fopen("/tmp/resolv.conf", "w"))) 
		{
			perror("/tmp/resolv.conf");
			return errno;
		}
		else fclose(fp);
	}
	else fclose(fp);

	if (!(fp = fopen("/tmp/dproxy.conf", "w")))
	{
		perror("/tmp/dproxy.conf");
		return errno;
	}

	fprintf(fp, "name_server=\n");
	fprintf(fp, "ppp_detect=0\n");
	fprintf(fp, "purge_time=1200\n");
	fprintf(fp, "resolv_file=/tmp/resolv.conf\n");
//	fprintf(fp, "deny_file=/tmp/dproxy.deny\n");
	fprintf(fp, "deny_file=\n");
	fprintf(fp, "cache_file=/tmp/dproxy.cache\n");
	fprintf(fp, "hosts_file=/tmp/hosts\n");
	fprintf(fp, "ppp_dev=/var/run/ppp0.pid\n");
	fprintf(fp, "debug_file=/tmp/dproxy.log\n");
	fclose(fp);
#if 0
	dns_list = ((strlen(nvram_safe_get(strcat_r(prefix, "dns", tmp1))) ? nvram_safe_get(strcat_r(prefix, "dns", tmp1)) : nvram_safe_get(strcat_r(prefix, "xdns", tmp2))));

	if (strlen(dns_list) > 0)	// chk, should not has this case
#else
	dns_list = nvram_safe_get(strcat_r(prefix, "dns", tmp1));
#endif
	{
		if (!(fp = fopen("/tmp/resolv.conf", "w+"))) {
			perror("/tmp/resolv.conf");
			return errno;
		}

//		foreach(word, (nvram_safe_get(tmp1) ? : nvram_safe_get(tmp2)), next)
		foreach(word, dns_list, next)
		{
			fprintf(fp, "nameserver %s\n", word);
		}
		fclose(fp);

		/* write also /etc/resolv.conf */
		//if (!(fp = fopen("/etc/resolv.conf", "w+"))) {
		//	perror("/etc/resolv.conf");
		//	return errno;
		//}

		//foreach(word, dns_list, next)
		//{
		//	fprintf(fp, "nameserver %s\n", word);
		//}
		//fclose(fp);
#ifndef OVERWRITE_DNS
		unlink("/etc/resolv.conf");
		symlink("/tmp/resolv.conf", "/etc/resolv.conf");
#endif
	}

	if (!(fp = fopen("/tmp/hosts", "w")))
	{
		perror("/tmp/hosts");
		return errno;
	}

	fprintf(fp, "127.0.0.1 localhost.localdomain localhost\n");
	fprintf(fp, "%s	my.router\n", nvram_safe_get("lan_ipaddr"));
	fprintf(fp, "%s	my.%s\n", nvram_safe_get("lan_ipaddr"), get_productid());
	fprintf(fp, "%s %s\n", nvram_safe_get("lan_ipaddr"), get_productid());

	if (strcmp(get_productid(), nvram_safe_get("computer_name")) && is_valid_hostname(nvram_safe_get("computer_name")))
		fprintf(fp, "%s %s\n", nvram_safe_get("lan_ipaddr"), nvram_safe_get("computer_name"));

	if (nvram_invmatch("lan_hostname", ""))
	{
		fprintf(fp, "%s %s.%s %s\n", nvram_safe_get("lan_ipaddr"),
					nvram_safe_get("lan_hostname"),
					nvram_safe_get("lan_domain"),
					nvram_safe_get("lan_hostname"));
	}	

#ifdef RTCONFIG_IPV6
	const char *s;
	if (ipv6_enabled()) {
		fprintf(fp, "::1 localhost6.localdomain6 localhost6\n");
		s = ipv6_router_address(NULL);
		if (*s) fprintf(fp, "%s %s\n", s, nvram_safe_get("lan_hostname"));
	}
#endif

	fclose(fp);	
		
	return system("dproxy -c /tmp/dproxy.conf");
}	

void
stop_dns(void)
{
	int delay_count = 10;

	if (pids("dproxy"))
		killall("dproxy", SIGKILL);

	while (pids("dproxy") && (delay_count-- > 0))
		sleep(1);

//	unlink("/tmp/dproxy.deny");
}

int 
restart_dns()
{
	FILE *fp = NULL;

	if (pids("dproxy") && (fp = fopen("/tmp/dproxy.conf", "r")))
	{
		fclose(fp);
		return killall("dproxy", SIGHUP);
	}

	stop_dns();
	return start_dns();
}
#endif

int
ddns_updated_main(int argc, char *argv[])
{
	FILE *fp;
	char buf[64], *ip;

	if (!(fp=fopen("/tmp/ddns.cache", "r"))) return 0;
	
	fgets(buf, sizeof(buf), fp);
	fclose(fp);

	if (!(ip=strchr(buf, ','))) return 0;
	
	nvram_set("ddns_cache", buf);
	nvram_set("ddns_ipaddr", ip+1);
	nvram_set("ddns_status", "1");
	nvram_set("ddns_server_x_old", nvram_safe_get("ddns_server_x"));
	nvram_set("ddns_hostname_old", nvram_safe_get("ddns_hostname_x"));
	nvram_commit();

	logmessage("ddns", "ddns update ok");

	_dprintf("done\n");

	return 0;
}
	
// TODO: handle wan0 only now
int 
start_ddns(void)
{
	FILE *fp;
	char *wan_ip, *wan_ifname;
	char *ddns_cache;
	char *server;
	char *user;
	char *passwd;
	char *host;
	char *service;
	char usrstr[64];
	int wild = nvram_get_int("ddns_wildcard_x");
	int unit, asus_ddns = 0;
	char tmp[32], prefix[] = "wanXXXXXXXXXX_";

	if (!is_routing_enabled()) {
		_dprintf("return -1\n");
		return -1;
	}
	if (nvram_invmatch("ddns_enable_x", "1"))
		return -1;

	unit = wan_primary_ifunit();
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	wan_ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
	wan_ifname = get_wan_ifname(unit);

	if (!wan_ip || strcmp(wan_ip, "") == 0 || !inet_addr(wan_ip)) {
		logmessage("ddns", "WAN IP is empty.");
		return -1;
	}
	if ((inet_addr(wan_ip) == inet_addr(nvram_safe_get("ddns_ipaddr"))) &&
	    (strcmp(nvram_safe_get("ddns_server_x"), nvram_safe_get("ddns_server_x_old")) == 0) &&
	    (strcmp(nvram_safe_get("ddns_hostname_x"), nvram_safe_get("ddns_hostname_old")) == 0)) {
		logmessage("ddns", "IP address & hostname have not changed since the last update.");
		return -1;
	}

	// TODO : Check /tmp/ddns.cache to see current IP in DDNS
	// update when,
	// 	1. if ipaddr!= ipaddr in cache
	// 	
	// update
	// * nvram ddns_cache, the same with /tmp/ddns.cache

	if (!nvram_match("ddns_server_x_old", "") &&
	    strcmp(nvram_safe_get("ddns_server_x"), nvram_safe_get("ddns_server_x_old"))) {
		logmessage("ddns", "clear ddns cache file for server setting change");
		unlink("/tmp/ddns.cache");
	}
	else if (!(fp = fopen("/tmp/ddns.cache", "r")) && (ddns_cache = nvram_get("ddns_cache"))) {
		if ((fp = fopen("/tmp/ddns.cache", "w+"))) {
			fprintf(fp, "%s", ddns_cache);
			fclose(fp);
		}
	}

	server = nvram_safe_get("ddns_server_x");
	user = nvram_safe_get("ddns_username_x");
	passwd = nvram_safe_get("ddns_passwd_x");
	host = nvram_safe_get("ddns_hostname_x");

	if (strcmp(server, "WWW.DYNDNS.ORG")==0)
		service = "dyndns";
	else if (strcmp(server, "WWW.DYNDNS.ORG(CUSTOM)")==0)
		service = "dyndns-custom";
	else if (strcmp(server, "WWW.DYNDNS.ORG(STATIC)")==0)
		service = "dyndns-static";
	else if (strcmp(server, "WWW.TZO.COM")==0)
		service = "tzo";
	else if (strcmp(server, "WWW.ZONEEDIT.COM")==0)
		service = "zoneedit";
	else if (strcmp(server, "WWW.JUSTLINUX.COM")==0)
		service = "justlinux";
	else if (strcmp(server, "WWW.EASYDNS.COM")==0)
		service = "easydns";
	else if (strcmp(server, "WWW.DNSOMATIC.COM")==0)
		service = "dnsomatic";
	else if (strcmp(server, "WWW.TUNNELBROKER.NET")==0)
		service = "heipv6tb";
	else if (strcmp(server, "WWW.NO-IP.COM")==0)
		service = "noip";
	else if (strcmp(server, "WWW.ASUS.COM")==0) {
		service = "dyndns", asus_ddns = 1;
	} else
		service = "dyndns";

	snprintf(usrstr, sizeof(usrstr), "%s:%s", user, passwd);

	_dprintf("start_ddns update %s %s\n", server, service);

	if (asus_ddns)
		nvram_set("ddns_return_code", "ddns_query");

	if (pids("ez-ipupdate")) {
		killall("ez-ipupdate", SIGINT);
		sleep(1);
	}

	nvram_unset("ddns_cache");
	nvram_unset("ddns_ipaddr");
	nvram_unset("ddns_status");
	nvram_set("ddns_updated", "1");

	if (asus_ddns) {
		char *nserver = nvram_invmatch("ddns_serverhost_x", "") ?
			nvram_safe_get("ddns_serverhost_x") :
			"ns1.asuscomm.com";

		eval("ez-ipupdate",
		     "-S", service, "-i", wan_ifname, "-h", host,
		     "-A", "2", "-s", nserver,
		     "-e", "/sbin/ddns_updated", "-b", "/tmp/ddns.cache");
	} else if (*service) {
		eval("ez-ipupdate",
		     "-S", service, "-i", wan_ifname, "-h", host,
		     "-u", usrstr, wild ? "-w" : "",
		     "-e", "/sbin/ddns_updated", "-b", "/tmp/ddns.cache");
	}

	return 0;
}

void
stop_ddns(void)
{
	if (pids("ez-ipupdate"))
		killall("ez-ipupdate", SIGINT);
}

int
asusddns_reg_domain(int reg)
{
	FILE *fp;
	char *wan_ip, *wan_ifname;
	char *ddns_cache;
	char *nserver;
	int unit;
	char tmp[32], prefix[] = "wanXXXXXXXXXX_";

	if (!is_routing_enabled()) {
		_dprintf("return -1\n");
		return -1;
	}

	if (reg) { //0:Aidisk, 1:Advanced Setting
		if (nvram_invmatch("ddns_enable_x", "1"))
			return -1;
	}

	unit = wan_primary_ifunit();
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	wan_ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
	wan_ifname = get_wan_ifname(unit);

	if (!wan_ip || strcmp(wan_ip, "") == 0 || !inet_addr(wan_ip)) {
		logmessage("asusddns", "WAN IP is empty.");
		return -1;
	}

	// TODO : Check /tmp/ddns.cache to see current IP in DDNS,
	// update when ipaddr!= ipaddr in cache.
	// nvram ddns_cache, the same with /tmp/ddns.cache

	if ((inet_addr(wan_ip) == inet_addr(nvram_safe_get("ddns_ipaddr"))) &&
	    (strcmp(nvram_safe_get("ddns_hostname_x"), nvram_safe_get("ddns_hostname_old")) == 0)) {
		nvram_set("ddns_return_code", "no_change");
		logmessage("asusddns", "IP address & hostname have not changed since the last update.");
		return -1;
	}

	if (!nvram_match("ddns_server_x_old", "") &&
	    strcmp(nvram_safe_get("ddns_server_x"), nvram_safe_get("ddns_server_x_old")) != 0) {
		logmessage("asusddns", "clear ddns cache file for server setting change");
		unlink("/tmp/ddns.cache");
	}
	else if (!(fp = fopen("/tmp/ddns.cache", "r")) && (ddns_cache = nvram_get("ddns_cache"))) {
		if ((fp = fopen("/tmp/ddns.cache", "w+"))) {
			fprintf(fp, "%s", ddns_cache);
			fclose(fp);
		}
	}

	nvram_set("ddns_return_code", "ddns_query");

	if (pids("ez-ipupdate")) 
	{
		killall("ez-ipupdate", SIGINT);
		sleep(1);
	}

	nserver = nvram_invmatch("ddns_serverhost_x", "") ?
		    nvram_safe_get("ddns_serverhost_x") :
		    "ns1.asuscomm.com";

	eval("ez-ipupdate",
	     "-S", "dyndns", "-i", wan_ifname, "-h", nvram_safe_get("ddns_hostname_x"),
	     "-A", "1", "-s", nserver,
	     "-e", "/sbin/ddns_updated", "-b", "/tmp/ddns.cache");

	return 0;
}

void
stop_syslogd()
{
	if (pids("syslogd"))
		killall("syslogd", SIGTERM);
}

void
stop_klogd()
{
	if (pids("klogd"))
		killall("klogd", SIGTERM);
}

int 
start_syslogd()
{
	int argc;
	char *syslogd_argv[] = {"syslogd",
		"-m", "0",				/* disable marks */
		"-S",					/* small log */
//		"-D",					/* suppress dups */
		"-O", "/tmp/syslog.log",
		NULL, NULL,				/* -s log_size */
		NULL, NULL,				/* -l log_level */
		NULL, NULL,				/* -R log_ipaddr[:port] */
		NULL,					/* -L log locally too */
		NULL};

	for (argc = 0; syslogd_argv[argc]; argc++);

	if (nvram_invmatch("log_size", "")) {
		syslogd_argv[argc++] = "-s";
		syslogd_argv[argc++] = nvram_safe_get("log_size");
	}
	if (nvram_invmatch("log_level", "")) {
		syslogd_argv[argc++] = "-l";
		syslogd_argv[argc++] = nvram_safe_get("log_level");
	}
	if (nvram_invmatch("log_ipaddr", "")) {
		char tmp[64];
		char *addr = nvram_safe_get("log_ipaddr");
		int port = nvram_get_int("log_port");

		if (port) {
			snprintf(tmp, sizeof(tmp), "%s:%d", addr, port);
			addr = tmp;
		}
		syslogd_argv[argc++] = "-R";
		syslogd_argv[argc++] = addr;
		syslogd_argv[argc++] = "-L";
	}

	// TODO: make sure is it necessary?
	//time_zone_x_mapping();
	//setenv("TZ", nvram_safe_get("time_zone_x"), 1);

	return _eval(syslogd_argv, NULL, 0, NULL);
}

int
start_klogd()
{
	pid_t pid;
	char *klogd_argv[] = {"/sbin/klogd", NULL};

	return _eval(klogd_argv, NULL, 0, &pid);
}

int 
start_logger(void)
{
_dprintf("%s:\n", __FUNCTION__);
	start_syslogd();
	start_klogd();

	return 0;
}

int 
start_misc(void)
{
	char *infosvr_argv[] = {"infosvr", "br0", NULL};
	char *watchdog_argv[] = {"watchdog", NULL};
#ifdef RTCONFIG_FANCTRL
	char *phy_tempsense_argv[] = {"phy_tempsense", NULL};
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
	char *psta_monitor_argv[] = {"psta_monitor", NULL};
#endif
#endif
	pid_t pid;

	_eval(infosvr_argv, NULL, 0, &pid);
	_eval(watchdog_argv, NULL, 0, &pid);
#ifdef RTCONFIG_FANCTRL
	_eval(phy_tempsense_argv, NULL, 0, &pid);
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
	_eval(psta_monitor_argv, NULL, 0, &pid);
#endif
#endif
	return 0;
}

void
stop_misc(void)
{
	fprintf(stderr, "stop_misc()\n");

	if (pids("infosvr"))
		killall_tk("infosvr");
	if (pids("watchdog"))
		killall_tk("watchdog");
#ifdef RTCONFIG_FANCTRL
	if (pids("phy_tempsense"))
		killall_tk("phy_tempsense");
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
	if (pids("psta_monitor"))
		killall_tk("psta_monitor");
#endif
#endif
	if (pids("ntp"))
		killall_tk("ntp");
	if (pids("ntpclient"))
		killall_tk("ntpclient");

	stop_wps();
#ifdef RTCONFIG_BCMWL6
	stop_acsd();
#endif
	stop_lltd();
	stop_rstats();
	stop_cstats();
}

void
stop_misc_no_watchdog(void)
{
	_dprintf("done\n");
}


int
chpass(char *user, char *pass)
{
//	FILE *fp;

	if (!user)
		user = "admin";

	if (!pass)
		pass = "admin";
/*
	if ((fp = fopen("/etc/passwd", "a")))
	{
		fprintf(fp, "%s::0:0::/:\n", user);
		fclose(fp);
	}

	if ((fp = fopen("/etc/group", "a")))
	{
		fprintf(fp, "%s:x:0:%s\n", user, user);
		fclose(fp);
	}
*/
	eval("chpasswd.sh", user, pass);
	return 0;
}

int 
start_telnetd()
{
//	char *telnetd_argv[] = {"telnetd", NULL};
	FILE *fp;

	if (getpid() != 1) {
		notify_rc("start_telnetd");
		return 0;
	}

	if (!nvram_match("telnetd_enable", "1"))
		return 0;

	if (pids("telnetd"))
		killall_tk("telnetd");

	if (get_productid())
	{
		if ((fp=fopen("/proc/sys/kernel/hostname", "w+")))
		{
			fputs(get_productid(), fp);
			fclose(fp);
		}
	}

	chpass(nvram_safe_get("http_username"), nvram_safe_get("http_passwd"));	// vsftpd also needs

	return xstart("telnetd");
}

void 
stop_telnetd()
{
	if (getpid() != 1) {
		notify_rc("stop_telnetd");
		return;
	}

	if (pids("telnetd"))
		killall_tk("telnetd");
}

int 
run_telnetd()
{
//	char *telnetd_argv[] = {"telnetd", NULL};

	if (pids("telnetd"))
		killall_tk("telnetd");

	chpass(nvram_safe_get("http_username"), nvram_safe_get("http_passwd"));	// vsftpd also needs

	return xstart("telnetd");
}

void
start_httpd(void)
{
	char *httpd_argv[] = {"httpd", NULL};
	pid_t pid;
#ifdef RTCONFIG_HTTPS
	char *https_argv[] = {"httpd", "-s", "-p", nvram_safe_get("https_lanport"), NULL};
	pid_t pid_https;
#endif
	int enable;
#ifdef DEBUG_RCTEST
	char *httpd_dir;
#endif

	if (getpid() != 1) {
		notify_rc("start_httpd");
		return;
	}

#ifdef DEBUG_RCTEST // Left for UI debug
	httpd_dir = nvram_safe_get("httpd_dir");
	if(strlen(httpd_dir)) chdir(httpd_dir);
	else 
#endif
	chdir("/www");

	enable = nvram_get_int("http_enable");
	if(enable!=1){
		_eval(httpd_argv, NULL, 0, &pid);
		logmessage(LOGNAME, "start httpd");
	}
	
#ifdef RTCONFIG_HTTPS
	if(enable!=0){
		_eval(https_argv, NULL, 0, &pid_https);
		logmessage(LOGNAME, "start httpd - SSL");
	}
#endif
	chdir("/");

	return;
}

void
stop_httpd(void)
{
	if (getpid() != 1) {
		notify_rc("stop_httpd");
		return;
	}

	if (pids("httpd"))
		killall_tk("httpd");	
}

//////////vvvvvvvvvvvvvvvvvvvvvjerry5 2009.07
void
stop_rstats(void)
{
	if (pids("rstats"))
		killall_tk("rstats");
}

void
start_rstats(int new)
{
	if (!is_routing_enabled()) return;
	
	stop_rstats();
	if (new) xstart("rstats", "--new");
	else xstart("rstats");
}

void
restart_rstats()
{
	if (nvram_match("rstats_new", "1"))
	{
		start_rstats(1);
		nvram_set("rstats_new", "0");
	}
	else
	{
		start_rstats(0);
	}
}
////////^^^^^^^^^^^^^^^^^^^jerry5 2009.07

// TODO: so far, support wan0 only

void start_upnp(void)
{
	int upnp_enable, upnp_mnp_enable;
	FILE *f;
	int upnp_port;
	int unit;
	char tmp1[32], prefix[] = "wanXXXXXXXXXX_";

	if (getpid() != 1) {
		notify_rc("start_upnp");
		return;
	}

	if (!is_routing_enabled()) return;

	unit = wan_primary_ifunit();
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	upnp_enable = nvram_get_int("upnp_enable");
	upnp_mnp_enable = nvram_get_int("upnp_mnp");

	if (nvram_match(strcat_r(prefix, "upnp_enable", tmp1), "1")) {
		mkdir("/etc/upnp", 0777);
		if (f_exists("/etc/upnp/config.alt")) {
			xstart("miniupnpd", "-f", "/etc/upnp/config.alt");
		}
		else {
			if ((f = fopen("/etc/upnp/config", "w")) != NULL) {
				upnp_port = nvram_get_int("upnp_port");
				if ((upnp_port < 0) || (upnp_port >= 0xFFFF)) upnp_port = 0;
				char *lanip = nvram_safe_get("lan_ipaddr");
				char *lanmask = nvram_safe_get("lan_netmask");
				
				fprintf(f,
					"ext_ifname=%s\n"
					"listening_ip=%s/%s\n"
					"port=%d\n"
					"enable_upnp=%s\n"
					"enable_natpmp=%s\n"
					"secure_mode=%s\n"
					"upnp_forward_chain=FUPNP\n"
					"upnp_nat_chain=VUPNP\n"
					"notify_interval=%d\n"
					"system_uptime=yes\n"
					"friendly_name=%s\n"
					"model_number=%s.%s\n"
					"serial=%s\n"
					"\n"
					,
					get_wan_ifname(wan_primary_ifunit()),
					lanip, lanmask,
					upnp_port,
					upnp_enable ? "yes" : "no",	// upnp enable
					upnp_mnp_enable ? "yes" : "no",	// natpmp enable
					nvram_get_int("upnp_secure") ? "yes" : "no",	// secure_mode (only forward to self)
					nvram_get_int("upnp_ssdp_interval"),
					get_productid(),
					rt_version, rt_serialno,
					nvram_get("serial_no") ? : nvram_safe_get("et0macaddr")
				);

				if (nvram_get_int("upnp_clean")) {
					int interval = nvram_get_int("upnp_clean_interval");
					if (interval < 60) interval = 60;
					fprintf(f,
						"clean_ruleset_interval=%d\n"
						"clean_ruleset_threshold=%d\n",
						interval,
						nvram_get_int("upnp_clean_threshold")
					);
				}
				else
					fprintf(f,"clean_ruleset_interval=0\n");


				if (nvram_match("upnp_mnp", "1")) {
					int https = nvram_get_int("https_enable");
					fprintf(f, "presentation_url=http%s://%s\n",
						https ? "s" : "", lanip);
				}
				else {
					// Empty parameters are not included into XML service description
					fprintf(f, "presentation_url=\n");
				}


				char uuid[45];
				f_read_string("/proc/sys/kernel/random/uuid", uuid, sizeof(uuid));
				fprintf(f, "uuid=%s\n", uuid);

				int ports[4];
				if ((ports[0] = nvram_get_int("upnp_min_port_int")) > 0 &&
				    (ports[1] = nvram_get_int("upnp_max_port_int")) > 0 &&
				    (ports[2] = nvram_get_int("upnp_min_port_ext")) > 0 &&
				    (ports[3] = nvram_get_int("upnp_max_port_ext")) > 0) {
					fprintf(f,
						"allow %d-%d %s/%s %d-%d\n",
						ports[0], ports[1],
						lanip, lanmask,
						ports[2], ports[3]
					);
				}
				else {
					// by default allow only redirection of ports above 1024
					fprintf(f, "allow 1024-65535 %s/%s 1024-65535\n", lanip, lanmask);
				}

TRACE_PT("config 5\n");

				fappend(f, "/etc/upnp/config.custom");
				append_custom_config("upnp", f);
				fprintf(f, "\ndeny 0-65535 0.0.0.0/0 0-65535\n");
				fclose(f);
				use_custom_config("upnp", "/etc/upnp/config");
				xstart("miniupnpd", "-f", "/etc/upnp/config");
			}
		}
	}
}

void stop_upnp(void)
{
	if (getpid() != 1) {
		notify_rc("stop_upnp");
		return;
	}

	killall_tk("miniupnpd");
}

int
start_ntpc(void)
{
	char *ntp_argv[] = {"ntp", NULL};
	int pid;

	if (getpid() != 1) {
		notify_rc("start_ntpc");
		return 0;
	}

	if (pids("ntpclient"))
		killall_tk("ntpclient");

	if (!pids("ntp"))
		_eval(ntp_argv, NULL, 0, &pid);

	return 0;
}

void
stop_ntpc(void)
{
	if (getpid() != 1) {
		notify_rc("stop_ntpc");
		return;
	}

	if (pids("ntpclient"))
		killall_tk("ntpclient");
}


void refresh_ntpc(void)
{
	setup_timezone();

	if (pids("ntpclient"))
		killall_tk("ntpclient");;

	if (!pids("ntp"))
	{
		stop_ntpc();
		start_ntpc();
	}
	else
		kill_pidfile_s("/var/run/ntp.pid", SIGALRM);
}

int start_lltd(void)
{
	chdir("/usr/sbin");

#ifdef CONFIG_BCMWL5
	char *odmpid = nvram_safe_get("odmpid");
	int model = get_model();

	if (strlen(odmpid) && is_valid_hostname(odmpid))
	{
		switch (model) {
		case MODEL_RTN66U:
			eval("lld2d.rtn66r", "br0");
			break;
		case MODEL_RTAC66U:
			eval("lld2d.rtac66r", "br0");
			break;
		default:
			eval("lld2d", "br0");
			break;
		}
	}
	else
#endif
		eval("lld2d", "br0");

	chdir("/");

	return 0;
}

int stop_lltd(void)
{
	int ret;

#ifdef CONFIG_BCMWL5
        char *odmpid = nvram_safe_get("odmpid");
        int model = get_model();
	if (strlen(odmpid) && is_valid_hostname(odmpid))
	{
		switch (model) {
		case MODEL_RTN66U:
			ret = eval("killall", "lld2d.rtn66r");
			break;
		case MODEL_RTAC66U:
			ret = eval("killall", "lld2d.rtac66r");
			break;
		default:
			ret = eval("killall", "lld2d");
			break;
		}
	}
	else
#endif
	ret = eval("killall", "lld2d");

	return ret;
}

int
start_services(void)
{
	_dprintf("%s  %d\n",__FUNCTION__, __LINE__);	// tmp test

	//init_spinlock();
	start_telnetd();
#ifdef RTCONFIG_SSH
	if (nvram_match("sshd_enable", "1"))
	{
		start_sshd();
	}
#endif

#ifdef CONFIG_BCMWL5
	start_eapd();
	start_nas();
#elif defined RTCONFIG_RALINK
	start_8021x();
#endif
	start_wps();
	start_wpsaide();
#ifdef RTCONFIG_BCMWL6
	start_acsd();
#endif

#ifdef RTCONFIG_DNSMASQ
	start_dnsmasq();
#else
	start_dhcpd();
	start_dns();
#endif
	start_cifs();
	start_httpd();
#ifdef RTCONFIG_CROND
	start_cron();
#endif
	start_infosvr();
	start_networkmap();
	restart_rstats();
	restart_cstats();
	start_watchdog();
#ifdef RTCONFIG_FANCTRL
	start_phy_tempsense();
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
	start_psta_monitor();
#endif
#endif
	start_lltd();
	start_upnp();

#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD)
	start_pptpd();
#endif
//#ifdef RTCONFIG_IPV6
//	/* note: starting radvd here might be too early in case of
//	 * DHCPv6 or 6to4 because we won't have received a prefix and
//	 * so it will disable advertisements. To restart them, we have
//	 * to send radvd a SIGHUP, or restart it.
//	 */
//	start_radvd();
//#endif

#ifdef RTCONFIG_USB
//	_dprintf("restart_nas_services(%d): test 8.\n", getpid());
	restart_nas_services(0, 1);
#ifdef RTCONFIG_DISK_MONITOR
	start_diskmon();
#endif
#endif
	//start_dnsmasq();

#ifdef RTCONFIG_WEBDAV
	start_webdav();
#endif

	run_custom_script("services-start", NULL);

	return 0;
}

void
stop_logger(void)
{
	if (pids("klogd"))
		killall("klogd", SIGTERM);
	if (pids("syslogd"))
		killall("syslogd", SIGTERM);
}

void
stop_services(void)
{

	run_custom_script("services-stop", NULL);

#ifdef RTCONFIG_WEBDAV
	stop_webdav();
#endif

#ifdef RTCONFIG_USB
//_dprintf("restart_nas_services(%d): test 9.\n", getpid());
	restart_nas_services(1, 0);
#ifdef RTCONFIG_DISK_MONITOR
	stop_diskmon();
#endif
#endif
	stop_upnp();
	stop_lltd();
	stop_watchdog();
#ifdef RTCONFIG_FANCTRL
	stop_phy_tempsense();
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
	stop_psta_monitor();
#endif
#endif
	stop_cstats();
	stop_rstats();
	stop_networkmap();
	stop_infosvr();
#ifdef RTCONFIG_CROND
	stop_cron();
#endif
	stop_httpd();
	stop_cifs();
#ifdef RTCONFIG_DNSMASQ
	stop_dnsmasq();
#else
	stop_dns();
	stop_dhcpd();
#endif
#ifdef RTCONFIG_IPV6
	stop_radvd();
#endif
	stop_wps();
#ifdef CONFIG_BCMWL5
	stop_nas();
	stop_eapd();
#elif defined RTCONFIG_RALINK
	stop_8021x();
#endif
#ifdef RTCONFIG_BCMWL6
	stop_acsd();
#endif
#ifdef RTCONFIG_SSH
	stop_sshd();
#endif
	stop_telnetd();
}

// 2008.10 magic 
int start_wanduck(void)
{	
	char *argv[] = {"/sbin/wanduck", NULL};
	pid_t pid;
#if 0
	int sw_mode = nvram_get_int("sw_mode");

	if(sw_mode != SW_MODE_ROUTER && sw_mode != SW_MODE_REPEATER)
		return -1;
#endif

	if(!strcmp(nvram_safe_get("wanduck_down"), "1"))
		return 0;

	return _eval(argv, NULL, 0, &pid);
}

void stop_wanduck(void)
{	
	killall("wanduck", SIGTERM);
}


int 
stop_watchdog()
{
	if (pids("watchdog"))
		killall_tk("watchdog");
	return 0;
}

int 
start_watchdog()
{
	char *watchdog_argv[] = {"watchdog", NULL};
	pid_t whpid;

	return _eval(watchdog_argv, NULL, 0, &whpid);
}

#ifdef RTCONFIG_FANCTRL
int
stop_phy_tempsense()
{
	if (pids("phy_tempsense")) {
		killall_tk("phy_tempsense");
	}
	return 0;
}

int
start_phy_tempsense()
{
	char *phy_tempsense_argv[] = {"phy_tempsense", NULL};
	pid_t pid;

	return _eval(phy_tempsense_argv, NULL, 0, &pid);
}
#endif

#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
int
stop_psta_monitor()
{
	if (pids("psta_monitor")) {
		killall_tk("psta_monitor");
	}
	return 0;
}

int
start_psta_monitor()
{
	char *psta_monitor_argv[] = {"psta_monitor", NULL};
	pid_t pid;

	return _eval(psta_monitor_argv, NULL, 0, &pid);
}
#endif
#endif

#ifdef RTCONFIG_USB
int
start_usbled()
{
	char *usbled_argv[] = {"usbled", NULL};
	pid_t whpid;

	stop_usbled();
	return _eval(usbled_argv, NULL, 0, &whpid);
}

int
stop_usbled()
{
	if (pids("usbled"))
		killall("usbled", SIGTERM);

	return 0;
}
#endif

#ifdef RTCONFIG_CROND
void start_cron(void)
{
	stop_cron();
	eval("crond");
}


void stop_cron(void)
{
	killall_tk("crond");
}
#endif

void start_script(int argc, char *argv[])
{
	int pid;

	argv[argc] = NULL;
	_eval(argv, NULL, 0, &pid);

}

// -----------------------------------------------------------------------------

/* -1 = Don't check for this program, it is not expected to be running.
 * Other = This program has been started and should be kept running.  If no
 * process with the name is running, call func to restart it.
 * Note: At startup, dnsmasq forks a short-lived child which forks a
 * long-lived (grand)child.  The parents terminate.
 * Many daemons use this technique.
 */
static void _check(pid_t pid, const char *name, void (*func)(void))
{
	if (pid == -1) return;

	if (pidof(name) > 0) return;

	syslog(LOG_DEBUG, "%s terminated unexpectedly, restarting.\n", name);
	func();

	// Force recheck in 500 msec
	setitimer(ITIMER_REAL, &pop_tv, NULL);
}

void check_services(void)
{
//	TRACE_PT("keep alive\n");

	// Periodically reap any zombies
	setitimer(ITIMER_REAL, &zombie_tv, NULL);

#ifdef LINUX26
	_check(pids("hotplug2"), "hotplug2", start_hotplug2);
#endif
#ifdef RTCONFIG_CROND
	_check(pids("crond"), "crond", start_cron);
#endif
}

#define RC_SERVICE_STOP 0x01
#define RC_SERVICE_START 0x02

void handle_notifications(void) {
	char nv[256], nvtmp[32], *cmd[8], *script;
	char *nvp, *b, *nvptr;
	int action = 0;
	int count;
	int i;

	// handle command one by one only
	// handle at most 7 parameters only
	// maximum rc_service strlen is 256
	strcpy(nv, nvram_safe_get("rc_service"));
	nvptr = nv;
again:
	nvp = strsep(&nvptr, ";");

	count = 0;
	while ((b = strsep(&nvp, " ")) != NULL) 
	{
		_dprintf("cmd[%d]=%s\n", count, b);
		cmd[count] = b;
		count ++;
		if(count==8) break; 
	}

	while(count<8) cmd[count++] = 0;
	
	if(cmd[0]==0 || strlen(cmd[0])==0) {
		nvram_set("rc_service", "");
		return;
	}

	if(strncmp(cmd[0], "start_", 6)==0) {
		action |= RC_SERVICE_START;
		script = &cmd[0][6]; 
	}
	else if(strncmp(cmd[0], "stop_", 5)==0) {
		action |= RC_SERVICE_STOP;
		script = &cmd[0][5];
	}
	else if(strncmp(cmd[0], "restart_", 8)==0) {
		action |=(RC_SERVICE_START|RC_SERVICE_STOP);
		script = &cmd[0][8];
	}
	else {
		action = 0;
		script = cmd[0];
	}

	TRACE_PT("running: %d %s\n", action, script);

	if (strcmp(script, "reboot") == 0 || strcmp(script,"rebootandrestore")==0) {
		stop_wan();
#ifdef RTCONFIG_USB
		stop_usb();
		stop_usbled();
#endif

		if(strcmp(script,"rebootandrestore")==0) {
			for(i=1;i<count;i++) {
				if(cmd[i]) restore_defaults_module(cmd[i]);
			}
		}
		sleep(3);
		/* FIXME: Signal SIGHUP will only restarts WAN services
		 * without actual reboot? */
		kill(1, SIGTERM);
	}
	else if (strcmp(script, "resetdefault") == 0) {
		stop_wan();
#ifdef RTCONFIG_USB
		stop_usb();
		stop_usbled();
#endif
		sleep(3);
		eval("mtd-erase", "-d", "nvram");
		kill(1, SIGTERM);
	}
	else if (strcmp(script, "all") == 0) {
		int i;
		sleep(2); // wait for all httpd event done
		stop_lan_port();
		for(i=0;i<6;i++) {
			sleep(1);
			_dprintf("sleep\n");
		}
		start_lan_port();
		kill(1, SIGTERM);
	}
	else if(strcmp(script, "upgrade") == 0) {
		if(action&RC_SERVICE_STOP) {
			// what process need to stop to free memory or 
			// to avoid affecting upgrade
			stop_misc();
			stop_logger();
			stop_wanduck();
			stop_upnp();
#ifdef RTCONFIG_USB
			stop_usb();
			stop_usbled();
#endif
			killall("udhcpc", SIGTERM);
#ifdef RTCONFIG_IPV6
			stop_dhcp6c();
#endif
			// TODO free necessary memory here
		}
		if(action&RC_SERVICE_START) {
			char upgrade_file[64];
			char *webs_state_info;

			webs_state_info = nvram_safe_get("webs_state_info");

			if(strlen(webs_state_info)>5) {
				sprintf(upgrade_file, "/tmp/%s_%c.%c.%c.%c_%s.trx",
					nvram_safe_get("productid"),
					webs_state_info[0],
					webs_state_info[1],
					webs_state_info[2],
					webs_state_info[3],
					webs_state_info+5);
				
				_dprintf("upgrade file : %s \n", upgrade_file);
			}
			else upgrade_file[0]=0x0;

			if(f_exists("/tmp/linux.trx")) { // starting to upgrade
				stop_wan();
				eval("mtd-write", "-i", "/tmp/linux.trx", "-d", "linux");
#ifdef RTCONFIG_DUAL_TRX
				_dprintf(" Write FW to the 2nd partition.\n");
				eval("mtd-write", "-i", "/tmp/linux.trx", "-d", "linux2");
#endif
				kill(1, SIGTERM);
			}
			else if (strlen(upgrade_file) && f_exists(upgrade_file)) {
				stop_wan();
				eval("mtd-write", "-i", upgrade_file, "-d", "linux");
#ifdef RTCONFIG_DUAL_TRX
				_dprintf(" Write FW to the 2nd partition.\n");
                                eval("mtd-write", "-i", upgrade_file, "-d", "linux2");
#endif   
				kill(1, SIGTERM);
			}
			else {
				kill(1, SIGTERM);
				// recover? or reboot directly
			}
		}
	}
	else if(strcmp(script, "mfgmode") == 0) {
		//stop_infosvr(); //ATE need ifosvr
		killall_tk("ntp");
		stop_ntpc();
		stop_wps();
#ifdef RTCONFIG_BCMWL6
		stop_acsd();
#endif
		stop_lltd();	// 1017 add
		stop_wanduck();
		stop_logger();
		stop_wanduck();
#ifdef RTCONFIG_DNSMASQ
		stop_dnsmasq();
#else
		stop_dns();
		stop_dhcpd();
#endif
		stop_ots();
		stop_networkmap();
#ifdef RTCONFIG_USB
		stop_usbled();
#endif
	}
	else if (strcmp(script, "allnet") == 0) {
		if(action&RC_SERVICE_STOP) {
			// including switch setting
			// used for system mode change and vlan setting change
			sleep(2); // wait for all httpd event done
			stop_httpd();
#ifdef RTCONFIG_DNSMASQ
			stop_dnsmasq();
#else
			stop_dns();
			stop_dhcpd();
#endif
			stop_wps();
#ifdef CONFIG_BCMWL5
			stop_nas();
			stop_eapd();
#elif defined RTCONFIG_RALINK
			stop_8021x();
#endif
#ifdef RTCONFIG_BCMWL6
			stop_acsd();
#endif
			stop_wan();
			stop_lan();
			stop_vlan();

			// TODO free memory here
		}
		if(action&RC_SERVICE_START) {
			config_switch();

			start_vlan();
			start_lan();
			start_wan();
#ifdef CONFIG_BCMWL5
			start_eapd();
			start_nas();
#elif defined RTCONFIG_RALINK
			start_8021x();
#endif
			start_wps();
#ifdef RTCONFIG_BCMWL6
			start_acsd();
#endif
#ifdef RTCONFIG_DNSMASQ
			start_dnsmasq();
#else
			start_dhcpd();
			start_dns();
#endif
			start_httpd();
			start_wl();
			lanaccess_wl();
		}
	}
	else if (strcmp(script, "net") == 0) {
		if(action&RC_SERVICE_STOP) {
			sleep(2); // wait for all httpd event done
#ifdef RTCONFIG_USB_PRINTER
			stop_u2ec();
#endif
			stop_networkmap();
			stop_httpd();
#ifdef RTCONFIG_DNSMASQ
			stop_dnsmasq();
#else
			stop_dns();
			stop_dhcpd();
#endif
			stop_wps();
#ifdef CONFIG_BCMWL5
			stop_nas();
			stop_eapd();
#elif defined RTCONFIG_RALINK
			stop_8021x();
#endif
#ifdef RTCONFIG_BCMWL6
			stop_acsd();
#endif
			stop_wan();
			stop_lan();
			//stop_vlan();

			// free memory here
		}
		if(action&RC_SERVICE_START) {
			//start_vlan();
			start_lan();
			start_wan();
#ifdef CONFIG_BCMWL5
			start_eapd();
			start_nas();
#elif defined RTCONFIG_RALINK
			start_8021x();
#endif
			start_wps();
#ifdef RTCONFIG_BCMWL6
			start_acsd();
#endif
#ifdef RTCONFIG_DNSMASQ
			start_dnsmasq();
#else
			start_dhcpd();
			start_dns();
#endif
			start_httpd();
			start_networkmap();
#ifdef RTCONFIG_USB_PRINTER
			start_u2ec();
#endif
			start_wl();
			lanaccess_wl();
		}
	}
	else if (strcmp(script, "net_and_phy") == 0) {
		int i;

		if(action&RC_SERVICE_STOP) {
			sleep(2); // wait for all httpd event done

#ifdef RTCONFIG_MEDIA_SERVER
			force_stop_dms();
			stop_mt_daapd();
#endif

#if defined(RTCONFIG_SAMBASRV) && defined(RTCONFIG_FTP)
			stop_ftpd();
			stop_samba();
#endif

#ifdef RTCONFIG_USB_PRINTER
			stop_u2ec();
#endif
			stop_networkmap();
			stop_httpd();
#ifdef RTCONFIG_DNSMASQ
			stop_dnsmasq();
#else
			stop_dns();
			stop_dhcpd();
#endif
			stop_wps();
#ifdef CONFIG_BCMWL5
			stop_nas();
			stop_eapd();
#elif defined RTCONFIG_RALINK
			stop_8021x();
#endif
#ifdef RTCONFIG_BCMWL6
			stop_acsd();
#endif
			stop_wan();
			stop_lan();
			//stop_vlan();
			stop_lan_port();

			// free memory here
		}
		for(i=0;i<6;i++) {
			sleep(1);
			_dprintf("sleep\n");
		}
		if(action&RC_SERVICE_START) {
			//start_vlan();
			start_lan();
			start_wan();
#ifdef CONFIG_BCMWL5
			start_eapd();
			start_nas();
#elif defined RTCONFIG_RALINK
			start_8021x();
#endif
			start_wps();
#ifdef RTCONFIG_BCMWL6
			start_acsd();
#endif
#ifdef RTCONFIG_DNSMASQ
			start_dnsmasq();
#else
			start_dhcpd();
			start_dns();
#endif
			/* Link-up LAN ports after DHCP server ready. */
			start_lan_port();

			start_httpd();
			start_networkmap();
#ifdef RTCONFIG_USB_PRINTER
			start_u2ec();
#endif

#if defined(RTCONFIG_SAMBASRV) && defined(RTCONFIG_FTP)
			create_passwd();
			start_samba();
			start_ftpd();
#endif

#ifdef RTCONFIG_MEDIA_SERVER
			start_dms();
			start_mt_daapd();
#endif
			start_wl();
			lanaccess_wl();
		}
	}
	else if (strcmp(script, "wireless") == 0) {
		if(action&RC_SERVICE_STOP) {
#ifdef RTCONFIG_WIRELESSREPEATER
			stop_wlcconnect();

			kill_pidfile_s("/var/run/wanduck.pid", SIGUSR1);
#endif

#ifdef RTCONFIG_USB_PRINTER
			stop_u2ec();
#endif
			stop_networkmap();
		}
		if((action&RC_SERVICE_STOP)&&(action&RC_SERVICE_START)) {
			// TODO: free memory here
#ifdef RTCONFIG_RALINK
			reinit_hwnat();
#endif
			restart_wireless();
		}
		if(action&RC_SERVICE_START) {
#ifdef RTCONFIG_WIRELESSREPEATER
			start_wlcconnect();
#endif

			start_networkmap();
#ifdef RTCONFIG_USB_PRINTER
			start_u2ec();
#endif
		}
	}
#ifdef CONFIG_BCMWL5
	else if (strcmp(script, "set_wltxpower") == 0) {
	if ((get_model() == MODEL_RTAC66U) ||
		(get_model() == MODEL_RTN12HP) ||
		(get_model() == MODEL_RTN66U))
			set_wltxpower();
		else
			dbG("\n\tDon't do this!\n\n");
	}
#endif
#ifdef RTCONFIG_FANCTRL
	else if (strcmp(script, "fanctrl") == 0) {
		if((action&RC_SERVICE_STOP)&&(action&RC_SERVICE_START)) restart_fanctrl();
	}
#endif
	else if (strcmp(script, "wan") == 0) {
		if(action&RC_SERVICE_STOP) stop_wan();
		if(action&RC_SERVICE_START) start_wan();
	}
	else if (strcmp(script, "wan_if") == 0) {
		_dprintf("%s: wan_if: %s.\n", __FUNCTION__, cmd[1]);	
		if(cmd[1]) {
			if(action&RC_SERVICE_STOP) stop_wan_if(atoi(cmd[1]));
			if(action&RC_SERVICE_START) start_wan_if(atoi(cmd[1]));
		}
	}
#ifdef RTCONFIG_DSL	
	else if (strcmp(script, "dslwan_if") == 0) {
		_dprintf("%s: restart_dslwan_if: %s.\n", __FUNCTION__, cmd[1]);	
		if(cmd[1]) {
			if(action&RC_SERVICE_STOP) {
				stop_wan_if(atoi(cmd[1]));
			}
			if(action&RC_SERVICE_START) {
				remove_dsl_autodet();
				convert_dsl_wan_settings(2);
				start_wan_if(atoi(cmd[1]));
			}
		}
	}
	else if (strcmp(script, "dsl_wireless") == 0) {
		if(action&RC_SERVICE_STOP) {
#ifdef RTCONFIG_USB_PRINTER
			stop_u2ec();
#endif
			stop_networkmap();
		}
		if((action&RC_SERVICE_STOP)&&(action&RC_SERVICE_START)) {
// qis		
			remove_dsl_autodet();
			stop_wan_if(atoi(cmd[1]));
			convert_dsl_wan_settings(2);
			start_wan_if(atoi(cmd[1]));
			restart_wireless();
		}
		if(action&RC_SERVICE_START) {
			start_networkmap();
#ifdef RTCONFIG_USB_PRINTER
			start_u2ec();
#endif
		}
	}
#endif	
	else if (strcmp(script, "wan_line") == 0) {
	_dprintf("%s: restart_wan_line: %s.\n", __FUNCTION__, cmd[1]);	
		if(cmd[1]) {
			int wan_unit = atoi(cmd[1]);
			char *current_ifname = get_wan_ifname(wan_unit);

			wan_up(current_ifname);
		}
	}
#ifdef CONFIG_BCMWL5
	else if (strcmp(script, "nas") == 0) {
		if(action&RC_SERVICE_STOP) stop_nas();
		if(action&RC_SERVICE_START) {
			start_eapd();
			start_nas();
			start_wps();
#ifdef RTCONFIG_BCMWL6
			start_acsd();
#endif
			start_wl();
			lanaccess_wl();
		}
	}
#endif
#ifdef RTCONFIG_USB
	else if (strcmp(script, "nasapps") == 0)
	{
		if(action&RC_SERVICE_STOP){
//_dprintf("restart_nas_services(%d): test 10.\n", getpid());
			restart_nas_services(1, 0);
		}
		if(action&RC_SERVICE_START){
//_dprintf("restart_nas_services(%d): test 11.\n", getpid());
			restart_nas_services(0, 1);
		}
	}
#ifdef RTCONFIG_OPENVPN
        else if (strncmp(script, "vpnclient", 9) == 0) {
                if (action & RC_SERVICE_STOP) stop_vpnclient(atoi(&script[9]));
                if (action & RC_SERVICE_START) start_vpnclient(atoi(&script[9]));
        }

        else if (strncmp(script, "vpnserver" ,9) == 0) {
                if (action & RC_SERVICE_STOP) stop_vpnserver(atoi(&script[9]));
                if (action & RC_SERVICE_START) start_vpnserver(atoi(&script[9]));
        }
#endif

#if defined(RTCONFIG_SAMBASRV) && defined(RTCONFIG_FTP)
	else if (strcmp(script, "ftpsamba") == 0)
	{
		if(action&RC_SERVICE_STOP) {
			stop_ftpd();
			stop_samba();
		}
		if(action&RC_SERVICE_START) {
			create_passwd();
			start_samba();
			start_ftpd();
		}
	}
#endif
#ifdef RTCONFIG_FTP
	else if (strcmp(script, "ftpd") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_ftpd();
		if(action&RC_SERVICE_START) {
			start_ftpd();
			start_firewall(wan_primary_ifunit(), 0);
		}
	}
#endif
#ifdef RTCONFIG_SAMBASRV
	else if (strcmp(script, "samba") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_samba();
		if(action&RC_SERVICE_START) {
			start_samba();
		}
	}
#endif
#ifdef RTCONFIG_WEBDAV
	else if (strcmp(script, "webdav") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_webdav();
		if(action&RC_SERVICE_START) {
			start_firewall(wan_primary_ifunit(), 0);
			start_webdav();
		}
	}
	else if (strcmp(script, "enable_webdav") == 0)
	{	
		stop_ddns();
		stop_webdav();
		start_firewall(wan_primary_ifunit(), 0);
		start_webdav();
		start_ddns();
		
	}
#endif
#ifdef RTCONFIG_CLOUDSYNC
	else if (strcmp(script, "cloudsync") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_cloudsync();
		if(action&RC_SERVICE_START) start_cloudsync();
	}
#endif
#ifdef RTCONFIG_USB_PRINTER
	else if (strcmp(script, "lpd") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_lpd();
		if(action&RC_SERVICE_START) start_lpd();
	}
	else if (strcmp(script, "u2ec") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_u2ec();
		if(action&RC_SERVICE_START) start_u2ec();
	}
#endif
#ifdef RTCONFIG_MEDIA_SERVER
	else if (strcmp(script, "media") == 0)
	{
		if(action&RC_SERVICE_STOP) {
			force_stop_dms();
			stop_mt_daapd();
		}
		if(action&RC_SERVICE_START) {
			start_dms();
			start_mt_daapd();
		}
	}
	else if (strcmp(script, "dms") == 0)
	{
		if(action&RC_SERVICE_STOP) force_stop_dms();
		if(action&RC_SERVICE_START) start_dms();
	}
	else if (strcmp(script, "mt_daapd") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_mt_daapd();
		if(action&RC_SERVICE_START) start_mt_daapd();
	}
#endif
#ifdef RTCONFIG_DISK_MONITOR
	else if (strcmp(script, "diskmon")==0)
	{
		if(action&RC_SERVICE_STOP) stop_diskmon();
		if(action&RC_SERVICE_START) start_diskmon();
	}
	else if (strcmp(script, "diskscan")==0)
	{
		if(action&RC_SERVICE_START)
			kill_pidfile_s("/var/run/disk_monitor.pid", SIGUSR2);
	}
#endif
	else if(!strncmp(script, "apps_", 5)) 
	{
		if(action&RC_SERVICE_START) {
			if(strcmp(script, "apps_update")==0)
				strcpy(nvtmp, "app_update.sh");
			else if(strcmp(script, "apps_stop")==0)
				strcpy(nvtmp, "app_stop.sh");
			else if(strcmp(script, "apps_upgrade")==0)
				strcpy(nvtmp, "app_upgrade.sh");
			else if(strcmp(script, "apps_install")==0)
				strcpy(nvtmp, "app_install.sh");
			else if(strcmp(script, "apps_remove")==0)
				strcpy(nvtmp, "app_remove.sh");
			else if(strcmp(script, "apps_enable")==0)
				strcpy(nvtmp, "app_set_enabled.sh");
			else if(strcmp(script, "apps_switch")==0)
				strcpy(nvtmp, "app_switch.sh");
			else if(strcmp(script, "apps_cancel")==0)
				strcpy(nvtmp, "app_cancel.sh");
			else strcpy(nvtmp, "");

			if(strlen(nvtmp) > 0) {
				nvram_set("apps_state_autorun", "");
				nvram_set("apps_state_install", "");
				nvram_set("apps_state_remove", "");
				nvram_set("apps_state_switch", "");
				nvram_set("apps_state_stop", "");
				nvram_set("apps_state_enable", "");
				nvram_set("apps_state_update", "");
				nvram_set("apps_state_upgrade", "");
				nvram_set("apps_state_cancel", "");
				nvram_set("apps_state_error", "");

				free_caches(FREE_MEM_PAGE, 1, 0);

				cmd[0] = nvtmp;
				start_script(count, cmd);
			}
		}
	}
#endif
	else if(!strncmp(script, "webs_", 5)) 
	{
		if(action&RC_SERVICE_START) {
#ifdef DEBUG_RCTEST // Left for UI debug
			char *webscript_dir;
			webscript_dir = nvram_safe_get("webscript_dir");
			if(strlen(webscript_dir))
				sprintf(nvtmp, "%s/%s.sh", webscript_dir, script);
			else
#endif
			sprintf(nvtmp, "%s.sh", script);
			cmd[0] = nvtmp;
			start_script(count, cmd);
		}
	}
	else if (strcmp(script, "ddns") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_ddns();

		if(action&RC_SERVICE_START) {
			start_ddns();
				
			if (nvram_match("ddns_server_x", "WWW.ASUS.COM")
				&& strstr(nvram_safe_get("ddns_hostname_x"), ".asuscomm.com") != NULL) {
#ifdef RTCONFIG_USB
				// computer_name is followed by DDNS's hostname
//_dprintf("restart_nas_services(%d): test 12.\n", getpid());
				restart_nas_services(1, 1);
#endif
			}
		}
	}	
	else if (strcmp(script, "aidisk_asusddns_register") == 0)
	{
		asusddns_reg_domain(0);
	}
	else if (strcmp(script, "adm_asusddns_register") == 0)
	{
		asusddns_reg_domain(1);
	}
	else if (strcmp(script, "httpd") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_httpd();
		if(action&RC_SERVICE_START) start_httpd();
	}
#ifdef RTCONFIG_IPV6
	else if (strcmp(script, "ipv6") == 0) {
		if (action & RC_SERVICE_STOP)
			stop_ipv6();
		if (action & RC_SERVICE_START)
			start_ipv6();
	}
	else if (strcmp(script, "radvd") == 0) {
		if (action & RC_SERVICE_STOP)
			stop_radvd();
		if (action & RC_SERVICE_START)
			start_radvd();
	}
	else if (strncmp(script, "dhcp6", 5) == 0) {
		if (action & RC_SERVICE_STOP)
			stop_dhcp6c();
		if (action & RC_SERVICE_START)
			start_dhcp6c();
	}
	else if (strcmp(script, "wan6") == 0) {
		if (action & RC_SERVICE_STOP) {
			stop_wan6();
			stop_ipv6();
		}
		if (action & RC_SERVICE_START) {
			start_ipv6();
			// when no option from ipv4, restart wan entirely
			// support wan0 only
			if(update_6rd_info()==0) {
				stop_wan_if(0);
				start_wan_if(0);
			}
			else
				start_wan6();
		}
	}
#endif
	else if (strcmp(script, "dns") == 0)
	{
#ifdef RTCONFIG_DNSMASQ
		if(action&RC_SERVICE_START) {
			/* notify dnsmasq */
			kill_pidfile_s("/var/run/dnsmasq.pid", SIGHUP);
		}
#else
		if(action&RC_SERVICE_START) restart_dns();
#endif
	} 
	else if (strcmp(script, "dhcpd") == 0)
	{
#ifdef RTCONFIG_DNSMASQ
		if(action&RC_SERVICE_STOP) stop_dnsmasq();
		if(action&RC_SERVICE_START) start_dnsmasq();
#else
		if(action&RC_SERVICE_STOP) stop_dhcpd();
		if(action&RC_SERVICE_START) start_dhcpd();
#endif
	}
	else if (strcmp(script, "dnsmasq") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_dnsmasq();
		if(action&RC_SERVICE_START) start_dnsmasq();
	}
	else if (strcmp(script, "upnp") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_upnp();
		if(action&RC_SERVICE_START) start_upnp();
	}
	else if (strcmp(script, "qos") == 0)
	{	
		if(action&RC_SERVICE_STOP) {
			del_iQosRules();
			stop_iQos();
		}
		if(action&RC_SERVICE_START) {
#ifdef RTCONFIG_RALINK
			reinit_hwnat();
#endif
			add_iQosRules(get_wan_ifname(wan_primary_ifunit()));
			start_iQos();
		}
	}
	else if (strcmp(script, "logger") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_logger();
		if(action&RC_SERVICE_START) start_logger();
	}
#ifdef RTCONFIG_CROND
	else if (strcmp(script, "crond") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_cron();
		if(action&RC_SERVICE_START) start_cron();
	}
#endif
	else if (strcmp(script, "firewall") == 0)
	{
		if(action&RC_SERVICE_START)
		{
//			char wan_ifname[16];

#ifdef RTCONFIG_RALINK
			reinit_hwnat();
#endif
			// multiple instance is handled, but 0 is used
			start_default_filter(0);

#ifdef WEB_REDIRECT
			// handled in start_firewall already
			// redirect_setting();
#endif

			// TODO handle multiple wan
			//start_firewall(get_wan_ifname(0, wan_ifname), nvram_safe_get("wan0_ipaddr"), "br0", nvram_safe_get("lan_ipaddr"));
			start_firewall(wan_primary_ifunit(), 0);
		}
	}
	else if (strcmp(script, "iptrestore") == 0)
	{
		// center control for iptable restore, called by process out side of rc
		_dprintf("%s: restart_iptrestore: %s.\n", __FUNCTION__, cmd[1]);	
		if(cmd[1]) {
			if(action&RC_SERVICE_START) eval("iptables-restore", cmd[1]);
		}
	}
	else if (strcmp(script, "pppoe_relay") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_pppoe_relay();
		if(action&RC_SERVICE_START) start_pppoe_relay(get_wanx_ifname(wan_primary_ifunit()));
	}
	else if (strcmp(script, "ntpc") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_ntpc();
		if(action&RC_SERVICE_START) start_ntpc();
	}
	else if (strcmp(script, "rebuild_cifs_config_and_password") ==0)
	{
		fprintf(stderr, "rc rebuilding CIFS config and password databases.\n");
//		regen_passwd_files(); /* Must be called before regen_cifs_config_file(). */
//		regen_cifs_config_file();
	}
	else if (strcmp(script, "time") == 0) 
	{
		if(action&RC_SERVICE_STOP) {
			stop_telnetd();
#ifdef RTCONFIG_SSH
			stop_sshd();
#endif
			stop_logger();
			stop_httpd();
		}	
		if(action&RC_SERVICE_START) {
			refresh_ntpc();
			start_logger();
#ifdef RTCONFIG_SSH
			if (nvram_match("sshd_enable", "1"))
			{
				start_sshd();
			}
#endif
			start_telnetd();
			start_httpd();
			start_firewall(wan_primary_ifunit(), 0);
		}
	}
	else if (strcmp(script, "wps_method")==0)
	{
		if(action&RC_SERVICE_STOP) {
			stop_wps_method();
			if(!nvram_match("wps_ign_btn", "1"))
				kill_pidfile_s("/var/run/watchdog.pid", SIGUSR2);
		}
		if(action&RC_SERVICE_START) {
#ifdef CONFIG_BCMWL5
			stop_wps_method();
#endif
			start_wps_method();
			if(!nvram_match("wps_ign_btn", "1"))
				kill_pidfile_s("/var/run/watchdog.pid", SIGUSR1);
			else
				kill_pidfile_s("/var/run/watchdog.pid", SIGTSTP);
			nvram_unset("wps_ign_btn");
		}
	}
	else if (strcmp(script, "reset_wps")==0)
	{
		reset_wps();
		kill_pidfile_s("/var/run/watchdog.pid", SIGUSR2);
	}
	else if (strcmp(script, "wps")==0)
	{
		if(action&RC_SERVICE_STOP) stop_wps();
		if(action&RC_SERVICE_START) start_wps();
		kill_pidfile_s("/var/run/watchdog.pid", SIGUSR2);
	}
	else if (strcmp(script, "autodet")==0)
	{
		if(action&RC_SERVICE_STOP) stop_autodet();
		if(action&RC_SERVICE_START) start_autodet();
	}
#ifdef RTCONFIG_WIRELESSREPEATER
	else if (strcmp(script, "wlcscan")==0)
	{
		if(action&RC_SERVICE_STOP) stop_wlcscan();
		if(action&RC_SERVICE_START) start_wlcscan();
	}
	else if (strcmp(script, "wlcconnect")==0)
	{
		if(action&RC_SERVICE_STOP) stop_wlcconnect();

#ifdef WEB_REDIRECT
		_dprintf("%s: notify wanduck: wlc_state=%d.\n", __FUNCTION__, nvram_get_int("wlc_state"));
		// notify the change to wanduck.
		kill_pidfile_s("/var/run/wanduck.pid", SIGUSR1);
#endif

		if(action&RC_SERVICE_START) {
			restart_wireless();
			sleep(1);
			start_wlcconnect();
		}
	}
	else if (strcmp(script, "wlcmode")==0)
	{	
		int i;
		if(cmd[1]&& (atoi(cmd[1]) != nvram_get_int("wlc_mode"))) {
			nvram_set_int("wlc_mode", atoi(cmd[1]));
			if(nvram_match("lan_proto", "dhcp") && atoi(cmd[1])==0) {
				nvram_set("lan_ipaddr", nvram_default_get("lan_ipaddr"));
			}
			//setup_dnsmq(atoi(cmd[1]));

#if defined(RTCONFIG_SAMBASRV) && defined(RTCONFIG_FTP)
			stop_ftpd();
			stop_samba();
#endif

#ifdef RTCONFIG_USB_PRINTER
			stop_u2ec();
#endif
			stop_networkmap();
			stop_httpd();
			stop_dnsmasq();
			stop_lan_wlc();
			stop_lan_port();
			stop_lan_wlport();
			for(i=0;i<8;i++) {
				sleep(1);
				_dprintf("sleep\n");
			}
			start_lan_wlport();
			start_lan_port();
			start_lan_wlc();
			start_dnsmasq();
			start_httpd();
			start_networkmap();
#ifdef RTCONFIG_USB_PRINTER
			start_u2ec();
#endif

#if defined(RTCONFIG_SAMBASRV) && defined(RTCONFIG_FTP)
			create_passwd();
			start_samba();
			start_ftpd();
#endif
		}
	}
#endif
	else if (strcmp(script, "restore") == 0) {
		if(cmd[1]) restore_defaults_module(cmd[1]);
	}
	else if (strcmp(script, "chpass") == 0) {
			create_passwd();
	}
	// handle button action
	else if (strcmp(script, "wan_disconnect")==0) {
		logmessage("wan", "disconnected manually");
		stop_wan();
	}
	else if (strcmp(script,"wan_connect")==0)
	{
		logmessage("wan", "connected manually");

		rename("/tmp/ppp/log", "/tmp/ppp/log.~");
		start_wan();
		sleep(2);
		// TODO: function to force ppp connection
	}
#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD)
	else if (strcmp(script, "pptpd") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_pptpd();
		if(action&RC_SERVICE_START) {
			start_pptpd();
			restart_dnsmasq();
			start_firewall(wan_primary_ifunit(), 0);
		}
	}
#endif
#ifdef RTCONFIG_ISP_METER
	else if (strcmp(script, "isp_meter") == 0) {
		_dprintf("%s: isp_meter: %s\n", __FUNCTION__, cmd[1]);
		if(strcmp(cmd[1], "down")==0) {
			stop_wan_if(0);
			update_wan_state("wan0_", WAN_STATE_STOPPED, WAN_STOPPED_REASON_METER_LIMIT);
		}
		else if(strcmp(cmd[1], "up")==0) {
			_dprintf("notify wan up!\n");
			start_wan_if(0);
		}
	}
#endif
	else if (strcmp(script, "nat_rules") == 0) {
		if(action&RC_SERVICE_STOP){
			stop_nat_rules();
		}
		if(action&RC_SERVICE_START){
			start_nat_rules();
		}
	}
	else if (strcmp(script, "sh") == 0) {
		_dprintf("%s: shell: %s\n", __FUNCTION__, cmd[1]);
		if(cmd[1]) system(cmd[1]);
	}

	else if (strcmp(script, "rstats") == 0)
	{
		if(action&RC_SERVICE_STOP) stop_rstats();
		if(action&RC_SERVICE_START) restart_rstats();
	}
        else if (strcmp(script, "cstats") == 0)
        {
                if(action&RC_SERVICE_STOP) stop_cstats();
                if(action&RC_SERVICE_START) restart_cstats();
        }
	else if (strcmp(script, "conntrack") == 0)
	{
		setup_conntrack();
		setup_udp_timeout(TRUE);
//            start_firewall(wan_primary_ifunit(), 0);
	}
#ifdef RTCONFIG_USB
#ifdef LINUX26
        else if (strcmp(script, "sdidle") == 0) {
                if(action&RC_SERVICE_STOP){
                        stop_sd_idle();
                }
                if(action&RC_SERVICE_START){
                        start_sd_idle();
                }
	}
#endif
#endif
	else if (strcmp(script, "leds") == 0) {
		setup_leds();
	}
	else if (strcmp(script, "updateresolv") == 0) {
		update_resolvconf();
	}
	else
	{
		fprintf(stderr,
			"WARNING: rc notified of unrecognized event `%s'.\n",
					script);
	}

	if(nvptr){
_dprintf("goto again(%d)...\n", getpid());
		goto again;
	}

	nvram_set("rc_service", "");
	nvram_set("rc_service_pid", "");	
_dprintf("handle_notifications() end\n");
}

#ifdef RTCONFIG_WIRELESSREPEATER
void
start_wlcscan(void)
{
	char *wlcscan_argv[] = {"wlcscan", NULL};
	pid_t pid;

	if(getpid()!=1) {
		notify_rc("start_wlcscan");
		return;
	}

	killall("wlcscan", SIGTERM);

	return _eval(wlcscan_argv, NULL, 0, &pid);
}

void 
stop_wlcscan(void)
{
	if(getpid()!=1) {
		notify_rc("stop_wlcscan");
		return;
	}

	killall("wlcscan", SIGTERM);
}

void
start_wlcconnect(void)
{
	char *wlcconnect_argv[] = {"wlcconnect", NULL};
	pid_t pid;

	if(getpid()!=1) {
		notify_rc("start_wlcconnect");
		return;
	}

	killall("wlcconnect", SIGTERM);

	return _eval(wlcconnect_argv, NULL, 0, &pid);
}

void 
stop_wlcconnect(void)
{
	if(getpid()!=1) {
		notify_rc("stop_wlcconnect");
		return;
	}

	killall("wlcconnect", SIGTERM);
}
#endif

void
start_autodet(void)
{
	char *autodet_argv[] = {"autodet", NULL};
	pid_t pid;

	if(getpid()!=1) {
		notify_rc("start_autodet");
		return;
	}

	killall_tk("autodet");

	_eval(autodet_argv, NULL, 0, &pid);

	return;
}

void 
stop_autodet(void)
{
	if(getpid()!=1) {
		notify_rc("stop_autodet");
		return;
	}

	killall_tk("autodet");
}

// string = S20transmission -> return value = transmission.
int get_apps_name(const char *string){
	char *ptr;

	if(string == NULL)
		return 0;

	if((ptr = rindex(string, '/')) != NULL)
		++ptr;
	else
		ptr = string;
	if(ptr[0] != 'S')
		return 0;
	++ptr; // S.

	while(ptr != NULL){
		if(isdigit(ptr[0]))
			++ptr;
		else
			break;
	}

	printf("%s", ptr);

	return 1;
}

int run_app_script(const char *pkg_name, const char *pkg_action){
	char app_name[128];

	if(pkg_action == NULL || strlen(pkg_action) <= 0)
		return -1;

	memset(app_name, 0, 128);
	if(pkg_name == NULL)
		strcpy(app_name, "allpkg");
	else
		strcpy(app_name, pkg_name);

	return doSystem("app_init_run.sh %s %s", app_name, pkg_action);
}

void start_nat_rules() {
	// all rules applied directly according to currently status, wanduck help to triger those not cover by normal flow
	if(nvram_match("x_Setting", "0")){
		stop_nat_rules();
		return;
	}
	
	if(nvram_get_int("nat_state")==NAT_STATE_NORMAL) return;
	
	if(getpid() != 1) {
		notify_rc("start_nat_rules");
		return;
	}

	nvram_set_int("nat_state", NAT_STATE_NORMAL);
	_dprintf("%s: apply the nat_rules!\n", __FUNCTION__);
	logmessage("start_nat_rules", "apply the nat_rules!");

	setup_ct_timeout(TRUE);
	setup_udp_timeout(TRUE);

	eval("iptables-restore", NAT_RULES);

	run_custom_script("nat-start", NULL);
	return;
}

void stop_nat_rules() {
	if (nvram_get_int("nat_state")==NAT_STATE_REDIRECT) return ;

	if(getpid() != 1) {
		notify_rc("stop_nat_rules");
		return;
	}

	nvram_set_int("nat_state", NAT_STATE_REDIRECT);

	_dprintf("%s: apply the redirect_rules!\n", __FUNCTION__);
	logmessage("stop_nat_rules", "apply the redirect_rules!");

	setup_ct_timeout(FALSE);
	setup_udp_timeout(FALSE);

	eval("iptables-restore", "/tmp/redirect_rules");

	return;
}

#ifdef RTCONFIG_BCMWL6
extern int restore_defaults_g;
void set_acs_ifnames()
{
	char acs_ifnames[64];
	char word[256], *next;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	int unit;

	unit = 0;
	memset(acs_ifnames, 0, sizeof(acs_ifnames));

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

		if (nvram_match(strcat_r(prefix, "radio", tmp), "1") &&
			nvram_match(strcat_r(prefix, "mode", tmp), "ap") &&
			(	nvram_match(strcat_r(prefix, "chanspec", tmp), "0") ||
				nvram_match(strcat_r(prefix, "bw", tmp), "0")))
		{
			if (nvram_match(strcat_r(prefix, "bw", tmp), "0"))
				nvram_set(strcat_r(prefix, "chanspec", tmp), "0");

			if (strlen(acs_ifnames))
				sprintf(acs_ifnames, "%s %s", acs_ifnames, word);
			else
				sprintf(acs_ifnames, "%s", word);
		}

		unit++;
	}
	nvram_set("acs_ifnames", acs_ifnames);
#if 0
	if (strlen(acs_ifnames))
		nvram_set("wlready", "0");
#endif
}

int
start_acsd(void)
{
	int ret = 0;
#if 0
#ifdef RTCONFIG_PROXYSTA
	if (is_psta(0) || is_psta(1))
	{
#ifdef ACS_ONCE
		nvram_set("acsd_restart_wl", "0");
#endif
		return 0;
	}
#endif
#endif
	if (!restore_defaults_g && strlen(nvram_safe_get("acs_ifnames")))
	{
#ifdef ACS_ONCE
		nvram_set("wlx0_chanspec", "0");
		nvram_set("wlx1_chanspec", "0");
		nvram_set("wly0_chanspec", "0");
		nvram_set("wly1_chanspec", "0");
		nvram_set("acsd_restart_wl", "0");
#endif
		ret = eval("/usr/sbin/acsd");
	}

	return ret;
}

int
stop_acsd(void)
{
	int ret = eval("killall", "acsd");
	return ret;
}
#endif

int service_main(int argc, char *argv[])
{
	if (argc != 2) usage_exit(argv[0], "<action_service>");
	notify_rc(argv[1]);
	printf("\nDone.\n");
	return 0;
}

void setup_leds()
{
	if (nvram_get_int("led_disable")==1) {

		led_control(LED_2G, LED_OFF);
		led_control(LED_5G, LED_OFF);
		led_control(LED_POWER, LED_OFF);
		led_control(LED_SWITCH, LED_OFF);
#ifdef RTCONFIG_USB
		stop_usbled();
		led_control(LED_USB, LED_OFF);
#endif
	} else {
		led_control(LED_2G, LED_ON);
		led_control(LED_5G, LED_ON);
		led_control(LED_POWER, LED_ON);
		led_control(LED_SWITCH, LED_ON);
#ifdef RTCONFIG_USB
		start_usbled();
#endif
	}
}

void stop_cstats(void)
{
	int n, m;
	int pid;
	int pidz;
	int ppidz;
	int w = 0;

	n = 60;
	m = 15;
	while ((n-- > 0) && ((pid = pidof("cstats")) > 0)) {
		w = 1;
		pidz = pidof("gzip");
		if (pidz < 1) pidz = pidof("cp");
		ppidz = ppid(ppid(pidz));
		if ((m > 0) && (pidz > 0) && (pid == ppidz)) {
			syslog(LOG_DEBUG, "cstats(PID %d) shutting down, waiting for helper process to complete(PID %d, PPID %d).\n", pid, pidz, ppidz);
			--m;
		} else {
			kill(pid, SIGTERM);
		}
		sleep(1);
	}
	if ((w == 1) && (n > 0))
		syslog(LOG_DEBUG, "cstats stopped.\n");
}

void start_cstats(int new)
{
	if (nvram_match("cstats_enable", "1")) {
		stop_cstats();
		if (new) {
			syslog(LOG_DEBUG, "starting cstats (new datafile).\n");
			xstart("cstats", "--new");
		} else {
			syslog(LOG_DEBUG, "starting cstats.\n");
			xstart("cstats");
		}
	}
}

void
restart_cstats()
{
        if (nvram_match("cstats_new", "1"))
        {
                start_cstats(1);
                nvram_set("cstats_new", "0");
        }
        else
        {
                start_cstats(0);
        }
}

