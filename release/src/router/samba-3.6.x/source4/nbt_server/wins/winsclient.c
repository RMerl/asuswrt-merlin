/* 
   Unix SMB/CIFS implementation.

   wins client name registration and refresh

   Copyright (C) Andrew Tridgell	2005
   
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
#include "nbt_server/nbt_server.h"
#include "nbt_server/wins/winsserver.h"
#include "libcli/composite/composite.h"
#include "lib/events/events.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "smbd/service_task.h"
#include "param/param.h"

/* we send WINS client requests using our primary network interface 
*/
static struct nbt_name_socket *wins_socket(struct nbtd_interface *iface)
{
	struct nbtd_server *nbtsrv = iface->nbtsrv;
	return nbtsrv->interfaces->nbtsock;
}


static void nbtd_wins_refresh(struct tevent_context *ev, struct tevent_timer *te,
			      struct timeval t, void *private_data);

/*
  retry a WINS name registration
*/
static void nbtd_wins_register_retry(struct tevent_context *ev, struct tevent_timer *te,
				     struct timeval t, void *private_data)
{
	struct nbtd_iface_name *iname = talloc_get_type(private_data, struct nbtd_iface_name);
	nbtd_winsclient_register(iname);
}

/*
  start a timer to refresh this name
*/
static void nbtd_wins_start_refresh_timer(struct nbtd_iface_name *iname)
{
	uint32_t refresh_time;
	uint32_t max_refresh_time = lpcfg_parm_int(iname->iface->nbtsrv->task->lp_ctx, NULL, "nbtd", "max_refresh_time", 7200);

	refresh_time = MIN(max_refresh_time, iname->ttl/2);
	
	event_add_timed(iname->iface->nbtsrv->task->event_ctx, 
			iname, 
			timeval_add(&iname->registration_time, refresh_time, 0),
			nbtd_wins_refresh, iname);
}

struct nbtd_wins_refresh_state {
	struct nbtd_iface_name *iname;
	struct nbt_name_refresh_wins io;
};

/*
  called when a wins name refresh has completed
*/
static void nbtd_wins_refresh_handler(struct tevent_req *subreq)
{
	NTSTATUS status;
	struct nbtd_wins_refresh_state *state =
		tevent_req_callback_data(subreq,
		struct nbtd_wins_refresh_state);
	struct nbtd_iface_name *iname = state->iname;

	status = nbt_name_refresh_wins_recv(subreq, state, &state->io);
	TALLOC_FREE(subreq);
	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
		/* our WINS server is dead - start registration over
		   from scratch */
		DEBUG(2,("Failed to refresh %s with WINS server %s\n",
			 nbt_name_string(state, &iname->name), iname->wins_server));
		talloc_free(state);
		nbtd_winsclient_register(iname);
		return;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("Name refresh failure with WINS for %s - %s\n", 
			 nbt_name_string(state, &iname->name), nt_errstr(status)));
		talloc_free(state);
		return;
	}

	if (state->io.out.rcode != 0) {
		DEBUG(1,("WINS server %s rejected name refresh of %s - %s\n", 
			 state->io.out.wins_server,
			 nbt_name_string(state, &iname->name),
			 nt_errstr(nbt_rcode_to_ntstatus(state->io.out.rcode))));
		iname->nb_flags |= NBT_NM_CONFLICT;
		talloc_free(state);
		return;
	}	

	DEBUG(4,("Refreshed name %s with WINS server %s\n",
		 nbt_name_string(state, &iname->name), iname->wins_server));
	/* success - start a periodic name refresh */
	iname->nb_flags |= NBT_NM_ACTIVE;
	if (iname->wins_server) {
		/*
		 * talloc_free() would generate a warning,
		 * so steal it into the tmp context
		 */
		talloc_steal(state, iname->wins_server);
	}
	iname->wins_server = talloc_move(iname, &state->io.out.wins_server);
	iname->registration_time = timeval_current();

	talloc_free(state);

	nbtd_wins_start_refresh_timer(iname);
}


/*
  refresh a WINS name registration
*/
static void nbtd_wins_refresh(struct tevent_context *ev, struct tevent_timer *te,
			      struct timeval t, void *private_data)
{
	struct nbtd_iface_name *iname = talloc_get_type(private_data, struct nbtd_iface_name);
	struct nbtd_interface *iface = iname->iface;
	struct nbt_name_socket *nbtsock = wins_socket(iface);
	struct tevent_req *subreq;
	struct nbtd_wins_refresh_state *state;

	state = talloc_zero(iname, struct nbtd_wins_refresh_state);
	if (state == NULL) {
		return;
	}

	state->iname = iname;

	/* setup a wins name refresh request */
	state->io.in.name            = iname->name;
	state->io.in.wins_servers    = (const char **)str_list_make_single(state, iname->wins_server);
	state->io.in.wins_port       = lpcfg_nbt_port(iface->nbtsrv->task->lp_ctx);
	state->io.in.addresses       = nbtd_address_list(iface, state);
	state->io.in.nb_flags        = iname->nb_flags;
	state->io.in.ttl             = iname->ttl;

	if (!state->io.in.addresses) {
		talloc_free(state);
		return;
	}

	subreq = nbt_name_refresh_wins_send(state, ev, nbtsock, &state->io);
	if (subreq == NULL) {
		talloc_free(state);
		return;
	}

	tevent_req_set_callback(subreq, nbtd_wins_refresh_handler, state);
}

struct nbtd_wins_register_state {
	struct nbtd_iface_name *iname;
	struct nbt_name_register_wins io;
};

/*
  called when a wins name register has completed
*/
static void nbtd_wins_register_handler(struct tevent_req *subreq)
{
	NTSTATUS status;
	struct nbtd_wins_register_state *state =
		tevent_req_callback_data(subreq,
		struct nbtd_wins_register_state);
	struct nbtd_iface_name *iname = state->iname;

	status = nbt_name_register_wins_recv(subreq, state, &state->io);
	TALLOC_FREE(subreq);
	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
		/* none of the WINS servers responded - try again 
		   periodically */
		int wins_retry_time = lpcfg_parm_int(iname->iface->nbtsrv->task->lp_ctx, NULL, "nbtd", "wins_retry", 300);
		event_add_timed(iname->iface->nbtsrv->task->event_ctx, 
				iname,
				timeval_current_ofs(wins_retry_time, 0),
				nbtd_wins_register_retry,
				iname);
		talloc_free(state);
		return;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("Name register failure with WINS for %s - %s\n", 
			 nbt_name_string(state, &iname->name), nt_errstr(status)));
		talloc_free(state);
		return;
	}	

	if (state->io.out.rcode != 0) {
		DEBUG(1,("WINS server %s rejected name register of %s - %s\n", 
			 state->io.out.wins_server,
			 nbt_name_string(state, &iname->name),
			 nt_errstr(nbt_rcode_to_ntstatus(state->io.out.rcode))));
		iname->nb_flags |= NBT_NM_CONFLICT;
		talloc_free(state);
		return;
	}	

	/* success - start a periodic name refresh */
	iname->nb_flags |= NBT_NM_ACTIVE;
	if (iname->wins_server) {
		/*
		 * talloc_free() would generate a warning,
		 * so steal it into the tmp context
		 */
		talloc_steal(state, iname->wins_server);
	}
	iname->wins_server = talloc_move(iname, &state->io.out.wins_server);

	iname->registration_time = timeval_current();

	DEBUG(3,("Registered %s with WINS server %s\n",
		 nbt_name_string(state, &iname->name), iname->wins_server));

	talloc_free(state);

	nbtd_wins_start_refresh_timer(iname);
}

/*
  register a name with our WINS servers
*/
void nbtd_winsclient_register(struct nbtd_iface_name *iname)
{
	struct nbtd_interface *iface = iname->iface;
	struct nbt_name_socket *nbtsock = wins_socket(iface);
	struct nbtd_wins_register_state *state;
	struct tevent_req *subreq;

	state = talloc_zero(iname, struct nbtd_wins_register_state);
	if (state == NULL) {
		return;
	}

	state->iname = iname;

	/* setup a wins name register request */
	state->io.in.name         = iname->name;
	state->io.in.wins_port    = lpcfg_nbt_port(iface->nbtsrv->task->lp_ctx);
	state->io.in.wins_servers = lpcfg_wins_server_list(iface->nbtsrv->task->lp_ctx);
	state->io.in.addresses    = nbtd_address_list(iface, state);
	state->io.in.nb_flags     = iname->nb_flags;
	state->io.in.ttl          = iname->ttl;

	if (state->io.in.addresses == NULL) {
		talloc_free(state);
		return;
	}

	subreq = nbt_name_register_wins_send(state, iface->nbtsrv->task->event_ctx,
					     nbtsock, &state->io);
	if (subreq == NULL) {
		talloc_free(state);
		return;
	}

	tevent_req_set_callback(subreq, nbtd_wins_register_handler, state);
}
