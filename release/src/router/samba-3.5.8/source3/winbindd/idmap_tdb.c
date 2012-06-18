/* 
   Unix SMB/CIFS implementation.

   idmap TDB backend

   Copyright (C) Tim Potter 2000
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2003
   Copyright (C) Jeremy Allison 2006
   Copyright (C) Simo Sorce 2003-2006
   
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
#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

/* idmap version determines auto-conversion - this is the database
   structure version specifier. */

#define IDMAP_VERSION 2

/* High water mark keys */
#define HWM_GROUP  "GROUP HWM"
#define HWM_USER   "USER HWM"

static struct idmap_tdb_state {

	/* User and group id pool */
	uid_t low_uid, high_uid;               /* Range of uids to allocate */
	gid_t low_gid, high_gid;               /* Range of gids to allocate */

} idmap_tdb_state;

struct convert_fn_state {
	struct db_context *db;
	bool failed;
};

/*****************************************************************************
 For idmap conversion: convert one record to new format
 Ancient versions (eg 2.2.3a) of winbindd_idmap.tdb mapped DOMAINNAME/rid
 instead of the SID.
*****************************************************************************/
static int convert_fn(struct db_record *rec, void *private_data)
{
	struct winbindd_domain *domain;
	char *p;
	NTSTATUS status;
	DOM_SID sid;
	uint32 rid;
	fstring keystr;
	fstring dom_name;
	TDB_DATA key2;
	struct convert_fn_state *s = (struct convert_fn_state *)private_data;

	DEBUG(10,("Converting %s\n", (const char *)rec->key.dptr));

	p = strchr((const char *)rec->key.dptr, '/');
	if (!p)
		return 0;

	*p = 0;
	fstrcpy(dom_name, (const char *)rec->key.dptr);
	*p++ = '/';

	domain = find_domain_from_name(dom_name);
	if (domain == NULL) {
		/* We must delete the old record. */
		DEBUG(0,("Unable to find domain %s\n", dom_name ));
		DEBUG(0,("deleting record %s\n", (const char *)rec->key.dptr ));

		status = rec->delete_rec(rec);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Unable to delete record %s:%s\n",
				(const char *)rec->key.dptr,
				nt_errstr(status)));
			s->failed = true;
			return -1;
		}

		return 0;
	}

	rid = atoi(p);

	sid_copy(&sid, &domain->sid);
	sid_append_rid(&sid, rid);

	sid_to_fstring(keystr, &sid);
	key2 = string_term_tdb_data(keystr);

	status = dbwrap_store(s->db, key2, rec->value, TDB_INSERT);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Unable to add record %s:%s\n",
			(const char *)key2.dptr,
			nt_errstr(status)));
		s->failed = true;
		return -1;
	}

	status = dbwrap_store(s->db, rec->value, key2, TDB_REPLACE);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Unable to update record %s:%s\n",
			(const char *)rec->value.dptr,
			nt_errstr(status)));
		s->failed = true;
		return -1;
	}

	status = rec->delete_rec(rec);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Unable to delete record %s:%s\n",
			(const char *)rec->key.dptr,
			nt_errstr(status)));
		s->failed = true;
		return -1;
	}

	return 0;
}

/*****************************************************************************
 Convert the idmap database from an older version.
*****************************************************************************/

static bool idmap_tdb_upgrade(struct db_context *db)
{
	int32 vers;
	bool bigendianheader;
	struct convert_fn_state s;

	DEBUG(0, ("Upgrading winbindd_idmap.tdb from an old version\n"));

	bigendianheader = (db->get_flags(db) & TDB_BIGENDIAN) ? True : False;

	vers = dbwrap_fetch_int32(db, "IDMAP_VERSION");

	if (((vers == -1) && bigendianheader) || (IREV(vers) == IDMAP_VERSION)) {
		/* Arrggghh ! Bytereversed or old big-endian - make order independent ! */
		/*
		 * high and low records were created on a
		 * big endian machine and will need byte-reversing.
		 */

		int32 wm;

		wm = dbwrap_fetch_int32(db, HWM_USER);

		if (wm != -1) {
			wm = IREV(wm);
		}  else {
			wm = idmap_tdb_state.low_uid;
		}

		if (dbwrap_store_int32(db, HWM_USER, wm) == -1) {
			DEBUG(0, ("Unable to byteswap user hwm in idmap database\n"));
			return False;
		}

		wm = dbwrap_fetch_int32(db, HWM_GROUP);
		if (wm != -1) {
			wm = IREV(wm);
		} else {
			wm = idmap_tdb_state.low_gid;
		}

		if (dbwrap_store_int32(db, HWM_GROUP, wm) == -1) {
			DEBUG(0, ("Unable to byteswap group hwm in idmap database\n"));
			return False;
		}
	}

	s.db = db;
	s.failed = false;

	/* the old format stored as DOMAIN/rid - now we store the SID direct */
	db->traverse(db, convert_fn, &s);

	if (s.failed) {
		DEBUG(0, ("Problem during conversion\n"));
		return False;
	}

	if (dbwrap_store_int32(db, "IDMAP_VERSION", IDMAP_VERSION) == -1) {
		DEBUG(0, ("Unable to store idmap version in databse\n"));
		return False;
	}

	return True;
}

static NTSTATUS idmap_tdb_load_ranges(void)
{
	uid_t low_uid = 0;
	uid_t high_uid = 0;
	gid_t low_gid = 0;
	gid_t high_gid = 0;

	if (!lp_idmap_uid(&low_uid, &high_uid)) {
		DEBUG(1, ("idmap uid missing\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!lp_idmap_gid(&low_gid, &high_gid)) {
		DEBUG(1, ("idmap gid missing\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	idmap_tdb_state.low_uid = low_uid;
	idmap_tdb_state.high_uid = high_uid;
	idmap_tdb_state.low_gid = low_gid;
	idmap_tdb_state.high_gid = high_gid;

	if (idmap_tdb_state.high_uid <= idmap_tdb_state.low_uid) {
		DEBUG(1, ("idmap uid range missing or invalid\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (idmap_tdb_state.high_gid <= idmap_tdb_state.low_gid) {
		DEBUG(1, ("idmap gid range missing or invalid\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

static NTSTATUS idmap_tdb_open_db(TALLOC_CTX *memctx,
				  bool check_config,
				  struct db_context **dbctx)
{
	NTSTATUS ret;
	TALLOC_CTX *ctx;
	char *tdbfile = NULL;
	struct db_context *db = NULL;
	int32_t version;
	bool config_error = false;

	ret = idmap_tdb_load_ranges();
	if (!NT_STATUS_IS_OK(ret)) {
		config_error = true;
		if (check_config) {
			return ret;
		}
	}

	/* use our own context here */
	ctx = talloc_stackframe();

	/* use the old database if present */
	tdbfile = state_path("winbindd_idmap.tdb");
	if (!tdbfile) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	DEBUG(10,("Opening tdbfile %s\n", tdbfile ));

	/* Open idmap repository */
	db = db_open(ctx, tdbfile, 0, TDB_DEFAULT, O_RDWR | O_CREAT, 0644);
	if (!db) {
		DEBUG(0, ("Unable to open idmap database\n"));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	/* check against earlier versions */
	version = dbwrap_fetch_int32(db, "IDMAP_VERSION");
	if (version != IDMAP_VERSION) {
		if (config_error) {
			DEBUG(0,("Upgrade of IDMAP_VERSION from %d to %d is not "
				 "possible with incomplete configuration\n",
				 version, IDMAP_VERSION));
			ret = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
		if (db->transaction_start(db) != 0) {
			DEBUG(0, ("Unable to start upgrade transaction!\n"));
			ret = NT_STATUS_INTERNAL_DB_ERROR;
			goto done;
		}

		if (!idmap_tdb_upgrade(db)) {
			db->transaction_cancel(db);
			DEBUG(0, ("Unable to open idmap database, it's in an old format, and upgrade failed!\n"));
			ret = NT_STATUS_INTERNAL_DB_ERROR;
			goto done;
		}

		if (db->transaction_commit(db) != 0) {
			DEBUG(0, ("Unable to commit upgrade transaction!\n"));
			ret = NT_STATUS_INTERNAL_DB_ERROR;
			goto done;
		}
	}

	*dbctx = talloc_move(memctx, &db);
	ret = NT_STATUS_OK;

done:
	talloc_free(ctx);
	return ret;
}

/**********************************************************************
 IDMAP ALLOC TDB BACKEND
**********************************************************************/
 
static struct db_context *idmap_alloc_db;

/**********************************
 Initialise idmap alloc database. 
**********************************/

static NTSTATUS idmap_tdb_alloc_init( const char *params )
{
	int ret;
	NTSTATUS status;
	uint32_t low_uid;
	uint32_t low_gid;
	bool update_uid = false;
	bool update_gid = false;

	status = idmap_tdb_open_db(NULL, true, &idmap_alloc_db);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("idmap will be unable to map foreign SIDs: %s\n",
			  nt_errstr(status)));
		return status;
	}

	low_uid = dbwrap_fetch_int32(idmap_alloc_db, HWM_USER);
	if (low_uid == -1 || low_uid < idmap_tdb_state.low_uid) {
		update_uid = true;
	}

	low_gid = dbwrap_fetch_int32(idmap_alloc_db, HWM_GROUP);
	if (low_gid == -1 || low_gid < idmap_tdb_state.low_gid) {
		update_gid = true;
	}

	if (!update_uid && !update_gid) {
		return NT_STATUS_OK;
	}

	if (idmap_alloc_db->transaction_start(idmap_alloc_db) != 0) {
		TALLOC_FREE(idmap_alloc_db);
		DEBUG(0, ("Unable to start upgrade transaction!\n"));
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	if (update_uid) {
		ret = dbwrap_store_int32(idmap_alloc_db, HWM_USER,
					 idmap_tdb_state.low_uid);
		if (ret == -1) {
			idmap_alloc_db->transaction_cancel(idmap_alloc_db);
			TALLOC_FREE(idmap_alloc_db);
			DEBUG(0, ("Unable to initialise user hwm in idmap "
				  "database\n"));
			return NT_STATUS_INTERNAL_DB_ERROR;
		}
	}

	if (update_gid) {
		ret = dbwrap_store_int32(idmap_alloc_db, HWM_GROUP,
					 idmap_tdb_state.low_gid);
		if (ret == -1) {
			idmap_alloc_db->transaction_cancel(idmap_alloc_db);
			TALLOC_FREE(idmap_alloc_db);
			DEBUG(0, ("Unable to initialise group hwm in idmap "
				  "database\n"));
			return NT_STATUS_INTERNAL_DB_ERROR;
		}
	}

	if (idmap_alloc_db->transaction_commit(idmap_alloc_db) != 0) {
		TALLOC_FREE(idmap_alloc_db);
		DEBUG(0, ("Unable to commit upgrade transaction!\n"));
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	return NT_STATUS_OK;
}

/**********************************
 Allocate a new id. 
**********************************/

static NTSTATUS idmap_tdb_allocate_id(struct unixid *xid)
{
	NTSTATUS ret;
	const char *hwmkey;
	const char *hwmtype;
	uint32_t high_hwm;
	uint32_t hwm;
	int res;

	/* Get current high water mark */
	switch (xid->type) {

	case ID_TYPE_UID:
		hwmkey = HWM_USER;
		hwmtype = "UID";
		high_hwm = idmap_tdb_state.high_uid;
		break;

	case ID_TYPE_GID:
		hwmkey = HWM_GROUP;
		hwmtype = "GID";
		high_hwm = idmap_tdb_state.high_gid;
		break;

	default:
		DEBUG(2, ("Invalid ID type (0x%x)\n", xid->type));
		return NT_STATUS_INVALID_PARAMETER;
	}

	res = idmap_alloc_db->transaction_start(idmap_alloc_db);
	if (res != 0) {
		DEBUG(1, (__location__ " Failed to start transaction.\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	if ((hwm = dbwrap_fetch_int32(idmap_alloc_db, hwmkey)) == -1) {
		idmap_alloc_db->transaction_cancel(idmap_alloc_db);
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	/* check it is in the range */
	if (hwm > high_hwm) {
		DEBUG(1, ("Fatal Error: %s range full!! (max: %lu)\n", 
			  hwmtype, (unsigned long)high_hwm));
		idmap_alloc_db->transaction_cancel(idmap_alloc_db);
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* fetch a new id and increment it */
	ret = dbwrap_change_uint32_atomic(idmap_alloc_db, hwmkey, &hwm, 1);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0, ("Fatal error while fetching a new %s value: %s\n!",
			  hwmtype, nt_errstr(ret)));
		idmap_alloc_db->transaction_cancel(idmap_alloc_db);
		return ret;
	}

	/* recheck it is in the range */
	if (hwm > high_hwm) {
		DEBUG(1, ("Fatal Error: %s range full!! (max: %lu)\n", 
			  hwmtype, (unsigned long)high_hwm));
		idmap_alloc_db->transaction_cancel(idmap_alloc_db);
		return NT_STATUS_UNSUCCESSFUL;
	}

	res = idmap_alloc_db->transaction_commit(idmap_alloc_db);
	if (res != 0) {
		DEBUG(1, (__location__ " Failed to commit transaction.\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	xid->id = hwm;
	DEBUG(10,("New %s = %d\n", hwmtype, hwm));

	return NT_STATUS_OK;
}

/**********************************
 Get current highest id. 
**********************************/

static NTSTATUS idmap_tdb_get_hwm(struct unixid *xid)
{
	const char *hwmkey;
	const char *hwmtype;
	uint32_t hwm;
	uint32_t high_hwm;

	/* Get current high water mark */
	switch (xid->type) {

	case ID_TYPE_UID:
		hwmkey = HWM_USER;
		hwmtype = "UID";
		high_hwm = idmap_tdb_state.high_uid;
		break;

	case ID_TYPE_GID:
		hwmkey = HWM_GROUP;
		hwmtype = "GID";
		high_hwm = idmap_tdb_state.high_gid;
		break;

	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	if ((hwm = dbwrap_fetch_int32(idmap_alloc_db, hwmkey)) == -1) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	xid->id = hwm;

	/* Warn if it is out of range */
	if (hwm >= high_hwm) {
		DEBUG(0, ("Warning: %s range full!! (max: %lu)\n", 
			  hwmtype, (unsigned long)high_hwm));
	}

	return NT_STATUS_OK;
}

/**********************************
 Set high id. 
**********************************/

static NTSTATUS idmap_tdb_set_hwm(struct unixid *xid)
{
	const char *hwmkey;
	const char *hwmtype;
	uint32_t hwm;
	uint32_t high_hwm;
	NTSTATUS ret;

	/* Get current high water mark */
	switch (xid->type) {

	case ID_TYPE_UID:
		hwmkey = HWM_USER;
		hwmtype = "UID";
		high_hwm = idmap_tdb_state.high_uid;
		break;

	case ID_TYPE_GID:
		hwmkey = HWM_GROUP;
		hwmtype = "GID";
		high_hwm = idmap_tdb_state.high_gid;
		break;

	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	hwm = xid->id;

	/* Warn if it is out of range */
	if (hwm >= high_hwm) {
		DEBUG(0, ("Warning: %s range full!! (max: %lu)\n", 
			  hwmtype, (unsigned long)high_hwm));
	}

	ret = dbwrap_trans_store_uint32(idmap_alloc_db, hwmkey, hwm);

	return ret;
}

/**********************************
 Close the alloc tdb 
**********************************/

static NTSTATUS idmap_tdb_alloc_close(void)
{
	TALLOC_FREE(idmap_alloc_db);
	return NT_STATUS_OK;
}

/**********************************************************************
 IDMAP MAPPING TDB BACKEND
**********************************************************************/
 
struct idmap_tdb_context {
	struct db_context *db;
	uint32_t filter_low_id;
	uint32_t filter_high_id;
};

/*****************************
 Initialise idmap database. 
*****************************/

static NTSTATUS idmap_tdb_db_init(struct idmap_domain *dom, const char *params)
{
	NTSTATUS ret;
	struct idmap_tdb_context *ctx;

	DEBUG(10, ("idmap_tdb_db_init called for domain '%s'\n", dom->name));

	ctx = talloc(dom, struct idmap_tdb_context);
	if ( ! ctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if (strequal(dom->name, "*")) {
		uid_t low_uid = 0;
		uid_t high_uid = 0;
		gid_t low_gid = 0;
		gid_t high_gid = 0;

		ctx->filter_low_id = 0;
		ctx->filter_high_id = 0;

		if (lp_idmap_uid(&low_uid, &high_uid)) {
			ctx->filter_low_id = low_uid;
			ctx->filter_high_id = high_uid;
		} else {
			DEBUG(3, ("Warning: 'idmap uid' not set!\n"));
		}

		if (lp_idmap_gid(&low_gid, &high_gid)) {
			if ((low_gid != low_uid) || (high_gid != high_uid)) {
				DEBUG(1, ("Warning: 'idmap uid' and 'idmap gid'"
				      " ranges do not agree -- building "
				      "intersection\n"));
				ctx->filter_low_id = MAX(ctx->filter_low_id,
							 low_gid);
				ctx->filter_high_id = MIN(ctx->filter_high_id,
							  high_gid);
			}
		} else {
			DEBUG(3, ("Warning: 'idmap gid' not set!\n"));
		}
	} else {
		char *config_option = NULL;
		const char *range;

		config_option = talloc_asprintf(ctx, "idmap config %s", dom->name);
		if ( ! config_option) {
			DEBUG(0, ("Out of memory!\n"));
			ret = NT_STATUS_NO_MEMORY;
			goto failed;
		}

		range = lp_parm_const_string(-1, config_option, "range", NULL);
		if (( ! range) ||
		    (sscanf(range, "%u - %u", &ctx->filter_low_id, &ctx->filter_high_id) != 2))
		{
			ctx->filter_low_id = 0;
			ctx->filter_high_id = 0;
		}

		talloc_free(config_option);
	}

	if (ctx->filter_low_id > ctx->filter_high_id) {
		ctx->filter_low_id = 0;
		ctx->filter_high_id = 0;
	}

	DEBUG(10, ("idmap_tdb_db_init: filter range %u-%u loaded for domain "
	      "'%s'\n", ctx->filter_low_id, ctx->filter_high_id, dom->name));

	ret = idmap_tdb_open_db(ctx, false, &ctx->db);
	if ( ! NT_STATUS_IS_OK(ret)) {
		goto failed;
	}

	dom->private_data = ctx;

	return NT_STATUS_OK;

failed:
	talloc_free(ctx);
	return ret;
}

/**********************************
 Single id to sid lookup function. 
**********************************/

static NTSTATUS idmap_tdb_id_to_sid(struct idmap_tdb_context *ctx, struct id_map *map)
{
	NTSTATUS ret;
	TDB_DATA data;
	char *keystr;

	if (!ctx || !map) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* apply filters before checking */
	if ((ctx->filter_low_id && (map->xid.id < ctx->filter_low_id)) ||
	    (ctx->filter_high_id && (map->xid.id > ctx->filter_high_id))) {
		DEBUG(5, ("Requested id (%u) out of range (%u - %u). Filtered!\n",
				map->xid.id, ctx->filter_low_id, ctx->filter_high_id));
		return NT_STATUS_NONE_MAPPED;
	}

	switch (map->xid.type) {

	case ID_TYPE_UID:
		keystr = talloc_asprintf(ctx, "UID %lu", (unsigned long)map->xid.id);
		break;
		
	case ID_TYPE_GID:
		keystr = talloc_asprintf(ctx, "GID %lu", (unsigned long)map->xid.id);
		break;

	default:
		DEBUG(2, ("INVALID unix ID type: 0x02%x\n", map->xid.type));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* final SAFE_FREE safe */
	data.dptr = NULL;

	if (keystr == NULL) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	DEBUG(10,("Fetching record %s\n", keystr));

	/* Check if the mapping exists */
	data = dbwrap_fetch_bystring(ctx->db, NULL, keystr);

	if (!data.dptr) {
		DEBUG(10,("Record %s not found\n", keystr));
		ret = NT_STATUS_NONE_MAPPED;
		goto done;
	}
		
	if (!string_to_sid(map->sid, (const char *)data.dptr)) {
		DEBUG(10,("INVALID SID (%s) in record %s\n",
			(const char *)data.dptr, keystr));
		ret = NT_STATUS_INTERNAL_DB_ERROR;
		goto done;
	}

	DEBUG(10,("Found record %s -> %s\n", keystr, (const char *)data.dptr));
	ret = NT_STATUS_OK;

done:
	talloc_free(data.dptr);
	talloc_free(keystr);
	return ret;
}

/**********************************
 Single sid to id lookup function. 
**********************************/

static NTSTATUS idmap_tdb_sid_to_id(struct idmap_tdb_context *ctx, struct id_map *map)
{
	NTSTATUS ret;
	TDB_DATA data;
	char *keystr;
	unsigned long rec_id = 0;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	keystr = sid_string_talloc(tmp_ctx, map->sid);
	if (keystr == NULL) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	DEBUG(10,("Fetching record %s\n", keystr));

	/* Check if sid is present in database */
	data = dbwrap_fetch_bystring(ctx->db, tmp_ctx, keystr);
	if (!data.dptr) {
		DEBUG(10,("Record %s not found\n", keystr));
		ret = NT_STATUS_NONE_MAPPED;
		goto done;
	}

	/* What type of record is this ? */
	if (sscanf((const char *)data.dptr, "UID %lu", &rec_id) == 1) { /* Try a UID record. */
		map->xid.id = rec_id;
		map->xid.type = ID_TYPE_UID;
		DEBUG(10,("Found uid record %s -> %s \n", keystr, (const char *)data.dptr ));
		ret = NT_STATUS_OK;

	} else if (sscanf((const char *)data.dptr, "GID %lu", &rec_id) == 1) { /* Try a GID record. */
		map->xid.id = rec_id;
		map->xid.type = ID_TYPE_GID;
		DEBUG(10,("Found gid record %s -> %s \n", keystr, (const char *)data.dptr ));
		ret = NT_STATUS_OK;

	} else { /* Unknown record type ! */
		DEBUG(2, ("Found INVALID record %s -> %s\n", keystr, (const char *)data.dptr));
		ret = NT_STATUS_INTERNAL_DB_ERROR;
	}

	/* apply filters before returning result */
	if ((ctx->filter_low_id && (map->xid.id < ctx->filter_low_id)) ||
	    (ctx->filter_high_id && (map->xid.id > ctx->filter_high_id))) {
		DEBUG(5, ("Requested id (%u) out of range (%u - %u). Filtered!\n",
				map->xid.id, ctx->filter_low_id, ctx->filter_high_id));
		ret = NT_STATUS_NONE_MAPPED;
	}

done:
	talloc_free(tmp_ctx);
	return ret;
}

/**********************************
 lookup a set of unix ids. 
**********************************/

static NTSTATUS idmap_tdb_unixids_to_sids(struct idmap_domain *dom, struct id_map **ids)
{
	struct idmap_tdb_context *ctx;
	NTSTATUS ret;
	int i;

	/* initialize the status to avoid suprise */
	for (i = 0; ids[i]; i++) {
		ids[i]->status = ID_UNKNOWN;
	}
	
	ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

	for (i = 0; ids[i]; i++) {
		ret = idmap_tdb_id_to_sid(ctx, ids[i]);
		if ( ! NT_STATUS_IS_OK(ret)) {

			/* if it is just a failed mapping continue */
			if (NT_STATUS_EQUAL(ret, NT_STATUS_NONE_MAPPED)) {

				/* make sure it is marked as unmapped */
				ids[i]->status = ID_UNMAPPED;
				continue;
			}
			
			/* some fatal error occurred, return immediately */
			goto done;
		}

		/* all ok, id is mapped */
		ids[i]->status = ID_MAPPED;
	}

	ret = NT_STATUS_OK;

done:
	return ret;
}

/**********************************
 lookup a set of sids. 
**********************************/

static NTSTATUS idmap_tdb_sids_to_unixids(struct idmap_domain *dom, struct id_map **ids)
{
	struct idmap_tdb_context *ctx;
	NTSTATUS ret;
	int i;

	/* initialize the status to avoid suprise */
	for (i = 0; ids[i]; i++) {
		ids[i]->status = ID_UNKNOWN;
	}
	
	ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

	for (i = 0; ids[i]; i++) {
		ret = idmap_tdb_sid_to_id(ctx, ids[i]);
		if ( ! NT_STATUS_IS_OK(ret)) {

			/* if it is just a failed mapping continue */
			if (NT_STATUS_EQUAL(ret, NT_STATUS_NONE_MAPPED)) {

				/* make sure it is marked as unmapped */
				ids[i]->status = ID_UNMAPPED;
				continue;
			}
			
			/* some fatal error occurred, return immediately */
			goto done;
		}

		/* all ok, id is mapped */
		ids[i]->status = ID_MAPPED;
	}

	ret = NT_STATUS_OK;

done:
	return ret;
}

/**********************************
 set a mapping.
**********************************/

static NTSTATUS idmap_tdb_set_mapping(struct idmap_domain *dom,
				      const struct id_map *map)
{
	struct idmap_tdb_context *ctx;
	NTSTATUS ret;
	TDB_DATA ksid, kid;
	char *ksidstr, *kidstr;
	fstring tmp;

	if (!map || !map->sid) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ksidstr = kidstr = NULL;

	/* TODO: should we filter a set_mapping using low/high filters ? */

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

	switch (map->xid.type) {

	case ID_TYPE_UID:
		kidstr = talloc_asprintf(ctx, "UID %lu",
					 (unsigned long)map->xid.id);
		break;

	case ID_TYPE_GID:
		kidstr = talloc_asprintf(ctx, "GID %lu",
					 (unsigned long)map->xid.id);
		break;

	default:
		DEBUG(2, ("INVALID unix ID type: 0x02%x\n", map->xid.type));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (kidstr == NULL) {
		DEBUG(0, ("ERROR: Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	if ((ksidstr = talloc_asprintf(
		     ctx, "%s", sid_to_fstring(tmp, map->sid))) == NULL) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	DEBUG(10, ("Storing %s <-> %s map\n", ksidstr, kidstr));
	kid = string_term_tdb_data(kidstr);
	ksid = string_term_tdb_data(ksidstr);

	if (ctx->db->transaction_start(ctx->db) != 0) {
		DEBUG(0, ("Failed to start transaction for %s\n",
			  ksidstr));
		ret = NT_STATUS_INTERNAL_DB_ERROR;
		goto done;
	}

	ret = dbwrap_store(ctx->db, ksid, kid, TDB_REPLACE);
	if (!NT_STATUS_IS_OK(ret)) {
		ctx->db->transaction_cancel(ctx->db);
		DEBUG(0, ("Error storing SID -> ID (%s -> %s): %s\n",
			  ksidstr, kidstr, nt_errstr(ret)));
		goto done;
	}
	ret = dbwrap_store(ctx->db, kid, ksid, TDB_REPLACE);
	if (!NT_STATUS_IS_OK(ret)) {
		ctx->db->transaction_cancel(ctx->db);
		DEBUG(0, ("Error storing ID -> SID (%s -> %s): %s\n",
			  kidstr, ksidstr, nt_errstr(ret)));
		goto done;
	}

	if (ctx->db->transaction_commit(ctx->db) != 0) {
		DEBUG(0, ("Failed to commit transaction for (%s -> %s)\n",
			  ksidstr, kidstr));
		ret = NT_STATUS_INTERNAL_DB_ERROR;
		goto done;
	}

	DEBUG(10,("Stored %s <-> %s\n", ksidstr, kidstr));
	ret = NT_STATUS_OK;

done:
	talloc_free(ksidstr);
	talloc_free(kidstr);
	return ret;
}

/**********************************
 remove a mapping.
**********************************/

static NTSTATUS idmap_tdb_remove_mapping(struct idmap_domain *dom,
					 const struct id_map *map)
{
	struct idmap_tdb_context *ctx;
	NTSTATUS ret;
	TDB_DATA ksid, kid, data;
	char *ksidstr, *kidstr;
	fstring tmp;

	if (!map || !map->sid) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ksidstr = kidstr = NULL;
	data.dptr = NULL;

	/* TODO: should we filter a remove_mapping using low/high filters ? */

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

	switch (map->xid.type) {

	case ID_TYPE_UID:
		kidstr = talloc_asprintf(ctx, "UID %lu",
					 (unsigned long)map->xid.id);
		break;

	case ID_TYPE_GID:
		kidstr = talloc_asprintf(ctx, "GID %lu",
					 (unsigned long)map->xid.id);
		break;

	default:
		DEBUG(2, ("INVALID unix ID type: 0x02%x\n", map->xid.type));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (kidstr == NULL) {
		DEBUG(0, ("ERROR: Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	if ((ksidstr = talloc_asprintf(
		     ctx, "%s", sid_to_fstring(tmp, map->sid))) == NULL) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	DEBUG(10, ("Checking %s <-> %s map\n", ksidstr, kidstr));
	ksid = string_term_tdb_data(ksidstr);
	kid = string_term_tdb_data(kidstr);

	if (ctx->db->transaction_start(ctx->db) != 0) {
		DEBUG(0, ("Failed to start transaction for %s\n",
			  ksidstr));
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	/* Check if sid is present in database */
	data = dbwrap_fetch(ctx->db, NULL, ksid);
	if (!data.dptr) {
		ctx->db->transaction_cancel(ctx->db);
		DEBUG(10,("Record %s not found\n", ksidstr));
		ret = NT_STATUS_NONE_MAPPED;
		goto done;
	}

	/* Check if sid is mapped to the specified ID */
	if ((data.dsize != kid.dsize) ||
	    (memcmp(data.dptr, kid.dptr, data.dsize) != 0)) {
		ctx->db->transaction_cancel(ctx->db);
		DEBUG(10,("Specified SID does not map to specified ID\n"));
		DEBUGADD(10,("Actual mapping is %s -> %s\n", ksidstr,
			 (const char *)data.dptr));
		ret = NT_STATUS_NONE_MAPPED;
		goto done;
	}

	DEBUG(10, ("Removing %s <-> %s map\n", ksidstr, kidstr));

	/* Delete previous mappings. */

	DEBUG(10, ("Deleting existing mapping %s -> %s\n", ksidstr, kidstr ));
	ret = dbwrap_delete(ctx->db, ksid);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Warning: Failed to delete %s: %s\n",
			 ksidstr, nt_errstr(ret)));
	}

	DEBUG(10,("Deleting existing mapping %s -> %s\n", kidstr, ksidstr ));
	ret = dbwrap_delete(ctx->db, kid);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Warning: Failed to delete %s: %s\n",
			 kidstr, nt_errstr(ret)));
	}

	if (ctx->db->transaction_commit(ctx->db) != 0) {
		DEBUG(0, ("Failed to commit transaction for (%s -> %s)\n",
			  ksidstr, kidstr));
		ret = NT_STATUS_INTERNAL_DB_ERROR;
		goto done;
	}

	ret = NT_STATUS_OK;

done:
	talloc_free(ksidstr);
	talloc_free(kidstr);
	talloc_free(data.dptr);
	return ret;
}

/**********************************
 Close the idmap tdb instance
**********************************/

static NTSTATUS idmap_tdb_close(struct idmap_domain *dom)
{
	struct idmap_tdb_context *ctx;

	if (dom->private_data) {
		ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

		TALLOC_FREE(ctx->db);
	}
	return NT_STATUS_OK;
}

struct dump_data {
	TALLOC_CTX *memctx;
	struct id_map **maps;
	int *num_maps;
	NTSTATUS ret;
};

static int idmap_tdb_dump_one_entry(struct db_record *rec, void *pdata)
{
	struct dump_data *data = talloc_get_type(pdata, struct dump_data);
	struct id_map *maps;
	int num_maps = *data->num_maps;

	/* ignore any record but the ones with a SID as key */
	if (strncmp((const char *)rec->key.dptr, "S-", 2) == 0) {

		maps = talloc_realloc(NULL, *data->maps, struct id_map, num_maps+1);
		if ( ! maps) {
			DEBUG(0, ("Out of memory!\n"));
			data->ret = NT_STATUS_NO_MEMORY;
			return -1;
		}
       		*data->maps = maps;
		maps[num_maps].sid = talloc(maps, DOM_SID);
		if ( ! maps[num_maps].sid) {
			DEBUG(0, ("Out of memory!\n"));
			data->ret = NT_STATUS_NO_MEMORY;
			return -1;
		}

		if (!string_to_sid(maps[num_maps].sid, (const char *)rec->key.dptr)) {
			DEBUG(10,("INVALID record %s\n", (const char *)rec->key.dptr));
			/* continue even with errors */
			return 0;
		}

		/* Try a UID record. */
		if (sscanf((const char *)rec->value.dptr, "UID %u", &(maps[num_maps].xid.id)) == 1) {
			maps[num_maps].xid.type = ID_TYPE_UID;
			maps[num_maps].status = ID_MAPPED;
			*data->num_maps = num_maps + 1;

		/* Try a GID record. */
		} else
		if (sscanf((const char *)rec->value.dptr, "GID %u", &(maps[num_maps].xid.id)) == 1) {
			maps[num_maps].xid.type = ID_TYPE_GID;
			maps[num_maps].status = ID_MAPPED;
			*data->num_maps = num_maps + 1;

		/* Unknown record type ! */
		} else {
			maps[num_maps].status = ID_UNKNOWN;
			DEBUG(2, ("Found INVALID record %s -> %s\n",
				(const char *)rec->key.dptr,
				(const char *)rec->value.dptr));
			/* do not increment num_maps */
		}
	}

	return 0;
}

/**********************************
 Dump all mappings out
**********************************/

static NTSTATUS idmap_tdb_dump_data(struct idmap_domain *dom, struct id_map **maps, int *num_maps)
{
	struct idmap_tdb_context *ctx;
	struct dump_data *data;
	NTSTATUS ret = NT_STATUS_OK;

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

	data = TALLOC_ZERO_P(ctx, struct dump_data);
	if ( ! data) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}
	data->maps = maps;
	data->num_maps = num_maps;
	data->ret = NT_STATUS_OK;

	ctx->db->traverse_read(ctx->db, idmap_tdb_dump_one_entry, data);

	if ( ! NT_STATUS_IS_OK(data->ret)) {
		ret = data->ret;
	}

	talloc_free(data);
	return ret;
}

static struct idmap_methods db_methods = {

	.init = idmap_tdb_db_init,
	.unixids_to_sids = idmap_tdb_unixids_to_sids,
	.sids_to_unixids = idmap_tdb_sids_to_unixids,
	.set_mapping = idmap_tdb_set_mapping,
	.remove_mapping = idmap_tdb_remove_mapping,
	.dump_data = idmap_tdb_dump_data,
	.close_fn = idmap_tdb_close
};

static struct idmap_alloc_methods db_alloc_methods = {

	.init = idmap_tdb_alloc_init,
	.allocate_id = idmap_tdb_allocate_id,
	.get_id_hwm = idmap_tdb_get_hwm,
	.set_id_hwm = idmap_tdb_set_hwm,
	.close_fn = idmap_tdb_alloc_close
};

NTSTATUS idmap_alloc_tdb_init(void)
{
	return smb_register_idmap_alloc(SMB_IDMAP_INTERFACE_VERSION, "tdb", &db_alloc_methods);
}

NTSTATUS idmap_tdb_init(void)
{
	NTSTATUS ret;

	DEBUG(10, ("calling idmap_tdb_init\n"));

	/* FIXME: bad hack to actually register also the alloc_tdb module without changining configure.in */
	ret = idmap_alloc_tdb_init();
	if (! NT_STATUS_IS_OK(ret)) {
		return ret;
	}
	return smb_register_idmap(SMB_IDMAP_INTERFACE_VERSION, "tdb", &db_methods);
}
