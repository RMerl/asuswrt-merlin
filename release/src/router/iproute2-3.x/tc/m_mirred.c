/*
 * m_egress.c		ingress/egress packet mirror/redir actions module
 *
 *		This program is free software; you can distribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:  J Hadi Salim (hadi@cyberus.ca)
 *
 * TODO: Add Ingress support
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
#include "utils.h"
#include "tc_util.h"
#include "tc_common.h"
#include <linux/tc_act/tc_mirred.h>

static void
explain(void)
{
	fprintf(stderr, "Usage: mirred <DIRECTION> <ACTION> [index INDEX] <dev DEVICENAME> \n");
	fprintf(stderr, "where: \n");
	fprintf(stderr, "\tDIRECTION := <ingress | egress>\n");
	fprintf(stderr, "\tACTION := <mirror | redirect>\n");
	fprintf(stderr, "\tINDEX  is the specific policy instance id\n");
	fprintf(stderr, "\tDEVICENAME is the devicename \n");

}

static void
usage(void)
{
	explain();
	exit(-1);
}

char *mirred_n2a(int action)
{
	switch (action) {
	case TCA_EGRESS_REDIR:
		return "Egress Redirect";
	case TCA_INGRESS_REDIR:
		return "Ingress Redirect";
	case TCA_EGRESS_MIRROR:
		return "Egress Mirror";
	case TCA_INGRESS_MIRROR:
		return "Ingress Mirror";
	default:
		return "unknown";
	}
}

int
parse_egress(struct action_util *a, int *argc_p, char ***argv_p, int tca_id, struct nlmsghdr *n)
{

	int argc = *argc_p;
	char **argv = *argv_p;
	int ok = 0, iok = 0, mirror=0,redir=0;
	struct tc_mirred p;
	struct rtattr *tail;
	char d[16];

	memset(d,0,sizeof(d)-1);
	memset(&p,0,sizeof(struct tc_mirred));

	while (argc > 0) {

		if (matches(*argv, "action") == 0) {
			break;
		} else if (matches(*argv, "egress") == 0) {
			NEXT_ARG();
			ok++;
			continue;
		} else {

			if (matches(*argv, "index") == 0) {
				NEXT_ARG();
				if (get_u32(&p.index, *argv, 10)) {
					fprintf(stderr, "Illegal \"index\"\n");
					return -1;
				}
				iok++;
				if (!ok) {
					argc--;
					argv++;
					break;
				}
			} else if(!ok) {
				fprintf(stderr, "was expecting egress (%s)\n", *argv);
				break;

			} else if (!mirror && matches(*argv, "mirror") == 0) {
				mirror=1;
				if (redir) {
					fprintf(stderr, "Cant have both mirror and redir\n");
					return -1;
				}
				p.eaction = TCA_EGRESS_MIRROR;
				p.action = TC_ACT_PIPE;
				ok++;
			} else if (!redir && matches(*argv, "redirect") == 0) {
				redir=1;
				if (mirror) {
					fprintf(stderr, "Cant have both mirror and redir\n");
					return -1;
				}
				p.eaction = TCA_EGRESS_REDIR;
				p.action = TC_ACT_STOLEN;
				ok++;
			} else if ((redir || mirror) && matches(*argv, "dev") == 0) {
				NEXT_ARG();
				if (strlen(d))
					duparg("dev", *argv);

				strncpy(d, *argv, sizeof(d)-1);
				argc--;
				argv++;

				break;

			}
		}

		NEXT_ARG();
	}

	if (!ok && !iok) {
		return -1;
	}



	if (d[0])  {
		int idx;
		ll_init_map(&rth);

		if ((idx = ll_name_to_index(d)) == 0) {
			fprintf(stderr, "Cannot find device \"%s\"\n", d);
			return -1;
		}

		p.ifindex = idx;
	}


	if (argc && p.eaction == TCA_EGRESS_MIRROR) {

		if (matches(*argv, "reclassify") == 0) {
			p.action = TC_POLICE_RECLASSIFY;
			NEXT_ARG();
		} else if (matches(*argv, "pipe") == 0) {
			p.action = TC_POLICE_PIPE;
			NEXT_ARG();
		} else if (matches(*argv, "drop") == 0 ||
			   matches(*argv, "shot") == 0) {
			p.action = TC_POLICE_SHOT;
			NEXT_ARG();
		} else if (matches(*argv, "continue") == 0) {
			p.action = TC_POLICE_UNSPEC;
			NEXT_ARG();
		} else if (matches(*argv, "pass") == 0) {
			p.action = TC_POLICE_OK;
			NEXT_ARG();
		}

	}

	if (argc) {
		if (iok && matches(*argv, "index") == 0) {
			fprintf(stderr, "mirred: Illegal double index\n");
			return -1;
		} else {
			if (matches(*argv, "index") == 0) {
				NEXT_ARG();
				if (get_u32(&p.index, *argv, 10)) {
					fprintf(stderr, "mirred: Illegal \"index\"\n");
					return -1;
				}
				argc--;
				argv++;
			}
		}
	}

	tail = NLMSG_TAIL(n);
	addattr_l(n, MAX_MSG, tca_id, NULL, 0);
	addattr_l(n, MAX_MSG, TCA_MIRRED_PARMS, &p, sizeof (p));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;

	*argc_p = argc;
	*argv_p = argv;
	return 0;
}


int
parse_mirred(struct action_util *a, int *argc_p, char ***argv_p, int tca_id, struct nlmsghdr *n)
{

	int argc = *argc_p;
	char **argv = *argv_p;

	if (argc < 0) {
		fprintf(stderr,"mirred bad arguement count %d\n", argc);
		return -1;
	}

	if (matches(*argv, "mirred") == 0) {
		NEXT_ARG();
	} else {
		fprintf(stderr,"mirred bad arguement %s\n", *argv);
		return -1;
	}


	if (matches(*argv, "egress") == 0 || matches(*argv, "index") == 0) {
		int ret = parse_egress(a, &argc, &argv, tca_id, n);
		if (ret == 0) {
			*argc_p = argc;
			*argv_p = argv;
			return 0;
		}

	} else if (matches(*argv, "ingress") == 0) {
		fprintf(stderr,"mirred ingress not supported at the moment\n");
	} else if (matches(*argv, "help") == 0) {
		usage();
	} else {
		fprintf(stderr,"mirred option not supported %s\n", *argv);
	}

	return -1;

}

int
print_mirred(struct action_util *au,FILE * f, struct rtattr *arg)
{
	struct tc_mirred *p;
	struct rtattr *tb[TCA_MIRRED_MAX + 1];
	const char *dev;
	SPRINT_BUF(b1);

	if (arg == NULL)
		return -1;

	parse_rtattr_nested(tb, TCA_MIRRED_MAX, arg);

	if (tb[TCA_MIRRED_PARMS] == NULL) {
		fprintf(f, "[NULL mirred parameters]");
		return -1;
	}
	p = RTA_DATA(tb[TCA_MIRRED_PARMS]);

	/*
	ll_init_map(&rth);
	*/


	if ((dev = ll_index_to_name(p->ifindex)) == 0) {
		fprintf(stderr, "Cannot find device %d\n", p->ifindex);
		return -1;
	}

	fprintf(f, "mirred (%s to device %s) %s", mirred_n2a(p->eaction), dev,action_n2a(p->action, b1, sizeof (b1)));

	fprintf(f, "\n ");
	fprintf(f, "\tindex %d ref %d bind %d",p->index,p->refcnt,p->bindcnt);

	if (show_stats) {
		if (tb[TCA_MIRRED_TM]) {
			struct tcf_t *tm = RTA_DATA(tb[TCA_MIRRED_TM]);
			print_tm(f,tm);
		}
	}
	fprintf(f, "\n ");
	return 0;
}

struct action_util mirred_action_util = {
	.id = "mirred",
	.parse_aopt = parse_mirred,
	.print_aopt = print_mirred,
};
