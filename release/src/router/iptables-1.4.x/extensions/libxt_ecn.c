/* Shared library add-on to iptables for ECN matching
 *
 * (C) 2002 by Harald Welte <laforge@netfilter.org>
 * (C) 2011 by Patrick McHardy <kaber@trash.net>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 * libipt_ecn.c borrowed heavily from libipt_dscp.c
 *
 */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_ecn.h>

enum {
	O_ECN_TCP_CWR = 0,
	O_ECN_TCP_ECE,
	O_ECN_IP_ECT,
};

static void ecn_help(void)
{
	printf(
"ECN match options\n"
"[!] --ecn-tcp-cwr 		Match CWR bit of TCP header\n"
"[!] --ecn-tcp-ece		Match ECE bit of TCP header\n"
"[!] --ecn-ip-ect [0..3]	Match ECN codepoint in IPv4/IPv6 header\n");
}

static const struct xt_option_entry ecn_opts[] = {
	{.name = "ecn-tcp-cwr", .id = O_ECN_TCP_CWR, .type = XTTYPE_NONE,
	 .flags = XTOPT_INVERT},
	{.name = "ecn-tcp-ece", .id = O_ECN_TCP_ECE, .type = XTTYPE_NONE,
	 .flags = XTOPT_INVERT},
	{.name = "ecn-ip-ect", .id = O_ECN_IP_ECT, .type = XTTYPE_UINT8,
	 .min = 0, .max = 3, .flags = XTOPT_INVERT},
	XTOPT_TABLEEND,
};

static void ecn_parse(struct xt_option_call *cb)
{
	struct xt_ecn_info *einfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_ECN_TCP_CWR:
		einfo->operation |= XT_ECN_OP_MATCH_CWR;
		if (cb->invert)
			einfo->invert |= XT_ECN_OP_MATCH_CWR;
		break;
	case O_ECN_TCP_ECE:
		einfo->operation |= XT_ECN_OP_MATCH_ECE;
		if (cb->invert)
			einfo->invert |= XT_ECN_OP_MATCH_ECE;
		break;
	case O_ECN_IP_ECT:
		if (cb->invert)
			einfo->invert |= XT_ECN_OP_MATCH_IP;
		einfo->operation |= XT_ECN_OP_MATCH_IP;
		einfo->ip_ect = cb->val.u8;
		break;
	}
}

static void ecn_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM,
		           "ECN match: some option required");
}

static void ecn_print(const void *ip, const struct xt_entry_match *match,
                      int numeric)
{
	const struct xt_ecn_info *einfo =
		(const struct xt_ecn_info *)match->data;

	printf(" ECN match");

	if (einfo->operation & XT_ECN_OP_MATCH_ECE) {
		printf(" %sECE",
		       (einfo->invert & XT_ECN_OP_MATCH_ECE) ? "!" : "");
	}

	if (einfo->operation & XT_ECN_OP_MATCH_CWR) {
		printf(" %sCWR",
		       (einfo->invert & XT_ECN_OP_MATCH_CWR) ? "!" : "");
	}

	if (einfo->operation & XT_ECN_OP_MATCH_IP) {
		printf(" %sECT=%d",
		       (einfo->invert & XT_ECN_OP_MATCH_IP) ? "!" : "",
		       einfo->ip_ect);
	}
}

static void ecn_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_ecn_info *einfo =
		(const struct xt_ecn_info *)match->data;

	if (einfo->operation & XT_ECN_OP_MATCH_ECE) {
		if (einfo->invert & XT_ECN_OP_MATCH_ECE)
			printf(" !");
		printf(" --ecn-tcp-ece");
	}

	if (einfo->operation & XT_ECN_OP_MATCH_CWR) {
		if (einfo->invert & XT_ECN_OP_MATCH_CWR)
			printf(" !");
		printf(" --ecn-tcp-cwr");
	}

	if (einfo->operation & XT_ECN_OP_MATCH_IP) {
		if (einfo->invert & XT_ECN_OP_MATCH_IP)
			printf(" !");
		printf(" --ecn-ip-ect %d", einfo->ip_ect);
	}
}

static struct xtables_match ecn_mt_reg = {
	.name          = "ecn",
	.version       = XTABLES_VERSION,
	.family        = NFPROTO_UNSPEC,
	.size          = XT_ALIGN(sizeof(struct xt_ecn_info)),
	.userspacesize = XT_ALIGN(sizeof(struct xt_ecn_info)),
	.help          = ecn_help,
	.print         = ecn_print,
	.save          = ecn_save,
	.x6_parse      = ecn_parse,
	.x6_fcheck     = ecn_check,
	.x6_options    = ecn_opts,
};

void _init(void)
{
	xtables_register_match(&ecn_mt_reg);
}
