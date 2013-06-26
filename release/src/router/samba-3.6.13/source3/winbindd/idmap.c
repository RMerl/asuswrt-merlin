/*
   Unix SMB/CIFS implementation.
   ID Mapping
   Copyright (C) Tim Potter 2000
   Copyright (C) Jim McDonough <jmcd@us.ibm.com>	2003
   Copyright (C) Simo Sorce 2003-2007
   Copyright (C) Jeremy Allison 2006
   Copyright (C) Michael Adam 2010

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
#include "idmap.h"
#include "passdb/machine_sid.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

static_decl_idmap;

static void idmap_init(void)
{
	static bool initialized;

	if (initialized) {
		return;
	}

	DEBUG(10, ("idmap_init(): calling static_init_idmap\n"));

	static_init_idmap;

	initialized = true;
}

/**
 * Pointer to the backend methods. Modules register themselves here via
 * smb_register_idmap.
 */

struct idmap_backend {
	const char *name;
	struct idmap_methods *methods;
	struct idmap_backend *prev, *next;
};
static struct idmap_backend *backends = NULL;

/**
 * Default idmap domain configured via "idmap backend".
 */
static struct idmap_domain *default_idmap_domain;

/**
 * Passdb idmap domain, not configurable. winbind must always give passdb a
 * chance to map ids.
 */
static struct idmap_domain *passdb_idmap_domain;

/**
 * List of specially configured idmap domains. This list is filled on demand
 * in the winbind idmap child when the parent winbind figures out via the
 * special range parameter or via the domain SID that a special "idmap config
 * domain" configuration is present.
 */
static struct idmap_domain **idmap_domains = NULL;
static int num_domains = 0;

static struct idmap_methods *get_methods(const char *name)
{
	struct idmap_backend *b;

	for (b = backends; b; b = b->next) {
		if (strequal(b->name, name)) {
			return b->methods;
		}
	}

	return NULL;
}

bool idmap_is_offline(void)
{
	return ( lp_winbind_offline_logon() &&
	     get_global_winbindd_state_offline() );
}

bool idmap_is_online(void)
{
	return !idmap_is_offline();
}

/**********************************************************************
 Allow a module to register itself as a method.
**********************************************************************/

NTSTATUS smb_register_idmap(int version, const char *name,
			    struct idmap_methods *methods)
{
	struct idmap_backend *entry;

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

	for (entry = backends; entry != NULL; entry = entry->next) {
		if (strequal(entry->name, name)) {
			DEBUG(0,("Idmap module %s already registered!\n",
				 name));
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
	}

	entry = talloc(NULL, struct idmap_backend);
	if ( ! entry) {
		DEBUG(0,("Out of memory!\n"));
		TALLOC_FREE(entry);
		return NT_STATUS_NO_MEMORY;
	}
	entry->name = talloc_strdup(entry, name);
	if ( ! entry->name) {
		DEBUG(0,("Out of memory!\n"));
		TALLOC_FREE(entry);
		return NT_STATUS_NO_MEMORY;
	}
	entry->methods = methods;

	DLIST_ADD(backends, entry);
	DEBUG(5, ("Successfully added idmap backend '%s'\n", name));
	return NT_STATUS_OK;
}

/**
 * Initialize a domain structure
 * @param[in] mem_ctx		memory context for the result
 * @param[in] domainname	which domain is this for
 * @param[in] modulename	which backend module
 * @param[in] check_range	whether range checking should be done
 * @result The initialized structure
 */
static struct idmap_domain *idmap_init_domain(TALLOC_CTX *mem_ctx,
					      const char *domainname,
					      const char *modulename,
					      bool check_range)
{
	struct idmap_domain *result;
	NTSTATUS status;
	char *config_option = NULL;
	const char *range;

	result = talloc_zero(mem_ctx, struct idmap_domain);
	if (result == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	result->name = talloc_strdup(result, domainname);
	if (result->name == NULL) {
		DEBUG(0, ("talloc failed\n"));
		goto fail;
	}

	/*
	 * load ranges and read only information from the config
	 */

	config_option = talloc_asprintf(result, "idmap config %s",
					result->name);
	if (config_option == NULL) {
		DEBUG(0, ("Out of memory!\n"));
		goto fail;
	}

	range = lp_parm_const_string(-1, config_option, "range", NULL);
	if (range == NULL) {
		DEBUG(1, ("idmap range not specified for domain %s\n",
			  result->name));
		if (check_range) {
			goto fail;
		}
	} else if (sscanf(range, "%u - %u", &result->low_id,
			  &result->high_id) != 2)
	{
		DEBUG(1, ("invalid range '%s' specified for domain "
			  "'%s'\n", range, result->name));
		if (check_range) {
			goto fail;
		}
	}

	result->read_only = lp_parm_bool(-1, config_option, "read only", false);

	talloc_free(config_option);

	if (result->low_id > result->high_id) {
		DEBUG(1, ("Error: invalid idmap range detected: %lu - %lu\n",
			  (unsigned long)result->low_id,
			  (unsigned long)result->high_id));
		if (check_range) {
			goto fail;
		}
	}

	result->methods = get_methods(modulename);
	if (result->methods == NULL) {
		DEBUG(3, ("idmap backend %s not found\n", modulename));

		status = smb_probe_module("idmap", modulename);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(3, ("Could not probe idmap module %s\n",
				  modulename));
			goto fail;
		}

		result->methods = get_methods(modulename);
	}
	if (result->methods == NULL) {
		DEBUG(1, ("idmap backend %s not found\n", modulename));
		goto fail;
	}

	status = result->methods->init(result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("idmap initialization returned %s\n",
			  nt_errstr(status)));
		goto fail;
	}

	return result;

fail:
	TALLOC_FREE(result);
	return NULL;
}

/**
 * Initialize a named domain structure
 * @param[in] mem_ctx		memory context for the result
 * @param[in] domname		the domain name
 * @result The default domain structure
 *
 * This routine looks at the "idmap config <domname>" parameters to figure out
 * the configuration.
 */

static struct idmap_domain *idmap_init_named_domain(TALLOC_CTX *mem_ctx,
						    const char *domname)
{
	struct idmap_domain *result = NULL;
	char *config_option;
	const char *backend;

	idmap_init();

	config_option = talloc_asprintf(talloc_tos(), "idmap config %s",
					domname);
	if (config_option == NULL) {
		DEBUG(0, ("talloc failed\n"));
		goto fail;
	}

	backend = lp_parm_const_string(-1, config_option, "backend", NULL);
	if (backend == NULL) {
		DEBUG(1, ("no backend defined for %s\n", config_option));
		goto fail;
	}

	result = idmap_init_domain(mem_ctx, domname, backend, true);
	if (result == NULL) {
		goto fail;
	}

	TALLOC_FREE(config_option);
	return result;

fail:
	TALLOC_FREE(config_option);
	TALLOC_FREE(result);
	return NULL;
}

/**
 * Initialize the default domain structure
 * @param[in] mem_ctx		memory context for the result
 * @result The default domain structure
 *
 * This routine takes the module name from the "idmap backend" parameter,
 * passing a possible parameter like ldap:ldap://ldap-url/ to the module.
 */

static struct idmap_domain *idmap_init_default_domain(TALLOC_CTX *mem_ctx)
{
	return idmap_init_named_domain(mem_ctx, "*");
}

/**
 * Initialize the passdb domain structure
 * @param[in] mem_ctx		memory context for the result
 * @result The default domain structure
 *
 * No config, passdb has its own configuration.
 */

static struct idmap_domain *idmap_init_passdb_domain(TALLOC_CTX *mem_ctx)
{
	idmap_init();

	/*
	 * Always init the default domain, we can't go without one
	 */
	if (default_idmap_domain == NULL) {
		default_idmap_domain = idmap_init_default_domain(NULL);
	}
	if (default_idmap_domain == NULL) {
		return NULL;
	}

	if (passdb_idmap_domain != NULL) {
		return passdb_idmap_domain;
	}

	passdb_idmap_domain = idmap_init_domain(NULL, get_global_sam_name(),
						"passdb", false);
	if (passdb_idmap_domain == NULL) {
		DEBUG(1, ("Could not init passdb idmap domain\n"));
	}

	return passdb_idmap_domain;
}

/**
 * Find a domain struct according to a domain name
 * @param[in] domname		Domain name to get the config for
 * @result The default domain structure that fits
 *
 * This is the central routine in the winbindd-idmap child to pick the correct
 * domain for looking up IDs. If domname is NULL or empty, we use the default
 * domain. If it contains something, we try to use idmap_init_named_domain()
 * to fetch the correct backend.
 *
 * The choice about "domname" is being made by the winbind parent, look at the
 * "have_idmap_config" of "struct winbindd_domain" which is set in
 * add_trusted_domain.
 */

struct idmap_domain *idmap_find_domain(const char *domname)
{
	struct idmap_domain *result;
	int i;

	DEBUG(10, ("idmap_find_domain called for domain '%s'\n",
		   domname?domname:"NULL"));

	idmap_init();

	/*
	 * Always init the default domain, we can't go without one
	 */
	if (default_idmap_domain == NULL) {
		default_idmap_domain = idmap_init_default_domain(NULL);
	}
	if (default_idmap_domain == NULL) {
		return NULL;
	}

	if ((domname == NULL) || (domname[0] == '\0')) {
		return default_idmap_domain;
	}

	for (i=0; i<num_domains; i++) {
		if (strequal(idmap_domains[i]->name, domname)) {
			return idmap_domains[i];
		}
	}

	if (idmap_domains == NULL) {
		/*
		 * talloc context for all idmap domains
		 */
		idmap_domains = TALLOC_ARRAY(NULL, struct idmap_domain *, 1);
	}

	if (idmap_domains == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	result = idmap_init_named_domain(idmap_domains, domname);
	if (result == NULL) {
		/*
		 * Could not init that domain -- try the default one
		 */
		return default_idmap_domain;
	}

	ADD_TO_ARRAY(idmap_domains, struct idmap_domain *, result,
		     &idmap_domains, &num_domains);
	return result;
}

void idmap_close(void)
{
	TALLOC_FREE(default_idmap_domain);
	TALLOC_FREE(passdb_idmap_domain);
	TALLOC_FREE(idmap_domains);
	num_domains = 0;
}

/**************************************************************************
 idmap allocator interface functions
**************************************************************************/

static NTSTATUS idmap_allocate_unixid(struct unixid *id)
{
	struct idmap_domain *dom;
	NTSTATUS ret;

	dom = idmap_find_domain(NULL);

	if (dom == NULL) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (dom->methods->allocate_id == NULL) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	ret = dom->methods->allocate_id(dom, id);

	return ret;
}


NTSTATUS idmap_allocate_uid(struct unixid *id)
{
	id->type = ID_TYPE_UID;
	return idmap_allocate_unixid(id);
}

NTSTATUS idmap_allocate_gid(struct unixid *id)
{
	id->type = ID_TYPE_GID;
	return idmap_allocate_unixid(id);
}

NTSTATUS idmap_backends_unixid_to_sid(const char *domname, struct id_map *id)
{
	struct idmap_domain *dom;
	struct id_map *maps[2];

	 DEBUG(10, ("idmap_backend_unixid_to_sid: domain = '%s', xid = %d "
		    "(type %d)\n",
		    domname?domname:"NULL", id->xid.id, id->xid.type));

	maps[0] = id;
	maps[1] = NULL;

	/*
	 * Always give passdb a chance first
	 */

	dom = idmap_init_passdb_domain(NULL);
	if ((dom != NULL)
	    && NT_STATUS_IS_OK(dom->methods->unixids_to_sids(dom, maps))
	    && id->status == ID_MAPPED) {
		return NT_STATUS_OK;
	}

	dom = idmap_find_domain(domname);
	if (dom == NULL) {
		return NT_STATUS_NONE_MAPPED;
	}

	return dom->methods->unixids_to_sids(dom, maps);
}

NTSTATUS idmap_backends_sid_to_unixid(const char *domain, struct id_map *id)
{
	struct idmap_domain *dom;
	struct id_map *maps[2];

	DEBUG(10, ("idmap_backends_sid_to_unixid: domain = '%s', sid = [%s]\n",
		   domain?domain:"NULL", sid_string_dbg(id->sid)));

	maps[0] = id;
	maps[1] = NULL;

	if (sid_check_is_in_builtin(id->sid)
	    || (sid_check_is_in_our_domain(id->sid)))
	{
		NTSTATUS status;

		DEBUG(10, ("asking passdb...\n"));

		dom = idmap_init_passdb_domain(NULL);
		if (dom == NULL) {
			return NT_STATUS_NONE_MAPPED;
		}
		status = dom->methods->sids_to_unixids(dom, maps);

		if (NT_STATUS_IS_OK(status) && id->status == ID_MAPPED) {
			return status;
		}

		DEBUG(10, ("passdb could not map.\n"));

		return NT_STATUS_NONE_MAPPED;
	}

	dom = idmap_find_domain(domain);
	if (dom == NULL) {
		return NT_STATUS_NONE_MAPPED;
	}

	return dom->methods->sids_to_unixids(dom, maps);
}
