/* 
   Unix SMB/CIFS implementation.

   idmap TDB backend

   Copyright (C) Tim Potter 2000
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2003
   Copyright (C) Jeremy Allison 2006
   Copyright (C) Simo Sorce 2003-2006
   Copyright (C) Michael Adam 2009-2010

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
#include "system/filesys.h"
#include "winbindd.h"
#include "idmap.h"
#include "idmap_rw.h"
#include "dbwrap.h"
#include "../libcli/security/security.h"
#include "util_tdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

/* idmap version determines auto-conversion - this is the database
   structure version specifier. */

#define IDMAP_VERSION 2

struct idmap_tdb_context {
	struct db_context *db;
	struct idmap_rw_ops *rw_ops;
};

/* High water mark keys */
#define HWM_GROUP  "GROUP HWM"
#define HWM_USER   "USER HWM"

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
	struct dom_sid sid;
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

	sid_compose(&sid, &domain->sid, rid);

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

static bool idmap_tdb_upgrade(struct idmap_domain *dom, struct db_context *db)
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
			wm = dom->low_id;
		}

		if (dbwrap_store_int32(db, HWM_USER, wm) == -1) {
			DEBUG(0, ("Unable to byteswap user hwm in idmap database\n"));
			return False;
		}

		wm = dbwrap_fetch_int32(db, HWM_GROUP);
		if (wm != -1) {
			wm = IREV(wm);
		} else {
			wm = dom->low_id;
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
		DEBUG(0, ("Unable to store idmap version in database\n"));
		return False;
	}

	return True;
}

static NTSTATUS idmap_tdb_init_hwm(struct idmap_domain *dom)
{
	int ret;
	uint32_t low_uid;
	uint32_t low_gid;
	bool update_uid = false;
	bool update_gid = false;
	struct idmap_tdb_context *ctx;

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

	low_uid = dbwrap_fetch_int32(ctx->db, HWM_USER);
	if (low_uid == -1 || low_uid < dom->low_id) {
		update_uid = true;
	}

	low_gid = dbwrap_fetch_int32(ctx->db, HWM_GROUP);
	if (low_gid == -1 || low_gid < dom->low_id) {
		update_gid = true;
	}

	if (!update_uid && !update_gid) {
		return NT_STATUS_OK;
	}

	if (ctx->db->transaction_start(ctx->db) != 0) {
		DEBUG(0, ("Unable to start upgrade transaction!\n"));
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	if (update_uid) {
		ret = dbwrap_store_int32(ctx->db, HWM_USER, dom->low_id);
		if (ret == -1) {
			ctx->db->transaction_cancel(ctx->db);
			DEBUG(0, ("Unable to initialise user hwm in idmap "
				  "database\n"));
			return NT_STATUS_INTERNAL_DB_ERROR;
		}
	}

	if (update_gid) {
		ret = dbwrap_store_int32(ctx->db, HWM_GROUP, dom->low_id);
		if (ret == -1) {
			ctx->db->transaction_cancel(ctx->db);
			DEBUG(0, ("Unable to initialise group hwm in idmap "
				  "database\n"));
			return NT_STATUS_INTERNAL_DB_ERROR;
		}
	}

	if (ctx->db->transaction_commit(ctx->db) != 0) {
		DEBUG(0, ("Unable to commit upgrade transaction!\n"));
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	return NT_STATUS_OK;
}

static NTSTATUS idmap_tdb_open_db(struct idmap_domain *dom)
{
	NTSTATUS ret;
	TALLOC_CTX *mem_ctx;
	char *tdbfile = NULL;
	struct db_context *db = NULL;
	int32_t version;
	bool config_error = false;
	struct idmap_tdb_context *ctx;

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

	if (ctx->db) {
		/* it is already open */
		return NT_STATUS_OK;
	}

	/* use our own context here */
	mem_ctx = talloc_stackframe();

	/* use the old database if present */
	tdbfile = state_path("winbindd_idmap.tdb");
	if (!tdbfile) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	DEBUG(10,("Opening tdbfile %s\n", tdbfile ));

	/* Open idmap repository */
	db = db_open(mem_ctx, tdbfile, 0, TDB_DEFAULT, O_RDWR | O_CREAT, 0644);
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

		if (!idmap_tdb_upgrade(dom, db)) {
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

	ctx->db = talloc_move(ctx, &db);

	ret = idmap_tdb_init_hwm(dom);

done:
	talloc_free(mem_ctx);
	return ret;
}

/**********************************************************************
 IDMAP ALLOC TDB BACKEND
**********************************************************************/

/**********************************
 Allocate a new id. 
**********************************/

struct idmap_tdb_allocate_id_context {
	const char *hwmkey;
	const char *hwmtype;
	uint32_t high_hwm;
	uint32_t hwm;
};

static NTSTATUS idmap_tdb_allocate_id_action(struct db_context *db,
					     void *private_data)
{
	NTSTATUS ret;
	struct idmap_tdb_allocate_id_context *state;
	uint32_t hwm;

	state = (struct idmap_tdb_allocate_id_context *)private_data;

	hwm = dbwrap_fetch_int32(db, state->hwmkey);
	if (hwm == -1) {
		ret = NT_STATUS_INTERNAL_DB_ERROR;
		goto done;
	}

	/* check it is in the range */
	if (hwm > state->high_hwm) {
		DEBUG(1, ("Fatal Error: %s range full!! (max: %lu)\n",
			  state->hwmtype, (unsigned long)state->high_hwm));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	/* fetch a new id and increment it */
	ret = dbwrap_trans_change_uint32_atomic(db, state->hwmkey, &hwm, 1);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0, ("Fatal error while fetching a new %s value: %s\n!",
			  state->hwmtype, nt_errstr(ret)));
		goto done;
	}

	/* recheck it is in the range */
	if (hwm > state->high_hwm) {
		DEBUG(1, ("Fatal Error: %s range full!! (max: %lu)\n",
			  state->hwmtype, (unsigned long)state->high_hwm));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	ret = NT_STATUS_OK;
	state->hwm = hwm;

done:
	return ret;
}

static NTSTATUS idmap_tdb_allocate_id(struct idmap_domain *dom,
				      struct unixid *xid)
{
	const char *hwmkey;
	const char *hwmtype;
	uint32_t high_hwm;
	uint32_t hwm = 0;
	NTSTATUS status;
	struct idmap_tdb_allocate_id_context state;
	struct idmap_tdb_context *ctx;

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

	/* Get current high water mark */
	switch (xid->type) {

	case ID_TYPE_UID:
		hwmkey = HWM_USER;
		hwmtype = "UID";
		break;

	case ID_TYPE_GID:
		hwmkey = HWM_GROUP;
		hwmtype = "GID";
		break;

	default:
		DEBUG(2, ("Invalid ID type (0x%x)\n", xid->type));
		return NT_STATUS_INVALID_PARAMETER;
	}

	high_hwm = dom->high_id;

	state.hwm = hwm;
	state.high_hwm = high_hwm;
	state.hwmtype = hwmtype;
	state.hwmkey = hwmkey;

	status = dbwrap_trans_do(ctx->db, idmap_tdb_allocate_id_action,
				 &state);

	if (NT_STATUS_IS_OK(status)) {
		xid->id = state.hwm;
		DEBUG(10,("New %s = %d\n", hwmtype, state.hwm));
	} else {
		DEBUG(1, ("Error allocating a new %s\n", hwmtype));
	}

	return status;
}

/**
 * Allocate a new unix-ID.
 * For now this is for the default idmap domain only.
 * Should be extended later on.
 */
static NTSTATUS idmap_tdb_get_new_id(struct idmap_domain *dom,
				     struct unixid *id)
{
	NTSTATUS ret;

	if (!strequal(dom->name, "*")) {
		DEBUG(3, ("idmap_tdb_get_new_id: "
			  "Refusing allocation of a new unixid for domain'%s'. "
			  "Currently only supported for the default "
			  "domain \"*\".\n",
			   dom->name));
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	ret = idmap_tdb_allocate_id(dom, id);

	return ret;
}

/**********************************************************************
 IDMAP MAPPING TDB BACKEND
**********************************************************************/

/*****************************
 Initialise idmap database. 
*****************************/

static NTSTATUS idmap_tdb_set_mapping(struct idmap_domain *dom,
				      const struct id_map *map);

static NTSTATUS idmap_tdb_db_init(struct idmap_domain *dom)
{
	NTSTATUS ret;
	struct idmap_tdb_context *ctx;

	DEBUG(10, ("idmap_tdb_db_init called for domain '%s'\n", dom->name));

	ctx = talloc_zero(dom, struct idmap_tdb_context);
	if ( ! ctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* load backend specific configuration here: */
#if 0
	if (strequal(dom->name, "*")) {
	} else {
	}
#endif

	ctx->rw_ops = talloc_zero(ctx, struct idmap_rw_ops);
	if (ctx->rw_ops == NULL) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	ctx->rw_ops->get_new_id = idmap_tdb_get_new_id;
	ctx->rw_ops->set_mapping = idmap_tdb_set_mapping;

	dom->private_data = ctx;

	ret = idmap_tdb_open_db(dom);
	if ( ! NT_STATUS_IS_OK(ret)) {
		goto failed;
	}

	return NT_STATUS_OK;

failed:
	talloc_free(ctx);
	return ret;
}


/**
 * store a mapping in the database
 */

struct idmap_tdb_set_mapping_context {
	const char *ksidstr;
	const char *kidstr;
};

static NTSTATUS idmap_tdb_set_mapping_action(struct db_context *db,
					     void *private_data)
{
	NTSTATUS ret;
	struct idmap_tdb_set_mapping_context *state;

	state = (struct idmap_tdb_set_mapping_context *)private_data;

	DEBUG(10, ("Storing %s <-> %s map\n", state->ksidstr, state->kidstr));

	ret = dbwrap_store_bystring(db, state->ksidstr,
				    string_term_tdb_data(state->kidstr),
				    TDB_REPLACE);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0, ("Error storing SID -> ID (%s -> %s): %s\n",
			  state->ksidstr, state->kidstr, nt_errstr(ret)));
		goto done;
	}

	ret = dbwrap_store_bystring(db, state->kidstr,
				    string_term_tdb_data(state->ksidstr),
				    TDB_REPLACE);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0, ("Error storing ID -> SID (%s -> %s): %s\n",
			  state->kidstr, state->ksidstr, nt_errstr(ret)));
		goto done;
	}

	DEBUG(10,("Stored %s <-> %s\n", state->ksidstr, state->kidstr));
	ret = NT_STATUS_OK;

done:
	return ret;
}

static NTSTATUS idmap_tdb_set_mapping(struct idmap_domain *dom,
				      const struct id_map *map)
{
	struct idmap_tdb_context *ctx;
	NTSTATUS ret;
	char *ksidstr, *kidstr;
	struct idmap_tdb_set_mapping_context state;

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

	ksidstr = sid_string_talloc(ctx, map->sid);
	if (ksidstr == NULL) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	state.ksidstr = ksidstr;
	state.kidstr = kidstr;

	ret = dbwrap_trans_do(ctx->db, idmap_tdb_set_mapping_action, &state);

done:
	talloc_free(ksidstr);
	talloc_free(kidstr);
	return ret;
}

/**
 * Create a new mapping for an unmapped SID, also allocating a new ID.
 * This should be run inside a transaction.
 *
 * TODO:
 * Properly integrate this with multi domain idmap config:
 * Currently, the allocator is default-config only.
 */
static NTSTATUS idmap_tdb_new_mapping(struct idmap_domain *dom, struct id_map *map)
{
	NTSTATUS ret;
	struct idmap_tdb_context *ctx;

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

	ret = idmap_rw_new_mapping(dom, ctx->rw_ops, map);

	return ret;
}


/**********************************
 Single id to sid lookup function. 
**********************************/

static NTSTATUS idmap_tdb_id_to_sid(struct idmap_domain *dom, struct id_map *map)
{
	NTSTATUS ret;
	TDB_DATA data;
	char *keystr;
	struct idmap_tdb_context *ctx;

	if (!dom || !map) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

	/* apply filters before checking */
	if (!idmap_unix_id_is_in_range(map->xid.id, dom)) {
		DEBUG(5, ("Requested id (%u) out of range (%u - %u). Filtered!\n",
				map->xid.id, dom->low_id, dom->high_id));
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

static NTSTATUS idmap_tdb_sid_to_id(struct idmap_domain *dom, struct id_map *map)
{
	NTSTATUS ret;
	TDB_DATA data;
	char *keystr;
	unsigned long rec_id = 0;
	struct idmap_tdb_context *ctx;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

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
		goto done;
	}

	/* apply filters before returning result */
	if (!idmap_unix_id_is_in_range(map->xid.id, dom)) {
		DEBUG(5, ("Requested id (%u) out of range (%u - %u). Filtered!\n",
				map->xid.id, dom->low_id, dom->high_id));
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
	NTSTATUS ret;
	int i;

	/* initialize the status to avoid suprise */
	for (i = 0; ids[i]; i++) {
		ids[i]->status = ID_UNKNOWN;
	}

	for (i = 0; ids[i]; i++) {
		ret = idmap_tdb_id_to_sid(dom, ids[i]);
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

struct idmap_tdb_sids_to_unixids_context {
	struct idmap_domain *dom;
	struct id_map **ids;
	bool allocate_unmapped;
};

static NTSTATUS idmap_tdb_sids_to_unixids_action(struct db_context *db,
						 void *private_data)
{
	struct idmap_tdb_sids_to_unixids_context *state;
	int i;
	NTSTATUS ret = NT_STATUS_OK;

	state = (struct idmap_tdb_sids_to_unixids_context *)private_data;

	DEBUG(10, ("idmap_tdb_sids_to_unixids_action: "
		   " domain: [%s], allocate: %s\n",
		   state->dom->name,
		   state->allocate_unmapped ? "yes" : "no"));

	for (i = 0; state->ids[i]; i++) {
		if ((state->ids[i]->status == ID_UNKNOWN) ||
		    /* retry if we could not map in previous run: */
		    (state->ids[i]->status == ID_UNMAPPED))
		{
			NTSTATUS ret2;

			ret2 = idmap_tdb_sid_to_id(state->dom, state->ids[i]);
			if (!NT_STATUS_IS_OK(ret2)) {

				/* if it is just a failed mapping, continue */
				if (NT_STATUS_EQUAL(ret2, NT_STATUS_NONE_MAPPED)) {

					/* make sure it is marked as unmapped */
					state->ids[i]->status = ID_UNMAPPED;
					ret = STATUS_SOME_UNMAPPED;
				} else {
					/* some fatal error occurred, return immediately */
					ret = ret2;
					goto done;
				}
			} else {
				/* all ok, id is mapped */
				state->ids[i]->status = ID_MAPPED;
			}
		}

		if ((state->ids[i]->status == ID_UNMAPPED) &&
		    state->allocate_unmapped)
		{
			ret = idmap_tdb_new_mapping(state->dom, state->ids[i]);
			if (!NT_STATUS_IS_OK(ret)) {
				goto done;
			}
		}
	}

done:
	return ret;
}

static NTSTATUS idmap_tdb_sids_to_unixids(struct idmap_domain *dom, struct id_map **ids)
{
	struct idmap_tdb_context *ctx;
	NTSTATUS ret;
	int i;
	struct idmap_tdb_sids_to_unixids_context state;

	/* initialize the status to avoid suprise */
	for (i = 0; ids[i]; i++) {
		ids[i]->status = ID_UNKNOWN;
	}

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb_context);

	state.dom = dom;
	state.ids = ids;
	state.allocate_unmapped = false;

	ret = idmap_tdb_sids_to_unixids_action(ctx->db, &state);

	if (NT_STATUS_EQUAL(ret, STATUS_SOME_UNMAPPED) && !dom->read_only) {
		state.allocate_unmapped = true;
		ret = dbwrap_trans_do(ctx->db,
				      idmap_tdb_sids_to_unixids_action,
				      &state);
	}

	return ret;
}


/**********************************
 Close the idmap tdb instance
**********************************/

static struct idmap_methods db_methods = {
	.init = idmap_tdb_db_init,
	.unixids_to_sids = idmap_tdb_unixids_to_sids,
	.sids_to_unixids = idmap_tdb_sids_to_unixids,
	.allocate_id = idmap_tdb_get_new_id,
};

NTSTATUS idmap_tdb_init(void)
{
	DEBUG(10, ("calling idmap_tdb_init\n"));

	return smb_register_idmap(SMB_IDMAP_INTERFACE_VERSION, "tdb", &db_methods);
}
