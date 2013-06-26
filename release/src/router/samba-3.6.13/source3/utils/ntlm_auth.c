/*
   Unix SMB/CIFS implementation.

   Winbind status program.

   Copyright (C) Tim Potter      2000-2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2003-2004
   Copyright (C) Francesco Chemolli <kinkie@kame.usr.dsi.unimi.it> 2000
   Copyright (C) Robert O'Callahan 2006 (added cached credential code).
   Copyright (C) Kai Blin <kai@samba.org> 2008

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
#include "popt_common.h"
#include "utils/ntlm_auth.h"
#include "../libcli/auth/libcli_auth.h"
#include "../libcli/auth/spnego.h"
#include "../libcli/auth/ntlmssp.h"
#include "smb_krb5.h"
#include <iniparser.h>
#include "../lib/crypto/arcfour.h"
#include "libads/kerberos_proto.h"
#include "nsswitch/winbind_client.h"
#include "librpc/gen_ndr/krb5pac.h"
#include "../lib/util/asn1.h"

#ifndef PAM_WINBIND_CONFIG_FILE
#define PAM_WINBIND_CONFIG_FILE "/etc/security/pam_winbind.conf"
#endif

#define WINBIND_KRB5_AUTH	0x00000080

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

#define INITIAL_BUFFER_SIZE 300
#define MAX_BUFFER_SIZE 630000

enum stdio_helper_mode {
	SQUID_2_4_BASIC,
	SQUID_2_5_BASIC,
	SQUID_2_5_NTLMSSP,
	NTLMSSP_CLIENT_1,
	GSS_SPNEGO,
	GSS_SPNEGO_CLIENT,
	NTLM_SERVER_1,
	NTLM_CHANGE_PASSWORD_1,
	NUM_HELPER_MODES
};

enum ntlm_auth_cli_state {
	CLIENT_INITIAL = 0,
	CLIENT_RESPONSE,
	CLIENT_FINISHED,
	CLIENT_ERROR
};

enum ntlm_auth_svr_state {
	SERVER_INITIAL = 0,
	SERVER_CHALLENGE,
	SERVER_FINISHED,
	SERVER_ERROR
};

struct ntlm_auth_state {
	TALLOC_CTX *mem_ctx;
	enum stdio_helper_mode helper_mode;
	enum ntlm_auth_cli_state cli_state;
	enum ntlm_auth_svr_state svr_state;
	struct ntlmssp_state *ntlmssp_state;
	uint32_t neg_flags;
	char *want_feature_list;
	char *spnego_mech;
	char *spnego_mech_oid;
	bool have_session_key;
	DATA_BLOB session_key;
	DATA_BLOB initial_message;
};

typedef void (*stdio_helper_function)(struct ntlm_auth_state *state, char *buf,
					int length);

static void manage_squid_basic_request (struct ntlm_auth_state *state,
					char *buf, int length);

static void manage_squid_ntlmssp_request (struct ntlm_auth_state *state,
					char *buf, int length);

static void manage_client_ntlmssp_request (struct ntlm_auth_state *state,
					char *buf, int length);

static void manage_gss_spnego_request (struct ntlm_auth_state *state,
					char *buf, int length);

static void manage_gss_spnego_client_request (struct ntlm_auth_state *state,
					char *buf, int length);

static void manage_ntlm_server_1_request (struct ntlm_auth_state *state,
					char *buf, int length);

static void manage_ntlm_change_password_1_request(struct ntlm_auth_state *state,
					char *buf, int length);

static const struct {
	enum stdio_helper_mode mode;
	const char *name;
	stdio_helper_function fn;
} stdio_helper_protocols[] = {
	{ SQUID_2_4_BASIC, "squid-2.4-basic", manage_squid_basic_request},
	{ SQUID_2_5_BASIC, "squid-2.5-basic", manage_squid_basic_request},
	{ SQUID_2_5_NTLMSSP, "squid-2.5-ntlmssp", manage_squid_ntlmssp_request},
	{ NTLMSSP_CLIENT_1, "ntlmssp-client-1", manage_client_ntlmssp_request},
	{ GSS_SPNEGO, "gss-spnego", manage_gss_spnego_request},
	{ GSS_SPNEGO_CLIENT, "gss-spnego-client", manage_gss_spnego_client_request},
	{ NTLM_SERVER_1, "ntlm-server-1", manage_ntlm_server_1_request},
	{ NTLM_CHANGE_PASSWORD_1, "ntlm-change-password-1", manage_ntlm_change_password_1_request},
	{ NUM_HELPER_MODES, NULL, NULL}
};

const char *opt_username;
const char *opt_domain;
const char *opt_workstation;
const char *opt_password;
static DATA_BLOB opt_challenge;
static DATA_BLOB opt_lm_response;
static DATA_BLOB opt_nt_response;
static int request_lm_key;
static int request_user_session_key;
static int use_cached_creds;

static const char *require_membership_of;
static const char *require_membership_of_sid;
static const char *opt_pam_winbind_conf;

static char winbind_separator(void)
{
	struct winbindd_response response;
	static bool got_sep;
	static char sep;

	if (got_sep)
		return sep;

	ZERO_STRUCT(response);

	/* Send off request */

	if (winbindd_request_response(WINBINDD_INFO, NULL, &response) !=
	    NSS_STATUS_SUCCESS) {
		d_printf("could not obtain winbind separator!\n");
		return *lp_winbind_separator();
	}

	sep = response.data.info.winbind_separator;
	got_sep = True;

	if (!sep) {
		d_printf("winbind separator was NULL!\n");
		return *lp_winbind_separator();
	}

	return sep;
}

const char *get_winbind_domain(void)
{
	struct winbindd_response response;

	static fstring winbind_domain;
	if (*winbind_domain) {
		return winbind_domain;
	}

	ZERO_STRUCT(response);

	/* Send off request */

	if (winbindd_request_response(WINBINDD_DOMAIN_NAME, NULL, &response) !=
	    NSS_STATUS_SUCCESS) {
		DEBUG(0, ("could not obtain winbind domain name!\n"));
		return lp_workgroup();
	}

	fstrcpy(winbind_domain, response.data.domain_name);

	return winbind_domain;

}

const char *get_winbind_netbios_name(void)
{
	struct winbindd_response response;

	static fstring winbind_netbios_name;

	if (*winbind_netbios_name) {
		return winbind_netbios_name;
	}

	ZERO_STRUCT(response);

	/* Send off request */

	if (winbindd_request_response(WINBINDD_NETBIOS_NAME, NULL, &response) !=
	    NSS_STATUS_SUCCESS) {
		DEBUG(0, ("could not obtain winbind netbios name!\n"));
		return global_myname();
	}

	fstrcpy(winbind_netbios_name, response.data.netbios_name);

	return winbind_netbios_name;

}

DATA_BLOB get_challenge(void) 
{
	static DATA_BLOB chal;
	if (opt_challenge.length)
		return opt_challenge;

	chal = data_blob(NULL, 8);

	generate_random_buffer(chal.data, chal.length);
	return chal;
}

/* Copy of parse_domain_user from winbindd_util.c.  Parse a string of the
   form DOMAIN/user into a domain and a user */

static bool parse_ntlm_auth_domain_user(const char *domuser, fstring domain, 
				     fstring user)
{

	char *p = strchr(domuser,winbind_separator());

	if (!p) {
		return False;
	}

	fstrcpy(user, p+1);
	fstrcpy(domain, domuser);
	domain[PTR_DIFF(p, domuser)] = 0;
	strupper_m(domain);

	return True;
}

static bool get_require_membership_sid(void) {
	struct winbindd_request request;
	struct winbindd_response response;

	if (!require_membership_of) {
		return True;
	}

	if (require_membership_of_sid) {
		return True;
	}

	/* Otherwise, ask winbindd for the name->sid request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (!parse_ntlm_auth_domain_user(require_membership_of, 
					 request.data.name.dom_name, 
					 request.data.name.name)) {
		DEBUG(0, ("Could not parse %s into seperate domain/name parts!\n", 
			  require_membership_of));
		return False;
	}

	if (winbindd_request_response(WINBINDD_LOOKUPNAME, &request, &response) !=
	    NSS_STATUS_SUCCESS) {
		DEBUG(0, ("Winbindd lookupname failed to resolve %s into a SID!\n", 
			  require_membership_of));
		return False;
	}

	require_membership_of_sid = SMB_STRDUP(response.data.sid.sid);

	if (require_membership_of_sid)
		return True;

	return False;
}

/* 
 * Get some configuration from pam_winbind.conf to see if we 
 * need to contact trusted domain
 */

int get_pam_winbind_config()
{
	int ctrl = 0;
	dictionary *d = NULL;

	if (!opt_pam_winbind_conf || !*opt_pam_winbind_conf) {
		opt_pam_winbind_conf = PAM_WINBIND_CONFIG_FILE;
	}

	d = iniparser_load(CONST_DISCARD(char *, opt_pam_winbind_conf));

	if (!d) {
		return 0;
	}

	if (iniparser_getboolean(d, CONST_DISCARD(char *, "global:krb5_auth"), false)) {
		ctrl |= WINBIND_KRB5_AUTH;
	}

	iniparser_freedict(d);

	return ctrl;
}

/* Authenticate a user with a plaintext password */

static bool check_plaintext_auth(const char *user, const char *pass,
				 bool stdout_diagnostics)
{
	struct winbindd_request request;
	struct winbindd_response response;
        NSS_STATUS result;

	if (!get_require_membership_sid()) {
		return False;
	}

	/* Send off request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	fstrcpy(request.data.auth.user, user);
	fstrcpy(request.data.auth.pass, pass);
	if (require_membership_of_sid) {
		strlcpy(request.data.auth.require_membership_of_sid,
			require_membership_of_sid,
			sizeof(request.data.auth.require_membership_of_sid));
	}

	result = winbindd_request_response(WINBINDD_PAM_AUTH, &request, &response);

	/* Display response */

	if (stdout_diagnostics) {
		if ((result != NSS_STATUS_SUCCESS) && (response.data.auth.nt_status == 0)) {
			d_printf("Reading winbind reply failed! (0x01)\n");
		}

		d_printf("%s: %s (0x%x)\n",
			 response.data.auth.nt_status_string,
			 response.data.auth.error_string,
			 response.data.auth.nt_status);
	} else {
		if ((result != NSS_STATUS_SUCCESS) && (response.data.auth.nt_status == 0)) {
			DEBUG(1, ("Reading winbind reply failed! (0x01)\n"));
		}

		DEBUG(3, ("%s: %s (0x%x)\n",
			  response.data.auth.nt_status_string,
			  response.data.auth.error_string,
			  response.data.auth.nt_status));
	}

        return (result == NSS_STATUS_SUCCESS);
}

/* authenticate a user with an encrypted username/password */

NTSTATUS contact_winbind_auth_crap(const char *username,
				   const char *domain,
				   const char *workstation,
				   const DATA_BLOB *challenge,
				   const DATA_BLOB *lm_response,
				   const DATA_BLOB *nt_response,
				   uint32 flags,
				   uint8 lm_key[8],
				   uint8 user_session_key[16],
				   char **error_string,
				   char **unix_name)
{
	NTSTATUS nt_status;
        NSS_STATUS result;
	struct winbindd_request request;
	struct winbindd_response response;

	if (!get_require_membership_sid()) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	request.flags = flags;

	request.data.auth_crap.logon_parameters = MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT | MSV1_0_ALLOW_SERVER_TRUST_ACCOUNT;

	if (require_membership_of_sid)
		fstrcpy(request.data.auth_crap.require_membership_of_sid, require_membership_of_sid);

        fstrcpy(request.data.auth_crap.user, username);
	fstrcpy(request.data.auth_crap.domain, domain);

	fstrcpy(request.data.auth_crap.workstation, 
		workstation);

	memcpy(request.data.auth_crap.chal, challenge->data, MIN(challenge->length, 8));

	if (lm_response && lm_response->length) {
		memcpy(request.data.auth_crap.lm_resp, 
		       lm_response->data, 
		       MIN(lm_response->length, sizeof(request.data.auth_crap.lm_resp)));
		request.data.auth_crap.lm_resp_len = lm_response->length;
	}

	if (nt_response && nt_response->length) {
		if (nt_response->length > sizeof(request.data.auth_crap.nt_resp)) {
			request.flags = request.flags | WBFLAG_BIG_NTLMV2_BLOB;
			request.extra_len = nt_response->length;
			request.extra_data.data = SMB_MALLOC_ARRAY(char, request.extra_len);
			if (request.extra_data.data == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
			memcpy(request.extra_data.data, nt_response->data,
			       nt_response->length);

		} else {
			memcpy(request.data.auth_crap.nt_resp,
			       nt_response->data, nt_response->length);
		}
                request.data.auth_crap.nt_resp_len = nt_response->length;
	}

	result = winbindd_request_response(WINBINDD_PAM_AUTH_CRAP, &request, &response);
	SAFE_FREE(request.extra_data.data);

	/* Display response */

	if ((result != NSS_STATUS_SUCCESS) && (response.data.auth.nt_status == 0)) {
		nt_status = NT_STATUS_UNSUCCESSFUL;
		if (error_string)
			*error_string = smb_xstrdup("Reading winbind reply failed!");
		winbindd_free_response(&response);
		return nt_status;
	}

	nt_status = (NT_STATUS(response.data.auth.nt_status));
	if (!NT_STATUS_IS_OK(nt_status)) {
		if (error_string) 
			*error_string = smb_xstrdup(response.data.auth.error_string);
		winbindd_free_response(&response);
		return nt_status;
	}

	if ((flags & WBFLAG_PAM_LMKEY) && lm_key) {
		memcpy(lm_key, response.data.auth.first_8_lm_hash, 
		       sizeof(response.data.auth.first_8_lm_hash));
	}
	if ((flags & WBFLAG_PAM_USER_SESSION_KEY) && user_session_key) {
		memcpy(user_session_key, response.data.auth.user_session_key, 
			sizeof(response.data.auth.user_session_key));
	}

	if (flags & WBFLAG_PAM_UNIX_NAME) {
		*unix_name = SMB_STRDUP(response.data.auth.unix_username);
		if (!*unix_name) {
			winbindd_free_response(&response);
			return NT_STATUS_NO_MEMORY;
		}
	}

	winbindd_free_response(&response);
	return nt_status;
}

/* contact server to change user password using auth crap */
static NTSTATUS contact_winbind_change_pswd_auth_crap(const char *username,
						      const char *domain,
						      const DATA_BLOB new_nt_pswd,
						      const DATA_BLOB old_nt_hash_enc,
						      const DATA_BLOB new_lm_pswd,
						      const DATA_BLOB old_lm_hash_enc,
						      char  **error_string)
{
	NTSTATUS nt_status;
	NSS_STATUS result;
	struct winbindd_request request;
	struct winbindd_response response;

	if (!get_require_membership_sid())
	{
		if(error_string)
			*error_string = smb_xstrdup("Can't get membership sid.");
		return NT_STATUS_INVALID_PARAMETER;
	}

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if(username != NULL)
		fstrcpy(request.data.chng_pswd_auth_crap.user, username);
	if(domain != NULL)
		fstrcpy(request.data.chng_pswd_auth_crap.domain,domain);

	if(new_nt_pswd.length)
	{
		memcpy(request.data.chng_pswd_auth_crap.new_nt_pswd, new_nt_pswd.data, sizeof(request.data.chng_pswd_auth_crap.new_nt_pswd));
		request.data.chng_pswd_auth_crap.new_nt_pswd_len = new_nt_pswd.length;
	}

	if(old_nt_hash_enc.length)
	{
		memcpy(request.data.chng_pswd_auth_crap.old_nt_hash_enc, old_nt_hash_enc.data, sizeof(request.data.chng_pswd_auth_crap.old_nt_hash_enc));
		request.data.chng_pswd_auth_crap.old_nt_hash_enc_len = old_nt_hash_enc.length;
	}

	if(new_lm_pswd.length)
	{
		memcpy(request.data.chng_pswd_auth_crap.new_lm_pswd, new_lm_pswd.data, sizeof(request.data.chng_pswd_auth_crap.new_lm_pswd));
		request.data.chng_pswd_auth_crap.new_lm_pswd_len = new_lm_pswd.length;
	}

	if(old_lm_hash_enc.length)
	{
		memcpy(request.data.chng_pswd_auth_crap.old_lm_hash_enc, old_lm_hash_enc.data, sizeof(request.data.chng_pswd_auth_crap.old_lm_hash_enc));
		request.data.chng_pswd_auth_crap.old_lm_hash_enc_len = old_lm_hash_enc.length;
	}

	result = winbindd_request_response(WINBINDD_PAM_CHNG_PSWD_AUTH_CRAP, &request, &response);

	/* Display response */

	if ((result != NSS_STATUS_SUCCESS) && (response.data.auth.nt_status == 0))
	{
		nt_status = NT_STATUS_UNSUCCESSFUL;
		if (error_string)
			*error_string = smb_xstrdup("Reading winbind reply failed!");
		winbindd_free_response(&response);
		return nt_status;
	}

	nt_status = (NT_STATUS(response.data.auth.nt_status));
	if (!NT_STATUS_IS_OK(nt_status))
	{
		if (error_string) 
			*error_string = smb_xstrdup(response.data.auth.error_string);
		winbindd_free_response(&response);
		return nt_status;
	}

	winbindd_free_response(&response);

    return nt_status;
}

static NTSTATUS winbind_pw_check(struct ntlmssp_state *ntlmssp_state, TALLOC_CTX *mem_ctx,
				 DATA_BLOB *user_session_key, DATA_BLOB *lm_session_key)
{
	static const char zeros[16] = { 0, };
	NTSTATUS nt_status;
	char *error_string = NULL;
	uint8 lm_key[8]; 
	uint8 user_sess_key[16]; 
	char *unix_name = NULL;

	nt_status = contact_winbind_auth_crap(ntlmssp_state->user, ntlmssp_state->domain,
					      ntlmssp_state->client.netbios_name,
					      &ntlmssp_state->chal,
					      &ntlmssp_state->lm_resp,
					      &ntlmssp_state->nt_resp, 
					      WBFLAG_PAM_LMKEY | WBFLAG_PAM_USER_SESSION_KEY | WBFLAG_PAM_UNIX_NAME,
					      lm_key, user_sess_key, 
					      &error_string, &unix_name);

	if (NT_STATUS_IS_OK(nt_status)) {
		if (memcmp(lm_key, zeros, 8) != 0) {
			*lm_session_key = data_blob_talloc(mem_ctx, NULL, 16);
			memcpy(lm_session_key->data, lm_key, 8);
			memset(lm_session_key->data+8, '\0', 8);
		}

		if (memcmp(user_sess_key, zeros, 16) != 0) {
			*user_session_key = data_blob_talloc(mem_ctx, user_sess_key, 16);
		}
		ntlmssp_state->callback_private = talloc_strdup(ntlmssp_state,
								unix_name);
	} else {
		DEBUG(NT_STATUS_EQUAL(nt_status, NT_STATUS_ACCESS_DENIED) ? 0 : 3, 
		      ("Login for user [%s]\\[%s]@[%s] failed due to [%s]\n", 
		       ntlmssp_state->domain, ntlmssp_state->user, 
		       ntlmssp_state->client.netbios_name,
		       error_string ? error_string : "unknown error (NULL)"));
		ntlmssp_state->callback_private = NULL;
	}

	SAFE_FREE(error_string);
	SAFE_FREE(unix_name);
	return nt_status;
}

static NTSTATUS local_pw_check(struct ntlmssp_state *ntlmssp_state, TALLOC_CTX *mem_ctx,
			       DATA_BLOB *user_session_key, DATA_BLOB *lm_session_key)
{
	NTSTATUS nt_status;
	struct samr_Password lm_pw, nt_pw;

	nt_lm_owf_gen (opt_password, nt_pw.hash, lm_pw.hash);

	nt_status = ntlm_password_check(mem_ctx,
					true, true, 0,
					&ntlmssp_state->chal,
					&ntlmssp_state->lm_resp,
					&ntlmssp_state->nt_resp, 
					ntlmssp_state->user, 
					ntlmssp_state->user, 
					ntlmssp_state->domain,
					&lm_pw, &nt_pw, user_session_key, lm_session_key);

	if (NT_STATUS_IS_OK(nt_status)) {
		ntlmssp_state->callback_private = talloc_asprintf(ntlmssp_state,
							      "%s%c%s", ntlmssp_state->domain, 
							      *lp_winbind_separator(), 
							      ntlmssp_state->user);
	} else {
		DEBUG(3, ("Login for user [%s]\\[%s]@[%s] failed due to [%s]\n", 
			  ntlmssp_state->domain, ntlmssp_state->user,
			  ntlmssp_state->client.netbios_name,
			  nt_errstr(nt_status)));
		ntlmssp_state->callback_private = NULL;
	}
	return nt_status;
}

static NTSTATUS ntlm_auth_start_ntlmssp_client(struct ntlmssp_state **client_ntlmssp_state)
{
	NTSTATUS status;
	if ( (opt_username == NULL) || (opt_domain == NULL) ) {
		status = NT_STATUS_UNSUCCESSFUL;
		DEBUG(1, ("Need username and domain for NTLMSSP\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = ntlmssp_client_start(NULL,
				      global_myname(),
				      lp_workgroup(),
				      lp_client_ntlmv2_auth(),
				      client_ntlmssp_state);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not start NTLMSSP client: %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(*client_ntlmssp_state);
		return status;
	}

	status = ntlmssp_set_username(*client_ntlmssp_state, opt_username);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not set username: %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(*client_ntlmssp_state);
		return status;
	}

	status = ntlmssp_set_domain(*client_ntlmssp_state, opt_domain);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not set domain: %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(*client_ntlmssp_state);
		return status;
	}

	if (opt_password) {
		status = ntlmssp_set_password(*client_ntlmssp_state, opt_password);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Could not set password: %s\n",
				  nt_errstr(status)));
			TALLOC_FREE(*client_ntlmssp_state);
			return status;
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS ntlm_auth_start_ntlmssp_server(struct ntlmssp_state **ntlmssp_state)
{
	NTSTATUS status;
	const char *netbios_name;
	const char *netbios_domain;
	const char *dns_name;
	char *dns_domain;
	bool is_standalone = false;

	if (opt_password) {
		netbios_name = global_myname();
		netbios_domain = lp_workgroup();
	} else {
		netbios_name = get_winbind_netbios_name();
		netbios_domain = get_winbind_domain();
	}
	/* This should be a 'netbios domain -> DNS domain' mapping */
	dns_domain = get_mydnsdomname(talloc_tos());
	if (dns_domain) {
		strlower_m(dns_domain);
	}
	dns_name = get_mydnsfullname();

	status = ntlmssp_server_start(NULL,
				      is_standalone,
				      netbios_name,
				      netbios_domain,
				      dns_name,
				      dns_domain,
				      ntlmssp_state);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Could not start NTLMSSP server: %s\n",
			  nt_errstr(status)));
		return status;
	}

	/* Have we been given a local password, or should we ask winbind? */
	if (opt_password) {
		(*ntlmssp_state)->check_password = local_pw_check;
	} else {
		(*ntlmssp_state)->check_password = winbind_pw_check;
	}
	return NT_STATUS_OK;
}

/*******************************************************************
 Used by firefox to drive NTLM auth to IIS servers.
*******************************************************************/

static NTSTATUS do_ccache_ntlm_auth(DATA_BLOB initial_msg, DATA_BLOB challenge_msg,
				DATA_BLOB *reply)
{
	struct winbindd_request wb_request;
	struct winbindd_response wb_response;
	int ctrl = 0;
	NSS_STATUS result;

	/* get winbindd to do the ntlmssp step on our behalf */
	ZERO_STRUCT(wb_request);
	ZERO_STRUCT(wb_response);

	/*
	 * This is tricky here. If we set krb5_auth in pam_winbind.conf
	 * creds for users in trusted domain will be stored the winbindd
	 * child of the trusted domain. If we ask the primary domain for
	 * ntlm_ccache_auth, it will fail. So, we have to ask the trusted
	 * domain's child for ccache_ntlm_auth. that is to say, we have to 
	 * set WBFLAG_PAM_CONTACT_TRUSTDOM in request.flags.
	 */
	ctrl = get_pam_winbind_config();

	if (ctrl & WINBIND_KRB5_AUTH) {
		wb_request.flags |= WBFLAG_PAM_CONTACT_TRUSTDOM;
	}

	fstr_sprintf(wb_request.data.ccache_ntlm_auth.user,
		"%s%c%s", opt_domain, winbind_separator(), opt_username);
	wb_request.data.ccache_ntlm_auth.uid = geteuid();
	wb_request.data.ccache_ntlm_auth.initial_blob_len = initial_msg.length;
	wb_request.data.ccache_ntlm_auth.challenge_blob_len = challenge_msg.length;
	wb_request.extra_len = initial_msg.length + challenge_msg.length;

	if (wb_request.extra_len > 0) {
		wb_request.extra_data.data = SMB_MALLOC_ARRAY(char, wb_request.extra_len);
		if (wb_request.extra_data.data == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		memcpy(wb_request.extra_data.data, initial_msg.data, initial_msg.length);
		memcpy(wb_request.extra_data.data + initial_msg.length,
			challenge_msg.data, challenge_msg.length);
	}

	result = winbindd_request_response(WINBINDD_CCACHE_NTLMAUTH, &wb_request, &wb_response);
	SAFE_FREE(wb_request.extra_data.data);

	if (result != NSS_STATUS_SUCCESS) {
		winbindd_free_response(&wb_response);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (reply) {
		*reply = data_blob(wb_response.extra_data.data,
				wb_response.data.ccache_ntlm_auth.auth_blob_len);
		if (wb_response.data.ccache_ntlm_auth.auth_blob_len > 0 &&
				reply->data == NULL) {
			winbindd_free_response(&wb_response);
			return NT_STATUS_NO_MEMORY;
		}
	}

	winbindd_free_response(&wb_response);
	return NT_STATUS_MORE_PROCESSING_REQUIRED;
}

static void manage_squid_ntlmssp_request_int(struct ntlm_auth_state *state,
					     char *buf, int length,
					     TALLOC_CTX *mem_ctx,
					     char **response)
{
	DATA_BLOB request, reply;
	NTSTATUS nt_status;

	if (strlen(buf) < 2) {
		DEBUG(1, ("NTLMSSP query [%s] invalid\n", buf));
		*response = talloc_strdup(mem_ctx, "BH NTLMSSP query invalid");
		return;
	}

	if (strlen(buf) > 3) {
		if(strncmp(buf, "SF ", 3) == 0){
			DEBUG(10, ("Setting flags to negotioate\n"));
			TALLOC_FREE(state->want_feature_list);
			state->want_feature_list = talloc_strdup(state->mem_ctx,
					buf+3);
			*response = talloc_strdup(mem_ctx, "OK");
			return;
		}
		request = base64_decode_data_blob(buf + 3);
	} else {
		request = data_blob_null;
	}

	if ((strncmp(buf, "PW ", 3) == 0)) {
		/* The calling application wants us to use a local password
		 * (rather than winbindd) */

		opt_password = SMB_STRNDUP((const char *)request.data,
				request.length);

		if (opt_password == NULL) {
			DEBUG(1, ("Out of memory\n"));
			*response = talloc_strdup(mem_ctx, "BH Out of memory");
			data_blob_free(&request);
			return;
		}

		*response = talloc_strdup(mem_ctx, "OK");
		data_blob_free(&request);
		return;
	}

	if (strncmp(buf, "YR", 2) == 0) {
		TALLOC_FREE(state->ntlmssp_state);
		state->svr_state = SERVER_INITIAL;
	} else if (strncmp(buf, "KK", 2) == 0) {
		/* No special preprocessing required */
	} else if (strncmp(buf, "GF", 2) == 0) {
		DEBUG(10, ("Requested negotiated NTLMSSP flags\n"));

		if (state->svr_state == SERVER_FINISHED) {
			*response = talloc_asprintf(mem_ctx, "GF 0x%08x",
						 state->neg_flags);
		}
		else {
			*response = talloc_strdup(mem_ctx, "BH\n");
		}
		data_blob_free(&request);
		return;
	} else if (strncmp(buf, "GK", 2) == 0) {
		DEBUG(10, ("Requested NTLMSSP session key\n"));
		if(state->have_session_key) {
			char *key64 = base64_encode_data_blob(state->mem_ctx,
					state->session_key);
			*response = talloc_asprintf(mem_ctx, "GK %s",
						 key64 ? key64 : "<NULL>");
			TALLOC_FREE(key64);
		} else {
			*response = talloc_strdup(mem_ctx, "BH");
		}

		data_blob_free(&request);
		return;
	} else {
		DEBUG(1, ("NTLMSSP query [%s] invalid\n", buf));
		*response = talloc_strdup(mem_ctx, "BH NTLMSSP query invalid");
		return;
	}

	if (!state->ntlmssp_state) {
		nt_status = ntlm_auth_start_ntlmssp_server(
				&state->ntlmssp_state);
		if (!NT_STATUS_IS_OK(nt_status)) {
			*response = talloc_asprintf(
				mem_ctx, "BH %s", nt_errstr(nt_status));
			return;
		}
		ntlmssp_want_feature_list(state->ntlmssp_state,
				state->want_feature_list);
	}

	DEBUG(10, ("got NTLMSSP packet:\n"));
	dump_data(10, request.data, request.length);

	nt_status = ntlmssp_update(state->ntlmssp_state, request, &reply);

	if (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		char *reply_base64 = base64_encode_data_blob(state->mem_ctx,
				reply);
		*response = talloc_asprintf(mem_ctx, "TT %s", reply_base64);
		TALLOC_FREE(reply_base64);
		data_blob_free(&reply);
		state->svr_state = SERVER_CHALLENGE;
		DEBUG(10, ("NTLMSSP challenge\n"));
	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_ACCESS_DENIED)) {
		*response = talloc_asprintf(mem_ctx, "BH %s",
					 nt_errstr(nt_status));
		DEBUG(0, ("NTLMSSP BH: %s\n", nt_errstr(nt_status)));

		TALLOC_FREE(state->ntlmssp_state);
	} else if (!NT_STATUS_IS_OK(nt_status)) {
		*response = talloc_asprintf(mem_ctx, "NA %s",
					 nt_errstr(nt_status));
		DEBUG(10, ("NTLMSSP %s\n", nt_errstr(nt_status)));
	} else {
		*response = talloc_asprintf(
			mem_ctx, "AF %s",
			(char *)state->ntlmssp_state->callback_private);
		DEBUG(10, ("NTLMSSP OK!\n"));

		if(state->have_session_key)
			data_blob_free(&state->session_key);
		state->session_key = data_blob(
				state->ntlmssp_state->session_key.data,
				state->ntlmssp_state->session_key.length);
		state->neg_flags = state->ntlmssp_state->neg_flags;
		state->have_session_key = true;
		state->svr_state = SERVER_FINISHED;
	}

	data_blob_free(&request);
}

static void manage_squid_ntlmssp_request(struct ntlm_auth_state *state,
					 char *buf, int length)
{
	char *response;

	manage_squid_ntlmssp_request_int(state, buf, length,
					 talloc_tos(), &response);

	if (response == NULL) {
		x_fprintf(x_stdout, "BH Out of memory\n");
		return;
	}
	x_fprintf(x_stdout, "%s\n", response);
	TALLOC_FREE(response);
}

static void manage_client_ntlmssp_request(struct ntlm_auth_state *state,
					 	char *buf, int length)
{
	DATA_BLOB request, reply;
	NTSTATUS nt_status;

	if (!opt_username || !*opt_username) {
		x_fprintf(x_stderr, "username must be specified!\n\n");
		exit(1);
	}

	if (strlen(buf) < 2) {
		DEBUG(1, ("NTLMSSP query [%s] invalid\n", buf));
		x_fprintf(x_stdout, "BH NTLMSSP query invalid\n");
		return;
	}

	if (strlen(buf) > 3) {
		if(strncmp(buf, "SF ", 3) == 0) {
			DEBUG(10, ("Looking for flags to negotiate\n"));
			talloc_free(state->want_feature_list);
			state->want_feature_list = talloc_strdup(state->mem_ctx,
					buf+3);
			x_fprintf(x_stdout, "OK\n");
			return;
		}
		request = base64_decode_data_blob(buf + 3);
	} else {
		request = data_blob_null;
	}

	if (strncmp(buf, "PW ", 3) == 0) {
		/* We asked for a password and obviously got it :-) */

		opt_password = SMB_STRNDUP((const char *)request.data,
				request.length);

		if (opt_password == NULL) {
			DEBUG(1, ("Out of memory\n"));
			x_fprintf(x_stdout, "BH Out of memory\n");
			data_blob_free(&request);
			return;
		}

		x_fprintf(x_stdout, "OK\n");
		data_blob_free(&request);
		return;
	}

	if (!state->ntlmssp_state && use_cached_creds) {
		/* check whether cached credentials are usable. */
		DATA_BLOB empty_blob = data_blob_null;

		nt_status = do_ccache_ntlm_auth(empty_blob, empty_blob, NULL);
		if (!NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			/* failed to use cached creds */
			use_cached_creds = False;
		}
	}

	if (opt_password == NULL && !use_cached_creds) {
		/* Request a password from the calling process.  After
		   sending it, the calling process should retry asking for the
		   negotiate. */

		DEBUG(10, ("Requesting password\n"));
		x_fprintf(x_stdout, "PW\n");
		return;
	}

	if (strncmp(buf, "YR", 2) == 0) {
		TALLOC_FREE(state->ntlmssp_state);
		state->cli_state = CLIENT_INITIAL;
	} else if (strncmp(buf, "TT", 2) == 0) {
		/* No special preprocessing required */
	} else if (strncmp(buf, "GF", 2) == 0) {
		DEBUG(10, ("Requested negotiated NTLMSSP flags\n"));

		if(state->cli_state == CLIENT_FINISHED) {
			x_fprintf(x_stdout, "GF 0x%08x\n", state->neg_flags);
		}
		else {
			x_fprintf(x_stdout, "BH\n");
		}

		data_blob_free(&request);
		return;
	} else if (strncmp(buf, "GK", 2) == 0 ) {
		DEBUG(10, ("Requested session key\n"));

		if(state->cli_state == CLIENT_FINISHED) {
			char *key64 = base64_encode_data_blob(state->mem_ctx,
					state->session_key);
			x_fprintf(x_stdout, "GK %s\n", key64?key64:"<NULL>");
			TALLOC_FREE(key64);
		}
		else {
			x_fprintf(x_stdout, "BH\n");
		}

		data_blob_free(&request);
		return;
	} else {
		DEBUG(1, ("NTLMSSP query [%s] invalid\n", buf));
		x_fprintf(x_stdout, "BH NTLMSSP query invalid\n");
		return;
	}

	if (!state->ntlmssp_state) {
		nt_status = ntlm_auth_start_ntlmssp_client(
				&state->ntlmssp_state);
		if (!NT_STATUS_IS_OK(nt_status)) {
			x_fprintf(x_stdout, "BH %s\n", nt_errstr(nt_status));
			return;
		}
		ntlmssp_want_feature_list(state->ntlmssp_state,
				state->want_feature_list);
		state->initial_message = data_blob_null;
	}

	DEBUG(10, ("got NTLMSSP packet:\n"));
	dump_data(10, request.data, request.length);

	if (use_cached_creds && !opt_password &&
			(state->cli_state == CLIENT_RESPONSE)) {
		nt_status = do_ccache_ntlm_auth(state->initial_message, request,
				&reply);
	} else {
		nt_status = ntlmssp_update(state->ntlmssp_state, request,
				&reply);
	}

	if (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		char *reply_base64 = base64_encode_data_blob(state->mem_ctx,
				reply);
		if (state->cli_state == CLIENT_INITIAL) {
			x_fprintf(x_stdout, "YR %s\n", reply_base64);
			state->initial_message = reply;
			state->cli_state = CLIENT_RESPONSE;
		} else {
			x_fprintf(x_stdout, "KK %s\n", reply_base64);
			data_blob_free(&reply);
		}
		TALLOC_FREE(reply_base64);
		DEBUG(10, ("NTLMSSP challenge\n"));
	} else if (NT_STATUS_IS_OK(nt_status)) {
		char *reply_base64 = base64_encode_data_blob(talloc_tos(),
				reply);
		x_fprintf(x_stdout, "AF %s\n", reply_base64);
		TALLOC_FREE(reply_base64);

		if(state->have_session_key)
			data_blob_free(&state->session_key);

		state->session_key = data_blob(
				state->ntlmssp_state->session_key.data,
				state->ntlmssp_state->session_key.length);
		state->neg_flags = state->ntlmssp_state->neg_flags;
		state->have_session_key = true;

		DEBUG(10, ("NTLMSSP OK!\n"));
		state->cli_state = CLIENT_FINISHED;
		TALLOC_FREE(state->ntlmssp_state);
	} else {
		x_fprintf(x_stdout, "BH %s\n", nt_errstr(nt_status));
		DEBUG(0, ("NTLMSSP BH: %s\n", nt_errstr(nt_status)));
		state->cli_state = CLIENT_ERROR;
		TALLOC_FREE(state->ntlmssp_state);
	}

	data_blob_free(&request);
}

static void manage_squid_basic_request(struct ntlm_auth_state *state,
					char *buf, int length)
{
	char *user, *pass;	
	user=buf;

	pass=(char *)memchr(buf,' ',length);
	if (!pass) {
		DEBUG(2, ("Password not found. Denying access\n"));
		x_fprintf(x_stdout, "ERR\n");
		return;
	}
	*pass='\0';
	pass++;

	if (state->helper_mode == SQUID_2_5_BASIC) {
		rfc1738_unescape(user);
		rfc1738_unescape(pass);
	}

	if (check_plaintext_auth(user, pass, False)) {
		x_fprintf(x_stdout, "OK\n");
	} else {
		x_fprintf(x_stdout, "ERR\n");
	}
}

static void offer_gss_spnego_mechs(void) {

	DATA_BLOB token;
	struct spnego_data spnego;
	ssize_t len;
	char *reply_base64;
	TALLOC_CTX *ctx = talloc_tos();
	char *principal;
	char *myname_lower;

	ZERO_STRUCT(spnego);

	myname_lower = talloc_strdup(ctx, global_myname());
	if (!myname_lower) {
		return;
	}
	strlower_m(myname_lower);

	principal = talloc_asprintf(ctx, "%s$@%s", myname_lower, lp_realm());
	if (!principal) {
		return;
	}

	/* Server negTokenInit (mech offerings) */
	spnego.type = SPNEGO_NEG_TOKEN_INIT;
	spnego.negTokenInit.mechTypes = talloc_array(ctx, const char *, 4);
#ifdef HAVE_KRB5
	spnego.negTokenInit.mechTypes[0] = talloc_strdup(ctx, OID_KERBEROS5_OLD);
	spnego.negTokenInit.mechTypes[1] = talloc_strdup(ctx, OID_KERBEROS5);
	spnego.negTokenInit.mechTypes[2] = talloc_strdup(ctx, OID_NTLMSSP);
	spnego.negTokenInit.mechTypes[3] = NULL;
#else
	spnego.negTokenInit.mechTypes[0] = talloc_strdup(ctx, OID_NTLMSSP);
	spnego.negTokenInit.mechTypes[1] = NULL;
#endif


	spnego.negTokenInit.mechListMIC = data_blob_talloc(ctx, principal,
						    strlen(principal));

	len = spnego_write_data(ctx, &token, &spnego);
	spnego_free_data(&spnego);

	if (len == -1) {
		DEBUG(1, ("Could not write SPNEGO data blob\n"));
		x_fprintf(x_stdout, "BH Could not write SPNEGO data blob\n");
		return;
	}

	reply_base64 = base64_encode_data_blob(talloc_tos(), token);
	x_fprintf(x_stdout, "TT %s *\n", reply_base64);

	TALLOC_FREE(reply_base64);
	data_blob_free(&token);
	DEBUG(10, ("sent SPNEGO negTokenInit\n"));
	return;
}

bool spnego_parse_krb5_wrap(TALLOC_CTX *ctx, DATA_BLOB blob, DATA_BLOB *ticket, uint8 tok_id[2])
{
	bool ret;
	ASN1_DATA *data;
	int data_remaining;

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return false;
	}

	asn1_load(data, blob);
	asn1_start_tag(data, ASN1_APPLICATION(0));
	asn1_check_OID(data, OID_KERBEROS5);

	data_remaining = asn1_tag_remaining(data);

	if (data_remaining < 3) {
		data->has_error = True;
	} else {
		asn1_read(data, tok_id, 2);
		data_remaining -= 2;
		*ticket = data_blob_talloc(ctx, NULL, data_remaining);
		asn1_read(data, ticket->data, ticket->length);
	}

	asn1_end_tag(data);

	ret = !data->has_error;

	if (data->has_error) {
		data_blob_free(ticket);
	}

	asn1_free(data);

	return ret;
}

static void manage_gss_spnego_request(struct ntlm_auth_state *state,
					char *buf, int length)
{
	struct spnego_data request, response;
	DATA_BLOB token;
	DATA_BLOB raw_in_token = data_blob_null;
	DATA_BLOB raw_out_token = data_blob_null;
	NTSTATUS status;
	ssize_t len;
	TALLOC_CTX *ctx = talloc_tos();

	char *user = NULL;
	char *domain = NULL;

	const char *reply_code;
	char       *reply_base64;
	char *reply_argument = NULL;
	char *supportedMech = NULL;

	if (strlen(buf) < 2) {
		DEBUG(1, ("SPENGO query [%s] invalid\n", buf));
		x_fprintf(x_stdout, "BH SPENGO query invalid\n");
		return;
	}

	if (strncmp(buf, "YR", 2) == 0) {
		TALLOC_FREE(state->ntlmssp_state);
		TALLOC_FREE(state->spnego_mech);
		TALLOC_FREE(state->spnego_mech_oid);
	} else if (strncmp(buf, "KK", 2) == 0) {
		;
	} else {
		DEBUG(1, ("SPENGO query [%s] invalid\n", buf));
		x_fprintf(x_stdout, "BH SPENGO query invalid\n");
		return;
	}

	if ( (strlen(buf) == 2)) {

		/* no client data, get the negTokenInit offering
                   mechanisms */

		offer_gss_spnego_mechs();
		return;
	}

	/* All subsequent requests have a blob. This might be negTokenInit or negTokenTarg */

	if (strlen(buf) <= 3) {
		DEBUG(1, ("GSS-SPNEGO query [%s] invalid\n", buf));
		x_fprintf(x_stdout, "BH GSS-SPNEGO query invalid\n");
		return;
	}

	token = base64_decode_data_blob(buf + 3);

	if ((token.length >= 7)
	    && (strncmp((char *)token.data, "NTLMSSP", 7) == 0)) {
		char *reply;

		data_blob_free(&token);

		DEBUG(10, ("Could not parse GSS-SPNEGO, trying raw "
			   "ntlmssp\n"));

		manage_squid_ntlmssp_request_int(state, buf, length,
						 talloc_tos(), &reply);
		if (reply == NULL) {
			x_fprintf(x_stdout, "BH Out of memory\n");
			return;
		}

		if (strncmp(reply, "AF ", 3) == 0) {
			x_fprintf(x_stdout, "AF * %s\n", reply+3);
		} else {
			x_fprintf(x_stdout, "%s *\n", reply);
		}

		TALLOC_FREE(reply);
		return;
	}

	ZERO_STRUCT(request);
	len = spnego_read_data(ctx, token, &request);
	data_blob_free(&token);

	if (len == -1) {
		DEBUG(1, ("GSS-SPNEGO query [%s] invalid\n", buf));
		x_fprintf(x_stdout, "BH GSS-SPNEGO query invalid\n");
		return;
	}

	if (request.type == SPNEGO_NEG_TOKEN_INIT) {
#ifdef HAVE_KRB5
		int krb5_idx = -1;
#endif
		int ntlm_idx = -1;
		int used_idx = -1;
		int i;

		if (state->spnego_mech) {
			DEBUG(1, ("Client restarted SPNEGO with NegTokenInit "
				  "while mech[%s] was already negotiated\n",
				  state->spnego_mech));
			x_fprintf(x_stdout, "BH Client send NegTokenInit twice\n");
			return;
		}

		/* Second request from Client. This is where the
		   client offers its mechanism to use. */

		if ( (request.negTokenInit.mechTypes == NULL) ||
		     (request.negTokenInit.mechTypes[0] == NULL) ) {
			DEBUG(1, ("Client did not offer any mechanism\n"));
			x_fprintf(x_stdout, "BH Client did not offer any "
					    "mechanism\n");
			return;
		}

		status = NT_STATUS_UNSUCCESSFUL;
		for (i = 0; request.negTokenInit.mechTypes[i] != NULL; i++) {
			DEBUG(10,("got mech[%d][%s]\n",
				i, request.negTokenInit.mechTypes[i]));
#ifdef HAVE_KRB5
			if (strcmp(request.negTokenInit.mechTypes[i], OID_KERBEROS5_OLD) == 0) {
				krb5_idx = i;
				break;
			}
			if (strcmp(request.negTokenInit.mechTypes[i], OID_KERBEROS5) == 0) {
				krb5_idx = i;
				break;
			}
#endif
			if (strcmp(request.negTokenInit.mechTypes[i], OID_NTLMSSP) == 0) {
				ntlm_idx = i;
				break;
			}
		}

		used_idx = ntlm_idx;
#ifdef HAVE_KRB5
		if (krb5_idx != -1) {
			ntlm_idx = -1;
			used_idx = krb5_idx;
		}
#endif
		if (ntlm_idx > -1) {
			state->spnego_mech = talloc_strdup(state, "ntlmssp");
			if (state->spnego_mech == NULL) {
				x_fprintf(x_stdout, "BH Out of memory\n");
				return;
			}

			if (state->ntlmssp_state) {
				DEBUG(1, ("Client wants a new NTLMSSP challenge, but "
					  "already got one\n"));
				x_fprintf(x_stdout, "BH Client wants a new "
						    "NTLMSSP challenge, but "
						    "already got one\n");
				TALLOC_FREE(state->ntlmssp_state);
				return;
			}

			status = ntlm_auth_start_ntlmssp_server(&state->ntlmssp_state);
			if (!NT_STATUS_IS_OK(status)) {
				x_fprintf(x_stdout, "BH %s\n", nt_errstr(status));
				return;
			}
		}

#ifdef HAVE_KRB5
		if (krb5_idx > -1) {
			state->spnego_mech = talloc_strdup(state, "krb5");
			if (state->spnego_mech == NULL) {
				x_fprintf(x_stdout, "BH Out of memory\n");
				return;
			}
		}
#endif
		if (used_idx > -1) {
			state->spnego_mech_oid = talloc_strdup(state,
				request.negTokenInit.mechTypes[used_idx]);
			if (state->spnego_mech_oid == NULL) {
				x_fprintf(x_stdout, "BH Out of memory\n");
				return;
			}
			supportedMech = talloc_strdup(ctx, state->spnego_mech_oid);
			if (supportedMech == NULL) {
				x_fprintf(x_stdout, "BH Out of memory\n");
				return;
			}

			status = NT_STATUS_MORE_PROCESSING_REQUIRED;
		} else {
			status = NT_STATUS_NOT_SUPPORTED;
		}
		if (used_idx == 0) {
			status = NT_STATUS_OK;
			raw_in_token = request.negTokenInit.mechToken;
		}
	} else {
		if (state->spnego_mech == NULL) {
			DEBUG(1,("Got netTokenTarg without negTokenInit\n"));
			x_fprintf(x_stdout, "BH Got a negTokenTarg without "
					    "negTokenInit\n");
			return;
		}

		if ((request.negTokenTarg.supportedMech != NULL) &&
		     (strcmp(request.negTokenTarg.supportedMech, state->spnego_mech_oid) != 0 ) ) {
			DEBUG(1, ("Got a negTokenTarg with mech[%s] while [%s] was already negotiated\n",
				  request.negTokenTarg.supportedMech,
				  state->spnego_mech_oid));
			x_fprintf(x_stdout, "BH Got a negTokenTarg with speficied mech\n");
			return;
		}

		status = NT_STATUS_OK;
		raw_in_token = request.negTokenTarg.responseToken;
	}

	if (!NT_STATUS_IS_OK(status)) {
		/* error or more processing */
	} else if (strcmp(state->spnego_mech, "ntlmssp") == 0) {

		DEBUG(10, ("got NTLMSSP packet:\n"));
		dump_data(10, raw_in_token.data, raw_in_token.length);

		status = ntlmssp_update(state->ntlmssp_state,
					raw_in_token,
					&raw_out_token);
		if (NT_STATUS_IS_OK(status)) {
			user = talloc_strdup(ctx, state->ntlmssp_state->user);
			domain = talloc_strdup(ctx, state->ntlmssp_state->domain);
		}
		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			TALLOC_FREE(state->ntlmssp_state);
		}
#ifdef HAVE_KRB5
	} else if (strcmp(state->spnego_mech, "krb5") == 0) {
		char *principal;
		DATA_BLOB ap_rep;
		DATA_BLOB session_key;
		struct PAC_LOGON_INFO *logon_info = NULL;
		DATA_BLOB ticket;
		uint8_t tok_id[2];

		if (!spnego_parse_krb5_wrap(ctx, raw_in_token,
					    &ticket, tok_id)) {
			DEBUG(1, ("spnego_parse_krb5_wrap failed\n"));
			x_fprintf(x_stdout, "BH spnego_parse_krb5_wrap failed\n");
			return;
		}

		status = ads_verify_ticket(ctx, lp_realm(), 0,
					   &ticket,
					   &principal, &logon_info, &ap_rep,
					   &session_key, True);

		/* Now in "principal" we have the name we are authenticated as. */

		if (NT_STATUS_IS_OK(status)) {

			domain = strchr_m(principal, '@');

			if (domain == NULL) {
				DEBUG(1, ("Did not get a valid principal "
					  "from ads_verify_ticket\n"));
				x_fprintf(x_stdout, "BH Did not get a "
					  "valid principal from "
					  "ads_verify_ticket\n");
				return;
			}

			*domain++ = '\0';
			domain = talloc_strdup(ctx, domain);
			user = talloc_strdup(ctx, principal);

			if (logon_info) {
				netsamlogon_cache_store(
					user, &logon_info->info3);
			}

			data_blob_free(&ap_rep);
			data_blob_free(&session_key);
		}
		data_blob_free(&ticket);
#endif
	}

	spnego_free_data(&request);
	ZERO_STRUCT(response);
	response.type = SPNEGO_NEG_TOKEN_TARG;

	if (NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(state->spnego_mech);
		TALLOC_FREE(state->spnego_mech_oid);
		response.negTokenTarg.negResult = SPNEGO_ACCEPT_COMPLETED;
		response.negTokenTarg.responseToken = raw_out_token;
		reply_code = "AF";
		reply_argument = talloc_asprintf(ctx, "%s\\%s", domain, user);
	} else if (NT_STATUS_EQUAL(status,
				   NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		response.negTokenTarg.supportedMech = supportedMech;
		response.negTokenTarg.responseToken = raw_out_token;
		response.negTokenTarg.negResult = SPNEGO_ACCEPT_INCOMPLETE;
		reply_code = "TT";
		reply_argument = talloc_strdup(ctx, "*");
	} else {
		TALLOC_FREE(state->spnego_mech);
		TALLOC_FREE(state->spnego_mech_oid);
		data_blob_free(&raw_out_token);
		response.negTokenTarg.negResult = SPNEGO_REJECT;
		reply_code = "NA";
		reply_argument = talloc_strdup(ctx, nt_errstr(status));
	}

	if (!reply_argument) {
		DEBUG(1, ("Could not write SPNEGO data blob\n"));
		x_fprintf(x_stdout, "BH Could not write SPNEGO data blob\n");
		spnego_free_data(&response);
		return;
	}

	len = spnego_write_data(ctx, &token, &response);
	spnego_free_data(&response);

	if (len == -1) {
		DEBUG(1, ("Could not write SPNEGO data blob\n"));
		x_fprintf(x_stdout, "BH Could not write SPNEGO data blob\n");
		return;
	}

	reply_base64 = base64_encode_data_blob(talloc_tos(), token);

	x_fprintf(x_stdout, "%s %s %s\n",
		  reply_code, reply_base64, reply_argument);

	TALLOC_FREE(reply_base64);
	data_blob_free(&token);

	return;
}

static struct ntlmssp_state *client_ntlmssp_state = NULL;

static bool manage_client_ntlmssp_init(struct spnego_data spnego)
{
	NTSTATUS status;
	DATA_BLOB null_blob = data_blob_null;
	DATA_BLOB to_server;
	char *to_server_base64;
	const char *my_mechs[] = {OID_NTLMSSP, NULL};
	TALLOC_CTX *ctx = talloc_tos();

	DEBUG(10, ("Got spnego negTokenInit with NTLMSSP\n"));

	if (client_ntlmssp_state != NULL) {
		DEBUG(1, ("Request for initial SPNEGO request where "
			  "we already have a state\n"));
		return False;
	}

	if (!client_ntlmssp_state) {
		if (!NT_STATUS_IS_OK(status = ntlm_auth_start_ntlmssp_client(&client_ntlmssp_state))) {
			x_fprintf(x_stdout, "BH %s\n", nt_errstr(status));
			return False;
		}
	}


	if (opt_password == NULL) {

		/* Request a password from the calling process.  After
		   sending it, the calling process should retry with
		   the negTokenInit. */

		DEBUG(10, ("Requesting password\n"));
		x_fprintf(x_stdout, "PW\n");
		return True;
	}

	spnego.type = SPNEGO_NEG_TOKEN_INIT;
	spnego.negTokenInit.mechTypes = my_mechs;
	spnego.negTokenInit.reqFlags = data_blob_null;
	spnego.negTokenInit.reqFlagsPadding = 0;
	spnego.negTokenInit.mechListMIC = null_blob;

	status = ntlmssp_update(client_ntlmssp_state, null_blob,
				       &spnego.negTokenInit.mechToken);

	if ( !(NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED) ||
			NT_STATUS_IS_OK(status)) ) {
		DEBUG(1, ("Expected OK or MORE_PROCESSING_REQUIRED, got: %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(client_ntlmssp_state);
		return False;
	}

	spnego_write_data(ctx, &to_server, &spnego);
	data_blob_free(&spnego.negTokenInit.mechToken);

	to_server_base64 = base64_encode_data_blob(talloc_tos(), to_server);
	data_blob_free(&to_server);
	x_fprintf(x_stdout, "KK %s\n", to_server_base64);
	TALLOC_FREE(to_server_base64);
	return True;
}

static void manage_client_ntlmssp_targ(struct spnego_data spnego)
{
	NTSTATUS status;
	DATA_BLOB null_blob = data_blob_null;
	DATA_BLOB request;
	DATA_BLOB to_server;
	char *to_server_base64;
	TALLOC_CTX *ctx = talloc_tos();

	DEBUG(10, ("Got spnego negTokenTarg with NTLMSSP\n"));

	if (client_ntlmssp_state == NULL) {
		DEBUG(1, ("Got NTLMSSP tArg without a client state\n"));
		x_fprintf(x_stdout, "BH Got NTLMSSP tArg without a client state\n");
		return;
	}

	if (spnego.negTokenTarg.negResult == SPNEGO_REJECT) {
		x_fprintf(x_stdout, "NA\n");
		TALLOC_FREE(client_ntlmssp_state);
		return;
	}

	if (spnego.negTokenTarg.negResult == SPNEGO_ACCEPT_COMPLETED) {
		x_fprintf(x_stdout, "AF\n");
		TALLOC_FREE(client_ntlmssp_state);
		return;
	}

	status = ntlmssp_update(client_ntlmssp_state,
				       spnego.negTokenTarg.responseToken,
				       &request);

	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		DEBUG(1, ("Expected MORE_PROCESSING_REQUIRED from "
			  "ntlmssp_client_update, got: %s\n",
			  nt_errstr(status)));
		x_fprintf(x_stdout, "BH Expected MORE_PROCESSING_REQUIRED from "
				    "ntlmssp_client_update\n");
		data_blob_free(&request);
		TALLOC_FREE(client_ntlmssp_state);
		return;
	}

	spnego.type = SPNEGO_NEG_TOKEN_TARG;
	spnego.negTokenTarg.negResult = SPNEGO_ACCEPT_INCOMPLETE;
	spnego.negTokenTarg.supportedMech = (char *)OID_NTLMSSP;
	spnego.negTokenTarg.responseToken = request;
	spnego.negTokenTarg.mechListMIC = null_blob;

	spnego_write_data(ctx, &to_server, &spnego);
	data_blob_free(&request);

	to_server_base64 = base64_encode_data_blob(talloc_tos(), to_server);
	data_blob_free(&to_server);
	x_fprintf(x_stdout, "KK %s\n", to_server_base64);
	TALLOC_FREE(to_server_base64);
	return;
}

#ifdef HAVE_KRB5

static bool manage_client_krb5_init(struct spnego_data spnego)
{
	char *principal;
	DATA_BLOB tkt, to_server;
	DATA_BLOB session_key_krb5 = data_blob_null;
	struct spnego_data reply;
	char *reply_base64;
	int retval;

	const char *my_mechs[] = {OID_KERBEROS5_OLD, NULL};
	ssize_t len;
	TALLOC_CTX *ctx = talloc_tos();

	if ( (spnego.negTokenInit.mechListMIC.data == NULL) ||
	     (spnego.negTokenInit.mechListMIC.length == 0) ) {
		DEBUG(1, ("Did not get a principal for krb5\n"));
		return False;
	}

	principal = (char *)SMB_MALLOC(
		spnego.negTokenInit.mechListMIC.length+1);

	if (principal == NULL) {
		DEBUG(1, ("Could not malloc principal\n"));
		return False;
	}

	memcpy(principal, spnego.negTokenInit.mechListMIC.data,
	       spnego.negTokenInit.mechListMIC.length);
	principal[spnego.negTokenInit.mechListMIC.length] = '\0';

	retval = cli_krb5_get_ticket(ctx, principal, 0,
					  &tkt, &session_key_krb5,
					  0, NULL, NULL, NULL);
	if (retval) {
		char *user = NULL;

		/* Let's try to first get the TGT, for that we need a
                   password. */

		if (opt_password == NULL) {
			DEBUG(10, ("Requesting password\n"));
			x_fprintf(x_stdout, "PW\n");
			return True;
		}

		user = talloc_asprintf(talloc_tos(), "%s@%s", opt_username, opt_domain);
		if (!user) {
			return false;
		}

		if ((retval = kerberos_kinit_password(user, opt_password, 0, NULL))) {
			DEBUG(10, ("Requesting TGT failed: %s\n", error_message(retval)));
			return False;
		}

		retval = cli_krb5_get_ticket(ctx, principal, 0,
						  &tkt, &session_key_krb5,
						  0, NULL, NULL, NULL);
		if (retval) {
			DEBUG(10, ("Kinit suceeded, but getting a ticket failed: %s\n", error_message(retval)));
			return False;
		}
	}

	data_blob_free(&session_key_krb5);

	ZERO_STRUCT(reply);

	reply.type = SPNEGO_NEG_TOKEN_INIT;
	reply.negTokenInit.mechTypes = my_mechs;
	reply.negTokenInit.reqFlags = data_blob_null;
	reply.negTokenInit.reqFlagsPadding = 0;
	reply.negTokenInit.mechToken = tkt;
	reply.negTokenInit.mechListMIC = data_blob_null;

	len = spnego_write_data(ctx, &to_server, &reply);
	data_blob_free(&tkt);

	if (len == -1) {
		DEBUG(1, ("Could not write SPNEGO data blob\n"));
		return False;
	}

	reply_base64 = base64_encode_data_blob(talloc_tos(), to_server);
	x_fprintf(x_stdout, "KK %s *\n", reply_base64);

	TALLOC_FREE(reply_base64);
	data_blob_free(&to_server);
	DEBUG(10, ("sent GSS-SPNEGO KERBEROS5 negTokenInit\n"));
	return True;
}

static void manage_client_krb5_targ(struct spnego_data spnego)
{
	switch (spnego.negTokenTarg.negResult) {
	case SPNEGO_ACCEPT_INCOMPLETE:
		DEBUG(1, ("Got a Kerberos negTokenTarg with ACCEPT_INCOMPLETE\n"));
		x_fprintf(x_stdout, "BH Got a Kerberos negTokenTarg with "
				    "ACCEPT_INCOMPLETE\n");
		break;
	case SPNEGO_ACCEPT_COMPLETED:
		DEBUG(10, ("Accept completed\n"));
		x_fprintf(x_stdout, "AF\n");
		break;
	case SPNEGO_REJECT:
		DEBUG(10, ("Rejected\n"));
		x_fprintf(x_stdout, "NA\n");
		break;
	default:
		DEBUG(1, ("Got an invalid negTokenTarg\n"));
		x_fprintf(x_stdout, "AF\n");
	}
}

#endif

static void manage_gss_spnego_client_request(struct ntlm_auth_state *state,
						char *buf, int length)
{
	DATA_BLOB request;
	struct spnego_data spnego;
	ssize_t len;
	TALLOC_CTX *ctx = talloc_tos();

	if (!opt_username || !*opt_username) {
		x_fprintf(x_stderr, "username must be specified!\n\n");
		exit(1);
	}

	if (strlen(buf) <= 3) {
		DEBUG(1, ("SPNEGO query [%s] too short\n", buf));
		x_fprintf(x_stdout, "BH SPNEGO query too short\n");
		return;
	}

	request = base64_decode_data_blob(buf+3);

	if (strncmp(buf, "PW ", 3) == 0) {

		/* We asked for a password and obviously got it :-) */

		opt_password = SMB_STRNDUP((const char *)request.data, request.length);

		if (opt_password == NULL) {
			DEBUG(1, ("Out of memory\n"));
			x_fprintf(x_stdout, "BH Out of memory\n");
			data_blob_free(&request);
			return;
		}

		x_fprintf(x_stdout, "OK\n");
		data_blob_free(&request);
		return;
	}

	if ( (strncmp(buf, "TT ", 3) != 0) &&
	     (strncmp(buf, "AF ", 3) != 0) &&
	     (strncmp(buf, "NA ", 3) != 0) ) {
		DEBUG(1, ("SPNEGO request [%s] invalid\n", buf));
		x_fprintf(x_stdout, "BH SPNEGO request invalid\n");
		data_blob_free(&request);
		return;
	}

	/* So we got a server challenge to generate a SPNEGO
           client-to-server request... */

	len = spnego_read_data(ctx, request, &spnego);
	data_blob_free(&request);

	if (len == -1) {
		DEBUG(1, ("Could not read SPNEGO data for [%s]\n", buf));
		x_fprintf(x_stdout, "BH Could not read SPNEGO data\n");
		return;
	}

	if (spnego.type == SPNEGO_NEG_TOKEN_INIT) {

		/* The server offers a list of mechanisms */

		const char **mechType = (const char **)spnego.negTokenInit.mechTypes;

		while (*mechType != NULL) {

#ifdef HAVE_KRB5
			if ( (strcmp(*mechType, OID_KERBEROS5_OLD) == 0) ||
			     (strcmp(*mechType, OID_KERBEROS5) == 0) ) {
				if (manage_client_krb5_init(spnego))
					goto out;
			}
#endif

			if (strcmp(*mechType, OID_NTLMSSP) == 0) {
				if (manage_client_ntlmssp_init(spnego))
					goto out;
			}

			mechType++;
		}

		DEBUG(1, ("Server offered no compatible mechanism\n"));
		x_fprintf(x_stdout, "BH Server offered no compatible mechanism\n");
		return;
	}

	if (spnego.type == SPNEGO_NEG_TOKEN_TARG) {

		if (spnego.negTokenTarg.supportedMech == NULL) {
			/* On accept/reject Windows does not send the
                           mechanism anymore. Handle that here and
                           shut down the mechanisms. */

			switch (spnego.negTokenTarg.negResult) {
			case SPNEGO_ACCEPT_COMPLETED:
				x_fprintf(x_stdout, "AF\n");
				break;
			case SPNEGO_REJECT:
				x_fprintf(x_stdout, "NA\n");
				break;
			default:
				DEBUG(1, ("Got a negTokenTarg with no mech and an "
					  "unknown negResult: %d\n",
					  spnego.negTokenTarg.negResult));
				x_fprintf(x_stdout, "BH Got a negTokenTarg with"
						    " no mech and an unknown "
						    "negResult\n");
			}

			TALLOC_FREE(client_ntlmssp_state);
			goto out;
		}

		if (strcmp(spnego.negTokenTarg.supportedMech,
			   OID_NTLMSSP) == 0) {
			manage_client_ntlmssp_targ(spnego);
			goto out;
		}

#if HAVE_KRB5
		if (strcmp(spnego.negTokenTarg.supportedMech,
			   OID_KERBEROS5_OLD) == 0) {
			manage_client_krb5_targ(spnego);
			goto out;
		}
#endif

	}

	DEBUG(1, ("Got an SPNEGO token I could not handle [%s]!\n", buf));
	x_fprintf(x_stdout, "BH Got an SPNEGO token I could not handle\n");
	return;

 out:
	spnego_free_data(&spnego);
	return;
}

static void manage_ntlm_server_1_request(struct ntlm_auth_state *state,
						char *buf, int length)
{
	char *request, *parameter;	
	static DATA_BLOB challenge;
	static DATA_BLOB lm_response;
	static DATA_BLOB nt_response;
	static char *full_username;
	static char *username;
	static char *domain;
	static char *plaintext_password;
	static bool ntlm_server_1_user_session_key;
	static bool ntlm_server_1_lm_session_key;

	if (strequal(buf, ".")) {
		if (!full_username && !username) {	
			x_fprintf(x_stdout, "Error: No username supplied!\n");
		} else if (plaintext_password) {
			/* handle this request as plaintext */
			if (!full_username) {
				if (asprintf(&full_username, "%s%c%s", domain, winbind_separator(), username) == -1) {
					x_fprintf(x_stdout, "Error: Out of memory in asprintf!\n.\n");
					return;
				}
			}
			if (check_plaintext_auth(full_username, plaintext_password, False)) {
				x_fprintf(x_stdout, "Authenticated: Yes\n");
			} else {
				x_fprintf(x_stdout, "Authenticated: No\n");
			}
		} else if (!lm_response.data && !nt_response.data) {
			x_fprintf(x_stdout, "Error: No password supplied!\n");
		} else if (!challenge.data) {	
			x_fprintf(x_stdout, "Error: No lanman-challenge supplied!\n");
		} else {
			char *error_string = NULL;
			uchar lm_key[8];
			uchar user_session_key[16];
			uint32 flags = 0;

			if (full_username && !username) {
				fstring fstr_user;
				fstring fstr_domain;

				if (!parse_ntlm_auth_domain_user(full_username, fstr_user, fstr_domain)) {
					/* username might be 'tainted', don't print into our new-line deleimianted stream */
					x_fprintf(x_stdout, "Error: Could not parse into domain and username\n");
				}
				SAFE_FREE(username);
				SAFE_FREE(domain);
				username = smb_xstrdup(fstr_user);
				domain = smb_xstrdup(fstr_domain);
			}

			if (!domain) {
				domain = smb_xstrdup(get_winbind_domain());
			}

			if (ntlm_server_1_lm_session_key) 
				flags |= WBFLAG_PAM_LMKEY;

			if (ntlm_server_1_user_session_key) 
				flags |= WBFLAG_PAM_USER_SESSION_KEY;

			if (!NT_STATUS_IS_OK(
				    contact_winbind_auth_crap(username, 
							      domain, 
							      global_myname(),
							      &challenge, 
							      &lm_response, 
							      &nt_response, 
							      flags, 
							      lm_key, 
							      user_session_key,
							      &error_string,
							      NULL))) {

				x_fprintf(x_stdout, "Authenticated: No\n");
				x_fprintf(x_stdout, "Authentication-Error: %s\n.\n", error_string);
			} else {
				static char zeros[16];
				char *hex_lm_key;
				char *hex_user_session_key;

				x_fprintf(x_stdout, "Authenticated: Yes\n");

				if (ntlm_server_1_lm_session_key 
				    && (memcmp(zeros, lm_key, 
					       sizeof(lm_key)) != 0)) {
					hex_lm_key = hex_encode_talloc(NULL,
								(const unsigned char *)lm_key,
								sizeof(lm_key));
					x_fprintf(x_stdout, "LANMAN-Session-Key: %s\n", hex_lm_key);
					TALLOC_FREE(hex_lm_key);
				}

				if (ntlm_server_1_user_session_key 
				    && (memcmp(zeros, user_session_key, 
					       sizeof(user_session_key)) != 0)) {
					hex_user_session_key = hex_encode_talloc(NULL,
									  (const unsigned char *)user_session_key, 
									  sizeof(user_session_key));
					x_fprintf(x_stdout, "User-Session-Key: %s\n", hex_user_session_key);
					TALLOC_FREE(hex_user_session_key);
				}
			}
			SAFE_FREE(error_string);
		}
		/* clear out the state */
		challenge = data_blob_null;
		nt_response = data_blob_null;
		lm_response = data_blob_null;
		SAFE_FREE(full_username);
		SAFE_FREE(username);
		SAFE_FREE(domain);
		SAFE_FREE(plaintext_password);
		ntlm_server_1_user_session_key = False;
		ntlm_server_1_lm_session_key = False;
		x_fprintf(x_stdout, ".\n");

		return;
	}

	request = buf;

	/* Indicates a base64 encoded structure */
	parameter = strstr_m(request, ":: ");
	if (!parameter) {
		parameter = strstr_m(request, ": ");

		if (!parameter) {
			DEBUG(0, ("Parameter not found!\n"));
			x_fprintf(x_stdout, "Error: Parameter not found!\n.\n");
			return;
		}

		parameter[0] ='\0';
		parameter++;
		parameter[0] ='\0';
		parameter++;

	} else {
		parameter[0] ='\0';
		parameter++;
		parameter[0] ='\0';
		parameter++;
		parameter[0] ='\0';
		parameter++;

		base64_decode_inplace(parameter);
	}

	if (strequal(request, "LANMAN-Challenge")) {
		challenge = strhex_to_data_blob(NULL, parameter);
		if (challenge.length != 8) {
			x_fprintf(x_stdout, "Error: hex decode of %s failed! (got %d bytes, expected 8)\n.\n", 
				  parameter,
				  (int)challenge.length);
			challenge = data_blob_null;
		}
	} else if (strequal(request, "NT-Response")) {
		nt_response = strhex_to_data_blob(NULL, parameter);
		if (nt_response.length < 24) {
			x_fprintf(x_stdout, "Error: hex decode of %s failed! (only got %d bytes, needed at least 24)\n.\n", 
				  parameter,
				  (int)nt_response.length);
			nt_response = data_blob_null;
		}
	} else if (strequal(request, "LANMAN-Response")) {
		lm_response = strhex_to_data_blob(NULL, parameter);
		if (lm_response.length != 24) {
			x_fprintf(x_stdout, "Error: hex decode of %s failed! (got %d bytes, expected 24)\n.\n", 
				  parameter,
				  (int)lm_response.length);
			lm_response = data_blob_null;
		}
	} else if (strequal(request, "Password")) {
		plaintext_password = smb_xstrdup(parameter);
	} else if (strequal(request, "NT-Domain")) {
		domain = smb_xstrdup(parameter);
	} else if (strequal(request, "Username")) {
		username = smb_xstrdup(parameter);
	} else if (strequal(request, "Full-Username")) {
		full_username = smb_xstrdup(parameter);
	} else if (strequal(request, "Request-User-Session-Key")) {
		ntlm_server_1_user_session_key = strequal(parameter, "Yes");
	} else if (strequal(request, "Request-LanMan-Session-Key")) {
		ntlm_server_1_lm_session_key = strequal(parameter, "Yes");
	} else {
		x_fprintf(x_stdout, "Error: Unknown request %s\n.\n", request);
	}
}

static void manage_ntlm_change_password_1_request(struct ntlm_auth_state *state,
							char *buf, int length)
{
	char *request, *parameter;	
	static DATA_BLOB new_nt_pswd;
	static DATA_BLOB old_nt_hash_enc;
	static DATA_BLOB new_lm_pswd;
	static DATA_BLOB old_lm_hash_enc;
	static char *full_username = NULL;
	static char *username = NULL;
	static char *domain = NULL;
	static char *newpswd =  NULL;
	static char *oldpswd = NULL;

	if (strequal(buf, "."))	{
		if(newpswd && oldpswd) {
			uchar old_nt_hash[16];
			uchar old_lm_hash[16];
			uchar new_nt_hash[16];
			uchar new_lm_hash[16];

			new_nt_pswd = data_blob(NULL, 516);
			old_nt_hash_enc = data_blob(NULL, 16);

			/* Calculate the MD4 hash (NT compatible) of the
			 * password */
			E_md4hash(oldpswd, old_nt_hash);
			E_md4hash(newpswd, new_nt_hash);

			/* E_deshash returns false for 'long'
			   passwords (> 14 DOS chars).  

			   Therefore, don't send a buffer
			   encrypted with the truncated hash
			   (it could allow an even easier
			   attack on the password)

			   Likewise, obey the admin's restriction
			*/

			if (lp_client_lanman_auth() &&
			    E_deshash(newpswd, new_lm_hash) &&
			    E_deshash(oldpswd, old_lm_hash)) {
				new_lm_pswd = data_blob(NULL, 516);
				old_lm_hash_enc = data_blob(NULL, 16);
				encode_pw_buffer(new_lm_pswd.data, newpswd,
						 STR_UNICODE);

				arcfour_crypt(new_lm_pswd.data, old_nt_hash, 516);
				E_old_pw_hash(new_nt_hash, old_lm_hash,
					      old_lm_hash_enc.data);
			} else {
				new_lm_pswd.data = NULL;
				new_lm_pswd.length = 0;
				old_lm_hash_enc.data = NULL;
				old_lm_hash_enc.length = 0;
			}

			encode_pw_buffer(new_nt_pswd.data, newpswd,
					 STR_UNICODE);

			arcfour_crypt(new_nt_pswd.data, old_nt_hash, 516);
			E_old_pw_hash(new_nt_hash, old_nt_hash,
				      old_nt_hash_enc.data);
		}

		if (!full_username && !username) {	
			x_fprintf(x_stdout, "Error: No username supplied!\n");
		} else if ((!new_nt_pswd.data || !old_nt_hash_enc.data) &&
			   (!new_lm_pswd.data || old_lm_hash_enc.data) ) {
			x_fprintf(x_stdout, "Error: No NT or LM password "
				  "blobs supplied!\n");
		} else {
			char *error_string = NULL;

			if (full_username && !username)	{
				fstring fstr_user;
				fstring fstr_domain;

				if (!parse_ntlm_auth_domain_user(full_username,
								 fstr_user,
								 fstr_domain)) {
					/* username might be 'tainted', don't
					 * print into our new-line
					 * deleimianted stream */
					x_fprintf(x_stdout, "Error: Could not "
						  "parse into domain and "
						  "username\n");
					SAFE_FREE(username);
					username = smb_xstrdup(full_username);
				} else {
					SAFE_FREE(username);
					SAFE_FREE(domain);
					username = smb_xstrdup(fstr_user);
					domain = smb_xstrdup(fstr_domain);
				}

			}

			if(!NT_STATUS_IS_OK(contact_winbind_change_pswd_auth_crap(
						    username, domain,
						    new_nt_pswd,
						    old_nt_hash_enc,
						    new_lm_pswd,
						    old_lm_hash_enc,
						    &error_string))) {
				x_fprintf(x_stdout, "Password-Change: No\n");
				x_fprintf(x_stdout, "Password-Change-Error: "
					  "%s\n.\n", error_string);
			} else {
				x_fprintf(x_stdout, "Password-Change: Yes\n");
			}

			SAFE_FREE(error_string);
		}
		/* clear out the state */
		new_nt_pswd = data_blob_null;
		old_nt_hash_enc = data_blob_null;
		new_lm_pswd = data_blob_null;
		old_nt_hash_enc = data_blob_null;
		SAFE_FREE(full_username);
		SAFE_FREE(username);
		SAFE_FREE(domain);
		SAFE_FREE(newpswd);
		SAFE_FREE(oldpswd);
		x_fprintf(x_stdout, ".\n");

		return;
	}

	request = buf;

	/* Indicates a base64 encoded structure */
	parameter = strstr_m(request, ":: ");
	if (!parameter) {
		parameter = strstr_m(request, ": ");

		if (!parameter)	{
			DEBUG(0, ("Parameter not found!\n"));
			x_fprintf(x_stdout, "Error: Parameter not found!\n.\n");
			return;
		}

		parameter[0] ='\0';
		parameter++;
		parameter[0] ='\0';
		parameter++;
	} else {
		parameter[0] ='\0';
		parameter++;
		parameter[0] ='\0';
		parameter++;
		parameter[0] ='\0';
		parameter++;

		base64_decode_inplace(parameter);
	}

	if (strequal(request, "new-nt-password-blob")) {
		new_nt_pswd = strhex_to_data_blob(NULL, parameter);
		if (new_nt_pswd.length != 516) {
			x_fprintf(x_stdout, "Error: hex decode of %s failed! "
				  "(got %d bytes, expected 516)\n.\n", 
				  parameter,
				  (int)new_nt_pswd.length);
			new_nt_pswd = data_blob_null;
		}
	} else if (strequal(request, "old-nt-hash-blob")) {
		old_nt_hash_enc = strhex_to_data_blob(NULL, parameter);
		if (old_nt_hash_enc.length != 16) {
			x_fprintf(x_stdout, "Error: hex decode of %s failed! "
				  "(got %d bytes, expected 16)\n.\n", 
				  parameter,
				  (int)old_nt_hash_enc.length);
			old_nt_hash_enc = data_blob_null;
		}
	} else if (strequal(request, "new-lm-password-blob")) {
		new_lm_pswd = strhex_to_data_blob(NULL, parameter);
		if (new_lm_pswd.length != 516) {
			x_fprintf(x_stdout, "Error: hex decode of %s failed! "
				  "(got %d bytes, expected 516)\n.\n", 
				  parameter,
				  (int)new_lm_pswd.length);
			new_lm_pswd = data_blob_null;
		}
	}
	else if (strequal(request, "old-lm-hash-blob"))	{
		old_lm_hash_enc = strhex_to_data_blob(NULL, parameter);
		if (old_lm_hash_enc.length != 16)
		{
			x_fprintf(x_stdout, "Error: hex decode of %s failed! "
				  "(got %d bytes, expected 16)\n.\n", 
				  parameter,
				  (int)old_lm_hash_enc.length);
			old_lm_hash_enc = data_blob_null;
		}
	} else if (strequal(request, "nt-domain")) {
		domain = smb_xstrdup(parameter);
	} else if(strequal(request, "username")) {
		username = smb_xstrdup(parameter);
	} else if(strequal(request, "full-username")) {
		username = smb_xstrdup(parameter);
	} else if(strequal(request, "new-password")) {
		newpswd = smb_xstrdup(parameter);
	} else if (strequal(request, "old-password")) {
		oldpswd = smb_xstrdup(parameter);
	} else {
		x_fprintf(x_stdout, "Error: Unknown request %s\n.\n", request);
	}
}

static void manage_squid_request(struct ntlm_auth_state *state,
		stdio_helper_function fn)
{
	char *buf;
	char tmp[INITIAL_BUFFER_SIZE+1];
	int length, buf_size = 0;
	char *c;

	buf = talloc_strdup(state->mem_ctx, "");
	if (!buf) {
		DEBUG(0, ("Failed to allocate input buffer.\n"));
		x_fprintf(x_stderr, "ERR\n");
		exit(1);
	}

	do {

		/* this is not a typo - x_fgets doesn't work too well under
		 * squid */
		if (fgets(tmp, sizeof(tmp)-1, stdin) == NULL) {
			if (ferror(stdin)) {
				DEBUG(1, ("fgets() failed! dying..... errno=%d "
					  "(%s)\n", ferror(stdin),
					  strerror(ferror(stdin))));

				exit(1);
			}
			exit(0);
		}

		buf = talloc_strdup_append_buffer(buf, tmp);
		buf_size += INITIAL_BUFFER_SIZE;

		if (buf_size > MAX_BUFFER_SIZE) {
			DEBUG(2, ("Oversized message\n"));
			x_fprintf(x_stderr, "ERR\n");
			talloc_free(buf);
			return;
		}

		c = strchr(buf, '\n');
	} while (c == NULL);

	*c = '\0';
	length = c-buf;

	DEBUG(10, ("Got '%s' from squid (length: %d).\n",buf,length));

	if (buf[0] == '\0') {
		DEBUG(2, ("Invalid Request\n"));
		x_fprintf(x_stderr, "ERR\n");
		talloc_free(buf);
		return;
	}

	fn(state, buf, length);
	talloc_free(buf);
}


static void squid_stream(enum stdio_helper_mode stdio_mode, stdio_helper_function fn) {
	TALLOC_CTX *mem_ctx;
	struct ntlm_auth_state *state;

	/* initialize FDescs */
	x_setbuf(x_stdout, NULL);
	x_setbuf(x_stderr, NULL);

	mem_ctx = talloc_init("ntlm_auth");
	if (!mem_ctx) {
		DEBUG(0, ("squid_stream: Failed to create talloc context\n"));
		x_fprintf(x_stderr, "ERR\n");
		exit(1);
	}

	state = talloc_zero(mem_ctx, struct ntlm_auth_state);
	if (!state) {
		DEBUG(0, ("squid_stream: Failed to talloc ntlm_auth_state\n"));
		x_fprintf(x_stderr, "ERR\n");
		exit(1);
	}

	state->mem_ctx = mem_ctx;
	state->helper_mode = stdio_mode;

	while(1) {
		TALLOC_CTX *frame = talloc_stackframe();
		manage_squid_request(state, fn);
		TALLOC_FREE(frame);
	}
}


/* Authenticate a user with a challenge/response */

static bool check_auth_crap(void)
{
	NTSTATUS nt_status;
	uint32 flags = 0;
	char lm_key[8];
	char user_session_key[16];
	char *hex_lm_key;
	char *hex_user_session_key;
	char *error_string;
	static uint8 zeros[16];

	x_setbuf(x_stdout, NULL);

	if (request_lm_key) 
		flags |= WBFLAG_PAM_LMKEY;

	if (request_user_session_key) 
		flags |= WBFLAG_PAM_USER_SESSION_KEY;

	flags |= WBFLAG_PAM_NT_STATUS_SQUASH;

	nt_status = contact_winbind_auth_crap(opt_username, opt_domain, 
					      opt_workstation,
					      &opt_challenge, 
					      &opt_lm_response, 
					      &opt_nt_response, 
					      flags,
					      (unsigned char *)lm_key, 
					      (unsigned char *)user_session_key, 
					      &error_string, NULL);

	if (!NT_STATUS_IS_OK(nt_status)) {
		x_fprintf(x_stdout, "%s (0x%x)\n", 
			  error_string,
			  NT_STATUS_V(nt_status));
		SAFE_FREE(error_string);
		return False;
	}

	if (request_lm_key 
	    && (memcmp(zeros, lm_key, 
		       sizeof(lm_key)) != 0)) {
		hex_lm_key = hex_encode_talloc(talloc_tos(), (const unsigned char *)lm_key,
					sizeof(lm_key));
		x_fprintf(x_stdout, "LM_KEY: %s\n", hex_lm_key);
		TALLOC_FREE(hex_lm_key);
	}
	if (request_user_session_key 
	    && (memcmp(zeros, user_session_key, 
		       sizeof(user_session_key)) != 0)) {
		hex_user_session_key = hex_encode_talloc(talloc_tos(), (const unsigned char *)user_session_key, 
						  sizeof(user_session_key));
		x_fprintf(x_stdout, "NT_KEY: %s\n", hex_user_session_key);
		TALLOC_FREE(hex_user_session_key);
	}

        return True;
}

/* Main program */

enum {
	OPT_USERNAME = 1000,
	OPT_DOMAIN,
	OPT_WORKSTATION,
	OPT_CHALLENGE,
	OPT_RESPONSE,
	OPT_LM,
	OPT_NT,
	OPT_PASSWORD,
	OPT_LM_KEY,
	OPT_USER_SESSION_KEY,
	OPT_DIAGNOSTICS,
	OPT_REQUIRE_MEMBERSHIP,
	OPT_USE_CACHED_CREDS,
	OPT_PAM_WINBIND_CONF
};

 int main(int argc, const char **argv)
{
	TALLOC_CTX *frame = talloc_stackframe();
	int opt;
	static const char *helper_protocol;
	static int diagnostics;

	static const char *hex_challenge;
	static const char *hex_lm_response;
	static const char *hex_nt_response;

	poptContext pc;

	/* NOTE: DO NOT change this interface without considering the implications!
	   This is an external interface, which other programs will use to interact 
	   with this helper.
	*/

	/* We do not use single-letter command abbreviations, because they harm future 
	   interface stability. */

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "helper-protocol", 0, POPT_ARG_STRING, &helper_protocol, OPT_DOMAIN, "operate as a stdio-based helper", "helper protocol to use"},
 		{ "username", 0, POPT_ARG_STRING, &opt_username, OPT_USERNAME, "username"},
 		{ "domain", 0, POPT_ARG_STRING, &opt_domain, OPT_DOMAIN, "domain name"},
 		{ "workstation", 0, POPT_ARG_STRING, &opt_workstation, OPT_WORKSTATION, "workstation"},
 		{ "challenge", 0, POPT_ARG_STRING, &hex_challenge, OPT_CHALLENGE, "challenge (HEX encoded)"},
		{ "lm-response", 0, POPT_ARG_STRING, &hex_lm_response, OPT_LM, "LM Response to the challenge (HEX encoded)"},
		{ "nt-response", 0, POPT_ARG_STRING, &hex_nt_response, OPT_NT, "NT or NTLMv2 Response to the challenge (HEX encoded)"},
		{ "password", 0, POPT_ARG_STRING, &opt_password, OPT_PASSWORD, "User's plaintext password"},		
		{ "request-lm-key", 0, POPT_ARG_NONE, &request_lm_key, OPT_LM_KEY, "Retrieve LM session key"},
		{ "request-nt-key", 0, POPT_ARG_NONE, &request_user_session_key, OPT_USER_SESSION_KEY, "Retrieve User (NT) session key"},
		{ "use-cached-creds", 0, POPT_ARG_NONE, &use_cached_creds, OPT_USE_CACHED_CREDS, "Use cached credentials if no password is given"},
		{ "diagnostics", 0, POPT_ARG_NONE, &diagnostics,
		  OPT_DIAGNOSTICS,
		  "Perform diagnostics on the authentication chain"},
		{ "require-membership-of", 0, POPT_ARG_STRING, &require_membership_of, OPT_REQUIRE_MEMBERSHIP, "Require that a user be a member of this group (either name or SID) for authentication to succeed" },
		{ "pam-winbind-conf", 0, POPT_ARG_STRING, &opt_pam_winbind_conf, OPT_PAM_WINBIND_CONF, "Require that request must set WBFLAG_PAM_CONTACT_TRUSTDOM when krb5 auth is required" },
		POPT_COMMON_CONFIGFILE
		POPT_COMMON_VERSION
		POPT_TABLEEND
	};

	/* Samba client initialisation */
	load_case_tables();

	setup_logging("ntlm_auth", DEBUG_STDERR);

	/* Parse options */

	pc = poptGetContext("ntlm_auth", argc, argv, long_options, 0);

	/* Parse command line options */

	if (argc == 1) {
		poptPrintHelp(pc, stderr, 0);
		return 1;
	}

	while((opt = poptGetNextOpt(pc)) != -1) {
		/* Get generic config options like --configfile */
	}

	poptFreeContext(pc);

	if (!lp_load(get_dyn_CONFIGFILE(), True, False, False, True)) {
		d_fprintf(stderr, "ntlm_auth: error opening config file %s. Error was %s\n",
			get_dyn_CONFIGFILE(), strerror(errno));
		exit(1);
	}

	pc = poptGetContext(NULL, argc, (const char **)argv, long_options, 
			    POPT_CONTEXT_KEEP_FIRST);

	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_CHALLENGE:
			opt_challenge = strhex_to_data_blob(NULL, hex_challenge);
			if (opt_challenge.length != 8) {
				x_fprintf(x_stderr, "hex decode of %s failed! (only got %d bytes)\n", 
					  hex_challenge,
					  (int)opt_challenge.length);
				exit(1);
			}
			break;
		case OPT_LM: 
			opt_lm_response = strhex_to_data_blob(NULL, hex_lm_response);
			if (opt_lm_response.length != 24) {
				x_fprintf(x_stderr, "hex decode of %s failed! (only got %d bytes)\n", 
					  hex_lm_response,
					  (int)opt_lm_response.length);
				exit(1);
			}
			break;

		case OPT_NT: 
			opt_nt_response = strhex_to_data_blob(NULL, hex_nt_response);
			if (opt_nt_response.length < 24) {
				x_fprintf(x_stderr, "hex decode of %s failed! (only got %d bytes)\n", 
					  hex_nt_response,
					  (int)opt_nt_response.length);
				exit(1);
			}
			break;

                case OPT_REQUIRE_MEMBERSHIP:
			if (StrnCaseCmp("S-", require_membership_of, 2) == 0) {
				require_membership_of_sid = require_membership_of;
			}
			break;
		}
	}

	if (opt_username) {
		char *domain = SMB_STRDUP(opt_username);
		char *p = strchr_m(domain, *lp_winbind_separator());
		if (p) {
			opt_username = p+1;
			*p = '\0';
			if (opt_domain && !strequal(opt_domain, domain)) {
				x_fprintf(x_stderr, "Domain specified in username (%s) "
					"doesn't match specified domain (%s)!\n\n",
					domain, opt_domain);
				poptPrintHelp(pc, stderr, 0);
				exit(1);
			}
			opt_domain = domain;
		} else {
			SAFE_FREE(domain);
		}
	}

	/* Note: if opt_domain is "" then send no domain */
	if (opt_domain == NULL) {
		opt_domain = get_winbind_domain();
	}

	if (opt_workstation == NULL) {
		opt_workstation = "";
	}

	if (helper_protocol) {
		int i;
		for (i=0; i<NUM_HELPER_MODES; i++) {
			if (strcmp(helper_protocol, stdio_helper_protocols[i].name) == 0) {
				squid_stream(stdio_helper_protocols[i].mode, stdio_helper_protocols[i].fn);
				exit(0);
			}
		}
		x_fprintf(x_stderr, "unknown helper protocol [%s]\n\nValid helper protools:\n\n", helper_protocol);

		for (i=0; i<NUM_HELPER_MODES; i++) {
			x_fprintf(x_stderr, "%s\n", stdio_helper_protocols[i].name);
		}

		exit(1);
	}

	if (!opt_username || !*opt_username) {
		x_fprintf(x_stderr, "username must be specified!\n\n");
		poptPrintHelp(pc, stderr, 0);
		exit(1);
	}

	if (opt_challenge.length) {
		if (!check_auth_crap()) {
			exit(1);
		}
		exit(0);
	} 

	if (!opt_password) {
		opt_password = getpass("password: ");
	}

	if (diagnostics) {
		if (!diagnose_ntlm_auth()) {
			return 1;
		}
	} else {
		fstring user;

		fstr_sprintf(user, "%s%c%s", opt_domain, winbind_separator(), opt_username);
		if (!check_plaintext_auth(user, opt_password, True)) {
			return 1;
		}
	}

	/* Exit code */

	poptFreeContext(pc);
	TALLOC_FREE(frame);
	return 0;
}
