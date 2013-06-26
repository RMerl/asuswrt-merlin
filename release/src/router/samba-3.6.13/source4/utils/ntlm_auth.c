/* 
   Unix SMB/CIFS implementation.

   Winbind status program.

   Copyright (C) Tim Potter      2000-2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2003-2004
   Copyright (C) Francesco Chemolli <kinkie@kame.usr.dsi.unimi.it> 2000 

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
#include "system/filesys.h"
#include "lib/cmdline/popt_common.h"
#include <ldb.h>
#include "auth/credentials/credentials.h"
#include "auth/gensec/gensec.h"
#include "auth/auth.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "auth/auth_sam.h"
#include "libcli/auth/libcli_auth.h"
#include "libcli/security/security.h"
#include "lib/events/events.h"
#include "lib/messaging/messaging.h"
#include "lib/messaging/irpc.h"
#include "auth/ntlmssp/ntlmssp.h"
#include "param/param.h"

#define INITIAL_BUFFER_SIZE 300
#define MAX_BUFFER_SIZE 63000

enum stdio_helper_mode {
	SQUID_2_4_BASIC,
	SQUID_2_5_BASIC,
	SQUID_2_5_NTLMSSP,
	NTLMSSP_CLIENT_1,
	GSS_SPNEGO_CLIENT,
	GSS_SPNEGO_SERVER,
	NTLM_SERVER_1,
	NUM_HELPER_MODES
};

#define NTLM_AUTH_FLAG_USER_SESSION_KEY     0x0004
#define NTLM_AUTH_FLAG_LMKEY                0x0008


typedef void (*stdio_helper_function)(enum stdio_helper_mode stdio_helper_mode, 
				      struct loadparm_context *lp_ctx,
				      char *buf, int length, void **private1,
				      unsigned int mux_id, void **private2);

static void manage_squid_basic_request (enum stdio_helper_mode stdio_helper_mode, 
					struct loadparm_context *lp_ctx,
					char *buf, int length, void **private1,
					unsigned int mux_id, void **private2);

static void manage_gensec_request (enum stdio_helper_mode stdio_helper_mode, 
				   struct loadparm_context *lp_ctx,
				   char *buf, int length, void **private1,
				   unsigned int mux_id, void **private2);

static void manage_ntlm_server_1_request (enum stdio_helper_mode stdio_helper_mode, 
					  struct loadparm_context *lp_ctx,
					  char *buf, int length, void **private1,
					  unsigned int mux_id, void **private2);

static void manage_squid_request(struct loadparm_context *lp_ctx,
				 enum stdio_helper_mode helper_mode, 
				 stdio_helper_function fn, void **private2);

static const struct {
	enum stdio_helper_mode mode;
	const char *name;
	stdio_helper_function fn;
} stdio_helper_protocols[] = {
	{ SQUID_2_4_BASIC, "squid-2.4-basic", manage_squid_basic_request},
	{ SQUID_2_5_BASIC, "squid-2.5-basic", manage_squid_basic_request},
	{ SQUID_2_5_NTLMSSP, "squid-2.5-ntlmssp", manage_gensec_request},
	{ GSS_SPNEGO_CLIENT, "gss-spnego-client", manage_gensec_request},
	{ GSS_SPNEGO_SERVER, "gss-spnego", manage_gensec_request},
	{ NTLMSSP_CLIENT_1, "ntlmssp-client-1", manage_gensec_request},
	{ NTLM_SERVER_1, "ntlm-server-1", manage_ntlm_server_1_request},
	{ NUM_HELPER_MODES, NULL, NULL}
};

extern int winbindd_fd;

static const char *opt_username;
static const char *opt_domain;
static const char *opt_workstation;
static const char *opt_password;
static int opt_multiplex;
static int use_cached_creds;


static void mux_printf(unsigned int mux_id, const char *format, ...) PRINTF_ATTRIBUTE(2, 3);

static void mux_printf(unsigned int mux_id, const char *format, ...)
{
	va_list ap;

	if (opt_multiplex) {
		x_fprintf(x_stdout, "%d ", mux_id);
	}

	va_start(ap, format);
	x_vfprintf(x_stdout, format, ap);
	va_end(ap);
}



/* Copy of parse_domain_user from winbindd_util.c.  Parse a string of the
   form DOMAIN/user into a domain and a user */

static bool parse_ntlm_auth_domain_user(const char *domuser, char **domain, 
										char **user, char winbind_separator)
{

	char *p = strchr(domuser, winbind_separator);

	if (!p) {
		return false;
	}
        
	*user = smb_xstrdup(p+1);
	*domain = smb_xstrdup(domuser);
	(*domain)[PTR_DIFF(p, domuser)] = 0;

	return true;
}

/**
 * Decode a base64 string into a DATA_BLOB - simple and slow algorithm
 **/
static DATA_BLOB base64_decode_data_blob(TALLOC_CTX *mem_ctx, const char *s)
{
	DATA_BLOB ret = data_blob_talloc(mem_ctx, s, strlen(s)+1);
	ret.length = ldb_base64_decode((char *)ret.data);
	return ret;
}

/**
 * Encode a base64 string into a talloc()ed string caller to free.
 **/
static char *base64_encode_data_blob(TALLOC_CTX *mem_ctx, DATA_BLOB data)
{
	return ldb_base64_encode(mem_ctx, (const char *)data.data, data.length);
}

/**
 * Decode a base64 string in-place - wrapper for the above
 **/
static void base64_decode_inplace(char *s)
{
	ldb_base64_decode(s);
}



/* Authenticate a user with a plaintext password */

static bool check_plaintext_auth(const char *user, const char *pass, 
				 bool stdout_diagnostics)
{
        return (strcmp(pass, opt_password) == 0);
}

/* authenticate a user with an encrypted username/password */

static NTSTATUS local_pw_check_specified(struct loadparm_context *lp_ctx,
					 const char *username, 
					 const char *domain, 
					 const char *workstation,
					 const DATA_BLOB *challenge, 
					 const DATA_BLOB *lm_response, 
					 const DATA_BLOB *nt_response, 
					 uint32_t flags, 
					 DATA_BLOB *lm_session_key, 
					 DATA_BLOB *user_session_key, 
					 char **error_string, 
					 char **unix_name) 
{
	NTSTATUS nt_status;
	struct samr_Password lm_pw, nt_pw;
	struct samr_Password *lm_pwd, *nt_pwd;
	TALLOC_CTX *mem_ctx = talloc_init("local_pw_check_specified");
	if (!mem_ctx) {
		nt_status = NT_STATUS_NO_MEMORY;
	} else {
		
		E_md4hash(opt_password, nt_pw.hash);
		if (E_deshash(opt_password, lm_pw.hash)) {
			lm_pwd = &lm_pw;
		} else {
			lm_pwd = NULL;
		}
		nt_pwd = &nt_pw;
		
		
		nt_status = ntlm_password_check(mem_ctx, 
						lpcfg_lanman_auth(lp_ctx),
						lpcfg_ntlm_auth(lp_ctx),
						MSV1_0_ALLOW_SERVER_TRUST_ACCOUNT |
						MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT,
						challenge,
						lm_response,
						nt_response,
						username,
						username,
						domain,
						lm_pwd, nt_pwd, user_session_key, lm_session_key);
		
		if (NT_STATUS_IS_OK(nt_status)) {
			if (unix_name) {
				if (asprintf(unix_name, "%s%c%s", domain,
					     *lpcfg_winbind_separator(lp_ctx),
					     username) < 0) {
					nt_status = NT_STATUS_NO_MEMORY;
				}
			}
		} else {
			DEBUG(3, ("Login for user [%s]\\[%s]@[%s] failed due to [%s]\n", 
				  domain, username, workstation, 
				  nt_errstr(nt_status)));
		}
		talloc_free(mem_ctx);
	}
	if (error_string) {
		*error_string = strdup(nt_errstr(nt_status));
	}
	return nt_status;
	
	
}

static void manage_squid_basic_request(enum stdio_helper_mode stdio_helper_mode, 
				       struct loadparm_context *lp_ctx,
				       char *buf, int length, void **private1,
				       unsigned int mux_id, void **private2) 
{
	char *user, *pass;	
	user=buf;
	
	pass = memchr(buf, ' ', length);
	if (!pass) {
		DEBUG(2, ("Password not found. Denying access\n"));
		mux_printf(mux_id, "ERR\n");
		return;
	}
	*pass='\0';
	pass++;
	
	if (stdio_helper_mode == SQUID_2_5_BASIC) {
		rfc1738_unescape(user);
		rfc1738_unescape(pass);
	}
	
	if (check_plaintext_auth(user, pass, false)) {
		mux_printf(mux_id, "OK\n");
	} else {
		mux_printf(mux_id, "ERR\n");
	}
}

/* This is a bit hairy, but the basic idea is to do a password callback
   to the calling application.  The callback comes from within gensec */

static void manage_gensec_get_pw_request(enum stdio_helper_mode stdio_helper_mode, 
					 struct loadparm_context *lp_ctx,
					 char *buf, int length, void **private1,
					 unsigned int mux_id, void **password)  
{
	DATA_BLOB in;
	if (strlen(buf) < 2) {
		DEBUG(1, ("query [%s] invalid", buf));
		mux_printf(mux_id, "BH Query invalid\n");
		return;
	}

	if (strlen(buf) > 3) {
		in = base64_decode_data_blob(NULL, buf + 3);
	} else {
		in = data_blob(NULL, 0);
	}

	if (strncmp(buf, "PW ", 3) == 0) {

		*password = talloc_strndup(*private1 /* hopefully the right gensec context, useful to use for talloc */,
					   (const char *)in.data, in.length);
		
		if (*password == NULL) {
			DEBUG(1, ("Out of memory\n"));
			mux_printf(mux_id, "BH Out of memory\n");
			data_blob_free(&in);
			return;
		}

		mux_printf(mux_id, "OK\n");
		data_blob_free(&in);
		return;
	}
	DEBUG(1, ("Asked for (and expected) a password\n"));
	mux_printf(mux_id, "BH Expected a password\n");
	data_blob_free(&in);
}

/** 
 * Callback for password credentials.  This is not async, and when
 * GENSEC and the credentials code is made async, it will look rather
 * different.
 */

static const char *get_password(struct cli_credentials *credentials) 
{
	char *password = NULL;
	
	/* Ask for a password */
	mux_printf((unsigned int)(uintptr_t)credentials->priv_data, "PW\n");
	credentials->priv_data = NULL;

	manage_squid_request(cmdline_lp_ctx, NUM_HELPER_MODES /* bogus */, manage_gensec_get_pw_request, (void **)&password);
	return password;
}

/**
 Check if a string is part of a list.
**/
static bool in_list(const char *s, const char *list, bool casesensitive)
{
	char *tok;
	size_t tok_len = 1024;
	const char *p=list;

	if (!list)
		return false;

	tok = (char *)malloc(tok_len);
	if (!tok) {
		return false;
	}

	while (next_token(&p, tok, LIST_SEP, tok_len)) {
		if ((casesensitive?strcmp:strcasecmp_m)(tok,s) == 0) {
			free(tok);
			return true;
		}
	}
	free(tok);
	return false;
}

static void gensec_want_feature_list(struct gensec_security *state, char* feature_list)
{
	if (in_list("NTLMSSP_FEATURE_SESSION_KEY", feature_list, true)) {
		DEBUG(10, ("want GENSEC_FEATURE_SESSION_KEY\n"));
		gensec_want_feature(state, GENSEC_FEATURE_SESSION_KEY);
	}
	if (in_list("NTLMSSP_FEATURE_SIGN", feature_list, true)) {
		DEBUG(10, ("want GENSEC_FEATURE_SIGN\n"));
		gensec_want_feature(state, GENSEC_FEATURE_SIGN);
	}
	if (in_list("NTLMSSP_FEATURE_SEAL", feature_list, true)) {
		DEBUG(10, ("want GENSEC_FEATURE_SEAL\n"));
		gensec_want_feature(state, GENSEC_FEATURE_SEAL);
	}
}

static void manage_gensec_request(enum stdio_helper_mode stdio_helper_mode, 
				  struct loadparm_context *lp_ctx,
				  char *buf, int length, void **private1,
				  unsigned int mux_id, void **private2) 
{
	DATA_BLOB in;
	DATA_BLOB out = data_blob(NULL, 0);
	char *out_base64 = NULL;
	const char *reply_arg = NULL;
	struct gensec_ntlm_state {
		struct gensec_security *gensec_state;
		const char *set_password;
	};
	struct gensec_ntlm_state *state;
	struct tevent_context *ev;
	struct messaging_context *msg;

	NTSTATUS nt_status;
	bool first = false;
	const char *reply_code;
	struct cli_credentials *creds;

	static char *want_feature_list = NULL;
	static DATA_BLOB session_key;

	TALLOC_CTX *mem_ctx;

	if (*private1) {
		state = (struct gensec_ntlm_state *)*private1;
	} else {
		state = talloc_zero(NULL, struct gensec_ntlm_state);
		if (!state) {
			mux_printf(mux_id, "BH No Memory\n");
			exit(1);
		}
		*private1 = state;
		if (opt_password) {
			state->set_password = opt_password;
		}
	}
	
	if (strlen(buf) < 2) {
		DEBUG(1, ("query [%s] invalid", buf));
		mux_printf(mux_id, "BH Query invalid\n");
		return;
	}

	if (strlen(buf) > 3) {
		if(strncmp(buf, "SF ", 3) == 0) {
			DEBUG(10, ("Setting flags to negotiate\n"));
			talloc_free(want_feature_list);
			want_feature_list = talloc_strndup(state, buf+3, strlen(buf)-3);
			mux_printf(mux_id, "OK\n");
			return;
		}
		in = base64_decode_data_blob(NULL, buf + 3);
	} else {
		in = data_blob(NULL, 0);
	}

	if (strncmp(buf, "YR", 2) == 0) {
		if (state->gensec_state) {
			talloc_free(state->gensec_state);
			state->gensec_state = NULL;
		}
	} else if ( (strncmp(buf, "OK", 2) == 0)) {
		/* Just return BH, like ntlm_auth from Samba 3 does. */
		mux_printf(mux_id, "BH Command expected\n");
		data_blob_free(&in);
		return;
	} else if ( (strncmp(buf, "TT ", 3) != 0) &&
		    (strncmp(buf, "KK ", 3) != 0) &&
		    (strncmp(buf, "AF ", 3) != 0) &&
		    (strncmp(buf, "NA ", 3) != 0) && 
		    (strncmp(buf, "UG", 2) != 0) && 
		    (strncmp(buf, "PW ", 3) != 0) &&
		    (strncmp(buf, "GK", 2) != 0) &&
		    (strncmp(buf, "GF", 2) != 0)) {
		DEBUG(1, ("SPNEGO request [%s] invalid\n", buf));
		mux_printf(mux_id, "BH SPNEGO request invalid\n");
		data_blob_free(&in);
		return;
	}

	ev = s4_event_context_init(state);
	if (!ev) {
		exit(1);
	}

	mem_ctx = talloc_named(NULL, 0, "manage_gensec_request internal mem_ctx");

	/* setup gensec */
	if (!(state->gensec_state)) {
		switch (stdio_helper_mode) {
		case GSS_SPNEGO_CLIENT:
		case NTLMSSP_CLIENT_1:
			/* setup the client side */

			nt_status = gensec_client_start(NULL, &state->gensec_state, ev, 
							lpcfg_gensec_settings(NULL, lp_ctx));
			if (!NT_STATUS_IS_OK(nt_status)) {
				talloc_free(mem_ctx);
				exit(1);
			}

			break;
		case GSS_SPNEGO_SERVER:
		case SQUID_2_5_NTLMSSP:
		{
			const char *winbind_method[] = { "winbind", NULL };
			struct auth_context *auth_context;

			msg = messaging_client_init(state, lpcfg_messaging_path(state, lp_ctx), ev);
			if (!msg) {
				talloc_free(mem_ctx);
				exit(1);
			}
			nt_status = auth_context_create_methods(mem_ctx, 
								winbind_method,
								ev, 
								msg, 
								lp_ctx,
								NULL,
								&auth_context);
	
			if (!NT_STATUS_IS_OK(nt_status)) {
				talloc_free(mem_ctx);
				exit(1);
			}
			
			if (!NT_STATUS_IS_OK(gensec_server_start(state, ev, 
								 lpcfg_gensec_settings(state, lp_ctx),
								 auth_context, &state->gensec_state))) {
				talloc_free(mem_ctx);
				exit(1);
			}
			break;
		}
		default:
			talloc_free(mem_ctx);
			abort();
		}

		creds = cli_credentials_init(state->gensec_state);
		cli_credentials_set_conf(creds, lp_ctx);
		if (opt_username) {
			cli_credentials_set_username(creds, opt_username, CRED_SPECIFIED);
		}
		if (opt_domain) {
			cli_credentials_set_domain(creds, opt_domain, CRED_SPECIFIED);
		}
		if (state->set_password) {
			cli_credentials_set_password(creds, state->set_password, CRED_SPECIFIED);
		} else {
			cli_credentials_set_password_callback(creds, get_password);
			creds->priv_data = (void*)(uintptr_t)mux_id;
		}
		if (opt_workstation) {
			cli_credentials_set_workstation(creds, opt_workstation, CRED_SPECIFIED);
		}
		
		switch (stdio_helper_mode) {
		case GSS_SPNEGO_SERVER:
		case SQUID_2_5_NTLMSSP:
			cli_credentials_set_machine_account(creds, lp_ctx);
			break;
		default:
			break;
		}

		gensec_set_credentials(state->gensec_state, creds);
		gensec_want_feature_list(state->gensec_state, want_feature_list);

		switch (stdio_helper_mode) {
		case GSS_SPNEGO_CLIENT:
		case GSS_SPNEGO_SERVER:
			nt_status = gensec_start_mech_by_oid(state->gensec_state, GENSEC_OID_SPNEGO);
			if (!in.length) {
				first = true;
			}
			break;
		case NTLMSSP_CLIENT_1:
			if (!in.length) {
				first = true;
			}
			/* fall through */
		case SQUID_2_5_NTLMSSP:
			nt_status = gensec_start_mech_by_oid(state->gensec_state, GENSEC_OID_NTLMSSP);
			break;
		default:
			talloc_free(mem_ctx);
			abort();
		}

		if (!NT_STATUS_IS_OK(nt_status)) {
			DEBUG(1, ("GENSEC mech failed to start: %s\n", nt_errstr(nt_status)));
			mux_printf(mux_id, "BH GENSEC mech failed to start\n");
			talloc_free(mem_ctx);
			return;
		}

	}

	/* update */

	if (strncmp(buf, "PW ", 3) == 0) {
		state->set_password = talloc_strndup(state,
						     (const char *)in.data, 
						     in.length);
		
		cli_credentials_set_password(gensec_get_credentials(state->gensec_state),
					     state->set_password,
					     CRED_SPECIFIED);
		mux_printf(mux_id, "OK\n");
		data_blob_free(&in);
		talloc_free(mem_ctx);
		return;
	}

	if (strncmp(buf, "UG", 2) == 0) {
		int i;
		char *grouplist = NULL;
		struct auth_session_info *session_info;

		nt_status = gensec_session_info(state->gensec_state, &session_info); 
		if (!NT_STATUS_IS_OK(nt_status)) {
			DEBUG(1, ("gensec_session_info failed: %s\n", nt_errstr(nt_status)));
			mux_printf(mux_id, "BH %s\n", nt_errstr(nt_status));
			data_blob_free(&in);
			talloc_free(mem_ctx);
			return;
		}
		
		/* get the string onto the context */
		grouplist = talloc_strdup(mem_ctx, "");
		
		for (i=0; i<session_info->security_token->num_sids; i++) {
			struct security_token *token = session_info->security_token; 
			const char *sidstr = dom_sid_string(session_info, 
							    &token->sids[i]);
			grouplist = talloc_asprintf_append_buffer(grouplist, "%s,", sidstr);
		}

		mux_printf(mux_id, "GL %s\n", grouplist);
		talloc_free(session_info);
		data_blob_free(&in);
		talloc_free(mem_ctx);
		return;
	}

	if (strncmp(buf, "GK", 2) == 0) {
		char *base64_key;
		DEBUG(10, ("Requested session key\n"));
		nt_status = gensec_session_key(state->gensec_state, &session_key);
		if(!NT_STATUS_IS_OK(nt_status)) {
			DEBUG(1, ("gensec_session_key failed: %s\n", nt_errstr(nt_status)));
			mux_printf(mux_id, "BH No session key\n");
			talloc_free(mem_ctx);
			return;
		} else {
			base64_key = base64_encode_data_blob(state, session_key);
			mux_printf(mux_id, "GK %s\n", base64_key);
			talloc_free(base64_key);
		}
		talloc_free(mem_ctx);
		return;
	}

	if (strncmp(buf, "GF", 2) == 0) {
		struct ntlmssp_state *ntlmssp_state;
		uint32_t neg_flags;

		ntlmssp_state = talloc_get_type(state->gensec_state->private_data,
				struct ntlmssp_state);
		neg_flags = ntlmssp_state->neg_flags;

		DEBUG(10, ("Requested negotiated feature flags\n"));
		mux_printf(mux_id, "GF 0x%08x\n", neg_flags);
		return;
	}

	nt_status = gensec_update(state->gensec_state, mem_ctx, in, &out);
	
	/* don't leak 'bad password'/'no such user' info to the network client */
	nt_status = nt_status_squash(nt_status);

	if (out.length) {
		out_base64 = base64_encode_data_blob(mem_ctx, out);
	} else {
		out_base64 = NULL;
	}

	if (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		reply_arg = "*";
		if (first) {
			reply_code = "YR";
		} else if (state->gensec_state->gensec_role == GENSEC_CLIENT) { 
			reply_code = "KK";
		} else if (state->gensec_state->gensec_role == GENSEC_SERVER) { 
			reply_code = "TT";
		} else {
			abort();
		}


	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_ACCESS_DENIED)) {
		reply_code = "BH NT_STATUS_ACCESS_DENIED";
		reply_arg = nt_errstr(nt_status);
		DEBUG(1, ("GENSEC login failed: %s\n", nt_errstr(nt_status)));
	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_UNSUCCESSFUL)) {
		reply_code = "BH NT_STATUS_UNSUCCESSFUL";
		reply_arg = nt_errstr(nt_status);
		DEBUG(1, ("GENSEC login failed: %s\n", nt_errstr(nt_status)));
	} else if (!NT_STATUS_IS_OK(nt_status)) {
		reply_code = "NA";
		reply_arg = nt_errstr(nt_status);
		DEBUG(1, ("GENSEC login failed: %s\n", nt_errstr(nt_status)));
	} else if /* OK */ (state->gensec_state->gensec_role == GENSEC_SERVER) {
		struct auth_session_info *session_info;

		nt_status = gensec_session_info(state->gensec_state, &session_info);
		if (!NT_STATUS_IS_OK(nt_status)) {
			reply_code = "BH Failed to retrive session info";
			reply_arg = nt_errstr(nt_status);
			DEBUG(1, ("GENSEC failed to retrieve the session info: %s\n", nt_errstr(nt_status)));
		} else {

			reply_code = "AF";
			reply_arg = talloc_asprintf(state->gensec_state, 
						    "%s%s%s", session_info->info->domain_name,
						    lpcfg_winbind_separator(lp_ctx), session_info->info->account_name);
			talloc_free(session_info);
		}
	} else if (state->gensec_state->gensec_role == GENSEC_CLIENT) {
		reply_code = "AF";
		reply_arg = out_base64;
	} else {
		abort();
	}

	switch (stdio_helper_mode) {
	case GSS_SPNEGO_SERVER:
		mux_printf(mux_id, "%s %s %s\n", reply_code, 
			  out_base64 ? out_base64 : "*", 
			  reply_arg ? reply_arg : "*");
		break;
	default:
		if (out_base64) {
			mux_printf(mux_id, "%s %s\n", reply_code, out_base64);
		} else if (reply_arg) {
			mux_printf(mux_id, "%s %s\n", reply_code, reply_arg);
		} else {
			mux_printf(mux_id, "%s\n", reply_code);
		}
	}

	talloc_free(mem_ctx);
	return;
}

static void manage_ntlm_server_1_request(enum stdio_helper_mode stdio_helper_mode, 
					 struct loadparm_context *lp_ctx,
					 char *buf, int length, void **private1,
					 unsigned int mux_id, void **private2) 
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
			mux_printf(mux_id, "Error: No username supplied!\n");
		} else if (plaintext_password) {
			/* handle this request as plaintext */
			if (!full_username) {
				if (asprintf(&full_username, "%s%c%s", domain, *lpcfg_winbind_separator(lp_ctx), username) < 0) {
					mux_printf(mux_id, "Error: Out of memory in asprintf!\n.\n");
					return;
				}
			}
			if (check_plaintext_auth(full_username, plaintext_password, false)) {
				mux_printf(mux_id, "Authenticated: Yes\n");
			} else {
				mux_printf(mux_id, "Authenticated: No\n");
			}
		} else if (!lm_response.data && !nt_response.data) {
			mux_printf(mux_id, "Error: No password supplied!\n");
		} else if (!challenge.data) {	
			mux_printf(mux_id, "Error: No lanman-challenge supplied!\n");
		} else {
			char *error_string = NULL;
			DATA_BLOB lm_key;
			DATA_BLOB user_session_key;
			uint32_t flags = 0;

			if (full_username && !username) {
				SAFE_FREE(username);
				SAFE_FREE(domain);
				if (!parse_ntlm_auth_domain_user(full_username, &username, 
												 &domain, 
												 *lpcfg_winbind_separator(lp_ctx))) {
					/* username might be 'tainted', don't print into our new-line deleimianted stream */
					mux_printf(mux_id, "Error: Could not parse into domain and username\n");
				}
			}

			if (!domain) {
				domain = smb_xstrdup(lpcfg_workgroup(lp_ctx));
			}

			if (ntlm_server_1_lm_session_key) 
				flags |= NTLM_AUTH_FLAG_LMKEY;
			
			if (ntlm_server_1_user_session_key) 
				flags |= NTLM_AUTH_FLAG_USER_SESSION_KEY;

			if (!NT_STATUS_IS_OK(
				    local_pw_check_specified(lp_ctx,
							     username, 
							      domain, 
							      lpcfg_netbios_name(lp_ctx),
							      &challenge, 
							      &lm_response, 
							      &nt_response, 
							      flags, 
							      &lm_key, 
							      &user_session_key,
							      &error_string,
							      NULL))) {

				mux_printf(mux_id, "Authenticated: No\n");
				mux_printf(mux_id, "Authentication-Error: %s\n.\n", error_string);
				SAFE_FREE(error_string);
			} else {
				static char zeros[16];
				char *hex_lm_key;
				char *hex_user_session_key;

				mux_printf(mux_id, "Authenticated: Yes\n");

				if (ntlm_server_1_lm_session_key 
				    && lm_key.length 
				    && (memcmp(zeros, lm_key.data, 
								lm_key.length) != 0)) {
					hex_encode(lm_key.data,
						   lm_key.length,
						   &hex_lm_key);
					mux_printf(mux_id, "LANMAN-Session-Key: %s\n", hex_lm_key);
					SAFE_FREE(hex_lm_key);
				}

				if (ntlm_server_1_user_session_key 
				    && user_session_key.length 
				    && (memcmp(zeros, user_session_key.data, 
					       user_session_key.length) != 0)) {
					hex_encode(user_session_key.data, 
						   user_session_key.length, 
						   &hex_user_session_key);
					mux_printf(mux_id, "User-Session-Key: %s\n", hex_user_session_key);
					SAFE_FREE(hex_user_session_key);
				}
			}
		}
		/* clear out the state */
		challenge = data_blob(NULL, 0);
		nt_response = data_blob(NULL, 0);
		lm_response = data_blob(NULL, 0);
		SAFE_FREE(full_username);
		SAFE_FREE(username);
		SAFE_FREE(domain);
		SAFE_FREE(plaintext_password);
		ntlm_server_1_user_session_key = false;
		ntlm_server_1_lm_session_key = false;
		mux_printf(mux_id, ".\n");

		return;
	}

	request = buf;

	/* Indicates a base64 encoded structure */
	parameter = strstr(request, ":: ");
	if (!parameter) {
		parameter = strstr(request, ": ");
		
		if (!parameter) {
			DEBUG(0, ("Parameter not found!\n"));
			mux_printf(mux_id, "Error: Parameter not found!\n.\n");
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
			mux_printf(mux_id, "Error: hex decode of %s failed! (got %d bytes, expected 8)\n.\n", 
				  parameter,
				  (int)challenge.length);
			challenge = data_blob(NULL, 0);
		}
	} else if (strequal(request, "NT-Response")) {
		nt_response = strhex_to_data_blob(NULL, parameter);
		if (nt_response.length < 24) {
			mux_printf(mux_id, "Error: hex decode of %s failed! (only got %d bytes, needed at least 24)\n.\n", 
				  parameter,
				  (int)nt_response.length);
			nt_response = data_blob(NULL, 0);
		}
	} else if (strequal(request, "LANMAN-Response")) {
		lm_response = strhex_to_data_blob(NULL, parameter);
		if (lm_response.length != 24) {
			mux_printf(mux_id, "Error: hex decode of %s failed! (got %d bytes, expected 24)\n.\n", 
				  parameter,
				  (int)lm_response.length);
			lm_response = data_blob(NULL, 0);
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
		mux_printf(mux_id, "Error: Unknown request %s\n.\n", request);
	}
}

static void manage_squid_request(struct loadparm_context *lp_ctx, enum stdio_helper_mode helper_mode,
				 stdio_helper_function fn, void **private2) 
{
	char *buf;
	char tmp[INITIAL_BUFFER_SIZE+1];
	unsigned int mux_id = 0;
	int length, buf_size = 0;
	char *c;
	struct mux_private {
		unsigned int max_mux;
		void **private_pointers;
	};

	static struct mux_private *mux_private;
	static void *normal_private;
	void **private1;

	buf = talloc_strdup(NULL, "");

	if (buf == NULL) {
		DEBUG(0, ("Failed to allocate memory for reading the input "
			  "buffer.\n"));
		x_fprintf(x_stdout, "ERR\n");
		return;
	}

	do {
		/* this is not a typo - x_fgets doesn't work too well under
		 * squid */
		if (fgets(tmp, INITIAL_BUFFER_SIZE, stdin) == NULL) {
			if (ferror(stdin)) {
				DEBUG(1, ("fgets() failed! dying..... errno=%d "
					  "(%s)\n", ferror(stdin),
					  strerror(ferror(stdin))));

				exit(1);    /* BIIG buffer */
			}
			exit(0);
		}

		buf = talloc_strdup_append_buffer(buf, tmp);
		buf_size += INITIAL_BUFFER_SIZE;

		if (buf_size > MAX_BUFFER_SIZE) {
			DEBUG(0, ("Invalid Request (too large)\n"));
			x_fprintf(x_stdout, "ERR\n");
			talloc_free(buf);
			return;
		}

		c = strchr(buf, '\n');
	} while (c == NULL);

	*c = '\0';
	length = c-buf;

	DEBUG(10, ("Got '%s' from squid (length: %d).\n",buf,length));

	if (buf[0] == '\0') {
		DEBUG(0, ("Invalid Request (empty)\n"));
		x_fprintf(x_stdout, "ERR\n");
		talloc_free(buf);
		return;
	}

	if (opt_multiplex) {
		if (sscanf(buf, "%u ", &mux_id) != 1) {
			DEBUG(0, ("Invalid Request - no multiplex id\n"));
			x_fprintf(x_stdout, "ERR\n");
			talloc_free(buf);
			return;
		}
		if (!mux_private) {
			mux_private = talloc(NULL, struct mux_private);
			mux_private->max_mux = 0;
			mux_private->private_pointers = NULL;
		}
		
		c=strchr(buf,' ');
		if (!c) {
			DEBUG(0, ("Invalid Request - no data after multiplex id\n"));
			x_fprintf(x_stdout, "ERR\n");
			talloc_free(buf);
			return;
		}
		c++;
		if (mux_id >= mux_private->max_mux) {
			unsigned int prev_max = mux_private->max_mux;
			mux_private->max_mux = mux_id + 1;
			mux_private->private_pointers
				= talloc_realloc(mux_private, 
						   mux_private->private_pointers, 
						   void *, mux_private->max_mux);
			memset(&mux_private->private_pointers[prev_max], '\0',  
			       (sizeof(*mux_private->private_pointers) * (mux_private->max_mux - prev_max))); 
		};

		private1 = &mux_private->private_pointers[mux_id];
	} else {
		c = buf;
		private1 = &normal_private;
	}

	fn(helper_mode, lp_ctx, c, length, private1, mux_id, private2);
	talloc_free(buf);
}

static void squid_stream(struct loadparm_context *lp_ctx,
			 enum stdio_helper_mode stdio_mode, 
			 stdio_helper_function fn) {
	/* initialize FDescs */
	x_setbuf(x_stdout, NULL);
	x_setbuf(x_stderr, NULL);
	while(1) {
		manage_squid_request(lp_ctx, stdio_mode, fn, NULL);
	}
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
	OPT_MULTIPLEX,
	OPT_USE_CACHED_CREDS,
};

int main(int argc, const char **argv)
{
	static const char *helper_protocol;
	int opt;

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
 		{ "domain", 0, POPT_ARG_STRING, &opt_domain, OPT_DOMAIN, "domain name"},
 		{ "workstation", 0, POPT_ARG_STRING, &opt_workstation, OPT_WORKSTATION, "workstation"},
		{ "username", 0, POPT_ARG_STRING, &opt_username, OPT_PASSWORD, "Username"},		
		{ "password", 0, POPT_ARG_STRING, &opt_password, OPT_PASSWORD, "User's plaintext password"},		
		{ "multiplex", 0, POPT_ARG_NONE, &opt_multiplex, OPT_MULTIPLEX, "Multiplex Mode"},
		{ "use-cached-creds", 0, POPT_ARG_NONE, &use_cached_creds, OPT_USE_CACHED_CREDS, "silently ignored for compatibility reasons"},
		POPT_COMMON_SAMBA
		POPT_COMMON_VERSION
		{ NULL }
	};

	/* Samba client initialisation */

	setup_logging(NULL, DEBUG_STDERR);

	/* Parse options */

	pc = poptGetContext("ntlm_auth", argc, argv, long_options, 0);

	/* Parse command line options */

	if (argc == 1) {
		poptPrintHelp(pc, stderr, 0);
		return 1;
	}

	pc = poptGetContext(NULL, argc, (const char **)argv, long_options, 
			    POPT_CONTEXT_KEEP_FIRST);

	while((opt = poptGetNextOpt(pc)) != -1) {
		if (opt < -1) {
			break;
		}
	}
	if (opt < -1) {
		fprintf(stderr, "%s: %s\n",
			poptBadOption(pc, POPT_BADOPTION_NOALIAS),
			poptStrerror(opt));
		return 1;
	}

	gensec_init(cmdline_lp_ctx);

	if (opt_domain == NULL) {
		opt_domain = lpcfg_workgroup(cmdline_lp_ctx);
	}

	if (helper_protocol) {
		int i;
		for (i=0; i<NUM_HELPER_MODES; i++) {
			if (strcmp(helper_protocol, stdio_helper_protocols[i].name) == 0) {
				squid_stream(cmdline_lp_ctx, stdio_helper_protocols[i].mode, stdio_helper_protocols[i].fn);
				exit(0);
			}
		}
		x_fprintf(x_stderr, "unknown helper protocol [%s]\n\nValid helper protools:\n\n", helper_protocol);

		for (i=0; i<NUM_HELPER_MODES; i++) {
			x_fprintf(x_stderr, "%s\n", stdio_helper_protocols[i].name);
		}

		exit(1);
	}

	if (!opt_username) {
		x_fprintf(x_stderr, "username must be specified!\n\n");
		poptPrintHelp(pc, stderr, 0);
		exit(1);
	}

	if (opt_workstation == NULL) {
		opt_workstation = lpcfg_netbios_name(cmdline_lp_ctx);
	}

	if (!opt_password) {
		opt_password = getpass("password: ");
	}

	{
		char *user;

		if (asprintf(&user, "%s%c%s", opt_domain,
		             *lpcfg_winbind_separator(cmdline_lp_ctx),
			     opt_username) < 0) {
			return 1;
		}
		if (!check_plaintext_auth(user, opt_password, true)) {
			return 1;
		}
	}

	/* Exit code */

	poptFreeContext(pc);
	return 0;
}
