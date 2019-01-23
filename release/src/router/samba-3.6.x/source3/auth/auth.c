/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Bartlett         2001-2002

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
#include "auth.h"
#include "smbd/globals.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

static_decl_auth;

static struct auth_init_function_entry *auth_backends = NULL;

static struct auth_init_function_entry *auth_find_backend_entry(const char *name);

NTSTATUS smb_register_auth(int version, const char *name, auth_init_function init)
{
	struct auth_init_function_entry *entry = auth_backends;

	if (version != AUTH_INTERFACE_VERSION) {
		DEBUG(0,("Can't register auth_method!\n"
			 "You tried to register an auth module with AUTH_INTERFACE_VERSION %d, while this version of samba uses %d\n",
			 version,AUTH_INTERFACE_VERSION));
		return NT_STATUS_OBJECT_TYPE_MISMATCH;
	}

	if (!name || !init) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	DEBUG(5,("Attempting to register auth backend %s\n", name));

	if (auth_find_backend_entry(name)) {
		DEBUG(0,("There already is an auth method registered with the name %s!\n", name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	entry = SMB_XMALLOC_P(struct auth_init_function_entry);
	entry->name = smb_xstrdup(name);
	entry->init = init;

	DLIST_ADD(auth_backends, entry);
	DEBUG(5,("Successfully added auth method '%s'\n", name));
	return NT_STATUS_OK;
}

static struct auth_init_function_entry *auth_find_backend_entry(const char *name)
{
	struct auth_init_function_entry *entry = auth_backends;

	while(entry) {
		if (strcmp(entry->name, name)==0) return entry;
		entry = entry->next;
	}

	return NULL;
}

/****************************************************************************
 Try to get a challenge out of the various authentication modules.
 Returns a const char of length 8 bytes.
****************************************************************************/

static NTSTATUS get_ntlm_challenge(struct auth_context *auth_context,
			       uint8_t chal[8])
{
	DATA_BLOB challenge = data_blob_null;
	const char *challenge_set_by = NULL;
	auth_methods *auth_method;

	if (auth_context->challenge.length) {
		DEBUG(5, ("get_ntlm_challenge (auth subsystem): returning previous challenge by module %s (normal)\n", 
			  auth_context->challenge_set_by));
		memcpy(chal, auth_context->challenge.data, 8);
		return NT_STATUS_OK;
	}

	auth_context->challenge_may_be_modified = False;

	for (auth_method = auth_context->auth_method_list; auth_method; auth_method = auth_method->next) {
		if (auth_method->get_chal == NULL) {
			DEBUG(5, ("auth_get_challenge: module %s did not want to specify a challenge\n", auth_method->name));
			continue;
		}

		DEBUG(5, ("auth_get_challenge: getting challenge from module %s\n", auth_method->name));
		if (challenge_set_by != NULL) {
			DEBUG(1, ("auth_get_challenge: CONFIGURATION ERROR: authentication method %s has already specified a challenge.  Challenge by %s ignored.\n", 
				  challenge_set_by, auth_method->name));
			continue;
		}

		challenge = auth_method->get_chal(auth_context, &auth_method->private_data,
						  auth_context);
		if (!challenge.length) {
			DEBUG(3, ("auth_get_challenge: getting challenge from authentication method %s FAILED.\n", 
				  auth_method->name));
		} else {
			DEBUG(5, ("auth_get_challenge: successfully got challenge from module %s\n", auth_method->name));
			auth_context->challenge = challenge;
			challenge_set_by = auth_method->name;
			auth_context->challenge_set_method = auth_method;
		}
	}

	if (!challenge_set_by) {
		uchar tmp[8];

		generate_random_buffer(tmp, sizeof(tmp));
		auth_context->challenge = data_blob_talloc(auth_context,
							   tmp, sizeof(tmp));

		challenge_set_by = "random";
		auth_context->challenge_may_be_modified = True;
	} 

	DEBUG(5, ("auth_context challenge created by %s\n", challenge_set_by));
	DEBUG(5, ("challenge is: \n"));
	dump_data(5, auth_context->challenge.data, auth_context->challenge.length);

	SMB_ASSERT(auth_context->challenge.length == 8);

	auth_context->challenge_set_by=challenge_set_by;

	memcpy(chal, auth_context->challenge.data, 8);
	return NT_STATUS_OK;
}


/**
 * Check user is in correct domain (if required)
 *
 * @param user Only used to fill in the debug message
 * 
 * @param domain The domain to be verified
 *
 * @return True if the user can connect with that domain, 
 *         False otherwise.
**/

static bool check_domain_match(const char *user, const char *domain) 
{
	/*
	 * If we aren't serving to trusted domains, we must make sure that
	 * the validation request comes from an account in the same domain
	 * as the Samba server
	 */

	if (!lp_allow_trusted_domains() &&
	    !(strequal("", domain) || 
	      strequal(lp_workgroup(), domain) || 
	      is_myname(domain))) {
		DEBUG(1, ("check_domain_match: Attempt to connect as user %s from domain %s denied.\n", user, domain));
		return False;
	} else {
		return True;
	}
}

/**
 * Check a user's Plaintext, LM or NTLM password.
 *
 * Check a user's password, as given in the user_info struct and return various
 * interesting details in the server_info struct.
 *
 * This function does NOT need to be in a become_root()/unbecome_root() pair
 * as it makes the calls itself when needed.
 *
 * The return value takes precedence over the contents of the server_info 
 * struct.  When the return is other than NT_STATUS_OK the contents 
 * of that structure is undefined.
 *
 * @param user_info Contains the user supplied components, including the passwords.
 *                  Must be created with make_user_info() or one of its wrappers.
 *
 * @param auth_context Supplies the challenges and some other data. 
 *                  Must be created with make_auth_context(), and the challenges should be 
 *                  filled in, either at creation or by calling the challenge geneation 
 *                  function auth_get_challenge().  
 *
 * @param server_info If successful, contains information about the authentication, 
 *                    including a struct samu struct describing the user.
 *
 * @return An NTSTATUS with NT_STATUS_OK or an appropriate error.
 *
 **/

static NTSTATUS check_ntlm_password(const struct auth_context *auth_context,
				    const struct auth_usersupplied_info *user_info, 
				    struct auth_serversupplied_info **server_info)
{
	/* if all the modules say 'not for me' this is reasonable */
	NTSTATUS nt_status = NT_STATUS_NO_SUCH_USER;
	const char *unix_username;
	auth_methods *auth_method;
	TALLOC_CTX *mem_ctx;

	if (!user_info || !auth_context || !server_info)
		return NT_STATUS_LOGON_FAILURE;

	DEBUG(3, ("check_ntlm_password:  Checking password for unmapped user [%s]\\[%s]@[%s] with the new password interface\n", 
		  user_info->client.domain_name, user_info->client.account_name, user_info->workstation_name));

	DEBUG(3, ("check_ntlm_password:  mapped user is: [%s]\\[%s]@[%s]\n", 
		  user_info->mapped.domain_name, user_info->mapped.account_name, user_info->workstation_name));

	if (auth_context->challenge.length != 8) {
		DEBUG(0, ("check_ntlm_password:  Invalid challenge stored for this auth context - cannot continue\n"));
		return NT_STATUS_LOGON_FAILURE;
	}

	if (auth_context->challenge_set_by)
		DEBUG(10, ("check_ntlm_password: auth_context challenge created by %s\n",
					auth_context->challenge_set_by));

	DEBUG(10, ("challenge is: \n"));
	dump_data(5, auth_context->challenge.data, auth_context->challenge.length);

#ifdef DEBUG_PASSWORD
	DEBUG(100, ("user_info has passwords of length %d and %d\n", 
		    (int)user_info->password.response.lanman.length, (int)user_info->password.response.nt.length));
	DEBUG(100, ("lm:\n"));
	dump_data(100, user_info->password.response.lanman.data, user_info->password.response.lanman.length);
	DEBUG(100, ("nt:\n"));
	dump_data(100, user_info->password.response.nt.data, user_info->password.response.nt.length);
#endif

	/* This needs to be sorted:  If it doesn't match, what should we do? */
	if (!check_domain_match(user_info->client.account_name, user_info->mapped.domain_name))
		return NT_STATUS_LOGON_FAILURE;

	for (auth_method = auth_context->auth_method_list;auth_method; auth_method = auth_method->next) {
		NTSTATUS result;

		mem_ctx = talloc_init("%s authentication for user %s\\%s", auth_method->name,
				      user_info->mapped.domain_name, user_info->client.account_name);

		result = auth_method->auth(auth_context, auth_method->private_data, mem_ctx, user_info, server_info);

		/* check if the module did anything */
		if ( NT_STATUS_V(result) == NT_STATUS_V(NT_STATUS_NOT_IMPLEMENTED) ) {
			DEBUG(10,("check_ntlm_password: %s had nothing to say\n", auth_method->name));
			talloc_destroy(mem_ctx);
			continue;
		}

		nt_status = result;

		if (NT_STATUS_IS_OK(nt_status)) {
			DEBUG(3, ("check_ntlm_password: %s authentication for user [%s] succeeded\n", 
				  auth_method->name, user_info->client.account_name));
		} else {
			DEBUG(5, ("check_ntlm_password: %s authentication for user [%s] FAILED with error %s\n", 
				  auth_method->name, user_info->client.account_name, nt_errstr(nt_status)));
		}

		talloc_destroy(mem_ctx);

		if ( NT_STATUS_IS_OK(nt_status))
		{
				break;			
		}
	}

	/* successful authentication */

	if (NT_STATUS_IS_OK(nt_status)) {
		unix_username = (*server_info)->unix_name;
		if (!(*server_info)->guest) {
			/* We might not be root if we are an RPC call */
			become_root();
			nt_status = smb_pam_accountcheck(
				unix_username,
				smbd_server_conn->client_id.name);
			unbecome_root();

			if (NT_STATUS_IS_OK(nt_status)) {
				DEBUG(5, ("check_ntlm_password:  PAM Account for user [%s] succeeded\n", 
					  unix_username));
			} else {
				DEBUG(3, ("check_ntlm_password:  PAM Account for user [%s] FAILED with error %s\n", 
					  unix_username, nt_errstr(nt_status)));
			} 
		}

		if (NT_STATUS_IS_OK(nt_status)) {
			DEBUG((*server_info)->guest ? 5 : 2, 
			      ("check_ntlm_password:  %sauthentication for user [%s] -> [%s] -> [%s] succeeded\n",
			       (*server_info)->guest ? "guest " : "",
			       user_info->client.account_name,
			       user_info->mapped.account_name,
			       unix_username));
		}

		return nt_status;
	}

	/* failed authentication; check for guest lapping */

	DEBUG(2, ("check_ntlm_password:  Authentication for user [%s] -> [%s] FAILED with error %s\n",
		  user_info->client.account_name, user_info->mapped.account_name,
		  nt_errstr(nt_status)));
	ZERO_STRUCTP(server_info);

	return nt_status;
}

/***************************************************************************
 Clear out a auth_context, and destroy the attached TALLOC_CTX
***************************************************************************/

static int auth_context_destructor(void *ptr)
{
	struct auth_context *ctx = talloc_get_type(ptr, struct auth_context);
	struct auth_methods *am;


	/* Free private data of context's authentication methods */
	for (am = ctx->auth_method_list; am; am = am->next) {
		TALLOC_FREE(am->private_data);
	}

	return 0;
}

/***************************************************************************
 Make a auth_info struct
***************************************************************************/

static NTSTATUS make_auth_context(TALLOC_CTX *mem_ctx,
				  struct auth_context **auth_context)
{
	struct auth_context *ctx;

	ctx = talloc_zero(mem_ctx, struct auth_context);
	if (!ctx) {
		DEBUG(0,("make_auth_context: talloc failed!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ctx->check_ntlm_password = check_ntlm_password;
	ctx->get_ntlm_challenge = get_ntlm_challenge;

	talloc_set_destructor((TALLOC_CTX *)ctx, auth_context_destructor);

	*auth_context = ctx;
	return NT_STATUS_OK;
}

bool load_auth_module(struct auth_context *auth_context, 
		      const char *module, auth_methods **ret) 
{
	static bool initialised_static_modules = False;

	struct auth_init_function_entry *entry;
	char *module_name = smb_xstrdup(module);
	char *module_params = NULL;
	char *p;
	bool good = False;

	/* Initialise static modules if not done so yet */
	if(!initialised_static_modules) {
		static_init_auth;
		initialised_static_modules = True;
	}

	DEBUG(5,("load_auth_module: Attempting to find an auth method to match %s\n",
		 module));

	p = strchr(module_name, ':');
	if (p) {
		*p = 0;
		module_params = p+1;
		trim_char(module_params, ' ', ' ');
	}

	trim_char(module_name, ' ', ' ');

	entry = auth_find_backend_entry(module_name);

	if (entry == NULL) {
		if (NT_STATUS_IS_OK(smb_probe_module("auth", module_name))) {
			entry = auth_find_backend_entry(module_name);
		}
	}

	if (entry != NULL) {
		if (!NT_STATUS_IS_OK(entry->init(auth_context, module_params, ret))) {
			DEBUG(0,("load_auth_module: auth method %s did not correctly init\n",
				 module_name));
		} else {
			DEBUG(5,("load_auth_module: auth method %s has a valid init\n",
				 module_name));
			good = True;
		}
	} else {
		DEBUG(0,("load_auth_module: can't find auth method %s!\n", module_name));
	}

	SAFE_FREE(module_name);
	return good;
}

/***************************************************************************
 Make a auth_info struct for the auth subsystem
***************************************************************************/

static NTSTATUS make_auth_context_text_list(TALLOC_CTX *mem_ctx,
					    struct auth_context **auth_context,
					    char **text_list)
{
	auth_methods *list = NULL;
	auth_methods *t = NULL;
	NTSTATUS nt_status;

	if (!text_list) {
		DEBUG(2,("make_auth_context_text_list: No auth method list!?\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	nt_status = make_auth_context(mem_ctx, auth_context);

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	for (;*text_list; text_list++) { 
		if (load_auth_module(*auth_context, *text_list, &t)) {
		    DLIST_ADD_END(list, t, auth_methods *);
		}
	}

	(*auth_context)->auth_method_list = list;

	return nt_status;
}

/***************************************************************************
 Make a auth_context struct for the auth subsystem
***************************************************************************/

NTSTATUS make_auth_context_subsystem(TALLOC_CTX *mem_ctx,
				     struct auth_context **auth_context)
{
	char **auth_method_list = NULL; 
	NTSTATUS nt_status;

	if (lp_auth_methods()
	    && !(auth_method_list = str_list_copy(talloc_tos(), 
			      lp_auth_methods()))) {
		return NT_STATUS_NO_MEMORY;
	}

	if (auth_method_list == NULL) {
		switch (lp_security()) 
		{
		case SEC_DOMAIN:
			DEBUG(5,("Making default auth method list for security=domain\n"));
			auth_method_list = str_list_make_v3(
				talloc_tos(), "guest sam winbind:ntdomain",
				NULL);
			break;
		case SEC_SERVER:
			DEBUG(5,("Making default auth method list for security=server\n"));
			auth_method_list = str_list_make_v3(
				talloc_tos(), "guest sam smbserver",
				NULL);
			break;
		case SEC_USER:
			if (lp_encrypted_passwords()) {	
				if ((lp_server_role() == ROLE_DOMAIN_PDC) || (lp_server_role() == ROLE_DOMAIN_BDC)) {
					DEBUG(5,("Making default auth method list for DC, security=user, encrypt passwords = yes\n"));
					auth_method_list = str_list_make_v3(
						talloc_tos(),
						"guest sam winbind:trustdomain",
						NULL);
				} else {
					DEBUG(5,("Making default auth method list for standalone security=user, encrypt passwords = yes\n"));
					auth_method_list = str_list_make_v3(
						talloc_tos(), "guest sam",
						NULL);
				}
			} else {
				DEBUG(5,("Making default auth method list for security=user, encrypt passwords = no\n"));
				auth_method_list = str_list_make_v3(
					talloc_tos(), "guest unix", NULL);
			}
			break;
		case SEC_SHARE:
			if (lp_encrypted_passwords()) {
				DEBUG(5,("Making default auth method list for security=share, encrypt passwords = yes\n"));
				auth_method_list = str_list_make_v3(
					talloc_tos(), "guest sam", NULL);
			} else {
				DEBUG(5,("Making default auth method list for security=share, encrypt passwords = no\n"));
				auth_method_list = str_list_make_v3(
					talloc_tos(), "guest unix", NULL);
			}
			break;
		case SEC_ADS:
			DEBUG(5,("Making default auth method list for security=ADS\n"));
			auth_method_list = str_list_make_v3(
				talloc_tos(), "guest sam winbind:ntdomain",
				NULL);
			break;
		default:
			DEBUG(5,("Unknown auth method!\n"));
			return NT_STATUS_UNSUCCESSFUL;
		}
	} else {
		DEBUG(5,("Using specified auth order\n"));
	}

	nt_status = make_auth_context_text_list(mem_ctx, auth_context,
						auth_method_list);

	TALLOC_FREE(auth_method_list);
	return nt_status;
}

/***************************************************************************
 Make a auth_info struct with a fixed challenge
***************************************************************************/

NTSTATUS make_auth_context_fixed(TALLOC_CTX *mem_ctx,
				 struct auth_context **auth_context,
				 uchar chal[8])
{
	NTSTATUS nt_status;
	nt_status = make_auth_context_subsystem(mem_ctx, auth_context);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	(*auth_context)->challenge = data_blob_talloc(*auth_context, chal, 8);
	(*auth_context)->challenge_set_by = "fixed";
	return nt_status;
}


