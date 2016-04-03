/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h>				/* assert */
#include <string.h>				/* memcmp, str* */

#include <libipset/linux_ip_set.h>		/* IPSET_CMD_* */
#include <libipset/icmp.h>			/* id_to_icmp */
#include <libipset/icmpv6.h>			/* id_to_icmpv6 */
#include <libipset/types.h>			/* IPSET_*_ARG */
#include <libipset/session.h>			/* ipset_envopt_parse */
#include <libipset/parse.h>			/* ipset_parse_family */
#include <libipset/print.h>			/* ipset_print_family */
#include <libipset/utils.h>			/* STREQ */
#include <libipset/ui.h>			/* prototypes */

/* Commands and environment options */

const struct ipset_commands ipset_commands[] = {
	/* Order is important */

	{	/* c[reate], --create, n[ew], -N */
		.cmd = IPSET_CMD_CREATE,
		.name = { "create", "new", "-N" },
		.has_arg = IPSET_MANDATORY_ARG2,
		.help = "SETNAME TYPENAME [type-specific-options]\n"
			"        Create a new set",
	},
	{	/* a[dd], --add, -A  */
		.cmd = IPSET_CMD_ADD,
		.name = { "add", "-A", NULL },
		.has_arg = IPSET_MANDATORY_ARG2,
		.help = "SETNAME ENTRY\n"
			"        Add entry to the named set",
	},
	{	/* d[el], --del, -D */
		.cmd = IPSET_CMD_DEL,
		.name = { "del", "-D", NULL },
		.has_arg = IPSET_MANDATORY_ARG2,
		.help = "SETNAME ENTRY\n"
			"        Delete entry from the named set",
	},
	{	/* t[est], --test, -T */
		.cmd = IPSET_CMD_TEST,
		.name = { "test", "-T", NULL },
		.has_arg = IPSET_MANDATORY_ARG2,
		.help = "SETNAME ENTRY\n"
			"        Test entry in the named set",
	},
	{	/* des[troy], --destroy, x, -X */
		.cmd = IPSET_CMD_DESTROY,
		.name = { "destroy", "x", "-X" },
		.has_arg = IPSET_OPTIONAL_ARG,
		.help = "[SETNAME]\n"
			"        Destroy a named set or all sets",
	},
	{	/* l[ist], --list, -L */
		.cmd = IPSET_CMD_LIST,
		.name = { "list", "-L", NULL },
		.has_arg = IPSET_OPTIONAL_ARG,
		.help = "[SETNAME]\n"
			"        List the entries of a named set or all sets",
	},
	{	/* s[save], --save, -S */
		.cmd = IPSET_CMD_SAVE,
		.name = { "save", "-S", NULL },
		.has_arg = IPSET_OPTIONAL_ARG,
		.help = "[SETNAME]\n"
			"        Save the named set or all sets to stdout",
	},
	{	/* r[estore], --restore, -R */
		.cmd = IPSET_CMD_RESTORE,
		.name = { "restore", "-R", NULL },
		.has_arg = IPSET_NO_ARG,
		.help = "\n"
			"        Restore a saved state",
	},
	{	/* f[lush], --flush, -F */
		.cmd = IPSET_CMD_FLUSH,
		.name = { "flush", "-F", NULL },
		.has_arg = IPSET_OPTIONAL_ARG,
		.help = "[SETNAME]\n"
			"        Flush a named set or all sets",
	},
	{	/* ren[ame], --rename, e, -E */
		.cmd = IPSET_CMD_RENAME,
		.name = { "rename", "e", "-E" },
		.has_arg = IPSET_MANDATORY_ARG2,
		.help = "FROM-SETNAME TO-SETNAME\n"
			"        Rename two sets",
	},
	{	/* sw[ap], --swap, w, -W */
		.cmd = IPSET_CMD_SWAP,
		.name = { "swap", "w", "-W" },
		.has_arg = IPSET_MANDATORY_ARG2,
		.help = "FROM-SETNAME TO-SETNAME\n"
			"        Swap the contect of two existing sets",
	},
	{	/* h[elp, --help, -H */
		.cmd = IPSET_CMD_HELP,
		.name = { "help", "-h", "-H" },
		.has_arg = IPSET_OPTIONAL_ARG,
		.help = "[TYPENAME]\n"
			"        Print help, and settype specific help",
	},
	{	/* v[ersion], --version, -V */
		.cmd = IPSET_CMD_VERSION,
		.name = { "version", "-v", "-V" },
		.has_arg = IPSET_NO_ARG,
		.help = "\n"
			"        Print version information",
	},
	{	/* q[uit] */
		.cmd = IPSET_CMD_QUIT,
		.name = { "quit", NULL },
		.has_arg = IPSET_NO_ARG,
		.help = "\n"
			"        Quit interactive mode",
	},
	{ },
};

/* Match a command: try to match as a prefix or letter-command */
bool
ipset_match_cmd(const char *arg, const char * const name[])
{
	size_t len, skip = 0;
	int i;

	assert(arg);
	assert(name && name[0]);

	/* Ignore two leading dashes */
	if (arg[0] == '-' && arg[1] == '-')
		skip = 2;

	len = strlen(arg);
	if (len <= skip || (len == 1 && arg[0] == '-'))
		return false;

	for (i = 0; i < IPSET_CMD_ALIASES && name[i] != NULL; i++) {
		/* New command name options */
		if (STRNEQ(arg + skip, name[i], len - skip))
			return true;
	}
	return false;
}

/* Used up so far
 *
 *	-A		add
 *	-D		del
 *	-E		rename
 *	-f		-file
 *	-F		flush
 *	-h		help
 *	-H		help
 *	-L		list
 *	-n		-name
 *	-N		create
 *	-o		-output
 *	-r		-resolve
 *	-R		restore
 *	-s		-sorted
 *	-S		save
 *	-t		-terse
 *	-T		test
 *	-q		-quiet
 *	-X		destroy
 *	-v		version
 *	-V		version
 *	-W		swap
 *	-!		-exist
 */

const struct ipset_envopts ipset_envopts[] = {
	{ .name = { "-o", "-output" },
	  .has_arg = IPSET_MANDATORY_ARG,	.flag = IPSET_OPT_MAX,
	  .parse = ipset_parse_output,
	  .help = "plain|save|xml\n"
		  "       Specify output mode for listing sets.\n"
		  "       Default value for \"list\" command is mode \"plain\"\n"
		  "       and for \"save\" command is mode \"save\".",
	},
	{ .name = { "-s", "-sorted" },
	  .parse = ipset_envopt_parse,
	  .has_arg = IPSET_NO_ARG,	.flag = IPSET_ENV_SORTED,
	  .help = "\n"
		  "        Print elements sorted (if supported by the set type).",
	},
	{ .name = { "-q", "-quiet" },
	  .parse = ipset_envopt_parse,
	  .has_arg = IPSET_NO_ARG,	.flag = IPSET_ENV_QUIET,
	  .help = "\n"
		  "        Suppress any notice or warning message.",
	},
	{ .name = { "-r", "-resolve" },
	  .parse = ipset_envopt_parse,
	  .has_arg = IPSET_NO_ARG,	.flag = IPSET_ENV_RESOLVE,
	  .help = "\n"
		  "        Try to resolve IP addresses in the output (slow!)",
	},
	{ .name = { "-!", "-exist" },
	  .parse = ipset_envopt_parse,
	  .has_arg = IPSET_NO_ARG,	.flag = IPSET_ENV_EXIST,
	  .help = "\n"
		  "        Ignore errors when creating or adding sets or\n"
		  "        elements that do exist or when deleting elements\n"
		  "        that don't exist.",
	},
	{ .name = { "-n", "-name" },
	  .parse = ipset_envopt_parse,
	  .has_arg = IPSET_NO_ARG,	.flag = IPSET_ENV_LIST_SETNAME,
	  .help = "\n"
		  "        When listing, just list setnames from the kernel.\n",
	},
	{ .name = { "-t", "-terse" },
	  .parse = ipset_envopt_parse,
	  .has_arg = IPSET_NO_ARG,	.flag = IPSET_ENV_LIST_HEADER,
	  .help = "\n"
		  "        When listing, list setnames and set headers\n"
		  "        from kernel only.",
	},
	{ .name = { "-f", "-file" },
	  .parse = ipset_parse_file,
	  .has_arg = IPSET_MANDATORY_ARG,	.flag = IPSET_OPT_MAX,
	  .help = "\n"
		  "        Read from the given file instead of standard\n"
		  "        input (restore) or write to given file instead\n"
		  "        of standard output (list/save).",
	},
	{ },
};

/* Strict option matching */
bool
ipset_match_option(const char *arg, const char * const name[])
{
	assert(arg);
	assert(name && name[0]);

	/* Skip two leading dashes */
	if (arg[0] == '-' && arg[1] == '-')
		arg++, arg++;

	return STREQ(arg, name[0]) ||
	       (name[1] != NULL && STREQ(arg, name[1]));
}

/* Strict envopt matching */
bool
ipset_match_envopt(const char *arg, const char * const name[])
{
	assert(arg);
	assert(name && name[0]);

	/* Skip one leading dash */
	if (arg[0] == '-' && arg[1] == '-')
		arg++;

	return STREQ(arg, name[0]) ||
	       (name[1] != NULL && STREQ(arg, name[1]));
}

/**
 * ipset_shift_argv - shift off an argument
 * @arc: argument count
 * @argv: array of argument strings
 * @from: from where shift off an argument
 *
 * Shift off the argument at "from" from the array of
 * arguments argv of size argc.
 */
void
ipset_shift_argv(int *argc, char *argv[], int from)
{
	int i;

	assert(*argc >= from + 1);

	for (i = from + 1; i <= *argc; i++)
		argv[i-1] = argv[i];
	(*argc)--;
	return;
}
