/* ebt_ulog
 *
 * Authors:
 * Bart De Schuymer <bdschuym@pandora.be>
 *
 * November, 2004
 */

#define __need_time_t
#define __need_suseconds_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "../include/ebtables_u.h"
#include <sys/time.h>
#include <linux/netfilter_bridge/ebt_ulog.h>

#define CP_NO_LIMIT_S "default_cprange"
#define CP_NO_LIMIT_N 0

#define ULOG_PREFIX     '1'
#define ULOG_NLGROUP    '2'
#define ULOG_CPRANGE    '3'
#define ULOG_QTHRESHOLD '4'
#define ULOG_ULOG       '5'
static struct option opts[] =
{
	{ "ulog-prefix"    , required_argument, 0, ULOG_PREFIX     },
	{ "ulog-nlgroup"   , required_argument, 0, ULOG_NLGROUP    },
	{ "ulog-cprange"   , required_argument, 0, ULOG_CPRANGE    },
	{ "ulog-qthreshold", required_argument, 0, ULOG_QTHRESHOLD },
	{ "ulog"           , no_argument      , 0, ULOG_ULOG       },
	{ 0 }
};

static void print_help()
{
	printf(
"ulog options:\n"
"--ulog                : use the default ulog parameters\n"
"--ulog-prefix prefix  : max %d characters (default is no prefix)\n"
"--ulog-nlgroup group  : 0 < group number < %d (default = %d)\n"
"--ulog-cprange range  : max copy range (default is " CP_NO_LIMIT_S ")\n"
"--ulog-qthreshold     : 0 < queueing threshold < %d (default = %d)\n",
	EBT_ULOG_PREFIX_LEN - 1, EBT_ULOG_MAXNLGROUPS + 1,
	EBT_ULOG_DEFAULT_NLGROUP + 1, EBT_ULOG_MAX_QLEN + 1,
	EBT_ULOG_DEFAULT_QTHRESHOLD);
}

static void init(struct ebt_entry_watcher *watcher)
{
	struct ebt_ulog_info *uloginfo = (struct ebt_ulog_info *)watcher->data;

	uloginfo->prefix[0] = '\0';
	uloginfo->nlgroup = EBT_ULOG_DEFAULT_NLGROUP;
	uloginfo->cprange = CP_NO_LIMIT_N; /* Use default netlink buffer size */
	uloginfo->qthreshold = EBT_ULOG_DEFAULT_QTHRESHOLD;
}

#define OPT_PREFIX     0x01
#define OPT_NLGROUP    0x02
#define OPT_CPRANGE    0x04
#define OPT_QTHRESHOLD 0x08
#define OPT_ULOG       0x10
static int parse(int c, char **argv, int argc, const struct ebt_u_entry *entry,
   unsigned int *flags, struct ebt_entry_watcher **watcher)
{
	struct ebt_ulog_info *uloginfo;
	unsigned int i;
	char *end;

	uloginfo = (struct ebt_ulog_info *)(*watcher)->data;
	switch (c) {
	case ULOG_PREFIX:
		if (ebt_check_inverse2(optarg))
			goto inverse_invalid;
		ebt_check_option2(flags, OPT_PREFIX);
		if (strlen(optarg) > EBT_ULOG_PREFIX_LEN - 1)
			ebt_print_error("Prefix too long for ulog-prefix");
		strcpy(uloginfo->prefix, optarg);
		break;

	case ULOG_NLGROUP:
		if (ebt_check_inverse2(optarg))
			goto inverse_invalid;
		ebt_check_option2(flags, OPT_NLGROUP);
		i = strtoul(optarg, &end, 10);
		if (*end != '\0')
			ebt_print_error2("Problem with ulog-nlgroup: %s", optarg);
		if (i < 1 || i > EBT_ULOG_MAXNLGROUPS)
			ebt_print_error2("the ulog-nlgroup number must be between 1 and 32");
		uloginfo->nlgroup = i - 1;
		break;

	case ULOG_CPRANGE:
		if (ebt_check_inverse2(optarg))
			goto inverse_invalid;
		ebt_check_option2(flags, OPT_CPRANGE);
		i = strtoul(optarg, &end, 10);
		if (*end != '\0') {
			if (strcasecmp(optarg, CP_NO_LIMIT_S))
				ebt_print_error2("Problem with ulog-cprange: %s", optarg);
			i = CP_NO_LIMIT_N;
		}
		uloginfo->cprange = i;
		break;

	case ULOG_QTHRESHOLD:
		if (ebt_check_inverse2(optarg))
			goto inverse_invalid;
		ebt_check_option2(flags, OPT_QTHRESHOLD);
		i = strtoul(optarg, &end, 10);
		if (*end != '\0')
			ebt_print_error2("Problem with ulog-qthreshold: %s", optarg);
		if (i > EBT_ULOG_MAX_QLEN)
			ebt_print_error2("ulog-qthreshold argument %d exceeds the maximum of %d", i, EBT_ULOG_MAX_QLEN);
		uloginfo->qthreshold = i;
		break;
	case ULOG_ULOG:
		if (ebt_check_inverse(optarg))
			goto inverse_invalid;
		ebt_check_option2(flags, OPT_ULOG);
		break;

	default:
		return 0;
	}
	return 1;

inverse_invalid:
	ebt_print_error("The use of '!' makes no sense for the ulog watcher");
	return 1;
}

static void final_check(const struct ebt_u_entry *entry,
   const struct ebt_entry_watcher *watcher, const char *name,
   unsigned int hookmask, unsigned int time)
{
}

static void print(const struct ebt_u_entry *entry,
   const struct ebt_entry_watcher *watcher)
{
	struct ebt_ulog_info *uloginfo = (struct ebt_ulog_info *)watcher->data;

	printf("--ulog-prefix \"%s\" --ulog-nlgroup %d --ulog-cprange ",
	       uloginfo->prefix, uloginfo->nlgroup + 1);
	if (uloginfo->cprange == CP_NO_LIMIT_N)
		printf(CP_NO_LIMIT_S);
	else
		printf("%d", uloginfo->cprange);
	printf(" --ulog-qthreshold %d ", uloginfo->qthreshold);
}

static int compare(const struct ebt_entry_watcher *w1,
   const struct ebt_entry_watcher *w2)
{
	struct ebt_ulog_info *uloginfo1 = (struct ebt_ulog_info *)w1->data;
	struct ebt_ulog_info *uloginfo2 = (struct ebt_ulog_info *)w2->data;

	if (uloginfo1->nlgroup != uloginfo2->nlgroup ||
	    uloginfo1->cprange != uloginfo2->cprange ||
	    uloginfo1->qthreshold != uloginfo2->qthreshold ||
	    strcmp(uloginfo1->prefix, uloginfo2->prefix))
		return 0;
	return 1;
}

static struct ebt_u_watcher ulog_watcher =
{
	.name		= "ulog",
	.size		= sizeof(struct ebt_ulog_info),
	.help		= print_help,
	.init		= init,
	.parse		= parse,
	.final_check	= final_check,
	.print		= print,
	.compare	= compare,
	.extra_ops	= opts,
};

void _init(void)
{
	ebt_register_watcher(&ulog_watcher);
}
