/*
   Unix SMB/CIFS implementation.
   Copyright (C) Andrew Tridgell 1992-2001
   Copyright (C) Andrew Bartlett      2002
   Copyright (C) Rafal Szczesniak     2002
   Copyright (C) Tim Potter           2001

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

/* the Samba secrets database stores any generated, private information
   such as the local SID and machine trust password */

#include "includes.h"
#include "../libcli/auth/libcli_auth.h"
#include "librpc/gen_ndr/ndr_secrets.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_PASSDB

static struct db_context *db_ctx;

/* Urrrg. global.... */
bool global_machine_password_needs_changing;

/**
 * Use a TDB to store an incrementing random seed.
 *
 * Initialised to the current pid, the very first time Samba starts,
 * and incremented by one each time it is needed.
 *
 * @note Not called by systems with a working /dev/urandom.
 */
static void get_rand_seed(void *userdata, int *new_seed)
{
	*new_seed = sys_getpid();
	if (db_ctx) {
		dbwrap_trans_change_int32_atomic(db_ctx, "INFO/random_seed",
						 new_seed, 1);
	}
}

/* open up the secrets database */
bool secrets_init(void)
{
	char *fname = NULL;
	unsigned char dummy;

	if (db_ctx != NULL)
		return True;

	fname = talloc_asprintf(talloc_tos(), "%s/secrets.tdb",
				lp_private_dir());
	if (fname == NULL) {
		return false;
	}

	db_ctx = db_open(NULL, fname, 0,
			 TDB_DEFAULT, O_RDWR|O_CREAT, 0600);

	if (db_ctx == NULL) {
		DEBUG(0,("Failed to open %s\n", fname));
		TALLOC_FREE(fname);
		return False;
	}

	TALLOC_FREE(fname);

	/**
	 * Set a reseed function for the crypto random generator
	 *
	 * This avoids a problem where systems without /dev/urandom
	 * could send the same challenge to multiple clients
	 */
	set_rand_reseed_callback(get_rand_seed, NULL);

	/* Ensure that the reseed is done now, while we are root, etc */
	generate_random_buffer(&dummy, sizeof(dummy));

	return True;
}

struct db_context *secrets_db_ctx(void)
{
	if (!secrets_init()) {
		return NULL;
	}

	return db_ctx;
}

/*
 * close secrets.tdb
 */
void secrets_shutdown(void)
{
	TALLOC_FREE(db_ctx);
}

/* read a entry from the secrets database - the caller must free the result
   if size is non-null then the size of the entry is put in there
 */
void *secrets_fetch(const char *key, size_t *size)
{
	TDB_DATA dbuf;
	void *result;

	if (!secrets_init()) {
		return NULL;
	}

	if (db_ctx->fetch(db_ctx, talloc_tos(), string_tdb_data(key),
			  &dbuf) != 0) {
		return NULL;
	}

	result = memdup(dbuf.dptr, dbuf.dsize);
	if (result == NULL) {
		return NULL;
	}
	TALLOC_FREE(dbuf.dptr);

	if (size) {
		*size = dbuf.dsize;
	}

	return result;
}

/* store a secrets entry
 */
bool secrets_store(const char *key, const void *data, size_t size)
{
	NTSTATUS status;

	if (!secrets_init()) {
		return false;
	}

	status = dbwrap_trans_store(db_ctx, string_tdb_data(key),
				    make_tdb_data((const uint8 *)data, size),
				    TDB_REPLACE);
	return NT_STATUS_IS_OK(status);
}


/* delete a secets database entry
 */
bool secrets_delete(const char *key)
{
	NTSTATUS status;
	if (!secrets_init()) {
		return false;
	}

	status = dbwrap_trans_delete(db_ctx, string_tdb_data(key));

	return NT_STATUS_IS_OK(status);
}

/**
 * Form a key for fetching the domain sid
 *
 * @param domain domain name
 *
 * @return keystring
 **/
static const char *domain_sid_keystr(const char *domain)
{
	char *keystr;

	keystr = talloc_asprintf_strupper_m(talloc_tos(), "%s/%s",
					    SECRETS_DOMAIN_SID, domain);
	SMB_ASSERT(keystr != NULL);
	return keystr;
}

bool secrets_store_domain_sid(const char *domain, const DOM_SID *sid)
{
	bool ret;

	ret = secrets_store(domain_sid_keystr(domain), sid, sizeof(DOM_SID));

	/* Force a re-query, in case we modified our domain */
	if (ret)
		reset_global_sam_sid();
	return ret;
}

bool secrets_fetch_domain_sid(const char *domain, DOM_SID *sid)
{
	DOM_SID *dyn_sid;
	size_t size = 0;

	dyn_sid = (DOM_SID *)secrets_fetch(domain_sid_keystr(domain), &size);

	if (dyn_sid == NULL)
		return False;

	if (size != sizeof(DOM_SID)) {
		SAFE_FREE(dyn_sid);
		return False;
	}

	*sid = *dyn_sid;
	SAFE_FREE(dyn_sid);
	return True;
}

bool secrets_store_domain_guid(const char *domain, struct GUID *guid)
{
	fstring key;

	slprintf(key, sizeof(key)-1, "%s/%s", SECRETS_DOMAIN_GUID, domain);
	strupper_m(key);
	return secrets_store(key, guid, sizeof(struct GUID));
}

bool secrets_fetch_domain_guid(const char *domain, struct GUID *guid)
{
	struct GUID *dyn_guid;
	fstring key;
	size_t size = 0;
	struct GUID new_guid;

	slprintf(key, sizeof(key)-1, "%s/%s", SECRETS_DOMAIN_GUID, domain);
	strupper_m(key);
	dyn_guid = (struct GUID *)secrets_fetch(key, &size);

	if (!dyn_guid) {
		if (lp_server_role() == ROLE_DOMAIN_PDC) {
			new_guid = GUID_random();
			if (!secrets_store_domain_guid(domain, &new_guid))
				return False;
			dyn_guid = (struct GUID *)secrets_fetch(key, &size);
		}
		if (dyn_guid == NULL) {
			return False;
		}
	}

	if (size != sizeof(struct GUID)) {
		DEBUG(1,("UUID size %d is wrong!\n", (int)size));
		SAFE_FREE(dyn_guid);
		return False;
	}

	*guid = *dyn_guid;
	SAFE_FREE(dyn_guid);
	return True;
}

bool secrets_store_local_schannel_key(uint8_t schannel_key[16])
{
	return secrets_store(SECRETS_LOCAL_SCHANNEL_KEY, schannel_key, 16);
}

bool secrets_fetch_local_schannel_key(uint8_t schannel_key[16])
{
	size_t size = 0;
	uint8_t *key;

	key = (uint8_t *)secrets_fetch(SECRETS_LOCAL_SCHANNEL_KEY, &size);
	if (key == NULL) {
		return false;
	}

	if (size != 16) {
		SAFE_FREE(key);
		return false;
	}

	memcpy(schannel_key, key, 16);
	SAFE_FREE(key);
	return true;
}

/**
 * Form a key for fetching the machine trust account sec channel type
 *
 * @param domain domain name
 *
 * @return keystring
 **/
static const char *machine_sec_channel_type_keystr(const char *domain)
{
	char *keystr;

	keystr = talloc_asprintf_strupper_m(talloc_tos(), "%s/%s",
					    SECRETS_MACHINE_SEC_CHANNEL_TYPE,
					    domain);
	SMB_ASSERT(keystr != NULL);
	return keystr;
}

/**
 * Form a key for fetching the machine trust account last change time
 *
 * @param domain domain name
 *
 * @return keystring
 **/
static const char *machine_last_change_time_keystr(const char *domain)
{
	char *keystr;

	keystr = talloc_asprintf_strupper_m(talloc_tos(), "%s/%s",
					    SECRETS_MACHINE_LAST_CHANGE_TIME,
					    domain);
	SMB_ASSERT(keystr != NULL);
	return keystr;
}


/**
 * Form a key for fetching the machine previous trust account password
 *
 * @param domain domain name
 *
 * @return keystring
 **/
static const char *machine_prev_password_keystr(const char *domain)
{
	char *keystr;

	keystr = talloc_asprintf_strupper_m(talloc_tos(), "%s/%s",
					    SECRETS_MACHINE_PASSWORD_PREV, domain);
	SMB_ASSERT(keystr != NULL);
	return keystr;
}

/**
 * Form a key for fetching the machine trust account password
 *
 * @param domain domain name
 *
 * @return keystring
 **/
static const char *machine_password_keystr(const char *domain)
{
	char *keystr;

	keystr = talloc_asprintf_strupper_m(talloc_tos(), "%s/%s",
					    SECRETS_MACHINE_PASSWORD, domain);
	SMB_ASSERT(keystr != NULL);
	return keystr;
}

/**
 * Form a key for fetching the machine trust account password
 *
 * @param domain domain name
 *
 * @return stored password's key
 **/
static const char *trust_keystr(const char *domain)
{
	char *keystr;

	keystr = talloc_asprintf_strupper_m(talloc_tos(), "%s/%s",
					    SECRETS_MACHINE_ACCT_PASS, domain);
	SMB_ASSERT(keystr != NULL);
	return keystr;
}

/**
 * Form a key for fetching a trusted domain password
 *
 * @param domain trusted domain name
 *
 * @return stored password's key
 **/
static char *trustdom_keystr(const char *domain)
{
	char *keystr;

	keystr = talloc_asprintf_strupper_m(talloc_tos(), "%s/%s",
					    SECRETS_DOMTRUST_ACCT_PASS,
					    domain);
	SMB_ASSERT(keystr != NULL);
	return keystr;
}

/************************************************************************
 Lock the trust password entry.
************************************************************************/

void *secrets_get_trust_account_lock(TALLOC_CTX *mem_ctx, const char *domain)
{
	if (!secrets_init()) {
		return NULL;
	}

	return db_ctx->fetch_locked(
		db_ctx, mem_ctx, string_term_tdb_data(trust_keystr(domain)));
}

/************************************************************************
 Routine to get the default secure channel type for trust accounts
************************************************************************/

enum netr_SchannelType get_default_sec_channel(void)
{
	if (lp_server_role() == ROLE_DOMAIN_BDC ||
	    lp_server_role() == ROLE_DOMAIN_PDC) {
		return SEC_CHAN_BDC;
	} else {
		return SEC_CHAN_WKSTA;
	}
}

/************************************************************************
 Routine to get the trust account password for a domain.
 This only tries to get the legacy hashed version of the password.
 The user of this function must have locked the trust password file using
 the above secrets_lock_trust_account_password().
************************************************************************/

bool secrets_fetch_trust_account_password_legacy(const char *domain,
						 uint8 ret_pwd[16],
						 time_t *pass_last_set_time,
						 enum netr_SchannelType *channel)
{
	struct machine_acct_pass *pass;
	size_t size = 0;

	if (!(pass = (struct machine_acct_pass *)secrets_fetch(
		      trust_keystr(domain), &size))) {
		DEBUG(5, ("secrets_fetch failed!\n"));
		return False;
	}

	if (size != sizeof(*pass)) {
		DEBUG(0, ("secrets were of incorrect size!\n"));
		SAFE_FREE(pass);
		return False;
	}

	if (pass_last_set_time) {
		*pass_last_set_time = pass->mod_time;
	}
	memcpy(ret_pwd, pass->hash, 16);

	if (channel) {
		*channel = get_default_sec_channel();
	}

	/* Test if machine password has expired and needs to be changed */
	if (lp_machine_password_timeout()) {
		if (pass->mod_time > 0 && time(NULL) > (pass->mod_time +
				(time_t)lp_machine_password_timeout())) {
			global_machine_password_needs_changing = True;
		}
	}

	SAFE_FREE(pass);
	return True;
}

/************************************************************************
 Routine to get the trust account password for a domain.
 The user of this function must have locked the trust password file using
 the above secrets_lock_trust_account_password().
************************************************************************/

bool secrets_fetch_trust_account_password(const char *domain, uint8 ret_pwd[16],
					  time_t *pass_last_set_time,
					  enum netr_SchannelType *channel)
{
	char *plaintext;

	plaintext = secrets_fetch_machine_password(domain, pass_last_set_time,
						   channel);
	if (plaintext) {
		DEBUG(4,("Using cleartext machine password\n"));
		E_md4hash(plaintext, ret_pwd);
		SAFE_FREE(plaintext);
		return True;
	}

	return secrets_fetch_trust_account_password_legacy(domain, ret_pwd,
							   pass_last_set_time,
							   channel);
}

/************************************************************************
 Routine to get account password to trusted domain
************************************************************************/

bool secrets_fetch_trusted_domain_password(const char *domain, char** pwd,
                                           DOM_SID *sid, time_t *pass_last_set_time)
{
	struct TRUSTED_DOM_PASS pass;
	enum ndr_err_code ndr_err;

	/* unpacking structures */
	DATA_BLOB blob;

	/* fetching trusted domain password structure */
	if (!(blob.data = (uint8_t *)secrets_fetch(trustdom_keystr(domain),
						   &blob.length))) {
		DEBUG(5, ("secrets_fetch failed!\n"));
		return False;
	}

	/* unpack trusted domain password */
	ndr_err = ndr_pull_struct_blob(&blob, talloc_tos(), NULL, &pass,
			(ndr_pull_flags_fn_t)ndr_pull_TRUSTED_DOM_PASS);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}

	SAFE_FREE(blob.data);

	/* the trust's password */
	if (pwd) {
		*pwd = SMB_STRDUP(pass.pass);
		if (!*pwd) {
			return False;
		}
	}

	/* last change time */
	if (pass_last_set_time) *pass_last_set_time = pass.mod_time;

	/* domain sid */
	if (sid != NULL) sid_copy(sid, &pass.domain_sid);

	return True;
}

/**
 * Routine to store the password for trusted domain
 *
 * @param domain remote domain name
 * @param pwd plain text password of trust relationship
 * @param sid remote domain sid
 *
 * @return true if succeeded
 **/

bool secrets_store_trusted_domain_password(const char* domain, const char* pwd,
                                           const DOM_SID *sid)
{
	bool ret;

	/* packing structures */
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	struct TRUSTED_DOM_PASS pass;
	ZERO_STRUCT(pass);

	pass.uni_name = domain;
	pass.uni_name_len = strlen(domain)+1;

	/* last change time */
	pass.mod_time = time(NULL);

	/* password of the trust */
	pass.pass_len = strlen(pwd);
	pass.pass = pwd;

	/* domain sid */
	sid_copy(&pass.domain_sid, sid);

	ndr_err = ndr_push_struct_blob(&blob, talloc_tos(), NULL, &pass,
			(ndr_push_flags_fn_t)ndr_push_TRUSTED_DOM_PASS);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}

	ret = secrets_store(trustdom_keystr(domain), blob.data, blob.length);

	data_blob_free(&blob);

	return ret;
}

/************************************************************************
 Routine to delete the old plaintext machine account password if any
************************************************************************/

static bool secrets_delete_prev_machine_password(const char *domain)
{
	char *oldpass = (char *)secrets_fetch(machine_prev_password_keystr(domain), NULL);
	if (oldpass == NULL) {
		return true;
	}
	SAFE_FREE(oldpass);
	return secrets_delete(machine_prev_password_keystr(domain));
}

/************************************************************************
 Routine to delete the plaintext machine account password and old
 password if any
************************************************************************/

bool secrets_delete_machine_password(const char *domain)
{
	if (!secrets_delete_prev_machine_password(domain)) {
		return false;
	}
	return secrets_delete(machine_password_keystr(domain));
}

/************************************************************************
 Routine to delete the plaintext machine account password, old password,
 sec channel type and last change time from secrets database
************************************************************************/

bool secrets_delete_machine_password_ex(const char *domain)
{
	if (!secrets_delete_prev_machine_password(domain)) {
		return false;
	}
	if (!secrets_delete(machine_password_keystr(domain))) {
		return false;
	}
	if (!secrets_delete(machine_sec_channel_type_keystr(domain))) {
		return false;
	}
	return secrets_delete(machine_last_change_time_keystr(domain));
}

/************************************************************************
 Routine to delete the domain sid
************************************************************************/

bool secrets_delete_domain_sid(const char *domain)
{
	return secrets_delete(domain_sid_keystr(domain));
}

/************************************************************************
 Routine to store the previous machine password (by storing the current password
 as the old)
************************************************************************/

static bool secrets_store_prev_machine_password(const char *domain)
{
	char *oldpass;
	bool ret;

	oldpass = (char *)secrets_fetch(machine_password_keystr(domain), NULL);
	if (oldpass == NULL) {
		return true;
	}
	ret = secrets_store(machine_prev_password_keystr(domain), oldpass, strlen(oldpass)+1);
	SAFE_FREE(oldpass);
	return ret;
}

/************************************************************************
 Routine to set the plaintext machine account password for a realm
 the password is assumed to be a null terminated ascii string.
 Before storing
************************************************************************/

bool secrets_store_machine_password(const char *pass, const char *domain,
				    enum netr_SchannelType sec_channel)
{
	bool ret;
	uint32 last_change_time;
	uint32 sec_channel_type;

	if (!secrets_store_prev_machine_password(domain)) {
		return false;
	}

	ret = secrets_store(machine_password_keystr(domain), pass, strlen(pass)+1);
	if (!ret)
		return ret;

	SIVAL(&last_change_time, 0, time(NULL));
	ret = secrets_store(machine_last_change_time_keystr(domain), &last_change_time, sizeof(last_change_time));

	SIVAL(&sec_channel_type, 0, sec_channel);
	ret = secrets_store(machine_sec_channel_type_keystr(domain), &sec_channel_type, sizeof(sec_channel_type));

	return ret;
}


/************************************************************************
 Routine to fetch the previous plaintext machine account password for a realm
 the password is assumed to be a null terminated ascii string.
************************************************************************/

char *secrets_fetch_prev_machine_password(const char *domain)
{
	return (char *)secrets_fetch(machine_prev_password_keystr(domain), NULL);
}

/************************************************************************
 Routine to fetch the plaintext machine account password for a realm
 the password is assumed to be a null terminated ascii string.
************************************************************************/

char *secrets_fetch_machine_password(const char *domain,
				     time_t *pass_last_set_time,
				     enum netr_SchannelType *channel)
{
	char *ret;
	ret = (char *)secrets_fetch(machine_password_keystr(domain), NULL);

	if (pass_last_set_time) {
		size_t size;
		uint32 *last_set_time;
		last_set_time = (unsigned int *)secrets_fetch(machine_last_change_time_keystr(domain), &size);
		if (last_set_time) {
			*pass_last_set_time = IVAL(last_set_time,0);
			SAFE_FREE(last_set_time);
		} else {
			*pass_last_set_time = 0;
		}
	}

	if (channel) {
		size_t size;
		uint32 *channel_type;
		channel_type = (unsigned int *)secrets_fetch(machine_sec_channel_type_keystr(domain), &size);
		if (channel_type) {
			*channel = IVAL(channel_type,0);
			SAFE_FREE(channel_type);
		} else {
			*channel = get_default_sec_channel();
		}
	}

	return ret;
}

/************************************************************************
 Routine to delete the password for trusted domain
************************************************************************/

bool trusted_domain_password_delete(const char *domain)
{
	return secrets_delete(trustdom_keystr(domain));
}

bool secrets_store_ldap_pw(const char* dn, char* pw)
{
	char *key = NULL;
	bool ret;

	if (asprintf(&key, "%s/%s", SECRETS_LDAP_BIND_PW, dn) < 0) {
		DEBUG(0, ("secrets_store_ldap_pw: asprintf failed!\n"));
		return False;
	}

	ret = secrets_store(key, pw, strlen(pw)+1);

	SAFE_FREE(key);
	return ret;
}

/*******************************************************************
 Find the ldap password.
******************************************************************/

bool fetch_ldap_pw(char **dn, char** pw)
{
	char *key = NULL;
	size_t size = 0;

	*dn = smb_xstrdup(lp_ldap_admin_dn());

	if (asprintf(&key, "%s/%s", SECRETS_LDAP_BIND_PW, *dn) < 0) {
		SAFE_FREE(*dn);
		DEBUG(0, ("fetch_ldap_pw: asprintf failed!\n"));
		return false;
	}

	*pw=(char *)secrets_fetch(key, &size);
	SAFE_FREE(key);

	if (!size) {
		/* Upgrade 2.2 style entry */
		char *p;
	        char* old_style_key = SMB_STRDUP(*dn);
		char *data;
		fstring old_style_pw;

		if (!old_style_key) {
			DEBUG(0, ("fetch_ldap_pw: strdup failed!\n"));
			return False;
		}

		for (p=old_style_key; *p; p++)
			if (*p == ',') *p = '/';

		data=(char *)secrets_fetch(old_style_key, &size);
		if ((data == NULL) || (size < sizeof(old_style_pw))) {
			DEBUG(0,("fetch_ldap_pw: neither ldap secret retrieved!\n"));
			SAFE_FREE(old_style_key);
			SAFE_FREE(*dn);
			SAFE_FREE(data);
			return False;
		}

		size = MIN(size, sizeof(fstring)-1);
		strncpy(old_style_pw, data, size);
		old_style_pw[size] = 0;

		SAFE_FREE(data);

		if (!secrets_store_ldap_pw(*dn, old_style_pw)) {
			DEBUG(0,("fetch_ldap_pw: ldap secret could not be upgraded!\n"));
			SAFE_FREE(old_style_key);
			SAFE_FREE(*dn);
			return False;
		}
		if (!secrets_delete(old_style_key)) {
			DEBUG(0,("fetch_ldap_pw: old ldap secret could not be deleted!\n"));
		}

		SAFE_FREE(old_style_key);

		*pw = smb_xstrdup(old_style_pw);
	}

	return True;
}

/**
 * Get trusted domains info from secrets.tdb.
 **/

struct list_trusted_domains_state {
	uint32 num_domains;
	struct trustdom_info **domains;
};

static int list_trusted_domain(struct db_record *rec, void *private_data)
{
	const size_t prefix_len = strlen(SECRETS_DOMTRUST_ACCT_PASS);
	struct TRUSTED_DOM_PASS pass;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct trustdom_info *dom_info;

	struct list_trusted_domains_state *state =
		(struct list_trusted_domains_state *)private_data;

	if ((rec->key.dsize < prefix_len)
	    || (strncmp((char *)rec->key.dptr, SECRETS_DOMTRUST_ACCT_PASS,
			prefix_len) != 0)) {
		return 0;
	}

	blob = data_blob_const(rec->value.dptr, rec->value.dsize);

	ndr_err = ndr_pull_struct_blob(&blob, talloc_tos(), NULL, &pass,
			(ndr_pull_flags_fn_t)ndr_pull_TRUSTED_DOM_PASS);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}

	if (pass.domain_sid.num_auths != 4) {
		DEBUG(0, ("SID %s is not a domain sid, has %d "
			  "auths instead of 4\n",
			  sid_string_dbg(&pass.domain_sid),
			  pass.domain_sid.num_auths));
		return 0;
	}

	if (!(dom_info = TALLOC_P(state->domains, struct trustdom_info))) {
		DEBUG(0, ("talloc failed\n"));
		return 0;
	}

	dom_info->name = talloc_strdup(dom_info, pass.uni_name);
	if (!dom_info->name) {
		TALLOC_FREE(dom_info);
		return 0;
	}

	sid_copy(&dom_info->sid, &pass.domain_sid);

	ADD_TO_ARRAY(state->domains, struct trustdom_info *, dom_info,
		     &state->domains, &state->num_domains);

	if (state->domains == NULL) {
		state->num_domains = 0;
		return -1;
	}
	return 0;
}

NTSTATUS secrets_trusted_domains(TALLOC_CTX *mem_ctx, uint32 *num_domains,
				 struct trustdom_info ***domains)
{
	struct list_trusted_domains_state state;

	secrets_init();

	if (db_ctx == NULL) {
		return NT_STATUS_ACCESS_DENIED;
	}

	state.num_domains = 0;

	/*
	 * Make sure that a talloc context for the trustdom_info structs
	 * exists
	 */

	if (!(state.domains = TALLOC_ARRAY(
		      mem_ctx, struct trustdom_info *, 1))) {
		return NT_STATUS_NO_MEMORY;
	}

	db_ctx->traverse_read(db_ctx, list_trusted_domain, (void *)&state);

	*num_domains = state.num_domains;
	*domains = state.domains;
	return NT_STATUS_OK;
}

/*******************************************************************************
 Store a complete AFS keyfile into secrets.tdb.
*******************************************************************************/

bool secrets_store_afs_keyfile(const char *cell, const struct afs_keyfile *keyfile)
{
	fstring key;

	if ((cell == NULL) || (keyfile == NULL))
		return False;

	if (ntohl(keyfile->nkeys) > SECRETS_AFS_MAXKEYS)
		return False;

	slprintf(key, sizeof(key)-1, "%s/%s", SECRETS_AFS_KEYFILE, cell);
	return secrets_store(key, keyfile, sizeof(struct afs_keyfile));
}

/*******************************************************************************
 Fetch the current (highest) AFS key from secrets.tdb
*******************************************************************************/
bool secrets_fetch_afs_key(const char *cell, struct afs_key *result)
{
	fstring key;
	struct afs_keyfile *keyfile;
	size_t size = 0;
	uint32 i;

	slprintf(key, sizeof(key)-1, "%s/%s", SECRETS_AFS_KEYFILE, cell);

	keyfile = (struct afs_keyfile *)secrets_fetch(key, &size);

	if (keyfile == NULL)
		return False;

	if (size != sizeof(struct afs_keyfile)) {
		SAFE_FREE(keyfile);
		return False;
	}

	i = ntohl(keyfile->nkeys);

	if (i > SECRETS_AFS_MAXKEYS) {
		SAFE_FREE(keyfile);
		return False;
	}

	*result = keyfile->entry[i-1];

	result->kvno = ntohl(result->kvno);

	SAFE_FREE(keyfile);

	return True;
}

/******************************************************************************
  When kerberos is not available, choose between anonymous or
  authenticated connections.

  We need to use an authenticated connection if DCs have the
  RestrictAnonymous registry entry set > 0, or the "Additional
  restrictions for anonymous connections" set in the win2k Local
  Security Policy.

  Caller to free() result in domain, username, password
*******************************************************************************/
void secrets_fetch_ipc_userpass(char **username, char **domain, char **password)
{
	*username = (char *)secrets_fetch(SECRETS_AUTH_USER, NULL);
	*domain = (char *)secrets_fetch(SECRETS_AUTH_DOMAIN, NULL);
	*password = (char *)secrets_fetch(SECRETS_AUTH_PASSWORD, NULL);

	if (*username && **username) {

		if (!*domain || !**domain)
			*domain = smb_xstrdup(lp_workgroup());

		if (!*password || !**password)
			*password = smb_xstrdup("");

		DEBUG(3, ("IPC$ connections done by user %s\\%s\n",
			  *domain, *username));

	} else {
		DEBUG(3, ("IPC$ connections done anonymously\n"));
		*username = smb_xstrdup("");
		*domain = smb_xstrdup("");
		*password = smb_xstrdup("");
	}
}

bool secrets_store_generic(const char *owner, const char *key, const char *secret)
{
	char *tdbkey = NULL;
	bool ret;

	if (asprintf(&tdbkey, "SECRETS/GENERIC/%s/%s", owner, key) < 0) {
		DEBUG(0, ("asprintf failed!\n"));
		return False;
	}

	ret = secrets_store(tdbkey, secret, strlen(secret)+1);

	SAFE_FREE(tdbkey);
	return ret;
}

bool secrets_delete_generic(const char *owner, const char *key)
{
	char *tdbkey = NULL;
	bool ret;

	if (asprintf(&tdbkey, "SECRETS/GENERIC/%s/%s", owner, key) < 0) {
		DEBUG(0, ("asprintf failed!\n"));
		return False;
	}

	ret = secrets_delete(tdbkey);

	SAFE_FREE(tdbkey);
	return ret;
}

/*******************************************************************
 Find the ldap password.
******************************************************************/

char *secrets_fetch_generic(const char *owner, const char *key)
{
	char *secret = NULL;
	char *tdbkey = NULL;

	if (( ! owner) || ( ! key)) {
		DEBUG(1, ("Invalid Paramters"));
		return NULL;
	}

	if (asprintf(&tdbkey, "SECRETS/GENERIC/%s/%s", owner, key) < 0) {
		DEBUG(0, ("Out of memory!\n"));
		return NULL;
	}

	secret = (char *)secrets_fetch(tdbkey, NULL);
	SAFE_FREE(tdbkey);

	return secret;
}

