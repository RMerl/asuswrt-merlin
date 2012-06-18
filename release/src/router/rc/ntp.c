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
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>

#include <bcmnvram.h>
#include <shutils.h>
#include <rc.h>
#include <stdarg.h>

static int server_idx = 0;
static char servers[32];

static void catch_sig(int sig)
{
	if (sig == SIGTSTP)
	{
		if (server_idx)
			strcpy(servers, nvram_safe_get("ntp_server1"));
		else
			strcpy(servers, nvram_safe_get("ntp_server0"));
		nvram_set("ntp_server_tried", servers);
		server_idx = (++server_idx) % 2;
	}
	else if (sig == SIGTERM)
	{
		remove("/var/run/ntp.pid");
		exit(0);
	}
	else if (sig == SIGALRM)
	{
		if (server_idx)
			strcpy(servers, nvram_safe_get("ntp_server1"));
		else
			strcpy(servers, nvram_safe_get("ntp_server0"));
		nvram_set("ntp_server_tried", servers);
		server_idx = (++server_idx) % 2;
	}
}

int ntp_main(int argc, char *argv[])
{
	FILE *fp;
	int ret;

//	strcpy(servers, nvram_safe_get("ntp_servers"));
	strcpy(servers, nvram_safe_get("ntp_server0"));
	nvram_set("ntp_server_tried", servers);

	if (!strlen(servers))
		return 0;
	
	fp = fopen("/var/run/ntp.pid","w");
	if (fp == NULL)
		exit(0);
	fprintf(fp, "%d", getpid());
	fclose(fp);

	fprintf(stderr, "starting ntp...\n");

	signal(SIGTSTP, catch_sig);
	signal(SIGALRM, catch_sig);

	while (1)
	{
		ret = doSystem("ntpclient -h %s -i 3 -l -s &", servers);
		sleep(3);
		setup_timezone();
		
		// only dst_timezone 
		if(nvram_safe_get("time_zone_dst")){
			struct tm local;
			time_t now;
			int diff_sec;

			time(&now);
			localtime_r(&now, &local);
			//cprintf("%s: %d-%d-%d, %d:%d:%d dst:%d\n", __FUNCTION__, local.tm_year+1900, local.tm_mon+1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, local.tm_isdst); //tmp test
	
			/* every hour */
			// 3597 = 3600 - 3, because of sleep(3)
			if((local.tm_min != 0) || (local.tm_sec != 0)){
				diff_sec = (3600-3) - (local.tm_min*60 + local.tm_sec);
				if(diff_sec == 0) diff_sec = 3597;
				//fprintf(stderr, "diff_sec: %d \n", diff_sec);
				alarm(diff_sec);
			}
			else{
				alarm(3597);
			}
		}

		pause();
	}	
}
