/* 
   Unix SMB/CIFS implementation.
   test suite for mgmt rpc operations

   Copyright (C) Andrew Tridgell 2003
   
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
#include "librpc/gen_ndr/ndr_mgmt_c.h"
#include "auth/gensec/gensec.h"
#include "librpc/ndr/ndr_table.h"
#include "torture/rpc/torture_rpc.h"
#include "param/param.h"


/*
  ask the server what interface IDs are available on this endpoint
*/
bool test_inq_if_ids(struct torture_context *tctx, 
		     struct dcerpc_binding_handle *b,
		     TALLOC_CTX *mem_ctx,
		     bool (*per_id_test)(struct torture_context *,
					 const struct ndr_interface_table *iface,
					 TALLOC_CTX *mem_ctx,
					 struct ndr_syntax_id *id),
		     const void *priv)
{
	NTSTATUS status;
	struct mgmt_inq_if_ids r;
	struct rpc_if_id_vector_t *vector;
	int i;

	vector = talloc(mem_ctx, struct rpc_if_id_vector_t);
	r.out.if_id_vector = &vector;
	
	status = dcerpc_mgmt_inq_if_ids_r(b, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("inq_if_ids failed - %s\n", nt_errstr(status));
		return false;
	}

	if (!W_ERROR_IS_OK(r.out.result)) {
		printf("inq_if_ids gave error code %s\n", win_errstr(r.out.result));
		return false;
	}

	if (!vector) {
		printf("inq_if_ids gave NULL if_id_vector\n");
		return false;
	}

	for (i=0;i<vector->count;i++) {
		struct ndr_syntax_id *id = vector->if_id[i].id;
		if (!id) continue;

		printf("\tuuid %s  version 0x%08x  '%s'\n",
		       GUID_string(mem_ctx, &id->uuid),
		       id->if_version,
		       ndr_interface_name(&id->uuid, id->if_version));

		if (per_id_test) {
			per_id_test(tctx, priv, mem_ctx, id);
		}
	}

	return true;
}

static bool test_inq_stats(struct dcerpc_binding_handle *b,
			   TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct mgmt_inq_stats r;
	struct mgmt_statistics statistics;

	r.in.max_count = MGMT_STATS_ARRAY_MAX_SIZE;
	r.in.unknown = 0;
	r.out.statistics = &statistics;

	status = dcerpc_mgmt_inq_stats_r(b, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("inq_stats failed - %s\n", nt_errstr(status));
		return false;
	}

	if (statistics.count != MGMT_STATS_ARRAY_MAX_SIZE) {
		printf("Unexpected array size %d\n", statistics.count);
		return false;
	}

	printf("\tcalls_in %6d  calls_out %6d\n\tpkts_in  %6d  pkts_out  %6d\n",
	       statistics.statistics[MGMT_STATS_CALLS_IN],
	       statistics.statistics[MGMT_STATS_CALLS_OUT],
	       statistics.statistics[MGMT_STATS_PKTS_IN],
	       statistics.statistics[MGMT_STATS_PKTS_OUT]);

	return true;
}

static bool test_inq_princ_name(struct dcerpc_binding_handle *b,
				TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct mgmt_inq_princ_name r;
	int i;
	bool ret = false;

	for (i=0;i<100;i++) {
		r.in.authn_proto = i;  /* DCERPC_AUTH_TYPE_* */
		r.in.princ_name_size = 100;

		status = dcerpc_mgmt_inq_princ_name_r(b, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			continue;
		}
		if (W_ERROR_IS_OK(r.out.result)) {
			const char *name = gensec_get_name_by_authtype(NULL, i);
			ret = true;
			if (name) {
				printf("\tprinciple name for proto %u (%s) is '%s'\n", 
				       i, name, r.out.princ_name);
			} else {
				printf("\tprinciple name for proto %u is '%s'\n", 
				       i, r.out.princ_name);
			}
		}
	}

	if (!ret) {
		printf("\tno principle names?\n");
	}

	return true;
}

static bool test_is_server_listening(struct dcerpc_binding_handle *b,
				     TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct mgmt_is_server_listening r;
	r.out.status = talloc(mem_ctx, uint32_t);

	status = dcerpc_mgmt_is_server_listening_r(b, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("is_server_listening failed - %s\n", nt_errstr(status));
		return false;
	}

	if (*r.out.status != 0 || r.out.result == 0) {
		printf("\tserver is NOT listening\n");
	} else {
		printf("\tserver is listening\n");
	}

	return true;
}

static bool test_stop_server_listening(struct dcerpc_binding_handle *b,
				       TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	struct mgmt_stop_server_listening r;

	status = dcerpc_mgmt_stop_server_listening_r(b, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("stop_server_listening failed - %s\n", nt_errstr(status));
		return false;
	}

	if (!W_ERROR_IS_OK(r.out.result)) {
		printf("\tserver refused to stop listening - %s\n", win_errstr(r.out.result));
	} else {
		printf("\tserver allowed a stop_server_listening request\n");
		return false;
	}

	return true;
}


bool torture_rpc_mgmt(struct torture_context *torture)
{
        NTSTATUS status;
        struct dcerpc_pipe *p;
	TALLOC_CTX *mem_ctx, *loop_ctx;
	bool ret = true;
	const struct ndr_interface_list *l;
	struct dcerpc_binding *b;

	mem_ctx = talloc_init("torture_rpc_mgmt");

	status = torture_rpc_binding(torture, &b);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return false;
	}

	for (l=ndr_table_list();l;l=l->next) {		
		struct dcerpc_binding_handle *bh;

		loop_ctx = talloc_named(mem_ctx, 0, "torture_rpc_mgmt loop context");
		
		/* some interfaces are not mappable */
		if (l->table->num_calls == 0 ||
		    strcmp(l->table->name, "mgmt") == 0) {
			talloc_free(loop_ctx);
			continue;
		}

		printf("\nTesting pipe '%s'\n", l->table->name);

		status = dcerpc_epm_map_binding(loop_ctx, b, l->table, NULL, torture->lp_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			printf("Failed to map port for uuid %s\n", 
				   GUID_string(loop_ctx, &l->table->syntax_id.uuid));
			talloc_free(loop_ctx);
			continue;
		}

		lpcfg_set_cmdline(torture->lp_ctx, "torture:binding", dcerpc_binding_string(loop_ctx, b));

		status = torture_rpc_connection(torture, &p, &ndr_table_mgmt);
		if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			printf("Interface not available - skipping\n");
			talloc_free(loop_ctx);
			continue;
		}
		bh = p->binding_handle;

		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(loop_ctx);
			ret = false;
			continue;
		}

		if (!test_is_server_listening(bh, loop_ctx)) {
			ret = false;
		}

		if (!test_stop_server_listening(bh, loop_ctx)) {
			ret = false;
		}

		if (!test_inq_stats(bh, loop_ctx)) {
			ret = false;
		}

		if (!test_inq_princ_name(bh, loop_ctx)) {
			ret = false;
		}

		if (!test_inq_if_ids(torture, bh, loop_ctx, NULL, NULL)) {
			ret = false;
		}

	}

	return ret;
}
