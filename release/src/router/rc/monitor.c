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
 * Copyright 2012, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <shared.h>

#ifdef RTCONFIG_IPERF

#define NORMAL_PERIOD	3

static struct itimerval itv;
static void
alarmtimer(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec  = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

static void
keepalive()
{
	if (!pids("iperf"))
		start_iperf();	
}

static void
monitor(int sig)
{
	if (sig == SIGALRM)
	{
		keepalive();
		alarm(NORMAL_PERIOD);
	}
}

static void
monitor_exit(int sig)
{
	if (sig == SIGTERM)
	{
		alarmtimer(0, 0);
		remove("/var/run/monitor.pid");
		exit(0);
	}
}

int 
monitor_main(int argc, char *argv[])
{
	FILE *fp;
	sigset_t sigs_to_catch;

	/* write pid */
	if ((fp = fopen("/var/run/monitor.pid", "w")) != NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	/* set the signal handler */
	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGALRM);
	sigaddset(&sigs_to_catch, SIGTERM);
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

	signal(SIGALRM, monitor);
	signal(SIGTERM, monitor_exit);

	alarm(NORMAL_PERIOD);

	/* Most of time it goes to sleep */
	while (1)
	{
		pause();
	}

	return 0;
}
#endif
