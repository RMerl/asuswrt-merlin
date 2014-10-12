/*
   ldb database library

   Copyright (C) Simo Sorce  2004-2008

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

/*
 *  Name: ldb
 *
 *  Component: ldb modules core
 *
 *  Description: core modules routines
 *
 *  Author: Simo Sorce
 */

#include "ldb_private.h"
#include "dlinklist.h"
#include "system/dir.h"

static char *ldb_modules_strdup_no_spaces(TALLOC_CTX *mem_ctx, const char *string)
{
	size_t i, len;
	char *trimmed;

	trimmed = talloc_strdup(mem_ctx, string);
	if (!trimmed) {
		return NULL;
	}

	len = strlen(trimmed);
	for (i = 0; trimmed[i] != '\0'; i++) {
		switch (trimmed[i]) {
		case ' ':
		case '\t':
		case '\n':
			memmove(&trimmed[i], &trimmed[i + 1], len -i -1);
			break;
		}
	}

	return trimmed;
}


/* modules are called in inverse order on the stack.
   Lets place them as an admin would think the right order is.
   Modules order is important */
const char **ldb_modules_list_from_string(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, const char *string)
{
	char **modules = NULL;
	const char **m;
	char *modstr, *p;
	unsigned int i;

	/* spaces not admitted */
	modstr = ldb_modules_strdup_no_spaces(mem_ctx, string);
	if ( ! modstr) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "Out of Memory in ldb_modules_strdup_no_spaces()");
		return NULL;
	}

	modules = talloc_realloc(mem_ctx, modules, char *, 2);
	if ( ! modules ) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "Out of Memory in ldb_modules_list_from_string()");
		talloc_free(modstr);
		return NULL;
	}
	talloc_steal(modules, modstr);

	if (modstr[0] == '\0') {
		modules[0] = NULL;
		m = (const char **)modules;
		return m;
	}

	i = 0;
	/* The str*r*chr walks backwards:  This is how we get the inverse order mentioned above */
	while ((p = strrchr(modstr, ',')) != NULL) {
		*p = '\0';
		p++;
		modules[i] = p;

		i++;
		modules = talloc_realloc(mem_ctx, modules, char *, i + 2);
		if ( ! modules ) {
			ldb_debug(ldb, LDB_DEBUG_FATAL, "Out of Memory in ldb_modules_list_from_string()");
			return NULL;
		}

	}
	modules[i] = modstr;

	modules[i + 1] = NULL;

	m = (const char **)modules;

	return m;
}

static struct backends_list_entry {
	struct ldb_backend_ops *ops;
	struct backends_list_entry *prev, *next;
} *ldb_backends = NULL;

static struct ops_list_entry {
	const struct ldb_module_ops *ops;
	struct ops_list_entry *next;
} *registered_modules = NULL;

static struct backends_list_entry *ldb_find_backend(const char *url_prefix)
{
	struct backends_list_entry *backend;

	for (backend = ldb_backends; backend; backend = backend->next) {
		if (strcmp(backend->ops->name, url_prefix) == 0) {
			return backend;
		}
	}

	return NULL;
}

/*
  register a new ldb backend

  if override is true, then override any existing backend for this prefix
*/
int ldb_register_backend(const char *url_prefix, ldb_connect_fn connectfn, bool override)
{
	struct backends_list_entry *be;

	be = ldb_find_backend(url_prefix);
	if (be) {
		if (!override) {
			return LDB_SUCCESS;
		}
	} else {
		be = talloc_zero(ldb_backends, struct backends_list_entry);
		if (!be) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		be->ops = talloc_zero(be, struct ldb_backend_ops);
		if (!be->ops) {
			talloc_free(be);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		DLIST_ADD_END(ldb_backends, be, struct backends_list_entry);
	}

	be->ops->name = url_prefix;
	be->ops->connect_fn = connectfn;

	return LDB_SUCCESS;
}

/*
   Return the ldb module form of a database.
   The URL can either be one of the following forms
   ldb://path
   ldapi://path

   flags is made up of LDB_FLG_*

   the options are passed uninterpreted to the backend, and are
   backend specific.

   This allows modules to get at only the backend module, for example where a
   module may wish to direct certain requests at a particular backend.
*/
int ldb_module_connect_backend(struct ldb_context *ldb,
			       const char *url,
			       const char *options[],
			       struct ldb_module **backend_module)
{
	int ret;
	char *backend;
	struct backends_list_entry *be;

	if (strchr(url, ':') != NULL) {
		backend = talloc_strndup(ldb, url, strchr(url, ':')-url);
	} else {
		/* Default to tdb */
		backend = talloc_strdup(ldb, "tdb");
	}

	be = ldb_find_backend(backend);

	talloc_free(backend);

	if (be == NULL) {
		ldb_debug(ldb, LDB_DEBUG_FATAL,
			  "Unable to find backend for '%s' - do you need to set LDB_MODULES_PATH?", url);
		return LDB_ERR_OTHER;
	}

	ret = be->ops->connect_fn(ldb, url, ldb->flags, options, backend_module);

	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			  "Failed to connect to '%s' with backend '%s'", url, be->ops->name);
		return ret;
	}
	return ret;
}

static struct ldb_hooks {
	struct ldb_hooks *next, *prev;
	ldb_hook_fn hook_fn;
} *ldb_hooks;

/*
  register a ldb hook function
 */
int ldb_register_hook(ldb_hook_fn hook_fn)
{
	struct ldb_hooks *lc;
	lc = talloc_zero(ldb_hooks, struct ldb_hooks);
	if (lc == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	lc->hook_fn = hook_fn;
	DLIST_ADD_END(ldb_hooks, lc, struct ldb_hooks);
	return LDB_SUCCESS;
}

/*
  call ldb hooks of a given type
 */
int ldb_modules_hook(struct ldb_context *ldb, enum ldb_module_hook_type t)
{
	struct ldb_hooks *lc;
	for (lc = ldb_hooks; lc; lc=lc->next) {
		int ret = lc->hook_fn(ldb, t);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	return LDB_SUCCESS;
}


static const struct ldb_module_ops *ldb_find_module_ops(const char *name)
{
	struct ops_list_entry *e;

	for (e = registered_modules; e; e = e->next) {
 		if (strcmp(e->ops->name, name) == 0)
			return e->ops;
	}

	return NULL;
}


int ldb_register_module(const struct ldb_module_ops *ops)
{
	struct ops_list_entry *entry;

	if (ldb_find_module_ops(ops->name) != NULL)
		return LDB_ERR_ENTRY_ALREADY_EXISTS;

	entry = talloc(talloc_autofree_context(), struct ops_list_entry);
	if (entry == NULL)
		return -1;

	entry->ops = ops;
	entry->next = registered_modules;
	registered_modules = entry;

	return 0;
}

/*
  load a list of modules
 */
int ldb_module_load_list(struct ldb_context *ldb, const char **module_list,
			 struct ldb_module *backend, struct ldb_module **out)
{
	struct ldb_module *module;
	unsigned int i;

	module = backend;

	for (i = 0; module_list && module_list[i] != NULL; i++) {
		struct ldb_module *current;
		const struct ldb_module_ops *ops;

		if (strcmp(module_list[i], "") == 0) {
			continue;
		}

		ops = ldb_find_module_ops(module_list[i]);

		if (ops == NULL) {
			ldb_debug(ldb, LDB_DEBUG_FATAL, "WARNING: Module [%s] not found - do you need to set LDB_MODULES_PATH?",
				  module_list[i]);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		current = talloc_zero(ldb, struct ldb_module);
		if (current == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		talloc_set_name(current, "ldb_module: %s", module_list[i]);

		current->ldb = ldb;
		current->ops = ops;

		DLIST_ADD(module, current);
	}
	*out = module;
	return LDB_SUCCESS;
}

/*
  initialise a chain of modules
 */
int ldb_module_init_chain(struct ldb_context *ldb, struct ldb_module *module)
{
	while (module && module->ops->init_context == NULL)
		module = module->next;

	/* init is different in that it is not an error if modules
	 * do not require initialization */

	if (module) {
		int ret = module->ops->init_context(module);
		if (ret != LDB_SUCCESS) {
			ldb_debug(ldb, LDB_DEBUG_FATAL, "module %s initialization failed : %s",
				  module->ops->name, ldb_strerror(ret));
			return ret;
		}
	}

	return LDB_SUCCESS;
}

int ldb_load_modules(struct ldb_context *ldb, const char *options[])
{
	const char *modules_string;
	const char **modules = NULL;
	int ret;
	TALLOC_CTX *mem_ctx = talloc_new(ldb);
	if (!mem_ctx) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* find out which modules we are requested to activate */

	/* check if we have a custom module list passd as ldb option */
	if (options) {
		modules_string = ldb_options_find(ldb, options, "modules");
		if (modules_string) {
			modules = ldb_modules_list_from_string(ldb, mem_ctx, modules_string);
		}
	}

	/* if not overloaded by options and the backend is not ldap try to load the modules list from ldb */
	if ((modules == NULL) && (strcmp("ldap", ldb->modules->ops->name) != 0)) {
		const char * const attrs[] = { "@LIST" , NULL};
		struct ldb_result *res = NULL;
		struct ldb_dn *mods_dn;

		mods_dn = ldb_dn_new(mem_ctx, ldb, "@MODULES");
		if (mods_dn == NULL) {
			talloc_free(mem_ctx);
			return -1;
		}

		ret = ldb_search(ldb, mods_dn, &res, mods_dn, LDB_SCOPE_BASE, attrs, "@LIST=*");

		if (ret == LDB_ERR_NO_SUCH_OBJECT) {
			ldb_debug(ldb, LDB_DEBUG_TRACE, "no modules required by the db");
		} else if (ret != LDB_SUCCESS) {
			ldb_debug(ldb, LDB_DEBUG_FATAL, "ldb error (%s) occurred searching for modules, bailing out", ldb_errstring(ldb));
			talloc_free(mem_ctx);
			return ret;
		} else {
			const char *module_list;
			if (res->count == 0) {
				ldb_debug(ldb, LDB_DEBUG_TRACE, "no modules required by the db");
			} else if (res->count > 1) {
				ldb_debug(ldb, LDB_DEBUG_FATAL, "Too many records found (%d), bailing out", res->count);
				talloc_free(mem_ctx);
				return -1;
			} else {
				module_list = ldb_msg_find_attr_as_string(res->msgs[0], "@LIST", NULL);
				if (!module_list) {
					ldb_debug(ldb, LDB_DEBUG_TRACE, "no modules required by the db");
				}
				modules = ldb_modules_list_from_string(ldb, mem_ctx,
							       module_list);
			}
		}

		talloc_free(mods_dn);
	}

	if (modules != NULL) {
		ret = ldb_module_load_list(ldb, modules, ldb->modules, &ldb->modules);
		if (ret != LDB_SUCCESS) {
			talloc_free(mem_ctx);
			return ret;
		}
	} else {
		ldb_debug(ldb, LDB_DEBUG_TRACE, "No modules specified for this database");
	}

	ret = ldb_module_init_chain(ldb, ldb->modules);
	talloc_free(mem_ctx);
	return ret;
}

/*
  by using this we allow ldb modules to only implement the functions they care about,
  which makes writing a module simpler, and makes it more likely to keep working
  when ldb is extended
*/
#define FIND_OP_NOERR(module, op) do { \
	module = module->next; \
	while (module && module->ops->op == NULL) module = module->next; \
	if ((module && module->ldb->flags & LDB_FLG_ENABLE_TRACING)) { \
		ldb_debug(module->ldb, LDB_DEBUG_TRACE, "ldb_trace_next_request: (%s)->" #op, \
			  module->ops->name);				\
	}								\
} while (0)

#define FIND_OP(module, op) do { \
	struct ldb_context *ldb = module->ldb; \
	FIND_OP_NOERR(module, op); \
	if (module == NULL) { \
		ldb_asprintf_errstring(ldb, "Unable to find backend operation for " #op ); \
		return LDB_ERR_OPERATIONS_ERROR;	\
	}						\
} while (0)


struct ldb_module *ldb_module_new(TALLOC_CTX *memctx,
				  struct ldb_context *ldb,
				  const char *module_name,
				  const struct ldb_module_ops *ops)
{
	struct ldb_module *module;

	module = talloc(memctx, struct ldb_module);
	if (!module) {
		ldb_oom(ldb);
		return NULL;
	}
	talloc_set_name_const(module, module_name);
	module->ldb = ldb;
	module->prev = module->next = NULL;
	module->ops = ops;

	return module;
}

const char * ldb_module_get_name(struct ldb_module *module)
{
	return module->ops->name;
}

struct ldb_context *ldb_module_get_ctx(struct ldb_module *module)
{
	return module->ldb;
}

const struct ldb_module_ops *ldb_module_get_ops(struct ldb_module *module)
{
	return module->ops;
}

void *ldb_module_get_private(struct ldb_module *module)
{
	return module->private_data;
}

void ldb_module_set_private(struct ldb_module *module, void *private_data)
{
	module->private_data = private_data;
}

/*
   helper functions to call the next module in chain
*/

int ldb_next_request(struct ldb_module *module, struct ldb_request *request)
{
	int ret;

	if (request->callback == NULL) {
		ldb_set_errstring(module->ldb, "Requests MUST define callbacks");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	request->handle->nesting++;

	switch (request->operation) {
	case LDB_SEARCH:
		FIND_OP(module, search);
		ret = module->ops->search(module, request);
		break;
	case LDB_ADD:
		FIND_OP(module, add);
		ret = module->ops->add(module, request);
		break;
	case LDB_MODIFY:
		FIND_OP(module, modify);
		ret = module->ops->modify(module, request);
		break;
	case LDB_DELETE:
		FIND_OP(module, del);
		ret = module->ops->del(module, request);
		break;
	case LDB_RENAME:
		FIND_OP(module, rename);
		ret = module->ops->rename(module, request);
		break;
	case LDB_EXTENDED:
		FIND_OP(module, extended);
		ret = module->ops->extended(module, request);
		break;
	default:
		FIND_OP(module, request);
		ret = module->ops->request(module, request);
		break;
	}

	request->handle->nesting--;

	if (ret == LDB_SUCCESS) {
		return ret;
	}
	if (!ldb_errstring(module->ldb)) {
		/* Set a default error string, to place the blame somewhere */
		ldb_asprintf_errstring(module->ldb, "error in module %s: %s (%d)", module->ops->name, ldb_strerror(ret), ret);
	}

	if (!(request->handle->flags & LDB_HANDLE_FLAG_DONE_CALLED)) {
		/* It is _extremely_ common that a module returns a
		 * failure without calling ldb_module_done(), but that
		 * guarantees we will end up hanging in
		 * ldb_wait(). This fixes it without having to rewrite
		 * all our modules, and leaves us one less sharp
		 * corner for module developers to cut themselves on
		 */
		ret = ldb_module_done(request, NULL, NULL, ret);
	}
	return ret;
}

int ldb_next_init(struct ldb_module *module)
{
	module = module->next;

	return ldb_module_init_chain(module->ldb, module);
}

int ldb_next_start_trans(struct ldb_module *module)
{
	int ret;
	FIND_OP(module, start_transaction);
	ret = module->ops->start_transaction(module);
	if (ret == LDB_SUCCESS) {
		return ret;
	}
	if (!ldb_errstring(module->ldb)) {
		/* Set a default error string, to place the blame somewhere */
		ldb_asprintf_errstring(module->ldb, "start_trans error in module %s: %s (%d)", module->ops->name, ldb_strerror(ret), ret);
	}
	if ((module && module->ldb->flags & LDB_FLG_ENABLE_TRACING)) { 
		ldb_debug(module->ldb, LDB_DEBUG_TRACE, "ldb_next_start_trans error: %s", 
			  ldb_errstring(module->ldb));				
	}
	return ret;
}

int ldb_next_end_trans(struct ldb_module *module)
{
	int ret;
	FIND_OP(module, end_transaction);
	ret = module->ops->end_transaction(module);
	if (ret == LDB_SUCCESS) {
		return ret;
	}
	if (!ldb_errstring(module->ldb)) {
		/* Set a default error string, to place the blame somewhere */
		ldb_asprintf_errstring(module->ldb, "end_trans error in module %s: %s (%d)", module->ops->name, ldb_strerror(ret), ret);
	}
	if ((module && module->ldb->flags & LDB_FLG_ENABLE_TRACING)) { 
		ldb_debug(module->ldb, LDB_DEBUG_TRACE, "ldb_next_end_trans error: %s", 
			  ldb_errstring(module->ldb));				
	}
	return ret;
}

int ldb_next_prepare_commit(struct ldb_module *module)
{
	int ret;
	FIND_OP_NOERR(module, prepare_commit);
	if (module == NULL) {
		/* we are allowed to have no prepare commit in
		   backends */
		return LDB_SUCCESS;
	}
	ret = module->ops->prepare_commit(module);
	if (ret == LDB_SUCCESS) {
		return ret;
	}
	if (!ldb_errstring(module->ldb)) {
		/* Set a default error string, to place the blame somewhere */
		ldb_asprintf_errstring(module->ldb, "prepare_commit error in module %s: %s (%d)", module->ops->name, ldb_strerror(ret), ret);
	}
	if ((module && module->ldb->flags & LDB_FLG_ENABLE_TRACING)) { 
		ldb_debug(module->ldb, LDB_DEBUG_TRACE, "ldb_next_prepare_commit error: %s", 
			  ldb_errstring(module->ldb));				
	}
	return ret;
}

int ldb_next_del_trans(struct ldb_module *module)
{
	int ret;
	FIND_OP(module, del_transaction);
	ret = module->ops->del_transaction(module);
	if (ret == LDB_SUCCESS) {
		return ret;
	}
	if (!ldb_errstring(module->ldb)) {
		/* Set a default error string, to place the blame somewhere */
		ldb_asprintf_errstring(module->ldb, "del_trans error in module %s: %s (%d)", module->ops->name, ldb_strerror(ret), ret);
	}
	if ((module && module->ldb->flags & LDB_FLG_ENABLE_TRACING)) { 
		ldb_debug(module->ldb, LDB_DEBUG_TRACE, "ldb_next_del_trans error: %s", 
			  ldb_errstring(module->ldb));				
	}
	return ret;
}

struct ldb_handle *ldb_handle_new(TALLOC_CTX *mem_ctx, struct ldb_context *ldb)
{
	struct ldb_handle *h;

	h = talloc_zero(mem_ctx, struct ldb_handle);
	if (h == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return NULL;
	}

	h->status = LDB_SUCCESS;
	h->state = LDB_ASYNC_INIT;
	h->ldb = ldb;
	h->flags = 0;
	h->location = NULL;
	h->parent = NULL;

	return h;
}

/* calls the request callback to send an entry
 *
 * params:
 *      req: the original request passed to your module
 *      msg: reply message (must be a talloc pointer, and it will be stolen
 *           on the ldb_reply that is sent to the callback)
 * 	ctrls: controls to send in the reply  (must be a talloc pointer, and it will be stolen
 *           on the ldb_reply that is sent to the callback)
 */

int ldb_module_send_entry(struct ldb_request *req,
			  struct ldb_message *msg,
			  struct ldb_control **ctrls)
{
	struct ldb_reply *ares;

	ares = talloc_zero(req, struct ldb_reply);
	if (!ares) {
		ldb_oom(req->handle->ldb);
		req->callback(req, NULL);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ares->type = LDB_REPLY_ENTRY;
	ares->message = talloc_steal(ares, msg);
	ares->controls = talloc_steal(ares, ctrls);
	ares->error = LDB_SUCCESS;

	if ((req->handle->ldb->flags & LDB_FLG_ENABLE_TRACING) &&
	    req->handle->nesting == 0) {
		char *s;
		ldb_debug_add(req->handle->ldb, "ldb_trace_response: ENTRY\n");
		s = ldb_ldif_message_string(req->handle->ldb, msg, LDB_CHANGETYPE_NONE, msg);
		ldb_debug_add(req->handle->ldb, "%s\n", s);
		talloc_free(s);
		ldb_debug_end(req->handle->ldb, LDB_DEBUG_TRACE);
	}

	return req->callback(req, ares);
}

/* calls the request callback to send an referrals
 *
 * params:
 *      req: the original request passed to your module
 *      ref: referral string (must be a talloc pointeri, steal)
 */

int ldb_module_send_referral(struct ldb_request *req,
					   char *ref)
{
	struct ldb_reply *ares;

	ares = talloc_zero(req, struct ldb_reply);
	if (!ares) {
		ldb_oom(req->handle->ldb);
		req->callback(req, NULL);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ares->type = LDB_REPLY_REFERRAL;
	ares->referral = talloc_steal(ares, ref);
	ares->error = LDB_SUCCESS;

	if ((req->handle->ldb->flags & LDB_FLG_ENABLE_TRACING) &&
	    req->handle->nesting == 0) {
		ldb_debug_add(req->handle->ldb, "ldb_trace_response: REFERRAL\n");
		ldb_debug_add(req->handle->ldb, "ref: %s\n", ref);
		ldb_debug_end(req->handle->ldb, LDB_DEBUG_TRACE);
	}

	return req->callback(req, ares);
}

/* calls the original request callback
 *
 * params:
 * 	req:   the original request passed to your module
 * 	ctrls: controls to send in the reply (must be a talloc pointer, steal)
 * 	response: results for extended request (steal)
 * 	error: LDB_SUCCESS for a successful return
 * 	       any other ldb error otherwise
 */
int ldb_module_done(struct ldb_request *req,
		    struct ldb_control **ctrls,
		    struct ldb_extended *response,
		    int error)
{
	struct ldb_reply *ares;

	ares = talloc_zero(req, struct ldb_reply);
	if (!ares) {
		ldb_oom(req->handle->ldb);
		req->callback(req, NULL);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ares->type = LDB_REPLY_DONE;
	ares->controls = talloc_steal(ares, ctrls);
	ares->response = talloc_steal(ares, response);
	ares->error = error;

	req->handle->flags |= LDB_HANDLE_FLAG_DONE_CALLED;

	if ((req->handle->ldb->flags & LDB_FLG_ENABLE_TRACING) &&
	    req->handle->nesting == 0) {
		ldb_debug_add(req->handle->ldb, "ldb_trace_response: DONE\n");
		ldb_debug_add(req->handle->ldb, "error: %u\n", error);
		if (ldb_errstring(req->handle->ldb)) {
			ldb_debug_add(req->handle->ldb, "msg: %s\n",
				  ldb_errstring(req->handle->ldb));
		}
		ldb_debug_end(req->handle->ldb, LDB_DEBUG_TRACE);
	}

	return req->callback(req, ares);
}

/* to be used *only* in modules init functions.
 * this function is synchronous and will register
 * the requested OID in the rootdse module if present
 * otherwise it will return an error */
int ldb_mod_register_control(struct ldb_module *module, const char *oid)
{
	struct ldb_request *req;
	int ret;

	req = talloc_zero(module, struct ldb_request);
	if (req == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->operation = LDB_REQ_REGISTER_CONTROL;
	req->op.reg_control.oid = oid;
	req->callback = ldb_op_default_callback;

	ldb_set_timeout(module->ldb, req, 0);

	req->handle = ldb_handle_new(req, module->ldb);
	if (req->handle == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_request(module->ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}
	talloc_free(req);

	return ret;
}

static int ldb_modules_load_dir(const char *modules_dir, const char *version);


/*
  load one module. A static list of loaded module inode numbers is
  used to prevent a module being loaded twice

  dlopen() is used on the module, and dlsym() is then used to look for
  a ldb_init_module() function. If present, that function is called
  with the ldb version number as an argument.

  The ldb_init_module() function will typically call
  ldb_register_module() and ldb_register_backend() to register a
  module or backend, but it may also be used to register command line
  handling functions, ldif handlers or any other local
  modififications.

  The ldb_init_module() function does not get a ldb_context passed in,
  as modules will be used for multiple ldb context handles. The call
  from the first ldb_init() is just a convenient way to ensure it is
  called early enough.
 */
static int ldb_modules_load_path(const char *path, const char *version)
{
	void *handle;
	int (*init_fn)(const char *);
	int ret;
	struct stat st;
	static struct loaded {
		struct loaded *next, *prev;
		ino_t st_ino;
		dev_t st_dev;
	} *loaded;
	struct loaded *le;
	int dlopen_flags;

	ret = stat(path, &st);
	if (ret != 0) {
		fprintf(stderr, "ldb: unable to stat module %s : %s\n", path, strerror(errno));
		return LDB_ERR_UNAVAILABLE;
	}

	for (le=loaded; le; le=le->next) {
		if (le->st_ino == st.st_ino &&
		    le->st_dev == st.st_dev) {
			/* its already loaded */
			return LDB_SUCCESS;
		}
	}

	le = talloc(loaded, struct loaded);
	if (le == NULL) {
		fprintf(stderr, "ldb: unable to allocated loaded entry\n");
		return LDB_ERR_UNAVAILABLE;
	}

	le->st_ino = st.st_ino;
	le->st_dev = st.st_dev;

	DLIST_ADD_END(loaded, le, struct loaded);

	/* if it is a directory, recurse */
	if (S_ISDIR(st.st_mode)) {
		return ldb_modules_load_dir(path, version);
	}

	dlopen_flags = RTLD_NOW;
#ifdef RTLD_DEEPBIND
	/* use deepbind if possible, to avoid issues with different
	   system library varients, for example ldb modules may be linked
	   against Heimdal while the application may use MIT kerberos

	   See the dlopen manpage for details
	*/
	dlopen_flags |= RTLD_DEEPBIND;
#endif

	handle = dlopen(path, dlopen_flags);
	if (handle == NULL) {
		fprintf(stderr, "ldb: unable to dlopen %s : %s\n", path, dlerror());
		return LDB_SUCCESS;
	}

	init_fn = dlsym(handle, "ldb_init_module");
	if (init_fn == NULL) {
		/* ignore it, it could be an old-style
		 * module. Once we've converted all modules we
		 * could consider this an error */
		dlclose(handle);
		return LDB_SUCCESS;
	}

	ret = init_fn(version);
	if (ret == LDB_ERR_ENTRY_ALREADY_EXISTS) {
		/* the module is already registered - ignore this, as
		 * it can happen if LDB_MODULES_PATH points at both
		 * the build and install directory
		 */
		ret = LDB_SUCCESS;
	}
	return ret;
}

static int qsort_string(const char **s1, const char **s2)
{
	return strcmp(*s1, *s2);
}


/*
  load all modules from the given ldb modules directory. This is run once
  during the first ldb_init() call.

  Modules are loaded in alphabetical order to ensure that any module
  load ordering dependencies are reproducible. Modules should avoid
  relying on load order
 */
static int ldb_modules_load_dir(const char *modules_dir, const char *version)
{
	DIR *dir;
	struct dirent *de;
	const char **modlist = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(NULL);
	unsigned i, num_modules = 0;

	dir = opendir(modules_dir);
	if (dir == NULL) {
		if (errno == ENOENT) {
			talloc_free(tmp_ctx);
			/* we don't have any modules */
			return LDB_SUCCESS;
		}
		talloc_free(tmp_ctx);
		fprintf(stderr, "ldb: unable to open modules directory '%s' - %s\n",
			modules_dir, strerror(errno));
		return LDB_ERR_UNAVAILABLE;
	}


	while ((de = readdir(dir))) {
		if (ISDOT(de->d_name) || ISDOTDOT(de->d_name))
			continue;

		modlist = talloc_realloc(tmp_ctx, modlist, const char *, num_modules+1);
		if (modlist == NULL) {
			talloc_free(tmp_ctx);
			closedir(dir);
			fprintf(stderr, "ldb: unable to allocate modules list\n");
			return LDB_ERR_UNAVAILABLE;
		}
		modlist[num_modules] = talloc_asprintf(modlist, "%s/%s", modules_dir, de->d_name);
		if (modlist[num_modules] == NULL) {
			talloc_free(tmp_ctx);
			closedir(dir);
			fprintf(stderr, "ldb: unable to allocate module list entry\n");
			return LDB_ERR_UNAVAILABLE;
		}
		num_modules++;
	}

	closedir(dir);

	/* sort the directory, so we get consistent load ordering */
	TYPESAFE_QSORT(modlist, num_modules, qsort_string);

	for (i=0; i<num_modules; i++) {
		int ret = ldb_modules_load_path(modlist[i], version);
		if (ret != LDB_SUCCESS) {
			fprintf(stderr, "ldb: failed to initialise module %s : %s\n",
				modlist[i], ldb_strerror(ret));
			talloc_free(tmp_ctx);
			return ret;
		}
	}

	talloc_free(tmp_ctx);

	return LDB_SUCCESS;
}

/* 
   load any additional modules from the given directory 
*/
void ldb_set_modules_dir(struct ldb_context *ldb, const char *path)
{
	int ret = ldb_modules_load_path(path, LDB_VERSION);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb, "Failed to load modules from: %s\n", path);
	}
}


/*
  load all modules static (builtin) modules
 */
static int ldb_modules_load_static(const char *version)
{
	static bool initialised;
#define _MODULE_PROTO(init) extern int init(const char *);
	STATIC_ldb_MODULES_PROTO;
	const ldb_module_init_fn static_init_functions[] = { STATIC_ldb_MODULES };
	unsigned i;

	if (initialised) {
		return LDB_SUCCESS;
	}
	initialised = true;

	for (i=0; static_init_functions[i]; i++) {
		int ret = static_init_functions[i](version);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	return LDB_SUCCESS;
}

/*
  load all modules from the given ldb modules path, colon
  separated.

  modules are loaded recursively for all subdirectories in the paths
 */
int ldb_modules_load(const char *modules_path, const char *version)
{
	char *tok, *path, *tok_ptr=NULL;
	int ret;

	ret = ldb_modules_load_static(version);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	path = talloc_strdup(NULL, modules_path);
	if (path == NULL) {
		fprintf(stderr, "ldb: failed to allocate modules_path\n");
		return LDB_ERR_UNAVAILABLE;
	}

	for (tok=strtok_r(path, ":", &tok_ptr);
	     tok;
	     tok=strtok_r(NULL, ":", &tok_ptr)) {
		ret = ldb_modules_load_path(tok, version);
		if (ret != LDB_SUCCESS) {
			talloc_free(path);
			return ret;
		}
	}
	talloc_free(path);

	return LDB_SUCCESS;
}


/*
  return a string representation of the calling chain for the given
  ldb request
 */
char *ldb_module_call_chain(struct ldb_request *req, TALLOC_CTX *mem_ctx)
{
	char *ret;
	int i=0;

	ret = talloc_strdup(mem_ctx, "");
	if (ret == NULL) {
		return NULL;
	}

	while (req && req->handle) {
		char *s = talloc_asprintf_append_buffer(ret, "req[%u] %p  : %s\n",
							i++, req, ldb_req_location(req));
		if (s == NULL) {
			talloc_free(ret);
			return NULL;
		}
		ret = s;
		req = req->handle->parent;
	}
	return ret;
}


/*
  return the next module in the chain
 */
struct ldb_module *ldb_module_next(struct ldb_module *module)
{
	return module->next;
}

/*
  set the next module in the module chain
 */
void ldb_module_set_next(struct ldb_module *module, struct ldb_module *next)
{
	module->next = next;
}


/*
  get the popt_options pointer in the ldb structure. This allows a ldb
  module to change the command line parsing
 */
struct poptOption **ldb_module_popt_options(struct ldb_context *ldb)
{
	return &ldb->popt_options;
}


/*
  return the current ldb flags LDB_FLG_*
 */
uint32_t ldb_module_flags(struct ldb_context *ldb)
{
	return ldb->flags;
}
