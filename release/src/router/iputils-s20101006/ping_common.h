#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <linux/errqueue.h>

#include "SNAPSHOT.h"

#define	DEFDATALEN	(64 - 8)	/* default data length */

#define	MAXWAIT		10		/* max seconds to wait for response */
#define MININTERVAL	10		/* Minimal interpacket gap */
#define MINUSERINTERVAL	200		/* Minimal allowed interval for non-root */

#define SCHINT(a)	(((a) <= MININTERVAL) ? MININTERVAL : (a))

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

/* various options */
extern int options;
#define	F_FLOOD		0x001
#define	F_INTERVAL	0x002
#define	F_NUMERIC	0x004
#define	F_PINGFILLED	0x008
#define	F_QUIET		0x010
#define	F_RROUTE	0x020
#define	F_SO_DEBUG	0x040
#define	F_SO_DONTROUTE	0x080
#define	F_VERBOSE	0x100
#define	F_TIMESTAMP	0x200
#define	F_FLOWINFO	0x200
#define	F_SOURCEROUTE	0x400
#define	F_TCLASS	0x400
#define	F_FLOOD_POLL	0x800
#define	F_LATENCY	0x1000
#define	F_AUDIBLE	0x2000
#define	F_ADAPTIVE	0x4000
#define	F_STRICTSOURCE	0x8000
#define F_NOLOOP	0x10000
#define F_TTL		0x20000
#define F_MARK		0x40000
#define F_PTIMEOFDAY	0x80000

/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.
 */
#define	MAX_DUP_CHK	0x10000
extern int mx_dup_ck;
extern char rcvd_tbl[MAX_DUP_CHK / 8];


extern u_char outpack[];
extern int maxpacket;

extern int datalen;
extern char *hostname;
extern int uid;
extern int ident;			/* process id to identify our packets */

extern int sndbuf;
extern int ttl;

extern long npackets;			/* max packets to transmit */
extern long nreceived;			/* # of packets we got back */
extern long nrepeats;			/* number of duplicates */
extern long ntransmitted;		/* sequence # for outbound packets = #sent */
extern long nchecksum;			/* replies with bad checksum */
extern long nerrors;			/* icmp errors */
extern int interval;			/* interval between packets (msec) */
extern int preload;
extern int deadline;			/* time to die */
extern int lingertime;
extern struct timeval start_time, cur_time;
extern volatile int exiting;
extern volatile int status_snapshot;
extern int confirm;
extern int confirm_flag;
extern int working_recverr;

#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0
#endif


/* timing */
extern int timing;			/* flag to do timing */
extern long tmin;			/* minimum round trip time */
extern long tmax;			/* maximum round trip time */
extern long long tsum;			/* sum of all times, for doing average */
extern long long tsum2;
extern int rtt;
extern __u16 acked;
extern int pipesize;

#define COMMON_OPTIONS \
case 'a': case 'U': case 'c': case 'd': \
case 'f': case 'i': case 'w': case 'l': \
case 'S': case 'n': case 'p': case 'q': \
case 'r': case 's': case 'v': case 'L': \
case 't': case 'A': case 'W': case 'B': case 'm': \
case 'D':

#define COMMON_OPTSTR "h?VQ:I:M:aUc:dfi:w:l:S:np:qrs:vLt:AW:Bm:D"


/*
 * tvsub --
 *	Subtract 2 timeval structs:  out = out - in.  Out is assumed to
 * be >= in.
 */
static inline void tvsub(struct timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) < 0) {
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

static inline void set_signal(int signo, void (*handler)(int))
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));

	sa.sa_handler = (void (*)(int))handler;
#ifdef SA_INTERRUPT
	sa.sa_flags = SA_INTERRUPT;
#endif
	sigaction(signo, &sa, NULL);
}

extern int __schedule_exit(int next);

static inline int schedule_exit(int next)
{
	if (npackets && ntransmitted >= npackets && !deadline)
		next = __schedule_exit(next);
	return next;
}

static inline int in_flight(void)
{
	__u16 diff = (__u16)ntransmitted - acked;
	return (diff<=0x7FFF) ? diff : ntransmitted-nreceived-nerrors;
}

static inline void acknowledge(__u16 seq)
{
	__u16 diff = (__u16)ntransmitted - seq;
	if (diff <= 0x7FFF) {
		if ((int)diff+1 > pipesize)
			pipesize = (int)diff+1;
		if ((__s16)(seq - acked) > 0 ||
		    (__u16)ntransmitted - acked > 0x7FFF)
			acked = seq;
	}
}

static inline void advance_ntransmitted(void)
{
	ntransmitted++;
	/* Invalidate acked, if 16 bit seq overflows. */
	if ((__u16)ntransmitted - acked > 0x7FFF)
		acked = (__u16)ntransmitted + 1;
}


extern int send_probe(void);
extern int receive_error_msg(void);
extern int parse_reply(struct msghdr *msg, int len, void *addr, struct timeval *);
extern void install_filter(void);

extern int pinger(void);
extern void sock_setbufs(int icmp_sock, int alloc);
extern void setup(int icmp_sock);
extern void main_loop(int icmp_sock, __u8 *buf, int buflen) __attribute__((noreturn));
extern void finish(void) __attribute__((noreturn));
extern void status(void);
extern void common_options(int ch);
extern int gather_statistics(__u8 *ptr, int icmplen,
			     int cc, __u16 seq, int hops,
			     int csfailed, struct timeval *tv, char *from,
			     void (*pr_reply)(__u8 *ptr, int cc));
extern void print_timestamp(void);
