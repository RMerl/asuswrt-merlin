/*
 * m_xt.c	xtables based targets
 *		utilities mostly ripped from iptables <duh, its the linux way>
 *
 *		This program is free software; you can distribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:  J Hadi Salim (hadi@cyberus.ca)
 */

#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <limits.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <xtables.h>
#include "utils.h"
#include "tc_util.h"
#include <linux/tc_act/tc_ipt.h>
#include <stdio.h>
#include <dlfcn.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#ifndef XT_LIB_DIR
#       define XT_LIB_DIR "/lib/xtables"
#endif

#ifndef __ALIGN_KERNEL
#define __ALIGN_KERNEL(x, a)		__ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#endif

#ifndef ALIGN
#define ALIGN(x,a)	__ALIGN_KERNEL((x), (a))
#endif

static const char *tname = "mangle";

char *lib_dir;

static const char *ipthooks[] = {
	"NF_IP_PRE_ROUTING",
	"NF_IP_LOCAL_IN",
	"NF_IP_FORWARD",
	"NF_IP_LOCAL_OUT",
	"NF_IP_POST_ROUTING",
};

static struct option original_opts[] = {
	{
		.name = "jump",
		.has_arg = 1,
		.val = 'j'
	},
	{0, 0, 0, 0}
};

static struct xtables_globals tcipt_globals = {
	.option_offset = 0,
	.program_name = "tc-ipt",
	.program_version = "0.2",
	.orig_opts = original_opts,
	.opts = original_opts,
	.exit_err = NULL,
};

/*
 * we may need to check for version mismatch
*/
static int
build_st(struct xtables_target *target, struct xt_entry_target *t)
{

	size_t size =
		    XT_ALIGN(sizeof (struct xt_entry_target)) + target->size;

	if (NULL == t) {
		target->t = xtables_calloc(1, size);
		target->t->u.target_size = size;
		strcpy(target->t->u.user.name, target->name);
		target->t->u.user.revision = target->revision;

		if (target->init != NULL)
			target->init(target->t);
	} else {
		target->t = t;
	}
	return 0;

}

static void set_lib_dir(void)
{

	lib_dir = getenv("XTABLES_LIBDIR");
	if (!lib_dir) {
		lib_dir = getenv("IPTABLES_LIB_DIR");
		if (lib_dir)
			fprintf(stderr, "using deprecated IPTABLES_LIB_DIR \n");
	}
	if (lib_dir == NULL)
		lib_dir = XT_LIB_DIR;

}

static int parse_ipt(struct action_util *a,int *argc_p,
		     char ***argv_p, int tca_id, struct nlmsghdr *n)
{
	struct xtables_target *m = NULL;
	struct ipt_entry fw;
	struct rtattr *tail;

	int c;
	int rargc = *argc_p;
	char **argv = *argv_p;
	int argc = 0, iargc = 0;
	char k[16];
	int size = 0;
	int iok = 0, ok = 0;
	__u32 hook = 0, index = 0;
	struct option *opts = NULL;

	xtables_init_all(&tcipt_globals, NFPROTO_IPV4);
	set_lib_dir();

	{
		int i;
		for (i = 0; i < rargc; i++) {
			if (NULL == argv[i] || 0 == strcmp(argv[i], "action")) {
				break;
			}
		}
		iargc = argc = i;
	}

	if (argc <= 2) {
		fprintf(stderr,"bad arguments to ipt %d vs %d \n", argc, rargc);
		return -1;
	}

	while (1) {
		c = getopt_long(argc, argv, "j:", tcipt_globals.opts, NULL);
		if (c == -1)
			break;
		switch (c) {
		case 'j':
			m = xtables_find_target(optarg, XTF_TRY_LOAD);
			if (NULL != m) {

				if (0 > build_st(m, NULL)) {
					printf(" %s error \n", m->name);
					return -1;
				}
#if (XTABLES_VERSION_CODE >= 6)
			opts = xtables_options_xfrm(tcipt_globals.orig_opts,
						    tcipt_globals.opts,
						    m->x6_options,
						    &m->option_offset);
#else
			opts = xtables_merge_options(tcipt_globals.opts,
						     m->extra_opts,
						     &m->option_offset);
#endif
			if (opts == NULL) {
				fprintf(stderr, " failed to find additional options for target %s\n\n", optarg);
				return -1;
			} else
				tcipt_globals.opts = opts;
			} else {
				fprintf(stderr," failed to find target %s\n\n", optarg);
				return -1;
			}
			ok++;
			break;

		default:
			memset(&fw, 0, sizeof (fw));
#if (XTABLES_VERSION_CODE >= 6)
		if (m != NULL && m->x6_parse != NULL ) {
			xtables_option_tpcall(c, argv, 0 , m, NULL);
#else
		if (m != NULL && m->parse != NULL ) {
			m->parse(c - m->option_offset, argv, 0, &m->tflags,
				 NULL, &m->t);
#endif
			} else {
				fprintf(stderr,"failed to find target %s\n\n", optarg);
				return -1;

			}
			ok++;
			break;
		}
	}

	if (iargc > optind) {
		if (matches(argv[optind], "index") == 0) {
			if (get_u32(&index, argv[optind + 1], 10)) {
				fprintf(stderr, "Illegal \"index\"\n");
				xtables_free_opts(1);
				return -1;
			}
			iok++;

			optind += 2;
		}
	}

	if (!ok && !iok) {
		fprintf(stderr," ipt Parser BAD!! (%s)\n", *argv);
		return -1;
	}

	/* check that we passed the correct parameters to the target */
#if (XTABLES_VERSION_CODE >= 6)
	if (m)
		xtables_option_tfcall(m);
#else
	if (m && m->final_check)
		m->final_check(m->tflags);
#endif

	{
		struct tcmsg *t = NLMSG_DATA(n);
		if (t->tcm_parent != TC_H_ROOT
		    && t->tcm_parent == TC_H_MAJ(TC_H_INGRESS)) {
			hook = NF_IP_PRE_ROUTING;
		} else {
			hook = NF_IP_POST_ROUTING;
		}
	}

	tail = NLMSG_TAIL(n);
	addattr_l(n, MAX_MSG, tca_id, NULL, 0);
	fprintf(stdout, "tablename: %s hook: %s\n ", tname, ipthooks[hook]);
	fprintf(stdout, "\ttarget: ");

	if (m)
		m->print(NULL, m->t, 0);
	fprintf(stdout, " index %d\n", index);

	if (strlen(tname) > 16) {
		size = 16;
		k[15] = 0;
	} else {
		size = 1 + strlen(tname);
	}
	strncpy(k, tname, size);

	addattr_l(n, MAX_MSG, TCA_IPT_TABLE, k, size);
	addattr_l(n, MAX_MSG, TCA_IPT_HOOK, &hook, 4);
	addattr_l(n, MAX_MSG, TCA_IPT_INDEX, &index, 4);
	if (m)
		addattr_l(n, MAX_MSG, TCA_IPT_TARG, m->t, m->t->u.target_size);
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;

	argc -= optind;
	argv += optind;
	*argc_p = rargc - iargc;
	*argv_p = argv;

	optind = 0;
	xtables_free_opts(1);

	if (m) {
		/* Clear flags if target will be used again */
		m->tflags = 0;
		m->used = 0;
		/* Free allocated memory */
		if (m->t)
			free(m->t);
	}

	return 0;

}

static int
print_ipt(struct action_util *au,FILE * f, struct rtattr *arg)
{
	struct rtattr *tb[TCA_IPT_MAX + 1];
	struct xt_entry_target *t = NULL;
	struct option *opts = NULL;

	if (arg == NULL)
		return -1;

	/* copy tcipt_globals because .opts will be modified by iptables */
	struct xtables_globals tmp_tcipt_globals = tcipt_globals;

	xtables_init_all(&tmp_tcipt_globals, NFPROTO_IPV4);
	set_lib_dir();

	parse_rtattr_nested(tb, TCA_IPT_MAX, arg);

	if (tb[TCA_IPT_TABLE] == NULL) {
		fprintf(f, "[NULL ipt table name ] assuming mangle ");
	} else {
		fprintf(f, "tablename: %s ",
			rta_getattr_str(tb[TCA_IPT_TABLE]));
	}

	if (tb[TCA_IPT_HOOK] == NULL) {
		fprintf(f, "[NULL ipt hook name ]\n ");
		return -1;
	} else {
		__u32 hook;
		hook = rta_getattr_u32(tb[TCA_IPT_HOOK]);
		fprintf(f, " hook: %s \n", ipthooks[hook]);
	}

	if (tb[TCA_IPT_TARG] == NULL) {
		fprintf(f, "\t[NULL ipt target parameters ] \n");
		return -1;
	} else {
		struct xtables_target *m = NULL;
		t = RTA_DATA(tb[TCA_IPT_TARG]);
		m = xtables_find_target(t->u.user.name, XTF_TRY_LOAD);
		if (NULL != m) {
			if (0 > build_st(m, t)) {
				fprintf(stderr, " %s error \n", m->name);
				return -1;
			}

#if (XTABLES_VERSION_CODE >= 6)
		opts = xtables_options_xfrm(tmp_tcipt_globals.orig_opts,
					    tmp_tcipt_globals.opts,
					    m->x6_options,
					    &m->option_offset);
#else
		opts = xtables_merge_options(tmp_tcipt_globals.opts,
					     m->extra_opts,
					     &m->option_offset);
#endif
	if (opts == NULL) {
		fprintf(stderr, " failed to find additional options for target %s\n\n", optarg);
		return -1;
	} else
		tmp_tcipt_globals.opts = opts;
		} else {
			fprintf(stderr, " failed to find target %s\n\n",
				t->u.user.name);
			return -1;
		}
		fprintf(f, "\ttarget ");
		m->print(NULL, m->t, 0);
		if (tb[TCA_IPT_INDEX] == NULL) {
			fprintf(f, " [NULL ipt target index ]\n");
		} else {
			__u32 index;
			index = rta_getattr_u32(tb[TCA_IPT_INDEX]);
			fprintf(f, " \n\tindex %d", index);
		}

		if (tb[TCA_IPT_CNT]) {
			struct tc_cnt *c  = RTA_DATA(tb[TCA_IPT_CNT]);;
			fprintf(f, " ref %d bind %d", c->refcnt, c->bindcnt);
		}
		if (show_stats) {
			if (tb[TCA_IPT_TM]) {
				struct tcf_t *tm = RTA_DATA(tb[TCA_IPT_TM]);
				print_tm(f,tm);
			}
		}
		fprintf(f, " \n");

	}
	xtables_free_opts(1);

	return 0;
}

struct action_util xt_action_util = {
        .id = "xt",
        .parse_aopt = parse_ipt,
        .print_aopt = print_ipt,
};
