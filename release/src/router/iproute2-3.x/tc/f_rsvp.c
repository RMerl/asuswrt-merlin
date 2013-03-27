/*
 * q_rsvp.c		RSVP filter.
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

#include "rt_names.h"
#include "utils.h"
#include "tc_util.h"

static void explain(void)
{
	fprintf(stderr, "Usage: ... rsvp ipproto PROTOCOL session DST[/PORT | GPI ]\n");
	fprintf(stderr, "                [ sender SRC[/PORT | GPI ]\n");
	fprintf(stderr, "                [ classid CLASSID ] [ police POLICE_SPEC ]\n");
	fprintf(stderr, "                [ tunnelid ID ] [ tunnel ID skip NUMBER ]\n");
	fprintf(stderr, "Where: GPI := { flowlabel NUMBER | spi/ah SPI | spi/esp SPI |\n");
	fprintf(stderr, "                u{8|16|32} NUMBER mask MASK at OFFSET}\n");
	fprintf(stderr, "       POLICE_SPEC := ... look at TBF\n");
	fprintf(stderr, "       FILTERID := X:Y\n");
	fprintf(stderr, "\nNOTE: CLASSID is parsed as hexadecimal input.\n");
}

int get_addr_and_pi(int *argc_p, char ***argv_p, inet_prefix * addr,
		    struct tc_rsvp_pinfo *pinfo, int dir, int family)
{
	int argc = *argc_p;
	char **argv = *argv_p;
	char *p = strchr(*argv, '/');
	struct tc_rsvp_gpi *pi = dir ? &pinfo->dpi : &pinfo->spi;

	if (p) {
		__u16 tmp;

		if (get_u16(&tmp, p+1, 0))
			return -1;

		if (dir == 0) {
			/* Source port: u16 at offset 0 */
			pi->key = htonl(((__u32)tmp)<<16);
			pi->mask = htonl(0xFFFF0000);
		} else {
			/* Destination port: u16 at offset 2 */
			pi->key = htonl(((__u32)tmp));
			pi->mask = htonl(0x0000FFFF);
		}
		pi->offset = 0;
		*p = 0;
	}
	if (get_addr_1(addr, *argv, family))
		return -1;
	if (p)
		*p = '/';

	argc--; argv++;

	if (pi->mask || argc <= 0)
		goto done;

	if (strcmp(*argv, "spi/ah") == 0 ||
	    strcmp(*argv, "gpi/ah") == 0) {
		__u32 gpi;
		NEXT_ARG();
		if (get_u32(&gpi, *argv, 0))
			return -1;
		pi->mask = htonl(0xFFFFFFFF);
		pi->key = htonl(gpi);
		pi->offset = 4;
		if (pinfo->protocol == 0)
			pinfo->protocol = IPPROTO_AH;
		argc--; argv++;
	} else if (strcmp(*argv, "spi/esp") == 0 ||
		   strcmp(*argv, "gpi/esp") == 0) {
		__u32 gpi;
		NEXT_ARG();
		if (get_u32(&gpi, *argv, 0))
			return -1;
		pi->mask = htonl(0xFFFFFFFF);
		pi->key = htonl(gpi);
		pi->offset = 0;
		if (pinfo->protocol == 0)
			pinfo->protocol = IPPROTO_ESP;
		argc--; argv++;
	} else if (strcmp(*argv, "flowlabel") == 0) {
		__u32 flabel;
		NEXT_ARG();
		if (get_u32(&flabel, *argv, 0))
			return -1;
		if (family != AF_INET6)
			return -1;
		pi->mask = htonl(0x000FFFFF);
		pi->key = htonl(flabel) & pi->mask;
		pi->offset = -40;
		argc--; argv++;
	} else if (strcmp(*argv, "u32") == 0 ||
		   strcmp(*argv, "u16") == 0 ||
		   strcmp(*argv, "u8") == 0) {
		int sz = 1;
		__u32 tmp;
		__u32 mask = 0xff;
		if (strcmp(*argv, "u32") == 0) {
			sz = 4;
			mask = 0xffff;
		} else if (strcmp(*argv, "u16") == 0) {
			mask = 0xffffffff;
			sz = 2;
		}
		NEXT_ARG();
		if (get_u32(&tmp, *argv, 0))
			return -1;
		argc--; argv++;
		if (strcmp(*argv, "mask") == 0) {
			NEXT_ARG();
			if (get_u32(&mask, *argv, 16))
				return -1;
			argc--; argv++;
		}
		if (strcmp(*argv, "at") == 0) {
			NEXT_ARG();
			if (get_integer(&pi->offset, *argv, 0))
				return -1;
			argc--; argv++;
		}
		if (sz == 1) {
			if ((pi->offset & 3) == 0) {
				mask <<= 24;
				tmp <<= 24;
			} else if ((pi->offset & 3) == 1) {
				mask <<= 16;
				tmp <<= 16;
			} else if ((pi->offset & 3) == 3) {
				mask <<= 8;
				tmp <<= 8;
			}
		} else if (sz == 2) {
			if ((pi->offset & 3) == 0) {
				mask <<= 16;
				tmp <<= 16;
			}
		}
		pi->offset &= ~3;
		pi->mask = htonl(mask);
		pi->key = htonl(tmp) & pi->mask;
	}

done:
	*argc_p = argc;
	*argv_p = argv;
	return 0;
}


static int rsvp_parse_opt(struct filter_util *qu, char *handle, int argc, char **argv, struct nlmsghdr *n)
{
	int family = strcmp(qu->id, "rsvp") == 0 ? AF_INET : AF_INET6;
	struct tc_rsvp_pinfo pinfo;
	struct tc_police tp;
	struct tcmsg *t = NLMSG_DATA(n);
	int pinfo_ok = 0;
	struct rtattr *tail;

	memset(&pinfo, 0, sizeof(pinfo));
	memset(&tp, 0, sizeof(tp));

	if (handle) {
		if (get_u32(&t->tcm_handle, handle, 0)) {
			fprintf(stderr, "Illegal \"handle\"\n");
			return -1;
		}
	}

	if (argc == 0)
		return 0;

	tail = NLMSG_TAIL(n);
	addattr_l(n, 4096, TCA_OPTIONS, NULL, 0);

	while (argc > 0) {
		if (matches(*argv, "session") == 0) {
			inet_prefix addr;
			NEXT_ARG();
			if (get_addr_and_pi(&argc, &argv, &addr, &pinfo, 1, family)) {
				fprintf(stderr, "Illegal \"session\"\n");
				return -1;
			}
			addattr_l(n, 4096, TCA_RSVP_DST, &addr.data, addr.bytelen);
			if (pinfo.dpi.mask || pinfo.protocol)
				pinfo_ok++;
			continue;
		} else if (matches(*argv, "sender") == 0 ||
			   matches(*argv, "flowspec") == 0) {
			inet_prefix addr;
			NEXT_ARG();
			if (get_addr_and_pi(&argc, &argv, &addr, &pinfo, 0, family)) {
				fprintf(stderr, "Illegal \"sender\"\n");
				return -1;
			}
			addattr_l(n, 4096, TCA_RSVP_SRC, &addr.data, addr.bytelen);
			if (pinfo.spi.mask || pinfo.protocol)
				pinfo_ok++;
			continue;
		} else if (matches("ipproto", *argv) == 0) {
			int num;
			NEXT_ARG();
			num = inet_proto_a2n(*argv);
			if (num < 0) {
				fprintf(stderr, "Illegal \"ipproto\"\n");
				return -1;
			}
			pinfo.protocol = num;
			pinfo_ok++;
		} else if (matches(*argv, "classid") == 0 ||
			   strcmp(*argv, "flowid") == 0) {
			unsigned handle;
			NEXT_ARG();
			if (get_tc_classid(&handle, *argv)) {
				fprintf(stderr, "Illegal \"classid\"\n");
				return -1;
			}
			addattr_l(n, 4096, TCA_RSVP_CLASSID, &handle, 4);
		} else if (strcmp(*argv, "tunnelid") == 0) {
			unsigned tid;
			NEXT_ARG();
			if (get_unsigned(&tid, *argv, 0)) {
				fprintf(stderr, "Illegal \"tunnelid\"\n");
				return -1;
			}
			pinfo.tunnelid = tid;
			pinfo_ok++;
		} else if (strcmp(*argv, "tunnel") == 0) {
			unsigned tid;
			NEXT_ARG();
			if (get_unsigned(&tid, *argv, 0)) {
				fprintf(stderr, "Illegal \"tunnel\"\n");
				return -1;
			}
			addattr_l(n, 4096, TCA_RSVP_CLASSID, &tid, 4);
			NEXT_ARG();
			if (strcmp(*argv, "skip") == 0) {
				NEXT_ARG();
			}
			if (get_unsigned(&tid, *argv, 0)) {
				fprintf(stderr, "Illegal \"skip\"\n");
				return -1;
			}
			pinfo.tunnelhdr = tid;
			pinfo_ok++;
		} else if (matches(*argv, "police") == 0) {
			NEXT_ARG();
			if (parse_police(&argc, &argv, TCA_RSVP_POLICE, n)) {
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

	if (pinfo_ok)
		addattr_l(n, 4096, TCA_RSVP_PINFO, &pinfo, sizeof(pinfo));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}

static char * sprint_spi(struct tc_rsvp_gpi *pi, int dir, char *buf)
{
	if (pi->offset == 0) {
		if (dir && pi->mask == htonl(0xFFFF)) {
			snprintf(buf, SPRINT_BSIZE-1, "/%d", htonl(pi->key));
			return buf;
		}
		if (!dir && pi->mask == htonl(0xFFFF0000)) {
			snprintf(buf, SPRINT_BSIZE-1, "/%d", htonl(pi->key)>>16);
			return buf;
		}
		if (pi->mask == htonl(0xFFFFFFFF)) {
			snprintf(buf, SPRINT_BSIZE-1, " spi/esp 0x%08x", htonl(pi->key));
			return buf;
		}
	} else if (pi->offset == 4 && pi->mask == htonl(0xFFFFFFFF)) {
		snprintf(buf, SPRINT_BSIZE-1, " spi/ah 0x%08x", htonl(pi->key));
		return buf;
	} else if (pi->offset == -40 && pi->mask == htonl(0x000FFFFF)) {
		snprintf(buf, SPRINT_BSIZE-1, " flowlabel 0x%05x", htonl(pi->key));
		return buf;
	}
	snprintf(buf, SPRINT_BSIZE-1, " u32 0x%08x mask %08x at %d",
		 htonl(pi->key), htonl(pi->mask), pi->offset);
	return buf;
}

static int rsvp_print_opt(struct filter_util *qu, FILE *f, struct rtattr *opt, __u32 handle)
{
	int family = strcmp(qu->id, "rsvp") == 0 ? AF_INET : AF_INET6;
	struct rtattr *tb[TCA_RSVP_MAX+1];
	struct tc_rsvp_pinfo *pinfo = NULL;

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_RSVP_MAX, opt);

	if (handle)
		fprintf(f, "fh 0x%08x ", handle);

	if (tb[TCA_RSVP_PINFO]) {
		if (RTA_PAYLOAD(tb[TCA_RSVP_PINFO])  < sizeof(*pinfo))
			return -1;

		pinfo = RTA_DATA(tb[TCA_RSVP_PINFO]);
	}

	if (tb[TCA_RSVP_CLASSID]) {
		SPRINT_BUF(b1);
		if (!pinfo || pinfo->tunnelhdr == 0)
			fprintf(f, "flowid %s ", sprint_tc_classid(*(__u32*)RTA_DATA(tb[TCA_RSVP_CLASSID]), b1));
		else
			fprintf(f, "tunnel %d skip %d ", *(__u32*)RTA_DATA(tb[TCA_RSVP_CLASSID]), pinfo->tunnelhdr);
	} else if (pinfo && pinfo->tunnelhdr)
		fprintf(f, "tunnel [BAD] skip %d ", pinfo->tunnelhdr);

	if (tb[TCA_RSVP_DST]) {
		char buf[128];
		fprintf(f, "session ");
		if (inet_ntop(family, RTA_DATA(tb[TCA_RSVP_DST]), buf, sizeof(buf)) == 0)
			fprintf(f, " [INVALID DADDR] ");
		else
			fprintf(f, "%s", buf);
		if (pinfo && pinfo->dpi.mask) {
			SPRINT_BUF(b2);
			fprintf(f, "%s ", sprint_spi(&pinfo->dpi, 1, b2));
		} else
			fprintf(f, " ");
	} else {
		if (pinfo && pinfo->dpi.mask) {
			SPRINT_BUF(b2);
			fprintf(f, "session [NONE]%s ", sprint_spi(&pinfo->dpi, 1, b2));
		} else
			fprintf(f, "session NONE ");
	}

	if (pinfo && pinfo->protocol) {
		SPRINT_BUF(b1);
		fprintf(f, "ipproto %s ", inet_proto_n2a(pinfo->protocol, b1, sizeof(b1)));
	}
	if (pinfo && pinfo->tunnelid)
		fprintf(f, "tunnelid %d ", pinfo->tunnelid);
	if (tb[TCA_RSVP_SRC]) {
		char buf[128];
		fprintf(f, "sender ");
		if (inet_ntop(family, RTA_DATA(tb[TCA_RSVP_SRC]), buf, sizeof(buf)) == 0) {
			fprintf(f, "[BAD]");
		} else {
			fprintf(f, " %s", buf);
		}
		if (pinfo && pinfo->spi.mask) {
			SPRINT_BUF(b2);
			fprintf(f, "%s ", sprint_spi(&pinfo->spi, 0, b2));
		} else
			fprintf(f, " ");
	} else if (pinfo && pinfo->spi.mask) {
		SPRINT_BUF(b2);
		fprintf(f, "sender [NONE]%s ", sprint_spi(&pinfo->spi, 0, b2));
	}
	if (tb[TCA_RSVP_POLICE])
		tc_print_police(f, tb[TCA_RSVP_POLICE]);
	return 0;
}

struct filter_util rsvp_filter_util = {
	.id = "rsvp",
	.parse_fopt = rsvp_parse_opt,
	.print_fopt = rsvp_print_opt,
};

struct filter_util rsvp6_filter_util = {
	.id = "rsvp6",
	.parse_fopt = rsvp_parse_opt,
	.print_fopt = rsvp_print_opt,
};
