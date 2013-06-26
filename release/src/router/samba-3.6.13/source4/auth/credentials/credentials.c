/* 
   Unix SMB/CIFS implementation.

   User credentials handling

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
#include "librpc/gen_ndr/samr.h" /* for struct samrPassword */
#include "auth/credentials/credentials.h"
#include "libcli/auth/libcli_auth.h"
#include "lib/events/events.h"
#include "param/param.h"
#include "system/filesys.h"

/**
 * Create a new credentials structure
 * @param mem_ctx TALLOC_CTX parent for credentials structure 
 */
_PUBLIC_ struct cli_credentials *cli_credentials_init(TALLOC_CTX *mem_ctx) 
{
	struct cli_credentials *cred = talloc(mem_ctx, struct cli_credentials);
	if (cred == NULL) {
		return cred;
	}

	cred->workstation_obtained = CRED_UNINITIALISED;
	cred->username_obtained = CRED_UNINITIALISED;
	cred->password_obtained = CRED_UNINITIALISED;
	cred->domain_obtained = CRED_UNINITIALISED;
	cred->realm_obtained = CRED_UNINITIALISED;
	cred->ccache_obtained = CRED_UNINITIALISED;
	cred->client_gss_creds_obtained = CRED_UNINITIALISED;
	cred->principal_obtained = CRED_UNINITIALISED;
	cred->keytab_obtained = CRED_UNINITIALISED;
	cred->server_gss_creds_obtained = CRED_UNINITIALISED;

	cred->ccache_threshold = CRED_UNINITIALISED;
	cred->client_gss_creds_threshold = CRED_UNINITIALISED;

	cred->workstation = NULL;
	cred->username = NULL;
	cred->password = NULL;
	cred->old_password = NULL;
	cred->domain = NULL;
	cred->realm = NULL;
	cred->principal = NULL;
	cred->salt_principal = NULL;
	cred->impersonate_principal = NULL;
	cred->target_service = NULL;

	cred->bind_dn = NULL;

	cred->nt_hash = NULL;

	cred->lm_response.data = NULL;
	cred->lm_response.length = 0;
	cred->nt_response.data = NULL;
	cred->nt_response.length = 0;

	cred->ccache = NULL;
	cred->client_gss_creds = NULL;
	cred->keytab = NULL;
	cred->server_gss_creds = NULL;

	cred->workstation_cb = NULL;
	cred->password_cb = NULL;
	cred->username_cb = NULL;
	cred->domain_cb = NULL;
	cred->realm_cb = NULL;
	cred->principal_cb = NULL;

	cred->priv_data = NULL;

	cred->netlogon_creds = NULL;
	cred->secure_channel_type = SEC_CHAN_NULL;

	cred->kvno = 0;

	cred->password_last_changed_time = 0;

	cred->smb_krb5_context = NULL;

	cred->machine_account_pending = false;
	cred->machine_account_pending_lp_ctx = NULL;

	cred->machine_account = false;

	cred->tries = 3;

	cred->callback_running = false;

	cli_credentials_set_kerberos_state(cred, CRED_AUTO_USE_KERBEROS);
	cli_credentials_set_gensec_features(cred, 0);
	cli_credentials_set_krb_forwardable(cred, CRED_AUTO_KRB_FORWARDABLE);

	return cred;
}

/**
 * Create a new anonymous credential
 * @param mem_ctx TALLOC_CTX parent for credentials structure 
 */
_PUBLIC_ struct cli_credentials *cli_credentials_init_anon(TALLOC_CTX *mem_ctx)
{
	struct cli_credentials *anon_credentials;

	anon_credentials = cli_credentials_init(mem_ctx);
	cli_credentials_set_anonymous(anon_credentials);

	return anon_credentials;
}

_PUBLIC_ void cli_credentials_set_kerberos_state(struct cli_credentials *creds, 
					enum credentials_use_kerberos use_kerberos)
{
	creds->use_kerberos = use_kerberos;
}

_PUBLIC_ void cli_credentials_set_krb_forwardable(struct cli_credentials *creds,
						  enum credentials_krb_forwardable krb_forwardable)
{
	creds->krb_forwardable = krb_forwardable;
}

_PUBLIC_ enum credentials_use_kerberos cli_credentials_get_kerberos_state(struct cli_credentials *creds)
{
	return creds->use_kerberos;
}

_PUBLIC_ enum credentials_krb_forwardable cli_credentials_get_krb_forwardable(struct cli_credentials *creds)
{
	return creds->krb_forwardable;
}

_PUBLIC_ void cli_credentials_set_gensec_features(struct cli_credentials *creds, uint32_t gensec_features)
{
	creds->gensec_features = gensec_features;
}

_PUBLIC_ uint32_t cli_credentials_get_gensec_features(struct cli_credentials *creds)
{
	return creds->gensec_features;
}


/**
 * Obtain the username for this credentials context.
 * @param cred credentials context
 * @retval The username set on this context.
 * @note Return value will never be NULL except by programmer error.
 */
_PUBLIC_ const char *cli_credentials_get_username(struct cli_credentials *cred)
{
	if (cred->machine_account_pending) {
		cli_credentials_set_machine_account(cred, 
					cred->machine_account_pending_lp_ctx);
	}

	if (cred->username_obtained == CRED_CALLBACK && 
	    !cred->callback_running) {
	    	cred->callback_running = true;
		cred->username = cred->username_cb(cred);
	    	cred->callback_running = false;
		cred->username_obtained = CRED_SPECIFIED;
		cli_credentials_invalidate_ccache(cred, cred->username_obtained);
	}

	return cred->username;
}

_PUBLIC_ bool cli_credentials_set_username(struct cli_credentials *cred, 
				  const char *val, enum credentials_obtained obtained)
{
	if (obtained >= cred->username_obtained) {
		cred->username = talloc_strdup(cred, val);
		cred->username_obtained = obtained;
		cli_credentials_invalidate_ccache(cred, cred->username_obtained);
		return true;
	}

	return false;
}

bool cli_credentials_set_username_callback(struct cli_credentials *cred,
				  const char *(*username_cb) (struct cli_credentials *))
{
	if (cred->username_obtained < CRED_CALLBACK) {
		cred->username_cb = username_cb;
		cred->username_obtained = CRED_CALLBACK;
		return true;
	}

	return false;
}

_PUBLIC_ bool cli_credentials_set_bind_dn(struct cli_credentials *cred, 
				 const char *bind_dn)
{
	cred->bind_dn = talloc_strdup(cred, bind_dn);
	return true;
}

/**
 * Obtain the BIND DN for this credentials context.
 * @param cred credentials context
 * @retval The username set on this context.
 * @note Return value will be NULL if not specified explictly
 */
_PUBLIC_ const char *cli_credentials_get_bind_dn(struct cli_credentials *cred)
{
	return cred->bind_dn;
}


/**
 * Obtain the client principal for this credentials context.
 * @param cred credentials context
 * @retval The username set on this context.
 * @note Return value will never be NULL except by programmer error.
 */
const char *cli_credentials_get_principal_and_obtained(struct cli_credentials *cred, TALLOC_CTX *mem_ctx, enum credentials_obtained *obtained)
{
	if (cred->machine_account_pending) {
		cli_credentials_set_machine_account(cred,
					cred->machine_account_pending_lp_ctx);
	}

	if (cred->principal_obtained == CRED_CALLBACK && 
	    !cred->callback_running) {
	    	cred->callback_running = true;
		cred->principal = cred->principal_cb(cred);
	    	cred->callback_running = false;
		cred->principal_obtained = CRED_SPECIFIED;
		cli_credentials_invalidate_ccache(cred, cred->principal_obtained);
	}

	if (cred->principal_obtained < cred->username_obtained
	    || cred->principal_obtained < MAX(cred->domain_obtained, cred->realm_obtained)) {
		if (cred->domain_obtained > cred->realm_obtained) {
			*obtained = MIN(cred->domain_obtained, cred->username_obtained);
			return talloc_asprintf(mem_ctx, "%s@%s", 
					       cli_credentials_get_username(cred),
					       cli_credentials_get_domain(cred));
		} else {
			*obtained = MIN(cred->domain_obtained, cred->username_obtained);
			return talloc_asprintf(mem_ctx, "%s@%s", 
					       cli_credentials_get_username(cred),
					       cli_credentials_get_realm(cred));
		}
	}
	*obtained = cred->principal_obtained;
	return talloc_reference(mem_ctx, cred->principal);
}

/**
 * Obtain the client principal for this credentials context.
 * @param cred credentials context
 * @retval The username set on this context.
 * @note Return value will never be NULL except by programmer error.
 */
_PUBLIC_ const char *cli_credentials_get_principal(struct cli_credentials *cred, TALLOC_CTX *mem_ctx)
{
	enum credentials_obtained obtained;
	return cli_credentials_get_principal_and_obtained(cred, mem_ctx, &obtained);
}

bool cli_credentials_set_principal(struct cli_credentials *cred, 
				   const char *val, 
				   enum credentials_obtained obtained)
{
	if (obtained >= cred->principal_obtained) {
		cred->principal = talloc_strdup(cred, val);
		cred->principal_obtained = obtained;
		cli_credentials_invalidate_ccache(cred, cred->principal_obtained);
		return true;
	}

	return false;
}

/* Set a callback to get the principal.  This could be a popup dialog,
 * a terminal prompt or similar.  */
bool cli_credentials_set_principal_callback(struct cli_credentials *cred,
				  const char *(*principal_cb) (struct cli_credentials *))
{
	if (cred->principal_obtained < CRED_CALLBACK) {
		cred->principal_cb = principal_cb;
		cred->principal_obtained = CRED_CALLBACK;
		return true;
	}

	return false;
}

/* Some of our tools are 'anonymous by default'.  This is a single
 * function to determine if authentication has been explicitly
 * requested */

_PUBLIC_ bool cli_credentials_authentication_requested(struct cli_credentials *cred) 
{
	if (cred->bind_dn) {
		return true;
	}

	if (cli_credentials_is_anonymous(cred)){
		return false;
	}

	if (cred->principal_obtained >= CRED_SPECIFIED) {
		return true;
	}
	if (cred->username_obtained >= CRED_SPECIFIED) {
		return true;
	}

	if (cli_credentials_get_kerberos_state(cred) == CRED_MUST_USE_KERBEROS) {
		return true;
	}

	return false;
}

/**
 * Obtain the password for this credentials context.
 * @param cred credentials context
 * @retval If set, the cleartext password, otherwise NULL
 */
_PUBLIC_ const char *cli_credentials_get_password(struct cli_credentials *cred)
{
	if (cred->machine_account_pending) {
		cli_credentials_set_machine_account(cred,
						    cred->machine_account_pending_lp_ctx);
	}

	if (cred->password_obtained == CRED_CALLBACK && 
	    !cred->callback_running) {
	    	cred->callback_running = true;
		cred->password = cred->password_cb(cred);
	    	cred->callback_running = false;
		cred->password_obtained = CRED_CALLBACK_RESULT;
		cli_credentials_invalidate_ccache(cred, cred->password_obtained);
	}

	return cred->password;
}

/* Set a password on the credentials context, including an indication
 * of 'how' the password was obtained */

_PUBLIC_ bool cli_credentials_set_password(struct cli_credentials *cred, 
				  const char *val, 
				  enum credentials_obtained obtained)
{
	if (obtained >= cred->password_obtained) {
		cred->password = talloc_strdup(cred, val);
		cred->password_obtained = obtained;
		cli_credentials_invalidate_ccache(cred, cred->password_obtained);

		cred->nt_hash = NULL;
		cred->lm_response = data_blob(NULL, 0);
		cred->nt_response = data_blob(NULL, 0);
		return true;
	}

	return false;
}

_PUBLIC_ bool cli_credentials_set_password_callback(struct cli_credentials *cred,
					   const char *(*password_cb) (struct cli_credentials *))
{
	if (cred->password_obtained < CRED_CALLBACK) {
		cred->password_cb = password_cb;
		cred->password_obtained = CRED_CALLBACK;
		cli_credentials_invalidate_ccache(cred, cred->password_obtained);
		return true;
	}

	return false;
}

/**
 * Obtain the 'old' password for this credentials context (used for join accounts).
 * @param cred credentials context
 * @retval If set, the cleartext password, otherwise NULL
 */
const char *cli_credentials_get_old_password(struct cli_credentials *cred)
{
	if (cred->machine_account_pending) {
		cli_credentials_set_machine_account(cred,
						    cred->machine_account_pending_lp_ctx);
	}

	return cred->old_password;
}

bool cli_credentials_set_old_password(struct cli_credentials *cred, 
				      const char *val, 
				      enum credentials_obtained obtained)
{
	cred->old_password = talloc_strdup(cred, val);
	return true;
}

/**
 * Obtain the password, in the form MD4(unicode(password)) for this credentials context.
 *
 * Sometimes we only have this much of the password, while the rest of
 * the time this call avoids calling E_md4hash themselves.
 *
 * @param cred credentials context
 * @retval If set, the cleartext password, otherwise NULL
 */
_PUBLIC_ const struct samr_Password *cli_credentials_get_nt_hash(struct cli_credentials *cred, 
							TALLOC_CTX *mem_ctx)
{
	const char *password = cli_credentials_get_password(cred);

	if (password) {
		struct samr_Password *nt_hash = talloc(mem_ctx, struct samr_Password);
		if (!nt_hash) {
			return NULL;
		}
		
		E_md4hash(password, nt_hash->hash);    

		return nt_hash;
	} else {
		return cred->nt_hash;
	}
}

/**
 * Obtain the 'short' or 'NetBIOS' domain for this credentials context.
 * @param cred credentials context
 * @retval The domain set on this context. 
 * @note Return value will never be NULL except by programmer error.
 */
_PUBLIC_ const char *cli_credentials_get_domain(struct cli_credentials *cred)
{
	if (cred->machine_account_pending) {
		cli_credentials_set_machine_account(cred,
						    cred->machine_account_pending_lp_ctx);
	}

	if (cred->domain_obtained == CRED_CALLBACK && 
	    !cred->callback_running) {
	    	cred->callback_running = true;
		cred->domain = cred->domain_cb(cred);
	    	cred->callback_running = false;
		cred->domain_obtained = CRED_SPECIFIED;
		cli_credentials_invalidate_ccache(cred, cred->domain_obtained);
	}

	return cred->domain;
}


_PUBLIC_ bool cli_credentials_set_domain(struct cli_credentials *cred, 
				const char *val, 
				enum credentials_obtained obtained)
{
	if (obtained >= cred->domain_obtained) {
		/* it is important that the domain be in upper case,
		 * particularly for the sensitive NTLMv2
		 * calculations */
		cred->domain = strupper_talloc(cred, val);
		cred->domain_obtained = obtained;
		cli_credentials_invalidate_ccache(cred, cred->domain_obtained);
		return true;
	}

	return false;
}

bool cli_credentials_set_domain_callback(struct cli_credentials *cred,
					 const char *(*domain_cb) (struct cli_credentials *))
{
	if (cred->domain_obtained < CRED_CALLBACK) {
		cred->domain_cb = domain_cb;
		cred->domain_obtained = CRED_CALLBACK;
		return true;
	}

	return false;
}

/**
 * Obtain the Kerberos realm for this credentials context.
 * @param cred credentials context
 * @retval The realm set on this context. 
 * @note Return value will never be NULL except by programmer error.
 */
_PUBLIC_ const char *cli_credentials_get_realm(struct cli_credentials *cred)
{	
	if (cred->machine_account_pending) {
		cli_credentials_set_machine_account(cred,
						    cred->machine_account_pending_lp_ctx);
	}

	if (cred->realm_obtained == CRED_CALLBACK && 
	    !cred->callback_running) {
	    	cred->callback_running = true;
		cred->realm = cred->realm_cb(cred);
	    	cred->callback_running = false;
		cred->realm_obtained = CRED_SPECIFIED;
		cli_credentials_invalidate_ccache(cred, cred->realm_obtained);
	}

	return cred->realm;
}

/**
 * Set the realm for this credentials context, and force it to
 * uppercase for the sainity of our local kerberos libraries 
 */
_PUBLIC_ bool cli_credentials_set_realm(struct cli_credentials *cred, 
			       const char *val, 
			       enum credentials_obtained obtained)
{
	if (obtained >= cred->realm_obtained) {
		cred->realm = strupper_talloc(cred, val);
		cred->realm_obtained = obtained;
		cli_credentials_invalidate_ccache(cred, cred->realm_obtained);
		return true;
	}

	return false;
}

bool cli_credentials_set_realm_callback(struct cli_credentials *cred,
					const char *(*realm_cb) (struct cli_credentials *))
{
	if (cred->realm_obtained < CRED_CALLBACK) {
		cred->realm_cb = realm_cb;
		cred->realm_obtained = CRED_CALLBACK;
		return true;
	}

	return false;
}

/**
 * Obtain the 'short' or 'NetBIOS' workstation name for this credentials context.
 *
 * @param cred credentials context
 * @retval The workstation name set on this context. 
 * @note Return value will never be NULL except by programmer error.
 */
_PUBLIC_ const char *cli_credentials_get_workstation(struct cli_credentials *cred)
{
	if (cred->workstation_obtained == CRED_CALLBACK && 
	    !cred->callback_running) {
	    	cred->callback_running = true;
		cred->workstation = cred->workstation_cb(cred);
	    	cred->callback_running = false;
		cred->workstation_obtained = CRED_SPECIFIED;
	}

	return cred->workstation;
}

_PUBLIC_ bool cli_credentials_set_workstation(struct cli_credentials *cred, 
				     const char *val, 
				     enum credentials_obtained obtained)
{
	if (obtained >= cred->workstation_obtained) {
		cred->workstation = talloc_strdup(cred, val);
		cred->workstation_obtained = obtained;
		return true;
	}

	return false;
}

bool cli_credentials_set_workstation_callback(struct cli_credentials *cred,
					      const char *(*workstation_cb) (struct cli_credentials *))
{
	if (cred->workstation_obtained < CRED_CALLBACK) {
		cred->workstation_cb = workstation_cb;
		cred->workstation_obtained = CRED_CALLBACK;
		return true;
	}

	return false;
}

/**
 * Given a string, typically obtained from a -U argument, parse it into domain, username, realm and password fields
 *
 * The format accepted is [domain\\]user[%password] or user[@realm][%password]
 *
 * @param credentials Credentials structure on which to set the password
 * @param data the string containing the username, password etc
 * @param obtained This enum describes how 'specified' this password is
 */

_PUBLIC_ void cli_credentials_parse_string(struct cli_credentials *credentials, const char *data, enum credentials_obtained obtained)
{
	char *uname, *p;

	if (strcmp("%",data) == 0) {
		cli_credentials_set_anonymous(credentials);
		return;
	}

	uname = talloc_strdup(credentials, data); 
	if ((p = strchr_m(uname,'%'))) {
		*p = 0;
		cli_credentials_set_password(credentials, p+1, obtained);
	}

	if ((p = strchr_m(uname,'@'))) {
		cli_credentials_set_principal(credentials, uname, obtained);
		*p = 0;
		cli_credentials_set_realm(credentials, p+1, obtained);
		return;
	} else if ((p = strchr_m(uname,'\\')) || (p = strchr_m(uname, '/'))) {
		*p = 0;
		cli_credentials_set_domain(credentials, uname, obtained);
		uname = p+1;
	}
	cli_credentials_set_username(credentials, uname, obtained);
}

/**
 * Given a a credentials structure, print it as a string
 *
 * The format output is [domain\\]user[%password] or user[@realm][%password]
 *
 * @param credentials Credentials structure on which to set the password
 * @param mem_ctx The memory context to place the result on
 */

_PUBLIC_ const char *cli_credentials_get_unparsed_name(struct cli_credentials *credentials, TALLOC_CTX *mem_ctx)
{
	const char *bind_dn = cli_credentials_get_bind_dn(credentials);
	const char *domain;
	const char *username;
	const char *name;

	if (bind_dn) {
		name = talloc_reference(mem_ctx, bind_dn);
	} else {
		cli_credentials_get_ntlm_username_domain(credentials, mem_ctx, &username, &domain);
		if (domain && domain[0]) {
			name = talloc_asprintf(mem_ctx, "%s\\%s", 
					       domain, username);
		} else {
			name = talloc_asprintf(mem_ctx, "%s", 
					       username);
		}
	}
	return name;
}

/**
 * Specifies default values for domain, workstation and realm
 * from the smb.conf configuration file
 *
 * @param cred Credentials structure to fill in
 */
_PUBLIC_ void cli_credentials_set_conf(struct cli_credentials *cred, 
			      struct loadparm_context *lp_ctx)
{
	cli_credentials_set_username(cred, "", CRED_UNINITIALISED);
	cli_credentials_set_domain(cred, lpcfg_workgroup(lp_ctx), CRED_UNINITIALISED);
	cli_credentials_set_workstation(cred, lpcfg_netbios_name(lp_ctx), CRED_UNINITIALISED);
	cli_credentials_set_realm(cred, lpcfg_realm(lp_ctx), CRED_UNINITIALISED);
}

/**
 * Guess defaults for credentials from environment variables, 
 * and from the configuration file
 * 
 * @param cred Credentials structure to fill in
 */
_PUBLIC_ void cli_credentials_guess(struct cli_credentials *cred,
			   struct loadparm_context *lp_ctx)
{
	char *p;
	const char *error_string;

	if (lp_ctx != NULL) {
		cli_credentials_set_conf(cred, lp_ctx);
	}
	
	if (getenv("LOGNAME")) {
		cli_credentials_set_username(cred, getenv("LOGNAME"), CRED_GUESS_ENV);
	}

	if (getenv("USER")) {
		cli_credentials_parse_string(cred, getenv("USER"), CRED_GUESS_ENV);
		if ((p = strchr_m(getenv("USER"),'%'))) {
			memset(p,0,strlen(cred->password));
		}
	}

	if (getenv("PASSWD")) {
		cli_credentials_set_password(cred, getenv("PASSWD"), CRED_GUESS_ENV);
	}

	if (getenv("PASSWD_FD")) {
		cli_credentials_parse_password_fd(cred, atoi(getenv("PASSWD_FD")), 
						  CRED_GUESS_FILE);
	}
	
	p = getenv("PASSWD_FILE");
	if (p && p[0]) {
		cli_credentials_parse_password_file(cred, p, CRED_GUESS_FILE);
	}
	
	if (cli_credentials_get_kerberos_state(cred) != CRED_DONT_USE_KERBEROS) {
		cli_credentials_set_ccache(cred, lp_ctx, NULL, CRED_GUESS_FILE,
					   &error_string);
	}
}

/**
 * Attach NETLOGON credentials for use with SCHANNEL
 */

_PUBLIC_ void cli_credentials_set_netlogon_creds(struct cli_credentials *cred, 
						 struct netlogon_creds_CredentialState *netlogon_creds)
{
	cred->netlogon_creds = talloc_reference(cred, netlogon_creds);
}

/**
 * Return attached NETLOGON credentials 
 */

struct netlogon_creds_CredentialState *cli_credentials_get_netlogon_creds(struct cli_credentials *cred)
{
	return cred->netlogon_creds;
}

/** 
 * Set NETLOGON secure channel type
 */

_PUBLIC_ void cli_credentials_set_secure_channel_type(struct cli_credentials *cred,
					     enum netr_SchannelType secure_channel_type)
{
	cred->secure_channel_type = secure_channel_type;
}

/**
 * Return NETLOGON secure chanel type
 */

_PUBLIC_ time_t cli_credentials_get_password_last_changed_time(struct cli_credentials *cred)
{
	return cred->password_last_changed_time;
}

/** 
 * Set NETLOGON secure channel type
 */

_PUBLIC_ void cli_credentials_set_password_last_changed_time(struct cli_credentials *cred,
							     time_t last_changed_time)
{
	cred->password_last_changed_time = last_changed_time;
}

/**
 * Return NETLOGON secure chanel type
 */

_PUBLIC_ enum netr_SchannelType cli_credentials_get_secure_channel_type(struct cli_credentials *cred)
{
	return cred->secure_channel_type;
}

/**
 * Fill in a credentials structure as the anonymous user
 */
_PUBLIC_ void cli_credentials_set_anonymous(struct cli_credentials *cred) 
{
	cli_credentials_set_username(cred, "", CRED_SPECIFIED);
	cli_credentials_set_domain(cred, "", CRED_SPECIFIED);
	cli_credentials_set_password(cred, NULL, CRED_SPECIFIED);
	cli_credentials_set_realm(cred, NULL, CRED_SPECIFIED);
	cli_credentials_set_workstation(cred, "", CRED_UNINITIALISED);
}

/**
 * Describe a credentials context as anonymous or authenticated
 * @retval true if anonymous, false if a username is specified
 */

_PUBLIC_ bool cli_credentials_is_anonymous(struct cli_credentials *cred)
{
	const char *username;
	
	/* if bind dn is set it's not anonymous */
	if (cred->bind_dn) {
		return false;
	}

	if (cred->machine_account_pending) {
		cli_credentials_set_machine_account(cred,
						    cred->machine_account_pending_lp_ctx);
	}

	username = cli_credentials_get_username(cred);
	
	/* Yes, it is deliberate that we die if we have a NULL pointer
	 * here - anonymous is "", not NULL, which is 'never specified,
	 * never guessed', ie programmer bug */
	if (!username[0]) {
		return true;
	}

	return false;
}

/**
 * Mark the current password for a credentials struct as wrong. This will 
 * cause the password to be prompted again (if a callback is set).
 *
 * This will decrement the number of times the password can be tried.
 *
 * @retval whether the credentials struct is finished
 */
_PUBLIC_ bool cli_credentials_wrong_password(struct cli_credentials *cred)
{
	if (cred->password_obtained != CRED_CALLBACK_RESULT) {
		return false;
	}
	
	cred->password_obtained = CRED_CALLBACK;

	cred->tries--;

	return (cred->tries > 0);
}

_PUBLIC_ void cli_credentials_get_ntlm_username_domain(struct cli_credentials *cred, TALLOC_CTX *mem_ctx, 
					      const char **username, 
					      const char **domain) 
{
	if (cred->principal_obtained > cred->username_obtained) {
		*domain = talloc_strdup(mem_ctx, "");
		*username = cli_credentials_get_principal(cred, mem_ctx);
	} else {
		*domain = cli_credentials_get_domain(cred);
		*username = cli_credentials_get_username(cred);
	}
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


