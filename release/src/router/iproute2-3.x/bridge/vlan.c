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

#include "libnetlink.h"
#include "br_common.h"
#include "utils.h"

static unsigned int filter_index;

static void usage(void)
{
	fprintf(stderr, "Usage: bridge vlan { add | del } vid VLAN_ID dev DEV [ pvid] [ untagged ]\n");
	fprintf(stderr, "                                                     [ self ] [ master ]\n");
	fprintf(stderr, "       bridge vlan { show } [ dev DEV ]\n");
	exit(-1);
}

static int vlan_modify(int cmd, int argc, char **argv)
{
	struct {
		struct nlmsghdr 	n;
		struct ifinfomsg 	ifm;
		char   			buf[1024];
	} req;
	char *d = NULL;
	short vid = -1;
	struct rtattr *afspec;
	struct bridge_vlan_info vinfo;
	unsigned short flags = 0;

	memset(&vinfo, 0, sizeof(vinfo));
	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = cmd;
	req.ifm.ifi_family = PF_BRIDGE;

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			d = *argv;
		} else if (strcmp(*argv, "vid") == 0) {
			NEXT_ARG();
			vid = atoi(*argv);
		} else if (strcmp(*argv, "self") == 0) {
			flags |= BRIDGE_FLAGS_SELF;
		} else if (strcmp(*argv, "master") == 0) {
			flags |= BRIDGE_FLAGS_MASTER;
		} else if (strcmp(*argv, "pvid") == 0) {
			vinfo.flags |= BRIDGE_VLAN_INFO_PVID;
		} else if (strcmp(*argv, "untagged") == 0) {
			vinfo.flags |= BRIDGE_VLAN_INFO_UNTAGGED;
		} else {
			if (matches(*argv, "help") == 0) {
				NEXT_ARG();
			}
		}
		argc--; argv++;
	}

	if (d == NULL || vid == -1) {
		fprintf(stderr, "Device and VLAN ID are required arguments.\n");
		exit(-1);
	}

	req.ifm.ifi_index = ll_name_to_index(d);
	if (req.ifm.ifi_index == 0) {
		fprintf(stderr, "Cannot find bridge device \"%s\"\n", d);
		return -1;
	}

	if (vid >= 4096) {
		fprintf(stderr, "Invalid VLAN ID \"%hu\"\n", vid);
		return -1;
	}

	vinfo.vid = vid;

	afspec = addattr_nest(&req.n, sizeof(req), IFLA_AF_SPEC);

	if (flags)
		addattr16(&req.n, sizeof(req), IFLA_BRIDGE_FLAGS, flags);

	addattr_l(&req.n, sizeof(req), IFLA_BRIDGE_VLAN_INFO, &vinfo,
		  sizeof(vinfo));

	addattr_nest_end(&req.n, afspec);

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL) < 0)
		exit(2);

	return 0;
}

static int print_vlan(const struct sockaddr_nl *who,
		      struct nlmsghdr *n,
		      void *arg)
{
	FILE *fp = arg;
	struct ifinfomsg *ifm = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * tb[IFLA_MAX+1];

	if (n->nlmsg_type != RTM_NEWLINK) {
		fprintf(stderr, "Not RTM_NEWLINK: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
		return 0;
	}

	len -= NLMSG_LENGTH(sizeof(*ifm));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	if (ifm->ifi_family != AF_BRIDGE)
		return 0;

	if (filter_index && filter_index != ifm->ifi_index)
		return 0;

	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifm), len);

	/* if AF_SPEC isn't there, vlan table is not preset for this port */
	if (!tb[IFLA_AF_SPEC]) {
		fprintf(fp, "%s\tNone\n", ll_index_to_name(ifm->ifi_index));
		return 0;
	} else {
		struct rtattr *i, *list = tb[IFLA_AF_SPEC];
		int rem = RTA_PAYLOAD(list);

		fprintf(fp, "%s", ll_index_to_name(ifm->ifi_index));
		for (i = RTA_DATA(list); RTA_OK(i, rem); i = RTA_NEXT(i, rem)) {
			struct bridge_vlan_info *vinfo;

			if (i->rta_type != IFLA_BRIDGE_VLAN_INFO)
				continue;

			vinfo = RTA_DATA(i);
			fprintf(fp, "\t %hu", vinfo->vid);
			if (vinfo->flags & BRIDGE_VLAN_INFO_PVID)
				fprintf(fp, " PVID");
			if (vinfo->flags & BRIDGE_VLAN_INFO_UNTAGGED)
				fprintf(fp, " Egress Untagged");
			fprintf(fp, "\n");
		}
	}
	fprintf(fp, "\n");
	fflush(fp);
	return 0;
}

static int vlan_show(int argc, char **argv)
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
		if ((filter_index = if_nametoindex(filter_dev)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n",
			       filter_dev);
			return -1;
		}
	}

	if (rtnl_wilddump_req_filter(&rth, PF_BRIDGE, RTM_GETLINK,
				     RTEXT_FILTER_BRVLAN) < 0) {
		perror("Cannont send dump request");
		exit(1);
	}

	printf("port\tvlan ids\n");
	if (rtnl_dump_filter(&rth, print_vlan, stdout) < 0) {
		fprintf(stderr, "Dump ternminated\n");
		exit(1);
	}

	return 0;
}


int do_vlan(int argc, char **argv)
{
	ll_init_map(&rth);

	if (argc > 0) {
		if (matches(*argv, "add") == 0)
			return vlan_modify(RTM_SETLINK, argc-1, argv+1);
		if (matches(*argv, "delete") == 0)
			return vlan_modify(RTM_DELLINK, argc-1, argv+1);
		if (matches(*argv, "show") == 0 ||
		    matches(*argv, "lst") == 0 ||
		    matches(*argv, "list") == 0)
			return vlan_show(argc-1, argv+1);
		if (matches(*argv, "help") == 0)
			usage();
	} else
		return vlan_show(0, NULL);

	fprintf(stderr, "Command \"%s\" is unknown, try \"bridge fdb help\".\n", *argv);
	exit(-1);
}
