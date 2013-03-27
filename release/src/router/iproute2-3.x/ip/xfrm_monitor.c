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
	fprintf(stderr, "Usage: ip xfrm monitor [ all | LISTofXFRM-OBJECTS ]\n");
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

static int xfrm_state_flush_print(const struct sockaddr_nl *who,
				  struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	struct xfrm_usersa_flush *xsf = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	const char *str;

	len -= NLMSG_SPACE(sizeof(*xsf));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	fprintf(fp, "Flushed state ");

	str = strxf_xfrmproto(xsf->proto);
	if (str)
		fprintf(fp, "proto %s", str);
	else
		fprintf(fp, "proto %u", xsf->proto);
	fprintf(fp, "%s", _SL_);

	if (oneline)
		fprintf(fp, "\n");
	fflush(fp);

	return 0;
}

static int xfrm_policy_flush_print(const struct sockaddr_nl *who,
				   struct nlmsghdr *n, void *arg)
{
	struct rtattr * tb[XFRMA_MAX+1];
	FILE *fp = (FILE*)arg;
	int len = n->nlmsg_len;

	len -= NLMSG_SPACE(0);
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	fprintf(fp, "Flushed policy ");

	parse_rtattr(tb, XFRMA_MAX, NLMSG_DATA(n), len);

	if (tb[XFRMA_POLICY_TYPE]) {
		struct xfrm_userpolicy_type *upt;

		fprintf(fp, "ptype ");

		if (RTA_PAYLOAD(tb[XFRMA_POLICY_TYPE]) < sizeof(*upt))
			fprintf(fp, "(ERROR truncated)");

		upt = (struct xfrm_userpolicy_type *)RTA_DATA(tb[XFRMA_POLICY_TYPE]);
		fprintf(fp, "%s ", strxf_ptype(upt->type));
	}

	fprintf(fp, "%s", _SL_);

	if (oneline)
		fprintf(fp, "\n");
	fflush(fp);

	return 0;
}

static int xfrm_report_print(const struct sockaddr_nl *who,
			     struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	struct xfrm_user_report *xrep = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * tb[XFRMA_MAX+1];
	__u16 family;

	len -= NLMSG_LENGTH(sizeof(*xrep));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	family = xrep->sel.family;
	if (family == AF_UNSPEC)
		family = preferred_family;

	fprintf(fp, "report ");

	fprintf(fp, "proto %s ", strxf_xfrmproto(xrep->proto));
	fprintf(fp, "%s", _SL_);

	xfrm_selector_print(&xrep->sel, family, fp, "  sel ");

	parse_rtattr(tb, XFRMA_MAX, XFRMREP_RTA(xrep), len);

	xfrm_xfrma_print(tb, family, fp, "  ");

	if (oneline)
		fprintf(fp, "\n");

	return 0;
}

void xfrm_ae_flags_print(__u32 flags, void *arg)
{
	FILE *fp = (FILE*)arg;
	fprintf(fp, " (0x%x) ", flags);
	if (!flags)
		return;
	if (flags & XFRM_AE_CR)
		fprintf(fp, " replay update ");
	if (flags & XFRM_AE_CE)
		fprintf(fp, " timer expired ");
	if (flags & XFRM_AE_CU)
		fprintf(fp, " policy updated ");

}

static int xfrm_ae_print(const struct sockaddr_nl *who,
			     struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	struct xfrm_aevent_id *id = NLMSG_DATA(n);
	char abuf[256];

	fprintf(fp, "Async event ");
	xfrm_ae_flags_print(id->flags, arg);
	fprintf(fp,"\n\t");
	memset(abuf, '\0', sizeof(abuf));
	fprintf(fp, "src %s ", rt_addr_n2a(id->sa_id.family,
		sizeof(id->saddr), &id->saddr,
		abuf, sizeof(abuf)));
	memset(abuf, '\0', sizeof(abuf));
	fprintf(fp, "dst %s ", rt_addr_n2a(id->sa_id.family,
		sizeof(id->sa_id.daddr), &id->sa_id.daddr,
		abuf, sizeof(abuf)));
	fprintf(fp, " reqid 0x%x", id->reqid);
	fprintf(fp, " protocol %s ", strxf_proto(id->sa_id.proto));
	fprintf(fp, " SPI 0x%x", ntohl(id->sa_id.spi));

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

	switch (n->nlmsg_type) {
	case XFRM_MSG_NEWSA:
	case XFRM_MSG_DELSA:
	case XFRM_MSG_UPDSA:
	case XFRM_MSG_EXPIRE:
		xfrm_state_print(who, n, arg);
		return 0;
	case XFRM_MSG_NEWPOLICY:
	case XFRM_MSG_DELPOLICY:
	case XFRM_MSG_UPDPOLICY:
	case XFRM_MSG_POLEXPIRE:
		xfrm_policy_print(who, n, arg);
		return 0;
	case XFRM_MSG_ACQUIRE:
		xfrm_acquire_print(who, n, arg);
		return 0;
	case XFRM_MSG_FLUSHSA:
		xfrm_state_flush_print(who, n, arg);
		return 0;
	case XFRM_MSG_FLUSHPOLICY:
		xfrm_policy_flush_print(who, n, arg);
		return 0;
	case XFRM_MSG_REPORT:
		xfrm_report_print(who, n, arg);
		return 0;
	case XFRM_MSG_NEWAE:
		xfrm_ae_print(who, n, arg);
		return 0;
	default:
		break;
	}

	if (n->nlmsg_type != NLMSG_ERROR && n->nlmsg_type != NLMSG_NOOP &&
	    n->nlmsg_type != NLMSG_DONE) {
		fprintf(fp, "Unknown message: %08d 0x%08x 0x%08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
	}
	return 0;
}

extern struct rtnl_handle rth;

int do_xfrm_monitor(int argc, char **argv)
{
	char *file = NULL;
	unsigned groups = ~((unsigned)0); /* XXX */
	int lacquire=0;
	int lexpire=0;
	int laevent=0;
	int lpolicy=0;
	int lsa=0;
	int lreport=0;

	rtnl_close(&rth);

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
		} else if (matches(*argv, "aevent") == 0) {
			laevent=1;
			groups = 0;
		} else if (matches(*argv, "policy") == 0) {
			lpolicy=1;
			groups = 0;
		} else if (matches(*argv, "report") == 0) {
			lreport=1;
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
		groups |= nl_mgrp(XFRMNLGRP_ACQUIRE);
	if (lexpire)
		groups |= nl_mgrp(XFRMNLGRP_EXPIRE);
	if (lsa)
		groups |= nl_mgrp(XFRMNLGRP_SA);
	if (lpolicy)
		groups |= nl_mgrp(XFRMNLGRP_POLICY);
	if (laevent)
		groups |= nl_mgrp(XFRMNLGRP_AEVENTS);
	if (lreport)
		groups |= nl_mgrp(XFRMNLGRP_REPORT);

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
