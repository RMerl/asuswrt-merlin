/* Shared library add-on to iptables to add NFMARK matching support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
/* For 64bit kernel / 32bit userspace */
#include "../include/linux/netfilter_ipv4/ipt_mark.h"

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"MARK match v%s options:\n"
"[!] --mark value[/mask]         Match nfmark value with optional mask\n"
"\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "mark", 1, 0, '1' },
	{0}
};

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_mark_info *markinfo = (struct ipt_mark_info *)(*match)->data;

	switch (c) {
		char *end;
	case '1':
		check_inverse(optarg, &invert, &optind, 0);
#ifdef KERNEL_64_USERSPACE_32
		markinfo->mark = strtoull(optarg, &end, 0);
		if (*end == '/') {
			markinfo->mask = strtoull(end+1, &end, 0);
		} else
			markinfo->mask = 0xffffffffffffffffULL;
#else
		markinfo->mark = strtoul(optarg, &end, 0);
		if (*end == '/') {
			markinfo->mask = strtoul(end+1, &end, 0);
		} else
			markinfo->mask = 0xffffffff;
#endif
		if (*end != '\0' || end == optarg)
			exit_error(PARAMETER_PROBLEM, "Bad MARK value `%s'", optarg);
		if (invert)
			markinfo->invert = 1;
		*flags = 1;
		break;

	default:
		return 0;
	}
	return 1;
}

#ifdef KERNEL_64_USERSPACE_32
static void
print_mark(unsigned long long mark, unsigned long long mask, int numeric)
{
	if(mask != 0xffffffffffffffffULL)
		printf("0x%llx/0x%llx ", mark, mask);
	else
		printf("0x%llx ", mark);
}
#else
static void
print_mark(unsigned long mark, unsigned long mask, int numeric)
{
	if(mask != 0xffffffff)
		printf("0x%lx/0x%lx ", mark, mask);
	else
		printf("0x%lx ", mark);
}
#endif

/* Final check; must have specified --mark. */
static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "MARK match: You must specify `--mark'");
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	struct ipt_mark_info *info = (struct ipt_mark_info *)match->data;

	printf("MARK match ");

	if (info->invert)
		printf("!");
	
	print_mark(info->mark, info->mask, numeric);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	struct ipt_mark_info *info = (struct ipt_mark_info *)match->data;

	if (info->invert)
		printf("! ");
	
	printf("--mark ");
	print_mark(info->mark, info->mask, 0);
}

static struct iptables_match mark = { 
	.next		= NULL,
	.name		= "mark",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_mark_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_mark_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match(&mark);
}
