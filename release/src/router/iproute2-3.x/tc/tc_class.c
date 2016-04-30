/*
 * tc_class.c		"tc class".
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
#include <math.h>

#include "utils.h"
#include "tc_util.h"
#include "tc_common.h"
#include "hlist.h"

struct graph_node {
	struct hlist_node hlist;
	__u32 id;
	__u32 parent_id;
	struct graph_node *parent_node;
	struct graph_node *right_node;
	void *data;
	int data_len;
	int nodes_count;
};

static struct hlist_head cls_list = {};
static struct hlist_head root_cls_list = {};

static void usage(void);

static void usage(void)
{
	fprintf(stderr, "Usage: tc class [ add | del | change | replace | show ] dev STRING\n");
	fprintf(stderr, "       [ classid CLASSID ] [ root | parent CLASSID ]\n");
	fprintf(stderr, "       [ [ QDISC_KIND ] [ help | OPTIONS ] ]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "       tc class show [ dev STRING ] [ root | parent CLASSID ]\n");
	fprintf(stderr, "Where:\n");
	fprintf(stderr, "QDISC_KIND := { prio | cbq | etc. }\n");
	fprintf(stderr, "OPTIONS := ... try tc class add <desired QDISC_KIND> help\n");
	return;
}

static int tc_class_modify(int cmd, unsigned flags, int argc, char **argv)
{
	struct {
		struct nlmsghdr 	n;
		struct tcmsg 		t;
		char   			buf[4096];
	} req;
	struct qdisc_util *q = NULL;
	struct tc_estimator est;
	char  d[16];
	char  k[16];

	memset(&req, 0, sizeof(req));
	memset(&est, 0, sizeof(est));
	memset(d, 0, sizeof(d));
	memset(k, 0, sizeof(k));

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
		} else if (strcmp(*argv, "classid") == 0) {
			__u32 handle;
			NEXT_ARG();
			if (req.t.tcm_handle)
				duparg("classid", *argv);
			if (get_tc_classid(&handle, *argv))
				invarg("invalid class ID", *argv);
			req.t.tcm_handle = handle;
		} else if (strcmp(*argv, "handle") == 0) {
			fprintf(stderr, "Error: try \"classid\" instead of \"handle\"\n");
			return -1;
		} else if (strcmp(*argv, "root") == 0) {
			if (req.t.tcm_parent) {
				fprintf(stderr, "Error: \"root\" is duplicate parent ID.\n");
				return -1;
			}
			req.t.tcm_parent = TC_H_ROOT;
		} else if (strcmp(*argv, "parent") == 0) {
			__u32 handle;
			NEXT_ARG();
			if (req.t.tcm_parent)
				duparg("parent", *argv);
			if (get_tc_classid(&handle, *argv))
				invarg("invalid parent ID", *argv);
			req.t.tcm_parent = handle;
		} else if (matches(*argv, "estimator") == 0) {
			if (parse_estimator(&argc, &argv, &est))
				return -1;
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
		if (q->parse_copt == NULL) {
			fprintf(stderr, "Error: Qdisc \"%s\" is classless.\n", k);
			return 1;
		}
		if (q->parse_copt(q, argc, argv, &req.n))
			return 1;
	} else {
		if (argc) {
			if (matches(*argv, "help") == 0)
				usage();
			fprintf(stderr, "Garbage instead of arguments \"%s ...\". Try \"tc class help\".", *argv);
			return -1;
		}
	}

	if (d[0])  {
		ll_init_map(&rth);

		if ((req.t.tcm_ifindex = ll_name_to_index(d)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", d);
			return 1;
		}
	}

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL) < 0)
		return 2;

	return 0;
}

int filter_ifindex;
__u32 filter_qdisc;
__u32 filter_classid;

static void graph_node_add(__u32 parent_id, __u32 id, void *data,
		int len)
{
	struct graph_node *node = malloc(sizeof(struct graph_node));

	memset(node, 0, sizeof(*node));
	node->id         = id;
	node->parent_id  = parent_id;

	if (data && len) {
		node->data       = malloc(len);
		node->data_len   = len;
		memcpy(node->data, data, len);
	}

	if (parent_id == TC_H_ROOT)
		hlist_add_head(&node->hlist, &root_cls_list);
	else
		hlist_add_head(&node->hlist, &cls_list);
}

static void graph_indent(char *buf, struct graph_node *node, int is_newline,
		int add_spaces)
{
	char spaces[100] = {0};

	while (node && node->parent_node) {
		node->parent_node->right_node = node;
		node = node->parent_node;
	}
	while (node && node->right_node) {
		if (node->hlist.next)
			strcat(buf, "|    ");
		else
			strcat(buf, "     ");

		node = node->right_node;
	}

	if (is_newline) {
		if (node->hlist.next && node->nodes_count)
			strcat(buf, "|    |");
		else if (node->hlist.next)
			strcat(buf, "|     ");
		else if (node->nodes_count)
			strcat(buf, "     |");
		else if (!node->hlist.next)
			strcat(buf, "      ");
	}
	if (add_spaces > 0) {
		sprintf(spaces, "%-*s", add_spaces, "");
		strcat(buf, spaces);
	}
}

static void graph_cls_show(FILE *fp, char *buf, struct hlist_head *root_list,
		int level)
{
	struct hlist_node *n, *tmp_cls;
	char cls_id_str[256] = {};
	struct rtattr *tb[TCA_MAX + 1] = {};
	struct qdisc_util *q;
	char str[100] = {};

	hlist_for_each_safe(n, tmp_cls, root_list) {
		struct hlist_node *c, *tmp_chld;
		struct hlist_head children = {};
		struct graph_node *cls = container_of(n, struct graph_node,
				hlist);

		hlist_for_each_safe(c, tmp_chld, &cls_list) {
			struct graph_node *child = container_of(c,
					struct graph_node, hlist);

			if (cls->id == child->parent_id) {
				hlist_del(c);
				hlist_add_head(c, &children);
				cls->nodes_count++;
				child->parent_node = cls;
			}
		}

		graph_indent(buf, cls, 0, 0);

		print_tc_classid(cls_id_str, sizeof(cls_id_str), cls->id);
		sprintf(str, "+---(%s)", cls_id_str);
		strcat(buf, str);

		parse_rtattr(tb, TCA_MAX, (struct rtattr *)cls->data,
				cls->data_len);

		if (tb[TCA_KIND] == NULL) {
			strcat(buf, " [unknown qdisc kind] ");
		} else {
			const char *kind = rta_getattr_str(tb[TCA_KIND]);

			sprintf(str, " %s ", kind);
			strcat(buf, str);
			fprintf(fp, "%s", buf);
			buf[0] = '\0';

			q = get_qdisc_kind(kind);
			if (q && q->print_copt) {
				q->print_copt(q, fp, tb[TCA_OPTIONS]);
			}
			if (q && show_stats) {
				int cls_indent = strlen(q->id) - 2 +
					strlen(cls_id_str);
				struct rtattr *stats = NULL;

				graph_indent(buf, cls, 1, cls_indent);

				if (tb[TCA_STATS] || tb[TCA_STATS2]) {
					fprintf(fp, "\n");
					print_tcstats_attr(fp, tb, buf, &stats);
					buf[0] = '\0';
				}
				if (cls->hlist.next || cls->nodes_count) {
					strcat(buf, "\n");
					graph_indent(buf, cls, 1, 0);
				}
			}
		}
		free(cls->data);
		fprintf(fp, "%s\n", buf);
		buf[0] = '\0';

		graph_cls_show(fp, buf, &children, level + 1);
		if (!cls->hlist.next) {
			graph_indent(buf, cls, 0, 0);
			strcat(buf, "\n");
		}

		fprintf(fp, "%s", buf);
		buf[0] = '\0';
		free(cls);
	}
}

int print_class(const struct sockaddr_nl *who,
		       struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	struct tcmsg *t = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr *tb[TCA_MAX + 1] = {};
	struct qdisc_util *q;
	char abuf[256];

	if (n->nlmsg_type != RTM_NEWTCLASS && n->nlmsg_type != RTM_DELTCLASS) {
		fprintf(stderr, "Not a class\n");
		return 0;
	}
	len -= NLMSG_LENGTH(sizeof(*t));
	if (len < 0) {
		fprintf(stderr, "Wrong len %d\n", len);
		return -1;
	}

	if (show_graph) {
		graph_node_add(t->tcm_parent, t->tcm_handle, TCA_RTA(t), len);
		return 0;
	}

	if (filter_qdisc && TC_H_MAJ(t->tcm_handle^filter_qdisc))
		return 0;

	if (filter_classid && t->tcm_handle != filter_classid)
		return 0;

	parse_rtattr(tb, TCA_MAX, TCA_RTA(t), len);

	if (tb[TCA_KIND] == NULL) {
		fprintf(stderr, "print_class: NULL kind\n");
		return -1;
	}

	if (n->nlmsg_type == RTM_DELTCLASS)
		fprintf(fp, "deleted ");

	abuf[0] = 0;
	if (t->tcm_handle) {
		if (filter_qdisc)
			print_tc_classid(abuf, sizeof(abuf), TC_H_MIN(t->tcm_handle));
		else
			print_tc_classid(abuf, sizeof(abuf), t->tcm_handle);
	}
	fprintf(fp, "class %s %s ", rta_getattr_str(tb[TCA_KIND]), abuf);

	if (filter_ifindex == 0)
		fprintf(fp, "dev %s ", ll_index_to_name(t->tcm_ifindex));

	if (t->tcm_parent == TC_H_ROOT)
		fprintf(fp, "root ");
	else {
		if (filter_qdisc)
			print_tc_classid(abuf, sizeof(abuf), TC_H_MIN(t->tcm_parent));
		else
			print_tc_classid(abuf, sizeof(abuf), t->tcm_parent);
		fprintf(fp, "parent %s ", abuf);
	}
	if (t->tcm_info)
		fprintf(fp, "leaf %x: ", t->tcm_info>>16);
	q = get_qdisc_kind(RTA_DATA(tb[TCA_KIND]));
	if (tb[TCA_OPTIONS]) {
		if (q && q->print_copt)
			q->print_copt(q, fp, tb[TCA_OPTIONS]);
		else
			fprintf(fp, "[cannot parse class parameters]");
	}
	fprintf(fp, "\n");
	if (show_stats) {
		struct rtattr *xstats = NULL;

		if (tb[TCA_STATS] || tb[TCA_STATS2]) {
			print_tcstats_attr(fp, tb, " ", &xstats);
			fprintf(fp, "\n");
		}
		if (q && (xstats || tb[TCA_XSTATS]) && q->print_xstats) {
			q->print_xstats(q, fp, xstats ? : tb[TCA_XSTATS]);
			fprintf(fp, "\n");
		}
	}
	fflush(fp);
	return 0;
}


static int tc_class_list(int argc, char **argv)
{
	struct tcmsg t;
	char d[16];
	char buf[1024] = {0};

	memset(&t, 0, sizeof(t));
	t.tcm_family = AF_UNSPEC;
	memset(d, 0, sizeof(d));

	filter_qdisc = 0;
	filter_classid = 0;

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			if (d[0])
				duparg("dev", *argv);
			strncpy(d, *argv, sizeof(d)-1);
		} else if (strcmp(*argv, "qdisc") == 0) {
			NEXT_ARG();
			if (filter_qdisc)
				duparg("qdisc", *argv);
			if (get_qdisc_handle(&filter_qdisc, *argv))
				invarg("invalid qdisc ID", *argv);
		} else if (strcmp(*argv, "classid") == 0) {
			NEXT_ARG();
			if (filter_classid)
				duparg("classid", *argv);
			if (get_tc_classid(&filter_classid, *argv))
				invarg("invalid class ID", *argv);
		} else if (strcmp(*argv, "root") == 0) {
			if (t.tcm_parent) {
				fprintf(stderr, "Error: \"root\" is duplicate parent ID\n");
				return -1;
			}
			t.tcm_parent = TC_H_ROOT;
		} else if (strcmp(*argv, "parent") == 0) {
			__u32 handle;
			if (t.tcm_parent)
				duparg("parent", *argv);
			NEXT_ARG();
			if (get_tc_classid(&handle, *argv))
				invarg("invalid parent ID", *argv);
			t.tcm_parent = handle;
		} else if (matches(*argv, "help") == 0) {
			usage();
		} else {
			fprintf(stderr, "What is \"%s\"? Try \"tc class help\".\n", *argv);
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

	if (rtnl_dump_request(&rth, RTM_GETTCLASS, &t, sizeof(t)) < 0) {
		perror("Cannot send dump request");
		return 1;
	}

	if (rtnl_dump_filter(&rth, print_class, stdout) < 0) {
		fprintf(stderr, "Dump terminated\n");
		return 1;
	}

	if (show_graph)
		graph_cls_show(stdout, &buf[0], &root_cls_list, 0);

	return 0;
}

int do_class(int argc, char **argv)
{
	if (argc < 1)
		return tc_class_list(0, NULL);
	if (matches(*argv, "add") == 0)
		return tc_class_modify(RTM_NEWTCLASS, NLM_F_EXCL|NLM_F_CREATE, argc-1, argv+1);
	if (matches(*argv, "change") == 0)
		return tc_class_modify(RTM_NEWTCLASS, 0, argc-1, argv+1);
	if (matches(*argv, "replace") == 0)
		return tc_class_modify(RTM_NEWTCLASS, NLM_F_CREATE, argc-1, argv+1);
	if (matches(*argv, "delete") == 0)
		return tc_class_modify(RTM_DELTCLASS, 0,  argc-1, argv+1);
#if 0
	if (matches(*argv, "get") == 0)
		return tc_class_get(RTM_GETTCLASS, 0,  argc-1, argv+1);
#endif
	if (matches(*argv, "list") == 0 || matches(*argv, "show") == 0
	    || matches(*argv, "lst") == 0)
		return tc_class_list(argc-1, argv+1);
	if (matches(*argv, "help") == 0) {
		usage();
		return 0;
	}
	fprintf(stderr, "Command \"%s\" is unknown, try \"tc class help\".\n", *argv);
	return -1;
}
