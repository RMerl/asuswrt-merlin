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

static void nbtd_wins_refresh_handler(struct composite_context *c);

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
	uint32_t max_refresh_time = lp_parm_int(iname->iface->nbtsrv->task->lp_ctx, NULL, "nbtd", "max_refresh_time", 7200);

	refresh_time = MIN(max_refresh_time, iname->ttl/2);
	
	event_add_timed(iname->iface->nbtsrv->task->event_ctx, 
			iname, 
			timeval_add(&iname->registration_time, refresh_time, 0),
			nbtd_wins_refresh, iname);
}

/*
  called when a wins name refresh has completed
*/
static void nbtd_wins_refresh_handler(struct composite_context *c)
{
	NTSTATUS status;
	struct nbt_name_refresh_wins io;
	struct nbtd_iface_name *iname = talloc_get_type(c->async.private_data, 
							struct nbtd_iface_name);
	TALLOC_CTX *tmp_ctx = talloc_new(iname);

	status = nbt_name_refresh_wins_recv(c, tmp_ctx, &io);
	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
		/* our WINS server is dead - start registration over
		   from scratch */
		DEBUG(2,("Failed to refresh %s with WINS server %s\n",
			 nbt_name_string(tmp_ctx, &iname->name), iname->wins_server));
		talloc_free(tmp_ctx);
		nbtd_winsclient_register(iname);
		return;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("Name refresh failure with WINS for %s - %s\n", 
			 nbt_name_string(tmp_ctx, &iname->name), nt_errstr(status)));
		talloc_free(tmp_ctx);
		return;
	}

	if (io.out.rcode != 0) {
		DEBUG(1,("WINS server %s rejected name refresh of %s - %s\n", 
			 io.out.wins_server, nbt_name_string(tmp_ctx, &iname->name), 
			 nt_errstr(nbt_rcode_to_ntstatus(io.out.rcode))));
		iname->nb_flags |= NBT_NM_CONFLICT;
		talloc_free(tmp_ctx);
		return;
	}	

	DEBUG(4,("Refreshed name %s with WINS server %s\n",
		 nbt_name_string(tmp_ctx, &iname->name), iname->wins_server));
	/* success - start a periodic name refresh */
	iname->nb_flags |= NBT_NM_ACTIVE;
	if (iname->wins_server) {
		/*
		 * talloc_free() would generate a warning,
		 * so steal it into the tmp context
		 */
		talloc_steal(tmp_ctx, iname->wins_server);
	}
	iname->wins_server = talloc_steal(iname, io.out.wins_server);

	iname->registration_time = timeval_current();
	nbtd_wins_start_refresh_timer(iname);

	talloc_free(tmp_ctx);
}


/*
  refresh a WINS name registration
*/
static void nbtd_wins_refresh(struct tevent_context *ev, struct tevent_timer *te,
			      struct timeval t, void *private_data)
{
	struct nbtd_iface_name *iname = talloc_get_type(private_data, struct nbtd_iface_name);
	struct nbtd_interface *iface = iname->iface;
	struct nbt_name_refresh_wins io;
	struct composite_context *c;
	TALLOC_CTX *tmp_ctx = talloc_new(iname);

	/* setup a wins name refresh request */
	io.in.name            = iname->name;
	io.in.wins_servers    = (const char **)str_list_make_single(tmp_ctx, iname->wins_server);
	io.in.wins_port       = lp_nbt_port(iface->nbtsrv->task->lp_ctx);
	io.in.addresses       = nbtd_address_list(iface, tmp_ctx);
	io.in.nb_flags        = iname->nb_flags;
	io.in.ttl             = iname->ttl;

	if (!io.in.addresses) {
		talloc_free(tmp_ctx);
		return;
	}

	c = nbt_name_refresh_wins_send(wins_socket(iface), &io);
	if (c == NULL) {
		talloc_free(tmp_ctx);
		return;
	}
	talloc_steal(c, io.in.addresses);

	c->async.fn = nbtd_wins_refresh_handler;
	c->async.private_data = iname;

	talloc_free(tmp_ctx);
}


/*
  called when a wins name register has completed
*/
static void nbtd_wins_register_handler(struct composite_context *c)
{
	NTSTATUS status;
	struct nbt_name_register_wins io;
	struct nbtd_iface_name *iname = talloc_get_type(c->async.private_data, 
							struct nbtd_iface_name);
	TALLOC_CTX *tmp_ctx = talloc_new(iname);

	status = nbt_name_register_wins_recv(c, tmp_ctx, &io);
	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
		/* none of the WINS servers responded - try again 
		   periodically */
		int wins_retry_time = lp_parm_int(iname->iface->nbtsrv->task->lp_ctx, NULL, "nbtd", "wins_retry", 300);
		event_add_timed(iname->iface->nbtsrv->task->event_ctx, 
				iname,
				timeval_current_ofs(wins_retry_time, 0),
				nbtd_wins_register_retry,
				iname);
		talloc_free(tmp_ctx);
		return;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("Name register failure with WINS for %s - %s\n", 
			 nbt_name_string(tmp_ctx, &iname->name), nt_errstr(status)));
		talloc_free(tmp_ctx);
		return;
	}	

	if (io.out.rcode != 0) {
		DEBUG(1,("WINS server %s rejected name register of %s - %s\n", 
			 io.out.wins_server, nbt_name_string(tmp_ctx, &iname->name), 
			 nt_errstr(nbt_rcode_to_ntstatus(io.out.rcode))));
		iname->nb_flags |= NBT_NM_CONFLICT;
		talloc_free(tmp_ctx);
		return;
	}	

	/* success - start a periodic name refresh */
	iname->nb_flags |= NBT_NM_ACTIVE;
	if (iname->wins_server) {
		/*
		 * talloc_free() would generate a warning,
		 * so steal it into the tmp context
		 */
		talloc_steal(tmp_ctx, iname->wins_server);
	}
	iname->wins_server = talloc_steal(iname, io.out.wins_server);

	iname->registration_time = timeval_current();
	nbtd_wins_start_refresh_timer(iname);

	DEBUG(3,("Registered %s with WINS server %s\n",
		 nbt_name_string(tmp_ctx, &iname->name), iname->wins_server));

	talloc_free(tmp_ctx);
}

/*
  register a name with our WINS servers
*/
void nbtd_winsclient_register(struct nbtd_iface_name *iname)
{
	struct nbtd_interface *iface = iname->iface;
	struct nbt_name_register_wins io;
	struct composite_context *c;

	/* setup a wins name register request */
	io.in.name            = iname->name;
	io.in.wins_port       = lp_nbt_port(iname->iface->nbtsrv->task->lp_ctx);
	io.in.wins_servers    = lp_wins_server_list(iname->iface->nbtsrv->task->lp_ctx);
	io.in.addresses       = nbtd_address_list(iface, iname);
	io.in.nb_flags        = iname->nb_flags;
	io.in.ttl             = iname->ttl;

	if (!io.in.addresses) {
		return;
	}

	c = nbt_name_register_wins_send(wins_socket(iface), &io);
	if (c == NULL) {
		talloc_free(io.in.addresses);
		return;
	}
	talloc_steal(c, io.in.addresses);

	c->async.fn = nbtd_wins_register_handler;
	c->async.private_data = iname;
}
