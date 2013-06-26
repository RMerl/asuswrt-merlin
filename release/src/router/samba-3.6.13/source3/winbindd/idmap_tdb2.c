/* 
   Unix SMB/CIFS implementation.

   idmap TDB2 backend, used for clustered Samba setups.

   This uses dbwrap to access tdb files. The location can be set
   using tdb:idmap2.tdb =" in smb.conf

   Copyright (C) Andrew Tridgell 2007

   This is heavily based upon idmap_tdb.c, which is:

   Copyright (C) Tim Potter 2000
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2003
   Copyright (C) Jeremy Allison 2006
   Copyright (C) Simo Sorce 2003-2006
   Copyright (C) Michael Adam 2009-2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "system/filesys.h"
#include "winbindd.h"
#include "idmap.h"
#include "idmap_rw.h"
#include "dbwrap.h"
#include "../libcli/security/dom_sid.h"
#include "util_tdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

struct idmap_tdb2_context {
	struct db_context *db;
	const char *script; /* script to provide idmaps */
	struct idmap_rw_ops *rw_ops;
};

/* High water mark keys */
#define HWM_GROUP  "GROUP HWM"
#define HWM_USER   "USER HWM"


/*
 * check and initialize high/low water marks in the db
 */
static NTSTATUS idmap_tdb2_init_hwm(struct idmap_domain *dom)
{
	NTSTATUS status;
	uint32 low_id;
	struct idmap_tdb2_context *ctx;

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb2_context);

	/* Create high water marks for group and user id */

	low_id = dbwrap_fetch_int32(ctx->db, HWM_USER);
	if ((low_id == -1) || (low_id < dom->low_id)) {
		status = dbwrap_trans_store_int32(ctx->db, HWM_USER,
						  dom->low_id);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Unable to initialise user hwm in idmap "
				  "database: %s\n", nt_errstr(status)));
			return NT_STATUS_INTERNAL_DB_ERROR;
		}
	}

	low_id = dbwrap_fetch_int32(ctx->db, HWM_GROUP);
	if ((low_id == -1) || (low_id < dom->low_id)) {
		status = dbwrap_trans_store_int32(ctx->db, HWM_GROUP,
						  dom->low_id);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Unable to initialise group hwm in idmap "
				  "database: %s\n", nt_errstr(status)));
			return NT_STATUS_INTERNAL_DB_ERROR;
		}
	}

	return NT_STATUS_OK;
}


/*
  open the permanent tdb
 */
static NTSTATUS idmap_tdb2_open_db(struct idmap_domain *dom)
{
	char *db_path;
	struct idmap_tdb2_context *ctx;

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb2_context);

	if (ctx->db) {
		/* its already open */
		return NT_STATUS_OK;
	}

	db_path = talloc_asprintf(NULL, "%s/idmap2.tdb", lp_private_dir());
	NT_STATUS_HAVE_NO_MEMORY(db_path);

	/* Open idmap repository */
	ctx->db = db_open(ctx, db_path, 0, TDB_DEFAULT, O_RDWR|O_CREAT, 0644);
	TALLOC_FREE(db_path);

	if (ctx->db == NULL) {
		DEBUG(0, ("Unable to open idmap_tdb2 database '%s'\n",
			  db_path));
		return NT_STATUS_UNSUCCESSFUL;
	}

	return idmap_tdb2_init_hwm(dom);
}


/*
  Allocate a new id. 
*/

struct idmap_tdb2_allocate_id_context {
	const char *hwmkey;
	const char *hwmtype;
	uint32_t high_hwm;
	uint32_t hwm;
};

static NTSTATUS idmap_tdb2_allocate_id_action(struct db_context *db,
					      void *private_data)
{
	NTSTATUS ret;
	struct idmap_tdb2_allocate_id_context *state;
	uint32_t hwm;

	state = (struct idmap_tdb2_allocate_id_context *)private_data;

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
		DEBUG(1, ("Fatal error while fetching a new %s value\n!",
			  state->hwmtype));
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

static NTSTATUS idmap_tdb2_allocate_id(struct idmap_domain *dom,
				       struct unixid *xid)
{
	const char *hwmkey;
	const char *hwmtype;
	uint32_t high_hwm;
	uint32_t hwm = 0;
	NTSTATUS status;
	struct idmap_tdb2_allocate_id_context state;
	struct idmap_tdb2_context *ctx;

	status = idmap_tdb2_open_db(dom);
	NT_STATUS_NOT_OK_RETURN(status);

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb2_context);

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

	status = dbwrap_trans_do(ctx->db, idmap_tdb2_allocate_id_action,
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
static NTSTATUS idmap_tdb2_get_new_id(struct idmap_domain *dom,
				      struct unixid *id)
{
	NTSTATUS ret;

	if (!strequal(dom->name, "*")) {
		DEBUG(3, ("idmap_tdb2_get_new_id: "
			  "Refusing creation of mapping for domain'%s'. "
			  "Currently only supported for the default "
			  "domain \"*\".\n",
			   dom->name));
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	ret = idmap_tdb2_allocate_id(dom, id);

	return ret;
}

/*
  IDMAP MAPPING TDB BACKEND
*/

static NTSTATUS idmap_tdb2_set_mapping(struct idmap_domain *dom,
				       const struct id_map *map);

/*
  Initialise idmap database. 
*/
static NTSTATUS idmap_tdb2_db_init(struct idmap_domain *dom)
{
	NTSTATUS ret;
	struct idmap_tdb2_context *ctx;
	char *config_option = NULL;
	const char * idmap_script = NULL;

	ctx = talloc_zero(dom, struct idmap_tdb2_context);
	if ( ! ctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	config_option = talloc_asprintf(ctx, "idmap config %s", dom->name);
	if (config_option == NULL) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto failed;
	}
	ctx->script = lp_parm_const_string(-1, config_option, "script", NULL);
	talloc_free(config_option);

	idmap_script = lp_parm_const_string(-1, "idmap", "script", NULL);
	if (idmap_script != NULL) {
		DEBUG(0, ("Warning: 'idmap:script' is deprecated. "
			  " Please use 'idmap config * : script' instead!\n"));
	}

	if (strequal(dom->name, "*") && ctx->script == NULL) {
		/* fall back to idmap:script for backwards compatibility */
		ctx->script = idmap_script;
	}

	if (ctx->script) {
		DEBUG(1, ("using idmap script '%s'\n", ctx->script));
	}

	ctx->rw_ops = talloc_zero(ctx, struct idmap_rw_ops);
	if (ctx->rw_ops == NULL) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	ctx->rw_ops->get_new_id = idmap_tdb2_get_new_id;
	ctx->rw_ops->set_mapping = idmap_tdb2_set_mapping;

	dom->private_data = ctx;

	ret = idmap_tdb2_open_db(dom);
	if (!NT_STATUS_IS_OK(ret)) {
		goto failed;
	}

	return NT_STATUS_OK;

failed:
	talloc_free(ctx);
	return ret;
}


/**
 * store a mapping in the database.
 */

struct idmap_tdb2_set_mapping_context {
	const char *ksidstr;
	const char *kidstr;
};

static NTSTATUS idmap_tdb2_set_mapping_action(struct db_context *db,
					      void *private_data)
{
	TDB_DATA data;
	NTSTATUS ret;
	struct idmap_tdb2_set_mapping_context *state;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	state = (struct idmap_tdb2_set_mapping_context *)private_data;

	DEBUG(10, ("Storing %s <-> %s map\n", state->ksidstr, state->kidstr));

	/* check wheter sid mapping is already present in db */
	data = dbwrap_fetch_bystring(db, tmp_ctx, state->ksidstr);
	if (data.dptr) {
		ret = NT_STATUS_OBJECT_NAME_COLLISION;
		goto done;
	}

	ret = dbwrap_store_bystring(db, state->ksidstr,
				    string_term_tdb_data(state->kidstr),
				    TDB_INSERT);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0, ("Error storing SID -> ID: %s\n", nt_errstr(ret)));
		goto done;
	}

	ret = dbwrap_store_bystring(db, state->kidstr,
				    string_term_tdb_data(state->ksidstr),
				    TDB_INSERT);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0, ("Error storing ID -> SID: %s\n", nt_errstr(ret)));
		/* try to remove the previous stored SID -> ID map */
		dbwrap_delete_bystring(db, state->ksidstr);
		goto done;
	}

	DEBUG(10,("Stored %s <-> %s\n", state->ksidstr, state->kidstr));

done:
	talloc_free(tmp_ctx);
	return ret;
}

static NTSTATUS idmap_tdb2_set_mapping(struct idmap_domain *dom, const struct id_map *map)
{
	struct idmap_tdb2_context *ctx;
	NTSTATUS ret;
	char *ksidstr, *kidstr;
	struct idmap_tdb2_set_mapping_context state;

	if (!map || !map->sid) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ksidstr = kidstr = NULL;

	/* TODO: should we filter a set_mapping using low/high filters ? */

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb2_context);

	switch (map->xid.type) {

	case ID_TYPE_UID:
		kidstr = talloc_asprintf(ctx, "UID %lu", (unsigned long)map->xid.id);
		break;

	case ID_TYPE_GID:
		kidstr = talloc_asprintf(ctx, "GID %lu", (unsigned long)map->xid.id);
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

	ret = dbwrap_trans_do(ctx->db, idmap_tdb2_set_mapping_action,
			      &state);

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
*  Properly integrate this with multi domain idmap config:
 * Currently, the allocator is default-config only.
 */
static NTSTATUS idmap_tdb2_new_mapping(struct idmap_domain *dom, struct id_map *map)
{
	NTSTATUS ret;
	struct idmap_tdb2_context *ctx;

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb2_context);

	ret = idmap_rw_new_mapping(dom, ctx->rw_ops, map);

	return ret;
}


/*
  run a script to perform a mapping

  The script should the following command lines:

      SIDTOID S-1-xxxx
      IDTOSID UID xxxx
      IDTOSID GID xxxx

  and should return one of the following as a single line of text
     UID:xxxx
     GID:xxxx
     SID:xxxx
     ERR:xxxx
 */
static NTSTATUS idmap_tdb2_script(struct idmap_tdb2_context *ctx, struct id_map *map,
				  const char *fmt, ...)
{
	va_list ap;
	char *cmd;
	FILE *p;
	char line[64];
	unsigned long v;

	cmd = talloc_asprintf(ctx, "%s ", ctx->script);
	NT_STATUS_HAVE_NO_MEMORY(cmd);	

	va_start(ap, fmt);
	cmd = talloc_vasprintf_append(cmd, fmt, ap);
	va_end(ap);
	NT_STATUS_HAVE_NO_MEMORY(cmd);

	p = popen(cmd, "r");
	talloc_free(cmd);
	if (p == NULL) {
		return NT_STATUS_NONE_MAPPED;
	}

	if (fgets(line, sizeof(line)-1, p) == NULL) {
		pclose(p);
		return NT_STATUS_NONE_MAPPED;
	}
	pclose(p);

	DEBUG(10,("idmap script gave: %s\n", line));

	if (sscanf(line, "UID:%lu", &v) == 1) {
		map->xid.id   = v;
		map->xid.type = ID_TYPE_UID;
	} else if (sscanf(line, "GID:%lu", &v) == 1) {
		map->xid.id   = v;
		map->xid.type = ID_TYPE_GID;		
	} else if (strncmp(line, "SID:S-", 6) == 0) {
		if (!string_to_sid(map->sid, &line[4])) {
			DEBUG(0,("Bad SID in '%s' from idmap script %s\n",
				 line, ctx->script));
			return NT_STATUS_NONE_MAPPED;			
		}
	} else {
		DEBUG(0,("Bad reply '%s' from idmap script %s\n",
			 line, ctx->script));
		return NT_STATUS_NONE_MAPPED;
	}

	return NT_STATUS_OK;
}



/*
  Single id to sid lookup function. 
*/
static NTSTATUS idmap_tdb2_id_to_sid(struct idmap_domain *dom, struct id_map *map)
{
	NTSTATUS ret;
	TDB_DATA data;
	char *keystr;
	NTSTATUS status;
	struct idmap_tdb2_context *ctx;


	if (!dom || !map) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = idmap_tdb2_open_db(dom);
	NT_STATUS_NOT_OK_RETURN(status);

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb2_context);

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

	if (keystr == NULL) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	DEBUG(10,("Fetching record %s\n", keystr));

	/* Check if the mapping exists */
	data = dbwrap_fetch_bystring(ctx->db, keystr, keystr);

	if (!data.dptr) {
		char *sidstr;
		struct idmap_tdb2_set_mapping_context store_state;

		DEBUG(10,("Record %s not found\n", keystr));
		if (ctx->script == NULL) {
			ret = NT_STATUS_NONE_MAPPED;
			goto done;
		}

		ret = idmap_tdb2_script(ctx, map, "IDTOSID %s", keystr);
		if (!NT_STATUS_IS_OK(ret)) {
			goto done;
		}

		sidstr = sid_string_talloc(keystr, map->sid);
		if (!sidstr) {
			ret = NT_STATUS_NO_MEMORY;
			goto done;
		}

		store_state.ksidstr = sidstr;
		store_state.kidstr = keystr;

		ret = dbwrap_trans_do(ctx->db, idmap_tdb2_set_mapping_action,
				      &store_state);
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
	talloc_free(keystr);
	return ret;
}


/*
 Single sid to id lookup function. 
*/
static NTSTATUS idmap_tdb2_sid_to_id(struct idmap_domain *dom, struct id_map *map)
{
	NTSTATUS ret;
	TDB_DATA data;
	char *keystr;
	unsigned long rec_id = 0;
	struct idmap_tdb2_context *ctx;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	ret = idmap_tdb2_open_db(dom);
	NT_STATUS_NOT_OK_RETURN(ret);

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb2_context);

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
		char *idstr;
		struct idmap_tdb2_set_mapping_context store_state;

		DEBUG(10,(__location__ " Record %s not found\n", keystr));

		if (ctx->script == NULL) {
			ret = NT_STATUS_NONE_MAPPED;
			goto done;
		}

		ret = idmap_tdb2_script(ctx, map, "SIDTOID %s", keystr);
		if (!NT_STATUS_IS_OK(ret)) {
			goto done;
		}

		/* apply filters before returning result */
		if (!idmap_unix_id_is_in_range(map->xid.id, dom)) {
			DEBUG(5, ("Script returned id (%u) out of range "
				  "(%u - %u). Filtered!\n",
				  map->xid.id, dom->low_id, dom->high_id));
			ret = NT_STATUS_NONE_MAPPED;
			goto done;
		}

		idstr = talloc_asprintf(tmp_ctx, "%cID %lu",
					map->xid.type == ID_TYPE_UID?'U':'G',
					(unsigned long)map->xid.id);
		if (idstr == NULL) {
			ret = NT_STATUS_NO_MEMORY;
			goto done;
		}

		store_state.ksidstr = keystr;
		store_state.kidstr = idstr;

		ret = dbwrap_trans_do(ctx->db, idmap_tdb2_set_mapping_action,
				      &store_state);
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

/*
  lookup a set of unix ids. 
*/
static NTSTATUS idmap_tdb2_unixids_to_sids(struct idmap_domain *dom, struct id_map **ids)
{
	NTSTATUS ret;
	int i;

	/* initialize the status to avoid suprise */
	for (i = 0; ids[i]; i++) {
		ids[i]->status = ID_UNKNOWN;
	}

	for (i = 0; ids[i]; i++) {
		ret = idmap_tdb2_id_to_sid(dom, ids[i]);
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

/*
  lookup a set of sids. 
*/

struct idmap_tdb2_sids_to_unixids_context {
	struct idmap_domain *dom;
	struct id_map **ids;
	bool allocate_unmapped;
};

static NTSTATUS idmap_tdb2_sids_to_unixids_action(struct db_context *db,
						  void *private_data)
{
	struct idmap_tdb2_sids_to_unixids_context *state;
	int i;
	NTSTATUS ret = NT_STATUS_OK;

	state = (struct idmap_tdb2_sids_to_unixids_context *)private_data;

	DEBUG(10, ("idmap_tdb2_sids_to_unixids_action: "
		   " domain: [%s], allocate: %s\n",
		   state->dom->name,
		   state->allocate_unmapped ? "yes" : "no"));

	for (i = 0; state->ids[i]; i++) {
		if ((state->ids[i]->status == ID_UNKNOWN) ||
		    /* retry if we could not map in previous run: */
		    (state->ids[i]->status == ID_UNMAPPED))
		{
			NTSTATUS ret2;

			ret2 = idmap_tdb2_sid_to_id(state->dom, state->ids[i]);
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
			ret = idmap_tdb2_new_mapping(state->dom, state->ids[i]);
			if (!NT_STATUS_IS_OK(ret)) {
				goto done;
			}
		}
	}

done:
	return ret;
}

static NTSTATUS idmap_tdb2_sids_to_unixids(struct idmap_domain *dom, struct id_map **ids)
{
	NTSTATUS ret;
	int i;
	struct idmap_tdb2_sids_to_unixids_context state;
	struct idmap_tdb2_context *ctx;

	ctx = talloc_get_type(dom->private_data, struct idmap_tdb2_context);

	/* initialize the status to avoid suprise */
	for (i = 0; ids[i]; i++) {
		ids[i]->status = ID_UNKNOWN;
	}

	state.dom = dom;
	state.ids = ids;
	state.allocate_unmapped = false;

	ret = idmap_tdb2_sids_to_unixids_action(ctx->db, &state);

	if (NT_STATUS_EQUAL(ret, STATUS_SOME_UNMAPPED) && !dom->read_only) {
		state.allocate_unmapped = true;
		ret = dbwrap_trans_do(ctx->db,
				      idmap_tdb2_sids_to_unixids_action,
				      &state);
	}

	return ret;
}


static struct idmap_methods db_methods = {
	.init            = idmap_tdb2_db_init,
	.unixids_to_sids = idmap_tdb2_unixids_to_sids,
	.sids_to_unixids = idmap_tdb2_sids_to_unixids,
	.allocate_id     = idmap_tdb2_get_new_id
};

NTSTATUS idmap_tdb2_init(void)
{
	return smb_register_idmap(SMB_IDMAP_INTERFACE_VERSION, "tdb2", &db_methods);
}
