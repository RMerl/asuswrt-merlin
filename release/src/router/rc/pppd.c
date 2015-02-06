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
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */
#include "rc.h"

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

static int
handle_special_char_for_pppd(char *buf, size_t buf_size, char *src)
{
	const char special_chars[] = "'\\";
	char *p, *q;
	size_t len;

	if (!buf || !src || buf_size <= 1)
		return -1;

	for (p = src, q = buf, len = buf_size; *p != '\0' && len > 1; ++p, --len) {
		if (strchr(special_chars, *p))
			*q++ = '\\';

		*q++ = *p;
	}

	*q++ = '\0';

	return 0;
}

int
start_pppd(int unit)
{
	FILE *fp;
	char options[80];
	char *pppd_argv[] = { "/usr/sbin/pppd", "file", options, NULL};
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char buf[256];	/* although maximum length of pppoe_username/pppoe_passwd is 64. pppd accepts up to 256 characters. */
	mode_t mask;
	int ret = 0;

_dprintf("%s: unit=%d.\n", __FUNCTION__, unit);

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	sprintf(options, "/tmp/ppp/options.wan%d", unit);

	mask = umask(0000);

	/* Generate options file */
	if (!(fp = fopen(options, "w"))) {
		perror(options);
		umask(mask);
		return -1;
	}

	umask(mask);

	/* do not authenticate peer and do not use eap */
	fprintf(fp, "noauth\n");
	fprintf(fp, "refuse-eap\n");
	handle_special_char_for_pppd(buf, sizeof(buf), nvram_safe_get(strcat_r(prefix, "pppoe_username", tmp)));
	fprintf(fp, "user '%s'\n", buf);
	handle_special_char_for_pppd(buf, sizeof(buf), nvram_safe_get(strcat_r(prefix, "pppoe_passwd", tmp)));
	fprintf(fp, "password '%s'\n", buf);

	if (nvram_match(strcat_r(prefix, "proto", tmp), "pptp")) {
		fprintf(fp, "plugin pptp.so\n");
		fprintf(fp, "pptp_server '%s'\n",
			nvram_invmatch(strcat_r(prefix, "heartbeat_x", tmp), "") ?
			nvram_safe_get(strcat_r(prefix, "heartbeat_x", tmp)) :
			nvram_safe_get(strcat_r(prefix, "gateway_x", tmp)));
		/* see KB Q189595 -- historyless & mtu */
		fprintf(fp, "nomppe-stateful mtu 1400\n");
		if (nvram_match(strcat_r(prefix, "pptp_options_x", tmp), "-mppc")) {
			fprintf(fp, "nomppe nomppc\n");
		} else
		if (nvram_match(strcat_r(prefix, "pptp_options_x", tmp), "+mppe-40")) {
			fprintf(fp, "require-mppe\n");
			fprintf(fp, "require-mppe-40\n");
		} else
		if (nvram_match(strcat_r(prefix, "pptp_options_x", tmp), "+mppe-56")) {
			fprintf(fp, "nomppe-40\n"
				    "require-mppe\n"
				    "require-mppe-56\n");
		} else
		if (nvram_match(strcat_r(prefix, "pptp_options_x", tmp), "+mppe-128")) {
			fprintf(fp, "nomppe-40\n"
				    "nomppe-56\n"
				    "require-mppe\n"
				    "require-mppe-128\n");
		}
	} else {
		fprintf(fp, "nomppe nomppc\n");
	}

	if (nvram_match(strcat_r(prefix, "proto", tmp), "pppoe")) {
		fprintf(fp, "plugin rp-pppoe.so nic-%s\n",
			nvram_safe_get(strcat_r(prefix, "ifname", tmp)));

		if (nvram_invmatch(strcat_r(prefix, "pppoe_service", tmp), "")) {
			fprintf(fp, "rp_pppoe_service '%s'\n",
				nvram_safe_get(strcat_r(prefix, "pppoe_service", tmp)));
		}

		if (nvram_invmatch(strcat_r(prefix, "pppoe_ac", tmp), "")) {
			fprintf(fp, "rp_pppoe_ac '%s'\n",
				nvram_safe_get(strcat_r(prefix, "pppoe_ac", tmp)));
		}

#ifdef RTCONFIG_DSL
		if (nvram_match(strcat_r(prefix, "pppoe_auth", tmp), "pap")) {
			fprintf(fp, "-chap\n"
						"-mschap\n"
						"-mschap-v2\n"
						);
		}
		else if (nvram_match(strcat_r(prefix, "pppoe_auth", tmp), "chap")) {
			fprintf(fp, "-pap\n");
		}

		if (nvram_match("dsl0_proto", "pppoa")) {
			FILE *fp_dsl_mac;
			char *dsl_mac = NULL;
			int timeout = 10; /* wait up to 10 seconds */

			while (timeout--) {
				fp_dsl_mac = fopen("/tmp/adsl/tc_mac.txt","r");
				if (fp_dsl_mac != NULL) {
					dsl_mac = fgets(tmp, sizeof(tmp), fp_dsl_mac);
					dsl_mac = strsep(&dsl_mac, "\r\n");
					fclose(fp_dsl_mac);
					break;
				}
				usleep(1000*1000);
			}

			fprintf(fp, "rp_pppoe_sess %d:%s\n", 154,
				(dsl_mac && *dsl_mac) ? dsl_mac : "00:11:22:33:44:55");
		}
#endif

		fprintf(fp, "mru %s mtu %s\n",
			nvram_safe_get(strcat_r(prefix, "pppoe_mru", tmp)),
			nvram_safe_get(strcat_r(prefix, "pppoe_mtu", tmp)));
	}

	if (nvram_invmatch(strcat_r(prefix, "proto", tmp), "l2tp")) {
		ret = nvram_get_int(strcat_r(prefix, "pppoe_idletime", tmp));
		if (ret && nvram_get_int(strcat_r(prefix, "pppoe_demand", tmp))) {
			fprintf(fp, "idle %d ", ret);
			if (nvram_invmatch(strcat_r(prefix, "pppoe_txonly_x", tmp), "0"))
				fprintf(fp, "tx_only ");
			fprintf(fp, "demand\n");
		}
		fprintf(fp, "persist\n");
	}

	fprintf(fp, "holdoff %d\n", nvram_get_int(strcat_r(prefix, "pppoe_holdoff", tmp)) ? : 10);
	fprintf(fp, "maxfail %d\n", nvram_get_int(strcat_r(prefix, "pppoe_maxfail", tmp)));

	if (nvram_get_int(strcat_r(prefix, "dnsenable_x", tmp)))
		fprintf(fp, "usepeerdns\n");

	fprintf(fp, "ipcp-accept-remote ipcp-accept-local noipdefault\n");
	fprintf(fp, "ktune\n");

	/* pppoe set these options automatically */
	/* looks like pptp also likes them */
	fprintf(fp, "default-asyncmap nopcomp noaccomp\n");

	/* pppoe disables "vj bsdcomp deflate" automagically */
	/* ccp should still be enabled - mppe/mppc requires this */
	fprintf(fp, "novj nobsdcomp nodeflate\n");

	/* echo failures */
	fprintf(fp, "lcp-echo-interval 6\n");
	fprintf(fp, "lcp-echo-failure 10\n");

	/* pptp has Echo Request/Reply, l2tp has Hello packets */
	if (nvram_match(strcat_r(prefix, "proto", tmp), "pptp") ||
	    nvram_match(strcat_r(prefix, "proto", tmp), "l2tp"))
		fprintf(fp, "lcp-echo-adaptive\n");

	fprintf(fp, "unit %d\n", unit);
	fprintf(fp, "linkname wan%d\n", unit);

#ifdef RTCONFIG_IPV6
	switch (get_ipv6_service()) {
	case IPV6_NATIVE_DHCP:
	case IPV6_MANUAL:
		if (nvram_match("ipv6_ifdev", "ppp"))
			fprintf(fp, "+ipv6\n");
		break;
        }
#endif

	/* user specific options */
	fprintf(fp, "%s\n",
		nvram_safe_get(strcat_r(prefix, "pppoe_options_x", tmp)));

	fclose(fp);

	/* shut down previous instance if any */
	stop_pppd(unit);
	nvram_set(strcat_r(prefix, "pppoe_ifname", tmp), "");

	if (nvram_match(strcat_r(prefix, "proto", tmp), "l2tp"))
	{
		if (!(fp = fopen("/tmp/l2tp.conf", "w"))) {
			perror(options);
			return -1;
		}

		fprintf(fp, "# automagically generated\n"
			"global\n\n"
			"load-handler \"sync-pppd.so\"\n"
			"load-handler \"cmd.so\"\n\n"
			"section sync-pppd\n\n"
			"lac-pppd-opts \"file %s\"\n\n"
			"section peer\n"
			"port 1701\n"
			"peername %s\n"
			"hostname %s\n"
			"lac-handler sync-pppd\n"
			"persist yes\n"
			"maxfail %d\n"
			"holdoff %d\n"
			"hide-avps no\n"
			"section cmd\n\n",
			options,
                        nvram_invmatch(strcat_r(prefix, "heartbeat_x", tmp), "") ?
                                nvram_safe_get(strcat_r(prefix, "heartbeat_x", tmp)) :
                                nvram_safe_get(strcat_r(prefix, "gateway_x", tmp)),
			nvram_invmatch(strcat_r(prefix, "hostname", tmp), "") ?
				nvram_safe_get(strcat_r(prefix, "hostname", tmp)) : "localhost",
			nvram_get_int(strcat_r(prefix, "pppoe_maxfail", tmp))  ? : 32767,
			nvram_get_int(strcat_r(prefix, "pppoe_holdoff", tmp)) ? : 10);

		fclose(fp);

		/* launch l2tp */
		eval("/usr/sbin/l2tpd");

		ret = 3;
		do {
			_dprintf("%s: wait l2tpd up at %d seconds...\n", __FUNCTION__, ret);
			usleep(1000*1000);
		} while (!pids("l2tpd") && ret--);

		/* start-session */
		ret = eval("/usr/sbin/l2tp-control", "start-session 0.0.0.0");

		/* pppd sync nodetach noaccomp nobsdcomp nodeflate */
		/* nopcomp novj novjccomp file /tmp/ppp/options.l2tp */

	} else
		ret = _eval(pppd_argv, NULL, 0, NULL);

	return ret;
}

void
stop_pppd(int unit)
{
	char pidfile[sizeof("/var/run/ppp-wanXXXXXXXXXX.pid")];

	snprintf(pidfile, sizeof(pidfile), "/var/run/ppp-wan%d.pid", unit);
	if (kill_pidfile_s(pidfile, SIGHUP) == 0 &&
	    kill_pidfile_s(pidfile, SIGTERM) == 0) {
		usleep(1000*1000);
		kill_pidfile_tk(pidfile);
	}
}

int
start_demand_ppp(int unit, int wait)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char *ping_argv[] = { "ping", "-c1", "203.69.138.19", NULL };
	char *value;
	pid_t pid;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	value = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	if (strcmp(value, "pppoe") != 0 && strcmp(value, "pptp") != 0)
		return -1;
	if (nvram_get_int(strcat_r(prefix, "pppoe_demand", tmp)) != 2)
		return -1;

	value = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
	if (inet_addr_(value) != INADDR_ANY)
		ping_argv[2] = value;

	_dprintf("%s: %s\n", __FUNCTION__, "trigger the PPP connection via %s", value);
	logmessage("WAN Connection", "trigger the PPP connection via %s", value);

	return _eval(ping_argv, NULL, 0, wait ? NULL : &pid);
}

int
start_pppoe_relay(char *wan_if)
{
	char *pppoerelay_argv[] = {"/usr/sbin/pppoe-relay",
		"-C", "br0",
		"-S", wan_if,
		"-F", NULL};
	pid_t pid;
	int ret = 0;

	killall_tk("pppoe-relay");
	if (nvram_get_int("fw_pt_pppoerelay"))
		ret = _eval(pppoerelay_argv, NULL, 0, &pid);

	return ret;
}

void
stop_pppoe_relay(void)
{
	killall_tk("pppoe-relay");
}
