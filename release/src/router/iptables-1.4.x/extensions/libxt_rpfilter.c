#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_rpfilter.h>

enum {
	O_RPF_LOOSE = 0,
	O_RPF_VMARK = 1,
	O_RPF_ACCEPT_LOCAL = 2,
	O_RPF_INVERT = 3,
};

static void rpfilter_help(void)
{
	printf(
"rpfilter match options:\n"
"    --loose          permit reverse path via any interface\n"
"    --validmark      use skb nfmark when performing route lookup\n"
"    --accept-local   do not reject packets with a local source address\n"
"    --invert         match packets that failed the reverse path test\n"
	);
}

static const struct xt_option_entry rpfilter_opts[] = {
	{.name = "loose", .id = O_RPF_LOOSE, .type = XTTYPE_NONE, },
	{.name = "validmark", .id = O_RPF_VMARK, .type = XTTYPE_NONE, },
	{.name = "accept-local", .id = O_RPF_ACCEPT_LOCAL, .type = XTTYPE_NONE, },
	{.name = "invert", .id = O_RPF_INVERT, .type = XTTYPE_NONE, },
	XTOPT_TABLEEND,
};

static void rpfilter_parse(struct xt_option_call *cb)
{
	struct xt_rpfilter_info *rpfinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_RPF_LOOSE:
		rpfinfo->flags |= XT_RPFILTER_LOOSE;
		break;
	case O_RPF_VMARK:
		rpfinfo->flags |= XT_RPFILTER_VALID_MARK;
		break;
	case O_RPF_ACCEPT_LOCAL:
		rpfinfo->flags |= XT_RPFILTER_ACCEPT_LOCAL;
		break;
	case O_RPF_INVERT:
		rpfinfo->flags |= XT_RPFILTER_INVERT;
		break;
	}
}

static void
rpfilter_print_prefix(const void *ip, const void *matchinfo,
			const char *prefix)
{
	const struct xt_rpfilter_info *info = matchinfo;
	if (info->flags & XT_RPFILTER_LOOSE)
		printf(" %s%s", prefix, rpfilter_opts[O_RPF_LOOSE].name);
	if (info->flags & XT_RPFILTER_VALID_MARK)
		printf(" %s%s", prefix, rpfilter_opts[O_RPF_VMARK].name);
	if (info->flags & XT_RPFILTER_ACCEPT_LOCAL)
		printf(" %s%s", prefix, rpfilter_opts[O_RPF_ACCEPT_LOCAL].name);
	if (info->flags & XT_RPFILTER_INVERT)
		printf(" %s%s", prefix, rpfilter_opts[O_RPF_INVERT].name);
}


static void
rpfilter_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	printf(" rpfilter");
	return rpfilter_print_prefix(ip, match->data, "");
}

static void rpfilter_save(const void *ip, const struct xt_entry_match *match)
{
	return rpfilter_print_prefix(ip, match->data, "--");
}

static struct xtables_match rpfilter_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "rpfilter",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_rpfilter_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_rpfilter_info)),
	.help		= rpfilter_help,
	.print		= rpfilter_print,
	.save		= rpfilter_save,
	.x6_parse	= rpfilter_parse,
	.x6_options	= rpfilter_opts,
};

void _init(void)
{
	xtables_register_match(&rpfilter_match);
}
