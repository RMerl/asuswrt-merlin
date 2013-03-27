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

/* iptable-1.4.0rc1 port. (ipt->xt)
 *
 * Copyright (C) 2008, Ralink Technology Corporation
 * All Rights Reserved.
 */

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
//#include <linux/netfilter_ipv4/ipt_webstr.h>

#define BM_MAX_NLEN 256
#define BM_MAX_HLEN 1024

#define BLK_JAVA        0x01
#define BLK_ACTIVE      0x02
#define BLK_COOKIE      0x04
#define BLK_PROXY       0x08

typedef char *(*proc_ipt_search) (char *, char *, int, int);

struct ipt_webstr_info {
    char string[BM_MAX_NLEN];
    u_int16_t invert;
    u_int16_t len;
    u_int8_t type;
};

enum xt_webstr_type
{
    IPT_WEBSTR_HOST,
    IPT_WEBSTR_URL,
    IPT_WEBSTR_CONTENT
};



/* Function which prints out usage message. */
static void
webstr_help(void)
{
	printf(
"WEBSTR match v%s options:\n"
"--webstr [!] host            Match a http string in a packet\n"
"--webstr [!] url             Match a http string in a packet\n"
"--webstr [!] content         Match a http string in a packet\n",
XTABLES_VERSION);

	fputc('\n', stdout);
}

static struct option webstr_opts[] = {
	{.name = "host", .has_arg = true, .val = '1' },
	{.name = "url", .has_arg = true, .val = '2' },
	{.name = "content", .has_arg = true, .val = '3' },
	XT_GETOPT_TABLEEND,
};

/* Initialize the match. */
static void
webstr_init(struct ipt_entry_match *m)
{
	return;
}

static void
parse_string(const unsigned char *s, struct ipt_webstr_info *info)
{	
        if (strlen(s) <= BM_MAX_NLEN) strcpy(info->string, s);
	else xtables_error(PARAMETER_PROBLEM, "WEBSTR too long `%s'", s);
}

/* Function which parses command options; returns true if it
   ate an option */
static int
webstr_parse(int c, char **argv, int invert, unsigned int *flags,
      const void *entry,
#if 0
      unsigned int *nfcache,
#endif
      struct ipt_entry_match **match)
{
	struct ipt_webstr_info *stringinfo = (struct ipt_webstr_info *)(*match)->data;

	switch (c) {
	case '1':
		parse_string(argv[optind-1], stringinfo);
		if (invert)
			stringinfo->invert = 1;
                stringinfo->len=strlen((char *)&stringinfo->string);
                stringinfo->type = IPT_WEBSTR_HOST;
		break;

	case '2':
		parse_string(argv[optind-1], stringinfo);
		if (invert)
			stringinfo->invert = 1;
                stringinfo->len=strlen((char *)&stringinfo->string);
                stringinfo->type = IPT_WEBSTR_URL;
		break;

	case '3':
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
webstr_final_check(unsigned int flags)
{
	if (!flags)
		xtables_error(PARAMETER_PROBLEM,
			   "WEBSTR match: You must specify `--webstr'");
}

/* Prints out the matchinfo. */
static void
webstr_print(const void *ip,
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
webstr_save(const void *ip, const struct ipt_entry_match *match)
{
	printf("--webstr ");
	print_string(((struct ipt_webstr_info *)match->data)->string,
		  ((struct ipt_webstr_info *)match->data)->invert, 0);
}

static
struct xtables_match webstr_match = {
    .family = NFPROTO_IPV4,
    .name = "webstr",
    .version = XTABLES_VERSION,
    .size = XT_ALIGN(sizeof(struct ipt_webstr_info)),
    .userspacesize = XT_ALIGN(sizeof(struct ipt_webstr_info)),
    .help = webstr_help,
    .init = webstr_init,
    .parse = webstr_parse,
    .final_check = webstr_final_check,
    .print = webstr_print,
    .save = webstr_save,
    .extra_opts = webstr_opts
};

void _init(void)
{
	xtables_register_match(&webstr_match);
}
