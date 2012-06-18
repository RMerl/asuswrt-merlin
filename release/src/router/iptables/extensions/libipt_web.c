/*

	web (experimental)
	HTTP client match
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_web.h>


#undef IPTABLES_SAVE


static void help(void)
{
	printf(
		"web match v0.01 (experimental)\n"
		"Copyright (C) 2006 Jonathan Zarate\n"
		"Options:\n"
		"[!] --http (default)   find an HTTP GET/POST request\n"
		"[!] --host <text ...>  find in host line\n"
		"[!] --req <text ...>   find in request\n"
		"[!] --path <text ...>  find in request path\n"
		"[!] --query <text ...> find in request query\n"
		"[!] --hore <text ...>  find in host or request line\n"
		" <text> can be:\n"
		"  text    contains\n"
		"  ^text   begins with\n"
		"  text$   ends with\n"
		"  ^text$  exact match\n");
}

static void init(struct ipt_entry_match *m, unsigned int *nfcache)
{
	*nfcache |= NFC_UNKNOWN;
}

static struct option opts[] = {
	{ .name = "http",  .has_arg = 0, .flag = 0, .val = '1' },
	{ .name = "host",  .has_arg = 1, .flag = 0, .val = '2' },
	{ .name = "req",   .has_arg = 1, .flag = 0, .val = '3' },
	{ .name = "path",  .has_arg = 1, .flag = 0, .val = '4' },
	{ .name = "query", .has_arg = 1, .flag = 0, .val = '5' },
	{ .name = "hore",  .has_arg = 1, .flag = 0, .val = '6' },
	{ .name = 0 }
};

static int parse(int c, char **argv, int invert, unsigned int *flags,
				 const struct ipt_entry *entry, unsigned int *nfcache,
				 struct ipt_entry_match **match)
{
	const char *s;
	char *e, *p;
	int n;
	struct ipt_web_info *info;

	if ((c < '1') || (c > '6')) return 0;

	if (*flags) exit_error(PARAMETER_PROBLEM, "Multiple modes are not supported");
	*flags = 1;

	info = (struct ipt_web_info *)(*match)->data;
	switch (c) {
	case '2':
		info->mode = IPT_WEB_HOST;
		break;
	case '3':
		info->mode = IPT_WEB_RURI;
		break;
	case '4':
		info->mode = IPT_WEB_PATH;
		break;
	case '5':
		info->mode = IPT_WEB_QUERY;
		break;
	case '6':
		info->mode = IPT_WEB_HORE;
		break;
	default:	// IPT_WEB_HTTP
		return 1;
	}

	if (entry->ip.proto != IPPROTO_TCP) {
		exit_error(PARAMETER_PROBLEM, "web match requires -p tcp");
	}

	check_inverse(optarg, &invert, &optind, 0);
	if (invert) info->invert = 1;

	// convert arg to text\0text\0\0
	s = argv[optind - 1];

	if ((p = malloc(strlen(s) + 2)) == NULL) {
		exit_error(PARAMETER_PROBLEM, "Not enough memory");
	}

	e = p;
	while (*s) {
		while ((*s == ' ') || (*s == '\n') || (*s == '\t')) ++s;
		if (*s == 0) break;
		while ((*s != 0) && (*s != ' ') && (*s != '\n') && (*s != '\t')) {
			*e++ = *s++;
		}
		*e++ = 0;
	}
	n = (e - p);

#if 0
	*e = 0;
	e = p;
	while (*e) {
		printf("[%s]\n", e);
		e += strlen(e) + 1;
	}
#endif

	if (n <= 1) {
		exit_error(PARAMETER_PROBLEM, "Text is too short");
	}
	if (n >= IPT_WEB_MAXTEXT) {
		exit_error(PARAMETER_PROBLEM, "Text is too long");
	}
	memcpy(info->text, p, n);
	memset(info->text + n, 0, IPT_WEB_MAXTEXT - n);		// term, need to clear rest for ipt rule cmp
	free(p);
	return 1;
}

static void final_check(unsigned int flags)
{
}

static void print_match(const struct ipt_web_info *info)
{
	const char *text;

	if (info->invert) printf("! ");

	switch (info->mode) {
	case IPT_WEB_HOST:
		printf("--host");
		break;
	case IPT_WEB_RURI:
		printf("--req");
		break;
	case IPT_WEB_PATH:
		printf("--path");
		break;
	case IPT_WEB_QUERY:
		printf("--query");
		break;
	case IPT_WEB_HORE:
		printf("--hore");
		break;
	default:
		printf("--http");
		return;
	}

	text = info->text;
	printf(" \"");
	while (*text) {
		while (*text) {
			if (*text == '"') printf("\\\"");
				else putc(*text, stdout);
			++text;
		}
		++text;
		if (*text == 0) break;
		putc(' ', stdout);
	}
	printf("\" ");
}

static void print(const struct ipt_ip *ip, const struct ipt_entry_match *match, int numeric)
{
	printf("web ");
	print_match((const struct ipt_web_info *)match->data);
}

static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
#ifdef IPTABLES_SAVE
	print_match((const struct ipt_web_info *)match->data);
#endif
}


static struct iptables_match web_match = {
	.name          = "web",
	.version       = IPTABLES_VERSION,
	.size          = IPT_ALIGN(sizeof(struct ipt_web_info)),
	.userspacesize = IPT_ALIGN(sizeof(struct ipt_web_info)),
	.help          = &help,
	.init          = &init,
	.parse         = &parse,
	.final_check   = &final_check,
	.print         = &print,
	.save          = &save,
	.extra_opts    = opts
};

void _init(void)
{
	register_match(&web_match);
}
