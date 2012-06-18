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
#include "lib/ldb/include/ldb.h"
#include "librpc/gen_ndr/samr.h" /* for struct samrPassword */
#include "param/secrets.h"
#include "system/filesys.h"
#include "../lib/util/util_ldb.h"
#include "auth/credentials/credentials.h"
#include "auth/credentials/credentials_krb5.h"
#include "auth/credentials/credentials_proto.h"
#include "param/param.h"
#include "lib/events/events.h"

/**
 * Read a file descriptor, and parse it for a password (eg from a file or stdin)
 *
 * @param credentials Credentials structure on which to set the password
 * @param fd open file descriptor to read the password from 
 * @param obtained This enum describes how 'specified' this password is
 */

_PUBLIC_ bool cli_credentials_parse_password_fd(struct cli_credentials *credentials, 
				       int fd, enum credentials_obtained obtained)
{
	char *p;
	char pass[128];

	for(p = pass, *p = '\0'; /* ensure that pass is null-terminated */
		p && p - pass < sizeof(pass);) {
		switch (read(fd, p, 1)) {
		case 1:
			if (*p != '\n' && *p != '\0') {
				*++p = '\0'; /* advance p, and null-terminate pass */
				break;
			}
			/* fall through */
		case 0:
			if (p - pass) {
				*p = '\0'; /* null-terminate it, just in case... */
				p = NULL; /* then force the loop condition to become false */
				break;
			} else {
				fprintf(stderr, "Error reading password from file descriptor %d: %s\n", fd, "empty password\n");
				return false;
			}

		default:
			fprintf(stderr, "Error reading password from file descriptor %d: %s\n",
					fd, strerror(errno));
			return false;
		}
	}

	cli_credentials_set_password(credentials, pass, obtained);
	return true;
}

/**
 * Read a named file, and parse it for a password
 *
 * @param credentials Credentials structure on which to set the password
 * @param file a named file to read the password from 
 * @param obtained This enum describes how 'specified' this password is
 */

_PUBLIC_ bool cli_credentials_parse_password_file(struct cli_credentials *credentials, const char *file, enum credentials_obtained obtained)
{
	int fd = open(file, O_RDONLY, 0);
	bool ret;

	if (fd < 0) {
		fprintf(stderr, "Error opening password file %s: %s\n",
				file, strerror(errno));
		return false;
	}

	ret = cli_credentials_parse_password_fd(credentials, fd, obtained);

	close(fd);
	
	return ret;
}

/**
 * Read a named file, and parse it for username, domain, realm and password
 *
 * @param credentials Credentials structure on which to set the password
 * @param file a named file to read the details from 
 * @param obtained This enum describes how 'specified' this password is
 */

_PUBLIC_ bool cli_credentials_parse_file(struct cli_credentials *cred, const char *file, enum credentials_obtained obtained) 
{
	uint16_t len = 0;
	char *ptr, *val, *param;
	char **lines;
	int i, numlines;

	lines = file_lines_load(file, &numlines, 0, NULL);

	if (lines == NULL)
	{
		/* fail if we can't open the credentials file */
		d_printf("ERROR: Unable to open credentials file!\n");
		return false;
	}

	for (i = 0; i < numlines; i++) {
		len = strlen(lines[i]);

		if (len == 0)
			continue;

		/* break up the line into parameter & value.
		 * will need to eat a little whitespace possibly */
		param = lines[i];
		if (!(ptr = strchr_m (lines[i], '=')))
			continue;

		val = ptr+1;
		*ptr = '\0';

		/* eat leading white space */
		while ((*val!='\0') && ((*val==' ') || (*val=='\t')))
			val++;

		if (strwicmp("password", param) == 0) {
			cli_credentials_set_password(cred, val, obtained);
		} else if (strwicmp("username", param) == 0) {
			cli_credentials_set_username(cred, val, obtained);
		} else if (strwicmp("domain", param) == 0) {
			cli_credentials_set_domain(cred, val, obtained);
		} else if (strwicmp("realm", param) == 0) {
			cli_credentials_set_realm(cred, val, obtained);
		}
		memset(lines[i], 0, len);
	}

	talloc_free(lines);

	return true;
}


/**
 * Fill in credentials for the machine trust account, from the secrets database.
 * 
 * @param cred Credentials structure to fill in
 * @retval NTSTATUS error detailing any failure
 */
_PUBLIC_ NTSTATUS cli_credentials_set_secrets(struct cli_credentials *cred, 
					      struct tevent_context *event_ctx,
				     struct loadparm_context *lp_ctx,
				     struct ldb_context *ldb,
				     const char *base,
				     const char *filter)
{
	TALLOC_CTX *mem_ctx;
	
	int ldb_ret;
	struct ldb_message **msgs;
	const char *attrs[] = {
		"secret",
		"priorSecret",
		"samAccountName",
		"flatname",
		"realm",
		"secureChannelType",
		"unicodePwd",
		"msDS-KeyVersionNumber",
		"saltPrincipal",
		"privateKeytab",
		"krb5Keytab",
		"servicePrincipalName",
		"ldapBindDn",
		NULL
	};
	
	const char *machine_account;
	const char *password;
	const char *old_password;
	const char *domain;
	const char *realm;
	enum netr_SchannelType sct;
	const char *salt_principal;
	const char *keytab;
	
	/* ok, we are going to get it now, don't recurse back here */
	cred->machine_account_pending = false;

	/* some other parts of the system will key off this */
	cred->machine_account = true;

	mem_ctx = talloc_named(cred, 0, "cli_credentials fetch machine password");

	if (!ldb) {
		/* Local secrets are stored in secrets.ldb */
		ldb = secrets_db_connect(mem_ctx, event_ctx, lp_ctx);
		if (!ldb) {
			/* set anonymous as the fallback, if the machine account won't work */
			cli_credentials_set_anonymous(cred);
			DEBUG(1, ("Could not open secrets.ldb\n"));
			talloc_free(mem_ctx);
			return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		}
	}

	/* search for the secret record */
	ldb_ret = gendb_search(ldb,
			       mem_ctx, ldb_dn_new(mem_ctx, ldb, base), 
			       &msgs, attrs,
			       "%s", filter);
	if (ldb_ret == 0) {
		DEBUG(5, ("(normal if no LDAP backend required) Could not find entry to match filter: '%s' base: '%s'\n",
			  filter, base));
		/* set anonymous as the fallback, if the machine account won't work */
		cli_credentials_set_anonymous(cred);
		talloc_free(mem_ctx);
		return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
	} else if (ldb_ret != 1) {
		DEBUG(5, ("Found more than one (%d) entry to match filter: '%s' base: '%s'\n",
			  ldb_ret, filter, base));
		/* set anonymous as the fallback, if the machine account won't work */
		cli_credentials_set_anonymous(cred);
		talloc_free(mem_ctx);
		return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
	}
	
	password = ldb_msg_find_attr_as_string(msgs[0], "secret", NULL);
	old_password = ldb_msg_find_attr_as_string(msgs[0], "priorSecret", NULL);

	machine_account = ldb_msg_find_attr_as_string(msgs[0], "samAccountName", NULL);

	if (!machine_account) {
		machine_account = ldb_msg_find_attr_as_string(msgs[0], "servicePrincipalName", NULL);
		
		if (!machine_account) {
			const char *ldap_bind_dn = ldb_msg_find_attr_as_string(msgs[0], "ldapBindDn", NULL);
			if (!ldap_bind_dn) {
				DEBUG(1, ("Could not find 'samAccountName', 'servicePrincipalName' or 'ldapBindDn' in secrets record: filter: '%s' base: '%s'\n",
					  filter, base));
				/* set anonymous as the fallback, if the machine account won't work */
				cli_credentials_set_anonymous(cred);
				talloc_free(mem_ctx);
				return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
			}
		}
	}

	salt_principal = ldb_msg_find_attr_as_string(msgs[0], "saltPrincipal", NULL);
	cli_credentials_set_salt_principal(cred, salt_principal);
	
	sct = ldb_msg_find_attr_as_int(msgs[0], "secureChannelType", 0);
	if (sct) { 
		cli_credentials_set_secure_channel_type(cred, sct);
	}
	
	if (!password) {
		const struct ldb_val *nt_password_hash = ldb_msg_find_ldb_val(msgs[0], "unicodePwd");
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

	
	domain = ldb_msg_find_attr_as_string(msgs[0], "flatname", NULL);
	if (domain) {
		cli_credentials_set_domain(cred, domain, CRED_SPECIFIED);
	}

	realm = ldb_msg_find_attr_as_string(msgs[0], "realm", NULL);
	if (realm) {
		cli_credentials_set_realm(cred, realm, CRED_SPECIFIED);
	}

	if (machine_account) {
		cli_credentials_set_username(cred, machine_account, CRED_SPECIFIED);
	}

	cli_credentials_set_kvno(cred, ldb_msg_find_attr_as_int(msgs[0], "msDS-KeyVersionNumber", 0));

	/* If there was an external keytab specified by reference in
	 * the LDB, then use this.  Otherwise we will make one up
	 * (chewing CPU time) from the password */
	keytab = ldb_msg_find_attr_as_string(msgs[0], "krb5Keytab", NULL);
	if (keytab) {
		cli_credentials_set_keytab_name(cred, event_ctx, lp_ctx, keytab, CRED_SPECIFIED);
	} else {
		keytab = ldb_msg_find_attr_as_string(msgs[0], "privateKeytab", NULL);
		if (keytab) {
			keytab = talloc_asprintf(mem_ctx, "FILE:%s", private_path(mem_ctx, lp_ctx, keytab));
			if (keytab) {
				cli_credentials_set_keytab_name(cred, event_ctx, lp_ctx, keytab, CRED_SPECIFIED);
			}
		}
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
	/* Bleh, nasty recursion issues: We are setting a machine
	 * account here, so we don't want the 'pending' flag around
	 * any more */
	cred->machine_account_pending = false;
	filter = talloc_asprintf(cred, SECRETS_PRIMARY_DOMAIN_FILTER, 
				       cli_credentials_get_domain(cred));
	status = cli_credentials_set_secrets(cred, event_context_find(cred), lp_ctx, NULL, 
					   SECRETS_PRIMARY_DOMAIN_DN,
					   filter);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not find machine account in secrets database: %s", nt_errstr(status)));
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
			            struct tevent_context *event_ctx,
				    struct loadparm_context *lp_ctx)
{
	NTSTATUS status;
	char *filter;
	/* Bleh, nasty recursion issues: We are setting a machine
	 * account here, so we don't want the 'pending' flag around
	 * any more */
	cred->machine_account_pending = false;
	filter = talloc_asprintf(cred, SECRETS_KRBTGT_SEARCH,
				       cli_credentials_get_realm(cred),
				       cli_credentials_get_domain(cred));
	status = cli_credentials_set_secrets(cred, event_ctx, lp_ctx, NULL, 
					   SECRETS_PRINCIPALS_DN,
					   filter);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not find krbtgt (master Kerberos) account in secrets database: %s", nt_errstr(status)));
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
						       struct tevent_context *event_ctx,
					      struct loadparm_context *lp_ctx,
					      const char *serviceprincipal)
{
	NTSTATUS status;
	char *filter;
	/* Bleh, nasty recursion issues: We are setting a machine
	 * account here, so we don't want the 'pending' flag around
	 * any more */
	cred->machine_account_pending = false;
	filter = talloc_asprintf(cred, SECRETS_PRINCIPAL_SEARCH,
				 cli_credentials_get_realm(cred),
				 cli_credentials_get_domain(cred),
				 serviceprincipal);
	status = cli_credentials_set_secrets(cred, event_ctx, lp_ctx, NULL, 
					   SECRETS_PRINCIPALS_DN, filter);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not find %s principal in secrets database: %s", serviceprincipal, nt_errstr(status)));
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


