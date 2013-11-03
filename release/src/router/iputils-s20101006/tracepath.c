/*
 * tracepath.c
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
#include <linux/types.h>
#include <linux/errqueue.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <arpa/inet.h>

#ifndef IP_PMTUDISC_PROBE
#define IP_PMTUDISC_PROBE	3
#endif

struct hhistory
{
	int	hops;
	struct timeval sendtime;
};

struct hhistory his[64];
int hisptr;

struct sockaddr_in target;
__u16 base_port;

const int overhead = 28;
int mtu = 65535;
int hops_to = -1;
int hops_from = -1;
int no_resolve = 0;
int show_both = 0;

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
	struct sockaddr_in addr;
	struct timeval tv;
	struct timeval *rettv;
	int slot;
	int rethops;
	int sndhops;
	int progress = -1;
	int broken_router;

restart:
	memset(&rcvbuf, -1, sizeof(rcvbuf));
	iov.iov_base = &rcvbuf;
	iov.iov_len = sizeof(rcvbuf);
	msg.msg_name = (__u8*)&addr;
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
	slot = ntohs(addr.sin_port) - base_port;
	if (slot>=0 && slot < 63 && his[slot].hops) {
		sndhops = his[slot].hops;
		rettv = &his[slot].sendtime;
		his[slot].hops = 0;
	}
	broken_router = 0;
	if (res == sizeof(rcvbuf)) {
		if (rcvbuf.ttl == 0 || rcvbuf.tv.tv_sec == 0) {
			broken_router = 1;
		} else {
			sndhops = rcvbuf.ttl;
			rettv = &rcvbuf.tv;
		}
	}

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_IP) {
			if (cmsg->cmsg_type == IP_RECVERR) {
				e = (struct sock_extended_err *) CMSG_DATA(cmsg);
			} else if (cmsg->cmsg_type == IP_TTL) {
				rethops = *(int*)CMSG_DATA(cmsg);
			} else {
				printf("cmsg:%d\n ", cmsg->cmsg_type);
			}
		}
	}
	if (e == NULL) {
		printf("no info\n");
		return 0;
	}
	if (e->ee_origin == SO_EE_ORIGIN_LOCAL) {
		printf("%2d?: %*s ", ttl, -(HOST_COLUMN_SIZE - 1), "[LOCALHOST]");
	} else if (e->ee_origin == SO_EE_ORIGIN_ICMP) {
		char abuf[128];
		struct sockaddr_in *sin = (struct sockaddr_in*)(e+1);
		struct hostent *h = NULL;

		inet_ntop(AF_INET, &sin->sin_addr, abuf, sizeof(abuf));

		if (sndhops>0)
			printf("%2d:  ", sndhops);
		else
			printf("%2d?: ", ttl);

		if (!no_resolve || show_both) {
			fflush(stdout);
			h = gethostbyaddr((char *) &sin->sin_addr, sizeof(sin->sin_addr), AF_INET);
		}

		if (no_resolve)
			print_host(abuf, h ? h->h_name : abuf, show_both);
		else
			print_host(h ? h->h_name : abuf, abuf, show_both);
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
		if (e->ee_origin == SO_EE_ORIGIN_ICMP &&
		    e->ee_type == 11 &&
		    e->ee_code == 0) {
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

	memset(sndbuf,0,mtu);

restart:
	for (i=0; i<10; i++) {
		int res;

		hdr->ttl = ttl;
		target.sin_port = htons(base_port + hisptr);
		gettimeofday(&hdr->tv, NULL);
		his[hisptr].hops = ttl;
		his[hisptr].sendtime = hdr->tv;
		if (sendto(fd, sndbuf, mtu-overhead, 0, (struct sockaddr*)&target, sizeof(target)) > 0)
			break;
		res = recverr(fd, ttl);
		his[hisptr].hops = 0;
		if (res==0)
			return 0;
		if (res > 0)
			goto restart;
	}
	hisptr = (hisptr + 1)&63;

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
	fprintf(stderr, "Usage: tracepath [-n] [-b] [-l <len>] <destination>[/<port>]\n");
	exit(-1);
}

int
main(int argc, char **argv)
{
	struct hostent *he;
	int fd;
	int on;
	int ttl;
	char *p;
	int ch;

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


	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(1);
	}
	target.sin_family = AF_INET;

	p = strchr(argv[0], '/');
	if (p) {
		*p = 0;
		base_port = atoi(p+1);
	} else
		base_port = 44444;
	he = gethostbyname(argv[0]);
	if (he == NULL) {
		herror("gethostbyname");
		exit(1);
	}
	memcpy(&target.sin_addr, he->h_addr, 4);

	on = IP_PMTUDISC_PROBE;
	if (setsockopt(fd, SOL_IP, IP_MTU_DISCOVER, &on, sizeof(on)) &&
	    (on = IP_PMTUDISC_DO,
	     setsockopt(fd, SOL_IP, IP_MTU_DISCOVER, &on, sizeof(on)))) {
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

	for (ttl=1; ttl<32; ttl++) {
		int res;
		int i;

		on = ttl;
		if (setsockopt(fd, SOL_IP, IP_TTL, &on, sizeof(on))) {
			perror("IP_TTL");
			exit(1);
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
