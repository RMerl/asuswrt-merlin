/* Shared library add-on to iptables to add source-NAT support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter/nf_nat.h>

#define IPT_SNAT_OPT_SOURCE 0x01
#define IPT_SNAT_OPT_RANDOM 0x02

/* Source NAT data consists of a multi-range, indicating where to map
   to. */
struct ipt_natinfo
{
	struct ipt_entry_target t;
	struct ip_nat_multi_range mr;
};

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"SNAT v%s options:\n"
" --to-source <ipaddr>[-<ipaddr>][:port-port]\n"
"				Address to map source to.\n"
"[--random]\n"
"\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "to-source", 1, 0, '1' },
	{ "random", 0, 0, '2' },
	{ 0 }
};

static struct ipt_natinfo *
append_range(struct ipt_natinfo *info, const struct ip_nat_range *range)
{
	unsigned int size;

	/* One rangesize already in struct ipt_natinfo */
	size = IPT_ALIGN(sizeof(*info) + info->mr.rangesize * sizeof(*range));

	info = realloc(info, size);
	if (!info)
		exit_error(OTHER_PROBLEM, "Out of memory\n");

	info->t.u.target_size = size;
	info->mr.range[info->mr.rangesize] = *range;
	info->mr.rangesize++;

	return info;
}

/* Ranges expected in network order. */
static struct ipt_entry_target *
parse_to(char *arg, int portok, struct ipt_natinfo *info)
{
	struct ip_nat_range range;
	char *colon, *dash, *error;
	struct in_addr *ip;

	memset(&range, 0, sizeof(range));
	colon = strchr(arg, ':');

	if (colon) {
		int port;

		if (!portok)
			exit_error(PARAMETER_PROBLEM,
				   "Need TCP or UDP with port specification");

		range.flags |= IP_NAT_RANGE_PROTO_SPECIFIED;

		port = atoi(colon+1);
		if (port <= 0 || port > 65535)
			exit_error(PARAMETER_PROBLEM,
				   "Port `%s' not valid\n", colon+1);

		error = strchr(colon+1, ':');
		if (error)
			exit_error(PARAMETER_PROBLEM,
				   "Invalid port:port syntax - use dash\n");

		dash = strchr(colon, '-');
		if (!dash) {
			range.min.tcp.port
				= range.max.tcp.port
				= htons(port);
		} else {
			int maxport;

			maxport = atoi(dash + 1);
			if (maxport <= 0 || maxport > 65535)
				exit_error(PARAMETER_PROBLEM,
					   "Port `%s' not valid\n", dash+1);
			if (maxport < port)
				/* People are stupid. */
				exit_error(PARAMETER_PROBLEM,
					   "Port range `%s' funky\n", colon+1);
			range.min.tcp.port = htons(port);
			range.max.tcp.port = htons(maxport);
		}
		/* Starts with a colon? No IP info...*/
		if (colon == arg)
			return &(append_range(info, &range)->t);
		*colon = '\0';
	}

	range.flags |= IP_NAT_RANGE_MAP_IPS;
	dash = strchr(arg, '-');
	if (colon && dash && dash > colon)
		dash = NULL;

	if (dash)
		*dash = '\0';

	ip = dotted_to_addr(arg);
	if (!ip)
		exit_error(PARAMETER_PROBLEM, "Bad IP address `%s'\n",
			   arg);
	range.min_ip = ip->s_addr;
	if (dash) {
		ip = dotted_to_addr(dash+1);
		if (!ip)
			exit_error(PARAMETER_PROBLEM, "Bad IP address `%s'\n",
				   dash+1);
		range.max_ip = ip->s_addr;
	} else
		range.max_ip = range.min_ip;

	return &(append_range(info, &range)->t);
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	struct ipt_natinfo *info = (void *)*target;
	int portok;

	if (entry->ip.proto == IPPROTO_TCP
	    || entry->ip.proto == IPPROTO_UDP
	    || entry->ip.proto == IPPROTO_ICMP)
		portok = 1;
	else
		portok = 0;

	switch (c) {
	case '1':
		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --to-source");

		if (*flags & IPT_SNAT_OPT_SOURCE) {
			if (!kernel_version)
				get_kernel_version();
			if (kernel_version > LINUX_VERSION(2, 6, 10))
				exit_error(PARAMETER_PROBLEM,
					   "Multiple --to-source not supported");
		}
		*target = parse_to(optarg, portok, info);
		/* WTF do we need this for?? */
		if (*flags & IPT_SNAT_OPT_RANDOM)
			info->mr.range[0].flags |= IP_NAT_RANGE_PROTO_RANDOM;
		*flags |= IPT_SNAT_OPT_SOURCE;
		return 1;

	case '2':
		if (*flags & IPT_SNAT_OPT_SOURCE) {
			info->mr.range[0].flags |= IP_NAT_RANGE_PROTO_RANDOM;
			*flags |= IPT_SNAT_OPT_RANDOM;
		} else
			*flags |= IPT_SNAT_OPT_RANDOM;
		return 1;

	default:
		return 0;
	}
}

/* Final check; must have specfied --to-source. */
static void final_check(unsigned int flags)
{
	if (!(flags & IPT_SNAT_OPT_SOURCE))
		exit_error(PARAMETER_PROBLEM,
			   "You must specify --to-source");
}

static void print_range(const struct ip_nat_range *r)
{
	if (r->flags & IP_NAT_RANGE_MAP_IPS) {
		struct in_addr a;

		a.s_addr = r->min_ip;
		printf("%s", addr_to_dotted(&a));
		if (r->max_ip != r->min_ip) {
			a.s_addr = r->max_ip;
			printf("-%s", addr_to_dotted(&a));
		}
	}
	if (r->flags & IP_NAT_RANGE_PROTO_SPECIFIED) {
		printf(":");
		printf("%hu", ntohs(r->min.tcp.port));
		if (r->max.tcp.port != r->min.tcp.port)
			printf("-%hu", ntohs(r->max.tcp.port));
	}
}

/* Prints out the targinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
      int numeric)
{
	struct ipt_natinfo *info = (void *)target;
	unsigned int i = 0;

	printf("to:");
	for (i = 0; i < info->mr.rangesize; i++) {
		print_range(&info->mr.range[i]);
		printf(" ");
		if (info->mr.range[i].flags & IP_NAT_RANGE_PROTO_RANDOM)
			printf("random ");
	}
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	struct ipt_natinfo *info = (void *)target;
	unsigned int i = 0;

	for (i = 0; i < info->mr.rangesize; i++) {
		printf("--to-source ");
		print_range(&info->mr.range[i]);
		printf(" ");
		if (info->mr.range[i].flags & IP_NAT_RANGE_PROTO_RANDOM)
			printf("--random ");
	}
}

static struct iptables_target snat = {
	.next		= NULL,
	.name		= "SNAT",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ip_nat_multi_range)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ip_nat_multi_range)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_target(&snat);
}
