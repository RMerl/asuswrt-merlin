/* Shared library add-on to iptables for DSCP
 *
 * (C) 2000- 2002 by Matthew G. Marsh <mgm@paktronix.com>,
 * 		     Harald Welte <laforge@gnumonks.org>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 * libipt_DSCP.c borrowed heavily from libipt_TOS.c
 *
 * --set-class added by Iain Barnes
 */
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter/xt_DSCP.h>

/* This is evil, but it's my code - HW*/
#include "dscp_helper.c"

enum {
	O_SET_DSCP = 0,
	O_SET_DSCP_CLASS,
	F_SET_DSCP       = 1 << O_SET_DSCP,
	F_SET_DSCP_CLASS = 1 << O_SET_DSCP_CLASS,
};

static void DSCP_help(void)
{
	printf(
"DSCP target options\n"
"  --set-dscp value		Set DSCP field in packet header to value\n"
"  		                This value can be in decimal (ex: 32)\n"
"               		or in hex (ex: 0x20)\n"
"  --set-dscp-class class	Set the DSCP field in packet header to the\n"
"				value represented by the DiffServ class value.\n"
"				This class may be EF,BE or any of the CSxx\n"
"				or AFxx classes.\n"
"\n"
"				These two options are mutually exclusive !\n"
);
}

static const struct xt_option_entry DSCP_opts[] = {
	{.name = "set-dscp", .id = O_SET_DSCP, .excl = F_SET_DSCP_CLASS,
	 .type = XTTYPE_UINT8, .min = 0, .max = XT_DSCP_MAX,
	 .flags = XTOPT_PUT,
	 XTOPT_POINTER(struct xt_DSCP_info, dscp)},
	{.name = "set-dscp-class", .id = O_SET_DSCP_CLASS, .excl = F_SET_DSCP,
	 .type = XTTYPE_STRING},
	XTOPT_TABLEEND,
};

static void DSCP_parse(struct xt_option_call *cb)
{
	struct xt_DSCP_info *dinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SET_DSCP_CLASS:
		dinfo->dscp = class_to_dscp(cb->arg);
		break;
	}
}

static void DSCP_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM,
		           "DSCP target: Parameter --set-dscp is required");
}

static void
print_dscp(uint8_t dscp, int numeric)
{
	printf(" 0x%02x", dscp);
}

static void DSCP_print(const void *ip, const struct xt_entry_target *target,
                       int numeric)
{
	const struct xt_DSCP_info *dinfo =
		(const struct xt_DSCP_info *)target->data;
	printf(" DSCP set");
	print_dscp(dinfo->dscp, numeric);
}

static void DSCP_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_DSCP_info *dinfo =
		(const struct xt_DSCP_info *)target->data;

	printf(" --set-dscp 0x%02x", dinfo->dscp);
}

static struct xtables_target dscp_target = {
	.family		= NFPROTO_UNSPEC,
	.name		= "DSCP",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_DSCP_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_DSCP_info)),
	.help		= DSCP_help,
	.print		= DSCP_print,
	.save		= DSCP_save,
	.x6_parse	= DSCP_parse,
	.x6_fcheck	= DSCP_check,
	.x6_options	= DSCP_opts,
};

void _init(void)
{
	xtables_register_target(&dscp_target);
}
