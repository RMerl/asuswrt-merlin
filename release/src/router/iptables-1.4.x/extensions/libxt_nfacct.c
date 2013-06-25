/*
 * (C) 2011 by Pablo Neira Ayuso <pablo@netfilter.org>
 * (C) 2011 by Intra2Net AG <http://www.intra2net.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 (or
 * any later at your option) as published by the Free Software Foundation.
 */
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <xtables.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_nfacct.h>

enum {
	O_NAME = 0,
};

#define s struct xt_nfacct_match_info
static const struct xt_option_entry nfacct_opts[] = {
	{.name = "nfacct-name", .id = O_NAME, .type = XTTYPE_STRING,
	 .min = 1, .flags = XTOPT_MAND|XTOPT_PUT, XTOPT_POINTER(s, name)},
	XTOPT_TABLEEND,
};
#undef s

static void nfacct_help(void)
{
	printf("nfacct match options:\n"
	       " --nfacct-name STRING		Name of accouting area\n");
}

static void nfacct_parse(struct xt_option_call *cb)
{
	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_NAME:
		if (strchr(cb->arg, '\n') != NULL)
			xtables_error(PARAMETER_PROBLEM,
				   "Newlines not allowed in --nfacct-name");
		break;
	}
}

static void
nfacct_print_name(const struct xt_nfacct_match_info *info, char *name)
{
	printf(" %snfacct-name ", name);
	xtables_save_string(info->name);
}

static void nfacct_print(const void *ip, const struct xt_entry_match *match,
                        int numeric)
{
	const struct xt_nfacct_match_info *info =
		(struct xt_nfacct_match_info *)match->data;

	nfacct_print_name(info, "");
}

static void nfacct_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_nfacct_match_info *info =
		(struct xt_nfacct_match_info *)match->data;

	nfacct_print_name(info, "--");
}

static struct xtables_match nfacct_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "nfacct",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_nfacct_match_info)),
	.userspacesize	= offsetof(struct xt_nfacct_match_info, nfacct),
	.help		= nfacct_help,
	.x6_parse	= nfacct_parse,
	.print		= nfacct_print,
	.save		= nfacct_save,
	.x6_options	= nfacct_opts,
};

void _init(void)
{
	xtables_register_match(&nfacct_match);
}
