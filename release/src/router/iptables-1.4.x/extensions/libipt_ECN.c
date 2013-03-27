/* Shared library add-on to iptables for ECN, $Version$
 *
 * (C) 2002 by Harald Welte <laforge@gnumonks.org>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 * libipt_ECN.c borrowed heavily from libipt_DSCP.c
 */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter_ipv4/ipt_ECN.h>

enum {
	O_ECN_TCP_REMOVE = 0,
	O_ECN_TCP_CWR,
	O_ECN_TCP_ECE,
	O_ECN_IP_ECT,
	F_ECN_TCP_REMOVE = 1 << O_ECN_TCP_REMOVE,
	F_ECN_TCP_CWR    = 1 << O_ECN_TCP_CWR,
	F_ECN_TCP_ECE    = 1 << O_ECN_TCP_ECE,
};

static void ECN_help(void)
{
	printf(
"ECN target options\n"
"  --ecn-tcp-remove		Remove all ECN bits from TCP header\n");
}

#if 0
"ECN target v%s EXPERIMENTAL options (use with extreme care!)\n"
"  --ecn-ip-ect			Set the IPv4 ECT codepoint (0 to 3)\n"
"  --ecn-tcp-cwr		Set the IPv4 CWR bit (0 or 1)\n"
"  --ecn-tcp-ece		Set the IPv4 ECE bit (0 or 1)\n",
#endif

static const struct xt_option_entry ECN_opts[] = {
	{.name = "ecn-tcp-remove", .id = O_ECN_TCP_REMOVE, .type = XTTYPE_NONE,
	 .excl = F_ECN_TCP_CWR | F_ECN_TCP_ECE},
	{.name = "ecn-tcp-cwr", .id = O_ECN_TCP_CWR, .type = XTTYPE_UINT8,
	 .min = 0, .max = 1, .excl = F_ECN_TCP_REMOVE},
	{.name = "ecn-tcp-ece", .id = O_ECN_TCP_ECE, .type = XTTYPE_UINT8,
	 .min = 0, .max = 1, .excl = F_ECN_TCP_REMOVE},
	{.name = "ecn-ip-ect", .id = O_ECN_IP_ECT, .type = XTTYPE_UINT8,
	 .min = 0, .max = 3},
	XTOPT_TABLEEND,
};

static void ECN_parse(struct xt_option_call *cb)
{
	struct ipt_ECN_info *einfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_ECN_TCP_REMOVE:
		einfo->operation = IPT_ECN_OP_SET_ECE | IPT_ECN_OP_SET_CWR;
		einfo->proto.tcp.ece = 0;
		einfo->proto.tcp.cwr = 0;
		break;
	case O_ECN_TCP_CWR:
		einfo->operation |= IPT_ECN_OP_SET_CWR;
		einfo->proto.tcp.cwr = cb->val.u8;
		break;
	case O_ECN_TCP_ECE:
		einfo->operation |= IPT_ECN_OP_SET_ECE;
		einfo->proto.tcp.ece = cb->val.u8;
		break;
	case O_ECN_IP_ECT:
		einfo->operation |= IPT_ECN_OP_SET_IP;
		einfo->ip_ect = cb->val.u8;
		break;
	}
}

static void ECN_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM,
		           "ECN target: An operation is required");
}

static void ECN_print(const void *ip, const struct xt_entry_target *target,
                      int numeric)
{
	const struct ipt_ECN_info *einfo =
		(const struct ipt_ECN_info *)target->data;

	printf(" ECN");

	if (einfo->operation == (IPT_ECN_OP_SET_ECE|IPT_ECN_OP_SET_CWR)
	    && einfo->proto.tcp.ece == 0
	    && einfo->proto.tcp.cwr == 0)
		printf(" TCP remove");
	else {
		if (einfo->operation & IPT_ECN_OP_SET_ECE)
			printf(" ECE=%u", einfo->proto.tcp.ece);

		if (einfo->operation & IPT_ECN_OP_SET_CWR)
			printf(" CWR=%u", einfo->proto.tcp.cwr);

		if (einfo->operation & IPT_ECN_OP_SET_IP)
			printf(" ECT codepoint=%u", einfo->ip_ect);
	}
}

static void ECN_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ipt_ECN_info *einfo =
		(const struct ipt_ECN_info *)target->data;

	if (einfo->operation == (IPT_ECN_OP_SET_ECE|IPT_ECN_OP_SET_CWR)
	    && einfo->proto.tcp.ece == 0
	    && einfo->proto.tcp.cwr == 0)
		printf(" --ecn-tcp-remove");
	else {

		if (einfo->operation & IPT_ECN_OP_SET_ECE)
			printf(" --ecn-tcp-ece %d", einfo->proto.tcp.ece);

		if (einfo->operation & IPT_ECN_OP_SET_CWR)
			printf(" --ecn-tcp-cwr %d", einfo->proto.tcp.cwr);

		if (einfo->operation & IPT_ECN_OP_SET_IP)
			printf(" --ecn-ip-ect %d", einfo->ip_ect);
	}
}

static struct xtables_target ecn_tg_reg = {
	.name		= "ECN",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(sizeof(struct ipt_ECN_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ipt_ECN_info)),
	.help		= ECN_help,
	.print		= ECN_print,
	.save		= ECN_save,
	.x6_parse	= ECN_parse,
	.x6_fcheck	= ECN_check,
	.x6_options	= ECN_opts,
};

void _init(void)
{
	xtables_register_target(&ecn_tg_reg);
}
