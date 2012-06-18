/* Shared library add-on to iptables to add TRACE target support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"TRACE target v%s takes no options\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ 0 }
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
	return 0;
}

static void
final_check(unsigned int flags)
{
}

static
struct ip6tables_target trace
= {	.next = NULL,
	.name = "TRACE",
	.version = IPTABLES_VERSION,
	.size = IP6T_ALIGN(0),
	.userspacesize = IP6T_ALIGN(0),
	.help = &help,
	.init = &init,
	.parse = &parse,
	.final_check = &final_check,
	.print = NULL, /* print */
	.save = NULL, /* save */
	.extra_opts = opts
};

void _init(void)
{
	register_target6(&trace);
}
