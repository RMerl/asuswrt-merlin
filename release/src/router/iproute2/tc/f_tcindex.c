/*
 * f_tcindex.c		Traffic control index filter
 *
 * Written 1998,1999 by Werner Almesberger
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>

#include "utils.h"
#include "tc_util.h"

static void explain(void)
{
	fprintf(stderr," Usage: ... tcindex [ hash SIZE ] [ mask MASK ]"
	    " [ shift SHIFT ]\n");
	fprintf(stderr,"                    [ pass_on | fall_through ]\n");
	fprintf(stderr,"                    [ classid CLASSID ] "
	    "[ police POLICE_SPEC ]\n");
}


#define usage() return(-1)


static int tcindex_parse_opt(struct filter_util *qu, char *handle, int argc,
    char **argv, struct nlmsghdr *n)
{
	struct tcmsg *t = NLMSG_DATA(n);
	struct rtattr *tail;
	char *end;

	if (handle) {
		t->tcm_handle = strtoul(handle,&end,0);
		if (*end) {
			fprintf(stderr, "Illegal filter ID\n");
			return -1;
		}
	}
	if (!argc) return 0;
	tail = NLMSG_TAIL(n);
	addattr_l(n,4096,TCA_OPTIONS,NULL,0);
	while (argc) {
		if (!strcmp(*argv,"hash")) {
			int hash;

			NEXT_ARG();
			hash = strtoul(*argv,&end,0);
			if (*end || !hash || hash > 0x10000) {
				explain();
				return -1;
			}
			addattr_l(n,4096,TCA_TCINDEX_HASH,&hash,sizeof(hash));
		}
		else if (!strcmp(*argv,"mask")) {
			__u16 mask;

			NEXT_ARG();
			mask = strtoul(*argv,&end,0);
			if (*end) {
				explain();
				return -1;
			}
			addattr_l(n,4096,TCA_TCINDEX_MASK,&mask,sizeof(mask));
		}
		else if (!strcmp(*argv,"shift")) {
			int shift;

			NEXT_ARG();
			shift = strtoul(*argv,&end,0);
			if (*end) {
				explain();
				return -1;
			}
			addattr_l(n,4096,TCA_TCINDEX_SHIFT,&shift,
			    sizeof(shift));
		}
		else if (!strcmp(*argv,"fall_through")) {
			int value = 1;

			addattr_l(n,4096,TCA_TCINDEX_FALL_THROUGH,&value,
			    sizeof(value));
		}
		else if (!strcmp(*argv,"pass_on")) {
			int value = 0;

			addattr_l(n,4096,TCA_TCINDEX_FALL_THROUGH,&value,
			    sizeof(value));
		}
		else if (!strcmp(*argv,"classid")) {
			__u32 handle;

			NEXT_ARG();
			if (get_tc_classid(&handle,*argv)) {
				fprintf(stderr, "Illegal \"classid\"\n");
				return -1;
			}
			addattr_l(n, 4096, TCA_TCINDEX_CLASSID, &handle, 4);
		}
		else if (!strcmp(*argv,"police")) {
			NEXT_ARG();
			if (parse_police(&argc, &argv, TCA_TCINDEX_POLICE, n)) {
				fprintf(stderr, "Illegal \"police\"\n");
				return -1;
			}
			continue;
		}
		else {
			explain();
			return -1;
		}
		argc--;
		argv++;
	}
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;
	return 0;
}


static int tcindex_print_opt(struct filter_util *qu, FILE *f,
     struct rtattr *opt, __u32 handle)
{
	struct rtattr *tb[TCA_TCINDEX_MAX+1];

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_TCINDEX_MAX, opt);

	if (handle != ~0) fprintf(f,"handle 0x%04x ",handle);
	if (tb[TCA_TCINDEX_HASH]) {
		__u16 hash;

		if (RTA_PAYLOAD(tb[TCA_TCINDEX_HASH]) < sizeof(hash))
			return -1;
		hash = *(__u16 *) RTA_DATA(tb[TCA_TCINDEX_HASH]);
		fprintf(f,"hash %d ",hash);
	}
	if (tb[TCA_TCINDEX_MASK]) {
		__u16 mask;

		if (RTA_PAYLOAD(tb[TCA_TCINDEX_MASK]) < sizeof(mask))
			return -1;
		mask = *(__u16 *) RTA_DATA(tb[TCA_TCINDEX_MASK]);
		fprintf(f,"mask 0x%04x ",mask);
	}
	if (tb[TCA_TCINDEX_SHIFT]) {
		int shift;

		if (RTA_PAYLOAD(tb[TCA_TCINDEX_SHIFT]) < sizeof(shift))
			return -1;
		shift = *(int *) RTA_DATA(tb[TCA_TCINDEX_SHIFT]);
		fprintf(f,"shift %d ",shift);
	}
	if (tb[TCA_TCINDEX_FALL_THROUGH]) {
		int fall_through;

		if (RTA_PAYLOAD(tb[TCA_TCINDEX_FALL_THROUGH]) <
		    sizeof(fall_through))
			return -1;
		fall_through = *(int *) RTA_DATA(tb[TCA_TCINDEX_FALL_THROUGH]);
		fprintf(f,fall_through ? "fall_through " : "pass_on ");
	}
	if (tb[TCA_TCINDEX_CLASSID]) {
		SPRINT_BUF(b1);
		fprintf(f, "classid %s ",sprint_tc_classid(*(__u32 *)
		    RTA_DATA(tb[TCA_TCINDEX_CLASSID]), b1));
	}
	if (tb[TCA_TCINDEX_POLICE]) {
		fprintf(f, "\n");
		tc_print_police(f, tb[TCA_TCINDEX_POLICE]);
	}
	return 0;
}

struct filter_util tcindex_filter_util = {
	.id = "tcindex",
	.parse_fopt = tcindex_parse_opt,
	.print_fopt = tcindex_print_opt,
};
