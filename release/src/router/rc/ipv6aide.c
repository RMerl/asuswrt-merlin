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

#include <sys/signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <rc.h>
#include <shutils.h>
#include <shared.h>

#define NORMAL_PERIOD	1	/* second */

static time_t up;
static struct itimerval itv;

static void alarmtimer(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec  = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

static void ipv6aide_exit(int sig)
{
	alarmtimer(0, 0);

	remove("/var/run/ipv6aide.pid");
	exit(0);
}

static void ipv6aide_check(int sig)
{
	char tmp[64];
	char *p = NULL;
	char *q;

	if (get_ipv6_service() != IPV6_NATIVE_DHCP)
		goto END;

	memset(tmp, 0, sizeof(tmp));
	q = tmp;
	p = strtok_r(ipv6_gateway_address(), " ", &q);

	if (!p || !strlen(p) || !strlen(q))
	{
		alarmtimer(NORMAL_PERIOD, 0);
		return;
	}

	dbG("ipv6 gateway: %s (dev %s)\n", p, q);
	eval("route", "-A", "inet6", "add", "2000::/3", "gw", p, "dev", q);
END:
	ipv6aide_exit(sig);
}

int
ipv6aide_main(int argc, char *argv[])
{
	FILE *fp;
	sigset_t sigs_to_catch;

	/* write pid */
	if ((fp=fopen("/var/run/ipv6aide.pid", "w")) != NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	up = uptime();

	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGTERM);
	sigaddset(&sigs_to_catch, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);
	signal(SIGTERM, ipv6aide_exit);
	signal(SIGALRM, ipv6aide_check);
	alarmtimer(NORMAL_PERIOD, 0);

	/* listen for replies */
	while (1)
	{
		pause();
	}

	return 0;
}
