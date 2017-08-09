/*
   Unix SMB/CIFS implementation.

   Map SIDs to unixids and back

   Copyright (C) Kai Blin 2008

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
#include "librpc/gen_ndr/ndr_security.h"
#include <ldb.h>
#include "ldb_wrap.h"
#include "param/param.h"
#include "winbind/idmap.h"
#include "libcli/security/security.h"
#include "libcli/ldap/ldap_ndr.h"

/**
 * Get uid/gid bounds from idmap database
 *
 * \param idmap_ctx idmap context to use
 * \param low lower uid/gid bound is stored here
 * \param high upper uid/gid bound is stored here
 * \return 0 on success, nonzero on failure
 */
static int idmap_get_bounds(struct idmap_context *idmap_ctx, uint32_t *low,
		uint32_t *high)
{
	int ret = -1;
	struct ldb_context *ldb = idmap_ctx->ldb_ctx;
	struct ldb_dn *dn;
	struct ldb_result *res = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(idmap_ctx);
	uint32_t lower_bound = (uint32_t) -1;
	uint32_t upper_bound = (uint32_t) -1;

	dn = ldb_dn_new(tmp_ctx, ldb, "CN=CONFIG");
	if (dn == NULL) goto failed;

	ret = ldb_search(ldb, tmp_ctx, &res, dn, LDB_SCOPE_BASE, NULL, NULL);
	if (ret != LDB_SUCCESS) goto failed;

	if (res->count != 1) {
		ret = -1;
		goto failed;
	}

	lower_bound = ldb_msg_find_attr_as_uint(res->msgs[0], "lowerBound", -1);
	if (lower_bound != (uint32_t) -1) {
		ret = LDB_SUCCESS;
	} else {
		ret = -1;
		goto failed;
	}

	upper_bound = ldb_msg_find_attr_as_uint(res->msgs[0], "upperBound", -1);
	if (upper_bound != (uint32_t) -1) {
		ret = LDB_SUCCESS;
	} else {
		ret = -1;
	}

failed:
	talloc_free(tmp_ctx);
	*low  = lower_bound;
	*high = upper_bound;
	return ret;
}

/**
 * Add a dom_sid structure to a ldb_message
 * \param idmap_ctx idmap context to use
 * \param mem_ctx talloc context to use
 * \param ldb_message ldb message to add dom_sid to
 * \param attr_name name of the attribute to store the dom_sid in
 * \param sid dom_sid to store
 * \return 0 on success, an ldb error code on failure.
 */
static int idmap_msg_add_dom_sid(struct idmap_context *idmap_ctx,
		TALLOC_CTX *mem_ctx, struct ldb_message *msg,
		const char *attr_name, const struct dom_sid *sid)
{
	struct ldb_val val;
	enum ndr_err_code ndr_err;

	ndr_err = ndr_push_struct_blob(&val, mem_ctx, sid,
				       (ndr_push_flags_fn_t)ndr_push_dom_sid);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return -1;
	}

	return ldb_msg_add_value(msg, attr_name, &val, NULL);
}

/**
 * Get a dom_sid structure from a ldb message.
 *
 * \param mem_ctx talloc context to allocate dom_sid memory in
 * \param msg ldb_message to get dom_sid from
 * \param attr_name key that has the dom_sid as data
 * \return dom_sid structure on success, NULL on failure
 */
static struct dom_sid *idmap_msg_get_dom_sid(TALLOC_CTX *mem_ctx,
		struct ldb_message *msg, const char *attr_name)
{
	struct dom_sid *sid;
	const struct ldb_val *val;
	enum ndr_err_code ndr_err;

	val = ldb_msg_find_ldb_val(msg, attr_name);
	if (val == NULL) {
		return NULL;
	}

	sid = talloc(mem_ctx, struct dom_sid);
	if (sid == NULL) {
		return NULL;
	}

	ndr_err = ndr_pull_struct_blob(val, sid, sid,
				       (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(sid);
		return NULL;
	}

	return sid;
}

/**
 * Initialize idmap context
 *
 * talloc_free to close.
 *
 * \param mem_ctx talloc context to use.
 * \return allocated idmap_context on success, NULL on error
 */
struct idmap_context *idmap_init(TALLOC_CTX *mem_ctx,
				 struct tevent_context *ev_ctx,
				 struct loadparm_context *lp_ctx)
{
	struct idmap_context *idmap_ctx;

	idmap_ctx = talloc(mem_ctx, struct idmap_context);
	if (idmap_ctx == NULL) {
		return NULL;
	}

	idmap_ctx->lp_ctx = lp_ctx;

	idmap_ctx->ldb_ctx = ldb_wrap_connect(mem_ctx, ev_ctx, lp_ctx,
					      lpcfg_idmap_url(lp_ctx),
					      system_session(lp_ctx),
					      NULL, 0);
	if (idmap_ctx->ldb_ctx == NULL) {
		return NULL;
	}

	idmap_ctx->unix_groups_sid = dom_sid_parse_talloc(mem_ctx, "S-1-22-2");
	if (idmap_ctx->unix_groups_sid == NULL) {
		return NULL;
	}

	idmap_ctx->unix_users_sid = dom_sid_parse_talloc(mem_ctx, "S-1-22-1");
	if (idmap_ctx->unix_users_sid == NULL) {
		return NULL;
	}

	return idmap_ctx;
}

/**
 * Convert an unixid to the corresponding SID
 *
 * \param idmap_ctx idmap context to use
 * \param mem_ctx talloc context the memory for the struct dom_sid is allocated
 * from.
 * \param unixid pointer to a unixid struct to convert
 * \param sid pointer that will take the struct dom_sid pointer if the mapping
 * succeeds.
 * \return NT_STATUS_OK on success, NT_STATUS_NONE_MAPPED if mapping not
 * possible or some other NTSTATUS that is more descriptive on failure.
 */

static NTSTATUS idmap_xid_to_sid(struct idmap_context *idmap_ctx,
				 TALLOC_CTX *mem_ctx,
				 const struct unixid *unixid,
				 struct dom_sid **sid)
{
	int ret;
	NTSTATUS status = NT_STATUS_NONE_MAPPED;
	struct ldb_context *ldb = idmap_ctx->ldb_ctx;
	struct ldb_result *res = NULL;
	struct dom_sid *unix_sid, *new_sid;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	const char *id_type;

	switch (unixid->type) {
		case ID_TYPE_UID:
			id_type = "ID_TYPE_UID";
			break;
		case ID_TYPE_GID:
			id_type = "ID_TYPE_GID";
			break;
		default:
			DEBUG(1, ("unixid->type must be type gid or uid (got %u) for lookup with id %lu\n",
				  (unsigned)unixid->type, (unsigned long)unixid->id));
			status = NT_STATUS_NONE_MAPPED;
			goto failed;
	}

	ret = ldb_search(ldb, tmp_ctx, &res, NULL, LDB_SCOPE_SUBTREE,
				 NULL, "(&(|(type=ID_TYPE_BOTH)(type=%s))"
				 "(xidNumber=%u))", id_type, unixid->id);
	if (ret != LDB_SUCCESS) {
		DEBUG(1, ("Search failed: %s\n", ldb_errstring(ldb)));
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	if (res->count == 1) {
		*sid = idmap_msg_get_dom_sid(mem_ctx, res->msgs[0],
					     "objectSid");
		if (*sid == NULL) {
			DEBUG(1, ("Failed to get sid from db: %u\n", ret));
			status = NT_STATUS_NONE_MAPPED;
			goto failed;
		}
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	DEBUG(6, ("xid not found in idmap db, create S-1-22- SID.\n"));

	/* For local users/groups , we just create a rid = uid/gid */
	if (unixid->type == ID_TYPE_UID) {
		unix_sid = dom_sid_parse_talloc(tmp_ctx, "S-1-22-1");
	} else {
		unix_sid = dom_sid_parse_talloc(tmp_ctx, "S-1-22-2");
	}
	if (unix_sid == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	new_sid = dom_sid_add_rid(mem_ctx, unix_sid, unixid->id);
	if (new_sid == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	*sid = new_sid;
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;

failed:
	talloc_free(tmp_ctx);
	return status;
}


/**
 * Map a SID to an unixid struct.
 *
 * If no mapping exists, a new mapping will be created.
 *
 * \todo Check if SIDs can be resolved if lpcfg_idmap_trusted_only() == true
 * \todo Fix backwards compatibility for Samba3
 *
 * \param idmap_ctx idmap context to use
 * \param mem_ctx talloc context to use
 * \param sid SID to map to an unixid struct
 * \param unixid pointer to a unixid struct
 * \return NT_STATUS_OK on success, NT_STATUS_INVALID_SID if the sid is not from
 * a trusted domain and idmap trusted only = true, NT_STATUS_NONE_MAPPED if the
 * mapping failed.
 */
static NTSTATUS idmap_sid_to_xid(struct idmap_context *idmap_ctx,
				 TALLOC_CTX *mem_ctx,
				 const struct dom_sid *sid,
				 struct unixid *unixid)
{
	int ret;
	NTSTATUS status = NT_STATUS_NONE_MAPPED;
	struct ldb_context *ldb = idmap_ctx->ldb_ctx;
	struct ldb_dn *dn;
	struct ldb_message *hwm_msg, *map_msg;
	struct ldb_result *res = NULL;
	int trans;
	uint32_t low, high, hwm, new_xid;
	char *sid_string, *unixid_string, *hwm_string;
	bool hwm_entry_exists;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	if (dom_sid_in_domain(idmap_ctx->unix_users_sid, sid)) {
		uint32_t rid;
		DEBUG(6, ("This is a local unix uid, just calculate that.\n"));
		status = dom_sid_split_rid(tmp_ctx, sid, NULL, &rid);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(tmp_ctx);
			return status;
		}

		unixid->id = rid;
		unixid->type = ID_TYPE_UID;

		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	if (dom_sid_in_domain(idmap_ctx->unix_groups_sid, sid)) {
		uint32_t rid;
		DEBUG(6, ("This is a local unix gid, just calculate that.\n"));
		status = dom_sid_split_rid(tmp_ctx, sid, NULL, &rid);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(tmp_ctx);
			return status;
		}

		unixid->id = rid;
		unixid->type = ID_TYPE_GID;

		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	 }

	ret = ldb_search(ldb, tmp_ctx, &res, NULL, LDB_SCOPE_SUBTREE,
				 NULL, "(&(objectClass=sidMap)(objectSid=%s))",
				 ldap_encode_ndr_dom_sid(tmp_ctx, sid));
	if (ret != LDB_SUCCESS) {
		DEBUG(1, ("Search failed: %s\n", ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return NT_STATUS_NONE_MAPPED;
	}

	if (res->count == 1) {
		const char *type = ldb_msg_find_attr_as_string(res->msgs[0],
							       "type", NULL);
		new_xid = ldb_msg_find_attr_as_uint(res->msgs[0], "xidNumber",
						    -1);
		if (new_xid == (uint32_t) -1) {
			DEBUG(1, ("Invalid xid mapping.\n"));
			talloc_free(tmp_ctx);
			return NT_STATUS_NONE_MAPPED;
		}

		if (type == NULL) {
			DEBUG(1, ("Invalid type for mapping entry.\n"));
			talloc_free(tmp_ctx);
			return NT_STATUS_NONE_MAPPED;
		}

		unixid->id = new_xid;

		if (strcmp(type, "ID_TYPE_BOTH") == 0) {
			unixid->type = ID_TYPE_BOTH;
		} else if (strcmp(type, "ID_TYPE_UID") == 0) {
			unixid->type = ID_TYPE_UID;
		} else {
			unixid->type = ID_TYPE_GID;
		}

		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	DEBUG(6, ("No existing mapping found, attempting to create one.\n"));

	trans = ldb_transaction_start(ldb);
	if (trans != LDB_SUCCESS) {
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	/* Redo the search to make sure noone changed the mapping while we
	 * weren't looking */
	ret = ldb_search(ldb, tmp_ctx, &res, NULL, LDB_SCOPE_SUBTREE,
				 NULL, "(&(objectClass=sidMap)(objectSid=%s))",
				 ldap_encode_ndr_dom_sid(tmp_ctx, sid));
	if (ret != LDB_SUCCESS) {
		DEBUG(1, ("Search failed: %s\n", ldb_errstring(ldb)));
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	if (res->count > 0) {
		DEBUG(1, ("Database changed while trying to add a sidmap.\n"));
		status = NT_STATUS_RETRY;
		goto failed;
	}

	/*FIXME: if lpcfg_idmap_trusted_only() == true, check if SID can be
	 * resolved here. */

	ret = idmap_get_bounds(idmap_ctx, &low, &high);
	if (ret != LDB_SUCCESS) {
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	dn = ldb_dn_new(tmp_ctx, ldb, "CN=CONFIG");
	if (dn == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	ret = ldb_search(ldb, tmp_ctx, &res, dn, LDB_SCOPE_BASE, NULL, NULL);
	if (ret != LDB_SUCCESS) {
		DEBUG(1, ("Search failed: %s\n", ldb_errstring(ldb)));
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	if (res->count != 1) {
		DEBUG(1, ("No CN=CONFIG record, idmap database is broken.\n"));
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	hwm = ldb_msg_find_attr_as_uint(res->msgs[0], "xidNumber", -1);
	if (hwm == (uint32_t)-1) {
		hwm = low;
		hwm_entry_exists = false;
	} else {
		hwm_entry_exists = true;
	}

	if (hwm > high) {
		DEBUG(1, ("Out of xids to allocate.\n"));
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	hwm_msg = ldb_msg_new(tmp_ctx);
	if (hwm_msg == NULL) {
		DEBUG(1, ("Out of memory when creating ldb_message\n"));
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	hwm_msg->dn = dn;

	new_xid = hwm;
	hwm++;

	hwm_string = talloc_asprintf(tmp_ctx, "%u", hwm);
	if (hwm_string == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	sid_string = dom_sid_string(tmp_ctx, sid);
	if (sid_string == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	unixid_string = talloc_asprintf(tmp_ctx, "%u", new_xid);
	if (unixid_string == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	if (hwm_entry_exists) {
		struct ldb_message_element *els;
		struct ldb_val *vals;

		/* We're modifying the entry, not just adding a new one. */
		els = talloc_array(tmp_ctx, struct ldb_message_element, 2);
		if (els == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto failed;
		}

		vals = talloc_array(tmp_ctx, struct ldb_val, 2);
		if (els == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto failed;
		}

		hwm_msg->num_elements = 2;
		hwm_msg->elements = els;

		els[0].num_values = 1;
		els[0].values = &vals[0];
		els[0].flags = LDB_FLAG_MOD_DELETE;
		els[0].name = talloc_strdup(tmp_ctx, "xidNumber");
		if (els[0].name == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto failed;
		}

		els[1].num_values = 1;
		els[1].values = &vals[1];
		els[1].flags = LDB_FLAG_MOD_ADD;
		els[1].name = els[0].name;

		vals[0].data = (uint8_t *)unixid_string;
		vals[0].length = strlen(unixid_string);
		vals[1].data = (uint8_t *)hwm_string;
		vals[1].length = strlen(hwm_string);
	} else {
		ret = ldb_msg_add_empty(hwm_msg, "xidNumber", LDB_FLAG_MOD_ADD,
					NULL);
		if (ret != LDB_SUCCESS) {
			status = NT_STATUS_NONE_MAPPED;
			goto failed;
		}

		ret = ldb_msg_add_string(hwm_msg, "xidNumber", hwm_string);
		if (ret != LDB_SUCCESS)
		{
			status = NT_STATUS_NONE_MAPPED;
			goto failed;
		}
	}

	ret = ldb_modify(ldb, hwm_msg);
	if (ret != LDB_SUCCESS) {
		DEBUG(1, ("Updating the xid high water mark failed: %s\n",
			  ldb_errstring(ldb)));
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	map_msg = ldb_msg_new(tmp_ctx);
	if (map_msg == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	map_msg->dn = ldb_dn_new_fmt(tmp_ctx, ldb, "CN=%s", sid_string);
	if (map_msg->dn == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	ret = ldb_msg_add_string(map_msg, "xidNumber", unixid_string);
	if (ret != LDB_SUCCESS) {
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	ret = idmap_msg_add_dom_sid(idmap_ctx, tmp_ctx, map_msg, "objectSid",
			sid);
	if (ret != LDB_SUCCESS) {
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	ret = ldb_msg_add_string(map_msg, "objectClass", "sidMap");
	if (ret != LDB_SUCCESS) {
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	ret = ldb_msg_add_string(map_msg, "type", "ID_TYPE_BOTH");
	if (ret != LDB_SUCCESS) {
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	ret = ldb_msg_add_string(map_msg, "cn", sid_string);
	if (ret != LDB_SUCCESS) {
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	ret = ldb_add(ldb, map_msg);
	if (ret != LDB_SUCCESS) {
		DEBUG(1, ("Adding a sidmap failed: %s\n", ldb_errstring(ldb)));
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	trans = ldb_transaction_commit(ldb);
	if (trans != LDB_SUCCESS) {
		DEBUG(1, ("Transaction failed: %s\n", ldb_errstring(ldb)));
		status = NT_STATUS_NONE_MAPPED;
		goto failed;
	}

	unixid->id = new_xid;
	unixid->type = ID_TYPE_BOTH;
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;

failed:
	if (trans == LDB_SUCCESS) ldb_transaction_cancel(ldb);
	talloc_free(tmp_ctx);
	return status;
}

/**
 * Convert an array of unixids to the corresponding array of SIDs
 *
 * \param idmap_ctx idmap context to use
 * \param mem_ctx talloc context the memory for the dom_sids is allocated
 * from.
 * \param count length of id_mapping array.
 * \param id array of id_mappings.
 * \return NT_STATUS_OK on success, NT_STATUS_NONE_MAPPED if mapping is not
 * possible at all, NT_STATUS_SOME_UNMAPPED if some mappings worked and some
 * did not.
 */

NTSTATUS idmap_xids_to_sids(struct idmap_context *idmap_ctx,
			    TALLOC_CTX *mem_ctx,
			    struct id_map **id)
{
	unsigned int i, error_count = 0;
	NTSTATUS status;

	for (i = 0; id && id[i]; i++) {
		status = idmap_xid_to_sid(idmap_ctx, mem_ctx,
						&id[i]->xid, &id[i]->sid);
		if (NT_STATUS_EQUAL(status, NT_STATUS_RETRY)) {
			status = idmap_xid_to_sid(idmap_ctx, mem_ctx,
							&id[i]->xid,
							&id[i]->sid);
		}
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("idmapping xid_to_sid failed for id[%d]=%lu: %s\n",
				  i, (unsigned long)id[i]->xid.id, nt_errstr(status)));
			error_count++;
			id[i]->status = ID_UNMAPPED;
		} else {
			id[i]->status = ID_MAPPED;
		}
	}

	if (error_count == i) {
		/* Mapping did not work at all. */
		return NT_STATUS_NONE_MAPPED;
	} else if (error_count > 0) {
		/* Some mappings worked, some did not. */
		return STATUS_SOME_UNMAPPED;
	} else {
		return NT_STATUS_OK;
	}
}

/**
 * Convert an array of SIDs to the corresponding array of unixids
 *
 * \param idmap_ctx idmap context to use
 * \param mem_ctx talloc context the memory for the unixids is allocated
 * from.
 * \param count length of id_mapping array.
 * \param id array of id_mappings.
 * \return NT_STATUS_OK on success, NT_STATUS_NONE_MAPPED if mapping is not
 * possible at all, NT_STATUS_SOME_UNMAPPED if some mappings worked and some
 * did not.
 */

NTSTATUS idmap_sids_to_xids(struct idmap_context *idmap_ctx,
			    TALLOC_CTX *mem_ctx,
			    struct id_map **id)
{
	unsigned int i, error_count = 0;
	NTSTATUS status;

	for (i = 0; id && id[i]; i++) {
		status = idmap_sid_to_xid(idmap_ctx, mem_ctx,
					  id[i]->sid, &id[i]->xid);
		if (NT_STATUS_EQUAL(status, NT_STATUS_RETRY)) {
			status = idmap_sid_to_xid(idmap_ctx, mem_ctx,
						  id[i]->sid,
						  &id[i]->xid);
		}
		if (!NT_STATUS_IS_OK(status)) {
			char *str = dom_sid_string(mem_ctx, id[i]->sid);
			DEBUG(1, ("idmapping sid_to_xid failed for id[%d]=%s: %s\n",
				  i, str, nt_errstr(status)));
			talloc_free(str);
			error_count++;
			id[i]->status = ID_UNMAPPED;
		} else {
			id[i]->status = ID_MAPPED;
		}
	}

	if (error_count == i) {
		/* Mapping did not work at all. */
		return NT_STATUS_NONE_MAPPED;
	} else if (error_count > 0) {
		/* Some mappings worked, some did not. */
		return STATUS_SOME_UNMAPPED;
	} else {
		return NT_STATUS_OK;
	}
}

