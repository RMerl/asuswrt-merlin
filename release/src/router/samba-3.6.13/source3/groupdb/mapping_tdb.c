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
#include "system/filesys.h"
#include "passdb.h"
#include "groupdb/mapping.h"
#include "dbwrap.h"
#include "util_tdb.h"
#include "../libcli/security/security.h"

static struct db_context *db; /* used for driver files */

static bool enum_group_mapping(const struct dom_sid *domsid,
			       enum lsa_SidType sid_name_use,
			       GROUP_MAP **pp_rmap,
			       size_t *p_num_entries,
			       bool unix_only);
static bool group_map_remove(const struct dom_sid *sid);

static bool mapping_switch(const char *ldb_path);

/****************************************************************************
 Open the group mapping tdb.
****************************************************************************/
static bool init_group_mapping(void)
{
	const char *ldb_path;

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

	ldb_path = state_path("group_mapping.ldb");
	if (file_exist(ldb_path) && !mapping_switch(ldb_path)) {
		unlink(state_path("group_mapping.tdb"));
		return false;

	} else {
		/* handle upgrade from old versions of the database */
#if 0 /* -- Needs conversion to dbwrap -- */
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
#endif
	}
	return true;
}

static char *group_mapping_key(TALLOC_CTX *mem_ctx, const struct dom_sid *sid)
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

static bool get_group_map_from_sid(struct dom_sid sid, GROUP_MAP *map)
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

static bool group_map_remove(const struct dom_sid *sid)
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
	const struct dom_sid *domsid;
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
	    (dom_sid_compare_domain(state->domsid, &map.sid) != 0)) {
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

static bool enum_group_mapping(const struct dom_sid *domsid,
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

static NTSTATUS one_alias_membership(const struct dom_sid *member,
			       struct dom_sid **sids, size_t *num)
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
		struct dom_sid alias;
		uint32_t num_sids;

		if (!string_to_sid(&alias, string_sid))
			continue;

		num_sids = *num;
		status= add_sid_to_array_unique(NULL, &alias, sids, &num_sids);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
		*num = num_sids;
	}

done:
	TALLOC_FREE(frame);
	return status;
}

static NTSTATUS alias_memberships(const struct dom_sid *members, size_t num_members,
				  struct dom_sid **sids, size_t *num)
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

static bool is_aliasmem(const struct dom_sid *alias, const struct dom_sid *member)
{
	struct dom_sid *sids;
	size_t i;
	size_t num;

	/* This feels the wrong way round, but the on-disk data structure
	 * dictates it this way. */
	if (!NT_STATUS_IS_OK(alias_memberships(member, 1, &sids, &num)))
		return False;

	for (i=0; i<num; i++) {
		if (dom_sid_compare(alias, &sids[i]) == 0) {
			TALLOC_FREE(sids);
			return True;
		}
	}
	TALLOC_FREE(sids);
	return False;
}


static NTSTATUS add_aliasmem(const struct dom_sid *alias, const struct dom_sid *member)
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
	const struct dom_sid *alias;
	struct dom_sid **sids;
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
		struct dom_sid alias, member;
		const char *member_string;
		uint32_t num_sids;

		if (!string_to_sid(&alias, alias_string))
			continue;

		if (dom_sid_compare(state->alias, &alias) != 0)
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

		num_sids = *state->num;
		if (!NT_STATUS_IS_OK(add_sid_to_array(state->mem_ctx, &member,
						      state->sids,
						      &num_sids)))
		{
			/* talloc fail. */
			break;
		}
		*state->num = num_sids;
	}

	TALLOC_FREE(frame);
	return 0;
}

static NTSTATUS enum_aliasmem(const struct dom_sid *alias, TALLOC_CTX *mem_ctx,
			      struct dom_sid **sids, size_t *num)
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

static NTSTATUS del_aliasmem(const struct dom_sid *alias, const struct dom_sid *member)
{
	NTSTATUS status;
	struct dom_sid *sids;
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
		if (dom_sid_compare(&sids[i], alias) == 0) {
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


/* -- ldb->tdb switching code -------------------------------------------- */

/* change this if the data format ever changes */
#define LTDB_PACKING_FORMAT 0x26011967

/* old packing formats (not supported for now,
 * it was never used for group mapping AFAIK) */
#define LTDB_PACKING_FORMAT_NODN 0x26011966

static unsigned int pull_uint32(uint8_t *p, int ofs)
{
	p += ofs;
	return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

/*
  unpack a ldb message from a linear buffer in TDB_DATA
*/
static int convert_ldb_record(TDB_CONTEXT *ltdb, TDB_DATA key,
			      TDB_DATA data, void *ptr)
{
	TALLOC_CTX *tmp_ctx = talloc_tos();
	GROUP_MAP map;
	uint8_t *p;
	uint32_t format;
	uint32_t num_el;
	unsigned int remaining;
	unsigned int i, j;
	size_t len;
	char *name;
	char *val;
	char *q;
	uint32_t num_mem = 0;
	struct dom_sid *members = NULL;

	p = (uint8_t *)data.dptr;
	if (data.dsize < 8) {
		errno = EIO;
		goto failed;
	}

	format = pull_uint32(p, 0);
	num_el = pull_uint32(p, 4);
	p += 8;

	remaining = data.dsize - 8;

	switch (format) {
	case LTDB_PACKING_FORMAT:
		len = strnlen((char *)p, remaining);
		if (len == remaining) {
			errno = EIO;
			goto failed;
		}

		if (*p == '@') {
			/* ignore special LDB attributes */
			return 0;
		}

		if (strncmp((char *)p, "rid=", 4)) {
			/* unknown entry, ignore */
			DEBUG(3, ("Found unknown entry in group mapping "
				  "database named [%s]\n", (char *)p));
			return 0;
		}

		remaining -= len + 1;
		p += len + 1;
		break;

	case LTDB_PACKING_FORMAT_NODN:
	default:
		errno = EIO;
		goto failed;
	}

	if (num_el == 0) {
		/* bad entry, ignore */
		return 0;
	}

	if (num_el > remaining / 6) {
		errno = EIO;
		goto failed;
	}

	ZERO_STRUCT(map);

	for (i = 0; i < num_el; i++) {
		uint32_t num_vals;

		if (remaining < 10) {
			errno = EIO;
			goto failed;
		}
		len = strnlen((char *)p, remaining - 6);
		if (len == remaining - 6) {
			errno = EIO;
			goto failed;
		}
		name = talloc_strndup(tmp_ctx, (char *)p, len);
		if (name == NULL) {
			errno = ENOMEM;
			goto failed;
		}
		remaining -= len + 1;
		p += len + 1;

		num_vals = pull_uint32(p, 0);
		if (StrCaseCmp(name, "member") == 0) {
			num_mem = num_vals;
			members = talloc_array(tmp_ctx, struct dom_sid, num_mem);
			if (members == NULL) {
				errno = ENOMEM;
				goto failed;
			}
		} else if (num_vals != 1) {
			errno = EIO;
			goto failed;
		}

		p += 4;
		remaining -= 4;

		for (j = 0; j < num_vals; j++) {
			len = pull_uint32(p, 0);
			if (len > remaining-5) {
				errno = EIO;
				goto failed;
			}

			val = talloc_strndup(tmp_ctx, (char *)(p + 4), len);
			if (val == NULL) {
				errno = ENOMEM;
				goto failed;
			}

			remaining -= len+4+1;
			p += len+4+1;

			/* we ignore unknown or uninteresting attributes
			 * (objectclass, etc.) */
			if (StrCaseCmp(name, "gidNumber") == 0) {
				map.gid = strtoul(val, &q, 10);
				if (*q) {
					errno = EIO;
					goto failed;
				}
			} else if (StrCaseCmp(name, "sid") == 0) {
				if (!string_to_sid(&map.sid, val)) {
					errno = EIO;
					goto failed;
				}
			} else if (StrCaseCmp(name, "sidNameUse") == 0) {
				map.sid_name_use = strtoul(val, &q, 10);
				if (*q) {
					errno = EIO;
					goto failed;
				}
			} else if (StrCaseCmp(name, "ntname") == 0) {
				strlcpy(map.nt_name, val,
					sizeof(map.nt_name));
			} else if (StrCaseCmp(name, "comment") == 0) {
				strlcpy(map.comment, val,
					sizeof(map.comment));
			} else if (StrCaseCmp(name, "member") == 0) {
				if (!string_to_sid(&members[j], val)) {
					errno = EIO;
					goto failed;
				}
			}

			TALLOC_FREE(val);
		}

		TALLOC_FREE(name);
	}

	if (!add_mapping_entry(&map, 0)) {
		errno = EIO;
		goto failed;
	}

	if (num_mem) {
		for (j = 0; j < num_mem; j++) {
			NTSTATUS status;
			status = add_aliasmem(&map.sid, &members[j]);
			if (!NT_STATUS_IS_OK(status)) {
				errno = EIO;
				goto failed;
			}
		}
	}

	if (remaining != 0) {
		DEBUG(0, ("Errror: %d bytes unread in ltdb_unpack_data\n",
			  remaining));
	}

	return 0;

failed:
	return -1;
}

static bool mapping_switch(const char *ldb_path)
{
	TDB_CONTEXT *ltdb;
	TALLOC_CTX *frame;
	char *new_path;
	int ret;

	frame = talloc_stackframe();

	ltdb = tdb_open_log(ldb_path, 0, TDB_DEFAULT, O_RDONLY, 0600);
	if (ltdb == NULL) goto failed;

	/* ldb is just a very fancy tdb, read out raw data and perform
	 * conversion */
	ret = tdb_traverse(ltdb, convert_ldb_record, NULL);
	if (ret == -1) goto failed;

	if (ltdb) {
		tdb_close(ltdb);
		ltdb = NULL;
	}

	/* now rename the old db out of the way */
	new_path = state_path("group_mapping.ldb.replaced");
	if (!new_path) {
		goto failed;
	}
	if (rename(ldb_path, new_path) != 0) {
		DEBUG(0,("Failed to rename old group mapping database\n"));
		goto failed;
	}
	TALLOC_FREE(frame);
	return True;

failed:
	DEBUG(0, ("Failed to switch to tdb group mapping database\n"));
	if (ltdb) tdb_close(ltdb);
	TALLOC_FREE(frame);
	return False;
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
