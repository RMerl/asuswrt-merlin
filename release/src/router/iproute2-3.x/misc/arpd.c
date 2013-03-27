/*
 * arpd.c	ARP helper daemon.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 */

#include <stdio.h>
#include <syslog.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <db_185.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/filter.h>

#include "libnetlink.h"
#include "utils.h"

int resolve_hosts;

DB	*dbase;
char	*dbname = "/var/lib/arpd/arpd.db";

int	ifnum;
int	*ifvec;
char	**ifnames;

struct dbkey
{
	__u32	iface;
	__u32	addr;
};

#define IS_NEG(x)	(((__u8*)(x))[0] == 0xFF)
#define NEG_TIME(x)	(((x)[2]<<24)|((x)[3]<<16)|((x)[4]<<8)|(x)[5])
#define NEG_AGE(x)	((__u32)time(NULL) - NEG_TIME((__u8*)x))
#define NEG_VALID(x)	(NEG_AGE(x) < negative_timeout)
#define NEG_CNT(x)	(((__u8*)(x))[1])

struct rtnl_handle rth;

struct pollfd pset[2];
int udp_sock = -1;

volatile int do_exit;
volatile int do_sync;
volatile int do_stats;

struct {
	unsigned long arp_new;
	unsigned long arp_change;

	unsigned long app_recv;
	unsigned long app_success;
	unsigned long app_bad;
	unsigned long app_neg;
	unsigned long app_suppressed;

	unsigned long kern_neg;
	unsigned long kern_new;
	unsigned long kern_change;

	unsigned long probes_sent;
	unsigned long probes_suppressed;
} stats;

int active_probing;
int negative_timeout = 60;
int no_kernel_broadcasts;
int broadcast_rate = 1000;
int broadcast_burst = 3000;

void usage(void)
{
	fprintf(stderr,
"Usage: arpd [ -lkh? ] [ -a N ] [ -b dbase ] [ -B number ] [ -f file ] [ -n time ] [ -R rate ] [ interfaces ]\n");
	exit(1);
}

int handle_if(int ifindex)
{
	int i;

	if (ifnum == 0)
		return 1;

	for (i=0; i<ifnum; i++)
		if (ifvec[i] == ifindex)
			return 1;
	return 0;
}

int sysctl_adjusted;

void do_sysctl_adjustments(void)
{
	int i;

	if (!ifnum)
		return;

	for (i=0; i<ifnum; i++) {
		char buf[128];
		FILE *fp;

		if (active_probing) {
			sprintf(buf, "/proc/sys/net/ipv4/neigh/%s/mcast_solicit", ifnames[i]);
			if ((fp = fopen(buf, "w")) != NULL) {
				if (no_kernel_broadcasts)
					strcpy(buf, "0\n");
				else
					sprintf(buf, "%d\n", active_probing>=2 ? 1 : 3-active_probing);
				fputs(buf, fp);
				fclose(fp);
			}
		}

		sprintf(buf, "/proc/sys/net/ipv4/neigh/%s/app_solicit", ifnames[i]);
		if ((fp = fopen(buf, "w")) != NULL) {
			sprintf(buf, "%d\n", active_probing<=1 ? 1 : active_probing);
			fputs(buf, fp);
			fclose(fp);
		}
	}
	sysctl_adjusted = 1;
}

void undo_sysctl_adjustments(void)
{
	int i;

	if (!sysctl_adjusted)
		return;

	for (i=0; i<ifnum; i++) {
		char buf[128];
		FILE *fp;

		if (active_probing) {
			sprintf(buf, "/proc/sys/net/ipv4/neigh/%s/mcast_solicit", ifnames[i]);
			if ((fp = fopen(buf, "w")) != NULL) {
				strcpy(buf, "3\n");
				fputs(buf, fp);
				fclose(fp);
			}
		}
		sprintf(buf, "/proc/sys/net/ipv4/neigh/%s/app_solicit", ifnames[i]);
		if ((fp = fopen(buf, "w")) != NULL) {
			strcpy(buf, "0\n");
			fputs(buf, fp);
			fclose(fp);
		}
	}
	sysctl_adjusted = 0;
}


int send_probe(int ifindex, __u32 addr)
{
	struct ifreq ifr;
	struct sockaddr_in dst;
	socklen_t len;
	unsigned char buf[256];
	struct arphdr *ah = (struct arphdr*)buf;
	unsigned char *p = (unsigned char *)(ah+1);
	struct sockaddr_ll sll;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = ifindex;
	if (ioctl(udp_sock, SIOCGIFNAME, &ifr))
		return -1;
	if (ioctl(udp_sock, SIOCGIFHWADDR, &ifr))
		return -1;
	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
		return -1;
	if (setsockopt(udp_sock, SOL_SOCKET, SO_BINDTODEVICE, ifr.ifr_name, strlen(ifr.ifr_name)+1) < 0)
		return -1;

	dst.sin_family = AF_INET;
	dst.sin_port = htons(1025);
	dst.sin_addr.s_addr = addr;
	if (connect(udp_sock, (struct sockaddr*)&dst, sizeof(dst)) < 0)
		return -1;
	len = sizeof(dst);
	if (getsockname(udp_sock, (struct sockaddr*)&dst, &len) < 0)
		return -1;

	ah->ar_hrd = htons(ifr.ifr_hwaddr.sa_family);
	ah->ar_pro = htons(ETH_P_IP);
	ah->ar_hln = 6;
	ah->ar_pln = 4;
	ah->ar_op  = htons(ARPOP_REQUEST);

	memcpy(p, ifr.ifr_hwaddr.sa_data, ah->ar_hln);
	p += ah->ar_hln;

	memcpy(p, &dst.sin_addr, 4);
	p+=4;

	sll.sll_family = AF_PACKET;
	memset(sll.sll_addr, 0xFF, sizeof(sll.sll_addr));
	sll.sll_ifindex = ifindex;
	sll.sll_protocol = htons(ETH_P_ARP);
	memcpy(p, &sll.sll_addr, ah->ar_hln);
	p+=ah->ar_hln;

	memcpy(p, &addr, 4);
	p+=4;

	if (sendto(pset[0].fd, buf, p-buf, 0, (struct sockaddr*)&sll, sizeof(sll)) < 0)
		return -1;
	stats.probes_sent++;
	return 0;
}

/* Be very tough on sending probes: 1 per second with burst of 3. */

int queue_active_probe(int ifindex, __u32 addr)
{
	static struct timeval prev;
	static int buckets;
	struct timeval now;

	gettimeofday(&now, NULL);
	if (prev.tv_sec) {
		int diff = (now.tv_sec-prev.tv_sec)*1000+(now.tv_usec-prev.tv_usec)/1000;
		buckets += diff;
	} else {
		buckets = broadcast_burst;
	}
	if (buckets > broadcast_burst)
		buckets = broadcast_burst;
	if (buckets >= broadcast_rate && !send_probe(ifindex, addr)) {
		buckets -= broadcast_rate;
		prev = now;
		return 0;
	}
	stats.probes_suppressed++;
	return -1;
}

int respond_to_kernel(int ifindex, __u32 addr, char *lla, int llalen)
{
	struct {
		struct nlmsghdr 	n;
		struct ndmsg 		ndm;
		char   			buf[256];
	} req;

	memset(&req.n, 0, sizeof(req.n));
	memset(&req.ndm, 0, sizeof(req.ndm));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ndmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_NEWNEIGH;
	req.ndm.ndm_family = AF_INET;
	req.ndm.ndm_state = NUD_STALE;
	req.ndm.ndm_ifindex = ifindex;
	req.ndm.ndm_type = RTN_UNICAST;

	addattr_l(&req.n, sizeof(req), NDA_DST, &addr, 4);
	addattr_l(&req.n, sizeof(req), NDA_LLADDR, lla, llalen);
	return rtnl_send(&rth, (char*)&req, req.n.nlmsg_len) <= 0;
}

void prepare_neg_entry(__u8 *ndata, __u32 stamp)
{
	ndata[0] = 0xFF;
	ndata[1] = 0;
	ndata[2] = stamp>>24;
	ndata[3] = stamp>>16;
	ndata[4] = stamp>>8;
	ndata[5] = stamp;
}


int do_one_request(struct nlmsghdr *n)
{
	struct ndmsg *ndm = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * tb[NDA_MAX+1];
	struct dbkey key;
	DBT dbkey, dbdat;
	int do_acct = 0;

	if (n->nlmsg_type == NLMSG_DONE) {
		dbase->sync(dbase, 0);

		/* Now we have at least mirror of kernel db, so that
		 * may start real resolution.
		 */
		do_sysctl_adjustments();
		return 0;
	}

	if (n->nlmsg_type != RTM_GETNEIGH && n->nlmsg_type != RTM_NEWNEIGH)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*ndm));
	if (len < 0)
		return -1;

	if (ndm->ndm_family != AF_INET ||
	    (ifnum && !handle_if(ndm->ndm_ifindex)) ||
	    ndm->ndm_flags ||
	    ndm->ndm_type != RTN_UNICAST ||
	    !(ndm->ndm_state&~NUD_NOARP))
		return 0;

	parse_rtattr(tb, NDA_MAX, NDA_RTA(ndm), len);

	if (!tb[NDA_DST])
		return 0;

	key.iface = ndm->ndm_ifindex;
	memcpy(&key.addr, RTA_DATA(tb[NDA_DST]), 4);
	dbkey.data = &key;
	dbkey.size = sizeof(key);

	if (dbase->get(dbase, &dbkey, &dbdat, 0) != 0) {
		dbdat.data = 0;
		dbdat.size = 0;
	}

	if (n->nlmsg_type == RTM_GETNEIGH) {
		if (!(n->nlmsg_flags&NLM_F_REQUEST))
			return 0;

		if (!(ndm->ndm_state&(NUD_PROBE|NUD_INCOMPLETE))) {
			stats.app_bad++;
			return 0;
		}

		if (ndm->ndm_state&NUD_PROBE) {
			/* If we get this, kernel still has some valid
			 * address, but unicast probing failed and host
			 * is either dead or changed its mac address.
			 * Kernel is going to initiate broadcast resolution.
			 * OK, we invalidate our information as well.
			 */
			if (dbdat.data && !IS_NEG(dbdat.data))
				stats.app_neg++;

			dbase->del(dbase, &dbkey, 0);
		} else {
			/* If we get this kernel does not have any information.
			 * If we have something tell this to kernel. */
			stats.app_recv++;
			if (dbdat.data && !IS_NEG(dbdat.data)) {
				stats.app_success++;
				respond_to_kernel(key.iface, key.addr, dbdat.data, dbdat.size);
				return 0;
			}

			/* Sheeit! We have nothing to tell. */
			/* If we have recent negative entry, be silent. */
			if (dbdat.data && NEG_VALID(dbdat.data)) {
				if (NEG_CNT(dbdat.data) >= active_probing) {
					stats.app_suppressed++;
					return 0;
				}
				do_acct = 1;
			}
		}

		if (active_probing &&
		    queue_active_probe(ndm->ndm_ifindex, key.addr) == 0 &&
		    do_acct) {
			NEG_CNT(dbdat.data)++;
			dbase->put(dbase, &dbkey, &dbdat, 0);
		}
	} else if (n->nlmsg_type == RTM_NEWNEIGH) {
		if (n->nlmsg_flags&NLM_F_REQUEST)
			return 0;

		if (ndm->ndm_state&NUD_FAILED) {
			/* Kernel was not able to resolve. Host is dead.
			 * Create negative entry if it is not present
			 * or renew it if it is too old. */
			if (!dbdat.data ||
			    !IS_NEG(dbdat.data) ||
			    !NEG_VALID(dbdat.data)) {
				__u8 ndata[6];
				stats.kern_neg++;
				prepare_neg_entry(ndata, time(NULL));
				dbdat.data = ndata;
				dbdat.size = sizeof(ndata);
				dbase->put(dbase, &dbkey, &dbdat, 0);
			}
		} else if (tb[NDA_LLADDR]) {
			if (dbdat.data && !IS_NEG(dbdat.data)) {
				if (memcmp(RTA_DATA(tb[NDA_LLADDR]), dbdat.data, dbdat.size) == 0)
					return 0;
				stats.kern_change++;
			} else {
				stats.kern_new++;
			}
			dbdat.data = RTA_DATA(tb[NDA_LLADDR]);
			dbdat.size = RTA_PAYLOAD(tb[NDA_LLADDR]);
			dbase->put(dbase, &dbkey, &dbdat, 0);
		}
	}
	return 0;
}

void load_initial_table(void)
{
	rtnl_wilddump_request(&rth, AF_INET, RTM_GETNEIGH);
}

void get_kern_msg(void)
{
	int status;
	struct nlmsghdr *h;
	struct sockaddr_nl nladdr;
	struct iovec iov;
	char   buf[8192];
	struct msghdr msg = {
		(void*)&nladdr, sizeof(nladdr),
		&iov,	1,
		NULL,	0,
		0
	};

	memset(&nladdr, 0, sizeof(nladdr));

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	status = recvmsg(rth.fd, &msg, MSG_DONTWAIT);

	if (status <= 0)
		return;

	if (msg.msg_namelen != sizeof(nladdr))
		return;

	if (nladdr.nl_pid)
		return;

	for (h = (struct nlmsghdr*)buf; status >= sizeof(*h); ) {
		int len = h->nlmsg_len;
		int l = len - sizeof(*h);

		if (l < 0 || len > status)
			return;

		if (do_one_request(h) < 0)
			return;

		status -= NLMSG_ALIGN(len);
		h = (struct nlmsghdr*)((char*)h + NLMSG_ALIGN(len));
	}
}

/* Receive gratuitous ARP messages and store them, that's all. */
void get_arp_pkt(void)
{
	unsigned char buf[1024];
	struct sockaddr_ll sll;
	socklen_t sll_len = sizeof(sll);
	struct arphdr *a = (struct arphdr*)buf;
	struct dbkey key;
	DBT dbkey, dbdat;
	int n;

	n = recvfrom(pset[0].fd, buf, sizeof(buf), MSG_DONTWAIT,
		     (struct sockaddr*)&sll, &sll_len);
	if (n < 0) {
		if (errno != EINTR && errno != EAGAIN)
			syslog(LOG_ERR, "recvfrom: %m");
		return;
	}

	if (ifnum && !handle_if(sll.sll_ifindex))
		return;

	/* Sanity checks */

	if (n < sizeof(*a) ||
	    (a->ar_op != htons(ARPOP_REQUEST) &&
	     a->ar_op != htons(ARPOP_REPLY)) ||
	    a->ar_pln != 4 ||
	    a->ar_pro != htons(ETH_P_IP) ||
	    a->ar_hln != sll.sll_halen ||
	    sizeof(*a) + 2*4 + 2*a->ar_hln > n)
		return;

	key.iface = sll.sll_ifindex;
	memcpy(&key.addr, (char*)(a+1) + a->ar_hln, 4);

	/* DAD message, ignore. */
	if (key.addr == 0)
		return;

	dbkey.data = &key;
	dbkey.size = sizeof(key);

	if (dbase->get(dbase, &dbkey, &dbdat, 0) == 0 && !IS_NEG(dbdat.data)) {
		if (memcmp(dbdat.data, a+1, dbdat.size) == 0)
			return;
		stats.arp_change++;
	} else {
		stats.arp_new++;
	}

	dbdat.data = a+1;
	dbdat.size = a->ar_hln;
	dbase->put(dbase, &dbkey, &dbdat, 0);
}

void catch_signal(int sig, void (*handler)(int))
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
#ifdef SA_INTERRUPT
	sa.sa_flags = SA_INTERRUPT;
#endif
	sigaction(sig, &sa, NULL);
}

#include <setjmp.h>
sigjmp_buf env;
volatile int in_poll;

void sig_exit(int signo)
{
	do_exit = 1;
	if (in_poll)
		siglongjmp(env, 1);
}

void sig_sync(int signo)
{
	do_sync = 1;
	if (in_poll)
		siglongjmp(env, 1);
}

void sig_stats(int signo)
{
	do_sync = 1;
	do_stats = 1;
	if (in_poll)
		siglongjmp(env, 1);
}

void send_stats(void)
{
	syslog(LOG_INFO, "arp_rcv: n%lu c%lu app_rcv: tot %lu hits %lu bad %lu neg %lu sup %lu",
	       stats.arp_new, stats.arp_change,

	       stats.app_recv, stats.app_success,
	       stats.app_bad, stats.app_neg, stats.app_suppressed
	       );
	syslog(LOG_INFO, "kern: n%lu c%lu neg %lu arp_send: %lu rlim %lu",
	       stats.kern_new, stats.kern_change, stats.kern_neg,

	       stats.probes_sent, stats.probes_suppressed
	       );
	do_stats = 0;
}


int main(int argc, char **argv)
{
	int opt;
	int do_list = 0;
	char *do_load = NULL;

	while ((opt = getopt(argc, argv, "h?b:lf:a:n:kR:B:")) != EOF) {
		switch (opt) {
	        case 'b':
			dbname = optarg;
			break;
		case 'f':
			if (do_load) {
				fprintf(stderr, "Duplicate option -f\n");
				usage();
			}
			do_load = optarg;
			break;
		case 'l':
			do_list = 1;
			break;
		case 'a':
			active_probing = atoi(optarg);
			break;
		case 'n':
			negative_timeout = atoi(optarg);
			break;
		case 'k':
			no_kernel_broadcasts = 1;
			break;
		case 'R':
			if ((broadcast_rate = atoi(optarg)) <= 0 ||
			    (broadcast_rate = 1000/broadcast_rate) <= 0) {
				fprintf(stderr, "Invalid ARP rate\n");
				exit(-1);
			}
			break;
		case 'B':
			if ((broadcast_burst = atoi(optarg)) <= 0 ||
			    (broadcast_burst = 1000*broadcast_burst) <= 0) {
				fprintf(stderr, "Invalid ARP burst\n");
				exit(-1);
			}
			break;
		case 'h':
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0) {
		ifnum = argc;
		ifnames = argv;
		ifvec = malloc(argc*sizeof(int));
		if (!ifvec) {
			perror("malloc");
			exit(-1);
		}
	}

	if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(-1);
	}

        if (ifnum) {
		int i;
		struct ifreq ifr;
		memset(&ifr, 0, sizeof(ifr));
		for (i=0; i<ifnum; i++) {
			strncpy(ifr.ifr_name, ifnames[i], IFNAMSIZ);
			if (ioctl(udp_sock, SIOCGIFINDEX, &ifr)) {
				perror("ioctl(SIOCGIFINDEX)");
				exit(-1);;
			}
			ifvec[i] = ifr.ifr_ifindex;
		}
	}

	dbase = dbopen(dbname, O_CREAT|O_RDWR, 0644, DB_HASH, NULL);
	if (dbase == NULL) {
		perror("db_open");
		exit(-1);
	}

	if (do_load) {
		char buf[128];
		FILE *fp;
		struct dbkey k;
		DBT dbkey, dbdat;

		dbkey.data = &k;
		dbkey.size = sizeof(k);

		if (strcmp(do_load, "-") == 0 || strcmp(do_load, "--") == 0) {
			fp = stdin;
		} else if ((fp = fopen(do_load, "r")) == NULL) {
			perror("fopen");
			goto do_abort;
		}

		buf[sizeof(buf)-1] = 0;
		while (fgets(buf, sizeof(buf)-1, fp)) {
			__u8 b1[6];
			char ipbuf[128];
			char macbuf[128];

			if (buf[0] == '#')
				continue;

			if (sscanf(buf, "%u%s%s", &k.iface, ipbuf, macbuf) != 3) {
				fprintf(stderr, "Wrong format of input file \"%s\"\n", do_load);
				goto do_abort;
			}
			if (strncmp(macbuf, "FAILED:", 7) == 0)
				continue;
			if (!inet_aton(ipbuf, (struct in_addr*)&k.addr)) {
				fprintf(stderr, "Invalid IP address: \"%s\"\n", ipbuf);
				goto do_abort;
			}

			dbdat.data = hexstring_a2n(macbuf, b1, 6);
			if (dbdat.data == NULL)
				goto do_abort;
			dbdat.size = 6;

			if (dbase->put(dbase, &dbkey, &dbdat, 0)) {
				perror("hash->put");
				goto do_abort;
			}
		}
		dbase->sync(dbase, 0);
		if (fp != stdin)
			fclose(fp);
	}

	if (do_list) {
		DBT dbkey, dbdat;
		printf("%-8s %-15s %s\n", "#Ifindex", "IP", "MAC");
		while (dbase->seq(dbase, &dbkey, &dbdat, R_NEXT) == 0) {
			struct dbkey *key = dbkey.data;
			if (handle_if(key->iface)) {
				if (!IS_NEG(dbdat.data)) {
					char b1[18];
					printf("%-8d %-15s %s\n",
					       key->iface,
					       inet_ntoa(*(struct in_addr*)&key->addr),
					       hexstring_n2a(dbdat.data, 6, b1, 18));
				} else {
					printf("%-8d %-15s FAILED: %dsec ago\n",
					       key->iface,
					       inet_ntoa(*(struct in_addr*)&key->addr),
					       NEG_AGE(dbdat.data));
				}
			}
		}
	}

	if (do_load || do_list)
		goto out;

	pset[0].fd = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (pset[0].fd < 0) {
		perror("socket");
		exit(-1);
	}

	if (1) {
		struct sockaddr_ll sll;
		memset(&sll, 0, sizeof(sll));
		sll.sll_family = AF_PACKET;
		sll.sll_protocol = htons(ETH_P_ARP);
		sll.sll_ifindex = (ifnum == 1 ? ifvec[0] : 0);
		if (bind(pset[0].fd, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
			perror("bind");
			goto do_abort;
		}
	}

	if (rtnl_open(&rth, RTMGRP_NEIGH) < 0) {
		perror("rtnl_open");
		goto do_abort;
	}
	pset[1].fd = rth.fd;

	load_initial_table();

	if (daemon(0, 0)) {
		perror("arpd: daemon");
		goto do_abort;
	}

	openlog("arpd", LOG_PID | LOG_CONS, LOG_DAEMON);
	catch_signal(SIGINT, sig_exit);
	catch_signal(SIGTERM, sig_exit);
	catch_signal(SIGHUP, sig_sync);
	catch_signal(SIGUSR1, sig_stats);

#define EVENTS (POLLIN|POLLPRI|POLLERR|POLLHUP)
	pset[0].events = EVENTS;
	pset[0].revents = 0;
	pset[1].events = EVENTS;
	pset[1].revents = 0;

	sigsetjmp(env, 1);

	for (;;) {
		in_poll = 1;

		if (do_exit)
			break;
		if (do_sync) {
			in_poll = 0;
			dbase->sync(dbase, 0);
			do_sync = 0;
			in_poll = 1;
		}
		if (do_stats)
			send_stats();
		if (poll(pset, 2, 30000) > 0) {
			in_poll = 0;
			if (pset[0].revents&EVENTS)
				get_arp_pkt();
			if (pset[1].revents&EVENTS)
				get_kern_msg();
		} else {
			do_sync = 1;
		}
	}

	undo_sysctl_adjustments();
out:
	dbase->close(dbase);
	exit(0);

do_abort:
	dbase->close(dbase);
	exit(-1);
}
