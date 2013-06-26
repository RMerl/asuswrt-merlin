/*
 * Unix SMB/CIFS implementation. 
 * secrets.tdb file format info
 * Copyright (C) Andrew Tridgell              2000
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.  
 */

#ifndef _SECRETS_H
#define _SECRETS_H

/* the first one is for the hashed password (NT4 style) the latter
   for plaintext (ADS)
*/
#define SECRETS_MACHINE_ACCT_PASS "SECRETS/$MACHINE.ACC"
#define SECRETS_MACHINE_PASSWORD "SECRETS/MACHINE_PASSWORD"
#define SECRETS_MACHINE_PASSWORD_PREV "SECRETS/MACHINE_PASSWORD.PREV"
#define SECRETS_MACHINE_LAST_CHANGE_TIME "SECRETS/MACHINE_LAST_CHANGE_TIME"
#define SECRETS_MACHINE_SEC_CHANNEL_TYPE "SECRETS/MACHINE_SEC_CHANNEL_TYPE"
#define SECRETS_MACHINE_TRUST_ACCOUNT_NAME "SECRETS/SECRETS_MACHINE_TRUST_ACCOUNT_NAME"
/* this one is for storing trusted domain account password */
#define SECRETS_DOMTRUST_ACCT_PASS "SECRETS/$DOMTRUST.ACC"

/* Store the principal name used for Kerberos DES key salt under this key name. */
#define SECRETS_SALTING_PRINCIPAL "SECRETS/SALTING_PRINCIPAL"

/* The domain sid and our sid are stored here even though they aren't
   really secret. */
#define SECRETS_DOMAIN_SID    "SECRETS/SID"
#define SECRETS_SAM_SID       "SAM/SID"

/* The domain GUID and server GUID (NOT the same) are also not secret */
#define SECRETS_DOMAIN_GUID   "SECRETS/DOMGUID"
#define SECRETS_SERVER_GUID   "SECRETS/GUID"

#define SECRETS_LDAP_BIND_PW "SECRETS/LDAP_BIND_PW"

#define SECRETS_LOCAL_SCHANNEL_KEY "SECRETS/LOCAL_SCHANNEL_KEY"

/* Authenticated user info is stored in secrets.tdb under these keys */

#define SECRETS_AUTH_USER      "SECRETS/AUTH_USER"
#define SECRETS_AUTH_DOMAIN      "SECRETS/AUTH_DOMAIN"
#define SECRETS_AUTH_PASSWORD  "SECRETS/AUTH_PASSWORD"

/* structure for storing machine account password
   (ie. when samba server is member of a domain */
struct machine_acct_pass {
	uint8 hash[16];
	time_t mod_time;
};

/*
 * Format of an OpenAFS keyfile
 */

#define SECRETS_AFS_MAXKEYS 8

struct afs_key {
	uint32 kvno;
	char key[8];
};

struct afs_keyfile {
	uint32 nkeys;
	struct afs_key entry[SECRETS_AFS_MAXKEYS];
};

#define SECRETS_AFS_KEYFILE "SECRETS/AFS_KEYFILE"

/* The following definitions come from passdb/secrets.c  */

bool secrets_init(void);
struct db_context *secrets_db_ctx(void);
void secrets_shutdown(void);
void *secrets_fetch(const char *key, size_t *size);
bool secrets_store(const char *key, const void *data, size_t size);
bool secrets_delete(const char *key);
bool secrets_store_domain_sid(const char *domain, const struct dom_sid  *sid);
bool secrets_fetch_domain_sid(const char *domain, struct dom_sid  *sid);
bool secrets_store_domain_guid(const char *domain, struct GUID *guid);
bool secrets_fetch_domain_guid(const char *domain, struct GUID *guid);
void *secrets_get_trust_account_lock(TALLOC_CTX *mem_ctx, const char *domain);
enum netr_SchannelType get_default_sec_channel(void);
bool secrets_fetch_trust_account_password_legacy(const char *domain,
						 uint8 ret_pwd[16],
						 time_t *pass_last_set_time,
						 enum netr_SchannelType *channel);
bool secrets_fetch_trust_account_password(const char *domain, uint8 ret_pwd[16],
					  time_t *pass_last_set_time,
					  enum netr_SchannelType *channel);
bool secrets_fetch_trusted_domain_password(const char *domain, char** pwd,
                                           struct dom_sid  *sid, time_t *pass_last_set_time);
bool secrets_store_trusted_domain_password(const char* domain, const char* pwd,
                                           const struct dom_sid  *sid);
bool secrets_delete_machine_password(const char *domain);
bool secrets_delete_machine_password_ex(const char *domain);
bool secrets_delete_domain_sid(const char *domain);
bool secrets_store_machine_password(const char *pass, const char *domain, enum netr_SchannelType sec_channel);
char *secrets_fetch_prev_machine_password(const char *domain);
char *secrets_fetch_machine_password(const char *domain,
				     time_t *pass_last_set_time,
				     enum netr_SchannelType *channel);
bool trusted_domain_password_delete(const char *domain);
bool secrets_store_ldap_pw(const char* dn, char* pw);
bool fetch_ldap_pw(char **dn, char** pw);
struct trustdom_info;
NTSTATUS secrets_trusted_domains(TALLOC_CTX *mem_ctx, uint32 *num_domains,
				 struct trustdom_info ***domains);
bool secrets_store_afs_keyfile(const char *cell, const struct afs_keyfile *keyfile);
bool secrets_fetch_afs_key(const char *cell, struct afs_key *result);
void secrets_fetch_ipc_userpass(char **username, char **domain, char **password);
bool secrets_store_generic(const char *owner, const char *key, const char *secret);
char *secrets_fetch_generic(const char *owner, const char *key);
bool secrets_delete_generic(const char *owner, const char *key);

#endif /* _SECRETS_H */
