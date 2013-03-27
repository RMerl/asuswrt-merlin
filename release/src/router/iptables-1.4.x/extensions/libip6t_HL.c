/*
 * IPv6 Hop Limit Target module
 * Maciej Soltysiak <solt@dns.toxicfilms.tv>
 * Based on HW's ttl target
 * This program is distributed under the terms of GNU GPL
 */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter_ipv6/ip6t_HL.h>

enum {
	O_HL_SET = 0,
	O_HL_INC,
	O_HL_DEC,
	F_HL_SET = 1 << O_HL_SET,
	F_HL_INC = 1 << O_HL_INC,
	F_HL_DEC = 1 << O_HL_DEC,
	F_ANY    = F_HL_SET | F_HL_INC | F_HL_DEC,
};

#define s struct ip6t_HL_info
static const struct xt_option_entry HL_opts[] = {
	{.name = "hl-set", .type = XTTYPE_UINT8, .id = O_HL_SET,
	 .excl = F_ANY, .flags = XTOPT_PUT, XTOPT_POINTER(s, hop_limit)},
	{.name = "hl-dec", .type = XTTYPE_UINT8, .id = O_HL_DEC,
	 .excl = F_ANY, .flags = XTOPT_PUT, XTOPT_POINTER(s, hop_limit),
	 .min = 1},
	{.name = "hl-inc", .type = XTTYPE_UINT8, .id = O_HL_INC,
	 .excl = F_ANY, .flags = XTOPT_PUT, XTOPT_POINTER(s, hop_limit),
	 .min = 1},
	XTOPT_TABLEEND,
};
#undef s

static void HL_help(void)
{
	printf(
"HL target options\n"
"  --hl-set value		Set HL to <value 0-255>\n"
"  --hl-dec value		Decrement HL by <value 1-255>\n"
"  --hl-inc value		Increment HL by <value 1-255>\n");
}

static void HL_parse(struct xt_option_call *cb)
{
	struct ip6t_HL_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_HL_SET:
		info->mode = IP6T_HL_SET;
		break;
	case O_HL_INC:
		info->mode = IP6T_HL_INC;
		break;
	case O_HL_DEC:
		info->mode = IP6T_HL_DEC;
		break;
	}
}

static void HL_check(struct xt_fcheck_call *cb)
{
	if (!(cb->xflags & F_ANY))
		xtables_error(PARAMETER_PROBLEM,
				"HL: You must specify an action");
}

static void HL_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ip6t_HL_info *info = 
		(struct ip6t_HL_info *) target->data;

	switch (info->mode) {
		case IP6T_HL_SET:
			printf(" --hl-set");
			break;
		case IP6T_HL_DEC:
			printf(" --hl-dec");
			break;

		case IP6T_HL_INC:
			printf(" --hl-inc");
			break;
	}
	printf(" %u", info->hop_limit);
}

static void HL_print(const void *ip, const struct xt_entry_target *target,
                     int numeric)
{
	const struct ip6t_HL_info *info =
		(struct ip6t_HL_info *) target->data;

	printf(" HL ");
	switch (info->mode) {
		case IP6T_HL_SET:
			printf("set to");
			break;
		case IP6T_HL_DEC:
			printf("decrement by");
			break;
		case IP6T_HL_INC:
			printf("increment by");
			break;
	}
	printf(" %u", info->hop_limit);
}

static struct xtables_target hl_tg6_reg = {
	.name 		= "HL",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV6,
	.size		= XT_ALIGN(sizeof(struct ip6t_HL_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ip6t_HL_info)),
	.help		= HL_help,
	.print		= HL_print,
	.save		= HL_save,
	.x6_parse	= HL_parse,
	.x6_fcheck	= HL_check,
	.x6_options	= HL_opts,
};

void _init(void)
{
	xtables_register_target(&hl_tg6_reg);
}
