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
#include <signal.h>

int check_process_exist(int pid){
	char path[32];

	memset(path, 0, 32);
	sprintf(path, "/proc/%d", pid);

	return test_if_dir(path);
}

int start_pppd(int unit)
{
	int ret;
	
	FILE *fp;
	char options[80];
	char *pppd_argv[] = { "/usr/sbin/pppd", "file", options, NULL};
	char *l2tpd_argv[] = { "/usr/sbin/l2tpd", "-f", NULL};
	char tmp[100], tmp1[32], prefix[] = "wanXXXXXXXXXX_";
	mode_t mask;
	int pid;

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
	fprintf(fp, "user '%s'\n",
		nvram_safe_get(strcat_r(prefix, "pppoe_username", tmp)));
	fprintf(fp, "password '%s'\n",
		nvram_safe_get(strcat_r(prefix, "pppoe_passwd", tmp)));

	if (nvram_match(strcat_r(prefix, "proto", tmp), "pptp"))
	{
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
			fprintf(fp, "require-mppe-40\n");
		} else
		if (nvram_match(strcat_r(prefix, "pptp_options_x", tmp), "+mppe-56")) {
			fprintf(fp, "nomppe-40\n"
				    "require-mppe-56\n");
		} else
		if (nvram_match(strcat_r(prefix, "pptp_options_x", tmp), "+mppe-128")) {
			fprintf(fp, "nomppe-40\n"
				    "nomppe-56\n"
				    "require-mppe-128\n");
		}
	} else {
		fprintf(fp, "nomppe nomppc\n");
	}

	if (nvram_match(strcat_r(prefix, "proto", tmp), "pppoe"))
	{
#ifdef RTCONFIG_DSL	
		FILE* fp_dsl_mac;
		char buf_mac[32];
		int trp_cnt;
		int rm_cnt;
#endif
		
		fprintf(fp, "plugin rp-pppoe.so");

		if (nvram_invmatch(strcat_r(prefix, "pppoe_service", tmp), "")) {
			fprintf(fp, " rp_pppoe_service '%s'",
				nvram_safe_get(strcat_r(prefix, "pppoe_service", tmp)));
		}

		if (nvram_invmatch(strcat_r(prefix, "pppoe_ac", tmp), "")) {
			fprintf(fp, " rp_pppoe_ac '%s'",
				nvram_safe_get(strcat_r(prefix, "pppoe_ac", tmp)));
		}

		fprintf(fp, " nic-%s\n", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));

		fprintf(fp, "mru %s mtu %s\n",
			nvram_safe_get(strcat_r(prefix, "pppoe_mru", tmp)),
			nvram_safe_get(strcat_r(prefix, "pppoe_mtu", tmp)));


	// wait 10 seconds for DSL MAC address file ready
#ifdef RTCONFIG_DSL
		if (nvram_match("dsl0_proto", "pppoa"))
		{
			strcpy(buf_mac, "00:11:22:33:44:55");
			for (trp_cnt = 0; trp_cnt < 10; trp_cnt++)
			{
				fp_dsl_mac = fopen("/tmp/adsl/tc_mac.txt","r");
				if (fp_dsl_mac != NULL)
				{
					fgets(buf_mac,sizeof(buf_mac),fp_dsl_mac);
					fclose(fp_dsl_mac);					 
					break;
				}
				usleep(1000*1000);				
			}
			// remove cr lf in buf_mac
            for (rm_cnt = 0; rm_cnt < sizeof(buf_mac); rm_cnt++)
            {
            	if (buf_mac[rm_cnt] == 0) break;
            	if (buf_mac[rm_cnt] == 0x0a || buf_mac[rm_cnt] == 0x0d)
            	{
            		buf_mac[rm_cnt]=0;
            		break;
        		}
            }			
			fprintf(fp, "rp_pppoe_sess %d:%s\n", 154, buf_mac);									
		}
#endif		
			
	}

	if (nvram_invmatch(strcat_r(prefix, "proto", tmp), "l2tp")){
		ret = nvram_get_int(strcat_r(prefix, "pppoe_idletime", tmp));
		if (ret && nvram_match(strcat_r(prefix, "pppoe_demand", tmp), "1"))
		{
			fprintf(fp, "idle %d ", ret);
			if (nvram_invmatch(strcat_r(prefix, "pppoe_txonly_x", tmp), "0"))
				fprintf(fp, "tx_only ");
			fprintf(fp, "demand\n");
		}

		fprintf(fp, "persist\n");
	}

	fprintf(fp, "holdoff %s\n", nvram_invmatch(strcat_r(prefix, "pppoe_holdoff", tmp), "")?nvram_safe_get(tmp):"10");	// pppd re-call-time(s)
	fprintf(fp, "maxfail 0\n");

	if (nvram_invmatch(strcat_r(prefix, "dnsenable_x", tmp), "0"))
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

	fprintf(fp, "unit %d\n", unit);
	fprintf(fp, "linkname wan%d\n", unit);

#ifdef RTCONFIG_IPV6
	switch (get_ipv6_service()) {
		case IPV6_NATIVE:
		case IPV6_NATIVE_DHCP:
		case IPV6_MANUAL:
			fprintf(fp, "+ipv6\n");
			break;
        }
#endif

	/* user specific options */
	fprintf(fp, "%s\n",
		nvram_safe_get(strcat_r(prefix, "pppoe_options_x", tmp)));

	fclose(fp);

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
			"maxfail %s\n"
			"holdoff %s\n"
			"hide-avps no\n"
			"section cmd\n\n",
			options,
                        nvram_invmatch(strcat_r(prefix, "heartbeat_x", tmp), "") ?
                                nvram_safe_get(strcat_r(prefix, "heartbeat_x", tmp)) :
                                nvram_safe_get(strcat_r(prefix, "gateway_x", tmp)),
			nvram_invmatch(strcat_r(prefix, "hostname", tmp), "") ?	// ham 0509
				nvram_safe_get(strcat_r(prefix, "hostname", tmp)) : "localhost",
			nvram_invmatch(strcat_r(prefix, "pppoe_maxfail", tmp), "") ?
				nvram_safe_get(strcat_r(prefix, "pppoe_maxfail", tmp)) : "32767",
			nvram_invmatch(strcat_r(prefix, "pppoe_holdoff", tmp), "") ?
				nvram_safe_get(strcat_r(prefix, "pppoe_holdoff", tmp)) : "10");

		fclose(fp);

		/* launch l2tp */
		ret = _eval(l2tpd_argv, NULL, 0, &pid);

		int retry = 3;
		while(!pids("l2tpd") && retry--){
			_dprintf("%s: wait l2tpd up at %d seconds...\n", __FUNCTION__, retry);
			sleep(1);
		}
		sleep(1); // when the pid of l2tpd is existed, also need to wait a more second.

		/* start-session */
		ret = eval("/usr/sbin/l2tp-control", "start-session 0.0.0.0");

		/* pppd sync nodetach noaccomp nobsdcomp nodeflate */
		/* nopcomp novj novjccomp file /tmp/ppp/options.l2tp */

	} else{
		char pid_file[256], *value;
		int orig_pid;
		int wait_time = 0;

		memset(pid_file, 0, 256);
		snprintf(pid_file, 256, "/var/run/ppp-wan%d.pid", unit);

		if((value = file2str(pid_file)) != NULL && (orig_pid = atoi(value)) > 1){
_dprintf("%s: kill pppd(%d).\n", __FUNCTION__, orig_pid);
			kill(orig_pid, SIGHUP);
			sleep(1);
			while(check_process_exist(orig_pid) && wait_time < MAX_WAIT_FILE){
_dprintf("%s: kill pppd(%d).\n", __FUNCTION__, orig_pid);
				++wait_time;
				kill(orig_pid, SIGTERM);
				sleep(1);
			}

			if(check_process_exist(orig_pid)){
				kill(orig_pid, SIGKILL);
				sleep(1);
			}
		}
		if(value != NULL)
			free(value);

		ret = _eval(pppd_argv, NULL, 0, NULL);
	}
	
	return 0;
}

void stop_pppoe_relay()
{	
	killall_tk("pppoe-relay");
}

void start_pppoe_relay(char *wan_if)
{
	if (nvram_match("fw_pt_pppoerelay", "1"))
	{
		char *pppoerelay_argv[] = {"/usr/sbin/pppoe-relay", "-C", "br0", "-S", wan_if, "-F", NULL};
		int ret;
		pid_t pid;

		killall_tk("pppoe-relay");
		ret = _eval(pppoerelay_argv, NULL, 0, &pid);
	}
}
