/*
 * tc_filter.c		"tc filter".
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <linux/if_ether.h>

#include "rt_names.h"
#include "utils.h"
#include "tc_util.h"
#include "tc_common.h"

static void usage(void);

static void usage(void)
{
	fprintf(stderr, "Usage: tc filter [ add | del | change | replace | show ] dev STRING\n");
	fprintf(stderr, "       [ pref PRIO ] protocol PROTO\n");
	fprintf(stderr, "       [ estimator INTERVAL TIME_CONSTANT ]\n");
	fprintf(stderr, "       [ root | classid CLASSID ] [ handle FILTERID ]\n");
	fprintf(stderr, "       [ [ FILTER_TYPE ] [ help | OPTIONS ] ]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "       tc filter show [ dev STRING ] [ root | parent CLASSID ]\n");
	fprintf(stderr, "Where:\n");
	fprintf(stderr, "FILTER_TYPE := { rsvp | u32 | fw | route | etc. }\n");
	fprintf(stderr, "FILTERID := ... format depends on classifier, see there\n");
	fprintf(stderr, "OPTIONS := ... try tc filter add <desired FILTER_KIND> help\n");
	return;
}


int tc_filter_modify(int cmd, unsigned flags, int argc, char **argv)
{
	struct {
		struct nlmsghdr 	n;
		struct tcmsg 		t;
		char   			buf[MAX_MSG];
	} req;
	struct filter_util *q = NULL;
	__u32 prio = 0;
	__u32 protocol = 0;
	int protocol_set = 0;
	char *fhandle = NULL;
	char  d[16];
	char  k[16];
	struct tc_estimator est;

	memset(&req, 0, sizeof(req));
	memset(&est, 0, sizeof(est));
	memset(d, 0, sizeof(d));
	memset(k, 0, sizeof(k));
	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST|flags;
	req.n.nlmsg_type = cmd;
	req.t.tcm_family = AF_UNSPEC;

	if (cmd == RTM_NEWTFILTER && flags & NLM_F_CREATE)
		protocol = htons(ETH_P_ALL);

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			if (d[0])
				duparg("dev", *argv);
			strncpy(d, *argv, sizeof(d)-1);
		} else if (strcmp(*argv, "root") == 0) {
			if (req.t.tcm_parent) {
				fprintf(stderr, "Error: \"root\" is duplicate parent ID\n");
				return -1;
			}
			req.t.tcm_parent = TC_H_ROOT;
		} else if (strcmp(*argv, "parent") == 0) {
			__u32 handle;
			NEXT_ARG();
			if (req.t.tcm_parent)
				duparg("parent", *argv);
			if (get_tc_classid(&handle, *argv))
				invarg(*argv, "Invalid parent ID");
			req.t.tcm_parent = handle;
		} else if (strcmp(*argv, "handle") == 0) {
			NEXT_ARG();
			if (fhandle)
				duparg("handle", *argv);
			fhandle = *argv;
		} else if (matches(*argv, "preference") == 0 ||
			   matches(*argv, "priority") == 0) {
			NEXT_ARG();
			if (prio)
				duparg("priority", *argv);
			if (get_u32(&prio, *argv, 0))
				invarg(*argv, "invalid priority value");
		} else if (matches(*argv, "protocol") == 0) {
			__u16 id;
			NEXT_ARG();
			if (protocol_set)
				duparg("protocol", *argv);
			if (ll_proto_a2n(&id, *argv))
				invarg(*argv, "invalid protocol");
			protocol = id;
			protocol_set = 1;
		} else if (matches(*argv, "estimator") == 0) {
			if (parse_estimator(&argc, &argv, &est) < 0)
				return -1;
		} else if (matches(*argv, "help") == 0) {
			usage();
			return 0;
		} else {
			strncpy(k, *argv, sizeof(k)-1);

			q = get_filter_kind(k);
			argc--; argv++;
			break;
		}

		argc--; argv++;
	}

	req.t.tcm_info = TC_H_MAKE(prio<<16, protocol);

	if (k[0])
		addattr_l(&req.n, sizeof(req), TCA_KIND, k, strlen(k)+1);

	if (q) {
		if (q->parse_fopt(q, fhandle, argc, argv, &req.n))
			return 1;
	} else {
		if (fhandle) {
			fprintf(stderr, "Must specify filter type when using "
				"\"handle\"\n");
			return -1;
		}
		if (argc) {
			if (matches(*argv, "help") == 0)
				usage();
			fprintf(stderr, "Garbage instead of arguments \"%s ...\". Try \"tc filter help\".\n", *argv);
			return -1;
		}
	}
	if (est.ewma_log)
		addattr_l(&req.n, sizeof(req), TCA_RATE, &est, sizeof(est));


	if (d[0])  {
 		ll_init_map(&rth);

		if ((req.t.tcm_ifindex = ll_name_to_index(d)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", d);
			return 1;
		}
	}

 	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
		fprintf(stderr, "We have an error talking to the kernel\n");
		return 2;
	}

	return 0;
}

static __u32 filter_parent;
static int filter_ifindex;
static __u32 filter_prio;
static __u32 filter_protocol;
__u16 f_proto = 0;

int print_filter(const struct sockaddr_nl *who,
			struct nlmsghdr *n,
			void *arg)
{
	FILE *fp = (FILE*)arg;
	struct tcmsg *t = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * tb[TCA_MAX+1];
	struct filter_util *q;
	char abuf[256];

	if (n->nlmsg_type != RTM_NEWTFILTER && n->nlmsg_type != RTM_DELTFILTER) {
		fprintf(stderr, "Not a filter\n");
		return 0;
	}
	len -= NLMSG_LENGTH(sizeof(*t));
	if (len < 0) {
		fprintf(stderr, "Wrong len %d\n", len);
		return -1;
	}

	memset(tb, 0, sizeof(tb));
	parse_rtattr(tb, TCA_MAX, TCA_RTA(t), len);

	if (tb[TCA_KIND] == NULL) {
		fprintf(stderr, "print_filter: NULL kind\n");
		return -1;
	}

	if (n->nlmsg_type == RTM_DELTFILTER)
		fprintf(fp, "deleted ");

	fprintf(fp, "filter ");
	if (!filter_ifindex || filter_ifindex != t->tcm_ifindex)
		fprintf(fp, "dev %s ", ll_index_to_name(t->tcm_ifindex));

	if (!filter_parent || filter_parent != t->tcm_parent) {
		if (t->tcm_parent == TC_H_ROOT)
			fprintf(fp, "root ");
		else {
			print_tc_classid(abuf, sizeof(abuf), t->tcm_parent);
			fprintf(fp, "parent %s ", abuf);
		}
	}
	if (t->tcm_info) {
		f_proto = TC_H_MIN(t->tcm_info);
		__u32 prio = TC_H_MAJ(t->tcm_info)>>16;
		if (!filter_protocol || filter_protocol != f_proto) {
			if (f_proto) {
				SPRINT_BUF(b1);
				fprintf(fp, "protocol %s ",
					ll_proto_n2a(f_proto, b1, sizeof(b1)));
			}
		}
		if (!filter_prio || filter_prio != prio) {
			if (prio)
				fprintf(fp, "pref %u ", prio);
		}
	}
	fprintf(fp, "%s ", (char*)RTA_DATA(tb[TCA_KIND]));
	q = get_filter_kind(RTA_DATA(tb[TCA_KIND]));
	if (tb[TCA_OPTIONS]) {
		if (q)
			q->print_fopt(q, fp, tb[TCA_OPTIONS], t->tcm_handle);
		else
			fprintf(fp, "[cannot parse parameters]");
	}
	fprintf(fp, "\n");

	if (show_stats && (tb[TCA_STATS] || tb[TCA_STATS2])) {
		print_tcstats_attr(fp, tb, " ", NULL);
		fprintf(fp, "\n");
	}

	fflush(fp);
	return 0;
}


int tc_filter_list(int argc, char **argv)
{
	struct tcmsg t;
	char d[16];
	__u32 prio = 0;
	__u32 protocol = 0;
	char *fhandle = NULL;

	memset(&t, 0, sizeof(t));
	t.tcm_family = AF_UNSPEC;
	memset(d, 0, sizeof(d));

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			if (d[0])
				duparg("dev", *argv);
			strncpy(d, *argv, sizeof(d)-1);
		} else if (strcmp(*argv, "root") == 0) {
			if (t.tcm_parent) {
				fprintf(stderr, "Error: \"root\" is duplicate parent ID\n");
				return -1;
			}
			filter_parent = t.tcm_parent = TC_H_ROOT;
		} else if (strcmp(*argv, "parent") == 0) {
			__u32 handle;
			NEXT_ARG();
			if (t.tcm_parent)
				duparg("parent", *argv);
			if (get_tc_classid(&handle, *argv))
				invarg(*argv, "invalid parent ID");
			filter_parent = t.tcm_parent = handle;
		} else if (strcmp(*argv, "handle") == 0) {
			NEXT_ARG();
			if (fhandle)
				duparg("handle", *argv);
			fhandle = *argv;
		} else if (matches(*argv, "preference") == 0 ||
			   matches(*argv, "priority") == 0) {
			NEXT_ARG();
			if (prio)
				duparg("priority", *argv);
			if (get_u32(&prio, *argv, 0))
				invarg(*argv, "invalid preference");
			filter_prio = prio;
		} else if (matches(*argv, "protocol") == 0) {
			__u16 res;
			NEXT_ARG();
			if (protocol)
				duparg("protocol", *argv);
			if (ll_proto_a2n(&res, *argv))
				invarg(*argv, "invalid protocol");
			protocol = res;
			filter_protocol = protocol;
		} else if (matches(*argv, "help") == 0) {
			usage();
		} else {
			fprintf(stderr, " What is \"%s\"? Try \"tc filter help\"\n", *argv);
			return -1;
		}

		argc--; argv++;
	}

	t.tcm_info = TC_H_MAKE(prio<<16, protocol);

 	ll_init_map(&rth);

	if (d[0]) {
		if ((t.tcm_ifindex = ll_name_to_index(d)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", d);
			return 1;
		}
		filter_ifindex = t.tcm_ifindex;
	}

 	if (rtnl_dump_request(&rth, RTM_GETTFILTER, &t, sizeof(t)) < 0) {
		perror("Cannot send dump request");
		return 1;
	}

 	if (rtnl_dump_filter(&rth, print_filter, stdout, NULL, NULL) < 0) {
		fprintf(stderr, "Dump terminated\n");
		return 1;
	}

	return 0;
}

int do_filter(int argc, char **argv)
{
	if (argc < 1)
		return tc_filter_list(0, NULL);
	if (matches(*argv, "add") == 0)
		return tc_filter_modify(RTM_NEWTFILTER, NLM_F_EXCL|NLM_F_CREATE, argc-1, argv+1);
	if (matches(*argv, "change") == 0)
		return tc_filter_modify(RTM_NEWTFILTER, 0, argc-1, argv+1);
	if (matches(*argv, "replace") == 0)
		return tc_filter_modify(RTM_NEWTFILTER, NLM_F_CREATE, argc-1, argv+1);
	if (matches(*argv, "delete") == 0)
		return tc_filter_modify(RTM_DELTFILTER, 0,  argc-1, argv+1);
#if 0
	if (matches(*argv, "get") == 0)
		return tc_filter_get(RTM_GETTFILTER, 0,  argc-1, argv+1);
#endif
	if (matches(*argv, "list") == 0 || matches(*argv, "show") == 0
	    || matches(*argv, "lst") == 0)
		return tc_filter_list(argc-1, argv+1);
	if (matches(*argv, "help") == 0) {
		usage();
		return 0;
        }
	fprintf(stderr, "Command \"%s\" is unknown, try \"tc filter help\".\n", *argv);
	return -1;
}

