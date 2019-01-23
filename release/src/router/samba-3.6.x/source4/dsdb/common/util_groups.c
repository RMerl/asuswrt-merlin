/*
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2001-2010
   Copyright (C) Stefan Metzmacher                         2005
   Copyright (C) Matthias Dieter Wallnöfer                 2009

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
#include "auth/auth.h"
#include <ldb.h>
#include "dsdb/samdb/samdb.h"
#include "libcli/security/security.h"
#include "dsdb/common/util.h"

/* This function tests if a SID structure "sids" contains the SID "sid" */
static bool sids_contains_sid(const struct dom_sid *sids,
			      const unsigned int num_sids,
			      const struct dom_sid *sid)
{
	unsigned int i;

	for (i = 0; i < num_sids; i++) {
		if (dom_sid_equal(&sids[i], sid))
			return true;
	}
	return false;
}

/*
 * This function generates the transitive closure of a given SAM object "dn_val"
 * (it basically expands nested memberships).
 * If the object isn't located in the "res_sids" structure yet and the
 * "only_childs" flag is false, we add it to "res_sids".
 * Then we've always to consider the "memberOf" attributes. We invoke the
 * function recursively on each of it with the "only_childs" flag set to
 * "false".
 * The "only_childs" flag is particularly useful if you have a user object and
 * want to include all it's groups (referenced with "memberOf") but not itself
 * or considering if that object matches the filter.
 *
 * At the beginning "res_sids" should reference to a NULL pointer.
 */
NTSTATUS dsdb_expand_nested_groups(struct ldb_context *sam_ctx,
				   struct ldb_val *dn_val, const bool only_childs, const char *filter,
				   TALLOC_CTX *res_sids_ctx, struct dom_sid **res_sids,
				   unsigned int *num_res_sids)
{
	const char * const attrs[] = { "memberOf", NULL };
	unsigned int i;
	int ret;
	bool already_there;
	struct ldb_dn *dn;
	struct dom_sid sid;
	TALLOC_CTX *tmp_ctx;
	struct ldb_result *res;
	NTSTATUS status;
	const struct ldb_message_element *el;

	if (*res_sids == NULL) {
		*num_res_sids = 0;
	}

	if (!sam_ctx) {
		DEBUG(0, ("No SAM available, cannot determine local groups\n"));
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	tmp_ctx = talloc_new(res_sids_ctx);

	dn = ldb_dn_from_ldb_val(tmp_ctx, sam_ctx, dn_val);
	if (dn == NULL) {
		talloc_free(tmp_ctx);
		DEBUG(0, (__location__ ": we failed parsing DN %.*s, so we cannot calculate the group token\n",
			  (int)dn_val->length, dn_val->data));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	status = dsdb_get_extended_dn_sid(dn, &sid, "SID");
	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
		/* If we fail finding a SID then this is no error since it could
		 * be a non SAM object - e.g. a group with object class
		 * "groupOfNames" */
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	} else if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, (__location__ ": when parsing DN '%s' we failed to parse it's SID component, so we cannot calculate the group token: %s\n",
			  ldb_dn_get_extended_linearized(tmp_ctx, dn, 1),
			  nt_errstr(status)));
		talloc_free(tmp_ctx);
		return status;
	}

	if (!ldb_dn_minimise(dn)) {
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (only_childs) {
		ret = dsdb_search_dn(sam_ctx, tmp_ctx, &res, dn, attrs,
				     DSDB_SEARCH_SHOW_EXTENDED_DN);
	} else {
		/* This is an O(n^2) linear search */
		already_there = sids_contains_sid(*res_sids,
						  *num_res_sids, &sid);
		if (already_there) {
			talloc_free(tmp_ctx);
			return NT_STATUS_OK;
		}

		ret = dsdb_search(sam_ctx, tmp_ctx, &res, dn, LDB_SCOPE_BASE,
				  attrs, DSDB_SEARCH_SHOW_EXTENDED_DN, "%s",
				  filter);
	}

	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	if (ret != LDB_SUCCESS) {
		DEBUG(1, (__location__ ": dsdb_search for %s failed: %s\n",
			  ldb_dn_get_extended_linearized(tmp_ctx, dn, 1),
			  ldb_errstring(sam_ctx)));
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* We may get back 0 results, if the SID didn't match the filter - such as it wasn't a domain group, for example */
	if (res->count != 1) {
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	/* We only apply this test once we know the SID matches the filter */
	if (!only_childs) {
		*res_sids = talloc_realloc(res_sids_ctx, *res_sids,
			struct dom_sid, *num_res_sids + 1);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(*res_sids, tmp_ctx);
		(*res_sids)[*num_res_sids] = sid;
		++(*num_res_sids);
	}

	el = ldb_msg_find_element(res->msgs[0], "memberOf");

	for (i = 0; el && i < el->num_values; i++) {
		status = dsdb_expand_nested_groups(sam_ctx, &el->values[i],
						   false, filter, res_sids_ctx, res_sids, num_res_sids);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(tmp_ctx);
			return status;
		}
	}

	talloc_free(tmp_ctx);

	return NT_STATUS_OK;
}
