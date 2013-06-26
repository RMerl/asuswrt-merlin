/* 
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *  Copyright (C) Gerald Carter                     2002-2005
 *  Copyright (C) Michael Adam                      2007-2009
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

/* Implementation of internal registry database functions. */

#include "includes.h"
#include "system/filesys.h"
#include "registry.h"
#include "reg_db.h"
#include "reg_util_internal.h"
#include "reg_backend_db.h"
#include "reg_objects.h"
#include "nt_printing.h"
#include "util_tdb.h"
#include "dbwrap.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

static struct db_context *regdb = NULL;
static int regdb_refcount;

static bool regdb_key_exists(struct db_context *db, const char *key);
static bool regdb_key_is_base_key(const char *key);
static WERROR regdb_fetch_keys_internal(struct db_context *db, const char *key,
					struct regsubkey_ctr *ctr);
static bool regdb_store_keys_internal(struct db_context *db, const char *key,
				      struct regsubkey_ctr *ctr);
static int regdb_fetch_values_internal(struct db_context *db, const char* key,
				       struct regval_ctr *values);
static bool regdb_store_values_internal(struct db_context *db, const char *key,
					struct regval_ctr *values);

static NTSTATUS create_sorted_subkeys(const char *key);

/* List the deepest path into the registry.  All part components will be created.*/

/* If you want to have a part of the path controlled by the tdb and part by
   a virtual registry db (e.g. printing), then you have to list the deepest path.
   For example,"HKLM/SOFTWARE/Microsoft/Windows NT/CurrentVersion/Print" 
   allows the reg_db backend to handle everything up to 
   "HKLM/SOFTWARE/Microsoft/Windows NT/CurrentVersion" and then we'll hook 
   the reg_printing backend onto the last component of the path (see 
   KEY_PRINTING_2K in include/rpc_reg.h)   --jerry */

static const char *builtin_registry_paths[] = {
	KEY_PRINTING_2K,
	KEY_PRINTING_PORTS,
	KEY_PRINTING,
	KEY_PRINTING "\\Forms",
	KEY_PRINTING "\\Printers",
	KEY_PRINTING "\\Environments\\Windows NT x86\\Print Processors\\winprint",
	KEY_SHARES,
	KEY_EVENTLOG,
	KEY_SMBCONF,
	KEY_PERFLIB,
	KEY_PERFLIB_009,
	KEY_GROUP_POLICY,
	KEY_SAMBA_GROUP_POLICY,
	KEY_GP_MACHINE_POLICY,
	KEY_GP_MACHINE_WIN_POLICY,
	KEY_HKCU,
	KEY_GP_USER_POLICY,
	KEY_GP_USER_WIN_POLICY,
	"HKLM\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon\\GPExtensions",
	"HKLM\\SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors",
	KEY_PROD_OPTIONS,
	"HKLM\\SYSTEM\\CurrentControlSet\\Control\\Terminal Server\\DefaultUserConfiguration",
	KEY_TCPIP_PARAMS,
	KEY_NETLOGON_PARAMS,
	KEY_HKU,
	KEY_HKCR,
	KEY_HKPD,
	KEY_HKPT,
	 NULL };

struct builtin_regkey_value {
	const char *path;
	const char *valuename;
	uint32 type;
	union {
		const char *string;
		uint32 dw_value;
	} data;
};

static struct builtin_regkey_value builtin_registry_values[] = {
	{ KEY_PRINTING_PORTS,
		SAMBA_PRINTER_PORT_NAME, REG_SZ, { "" } },
	{ KEY_PRINTING_2K,
		"DefaultSpoolDirectory", REG_SZ, { "C:\\Windows\\System32\\Spool\\Printers" } },
	{ KEY_EVENTLOG,
		"DisplayName", REG_SZ, { "Event Log" } },
	{ KEY_EVENTLOG,
		"ErrorControl", REG_DWORD, { (char*)0x00000001 } },
	{ NULL, NULL, 0, { NULL } }
};

/**
 * Initialize a key in the registry:
 * create each component key of the specified path.
 */
static WERROR init_registry_key_internal(struct db_context *db,
					 const char *add_path)
{
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();
	char *path = NULL;
	char *base = NULL;
	char *remaining = NULL;
	char *keyname;
	char *subkeyname;
	struct regsubkey_ctr *subkeys;
	const char *p, *p2;

	DEBUG(6, ("init_registry_key: Adding [%s]\n", add_path));

	path = talloc_strdup(frame, add_path);
	base = talloc_strdup(frame, "");
	if (!path || !base) {
		werr = WERR_NOMEM;
		goto fail;
	}
	p = path;

	while (next_token_talloc(frame, &p, &keyname, "\\")) {

		/* build up the registry path from the components */

		if (*base) {
			base = talloc_asprintf(frame, "%s\\", base);
			if (!base) {
				werr = WERR_NOMEM;
				goto fail;
			}
		}
		base = talloc_asprintf_append(base, "%s", keyname);
		if (!base) {
			werr = WERR_NOMEM;
			goto fail;
		}

		/* get the immediate subkeyname (if we have one ) */

		subkeyname = talloc_strdup(frame, "");
		if (!subkeyname) {
			werr = WERR_NOMEM;
			goto fail;
		}
		if (*p) {
			remaining = talloc_strdup(frame, p);
			if (!remaining) {
				werr = WERR_NOMEM;
				goto fail;
			}
			p2 = remaining;

			if (!next_token_talloc(frame, &p2,
						&subkeyname, "\\"))
			{
				subkeyname = talloc_strdup(frame,p2);
				if (!subkeyname) {
					werr = WERR_NOMEM;
					goto fail;
				}
			}
		}

		DEBUG(10,("init_registry_key: Storing key [%s] with "
			  "subkey [%s]\n", base,
			  *subkeyname ? subkeyname : "NULL"));

		/* we don't really care if the lookup succeeds or not
		 * since we are about to update the record.
		 * We just want any subkeys already present */

		werr = regsubkey_ctr_init(frame, &subkeys);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,("talloc() failure!\n"));
			goto fail;
		}

		werr = regdb_fetch_keys_internal(db, base, subkeys);
		if (!W_ERROR_IS_OK(werr) &&
		    !W_ERROR_EQUAL(werr, WERR_NOT_FOUND))
		{
			goto fail;
		}

		if (*subkeyname) {
			werr = regsubkey_ctr_addkey(subkeys, subkeyname);
			if (!W_ERROR_IS_OK(werr)) {
				goto fail;
			}
		}
		if (!regdb_store_keys_internal(db, base, subkeys)) {
			werr = WERR_CAN_NOT_COMPLETE;
			goto fail;
		}
	}

	werr = WERR_OK;

fail:
	TALLOC_FREE(frame);
	return werr;
}

struct init_registry_key_context {
	const char *add_path;
};

static NTSTATUS init_registry_key_action(struct db_context *db,
					 void *private_data)
{
	struct init_registry_key_context *init_ctx =
		(struct init_registry_key_context *)private_data;

	return werror_to_ntstatus(init_registry_key_internal(
					db, init_ctx->add_path));
}

/**
 * Initialize a key in the registry:
 * create each component key of the specified path,
 * wrapped in one db transaction.
 */
WERROR init_registry_key(const char *add_path)
{
	struct init_registry_key_context init_ctx;

	if (regdb_key_exists(regdb, add_path)) {
		return WERR_OK;
	}

	init_ctx.add_path = add_path;

	return ntstatus_to_werror(dbwrap_trans_do(regdb,
						  init_registry_key_action,
						  &init_ctx));
}

/***********************************************************************
 Open the registry data in the tdb
 ***********************************************************************/

static void regdb_ctr_add_value(struct regval_ctr *ctr,
				struct builtin_regkey_value *value)
{
	switch(value->type) {
	case REG_DWORD:
		regval_ctr_addvalue(ctr, value->valuename, REG_DWORD,
				    (uint8_t *)&value->data.dw_value,
				    sizeof(uint32));
		break;

	case REG_SZ:
		regval_ctr_addvalue_sz(ctr, value->valuename,
				       value->data.string);
		break;

	default:
		DEBUG(0, ("regdb_ctr_add_value: invalid value type in "
			  "registry values [%d]\n", value->type));
	}
}

static NTSTATUS init_registry_data_action(struct db_context *db,
					  void *private_data)
{
	NTSTATUS status;
	TALLOC_CTX *frame = talloc_stackframe();
	struct regval_ctr *values;
	int i;

	/* loop over all of the predefined paths and add each component */

	for (i=0; builtin_registry_paths[i] != NULL; i++) {
		if (regdb_key_exists(db, builtin_registry_paths[i])) {
			continue;
		}
		status = werror_to_ntstatus(init_registry_key_internal(db,
						  builtin_registry_paths[i]));
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
	}

	/* loop over all of the predefined values and add each component */

	for (i=0; builtin_registry_values[i].path != NULL; i++) {
		WERROR werr;

		werr = regval_ctr_init(frame, &values);
		if (!W_ERROR_IS_OK(werr)) {
			status = werror_to_ntstatus(werr);
			goto done;
		}

		regdb_fetch_values_internal(db,
					    builtin_registry_values[i].path,
					    values);

		/* preserve existing values across restarts. Only add new ones */

		if (!regval_ctr_value_exists(values,
					builtin_registry_values[i].valuename))
		{
			regdb_ctr_add_value(values,
					    &builtin_registry_values[i]);
			regdb_store_values_internal(db,
					builtin_registry_values[i].path,
					values);
		}
		TALLOC_FREE(values);
	}

	status = NT_STATUS_OK;

done:

	TALLOC_FREE(frame);
	return status;
}

WERROR init_registry_data(void)
{
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();
	struct regval_ctr *values;
	int i;

	/*
	 * First, check for the existence of the needed keys and values.
	 * If all do already exist, we can save the writes.
	 */
	for (i=0; builtin_registry_paths[i] != NULL; i++) {
		if (!regdb_key_exists(regdb, builtin_registry_paths[i])) {
			goto do_init;
		}
	}

	for (i=0; builtin_registry_values[i].path != NULL; i++) {
		werr = regval_ctr_init(frame, &values);
		W_ERROR_NOT_OK_GOTO_DONE(werr);

		regdb_fetch_values_internal(regdb,
					    builtin_registry_values[i].path,
					    values);
		if (!regval_ctr_value_exists(values,
					builtin_registry_values[i].valuename))
		{
			TALLOC_FREE(values);
			goto do_init;
		}

		TALLOC_FREE(values);
	}

	werr = WERR_OK;
	goto done;

do_init:

	/*
	 * There are potentially quite a few store operations which are all
	 * indiviually wrapped in tdb transactions. Wrapping them in a single
	 * transaction gives just a single transaction_commit() to actually do
	 * its fsync()s. See tdb/common/transaction.c for info about nested
	 * transaction behaviour.
	 */

	werr = ntstatus_to_werror(dbwrap_trans_do(regdb,
						  init_registry_data_action,
						  NULL));

done:
	TALLOC_FREE(frame);
	return werr;
}

static int regdb_normalize_keynames_fn(struct db_record *rec,
				       void *private_data)
{
	TALLOC_CTX *mem_ctx = talloc_tos();
	const char *keyname;
	NTSTATUS status;

	if (rec->key.dptr == NULL || rec->key.dsize == 0) {
		return 0;
	}

	keyname = strchr((const char *) rec->key.dptr, '/');
	if (keyname) {
		struct db_record new_rec;

		keyname = talloc_string_sub(mem_ctx,
					    (const char *) rec->key.dptr,
					    "/",
					    "\\");

		DEBUG(2, ("regdb_normalize_keynames_fn: Convert %s to %s\n",
			  (const char *) rec->key.dptr,
			  keyname));

		new_rec.value = rec->value;
		new_rec.key = string_term_tdb_data(keyname);
		new_rec.private_data = rec->private_data;

		/* Delete the original record and store the normalized key */
		status = rec->delete_rec(rec);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("regdb_normalize_keynames_fn: "
				 "tdb_delete for [%s] failed!\n",
				 rec->key.dptr));
			return 1;
		}

		status = rec->store(&new_rec, new_rec.value, TDB_REPLACE);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("regdb_normalize_keynames_fn: "
				 "failed to store new record for [%s]!\n",
				 keyname));
			return 1;
		}
	}

	return 0;
}

static WERROR regdb_store_regdb_version(uint32_t version)
{
	NTSTATUS status;
	const char *version_keyname = "INFO/version";

	if (!regdb) {
		return WERR_CAN_NOT_COMPLETE;
	}

	status = dbwrap_trans_store_int32(regdb, version_keyname, version);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("regdb_store_regdb_version: error storing %s = %d: %s\n",
			  version_keyname, version, nt_errstr(status)));
		return ntstatus_to_werror(status);
	} else {
		DEBUG(10, ("regdb_store_regdb_version: stored %s = %d\n",
			  version_keyname, version));
		return WERR_OK;
	}
}

static WERROR regdb_upgrade_v1_to_v2(void)
{
	TALLOC_CTX *mem_ctx;
	int rc;
	WERROR werr;

	mem_ctx = talloc_stackframe();
	if (mem_ctx == NULL) {
		return WERR_NOMEM;
	}

	rc = regdb->traverse(regdb, regdb_normalize_keynames_fn, mem_ctx);

	talloc_destroy(mem_ctx);

	if (rc == -1) {
		return WERR_REG_IO_FAILURE;
	}

	werr = regdb_store_regdb_version(REGVER_V2);
	return werr;
}

/***********************************************************************
 Open the registry database
 ***********************************************************************/

WERROR regdb_init(void)
{
	const char *vstring = "INFO/version";
	uint32 vers_id, expected_version;
	WERROR werr;

	if (regdb) {
		DEBUG(10, ("regdb_init: incrementing refcount (%d->%d)\n",
			   regdb_refcount, regdb_refcount+1));
		regdb_refcount++;
		return WERR_OK;
	}

	regdb = db_open(NULL, state_path("registry.tdb"), 0,
			      REG_TDB_FLAGS, O_RDWR, 0600);
	if (!regdb) {
		regdb = db_open(NULL, state_path("registry.tdb"), 0,
				      REG_TDB_FLAGS, O_RDWR|O_CREAT, 0600);
		if (!regdb) {
			werr = ntstatus_to_werror(map_nt_error_from_unix(errno));
			DEBUG(1,("regdb_init: Failed to open registry %s (%s)\n",
				state_path("registry.tdb"), strerror(errno) ));
			return werr;
		}

		DEBUG(10,("regdb_init: Successfully created registry tdb\n"));
	}

	regdb_refcount = 1;
	DEBUG(10, ("regdb_init: registry db openend. refcount reset (%d)\n",
		   regdb_refcount));

	expected_version = REGVER_V2;

	vers_id = dbwrap_fetch_int32(regdb, vstring);
	if (vers_id == -1) {
		DEBUG(10, ("regdb_init: registry version uninitialized "
			   "(got %d), initializing to version %d\n",
			   vers_id, expected_version));

		werr = regdb_store_regdb_version(expected_version);
		return werr;
	}

	if (vers_id > expected_version || vers_id == 0) {
		DEBUG(1, ("regdb_init: unknown registry version %d "
			  "(code version = %d), refusing initialization\n",
			  vers_id, expected_version));
		return WERR_CAN_NOT_COMPLETE;
	}

	if (vers_id == REGVER_V1) {
		DEBUG(10, ("regdb_init: got registry db version %d, upgrading "
			   "to version %d\n", REGVER_V1, REGVER_V2));

		if (regdb->transaction_start(regdb) != 0) {
			return WERR_REG_IO_FAILURE;
		}

		werr = regdb_upgrade_v1_to_v2();
		if (!W_ERROR_IS_OK(werr)) {
			regdb->transaction_cancel(regdb);
			return werr;
		}

		if (regdb->transaction_commit(regdb) != 0) {
			return WERR_REG_IO_FAILURE;
		}

		vers_id = REGVER_V2;
	}

	/* future upgrade code should go here */

	return WERR_OK;
}

/***********************************************************************
 Open the registry.  Must already have been initialized by regdb_init()
 ***********************************************************************/

WERROR regdb_open( void )
{
	WERROR result = WERR_OK;

	if ( regdb ) {
		DEBUG(10, ("regdb_open: incrementing refcount (%d->%d)\n",
			   regdb_refcount, regdb_refcount+1));
		regdb_refcount++;
		return WERR_OK;
	}

	become_root();

	regdb = db_open(NULL, state_path("registry.tdb"), 0,
			      REG_TDB_FLAGS, O_RDWR, 0600);
	if ( !regdb ) {
		result = ntstatus_to_werror( map_nt_error_from_unix( errno ) );
		DEBUG(0,("regdb_open: Failed to open %s! (%s)\n",
			state_path("registry.tdb"), strerror(errno) ));
	}

	unbecome_root();

	regdb_refcount = 1;
	DEBUG(10, ("regdb_open: registry db opened. refcount reset (%d)\n",
		   regdb_refcount));

	return result;
}

/***********************************************************************
 ***********************************************************************/

int regdb_close( void )
{
	if (regdb_refcount == 0) {
		return 0;
	}

	regdb_refcount--;

	DEBUG(10, ("regdb_close: decrementing refcount (%d->%d)\n",
		   regdb_refcount+1, regdb_refcount));

	if ( regdb_refcount > 0 )
		return 0;

	SMB_ASSERT( regdb_refcount >= 0 );

	TALLOC_FREE(regdb);
	return 0;
}

WERROR regdb_transaction_start(void)
{
	return (regdb->transaction_start(regdb) == 0) ?
		WERR_OK : WERR_REG_IO_FAILURE;
}

WERROR regdb_transaction_commit(void)
{
	return (regdb->transaction_commit(regdb) == 0) ?
		WERR_OK : WERR_REG_IO_FAILURE;
}

WERROR regdb_transaction_cancel(void)
{
	return (regdb->transaction_cancel(regdb) == 0) ?
		WERR_OK : WERR_REG_IO_FAILURE;
}

/***********************************************************************
 return the tdb sequence number of the registry tdb.
 this is an indicator for the content of the registry
 having changed. it will change upon regdb_init, too, though.
 ***********************************************************************/
int regdb_get_seqnum(void)
{
	return regdb->get_seqnum(regdb);
}


static WERROR regdb_delete_key_with_prefix(struct db_context *db,
					   const char *keyname,
					   const char *prefix)
{
	char *path;
	WERROR werr = WERR_NOMEM;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	if (keyname == NULL) {
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	if (prefix == NULL) {
		path = discard_const_p(char, keyname);
	} else {
		path = talloc_asprintf(mem_ctx, "%s\\%s", prefix, keyname);
		if (path == NULL) {
			goto done;
		}
	}

	path = normalize_reg_path(mem_ctx, path);
	if (path == NULL) {
		goto done;
	}

	werr = ntstatus_to_werror(dbwrap_delete_bystring(db, path));

	/* treat "not" found" as ok */
	if (W_ERROR_EQUAL(werr, WERR_NOT_FOUND)) {
		werr = WERR_OK;
	}

done:
	talloc_free(mem_ctx);
	return werr;
}


static WERROR regdb_delete_values(struct db_context *db, const char *keyname)
{
	return regdb_delete_key_with_prefix(db, keyname, REG_VALUE_PREFIX);
}

static WERROR regdb_delete_secdesc(struct db_context *db, const char *keyname)
{
	return regdb_delete_key_with_prefix(db, keyname, REG_SECDESC_PREFIX);
}

static WERROR regdb_delete_subkeylist(struct db_context *db, const char *keyname)
{
	return regdb_delete_key_with_prefix(db, keyname, NULL);
}

static WERROR regdb_delete_key_lists(struct db_context *db, const char *keyname)
{
	WERROR werr;

	werr = regdb_delete_values(db, keyname);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(1, (__location__ " Deleting %s\\%s failed: %s\n",
			  REG_VALUE_PREFIX, keyname, win_errstr(werr)));
		goto done;
	}

	werr = regdb_delete_secdesc(db, keyname);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(1, (__location__ " Deleting %s\\%s failed: %s\n",
			  REG_SECDESC_PREFIX, keyname, win_errstr(werr)));
		goto done;
	}

	werr = regdb_delete_subkeylist(db, keyname);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(1, (__location__ " Deleting %s failed: %s\n",
			  keyname, win_errstr(werr)));
		goto done;
	}

done:
	return werr;
}

/***********************************************************************
 Add subkey strings to the registry tdb under a defined key
 fmt is the same format as tdb_pack except this function only supports
 fstrings
 ***********************************************************************/

static WERROR regdb_store_keys_internal2(struct db_context *db,
					 const char *key,
					 struct regsubkey_ctr *ctr)
{
	TDB_DATA dbuf;
	uint8 *buffer = NULL;
	int i = 0;
	uint32 len, buflen;
	uint32 num_subkeys = regsubkey_ctr_numkeys(ctr);
	char *keyname = NULL;
	TALLOC_CTX *ctx = talloc_stackframe();
	WERROR werr;

	if (!key) {
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	keyname = talloc_strdup(ctx, key);
	if (!keyname) {
		werr = WERR_NOMEM;
		goto done;
	}

	keyname = normalize_reg_path(ctx, keyname);
	if (!keyname) {
		werr = WERR_NOMEM;
		goto done;
	}

	/* allocate some initial memory */

	buffer = (uint8 *)SMB_MALLOC(1024);
	if (buffer == NULL) {
		werr = WERR_NOMEM;
		goto done;
	}
	buflen = 1024;
	len = 0;

	/* store the number of subkeys */

	len += tdb_pack(buffer+len, buflen-len, "d", num_subkeys);

	/* pack all the strings */

	for (i=0; i<num_subkeys; i++) {
		size_t thistime;

		thistime = tdb_pack(buffer+len, buflen-len, "f",
				    regsubkey_ctr_specific_key(ctr, i));
		if (len+thistime > buflen) {
			size_t thistime2;
			/*
			 * tdb_pack hasn't done anything because of the short
			 * buffer, allocate extra space.
			 */
			buffer = SMB_REALLOC_ARRAY(buffer, uint8_t,
						   (len+thistime)*2);
			if(buffer == NULL) {
				DEBUG(0, ("regdb_store_keys: Failed to realloc "
					  "memory of size [%u]\n",
					  (unsigned int)(len+thistime)*2));
				werr = WERR_NOMEM;
				goto done;
			}
			buflen = (len+thistime)*2;
			thistime2 = tdb_pack(
				buffer+len, buflen-len, "f",
				regsubkey_ctr_specific_key(ctr, i));
			if (thistime2 != thistime) {
				DEBUG(0, ("tdb_pack failed\n"));
				werr = WERR_CAN_NOT_COMPLETE;
				goto done;
			}
		}
		len += thistime;
	}

	/* finally write out the data */

	dbuf.dptr = buffer;
	dbuf.dsize = len;
	werr = ntstatus_to_werror(dbwrap_store_bystring(db, keyname, dbuf,
							TDB_REPLACE));
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	/*
	 * recreate the sorted subkey cache for regdb_key_exists()
	 */
	werr = ntstatus_to_werror(create_sorted_subkeys(keyname));

done:
	TALLOC_FREE(ctx);
	SAFE_FREE(buffer);
	return werr;
}

/***********************************************************************
 Store the new subkey record and create any child key records that
 do not currently exist
 ***********************************************************************/

struct regdb_store_keys_context {
	const char *key;
	struct regsubkey_ctr *ctr;
};

static NTSTATUS regdb_store_keys_action(struct db_context *db,
					void *private_data)
{
	struct regdb_store_keys_context *store_ctx;
	WERROR werr;
	int num_subkeys, i;
	char *path = NULL;
	struct regsubkey_ctr *subkeys = NULL, *old_subkeys = NULL;
	char *oldkeyname = NULL;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	store_ctx = (struct regdb_store_keys_context *)private_data;

	/*
	 * Re-fetch the old keys inside the transaction
	 */

	werr = regsubkey_ctr_init(mem_ctx, &old_subkeys);
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	werr = regdb_fetch_keys_internal(db, store_ctx->key, old_subkeys);
	if (!W_ERROR_IS_OK(werr) &&
	    !W_ERROR_EQUAL(werr, WERR_NOT_FOUND))
	{
		goto done;
	}

	/*
	 * Make the store operation as safe as possible without transactions:
	 *
	 * (1) For each subkey removed from ctr compared with old_subkeys:
	 *
	 *     (a) First delete the value db entry.
	 *
	 *     (b) Next delete the secdesc db record.
	 *
	 *     (c) Then delete the subkey list entry.
	 *
	 * (2) Now write the list of subkeys of the parent key,
	 *     deleting removed entries and adding new ones.
	 *
	 * (3) Finally create the subkey list entries for the added keys.
	 *
	 * This way if we crash half-way in between deleting the subkeys
	 * and storing the parent's list of subkeys, no old data can pop up
	 * out of the blue when re-adding keys later on.
	 */

	/* (1) delete removed keys' lists (values/secdesc/subkeys) */

	num_subkeys = regsubkey_ctr_numkeys(old_subkeys);
	for (i=0; i<num_subkeys; i++) {
		oldkeyname = regsubkey_ctr_specific_key(old_subkeys, i);

		if (regsubkey_ctr_key_exists(store_ctx->ctr, oldkeyname)) {
			/*
			 * It's still around, don't delete
			 */
			continue;
		}

		path = talloc_asprintf(mem_ctx, "%s\\%s", store_ctx->key,
				       oldkeyname);
		if (!path) {
			werr = WERR_NOMEM;
			goto done;
		}

		werr = regdb_delete_key_lists(db, path);
		W_ERROR_NOT_OK_GOTO_DONE(werr);

		TALLOC_FREE(path);
	}

	TALLOC_FREE(old_subkeys);

	/* (2) store the subkey list for the parent */

	werr = regdb_store_keys_internal2(db, store_ctx->key, store_ctx->ctr);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,("regdb_store_keys: Failed to store new subkey list "
			 "for parent [%s]: %s\n", store_ctx->key,
			 win_errstr(werr)));
		goto done;
	}

	/* (3) now create records for any subkeys that don't already exist */

	num_subkeys = regsubkey_ctr_numkeys(store_ctx->ctr);

	if (num_subkeys == 0) {
		werr = regsubkey_ctr_init(mem_ctx, &subkeys);
		W_ERROR_NOT_OK_GOTO_DONE(werr);

		werr = regdb_store_keys_internal2(db, store_ctx->key, subkeys);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,("regdb_store_keys: Failed to store "
				 "new record for key [%s]: %s\n",
				 store_ctx->key, win_errstr(werr)));
			goto done;
		}
		TALLOC_FREE(subkeys);
	}

	for (i=0; i<num_subkeys; i++) {
		path = talloc_asprintf(mem_ctx, "%s\\%s", store_ctx->key,
				regsubkey_ctr_specific_key(store_ctx->ctr, i));
		if (!path) {
			werr = WERR_NOMEM;
			goto done;
		}
		werr = regsubkey_ctr_init(mem_ctx, &subkeys);
		W_ERROR_NOT_OK_GOTO_DONE(werr);

		werr = regdb_fetch_keys_internal(db, path, subkeys);
		if (!W_ERROR_IS_OK(werr)) {
			/* create a record with 0 subkeys */
			werr = regdb_store_keys_internal2(db, path, subkeys);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(0,("regdb_store_keys: Failed to store "
					 "new record for key [%s]: %s\n", path,
					 win_errstr(werr)));
				goto done;
			}
		}

		TALLOC_FREE(subkeys);
		TALLOC_FREE(path);
	}

	/*
	 * Update the seqnum in the container to possibly
	 * prevent next read from going to disk
	 */
	werr = regsubkey_ctr_set_seqnum(store_ctx->ctr, db->get_seqnum(db));

done:
	talloc_free(mem_ctx);
	return werror_to_ntstatus(werr);
}

static bool regdb_store_keys_internal(struct db_context *db, const char *key,
				      struct regsubkey_ctr *ctr)
{
	int num_subkeys, old_num_subkeys, i;
	struct regsubkey_ctr *old_subkeys = NULL;
	TALLOC_CTX *ctx = talloc_stackframe();
	WERROR werr;
	bool ret = false;
	struct regdb_store_keys_context store_ctx;

	if (!regdb_key_is_base_key(key) && !regdb_key_exists(db, key)) {
		goto done;
	}

	/*
	 * fetch a list of the old subkeys so we can determine if anything has
	 * changed
	 */

	werr = regsubkey_ctr_init(ctx, &old_subkeys);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,("regdb_store_keys: talloc() failure!\n"));
		goto done;
	}

	werr = regdb_fetch_keys_internal(db, key, old_subkeys);
	if (!W_ERROR_IS_OK(werr) &&
	    !W_ERROR_EQUAL(werr, WERR_NOT_FOUND))
	{
		goto done;
	}

	num_subkeys = regsubkey_ctr_numkeys(ctr);
	old_num_subkeys = regsubkey_ctr_numkeys(old_subkeys);
	if ((num_subkeys && old_num_subkeys) &&
	    (num_subkeys == old_num_subkeys)) {

		for (i = 0; i < num_subkeys; i++) {
			if (strcmp(regsubkey_ctr_specific_key(ctr, i),
				   regsubkey_ctr_specific_key(old_subkeys, i))
			    != 0)
			{
				break;
			}
		}
		if (i == num_subkeys) {
			/*
			 * Nothing changed, no point to even start a tdb
			 * transaction
			 */

			ret = true;
			goto done;
		}
	}

	TALLOC_FREE(old_subkeys);

	store_ctx.key = key;
	store_ctx.ctr = ctr;

	werr = ntstatus_to_werror(dbwrap_trans_do(db,
						  regdb_store_keys_action,
						  &store_ctx));

	ret = W_ERROR_IS_OK(werr);

done:
	TALLOC_FREE(ctx);

	return ret;
}

bool regdb_store_keys(const char *key, struct regsubkey_ctr *ctr)
{
	return regdb_store_keys_internal(regdb, key, ctr);
}

/**
 * create a subkey of a given key
 */

struct regdb_create_subkey_context {
	const char *key;
	const char *subkey;
};

static NTSTATUS regdb_create_subkey_action(struct db_context *db,
					   void *private_data)
{
	WERROR werr;
	struct regdb_create_subkey_context *create_ctx;
	struct regsubkey_ctr *subkeys;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	create_ctx = (struct regdb_create_subkey_context *)private_data;

	werr = regsubkey_ctr_init(mem_ctx, &subkeys);
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	werr = regdb_fetch_keys_internal(db, create_ctx->key, subkeys);
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	werr = regsubkey_ctr_addkey(subkeys, create_ctx->subkey);
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	werr = regdb_store_keys_internal2(db, create_ctx->key, subkeys);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, (__location__ " failed to store new subkey list for "
			 "parent key %s: %s\n", create_ctx->key,
			 win_errstr(werr)));
	}

done:
	talloc_free(mem_ctx);
	return werror_to_ntstatus(werr);
}

static WERROR regdb_create_subkey(const char *key, const char *subkey)
{
	WERROR werr;
	struct regsubkey_ctr *subkeys;
	TALLOC_CTX *mem_ctx = talloc_stackframe();
	struct regdb_create_subkey_context create_ctx;

	if (!regdb_key_is_base_key(key) && !regdb_key_exists(regdb, key)) {
		werr = WERR_NOT_FOUND;
		goto done;
	}

	werr = regsubkey_ctr_init(mem_ctx, &subkeys);
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	werr = regdb_fetch_keys_internal(regdb, key, subkeys);
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	if (regsubkey_ctr_key_exists(subkeys, subkey)) {
		werr = WERR_OK;
		goto done;
	}

	talloc_free(subkeys);

	create_ctx.key = key;
	create_ctx.subkey = subkey;

	werr = ntstatus_to_werror(dbwrap_trans_do(regdb,
						  regdb_create_subkey_action,
						  &create_ctx));

done:
	talloc_free(mem_ctx);
	return werr;
}

/**
 * create a subkey of a given key
 */

struct regdb_delete_subkey_context {
	const char *key;
	const char *subkey;
	const char *path;
};

static NTSTATUS regdb_delete_subkey_action(struct db_context *db,
					   void *private_data)
{
	WERROR werr;
	struct regdb_delete_subkey_context *delete_ctx;
	struct regsubkey_ctr *subkeys;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	delete_ctx = (struct regdb_delete_subkey_context *)private_data;

	werr = regdb_delete_key_lists(db, delete_ctx->path);
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	werr = regsubkey_ctr_init(mem_ctx, &subkeys);
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	werr = regdb_fetch_keys_internal(db, delete_ctx->key, subkeys);
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	werr = regsubkey_ctr_delkey(subkeys, delete_ctx->subkey);
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	werr = regdb_store_keys_internal2(db, delete_ctx->key, subkeys);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, (__location__ " failed to store new subkey_list for "
			 "parent key %s: %s\n", delete_ctx->key,
			 win_errstr(werr)));
	}

done:
	talloc_free(mem_ctx);
	return werror_to_ntstatus(werr);
}

static WERROR regdb_delete_subkey(const char *key, const char *subkey)
{
	WERROR werr;
	char *path;
	struct regdb_delete_subkey_context delete_ctx;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	if (!regdb_key_is_base_key(key) && !regdb_key_exists(regdb, key)) {
		werr = WERR_NOT_FOUND;
		goto done;
	}

	path = talloc_asprintf(mem_ctx, "%s\\%s", key, subkey);
	if (path == NULL) {
		werr = WERR_NOMEM;
		goto done;
	}

	if (!regdb_key_exists(regdb, path)) {
		werr = WERR_OK;
		goto done;
	}

	delete_ctx.key = key;
	delete_ctx.subkey = subkey;
	delete_ctx.path = path;

	werr = ntstatus_to_werror(dbwrap_trans_do(regdb,
						  regdb_delete_subkey_action,
						  &delete_ctx));

done:
	talloc_free(mem_ctx);
	return werr;
}

static TDB_DATA regdb_fetch_key_internal(struct db_context *db,
					 TALLOC_CTX *mem_ctx, const char *key)
{
	char *path = NULL;
	TDB_DATA data;

	path = normalize_reg_path(mem_ctx, key);
	if (!path) {
		return make_tdb_data(NULL, 0);
	}

	data = dbwrap_fetch_bystring(db, mem_ctx, path);

	TALLOC_FREE(path);
	return data;
}


/**
 * check whether a given key name represents a base key,
 * i.e one without a subkey separator ('\').
 */
static bool regdb_key_is_base_key(const char *key)
{
	TALLOC_CTX *mem_ctx = talloc_stackframe();
	bool ret = false;
	char *path;

	if (key == NULL) {
		goto done;
	}

	path = normalize_reg_path(mem_ctx, key);
	if (path == NULL) {
		DEBUG(0, ("out of memory! (talloc failed)\n"));
		goto done;
	}

	if (*path == '\0') {
		goto done;
	}

	ret = (strrchr(path, '\\') == NULL);

done:
	TALLOC_FREE(mem_ctx);
	return ret;
}

/*
 * regdb_key_exists() is a very frequent operation. It can be quite
 * time-consuming to fully fetch the parent's subkey list, talloc_strdup all
 * subkeys and then compare the keyname linearly to all the parent's subkeys.
 *
 * The following code tries to make this operation as efficient as possible:
 * Per registry key we create a list of subkeys that is very efficient to
 * search for existence of a subkey. Its format is:
 *
 * 4 bytes num_subkeys
 * 4*num_subkey bytes offset into the string array
 * then follows a sorted list of subkeys in uppercase
 *
 * This record is created by create_sorted_subkeys() on demand if it does not
 * exist. scan_parent_subkeys() uses regdb->parse_record to search the sorted
 * list, the parsing code and the binary search can be found in
 * parent_subkey_scanner. The code uses parse_record() to avoid a memcpy of
 * the potentially large subkey record.
 *
 * The sorted subkey record is deleted in regdb_store_keys_internal2 and
 * recreated on demand.
 */

static int cmp_keynames(char **p1, char **p2)
{
	return StrCaseCmp(*p1, *p2);
}

struct create_sorted_subkeys_context {
	const char *key;
	const char *sorted_keyname;
};

static NTSTATUS create_sorted_subkeys_action(struct db_context *db,
					     void *private_data)
{
	char **sorted_subkeys;
	struct regsubkey_ctr *ctr;
	NTSTATUS status;
	char *buf;
	char *p;
	int i;
	size_t len;
	int num_subkeys;
	struct create_sorted_subkeys_context *sorted_ctx;

	sorted_ctx = (struct create_sorted_subkeys_context *)private_data;

	/*
	 * In this function, we only treat failing of the actual write to
	 * the db as a real error. All preliminary errors, at a stage when
	 * nothing has been written to the DB yet are treated as success
	 * to be committed (as an empty transaction).
	 *
	 * The reason is that this (disposable) call might be nested in other
	 * transactions. Doing a cancel here would destroy the possibility of
	 * a transaction_commit for transactions that we might be wrapped in.
	 */

	status = werror_to_ntstatus(regsubkey_ctr_init(talloc_tos(), &ctr));
	if (!NT_STATUS_IS_OK(status)) {
		/* don't treat this as an error */
		status = NT_STATUS_OK;
		goto done;
	}

	status = werror_to_ntstatus(regdb_fetch_keys_internal(db,
							      sorted_ctx->key,
							      ctr));
	if (!NT_STATUS_IS_OK(status)) {
		/* don't treat this as an error */
		status = NT_STATUS_OK;
		goto done;
	}

	num_subkeys = regsubkey_ctr_numkeys(ctr);
	sorted_subkeys = talloc_array(ctr, char *, num_subkeys);
	if (sorted_subkeys == NULL) {
		/* don't treat this as an error */
		goto done;
	}

	len = 4 + 4*num_subkeys;

	for (i = 0; i < num_subkeys; i++) {
		sorted_subkeys[i] = talloc_strdup_upper(sorted_subkeys,
					regsubkey_ctr_specific_key(ctr, i));
		if (sorted_subkeys[i] == NULL) {
			/* don't treat this as an error */
			goto done;
		}
		len += strlen(sorted_subkeys[i])+1;
	}

	TYPESAFE_QSORT(sorted_subkeys, num_subkeys, cmp_keynames);

	buf = talloc_array(ctr, char, len);
	if (buf == NULL) {
		/* don't treat this as an error */
		goto done;
	}
	p = buf + 4 + 4*num_subkeys;

	SIVAL(buf, 0, num_subkeys);

	for (i=0; i < num_subkeys; i++) {
		ptrdiff_t offset = p - buf;
		SIVAL(buf, 4 + 4*i, offset);
		strlcpy(p, sorted_subkeys[i], len-offset);
		p += strlen(sorted_subkeys[i]) + 1;
	}

	status = dbwrap_store_bystring(
		db, sorted_ctx->sorted_keyname, make_tdb_data((uint8_t *)buf,
		len),
		TDB_REPLACE);

done:
	talloc_free(ctr);
	return status;
}

static NTSTATUS create_sorted_subkeys_internal(const char *key,
					       const char *sorted_keyname)
{
	NTSTATUS status;
	struct create_sorted_subkeys_context sorted_ctx;

	sorted_ctx.key = key;
	sorted_ctx.sorted_keyname = sorted_keyname;

	status = dbwrap_trans_do(regdb,
				 create_sorted_subkeys_action,
				 &sorted_ctx);

	return status;
}

static NTSTATUS create_sorted_subkeys(const char *key)
{
	char *sorted_subkeys_keyname;
	NTSTATUS status;

	sorted_subkeys_keyname = talloc_asprintf(talloc_tos(), "%s\\%s",
						 REG_SORTED_SUBKEYS_PREFIX,
						 key);
	if (sorted_subkeys_keyname == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	status = create_sorted_subkeys_internal(key, sorted_subkeys_keyname);

done:
	return status;
}

struct scan_subkey_state {
	char *name;
	bool scanned;
	bool found;
};

static int parent_subkey_scanner(TDB_DATA key, TDB_DATA data,
				 void *private_data)
{
	struct scan_subkey_state *state =
		(struct scan_subkey_state *)private_data;
	uint32_t num_subkeys;
	uint32_t l, u;

	if (data.dsize < sizeof(uint32_t)) {
		return -1;
	}

	state->scanned = true;
	state->found = false;

	tdb_unpack(data.dptr, data.dsize, "d", &num_subkeys);

	l = 0;
	u = num_subkeys;

	while (l < u) {
		uint32_t idx = (l+u)/2;
		char *s = (char *)data.dptr + IVAL(data.dptr, 4 + 4*idx);
		int comparison = strcmp(state->name, s);

		if (comparison < 0) {
			u = idx;
		} else if (comparison > 0) {
			l = idx + 1;
		} else {
			state->found = true;
			return 0;
		}
	}
	return 0;
}

static bool scan_parent_subkeys(struct db_context *db, const char *parent,
				const char *name)
{
	char *path = NULL;
	char *key = NULL;
	struct scan_subkey_state state = { 0, };
	bool result = false;
	int res;

	state.name = NULL;

	path = normalize_reg_path(talloc_tos(), parent);
	if (path == NULL) {
		goto fail;
	}

	key = talloc_asprintf(talloc_tos(), "%s\\%s",
			      REG_SORTED_SUBKEYS_PREFIX, path);
	if (key == NULL) {
		goto fail;
	}

	state.name = talloc_strdup_upper(talloc_tos(), name);
	if (state.name == NULL) {
		goto fail;
	}
	state.scanned = false;

	res = db->parse_record(db, string_term_tdb_data(key),
			       parent_subkey_scanner, &state);

	if (state.scanned) {
		result = state.found;
	} else {
		NTSTATUS status;

		res = db->transaction_start(db);
		if (res != 0) {
			DEBUG(0, ("error starting transaction\n"));
			goto fail;
		}

		DEBUG(2, (__location__ " WARNING: recreating the sorted "
			  "subkeys cache for key '%s' from scan_parent_subkeys "
			  "this should not happen (too frequently)...\n",
			  path));

		status = create_sorted_subkeys_internal(path, key);
		if (!NT_STATUS_IS_OK(status)) {
			res = db->transaction_cancel(db);
			if (res != 0) {
				smb_panic("Failed to cancel transaction.");
			}
			goto fail;
		}

		res = db->parse_record(db, string_term_tdb_data(key),
				       parent_subkey_scanner, &state);
		if ((res == 0) && (state.scanned)) {
			result = state.found;
		}

		res = db->transaction_commit(db);
		if (res != 0) {
			DEBUG(0, ("error committing transaction\n"));
			result = false;
		}
	}

 fail:
	TALLOC_FREE(path);
	TALLOC_FREE(state.name);
	return result;
}

/**
 * Check for the existence of a key.
 *
 * Existence of a key is authoritatively defined by its
 * existence in the list of subkeys of its parent key.
 * The exeption of this are keys without a parent key,
 * i.e. the "base" keys (HKLM, HKCU, ...).
 */
static bool regdb_key_exists(struct db_context *db, const char *key)
{
	TALLOC_CTX *mem_ctx = talloc_stackframe();
	TDB_DATA value;
	bool ret = false;
	char *path, *p;

	if (key == NULL) {
		goto done;
	}

	path = normalize_reg_path(mem_ctx, key);
	if (path == NULL) {
		DEBUG(0, ("out of memory! (talloc failed)\n"));
		goto done;
	}

	if (*path == '\0') {
		goto done;
	}

	p = strrchr(path, '\\');
	if (p == NULL) {
		/* this is a base key */
		value = regdb_fetch_key_internal(db, mem_ctx, path);
		ret = (value.dptr != NULL);
	} else {
		*p = '\0';
		ret = scan_parent_subkeys(db, path, p+1);
	}

done:
	TALLOC_FREE(mem_ctx);
	return ret;
}


/***********************************************************************
 Retrieve an array of strings containing subkeys.  Memory should be
 released by the caller.
 ***********************************************************************/

static WERROR regdb_fetch_keys_internal(struct db_context *db, const char *key,
					struct regsubkey_ctr *ctr)
{
	WERROR werr;
	uint32_t num_items;
	uint8 *buf;
	uint32 buflen, len;
	int i;
	fstring subkeyname;
	TALLOC_CTX *frame = talloc_stackframe();
	TDB_DATA value;
	int seqnum[2], count;

	DEBUG(11,("regdb_fetch_keys: Enter key => [%s]\n", key ? key : "NULL"));

	if (!regdb_key_exists(db, key)) {
		DEBUG(10, ("key [%s] not found\n", key));
		werr = WERR_NOT_FOUND;
		goto done;
	}

	werr = regsubkey_ctr_reinit(ctr);
	W_ERROR_NOT_OK_GOTO_DONE(werr);

	count = 0;
	ZERO_STRUCT(value);
	seqnum[0] = db->get_seqnum(db);

	do {
		count++;
		TALLOC_FREE(value.dptr);
		value = regdb_fetch_key_internal(db, frame, key);
		seqnum[count % 2] = db->get_seqnum(db);

	} while (seqnum[0] != seqnum[1]);

	if (count > 1) {
		DEBUG(5, ("regdb_fetch_keys_internal: it took %d attempts to "
			  "fetch key '%s' with constant seqnum\n",
			  count, key));
	}

	werr = regsubkey_ctr_set_seqnum(ctr, seqnum[0]);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	if (value.dsize == 0 || value.dptr == NULL) {
		DEBUG(10, ("regdb_fetch_keys: no subkeys found for key [%s]\n",
			   key));
		goto done;
	}

	buf = value.dptr;
	buflen = value.dsize;
	len = tdb_unpack( buf, buflen, "d", &num_items);
	if (len == (uint32_t)-1) {
		werr = WERR_NOT_FOUND;
		goto done;
	}

	for (i=0; i<num_items; i++) {
		len += tdb_unpack(buf+len, buflen-len, "f", subkeyname);
		werr = regsubkey_ctr_addkey(ctr, subkeyname);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(5, ("regdb_fetch_keys: regsubkey_ctr_addkey "
				  "failed: %s\n", win_errstr(werr)));
			num_items = 0;
			goto done;
		}
	}

	DEBUG(11,("regdb_fetch_keys: Exit [%d] items\n", num_items));

done:
	TALLOC_FREE(frame);
	return werr;
}

int regdb_fetch_keys(const char *key, struct regsubkey_ctr *ctr)
{
	WERROR werr;

	werr = regdb_fetch_keys_internal(regdb, key, ctr);
	if (!W_ERROR_IS_OK(werr)) {
		return -1;
	}

	return regsubkey_ctr_numkeys(ctr);
}

/****************************************************************************
 Unpack a list of registry values frem the TDB
 ***************************************************************************/

static int regdb_unpack_values(struct regval_ctr *values, uint8 *buf, int buflen)
{
	int 		len = 0;
	uint32		type;
	fstring valuename;
	uint32		size;
	uint8		*data_p;
	uint32 		num_values = 0;
	int 		i;

	/* loop and unpack the rest of the registry values */

	len += tdb_unpack(buf+len, buflen-len, "d", &num_values);

	for ( i=0; i<num_values; i++ ) {
		/* unpack the next regval */

		type = REG_NONE;
		size = 0;
		data_p = NULL;
		valuename[0] = '\0';
		len += tdb_unpack(buf+len, buflen-len, "fdB",
				  valuename,
				  &type,
				  &size,
				  &data_p);

		regval_ctr_addvalue(values, valuename, type,
				(uint8_t *)data_p, size);
		SAFE_FREE(data_p); /* 'B' option to tdb_unpack does a malloc() */

		DEBUG(10, ("regdb_unpack_values: value[%d]: name[%s] len[%d]\n",
			   i, valuename, size));
	}

	return len;
}

/****************************************************************************
 Pack all values in all printer keys
 ***************************************************************************/

static int regdb_pack_values(struct regval_ctr *values, uint8 *buf, int buflen)
{
	int 		len = 0;
	int 		i;
	struct regval_blob	*val;
	int		num_values;

	if ( !values )
		return 0;

	num_values = regval_ctr_numvals( values );

	/* pack the number of values first */

	len += tdb_pack( buf+len, buflen-len, "d", num_values );

	/* loop over all values */

	for ( i=0; i<num_values; i++ ) {
		val = regval_ctr_specific_value( values, i );
		len += tdb_pack(buf+len, buflen-len, "fdB",
				regval_name(val),
				regval_type(val),
				regval_size(val),
				regval_data_p(val) );
	}

	return len;
}

/***********************************************************************
 Retrieve an array of strings containing subkeys.  Memory should be
 released by the caller.
 ***********************************************************************/

static int regdb_fetch_values_internal(struct db_context *db, const char* key,
				       struct regval_ctr *values)
{
	char *keystr = NULL;
	TALLOC_CTX *ctx = talloc_stackframe();
	int ret = 0;
	TDB_DATA value;
	WERROR werr;
	int seqnum[2], count;

	DEBUG(10,("regdb_fetch_values: Looking for values of key [%s]\n", key));

	if (!regdb_key_exists(db, key)) {
		DEBUG(10, ("regb_fetch_values: key [%s] does not exist\n",
			   key));
		ret = -1;
		goto done;
	}

	keystr = talloc_asprintf(ctx, "%s\\%s", REG_VALUE_PREFIX, key);
	if (!keystr) {
		goto done;
	}

	ZERO_STRUCT(value);
	count = 0;
	seqnum[0] = db->get_seqnum(db);

	do {
		count++;
		TALLOC_FREE(value.dptr);
		value = regdb_fetch_key_internal(db, ctx, keystr);
		seqnum[count % 2] = db->get_seqnum(db);
	} while (seqnum[0] != seqnum[1]);

	if (count > 1) {
		DEBUG(5, ("regdb_fetch_values_internal: it took %d attempts "
			  "to fetch key '%s' with constant seqnum\n",
			  count, key));
	}

	werr = regval_ctr_set_seqnum(values, seqnum[0]);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	if (!value.dptr) {
		/* all keys have zero values by default */
		goto done;
	}

	regdb_unpack_values(values, value.dptr, value.dsize);
	ret = regval_ctr_numvals(values);

done:
	TALLOC_FREE(ctx);
	return ret;
}

int regdb_fetch_values(const char* key, struct regval_ctr *values)
{
	return regdb_fetch_values_internal(regdb, key, values);
}

static bool regdb_store_values_internal(struct db_context *db, const char *key,
					struct regval_ctr *values)
{
	TDB_DATA old_data, data;
	char *keystr = NULL;
	TALLOC_CTX *ctx = talloc_stackframe();
	int len;
	NTSTATUS status;
	bool result = false;
	WERROR werr;

	DEBUG(10,("regdb_store_values: Looking for values of key [%s]\n", key));

	if (!regdb_key_exists(db, key)) {
		goto done;
	}

	ZERO_STRUCT(data);

	len = regdb_pack_values(values, data.dptr, data.dsize);
	if (len <= 0) {
		DEBUG(0,("regdb_store_values: unable to pack values. len <= 0\n"));
		goto done;
	}

	data.dptr = TALLOC_ARRAY(ctx, uint8, len);
	data.dsize = len;

	len = regdb_pack_values(values, data.dptr, data.dsize);

	SMB_ASSERT( len == data.dsize );

	keystr = talloc_asprintf(ctx, "%s\\%s", REG_VALUE_PREFIX, key );
	if (!keystr) {
		goto done;
	}
	keystr = normalize_reg_path(ctx, keystr);
	if (!keystr) {
		goto done;
	}

	old_data = dbwrap_fetch_bystring(db, ctx, keystr);

	if ((old_data.dptr != NULL)
	    && (old_data.dsize == data.dsize)
	    && (memcmp(old_data.dptr, data.dptr, data.dsize) == 0))
	{
		result = true;
		goto done;
	}

	status = dbwrap_trans_store_bystring(db, keystr, data, TDB_REPLACE);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("regdb_store_values_internal: error storing: %s\n", nt_errstr(status)));
		goto done;
	}

	/*
	 * update the seqnum in the cache to prevent the next read
	 * from going to disk
	 */
	werr = regval_ctr_set_seqnum(values, db->get_seqnum(db));
	result = W_ERROR_IS_OK(status);

done:
	TALLOC_FREE(ctx);
	return result;
}

bool regdb_store_values(const char *key, struct regval_ctr *values)
{
	return regdb_store_values_internal(regdb, key, values);
}

static WERROR regdb_get_secdesc(TALLOC_CTX *mem_ctx, const char *key,
				struct security_descriptor **psecdesc)
{
	char *tdbkey;
	TDB_DATA data;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();
	WERROR err = WERR_OK;

	DEBUG(10, ("regdb_get_secdesc: Getting secdesc of key [%s]\n", key));

	if (!regdb_key_exists(regdb, key)) {
		err = WERR_BADFILE;
		goto done;
	}

	tdbkey = talloc_asprintf(tmp_ctx, "%s\\%s", REG_SECDESC_PREFIX, key);
	if (tdbkey == NULL) {
		err = WERR_NOMEM;
		goto done;
	}

	tdbkey = normalize_reg_path(tmp_ctx, tdbkey);
	if (tdbkey == NULL) {
		err = WERR_NOMEM;
		goto done;
	}

	data = dbwrap_fetch_bystring(regdb, tmp_ctx, tdbkey);
	if (data.dptr == NULL) {
		err = WERR_BADFILE;
		goto done;
	}

	status = unmarshall_sec_desc(mem_ctx, (uint8 *)data.dptr, data.dsize,
				     psecdesc);

	if (NT_STATUS_EQUAL(status, NT_STATUS_NO_MEMORY)) {
		err = WERR_NOMEM;
	} else if (!NT_STATUS_IS_OK(status)) {
		err = WERR_REG_CORRUPT;
	}

done:
	TALLOC_FREE(tmp_ctx);
	return err;
}

static WERROR regdb_set_secdesc(const char *key,
				struct security_descriptor *secdesc)
{
	TALLOC_CTX *mem_ctx = talloc_stackframe();
	char *tdbkey;
	WERROR err = WERR_NOMEM;
	TDB_DATA tdbdata;

	if (!regdb_key_exists(regdb, key)) {
		err = WERR_BADFILE;
		goto done;
	}

	tdbkey = talloc_asprintf(mem_ctx, "%s\\%s", REG_SECDESC_PREFIX, key);
	if (tdbkey == NULL) {
		goto done;
	}

	tdbkey = normalize_reg_path(mem_ctx, tdbkey);
	if (tdbkey == NULL) {
		err = WERR_NOMEM;
		goto done;
	}

	if (secdesc == NULL) {
		/* assuming a delete */
		err = ntstatus_to_werror(dbwrap_trans_delete_bystring(regdb,
								      tdbkey));
		goto done;
	}

	err = ntstatus_to_werror(marshall_sec_desc(mem_ctx, secdesc,
						   &tdbdata.dptr,
						   &tdbdata.dsize));
	W_ERROR_NOT_OK_GOTO_DONE(err);

	err = ntstatus_to_werror(dbwrap_trans_store_bystring(regdb, tdbkey,
							     tdbdata, 0));

 done:
	TALLOC_FREE(mem_ctx);
	return err;
}

bool regdb_subkeys_need_update(struct regsubkey_ctr *subkeys)
{
	return (regdb_get_seqnum() != regsubkey_ctr_get_seqnum(subkeys));
}

bool regdb_values_need_update(struct regval_ctr *values)
{
	return (regdb_get_seqnum() != regval_ctr_get_seqnum(values));
}

/*
 * Table of function pointers for default access
 */

struct registry_ops regdb_ops = {
	.fetch_subkeys = regdb_fetch_keys,
	.fetch_values = regdb_fetch_values,
	.store_subkeys = regdb_store_keys,
	.store_values = regdb_store_values,
	.create_subkey = regdb_create_subkey,
	.delete_subkey = regdb_delete_subkey,
	.get_secdesc = regdb_get_secdesc,
	.set_secdesc = regdb_set_secdesc,
	.subkeys_need_update = regdb_subkeys_need_update,
	.values_need_update = regdb_values_need_update
};
