/*
   Unix SMB/CIFS implementation.

   implement the DsWriteAccountSpn call

   Copyright (C) Stefan Metzmacher 2009
   Copyright (C) Andrew Tridgell   2010

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
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/util.h"
#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"
#include "libcli/security/security.h"
#include "libcli/security/session.h"
#include "rpc_server/drsuapi/dcesrv_drsuapi.h"
#include "auth/session.h"

/*
  check that the SPN update should be allowed as an override
  via sam_ctx_system

  This is only called if the client is not a domain controller or
  administrator
 */
static bool writespn_check_spn(struct drsuapi_bind_state *b_state,
			       struct dcesrv_call_state *dce_call,
			       struct ldb_dn *dn,
			       const char *spn)
{
	/*
	 * we only allow SPN updates if:
	 *
	 * 1) they are on the clients own account object
	 * 2) they are of the form SERVICE/dnshostname
	 */
	struct dom_sid *user_sid, *sid;
	TALLOC_CTX *tmp_ctx = talloc_new(dce_call);
	struct ldb_result *res;
	const char *attrs[] = { "objectSID", "dNSHostName", NULL };
	int ret;
	krb5_context krb_ctx;
	krb5_error_code kerr;
	krb5_principal principal;
	const char *dns_name, *dnsHostName;

	/* The service principal name shouldn't be NULL */
	if (spn == NULL) {
		talloc_free(tmp_ctx);
		return false;
	}

	/*
	  get the objectSid of the DN that is being modified, and
	  check it matches the user_sid in their token
	 */

	ret = dsdb_search_dn(b_state->sam_ctx, tmp_ctx, &res, dn, attrs,
			     DSDB_SEARCH_ONE_ONLY);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return false;
	}

	user_sid = &dce_call->conn->auth_state.session_info->security_token->sids[PRIMARY_USER_SID_INDEX];
	sid = samdb_result_dom_sid(tmp_ctx, res->msgs[0], "objectSid");
	if (sid == NULL) {
		talloc_free(tmp_ctx);
		return false;
	}

	dnsHostName = ldb_msg_find_attr_as_string(res->msgs[0], "dNSHostName",
						  NULL);
	if (dnsHostName == NULL) {
		talloc_free(tmp_ctx);
		return false;
	}

	if (!dom_sid_equal(sid, user_sid)) {
		talloc_free(tmp_ctx);
		return false;
	}

	kerr = smb_krb5_init_context_basic(tmp_ctx,
					   dce_call->conn->dce_ctx->lp_ctx,
					   &krb_ctx);
	if (kerr != 0) {
		talloc_free(tmp_ctx);
		return false;
	}

	ret = krb5_parse_name_flags(krb_ctx, spn, KRB5_PRINCIPAL_PARSE_NO_REALM,
				    &principal);
	if (kerr != 0) {
		krb5_free_context(krb_ctx);
		talloc_free(tmp_ctx);
		return false;
	}

	if (principal->name.name_string.len != 2) {
		krb5_free_principal(krb_ctx, principal);
		krb5_free_context(krb_ctx);
		talloc_free(tmp_ctx);
		return false;
	}

	dns_name = principal->name.name_string.val[1];

	if (strcasecmp(dns_name, dnsHostName) != 0) {
		krb5_free_principal(krb_ctx, principal);
		krb5_free_context(krb_ctx);
		talloc_free(tmp_ctx);
		return false;
	}

	/* its a simple update on their own account - allow it with
	 * permissions override */
	krb5_free_principal(krb_ctx, principal);
	krb5_free_context(krb_ctx);
	talloc_free(tmp_ctx);

	return true;
}

/*
  drsuapi_DsWriteAccountSpn
*/
WERROR dcesrv_drsuapi_DsWriteAccountSpn(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					struct drsuapi_DsWriteAccountSpn *r)
{
	struct drsuapi_bind_state *b_state;
	struct dcesrv_handle *h;
	enum security_user_level level;

	*r->out.level_out = r->in.level;

	DCESRV_PULL_HANDLE_WERR(h, r->in.bind_handle, DRSUAPI_BIND_HANDLE);
	b_state = h->data;

	r->out.res = talloc(mem_ctx, union drsuapi_DsWriteAccountSpnResult);
	W_ERROR_HAVE_NO_MEMORY(r->out.res);

	level = security_session_user_level(dce_call->conn->auth_state.session_info, NULL);

	switch (r->in.level) {
		case 1: {
			struct drsuapi_DsWriteAccountSpnRequest1 *req;
			struct ldb_message *msg;
			uint32_t count;
			unsigned int i;
			int ret;
			unsigned spn_count=0;
			bool passed_checks = true;
			struct ldb_context *sam_ctx;

			req = &r->in.req->req1;
			count = req->count;

			msg = ldb_msg_new(mem_ctx);
			if (msg == NULL) {
				return WERR_NOMEM;
			}

			msg->dn = ldb_dn_new(msg, b_state->sam_ctx,
					     req->object_dn);
			if ( ! ldb_dn_validate(msg->dn)) {
				r->out.res->res1.status = WERR_OK;
				return WERR_OK;
			}

			/* construct mods */
			for (i = 0; i < count; i++) {
				if (!writespn_check_spn(b_state,
						       dce_call,
						       msg->dn,
						       req->spn_names[i].str)) {
					passed_checks = false;
				}
				ret = ldb_msg_add_string(msg,
							 "servicePrincipalName",
							 req->spn_names[i].str);
				if (ret != LDB_SUCCESS) {
					return WERR_NOMEM;
				}
				spn_count++;
			}

			if (msg->num_elements == 0) {
				DEBUG(2,("No SPNs need changing on %s\n",
					 ldb_dn_get_linearized(msg->dn)));
				r->out.res->res1.status = WERR_OK;
				return WERR_OK;
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

			if (passed_checks && b_state->sam_ctx_system) {
				sam_ctx = b_state->sam_ctx_system;
			} else {
				sam_ctx = b_state->sam_ctx;
			}

			/* Apply to database */
			ret = dsdb_modify(sam_ctx, msg, DSDB_MODIFY_PERMISSIVE);
			if (ret != LDB_SUCCESS) {
				DEBUG(0,("Failed to modify SPNs on %s: %s\n",
					 ldb_dn_get_linearized(msg->dn),
					 ldb_errstring(b_state->sam_ctx)));
				r->out.res->res1.status = WERR_ACCESS_DENIED;
			} else {
				DEBUG(2,("Modified %u SPNs on %s\n", spn_count,
					 ldb_dn_get_linearized(msg->dn)));
				r->out.res->res1.status = WERR_OK;
			}

			return WERR_OK;
		}
	}

	return WERR_UNKNOWN_LEVEL;
}
