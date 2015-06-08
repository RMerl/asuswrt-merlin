/* Shared library add-on to iptables to add MARK target support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
/* For 64bit kernel / 32bit userspace */
#include "../include/linux/netfilter_ipv4/ipt_MARK.h"

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"MARK target v%s options:\n"
"  --set-mark value                   Set nfmark value\n"
"  --and-mark value                   Binary AND the nfmark with value\n"
"  --or-mark  value                   Binary OR  the nfmark with value\n"
"\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "set-mark", 1, 0, '1' },
	{ "and-mark", 1, 0, '2' },
	{ "or-mark", 1, 0, '3' },
	{ 0 }
};

/* Initialize the target. */
static void
init(struct ipt_entry_target *t, unsigned int *nfcache)
{
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse_v0(int c, char **argv, int invert, unsigned int *flags,
	 const struct ipt_entry *entry,
	 struct ipt_entry_target **target)
{
	struct ipt_mark_target_info *markinfo
		= (struct ipt_mark_target_info *)(*target)->data;

	switch (c) {
	case '1':
#ifdef KERNEL_64_USERSPACE_32
		if (string_to_number_ll(optarg, 0, 0, 
				     &markinfo->mark))
#else
		if (string_to_number_l(optarg, 0, 0, 
				     &markinfo->mark))
#endif
			exit_error(PARAMETER_PROBLEM, "Bad MARK value `%s'", optarg);
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "MARK target: Can't specify --set-mark twice");
		*flags = 1;
		break;
	case '2':
		exit_error(PARAMETER_PROBLEM,
			   "MARK target: kernel too old for --and-mark");
	case '3':
		exit_error(PARAMETER_PROBLEM,
			   "MARK target: kernel too old for --or-mark");
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
		           "MARK target: Parameter --set/and/or-mark"
			   " is required");
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse_v1(int c, char **argv, int invert, unsigned int *flags,
	 const struct ipt_entry *entry,
	 struct ipt_entry_target **target)
{
	struct ipt_mark_target_info_v1 *markinfo
		= (struct ipt_mark_target_info_v1 *)(*target)->data;

	switch (c) {
	case '1':
	        markinfo->mode = IPT_MARK_SET;
		break;
	case '2':
	        markinfo->mode = IPT_MARK_AND;
		break;
	case '3':
	        markinfo->mode = IPT_MARK_OR;
		break;
	default:
		return 0;
	}

#ifdef KERNEL_64_USERSPACE_32
	if (string_to_number_ll(optarg, 0, 0,  &markinfo->mark))
#else
	if (string_to_number_l(optarg, 0, 0, &markinfo->mark))
#endif
		exit_error(PARAMETER_PROBLEM, "Bad MARK value `%s'", optarg);

	if (*flags)
		exit_error(PARAMETER_PROBLEM,
			   "MARK target: Can't specify --set-mark twice");

	*flags = 1;
	return 1;
}

#ifdef KERNEL_64_USERSPACE_32
static void
print_mark(unsigned long long mark)
{
	printf("0x%llx ", mark);
}
#else
static void
print_mark(unsigned long mark)
{
	printf("0x%lx ", mark);
}
#endif

/* Prints out the targinfo. */
static void
print_v0(const struct ipt_ip *ip,
	 const struct ipt_entry_target *target,
	 int numeric)
{
	const struct ipt_mark_target_info *markinfo =
		(const struct ipt_mark_target_info *)target->data;
	printf("MARK set ");
	print_mark(markinfo->mark);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save_v0(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	const struct ipt_mark_target_info *markinfo =
		(const struct ipt_mark_target_info *)target->data;

	printf("--set-mark ");
	print_mark(markinfo->mark);
}

/* Prints out the targinfo. */
static void
print_v1(const struct ipt_ip *ip,
	 const struct ipt_entry_target *target,
	 int numeric)
{
	const struct ipt_mark_target_info_v1 *markinfo =
		(const struct ipt_mark_target_info_v1 *)target->data;

	switch (markinfo->mode) {
	case IPT_MARK_SET:
		printf("MARK set ");
		break;
	case IPT_MARK_AND:
		printf("MARK and ");
		break;
	case IPT_MARK_OR: 
		printf("MARK or ");
		break;
	}
	print_mark(markinfo->mark);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save_v1(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	const struct ipt_mark_target_info_v1 *markinfo =
		(const struct ipt_mark_target_info_v1 *)target->data;

	switch (markinfo->mode) {
	case IPT_MARK_SET:
		printf("--set-mark ");
		break;
	case IPT_MARK_AND:
		printf("--and-mark ");
		break;
	case IPT_MARK_OR: 
		printf("--or-mark ");
		break;
	}
	print_mark(markinfo->mark);
}

static
struct iptables_target mark_v0 = {
	.next		= NULL,
	.name		= "MARK",
	.version	= IPTABLES_VERSION,
	.revision	= 0,
	.size		= IPT_ALIGN(sizeof(struct ipt_mark_target_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_mark_target_info)),
	.help		= &help,
	.init		= &init,
	.parse		= &parse_v0,
	.final_check	= &final_check,
	.print		= &print_v0,
	.save		= &save_v0,
	.extra_opts	= opts
};

static
struct iptables_target mark_v1 = {
	.next		= NULL,
	.name		= "MARK",
	.version	= IPTABLES_VERSION,
	.revision	= 1,
	.size		= IPT_ALIGN(sizeof(struct ipt_mark_target_info_v1)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_mark_target_info_v1)),
	.help		= &help,
	.init		= &init,
	.parse		= &parse_v1,
	.final_check	= &final_check,
	.print		= &print_v1,
	.save		= &save_v1,
	.extra_opts	= opts
};

void _init(void)
{
	register_target(&mark_v0);
	register_target(&mark_v1);
}
