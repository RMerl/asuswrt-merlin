/* ebt_mark_m
 *
 * Authors:
 * Bart De Schuymer <bdschuym@pandora.be>
 *
 * July, 2002
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "../include/ebtables_u.h"
#include <linux/netfilter_bridge/ebt_mark_m.h>

#define MARK '1'

static struct option opts[] =
{
	{ "mark", required_argument, 0, MARK },
	{ 0 }
};

static void print_help()
{
	printf(
"mark option:\n"
"--mark    [!] [value][/mask]: Match nfmask value (see man page)\n");
}

static void init(struct ebt_entry_match *match)
{
	struct ebt_mark_m_info *markinfo = (struct ebt_mark_m_info *)match->data;

	markinfo->mark    = 0;
	markinfo->mask    = 0;
	markinfo->invert  = 0;
	markinfo->bitmask = 0;
}

#define OPT_MARK 0x01
static int parse(int c, char **argv, int argc, const struct ebt_u_entry *entry,
   unsigned int *flags, struct ebt_entry_match **match)
{
	struct ebt_mark_m_info *markinfo = (struct ebt_mark_m_info *)
	   (*match)->data;
	char *end;

	switch (c) {
	case MARK:
		ebt_check_option2(flags, MARK);
		if (ebt_check_inverse2(optarg))
			markinfo->invert = 1;
		markinfo->mark = strtoul(optarg, &end, 0);
		markinfo->bitmask = EBT_MARK_AND;
		if (*end == '/') {
			if (end == optarg)
				markinfo->bitmask = EBT_MARK_OR;
			markinfo->mask = strtoul(end+1, &end, 0);
		} else
			markinfo->mask = 0xffffffff;
		if ( *end != '\0' || end == optarg)
			ebt_print_error2("Bad mark value '%s'", optarg);
		break;
	default:
		return 0;
	}
	return 1;
}

static void final_check(const struct ebt_u_entry *entry,
   const struct ebt_entry_match *match, const char *name,
   unsigned int hookmask, unsigned int time)
{
}

static void print(const struct ebt_u_entry *entry,
   const struct ebt_entry_match *match)
{
	struct ebt_mark_m_info *markinfo =
	   (struct ebt_mark_m_info *)match->data;

	printf("--mark ");
	if (markinfo->invert)
		printf("! ");
	if (markinfo->bitmask == EBT_MARK_OR)
		printf("/0x%lx ", markinfo->mask);
	else if(markinfo->mask != 0xffffffff)
		printf("0x%lx/0x%lx ", markinfo->mark, markinfo->mask);
	else
		printf("0x%lx ", markinfo->mark);
}

static int compare(const struct ebt_entry_match *m1,
   const struct ebt_entry_match *m2)
{
	struct ebt_mark_m_info *markinfo1 = (struct ebt_mark_m_info *)m1->data;
	struct ebt_mark_m_info *markinfo2 = (struct ebt_mark_m_info *)m2->data;

	if (markinfo1->invert != markinfo2->invert)
		return 0;
	if (markinfo1->mark != markinfo2->mark)
		return 0;
	if (markinfo1->mask != markinfo2->mask)
		return 0;
	if (markinfo1->bitmask != markinfo2->bitmask)
		return 0;
	return 1;
}

static struct ebt_u_match mark_match =
{
	.name		= "mark_m",
	.size		= sizeof(struct ebt_mark_m_info),
	.help		= print_help,
	.init		= init,
	.parse		= parse,
	.final_check	= final_check,
	.print		= print,
	.compare	= compare,
	.extra_ops	= opts,
};

void _init(void)
{
	ebt_register_match(&mark_match);
}
