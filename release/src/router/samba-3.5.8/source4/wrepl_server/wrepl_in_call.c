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
#include "smbd/service_stream.h"
#include "libcli/wrepl/winsrepl.h"
#include "wrepl_server/wrepl_server.h"
#include "libcli/composite/composite.h"
#include "nbt_server/wins/winsdb.h"
#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_errors.h"
#include "system/time.h"

static NTSTATUS wreplsrv_in_start_association(struct wreplsrv_in_call *call)
{
	struct wrepl_start *start	= &call->req_packet.message.start;
	struct wrepl_start *start_reply	= &call->rep_packet.message.start_reply;

	if (call->req_packet.opcode & WREPL_OPCODE_BITS) {
		/*
		 *if the assoc_ctx doesn't match ignore the packet
		 */
		if ((call->req_packet.assoc_ctx != call->wreplconn->assoc_ctx.our_ctx)
		   && (call->req_packet.assoc_ctx != 0)) {
			return ERROR_INVALID_PARAMETER;
		}
	} else {
		call->wreplconn->assoc_ctx.our_ctx = WREPLSRV_INVALID_ASSOC_CTX;
		return NT_STATUS_OK;
	}

/*
 * it seems that we don't know all details about the start_association
 * to support replication with NT4 (it sends 1.1 instead of 5.2)
 * we ignore the version numbers until we know all details
 */
#if 0
	if (start->minor_version != 2 || start->major_version != 5) {
		/* w2k terminate the connection if the versions doesn't match */
		return NT_STATUS_UNKNOWN_REVISION;
	}
#endif

	call->wreplconn->assoc_ctx.stopped	= false;
	call->wreplconn->assoc_ctx.our_ctx	= WREPLSRV_VALID_ASSOC_CTX;
	call->wreplconn->assoc_ctx.peer_ctx	= start->assoc_ctx;

	call->rep_packet.mess_type		= WREPL_START_ASSOCIATION_REPLY;
	start_reply->assoc_ctx			= call->wreplconn->assoc_ctx.our_ctx;
	start_reply->minor_version		= 2;
	start_reply->major_version		= 5;

	/*
	 * nt4 uses 41 bytes for the start_association call
	 * so do it the same and as we don't know the meanings of this bytes
	 * we just send zeros and nt4, w2k and w2k3 seems to be happy with this
	 *
	 * if we don't do this nt4 uses an old version of the wins replication protocol
	 * and that would break nt4 <-> samba replication
	 */
	call->rep_packet.padding		= data_blob_talloc(call, NULL, 21);
	NT_STATUS_HAVE_NO_MEMORY(call->rep_packet.padding.data);

	memset(call->rep_packet.padding.data, 0, call->rep_packet.padding.length);

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_in_stop_assoc_ctx(struct wreplsrv_in_call *call)
{
	struct wrepl_stop *stop_out		= &call->rep_packet.message.stop;

	call->wreplconn->assoc_ctx.stopped	= true;

	call->rep_packet.mess_type		= WREPL_STOP_ASSOCIATION;
	stop_out->reason			= 4;

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_in_stop_association(struct wreplsrv_in_call *call)
{
	/*
	 * w2k only check the assoc_ctx if the opcode has the 0x00007800 bits are set
	 */
	if (call->req_packet.opcode & WREPL_OPCODE_BITS) {
		/*
		 *if the assoc_ctx doesn't match ignore the packet
		 */
		if (call->req_packet.assoc_ctx != call->wreplconn->assoc_ctx.our_ctx) {
			return ERROR_INVALID_PARAMETER;
		}
		/* when the opcode bits are set the connection should be directly terminated */
		return NT_STATUS_CONNECTION_RESET;
	}

	if (call->wreplconn->assoc_ctx.stopped) {
		/* this causes the connection to be directly terminated */
		return NT_STATUS_CONNECTION_RESET;
	}

	/* this will cause to not receive packets anymore and terminate the connection if the reply is send */
	call->terminate_after_send = true;
	return wreplsrv_in_stop_assoc_ctx(call);
}

static NTSTATUS wreplsrv_in_table_query(struct wreplsrv_in_call *call)
{
	struct wreplsrv_service *service = call->wreplconn->service;
	struct wrepl_replication *repl_out = &call->rep_packet.message.replication;
	struct wrepl_table *table_out = &call->rep_packet.message.replication.info.table;

	repl_out->command = WREPL_REPL_TABLE_REPLY;

	return wreplsrv_fill_wrepl_table(service, call, table_out,
					 service->wins_db->local_owner, true);
}

static int wreplsrv_in_sort_wins_name(struct wrepl_wins_name *n1,
				      struct wrepl_wins_name *n2)
{
	if (n1->id < n2->id) return -1;
	if (n1->id > n2->id) return 1;
	return 0;
}

static NTSTATUS wreplsrv_record2wins_name(TALLOC_CTX *mem_ctx,
					  struct wrepl_wins_name *name,
					  struct winsdb_record *rec)
{
	uint32_t num_ips, i;
	struct wrepl_ip *ips;

	name->name		= rec->name;
	talloc_steal(mem_ctx, rec->name);

	name->id		= rec->version;
	name->unknown		= "255.255.255.255";

	name->flags		= WREPL_NAME_FLAGS(rec->type, rec->state, rec->node, rec->is_static);

	switch (name->flags & 2) {
	case 0:
		name->addresses.ip			= rec->addresses[0]->address;
		talloc_steal(mem_ctx, rec->addresses[0]->address);
		break;
	case 2:
		num_ips	= winsdb_addr_list_length(rec->addresses);
		ips	= talloc_array(mem_ctx, struct wrepl_ip, num_ips);
		NT_STATUS_HAVE_NO_MEMORY(ips);

		for (i = 0; i < num_ips; i++) {
			ips[i].owner	= rec->addresses[i]->wins_owner;
			talloc_steal(ips, rec->addresses[i]->wins_owner);
			ips[i].ip	= rec->addresses[i]->address;
			talloc_steal(ips, rec->addresses[i]->address);
		}

		name->addresses.addresses.num_ips	= num_ips;
		name->addresses.addresses.ips		= ips;
		break;
	}

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_in_send_request(struct wreplsrv_in_call *call)
{
	struct wreplsrv_service *service = call->wreplconn->service;
	struct wrepl_wins_owner *owner_in = &call->req_packet.message.replication.info.owner;
	struct wrepl_replication *repl_out = &call->rep_packet.message.replication;
	struct wrepl_send_reply *reply_out = &call->rep_packet.message.replication.info.reply;
	struct wreplsrv_owner *owner;
	const char *owner_filter;
	const char *filter;
	struct ldb_result *res = NULL;
	int ret;
	struct wrepl_wins_name *names;
	struct winsdb_record *rec;
	NTSTATUS status;
	uint32_t i, j;
	time_t now = time(NULL);

	owner = wreplsrv_find_owner(service, service->table, owner_in->address);

	repl_out->command	= WREPL_REPL_SEND_REPLY;
	reply_out->num_names	= 0;
	reply_out->names	= NULL;

	/*
	 * if we didn't know this owner, must be a bug in the partners client code...
	 * return an empty list.
	 */
	if (!owner) {
		DEBUG(2,("WINSREPL:reply [0] records unknown owner[%s] to partner[%s]\n",
			owner_in->address, call->wreplconn->partner->address));
		return NT_STATUS_OK;
	}

	/*
	 * the client sends a max_version of 0, interpret it as
	 * (uint64_t)-1
	 */
	if (owner_in->max_version == 0) {
		owner_in->max_version = (uint64_t)-1;
	}

	/*
	 * if the partner ask for nothing, or give invalid ranges,
	 * return an empty list.
	 */
	if (owner_in->min_version > owner_in->max_version) {
		DEBUG(2,("WINSREPL:reply [0] records owner[%s] min[%llu] max[%llu] to partner[%s]\n",
			owner_in->address, 
			(long long)owner_in->min_version, 
			(long long)owner_in->max_version,
			call->wreplconn->partner->address));
		return NT_STATUS_OK;
	}

	/*
	 * if the partner has already all records for nothing, or give invalid ranges,
	 * return an empty list.
	 */
	if (owner_in->min_version > owner->owner.max_version) {
		DEBUG(2,("WINSREPL:reply [0] records owner[%s] min[%llu] max[%llu] to partner[%s]\n",
			owner_in->address, 
			(long long)owner_in->min_version, 
			(long long)owner_in->max_version,
			call->wreplconn->partner->address));
		return NT_STATUS_OK;
	}

	owner_filter = wreplsrv_owner_filter(service, call, owner->owner.address);
	NT_STATUS_HAVE_NO_MEMORY(owner_filter);
	filter = talloc_asprintf(call,
				 "(&%s(objectClass=winsRecord)"
				 "(|(recordState=%u)(recordState=%u))"
				 "(versionID>=%llu)(versionID<=%llu))",
				 owner_filter,
				 WREPL_STATE_ACTIVE, WREPL_STATE_TOMBSTONE,
				 (long long)owner_in->min_version, 
				 (long long)owner_in->max_version);
	NT_STATUS_HAVE_NO_MEMORY(filter);
	ret = ldb_search(service->wins_db->ldb, call, &res, NULL, LDB_SCOPE_SUBTREE, NULL, "%s", filter);
	if (ret != LDB_SUCCESS) return NT_STATUS_INTERNAL_DB_CORRUPTION;
	DEBUG(10,("WINSREPL: filter '%s' count %d\n", filter, res->count));

	if (res->count == 0) {
		DEBUG(2,("WINSREPL:reply [%u] records owner[%s] min[%llu] max[%llu] to partner[%s]\n",
			res->count, owner_in->address, 
			(long long)owner_in->min_version, 
			(long long)owner_in->max_version,
			call->wreplconn->partner->address));
		return NT_STATUS_OK;
	}

	names = talloc_array(call, struct wrepl_wins_name, res->count);
	NT_STATUS_HAVE_NO_MEMORY(names);

	for (i=0, j=0; i < res->count; i++) {
		status = winsdb_record(service->wins_db, res->msgs[i], call, now, &rec);
		NT_STATUS_NOT_OK_RETURN(status);

		/*
		 * it's possible that winsdb_record() made the record RELEASED
		 * because it's expired, but in the database it's still stored
		 * as ACTIVE...
		 *
		 * make sure we really only replicate ACTIVE and TOMBSTONE records
		 */
		if (rec->state == WREPL_STATE_ACTIVE || rec->state == WREPL_STATE_TOMBSTONE) {
			status = wreplsrv_record2wins_name(names, &names[j], rec);
			NT_STATUS_NOT_OK_RETURN(status);
			j++;
		}

		talloc_free(rec);
		talloc_free(res->msgs[i]);
	}

	/* sort the names before we send them */
	qsort(names, j, sizeof(struct wrepl_wins_name), (comparison_fn_t)wreplsrv_in_sort_wins_name);

	DEBUG(2,("WINSREPL:reply [%u] records owner[%s] min[%llu] max[%llu] to partner[%s]\n",
		j, owner_in->address, 
		(long long)owner_in->min_version, 
		(long long)owner_in->max_version,
		call->wreplconn->partner->address));

	reply_out->num_names	= j;
	reply_out->names	= names;

	return NT_STATUS_OK;
}

struct wreplsrv_in_update_state {
	struct wreplsrv_in_connection *wrepl_in;
	struct wreplsrv_out_connection *wrepl_out;
	struct composite_context *creq;
	struct wreplsrv_pull_cycle_io cycle_io;
};

static void wreplsrv_in_update_handler(struct composite_context *creq)
{
	struct wreplsrv_in_update_state *update_state = talloc_get_type(creq->async.private_data,
							struct wreplsrv_in_update_state);
	NTSTATUS status;

	status = wreplsrv_pull_cycle_recv(creq);

	talloc_free(update_state->wrepl_out);

	wreplsrv_terminate_in_connection(update_state->wrepl_in, nt_errstr(status));
}

static NTSTATUS wreplsrv_in_update(struct wreplsrv_in_call *call)
{
	struct wreplsrv_in_connection *wrepl_in = call->wreplconn;
	struct wreplsrv_out_connection *wrepl_out;
	struct wrepl_table *update_in = &call->req_packet.message.replication.info.table;
	struct wreplsrv_in_update_state *update_state;
	uint16_t fde_flags;

	DEBUG(2,("WREPL_REPL_UPDATE: partner[%s] initiator[%s] num_owners[%u]\n",
		call->wreplconn->partner->address,
		update_in->initiator, update_in->partner_count));

	/* 
	 * we need to flip the connection into a client connection
	 * and do a WREPL_REPL_SEND_REQUEST's on the that connection
	 * and then stop this connection
	 */
	fde_flags = event_get_fd_flags(wrepl_in->conn->event.fde);
	talloc_free(wrepl_in->conn->event.fde);
	wrepl_in->conn->event.fde = NULL;

	update_state = talloc(wrepl_in, struct wreplsrv_in_update_state);
	NT_STATUS_HAVE_NO_MEMORY(update_state);

	wrepl_out = talloc(update_state, struct wreplsrv_out_connection);
	NT_STATUS_HAVE_NO_MEMORY(wrepl_out);
	wrepl_out->service		= wrepl_in->service;
	wrepl_out->partner		= wrepl_in->partner;
	wrepl_out->assoc_ctx.our_ctx	= wrepl_in->assoc_ctx.our_ctx;
	wrepl_out->assoc_ctx.peer_ctx	= wrepl_in->assoc_ctx.peer_ctx;
	wrepl_out->sock			= wrepl_socket_merge(wrepl_out,
							     wrepl_in->conn->event.ctx,
							     wrepl_in->conn->socket,
							     wrepl_in->packet);
	NT_STATUS_HAVE_NO_MEMORY(wrepl_out->sock);

	event_set_fd_flags(wrepl_out->sock->event.fde, fde_flags);

	update_state->wrepl_in			= wrepl_in;
	update_state->wrepl_out			= wrepl_out;
	update_state->cycle_io.in.partner	= wrepl_out->partner;
	update_state->cycle_io.in.num_owners	= update_in->partner_count;
	update_state->cycle_io.in.owners	= update_in->partners;
	talloc_steal(update_state, update_in->partners);
	update_state->cycle_io.in.wreplconn	= wrepl_out;
	update_state->creq = wreplsrv_pull_cycle_send(update_state, &update_state->cycle_io);
	if (!update_state->creq) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	update_state->creq->async.fn		= wreplsrv_in_update_handler;
	update_state->creq->async.private_data	= update_state;

	return ERROR_INVALID_PARAMETER;
}

static NTSTATUS wreplsrv_in_update2(struct wreplsrv_in_call *call)
{
	return wreplsrv_in_update(call);
}

static NTSTATUS wreplsrv_in_inform(struct wreplsrv_in_call *call)
{
	struct wrepl_table *inform_in = &call->req_packet.message.replication.info.table;

	DEBUG(2,("WREPL_REPL_INFORM: partner[%s] initiator[%s] num_owners[%u]\n",
		call->wreplconn->partner->address,
		inform_in->initiator, inform_in->partner_count));

	wreplsrv_out_partner_pull(call->wreplconn->partner, inform_in);

	/* we don't reply to WREPL_REPL_INFORM messages */
	return ERROR_INVALID_PARAMETER;
}

static NTSTATUS wreplsrv_in_inform2(struct wreplsrv_in_call *call)
{
	return wreplsrv_in_inform(call);
}

static NTSTATUS wreplsrv_in_replication(struct wreplsrv_in_call *call)
{
	struct wrepl_replication *repl_in = &call->req_packet.message.replication;
	NTSTATUS status;

	/*
	 * w2k only check the assoc_ctx if the opcode has the 0x00007800 bits are set
	 */
	if (call->req_packet.opcode & WREPL_OPCODE_BITS) {
		/*
		 *if the assoc_ctx doesn't match ignore the packet
		 */
		if (call->req_packet.assoc_ctx != call->wreplconn->assoc_ctx.our_ctx) {
			return ERROR_INVALID_PARAMETER;
		}
	}

	if (!call->wreplconn->partner) {
		struct socket_address *partner_ip = socket_get_peer_addr(call->wreplconn->conn->socket, call);

		call->wreplconn->partner = wreplsrv_find_partner(call->wreplconn->service, partner_ip->addr);
		if (!call->wreplconn->partner) {
			DEBUG(1,("Failing WINS replication from non-partner %s\n",
				 partner_ip ? partner_ip->addr : NULL));
			return wreplsrv_in_stop_assoc_ctx(call);
		}
	}

	switch (repl_in->command) {
		case WREPL_REPL_TABLE_QUERY:
			if (!(call->wreplconn->partner->type & WINSREPL_PARTNER_PUSH)) {
				DEBUG(0,("Failing WINS replication TABLE_QUERY from non-push-partner %s\n",
					 call->wreplconn->partner->address));
				return wreplsrv_in_stop_assoc_ctx(call);
			}
			status = wreplsrv_in_table_query(call);
			break;

		case WREPL_REPL_TABLE_REPLY:
			return ERROR_INVALID_PARAMETER;

		case WREPL_REPL_SEND_REQUEST:
			if (!(call->wreplconn->partner->type & WINSREPL_PARTNER_PUSH)) {
				DEBUG(0,("Failing WINS replication SEND_REQUESET from non-push-partner %s\n",
					 call->wreplconn->partner->address));
				return wreplsrv_in_stop_assoc_ctx(call);
			}
			status = wreplsrv_in_send_request(call);
			break;

		case WREPL_REPL_SEND_REPLY:
			return ERROR_INVALID_PARAMETER;
	
		case WREPL_REPL_UPDATE:
			if (!(call->wreplconn->partner->type & WINSREPL_PARTNER_PULL)) {
				DEBUG(0,("Failing WINS replication UPDATE from non-pull-partner %s\n",
					 call->wreplconn->partner->address));
				return wreplsrv_in_stop_assoc_ctx(call);
			}
			status = wreplsrv_in_update(call);
			break;

		case WREPL_REPL_UPDATE2:
			if (!(call->wreplconn->partner->type & WINSREPL_PARTNER_PULL)) {
				DEBUG(0,("Failing WINS replication UPDATE2 from non-pull-partner %s\n",
					 call->wreplconn->partner->address));
				return wreplsrv_in_stop_assoc_ctx(call);
			}
			status = wreplsrv_in_update2(call);
			break;

		case WREPL_REPL_INFORM:
			if (!(call->wreplconn->partner->type & WINSREPL_PARTNER_PULL)) {
				DEBUG(0,("Failing WINS replication INFORM from non-pull-partner %s\n",
					 call->wreplconn->partner->address));
				return wreplsrv_in_stop_assoc_ctx(call);
			}
			status = wreplsrv_in_inform(call);
			break;

		case WREPL_REPL_INFORM2:
			if (!(call->wreplconn->partner->type & WINSREPL_PARTNER_PULL)) {
				DEBUG(0,("Failing WINS replication INFORM2 from non-pull-partner %s\n",
					 call->wreplconn->partner->address));
				return wreplsrv_in_stop_assoc_ctx(call);
			}
			status = wreplsrv_in_inform2(call);
			break;

		default:
			return ERROR_INVALID_PARAMETER;
	}

	if (NT_STATUS_IS_OK(status)) {
		call->rep_packet.mess_type = WREPL_REPLICATION;
	}

	return status;
}

static NTSTATUS wreplsrv_in_invalid_assoc_ctx(struct wreplsrv_in_call *call)
{
	struct wrepl_start *start	= &call->rep_packet.message.start;

	call->rep_packet.opcode		= 0x00008583;
	call->rep_packet.assoc_ctx	= 0;
	call->rep_packet.mess_type	= WREPL_START_ASSOCIATION;

	start->assoc_ctx		= 0x0000000a;
	start->minor_version		= 0x0001;
	start->major_version		= 0x0000;

	call->rep_packet.padding	= data_blob_talloc(call, NULL, 4);
	memset(call->rep_packet.padding.data, '\0', call->rep_packet.padding.length);

	return NT_STATUS_OK;
}

NTSTATUS wreplsrv_in_call(struct wreplsrv_in_call *call)
{
	NTSTATUS status;

	if (!(call->req_packet.opcode & WREPL_OPCODE_BITS)
	    && (call->wreplconn->assoc_ctx.our_ctx == WREPLSRV_INVALID_ASSOC_CTX)) {
		return wreplsrv_in_invalid_assoc_ctx(call);
	}

	switch (call->req_packet.mess_type) {
		case WREPL_START_ASSOCIATION:
			status = wreplsrv_in_start_association(call);
			break;
		case WREPL_START_ASSOCIATION_REPLY:
			/* this is not valid here, so we ignore it */
			return ERROR_INVALID_PARAMETER;

		case WREPL_STOP_ASSOCIATION:
			status = wreplsrv_in_stop_association(call);
			break;

		case WREPL_REPLICATION:
			status = wreplsrv_in_replication(call);
			break;
		default:
			/* everythingelse is also not valid here, so we ignore it */
			return ERROR_INVALID_PARAMETER;
	}

	if (call->wreplconn->assoc_ctx.our_ctx == WREPLSRV_INVALID_ASSOC_CTX) {
		return wreplsrv_in_invalid_assoc_ctx(call);
	}

	if (NT_STATUS_IS_OK(status)) {
		/* let the backend to set some of the opcode bits, but always add the standards */
		call->rep_packet.opcode		|= WREPL_OPCODE_BITS;
		call->rep_packet.assoc_ctx	= call->wreplconn->assoc_ctx.peer_ctx;
	}

	return status;
}
