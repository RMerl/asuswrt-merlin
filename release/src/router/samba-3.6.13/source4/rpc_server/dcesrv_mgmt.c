/* 
   Unix SMB/CIFS implementation.

   endpoint server for the mgmt pipe

   Copyright (C) Jelmer Vernooij 2006
   
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
#include "rpc_server/dcerpc_server.h"
#include "librpc/gen_ndr/ndr_mgmt.h"

/* 
  mgmt_inq_if_ids 
*/
static WERROR dcesrv_mgmt_inq_if_ids(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct mgmt_inq_if_ids *r)
{
	const struct dcesrv_endpoint *ep = dce_call->conn->endpoint;
	struct dcesrv_if_list *l;
	struct rpc_if_id_vector_t *vector;

	vector = *r->out.if_id_vector = talloc(mem_ctx, struct rpc_if_id_vector_t);
	vector->count = 0;
	vector->if_id = NULL;
	for (l = ep->interface_list; l; l = l->next) {
		vector->count++;
		vector->if_id = talloc_realloc(mem_ctx, vector->if_id, struct ndr_syntax_id_p, vector->count);
		vector->if_id[vector->count-1].id = &l->iface.syntax_id;
	}
	return WERR_OK;
}


/* 
  mgmt_inq_stats 
*/
static WERROR dcesrv_mgmt_inq_stats(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct mgmt_inq_stats *r)
{
	if (r->in.max_count != MGMT_STATS_ARRAY_MAX_SIZE)
		return WERR_NOT_SUPPORTED;

	r->out.statistics->count = r->in.max_count;
	r->out.statistics->statistics = talloc_array(mem_ctx, uint32_t, r->in.max_count);
	/* FIXME */
	r->out.statistics->statistics[MGMT_STATS_CALLS_IN] = 0;
	r->out.statistics->statistics[MGMT_STATS_CALLS_OUT] = 0;
	r->out.statistics->statistics[MGMT_STATS_PKTS_IN] = 0;
	r->out.statistics->statistics[MGMT_STATS_PKTS_OUT] = 0;

	return WERR_OK;
}


/* 
  mgmt_is_server_listening 
*/
static uint32_t dcesrv_mgmt_is_server_listening(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct mgmt_is_server_listening *r)
{
	*r->out.status = 0;
	return 1;
}


/* 
  mgmt_stop_server_listening 
*/
static WERROR dcesrv_mgmt_stop_server_listening(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct mgmt_stop_server_listening *r)
{
	return WERR_ACCESS_DENIED;
}


/* 
  mgmt_inq_princ_name 
*/
static WERROR dcesrv_mgmt_inq_princ_name(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct mgmt_inq_princ_name *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_mgmt_s.c"
