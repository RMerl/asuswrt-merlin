/*
   Unix SMB/CIFS implementation.

   Copyright (C) Guenther Deschner <gd@samba.org> 2008

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
#include "system/passwd.h"
#include "libnet/libnet_dssync.h"
#include "../libcli/security/security.h"
#include "../libds/common/flags.h"
#include "../librpc/gen_ndr/ndr_drsuapi.h"
#include "util_tdb.h"
#include "dbwrap.h"
#include "../libds/common/flag_mapping.h"
#include "passdb.h"

/****************************************************************
****************************************************************/

struct dssync_passdb {
	struct pdb_methods *methods;
	struct db_context *all;
	struct db_context *aliases;
	struct db_context *groups;
};

struct dssync_passdb_obj {
	struct dssync_passdb_obj *self;
	uint32_t type;
	struct drsuapi_DsReplicaObjectListItemEx *cur;
	TDB_DATA key;
	TDB_DATA data;
	struct db_context *members;
};

struct dssync_passdb_mem {
	struct dssync_passdb_mem *self;
	bool active;
	struct drsuapi_DsReplicaObjectIdentifier3 *cur;
	struct dssync_passdb_obj *obj;
	TDB_DATA key;
	TDB_DATA data;
};

static NTSTATUS dssync_insert_obj(struct dssync_passdb *pctx,
				  struct db_context *db,
				  struct dssync_passdb_obj *obj)
{
	NTSTATUS status;
	struct db_record *rec;

	rec = db->fetch_locked(db, talloc_tos(), obj->key);
	if (rec == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	if (rec->value.dsize != 0) {
		abort();
	}

	status = rec->store(rec, obj->data, TDB_INSERT);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(rec);
		return status;
	}
	TALLOC_FREE(rec);
	return NT_STATUS_OK;
}

static struct dssync_passdb_obj *dssync_parse_obj(const TDB_DATA data)
{
	struct dssync_passdb_obj *obj;

	if (data.dsize != sizeof(obj)) {
		return NULL;
	}

	/*
	 * we need to copy the pointer to avoid alignment problems
	 * on some systems.
	 */
	memcpy(&obj, data.dptr, sizeof(obj));

	return talloc_get_type_abort(obj, struct dssync_passdb_obj);
}

static struct dssync_passdb_obj *dssync_search_obj_by_guid(struct dssync_passdb *pctx,
							   struct db_context *db,
							   const struct GUID *guid)
{
	struct dssync_passdb_obj *obj;
	int ret;
	TDB_DATA key;
	TDB_DATA data;

	key = make_tdb_data((const uint8_t *)(void *)guid,
			     sizeof(*guid));

	ret = db->fetch(db, talloc_tos(), key, &data);
	if (ret != 0) {
		return NULL;
	}

	obj = dssync_parse_obj(data);
	return obj;
}

static NTSTATUS dssync_create_obj(struct dssync_passdb *pctx,
				  struct db_context *db,
				  uint32_t type,
				  struct drsuapi_DsReplicaObjectListItemEx *cur,
				  struct dssync_passdb_obj **_obj)
{
	NTSTATUS status;
	struct dssync_passdb_obj *obj;

	obj = talloc_zero(pctx, struct dssync_passdb_obj);
	if (obj == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	obj->self = obj;
	obj->cur = cur;
	obj->type = type;
	obj->key = make_tdb_data((const uint8_t *)(void *)&cur->object.identifier->guid,
				   sizeof(cur->object.identifier->guid));
	obj->data = make_tdb_data((const uint8_t *)(void *)&obj->self,
				  sizeof(obj->self));

	obj->members = db_open_rbt(obj);
	if (obj->members == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = dssync_insert_obj(pctx, db, obj);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(obj);
		return status;
	}
	*_obj = obj;
	return NT_STATUS_OK;
}

static NTSTATUS dssync_insert_mem(struct dssync_passdb *pctx,
				  struct dssync_passdb_obj *obj,
				  struct dssync_passdb_mem *mem)
{
	NTSTATUS status;
	struct db_record *rec;

	rec = obj->members->fetch_locked(obj->members, talloc_tos(), mem->key);
	if (rec == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	if (rec->value.dsize != 0) {
		abort();
	}

	status = rec->store(rec, mem->data, TDB_INSERT);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(rec);
		return status;
	}
	TALLOC_FREE(rec);
	return NT_STATUS_OK;
}

static NTSTATUS dssync_create_mem(struct dssync_passdb *pctx,
				  struct dssync_passdb_obj *obj,
				  bool active,
				  struct drsuapi_DsReplicaObjectIdentifier3 *cur,
				  struct dssync_passdb_mem **_mem)
{
	NTSTATUS status;
	struct dssync_passdb_mem *mem;

	mem = talloc_zero(pctx, struct dssync_passdb_mem);
	if (mem == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	mem->self = mem;
	mem->cur = cur;
	mem->active = active;
	mem->obj = NULL;
	mem->key = make_tdb_data((const uint8_t *)(void *)&cur->guid,
				   sizeof(cur->guid));
	mem->data = make_tdb_data((const uint8_t *)(void *)&mem->self,
				  sizeof(mem->self));

	status = dssync_insert_mem(pctx, obj, mem);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(obj);
		return status;
	}
	*_mem = mem;
	return NT_STATUS_OK;
}

static struct dssync_passdb_mem *dssync_parse_mem(const TDB_DATA data)
{
	struct dssync_passdb_mem *mem;

	if (data.dsize != sizeof(mem)) {
		return NULL;
	}

	/*
	 * we need to copy the pointer to avoid alignment problems
	 * on some systems.
	 */
	memcpy(&mem, data.dptr, sizeof(mem));

	return talloc_get_type_abort(mem, struct dssync_passdb_mem);
}

static NTSTATUS passdb_startup(struct dssync_context *ctx, TALLOC_CTX *mem_ctx,
			       struct replUpToDateVectorBlob **pold_utdv)
{
	NTSTATUS status;
	struct dssync_passdb *pctx;

	pctx = talloc_zero(mem_ctx, struct dssync_passdb);
	if (pctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (ctx->output_filename) {
		status = make_pdb_method_name(&pctx->methods, ctx->output_filename);
	} else {
		status = make_pdb_method_name(&pctx->methods, lp_passdb_backend());
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	pctx->all = db_open_rbt(pctx);
	if (pctx->all == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	pctx->aliases = db_open_rbt(pctx);
	if (pctx->aliases == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	pctx->groups = db_open_rbt(pctx);
	if (pctx->groups == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ctx->private_data = pctx;

	return status;
}

/****************************************************************
****************************************************************/

struct dssync_passdb_traverse_amembers {
	struct dssync_context *ctx;
	struct dssync_passdb_obj *obj;
	const char *name;
	uint32_t idx;
};

struct dssync_passdb_traverse_aliases {
	struct dssync_context *ctx;
	const char *name;
	uint32_t idx;
};

static int dssync_passdb_traverse_amembers(struct db_record *rec,
					   void *private_data)
{
	struct dssync_passdb_traverse_amembers *state =
		(struct dssync_passdb_traverse_amembers *)private_data;
	struct dssync_passdb *pctx =
		talloc_get_type_abort(state->ctx->private_data,
		struct dssync_passdb);
	struct dssync_passdb_mem *mem;
	NTSTATUS status;
	struct dom_sid alias_sid;
	struct dom_sid member_sid;
	const char *member_dn;
	size_t num_members;
	size_t i;
	struct dom_sid *members;
	bool is_member = false;
	const char *action;

	state->idx++;

	alias_sid = state->obj->cur->object.identifier->sid;

	mem = dssync_parse_mem(rec->value);
	if (mem == NULL) {
		return -1;
	}

	member_sid = mem->cur->sid;
	member_dn = mem->cur->dn;

	mem->obj = dssync_search_obj_by_guid(pctx, pctx->all, &mem->cur->guid);
	if (mem->obj == NULL) {
		DEBUG(0,("alias[%s] member[%s] can't resolve member - ignoring\n",
			 sid_string_dbg(&alias_sid),
			 is_null_sid(&member_sid)?
			 sid_string_dbg(&member_sid):
			 member_dn));
		return 0;
	}

	switch (mem->obj->type) {
	case ATYPE_DISTRIBUTION_LOCAL_GROUP:
	case ATYPE_DISTRIBUTION_GLOBAL_GROUP:
		DEBUG(0, ("alias[%s] ignore distribution group [%s]\n",
			  sid_string_dbg(&alias_sid),
			  member_dn));
		return 0;
	default:
		break;
	}

	DEBUG(0,("alias[%s] member[%s]\n",
		 sid_string_dbg(&alias_sid),
		 sid_string_dbg(&member_sid)));

	status = pdb_enum_aliasmem(&alias_sid, talloc_tos(),
				   &members, &num_members);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Could not find current alias members %s - %s\n",
			  sid_string_dbg(&alias_sid),
			  nt_errstr(status)));
		return -1;
	}

	for (i=0; i < num_members; i++) {
		bool match;

		match = dom_sid_equal(&members[i], &member_sid);
		if (match) {
			is_member = true;
			break;
		}
	}

	status = NT_STATUS_OK;
	action = "none";
	if (!is_member && mem->active) {
		action = "add";
		pdb_add_aliasmem(&alias_sid, &member_sid);
	} else if (is_member && !mem->active) {
		action = "delete";
		pdb_del_aliasmem(&alias_sid, &member_sid);
	}
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Could not %s %s as alias members of %s - %s\n",
			  action,
			  sid_string_dbg(&member_sid),
			  sid_string_dbg(&alias_sid),
			  nt_errstr(status)));
		return -1;
	}

	return 0;
}

static int dssync_passdb_traverse_aliases(struct db_record *rec,
					  void *private_data)
{
	struct dssync_passdb_traverse_aliases *state =
		(struct dssync_passdb_traverse_aliases *)private_data;
	struct dssync_passdb *pctx =
		talloc_get_type_abort(state->ctx->private_data,
		struct dssync_passdb);
	struct dssync_passdb_traverse_amembers mstate;
	struct dssync_passdb_obj *obj;
	int ret;

	state->idx++;
	if (pctx->methods == NULL) {
		return -1;
	}

	obj = dssync_parse_obj(rec->value);
	if (obj == NULL) {
		return -1;
	}

	ZERO_STRUCT(mstate);
	mstate.ctx = state->ctx;
	mstate.name = "members";
	mstate.obj = obj;
	ret = obj->members->traverse_read(obj->members,
					  dssync_passdb_traverse_amembers,
					  &mstate);
	if (ret < 0) {
		return -1;
	}

	return 0;
}

struct dssync_passdb_traverse_gmembers {
	struct dssync_context *ctx;
	struct dssync_passdb_obj *obj;
	const char *name;
	uint32_t idx;
};

struct dssync_passdb_traverse_groups {
	struct dssync_context *ctx;
	const char *name;
	uint32_t idx;
};

static int dssync_passdb_traverse_gmembers(struct db_record *rec,
					   void *private_data)
{
	struct dssync_passdb_traverse_gmembers *state =
		(struct dssync_passdb_traverse_gmembers *)private_data;
	struct dssync_passdb *pctx =
		talloc_get_type_abort(state->ctx->private_data,
		struct dssync_passdb);
	struct dssync_passdb_mem *mem;
	NTSTATUS status;
	char *nt_member = NULL;
	char **unix_members;
	struct dom_sid group_sid;
	struct dom_sid member_sid;
	struct samu *member = NULL;
	const char *member_dn = NULL;
	GROUP_MAP map;
	struct group *grp;
	uint32_t rid;
	bool is_unix_member = false;

	state->idx++;

	group_sid = state->obj->cur->object.identifier->sid;

	status = dom_sid_split_rid(talloc_tos(), &group_sid, NULL, &rid);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	mem = dssync_parse_mem(rec->value);
	if (mem == NULL) {
		return -1;
	}

	member_sid = mem->cur->sid;
	member_dn = mem->cur->dn;

	mem->obj = dssync_search_obj_by_guid(pctx, pctx->all, &mem->cur->guid);
	if (mem->obj == NULL) {
		DEBUG(0,("group[%s] member[%s] can't resolve member - ignoring\n",
			 sid_string_dbg(&group_sid),
			 is_null_sid(&member_sid)?
			 sid_string_dbg(&member_sid):
			 member_dn));
		return 0;
	}

	member_sid = mem->obj->cur->object.identifier->sid;
	member_dn = mem->obj->cur->object.identifier->dn;

	switch (mem->obj->type) {
	case ATYPE_SECURITY_LOCAL_GROUP:
	case ATYPE_SECURITY_GLOBAL_GROUP:
		DEBUG(0, ("Group[%s] ignore member group [%s]\n",
			  sid_string_dbg(&group_sid),
			  sid_string_dbg(&member_sid)));
		return 0;

	case ATYPE_DISTRIBUTION_LOCAL_GROUP:
	case ATYPE_DISTRIBUTION_GLOBAL_GROUP:
		DEBUG(0, ("Group[%s] ignore distribution group [%s]\n",
			  sid_string_dbg(&group_sid),
			  member_dn));
		return 0;
	default:
		break;
	}

	if (!get_domain_group_from_sid(group_sid, &map)) {
		DEBUG(0, ("Could not find global group %s\n",
			  sid_string_dbg(&group_sid)));
		//return NT_STATUS_NO_SUCH_GROUP;
		return -1;
	}

	if (!(grp = getgrgid(map.gid))) {
		DEBUG(0, ("Could not find unix group %lu\n", (unsigned long)map.gid));
		//return NT_STATUS_NO_SUCH_GROUP;
		return -1;
	}

	DEBUG(0,("Group members of %s: ", grp->gr_name));

	if ( !(member = samu_new(talloc_tos())) ) {
		//return NT_STATUS_NO_MEMORY;
		return -1;
	}

	if (!pdb_getsampwsid(member, &member_sid)) {
		DEBUG(1, ("Found bogus group member: (member_sid=%s group=%s)\n",
			  sid_string_tos(&member_sid), grp->gr_name));
		TALLOC_FREE(member);
		return -1;
	}

	if (pdb_get_group_rid(member) == rid) {
		DEBUGADD(0,("%s(primary),", pdb_get_username(member)));
		TALLOC_FREE(member);
		return -1;
	}

	DEBUGADD(0,("%s,", pdb_get_username(member)));
	nt_member = talloc_strdup(talloc_tos(), pdb_get_username(member));
	TALLOC_FREE(member);

	DEBUGADD(0,("\n"));

	unix_members = grp->gr_mem;

	while (*unix_members) {
		if (strcmp(*unix_members, nt_member) == 0) {
			is_unix_member = true;
			break;
		}
		unix_members += 1;
	}

	if (!is_unix_member && mem->active) {
		smb_add_user_group(grp->gr_name, nt_member);
	} else if (is_unix_member && !mem->active) {
		smb_delete_user_group(grp->gr_name, nt_member);
	}

	return 0;
}

static int dssync_passdb_traverse_groups(struct db_record *rec,
					 void *private_data)
{
	struct dssync_passdb_traverse_groups *state =
		(struct dssync_passdb_traverse_groups *)private_data;
	struct dssync_passdb *pctx =
		talloc_get_type_abort(state->ctx->private_data,
		struct dssync_passdb);
	struct dssync_passdb_traverse_gmembers mstate;
	struct dssync_passdb_obj *obj;
	int ret;

	state->idx++;
	if (pctx->methods == NULL) {
		return -1;
	}

	obj = dssync_parse_obj(rec->value);
	if (obj == NULL) {
		return -1;
	}

	ZERO_STRUCT(mstate);
	mstate.ctx = state->ctx;
	mstate.name = "members";
	mstate.obj = obj;
	ret = obj->members->traverse_read(obj->members,
					  dssync_passdb_traverse_gmembers,
					  &mstate);
	if (ret < 0) {
		return -1;
	}

	return 0;
}

static NTSTATUS passdb_finish(struct dssync_context *ctx, TALLOC_CTX *mem_ctx,
			      struct replUpToDateVectorBlob *new_utdv)
{
	struct dssync_passdb *pctx =
		talloc_get_type_abort(ctx->private_data,
		struct dssync_passdb);
	struct dssync_passdb_traverse_aliases astate;
	struct dssync_passdb_traverse_groups gstate;
	int ret;

	ZERO_STRUCT(astate);
	astate.ctx = ctx;
	astate.name = "aliases";
	ret = pctx->aliases->traverse_read(pctx->aliases,
					   dssync_passdb_traverse_aliases,
					   &astate);
	if (ret < 0) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	ZERO_STRUCT(gstate);
	gstate.ctx = ctx;
	gstate.name = "groups";
	ret = pctx->groups->traverse_read(pctx->groups,
					  dssync_passdb_traverse_groups,
					  &gstate);
	if (ret < 0) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	TALLOC_FREE(pctx->methods);
	TALLOC_FREE(pctx);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS smb_create_user(TALLOC_CTX *mem_ctx,
				uint32_t acct_flags,
				const char *account,
				struct passwd **passwd_p)
{
	struct passwd *passwd;
	char *add_script = NULL;

	passwd = Get_Pwnam_alloc(mem_ctx, account);
	if (passwd) {
		*passwd_p = passwd;
		return NT_STATUS_OK;
	}

	/* Create appropriate user */
	if (acct_flags & ACB_NORMAL) {
		add_script = talloc_strdup(mem_ctx, lp_adduser_script());
	} else if ( (acct_flags & ACB_WSTRUST) ||
		    (acct_flags & ACB_SVRTRUST) ||
		    (acct_flags & ACB_DOMTRUST) ) {
		add_script = talloc_strdup(mem_ctx, lp_addmachine_script());
	} else {
		DEBUG(1, ("Unknown user type: %s\n",
			  pdb_encode_acct_ctrl(acct_flags, NEW_PW_FORMAT_SPACE_PADDED_LEN)));
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!add_script) {
		return NT_STATUS_NO_MEMORY;
	}

	if (*add_script) {
		int add_ret;
		add_script = talloc_all_string_sub(mem_ctx, add_script,
						   "%u", account);
		if (!add_script) {
			return NT_STATUS_NO_MEMORY;
		}
		add_ret = smbrun(add_script, NULL);
		DEBUG(add_ret ? 0 : 1,("fetch_account: Running the command `%s' "
			 "gave %d\n", add_script, add_ret));
		if (add_ret == 0) {
			smb_nscd_flush_user_cache();
		}
	}

	/* try and find the possible unix account again */
	passwd = Get_Pwnam_alloc(mem_ctx, account);
	if (!passwd) {
		return NT_STATUS_NO_SUCH_USER;
	}

	*passwd_p = passwd;

	return NT_STATUS_OK;
}

static struct drsuapi_DsReplicaAttribute *find_drsuapi_attr(
			const struct drsuapi_DsReplicaObjectListItemEx *cur,
			uint32_t attid)
{
	int i = 0;

	for (i = 0; i < cur->object.attribute_ctr.num_attributes; i++) {
		struct drsuapi_DsReplicaAttribute *attr;

		attr = &cur->object.attribute_ctr.attributes[i];

		if (attr->attid == attid) {
			return attr;
		}
	}

	return NULL;
}

static NTSTATUS find_drsuapi_attr_string(TALLOC_CTX *mem_ctx,
					 const struct drsuapi_DsReplicaObjectListItemEx *cur,
					 uint32_t attid,
					 uint32_t *_count,
					 char ***_array)
{
	struct drsuapi_DsReplicaAttribute *attr;
	char **array;
	uint32_t a;

	attr = find_drsuapi_attr(cur, attid);
	if (attr == NULL) {
		return NT_STATUS_PROPSET_NOT_FOUND;
	}

	array = talloc_array(mem_ctx, char *, attr->value_ctr.num_values);
	if (array == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (a = 0; a < attr->value_ctr.num_values; a++) {
		const DATA_BLOB *blob;
		ssize_t ret;

		blob = attr->value_ctr.values[a].blob;

		if (blob == NULL) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		ret = pull_string_talloc(array, NULL, 0, &array[a],
					 blob->data, blob->length,
					 STR_UNICODE);
		if (ret == -1) {
			//TODO
			return NT_STATUS_INTERNAL_ERROR;
		}
	}

	*_count = attr->value_ctr.num_values;
	*_array = array;
	return NT_STATUS_OK;
}

static NTSTATUS find_drsuapi_attr_int32(TALLOC_CTX *mem_ctx,
					const struct drsuapi_DsReplicaObjectListItemEx *cur,
					uint32_t attid,
					uint32_t *_count,
					int32_t **_array)
{
	struct drsuapi_DsReplicaAttribute *attr;
	int32_t *array;
	uint32_t a;

	attr = find_drsuapi_attr(cur, attid);
	if (attr == NULL) {
		return NT_STATUS_PROPSET_NOT_FOUND;
	}

	array = talloc_array(mem_ctx, int32_t, attr->value_ctr.num_values);
	if (array == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (a = 0; a < attr->value_ctr.num_values; a++) {
		const DATA_BLOB *blob;

		blob = attr->value_ctr.values[a].blob;

		if (blob == NULL) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (blob->length != 4) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		array[a] = IVAL(blob->data, 0);
	}

	*_count = attr->value_ctr.num_values;
	*_array = array;
	return NT_STATUS_OK;
}

static NTSTATUS find_drsuapi_attr_blob(TALLOC_CTX *mem_ctx,
				       const struct drsuapi_DsReplicaObjectListItemEx *cur,
				       uint32_t attid,
				       uint32_t *_count,
				       DATA_BLOB **_array)
{
	struct drsuapi_DsReplicaAttribute *attr;
	DATA_BLOB *array;
	uint32_t a;

	attr = find_drsuapi_attr(cur, attid);
	if (attr == NULL) {
		return NT_STATUS_PROPSET_NOT_FOUND;
	}

	array = talloc_array(mem_ctx, DATA_BLOB, attr->value_ctr.num_values);
	if (array == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (a = 0; a < attr->value_ctr.num_values; a++) {
		const DATA_BLOB *blob;

		blob = attr->value_ctr.values[a].blob;

		if (blob == NULL) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		array[a] = data_blob_talloc(array, blob->data, blob->length);
		if (array[a].length != blob->length) {
			return NT_STATUS_NO_MEMORY;
		}
	}
	*_count = attr->value_ctr.num_values;
	*_array = array;
	return NT_STATUS_OK;
}

static NTSTATUS find_drsuapi_attr_int64(TALLOC_CTX *mem_ctx,
					const struct drsuapi_DsReplicaObjectListItemEx *cur,
					uint32_t attid,
					uint32_t *_count,
					int64_t **_array)
{
	struct drsuapi_DsReplicaAttribute *attr;
	int64_t *array;
	uint32_t a;

	attr = find_drsuapi_attr(cur, attid);
	if (attr == NULL) {
		return NT_STATUS_PROPSET_NOT_FOUND;
	}

	array = talloc_array(mem_ctx, int64_t, attr->value_ctr.num_values);
	if (array == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (a = 0; a < attr->value_ctr.num_values; a++) {
		const DATA_BLOB *blob;

		blob = attr->value_ctr.values[a].blob;

		if (blob == NULL) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (blob->length != 8) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		array[a] = BVAL(blob->data, 0);
	}
	*_count = attr->value_ctr.num_values;
	*_array = array;
	return NT_STATUS_OK;
}

static NTSTATUS find_drsuapi_attr_dn(TALLOC_CTX *mem_ctx,
				     const struct drsuapi_DsReplicaObjectListItemEx *cur,
				     uint32_t attid,
				     uint32_t *_count,
				     struct drsuapi_DsReplicaObjectIdentifier3 **_array)
{
	struct drsuapi_DsReplicaAttribute *attr;
	struct drsuapi_DsReplicaObjectIdentifier3 *array;
	uint32_t a;

	attr = find_drsuapi_attr(cur, attid);
	if (attr == NULL) {
		return NT_STATUS_PROPSET_NOT_FOUND;
	}

	array = talloc_array(mem_ctx,
			     struct drsuapi_DsReplicaObjectIdentifier3,
			     attr->value_ctr.num_values);
	if (array == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (a = 0; a < attr->value_ctr.num_values; a++) {
		const DATA_BLOB *blob;
		enum ndr_err_code ndr_err;
		NTSTATUS status;

		blob = attr->value_ctr.values[a].blob;

		if (blob == NULL) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		/* windows sometimes sends an extra two pad bytes here */
		ndr_err = ndr_pull_struct_blob(blob, array, &array[a],
				(ndr_pull_flags_fn_t)ndr_pull_drsuapi_DsReplicaObjectIdentifier3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
			return status;
		}
	}
	*_count = attr->value_ctr.num_values;
	*_array = array;
	return NT_STATUS_OK;
}

#define GET_BLOB_EX(attr, needed) do { \
	NTSTATUS _status; \
	uint32_t _cnt; \
	DATA_BLOB *_vals = NULL; \
	attr = data_blob_null; \
	_status = find_drsuapi_attr_blob(mem_ctx, cur, \
					 DRSUAPI_ATTID_ ## attr, \
					 &_cnt, &_vals); \
	if (NT_STATUS_EQUAL(_status, NT_STATUS_PROPSET_NOT_FOUND)) { \
		if (!needed) { \
			_status = NT_STATUS_OK; \
			_cnt = 0; \
		} \
	} \
	if (!NT_STATUS_IS_OK(_status)) { \
		DEBUG(0,(__location__ "attr[%s] %s\n", \
			#attr, nt_errstr(_status))); \
		return _status; \
	} \
	if (_cnt == 0) { \
		if (needed) { \
			talloc_free(_vals); \
			DEBUG(0,(__location__ "attr[%s] count[%u]\n", #attr, _cnt)); \
			return NT_STATUS_OBJECT_NAME_NOT_FOUND; \
		} \
	} else if (_cnt > 1) { \
		talloc_free(_vals); \
		DEBUG(0,(__location__ "attr[%s] count[%u]\n", #attr, _cnt)); \
		return NT_STATUS_INTERNAL_DB_CORRUPTION; \
	} else { \
		attr = _vals[0]; \
		(void)talloc_steal(mem_ctx, _vals[0].data); \
	} \
	talloc_free(_vals); \
} while(0)

#define GET_STRING_EX(attr, needed) do { \
	NTSTATUS _status; \
	uint32_t _cnt; \
	char **_vals = NULL; \
	attr = NULL; \
	_status = find_drsuapi_attr_string(mem_ctx, cur, \
					   DRSUAPI_ATTID_ ## attr, \
					   &_cnt, &_vals); \
	if (NT_STATUS_EQUAL(_status, NT_STATUS_PROPSET_NOT_FOUND)) { \
		if (!needed) { \
			_status = NT_STATUS_OK; \
			_cnt = 0; \
		} \
	} \
	if (!NT_STATUS_IS_OK(_status)) { \
		DEBUG(0,(__location__ "attr[%s] %s\n", \
			#attr, nt_errstr(_status))); \
		return _status; \
	} \
	if (_cnt == 0) { \
		if (needed) { \
			talloc_free(_vals); \
			DEBUG(0,(__location__ "attr[%s] count[%u]\n", #attr, _cnt)); \
			return NT_STATUS_OBJECT_NAME_NOT_FOUND; \
		} \
	} else if (_cnt > 1) { \
		talloc_free(_vals); \
		DEBUG(0,(__location__ "attr[%s] count[%u]\n", #attr, _cnt)); \
		return NT_STATUS_INTERNAL_DB_CORRUPTION; \
	} else { \
		attr = talloc_move(mem_ctx, &_vals[0]); \
	} \
	talloc_free(_vals); \
} while(0)

#define GET_UINT32_EX(attr, needed) do { \
	NTSTATUS _status; \
	uint32_t _cnt; \
	int32_t*_vals = NULL; \
	attr = 0; \
	_status = find_drsuapi_attr_int32(mem_ctx, cur, \
					  DRSUAPI_ATTID_ ## attr, \
					  &_cnt, &_vals); \
	if (NT_STATUS_EQUAL(_status, NT_STATUS_PROPSET_NOT_FOUND)) { \
		if (!needed) { \
			_status = NT_STATUS_OK; \
			_cnt = 0; \
		} \
	} \
	if (!NT_STATUS_IS_OK(_status)) { \
		DEBUG(0,(__location__ "attr[%s] %s\n", \
			#attr, nt_errstr(_status))); \
		return _status; \
	} \
	if (_cnt == 0) { \
		if (needed) { \
			talloc_free(_vals); \
			DEBUG(0,(__location__ "attr[%s] count[%u]\n", #attr, _cnt)); \
			return NT_STATUS_OBJECT_NAME_NOT_FOUND; \
		} \
	} else if (_cnt > 1) { \
		talloc_free(_vals); \
		DEBUG(0,(__location__ "attr[%s] count[%u]\n", #attr, _cnt)); \
		return NT_STATUS_INTERNAL_DB_CORRUPTION; \
	} else { \
		attr = (uint32_t)_vals[0]; \
	} \
	talloc_free(_vals); \
} while(0)

#define GET_UINT64_EX(attr, needed) do { \
	NTSTATUS _status; \
	uint32_t _cnt; \
	int64_t *_vals = NULL; \
	attr = 0; \
	_status = find_drsuapi_attr_int64(mem_ctx, cur, \
					  DRSUAPI_ATTID_ ## attr, \
					  &_cnt, &_vals); \
	if (NT_STATUS_EQUAL(_status, NT_STATUS_PROPSET_NOT_FOUND)) { \
		if (!needed) { \
			_status = NT_STATUS_OK; \
			_cnt = 0; \
		} \
	} \
	if (!NT_STATUS_IS_OK(_status)) { \
		DEBUG(0,(__location__ "attr[%s] %s\n", \
			#attr, nt_errstr(_status))); \
		return _status; \
	} \
	if (_cnt == 0) { \
		if (needed) { \
			talloc_free(_vals); \
			DEBUG(0,(__location__ "attr[%s] count[%u]\n", #attr, _cnt)); \
			return NT_STATUS_OBJECT_NAME_NOT_FOUND; \
		} \
	} else if (_cnt > 1) { \
		talloc_free(_vals); \
		DEBUG(0,(__location__ "attr[%s] count[%u]\n", #attr, _cnt)); \
		return NT_STATUS_INTERNAL_DB_CORRUPTION; \
	} else { \
		attr = (uint64_t)_vals[0]; \
	} \
	talloc_free(_vals); \
} while(0)

#define GET_BLOB(attr) GET_BLOB_EX(attr, false)
#define GET_STRING(attr) GET_STRING_EX(attr, false)
#define GET_UINT32(attr) GET_UINT32_EX(attr, false)
#define GET_UINT64(attr) GET_UINT64_EX(attr, false)

/* Convert a struct samu_DELTA to a struct samu. */
#define STRING_CHANGED (old_string && !new_string) ||\
		    (!old_string && new_string) ||\
		(old_string && new_string && (strcmp(old_string, new_string) != 0))

#define STRING_CHANGED_NC(s1,s2) ((s1) && !(s2)) ||\
		    (!(s1) && (s2)) ||\
		((s1) && (s2) && (strcmp((s1), (s2)) != 0))

/****************************************************************
****************************************************************/

static NTSTATUS sam_account_from_object(struct samu *account,
				struct drsuapi_DsReplicaObjectListItemEx *cur)
{
	TALLOC_CTX *mem_ctx = account;
	const char *old_string, *new_string;
	time_t unix_time, stored_time;
	uchar zero_buf[16];
	NTSTATUS status;

	NTTIME lastLogon;
	NTTIME lastLogoff;
	NTTIME pwdLastSet;
	NTTIME accountExpires;
	const char *sAMAccountName;
	const char *displayName;
	const char *homeDirectory;
	const char *homeDrive;
	const char *scriptPath;
	const char *profilePath;
	const char *description;
	const char *userWorkstations;
	const char *comment;
	DATA_BLOB userParameters;
	struct dom_sid objectSid;
	uint32_t primaryGroupID;
	uint32_t userAccountControl;
	DATA_BLOB logonHours;
	uint32_t badPwdCount;
	uint32_t logonCount;
	DATA_BLOB unicodePwd;
	DATA_BLOB dBCSPwd;

	uint32_t rid = 0;
	uint32_t acct_flags;
	uint32_t units_per_week;

	memset(zero_buf, '\0', sizeof(zero_buf));

	objectSid = cur->object.identifier->sid;
	GET_STRING_EX(sAMAccountName, true);
	DEBUG(0,("sam_account_from_object(%s, %s) start\n",
		 sAMAccountName, sid_string_dbg(&objectSid)));
	GET_UINT64(lastLogon);
	GET_UINT64(lastLogoff);
	GET_UINT64(pwdLastSet);
	GET_UINT64(accountExpires);
	GET_STRING(displayName);
	GET_STRING(homeDirectory);
	GET_STRING(homeDrive);
	GET_STRING(scriptPath);
	GET_STRING(profilePath);
	GET_STRING(description);
	GET_STRING(userWorkstations);
	GET_STRING(comment);
	GET_BLOB(userParameters);
	GET_UINT32(primaryGroupID);
	GET_UINT32(userAccountControl);
	GET_BLOB(logonHours);
	GET_UINT32(badPwdCount);
	GET_UINT32(logonCount);
	GET_BLOB(unicodePwd);
	GET_BLOB(dBCSPwd);

	status = dom_sid_split_rid(mem_ctx, &objectSid, NULL, &rid);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	acct_flags = ds_uf2acb(userAccountControl);

	/* Username, fullname, home dir, dir drive, logon script, acct
	   desc, workstations, profile. */

	if (sAMAccountName) {
		old_string = pdb_get_nt_username(account);
		new_string = sAMAccountName;

		if (STRING_CHANGED) {
			pdb_set_nt_username(account, new_string, PDB_CHANGED);
		}

		/* Unix username is the same - for sanity */
		old_string = pdb_get_username( account );
		if (STRING_CHANGED) {
			pdb_set_username(account, new_string, PDB_CHANGED);
		}
	}

	if (displayName) {
		old_string = pdb_get_fullname(account);
		new_string = displayName;

		if (STRING_CHANGED)
			pdb_set_fullname(account, new_string, PDB_CHANGED);
	}

	if (homeDirectory) {
		old_string = pdb_get_homedir(account);
		new_string = homeDirectory;

		if (STRING_CHANGED)
			pdb_set_homedir(account, new_string, PDB_CHANGED);
	}

	if (homeDrive) {
		old_string = pdb_get_dir_drive(account);
		new_string = homeDrive;

		if (STRING_CHANGED)
			pdb_set_dir_drive(account, new_string, PDB_CHANGED);
	}

	if (scriptPath) {
		old_string = pdb_get_logon_script(account);
		new_string = scriptPath;

		if (STRING_CHANGED)
			pdb_set_logon_script(account, new_string, PDB_CHANGED);
	}

	if (description) {
		old_string = pdb_get_acct_desc(account);
		new_string = description;

		if (STRING_CHANGED)
			pdb_set_acct_desc(account, new_string, PDB_CHANGED);
	}

	if (userWorkstations) {
		old_string = pdb_get_workstations(account);
		new_string = userWorkstations;

		if (STRING_CHANGED)
			pdb_set_workstations(account, new_string, PDB_CHANGED);
	}

	if (profilePath) {
		old_string = pdb_get_profile_path(account);
		new_string = profilePath;

		if (STRING_CHANGED)
			pdb_set_profile_path(account, new_string, PDB_CHANGED);
	}

	if (userParameters.data) {
		char *newstr;
		old_string = pdb_get_munged_dial(account);
		newstr = (userParameters.length == 0) ? NULL :
			base64_encode_data_blob(talloc_tos(), userParameters);

		if (STRING_CHANGED_NC(old_string, newstr))
			pdb_set_munged_dial(account, newstr, PDB_CHANGED);
		TALLOC_FREE(newstr);
	}

	/* User and group sid */
	if (rid != 0 && pdb_get_user_rid(account) != rid) {
		pdb_set_user_sid_from_rid(account, rid, PDB_CHANGED);
	}
	if (primaryGroupID != 0 && pdb_get_group_rid(account) != primaryGroupID) {
		pdb_set_group_sid_from_rid(account, primaryGroupID, PDB_CHANGED);
	}

	/* Logon and password information */
	if (!nt_time_is_zero(&lastLogon)) {
		unix_time = nt_time_to_unix(lastLogon);
		stored_time = pdb_get_logon_time(account);
		if (stored_time != unix_time)
			pdb_set_logon_time(account, unix_time, PDB_CHANGED);
	}

	if (!nt_time_is_zero(&lastLogoff)) {
		unix_time = nt_time_to_unix(lastLogoff);
		stored_time = pdb_get_logoff_time(account);
		if (stored_time != unix_time)
			pdb_set_logoff_time(account, unix_time,PDB_CHANGED);
	}

	/* Logon Divs */
	units_per_week = logonHours.length * 8;

	if (pdb_get_logon_divs(account) != units_per_week) {
		pdb_set_logon_divs(account, units_per_week, PDB_CHANGED);
	}

	/* Logon Hours Len */
	if (units_per_week/8 != pdb_get_hours_len(account)) {
		pdb_set_hours_len(account, units_per_week/8, PDB_CHANGED);
	}

	/* Logon Hours */
	if (logonHours.data) {
		char oldstr[44], newstr[44];
		pdb_sethexhours(oldstr, pdb_get_hours(account));
		pdb_sethexhours(newstr, logonHours.data);
		if (!strequal(oldstr, newstr)) {
			pdb_set_hours(account, logonHours.data,
				      logonHours.length, PDB_CHANGED);
		}
	}

	if (pdb_get_bad_password_count(account) != badPwdCount)
		pdb_set_bad_password_count(account, badPwdCount, PDB_CHANGED);

	if (pdb_get_logon_count(account) != logonCount)
		pdb_set_logon_count(account, logonCount, PDB_CHANGED);

	if (!nt_time_is_zero(&pwdLastSet)) {
		unix_time = nt_time_to_unix(pwdLastSet);
		stored_time = pdb_get_pass_last_set_time(account);
		if (stored_time != unix_time)
			pdb_set_pass_last_set_time(account, unix_time, PDB_CHANGED);
	} else {
		/* no last set time, make it now */
		pdb_set_pass_last_set_time(account, time(NULL), PDB_CHANGED);
	}

	if (!nt_time_is_zero(&accountExpires)) {
		unix_time = nt_time_to_unix(accountExpires);
		stored_time = pdb_get_kickoff_time(account);
		if (stored_time != unix_time)
			pdb_set_kickoff_time(account, unix_time, PDB_CHANGED);
	}

	/* Decode hashes from password hash
	   Note that win2000 may send us all zeros for the hashes if it doesn't
	   think this channel is secure enough - don't set the passwords at all
	   in that case
	*/
	if (dBCSPwd.length == 16 && memcmp(dBCSPwd.data, zero_buf, 16) != 0) {
		pdb_set_lanman_passwd(account, dBCSPwd.data, PDB_CHANGED);
	}

	if (unicodePwd.length == 16 && memcmp(unicodePwd.data, zero_buf, 16) != 0) {
		pdb_set_nt_passwd(account, unicodePwd.data, PDB_CHANGED);
	}

	/* TODO: history */

	/* TODO: account expiry time */

	pdb_set_acct_ctrl(account, acct_flags, PDB_CHANGED);

	pdb_set_domain(account, lp_workgroup(), PDB_CHANGED);

	DEBUG(0,("sam_account_from_object(%s, %s) done\n",
		 sAMAccountName, sid_string_dbg(&objectSid)));
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS handle_account_object(struct dssync_passdb *pctx,
				      TALLOC_CTX *mem_ctx,
				      struct dssync_passdb_obj *obj)
{
	struct drsuapi_DsReplicaObjectListItemEx *cur = obj->cur;
	NTSTATUS status;
	fstring account;
	struct samu *sam_account=NULL;
	GROUP_MAP map;
	struct group *grp;
	struct dom_sid user_sid;
	struct dom_sid group_sid;
	struct passwd *passwd = NULL;
	uint32_t acct_flags;
	uint32_t rid;

	const char *sAMAccountName;
	uint32_t sAMAccountType;
	uint32_t userAccountControl;

	user_sid = cur->object.identifier->sid;
	GET_STRING_EX(sAMAccountName, true);
	GET_UINT32_EX(sAMAccountType, true);
	GET_UINT32_EX(userAccountControl, true);

	status = dom_sid_split_rid(mem_ctx, &user_sid, NULL, &rid);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	fstrcpy(account, sAMAccountName);
	if (rid == DOMAIN_RID_GUEST) {
		/*
		 * pdb_getsampwsid() has special handling for DOMAIN_RID_GUEST
		 * that's why we need to ignore it here.
		 *
		 * pdb_smbpasswd.c also has some DOMAIN_RID_GUEST related
		 * code...
		 */
		DEBUG(0,("Ignore %s - %s\n", account, sid_string_dbg(&user_sid)));
		return NT_STATUS_OK;
	}
	DEBUG(0,("Creating account: %s\n", account));

	if ( !(sam_account = samu_new(mem_ctx)) ) {
		return NT_STATUS_NO_MEMORY;
	}

	acct_flags = ds_uf2acb(userAccountControl);
	status = smb_create_user(sam_account, acct_flags, account, &passwd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Could not create posix account info for '%s'- %s\n",
			account, nt_errstr(status)));
		TALLOC_FREE(sam_account);
		return status;
	}

	DEBUG(3, ("Attempting to find SID %s for user %s in the passdb\n",
		  sid_string_dbg(&user_sid), account));
	if (!pdb_getsampwsid(sam_account, &user_sid)) {
		sam_account_from_object(sam_account, cur);
		DEBUG(3, ("Attempting to add user SID %s for user %s in the passdb\n",
			  sid_string_dbg(&user_sid),
			  pdb_get_username(sam_account)));
		if (!NT_STATUS_IS_OK(pdb_add_sam_account(sam_account))) {
			DEBUG(1, ("SAM Account for %s failed to be added to the passdb!\n",
				  account));
			TALLOC_FREE(sam_account);
			return NT_STATUS_ACCESS_DENIED;
		}
	} else {
		sam_account_from_object(sam_account, cur);
		DEBUG(3, ("Attempting to update user SID %s for user %s in the passdb\n",
			  sid_string_dbg(&user_sid),
			  pdb_get_username(sam_account)));
		if (!NT_STATUS_IS_OK(pdb_update_sam_account(sam_account))) {
			DEBUG(1, ("SAM Account for %s failed to be updated in the passdb!\n",
				  account));
			TALLOC_FREE(sam_account);
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	if (pdb_get_group_sid(sam_account) == NULL) {
		TALLOC_FREE(sam_account);
		return NT_STATUS_UNSUCCESSFUL;
	}

	group_sid = *pdb_get_group_sid(sam_account);

	if (!pdb_getgrsid(&map, group_sid)) {
		DEBUG(0, ("Primary group of %s has no mapping!\n",
			  pdb_get_username(sam_account)));
	} else {
		if (map.gid != passwd->pw_gid) {
			if (!(grp = getgrgid(map.gid))) {
				DEBUG(0, ("Could not find unix group %lu for user %s (group SID=%s)\n",
					  (unsigned long)map.gid, pdb_get_username(sam_account),
					  sid_string_dbg(&group_sid)));
			} else {
				smb_set_primary_group(grp->gr_name, pdb_get_username(sam_account));
			}
		}
	}

	if ( !passwd ) {
		DEBUG(1, ("No unix user for this account (%s), cannot adjust mappings\n",
			pdb_get_username(sam_account)));
	}

	TALLOC_FREE(sam_account);
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS handle_alias_object(struct dssync_passdb *pctx,
				    TALLOC_CTX *mem_ctx,
				    struct dssync_passdb_obj *obj)
{
	struct drsuapi_DsReplicaObjectListItemEx *cur = obj->cur;
	NTSTATUS status;
	fstring name;
	fstring comment;
	struct group *grp = NULL;
	struct dom_sid group_sid;
	uint32_t rid = 0;
	struct dom_sid *dom_sid = NULL;
	fstring sid_string;
	GROUP_MAP map;
	bool insert = true;

	const char *sAMAccountName;
	uint32_t sAMAccountType;
	uint32_t groupType;
	const char *description;
	uint32_t i;
	uint32_t num_members = 0;
	struct drsuapi_DsReplicaObjectIdentifier3 *members = NULL;

	group_sid = cur->object.identifier->sid;
	GET_STRING_EX(sAMAccountName, true);
	GET_UINT32_EX(sAMAccountType, true);
	GET_UINT32_EX(groupType, true);
	GET_STRING(description);

	status = find_drsuapi_attr_dn(obj, cur, DRSUAPI_ATTID_member,
				      &num_members, &members);
	if (NT_STATUS_EQUAL(status, NT_STATUS_PROPSET_NOT_FOUND)) {
		status = NT_STATUS_OK;
	}
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	fstrcpy(name, sAMAccountName);
	fstrcpy(comment, description);

	dom_sid_split_rid(mem_ctx, &group_sid, &dom_sid, &rid);

	sid_to_fstring(sid_string, &group_sid);
	DEBUG(0,("Creating alias[%s] - %s members[%u]\n",
		  name, sid_string, num_members));

	status = dssync_insert_obj(pctx, pctx->aliases, obj);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (pdb_getgrsid(&map, group_sid)) {
		if ( map.gid != -1 )
			grp = getgrgid(map.gid);
		insert = false;
	}

	if (grp == NULL) {
		gid_t gid;

		/* No group found from mapping, find it from its name. */
		if ((grp = getgrnam(name)) == NULL) {

			/* No appropriate group found, create one */

			DEBUG(0,("Creating unix group: '%s'\n", name));

			if (smb_create_group(name, &gid) != 0)
				return NT_STATUS_ACCESS_DENIED;

			if ((grp = getgrgid(gid)) == NULL)
				return NT_STATUS_ACCESS_DENIED;
		}
	}

	map.gid = grp->gr_gid;
	map.sid = group_sid;

	if (dom_sid_equal(dom_sid, &global_sid_Builtin)) {
		/*
		 * pdb_ldap does not like SID_NAME_WKN_GRP...
		 *
		 * map.sid_name_use = SID_NAME_WKN_GRP;
		 */
		map.sid_name_use = SID_NAME_ALIAS;
	} else {
		map.sid_name_use = SID_NAME_ALIAS;
	}

	fstrcpy(map.nt_name, name);
	if (description) {
		fstrcpy(map.comment, comment);
	} else {
		fstrcpy(map.comment, "");
	}

	if (insert)
		pdb_add_group_mapping_entry(&map);
	else
		pdb_update_group_mapping_entry(&map);

	for (i=0; i < num_members; i++) {
		struct dssync_passdb_mem *mem;

		status = dssync_create_mem(pctx, obj,
					   true /* active */,
					   &members[i], &mem);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS handle_group_object(struct dssync_passdb *pctx,
				    TALLOC_CTX *mem_ctx,
				    struct dssync_passdb_obj *obj)
{
	struct drsuapi_DsReplicaObjectListItemEx *cur = obj->cur;
	NTSTATUS status;
	fstring name;
	fstring comment;
	struct group *grp = NULL;
	struct dom_sid group_sid;
	fstring sid_string;
	GROUP_MAP map;
	bool insert = true;

	const char *sAMAccountName;
	uint32_t sAMAccountType;
	uint32_t groupType;
	const char *description;
	uint32_t i;
	uint32_t num_members = 0;
	struct drsuapi_DsReplicaObjectIdentifier3 *members = NULL;

	group_sid = cur->object.identifier->sid;
	GET_STRING_EX(sAMAccountName, true);
	GET_UINT32_EX(sAMAccountType, true);
	GET_UINT32_EX(groupType, true);
	GET_STRING(description);

	status = find_drsuapi_attr_dn(obj, cur, DRSUAPI_ATTID_member,
				      &num_members, &members);
	if (NT_STATUS_EQUAL(status, NT_STATUS_PROPSET_NOT_FOUND)) {
		status = NT_STATUS_OK;
	}
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	fstrcpy(name, sAMAccountName);
	fstrcpy(comment, description);

	sid_to_fstring(sid_string, &group_sid);
	DEBUG(0,("Creating group[%s] - %s members [%u]\n",
		  name, sid_string, num_members));

	status = dssync_insert_obj(pctx, pctx->groups, obj);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (pdb_getgrsid(&map, group_sid)) {
		if ( map.gid != -1 )
			grp = getgrgid(map.gid);
		insert = false;
	}

	if (grp == NULL) {
		gid_t gid;

		/* No group found from mapping, find it from its name. */
		if ((grp = getgrnam(name)) == NULL) {

			/* No appropriate group found, create one */

			DEBUG(0,("Creating unix group: '%s'\n", name));

			if (smb_create_group(name, &gid) != 0)
				return NT_STATUS_ACCESS_DENIED;

			if ((grp = getgrnam(name)) == NULL)
				return NT_STATUS_ACCESS_DENIED;
		}
	}

	map.gid = grp->gr_gid;
	map.sid = group_sid;
	map.sid_name_use = SID_NAME_DOM_GRP;
	fstrcpy(map.nt_name, name);
	if (description) {
		fstrcpy(map.comment, comment);
	} else {
		fstrcpy(map.comment, "");
	}

	if (insert)
		pdb_add_group_mapping_entry(&map);
	else
		pdb_update_group_mapping_entry(&map);

	for (i=0; i < num_members; i++) {
		struct dssync_passdb_mem *mem;

		status = dssync_create_mem(pctx, obj,
					   true /* active */,
					   &members[i], &mem);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS handle_interdomain_trust_object(struct dssync_passdb *pctx,
						TALLOC_CTX *mem_ctx,
						struct dssync_passdb_obj *obj)
{
	struct drsuapi_DsReplicaObjectListItemEx *cur = obj->cur;
	DEBUG(0,("trust: %s\n", cur->object.identifier->dn));
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

struct dssync_object_table_t {
	uint32_t type;
	NTSTATUS (*fn) (struct dssync_passdb *pctx,
			TALLOC_CTX *mem_ctx,
			struct dssync_passdb_obj *obj);
};

static const struct dssync_object_table_t dssync_object_table[] = {
	{ ATYPE_NORMAL_ACCOUNT,		handle_account_object },
	{ ATYPE_WORKSTATION_TRUST,	handle_account_object },
	{ ATYPE_SECURITY_LOCAL_GROUP,	handle_alias_object },
	{ ATYPE_SECURITY_GLOBAL_GROUP,	handle_group_object },
	{ ATYPE_INTERDOMAIN_TRUST,	handle_interdomain_trust_object },
};

/****************************************************************
****************************************************************/

static NTSTATUS parse_object(struct dssync_passdb *pctx,
			     TALLOC_CTX *mem_ctx,
			     struct drsuapi_DsReplicaObjectListItemEx *cur)
{
	NTSTATUS status = NT_STATUS_OK;
	DATA_BLOB *blob;
	int i = 0;
	int a = 0;
	struct drsuapi_DsReplicaAttribute *attr;

	char *name = NULL;
	uint32_t uacc = 0;
	uint32_t sam_type = 0;

	DEBUG(3, ("parsing object '%s'\n", cur->object.identifier->dn));

	for (i=0; i < cur->object.attribute_ctr.num_attributes; i++) {

		attr = &cur->object.attribute_ctr.attributes[i];

		if (attr->value_ctr.num_values != 1) {
			continue;
		}

		if (!attr->value_ctr.values[0].blob) {
			continue;
		}

		blob = attr->value_ctr.values[0].blob;

		switch (attr->attid) {
			case DRSUAPI_ATTID_sAMAccountName:
				pull_string_talloc(mem_ctx, NULL, 0, &name,
						   blob->data, blob->length,
						   STR_UNICODE);
				break;
			case DRSUAPI_ATTID_sAMAccountType:
				sam_type = IVAL(blob->data, 0);
				break;
			case DRSUAPI_ATTID_userAccountControl:
				uacc = IVAL(blob->data, 0);
				break;
			default:
				break;
		}
	}

	for (a=0; a < ARRAY_SIZE(dssync_object_table); a++) {
		if (sam_type == dssync_object_table[a].type) {
			if (dssync_object_table[a].fn) {
				struct dssync_passdb_obj *obj;
				status = dssync_create_obj(pctx, pctx->all,
							   sam_type, cur, &obj);
				if (!NT_STATUS_IS_OK(status)) {
					break;
				}
				status = dssync_object_table[a].fn(pctx,
								   mem_ctx,
								   obj);
				break;
			}
		}
	}

	return status;
}

static NTSTATUS parse_link(struct dssync_passdb *pctx,
			   TALLOC_CTX *mem_ctx,
			   struct drsuapi_DsReplicaLinkedAttribute *cur)
{
	struct drsuapi_DsReplicaObjectIdentifier3 *id3;
	const DATA_BLOB *blob;
	enum ndr_err_code ndr_err;
	NTSTATUS status;
	bool active = false;
	struct dssync_passdb_mem *mem;
	struct dssync_passdb_obj *obj;

	if (cur->attid != DRSUAPI_ATTID_member) {
		return NT_STATUS_OK;
	}

	if (cur->flags & DRSUAPI_DS_LINKED_ATTRIBUTE_FLAG_ACTIVE) {
		active = true;
	}

	DEBUG(3, ("parsing link '%s' - %s\n",
		  cur->identifier->dn, active?"adding":"deleting"));

	blob = cur->value.blob;

	if (blob == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	obj = dssync_search_obj_by_guid(pctx, pctx->all, &cur->identifier->guid);
	if (obj == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	id3 = talloc_zero(obj, struct drsuapi_DsReplicaObjectIdentifier3);
	if (id3 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* windows sometimes sends an extra two pad bytes here */
	ndr_err = ndr_pull_struct_blob(blob, id3, id3,
			(ndr_pull_flags_fn_t)ndr_pull_drsuapi_DsReplicaObjectIdentifier3);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		return status;
	}

	status = dssync_create_mem(pctx, obj,
				   active,
				   id3, &mem);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS passdb_process_objects(struct dssync_context *ctx,
				       TALLOC_CTX *mem_ctx,
				       struct drsuapi_DsReplicaObjectListItemEx *cur,
				       struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr)
{
	NTSTATUS status = NT_STATUS_OK;
	struct dssync_passdb *pctx =
		talloc_get_type_abort(ctx->private_data,
		struct dssync_passdb);

	for (; cur; cur = cur->next_object) {
		status = parse_object(pctx, mem_ctx, cur);
		if (!NT_STATUS_IS_OK(status)) {
			goto out;
		}
	}

 out:
	return status;
}

/****************************************************************
****************************************************************/

static NTSTATUS passdb_process_links(struct dssync_context *ctx,
				     TALLOC_CTX *mem_ctx,
				     uint32_t count,
				     struct drsuapi_DsReplicaLinkedAttribute *links,
				     struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr)
{
	NTSTATUS status = NT_STATUS_OK;
	struct dssync_passdb *pctx =
		talloc_get_type_abort(ctx->private_data,
		struct dssync_passdb);
	uint32_t i;

	for (i = 0; i < count; i++) {
		status = parse_link(pctx, mem_ctx, &links[i]);
		if (!NT_STATUS_IS_OK(status)) {
			goto out;
		}
	}

 out:
	return status;
}

/****************************************************************
****************************************************************/

const struct dssync_ops libnet_dssync_passdb_ops = {
	.startup		= passdb_startup,
	.process_objects	= passdb_process_objects,
	.process_links		= passdb_process_links,
	.finish			= passdb_finish,
};
