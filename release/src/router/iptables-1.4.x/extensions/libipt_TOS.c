/* Shared library add-on to iptables to add TOS matching support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_tos.h>

/* TOS names and values. */
static
struct TOS_value
{
	unsigned char TOS;
	const char *name;
} TOS_values[] = {
	{ IPTOS_LOWDELAY,    "Minimize-Delay" },
	{ IPTOS_THROUGHPUT,  "Maximize-Throughput" },
	{ IPTOS_RELIABILITY, "Maximize-Reliability" },
	{ IPTOS_MINCOST,     "Minimize-Cost" },
	{ IPTOS_NORMALSVC,   "Normal-Service" },
};

/* Function which prints out usage message. */
static void
help(void)
{
	unsigned int i;

	printf(
"TOS match v%s options:\n"
"[!] --tos value                 Match Type of Service field from one of the\n"
"                                following numeric or descriptive values:\n",
XTABLES_VERSION);

	for (i = 0; i < sizeof(TOS_values)/sizeof(struct TOS_value);i++)
		printf("                                     %s %u (0x%02x)\n",
		       TOS_values[i].name,
                       TOS_values[i].TOS,
                       TOS_values[i].TOS);
	fputc('\n', stdout);
}

static struct option opts[] = {
	{ .name = "tos", .has_arg = true, .val = '1' },
	XT_GETOPT_TABLEEND
};

static void
parse_tos(const char *s, struct ipt_tos_info *info)
{
	unsigned int i;
	unsigned int tos;

	if (xtables_strtoui(s, NULL, &tos, 0, UINT8_MAX)) {
		if (tos == IPTOS_LOWDELAY
		    || tos == IPTOS_THROUGHPUT
		    || tos == IPTOS_RELIABILITY
		    || tos == IPTOS_MINCOST
		    || tos == IPTOS_NORMALSVC) {
		    	info->tos = (u_int8_t )tos;
		    	return;
		}
	} else {
		for (i = 0; i<sizeof(TOS_values)/sizeof(struct TOS_value); i++)
			if (strcasecmp(s,TOS_values[i].name) == 0) {
				info->tos = TOS_values[i].TOS;
				return;
			}
	}
	xtables_error(PARAMETER_PROBLEM, "Bad TOS value `%s'", s);
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const void *entry,
      struct xt_entry_match **match)
{
	struct ipt_tos_info *tosinfo = (struct ipt_tos_info *)(*match)->data;

	switch (c) {
	case '1':
		/* Ensure that `--tos' haven't been used yet. */
		if (*flags == 1)
			xtables_error(PARAMETER_PROBLEM,
					"tos match: only use --tos once!");

		parse_tos(argv[optind-1], tosinfo);
		if (invert)
			tosinfo->invert = 1;
		*flags = 1;
		break;

	default:
		return 0;
	}
	return 1;
}

static void
print_tos(u_int8_t tos, int numeric)
{
	unsigned int i;

	if (!numeric) {
		for (i = 0; i<sizeof(TOS_values)/sizeof(struct TOS_value); i++)
			if (TOS_values[i].TOS == tos) {
				printf("%s ", TOS_values[i].name);
				return;
			}
	}
	printf("0x%02x ", tos);
}

/* Final check; must have specified --tos. */
static void
final_check(unsigned int flags)
{
	if (!flags)
		xtables_error(PARAMETER_PROBLEM,
			   "TOS match: You must specify `--tos'");
}

/* Prints out the matchinfo. */
static void
print(const void *ip,
      const struct xt_entry_match *match,
      int numeric)
{
	const struct ipt_tos_info *info = (const struct ipt_tos_info *)match->data;
    
	printf("TOS match ");
	if (info->invert)
		printf("!");
	print_tos(info->tos, numeric);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
save(const void *ip, const struct xt_entry_match *match)
{
	const struct ipt_tos_info *info = (const struct ipt_tos_info *)match->data;
    
	if (info->invert)
		printf("! ");
	printf("--tos ");
	print_tos(info->tos, 0);
}

static struct xtables_match tos = { 
	.name		= "tos",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct ipt_tos_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ipt_tos_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	xtables_register_match(&tos);
}
