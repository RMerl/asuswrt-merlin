/*
 * Shared library add-on to ip6tables to add CONNSECMARK target support.
 *
 * Based on the MARK and CONNMARK targets.
 *
 * Copyright (C) 2006 Red Hat, Inc., James Morris <jmorris@redhat.com>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ip6tables.h>
#include <linux/netfilter/xt_CONNSECMARK.h>

#define PFX "CONNSECMARK target: "

static void help(void)
{
	printf(
"CONNSECMARK target v%s options:\n"
"  --save                   Copy security mark from packet to conntrack\n"
"  --restore                Copy security mark from connection to packet\n"
"\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "save", 0, 0, '1' },
	{ "restore", 0, 0, '2' },
	{ 0 }
};

static int parse(int c, char **argv, int invert, unsigned int *flags,
                 const struct ip6t_entry *entry, struct ip6t_entry_target **target)
{
	struct xt_connsecmark_target_info *info =
		(struct xt_connsecmark_target_info*)(*target)->data;

	switch (c) {
	case '1':
		if (*flags & CONNSECMARK_SAVE)
			exit_error(PARAMETER_PROBLEM, PFX
				   "Can't specify --save twice");
		info->mode = CONNSECMARK_SAVE;
		*flags |= CONNSECMARK_SAVE;
		break;

	case '2':
		if (*flags & CONNSECMARK_RESTORE)
			exit_error(PARAMETER_PROBLEM, PFX
				   "Can't specify --restore twice");
		info->mode = CONNSECMARK_RESTORE;
		*flags |= CONNSECMARK_RESTORE;
		break;

	default:
		return 0;
	}

	return 1;
}

static void final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM, PFX "parameter required");

	if (flags == (CONNSECMARK_SAVE|CONNSECMARK_RESTORE))
		exit_error(PARAMETER_PROBLEM, PFX "only one flag of --save "
		           "or --restore is allowed");
}

static void print_connsecmark(struct xt_connsecmark_target_info *info)
{
	switch (info->mode) {
	case CONNSECMARK_SAVE:
		printf("save ");
		break;
		
	case CONNSECMARK_RESTORE:
		printf("restore ");
		break;
		
	default:
		exit_error(OTHER_PROBLEM, PFX "invalid mode %hhu\n", info->mode);
	}
}

static void print(const struct ip6t_ip6 *ip,
		  const struct ip6t_entry_target *target, int numeric)
{
	struct xt_connsecmark_target_info *info =
		(struct xt_connsecmark_target_info*)(target)->data;

	printf("CONNSECMARK ");
	print_connsecmark(info);
}

static void save(const struct ip6t_ip6 *ip, const struct ip6t_entry_target *target)
{
	struct xt_connsecmark_target_info *info =
		(struct xt_connsecmark_target_info*)target->data;

	printf("--");
	print_connsecmark(info);
}

static struct ip6tables_target connsecmark = {
	.name		= "CONNSECMARK",
	.version	= IPTABLES_VERSION,
	.size		= IP6T_ALIGN(sizeof(struct xt_connsecmark_target_info)),
	.userspacesize	= IP6T_ALIGN(sizeof(struct xt_connsecmark_target_info)),
	.parse		= &parse,
	.help		= &help,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_target6(&connsecmark);
}
