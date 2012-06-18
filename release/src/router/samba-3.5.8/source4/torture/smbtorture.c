/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Andrew Tridgell 1997-2003
   Copyright (C) Jelmer Vernooij 2006-2008
   
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
#include "lib/cmdline/popt_common.h"
#include "system/time.h"
#include "system/wait.h"
#include "system/filesys.h"
#include "system/readline.h"
#include "lib/smbreadline/smbreadline.h"
#include "libcli/libcli.h"
#include "lib/ldb/include/ldb.h"
#include "lib/events/events.h"
#include "dynconfig/dynconfig.h"

#include "torture/smbtorture.h"
#include "../lib/util/dlinklist.h"
#include "librpc/rpc/dcerpc.h"
#include "auth/gensec/gensec.h"
#include "param/param.h"

#include "auth/credentials/credentials.h"

static bool run_matching(struct torture_context *torture,
						 const char *prefix, 
						 const char *expr,
						 struct torture_suite *suite,
						 bool *matched)
{
	bool ret = true;

	if (suite == NULL) {
		struct torture_suite *o;

		for (o = (torture_root == NULL?NULL:torture_root->children); o; o = o->next) {
			if (gen_fnmatch(expr, o->name) == 0) {
				*matched = true;
				reload_charcnv(torture->lp_ctx);
				ret &= torture_run_suite(torture, o);
				continue;
			}

			ret &= run_matching(torture, o->name, expr, o, matched);
		}
	} else {
		char *name;
		struct torture_suite *c;
		struct torture_tcase *t;

		for (c = suite->children; c; c = c->next) {
			asprintf(&name, "%s-%s", prefix, c->name);

			if (gen_fnmatch(expr, name) == 0) {
				*matched = true;
				reload_charcnv(torture->lp_ctx);
				torture->active_testname = talloc_strdup(torture, prefix);
				ret &= torture_run_suite(torture, c);
				free(name);
				continue;
			}
			
			ret &= run_matching(torture, name, expr, c, matched);

			free(name);
		}

		for (t = suite->testcases; t; t = t->next) {
			asprintf(&name, "%s-%s", prefix, t->name);
			if (gen_fnmatch(expr, name) == 0) {
				*matched = true;
				reload_charcnv(torture->lp_ctx);
				torture->active_testname = talloc_strdup(torture, prefix);
				ret &= torture_run_tcase(torture, t);
				talloc_free(torture->active_testname);
			}
			free(name);
		}
	}

	return ret;
}

#define MAX_COLS 80 /* FIXME: Determine this at run-time */

/****************************************************************************
run a specified test or "ALL"
****************************************************************************/
static bool run_test(struct torture_context *torture, const char *name)
{
	bool ret = true;
	bool matched = false;
	struct torture_suite *o;

	if (strequal(name, "ALL")) {
		for (o = torture_root->children; o; o = o->next) {
			ret &= torture_run_suite(torture, o);
		}
		return ret;
	}

	ret = run_matching(torture, NULL, name, NULL, &matched);

	if (!matched) {
		printf("Unknown torture operation '%s'\n", name);
		return false;
	}

	return ret;
}

static bool parse_target(struct loadparm_context *lp_ctx, const char *target)
{
	char *host = NULL, *share = NULL;
	struct dcerpc_binding *binding_struct;
	NTSTATUS status;

	/* see if its a RPC transport specifier */
	if (!smbcli_parse_unc(target, NULL, &host, &share)) {
		status = dcerpc_parse_binding(talloc_autofree_context(), target, &binding_struct);
		if (NT_STATUS_IS_ERR(status)) {
			d_printf("Invalid option: %s is not a valid torture target (share or binding string)\n\n", target);
			return false;
		}
		lp_set_cmdline(lp_ctx, "torture:host", binding_struct->host);
		if (lp_parm_string(lp_ctx, NULL, "torture", "share") == NULL)
			lp_set_cmdline(lp_ctx, "torture:share", "IPC$");
		lp_set_cmdline(lp_ctx, "torture:binding", target);
	} else {
		lp_set_cmdline(lp_ctx, "torture:host", host);
		lp_set_cmdline(lp_ctx, "torture:share", share);
		lp_set_cmdline(lp_ctx, "torture:binding", host);
	}

	return true;
}

static void parse_dns(struct loadparm_context *lp_ctx, const char *dns)
{
	char *userdn, *basedn, *secret;
	char *p, *d;

	/* retrievieng the userdn */
	p = strchr_m(dns, '#');
	if (!p) {
		lp_set_cmdline(lp_ctx, "torture:ldap_userdn", "");
		lp_set_cmdline(lp_ctx, "torture:ldap_basedn", "");
		lp_set_cmdline(lp_ctx, "torture:ldap_secret", "");
		return;
	}
	userdn = strndup(dns, p - dns);
	lp_set_cmdline(lp_ctx, "torture:ldap_userdn", userdn);

	/* retrieve the basedn */
	d = p + 1;
	p = strchr_m(d, '#');
	if (!p) {
		lp_set_cmdline(lp_ctx, "torture:ldap_basedn", "");
		lp_set_cmdline(lp_ctx, "torture:ldap_secret", "");
		return;
	}
	basedn = strndup(d, p - d);
	lp_set_cmdline(lp_ctx, "torture:ldap_basedn", basedn);

	/* retrieve the secret */
	p = p + 1;
	if (!p) {
		lp_set_cmdline(lp_ctx, "torture:ldap_secret", "");
		return;
	}
	secret = strdup(p);
	lp_set_cmdline(lp_ctx, "torture:ldap_secret", secret);

	printf ("%s - %s - %s\n", userdn, basedn, secret);

}

static void print_test_list(void)
{
	struct torture_suite *o;
	struct torture_suite *s;
	struct torture_tcase *t;

	if (torture_root == NULL)
		return;

	for (o = torture_root->children; o; o = o->next) {
		for (s = o->children; s; s = s->next) {
			printf("%s-%s\n", o->name, s->name);
		}

		for (t = o->testcases; t; t = t->next) {
			printf("%s-%s\n", o->name, t->name);
		}
	}
}

_NORETURN_ static void usage(poptContext pc)
{
	struct torture_suite *o;
	struct torture_suite *s;
	struct torture_tcase *t;
	int i;

	poptPrintUsage(pc, stdout, 0);
	printf("\n");

	printf("The binding format is:\n\n");

	printf("  TRANSPORT:host[flags]\n\n");

	printf("  where TRANSPORT is either ncacn_np for SMB, ncacn_ip_tcp for RPC/TCP\n");
	printf("  or ncalrpc for local connections.\n\n");

	printf("  'host' is an IP or hostname or netbios name. If the binding string\n");
	printf("  identifies the server side of an endpoint, 'host' may be an empty\n");
	printf("  string.\n\n");

	printf("  'flags' can include a SMB pipe name if using the ncacn_np transport or\n");
	printf("  a TCP port number if using the ncacn_ip_tcp transport, otherwise they\n");
	printf("  will be auto-determined.\n\n");

	printf("  other recognised flags are:\n\n");

	printf("    sign : enable ntlmssp signing\n");
	printf("    seal : enable ntlmssp sealing\n");
	printf("    connect : enable rpc connect level auth (auth, but no sign or seal)\n");
	printf("    validate: enable the NDR validator\n");
	printf("    print: enable debugging of the packets\n");
	printf("    bigendian: use bigendian RPC\n");
	printf("    padcheck: check reply data for non-zero pad bytes\n\n");

	printf("  For example, these all connect to the samr pipe:\n\n");

	printf("    ncacn_np:myserver\n");
	printf("    ncacn_np:myserver[samr]\n");
	printf("    ncacn_np:myserver[\\pipe\\samr]\n");
	printf("    ncacn_np:myserver[/pipe/samr]\n");
	printf("    ncacn_np:myserver[samr,sign,print]\n");
	printf("    ncacn_np:myserver[\\pipe\\samr,sign,seal,bigendian]\n");
	printf("    ncacn_np:myserver[/pipe/samr,seal,validate]\n");
	printf("    ncacn_np:\n");
	printf("    ncacn_np:[/pipe/samr]\n\n");

	printf("    ncacn_ip_tcp:myserver\n");
	printf("    ncacn_ip_tcp:myserver[1024]\n");
	printf("    ncacn_ip_tcp:myserver[1024,sign,seal]\n\n");

	printf("    ncalrpc:\n\n");

	printf("The UNC format is:\n\n");

	printf("  //server/share\n\n");

	printf("Tests are:");

	if (torture_root == NULL) {
	    printf("NO TESTS LOADED\n");
	    exit(1);
	}

	for (o = torture_root->children; o; o = o->next) {
		printf("\n%s (%s):\n  ", o->description, o->name);

		i = 0;
		for (s = o->children; s; s = s->next) {
			if (i + strlen(o->name) + strlen(s->name) >= (MAX_COLS - 3)) {
				printf("\n  ");
				i = 0;
			}
			i+=printf("%s-%s ", o->name, s->name);
		}

		for (t = o->testcases; t; t = t->next) {
			if (i + strlen(o->name) + strlen(t->name) >= (MAX_COLS - 3)) {
				printf("\n  ");
				i = 0;
			}
			i+=printf("%s-%s ", o->name, t->name);
		}

		if (i) printf("\n");
	}

	printf("\nThe default test is ALL.\n");

	exit(1);
}

_NORETURN_ static void max_runtime_handler(int sig)
{
	DEBUG(0,("maximum runtime exceeded for smbtorture - terminating\n"));
	exit(1);
}

struct timeval last_suite_started;

static void simple_suite_start(struct torture_context *ctx,
			       struct torture_suite *suite)
{
	last_suite_started = timeval_current();
	printf("Running %s\n", suite->name);
}

static void simple_suite_finish(struct torture_context *ctx,
			        struct torture_suite *suite)
{

	printf("%s took %g secs\n\n", suite->name, 
		   timeval_elapsed(&last_suite_started));
}

static void simple_test_result(struct torture_context *context, 
			       enum torture_result res, const char *reason)
{
	switch (res) {
	case TORTURE_OK:
		if (reason)
			printf("OK: %s\n", reason);
		break;
	case TORTURE_FAIL:
		printf("TEST %s FAILED! - %s\n", context->active_test->name, reason);
		break;
	case TORTURE_ERROR:
		printf("ERROR IN TEST %s! - %s\n", context->active_test->name, reason); 
		break;
	case TORTURE_SKIP:
		printf("SKIP: %s - %s\n", context->active_test->name, reason);
		break;
	}
}

static void simple_comment(struct torture_context *test, 
			   const char *comment)
{
	printf("%s", comment);
}

static void simple_warning(struct torture_context *test, 
			   const char *comment)
{
	fprintf(stderr, "WARNING: %s\n", comment);
}

const static struct torture_ui_ops std_ui_ops = {
	.comment = simple_comment,
	.warning = simple_warning,
	.suite_start = simple_suite_start,
	.suite_finish = simple_suite_finish,
	.test_result = simple_test_result
};


static void run_shell(struct torture_context *tctx)
{
	char *cline;
	int argc;
	const char **argv;
	int ret;

	while (1) {
		cline = smb_readline("torture> ", NULL, NULL);

		if (cline == NULL)
			return;
	
		ret = poptParseArgvString(cline, &argc, &argv);
		if (ret != 0) {
			fprintf(stderr, "Error parsing line\n");
			continue;
		}

		if (!strcmp(argv[0], "quit")) {
			return;
		} else if (!strcmp(argv[0], "set")) {
			if (argc < 3) {
				fprintf(stderr, "Usage: set <variable> <value>\n");
			} else {
				char *name = talloc_asprintf(NULL, "torture:%s", argv[1]);
				lp_set_cmdline(tctx->lp_ctx, name, argv[2]);
				talloc_free(name);
			}
		} else if (!strcmp(argv[0], "help")) {
			fprintf(stderr, "Available commands:\n"
							" help - This help command\n"
							" run - Run test\n"
							" set - Change variables\n"
							"\n");
		} else if (!strcmp(argv[0], "run")) {
			if (argc < 2) {
				fprintf(stderr, "Usage: run TEST-NAME [OPTIONS...]\n");
			} else {
				run_test(tctx, argv[1]);
			}
		}
		free(cline);
	}
}

/****************************************************************************
  main program
****************************************************************************/
int main(int argc,char *argv[])
{
	int opt, i;
	bool correct = true;
	int max_runtime=0;
	int argc_new;
	struct torture_context *torture;
	struct torture_results *results;
	const struct torture_ui_ops *ui_ops;
	char **argv_new;
	poptContext pc;
	static const char *target = "other";
	NTSTATUS status;
	int shell = false;
	static const char *ui_ops_name = "subunit";
	const char *basedir = NULL;
	const char *extra_module = NULL;
	static int list_tests = 0;
	int num_extra_users = 0;
	enum {OPT_LOADFILE=1000,OPT_UNCLIST,OPT_TIMELIMIT,OPT_DNS, OPT_LIST,
	      OPT_DANGEROUS,OPT_SMB_PORTS,OPT_ASYNC,OPT_NUMPROGS,
	      OPT_EXTRA_USER,};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"format", 0, POPT_ARG_STRING, &ui_ops_name, 0, "Output format (one of: simple, subunit)", NULL },
		{"smb-ports",	'p', POPT_ARG_STRING, NULL,     OPT_SMB_PORTS,	"SMB ports", 	NULL},
		{"basedir",	  0, POPT_ARG_STRING, &basedir, 0, "base directory", "BASEDIR" },
		{"seed",	  0, POPT_ARG_INT,  &torture_seed, 	0,	"Seed to use for randomizer", 	NULL},
		{"num-progs",	  0, POPT_ARG_INT,  NULL, 	OPT_NUMPROGS,	"num progs",	NULL},
		{"num-ops",	  0, POPT_ARG_INT,  &torture_numops, 	0, 	"num ops",	NULL},
		{"entries",	  0, POPT_ARG_INT,  &torture_entries, 	0,	"entries",	NULL},
		{"loadfile",	  0, POPT_ARG_STRING,	NULL, 	OPT_LOADFILE,	"NBench load file to use", 	NULL},
		{"list", 	  0, POPT_ARG_NONE, &list_tests, 0, "List available tests and exit", NULL },
		{"unclist",	  0, POPT_ARG_STRING,	NULL, 	OPT_UNCLIST,	"unclist", 	NULL},
		{"timelimit",	't', POPT_ARG_INT,	NULL, 	OPT_TIMELIMIT,	"Set time limit (in seconds)", 	NULL},
		{"failures",	'f', POPT_ARG_INT,  &torture_failures, 	0,	"failures", 	NULL},
		{"parse-dns",	'D', POPT_ARG_STRING,	NULL, 	OPT_DNS,	"parse-dns", 	NULL},
		{"dangerous",	'X', POPT_ARG_NONE,	NULL,   OPT_DANGEROUS,
		 "run dangerous tests (eg. wiping out password database)", NULL},
		{"load-module",  0,  POPT_ARG_STRING, &extra_module,     0, "load tests from DSO file",    "SOFILE"},
		{"shell", 		0, POPT_ARG_NONE, &shell, true, "Run shell", NULL},
		{"target", 		'T', POPT_ARG_STRING, &target, 0, "samba3|samba4|other", NULL},
		{"async",       'a', POPT_ARG_NONE,     NULL,   OPT_ASYNC,
		 "run async tests", NULL},
		{"num-async",    0, POPT_ARG_INT,  &torture_numasync,  0,
		 "number of simultaneous async requests", NULL},
		{"maximum-runtime", 0, POPT_ARG_INT, &max_runtime, 0, 
		 "set maximum time for smbtorture to live", "seconds"},
		{"extra-user",   0, POPT_ARG_STRING, NULL, OPT_EXTRA_USER,
		 "extra user credentials", NULL},
		POPT_COMMON_SAMBA
		POPT_COMMON_CONNECTION
		POPT_COMMON_CREDENTIALS
		POPT_COMMON_VERSION
		{ NULL }
	};

	setlinebuf(stdout);

	/* we are never interested in SIGPIPE */
	BlockSignals(true, SIGPIPE);

	pc = poptGetContext("smbtorture", argc, (const char **) argv, long_options, 
			    POPT_CONTEXT_KEEP_FIRST);

	poptSetOtherOptionHelp(pc, "<binding>|<unc> TEST1 TEST2 ...");

	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_LOADFILE:
			lp_set_cmdline(cmdline_lp_ctx, "torture:loadfile", poptGetOptArg(pc));
			break;
		case OPT_UNCLIST:
			lp_set_cmdline(cmdline_lp_ctx, "torture:unclist", poptGetOptArg(pc));
			break;
		case OPT_TIMELIMIT:
			lp_set_cmdline(cmdline_lp_ctx, "torture:timelimit", poptGetOptArg(pc));
			break;
		case OPT_NUMPROGS:
			lp_set_cmdline(cmdline_lp_ctx, "torture:nprocs", poptGetOptArg(pc));
			break;
		case OPT_DNS:
			parse_dns(cmdline_lp_ctx, poptGetOptArg(pc));
			break;
		case OPT_DANGEROUS:
			lp_set_cmdline(cmdline_lp_ctx, "torture:dangerous", "Yes");
			break;
		case OPT_ASYNC:
			lp_set_cmdline(cmdline_lp_ctx, "torture:async", "Yes");
			break;
		case OPT_SMB_PORTS:
			lp_set_cmdline(cmdline_lp_ctx, "smb ports", poptGetOptArg(pc));
			break;
		case OPT_EXTRA_USER:
			{
				char *option = talloc_asprintf(NULL, "torture:extra_user%u",
							       ++num_extra_users);
				char *value = poptGetOptArg(pc);
				lp_set_cmdline(cmdline_lp_ctx, option, value);
				talloc_free(option);
			}
			break;
		default:
			printf("bad command line option\n");
			exit(1);
		}
	}

	if (strcmp(target, "samba3") == 0) {
		lp_set_cmdline(cmdline_lp_ctx, "torture:samba3", "true");
	} else if (strcmp(target, "samba4") == 0) {
		lp_set_cmdline(cmdline_lp_ctx, "torture:samba4", "true");
	} else if (strcmp(target, "w2k8") == 0) {
		lp_set_cmdline(cmdline_lp_ctx, "torture:w2k8", "true");
	} else if (strcmp(target, "win7") == 0) {
		lp_set_cmdline(cmdline_lp_ctx, "torture:win7", "true");
	} else if (strcmp(target, "onefs") == 0) {
		lp_set_cmdline(cmdline_lp_ctx, "torture:sacl_support", "false");
	}

	if (max_runtime) {
		/* this will only work if nobody else uses alarm(),
		   which means it won't work for some tests, but we
		   can't use the event context method we use for smbd
		   as so many tests create their own event
		   context. This will at least catch most cases. */
		signal(SIGALRM, max_runtime_handler);
		alarm(max_runtime);
	}

	if (extra_module != NULL) {
	    init_module_fn fn = load_module(talloc_autofree_context(), poptGetOptArg(pc));

	    if (fn == NULL) 
		d_printf("Unable to load module from %s\n", poptGetOptArg(pc));
	    else {
		status = fn();
		if (NT_STATUS_IS_ERR(status)) {
		    d_printf("Error initializing module %s: %s\n", 
			     poptGetOptArg(pc), nt_errstr(status));
		}
	    }
	} else { 
		torture_init();
	}

	if (list_tests) {
		print_test_list();
		return 0;
	}

	if (torture_seed == 0) {
		torture_seed = time(NULL);
	} 
	printf("Using seed %d\n", torture_seed);
	srandom(torture_seed);

	argv_new = discard_const_p(char *, poptGetArgs(pc));

	argc_new = argc;
	for (i=0; i<argc; i++) {
		if (argv_new[i] == NULL) {
			argc_new = i;
			break;
		}
	}

	if (!(argc_new >= 3 || (shell && argc_new >= 2))) {
		usage(pc);
		exit(1);
	}

	if (!parse_target(cmdline_lp_ctx, argv_new[1])) {
		usage(pc);
		exit(1);
	}

	if (!strcmp(ui_ops_name, "simple")) {
		ui_ops = &std_ui_ops;
	} else if (!strcmp(ui_ops_name, "subunit")) {
		ui_ops = &torture_subunit_ui_ops;
	} else {
		printf("Unknown output format '%s'\n", ui_ops_name);
		exit(1);
	}

	results = torture_results_init(talloc_autofree_context(), ui_ops);

	torture = torture_context_init(s4_event_context_init(NULL), results);
	if (basedir != NULL) {
		if (basedir[0] != '/') {
			fprintf(stderr, "Please specify an absolute path to --basedir\n");
			return 1;
		}
		torture->outputdir = basedir;
	} else {
		char *pwd = talloc_size(torture, PATH_MAX);
		if (!getcwd(pwd, PATH_MAX)) {
			fprintf(stderr, "Unable to determine current working directory\n");
			return 1;
		}
		torture->outputdir = pwd;
	}

	torture->lp_ctx = cmdline_lp_ctx;

	gensec_init(cmdline_lp_ctx);

	if (argc_new == 0) {
		printf("You must specify a test to run, or 'ALL'\n");
	} else if (shell) {
		run_shell(torture);
	} else {
		for (i=2;i<argc_new;i++) {
			if (!run_test(torture, argv_new[i])) {
				correct = false;
			}
		}
	}

	if (torture->results->returncode && correct) {
		return(0);
	} else {
		return(1);
	}
}
