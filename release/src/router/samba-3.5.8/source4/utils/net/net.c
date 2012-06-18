/* 
   Samba Unix/Linux SMB client library 
   Distributed SMB/CIFS Server Management Utility 
   Copyright (C) 2001 Steve French  (sfrench@us.ibm.com)
   Copyright (C) 2001 Jim McDonough (jmcd@us.ibm.com)
   Copyright (C) 2001 Andrew Tridgell (tridge@samba.org)
   Copyright (C) 2001 Andrew Bartlett (abartlet@samba.org)
   Copyright (C) 2004 Stefan Metzmacher (metze@samba.org)

   Largely rewritten by metze in August 2004

   Originally written by Steve and Jim. Largely rewritten by tridge in
   November 2001.

   Reworked again by abartlet in December 2001

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
 
/*****************************************************/
/*                                                   */
/*   Distributed SMB/CIFS Server Management Utility  */
/*                                                   */
/*   The intent was to make the syntax similar       */
/*   to the NET utility (first developed in DOS      */
/*   with additional interesting & useful functions  */
/*   added in later SMB server network operating     */
/*   systems).                                       */
/*                                                   */
/*****************************************************/

#include "includes.h"
#include "utils/net/net.h"
#include "lib/cmdline/popt_common.h"
#include "lib/ldb/include/ldb.h"
#include "librpc/rpc/dcerpc.h"
#include "param/param.h"
#include "lib/events/events.h"
#include "auth/credentials/credentials.h"

/*
  run a function from a function table. If not found then
  call the specified usage function 
*/
int net_run_function(struct net_context *ctx,
			int argc, const char **argv,
			const struct net_functable *functable, 
			int (*usage_fn)(struct net_context *ctx, int argc, const char **argv))
{
	int i;

	if (argc == 0) {
		return usage_fn(ctx, argc, argv);

	} else if (argc == 1 && strequal(argv[0], "help")) {
		return net_help(ctx, functable);
	}

	for (i=0; functable[i].name; i++) {
		if (strcasecmp_m(argv[0], functable[i].name) == 0)
			return functable[i].fn(ctx, argc-1, argv+1);
	}

	d_printf("No command: %s\n", argv[0]);
	return usage_fn(ctx, argc, argv);
}

/*
  run a usage function from a function table. If not found then fail
*/
int net_run_usage(struct net_context *ctx,
			int argc, const char **argv,
			const struct net_functable *functable)
{
	int i;

	for (i=0; functable[i].name; i++) {
		if (strcasecmp_m(argv[0], functable[i].name) == 0)
			if (functable[i].usage) {
				return functable[i].usage(ctx, argc-1, argv+1);
			}
	}

	d_printf("No usage information for command: %s\n", argv[0]);

	return 1;
}


/* main function table */
static const struct net_functable net_functable[] = {
	{"password", "change password\n", net_password, net_password_usage},
	{"time", "get remote server's time\n", net_time, net_time_usage},
	{"join", "join a domain\n", net_join, net_join_usage},
	{"samdump", "dump the sam of a domain\n", net_samdump, net_samdump_usage},
	{"export", "dump the sam of this domain\n", net_export, net_export_usage},
	{"vampire", "join and syncronise an AD domain onto the local server\n", net_vampire, net_vampire_usage},
	{"samsync", "synchronise into the local ldb the sam of an NT4 domain\n", net_samsync_ldb, net_samsync_ldb_usage},
	{"user", "manage user accounts\n", net_user, net_user_usage},
	{"machinepw", "Get a machine password out of our SAM\n", net_machinepw, net_machinepw_usage},
	{NULL, NULL, NULL, NULL}
};

int net_help(struct net_context *ctx, const struct net_functable *ftable)
{
	int i = 0;
	const char *name = ftable[i].name;
	const char *desc = ftable[i].desc;

	d_printf("Available commands:\n");
	while (name && desc) {
		d_printf("\t%s\t\t%s", name, desc);
		name = ftable[++i].name;
		desc = ftable[i].desc;
	}

	return 0;
}

static int net_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Usage:\n");
	d_printf("net <command> [options]\n");
	return 0;
}

/****************************************************************************
  main program
****************************************************************************/
static int binary_net(int argc, const char **argv)
{
	int opt,i;
	int rc;
	int argc_new;
	const char **argv_new;
	struct tevent_context *ev;
	struct net_context *ctx = NULL;
	poptContext pc;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		POPT_COMMON_SAMBA
		POPT_COMMON_CONNECTION
		POPT_COMMON_CREDENTIALS
		POPT_COMMON_VERSION
		{ NULL }
	};

	setlinebuf(stdout);

	pc = poptGetContext("net", argc, (const char **) argv, long_options, 
			    POPT_CONTEXT_KEEP_FIRST);

	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		default:
			d_printf("Invalid option %s: %s\n", 
				 poptBadOption(pc, 0), poptStrerror(opt));
			net_usage(ctx, argc, argv);
			exit(1);
		}
	}

	argv_new = (const char **)poptGetArgs(pc);

	argc_new = argc;
	for (i=0; i<argc; i++) {
		if (argv_new[i] == NULL) {
			argc_new = i;
			break;
		}
	}

	if (argc_new < 2) {
		return net_usage(ctx, argc, argv);
	}

	dcerpc_init(cmdline_lp_ctx);

	ev = s4_event_context_init(NULL);
	if (!ev) {
		d_printf("Failed to create an event context\n");
		exit(1);
	}
	ctx = talloc(ev, struct net_context);
	if (!ctx) {
		d_printf("Failed to talloc a net_context\n");
		exit(1);
	}

	ZERO_STRUCTP(ctx);
	ctx->lp_ctx = cmdline_lp_ctx;
	ctx->credentials = cmdline_credentials;
	ctx->event_ctx = ev;

	rc = net_run_function(ctx, argc_new-1, argv_new+1, net_functable, net_usage);

	if (rc != 0) {
		DEBUG(0,("return code = %d\n", rc));
	}

	talloc_free(ev);
	return rc;
}

 int main(int argc, const char **argv)
{
	return binary_net(argc, argv);
}
