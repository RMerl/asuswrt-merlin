/*
 * Ethernet Switch IGMP Snooper
 * Copyright (C) 2014 ASUSTeK Inc.
 * All Rights Reserved.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <linux/types.h>
#include <linux/filter.h>
#include <asm/byteorder.h>
#include <syslog.h>

#include "snooper.h"
#include "queue.h"

#define INADDR_IGMPV3_GROUP ((in_addr_t) 0xe0000016)

static int terminated = 0;
static int fd, fd_loop;
static int ifindex;
unsigned char ifhwaddr[ETHER_ADDR_LEN];
in_addr_t ifaddr;

static int init_socket(char *ifname, int rcvsize)
{
	static const struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS, SKF_AD_OFF + SKF_AD_PROTOCOL),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, ETHERTYPE_IP, 0, 3),
		BPF_STMT(BPF_LD|BPF_B|BPF_ABS, 9),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, IPPROTO_IGMP, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, 0x7fffffff),
		BPF_STMT(BPF_RET|BPF_K, 0),
	};
	static const struct sock_fprog fprog = {
		.len = sizeof(filter)/sizeof(filter[0]),
		.filter = (struct sock_filter *) filter,
	};
	static const struct sock_filter filter_loop[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS, SKF_AD_OFF + SKF_AD_PKTTYPE),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, PACKET_LOOPBACK, 0, 3),
		BPF_STMT(BPF_LD|BPF_B|BPF_ABS, 9),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, IPPROTO_IGMP, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, 0x7fffffff),
		BPF_STMT(BPF_RET|BPF_K, 0),
	};
	static const struct sock_fprog fprog_loop = {
		.len = sizeof(filter_loop)/sizeof(filter_loop[0]),
		.filter = (struct sock_filter *) filter_loop,
	};
	struct ifreq ifr;
	struct sockaddr_ll sll;
	struct packet_mreq mreq;
	int int_1 = 1;

	ifindex = if_nametoindex(ifname);
	if (ifindex == 0) {
		log_error("ifindex: %s", ifname, strerror(errno));
		return -1;
	}

	fd = open_socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	if (fd < 0)
		return -1;

	fd_loop = open_socket(AF_INET, SOCK_RAW, IPPROTO_IGMP);
	if (fd_loop < 0)
		goto error_loop;

	if (setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &fprog, sizeof(fprog)) < 0) {
		log_error("setsockopt::SO_ATTACH_FILTER: %s", strerror(errno));
		goto error;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvsize, sizeof(rcvsize)) < 0) {
		log_error("setsockopt::SO_RCVBUF: %s", strerror(errno));
		goto error;
	}

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = htons(ETH_P_IP);
	sll.sll_ifindex = ifindex;
	if (bind(fd, (struct sockaddr *) &sll , sizeof(sll)) < 0) {
		log_error("bind: %s", strerror(errno));
		goto error;
	}

	memset(&mreq, 0, sizeof(mreq));
	mreq.mr_ifindex = ifindex;
	mreq.mr_type = PACKET_MR_ALLMULTI;
	if (setsockopt(fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		log_error("setsockopt::PACKET_ADD_MEMBERSHIP: %s", strerror(errno));
		goto error;
	}

	if (setsockopt(fd_loop, SOL_SOCKET, SO_ATTACH_FILTER, &fprog_loop, sizeof(fprog_loop)) < 0) {
		log_error("setsockopt::SO_ATTACH_FILTER: %s", strerror(errno));
		goto error;
	}

	if (setsockopt(fd_loop, SOL_SOCKET, SO_RCVBUF, &rcvsize, sizeof(rcvsize)) < 0) {
		log_error("setsockopt::SO_RCVBUF: %s", strerror(errno));
		goto error;
	}

	if (setsockopt(fd_loop, SOL_SOCKET, SO_BINDTODEVICE, ifname, strlen(ifname)) < 0) {
		log_error("setsockopt::SO_BINDTODEVICE: %s", strerror(errno));
		goto error;
	}

	if (setsockopt(fd_loop, IPPROTO_IP, IP_MULTICAST_LOOP, &int_1, sizeof(int_1)) < 0) {
		log_error("setsockopt::IP_MULTICAST_LOOP: %s", strerror(errno));
		goto error;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		log_error("ioctl::SIOCGIFHWADDR: %s", strerror(errno));
		goto error;
	}
	memcpy(ifhwaddr, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ifaddr = (ioctl(fd, SIOCGIFADDR, &ifr) < 0) ? INADDR_ANY :
		((struct sockaddr_in*) &ifr.ifr_addr)->sin_addr.s_addr;

	return 0;

error:
	close(fd_loop), fd_loop = -1;
error_loop:
	close(fd), fd = -1;
	return -1;
}

int listen_query(in_addr_t group, int timeout)
{
	struct ip_mreqn imreq;
	int opt = timeout ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP;

	memset(&imreq, 0, sizeof(imreq));
	imreq.imr_multiaddr.s_addr = group;
	imreq.imr_ifindex = ifindex;
	return setsockopt(fd_loop, IPPROTO_IP, opt, &imreq, sizeof(imreq));
}

int send_query(in_addr_t group)
{
	unsigned char packet[1500];
	unsigned char ea[ETHER_ADDR_LEN];
	struct sockaddr_ll sll;
	in_addr_t dst = group ? : htonl(INADDR_ALLHOSTS_GROUP);
	int ret, len;

	ether_mtoe(dst, ea);
	len = build_query(packet, sizeof(packet), group, dst);
	if (len < 0)
		return -1;

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = htons(ETH_P_IP);
	sll.sll_ifindex = ifindex;
	sll.sll_halen = sizeof(ea);
	memcpy(sll.sll_addr, ea, sizeof(ea));

	do {
		ret = sendto(fd, packet, len, 0, (struct sockaddr *) &sll, sizeof(sll));
	} while (ret < 0 && errno == EINTR);
	if (ret < 0 && errno != ENETDOWN)
		log_error("sendto: %s", strerror(errno));

	if (ret > 0)
		accept_igmp(packet, ret, ifhwaddr, -1);

	return ret;
}

int switch_probe(void)
{
	struct {
		unsigned short skipcount;
		unsigned short func_code;
		unsigned short rcpt_num;
	} __attribute__((packed)) packet;
	struct sockaddr_ll sll;
	int ret;

	memset(&packet, 0, sizeof(packet));
	packet.func_code = __cpu_to_le16(1);

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = htons(0x9000);
	sll.sll_ifindex = ifindex;
	sll.sll_halen = ETHER_ADDR_LEN;
	memcpy(sll.sll_addr, ifhwaddr, ETHER_ADDR_LEN);

	do {
		ret = sendto(fd, &packet, sizeof(packet), 0,
			(struct sockaddr *) &sll, sizeof(sll));
	} while (ret < 0 && errno == EINTR);
	if (ret < 0 && errno != ENETDOWN)
		log_error("sendto: %s", strerror(errno));

	return ret;
}

static void sighandler(int sig)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
	case SIGALRM:
		terminated = sig ? : -1;
		break;
	}
}

static void usage(char *name, int service, int querier)
{
	printf("Usage: %s [-d|-D] [-q|-Q] [-s <switch>] [-b <bridge>] [-v <vid] [-x]\n", name);
#ifdef DEBUG
	printf(" -d or -D	run in background%s or in foreground%s\n",
	    service ? " (default)" : "", service ? "" : " (default)");
	printf(" -q or -Q	enable%s or disable%s querier\n",
	    querier ? " (default)" : "", querier ? "" : " (default)");
	printf(" -s <switch>	switch interface\n");
	printf(" -b <bridge>	bridge interface of switch interface\n");
	printf(" -v <vid>	vlan id of switch interface\n");
	printf(" -x		disable switch ports isolation\n");
#endif
}

int main(int argc, char *argv[])
{
	unsigned char packet[1500];
	struct sockaddr_ll sll;
	struct sockaddr_in sin;
	socklen_t socklen;
	struct sigaction sa;
	char *name, *ifswitch, *ifbridge;
	int opt, ifvid, querier, service, cputrap, ret = -1;

	name = strrchr(argv[0], '/');
	name = name ? name + 1 : argv[0];

	ifswitch = ifbridge = NULL;
	ifvid = -1;
	querier = 1;
	service = 1;
	cputrap = 1;

	while ((opt = getopt(argc, argv, "s:b:v:qQdDxh")) > 0) {
		switch (opt) {
		case 's':
			ifswitch = optarg;
			break;
		case 'b':
			ifbridge = optarg;
			break;
		case 'v':
			ifvid = atoi(optarg);
			break;
		case 'd':
		case 'D':
			service = (opt == 'd');
			break;
		case 'q':
		case 'Q':
			querier = (opt == 'q');
			break;
		case 'x':
			cputrap = 0;
			break;
		default:
			usage(name, service, querier);
			return 1;
		}
	}

	if (!ifswitch) {
		usage(name, service, querier);
		return 1;
	} else if (ifbridge && strcmp(ifbridge, ifswitch) == 0)
		ifbridge = NULL;

	if (service)
		openlog(name, LOG_PID, LOG_DAEMON);
	log_info("started on %s%s%s", ifswitch, ifbridge ? "@" : "", ifbridge ? : "");

	if (init_socket(ifbridge ? : ifswitch, sizeof(packet) * 16) < 0)
		goto socket_error;
	if (switch_probe() < 0 ||
	    switch_init(ifswitch, ifvid, cputrap) < 0)
		goto switch_error;
	if (init_timers() < 0)
		goto timer_error;
	if (init_cache() < 0)
		goto cache_error;

	if (service && daemon(0, 0) < 0)
		log_error("daemon: %s", strerror(errno));

	sa.sa_handler = sighandler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGALRM, &sa, NULL);

	init_querier(querier);

	while (!terminated) {
		fd_set rfds;
		struct timeval tv, *timeout;
		int fdmax = (fd > fd_loop) ? fd : fd_loop;

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		FD_SET(fd_loop, &rfds);
		timeout = (next_timer(&tv) < 0) ? NULL : &tv;

		ret = select(fdmax + 1, &rfds, NULL, NULL, timeout);
		if (ret < 0 && errno == EINTR)
			continue;
		if (ret < 0) {
			log_error("select: %s", strerror(errno));
			goto error;
		} else if (ret == 0)
			goto timer;

		if (FD_ISSET(fd, &rfds)) {
			do {
				socklen = sizeof(sll);
				ret = recvfrom(fd, packet, sizeof(packet), 0, (struct sockaddr *) &sll, &socklen);
			} while (ret < 0 && errno == EINTR);
			if (ret < 0 && errno != ENETDOWN) {
				log_error("recvfrom: %s", strerror(errno));
				goto error;
			}
			if (sll.sll_hatype == ARPHRD_ETHER && sll.sll_halen == ETHER_ADDR_LEN &&
			    ret > 0 && accept_igmp(packet, ret, sll.sll_addr, 0) < 0)
				goto error;
		}
		if (FD_ISSET(fd_loop, &rfds)) {
			socklen_t socklen = sizeof(sin);
			do {
				ret = recvfrom(fd_loop, packet, sizeof(packet), 0, (struct sockaddr *) &sin, &socklen);
			} while (ret < 0 && errno == EINTR);
			if (ret < 0 && errno != ENETDOWN) {
				log_error("recvfrom: %s", strerror(errno));
				goto error;
			}
			if (sin.sin_family == AF_INET &&
			    ret > 0 && accept_igmp(packet, ret, ifhwaddr, 1) < 0)
				goto error;
		}

	timer:
		run_timers();
	}

	log_info("terminated with signal %d", terminated);
	ret = 0;

error:
	purge_cache();
cache_error:
	purge_timers();
timer_error:
	switch_done();
switch_error:
	if (fd >= 0)
		close(fd);
	if (fd_loop >= 0)
		close(fd_loop);
socket_error:

	return ret;
}
