/*
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Volker Lendecke 2009

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
#include "rpcclient.h"
#include "../librpc/gen_ndr/ndr_epmapper_c.h"
#include "../librpc/gen_ndr/ndr_lsa.h"

static NTSTATUS cmd_epmapper_map(struct rpc_pipe_client *p,
				 TALLOC_CTX *mem_ctx,
				 int argc, const char **argv)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct dcerpc_binding map_binding;
	struct epm_twr_t map_tower;
	struct epm_twr_p_t towers[500];
	struct policy_handle entry_handle;
	struct ndr_syntax_id abstract_syntax;
	uint32_t num_towers;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();
	NTSTATUS status;
	uint32_t result;
	uint32_t i;

	abstract_syntax = ndr_table_lsarpc.syntax_id;

	map_binding.transport = NCACN_NP;
        map_binding.object = abstract_syntax;
        map_binding.host = "127.0.0.1"; /* needed? */
        map_binding.endpoint = "0"; /* correct? needed? */

	status = dcerpc_binding_build_tower(tmp_ctx, &map_binding,
					    &map_tower.tower);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "dcerpc_binding_build_tower returned %s\n",
			  nt_errstr(status));
		return status;
	}

	ZERO_STRUCT(towers);
	ZERO_STRUCT(entry_handle);

	status = dcerpc_epm_Map(
		b, tmp_ctx, &abstract_syntax.uuid,
		&map_tower, &entry_handle, ARRAY_SIZE(towers),
		&num_towers, towers, &result);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "dcerpc_epm_Map returned %s\n",
			  nt_errstr(status));
		return status;
	}

	if (result != EPMAPPER_STATUS_OK) {
		d_fprintf(stderr, "epm_Map returned %u (0x%08X)\n",
			  result, result);
		return NT_STATUS_UNSUCCESSFUL;
	}

	d_printf("num_tower[%u]\n", num_towers);

	for (i=0; i < num_towers; i++) {
		struct dcerpc_binding *binding;

		if (towers[i].twr == NULL) {
			d_fprintf(stderr, "tower[%u] NULL\n", i);
			break;
		}

		status = dcerpc_binding_from_tower(tmp_ctx, &towers[i].twr->tower,
						   &binding);
		if (!NT_STATUS_IS_OK(status)) {
			break;
		}

		d_printf("tower[%u] %s\n", i, dcerpc_binding_string(tmp_ctx, binding));
	}

	return NT_STATUS_OK;
}

static NTSTATUS cmd_epmapper_lookup(struct rpc_pipe_client *p,
				    TALLOC_CTX *mem_ctx,
				    int argc, const char **argv)
{
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct policy_handle entry_handle;

	ZERO_STRUCT(entry_handle);

	while (true) {
		TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
		uint32_t num_entries;
		struct epm_entry_t entry;
		NTSTATUS status;
		char *guid_string;
		struct dcerpc_binding *binding;
		uint32_t result;

		status = dcerpc_epm_Lookup(b, tmp_ctx,
				   0, /* rpc_c_ep_all */
				   NULL,
				   NULL,
				   0, /* rpc_c_vers_all */
				   &entry_handle,
				   1, /* max_ents */
				   &num_entries, &entry,
				   &result);
		if (!NT_STATUS_IS_OK(status)) {
			d_fprintf(stderr, "dcerpc_epm_Lookup returned %s\n",
				  nt_errstr(status));
			break;
		}

		if (result == EPMAPPER_STATUS_NO_MORE_ENTRIES) {
			d_fprintf(stderr, "epm_Lookup no more entries\n");
			break;
		}

		if (result != EPMAPPER_STATUS_OK) {
			d_fprintf(stderr, "epm_Lookup returned %u (0x%08X)\n",
				  result, result);
			break;
		}

		if (num_entries != 1) {
			d_fprintf(stderr, "epm_Lookup returned %d "
				  "entries, expected one\n", (int)num_entries);
			break;
		}

		guid_string = GUID_string(tmp_ctx, &entry.object);
		if (guid_string == NULL) {
			break;
		}

		status = dcerpc_binding_from_tower(tmp_ctx, &entry.tower->tower,
						   &binding);
		if (!NT_STATUS_IS_OK(status)) {
			break;
		}

		d_printf("%s %s: %s\n", guid_string,
			 dcerpc_binding_string(tmp_ctx, binding),
			 entry.annotation);

		TALLOC_FREE(tmp_ctx);
	}

	return NT_STATUS_OK;
}


/* List of commands exported by this module */

struct cmd_set epmapper_commands[] = {

	{ "EPMAPPER" },

	{ "epmmap", RPC_RTYPE_NTSTATUS, cmd_epmapper_map,     NULL,
	  &ndr_table_epmapper.syntax_id, NULL, "Map a binding", "" },
	{ "epmlookup", RPC_RTYPE_NTSTATUS, cmd_epmapper_lookup,     NULL,
	  &ndr_table_epmapper.syntax_id, NULL, "Lookup bindings", "" },
	{ NULL }
};
