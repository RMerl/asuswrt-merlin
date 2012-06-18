/* $USAGI: $ */

/*
 * Copyright (C)2005 USAGI/WIDE Project
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * based on ipmonitor.c
 */
/*
 * Authors:
 *	Masahide NAKAMURA @USAGI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/xfrm.h>
#include "utils.h"
#include "xfrm.h"
#include "ip_common.h"

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip xfrm monitor [ all | LISTofOBJECTS ]\n");
	exit(-1);
}

static int xfrm_acquire_print(const struct sockaddr_nl *who,
			      struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	struct xfrm_user_acquire *xacq = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * tb[XFRMA_MAX+1];
	__u16 family;

	if (n->nlmsg_type != XFRM_MSG_ACQUIRE) {
		fprintf(stderr, "Not an acquire: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
		return 0;
	}

	len -= NLMSG_LENGTH(sizeof(*xacq));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	parse_rtattr(tb, XFRMA_MAX, XFRMACQ_RTA(xacq), len);

	family = xacq->sel.family;
	if (family == AF_UNSPEC)
		family = xacq->policy.sel.family;
	if (family == AF_UNSPEC)
		family = preferred_family;

	fprintf(fp, "acquire ");

	fprintf(fp, "proto %s ", strxf_xfrmproto(xacq->id.proto));
	if (show_stats > 0 || xacq->id.spi) {
		__u32 spi = ntohl(xacq->id.spi);
		fprintf(fp, "spi 0x%08x", spi);
		if (show_stats > 0)
			fprintf(fp, "(%u)", spi);
		fprintf(fp, " ");
	}
	fprintf(fp, "%s", _SL_);

	xfrm_selector_print(&xacq->sel, family, fp, "  sel ");

	xfrm_policy_info_print(&xacq->policy, tb, fp, "    ", "  policy ");

	if (show_stats > 0)
		fprintf(fp, "  seq 0x%08u ", xacq->seq);
	if (show_stats > 0) {
		fprintf(fp, "%s-mask %s ",
			strxf_algotype(XFRMA_ALG_CRYPT),
			strxf_mask32(xacq->ealgos));
		fprintf(fp, "%s-mask %s ",
			strxf_algotype(XFRMA_ALG_AUTH),
			strxf_mask32(xacq->aalgos));
		fprintf(fp, "%s-mask %s",
			strxf_algotype(XFRMA_ALG_COMP),
			strxf_mask32(xacq->calgos));
	}
	fprintf(fp, "%s", _SL_);

	if (oneline)
		fprintf(fp, "\n");
	fflush(fp);

	return 0;
}

static int xfrm_accept_msg(const struct sockaddr_nl *who,
			   struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;

	if (timestamp)
		print_timestamp(fp);

	if (n->nlmsg_type == XFRM_MSG_NEWSA ||
	    n->nlmsg_type == XFRM_MSG_DELSA ||
	    n->nlmsg_type == XFRM_MSG_UPDSA ||
	    n->nlmsg_type == XFRM_MSG_EXPIRE) {
		xfrm_state_print(who, n, arg);
		return 0;
	}
	if (n->nlmsg_type == XFRM_MSG_NEWPOLICY ||
	    n->nlmsg_type == XFRM_MSG_DELPOLICY ||
	    n->nlmsg_type == XFRM_MSG_UPDPOLICY ||
	    n->nlmsg_type == XFRM_MSG_POLEXPIRE) {
		xfrm_policy_print(who, n, arg);
		return 0;
	}

	if (n->nlmsg_type == XFRM_MSG_ACQUIRE) {
		xfrm_acquire_print(who, n, arg);
		return 0;
	}
	if (n->nlmsg_type == XFRM_MSG_FLUSHSA) {
		/* XXX: Todo: show proto in xfrm_usersa_flush */
		fprintf(fp, "Flushed state\n");
		return 0;
	}
	if (n->nlmsg_type == XFRM_MSG_FLUSHPOLICY) {
		fprintf(fp, "Flushed policy\n");
		return 0;
	}
	if (n->nlmsg_type != NLMSG_ERROR && n->nlmsg_type != NLMSG_NOOP &&
	    n->nlmsg_type != NLMSG_DONE) {
		fprintf(fp, "Unknown message: %08d 0x%08x 0x%08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
	}
	return 0;
}

int do_xfrm_monitor(int argc, char **argv)
{
	struct rtnl_handle rth;
	char *file = NULL;
	unsigned groups = ~((unsigned)0); /* XXX */
	int lacquire=0;
	int lexpire=0;
	int lpolicy=0;
	int lsa=0;

	while (argc > 0) {
		if (matches(*argv, "file") == 0) {
			NEXT_ARG();
			file = *argv;
		} else if (matches(*argv, "acquire") == 0) {
			lacquire=1;
			groups = 0;
		} else if (matches(*argv, "expire") == 0) {
			lexpire=1;
			groups = 0;
		} else if (matches(*argv, "SA") == 0) {
			lsa=1;
			groups = 0;
		} else if (matches(*argv, "policy") == 0) {
			lpolicy=1;
			groups = 0;
		} else if (matches(*argv, "help") == 0) {
			usage();
		} else {
			fprintf(stderr, "Argument \"%s\" is unknown, try \"ip xfrm monitor help\".\n", *argv);
			exit(-1);
		}
		argc--;	argv++;
	}

	if (lacquire)
		groups |= XFRMGRP_ACQUIRE;
	if (lexpire)
		groups |= XFRMGRP_EXPIRE;
	if (lsa)
		groups |= XFRMGRP_SA;
	if (lpolicy)
		groups |= XFRMGRP_POLICY;

	if (file) {
		FILE *fp;
		fp = fopen(file, "r");
		if (fp == NULL) {
			perror("Cannot fopen");
			exit(-1);
		}
		return rtnl_from_file(fp, xfrm_accept_msg, (void*)stdout);
	}

	//ll_init_map(&rth);

	if (rtnl_open_byproto(&rth, groups, NETLINK_XFRM) < 0)
		exit(1);

	if (rtnl_listen(&rth, xfrm_accept_msg, (void*)stdout) < 0)
		exit(2);

	return 0;
}
