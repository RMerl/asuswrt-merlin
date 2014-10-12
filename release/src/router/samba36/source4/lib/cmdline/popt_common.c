/* 
   Unix SMB/CIFS implementation.
   Common popt routines

   Copyright (C) Tim Potter 2001,2002
   Copyright (C) Jelmer Vernooij 2002,2003,2005

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
#include "version.h"
#include "lib/cmdline/popt_common.h"
#include "param/param.h"

/* Handle command line options:
 *		-d,--debuglevel 
 *		-s,--configfile 
 *		-O,--socket-options 
 *		-V,--version
 *		-l,--log-base
 *		-n,--netbios-name
 *		-W,--workgroup
 *		--realm
 *		-i,--scope
 */

enum {OPT_OPTION=1,OPT_LEAK_REPORT,OPT_LEAK_REPORT_FULL,OPT_DEBUG_STDERR};

struct cli_credentials *cmdline_credentials = NULL;
struct loadparm_context *cmdline_lp_ctx = NULL;

static void popt_version_callback(poptContext con,
			   enum poptCallbackReason reason,
			   const struct poptOption *opt,
			   const char *arg, const void *data)
{
	switch(opt->val) {
	case 'V':
		printf("Version %s\n", SAMBA_VERSION_STRING );
		exit(0);
	}
}

static void popt_s4_talloc_log_fn(const char *message)
{
	DEBUG(0,("%s", message));
}

static void popt_samba_callback(poptContext con, 
			   enum poptCallbackReason reason,
			   const struct poptOption *opt,
			   const char *arg, const void *data)
{
	const char *pname;

	if (reason == POPT_CALLBACK_REASON_POST) {
		if (lpcfg_configfile(cmdline_lp_ctx) == NULL) {
			lpcfg_load_default(cmdline_lp_ctx);
		}
		/* Hook any 'every Samba program must do this, after
		 * the smb.conf is setup' functions here */
		return;
	}

	/* Find out basename of current program */
	pname = strrchr_m(poptGetInvocationName(con),'/');

	if (!pname)
		pname = poptGetInvocationName(con);
	else 
		pname++;

	if (reason == POPT_CALLBACK_REASON_PRE) {
		cmdline_lp_ctx = loadparm_init_global(false);

		/* Hook for 'almost the first thing to do in a samba program' here */
		/* setup for panics */
		fault_setup(poptGetInvocationName(con));

		/* and logging */
		setup_logging(pname, DEBUG_STDOUT);
		talloc_set_log_fn(popt_s4_talloc_log_fn);
		talloc_set_abort_fn(smb_panic);

		return;
	}

	switch(opt->val) {

	case OPT_LEAK_REPORT:
		talloc_enable_leak_report();
		break;

	case OPT_LEAK_REPORT_FULL:
		talloc_enable_leak_report_full();
		break;

	case OPT_OPTION:
		if (!lpcfg_set_option(cmdline_lp_ctx, arg)) {
			fprintf(stderr, "Error setting option '%s'\n", arg);
			exit(1);
		}
		break;

	case 'd':
		lpcfg_set_cmdline(cmdline_lp_ctx, "log level", arg);
		break;

	case OPT_DEBUG_STDERR:
		setup_logging(pname, DEBUG_STDERR);
		break;

	case 's':
		if (arg) {
			lpcfg_load(cmdline_lp_ctx, arg);
		}
		break;

	case 'l':
		if (arg) {
			char *new_logfile = talloc_asprintf(NULL, "%s/log.%s", arg, pname);
			lpcfg_set_cmdline(cmdline_lp_ctx, "log file", new_logfile);
			talloc_free(new_logfile);
		}
		break;
	

	}

}


static void popt_common_callback(poptContext con, 
			   enum poptCallbackReason reason,
			   const struct poptOption *opt,
			   const char *arg, const void *data)
{
	struct loadparm_context *lp_ctx = cmdline_lp_ctx;

	switch(opt->val) {
	case 'O':
		if (arg) {
			lpcfg_set_cmdline(lp_ctx, "socket options", arg);
		}
		break;
	
	case 'W':
		lpcfg_set_cmdline(lp_ctx, "workgroup", arg);
		break;

	case 'r':
		lpcfg_set_cmdline(lp_ctx, "realm", arg);
		break;
		
	case 'n':
		lpcfg_set_cmdline(lp_ctx, "netbios name", arg);
		break;
		
	case 'i':
		lpcfg_set_cmdline(lp_ctx, "netbios scope", arg);
		break;

	case 'm':
		lpcfg_set_cmdline(lp_ctx, "client max protocol", arg);
		break;

	case 'R':
		lpcfg_set_cmdline(lp_ctx, "name resolve order", arg);
		break;

	case 'S':
		lpcfg_set_cmdline(lp_ctx, "client signing", arg);
		break;

	}
}

struct poptOption popt_common_connection[] = {
	{ NULL, 0, POPT_ARG_CALLBACK, (void *)popt_common_callback },
	{ "name-resolve", 'R', POPT_ARG_STRING, NULL, 'R', "Use these name resolution services only", "NAME-RESOLVE-ORDER" },
	{ "socket-options", 'O', POPT_ARG_STRING, NULL, 'O', "socket options to use", "SOCKETOPTIONS" },
	{ "netbiosname", 'n', POPT_ARG_STRING, NULL, 'n', "Primary netbios name", "NETBIOSNAME" },
	{ "signing", 'S', POPT_ARG_STRING, NULL, 'S', "Set the client signing state", "on|off|required" },
	{ "workgroup", 'W', POPT_ARG_STRING, NULL, 'W', "Set the workgroup name", "WORKGROUP" },
	{ "realm", 0, POPT_ARG_STRING, NULL, 'r', "Set the realm name", "REALM" },
	{ "scope", 'i', POPT_ARG_STRING, NULL, 'i', "Use this Netbios scope", "SCOPE" },
	{ "maxprotocol", 'm', POPT_ARG_STRING, NULL, 'm', "Set max protocol level", "MAXPROTOCOL" },
	{ NULL }
};

struct poptOption popt_common_samba[] = {
	{ NULL, 0, POPT_ARG_CALLBACK|POPT_CBFLAG_PRE|POPT_CBFLAG_POST, (void *)popt_samba_callback },
	{ "debuglevel",   'd', POPT_ARG_STRING, NULL, 'd', "Set debug level", "DEBUGLEVEL" },
	{ "debug-stderr", 0, POPT_ARG_NONE, NULL, OPT_DEBUG_STDERR, "Send debug output to STDERR", NULL },
	{ "configfile",   's', POPT_ARG_STRING, NULL, 's', "Use alternative configuration file", "CONFIGFILE" },
	{ "option",         0, POPT_ARG_STRING, NULL, OPT_OPTION, "Set smb.conf option from command line", "name=value" },
	{ "log-basename", 'l', POPT_ARG_STRING, NULL, 'l', "Basename for log/debug files", "LOGFILEBASE" },
	{ "leak-report",     0, POPT_ARG_NONE, NULL, OPT_LEAK_REPORT, "enable talloc leak reporting on exit", NULL },	
	{ "leak-report-full",0, POPT_ARG_NONE, NULL, OPT_LEAK_REPORT_FULL, "enable full talloc leak reporting on exit", NULL },
	{ NULL }
};

struct poptOption popt_common_version[] = {
	{ NULL, 0, POPT_ARG_CALLBACK, (void *)popt_version_callback },
	{ "version", 'V', POPT_ARG_NONE, NULL, 'V', "Print version" },
	{ NULL }
};

