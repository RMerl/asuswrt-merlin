/* ebt_limit
 *
 * Authors:
 * Tom Marshall <tommy@home.tig-grr.com>
 *
 * Mostly copied from iptables' limit match.
 *
 * September, 2003
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include "../include/ebtables_u.h"
#include <linux/netfilter_bridge/ebt_limit.h>

#define EBT_LIMIT_AVG	"3/hour"
#define EBT_LIMIT_BURST	5

static int string_to_number(const char *s, unsigned int min, unsigned int max,
   unsigned int *ret)
{
	long number;
	char *end;

	errno = 0;
	number = strtol(s, &end, 0);
	if (*end == '\0' && end != s) {
		if (errno != ERANGE && min <= number && number <= max) {
			*ret = number;
			return 0;
		}
	}
	return -1;
}

#define FLAG_LIMIT		0x01
#define FLAG_LIMIT_BURST	0x02
#define ARG_LIMIT		'1'
#define ARG_LIMIT_BURST		'2'

static struct option opts[] =
{
	{ "limit",		required_argument, 0, ARG_LIMIT },
	{ "limit-burst",	required_argument, 0, ARG_LIMIT_BURST },
	{ 0 }
};

static void print_help(void)
{
	printf(
"limit options:\n"
"--limit avg                   : max average match rate: default "EBT_LIMIT_AVG"\n"
"                                [Packets per second unless followed by \n"
"                                /sec /minute /hour /day postfixes]\n"
"--limit-burst number          : number to match in a burst, -1 < number < 10001,\n"
"                                default %u\n", EBT_LIMIT_BURST);
}

static int parse_rate(const char *rate, u_int32_t *val)
{
	const char *delim;
	u_int32_t r;
	u_int32_t mult = 1;  /* Seconds by default. */

	delim = strchr(rate, '/');
	if (delim) {
		if (strlen(delim+1) == 0)
			return 0;

		if (strncasecmp(delim+1, "second", strlen(delim+1)) == 0)
			mult = 1;
		else if (strncasecmp(delim+1, "minute", strlen(delim+1)) == 0)
			mult = 60;
		else if (strncasecmp(delim+1, "hour", strlen(delim+1)) == 0)
			mult = 60*60;
		else if (strncasecmp(delim+1, "day", strlen(delim+1)) == 0)
			mult = 24*60*60;
		else
			return 0;
	}
	r = atoi(rate);
	if (!r)
		return 0;

	/* This would get mapped to infinite (1/day is minimum they
	   can specify, so we're ok at that end). */
	if (r / mult > EBT_LIMIT_SCALE)
		return 0;

	*val = EBT_LIMIT_SCALE * mult / r;
	return 1;
}

/* Initialize the match. */
static void init(struct ebt_entry_match *m)
{
	struct ebt_limit_info *r = (struct ebt_limit_info *)m->data;

	parse_rate(EBT_LIMIT_AVG, &r->avg);
	r->burst = EBT_LIMIT_BURST;
}

/* FIXME: handle overflow:
	if (r->avg*r->burst/r->burst != r->avg)
		exit_error(PARAMETER_PROBLEM,
			   "Sorry: burst too large for that avg rate.\n");
*/

static int parse(int c, char **argv, int argc,
      const struct ebt_u_entry *entry,
      unsigned int *flags,
      struct ebt_entry_match **match)
{
	struct ebt_limit_info *r = (struct ebt_limit_info *)(*match)->data;
	unsigned int num;

	switch(c) {
	case ARG_LIMIT:
		ebt_check_option2(flags, FLAG_LIMIT);
		if (ebt_check_inverse2(optarg))
			ebt_print_error2("Unexpected `!' after --limit");
		if (!parse_rate(optarg, &r->avg))
			ebt_print_error2("bad rate `%s'", optarg);
		break;

	case ARG_LIMIT_BURST:
		ebt_check_option2(flags, FLAG_LIMIT_BURST);
		if (ebt_check_inverse2(optarg))
			ebt_print_error2("Unexpected `!' after --limit-burst");
		if (string_to_number(optarg, 0, 10000, &num) == -1)
			ebt_print_error2("bad --limit-burst `%s'", optarg);
		r->burst = num;
		break;

	default:
		return 0;
	}

	return 1;
}

static void final_check(const struct ebt_u_entry *entry,
   const struct ebt_entry_match *match, const char *name,
   unsigned int hookmask, unsigned int time)
{
}

struct rates
{
	const char *name;
	u_int32_t mult;
};

static struct rates g_rates[] =
{
	{ "day", EBT_LIMIT_SCALE*24*60*60 },
	{ "hour", EBT_LIMIT_SCALE*60*60 },
	{ "min", EBT_LIMIT_SCALE*60 },
	{ "sec", EBT_LIMIT_SCALE }
};

static void print_rate(u_int32_t period)
{
	unsigned int i;

	for (i = 1; i < sizeof(g_rates)/sizeof(struct rates); i++)
		if (period > g_rates[i].mult ||
		    g_rates[i].mult/period < g_rates[i].mult%period)
			break;

	printf("%u/%s ", g_rates[i-1].mult / period, g_rates[i-1].name);
}

static void print(const struct ebt_u_entry *entry,
   const struct ebt_entry_match *match)
{
	struct ebt_limit_info *r = (struct ebt_limit_info *)match->data;

	printf("--limit ");
	print_rate(r->avg);
	printf("--limit-burst %u ", r->burst);
}

static int compare(const struct ebt_entry_match* m1,
   const struct ebt_entry_match *m2)
{
	struct ebt_limit_info* li1 = (struct ebt_limit_info*)m1->data;
	struct ebt_limit_info* li2 = (struct ebt_limit_info*)m2->data;

	if (li1->avg != li2->avg)
		return 0;

	if (li1->burst != li2->burst)
		return 0;

	return 1;
}

static struct ebt_u_match limit_match =
{
	.name		= "limit",
	.size		= sizeof(struct ebt_limit_info),
	.help		= print_help,
	.init		= init,
	.parse		= parse,
	.final_check	= final_check,
	.print		= print,
	.compare	= compare,
	.extra_ops	= opts,
};

void _init(void)
{
	ebt_register_match(&limit_match);
}
