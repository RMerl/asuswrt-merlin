/*
 * rtacct.c		Applet to display contents of /proc/net/rt_acct.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <fnmatch.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include <math.h>

#include "rt_names.h"

#include <SNAPSHOT.h>

int reset_history = 0;
int ignore_history = 0;
int no_output = 0;
int no_update = 0;
int scan_interval = 0;
int time_constant = 0;
int dump_zeros = 0;
unsigned long magic_number = 0;
double W;

static int generic_proc_open(const char *env, const char *name)
{
	char store[1024];
	char *p = getenv(env);
	if (!p) {
		p = getenv("PROC_ROOT") ? : "/proc";
		snprintf(store, sizeof(store)-1, "%s/%s", p, name);
		p = store;
	}
	return open(p, O_RDONLY);
}

static int net_rtacct_open(void)
{
	return generic_proc_open("PROC_NET_RTACCT", "net/rt_acct");
}

static __u32 rmap[256/4];

struct rtacct_data
{
	__u32			ival[256*4];

	unsigned long long	val[256*4];
	double			rate[256*4];
	char			signature[128];
};

static struct rtacct_data kern_db_static;

static struct rtacct_data *kern_db = &kern_db_static;
static struct rtacct_data *hist_db;

static void nread(int fd, char *buf, int tot)
{
	int count = 0;

	while (count < tot) {
		int n = read(fd, buf+count, tot-count);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			exit(-1);
		}
		if (n == 0)
			exit(-1);
		count += n;
	}
}

static __u32 *read_kern_table(__u32 *tbl)
{
	static __u32 *tbl_ptr;
	int fd;

	if (magic_number) {
		if (tbl_ptr != NULL)
			return tbl_ptr;

		fd = open("/dev/mem", O_RDONLY);
		if (fd < 0) {
			perror("magic open");
			exit(-1);
		}
		tbl_ptr = mmap(NULL, 4096,
			       PROT_READ,
			       MAP_SHARED,
			       fd, magic_number);
		if ((unsigned long)tbl_ptr == ~0UL) {
			perror("magic mmap");
			exit(-1);
		}
		close(fd);
		return tbl_ptr;
	}

	fd = net_rtacct_open();
	if (fd >= 0) {
		nread(fd, (char*)tbl, 256*16);
		close(fd);
	} else {
		memset(tbl, 0, 256*16);
	}
	return tbl;
}

static void format_rate(FILE *fp, double rate)
{
	char temp[64];

	if (rate > 1024*1024) {
		sprintf(temp, "%uM", (unsigned)rint(rate/(1024*1024)));
		fprintf(fp, " %-10s", temp);
	} else if (rate > 1024) {
		sprintf(temp, "%uK", (unsigned)rint(rate/1024));
		fprintf(fp, " %-10s", temp);
	} else
		fprintf(fp, " %-10u", (unsigned)rate);
}

static void format_count(FILE *fp, unsigned long long val)
{
	if (val > 1024*1024*1024)
		fprintf(fp, " %10lluM", val/(1024*1024));
	else if (val > 1024*1024)
		fprintf(fp, " %10lluK", val/1024);
	else
		fprintf(fp, " %10llu", val);
}

static void dump_abs_db(FILE *fp)
{
	int realm;
	char b1[16];

	if (!no_output) {
		fprintf(fp, "#%s\n", kern_db->signature);
		fprintf(fp,
"%-10s "
"%-10s "
"%-10s "
"%-10s "
"%-10s "
"\n"
		       , "Realm", "BytesTo", "PktsTo", "BytesFrom", "PktsFrom");
		fprintf(fp,
"%-10s "
"%-10s "
"%-10s "
"%-10s "
"%-10s "
"\n"
		       , "", "BPSTo", "PPSTo", "BPSFrom", "PPSFrom");

	}

	for (realm=0; realm<256; realm++) {
		int i;
		unsigned long long *val;
		double		   *rate;

		if (!(rmap[realm>>5] & (1<<(realm&0x1f))))
			continue;

		val = &kern_db->val[realm*4];
		rate = &kern_db->rate[realm*4];

		if (!dump_zeros &&
		    !val[0] && !rate[0] &&
		    !val[1] && !rate[1] &&
		    !val[2] && !rate[2] &&
		    !val[3] && !rate[3])
			continue;

		if (hist_db) {
			memcpy(&hist_db->val[realm*4], val, sizeof(*val)*4);
		}

		if (no_output)
			continue;

		fprintf(fp, "%-10s", rtnl_rtrealm_n2a(realm, b1, sizeof(b1)));
		for (i = 0; i < 4; i++)
			format_count(fp, val[i]);
		fprintf(fp, "\n%-10s", "");
		for (i = 0; i < 4; i++)
			format_rate(fp, rate[i]);
		fprintf(fp, "\n");
	}
}


static void dump_incr_db(FILE *fp)
{
	int k, realm;
	char b1[16];

	if (!no_output) {
		fprintf(fp, "#%s\n", kern_db->signature);
		fprintf(fp,
"%-10s "
"%-10s "
"%-10s "
"%-10s "
"%-10s "
"\n"
		       , "Realm", "BytesTo", "PktsTo", "BytesFrom", "PktsFrom");
		fprintf(fp,
"%-10s "
"%-10s "
"%-10s "
"%-10s "
"%-10s "
"\n"
		       , "", "BPSTo", "PPSTo", "BPSFrom", "PPSFrom");
	}

	for (realm=0; realm<256; realm++) {
		int ovfl = 0;
		int i;
		unsigned long long *val;
		double		   *rate;
		unsigned long long rval[4];

		if (!(rmap[realm>>5] & (1<<(realm&0x1f))))
			continue;

		val = &kern_db->val[realm*4];
		rate = &kern_db->rate[realm*4];

		for (k=0; k<4; k++) {
			rval[k] = val[k];
			if (rval[k] < hist_db->val[realm*4+k])
				ovfl = 1;
			else
				rval[k] -= hist_db->val[realm*4+k];
		}
		if (ovfl) {
			for (k=0; k<4; k++)
				rval[k] = val[k];
		}
		if (hist_db) {
			memcpy(&hist_db->val[realm*4], val, sizeof(*val)*4);
		}

		if (no_output)
			continue;

		if (!dump_zeros &&
		    !rval[0] && !rate[0] &&
		    !rval[1] && !rate[1] &&
		    !rval[2] && !rate[2] &&
		    !rval[3] && !rate[3])
			continue;


		fprintf(fp, "%-10s", rtnl_rtrealm_n2a(realm, b1, sizeof(b1)));
		for (i = 0; i < 4; i++)
			format_count(fp, rval[i]);
		fprintf(fp, "\n%-10s", "");
		for (i = 0; i < 4; i++)
			format_rate(fp, rate[i]);
		fprintf(fp, "\n");
	}
}


static int children;

static void sigchild(int signo)
{
}

/* Server side only: read kernel data, update tables, calculate rates. */

static void update_db(int interval)
{
	int i;
	__u32 *ival;
	__u32 _ival[256*4];

	ival = read_kern_table(_ival);

	for (i=0; i<256*4; i++) {
		double sample;
		__u32 incr = ival[i] - kern_db->ival[i];

		if (ival[i] == 0 && incr == 0 &&
		    kern_db->val[i] == 0 && kern_db->rate[i] == 0)
			continue;

		kern_db->val[i] += incr;
		kern_db->ival[i] = ival[i];
		sample = (double)(incr*1000)/interval;
		if (interval >= scan_interval) {
			kern_db->rate[i] += W*(sample-kern_db->rate[i]);
		} else if (interval >= 1000) {
			if (interval >= time_constant) {
				kern_db->rate[i] = sample;
			} else {
				double w = W*(double)interval/scan_interval;
				kern_db->rate[i] += w*(sample-kern_db->rate[i]);
			}
		}
	}
}

static void send_db(int fd)
{
	int tot = 0;

	while (tot < sizeof(*kern_db)) {
		int n = write(fd, ((char*)kern_db) + tot, sizeof(*kern_db)-tot);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return;
		}
		tot += n;
	}
}



#define T_DIFF(a,b) (((a).tv_sec-(b).tv_sec)*1000 + ((a).tv_usec-(b).tv_usec)/1000)


static void pad_kern_table(struct rtacct_data *dat, __u32 *ival)
{
	int i;
	memset(dat->rate, 0, sizeof(dat->rate));
	if (dat->ival != ival)
		memcpy(dat->ival, ival, sizeof(dat->ival));
	for (i=0; i<256*4; i++)
		dat->val[i] = ival[i];
}

static void server_loop(int fd)
{
	struct timeval snaptime = { 0 };
	struct pollfd p;
	p.fd = fd;
	p.events = p.revents = POLLIN;

	sprintf(kern_db->signature,
		"%u.%lu sampling_interval=%d time_const=%d",
		(unsigned) getpid(), (unsigned long)random(),
		scan_interval/1000, time_constant/1000);

	pad_kern_table(kern_db, read_kern_table(kern_db->ival));

	for (;;) {
		int status;
		int tdiff;
		struct timeval now;
		gettimeofday(&now, NULL);
		tdiff = T_DIFF(now, snaptime);
		if (tdiff >= scan_interval) {
			update_db(tdiff);
			snaptime = now;
			tdiff = 0;
		}
		if (poll(&p, 1, tdiff + scan_interval) > 0
		    && (p.revents&POLLIN)) {
			int clnt = accept(fd, NULL, NULL);
			if (clnt >= 0) {
				pid_t pid;
				if (children >= 5) {
					close(clnt);
				} else if ((pid = fork()) != 0) {
					if (pid>0)
						children++;
					close(clnt);
				} else {
					if (tdiff > 0)
						update_db(tdiff);
					send_db(clnt);
					exit(0);
				}
			}
		}
		while (children && waitpid(-1, &status, WNOHANG) > 0)
			children--;
	}
}

static int verify_forging(int fd)
{
	struct ucred cred;
	socklen_t olen = sizeof(cred);

	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, (void*)&cred, &olen) ||
	    olen < sizeof(cred))
		return -1;
	if (cred.uid == getuid() || cred.uid == 0)
		return 0;
	return -1;
}

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr,
"Usage: rtacct [ -h?vVzrnasd:t: ] [ ListOfRealms ]\n"
		);
	exit(-1);
}

int main(int argc, char *argv[])
{
	char hist_name[128];
	struct sockaddr_un sun;
	int ch;
	int fd;

	while ((ch = getopt(argc, argv, "h?vVzrM:nasd:t:")) != EOF) {
		switch(ch) {
		case 'z':
			dump_zeros = 1;
			break;
		case 'r':
			reset_history = 1;
			break;
		case 'a':
			ignore_history = 1;
			break;
		case 's':
			no_update = 1;
			break;
		case 'n':
			no_output = 1;
			break;
		case 'd':
			scan_interval = 1000*atoi(optarg);
			break;
		case 't':
			if (sscanf(optarg, "%d", &time_constant) != 1 ||
			    time_constant <= 0) {
				fprintf(stderr, "rtacct: invalid time constant divisor\n");
				exit(-1);
			}
			break;
		case 'v':
		case 'V':
			printf("rtacct utility, iproute2-ss%s\n", SNAPSHOT);
			exit(0);
		case 'M':
			/* Some secret undocumented option, nobody
			 * is expected to ask about its sense. See?
			 */
			sscanf(optarg, "%lx", &magic_number);
			break;
		case 'h':
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc) {
		while (argc > 0) {
			__u32 realm;
			if (rtnl_rtrealm_a2n(&realm, argv[0])) {
				fprintf(stderr, "Warning: realm \"%s\" does not exist.\n", argv[0]);
				exit(-1);
			}
			rmap[realm>>5] |= (1<<(realm&0x1f));
			argc--; argv++;
		}
	} else {
		memset(rmap, ~0, sizeof(rmap));
		/* Always suppress zeros. */
		dump_zeros = 0;
	}

	sun.sun_family = AF_UNIX;
	sun.sun_path[0] = 0;
	sprintf(sun.sun_path+1, "rtacct%d", getuid());

	if (scan_interval > 0) {
		if (time_constant == 0)
			time_constant = 60;
		time_constant *= 1000;
		W = 1 - 1/exp(log(10)*(double)scan_interval/time_constant);
		if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
			perror("rtacct: socket");
			exit(-1);
		}
		if (bind(fd, (struct sockaddr*)&sun, 2+1+strlen(sun.sun_path+1)) < 0) {
			perror("rtacct: bind");
			exit(-1);
		}
		if (listen(fd, 5) < 0) {
			perror("rtacct: listen");
			exit(-1);
		}
		if (daemon(0, 0)) {
			perror("rtacct: daemon");
			exit(-1);
		}
		signal(SIGPIPE, SIG_IGN);
		signal(SIGCHLD, sigchild);
		server_loop(fd);
		exit(0);
	}

	if (getenv("RTACCT_HISTORY"))
		snprintf(hist_name, sizeof(hist_name), "%s", getenv("RTACCT_HISTORY"));
	else
		sprintf(hist_name, "/tmp/.rtacct.u%d", getuid());

	if (reset_history)
		unlink(hist_name);

	if (!ignore_history || !no_update) {
		struct stat stb;

		fd = open(hist_name, O_RDWR|O_CREAT|O_NOFOLLOW, 0600);
		if (fd < 0) {
			perror("rtacct: open history file");
			exit(-1);
		}
		if (flock(fd, LOCK_EX)) {
			perror("rtacct: flock history file");
			exit(-1);
		}
		if (fstat(fd, &stb) != 0) {
			perror("rtacct: fstat history file");
			exit(-1);
		}
		if (stb.st_nlink != 1 || stb.st_uid != getuid()) {
			fprintf(stderr, "rtacct: something is so wrong with history file, that I prefer not to proceed.\n");
			exit(-1);
		}
		if (stb.st_size != sizeof(*hist_db))
			if (write(fd, kern_db, sizeof(*hist_db)) < 0) {
				perror("rtacct: write history file");
				exit(-1);
			}

		hist_db = mmap(NULL, sizeof(*hist_db),
			       PROT_READ|PROT_WRITE,
			       no_update ? MAP_PRIVATE : MAP_SHARED,
			       fd, 0);

		if ((unsigned long)hist_db == ~0UL) {
			perror("mmap");
			exit(-1);
		}

		if (!ignore_history) {
			FILE *tfp;
			long uptime = -1;
			if ((tfp = fopen("/proc/uptime", "r")) != NULL) {
				if (fscanf(tfp, "%ld", &uptime) != 1)
					uptime = -1;
				fclose(tfp);
			}

			if (uptime >= 0 && time(NULL) >= stb.st_mtime+uptime) {
				fprintf(stderr, "rtacct: history is aged out, resetting\n");
				memset(hist_db, 0, sizeof(*hist_db));
			}
		}

		close(fd);
	}

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0 &&
	    (connect(fd, (struct sockaddr*)&sun, 2+1+strlen(sun.sun_path+1)) == 0
	     || (strcpy(sun.sun_path+1, "rtacct0"),
		 connect(fd, (struct sockaddr*)&sun, 2+1+strlen(sun.sun_path+1)) == 0))
	    && verify_forging(fd) == 0) {
		nread(fd, (char*)kern_db, sizeof(*kern_db));
		if (hist_db && hist_db->signature[0] &&
		    strcmp(kern_db->signature, hist_db->signature)) {
			fprintf(stderr, "rtacct: history is stale, ignoring it.\n");
			hist_db = NULL;
		}
		close(fd);
	} else {
		if (fd >= 0)
			close(fd);

		if (hist_db && hist_db->signature[0] &&
		    strcmp(hist_db->signature, "kernel")) {
			fprintf(stderr, "rtacct: history is stale, ignoring it.\n");
			hist_db = NULL;
		}

		pad_kern_table(kern_db, read_kern_table(kern_db->ival));
		strcpy(kern_db->signature, "kernel");
	}

	if (ignore_history || hist_db == NULL)
		dump_abs_db(stdout);
	else
		dump_incr_db(stdout);

	exit(0);
}
