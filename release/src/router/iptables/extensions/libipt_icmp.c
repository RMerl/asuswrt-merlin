/* Shared library add-on to iptables to add ICMP support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>

/* special hack for icmp-type 'any': 
 * Up to kernel <=2.4.20 the problem was:
 * '-p icmp ' matches all icmp packets
 * '-p icmp -m icmp' matches _only_ ICMP type 0 :(
 * This is now fixed by initializing the field * to icmp type 0xFF
 * See: https://bugzilla.netfilter.org/cgi-bin/bugzilla/show_bug.cgi?id=37
 */

struct icmp_names {
	const char *name;
	u_int8_t type;
	u_int8_t code_min, code_max;
};

static const struct icmp_names icmp_codes[] = {
	{ "any", 0xFF, 0, 0xFF },
	{ "echo-reply", 0, 0, 0xFF },
	/* Alias */ { "pong", 0, 0, 0xFF },

	{ "destination-unreachable", 3, 0, 0xFF },
	{   "network-unreachable", 3, 0, 0 },
	{   "host-unreachable", 3, 1, 1 },
	{   "protocol-unreachable", 3, 2, 2 },
	{   "port-unreachable", 3, 3, 3 },
	{   "fragmentation-needed", 3, 4, 4 },
	{   "source-route-failed", 3, 5, 5 },
	{   "network-unknown", 3, 6, 6 },
	{   "host-unknown", 3, 7, 7 },
	{   "network-prohibited", 3, 9, 9 },
	{   "host-prohibited", 3, 10, 10 },
	{   "TOS-network-unreachable", 3, 11, 11 },
	{   "TOS-host-unreachable", 3, 12, 12 },
	{   "communication-prohibited", 3, 13, 13 },
	{   "host-precedence-violation", 3, 14, 14 },
	{   "precedence-cutoff", 3, 15, 15 },

	{ "source-quench", 4, 0, 0xFF },

	{ "redirect", 5, 0, 0xFF },
	{   "network-redirect", 5, 0, 0 },
	{   "host-redirect", 5, 1, 1 },
	{   "TOS-network-redirect", 5, 2, 2 },
	{   "TOS-host-redirect", 5, 3, 3 },

	{ "echo-request", 8, 0, 0xFF },
	/* Alias */ { "ping", 8, 0, 0xFF },

	{ "router-advertisement", 9, 0, 0xFF },

	{ "router-solicitation", 10, 0, 0xFF },

	{ "time-exceeded", 11, 0, 0xFF },
	/* Alias */ { "ttl-exceeded", 11, 0, 0xFF },
	{   "ttl-zero-during-transit", 11, 0, 0 },
	{   "ttl-zero-during-reassembly", 11, 1, 1 },

	{ "parameter-problem", 12, 0, 0xFF },
	{   "ip-header-bad", 12, 0, 0 },
	{   "required-option-missing", 12, 1, 1 },

	{ "timestamp-request", 13, 0, 0xFF },

	{ "timestamp-reply", 14, 0, 0xFF },

	{ "address-mask-request", 17, 0, 0xFF },

	{ "address-mask-reply", 18, 0, 0xFF }
};

static void
print_icmptypes()
{
	unsigned int i;
	printf("Valid ICMP Types:");

	for (i = 0; i < sizeof(icmp_codes)/sizeof(struct icmp_names); i++) {
		if (i && icmp_codes[i].type == icmp_codes[i-1].type) {
			if (icmp_codes[i].code_min == icmp_codes[i-1].code_min
			    && (icmp_codes[i].code_max
				== icmp_codes[i-1].code_max))
				printf(" (%s)", icmp_codes[i].name);
			else
				printf("\n   %s", icmp_codes[i].name);
		}
		else
			printf("\n%s", icmp_codes[i].name);
	}
	printf("\n");
}

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"ICMP v%s options:\n"
" --icmp-type [!] typename	match icmp type\n"
"				(or numeric type or type/code)\n"
"\n", IPTABLES_VERSION);
	print_icmptypes();
}

static struct option opts[] = {
	{ "icmp-type", 1, 0, '1' },
	{0}
};

static void 
parse_icmp(const char *icmptype, u_int8_t *type, u_int8_t code[])
{
	unsigned int limit = sizeof(icmp_codes)/sizeof(struct icmp_names);
	unsigned int match = limit;
	unsigned int i;

	for (i = 0; i < limit; i++) {
		if (strncasecmp(icmp_codes[i].name, icmptype, strlen(icmptype))
		    == 0) {
			if (match != limit)
				exit_error(PARAMETER_PROBLEM,
					   "Ambiguous ICMP type `%s':"
					   " `%s' or `%s'?",
					   icmptype,
					   icmp_codes[match].name,
					   icmp_codes[i].name);
			match = i;
		}
	}

	if (match != limit) {
		*type = icmp_codes[match].type;
		code[0] = icmp_codes[match].code_min;
		code[1] = icmp_codes[match].code_max;
	} else {
		char *slash;
		char buffer[strlen(icmptype) + 1];
		unsigned int number;

		strcpy(buffer, icmptype);
		slash = strchr(buffer, '/');

		if (slash)
			*slash = '\0';

		if (string_to_number(buffer, 0, 255, &number) == -1)
			exit_error(PARAMETER_PROBLEM,
				   "Invalid ICMP type `%s'\n", buffer);
		*type = number;
		if (slash) {
			if (string_to_number(slash+1, 0, 255, &number) == -1)
				exit_error(PARAMETER_PROBLEM,
					   "Invalid ICMP code `%s'\n",
					   slash+1);
			code[0] = code[1] = number;
		} else {
			code[0] = 0;
			code[1] = 0xFF;
		}
	}
}

/* Initialize the match. */
static void
init(struct ipt_entry_match *m, unsigned int *nfcache)
{
	struct ipt_icmp *icmpinfo = (struct ipt_icmp *)m->data;

	icmpinfo->type = 0xFF;
	icmpinfo->code[1] = 0xFF;
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_icmp *icmpinfo = (struct ipt_icmp *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags == 1)
			exit_error(PARAMETER_PROBLEM,
				   "icmp match: only use --icmp-type once!");
		check_inverse(optarg, &invert, &optind, 0);
		parse_icmp(argv[optind-1], &icmpinfo->type, 
			   icmpinfo->code);
		if (invert)
			icmpinfo->invflags |= IPT_ICMP_INV;
		*flags = 1;
		break;

	default:
		return 0;
	}

	return 1;
}

static void print_icmptype(u_int8_t type,
			   u_int8_t code_min, u_int8_t code_max,
			   int invert,
			   int numeric)
{
	if (!numeric) {
		unsigned int i;

		for (i = 0;
		     i < sizeof(icmp_codes)/sizeof(struct icmp_names);
		     i++) {
			if (icmp_codes[i].type == type
			    && icmp_codes[i].code_min == code_min
			    && icmp_codes[i].code_max == code_max)
				break;
		}

		if (i != sizeof(icmp_codes)/sizeof(struct icmp_names)) {
			printf("%s%s ",
			       invert ? "!" : "",
			       icmp_codes[i].name);
			return;
		}
	}

	if (invert)
		printf("!");

	printf("type %u", type);
	if (code_min == 0 && code_max == 0xFF)
		printf(" ");
	else if (code_min == code_max)
		printf(" code %u ", code_min);
	else
		printf(" codes %u-%u ", code_min, code_max);
}

/* Prints out the union ipt_matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	const struct ipt_icmp *icmp = (struct ipt_icmp *)match->data;

	printf("icmp ");
	print_icmptype(icmp->type, icmp->code[0], icmp->code[1],
		       icmp->invflags & IPT_ICMP_INV,
		       numeric);

	if (icmp->invflags & ~IPT_ICMP_INV)
		printf("Unknown invflags: 0x%X ",
		       icmp->invflags & ~IPT_ICMP_INV);
}

/* Saves the match in parsable form to stdout. */
static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	const struct ipt_icmp *icmp = (struct ipt_icmp *)match->data;

	if (icmp->invflags & IPT_ICMP_INV)
		printf("! ");

	/* special hack for 'any' case */
	if (icmp->type == 0xFF) {
		printf("--icmp-type any ");
	} else {
		printf("--icmp-type %u", icmp->type);
		if (icmp->code[0] != 0 || icmp->code[1] != 0xFF)
			printf("/%u", icmp->code[0]);
		printf(" ");
	}
}

/* Final check; we don't care. We can pass 0xFF to match any type */
static void final_check(unsigned int flags)
{
}

static struct iptables_match icmp = { 
	.next		= NULL,
	.name		= "icmp",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_icmp)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_icmp)),
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
	register_match(&icmp);
}
