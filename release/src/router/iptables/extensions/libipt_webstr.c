/* Shared library add-on to iptables to add string matching support. 
 * 
 * Copyright (C) 2000 Emmanuel Roger  <winfield@freegates.be>
 *
 * ChangeLog
 *     27.01.2001: Gianni Tedesco <gianni@ecsc.co.uk>
 *             Changed --tos to --string in save(). Also
 *             updated to work with slightly modified
 *             ipt_string_info.
 */

/* Shared library add-on to iptables to add webstr matching support. 
 *
 * Copyright (C) 2003, CyberTAN Corporation
 * All Rights Reserved.
 *
 * Description:
 *   This is shared library, added to iptables, for web content inspection. 
 *   It was derived from 'string' matching support, declared as above.
 *
 */


#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_webstr.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"WEBSTR match v%s options:\n"
"--webstr [!] host            Match a http string in a packet\n"
"--webstr [!] url             Match a http string in a packet\n"
"--webstr [!] content         Match a http string in a packet\n",
IPTABLES_VERSION);

	fputc('\n', stdout);
}

static struct option opts[] = {
	{ "host", 1, 0, '1' },
	{ "url", 1, 0, '2' },
	{ "content", 1, 0, '3' },
	{0}
};

/* Initialize the match. */
static void
init(struct ipt_entry_match *m, unsigned int *nfcache)
{
	*nfcache |= NFC_UNKNOWN;
}

static void
parse_string(const unsigned char *s, struct ipt_webstr_info *info)
{	
        if (strlen(s) <= BM_MAX_NLEN) strcpy(info->string, s);
	else exit_error(PARAMETER_PROBLEM, "WEBSTR too long `%s'", s);
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_webstr_info *stringinfo = (struct ipt_webstr_info *)(*match)->data;

	switch (c) {
	case '1':
		check_inverse(optarg, &invert, &optind, 0);
		parse_string(argv[optind-1], stringinfo);
		if (invert)
			stringinfo->invert = 1;
                stringinfo->len=strlen((char *)&stringinfo->string);
                stringinfo->type = IPT_WEBSTR_HOST;
		break;

	case '2':
		check_inverse(optarg, &invert, &optind, 0);
		parse_string(argv[optind-1], stringinfo);
		if (invert)
			stringinfo->invert = 1;
                stringinfo->len=strlen((char *)&stringinfo->string);
                stringinfo->type = IPT_WEBSTR_URL;
		break;

	case '3':
		check_inverse(optarg, &invert, &optind, 0);
		parse_string(argv[optind-1], stringinfo);
		if (invert)
			stringinfo->invert = 1;
                stringinfo->len=strlen((char *)&stringinfo->string);
                stringinfo->type = IPT_WEBSTR_CONTENT;
		break;

	default:
		return 0;
	}

	*flags = 1;
	return 1;
}

static void
print_string(char string[], int invert, int numeric)
{

	if (invert)
		fputc('!', stdout);
	printf("%s ",string);
}

/* Final check; must have specified --string. */
static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "WEBSTR match: You must specify `--webstr'");
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	struct ipt_webstr_info *stringinfo = (struct ipt_webstr_info *)match->data;

	printf("WEBSTR match ");

	
	switch (stringinfo->type) {
	case IPT_WEBSTR_HOST:
		printf("host ");
		break;

	case IPT_WEBSTR_URL:
		printf("url ");
		break;

	case IPT_WEBSTR_CONTENT:
		printf("content ");
		break;

	default:
		printf("ERROR ");
		break;
	}

	print_string(((struct ipt_webstr_info *)match->data)->string,
		  ((struct ipt_webstr_info *)match->data)->invert, numeric);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	printf("--webstr ");
	print_string(((struct ipt_webstr_info *)match->data)->string,
		  ((struct ipt_webstr_info *)match->data)->invert, 0);
}

static
struct iptables_match webstr
= { 
	.next = NULL,
	.name = "webstr",
    .version = IPTABLES_VERSION,
    .size = IPT_ALIGN(sizeof(struct ipt_webstr_info)),
    .userspacesize = IPT_ALIGN(sizeof(struct ipt_webstr_info)),
    .help = &help,
    .init = &init,
    .parse = &parse,
    .final_check = &final_check,
    .print = &print,
    .save = &save,
    .extra_opts = opts
};

void _init(void)
{
	register_match(&webstr);
}
