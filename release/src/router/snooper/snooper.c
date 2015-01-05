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
#include <syslog.h>

#include "snooper.h"
#include "queue.h"

#define INADDR_IGMPV3_GROUP ((in_addr_t) 0xe0000016)

static int terminated = 0;
static int fd;
static int ifindex;
unsigned char ifhwaddr[ETHER_ADDR_LEN];
in_addr_t ifaddr;

static int snoop_init(char *ifswitch, char *ifbridge, int rcvsize, unsigned char *ifhwaddr, in_addr_t *ifaddr)
{
	static const struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS, SKF_AD_OFF + SKF_AD_PROTOCOL),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, ETHERTYPE_IP, 0, 5),
		BPF_STMT(BPF_LD|BPF_H|BPF_ABS, 12),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, ETHERTYPE_IP, 0, 3),
		BPF_STMT(BPF_LD|BPF_B|BPF_ABS, 23),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, IPPROTO_IGMP, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, 0x7fffffff),
		BPF_STMT(BPF_RET|BPF_K, 0),
	};
	static const struct sock_fprog fprog = {
		.len = sizeof(filter)/sizeof(filter[0]),
		.filter = (struct sock_filter *) filter,
	};
	struct ifreq ifr;
	struct sockaddr_ll sll;
	struct packet_mreq mreq;
	int fd;

	if (init_timers() < 0 || init_cache() < 0)
		return -1;

#ifdef SOCK_CLOEXEC
	fd = socket(PF_PACKET, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, htons(ETH_P_IP));
	if (fd < 0) {
		log_error("socket: %s", strerror(errno));
		return -1;
	}
#else
	int value;

	fd = socket(PF_PACKET, SOCK_RAW, htons(ETHERTYPE_IP));
	if (fd < 0) {
		log_error("socket: %s", strerror(errno));
		return -1;
	}

	value = fcntl(fd, F_GETFD, 0);
	if (value < 0 || fcntl(fd, F_SETFD, value | FD_CLOEXEC) < 0) {
		log_error("fcntl::FD_CLOEXEC: %s", strerror(errno));
		goto error;
	}

	value = fcntl(fd, F_GETFL, 0);
	if (value < 0 || fcntl(fd, F_SETFL, value | O_NONBLOCK) < 0) {
		log_error("fcntl::O_NONBLOCK: %s", strerror(errno));
		goto error;
	}
#endif

	if (setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &fprog, sizeof(fprog)) < 0) {
		log_error("setsockopt::SO_ATTACH_FILTER: %s", strerror(errno));
		goto error;
	}

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_ifindex = ifindex;
	sll.sll_protocol = ifbridge ? htons(ETH_P_ALL) : htons(ETH_P_IP);
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

	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvsize, sizeof(rcvsize)) < 0) {
		log_error("setsockopt::SO_RCVBUF: %s", strerror(errno));
		goto error;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifbridge ? : ifswitch, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		log_error("ioctl::SIOCGIFHWADDR: %s", strerror(errno));
		goto error;
	}
	if (ifhwaddr)
		memcpy(ifhwaddr, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);

	if (ifaddr) {
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, ifbridge ? : ifswitch, sizeof(ifr.ifr_name));
		*ifaddr = (ioctl(fd, SIOCGIFADDR, &ifr) < 0) ?
		    INADDR_ANY : ((struct sockaddr_in*) &ifr.ifr_addr)->sin_addr.s_addr;
	}

	if (switch_init(ifswitch) < 0)
		goto error;

	return fd;

error:
	close(fd);
	return -1;
}

static void snoop_done(void)
{
	purge_timers();
	purge_cache();
	if (fd >= 0)
		close(fd);
	switch_done();
}

int send_query(in_addr_t group)
{
	unsigned char packet[1500];
	unsigned char ea[ETHER_ADDR_LEN];
	struct sockaddr_ll sll;
	in_addr_t dst = group ? : htonl(INADDR_ALLHOSTS_GROUP);
	int ret, len;

	ether_mtoe(dst, ea);
	len = build_query(packet, sizeof(packet), group, dst, ea);
	if (len < 0)
		return -1;

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_ifindex = ifindex;
	sll.sll_halen = sizeof(ea);
	memcpy(sll.sll_addr, ea, sizeof(ea));

	do {
		ret = sendto(fd, packet, len, 0, (struct sockaddr *) &sll, sizeof(sll));
	} while (ret < 0 && errno == EINTR);
	if (ret < 0 && errno != ENETDOWN)
		log_error("sendto: %s", strerror(errno));

	if (ret > 0)
		accept_igmp(packet, ret, ifhwaddr, 1);

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
	printf("Usage: %s [-d|-D] [-q|-Q] [-s <switch>] [-b <bridge>]\n", name);
	printf(" -d or -D	run in background%s or in foreground%s\n",
	    service ? " (default)" : "", service ? "" : " (default)");
	printf(" -q or -Q	enable%s or disable%s querier\n",
	    querier ? " (default)" : "", querier ? "" : " (default)");
	printf(" -s <switch>	switch interface\n");
	printf(" -b <bridge>	bridge interface, if switch interface is bridged\n");
}

int main(int argc, char *argv[])
{
	unsigned char packet[1500];
	struct sockaddr_ll sll;
        struct sigaction sa;
	char *name, *ifswitch, *ifbridge;
	int opt, querier, service, ret = -1;

	name = strrchr(argv[0], '/');
	name = name ? name + 1 : argv[0];

	ifswitch = ifbridge = NULL;
	querier = 1;
	service = 1;

	while ((opt = getopt(argc, argv, "s:b:qQdDh")) > 0) {
		switch (opt) {
		case 's':
			ifswitch = optarg;
			break;
		case 'b':
			ifbridge = optarg;
			break;
		case 'd':
		case 'D':
			service = (opt == 'd');
			break;
		case 'q':
		case 'Q':
			querier = (opt == 'q');
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

	openlog(name, 0, LOG_USER);
	log_info("started on %s%s%s", ifswitch, ifbridge ? "/" : "", ifbridge ? : "");

	ifindex = if_nametoindex(ifswitch);
	if (ifindex == 0) {
		log_error("%s: %s", ifbridge ? : ifswitch, strerror(errno));
		return 1;
	}

	fd = snoop_init(ifswitch, ifbridge, sizeof(packet) * 16, ifhwaddr, &ifaddr);
	if (fd < 0)
		goto error;

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

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		timeout = (next_timer(&tv) < 0) ? NULL : &tv;

		ret = select(fd + 1, &rfds, NULL, NULL, timeout);
		if (ret < 0 && errno == EINTR)
			continue;
		if (ret < 0) {
			log_error("select: %s", strerror(errno));
			goto error;
		}

		if (ret > 0 && FD_ISSET(fd, &rfds)) {
			socklen_t socklen = sizeof(sll);
			do {
				ret = recvfrom(fd, packet, sizeof(packet), 0, (struct sockaddr *) &sll, &socklen);
			} while (ret < 0 && errno == EINTR);
			if (ret < 0 && errno != ENETDOWN) {
				log_error("recvfrom: %s", strerror(errno));
				goto error;
			}
			if (sll.sll_hatype == ARPHRD_ETHER && sll.sll_halen == ETHER_ADDR_LEN) {
				ret = accept_igmp(packet, ret, sll.sll_addr, 0);
				if (ret < 0)
					goto error;
			}
		}

		run_timers();
	}

	log_info("terminated with signal %d", terminated);
	ret = 0;

error:
	snoop_done();

	return ret;
}
