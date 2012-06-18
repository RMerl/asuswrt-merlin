/* Shared library add-on to iptables to add IP range matching support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_iprange.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"iprange match v%s options:\n"
"[!] --src-range ip-ip        Match source IP in the specified range\n"
"[!] --dst-range ip-ip        Match destination IP in the specified range\n"
"\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "src-range", 1, 0, '1' },
	{ "dst-range", 1, 0, '2' },
	{0}
};

static void
parse_iprange(char *arg, struct ipt_iprange *range)
{
	char *dash;
	struct in_addr *ip;

	dash = strchr(arg, '-');
	if (dash)
		*dash = '\0';
		
	ip = dotted_to_addr(arg);
	if (!ip)
		exit_error(PARAMETER_PROBLEM, "iprange match: Bad IP address `%s'\n", 
			   arg);
	range->min_ip = ip->s_addr;

	if (dash) {
		ip = dotted_to_addr(dash+1);
		if (!ip)
			exit_error(PARAMETER_PROBLEM, "iprange match: Bad IP address `%s'\n",
				   dash+1);
		range->max_ip = ip->s_addr;
	} else
		range->max_ip = range->min_ip;
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_iprange_info *info = (struct ipt_iprange_info *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & IPRANGE_SRC)
			exit_error(PARAMETER_PROBLEM,
				   "iprange match: Only use --src-range ONCE!");
		*flags |= IPRANGE_SRC;

		info->flags |= IPRANGE_SRC;
		check_inverse(optarg, &invert, &optind, 0);
		if (invert) {
			info->flags |= IPRANGE_SRC_INV;
		}
		parse_iprange(optarg, &info->src);		

		break;

	case '2':
		if (*flags & IPRANGE_DST)
			exit_error(PARAMETER_PROBLEM,
				   "iprange match: Only use --dst-range ONCE!");
		*flags |= IPRANGE_DST;

		info->flags |= IPRANGE_DST;
		check_inverse(optarg, &invert, &optind, 0);
		if (invert)
			info->flags |= IPRANGE_DST_INV;

		parse_iprange(optarg, &info->dst);		

		break;

	default:
		return 0;
	}
	return 1;
}

/* Final check; must have specified --src-range or --dst-range. */
static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "iprange match: You must specify `--src-range' or `--dst-range'");
}

static void
print_iprange(const struct ipt_iprange *range)
{
	const unsigned char *byte_min, *byte_max;

	byte_min = (const unsigned char *) &(range->min_ip);
	byte_max = (const unsigned char *) &(range->max_ip);
	printf("%d.%d.%d.%d-%d.%d.%d.%d ", 
		byte_min[0], byte_min[1], byte_min[2], byte_min[3],
		byte_max[0], byte_max[1], byte_max[2], byte_max[3]);
}

/* Prints out the info. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	struct ipt_iprange_info *info = (struct ipt_iprange_info *)match->data;

	if (info->flags & IPRANGE_SRC) {
		printf("source IP range ");
		if (info->flags & IPRANGE_SRC_INV)
			printf("! ");
		print_iprange(&info->src);
	}
	if (info->flags & IPRANGE_DST) {
		printf("destination IP range ");
		if (info->flags & IPRANGE_DST_INV)
			printf("! ");
		print_iprange(&info->dst);
	}
}

/* Saves the union ipt_info in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	struct ipt_iprange_info *info = (struct ipt_iprange_info *)match->data;

	if (info->flags & IPRANGE_SRC) {
		if (info->flags & IPRANGE_SRC_INV)
			printf("! ");
		printf("--src-range ");
		print_iprange(&info->src);
		if (info->flags & IPRANGE_DST)
			fputc(' ', stdout);
	}
	if (info->flags & IPRANGE_DST) {
		if (info->flags & IPRANGE_DST_INV)
			printf("! ");
		printf("--dst-range ");
		print_iprange(&info->dst);
	}
}

static struct iptables_match iprange = { 
	.next		= NULL,
	.name		= "iprange",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_iprange_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_iprange_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match(&iprange);
}
