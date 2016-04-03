/* This example is placed in the public domain. */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <net/if.h>

#include <libmnl/libmnl.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>

int main(int argc, char *argv[])
{
	if (argc <= 3) {
		printf("Usage: %s iface destination cidr [gateway]\n", argv[0]);
		printf("Example: %s eth0 10.0.1.12 32 10.0.1.11\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int iface;
	iface = if_nametoindex(argv[1]);
	if (iface == 0) {
		printf("Bad interface name\n");
		exit(EXIT_FAILURE);
	}

	in_addr_t dst;
	if (!inet_pton(AF_INET, argv[2], &dst)) {
		printf("Bad destination\n");
		exit(EXIT_FAILURE);
	}

	uint32_t mask;
	if (sscanf(argv[3], "%u", &mask) == 0) {
		printf("Bad CIDR\n");
		exit(EXIT_FAILURE);
	}

	in_addr_t gw;
	if (argc >= 5 && !inet_pton(AF_INET, argv[4], &gw)) {
		printf("Bad gateway\n");
		exit(EXIT_FAILURE);
	}

	struct mnl_socket *nl;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct rtmsg *rtm;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWROUTE;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
	nlh->nlmsg_seq = time(NULL);

	rtm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtmsg));
	rtm->rtm_family = AF_INET;
	rtm->rtm_dst_len = mask;
	rtm->rtm_src_len = 0;
	rtm->rtm_tos = 0;
	rtm->rtm_protocol = RTPROT_BOOT;
	rtm->rtm_table = RT_TABLE_MAIN;
	rtm->rtm_type = RTN_UNICAST;
	/* is there any gateway? */
	rtm->rtm_scope = (argc == 4) ? RT_SCOPE_LINK : RT_SCOPE_UNIVERSE;
	rtm->rtm_flags = 0;

	mnl_attr_put_u32(nlh, RTA_DST, dst);
	mnl_attr_put_u32(nlh, RTA_OIF, iface);
	if (argc >= 5)
		mnl_attr_put_u32(nlh, RTA_GATEWAY, gw);

	nl = mnl_socket_open(NETLINK_ROUTE);
	if (nl == NULL) {
		perror("mnl_socket_open");
		exit(EXIT_FAILURE);
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		exit(EXIT_FAILURE);
	}

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_send");
		exit(EXIT_FAILURE);
	}

	mnl_socket_close(nl);

	return 0;
}
