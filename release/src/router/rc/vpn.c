/*
 * pptp.c
 *
 * Copyright (C) 2007 Sebastian Gottschall <gottschall@dd-wrt.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id:
 */

#include <rc.h>
#include <stdlib.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <utils.h>
#include <syslog.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

char *ip2bcast(char *ip, char *netmask, char *buf)
{
	struct in_addr addr;

	addr.s_addr = inet_addr(ip) | ~inet_addr(netmask);
	if (buf)
		sprintf(buf, "%s", inet_ntoa(addr));

	return buf;
}

void write_chap_secret(char *file)
{
	FILE *fp;
	char *nv, *nvp, *b;
	char *username, *passwd;
	char namebuf[256], passwdbuf[256];

	fp = fopen(file, "w");
	if (fp == NULL)
		return;

	nv = nvp = strdup(nvram_safe_get("pptpd_clientlist"));
	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			if ((vstrsep(b, ">", &username, &passwd) != 2))
				continue;
			if (*username =='\0' /*|| *passwd == '\0'*/)
				continue;
			fprintf(fp, "'%s' * '%s' *\n",
				ppp_safe_escape(username, namebuf, sizeof(namebuf)),
				ppp_safe_escape(passwd, passwdbuf, sizeof(passwdbuf)));
		}
		free(nv);
	}
	fclose(fp);
}

void start_pptpd(void)
{
	FILE *fp;
	char *pptpd_argv[] = {"pptpd",
		"-c", "/tmp/pptpd/pptpd.conf",
		"-o", "/tmp/pptpd/options.pptpd",
		NULL };
	char *nv, *nvp, *b;
	char *pptpd_client, *vpn_network, *vpn_netmask;
	char bcast[32];
	int ret, pptpd_opt, count, port;

	if (!nvram_get_int("pptpd_enable"))
		return;

	if (getpid() != 1) {
		notify_rc("start_pptpd");
		return;
	}


	// cprintf("stop vpn modules\n");
	// stop_vpn_modules ();

	// Create directory for use by pptpd daemon and its supporting files
	mkdir("/tmp/pptpd", 0744);
	//cprintf("open options file\n");
	// Create options file that will be unique to pptpd to avoid interference 
	// with pppoe and pptp
	fp = fopen("/tmp/pptpd/options.pptpd", "w");
	fprintf(fp, "logfile %s\n", "/var/log/pptpd-pppd.log");
	//fprintf(fp, "debug dump logfd 2 nodetach\n");

	if (nvram_get_int("pptpd_radius") &&
	    nvram_invmatch("pptpd_radserver", "") && nvram_invmatch("pptpd_radpass", "")) {
		fprintf(fp,
			"plugin radius.so\n"
			"plugin radattr.so\n"
			"radius-config-file %s\n",
			"/tmp/pptpd/radius/radiusclient.conf");
	}

	fprintf(fp, "lock\n"
		"name *\n"
		"proxyarp\n"
//		"ipcp-accept-local\n"
//		"ipcp-accept-remote\n"
		"lcp-echo-failure 10\n"
		"lcp-echo-interval 6\n"
		"lcp-echo-adaptive\n"
		"deflate 0\n"
		"auth\n"
		"refuse-chap\n"
		"nomppe-stateful\n");

	pptpd_opt = nvram_get_int("pptpd_chap");
	fprintf(fp, "%smschap\n", (pptpd_opt == 0 || pptpd_opt & 1) ? "+" : "-");
	fprintf(fp, "%smschap-v2\n", (pptpd_opt == 0 || pptpd_opt & 2) ? "+" : "-");

	pptpd_opt = nvram_get_int("pptpd_mppe");
	if (pptpd_opt == 0) 
		pptpd_opt = 1 | 4 | 8;
	if (pptpd_opt & (1 | 2 | 4)) {
		fprintf(fp, "%s", (pptpd_opt & 8) ? "" : "require-mppe\n");
		fprintf(fp, "%smppe-128\n", (pptpd_opt & 1) ? "require-" : "no");
		fprintf(fp, "%smppe-56\n", (pptpd_opt & 2) ? "require-" : "no");
		fprintf(fp, "%smppe-40\n", (pptpd_opt & 4) ? "require-" : "no");
	} else
		fprintf(fp, "nomppe nomppc\n");

	fprintf(fp, "chapms-strip-domain\n"
		"chap-secrets /tmp/pptpd/chap-secrets\n"
		"ip-up-script /tmp/pptpd/ip-up\n"
		"ip-down-script /tmp/pptpd/ip-down\n"
		"mtu %d mru %d\n",
		nvram_get_int("pptpd_mtu") ? : 1400,
		nvram_get_int("pptpd_mru") ? : 1400);

	//DNS Server
	count = 0;
	if (nvram_invmatch("pptpd_dns1", ""))
		count += fprintf(fp, "ms-dns %s\n", nvram_safe_get("pptpd_dns1")) > 0 ? 1 : 0;
	if (nvram_invmatch("pptpd_dns2", ""))
		count += fprintf(fp, "ms-dns %s\n", nvram_safe_get("pptpd_dns2")) > 0 ? 1 : 0;
	if (count == 0 && nvram_invmatch("lan_ipaddr", ""))
		fprintf(fp, "ms-dns %s\n", nvram_safe_get("lan_ipaddr"));

        //WINS Server
	count = 0;
	if (nvram_match("pptpd_ms_network", "1") && nvram_match("smbd_enable", "1"))
		count += fprintf(fp, "ms-wins %s\n", nvram_safe_get("lan_ipaddr")) > 0 ? 1 : 0;
	if (nvram_invmatch("pptpd_wins1", "") && count < 2)
		count += fprintf(fp,"ms-wins %s\n", nvram_safe_get("pptpd_wins1")) > 0 ? 1 : 0;
	if (nvram_invmatch("pptpd_wins2", "") && count < 2)
		count += fprintf(fp,"ms-wins %s\n", nvram_safe_get("pptpd_wins2")) > 0 ? 1 : 0;

	// force ppp interface starting from 10
	fprintf(fp, "minunit 10\n");
	fclose(fp);

	// Following is all crude and need to be revisited once testing confirms
	// that it does work
	// Should be enough for testing..
	if (nvram_get_int("pptpd_radius") &&
	    nvram_invmatch("pptpd_radserver", "") && nvram_invmatch("pptpd_radpass", "")) {
		mkdir("/tmp/pptpd/radius", 0744);

		fp = fopen("/tmp/pptpd/radius/radiusclient.conf", "w");
		fprintf(fp, "auth_order radius\n"
			"login_tries 4\n"
			"login_timeout 60\n"
			"radius_timeout 10\n"
			"nologin /etc/nologin\n"
			"servers /tmp/pptpd/radius/servers\n"
			"dictionary /etc/dictionary\n"
			"seqfile /var/run/radius.seq\n"
			"mapfile /etc/port-id-map\n"
			"radius_retries 3\n");
		port = nvram_get_int("pptpd_radport");
		fprintf(fp, "authserver %s%s%s\n", nvram_safe_get("pptpd_radserver"),
			port ? ":" : "", port ? nvram_safe_get("pptpd_radport") : "");
		port = nvram_get_int("pptpd_acctport");
		fprintf(fp, "acctserver %s%s%s\n", nvram_safe_get("pptpd_radserver"),
			port ? ":" : "", port ? nvram_safe_get("pptpd_acctport") : "");
		fclose(fp);

		fp = fopen("/tmp/pptpd/radius/servers", "w");
		fprintf(fp, "%s\t%s\n", nvram_get("pptpd_radserver"),
			nvram_safe_get("pptpd_radpass"));
		fclose(fp);
	}

	// Create pptpd.conf options file for pptpd daemon
	fp = fopen("/tmp/pptpd/pptpd.conf", "w");
	fprintf(fp, "localip %s\n"
		"remoteip %s\n", nvram_safe_get("lan_ipaddr"),
		nvram_safe_get("pptpd_clients"));
	if (nvram_invmatch("pptpd_broadcast", "") &&
	    nvram_invmatch("pptpd_broadcast", "disable")) {
		fprintf(fp, "bcrelay %s,%s\n",
			nvram_safe_get("lan_ifname"), "ppp1[0-9].*");
	}

	append_custom_config("pptpd.conf", fp);

	fclose(fp);

	use_custom_config("pptpd.conf", "/tmp/pptpd/pptpd.conf");
	run_postconf("pptpd", "/tmp/pptpd/pptpd.conf");
	// Create ip-up and ip-down scripts that are unique to pptpd to avoid
	// interference with pppoe and pptp
	ip2bcast(nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"), bcast);

	fp = fopen("/tmp/pptpd/ip-up", "w");
	fprintf(fp, "#!/bin/sh\n"
		"startservice set_routes\n"	// reinitialize
		"echo \"$PPPD_PID $1 $5 $6 $PEERNAME\" >> /tmp/pptp_connected\n" 
		"iptables -I INPUT -i $1 -j ACCEPT\n"
		"iptables -I FORWARD 1 -i $1 -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu\n"
		"iptables -I FORWARD 2 -i $1 -j ACCEPT\n"
		"iptables -t nat -I PREROUTING -i $1 -p udp -m udp --sport 9 -j DNAT --to-destination %s\n",	// rule for wake on lan over pptp tunnel
		bcast);
#if defined(CONFIG_BCMWL5) || defined(RTCONFIG_BCMWL6) || defined(RTCONFIG_BCMARM)
	/* mark connect to bypass CTF */
	if (nvram_match("ctf_disable", "0"))
		fprintf(fp, "iptables -t mangle -A FORWARD -i $1 -m state --state NEW -j MARK --set-mark 0x01/0x7\n");
#endif
	/* Add static route for vpn client */
	nv = nvp = strdup(nvram_safe_get("pptpd_sr_rulelist"));
	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			if ((vstrsep(b, ">", &pptpd_client, &vpn_network, &vpn_netmask) != 3))
				continue;
			if (*pptpd_client == '\0' || *vpn_network == '\0')
				continue;
			if (*vpn_netmask == '\0')
				vpn_netmask = "255.255.255.255";
			fprintf(fp, "if [ \"$PEERNAME\" = \"%s\" ]; then\n", pptpd_client);
			fprintf(fp, "route del -net %s netmask %s\n", vpn_network, vpn_netmask);
			fprintf(fp, "route add -net %s netmask %s dev $1\n", vpn_network, vpn_netmask);
			fprintf(fp, "fi\n");
		}
		free(nv);
	}
	/* Keep ip-up script last */
	if (nvram_invmatch("pptpd_ipup_script", ""))
		fprintf(fp, "%s\n", nvram_safe_get("pptpd_ipup_script"));
	fclose(fp);

	fp = fopen("/tmp/pptpd/ip-down", "w");
	fprintf(fp, "#!/bin/sh\n"
		"sed -i \"/$1/d\" /tmp/pptp_connected\n"
		"iptables -D INPUT -i $1 -j ACCEPT\n"
		"iptables -D FORWARD -i $1 -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu\n"
		"iptables -D FORWARD -i $1 -j ACCEPT\n"
		"iptables -t nat -D PREROUTING -i $1 -p udp -m udp --sport 9 -j DNAT --to-destination %s\n",	// rule for wake on lan over pptp tunnel
		bcast);
#if defined(CONFIG_BCMWL5) || defined(RTCONFIG_BCMWL6) || defined(RTCONFIG_BCMARM)
	/* mark connect to bypass CTF */
	if (nvram_match("ctf_disable", "0"))
		fprintf(fp, "iptables -t mangle -D FORWARD -i $1 -m state --state NEW -j MARK --set-mark 0x01/0x7\n");
#endif
	/* Keep ip-down script last */
	if (nvram_invmatch("pptpd_ipdown_script", ""))
		fprintf(fp, "%s\n", nvram_safe_get("pptpd_ipdown_script"));

	fclose(fp);

	chmod("/tmp/pptpd/ip-up", 0744);
	chmod("/tmp/pptpd/ip-down", 0744);

	// Exctract chap-secrets from nvram
	write_chap_secret("/tmp/pptpd/chap-secrets");
	chmod("/tmp/pptpd/chap-secrets", 0600);

	// Execute pptpd daemon
	ret = _eval(pptpd_argv, NULL, 0, NULL);
	_dprintf("start_pptpd: ret= %d\n", ret);
}

void stop_pptpd(void)
{
	if (getpid() != 1) {
		notify_rc("stop_pptpd");
		return;
	}

	killall_tk("pptpd");
	killall_tk("bcrelay");
}
