/* Provides a NETLINK target, identical to that of the ipchains -o flag */
/* AUTHOR: Gianni Tedesco <gianni@ecsc.co.uk> */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_NETLINK.h>

static void help(void)
{
	printf("NETLINK v%s options:\n"
		" --nldrop		Drop the packet too\n"
		" --nlmark <number>	Mark the packet\n"
		" --nlsize <bytes>	Limit packet size\n",
	       IPTABLES_VERSION);
}

static struct option opts[] = {
	{"nldrop", 0, 0, 'd'},
	{"nlmark", 1, 0, 'm'},
	{"nlsize", 1, 0, 's'},
	{0}
};

static void init(struct ipt_entry_target *t, unsigned int *nfcache)
{
	struct ipt_nldata *nld = (struct ipt_nldata *) t->data;
	
	nld->flags=0;
	
}

/* Parse command options */
static int parse(int c, char **argv, int invert, unsigned int *flags,
		 const struct ipt_entry *entry,
		 struct ipt_entry_target **target)
{
	struct ipt_nldata *nld=(struct ipt_nldata *)(*target)->data;

	switch (c) {
		case 'd':
			if (MASK(*flags, USE_DROP))
				exit_error(PARAMETER_PROBLEM,
				"Can't specify --nldrop twice");

			if ( check_inverse(optarg, &invert, NULL, 0) ) {
				MASK_UNSET(nld->flags, USE_DROP);
			} else {
				MASK_SET(nld->flags, USE_DROP);
			}

			MASK_SET(*flags, USE_DROP);			

			break;
		case 'm':
			if (MASK(*flags, USE_MARK))
				exit_error(PARAMETER_PROBLEM,
				"Can't specify --nlmark twice");

			if (check_inverse(optarg, &invert, NULL, 0)) {
				MASK_UNSET(nld->flags, USE_MARK);
			}else{
				MASK_SET(nld->flags, USE_MARK);
				nld->mark=atoi(optarg);
			}

			MASK_SET(*flags, USE_MARK);
			break;
		case 's':
			if (MASK(*flags, USE_SIZE))
				exit_error(PARAMETER_PROBLEM,
				"Can't specify --nlsize twice");

			if ( atoi(optarg) <= 0 )
				exit_error(PARAMETER_PROBLEM,
				"--nlsize must be larger than zero");
			

			if (check_inverse(optarg, &invert, NULL, 0)) {
				MASK_UNSET(nld->flags, USE_SIZE);
			}else{
				MASK_SET(nld->flags, USE_SIZE);
				nld->size=atoi(optarg);
			}
			MASK_SET(*flags, USE_SIZE);
			break;

		default:
			return 0;
	}
	return 1;
}

static void final_check(unsigned int flags)
{
	/* ?? */
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void save(const struct ipt_ip *ip,
		 const struct ipt_entry_target *target)
{
	const struct ipt_nldata *nld
	    = (const struct ipt_nldata *) target->data;

	if ( MASK(nld->flags, USE_DROP) )
		printf("--nldrop ");

	if ( MASK(nld->flags, USE_MARK) )
		printf("--nlmark %i ", nld->mark);

	if ( MASK(nld->flags, USE_SIZE) )
		printf("--nlsize %i ", nld->size);		
}

/* Prints out the targinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target, int numeric)
{
	const struct ipt_nldata *nld
	    = (const struct ipt_nldata *) target->data;

	if ( MASK(nld->flags, USE_DROP) )
		printf("nldrop ");

	if ( MASK(nld->flags, USE_MARK) )
		printf("nlmark %i ", nld->mark);

	if ( MASK(nld->flags, USE_SIZE) )
		printf("nlsize %i ", nld->size);
}

static struct iptables_target netlink = {
	.next		= NULL,
	.name		= "NETLINK",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_nldata)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_nldata)),
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
	register_target(&netlink);
}

