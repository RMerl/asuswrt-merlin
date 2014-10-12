/*
   samba3sid module

   Copyright (C) Andrew Bartlett 2010
   Copyright (C) Andrew Tridgell 2010

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
  add objectSid to users and groups using samba3 nextRid method
 */

#include "includes.h"
#include "libcli/ldap/ldap_ndr.h"
#include "ldb_module.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/samdb/ldb_modules/util.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "ldb_wrap.h"
#include "param/param.h"

/*
  RID algorithm from pdb_ldap.c in source3/passdb/
  (loosely based on Volkers code)
 */
static int samba3sid_next_sid(struct ldb_module *module,
			      TALLOC_CTX *mem_ctx, char **sid,
			      struct ldb_request *parent)
{
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	struct ldb_result *res;
	const char *attrs[] = { "sambaNextRid", "sambaNextUserRid",
				"sambaNextGroupRid", "sambaSID", NULL };
	int ret;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct ldb_message *msg;
	uint32_t sambaNextRid, sambaNextGroupRid, sambaNextUserRid, rid;
	const char *sambaSID;

	ret = dsdb_module_search(module, tmp_ctx, &res, NULL, LDB_SCOPE_SUBTREE,
				 attrs,
				 DSDB_FLAG_NEXT_MODULE |
				 DSDB_SEARCH_SEARCH_ALL_PARTITIONS,
				 parent,
				 "(&(objectClass=sambaDomain)(sambaDomainName=%s))",
				 lpcfg_sam_name(ldb_get_opaque(ldb, "loadparm")));
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb,
				       __location__
				       ": Failed to find domain object - %s",
				       ldb_errstring(ldb));
		talloc_free(tmp_ctx);
		return ret;
	}
	if (res->count != 1) {
		ldb_asprintf_errstring(ldb,
				       __location__
				       ": Expected exactly 1 domain object - got %u",
				       res->count);
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	msg = res->msgs[0];

	sambaNextRid = ldb_msg_find_attr_as_uint(msg, "sambaNextRid",
						 (uint32_t) -1);
	sambaNextUserRid = ldb_msg_find_attr_as_uint(msg, "sambaNextUserRid",
						     (uint32_t) -1);
	sambaNextGroupRid = ldb_msg_find_attr_as_uint(msg, "sambaNextGroupRid",
						      (uint32_t) -1);
	sambaSID = ldb_msg_find_attr_as_string(msg, "sambaSID", NULL);

	if (sambaSID == NULL) {
		ldb_asprintf_errstring(ldb,
				       __location__
				       ": No sambaSID in %s",
				       ldb_dn_get_linearized(msg->dn));
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* choose the highest of the 3 - see pdb_ldap.c for an
	 * explaination */
	rid = sambaNextRid;
	if ((sambaNextUserRid != (uint32_t) -1) && (sambaNextUserRid > rid)) {
		rid = sambaNextUserRid;
	}
	if ((sambaNextGroupRid != (uint32_t) -1) && (sambaNextGroupRid > rid)) {
		rid = sambaNextGroupRid;
	}
	if (rid == (uint32_t) -1) {
		ldb_asprintf_errstring(ldb,
				       __location__
				       ": No sambaNextRid in %s",
				       ldb_dn_get_linearized(msg->dn));
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* sambaNextRid is actually the previous RID .... */
	rid += 1;

	(*sid) = talloc_asprintf(tmp_ctx, "%s-%u", sambaSID, rid);
	if (!*sid) {
		talloc_free(tmp_ctx);
		return ldb_module_oom(module);
	}

	ret = dsdb_module_constrainted_update_uint32(module, msg->dn,
						     "sambaNextRid",
						     &sambaNextRid, &rid, parent);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb,
				       __location__
				       ": Failed to update sambaNextRid - %s",
				       ldb_errstring(ldb));
		talloc_free(tmp_ctx);
		return ret;
	}

	talloc_steal(mem_ctx, *sid);
	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}



/* add */
static int samba3sid_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	int ret;
	const struct ldb_message *msg = req->op.add.message;
	struct ldb_message *new_msg;
	char *sid;
	struct ldb_request *new_req;

	ldb = ldb_module_get_ctx(module);

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	if (!samdb_find_attribute(ldb, msg, "objectclass", "posixAccount") &&
	    !samdb_find_attribute(ldb, msg, "objectclass", "posixGroup")) {
		/* its not a user or a group */
		return ldb_next_request(module, req);
	}

	if (ldb_msg_find_element(msg, "sambaSID")) {
		/* a SID was supplied */
		return ldb_next_request(module, req);
	}

	new_msg = ldb_msg_copy_shallow(req, req->op.add.message);
	if (!new_msg) {
		return ldb_module_oom(module);
	}

	ret = samba3sid_next_sid(module, new_msg, &sid, req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_msg_add_steal_string(new_msg, "sambaSID", sid);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_build_add_req(&new_req, ldb, req,
				new_msg,
				req->controls,
				req, dsdb_next_callback,
				req);
	LDB_REQ_SET_LOCATION(new_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(module, new_req);
}

static const struct ldb_module_ops ldb_samba3sid_module_ops = {
	.name          = "samba3sid",
	.add           = samba3sid_add,
};


int ldb_samba3sid_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_samba3sid_module_ops);
}
