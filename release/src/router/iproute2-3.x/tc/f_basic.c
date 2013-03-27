/*
 * f_basic.c		Basic Classifier
 *
 *		This program is free software; you can distribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Thomas Graf <tgraf@suug.ch>
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
#include <linux/if.h>

#include "utils.h"
#include "tc_util.h"
#include "m_ematch.h"

static void explain(void)
{
	fprintf(stderr, "Usage: ... basic [ match EMATCH_TREE ] [ police POLICE_SPEC ]\n");
	fprintf(stderr, "                 [ action ACTION_SPEC ] [ classid CLASSID ]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Where: SELECTOR := SAMPLE SAMPLE ...\n");
	fprintf(stderr, "       FILTERID := X:Y:Z\n");
	fprintf(stderr, "\nNOTE: CLASSID is parsed as hexadecimal input.\n");
}

static int basic_parse_opt(struct filter_util *qu, char *handle,
			   int argc, char **argv, struct nlmsghdr *n)
{
	struct tcmsg *t = NLMSG_DATA(n);
	struct rtattr *tail;
	long h = 0;

	if (argc == 0)
		return 0;

	if (handle) {
		h = strtol(handle, NULL, 0);
		if (h == LONG_MIN || h == LONG_MAX) {
			fprintf(stderr, "Illegal handle \"%s\", must be numeric.\n",
			    handle);
			return -1;
		}
	}

	t->tcm_handle = h;

	tail = (struct rtattr*)(((void*)n)+NLMSG_ALIGN(n->nlmsg_len));
	addattr_l(n, MAX_MSG, TCA_OPTIONS, NULL, 0);

	while (argc > 0) {
		if (matches(*argv, "match") == 0) {
			NEXT_ARG();
			if (parse_ematch(&argc, &argv, TCA_BASIC_EMATCHES, n)) {
				fprintf(stderr, "Illegal \"ematch\"\n");
				return -1;
			}
			continue;
		} else if (matches(*argv, "classid") == 0 ||
			   strcmp(*argv, "flowid") == 0) {
			unsigned handle;
			NEXT_ARG();
			if (get_tc_classid(&handle, *argv)) {
				fprintf(stderr, "Illegal \"classid\"\n");
				return -1;
			}
			addattr_l(n, MAX_MSG, TCA_BASIC_CLASSID, &handle, 4);
		} else if (matches(*argv, "action") == 0) {
			NEXT_ARG();
			if (parse_action(&argc, &argv, TCA_BASIC_ACT, n)) {
				fprintf(stderr, "Illegal \"action\"\n");
				return -1;
			}
			continue;

		} else if (matches(*argv, "police") == 0) {
			NEXT_ARG();
			if (parse_police(&argc, &argv, TCA_BASIC_POLICE, n)) {
				fprintf(stderr, "Illegal \"police\"\n");
				return -1;
			}
			continue;
		} else if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--; argv++;
	}

	tail->rta_len = (((void*)n)+n->nlmsg_len) - (void*)tail;
	return 0;
}

static int basic_print_opt(struct filter_util *qu, FILE *f,
			   struct rtattr *opt, __u32 handle)
{
	struct rtattr *tb[TCA_BASIC_MAX+1];

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_BASIC_MAX, opt);

	if (handle)
		fprintf(f, "handle 0x%x ", handle);

	if (tb[TCA_BASIC_CLASSID]) {
		SPRINT_BUF(b1);
		fprintf(f, "flowid %s ",
			sprint_tc_classid(*(__u32*)RTA_DATA(tb[TCA_BASIC_CLASSID]), b1));
	}

	if (tb[TCA_BASIC_EMATCHES])
		print_ematch(f, tb[TCA_BASIC_EMATCHES]);

	if (tb[TCA_BASIC_POLICE]) {
		fprintf(f, "\n");
		tc_print_police(f, tb[TCA_BASIC_POLICE]);
	}

	if (tb[TCA_BASIC_ACT]) {
		tc_print_action(f, tb[TCA_BASIC_ACT]);
	}

	return 0;
}

struct filter_util basic_filter_util = {
	.id = "basic",
	.parse_fopt = basic_parse_opt,
	.print_fopt = basic_print_opt,
};
