#include <syslog.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <sys/socket.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <linux/rtnetlink.h>
#include <linux/filter.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "odhcp6c.h"

static int sock = -1, rtnl = -1;
static int if_index = -1;
static int bfd_failed = 0, bfd_limit = 0, bfd_interval = 0;
static bool bfd_armed = false;


static void bfd_send(int signal __attribute__((unused)))
{
	struct {
		struct ip6_hdr ip6;
		struct icmp6_hdr icmp6;
	} ping;
	memset(&ping, 0, sizeof(ping));

	ping.ip6.ip6_vfc = 6 << 4;
	ping.ip6.ip6_plen = htons(8);
	ping.ip6.ip6_nxt = IPPROTO_ICMPV6;
	ping.ip6.ip6_hlim = 255;

	ping.icmp6.icmp6_type = ICMP6_ECHO_REQUEST;
	ping.icmp6.icmp6_data32[0] = htonl(0xbfd0bfd);

	size_t pdlen, rtlen;
	struct odhcp6c_entry *pd = odhcp6c_get_state(STATE_IA_PD, &pdlen), *cpd = NULL;
	struct odhcp6c_entry *rt = odhcp6c_get_state(STATE_RA_ROUTE, &rtlen), *crt = NULL;
	bool crt_found = false;

	alarm(bfd_interval);

	if (bfd_armed) {
		if (++bfd_failed > bfd_limit) {
			raise(SIGUSR2);
			return;
		}
	}

	// Detect PD-Prefix
	for (size_t i = 0; i < pdlen / sizeof(*pd); ++i)
		if (!cpd || ((cpd->target.s6_addr[0] & 7) == 0xfc) > ((pd[i].target.s6_addr[0] & 7) == 0xfc)
				|| cpd->preferred < pd[i].preferred)
			cpd = &pd[i];

	// Detect default router
	for (size_t i = 0; i < rtlen / sizeof(*rt); ++i)
		if (IN6_IS_ADDR_UNSPECIFIED(&rt[i].target) && (!crt || crt->priority > rt[i].priority))
			crt = &rt[i];

	struct sockaddr_ll dest = {
		.sll_family = AF_PACKET,
		.sll_protocol = htons(ETH_P_IPV6),
		.sll_ifindex = if_index,
		.sll_halen = ETH_ALEN,
	};

	if (crt) {
		struct {
			struct nlmsghdr hdr;
			struct ndmsg ndm;
		} req = {
			.hdr = {sizeof(req), RTM_GETNEIGH, NLM_F_REQUEST | NLM_F_DUMP, 1, 0},
			.ndm = {.ndm_family = AF_INET6, .ndm_ifindex = if_index}
		};
		send(rtnl, &req, sizeof(req), 0);

		uint8_t buf[8192];
		struct nlmsghdr *nhm;
		do {
			ssize_t read = recv(rtnl, buf, sizeof(buf), 0);
			nhm = (struct nlmsghdr*)buf;
			if ((read < 0 && errno == EINTR) || !NLMSG_OK(nhm, (size_t)read))
				continue;
			else if (read < 0)
				break;

			for (; read > 0 && NLMSG_OK(nhm, (size_t)read); nhm = NLMSG_NEXT(nhm, read)) {
				ssize_t attrlen = NLMSG_PAYLOAD(nhm, sizeof(struct ndmsg));
				if (nhm->nlmsg_type != RTM_NEWNEIGH || attrlen <= 0) {
					nhm = NULL;
					break;
				}

				// Already have our MAC
				if (crt_found)
					continue;

				struct ndmsg *ndm = NLMSG_DATA(nhm);
				for (struct rtattr *rta = (struct rtattr*)&ndm[1];
						attrlen > 0 && RTA_OK(rta, (size_t)attrlen);
						rta = RTA_NEXT(rta, attrlen)) {
					if (rta->rta_type == NDA_DST) {
						crt_found = IN6_ARE_ADDR_EQUAL(RTA_DATA(rta), &crt->router);
					} else if (rta->rta_type == NDA_LLADDR) {
						memcpy(dest.sll_addr, RTA_DATA(rta), ETH_ALEN);
					}
				}
			}
		} while (nhm);
	}

	if (!crt_found || !cpd)
		return;

	ping.ip6.ip6_src = cpd->target;
	ping.ip6.ip6_dst = cpd->target;

	struct sock_filter bpf[] = {
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct ip6_hdr, ip6_plen)),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 8 << 16 | IPPROTO_ICMPV6 << 8 | 254, 0, 13),
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct ip6_hdr, ip6_dst)),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ntohl(ping.ip6.ip6_dst.s6_addr32[0]), 0, 11),
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct ip6_hdr, ip6_dst) + 4),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ntohl(ping.ip6.ip6_dst.s6_addr32[1]), 0, 9),
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct ip6_hdr, ip6_dst) + 8),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ntohl(ping.ip6.ip6_dst.s6_addr32[2]), 0, 7),
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct ip6_hdr, ip6_dst) + 12),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ntohl(ping.ip6.ip6_dst.s6_addr32[3]), 0, 5),
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, sizeof(struct ip6_hdr) +
				offsetof(struct icmp6_hdr, icmp6_type)),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ICMP6_ECHO_REQUEST << 24, 0, 3),
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, sizeof(struct ip6_hdr) +
				offsetof(struct icmp6_hdr, icmp6_data32)),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ntohl(ping.icmp6.icmp6_data32[0]), 0, 1),
		BPF_STMT(BPF_RET | BPF_K, 0xffffffff),
		BPF_STMT(BPF_RET | BPF_K, 0),
	};
	struct sock_fprog bpf_prog = {sizeof(bpf) / sizeof(*bpf), bpf};


	if (sock < 0) {
		sock = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IPV6));
		fcntl(sock, F_SETFD, FD_CLOEXEC);
		bind(sock, (struct sockaddr*)&dest, sizeof(dest));

		fcntl(sock, F_SETOWN, getpid());
		fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_ASYNC);
	}

	setsockopt(sock, SOL_SOCKET, SO_DETACH_FILTER, &bpf_prog, sizeof(bpf_prog));
	if (setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &bpf_prog, sizeof(bpf_prog))) {
		close(sock);
		sock = -1;
		return;
	}

	uint8_t dummy[8];
	while (recv(sock, dummy, sizeof(dummy), MSG_DONTWAIT | MSG_TRUNC) > 0);

	sendto(sock, &ping, sizeof(ping), MSG_DONTWAIT,
			(struct sockaddr*)&dest, sizeof(dest));
}


void bfd_receive(void)
{
	uint8_t dummy[8];
	while (recv(sock, dummy, sizeof(dummy), MSG_DONTWAIT | MSG_TRUNC) > 0) {
		bfd_failed = 0;
		bfd_armed = true;
	}
}


int bfd_start(const char *ifname, int limit, int interval)
{
	if_index = if_nametoindex(ifname);
	bfd_armed = false;
	bfd_failed = 0;
	bfd_limit = limit;
	bfd_interval = interval;

	if (limit < 1 || interval < 1)
		return 0;

	rtnl = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	fcntl(rtnl, F_SETFD, FD_CLOEXEC);
	struct sockaddr_nl rtnl_kernel = { .nl_family = AF_NETLINK };
	connect(rtnl, (const struct sockaddr*)&rtnl_kernel, sizeof(rtnl_kernel));

	signal(SIGALRM, bfd_send);
	alarm(5);
	return 0;
}


void bfd_stop(void)
{
	alarm(0);
	close(sock);
	close(rtnl);

	sock = -1;
	rtnl = -1;
}
