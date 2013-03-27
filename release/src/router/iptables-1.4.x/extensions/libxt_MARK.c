#include <stdbool.h>
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_MARK.h>

/* Version 0 */
struct xt_mark_target_info {
	unsigned long mark;
};

/* Version 1 */
enum {
	XT_MARK_SET=0,
	XT_MARK_AND,
	XT_MARK_OR,
};

struct xt_mark_target_info_v1 {
	unsigned long mark;
	uint8_t mode;
};

enum {
	O_SET_MARK = 0,
	O_AND_MARK,
	O_OR_MARK,
	O_XOR_MARK,
	O_SET_XMARK,
	F_SET_MARK  = 1 << O_SET_MARK,
	F_AND_MARK  = 1 << O_AND_MARK,
	F_OR_MARK   = 1 << O_OR_MARK,
	F_XOR_MARK  = 1 << O_XOR_MARK,
	F_SET_XMARK = 1 << O_SET_XMARK,
	F_ANY       = F_SET_MARK | F_AND_MARK | F_OR_MARK |
	              F_XOR_MARK | F_SET_XMARK,
};

static void MARK_help(void)
{
	printf(
"MARK target options:\n"
"  --set-mark value                   Set nfmark value\n"
"  --and-mark value                   Binary AND the nfmark with value\n"
"  --or-mark  value                   Binary OR  the nfmark with value\n");
}

static const struct xt_option_entry MARK_opts[] = {
	{.name = "set-mark", .id = O_SET_MARK, .type = XTTYPE_UINT32,
	 .excl = F_ANY},
	{.name = "and-mark", .id = O_AND_MARK, .type = XTTYPE_UINT32,
	 .excl = F_ANY},
	{.name = "or-mark", .id = O_OR_MARK, .type = XTTYPE_UINT32,
	 .excl = F_ANY},
	XTOPT_TABLEEND,
};

static const struct xt_option_entry mark_tg_opts[] = {
	{.name = "set-xmark", .id = O_SET_XMARK, .type = XTTYPE_MARKMASK32,
	 .excl = F_ANY},
	{.name = "set-mark", .id = O_SET_MARK, .type = XTTYPE_MARKMASK32,
	 .excl = F_ANY},
	{.name = "and-mark", .id = O_AND_MARK, .type = XTTYPE_UINT32,
	 .excl = F_ANY},
	{.name = "or-mark", .id = O_OR_MARK, .type = XTTYPE_UINT32,
	 .excl = F_ANY},
	{.name = "xor-mark", .id = O_XOR_MARK, .type = XTTYPE_UINT32,
	 .excl = F_ANY},
	XTOPT_TABLEEND,
};

static void mark_tg_help(void)
{
	printf(
"MARK target options:\n"
"  --set-xmark value[/mask]  Clear bits in mask and XOR value into nfmark\n"
"  --set-mark value[/mask]   Clear bits in mask and OR value into nfmark\n"
"  --and-mark bits           Binary AND the nfmark with bits\n"
"  --or-mark bits            Binary OR the nfmark with bits\n"
"  --xor-mask bits           Binary XOR the nfmark with bits\n"
"\n");
}

static void MARK_parse_v0(struct xt_option_call *cb)
{
	struct xt_mark_target_info *markinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SET_MARK:
		markinfo->mark = cb->val.mark;
		break;
	default:
		xtables_error(PARAMETER_PROBLEM,
			   "MARK target: kernel too old for --%s",
			   cb->entry->name);
	}
}

static void MARK_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM,
		           "MARK target: Parameter --set/and/or-mark"
			   " is required");
}

static void MARK_parse_v1(struct xt_option_call *cb)
{
	struct xt_mark_target_info_v1 *markinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SET_MARK:
	        markinfo->mode = XT_MARK_SET;
		break;
	case O_AND_MARK:
	        markinfo->mode = XT_MARK_AND;
		break;
	case O_OR_MARK:
	        markinfo->mode = XT_MARK_OR;
		break;
	}
	markinfo->mark = cb->val.u32;
}

static void mark_tg_parse(struct xt_option_call *cb)
{
	struct xt_mark_tginfo2 *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SET_XMARK:
		info->mark = cb->val.mark;
		info->mask = cb->val.mask;
		break;
	case O_SET_MARK:
		info->mark = cb->val.mark;
		info->mask = cb->val.mark | cb->val.mask;
		break;
	case O_AND_MARK:
		info->mark = 0;
		info->mask = ~cb->val.u32;
		break;
	case O_OR_MARK:
		info->mark = info->mask = cb->val.u32;
		break;
	case O_XOR_MARK:
		info->mark = cb->val.u32;
		info->mask = 0;
		break;
	}
}

static void mark_tg_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM, "MARK: One of the --set-xmark, "
		           "--{and,or,xor,set}-mark options is required");
}

static void
print_mark(unsigned long mark)
{
	printf(" 0x%lx", mark);
}

static void MARK_print_v0(const void *ip,
                          const struct xt_entry_target *target, int numeric)
{
	const struct xt_mark_target_info *markinfo =
		(const struct xt_mark_target_info *)target->data;
	printf(" MARK set");
	print_mark(markinfo->mark);
}

static void MARK_save_v0(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_mark_target_info *markinfo =
		(const struct xt_mark_target_info *)target->data;

	printf(" --set-mark");
	print_mark(markinfo->mark);
}

static void MARK_print_v1(const void *ip, const struct xt_entry_target *target,
                          int numeric)
{
	const struct xt_mark_target_info_v1 *markinfo =
		(const struct xt_mark_target_info_v1 *)target->data;

	switch (markinfo->mode) {
	case XT_MARK_SET:
		printf(" MARK set");
		break;
	case XT_MARK_AND:
		printf(" MARK and");
		break;
	case XT_MARK_OR: 
		printf(" MARK or");
		break;
	}
	print_mark(markinfo->mark);
}

static void mark_tg_print(const void *ip, const struct xt_entry_target *target,
                          int numeric)
{
	const struct xt_mark_tginfo2 *info = (const void *)target->data;

	if (info->mark == 0)
		printf(" MARK and 0x%x", (unsigned int)(uint32_t)~info->mask);
	else if (info->mark == info->mask)
		printf(" MARK or 0x%x", info->mark);
	else if (info->mask == 0)
		printf(" MARK xor 0x%x", info->mark);
	else if (info->mask == 0xffffffffU)
		printf(" MARK set 0x%x", info->mark);
	else
		printf(" MARK xset 0x%x/0x%x", info->mark, info->mask);
}

static void MARK_save_v1(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_mark_target_info_v1 *markinfo =
		(const struct xt_mark_target_info_v1 *)target->data;

	switch (markinfo->mode) {
	case XT_MARK_SET:
		printf(" --set-mark");
		break;
	case XT_MARK_AND:
		printf(" --and-mark");
		break;
	case XT_MARK_OR: 
		printf(" --or-mark");
		break;
	}
	print_mark(markinfo->mark);
}

static void mark_tg_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_mark_tginfo2 *info = (const void *)target->data;

	printf(" --set-xmark 0x%x/0x%x", info->mark, info->mask);
}

static struct xtables_target mark_tg_reg[] = {
	{
		.family        = NFPROTO_UNSPEC,
		.name          = "MARK",
		.version       = XTABLES_VERSION,
		.revision      = 0,
		.size          = XT_ALIGN(sizeof(struct xt_mark_target_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_mark_target_info)),
		.help          = MARK_help,
		.print         = MARK_print_v0,
		.save          = MARK_save_v0,
		.x6_parse      = MARK_parse_v0,
		.x6_fcheck     = MARK_check,
		.x6_options    = MARK_opts,
	},
	{
		.family        = NFPROTO_IPV4,
		.name          = "MARK",
		.version       = XTABLES_VERSION,
		.revision      = 1,
		.size          = XT_ALIGN(sizeof(struct xt_mark_target_info_v1)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_mark_target_info_v1)),
		.help          = MARK_help,
		.print         = MARK_print_v1,
		.save          = MARK_save_v1,
		.x6_parse      = MARK_parse_v1,
		.x6_fcheck     = MARK_check,
		.x6_options    = MARK_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "MARK",
		.revision      = 2,
		.family        = NFPROTO_UNSPEC,
		.size          = XT_ALIGN(sizeof(struct xt_mark_tginfo2)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_mark_tginfo2)),
		.help          = mark_tg_help,
		.print         = mark_tg_print,
		.save          = mark_tg_save,
		.x6_parse      = mark_tg_parse,
		.x6_fcheck     = mark_tg_check,
		.x6_options    = mark_tg_opts,
	},
};

void _init(void)
{
	xtables_register_targets(mark_tg_reg, ARRAY_SIZE(mark_tg_reg));
}
