/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Bartlett         2001-2002
   Copyright (C) Stefan Metzmacher       2005
   
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
#include "../lib/util/dlinklist.h"
#include "auth/auth.h"
#include "auth/ntlm/auth_proto.h"
#include "lib/events/events.h"
#include "param/param.h"

/***************************************************************************
 Set a fixed challenge
***************************************************************************/
_PUBLIC_ NTSTATUS auth_context_set_challenge(struct auth_context *auth_ctx, const uint8_t chal[8], const char *set_by) 
{
	auth_ctx->challenge.set_by = talloc_strdup(auth_ctx, set_by);
	NT_STATUS_HAVE_NO_MEMORY(auth_ctx->challenge.set_by);

	auth_ctx->challenge.data = data_blob_talloc(auth_ctx, chal, 8);
	NT_STATUS_HAVE_NO_MEMORY(auth_ctx->challenge.data.data);

	return NT_STATUS_OK;
}

/***************************************************************************
 Set a fixed challenge
***************************************************************************/
bool auth_challenge_may_be_modified(struct auth_context *auth_ctx) 
{
	return auth_ctx->challenge.may_be_modified;
}

/****************************************************************************
 Try to get a challenge out of the various authentication modules.
 Returns a const char of length 8 bytes.
****************************************************************************/
_PUBLIC_ NTSTATUS auth_get_challenge(struct auth_context *auth_ctx, const uint8_t **_chal)
{
	NTSTATUS nt_status;
	struct auth_method_context *method;

	if (auth_ctx->challenge.data.length) {
		DEBUG(5, ("auth_get_challenge: returning previous challenge by module %s (normal)\n", 
			  auth_ctx->challenge.set_by));
		*_chal = auth_ctx->challenge.data.data;
		return NT_STATUS_OK;
	}

	for (method = auth_ctx->methods; method; method = method->next) {
		DATA_BLOB challenge = data_blob(NULL,0);

		nt_status = method->ops->get_challenge(method, auth_ctx, &challenge);
		if (NT_STATUS_EQUAL(nt_status, NT_STATUS_NOT_IMPLEMENTED)) {
			continue;
		}

		NT_STATUS_NOT_OK_RETURN(nt_status);

		if (challenge.length != 8) {
			DEBUG(0, ("auth_get_challenge: invalid challenge (length %u) by mothod [%s]\n",
				(unsigned)challenge.length, method->ops->name));
			return NT_STATUS_INTERNAL_ERROR;
		}

		auth_ctx->challenge.data	= challenge;
		auth_ctx->challenge.set_by	= method->ops->name;

		break;
	}

	if (!auth_ctx->challenge.set_by) {
		uint8_t chal[8];
		generate_random_buffer(chal, 8);

		auth_ctx->challenge.data		= data_blob_talloc(auth_ctx, chal, 8);
		NT_STATUS_HAVE_NO_MEMORY(auth_ctx->challenge.data.data);
		auth_ctx->challenge.set_by		= "random";

		auth_ctx->challenge.may_be_modified	= true;
	}

	DEBUG(10,("auth_get_challenge: challenge set by %s\n",
		 auth_ctx->challenge.set_by));

	*_chal = auth_ctx->challenge.data.data;
	return NT_STATUS_OK;
}

/****************************************************************************
 Try to get a challenge out of the various authentication modules.
 Returns a const char of length 8 bytes.
****************************************************************************/
_PUBLIC_ NTSTATUS auth_get_server_info_principal(TALLOC_CTX *mem_ctx, 
						  struct auth_context *auth_ctx,
						  const char *principal,
						  struct auth_serversupplied_info **server_info)
{
	NTSTATUS nt_status;
	struct auth_method_context *method;

	for (method = auth_ctx->methods; method; method = method->next) {
		if (!method->ops->get_server_info_principal) {
			continue;
		}

		nt_status = method->ops->get_server_info_principal(mem_ctx, auth_ctx, principal, server_info);
		if (NT_STATUS_EQUAL(nt_status, NT_STATUS_NOT_IMPLEMENTED)) {
			continue;
		}

		NT_STATUS_NOT_OK_RETURN(nt_status);

		break;
	}

	return NT_STATUS_OK;
}

struct auth_check_password_sync_state {
	bool finished;
	NTSTATUS status;
	struct auth_serversupplied_info *server_info;
};

static void auth_check_password_sync_callback(struct auth_check_password_request *req,
					      void *private_data)
{
	struct auth_check_password_sync_state *s = talloc_get_type(private_data,
						   struct auth_check_password_sync_state);

	s->finished = true;
	s->status = auth_check_password_recv(req, s, &s->server_info);
}

/**
 * Check a user's Plaintext, LM or NTLM password.
 * (sync version)
 *
 * Check a user's password, as given in the user_info struct and return various
 * interesting details in the server_info struct.
 *
 * The return value takes precedence over the contents of the server_info 
 * struct.  When the return is other than NT_STATUS_OK the contents 
 * of that structure is undefined.
 *
 * @param auth_ctx Supplies the challenges and some other data. 
 *                  Must be created with auth_context_create(), and the challenges should be 
 *                  filled in, either at creation or by calling the challenge geneation 
 *                  function auth_get_challenge().  
 *
 * @param user_info Contains the user supplied components, including the passwords.
 *
 * @param mem_ctx The parent memory context for the server_info structure
 *
 * @param server_info If successful, contains information about the authentication, 
 *                    including a SAM_ACCOUNT struct describing the user.
 *
 * @return An NTSTATUS with NT_STATUS_OK or an appropriate error.
 *
 **/

_PUBLIC_ NTSTATUS auth_check_password(struct auth_context *auth_ctx,
			     TALLOC_CTX *mem_ctx,
			     const struct auth_usersupplied_info *user_info, 
			     struct auth_serversupplied_info **server_info)
{
	struct auth_check_password_sync_state *sync_state;
	NTSTATUS status;

	sync_state = talloc_zero(auth_ctx, struct auth_check_password_sync_state);
	NT_STATUS_HAVE_NO_MEMORY(sync_state);

	auth_check_password_send(auth_ctx, user_info, auth_check_password_sync_callback, sync_state);

	while (!sync_state->finished) {
		event_loop_once(auth_ctx->event_ctx);
	}

	status = sync_state->status;

	if (NT_STATUS_IS_OK(status)) {
		*server_info = talloc_steal(mem_ctx, sync_state->server_info);
	}

	talloc_free(sync_state);
	return status;
}

struct auth_check_password_request {
	struct auth_context *auth_ctx;
	const struct auth_usersupplied_info *user_info;
	struct auth_serversupplied_info *server_info;
	struct auth_method_context *method;
	NTSTATUS status;
	struct {
		void (*fn)(struct auth_check_password_request *req, void *private_data);
		void *private_data;
	} callback;
};

static void auth_check_password_async_timed_handler(struct tevent_context *ev, struct tevent_timer *te,
						    struct timeval t, void *ptr)
{
	struct auth_check_password_request *req = talloc_get_type(ptr, struct auth_check_password_request);
	req->status = req->method->ops->check_password(req->method, req, req->user_info, &req->server_info);
	req->callback.fn(req, req->callback.private_data);
}

/**
 * Check a user's Plaintext, LM or NTLM password.
 * async send hook
 *
 * Check a user's password, as given in the user_info struct and return various
 * interesting details in the server_info struct.
 *
 * The return value takes precedence over the contents of the server_info 
 * struct.  When the return is other than NT_STATUS_OK the contents 
 * of that structure is undefined.
 *
 * @param auth_ctx Supplies the challenges and some other data. 
 *                  Must be created with make_auth_context(), and the challenges should be 
 *                  filled in, either at creation or by calling the challenge geneation 
 *                  function auth_get_challenge().  
 *
 * @param user_info Contains the user supplied components, including the passwords.
 *
 * @param callback A callback function which will be called when the operation is finished.
 *                 The callback function needs to call auth_check_password_recv() to get the return values
 *
 * @param private_data A private pointer which will ba passed to the callback function
 *
 **/

_PUBLIC_ void auth_check_password_send(struct auth_context *auth_ctx,
			      const struct auth_usersupplied_info *user_info,
			      void (*callback)(struct auth_check_password_request *req, void *private_data),
			      void *private_data)
{
	/* if all the modules say 'not for me' this is reasonable */
	NTSTATUS nt_status;
	struct auth_method_context *method;
	const uint8_t *challenge;
	struct auth_usersupplied_info *user_info_tmp;
	struct auth_check_password_request *req = NULL;

	DEBUG(3,   ("auth_check_password_send:  Checking password for unmapped user [%s]\\[%s]@[%s]\n", 
		    user_info->client.domain_name, user_info->client.account_name, user_info->workstation_name));

	req = talloc_zero(auth_ctx, struct auth_check_password_request);
	if (!req) {
		callback(NULL, private_data);
		return;
	}
	req->auth_ctx			= auth_ctx;
	req->user_info			= user_info;
	req->callback.fn		= callback;
	req->callback.private_data	= private_data;

	if (!user_info->mapped_state) {
		nt_status = map_user_info(req, lp_workgroup(auth_ctx->lp_ctx), user_info, &user_info_tmp);
		if (!NT_STATUS_IS_OK(nt_status)) goto failed;
		user_info = user_info_tmp;
		req->user_info	= user_info_tmp;
	}

	DEBUGADD(3,("auth_check_password_send:  mapped user is: [%s]\\[%s]@[%s]\n", 
		    user_info->mapped.domain_name, user_info->mapped.account_name, user_info->workstation_name));

	nt_status = auth_get_challenge(auth_ctx, &challenge);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("auth_check_password_send:  Invalid challenge (length %u) stored for this auth context set_by %s - cannot continue: %s\n",
			(unsigned)auth_ctx->challenge.data.length, auth_ctx->challenge.set_by, nt_errstr(nt_status)));
		goto failed;
	}

	if (auth_ctx->challenge.set_by) {
		DEBUG(10, ("auth_check_password_send: auth_context challenge created by %s\n",
					auth_ctx->challenge.set_by));
	}

	DEBUG(10, ("auth_check_password_send: challenge is: \n"));
	dump_data(5, auth_ctx->challenge.data.data, auth_ctx->challenge.data.length);

	nt_status = NT_STATUS_NO_SUCH_USER; /* If all the modules say 'not for me', then this is reasonable */
	for (method = auth_ctx->methods; method; method = method->next) {
		NTSTATUS result;
		struct tevent_timer *te = NULL;

		/* check if the module wants to chek the password */
		result = method->ops->want_check(method, req, user_info);
		if (NT_STATUS_EQUAL(result, NT_STATUS_NOT_IMPLEMENTED)) {
			DEBUG(11,("auth_check_password_send: %s had nothing to say\n", method->ops->name));
			continue;
		}

		nt_status = result;
		req->method	= method;

		if (!NT_STATUS_IS_OK(nt_status)) break;

		te = event_add_timed(auth_ctx->event_ctx, req,
				     timeval_zero(),
				     auth_check_password_async_timed_handler, req);
		if (!te) {
			nt_status = NT_STATUS_NO_MEMORY;
			goto failed;
		}
		return;
	}

failed:
	req->status = nt_status;
	req->callback.fn(req, req->callback.private_data);
}

/**
 * Check a user's Plaintext, LM or NTLM password.
 * async receive function
 *
 * The return value takes precedence over the contents of the server_info 
 * struct.  When the return is other than NT_STATUS_OK the contents 
 * of that structure is undefined.
 *
 *
 * @param req The async auth_check_password state, passes to the callers callback function
 *
 * @param mem_ctx The parent memory context for the server_info structure
 *
 * @param server_info If successful, contains information about the authentication, 
 *                    including a SAM_ACCOUNT struct describing the user.
 *
 * @return An NTSTATUS with NT_STATUS_OK or an appropriate error.
 *
 **/

_PUBLIC_ NTSTATUS auth_check_password_recv(struct auth_check_password_request *req,
				  TALLOC_CTX *mem_ctx,
				  struct auth_serversupplied_info **server_info)
{
	NTSTATUS status;

	NT_STATUS_HAVE_NO_MEMORY(req);

	if (NT_STATUS_IS_OK(req->status)) {
		DEBUG(5,("auth_check_password_recv: %s authentication for user [%s\\%s] succeeded\n",
			 req->method->ops->name, req->server_info->domain_name, req->server_info->account_name));

		*server_info = talloc_steal(mem_ctx, req->server_info);
	} else {
		DEBUG(2,("auth_check_password_recv: %s authentication for user [%s\\%s] FAILED with error %s\n", 
			 (req->method ? req->method->ops->name : "NO_METHOD"),
			 req->user_info->mapped.domain_name,
			 req->user_info->mapped.account_name, 
			 nt_errstr(req->status)));
	}

	status = req->status;
	talloc_free(req);
	return status;
}

/***************************************************************************
 Make a auth_info struct for the auth subsystem
 - Allow the caller to specify the methods to use
***************************************************************************/
_PUBLIC_ NTSTATUS auth_context_create_methods(TALLOC_CTX *mem_ctx, const char **methods, 
				     struct tevent_context *ev,
				     struct messaging_context *msg,
				     struct loadparm_context *lp_ctx,
				     struct auth_context **auth_ctx)
{
	int i;
	struct auth_context *ctx;

	auth_init();

	if (!methods) {
		DEBUG(0,("auth_context_create: No auth method list!?\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (!ev) {
		DEBUG(0,("auth_context_create: called with out event context\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (!msg) {
		DEBUG(0,("auth_context_create: called with out messaging context\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	ctx = talloc(mem_ctx, struct auth_context);
	NT_STATUS_HAVE_NO_MEMORY(ctx);
	ctx->challenge.set_by		= NULL;
	ctx->challenge.may_be_modified	= false;
	ctx->challenge.data		= data_blob(NULL, 0);
	ctx->methods			= NULL;
	ctx->event_ctx			= ev;
	ctx->msg_ctx			= msg;
	ctx->lp_ctx			= lp_ctx;

	for (i=0; methods[i] ; i++) {
		struct auth_method_context *method;

		method = talloc(ctx, struct auth_method_context);
		NT_STATUS_HAVE_NO_MEMORY(method);

		method->ops = auth_backend_byname(methods[i]);
		if (!method->ops) {
			DEBUG(1,("auth_context_create: failed to find method=%s\n",
				methods[i]));
			return NT_STATUS_INTERNAL_ERROR;
		}
		method->auth_ctx	= ctx;
		method->depth		= i;
		DLIST_ADD_END(ctx->methods, method, struct auth_method_context *);
	}

	if (!ctx->methods) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	ctx->check_password = auth_check_password;
	ctx->get_challenge = auth_get_challenge;
	ctx->set_challenge = auth_context_set_challenge;
	ctx->challenge_may_be_modified = auth_challenge_may_be_modified;
	ctx->get_server_info_principal = auth_get_server_info_principal;

	*auth_ctx = ctx;

	return NT_STATUS_OK;
}
/***************************************************************************
 Make a auth_info struct for the auth subsystem
 - Uses default auth_methods, depending on server role and smb.conf settings
***************************************************************************/
_PUBLIC_ NTSTATUS auth_context_create(TALLOC_CTX *mem_ctx, 
			     struct tevent_context *ev,
			     struct messaging_context *msg,
			     struct loadparm_context *lp_ctx,
			     struct auth_context **auth_ctx)
{
	const char **auth_methods = NULL;
	switch (lp_server_role(lp_ctx)) {
	case ROLE_STANDALONE:
		auth_methods = lp_parm_string_list(mem_ctx, lp_ctx, NULL, "auth methods", "standalone", NULL);
		break;
	case ROLE_DOMAIN_MEMBER:
		auth_methods = lp_parm_string_list(mem_ctx, lp_ctx, NULL, "auth methods", "member server", NULL);
		break;
	case ROLE_DOMAIN_CONTROLLER:
		auth_methods = lp_parm_string_list(mem_ctx, lp_ctx, NULL, "auth methods", "domain controller", NULL);
		break;
	}
	return auth_context_create_methods(mem_ctx, auth_methods, ev, msg, lp_ctx, auth_ctx);
}


/* the list of currently registered AUTH backends */
static struct auth_backend {
	const struct auth_operations *ops;
} *backends = NULL;
static int num_backends;

/*
  register a AUTH backend. 

  The 'name' can be later used by other backends to find the operations
  structure for this backend.
*/
_PUBLIC_ NTSTATUS auth_register(const struct auth_operations *ops)
{
	struct auth_operations *new_ops;
	
	if (auth_backend_byname(ops->name) != NULL) {
		/* its already registered! */
		DEBUG(0,("AUTH backend '%s' already registered\n", 
			 ops->name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	backends = talloc_realloc(talloc_autofree_context(), backends, 
				  struct auth_backend, num_backends+1);
	NT_STATUS_HAVE_NO_MEMORY(backends);

	new_ops = (struct auth_operations *)talloc_memdup(backends, ops, sizeof(*ops));
	NT_STATUS_HAVE_NO_MEMORY(new_ops);
	new_ops->name = talloc_strdup(new_ops, ops->name);
	NT_STATUS_HAVE_NO_MEMORY(new_ops->name);

	backends[num_backends].ops = new_ops;

	num_backends++;

	DEBUG(3,("AUTH backend '%s' registered\n", 
		 ops->name));

	return NT_STATUS_OK;
}

/*
  return the operations structure for a named backend of the specified type
*/
const struct auth_operations *auth_backend_byname(const char *name)
{
	int i;

	for (i=0;i<num_backends;i++) {
		if (strcmp(backends[i].ops->name, name) == 0) {
			return backends[i].ops;
		}
	}

	return NULL;
}

/*
  return the AUTH interface version, and the size of some critical types
  This can be used by backends to either detect compilation errors, or provide
  multiple implementations for different smbd compilation options in one module
*/
const struct auth_critical_sizes *auth_interface_version(void)
{
	static const struct auth_critical_sizes critical_sizes = {
		AUTH_INTERFACE_VERSION,
		sizeof(struct auth_operations),
		sizeof(struct auth_method_context),
		sizeof(struct auth_context),
		sizeof(struct auth_usersupplied_info),
		sizeof(struct auth_serversupplied_info)
	};

	return &critical_sizes;
}

_PUBLIC_ NTSTATUS auth_init(void)
{
	static bool initialized = false;
	extern NTSTATUS auth_developer_init(void);
	extern NTSTATUS auth_winbind_init(void);
	extern NTSTATUS auth_anonymous_init(void);
	extern NTSTATUS auth_unix_init(void);
	extern NTSTATUS auth_sam_init(void);
	extern NTSTATUS auth_server_init(void);

	init_module_fn static_init[] = { STATIC_auth_MODULES };
	
	if (initialized) return NT_STATUS_OK;
	initialized = true;
	
	run_init_functions(static_init);
	
	return NT_STATUS_OK;	
}

NTSTATUS server_service_auth_init(void)
{
	return auth_init();
}
