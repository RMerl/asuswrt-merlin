/*
  ldb database library

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
 *  Name: dsdb_access
 *
 *  Description: utility functions for access checking on objects
 *
 *  Authors: Nadezhda Ivanova
 */

#include "includes.h"
#include "ldb.h"
#include "ldb_module.h"
#include "ldb_errors.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "libcli/ldap/ldap_ndr.h"
#include "param/param.h"
#include "auth/auth.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/util.h"

void dsdb_acl_debug(struct security_descriptor *sd,
		      struct security_token *token,
		      struct ldb_dn *dn,
		      bool denied,
		      int level)
{
	if (denied) {
		DEBUG(level, ("Access on %s denied", ldb_dn_get_linearized(dn)));
	} else {
		DEBUG(level, ("Access on %s granted", ldb_dn_get_linearized(dn)));
	}

	DEBUG(level,("Security context: %s\n",
		     ndr_print_struct_string(0,(ndr_print_fn_t)ndr_print_security_token,"", token)));
	DEBUG(level,("Security descriptor: %s\n",
		     ndr_print_struct_string(0,(ndr_print_fn_t)ndr_print_security_descriptor,"", sd)));
}

int dsdb_get_sd_from_ldb_message(struct ldb_context *ldb,
				 TALLOC_CTX *mem_ctx,
				 struct ldb_message *acl_res,
				 struct security_descriptor **sd)
{
	struct ldb_message_element *sd_element;
	enum ndr_err_code ndr_err;

	sd_element = ldb_msg_find_element(acl_res, "nTSecurityDescriptor");
	if (!sd_element) {
		*sd = NULL;
		return LDB_SUCCESS;
	}
	*sd = talloc(mem_ctx, struct security_descriptor);
	if(!*sd) {
		return ldb_oom(ldb);
	}
	ndr_err = ndr_pull_struct_blob(&sd_element->values[0], *sd, *sd,
				       (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ldb_operr(ldb);
	}

	return LDB_SUCCESS;
}

int dsdb_check_access_on_dn_internal(struct ldb_context *ldb,
				     struct ldb_result *acl_res,
				     TALLOC_CTX *mem_ctx,
				     struct security_token *token,
				     struct ldb_dn *dn,
				     uint32_t access_mask,
				     const struct GUID *guid)
{
	struct security_descriptor *sd = NULL;
	struct dom_sid *sid = NULL;
	struct object_tree *root = NULL;
	struct object_tree *new_node = NULL;
	NTSTATUS status;
	uint32_t access_granted;
	int ret;

	ret = dsdb_get_sd_from_ldb_message(ldb, mem_ctx, acl_res->msgs[0], &sd);
	if (ret != LDB_SUCCESS) {
		return ldb_operr(ldb);
	}
	/* Theoretically we pass the check if the object has no sd */
	if (!sd) {
		return LDB_SUCCESS;
	}
	sid = samdb_result_dom_sid(mem_ctx, acl_res->msgs[0], "objectSid");
	if (guid) {
		if (!insert_in_object_tree(mem_ctx, guid, access_mask, &root,
					   &new_node)) {
			return ldb_operr(ldb);
		}
	}
	status = sec_access_check_ds(sd, token,
				     access_mask,
				     &access_granted,
				     root,
				     sid);
	if (!NT_STATUS_IS_OK(status)) {
		dsdb_acl_debug(sd,
			       token,
			       dn,
			       true,
			       10);
		return LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
	}
	return LDB_SUCCESS;
}

/* performs an access check from outside the module stack
 * given the dn of the object to be checked, the required access
 * guid is either the guid of the extended right, or NULL
 */

int dsdb_check_access_on_dn(struct ldb_context *ldb,
			    TALLOC_CTX *mem_ctx,
			    struct ldb_dn *dn,
			    struct security_token *token,
			    uint32_t access_mask,
			    const char *ext_right)
{
	int ret;
	struct GUID guid;
	struct ldb_result *acl_res;
	static const char *acl_attrs[] = {
		"nTSecurityDescriptor",
		"objectSid",
		NULL
	};
	NTSTATUS status = GUID_from_string(ext_right, &guid);
	if (!NT_STATUS_IS_OK(status)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = dsdb_search_dn(ldb, mem_ctx, &acl_res, dn, acl_attrs, DSDB_SEARCH_SHOW_DELETED);
	if (ret != LDB_SUCCESS) {
		DEBUG(10,("access_check: failed to find object %s\n", ldb_dn_get_linearized(dn)));
		return ret;
	}

	return dsdb_check_access_on_dn_internal(ldb, acl_res,
						mem_ctx,
						token,
						dn,
						access_mask,
						&guid);
}

