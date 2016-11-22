/* Shared library add-on to iptables to add MARK target support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
/* For 64bit kernel / 32bit userspace */
#include "../include/linux/netfilter_ipv6/ip6t_MARK.h"

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"MARK target v%s options:\n"
"  --set-mark value[/mask]            Clear bits in mask and OR value into nfmark\n"
"\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ .name = "set-mark", .has_arg = 1, .flag = 0, .val = '1' },
	{ .name = 0 }
};

/* Initialize the target. */
static void
init(struct ip6t_entry_target *t, unsigned int *nfcache)
{
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      struct ip6t_entry_target **target)
{
	struct ip6t_mark_target_info *markinfo
		= (struct ip6t_mark_target_info *)(*target)->data;

	switch (c) {
		char *end;
	case '1':
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
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "MARK target: Can't specify --set-mark twice");
		*flags = 1;
		break;

	default:
		return 0;
	}

	return 1;
}

static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
		           "MARK target: Parameter --set-mark is required");
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

/* Prints out the targinfo. */
static void
print(const struct ip6t_ip6 *ip,
      const struct ip6t_entry_target *target,
      int numeric)
{
	const struct ip6t_mark_target_info *markinfo =
		(const struct ip6t_mark_target_info *)target->data;

	printf("MARK set ");
	print_mark(markinfo->mark, markinfo->mask, numeric);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save(const struct ip6t_ip6 *ip, const struct ip6t_entry_target *target)
{
	const struct ip6t_mark_target_info *markinfo =
		(const struct ip6t_mark_target_info *)target->data;

	printf("--set-mark ");
	print_mark(markinfo->mark, markinfo->mask, 0);
}

static
struct ip6tables_target mark = {
	.name          = "MARK",
	.version       = IPTABLES_VERSION,
	.size          = IP6T_ALIGN(sizeof(struct ip6t_mark_target_info)),
	.userspacesize = IP6T_ALIGN(sizeof(struct ip6t_mark_target_info)),
	.help          = &help,
	.init          = &init,
	.parse         = &parse,
	.final_check   = &final_check,
	.print         = &print,
	.save          = &save,
	.extra_opts    = opts
};

void _init(void)
{
	register_target6(&mark);
}
