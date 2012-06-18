/*

	BCOUNT target (experimental)
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_BCOUNT.h>

static void help(void)
{
	printf("BCOUNT target (experimental) v0.01\nCopyright (C) 2006 Jonathan Zarate\n");
}

static void init(struct ipt_entry_target *t, unsigned int *nfcache)
{
}

static struct option opts[] = { { 0 } };

static int parse(int c, char **argv, int invert, unsigned int *flags,
				 const struct ipt_entry *entry, struct ipt_entry_target **target)
{
	return 0;
}

static void final_check(unsigned int flags)
{
}

static struct iptables_target BCOUNT_target
= {	.next = NULL,
	.name = "BCOUNT",
	.version = IPTABLES_VERSION,
	.size = IPT_ALIGN(sizeof(struct ipt_BCOUNT_target)),
	.userspacesize = IPT_ALIGN(sizeof(struct ipt_BCOUNT_target)),
	.help = &help,
	.init = &init,
	.parse = &parse,
	.final_check = &final_check,
	.print = NULL,
	.save = NULL,
	.extra_opts = opts
};

void _init(void)
{
	register_target(&BCOUNT_target);
}
