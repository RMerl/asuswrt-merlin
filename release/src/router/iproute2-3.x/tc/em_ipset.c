/*
 * em_ipset.c		IPset Ematch
 *
 * (C) 2012 Florian Westphal <fw@strlen.de>
 *
 * Parts taken from iptables libxt_set.h:
 * Copyright (C) 2000-2002 Joakim Axelsson <gozem@linux.nu>
 *                         Patrick Schaaf <bof@bof.de>
 *                         Martin Josefsson <gandalf@wlug.westbo.se>
 * Copyright (C) 2003-2010 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <xtables.h>
#include <linux/netfilter/ipset/ip_set.h>

#ifndef IPSET_INVALID_ID
typedef __u16 ip_set_id_t;

enum ip_set_dim {
	IPSET_DIM_ZERO = 0,
	IPSET_DIM_ONE,
	IPSET_DIM_TWO,
	IPSET_DIM_THREE,
	IPSET_DIM_MAX = 6,
};
#endif /* IPSET_INVALID_ID */

#include <linux/netfilter/xt_set.h>
#include "m_ematch.h"

#ifndef IPSET_INVALID_ID
#define IPSET_INVALID_ID	65535
#define SO_IP_SET		83

union ip_set_name_index {
	char name[IPSET_MAXNAMELEN];
	__u16 index;
};

#define IP_SET_OP_GET_BYNAME	0x00000006	/* Get set index by name */
struct ip_set_req_get_set {
	unsigned op;
	unsigned version;
	union ip_set_name_index set;
};

#define IP_SET_OP_GET_BYINDEX	0x00000007	/* Get set name by index */
/* Uses ip_set_req_get_set */

#define IP_SET_OP_VERSION	0x00000100	/* Ask kernel version */
struct ip_set_req_version {
	unsigned op;
	unsigned version;
};
#endif /* IPSET_INVALID_ID */

extern struct ematch_util ipset_ematch_util;

static int get_version(unsigned *version)
{
	int res, sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	struct ip_set_req_version req_version;
	socklen_t size = sizeof(req_version);

	if (sockfd < 0) {
		fputs("Can't open socket to ipset.\n", stderr);
		return -1;
	}

	req_version.op = IP_SET_OP_VERSION;
	res = getsockopt(sockfd, SOL_IP, SO_IP_SET, &req_version, &size);
	if (res != 0) {
		perror("xt_set getsockopt");
		return -1;
	}

	*version = req_version.version;
	return sockfd;
}

static int do_getsockopt(struct ip_set_req_get_set *req)
{
	int sockfd, res;
	socklen_t size = sizeof(struct ip_set_req_get_set);
	sockfd = get_version(&req->version);
	if (sockfd < 0)
		return -1;
	res = getsockopt(sockfd, SOL_IP, SO_IP_SET, req, &size);
	if (res != 0)
		perror("Problem when communicating with ipset");
	close(sockfd);
	if (res != 0)
		return -1;

	if (size != sizeof(struct ip_set_req_get_set)) {
		fprintf(stderr,
			"Incorrect return size from kernel during ipset lookup, "
			"(want %zu, got %zu)\n",
			sizeof(struct ip_set_req_get_set), (size_t)size);
		return -1;
	}

	return res;
}

static int
get_set_byid(char *setname, unsigned int idx)
{
	struct ip_set_req_get_set req;
	int res;

	req.op = IP_SET_OP_GET_BYINDEX;
	req.set.index = idx;
	res = do_getsockopt(&req);
	if (res != 0)
		return -1;
	if (req.set.name[0] == '\0') {
		fprintf(stderr,
			"Set with index %i in kernel doesn't exist.\n", idx);
		return -1;
	}

	strncpy(setname, req.set.name, IPSET_MAXNAMELEN);
	return 0;
}

static int
get_set_byname(const char *setname, struct xt_set_info *info)
{
	struct ip_set_req_get_set req;
	int res;

	req.op = IP_SET_OP_GET_BYNAME;
	strncpy(req.set.name, setname, IPSET_MAXNAMELEN);
	req.set.name[IPSET_MAXNAMELEN - 1] = '\0';
	res = do_getsockopt(&req);
	if (res != 0)
		return -1;
	if (req.set.index == IPSET_INVALID_ID)
		return -1;
	info->index = req.set.index;
	return 0;
}

static int
parse_dirs(const char *opt_arg, struct xt_set_info *info)
{
        char *saved = strdup(opt_arg);
        char *ptr, *tmp = saved;

	if (!tmp) {
		perror("strdup");
		return -1;
	}

        while (info->dim < IPSET_DIM_MAX && tmp != NULL) {
                info->dim++;
                ptr = strsep(&tmp, ",");
                if (strncmp(ptr, "src", 3) == 0)
                        info->flags |= (1 << info->dim);
                else if (strncmp(ptr, "dst", 3) != 0) {
                        fputs("You must specify (the comma separated list of) 'src' or 'dst'\n", stderr);
			free(saved);
			return -1;
		}
        }

        if (tmp)
                fprintf(stderr, "Can't be more src/dst options than %u", IPSET_DIM_MAX);
        free(saved);
	return tmp ? -1 : 0;
}

static void ipset_print_usage(FILE *fd)
{
	fprintf(fd,
	    "Usage: ipset(SETNAME FLAGS)\n" \
	    "where: SETNAME:= string\n" \
	    "       FLAGS  := { FLAG[,FLAGS] }\n" \
	    "       FLAG   := { src | dst }\n" \
	    "\n" \
	    "Example: 'ipset(bulk src,dst)'\n");
}

static int ipset_parse_eopt(struct nlmsghdr *n, struct tcf_ematch_hdr *hdr,
			    struct bstr *args)
{
	struct xt_set_info set_info;
	int ret;

	memset(&set_info, 0, sizeof(set_info));

#define PARSE_ERR(CARG, FMT, ARGS...) \
	em_parse_error(EINVAL, args, CARG, &ipset_ematch_util, FMT ,##ARGS)

	if (args == NULL)
		return PARSE_ERR(args, "ipset: missing set name");

	if (args->len >= IPSET_MAXNAMELEN)
		return PARSE_ERR(args, "ipset: set name too long (max %u)", IPSET_MAXNAMELEN - 1);
	ret = get_set_byname(args->data, &set_info);
	if (ret < 0)
		return PARSE_ERR(args, "ipset: unknown set name '%s'", args->data);

	if (args->next == NULL)
		return PARSE_ERR(args, "ipset: missing set flags");

	args = bstr_next(args);
	if (parse_dirs(args->data, &set_info))
		return PARSE_ERR(args, "ipset: error parsing set flags");

	if (args->next) {
		args = bstr_next(args);
		return PARSE_ERR(args, "ipset: unknown parameter");
	}

	addraw_l(n, MAX_MSG, hdr, sizeof(*hdr));
	addraw_l(n, MAX_MSG, &set_info, sizeof(set_info));

#undef PARSE_ERR
	return 0;
}

static int ipset_print_eopt(FILE *fd, struct tcf_ematch_hdr *hdr, void *data,
			    int data_len)
{
	int i;
        char setname[IPSET_MAXNAMELEN];
	const struct xt_set_info *set_info = data;

	if (data_len != sizeof(*set_info)) {
		fprintf(stderr, "xt_set_info struct size mismatch\n");
		return -1;
	}

        if (get_set_byid(setname, set_info->index))
		return -1;
	fputs(setname, fd);
	for (i = 1; i <= set_info->dim; i++) {
		fprintf(fd, "%s%s", i == 1 ? " " : ",", set_info->flags & (1 << i) ? "src" : "dst");
	}

	return 0;
}

struct ematch_util ipset_ematch_util = {
	.kind = "ipset",
	.kind_num = TCF_EM_IPSET,
	.parse_eopt = ipset_parse_eopt,
	.print_eopt = ipset_print_eopt,
	.print_usage = ipset_print_usage
};
