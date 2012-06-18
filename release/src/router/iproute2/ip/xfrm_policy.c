/* $USAGI: $ */

/*
 * Copyright (C)2004 USAGI/WIDE Project
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
 * based on iproute.c
 */
/*
 * Authors:
 *	Masahide NAKAMURA @USAGI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <linux/netlink.h>
#include <linux/xfrm.h>
#include "utils.h"
#include "xfrm.h"
#include "ip_common.h"

//#define NLMSG_DELETEALL_BUF_SIZE (4096-512)
#define NLMSG_DELETEALL_BUF_SIZE 8192

/*
 * Receiving buffer defines:
 * nlmsg
 *   data = struct xfrm_userpolicy_info
 *   rtattr
 *     data = struct xfrm_user_tmpl[]
 */
#define NLMSG_BUF_SIZE 4096
#define RTA_BUF_SIZE 2048
#define XFRM_TMPLS_BUF_SIZE 1024

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip xfrm policy { add | update } dir DIR SELECTOR [ index INDEX ] \n");
	fprintf(stderr, "        [ action ACTION ] [ priority PRIORITY ] [ LIMIT-LIST ] [ TMPL-LIST ]\n");
	fprintf(stderr, "Usage: ip xfrm policy { delete | get } dir DIR [ SELECTOR | index INDEX ]\n");
	fprintf(stderr, "Usage: ip xfrm policy { deleteall | list } [ dir DIR ] [ SELECTOR ]\n");
	fprintf(stderr, "        [ index INDEX ] [ action ACTION ] [ priority PRIORITY ]\n");
	fprintf(stderr, "Usage: ip xfrm policy flush\n");
	fprintf(stderr, "DIR := [ in | out | fwd ]\n");

	fprintf(stderr, "SELECTOR := src ADDR[/PLEN] dst ADDR[/PLEN] [ UPSPEC ] [ dev DEV ]\n");

	fprintf(stderr, "UPSPEC := proto PROTO [ [ sport PORT ] [ dport PORT ] |\n");
	fprintf(stderr, "                        [ type NUMBER ] [ code NUMBER ] ]\n");

	//fprintf(stderr, "DEV - device name(default=none)\n");

	fprintf(stderr, "ACTION := [ allow | block ](default=allow)\n");

	//fprintf(stderr, "PRIORITY - priority value(default=0)\n");

	fprintf(stderr, "LIMIT-LIST := [ LIMIT-LIST ] | [ limit LIMIT ]\n");
	fprintf(stderr, "LIMIT := [ [time-soft|time-hard|time-use-soft|time-use-hard] SECONDS ] |\n");
	fprintf(stderr, "         [ [byte-soft|byte-hard] SIZE ] | [ [packet-soft|packet-hard] NUMBER ]\n");

	fprintf(stderr, "TMPL-LIST := [ TMPL-LIST ] | [ tmpl TMPL ]\n");
	fprintf(stderr, "TMPL := ID [ mode MODE ] [ reqid REQID ] [ level LEVEL ]\n");
	fprintf(stderr, "ID := [ src ADDR ] [ dst ADDR ] [ proto XFRM_PROTO ] [ spi SPI ]\n");

	//fprintf(stderr, "XFRM_PROTO := [ esp | ah | comp ]\n");
	fprintf(stderr, "XFRM_PROTO := [ ");
	fprintf(stderr, "%s | ", strxf_xfrmproto(IPPROTO_ESP));
	fprintf(stderr, "%s | ", strxf_xfrmproto(IPPROTO_AH));
	fprintf(stderr, "%s", strxf_xfrmproto(IPPROTO_COMP));
	fprintf(stderr, " ]\n");

 	fprintf(stderr, "MODE := [ transport | tunnel ](default=transport)\n");
 	//fprintf(stderr, "REQID - number(default=0)\n");
	fprintf(stderr, "LEVEL := [ required | use ](default=required)\n");

	exit(-1);
}

static int xfrm_policy_dir_parse(__u8 *dir, int *argcp, char ***argvp)
{
	int argc = *argcp;
	char **argv = *argvp;

	if (strcmp(*argv, "in") == 0)
		*dir = XFRM_POLICY_IN;
	else if (strcmp(*argv, "out") == 0)
		*dir = XFRM_POLICY_OUT;
	else if (strcmp(*argv, "fwd") == 0)
		*dir = XFRM_POLICY_FWD;
	else
		invarg("\"DIR\" is invalid", *argv);

	*argcp = argc;
	*argvp = argv;

	return 0;
}

static int xfrm_tmpl_parse(struct xfrm_user_tmpl *tmpl,
			   int *argcp, char ***argvp)
{
	int argc = *argcp;
	char **argv = *argvp;
	char *idp = NULL;

	while (1) {
		if (strcmp(*argv, "mode") == 0) {
			NEXT_ARG();
			xfrm_mode_parse(&tmpl->mode,  &argc, &argv);
		} else if (strcmp(*argv, "reqid") == 0) {
			NEXT_ARG();
			xfrm_reqid_parse(&tmpl->reqid, &argc, &argv);
		} else if (strcmp(*argv, "level") == 0) {
			NEXT_ARG();

			if (strcmp(*argv, "required") == 0)
				tmpl->optional = 0;
			else if (strcmp(*argv, "use") == 0)
				tmpl->optional = 1;
			else
				invarg("\"LEVEL\" is invalid\n", *argv);

		} else {
			if (idp) {
				PREV_ARG(); /* back track */
				break;
			}
			idp = *argv;
			xfrm_id_parse(&tmpl->saddr, &tmpl->id, &tmpl->family,
				      0, &argc, &argv);
			if (preferred_family == AF_UNSPEC)
				preferred_family = tmpl->family;
		}

		if (!NEXT_ARG_OK())
			break;

		NEXT_ARG();
	}
	if (argc == *argcp)
		missarg("TMPL");

	*argcp = argc;
	*argvp = argv;

	return 0;
}

static int xfrm_policy_modify(int cmd, unsigned flags, int argc, char **argv)
{
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr			n;
		struct xfrm_userpolicy_info	xpinfo;
		char				buf[RTA_BUF_SIZE];
	} req;
	char *dirp = NULL;
	char *selp = NULL;
	char tmpls_buf[XFRM_TMPLS_BUF_SIZE];
	int tmpls_len = 0;

	memset(&req, 0, sizeof(req));
	memset(&tmpls_buf, 0, sizeof(tmpls_buf));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(req.xpinfo));
	req.n.nlmsg_flags = NLM_F_REQUEST|flags;
	req.n.nlmsg_type = cmd;
	req.xpinfo.sel.family = preferred_family;

	req.xpinfo.lft.soft_byte_limit = XFRM_INF;
	req.xpinfo.lft.hard_byte_limit = XFRM_INF;
	req.xpinfo.lft.soft_packet_limit = XFRM_INF;
	req.xpinfo.lft.hard_packet_limit = XFRM_INF;

	while (argc > 0) {
		if (strcmp(*argv, "dir") == 0) {
			if (dirp)
				duparg("dir", *argv);
			dirp = *argv;

			NEXT_ARG();
			xfrm_policy_dir_parse(&req.xpinfo.dir, &argc, &argv);

			filter.dir_mask = XFRM_FILTER_MASK_FULL;

		} else if (strcmp(*argv, "index") == 0) {
			NEXT_ARG();
			if (get_u32(&req.xpinfo.index, *argv, 0))
				invarg("\"INDEX\" is invalid", *argv);

			filter.index_mask = XFRM_FILTER_MASK_FULL;

		} else if (strcmp(*argv, "action") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "allow") == 0)
				req.xpinfo.action = XFRM_POLICY_ALLOW;
			else if (strcmp(*argv, "block") == 0)
				req.xpinfo.action = XFRM_POLICY_BLOCK;
			else
				invarg("\"action\" value is invalid\n", *argv);

			filter.action_mask = XFRM_FILTER_MASK_FULL;

		} else if (strcmp(*argv, "priority") == 0) {
			NEXT_ARG();
			if (get_u32(&req.xpinfo.priority, *argv, 0))
				invarg("\"PRIORITY\" is invalid", *argv);

			filter.priority_mask = XFRM_FILTER_MASK_FULL;

		} else if (strcmp(*argv, "limit") == 0) {
			NEXT_ARG();
			xfrm_lifetime_cfg_parse(&req.xpinfo.lft, &argc, &argv);
		} else if (strcmp(*argv, "tmpl") == 0) {
			struct xfrm_user_tmpl *tmpl;

			if (tmpls_len + sizeof(*tmpl) > sizeof(tmpls_buf)) {
				fprintf(stderr, "Too many tmpls: buffer overflow\n");
				exit(1);
			}
			tmpl = (struct xfrm_user_tmpl *)((char *)tmpls_buf + tmpls_len);

			tmpl->family = preferred_family;
			tmpl->aalgos = (~(__u32)0);
			tmpl->ealgos = (~(__u32)0);
			tmpl->calgos = (~(__u32)0);

			NEXT_ARG();
			xfrm_tmpl_parse(tmpl, &argc, &argv);

			tmpls_len += sizeof(*tmpl);
		} else {
			if (selp)
				duparg("unknown", *argv);
			selp = *argv;

			xfrm_selector_parse(&req.xpinfo.sel, &argc, &argv);
			if (preferred_family == AF_UNSPEC)
				preferred_family = req.xpinfo.sel.family;
		}

		argc--; argv++;
	}

	if (!dirp) {
		fprintf(stderr, "Not enough information: \"DIR\" is required.\n");
		exit(1);
	}

	if (tmpls_len > 0) {
		addattr_l(&req.n, sizeof(req), XFRMA_TMPL,
			  (void *)tmpls_buf, tmpls_len);
	}

	if (rtnl_open_byproto(&rth, 0, NETLINK_XFRM) < 0)
		exit(1);

	if (req.xpinfo.sel.family == AF_UNSPEC)
		req.xpinfo.sel.family = AF_INET;

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		exit(2);

	rtnl_close(&rth);

	return 0;
}

static int xfrm_policy_filter_match(struct xfrm_userpolicy_info *xpinfo)
{
	if (!filter.use)
		return 1;

	if ((xpinfo->dir^filter.xpinfo.dir)&filter.dir_mask)
		return 0;

	if (filter.sel_src_mask) {
		if (xfrm_addr_match(&xpinfo->sel.saddr, &filter.xpinfo.sel.saddr,
				    filter.sel_src_mask))
			return 0;
	}

	if (filter.sel_dst_mask) {
		if (xfrm_addr_match(&xpinfo->sel.daddr, &filter.xpinfo.sel.daddr,
				    filter.sel_dst_mask))
			return 0;
	}

	if ((xpinfo->sel.ifindex^filter.xpinfo.sel.ifindex)&filter.sel_dev_mask)
		return 0;

	if ((xpinfo->sel.proto^filter.xpinfo.sel.proto)&filter.upspec_proto_mask)
		return 0;

	if (filter.upspec_sport_mask) {
		if ((xpinfo->sel.sport^filter.xpinfo.sel.sport)&filter.upspec_sport_mask)
			return 0;
	}

	if (filter.upspec_dport_mask) {
		if ((xpinfo->sel.dport^filter.xpinfo.sel.dport)&filter.upspec_dport_mask)
			return 0;
	}

	if ((xpinfo->index^filter.xpinfo.index)&filter.index_mask)
		return 0;

	if ((xpinfo->action^filter.xpinfo.action)&filter.action_mask)
		return 0;

	if ((xpinfo->priority^filter.xpinfo.priority)&filter.priority_mask)
		return 0;

	return 1;
}

int xfrm_policy_print(const struct sockaddr_nl *who, struct nlmsghdr *n,
		      void *arg)
{
	struct rtattr * tb[XFRMA_MAX+1];
	struct rtattr * rta;
	struct xfrm_userpolicy_info *xpinfo = NULL;
	struct xfrm_user_polexpire *xpexp = NULL;
	struct xfrm_userpolicy_id *xpid = NULL;
	FILE *fp = (FILE*)arg;
	int len = n->nlmsg_len;

	if (n->nlmsg_type != XFRM_MSG_NEWPOLICY &&
	    n->nlmsg_type != XFRM_MSG_DELPOLICY &&
	    n->nlmsg_type != XFRM_MSG_UPDPOLICY &&
	    n->nlmsg_type != XFRM_MSG_POLEXPIRE) {
		fprintf(stderr, "Not a policy: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
		return 0;
	}

	if (n->nlmsg_type == XFRM_MSG_DELPOLICY)  {
		xpid = NLMSG_DATA(n);
		len -= NLMSG_LENGTH(sizeof(*xpid));
	} else if (n->nlmsg_type == XFRM_MSG_POLEXPIRE) {
		xpexp = NLMSG_DATA(n);
		xpinfo = &xpexp->pol;
		len -= NLMSG_LENGTH(sizeof(*xpexp));
	} else {
		xpexp = NULL;
		xpinfo = NLMSG_DATA(n);
		len -= NLMSG_LENGTH(sizeof(*xpinfo));
	}

	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	if (xpinfo && !xfrm_policy_filter_match(xpinfo))
		return 0;

	if (n->nlmsg_type == XFRM_MSG_DELPOLICY)
		fprintf(fp, "Deleted ");
	else if (n->nlmsg_type == XFRM_MSG_UPDPOLICY)
		fprintf(fp, "Updated ");
	else if (n->nlmsg_type == XFRM_MSG_POLEXPIRE)
		fprintf(fp, "Expired ");

	if (n->nlmsg_type == XFRM_MSG_DELPOLICY)
		rta = XFRMPID_RTA(xpid);
	else if (n->nlmsg_type == XFRM_MSG_POLEXPIRE)
		rta = XFRMPEXP_RTA(xpexp);
	else
		rta = XFRMP_RTA(xpinfo);

	parse_rtattr(tb, XFRMA_MAX, rta, len);

	if (n->nlmsg_type == XFRM_MSG_DELPOLICY) {
		//xfrm_policy_id_print();
		if (!tb[XFRMA_POLICY]) {
			fprintf(stderr, "Buggy XFRM_MSG_DELPOLICY: no XFRMA_POLICY\n");
			return -1;
		}
		if (RTA_PAYLOAD(tb[XFRMA_POLICY]) < sizeof(*xpinfo)) {
			fprintf(stderr, "Buggy XFRM_MSG_DELPOLICY: too short XFRMA_POLICY len\n");
			return -1;
		}
		xpinfo = (struct xfrm_userpolicy_info *)RTA_DATA(tb[XFRMA_POLICY]);
	}

	xfrm_policy_info_print(xpinfo, tb, fp, NULL, NULL);

	if (n->nlmsg_type == XFRM_MSG_POLEXPIRE) {
		fprintf(fp, "\t");
		fprintf(fp, "hard %u", xpexp->hard);
		fprintf(fp, "%s", _SL_);
	}

	if (oneline)
		fprintf(fp, "\n");
	fflush(fp);

	return 0;
}

static int xfrm_policy_get_or_delete(int argc, char **argv, int delete,
				     void *res_nlbuf)
{
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr			n;
		struct xfrm_userpolicy_id	xpid;
	} req;
	char *dirp = NULL;
	char *selp = NULL;
	char *indexp = NULL;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(req.xpid));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = delete ? XFRM_MSG_DELPOLICY : XFRM_MSG_GETPOLICY;

	while (argc > 0) {
		if (strcmp(*argv, "dir") == 0) {
			if (dirp)
				duparg("dir", *argv);
			dirp = *argv;

			NEXT_ARG();
			xfrm_policy_dir_parse(&req.xpid.dir, &argc, &argv);

		} else if (strcmp(*argv, "index") == 0) {
			if (indexp)
				duparg("index", *argv);
			indexp = *argv;

			NEXT_ARG();
			if (get_u32(&req.xpid.index, *argv, 0))
				invarg("\"INDEX\" is invalid", *argv);

		} else {
			if (selp)
				invarg("unknown", *argv);
			selp = *argv;

			xfrm_selector_parse(&req.xpid.sel, &argc, &argv);
			if (preferred_family == AF_UNSPEC)
				preferred_family = req.xpid.sel.family;

		}

		argc--; argv++;
	}

	if (!dirp) {
		fprintf(stderr, "Not enough information: \"DIR\" is required.\n");
		exit(1);
	}
	if (!selp && !indexp) {
		fprintf(stderr, "Not enough information: either \"SELECTOR\" or \"INDEX\" is required.\n");
		exit(1);
	}
	if (selp && indexp)
		duparg2("SELECTOR", "INDEX");

	if (rtnl_open_byproto(&rth, 0, NETLINK_XFRM) < 0)
		exit(1);

	if (req.xpid.sel.family == AF_UNSPEC)
		req.xpid.sel.family = AF_INET;

	if (rtnl_talk(&rth, &req.n, 0, 0, res_nlbuf, NULL, NULL) < 0)
		exit(2);

	rtnl_close(&rth);

	return 0;
}

static int xfrm_policy_delete(int argc, char **argv)
{
	return xfrm_policy_get_or_delete(argc, argv, 1, NULL);
}

static int xfrm_policy_get(int argc, char **argv)
{
	char buf[NLMSG_BUF_SIZE];
	struct nlmsghdr *n = (struct nlmsghdr *)buf;

	memset(buf, 0, sizeof(buf));

	xfrm_policy_get_or_delete(argc, argv, 0, n);

	if (xfrm_policy_print(NULL, n, (void*)stdout) < 0) {
		fprintf(stderr, "An error :-)\n");
		exit(1);
	}

	return 0;
}

/*
 * With an existing policy of nlmsg, make new nlmsg for deleting the policy
 * and store it to buffer.
 */
static int xfrm_policy_keep(const struct sockaddr_nl *who,
			    struct nlmsghdr *n,
			    void *arg)
{
	struct xfrm_buffer *xb = (struct xfrm_buffer *)arg;
	struct rtnl_handle *rth = xb->rth;
	struct xfrm_userpolicy_info *xpinfo = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct nlmsghdr *new_n;
	struct xfrm_userpolicy_id *xpid;

	if (n->nlmsg_type != XFRM_MSG_NEWPOLICY) {
		fprintf(stderr, "Not a policy: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
		return 0;
	}

	len -= NLMSG_LENGTH(sizeof(*xpinfo));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	if (!xfrm_policy_filter_match(xpinfo))
		return 0;

	if (xb->offset > xb->size) {
		fprintf(stderr, "Policy buffer overflow\n");
		return -1;
	}

	new_n = (struct nlmsghdr *)(xb->buf + xb->offset);
	new_n->nlmsg_len = NLMSG_LENGTH(sizeof(*xpid));
	new_n->nlmsg_flags = NLM_F_REQUEST;
	new_n->nlmsg_type = XFRM_MSG_DELPOLICY;
	new_n->nlmsg_seq = ++rth->seq;

	xpid = NLMSG_DATA(new_n);
	memcpy(&xpid->sel, &xpinfo->sel, sizeof(xpid->sel));
	xpid->dir = xpinfo->dir;
	xpid->index = xpinfo->index;

	xb->offset += new_n->nlmsg_len;
	xb->nlmsg_count ++;

	return 0;
}

static int xfrm_policy_list_or_deleteall(int argc, char **argv, int deleteall)
{
	char *selp = NULL;
	struct rtnl_handle rth;

	if (argc > 0)
		filter.use = 1;
	filter.xpinfo.sel.family = preferred_family;

	while (argc > 0) {
		if (strcmp(*argv, "dir") == 0) {
			NEXT_ARG();
			xfrm_policy_dir_parse(&filter.xpinfo.dir, &argc, &argv);

			filter.dir_mask = XFRM_FILTER_MASK_FULL;

		} else if (strcmp(*argv, "index") == 0) {
			NEXT_ARG();
			if (get_u32(&filter.xpinfo.index, *argv, 0))
				invarg("\"INDEX\" is invalid", *argv);

			filter.index_mask = XFRM_FILTER_MASK_FULL;

		} else if (strcmp(*argv, "action") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "allow") == 0)
				filter.xpinfo.action = XFRM_POLICY_ALLOW;
			else if (strcmp(*argv, "block") == 0)
				filter.xpinfo.action = XFRM_POLICY_BLOCK;
			else
				invarg("\"ACTION\" is invalid\n", *argv);

			filter.action_mask = XFRM_FILTER_MASK_FULL;

		} else if (strcmp(*argv, "priority") == 0) {
			NEXT_ARG();
			if (get_u32(&filter.xpinfo.priority, *argv, 0))
				invarg("\"PRIORITY\" is invalid", *argv);

			filter.priority_mask = XFRM_FILTER_MASK_FULL;

		} else {
			if (selp)
				invarg("unknown", *argv);
			selp = *argv;

			xfrm_selector_parse(&filter.xpinfo.sel, &argc, &argv);
			if (preferred_family == AF_UNSPEC)
				preferred_family = filter.xpinfo.sel.family;

		}

		argc--; argv++;
	}

	if (rtnl_open_byproto(&rth, 0, NETLINK_XFRM) < 0)
		exit(1);

	if (deleteall) {
		struct xfrm_buffer xb;
		char buf[NLMSG_DELETEALL_BUF_SIZE];
		int i;

		xb.buf = buf;
		xb.size = sizeof(buf);
		xb.rth = &rth;

		for (i = 0; ; i++) {
			xb.offset = 0;
			xb.nlmsg_count = 0;

			if (show_stats > 1)
				fprintf(stderr, "Delete-all round = %d\n", i);

			if (rtnl_wilddump_request(&rth, preferred_family, XFRM_MSG_GETPOLICY) < 0) {
				perror("Cannot send dump request");
				exit(1);
			}

			if (rtnl_dump_filter(&rth, xfrm_policy_keep, &xb, NULL, NULL) < 0) {
				fprintf(stderr, "Delete-all terminated\n");
				exit(1);
			}
			if (xb.nlmsg_count == 0) {
				if (show_stats > 1)
					fprintf(stderr, "Delete-all completed\n");
				break;
			}

			if (rtnl_send(&rth, xb.buf, xb.offset) < 0) {
				perror("Failed to send delete-all request\n");
				exit(1);
			}
			if (show_stats > 1)
				fprintf(stderr, "Delete-all nlmsg count = %d\n", xb.nlmsg_count);

			xb.offset = 0;
			xb.nlmsg_count = 0;
		}
	} else {
		if (rtnl_wilddump_request(&rth, preferred_family, XFRM_MSG_GETPOLICY) < 0) {
			perror("Cannot send dump request");
			exit(1);
		}

		if (rtnl_dump_filter(&rth, xfrm_policy_print, stdout, NULL, NULL) < 0) {
			fprintf(stderr, "Dump terminated\n");
			exit(1);
		}
	}

	rtnl_close(&rth);

	exit(0);
}

static int xfrm_policy_flush(void)
{
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr	n;
	} req;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(0); /* nlmsg data is nothing */
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = XFRM_MSG_FLUSHPOLICY;

	if (rtnl_open_byproto(&rth, 0, NETLINK_XFRM) < 0)
		exit(1);

	if (show_stats > 1)
		fprintf(stderr, "Flush policy\n");

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		exit(2);

	rtnl_close(&rth);

	return 0;
}

int do_xfrm_policy(int argc, char **argv)
{
	if (argc < 1)
		return xfrm_policy_list_or_deleteall(0, NULL, 0);

	if (matches(*argv, "add") == 0)
		return xfrm_policy_modify(XFRM_MSG_NEWPOLICY, 0,
					  argc-1, argv+1);
	if (matches(*argv, "update") == 0)
		return xfrm_policy_modify(XFRM_MSG_UPDPOLICY, 0,
					  argc-1, argv+1);
	if (matches(*argv, "delete") == 0)
		return xfrm_policy_delete(argc-1, argv+1);
	if (matches(*argv, "deleteall") == 0 || matches(*argv, "delall") == 0)
		return xfrm_policy_list_or_deleteall(argc-1, argv+1, 1);
	if (matches(*argv, "list") == 0 || matches(*argv, "show") == 0
	    || matches(*argv, "lst") == 0)
		return xfrm_policy_list_or_deleteall(argc-1, argv+1, 0);
	if (matches(*argv, "get") == 0)
		return xfrm_policy_get(argc-1, argv+1);
	if (matches(*argv, "flush") == 0)
		return xfrm_policy_flush();
	if (matches(*argv, "help") == 0)
		usage();
	fprintf(stderr, "Command \"%s\" is unknown, try \"ip xfrm policy help\".\n", *argv);
	exit(-1);
}
