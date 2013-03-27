/* Shared library add-on to iptables for the TTL target
 * (C) 2000 by Harald Welte <laforge@gnumonks.org>
 *
 * This program is distributed under the terms of GNU GPL
 */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter_ipv4/ipt_TTL.h>

enum {
	O_TTL_SET = 0,
	O_TTL_INC,
	O_TTL_DEC,
	F_TTL_SET = 1 << O_TTL_SET,
	F_TTL_INC = 1 << O_TTL_INC,
	F_TTL_DEC = 1 << O_TTL_DEC,
	F_ANY     = F_TTL_SET | F_TTL_INC | F_TTL_DEC,
};

#define s struct ipt_TTL_info
static const struct xt_option_entry TTL_opts[] = {
	{.name = "ttl-set", .type = XTTYPE_UINT8, .id = O_TTL_SET,
	 .excl = F_ANY, .flags = XTOPT_PUT, XTOPT_POINTER(s, ttl)},
	{.name = "ttl-dec", .type = XTTYPE_UINT8, .id = O_TTL_DEC,
	 .excl = F_ANY, .flags = XTOPT_PUT, XTOPT_POINTER(s, ttl),
	 .min = 1},
	{.name = "ttl-inc", .type = XTTYPE_UINT8, .id = O_TTL_INC,
	 .excl = F_ANY, .flags = XTOPT_PUT, XTOPT_POINTER(s, ttl),
	 .min = 1},
	XTOPT_TABLEEND,
};
#undef s

static void TTL_help(void)
{
	printf(
"TTL target options\n"
"  --ttl-set value		Set TTL to <value 0-255>\n"
"  --ttl-dec value		Decrement TTL by <value 1-255>\n"
"  --ttl-inc value		Increment TTL by <value 1-255>\n");
}

static void TTL_parse(struct xt_option_call *cb)
{
	struct ipt_TTL_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_TTL_SET:
		info->mode = IPT_TTL_SET;
		break;
	case O_TTL_DEC:
		info->mode = IPT_TTL_DEC;
		break;
	case O_TTL_INC:
		info->mode = IPT_TTL_INC;
		break;
	}
}

static void TTL_check(struct xt_fcheck_call *cb)
{
	if (!(cb->xflags & F_ANY))
		xtables_error(PARAMETER_PROBLEM,
				"TTL: You must specify an action");
}

static void TTL_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ipt_TTL_info *info = 
		(struct ipt_TTL_info *) target->data;

	switch (info->mode) {
		case IPT_TTL_SET:
			printf(" --ttl-set");
			break;
		case IPT_TTL_DEC:
			printf(" --ttl-dec");
			break;

		case IPT_TTL_INC:
			printf(" --ttl-inc");
			break;
	}
	printf(" %u", info->ttl);
}

static void TTL_print(const void *ip, const struct xt_entry_target *target,
                      int numeric)
{
	const struct ipt_TTL_info *info =
		(struct ipt_TTL_info *) target->data;

	printf(" TTL ");
	switch (info->mode) {
		case IPT_TTL_SET:
			printf("set to");
			break;
		case IPT_TTL_DEC:
			printf("decrement by");
			break;
		case IPT_TTL_INC:
			printf("increment by");
			break;
	}
	printf(" %u", info->ttl);
}

static struct xtables_target ttl_tg_reg = {
	.name		= "TTL",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(sizeof(struct ipt_TTL_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ipt_TTL_info)),
	.help		= TTL_help,
	.print		= TTL_print,
	.save		= TTL_save,
	.x6_parse	= TTL_parse,
	.x6_fcheck	= TTL_check,
	.x6_options	= TTL_opts,
};

void _init(void)
{
	xtables_register_target(&ttl_tg_reg);
}
