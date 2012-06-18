/* Shared library add-on to iptables to add related packet matching support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_helper.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"helper match v%s options:\n"
"[!] --helper string        Match helper identified by string\n"
"\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "helper", 1, 0, '1' },
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
	struct ipt_helper_info *info = (struct ipt_helper_info *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
					"helper match: Only use --helper ONCE!");
		check_inverse(optarg, &invert, &invert, 0);
		strncpy(info->name, optarg, 29);
		info->name[29] = '\0';
		if (invert)
			info->invert = 1;
		*flags = 1;
		break;

	default:
		return 0;
	}
	return 1;
}

/* Final check; must have specified --helper. */
static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "helper match: You must specify `--helper'");
}

/* Prints out the info. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	struct ipt_helper_info *info = (struct ipt_helper_info *)match->data;

	printf("helper match %s\"%s\" ", info->invert ? "! " : "", info->name);
}

/* Saves the union ipt_info in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	struct ipt_helper_info *info = (struct ipt_helper_info *)match->data;

	printf("%s--helper \"%s\" ",info->invert ? "! " : "", info->name);
}

static struct iptables_match helper = { 
	.next		= NULL,
	.name		= "helper",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_helper_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match(&helper);
}
