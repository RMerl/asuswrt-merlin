#include <time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#define TSPTYPES
#include <protocols/timed.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/types.h>

void usage(void) __attribute__((noreturn));

#define MAX_HOSTNAMELEN	NI_MAXHOST

/*
 * Checksum routine for Internet Protocol family headers.
 *
 * This routine is very heavily used in the network
 * code and should be modified for each CPU to be as fast as possible.
 *
 * This implementation is TAHOE version.
 */

#undef	ADDCARRY
#define ADDCARRY(sum) { \
	if (sum & 0xffff0000) {	\
		sum &= 0xffff; \
		sum++; \
	} \
}

int in_cksum(u_short *addr, int len)
{
	union word {
		char	c[2];
		u_short	s;
	} u;
	int sum = 0;

	while (len > 0) {
		/*
		 * add by words.
		 */
		while ((len -= 2) >= 0) {
			if ((unsigned long)addr & 0x1) {
				/* word is not aligned */
				u.c[0] = *(char *)addr;
				u.c[1] = *((char *)addr+1);
				sum += u.s;
				addr++;
			} else
				sum += *addr++;
			ADDCARRY(sum);
		}
		if (len == -1)
			/*
			 * Odd number of bytes.
			 */
			u.c[0] = *(u_char *)addr;
	}
	if (len == -1) {
		/* The last mbuf has odd # of bytes. Follow the
		   standard (the odd byte is shifted left by 8 bits) */
		u.c[1] = 0;
		sum += u.s;
		ADDCARRY(sum);
	}
	return (~sum & 0xffff);
}

#define ON		1
#define OFF		0

#define RANGE		1		/* best expected round-trip time, ms */
#define MSGS 		50
#define TRIALS		10

#define GOOD		0
#define UNREACHABLE	2
#define NONSTDTIME	3
#define HOSTDOWN 	0x7fffffff


int interactive = 0;
int id;
int sock;
int sock_raw;
struct sockaddr_in server;
int ip_opt_len = 0;

#define BIASP	 	43199999
#define BIASN		-43200000
#define MODULO	 	86400000
#define PROCESSING_TIME	0 	/* ms. to reduce error in measurement */

#define PACKET_IN	1024

int measure_delta;
int measure_delta1;
static u_short seqno, seqno0, acked;
long rtt = 1000;
long min_rtt;
long rtt_sigma = 0;

/*
 * Measures the differences between machines' clocks using
 * ICMP timestamp messages.
 */
int
measure(struct sockaddr_in * addr)
{
	socklen_t length;
	int msgcount;
	int cc, count;
	fd_set ready;
	long sendtime, recvtime, histime,  histime1;
	long min1, min2, diff;
	long delta1, delta2;
	struct timeval tv1, tout;
	u_char packet[PACKET_IN], opacket[64];
	struct icmphdr *icp = (struct icmphdr *) packet;
	struct icmphdr *oicp = (struct icmphdr *) opacket;
	struct iphdr *ip = (struct iphdr *) packet;

	min1 = min2 = 0x7fffffff;
	min_rtt = 0x7fffffff;
	measure_delta = HOSTDOWN;
	measure_delta1 = HOSTDOWN;

/* empties the icmp input queue */
	FD_ZERO(&ready);

empty:
	tout.tv_sec = tout.tv_usec = 0;
	FD_SET(sock_raw, &ready);
	if (select(FD_SETSIZE, &ready, (fd_set *)0, (fd_set *)0, &tout)) {
		length = sizeof(struct sockaddr_in);
		cc = recvfrom(sock_raw, (char *)packet, PACKET_IN, 0,
		    (struct sockaddr *)NULL, &length);
		if (cc < 0)
			return -1;
		goto empty;
	}

	/*
	 * To measure the difference, select MSGS messages whose round-trip
	 * time is smaller than RANGE if ckrange is 1, otherwise simply
	 * select MSGS messages regardless of round-trip transmission time.
	 * Choose the smallest transmission time in each of the two directions.
	 * Use these two latter quantities to compute the delta between
	 * the two clocks.
	 */

	length = sizeof(struct sockaddr_in);
	oicp->type = ICMP_TIMESTAMP;
	oicp->code = 0;
	oicp->checksum = 0;
	oicp->un.echo.id = id;
	((__u32*)(oicp+1))[0] = 0;
	((__u32*)(oicp+1))[1] = 0;
	((__u32*)(oicp+1))[2] = 0;
	FD_ZERO(&ready);
	msgcount = 0;

	acked = seqno = seqno0 = 0;

	for (msgcount = 0; msgcount < MSGS; ) {

	/*
	 * If no answer is received for TRIALS consecutive times,
	 * the machine is assumed to be down
	 */
		if (seqno - acked > TRIALS)
			return HOSTDOWN;

		oicp->un.echo.sequence = ++seqno;
		oicp->checksum = 0;

		(void)gettimeofday (&tv1, (struct timezone *)0);
		*(__u32*)(oicp+1) = htonl((tv1.tv_sec % (24*60*60)) * 1000
					  + tv1.tv_usec / 1000);
		oicp->checksum = in_cksum((u_short *)oicp, sizeof(*oicp) + 12);

		count = sendto(sock_raw, (char *)opacket, sizeof(*oicp)+12, 0,
			       (struct sockaddr *)addr, sizeof(struct sockaddr_in));

		if (count < 0)
			return UNREACHABLE;

		for (;;) {
			FD_ZERO(&ready);
			FD_SET(sock_raw, &ready);
			{
			  long tmo = rtt + rtt_sigma;
			  tout.tv_sec =  tmo/1000;
			  tout.tv_usec = (tmo - (tmo/1000)*1000)*1000;
			}

			if ((count = select(FD_SETSIZE, &ready, (fd_set *)0,
			    (fd_set *)0, &tout)) <= 0)
				break;

			(void)gettimeofday(&tv1, (struct timezone *)0);
			cc = recvfrom(sock_raw, (char *)packet, PACKET_IN, 0,
			    (struct sockaddr *)NULL, &length);

			if (cc < 0)
				return(-1);

			icp = (struct icmphdr *)(packet + (ip->ihl << 2));
			if( icp->type == ICMP_TIMESTAMPREPLY &&
			    icp->un.echo.id == id && icp->un.echo.sequence >= seqno0 &&
						  icp->un.echo.sequence <= seqno) {
			  if (acked < icp->un.echo.sequence)
			    acked = icp->un.echo.sequence;

			  recvtime = (tv1.tv_sec % (24*60*60)) * 1000 +
				     tv1.tv_usec / 1000;
			  sendtime = ntohl(*(__u32*)(icp+1));
			  diff = recvtime - sendtime;
		/*
		 * diff can be less than 0 aroud midnight
		 */
			  if (diff < 0)
			    continue;
			  rtt = (rtt * 3 + diff)/4;
			  rtt_sigma = (rtt_sigma *3 + abs(diff-rtt))/4;
			  msgcount++;
			  histime = ntohl(((__u32*)(icp+1))[1]);
			  histime1 = ntohl(((__u32*)(icp+1))[2]);
		/*
		 * a hosts using a time format different from
		 * ms. since midnight UT (as per RFC792) should
		 * set the high order bit of the 32-bit time
		 * value it transmits.
		 */
			if ((histime & 0x80000000) != 0)
			  return NONSTDTIME;

			if (interactive) {
			  printf(".");
			  fflush(stdout);
			}

			delta1 = histime - sendtime;
		/*
		 * Handles wrap-around to avoid that around
		 * midnight small time differences appear
		 * enormous. However, the two machine's clocks
		 * must be within 12 hours from each other.
		 */
			if (delta1 < BIASN)
				delta1 += MODULO;
			else if (delta1 > BIASP)
				delta1 -= MODULO;

			delta2 = recvtime - histime;
			if (delta2 < BIASN)
				delta2 += MODULO;
			else if (delta2 > BIASP)
				delta2 -= MODULO;

			if (delta1 < min1)
				min1 = delta1;
			if (delta2 < min2)
				min2 = delta2;
			if (delta1 + delta2 < min_rtt) {
			  min_rtt  = delta1 + delta2;
			  measure_delta1 = (delta1 - delta2)/2 + PROCESSING_TIME;
			}
			if (diff < RANGE) {
				min1 = delta1;
				min2 = delta2;
				goto good_exit;
			}
		      }
		}
	}

good_exit:
	measure_delta = (min1 - min2)/2 + PROCESSING_TIME;
	return GOOD;
}

char *myname, *hisname;

int
measure_opt(struct sockaddr_in * addr)
{
	socklen_t length;
	int msgcount;
	int cc, count;
	fd_set ready;
	long sendtime, recvtime, histime, histime1;
	long min1, min2, diff;
	long delta1, delta2;
	struct timeval tv1, tout;
	u_char packet[PACKET_IN], opacket[64];
	struct icmphdr *icp = (struct icmphdr *) packet;
	struct icmphdr *oicp = (struct icmphdr *) opacket;
	struct iphdr *ip = (struct iphdr *) packet;

	min1 = min2 = 0x7fffffff;
	min_rtt = 0x7fffffff;
	measure_delta = HOSTDOWN;
	measure_delta1 = HOSTDOWN;

/* empties the icmp input queue */
	FD_ZERO(&ready);
empty:
	tout.tv_sec = tout.tv_usec = 0;
	FD_SET(sock_raw, &ready);
	if (select(FD_SETSIZE, &ready, (fd_set *)0, (fd_set *)0, &tout)) {
		length = sizeof(struct sockaddr_in);
		cc = recvfrom(sock_raw, (char *)packet, PACKET_IN, 0,
		    (struct sockaddr *)NULL, &length);
		if (cc < 0)
			return -1;
		goto empty;
	}

	/*
	 * To measure the difference, select MSGS messages whose round-trip
	 * time is smaller than RANGE if ckrange is 1, otherwise simply
	 * select MSGS messages regardless of round-trip transmission time.
	 * Choose the smallest transmission time in each of the two directions.
	 * Use these two latter quantities to compute the delta between
	 * the two clocks.
	 */

	length = sizeof(struct sockaddr_in);
	oicp->type = ICMP_ECHO;
	oicp->code = 0;
	oicp->checksum = 0;
	oicp->un.echo.id = id;
	((__u32*)(oicp+1))[0] = 0;
	((__u32*)(oicp+1))[1] = 0;
	((__u32*)(oicp+1))[2] = 0;

	FD_ZERO(&ready);
	msgcount = 0;

	acked = seqno = seqno0 = 0;

	for (msgcount = 0; msgcount < MSGS; ) {

	/*
	 * If no answer is received for TRIALS consecutive times,
	 * the machine is assumed to be down
	 */
		if ( seqno - acked > TRIALS) {
			errno = EHOSTDOWN;
			return HOSTDOWN;
		}
		oicp->un.echo.sequence = ++seqno;
		oicp->checksum = 0;

		gettimeofday (&tv1, NULL);
		((__u32*)(oicp+1))[0] = htonl((tv1.tv_sec % (24*60*60)) * 1000
					      + tv1.tv_usec / 1000);
		oicp->checksum = in_cksum((u_short *)oicp, sizeof(*oicp)+12);

		count = sendto(sock_raw, (char *)opacket, sizeof(*oicp)+12, 0,
			       (struct sockaddr *)addr, sizeof(struct sockaddr_in));

		if (count < 0) {
			errno = EHOSTUNREACH;
			return UNREACHABLE;
		}

		for (;;) {
			FD_ZERO(&ready);
			FD_SET(sock_raw, &ready);
			{
				long tmo = rtt + rtt_sigma;
				tout.tv_sec =  tmo/1000;
				tout.tv_usec = (tmo - (tmo/1000)*1000)*1000;
			}

			if ((count = select(FD_SETSIZE, &ready, (fd_set *)0,
			    (fd_set *)0, &tout)) <= 0)
				break;

			(void)gettimeofday(&tv1, (struct timezone *)0);
			cc = recvfrom(sock_raw, (char *)packet, PACKET_IN, 0,
				      (struct sockaddr *)NULL, &length);

			if (cc < 0)
				return(-1);

			icp = (struct icmphdr *)(packet + (ip->ihl << 2));
			if (icp->type == ICMP_ECHOREPLY &&
			    packet[20] == IPOPT_TIMESTAMP &&
			    icp->un.echo.id == id &&
			    icp->un.echo.sequence >= seqno0 &&
			    icp->un.echo.sequence <= seqno) {
				int i;
				__u8 *opt = packet+20;

				if (acked < icp->un.echo.sequence)
					acked = icp->un.echo.sequence;
				if ((opt[3]&0xF) != IPOPT_TS_PRESPEC) {
					fprintf(stderr, "Wrong timestamp %d\n", opt[3]&0xF);
					return NONSTDTIME;
				}
				if (opt[3]>>4) {
					if ((opt[3]>>4) != 1 || ip_opt_len != 4+3*8)
						fprintf(stderr, "Overflow %d hops\n", opt[3]>>4);
				}
				sendtime = recvtime = histime = histime1 = 0;
				for (i=0; i < (opt[2]-5)/8; i++) {
					__u32 *timep = (__u32*)(opt+4+i*8+4);
					__u32 t = ntohl(*timep);

					if (t & 0x80000000)
						return NONSTDTIME;

					if (i == 0)
						sendtime = t;
					if (i == 1)
						histime = histime1 = t;
					if (i == 2) {
						if (ip_opt_len == 4+4*8)
							histime1 = t;
						else
							recvtime = t;
					}
					if (i == 3)
						recvtime = t;
				}

				if (!(sendtime&histime&histime1&recvtime)) {
					fprintf(stderr, "wrong timestamps\n");
					return -1;
				}

				diff = recvtime - sendtime;
				/*
				 * diff can be less than 0 aroud midnight
				 */
				if (diff < 0)
					continue;
				rtt = (rtt * 3 + diff)/4;
				rtt_sigma = (rtt_sigma *3 + abs(diff-rtt))/4;
				msgcount++;

				if (interactive) {
					printf(".");
					fflush(stdout);
				}

				delta1 = histime - sendtime;
				/*
				 * Handles wrap-around to avoid that around
				 * midnight small time differences appear
				 * enormous. However, the two machine's clocks
				 * must be within 12 hours from each other.
				 */
				if (delta1 < BIASN)
					delta1 += MODULO;
				else if (delta1 > BIASP)
					delta1 -= MODULO;

				delta2 = recvtime - histime1;
				if (delta2 < BIASN)
					delta2 += MODULO;
				else if (delta2 > BIASP)
					delta2 -= MODULO;

				if (delta1 < min1)
					min1 = delta1;
				if (delta2 < min2)
					min2 = delta2;
				if (delta1 + delta2 < min_rtt) {
					min_rtt  = delta1 + delta2;
					measure_delta1 = (delta1 - delta2)/2 + PROCESSING_TIME;
				}
				if (diff < RANGE) {
					min1 = delta1;
					min2 = delta2;
					goto good_exit;
				}
			}
		}
	}

good_exit:
	measure_delta = (min1 - min2)/2 + PROCESSING_TIME;
	return GOOD;
}


/*
 * Clockdiff computes the difference between the time of the machine on
 * which it is called and the time of the machines given as argument.
 * The time differences measured by clockdiff are obtained using a sequence
 * of ICMP TSTAMP messages which are returned to the sender by the IP module
 * in the remote machine.
 * In order to compare clocks of machines in different time zones, the time
 * is transmitted (as a 32-bit value) in milliseconds since midnight UT.
 * If a hosts uses a different time format, it should set the high order
 * bit of the 32-bit quantity it transmits.
 * However, VMS apparently transmits the time in milliseconds since midnight
 * local time (rather than GMT) without setting the high order bit.
 * Furthermore, it does not understand daylight-saving time.  This makes
 * clockdiff behaving inconsistently with hosts running VMS.
 *
 * In order to reduce the sensitivity to the variance of message transmission
 * time, clockdiff sends a sequence of messages.  Yet, measures between
 * two `distant' hosts can be affected by a small error. The error can, however,
 * be reduced by increasing the number of messages sent in each measurement.
 */

void
usage() {
  fprintf(stderr, "Usage: clockdiff [-o] <host>\n");
  exit(1);
}


int
main(int argc, char *argv[])
{
	int measure_status;
	struct hostent * hp;
	char hostname[MAX_HOSTNAMELEN];
	int s_errno = 0;

	if (argc < 2) {
		if (setuid(getuid())) {
			perror("clockdiff: setuid");
			exit(-1);
		}
		usage();
	}

	sock_raw = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	s_errno = errno;

	if (setuid(getuid())) {
		perror("clockdiff: setuid");
		exit(-1);
	}

	if (argc == 3) {
		if (strcmp(argv[1], "-o") == 0) {
			ip_opt_len = 4 + 4*8;
			argv++;
		} else if (strcmp(argv[1], "-o1") == 0) {
			ip_opt_len = 4 + 3*8;
			argv++;
		} else
			usage();
	} else if (argc != 2)
		usage();

	if (sock_raw < 0)  {
		errno = s_errno;
		perror("clockdiff: socket");
		exit(1);
	}

	if (isatty(fileno(stdin)) && isatty(fileno(stdout)))
		interactive = 1;

	id = getpid();

	(void)gethostname(hostname,sizeof(hostname));
	hp = gethostbyname(hostname);
	if (hp == NULL) {
		fprintf(stderr, "clockdiff: %s: my host not found\n", hostname);
		exit(1);
	}
	myname = strdup(hp->h_name);

	hp = gethostbyname(argv[1]);
	if (hp == NULL) {
		fprintf(stderr, "clockdiff: %s: host not found\n", argv[1]);
		exit(1);
	}
	hisname = strdup(hp->h_name);

	memset(&server, 0, sizeof(server));
	server.sin_family = hp->h_addrtype;
	memcpy(&(server.sin_addr.s_addr), hp->h_addr, 4);

	if (connect(sock_raw, (struct sockaddr*)&server, sizeof(server)) == -1) {
		perror("connect");
		exit(1);
	}
	if (ip_opt_len) {
		struct sockaddr_in myaddr;
		socklen_t addrlen = sizeof(myaddr);
		unsigned char rspace[ip_opt_len];

		memset(rspace, 0, sizeof(rspace));
		rspace[0] = IPOPT_TIMESTAMP;
		rspace[1] = ip_opt_len;
		rspace[2] = 5;
		rspace[3] = IPOPT_TS_PRESPEC;
		if (getsockname(sock_raw, (struct sockaddr*)&myaddr, &addrlen) == -1) {
			perror("getsockname");
			exit(1);
		}
		((__u32*)(rspace+4))[0*2] = myaddr.sin_addr.s_addr;
		((__u32*)(rspace+4))[1*2] = server.sin_addr.s_addr;
		((__u32*)(rspace+4))[2*2] = myaddr.sin_addr.s_addr;
		if (ip_opt_len == 4+4*8) {
			((__u32*)(rspace+4))[2*2] = server.sin_addr.s_addr;
			((__u32*)(rspace+4))[3*2] = myaddr.sin_addr.s_addr;
		}

		if (setsockopt(sock_raw, IPPROTO_IP, IP_OPTIONS, rspace, ip_opt_len) < 0) {
			perror("ping: IP_OPTIONS (fallback to icmp tstamps)");
			ip_opt_len = 0;
		}
	}

	nice(-16);

	if ((measure_status = (ip_opt_len ? measure_opt : measure)(&server)) < 0) {
		if (errno)
			perror("measure");
		else
			fprintf(stderr, "measure: unknown failure\n");
		exit(1);
	}

	switch (measure_status) {
	case HOSTDOWN:
		fprintf(stderr, "%s is down\n", hisname);
		exit(1);
	case NONSTDTIME:
		fprintf(stderr, "%s time transmitted in a non-standard format\n", hisname);
		exit(1);
	case UNREACHABLE:
		fprintf(stderr, "%s is unreachable\n", hisname);
		exit(1);
	default:
		break;
	}


	{
		time_t now = time(NULL);

		if (interactive)
			printf("\nhost=%s rtt=%ld(%ld)ms/%ldms delta=%dms/%dms %s", hisname,
			       rtt, rtt_sigma, min_rtt,
			       measure_delta, measure_delta1,
			       ctime(&now));
		else
			printf("%ld %d %d\n", now, measure_delta, measure_delta1);
	}
	exit(0);
}
