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
 
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>

#include <bcmnvram.h>
#include <shutils.h>
#include <rc.h>
#include <stdarg.h>

#define SECONDS_TO_WAIT 3
#define NTP_RETRY_INTERVAL 30

static char servers[32];
int first_sync = 1;
static int sig_cur = -1;

static void ntp_service()
{
	if (first_sync)
	{
		first_sync = 0;
		nvram_set("reload_svc_radio", "1");
		nvram_set("svc_ready", "1");

		setup_timezone();

		if (is_routing_enabled())
			notify_rc("restart_upnp");
#ifdef RTCONFIG_IPV6
		if (get_ipv6_service() != IPV6_DISABLED)
			notify_rc("restart_radvd");
#endif
#ifdef RTCONFIG_DISK_MONITOR
			notify_rc("restart_diskmon");
#endif
	}
}

static void catch_sig(int sig)
{
	static int server_idx = 0;

	sig_cur = sig;

	if (sig == SIGALRM)
	{
		if (strlen(nvram_safe_get("ntp_server0")))
		{
			if (server_idx)
				strcpy(servers, nvram_safe_get("ntp_server1"));
			else
				strcpy(servers, nvram_safe_get("ntp_server0"));
			server_idx = (server_idx + 1) % 2;
		}
		else strcpy(servers, "");
	}
	else if (sig == SIGTSTP)
	{
		ntp_service();
	}
	else if (sig == SIGTERM)
	{
		remove("/var/run/ntp.pid");
		exit(0);
	}
}

int ntp_main(int argc, char *argv[])
{
	FILE *fp;
	int ret;

	strcpy(servers, nvram_safe_get("ntp_server0"));

	fp = fopen("/var/run/ntp.pid", "w");
	if (fp == NULL)
		exit(0);
	fprintf(fp, "%d", getpid());
	fclose(fp);

	dbg("starting ntp...\n");

	signal(SIGALRM, catch_sig);
	signal(SIGTSTP, catch_sig);
	signal(SIGTERM, catch_sig);
	signal(SIGCHLD, chld_reap);

	nvram_set("ntp_ready", "0");
	nvram_set("svc_ready", "0");
	while (1)
	{
		if ((sig_cur != SIGALRM) && (sig_cur != -1))
			pause();
		else if (nvram_get_int("sw_mode") == SW_MODE_ROUTER
				&& !(  nvram_match("link_internet", "1") 
		        ))
		{
			sleep(NTP_RETRY_INTERVAL);
		}
		else if (strlen(servers))
		{
			pid_t pid;
			char *args[] = {"ntpclient", "-h", servers, "-i", "3", "-l", "-s", NULL};
			dbg("run ntpclient\n");

			nvram_set("ntp_server_tried", servers);
			ret = _eval(args, NULL, 0, &pid);
			sleep(SECONDS_TO_WAIT);

			if(nvram_get_int("ntp_ready"))
			{
				nvram_set("ntp_ready", "0");

				/* ntp sync every hour when time_zone set as "DST" */
				if(strstr(nvram_safe_get("time_zone_x"), "DST")) {
					struct tm local;
					time_t now;
					int diff_sec;

					time(&now);
					localtime_r(&now, &local);
//					dbg("%s: %d-%d-%d, %d:%d:%d dst:%d\n", __FUNCTION__, local.tm_year+1900, local.tm_mon+1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, local.tm_isdst);

					/* every hour */
					if((local.tm_min != 0) || (local.tm_sec != 0)) {
						/* compensate for the sleep(SECONDS_TO_WAIT) */
						diff_sec = (3600 - SECONDS_TO_WAIT) - (local.tm_min * 60 + local.tm_sec);
						if(diff_sec == 0) diff_sec = 3600 - SECONDS_TO_WAIT;
						else if(diff_sec < 0) diff_sec = -diff_sec;
//						dbg("diff_sec: %d \n", diff_sec);
						sleep(diff_sec);
					}
					else sleep(3600 - SECONDS_TO_WAIT);
				}
				else	/* every 12 hours */
				{
					sleep(3600 * 12 - SECONDS_TO_WAIT);
				}
			}
			else
			{
				sleep(NTP_RETRY_INTERVAL - SECONDS_TO_WAIT);
			}
		}
		else pause();
	}
}
