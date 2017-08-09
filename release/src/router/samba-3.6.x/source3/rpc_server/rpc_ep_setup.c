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
#include "ntdomain.h"

#include "../librpc/gen_ndr/ndr_epmapper_c.h"
#include "../librpc/gen_ndr/srv_epmapper.h"
#include "../librpc/gen_ndr/srv_srvsvc.h"
#include "../librpc/gen_ndr/srv_winreg.h"
#include "../librpc/gen_ndr/srv_dfs.h"
#include "../librpc/gen_ndr/srv_dssetup.h"
#include "../librpc/gen_ndr/srv_echo.h"
#include "../librpc/gen_ndr/srv_eventlog.h"
#include "../librpc/gen_ndr/srv_initshutdown.h"
#include "../librpc/gen_ndr/srv_lsa.h"
#include "../librpc/gen_ndr/srv_netlogon.h"
#include "../librpc/gen_ndr/srv_ntsvcs.h"
#include "../librpc/gen_ndr/srv_samr.h"
#include "../librpc/gen_ndr/srv_spoolss.h"
#include "../librpc/gen_ndr/srv_svcctl.h"
#include "../librpc/gen_ndr/srv_wkssvc.h"

#include "printing/nt_printing_migrate_internal.h"
#include "rpc_server/eventlog/srv_eventlog_reg.h"
#include "rpc_server/svcctl/srv_svcctl_reg.h"
#include "rpc_server/spoolss/srv_spoolss_nt.h"
#include "rpc_server/svcctl/srv_svcctl_nt.h"

#include "librpc/rpc/dcerpc_ep.h"

#include "rpc_server/rpc_ep_setup.h"
#include "rpc_server/rpc_server.h"
#include "rpc_server/epmapper/srv_epmapper.h"

struct dcesrv_ep_context {
	struct tevent_context *ev_ctx;
	struct messaging_context *msg_ctx;
};

static uint16_t _open_sockets(struct tevent_context *ev_ctx,
			      struct messaging_context *msg_ctx,
			      struct ndr_syntax_id syntax_id,
			      uint16_t port)
{
	uint32_t num_ifs = iface_count();
	uint32_t i;
	uint16_t p = 0;

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

			p = setup_dcerpc_ncacn_tcpip_socket(ev_ctx,
							    msg_ctx,
							    syntax_id,
							    ifss,
							    port);
			if (p == 0) {
				return 0;
			}
			port = p;
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

			p = setup_dcerpc_ncacn_tcpip_socket(ev_ctx,
							    msg_ctx,
							    syntax_id,
							    &ss,
							    port);
			if (p == 0) {
				return 0;
			}
			port = p;
		}
	}

	return p;
}

static void rpc_ep_setup_register_loop(struct tevent_req *subreq);
static NTSTATUS rpc_ep_setup_try_register(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev_ctx,
					  struct messaging_context *msg_ctx,
					  const struct ndr_interface_table *iface,
					  const char *ncalrpc,
					  uint16_t port,
					  struct dcerpc_binding_handle **pbh);

struct rpc_ep_regsiter_state {
	struct dcerpc_binding_handle *h;

	TALLOC_CTX *mem_ctx;
	struct tevent_context *ev_ctx;
	struct messaging_context *msg_ctx;

	const struct ndr_interface_table *iface;

	const char *ncalrpc;
	uint16_t port;

	uint32_t wait_time;
};

NTSTATUS rpc_ep_setup_register(struct tevent_context *ev_ctx,
			       struct messaging_context *msg_ctx,
			       const struct ndr_interface_table *iface,
			       const char *ncalrpc,
			       uint16_t port)
{
	struct rpc_ep_regsiter_state *state;
	struct tevent_req *req;

	state = talloc(ev_ctx, struct rpc_ep_regsiter_state);
	if (state == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	state->mem_ctx = talloc_named(state,
				      0,
				      "ep %s %p",
				      iface->name, state);
	if (state->mem_ctx == NULL) {
		talloc_free(state);
		return NT_STATUS_NO_MEMORY;
	}

	state->wait_time = 1;
	state->ev_ctx = ev_ctx;
	state->msg_ctx = msg_ctx;
	state->iface = iface;
	state->ncalrpc = talloc_strdup(state, ncalrpc);
	state->port = port;

	req = tevent_wakeup_send(state->mem_ctx,
				 state->ev_ctx,
				 timeval_current_ofs(1, 0));
	if (tevent_req_nomem(state->mem_ctx, req)) {
		talloc_free(state);
		return NT_STATUS_NO_MEMORY;
	}

	tevent_req_set_callback(req, rpc_ep_setup_register_loop, state);

	return NT_STATUS_OK;
}

#define MONITOR_WAIT_TIME 15
static void rpc_ep_setup_monitor_loop(struct tevent_req *subreq);

static void rpc_ep_setup_register_loop(struct tevent_req *subreq)
{
	struct rpc_ep_regsiter_state *state =
		tevent_req_callback_data(subreq, struct rpc_ep_regsiter_state);
	NTSTATUS status;
	bool ok;

	ok = tevent_wakeup_recv(subreq);
	TALLOC_FREE(subreq);
	if (!ok) {
		talloc_free(state);
		return;
	}

	status = rpc_ep_setup_try_register(state->mem_ctx,
					   state->ev_ctx,
					   state->msg_ctx,
					   state->iface,
					   state->ncalrpc,
					   state->port,
					   &state->h);
	if (NT_STATUS_IS_OK(status)) {
		/* endpoint registered, monitor the connnection. */
		subreq = tevent_wakeup_send(state->mem_ctx,
					    state->ev_ctx,
					    timeval_current_ofs(MONITOR_WAIT_TIME, 0));
		if (tevent_req_nomem(state->mem_ctx, subreq)) {
			talloc_free(state);
			return;
		}

		tevent_req_set_callback(subreq, rpc_ep_setup_monitor_loop, state);
		return;
	}

	state->wait_time = state->wait_time * 2;
	if (state->wait_time > 16) {
		DEBUG(0, ("Failed to register endpoint '%s'!\n",
			   state->iface->name));
		state->wait_time = 16;
	}

	subreq = tevent_wakeup_send(state->mem_ctx,
				    state->ev_ctx,
				    timeval_current_ofs(state->wait_time, 0));
	if (tevent_req_nomem(state->mem_ctx, subreq)) {
		talloc_free(state);
		return;
	}

	tevent_req_set_callback(subreq, rpc_ep_setup_register_loop, state);
	return;
}

static NTSTATUS rpc_ep_setup_try_register(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev_ctx,
					  struct messaging_context *msg_ctx,
					  const struct ndr_interface_table *iface,
					  const char *ncalrpc,
					  uint16_t port,
					  struct dcerpc_binding_handle **pbh)
{
	struct dcerpc_binding_vector *v = NULL;
	NTSTATUS status;

	status = dcerpc_binding_vector_create(mem_ctx,
					      iface,
					      port,
					      ncalrpc,
					      &v);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = dcerpc_ep_register(mem_ctx,
				    iface,
				    v,
				    &iface->syntax_id.uuid,
				    iface->name,
				    pbh);
	talloc_free(v);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return status;
}

/*
 * Monitor the connection to the endpoint mapper and if it goes away, try to
 * register the endpoint.
 */
static void rpc_ep_setup_monitor_loop(struct tevent_req *subreq)
{
	struct rpc_ep_regsiter_state *state =
		tevent_req_callback_data(subreq, struct rpc_ep_regsiter_state);
	struct policy_handle entry_handle;
	struct dcerpc_binding map_binding;
	struct epm_twr_p_t towers[10];
	struct epm_twr_t *map_tower;
	uint32_t num_towers = 0;
	struct GUID object;
	NTSTATUS status;
	uint32_t result = EPMAPPER_STATUS_CANT_PERFORM_OP;
	TALLOC_CTX *tmp_ctx;
	bool ok;

	ZERO_STRUCT(object);
	ZERO_STRUCT(entry_handle);

	tmp_ctx = talloc_stackframe();
	if (tmp_ctx == NULL) {
		talloc_free(state);
		return;
	}

	ok = tevent_wakeup_recv(subreq);
	TALLOC_FREE(subreq);
	if (!ok) {
		talloc_free(state);
		return;
	}

	/* Create map tower */
	map_binding.transport = NCACN_NP;
	map_binding.object = state->iface->syntax_id;
	map_binding.host = "";
	map_binding.endpoint = "";

	map_tower = talloc_zero(tmp_ctx, struct epm_twr_t);
	if (map_tower == NULL) {
		talloc_free(tmp_ctx);
		talloc_free(state);
		return;
	}

	status = dcerpc_binding_build_tower(map_tower, &map_binding,
					    &map_tower->tower);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		talloc_free(state);
		return;
	}

	ok = false;
	status = dcerpc_epm_Map(state->h,
				tmp_ctx,
				&object,
				map_tower,
				&entry_handle,
				10,
				&num_towers,
				towers,
				&result);
	if (NT_STATUS_IS_OK(status)) {
		ok = true;
	}
	if (result == EPMAPPER_STATUS_OK ||
	    result == EPMAPPER_STATUS_NO_MORE_ENTRIES) {
		ok = true;
	}
	if (num_towers == 0) {
		ok = false;
	}

	talloc_free(tmp_ctx);

	subreq = tevent_wakeup_send(state->mem_ctx,
				    state->ev_ctx,
				    timeval_current_ofs(MONITOR_WAIT_TIME, 0));
	if (tevent_req_nomem(state->mem_ctx, subreq)) {
		talloc_free(state);
		return;
	}

	if (ok) {
		tevent_req_set_callback(subreq, rpc_ep_setup_monitor_loop, state);
	} else {
		TALLOC_FREE(state->h);
		state->wait_time = 1;

		tevent_req_set_callback(subreq, rpc_ep_setup_register_loop, state);
	}

	return;
}

static bool epmapper_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	uint16_t port;

	port = _open_sockets(ep_ctx->ev_ctx,
			     ep_ctx->msg_ctx,
			     ndr_table_epmapper.syntax_id,
			     135);
	if (port == 135) {
		return true;
	}

	return false;
}

static bool epmapper_shutdown_cb(void *ptr)
{
	srv_epmapper_cleanup();

	return true;
}

#ifdef WINREG_SUPPORT
static bool winreg_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	struct ndr_syntax_id abstract_syntax = ndr_table_winreg.syntax_id;
	const char *pipe_name = "winreg";
	const char *rpcsrv_type;
	uint16_t port;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");

	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;
		bool ok;

		ok = setup_dcerpc_ncalrpc_socket(ep_ctx->ev_ctx,
						 ep_ctx->msg_ctx,
						 abstract_syntax,
						 pipe_name,
						 NULL);
		if (!ok) {
			return false;
		}
		port = _open_sockets(ep_ctx->ev_ctx,
				     ep_ctx->msg_ctx,
				     abstract_syntax,
				     0);
		if (port == 0) {
			return false;
		}

		status = rpc_ep_setup_register(ep_ctx->ev_ctx,
						ep_ctx->msg_ctx,
						&ndr_table_winreg,
						pipe_name,
						port);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}
#endif

static bool srvsvc_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	struct ndr_syntax_id abstract_syntax = ndr_table_srvsvc.syntax_id;
	const char *pipe_name = "srvsvc";
	const char *rpcsrv_type;
	uint16_t port;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");

	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;
		bool ok;

		ok = setup_dcerpc_ncalrpc_socket(ep_ctx->ev_ctx,
						 ep_ctx->msg_ctx,
						 abstract_syntax,
						 pipe_name,
						 NULL);
		if (!ok) {
			return false;
		}

		port = _open_sockets(ep_ctx->ev_ctx,
				     ep_ctx->msg_ctx,
				     abstract_syntax,
				     0);
		if (port == 0) {
			return false;
		}

		status = rpc_ep_setup_register(ep_ctx->ev_ctx,
					  ep_ctx->msg_ctx,
					  &ndr_table_srvsvc,
					  pipe_name,
					  port);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}

#ifdef LSA_SUPPORT
static bool lsarpc_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	struct ndr_syntax_id abstract_syntax = ndr_table_lsarpc.syntax_id;
	const char *pipe_name = "lsarpc";
	const char *rpcsrv_type;
	uint16_t port;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");

	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;
		bool ok;

		ok = setup_dcerpc_ncalrpc_socket(ep_ctx->ev_ctx,
						 ep_ctx->msg_ctx,
						 abstract_syntax,
						 pipe_name,
						 NULL);
		if (!ok) {
			return false;
		}

		port = _open_sockets(ep_ctx->ev_ctx,
				     ep_ctx->msg_ctx,
				     abstract_syntax,
				     0);
		if (port == 0) {
			return false;
		}

		status = rpc_ep_setup_register(ep_ctx->ev_ctx,
					  ep_ctx->msg_ctx,
					  &ndr_table_lsarpc,
					  pipe_name,
					  port);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}
#endif

#ifdef SAMR_SUPPORT
static bool samr_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	struct ndr_syntax_id abstract_syntax = ndr_table_samr.syntax_id;
	const char *pipe_name = "samr";
	const char *rpcsrv_type;
	uint16_t port;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");

	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;
		bool ok;

		ok = setup_dcerpc_ncalrpc_socket(ep_ctx->ev_ctx,
						 ep_ctx->msg_ctx,
						 abstract_syntax,
						 pipe_name,
						 NULL);
		if (!ok) {
			return false;
		}

		port = _open_sockets(ep_ctx->ev_ctx,
				     ep_ctx->msg_ctx,
				     abstract_syntax,
				     0);
		if (port == 0) {
			return false;
		}

		status = rpc_ep_setup_register(ep_ctx->ev_ctx,
					  ep_ctx->msg_ctx,
					  &ndr_table_samr,
					  pipe_name,
					  port);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}
#endif

#ifdef NETLOGON_SUPPORT
static bool netlogon_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	struct ndr_syntax_id abstract_syntax = ndr_table_netlogon.syntax_id;
	const char *pipe_name = "netlogon";
	const char *rpcsrv_type;
	uint16_t port;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");

	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;
		bool ok;

		ok = setup_dcerpc_ncalrpc_socket(ep_ctx->ev_ctx,
						 ep_ctx->msg_ctx,
						 abstract_syntax,
						 pipe_name,
						 NULL);
		if (!ok) {
			return false;
		}

		port = _open_sockets(ep_ctx->ev_ctx,
				     ep_ctx->msg_ctx,
				     abstract_syntax,
				     0);
		if (port == 0) {
			return false;
		}

		status = rpc_ep_setup_register(ep_ctx->ev_ctx,
					  ep_ctx->msg_ctx,
					  &ndr_table_netlogon,
					  pipe_name,
					  port);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}
#endif

static bool spoolss_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	const char *rpcsrv_type;
	bool ok;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");

	/*
	 * Migrate the printers first.
	 */
	ok = nt_printing_tdb_migrate(ep_ctx->msg_ctx);
	if (!ok) {
		return false;
	}

	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;

		status =rpc_ep_setup_register(ep_ctx->ev_ctx,
					 ep_ctx->msg_ctx,
					 &ndr_table_spoolss,
					 "spoolss",
					 0);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}

static bool spoolss_shutdown_cb(void *ptr)
{
	srv_spoolss_cleanup();

	return true;
}

#ifdef EXTRA_SERVICES
static bool svcctl_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	const char *rpcsrv_type;
	bool ok;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");

#ifdef WINREG_SUPPORT
	ok = svcctl_init_winreg(ep_ctx->msg_ctx);
	if (!ok) {
		return false;
	}
#endif

	/* initialize the control hooks */
	init_service_op_table();

	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;

		status = rpc_ep_setup_register(ep_ctx->ev_ctx,
					  ep_ctx->msg_ctx,
					  &ndr_table_svcctl,
					  "svcctl",
					  0);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}
#endif

static bool svcctl_shutdown_cb(void *ptr)
{
	shutdown_service_op_table();

	return true;
}

#ifdef EXTRA_SERVICES

static bool ntsvcs_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	const char *rpcsrv_type;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");

	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;

		status = rpc_ep_setup_register(ep_ctx->ev_ctx,
					  ep_ctx->msg_ctx,
					  &ndr_table_ntsvcs,
					  "ntsvcs",
					  0);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}

static bool eventlog_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	const char *rpcsrv_type;
	bool ok;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");

#ifdef WINREG_SUPPORT
	ok = eventlog_init_winreg(ep_ctx->msg_ctx);
	if (!ok) {
		return false;
	}
#endif

	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;

		status =rpc_ep_setup_register(ep_ctx->ev_ctx,
					 ep_ctx->msg_ctx,
					 &ndr_table_eventlog,
					 "eventlog",
					 0);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}
#endif

static bool initshutdown_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	const char *rpcsrv_type;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");

	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;

		status = rpc_ep_setup_register(ep_ctx->ev_ctx,
					  ep_ctx->msg_ctx,
					  &ndr_table_initshutdown,
					  "initshutdown",
					  0);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}

#ifdef DEVELOPER
static bool rpcecho_init_cb(void *ptr) {
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	const char *rpcsrv_type;
	uint16_t port;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");

	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;

		port = _open_sockets(ep_ctx->ev_ctx,
				     ep_ctx->msg_ctx,
				     ndr_table_rpcecho.syntax_id,
				     0);
		if (port == 0) {
			return false;
		}

		status = rpc_ep_setup_register(ep_ctx->ev_ctx,
					  ep_ctx->msg_ctx,
					  &ndr_table_rpcecho,
					  "rpcecho",
					  port);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}

#endif

#ifdef DFS_SUPPORT
static bool netdfs_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	struct ndr_syntax_id abstract_syntax = ndr_table_netdfs.syntax_id;
	const char *pipe_name = "netdfs";
	const char *rpcsrv_type;
	uint16_t port;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");
	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;
		bool ok;

		ok = setup_dcerpc_ncalrpc_socket(ep_ctx->ev_ctx,
						 ep_ctx->msg_ctx,
						 abstract_syntax,
						 pipe_name,
						 NULL);
		if (!ok) {
			return false;
		}

		port = _open_sockets(ep_ctx->ev_ctx,
				     ep_ctx->msg_ctx,
				     abstract_syntax,
				     0);
		if (port == 0) {
			return false;
		}

		status = rpc_ep_setup_register(ep_ctx->ev_ctx,
					  ep_ctx->msg_ctx,
					  &ndr_table_netdfs,
					  pipe_name,
					  port);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}
#endif

#ifdef ACTIVE_DIRECTORY
static bool dssetup_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	struct ndr_syntax_id abstract_syntax = ndr_table_dssetup.syntax_id;
	const char *pipe_name = "dssetup";
	const char *rpcsrv_type;
	uint16_t port;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");

	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;
		bool ok;

		ok = setup_dcerpc_ncalrpc_socket(ep_ctx->ev_ctx,
						 ep_ctx->msg_ctx,
						 abstract_syntax,
						 pipe_name,
						 NULL);
		if (!ok) {
			return false;
		}

		port = _open_sockets(ep_ctx->ev_ctx,
				     ep_ctx->msg_ctx,
				     abstract_syntax,
				     0);
		if (port == 0) {
			return false;
		}

		status = rpc_ep_setup_register(ep_ctx->ev_ctx,
					  ep_ctx->msg_ctx,
					  &ndr_table_dssetup,
					  "dssetup",
					  port);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}
#endif

static bool wkssvc_init_cb(void *ptr)
{
	struct dcesrv_ep_context *ep_ctx =
		talloc_get_type_abort(ptr, struct dcesrv_ep_context);
	struct ndr_syntax_id abstract_syntax = ndr_table_wkssvc.syntax_id;
	const char *pipe_name = "wkssvc";
	const char *rpcsrv_type;
	uint16_t port;

	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");
	if (StrCaseCmp(rpcsrv_type, "embedded") == 0 ||
	    StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		NTSTATUS status;
		bool ok;

		ok = setup_dcerpc_ncalrpc_socket(ep_ctx->ev_ctx,
						 ep_ctx->msg_ctx,
						 abstract_syntax,
						 pipe_name,
						 NULL);
		if (!ok) {
			return false;
		}

		port = _open_sockets(ep_ctx->ev_ctx,
				     ep_ctx->msg_ctx,
				     abstract_syntax,
				     0);
		if (port == 0) {
			return false;
		}

		status = rpc_ep_setup_register(ep_ctx->ev_ctx,
					  ep_ctx->msg_ctx,
					  &ndr_table_wkssvc,
					  "wkssvc",
					  port);
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	return true;
}

bool dcesrv_ep_setup(struct tevent_context *ev_ctx,
		     struct messaging_context *msg_ctx)
{
	struct dcesrv_ep_context *ep_ctx;

	struct rpc_srv_callbacks epmapper_cb;

	struct rpc_srv_callbacks winreg_cb;
	struct rpc_srv_callbacks srvsvc_cb;

	struct rpc_srv_callbacks lsarpc_cb;
	struct rpc_srv_callbacks samr_cb;
	struct rpc_srv_callbacks netlogon_cb;

	struct rpc_srv_callbacks spoolss_cb;
	struct rpc_srv_callbacks svcctl_cb;
	struct rpc_srv_callbacks ntsvcs_cb;
	struct rpc_srv_callbacks eventlog_cb;
	struct rpc_srv_callbacks initshutdown_cb;
	struct rpc_srv_callbacks netdfs_cb;
#ifdef DEVELOPER
	struct rpc_srv_callbacks rpcecho_cb;
#endif
	struct rpc_srv_callbacks dssetup_cb;
	struct rpc_srv_callbacks wkssvc_cb;

	const char *rpcsrv_type;

	ep_ctx = talloc(ev_ctx, struct dcesrv_ep_context);
	if (ep_ctx == NULL) {
		return false;
	}

	ep_ctx->ev_ctx = ev_ctx;
	ep_ctx->msg_ctx = msg_ctx;

	/* start endpoint mapper only if enabled */
	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "epmapper",
					   "none");
	if (StrCaseCmp(rpcsrv_type, "embedded") == 0) {
		epmapper_cb.init         = epmapper_init_cb;
		epmapper_cb.shutdown     = epmapper_shutdown_cb;
		epmapper_cb.private_data = ep_ctx;

		if (!NT_STATUS_IS_OK(rpc_epmapper_init(&epmapper_cb))) {
			return false;
		}
	} else if (StrCaseCmp(rpcsrv_type, "daemon") == 0) {
		if (!NT_STATUS_IS_OK(rpc_epmapper_init(NULL))) {
			return false;
		}
	}

#ifdef WINREG_SUPPORT
	winreg_cb.init         = winreg_init_cb;
	winreg_cb.shutdown     = NULL;
	winreg_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_winreg_init(&winreg_cb))) {
		return false;
	}
#endif

	srvsvc_cb.init         = srvsvc_init_cb;
	srvsvc_cb.shutdown     = NULL;
	srvsvc_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_srvsvc_init(&srvsvc_cb))) {
		return false;
	}


#ifdef LSA_SUPPORT
	lsarpc_cb.init         = lsarpc_init_cb;
	lsarpc_cb.shutdown     = NULL;
	lsarpc_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_lsarpc_init(&lsarpc_cb))) {
		return false;
	}
#endif

#ifdef SAMR_SUPPORT
	samr_cb.init         = samr_init_cb;
	samr_cb.shutdown     = NULL;
	samr_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_samr_init(&samr_cb))) {
		return false;
	}
#endif

#ifdef NETLOGON_SUPPORT
	netlogon_cb.init         = netlogon_init_cb;
	netlogon_cb.shutdown     = NULL;
	netlogon_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_netlogon_init(&netlogon_cb))) {
		return false;
	}
#endif


	rpcsrv_type = lp_parm_const_string(GLOBAL_SECTION_SNUM,
					   "rpc_server",
					   "spoolss",
					   "embedded");
#ifndef PRINTER_SUPPORT
	if (1) {
	} else
#endif
	if (StrCaseCmp(rpcsrv_type, "embedded") == 0) {
		spoolss_cb.init         = spoolss_init_cb;
		spoolss_cb.shutdown     = spoolss_shutdown_cb;
		spoolss_cb.private_data = ep_ctx;
		if (!NT_STATUS_IS_OK(rpc_spoolss_init(&spoolss_cb))) {
			return false;
		}
	} else if (StrCaseCmp(rpcsrv_type, "daemon") == 0 ||
		   StrCaseCmp(rpcsrv_type, "external") == 0) {
		if (!NT_STATUS_IS_OK(rpc_spoolss_init(NULL))) {
			return false;
		}
	}

#ifdef EXTRA_SERVICES
	svcctl_cb.init         = svcctl_init_cb;
	svcctl_cb.shutdown     = svcctl_shutdown_cb;
	svcctl_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_svcctl_init(&svcctl_cb))) {
		return false;
	}

	ntsvcs_cb.init         = ntsvcs_init_cb;
	ntsvcs_cb.shutdown     = NULL;
	ntsvcs_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_ntsvcs_init(&ntsvcs_cb))) {
		return false;
	}

	eventlog_cb.init         = eventlog_init_cb;
	eventlog_cb.shutdown     = NULL;
	eventlog_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_eventlog_init(&eventlog_cb))) {
		return false;
	}
#endif

	initshutdown_cb.init         = initshutdown_init_cb;
	initshutdown_cb.shutdown     = NULL;
	initshutdown_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_initshutdown_init(&initshutdown_cb))) {
		return false;
	}

#ifdef DFS_SUPPORT
	netdfs_cb.init         = netdfs_init_cb;
	netdfs_cb.shutdown     = NULL;
	netdfs_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_netdfs_init(&netdfs_cb))) {
		return false;
	}
#endif

#ifdef DEVELOPER
	rpcecho_cb.init         = rpcecho_init_cb;
	rpcecho_cb.shutdown     = NULL;
	rpcecho_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_rpcecho_init(&rpcecho_cb))) {
		return false;
	}
#endif

#ifdef ACTIVE_DIRECTORY
	dssetup_cb.init         = dssetup_init_cb;
	dssetup_cb.shutdown     = NULL;
	dssetup_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_dssetup_init(&dssetup_cb))) {
		return false;
	}
#endif

	wkssvc_cb.init         = wkssvc_init_cb;
	wkssvc_cb.shutdown     = NULL;
	wkssvc_cb.private_data = ep_ctx;
	if (!NT_STATUS_IS_OK(rpc_wkssvc_init(&wkssvc_cb))) {
		return false;
	}

	return true;
}

/* vim: set ts=8 sw=8 noet cindent ft=c.doxygen: */
