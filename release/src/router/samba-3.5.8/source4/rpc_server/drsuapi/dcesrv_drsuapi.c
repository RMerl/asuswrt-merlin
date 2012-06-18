/* 
   Unix SMB/CIFS implementation.

   endpoint server for the drsuapi pipe

   Copyright (C) Stefan Metzmacher 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006
   
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
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/common/common.h"
#include "dsdb/samdb/samdb.h"
#include "rpc_server/drsuapi/dcesrv_drsuapi.h"
#include "libcli/security/security.h"

/* 
  drsuapi_DsBind 
*/
static WERROR dcesrv_drsuapi_DsBind(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct drsuapi_DsBind *r)
{
	struct drsuapi_bind_state *b_state;
	struct dcesrv_handle *handle;
	struct drsuapi_DsBindInfoCtr *bind_info;
	struct GUID site_guid;
	struct ldb_result *site_res;
	struct ldb_dn *server_site_dn;
	static const char *site_attrs[] = { "objectGUID", NULL };
	struct ldb_result *ntds_res;
	struct ldb_dn *ntds_dn;
	static const char *ntds_attrs[] = { "ms-DS-ReplicationEpoch", NULL };
	uint32_t pid;
	uint32_t repl_epoch;
	int ret;

	r->out.bind_info = NULL;
	ZERO_STRUCTP(r->out.bind_handle);

	b_state = talloc_zero(mem_ctx, struct drsuapi_bind_state);
	W_ERROR_HAVE_NO_MEMORY(b_state);

	/*
	 * connect to the samdb
	 */
	b_state->sam_ctx = samdb_connect(b_state, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, dce_call->conn->auth_state.session_info); 
	if (!b_state->sam_ctx) {
		return WERR_FOOBAR;
	}

	/*
	 * find out the guid of our own site
	 */
	server_site_dn = samdb_server_site_dn(b_state->sam_ctx, mem_ctx);
	W_ERROR_HAVE_NO_MEMORY(server_site_dn);

	ret = ldb_search(b_state->sam_ctx, mem_ctx, &site_res,
				 server_site_dn, LDB_SCOPE_BASE, site_attrs,
				 "(objectClass=*)");
	if (ret != LDB_SUCCESS) {
		return WERR_DS_DRA_INTERNAL_ERROR;
	}
	if (site_res->count != 1) {
		return WERR_DS_DRA_INTERNAL_ERROR;
	}
	site_guid = samdb_result_guid(site_res->msgs[0], "objectGUID");

	/*
	 * lookup the local servers Replication Epoch
	 */
	ntds_dn = samdb_ntds_settings_dn(b_state->sam_ctx);
	W_ERROR_HAVE_NO_MEMORY(ntds_dn);

	ret = ldb_search(b_state->sam_ctx, mem_ctx, &ntds_res,
				 ntds_dn, LDB_SCOPE_BASE, ntds_attrs,
				 "(objectClass=*)");
	if (ret != LDB_SUCCESS) {
		return WERR_DS_DRA_INTERNAL_ERROR;
	}
	if (ntds_res->count != 1) {
		return WERR_DS_DRA_INTERNAL_ERROR;
	}
	repl_epoch = samdb_result_uint(ntds_res->msgs[0], "ms-DS-ReplicationEpoch", 0);

	/*
	 * The "process identifier" of the client.
	 * According to the WSPP docs, sectin 5.35, this is
	 * for informational and debugging purposes only.
	 * The assignment is implementation specific.
	 */
	pid = 0;

	/*
	 * store the clients bind_guid
	 */
	if (r->in.bind_guid) {
		b_state->remote_bind_guid = *r->in.bind_guid;
	}

	/*
	 * store the clients bind_info
	 */
	if (r->in.bind_info) {
		switch (r->in.bind_info->length) {
		case 24: {
			struct drsuapi_DsBindInfo24 *info24;
			info24 = &r->in.bind_info->info.info24;
			b_state->remote_info28.supported_extensions	= info24->supported_extensions;
			b_state->remote_info28.site_guid		= info24->site_guid;
			b_state->remote_info28.pid			= info24->pid;
			b_state->remote_info28.repl_epoch		= 0;
			break;
		}
		case 28:
			b_state->remote_info28 = r->in.bind_info->info.info28;
			break;
		}
	}

	/*
	 * fill in our local bind info 28
	 */
	b_state->local_info28.supported_extensions	= 0;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_BASE;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ASYNC_REPLICATION;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_REMOVEAPI;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_MOVEREQ_V2;
#if 0 /* we don't support MSZIP compression (only decompression) */
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHG_COMPRESS;
#endif
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V1;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_RESTORE_USN_OPTIMIZATION;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_KCC_EXECUTE;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRY_V2;
	if (0 /*domain.behavior_version == 2*/) {
		/* TODO: find out how this is really triggered! */
		b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_LINKED_VALUE_REPLICATION;
	}
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V2;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_INSTANCE_TYPE_NOT_REQ_ON_MOD;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_CRYPTO_BIND;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GET_REPL_INFO;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_STRONG_ENCRYPTION;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V01;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_TRANSITIVE_MEMBERSHIP;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADD_SID_HISTORY;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_POST_BETA3;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_00100000;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GET_MEMBERSHIPS2;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V6;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_NONDOMAIN_NCS;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V8;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V5;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V6;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRYREPLY_V3;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V7;
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_VERIFY_OBJECT;
#if 0 /* we don't support XPRESS compression yet */
	b_state->local_info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_XPRESS_COMPRESS;
#endif
	b_state->local_info28.site_guid			= site_guid;
	b_state->local_info28.pid			= pid;
	b_state->local_info28.repl_epoch		= repl_epoch;

	/*
	 * allocate the return bind_info
	 */
	bind_info = talloc(mem_ctx, struct drsuapi_DsBindInfoCtr);
	W_ERROR_HAVE_NO_MEMORY(bind_info);

	bind_info->length	= 28;
	bind_info->info.info28	= b_state->local_info28;

	/*
	 * allocate a bind handle
	 */
	handle = dcesrv_handle_new(dce_call->context, DRSUAPI_BIND_HANDLE);
	W_ERROR_HAVE_NO_MEMORY(handle);
	handle->data = talloc_steal(handle, b_state);

	/*
	 * prepare reply
	 */
	r->out.bind_info = bind_info;
	*r->out.bind_handle = handle->wire_handle;

	return WERR_OK;
}


/* 
  drsuapi_DsUnbind 
*/
static WERROR dcesrv_drsuapi_DsUnbind(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			       struct drsuapi_DsUnbind *r)
{
	struct dcesrv_handle *h;

	*r->out.bind_handle = *r->in.bind_handle;

	DCESRV_PULL_HANDLE_WERR(h, r->in.bind_handle, DRSUAPI_BIND_HANDLE);

	talloc_free(h);

	ZERO_STRUCTP(r->out.bind_handle);

	return WERR_OK;
}


/* 
  drsuapi_DsReplicaSync 
*/
static WERROR dcesrv_drsuapi_DsReplicaSync(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					   struct drsuapi_DsReplicaSync *r)
{
	WERROR status;

	status = drs_security_level_check(dce_call, "DsReplicaSync");
	if (!W_ERROR_IS_OK(status)) {
		return status;
	}

	dcesrv_irpc_forward_rpc_call(dce_call, mem_ctx, r, NDR_DRSUAPI_DSREPLICASYNC,
				     &ndr_table_drsuapi,
				     "dreplsrv", "DsReplicaSync");

	return WERR_OK;
}


/* 
  drsuapi_DsReplicaAdd 
*/
static WERROR dcesrv_drsuapi_DsReplicaAdd(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					  struct drsuapi_DsReplicaAdd *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  drsuapi_DsReplicaDel 
*/
static WERROR dcesrv_drsuapi_DsReplicaDel(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					  struct drsuapi_DsReplicaDel *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  drsuapi_DsReplicaModify 
*/
static WERROR dcesrv_drsuapi_DsReplicaMod(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					  struct drsuapi_DsReplicaMod *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  DRSUAPI_VERIFY_NAMES 
*/
static WERROR dcesrv_DRSUAPI_VERIFY_NAMES(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct DRSUAPI_VERIFY_NAMES *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  drsuapi_DsGetMemberships 
*/
static WERROR dcesrv_drsuapi_DsGetMemberships(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct drsuapi_DsGetMemberships *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  DRSUAPI_INTER_DOMAIN_MOVE 
*/
static WERROR dcesrv_DRSUAPI_INTER_DOMAIN_MOVE(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct DRSUAPI_INTER_DOMAIN_MOVE *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  drsuapi_DsGetNT4ChangeLog 
*/
static WERROR dcesrv_drsuapi_DsGetNT4ChangeLog(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct drsuapi_DsGetNT4ChangeLog *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  drsuapi_DsCrackNames 
*/
static WERROR dcesrv_drsuapi_DsCrackNames(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			    struct drsuapi_DsCrackNames *r)
{
	WERROR status;
	struct drsuapi_bind_state *b_state;
	struct dcesrv_handle *h;

	*r->out.level_out = r->in.level;

	DCESRV_PULL_HANDLE_WERR(h, r->in.bind_handle, DRSUAPI_BIND_HANDLE);
	b_state = h->data;

	r->out.ctr = talloc_zero(mem_ctx, union drsuapi_DsNameCtr);
	W_ERROR_HAVE_NO_MEMORY(r->out.ctr);

	switch (r->in.level) {
		case 1: {
			struct drsuapi_DsNameCtr1 *ctr1;
			struct drsuapi_DsNameInfo1 *names;
			int count;
			int i;

			ctr1 = talloc(mem_ctx, struct drsuapi_DsNameCtr1);
			W_ERROR_HAVE_NO_MEMORY(ctr1);

			count = r->in.req->req1.count;
			names = talloc_array(mem_ctx, struct drsuapi_DsNameInfo1, count);
			W_ERROR_HAVE_NO_MEMORY(names);

			for (i=0; i < count; i++) {
				status = DsCrackNameOneName(b_state->sam_ctx, mem_ctx,
							    r->in.req->req1.format_flags,
							    r->in.req->req1.format_offered,
							    r->in.req->req1.format_desired,
							    r->in.req->req1.names[i].str,
							    &names[i]);
				if (!W_ERROR_IS_OK(status)) {
					return status;
				}
			}

			ctr1->count = count;
			ctr1->array = names;
			r->out.ctr->ctr1 = ctr1;

			return WERR_OK;
		}
	}
	
	return WERR_UNKNOWN_LEVEL;
}

/* 
  drsuapi_DsWriteAccountSpn 
*/
static WERROR dcesrv_drsuapi_DsWriteAccountSpn(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct drsuapi_DsWriteAccountSpn *r)
{
	struct drsuapi_bind_state *b_state;
	struct dcesrv_handle *h;

	*r->out.level_out = r->in.level;

	DCESRV_PULL_HANDLE_WERR(h, r->in.bind_handle, DRSUAPI_BIND_HANDLE);
	b_state = h->data;

	r->out.res = talloc(mem_ctx, union drsuapi_DsWriteAccountSpnResult);
	W_ERROR_HAVE_NO_MEMORY(r->out.res);

	switch (r->in.level) {
		case 1: {
			struct drsuapi_DsWriteAccountSpnRequest1 *req;
			struct ldb_message *msg;
			int count, i, ret;
			req = &r->in.req->req1;
			count = req->count;

			msg = ldb_msg_new(mem_ctx);
			if (msg == NULL) {
				return WERR_NOMEM;
			}

			msg->dn = ldb_dn_new(msg, b_state->sam_ctx, req->object_dn);
			if ( ! ldb_dn_validate(msg->dn)) {
				r->out.res->res1.status = WERR_OK;
				return WERR_OK;
			}
			
			/* construct mods */
			for (i = 0; i < count; i++) {
				samdb_msg_add_string(b_state->sam_ctx, 
						     msg, msg, "servicePrincipalName",
						     req->spn_names[i].str);
			}
			for (i=0;i<msg->num_elements;i++) {
				switch (req->operation) {
				case DRSUAPI_DS_SPN_OPERATION_ADD:
 					msg->elements[i].flags = LDB_FLAG_MOD_ADD;
					break;
				case DRSUAPI_DS_SPN_OPERATION_REPLACE:
					msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
					break;
				case DRSUAPI_DS_SPN_OPERATION_DELETE:
					msg->elements[i].flags = LDB_FLAG_MOD_DELETE;
					break;
				}
			}
   
			/* Apply to database */

			ret = ldb_modify(b_state->sam_ctx, msg);
			if (ret != 0) {
				DEBUG(0,("Failed to modify SPNs on %s: %s\n",
					 ldb_dn_get_linearized(msg->dn), 
					 ldb_errstring(b_state->sam_ctx)));
				r->out.res->res1.status = WERR_ACCESS_DENIED;
			} else {
				r->out.res->res1.status = WERR_OK;
			}

			return WERR_OK;
		}
	}
	
	return WERR_UNKNOWN_LEVEL;
}


/* 
  drsuapi_DsRemoveDSServer
*/
static WERROR dcesrv_drsuapi_DsRemoveDSServer(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				       struct drsuapi_DsRemoveDSServer *r)
{
	struct drsuapi_bind_state *b_state;
	struct dcesrv_handle *h;
	struct ldb_dn *ntds_dn;
	int ret;
	bool ok;
	WERROR status;

	ZERO_STRUCT(r->out.res);
	*r->out.level_out = 1;

	status = drs_security_level_check(dce_call, "DsRemoveDSServer");
	if (!W_ERROR_IS_OK(status)) {
		return status;
	}

	DCESRV_PULL_HANDLE_WERR(h, r->in.bind_handle, DRSUAPI_BIND_HANDLE);
	b_state = h->data;

	switch (r->in.level) {
	case 1:
		ntds_dn = ldb_dn_new(mem_ctx, b_state->sam_ctx, r->in.req->req1.server_dn);
		W_ERROR_HAVE_NO_MEMORY(ntds_dn);

		ok = ldb_dn_validate(ntds_dn);
		if (!ok) {
			return WERR_FOOBAR;
		}

		/* TODO: it's likely that we need more checks here */

		ok = ldb_dn_add_child_fmt(ntds_dn, "CN=NTDS Settings");
		if (!ok) {
			return WERR_FOOBAR;
		}

		if (r->in.req->req1.commit) {
			ret = ldb_delete(b_state->sam_ctx, ntds_dn);
			if (ret != LDB_SUCCESS) {
				return WERR_FOOBAR;
			}
		}

		return WERR_OK;
	default:
		break;
	}

	return WERR_FOOBAR;
}


/* 
  DRSUAPI_REMOVE_DS_DOMAIN 
*/
static WERROR dcesrv_DRSUAPI_REMOVE_DS_DOMAIN(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct DRSUAPI_REMOVE_DS_DOMAIN *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* Obtain the site name from a server DN */
static const char *result_site_name(struct ldb_dn *site_dn)
{
	/* Format is cn=<NETBIOS name>,cn=Servers,cn=<site>,cn=sites.... */
	const struct ldb_val *val = ldb_dn_get_component_val(site_dn, 2);
	const char *name = ldb_dn_get_component_name(site_dn, 2);

	if (!name || (ldb_attr_cmp(name, "cn") != 0)) {
		/* Ensure this matches the format.  This gives us a
		 * bit more confidence that a 'cn' value will be a
		 * ascii string */
		return NULL;
	}
	if (val) {
		return (char *)val->data;
	}
	return NULL;
}

/* 
  drsuapi_DsGetDomainControllerInfo 
*/
static WERROR dcesrv_drsuapi_DsGetDomainControllerInfo_1(struct drsuapi_bind_state *b_state, 
						TALLOC_CTX *mem_ctx,
						struct drsuapi_DsGetDomainControllerInfo *r)
{
	struct ldb_dn *sites_dn;
	struct ldb_result *res;

	const char *attrs_account_1[] = { "cn", "dnsHostName", NULL };
	const char *attrs_account_2[] = { "cn", "dnsHostName", "objectGUID", NULL };

	const char *attrs_none[] = { NULL };

	const char *attrs_site[] = { "objectGUID", NULL };

	const char *attrs_ntds[] = { "options", "objectGUID", NULL };

	const char *attrs_1[] = { "serverReference", "cn", "dnsHostName", NULL };
	const char *attrs_2[] = { "serverReference", "cn", "dnsHostName", "objectGUID", NULL };
	const char **attrs;

	struct drsuapi_DsGetDCInfoCtr1 *ctr1;
	struct drsuapi_DsGetDCInfoCtr2 *ctr2;

	int ret, i;

	*r->out.level_out = r->in.req->req1.level;
	r->out.ctr = talloc(mem_ctx, union drsuapi_DsGetDCInfoCtr);
	W_ERROR_HAVE_NO_MEMORY(r->out.ctr);

	sites_dn = samdb_sites_dn(b_state->sam_ctx, mem_ctx);
	if (!sites_dn) {
		return WERR_DS_OBJ_NOT_FOUND;
	}

	switch (*r->out.level_out) {
	case -1:
		/* this level is not like the others */
		return WERR_UNKNOWN_LEVEL;
	case 1:
		attrs = attrs_1;
		break;
	case 2:
		attrs = attrs_2;
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	ret = ldb_search(b_state->sam_ctx, mem_ctx, &res, sites_dn, LDB_SCOPE_SUBTREE, attrs,
				 "objectClass=server");
	
	if (ret) {
		DEBUG(1, ("searching for servers in sites DN %s failed: %s\n", 
			  ldb_dn_get_linearized(sites_dn), ldb_errstring(b_state->sam_ctx)));
		return WERR_GENERAL_FAILURE;
	}

	switch (*r->out.level_out) {
	case 1:
		ctr1 = &r->out.ctr->ctr1;
		ctr1->count = res->count;
		ctr1->array = talloc_zero_array(mem_ctx, 
						struct drsuapi_DsGetDCInfo1, 
						res->count);
		for (i=0; i < res->count; i++) {
			struct ldb_dn *domain_dn;
			struct ldb_result *res_domain;
			struct ldb_result *res_account;
			struct ldb_dn *ntds_dn = ldb_dn_copy(mem_ctx, res->msgs[i]->dn);
			
			struct ldb_dn *ref_dn
				= ldb_msg_find_attr_as_dn(b_state->sam_ctx, 
							  mem_ctx, res->msgs[i], 
							  "serverReference");

			if (!ntds_dn || !ldb_dn_add_child_fmt(ntds_dn, "CN=NTDS Settings")) {
				return WERR_NOMEM;
			}

			ret = ldb_search(b_state->sam_ctx, mem_ctx, &res_account, ref_dn,
						 LDB_SCOPE_BASE, attrs_account_1, "objectClass=computer");
			if (ret == LDB_SUCCESS && res_account->count == 1) {
				const char *errstr;
				ctr1->array[i].dns_name
					= ldb_msg_find_attr_as_string(res_account->msgs[0], "dNSHostName", NULL);
				ctr1->array[i].netbios_name
					= ldb_msg_find_attr_as_string(res_account->msgs[0], "cn", NULL);
				ctr1->array[i].computer_dn
					= ldb_dn_get_linearized(res_account->msgs[0]->dn);

				/* Determine if this is the PDC */
				ret = samdb_search_for_parent_domain(b_state->sam_ctx, 
								     mem_ctx, res_account->msgs[0]->dn,
								     &domain_dn, &errstr);
				
				if (ret == LDB_SUCCESS) {
					ret = ldb_search(b_state->sam_ctx, mem_ctx, &res_domain, domain_dn,
								 LDB_SCOPE_BASE, attrs_none, "fSMORoleOwner=%s",
								 ldb_dn_get_linearized(ntds_dn));
					if (ret) {
						return WERR_GENERAL_FAILURE;
					}
					if (res_domain->count == 1) {
						ctr1->array[i].is_pdc = true;
					}
				}
			}
			if ((ret != LDB_SUCCESS) && (ret != LDB_ERR_NO_SUCH_OBJECT)) {
				DEBUG(5, ("warning: searching for computer DN %s failed: %s\n", 
					  ldb_dn_get_linearized(ref_dn), ldb_errstring(b_state->sam_ctx)));
			}

			/* Look at server DN and extract site component */
			ctr1->array[i].site_name = result_site_name(res->msgs[i]->dn);
			ctr1->array[i].server_dn = ldb_dn_get_linearized(res->msgs[i]->dn);


			ctr1->array[i].is_enabled = true;

		}
		break;
	case 2:
		ctr2 = &r->out.ctr->ctr2;
		ctr2->count = res->count;
		ctr2->array = talloc_zero_array(mem_ctx, 
						 struct drsuapi_DsGetDCInfo2, 
						 res->count);
		for (i=0; i < res->count; i++) {
			struct ldb_dn *domain_dn;
			struct ldb_result *res_domain;
			struct ldb_result *res_account;
			struct ldb_dn *ntds_dn = ldb_dn_copy(mem_ctx, res->msgs[i]->dn);
			struct ldb_result *res_ntds;
			struct ldb_dn *site_dn = ldb_dn_copy(mem_ctx, res->msgs[i]->dn);
			struct ldb_result *res_site;
			struct ldb_dn *ref_dn
				= ldb_msg_find_attr_as_dn(b_state->sam_ctx, 
							  mem_ctx, res->msgs[i], 
							  "serverReference");

			if (!ntds_dn || !ldb_dn_add_child_fmt(ntds_dn, "CN=NTDS Settings")) {
				return WERR_NOMEM;
			}

			/* Format is cn=<NETBIOS name>,cn=Servers,cn=<site>,cn=sites.... */
			if (!site_dn || !ldb_dn_remove_child_components(site_dn, 2)) {
				return WERR_NOMEM;
			}

			ret = ldb_search(b_state->sam_ctx, mem_ctx, &res_ntds, ntds_dn,
						 LDB_SCOPE_BASE, attrs_ntds, "objectClass=nTDSDSA");
			if (ret == LDB_SUCCESS && res_ntds->count == 1) {
				ctr2->array[i].is_gc
					= (ldb_msg_find_attr_as_int(res_ntds->msgs[0], "options", 0) == 1);
				ctr2->array[i].ntds_guid 
					= samdb_result_guid(res_ntds->msgs[0], "objectGUID");
				ctr2->array[i].ntds_dn = ldb_dn_get_linearized(res_ntds->msgs[0]->dn);
			}
			if ((ret != LDB_SUCCESS) && (ret != LDB_ERR_NO_SUCH_OBJECT)) {
				DEBUG(5, ("warning: searching for NTDS DN %s failed: %s\n", 
					  ldb_dn_get_linearized(ntds_dn), ldb_errstring(b_state->sam_ctx)));
			}

			ret = ldb_search(b_state->sam_ctx, mem_ctx, &res_site, site_dn,
						 LDB_SCOPE_BASE, attrs_site, "objectClass=site");
			if (ret == LDB_SUCCESS && res_site->count == 1) {
				ctr2->array[i].site_guid 
					= samdb_result_guid(res_site->msgs[0], "objectGUID");
				ctr2->array[i].site_dn = ldb_dn_get_linearized(res_site->msgs[0]->dn);
			}
			if ((ret != LDB_SUCCESS) && (ret != LDB_ERR_NO_SUCH_OBJECT)) {
				DEBUG(5, ("warning: searching for site DN %s failed: %s\n", 
					  ldb_dn_get_linearized(site_dn), ldb_errstring(b_state->sam_ctx)));
			}

			ret = ldb_search(b_state->sam_ctx, mem_ctx, &res_account, ref_dn,
						 LDB_SCOPE_BASE, attrs_account_2, "objectClass=computer");
			if (ret == LDB_SUCCESS && res_account->count == 1) {
				const char *errstr;
				ctr2->array[i].dns_name
					= ldb_msg_find_attr_as_string(res_account->msgs[0], "dNSHostName", NULL);
				ctr2->array[i].netbios_name
					= ldb_msg_find_attr_as_string(res_account->msgs[0], "cn", NULL);
				ctr2->array[i].computer_dn = ldb_dn_get_linearized(res_account->msgs[0]->dn);
				ctr2->array[i].computer_guid 
					= samdb_result_guid(res_account->msgs[0], "objectGUID");

				/* Determine if this is the PDC */
				ret = samdb_search_for_parent_domain(b_state->sam_ctx, 
								     mem_ctx, res_account->msgs[0]->dn,
								     &domain_dn, &errstr);
				
				if (ret == LDB_SUCCESS) {
					ret = ldb_search(b_state->sam_ctx, mem_ctx, &res_domain, domain_dn,
								 LDB_SCOPE_BASE, attrs_none, "fSMORoleOwner=%s",
								 ldb_dn_get_linearized(ntds_dn));
					if (ret == LDB_SUCCESS && res_domain->count == 1) {
						ctr2->array[i].is_pdc = true;
					}
					if ((ret != LDB_SUCCESS) && (ret != LDB_ERR_NO_SUCH_OBJECT)) {
						DEBUG(5, ("warning: searching for domain DN %s failed: %s\n", 
							  ldb_dn_get_linearized(domain_dn), ldb_errstring(b_state->sam_ctx)));
					}
				}
			}
			if ((ret != LDB_SUCCESS) && (ret != LDB_ERR_NO_SUCH_OBJECT)) {
				DEBUG(5, ("warning: searching for computer account DN %s failed: %s\n", 
					  ldb_dn_get_linearized(ref_dn), ldb_errstring(b_state->sam_ctx)));
			}

			/* Look at server DN and extract site component */
			ctr2->array[i].site_name = result_site_name(res->msgs[i]->dn);
			ctr2->array[i].server_dn = ldb_dn_get_linearized(res->msgs[i]->dn);
			ctr2->array[i].server_guid 
				= samdb_result_guid(res->msgs[i], "objectGUID");

			ctr2->array[i].is_enabled = true;

		}
		break;
	}
	return WERR_OK;
}

/* 
  drsuapi_DsGetDomainControllerInfo 
*/
static WERROR dcesrv_drsuapi_DsGetDomainControllerInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
						struct drsuapi_DsGetDomainControllerInfo *r)
{
	struct dcesrv_handle *h;
	struct drsuapi_bind_state *b_state;	
	DCESRV_PULL_HANDLE_WERR(h, r->in.bind_handle, DRSUAPI_BIND_HANDLE);
	b_state = h->data;

	switch (r->in.level) {
	case 1:
		return dcesrv_drsuapi_DsGetDomainControllerInfo_1(b_state, mem_ctx, r);
	}

	return WERR_UNKNOWN_LEVEL;
}



/* 
  drsuapi_DsExecuteKCC 
*/
static WERROR dcesrv_drsuapi_DsExecuteKCC(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				  struct drsuapi_DsExecuteKCC *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  drsuapi_DsReplicaGetInfo 
*/
static WERROR dcesrv_drsuapi_DsReplicaGetInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct drsuapi_DsReplicaGetInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  DRSUAPI_ADD_SID_HISTORY 
*/
static WERROR dcesrv_DRSUAPI_ADD_SID_HISTORY(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct DRSUAPI_ADD_SID_HISTORY *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
  drsuapi_DsGetMemberships2 
*/
static WERROR dcesrv_drsuapi_DsGetMemberships2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct drsuapi_DsGetMemberships2 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
  DRSUAPI_REPLICA_VERIFY_OBJECTS 
*/
static WERROR dcesrv_DRSUAPI_REPLICA_VERIFY_OBJECTS(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct DRSUAPI_REPLICA_VERIFY_OBJECTS *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  DRSUAPI_GET_OBJECT_EXISTENCE 
*/
static WERROR dcesrv_DRSUAPI_GET_OBJECT_EXISTENCE(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct DRSUAPI_GET_OBJECT_EXISTENCE *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  drsuapi_QuerySitesByCost 
*/
static WERROR dcesrv_drsuapi_QuerySitesByCost(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct drsuapi_QuerySitesByCost *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_drsuapi_s.c"
