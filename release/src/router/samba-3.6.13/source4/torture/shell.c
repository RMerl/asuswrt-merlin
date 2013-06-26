/*
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Andrew Tridgell 1997-2003
   Copyright (C) Jelmer Vernooij 2006-2008
   Copyright (C) James Peach 2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/readline.h"
#include "../libcli/smbreadline/smbreadline.h"
#include "lib/cmdline/popt_common.h"
#include "auth/credentials/credentials.h"
#include "torture/smbtorture.h"
#include "param/param.h"

struct shell_command;

typedef void (*shell_function)(const struct shell_command *,
	struct torture_context *, int, const char **);

static void shell_quit(const struct shell_command *,
	struct torture_context *, int, const char **);
static void shell_help(const struct shell_command *,
	struct torture_context *, int, const char **);
static void shell_set(const struct shell_command *,
	struct torture_context *, int, const char **);
static void shell_run(const struct shell_command *,
	struct torture_context *, int, const char **);
static void shell_list(const struct shell_command *,
	struct torture_context *, int, const char **);
static void shell_auth(const struct shell_command *,
	struct torture_context *, int, const char **);
static void shell_target(const struct shell_command *,
	struct torture_context *, int, const char **);

static void shell_usage(const struct shell_command *);
static bool match_command(const char *, const struct shell_command *);

struct shell_command
{
    shell_function  handler;
    const char *    name;
    const char *    usage;
    const char *    help;
} shell_command;

static const struct shell_command commands[] =
{
    {
	shell_auth, "auth",
	"[[username | principal | domain | realm | password] STRING]",
	"set authentication parameters"
    },

    {
	shell_help, "help", NULL,
	"print this help message"
    },

    {
	shell_list, "list", NULL,
	"list the available tests"
    },

    {
	shell_quit, "quit", NULL,
	"exit smbtorture"
    },

    {
	shell_run, "run", "[TESTNAME]",
	"run the specified test"
    },

    {
	shell_set, "set", "[NAME VALUE]",
	"print or set test configuration parameters"
    },

    {
	shell_target, "target", "[TARGET]",
	"print or set the test target"
    }

};

void torture_shell(struct torture_context *tctx)
{
	char *cline;
	int argc;
	const char **argv;
	int ret;
	int i;

	/* If we don't have a specified password, specify it as empty. This
	 * stops the credentials system prompting when we use the "auth"
	 * command to display the current auth parameters.
	 */
	if (cmdline_credentials->password_obtained != CRED_SPECIFIED) {
	    cli_credentials_set_password(cmdline_credentials, "",
		    CRED_SPECIFIED);
	}

	while (1) {
		cline = smb_readline("torture> ", NULL, NULL);

		if (cline == NULL)
			return;

#if HAVE_ADD_HISTORY
		add_history(cline);
#endif

		ret = poptParseArgvString(cline, &argc, &argv);
		if (ret != 0) {
			fprintf(stderr, "Error parsing line\n");
			continue;
		}

		for (i = 0; i < ARRAY_SIZE(commands); i++) {
			if (match_command(argv[0], &commands[i])) {
				argc--;
				argv++;
				commands[i].handler(&commands[i],
					tctx, argc, argv);
				break;
			}
		}

		free(cline);
	}
}

static void shell_quit(const struct shell_command * command,
	struct torture_context *tctx, int argc, const char **argv)
{
    exit(0);
}

static void shell_help(const struct shell_command * command,
	struct torture_context *tctx, int argc, const char **argv)
{
    int i;

    if (argc == 1) {
	    for (i = 0; i < ARRAY_SIZE(commands); i++) {
		    if (match_command(argv[0], &commands[i])) {
			    shell_usage(&commands[i]);
			    return;
		    }
	    }
    } else {
	    fprintf(stdout, "Available commands:\n");
	    for (i = 0; i < ARRAY_SIZE(commands); i++) {
		    fprintf(stdout, "\t%s - %s\n",
			    commands[i].name, commands[i].help);
	    }
    }
}

static void shell_set(const struct shell_command *command,
	struct torture_context *tctx, int argc, const char **argv)
{
	switch (argc) {
	case 0:
	    lpcfg_dump(tctx->lp_ctx, stdout,
		    false /* show_defaults */,
		    0 /* skip services */);
	    break;

	case 2:
	    /* We want to allow users to set any config option. Top level
	     * options will get checked against their static definition, but
	     * parametric options can't be checked and will just get stashed
	     * as they are provided.
	     */
	    lpcfg_set_cmdline(tctx->lp_ctx, argv[0], argv[1]);
	    break;

	default:
	    shell_usage(command);
	}
}

static void shell_run(const struct shell_command * command,
	struct torture_context *tctx, int argc, const char **argv)
{
    if (argc != 1) {
	shell_usage(command);
	return;
    }

    torture_run_named_tests(tctx, argv[0], NULL /* restricted */);
}

static void shell_list(const struct shell_command * command,
	struct torture_context *tctx, int argc, const char **argv)
{
    if (argc != 0) {
	shell_usage(command);
	return;
    }

    torture_print_testsuites(true);
}

static void shell_auth(const struct shell_command * command,
	struct torture_context *tctx, int argc, const char **argv)
{

    if (argc == 0) {
	    const char * username;
	    const char * domain;
	    const char * realm;
	    const char * password;
	    const char * principal;

	    username = cli_credentials_get_username(cmdline_credentials);
	    principal = cli_credentials_get_principal(cmdline_credentials, tctx);
	    domain = cli_credentials_get_domain(cmdline_credentials);
	    realm = cli_credentials_get_realm(cmdline_credentials);
	    password = cli_credentials_get_password(cmdline_credentials);

	    printf("Username: %s\n", username ? username : "");
	    printf("User Principal: %s\n", principal ? principal : "");
	    printf("Domain: %s\n", domain ? domain : "");
	    printf("Realm: %s\n", realm ? realm : "");
	    printf("Password: %s\n", password ? password : "");
    } else if (argc == 2) {
	    bool result;

	    if (!strcmp(argv[0], "username")) {
		    result = cli_credentials_set_username(
			cmdline_credentials, argv[1], CRED_SPECIFIED);
	    } else if (!strcmp(argv[0], "principal")) {
		    result = cli_credentials_set_principal(
			cmdline_credentials, argv[1], CRED_SPECIFIED);
	    } else if (!strcmp(argv[0], "domain")) {
		    result = cli_credentials_set_domain(
			cmdline_credentials, argv[1], CRED_SPECIFIED);
	    } else if (!strcmp(argv[0], "realm")) {
		    result = cli_credentials_set_realm(
			cmdline_credentials, argv[1], CRED_SPECIFIED);
	    } else if (!strcmp(argv[0], "password")) {
		    result = cli_credentials_set_password(
			cmdline_credentials, argv[1], CRED_SPECIFIED);
	    } else {
		    shell_usage(command);
		    return;
	    }

	    if (!result) {
		    printf("failed to set %s\n", argv[0]);
	    }
    } else {
	    shell_usage(command);
    }

}

static void shell_target(const struct shell_command *command,
	struct torture_context *tctx, int argc, const char **argv)
{
	if (argc == 0) {
		const char * host;
		const char * share;
		const char * binding;

		host = torture_setting_string(tctx, "host", NULL);
		share = torture_setting_string(tctx, "share", NULL);
		binding = torture_setting_string(tctx, "binding", NULL);

		printf("Target host: %s\n", host ? host : "");
		printf("Target share: %s\n", share ? share : "");
		printf("Target binding: %s\n", binding ? binding : "");
	} else if (argc == 1) {
		torture_parse_target(tctx->lp_ctx, argv[0]);
	} else {
		shell_usage(command);
	}
}

static void shell_usage(const struct shell_command * command)
{
    if (command->usage) {
	    fprintf(stderr, "Usage: %s %s\n",
		    command->name, command->usage);
    } else {
	    fprintf(stderr, "Usage: %s\n",
		    command->name);
    }
}

static bool match_command(const char * name,
	const struct shell_command * command)
{
	if (!strcmp(name, command->name)) {
		return true;
	}

	if (name[0] == command->name[0] && name[1] == '\0') {
	    return true;
	}

	return false;
}
