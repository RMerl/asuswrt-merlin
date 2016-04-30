/*
 * f_bpf.c	BPF-based Classifier
 *
 *		This program is free software; you can distribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Daniel Borkmann <dborkman@redhat.com>
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
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <linux/filter.h>
#include <linux/if.h>

#include "utils.h"
#include "tc_util.h"

static void explain(void)
{
	fprintf(stderr, "Usage: ... bpf ...\n");
	fprintf(stderr, "\n");
	fprintf(stderr, " [inline]:     run bytecode BPF_BYTECODE\n");
	fprintf(stderr, " [from file]:  run bytecode-file FILE\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "               [ action ACTION_SPEC ]\n");
	fprintf(stderr, "               [ classid CLASSID ]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Where BPF_BYTECODE := \'s,c t f k,c t f k,c t f k,...\'\n");
	fprintf(stderr, "      c,t,f,k and s are decimals; s denotes number of 4-tuples\n");
	fprintf(stderr, "Where FILE points to a file containing the BPF_BYTECODE string\n");
	fprintf(stderr, "\nACTION_SPEC := ... look at individual actions\n");
	fprintf(stderr, "NOTE: CLASSID is parsed as hexadecimal input.\n");
}

static int bpf_parse_string(char *arg, bool from_file, __u16 *bpf_len,
			    char **bpf_string, bool *need_release,
			    const char separator)
{
	char sp;

	if (from_file) {
		size_t tmp_len, op_len = sizeof("65535 255 255 4294967295,");
		char *tmp_string;
		FILE *fp;

		tmp_len = sizeof("4096,") + BPF_MAXINSNS * op_len;
		tmp_string = malloc(tmp_len);
		if (tmp_string == NULL)
			return -ENOMEM;

		memset(tmp_string, 0, tmp_len);

		fp = fopen(arg, "r");
		if (fp == NULL) {
			perror("Cannot fopen");
			free(tmp_string);
			return -ENOENT;
		}

		if (!fgets(tmp_string, tmp_len, fp)) {
			free(tmp_string);
			fclose(fp);
			return -EIO;
		}

		fclose(fp);

		*need_release = true;
		*bpf_string = tmp_string;
	} else {
		*need_release = false;
		*bpf_string = arg;
	}

	if (sscanf(*bpf_string, "%hu%c", bpf_len, &sp) != 2 ||
	    sp != separator) {
		if (*need_release)
			free(*bpf_string);
		return -EINVAL;
	}

	return 0;
}

static int bpf_parse_ops(int argc, char **argv, struct nlmsghdr *n,
			 bool from_file)
{
	char *bpf_string, *token, separator = ',';
	struct sock_filter bpf_ops[BPF_MAXINSNS];
	int ret = 0, i = 0;
	bool need_release;
	__u16 bpf_len = 0;

	if (argc < 1)
		return -EINVAL;
	if (bpf_parse_string(argv[0], from_file, &bpf_len, &bpf_string,
			     &need_release, separator))
		return -EINVAL;
	if (bpf_len == 0 || bpf_len > BPF_MAXINSNS) {
		ret = -EINVAL;
		goto out;
	}

	token = bpf_string;
	while ((token = strchr(token, separator)) && (++token)[0]) {
		if (i >= bpf_len) {
			fprintf(stderr, "Real program length exceeds encoded "
				"length parameter!\n");
			ret = -EINVAL;
			goto out;
		}

		if (sscanf(token, "%hu %hhu %hhu %u,",
			   &bpf_ops[i].code, &bpf_ops[i].jt,
			   &bpf_ops[i].jf, &bpf_ops[i].k) != 4) {
			fprintf(stderr, "Error at instruction %d!\n", i);
			ret = -EINVAL;
			goto out;
		}

		i++;
	}

	if (i != bpf_len) {
		fprintf(stderr, "Parsed program length is less than encoded"
			"length parameter!\n");
		ret = -EINVAL;
		goto out;
	}

	addattr_l(n, MAX_MSG, TCA_BPF_OPS_LEN, &bpf_len, sizeof(bpf_len));
	addattr_l(n, MAX_MSG, TCA_BPF_OPS, &bpf_ops,
		  bpf_len * sizeof(struct sock_filter));
out:
	if (need_release)
		free(bpf_string);

	return ret;
}

static void bpf_print_ops(FILE *f, struct rtattr *bpf_ops, __u16 len)
{
	struct sock_filter *ops = (struct sock_filter *) RTA_DATA(bpf_ops);
	int i;

	if (len == 0)
		return;

	fprintf(f, "bytecode \'%u,", len);

	for (i = 0; i < len - 1; i++)
		fprintf(f, "%hu %hhu %hhu %u,", ops[i].code, ops[i].jt,
			ops[i].jf, ops[i].k);

	fprintf(f, "%hu %hhu %hhu %u\'\n", ops[i].code, ops[i].jt,
		ops[i].jf, ops[i].k);
}

static int bpf_parse_opt(struct filter_util *qu, char *handle,
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
			fprintf(stderr, "Illegal handle \"%s\", must be "
				"numeric.\n", handle);
			return -1;
		}
	}

	t->tcm_handle = h;

	tail = (struct rtattr*)(((void*)n)+NLMSG_ALIGN(n->nlmsg_len));
	addattr_l(n, MAX_MSG, TCA_OPTIONS, NULL, 0);

	while (argc > 0) {
		if (matches(*argv, "run") == 0) {
			bool from_file;
			NEXT_ARG();
			if (strcmp(*argv, "bytecode-file") == 0) {
				from_file = true;
			} else if (strcmp(*argv, "bytecode") == 0) {
				from_file = false;
			} else {
				fprintf(stderr, "What is \"%s\"?\n", *argv);
				explain();
				return -1;
			}
			NEXT_ARG();
			if (bpf_parse_ops(argc, argv, n, from_file)) {
				fprintf(stderr, "Illegal \"bytecode\"\n");
				return -1;
			}
		} else if (matches(*argv, "classid") == 0 ||
			   strcmp(*argv, "flowid") == 0) {
			unsigned handle;
			NEXT_ARG();
			if (get_tc_classid(&handle, *argv)) {
				fprintf(stderr, "Illegal \"classid\"\n");
				return -1;
			}
			addattr_l(n, MAX_MSG, TCA_BPF_CLASSID, &handle, 4);
		} else if (matches(*argv, "action") == 0) {
			NEXT_ARG();
			if (parse_action(&argc, &argv, TCA_BPF_ACT, n)) {
				fprintf(stderr, "Illegal \"action\"\n");
				return -1;
			}
			continue;
		} else if (matches(*argv, "police") == 0) {
			NEXT_ARG();
			if (parse_police(&argc, &argv, TCA_BPF_POLICE, n)) {
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

static int bpf_print_opt(struct filter_util *qu, FILE *f,
			 struct rtattr *opt, __u32 handle)
{
	struct rtattr *tb[TCA_BPF_MAX + 1];

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_BPF_MAX, opt);

	if (handle)
		fprintf(f, "handle 0x%x ", handle);

	if (tb[TCA_BPF_CLASSID]) {
		SPRINT_BUF(b1);
		fprintf(f, "flowid %s ",
			sprint_tc_classid(rta_getattr_u32(tb[TCA_BPF_CLASSID]), b1));
	}

	if (tb[TCA_BPF_OPS] && tb[TCA_BPF_OPS_LEN])
		bpf_print_ops(f, tb[TCA_BPF_OPS],
			      rta_getattr_u16(tb[TCA_BPF_OPS_LEN]));

	if (tb[TCA_BPF_POLICE]) {
		fprintf(f, "\n");
		tc_print_police(f, tb[TCA_BPF_POLICE]);
	}

	if (tb[TCA_BPF_ACT]) {
		tc_print_action(f, tb[TCA_BPF_ACT]);
	}

	return 0;
}

struct filter_util bpf_filter_util = {
	.id = "bpf",
	.parse_fopt = bpf_parse_opt,
	.print_fopt = bpf_print_opt,
};
