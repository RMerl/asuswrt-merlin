/* 
   Unix SMB/CIFS implementation.

   Winbind daemon for ntdom nss module

   Copyright (C) Tim Potter 2000-2001
   Copyright (C) 2001 by Martin Pool <mbp@samba.org>
   
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

#include "includes.h"
#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

extern struct winbindd_methods cache_methods;
extern struct winbindd_methods builtin_passdb_methods;
extern struct winbindd_methods sam_passdb_methods;

/**
 * @file winbindd_util.c
 *
 * Winbind daemon for NT domain authentication nss module.
 **/


/* The list of trusted domains.  Note that the list can be deleted and
   recreated using the init_domain_list() function so pointers to
   individual winbindd_domain structures cannot be made.  Keep a copy of
   the domain name instead. */

static struct winbindd_domain *_domain_list;

/**
   When was the last scan of trusted domains done?
   
   0 == not ever
*/

static time_t last_trustdom_scan;

struct winbindd_domain *domain_list(void)
{
	/* Initialise list */

	if ((!_domain_list) && (!init_domain_list())) {
		smb_panic("Init_domain_list failed\n");
	}

	return _domain_list;
}

/* Free all entries in the trusted domain list */

void free_domain_list(void)
{
	struct winbindd_domain *domain = _domain_list;

	while(domain) {
		struct winbindd_domain *next = domain->next;
		
		DLIST_REMOVE(_domain_list, domain);
		SAFE_FREE(domain);
		domain = next;
	}
}

static BOOL is_internal_domain(const DOM_SID *sid)
{
	if (sid == NULL)
		return False;

	if ( IS_DC )
		return sid_check_is_builtin(sid);

	return (sid_check_is_domain(sid) || sid_check_is_builtin(sid));
}

static BOOL is_in_internal_domain(const DOM_SID *sid)
{
	if (sid == NULL)
		return False;

	if ( IS_DC )
		return sid_check_is_in_builtin(sid);

	return (sid_check_is_in_our_domain(sid) || sid_check_is_in_builtin(sid));
}


/* Add a trusted domain to our list of domains */
static struct winbindd_domain *add_trusted_domain(const char *domain_name, const char *alt_name,
						  struct winbindd_methods *methods,
						  const DOM_SID *sid)
{
	struct winbindd_domain *domain;
	const char *alternative_name = NULL;
	
	/* ignore alt_name if we are not in an AD domain */
	
	if ( (lp_security() == SEC_ADS) && alt_name && *alt_name) {
		alternative_name = alt_name;
	}
        
	/* We can't call domain_list() as this function is called from
	   init_domain_list() and we'll get stuck in a loop. */
	for (domain = _domain_list; domain; domain = domain->next) {
		if (strequal(domain_name, domain->name) ||
		    strequal(domain_name, domain->alt_name)) {
			return domain;
		}
		if (alternative_name && *alternative_name) {
			if (strequal(alternative_name, domain->name) ||
			    strequal(alternative_name, domain->alt_name)) {
				return domain;
			}
		}
		if (sid) {
			if (is_null_sid(sid)) {
				
			} else if (sid_equal(sid, &domain->sid)) {
				return domain;
			}
		}
	}
        
	/* Create new domain entry */

	if ((domain = SMB_MALLOC_P(struct winbindd_domain)) == NULL)
		return NULL;

	/* Fill in fields */
        
	ZERO_STRUCTP(domain);

	fstrcpy(domain->name, domain_name);
	if (alternative_name) {
		fstrcpy(domain->alt_name, alternative_name);
	}

	domain->methods = methods;
	domain->backend = NULL;
	domain->internal = is_internal_domain(sid);
	domain->sequence_number = DOM_SEQUENCE_NONE;
	domain->last_seq_check = 0;
	domain->initialized = False;
	domain->online = is_internal_domain(sid);
	domain->check_online_timeout = 0;
	if (sid) {
		sid_copy(&domain->sid, sid);
	}
	
	/* Link to domain list */
	DLIST_ADD(_domain_list, domain);
        
	DEBUG(2,("Added domain %s %s %s\n", 
		 domain->name, domain->alt_name,
		 &domain->sid?sid_string_static(&domain->sid):""));
        
	return domain;
}

/********************************************************************
  rescan our domains looking for new trusted domains
********************************************************************/

struct trustdom_state {
	TALLOC_CTX *mem_ctx;
	struct winbindd_response *response;
};

static void trustdom_recv(void *private_data, BOOL success);

static void add_trusted_domains( struct winbindd_domain *domain )
{
	TALLOC_CTX *mem_ctx;
	struct winbindd_request *request;
	struct winbindd_response *response;

	struct trustdom_state *state;

	mem_ctx = talloc_init("add_trusted_domains");
	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_init failed\n"));
		return;
	}

	request = TALLOC_ZERO_P(mem_ctx, struct winbindd_request);
	response = TALLOC_P(mem_ctx, struct winbindd_response);
	state = TALLOC_P(mem_ctx, struct trustdom_state);

	if ((request == NULL) || (response == NULL) || (state == NULL)) {
		DEBUG(0, ("talloc failed\n"));
		talloc_destroy(mem_ctx);
		return;
	}

	state->mem_ctx = mem_ctx;
	state->response = response;

	request->length = sizeof(*request);
	request->cmd = WINBINDD_LIST_TRUSTDOM;

	async_domain_request(mem_ctx, domain, request, response,
			     trustdom_recv, state);
}

static void trustdom_recv(void *private_data, BOOL success)
{
	struct trustdom_state *state =
		talloc_get_type_abort(private_data, struct trustdom_state);
	struct winbindd_response *response = state->response;
	char *p;

	if ((!success) || (response->result != WINBINDD_OK)) {
		DEBUG(1, ("Could not receive trustdoms\n"));
		talloc_destroy(state->mem_ctx);
		return;
	}

	p = (char *)response->extra_data.data;

	while ((p != NULL) && (*p != '\0')) {
		char *q, *sidstr, *alt_name;
		DOM_SID sid;

		alt_name = strchr(p, '\\');
		if (alt_name == NULL) {
			DEBUG(0, ("Got invalid trustdom response\n"));
			break;
		}

		*alt_name = '\0';
		alt_name += 1;

		sidstr = strchr(alt_name, '\\');
		if (sidstr == NULL) {
			DEBUG(0, ("Got invalid trustdom response\n"));
			break;
		}

		*sidstr = '\0';
		sidstr += 1;

		q = strchr(sidstr, '\n');
		if (q != NULL)
			*q = '\0';

		if (!string_to_sid(&sid, sidstr)) {
			/* Allow NULL sid for sibling domains */
			if ( strcmp(sidstr,"S-0-0") == 0) {
				sid_copy( &sid, &global_sid_NULL);				
			} else {				
				DEBUG(0, ("Got invalid trustdom response\n"));
				break;
			}			
		}

		if (find_domain_from_name_noinit(p) == NULL) {
			struct winbindd_domain *domain;
			char *alternate_name = NULL;
			
			/* use the real alt_name if we have one, else pass in NULL */

			if ( !strequal( alt_name, "(null)" ) )
				alternate_name = alt_name;

			domain = add_trusted_domain(p, alternate_name,
						    &cache_methods,
						    &sid);
			setup_domain_child(domain, &domain->child, NULL);
		}
		p=q;
		if (p != NULL)
			p += 1;
	}

	SAFE_FREE(response->extra_data.data);
	talloc_destroy(state->mem_ctx);
}

/********************************************************************
 Periodically we need to refresh the trusted domain cache for smbd 
********************************************************************/

void rescan_trusted_domains( void )
{
	time_t now = time(NULL);
	
	/* see if the time has come... */
	
	if ((now >= last_trustdom_scan) &&
	    ((now-last_trustdom_scan) < WINBINDD_RESCAN_FREQ) )
		return;
		
	/* this will only add new domains we didn't already know about */
	
	add_trusted_domains( find_our_domain() );

	last_trustdom_scan = now;
	
	return;	
}

struct init_child_state {
	TALLOC_CTX *mem_ctx;
	struct winbindd_domain *domain;
	struct winbindd_request *request;
	struct winbindd_response *response;
	void (*continuation)(void *private_data, BOOL success);
	void *private_data;
};

static void init_child_recv(void *private_data, BOOL success);
static void init_child_getdc_recv(void *private_data, BOOL success);

enum winbindd_result init_child_connection(struct winbindd_domain *domain,
					   void (*continuation)(void *private_data,
								BOOL success),
					   void *private_data)
{
	TALLOC_CTX *mem_ctx;
	struct winbindd_request *request;
	struct winbindd_response *response;
	struct init_child_state *state;
	struct winbindd_domain *request_domain;

	mem_ctx = talloc_init("init_child_connection");
	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_init failed\n"));
		return WINBINDD_ERROR;
	}

	request = TALLOC_ZERO_P(mem_ctx, struct winbindd_request);
	response = TALLOC_P(mem_ctx, struct winbindd_response);
	state = TALLOC_P(mem_ctx, struct init_child_state);

	if ((request == NULL) || (response == NULL) || (state == NULL)) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(mem_ctx);
		continuation(private_data, False);
		return WINBINDD_ERROR;
	}

	request->length = sizeof(*request);

	state->mem_ctx = mem_ctx;
	state->domain = domain;
	state->request = request;
	state->response = response;
	state->continuation = continuation;
	state->private_data = private_data;

	if (IS_DC || domain->primary) {
		/* The primary domain has to find the DC name itself */
		request->cmd = WINBINDD_INIT_CONNECTION;
		fstrcpy(request->domain_name, domain->name);
		request->data.init_conn.is_primary = True;
		fstrcpy(request->data.init_conn.dcname, "");
		async_request(mem_ctx, &domain->child, request, response,
			      init_child_recv, state);
		return WINBINDD_PENDING;
	}

	/* This is *not* the primary domain, let's ask our DC about a DC
	 * name */

	request->cmd = WINBINDD_GETDCNAME;
	fstrcpy(request->domain_name, domain->name);

	request_domain = find_our_domain();
	
	async_domain_request(mem_ctx, request_domain, request, response,
			     init_child_getdc_recv, state);
	return WINBINDD_PENDING;
}

static void init_child_getdc_recv(void *private_data, BOOL success)
{
	struct init_child_state *state =
		talloc_get_type_abort(private_data, struct init_child_state);
	const char *dcname = "";

	DEBUG(10, ("Received getdcname response\n"));

	if (success && (state->response->result == WINBINDD_OK)) {
		dcname = state->response->data.dc_name;
	}

	state->request->cmd = WINBINDD_INIT_CONNECTION;
	fstrcpy(state->request->domain_name, state->domain->name);
	state->request->data.init_conn.is_primary = False;
	fstrcpy(state->request->data.init_conn.dcname, dcname);

	async_request(state->mem_ctx, &state->domain->child,
		      state->request, state->response,
		      init_child_recv, state);
}

static void init_child_recv(void *private_data, BOOL success)
{
	struct init_child_state *state =
		talloc_get_type_abort(private_data, struct init_child_state);

	DEBUG(5, ("Received child initialization response for domain %s\n",
		  state->domain->name));

	if ((!success) || (state->response->result != WINBINDD_OK)) {
		DEBUG(3, ("Could not init child\n"));
		state->continuation(state->private_data, False);
		talloc_destroy(state->mem_ctx);
		return;
	}

	fstrcpy(state->domain->name,
		state->response->data.domain_info.name);
	fstrcpy(state->domain->alt_name,
		state->response->data.domain_info.alt_name);
	string_to_sid(&state->domain->sid,
		      state->response->data.domain_info.sid);
	state->domain->native_mode =
		state->response->data.domain_info.native_mode;
	state->domain->active_directory =
		state->response->data.domain_info.active_directory;
	state->domain->sequence_number =
		state->response->data.domain_info.sequence_number;

	init_dc_connection(state->domain);

	if (state->continuation != NULL)
		state->continuation(state->private_data, True);
	talloc_destroy(state->mem_ctx);
}

enum winbindd_result winbindd_dual_init_connection(struct winbindd_domain *domain,
						   struct winbindd_cli_state *state)
{
	/* Ensure null termination */
	state->request.domain_name
		[sizeof(state->request.domain_name)-1]='\0';
	state->request.data.init_conn.dcname
		[sizeof(state->request.data.init_conn.dcname)-1]='\0';

	if (strlen(state->request.data.init_conn.dcname) > 0) {
		fstrcpy(domain->dcname, state->request.data.init_conn.dcname);
	}

	init_dc_connection(domain);

	if (!domain->initialized) {
		/* If we return error here we can't do any cached authentication,
		   but we may be in disconnected mode and can't initialize correctly.
		   Do what the previous code did and just return without initialization,
		   once we go online we'll re-initialize.
		*/
		DEBUG(5, ("winbindd_dual_init_connection: %s returning without initialization "
			"online = %d\n", domain->name, (int)domain->online ));
	}

	fstrcpy(state->response.data.domain_info.name, domain->name);
	fstrcpy(state->response.data.domain_info.alt_name, domain->alt_name);
	fstrcpy(state->response.data.domain_info.sid,
		sid_string_static(&domain->sid));
	
	state->response.data.domain_info.native_mode
		= domain->native_mode;
	state->response.data.domain_info.active_directory
		= domain->active_directory;
	state->response.data.domain_info.primary
		= domain->primary;
	state->response.data.domain_info.sequence_number =
		domain->sequence_number;

	return WINBINDD_OK;
}

/* Look up global info for the winbind daemon */
BOOL init_domain_list(void)
{
	struct winbindd_domain *domain;
	int role = lp_server_role();

	/* Free existing list */
	free_domain_list();

	/* Add ourselves as the first entry. */

	if ( role == ROLE_DOMAIN_MEMBER ) {
		DOM_SID our_sid;

		if (!secrets_fetch_domain_sid(lp_workgroup(), &our_sid)) {
			DEBUG(0, ("Could not fetch our SID - did we join?\n"));
			return False;
		}
	
		domain = add_trusted_domain( lp_workgroup(), lp_realm(),
					     &cache_methods, &our_sid);
		domain->primary = True;
		setup_domain_child(domain, &domain->child, NULL);
		
		/* Even in the parent winbindd we'll need to
		   talk to the DC, so try and see if we can
		   contact it. Theoretically this isn't neccessary
		   as the init_dc_connection() in init_child_recv()
		   will do this, but we can start detecting the DC
		   early here. */
		set_domain_online_request(domain);
	}

	/* Local SAM */

	domain = add_trusted_domain(get_global_sam_name(), NULL,
				    &sam_passdb_methods, get_global_sam_sid());
	if ( role != ROLE_DOMAIN_MEMBER ) {
		domain->primary = True;
	}
	setup_domain_child(domain, &domain->child, NULL);

	/* BUILTIN domain */

	domain = add_trusted_domain("BUILTIN", NULL, &builtin_passdb_methods,
				    &global_sid_Builtin);
	setup_domain_child(domain, &domain->child, NULL);

	return True;
}

/** 
 * Given a domain name, return the struct winbindd domain info for it 
 *
 * @note Do *not* pass lp_workgroup() to this function.  domain_list
 *       may modify it's value, and free that pointer.  Instead, our local
 *       domain may be found by calling find_our_domain().
 *       directly.
 *
 *
 * @return The domain structure for the named domain, if it is working.
 */

struct winbindd_domain *find_domain_from_name_noinit(const char *domain_name)
{
	struct winbindd_domain *domain;

	/* Search through list */

	for (domain = domain_list(); domain != NULL; domain = domain->next) {
		if (strequal(domain_name, domain->name) ||
		    (domain->alt_name[0] &&
		     strequal(domain_name, domain->alt_name))) {
			return domain;
		}
	}

	/* Not found */

	return NULL;
}

struct winbindd_domain *find_domain_from_name(const char *domain_name)
{
	struct winbindd_domain *domain;

	domain = find_domain_from_name_noinit(domain_name);

	if (domain == NULL)
		return NULL;

	if (!domain->initialized)
		init_dc_connection(domain);

	return domain;
}

/* Given a domain sid, return the struct winbindd domain info for it */

struct winbindd_domain *find_domain_from_sid_noinit(const DOM_SID *sid)
{
	struct winbindd_domain *domain;

	/* Search through list */

	for (domain = domain_list(); domain != NULL; domain = domain->next) {
		if (sid_compare_domain(sid, &domain->sid) == 0)
			return domain;
	}

	/* Not found */

	return NULL;
}

/* Given a domain sid, return the struct winbindd domain info for it */

struct winbindd_domain *find_domain_from_sid(const DOM_SID *sid)
{
	struct winbindd_domain *domain;

	domain = find_domain_from_sid_noinit(sid);

	if (domain == NULL)
		return NULL;

	if (!domain->initialized)
		init_dc_connection(domain);

	return domain;
}

struct winbindd_domain *find_our_domain(void)
{
	struct winbindd_domain *domain;

	/* Search through list */

	for (domain = domain_list(); domain != NULL; domain = domain->next) {
		if (domain->primary)
			return domain;
	}

	smb_panic("Could not find our domain\n");
	return NULL;
}

struct winbindd_domain *find_root_domain(void)
{
	struct winbindd_domain *ours = find_our_domain();	
	
	if ( !ours )
		return NULL;
	
	if ( strlen(ours->forest_name) == 0 )
		return NULL;
	
	return find_domain_from_name( ours->forest_name );
}

struct winbindd_domain *find_builtin_domain(void)
{
	DOM_SID sid;
	struct winbindd_domain *domain;

	string_to_sid(&sid, "S-1-5-32");
	domain = find_domain_from_sid(&sid);

	if (domain == NULL)
		smb_panic("Could not find BUILTIN domain\n");

	return domain;
}

/* Find the appropriate domain to lookup a name or SID */

struct winbindd_domain *find_lookup_domain_from_sid(const DOM_SID *sid)
{
	/* A DC can't ask the local smbd for remote SIDs, here winbindd is the
	 * one to contact the external DC's. On member servers the internal
	 * domains are different: These are part of the local SAM. */

	DEBUG(10, ("find_lookup_domain_from_sid(%s)\n",
		   sid_string_static(sid)));

	if (IS_DC || is_internal_domain(sid) || is_in_internal_domain(sid)) {
		DEBUG(10, ("calling find_domain_from_sid\n"));
		return find_domain_from_sid(sid);
	}

	/* On a member server a query for SID or name can always go to our
	 * primary DC. */

	DEBUG(10, ("calling find_our_domain\n"));
	return find_our_domain();
}

struct winbindd_domain *find_lookup_domain_from_name(const char *domain_name)
{
	if (IS_DC || strequal(domain_name, "BUILTIN") ||
	    strequal(domain_name, get_global_sam_name()))
		return find_domain_from_name_noinit(domain_name);

	return find_our_domain();
}

/* Lookup a sid in a domain from a name */

BOOL winbindd_lookup_sid_by_name(TALLOC_CTX *mem_ctx,
				 struct winbindd_domain *domain, 
				 const char *domain_name,
				 const char *name, DOM_SID *sid, 
				 enum lsa_SidType *type)
{
	NTSTATUS result;

	/* Lookup name */
	result = domain->methods->name_to_sid(domain, mem_ctx, domain_name, name, sid, type);

	/* Return sid and type if lookup successful */
	if (!NT_STATUS_IS_OK(result)) {
		*type = SID_NAME_UNKNOWN;
	}

	return NT_STATUS_IS_OK(result);
}

/**
 * @brief Lookup a name in a domain from a sid.
 *
 * @param sid Security ID you want to look up.
 * @param name On success, set to the name corresponding to @p sid.
 * @param dom_name On success, set to the 'domain name' corresponding to @p sid.
 * @param type On success, contains the type of name: alias, group or
 * user.
 * @retval True if the name exists, in which case @p name and @p type
 * are set, otherwise False.
 **/
BOOL winbindd_lookup_name_by_sid(TALLOC_CTX *mem_ctx,
				 DOM_SID *sid,
				 char **dom_name,
				 char **name,
				 enum lsa_SidType *type)
{
	NTSTATUS result;
	struct winbindd_domain *domain;

	*dom_name = NULL;
	*name = NULL;

	domain = find_lookup_domain_from_sid(sid);

	if (!domain) {
		DEBUG(1,("Can't find domain from sid\n"));
		return False;
	}

	/* Lookup name */

	result = domain->methods->sid_to_name(domain, mem_ctx, sid, dom_name, name, type);

	/* Return name and type if successful */
        
	if (NT_STATUS_IS_OK(result)) {
		return True;
	}

	*type = SID_NAME_UNKNOWN;
        
	return False;
}

/* Free state information held for {set,get,end}{pw,gr}ent() functions */

void free_getent_state(struct getent_state *state)
{
	struct getent_state *temp;

	/* Iterate over state list */

	temp = state;

	while(temp != NULL) {
		struct getent_state *next = temp->next;

		/* Free sam entries then list entry */

		SAFE_FREE(state->sam_entries);
		DLIST_REMOVE(state, state);

		SAFE_FREE(temp);
		temp = next;
	}
}

/* Is this a domain which we may assume no DOMAIN\ prefix? */

static BOOL assume_domain(const char *domain)
{
	/* never assume the domain on a standalone server */

	if ( lp_server_role() == ROLE_STANDALONE )
		return False;

	/* domain member servers may possibly assume for the domain name */

	if ( lp_server_role() == ROLE_DOMAIN_MEMBER ) {
		if ( !strequal(lp_workgroup(), domain) )
			return False;

		if ( lp_winbind_use_default_domain() || lp_winbind_trusted_domains_only() )
			return True;
	} 

	/* only left with a domain controller */

	if ( strequal(get_global_sam_name(), domain) )  {
		return True;
	}
	
	return False;
}

/* Parse a string of the form DOMAIN\user into a domain and a user */

BOOL parse_domain_user(const char *domuser, fstring domain, fstring user)
{
	char *p = strchr(domuser,*lp_winbind_separator());

	if ( !p ) {
		fstrcpy(user, domuser);

		if ( assume_domain(lp_workgroup())) {
			fstrcpy(domain, lp_workgroup());
		} else {
			return False;
		}
	} else {
		fstrcpy(user, p+1);
		fstrcpy(domain, domuser);
		domain[PTR_DIFF(p, domuser)] = 0;
	}
	
	strupper_m(domain);
	
	return True;
}

BOOL parse_domain_user_talloc(TALLOC_CTX *mem_ctx, const char *domuser,
			      char **domain, char **user)
{
	fstring fstr_domain, fstr_user;
	if (!parse_domain_user(domuser, fstr_domain, fstr_user)) {
		return False;
	}
	*domain = talloc_strdup(mem_ctx, fstr_domain);
	*user = talloc_strdup(mem_ctx, fstr_user);
	return ((*domain != NULL) && (*user != NULL));
}

/* Ensure an incoming username from NSS is fully qualified. Replace the
   incoming fstring with DOMAIN <separator> user. Returns the same
   values as parse_domain_user() but also replaces the incoming username.
   Used to ensure all names are fully qualified within winbindd.
   Used by the NSS protocols of auth, chauthtok, logoff and ccache_ntlm_auth.
   The protocol definitions of auth_crap, chng_pswd_auth_crap
   really should be changed to use this instead of doing things
   by hand. JRA. */

BOOL canonicalize_username(fstring username_inout, fstring domain, fstring user)
{
	if (!parse_domain_user(username_inout, domain, user)) {
		return False;
	}
	slprintf(username_inout, sizeof(fstring) - 1, "%s%c%s",
		 domain, *lp_winbind_separator(),
		 user);
	return True;
}

/*
    Fill DOMAIN\\USERNAME entry accounting 'winbind use default domain' and
    'winbind separator' options.
    This means:
	- omit DOMAIN when 'winbind use default domain = true' and DOMAIN is
	lp_workgroup()

    If we are a PDC or BDC, and this is for our domain, do likewise.

    Also, if omit DOMAIN if 'winbind trusted domains only = true', as the 
    username is then unqualified in unix

    We always canonicalize as UPPERCASE DOMAIN, lowercase username.
*/
void fill_domain_username(fstring name, const char *domain, const char *user, BOOL can_assume)
{
	fstring tmp_user;

	fstrcpy(tmp_user, user);
	strlower_m(tmp_user);

	if (can_assume && assume_domain(domain)) {
		strlcpy(name, tmp_user, sizeof(fstring));
	} else {
		slprintf(name, sizeof(fstring) - 1, "%s%c%s",
			 domain, *lp_winbind_separator(),
			 tmp_user);
	}
}

/*
 * Winbindd socket accessor functions
 */

char *get_winbind_priv_pipe_dir(void) 
{
	return lock_path(WINBINDD_PRIV_SOCKET_SUBDIR);
}

/* Open the winbindd socket */

static int _winbindd_socket = -1;
static int _winbindd_priv_socket = -1;

int open_winbindd_socket(void)
{
	if (_winbindd_socket == -1) {
		_winbindd_socket = create_pipe_sock(
			WINBINDD_SOCKET_DIR, WINBINDD_SOCKET_NAME, 0755);
		DEBUG(10, ("open_winbindd_socket: opened socket fd %d\n",
			   _winbindd_socket));
	}

	return _winbindd_socket;
}

int open_winbindd_priv_socket(void)
{
	if (_winbindd_priv_socket == -1) {
		_winbindd_priv_socket = create_pipe_sock(
			get_winbind_priv_pipe_dir(), WINBINDD_SOCKET_NAME, 0750);
		DEBUG(10, ("open_winbindd_priv_socket: opened socket fd %d\n",
			   _winbindd_priv_socket));
	}

	return _winbindd_priv_socket;
}

/* Close the winbindd socket */

void close_winbindd_socket(void)
{
	if (_winbindd_socket != -1) {
		DEBUG(10, ("close_winbindd_socket: closing socket fd %d\n",
			   _winbindd_socket));
		close(_winbindd_socket);
		_winbindd_socket = -1;
	}
	if (_winbindd_priv_socket != -1) {
		DEBUG(10, ("close_winbindd_socket: closing socket fd %d\n",
			   _winbindd_priv_socket));
		close(_winbindd_priv_socket);
		_winbindd_priv_socket = -1;
	}
}

/*
 * Client list accessor functions
 */

static struct winbindd_cli_state *_client_list;
static int _num_clients;

/* Return list of all connected clients */

struct winbindd_cli_state *winbindd_client_list(void)
{
	return _client_list;
}

/* Add a connection to the list */

void winbindd_add_client(struct winbindd_cli_state *cli)
{
	DLIST_ADD(_client_list, cli);
	_num_clients++;
}

/* Remove a client from the list */

void winbindd_remove_client(struct winbindd_cli_state *cli)
{
	DLIST_REMOVE(_client_list, cli);
	_num_clients--;
}

/* Close all open clients */

void winbindd_kill_all_clients(void)
{
	struct winbindd_cli_state *cl = winbindd_client_list();

	DEBUG(10, ("winbindd_kill_all_clients: going postal\n"));

	while (cl) {
		struct winbindd_cli_state *next;
		
		next = cl->next;
		winbindd_remove_client(cl);
		cl = next;
	}
}

/* Return number of open clients */

int winbindd_num_clients(void)
{
	return _num_clients;
}

NTSTATUS lookup_usergroups_cached(struct winbindd_domain *domain,
				  TALLOC_CTX *mem_ctx,
				  const DOM_SID *user_sid,
				  uint32 *p_num_groups, DOM_SID **user_sids)
{
	NET_USER_INFO_3 *info3 = NULL;
	NTSTATUS status = NT_STATUS_NO_MEMORY;
	int i;
	size_t num_groups = 0;
	DOM_SID group_sid, primary_group;
	
	DEBUG(3,(": lookup_usergroups_cached\n"));
	
	*user_sids = NULL;
	num_groups = 0;
	*p_num_groups = 0;

	info3 = netsamlogon_cache_get(mem_ctx, user_sid);

	if (info3 == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (info3->num_groups == 0) {
		TALLOC_FREE(info3);
		return NT_STATUS_UNSUCCESSFUL;
	}
	
	/* always add the primary group to the sid array */
	sid_compose(&primary_group, &info3->dom_sid.sid, info3->user_rid);
	
	if (!add_sid_to_array(mem_ctx, &primary_group, user_sids, &num_groups)) {
		TALLOC_FREE(info3);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<info3->num_groups; i++) {
		sid_copy(&group_sid, &info3->dom_sid.sid);
		sid_append_rid(&group_sid, info3->gids[i].g_rid);

		if (!add_sid_to_array(mem_ctx, &group_sid, user_sids,
				 &num_groups)) {
			TALLOC_FREE(info3);
			return NT_STATUS_NO_MEMORY;
		}
	}

	TALLOC_FREE(info3);
	*p_num_groups = num_groups;
	status = (user_sids != NULL) ? NT_STATUS_OK : NT_STATUS_NO_MEMORY;
	
	DEBUG(3,(": lookup_usergroups_cached succeeded\n"));

	return status;
}

/*********************************************************************
 We use this to remove spaces from user and group names
********************************************************************/

void ws_name_replace( char *name, char replace )
{
	char replace_char[2] = { 0x0, 0x0 };
    
	if ( !lp_winbind_normalize_names() || (replace == '\0') ) 
		return;

	replace_char[0] = replace;	
	all_string_sub( name, " ", replace_char, 0 );

	return;	
}

/*********************************************************************
 We use this to do the inverse of ws_name_replace()
********************************************************************/

void ws_name_return( char *name, char replace )
{
	char replace_char[2] = { 0x0, 0x0 };
    
	if ( !lp_winbind_normalize_names() || (replace == '\0') ) 
		return;
	
	replace_char[0] = replace;	
	all_string_sub( name, replace_char, " ", 0 );

	return;	
}
