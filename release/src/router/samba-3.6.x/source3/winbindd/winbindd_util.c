/*
   Unix SMB/CIFS implementation.

   Winbind daemon for ntdom nss module

   Copyright (C) Tim Potter 2000-2001
   Copyright (C) 2001 by Martin Pool <mbp@samba.org>

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
#include "winbindd.h"
#include "secrets.h"
#include "../libcli/security/security.h"
#include "../libcli/auth/pam_errors.h"
#include "passdb/machine_sid.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

extern struct winbindd_methods cache_methods;

/**
 * @file winbindd_util.cq
 *
 * Winbind daemon for NT domain authentication nss module.
 **/


/* The list of trusted domains.  Note that the list can be deleted and
   recreated using the init_domain_list() function so pointers to
   individual winbindd_domain structures cannot be made.  Keep a copy of
   the domain name instead. */

static struct winbindd_domain *_domain_list = NULL;

struct winbindd_domain *domain_list(void)
{
	/* Initialise list */

	if ((!_domain_list) && (!init_domain_list())) {
		smb_panic("Init_domain_list failed");
	}

	return _domain_list;
}

/* Free all entries in the trusted domain list */

static void free_domain_list(void)
{
	struct winbindd_domain *domain = _domain_list;

	while(domain) {
		struct winbindd_domain *next = domain->next;

		DLIST_REMOVE(_domain_list, domain);
		SAFE_FREE(domain);
		domain = next;
	}
}

static bool is_internal_domain(const struct dom_sid *sid)
{
	if (sid == NULL)
		return False;

	return (sid_check_is_domain(sid) || sid_check_is_builtin(sid));
}

static bool is_in_internal_domain(const struct dom_sid *sid)
{
	if (sid == NULL)
		return False;

	return (sid_check_is_in_our_domain(sid) || sid_check_is_in_builtin(sid));
}


/* Add a trusted domain to our list of domains */
static struct winbindd_domain *add_trusted_domain(const char *domain_name, const char *alt_name,
						  struct winbindd_methods *methods,
						  const struct dom_sid *sid)
{
	struct winbindd_domain *domain;
	const char *alternative_name = NULL;
	char *idmap_config_option;
	const char *param;
	const char **ignored_domains, **dom;

	ignored_domains = lp_parm_string_list(-1, "winbind", "ignore domains", NULL);
	for (dom=ignored_domains; dom && *dom; dom++) {
		if (gen_fnmatch(*dom, domain_name) == 0) {
			DEBUG(2,("Ignoring domain '%s'\n", domain_name));
			return NULL;
		}
	}

	/* ignore alt_name if we are not in an AD domain */

	if ( (lp_security() == SEC_ADS) && alt_name && *alt_name) {
		alternative_name = alt_name;
	}

	/* We can't call domain_list() as this function is called from
	   init_domain_list() and we'll get stuck in a loop. */
	for (domain = _domain_list; domain; domain = domain->next) {
		if (strequal(domain_name, domain->name) ||
		    strequal(domain_name, domain->alt_name))
		{
			break;
		}

		if (alternative_name && *alternative_name)
		{
			if (strequal(alternative_name, domain->name) ||
			    strequal(alternative_name, domain->alt_name))
			{
				break;
			}
		}

		if (sid)
		{
			if (is_null_sid(sid)) {
				continue;
			}

			if (dom_sid_equal(sid, &domain->sid)) {
				break;
			}
		}
	}

	if (domain != NULL) {
		/*
		 * We found a match. Possibly update the SID
		 */
		if ((sid != NULL)
		    && dom_sid_equal(&domain->sid, &global_sid_NULL)) {
			sid_copy( &domain->sid, sid );
		}
		return domain;
	}

	/* Create new domain entry */

	if ((domain = SMB_MALLOC_P(struct winbindd_domain)) == NULL)
		return NULL;

	/* Fill in fields */

	ZERO_STRUCTP(domain);

	domain->children = SMB_MALLOC_ARRAY(
		struct winbindd_child, lp_winbind_max_domain_connections());
	if (domain->children == NULL) {
		SAFE_FREE(domain);
		return NULL;
	}
	memset(domain->children, 0,
	       sizeof(struct winbindd_child)
	       * lp_winbind_max_domain_connections());

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
	domain->dc_probe_pid = (pid_t)-1;
	if (sid) {
		sid_copy(&domain->sid, sid);
	}

	/* Link to domain list */
	DLIST_ADD_END(_domain_list, domain, struct winbindd_domain *);

	wcache_tdc_add_domain( domain );

	idmap_config_option = talloc_asprintf(talloc_tos(), "idmap config %s",
					      domain->name);
	if (idmap_config_option == NULL) {
		DEBUG(0, ("talloc failed, not looking for idmap config\n"));
		goto done;
	}

	param = lp_parm_const_string(-1, idmap_config_option, "range", NULL);

	DEBUG(10, ("%s : range = %s\n", idmap_config_option,
		   param ? param : "not defined"));

	if (param != NULL) {
		unsigned low_id, high_id;
		if (sscanf(param, "%u - %u", &low_id, &high_id) != 2) {
			DEBUG(1, ("invalid range syntax in %s: %s\n",
				  idmap_config_option, param));
			goto done;
		}
		if (low_id > high_id) {
			DEBUG(1, ("invalid range in %s: %s\n",
				  idmap_config_option, param));
			goto done;
		}
		domain->have_idmap_config = true;
		domain->id_range_low = low_id;
		domain->id_range_high = high_id;
	}

done:

	DEBUG(2,("Added domain %s %s %s\n",
		 domain->name, domain->alt_name,
		 &domain->sid?sid_string_dbg(&domain->sid):""));

	return domain;
}

bool domain_is_forest_root(const struct winbindd_domain *domain)
{
	const uint32_t fr_flags =
		(NETR_TRUST_FLAG_TREEROOT|NETR_TRUST_FLAG_IN_FOREST);

	return ((domain->domain_flags & fr_flags) == fr_flags);
}

/********************************************************************
  rescan our domains looking for new trusted domains
********************************************************************/

struct trustdom_state {
	struct winbindd_domain *domain;
	struct winbindd_request request;
};

static void trustdom_list_done(struct tevent_req *req);
static void rescan_forest_root_trusts( void );
static void rescan_forest_trusts( void );

static void add_trusted_domains( struct winbindd_domain *domain )
{
	struct trustdom_state *state;
	struct tevent_req *req;

	state = TALLOC_ZERO_P(NULL, struct trustdom_state);
	if (state == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return;
	}
	state->domain = domain;

	state->request.length = sizeof(state->request);
	state->request.cmd = WINBINDD_LIST_TRUSTDOM;

	req = wb_domain_request_send(state, winbind_event_context(),
				     domain, &state->request);
	if (req == NULL) {
		DEBUG(1, ("wb_domain_request_send failed\n"));
		TALLOC_FREE(state);
		return;
	}
	tevent_req_set_callback(req, trustdom_list_done, state);
}

static void trustdom_list_done(struct tevent_req *req)
{
	struct trustdom_state *state = tevent_req_callback_data(
		req, struct trustdom_state);
	struct winbindd_response *response;
	int res, err;
	char *p;

	res = wb_domain_request_recv(req, state, &response, &err);
	if ((res == -1) || (response->result != WINBINDD_OK)) {
		DEBUG(1, ("Could not receive trustdoms\n"));
		TALLOC_FREE(state);
		return;
	}

	p = (char *)response->extra_data.data;

	while ((p != NULL) && (*p != '\0')) {
		char *q, *sidstr, *alt_name;
		struct dom_sid sid;
		struct winbindd_domain *domain;
		char *alternate_name = NULL;

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
			DEBUG(0, ("Got invalid trustdom response\n"));
			break;
		}

		/* use the real alt_name if we have one, else pass in NULL */

		if ( !strequal( alt_name, "(null)" ) )
			alternate_name = alt_name;

		/* If we have an existing domain structure, calling
 		   add_trusted_domain() will update the SID if
 		   necessary.  This is important because we need the
 		   SID for sibling domains */

		if ( find_domain_from_name_noinit(p) != NULL ) {
			domain = add_trusted_domain(p, alternate_name,
						    &cache_methods,
						    &sid);
		} else {
			domain = add_trusted_domain(p, alternate_name,
						    &cache_methods,
						    &sid);
			if (domain) {
				setup_domain_child(domain);
			}
		}
		p=q;
		if (p != NULL)
			p += 1;
	}

	/*
	   Cases to consider when scanning trusts:
	   (a) we are calling from a child domain (primary && !forest_root)
	   (b) we are calling from the root of the forest (primary && forest_root)
	   (c) we are calling from a trusted forest domain (!primary
	       && !forest_root)
	*/

	if (state->domain->primary) {
		/* If this is our primary domain and we are not in the
		   forest root, we have to scan the root trusts first */

		if (!domain_is_forest_root(state->domain))
			rescan_forest_root_trusts();
		else
			rescan_forest_trusts();

	} else if (domain_is_forest_root(state->domain)) {
		/* Once we have done root forest trust search, we can
		   go on to search the trusted forests */

		rescan_forest_trusts();
	}

	TALLOC_FREE(state);

	return;
}

/********************************************************************
 Scan the trusts of our forest root
********************************************************************/

static void rescan_forest_root_trusts( void )
{
	struct winbindd_tdc_domain *dom_list = NULL;
        size_t num_trusts = 0;
	int i;

	/* The only transitive trusts supported by Windows 2003 AD are
	   (a) Parent-Child, (b) Tree-Root, and (c) Forest.   The
	   first two are handled in forest and listed by
	   DsEnumerateDomainTrusts().  Forest trusts are not so we
	   have to do that ourselves. */

	if ( !wcache_tdc_fetch_list( &dom_list, &num_trusts ) )
		return;

	for ( i=0; i<num_trusts; i++ ) {
		struct winbindd_domain *d = NULL;

		/* Find the forest root.  Don't necessarily trust
		   the domain_list() as our primary domain may not
		   have been initialized. */

		if ( !(dom_list[i].trust_flags & NETR_TRUST_FLAG_TREEROOT) ) {
			continue;
		}

		/* Here's the forest root */

		d = find_domain_from_name_noinit( dom_list[i].domain_name );

		if ( !d ) {
			d = add_trusted_domain( dom_list[i].domain_name,
						dom_list[i].dns_name,
						&cache_methods,
						&dom_list[i].sid );
			if (d != NULL) {
				setup_domain_child(d);
			}
		}

		if (d == NULL) {
			continue;
		}

       		DEBUG(10,("rescan_forest_root_trusts: Following trust path "
			  "for domain tree root %s (%s)\n",
	       		  d->name, d->alt_name ));

		d->domain_flags = dom_list[i].trust_flags;
		d->domain_type  = dom_list[i].trust_type;
		d->domain_trust_attribs = dom_list[i].trust_attribs;

		add_trusted_domains( d );

		break;
	}

	TALLOC_FREE( dom_list );

	return;
}

/********************************************************************
 scan the transitive forest trusts (not our own)
********************************************************************/


static void rescan_forest_trusts( void )
{
	struct winbindd_domain *d = NULL;
	struct winbindd_tdc_domain *dom_list = NULL;
        size_t num_trusts = 0;
	int i;

	/* The only transitive trusts supported by Windows 2003 AD are
	   (a) Parent-Child, (b) Tree-Root, and (c) Forest.   The
	   first two are handled in forest and listed by
	   DsEnumerateDomainTrusts().  Forest trusts are not so we
	   have to do that ourselves. */

	if ( !wcache_tdc_fetch_list( &dom_list, &num_trusts ) )
		return;

	for ( i=0; i<num_trusts; i++ ) {
		uint32 flags   = dom_list[i].trust_flags;
		uint32 type    = dom_list[i].trust_type;
		uint32 attribs = dom_list[i].trust_attribs;

		d = find_domain_from_name_noinit( dom_list[i].domain_name );

		/* ignore our primary and internal domains */

		if ( d && (d->internal || d->primary ) )
			continue;

		if ( (flags & NETR_TRUST_FLAG_INBOUND) &&
		     (type == NETR_TRUST_TYPE_UPLEVEL) &&
		     (attribs == NETR_TRUST_ATTRIBUTE_FOREST_TRANSITIVE) )
		{
			/* add the trusted domain if we don't know
			   about it */

			if ( !d ) {
				d = add_trusted_domain( dom_list[i].domain_name,
							dom_list[i].dns_name,
							&cache_methods,
							&dom_list[i].sid );
				if (d != NULL) {
					setup_domain_child(d);
				}
			}

			if (d == NULL) {
				continue;
			}

			DEBUG(10,("Following trust path for domain %s (%s)\n",
				  d->name, d->alt_name ));
			add_trusted_domains( d );
		}
	}

	TALLOC_FREE( dom_list );

	return;
}

/*********************************************************************
 The process of updating the trusted domain list is a three step
 async process:
 (a) ask our domain
 (b) ask the root domain in our forest
 (c) ask the a DC in any Win2003 trusted forests
*********************************************************************/

void rescan_trusted_domains(struct tevent_context *ev, struct tevent_timer *te,
			    struct timeval now, void *private_data)
{
	TALLOC_FREE(te);

	/* I use to clear the cache here and start over but that
	   caused problems in child processes that needed the
	   trust dom list early on.  Removing it means we
	   could have some trusted domains listed that have been
	   removed from our primary domain's DC until a full
	   restart.  This should be ok since I think this is what
	   Windows does as well. */

	/* this will only add new domains we didn't already know about
	   in the domain_list()*/

	add_trusted_domains( find_our_domain() );

	te = tevent_add_timer(
		ev, NULL, timeval_current_ofs(WINBINDD_RESCAN_FREQ, 0),
		rescan_trusted_domains, NULL);
	/*
	 * If te == NULL, there's not much we can do here. Don't fail, the
	 * only thing we miss is new trusted domains.
	 */

	return;
}

enum winbindd_result winbindd_dual_init_connection(struct winbindd_domain *domain,
						   struct winbindd_cli_state *state)
{
	/* Ensure null termination */
	state->request->domain_name
		[sizeof(state->request->domain_name)-1]='\0';
	state->request->data.init_conn.dcname
		[sizeof(state->request->data.init_conn.dcname)-1]='\0';

	if (strlen(state->request->data.init_conn.dcname) > 0) {
		fstrcpy(domain->dcname, state->request->data.init_conn.dcname);
	}

	if (domain->internal) {
		domain->initialized = true;
	} else {
		init_dc_connection(domain);
	}

	if (!domain->initialized) {
		/* If we return error here we can't do any cached authentication,
		   but we may be in disconnected mode and can't initialize correctly.
		   Do what the previous code did and just return without initialization,
		   once we go online we'll re-initialize.
		*/
		DEBUG(5, ("winbindd_dual_init_connection: %s returning without initialization "
			"online = %d\n", domain->name, (int)domain->online ));
	}

	fstrcpy(state->response->data.domain_info.name, domain->name);
	fstrcpy(state->response->data.domain_info.alt_name, domain->alt_name);
	sid_to_fstring(state->response->data.domain_info.sid, &domain->sid);

	state->response->data.domain_info.native_mode
		= domain->native_mode;
	state->response->data.domain_info.active_directory
		= domain->active_directory;
	state->response->data.domain_info.primary
		= domain->primary;

	return WINBINDD_OK;
}

/* Look up global info for the winbind daemon */
bool init_domain_list(void)
{
	struct winbindd_domain *domain;
	int role = lp_server_role();

	/* Free existing list */
	free_domain_list();

	/* BUILTIN domain */

	domain = add_trusted_domain("BUILTIN", NULL, &cache_methods,
				    &global_sid_Builtin);
	if (domain) {
		setup_domain_child(domain);
	}

	/* Local SAM */

	domain = add_trusted_domain(get_global_sam_name(), NULL,
				    &cache_methods, get_global_sam_sid());
	if (domain) {
		if ( role != ROLE_DOMAIN_MEMBER ) {
			domain->primary = True;
		}
		setup_domain_child(domain);
	}

	/* Add ourselves as the first entry. */

	if ( role == ROLE_DOMAIN_MEMBER ) {
		struct dom_sid our_sid;

		if (!secrets_fetch_domain_sid(lp_workgroup(), &our_sid)) {
			DEBUG(0, ("Could not fetch our SID - did we join?\n"));
			return False;
		}

		domain = add_trusted_domain( lp_workgroup(), lp_realm(),
					     &cache_methods, &our_sid);
		if (domain) {
			domain->primary = True;
			setup_domain_child(domain);

			/* Even in the parent winbindd we'll need to
			   talk to the DC, so try and see if we can
			   contact it. Theoretically this isn't neccessary
			   as the init_dc_connection() in init_child_recv()
			   will do this, but we can start detecting the DC
			   early here. */
			set_domain_online_request(domain);
		}
	}

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

struct winbindd_domain *find_domain_from_sid_noinit(const struct dom_sid *sid)
{
	struct winbindd_domain *domain;

	/* Search through list */

	for (domain = domain_list(); domain != NULL; domain = domain->next) {
		if (dom_sid_compare_domain(sid, &domain->sid) == 0)
			return domain;
	}

	/* Not found */

	return NULL;
}

/* Given a domain sid, return the struct winbindd domain info for it */

struct winbindd_domain *find_domain_from_sid(const struct dom_sid *sid)
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

	smb_panic("Could not find our domain");
	return NULL;
}

struct winbindd_domain *find_root_domain(void)
{
	struct winbindd_domain *ours = find_our_domain();

	if (ours->forest_name[0] == '\0') {
		return NULL;
	}

	return find_domain_from_name( ours->forest_name );
}

struct winbindd_domain *find_builtin_domain(void)
{
	struct winbindd_domain *domain;

	domain = find_domain_from_sid(&global_sid_Builtin);
	if (domain == NULL) {
		smb_panic("Could not find BUILTIN domain");
	}

	return domain;
}

/* Find the appropriate domain to lookup a name or SID */

struct winbindd_domain *find_lookup_domain_from_sid(const struct dom_sid *sid)
{
	/* SIDs in the S-1-22-{1,2} domain should be handled by our passdb */

	if ( sid_check_is_in_unix_groups(sid) ||
	     sid_check_is_unix_groups(sid) ||
	     sid_check_is_in_unix_users(sid) ||
	     sid_check_is_unix_users(sid) )
	{
		return find_domain_from_sid(get_global_sam_sid());
	}

	/* A DC can't ask the local smbd for remote SIDs, here winbindd is the
	 * one to contact the external DC's. On member servers the internal
	 * domains are different: These are part of the local SAM. */

	DEBUG(10, ("find_lookup_domain_from_sid(%s)\n", sid_string_dbg(sid)));

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
	if ( strequal(domain_name, unix_users_domain_name() ) ||
	     strequal(domain_name, unix_groups_domain_name() ) )
	{
		/*
		 * The "Unix User" and "Unix Group" domain our handled by
		 * passdb
		 */
		return find_domain_from_name_noinit( get_global_sam_name() );
	}

	if (IS_DC || strequal(domain_name, "BUILTIN") ||
	    strequal(domain_name, get_global_sam_name()))
		return find_domain_from_name_noinit(domain_name);


	return find_our_domain();
}

/* Is this a domain which we may assume no DOMAIN\ prefix? */

static bool assume_domain(const char *domain)
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

bool parse_domain_user(const char *domuser, fstring domain, fstring user)
{
	char *p = strchr(domuser,*lp_winbind_separator());

	if ( !p ) {
		fstrcpy(user, domuser);

		if ( assume_domain(lp_workgroup())) {
			fstrcpy(domain, lp_workgroup());
		} else if ((p = strchr(domuser, '@')) != NULL) {
			fstrcpy(domain, p + 1);
			user[PTR_DIFF(p, domuser)] = 0;
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

bool parse_domain_user_talloc(TALLOC_CTX *mem_ctx, const char *domuser,
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

bool canonicalize_username(fstring username_inout, fstring domain, fstring user)
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
void fill_domain_username(fstring name, const char *domain, const char *user, bool can_assume)
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

/**
 * talloc version of fill_domain_username()
 * return NULL on talloc failure.
 */
char *fill_domain_username_talloc(TALLOC_CTX *mem_ctx,
				  const char *domain,
				  const char *user,
				  bool can_assume)
{
	char *tmp_user, *name;

	tmp_user = talloc_strdup(mem_ctx, user);
	strlower_m(tmp_user);

	if (can_assume && assume_domain(domain)) {
		name = tmp_user;
	} else {
		name = talloc_asprintf(mem_ctx, "%s%c%s",
				       domain,
				       *lp_winbind_separator(),
				       tmp_user);
		TALLOC_FREE(tmp_user);
	}

	return name;
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

/* Return number of open clients */

int winbindd_num_clients(void)
{
	return _num_clients;
}

NTSTATUS lookup_usergroups_cached(struct winbindd_domain *domain,
				  TALLOC_CTX *mem_ctx,
				  const struct dom_sid *user_sid,
				  uint32_t *p_num_groups, struct dom_sid **user_sids)
{
	struct netr_SamInfo3 *info3 = NULL;
	NTSTATUS status = NT_STATUS_NO_MEMORY;
	uint32_t num_groups = 0;

	DEBUG(3,(": lookup_usergroups_cached\n"));

	*user_sids = NULL;
	*p_num_groups = 0;

	info3 = netsamlogon_cache_get(mem_ctx, user_sid);

	if (info3 == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (info3->base.groups.count == 0) {
		TALLOC_FREE(info3);
		return NT_STATUS_UNSUCCESSFUL;
	}

	/*
	 * Before bug #7843 the "Domain Local" groups were added with a
	 * lookupuseraliases call, but this isn't done anymore for our domain
	 * so we need to resolve resource groups here.
	 *
	 * When to use Resource Groups:
	 * http://technet.microsoft.com/en-us/library/cc753670%28v=WS.10%29.aspx
	 */
	status = sid_array_from_info3(mem_ctx, info3,
				      user_sids,
				      &num_groups,
				      false);

	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(info3);
		return status;
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

NTSTATUS normalize_name_map(TALLOC_CTX *mem_ctx,
			     struct winbindd_domain *domain,
			     const char *name,
			     char **normalized)
{
	NTSTATUS nt_status;

	if (!name || !normalized) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!lp_winbind_normalize_names()) {
		return NT_STATUS_PROCEDURE_NOT_FOUND;
	}

	/* Alias support and whitespace replacement are mutually
	   exclusive */

	nt_status = resolve_username_to_alias(mem_ctx, domain,
					      name, normalized );
	if (NT_STATUS_IS_OK(nt_status)) {
		/* special return code to let the caller know we
		   mapped to an alias */
		return NT_STATUS_FILE_RENAMED;
	}

	/* check for an unreachable domain */

	if (NT_STATUS_EQUAL(nt_status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		DEBUG(5,("normalize_name_map: Setting domain %s offline\n",
			 domain->name));
		set_domain_offline(domain);
		return nt_status;
	}

	/* deal with whitespace */

	*normalized = talloc_strdup(mem_ctx, name);
	if (!(*normalized)) {
		return NT_STATUS_NO_MEMORY;
	}

	all_string_sub( *normalized, " ", "_", 0 );

	return NT_STATUS_OK;
}

/*********************************************************************
 We use this to do the inverse of normalize_name_map()
********************************************************************/

NTSTATUS normalize_name_unmap(TALLOC_CTX *mem_ctx,
			      char *name,
			      char **normalized)
{
	NTSTATUS nt_status;
	struct winbindd_domain *domain = find_our_domain();

	if (!name || !normalized) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!lp_winbind_normalize_names()) {
		return NT_STATUS_PROCEDURE_NOT_FOUND;
	}

	/* Alias support and whitespace replacement are mutally
	   exclusive */

	/* When mapping from an alias to a username, we don't know the
	   domain.  But we only need a domain structure to cache
	   a successful lookup , so just our own domain structure for
	   the seqnum. */

	nt_status = resolve_alias_to_username(mem_ctx, domain,
					      name, normalized);
	if (NT_STATUS_IS_OK(nt_status)) {
		/* Special return code to let the caller know we mapped
		   from an alias */
		return NT_STATUS_FILE_RENAMED;
	}

	/* check for an unreachable domain */

	if (NT_STATUS_EQUAL(nt_status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		DEBUG(5,("normalize_name_unmap: Setting domain %s offline\n",
			 domain->name));
		set_domain_offline(domain);
		return nt_status;
	}

	/* deal with whitespace */

	*normalized = talloc_strdup(mem_ctx, name);
	if (!(*normalized)) {
		return NT_STATUS_NO_MEMORY;
	}

	all_string_sub(*normalized, "_", " ", 0);

	return NT_STATUS_OK;
}

/*********************************************************************
 ********************************************************************/

bool winbindd_can_contact_domain(struct winbindd_domain *domain)
{
	struct winbindd_tdc_domain *tdc = NULL;
	TALLOC_CTX *frame = talloc_stackframe();
	bool ret = false;

	/* We can contact the domain if it is our primary domain */

	if (domain->primary) {
		return true;
	}

	/* Trust the TDC cache and not the winbindd_domain flags */

	if ((tdc = wcache_tdc_fetch_domain(frame, domain->name)) == NULL) {
		DEBUG(10,("winbindd_can_contact_domain: %s not found in cache\n",
			  domain->name));
		return false;
	}

	/* Can always contact a domain that is in out forest */

	if (tdc->trust_flags & NETR_TRUST_FLAG_IN_FOREST) {
		ret = true;
		goto done;
	}

	/*
	 * On a _member_ server, we cannot contact the domain if it
	 * is running AD and we have no inbound trust.
	 */

	if (!IS_DC &&
	     domain->active_directory &&
	    ((tdc->trust_flags & NETR_TRUST_FLAG_INBOUND) != NETR_TRUST_FLAG_INBOUND))
	{
		DEBUG(10, ("winbindd_can_contact_domain: %s is an AD domain "
			   "and we have no inbound trust.\n", domain->name));
		goto done;
	}

	/* Assume everything else is ok (probably not true but what
	   can you do?) */

	ret = true;

done:
	talloc_destroy(frame);

	return ret;
}

/*********************************************************************
 ********************************************************************/

bool winbindd_internal_child(struct winbindd_child *child)
{
	if ((child == idmap_child()) || (child == locator_child())) {
		return True;
	}

	return False;
}

#ifdef HAVE_KRB5_LOCATE_PLUGIN_H

/*********************************************************************
 ********************************************************************/

static void winbindd_set_locator_kdc_env(const struct winbindd_domain *domain)
{
	char *var = NULL;
	char addr[INET6_ADDRSTRLEN];
	const char *kdc = NULL;
	int lvl = 11;

	if (!domain || !domain->alt_name || !*domain->alt_name) {
		return;
	}

	if (domain->initialized && !domain->active_directory) {
		DEBUG(lvl,("winbindd_set_locator_kdc_env: %s not AD\n",
			domain->alt_name));
		return;
	}

	print_sockaddr(addr, sizeof(addr), &domain->dcaddr);
	kdc = addr;
	if (!*kdc) {
		DEBUG(lvl,("winbindd_set_locator_kdc_env: %s no DC IP\n",
			domain->alt_name));
		kdc = domain->dcname;
	}

	if (!kdc || !*kdc) {
		DEBUG(lvl,("winbindd_set_locator_kdc_env: %s no DC at all\n",
			domain->alt_name));
		return;
	}

	if (asprintf_strupper_m(&var, "%s_%s", WINBINDD_LOCATOR_KDC_ADDRESS,
				domain->alt_name) == -1) {
		return;
	}

	DEBUG(lvl,("winbindd_set_locator_kdc_env: setting var: %s to: %s\n",
		var, kdc));

	setenv(var, kdc, 1);
	free(var);
}

/*********************************************************************
 ********************************************************************/

void winbindd_set_locator_kdc_envs(const struct winbindd_domain *domain)
{
	struct winbindd_domain *our_dom = find_our_domain();

	winbindd_set_locator_kdc_env(domain);

	if (domain != our_dom) {
		winbindd_set_locator_kdc_env(our_dom);
	}
}

/*********************************************************************
 ********************************************************************/

void winbindd_unset_locator_kdc_env(const struct winbindd_domain *domain)
{
	char *var = NULL;

	if (!domain || !domain->alt_name || !*domain->alt_name) {
		return;
	}

	if (asprintf_strupper_m(&var, "%s_%s", WINBINDD_LOCATOR_KDC_ADDRESS,
				domain->alt_name) == -1) {
		return;
	}

	unsetenv(var);
	free(var);
}
#else

void winbindd_set_locator_kdc_envs(const struct winbindd_domain *domain)
{
	return;
}

void winbindd_unset_locator_kdc_env(const struct winbindd_domain *domain)
{
	return;
}

#endif /* HAVE_KRB5_LOCATE_PLUGIN_H */

void set_auth_errors(struct winbindd_response *resp, NTSTATUS result)
{
	resp->data.auth.nt_status = NT_STATUS_V(result);
	fstrcpy(resp->data.auth.nt_status_string, nt_errstr(result));

	/* we might have given a more useful error above */
	if (*resp->data.auth.error_string == '\0')
		fstrcpy(resp->data.auth.error_string,
			get_friendly_nt_error_msg(result));
	resp->data.auth.pam_error = nt_status_to_pam(result);
}

bool is_domain_offline(const struct winbindd_domain *domain)
{
	if (!lp_winbind_offline_logon()) {
		return false;
	}
	if (get_global_winbindd_state_offline()) {
		return true;
	}
	return !domain->online;
}

bool is_domain_online(const struct winbindd_domain *domain)
{
	return !is_domain_offline(domain);
}

bool parse_sidlist(TALLOC_CTX *mem_ctx, const char *sidstr,
		   struct dom_sid **sids, uint32_t *num_sids)
{
	const char *p;

	p = sidstr;
	if (p == NULL)
		return False;

	while (p[0] != '\0') {
		struct dom_sid sid;
		const char *q = NULL;

		if (!dom_sid_parse_endp(p, &sid, &q)) {
			DEBUG(1, ("Could not parse sid %s\n", p));
			return false;
		}
		if ((q == NULL) || (q[0] != '\n')) {
			DEBUG(1, ("Got invalid sidstr: %s\n", p));
			return false;
		}
		if (!NT_STATUS_IS_OK(add_sid_to_array(mem_ctx, &sid, sids,
						      num_sids)))
		{
			return False;
		}
		p = q+1;
	}
	return True;
}
