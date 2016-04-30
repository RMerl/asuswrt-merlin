/*
 * Get mdb table with netlink
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/if_bridge.h>
#include <linux/if_ether.h>
#include <string.h>
#include <arpa/inet.h>

#include "libnetlink.h"
#include "br_common.h"
#include "rt_names.h"
#include "utils.h"

#ifndef MDBA_RTA
#define MDBA_RTA(r) \
	((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct br_port_msg))))
#endif

static unsigned int filter_index;

static void usage(void)
{
	fprintf(stderr, "Usage: bridge mdb { add | del } dev DEV port PORT grp GROUP [permanent | temp]\n");
	fprintf(stderr, "       bridge mdb {show} [ dev DEV ]\n");
	exit(-1);
}

static void br_print_router_ports(FILE *f, struct rtattr *attr)
{
	uint32_t *port_ifindex;
	struct rtattr *i;
	int rem;

	rem = RTA_PAYLOAD(attr);
	for (i = RTA_DATA(attr); RTA_OK(i, rem); i = RTA_NEXT(i, rem)) {
		port_ifindex = RTA_DATA(i);
		fprintf(f, "%s ", ll_index_to_name(*port_ifindex));
	}

	fprintf(f, "\n");
}

static void print_mdb_entry(FILE *f, int ifindex, struct br_mdb_entry *e)
{
	SPRINT_BUF(abuf);

	if (e->addr.proto == htons(ETH_P_IP))
		fprintf(f, "dev %s port %s grp %s %s\n", ll_index_to_name(ifindex),
			ll_index_to_name(e->ifindex),
			inet_ntop(AF_INET, &e->addr.u.ip4, abuf, sizeof(abuf)),
			(e->state & MDB_PERMANENT) ? "permanent" : "temp");
	else
		fprintf(f, "dev %s port %s grp %s %s\n", ll_index_to_name(ifindex),
			ll_index_to_name(e->ifindex),
			inet_ntop(AF_INET6, &e->addr.u.ip6, abuf, sizeof(abuf)),
			(e->state & MDB_PERMANENT) ? "permanent" : "temp");
}

static void br_print_mdb_entry(FILE *f, int ifindex, struct rtattr *attr)
{
	struct rtattr *i;
	int rem;
	struct br_mdb_entry *e;

	rem = RTA_PAYLOAD(attr);
	for (i = RTA_DATA(attr); RTA_OK(i, rem); i = RTA_NEXT(i, rem)) {
		e = RTA_DATA(i);
		print_mdb_entry(f, ifindex, e);
	}
}

int print_mdb(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	FILE *fp = arg;
	struct br_port_msg *r = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * tb[MDBA_MAX+1];

	if (n->nlmsg_type != RTM_GETMDB && n->nlmsg_type != RTM_NEWMDB && n->nlmsg_type != RTM_DELMDB) {
		fprintf(stderr, "Not RTM_GETMDB, RTM_NEWMDB or RTM_DELMDB: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);

		return 0;
	}

	len -= NLMSG_LENGTH(sizeof(*r));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	if (filter_index && filter_index != r->ifindex)
		return 0;

	parse_rtattr(tb, MDBA_MAX, MDBA_RTA(r), n->nlmsg_len - NLMSG_LENGTH(sizeof(*r)));

	if (tb[MDBA_MDB]) {
		struct rtattr *i;
		int rem = RTA_PAYLOAD(tb[MDBA_MDB]);

		for (i = RTA_DATA(tb[MDBA_MDB]); RTA_OK(i, rem); i = RTA_NEXT(i, rem))
			br_print_mdb_entry(fp, r->ifindex, i);
	}

	if (tb[MDBA_ROUTER]) {
		if (show_details) {
			fprintf(fp, "router ports on %s: ", ll_index_to_name(r->ifindex));
			br_print_router_ports(fp, tb[MDBA_ROUTER]);
		}
	}

	return 0;
}

static int mdb_show(int argc, char **argv)
{
	char *filter_dev = NULL;

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			if (filter_dev)
				duparg("dev", *argv);
			filter_dev = *argv;
		}
		argc--; argv++;
	}

	if (filter_dev) {
		filter_index = if_nametoindex(filter_dev);
		if (filter_index == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n",
				filter_dev);
			return -1;
		}
	}

	if (rtnl_wilddump_request(&rth, PF_BRIDGE, RTM_GETMDB) < 0) {
		perror("Cannot send dump request");
		exit(1);
	}

	if (rtnl_dump_filter(&rth, print_mdb, stdout) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}

	return 0;
}

static int mdb_modify(int cmd, int flags, int argc, char **argv)
{
	struct {
		struct nlmsghdr 	n;
		struct br_port_msg	bpm;
		char   			buf[1024];
	} req;
	struct br_mdb_entry entry;
	char *d = NULL, *p = NULL, *grp = NULL;

	memset(&req, 0, sizeof(req));
	memset(&entry, 0, sizeof(entry));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct br_port_msg));
	req.n.nlmsg_flags = NLM_F_REQUEST|flags;
	req.n.nlmsg_type = cmd;
	req.bpm.family = PF_BRIDGE;

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			d = *argv;
		} else if (strcmp(*argv, "grp") == 0) {
			NEXT_ARG();
			grp = *argv;
		} else if (strcmp(*argv, "port") == 0) {
			NEXT_ARG();
			p = *argv;
		} else if (strcmp(*argv, "permanent") == 0) {
			if (cmd == RTM_NEWMDB)
				entry.state |= MDB_PERMANENT;
		} else if (strcmp(*argv, "temp") == 0) {
			;/* nothing */
		} else {
			if (matches(*argv, "help") == 0)
				usage();
		}
		argc--; argv++;
	}

	if (d == NULL || grp == NULL || p == NULL) {
		fprintf(stderr, "Device, group address and port name are required arguments.\n");
		exit(-1);
	}

	req.bpm.ifindex = ll_name_to_index(d);
	if (req.bpm.ifindex == 0) {
		fprintf(stderr, "Cannot find device \"%s\"\n", d);
		return -1;
	}

	entry.ifindex = ll_name_to_index(p);
	if (entry.ifindex == 0) {
		fprintf(stderr, "Cannot find device \"%s\"\n", p);
		return -1;
	}

	if (!inet_pton(AF_INET, grp, &entry.addr.u.ip4)) {
		if (!inet_pton(AF_INET6, grp, &entry.addr.u.ip6)) {
			fprintf(stderr, "Invalid address \"%s\"\n", grp);
			return -1;
		} else
			entry.addr.proto = htons(ETH_P_IPV6);
	} else
		entry.addr.proto = htons(ETH_P_IP);

	addattr_l(&req.n, sizeof(req), MDBA_SET_ENTRY, &entry, sizeof(entry));

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL) < 0)
		exit(2);

	return 0;
}

int do_mdb(int argc, char **argv)
{
	ll_init_map(&rth);

	if (argc > 0) {
		if (matches(*argv, "add") == 0)
			return mdb_modify(RTM_NEWMDB, NLM_F_CREATE|NLM_F_EXCL, argc-1, argv+1);
		if (matches(*argv, "delete") == 0)
			return mdb_modify(RTM_DELMDB, 0, argc-1, argv+1);

		if (matches(*argv, "show") == 0 ||
		    matches(*argv, "lst") == 0 ||
		    matches(*argv, "list") == 0)
			return mdb_show(argc-1, argv+1);
		if (matches(*argv, "help") == 0)
			usage();
	} else
		return mdb_show(0, NULL);

	fprintf(stderr, "Command \"%s\" is unknown, try \"bridge mdb help\".\n", *argv);
	exit(-1);
}
