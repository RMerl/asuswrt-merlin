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
 */

#include <rc.h>
#ifdef RTCONFIG_RALINK
#include <sys/signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bcmnvram.h>
#include <shutils.h>

static time_t up;
static struct itimerval itv;

static void alarmtimer(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec  = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

static void wpsfix_exit(int sig)
{
	alarmtimer(0, 0);

	remove("/var/run/wpsfix.pid");
	exit(0);
}

static void wpsfix_check(int sig)
{
	int unit;
	time_t now = uptime();

	if (nvram_match("wps_band", "0"))
		unit = 0;
	else
		unit = 1;

	if (((now - up) >= 600) && !wl_WscConfigured(unit))
	{
		stop_wsc();

		nvram_set("wps_sta_pin", "00000000");
		nvram_set("wps_enable", "0");
		nvram_set("wl_wps_mode", "disabled");
		nvram_set("wl0_wps_mode", "disabled");
		nvram_set("wl1_wps_mode", "disabled");

		wpsfix_exit(sig);
	}
	else
		alarmtimer(60, 0);
}

int
wpsfix_main(int argc, char *argv[])
{
	FILE *fp;

	/* write pid */
	if ((fp=fopen("/var/run/wpsfix.pid", "w"))!=NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	up = uptime();
	signal(SIGTERM, wpsfix_exit);
	signal(SIGALRM, wpsfix_check);
	alarmtimer(60, 0);

	/* listen for replies */
	while (1)
	{
		pause();
	}
}
#endif
