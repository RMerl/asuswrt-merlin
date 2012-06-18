/* Shared library add-on to iptables to add u32 matching,
 * generalized matching on values found at packet offsets
 *
 * Detailed doc is in the kernel module source
 * net/ipv4/netfilter/ipt_u32.c
 *
 * (C) 2002 by Don Cohen <don-netf@isis.cs3-inc.com>
 * Released under the terms of GNU GPL v2
 */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_u32.h>
#include <errno.h>
#include <ctype.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf( "u32 v%s options:\n"
		" --u32 tests\n"
		" tests := location = value | tests && location = value\n"
		" value := range | value , range\n"
		" range := number | number : number\n"
		" location := number | location operator number\n"
		" operator := & | << | >> | @\n"
		,IPTABLES_VERSION);
}

/* defined in /usr/include/getopt.h maybe in man getopt */
static struct option opts[] = {
	{ "u32", 1, 0, '1' },
	{ 0 }
};

/* shared printing code */
static void print_u32(struct ipt_u32 *data)
{
	unsigned int testind;

	for (testind=0; testind < data->ntests; testind++) {
		if (testind) printf("&&");
		{
			unsigned int i;

			printf("0x%x", data->tests[testind].location[0].number);
			for (i = 1; i < data->tests[testind].nnums; i++) {
				switch (data->tests[testind].location[i].nextop) {
				case IPT_U32_AND: printf("&"); break;
				case IPT_U32_LEFTSH: printf("<<"); break;
				case IPT_U32_RIGHTSH: printf(">>"); break;
				case IPT_U32_AT: printf("@"); break;
				}
				printf("0x%x", data->tests[testind].location[i].number);
			}
			printf("=");
			for (i = 0; i < data->tests[testind].nvalues; i++) {
				if (i) printf(",");
				if (data->tests[testind].value[i].min
				    == data->tests[testind].value[i].max)
					printf("0x%x", data->tests[testind].value[i].min);
				else printf("0x%x:0x%x", data->tests[testind].value[i].min,
					    data->tests[testind].value[i].max);
			}
		}
	}
	printf(" ");
}

/* string_to_number is not quite what we need here ... */
u_int32_t parse_number(char **s, int pos)
{
	u_int32_t number;
	char *end;
	errno = 0;

	number = strtoul(*s, &end, 0);
	if (end == *s)
		exit_error(PARAMETER_PROBLEM, 
			   "u32: at char %d expected number", pos);
	if (errno)
		exit_error(PARAMETER_PROBLEM, 
			   "u32: at char %d error reading number", pos);
	*s = end;
	return number;
}

/* Function which parses command options; returns true if it ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_u32 *data = (struct ipt_u32 *)(*match)->data;
	char *arg = argv[optind-1]; /* the argument string */
	char *start = arg;
	int state=0, testind=0, locind=0, valind=0;

	if (c != '1') return 0;
	/* states: 0 = looking for numbers and operations, 1 = looking for ranges */
	while (1) { /* read next operand/number or range */
		while (isspace(*arg)) 
			arg++;  /* skip white space */
		if (! *arg) { /* end of argument found */
			if (state == 0)
				exit_error(PARAMETER_PROBLEM, 
					   "u32: input ended in location spec");
			if (valind == 0)
				exit_error(PARAMETER_PROBLEM, 
					   "u32: test ended with no value spec");
			data->tests[testind].nnums = locind;
			data->tests[testind].nvalues = valind;
			testind++;
			data->ntests=testind;
			if (testind > U32MAXSIZE)
				exit_error(PARAMETER_PROBLEM, 
					   "u32: at char %d too many &&'s",
					   arg-start);
			/* debugging 
			   print_u32(data);printf("\n");
			   exit_error(PARAMETER_PROBLEM, "debugging output done"); */
			return 1;
		}
		if (state == 0) {
			/* reading location: read a number if nothing read yet,
			   otherwise either op number or = to end location spec */	 
			if (*arg == '=') {
				if (locind == 0)
					exit_error(PARAMETER_PROBLEM,
						   "u32: at char %d location spec missing", arg-start);
				else {
					arg++; 
					state=1;
				}
			}
			else {
				if (locind) { /* need op before number */
					if (*arg == '&') {
						data->tests[testind].location[locind].nextop = IPT_U32_AND;
					}
					else if (*arg == '<') {
						arg++;
						if (*arg != '<')
							exit_error(PARAMETER_PROBLEM,
								   "u32: at char %d a second < expected", arg-start);
						data->tests[testind].location[locind].nextop = IPT_U32_LEFTSH;
					}
					else if (*arg == '>') {
						arg++;
						if (*arg != '>')
							exit_error(PARAMETER_PROBLEM,
								   "u32: at char %d a second > expected", arg-start);
						data->tests[testind].location[locind].nextop = IPT_U32_RIGHTSH;
					}
					else if (*arg == '@') {
						data->tests[testind].location[locind].nextop = IPT_U32_AT;
					}
					else exit_error(PARAMETER_PROBLEM,
							"u32: at char %d operator expected", arg-start);
					arg++;
				}
				/* now a number; string_to_number skips white space? */
				data->tests[testind].location[locind].number =
					parse_number(&arg, arg-start);
				locind++;
				if (locind > U32MAXSIZE)
					exit_error(PARAMETER_PROBLEM,
						   "u32: at char %d too many operators", arg-start);
			}
		}
		else {
			/* state 1 - reading values: read a range if nothing read yet,
			   otherwise either ,range or && to end test spec */
			if (*arg == '&') {
				arg++;
				if (*arg != '&')
					exit_error(PARAMETER_PROBLEM,
						   "u32: at char %d a second & expected", arg-start);
				if (valind == 0)
					exit_error(PARAMETER_PROBLEM,
						   "u32: at char %d value spec missing", arg-start);
				else {
					data->tests[testind].nnums = locind;
					data->tests[testind].nvalues = valind;
					testind++;
					if (testind > U32MAXSIZE)
						exit_error(PARAMETER_PROBLEM,
							   "u32: at char %d too many &&'s", arg-start);
					arg++; state=0; locind=0; valind=0;
				}
			}
			else { /* read value range */
				if (valind) { /* need , before number */
					if (*arg != ',')
						exit_error(PARAMETER_PROBLEM,
							   "u32: at char %d expected , or &&", arg-start);
					arg++;
				}
				data->tests[testind].value[valind].min = parse_number(&arg, arg-start);
				while (isspace(*arg)) 
					arg++;  /* another place white space could be */
				if (*arg==':') {
					arg++;
					data->tests[testind].value[valind].max
						= parse_number(&arg, arg-start);
				}
				else data->tests[testind].value[valind].max
					     = data->tests[testind].value[valind].min;
				valind++;
				if (valind > U32MAXSIZE)
					exit_error(PARAMETER_PROBLEM,
						   "u32: at char %d too many ,'s", arg-start);
			}
		}
	}
}

/* Final check; must specify something. */
static void
final_check(unsigned int flags)
{
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	printf("u32 ");
	print_u32((struct ipt_u32 *)match->data);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	printf("--u32 ");
	print_u32((struct ipt_u32 *)match->data);
}

struct iptables_match u32 = {
	.next		= NULL,
	.name		= "u32",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_u32)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_u32)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void
_init(void)
{
	register_match(&u32);
}
