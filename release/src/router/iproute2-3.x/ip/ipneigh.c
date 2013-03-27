/*
 * ipneigh.c		"ip neigh".
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
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

#define NUD_VALID	(NUD_PERMANENT|NUD_NOARP|NUD_REACHABLE|NUD_PROBE|NUD_STALE|NUD_DELAY)
#define MAX_ROUNDS	10

static struct
{
	int family;
        int index;
	int state;
	int unused_only;
	inet_prefix pfx;
	int flushed;
	char *flushb;
	int flushp;
	int flushe;
} filter;

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip neigh { add | del | change | replace } { ADDR [ lladdr LLADDR ]\n"
		        "          [ nud { permanent | noarp | stale | reachable } ]\n"
		        "          | proxy ADDR } [ dev DEV ]\n");
	fprintf(stderr, "       ip neigh {show|flush} [ to PREFIX ] [ dev DEV ] [ nud STATE ]\n");
	exit(-1);
}

int nud_state_a2n(unsigned *state, char *arg)
{
	if (matches(arg, "permanent") == 0)
		*state = NUD_PERMANENT;
	else if (matches(arg, "reachable") == 0)
		*state = NUD_REACHABLE;
	else if (strcmp(arg, "noarp") == 0)
		*state = NUD_NOARP;
	else if (strcmp(arg, "none") == 0)
		*state = NUD_NONE;
	else if (strcmp(arg, "stale") == 0)
		*state = NUD_STALE;
	else if (strcmp(arg, "incomplete") == 0)
		*state = NUD_INCOMPLETE;
	else if (strcmp(arg, "delay") == 0)
		*state = NUD_DELAY;
	else if (strcmp(arg, "probe") == 0)
		*state = NUD_PROBE;
	else if (matches(arg, "failed") == 0)
		*state = NUD_FAILED;
	else {
		if (get_unsigned(state, arg, 0))
			return -1;
		if (*state>=0x100 || (*state&((*state)-1)))
			return -1;
	}
	return 0;
}

static int flush_update(void)
{
	if (rtnl_send_check(&rth, filter.flushb, filter.flushp) < 0) {
		perror("Failed to send flush request");
		return -1;
	}
	filter.flushp = 0;
	return 0;
}


static int ipneigh_modify(int cmd, int flags, int argc, char **argv)
{
	struct {
		struct nlmsghdr 	n;
		struct ndmsg 		ndm;
		char   			buf[256];
	} req;
	char  *d = NULL;
	int dst_ok = 0;
	int lladdr_ok = 0;
	char * lla = NULL;
	inet_prefix dst;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ndmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST|flags;
	req.n.nlmsg_type = cmd;
	req.ndm.ndm_family = preferred_family;
	req.ndm.ndm_state = NUD_PERMANENT;

	while (argc > 0) {
		if (matches(*argv, "lladdr") == 0) {
			NEXT_ARG();
			if (lladdr_ok)
				duparg("lladdr", *argv);
			lla = *argv;
			lladdr_ok = 1;
		} else if (strcmp(*argv, "nud") == 0) {
			unsigned state;
			NEXT_ARG();
			if (nud_state_a2n(&state, *argv))
				invarg("nud state is bad", *argv);
			req.ndm.ndm_state = state;
		} else if (matches(*argv, "proxy") == 0) {
			NEXT_ARG();
			if (matches(*argv, "help") == 0)
				usage();
			if (dst_ok)
				duparg("address", *argv);
			get_addr(&dst, *argv, preferred_family);
			dst_ok = 1;
			req.ndm.ndm_flags |= NTF_PROXY;
		} else if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			d = *argv;
		} else {
			if (strcmp(*argv, "to") == 0) {
				NEXT_ARG();
			}
			if (matches(*argv, "help") == 0) {
				NEXT_ARG();
			}
			if (dst_ok)
				duparg2("to", *argv);
			get_addr(&dst, *argv, preferred_family);
			dst_ok = 1;
		}
		argc--; argv++;
	}
	if (d == NULL || !dst_ok || dst.family == AF_UNSPEC) {
		fprintf(stderr, "Device and destination are required arguments.\n");
		exit(-1);
	}
	req.ndm.ndm_family = dst.family;
	addattr_l(&req.n, sizeof(req), NDA_DST, &dst.data, dst.bytelen);

	if (lla && strcmp(lla, "null")) {
		char llabuf[20];
		int l;

		l = ll_addr_a2n(llabuf, sizeof(llabuf), lla);
		addattr_l(&req.n, sizeof(req), NDA_LLADDR, llabuf, l);
	}

	ll_init_map(&rth);

	if ((req.ndm.ndm_ifindex = ll_name_to_index(d)) == 0) {
		fprintf(stderr, "Cannot find device \"%s\"\n", d);
		return -1;
	}

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		exit(2);

	return 0;
}


int print_neigh(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	struct ndmsg *r = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * tb[NDA_MAX+1];
	char abuf[256];

	if (n->nlmsg_type != RTM_NEWNEIGH && n->nlmsg_type != RTM_DELNEIGH) {
		fprintf(stderr, "Not RTM_NEWNEIGH: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);

		return 0;
	}
	len -= NLMSG_LENGTH(sizeof(*r));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	if (filter.flushb && n->nlmsg_type != RTM_NEWNEIGH)
		return 0;

	if (filter.family && filter.family != r->ndm_family)
		return 0;
	if (filter.index && filter.index != r->ndm_ifindex)
		return 0;
	if (!(filter.state&r->ndm_state) &&
	    (r->ndm_state || !(filter.state&0x100)) &&
             (r->ndm_family != AF_DECnet))
		return 0;

	parse_rtattr(tb, NDA_MAX, NDA_RTA(r), n->nlmsg_len - NLMSG_LENGTH(sizeof(*r)));

	if (tb[NDA_DST]) {
		if (filter.pfx.family) {
			inet_prefix dst;
			memset(&dst, 0, sizeof(dst));
			dst.family = r->ndm_family;
			memcpy(&dst.data, RTA_DATA(tb[NDA_DST]), RTA_PAYLOAD(tb[NDA_DST]));
			if (inet_addr_match(&dst, &filter.pfx, filter.pfx.bitlen))
				return 0;
		}
	}
	if (filter.unused_only && tb[NDA_CACHEINFO]) {
		struct nda_cacheinfo *ci = RTA_DATA(tb[NDA_CACHEINFO]);
		if (ci->ndm_refcnt)
			return 0;
	}

	if (filter.flushb) {
		struct nlmsghdr *fn;
		if (NLMSG_ALIGN(filter.flushp) + n->nlmsg_len > filter.flushe) {
			if (flush_update())
				return -1;
		}
		fn = (struct nlmsghdr*)(filter.flushb + NLMSG_ALIGN(filter.flushp));
		memcpy(fn, n, n->nlmsg_len);
		fn->nlmsg_type = RTM_DELNEIGH;
		fn->nlmsg_flags = NLM_F_REQUEST;
		fn->nlmsg_seq = ++rth.seq;
		filter.flushp = (((char*)fn) + n->nlmsg_len) - filter.flushb;
		filter.flushed++;
		if (show_stats < 2)
			return 0;
	}

	if (tb[NDA_DST]) {
		fprintf(fp, "%s ",
			format_host(r->ndm_family,
				    RTA_PAYLOAD(tb[NDA_DST]),
				    RTA_DATA(tb[NDA_DST]),
				    abuf, sizeof(abuf)));
	}
	if (!filter.index && r->ndm_ifindex)
		fprintf(fp, "dev %s ", ll_index_to_name(r->ndm_ifindex));
	if (tb[NDA_LLADDR]) {
		SPRINT_BUF(b1);
		fprintf(fp, "lladdr %s", ll_addr_n2a(RTA_DATA(tb[NDA_LLADDR]),
					      RTA_PAYLOAD(tb[NDA_LLADDR]),
					      ll_index_to_type(r->ndm_ifindex),
					      b1, sizeof(b1)));
	}
	if (r->ndm_flags & NTF_ROUTER) {
		fprintf(fp, " router");
	}
	if (tb[NDA_CACHEINFO] && show_stats) {
		struct nda_cacheinfo *ci = RTA_DATA(tb[NDA_CACHEINFO]);
		int hz = get_user_hz();

		if (ci->ndm_refcnt)
			printf(" ref %d", ci->ndm_refcnt);
		fprintf(fp, " used %d/%d/%d", ci->ndm_used/hz,
		       ci->ndm_confirmed/hz, ci->ndm_updated/hz);
	}

	if (tb[NDA_PROBES] && show_stats) {
		__u32 p = *(__u32 *) RTA_DATA(tb[NDA_PROBES]);
		fprintf(fp, " probes %u", p);
	}

	if (r->ndm_state) {
		int nud = r->ndm_state;
		fprintf(fp, " ");

#define PRINT_FLAG(f) if (nud & NUD_##f) { \
	nud &= ~NUD_##f; fprintf(fp, #f "%s", nud ? "," : ""); }
		PRINT_FLAG(INCOMPLETE);
		PRINT_FLAG(REACHABLE);
		PRINT_FLAG(STALE);
		PRINT_FLAG(DELAY);
		PRINT_FLAG(PROBE);
		PRINT_FLAG(FAILED);
		PRINT_FLAG(NOARP);
		PRINT_FLAG(PERMANENT);
#undef PRINT_FLAG
	}
	fprintf(fp, "\n");

	fflush(fp);
	return 0;
}

void ipneigh_reset_filter()
{
	memset(&filter, 0, sizeof(filter));
	filter.state = ~0;
}

int do_show_or_flush(int argc, char **argv, int flush)
{
	char *filter_dev = NULL;
	int state_given = 0;

	ipneigh_reset_filter();

	if (!filter.family)
		filter.family = preferred_family;

	if (flush) {
		if (argc <= 0) {
			fprintf(stderr, "Flush requires arguments.\n");
			return -1;
		}
		filter.state = ~(NUD_PERMANENT|NUD_NOARP);
	} else
		filter.state = 0xFF & ~NUD_NOARP;

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			if (filter_dev)
				duparg("dev", *argv);
			filter_dev = *argv;
		} else if (strcmp(*argv, "unused") == 0) {
			filter.unused_only = 1;
		} else if (strcmp(*argv, "nud") == 0) {
			unsigned state;
			NEXT_ARG();
			if (!state_given) {
				state_given = 1;
				filter.state = 0;
			}
			if (nud_state_a2n(&state, *argv)) {
				if (strcmp(*argv, "all") != 0)
					invarg("nud state is bad", *argv);
				state = ~0;
				if (flush)
					state &= ~NUD_NOARP;
			}
			if (state == 0)
				state = 0x100;
			filter.state |= state;
		} else {
			if (strcmp(*argv, "to") == 0) {
				NEXT_ARG();
			}
			if (matches(*argv, "help") == 0)
				usage();
			get_prefix(&filter.pfx, *argv, filter.family);
			if (filter.family == AF_UNSPEC)
				filter.family = filter.pfx.family;
		}
		argc--; argv++;
	}

	ll_init_map(&rth);

	if (filter_dev) {
		if ((filter.index = ll_name_to_index(filter_dev)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", filter_dev);
			return -1;
		}
	}

	if (flush) {
		int round = 0;
		char flushb[4096-512];

		filter.flushb = flushb;
		filter.flushp = 0;
		filter.flushe = sizeof(flushb);
		filter.state &= ~NUD_FAILED;

		while (round < MAX_ROUNDS) {
			if (rtnl_wilddump_request(&rth, filter.family, RTM_GETNEIGH) < 0) {
				perror("Cannot send dump request");
				exit(1);
			}
			filter.flushed = 0;
			if (rtnl_dump_filter(&rth, print_neigh, stdout, NULL, NULL) < 0) {
				fprintf(stderr, "Flush terminated\n");
				exit(1);
			}
			if (filter.flushed == 0) {
				if (show_stats) {
					if (round == 0)
						printf("Nothing to flush.\n");
					else
						printf("*** Flush is complete after %d round%s ***\n", round, round>1?"s":"");
				}
				fflush(stdout);
				return 0;
			}
			round++;
			if (flush_update() < 0)
				exit(1);
			if (show_stats) {
				printf("\n*** Round %d, deleting %d entries ***\n", round, filter.flushed);
				fflush(stdout);
			}
		}
		printf("*** Flush not complete bailing out after %d rounds\n",
			MAX_ROUNDS);
		return 1;
	}

	if (rtnl_wilddump_request(&rth, filter.family, RTM_GETNEIGH) < 0) {
		perror("Cannot send dump request");
		exit(1);
	}

	if (rtnl_dump_filter(&rth, print_neigh, stdout, NULL, NULL) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}

	return 0;
}

int do_ipneigh(int argc, char **argv)
{
	if (argc > 0) {
		if (matches(*argv, "add") == 0)
			return ipneigh_modify(RTM_NEWNEIGH, NLM_F_CREATE|NLM_F_EXCL, argc-1, argv+1);
		if (matches(*argv, "change") == 0 ||
		    strcmp(*argv, "chg") == 0)
			return ipneigh_modify(RTM_NEWNEIGH, NLM_F_REPLACE, argc-1, argv+1);
		if (matches(*argv, "replace") == 0)
			return ipneigh_modify(RTM_NEWNEIGH, NLM_F_CREATE|NLM_F_REPLACE, argc-1, argv+1);
		if (matches(*argv, "delete") == 0)
			return ipneigh_modify(RTM_DELNEIGH, 0, argc-1, argv+1);
		if (matches(*argv, "get") == 0) {
			fprintf(stderr, "Sorry, \"neigh get\" is not implemented :-(\n");
			return -1;
		}
		if (matches(*argv, "show") == 0 ||
		    matches(*argv, "lst") == 0 ||
		    matches(*argv, "list") == 0)
			return do_show_or_flush(argc-1, argv+1, 0);
		if (matches(*argv, "flush") == 0)
			return do_show_or_flush(argc-1, argv+1, 1);
		if (matches(*argv, "help") == 0)
			usage();
	} else
		return do_show_or_flush(0, NULL, 0);

	fprintf(stderr, "Command \"%s\" is unknown, try \"ip neigh help\".\n", *argv);
	exit(-1);
}
