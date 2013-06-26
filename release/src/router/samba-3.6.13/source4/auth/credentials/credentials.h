/* 
   samba -- Unix SMB/CIFS implementation.

   Client credentials structure

   Copyright (C) Jelmer Vernooij 2004-2006
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
#ifndef __CREDENTIALS_H__
#define __CREDENTIALS_H__

#include "../lib/util/data_blob.h"
#include "librpc/gen_ndr/misc.h"

struct ccache_container;
struct tevent_context;

/* In order of priority */
enum credentials_obtained { 
	CRED_UNINITIALISED = 0,  /* We don't even have a guess yet */
	CRED_CALLBACK, 		 /* Callback should be used to obtain value */
	CRED_GUESS_ENV,	         /* Current value should be used, which was guessed */
	CRED_GUESS_FILE,	 /* A guess from a file (or file pointed at in env variable) */
	CRED_CALLBACK_RESULT,    /* Value was obtained from a callback */
	CRED_SPECIFIED		 /* Was explicitly specified on the command-line */
};

enum credentials_use_kerberos {
	CRED_AUTO_USE_KERBEROS = 0, /* Default, we try kerberos if available */
	CRED_DONT_USE_KERBEROS,     /* Sometimes trying kerberos just does 'bad things', so don't */
	CRED_MUST_USE_KERBEROS      /* Sometimes administrators are parinoid, so always do kerberos */
};

enum credentials_krb_forwardable {
	CRED_AUTO_KRB_FORWARDABLE = 0, /* Default, follow library defaults */
	CRED_NO_KRB_FORWARDABLE,       /* not forwardable */
	CRED_FORCE_KRB_FORWARDABLE     /* forwardable */
};

#define CLI_CRED_NTLM2       0x01
#define CLI_CRED_NTLMv2_AUTH 0x02
#define CLI_CRED_LANMAN_AUTH 0x04
#define CLI_CRED_NTLM_AUTH   0x08
#define CLI_CRED_CLEAR_AUTH  0x10   /* TODO:  Push cleartext auth with this flag */

struct cli_credentials {
	enum credentials_obtained workstation_obtained;
	enum credentials_obtained username_obtained;
	enum credentials_obtained password_obtained;
	enum credentials_obtained domain_obtained;
	enum credentials_obtained realm_obtained;
	enum credentials_obtained ccache_obtained;
	enum credentials_obtained client_gss_creds_obtained;
	enum credentials_obtained principal_obtained;
	enum credentials_obtained keytab_obtained;
	enum credentials_obtained server_gss_creds_obtained;

	/* Threshold values (essentially a MAX() over a number of the
	 * above) for the ccache and GSS credentials, to ensure we
	 * regenerate/pick correctly */

	enum credentials_obtained ccache_threshold;
	enum credentials_obtained client_gss_creds_threshold;

	const char *workstation;
	const char *username;
	const char *password;
	const char *old_password;
	const char *domain;
	const char *realm;
	const char *principal;
	char *salt_principal;
	char *impersonate_principal;
	char *target_service;

	const char *bind_dn;

	/* Allows authentication from a keytab or similar */
	struct samr_Password *nt_hash;

	/* Allows NTLM pass-though authentication */
	DATA_BLOB lm_response;
	DATA_BLOB nt_response;

	struct ccache_container *ccache;
	struct gssapi_creds_container *client_gss_creds;
	struct keytab_container *keytab;
	struct gssapi_creds_container *server_gss_creds;

	const char *(*workstation_cb) (struct cli_credentials *);
	const char *(*password_cb) (struct cli_credentials *);
	const char *(*username_cb) (struct cli_credentials *);
	const char *(*domain_cb) (struct cli_credentials *);
	const char *(*realm_cb) (struct cli_credentials *);
	const char *(*principal_cb) (struct cli_credentials *);

	/* Private handle for the callback routines to use */
	void *priv_data;

	struct netlogon_creds_CredentialState *netlogon_creds;
	enum netr_SchannelType secure_channel_type;
	int kvno;
	time_t password_last_changed_time;

	struct smb_krb5_context *smb_krb5_context;

	/* We are flagged to get machine account details from the
	 * secrets.ldb when we are asked for a username or password */
	bool machine_account_pending;
	struct loadparm_context *machine_account_pending_lp_ctx;
	
	/* Is this a machine account? */
	bool machine_account;

	/* Should we be trying to use kerberos? */
	enum credentials_use_kerberos use_kerberos;

	/* Should we get a forwardable ticket? */
	enum credentials_krb_forwardable krb_forwardable;

	/* gensec features which should be used for connections */
	uint32_t gensec_features;

	/* Number of retries left before bailing out */
	int tries;

	/* Whether any callback is currently running */
	bool callback_running;
};

struct ldb_context;
struct ldb_message;
struct loadparm_context;
struct ccache_container;

struct gssapi_creds_container;

const char *cli_credentials_get_workstation(struct cli_credentials *cred);
bool cli_credentials_set_workstation(struct cli_credentials *cred, 
				     const char *val, 
				     enum credentials_obtained obtained);
bool cli_credentials_is_anonymous(struct cli_credentials *cred);
struct cli_credentials *cli_credentials_init(TALLOC_CTX *mem_ctx);
void cli_credentials_set_anonymous(struct cli_credentials *cred);
bool cli_credentials_wrong_password(struct cli_credentials *cred);
const char *cli_credentials_get_password(struct cli_credentials *cred);
void cli_credentials_get_ntlm_username_domain(struct cli_credentials *cred, TALLOC_CTX *mem_ctx, 
					      const char **username, 
					      const char **domain);
NTSTATUS cli_credentials_get_ntlm_response(struct cli_credentials *cred, TALLOC_CTX *mem_ctx, 
					   int *flags,
					   DATA_BLOB challenge, DATA_BLOB target_info, 
					   DATA_BLOB *_lm_response, DATA_BLOB *_nt_response, 
					   DATA_BLOB *_lm_session_key, DATA_BLOB *_session_key);
const char *cli_credentials_get_realm(struct cli_credentials *cred);
const char *cli_credentials_get_username(struct cli_credentials *cred);
int cli_credentials_get_krb5_context(struct cli_credentials *cred, 
				     struct loadparm_context *lp_ctx,
				     struct smb_krb5_context **smb_krb5_context);
int cli_credentials_get_ccache(struct cli_credentials *cred, 
			       struct tevent_context *event_ctx,
			       struct loadparm_context *lp_ctx,
			       struct ccache_container **ccc,
			       const char **error_string);
int cli_credentials_get_named_ccache(struct cli_credentials *cred, 
				     struct tevent_context *event_ctx,
				     struct loadparm_context *lp_ctx,
				     char *ccache_name,
				     struct ccache_container **ccc, const char **error_string);
int cli_credentials_get_keytab(struct cli_credentials *cred, 
			       struct loadparm_context *lp_ctx,
			       struct keytab_container **_ktc);
const char *cli_credentials_get_domain(struct cli_credentials *cred);
struct netlogon_creds_CredentialState *cli_credentials_get_netlogon_creds(struct cli_credentials *cred);
void cli_credentials_set_machine_account_pending(struct cli_credentials *cred,
						 struct loadparm_context *lp_ctx);
void cli_credentials_set_conf(struct cli_credentials *cred, 
			      struct loadparm_context *lp_ctx);
const char *cli_credentials_get_principal(struct cli_credentials *cred, TALLOC_CTX *mem_ctx);
int cli_credentials_get_server_gss_creds(struct cli_credentials *cred, 
					 struct loadparm_context *lp_ctx,
					 struct gssapi_creds_container **_gcc);
int cli_credentials_get_client_gss_creds(struct cli_credentials *cred, 
					 struct tevent_context *event_ctx,
					 struct loadparm_context *lp_ctx,
					 struct gssapi_creds_container **_gcc,
					 const char **error_string);
void cli_credentials_set_kerberos_state(struct cli_credentials *creds, 
					enum credentials_use_kerberos use_kerberos);
void cli_credentials_set_krb_forwardable(struct cli_credentials *creds,
					 enum credentials_krb_forwardable krb_forwardable);
bool cli_credentials_set_domain(struct cli_credentials *cred, 
				const char *val, 
				enum credentials_obtained obtained);
bool cli_credentials_set_domain_callback(struct cli_credentials *cred,
					 const char *(*domain_cb) (struct cli_credentials *));
bool cli_credentials_set_username(struct cli_credentials *cred, 
				  const char *val, enum credentials_obtained obtained);
bool cli_credentials_set_username_callback(struct cli_credentials *cred,
				  const char *(*username_cb) (struct cli_credentials *));
bool cli_credentials_set_principal(struct cli_credentials *cred, 
				   const char *val, 
				   enum credentials_obtained obtained);
bool cli_credentials_set_principal_callback(struct cli_credentials *cred,
				  const char *(*principal_cb) (struct cli_credentials *));
bool cli_credentials_set_password(struct cli_credentials *cred, 
				  const char *val, 
				  enum credentials_obtained obtained);
struct cli_credentials *cli_credentials_init_anon(TALLOC_CTX *mem_ctx);
void cli_credentials_parse_string(struct cli_credentials *credentials, const char *data, enum credentials_obtained obtained);
const struct samr_Password *cli_credentials_get_nt_hash(struct cli_credentials *cred, 
							TALLOC_CTX *mem_ctx);
bool cli_credentials_set_realm(struct cli_credentials *cred, 
			       const char *val, 
			       enum credentials_obtained obtained);
void cli_credentials_set_secure_channel_type(struct cli_credentials *cred,
				     enum netr_SchannelType secure_channel_type);
void cli_credentials_set_password_last_changed_time(struct cli_credentials *cred,
							     time_t last_change_time);
void cli_credentials_set_netlogon_creds(struct cli_credentials *cred, 
					struct netlogon_creds_CredentialState *netlogon_creds);
NTSTATUS cli_credentials_set_krb5_context(struct cli_credentials *cred, 
					  struct smb_krb5_context *smb_krb5_context);
NTSTATUS cli_credentials_set_stored_principal(struct cli_credentials *cred,
					      struct loadparm_context *lp_ctx,
					      const char *serviceprincipal);
NTSTATUS cli_credentials_set_machine_account(struct cli_credentials *cred,
					     struct loadparm_context *lp_ctx);
bool cli_credentials_authentication_requested(struct cli_credentials *cred);
void cli_credentials_guess(struct cli_credentials *cred,
			   struct loadparm_context *lp_ctx);
bool cli_credentials_set_bind_dn(struct cli_credentials *cred, 
				 const char *bind_dn);
const char *cli_credentials_get_bind_dn(struct cli_credentials *cred);
bool cli_credentials_parse_file(struct cli_credentials *cred, const char *file, enum credentials_obtained obtained);
const char *cli_credentials_get_unparsed_name(struct cli_credentials *credentials, TALLOC_CTX *mem_ctx);
bool cli_credentials_set_password_callback(struct cli_credentials *cred,
					   const char *(*password_cb) (struct cli_credentials *));
enum netr_SchannelType cli_credentials_get_secure_channel_type(struct cli_credentials *cred);
time_t cli_credentials_get_password_last_changed_time(struct cli_credentials *cred);
void cli_credentials_set_kvno(struct cli_credentials *cred,
			      int kvno);
bool cli_credentials_set_nt_hash(struct cli_credentials *cred,
				 const struct samr_Password *nt_hash, 
				 enum credentials_obtained obtained);
bool cli_credentials_set_ntlm_response(struct cli_credentials *cred,
				       const DATA_BLOB *lm_response, 
				       const DATA_BLOB *nt_response, 
				       enum credentials_obtained obtained);
int cli_credentials_set_keytab_name(struct cli_credentials *cred, 
				    struct loadparm_context *lp_ctx,
				    const char *keytab_name, 
				    enum credentials_obtained obtained);
void cli_credentials_set_gensec_features(struct cli_credentials *creds, uint32_t gensec_features);
uint32_t cli_credentials_get_gensec_features(struct cli_credentials *creds);
int cli_credentials_set_ccache(struct cli_credentials *cred, 
			       struct loadparm_context *lp_ctx,
			       const char *name, 
			       enum credentials_obtained obtained,
			       const char **error_string);
bool cli_credentials_parse_password_file(struct cli_credentials *credentials, const char *file, enum credentials_obtained obtained);
bool cli_credentials_parse_password_fd(struct cli_credentials *credentials, 
				       int fd, enum credentials_obtained obtained);
void cli_credentials_invalidate_ccache(struct cli_credentials *cred, 
				       enum credentials_obtained obtained);
void cli_credentials_set_salt_principal(struct cli_credentials *cred, const char *principal);
void cli_credentials_set_impersonate_principal(struct cli_credentials *cred, const char *principal);
void cli_credentials_set_target_service(struct cli_credentials *cred, const char *principal);
const char *cli_credentials_get_salt_principal(struct cli_credentials *cred);
const char *cli_credentials_get_impersonate_principal(struct cli_credentials *cred);
const char *cli_credentials_get_target_service(struct cli_credentials *cred);
enum credentials_use_kerberos cli_credentials_get_kerberos_state(struct cli_credentials *creds);
enum credentials_krb_forwardable cli_credentials_get_krb_forwardable(struct cli_credentials *creds);
NTSTATUS cli_credentials_set_secrets(struct cli_credentials *cred, 
				     struct loadparm_context *lp_ctx,
				     struct ldb_context *ldb,
				     const char *base,
				     const char *filter, 
				     char **error_string);
 int cli_credentials_get_kvno(struct cli_credentials *cred);


#endif /* __CREDENTIALS_H__ */
