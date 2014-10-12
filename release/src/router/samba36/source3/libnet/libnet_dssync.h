/*
 *  Unix SMB/CIFS implementation.
 *  libnet Support
 *  Copyright (C) Guenther Deschner 2008
 *  Copyright (C) Michael Adam 2008
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

#include "../librpc/gen_ndr/drsuapi.h"
#include "../librpc/gen_ndr/drsblobs.h"

struct dssync_context;

struct dssync_ops {
	NTSTATUS (*startup)(struct dssync_context *ctx, TALLOC_CTX *mem_ctx,
			    struct replUpToDateVectorBlob **pold_utdv);
	NTSTATUS (*process_objects)(struct dssync_context *ctx,
				    TALLOC_CTX *mem_ctx,
				    struct drsuapi_DsReplicaObjectListItemEx *objects,
				    struct drsuapi_DsReplicaOIDMapping_Ctr *mappings);
	NTSTATUS (*process_links)(struct dssync_context *ctx,
				  TALLOC_CTX *mem_ctx,
				  uint32_t count,
				  struct drsuapi_DsReplicaLinkedAttribute *links,
				  struct drsuapi_DsReplicaOIDMapping_Ctr *mappings);
	NTSTATUS (*finish)(struct dssync_context *ctx, TALLOC_CTX *mem_ctx,
			   struct replUpToDateVectorBlob *new_utdv);
};

struct dssync_context {
	const char *domain_name;
	const char *dns_domain_name;
	struct rpc_pipe_client *cli;
	const char *nc_dn;
	bool single_object_replication;
	bool force_full_replication;
	bool clean_old_entries;
	uint32_t object_count;
	const char **object_dns;
	struct policy_handle bind_handle;
	DATA_BLOB session_key;
	const char *output_filename;
	struct drsuapi_DsBindInfo28 remote_info28;

	void *private_data;

	const struct dssync_ops *ops;

	char *result_message;
	char *error_message;
};

extern const struct dssync_ops libnet_dssync_keytab_ops;
extern const struct dssync_ops libnet_dssync_passdb_ops;

/* The following definitions come from libnet/libnet_dssync.c  */

NTSTATUS libnet_dssync_init_context(TALLOC_CTX *mem_ctx,
				    struct dssync_context **ctx_p);
NTSTATUS libnet_dssync(TALLOC_CTX *mem_ctx,
		       struct dssync_context *ctx);
