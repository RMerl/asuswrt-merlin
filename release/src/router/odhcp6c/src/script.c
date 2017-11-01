/**
 * Copyright (C) 2012-2014 Steven Barth <steven@midlink.org>
 * Copyright (C) 2017 Hans Dedecker <dedeckeh@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <stdio.h>
#include <netdb.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include "odhcp6c.h"

static const char hexdigits[] = "0123456789abcdef";
static const int8_t hexvals[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -2, -1, -1, -2, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static char action[16] = "";
static char *argv[4] = {NULL, NULL, action, NULL};
static volatile pid_t running = 0;
static time_t started;

static void script_sighandle(int signal)
{
	if (signal == SIGCHLD) {
		pid_t child;

		while ((child = waitpid(-1, NULL, WNOHANG)) > 0)
			if (running == child)
				running = 0;
	}
}

int script_init(const char *path, const char *ifname)
{
	argv[0] = (char*)path;
	argv[1] = (char*)ifname;
	signal(SIGCHLD, script_sighandle);

	return 0;
}

ssize_t script_unhexlify(uint8_t *dst, size_t len, const char *src)
{
	size_t c;

	for (c = 0; c < len && src[0] && src[1]; ++c) {
		int8_t x = (int8_t)*src++;
		int8_t y = (int8_t)*src++;
		if (x < 0 || (x = hexvals[x]) < 0
				|| y < 0 || (y = hexvals[y]) < 0)
			return -1;
		dst[c] = x << 4 | y;
		while (((int8_t)*src) < 0 ||
				(*src && hexvals[(uint8_t)*src] < 0))
			src++;
	}

	return c;
}

static void script_hexlify(char *dst, const uint8_t *src, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		*dst++ = hexdigits[src[i] >> 4];
		*dst++ = hexdigits[src[i] & 0x0f];
	}

	*dst = 0;
}

static void ipv6_to_env(const char *name,
		const struct in6_addr *addr, size_t cnt)
{
	size_t buf_len = strlen(name);
	char *buf = realloc(NULL, cnt * INET6_ADDRSTRLEN + buf_len + 2);

	memcpy(buf, name, buf_len);
	buf[buf_len++] = '=';

	for (size_t i = 0; i < cnt; ++i) {
		inet_ntop(AF_INET6, &addr[i], &buf[buf_len], INET6_ADDRSTRLEN);
		buf_len += strlen(&buf[buf_len]);
		buf[buf_len++] = ' ';
	}

	if (buf[buf_len - 1] == ' ')
		buf_len--;

	buf[buf_len] = '\0';
	putenv(buf);
}

static void fqdn_to_env(const char *name, const uint8_t *fqdn, size_t len)
{
	size_t buf_len = strlen(name);
	size_t buf_size = len + buf_len + 2;
	const uint8_t *fqdn_end = fqdn + len;
	char *buf = realloc(NULL, len + buf_len + 2);

	memcpy(buf, name, buf_len);
	buf[buf_len++] = '=';

	while (fqdn < fqdn_end) {
		int l = dn_expand(fqdn, fqdn_end, fqdn, &buf[buf_len], buf_size - buf_len);
		if (l <= 0)
			break;
		fqdn += l;
		buf_len += strlen(&buf[buf_len]);
		buf[buf_len++] = ' ';
	}

	if (buf[buf_len - 1] == ' ')
		buf_len--;

	buf[buf_len] = '\0';
	putenv(buf);
}

static void bin_to_env(uint8_t *opts, size_t len)
{
	uint8_t *oend = opts + len, *odata;
	uint16_t otype, olen;

	dhcpv6_for_each_option(opts, oend, otype, olen, odata) {
		char *buf = realloc(NULL, 14 + (olen * 2));
		size_t buf_len = 0;

		snprintf(buf, 14, "OPTION_%hu=", otype);
		buf_len += strlen(buf);

		script_hexlify(&buf[buf_len], odata, olen);
		putenv(buf);
	}
}

enum entry_type {
	ENTRY_ADDRESS,
	ENTRY_HOST,
	ENTRY_ROUTE,
	ENTRY_PREFIX
};

static void entry_to_env(const char *name, const void *data, size_t len, enum entry_type type)
{
	size_t buf_len = strlen(name);
	const struct odhcp6c_entry *e = data;
	// Worst case: ENTRY_PREFIX with iaid != 1 and exclusion
	const size_t max_entry_len = (INET6_ADDRSTRLEN-1 + 5 + 22 + 15 + 10 +
				      INET6_ADDRSTRLEN-1 + 11 + 1);
	char *buf = realloc(NULL, buf_len + 2 + (len / sizeof(*e)) * max_entry_len);

	memcpy(buf, name, buf_len);
	buf[buf_len++] = '=';

	for (size_t i = 0; i < len / sizeof(*e); ++i) {
		inet_ntop(AF_INET6, &e[i].target, &buf[buf_len], INET6_ADDRSTRLEN);
		buf_len += strlen(&buf[buf_len]);

		if (type != ENTRY_HOST) {
			snprintf(&buf[buf_len], 6, "/%"PRIu16, e[i].length);
			buf_len += strlen(&buf[buf_len]);

			if (type == ENTRY_ROUTE) {
				buf[buf_len++] = ',';

				if (!IN6_IS_ADDR_UNSPECIFIED(&e[i].router)) {
					inet_ntop(AF_INET6, &e[i].router, &buf[buf_len], INET6_ADDRSTRLEN);
					buf_len += strlen(&buf[buf_len]);
				}

				snprintf(&buf[buf_len], 23, ",%u,%u", e[i].valid, e[i].priority);
				buf_len += strlen(&buf[buf_len]);
			} else {
				snprintf(&buf[buf_len], 23, ",%u,%u", e[i].preferred, e[i].valid);
				buf_len += strlen(&buf[buf_len]);
			}

			if (type == ENTRY_PREFIX && ntohl(e[i].iaid) != 1) {
				snprintf(&buf[buf_len], 16, ",class=%08x", ntohl(e[i].iaid));
				buf_len += strlen(&buf[buf_len]);
			}

			if (type == ENTRY_PREFIX && e[i].priority) {
				// priority and router are abused for prefix exclusion
				snprintf(&buf[buf_len], 11, ",excluded=");
				buf_len += strlen(&buf[buf_len]);
				inet_ntop(AF_INET6, &e[i].router, &buf[buf_len], INET6_ADDRSTRLEN);
				buf_len += strlen(&buf[buf_len]);
				snprintf(&buf[buf_len], 12, "/%u", e[i].priority);
				buf_len += strlen(&buf[buf_len]);
			}
		}

		buf[buf_len++] = ' ';
	}

	if (buf[buf_len - 1] == ' ')
		buf_len--;

	buf[buf_len] = '\0';
	putenv(buf);
}

static void search_to_env(const char *name, const uint8_t *start, size_t len)
{
	size_t buf_len = strlen(name);
	char *buf = realloc(NULL, buf_len + 2 + len);
	char *c = mempcpy(buf, name, buf_len);
	*c++ = '=';

	for (struct odhcp6c_entry *e = (struct odhcp6c_entry*)start;
				(uint8_t*)e < &start[len] &&
				(uint8_t*)odhcp6c_next_entry(e) <= &start[len];
				e = odhcp6c_next_entry(e)) {
		c = mempcpy(c, e->auxtarget, e->auxlen);
		*c++ = ' ';
	}

	if (c[-1] == ' ')
		c--;

	*c = '\0';
	putenv(buf);
}

static void int_to_env(const char *name, int value)
{
	size_t len = 13 + strlen(name);
	char *buf = realloc(NULL, len);

	snprintf(buf, len, "%s=%d", name, value);
	putenv(buf);
}

static void s46_to_env_portparams(const uint8_t *data, size_t len, FILE *fp)
{
	uint8_t *odata;
	uint16_t otype, olen;

	dhcpv6_for_each_option(data, &data[len], otype, olen, odata) {
		if (otype == DHCPV6_OPT_S46_PORTPARAMS &&
				olen == sizeof(struct dhcpv6_s46_portparams)) {
			struct dhcpv6_s46_portparams *params = (void*)odata;
			fprintf(fp, "offset=%d,psidlen=%d,psid=%d,",
					params->offset, params->psid_len, ntohs(params->psid));
		}
	}
}

static void s46_to_env(enum odhcp6c_state state, const uint8_t *data, size_t len)
{
	const char *name = (state == STATE_S46_MAPE) ? "MAPE" :
			(state == STATE_S46_MAPT) ? "MAPT" : "LW4O6";

	if (len == 0)
		return;

	char *str;
	size_t strsize;

	FILE *fp = open_memstream(&str, &strsize);
	fputs(name, fp);
	fputc('=', fp);

	const char *type = (state == STATE_S46_MAPE) ? "map-e" :
			(state == STATE_S46_MAPT) ? "map-t" : "lw4o6";

	uint8_t *odata;
	uint16_t otype, olen;

	dhcpv6_for_each_option(data, &data[len], otype, olen, odata) {
		struct dhcpv6_s46_rule *rule = (struct dhcpv6_s46_rule*)odata;
		struct dhcpv6_s46_v4v6bind *bind = (struct dhcpv6_s46_v4v6bind*)odata;

		if (state != STATE_S46_LW && otype == DHCPV6_OPT_S46_RULE &&
				olen >= sizeof(struct dhcpv6_s46_rule)) {
			char buf4[INET_ADDRSTRLEN];
			char buf6[INET6_ADDRSTRLEN];
			struct in6_addr in6 = IN6ADDR_ANY_INIT;

			size_t prefix6len = rule->prefix6_len;
			prefix6len = (prefix6len % 8 == 0) ? prefix6len / 8 : prefix6len / 8 + 1;

			if (prefix6len > sizeof(in6) ||
			    olen < sizeof(struct dhcpv6_s46_rule) + prefix6len)
				continue;

			memcpy(&in6, rule->ipv6_prefix, prefix6len);

			inet_ntop(AF_INET, &rule->ipv4_prefix, buf4, sizeof(buf4));
			inet_ntop(AF_INET6, &in6, buf6, sizeof(buf6));

			if (rule->flags & 1)
				fputs("fmr,", fp);

			fprintf(fp, "type=%s,ealen=%d,prefix4len=%d,prefix6len=%d,ipv4prefix=%s,ipv6prefix=%s,",
					type, rule->ea_len, rule->prefix4_len, rule->prefix6_len, buf4, buf6);

			s46_to_env_portparams(&rule->ipv6_prefix[prefix6len],
					olen - sizeof(*rule) - prefix6len, fp);

			dhcpv6_for_each_option(data, &data[len], otype, olen, odata) {
				if (state != STATE_S46_MAPT && otype == DHCPV6_OPT_S46_BR &&
						olen == sizeof(struct in6_addr)) {
					inet_ntop(AF_INET6, odata, buf6, sizeof(buf6));
					fprintf(fp, "br=%s,", buf6);
				} else if (state == STATE_S46_MAPT && otype == DHCPV6_OPT_S46_DMR &&
						olen >= sizeof(struct dhcpv6_s46_dmr)) {
					struct dhcpv6_s46_dmr *dmr = (struct dhcpv6_s46_dmr*)odata;
					memset(&in6, 0, sizeof(in6));
					size_t prefix6len = dmr->dmr_prefix6_len;
					prefix6len = (prefix6len % 8 == 0) ? prefix6len / 8 : prefix6len / 8 + 1;

					if (prefix6len > sizeof(in6) ||
					    olen < sizeof(struct dhcpv6_s46_dmr) + prefix6len)
						continue;

					memcpy(&in6, dmr->dmr_ipv6_prefix, prefix6len);
					inet_ntop(AF_INET6, &in6, buf6, sizeof(buf6));
					fprintf(fp, "dmr=%s/%d,", buf6, dmr->dmr_prefix6_len);
				}
			}

			fputc(' ', fp);
		} else if (state == STATE_S46_LW && otype == DHCPV6_OPT_S46_V4V6BIND &&
				olen >= sizeof(struct dhcpv6_s46_v4v6bind)) {
			char buf4[INET_ADDRSTRLEN];
			char buf6[INET6_ADDRSTRLEN];
			struct in6_addr in6 = IN6ADDR_ANY_INIT;

			size_t prefix6len = bind->bindprefix6_len;
			prefix6len = (prefix6len % 8 == 0) ? prefix6len / 8 : prefix6len / 8 + 1;

			if (prefix6len > sizeof(in6) ||
			    olen < sizeof(struct dhcpv6_s46_v4v6bind) + prefix6len)
				continue;

			memcpy(&in6, bind->bind_ipv6_prefix, prefix6len);

			inet_ntop(AF_INET, &bind->ipv4_address, buf4, sizeof(buf4));
			inet_ntop(AF_INET6, &in6, buf6, sizeof(buf6));

			fprintf(fp, "type=%s,prefix4len=32,prefix6len=%d,ipv4prefix=%s,ipv6prefix=%s,",
					type, bind->bindprefix6_len, buf4, buf6);

			s46_to_env_portparams(&bind->bind_ipv6_prefix[prefix6len],
					olen - sizeof(*bind) - prefix6len, fp);

			dhcpv6_for_each_option(data, &data[len], otype, olen, odata) {
				if (otype == DHCPV6_OPT_S46_BR && olen == sizeof(struct in6_addr)) {
					inet_ntop(AF_INET6, odata, buf6, sizeof(buf6));
					fprintf(fp, "br=%s,", buf6);
				}
			}

			fputc(' ', fp);
		}
	}

	fclose(fp);
	putenv(str);
}

void script_call(const char *status, int delay, bool resume)
{
	time_t now = odhcp6c_get_milli_time() / 1000;
	bool running_script = false;

	if (running) {
		kill(running, SIGTERM);
		delay -= now - started;
		running_script = true;
	}

	if (resume || !running_script || !action[0])
		strncpy(action, status, sizeof(action) - 1);

	pid_t pid = fork();

	if (pid > 0) {
		running = pid;
		started = now;

		if (!resume)
			action[0] = 0;

	} else if (pid == 0) {
		size_t dns_len, search_len, custom_len, sntp_ip_len, ntp_ip_len, ntp_dns_len;
		size_t sip_ip_len, sip_fqdn_len, aftr_name_len, cer_len, addr_len;
		size_t s46_mapt_len, s46_mape_len, s46_lw_len, passthru_len;

		signal(SIGTERM, SIG_DFL);
		if (delay > 0) {
			sleep(delay);
			odhcp6c_expire();
		}

		struct in6_addr *addr = odhcp6c_get_state(STATE_SERVER_ADDR, &addr_len);
		struct in6_addr *dns = odhcp6c_get_state(STATE_DNS, &dns_len);
		uint8_t *search = odhcp6c_get_state(STATE_SEARCH, &search_len);
		uint8_t *custom = odhcp6c_get_state(STATE_CUSTOM_OPTS, &custom_len);
		struct in6_addr *sntp = odhcp6c_get_state(STATE_SNTP_IP, &sntp_ip_len);
		struct in6_addr *ntp = odhcp6c_get_state(STATE_NTP_IP, &ntp_ip_len);
		uint8_t *ntp_dns = odhcp6c_get_state(STATE_NTP_FQDN, &ntp_dns_len);
		struct in6_addr *sip = odhcp6c_get_state(STATE_SIP_IP, &sip_ip_len);
		uint8_t *sip_fqdn = odhcp6c_get_state(STATE_SIP_FQDN, &sip_fqdn_len);
		uint8_t *aftr_name = odhcp6c_get_state(STATE_AFTR_NAME, &aftr_name_len);
		struct in6_addr *cer = odhcp6c_get_state(STATE_CER, &cer_len);
		uint8_t *s46_mapt = odhcp6c_get_state(STATE_S46_MAPT, &s46_mapt_len);
		uint8_t *s46_mape = odhcp6c_get_state(STATE_S46_MAPE, &s46_mape_len);
		uint8_t *s46_lw = odhcp6c_get_state(STATE_S46_LW, &s46_lw_len);
		uint8_t *passthru = odhcp6c_get_state(STATE_PASSTHRU, &passthru_len);

		size_t prefix_len, address_len, ra_pref_len,
			ra_route_len, ra_dns_len, ra_search_len;
		uint8_t *prefix = odhcp6c_get_state(STATE_IA_PD, &prefix_len);
		uint8_t *address = odhcp6c_get_state(STATE_IA_NA, &address_len);
		uint8_t *ra_pref = odhcp6c_get_state(STATE_RA_PREFIX, &ra_pref_len);
		uint8_t *ra_route = odhcp6c_get_state(STATE_RA_ROUTE, &ra_route_len);
		uint8_t *ra_dns = odhcp6c_get_state(STATE_RA_DNS, &ra_dns_len);
		uint8_t *ra_search = odhcp6c_get_state(STATE_RA_SEARCH, &ra_search_len);

		ipv6_to_env("SERVER", addr, addr_len / sizeof(*addr));
		ipv6_to_env("RDNSS", dns, dns_len / sizeof(*dns));
		ipv6_to_env("SNTP_IP", sntp, sntp_ip_len / sizeof(*sntp));
		ipv6_to_env("NTP_IP", ntp, ntp_ip_len / sizeof(*ntp));
		fqdn_to_env("NTP_FQDN", ntp_dns, ntp_dns_len);
		ipv6_to_env("SIP_IP", sip, sip_ip_len / sizeof(*sip));
		fqdn_to_env("DOMAINS", search, search_len);
		fqdn_to_env("SIP_DOMAIN", sip_fqdn, sip_fqdn_len);
		fqdn_to_env("AFTR", aftr_name, aftr_name_len);
		ipv6_to_env("CER", cer, cer_len / sizeof(*cer));
		s46_to_env(STATE_S46_MAPE, s46_mape, s46_mape_len);
		s46_to_env(STATE_S46_MAPT, s46_mapt, s46_mapt_len);
		s46_to_env(STATE_S46_LW, s46_lw, s46_lw_len);
		bin_to_env(custom, custom_len);

		if (odhcp6c_is_bound()) {
			entry_to_env("PREFIXES", prefix, prefix_len, ENTRY_PREFIX);
			entry_to_env("ADDRESSES", address, address_len, ENTRY_ADDRESS);
		}

		entry_to_env("RA_ADDRESSES", ra_pref, ra_pref_len, ENTRY_ADDRESS);
		entry_to_env("RA_ROUTES", ra_route, ra_route_len, ENTRY_ROUTE);
		entry_to_env("RA_DNS", ra_dns, ra_dns_len, ENTRY_HOST);
		search_to_env("RA_DOMAINS", ra_search, ra_search_len);

		int_to_env("RA_HOPLIMIT", ra_conf_hoplimit(0));
		int_to_env("RA_MTU", ra_conf_mtu(0));
		int_to_env("RA_REACHABLE", ra_conf_reachable(0));
		int_to_env("RA_RETRANSMIT", ra_conf_retransmit(0));

		char *buf = malloc(10 + passthru_len * 2);
		strncpy(buf, "PASSTHRU=", 10);
		script_hexlify(&buf[9], passthru, passthru_len);
		putenv(buf);

		execv(argv[0], argv);
		_exit(128);
	}
}
