/*
 *  Unix SMB/CIFS implementation.
 *
 *  SMBD RPC service callbacks
 *
 *  Copyright (c) 2011      Andreas Schneider <asn@samba.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

#include "serverid.h"
#include "ntdomain.h"
#include "../librpc/gen_ndr/srv_epmapper.h"
#include "rpc_server/rpc_server.h"
#include "rpc_server/epmapper/srv_epmapper.h"
#include "messages.h"

#define DAEMON_NAME "epmd"

void start_epmd(struct tevent_context *ev_ctx,
		struct messaging_context *msg_ctx);

static bool epmd_open_sockets(struct tevent_context *ev_ctx,
			      struct messaging_context *msg_ctx)
{
	uint32_t num_ifs = iface_count();
	uint16_t port;
	uint32_t i;

	if (lp_interfaces() && lp_bind_interfaces_only()) {
		/*
		 * We have been given an interfaces line, and been told to only
		 * bind to those interfaces. Create a socket per interface and
		 * bind to only these.
		 */

		/* Now open a listen socket for each of the interfaces. */
		for(i = 0; i < num_ifs; i++) {
			const struct sockaddr_storage *ifss =
					iface_n_sockaddr_storage(i);

			port = setup_dcerpc_ncacn_tcpip_socket(ev_ctx,
							       msg_ctx,
							       ndr_table_epmapper.syntax_id,
							       ifss,
							       135);
			if (port == 0) {
				return false;
			}
		}
	} else {
		const char *sock_addr = lp_socket_address();
		const char *sock_ptr;
		char *sock_tok;

		if (strequal(sock_addr, "0.0.0.0") ||
		    strequal(sock_addr, "::")) {
#if HAVE_IPV6
			sock_addr = "::";
#else
			sock_addr = "0.0.0.0";
#endif
		}

		for (sock_ptr = sock_addr;
		     next_token_talloc(talloc_tos(), &sock_ptr, &sock_tok, " \t,");
		    ) {
			struct sockaddr_storage ss;

			/* open an incoming socket */
			if (!interpret_string_addr(&ss,
						   sock_tok,
						   AI_NUMERICHOST|AI_PASSIVE)) {
				continue;
			}

			port = setup_dcerpc_ncacn_tcpip_socket(ev_ctx,
							       msg_ctx,
							       ndr_table_epmapper.syntax_id,
							       &ss,
							       135);
			if (port == 0) {
				return false;
			}
		}
	}

	return true;
}

static void epmd_reopen_logs(void)
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

static void epmd_smb_conf_updated(struct messaging_context *msg,
				  void *private_data,
				  uint32_t msg_type,
				  struct server_id server_id,
				  DATA_BLOB *data)
{
	DEBUG(10, ("Got message saying smb.conf was updated. Reloading.\n"));
	change_to_root_user();
	epmd_reopen_logs();
}

static void epmd_sig_term_handler(struct tevent_context *ev,
				  struct tevent_signal *se,
				  int signum,
				  int count,
				  void *siginfo,
				  void *private_data)
{
	rpc_epmapper_shutdown();

	exit_server_cleanly("termination signal");
}

static void epmd_setup_sig_term_handler(struct tevent_context *ev_ctx)
{
	struct tevent_signal *se;

	se = tevent_add_signal(ev_ctx,
			       ev_ctx,
			       SIGTERM, 0,
			       epmd_sig_term_handler,
			       NULL);
	if (se == NULL) {
		exit_server("failed to setup SIGTERM handler");
	}
}

static void epmd_sig_hup_handler(struct tevent_context *ev,
				    struct tevent_signal *se,
				    int signum,
				    int count,
				    void *siginfo,
				    void *private_data)
{
	change_to_root_user();

	DEBUG(1,("Reloading printers after SIGHUP\n"));
	epmd_reopen_logs();
}

static void epmd_setup_sig_hup_handler(struct tevent_context *ev_ctx,
				       struct messaging_context *msg_ctx)
{
	struct tevent_signal *se;

	se = tevent_add_signal(ev_ctx,
			       ev_ctx,
			       SIGHUP, 0,
			       epmd_sig_hup_handler,
			       msg_ctx);
	if (se == NULL) {
		exit_server("failed to setup SIGHUP handler");
	}
}

static bool epmapper_shutdown_cb(void *ptr) {
	srv_epmapper_cleanup();

	return true;
}

void start_epmd(struct tevent_context *ev_ctx,
		struct messaging_context *msg_ctx)
{
	struct rpc_srv_callbacks epmapper_cb;
	NTSTATUS status;
	pid_t pid;
	bool ok;
	int rc;

	epmapper_cb.init = NULL;
	epmapper_cb.shutdown = epmapper_shutdown_cb;
	epmapper_cb.private_data = NULL;

	DEBUG(1, ("Forking Endpoint Mapper Daemon\n"));

	pid = sys_fork();

	if (pid == -1) {
		DEBUG(0, ("Failed to fork Endpoint Mapper [%s], aborting ...\n",
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
				   procid_self(),
				   true);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("reinit_after_fork() failed\n"));
		smb_panic("reinit_after_fork() failed");
	}

	epmd_reopen_logs();

	epmd_setup_sig_term_handler(ev_ctx);
	epmd_setup_sig_hup_handler(ev_ctx, msg_ctx);

	ok = serverid_register(procid_self(),
			       FLAG_MSG_GENERAL|FLAG_MSG_SMBD
			       |FLAG_MSG_PRINT_GENERAL);
	if (!ok) {
		DEBUG(0, ("Failed to register serverid in epmd!\n"));
		exit(1);
	}

	messaging_register(msg_ctx,
			   ev_ctx,
			   MSG_SMB_CONF_UPDATED,
			   epmd_smb_conf_updated);

	status = rpc_epmapper_init(&epmapper_cb);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to register epmd rpc inteface! (%s)\n",
			  nt_errstr(status)));
		exit(1);
	}

	ok = setup_dcerpc_ncalrpc_socket(ev_ctx,
					 msg_ctx,
					 ndr_table_epmapper.syntax_id,
					 "EPMAPPER",
					 srv_epmapper_delete_endpoints);
	if (!ok) {
		DEBUG(0, ("Failed to open epmd ncalrpc pipe!\n"));
		exit(1);
	}

	ok = epmd_open_sockets(ev_ctx, msg_ctx);
	if (!ok) {
		DEBUG(0, ("Failed to open epmd tcpip sockets!\n"));
		exit(1);
	}

	ok = setup_named_pipe_socket("epmapper", ev_ctx);
	if (!ok) {
		DEBUG(0, ("Failed to open epmd named pipe!\n"));
		exit(1);
	}

	DEBUG(1, ("Endpoint Mapper Daemon Started (%d)\n", getpid()));

	/* loop forever */
	rc = tevent_loop_wait(ev_ctx);

	/* should not be reached */
	DEBUG(0,("background_queue: tevent_loop_wait() exited with %d - %s\n",
		 rc, (rc == 0) ? "out of events" : strerror(errno)));

	exit(1);
}

/* vim: set ts=8 sw=8 noet cindent ft=c.doxygen: */
