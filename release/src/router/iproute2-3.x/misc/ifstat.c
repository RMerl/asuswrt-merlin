/*
 * ifstat.c	handy utility to read net interface statistics
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
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
#include <signal.h>
#include <math.h>
#include <getopt.h>

#include <libnetlink.h>
#include <linux/if.h>
#include <linux/if_link.h>

#include <SNAPSHOT.h>

int dump_zeros = 0;
int reset_history = 0;
int ignore_history = 0;
int no_output = 0;
int no_update = 0;
int scan_interval = 0;
int time_constant = 0;
int show_errors = 0;
double W;
char **patterns;
int npatterns;

char info_source[128];
int source_mismatch;

#define MAXS (sizeof(struct rtnl_link_stats)/sizeof(__u32))

struct ifstat_ent
{
	struct ifstat_ent	*next;
	char			*name;
	int			ifindex;
	unsigned long long	val[MAXS];
	double			rate[MAXS];
	__u32			ival[MAXS];
};

struct ifstat_ent *kern_db;
struct ifstat_ent *hist_db;

static int match(const char *id)
{
	int i;

	if (npatterns == 0)
		return 1;

	for (i=0; i<npatterns; i++) {
		if (!fnmatch(patterns[i], id, 0))
			return 1;
	}
	return 0;
}

static int get_nlmsg(const struct sockaddr_nl *who,
		     struct nlmsghdr *m, void *arg)
{
	struct ifinfomsg *ifi = NLMSG_DATA(m);
	struct rtattr * tb[IFLA_MAX+1];
	int len = m->nlmsg_len;
	struct ifstat_ent *n;
	int i;

	if (m->nlmsg_type != RTM_NEWLINK)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0)
		return -1;

	if (!(ifi->ifi_flags&IFF_UP))
		return 0;

	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);
	if (tb[IFLA_IFNAME] == NULL || tb[IFLA_STATS] == NULL)
		return 0;

	n = malloc(sizeof(*n));
	if (!n)
		abort();
	n->ifindex = ifi->ifi_index;
	n->name = strdup(RTA_DATA(tb[IFLA_IFNAME]));
	memcpy(&n->ival, RTA_DATA(tb[IFLA_STATS]), sizeof(n->ival));
	memset(&n->rate, 0, sizeof(n->rate));
	for (i=0; i<MAXS; i++)
		n->val[i] = n->ival[i];
	n->next = kern_db;
	kern_db = n;
	return 0;
}

void load_info(void)
{
	struct ifstat_ent *db, *n;
	struct rtnl_handle rth;

	if (rtnl_open(&rth, 0) < 0)
		exit(1);

	if (rtnl_wilddump_request(&rth, AF_INET, RTM_GETLINK) < 0) {
		perror("Cannot send dump request");
		exit(1);
	}

	if (rtnl_dump_filter(&rth, get_nlmsg, NULL, NULL, NULL) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}

	rtnl_close(&rth);

	db = kern_db;
	kern_db = NULL;

	while (db) {
		n = db;
		db = db->next;
		n->next = kern_db;
		kern_db = n;
	}
}

void load_raw_table(FILE *fp)
{
	char buf[4096];
	struct ifstat_ent *db = NULL;
	struct ifstat_ent *n;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char *p;
		char *next;
		int i;

		if (buf[0] == '#') {
			buf[strlen(buf)-1] = 0;
			if (info_source[0] && strcmp(info_source, buf+1))
				source_mismatch = 1;
			strncpy(info_source, buf+1, sizeof(info_source)-1);
			continue;
		}
		if ((n = malloc(sizeof(*n))) == NULL)
			abort();

		if (!(p = strchr(buf, ' ')))
			abort();
		*p++ = 0;

		if (sscanf(buf, "%d", &n->ifindex) != 1)
			abort();
		if (!(next = strchr(p, ' ')))
			abort();
		*next++ = 0;

		n->name = strdup(p);
		p = next;

		for (i=0; i<MAXS; i++) {
			unsigned rate;
			if (!(next = strchr(p, ' ')))
				abort();
			*next++ = 0;
			if (sscanf(p, "%llu", n->val+i) != 1)
				abort();
			n->ival[i] = (__u32)n->val[i];
			p = next;
			if (!(next = strchr(p, ' ')))
				abort();
			*next++ = 0;
			if (sscanf(p, "%u", &rate) != 1)
				abort();
			n->rate[i] = rate;
			p = next;
		}
		n->next = db;
		db = n;
	}

	while (db) {
		n = db;
		db = db->next;
		n->next = kern_db;
		kern_db = n;
	}
}

void dump_raw_db(FILE *fp, int to_hist)
{
	struct ifstat_ent *n, *h;
	h = hist_db;
	fprintf(fp, "#%s\n", info_source);

	for (n=kern_db; n; n=n->next) {
		int i;
		unsigned long long *vals = n->val;
		double *rates = n->rate;
		if (!match(n->name)) {
			struct ifstat_ent *h1;
			if (!to_hist)
				continue;
			for (h1 = h; h1; h1 = h1->next) {
				if (h1->ifindex == n->ifindex) {
					vals = h1->val;
					rates = h1->rate;
					h = h1->next;
					break;
				}
			}
		}
		fprintf(fp, "%d %s ", n->ifindex, n->name);
		for (i=0; i<MAXS; i++)
			fprintf(fp, "%llu %u ", vals[i], (unsigned)rates[i]);
		fprintf(fp, "\n");
	}
}

/* use communication definitions of meg/kilo etc */
static const unsigned long long giga = 1000000000ull;
static const unsigned long long mega = 1000000;
static const unsigned long long kilo = 1000;

void format_rate(FILE *fp, unsigned long long *vals, double *rates, int i)
{
	char temp[64];
	if (vals[i] > giga)
		fprintf(fp, "%7lluM ", vals[i]/mega);
	else if (vals[i] > mega)
		fprintf(fp, "%7lluK ", vals[i]/kilo);
	else
		fprintf(fp, "%8llu ", vals[i]);

	if (rates[i] > mega) {
		sprintf(temp, "%uM", (unsigned)(rates[i]/mega));
		fprintf(fp, "%-6s ", temp);
	} else if (rates[i] > kilo) {
		sprintf(temp, "%uK", (unsigned)(rates[i]/kilo));
		fprintf(fp, "%-6s ", temp);
	} else
		fprintf(fp, "%-6u ", (unsigned)rates[i]);
}

void format_pair(FILE *fp, unsigned long long *vals, int i, int k)
{
	char temp[64];
	if (vals[i] > giga)
		fprintf(fp, "%7lluM ", vals[i]/mega);
	else if (vals[i] > mega)
		fprintf(fp, "%7lluK ", vals[i]/kilo);
	else
		fprintf(fp, "%8llu ", vals[i]);

	if (vals[k] > giga) {
		sprintf(temp, "%uM", (unsigned)(vals[k]/mega));
		fprintf(fp, "%-6s ", temp);
	} else if (vals[k] > mega) {
		sprintf(temp, "%uK", (unsigned)(vals[k]/kilo));
		fprintf(fp, "%-6s ", temp);
	} else
		fprintf(fp, "%-6u ", (unsigned)vals[k]);
}

void print_head(FILE *fp)
{
	fprintf(fp, "#%s\n", info_source);
	fprintf(fp, "%-15s ", "Interface");

	fprintf(fp, "%8s/%-6s ", "RX Pkts", "Rate");
	fprintf(fp, "%8s/%-6s ", "TX Pkts", "Rate");
	fprintf(fp, "%8s/%-6s ", "RX Data", "Rate");
	fprintf(fp, "%8s/%-6s\n","TX Data", "Rate");

	if (!show_errors) {
		fprintf(fp, "%-15s ", "");
		fprintf(fp, "%8s/%-6s ", "RX Errs", "Drop");
		fprintf(fp, "%8s/%-6s ", "TX Errs", "Drop");
		fprintf(fp, "%8s/%-6s ", "RX Over", "Rate");
		fprintf(fp, "%8s/%-6s\n","TX Coll", "Rate");
	} else {
		fprintf(fp, "%-15s ", "");
		fprintf(fp, "%8s/%-6s ", "RX Errs", "Rate");
		fprintf(fp, "%8s/%-6s ", "RX Drop", "Rate");
		fprintf(fp, "%8s/%-6s ", "RX Over", "Rate");
		fprintf(fp, "%8s/%-6s\n","RX Leng", "Rate");

		fprintf(fp, "%-15s ", "");
		fprintf(fp, "%8s/%-6s ", "RX Crc", "Rate");
		fprintf(fp, "%8s/%-6s ", "RX Frm", "Rate");
		fprintf(fp, "%8s/%-6s ", "RX Fifo", "Rate");
		fprintf(fp, "%8s/%-6s\n","RX Miss", "Rate");

		fprintf(fp, "%-15s ", "");
		fprintf(fp, "%8s/%-6s ", "TX Errs", "Rate");
		fprintf(fp, "%8s/%-6s ", "TX Drop", "Rate");
		fprintf(fp, "%8s/%-6s ", "TX Coll", "Rate");
		fprintf(fp, "%8s/%-6s\n","TX Carr", "Rate");

		fprintf(fp, "%-15s ", "");
		fprintf(fp, "%8s/%-6s ", "TX Abrt", "Rate");
		fprintf(fp, "%8s/%-6s ", "TX Fifo", "Rate");
		fprintf(fp, "%8s/%-6s ", "TX Hear", "Rate");
		fprintf(fp, "%8s/%-6s\n","TX Wind", "Rate");
	}
}

void print_one_if(FILE *fp, struct ifstat_ent *n, unsigned long long *vals)
{
	int i;
	fprintf(fp, "%-15s ", n->name);
	for (i=0; i<4; i++)
		format_rate(fp, vals, n->rate, i);
	fprintf(fp, "\n");

	if (!show_errors) {
		fprintf(fp, "%-15s ", "");
		format_pair(fp, vals, 4, 6);
		format_pair(fp, vals, 5, 7);
		format_rate(fp, vals, n->rate, 11);
		format_rate(fp, vals, n->rate, 9);
		fprintf(fp, "\n");
	} else {
		fprintf(fp, "%-15s ", "");
		format_rate(fp, vals, n->rate, 4);
		format_rate(fp, vals, n->rate, 6);
		format_rate(fp, vals, n->rate, 11);
		format_rate(fp, vals, n->rate, 10);
		fprintf(fp, "\n");

		fprintf(fp, "%-15s ", "");
		format_rate(fp, vals, n->rate, 12);
		format_rate(fp, vals, n->rate, 13);
		format_rate(fp, vals, n->rate, 14);
		format_rate(fp, vals, n->rate, 15);
		fprintf(fp, "\n");

		fprintf(fp, "%-15s ", "");
		format_rate(fp, vals, n->rate, 5);
		format_rate(fp, vals, n->rate, 7);
		format_rate(fp, vals, n->rate, 9);
		format_rate(fp, vals, n->rate, 17);
		fprintf(fp, "\n");

		fprintf(fp, "%-15s ", "");
		format_rate(fp, vals, n->rate, 16);
		format_rate(fp, vals, n->rate, 18);
		format_rate(fp, vals, n->rate, 19);
		format_rate(fp, vals, n->rate, 20);
		fprintf(fp, "\n");
	}
}


void dump_kern_db(FILE *fp)
{
	struct ifstat_ent *n;

	print_head(fp);

	for (n=kern_db; n; n=n->next) {
		if (!match(n->name))
			continue;
		print_one_if(fp, n, n->val);
	}
}


void dump_incr_db(FILE *fp)
{
	struct ifstat_ent *n, *h;
	h = hist_db;

	print_head(fp);

	for (n=kern_db; n; n=n->next) {
		int i;
		unsigned long long vals[MAXS];
		struct ifstat_ent *h1;

		memcpy(vals, n->val, sizeof(vals));

		for (h1 = h; h1; h1 = h1->next) {
			if (h1->ifindex == n->ifindex) {
				for (i = 0; i < MAXS; i++)
					vals[i] -= h1->val[i];
				h = h1->next;
				break;
			}
		}
		if (!match(n->name))
			continue;
		print_one_if(fp, n, vals);
	}
}


static int children;

void sigchild(int signo)
{
}

void update_db(int interval)
{
	struct ifstat_ent *n, *h;

	n = kern_db;
	kern_db = NULL;

	load_info();

	h = kern_db;
	kern_db = n;

	for (n = kern_db; n; n = n->next) {
		struct ifstat_ent *h1;
		for (h1 = h; h1; h1 = h1->next) {
			if (h1->ifindex == n->ifindex) {
				int i;
				for (i = 0; i < MAXS; i++) {
					if ((long)(h1->ival[i] - n->ival[i]) < 0) {
						memset(n->ival, 0, sizeof(n->ival));
						break;
					}
				}
				for (i = 0; i < MAXS; i++) {
					double sample;
					unsigned long incr = h1->ival[i] - n->ival[i];
					n->val[i] += incr;
					n->ival[i] = h1->ival[i];
					sample = (double)(incr*1000)/interval;
					if (interval >= scan_interval) {
						n->rate[i] += W*(sample-n->rate[i]);
					} else if (interval >= 1000) {
						if (interval >= time_constant) {
							n->rate[i] = sample;
						} else {
							double w = W*(double)interval/scan_interval;
							n->rate[i] += w*(sample-n->rate[i]);
						}
					}
				}

				while (h != h1) {
					struct ifstat_ent *tmp = h;
					h = h->next;
					free(tmp->name);
					free(tmp);
				};
				h = h1->next;
				free(h1->name);
				free(h1);
				break;
			}
		}
	}
}

#define T_DIFF(a,b) (((a).tv_sec-(b).tv_sec)*1000 + ((a).tv_usec-(b).tv_usec)/1000)


void server_loop(int fd)
{
	struct timeval snaptime = { 0 };
	struct pollfd p;
	p.fd = fd;
	p.events = p.revents = POLLIN;

	sprintf(info_source, "%d.%lu sampling_interval=%d time_const=%d",
		getpid(), (unsigned long)random(), scan_interval/1000, time_constant/1000);

	load_info();

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
					FILE *fp = fdopen(clnt, "w");
					if (fp) {
						if (tdiff > 0)
							update_db(tdiff);
						dump_raw_db(fp, 0);
					}
					exit(0);
				}
			}
		}
		while (children && waitpid(-1, &status, WNOHANG) > 0)
			children--;
	}
}

int verify_forging(int fd)
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
"Usage: ifstat [OPTION] [ PATTERN [ PATTERN ] ]\n"
"   -h, --help		this message\n"
"   -a, --ignore	ignore history\n"
"   -d, --scan=SECS	sample every statistics every SECS\n"
"   -e, --errors	show errors\n"
"   -n, --nooutput	do history only\n"
"   -r, --reset		reset history\n"
"   -s, --noupdate	don;t update history\n"
"   -t, --interval=SECS	report average over the last SECS\n"
"   -V, --version	output version information\n"
"   -z, --zeros		show entries with zero activity\n");

	exit(-1);
}

static const struct option longopts[] = {
	{ "help", 0, 0, 'h' },
	{ "ignore",  0,  0, 'a' },
	{ "scan", 1, 0, 'd'},
	{ "errors", 0, 0, 'e' },
	{ "nooutput", 0, 0, 'n' },
	{ "reset", 0, 0, 'r' },
	{ "noupdate", 0, 0, 's' },
	{ "interval", 1, 0, 't' },
	{ "version", 0, 0, 'V' },
	{ "zeros", 0, 0, 'z' },
	{ 0 }
};

int main(int argc, char *argv[])
{
	char hist_name[128];
	struct sockaddr_un sun;
	FILE *hist_fp = NULL;
	int ch;
	int fd;

	while ((ch = getopt_long(argc, argv, "hvVzrnasd:t:eK",
			longopts, NULL)) != EOF) {
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
		case 'e':
			show_errors = 1;
			break;
		case 'd':
			scan_interval = atoi(optarg) * 1000;
			if (scan_interval <= 0) {
				fprintf(stderr, "ifstat: invalid scan interval\n");
				exit(-1);
			}
			break;
		case 't':
			time_constant = atoi(optarg);
			if (time_constant <= 0) {
				fprintf(stderr, "ifstat: invalid time constant divisor\n");
				exit(-1);
			}
			break;
		case 'v':
		case 'V':
			printf("ifstat utility, iproute2-ss%s\n", SNAPSHOT);
			exit(0);
		case 'h':
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	sun.sun_family = AF_UNIX;
	sun.sun_path[0] = 0;
	sprintf(sun.sun_path+1, "ifstat%d", getuid());

	if (scan_interval > 0) {
		if (time_constant == 0)
			time_constant = 60;
		time_constant *= 1000;
		W = 1 - 1/exp(log(10)*(double)scan_interval/time_constant);
		if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
			perror("ifstat: socket");
			exit(-1);
		}
		if (bind(fd, (struct sockaddr*)&sun, 2+1+strlen(sun.sun_path+1)) < 0) {
			perror("ifstat: bind");
			exit(-1);
		}
		if (listen(fd, 5) < 0) {
			perror("ifstat: listen");
			exit(-1);
		}
		if (daemon(0, 0)) {
			perror("ifstat: daemon");
			exit(-1);
		}
		signal(SIGPIPE, SIG_IGN);
		signal(SIGCHLD, sigchild);
		server_loop(fd);
		exit(0);
	}

	patterns = argv;
	npatterns = argc;

	if (getenv("IFSTAT_HISTORY"))
		snprintf(hist_name, sizeof(hist_name),
			 "%s", getenv("IFSTAT_HISTORY"));
	else
		snprintf(hist_name, sizeof(hist_name),
			 "%s/.ifstat.u%d", P_tmpdir, getuid());

	if (reset_history)
		unlink(hist_name);

	if (!ignore_history || !no_update) {
		struct stat stb;

		fd = open(hist_name, O_RDWR|O_CREAT|O_NOFOLLOW, 0600);
		if (fd < 0) {
			perror("ifstat: open history file");
			exit(-1);
		}
		if ((hist_fp = fdopen(fd, "r+")) == NULL) {
			perror("ifstat: fdopen history file");
			exit(-1);
		}
		if (flock(fileno(hist_fp), LOCK_EX)) {
			perror("ifstat: flock history file");
			exit(-1);
		}
		if (fstat(fileno(hist_fp), &stb) != 0) {
			perror("ifstat: fstat history file");
			exit(-1);
		}
		if (stb.st_nlink != 1 || stb.st_uid != getuid()) {
			fprintf(stderr, "ifstat: something is so wrong with history file, that I prefer not to proceed.\n");
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
				fprintf(stderr, "ifstat: history is aged out, resetting\n");
				ftruncate(fileno(hist_fp), 0);
			}
		}

		load_raw_table(hist_fp);

		hist_db = kern_db;
		kern_db = NULL;
	}

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0 &&
	    (connect(fd, (struct sockaddr*)&sun, 2+1+strlen(sun.sun_path+1)) == 0
	     || (strcpy(sun.sun_path+1, "ifstat0"),
		 connect(fd, (struct sockaddr*)&sun, 2+1+strlen(sun.sun_path+1)) == 0))
	    && verify_forging(fd) == 0) {
		FILE *sfp = fdopen(fd, "r");
		load_raw_table(sfp);
		if (hist_db && source_mismatch) {
			fprintf(stderr, "ifstat: history is stale, ignoring it.\n");
			hist_db = NULL;
		}
		fclose(sfp);
	} else {
		if (fd >= 0)
			close(fd);
		if (hist_db && info_source[0] && strcmp(info_source, "kernel")) {
			fprintf(stderr, "ifstat: history is stale, ignoring it.\n");
			hist_db = NULL;
			info_source[0] = 0;
		}
		load_info();
		if (info_source[0] == 0)
			strcpy(info_source, "kernel");
	}

	if (!no_output) {
		if (ignore_history || hist_db == NULL)
			dump_kern_db(stdout);
		else
			dump_incr_db(stdout);
	}
	if (!no_update) {
		ftruncate(fileno(hist_fp), 0);
		rewind(hist_fp);
		dump_raw_db(hist_fp, 1);
		fflush(hist_fp);
	}
	exit(0);
}
