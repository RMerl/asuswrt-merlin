/* Shared library add-on to iptables to add CLASSIFY target support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_CLASSIFY.h>
#include <linux/types.h>
#include <linux/pkt_sched.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"CLASSIFY target v%s options:\n"
"  --set-class [MAJOR:MINOR]    Set skb->priority value\n"
"\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "set-class", 1, 0, '1' },
	{ 0 }
};

/* Initialize the target. */
static void
init(struct ipt_entry_target *t, unsigned int *nfcache)
{
}

int string_to_priority(const char *s, unsigned int *p)
{
	unsigned int i, j;

	if (sscanf(s, "%x:%x", &i, &j) != 2)
		return 1;
	
	*p = TC_H_MAKE(i<<16, j);
	return 0;
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	struct ipt_classify_target_info *clinfo
		= (struct ipt_classify_target_info *)(*target)->data;

	switch (c) {
	case '1':
		if (string_to_priority(optarg, &clinfo->priority))
			exit_error(PARAMETER_PROBLEM,
				   "Bad class value `%s'", optarg);
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "CLASSIFY: Can't specify --set-class twice");
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
		           "CLASSIFY: Parameter --set-class is required");
}

static void
print_class(unsigned int priority, int numeric)
{
	printf("%x:%x ", TC_H_MAJ(priority)>>16, TC_H_MIN(priority));
}

/* Prints out the targinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
      int numeric)
{
	const struct ipt_classify_target_info *clinfo =
		(const struct ipt_classify_target_info *)target->data;
	printf("CLASSIFY set ");
	print_class(clinfo->priority, numeric);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	const struct ipt_classify_target_info *clinfo =
		(const struct ipt_classify_target_info *)target->data;

	printf("--set-class %.4x:%.4x ",
	       TC_H_MAJ(clinfo->priority)>>16, TC_H_MIN(clinfo->priority));
}

static struct iptables_target classify = { 
	.next		= NULL,
	.name		= "CLASSIFY",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_classify_target_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_classify_target_info)),
	.help		= &help,
	.init		= &init,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_target(&classify);
}
