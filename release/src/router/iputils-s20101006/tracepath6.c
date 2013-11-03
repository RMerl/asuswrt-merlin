/*
 * tracepath6.c
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>

#include <linux/types.h>
#include <linux/errqueue.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <resolv.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <arpa/inet.h>

#ifndef SOL_IPV6
#define SOL_IPV6 IPPROTO_IPV6
#endif

#ifndef IP_PMTUDISC_DO
#define IP_PMTUDISC_DO		3
#endif
#ifndef IPV6_PMTUDISC_DO
#define IPV6_PMTUDISC_DO	3
#endif

struct hhistory
{
	int	hops;
	struct timeval sendtime;
};

struct hhistory his[64];
int hisptr;

sa_family_t family = AF_INET6;
struct sockaddr_storage target;
socklen_t targetlen;
__u16 base_port;

int overhead = 48;
int mtu = 128000;
int hops_to = -1;
int hops_from = -1;
int no_resolve = 0;
int show_both = 0;
int mapped;

#define HOST_COLUMN_SIZE	52

struct probehdr
{
	__u32 ttl;
	struct timeval tv;
};

void data_wait(int fd)
{
	fd_set fds;
	struct timeval tv;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	select(fd+1, &fds, NULL, NULL, &tv);
}

void print_host(const char *a, const char *b, int both)
{
	size_t plen = 0;
	printf("%s", a);
	plen = strlen(a);
	if (both) {
		printf(" (%s)", b);
		plen += strlen(b) + 3;
	}
	if (plen >= HOST_COLUMN_SIZE)
		plen = HOST_COLUMN_SIZE - 1;
	printf("%*s", HOST_COLUMN_SIZE - plen, "");
}

int recverr(int fd, int ttl)
{
	int res;
	struct probehdr rcvbuf;
	char cbuf[512];
	struct iovec  iov;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct sock_extended_err *e;
	struct sockaddr_storage addr;
	struct timeval tv;
	struct timeval *rettv;
	int slot = 0;
	int rethops;
	int sndhops;
	int progress = -1;
	int broken_router;

restart:
	memset(&rcvbuf, -1, sizeof(rcvbuf));
	iov.iov_base = &rcvbuf;
	iov.iov_len = sizeof(rcvbuf);
	msg.msg_name = (caddr_t)&addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_flags = 0;
	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);

	gettimeofday(&tv, NULL);
	res = recvmsg(fd, &msg, MSG_ERRQUEUE);
	if (res < 0) {
		if (errno == EAGAIN)
			return progress;
		goto restart;
	}

	progress = mtu;

	rethops = -1;
	sndhops = -1;
	e = NULL;
	rettv = NULL;

	slot = -base_port;
	switch (family) {
	case AF_INET6:
		slot += ntohs(((struct sockaddr_in6 *)&addr)->sin6_port);
		break;
	case AF_INET:
		slot += ntohs(((struct sockaddr_in *)&addr)->sin_port);
		break;
	}

	if (slot >= 0 && slot < 63 && his[slot].hops) {
		sndhops = his[slot].hops;
		rettv = &his[slot].sendtime;
		his[slot].hops = 0;
	}
	broken_router = 0;
	if (res == sizeof(rcvbuf)) {
		if (rcvbuf.ttl == 0 || rcvbuf.tv.tv_sec == 0)
			broken_router = 1;
		else {
			sndhops = rcvbuf.ttl;
			rettv = &rcvbuf.tv;
		}
	}

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		switch (cmsg->cmsg_level) {
		case SOL_IPV6:
			switch(cmsg->cmsg_type) {
			case IPV6_RECVERR:
				e = (struct sock_extended_err *)CMSG_DATA(cmsg);
				break;
			case IPV6_HOPLIMIT:
#ifdef IPV6_2292HOPLIMIT
			case IPV6_2292HOPLIMIT:
#endif
				rethops = *(int*)CMSG_DATA(cmsg);
				break;
			default:
				printf("cmsg6:%d\n ", cmsg->cmsg_type);
			}
			break;
		case SOL_IP:
			switch(cmsg->cmsg_type) {
			case IP_RECVERR:
				e = (struct sock_extended_err *)CMSG_DATA(cmsg);
				break;
			case IP_TTL:
				rethops = *(__u8*)CMSG_DATA(cmsg);
				break;
			default:
				printf("cmsg4:%d\n ", cmsg->cmsg_type);
			}
		}
	}
	if (e == NULL) {
		printf("no info\n");
		return 0;
	}
	if (e->ee_origin == SO_EE_ORIGIN_LOCAL)
		printf("%2d?: %-32s ", ttl, "[LOCALHOST]");
	else if (e->ee_origin == SO_EE_ORIGIN_ICMP6 ||
		 e->ee_origin == SO_EE_ORIGIN_ICMP) {
		char abuf[NI_MAXHOST], hbuf[NI_MAXHOST];
		struct sockaddr *sa = (struct sockaddr *)(e + 1);
		socklen_t salen;

		if (sndhops>0)
			printf("%2d:  ", sndhops);
		else
			printf("%2d?: ", ttl);

		switch (sa->sa_family) {
		case AF_INET6:
			salen = sizeof(struct sockaddr_in6);
			break;
		case AF_INET:
			salen = sizeof(struct sockaddr_in);
			break;
		default:
			salen = 0;
		}

		if (no_resolve || show_both) {
			if (getnameinfo(sa, salen,
					abuf, sizeof(abuf), NULL, 0,
					NI_NUMERICHOST))
				strcpy(abuf, "???");
		} else
			abuf[0] = 0;

		if (!no_resolve || show_both) {
			fflush(stdout);
			if (getnameinfo(sa, salen,
					hbuf, sizeof(hbuf), NULL, 0,
					0))
				strcpy(hbuf, "???");
		} else
			hbuf[0] = 0;

		if (no_resolve)
			print_host(abuf, hbuf, show_both);
		else
			print_host(hbuf, abuf, show_both);
	}

	if (rettv) {
		int diff = (tv.tv_sec-rettv->tv_sec)*1000000+(tv.tv_usec-rettv->tv_usec);
		printf("%3d.%03dms ", diff/1000, diff%1000);
		if (broken_router)
			printf("(This broken router returned corrupted payload) ");
	}

	switch (e->ee_errno) {
	case ETIMEDOUT:
		printf("\n");
		break;
	case EMSGSIZE:
		printf("pmtu %d\n", e->ee_info);
		mtu = e->ee_info;
		progress = mtu;
		break;
	case ECONNREFUSED:
		printf("reached\n");
		hops_to = sndhops<0 ? ttl : sndhops;
		hops_from = rethops;
		return 0;
	case EPROTO:
		printf("!P\n");
		return 0;
	case EHOSTUNREACH:
		if ((e->ee_origin == SO_EE_ORIGIN_ICMP &&
		     e->ee_type == 11 &&
		     e->ee_code == 0) ||
		    (e->ee_origin == SO_EE_ORIGIN_ICMP6 &&
		     e->ee_type == 3 &&
		     e->ee_code == 0)) {
			if (rethops>=0) {
				if (rethops<=64)
					rethops = 65-rethops;
				else if (rethops<=128)
					rethops = 129-rethops;
				else
					rethops = 256-rethops;
				if (sndhops>=0 && rethops != sndhops)
					printf("asymm %2d ", rethops);
				else if (sndhops<0 && rethops != ttl)
					printf("asymm %2d ", rethops);
			}
			printf("\n");
			break;
		}
		printf("!H\n");
		return 0;
	case ENETUNREACH:
		printf("!N\n");
		return 0;
	case EACCES:
		printf("!A\n");
		return 0;
	default:
		printf("\n");
		errno = e->ee_errno;
		perror("NET ERROR");
		return 0;
	}
	goto restart;
}

int probe_ttl(int fd, int ttl)
{
	int i;
	char sndbuf[mtu];
	struct probehdr *hdr = (struct probehdr*)sndbuf;

	memset(sndbuf, 0, mtu);

restart:

	for (i=0; i<10; i++) {
		int res;

		hdr->ttl = ttl;
		switch (family) {
		case AF_INET6:
			((struct sockaddr_in6 *)&target)->sin6_port = htons(base_port + hisptr);
			break;
		case AF_INET:
			((struct sockaddr_in *)&target)->sin_port = htons(base_port + hisptr);
			break;
		}
		gettimeofday(&hdr->tv, NULL);
		his[hisptr].hops = ttl;
		his[hisptr].sendtime = hdr->tv;
		if (sendto(fd, sndbuf, mtu-overhead, 0, (struct sockaddr *)&target, targetlen) > 0)
			break;
		res = recverr(fd, ttl);
		his[hisptr].hops = 0;
		if (res==0)
			return 0;
		if (res > 0)
			goto restart;
	}
	hisptr = (hisptr + 1) & 63;

	if (i<10) {
		data_wait(fd);
		if (recv(fd, sndbuf, sizeof(sndbuf), MSG_DONTWAIT) > 0) {
			printf("%2d?: reply received 8)\n", ttl);
			return 0;
		}
		return recverr(fd, ttl);
	}

	printf("%2d:  send failed\n", ttl);
	return 0;
}

static void usage(void) __attribute((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: tracepath6 [-n] [-b] [-l <len>] <destination>[/<port>]\n");
	exit(-1);
}


int main(int argc, char **argv)
{
	int fd;
	int on;
	int ttl;
	char *p;
	struct addrinfo hints, *ai, *ai0;
	int ch;
	int gai;
	char pbuf[NI_MAXSERV];

	while ((ch = getopt(argc, argv, "nbh?l:")) != EOF) {
		switch(ch) {
		case 'n':
			no_resolve = 1;
			break;
		case 'b':
			show_both = 1;
			break;
		case 'l':
			if ((mtu = atoi(optarg)) <= overhead) {
				fprintf(stderr, "Error: length must be >= %d\n", overhead);
				exit(1);
			}
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	p = strchr(argv[0], '/');
	if (p) {
		*p = 0;
		base_port = (unsigned)atoi(p+1);
	} else {
		base_port = 44444;
	}
	sprintf(pbuf, "%u", base_port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = 0;
	gai = getaddrinfo(argv[0], pbuf, &hints, &ai0);
	if (gai) {
		herror("getaddrinfo");
		exit(1);
	}

	fd = -1;
	for (ai = ai0; ai; ai = ai->ai_next) {
		/* sanity check */
		if (family && ai->ai_family != family)
			continue;
		family = ai->ai_family;
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0)
			continue;
		memcpy(&target, ai->ai_addr, sizeof(target));
		targetlen = ai->ai_addrlen;
		break;
	}
	if (fd < 0) {
		perror("socket/connect");
		exit(1);
	}
	freeaddrinfo(ai0);

	switch (family) {
	case AF_INET6:
		mtu = 128000;
		overhead = 48;
		on = IPV6_PMTUDISC_DO;
		if (setsockopt(fd, SOL_IPV6, IPV6_MTU_DISCOVER, &on, sizeof(on)) &&
		    (on = IPV6_PMTUDISC_DO,
		     setsockopt(fd, SOL_IPV6, IPV6_MTU_DISCOVER, &on, sizeof(on)))) {
			perror("IPV6_MTU_DISCOVER");
			exit(1);
		}
		on = 1;
		if (setsockopt(fd, SOL_IPV6, IPV6_RECVERR, &on, sizeof(on))) {
			perror("IPV6_RECVERR");
			exit(1);
		}
		if (
#ifdef IPV6_RECVHOPLIMIT
		    setsockopt(fd, SOL_IPV6, IPV6_HOPLIMIT, &on, sizeof(on)) &&
		    setsockopt(fd, SOL_IPV6, IPV6_2292HOPLIMIT, &on, sizeof(on))
#else
		    setsockopt(fd, SOL_IPV6, IPV6_HOPLIMIT, &on, sizeof(on))
#endif
		    ) {
			perror("IPV6_HOPLIMIT");
			exit(1);
		}
		if (!IN6_IS_ADDR_V4MAPPED(&(((struct sockaddr_in6 *)&target)->sin6_addr)))
			break;
		mapped = 1;
		/*FALLTHROUGH*/
	case AF_INET:
		mtu = 65535;
		overhead = 28;
		on = IP_PMTUDISC_DO;
		if (setsockopt(fd, SOL_IP, IP_MTU_DISCOVER, &on, sizeof(on))) {
			perror("IP_MTU_DISCOVER");
			exit(1);
		}
		on = 1;
		if (setsockopt(fd, SOL_IP, IP_RECVERR, &on, sizeof(on))) {
			perror("IP_RECVERR");
			exit(1);
		}
		if (setsockopt(fd, SOL_IP, IP_RECVTTL, &on, sizeof(on))) {
			perror("IP_RECVTTL");
			exit(1);
		}
	}

	for (ttl=1; ttl<32; ttl++) {
		int res;
		int i;

		on = ttl;
		switch (family) {
		case AF_INET6:
			if (setsockopt(fd, SOL_IPV6, IPV6_UNICAST_HOPS, &on, sizeof(on))) {
				perror("IPV6_UNICAST_HOPS");
				exit(1);
			}
			if (!mapped)
				break;
			/*FALLTHROUGH*/
		case AF_INET:
			if (setsockopt(fd, SOL_IP, IP_TTL, &on, sizeof(on))) {
				perror("IP_TTL");
				exit(1);
			}
		}

restart:
		for (i=0; i<3; i++) {
			int old_mtu;

			old_mtu = mtu;
			res = probe_ttl(fd, ttl);
			if (mtu != old_mtu)
				goto restart;
			if (res == 0)
				goto done;
			if (res > 0)
				break;
		}

		if (res < 0)
			printf("%2d:  no reply\n", ttl);
	}
	printf("     Too many hops: pmtu %d\n", mtu);

done:
	printf("     Resume: pmtu %d ", mtu);
	if (hops_to>=0)
		printf("hops %d ", hops_to);
	if (hops_from>=0)
		printf("back %d ", hops_from);
	printf("\n");
	exit(0);
}
