/*****************************************************************************/

/*
 *	cpu.c -- simple CPU usage reporting tool.
 *
 *	(C) Copyright 2000, Greg Ungerer (gerg@snapgear.com)
 */

/*****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/time.h>

/*****************************************************************************/

static char *version = "1.0.1";

/*****************************************************************************/

struct stats {
	unsigned int	user;
	unsigned int	nice;
	unsigned int	system;
	unsigned int	idle;
	unsigned int	total;
};

/*****************************************************************************/

static int getdata(FILE *fp, struct stats *st)
{
	unsigned char	buf[80];

	if (fseek(fp, 0, SEEK_SET) < 0)
		return(-1);
	fscanf(fp, "%s %d %d %d %d", &buf[0], &st->user, &st->nice,
		&st->system, &st->idle);

	st->total = st->user + st->nice + st->system + st->idle;
	return(0);
}

/*****************************************************************************/

static volatile int gotalarm;

static void alarm_handler(int i)
{
	        gotalarm = 1;
}
#if 0
static struct itimerval itv;
static void
alarmtimer(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec  = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}
#endif
/*****************************************************************************/

static void usage(FILE *fp, int rc)
{
	fprintf(fp, "Usage: cpu [-h?rai] [-s seconds] [-c count] "
		"[-d <device>]\n\n");
	fprintf(fp, "        -h?            this help\n");
	fprintf(fp, "        -v             print version info\n");
	fprintf(fp, "        -r             repeated output\n");
	fprintf(fp, "        -a             print system average\n");
	fprintf(fp, "        -i             idle measurement via busy loop\n");
	fprintf(fp, "        -c count       repeat count times\n");
#if 0
	fprintf(fp, "        -s seconds     seconds between output\n");
#endif
	fprintf(fp, "        -d <device>    proc device to use (default /proc/stat)\n");
	exit(rc);
}

/*****************************************************************************/

int cpu_main(int argc, char *argv[], int output)
{
	FILE		*fp;
	struct stats	st, stold;
	unsigned int	curtotal;
#if 1
	int		c, cnt, repeat, delay, average, idle;
#else
	int             c, cnt, repeat, average, idle;
#endif
	char		*procdevice;
	struct timeval	start, stop;
	double idletotal = -1.00, idlediff;
	unsigned int	timediff, idlecount=0, idlepercent;
	struct sigaction sa;
	unsigned int busy = 0, busy2 = 0;

	repeat = 0;
#if 1
	delay = 1;
#endif
	procdevice = "/proc/stat";
	cnt = 1;
	average = 0;
	idle = 0;

	if (argc)
	while ((c = getopt(argc, argv, "raivh?s:d:c:")) > 0) {
		switch (c) {
		case 'v':
			if (output)
			printf("%s: version %s\n", argv[0], version);
#if 0
			exit(0);
#else
			return busy;
#endif
		case 'a':
			average++;
			break;
		case 'i':
			idle++;
			break;
		case 'r':
			repeat++;
			break;
#if 1
		case 's':
			delay = atoi(optarg);
			break;
#endif
		case 'd':
			procdevice = optarg;
			break;
		case 'c':
			cnt = atoi(optarg);
			break;
		case 'h':
		case '?':
			usage(stdout, 0);
			break;
		default:
			fprintf(stderr, "ERROR: unkown option '%c'\n", c);
			usage(stderr, 1);
			break;
		}
	}

	/*
	 *	Check device is real, and open it.
	 */
	if ((fp = fopen(procdevice, "r")) == NULL) {
		fprintf(stderr, "ERROR: failed to open %s, errno=%d\n",
			procdevice, errno);
#if 0
		exit(0);
#else
		return busy;
#endif
	}

	if (setvbuf(fp, NULL, _IONBF, 0) != 0) {
		fprintf(stderr, "ERROR: failed to setvbuf(%s), errno=%d\n",
			procdevice, errno);
#if 0
		exit(0);
#else
		return busy;
#endif
	}

	getdata(fp, &st);

	if (average) {
		if (output)
		printf("CPU:  average %d%%  (system=%d%% user=%d%% "
			"nice=%d%% idle=%d%%)\n",
			(st.system + st.user + st.nice) * 100 / st.total,
			st.system * 100 / st.total, st.user * 100 / st.total,
			st.nice * 100 / st.total, st.idle * 100 / st.total);
		cnt = repeat = 0;
	}

	if (idle) {
		nice(19);
		memset(&sa, 0, sizeof(sa));
		sa.sa_flags = 0;
		sa.sa_handler = alarm_handler;
		sigaction(SIGALRM, &sa, NULL);
	} 


	for (c = 0; ((c < cnt) || repeat); c++) {
		if (idle) {
			gotalarm = 0;
#if 1
			alarm(delay);
#else
			alarmtimer(1, 0);
#endif
			gettimeofday(&start, NULL);
			for (idlecount=0; !gotalarm; idlecount++);
			gettimeofday(&stop, NULL);
		} else {
#if 1
			sleep(delay);
#else
			sleep(1);
#endif
		}
		stold = st;
		getdata(fp, &st);

		curtotal = st.total - stold.total;
		if (idle) {
			timediff = (stop.tv_sec - start.tv_sec)*1000000
				+ stop.tv_usec - start.tv_usec;
			idlediff = (double)idlecount / (double)timediff;
			if (idlediff > idletotal)
				idletotal = idlediff;
			idlepercent = idlediff * 100.0 / idletotal;
			/* Now move ticks from st.nice to st.idle to account
			 * for our busy loop.  Don't recalculate idlepercent
			 * though to preserve accuracy. */
			st.nice = st.nice + st.idle - stold.idle
				- idlepercent * curtotal / 100;
			st.idle = stold.idle
				+ idlepercent * curtotal / 100;
			if (st.nice < stold.nice)
				st.nice = stold.nice;
		} else {
			idlepercent = (st.idle - stold.idle) * 100 / curtotal;
		}

		busy =	((st.system + st.user + st.nice) - 
			(stold.system + stold.user + stold.nice)) *
			100 / curtotal;

		busy2 = (st.system - stold.system) * 100 / curtotal;

		if (output)
		printf("CPU:  busy %d%%  (system=%d%% user=%d%% "
			"nice=%d%% idle=%d%%)\n",
#if 0
			((st.system + st.user + st.nice) -
			 (stold.system + stold.user + stold.nice)) *
			 100 / curtotal,
#else
			busy,
#endif
			(st.system - stold.system) * 100 / curtotal,
			(st.user - stold.user) * 100 / curtotal,
			(st.nice - stold.nice) * 100 / curtotal,
			idlepercent);
	}

	fclose(fp);
#if 0
	exit(0);
#else
	return busy2;
#endif
}

/*****************************************************************************/
