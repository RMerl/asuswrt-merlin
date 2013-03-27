/*
 * Copyright (C)2006 USAGI/WIDE Project
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
 * based on ipneigh.c
 */
/*
 * Authors:
 *	Masahide NAKAMURA @USAGI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "utils.h"
#include "ip_common.h"

static struct
{
	int family;
        int index;
#define NONE_DEV	(-1)
	char name[1024];
} filter;

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr,
		"Usage: ip ntable change name NAME [ dev DEV ]\n"
		"          [ thresh1 VAL ] [ thresh2 VAL ] [ thresh3 VAL ] [ gc_int MSEC ]\n"
		"          [ PARMS ]\n"
		"Usage: ip ntable show [ dev DEV ] [ name NAME ]\n"

		"PARMS := [ base_reachable MSEC ] [ retrans MSEC ] [ gc_stale MSEC ]\n"
		"         [ delay_probe MSEC ] [ queue LEN ]\n"
		"         [ app_probs VAL ] [ ucast_probes VAL ] [ mcast_probes VAL ]\n"
		"         [ anycast_delay MSEC ] [ proxy_delay MSEC ] [ proxy_queue LEN ]\n"
		"         [ locktime MSEC ]\n"
		);

	exit(-1);
}

static int ipntable_modify(int cmd, int flags, int argc, char **argv)
{
	struct {
		struct nlmsghdr 	n;
		struct ndtmsg		ndtm;
		char   			buf[1024];
	} req;
	char *namep = NULL;
	char *threshsp = NULL;
	char *gc_intp = NULL;
	char parms_buf[1024];
	struct rtattr *parms_rta = (struct rtattr *)parms_buf;
	int parms_change = 0;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ndtmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST|flags;
	req.n.nlmsg_type = cmd;

	req.ndtm.ndtm_family = preferred_family;
	req.ndtm.ndtm_pad1 = 0;
	req.ndtm.ndtm_pad2 = 0;

	memset(&parms_buf, 0, sizeof(parms_buf));

	parms_rta->rta_type = NDTA_PARMS;
	parms_rta->rta_len = RTA_LENGTH(0);

	while (argc > 0) {
		if (strcmp(*argv, "name") == 0) {
			int len;

			NEXT_ARG();
			if (namep)
				duparg("NAME", *argv);

			namep = *argv;
			len = strlen(namep) + 1;
			addattr_l(&req.n, sizeof(req), NDTA_NAME, namep, len);
		} else if (strcmp(*argv, "thresh1") == 0) {
			__u32 thresh1;

			NEXT_ARG();
			threshsp = *argv;

			if (get_u32(&thresh1, *argv, 0))
				invarg("\"thresh1\" value is invalid", *argv);

			addattr32(&req.n, sizeof(req), NDTA_THRESH1, thresh1);
		} else if (strcmp(*argv, "thresh2") == 0) {
			__u32 thresh2;

			NEXT_ARG();
			threshsp = *argv;

			if (get_u32(&thresh2, *argv, 0))
				invarg("\"thresh2\" value is invalid", *argv);

			addattr32(&req.n, sizeof(req), NDTA_THRESH2, thresh2);
		} else if (strcmp(*argv, "thresh3") == 0) {
			__u32 thresh3;

			NEXT_ARG();
			threshsp = *argv;

			if (get_u32(&thresh3, *argv, 0))
				invarg("\"thresh3\" value is invalid", *argv);

			addattr32(&req.n, sizeof(req), NDTA_THRESH3, thresh3);
		} else if (strcmp(*argv, "gc_int") == 0) {
			__u64 gc_int;

			NEXT_ARG();
			gc_intp = *argv;

			if (get_u64(&gc_int, *argv, 0))
				invarg("\"gc_int\" value is invalid", *argv);

			addattr_l(&req.n, sizeof(req), NDTA_GC_INTERVAL,
				  &gc_int, sizeof(gc_int));
		} else if (strcmp(*argv, "dev") == 0) {
			__u32 ifindex;

			NEXT_ARG();
			ifindex = ll_name_to_index(*argv);
			if (ifindex == 0) {
				fprintf(stderr, "Cannot find device \"%s\"\n", *argv);
				return -1;
			}

			rta_addattr32(parms_rta, sizeof(parms_buf),
				      NDTPA_IFINDEX, ifindex);
		} else if (strcmp(*argv, "base_reachable") == 0) {
			__u64 breachable;

			NEXT_ARG();

			if (get_u64(&breachable, *argv, 0))
				invarg("\"base_reachable\" value is invalid", *argv);

			rta_addattr_l(parms_rta, sizeof(parms_buf),
				      NDTPA_BASE_REACHABLE_TIME,
				      &breachable, sizeof(breachable));
			parms_change = 1;
		} else if (strcmp(*argv, "retrans") == 0) {
			__u64 retrans;

			NEXT_ARG();

			if (get_u64(&retrans, *argv, 0))
				invarg("\"retrans\" value is invalid", *argv);

			rta_addattr_l(parms_rta, sizeof(parms_buf),
				      NDTPA_RETRANS_TIME,
				      &retrans, sizeof(retrans));
			parms_change = 1;
		} else if (strcmp(*argv, "gc_stale") == 0) {
			__u64 gc_stale;

			NEXT_ARG();

			if (get_u64(&gc_stale, *argv, 0))
				invarg("\"gc_stale\" value is invalid", *argv);

			rta_addattr_l(parms_rta, sizeof(parms_buf),
				      NDTPA_GC_STALETIME,
				      &gc_stale, sizeof(gc_stale));
			parms_change = 1;
		} else if (strcmp(*argv, "delay_probe") == 0) {
			__u64 delay_probe;

			NEXT_ARG();

			if (get_u64(&delay_probe, *argv, 0))
				invarg("\"delay_probe\" value is invalid", *argv);

			rta_addattr_l(parms_rta, sizeof(parms_buf),
				      NDTPA_DELAY_PROBE_TIME,
				      &delay_probe, sizeof(delay_probe));
			parms_change = 1;
		} else if (strcmp(*argv, "queue") == 0) {
			__u32 queue;

			NEXT_ARG();

			if (get_u32(&queue, *argv, 0))
				invarg("\"queue\" value is invalid", *argv);

			if (!parms_rta)
				parms_rta = (struct rtattr *)&parms_buf;
			rta_addattr32(parms_rta, sizeof(parms_buf),
				      NDTPA_QUEUE_LEN, queue);
			parms_change = 1;
		} else if (strcmp(*argv, "app_probes") == 0) {
			__u32 aprobe;

			NEXT_ARG();

			if (get_u32(&aprobe, *argv, 0))
				invarg("\"app_probes\" value is invalid", *argv);

			rta_addattr32(parms_rta, sizeof(parms_buf),
				      NDTPA_APP_PROBES, aprobe);
			parms_change = 1;
		} else if (strcmp(*argv, "ucast_probes") == 0) {
			__u32 uprobe;

			NEXT_ARG();

			if (get_u32(&uprobe, *argv, 0))
				invarg("\"ucast_probes\" value is invalid", *argv);

			rta_addattr32(parms_rta, sizeof(parms_buf),
				      NDTPA_UCAST_PROBES, uprobe);
			parms_change = 1;
		} else if (strcmp(*argv, "mcast_probes") == 0) {
			__u32 mprobe;

			NEXT_ARG();

			if (get_u32(&mprobe, *argv, 0))
				invarg("\"mcast_probes\" value is invalid", *argv);

			rta_addattr32(parms_rta, sizeof(parms_buf),
				      NDTPA_MCAST_PROBES, mprobe);
			parms_change = 1;
		} else if (strcmp(*argv, "anycast_delay") == 0) {
			__u64 anycast_delay;

			NEXT_ARG();

			if (get_u64(&anycast_delay, *argv, 0))
				invarg("\"anycast_delay\" value is invalid", *argv);

			rta_addattr_l(parms_rta, sizeof(parms_buf),
				      NDTPA_ANYCAST_DELAY,
				      &anycast_delay, sizeof(anycast_delay));
			parms_change = 1;
		} else if (strcmp(*argv, "proxy_delay") == 0) {
			__u64 proxy_delay;

			NEXT_ARG();

			if (get_u64(&proxy_delay, *argv, 0))
				invarg("\"proxy_delay\" value is invalid", *argv);

			rta_addattr_l(parms_rta, sizeof(parms_buf),
				      NDTPA_PROXY_DELAY,
				      &proxy_delay, sizeof(proxy_delay));
			parms_change = 1;
		} else if (strcmp(*argv, "proxy_queue") == 0) {
			__u32 pqueue;

			NEXT_ARG();

			if (get_u32(&pqueue, *argv, 0))
				invarg("\"proxy_queue\" value is invalid", *argv);

			rta_addattr32(parms_rta, sizeof(parms_buf),
				      NDTPA_PROXY_QLEN, pqueue);
			parms_change = 1;
		} else if (strcmp(*argv, "locktime") == 0) {
			__u64 locktime;

			NEXT_ARG();

			if (get_u64(&locktime, *argv, 0))
				invarg("\"locktime\" value is invalid", *argv);

			rta_addattr_l(parms_rta, sizeof(parms_buf),
				      NDTPA_LOCKTIME,
				      &locktime, sizeof(locktime));
			parms_change = 1;
		} else {
			invarg("unknown", *argv);
		}

		argc--; argv++;
	}

	if (!namep)
		missarg("NAME");
	if (!threshsp && !gc_intp && !parms_change) {
		fprintf(stderr, "Not enough information: changable attributes required.\n");
		exit(-1);
	}

	if (parms_rta->rta_len > RTA_LENGTH(0)) {
		addattr_l(&req.n, sizeof(req), NDTA_PARMS, RTA_DATA(parms_rta),
			  RTA_PAYLOAD(parms_rta));
	}

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		exit(2);

	return 0;
}

static const char *ntable_strtime_delta(__u32 msec)
{
	static char str[32];
	struct timeval now;
	time_t t;
	struct tm *tp;

	if (msec == 0)
		goto error;

	memset(&now, 0, sizeof(now));

	if (gettimeofday(&now, NULL) < 0) {
		perror("gettimeofday");
		goto error;
	}

	t = now.tv_sec - (msec / 1000);
	tp = localtime(&t);
	if (!tp)
		goto error;

	strftime(str, sizeof(str), "%Y-%m-%d %T", tp);

	return str;
 error:
	strcpy(str, "(error)");
	return str;
}

int print_ntable(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	struct ndtmsg *ndtm = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr *tb[NDTA_MAX+1];
	struct rtattr *tpb[NDTPA_MAX+1];
	int ret;

	if (n->nlmsg_type != RTM_NEWNEIGHTBL) {
		fprintf(stderr, "Not NEIGHTBL: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
		return 0;
	}
	len -= NLMSG_LENGTH(sizeof(*ndtm));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	if (preferred_family && preferred_family != ndtm->ndtm_family)
		return 0;

	parse_rtattr(tb, NDTA_MAX, NDTA_RTA(ndtm),
		     n->nlmsg_len - NLMSG_LENGTH(sizeof(*ndtm)));

	if (tb[NDTA_NAME]) {
		char *name = RTA_DATA(tb[NDTA_NAME]);

		if (strlen(filter.name) > 0 && strcmp(filter.name, name))
			return 0;
	}
	if (tb[NDTA_PARMS]) {
		parse_rtattr(tpb, NDTPA_MAX, RTA_DATA(tb[NDTA_PARMS]),
			     RTA_PAYLOAD(tb[NDTA_PARMS]));

		if (tpb[NDTPA_IFINDEX]) {
			__u32 ifindex = *(__u32 *)RTA_DATA(tpb[NDTPA_IFINDEX]);

			if (filter.index && filter.index != ifindex)
				return 0;
		} else {
			if (filter.index && filter.index != NONE_DEV)
				return 0;
		}
	}

	if (ndtm->ndtm_family == AF_INET)
		fprintf(fp, "inet ");
	else if (ndtm->ndtm_family == AF_INET6)
		fprintf(fp, "inet6 ");
	else if (ndtm->ndtm_family == AF_DECnet)
		fprintf(fp, "dnet ");
	else
		fprintf(fp, "(%d) ", ndtm->ndtm_family);

	if (tb[NDTA_NAME]) {
		char *name = RTA_DATA(tb[NDTA_NAME]);
		fprintf(fp, "%s ", name);
	}

	fprintf(fp, "%s", _SL_);

	ret = (tb[NDTA_THRESH1] || tb[NDTA_THRESH2] || tb[NDTA_THRESH3] ||
	       tb[NDTA_GC_INTERVAL]);
	if (ret)
		fprintf(fp, "    ");

	if (tb[NDTA_THRESH1]) {
		__u32 thresh1 = *(__u32 *)RTA_DATA(tb[NDTA_THRESH1]);
		fprintf(fp, "thresh1 %u ", thresh1);
	}
	if (tb[NDTA_THRESH2]) {
		__u32 thresh2 = *(__u32 *)RTA_DATA(tb[NDTA_THRESH2]);
		fprintf(fp, "thresh2 %u ", thresh2);
	}
	if (tb[NDTA_THRESH3]) {
		__u32 thresh3 = *(__u32 *)RTA_DATA(tb[NDTA_THRESH3]);
		fprintf(fp, "thresh3 %u ", thresh3);
	}
	if (tb[NDTA_GC_INTERVAL]) {
		__u64 gc_int = *(__u64 *)RTA_DATA(tb[NDTA_GC_INTERVAL]);
		fprintf(fp, "gc_int %llu ", gc_int);
	}

	if (ret)
		fprintf(fp, "%s", _SL_);

	if (tb[NDTA_CONFIG] && show_stats) {
		struct ndt_config *ndtc = RTA_DATA(tb[NDTA_CONFIG]);

		fprintf(fp, "    ");
		fprintf(fp, "config ");

		fprintf(fp, "key_len %u ", ndtc->ndtc_key_len);
		fprintf(fp, "entry_size %u ", ndtc->ndtc_entry_size);
		fprintf(fp, "entries %u ", ndtc->ndtc_entries);

		fprintf(fp, "%s", _SL_);
		fprintf(fp, "        ");

		fprintf(fp, "last_flush %s ",
			ntable_strtime_delta(ndtc->ndtc_last_flush));
		fprintf(fp, "last_rand %s ",
			ntable_strtime_delta(ndtc->ndtc_last_rand));

		fprintf(fp, "%s", _SL_);
		fprintf(fp, "        ");

		fprintf(fp, "hash_rnd %u ", ndtc->ndtc_hash_rnd);
		fprintf(fp, "hash_mask %08x ", ndtc->ndtc_hash_mask);

		fprintf(fp, "hash_chain_gc %u ", ndtc->ndtc_hash_chain_gc);
		fprintf(fp, "proxy_qlen %u ", ndtc->ndtc_proxy_qlen);

		fprintf(fp, "%s", _SL_);
	}

	if (tb[NDTA_PARMS]) {
		if (tpb[NDTPA_IFINDEX]) {
			__u32 ifindex = *(__u32 *)RTA_DATA(tpb[NDTPA_IFINDEX]);

			fprintf(fp, "    ");
			fprintf(fp, "dev %s ", ll_index_to_name(ifindex));
			fprintf(fp, "%s", _SL_);
		}

		fprintf(fp, "    ");

		if (tpb[NDTPA_REFCNT]) {
			__u32 refcnt = *(__u32 *)RTA_DATA(tpb[NDTPA_REFCNT]);
			fprintf(fp, "refcnt %u ", refcnt);
		}
		if (tpb[NDTPA_REACHABLE_TIME]) {
			__u64 reachable = *(__u64 *)RTA_DATA(tpb[NDTPA_REACHABLE_TIME]);
			fprintf(fp, "reachable %llu ", reachable);
		}
		if (tpb[NDTPA_BASE_REACHABLE_TIME]) {
			__u64 breachable = *(__u64 *)RTA_DATA(tpb[NDTPA_BASE_REACHABLE_TIME]);
			fprintf(fp, "base_reachable %llu ", breachable);
		}
		if (tpb[NDTPA_RETRANS_TIME]) {
			__u64 retrans = *(__u64 *)RTA_DATA(tpb[NDTPA_RETRANS_TIME]);
			fprintf(fp, "retrans %llu ", retrans);
		}

		fprintf(fp, "%s", _SL_);

		fprintf(fp, "    ");

		if (tpb[NDTPA_GC_STALETIME]) {
			__u64 gc_stale = *(__u64 *)RTA_DATA(tpb[NDTPA_GC_STALETIME]);
			fprintf(fp, "gc_stale %llu ", gc_stale);
		}
		if (tpb[NDTPA_DELAY_PROBE_TIME]) {
			__u64 delay_probe = *(__u64 *)RTA_DATA(tpb[NDTPA_DELAY_PROBE_TIME]);
			fprintf(fp, "delay_probe %llu ", delay_probe);
		}
		if (tpb[NDTPA_QUEUE_LEN]) {
			__u32 queue = *(__u32 *)RTA_DATA(tpb[NDTPA_QUEUE_LEN]);
			fprintf(fp, "queue %u ", queue);
		}

		fprintf(fp, "%s", _SL_);

		fprintf(fp, "    ");

		if (tpb[NDTPA_APP_PROBES]) {
			__u32 aprobe = *(__u32 *)RTA_DATA(tpb[NDTPA_APP_PROBES]);
			fprintf(fp, "app_probes %u ", aprobe);
		}
		if (tpb[NDTPA_UCAST_PROBES]) {
			__u32 uprobe = *(__u32 *)RTA_DATA(tpb[NDTPA_UCAST_PROBES]);
			fprintf(fp, "ucast_probes %u ", uprobe);
		}
		if (tpb[NDTPA_MCAST_PROBES]) {
			__u32 mprobe = *(__u32 *)RTA_DATA(tpb[NDTPA_MCAST_PROBES]);
			fprintf(fp, "mcast_probes %u ", mprobe);
		}

		fprintf(fp, "%s", _SL_);

		fprintf(fp, "    ");

		if (tpb[NDTPA_ANYCAST_DELAY]) {
			__u64 anycast_delay = *(__u64 *)RTA_DATA(tpb[NDTPA_ANYCAST_DELAY]);
			fprintf(fp, "anycast_delay %llu ", anycast_delay);
		}
		if (tpb[NDTPA_PROXY_DELAY]) {
			__u64 proxy_delay = *(__u64 *)RTA_DATA(tpb[NDTPA_PROXY_DELAY]);
			fprintf(fp, "proxy_delay %llu ", proxy_delay);
		}
		if (tpb[NDTPA_PROXY_QLEN]) {
			__u32 pqueue = *(__u32 *)RTA_DATA(tpb[NDTPA_PROXY_QLEN]);
			fprintf(fp, "proxy_queue %u ", pqueue);
		}
		if (tpb[NDTPA_LOCKTIME]) {
			__u64 locktime = *(__u64 *)RTA_DATA(tpb[NDTPA_LOCKTIME]);
			fprintf(fp, "locktime %llu ", locktime);
		}

		fprintf(fp, "%s", _SL_);
	}

	if (tb[NDTA_STATS] && show_stats) {
		struct ndt_stats *ndts = RTA_DATA(tb[NDTA_STATS]);

		fprintf(fp, "    ");
		fprintf(fp, "stats ");

		fprintf(fp, "allocs %llu ", ndts->ndts_allocs);
		fprintf(fp, "destroys %llu ", ndts->ndts_destroys);
		fprintf(fp, "hash_grows %llu ", ndts->ndts_hash_grows);

		fprintf(fp, "%s", _SL_);
		fprintf(fp, "        ");

		fprintf(fp, "res_failed %llu ", ndts->ndts_res_failed);
		fprintf(fp, "lookups %llu ", ndts->ndts_lookups);
		fprintf(fp, "hits %llu ", ndts->ndts_hits);

		fprintf(fp, "%s", _SL_);
		fprintf(fp, "        ");

		fprintf(fp, "rcv_probes_mcast %llu ", ndts->ndts_rcv_probes_mcast);
		fprintf(fp, "rcv_probes_ucast %llu ", ndts->ndts_rcv_probes_ucast);

		fprintf(fp, "%s", _SL_);
		fprintf(fp, "        ");

		fprintf(fp, "periodic_gc_runs %llu ", ndts->ndts_periodic_gc_runs);
		fprintf(fp, "forced_gc_runs %llu ", ndts->ndts_forced_gc_runs);

		fprintf(fp, "%s", _SL_);
	}

	fprintf(fp, "\n");

	fflush(fp);
	return 0;
}

void ipntable_reset_filter(void)
{
	memset(&filter, 0, sizeof(filter));
}

static int ipntable_show(int argc, char **argv)
{
	ipntable_reset_filter();

	filter.family = preferred_family;

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();

			if (strcmp("none", *argv) == 0)
				filter.index = NONE_DEV;
			else if ((filter.index = ll_name_to_index(*argv)) == 0)
				invarg("\"DEV\" is invalid", *argv);
		} else if (strcmp(*argv, "name") == 0) {
			NEXT_ARG();

			strncpy(filter.name, *argv, sizeof(filter.name));
		} else
			invarg("unknown", *argv);

		argc--; argv++;
	}

	if (rtnl_wilddump_request(&rth, preferred_family, RTM_GETNEIGHTBL) < 0) {
		perror("Cannot send dump request");
		exit(1);
	}

	if (rtnl_dump_filter(&rth, print_ntable, stdout, NULL, NULL) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}

	return 0;
}

int do_ipntable(int argc, char **argv)
{
	ll_init_map(&rth);

	if (argc > 0) {
		if (matches(*argv, "change") == 0 ||
		    matches(*argv, "chg") == 0)
			return ipntable_modify(RTM_SETNEIGHTBL,
					       NLM_F_REPLACE,
					       argc-1, argv+1);
		if (matches(*argv, "show") == 0 ||
		    matches(*argv, "lst") == 0 ||
		    matches(*argv, "list") == 0)
			return ipntable_show(argc-1, argv+1);
		if (matches(*argv, "help") == 0)
			usage();
	} else
		return ipntable_show(0, NULL);

	fprintf(stderr, "Command \"%s\" is unknown, try \"ip ntable help\".\n", *argv);
	exit(-1);
}
