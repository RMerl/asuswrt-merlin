/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Almost completely rewritten by (C) Jeremy Allison 2005 - 2010
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
#include "librpc/rpc/dcerpc.h"
#include "srv_pipe_internal.h"
#include "rpc_server/srv_pipe_register.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

struct rpc_table {
	struct {
		const char *clnt;
		const char *srv;
	} pipe;
	struct ndr_syntax_id rpc_interface;
	const struct api_struct *cmds;
	uint32_t n_cmds;
	bool (*shutdown_fn)(void *private_data);
	void *shutdown_data;
};

static struct rpc_table *rpc_lookup;
static uint32_t rpc_lookup_size;

static struct rpc_table *rpc_srv_get_pipe_by_id(const struct ndr_syntax_id *id)
{
	uint32_t i;

	for (i = 0; i < rpc_lookup_size; i++) {
		if (ndr_syntax_id_equal(&rpc_lookup[i].rpc_interface, id)) {
			return &rpc_lookup[i];
		}
	}

	return NULL;
}

bool rpc_srv_pipe_exists_by_id(const struct ndr_syntax_id *id)
{
	uint32_t i;

	for (i = 0; i < rpc_lookup_size; i++) {
		if (ndr_syntax_id_equal(&rpc_lookup[i].rpc_interface, id)) {
			return true;
		}
	}

	return false;
}

bool rpc_srv_pipe_exists_by_cli_name(const char *cli_name)
{
	uint32_t i;

	for (i = 0; i < rpc_lookup_size; i++) {
		if (strequal(rpc_lookup[i].pipe.clnt, cli_name)) {
			return true;
		}
	}

	return false;
}

bool rpc_srv_pipe_exists_by_srv_name(const char *srv_name)
{
	uint32_t i;

	for (i = 0; i < rpc_lookup_size; i++) {
		if (strequal(rpc_lookup[i].pipe.srv, srv_name)) {
			return true;
		}
	}

	return false;
}

const char *rpc_srv_get_pipe_cli_name(const struct ndr_syntax_id *id)
{
	uint32_t i;

	for (i = 0; i < rpc_lookup_size; i++) {
		if (ndr_syntax_id_equal(&rpc_lookup[i].rpc_interface, id)) {
			return rpc_lookup[i].pipe.clnt;
		}
	}

	return NULL;
}

const char *rpc_srv_get_pipe_srv_name(const struct ndr_syntax_id *id)
{
	uint32_t i;

	for (i = 0; i < rpc_lookup_size; i++) {
		if (ndr_syntax_id_equal(&rpc_lookup[i].rpc_interface, id)) {
			return rpc_lookup[i].pipe.srv;
		}
	}

	return NULL;
}

uint32_t rpc_srv_get_pipe_num_cmds(const struct ndr_syntax_id *id)
{
	uint32_t i;

	for (i = 0; i < rpc_lookup_size; i++) {
		if (ndr_syntax_id_equal(&rpc_lookup[i].rpc_interface, id)) {
			return rpc_lookup[i].n_cmds;
		}
	}

	return 0;
}

const struct api_struct *rpc_srv_get_pipe_cmds(const struct ndr_syntax_id *id)
{
	uint32_t i;

	for (i = 0; i < rpc_lookup_size; i++) {
		if (ndr_syntax_id_equal(&rpc_lookup[i].rpc_interface, id)) {
			return rpc_lookup[i].cmds;
		}
	}

	return NULL;
}

bool rpc_srv_get_pipe_interface_by_cli_name(const char *cli_name,
					    struct ndr_syntax_id *id)
{
	uint32_t i;

	for (i = 0; i < rpc_lookup_size; i++) {
		if (strequal(rpc_lookup[i].pipe.clnt, cli_name)) {
			if (id) {
				*id = rpc_lookup[i].rpc_interface;
			}
			return true;
		}
	}

	return false;
}

/*******************************************************************
 Register commands to an RPC pipe
*******************************************************************/

NTSTATUS rpc_srv_register(int version, const char *clnt, const char *srv,
			  const struct ndr_interface_table *iface,
			  const struct api_struct *cmds, int size,
			  const struct rpc_srv_callbacks *rpc_srv_cb)
{
	struct rpc_table *rpc_entry;

	if (!clnt || !srv || !cmds) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (version != SMB_RPC_INTERFACE_VERSION) {
		DEBUG(0,("Can't register rpc commands!\n"
			 "You tried to register a rpc module with SMB_RPC_INTERFACE_VERSION %d"
			 ", while this version of samba uses version %d!\n",
			 version,SMB_RPC_INTERFACE_VERSION));
		return NT_STATUS_OBJECT_TYPE_MISMATCH;
	}

	/* Don't register the same command twice */
	if (rpc_srv_pipe_exists_by_id(&iface->syntax_id)) {
		return NT_STATUS_OK;
	}

	/*
	 * We use a temporary variable because this call can fail and
	 * rpc_lookup will still be valid afterwards.  It could then succeed if
	 * called again later
	 */
	rpc_lookup_size++;
	rpc_entry = SMB_REALLOC_ARRAY_KEEP_OLD_ON_ERROR(rpc_lookup, struct rpc_table, rpc_lookup_size);
	if (NULL == rpc_entry) {
		rpc_lookup_size--;
		DEBUG(0, ("rpc_srv_register: memory allocation failed\n"));
		return NT_STATUS_NO_MEMORY;
	} else {
		rpc_lookup = rpc_entry;
	}

	rpc_entry = rpc_lookup + (rpc_lookup_size - 1);
	ZERO_STRUCTP(rpc_entry);
	rpc_entry->pipe.clnt = SMB_STRDUP(clnt);
	rpc_entry->pipe.srv = SMB_STRDUP(srv);
	rpc_entry->rpc_interface = iface->syntax_id;
	rpc_entry->cmds = cmds;
	rpc_entry->n_cmds = size;

	if (rpc_srv_cb != NULL) {
		rpc_entry->shutdown_fn = rpc_srv_cb->shutdown;
		rpc_entry->shutdown_data = rpc_srv_cb->private_data;

		if (rpc_srv_cb->init != NULL &&
		    !rpc_srv_cb->init(rpc_srv_cb->private_data)) {
			DEBUG(0, ("rpc_srv_register: Failed to call the %s "
				  "init function!\n", srv));
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	return NT_STATUS_OK;
}

NTSTATUS rpc_srv_unregister(const struct ndr_interface_table *iface)
{
	struct rpc_table *rpc_entry = rpc_srv_get_pipe_by_id(&iface->syntax_id);

	if (rpc_entry != NULL && rpc_entry->shutdown_fn != NULL) {
		if (!rpc_entry->shutdown_fn(rpc_entry->shutdown_data)) {
			DEBUG(0, ("rpc_srv_unregister: Failed to call the %s "
				  "init function!\n", rpc_entry->pipe.srv));
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	return NT_STATUS_OK;
}
