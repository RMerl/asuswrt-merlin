/* ebt_redirect
 *
 * Authors:
 * Bart De Schuymer <bdschuym@pandora.be>
 *
 * April, 2002
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "../include/ebtables_u.h"
#include <linux/netfilter_bridge/ebt_redirect.h>

#define REDIRECT_TARGET '1'
static struct option opts[] =
{
	{ "redirect-target", required_argument, 0, REDIRECT_TARGET },
	{ 0 }
};

static void print_help()
{
	printf(
	"redirect option:\n"
	" --redirect-target target   : ACCEPT, DROP, RETURN or CONTINUE\n");
}

static void init(struct ebt_entry_target *target)
{
	struct ebt_redirect_info *redirectinfo =
	   (struct ebt_redirect_info *)target->data;

	redirectinfo->target = EBT_ACCEPT;
	return;
}

#define OPT_REDIRECT_TARGET  0x01
static int parse(int c, char **argv, int argc,
   const struct ebt_u_entry *entry, unsigned int *flags,
   struct ebt_entry_target **target)
{
	struct ebt_redirect_info *redirectinfo =
	   (struct ebt_redirect_info *)(*target)->data;

	switch (c) {
	case REDIRECT_TARGET:
		ebt_check_option2(flags, OPT_REDIRECT_TARGET);
		if (FILL_TARGET(optarg, redirectinfo->target))
			ebt_print_error2("Illegal --redirect-target target");
		break;
	default:
		return 0;
	}
	return 1;
}

static void final_check(const struct ebt_u_entry *entry,
   const struct ebt_entry_target *target, const char *name,
   unsigned int hookmask, unsigned int time)
{
	struct ebt_redirect_info *redirectinfo =
	   (struct ebt_redirect_info *)target->data;

	if (BASE_CHAIN && redirectinfo->target == EBT_RETURN) {
		ebt_print_error("--redirect-target RETURN not allowed on base chain");
		return;
	}
	CLEAR_BASE_CHAIN_BIT;
	if ( ((hookmask & ~(1 << NF_BR_PRE_ROUTING)) || strcmp(name, "nat")) &&
	   ((hookmask & ~(1 << NF_BR_BROUTING)) || strcmp(name, "broute")) )
		ebt_print_error("Wrong chain for redirect");
}

static void print(const struct ebt_u_entry *entry,
   const struct ebt_entry_target *target)
{
	struct ebt_redirect_info *redirectinfo =
	   (struct ebt_redirect_info *)target->data;

	if (redirectinfo->target == EBT_ACCEPT)
		return;
	printf(" --redirect-target %s", TARGET_NAME(redirectinfo->target));
}

static int compare(const struct ebt_entry_target *t1,
   const struct ebt_entry_target *t2)
{
	struct ebt_redirect_info *redirectinfo1 =
	   (struct ebt_redirect_info *)t1->data;
	struct ebt_redirect_info *redirectinfo2 =
	   (struct ebt_redirect_info *)t2->data;

	return redirectinfo1->target == redirectinfo2->target;
}

static struct ebt_u_target redirect_target =
{
	.name		= "redirect",
	.size		= sizeof(struct ebt_redirect_info),
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
	ebt_register_target(&redirect_target);
}
