/* 
   Unix SMB/CIFS implementation.

   Main SMB server routines

   Copyright (C) Andrew Tridgell		1992-2005
   Copyright (C) Martin Pool			2002
   Copyright (C) Jelmer Vernooij		2002
   Copyright (C) James J Myers 			2003 <myersjj@samba.org>
   
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
#include "lib/events/events.h"
#include "version.h"
#include "lib/cmdline/popt_common.h"
#include "system/dir.h"
#include "system/filesys.h"
#include "ntvfs/ntvfs.h"
#include "ntptr/ntptr.h"
#include "auth/gensec/gensec.h"
#include "smbd/process_model.h"
#include "param/secrets.h"
#include "smbd/pidfile.h"
#include "param/param.h"
#include "dsdb/samdb/samdb.h"
#include "auth/session.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/ndr_irpc.h"
#include "cluster/cluster.h"

/*
  recursively delete a directory tree
*/
static void recursive_delete(const char *path)
{
	DIR *dir;
	struct dirent *de;

	dir = opendir(path);
	if (!dir) {
		return;
	}

	for (de=readdir(dir);de;de=readdir(dir)) {
		char *fname;
		struct stat st;

		if (ISDOT(de->d_name) || ISDOTDOT(de->d_name)) {
			continue;
		}

		fname = talloc_asprintf(path, "%s/%s", path, de->d_name);
		if (stat(fname, &st) != 0) {
			continue;
		}
		if (S_ISDIR(st.st_mode)) {
			recursive_delete(fname);
			talloc_free(fname);
			continue;
		}
		if (unlink(fname) != 0) {
			DEBUG(0,("Unabled to delete '%s' - %s\n", 
				 fname, strerror(errno)));
			smb_panic("unable to cleanup tmp files");
		}
		talloc_free(fname);
	}
	closedir(dir);
}

/*
  cleanup temporary files. This is the new alternative to
  TDB_CLEAR_IF_FIRST. Unfortunately TDB_CLEAR_IF_FIRST is not
  efficient on unix systems due to the lack of scaling of the byte
  range locking system. So instead of putting the burden on tdb to
  cleanup tmp files, this function deletes them. 
*/
static void cleanup_tmp_files(struct loadparm_context *lp_ctx)
{
	char *path;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);

	path = smbd_tmp_path(mem_ctx, lp_ctx, NULL);

	recursive_delete(path);
	talloc_free(mem_ctx);
}

static void sig_hup(int sig)
{
	debug_schedule_reopen_logs();
}

static void sig_term(int sig)
{
#if HAVE_GETPGRP
	static int done_sigterm;
	if (done_sigterm == 0 && getpgrp() == getpid()) {
		DEBUG(0,("SIGTERM: killing children\n"));
		done_sigterm = 1;
		kill(-getpgrp(), SIGTERM);
	}
#endif
	DEBUG(0,("Exiting pid %d on SIGTERM\n", (int)getpid()));
	exit(0);
}

/*
  setup signal masks
*/
static void setup_signals(void)
{
	/* we are never interested in SIGPIPE */
	BlockSignals(true,SIGPIPE);

#if defined(SIGFPE)
	/* we are never interested in SIGFPE */
	BlockSignals(true,SIGFPE);
#endif

	/* We are no longer interested in USR1 */
	BlockSignals(true, SIGUSR1);

#if defined(SIGUSR2)
	/* We are no longer interested in USR2 */
	BlockSignals(true,SIGUSR2);
#endif

	/* POSIX demands that signals are inherited. If the invoking process has
	 * these signals masked, we will have problems, as we won't recieve them. */
	BlockSignals(false, SIGHUP);
	BlockSignals(false, SIGTERM);

	CatchSignal(SIGHUP, sig_hup);
	CatchSignal(SIGTERM, sig_term);
}

/*
  handle io on stdin
*/
static void server_stdin_handler(struct tevent_context *event_ctx, struct tevent_fd *fde, 
				 uint16_t flags, void *private_data)
{
	const char *binary_name = (const char *)private_data;
	uint8_t c;
	if (read(0, &c, 1) == 0) {
		DEBUG(0,("%s: EOF on stdin - terminating\n", binary_name));
#if HAVE_GETPGRP
		if (getpgrp() == getpid()) {
			DEBUG(0,("Sending SIGTERM from pid %d\n", (int)getpid()));
			kill(-getpgrp(), SIGTERM);
		}
#endif
		exit(0);
	}
}

/*
  die if the user selected maximum runtime is exceeded
*/
_NORETURN_ static void max_runtime_handler(struct tevent_context *ev, 
					   struct tevent_timer *te, 
					   struct timeval t, void *private_data)
{
	const char *binary_name = (const char *)private_data;
	DEBUG(0,("%s: maximum runtime exceeded - terminating\n", binary_name));
	exit(0);
}

/*
  pre-open the sam ldb to ensure the schema has been loaded. This
  saves a lot of time in child processes  
 */
static void prime_samdb_schema(struct tevent_context *event_ctx)
{
	TALLOC_CTX *samdb_context;
	samdb_context = talloc_new(event_ctx);
	samdb_connect(samdb_context, event_ctx, cmdline_lp_ctx, system_session(samdb_context, cmdline_lp_ctx));
	talloc_free(samdb_context);
}


/*
  called when a fatal condition occurs in a child task
 */
static NTSTATUS samba_terminate(struct irpc_message *msg, 
				struct samba_terminate *r)
{
	DEBUG(0,("samba_terminate: %s\n", r->in.reason));
	exit(1);
}

/*
  setup messaging for the top level samba (parent) task
 */
static NTSTATUS setup_parent_messaging(struct tevent_context *event_ctx, 
				       struct loadparm_context *lp_ctx)
{
	struct messaging_context *msg;
	NTSTATUS status;

	msg = messaging_init(talloc_autofree_context(), 
			     lp_messaging_path(event_ctx, lp_ctx),
			     cluster_id(0, SAMBA_PARENT_TASKID),
			     lp_iconv_convenience(lp_ctx),
			     event_ctx);
	NT_STATUS_HAVE_NO_MEMORY(msg);

	irpc_add_name(msg, "samba");

	status = IRPC_REGISTER(msg, irpc, SAMBA_TERMINATE,
			       samba_terminate, NULL);

	return status;
}



/*
 main server.
*/
static int binary_smbd_main(const char *binary_name, int argc, const char *argv[])
{
	bool opt_daemon = false;
	bool opt_interactive = false;
	int opt;
	poptContext pc;
	extern NTSTATUS server_service_wrepl_init(void);
	extern NTSTATUS server_service_kdc_init(void);
	extern NTSTATUS server_service_ldap_init(void);
	extern NTSTATUS server_service_web_init(void);
	extern NTSTATUS server_service_ldap_init(void);
	extern NTSTATUS server_service_winbind_init(void);
	extern NTSTATUS server_service_nbtd_init(void);
	extern NTSTATUS server_service_auth_init(void);
	extern NTSTATUS server_service_cldapd_init(void);
	extern NTSTATUS server_service_smb_init(void);
	extern NTSTATUS server_service_drepl_init(void);
	extern NTSTATUS server_service_kcc_init(void);
	extern NTSTATUS server_service_rpc_init(void);
	extern NTSTATUS server_service_ntp_signd_init(void);
	extern NTSTATUS server_service_samba3_smb_init(void);
	init_module_fn static_init[] = { STATIC_service_MODULES };
	init_module_fn *shared_init;
	struct tevent_context *event_ctx;
	uint16_t stdin_event_flags;
	NTSTATUS status;
	const char *model = "standard";
	int max_runtime = 0;
	enum {
		OPT_DAEMON = 1000,
		OPT_INTERACTIVE,
		OPT_PROCESS_MODEL
	};
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"daemon", 'D', POPT_ARG_NONE, NULL, OPT_DAEMON,
		 "Become a daemon (default)", NULL },
		{"interactive",	'i', POPT_ARG_NONE, NULL, OPT_INTERACTIVE,
		 "Run interactive (not a daemon)", NULL},
		{"model", 'M', POPT_ARG_STRING,	NULL, OPT_PROCESS_MODEL, 
		 "Select process model", "MODEL"},
		{"maximum-runtime",0, POPT_ARG_INT, &max_runtime, 0, 
		 "set maximum runtime of the server process, till autotermination", "seconds"},
		POPT_COMMON_SAMBA
		POPT_COMMON_VERSION
		{ NULL }
	};

	pc = poptGetContext(binary_name, argc, argv, long_options, 0);
	while((opt = poptGetNextOpt(pc)) != -1) {
		switch(opt) {
		case OPT_DAEMON:
			opt_daemon = true;
			break;
		case OPT_INTERACTIVE:
			opt_interactive = true;
			break;
		case OPT_PROCESS_MODEL:
			model = poptGetOptArg(pc);
			break;
		default:
			fprintf(stderr, "\nInvalid option %s: %s\n\n",
				  poptBadOption(pc, 0), poptStrerror(opt));
			poptPrintUsage(pc, stderr, 0);
			exit(1);
		}
	}

	if (opt_daemon && opt_interactive) {
		fprintf(stderr,"\nERROR: "
			  "Option -i|--interactive is not allowed together with -D|--daemon\n\n");
		poptPrintUsage(pc, stderr, 0);
		exit(1);
	} else if (!opt_interactive) {
		/* default is --daemon */
		opt_daemon = true;
	}

	poptFreeContext(pc);

	setup_logging(binary_name, opt_interactive?DEBUG_STDOUT:DEBUG_FILE);
	setup_signals();

	/* we want total control over the permissions on created files,
	   so set our umask to 0 */
	umask(0);

	DEBUG(0,("%s version %s started.\n", binary_name, SAMBA_VERSION_STRING));
	DEBUGADD(0,("Copyright Andrew Tridgell and the Samba Team 1992-2009\n"));

	if (sizeof(uint16_t) < 2 || sizeof(uint32_t) < 4 || sizeof(uint64_t) < 8) {
		DEBUG(0,("ERROR: Samba is not configured correctly for the word size on your machine\n"));
		DEBUGADD(0,("sizeof(uint16_t) = %u, sizeof(uint32_t) %u, sizeof(uint64_t) = %u\n",
			    (unsigned int)sizeof(uint16_t), (unsigned int)sizeof(uint32_t), (unsigned int)sizeof(uint64_t)));
		exit(1);
	}

	if (opt_daemon) {
		DEBUG(3,("Becoming a daemon.\n"));
		become_daemon(true, false);
	}

	cleanup_tmp_files(cmdline_lp_ctx);

	if (!directory_exist(lp_lockdir(cmdline_lp_ctx))) {
		mkdir(lp_lockdir(cmdline_lp_ctx), 0755);
	}

	pidfile_create(lp_piddir(cmdline_lp_ctx), binary_name);

	/* Do *not* remove this, until you have removed
	 * passdb/secrets.c, and proved that Samba still builds... */
	/* Setup the SECRETS subsystem */
	if (secrets_init(talloc_autofree_context(), cmdline_lp_ctx) == NULL) {
		exit(1);
	}

	gensec_init(cmdline_lp_ctx); /* FIXME: */

	ntptr_init(cmdline_lp_ctx);	/* FIXME: maybe run this in the initialization function 
						of the spoolss RPC server instead? */

	ntvfs_init(cmdline_lp_ctx); 	/* FIXME: maybe run this in the initialization functions 
						of the SMB[,2] server instead? */

	process_model_init(cmdline_lp_ctx); 

	shared_init = load_samba_modules(NULL, cmdline_lp_ctx, "service");

	run_init_functions(static_init);
	run_init_functions(shared_init);

	talloc_free(shared_init);
	
	/* the event context is the top level structure in smbd. Everything else
	   should hang off that */
	event_ctx = s4_event_context_init(talloc_autofree_context());

	if (event_ctx == NULL) {
		DEBUG(0,("Initializing event context failed\n"));
		return 1;
	}

	if (opt_interactive) {
		/* terminate when stdin goes away */
		stdin_event_flags = TEVENT_FD_READ;
	} else {
		/* stay alive forever */
		stdin_event_flags = 0;
	}

	/* catch EOF on stdin */
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_IGN);
#endif
	tevent_add_fd(event_ctx, event_ctx, 0, stdin_event_flags,
		      server_stdin_handler,
		      discard_const(binary_name));

	if (max_runtime) {
		tevent_add_timer(event_ctx, event_ctx,
				 timeval_current_ofs(max_runtime, 0),
				 max_runtime_handler,
				 discard_const(binary_name));
	}

	prime_samdb_schema(event_ctx);

	status = setup_parent_messaging(event_ctx, cmdline_lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Failed to setup parent messaging - %s\n", nt_errstr(status)));
		return 1;
	}

	DEBUG(0,("%s: using '%s' process model\n", binary_name, model));

	status = server_service_startup(event_ctx, cmdline_lp_ctx, model, 
					lp_server_services(cmdline_lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Starting Services failed - %s\n", nt_errstr(status)));
		return 1;
	}

	/* wait for events - this is where smbd sits for most of its
	   life */
	tevent_loop_wait(event_ctx);

	/* as everything hangs off this event context, freeing it
	   should initiate a clean shutdown of all services */
	talloc_free(event_ctx);

	return 0;
}

int main(int argc, const char *argv[])
{
	return binary_smbd_main("samba", argc, argv);
}
