/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan (metze) Metzmacher 2005
   Copyright (C) Guenther Deschner 2008
   Copyright (C) Michael Adam 2008

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
#include "libnet/libnet_dssync.h"
#include "rpc_client/cli_pipe.h"
#include "../libcli/drsuapi/drsuapi.h"
#include "../librpc/gen_ndr/ndr_drsuapi_c.h"

/****************************************************************
****************************************************************/

static int libnet_dssync_free_context(struct dssync_context *ctx)
{
	WERROR result;
	struct dcerpc_binding_handle *b;

	if (!ctx) {
		return 0;
	}

	if (is_valid_policy_hnd(&ctx->bind_handle) && ctx->cli) {
		b = ctx->cli->binding_handle;
		dcerpc_drsuapi_DsUnbind(b, ctx, &ctx->bind_handle, &result);
	}

	return 0;
}

/****************************************************************
****************************************************************/

NTSTATUS libnet_dssync_init_context(TALLOC_CTX *mem_ctx,
				    struct dssync_context **ctx_p)
{
	struct dssync_context *ctx;

	ctx = TALLOC_ZERO_P(mem_ctx, struct dssync_context);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	talloc_set_destructor(ctx, libnet_dssync_free_context);
	ctx->clean_old_entries = false;

	*ctx_p = ctx;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static void parse_obj_identifier(struct drsuapi_DsReplicaObjectIdentifier *id,
				 uint32_t *rid)
{
	if (!id || !rid) {
		return;
	}

	*rid = 0;

	if (id->sid.num_auths > 0) {
		*rid = id->sid.sub_auths[id->sid.num_auths - 1];
	}
}

/****************************************************************
****************************************************************/

static void libnet_dssync_decrypt_attributes(TALLOC_CTX *mem_ctx,
					     DATA_BLOB *session_key,
					     struct drsuapi_DsReplicaObjectListItemEx *cur)
{
	for (; cur; cur = cur->next_object) {

		uint32_t i;
		uint32_t rid = 0;

		parse_obj_identifier(cur->object.identifier, &rid);

		for (i=0; i < cur->object.attribute_ctr.num_attributes; i++) {

			struct drsuapi_DsReplicaAttribute *attr;

			attr = &cur->object.attribute_ctr.attributes[i];

			if (attr->value_ctr.num_values < 1) {
				continue;
			}

			if (!attr->value_ctr.values[0].blob) {
				continue;
			}

			drsuapi_decrypt_attribute(mem_ctx,
						  session_key,
						  rid,
						  attr);
		}
	}
}
/****************************************************************
****************************************************************/

static NTSTATUS libnet_dssync_bind(TALLOC_CTX *mem_ctx,
				   struct dssync_context *ctx)
{
	NTSTATUS status;
	WERROR werr;

	struct GUID bind_guid;
	struct drsuapi_DsBindInfoCtr bind_info;
	struct drsuapi_DsBindInfo28 info28;
	struct dcerpc_binding_handle *b = ctx->cli->binding_handle;

	ZERO_STRUCT(info28);

	GUID_from_string(DRSUAPI_DS_BIND_GUID, &bind_guid);

	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_BASE;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ASYNC_REPLICATION;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_REMOVEAPI;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_MOVEREQ_V2;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHG_COMPRESS;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V1;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_RESTORE_USN_OPTIMIZATION;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_KCC_EXECUTE;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRY_V2;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_LINKED_VALUE_REPLICATION;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V2;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_INSTANCE_TYPE_NOT_REQ_ON_MOD;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_CRYPTO_BIND;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GET_REPL_INFO;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_STRONG_ENCRYPTION;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V01;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_TRANSITIVE_MEMBERSHIP;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADD_SID_HISTORY;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_POST_BETA3;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GET_MEMBERSHIPS2;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V6;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_NONDOMAIN_NCS;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V8;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V5;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V6;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRYREPLY_V3;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V7;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_VERIFY_OBJECT;
	info28.site_guid		= GUID_zero();
	info28.pid			= 508;
	info28.repl_epoch		= 0;

	bind_info.length = 28;
	bind_info.info.info28 = info28;

	status = dcerpc_drsuapi_DsBind(b, mem_ctx,
				       &bind_guid,
				       &bind_info,
				       &ctx->bind_handle,
				       &werr);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!W_ERROR_IS_OK(werr)) {
		return werror_to_ntstatus(werr);
	}

	ZERO_STRUCT(ctx->remote_info28);
	switch (bind_info.length) {
	case 24: {
		struct drsuapi_DsBindInfo24 *info24;
		info24 = &bind_info.info.info24;
		ctx->remote_info28.site_guid		= info24->site_guid;
		ctx->remote_info28.supported_extensions	= info24->supported_extensions;
		ctx->remote_info28.pid			= info24->pid;
		ctx->remote_info28.repl_epoch		= 0;
		break;
	}
	case 28:
		ctx->remote_info28 = bind_info.info.info28;
		break;
	case 48: {
		struct drsuapi_DsBindInfo48 *info48;
		info48 = &bind_info.info.info48;
		ctx->remote_info28.site_guid		= info48->site_guid;
		ctx->remote_info28.supported_extensions	= info48->supported_extensions;
		ctx->remote_info28.pid			= info48->pid;
		ctx->remote_info28.repl_epoch		= info48->repl_epoch;
		break;
	}
	default:
		DEBUG(1, ("Warning: invalid info length in bind info: %d\n",
			  bind_info.length));
		break;
	}

	return status;
}

/****************************************************************
****************************************************************/

static NTSTATUS libnet_dssync_lookup_nc(TALLOC_CTX *mem_ctx,
					struct dssync_context *ctx)
{
	NTSTATUS status;
	WERROR werr;
	uint32_t level = 1;
	union drsuapi_DsNameRequest req;
	uint32_t level_out;
	struct drsuapi_DsNameString names[1];
	union drsuapi_DsNameCtr ctr;
	struct dcerpc_binding_handle *b = ctx->cli->binding_handle;

	names[0].str = talloc_asprintf(mem_ctx, "%s\\", ctx->domain_name);
	NT_STATUS_HAVE_NO_MEMORY(names[0].str);

	req.req1.codepage	= 1252; /* german */
	req.req1.language	= 0x00000407; /* german */
	req.req1.count		= 1;
	req.req1.names		= names;
	req.req1.format_flags	= DRSUAPI_DS_NAME_FLAG_NO_FLAGS;
	req.req1.format_offered	= DRSUAPI_DS_NAME_FORMAT_UNKNOWN;
	req.req1.format_desired	= DRSUAPI_DS_NAME_FORMAT_FQDN_1779;

	status = dcerpc_drsuapi_DsCrackNames(b, mem_ctx,
					     &ctx->bind_handle,
					     level,
					     &req,
					     &level_out,
					     &ctr,
					     &werr);
	if (!NT_STATUS_IS_OK(status)) {
		ctx->error_message = talloc_asprintf(ctx,
			"Failed to lookup DN for domain name: %s",
			get_friendly_nt_error_msg(status));
		return status;
	}

	if (!W_ERROR_IS_OK(werr)) {
		ctx->error_message = talloc_asprintf(ctx,
			"Failed to lookup DN for domain name: %s",
			get_friendly_werror_msg(werr));
		return werror_to_ntstatus(werr);
	}

	if (ctr.ctr1->count != 1) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (ctr.ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_OK) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	ctx->nc_dn = talloc_strdup(mem_ctx, ctr.ctr1->array[0].result_name);
	NT_STATUS_HAVE_NO_MEMORY(ctx->nc_dn);

	if (!ctx->dns_domain_name) {
		ctx->dns_domain_name = talloc_strdup_upper(mem_ctx,
			ctr.ctr1->array[0].dns_domain_name);
		NT_STATUS_HAVE_NO_MEMORY(ctx->dns_domain_name);
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS libnet_dssync_init(TALLOC_CTX *mem_ctx,
				   struct dssync_context *ctx)
{
	NTSTATUS status;

	status = libnet_dssync_bind(mem_ctx, ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!ctx->nc_dn) {
		status = libnet_dssync_lookup_nc(mem_ctx, ctx);
	}

	return status;
}

/****************************************************************
****************************************************************/

static NTSTATUS libnet_dssync_build_request(TALLOC_CTX *mem_ctx,
					    struct dssync_context *ctx,
					    const char *dn,
					    struct replUpToDateVectorBlob *utdv,
					    uint32_t *plevel,
					    union drsuapi_DsGetNCChangesRequest *preq)
{
	NTSTATUS status;
	uint32_t count;
	uint32_t level;
	union drsuapi_DsGetNCChangesRequest req;
	struct dom_sid null_sid;
	enum drsuapi_DsExtendedOperation extended_op;
	struct drsuapi_DsReplicaObjectIdentifier *nc = NULL;
	struct drsuapi_DsReplicaCursorCtrEx *cursors = NULL;

	uint32_t replica_flags	= DRSUAPI_DRS_WRIT_REP |
				  DRSUAPI_DRS_INIT_SYNC |
				  DRSUAPI_DRS_PER_SYNC |
				  DRSUAPI_DRS_GET_ANC |
				  DRSUAPI_DRS_NEVER_SYNCED;

	ZERO_STRUCT(null_sid);
	ZERO_STRUCT(req);

	if (ctx->remote_info28.supported_extensions
	    & DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V8)
	{
		level = 8;
	} else {
		level = 5;
	}

	nc = TALLOC_ZERO_P(mem_ctx, struct drsuapi_DsReplicaObjectIdentifier);
	if (!nc) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}
	nc->dn = dn;
	nc->guid = GUID_zero();
	nc->sid = null_sid;

	if (!ctx->single_object_replication &&
	    !ctx->force_full_replication && utdv)
	{
		cursors = TALLOC_ZERO_P(mem_ctx,
					 struct drsuapi_DsReplicaCursorCtrEx);
		if (!cursors) {
			status = NT_STATUS_NO_MEMORY;
			goto fail;
		}

		switch (utdv->version) {
		case 1:
			cursors->count = utdv->ctr.ctr1.count;
			cursors->cursors = utdv->ctr.ctr1.cursors;
			break;
		case 2:
			cursors->count = utdv->ctr.ctr2.count;
			cursors->cursors = talloc_array(cursors,
						struct drsuapi_DsReplicaCursor,
						cursors->count);
			if (!cursors->cursors) {
				status = NT_STATUS_NO_MEMORY;
				goto fail;
			}
			for (count = 0; count < cursors->count; count++) {
				cursors->cursors[count].source_dsa_invocation_id =
					utdv->ctr.ctr2.cursors[count].source_dsa_invocation_id;
				cursors->cursors[count].highest_usn =
					utdv->ctr.ctr2.cursors[count].highest_usn;
			}
			break;
		}
	}

	if (ctx->single_object_replication) {
		extended_op = DRSUAPI_EXOP_REPL_OBJ;
	} else {
		extended_op = DRSUAPI_EXOP_NONE;
	}

	if (level == 8) {
		req.req8.naming_context		= nc;
		req.req8.replica_flags		= replica_flags;
		req.req8.max_object_count	= 402;
		req.req8.max_ndr_size		= 402116;
		req.req8.uptodateness_vector	= cursors;
		req.req8.extended_op		= extended_op;
	} else if (level == 5) {
		req.req5.naming_context		= nc;
		req.req5.replica_flags		= replica_flags;
		req.req5.max_object_count	= 402;
		req.req5.max_ndr_size		= 402116;
		req.req5.uptodateness_vector	= cursors;
		req.req5.extended_op		= extended_op;
	} else {
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	if (plevel) {
		*plevel = level;
	}

	if (preq) {
		*preq = req;
	}

	return NT_STATUS_OK;

fail:
	TALLOC_FREE(nc);
	TALLOC_FREE(cursors);
	return status;
}

static NTSTATUS libnet_dssync_getncchanges(TALLOC_CTX *mem_ctx,
					   struct dssync_context *ctx,
					   uint32_t level,
					   union drsuapi_DsGetNCChangesRequest *req,
					   struct replUpToDateVectorBlob **pnew_utdv)
{
	NTSTATUS status;
	WERROR werr;
	union drsuapi_DsGetNCChangesCtr ctr;
	struct drsuapi_DsGetNCChangesCtr1 *ctr1 = NULL;
	struct drsuapi_DsGetNCChangesCtr6 *ctr6 = NULL;
	struct replUpToDateVectorBlob *new_utdv = NULL;
	uint32_t level_out = 0;
	uint32_t out_level = 0;
	int y;
	bool last_query;
	struct dcerpc_binding_handle *b = ctx->cli->binding_handle;

	if (!ctx->single_object_replication) {
		new_utdv = TALLOC_ZERO_P(mem_ctx, struct replUpToDateVectorBlob);
		if (!new_utdv) {
			status = NT_STATUS_NO_MEMORY;
			goto out;
		}
	}

	for (y=0, last_query = false; !last_query; y++) {
		struct drsuapi_DsReplicaObjectListItemEx *first_object = NULL;
		struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr = NULL;
		uint32_t linked_attributes_count = 0;
		struct drsuapi_DsReplicaLinkedAttribute *linked_attributes = NULL;

		if (level == 8) {
			DEBUG(1,("start[%d] tmp_higest_usn: %llu , highest_usn: %llu\n",y,
				(long long)req->req8.highwatermark.tmp_highest_usn,
				(long long)req->req8.highwatermark.highest_usn));
		} else if (level == 5) {
			DEBUG(1,("start[%d] tmp_higest_usn: %llu , highest_usn: %llu\n",y,
				(long long)req->req5.highwatermark.tmp_highest_usn,
				(long long)req->req5.highwatermark.highest_usn));
		}

		status = dcerpc_drsuapi_DsGetNCChanges(b, mem_ctx,
						       &ctx->bind_handle,
						       level,
						       req,
						       &level_out,
						       &ctr,
						       &werr);
		if (!NT_STATUS_IS_OK(status)) {
			ctx->error_message = talloc_asprintf(ctx,
				"Failed to get NC Changes: %s",
				get_friendly_nt_error_msg(status));
			goto out;
		}

		if (!W_ERROR_IS_OK(werr)) {
			status = werror_to_ntstatus(werr);
			ctx->error_message = talloc_asprintf(ctx,
				"Failed to get NC Changes: %s",
				get_friendly_werror_msg(werr));
			goto out;
		}

		if (level_out == 1) {
			out_level = 1;
			ctr1 = &ctr.ctr1;
		} else if (level_out == 2 && ctr.ctr2.mszip1.ts) {
			out_level = 1;
			ctr1 = &ctr.ctr2.mszip1.ts->ctr1;
		} else if (level_out == 6) {
			out_level = 6;
			ctr6 = &ctr.ctr6;
		} else if (level_out == 7
			   && ctr.ctr7.level == 6
			   && ctr.ctr7.type == DRSUAPI_COMPRESSION_TYPE_MSZIP
			   && ctr.ctr7.ctr.mszip6.ts) {
			out_level = 6;
			ctr6 = &ctr.ctr7.ctr.mszip6.ts->ctr6;
		} else if (level_out == 7
			   && ctr.ctr7.level == 6
			   && ctr.ctr7.type == DRSUAPI_COMPRESSION_TYPE_XPRESS
			   && ctr.ctr7.ctr.xpress6.ts) {
			out_level = 6;
			ctr6 = &ctr.ctr7.ctr.xpress6.ts->ctr6;
		}

		if (out_level == 1) {
			DEBUG(1,("end[%d] tmp_highest_usn: %llu , highest_usn: %llu\n",y,
				(long long)ctr1->new_highwatermark.tmp_highest_usn,
				(long long)ctr1->new_highwatermark.highest_usn));

			first_object = ctr1->first_object;
			mapping_ctr = &ctr1->mapping_ctr;

			if (ctr1->more_data) {
				req->req5.highwatermark = ctr1->new_highwatermark;
			} else {
				last_query = true;
				if (ctr1->uptodateness_vector &&
				    !ctx->single_object_replication)
				{
					new_utdv->version = 1;
					new_utdv->ctr.ctr1.count =
						ctr1->uptodateness_vector->count;
					new_utdv->ctr.ctr1.cursors =
						ctr1->uptodateness_vector->cursors;
				}
			}
		} else if (out_level == 6) {
			DEBUG(1,("end[%d] tmp_highest_usn: %llu , highest_usn: %llu\n",y,
				(long long)ctr6->new_highwatermark.tmp_highest_usn,
				(long long)ctr6->new_highwatermark.highest_usn));

			first_object = ctr6->first_object;
			mapping_ctr = &ctr6->mapping_ctr;

			linked_attributes = ctr6->linked_attributes;
			linked_attributes_count = ctr6->linked_attributes_count;

			if (ctr6->more_data) {
				req->req8.highwatermark = ctr6->new_highwatermark;
			} else {
				last_query = true;
				if (ctr6->uptodateness_vector &&
				    !ctx->single_object_replication)
				{
					new_utdv->version = 2;
					new_utdv->ctr.ctr2.count =
						ctr6->uptodateness_vector->count;
					new_utdv->ctr.ctr2.cursors =
						ctr6->uptodateness_vector->cursors;
				}
			}
		}

		status = cli_get_session_key(mem_ctx, ctx->cli, &ctx->session_key);
		if (!NT_STATUS_IS_OK(status)) {
			ctx->error_message = talloc_asprintf(ctx,
				"Failed to get Session Key: %s",
				nt_errstr(status));
			goto out;
		}

		libnet_dssync_decrypt_attributes(mem_ctx,
						 &ctx->session_key,
						 first_object);

		if (ctx->ops->process_objects) {
			status = ctx->ops->process_objects(ctx, mem_ctx,
							   first_object,
							   mapping_ctr);
			if (!NT_STATUS_IS_OK(status)) {
				ctx->error_message = talloc_asprintf(ctx,
					"Failed to call processing function: %s",
					nt_errstr(status));
				goto out;
			}
		}

		if (linked_attributes_count == 0) {
			continue;
		}

		if (ctx->ops->process_links) {
			status = ctx->ops->process_links(ctx, mem_ctx,
							 linked_attributes_count,
							 linked_attributes,
							 mapping_ctr);
			if (!NT_STATUS_IS_OK(status)) {
				ctx->error_message = talloc_asprintf(ctx,
					"Failed to call processing function: %s",
					nt_errstr(status));
				goto out;
			}
		}
	}

	*pnew_utdv = new_utdv;

out:
	return status;
}

static NTSTATUS libnet_dssync_process(TALLOC_CTX *mem_ctx,
				      struct dssync_context *ctx)
{
	NTSTATUS status;

	uint32_t level = 0;
	union drsuapi_DsGetNCChangesRequest req;
	struct replUpToDateVectorBlob *old_utdv = NULL;
	struct replUpToDateVectorBlob *pnew_utdv = NULL;
	const char **dns;
	uint32_t dn_count;
	uint32_t count;

	if (ctx->ops->startup) {
		status = ctx->ops->startup(ctx, mem_ctx, &old_utdv);
		if (!NT_STATUS_IS_OK(status)) {
			ctx->error_message = talloc_asprintf(ctx,
				"Failed to call startup operation: %s",
				nt_errstr(status));
			goto out;
		}
	}

	if (ctx->single_object_replication && ctx->object_dns) {
		dns = ctx->object_dns;
		dn_count = ctx->object_count;
	} else {
		dns = &ctx->nc_dn;
		dn_count = 1;
	}

	status = NT_STATUS_OK;

	for (count=0; count < dn_count; count++) {
		status = libnet_dssync_build_request(mem_ctx, ctx,
						     dns[count],
						     old_utdv, &level,
						     &req);
		if (!NT_STATUS_IS_OK(status)) {
			goto out;
		}

		status = libnet_dssync_getncchanges(mem_ctx, ctx, level, &req,
						    &pnew_utdv);
		if (!NT_STATUS_IS_OK(status)) {
			ctx->error_message = talloc_asprintf(ctx,
				"Failed to call DsGetNCCHanges: %s",
				nt_errstr(status));
			goto out;
		}
	}

	if (ctx->ops->finish) {
		status = ctx->ops->finish(ctx, mem_ctx, pnew_utdv);
		if (!NT_STATUS_IS_OK(status)) {
			ctx->error_message = talloc_asprintf(ctx,
				"Failed to call finishing operation: %s",
				nt_errstr(status));
			goto out;
		}
	}

 out:
	return status;
}

/****************************************************************
****************************************************************/

NTSTATUS libnet_dssync(TALLOC_CTX *mem_ctx,
		       struct dssync_context *ctx)
{
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	status = libnet_dssync_init(tmp_ctx, ctx);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	status = libnet_dssync_process(tmp_ctx, ctx);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

 out:
	TALLOC_FREE(tmp_ctx);
	return status;
}

