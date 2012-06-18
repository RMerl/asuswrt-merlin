/* Shared library add-on to iptables for IPV4OPTSSTRIP
 * This modules strip all the IP options.
 *
 * (C) 2001 by Fabrice MARIE <fabrice@netfilter.org>
 * This program is distributed under the terms of GNU GPL v2, 1991
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>

static void help(void) 
{
	printf("IPV4OPTSSTRIP v%s target takes no option !! Make sure you use it in the mangle table.\n",
	       IPTABLES_VERSION);
}

static struct option opts[] = {
	{ 0 }
};

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	return 0;
}

static void
final_check(unsigned int flags)
{
}

/* Prints out the targinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
      int numeric)
{
	/* nothing to print, we don't take option... */
}

/* Saves the stuff in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	/* nothing to print, we don't take option... */
}

static struct iptables_target IPV4OPTSSTRIP = { 
	.next		= NULL,
	.name		= "IPV4OPTSSTRIP",
	.version	= IPTABLES_VERSION,
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
	register_target(&IPV4OPTSSTRIP);
}
