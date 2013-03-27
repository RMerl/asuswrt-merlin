/* Shared library add-on to iptables to add CONNMARK target support.
 *
 * (C) 2002,2004 MARA Systems AB <http://www.marasystems.com>
 * by Henrik Nordstrom <hno@marasystems.com>
 *
 * Version 1.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_CONNMARK.h>

struct xt_connmark_target_info {
	unsigned long mark;
	unsigned long mask;
	uint8_t mode;
};

enum {
	O_SET_MARK = 0,
	O_SAVE_MARK,
	O_SET_RETURN,
	O_RESTORE_MARK,
	O_AND_MARK,
	O_OR_MARK,
	O_XOR_MARK,
	O_SET_XMARK,
	O_CTMASK,
	O_NFMASK,
	O_MASK,
	F_SET_MARK     = 1 << O_SET_MARK,
	F_SAVE_MARK    = 1 << O_SAVE_MARK,
	F_SET_RETURN   = 1 << O_SET_RETURN,
	F_RESTORE_MARK = 1 << O_RESTORE_MARK,
	F_AND_MARK     = 1 << O_AND_MARK,
	F_OR_MARK      = 1 << O_OR_MARK,
	F_XOR_MARK     = 1 << O_XOR_MARK,
	F_SET_XMARK    = 1 << O_SET_XMARK,
	F_CTMASK       = 1 << O_CTMASK,
	F_NFMASK       = 1 << O_NFMASK,
	F_MASK         = 1 << O_MASK,
	F_OP_ANY       = F_SET_MARK | F_SAVE_MARK | F_SET_RETURN | F_RESTORE_MARK |
	                 F_AND_MARK | F_OR_MARK | F_XOR_MARK | F_SET_XMARK,
};

static void CONNMARK_help(void)
{
	printf(
"CONNMARK target options:\n"
"  --set-mark value[/mask]       Set conntrack mark value\n"
"  --set-return [--mask mask]    Set conntrack mark & nfmark, RETURN\n"
"  --save-mark [--mask mask]     Save the packet nfmark in the connection\n"
"  --restore-mark [--mask mask]  Restore saved nfmark value\n");
}

#define s struct xt_connmark_target_info
static const struct xt_option_entry CONNMARK_opts[] = {
	{.name = "set-mark", .id = O_SET_MARK, .type = XTTYPE_MARKMASK32,
	 .excl = F_OP_ANY},
	{.name = "save-mark", .id = O_SAVE_MARK, .type = XTTYPE_NONE,
	 .excl = F_OP_ANY},
	{.name = "set-return", .id = O_SET_RETURN, .type = XTTYPE_MARKMASK32,
	 .excl = F_OP_ANY},
	{.name = "restore-mark", .id = O_RESTORE_MARK, .type = XTTYPE_NONE,
	 .excl = F_OP_ANY},
	{.name = "mask", .id = O_MASK, .type = XTTYPE_UINT32},
	XTOPT_TABLEEND,
};
#undef s

#define s struct xt_connmark_tginfo1
static const struct xt_option_entry connmark_tg_opts[] = {
	{.name = "set-xmark", .id = O_SET_XMARK, .type = XTTYPE_MARKMASK32,
	 .excl = F_OP_ANY},
	{.name = "set-mark", .id = O_SET_MARK, .type = XTTYPE_MARKMASK32,
	 .excl = F_OP_ANY},
	{.name = "set-return", .id = O_SET_RETURN, .type = XTTYPE_MARKMASK32,
	 .excl = F_OP_ANY},
	{.name = "and-mark", .id = O_AND_MARK, .type = XTTYPE_UINT32,
	 .excl = F_OP_ANY},
	{.name = "or-mark", .id = O_OR_MARK, .type = XTTYPE_UINT32,
	 .excl = F_OP_ANY},
	{.name = "xor-mark", .id = O_XOR_MARK, .type = XTTYPE_UINT32,
	 .excl = F_OP_ANY},
	{.name = "save-mark", .id = O_SAVE_MARK, .type = XTTYPE_NONE,
	 .excl = F_OP_ANY},
	{.name = "restore-mark", .id = O_RESTORE_MARK, .type = XTTYPE_NONE,
	 .excl = F_OP_ANY},
	{.name = "ctmask", .id = O_CTMASK, .type = XTTYPE_UINT32,
	 .excl = F_MASK, .flags = XTOPT_PUT, XTOPT_POINTER(s, ctmask)},
	{.name = "nfmask", .id = O_NFMASK, .type = XTTYPE_UINT32,
	 .excl = F_MASK, .flags = XTOPT_PUT, XTOPT_POINTER(s, nfmask)},
	{.name = "mask", .id = O_MASK, .type = XTTYPE_UINT32,
	 .excl = F_CTMASK | F_NFMASK},
	XTOPT_TABLEEND,
};
#undef s

static void connmark_tg_help(void)
{
	printf(
"CONNMARK target options:\n"
"  --set-xmark value[/ctmask]    Zero mask bits and XOR ctmark with value\n"
"  --save-mark [--ctmask mask] [--nfmask mask]\n"
"                                Copy ctmark to nfmark using masks\n"
"  --restore-mark [--ctmask mask] [--nfmask mask]\n"
"                                Copy nfmark to ctmark using masks\n"
"  --set-mark value[/mask]       Set conntrack mark value\n"
"  --set-return value[/mask]     Set conntrack mark & nfmark, RETURN\n"
"  --save-mark [--mask mask]     Save the packet nfmark in the connection\n"
"  --restore-mark [--mask mask]  Restore saved nfmark value\n"
"  --and-mark value              Binary AND the ctmark with bits\n"
"  --or-mark value               Binary OR  the ctmark with bits\n"
"  --xor-mark value              Binary XOR the ctmark with bits\n"
);
}

static void connmark_tg_init(struct xt_entry_target *target)
{
	struct xt_connmark_tginfo1 *info = (void *)target->data;

	/*
	 * Need these defaults for --save-mark/--restore-mark if no
	 * --ctmark or --nfmask is given.
	 */
	info->ctmask = UINT32_MAX;
	info->nfmask = UINT32_MAX;
}

static void CONNMARK_parse(struct xt_option_call *cb)
{
	struct xt_connmark_target_info *markinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SET_MARK:
		markinfo->mode = XT_CONNMARK_SET;
		markinfo->mark = cb->val.mark;
		markinfo->mask = cb->val.mask;
		break;
	case O_SET_RETURN:
		markinfo->mode = XT_CONNMARK_SET_RETURN;
		markinfo->mark = cb->val.mark;
		markinfo->mask = cb->val.mask;
		break;
	case O_SAVE_MARK:
		markinfo->mode = XT_CONNMARK_SAVE;
		break;
	case O_RESTORE_MARK:
		markinfo->mode = XT_CONNMARK_RESTORE;
		break;
	case O_MASK:
		markinfo->mask = cb->val.u32;
		break;
	}
}

static void connmark_tg_parse(struct xt_option_call *cb)
{
	struct xt_connmark_tginfo1 *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SET_XMARK:
		info->mode   = XT_CONNMARK_SET;
		info->ctmark = cb->val.mark;
		info->ctmask = cb->val.mask;
		break;
	case O_SET_MARK:
		info->mode   = XT_CONNMARK_SET;
		info->ctmark = cb->val.mark;
		info->ctmask = cb->val.mark | cb->val.mask;
		break;
	case O_SET_RETURN:
		info->mode   = XT_CONNMARK_SET_RETURN;
		info->ctmark = cb->val.mark;
		info->ctmask = cb->val.mask;
		break;
	case O_AND_MARK:
		info->mode   = XT_CONNMARK_SET;
		info->ctmark = 0;
		info->ctmask = ~cb->val.u32;
		break;
	case O_OR_MARK:
		info->mode   = XT_CONNMARK_SET;
		info->ctmark = cb->val.u32;
		info->ctmask = cb->val.u32;
		break;
	case O_XOR_MARK:
		info->mode   = XT_CONNMARK_SET;
		info->ctmark = cb->val.u32;
		info->ctmask = 0;
		break;
	case O_SAVE_MARK:
		info->mode = XT_CONNMARK_SAVE;
		break;
	case O_RESTORE_MARK:
		info->mode = XT_CONNMARK_RESTORE;
		break;
	case O_MASK:
		info->nfmask = info->ctmask = cb->val.u32;
		break;
	}
}

static void connmark_tg_check(struct xt_fcheck_call *cb)
{
	if (!(cb->xflags & F_OP_ANY))
		xtables_error(PARAMETER_PROBLEM,
		           "CONNMARK target: No operation specified");
}

static void
print_mark(unsigned long mark)
{
	printf("0x%lx", mark);
}

static void
print_mask(const char *text, unsigned long mask)
{
	if (mask != 0xffffffffUL)
		printf("%s0x%lx", text, mask);
}

static void CONNMARK_print(const void *ip,
                           const struct xt_entry_target *target, int numeric)
{
	const struct xt_connmark_target_info *markinfo =
		(const struct xt_connmark_target_info *)target->data;
	switch (markinfo->mode) {
	case XT_CONNMARK_SET:
	    printf(" CONNMARK set ");
	    print_mark(markinfo->mark);
	    print_mask("/", markinfo->mask);
	    break;
	case XT_CONNMARK_SET_RETURN:
	    printf(" CONNMARK set-return ");
	    print_mark(markinfo->mark);
	    print_mask("/", markinfo->mask);
	    break;
	case XT_CONNMARK_SAVE:
	    printf(" CONNMARK save ");
	    print_mask("mask ", markinfo->mask);
	    break;
	case XT_CONNMARK_RESTORE:
	    printf(" CONNMARK restore ");
	    print_mask("mask ", markinfo->mask);
	    break;
	default:
	    printf(" ERROR: UNKNOWN CONNMARK MODE");
	    break;
	}
}

static void
connmark_tg_print(const void *ip, const struct xt_entry_target *target,
                  int numeric)
{
	const struct xt_connmark_tginfo1 *info = (const void *)target->data;

	switch (info->mode) {
	case XT_CONNMARK_SET:
		if (info->ctmark == 0)
			printf(" CONNMARK and 0x%x",
			       (unsigned int)(uint32_t)~info->ctmask);
		else if (info->ctmark == info->ctmask)
			printf(" CONNMARK or 0x%x", info->ctmark);
		else if (info->ctmask == 0)
			printf(" CONNMARK xor 0x%x", info->ctmark);
		else if (info->ctmask == 0xFFFFFFFFU)
			printf(" CONNMARK set 0x%x", info->ctmark);
		else
			printf(" CONNMARK xset 0x%x/0x%x",
			       info->ctmark, info->ctmask);
		break;
	case XT_CONNMARK_SET_RETURN:
		if (info->ctmask == 0xFFFFFFFFU)
			printf(" CONNMARK set-return 0x%x", info->ctmark);
		else
			printf(" CONNMARK set-return 0x%x/0x%x",
			       info->ctmark, info->ctmask);
		break;
	case XT_CONNMARK_SAVE:
		if (info->nfmask == UINT32_MAX && info->ctmask == UINT32_MAX)
			printf(" CONNMARK save");
		else if (info->nfmask == info->ctmask)
			printf(" CONNMARK save mask 0x%x", info->nfmask);
		else
			printf(" CONNMARK save nfmask 0x%x ctmask ~0x%x",
			       info->nfmask, info->ctmask);
		break;
	case XT_CONNMARK_RESTORE:
		if (info->ctmask == UINT32_MAX && info->nfmask == UINT32_MAX)
			printf(" CONNMARK restore");
		else if (info->ctmask == info->nfmask)
			printf(" CONNMARK restore mask 0x%x", info->ctmask);
		else
			printf(" CONNMARK restore ctmask 0x%x nfmask ~0x%x",
			       info->ctmask, info->nfmask);
		break;

	default:
		printf(" ERROR: UNKNOWN CONNMARK MODE");
		break;
	}
}

static void CONNMARK_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_connmark_target_info *markinfo =
		(const struct xt_connmark_target_info *)target->data;

	switch (markinfo->mode) {
	case XT_CONNMARK_SET:
	    printf(" --set-mark ");
	    print_mark(markinfo->mark);
	    print_mask("/", markinfo->mask);
	    break;
	case XT_CONNMARK_SET_RETURN:
	    printf(" --set-return ");
	    print_mark(markinfo->mark);
	    print_mask("/", markinfo->mask);
	    break;
	case XT_CONNMARK_SAVE:
	    printf(" --save-mark ");
	    print_mask("--mask ", markinfo->mask);
	    break;
	case XT_CONNMARK_RESTORE:
	    printf(" --restore-mark ");
	    print_mask("--mask ", markinfo->mask);
	    break;
	default:
	    printf(" ERROR: UNKNOWN CONNMARK MODE");
	    break;
	}
}

static void CONNMARK_init(struct xt_entry_target *t)
{
	struct xt_connmark_target_info *markinfo
		= (struct xt_connmark_target_info *)t->data;

	markinfo->mask = 0xffffffffUL;
}

static void
connmark_tg_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_connmark_tginfo1 *info = (const void *)target->data;

	switch (info->mode) {
	case XT_CONNMARK_SET:
		printf(" --set-xmark 0x%x/0x%x", info->ctmark, info->ctmask);
		break;
	case XT_CONNMARK_SET_RETURN:
		printf(" --set-return 0x%x/0x%x", info->ctmark, info->ctmask);
		break;
	case XT_CONNMARK_SAVE:
		printf(" --save-mark --nfmask 0x%x --ctmask 0x%x",
		       info->nfmask, info->ctmask);
		break;
	case XT_CONNMARK_RESTORE:
		printf(" --restore-mark --nfmask 0x%x --ctmask 0x%x",
		       info->nfmask, info->ctmask);
		break;
	default:
		printf(" ERROR: UNKNOWN CONNMARK MODE");
		break;
	}
}

static struct xtables_target connmark_tg_reg[] = {
	{
		.family        = NFPROTO_UNSPEC,
		.name          = "CONNMARK",
		.revision      = 0,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_connmark_target_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_connmark_target_info)),
		.help          = CONNMARK_help,
		.init          = CONNMARK_init,
		.print         = CONNMARK_print,
		.save          = CONNMARK_save,
		.x6_parse      = CONNMARK_parse,
		.x6_fcheck     = connmark_tg_check,
		.x6_options    = CONNMARK_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "CONNMARK",
		.revision      = 1,
		.family        = NFPROTO_UNSPEC,
		.size          = XT_ALIGN(sizeof(struct xt_connmark_tginfo1)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_connmark_tginfo1)),
		.help          = connmark_tg_help,
		.init          = connmark_tg_init,
		.print         = connmark_tg_print,
		.save          = connmark_tg_save,
		.x6_parse      = connmark_tg_parse,
		.x6_fcheck     = connmark_tg_check,
		.x6_options    = connmark_tg_opts,
	},
};

void _init(void)
{
	xtables_register_targets(connmark_tg_reg, ARRAY_SIZE(connmark_tg_reg));
}
