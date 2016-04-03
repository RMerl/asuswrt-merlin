/* Copyright 2000-2002 Joakim Axelsson (gozem@linux.nu)
 *                     Patrick Schaaf (bof@bof.de)
 * Copyright 2003-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h>			/* assert */
#include <ctype.h>			/* isspace */
#include <errno.h>			/* errno */
#include <stdarg.h>			/* va_* */
#include <stdbool.h>			/* bool */
#include <stdio.h>			/* fprintf, fgets */
#include <stdlib.h>			/* exit */
#include <string.h>			/* str* */

#include <config.h>

#include <libipset/debug.h>		/* D() */
#include <libipset/data.h>		/* enum ipset_data */
#include <libipset/parse.h>		/* ipset_parse_* */
#include <libipset/session.h>		/* ipset_session_* */
#include <libipset/types.h>		/* struct ipset_type */
#include <libipset/ui.h>		/* core options, commands */
#include <libipset/utils.h>		/* STREQ */

static char program_name[] = PACKAGE;
static char program_version[] = PACKAGE_VERSION;

static struct ipset_session *session;
static uint32_t restore_line;
static bool interactive;
static char cmdline[1024];
static char *newargv[255];
static int newargc;
static FILE *fd = NULL;
static const char *filename = NULL;

enum exittype {
	NO_PROBLEM = 0,
	OTHER_PROBLEM,
	PARAMETER_PROBLEM,
	VERSION_PROBLEM,
	SESSION_PROBLEM,
};

static int __attribute__((format(printf, 2, 3)))
exit_error(int status, const char *msg, ...)
{
	bool quiet = !interactive &&
		     session &&
		     ipset_envopt_test(session, IPSET_ENV_QUIET);

	if (status && msg && !quiet) {
		va_list args;

		fprintf(stderr, "%s v%s: ", program_name, program_version);
		va_start(args, msg);
		vfprintf(stderr, msg, args);
		va_end(args);
		if (status != SESSION_PROBLEM)
			fprintf(stderr, "\n");

		if (status == PARAMETER_PROBLEM)
			fprintf(stderr,
				"Try `%s help' for more information.\n",
				program_name);
	}
	/* Ignore errors in interactive mode */
	if (status && interactive) {
		if (session)
			ipset_session_report_reset(session);
		return -1;
	}

	if (session)
		ipset_session_fini(session);

	D("status: %u", status);
	if (fd)
		fclose(fd);
	exit(status > VERSION_PROBLEM ? OTHER_PROBLEM : status);
	/* Unreached */
	return -1;
}

static int
handle_error(void)
{
	if (ipset_session_warning(session) &&
	    !ipset_envopt_test(session, IPSET_ENV_QUIET))
		fprintf(stderr, "Warning: %s\n",
			ipset_session_warning(session));
	if (ipset_session_error(session))
		return exit_error(SESSION_PROBLEM, "%s",
				  ipset_session_error(session));

	if (!interactive) {
		ipset_session_fini(session);
		if (fd)
			fclose(fd);
		exit(OTHER_PROBLEM);
	}

	ipset_session_report_reset(session);
	return -1;
}

static void
help(void)
{
	const struct ipset_commands *c;
	const struct ipset_envopts *opt = ipset_envopts;

	printf("%s v%s\n\n"
	       "Usage: %s [options] COMMAND\n\nCommands:\n",
	       program_name, program_version, program_name);

	for (c = ipset_commands; c->cmd; c++)
		printf("%s %s\n", c->name[0], c->help);
	printf("\nOptions:\n");

	while (opt->flag) {
		if (opt->help)
			printf("%s %s\n", opt->name[0], opt->help);
		opt++;
	}
}

int
ipset_parse_file(struct ipset_session *s UNUSED,
		 int opt UNUSED, const char *str)
{
	if (filename != NULL)
		return exit_error(PARAMETER_PROBLEM,
				  "-file option can be specified once");
	filename = str;

	return 0;
}

static
int __attribute__ ((format (printf, 1, 2)))
ipset_print_file(const char *fmt, ...)
{
	int len;
	va_list args;

	assert(fd != NULL);
	va_start(args, fmt);
	len = vfprintf(fd, fmt, args);
	va_end(args);

	return len;
}

/* Build faked argv from parsed line */
static void
build_argv(char *buffer)
{
	char *tmp, *arg;
	int i;
	bool quoted = false;

	/* Reset */
	for (i = 1; i < newargc; i++) {
		if (newargv[i])
			free(newargv[i]);
		newargv[i] = NULL;
	}
	newargc = 1;

	arg = calloc(strlen(buffer) + 1, sizeof(*buffer));
	for (tmp = buffer, i = 0; *tmp; tmp++) {
		if ((newargc + 1) == (int)(sizeof(newargv)/sizeof(char *))) {
			exit_error(PARAMETER_PROBLEM,
				   "Line is too long to parse.");
			return;
		}
		switch (*tmp) {
		case '"':
			quoted = !quoted;
			if (*(tmp+1))
				continue;
			break;
		case ' ':
		case '\r':
		case '\n':
		case '\t':
			if (!quoted)
				break;
			arg[i++] = *tmp;
			continue;
		default:
			arg[i++] = *tmp;
			if (*(tmp+1))
				continue;
			break;
		}
		if (!*(tmp+1) && quoted) {
			exit_error(PARAMETER_PROBLEM, "Missing close quote!");
			return;
		}
		if (!*arg)
			continue;
		newargv[newargc] = calloc(strlen(arg) + 1, sizeof(*arg));
		ipset_strlcpy(newargv[newargc++], arg, strlen(arg) + 1);
		memset(arg, 0, strlen(arg) + 1);
		i = 0;
	}
	free(arg);
}

/* Main parser function, workhorse */
int parse_commandline(int argc, char *argv[]);

/*
 * Performs a restore from stdin
 */
static int
restore(char *argv0)
{
	int ret = 0;
	char *c;
	FILE *rfd = stdin;

	/* Initialize newargv/newargc */
	newargc = 0;
	newargv[newargc] = calloc(strlen(argv0) + 1, sizeof(*argv0));
	ipset_strlcpy(newargv[newargc++], argv0, strlen(argv0) + 1);
	if (filename) {
		fd = fopen(filename, "r");
		if (!fd) {
			return exit_error(OTHER_PROBLEM,
					  "Cannot open %s for reading: %s",
					  filename, strerror(errno));
		}
		rfd = fd;
	}

	while (fgets(cmdline, sizeof(cmdline), rfd)) {
		restore_line++;
		c = cmdline;
		while (isspace(c[0]))
			c++;
		if (c[0] == '\0' || c[0] == '#')
			continue;
		else if (STREQ(c, "COMMIT\n") || STREQ(c, "COMMIT\r\n")) {
			ret = ipset_commit(session);
			if (ret < 0)
				handle_error();
			continue;
		}
		/* Build faked argv, argc */
		build_argv(c);

		/* Execute line */
		ret = parse_commandline(newargc, newargv);
		if (ret < 0)
			handle_error();
	}
	/* implicit "COMMIT" at EOF */
	ret = ipset_commit(session);
	if (ret < 0)
		handle_error();

	free(newargv[0]);
	return ret;
}

static bool do_parse(const struct ipset_arg *arg, bool family)
{
	return !((family == true) ^ (arg->opt == IPSET_OPT_FAMILY));
}

static int
call_parser(int *argc, char *argv[], const struct ipset_arg *args, bool family)
{
	int ret = 0, i = 1;
	const struct ipset_arg *arg;
	const char *optstr;

	/* Currently CREATE and ADT may have got additional arguments */
	if (!args && *argc > 1)
		goto err_unknown;
	while (*argc > i) {
		ret = -1;
		for (arg = args; arg->opt; arg++) {
			D("argc: %u, %s vs %s", i, argv[i], arg->name[0]);
			if (!(ipset_match_option(argv[i], arg->name)))
				continue;

			optstr = argv[i];
			/* Matched option */
			D("match %s, argc %u, i %u, %s",
			  arg->name[0], *argc, i + 1,
			  do_parse(arg, family) ? "parse" : "skip");
			i++;
			ret = 0;
			switch (arg->has_arg) {
			case IPSET_MANDATORY_ARG:
				if (*argc - i < 1)
					return exit_error(PARAMETER_PROBLEM,
						"Missing mandatory argument "
						"of option `%s'",
						arg->name[0]);
				/* Fall through */
			case IPSET_OPTIONAL_ARG:
				if (*argc - i >= 1) {
					if (do_parse(arg, family)) {
						ret = ipset_call_parser(
							session, arg, argv[i]);
						if (ret < 0)
							return ret;
					}
					i++;
					break;
				}
				/* Fall through */
			default:
				if (do_parse(arg, family)) {
					ret = ipset_call_parser(
						session, arg, optstr);
					if (ret < 0)
						return ret;
				}
			}
			break;
		}
		if (ret < 0)
			goto err_unknown;
	}
	if (!family)
		*argc = 0;
	return ret;

err_unknown:
	return exit_error(PARAMETER_PROBLEM, "Unknown argument: `%s'", argv[i]);
}

static enum ipset_adt
cmd2cmd(int cmd)
{
	switch (cmd) {
	case IPSET_CMD_ADD:
		return IPSET_ADD;
	case IPSET_CMD_DEL:
		return IPSET_DEL;
	case IPSET_CMD_TEST:
		return IPSET_TEST;
	case IPSET_CMD_CREATE:
		return IPSET_CREATE;
	default:
		return 0;
	}
}

static void
check_mandatory(const struct ipset_type *type, enum ipset_cmd command)
{
	enum ipset_adt cmd = cmd2cmd(command);
	uint64_t flags = ipset_data_flags(ipset_session_data(session));
	uint64_t mandatory = type->mandatory[cmd];
	const struct ipset_arg *arg = type->args[cmd];

	/* Range can be expressed by ip/cidr */
	if (flags & IPSET_FLAG(IPSET_OPT_CIDR))
		flags |= IPSET_FLAG(IPSET_OPT_IP_TO);

	mandatory &= ~flags;
	if (!mandatory)
		return;
	if (!arg) {
		exit_error(OTHER_PROBLEM,
			"There are missing mandatory flags "
			"but can't check them. "
			"It's a bug, please report the problem.");
		return;
	}

	for (; arg->opt; arg++)
		if (mandatory & IPSET_FLAG(arg->opt)) {
			exit_error(PARAMETER_PROBLEM,
				   "Mandatory option `%s' is missing",
				   arg->name[0]);
			return;
		}
}

static const char *
cmd2name(enum ipset_cmd cmd)
{
	const struct ipset_commands *c;

	for (c = ipset_commands; c->cmd; c++)
		if (cmd == c->cmd)
			return c->name[0];
	return "unknown command";
}

static const char *
session_family(void)
{
	switch (ipset_data_family(ipset_session_data(session))) {
	case NFPROTO_IPV4:
		return "inet";
	case NFPROTO_IPV6:
		return "inet6";
	default:
		return "unspec";
	}
}

static void
check_allowed(const struct ipset_type *type, enum ipset_cmd command)
{
	uint64_t flags = ipset_data_flags(ipset_session_data(session));
	enum ipset_adt cmd = cmd2cmd(command);
	uint64_t allowed = type->full[cmd];
	uint64_t cmdflags = command == IPSET_CMD_CREATE
				? IPSET_CREATE_FLAGS : IPSET_ADT_FLAGS;
	const struct ipset_arg *arg = type->args[cmd];
	enum ipset_opt i;

	/* Range can be expressed by ip/cidr or from-to */
	if (allowed & IPSET_FLAG(IPSET_OPT_IP_TO))
		allowed |= IPSET_FLAG(IPSET_OPT_CIDR);

	for (i = IPSET_OPT_IP; i < IPSET_OPT_FLAGS; i++) {
		if (!(cmdflags & IPSET_FLAG(i)) ||
		    (allowed & IPSET_FLAG(i)) ||
		    !(flags & IPSET_FLAG(i)))
			continue;
		/* Not allowed element-expressions */
		switch (i) {
		case IPSET_OPT_CIDR:
			exit_error(OTHER_PROBLEM,
				"IP/CIDR range is not allowed in command %s "
				"with set type %s and family %s",
				cmd2name(command), type->name,
				session_family());
			return;
		case IPSET_OPT_IP_TO:
			exit_error(OTHER_PROBLEM,
				"FROM-TO IP range is not allowed in command %s "
				"with set type %s and family %s",
				cmd2name(command), type->name,
				session_family());
			return;
		case IPSET_OPT_PORT_TO:
			exit_error(OTHER_PROBLEM,
				"FROM-TO port range is not allowed in command %s "
				"with set type %s and family %s",
				cmd2name(command), type->name,
				session_family());
			return;
		default:
			break;
		}
		/* Other options */
		if (!arg) {
			exit_error(OTHER_PROBLEM,
				"There are not allowed options (%u) "
				"but option list is NULL. "
				"It's a bug, please report the problem.", i);
			return;
		}
		for (; arg->opt; arg++) {
			if (arg->opt != i)
				continue;
			exit_error(OTHER_PROBLEM,
				"%s parameter is not allowed in command %s "
				"with set type %s and family %s",
				arg->name[0],
				cmd2name(command), type->name,
				session_family());
			return;
		}
		exit_error(OTHER_PROBLEM,
			"There are not allowed options (%u) "
			"but can't resolve them. "
			"It's a bug, please report the problem.", i);
		return;
	}
}

static const struct ipset_type *
type_find(const char *name)
{
	const struct ipset_type *t = ipset_types();

	while (t) {
		if (ipset_match_typename(name, t))
			return t;
		t = t->next;
	}
	return NULL;
}

/* Workhorse */
int
parse_commandline(int argc, char *argv[])
{
	int ret = 0;
	enum ipset_cmd cmd = IPSET_CMD_NONE;
	int i;
	char *arg0 = NULL, *arg1 = NULL, *c;
	const struct ipset_envopts *opt;
	const struct ipset_commands *command;
	const struct ipset_type *type;

	/* Set session lineno to report parser errors correctly */
	ipset_session_lineno(session, restore_line);

	/* Commandline parsing, somewhat similar to that of 'ip' */

	/* First: parse core options */
	for (opt = ipset_envopts; opt->flag; opt++) {
		for (i = 1; i < argc; ) {
			if (!ipset_match_envopt(argv[i], opt->name)) {
				i++;
				continue;
			}
			/* Shift off matched option */
			ipset_shift_argv(&argc, argv, i);
			switch (opt->has_arg) {
			case IPSET_MANDATORY_ARG:
				if (i + 1 > argc)
					return exit_error(PARAMETER_PROBLEM,
						"Missing mandatory argument "
						"to option %s",
						opt->name[0]);
				/* Fall through */
			case IPSET_OPTIONAL_ARG:
				if (i + 1 <= argc) {
					ret = opt->parse(session, opt->flag,
							 argv[i]);
					if (ret < 0)
						return handle_error();
					ipset_shift_argv(&argc, argv, i);
				}
				break;
			case IPSET_NO_ARG:
				ret = opt->parse(session, opt->flag,
						 opt->name[0]);
				if (ret < 0)
					return handle_error();
				break;
			default:
				break;
			}
		}
	}

	/* Second: parse command */
	for (command = ipset_commands;
		 argc > 1 && command->cmd && cmd == IPSET_CMD_NONE;
	     command++) {
		if (!ipset_match_cmd(argv[1], command->name))
			continue;

		if (restore_line != 0 &&
		    (command->cmd == IPSET_CMD_RESTORE ||
		     command->cmd == IPSET_CMD_VERSION ||
		     command->cmd == IPSET_CMD_HELP))
			return exit_error(PARAMETER_PROBLEM,
				"Command `%s' is invalid "
				"in restore mode.",
				command->name[0]);
		if (interactive && command->cmd == IPSET_CMD_RESTORE) {
			printf("Restore command ignored "
			       "in interactive mode\n");
			return 0;
		}

		/* Shift off matched command arg */
		ipset_shift_argv(&argc, argv, 1);
		cmd = command->cmd;
		switch (command->has_arg) {
		case IPSET_MANDATORY_ARG:
		case IPSET_MANDATORY_ARG2:
			if (argc < 2)
				return exit_error(PARAMETER_PROBLEM,
					"Missing mandatory argument "
					"to command %s",
					command->name[0]);
			/* Fall through */
		case IPSET_OPTIONAL_ARG:
			arg0 = argv[1];
			if (argc >= 2)
				/* Shift off first arg */
				ipset_shift_argv(&argc, argv, 1);
			break;
		default:
			break;
		}
		if (command->has_arg == IPSET_MANDATORY_ARG2) {
			if (argc < 2)
				return exit_error(PARAMETER_PROBLEM,
					"Missing second mandatory "
					"argument to command %s",
					command->name[0]);
			arg1 = argv[1];
			/* Shift off second arg */
			ipset_shift_argv(&argc, argv, 1);
		}
		break;
	}

	/* Third: catch interactive mode, handle help, version */
	switch (cmd) {
	case IPSET_CMD_NONE:
		if (interactive) {
			printf("No command specified\n");
			if (session)
				ipset_envopt_parse(session, 0, "reset");
			return 0;
		}
		if (argc > 1 && STREQ(argv[1], "-")) {
			interactive = true;
			printf("%s> ", program_name);
			/* Initialize newargv/newargc */
			newargv[newargc++] = program_name;
			while (fgets(cmdline, sizeof(cmdline), stdin)) {
				c = cmdline;
				while (isspace(c[0]))
					c++;
				if (c[0] == '\0' || c[0] == '#') {
					printf("%s> ", program_name);
					continue;
				}
				/* Build fake argv, argc */
				build_argv(c);
				/* Execute line: ignore soft errors */
				if (parse_commandline(newargc, newargv) < 0)
					handle_error();
				printf("%s> ", program_name);
			}
			return exit_error(NO_PROBLEM, NULL);
		}
		if (argc > 1)
			return exit_error(PARAMETER_PROBLEM,
				"No command specified: unknown argument %s",
				argv[1]);
		return exit_error(PARAMETER_PROBLEM, "No command specified.");
	case IPSET_CMD_VERSION:
		printf("%s v%s, protocol version: %u\n",
		       program_name, program_version, IPSET_PROTOCOL);
		if (interactive)
			return 0;
		return exit_error(NO_PROBLEM, NULL);
	case IPSET_CMD_HELP:
		help();

		if (interactive ||
		    !ipset_envopt_test(session, IPSET_ENV_QUIET)) {
			if (arg0) {
				/* Type-specific help, without kernel checking */
				type = type_find(arg0);
				if (!type)
					return exit_error(PARAMETER_PROBLEM,
						"Unknown settype: `%s'", arg0);
				printf("\n%s type specific options:\n\n%s",
				       type->name, type->usage);
				if (type->usagefn)
					type->usagefn();
				if (type->family == NFPROTO_UNSPEC)
					printf("\nType %s is family neutral.\n",
					       type->name);
				else if (type->family == NFPROTO_IPSET_IPV46)
					printf("\nType %s supports INET "
					       "and INET6.\n",
					       type->name);
				else
					printf("\nType %s supports family "
					       "%s only.\n",
					       type->name,
					       type->family == NFPROTO_IPV4
						? "INET" : "INET6");
			} else {
				printf("\nSupported set types:\n");
				type = ipset_types();
				while (type) {
					printf("    %s\t%s%u\t%s\n",
					       type->name,
					       strlen(type->name) < 12 ? "\t" : "",
					       type->revision,
					       type->description);
					type = type->next;
				}
			}
		}
		if (interactive)
			return 0;
		return exit_error(NO_PROBLEM, NULL);
	case IPSET_CMD_QUIT:
		return exit_error(NO_PROBLEM, NULL);
	default:
		break;
	}

	/* Forth: parse command args and issue the command */
	switch (cmd) {
	case IPSET_CMD_CREATE:
		/* Args: setname typename [type specific options] */
		ret = ipset_parse_setname(session, IPSET_SETNAME, arg0);
		if (ret < 0)
			return handle_error();

		ret = ipset_parse_typename(session, IPSET_OPT_TYPENAME, arg1);
		if (ret < 0)
			return handle_error();

		type = ipset_type_get(session, cmd);
		if (type == NULL)
			return handle_error();

		/* Parse create options: first check INET family */
		ret = call_parser(&argc, argv, type->args[IPSET_CREATE], true);
		if (ret < 0)
			return handle_error();
		else if (ret)
			return ret;

		/* Parse create options: then check all options */
		ret = call_parser(&argc, argv, type->args[IPSET_CREATE], false);
		if (ret < 0)
			return handle_error();
		else if (ret)
			return ret;

		/* Check mandatory, then allowed options */
		check_mandatory(type, cmd);
		check_allowed(type, cmd);

		break;
	case IPSET_CMD_LIST:
	case IPSET_CMD_SAVE:
		if (filename != NULL) {
			fd = fopen(filename, "w");
			if (!fd)
				return exit_error(OTHER_PROBLEM,
						  "Cannot open %s for writing: "
						  "%s", filename,
						  strerror(errno));
			ipset_session_outfn(session, ipset_print_file);
		}
	case IPSET_CMD_DESTROY:
	case IPSET_CMD_FLUSH:
		/* Args: [setname] */
		if (arg0) {
			ret = ipset_parse_setname(session,
						  IPSET_SETNAME, arg0);
			if (ret < 0)
				return handle_error();
		}
		break;

	case IPSET_CMD_RENAME:
	case IPSET_CMD_SWAP:
		/* Args: from-setname to-setname */
		ret = ipset_parse_setname(session, IPSET_SETNAME, arg0);
		if (ret < 0)
			return handle_error();
		ret = ipset_parse_setname(session, IPSET_OPT_SETNAME2, arg1);
		if (ret < 0)
			return handle_error();
		break;

	case IPSET_CMD_RESTORE:
		/* Restore mode */
		if (argc > 1)
			return exit_error(PARAMETER_PROBLEM,
				"Unknown argument %s", argv[1]);
		return restore(argv[0]);
	case IPSET_CMD_ADD:
	case IPSET_CMD_DEL:
	case IPSET_CMD_TEST:
		D("ADT: setname %s", arg0);
		/* Args: setname ip [options] */
		ret = ipset_parse_setname(session, IPSET_SETNAME, arg0);
		if (ret < 0)
			return handle_error();

		type = ipset_type_get(session, cmd);
		if (type == NULL)
			return handle_error();

		ret = ipset_parse_elem(session, type->last_elem_optional, arg1);
		if (ret < 0)
			return handle_error();

		/* Parse additional ADT options */
		ret = call_parser(&argc, argv, type->args[cmd2cmd(cmd)], false);
		if (ret < 0)
			return handle_error();
		else if (ret)
			return ret;

		/* Check mandatory, then allowed options */
		check_mandatory(type, cmd);
		check_allowed(type, cmd);

		break;
	default:
		break;
	}

	if (argc > 1)
		return exit_error(PARAMETER_PROBLEM,
			"Unknown argument %s", argv[1]);
	ret = ipset_cmd(session, cmd, restore_line);
	D("ret %d", ret);
	/* Special case for TEST and non-quiet mode */
	if (cmd == IPSET_CMD_TEST && ipset_session_warning(session)) {
		if (!ipset_envopt_test(session, IPSET_ENV_QUIET))
			fprintf(stderr, "%s", ipset_session_warning(session));
		ipset_session_report_reset(session);
	}
	if (ret < 0)
		handle_error();

	return ret;
}

int
main(int argc, char *argv[])
{
	int ret;

	/* Load set types */
	ipset_load_types();

	/* Initialize session */
	session = ipset_session_init(printf);
	if (session == NULL)
		return exit_error(OTHER_PROBLEM,
			"Cannot initialize ipset session, aborting.");

	ret = parse_commandline(argc, argv);

	ipset_session_fini(session);
	if (fd)
		fclose(fd);

	return ret;
}
