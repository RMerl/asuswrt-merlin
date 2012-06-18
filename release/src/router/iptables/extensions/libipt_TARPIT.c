/* Shared library add-on to iptables for TARPIT support */
#include <stdio.h>
#include <getopt.h>
#include <iptables.h>

#ifndef XTABLES_VERSION
#define XTABLES_VERSION IPTABLES_VERSION
#endif

#ifdef IPT_LIB_DIR
#define xtables_target iptables_target
#define xtables_register_target register_target
#endif

static void
help(void)
{
	fputs(
"TARPIT takes no options\n"
"\n", stdout);
}

static struct option opts[] = {
	{ 0 }
};


static int
parse(int c, char **argv, int invert, unsigned int *flags,
#ifdef _XTABLES_H
      const void *entry, struct xt_entry_target **target)
#else
      const struct ipt_entry *entry, struct ipt_entry_target **target)
#endif
{
	return 0;
}

static void final_check(unsigned int flags)
{
}

static void
#ifdef _XTABLES_H
print(const void *ip,
      const struct xt_entry_target *target,
#else
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
#endif
      int numeric)
{
}

static void
#ifdef _XTABLES_H
save(const void *ip,
     const struct xt_entry_target *target)
#else
save(const struct ipt_ip *ip,
     const struct ipt_entry_target *target)
#endif
{
}

static struct xtables_target tarpit = {
	.next		= NULL,
	.name		= "TARPIT",
	.version	= XTABLES_VERSION,
	.size		= IPT_ALIGN(0),
	.userspacesize	= IPT_ALIGN(0),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	xtables_register_target(&tarpit);
}
