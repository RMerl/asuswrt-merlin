/*
   Unix SMB/CIFS implementation.
   ID Mapping
   Copyright (C) Tim Potter 2000
   Copyright (C) Jim McDonough <jmcd@us.ibm.com>	2003
   Copyright (C) Simo Sorce 2003-2007
   Copyright (C) Jeremy Allison 2006

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
#define DBGC_CLASS DBGC_IDMAP

static_decl_idmap;

struct idmap_backend {
	const char *name;
	struct idmap_methods *methods;
	struct idmap_backend *prev, *next;
};

struct idmap_alloc_backend {
	const char *name;
	struct idmap_alloc_methods *methods;
	struct idmap_alloc_backend *prev, *next;
};

struct idmap_cache_ctx;

struct idmap_alloc_context {
	const char *params;
	struct idmap_alloc_methods *methods;
	BOOL initialized;
};

static TALLOC_CTX *idmap_ctx = NULL;
static struct idmap_cache_ctx *idmap_cache;

static struct idmap_backend *backends = NULL;
static struct idmap_domain **idmap_domains = NULL;
static int num_domains = 0;
static int pdb_dom_num = -1;
static int def_dom_num = -1;

static struct idmap_alloc_backend *alloc_backends = NULL;
static struct idmap_alloc_context *idmap_alloc_ctx = NULL;

#define IDMAP_CHECK_RET(ret) do { \
	if ( ! NT_STATUS_IS_OK(ret)) { \
		DEBUG(2, ("ERROR: NTSTATUS = 0x%08x\n", NT_STATUS_V(ret))); \
			goto done; \
	} } while(0)
#define IDMAP_REPORT_RET(ret) do { \
	if ( ! NT_STATUS_IS_OK(ret)) { \
		DEBUG(2, ("ERROR: NTSTATUS = 0x%08x\n", NT_STATUS_V(ret))); \
	} } while(0)
#define IDMAP_CHECK_ALLOC(mem) do { \
	if (!mem) { \
		DEBUG(0, ("Out of memory!\n")); ret = NT_STATUS_NO_MEMORY; \
		goto done; \
	} } while(0)

static struct idmap_methods *get_methods(struct idmap_backend *be,
					 const char *name)
{
	struct idmap_backend *b;

	for (b = be; b; b = b->next) {
		if (strequal(b->name, name)) {
			return b->methods;
		}
	}

	return NULL;
}

static struct idmap_alloc_methods *get_alloc_methods(
						struct idmap_alloc_backend *be,
						const char *name)
{
	struct idmap_alloc_backend *b;

	for (b = be; b; b = b->next) {
		if (strequal(b->name, name)) {
			return b->methods;
		}
	}

	return NULL;
}

BOOL idmap_is_offline(void)
{
	return ( lp_winbind_offline_logon() &&
	     get_global_winbindd_state_offline() );
}

/**********************************************************************
 Allow a module to register itself as a method.
**********************************************************************/

NTSTATUS smb_register_idmap(int version, const char *name,
			    struct idmap_methods *methods)
{
	struct idmap_methods *test;
	struct idmap_backend *entry;

	if (!idmap_ctx) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

 	if ((version != SMB_IDMAP_INTERFACE_VERSION)) {
		DEBUG(0, ("Failed to register idmap module.\n"
		          "The module was compiled against "
			  "SMB_IDMAP_INTERFACE_VERSION %d,\n"
		          "current SMB_IDMAP_INTERFACE_VERSION is %d.\n"
		          "Please recompile against the current version "
			  "of samba!\n",
			  version, SMB_IDMAP_INTERFACE_VERSION));
		return NT_STATUS_OBJECT_TYPE_MISMATCH;
  	}

	if (!name || !name[0] || !methods) {
		DEBUG(0,("Called with NULL pointer or empty name!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	test = get_methods(backends, name);
	if (test) {
		DEBUG(0,("Idmap module %s already registered!\n", name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	entry = talloc(idmap_ctx, struct idmap_backend);
	if ( ! entry) {
		DEBUG(0,("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}
	entry->name = talloc_strdup(idmap_ctx, name);
	if ( ! entry->name) {
		DEBUG(0,("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}
	entry->methods = methods;

	DLIST_ADD(backends, entry);
	DEBUG(5, ("Successfully added idmap backend '%s'\n", name));
	return NT_STATUS_OK;
}

/**********************************************************************
 Allow a module to register itself as a method.
**********************************************************************/

NTSTATUS smb_register_idmap_alloc(int version, const char *name,
				  struct idmap_alloc_methods *methods)
{
	struct idmap_alloc_methods *test;
	struct idmap_alloc_backend *entry;

	if (!idmap_ctx) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

 	if ((version != SMB_IDMAP_INTERFACE_VERSION)) {
		DEBUG(0, ("Failed to register idmap alloc module.\n"
		          "The module was compiled against "
			  "SMB_IDMAP_INTERFACE_VERSION %d,\n"
		          "current SMB_IDMAP_INTERFACE_VERSION is %d.\n"
		          "Please recompile against the current version "
			  "of samba!\n",
			  version, SMB_IDMAP_INTERFACE_VERSION));
		return NT_STATUS_OBJECT_TYPE_MISMATCH;
  	}

	if (!name || !name[0] || !methods) {
		DEBUG(0,("Called with NULL pointer or empty name!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	test = get_alloc_methods(alloc_backends, name);
	if (test) {
		DEBUG(0,("idmap_alloc module %s already registered!\n", name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	entry = talloc(idmap_ctx, struct idmap_alloc_backend);
	if ( ! entry) {
		DEBUG(0,("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}
	entry->name = talloc_strdup(idmap_ctx, name);
	if ( ! entry->name) {
		DEBUG(0,("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}
	entry->methods = methods;

	DLIST_ADD(alloc_backends, entry);
	DEBUG(5, ("Successfully added idmap alloc backend '%s'\n", name));
	return NT_STATUS_OK;
}

static int close_domain_destructor(struct idmap_domain *dom)
{
	NTSTATUS ret;

	ret = dom->methods->close_fn(dom);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(3, ("Failed to close idmap domain [%s]!\n", dom->name));
	}

	return 0;
}

/**************************************************************************
 Shutdown.
**************************************************************************/

NTSTATUS idmap_close(void)
{
	/* close the alloc backend first before freeing idmap_ctx */
	if (idmap_alloc_ctx) {
		idmap_alloc_ctx->methods->close_fn();
		idmap_alloc_ctx->methods = NULL;
	}
	alloc_backends = NULL;

	/* this talloc_free call will fire the talloc destructors
	 * that will free all active backends resources */
	TALLOC_FREE(idmap_ctx);
	idmap_cache = NULL;
	idmap_domains = NULL;
	backends = NULL;

	return NT_STATUS_OK;
}

/****************************************************************************
 ****************************************************************************/

NTSTATUS idmap_init_cache(void)
{
	/* Always initialize the cache.  We'll have to delay initialization
	   of backends if we are offline */

	if ( idmap_ctx ) {
		return NT_STATUS_OK;
	}

	if ( (idmap_ctx = talloc_named_const(NULL, 0, "idmap_ctx")) == NULL ) {
		return NT_STATUS_NO_MEMORY;
	}

	if ( (idmap_cache = idmap_cache_init(idmap_ctx)) == NULL ) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 ****************************************************************************/

NTSTATUS idmap_init(void)
{
	NTSTATUS ret;
	static NTSTATUS idmap_init_status = NT_STATUS_UNSUCCESSFUL;
	struct idmap_domain *dom;
	char *compat_backend = NULL;
	char *compat_params = NULL;
	const char **dom_list = NULL;
	const char *default_domain = NULL;
	char *alloc_backend = NULL;
	BOOL default_already_defined = False;
	BOOL pri_dom_is_in_list = False;
	int compat = 0;
	int i;

	ret = idmap_init_cache();
	if (!NT_STATUS_IS_OK(ret))
		return ret;

	if (NT_STATUS_IS_OK(idmap_init_status))
		return NT_STATUS_OK;

	static_init_idmap;

	dom_list = lp_idmap_domains();

	if ( lp_idmap_backend() ) {
       		const char **compat_list = lp_idmap_backend();
		char *p = NULL;
		const char *q = NULL;

		if (dom_list) {
			DEBUG(0, ("WARNING: idmap backend and idmap domains "
				  "are mutually excusive!\n"));
			DEBUGADD(0,("idmap backend option will be IGNORED!\n"));
		} else {
			compat = 1;

			/* strip any leading idmap_ prefix of */
			if (strncmp(*compat_list, "idmap_", 6) == 0 ) {
				q = *compat_list += 6;
				DEBUG(0, ("WARNING: idmap backend uses obsolete"
					  " and deprecated 'idmap_' prefix.\n"
					  "Please replace 'idmap_%s' by '%s' in"
					  " %s\n", q, q, dyn_CONFIGFILE));
				compat_backend = talloc_strdup(idmap_ctx, q);
			} else {
				compat_backend = talloc_strdup(idmap_ctx,
								*compat_list);
			}

			if (compat_backend == NULL) {
				ret = NT_STATUS_NO_MEMORY;
				goto done;
			}

			/* separate the backend and module arguements */
			if ((p = strchr(compat_backend, ':')) != NULL) {
				*p = '\0';
				compat_params = p + 1;
			}
		}
	} else if ( !dom_list ) {
		/* Back compatible: without idmap domains and explicit
		   idmap backend.  Taking default idmap backend: tdb */

		compat = 1;
		compat_backend = talloc_strdup( idmap_ctx, "tdb");
		compat_params = compat_backend;
	}

	if ( ! dom_list) {
		/* generate a list with our main domain */
		char ** dl;

		dl = talloc_array(idmap_ctx, char *, 2);
		if (dl == NULL) {
			ret = NT_STATUS_NO_MEMORY;
			goto done;
		}
		dl[0] = talloc_strdup(dl, lp_workgroup());
		if (dl[0] == NULL) {
			ret = NT_STATUS_NO_MEMORY;
			goto done;
		}

		/* terminate */
		dl[1] = NULL;

		dom_list = (const char **)dl;
		default_domain = dl[0];
	}

	/***************************
	 * initialize idmap domains
	 */
	DEBUG(1, ("Initializing idmap domains\n"));

	for (i=0, num_domains=0; dom_list[i]; i++) {
	       	const char *parm_backend;
		char *config_option;

		/* ignore BUILTIN and local MACHINE domains */
		if (strequal(dom_list[i], "BUILTIN")
		     || strequal(dom_list[i], get_global_sam_name()))
		{
			DEBUG(0,("idmap_init: Ignoring domain %s\n",
				 dom_list[i]));
			continue;
		}

		if ((dom_list[i] != default_domain) &&
		    strequal(dom_list[i], lp_workgroup())) {
			pri_dom_is_in_list = True;
		}
		/* init domain */

		dom = TALLOC_ZERO_P(idmap_ctx, struct idmap_domain);
		IDMAP_CHECK_ALLOC(dom);

		dom->name = talloc_strdup(dom, dom_list[i]);
		IDMAP_CHECK_ALLOC(dom->name);

		config_option = talloc_asprintf(dom, "idmap config %s",
						dom->name);
		IDMAP_CHECK_ALLOC(config_option);

		/* default or specific ? */

		dom->default_domain = lp_parm_bool(-1, config_option,
						   "default", False);

		if (dom->default_domain ||
		    (default_domain && strequal(dom_list[i], default_domain))) {

			/* make sure this is set even when we match
			 * default_domain */
			dom->default_domain = True;

			if (default_already_defined) {
				DEBUG(1, ("ERROR: Multiple domains defined as"
					  " default!\n"));
				ret = NT_STATUS_INVALID_PARAMETER;
				goto done;
			}

			default_already_defined = True;

		}

		dom->readonly = lp_parm_bool(-1, config_option,
					     "readonly", False);

		/* find associated backend (default: tdb) */
		if (compat) {
			parm_backend = talloc_strdup(idmap_ctx, compat_backend);
		} else {
			parm_backend = talloc_strdup(idmap_ctx,
						     lp_parm_const_string(
							-1, config_option,
							"backend", "tdb"));
		}
		IDMAP_CHECK_ALLOC(parm_backend);

		/* get the backend methods for this domain */
		dom->methods = get_methods(backends, parm_backend);

		if ( ! dom->methods) {
			ret = smb_probe_module("idmap", parm_backend);
			if (NT_STATUS_IS_OK(ret)) {
				dom->methods = get_methods(backends,
							   parm_backend);
			}
		}
		if ( ! dom->methods) {
			DEBUG(0, ("ERROR: Could not get methods for "
				  "backend %s\n", parm_backend));
			ret = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}

		/* check the set_mapping function exists otherwise mark the
		 * module as readonly */
		if ( ! dom->methods->set_mapping) {
			DEBUG(5, ("Forcing to readonly, as this module can't"
				  " store arbitrary mappings.\n"));
			dom->readonly = True;
		}

		/* now that we have methods,
		 * set the destructor for this domain */
		talloc_set_destructor(dom, close_domain_destructor);

		if (compat_params) {
			dom->params = talloc_strdup(dom, compat_params);
			IDMAP_CHECK_ALLOC(dom->params);
		} else {
			dom->params = NULL;
		}

		/* Finally instance a backend copy for this domain */
		ret = dom->methods->init(dom);
		if ( ! NT_STATUS_IS_OK(ret)) {
			DEBUG(0, ("ERROR: Initialization failed for backend "
				  "%s (domain %s), deferred!\n",
				  parm_backend, dom->name));
		}
		idmap_domains = talloc_realloc(idmap_ctx, idmap_domains,
						struct idmap_domain *, i+1);
		if ( ! idmap_domains) {
			DEBUG(0, ("Out of memory!\n"));
			ret = NT_STATUS_NO_MEMORY;
			goto done;
		}
		idmap_domains[num_domains] = dom;

		/* save default domain position for future uses */
		if (dom->default_domain) {
			def_dom_num = num_domains;
		}

		/* Bump counter to next available slot */

		num_domains++;

		DEBUG(10, ("Domain %s - Backend %s - %sdefault - %sreadonly\n",
				dom->name, parm_backend,
				dom->default_domain?"":"not ",
				dom->readonly?"":"not "));

		talloc_free(config_option);
	}

	/* on DCs we need to add idmap_tdb as the default backend if compat is
	 * defined (when the old implicit configuration is used)
	 * This is not done in the previous loop a on member server we exclude
	 * the local domain. But on a DC the local domain is the only domain
	 * available therefore we are left with no default domain */
	if (((lp_server_role() == ROLE_DOMAIN_PDC) ||
	     (lp_server_role() == ROLE_DOMAIN_BDC)) &&
            ((num_domains == 0) && (compat == 1))) {

		dom = TALLOC_ZERO_P(idmap_ctx, struct idmap_domain);
		IDMAP_CHECK_ALLOC(dom);

		dom->name = talloc_strdup(dom, "__default__");
		IDMAP_CHECK_ALLOC(dom->name);

		dom->default_domain = True;
		dom->readonly = False;

		/* get the backend methods for this domain */
		dom->methods = get_methods(backends, compat_backend);

		if ( ! dom->methods) {
			ret = smb_probe_module("idmap", compat_backend);
			if (NT_STATUS_IS_OK(ret)) {
				dom->methods = get_methods(backends,
							   compat_backend);
			}
		}
		if ( ! dom->methods) {
			DEBUG(0, ("ERROR: Could not get methods for "
				  "backend %s\n", compat_backend));
			ret = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}

		/* now that we have methods,
		 * set the destructor for this domain */
		talloc_set_destructor(dom, close_domain_destructor);

		dom->params = talloc_strdup(dom, compat_params);
		IDMAP_CHECK_ALLOC(dom->params);

		/* Finally instance a backend copy for this domain */
		ret = dom->methods->init(dom);
		if ( ! NT_STATUS_IS_OK(ret)) {
			DEBUG(0, ("ERROR: Initialization failed for backend "
				  "%s (domain %s), deferred!\n",
				  compat_backend, dom->name));
		}
		idmap_domains = talloc_realloc(idmap_ctx, idmap_domains,
						struct idmap_domain *, 2);
		if ( ! idmap_domains) {
			DEBUG(0, ("Out of memory!\n"));
			ret = NT_STATUS_NO_MEMORY;
			goto done;
		}
		idmap_domains[num_domains] = dom;

		def_dom_num = num_domains;

		/* Bump counter to next available slot */

		num_domains++;

		DEBUG(10, ("Domain %s - Backend %s - %sdefault - %sreadonly\n",
				dom->name, compat_backend,
				dom->default_domain?"":"not ",
				dom->readonly?"":"not "));
	}

	/* automatically add idmap_nss backend if needed */
	if ((lp_server_role() == ROLE_DOMAIN_MEMBER) &&
	    ( ! pri_dom_is_in_list) &&
	    lp_winbind_trusted_domains_only()) {

		dom = TALLOC_ZERO_P(idmap_ctx, struct idmap_domain);
		IDMAP_CHECK_ALLOC(dom);

		dom->name = talloc_strdup(dom, lp_workgroup());
		IDMAP_CHECK_ALLOC(dom->name);

		dom->default_domain = False;
		dom->readonly = True;

		/* get the backend methods for passdb */
		dom->methods = get_methods(backends, "nss");

		/* (the nss module is always statically linked) */
		if ( ! dom->methods) {
			DEBUG(0, ("ERROR: No methods for idmap_nss ?!\n"));
			ret = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}

		/* now that we have methods,
		 * set the destructor for this domain */
		talloc_set_destructor(dom, close_domain_destructor);

		if (compat_params) {
			dom->params = talloc_strdup(dom, compat_params);
			IDMAP_CHECK_ALLOC(dom->params);
		} else {
			dom->params = NULL;
		}

		/* Finally instance a backend copy for this domain */
		ret = dom->methods->init(dom);
		if ( ! NT_STATUS_IS_OK(ret)) {
			DEBUG(0, ("ERROR: Init. failed for idmap_nss ?!\n"));
			ret = NT_STATUS_UNSUCCESSFUL;
			goto done;
		}

		idmap_domains = talloc_realloc(idmap_ctx,
						idmap_domains,
						struct idmap_domain *,
						num_domains+1);
		if ( ! idmap_domains) {
			DEBUG(0, ("Out of memory!\n"));
			ret = NT_STATUS_NO_MEMORY;
			goto done;
		}
		idmap_domains[num_domains] = dom;

		DEBUG(10, ("Domain %s - Backend nss - not default - readonly\n",
			   dom->name ));

		num_domains++;
	}

	/**** automatically add idmap_passdb backend ****/
	dom = TALLOC_ZERO_P(idmap_ctx, struct idmap_domain);
	IDMAP_CHECK_ALLOC(dom);

	dom->name = talloc_strdup(dom, get_global_sam_name());
	IDMAP_CHECK_ALLOC(dom->name);

	dom->default_domain = False;
	dom->readonly = True;

	/* get the backend methods for passdb */
	dom->methods = get_methods(backends, "passdb");

	/* (the passdb module is always statically linked) */
	if ( ! dom->methods) {
		DEBUG(0, ("ERROR: No methods for idmap_passdb ?!\n"));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	/* now that we have methods, set the destructor for this domain */
	talloc_set_destructor(dom, close_domain_destructor);

	if (compat_params) {
		dom->params = talloc_strdup(dom, compat_params);
		IDMAP_CHECK_ALLOC(dom->params);
	} else {
		dom->params = NULL;
	}

	/* Finally instance a backend copy for this domain */
	ret = dom->methods->init(dom);
	if ( ! NT_STATUS_IS_OK(ret)) {
		DEBUG(0, ("ERROR: Init. failed for idmap_passdb ?!\n"));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	idmap_domains = talloc_realloc(idmap_ctx,
					idmap_domains,
					struct idmap_domain *,
					num_domains+1);
	if ( ! idmap_domains) {
		DEBUG(0, ("Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}
	idmap_domains[num_domains] = dom;

	/* needed to handle special BUILTIN and wellknown SIDs cases */
	pdb_dom_num = num_domains;

	DEBUG(10, ("Domain %s - Backend passdb - not default - readonly\n",
		   dom->name));

	num_domains++;
	/**** finished adding idmap_passdb backend ****/

	/* sort domains so that the default is the last one */
	/* don't sort if no default domain defined */
	if (def_dom_num != -1 && def_dom_num != num_domains-1) {
		/* default is not last, move it */
		struct idmap_domain *tmp;

		if (pdb_dom_num > def_dom_num) {
			pdb_dom_num --;

		} else if (pdb_dom_num == def_dom_num) { /* ?? */
			pdb_dom_num = num_domains - 1;
		}

		tmp = idmap_domains[def_dom_num];

		for (i = def_dom_num; i < num_domains-1; i++) {
			idmap_domains[i] = idmap_domains[i+1];
		}
		idmap_domains[i] = tmp;
		def_dom_num = i;
	}


	/* Initialize alloc module */

	DEBUG(3, ("Initializing idmap alloc module\n"));

	alloc_backend = NULL;
	if (compat) {
		alloc_backend = talloc_strdup(idmap_ctx, compat_backend);
	} else {
		char *ab = lp_idmap_alloc_backend();

		if (ab && (ab[0] != '\0')) {
			alloc_backend = talloc_strdup(idmap_ctx,
						      lp_idmap_alloc_backend());
		}
	}

	if ( alloc_backend ) {

		idmap_alloc_ctx = TALLOC_ZERO_P(idmap_ctx,
						struct idmap_alloc_context);
		IDMAP_CHECK_ALLOC(idmap_alloc_ctx);

		idmap_alloc_ctx->methods = get_alloc_methods(alloc_backends,
							     alloc_backend);
		if ( ! idmap_alloc_ctx->methods) {
			ret = smb_probe_module("idmap", alloc_backend);
			if (NT_STATUS_IS_OK(ret)) {
				idmap_alloc_ctx->methods =
					get_alloc_methods(alloc_backends,
							  alloc_backend);
			}
		}
		if (idmap_alloc_ctx->methods) {

			if (compat_params) {
				idmap_alloc_ctx->params =
					talloc_strdup(idmap_alloc_ctx,
						      compat_params);
				IDMAP_CHECK_ALLOC(idmap_alloc_ctx->params);
			} else {
				idmap_alloc_ctx->params = NULL;
			}

			ret = idmap_alloc_ctx->methods->init(idmap_alloc_ctx->params);
			if ( ! NT_STATUS_IS_OK(ret)) {
				DEBUG(0, ("ERROR: Initialization failed for "
					  "alloc backend %s, deferred!\n",
					  alloc_backend));
			} else {
				idmap_alloc_ctx->initialized = True;
			}
		} else {
			DEBUG(2, ("idmap_init: Unable to get methods for "
				  "alloc backend %s\n",
				  alloc_backend));
			/* certain compat backends are just readonly */
			if ( compat ) {
				TALLOC_FREE(idmap_alloc_ctx);
				ret = NT_STATUS_OK;
			} else {
				ret = NT_STATUS_UNSUCCESSFUL;
			}
		}
	}

	/* cleanpu temporary strings */
	TALLOC_FREE( compat_backend );

	idmap_init_status = NT_STATUS_OK;

	return ret;

done:
	DEBUG(0, ("Aborting IDMAP Initialization ...\n"));
	idmap_close();

	return ret;
}

static NTSTATUS idmap_alloc_init(void)
{
	NTSTATUS ret;

	if (! NT_STATUS_IS_OK(ret = idmap_init())) {
		return ret;
	}

	if ( ! idmap_alloc_ctx) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	if ( ! idmap_alloc_ctx->initialized) {
		ret = idmap_alloc_ctx->methods->init(idmap_alloc_ctx->params);
		if ( ! NT_STATUS_IS_OK(ret)) {
			DEBUG(0, ("ERROR: Initialization failed for alloc "
				  "backend, deferred!\n"));
			return ret;
		} else {
			idmap_alloc_ctx->initialized = True;
		}
	}

	return NT_STATUS_OK;
}

/**************************************************************************
 idmap allocator interface functions
**************************************************************************/

NTSTATUS idmap_allocate_uid(struct unixid *id)
{
	NTSTATUS ret;

	if (! NT_STATUS_IS_OK(ret = idmap_alloc_init())) {
		return ret;
	}

	id->type = ID_TYPE_UID;
	return idmap_alloc_ctx->methods->allocate_id(id);
}

NTSTATUS idmap_allocate_gid(struct unixid *id)
{
	NTSTATUS ret;

	if (! NT_STATUS_IS_OK(ret = idmap_alloc_init())) {
		return ret;
	}

	id->type = ID_TYPE_GID;
	return idmap_alloc_ctx->methods->allocate_id(id);
}

NTSTATUS idmap_set_uid_hwm(struct unixid *id)
{
	NTSTATUS ret;

	if (! NT_STATUS_IS_OK(ret = idmap_alloc_init())) {
		return ret;
	}

	id->type = ID_TYPE_UID;
	return idmap_alloc_ctx->methods->set_id_hwm(id);
}

NTSTATUS idmap_set_gid_hwm(struct unixid *id)
{
	NTSTATUS ret;

	if (! NT_STATUS_IS_OK(ret = idmap_alloc_init())) {
		return ret;
	}

	id->type = ID_TYPE_GID;
	return idmap_alloc_ctx->methods->set_id_hwm(id);
}

/******************************************************************************
 Lookup an idmap_domain give a full user or group SID
 ******************************************************************************/

static struct idmap_domain* find_idmap_domain_from_sid( DOM_SID *account_sid )
{
	DOM_SID domain_sid;
	uint32 rid;
	struct winbindd_domain *domain = NULL;
	int i;

	/* 1. Handle BUILTIN or Special SIDs and prevent them from
	   falling into the default domain space (if we have a
	   configured passdb backend. */

	if ( (pdb_dom_num != -1) &&
	     (sid_check_is_in_builtin(account_sid) ||
	      sid_check_is_in_wellknown_domain(account_sid) ||
	      sid_check_is_in_unix_groups(account_sid) ||
	      sid_check_is_in_unix_users(account_sid)) )
	{
		return idmap_domains[pdb_dom_num];
	}

	/* 2. Lookup the winbindd_domain from the account_sid */

	sid_copy( &domain_sid, account_sid );
	sid_split_rid( &domain_sid, &rid );
	domain = find_domain_from_sid_noinit( &domain_sid );

	for (i = 0; domain && i < num_domains; i++) {
		if ( strequal( idmap_domains[i]->name, domain->name ) ) {
			return idmap_domains[i];
		}
	}

	/* 3. Fall back to the default domain */

	if ( def_dom_num != -1 ) {
		return idmap_domains[def_dom_num];
	}

	return NULL;
}

/******************************************************************************
 Lookup an index given an idmap_domain pointer
 ******************************************************************************/

static uint32 find_idmap_domain_index( struct idmap_domain *id_domain)
{
	int i;

	for (i = 0; i < num_domains; i++) {
		if ( idmap_domains[i] == id_domain )
			return i;
	}

	return -1;
}


/*********************************************************
 Check if creating a mapping is permitted for the domain
*********************************************************/

static NTSTATUS idmap_can_map(const struct id_map *map,
			      struct idmap_domain **ret_dom)
{
	struct idmap_domain *dom;

	/* Check we do not create mappings for our own local domain,
	 * or BUILTIN or special SIDs */
	if ((sid_compare_domain(map->sid, get_global_sam_sid()) == 0) ||
	    sid_check_is_in_builtin(map->sid) ||
	    sid_check_is_in_wellknown_domain(map->sid)) {
		DEBUG(10, ("We are not supposed to create mappings for "
			   "our own domains (local, builtin, specials)\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* Special check for trusted domain only = Yes */
	if (lp_winbind_trusted_domains_only()) {
		struct winbindd_domain *wdom = find_our_domain();
		if (wdom && (sid_compare_domain(map->sid, &wdom->sid) == 0)) {
			DEBUG(10, ("We are not supposed to create mappings for "
				   "our primary domain when <trusted domain "
				   "only> is True\n"));
			DEBUGADD(10, ("Leave [%s] unmapped\n",
				      sid_string_static(map->sid)));
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	if ( (dom = find_idmap_domain_from_sid( map->sid )) == NULL ) {
		/* huh, couldn't find a suitable domain,
		 *  let's just leave it unmapped */
		DEBUG(10, ("Could not find idmap backend for SID %s\n",
			   sid_string_static(map->sid)));
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	if (dom->readonly) {
		/* ouch the domain is read only,
		 *  let's just leave it unmapped */
		DEBUG(10, ("idmap backend for SID %s is READONLY!\n",
			   sid_string_static(map->sid)));
		return NT_STATUS_UNSUCCESSFUL;
	}

	*ret_dom = dom;
	return NT_STATUS_OK;
}

static NTSTATUS idmap_new_mapping(TALLOC_CTX *ctx, struct id_map *map)
{
	NTSTATUS ret;
	struct idmap_domain *dom;

	/* If we are offline we cannot lookup SIDs, deny mapping */
	if (idmap_is_offline())	{
		return NT_STATUS_FILE_IS_OFFLINE;
	}

	ret = idmap_can_map(map, &dom);
	if ( ! NT_STATUS_IS_OK(ret)) {
		return NT_STATUS_NONE_MAPPED;
	}

	/* check if this is a valid SID and then map it */
	switch (map->xid.type) {
	case ID_TYPE_UID:
		ret = idmap_allocate_uid(&map->xid);
		if ( ! NT_STATUS_IS_OK(ret)) {
			/* can't allocate id, let's just leave it unmapped */
			DEBUG(2, ("uid allocation failed! "
				  "Can't create mapping\n"));
			return NT_STATUS_NONE_MAPPED;
		}
		break;
	case ID_TYPE_GID:
		ret = idmap_allocate_gid(&map->xid);
		if ( ! NT_STATUS_IS_OK(ret)) {
			/* can't allocate id, let's just leave it unmapped */
			DEBUG(2, ("gid allocation failed! "
				  "Can't create mapping\n"));
			return NT_STATUS_NONE_MAPPED;
		}
		break;
	default:
		/* invalid sid, let's just leave it unmapped */
		DEBUG(3,("idmap_new_mapping: Refusing to create a "
			 "mapping for an unspecified ID type.\n"));
		return NT_STATUS_NONE_MAPPED;
	}

	/* ok, got a new id, let's set a mapping */
	map->status = ID_MAPPED;

	DEBUG(10, ("Setting mapping: %s <-> %s %lu\n",
		   sid_string_static(map->sid),
		   (map->xid.type == ID_TYPE_UID) ? "UID" : "GID",
		   (unsigned long)map->xid.id));
	ret = dom->methods->set_mapping(dom, map);

	if ( ! NT_STATUS_IS_OK(ret)) {
		/* something wrong here :-( */
		DEBUG(2, ("Failed to commit mapping\n!"));

	/* TODO: would it make sense to have an "unalloc_id function?" */

		return NT_STATUS_NONE_MAPPED;
	}

	return NT_STATUS_OK;
}

static NTSTATUS idmap_backends_set_mapping(const struct id_map *map)
{
	struct idmap_domain *dom;
	NTSTATUS ret;

	DEBUG(10, ("Setting mapping %s <-> %s %lu\n",
		   sid_string_static(map->sid),
		   (map->xid.type == ID_TYPE_UID) ? "UID" : "GID",
		   (unsigned long)map->xid.id));

	ret = idmap_can_map(map, &dom);
	if ( ! NT_STATUS_IS_OK(ret)) {
		return ret;
	}

	DEBUG(10,("set_mapping for domain %s\n", dom->name ));

	return dom->methods->set_mapping(dom, map);
}

static NTSTATUS idmap_backends_unixids_to_sids(struct id_map **ids)
{
	struct idmap_domain *dom;
	struct id_map **unmapped;
	struct id_map **_ids;
	TALLOC_CTX *ctx;
	NTSTATUS ret;
	int i, u, n;

	if (!ids || !*ids) {
		DEBUG(1, ("Invalid list of maps\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	ctx = talloc_named_const(NULL, 0, "idmap_backends_unixids_to_sids ctx");
	if ( ! ctx) {
		DEBUG(0, ("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(10, ("Query backends to map ids->sids\n"));

	/* start from the default (the last one) and then if there are still
	 * unmapped entries cycle through the others */

	_ids = ids;

	unmapped = NULL;
	for (n = num_domains-1; n >= 0; n--) { /* cycle backwards */

		dom = idmap_domains[n];

		DEBUG(10, ("Query sids from domain %s\n", dom->name));

		ret = dom->methods->unixids_to_sids(dom, _ids);
		IDMAP_REPORT_RET(ret);

		unmapped = NULL;

		for (i = 0, u = 0; _ids[i]; i++) {
			if (_ids[i]->status != ID_MAPPED) {
				unmapped = talloc_realloc(ctx, unmapped,
							struct id_map *, u + 2);
				IDMAP_CHECK_ALLOC(unmapped);
				unmapped[u] = _ids[i];
				u++;
			}
		}
		if (unmapped) {
			/* terminate the unmapped list */
			unmapped[u] = NULL;
		} else { /* no more entries, get out */
			break;
		}

		_ids = unmapped;

	}

	if (unmapped) {
		/* there are still unmapped ids,
		 * map them to the unix users/groups domains */
		/* except for expired entries,
		 * these will be returned as valid (offline mode) */
		for (i = 0; unmapped[i]; i++) {
			if (unmapped[i]->status == ID_EXPIRED) continue;
			switch (unmapped[i]->xid.type) {
			case ID_TYPE_UID:
				uid_to_unix_users_sid(
						(uid_t)unmapped[i]->xid.id,
						unmapped[i]->sid);
				unmapped[i]->status = ID_MAPPED;
				break;
			case ID_TYPE_GID:
				gid_to_unix_groups_sid(
						(gid_t)unmapped[i]->xid.id,
						unmapped[i]->sid);
				unmapped[i]->status = ID_MAPPED;
				break;
			default: /* what?! */
				unmapped[i]->status = ID_UNKNOWN;
				break;
			}
		}
	}

	ret = NT_STATUS_OK;

done:
	talloc_free(ctx);
	return ret;
}

static NTSTATUS idmap_backends_sids_to_unixids(struct id_map **ids)
{
	struct id_map ***dom_ids;
	struct idmap_domain *dom;
	TALLOC_CTX *ctx;
	NTSTATUS ret;
	int i, *counters;

	if ( (ctx = talloc_named_const(NULL, 0, "be_sids_to_ids")) == NULL ) {
		DEBUG(1, ("failed to allocate talloc context, OOM?\n"));
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(10, ("Query backends to map sids->ids\n"));

	/* split list per domain */
	if (num_domains == 0) {
		DEBUG(1, ("No domains available?\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	dom_ids = TALLOC_ZERO_ARRAY(ctx, struct id_map **, num_domains);
	IDMAP_CHECK_ALLOC(dom_ids);
	counters = TALLOC_ZERO_ARRAY(ctx, int, num_domains);
	IDMAP_CHECK_ALLOC(counters);

	/* partition the requests by domain */

	for (i = 0; ids[i]; i++) {
		uint32 idx;

		if ((dom = find_idmap_domain_from_sid(ids[i]->sid)) == NULL) {
			/* no available idmap_domain.  Move on */
			continue;
		}

		DEBUG(10,("SID %s is being handled by %s\n",
			  sid_string_static(ids[i]->sid),
			  dom ? dom->name : "none" ));

		idx = find_idmap_domain_index( dom );
		SMB_ASSERT( idx != -1 );

		dom_ids[idx] = talloc_realloc(ctx, dom_ids[idx],
					      struct id_map *,
					      counters[idx] + 2);
		IDMAP_CHECK_ALLOC(dom_ids[idx]);

		dom_ids[idx][counters[idx]] = ids[i];
		counters[idx]++;
		dom_ids[idx][counters[idx]] = NULL;
	}

	/* All the ids have been dispatched in the right queues.
	   Let's cycle through the filled ones */

	for (i = 0; i < num_domains; i++) {
		if (dom_ids[i]) {
			dom = idmap_domains[i];
			DEBUG(10, ("Query ids from domain %s\n", dom->name));
			ret = dom->methods->sids_to_unixids(dom, dom_ids[i]);
			IDMAP_REPORT_RET(ret);
		}
	}

	/* ok all the backends have been contacted at this point */
	/* let's see if we have any unmapped SID left and act accordingly */

	for (i = 0; ids[i]; i++) {
		/* NOTE: this will NOT touch ID_EXPIRED entries that the backend
		 * was not able to confirm/deny (offline mode) */
		if (ids[i]->status == ID_UNKNOWN ||
			ids[i]->status == ID_UNMAPPED) {
			/* ok this is an unmapped one, see if we can map it */
			ret = idmap_new_mapping(ctx, ids[i]);
			if (NT_STATUS_IS_OK(ret)) {
				/* successfully mapped */
				ids[i]->status = ID_MAPPED;
			} else
			if (NT_STATUS_EQUAL(ret, NT_STATUS_NONE_MAPPED)) {
				/* could not map it */
				ids[i]->status = ID_UNMAPPED;
			} else {
				/* Something very bad happened down there
				 * OR we are offline */
				ids[i]->status = ID_UNKNOWN;
			}
		}
	}

	ret = NT_STATUS_OK;

done:
	talloc_free(ctx);
	return ret;
}

/**************************************************************************
 idmap interface functions
**************************************************************************/

NTSTATUS idmap_unixids_to_sids(struct id_map **ids)
{
	TALLOC_CTX *ctx;
	NTSTATUS ret;
	struct id_map **bids;
	int i, bi;
	int bn = 0;

	if (! NT_STATUS_IS_OK(ret = idmap_init())) {
		return ret;
	}

	if (!ids || !*ids) {
		DEBUG(1, ("Invalid list of maps\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	ctx = talloc_named_const(NULL, 0, "idmap_unixids_to_sids ctx");
	if ( ! ctx) {
		DEBUG(1, ("failed to allocate talloc context, OOM?\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* no ids to be asked to the backends by default */
	bids = NULL;
	bi = 0;

	for (i = 0; ids[i]; i++) {

		if ( ! ids[i]->sid) {
			DEBUG(1, ("invalid null SID in id_map array"));
			talloc_free(ctx);
			return NT_STATUS_INVALID_PARAMETER;
		}

		ret = idmap_cache_map_id(idmap_cache, ids[i]);

		if ( ! NT_STATUS_IS_OK(ret)) {

			if ( ! bids) {
				/* alloc space for ids to be resolved by
				 * backends (realloc ten by ten) */
				bids = TALLOC_ARRAY(ctx, struct id_map *, 10);
				if ( ! bids) {
					DEBUG(1, ("Out of memory!\n"));
					talloc_free(ctx);
					return NT_STATUS_NO_MEMORY;
				}
				bn = 10;
			}

			/* add this id to the ones to be retrieved
			 * from the backends */
			bids[bi] = ids[i];
			bi++;

			/* check if we need to allocate new space
			 *  on the rids array */
			if (bi == bn) {
				bn += 10;
				bids = talloc_realloc(ctx, bids,
						      struct id_map *, bn);
				if ( ! bids) {
					DEBUG(1, ("Out of memory!\n"));
					talloc_free(ctx);
					return NT_STATUS_NO_MEMORY;
				}
			}

			/* make sure the last element is NULL */
			bids[bi] = NULL;
		}
	}

	/* let's see if there is any id mapping to be retieved
	 * from the backends */
	if (bi) {

		ret = idmap_backends_unixids_to_sids(bids);
		IDMAP_CHECK_RET(ret);

		/* update the cache */
		for (i = 0; i < bi; i++) {
			if (bids[i]->status == ID_MAPPED) {
				ret = idmap_cache_set(idmap_cache, bids[i]);
			} else if (bids[i]->status == ID_EXPIRED) {
				/* the cache returned an expired entry and the
				 * backend was not able to clear the situation
				 * (offline). This handles a previous
				 * NT_STATUS_SYNCHRONIZATION_REQUIRED
				 * for disconnected mode, */
				bids[i]->status = ID_MAPPED;
			} else if (bids[i]->status == ID_UNKNOWN) {
				/* something bad here. We were not able to
				 * handle this for some reason, mark it as
				 * unmapped and hope next time things will
				 * settle down. */
				bids[i]->status = ID_UNMAPPED;
			} else { /* unmapped */
				ret = idmap_cache_set_negative_id(idmap_cache,
								  bids[i]);
			}
			IDMAP_CHECK_RET(ret);
		}
	}

	ret = NT_STATUS_OK;
done:
	talloc_free(ctx);
	return ret;
}

NTSTATUS idmap_sids_to_unixids(struct id_map **ids)
{
	TALLOC_CTX *ctx;
	NTSTATUS ret;
	struct id_map **bids;
	int i, bi;
	int bn = 0;

	if (! NT_STATUS_IS_OK(ret = idmap_init())) {
		return ret;
	}

	if (!ids || !*ids) {
		DEBUG(1, ("Invalid list of maps\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	ctx = talloc_named_const(NULL, 0, "idmap_sids_to_unixids ctx");
	if ( ! ctx) {
		DEBUG(1, ("failed to allocate talloc context, OOM?\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* no ids to be asked to the backends by default */
	bids = NULL;
	bi = 0;

	for (i = 0; ids[i]; i++) {

		if ( ! ids[i]->sid) {
			DEBUG(1, ("invalid null SID in id_map array\n"));
			talloc_free(ctx);
			return NT_STATUS_INVALID_PARAMETER;
		}

		ret = idmap_cache_map_sid(idmap_cache, ids[i]);

		if ( ! NT_STATUS_IS_OK(ret)) {

			if ( ! bids) {
				/* alloc space for ids to be resolved
				   by backends (realloc ten by ten) */
				bids = TALLOC_ARRAY(ctx, struct id_map *, 10);
				if ( ! bids) {
					DEBUG(1, ("Out of memory!\n"));
					talloc_free(ctx);
					return NT_STATUS_NO_MEMORY;
				}
				bn = 10;
			}

			/* add this id to the ones to be retrieved
			 * from the backends */
			bids[bi] = ids[i];
			bi++;

			/* check if we need to allocate new space
			 * on the ids array */
			if (bi == bn) {
				bn += 10;
				bids = talloc_realloc(ctx, bids,
						      struct id_map *, bn);
				if ( ! bids) {
					DEBUG(1, ("Out of memory!\n"));
					talloc_free(ctx);
					return NT_STATUS_NO_MEMORY;
				}
			}

			/* make sure the last element is NULL */
			bids[bi] = NULL;
		}
	}

	/* let's see if there is any id mapping to be retieved
	 * from the backends */
	if (bids) {

		ret = idmap_backends_sids_to_unixids(bids);
		IDMAP_CHECK_RET(ret);

		/* update the cache */
		for (i = 0; bids[i]; i++) {
			if (bids[i]->status == ID_MAPPED) {
				ret = idmap_cache_set(idmap_cache, bids[i]);
			} else if (bids[i]->status == ID_EXPIRED) {
				/* the cache returned an expired entry and the
				 * backend was not able to clear the situation
				 * (offline). This handles a previous
				 * NT_STATUS_SYNCHRONIZATION_REQUIRED
				 * for disconnected mode, */
				bids[i]->status = ID_MAPPED;
			} else if (bids[i]->status == ID_UNKNOWN) {
				/* something bad here. We were not able to
				 * handle this for some reason, mark it as
				 * unmapped and hope next time things will
				 * settle down. */
				bids[i]->status = ID_UNMAPPED;
			} else { /* unmapped */
				ret = idmap_cache_set_negative_sid(idmap_cache,
								   bids[i]);
			}
			IDMAP_CHECK_RET(ret);
		}
	}

	ret = NT_STATUS_OK;
done:
	talloc_free(ctx);
	return ret;
}

NTSTATUS idmap_set_mapping(const struct id_map *id)
{
	TALLOC_CTX *ctx;
	NTSTATUS ret;

	if (! NT_STATUS_IS_OK(ret = idmap_init())) {
		return ret;
	}

	/* sanity checks */
	if ((id->sid == NULL) || (id->status != ID_MAPPED)) {
		DEBUG(1, ("NULL SID or unmapped entry\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* TODO: check uid/gid range ? */

	ctx = talloc_named_const(NULL, 0, "idmap_set_mapping ctx");
	if ( ! ctx) {
		DEBUG(1, ("failed to allocate talloc context, OOM?\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* set the new mapping */
	ret = idmap_backends_set_mapping(id);
	IDMAP_CHECK_RET(ret);

	/* set the mapping in the cache */
	ret = idmap_cache_set(idmap_cache, id);
	IDMAP_CHECK_RET(ret);

done:
	talloc_free(ctx);
	return ret;
}

/**************************************************************************
 Dump backend status.
**************************************************************************/

void idmap_dump_maps(char *logfile)
{
	NTSTATUS ret;
	struct unixid allid;
	struct id_map *maps;
	int num_maps;
	FILE *dump;
	int i;

	if (! NT_STATUS_IS_OK(ret = idmap_init())) {
		return;
	}

	dump = fopen(logfile, "w");
	if ( ! dump) {
		DEBUG(0, ("Unable to open open stream for file [%s], "
			  "errno: %d\n", logfile, errno));
		return;
	}

	if (NT_STATUS_IS_OK(ret = idmap_alloc_init())) {
		allid.type = ID_TYPE_UID;
		allid.id = 0;
		idmap_alloc_ctx->methods->get_id_hwm(&allid);
		fprintf(dump, "USER HWM %lu\n", (unsigned long)allid.id);

		allid.type = ID_TYPE_GID;
		allid.id = 0;
		idmap_alloc_ctx->methods->get_id_hwm(&allid);
		fprintf(dump, "GROUP HWM %lu\n", (unsigned long)allid.id);
	}

	maps = talloc(idmap_ctx, struct id_map);
	num_maps = 0;

	for (i = 0; i < num_domains; i++) {
		if (idmap_domains[i]->methods->dump_data) {
			idmap_domains[i]->methods->dump_data(idmap_domains[i],
							     &maps, &num_maps);
		}
	}

	for (i = 0; i < num_maps; i++) {
		switch (maps[i].xid.type) {
		case ID_TYPE_UID:
			fprintf(dump, "UID %lu %s\n",
				(unsigned long)maps[i].xid.id,
				sid_string_static(maps[i].sid));
			break;
		case ID_TYPE_GID:
			fprintf(dump, "GID %lu %s\n",
				(unsigned long)maps[i].xid.id,
				sid_string_static(maps[i].sid));
			break;
		case ID_TYPE_NOT_SPECIFIED:
			break;
		}
	}

	fflush(dump);
	fclose(dump);
}

char *idmap_fetch_secret(const char *backend, bool alloc,
			       const char *domain, const char *identity)
{
	char *tmp, *ret;
	int r;

	if (alloc) {
		r = asprintf(&tmp, "IDMAP_ALLOC_%s", backend);
	} else {
		r = asprintf(&tmp, "IDMAP_%s_%s", backend, domain);
	}

	if (r < 0)
		return NULL;

	strupper_m(tmp); /* make sure the key is case insensitive */
	ret = secrets_fetch_generic(tmp, identity);

	SAFE_FREE(tmp);

	return ret;
}

