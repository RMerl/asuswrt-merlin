/*
 * tc_qdisc.c		"tc qdisc".
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *		J Hadi Salim: Extension to ingress
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
#include <math.h>
#include <malloc.h>

#include "utils.h"
#include "tc_util.h"
#include "tc_common.h"

static int usage(void);

static int usage(void)
{
	fprintf(stderr, "Usage: tc qdisc [ add | del | replace | change | show ] dev STRING\n");
	fprintf(stderr, "       [ handle QHANDLE ] [ root | ingress | parent CLASSID ]\n");
	fprintf(stderr, "       [ estimator INTERVAL TIME_CONSTANT ]\n");
	fprintf(stderr, "       [ stab [ help | STAB_OPTIONS] ]\n");
	fprintf(stderr, "       [ [ QDISC_KIND ] [ help | OPTIONS ] ]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "       tc qdisc show [ dev STRING ] [ingress]\n");
	fprintf(stderr, "Where:\n");
	fprintf(stderr, "QDISC_KIND := { [p|b]fifo | tbf | prio | cbq | red | etc. }\n");
	fprintf(stderr, "OPTIONS := ... try tc qdisc add <desired QDISC_KIND> help\n");
	fprintf(stderr, "STAB_OPTIONS := ... try tc qdisc add stab help\n");
	return -1;
}

int tc_qdisc_modify(int cmd, unsigned flags, int argc, char **argv)
{
	struct qdisc_util *q = NULL;
	struct tc_estimator est;
	struct {
		struct tc_sizespec	szopts;
		__u16			*data;
	} stab;
	char  d[16];
	char  k[16];
	struct {
		struct nlmsghdr 	n;
		struct tcmsg 		t;
		char   			buf[TCA_BUF_MAX];
	} req;

	memset(&req, 0, sizeof(req));
	memset(&stab, 0, sizeof(stab));
	memset(&est, 0, sizeof(est));
	memset(&d, 0, sizeof(d));
	memset(&k, 0, sizeof(k));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST|flags;
	req.n.nlmsg_type = cmd;
	req.t.tcm_family = AF_UNSPEC;

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			if (d[0])
				duparg("dev", *argv);
			strncpy(d, *argv, sizeof(d)-1);
		} else if (strcmp(*argv, "handle") == 0) {
			__u32 handle;
			if (req.t.tcm_handle)
				duparg("handle", *argv);
			NEXT_ARG();
			if (get_qdisc_handle(&handle, *argv))
				invarg(*argv, "invalid qdisc ID");
			req.t.tcm_handle = handle;
		} else if (strcmp(*argv, "root") == 0) {
			if (req.t.tcm_parent) {
				fprintf(stderr, "Error: \"root\" is duplicate parent ID\n");
				return -1;
			}
			req.t.tcm_parent = TC_H_ROOT;
#ifdef TC_H_INGRESS
		} else if (strcmp(*argv, "ingress") == 0) {
			if (req.t.tcm_parent) {
				fprintf(stderr, "Error: \"ingress\" is a duplicate parent ID\n");
				return -1;
			}
			req.t.tcm_parent = TC_H_INGRESS;
			strncpy(k, "ingress", sizeof(k)-1);
			q = get_qdisc_kind(k);
			req.t.tcm_handle = 0xffff0000;

			argc--; argv++;
			break;
#endif
		} else if (strcmp(*argv, "parent") == 0) {
			__u32 handle;
			NEXT_ARG();
			if (req.t.tcm_parent)
				duparg("parent", *argv);
			if (get_tc_classid(&handle, *argv))
				invarg(*argv, "invalid parent ID");
			req.t.tcm_parent = handle;
		} else if (matches(*argv, "estimator") == 0) {
			if (parse_estimator(&argc, &argv, &est))
				return -1;
		} else if (matches(*argv, "stab") == 0) {
			if (parse_size_table(&argc, &argv, &stab.szopts) < 0)
				return -1;
			continue;
		} else if (matches(*argv, "help") == 0) {
			usage();
		} else {
			strncpy(k, *argv, sizeof(k)-1);

			q = get_qdisc_kind(k);
			argc--; argv++;
			break;
		}
		argc--; argv++;
	}

	if (k[0])
		addattr_l(&req.n, sizeof(req), TCA_KIND, k, strlen(k)+1);
	if (est.ewma_log)
		addattr_l(&req.n, sizeof(req), TCA_RATE, &est, sizeof(est));

	if (q) {
		if (!q->parse_qopt) {
			fprintf(stderr, "qdisc '%s' does not support option parsing\n", k);
			return -1;
		}
		if (q->parse_qopt(q, argc, argv, &req.n))
			return 1;
	} else {
		if (argc) {
			if (matches(*argv, "help") == 0)
				usage();

			fprintf(stderr, "Garbage instead of arguments \"%s ...\". Try \"tc qdisc help\".\n", *argv);
			return -1;
		}
	}

	if (check_size_table_opts(&stab.szopts)) {
		struct rtattr *tail;

		if (tc_calc_size_table(&stab.szopts, &stab.data) < 0) {
			fprintf(stderr, "failed to calculate size table.\n");
			return -1;
		}

		tail = NLMSG_TAIL(&req.n);
		addattr_l(&req.n, sizeof(req), TCA_STAB, NULL, 0);
		addattr_l(&req.n, sizeof(req), TCA_STAB_BASE, &stab.szopts,
			  sizeof(stab.szopts));
		if (stab.data)
			addattr_l(&req.n, sizeof(req), TCA_STAB_DATA, stab.data,
				  stab.szopts.tsize * sizeof(__u16));
		tail->rta_len = (void *)NLMSG_TAIL(&req.n) - (void *)tail;
		if (stab.data)
			free(stab.data);
	}

	if (d[0])  {
		int idx;

 		ll_init_map(&rth);

		if ((idx = ll_name_to_index(d)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", d);
			return 1;
		}
		req.t.tcm_ifindex = idx;
	}

 	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		return 2;

	return 0;
}

static int filter_ifindex;

int print_qdisc(const struct sockaddr_nl *who,
		       struct nlmsghdr *n,
		       void *arg)
{
	FILE *fp = (FILE*)arg;
	struct tcmsg *t = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * tb[TCA_MAX+1];
	struct qdisc_util *q;
	char abuf[256];

	if (n->nlmsg_type != RTM_NEWQDISC && n->nlmsg_type != RTM_DELQDISC) {
		fprintf(stderr, "Not a qdisc\n");
		return 0;
	}
	len -= NLMSG_LENGTH(sizeof(*t));
	if (len < 0) {
		fprintf(stderr, "Wrong len %d\n", len);
		return -1;
	}

	if (filter_ifindex && filter_ifindex != t->tcm_ifindex)
		return 0;

	memset(tb, 0, sizeof(tb));
	parse_rtattr(tb, TCA_MAX, TCA_RTA(t), len);

	if (tb[TCA_KIND] == NULL) {
		fprintf(stderr, "print_qdisc: NULL kind\n");
		return -1;
	}

	if (n->nlmsg_type == RTM_DELQDISC)
		fprintf(fp, "deleted ");

	fprintf(fp, "qdisc %s %x: ", (char*)RTA_DATA(tb[TCA_KIND]), t->tcm_handle>>16);
	if (filter_ifindex == 0)
		fprintf(fp, "dev %s ", ll_index_to_name(t->tcm_ifindex));
	if (t->tcm_parent == TC_H_ROOT)
		fprintf(fp, "root ");
	else if (t->tcm_parent) {
		print_tc_classid(abuf, sizeof(abuf), t->tcm_parent);
		fprintf(fp, "parent %s ", abuf);
	}
	if (t->tcm_info != 1) {
		fprintf(fp, "refcnt %d ", t->tcm_info);
	}
	/* pfifo_fast is generic enough to warrant the hardcoding --JHS */

	if (0 == strcmp("pfifo_fast", RTA_DATA(tb[TCA_KIND])))
		q = get_qdisc_kind("prio");
	else
		q = get_qdisc_kind(RTA_DATA(tb[TCA_KIND]));

	if (tb[TCA_OPTIONS]) {
		if (q)
			q->print_qopt(q, fp, tb[TCA_OPTIONS]);
		else
			fprintf(fp, "[cannot parse qdisc parameters]");
	}
	fprintf(fp, "\n");
	if (show_details && tb[TCA_STAB]) {
		print_size_table(fp, " ", tb[TCA_STAB]);
		fprintf(fp, "\n");
	}
	if (show_stats) {
		struct rtattr *xstats = NULL;

		if (tb[TCA_STATS] || tb[TCA_STATS2] || tb[TCA_XSTATS]) {
			print_tcstats_attr(fp, tb, " ", &xstats);
			fprintf(fp, "\n");
		}

		if (q && xstats && q->print_xstats) {
			q->print_xstats(q, fp, xstats);
			fprintf(fp, "\n");
		}
	}
	fflush(fp);
	return 0;
}


int tc_qdisc_list(int argc, char **argv)
{
	struct tcmsg t;
	char d[16];

	memset(&t, 0, sizeof(t));
	t.tcm_family = AF_UNSPEC;
	memset(&d, 0, sizeof(d));

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			strncpy(d, *argv, sizeof(d)-1);
#ifdef TC_H_INGRESS
                } else if (strcmp(*argv, "ingress") == 0) {
                             if (t.tcm_parent) {
                                     fprintf(stderr, "Duplicate parent ID\n");
                                     usage();
                             }
                             t.tcm_parent = TC_H_INGRESS;
#endif
		} else if (matches(*argv, "help") == 0) {
			usage();
		} else {
			fprintf(stderr, "What is \"%s\"? Try \"tc qdisc help\".\n", *argv);
			return -1;
		}

		argc--; argv++;
	}

 	ll_init_map(&rth);

	if (d[0]) {
		if ((t.tcm_ifindex = ll_name_to_index(d)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", d);
			return 1;
		}
		filter_ifindex = t.tcm_ifindex;
	}

 	if (rtnl_dump_request(&rth, RTM_GETQDISC, &t, sizeof(t)) < 0) {
		perror("Cannot send dump request");
		return 1;
	}

 	if (rtnl_dump_filter(&rth, print_qdisc, stdout, NULL, NULL) < 0) {
		fprintf(stderr, "Dump terminated\n");
		return 1;
	}

	return 0;
}

int do_qdisc(int argc, char **argv)
{
	if (argc < 1)
		return tc_qdisc_list(0, NULL);
	if (matches(*argv, "add") == 0)
		return tc_qdisc_modify(RTM_NEWQDISC, NLM_F_EXCL|NLM_F_CREATE, argc-1, argv+1);
	if (matches(*argv, "change") == 0)
		return tc_qdisc_modify(RTM_NEWQDISC, 0, argc-1, argv+1);
	if (matches(*argv, "replace") == 0)
		return tc_qdisc_modify(RTM_NEWQDISC, NLM_F_CREATE|NLM_F_REPLACE, argc-1, argv+1);
	if (matches(*argv, "link") == 0)
		return tc_qdisc_modify(RTM_NEWQDISC, NLM_F_REPLACE, argc-1, argv+1);
	if (matches(*argv, "delete") == 0)
		return tc_qdisc_modify(RTM_DELQDISC, 0,  argc-1, argv+1);
#if 0
	if (matches(*argv, "get") == 0)
		return tc_qdisc_get(RTM_GETQDISC, 0,  argc-1, argv+1);
#endif
	if (matches(*argv, "list") == 0 || matches(*argv, "show") == 0
	    || matches(*argv, "lst") == 0)
		return tc_qdisc_list(argc-1, argv+1);
	if (matches(*argv, "help") == 0) {
		usage();
		return 0;
        }
	fprintf(stderr, "Command \"%s\" is unknown, try \"tc qdisc help\".\n", *argv);
	return -1;
}
