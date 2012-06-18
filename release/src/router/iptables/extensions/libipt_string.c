/* Shared library add-on to iptables to add string matching support. 
 * 
 * Copyright (C) 2000 Emmanuel Roger  <winfield@freegates.be>
 *
 * 2005-08-05 Pablo Neira Ayuso <pablo@eurodev.net>
 * 	- reimplemented to use new string matching iptables match
 * 	- add functionality to match packets by using window offsets
 * 	- add functionality to select the string matching algorithm
 *
 * ChangeLog
 *     29.12.2003: Michael Rash <mbr@cipherdyne.org>
 *             Fixed iptables save/restore for ascii strings
 *             that contain space chars, and hex strings that
 *             contain embedded NULL chars.  Updated to print
 *             strings in hex mode if any non-printable char
 *             is contained within the string.
 *
 *     27.01.2001: Gianni Tedesco <gianni@ecsc.co.uk>
 *             Changed --tos to --string in save(). Also
 *             updated to work with slightly modified
 *             ipt_string_info.
 */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <iptables.h>
#include <stddef.h>
#include <linux/netfilter_ipv4/ipt_string.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"STRING match v%s options:\n"
"--from                       Offset to start searching from\n"
"--to                         Offset to stop searching\n"
"--algo	                      Algorithm\n"
"--string [!] string          Match a string in a packet\n"
"--hex-string [!] string      Match a hex string in a packet\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "from", 1, 0, '1' },
	{ "to", 1, 0, '2' },
	{ "algo", 1, 0, '3' },
	{ "string", 1, 0, '4' },
	{ "hex-string", 1, 0, '5' },
	{0}
};

static void
init(struct ipt_entry_match *m, unsigned int *nfcache)
{
	struct ipt_string_info *i = (struct ipt_string_info *) m->data;

	if (i->to_offset == 0)
		i->to_offset = (u_int16_t) ~0UL;
}

static void
parse_string(const char *s, struct ipt_string_info *info)
{	
	if (strlen(s) <= IPT_STRING_MAX_PATTERN_SIZE) {
		strncpy(info->pattern, s, IPT_STRING_MAX_PATTERN_SIZE);
		info->patlen = strlen(s);
		return;
	}
	exit_error(PARAMETER_PROBLEM, "STRING too long `%s'", s);
}

static void
parse_algo(const char *s, struct ipt_string_info *info)
{
	if (strlen(s) <= IPT_STRING_MAX_ALGO_NAME_SIZE) {
		strncpy(info->algo, s, IPT_STRING_MAX_ALGO_NAME_SIZE);
		return;
	}
	exit_error(PARAMETER_PROBLEM, "ALGO too long `%s'", s);
}

static void
parse_hex_string(const char *s, struct ipt_string_info *info)
{
	int i=0, slen, sindex=0, schar;
	short hex_f = 0, literal_f = 0;
	char hextmp[3];

	slen = strlen(s);

	if (slen == 0) {
		exit_error(PARAMETER_PROBLEM,
			"STRING must contain at least one char");
	}

	while (i < slen) {
		if (s[i] == '\\' && !hex_f) {
			literal_f = 1;
		} else if (s[i] == '\\') {
			exit_error(PARAMETER_PROBLEM,
				"Cannot include literals in hex data");
		} else if (s[i] == '|') {
			if (hex_f)
				hex_f = 0;
			else {
				hex_f = 1;
				/* get past any initial whitespace just after the '|' */
				while (s[i+1] == ' ')
					i++;
			}
			if (i+1 >= slen)
				break;
			else
				i++;  /* advance to the next character */
		}

		if (literal_f) {
			if (i+1 >= slen) {
				exit_error(PARAMETER_PROBLEM,
					"Bad literal placement at end of string");
			}
			info->pattern[sindex] = s[i+1];
			i += 2;  /* skip over literal char */
			literal_f = 0;
		} else if (hex_f) {
			if (i+1 >= slen) {
				exit_error(PARAMETER_PROBLEM,
					"Odd number of hex digits");
			}
			if (i+2 >= slen) {
				/* must end with a "|" */
				exit_error(PARAMETER_PROBLEM, "Invalid hex block");
			}
			if (! isxdigit(s[i])) /* check for valid hex char */
				exit_error(PARAMETER_PROBLEM, "Invalid hex char `%c'", s[i]);
			if (! isxdigit(s[i+1])) /* check for valid hex char */
				exit_error(PARAMETER_PROBLEM, "Invalid hex char `%c'", s[i+1]);
			hextmp[0] = s[i];
			hextmp[1] = s[i+1];
			hextmp[2] = '\0';
			if (! sscanf(hextmp, "%x", &schar))
				exit_error(PARAMETER_PROBLEM,
					"Invalid hex char `%c'", s[i]);
			info->pattern[sindex] = (char) schar;
			if (s[i+2] == ' ')
				i += 3;  /* spaces included in the hex block */
			else
				i += 2;
		} else {  /* the char is not part of hex data, so just copy */
			info->pattern[sindex] = s[i];
			i++;
		}
		if (sindex > IPT_STRING_MAX_PATTERN_SIZE)
			exit_error(PARAMETER_PROBLEM, "STRING too long `%s'", s);
		sindex++;
	}
	info->patlen = sindex;
}

#define STRING 0x1
#define ALGO   0x2
#define FROM   0x4
#define TO     0x8

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_string_info *stringinfo = (struct ipt_string_info *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & FROM)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify multiple --from");
		stringinfo->from_offset = atoi(optarg);
		*flags |= FROM;
		break;
	case '2':
		if (*flags & TO)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify multiple --to");
		stringinfo->to_offset = atoi(optarg);
		*flags |= TO;
		break;
	case '3':
		if (*flags & ALGO)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify multiple --algo");
		parse_algo(optarg, stringinfo);
		*flags |= ALGO;
		break;
	case '4':
		if (*flags & STRING)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify multiple --string");
		check_inverse(optarg, &invert, &optind, 0);
		parse_string(argv[optind-1], stringinfo);
		if (invert)
			stringinfo->invert = 1;
		stringinfo->patlen=strlen((char *)&stringinfo->pattern);
		*flags |= STRING;
		break;

	case '5':
		if (*flags & STRING)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify multiple --hex-string");

		check_inverse(optarg, &invert, &optind, 0);
		parse_hex_string(argv[optind-1], stringinfo);  /* sets length */
		if (invert)
			stringinfo->invert = 1;
		*flags |= STRING;
		break;

	default:
		return 0;
	}
	return 1;
}


/* Final check; must have specified --string. */
static void
final_check(unsigned int flags)
{
	if (!(flags & STRING))
		exit_error(PARAMETER_PROBLEM,
			   "STRING match: You must specify `--string' or "
			   "`--hex-string'");
	if (!(flags & ALGO))
		exit_error(PARAMETER_PROBLEM,
			   "STRING match: You must specify `--algo'");
}

/* Test to see if the string contains non-printable chars or quotes */
static unsigned short int
is_hex_string(const char *str, const unsigned short int len)
{
	unsigned int i;
	for (i=0; i < len; i++)
		if (! isprint(str[i]))
			return 1;  /* string contains at least one non-printable char */
	/* use hex output if the last char is a "\" */
	if ((unsigned char) str[len-1] == 0x5c)
		return 1;
	return 0;
}

/* Print string with "|" chars included as one would pass to --hex-string */
static void
print_hex_string(const char *str, const unsigned short int len)
{
	unsigned int i;
	/* start hex block */
	printf("\"|");
	for (i=0; i < len; i++) {
		/* see if we need to prepend a zero */
		if ((unsigned char) str[i] <= 0x0F)
			printf("0%x", (unsigned char) str[i]);
		else
			printf("%x", (unsigned char) str[i]);
	}
	/* close hex block */
	printf("|\" ");
}

static void
print_string(const char *str, const unsigned short int len)
{
	unsigned int i;
	printf("\"");
	for (i=0; i < len; i++) {
		if ((unsigned char) str[i] == 0x22)  /* escape any embedded quotes */
			printf("%c", 0x5c);
		printf("%c", (unsigned char) str[i]);
	}
	printf("\" ");  /* closing space and quote */
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	const struct ipt_string_info *info =
	    (const struct ipt_string_info*) match->data;

	if (is_hex_string(info->pattern, info->patlen)) {
		printf("STRING match %s", (info->invert) ? "!" : "");
		print_hex_string(info->pattern, info->patlen);
	} else {
		printf("STRING match %s", (info->invert) ? "!" : "");
		print_string(info->pattern, info->patlen);
	}
	printf("ALGO name %s ", info->algo);
	if (info->from_offset != 0)
		printf("FROM %u ", info->from_offset);
	if (info->to_offset != 0)
		printf("TO %u ", info->to_offset);
}


/* Saves the union ipt_matchinfo in parseable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	const struct ipt_string_info *info =
	    (const struct ipt_string_info*) match->data;

	if (is_hex_string(info->pattern, info->patlen)) {
		printf("--hex-string %s", (info->invert) ? "! ": "");
		print_hex_string(info->pattern, info->patlen);
	} else {
		printf("--string %s", (info->invert) ? "! ": "");
		print_string(info->pattern, info->patlen);
	}
	printf("--algo %s ", info->algo);
	if (info->from_offset != 0)
		printf("--from %u ", info->from_offset);
	if (info->to_offset != 0)
		printf("--to %u ", info->to_offset);
}


static struct iptables_match string = {
    .name		= "string",
    .version		= IPTABLES_VERSION,
    .size		= IPT_ALIGN(sizeof(struct ipt_string_info)),
    .userspacesize	= offsetof(struct ipt_string_info, config),
    .help		= help,
    .init		= init,
    .parse		= parse,
    .final_check	= final_check,
    .print		= print,
    .save		= save,
    .extra_opts		= opts
};


void _init(void)
{
	register_match(&string);
}
