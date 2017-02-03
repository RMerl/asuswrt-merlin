/* ebt_nflog
 *
 * Authors:
 * Peter Warasin <peter@endian.com>
 *
 *  February, 2008
 *
 * Based on:
 *  ebt_ulog.c, (C) 2004, Bart De Schuymer <bdschuym@pandora.be>
 *  libxt_NFLOG.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "../include/ebtables_u.h"
#include <linux/netfilter_bridge/ebt_nflog.h>

enum {
	NFLOG_GROUP = 0x1,
	NFLOG_PREFIX = 0x2,
	NFLOG_RANGE = 0x4,
	NFLOG_THRESHOLD = 0x8,
	NFLOG_NFLOG = 0x16,
};

static struct option nflog_opts[] = {
	{"nflog-group", required_argument, NULL, NFLOG_GROUP},
	{"nflog-prefix", required_argument, NULL, NFLOG_PREFIX},
	{"nflog-range", required_argument, NULL, NFLOG_RANGE},
	{"nflog-threshold", required_argument, NULL, NFLOG_THRESHOLD},
	{"nflog", no_argument, NULL, NFLOG_NFLOG},
	{.name = NULL}
};

static void nflog_help()
{
	printf("nflog options:\n"
	       "--nflog               : use the default nflog parameters\n"
	       "--nflog-prefix prefix : Prefix string for log message\n"
	       "--nflog-group group   : NETLINK group used for logging\n"
	       "--nflog-range range   : Number of byte to copy\n"
	       "--nflog-threshold     : Message threshold of"
	       "in-kernel queue\n");
}

static void init(struct ebt_entry_watcher *watcher)
{
	struct ebt_nflog_info *info = (struct ebt_nflog_info *)watcher->data;

	info->prefix[0] = '\0';
	info->group = EBT_NFLOG_DEFAULT_GROUP;
	info->threshold = EBT_NFLOG_DEFAULT_THRESHOLD;
}

static int nflog_parse(int c, char **argv, int argc,
		       const struct ebt_u_entry *entry, unsigned int *flags,
		       struct ebt_entry_watcher **watcher)
{
	struct ebt_nflog_info *info;
	unsigned int i;
	char *end;

	info = (struct ebt_nflog_info *)(*watcher)->data;
	switch (c) {
	case NFLOG_PREFIX:
		if (ebt_check_inverse2(optarg))
			goto inverse_invalid;
		ebt_check_option2(flags, NFLOG_PREFIX);
		if (strlen(optarg) > EBT_NFLOG_PREFIX_SIZE - 1)
			ebt_print_error("Prefix too long for nflog-prefix");
		strcpy(info->prefix, optarg);
		break;

	case NFLOG_GROUP:
		if (ebt_check_inverse2(optarg))
			goto inverse_invalid;
		ebt_check_option2(flags, NFLOG_GROUP);
		i = strtoul(optarg, &end, 10);
		if (*end != '\0')
			ebt_print_error2("--nflog-group must be a number!");
		info->group = i;
		break;

	case NFLOG_RANGE:
		if (ebt_check_inverse2(optarg))
			goto inverse_invalid;
		ebt_check_option2(flags, NFLOG_RANGE);
		i = strtoul(optarg, &end, 10);
		if (*end != '\0')
			ebt_print_error2("--nflog-range must be a number!");
		info->len = i;
		break;

	case NFLOG_THRESHOLD:
		if (ebt_check_inverse2(optarg))
			goto inverse_invalid;
		ebt_check_option2(flags, NFLOG_THRESHOLD);
		i = strtoul(optarg, &end, 10);
		if (*end != '\0')
			ebt_print_error2("--nflog-threshold must be a number!");
		info->threshold = i;
		break;
	case NFLOG_NFLOG:
		if (ebt_check_inverse(optarg))
			goto inverse_invalid;
		ebt_check_option2(flags, NFLOG_NFLOG);
		break;

	default:
		return 0;
	}
	return 1;

 inverse_invalid:
	ebt_print_error("The use of '!' makes no sense for the nflog watcher");
	return 1;
}

static void nflog_final_check(const struct ebt_u_entry *entry,
			      const struct ebt_entry_watcher *watcher,
			      const char *name, unsigned int hookmask,
			      unsigned int time)
{
}

static void nflog_print(const struct ebt_u_entry *entry,
			const struct ebt_entry_watcher *watcher)
{
	struct ebt_nflog_info *info = (struct ebt_nflog_info *)watcher->data;

	if (info->prefix[0] != '\0')
		printf("--nflog-prefix \"%s\"", info->prefix);
	if (info->group)
		printf("--nflog-group %d ", info->group);
	if (info->len)
		printf("--nflog-range %d", info->len);
	if (info->threshold != EBT_NFLOG_DEFAULT_THRESHOLD)
		printf(" --nflog-threshold %d ", info->threshold);
}

static int nflog_compare(const struct ebt_entry_watcher *w1,
			 const struct ebt_entry_watcher *w2)
{
	struct ebt_nflog_info *info1 = (struct ebt_nflog_info *)w1->data;
	struct ebt_nflog_info *info2 = (struct ebt_nflog_info *)w2->data;

	if (info1->group != info2->group ||
	    info1->len != info2->len ||
	    info1->threshold != info2->threshold ||
	    strcmp(info1->prefix, info2->prefix))
		return 0;
	return 1;
}

static struct ebt_u_watcher nflog_watcher = {
	.name = "nflog",
	.size = sizeof(struct ebt_nflog_info),
	.help = nflog_help,
	.init = init,
	.parse = nflog_parse,
	.final_check = nflog_final_check,
	.print = nflog_print,
	.compare = nflog_compare,
	.extra_ops = nflog_opts,
};

void _init(void)
{
	ebt_register_watcher(&nflog_watcher);
}
