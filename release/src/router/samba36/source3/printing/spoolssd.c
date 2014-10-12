/*
   Unix SMB/Netbios implementation.
   SPOOLSS Daemon
   Copyright (C) Simo Sorce 2010

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
#include "serverid.h"
#include "smbd/smbd.h"

#include "messages.h"
#include "include/printing.h"
#include "printing/nt_printing_migrate_internal.h"
#include "ntdomain.h"
#include "librpc/gen_ndr/srv_winreg.h"
#include "librpc/gen_ndr/srv_spoolss.h"
#include "rpc_server/rpc_server.h"
#include "rpc_server/rpc_ep_setup.h"
#include "rpc_server/spoolss/srv_spoolss_nt.h"

#define SPOOLSS_PIPE_NAME "spoolss"
#define DAEMON_NAME "spoolssd"

void start_spoolssd(struct tevent_context *ev_ctx,
		    struct messaging_context *msg_ctx);

static void spoolss_reopen_logs(void)
{
	char *lfile = lp_logfile();
	int rc;

	if (lfile == NULL || lfile[0] == '\0') {
		rc = asprintf(&lfile, "%s/log.%s", get_dyn_LOGFILEBASE(), DAEMON_NAME);
		if (rc > 0) {
			lp_set_logfile(lfile);
			SAFE_FREE(lfile);
		}
	} else {
		if (strstr(lfile, DAEMON_NAME) == NULL) {
			rc = asprintf(&lfile, "%s.%s", lp_logfile(), DAEMON_NAME);
			if (rc > 0) {
				lp_set_logfile(lfile);
				SAFE_FREE(lfile);
			}
		}
	}

	reopen_logs();
}

static void smb_conf_updated(struct messaging_context *msg,
			     void *private_data,
			     uint32_t msg_type,
			     struct server_id server_id,
			     DATA_BLOB *data)
{
	struct tevent_context *ev_ctx = talloc_get_type_abort(private_data,
							     struct tevent_context);

	DEBUG(10, ("Got message saying smb.conf was updated. Reloading.\n"));
	change_to_root_user();
	spoolss_reopen_logs();
}

static void spoolss_pcap_updated(struct messaging_context *msg,
				 void *private_data,
				 uint32_t msg_type,
				 struct server_id server_id,
				 DATA_BLOB *data)
{
	struct tevent_context *ev_ctx = talloc_get_type_abort(private_data,
							     struct tevent_context);

	DEBUG(10, ("Got message saying pcap was updated. Reloading.\n"));
	change_to_root_user();
	reload_printers(ev_ctx, msg);
}

static void spoolss_sig_term_handler(struct tevent_context *ev,
				     struct tevent_signal *se,
				     int signum,
				     int count,
				     void *siginfo,
				     void *private_data)
{
	exit_server_cleanly("termination signal");
}

static void spoolss_setup_sig_term_handler(struct tevent_context *ev_ctx)
{
	struct tevent_signal *se;

	se = tevent_add_signal(ev_ctx,
			       ev_ctx,
			       SIGTERM, 0,
			       spoolss_sig_term_handler,
			       NULL);
	if (!se) {
		exit_server("failed to setup SIGTERM handler");
	}
}

static void spoolss_sig_hup_handler(struct tevent_context *ev,
				    struct tevent_signal *se,
				    int signum,
				    int count,
				    void *siginfo,
				    void *private_data)
{
	struct messaging_context *msg_ctx = talloc_get_type_abort(private_data,
								  struct messaging_context);

	change_to_root_user();
	DEBUG(1,("Reloading printers after SIGHUP\n"));
	spoolss_reopen_logs();
}

static void spoolss_setup_sig_hup_handler(struct tevent_context *ev_ctx,
					  struct messaging_context *msg_ctx)
{
	struct tevent_signal *se;

	se = tevent_add_signal(ev_ctx,
			       ev_ctx,
			       SIGHUP, 0,
			       spoolss_sig_hup_handler,
			       msg_ctx);
	if (!se) {
		exit_server("failed to setup SIGHUP handler");
	}
}

static bool spoolss_init_cb(void *ptr)
{
	struct messaging_context *msg_ctx = talloc_get_type_abort(
		ptr, struct messaging_context);

	return nt_printing_tdb_migrate(msg_ctx);
}

static bool spoolss_shutdown_cb(void *ptr)
{
	srv_spoolss_cleanup();

	return true;
}

void start_spoolssd(struct tevent_context *ev_ctx,
		    struct messaging_context *msg_ctx)
{
	struct rpc_srv_callbacks spoolss_cb;
	pid_t pid;
	NTSTATUS status;
	int ret;

#ifndef PRINTER_SUPPORT
	return;
#endif

	DEBUG(1, ("Forking SPOOLSS Daemon\n"));

	pid = sys_fork();

	if (pid == -1) {
		DEBUG(0, ("Failed to fork SPOOLSS [%s], aborting ...\n",
			   strerror(errno)));
		exit(1);
	}

	if (pid) {
		/* parent */
		return;
	}

	/* child */
	close_low_fds(false);

	status = reinit_after_fork(msg_ctx,
				   ev_ctx,
				   procid_self(), true);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("reinit_after_fork() failed\n"));
		smb_panic("reinit_after_fork() failed");
	}

	spoolss_reopen_logs();

	spoolss_setup_sig_term_handler(ev_ctx);
	spoolss_setup_sig_hup_handler(ev_ctx, msg_ctx);

	if (!serverid_register(procid_self(),
				FLAG_MSG_GENERAL|FLAG_MSG_SMBD
				|FLAG_MSG_PRINT_GENERAL)) {
		exit(1);
	}

	if (!locking_init()) {
		exit(1);
	}

	messaging_register(msg_ctx, NULL,
			   MSG_PRINTER_UPDATE, print_queue_receive);
	messaging_register(msg_ctx, ev_ctx,
			   MSG_SMB_CONF_UPDATED, smb_conf_updated);
	messaging_register(msg_ctx, ev_ctx,
			   MSG_PRINTER_PCAP, spoolss_pcap_updated);

	/*
	 * Initialize spoolss with an init function to convert printers first.
	 * static_init_rpc will try to initialize the spoolss server too but you
	 * can't register it twice.
	 */
	spoolss_cb.init = spoolss_init_cb;
	spoolss_cb.shutdown = spoolss_shutdown_cb;
	spoolss_cb.private_data = msg_ctx;

	status = rpc_winreg_init(NULL);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to register winreg rpc inteface! (%s)\n",
			  nt_errstr(status)));
		exit(1);
	}

	status = rpc_spoolss_init(&spoolss_cb);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to register spoolss rpc inteface! (%s)\n",
			  nt_errstr(status)));
		exit(1);
	}

	if (!setup_named_pipe_socket(SPOOLSS_PIPE_NAME, ev_ctx)) {
		exit(1);
	}

	status = rpc_ep_setup_register(ev_ctx, msg_ctx, &ndr_table_spoolss, NULL, 0);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to register spoolss endpoint! (%s)\n",
			  nt_errstr(status)));
		exit(1);
	}

	DEBUG(1, ("SPOOLSS Daemon Started (%d)\n", getpid()));

	/* loop forever */
	ret = tevent_loop_wait(ev_ctx);

	/* should not be reached */
	DEBUG(0,("background_queue: tevent_loop_wait() exited with %d - %s\n",
		 ret, (ret == 0) ? "out of events" : strerror(errno)));
	exit(1);
}
