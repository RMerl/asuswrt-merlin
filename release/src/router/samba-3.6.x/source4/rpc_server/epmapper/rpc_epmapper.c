/* 
   Unix SMB/CIFS implementation.

   endpoint server for the epmapper pipe

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Jelmer Vernooij 2004
   
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
#include "librpc/gen_ndr/ndr_epmapper.h"
#include "rpc_server/dcerpc_server.h"

typedef uint32_t error_status_t;

/* handle types for this module */
enum handle_types {HTYPE_LOOKUP};

/* a endpoint combined with an interface description */
struct dcesrv_ep_iface {
	const char *name;
	struct epm_tower ep;
};

/*
  build a list of all interfaces handled by all endpoint servers
*/
static uint32_t build_ep_list(TALLOC_CTX *mem_ctx,
			      struct dcesrv_endpoint *endpoint_list,
			      struct dcesrv_ep_iface **eps)
{
	struct dcesrv_endpoint *d;
	uint32_t total = 0;
	NTSTATUS status;

	*eps = NULL;

	for (d=endpoint_list; d; d=d->next) {
		struct dcesrv_if_list *iface;
		struct dcerpc_binding *description;

		for (iface=d->interface_list;iface;iface=iface->next) {
			(*eps) = talloc_realloc(mem_ctx, 
						  *eps, 
						  struct dcesrv_ep_iface,
						  total + 1);
			if (!*eps) {
				return 0;
			}
			(*eps)[total].name = iface->iface.name;

			description = d->ep_description;
			description->object = iface->iface.syntax_id;

			status = dcerpc_binding_build_tower(*eps, description, &(*eps)[total].ep);
			if (NT_STATUS_IS_ERR(status)) {
				DEBUG(1, ("Unable to build tower for %s\n", iface->iface.name));
				continue;
			}
			total++;
		}
	}

	return total;
}


static error_status_t dcesrv_epm_Insert(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct epm_Insert *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

static error_status_t dcesrv_epm_Delete(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, 
				 struct epm_Delete *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  implement epm_Lookup. This call is used to enumerate the interfaces
  available on a rpc server
*/
static error_status_t dcesrv_epm_Lookup(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, 
				 struct epm_Lookup *r)
{
	struct dcesrv_handle *h;
	struct rpc_eps {
		uint32_t count;
		struct dcesrv_ep_iface *e;
	} *eps;
	uint32_t num_ents;
	unsigned int i;

	DCESRV_PULL_HANDLE_FAULT(h, r->in.entry_handle, HTYPE_LOOKUP);

	eps = h->data;

	if (!eps) {
		/* this is the first call - fill the list. Subsequent calls 
		   will feed from this list, stored in the handle */
		eps = talloc(h, struct rpc_eps);
		if (!eps) {
			return EPMAPPER_STATUS_NO_MEMORY;
		}
		h->data = eps;

		eps->count = build_ep_list(h, dce_call->conn->dce_ctx->endpoint_list, &eps->e);
	}

	/* return the next N elements */
	num_ents = r->in.max_ents;
	if (num_ents > eps->count) {
		num_ents = eps->count;
	}

	*r->out.entry_handle = h->wire_handle;
	r->out.num_ents = talloc(mem_ctx, uint32_t);
	*r->out.num_ents = num_ents;

	if (num_ents == 0) {
		r->out.entries = NULL;
		ZERO_STRUCTP(r->out.entry_handle);
		talloc_free(h);
		return EPMAPPER_STATUS_NO_MORE_ENTRIES;
	}

	r->out.entries = talloc_array(mem_ctx, struct epm_entry_t, num_ents);
	if (!r->out.entries) {
		return EPMAPPER_STATUS_NO_MEMORY;
	}

	for (i=0;i<num_ents;i++) {
		ZERO_STRUCT(r->out.entries[i].object);
		r->out.entries[i].annotation = eps->e[i].name;
		r->out.entries[i].tower = talloc(mem_ctx, struct epm_twr_t);
		if (!r->out.entries[i].tower) {
			return EPMAPPER_STATUS_NO_MEMORY;
		}
		r->out.entries[i].tower->tower = eps->e[i].ep;
	}

	eps->count -= num_ents;
	eps->e += num_ents;

	return EPMAPPER_STATUS_OK;
}


/*
  implement epm_Map. This is used to find the specific endpoint to talk to given
  a generic protocol tower
*/
static error_status_t dcesrv_epm_Map(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, 
			      struct epm_Map *r)
{
	uint32_t count;
	unsigned int i;
	struct dcesrv_ep_iface *eps;
	struct epm_floor *floors;
	enum dcerpc_transport_t transport;
	struct ndr_syntax_id ndr_syntax;

	count = build_ep_list(mem_ctx, dce_call->conn->dce_ctx->endpoint_list, &eps);

	ZERO_STRUCT(*r->out.entry_handle);
	r->out.num_towers = talloc(mem_ctx, uint32_t);
	if (!r->out.num_towers) {
		return EPMAPPER_STATUS_NO_MEMORY;
	}
	*r->out.num_towers = 1;
	r->out.towers = talloc(mem_ctx, struct epm_twr_p_t);
	if (!r->out.towers) {
		return EPMAPPER_STATUS_NO_MEMORY;
	}
	r->out.towers->twr = talloc(mem_ctx, struct epm_twr_t);
	if (!r->out.towers->twr) {
		return EPMAPPER_STATUS_NO_MEMORY;
	}
	
	if (!r->in.map_tower || r->in.max_towers == 0 || 
	    r->in.map_tower->tower.num_floors < 3) {
		goto failed;
	}

	floors = r->in.map_tower->tower.floors;

	dcerpc_floor_get_lhs_data(&r->in.map_tower->tower.floors[1], &ndr_syntax);

	if (floors[1].lhs.protocol != EPM_PROTOCOL_UUID ||
		!GUID_equal(&ndr_syntax.uuid, &ndr_transfer_syntax.uuid) ||
	    ndr_syntax.if_version != ndr_transfer_syntax.if_version) {
		goto failed;
	}

	transport = dcerpc_transport_by_tower(&r->in.map_tower->tower);

	if (transport == -1) {
		DEBUG(2, ("Client requested unknown transport with levels: "));
		for (i = 2; i < r->in.map_tower->tower.num_floors; i++) {
			DEBUG(2, ("%d, ", r->in.map_tower->tower.floors[i].lhs.protocol));
		}
		DEBUG(2, ("\n"));
		goto failed;
	}

	for (i=0;i<count;i++) {
		if (
			data_blob_cmp(&r->in.map_tower->tower.floors[0].lhs.lhs_data, 
			&eps[i].ep.floors[0].lhs.lhs_data) != 0 
			|| transport != dcerpc_transport_by_tower(&eps[i].ep)) {
			continue;
		}
		
		r->out.towers->twr->tower = eps[i].ep;
		r->out.towers->twr->tower_length = 0;
		return EPMAPPER_STATUS_OK;
	}


failed:
	*r->out.num_towers = 0;
	r->out.towers->twr = NULL;

	return EPMAPPER_STATUS_NO_MORE_ENTRIES;
}

static error_status_t dcesrv_epm_LookupHandleFree(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, 
					   struct epm_LookupHandleFree *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

static error_status_t dcesrv_epm_InqObject(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, 
				    struct epm_InqObject *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

static error_status_t dcesrv_epm_MgmtDelete(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, 
			       struct epm_MgmtDelete *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

static error_status_t dcesrv_epm_MapAuth(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			    struct epm_MapAuth *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_epmapper_s.c"
