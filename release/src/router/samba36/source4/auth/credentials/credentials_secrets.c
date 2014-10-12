/* 
   Unix SMB/CIFS implementation.

   User credentials handling (as regards on-disk files)

   Copyright (C) Jelmer Vernooij 2005
   Copyright (C) Tim Potter 2001
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   
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
#include "lib/events/events.h"
#include <ldb.h>
#include "librpc/gen_ndr/samr.h" /* for struct samrPassword */
#include "param/secrets.h"
#include "system/filesys.h"
#include "auth/credentials/credentials.h"
#include "auth/credentials/credentials_krb5.h"
#include "auth/kerberos/kerberos_util.h"
#include "param/param.h"
#include "lib/events/events.h"
#include "dsdb/samdb/samdb.h"

/**
 * Fill in credentials for the machine trust account, from the secrets database.
 * 
 * @param cred Credentials structure to fill in
 * @retval NTSTATUS error detailing any failure
 */
_PUBLIC_ NTSTATUS cli_credentials_set_secrets(struct cli_credentials *cred, 
					      struct loadparm_context *lp_ctx,
					      struct ldb_context *ldb,
					      const char *base,
					      const char *filter, 
					      char **error_string)
{
	TALLOC_CTX *mem_ctx;
	
	int ldb_ret;
	struct ldb_message *msg;
	
	const char *machine_account;
	const char *password;
	const char *old_password;
	const char *domain;
	const char *realm;
	enum netr_SchannelType sct;
	const char *salt_principal;
	char *keytab;
	const struct ldb_val *whenChanged;

	/* ok, we are going to get it now, don't recurse back here */
	cred->machine_account_pending = false;

	/* some other parts of the system will key off this */
	cred->machine_account = true;

	mem_ctx = talloc_named(cred, 0, "cli_credentials fetch machine password");

	if (!ldb) {
		/* Local secrets are stored in secrets.ldb */
		ldb = secrets_db_connect(mem_ctx, lp_ctx);
		if (!ldb) {
			/* set anonymous as the fallback, if the machine account won't work */
			cli_credentials_set_anonymous(cred);
			*error_string = talloc_strdup(cred, "Could not open secrets.ldb");
			talloc_free(mem_ctx);
			return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		}
	}

	ldb_ret = dsdb_search_one(ldb, mem_ctx, &msg,
				  ldb_dn_new(mem_ctx, ldb, base),
				  LDB_SCOPE_SUBTREE,
				  NULL, 0, "%s", filter);

	if (ldb_ret != LDB_SUCCESS) {
		*error_string = talloc_asprintf(cred, "Could not find entry to match filter: '%s' base: '%s': %s: %s\n",
						filter, base ? base : "",
						ldb_strerror(ldb_ret), ldb_errstring(ldb));
		/* set anonymous as the fallback, if the machine account won't work */
		cli_credentials_set_anonymous(cred);
		talloc_free(mem_ctx);
		return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
	}

	password = ldb_msg_find_attr_as_string(msg, "secret", NULL);
	old_password = ldb_msg_find_attr_as_string(msg, "priorSecret", NULL);

	machine_account = ldb_msg_find_attr_as_string(msg, "samAccountName", NULL);

	if (!machine_account) {
		machine_account = ldb_msg_find_attr_as_string(msg, "servicePrincipalName", NULL);
		
		if (!machine_account) {
			const char *ldap_bind_dn = ldb_msg_find_attr_as_string(msg, "ldapBindDn", NULL);
			if (!ldap_bind_dn) {
				*error_string = talloc_asprintf(cred, 
								"Could not find 'samAccountName', "
								"'servicePrincipalName' or "
								"'ldapBindDn' in secrets record: %s",
								ldb_dn_get_linearized(msg->dn));
				/* set anonymous as the fallback, if the machine account won't work */
				cli_credentials_set_anonymous(cred);
				talloc_free(mem_ctx);
				return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
			} else {
				/* store bind dn in credentials */
				cli_credentials_set_bind_dn(cred, ldap_bind_dn);
			}
		}
	}

	salt_principal = ldb_msg_find_attr_as_string(msg, "saltPrincipal", NULL);
	cli_credentials_set_salt_principal(cred, salt_principal);
	
	sct = ldb_msg_find_attr_as_int(msg, "secureChannelType", 0);
	if (sct) { 
		cli_credentials_set_secure_channel_type(cred, sct);
	}
	
	if (!password) {
		const struct ldb_val *nt_password_hash = ldb_msg_find_ldb_val(msg, "unicodePwd");
		struct samr_Password hash;
		ZERO_STRUCT(hash);
		if (nt_password_hash) {
			memcpy(hash.hash, nt_password_hash->data, 
			       MIN(nt_password_hash->length, sizeof(hash.hash)));
		
			cli_credentials_set_nt_hash(cred, &hash, CRED_SPECIFIED);
		} else {
			cli_credentials_set_password(cred, NULL, CRED_SPECIFIED);
		}
	} else {
		cli_credentials_set_password(cred, password, CRED_SPECIFIED);
	}

	
	domain = ldb_msg_find_attr_as_string(msg, "flatname", NULL);
	if (domain) {
		cli_credentials_set_domain(cred, domain, CRED_SPECIFIED);
	}

	realm = ldb_msg_find_attr_as_string(msg, "realm", NULL);
	if (realm) {
		cli_credentials_set_realm(cred, realm, CRED_SPECIFIED);
	}

	if (machine_account) {
		cli_credentials_set_username(cred, machine_account, CRED_SPECIFIED);
	}

	cli_credentials_set_kvno(cred, ldb_msg_find_attr_as_int(msg, "msDS-KeyVersionNumber", 0));

	whenChanged = ldb_msg_find_ldb_val(msg, "whenChanged");
	if (whenChanged) {
		time_t lct;
		if (ldb_val_to_time(whenChanged, &lct) == LDB_SUCCESS) {
			cli_credentials_set_password_last_changed_time(cred, lct);
		}
	}
	
	/* If there was an external keytab specified by reference in
	 * the LDB, then use this.  Otherwise we will make one up
	 * (chewing CPU time) from the password */
	keytab = keytab_name_from_msg(cred, ldb, msg);
	if (keytab) {
		cli_credentials_set_keytab_name(cred, lp_ctx, keytab, CRED_SPECIFIED);
		talloc_free(keytab);
	}
	talloc_free(mem_ctx);
	
	return NT_STATUS_OK;
}

/**
 * Fill in credentials for the machine trust account, from the secrets database.
 * 
 * @param cred Credentials structure to fill in
 * @retval NTSTATUS error detailing any failure
 */
_PUBLIC_ NTSTATUS cli_credentials_set_machine_account(struct cli_credentials *cred,
						      struct loadparm_context *lp_ctx)
{
	NTSTATUS status;
	char *filter;
	char *error_string;
	/* Bleh, nasty recursion issues: We are setting a machine
	 * account here, so we don't want the 'pending' flag around
	 * any more */
	cred->machine_account_pending = false;
	filter = talloc_asprintf(cred, SECRETS_PRIMARY_DOMAIN_FILTER, 
				 cli_credentials_get_domain(cred));
	status = cli_credentials_set_secrets(cred, lp_ctx, NULL,
					     SECRETS_PRIMARY_DOMAIN_DN,
					     filter, &error_string);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not find machine account in secrets database: %s: %s", nt_errstr(status), error_string));
		talloc_free(error_string);
	}
	return status;
}

/**
 * Fill in credentials for the machine trust account, from the secrets database.
 * 
 * @param cred Credentials structure to fill in
 * @retval NTSTATUS error detailing any failure
 */
NTSTATUS cli_credentials_set_krbtgt(struct cli_credentials *cred,
				    struct loadparm_context *lp_ctx)
{
	NTSTATUS status;
	char *filter;
	char *error_string;
	/* Bleh, nasty recursion issues: We are setting a machine
	 * account here, so we don't want the 'pending' flag around
	 * any more */
	cred->machine_account_pending = false;
	filter = talloc_asprintf(cred, SECRETS_KRBTGT_SEARCH,
				       cli_credentials_get_realm(cred),
				       cli_credentials_get_domain(cred));
	status = cli_credentials_set_secrets(cred, lp_ctx, NULL,
					     SECRETS_PRINCIPALS_DN,
					     filter, &error_string);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not find krbtgt (master Kerberos) account in secrets database: %s: %s", nt_errstr(status), error_string));
		talloc_free(error_string);
	}
	return status;
}

/**
 * Fill in credentials for a particular prinicpal, from the secrets database.
 * 
 * @param cred Credentials structure to fill in
 * @retval NTSTATUS error detailing any failure
 */
_PUBLIC_ NTSTATUS cli_credentials_set_stored_principal(struct cli_credentials *cred,
					      struct loadparm_context *lp_ctx,
					      const char *serviceprincipal)
{
	NTSTATUS status;
	char *filter;
	char *error_string;
	/* Bleh, nasty recursion issues: We are setting a machine
	 * account here, so we don't want the 'pending' flag around
	 * any more */
	cred->machine_account_pending = false;
	filter = talloc_asprintf(cred, SECRETS_PRINCIPAL_SEARCH,
				 cli_credentials_get_realm(cred),
				 cli_credentials_get_domain(cred),
				 serviceprincipal);
	status = cli_credentials_set_secrets(cred, lp_ctx, NULL,
					     SECRETS_PRINCIPALS_DN, filter,
					     &error_string);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not find %s principal in secrets database: %s: %s", serviceprincipal, nt_errstr(status), error_string));
	}
	return status;
}

/**
 * Ask that when required, the credentials system will be filled with
 * machine trust account, from the secrets database.
 * 
 * @param cred Credentials structure to fill in
 * @note This function is used to call the above function after, rather 
 *       than during, popt processing.
 *
 */
_PUBLIC_ void cli_credentials_set_machine_account_pending(struct cli_credentials *cred,
						 struct loadparm_context *lp_ctx)
{
	cred->machine_account_pending = true;
	cred->machine_account_pending_lp_ctx = lp_ctx;
}


