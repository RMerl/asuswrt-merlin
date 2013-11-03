#include "ping_common.h"
#include <ctype.h>
#include <sched.h>

int options;

int mark;
int sndbuf;
int ttl;
int rtt;
int rtt_addend;
__u16 acked;

int mx_dup_ck = MAX_DUP_CHK;
char rcvd_tbl[MAX_DUP_CHK / 8];


/* counters */
long npackets;			/* max packets to transmit */
long nreceived;			/* # of packets we got back */
long nrepeats;			/* number of duplicates */
long ntransmitted;		/* sequence # for outbound packets = #sent */
long nchecksum;			/* replies with bad checksum */
long nerrors;			/* icmp errors */
int interval = 1000;		/* interval between packets (msec) */
int preload;
int deadline = 0;		/* time to die */
int lingertime = MAXWAIT*1000;
struct timeval start_time, cur_time;
volatile int exiting;
volatile int status_snapshot;
int confirm = 0;

/* Stupid workarounds for bugs/missing functionality in older linuces.
 * confirm_flag fixes refusing service of kernels without MSG_CONFIRM.
 * i.e. for linux-2.2 */
int confirm_flag = MSG_CONFIRM;
int working_recverr;

/* timing */
int timing;			/* flag to do timing */
long tmin = LONG_MAX;		/* minimum round trip time */
long tmax;			/* maximum round trip time */
/* Message for rpm maintainers: have _shame_. If you want
 * to fix something send the patch to me for sanity checking.
 * "sparcfix" patch is a complete non-sense, apparenly the person
 * prepared it was stoned.
 */
long long tsum;			/* sum of all times, for doing average */
long long tsum2;
int  pipesize = -1;

int datalen = DEFDATALEN;

char *hostname;
int uid;
int ident;			/* process id to identify our packets */

static int screen_width = INT_MAX;

/* Fills all the outpack, excluding ICMP header, but _including_
 * timestamp area with supplied pattern.
 */
static void fill(char *patp)
{
	int ii, jj, kk;
	int pat[16];
	char *cp;
	u_char *bp = outpack+8;

	for (cp = patp; *cp; cp++) {
		if (!isxdigit(*cp)) {
			fprintf(stderr,
				"ping: patterns must be specified as hex digits.\n");
			exit(2);
		}
	}
	ii = sscanf(patp,
	    "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
	    &pat[0], &pat[1], &pat[2], &pat[3], &pat[4], &pat[5], &pat[6],
	    &pat[7], &pat[8], &pat[9], &pat[10], &pat[11], &pat[12],
	    &pat[13], &pat[14], &pat[15]);

	if (ii > 0) {
		for (kk = 0; kk <= maxpacket - (8 + ii); kk += ii)
			for (jj = 0; jj < ii; ++jj)
				bp[jj + kk] = pat[jj];
	}
	if (!(options & F_QUIET)) {
		printf("PATTERN: 0x");
		for (jj = 0; jj < ii; ++jj)
			printf("%02x", bp[jj] & 0xFF);
		printf("\n");
	}
}

void common_options(int ch)
{
	switch(ch) {
	case 'a':
		options |= F_AUDIBLE;
		break;
	case 'A':
		options |= F_ADAPTIVE;
		break;
	case 'c':
		npackets = atoi(optarg);
		if (npackets <= 0) {
			fprintf(stderr, "ping: bad number of packets to transmit.\n");
			exit(2);
		}
		break;
	case 'd':
		options |= F_SO_DEBUG;
		break;
	case 'D':
		options |= F_PTIMEOFDAY;
		break;
	case 'i':		/* wait between sending packets */
	{
		if (strchr(optarg, '.')) {
			float t;
			if (sscanf(optarg, "%f", &t) != 1) {
				fprintf(stderr, "ping: bad timing interval.\n");
				exit(2);
			}
			interval = (int)(t*1000);
		} else if (sscanf(optarg, "%d", &interval) == 1) {
			interval *= 1000;
		} else {
			fprintf(stderr, "ping: bad timing interval.\n");
			exit(2);
		}

		if (interval < 0) {
			fprintf(stderr, "ping: bad timing interval.\n");
			exit(2);
		}
		options |= F_INTERVAL;
		break;
	}
	case 'm':
	{
		char *endp;
		mark = (int)strtoul(optarg, &endp, 10);
		if (mark < 0 || *endp != '\0') {
			fprintf(stderr, "mark cannot be negative");
			exit(2);
		}
		options |= F_MARK;
		break;
	}
	case 'w':
		deadline = atoi(optarg);
		if (deadline < 0) {
			fprintf(stderr, "ping: bad wait time.\n");
			exit(2);
		}
		break;
	case 'l':
		preload = atoi(optarg);
		if (preload <= 0) {
			fprintf(stderr, "ping: bad preload value, should be 1..%d\n", mx_dup_ck);
			exit(2);
		}
		if (preload > mx_dup_ck)
			preload = mx_dup_ck;
		if (uid && preload > 3) {
			fprintf(stderr, "ping: cannot set preload to value > 3\n");
			exit(2);
		}
		break;
	case 'S':
		sndbuf = atoi(optarg);
		if (sndbuf <= 0) {
			fprintf(stderr, "ping: bad sndbuf value.\n");
			exit(2);
		}
		break;
	case 'f':
		options |= F_FLOOD;
		setbuf(stdout, (char *)NULL);
		/* fallthrough to numeric - avoid gethostbyaddr during flood */
	case 'n':
		options |= F_NUMERIC;
		break;
	case 'p':		/* fill buffer with user pattern */
		options |= F_PINGFILLED;
		fill(optarg);
		break;
	case 'q':
		options |= F_QUIET;
		break;
	case 'r':
		options |= F_SO_DONTROUTE;
		break;
	case 's':		/* size of packet to send */
		datalen = atoi(optarg);
		if (datalen < 0) {
			fprintf(stderr, "ping: illegal negative packet size %d.\n", datalen);
			exit(2);
		}
		if (datalen > maxpacket - 8) {
			fprintf(stderr, "ping: packet size too large: %d\n",
				datalen);
			exit(2);
		}
		break;
	case 'v':
		options |= F_VERBOSE;
		break;
	case 'L':
		options |= F_NOLOOP;
		break;
	case 't':
		options |= F_TTL;
		ttl = atoi(optarg);
		if (ttl < 0 || ttl > 255) {
			fprintf(stderr, "ping: ttl %u out of range\n", ttl);
			exit(2);
		}
		break;
	case 'U':
		options |= F_LATENCY;
		break;
	case 'B':
		options |= F_STRICTSOURCE;
		break;
	case 'W':
		lingertime = atoi(optarg);
		if (lingertime < 0 || lingertime > INT_MAX/1000000) {
			fprintf(stderr, "ping: bad linger time.\n");
			exit(2);
		}
		lingertime *= 1000;
		break;
	case 'V':
		printf("ping utility, iputils-ss%s\n", SNAPSHOT);
		exit(0);
	default:
		abort();
	}
}


static void sigexit(int signo)
{
	exiting = 1;
}

static void sigstatus(int signo)
{
	status_snapshot = 1;
}


int __schedule_exit(int next)
{
	static unsigned long waittime;
	struct itimerval it;

	if (waittime)
		return next;

	if (nreceived) {
		waittime = 2 * tmax;
		if (waittime < 1000*interval)
			waittime = 1000*interval;
	} else
		waittime = lingertime*1000;

	if (next < 0 || next < waittime/1000)
		next = waittime/1000;

	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = 0;
	it.it_value.tv_sec = waittime/1000000;
	it.it_value.tv_usec = waittime%1000000;
	setitimer(ITIMER_REAL, &it, NULL);
	return next;
}

static inline void update_interval(void)
{
	int est = rtt ? rtt/8 : interval*1000;

	interval = (est+rtt_addend+500)/1000;
	if (uid && interval < MINUSERINTERVAL)
		interval = MINUSERINTERVAL;
}

/*
 * Print timestamp
 */
void print_timestamp(void)
{
	if (options & F_PTIMEOFDAY) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		printf("[%lu.%06lu] ",
		       (unsigned long)tv.tv_sec, (unsigned long)tv.tv_usec);
	}
}

/*
 * pinger --
 * 	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
int pinger(void)
{
	static int oom_count;
	static int tokens;
	int i;

	/* Have we already sent enough? If we have, return an arbitrary positive value. */
	if (exiting || (npackets && ntransmitted >= npackets && !deadline))
		return 1000;

	/* Check that packets < rate*time + preload */
	if (cur_time.tv_sec == 0) {
		gettimeofday(&cur_time, NULL);
		tokens = interval*(preload-1);
	} else {
		long ntokens;
		struct timeval tv;

		gettimeofday(&tv, NULL);
		ntokens = (tv.tv_sec - cur_time.tv_sec)*1000 +
			(tv.tv_usec-cur_time.tv_usec)/1000;
		if (!interval) {
			/* Case of unlimited flood is special;
			 * if we see no reply, they are limited to 100pps */
			if (ntokens < MININTERVAL && in_flight() >= preload)
				return MININTERVAL-ntokens;
		}
		ntokens += tokens;
		if (ntokens > interval*preload)
			ntokens = interval*preload;
		if (ntokens < interval)
			return interval - ntokens;

		cur_time = tv;
		tokens = ntokens - interval;
	}

resend:
	i = send_probe();

	if (i == 0) {
		oom_count = 0;
		advance_ntransmitted();
		if (!(options & F_QUIET) && (options & F_FLOOD)) {
			/* Very silly, but without this output with
			 * high preload or pipe size is very confusing. */
			if ((preload < screen_width && pipesize < screen_width) ||
			    in_flight() < screen_width)
				write(STDOUT_FILENO, ".", 1);
		}
		return interval - tokens;
	}

	/* And handle various errors... */
	if (i > 0) {
		/* Apparently, it is some fatal bug. */
		abort();
	} else if (errno == ENOBUFS || errno == ENOMEM) {
		int nores_interval;

		/* Device queue overflow or OOM. Packet is not sent. */
		tokens = 0;
		/* Slowdown. This works only in adaptive mode (option -A) */
		rtt_addend += (rtt < 8*50000 ? rtt/8 : 50000);
		if (options&F_ADAPTIVE)
			update_interval();
		nores_interval = SCHINT(interval/2);
		if (nores_interval > 500)
			nores_interval = 500;
		oom_count++;
		if (oom_count*nores_interval < lingertime)
			return nores_interval;
		i = 0;
		/* Fall to hard error. It is to avoid complete deadlock
		 * on stuck output device even when dealine was not requested.
		 * Expected timings are screwed up in any case, but we will
		 * exit some day. :-) */
	} else if (errno == EAGAIN) {
		/* Socket buffer is full. */
		tokens += interval;
		return MININTERVAL;
	} else {
		if ((i=receive_error_msg()) > 0) {
			/* An ICMP error arrived. */
			tokens += interval;
			return MININTERVAL;
		}
		/* Compatibility with old linuces. */
		if (i == 0 && confirm_flag && errno == EINVAL) {
			confirm_flag = 0;
			errno = 0;
		}
		if (!errno)
			goto resend;
	}

	/* Hard local error. Pretend we sent packet. */
	advance_ntransmitted();

	if (i == 0 && !(options & F_QUIET)) {
		if (options & F_FLOOD)
			write(STDOUT_FILENO, "E", 1);
		else
			perror("ping: sendmsg");
	}
	tokens = 0;
	return SCHINT(interval);
}

/* Set socket buffers, "alloc" is an estimate of memory taken by single packet. */

void sock_setbufs(int icmp_sock, int alloc)
{
	int rcvbuf, hold;
	socklen_t tmplen = sizeof(hold);

	if (!sndbuf)
		sndbuf = alloc;
	setsockopt(icmp_sock, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, sizeof(sndbuf));

	rcvbuf = hold = alloc * preload;
	if (hold < 65536)
		hold = 65536;
	setsockopt(icmp_sock, SOL_SOCKET, SO_RCVBUF, (char *)&hold, sizeof(hold));
	if (getsockopt(icmp_sock, SOL_SOCKET, SO_RCVBUF, (char *)&hold, &tmplen) == 0) {
		if (hold < rcvbuf)
			fprintf(stderr, "WARNING: probably, rcvbuf is not enough to hold preload.\n");
	}
}

/* Protocol independent setup and parameter checks. */

void setup(int icmp_sock)
{
	int hold;
	struct timeval tv;

	if ((options & F_FLOOD) && !(options & F_INTERVAL))
		interval = 0;

	if (uid && interval < MINUSERINTERVAL) {
		fprintf(stderr, "ping: cannot flood; minimal interval, allowed for user, is %dms\n", MINUSERINTERVAL);
		exit(2);
	}

	if (interval >= INT_MAX/preload) {
		fprintf(stderr, "ping: illegal preload and/or interval\n");
		exit(2);
	}

	hold = 1;
	if (options & F_SO_DEBUG)
		setsockopt(icmp_sock, SOL_SOCKET, SO_DEBUG, (char *)&hold, sizeof(hold));
	if (options & F_SO_DONTROUTE)
		setsockopt(icmp_sock, SOL_SOCKET, SO_DONTROUTE, (char *)&hold, sizeof(hold));

#ifdef SO_TIMESTAMP
	if (!(options&F_LATENCY)) {
		int on = 1;
		if (setsockopt(icmp_sock, SOL_SOCKET, SO_TIMESTAMP, &on, sizeof(on)))
			fprintf(stderr, "Warning: no SO_TIMESTAMP support, falling back to SIOCGSTAMP\n");
	}
#endif
	if (options & F_MARK) {
		if (setsockopt(icmp_sock, SOL_SOCKET, SO_MARK,
				&mark, sizeof(mark)) == -1) {
			/* we probably dont wanna exit since old kernels
			 * dont support mark ..
			*/
			fprintf(stderr, "Warning: Failed to set mark %d\n", mark);
		}
	}

	/* Set some SNDTIMEO to prevent blocking forever
	 * on sends, when device is too slow or stalls. Just put limit
	 * of one second, or "interval", if it is less.
	 */
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (interval < 1000) {
		tv.tv_sec = 0;
		tv.tv_usec = 1000 * SCHINT(interval);
	}
	setsockopt(icmp_sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));

	/* Set RCVTIMEO to "interval". Note, it is just an optimization
	 * allowing to avoid redundant poll(). */
	tv.tv_sec = SCHINT(interval)/1000;
	tv.tv_usec = 1000*(SCHINT(interval)%1000);
	if (setsockopt(icmp_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv)))
		options |= F_FLOOD_POLL;

	if (!(options & F_PINGFILLED)) {
		int i;
		u_char *p = outpack+8;

		/* Do not forget about case of small datalen,
		 * fill timestamp area too!
		 */
		for (i = 0; i < datalen; ++i)
			*p++ = i;
	}

	ident = htons(getpid() & 0xFFFF);

	set_signal(SIGINT, sigexit);
	set_signal(SIGALRM, sigexit);
	set_signal(SIGQUIT, sigstatus);

	gettimeofday(&start_time, NULL);

	if (deadline) {
		struct itimerval it;

		it.it_interval.tv_sec = 0;
		it.it_interval.tv_usec = 0;
		it.it_value.tv_sec = deadline;
		it.it_value.tv_usec = 0;
		setitimer(ITIMER_REAL, &it, NULL);
	}

	if (isatty(STDOUT_FILENO)) {
		struct winsize w;

		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1) {
			if (w.ws_col > 0)
				screen_width = w.ws_col;
		}
	}
}

void main_loop(int icmp_sock, __u8 *packet, int packlen)
{
	char addrbuf[128];
	char ans_data[4096];
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *c;
	int cc;
	int next;
	int polling;

	iov.iov_base = (char *)packet;

	for (;;) {
		/* Check exit conditions. */
		if (exiting)
			break;
		if (npackets && nreceived + nerrors >= npackets)
			break;
		if (deadline && nerrors)
			break;
		/* Check for and do special actions. */
		if (status_snapshot)
			status();

		/* Send probes scheduled to this time. */
		do {
			next = pinger();
			next = schedule_exit(next);
		} while (next <= 0);

		/* "next" is time to send next probe, if positive.
		 * If next<=0 send now or as soon as possible. */

		/* Technical part. Looks wicked. Could be dropped,
		 * if everyone used the newest kernel. :-)
		 * Its purpose is:
		 * 1. Provide intervals less than resolution of scheduler.
		 *    Solution: spinning.
		 * 2. Avoid use of poll(), when recvmsg() can provide
		 *    timed waiting (SO_RCVTIMEO). */
		polling = 0;
		if ((options & (F_ADAPTIVE|F_FLOOD_POLL)) || next<SCHINT(interval)) {
			int recv_expected = in_flight();

			/* If we are here, recvmsg() is unable to wait for
			 * required timeout. */
			if (1000*next <= 1000000/(int)HZ) {
				/* Very short timeout... So, if we wait for
				 * something, we sleep for MININTERVAL.
				 * Otherwise, spin! */
				if (recv_expected) {
					next = MININTERVAL;
				} else {
					next = 0;
					/* When spinning, no reasons to poll.
					 * Use nonblocking recvmsg() instead. */
					polling = MSG_DONTWAIT;
					/* But yield yet. */
					sched_yield();
				}
			}

			if (!polling &&
			    ((options & (F_ADAPTIVE|F_FLOOD_POLL)) || interval)) {
				struct pollfd pset;
				pset.fd = icmp_sock;
				pset.events = POLLIN|POLLERR;
				pset.revents = 0;
				if (poll(&pset, 1, next) < 1 ||
				    !(pset.revents&(POLLIN|POLLERR)))
					continue;
				polling = MSG_DONTWAIT;
			}
		}

		for (;;) {
			struct timeval *recv_timep = NULL;
			struct timeval recv_time;
			int not_ours = 0; /* Raw socket can receive messages
					   * destined to other running pings. */

			iov.iov_len = packlen;
			memset(&msg, 0, sizeof(msg));
			msg.msg_name = addrbuf;
			msg.msg_namelen = sizeof(addrbuf);
			msg.msg_iov = &iov;
			msg.msg_iovlen = 1;
			msg.msg_control = ans_data;
			msg.msg_controllen = sizeof(ans_data);

			cc = recvmsg(icmp_sock, &msg, polling);
			polling = MSG_DONTWAIT;

			if (cc < 0) {
				if (errno == EAGAIN || errno == EINTR)
					break;
				if (!receive_error_msg()) {
					if (errno) {
						perror("ping: recvmsg");
						break;
					}
					not_ours = 1;
				}
			} else {

#ifdef SO_TIMESTAMP
				for (c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
					if (c->cmsg_level != SOL_SOCKET ||
					    c->cmsg_type != SO_TIMESTAMP)
						continue;
					if (c->cmsg_len < CMSG_LEN(sizeof(struct timeval)))
						continue;
					recv_timep = (struct timeval*)CMSG_DATA(c);
				}
#endif

				if ((options&F_LATENCY) || recv_timep == NULL) {
					if ((options&F_LATENCY) ||
					    ioctl(icmp_sock, SIOCGSTAMP, &recv_time))
						gettimeofday(&recv_time, NULL);
					recv_timep = &recv_time;
				}

				not_ours = parse_reply(&msg, cc, addrbuf, recv_timep);
			}

			/* See? ... someone runs another ping on this host. */
			if (not_ours)
				install_filter();

			/* If nothing is in flight, "break" returns us to pinger. */
			if (in_flight() == 0)
				break;

			/* Otherwise, try to recvmsg() again. recvmsg()
			 * is nonblocking after the first iteration, so that
			 * if nothing is queued, it will receive EAGAIN
			 * and return to pinger. */
		}
	}
	finish();
}

int gather_statistics(__u8 *icmph, int icmplen,
		      int cc, __u16 seq, int hops,
		      int csfailed, struct timeval *tv, char *from,
		      void (*pr_reply)(__u8 *icmph, int cc))
{
	int dupflag = 0;
	long triptime = 0;
	__u8 *ptr = icmph + icmplen;

	++nreceived;
	if (!csfailed)
		acknowledge(seq);

	if (timing && cc >= 8+sizeof(struct timeval)) {
		struct timeval tmp_tv;
		memcpy(&tmp_tv, ptr, sizeof(tmp_tv));

restamp:
		tvsub(tv, &tmp_tv);
		triptime = tv->tv_sec * 1000000 + tv->tv_usec;
		if (triptime < 0) {
			fprintf(stderr, "Warning: time of day goes back (%ldus), taking countermeasures.\n", triptime);
			triptime = 0;
			if (!(options & F_LATENCY)) {
				gettimeofday(tv, NULL);
				options |= F_LATENCY;
				goto restamp;
			}
		}
		if (!csfailed) {
			tsum += triptime;
			tsum2 += (long long)triptime * (long long)triptime;
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
			if (!rtt)
				rtt = triptime*8;
			else
				rtt += triptime-rtt/8;
			if (options&F_ADAPTIVE)
				update_interval();
		}
	}

	if (csfailed) {
		++nchecksum;
		--nreceived;
	} else if (TST(seq % mx_dup_ck)) {
		++nrepeats;
		--nreceived;
		dupflag = 1;
	} else {
		SET(seq % mx_dup_ck);
		dupflag = 0;
	}
	confirm = confirm_flag;

	if (options & F_QUIET)
		return 1;

	if (options & F_FLOOD) {
		if (!csfailed)
			write(STDOUT_FILENO, "\b \b", 3);
		else
			write(STDOUT_FILENO, "\bC", 1);
	} else {
		int i;
		__u8 *cp, *dp;

		print_timestamp();
		printf("%d bytes from %s:", cc, from);

		if (pr_reply)
			pr_reply(icmph, cc);

		if (hops >= 0)
			printf(" ttl=%d", hops);

		if (cc < datalen+8) {
			printf(" (truncated)\n");
			return 1;
		}
		if (timing) {
			if (triptime >= 100000)
				printf(" time=%ld ms", triptime/1000);
			else if (triptime >= 10000)
				printf(" time=%ld.%01ld ms", triptime/1000,
				       (triptime%1000)/100);
			else if (triptime >= 1000)
				printf(" time=%ld.%02ld ms", triptime/1000,
				       (triptime%1000)/10);
			else
				printf(" time=%ld.%03ld ms", triptime/1000,
				       triptime%1000);
		}
		if (dupflag)
			printf(" (DUP!)");
		if (csfailed)
			printf(" (BAD CHECKSUM!)");

		/* check the data */
		cp = ((u_char*)ptr) + sizeof(struct timeval);
		dp = &outpack[8 + sizeof(struct timeval)];
		for (i = sizeof(struct timeval); i < datalen; ++i, ++cp, ++dp) {
			if (*cp != *dp) {
				printf("\nwrong data byte #%d should be 0x%x but was 0x%x",
				       i, *dp, *cp);
				cp = (u_char*)ptr + sizeof(struct timeval);
				for (i = sizeof(struct timeval); i < datalen; ++i, ++cp) {
					if ((i % 32) == sizeof(struct timeval))
						printf("\n#%d\t", i);
					printf("%x ", *cp);
				}
				break;
			}
		}
	}
	return 0;
}

static long llsqrt(long long a)
{
	long long prev = ~((long long)1 << 63);
	long long x = a;

	if (x > 0) {
		while (x < prev) {
			prev = x;
			x = (x+(a/x))/2;
		}
	}

	return (long)x;
}

/*
 * finish --
 *	Print out statistics, and give up.
 */
void finish(void)
{
	struct timeval tv = cur_time;
	char *comma = "";

	tvsub(&tv, &start_time);

	putchar('\n');
	fflush(stdout);
	printf("--- %s ping statistics ---\n", hostname);
	printf("%ld packets transmitted, ", ntransmitted);
	printf("%ld received", nreceived);
	if (nrepeats)
		printf(", +%ld duplicates", nrepeats);
	if (nchecksum)
		printf(", +%ld corrupted", nchecksum);
	if (nerrors)
		printf(", +%ld errors", nerrors);
	if (ntransmitted) {
		printf(", %d%% packet loss",
		       (int) ((((long long)(ntransmitted - nreceived)) * 100) /
			      ntransmitted));
		printf(", time %ldms", 1000*tv.tv_sec+tv.tv_usec/1000);
	}
	putchar('\n');

	if (nreceived && timing) {
		long tmdev;

		tsum /= nreceived + nrepeats;
		tsum2 /= nreceived + nrepeats;
		tmdev = llsqrt(tsum2 - tsum * tsum);

		printf("rtt min/avg/max/mdev = %ld.%03ld/%lu.%03ld/%ld.%03ld/%ld.%03ld ms",
		       (long)tmin/1000, (long)tmin%1000,
		       (unsigned long)(tsum/1000), (long)(tsum%1000),
		       (long)tmax/1000, (long)tmax%1000,
		       (long)tmdev/1000, (long)tmdev%1000
		       );
		comma = ", ";
	}
	if (pipesize > 1) {
		printf("%spipe %d", comma, pipesize);
		comma = ", ";
	}
	if (ntransmitted > 1 && (!interval || (options&(F_FLOOD|F_ADAPTIVE)))) {
		int ipg = (1000000*(long long)tv.tv_sec+tv.tv_usec)/(ntransmitted-1);
		printf("%sipg/ewma %d.%03d/%d.%03d ms",
		       comma, ipg/1000, ipg%1000, rtt/8000, (rtt/8)%1000);
	}
	putchar('\n');
	exit(!nreceived || (deadline && nreceived < npackets));
}


void status(void)
{
	int loss = 0;
	long tavg = 0;

	status_snapshot = 0;

	if (ntransmitted)
		loss = (((long long)(ntransmitted - nreceived)) * 100) / ntransmitted;

	fprintf(stderr, "\r%ld/%ld packets, %d%% loss", ntransmitted, nreceived, loss);

	if (nreceived && timing) {
		tavg = tsum / (nreceived + nrepeats);

		fprintf(stderr, ", min/avg/ewma/max = %ld.%03ld/%lu.%03ld/%d.%03d/%ld.%03ld ms",
		       (long)tmin/1000, (long)tmin%1000,
		       tavg/1000, tavg%1000,
		       rtt/8000, (rtt/8)%1000,
		       (long)tmax/1000, (long)tmax%1000
		       );
	}
	fprintf(stderr, "\n");
}
