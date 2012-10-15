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
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

void get_broadcast(char *ipaddr, char *netmask)
{
        int ip2[4], mask2[4];
        unsigned char ip[4], mask[4];

        if (!ipaddr || !netmask)
                return;

        sscanf(ipaddr, "%d.%d.%d.%d", &ip2[0], &ip2[1], &ip2[2], &ip2[3]);
        sscanf(netmask, "%d.%d.%d.%d", &mask2[0], &mask2[1], &mask2[2],
               &mask2[3]);
        int i = 0;

        for (i = 0; i < 4; i++) {
                ip[i] = ip2[i];
                mask[i] = mask2[i];
                ip[i] = (ip[i] & mask[i]) | (0xff & ~mask[i]);
        }

        sprintf(ipaddr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

        //fprintf(stderr, "get_broadcast return %s\n", value);
}

char *pptp_encode(char *str, char *output)
{
	int i;
	int j;

	i=j=0;

	while(*(str+i)) {
		if(*(str+i)==' ' || *(str+i)=='\\' || *(str+i)=='*' || *(str+i)=='\'' || *(str+i)=='"' || *(str+i)=='#') {
			*(output+j++)='\\';
			*(output+j++)=*(str+i);
		}
		else *(output+j++) = *(str+i); 
		i++;
	}

	*(output+j)=0;

	return output;
}

void write_chap_secret(char *file)
{
        FILE *fp;
        char *nv, *nvp, *b;
        char *username, *passwd;
        char namebuf[256], passwdbuf[256];

        fp=fopen(file, "w");

        if (fp==NULL) return;

        nv = nvp = strdup(nvram_safe_get("pptpd_clientlist"));

        if(nv) {
            	while ((b = strsep(&nvp, "<")) != NULL) {
                	if((vstrsep(b, ">", &username, &passwd)!=2)) continue;
	                if(strlen(username)==0||strlen(passwd)==0) continue;
        	        fprintf(fp, "%s * %s *\n", pptp_encode(username, namebuf), pptp_encode(passwd, passwdbuf));
            	}
            	free(nv);
        }
        fclose(fp);
}

void start_pptpd(void)
{
	int ret = 0, mss = 0, manual_dns = 0, pptpd_opt = 0;
	char *lpTemp;
	FILE *fp;

	int pid = getpid();
	_dprintf("start_pptpd: getpid= %d\n", pid);

	if(getpid() != 1) {
		notify_rc("start_pptpd");
		return;
	}

	if (!nvram_match("pptpd_enable", "1")) {
		return;
	}
	// cprintf("stop vpn modules\n");
	// stop_vpn_modules ();

	// Create directory for use by pptpd daemon and its supporting files
	mkdir("/tmp/pptpd", 0744);
	cprintf("open options file\n");
	// Create options file that will be unique to pptpd to avoid interference 
	// with pppoe and pptp
	fp = fopen("/tmp/pptpd/options.pptpd", "w");
	fprintf(fp, "logfile /var/log/pptpd-pppd.log\n");
	//fprintf(fp, "debug dump logfd 2 nodetach\n");
	if (nvram_match("pptpd_radius", "1"))
		fprintf(fp, "plugin radius.so\nplugin radattr.so\n"
			"radius-config-file /tmp/pptpd/radius/radiusclient.conf\n");

	cprintf("check if wan_wins = zero\n");
	int nowins = 0;

	if (nvram_match("wan_wins", "0.0.0.0")) {
		nvram_set("wan_wins", "");
		nowins = 1;
	}
	if (strlen(nvram_safe_get("wan_wins")) == 0)
		nowins = 1;

	cprintf("write config\n");
	fprintf(fp, "lock\n"
		"name *\n"
		"proxyarp\n"
//		"ipcp-accept-local\n"
//		"ipcp-accept-remote\n"
		"lcp-echo-failure 10\n"
		"lcp-echo-interval 5\n"
		"deflate 0\n" "auth\n" "-chap\n"
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

	fprintf(fp, "ms-ignore-domain\n"
		"chap-secrets /tmp/pptpd/chap-secrets\n"
		"ip-up-script /tmp/pptpd/ip-up\n"
		"ip-down-script /tmp/pptpd/ip-down\n"
		"mtu %s\n" "mru %s\n",
		nvram_get("pptpd_mtu") ? nvram_get("pptpd_mtu") : "1450",
		nvram_get("pptpd_mru") ? nvram_get("pptpd_mru") : "1450");
	//WINS Server
	if (!nowins) {
		fprintf(fp, "ms-wins %s\n", nvram_safe_get("wan_wins"));
	}
	if (strlen(nvram_safe_get("pptpd_wins1"))) {
		fprintf(fp, "ms-wins %s\n", nvram_safe_get("pptpd_wins1"));
	}
	if (strlen(nvram_safe_get("pptpd_wins2"))) {
		fprintf(fp, "ms-wins %s\n", nvram_safe_get("pptpd_wins2"));
	}
	//DNS Server
	if (strlen(nvram_safe_get("pptpd_dns1"))) {
		fprintf(fp, "ms-dns %s\n", nvram_safe_get("pptpd_dns1"));
		manual_dns=1;
	}
	if (strlen(nvram_safe_get("pptpd_dns2"))) {
		fprintf(fp, "ms-dns %s\n", nvram_safe_get("pptpd_dns2"));
		manual_dns=1;
	}
	if(!manual_dns && !nvram_match("lan_ipaddr", ""))
                fprintf(fp, "ms-dns %s\n", nvram_safe_get("lan_ipaddr"));

	// force ppp interface starting from 10
	fprintf(fp, "minunit 10\n");

	// Following is all crude and need to be revisited once testing confirms
	// that it does work
	// Should be enough for testing..
	if (nvram_match("pptpd_radius", "1")) {
		if (nvram_get("pptpd_radserver") != NULL
		    && nvram_get("pptpd_radpass") != NULL) {

			fclose(fp);

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
				"radius_retries 3\n"
				"authserver %s:%s\n",
				nvram_get("pptpd_radserver"),
				nvram_get("pptpd_radport") ?
				nvram_get("pptpd_radport") : "radius");

			if (nvram_get("pptpd_radserver") != NULL
			    && nvram_get("pptpd_acctport") != NULL)
				fprintf(fp, "acctserver %s:%s\n",
					nvram_get("pptpd_radserver"),
					nvram_get("pptpd_acctport") ?
					nvram_get("pptpd_acctport") :
					"radacct");
			fclose(fp);

			fp = fopen("/tmp/pptpd/radius/servers", "w");
			fprintf(fp, "%s\t%s\n", nvram_get("pptpd_radserver"),
				nvram_get("pptpd_radpass"));
			fclose(fp);

		} else
			fclose(fp);
	} else
		fclose(fp);

	// Create pptpd.conf options file for pptpd daemon
	fp = fopen("/tmp/pptpd/pptpd.conf", "w");
	fprintf(fp, "bcrelay %s\n", nvram_safe_get("pptpd_broadcast"));
	fprintf(fp, "localip %s\n"
		"remoteip %s\n", nvram_safe_get("lan_ipaddr"),
		nvram_safe_get("pptpd_clients"));
	append_custom_config("pptpd.conf", fp);
	fclose(fp);

	use_custom_config("pptpd.conf", "/tmp/pptpd/pptpd.conf");

	// Create ip-up and ip-down scripts that are unique to pptpd to avoid
	// interference with pppoe and pptp
	/*
	 * adjust for tunneling overhead (mtu - 40 byte IP - 108 byte tunnel
	 * overhead) 
	 */
	if (nvram_match("mtu_enable", "1"))
		mss = atoi(nvram_safe_get("wan_mtu")) - 40 - 108;
	else
		mss = 1500 - 40 - 108;
	char bcast[32];

	strcpy(bcast, nvram_safe_get("lan_ipaddr"));
	get_broadcast(bcast, nvram_safe_get("lan_netmask"));

	fp = fopen("/tmp/pptpd/ip-up", "w");
	fprintf(fp, "#!/bin/sh\n" "startservice set_routes\n"	// reinitialize 
		"echo $PPPD_PID $1 $5 $6 $PEERNAME >> /tmp/pptp_connected\n" 
		"iptables -I FORWARD -i $1 -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu\n" 
		"iptables -I INPUT -i $1 -j ACCEPT\n" "iptables -I FORWARD -i $1 -j ACCEPT\n" 
		"iptables -t nat -I PREROUTING -i $1 -p udp -m udp --sport 9 -j DNAT --to-destination %s "	// rule for wake on lan over pptp tunnel
		"%s\n", bcast,
		nvram_get("pptpd_ipup_script") ? nvram_get("pptpd_ipup_script") : "");
	fclose(fp);
	fp = fopen("/tmp/pptpd/ip-down", "w");
	fprintf(fp, "#!/bin/sh\n" "grep -v $1  /tmp/pptp_connected > /tmp/pptp_connected.new\n" 
		"mv /tmp/pptp_connected.new /tmp/pptp_connected\n" 
		"iptables -D FORWARD -i $1 -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu\n" 
		"iptables -D INPUT -i $1 -j ACCEPT\n" 
		"iptables -D FORWARD -i $1 -j ACCEPT\n" 
		"iptables -t nat -D PREROUTING -i $1 -p udp -m udp --sport 9 -j DNAT --to-destination %s "	// rule for wake on lan over pptp tunnel
		"%s\n", bcast,
		nvram_get("pptpd_ipdown_script") ? nvram_get("pptpd_ipdown_script") : "");
	fclose(fp);
	chmod("/tmp/pptpd/ip-up", 0744);
	chmod("/tmp/pptpd/ip-down", 0744);

	// Exctract chap-secrets from nvram
	write_chap_secret("/tmp/pptpd/chap-secrets");

	chmod("/tmp/pptpd/chap-secrets", 0600);

	// Execute pptpd daemon
	ret =
	    eval("pptpd", "-c", "/tmp/pptpd/pptpd.conf", "-o",
		 "/tmp/pptpd/options.pptpd");

	_dprintf("start_pptpd: ret= %d\n", ret);
	//dd_syslog(LOG_INFO, "pptpd : pptp daemon successfully started\n");
	return;
}

void stop_pptpd(void)
{
	if (getpid() != 1) {
		notify_rc("stop_pptpd");
	}

	killall_tk("pptpd");
	killall_tk("bcrelay");
	return;
}
