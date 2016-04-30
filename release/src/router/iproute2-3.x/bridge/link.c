
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <linux/if_bridge.h>
#include <string.h>
#include <stdbool.h>

#include "libnetlink.h"
#include "utils.h"
#include "br_common.h"

static unsigned int filter_index;

static const char *port_states[] = {
	[BR_STATE_DISABLED] = "disabled",
	[BR_STATE_LISTENING] = "listening",
	[BR_STATE_LEARNING] = "learning",
	[BR_STATE_FORWARDING] = "forwarding",
	[BR_STATE_BLOCKING] = "blocking",
};

extern char *if_indextoname (unsigned int __ifindex, char *__ifname);

static void print_link_flags(FILE *fp, unsigned flags)
{
	fprintf(fp, "<");
	if (flags & IFF_UP && !(flags & IFF_RUNNING))
		fprintf(fp, "NO-CARRIER%s", flags ? "," : "");
	flags &= ~IFF_RUNNING;
#define _PF(f) if (flags&IFF_##f) { \
                  flags &= ~IFF_##f ; \
                  fprintf(fp, #f "%s", flags ? "," : ""); }
	_PF(LOOPBACK);
	_PF(BROADCAST);
	_PF(POINTOPOINT);
	_PF(MULTICAST);
	_PF(NOARP);
	_PF(ALLMULTI);
	_PF(PROMISC);
	_PF(MASTER);
	_PF(SLAVE);
	_PF(DEBUG);
	_PF(DYNAMIC);
	_PF(AUTOMEDIA);
	_PF(PORTSEL);
	_PF(NOTRAILERS);
	_PF(UP);
	_PF(LOWER_UP);
	_PF(DORMANT);
	_PF(ECHO);
#undef _PF
        if (flags)
		fprintf(fp, "%x", flags);
	fprintf(fp, "> ");
}

static const char *oper_states[] = {
	"UNKNOWN", "NOTPRESENT", "DOWN", "LOWERLAYERDOWN",
	"TESTING", "DORMANT",	 "UP"
};

static const char *hw_mode[] = {"VEB", "VEPA"};

static void print_operstate(FILE *f, __u8 state)
{
	if (state >= sizeof(oper_states)/sizeof(oper_states[0]))
		fprintf(f, "state %#x ", state);
	else
		fprintf(f, "state %s ", oper_states[state]);
}

static void print_portstate(FILE *f, __u8 state)
{
	if (state <= BR_STATE_BLOCKING)
		fprintf(f, "state %s ", port_states[state]);
	else
		fprintf(f, "state (%d) ", state);
}

static void print_onoff(FILE *f, char *flag, __u8 val)
{
	fprintf(f, "%s %s ", flag, val ? "on" : "off");
}

static void print_hwmode(FILE *f, __u16 mode)
{
	if (mode >= sizeof(hw_mode)/sizeof(hw_mode[0]))
		fprintf(f, "hwmode %#hx ", mode);
	else
		fprintf(f, "hwmode %s ", hw_mode[mode]);
}

int print_linkinfo(const struct sockaddr_nl *who,
		   struct nlmsghdr *n, void *arg)
{
	FILE *fp = arg;
	int len = n->nlmsg_len;
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct rtattr * tb[IFLA_MAX+1];
	char b1[IFNAMSIZ];

	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0) {
		fprintf(stderr, "Message too short!\n");
		return -1;
        }

	if (!(ifi->ifi_family == AF_BRIDGE || ifi->ifi_family == AF_UNSPEC))
		return 0;

	if (filter_index && filter_index != ifi->ifi_index)
		return 0;

	parse_rtattr_flags(tb, IFLA_MAX, IFLA_RTA(ifi), len, NLA_F_NESTED);

	if (tb[IFLA_IFNAME] == NULL) {
		fprintf(stderr, "BUG: nil ifname\n");
		return -1;
	}

	if (n->nlmsg_type == RTM_DELLINK)
		fprintf(fp, "Deleted ");

	fprintf(fp, "%d: %s ", ifi->ifi_index,
		tb[IFLA_IFNAME] ? rta_getattr_str(tb[IFLA_IFNAME]) : "<nil>");

	if (tb[IFLA_OPERSTATE])
		print_operstate(fp, rta_getattr_u8(tb[IFLA_OPERSTATE]));

	if (tb[IFLA_LINK]) {
		SPRINT_BUF(b1);
		int iflink = rta_getattr_u32(tb[IFLA_LINK]);
		if (iflink == 0)
			fprintf(fp, "@NONE: ");
		else
			fprintf(fp, "@%s: ",
				if_indextoname(iflink, b1));
	} else
		fprintf(fp, ": ");

	print_link_flags(fp, ifi->ifi_flags);

	if (tb[IFLA_MTU])
		fprintf(fp, "mtu %u ", rta_getattr_u32(tb[IFLA_MTU]));

	if (tb[IFLA_MASTER])
		fprintf(fp, "master %s ",
			if_indextoname(rta_getattr_u32(tb[IFLA_MASTER]), b1));

	if (tb[IFLA_PROTINFO]) {
		if (tb[IFLA_PROTINFO]->rta_type & NLA_F_NESTED) {
			struct rtattr *prtb[IFLA_BRPORT_MAX+1];

			parse_rtattr_nested(prtb, IFLA_BRPORT_MAX,
					    tb[IFLA_PROTINFO]);

			if (prtb[IFLA_BRPORT_STATE])
				print_portstate(fp,
						rta_getattr_u8(prtb[IFLA_BRPORT_STATE]));
			if (prtb[IFLA_BRPORT_PRIORITY])
				fprintf(fp, "priority %hu ",
					rta_getattr_u16(prtb[IFLA_BRPORT_PRIORITY]));
			if (prtb[IFLA_BRPORT_COST])
				fprintf(fp, "cost %u ",
					rta_getattr_u32(prtb[IFLA_BRPORT_COST]));

			if (show_details) {
				fprintf(fp, "%s    ", _SL_);

				if (prtb[IFLA_BRPORT_MODE])
					print_onoff(fp, "hairpin",
						    rta_getattr_u8(prtb[IFLA_BRPORT_MODE]));
				if (prtb[IFLA_BRPORT_GUARD])
					print_onoff(fp, "guard",
						    rta_getattr_u8(prtb[IFLA_BRPORT_GUARD]));
				if (prtb[IFLA_BRPORT_PROTECT])
					print_onoff(fp, "root_block",
						    rta_getattr_u8(prtb[IFLA_BRPORT_PROTECT]));
				if (prtb[IFLA_BRPORT_FAST_LEAVE])
					print_onoff(fp, "fastleave",
						    rta_getattr_u8(prtb[IFLA_BRPORT_FAST_LEAVE]));
				if (prtb[IFLA_BRPORT_LEARNING])
					print_onoff(fp, "learning",
						    rta_getattr_u8(prtb[IFLA_BRPORT_LEARNING]));
				if (prtb[IFLA_BRPORT_LEARNING_SYNC])
					print_onoff(fp, "learning_sync",
						    rta_getattr_u8(prtb[IFLA_BRPORT_LEARNING_SYNC]));
				if (prtb[IFLA_BRPORT_UNICAST_FLOOD])
					print_onoff(fp, "flood",
						    rta_getattr_u8(prtb[IFLA_BRPORT_UNICAST_FLOOD]));
			}
		} else
			print_portstate(fp, rta_getattr_u8(tb[IFLA_PROTINFO]));
	}

	if (tb[IFLA_AF_SPEC]) {
		/* This is reported by HW devices that have some bridging
		 * capabilities.
		 */
		struct rtattr *aftb[IFLA_BRIDGE_MAX+1];

		parse_rtattr_nested(aftb, IFLA_BRIDGE_MAX, tb[IFLA_AF_SPEC]);

		if (aftb[IFLA_BRIDGE_MODE])
			print_hwmode(fp, rta_getattr_u16(aftb[IFLA_BRIDGE_MODE]));
	}

	fprintf(fp, "\n");
	fflush(fp);
	return 0;
}

static void usage(void)
{
	fprintf(stderr, "Usage: bridge link set dev DEV [ cost COST ] [ priority PRIO ] [ state STATE ]\n");
	fprintf(stderr, "                               [ guard {on | off} ]\n");
	fprintf(stderr, "                               [ hairpin {on | off} ] \n");
	fprintf(stderr, "                               [ fastleave {on | off} ]\n");
	fprintf(stderr,	"                               [ root_block {on | off} ]\n");
	fprintf(stderr,	"                               [ learning {on | off} ]\n");
	fprintf(stderr,	"                               [ learning_sync {on | off} ]\n");
	fprintf(stderr,	"                               [ flood {on | off} ]\n");
	fprintf(stderr, "                               [ hwmode {vepa | veb} ]\n");
	fprintf(stderr, "       bridge link show [dev DEV]\n");
	exit(-1);
}

static bool on_off(char *arg, __s8 *attr, char *val)
{
	if (strcmp(val, "on") == 0)
		*attr = 1;
	else if (strcmp(val, "off") == 0)
		*attr = 0;
	else {
		fprintf(stderr,
			"Error: argument of \"%s\" must be \"on\" or \"off\"\n",
			arg);
		return false;
	}

	return true;
}

static int brlink_modify(int argc, char **argv)
{
	struct {
		struct nlmsghdr  n;
		struct ifinfomsg ifm;
		char             buf[512];
	} req;
	char *d = NULL;
	__s8 learning = -1;
	__s8 learning_sync = -1;
	__s8 flood = -1;
	__s8 hairpin = -1;
	__s8 bpdu_guard = -1;
	__s8 fast_leave = -1;
	__s8 root_block = -1;
	__u32 cost = 0;
	__s16 priority = -1;
	__s8 state = -1;
	__s16 mode = -1;
	__u16 flags = 0;
	struct rtattr *nest;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_SETLINK;
	req.ifm.ifi_family = PF_BRIDGE;

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			d = *argv;
		} else if (strcmp(*argv, "guard") == 0) {
			NEXT_ARG();
			if (!on_off("guard", &bpdu_guard, *argv))
				exit(-1);
		} else if (strcmp(*argv, "hairpin") == 0) {
			NEXT_ARG();
			if (!on_off("hairping", &hairpin, *argv))
				exit(-1);
		} else if (strcmp(*argv, "fastleave") == 0) {
			NEXT_ARG();
			if (!on_off("fastleave", &fast_leave, *argv))
				exit(-1);
		} else if (strcmp(*argv, "root_block") == 0) {
			NEXT_ARG();
			if (!on_off("root_block", &root_block, *argv))
				exit(-1);
		} else if (strcmp(*argv, "learning") == 0) {
			NEXT_ARG();
			if (!on_off("learning", &learning, *argv))
				exit(-1);
		} else if (strcmp(*argv, "learning_sync") == 0) {
			NEXT_ARG();
			if (!on_off("learning_sync", &learning_sync, *argv))
				exit(-1);
		} else if (strcmp(*argv, "flood") == 0) {
			NEXT_ARG();
			if (!on_off("flood", &flood, *argv))
				exit(-1);
		} else if (strcmp(*argv, "cost") == 0) {
			NEXT_ARG();
			cost = atoi(*argv);
		} else if (strcmp(*argv, "priority") == 0) {
			NEXT_ARG();
			priority = atoi(*argv);
		} else if (strcmp(*argv, "state") == 0) {
			NEXT_ARG();
			state = atoi(*argv);
		} else if (strcmp(*argv, "hwmode") == 0) {
			NEXT_ARG();
			flags = BRIDGE_FLAGS_SELF;
			if (strcmp(*argv, "vepa") == 0)
				mode = BRIDGE_MODE_VEPA;
			else if (strcmp(*argv, "veb") == 0)
				mode = BRIDGE_MODE_VEB;
			else {
				fprintf(stderr,
					"Mode argument must be \"vepa\" or "
					"\"veb\".\n");
				exit(-1);
			}
		} else if (strcmp(*argv, "self") == 0) {
			flags = BRIDGE_FLAGS_SELF;
		} else {
			usage();
		}
		argc--; argv++;
	}
	if (d == NULL) {
		fprintf(stderr, "Device is a required argument.\n");
		exit(-1);
	}


	req.ifm.ifi_index = ll_name_to_index(d);
	if (req.ifm.ifi_index == 0) {
		fprintf(stderr, "Cannot find bridge device \"%s\"\n", d);
		exit(-1);
	}

	/* Nested PROTINFO attribute.  Contains: port flags, cost, priority and
	 * state.
	 */
	nest = addattr_nest(&req.n, sizeof(req),
			    IFLA_PROTINFO | NLA_F_NESTED);
	/* Flags first */
	if (bpdu_guard >= 0)
		addattr8(&req.n, sizeof(req), IFLA_BRPORT_GUARD, bpdu_guard);
	if (hairpin >= 0)
		addattr8(&req.n, sizeof(req), IFLA_BRPORT_MODE, hairpin);
	if (fast_leave >= 0)
		addattr8(&req.n, sizeof(req), IFLA_BRPORT_FAST_LEAVE,
			 fast_leave);
	if (root_block >= 0)
		addattr8(&req.n, sizeof(req), IFLA_BRPORT_PROTECT, root_block);
	if (flood >= 0)
		addattr8(&req.n, sizeof(req), IFLA_BRPORT_UNICAST_FLOOD, flood);
	if (learning >= 0)
		addattr8(&req.n, sizeof(req), IFLA_BRPORT_LEARNING, learning);
	if (learning_sync >= 0)
		addattr8(&req.n, sizeof(req), IFLA_BRPORT_LEARNING_SYNC,
			 learning_sync);

	if (cost > 0)
		addattr32(&req.n, sizeof(req), IFLA_BRPORT_COST, cost);

	if (priority >= 0)
		addattr16(&req.n, sizeof(req), IFLA_BRPORT_PRIORITY, priority);

	if (state >= 0)
		addattr8(&req.n, sizeof(req), IFLA_BRPORT_STATE, state);

	addattr_nest_end(&req.n, nest);

	/* IFLA_AF_SPEC nested attribute. Contains IFLA_BRIDGE_FLAGS that
	 * designates master or self operation and IFLA_BRIDGE_MODE
	 * for hw 'vepa' or 'veb' operation modes. The hwmodes are
	 * only valid in 'self' mode on some devices so far.
	 */
	if (mode >= 0 || flags > 0) {
		nest = addattr_nest(&req.n, sizeof(req), IFLA_AF_SPEC);

		if (flags > 0)
			addattr16(&req.n, sizeof(req), IFLA_BRIDGE_FLAGS, flags);

		if (mode >= 0)
			addattr16(&req.n, sizeof(req), IFLA_BRIDGE_MODE, mode);

		addattr_nest_end(&req.n, nest);
	}

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL) < 0)
		exit(2);

	return 0;
}

static int brlink_show(int argc, char **argv)
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
		if ((filter_index = ll_name_to_index(filter_dev)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n",
				filter_dev);
			return -1;
		}
	}

	if (rtnl_wilddump_request(&rth, PF_BRIDGE, RTM_GETLINK) < 0) {
		perror("Cannon send dump request");
		exit(1);
	}

	if (rtnl_dump_filter(&rth, print_linkinfo, stdout) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}
	return 0;
}

int do_link(int argc, char **argv)
{
	ll_init_map(&rth);
	if (argc > 0) {
		if (matches(*argv, "set") == 0 ||
		    matches(*argv, "change") == 0)
			return brlink_modify(argc-1, argv+1);
		if (matches(*argv, "show") == 0 ||
		    matches(*argv, "lst") == 0 ||
		    matches(*argv, "list") == 0)
			return brlink_show(argc-1, argv+1);
		if (matches(*argv, "help") == 0)
			usage();
	} else
		return brlink_show(0, NULL);

	fprintf(stderr, "Command \"%s\" is unknown, try \"bridge link help\".\n", *argv);
	exit(-1);
}
