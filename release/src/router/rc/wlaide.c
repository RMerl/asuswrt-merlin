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

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <shared.h>
#include <rc.h>

#ifdef RTCONFIG_BCMARM

#define	NORMAL_PERIOD	15	/* second */

static struct itimerval itv;

static void
alarmtimer(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

static
int is_if_up(const char *ifname)
{
	struct ifreq ifr;
	int skfd;

	if (!ifname)
		return -1;

	skfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (skfd == -1)
	{
		perror("socket");
		return -1;
	}

	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
	{
		perror("ioctl");
		close(skfd);
		return -1;
	}
	close(skfd);

	if (ifr.ifr_flags & IFF_UP)
		return 1;
	else
		return 0;
}
#if 0
static int count = 0;
#endif
static void wlaide(int sig)
{
	char word[256], *next;

	foreach (word, nvram_safe_get("wl_ifnames"), next)
	{
#if 0
		count = (count + 1) % 30;
		if (!count)
			wl_check_assoc_scb(word);
#endif
		wl_phy_rssi_ant(word);
	}
}

static void wlaide_exit(int sig)
{
	remove("/var/run/wlaide.pid");
	exit(0);
}

int
wlaide_main(int argc, char *argv[])
{
	FILE *fp;
	sigset_t sigs_to_catch;
	int period = 0;

	/* write pid */
	if ((fp=fopen("/var/run/wlaide.pid", "w")) != NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGALRM);
	sigaddset(&sigs_to_catch, SIGCHLD);
	sigaddset(&sigs_to_catch, SIGTERM);
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);
	signal(SIGALRM, wlaide);
	signal(SIGCHLD, chld_reap);
	signal(SIGTERM, wlaide_exit);

	period = nvram_get_int("wlaide_period");
	period = (period <= 0 ? NORMAL_PERIOD : period);
	alarmtimer(period, 0);

	/* listen for replies */
	while (1)
	{
		pause();
	}

	return 0;
}

#endif
