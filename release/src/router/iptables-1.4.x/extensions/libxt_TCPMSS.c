/* Shared library add-on to iptables to add TCPMSS target support.
 *
 * Copyright (c) 2000 Marc Boucher
*/
#include "config.h"
#include <stdio.h>
#include <xtables.h>
#include <netinet/ip.h>
#include <linux/netfilter/xt_TCPMSS.h>

enum {
	O_SET_MSS = 0,
	O_CLAMP_MSS,
};

struct mssinfo {
	struct xt_entry_target t;
	struct xt_tcpmss_info mss;
};

static void __TCPMSS_help(int hdrsize)
{
	printf(
"TCPMSS target mutually-exclusive options:\n"
"  --set-mss value               explicitly set MSS option to specified value\n"
"  --clamp-mss-to-pmtu           automatically clamp MSS value to (path_MTU - %d)\n",
hdrsize);
}

static void TCPMSS_help(void)
{
	__TCPMSS_help(sizeof(struct iphdr));
}

static void TCPMSS_help6(void)
{
	__TCPMSS_help(SIZEOF_STRUCT_IP6_HDR);
}

static const struct xt_option_entry TCPMSS4_opts[] = {
	{.name = "set-mss", .id = O_SET_MSS, .type = XTTYPE_UINT16,
	 .min = 0, .max = UINT16_MAX - sizeof(struct iphdr),
	 .flags = XTOPT_PUT, XTOPT_POINTER(struct xt_tcpmss_info, mss)},
	{.name = "clamp-mss-to-pmtu", .id = O_CLAMP_MSS, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};

static const struct xt_option_entry TCPMSS6_opts[] = {
	{.name = "set-mss", .id = O_SET_MSS, .type = XTTYPE_UINT16,
	 .min = 0, .max = UINT16_MAX - SIZEOF_STRUCT_IP6_HDR,
	 .flags = XTOPT_PUT, XTOPT_POINTER(struct xt_tcpmss_info, mss)},
	{.name = "clamp-mss-to-pmtu", .id = O_CLAMP_MSS, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};

static void TCPMSS_parse(struct xt_option_call *cb)
{
	struct xt_tcpmss_info *mssinfo = cb->data;

	xtables_option_parse(cb);
	if (cb->entry->id == O_CLAMP_MSS)
		mssinfo->mss = XT_TCPMSS_CLAMP_PMTU;
}

static void TCPMSS_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM,
		           "TCPMSS target: At least one parameter is required");
}

static void TCPMSS_print(const void *ip, const struct xt_entry_target *target,
                         int numeric)
{
	const struct xt_tcpmss_info *mssinfo =
		(const struct xt_tcpmss_info *)target->data;
	if(mssinfo->mss == XT_TCPMSS_CLAMP_PMTU)
		printf(" TCPMSS clamp to PMTU");
	else
		printf(" TCPMSS set %u", mssinfo->mss);
}

static void TCPMSS_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_tcpmss_info *mssinfo =
		(const struct xt_tcpmss_info *)target->data;

	if(mssinfo->mss == XT_TCPMSS_CLAMP_PMTU)
		printf(" --clamp-mss-to-pmtu");
	else
		printf(" --set-mss %u", mssinfo->mss);
}

static struct xtables_target tcpmss_tg_reg[] = {
	{
		.family        = NFPROTO_IPV4,
		.name          = "TCPMSS",
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_tcpmss_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_tcpmss_info)),
		.help          = TCPMSS_help,
		.print         = TCPMSS_print,
		.save          = TCPMSS_save,
		.x6_parse      = TCPMSS_parse,
		.x6_fcheck     = TCPMSS_check,
		.x6_options    = TCPMSS4_opts,
	},
	{
		.family        = NFPROTO_IPV6,
		.name          = "TCPMSS",
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_tcpmss_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_tcpmss_info)),
		.help          = TCPMSS_help6,
		.print         = TCPMSS_print,
		.save          = TCPMSS_save,
		.x6_parse      = TCPMSS_parse,
		.x6_fcheck     = TCPMSS_check,
		.x6_options    = TCPMSS6_opts,
	},
};

void _init(void)
{
	xtables_register_targets(tcpmss_tg_reg, ARRAY_SIZE(tcpmss_tg_reg));
}
