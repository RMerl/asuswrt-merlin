/* Shared library add-on to iptables to add policy support. */

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <getopt.h>
#include <netdb.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ip6tables.h>

#include <linux/netfilter_ipv6/ip6_tables.h>
#include "../include/linux/netfilter_ipv6/ip6t_policy.h"

/*
 * HACK: global pointer to current matchinfo for making
 * final checks and adjustments in final_check.
 */
static struct ip6t_policy_info *policy_info;

static void help(void)
{
	printf(
"policy v%s options:\n"
"  --dir in|out			match policy applied during decapsulation/\n"
"				policy to be applied during encapsulation\n"
"  --pol none|ipsec		match policy\n"
"  --strict 			match entire policy instead of single element\n"
"				at any position\n"
"[!] --reqid reqid		match reqid\n"
"[!] --spi spi			match SPI\n"
"[!] --proto proto		match protocol (ah/esp/ipcomp)\n"
"[!] --mode mode 		match mode (transport/tunnel)\n"
"[!] --tunnel-src addr/masklen	match tunnel source\n"
"[!] --tunnel-dst addr/masklen	match tunnel destination\n"
"  --next 			begin next element in policy\n",
	IPTABLES_VERSION);
}

static struct option opts[] =
{
	{
		.name		= "dir",
		.has_arg	= 1,
		.val		= '1',
	},
	{
		.name		= "pol",
		.has_arg	= 1,
		.val		= '2',
	},
	{
		.name		= "strict",
		.val		= '3'
	},
	{
		.name		= "reqid",
		.has_arg	= 1,
		.val		= '4',
	},
	{
		.name		= "spi",
		.has_arg	= 1,
		.val		= '5'
	},
	{
		.name		= "tunnel-src",
		.has_arg	= 1,
		.val		= '6'
	},
	{
		.name		= "tunnel-dst",
		.has_arg	= 1,
		.val		= '7'
	},
	{
		.name		= "proto",
		.has_arg	= 1,
		.val		= '8'
	},
	{
		.name		= "mode",
		.has_arg	= 1,
		.val		= '9'
	},
	{
		.name		= "next",
		.val		= 'a'
	},
	{ }
};

/* FIXME - Duplicated code from ip6tables.c */
/* Duplicated to stop too many changes in other files .... */
static void
in6addrcpy(struct in6_addr *dst, struct in6_addr *src)
{
        memcpy(dst, src, sizeof(struct in6_addr));
        /* dst->s6_addr = src->s6_addr; */
}

static char *
addr_to_numeric(const struct in6_addr *addrp)
{
        /* 0000:0000:0000:0000:0000:000.000.000.000
	 * 0000:0000:0000:0000:0000:0000:0000:0000 */
        static char buf[50+1];
        return (char *)inet_ntop(AF_INET6, addrp, buf, sizeof(buf));
}

static char *
mask_to_numeric(const struct in6_addr *addrp)
{
        static char buf[50+2];
        int l = ipv6_prefix_length(addrp);
        if (l == -1) {
		strcpy(buf, "/");
		strcat(buf, addr_to_numeric(addrp));
		return buf;
	}
	sprintf(buf, "/%d", l);
	return buf;
}

/* These should be in include/ip6tables.h... */
extern u_int16_t parse_protocol(const char *s);
extern void parse_hostnetworkmask(const char *name, struct in6_addr **addrpp,
		struct in6_addr *maskp, unsigned int *naddrs);

/* End duplicated code from ip6tables.c */

static void init(struct ip6t_entry_match *m, unsigned int *nfcache)
{
	*nfcache |= NFC_UNKNOWN;
}

static int parse_direction(char *s)
{
	if (strcmp(s, "in") == 0)
		return IP6T_POLICY_MATCH_IN;
	if (strcmp(s, "out") == 0)
		return IP6T_POLICY_MATCH_OUT;
	exit_error(PARAMETER_PROBLEM, "policy_match: invalid dir `%s'", s);
}

static int parse_policy(char *s)
{
	if (strcmp(s, "none") == 0)
		return IP6T_POLICY_MATCH_NONE;
	if (strcmp(s, "ipsec") == 0)
		return 0;
	exit_error(PARAMETER_PROBLEM, "policy match: invalid policy `%s'", s);
}

static int parse_mode(char *s)
{
	if (strcmp(s, "transport") == 0)
		return IP6T_POLICY_MODE_TRANSPORT;
	if (strcmp(s, "tunnel") == 0)
		return IP6T_POLICY_MODE_TUNNEL;
	exit_error(PARAMETER_PROBLEM, "policy match: invalid mode `%s'", s);
}

static int parse(int c, char **argv, int invert, unsigned int *flags,
                 const struct ip6t_entry *entry,
                 unsigned int *nfcache,
                 struct ip6t_entry_match **match)
{
	struct ip6t_policy_info *info = (void *)(*match)->data;
	struct ip6t_policy_elem *e = &info->pol[info->len];
	struct in6_addr *addr = NULL, mask;
	unsigned int naddr = 0;
	int mode;

	check_inverse(optarg, &invert, &optind, 0);

	switch (c) {
	case '1':
		if (info->flags & (IP6T_POLICY_MATCH_IN|IP6T_POLICY_MATCH_OUT))
			exit_error(PARAMETER_PROBLEM,
			           "policy match: double --dir option");
		if (invert)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: can't invert --dir option");

		info->flags |= parse_direction(argv[optind-1]);
		break;
	case '2':
		if (invert)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: can't invert --policy option");

		info->flags |= parse_policy(argv[optind-1]);
		break;
	case '3':
		if (info->flags & IP6T_POLICY_MATCH_STRICT)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: double --strict option");

		if (invert)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: can't invert --strict option");

		info->flags |= IP6T_POLICY_MATCH_STRICT;
		break;
	case '4':
		if (e->match.reqid)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: double --reqid option");

		e->match.reqid = 1;
		e->invert.reqid = invert;
		e->reqid = strtol(argv[optind-1], NULL, 10);
		break;
	case '5':
		if (e->match.spi)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: double --spi option");

		e->match.spi = 1;
		e->invert.spi = invert;
		e->spi = strtol(argv[optind-1], NULL, 0x10);
		break;
	case '6':
		if (e->match.saddr)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: double --tunnel-src option");

		parse_hostnetworkmask(argv[optind-1], &addr, &mask, &naddr);
		if (naddr > 1)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: name resolves to multiple IPs");

		e->match.saddr = 1;
		e->invert.saddr = invert;
		in6addrcpy(&e->saddr.a6, addr);
		in6addrcpy(&e->smask.a6, &mask);
                break;
	case '7':
		if (e->match.daddr)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: double --tunnel-dst option");

		parse_hostnetworkmask(argv[optind-1], &addr, &mask, &naddr);
		if (naddr > 1)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: name resolves to multiple IPs");

		e->match.daddr = 1;
		e->invert.daddr = invert;
		in6addrcpy(&e->daddr.a6, addr);
		in6addrcpy(&e->dmask.a6, &mask);
		break;
	case '8':
		if (e->match.proto)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: double --proto option");

		e->proto = parse_protocol(argv[optind-1]);
		if (e->proto != IPPROTO_AH && e->proto != IPPROTO_ESP &&
		    e->proto != IPPROTO_COMP)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: protocol must ah/esp/ipcomp");
		e->match.proto = 1;
		e->invert.proto = invert;
		break;
	case '9':
		if (e->match.mode)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: double --mode option");

		mode = parse_mode(argv[optind-1]);
		e->match.mode = 1;
		e->invert.mode = invert;
		e->mode = mode;
		break;
	case 'a':
		if (invert)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: can't invert --next option");

		if (++info->len == IP6T_POLICY_MAX_ELEM)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: maximum policy depth reached");
		break;
	default:
		return 0;
	}

	policy_info = info;
	return 1;
}

static void final_check(unsigned int flags)
{
	struct ip6t_policy_info *info = policy_info;
	struct ip6t_policy_elem *e;
	int i;

	if (info == NULL)
		exit_error(PARAMETER_PROBLEM,
		           "policy match: no parameters given");

	if (!(info->flags & (IP6T_POLICY_MATCH_IN|IP6T_POLICY_MATCH_OUT)))
		exit_error(PARAMETER_PROBLEM,
		           "policy match: neither --in nor --out specified");

	if (info->flags & IP6T_POLICY_MATCH_NONE) {
		if (info->flags & IP6T_POLICY_MATCH_STRICT)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: policy none but --strict given");

		if (info->len != 0)
			exit_error(PARAMETER_PROBLEM,
			           "policy match: policy none but policy given");
	} else
		info->len++;	/* increase len by 1, no --next after last element */

	if (!(info->flags & IP6T_POLICY_MATCH_STRICT) && info->len > 1)
		exit_error(PARAMETER_PROBLEM,
		           "policy match: multiple elements but no --strict");

	for (i = 0; i < info->len; i++) {
		e = &info->pol[i];

                if (info->flags & IP6T_POLICY_MATCH_STRICT &&
		    !(e->match.reqid || e->match.spi || e->match.saddr ||
                      e->match.daddr || e->match.proto || e->match.mode))
                        exit_error(PARAMETER_PROBLEM,
                                   "policy match: empty policy element");

		if ((e->match.saddr || e->match.daddr)
		    && ((e->mode == IP6T_POLICY_MODE_TUNNEL && e->invert.mode) ||
		        (e->mode == IP6T_POLICY_MODE_TRANSPORT && !e->invert.mode)))
			exit_error(PARAMETER_PROBLEM,
			           "policy match: --tunnel-src/--tunnel-dst "
			           "is only valid in tunnel mode");
	}
}

static void print_mode(char *prefix, u_int8_t mode, int numeric)
{
	printf("%smode ", prefix);

	switch (mode) {
	case IP6T_POLICY_MODE_TRANSPORT:
		printf("transport ");
		break;
	case IP6T_POLICY_MODE_TUNNEL:
		printf("tunnel ");
		break;
	default:
		printf("??? ");
		break;
	}
}

static void print_proto(char *prefix, u_int8_t proto, int numeric)
{
	struct protoent *p = NULL;

	printf("%sproto ", prefix);
	if (!numeric)
		p = getprotobynumber(proto);
	if (p != NULL)
		printf("%s ", p->p_name);
	else
		printf("%u ", proto);
}

#define PRINT_INVERT(x)		\
do {				\
	if (x)			\
		printf("! ");	\
} while(0)

static void print_entry(char *prefix, const struct ip6t_policy_elem *e,
                        int numeric)
{
	if (e->match.reqid) {
		PRINT_INVERT(e->invert.reqid);
		printf("%sreqid %u ", prefix, e->reqid);
	}
	if (e->match.spi) {
		PRINT_INVERT(e->invert.spi);
		printf("%sspi 0x%x ", prefix, e->spi);
	}
	if (e->match.proto) {
		PRINT_INVERT(e->invert.proto);
		print_proto(prefix, e->proto, numeric);
	}
	if (e->match.mode) {
		PRINT_INVERT(e->invert.mode);
		print_mode(prefix, e->mode, numeric);
	}
	if (e->match.daddr) {
		PRINT_INVERT(e->invert.daddr);
		printf("%stunnel-dst %s%s ", prefix,
		       addr_to_numeric((struct in6_addr *)&e->daddr),
		       mask_to_numeric((struct in6_addr *)&e->dmask));
	}
	if (e->match.saddr) {
		PRINT_INVERT(e->invert.saddr);
		printf("%stunnel-src %s%s ", prefix,
		       addr_to_numeric((struct in6_addr *)&e->saddr),
		       mask_to_numeric((struct in6_addr *)&e->smask));
	}
}

static void print_flags(char *prefix, const struct ip6t_policy_info *info)
{
	if (info->flags & IP6T_POLICY_MATCH_IN)
		printf("%sdir in ", prefix);
	else
		printf("%sdir out ", prefix);

	if (info->flags & IP6T_POLICY_MATCH_NONE)
		printf("%spol none ", prefix);
	else
		printf("%spol ipsec ", prefix);

	if (info->flags & IP6T_POLICY_MATCH_STRICT)
		printf("%sstrict ", prefix);
}

static void print(const struct ip6t_ip6 *ip,
                  const struct ip6t_entry_match *match,
		  int numeric)
{
	const struct ip6t_policy_info *info = (void *)match->data;
	unsigned int i;

	printf("policy match ");
	print_flags("", info);
	for (i = 0; i < info->len; i++) {
		if (info->len > 1)
			printf("[%u] ", i);
		print_entry("", &info->pol[i], numeric);
	}

	printf("\n");
}

static void save(const struct ip6t_ip6 *ip, const struct ip6t_entry_match *match)
{
	const struct ip6t_policy_info *info = (void *)match->data;
	unsigned int i;

	print_flags("--", info);
	for (i = 0; i < info->len; i++) {
		print_entry("--", &info->pol[i], 0);
		if (i + 1 < info->len)
			printf("--next ");
	}
}

struct ip6tables_match policy = {
	.name		= "policy",
	.version	= IPTABLES_VERSION,
	.size		= IP6T_ALIGN(sizeof(struct ip6t_policy_info)),
	.userspacesize	= IP6T_ALIGN(sizeof(struct ip6t_policy_info)),
	.help		= help,
	.init		= init,
	.parse		= parse,
	.final_check	= final_check,
	.print		= print,
	.save		= save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match6(&policy);
}
