/*
  ACL utility functions

  Copyright (C) Nadezhda Ivanova 2010

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

/*
 *  Name: acl_util
 *
 *  Component: ldb ACL modules
 *
 *  Description: Some auxiliary functions used for access checking
 *
 *  Author: Nadezhda Ivanova
 */
#include "includes.h"
#include "ldb_module.h"
#include "auth/auth.h"
#include "libcli/security/security.h"
#include "dsdb/samdb/samdb.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "param/param.h"
#include "dsdb/samdb/ldb_modules/util.h"

struct security_token *acl_user_token(struct ldb_module *module)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct auth_session_info *session_info
		= (struct auth_session_info *)ldb_get_opaque(ldb, "sessionInfo");
	if(!session_info) {
		return NULL;
	}
	return session_info->security_token;
}

/* performs an access check from inside the module stack
 * given the dn of the object to be checked, the required access
 * guid is either the guid of the extended right, or NULL
 */

int dsdb_module_check_access_on_dn(struct ldb_module *module,
				   TALLOC_CTX *mem_ctx,
				   struct ldb_dn *dn,
				   uint32_t access_mask,
				   const struct GUID *guid,
				   struct ldb_request *parent)
{
	int ret;
	struct ldb_result *acl_res;
	static const char *acl_attrs[] = {
		"nTSecurityDescriptor",
		"objectSid",
		NULL
	};
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct auth_session_info *session_info
		= (struct auth_session_info *)ldb_get_opaque(ldb, "sessionInfo");
	if(!session_info) {
		return ldb_operr(ldb);
	}
	ret = dsdb_module_search_dn(module, mem_ctx, &acl_res, dn,
				    acl_attrs,
				    DSDB_FLAG_NEXT_MODULE |
				    DSDB_SEARCH_SHOW_RECYCLED,
				    parent);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("access_check: failed to find object %s\n", ldb_dn_get_linearized(dn)));
		return ret;
	}
	return dsdb_check_access_on_dn_internal(ldb, acl_res,
						mem_ctx,
						session_info->security_token,
						dn,
						access_mask,
						guid);
}

int dsdb_module_check_access_on_guid(struct ldb_module *module,
				     TALLOC_CTX *mem_ctx,
				     struct GUID *guid,
				     uint32_t access_mask,
				     const struct GUID *oc_guid,
				     struct ldb_request *parent)
{
	int ret;
	struct ldb_result *acl_res;
	static const char *acl_attrs[] = {
		"nTSecurityDescriptor",
		"objectSid",
		NULL
	};
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct auth_session_info *session_info
		= (struct auth_session_info *)ldb_get_opaque(ldb, "sessionInfo");
	if(!session_info) {
		return ldb_operr(ldb);
	}
	ret = dsdb_module_search(module, mem_ctx, &acl_res, NULL, LDB_SCOPE_SUBTREE,
				 acl_attrs,
				 DSDB_FLAG_NEXT_MODULE |
				 DSDB_SEARCH_SHOW_RECYCLED,
				 parent,
				 "objectGUID=%s", GUID_string(mem_ctx, guid));

	if (ret != LDB_SUCCESS || acl_res->count == 0) {
		DEBUG(0,("access_check: failed to find object %s\n", GUID_string(mem_ctx, guid)));
		return ret;
	}
	return dsdb_check_access_on_dn_internal(ldb, acl_res,
						mem_ctx,
						session_info->security_token,
						acl_res->msgs[0]->dn,
						access_mask,
						oc_guid);
}

int acl_check_access_on_attribute(struct ldb_module *module,
				  TALLOC_CTX *mem_ctx,
				  struct security_descriptor *sd,
				  struct dom_sid *rp_sid,
				  uint32_t access_mask,
				  const struct dsdb_attribute *attr)
{
	int ret;
	NTSTATUS status;
	uint32_t access_granted;
	struct object_tree *root = NULL;
	struct object_tree *new_node = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	struct security_token *token = acl_user_token(module);
	if (attr) {
		if (!GUID_all_zero(&attr->attributeSecurityGUID)) {
			if (!insert_in_object_tree(tmp_ctx,
						   &attr->attributeSecurityGUID,
						   access_mask, &root,
						   &new_node)) {
				DEBUG(10, ("acl_search: cannot add to object tree securityGUID\n"));
				goto fail;
			}

			if (!insert_in_object_tree(tmp_ctx,
						   &attr->schemaIDGUID,
						   access_mask, &new_node,
						   &new_node)) {
				DEBUG(10, ("acl_search: cannot add to object tree attributeGUID\n"));
				goto fail;
			}
		}
		else {
			if (!insert_in_object_tree(tmp_ctx,
						   &attr->schemaIDGUID,
						   access_mask, &root,
						   &new_node)) {
				DEBUG(10, ("acl_search: cannot add to object tree attributeGUID\n"));
				goto fail;
			}
		}
	}
	status = sec_access_check_ds(sd, token,
				     access_mask,
				     &access_granted,
				     root,
				     rp_sid);
	if (!NT_STATUS_IS_OK(status)) {
		ret = LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
	}
	else {
		ret = LDB_SUCCESS;
	}
	talloc_free(tmp_ctx);
	return ret;
fail:
	talloc_free(tmp_ctx);
	return ldb_operr(ldb_module_get_ctx(module));
}


/* checks for validated writes */
int acl_check_extended_right(TALLOC_CTX *mem_ctx,
			     struct security_descriptor *sd,
			     struct security_token *token,
			     const char *ext_right,
			     uint32_t right_type,
			     struct dom_sid *sid)
{
	struct GUID right;
	NTSTATUS status;
	uint32_t access_granted;
	struct object_tree *root = NULL;
	struct object_tree *new_node = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	GUID_from_string(ext_right, &right);

	if (!insert_in_object_tree(tmp_ctx, &right, right_type,
				   &root, &new_node)) {
		DEBUG(10, ("acl_ext_right: cannot add to object tree\n"));
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	status = sec_access_check_ds(sd, token,
				     right_type,
				     &access_granted,
				     root,
				     sid);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
	}
	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}

const char *acl_user_name(TALLOC_CTX *mem_ctx, struct ldb_module *module)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct auth_session_info *session_info
		= (struct auth_session_info *)ldb_get_opaque(ldb, "sessionInfo");
	if (!session_info) {
		return "UNKNOWN (NULL)";
	}

	return talloc_asprintf(mem_ctx, "%s\\%s",
			       session_info->info->domain_name,
			       session_info->info->account_name);
}
