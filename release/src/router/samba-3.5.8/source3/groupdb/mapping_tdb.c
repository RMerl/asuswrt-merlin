/* 
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-2006,
 *  Copyright (C) Jean François Micouleau      1998-2001.
 *  Copyright (C) Volker Lendecke              2006.
 *  Copyright (C) Gerald Carter                2006.
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

#include "includes.h"
#include "groupdb/mapping.h"

static struct db_context *db; /* used for driver files */

static bool enum_group_mapping(const DOM_SID *domsid, enum lsa_SidType sid_name_use, GROUP_MAP **pp_rmap,
			       size_t *p_num_entries, bool unix_only);
static bool group_map_remove(const DOM_SID *sid);
	
/****************************************************************************
 Open the group mapping tdb.
****************************************************************************/
static bool init_group_mapping(void)
{
	if (db != NULL) {
		return true;
	}

	db = db_open(NULL, state_path("group_mapping.tdb"), 0,
			   TDB_DEFAULT, O_RDWR|O_CREAT, 0600);
	if (db == NULL) {
		DEBUG(0, ("Failed to open group mapping database: %s\n",
			  strerror(errno)));
		return false;
	}

#if 0
	/*
	 * This code was designed to handle a group mapping version
	 * upgrade. mapping_tdb is not active by default anymore, so ignore
	 * this here.
	 */
	{
		const char *vstring = "INFO/version";
		int32 vers_id;
		GROUP_MAP *map_table = NULL;
		size_t num_entries = 0;

		/* handle a Samba upgrade */
		tdb_lock_bystring(tdb, vstring);

		/* Cope with byte-reversed older versions of the db. */
		vers_id = tdb_fetch_int32(tdb, vstring);
		if ((vers_id == DATABASE_VERSION_V1)
		    || (IREV(vers_id) == DATABASE_VERSION_V1)) {
			/*
			 * Written on a bigendian machine with old fetch_int
			 * code. Save as le.
			 */
			tdb_store_int32(tdb, vstring, DATABASE_VERSION_V2);
			vers_id = DATABASE_VERSION_V2;
		}

		/* if its an unknown version we remove everthing in the db */

		if (vers_id != DATABASE_VERSION_V2) {
			tdb_wipe_all(tdb);
			tdb_store_int32(tdb, vstring, DATABASE_VERSION_V2);
		}

		tdb_unlock_bystring(tdb, vstring);

		/* cleanup any map entries with a gid == -1 */

		if ( enum_group_mapping( NULL, SID_NAME_UNKNOWN, &map_table,
					 &num_entries, False ) ) {
			int i;

			for ( i=0; i<num_entries; i++ ) {
				if ( map_table[i].gid == -1 ) {
					group_map_remove( &map_table[i].sid );
				}
			}

			SAFE_FREE( map_table );
		}
	}
#endif

	return true;
}

static char *group_mapping_key(TALLOC_CTX *mem_ctx, const DOM_SID *sid)
{
	char *sidstr, *result;

	sidstr = sid_string_talloc(talloc_tos(), sid);
	if (sidstr == NULL) {
		return NULL;
	}

	result = talloc_asprintf(mem_ctx, "%s%s", GROUP_PREFIX, sidstr);

	TALLOC_FREE(sidstr);
	return result;
}

/****************************************************************************
****************************************************************************/
static bool add_mapping_entry(GROUP_MAP *map, int flag)
{
	char *key, *buf;
	int len;
	NTSTATUS status;

	key = group_mapping_key(talloc_tos(), &map->sid);
	if (key == NULL) {
		return false;
	}

	len = tdb_pack(NULL, 0, "ddff",
		map->gid, map->sid_name_use, map->nt_name, map->comment);

	buf = TALLOC_ARRAY(key, char, len);
	if (!buf) {
		TALLOC_FREE(key);
		return false;
	}
	len = tdb_pack((uint8 *)buf, len, "ddff", map->gid,
		       map->sid_name_use, map->nt_name, map->comment);

	status = dbwrap_trans_store(
		db, string_term_tdb_data(key),
		make_tdb_data((uint8_t *)buf, len), TDB_REPLACE);

	TALLOC_FREE(key);

	return NT_STATUS_IS_OK(status);
}


/****************************************************************************
 Return the sid and the type of the unix group.
****************************************************************************/

static bool get_group_map_from_sid(DOM_SID sid, GROUP_MAP *map)
{
	TDB_DATA dbuf;
	char *key;
	int ret = 0;

	/* the key is the SID, retrieving is direct */

	key = group_mapping_key(talloc_tos(), &sid);
	if (key == NULL) {
		return false;
	}

	dbuf = dbwrap_fetch_bystring(db, key, key);
	if (dbuf.dptr == NULL) {
		TALLOC_FREE(key);
		return false;
	}

	ret = tdb_unpack(dbuf.dptr, dbuf.dsize, "ddff",
			&map->gid, &map->sid_name_use,
			&map->nt_name, &map->comment);

	TALLOC_FREE(key);

	if ( ret == -1 ) {
		DEBUG(3,("get_group_map_from_sid: tdb_unpack failure\n"));
		return false;
	}

	sid_copy(&map->sid, &sid);

	return true;
}

static bool dbrec2map(const struct db_record *rec, GROUP_MAP *map)
{
	if ((rec->key.dsize < strlen(GROUP_PREFIX))
	    || (strncmp((char *)rec->key.dptr, GROUP_PREFIX,
			GROUP_PREFIX_LEN) != 0)) {
		return False;
	}

	if (!string_to_sid(&map->sid, (const char *)rec->key.dptr
			   + GROUP_PREFIX_LEN)) {
		return False;
	}

	return tdb_unpack(rec->value.dptr, rec->value.dsize, "ddff",
			  &map->gid, &map->sid_name_use, &map->nt_name,
			  &map->comment) != -1;
}

struct find_map_state {
	bool found;
	const char *name;	/* If != NULL, look for name */
	gid_t gid;		/* valid iff name == NULL */
	GROUP_MAP *map;
};

static int find_map(struct db_record *rec, void *private_data)
{
	struct find_map_state *state = (struct find_map_state *)private_data;

	if (!dbrec2map(rec, state->map)) {
		DEBUG(10, ("failed to unpack map\n"));
		return 0;
	}

	if (state->name != NULL) {
		if (strequal(state->name, state->map->nt_name)) {
			state->found = true;
			return 1;
		}
	}
	else {
		if (state->map->gid == state->gid) {
			state->found = true;
			return 1;
		}
	}

	return 0;
}

/****************************************************************************
 Return the sid and the type of the unix group.
****************************************************************************/

static bool get_group_map_from_gid(gid_t gid, GROUP_MAP *map)
{
	struct find_map_state state;

	state.found = false;
	state.name = NULL;	/* Indicate we're looking for gid */
	state.gid = gid;
	state.map = map;

	db->traverse_read(db, find_map, (void *)&state);

	return state.found;
}

/****************************************************************************
 Return the sid and the type of the unix group.
****************************************************************************/

static bool get_group_map_from_ntname(const char *name, GROUP_MAP *map)
{
	struct find_map_state state;

	state.found = false;
	state.name = name;
	state.map = map;

	db->traverse_read(db, find_map, (void *)&state);

	return state.found;
}

/****************************************************************************
 Remove a group mapping entry.
****************************************************************************/

static bool group_map_remove(const DOM_SID *sid)
{
	char *key;
	NTSTATUS status;

	key = group_mapping_key(talloc_tos(), sid);
	if (key == NULL) {
		return false;
	}

	status = dbwrap_trans_delete(db, string_term_tdb_data(key));

	TALLOC_FREE(key);
	return NT_STATUS_IS_OK(status);
}

/****************************************************************************
 Enumerate the group mapping.
****************************************************************************/

struct enum_map_state {
	const DOM_SID *domsid;
	enum lsa_SidType sid_name_use;
	bool unix_only;

	size_t num_maps;
	GROUP_MAP *maps;
};

static int collect_map(struct db_record *rec, void *private_data)
{
	struct enum_map_state *state = (struct enum_map_state *)private_data;
	GROUP_MAP map;
	GROUP_MAP *tmp;

	if (!dbrec2map(rec, &map)) {
		return 0;
	}
	/* list only the type or everything if UNKNOWN */
	if (state->sid_name_use != SID_NAME_UNKNOWN
	    && state->sid_name_use != map.sid_name_use) {
		DEBUG(11,("enum_group_mapping: group %s is not of the "
			  "requested type\n", map.nt_name));
		return 0;
	}

	if ((state->unix_only == ENUM_ONLY_MAPPED) && (map.gid == -1)) {
		DEBUG(11,("enum_group_mapping: group %s is non mapped\n",
			  map.nt_name));
		return 0;
	}

	if ((state->domsid != NULL) &&
	    (sid_compare_domain(state->domsid, &map.sid) != 0)) {
		DEBUG(11,("enum_group_mapping: group %s is not in domain\n",
			  sid_string_dbg(&map.sid)));
		return 0;
	}

	if (!(tmp = SMB_REALLOC_ARRAY(state->maps, GROUP_MAP,
				      state->num_maps+1))) {
		DEBUG(0,("enum_group_mapping: Unable to enlarge group "
			 "map!\n"));
		return 1;
	}

	state->maps = tmp;
	state->maps[state->num_maps] = map;
	state->num_maps++;
	return 0;
}

static bool enum_group_mapping(const DOM_SID *domsid,
			       enum lsa_SidType sid_name_use,
			       GROUP_MAP **pp_rmap,
			       size_t *p_num_entries, bool unix_only)
{
	struct enum_map_state state;

	state.domsid = domsid;
	state.sid_name_use = sid_name_use;
	state.unix_only = unix_only;
	state.num_maps = 0;
	state.maps = NULL;

	if (db->traverse_read(db, collect_map, (void *)&state) < 0) {
		return false;
	}

	*pp_rmap = state.maps;
	*p_num_entries = state.num_maps;

	return true;
}

/* This operation happens on session setup, so it should better be fast. We
 * store a list of aliases a SID is member of hanging off MEMBEROF/SID. */

static NTSTATUS one_alias_membership(const DOM_SID *member,
			       DOM_SID **sids, size_t *num)
{
	fstring tmp;
	fstring key;
	char *string_sid;
	TDB_DATA dbuf;
	const char *p;
	NTSTATUS status = NT_STATUS_OK;
	TALLOC_CTX *frame = talloc_stackframe();

	slprintf(key, sizeof(key), "%s%s", MEMBEROF_PREFIX,
		 sid_to_fstring(tmp, member));

	dbuf = dbwrap_fetch_bystring(db, frame, key);
	if (dbuf.dptr == NULL) {
		TALLOC_FREE(frame);
		return NT_STATUS_OK;
	}

	p = (const char *)dbuf.dptr;

	while (next_token_talloc(frame, &p, &string_sid, " ")) {
		DOM_SID alias;

		if (!string_to_sid(&alias, string_sid))
			continue;

		status= add_sid_to_array_unique(NULL, &alias, sids, num);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
	}

done:
	TALLOC_FREE(frame);
	return status;
}

static NTSTATUS alias_memberships(const DOM_SID *members, size_t num_members,
				  DOM_SID **sids, size_t *num)
{
	size_t i;

	*num = 0;
	*sids = NULL;

	for (i=0; i<num_members; i++) {
		NTSTATUS status = one_alias_membership(&members[i], sids, num);
		if (!NT_STATUS_IS_OK(status))
			return status;
	}
	return NT_STATUS_OK;
}

static bool is_aliasmem(const DOM_SID *alias, const DOM_SID *member)
{
	DOM_SID *sids;
	size_t i, num;

	/* This feels the wrong way round, but the on-disk data structure
	 * dictates it this way. */
	if (!NT_STATUS_IS_OK(alias_memberships(member, 1, &sids, &num)))
		return False;

	for (i=0; i<num; i++) {
		if (sid_compare(alias, &sids[i]) == 0) {
			TALLOC_FREE(sids);
			return True;
		}
	}
	TALLOC_FREE(sids);
	return False;
}


static NTSTATUS add_aliasmem(const DOM_SID *alias, const DOM_SID *member)
{
	GROUP_MAP map;
	char *key;
	fstring string_sid;
	char *new_memberstring;
	struct db_record *rec;
	NTSTATUS status;

	if (!get_group_map_from_sid(*alias, &map))
		return NT_STATUS_NO_SUCH_ALIAS;

	if ( (map.sid_name_use != SID_NAME_ALIAS) &&
	     (map.sid_name_use != SID_NAME_WKN_GRP) )
		return NT_STATUS_NO_SUCH_ALIAS;

	if (is_aliasmem(alias, member))
		return NT_STATUS_MEMBER_IN_ALIAS;

	sid_to_fstring(string_sid, member);

	key = talloc_asprintf(talloc_tos(), "%s%s", MEMBEROF_PREFIX,
			      string_sid);
	if (key == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (db->transaction_start(db) != 0) {
		DEBUG(0, ("transaction_start failed\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	rec = db->fetch_locked(db, key, string_term_tdb_data(key));

	if (rec == NULL) {
		DEBUG(10, ("fetch_lock failed\n"));
		TALLOC_FREE(key);
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto cancel;
	}

	sid_to_fstring(string_sid, alias);

	if (rec->value.dptr != NULL) {
		new_memberstring = talloc_asprintf(
			key, "%s %s", (char *)(rec->value.dptr), string_sid);
	} else {
		new_memberstring = talloc_strdup(key, string_sid);
	}

	if (new_memberstring == NULL) {
		TALLOC_FREE(key);
		status = NT_STATUS_NO_MEMORY;
		goto cancel;
	}

	status = rec->store(rec, string_term_tdb_data(new_memberstring), 0);

	TALLOC_FREE(key);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Could not store record: %s\n", nt_errstr(status)));
		goto cancel;
	}

	if (db->transaction_commit(db) != 0) {
		DEBUG(0, ("transaction_commit failed\n"));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		return status;
	}

	return NT_STATUS_OK;

 cancel:
	if (db->transaction_cancel(db) != 0) {
		smb_panic("transaction_cancel failed");
	}

	return status;
}

struct aliasmem_state {
	TALLOC_CTX *mem_ctx;
	const DOM_SID *alias;
	DOM_SID **sids;
	size_t *num;
};

static int collect_aliasmem(struct db_record *rec, void *priv)
{
	struct aliasmem_state *state = (struct aliasmem_state *)priv;
	const char *p;
	char *alias_string;
	TALLOC_CTX *frame;

	if (strncmp((const char *)rec->key.dptr, MEMBEROF_PREFIX,
		    MEMBEROF_PREFIX_LEN) != 0)
		return 0;

	p = (const char *)rec->value.dptr;

	frame = talloc_stackframe();

	while (next_token_talloc(frame, &p, &alias_string, " ")) {
		DOM_SID alias, member;
		const char *member_string;

		if (!string_to_sid(&alias, alias_string))
			continue;

		if (sid_compare(state->alias, &alias) != 0)
			continue;

		/* Ok, we found the alias we're looking for in the membership
		 * list currently scanned. The key represents the alias
		 * member. Add that. */

		member_string = strchr((const char *)rec->key.dptr, '/');

		/* Above we tested for MEMBEROF_PREFIX which includes the
		 * slash. */

		SMB_ASSERT(member_string != NULL);
		member_string += 1;

		if (!string_to_sid(&member, member_string))
			continue;

		if (!NT_STATUS_IS_OK(add_sid_to_array(state->mem_ctx, &member,
						      state->sids,
						      state->num)))
		{
			/* talloc fail. */
			break;
		}
	}

	TALLOC_FREE(frame);
	return 0;
}

static NTSTATUS enum_aliasmem(const DOM_SID *alias, TALLOC_CTX *mem_ctx,
			      DOM_SID **sids, size_t *num)
{
	GROUP_MAP map;
	struct aliasmem_state state;

	if (!get_group_map_from_sid(*alias, &map))
		return NT_STATUS_NO_SUCH_ALIAS;

	if ( (map.sid_name_use != SID_NAME_ALIAS) &&
	     (map.sid_name_use != SID_NAME_WKN_GRP) )
		return NT_STATUS_NO_SUCH_ALIAS;

	*sids = NULL;
	*num = 0;

	state.alias = alias;
	state.sids = sids;
	state.num = num;
	state.mem_ctx = mem_ctx;

	db->traverse_read(db, collect_aliasmem, &state);
	return NT_STATUS_OK;
}

static NTSTATUS del_aliasmem(const DOM_SID *alias, const DOM_SID *member)
{
	NTSTATUS status;
	DOM_SID *sids;
	size_t i, num;
	bool found = False;
	char *member_string;
	char *key;
	fstring sid_string;

	if (db->transaction_start(db) != 0) {
		DEBUG(0, ("transaction_start failed\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	status = alias_memberships(member, 1, &sids, &num);

	if (!NT_STATUS_IS_OK(status)) {
		goto cancel;
	}

	for (i=0; i<num; i++) {
		if (sid_compare(&sids[i], alias) == 0) {
			found = True;
			break;
		}
	}

	if (!found) {
		TALLOC_FREE(sids);
		status = NT_STATUS_MEMBER_NOT_IN_ALIAS;
		goto cancel;
	}

	if (i < num)
		sids[i] = sids[num-1];

	num -= 1;

	sid_to_fstring(sid_string, member);

	key = talloc_asprintf(sids, "%s%s", MEMBEROF_PREFIX, sid_string);
	if (key == NULL) {
		TALLOC_FREE(sids);
		status = NT_STATUS_NO_MEMORY;
		goto cancel;
	}

	if (num == 0) {
		status = dbwrap_delete_bystring(db, key);
		goto commit;
	}

	member_string = talloc_strdup(sids, "");
	if (member_string == NULL) {
		TALLOC_FREE(sids);
		status = NT_STATUS_NO_MEMORY;
		goto cancel;
	}

	for (i=0; i<num; i++) {

		sid_to_fstring(sid_string, &sids[i]);

		member_string = talloc_asprintf_append_buffer(
			member_string, " %s", sid_string);

		if (member_string == NULL) {
			TALLOC_FREE(sids);
			status = NT_STATUS_NO_MEMORY;
			goto cancel;
		}
	}

	status = dbwrap_store_bystring(
		db, key, string_term_tdb_data(member_string), 0);
 commit:
	TALLOC_FREE(sids);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("dbwrap_store_bystring failed: %s\n",
			   nt_errstr(status)));
		goto cancel;
	}

	if (db->transaction_commit(db) != 0) {
		DEBUG(0, ("transaction_commit failed\n"));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		return status;
	}

	return NT_STATUS_OK;

 cancel:
	if (db->transaction_cancel(db) != 0) {
		smb_panic("transaction_cancel failed");
	}
	return status;
}

static const struct mapping_backend tdb_backend = {
	.add_mapping_entry         = add_mapping_entry,
	.get_group_map_from_sid    = get_group_map_from_sid,
	.get_group_map_from_gid    = get_group_map_from_gid,
	.get_group_map_from_ntname = get_group_map_from_ntname,
	.group_map_remove          = group_map_remove,
	.enum_group_mapping        = enum_group_mapping,
	.one_alias_membership      = one_alias_membership,
	.add_aliasmem              = add_aliasmem,
	.del_aliasmem              = del_aliasmem,
	.enum_aliasmem             = enum_aliasmem	
};

/*
  initialise the tdb mapping backend
 */
const struct mapping_backend *groupdb_tdb_init(void)
{
	if (!init_group_mapping()) {
		DEBUG(0,("Failed to initialise tdb mapping backend\n"));
		return NULL;
	}

	return &tdb_backend;
}
