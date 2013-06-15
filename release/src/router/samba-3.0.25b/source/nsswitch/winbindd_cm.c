/* 
   Unix SMB/CIFS implementation.

   Winbind daemon connection manager

   Copyright (C) Tim Potter                2001
   Copyright (C) Andrew Bartlett           2002
   Copyright (C) Gerald (Jerry) Carter     2003-2005.
   Copyright (C) Volker Lendecke           2004-2005
   Copyright (C) Jeremy Allison		   2006
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
   We need to manage connections to domain controllers without having to
   mess up the main winbindd code with other issues.  The aim of the
   connection manager is to:
  
       - make connections to domain controllers and cache them
       - re-establish connections when networks or servers go down
       - centralise the policy on connection timeouts, domain controller
	 selection etc
       - manage re-entrancy for when winbindd becomes able to handle
	 multiple outstanding rpc requests
  
   Why not have connection management as part of the rpc layer like tng?
   Good question.  This code may morph into libsmb/rpc_cache.c or something
   like that but at the moment it's simply staying as part of winbind.	I
   think the TNG architecture of forcing every user of the rpc layer to use
   the connection caching system is a bad idea.	 It should be an optional
   method of using the routines.

   The TNG design is quite good but I disagree with some aspects of the
   implementation. -tpot

 */

/*
   TODO:

     - I'm pretty annoyed by all the make_nmb_name() stuff.  It should be
       moved down into another function.

     - Take care when destroying cli_structs as they can be shared between
       various sam handles.

 */

#include "includes.h"
#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

struct dc_name_ip {
	fstring name;
	struct in_addr ip;
};

extern struct winbindd_methods reconnect_methods;
extern BOOL override_logfile;

static NTSTATUS init_dc_connection_network(struct winbindd_domain *domain);
static void set_dc_type_and_flags( struct winbindd_domain *domain );
static BOOL get_dcs(TALLOC_CTX *mem_ctx, const struct winbindd_domain *domain,
		    struct dc_name_ip **dcs, int *num_dcs);

/****************************************************************
 Child failed to find DC's. Reschedule check.
****************************************************************/

static void msg_failed_to_go_online(int msg_type, struct process_id src,
				    void *buf, size_t len, void *private_data)
{
	struct winbindd_domain *domain;
	const char *domainname = (const char *)buf;

	if (buf == NULL || len == 0) {
		return;
	}

	DEBUG(5,("msg_fail_to_go_online: received for domain %s.\n", domainname));

	for (domain = domain_list(); domain; domain = domain->next) {
		if (domain->internal) {
			continue;
		}

		if (strequal(domain->name, domainname)) {
			if (domain->online) {
				/* We're already online, ignore. */
				DEBUG(5,("msg_fail_to_go_online: domain %s "
					"already online.\n", domainname));
				continue;
			}

			/* Reschedule the online check. */
			set_domain_offline(domain);
			break;
		}
	}
}

/****************************************************************
 Actually cause a reconnect from a message.
****************************************************************/

static void msg_try_to_go_online(int msg_type, struct process_id src,
				 void *buf, size_t len, void *private_data)
{
	struct winbindd_domain *domain;
	const char *domainname = (const char *)buf;

	if (buf == NULL || len == 0) {
		return;
	}

	DEBUG(5,("msg_try_to_go_online: received for domain %s.\n", domainname));

	for (domain = domain_list(); domain; domain = domain->next) {
		if (domain->internal) {
			continue;
		}

		if (strequal(domain->name, domainname)) {

			if (domain->online) {
				/* We're already online, ignore. */
				DEBUG(5,("msg_try_to_go_online: domain %s "
					"already online.\n", domainname));
				continue;
			}

			/* This call takes care of setting the online
			   flag to true if we connected, or re-adding
			   the offline handler if false. Bypasses online
			   check so always does network calls. */

			init_dc_connection_network(domain);
			break;
		}
	}
}

/****************************************************************
 Fork a child to try and contact a DC. Do this as contacting a
 DC requires blocking lookups and we don't want to block our
 parent.
****************************************************************/

static BOOL fork_child_dc_connect(struct winbindd_domain *domain)
{
	struct dc_name_ip *dcs = NULL;
	int num_dcs = 0;
	TALLOC_CTX *mem_ctx = NULL;
	pid_t child_pid;
	pid_t parent_pid = sys_getpid();

	/* Stop zombies */
	CatchChild();

	message_block();

	child_pid = sys_fork();

	if (child_pid == -1) {
		DEBUG(0, ("fork_child_dc_connect: Could not fork: %s\n", strerror(errno)));
		message_unblock();
		return False;
	}

	if (child_pid != 0) {
		/* Parent */
		message_register(MSG_WINBIND_TRY_TO_GO_ONLINE,
				 msg_try_to_go_online, NULL);
		message_register(MSG_WINBIND_FAILED_TO_GO_ONLINE,
				 msg_failed_to_go_online, NULL);
		message_unblock();
		return True;
	}

	/* Child. */

	/* Leave messages blocked - we will never process one. */

	/* tdb needs special fork handling */
	if (tdb_reopen_all(1) == -1) {
		DEBUG(0,("tdb_reopen_all failed.\n"));
		_exit(0);
	}

	close_conns_after_fork();

	if (!override_logfile) {
		pstring logfile;
		pstr_sprintf(logfile, "%s/log.winbindd-dc-connect", dyn_LOGFILEBASE);
		lp_set_logfile(logfile);
		reopen_logs();
	}

	mem_ctx = talloc_init("fork_child_dc_connect");
	if (!mem_ctx) {
		DEBUG(0,("talloc_init failed.\n"));
		_exit(0);
	}

	if ((!get_dcs(mem_ctx, domain, &dcs, &num_dcs)) || (num_dcs == 0)) {
		/* Still offline ? Can't find DC's. */
		message_send_pid(pid_to_procid(parent_pid), MSG_WINBIND_FAILED_TO_GO_ONLINE,
				domain->name,
				strlen(domain->name)+1, False);
		_exit(0);
	}

	/* We got a DC. Send a message to our parent to get it to
	   try and do the same. */

	message_send_pid(pid_to_procid(parent_pid), MSG_WINBIND_TRY_TO_GO_ONLINE,
				domain->name,
				strlen(domain->name)+1, False);
	_exit(0);
}

/****************************************************************
 Handler triggered if we're offline to try and detect a DC.
****************************************************************/

static void check_domain_online_handler(struct event_context *ctx,
					struct timed_event *te,
					const struct timeval *now,
					void *private_data)
{
        struct winbindd_domain *domain =
                (struct winbindd_domain *)private_data;

	DEBUG(10,("check_domain_online_handler: called for domain %s\n",
		domain->name ));

	TALLOC_FREE(domain->check_online_event);

	/* Are we still in "startup" mode ? */

	if (domain->startup && (now->tv_sec > domain->startup_time + 30)) {
		/* No longer in "startup" mode. */
		DEBUG(10,("check_domain_online_handler: domain %s no longer in 'startup' mode.\n",
			domain->name ));
		domain->startup = False;
	}

	/* We've been told to stay offline, so stay
	   that way. */

	if (get_global_winbindd_state_offline()) {
		DEBUG(10,("check_domain_online_handler: domain %s remaining globally offline\n",
			domain->name ));
		return;
	}

	/* Fork a child to test if it can contact a DC. 
	   If it can then send ourselves a message to
	   cause a reconnect. */

	fork_child_dc_connect(domain);
}

/****************************************************************
 If we're still offline setup the timeout check.
****************************************************************/

static void calc_new_online_timeout_check(struct winbindd_domain *domain)
{
	int wbc = lp_winbind_cache_time();

	if (domain->startup) {
		domain->check_online_timeout = 10;
	} else if (domain->check_online_timeout < wbc) {
		domain->check_online_timeout = wbc;
	}
}

/****************************************************************
 Set domain offline and also add handler to put us back online
 if we detect a DC.
****************************************************************/

void set_domain_offline(struct winbindd_domain *domain)
{
	DEBUG(10,("set_domain_offline: called for domain %s\n",
		domain->name ));

	TALLOC_FREE(domain->check_online_event);

	if (domain->internal) {
		DEBUG(3,("set_domain_offline: domain %s is internal - logic error.\n",
			domain->name ));
		return;
	}

	domain->online = False;

	/* Offline domains are always initialized. They're
	   re-initialized when they go back online. */

	domain->initialized = True;

	/* We only add the timeout handler that checks and
	   allows us to go back online when we've not
	   been told to remain offline. */

	if (get_global_winbindd_state_offline()) {
		DEBUG(10,("set_domain_offline: domain %s remaining globally offline\n",
			domain->name ));
		return;
	}

	/* If we're in statup mode, check again in 10 seconds, not in
	   lp_winbind_cache_time() seconds (which is 5 mins by default). */

	calc_new_online_timeout_check(domain);

	domain->check_online_event = event_add_timed(winbind_event_context(),
						NULL,
						timeval_current_ofs(domain->check_online_timeout,0),
						"check_domain_online_handler",
						check_domain_online_handler,
						domain);

	/* The above *has* to succeed for winbindd to work. */
	if (!domain->check_online_event) {
		smb_panic("set_domain_offline: failed to add online handler.\n");
	}

	DEBUG(10,("set_domain_offline: added event handler for domain %s\n",
		domain->name ));
}

/****************************************************************
 Set domain online - if allowed.
****************************************************************/

static void set_domain_online(struct winbindd_domain *domain)
{
	struct timeval now;

	DEBUG(10,("set_domain_online: called for domain %s\n",
		domain->name ));

	if (domain->internal) {
		DEBUG(3,("set_domain_online: domain %s is internal - logic error.\n",
			domain->name ));
		return;
	}

	if (get_global_winbindd_state_offline()) {
		DEBUG(10,("set_domain_online: domain %s remaining globally offline\n",
			domain->name ));
		return;
	}

	/* If we are waiting to get a krb5 ticket, trigger immediately. */
	GetTimeOfDay(&now);
	set_event_dispatch_time(winbind_event_context(),
				"krb5_ticket_gain_handler", now);

	/* Ok, we're out of any startup mode now... */
	domain->startup = False;

	if (domain->online == False) {
		/* We were offline - now we're online. We default to
		   using the MS-RPC backend if we started offline,
		   and if we're going online for the first time we
		   should really re-initialize the backends and the
		   checks to see if we're talking to an AD or NT domain.
		*/

		domain->initialized = False;

		/* 'reconnect_methods' is the MS-RPC backend. */
		if (domain->backend == &reconnect_methods) {
			domain->backend = NULL;
		}
	}

	/* Ensure we have no online timeout checks. */
	domain->check_online_timeout = 0;
	TALLOC_FREE(domain->check_online_event);

	/* Ensure we ignore any pending child messages. */
	message_deregister(MSG_WINBIND_TRY_TO_GO_ONLINE);
	message_deregister(MSG_WINBIND_FAILED_TO_GO_ONLINE);

	domain->online = True;
}

/****************************************************************
 Requested to set a domain online.
****************************************************************/

void set_domain_online_request(struct winbindd_domain *domain)
{
	struct timeval tev;

	DEBUG(10,("set_domain_online_request: called for domain %s\n",
		domain->name ));

	if (get_global_winbindd_state_offline()) {
		DEBUG(10,("set_domain_online_request: domain %s remaining globally offline\n",
			domain->name ));
		return;
	}

	/* We've been told it's safe to go online and
	   try and connect to a DC. But I don't believe it
	   because network manager seems to lie.
	   Wait at least 5 seconds. Heuristics suck... */

	if (!domain->check_online_event) {
		/* If we've come from being globally offline we
		   don't have a check online event handler set.
		   We need to add one now we're trying to go
		   back online. */

		DEBUG(10,("set_domain_online_request: domain %s was globally offline.\n",
			domain->name ));

		domain->check_online_event = event_add_timed(winbind_event_context(),
								NULL,
								timeval_current_ofs(5, 0),
								"check_domain_online_handler",
								check_domain_online_handler,
								domain);

		/* The above *has* to succeed for winbindd to work. */
		if (!domain->check_online_event) {
			smb_panic("set_domain_online_request: failed to add online handler.\n");
		}
	}

	GetTimeOfDay(&tev);

	/* Go into "startup" mode again. */
	domain->startup_time = tev.tv_sec;
	domain->startup = True;

	tev.tv_sec += 5;

	set_event_dispatch_time(winbind_event_context(), "check_domain_online_handler", tev);
}

/****************************************************************
 Add -ve connection cache entries for domain and realm.
****************************************************************/

void winbind_add_failed_connection_entry(const struct winbindd_domain *domain,
					const char *server,
					NTSTATUS result)
{
	add_failed_connection_entry(domain->name, server, result);
	/* If this was the saf name for the last thing we talked to,
	   remove it. */
	saf_delete(domain->name);
	if (*domain->alt_name) {
		add_failed_connection_entry(domain->alt_name, server, result);
		saf_delete(domain->alt_name);
	}
}

/* Choose between anonymous or authenticated connections.  We need to use
   an authenticated connection if DCs have the RestrictAnonymous registry
   entry set > 0, or the "Additional restrictions for anonymous
   connections" set in the win2k Local Security Policy. 
   
   Caller to free() result in domain, username, password
*/

static void cm_get_ipc_userpass(char **username, char **domain, char **password)
{
	*username = (char *)secrets_fetch(SECRETS_AUTH_USER, NULL);
	*domain = (char *)secrets_fetch(SECRETS_AUTH_DOMAIN, NULL);
	*password = (char *)secrets_fetch(SECRETS_AUTH_PASSWORD, NULL);
	
	if (*username && **username) {

		if (!*domain || !**domain)
			*domain = smb_xstrdup(lp_workgroup());
		
		if (!*password || !**password)
			*password = smb_xstrdup("");

		DEBUG(3, ("cm_get_ipc_userpass: Retrieved auth-user from secrets.tdb [%s\\%s]\n", 
			  *domain, *username));

	} else {
		DEBUG(3, ("cm_get_ipc_userpass: No auth-user defined\n"));
		*username = smb_xstrdup("");
		*domain = smb_xstrdup("");
		*password = smb_xstrdup("");
	}
}

static BOOL get_dc_name_via_netlogon(const struct winbindd_domain *domain,
				     fstring dcname, struct in_addr *dc_ip)
{
	struct winbindd_domain *our_domain = NULL;
	struct rpc_pipe_client *netlogon_pipe = NULL;
	NTSTATUS result;
	WERROR werr;
	TALLOC_CTX *mem_ctx;
	unsigned int orig_timeout;
	fstring tmp;
	char *p;

	/* Hmmmm. We can only open one connection to the NETLOGON pipe at the
	 * moment.... */

	if (IS_DC) {
		return False;
	}

	if (domain->primary) {
		return False;
	}

	our_domain = find_our_domain();

	if ((mem_ctx = talloc_init("get_dc_name_via_netlogon")) == NULL) {
		return False;
	}

	result = cm_connect_netlogon(our_domain, &netlogon_pipe);
	if (!NT_STATUS_IS_OK(result)) {
		talloc_destroy(mem_ctx);
		return False;
	}

	/* This call can take a long time - allow the server to time out.
	   35 seconds should do it. */

	orig_timeout = cli_set_timeout(netlogon_pipe->cli, 35000);
	
	werr = rpccli_netlogon_getanydcname(netlogon_pipe, mem_ctx, our_domain->dcname,
					    domain->name, tmp);

	/* And restore our original timeout. */
	cli_set_timeout(netlogon_pipe->cli, orig_timeout);

	talloc_destroy(mem_ctx);

	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(10, ("rpccli_netlogon_getanydcname failed: %s\n",
			   dos_errstr(werr)));
		return False;
	}

	/* cli_netlogon_getanydcname gives us a name with \\ */
	p = tmp;
	if (*p == '\\') {
		p+=1;
	}
	if (*p == '\\') {
		p+=1;
	}

	fstrcpy(dcname, p);

	DEBUG(10, ("rpccli_netlogon_getanydcname returned %s\n", dcname));

	if (!resolve_name(dcname, dc_ip, 0x20)) {
		return False;
	}

	return True;
}

/************************************************************************
 Given a fd with a just-connected TCP connection to a DC, open a connection
 to the pipe.
************************************************************************/

static NTSTATUS cm_prepare_connection(const struct winbindd_domain *domain,
				      const int sockfd,
				      const char *controller,
				      struct cli_state **cli,
				      BOOL *retry)
{
	char *machine_password, *machine_krb5_principal, *machine_account;
	char *ipc_username, *ipc_domain, *ipc_password;

	BOOL got_mutex;

	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	struct sockaddr peeraddr;
	socklen_t peeraddr_len;

	struct sockaddr_in *peeraddr_in = (struct sockaddr_in *)&peeraddr;

	DEBUG(10,("cm_prepare_connection: connecting to DC %s for domain %s\n",
		controller, domain->name ));

	machine_password = secrets_fetch_machine_password(lp_workgroup(), NULL,
							  NULL);
	
	if (asprintf(&machine_account, "%s$", global_myname()) == -1) {
		SAFE_FREE(machine_password);
		return NT_STATUS_NO_MEMORY;
	}

	if (asprintf(&machine_krb5_principal, "%s$@%s", global_myname(),
		     lp_realm()) == -1) {
		SAFE_FREE(machine_account);
		SAFE_FREE(machine_password);
		return NT_STATUS_NO_MEMORY;
	}

	cm_get_ipc_userpass(&ipc_username, &ipc_domain, &ipc_password);

	*retry = True;

	got_mutex = secrets_named_mutex(controller,
					WINBIND_SERVER_MUTEX_WAIT_TIME);

	if (!got_mutex) {
		DEBUG(0,("cm_prepare_connection: mutex grab failed for %s\n",
			 controller));
		result = NT_STATUS_POSSIBLE_DEADLOCK;
		goto done;
	}

	if ((*cli = cli_initialise()) == NULL) {
		DEBUG(1, ("Could not cli_initialize\n"));
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	(*cli)->timeout = 10000; 	/* 10 seconds */
	(*cli)->fd = sockfd;
	fstrcpy((*cli)->desthost, controller);
	(*cli)->use_kerberos = True;

	peeraddr_len = sizeof(peeraddr);

	if ((getpeername((*cli)->fd, &peeraddr, &peeraddr_len) != 0) ||
	    (peeraddr_len != sizeof(struct sockaddr_in)) ||
	    (peeraddr_in->sin_family != PF_INET))
	{
		DEBUG(0,("cm_prepare_connection: %s\n", strerror(errno)));
		result = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	if (ntohs(peeraddr_in->sin_port) == 139) {
		struct nmb_name calling;
		struct nmb_name called;

		make_nmb_name(&calling, global_myname(), 0x0);
		make_nmb_name(&called, "*SMBSERVER", 0x20);

		if (!cli_session_request(*cli, &calling, &called)) {
			DEBUG(8, ("cli_session_request failed for %s\n",
				  controller));
			result = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}
	}

	cli_setup_signing_state(*cli, Undefined);

	if (!cli_negprot(*cli)) {
		DEBUG(1, ("cli_negprot failed\n"));
		result = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}
			
	if ((*cli)->protocol >= PROTOCOL_NT1 && (*cli)->capabilities & CAP_EXTENDED_SECURITY) {
		ADS_STATUS ads_status;

		if (lp_security() == SEC_ADS) {

			/* Try a krb5 session */

			(*cli)->use_kerberos = True;
			DEBUG(5, ("connecting to %s from %s with kerberos principal "
				  "[%s]\n", controller, global_myname(),
				  machine_krb5_principal));

			ads_status = cli_session_setup_spnego(*cli,
							      machine_krb5_principal, 
							      machine_password, 
							      lp_workgroup());

			if (!ADS_ERR_OK(ads_status)) {
				DEBUG(4,("failed kerberos session setup with %s\n",
					 ads_errstr(ads_status)));
			}

			result = ads_ntstatus(ads_status);
			if (NT_STATUS_IS_OK(result)) {
				/* Ensure creds are stored for NTLMSSP authenticated pipe access. */
				cli_init_creds(*cli, machine_account, lp_workgroup(), machine_password);
				goto session_setup_done;
			}
		}

		/* Fall back to non-kerberos session setup using NTLMSSP SPNEGO with the machine account. */
		(*cli)->use_kerberos = False;

		DEBUG(5, ("connecting to %s from %s with username "
			  "[%s]\\[%s]\n",  controller, global_myname(),
			  lp_workgroup(), machine_account));

		ads_status = cli_session_setup_spnego(*cli,
						      machine_account, 
						      machine_password, 
						      lp_workgroup());
		if (!ADS_ERR_OK(ads_status)) {
			DEBUG(4, ("authenticated session setup failed with %s\n",
				ads_errstr(ads_status)));
		}

		result = ads_ntstatus(ads_status);
		if (NT_STATUS_IS_OK(result)) {
			/* Ensure creds are stored for NTLMSSP authenticated pipe access. */
			cli_init_creds(*cli, machine_account, lp_workgroup(), machine_password);
			goto session_setup_done;
		}
	}

	/* Fall back to non-kerberos session setup */

	(*cli)->use_kerberos = False;

	if ((((*cli)->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) != 0) &&
	    (strlen(ipc_username) > 0)) {

		/* Only try authenticated if we have a username */

		DEBUG(5, ("connecting to %s from %s with username "
			  "[%s]\\[%s]\n",  controller, global_myname(),
			  ipc_domain, ipc_username));

		if (NT_STATUS_IS_OK(cli_session_setup(
					    *cli, ipc_username,
					    ipc_password, strlen(ipc_password)+1,
					    ipc_password, strlen(ipc_password)+1,
					    ipc_domain))) {
			/* Successful logon with given username. */
			cli_init_creds(*cli, ipc_username, ipc_domain, ipc_password);
			goto session_setup_done;
		} else {
			DEBUG(4, ("authenticated session setup with user %s\\%s failed.\n",
				ipc_domain, ipc_username ));
		}
	}

	/* Fall back to anonymous connection, this might fail later */

	if (NT_STATUS_IS_OK(cli_session_setup(*cli, "", NULL, 0,
					      NULL, 0, ""))) {
		DEBUG(5, ("Connected anonymously\n"));
		cli_init_creds(*cli, "", "", "");
		goto session_setup_done;
	}

	result = cli_nt_error(*cli);

	if (NT_STATUS_IS_OK(result))
		result = NT_STATUS_UNSUCCESSFUL;

	/* We can't session setup */

	goto done;

 session_setup_done:

	/* cache the server name for later connections */

	saf_store( domain->name, (*cli)->desthost );
	if (domain->alt_name && (*cli)->use_kerberos) {
		saf_store( domain->alt_name, (*cli)->desthost );
	}

	if (!cli_send_tconX(*cli, "IPC$", "IPC", "", 0)) {

		result = cli_nt_error(*cli);

		DEBUG(1,("failed tcon_X with %s\n", nt_errstr(result)));

		if (NT_STATUS_IS_OK(result))
			result = NT_STATUS_UNSUCCESSFUL;

		goto done;
	}

	secrets_named_mutex_release(controller);
	got_mutex = False;
	*retry = False;

	/* set the domain if empty; needed for schannel connections */
	if ( !*(*cli)->domain ) {
		fstrcpy( (*cli)->domain, domain->name );
	}

	result = NT_STATUS_OK;

 done:
	if (got_mutex) {
		secrets_named_mutex_release(controller);
	}

	SAFE_FREE(machine_account);
	SAFE_FREE(machine_password);
	SAFE_FREE(machine_krb5_principal);
	SAFE_FREE(ipc_username);
	SAFE_FREE(ipc_domain);
	SAFE_FREE(ipc_password);

	if (!NT_STATUS_IS_OK(result)) {
		winbind_add_failed_connection_entry(domain, controller, result);
		if ((*cli) != NULL) {
			cli_shutdown(*cli);
			*cli = NULL;
		}
	}

	return result;
}

static BOOL add_one_dc_unique(TALLOC_CTX *mem_ctx, const char *domain_name,
			      const char *dcname, struct in_addr ip,
			      struct dc_name_ip **dcs, int *num)
{
	if (!NT_STATUS_IS_OK(check_negative_conn_cache(domain_name, dcname))) {
		DEBUG(10, ("DC %s was in the negative conn cache\n", dcname));
		return False;
	}

	*dcs = TALLOC_REALLOC_ARRAY(mem_ctx, *dcs, struct dc_name_ip, (*num)+1);

	if (*dcs == NULL)
		return False;

	fstrcpy((*dcs)[*num].name, dcname);
	(*dcs)[*num].ip = ip;
	*num += 1;
	return True;
}

static BOOL add_sockaddr_to_array(TALLOC_CTX *mem_ctx,
				  struct in_addr ip, uint16 port,
				  struct sockaddr_in **addrs, int *num)
{
	*addrs = TALLOC_REALLOC_ARRAY(mem_ctx, *addrs, struct sockaddr_in, (*num)+1);

	if (*addrs == NULL) {
		*num = 0;
		return False;
	}

	(*addrs)[*num].sin_family = PF_INET;
	putip((char *)&((*addrs)[*num].sin_addr), (char *)&ip);
	(*addrs)[*num].sin_port = htons(port);

	*num += 1;
	return True;
}

static void mailslot_name(struct in_addr dc_ip, fstring name)
{
	fstr_sprintf(name, "\\MAILSLOT\\NET\\GETDC%X", dc_ip.s_addr);
}

static BOOL send_getdc_request(struct in_addr dc_ip,
			       const char *domain_name,
			       const DOM_SID *sid)
{
	pstring outbuf;
	char *p;
	fstring my_acct_name;
	fstring my_mailslot;

	mailslot_name(dc_ip, my_mailslot);

	memset(outbuf, '\0', sizeof(outbuf));

	p = outbuf;

	SCVAL(p, 0, SAMLOGON);
	p++;

	SCVAL(p, 0, 0); /* Count pointer ... */
	p++;

	SIVAL(p, 0, 0); /* The sender's token ... */
	p += 2;

	p += dos_PutUniCode(p, global_myname(), sizeof(pstring), True);
	fstr_sprintf(my_acct_name, "%s$", global_myname());
	p += dos_PutUniCode(p, my_acct_name, sizeof(pstring), True);

	memcpy(p, my_mailslot, strlen(my_mailslot)+1);
	p += strlen(my_mailslot)+1;

	SIVAL(p, 0, 0x80);
	p+=4;

	SIVAL(p, 0, sid_size(sid));
	p+=4;

	p = ALIGN4(p, outbuf);

	sid_linearize(p, sid_size(sid), sid);
	p += sid_size(sid);

	SIVAL(p, 0, 1);
	SSVAL(p, 4, 0xffff);
	SSVAL(p, 6, 0xffff);
	p+=8;

	return cli_send_mailslot(False, "\\MAILSLOT\\NET\\NTLOGON", 0,
				 outbuf, PTR_DIFF(p, outbuf),
				 global_myname(), 0, domain_name, 0x1c,
				 dc_ip);
}

static BOOL receive_getdc_response(struct in_addr dc_ip,
				   const char *domain_name,
				   fstring dc_name)
{
	struct packet_struct *packet;
	fstring my_mailslot;
	char *buf, *p;
	fstring dcname, user, domain;
	int len;

	mailslot_name(dc_ip, my_mailslot);

	packet = receive_unexpected(DGRAM_PACKET, 0, my_mailslot);

	if (packet == NULL) {
		DEBUG(5, ("Did not receive packet for %s\n", my_mailslot));
		return False;
	}

	DEBUG(5, ("Received packet for %s\n", my_mailslot));

	buf = packet->packet.dgram.data;
	len = packet->packet.dgram.datasize;

	if (len < 70) {
		/* 70 is a completely arbitrary value to make sure
		   the SVAL below does not read uninitialized memory */
		DEBUG(3, ("GetDC got short response\n"));
		return False;
	}

	/* This should be (buf-4)+SVAL(buf-4, smb_vwv12)... */
	p = buf+SVAL(buf, smb_vwv10);

	if (CVAL(p,0) != SAMLOGON_R) {
		DEBUG(8, ("GetDC got invalid response type %d\n", CVAL(p, 0)));
		return False;
	}

	p+=2;
	pull_ucs2(buf, dcname, p, sizeof(dcname), PTR_DIFF(buf+len, p),
		  STR_TERMINATE|STR_NOALIGN);
	p = skip_unibuf(p, PTR_DIFF(buf+len, p));
	pull_ucs2(buf, user, p, sizeof(dcname), PTR_DIFF(buf+len, p),
		  STR_TERMINATE|STR_NOALIGN);
	p = skip_unibuf(p, PTR_DIFF(buf+len, p));
	pull_ucs2(buf, domain, p, sizeof(dcname), PTR_DIFF(buf+len, p),
		  STR_TERMINATE|STR_NOALIGN);
	p = skip_unibuf(p, PTR_DIFF(buf+len, p));

	if (!strequal(domain, domain_name)) {
		DEBUG(3, ("GetDC: Expected domain %s, got %s\n",
			  domain_name, domain));
		return False;
	}

	p = dcname;
	if (*p == '\\')	p += 1;
	if (*p == '\\')	p += 1;

	fstrcpy(dc_name, p);

	DEBUG(10, ("GetDC gave name %s for domain %s\n",
		   dc_name, domain));

	return True;
}

/*******************************************************************
 convert an ip to a name
*******************************************************************/

static BOOL dcip_to_name(const struct winbindd_domain *domain, struct in_addr ip, fstring name )
{
	struct ip_service ip_list;

	ip_list.ip = ip;
	ip_list.port = 0;

#ifdef WITH_ADS
	/* For active directory servers, try to get the ldap server name.
	   None of these failures should be considered critical for now */

	if (lp_security() == SEC_ADS) {
		ADS_STRUCT *ads;

		ads = ads_init(domain->alt_name, domain->name, NULL);
		ads->auth.flags |= ADS_AUTH_NO_BIND;

		if (ads_try_connect( ads, inet_ntoa(ip) ) )  {
			/* We got a cldap packet. */
			fstrcpy(name, ads->config.ldap_server_name);
			namecache_store(name, 0x20, 1, &ip_list);

			DEBUG(10,("dcip_to_name: flags = 0x%x\n", (unsigned int)ads->config.flags));

			if (domain->primary && (ads->config.flags & ADS_KDC) && ads_closest_dc(ads)) {
				char *sitename = sitename_fetch(ads->config.realm);

				/* We're going to use this KDC for this realm/domain.
				   If we are using sites, then force the krb5 libs
				   to use this KDC. */

				create_local_private_krb5_conf_for_domain(domain->alt_name,
								domain->name,
								sitename,
								ip);

				SAFE_FREE(sitename);
				/* Ensure we contact this DC also. */
				saf_store( domain->name, name);
				saf_store( domain->alt_name, name);
			}

			ads_destroy( &ads );
			return True;
		}

		ads_destroy( &ads );
	}
#endif

	/* try GETDC requests next */
	
	if (send_getdc_request(ip, domain->name, &domain->sid)) {
		int i;
		smb_msleep(100);
		for (i=0; i<5; i++) {
			if (receive_getdc_response(ip, domain->name, name)) {
				namecache_store(name, 0x20, 1, &ip_list);
				return True;
			}
			smb_msleep(500);
		}
	}

	/* try node status request */

	if ( name_status_find(domain->name, 0x1c, 0x20, ip, name) ) {
		namecache_store(name, 0x20, 1, &ip_list);
		return True;
	}
	return False;
}

/*******************************************************************
 Retreive a list of IP address for domain controllers.  Fill in 
 the dcs[]  with results.
*******************************************************************/

static BOOL get_dcs(TALLOC_CTX *mem_ctx, const struct winbindd_domain *domain,
		    struct dc_name_ip **dcs, int *num_dcs)
{
	fstring dcname;
	struct  in_addr ip;
	struct  ip_service *ip_list = NULL;
	int     iplist_size = 0;
	int     i;
	BOOL    is_our_domain;
	enum security_types sec = (enum security_types)lp_security();

	is_our_domain = strequal(domain->name, lp_workgroup());

	if ( !is_our_domain 
		&& get_dc_name_via_netlogon(domain, dcname, &ip) 
		&& add_one_dc_unique(mem_ctx, domain->name, dcname, ip, dcs, num_dcs) )
	{
		DEBUG(10, ("Retrieved DC %s at %s via netlogon\n",
			   dcname, inet_ntoa(ip)));
		return True;
	}

	if (sec == SEC_ADS) {
		char *sitename = NULL;

		/* We need to make sure we know the local site before
		   doing any DNS queries, as this will restrict the
		   get_sorted_dc_list() call below to only fetching
		   DNS records for the correct site. */

		/* Find any DC to get the site record.
		   We deliberately don't care about the
		   return here. */

		get_dc_name(domain->name, domain->alt_name, dcname, &ip);

		sitename = sitename_fetch(domain->alt_name);
		if (sitename) {

			/* Do the site-specific AD dns lookup first. */
			get_sorted_dc_list(domain->alt_name, sitename, &ip_list, &iplist_size, True);

			for ( i=0; i<iplist_size; i++ ) {
				add_one_dc_unique(mem_ctx, domain->name, inet_ntoa(ip_list[i].ip),
							ip_list[i].ip, dcs, num_dcs);
			}

			SAFE_FREE(ip_list);
			SAFE_FREE(sitename);
			iplist_size = 0;
		}

		/* Now we add DCs from the main AD dns lookup. */
		get_sorted_dc_list(domain->alt_name, NULL, &ip_list, &iplist_size, True);

		for ( i=0; i<iplist_size; i++ ) {
			add_one_dc_unique(mem_ctx, domain->name, inet_ntoa(ip_list[i].ip),
						ip_list[i].ip, dcs, num_dcs);
		}
        }

	/* try standard netbios queries if no ADS */

	if (iplist_size==0) {
		get_sorted_dc_list(domain->name, NULL, &ip_list, &iplist_size, False);
	}

	/* FIXME!! this is where we should re-insert the GETDC requests --jerry */

	/* now add to the dc array.  We'll wait until the last minute 
	   to look up the name of the DC.  But we fill in the char* for 
	   the ip now in to make the failed connection cache work */

	for ( i=0; i<iplist_size; i++ ) {
		add_one_dc_unique(mem_ctx, domain->name, inet_ntoa(ip_list[i].ip), 
			ip_list[i].ip, dcs, num_dcs);
	}

	SAFE_FREE( ip_list );

	return True;
}

static BOOL find_new_dc(TALLOC_CTX *mem_ctx,
			const struct winbindd_domain *domain,
			fstring dcname, struct sockaddr_in *addr, int *fd)
{
	struct dc_name_ip *dcs = NULL;
	int num_dcs = 0;

	const char **dcnames = NULL;
	int num_dcnames = 0;

	struct sockaddr_in *addrs = NULL;
	int num_addrs = 0;

	int i, fd_index;

 again:
	if (!get_dcs(mem_ctx, domain, &dcs, &num_dcs) || (num_dcs == 0))
		return False;

	for (i=0; i<num_dcs; i++) {

		if (!add_string_to_array(mem_ctx, dcs[i].name,
				    &dcnames, &num_dcnames)) {
			return False;
		}
		if (!add_sockaddr_to_array(mem_ctx, dcs[i].ip, 445,
				      &addrs, &num_addrs)) {
			return False;
		}

		if (!add_string_to_array(mem_ctx, dcs[i].name,
				    &dcnames, &num_dcnames)) {
			return False;
		}
		if (!add_sockaddr_to_array(mem_ctx, dcs[i].ip, 139,
				      &addrs, &num_addrs)) {
			return False;
		}
	}

	if ((num_dcnames == 0) || (num_dcnames != num_addrs))
		return False;

	if ((addrs == NULL) || (dcnames == NULL))
		return False;

	/* 5 second timeout. */
	if ( !open_any_socket_out(addrs, num_addrs, 5000, &fd_index, fd) ) 
	{
		for (i=0; i<num_dcs; i++) {
			DEBUG(10, ("find_new_dc: open_any_socket_out failed for "
				"domain %s address %s. Error was %s\n",
				domain->name, inet_ntoa(dcs[i].ip), strerror(errno) ));
			winbind_add_failed_connection_entry(domain,
				dcs[i].name, NT_STATUS_UNSUCCESSFUL);
		}
		return False;
	}

	*addr = addrs[fd_index];

	if (*dcnames[fd_index] != '\0' && !is_ipaddress(dcnames[fd_index])) {
		/* Ok, we've got a name for the DC */
		fstrcpy(dcname, dcnames[fd_index]);
		return True;
	}

	/* Try to figure out the name */
	if (dcip_to_name( domain, addr->sin_addr, dcname )) {
		return True;
	}

	/* We can not continue without the DC's name */
	winbind_add_failed_connection_entry(domain, dcs[fd_index].name,
				    NT_STATUS_UNSUCCESSFUL);
	goto again;
}

static NTSTATUS cm_open_connection(struct winbindd_domain *domain,
				   struct winbindd_cm_conn *new_conn)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS result;
	char *saf_servername = saf_fetch( domain->name );
	int retries;

	if ((mem_ctx = talloc_init("cm_open_connection")) == NULL) {
		SAFE_FREE(saf_servername);
		set_domain_offline(domain);
		return NT_STATUS_NO_MEMORY;
	}

	/* we have to check the server affinity cache here since 
	   later we selecte a DC based on response time and not preference */
	   
	/* Check the negative connection cache
	   before talking to it. It going down may have
	   triggered the reconnection. */

	if ( saf_servername && NT_STATUS_IS_OK(check_negative_conn_cache( domain->name, saf_servername))) {

		DEBUG(10,("cm_open_connection: saf_servername is '%s' for domain %s\n",
			saf_servername, domain->name ));

		/* convert an ip address to a name */
		if ( is_ipaddress( saf_servername ) ) {
			fstring saf_name;
			struct in_addr ip;

			ip = *interpret_addr2( saf_servername );
			if (dcip_to_name( domain, ip, saf_name )) {
				fstrcpy( domain->dcname, saf_name );
			} else {
				winbind_add_failed_connection_entry(
					domain, saf_servername,
					NT_STATUS_UNSUCCESSFUL);
			}
		} else {
			fstrcpy( domain->dcname, saf_servername );
		}

		SAFE_FREE( saf_servername );
	}

	for (retries = 0; retries < 3; retries++) {

		int fd = -1;
		BOOL retry = False;

		result = NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;

		DEBUG(10,("cm_open_connection: dcname is '%s' for domain %s\n",
			domain->dcname, domain->name ));

		if (*domain->dcname 
			&& NT_STATUS_IS_OK(check_negative_conn_cache( domain->name, domain->dcname))
			&& (resolve_name(domain->dcname, &domain->dcaddr.sin_addr, 0x20)))
		{
			struct sockaddr_in *addrs = NULL;
			int num_addrs = 0;
			int dummy = 0;

			if (!add_sockaddr_to_array(mem_ctx, domain->dcaddr.sin_addr, 445, &addrs, &num_addrs)) {
				set_domain_offline(domain);
				talloc_destroy(mem_ctx);
				return NT_STATUS_NO_MEMORY;
			}
			if (!add_sockaddr_to_array(mem_ctx, domain->dcaddr.sin_addr, 139, &addrs, &num_addrs)) {
				set_domain_offline(domain);
				talloc_destroy(mem_ctx);
				return NT_STATUS_NO_MEMORY;
			}

			/* 5 second timeout. */
			if (!open_any_socket_out(addrs, num_addrs, 5000, &dummy, &fd)) {
				fd = -1;
			}
		}

		if ((fd == -1) 
			&& !find_new_dc(mem_ctx, domain, domain->dcname, &domain->dcaddr, &fd))
		{
			/* This is the one place where we will
			   set the global winbindd offline state
			   to true, if a "WINBINDD_OFFLINE" entry
			   is found in the winbindd cache. */
			set_global_winbindd_state_offline();
			break;
		}

		new_conn->cli = NULL;

		result = cm_prepare_connection(domain, fd, domain->dcname,
			&new_conn->cli, &retry);

		if (!retry)
			break;
	}

	if (NT_STATUS_IS_OK(result)) {
		if (domain->online == False) {
			/* We're changing state from offline to online. */
			set_global_winbindd_state_online();
		}
		set_domain_online(domain);
	} else {
		/* Ensure we setup the retry handler. */
		set_domain_offline(domain);
	}

	talloc_destroy(mem_ctx);
	return result;
}

/* Close down all open pipes on a connection. */

void invalidate_cm_connection(struct winbindd_cm_conn *conn)
{
	/* We're closing down a possibly dead
	   connection. Don't have impossibly long (10s) timeouts. */

	if (conn->cli) {
		cli_set_timeout(conn->cli, 1000); /* 1 second. */
	}

	if (conn->samr_pipe != NULL) {
		if (!cli_rpc_pipe_close(conn->samr_pipe)) {
			/* Ok, it must be dead. Drop timeout to 0.5 sec. */
			if (conn->cli) {
				cli_set_timeout(conn->cli, 500);
			}
		}
		conn->samr_pipe = NULL;
	}

	if (conn->lsa_pipe != NULL) {
		if (!cli_rpc_pipe_close(conn->lsa_pipe)) {
			/* Ok, it must be dead. Drop timeout to 0.5 sec. */
			if (conn->cli) {
				cli_set_timeout(conn->cli, 500);
			}
		}
		conn->lsa_pipe = NULL;
	}

	if (conn->netlogon_pipe != NULL) {
		if (!cli_rpc_pipe_close(conn->netlogon_pipe)) {
			/* Ok, it must be dead. Drop timeout to 0.5 sec. */
			if (conn->cli) {
				cli_set_timeout(conn->cli, 500);
			}
		}
		conn->netlogon_pipe = NULL;
	}

	if (conn->cli) {
		cli_shutdown(conn->cli);
	}

	conn->cli = NULL;
}

void close_conns_after_fork(void)
{
	struct winbindd_domain *domain;

	for (domain = domain_list(); domain; domain = domain->next) {
		if (domain->conn.cli == NULL)
			continue;

		if (domain->conn.cli->fd == -1)
			continue;

		close(domain->conn.cli->fd);
		domain->conn.cli->fd = -1;
	}
}

static BOOL connection_ok(struct winbindd_domain *domain)
{
	if (domain->conn.cli == NULL) {
		DEBUG(8, ("connection_ok: Connection to %s for domain %s has NULL "
			  "cli!\n", domain->dcname, domain->name));
		return False;
	}

	if (!domain->conn.cli->initialised) {
		DEBUG(3, ("connection_ok: Connection to %s for domain %s was never "
			  "initialised!\n", domain->dcname, domain->name));
		return False;
	}

	if (domain->conn.cli->fd == -1) {
		DEBUG(3, ("connection_ok: Connection to %s for domain %s has died or was "
			  "never started (fd == -1)\n", 
			  domain->dcname, domain->name));
		return False;
	}

	if (domain->online == False) {
		DEBUG(3, ("connection_ok: Domain %s is offline\n", domain->name));
		return False;
	}

	return True;
}

/* Initialize a new connection up to the RPC BIND.
   Bypass online status check so always does network calls. */

static NTSTATUS init_dc_connection_network(struct winbindd_domain *domain)
{
	NTSTATUS result;

	/* Internal connections never use the network. */
	if (domain->internal) {
		domain->initialized = True;
		return NT_STATUS_OK;
	}

	if (connection_ok(domain)) {
		if (!domain->initialized) {
			set_dc_type_and_flags(domain);
		}
		return NT_STATUS_OK;
	}

	invalidate_cm_connection(&domain->conn);

	result = cm_open_connection(domain, &domain->conn);

	if (NT_STATUS_IS_OK(result) && !domain->initialized) {
		set_dc_type_and_flags(domain);
	}

	return result;
}

NTSTATUS init_dc_connection(struct winbindd_domain *domain)
{
	if (domain->initialized && !domain->online) {
		/* We check for online status elsewhere. */
		return NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;
	}

	return init_dc_connection_network(domain);
}

/******************************************************************************
 We can 'sense' certain things about the DC by it's replies to certain
 questions.

 This tells us if this particular remote server is Active Directory, and if it
 is native mode.
******************************************************************************/

static void set_dc_type_and_flags( struct winbindd_domain *domain )
{
	NTSTATUS 		result;
	DS_DOMINFO_CTR		ctr;
	TALLOC_CTX              *mem_ctx = NULL;
	struct rpc_pipe_client  *cli;
	POLICY_HND pol;

	char *domain_name = NULL;
	char *dns_name = NULL;
	char *forest_name = NULL;	
	DOM_SID *dom_sid = NULL;	

	ZERO_STRUCT( ctr );
	
	if (!connection_ok(domain)) {
		return;
	}

	DEBUG(5, ("set_dc_type_and_flags: domain %s\n", domain->name ));

	cli = cli_rpc_pipe_open_noauth(domain->conn.cli, PI_LSARPC_DS,
				       &result);

	if (cli == NULL) {
		DEBUG(5, ("set_dc_type_and_flags: Could not bind to "
			  "PI_LSARPC_DS on domain %s: (%s)\n",
			  domain->name, nt_errstr(result)));

		/* if this is just a non-AD domain we need to continue
		 * identifying so that we can in the end return with
		 * domain->initialized = True - gd */

		goto no_lsarpc_ds;
	}

	result = rpccli_ds_getprimarydominfo(cli, cli->cli->mem_ctx,
					     DsRolePrimaryDomainInfoBasic,
					     &ctr);
	cli_rpc_pipe_close(cli);

	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(5, ("set_dc_type_and_flags: rpccli_ds_getprimarydominfo "
			  "on domain %s failed: (%s)\n",
			  domain->name, nt_errstr(result)));
		return;
	}
	
	if ((ctr.basic->flags & DSROLE_PRIMARY_DS_RUNNING) &&
	    !(ctr.basic->flags & DSROLE_PRIMARY_DS_MIXED_MODE)) {
		domain->native_mode = True;
	} else {
		domain->native_mode = False;
	}

no_lsarpc_ds:
	cli = cli_rpc_pipe_open_noauth(domain->conn.cli, PI_LSARPC, &result);

	if (cli == NULL) {
		DEBUG(5, ("set_dc_type_and_flags: Could not bind to "
			  "PI_LSARPC on domain %s: (%s)\n",
			  domain->name, nt_errstr(result)));
		cli_rpc_pipe_close(cli);
		return;
	}

	mem_ctx = talloc_init("set_dc_type_and_flags on domain %s\n",
			      domain->name);
	if (!mem_ctx) {
		DEBUG(1, ("set_dc_type_and_flags: talloc_init() failed\n"));
		cli_rpc_pipe_close(cli);
		return;
	}

	result = rpccli_lsa_open_policy2(cli, mem_ctx, True, 
					 SEC_RIGHTS_MAXIMUM_ALLOWED, &pol);
		
	if (NT_STATUS_IS_OK(result)) {
		/* This particular query is exactly what Win2k clients use 
		   to determine that the DC is active directory */
		result = rpccli_lsa_query_info_policy2(cli, mem_ctx, &pol,
						       12, &domain_name,
						       &dns_name, &forest_name,
						       NULL, &dom_sid);
	}

	if (NT_STATUS_IS_OK(result)) {
		domain->active_directory = True;

		if (domain_name)
			fstrcpy(domain->name, domain_name);

		if (dns_name)
			fstrcpy(domain->alt_name, dns_name);

		if ( forest_name )
			fstrcpy(domain->forest_name, forest_name);		

		if (dom_sid) 
			sid_copy(&domain->sid, dom_sid);
	} else {
		domain->active_directory = False;

		result = rpccli_lsa_open_policy(cli, mem_ctx, True, 
						SEC_RIGHTS_MAXIMUM_ALLOWED,
						&pol);
			
		if (!NT_STATUS_IS_OK(result))
			goto done;
			
		result = rpccli_lsa_query_info_policy(cli, mem_ctx, 
						      &pol, 5, &domain_name, 
						      &dom_sid);
			
		if (NT_STATUS_IS_OK(result)) {
			if (domain_name)
				fstrcpy(domain->name, domain_name);

			if (dom_sid) 
				sid_copy(&domain->sid, dom_sid);
		}
	}
done:

	DEBUG(5, ("set_dc_type_and_flags: domain %s is %sin native mode.\n",
		  domain->name, domain->native_mode ? "" : "NOT "));

	DEBUG(5,("set_dc_type_and_flags: domain %s is %srunning active directory.\n",
		  domain->name, domain->active_directory ? "" : "NOT "));

	cli_rpc_pipe_close(cli);
	
	talloc_destroy(mem_ctx);

	domain->initialized = True;
}

static BOOL cm_get_schannel_dcinfo(struct winbindd_domain *domain,
				   struct dcinfo **ppdc)
{
	NTSTATUS result;
	struct rpc_pipe_client *netlogon_pipe;

	if (lp_client_schannel() == False) {
		return False;
	}

	result = cm_connect_netlogon(domain, &netlogon_pipe);
	if (!NT_STATUS_IS_OK(result)) {
		return False;
	}

	/* Return a pointer to the struct dcinfo from the
	   netlogon pipe. */

	*ppdc = domain->conn.netlogon_pipe->dc;
	return True;
}

NTSTATUS cm_connect_sam(struct winbindd_domain *domain, TALLOC_CTX *mem_ctx,
			struct rpc_pipe_client **cli, POLICY_HND *sam_handle)
{
	struct winbindd_cm_conn *conn;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	fstring conn_pwd;
	struct dcinfo *p_dcinfo;

	result = init_dc_connection(domain);
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	conn = &domain->conn;

	if (conn->samr_pipe != NULL) {
		goto done;
	}

	/*
	 * No SAMR pipe yet. Attempt to get an NTLMSSP SPNEGO authenticated
	 * sign and sealed pipe using the machine account password by
	 * preference. If we can't - try schannel, if that fails, try
	 * anonymous.
	 */

	pwd_get_cleartext(&conn->cli->pwd, conn_pwd);
	if ((conn->cli->user_name[0] == '\0') ||
	    (conn->cli->domain[0] == '\0') || 
	    (conn_pwd[0] == '\0')) {
		DEBUG(10, ("cm_connect_sam: No no user available for "
			   "domain %s, trying schannel\n", conn->cli->domain));
		goto schannel;
	}

	/* We have an authenticated connection. Use a NTLMSSP SPNEGO
	   authenticated SAMR pipe with sign & seal. */
	conn->samr_pipe =
		cli_rpc_pipe_open_spnego_ntlmssp(conn->cli, PI_SAMR,
						 PIPE_AUTH_LEVEL_PRIVACY,
						 conn->cli->domain,
						 conn->cli->user_name,
						 conn_pwd, &result);

	if (conn->samr_pipe == NULL) {
		DEBUG(10,("cm_connect_sam: failed to connect to SAMR "
			  "pipe for domain %s using NTLMSSP "
			  "authenticated pipe: user %s\\%s. Error was "
			  "%s\n", domain->name, conn->cli->domain,
			  conn->cli->user_name, nt_errstr(result)));
		goto schannel;
	}

	DEBUG(10,("cm_connect_sam: connected to SAMR pipe for "
		  "domain %s using NTLMSSP authenticated "
		  "pipe: user %s\\%s\n", domain->name,
		  conn->cli->domain, conn->cli->user_name ));

	result = rpccli_samr_connect(conn->samr_pipe, mem_ctx,
				     SEC_RIGHTS_MAXIMUM_ALLOWED,
				     &conn->sam_connect_handle);
	if (NT_STATUS_IS_OK(result)) {
		goto open_domain;
	}
	DEBUG(10,("cm_connect_sam: ntlmssp-sealed rpccli_samr_connect "
		  "failed for domain %s, error was %s. Trying schannel\n",
		  domain->name, nt_errstr(result) ));
	cli_rpc_pipe_close(conn->samr_pipe);

 schannel:

	/* Fall back to schannel if it's a W2K pre-SP1 box. */

	if (!cm_get_schannel_dcinfo(domain, &p_dcinfo)) {
		/* If this call fails - conn->cli can now be NULL ! */
		DEBUG(10, ("cm_connect_sam: Could not get schannel auth info "
			   "for domain %s, trying anon\n", domain->name));
		goto anonymous;
	}
	conn->samr_pipe = cli_rpc_pipe_open_schannel_with_key
		(conn->cli, PI_SAMR, PIPE_AUTH_LEVEL_PRIVACY,
		 domain->name, p_dcinfo, &result);

	if (conn->samr_pipe == NULL) {
		DEBUG(10,("cm_connect_sam: failed to connect to SAMR pipe for "
			  "domain %s using schannel. Error was %s\n",
			  domain->name, nt_errstr(result) ));
		goto anonymous;
	}
	DEBUG(10,("cm_connect_sam: connected to SAMR pipe for domain %s using "
		  "schannel.\n", domain->name ));

	result = rpccli_samr_connect(conn->samr_pipe, mem_ctx,
				     SEC_RIGHTS_MAXIMUM_ALLOWED,
				     &conn->sam_connect_handle);
	if (NT_STATUS_IS_OK(result)) {
		goto open_domain;
	}
	DEBUG(10,("cm_connect_sam: schannel-sealed rpccli_samr_connect failed "
		  "for domain %s, error was %s. Trying anonymous\n",
		  domain->name, nt_errstr(result) ));
	cli_rpc_pipe_close(conn->samr_pipe);

 anonymous:

	/* Finally fall back to anonymous. */
	conn->samr_pipe = cli_rpc_pipe_open_noauth(conn->cli, PI_SAMR,
						   &result);

	if (conn->samr_pipe == NULL) {
		result = NT_STATUS_PIPE_NOT_AVAILABLE;
		goto done;
	}

	result = rpccli_samr_connect(conn->samr_pipe, mem_ctx,
				     SEC_RIGHTS_MAXIMUM_ALLOWED,
				     &conn->sam_connect_handle);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(10,("cm_connect_sam: rpccli_samr_connect failed "
			  "for domain %s Error was %s\n",
			  domain->name, nt_errstr(result) ));
		goto done;
	}

 open_domain:
	result = rpccli_samr_open_domain(conn->samr_pipe,
					 mem_ctx,
					 &conn->sam_connect_handle,
					 SEC_RIGHTS_MAXIMUM_ALLOWED,
					 &domain->sid,
					 &conn->sam_domain_handle);

 done:

	if (!NT_STATUS_IS_OK(result)) {
		invalidate_cm_connection(conn);
		return result;
	}

	*cli = conn->samr_pipe;
	*sam_handle = conn->sam_domain_handle;
	return result;
}

NTSTATUS cm_connect_lsa(struct winbindd_domain *domain, TALLOC_CTX *mem_ctx,
			struct rpc_pipe_client **cli, POLICY_HND *lsa_policy)
{
	struct winbindd_cm_conn *conn;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	fstring conn_pwd;
	struct dcinfo *p_dcinfo;

	result = init_dc_connection(domain);
	if (!NT_STATUS_IS_OK(result))
		return result;

	conn = &domain->conn;

	if (conn->lsa_pipe != NULL) {
		goto done;
	}

	pwd_get_cleartext(&conn->cli->pwd, conn_pwd);
	if ((conn->cli->user_name[0] == '\0') ||
	    (conn->cli->domain[0] == '\0') || 
	    (conn_pwd[0] == '\0')) {
		DEBUG(10, ("cm_connect_lsa: No no user available for "
			   "domain %s, trying schannel\n", conn->cli->domain));
		goto schannel;
	}

	/* We have an authenticated connection. Use a NTLMSSP SPNEGO
	 * authenticated LSA pipe with sign & seal. */
	conn->lsa_pipe = cli_rpc_pipe_open_spnego_ntlmssp
		(conn->cli, PI_LSARPC, PIPE_AUTH_LEVEL_PRIVACY,
		 conn->cli->domain, conn->cli->user_name, conn_pwd, &result);

	if (conn->lsa_pipe == NULL) {
		DEBUG(10,("cm_connect_lsa: failed to connect to LSA pipe for "
			  "domain %s using NTLMSSP authenticated pipe: user "
			  "%s\\%s. Error was %s. Trying schannel.\n",
			  domain->name, conn->cli->domain,
			  conn->cli->user_name, nt_errstr(result)));
		goto schannel;
	}

	DEBUG(10,("cm_connect_lsa: connected to LSA pipe for domain %s using "
		  "NTLMSSP authenticated pipe: user %s\\%s\n",
		  domain->name, conn->cli->domain, conn->cli->user_name ));

	result = rpccli_lsa_open_policy(conn->lsa_pipe, mem_ctx, True,
					SEC_RIGHTS_MAXIMUM_ALLOWED,
					&conn->lsa_policy);
	if (NT_STATUS_IS_OK(result)) {
		goto done;
	}

	DEBUG(10,("cm_connect_lsa: rpccli_lsa_open_policy failed, trying "
		  "schannel\n"));

	cli_rpc_pipe_close(conn->lsa_pipe);

 schannel:

	/* Fall back to schannel if it's a W2K pre-SP1 box. */

	if (!cm_get_schannel_dcinfo(domain, &p_dcinfo)) {
		/* If this call fails - conn->cli can now be NULL ! */
		DEBUG(10, ("cm_connect_lsa: Could not get schannel auth info "
			   "for domain %s, trying anon\n", domain->name));
		goto anonymous;
	}
	conn->lsa_pipe = cli_rpc_pipe_open_schannel_with_key
		(conn->cli, PI_LSARPC, PIPE_AUTH_LEVEL_PRIVACY,
		 domain->name, p_dcinfo, &result);

	if (conn->lsa_pipe == NULL) {
		DEBUG(10,("cm_connect_lsa: failed to connect to LSA pipe for "
			  "domain %s using schannel. Error was %s\n",
			  domain->name, nt_errstr(result) ));
		goto anonymous;
	}
	DEBUG(10,("cm_connect_lsa: connected to LSA pipe for domain %s using "
		  "schannel.\n", domain->name ));

	result = rpccli_lsa_open_policy(conn->lsa_pipe, mem_ctx, True,
					SEC_RIGHTS_MAXIMUM_ALLOWED,
					&conn->lsa_policy);
	if (NT_STATUS_IS_OK(result)) {
		goto done;
	}

	DEBUG(10,("cm_connect_lsa: rpccli_lsa_open_policy failed, trying "
		  "anonymous\n"));

	cli_rpc_pipe_close(conn->lsa_pipe);

 anonymous:

	conn->lsa_pipe = cli_rpc_pipe_open_noauth(conn->cli, PI_LSARPC,
						  &result);
	if (conn->lsa_pipe == NULL) {
		result = NT_STATUS_PIPE_NOT_AVAILABLE;
		goto done;
	}

	result = rpccli_lsa_open_policy(conn->lsa_pipe, mem_ctx, True,
					SEC_RIGHTS_MAXIMUM_ALLOWED,
					&conn->lsa_policy);
 done:
	if (!NT_STATUS_IS_OK(result)) {
		invalidate_cm_connection(conn);
		return result;
	}

	*cli = conn->lsa_pipe;
	*lsa_policy = conn->lsa_policy;
	return result;
}

/****************************************************************************
 Open the netlogon pipe to this DC. Use schannel if specified in client conf.
 session key stored in conn->netlogon_pipe->dc->sess_key.
****************************************************************************/

NTSTATUS cm_connect_netlogon(struct winbindd_domain *domain,
			     struct rpc_pipe_client **cli)
{
	struct winbindd_cm_conn *conn;
	NTSTATUS result;

	uint32 neg_flags = NETLOGON_NEG_AUTH2_FLAGS;
	uint8  mach_pwd[16];
	uint32  sec_chan_type;
	const char *account_name;
	struct rpc_pipe_client *netlogon_pipe = NULL;

	*cli = NULL;

	result = init_dc_connection(domain);
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	conn = &domain->conn;

	if (conn->netlogon_pipe != NULL) {
		*cli = conn->netlogon_pipe;
		return NT_STATUS_OK;
	}

	if (!get_trust_pw(domain->name, mach_pwd, &sec_chan_type)) {
		return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
	}

	netlogon_pipe = cli_rpc_pipe_open_noauth(conn->cli, PI_NETLOGON,
						 &result);
	if (netlogon_pipe == NULL) {
		return result;
	}

	if (lp_client_schannel() != False) {
		neg_flags |= NETLOGON_NEG_SCHANNEL;
	}

	/* if we are a DC and this is a trusted domain, then we need to use our
	   domain name in the net_req_auth2() request */

	if ( IS_DC
		&& !strequal(domain->name, lp_workgroup())
		&& lp_allow_trusted_domains() ) 
	{
		account_name = lp_workgroup();
	} else {
		account_name = domain->primary ?
			global_myname() : domain->name;
	}

	if (account_name == NULL) {
		cli_rpc_pipe_close(netlogon_pipe);
		return NT_STATUS_NO_MEMORY;
	}

	result = rpccli_netlogon_setup_creds(
		 netlogon_pipe,
		 domain->dcname, /* server name. */
		 domain->name,   /* domain name */
		 global_myname(), /* client name */
		 account_name,   /* machine account */
		 mach_pwd,       /* machine password */
		 sec_chan_type,  /* from get_trust_pw */
		 &neg_flags);

	if (!NT_STATUS_IS_OK(result)) {
		cli_rpc_pipe_close(netlogon_pipe);
		return result;
	}

	if ((lp_client_schannel() == True) &&
			((neg_flags & NETLOGON_NEG_SCHANNEL) == 0)) {
		DEBUG(3, ("Server did not offer schannel\n"));
		cli_rpc_pipe_close(netlogon_pipe);
		return NT_STATUS_ACCESS_DENIED;
	}

	if ((lp_client_schannel() == False) ||
			((neg_flags & NETLOGON_NEG_SCHANNEL) == 0)) {
		/* We're done - just keep the existing connection to NETLOGON
		 * open */
		conn->netlogon_pipe = netlogon_pipe;
		*cli = conn->netlogon_pipe;
		return NT_STATUS_OK;
	}

	/* Using the credentials from the first pipe, open a signed and sealed
	   second netlogon pipe. The session key is stored in the schannel
	   part of the new pipe auth struct.
	*/

	conn->netlogon_pipe =
		cli_rpc_pipe_open_schannel_with_key(conn->cli,
						    PI_NETLOGON,
						    PIPE_AUTH_LEVEL_PRIVACY,
						    domain->name,
						    netlogon_pipe->dc,
						    &result);

	/* We can now close the initial netlogon pipe. */
	cli_rpc_pipe_close(netlogon_pipe);

	if (conn->netlogon_pipe == NULL) {
		DEBUG(3, ("Could not open schannel'ed NETLOGON pipe. Error "
			  "was %s\n", nt_errstr(result)));
			  
		/* make sure we return something besides OK */
		return !NT_STATUS_IS_OK(result) ? result : NT_STATUS_PIPE_NOT_AVAILABLE;
	}

	*cli = conn->netlogon_pipe;
	return NT_STATUS_OK;
}
