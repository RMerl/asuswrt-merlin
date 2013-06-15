/* 
   Unix SMB/CIFS implementation.

   Winbind child daemons

   Copyright (C) Andrew Tridgell 2002
   Copyright (C) Volker Lendecke 2004,2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * We fork a child per domain to be able to act non-blocking in the main
 * winbind daemon. A domain controller thousands of miles away being being
 * slow replying with a 10.000 user list should not hold up netlogon calls
 * that can be handled locally.
 */

#include "includes.h"
#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

extern BOOL override_logfile;

/* Read some data from a client connection */

static void child_read_request(struct winbindd_cli_state *state)
{
	ssize_t len;

	/* Read data */

	len = read_data(state->sock, (char *)&state->request,
			sizeof(state->request));

	if (len != sizeof(state->request)) {
		DEBUG(len > 0 ? 0 : 3, ("Got invalid request length: %d\n", (int)len));
		state->finished = True;
		return;
	}

	if (state->request.extra_len == 0) {
		state->request.extra_data.data = NULL;
		return;
	}

	DEBUG(10, ("Need to read %d extra bytes\n", (int)state->request.extra_len));

	state->request.extra_data.data =
		SMB_MALLOC_ARRAY(char, state->request.extra_len + 1);

	if (state->request.extra_data.data == NULL) {
		DEBUG(0, ("malloc failed\n"));
		state->finished = True;
		return;
	}

	/* Ensure null termination */
	state->request.extra_data.data[state->request.extra_len] = '\0';

	len = read_data(state->sock, state->request.extra_data.data,
			state->request.extra_len);

	if (len != state->request.extra_len) {
		DEBUG(0, ("Could not read extra data\n"));
		state->finished = True;
		return;
	}
}

/*
 * Machinery for async requests sent to children. You set up a
 * winbindd_request, select a child to query, and issue a async_request
 * call. When the request is completed, the callback function you specified is
 * called back with the private pointer you gave to async_request.
 */

struct winbindd_async_request {
	struct winbindd_async_request *next, *prev;
	TALLOC_CTX *mem_ctx;
	struct winbindd_child *child;
	struct winbindd_request *request;
	struct winbindd_response *response;
	void (*continuation)(void *private_data, BOOL success);
	struct timed_event *reply_timeout_event;
	pid_t child_pid; /* pid of the child we're waiting on. Used to detect
			    a restart of the child (child->pid != child_pid). */
	void *private_data;
};

static void async_main_request_sent(void *private_data, BOOL success);
static void async_request_sent(void *private_data, BOOL success);
static void async_reply_recv(void *private_data, BOOL success);
static void schedule_async_request(struct winbindd_child *child);

void async_request(TALLOC_CTX *mem_ctx, struct winbindd_child *child,
		   struct winbindd_request *request,
		   struct winbindd_response *response,
		   void (*continuation)(void *private_data, BOOL success),
		   void *private_data)
{
	struct winbindd_async_request *state;

	SMB_ASSERT(continuation != NULL);

	state = TALLOC_P(mem_ctx, struct winbindd_async_request);

	if (state == NULL) {
		DEBUG(0, ("talloc failed\n"));
		continuation(private_data, False);
		return;
	}

	state->mem_ctx = mem_ctx;
	state->child = child;
	state->request = request;
	state->response = response;
	state->continuation = continuation;
	state->private_data = private_data;

	DLIST_ADD_END(child->requests, state, struct winbindd_async_request *);

	schedule_async_request(child);

	return;
}

static void async_main_request_sent(void *private_data, BOOL success)
{
	struct winbindd_async_request *state =
		talloc_get_type_abort(private_data, struct winbindd_async_request);

	if (!success) {
		DEBUG(5, ("Could not send async request\n"));

		state->response->length = sizeof(struct winbindd_response);
		state->response->result = WINBINDD_ERROR;
		state->continuation(state->private_data, False);
		return;
	}

	if (state->request->extra_len == 0) {
		async_request_sent(private_data, True);
		return;
	}

	setup_async_write(&state->child->event, state->request->extra_data.data,
			  state->request->extra_len,
			  async_request_sent, state);
}

/****************************************************************
 Handler triggered if the child winbindd doesn't respond within
 a given timeout.
****************************************************************/

static void async_request_timeout_handler(struct event_context *ctx,
					struct timed_event *te,
					const struct timeval *now,
					void *private_data)
{
	struct winbindd_async_request *state =
		talloc_get_type_abort(private_data, struct winbindd_async_request);

	DEBUG(0,("async_request_timeout_handler: child pid %u is not responding. "
		"Closing connection to it.\n",
		state->child_pid ));

	/* Deal with the reply - set to error. */
	async_reply_recv(private_data, False);
}

/**************************************************************
 Common function called on both async send and recv fail.
 Cleans up the child and schedules the next request.
**************************************************************/

static void async_request_fail(struct winbindd_async_request *state)
{
	DLIST_REMOVE(state->child->requests, state);

	TALLOC_FREE(state->reply_timeout_event);

	SMB_ASSERT(state->child_pid != (pid_t)0);

	/* If not already reaped, send kill signal to child. */
	if (state->child->pid == state->child_pid) {
		kill(state->child_pid, SIGTERM);

		/* 
		 * Close the socket to the child.
		 */
		winbind_child_died(state->child_pid);
	}

	state->response->length = sizeof(struct winbindd_response);
	state->response->result = WINBINDD_ERROR;
	state->continuation(state->private_data, False);
}

static void async_request_sent(void *private_data_data, BOOL success)
{
	struct winbindd_async_request *state =
		talloc_get_type_abort(private_data_data, struct winbindd_async_request);

	if (!success) {
		DEBUG(5, ("Could not send async request to child pid %u\n",
			(unsigned int)state->child_pid ));
		async_request_fail(state);
		return;
	}

	/* Request successfully sent to the child, setup the wait for reply */

	setup_async_read(&state->child->event,
			 &state->response->result,
			 sizeof(state->response->result),
			 async_reply_recv, state);

	/* 
	 * Set up a timeout of 300 seconds for the response.
	 * If we don't get it close the child socket and
	 * report failure.
	 */

	state->reply_timeout_event = event_add_timed(winbind_event_context(),
							NULL,
							timeval_current_ofs(300,0),
							"async_request_timeout",
							async_request_timeout_handler,
							state);
	if (!state->reply_timeout_event) {
		smb_panic("async_request_sent: failed to add timeout handler.\n");
	}
}

static void async_reply_recv(void *private_data, BOOL success)
{
	struct winbindd_async_request *state =
		talloc_get_type_abort(private_data, struct winbindd_async_request);
	struct winbindd_child *child = state->child;

	TALLOC_FREE(state->reply_timeout_event);

	state->response->length = sizeof(struct winbindd_response);

	if (!success) {
		DEBUG(5, ("Could not receive async reply from child pid %u\n",
			(unsigned int)state->child_pid ));

		cache_cleanup_response(state->child_pid);
		async_request_fail(state);
		return;
	}

	SMB_ASSERT(cache_retrieve_response(state->child_pid,
					   state->response));

	cache_cleanup_response(state->child_pid);
	
	DLIST_REMOVE(child->requests, state);

	schedule_async_request(child);

	state->continuation(state->private_data, True);
}

static BOOL fork_domain_child(struct winbindd_child *child);

static void schedule_async_request(struct winbindd_child *child)
{
	struct winbindd_async_request *request = child->requests;

	if (request == NULL) {
		return;
	}

	if (child->event.flags != 0) {
		return;		/* Busy */
	}

	if ((child->pid == 0) && (!fork_domain_child(child))) {
		/* Cancel all outstanding requests */

		while (request != NULL) {
			/* request might be free'd in the continuation */
			struct winbindd_async_request *next = request->next;
			request->continuation(request->private_data, False);
			request = next;
		}
		return;
	}

	/* Now we know who we're sending to - remember the pid. */
	request->child_pid = child->pid;

	setup_async_write(&child->event, request->request,
			  sizeof(*request->request),
			  async_main_request_sent, request);

	return;
}

struct domain_request_state {
	TALLOC_CTX *mem_ctx;
	struct winbindd_domain *domain;
	struct winbindd_request *request;
	struct winbindd_response *response;
	void (*continuation)(void *private_data_data, BOOL success);
	void *private_data_data;
};

static void domain_init_recv(void *private_data_data, BOOL success);

void async_domain_request(TALLOC_CTX *mem_ctx,
			  struct winbindd_domain *domain,
			  struct winbindd_request *request,
			  struct winbindd_response *response,
			  void (*continuation)(void *private_data_data, BOOL success),
			  void *private_data_data)
{
	struct domain_request_state *state;

	if (domain->initialized) {
		async_request(mem_ctx, &domain->child, request, response,
			      continuation, private_data_data);
		return;
	}

	state = TALLOC_P(mem_ctx, struct domain_request_state);
	if (state == NULL) {
		DEBUG(0, ("talloc failed\n"));
		continuation(private_data_data, False);
		return;
	}

	state->mem_ctx = mem_ctx;
	state->domain = domain;
	state->request = request;
	state->response = response;
	state->continuation = continuation;
	state->private_data_data = private_data_data;

	init_child_connection(domain, domain_init_recv, state);
}

static void recvfrom_child(void *private_data_data, BOOL success)
{
	struct winbindd_cli_state *state =
		talloc_get_type_abort(private_data_data, struct winbindd_cli_state);
	enum winbindd_result result = state->response.result;

	/* This is an optimization: The child has written directly to the
	 * response buffer. The request itself is still in pending state,
	 * state that in the result code. */

	state->response.result = WINBINDD_PENDING;

	if ((!success) || (result != WINBINDD_OK)) {
		request_error(state);
		return;
	}

	request_ok(state);
}

void sendto_child(struct winbindd_cli_state *state,
		  struct winbindd_child *child)
{
	async_request(state->mem_ctx, child, &state->request,
		      &state->response, recvfrom_child, state);
}

void sendto_domain(struct winbindd_cli_state *state,
		   struct winbindd_domain *domain)
{
	async_domain_request(state->mem_ctx, domain,
			     &state->request, &state->response,
			     recvfrom_child, state);
}

static void domain_init_recv(void *private_data_data, BOOL success)
{
	struct domain_request_state *state =
		talloc_get_type_abort(private_data_data, struct domain_request_state);

	if (!success) {
		DEBUG(5, ("Domain init returned an error\n"));
		state->continuation(state->private_data_data, False);
		return;
	}

	async_request(state->mem_ctx, &state->domain->child,
		      state->request, state->response,
		      state->continuation, state->private_data_data);
}

struct winbindd_child_dispatch_table {
	enum winbindd_cmd cmd;
	enum winbindd_result (*fn)(struct winbindd_domain *domain,
				   struct winbindd_cli_state *state);
	const char *winbindd_cmd_name;
};

static struct winbindd_child_dispatch_table child_dispatch_table[] = {
	
	{ WINBINDD_LOOKUPSID,            winbindd_dual_lookupsid,             "LOOKUPSID" },
	{ WINBINDD_LOOKUPNAME,           winbindd_dual_lookupname,            "LOOKUPNAME" },
	{ WINBINDD_LOOKUPRIDS,           winbindd_dual_lookuprids,            "LOOKUPRIDS" },
	{ WINBINDD_LIST_TRUSTDOM,        winbindd_dual_list_trusted_domains,  "LIST_TRUSTDOM" },
	{ WINBINDD_INIT_CONNECTION,      winbindd_dual_init_connection,       "INIT_CONNECTION" },
	{ WINBINDD_GETDCNAME,            winbindd_dual_getdcname,             "GETDCNAME" },
	{ WINBINDD_SHOW_SEQUENCE,        winbindd_dual_show_sequence,         "SHOW_SEQUENCE" },
	{ WINBINDD_PAM_AUTH,             winbindd_dual_pam_auth,              "PAM_AUTH" },
	{ WINBINDD_PAM_AUTH_CRAP,        winbindd_dual_pam_auth_crap,         "AUTH_CRAP" },
	{ WINBINDD_PAM_LOGOFF,           winbindd_dual_pam_logoff,            "PAM_LOGOFF" },
	{ WINBINDD_PAM_CHNG_PSWD_AUTH_CRAP,winbindd_dual_pam_chng_pswd_auth_crap,"CHNG_PSWD_AUTH_CRAP" },
	{ WINBINDD_PAM_CHAUTHTOK,        winbindd_dual_pam_chauthtok,         "PAM_CHAUTHTOK" },
	{ WINBINDD_CHECK_MACHACC,        winbindd_dual_check_machine_acct,    "CHECK_MACHACC" },
	{ WINBINDD_DUAL_SID2UID,         winbindd_dual_sid2uid,               "DUAL_SID2UID" },
	{ WINBINDD_DUAL_SID2GID,         winbindd_dual_sid2gid,               "DUAL_SID2GID" },
#if 0   /* DISABLED until we fix the interface in Samba 3.0.26 --jerry */
	{ WINBINDD_DUAL_SIDS2XIDS,       winbindd_dual_sids2xids,             "DUAL_SIDS2XIDS" },
#endif  /* end DISABLED */
	{ WINBINDD_DUAL_UID2SID,         winbindd_dual_uid2sid,               "DUAL_UID2SID" },
	{ WINBINDD_DUAL_GID2SID,         winbindd_dual_gid2sid,               "DUAL_GID2SID" },
	{ WINBINDD_DUAL_UID2NAME,        winbindd_dual_uid2name,              "DUAL_UID2NAME" },
	{ WINBINDD_DUAL_NAME2UID,        winbindd_dual_name2uid,              "DUAL_NAME2UID" },
	{ WINBINDD_DUAL_GID2NAME,        winbindd_dual_gid2name,              "DUAL_GID2NAME" },
	{ WINBINDD_DUAL_NAME2GID,        winbindd_dual_name2gid,              "DUAL_NAME2GID" },
	{ WINBINDD_DUAL_SET_MAPPING,     winbindd_dual_set_mapping,           "DUAL_SET_MAPPING" },
	{ WINBINDD_DUAL_SET_HWM,         winbindd_dual_set_hwm,               "DUAL_SET_HWMS" },
	{ WINBINDD_DUAL_DUMP_MAPS,       winbindd_dual_dump_maps,             "DUAL_DUMP_MAPS" },
	{ WINBINDD_DUAL_USERINFO,        winbindd_dual_userinfo,              "DUAL_USERINFO" },
	{ WINBINDD_ALLOCATE_UID,         winbindd_dual_allocate_uid,          "ALLOCATE_UID" },
	{ WINBINDD_ALLOCATE_GID,         winbindd_dual_allocate_gid,          "ALLOCATE_GID" },
	{ WINBINDD_GETUSERDOMGROUPS,     winbindd_dual_getuserdomgroups,      "GETUSERDOMGROUPS" },
	{ WINBINDD_DUAL_GETSIDALIASES,   winbindd_dual_getsidaliases,         "GETSIDALIASES" },
	{ WINBINDD_CCACHE_NTLMAUTH,      winbindd_dual_ccache_ntlm_auth,      "CCACHE_NTLM_AUTH" },
	/* End of list */

	{ WINBINDD_NUM_CMDS, NULL, "NONE" }
};

static void child_process_request(struct winbindd_domain *domain,
				  struct winbindd_cli_state *state)
{
	struct winbindd_child_dispatch_table *table;

	/* Free response data - we may be interrupted and receive another
	   command before being able to send this data off. */

	state->response.result = WINBINDD_ERROR;
	state->response.length = sizeof(struct winbindd_response);

	state->mem_ctx = talloc_init("winbind request");
	if (state->mem_ctx == NULL)
		return;

	/* Process command */

	for (table = child_dispatch_table; table->fn; table++) {
		if (state->request.cmd == table->cmd) {
			DEBUG(10,("process_request: request fn %s\n",
				  table->winbindd_cmd_name ));
			state->response.result = table->fn(domain, state);
			break;
		}
	}

	if (!table->fn) {
		DEBUG(10,("process_request: unknown request fn number %d\n",
			  (int)state->request.cmd ));
		state->response.result = WINBINDD_ERROR;
	}

	talloc_destroy(state->mem_ctx);
}

void setup_domain_child(struct winbindd_domain *domain,
			struct winbindd_child *child,
			const char *explicit_logfile)
{
	if (explicit_logfile != NULL) {
		pstr_sprintf(child->logfilename, "%s/log.winbindd-%s",
			     dyn_LOGFILEBASE, explicit_logfile);
	} else if (domain != NULL) {
		pstr_sprintf(child->logfilename, "%s/log.wb-%s",
			     dyn_LOGFILEBASE, domain->name);
	} else {
		smb_panic("Internal error: domain == NULL && "
			  "explicit_logfile == NULL");
	}

	child->domain = domain;
}

struct winbindd_child *children = NULL;

void winbind_child_died(pid_t pid)
{
	struct winbindd_child *child;

	for (child = children; child != NULL; child = child->next) {
		if (child->pid == pid) {
			break;
		}
	}

	if (child == NULL) {
		DEBUG(5, ("Already reaped child %u died\n", (unsigned int)pid));
		return;
	}

	remove_fd_event(&child->event);
	close(child->event.fd);
	child->event.fd = 0;
	child->event.flags = 0;
	child->pid = 0;

	schedule_async_request(child);
}

/* Ensure any negative cache entries with the netbios or realm names are removed. */

void winbindd_flush_negative_conn_cache(struct winbindd_domain *domain)
{
	flush_negative_conn_cache_for_domain(domain->name);
	if (*domain->alt_name) {
		flush_negative_conn_cache_for_domain(domain->alt_name);
	}
}

/* Set our domains as offline and forward the offline message to our children. */

void winbind_msg_offline(int msg_type, struct process_id src,
			 void *buf, size_t len, void *private_data)
{
	struct winbindd_child *child;
	struct winbindd_domain *domain;

	DEBUG(10,("winbind_msg_offline: got offline message.\n"));

	if (!lp_winbind_offline_logon()) {
		DEBUG(10,("winbind_msg_offline: rejecting offline message.\n"));
		return;
	}

	/* Set our global state as offline. */
	if (!set_global_winbindd_state_offline()) {
		DEBUG(10,("winbind_msg_offline: offline request failed.\n"));
		return;
	}

	/* Set all our domains as offline. */
	for (domain = domain_list(); domain; domain = domain->next) {
		if (domain->internal) {
			continue;
		}
		DEBUG(5,("winbind_msg_offline: marking %s offline.\n", domain->name));
		set_domain_offline(domain);

		/* Send an offline message to the idmap child when our
		   primary domain goes offline */

		if ( domain->primary ) {
			struct winbindd_child *idmap = idmap_child();

			if ( idmap->pid != 0 ) {
				message_send_pid(pid_to_procid(idmap->pid), 
						 MSG_WINBIND_OFFLINE, 
						 domain->name, 
						 strlen(domain->name)+1, 
						 False);
			}			
		}
	}

	for (child = children; child != NULL; child = child->next) {
		/* Don't send message to idmap child.  We've already
		   done so above. */
		if (!child->domain || (child == idmap_child())) {
			continue;
		}

		/* Or internal domains (this should not be possible....) */
		if (child->domain->internal) {
			continue;
		}

		/* Each winbindd child should only process requests for one domain - make sure
		   we only set it online / offline for that domain. */

		DEBUG(10,("winbind_msg_offline: sending message to pid %u for domain %s.\n",
			(unsigned int)child->pid, domain->name ));

		message_send_pid(pid_to_procid(child->pid), MSG_WINBIND_OFFLINE, child->domain->name,
			strlen(child->domain->name)+1, False);
	}
}

/* Set our domains as online and forward the online message to our children. */

void winbind_msg_online(int msg_type, struct process_id src,
			void *buf, size_t len, void *private_data)
{
	struct winbindd_child *child;
	struct winbindd_domain *domain;

	DEBUG(10,("winbind_msg_online: got online message.\n"));

	if (!lp_winbind_offline_logon()) {
		DEBUG(10,("winbind_msg_online: rejecting online message.\n"));
		return;
	}

	/* Set our global state as online. */
	set_global_winbindd_state_online();

	smb_nscd_flush_user_cache();
	smb_nscd_flush_group_cache();

	/* Set all our domains as online. */
	for (domain = domain_list(); domain; domain = domain->next) {
		if (domain->internal) {
			continue;
		}
		DEBUG(5,("winbind_msg_online: requesting %s to go online.\n", domain->name));

		winbindd_flush_negative_conn_cache(domain);
		set_domain_online_request(domain);

		/* Send an online message to the idmap child when our
		   primary domain comes back online */

		if ( domain->primary ) {
			struct winbindd_child *idmap = idmap_child();
			
			if ( idmap->pid != 0 ) {
				message_send_pid(pid_to_procid(idmap->pid), 
						 MSG_WINBIND_ONLINE,
						 domain->name,
						 strlen(domain->name)+1, 
						 False);
			}
			
		}
	}

	for (child = children; child != NULL; child = child->next) {
		/* Don't send message to idmap child. */
		if (!child->domain || (child == idmap_child())) {
			continue;
		}

		/* Or internal domains (this should not be possible....) */
		if (child->domain->internal) {
			continue;
		}

		/* Each winbindd child should only process requests for one domain - make sure
		   we only set it online / offline for that domain. */

		DEBUG(10,("winbind_msg_online: sending message to pid %u for domain %s.\n",
			(unsigned int)child->pid, child->domain->name ));

		message_send_pid(pid_to_procid(child->pid), MSG_WINBIND_ONLINE, child->domain->name,
			strlen(child->domain->name)+1, False);
	}
}

/* Forward the online/offline messages to our children. */
void winbind_msg_onlinestatus(int msg_type, struct process_id src,
			      void *buf, size_t len, void *private_data)
{
	struct winbindd_child *child;

	DEBUG(10,("winbind_msg_onlinestatus: got onlinestatus message.\n"));

	for (child = children; child != NULL; child = child->next) {
		if (child->domain && child->domain->primary) {
			DEBUG(10,("winbind_msg_onlinestatus: "
				  "sending message to pid %u of primary domain.\n",
				  (unsigned int)child->pid));
			message_send_pid(pid_to_procid(child->pid), 
					 MSG_WINBIND_ONLINESTATUS, buf, len, False);
			break;
		}
	}
}


static void account_lockout_policy_handler(struct event_context *ctx,
					   struct timed_event *te,
					   const struct timeval *now,
					   void *private_data)
{
	struct winbindd_child *child =
		(struct winbindd_child *)private_data;
	TALLOC_CTX *mem_ctx = NULL;
	struct winbindd_methods *methods;
	SAM_UNK_INFO_12 lockout_policy;
	NTSTATUS result;

	DEBUG(10,("account_lockout_policy_handler called\n"));

	TALLOC_FREE(child->lockout_policy_event);

	methods = child->domain->methods;

	mem_ctx = talloc_init("account_lockout_policy_handler ctx");
	if (!mem_ctx) {
		result = NT_STATUS_NO_MEMORY;
	} else {
		result = methods->lockout_policy(child->domain, mem_ctx, &lockout_policy);
	}

	talloc_destroy(mem_ctx);

	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(10,("account_lockout_policy_handler: lockout_policy failed error %s\n",
			 nt_errstr(result)));
	}

	child->lockout_policy_event = event_add_timed(winbind_event_context(), NULL,
						      timeval_current_ofs(3600, 0),
						      "account_lockout_policy_handler",
						      account_lockout_policy_handler,
						      child);
}

/* Deal with a request to go offline. */

static void child_msg_offline(int msg_type, struct process_id src,
			      void *buf, size_t len, void *private_data)
{
	struct winbindd_domain *domain;
	const char *domainname = (const char *)buf;

	if (buf == NULL || len == 0) {
		return;
	}

	DEBUG(5,("child_msg_offline received for domain %s.\n", domainname));

	if (!lp_winbind_offline_logon()) {
		DEBUG(10,("child_msg_offline: rejecting offline message.\n"));
		return;
	}

	/* Set our global state as offline. */
	if (!set_global_winbindd_state_offline()) {
		DEBUG(10,("child_msg_offline: offline request failed.\n"));
		return;
	}

	/* Mark the requested domain offline. */

	for (domain = domain_list(); domain; domain = domain->next) {
		if (domain->internal) {
			continue;
		}
		if (strequal(domain->name, domainname)) {
			DEBUG(5,("child_msg_offline: marking %s offline.\n", domain->name));
			set_domain_offline(domain);
		}
	}
}

/* Deal with a request to go online. */

static void child_msg_online(int msg_type, struct process_id src,
			     void *buf, size_t len, void *private_data)
{
	struct winbindd_domain *domain;
	const char *domainname = (const char *)buf;

	if (buf == NULL || len == 0) {
		return;
	}

	DEBUG(5,("child_msg_online received for domain %s.\n", domainname));

	if (!lp_winbind_offline_logon()) {
		DEBUG(10,("child_msg_online: rejecting online message.\n"));
		return;
	}

	/* Set our global state as online. */
	set_global_winbindd_state_online();

	/* Try and mark everything online - delete any negative cache entries
	   to force a reconnect now. */

	for (domain = domain_list(); domain; domain = domain->next) {
		if (domain->internal) {
			continue;
		}
		if (strequal(domain->name, domainname)) {
			DEBUG(5,("child_msg_online: requesting %s to go online.\n", domain->name));
			winbindd_flush_negative_conn_cache(domain);
			set_domain_online_request(domain);
		}
	}
}

static const char *collect_onlinestatus(TALLOC_CTX *mem_ctx)
{
	struct winbindd_domain *domain;
	char *buf = NULL;

	if ((buf = talloc_asprintf(mem_ctx, "global:%s ", 
				   get_global_winbindd_state_offline() ? 
				   "Offline":"Online")) == NULL) {
		return NULL;
	}

	for (domain = domain_list(); domain; domain = domain->next) {
		if ((buf = talloc_asprintf_append(buf, "%s:%s ", 
						  domain->name, 
						  domain->online ?
						  "Online":"Offline")) == NULL) {
			return NULL;
		}
	}

	buf = talloc_asprintf_append(buf, "\n");

	DEBUG(5,("collect_onlinestatus: %s", buf));

	return buf;
}

static void child_msg_onlinestatus(int msg_type, struct process_id src,
				   void *buf, size_t len, void *private_data)
{
	TALLOC_CTX *mem_ctx;
	const char *message;
	struct process_id *sender;
	
	DEBUG(5,("winbind_msg_onlinestatus received.\n"));

	if (!buf) {
		return;
	}

	sender = (struct process_id *)buf;

	mem_ctx = talloc_init("winbind_msg_onlinestatus");
	if (mem_ctx == NULL) {
		return;
	}
	
	message = collect_onlinestatus(mem_ctx);
	if (message == NULL) {
		talloc_destroy(mem_ctx);
		return;
	}

	message_send_pid(*sender, MSG_WINBIND_ONLINESTATUS, 
			 message, strlen(message) + 1, True);

	talloc_destroy(mem_ctx);
}

static BOOL fork_domain_child(struct winbindd_child *child)
{
	int fdpair[2];
	struct winbindd_cli_state state;
	struct winbindd_domain *domain;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fdpair) != 0) {
		DEBUG(0, ("Could not open child pipe: %s\n",
			  strerror(errno)));
		return False;
	}

	ZERO_STRUCT(state);
	state.pid = sys_getpid();

	/* Stop zombies */
	CatchChild();

	/* Ensure we don't process messages whilst we're
	   changing the disposition for the child. */
	message_block();

	child->pid = sys_fork();

	if (child->pid == -1) {
		DEBUG(0, ("Could not fork: %s\n", strerror(errno)));
		message_unblock();
		return False;
	}

	if (child->pid != 0) {
		/* Parent */
		close(fdpair[0]);
		child->next = child->prev = NULL;
		DLIST_ADD(children, child);
		child->event.fd = fdpair[1];
		child->event.flags = 0;
		child->requests = NULL;
		add_fd_event(&child->event);
		/* We're ok with online/offline messages now. */
		message_unblock();
		return True;
	}

	/* Child */

	state.sock = fdpair[0];
	close(fdpair[1]);

	/* tdb needs special fork handling */
	if (tdb_reopen_all(1) == -1) {
		DEBUG(0,("tdb_reopen_all failed.\n"));
		_exit(0);
	}

	close_conns_after_fork();

	if (!override_logfile) {
		lp_set_logfile(child->logfilename);
		reopen_logs();
	}

	/* Don't handle the same messages as our parent. */
	message_deregister(MSG_SMB_CONF_UPDATED);
	message_deregister(MSG_SHUTDOWN);
	message_deregister(MSG_WINBIND_OFFLINE);
	message_deregister(MSG_WINBIND_ONLINE);
	message_deregister(MSG_WINBIND_ONLINESTATUS);

	/* The child is ok with online/offline messages now. */
	message_unblock();

	/* Handle online/offline messages. */
	message_register(MSG_WINBIND_OFFLINE, child_msg_offline, NULL);
	message_register(MSG_WINBIND_ONLINE, child_msg_online, NULL);
	message_register(MSG_WINBIND_ONLINESTATUS, child_msg_onlinestatus,
			 NULL);

	if ( child->domain ) {
		child->domain->startup = True;
		child->domain->startup_time = time(NULL);
	}

	/* Ensure we have no pending check_online events other
	   than one for this domain. */

	for (domain = domain_list(); domain; domain = domain->next) {
		if (domain != child->domain) {
			TALLOC_FREE(domain->check_online_event);
		}
	}

	/* Ensure we're not handling an event inherited from
	   our parent. */

	cancel_named_event(winbind_event_context(),
			   "krb5_ticket_refresh_handler");

	/* We might be in the idmap child...*/
	if (child->domain && !(child->domain->internal) &&
	    lp_winbind_offline_logon()) {

		set_domain_online_request(child->domain);

		child->lockout_policy_event = event_add_timed(
			winbind_event_context(), NULL, timeval_zero(),
			"account_lockout_policy_handler",
			account_lockout_policy_handler,
			child);
	}

	while (1) {

		int ret;
		fd_set read_fds;
		struct timeval t;
		struct timeval *tp;
		struct timeval now;

		/* free up any talloc memory */
		lp_TALLOC_FREE();
		main_loop_TALLOC_FREE();

		run_events(winbind_event_context(), 0, NULL, NULL);

		GetTimeOfDay(&now);

		if (child->domain && child->domain->startup &&
				(now.tv_sec > child->domain->startup_time + 30)) {
			/* No longer in "startup" mode. */
			DEBUG(10,("fork_domain_child: domain %s no longer in 'startup' mode.\n",
				child->domain->name ));
			child->domain->startup = False;
		}

		tp = get_timed_events_timeout(winbind_event_context(), &t);
		if (tp) {
			DEBUG(11,("select will use timeout of %u.%u seconds\n",
				(unsigned int)tp->tv_sec, (unsigned int)tp->tv_usec ));
		}

		/* Handle messages */

		message_dispatch();

		FD_ZERO(&read_fds);
		FD_SET(state.sock, &read_fds);

		ret = sys_select(state.sock + 1, &read_fds, NULL, NULL, tp);

		if (ret == 0) {
			DEBUG(11,("nothing is ready yet, continue\n"));
			continue;
		}

		if (ret == -1 && errno == EINTR) {
			/* We got a signal - continue. */
			continue;
		}

		if (ret == -1 && errno != EINTR) {
			DEBUG(0,("select error occured\n"));
			perror("select");
			return False;
		}

		/* fetch a request from the main daemon */
		child_read_request(&state);

		if (state.finished) {
			/* we lost contact with our parent */
			exit(0);
		}

		DEBUG(4,("child daemon request %d\n", (int)state.request.cmd));

		ZERO_STRUCT(state.response);
		state.request.null_term = '\0';
		child_process_request(child->domain, &state);

		SAFE_FREE(state.request.extra_data.data);

		cache_store_response(sys_getpid(), &state.response);

		SAFE_FREE(state.response.extra_data.data);

		/* We just send the result code back, the result
		 * structure needs to be fetched via the
		 * winbindd_cache. Hmm. That needs fixing... */

		if (write_data(state.sock,
			       (const char *)&state.response.result,
			       sizeof(state.response.result)) !=
		    sizeof(state.response.result)) {
			DEBUG(0, ("Could not write result\n"));
			exit(1);
		}
	}
}
