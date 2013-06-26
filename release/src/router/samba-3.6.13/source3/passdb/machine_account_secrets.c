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
#include "passdb.h"
#include "../libcli/auth/libcli_auth.h"
#include "secrets.h"
#include "dbwrap.h"
#include "../librpc/ndr/libndr.h"
#include "util_tdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_PASSDB

/* Urrrg. global.... */
bool global_machine_password_needs_changing;

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

bool secrets_store_domain_sid(const char *domain, const struct dom_sid  *sid)
{
	bool ret;

	ret = secrets_store(domain_sid_keystr(domain), sid, sizeof(struct dom_sid ));

	/* Force a re-query, in case we modified our domain */
	if (ret)
		reset_global_sam_sid();
	return ret;
}

bool secrets_fetch_domain_sid(const char *domain, struct dom_sid  *sid)
{
	struct dom_sid  *dyn_sid;
	size_t size = 0;

	dyn_sid = (struct dom_sid  *)secrets_fetch(domain_sid_keystr(domain), &size);

	if (dyn_sid == NULL)
		return False;

	if (size != sizeof(struct dom_sid)) {
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

/************************************************************************
 Lock the trust password entry.
************************************************************************/

void *secrets_get_trust_account_lock(TALLOC_CTX *mem_ctx, const char *domain)
{
	struct db_context *db_ctx;
	if (!secrets_init()) {
		return NULL;
	}

	db_ctx = secrets_db_ctx();

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
