/* 
   Unix SMB/CIFS implementation.

   local (dummy) clustering operations

   Copyright (C) Andrew Tridgell 2006
   
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
#include "cluster/cluster.h"
#include "cluster/cluster_private.h"
#include "../tdb/include/tdb.h"
#include "tdb_wrap.h"
#include "system/filesys.h"
#include "param/param.h"
#include "librpc/gen_ndr/server_id.h"

/*
  server a server_id for the local node
*/
static struct server_id local_id(struct cluster_ops *ops, uint64_t id, uint32_t id2)
{
	struct server_id server_id;
	ZERO_STRUCT(server_id);
	server_id.id = id;
	server_id.id2 = id2;
	return server_id;
}


/*
  return a server_id as a string
*/
static const char *local_id_string(struct cluster_ops *ops,
				   TALLOC_CTX *mem_ctx, struct server_id id)
{
	return talloc_asprintf(mem_ctx, "%u.%llu.%u", id.node, (unsigned long long)id.id, id.id2);
}


/*
  open a tmp tdb for the local node. By using smbd_tmp_path() we don't need
  TDB_CLEAR_IF_FIRST as the tmp path is wiped at startup
*/
static struct tdb_wrap *local_tdb_tmp_open(struct cluster_ops *ops,
					   TALLOC_CTX *mem_ctx, 
					   struct loadparm_context *lp_ctx,
					   const char *dbname, int flags)
{
	char *path = smbd_tmp_path(mem_ctx, lp_ctx, dbname);
	struct tdb_wrap *w;
	w = tdb_wrap_open(mem_ctx, path, 0, flags,
			  O_RDWR|O_CREAT, 0600);
	talloc_free(path);
	return w;
}

/*
  dummy backend handle function
*/
static void *local_backend_handle(struct cluster_ops *ops)
{
	return NULL;
}

/*
  dummy message init function - not needed as all messages are local
*/
static NTSTATUS local_message_init(struct cluster_ops *ops,
				   struct messaging_context *msg, 
				   struct server_id server,
				   cluster_message_fn_t handler)
{
	return NT_STATUS_OK;
}

/*
  dummy message send
*/
static NTSTATUS local_message_send(struct cluster_ops *ops,
				   struct server_id server, DATA_BLOB *data)
{
	return NT_STATUS_INVALID_DEVICE_REQUEST;
}

static struct cluster_ops cluster_local_ops = {
	.cluster_id           = local_id,
	.cluster_id_string    = local_id_string,
	.cluster_tdb_tmp_open = local_tdb_tmp_open,
	.backend_handle       = local_backend_handle,
	.message_init         = local_message_init,
	.message_send         = local_message_send,
	.private_data         = NULL
};

void cluster_local_init(void)
{
	cluster_set_ops(&cluster_local_ops);
}

