/*
 *  Unix SMB/CIFS implementation.
 *  account policy storage
 *  Copyright (C) Jean François Micouleau      1998-2001.
 *  Copyright (C) Andrew Bartlett              2002
 *  Copyright (C) Guenther Deschner            2004-2005
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
#include "dbwrap.h"
#include "../libcli/security/security.h"
#include "lib/privileges.h"

static struct db_context *db;

/* cache all entries for 60 seconds for to save ldap-queries (cache is updated
 * after this period if admins do not use pdbedit or usermanager but manipulate
 * ldap directly) - gd */

#define DATABASE_VERSION 	3
#define AP_TTL			60


struct ap_table {
	enum pdb_policy_type type;
	const char *string;
	uint32 default_val;
	const char *description;
	const char *ldap_attr;
};

static const struct ap_table account_policy_names[] = {
	{PDB_POLICY_MIN_PASSWORD_LEN, "min password length", MINPASSWDLENGTH,
		"Minimal password length (default: 5)",
		"sambaMinPwdLength" },

	{PDB_POLICY_PASSWORD_HISTORY, "password history", 0,
		"Length of Password History Entries (default: 0 => off)",
		"sambaPwdHistoryLength" },

	{PDB_POLICY_USER_MUST_LOGON_TO_CHG_PASS, "user must logon to change password", 0,
		"Force Users to logon for password change (default: 0 => off, 2 => on)",
		"sambaLogonToChgPwd" },

	{PDB_POLICY_MAX_PASSWORD_AGE, "maximum password age", (uint32) -1,
		"Maximum password age, in seconds (default: -1 => never expire passwords)",
		"sambaMaxPwdAge" },

	{PDB_POLICY_MIN_PASSWORD_AGE,"minimum password age", 0,
		"Minimal password age, in seconds (default: 0 => allow immediate password change)",
		"sambaMinPwdAge" },

	{PDB_POLICY_LOCK_ACCOUNT_DURATION, "lockout duration", 30,
		"Lockout duration in minutes (default: 30, -1 => forever)",
		"sambaLockoutDuration" },

	{PDB_POLICY_RESET_COUNT_TIME, "reset count minutes", 30,
		"Reset time after lockout in minutes (default: 30)",
		"sambaLockoutObservationWindow" },

	{PDB_POLICY_BAD_ATTEMPT_LOCKOUT, "bad lockout attempt", 0,
		"Lockout users after bad logon attempts (default: 0 => off)",
		"sambaLockoutThreshold" },

	{PDB_POLICY_TIME_TO_LOGOUT, "disconnect time", (uint32) -1,
		"Disconnect Users outside logon hours (default: -1 => off, 0 => on)",
		"sambaForceLogoff" },

	{PDB_POLICY_REFUSE_MACHINE_PW_CHANGE, "refuse machine password change", 0,
		"Allow Machine Password changes (default: 0 => off)",
		"sambaRefuseMachinePwdChange" },

	{0, NULL, 0, "", NULL}
};

void account_policy_names_list(const char ***names, int *num_names)
{
	const char **nl;
	int i, count;

	for (count=0; account_policy_names[count].string; count++) {
	}
	nl = SMB_MALLOC_ARRAY(const char *, count);
	if (!nl) {
		*num_names = 0;
		return;
	}
	for (i=0; account_policy_names[i].string; i++) {
		nl[i] = account_policy_names[i].string;
	}
	*num_names = count;
	*names = nl;
	return;
}

/****************************************************************************
Get the account policy name as a string from its #define'ed number
****************************************************************************/

const char *decode_account_policy_name(enum pdb_policy_type type)
{
	int i;
	for (i=0; account_policy_names[i].string; i++) {
		if (type == account_policy_names[i].type) {
			return account_policy_names[i].string;
		}
	}
	return NULL;
}

/****************************************************************************
Get the account policy LDAP attribute as a string from its #define'ed number
****************************************************************************/

const char *get_account_policy_attr(enum pdb_policy_type type)
{
	int i;
	for (i=0; account_policy_names[i].type; i++) {
		if (type == account_policy_names[i].type) {
			return account_policy_names[i].ldap_attr;
		}
	}
	return NULL;
}

/****************************************************************************
Get the account policy description as a string from its #define'ed number
****************************************************************************/

const char *account_policy_get_desc(enum pdb_policy_type type)
{
	int i;
	for (i=0; account_policy_names[i].string; i++) {
		if (type == account_policy_names[i].type) {
			return account_policy_names[i].description;
		}
	}
	return NULL;
}

/****************************************************************************
Get the account policy name as a string from its #define'ed number
****************************************************************************/

enum pdb_policy_type account_policy_name_to_typenum(const char *name)
{
	int i;
	for (i=0; account_policy_names[i].string; i++) {
		if (strcmp(name, account_policy_names[i].string) == 0) {
			return account_policy_names[i].type;
		}
	}
	return 0;
}

/*****************************************************************************
Get default value for account policy
*****************************************************************************/

bool account_policy_get_default(enum pdb_policy_type type, uint32_t *val)
{
	int i;
	for (i=0; account_policy_names[i].type; i++) {
		if (account_policy_names[i].type == type) {
			*val = account_policy_names[i].default_val;
			return True;
		}
	}
	DEBUG(0,("no default for account_policy index %d found. This should never happen\n",
		type));
	return False;
}

/*****************************************************************************
 Set default for a type if it is empty
*****************************************************************************/

static bool account_policy_set_default_on_empty(enum pdb_policy_type type)
{

	uint32 value;

	if (!account_policy_get(type, &value) &&
	    !account_policy_get_default(type, &value)) {
		return False;
	}

	return account_policy_set(type, value);
}

/*****************************************************************************
 Open the account policy tdb.
***`*************************************************************************/

bool init_account_policy(void)
{

	const char *vstring = "INFO/version";
	uint32 version;
	int i;

	if (db != NULL) {
		return True;
	}

	db = db_open(NULL, state_path("account_policy.tdb"), 0, TDB_DEFAULT,
		     O_RDWR, 0600);

	if (db == NULL) { /* the account policies files does not exist or open
			   * failed, try to create a new one */
		db = db_open(NULL, state_path("account_policy.tdb"), 0,
			     TDB_DEFAULT, O_RDWR|O_CREAT, 0600);
		if (db == NULL) {
			DEBUG(0,("Failed to open account policy database\n"));
			return False;
		}
	}

	version = dbwrap_fetch_int32(db, vstring);
	if (version == DATABASE_VERSION) {
		return true;
	}

	/* handle a Samba upgrade */

	if (db->transaction_start(db) != 0) {
		DEBUG(0, ("transaction_start failed\n"));
		TALLOC_FREE(db);
		return false;
	}

	version = dbwrap_fetch_int32(db, vstring);
	if (version == DATABASE_VERSION) {
		/*
		 * Race condition
		 */
		if (db->transaction_cancel(db)) {
			smb_panic("transaction_cancel failed");
		}
		return true;
	}

	if (version != DATABASE_VERSION) {
		if (dbwrap_store_uint32(db, vstring, DATABASE_VERSION) != 0) {
			DEBUG(0, ("dbwrap_store_uint32 failed\n"));
			goto cancel;
		}

		for (i=0; account_policy_names[i].type; i++) {

			if (!account_policy_set_default_on_empty(account_policy_names[i].type)) {
				DEBUG(0,("failed to set default value in account policy tdb\n"));
				goto cancel;
			}
		}
	}

	/* These exist by default on NT4 in [HKLM\SECURITY\Policy\Accounts] */

	privilege_create_account( &global_sid_World );
	privilege_create_account( &global_sid_Builtin_Account_Operators );
	privilege_create_account( &global_sid_Builtin_Server_Operators );
	privilege_create_account( &global_sid_Builtin_Print_Operators );
	privilege_create_account( &global_sid_Builtin_Backup_Operators );

	/* BUILTIN\Administrators get everything -- *always* */

	if ( lp_enable_privileges() ) {
		if ( !grant_all_privileges( &global_sid_Builtin_Administrators ) ) {
			DEBUG(1,("init_account_policy: Failed to grant privileges "
				"to BUILTIN\\Administrators!\n"));
		}
	}

	if (db->transaction_commit(db) != 0) {
		DEBUG(0, ("transaction_commit failed\n"));
		TALLOC_FREE(db);
		return false;
	}

	return True;

 cancel:
	if (db->transaction_cancel(db)) {
		smb_panic("transaction_cancel failed");
	}
	TALLOC_FREE(db);

	return false;
}

/*****************************************************************************
Get an account policy (from tdb)
*****************************************************************************/

bool account_policy_get(enum pdb_policy_type type, uint32_t *value)
{
	const char *name;
	uint32 regval;

	if (!init_account_policy()) {
		return False;
	}

	if (value) {
		*value = 0;
	}

	name = decode_account_policy_name(type);
	if (name == NULL) {
		DEBUG(1, ("account_policy_get: Field %d is not a valid account policy type!  Cannot get, returning 0.\n", type));
		return False;
	}

	if (!dbwrap_fetch_uint32(db, name, &regval)) {
		DEBUG(1, ("account_policy_get: tdb_fetch_uint32 failed for type %d (%s), returning 0\n", type, name));
		return False;
	}

	if (value) {
		*value = regval;
	}

	DEBUG(10,("account_policy_get: name: %s, val: %d\n", name, regval));
	return True;
}


/****************************************************************************
Set an account policy (in tdb)
****************************************************************************/

bool account_policy_set(enum pdb_policy_type type, uint32_t value)
{
	const char *name;
	NTSTATUS status;

	if (!init_account_policy()) {
		return False;
	}

	name = decode_account_policy_name(type);
	if (name == NULL) {
		DEBUG(1, ("Field %d is not a valid account policy type!  Cannot set.\n", type));
		return False;
	}

	status = dbwrap_trans_store_uint32(db, name, value);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("store_uint32 failed for type %d (%s) on value "
			  "%u: %s\n", type, name, value, nt_errstr(status)));
		return False;
	}

	DEBUG(10,("account_policy_set: name: %s, value: %d\n", name, value));

	return True;
}

/****************************************************************************
Set an account policy in the cache
****************************************************************************/

bool cache_account_policy_set(enum pdb_policy_type type, uint32_t value)
{
	const char *policy_name = NULL;
	char *cache_key = NULL;
	char *cache_value = NULL;
	bool ret = False;

	policy_name = decode_account_policy_name(type);
	if (policy_name == NULL) {
		DEBUG(0,("cache_account_policy_set: no policy found\n"));
		return False;
	}

	if (asprintf(&cache_key, "ACCT_POL/%s", policy_name) < 0) {
		DEBUG(0, ("asprintf failed\n"));
		goto done;
	}

	if (asprintf(&cache_value, "%lu\n", (unsigned long)value) < 0) {
		DEBUG(0, ("asprintf failed\n"));
		goto done;
	}

	DEBUG(10,("cache_account_policy_set: updating account pol cache\n"));

	ret = gencache_set(cache_key, cache_value, time(NULL)+AP_TTL);

 done:
	SAFE_FREE(cache_key);
	SAFE_FREE(cache_value);
	return ret;
}

/*****************************************************************************
Get an account policy from the cache
*****************************************************************************/

bool cache_account_policy_get(enum pdb_policy_type type, uint32_t *value)
{
	const char *policy_name = NULL;
	char *cache_key = NULL;
	char *cache_value = NULL;
	bool ret = False;

	policy_name = decode_account_policy_name(type);
	if (policy_name == NULL) {
		DEBUG(0,("cache_account_policy_set: no policy found\n"));
		return False;
	}

	if (asprintf(&cache_key, "ACCT_POL/%s", policy_name) < 0) {
		DEBUG(0, ("asprintf failed\n"));
		goto done;
	}

	if (gencache_get(cache_key, &cache_value, NULL)) {
		uint32 tmp = strtoul(cache_value, NULL, 10);
		*value = tmp;
		ret = True;
	}

 done:
	SAFE_FREE(cache_key);
	SAFE_FREE(cache_value);
	return ret;
}

/****************************************************************************
****************************************************************************/

struct db_context *get_account_pol_db( void )
{

	if ( db == NULL ) {
		if ( !init_account_policy() ) {
			return NULL;
		}
	}

	return db;
}
