/*
   Unix SMB/CIFS implementation.
   ID Mapping
   Copyright (C) Tim Potter 2000
   Copyright (C) Jim McDonough <jmcd@us.ibm.com>	2003
   Copyright (C) Simo Sorce 2003-2007
   Copyright (C) Jeremy Allison 2006

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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

static_decl_idmap;

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
 * Pointer to the alloc backend methods. Modules register themselves here via
 * smb_register_idmap_alloc.
 */
struct idmap_alloc_backend {
	const char *name;
	struct idmap_alloc_methods *methods;
	struct idmap_alloc_backend *prev, *next;
};
static struct idmap_alloc_backend *alloc_backends = NULL;

/**
 * The idmap alloc context that is configured via "idmap alloc
 * backend". Defaults to "idmap backend" in case the module (tdb, ldap) also
 * provides alloc methods.
 */
struct idmap_alloc_context {
	struct idmap_alloc_methods *methods;
};
static struct idmap_alloc_context *idmap_alloc_ctx = NULL;

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

static struct idmap_alloc_methods *get_alloc_methods(const char *name)
{
	struct idmap_alloc_backend *b;

	for (b = alloc_backends; b; b = b->next) {
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

/**********************************************************************
 Allow a module to register itself as an alloc method.
**********************************************************************/

NTSTATUS smb_register_idmap_alloc(int version, const char *name,
				  struct idmap_alloc_methods *methods)
{
	struct idmap_alloc_methods *test;
	struct idmap_alloc_backend *entry;

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

	test = get_alloc_methods(name);
	if (test) {
		DEBUG(0,("idmap_alloc module %s already registered!\n", name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	entry = talloc(NULL, struct idmap_alloc_backend);
	if ( ! entry) {
		DEBUG(0,("Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}
	entry->name = talloc_strdup(entry, name);
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

static bool parse_idmap_module(TALLOC_CTX *mem_ctx, const char *param,
			       char **pmodulename, char **pargs)
{
	char *modulename;
	char *args;

	if (strncmp(param, "idmap_", 6) == 0) {
		param += 6;
		DEBUG(1, ("idmap_init: idmap backend uses deprecated "
			  "'idmap_' prefix.  Please replace 'idmap_%s' by "
			  "'%s'\n", param, param));
	}

	modulename = talloc_strdup(mem_ctx, param);
	if (modulename == NULL) {
		return false;
	}

	args = strchr(modulename, ':');
	if (args == NULL) {
		*pmodulename = modulename;
		*pargs = NULL;
		return true;
	}

	*args = '\0';

	args = talloc_strdup(mem_ctx, args+1);
	if (args == NULL) {
		TALLOC_FREE(modulename);
		return false;
	}

	*pmodulename = modulename;
	*pargs = args;
	return true;
}

/**
 * Initialize a domain structure
 * @param[in] mem_ctx		memory context for the result
 * @param[in] domainname	which domain is this for
 * @param[in] modulename	which backend module
 * @param[in] params		parameter to pass to the init function
 * @result The initialized structure
 */
static struct idmap_domain *idmap_init_domain(TALLOC_CTX *mem_ctx,
					      const char *domainname,
					      const char *modulename,
					      const char *params)
{
	struct idmap_domain *result;
	NTSTATUS status;

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

	status = result->methods->init(result, params);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("idmap initialization returned %s\n",
			  nt_errstr(status)));
		goto fail;
	}

	talloc_set_destructor(result, close_domain_destructor);

	return result;

fail:
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
	struct idmap_domain *result;
	char *modulename;
	char *params;

	DEBUG(10, ("idmap_init_default_domain: calling static_init_idmap\n"));

	static_init_idmap;

	if (!parse_idmap_module(talloc_tos(), lp_idmap_backend(), &modulename,
				&params)) {
		DEBUG(1, ("parse_idmap_module failed\n"));
		return NULL;
	}

	DEBUG(3, ("idmap_init: using '%s' as remote backend\n", modulename));

	result = idmap_init_domain(mem_ctx, "*", modulename, params);
	if (result == NULL) {
		goto fail;
	}

	TALLOC_FREE(modulename);
	TALLOC_FREE(params);
	return result;

fail:
	TALLOC_FREE(modulename);
	TALLOC_FREE(params);
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

	result = idmap_init_domain(mem_ctx, domname, backend, NULL);
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
 * Initialize the passdb domain structure
 * @param[in] mem_ctx		memory context for the result
 * @result The default domain structure
 *
 * No config, passdb has its own configuration.
 */

static struct idmap_domain *idmap_init_passdb_domain(TALLOC_CTX *mem_ctx)
{
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
						"passdb", NULL);
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

static struct idmap_domain *idmap_find_domain(const char *domname)
{
	struct idmap_domain *result;
	int i;

	DEBUG(10, ("idmap_find_domain called for domain '%s'\n",
		   domname?domname:"NULL"));

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
        if (idmap_alloc_ctx) {
                idmap_alloc_ctx->methods->close_fn();
                idmap_alloc_ctx->methods = NULL;
        }
        alloc_backends = NULL;
	TALLOC_FREE(default_idmap_domain);
	TALLOC_FREE(passdb_idmap_domain);
	TALLOC_FREE(idmap_domains);
	num_domains = 0;
}

/**
 * Initialize the idmap alloc backend
 * @param[out] ctx		Where to put the alloc_ctx?
 * @result Did it work fine?
 *
 * This routine first looks at "idmap alloc backend" and if that is not
 * defined, it uses "idmap backend" for the module name.
 */
static NTSTATUS idmap_alloc_init(struct idmap_alloc_context **ctx)
{
	const char *backend;
	char *modulename, *params;
	NTSTATUS ret = NT_STATUS_NO_MEMORY;;

	static_init_idmap;

	if (idmap_alloc_ctx != NULL) {
		*ctx = idmap_alloc_ctx;
		return NT_STATUS_OK;
	}

	idmap_alloc_ctx = talloc(NULL, struct idmap_alloc_context);
	if (idmap_alloc_ctx == NULL) {
		DEBUG(0, ("talloc failed\n"));
		goto fail;
	}

	backend = lp_idmap_alloc_backend();
	if ((backend == NULL) || (backend[0] == '\0')) {
		backend = lp_idmap_backend();
	}

	if (backend == NULL) {
		DEBUG(3, ("no idmap alloc backend defined\n"));
		ret = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	if (!parse_idmap_module(idmap_alloc_ctx, backend, &modulename,
				&params)) {
		DEBUG(1, ("parse_idmap_module %s failed\n", backend));
		goto fail;
	}

	idmap_alloc_ctx->methods = get_alloc_methods(modulename);

	if (idmap_alloc_ctx->methods == NULL) {
		ret = smb_probe_module("idmap", modulename);
		if (NT_STATUS_IS_OK(ret)) {
			idmap_alloc_ctx->methods =
				get_alloc_methods(modulename);
		}
	}

	if (idmap_alloc_ctx->methods == NULL) {
		DEBUG(1, ("could not find idmap alloc module %s\n", backend));
		ret = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ret = idmap_alloc_ctx->methods->init(params);

	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0, ("ERROR: Initialization failed for alloc "
			  "backend, deferred!\n"));
		goto fail;
	}

	TALLOC_FREE(modulename);
	TALLOC_FREE(params);

	*ctx = idmap_alloc_ctx;
	return NT_STATUS_OK;

fail:
	TALLOC_FREE(idmap_alloc_ctx);
	return ret;
}

/**************************************************************************
 idmap allocator interface functions
**************************************************************************/

NTSTATUS idmap_allocate_uid(struct unixid *id)
{
	struct idmap_alloc_context *ctx;
	NTSTATUS ret;

	if (!NT_STATUS_IS_OK(ret = idmap_alloc_init(&ctx))) {
		return ret;
	}

	id->type = ID_TYPE_UID;
	return ctx->methods->allocate_id(id);
}

NTSTATUS idmap_allocate_gid(struct unixid *id)
{
	struct idmap_alloc_context *ctx;
	NTSTATUS ret;

	if (!NT_STATUS_IS_OK(ret = idmap_alloc_init(&ctx))) {
		return ret;
	}

	id->type = ID_TYPE_GID;
	return ctx->methods->allocate_id(id);
}

NTSTATUS idmap_set_uid_hwm(struct unixid *id)
{
	struct idmap_alloc_context *ctx;
	NTSTATUS ret;

	if (!NT_STATUS_IS_OK(ret = idmap_alloc_init(&ctx))) {
		return ret;
	}

	id->type = ID_TYPE_UID;
	return ctx->methods->set_id_hwm(id);
}

NTSTATUS idmap_set_gid_hwm(struct unixid *id)
{
	struct idmap_alloc_context *ctx;
	NTSTATUS ret;

	if (!NT_STATUS_IS_OK(ret = idmap_alloc_init(&ctx))) {
		return ret;
	}

	id->type = ID_TYPE_GID;
	return ctx->methods->set_id_hwm(id);
}

NTSTATUS idmap_new_mapping(const struct dom_sid *psid, enum id_type type,
			   struct unixid *pxid)
{
	struct dom_sid sid;
	struct idmap_domain *dom;
	struct id_map map;
	NTSTATUS status;

	dom = idmap_find_domain(NULL);
	if (dom == NULL) {
		DEBUG(3, ("no default domain, no place to write\n"));
		return NT_STATUS_ACCESS_DENIED;
	}
	if (dom->methods->set_mapping == NULL) {
		DEBUG(3, ("default domain not writable\n"));
		return NT_STATUS_MEDIA_WRITE_PROTECTED;
	}

	sid_copy(&sid, psid);
	map.sid = &sid;
	map.xid.type = type;

	switch (type) {
	case ID_TYPE_UID:
		status = idmap_allocate_uid(&map.xid);
		break;
	case ID_TYPE_GID:
		status = idmap_allocate_gid(&map.xid);
		break;
	default:
		status = NT_STATUS_INVALID_PARAMETER;
		break;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("Could not allocate id: %s\n", nt_errstr(status)));
		return status;
	}

	map.status = ID_MAPPED;

	DEBUG(10, ("Setting mapping: %s <-> %s %lu\n",
		   sid_string_dbg(map.sid),
		   (map.xid.type == ID_TYPE_UID) ? "UID" : "GID",
		   (unsigned long)map.xid.id));

	status = dom->methods->set_mapping(dom, &map);

	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_COLLISION)) {
		struct id_map *ids[2];
		DEBUG(5, ("Mapping for %s exists - retrying to map sid\n",
			  sid_string_dbg(map.sid)));
		ids[0] = &map;
		ids[1] = NULL;
		status = dom->methods->sids_to_unixids(dom, ids);
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("Could not store the new mapping: %s\n",
			  nt_errstr(status)));
		return status;
	}

	*pxid = map.xid;

	return NT_STATUS_OK;
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
	    || (sid_check_is_in_our_domain(id->sid))) {

		dom = idmap_init_passdb_domain(NULL);
		if (dom == NULL) {
			return NT_STATUS_NONE_MAPPED;
		}
		return dom->methods->sids_to_unixids(dom, maps);
	}

	dom = idmap_find_domain(domain);
	if (dom == NULL) {
		return NT_STATUS_NONE_MAPPED;
	}

	return dom->methods->sids_to_unixids(dom, maps);
}

NTSTATUS idmap_set_mapping(const struct id_map *map)
{
	struct idmap_domain *dom;

	dom = idmap_find_domain(NULL);
	if (dom == NULL) {
		DEBUG(3, ("no default domain, no place to write\n"));
		return NT_STATUS_ACCESS_DENIED;
	}
	if (dom->methods->set_mapping == NULL) {
		DEBUG(3, ("default domain not writable\n"));
		return NT_STATUS_MEDIA_WRITE_PROTECTED;
	}

	return dom->methods->set_mapping(dom, map);
}

NTSTATUS idmap_remove_mapping(const struct id_map *map)
{
	struct idmap_domain *dom;

	dom = idmap_find_domain(NULL);
	if (dom == NULL) {
		DEBUG(3, ("no default domain, no place to write\n"));
		return NT_STATUS_ACCESS_DENIED;
	}
	if (dom->methods->remove_mapping == NULL) {
		DEBUG(3, ("default domain not writable\n"));
		return NT_STATUS_MEDIA_WRITE_PROTECTED;
	}

	return dom->methods->remove_mapping(dom, map);
}
