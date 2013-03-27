/*
 * Shared library add-on to iptables to add TOS target support
 *
 * Copyright © CC Computer Consultants GmbH, 2007
 * Contact: Jan Engelhardt <jengelh@medozas.de>
 */
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include <xtables.h>
#include <linux/netfilter/xt_DSCP.h>
#include "tos_values.c"

struct ipt_tos_target_info {
	uint8_t tos;
};

enum {
	O_SET_TOS = 0,
	O_AND_TOS,
	O_OR_TOS,
	O_XOR_TOS,
	F_SET_TOS = 1 << O_SET_TOS,
	F_AND_TOS = 1 << O_AND_TOS,
	F_OR_TOS  = 1 << O_OR_TOS,
	F_XOR_TOS = 1 << O_XOR_TOS,
	F_ANY     = F_SET_TOS | F_AND_TOS | F_OR_TOS | F_XOR_TOS,
};

static const struct xt_option_entry tos_tg_opts_v0[] = {
	{.name = "set-tos", .id = O_SET_TOS, .type = XTTYPE_TOSMASK,
	 .excl = F_ANY, .max = 0xFF},
	XTOPT_TABLEEND,
};

static const struct xt_option_entry tos_tg_opts[] = {
	{.name = "set-tos", .id = O_SET_TOS, .type = XTTYPE_TOSMASK,
	 .excl = F_ANY, .max = 0x3F},
	{.name = "and-tos", .id = O_AND_TOS, .type = XTTYPE_UINT8,
	 .excl = F_ANY},
	{.name = "or-tos", .id = O_OR_TOS, .type = XTTYPE_UINT8,
	 .excl = F_ANY},
	{.name = "xor-tos", .id = O_XOR_TOS, .type = XTTYPE_UINT8,
	 .excl = F_ANY},
	XTOPT_TABLEEND,
};

static void tos_tg_help_v0(void)
{
	const struct tos_symbol_info *symbol;

	printf(
"TOS target options:\n"
"  --set-tos value     Set Type of Service/Priority field to value\n"
"  --set-tos symbol    Set TOS field (IPv4 only) by symbol\n"
"                      Accepted symbolic names for value are:\n");

	for (symbol = tos_symbol_names; symbol->name != NULL; ++symbol)
		printf("                        (0x%02x) %2u %s\n",
		       symbol->value, symbol->value, symbol->name);

	printf("\n");
}

static void tos_tg_help(void)
{
	const struct tos_symbol_info *symbol;

	printf(
"TOS target v%s options:\n"
"  --set-tos value[/mask]  Set Type of Service/Priority field to value\n"
"                          (Zero out bits in mask and XOR value into TOS)\n"
"  --set-tos symbol        Set TOS field (IPv4 only) by symbol\n"
"                          (this zeroes the 4-bit Precedence part!)\n"
"                          Accepted symbolic names for value are:\n",
XTABLES_VERSION);

	for (symbol = tos_symbol_names; symbol->name != NULL; ++symbol)
		printf("                            (0x%02x) %2u %s\n",
		       symbol->value, symbol->value, symbol->name);

	printf(
"\n"
"  --and-tos bits          Binary AND the TOS value with bits\n"
"  --or-tos  bits          Binary OR the TOS value with bits\n"
"  --xor-tos bits          Binary XOR the TOS value with bits\n"
);
}

static void tos_tg_parse_v0(struct xt_option_call *cb)
{
	struct ipt_tos_target_info *info = cb->data;

	xtables_option_parse(cb);
	if (cb->val.tos_mask != 0xFF)
		xtables_error(PARAMETER_PROBLEM, "tos match: Your kernel "
		           "is too old to support anything besides "
			   "/0xFF as a mask.");
	info->tos = cb->val.tos_value;
}

static void tos_tg_parse(struct xt_option_call *cb)
{
	struct xt_tos_target_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SET_TOS:
		info->tos_value = cb->val.tos_value;
		info->tos_mask  = cb->val.tos_mask;
		break;
	case O_AND_TOS:
		info->tos_value = 0;
		info->tos_mask  = ~cb->val.u8;
		break;
	case O_OR_TOS:
		info->tos_value = cb->val.u8;
		info->tos_mask  = cb->val.u8;
		break;
	case O_XOR_TOS:
		info->tos_value = cb->val.u8;
		info->tos_mask  = 0;
		break;
	}
}

static void tos_tg_check(struct xt_fcheck_call *cb)
{
	if (!(cb->xflags & F_ANY))
		xtables_error(PARAMETER_PROBLEM,
		           "TOS: An action is required");
}

static void tos_tg_print_v0(const void *ip,
                            const struct xt_entry_target *target, int numeric)
{
	const struct ipt_tos_target_info *info = (const void *)target->data;

	printf(" TOS set ");
	if (numeric || !tos_try_print_symbolic("", info->tos, 0xFF))
		printf("0x%02x", info->tos);
}

static void tos_tg_print(const void *ip, const struct xt_entry_target *target,
                         int numeric)
{
	const struct xt_tos_target_info *info = (const void *)target->data;

	if (numeric)
		printf(" TOS set 0x%02x/0x%02x",
		       info->tos_value, info->tos_mask);
	else if (tos_try_print_symbolic(" TOS set",
	    info->tos_value, info->tos_mask))
		/* already printed by call */
		return;
	else if (info->tos_value == 0)
		printf(" TOS and 0x%02x",
		       (unsigned int)(uint8_t)~info->tos_mask);
	else if (info->tos_value == info->tos_mask)
		printf(" TOS or 0x%02x", info->tos_value);
	else if (info->tos_mask == 0)
		printf(" TOS xor 0x%02x", info->tos_value);
	else
		printf(" TOS set 0x%02x/0x%02x",
		       info->tos_value, info->tos_mask);
}

static void tos_tg_save_v0(const void *ip, const struct xt_entry_target *target)
{
	const struct ipt_tos_target_info *info = (const void *)target->data;

	printf(" --set-tos 0x%02x", info->tos);
}

static void tos_tg_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_tos_target_info *info = (const void *)target->data;

	printf(" --set-tos 0x%02x/0x%02x", info->tos_value, info->tos_mask);
}

static struct xtables_target tos_tg_reg[] = {
	{
		.version       = XTABLES_VERSION,
		.name          = "TOS",
		.revision      = 0,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct xt_tos_target_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_tos_target_info)),
		.help          = tos_tg_help_v0,
		.print         = tos_tg_print_v0,
		.save          = tos_tg_save_v0,
		.x6_parse      = tos_tg_parse_v0,
		.x6_fcheck     = tos_tg_check,
		.x6_options    = tos_tg_opts_v0,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "TOS",
		.revision      = 1,
		.family        = NFPROTO_UNSPEC,
		.size          = XT_ALIGN(sizeof(struct xt_tos_target_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_tos_target_info)),
		.help          = tos_tg_help,
		.print         = tos_tg_print,
		.save          = tos_tg_save,
		.x6_parse      = tos_tg_parse,
		.x6_fcheck     = tos_tg_check,
		.x6_options    = tos_tg_opts,
	},
};

void _init(void)
{
	xtables_register_targets(tos_tg_reg, ARRAY_SIZE(tos_tg_reg));
}
