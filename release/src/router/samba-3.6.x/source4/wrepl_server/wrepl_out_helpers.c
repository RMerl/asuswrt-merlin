/* 
   Unix SMB/CIFS implementation.
   
   WINS Replication server
   
   Copyright (C) Stefan Metzmacher	2005
   
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
#include "lib/socket/socket.h"
#include "smbd/service_task.h"
#include "smbd/service_stream.h"
#include "librpc/gen_ndr/winsrepl.h"
#include "wrepl_server/wrepl_server.h"
#include "nbt_server/wins/winsdb.h"
#include "libcli/composite/composite.h"
#include "libcli/wrepl/winsrepl.h"
#include "libcli/resolve/resolve.h"
#include "param/param.h"

enum wreplsrv_out_connect_stage {
	WREPLSRV_OUT_CONNECT_STAGE_WAIT_SOCKET,
	WREPLSRV_OUT_CONNECT_STAGE_WAIT_ASSOC_CTX,
	WREPLSRV_OUT_CONNECT_STAGE_DONE
};

struct wreplsrv_out_connect_state {
	enum wreplsrv_out_connect_stage stage;
	struct composite_context *c;
	struct wrepl_associate assoc_io;
	enum winsrepl_partner_type type;
	struct wreplsrv_out_connection *wreplconn;
	struct tevent_req *subreq;
};

static void wreplsrv_out_connect_handler_treq(struct tevent_req *subreq);

static NTSTATUS wreplsrv_out_connect_wait_socket(struct wreplsrv_out_connect_state *state)
{
	NTSTATUS status;

	status = wrepl_connect_recv(state->subreq);
	TALLOC_FREE(state->subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	state->subreq = wrepl_associate_send(state,
					     state->wreplconn->service->task->event_ctx,
					     state->wreplconn->sock, &state->assoc_io);
	NT_STATUS_HAVE_NO_MEMORY(state->subreq);

	tevent_req_set_callback(state->subreq,
				wreplsrv_out_connect_handler_treq,
				state);

	state->stage = WREPLSRV_OUT_CONNECT_STAGE_WAIT_ASSOC_CTX;

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_out_connect_wait_assoc_ctx(struct wreplsrv_out_connect_state *state)
{
	NTSTATUS status;

	status = wrepl_associate_recv(state->subreq, &state->assoc_io);
	TALLOC_FREE(state->subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	state->wreplconn->assoc_ctx.peer_ctx = state->assoc_io.out.assoc_ctx;
	state->wreplconn->assoc_ctx.peer_major = state->assoc_io.out.major_version;

	if (state->type == WINSREPL_PARTNER_PUSH) {
		if (state->wreplconn->assoc_ctx.peer_major >= 5) {
			state->wreplconn->partner->push.wreplconn = state->wreplconn;
			talloc_steal(state->wreplconn->partner, state->wreplconn);
		} else {
			state->type = WINSREPL_PARTNER_NONE;
		}
	} else if (state->type == WINSREPL_PARTNER_PULL) {
		state->wreplconn->partner->pull.wreplconn = state->wreplconn;
		talloc_steal(state->wreplconn->partner, state->wreplconn);
	}

	state->stage = WREPLSRV_OUT_CONNECT_STAGE_DONE;

	return NT_STATUS_OK;
}

static void wreplsrv_out_connect_handler(struct wreplsrv_out_connect_state *state)
{
	struct composite_context *c = state->c;

	switch (state->stage) {
	case WREPLSRV_OUT_CONNECT_STAGE_WAIT_SOCKET:
		c->status = wreplsrv_out_connect_wait_socket(state);
		break;
	case WREPLSRV_OUT_CONNECT_STAGE_WAIT_ASSOC_CTX:
		c->status = wreplsrv_out_connect_wait_assoc_ctx(state);
		c->state  = COMPOSITE_STATE_DONE;
		break;
	case WREPLSRV_OUT_CONNECT_STAGE_DONE:
		c->status = NT_STATUS_INTERNAL_ERROR;
	}

	if (!NT_STATUS_IS_OK(c->status)) {
		c->state = COMPOSITE_STATE_ERROR;
	}

	if (c->state >= COMPOSITE_STATE_DONE && c->async.fn) {
		c->async.fn(c);
	}
}

static void wreplsrv_out_connect_handler_treq(struct tevent_req *subreq)
{
	struct wreplsrv_out_connect_state *state = tevent_req_callback_data(subreq,
						   struct wreplsrv_out_connect_state);
	wreplsrv_out_connect_handler(state);
	return;
}

static struct composite_context *wreplsrv_out_connect_send(struct wreplsrv_partner *partner,
							   enum winsrepl_partner_type type,
							   struct wreplsrv_out_connection *wreplconn)
{
	struct composite_context *c = NULL;
	struct wreplsrv_service *service = partner->service;
	struct wreplsrv_out_connect_state *state = NULL;
	struct wreplsrv_out_connection **wreplconnp = &wreplconn;
	bool cached_connection = false;

	c = talloc_zero(partner, struct composite_context);
	if (!c) goto failed;

	state = talloc_zero(c, struct wreplsrv_out_connect_state);
	if (!state) goto failed;
	state->c	= c;
	state->type	= type;

	c->state	= COMPOSITE_STATE_IN_PROGRESS;
	c->event_ctx	= service->task->event_ctx;
	c->private_data	= state;

	if (type == WINSREPL_PARTNER_PUSH) {
		cached_connection	= true;
		wreplconn		= partner->push.wreplconn;
		wreplconnp		= &partner->push.wreplconn;
	} else if (type == WINSREPL_PARTNER_PULL) {
		cached_connection	= true;
		wreplconn		= partner->pull.wreplconn;
		wreplconnp		= &partner->pull.wreplconn;
	}

	/* we have a connection already, so use it */
	if (wreplconn) {
		if (wrepl_socket_is_connected(wreplconn->sock)) {
			state->stage	= WREPLSRV_OUT_CONNECT_STAGE_DONE;
			state->wreplconn= wreplconn;
			composite_done(c);
			return c;
		} else if (!cached_connection) {
			state->stage	= WREPLSRV_OUT_CONNECT_STAGE_DONE;
			state->wreplconn= NULL;
			composite_done(c);
			return c;
		} else {
			talloc_free(wreplconn);
			*wreplconnp = NULL;
		}
	}

	wreplconn = talloc_zero(state, struct wreplsrv_out_connection);
	if (!wreplconn) goto failed;

	wreplconn->service	= service;
	wreplconn->partner	= partner;
	wreplconn->sock		= wrepl_socket_init(wreplconn, service->task->event_ctx);
	if (!wreplconn->sock) goto failed;

	state->stage	= WREPLSRV_OUT_CONNECT_STAGE_WAIT_SOCKET;
	state->wreplconn= wreplconn;
	state->subreq	= wrepl_connect_send(state,
					     service->task->event_ctx,
					     wreplconn->sock,
					     partner->our_address?partner->our_address:wrepl_best_ip(service->task->lp_ctx, partner->address),
					     partner->address);
	if (!state->subreq) goto failed;

	tevent_req_set_callback(state->subreq,
				wreplsrv_out_connect_handler_treq,
				state);

	return c;
failed:
	talloc_free(c);
	return NULL;
}

static NTSTATUS wreplsrv_out_connect_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
					  struct wreplsrv_out_connection **wreplconn)
{
	NTSTATUS status;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		struct wreplsrv_out_connect_state *state = talloc_get_type(c->private_data,
							   struct wreplsrv_out_connect_state);
		if (state->wreplconn) {
			*wreplconn = talloc_reference(mem_ctx, state->wreplconn);
			if (!*wreplconn) status = NT_STATUS_NO_MEMORY;
		} else {
			status = NT_STATUS_INVALID_CONNECTION;
		}
	}

	talloc_free(c);
	return status;
	
}

struct wreplsrv_pull_table_io {
	struct {
		struct wreplsrv_partner *partner;
		uint32_t num_owners;
		struct wrepl_wins_owner *owners;
	} in;
	struct {
		uint32_t num_owners;
		struct wrepl_wins_owner *owners;
	} out;
};

enum wreplsrv_pull_table_stage {
	WREPLSRV_PULL_TABLE_STAGE_WAIT_CONNECTION,
	WREPLSRV_PULL_TABLE_STAGE_WAIT_TABLE_REPLY,
	WREPLSRV_PULL_TABLE_STAGE_DONE
};

struct wreplsrv_pull_table_state {
	enum wreplsrv_pull_table_stage stage;
	struct composite_context *c;
	struct wrepl_pull_table table_io;
	struct wreplsrv_pull_table_io *io;
	struct composite_context *creq;
	struct wreplsrv_out_connection *wreplconn;
	struct tevent_req *subreq;
};

static void wreplsrv_pull_table_handler_treq(struct tevent_req *subreq);

static NTSTATUS wreplsrv_pull_table_wait_connection(struct wreplsrv_pull_table_state *state)
{
	NTSTATUS status;

	status = wreplsrv_out_connect_recv(state->creq, state, &state->wreplconn);
	NT_STATUS_NOT_OK_RETURN(status);

	state->table_io.in.assoc_ctx = state->wreplconn->assoc_ctx.peer_ctx;
	state->subreq = wrepl_pull_table_send(state,
					      state->wreplconn->service->task->event_ctx,
					      state->wreplconn->sock, &state->table_io);
	NT_STATUS_HAVE_NO_MEMORY(state->subreq);

	tevent_req_set_callback(state->subreq,
				wreplsrv_pull_table_handler_treq,
				state);

	state->stage = WREPLSRV_PULL_TABLE_STAGE_WAIT_TABLE_REPLY;

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_pull_table_wait_table_reply(struct wreplsrv_pull_table_state *state)
{
	NTSTATUS status;

	status = wrepl_pull_table_recv(state->subreq, state, &state->table_io);
	TALLOC_FREE(state->subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	state->stage = WREPLSRV_PULL_TABLE_STAGE_DONE;

	return NT_STATUS_OK;
}

static void wreplsrv_pull_table_handler(struct wreplsrv_pull_table_state *state)
{
	struct composite_context *c = state->c;

	switch (state->stage) {
	case WREPLSRV_PULL_TABLE_STAGE_WAIT_CONNECTION:
		c->status = wreplsrv_pull_table_wait_connection(state);
		break;
	case WREPLSRV_PULL_TABLE_STAGE_WAIT_TABLE_REPLY:
		c->status = wreplsrv_pull_table_wait_table_reply(state);
		c->state  = COMPOSITE_STATE_DONE;
		break;
	case WREPLSRV_PULL_TABLE_STAGE_DONE:
		c->status = NT_STATUS_INTERNAL_ERROR;
	}

	if (!NT_STATUS_IS_OK(c->status)) {
		c->state = COMPOSITE_STATE_ERROR;
	}

	if (c->state >= COMPOSITE_STATE_DONE && c->async.fn) {
		c->async.fn(c);
	}
}

static void wreplsrv_pull_table_handler_creq(struct composite_context *creq)
{
	struct wreplsrv_pull_table_state *state = talloc_get_type(creq->async.private_data,
						  struct wreplsrv_pull_table_state);
	wreplsrv_pull_table_handler(state);
	return;
}

static void wreplsrv_pull_table_handler_treq(struct tevent_req *subreq)
{
	struct wreplsrv_pull_table_state *state = tevent_req_callback_data(subreq,
						  struct wreplsrv_pull_table_state);
	wreplsrv_pull_table_handler(state);
	return;
}

static struct composite_context *wreplsrv_pull_table_send(TALLOC_CTX *mem_ctx, struct wreplsrv_pull_table_io *io)
{
	struct composite_context *c = NULL;
	struct wreplsrv_service *service = io->in.partner->service;
	struct wreplsrv_pull_table_state *state = NULL;

	c = talloc_zero(mem_ctx, struct composite_context);
	if (!c) goto failed;

	state = talloc_zero(c, struct wreplsrv_pull_table_state);
	if (!state) goto failed;
	state->c	= c;
	state->io	= io;

	c->state	= COMPOSITE_STATE_IN_PROGRESS;
	c->event_ctx	= service->task->event_ctx;
	c->private_data	= state;

	if (io->in.num_owners) {
		state->table_io.out.num_partners	= io->in.num_owners;
		state->table_io.out.partners		= io->in.owners;
		state->stage				= WREPLSRV_PULL_TABLE_STAGE_DONE;
		composite_done(c);
		return c;
	}

	state->stage    = WREPLSRV_PULL_TABLE_STAGE_WAIT_CONNECTION;
	state->creq	= wreplsrv_out_connect_send(io->in.partner, WINSREPL_PARTNER_PULL, NULL);
	if (!state->creq) goto failed;

	state->creq->async.fn		= wreplsrv_pull_table_handler_creq;
	state->creq->async.private_data	= state;

	return c;
failed:
	talloc_free(c);
	return NULL;
}

static NTSTATUS wreplsrv_pull_table_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
					 struct wreplsrv_pull_table_io *io)
{
	NTSTATUS status;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		struct wreplsrv_pull_table_state *state = talloc_get_type(c->private_data,
							  struct wreplsrv_pull_table_state);
		io->out.num_owners	= state->table_io.out.num_partners;
		io->out.owners		= talloc_reference(mem_ctx, state->table_io.out.partners);
	}

	talloc_free(c);
	return status;	
}

struct wreplsrv_pull_names_io {
	struct {
		struct wreplsrv_partner *partner;
		struct wreplsrv_out_connection *wreplconn;
		struct wrepl_wins_owner owner;
	} in;
	struct {
		uint32_t num_names;
		struct wrepl_name *names;
	} out;
};

enum wreplsrv_pull_names_stage {
	WREPLSRV_PULL_NAMES_STAGE_WAIT_CONNECTION,
	WREPLSRV_PULL_NAMES_STAGE_WAIT_SEND_REPLY,
	WREPLSRV_PULL_NAMES_STAGE_DONE
};

struct wreplsrv_pull_names_state {
	enum wreplsrv_pull_names_stage stage;
	struct composite_context *c;
	struct wrepl_pull_names pull_io;
	struct wreplsrv_pull_names_io *io;
	struct composite_context *creq;
	struct wreplsrv_out_connection *wreplconn;
	struct tevent_req *subreq;
};

static void wreplsrv_pull_names_handler_treq(struct tevent_req *subreq);

static NTSTATUS wreplsrv_pull_names_wait_connection(struct wreplsrv_pull_names_state *state)
{
	NTSTATUS status;

	status = wreplsrv_out_connect_recv(state->creq, state, &state->wreplconn);
	NT_STATUS_NOT_OK_RETURN(status);

	state->pull_io.in.assoc_ctx	= state->wreplconn->assoc_ctx.peer_ctx;
	state->pull_io.in.partner	= state->io->in.owner;
	state->subreq = wrepl_pull_names_send(state,
					      state->wreplconn->service->task->event_ctx,
					      state->wreplconn->sock,
					      &state->pull_io);
	NT_STATUS_HAVE_NO_MEMORY(state->subreq);

	tevent_req_set_callback(state->subreq,
				wreplsrv_pull_names_handler_treq,
				state);

	state->stage = WREPLSRV_PULL_NAMES_STAGE_WAIT_SEND_REPLY;

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_pull_names_wait_send_reply(struct wreplsrv_pull_names_state *state)
{
	NTSTATUS status;

	status = wrepl_pull_names_recv(state->subreq, state, &state->pull_io);
	TALLOC_FREE(state->subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	state->stage = WREPLSRV_PULL_NAMES_STAGE_DONE;

	return NT_STATUS_OK;
}

static void wreplsrv_pull_names_handler(struct wreplsrv_pull_names_state *state)
{
	struct composite_context *c = state->c;

	switch (state->stage) {
	case WREPLSRV_PULL_NAMES_STAGE_WAIT_CONNECTION:
		c->status = wreplsrv_pull_names_wait_connection(state);
		break;
	case WREPLSRV_PULL_NAMES_STAGE_WAIT_SEND_REPLY:
		c->status = wreplsrv_pull_names_wait_send_reply(state);
		c->state  = COMPOSITE_STATE_DONE;
		break;
	case WREPLSRV_PULL_NAMES_STAGE_DONE:
		c->status = NT_STATUS_INTERNAL_ERROR;
	}

	if (!NT_STATUS_IS_OK(c->status)) {
		c->state = COMPOSITE_STATE_ERROR;
	}

	if (c->state >= COMPOSITE_STATE_DONE && c->async.fn) {
		c->async.fn(c);
	}
}

static void wreplsrv_pull_names_handler_creq(struct composite_context *creq)
{
	struct wreplsrv_pull_names_state *state = talloc_get_type(creq->async.private_data,
						  struct wreplsrv_pull_names_state);
	wreplsrv_pull_names_handler(state);
	return;
}

static void wreplsrv_pull_names_handler_treq(struct tevent_req *subreq)
{
	struct wreplsrv_pull_names_state *state = tevent_req_callback_data(subreq,
						  struct wreplsrv_pull_names_state);
	wreplsrv_pull_names_handler(state);
	return;
}

static struct composite_context *wreplsrv_pull_names_send(TALLOC_CTX *mem_ctx, struct wreplsrv_pull_names_io *io)
{
	struct composite_context *c = NULL;
	struct wreplsrv_service *service = io->in.partner->service;
	struct wreplsrv_pull_names_state *state = NULL;
	enum winsrepl_partner_type partner_type = WINSREPL_PARTNER_PULL;

	if (io->in.wreplconn) partner_type = WINSREPL_PARTNER_NONE;

	c = talloc_zero(mem_ctx, struct composite_context);
	if (!c) goto failed;

	state = talloc_zero(c, struct wreplsrv_pull_names_state);
	if (!state) goto failed;
	state->c	= c;
	state->io	= io;

	c->state	= COMPOSITE_STATE_IN_PROGRESS;
	c->event_ctx	= service->task->event_ctx;
	c->private_data	= state;

	state->stage	= WREPLSRV_PULL_NAMES_STAGE_WAIT_CONNECTION;
	state->creq	= wreplsrv_out_connect_send(io->in.partner, partner_type, io->in.wreplconn);
	if (!state->creq) goto failed;

	state->creq->async.fn		= wreplsrv_pull_names_handler_creq;
	state->creq->async.private_data	= state;

	return c;
failed:
	talloc_free(c);
	return NULL;
}

static NTSTATUS wreplsrv_pull_names_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
					 struct wreplsrv_pull_names_io *io)
{
	NTSTATUS status;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		struct wreplsrv_pull_names_state *state = talloc_get_type(c->private_data,
							  struct wreplsrv_pull_names_state);
		io->out.num_names	= state->pull_io.out.num_names;
		io->out.names		= talloc_reference(mem_ctx, state->pull_io.out.names);
	}

	talloc_free(c);
	return status;
	
}

enum wreplsrv_pull_cycle_stage {
	WREPLSRV_PULL_CYCLE_STAGE_WAIT_TABLE_REPLY,
	WREPLSRV_PULL_CYCLE_STAGE_WAIT_SEND_REPLIES,
	WREPLSRV_PULL_CYCLE_STAGE_WAIT_STOP_ASSOC,
	WREPLSRV_PULL_CYCLE_STAGE_DONE
};

struct wreplsrv_pull_cycle_state {
	enum wreplsrv_pull_cycle_stage stage;
	struct composite_context *c;
	struct wreplsrv_pull_cycle_io *io;
	struct wreplsrv_pull_table_io table_io;
	uint32_t current;
	struct wreplsrv_pull_names_io names_io;
	struct composite_context *creq;
	struct wrepl_associate_stop assoc_stop_io;
	struct tevent_req *subreq;
};

static void wreplsrv_pull_cycle_handler_creq(struct composite_context *creq);
static void wreplsrv_pull_cycle_handler_treq(struct tevent_req *subreq);

static NTSTATUS wreplsrv_pull_cycle_next_owner_do_work(struct wreplsrv_pull_cycle_state *state)
{
	struct wreplsrv_owner *current_owner=NULL;
	struct wreplsrv_owner *local_owner;
	uint32_t i;
	uint64_t old_max_version = 0;
	bool do_pull = false;

	for (i=state->current; i < state->table_io.out.num_owners; i++) {
		current_owner = wreplsrv_find_owner(state->io->in.partner->service,
						    state->io->in.partner->pull.table,
						    state->table_io.out.owners[i].address);

		local_owner = wreplsrv_find_owner(state->io->in.partner->service,
						  state->io->in.partner->service->table,
						  state->table_io.out.owners[i].address);
		/*
		 * this means we are ourself the current owner,
		 * and we don't want replicate ourself
		 */
		if (!current_owner) continue;

		/*
		 * this means we don't have any records of this owner
		 * so fetch them
		 */
		if (!local_owner) {
			do_pull		= true;
			
			break;
		}

		/*
		 * this means the remote partner has some new records of this owner
		 * fetch them
		 */
		if (current_owner->owner.max_version > local_owner->owner.max_version) {
			do_pull		= true;
			old_max_version	= local_owner->owner.max_version;
			break;
		}
	}
	state->current = i;

	if (do_pull) {
		state->names_io.in.partner		= state->io->in.partner;
		state->names_io.in.wreplconn		= state->io->in.wreplconn;
		state->names_io.in.owner		= current_owner->owner;
		state->names_io.in.owner.min_version	= old_max_version + 1;
		state->creq = wreplsrv_pull_names_send(state, &state->names_io);
		NT_STATUS_HAVE_NO_MEMORY(state->creq);

		state->creq->async.fn		= wreplsrv_pull_cycle_handler_creq;
		state->creq->async.private_data	= state;

		return STATUS_MORE_ENTRIES;
	}

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_pull_cycle_next_owner_wrapper(struct wreplsrv_pull_cycle_state *state)
{
	NTSTATUS status;

	status = wreplsrv_pull_cycle_next_owner_do_work(state);
	if (NT_STATUS_IS_OK(status)) {
		state->stage = WREPLSRV_PULL_CYCLE_STAGE_DONE;
	} else if (NT_STATUS_EQUAL(STATUS_MORE_ENTRIES, status)) {
		state->stage = WREPLSRV_PULL_CYCLE_STAGE_WAIT_SEND_REPLIES;
		status = NT_STATUS_OK;
	}

	if (state->stage == WREPLSRV_PULL_CYCLE_STAGE_DONE && state->io->in.wreplconn) {
		state->assoc_stop_io.in.assoc_ctx	= state->io->in.wreplconn->assoc_ctx.peer_ctx;
		state->assoc_stop_io.in.reason		= 0;
		state->subreq = wrepl_associate_stop_send(state,
							  state->io->in.wreplconn->service->task->event_ctx,
							  state->io->in.wreplconn->sock,
							  &state->assoc_stop_io);
		NT_STATUS_HAVE_NO_MEMORY(state->subreq);

		tevent_req_set_callback(state->subreq,
					wreplsrv_pull_cycle_handler_treq,
					state);

		state->stage = WREPLSRV_PULL_CYCLE_STAGE_WAIT_STOP_ASSOC;
	}

	return status;
}

static NTSTATUS wreplsrv_pull_cycle_wait_table_reply(struct wreplsrv_pull_cycle_state *state)
{
	NTSTATUS status;
	uint32_t i;

	status = wreplsrv_pull_table_recv(state->creq, state, &state->table_io);
	NT_STATUS_NOT_OK_RETURN(status);

	/* update partner table */
	for (i=0; i < state->table_io.out.num_owners; i++) {
		status = wreplsrv_add_table(state->io->in.partner->service,
					    state->io->in.partner, 
					    &state->io->in.partner->pull.table,
					    state->table_io.out.owners[i].address,
					    state->table_io.out.owners[i].max_version);
		NT_STATUS_NOT_OK_RETURN(status);
	}

	status = wreplsrv_pull_cycle_next_owner_wrapper(state);
	NT_STATUS_NOT_OK_RETURN(status);

	return status;
}

static NTSTATUS wreplsrv_pull_cycle_apply_records(struct wreplsrv_pull_cycle_state *state)
{
	NTSTATUS status;

	status = wreplsrv_apply_records(state->io->in.partner,
					&state->names_io.in.owner,
					state->names_io.out.num_names,
					state->names_io.out.names);
	NT_STATUS_NOT_OK_RETURN(status);

	talloc_free(state->names_io.out.names);
	ZERO_STRUCT(state->names_io);

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_pull_cycle_wait_send_replies(struct wreplsrv_pull_cycle_state *state)
{
	NTSTATUS status;

	status = wreplsrv_pull_names_recv(state->creq, state, &state->names_io);
	NT_STATUS_NOT_OK_RETURN(status);

	/*
	 * TODO: this should maybe an async call,
	 *       because we may need some network access
	 *       for conflict resolving
	 */
	status = wreplsrv_pull_cycle_apply_records(state);
	NT_STATUS_NOT_OK_RETURN(status);

	status = wreplsrv_pull_cycle_next_owner_wrapper(state);
	NT_STATUS_NOT_OK_RETURN(status);

	return status;
}

static NTSTATUS wreplsrv_pull_cycle_wait_stop_assoc(struct wreplsrv_pull_cycle_state *state)
{
	NTSTATUS status;

	status = wrepl_associate_stop_recv(state->subreq, &state->assoc_stop_io);
	TALLOC_FREE(state->subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	state->stage = WREPLSRV_PULL_CYCLE_STAGE_DONE;

	return status;
}

static void wreplsrv_pull_cycle_handler(struct wreplsrv_pull_cycle_state *state)
{
	struct composite_context *c = state->c;

	switch (state->stage) {
	case WREPLSRV_PULL_CYCLE_STAGE_WAIT_TABLE_REPLY:
		c->status = wreplsrv_pull_cycle_wait_table_reply(state);
		break;
	case WREPLSRV_PULL_CYCLE_STAGE_WAIT_SEND_REPLIES:
		c->status = wreplsrv_pull_cycle_wait_send_replies(state);
		break;
	case WREPLSRV_PULL_CYCLE_STAGE_WAIT_STOP_ASSOC:
		c->status = wreplsrv_pull_cycle_wait_stop_assoc(state);
		break;
	case WREPLSRV_PULL_CYCLE_STAGE_DONE:
		c->status = NT_STATUS_INTERNAL_ERROR;
	}

	if (state->stage == WREPLSRV_PULL_CYCLE_STAGE_DONE) {
		c->state  = COMPOSITE_STATE_DONE;
	}

	if (!NT_STATUS_IS_OK(c->status)) {
		c->state = COMPOSITE_STATE_ERROR;
	}

	if (c->state >= COMPOSITE_STATE_DONE && c->async.fn) {
		c->async.fn(c);
	}
}

static void wreplsrv_pull_cycle_handler_creq(struct composite_context *creq)
{
	struct wreplsrv_pull_cycle_state *state = talloc_get_type(creq->async.private_data,
						  struct wreplsrv_pull_cycle_state);
	wreplsrv_pull_cycle_handler(state);
	return;
}

static void wreplsrv_pull_cycle_handler_treq(struct tevent_req *subreq)
{
	struct wreplsrv_pull_cycle_state *state = tevent_req_callback_data(subreq,
						  struct wreplsrv_pull_cycle_state);
	wreplsrv_pull_cycle_handler(state);
	return;
}

struct composite_context *wreplsrv_pull_cycle_send(TALLOC_CTX *mem_ctx, struct wreplsrv_pull_cycle_io *io)
{
	struct composite_context *c = NULL;
	struct wreplsrv_service *service = io->in.partner->service;
	struct wreplsrv_pull_cycle_state *state = NULL;

	c = talloc_zero(mem_ctx, struct composite_context);
	if (!c) goto failed;

	state = talloc_zero(c, struct wreplsrv_pull_cycle_state);
	if (!state) goto failed;
	state->c	= c;
	state->io	= io;

	c->state	= COMPOSITE_STATE_IN_PROGRESS;
	c->event_ctx	= service->task->event_ctx;
	c->private_data	= state;

	state->stage	= WREPLSRV_PULL_CYCLE_STAGE_WAIT_TABLE_REPLY;
	state->table_io.in.partner	= io->in.partner;
	state->table_io.in.num_owners	= io->in.num_owners;
	state->table_io.in.owners	= io->in.owners;
	state->creq = wreplsrv_pull_table_send(state, &state->table_io);
	if (!state->creq) goto failed;

	state->creq->async.fn		= wreplsrv_pull_cycle_handler_creq;
	state->creq->async.private_data	= state;

	return c;
failed:
	talloc_free(c);
	return NULL;
}

NTSTATUS wreplsrv_pull_cycle_recv(struct composite_context *c)
{
	NTSTATUS status;

	status = composite_wait(c);

	talloc_free(c);
	return status;
}

enum wreplsrv_push_notify_stage {
	WREPLSRV_PUSH_NOTIFY_STAGE_WAIT_CONNECT,
	WREPLSRV_PUSH_NOTIFY_STAGE_WAIT_UPDATE,
	WREPLSRV_PUSH_NOTIFY_STAGE_WAIT_INFORM,
	WREPLSRV_PUSH_NOTIFY_STAGE_DONE
};

struct wreplsrv_push_notify_state {
	enum wreplsrv_push_notify_stage stage;
	struct composite_context *c;
	struct wreplsrv_push_notify_io *io;
	enum wrepl_replication_cmd command;
	bool full_table;
	struct wrepl_send_ctrl ctrl;
	struct wrepl_packet req_packet;
	struct wrepl_packet *rep_packet;
	struct composite_context *creq;
	struct wreplsrv_out_connection *wreplconn;
	struct tevent_req *subreq;
};

static void wreplsrv_push_notify_handler_creq(struct composite_context *creq);
static void wreplsrv_push_notify_handler_treq(struct tevent_req *subreq);

static NTSTATUS wreplsrv_push_notify_update(struct wreplsrv_push_notify_state *state)
{
	struct wreplsrv_service *service = state->io->in.partner->service;
	struct wrepl_packet *req = &state->req_packet;
	struct wrepl_replication *repl_out = &state->req_packet.message.replication;
	struct wrepl_table *table_out = &state->req_packet.message.replication.info.table;
	NTSTATUS status;

	/* prepare the outgoing request */
	req->opcode	= WREPL_OPCODE_BITS;
	req->assoc_ctx	= state->wreplconn->assoc_ctx.peer_ctx;
	req->mess_type	= WREPL_REPLICATION;

	repl_out->command = state->command;

	status = wreplsrv_fill_wrepl_table(service, state, table_out,
					   service->wins_db->local_owner, state->full_table);
	NT_STATUS_NOT_OK_RETURN(status);

	/* queue the request */
	state->subreq = wrepl_request_send(state,
					   state->wreplconn->service->task->event_ctx,
					   state->wreplconn->sock, req, NULL);
	NT_STATUS_HAVE_NO_MEMORY(state->subreq);

	tevent_req_set_callback(state->subreq,
				wreplsrv_push_notify_handler_treq,
				state);

	state->stage = WREPLSRV_PUSH_NOTIFY_STAGE_WAIT_UPDATE;

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_push_notify_inform(struct wreplsrv_push_notify_state *state)
{
	struct wreplsrv_service *service = state->io->in.partner->service;
	struct wrepl_packet *req = &state->req_packet;
	struct wrepl_replication *repl_out = &state->req_packet.message.replication;
	struct wrepl_table *table_out = &state->req_packet.message.replication.info.table;
	NTSTATUS status;

	req->opcode	= WREPL_OPCODE_BITS;
	req->assoc_ctx	= state->wreplconn->assoc_ctx.peer_ctx;
	req->mess_type	= WREPL_REPLICATION;

	repl_out->command = state->command;

	status = wreplsrv_fill_wrepl_table(service, state, table_out,
					   service->wins_db->local_owner, state->full_table);
	NT_STATUS_NOT_OK_RETURN(status);

	/* we won't get a reply to a inform message */
	state->ctrl.send_only		= true;

	state->subreq = wrepl_request_send(state,
					   state->wreplconn->service->task->event_ctx,
					   state->wreplconn->sock, req, &state->ctrl);
	NT_STATUS_HAVE_NO_MEMORY(state->subreq);

	tevent_req_set_callback(state->subreq,
				wreplsrv_push_notify_handler_treq,
				state);

	state->stage = WREPLSRV_PUSH_NOTIFY_STAGE_WAIT_INFORM;

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_push_notify_wait_connect(struct wreplsrv_push_notify_state *state)
{
	NTSTATUS status;

	status = wreplsrv_out_connect_recv(state->creq, state, &state->wreplconn);
	NT_STATUS_NOT_OK_RETURN(status);

	/* is the peer doesn't support inform fallback to update */
	switch (state->command) {
	case WREPL_REPL_INFORM:
		if (state->wreplconn->assoc_ctx.peer_major < 5) {
			state->command = WREPL_REPL_UPDATE;
		}
		break;
	case WREPL_REPL_INFORM2:
		if (state->wreplconn->assoc_ctx.peer_major < 5) {
			state->command = WREPL_REPL_UPDATE2;
		}
		break;
	default:
		break;
	}

	switch (state->command) {
	case WREPL_REPL_UPDATE:
		state->full_table = true;
		return wreplsrv_push_notify_update(state);
	case WREPL_REPL_UPDATE2:
		state->full_table = false;
		return wreplsrv_push_notify_update(state);
	case WREPL_REPL_INFORM:
		state->full_table = true;
		return wreplsrv_push_notify_inform(state);
	case WREPL_REPL_INFORM2:
		state->full_table = false;
		return wreplsrv_push_notify_inform(state);
	default:
		return NT_STATUS_INTERNAL_ERROR;
	}
}

static NTSTATUS wreplsrv_push_notify_wait_update(struct wreplsrv_push_notify_state *state)
{
	struct wreplsrv_in_connection *wrepl_in;
	struct tstream_context *stream;
	NTSTATUS status;

	status = wrepl_request_recv(state->subreq, state, NULL);
	TALLOC_FREE(state->subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	/*
	 * now we need to convert the wrepl_socket (client connection)
	 * into a wreplsrv_in_connection (server connection), because
	 * we'll act as a server on this connection after the WREPL_REPL_UPDATE*
	 * message is received by the peer.
	 */

	status = wrepl_socket_split_stream(state->wreplconn->sock, state, &stream);
	NT_STATUS_NOT_OK_RETURN(status);

	/*
	 * now create a wreplsrv_in_connection,
	 * on which we act as server
	 *
	 * NOTE: stream will be stolen by
	 *       wreplsrv_in_connection_merge()
	 */
	status = wreplsrv_in_connection_merge(state->io->in.partner,
					      state->wreplconn->assoc_ctx.peer_ctx,
					      &stream,
					      &wrepl_in);
	NT_STATUS_NOT_OK_RETURN(status);

	/* now we can free the wreplsrv_out_connection */
	TALLOC_FREE(state->wreplconn);

	state->stage = WREPLSRV_PUSH_NOTIFY_STAGE_DONE;
	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_push_notify_wait_inform(struct wreplsrv_push_notify_state *state)
{
	NTSTATUS status;

	status = wrepl_request_recv(state->subreq, state, NULL);
	TALLOC_FREE(state->subreq);
	NT_STATUS_NOT_OK_RETURN(status);

	state->stage = WREPLSRV_PUSH_NOTIFY_STAGE_DONE;
	return status;
}

static void wreplsrv_push_notify_handler(struct wreplsrv_push_notify_state *state)
{
	struct composite_context *c = state->c;

	switch (state->stage) {
	case WREPLSRV_PUSH_NOTIFY_STAGE_WAIT_CONNECT:
		c->status = wreplsrv_push_notify_wait_connect(state);
		break;
	case WREPLSRV_PUSH_NOTIFY_STAGE_WAIT_UPDATE:
		c->status = wreplsrv_push_notify_wait_update(state);
		break;
	case WREPLSRV_PUSH_NOTIFY_STAGE_WAIT_INFORM:
		c->status = wreplsrv_push_notify_wait_inform(state);
		break;
	case WREPLSRV_PUSH_NOTIFY_STAGE_DONE:
		c->status = NT_STATUS_INTERNAL_ERROR;
	}

	if (state->stage == WREPLSRV_PUSH_NOTIFY_STAGE_DONE) {
		c->state  = COMPOSITE_STATE_DONE;
	}

	if (!NT_STATUS_IS_OK(c->status)) {
		c->state = COMPOSITE_STATE_ERROR;
	}

	if (c->state >= COMPOSITE_STATE_DONE && c->async.fn) {
		c->async.fn(c);
	}
}

static void wreplsrv_push_notify_handler_creq(struct composite_context *creq)
{
	struct wreplsrv_push_notify_state *state = talloc_get_type(creq->async.private_data,
						   struct wreplsrv_push_notify_state);
	wreplsrv_push_notify_handler(state);
	return;
}

static void wreplsrv_push_notify_handler_treq(struct tevent_req *subreq)
{
	struct wreplsrv_push_notify_state *state = tevent_req_callback_data(subreq,
						   struct wreplsrv_push_notify_state);
	wreplsrv_push_notify_handler(state);
	return;
}

struct composite_context *wreplsrv_push_notify_send(TALLOC_CTX *mem_ctx, struct wreplsrv_push_notify_io *io)
{
	struct composite_context *c = NULL;
	struct wreplsrv_service *service = io->in.partner->service;
	struct wreplsrv_push_notify_state *state = NULL;
	enum winsrepl_partner_type partner_type;

	c = talloc_zero(mem_ctx, struct composite_context);
	if (!c) goto failed;

	state = talloc_zero(c, struct wreplsrv_push_notify_state);
	if (!state) goto failed;
	state->c	= c;
	state->io	= io;

	if (io->in.inform) {
		/* we can cache the connection in partner->push->wreplconn */
		partner_type = WINSREPL_PARTNER_PUSH;
		if (io->in.propagate) {
			state->command	= WREPL_REPL_INFORM2;
		} else {
			state->command	= WREPL_REPL_INFORM;
		}
	} else {
		/* we can NOT cache the connection */
		partner_type = WINSREPL_PARTNER_NONE;
		if (io->in.propagate) {
			state->command	= WREPL_REPL_UPDATE2;
		} else {
			state->command	= WREPL_REPL_UPDATE;
		}	
	}

	c->state	= COMPOSITE_STATE_IN_PROGRESS;
	c->event_ctx	= service->task->event_ctx;
	c->private_data	= state;

	state->stage	= WREPLSRV_PUSH_NOTIFY_STAGE_WAIT_CONNECT;
	state->creq	= wreplsrv_out_connect_send(io->in.partner, partner_type, NULL);
	if (!state->creq) goto failed;

	state->creq->async.fn		= wreplsrv_push_notify_handler_creq;
	state->creq->async.private_data	= state;

	return c;
failed:
	talloc_free(c);
	return NULL;
}

NTSTATUS wreplsrv_push_notify_recv(struct composite_context *c)
{
	NTSTATUS status;

	status = composite_wait(c);

	talloc_free(c);
	return status;
}
