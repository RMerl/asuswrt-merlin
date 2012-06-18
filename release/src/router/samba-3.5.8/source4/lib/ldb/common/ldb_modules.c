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

#define LDB_MODULE_PREFIX	"modules:"
#define LDB_MODULE_PREFIX_LEN	8

static void *ldb_dso_load_symbol(struct ldb_context *ldb, const char *name,
				 const char *symbol);

void ldb_set_modules_dir(struct ldb_context *ldb, const char *path)
{
	talloc_free(ldb->modules_dir);
	ldb->modules_dir = talloc_strdup(ldb, path);
}

static char *ldb_modules_strdup_no_spaces(TALLOC_CTX *mem_ctx, const char *string)
{
	int i, len;
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
	int i;

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

static const struct ldb_builtins {
	const struct ldb_backend_ops *backend_ops;
	const struct ldb_module_ops *module_ops;
} builtins[];

static ldb_connect_fn ldb_find_backend(const char *url)
{
	struct backends_list_entry *backend;
	int i;

	for (i = 0; builtins[i].backend_ops || builtins[i].module_ops; i++) {
		if (builtins[i].backend_ops == NULL) continue;

		if (strncmp(builtins[i].backend_ops->name, url,
			    strlen(builtins[i].backend_ops->name)) == 0) {
			return builtins[i].backend_ops->connect_fn;
		}
	}

	for (backend = ldb_backends; backend; backend = backend->next) {
		if (strncmp(backend->ops->name, url,
			    strlen(backend->ops->name)) == 0) {
			return backend->ops->connect_fn;
		}
	}

	return NULL;
}

/*
 register a new ldb backend
*/
int ldb_register_backend(const char *url_prefix, ldb_connect_fn connectfn)
{
	struct ldb_backend_ops *backend;
	struct backends_list_entry *entry;

	backend = talloc(talloc_autofree_context(), struct ldb_backend_ops);
	if (!backend) return LDB_ERR_OPERATIONS_ERROR;

	entry = talloc(talloc_autofree_context(), struct backends_list_entry);
	if (!entry) {
		talloc_free(backend);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (ldb_find_backend(url_prefix)) {
		return LDB_SUCCESS;
	}

	/* Maybe check for duplicity here later on? */

	backend->name = talloc_strdup(backend, url_prefix);
	backend->connect_fn = connectfn;
	entry->ops = backend;
	DLIST_ADD(ldb_backends, entry);

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
int ldb_connect_backend(struct ldb_context *ldb,
			const char *url,
			const char *options[],
			struct ldb_module **backend_module)
{
	int ret;
	char *backend;
	ldb_connect_fn fn;

	if (strchr(url, ':') != NULL) {
		backend = talloc_strndup(ldb, url, strchr(url, ':')-url);
	} else {
		/* Default to tdb */
		backend = talloc_strdup(ldb, "tdb");
	}

	fn = ldb_find_backend(backend);

	if (fn == NULL) {
		struct ldb_backend_ops *ops;
		char *symbol_name = talloc_asprintf(ldb, "ldb_%s_backend_ops", backend);
		if (symbol_name == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		ops = ldb_dso_load_symbol(ldb, backend, symbol_name);
		if (ops != NULL) {
			fn = ops->connect_fn;
		}
		talloc_free(symbol_name);
	}

	talloc_free(backend);

	if (fn == NULL) {
		ldb_debug(ldb, LDB_DEBUG_FATAL,
			  "Unable to find backend for '%s'", url);
		return LDB_ERR_OTHER;
	}

	ret = fn(ldb, url, ldb->flags, options, backend_module);

	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			  "Failed to connect to '%s'", url);
		return ret;
	}
	return ret;
}

static const struct ldb_module_ops *ldb_find_module_ops(const char *name)
{
	struct ops_list_entry *e;
	int i;

	for (i = 0; builtins[i].backend_ops || builtins[i].module_ops; i++) {
		if (builtins[i].module_ops == NULL) continue;

		if (strcmp(builtins[i].module_ops->name, name) == 0)
			return builtins[i].module_ops;
	}

	for (e = registered_modules; e; e = e->next) {
 		if (strcmp(e->ops->name, name) == 0)
			return e->ops;
	}

	return NULL;
}


int ldb_register_module(const struct ldb_module_ops *ops)
{
	struct ops_list_entry *entry = talloc(talloc_autofree_context(), struct ops_list_entry);

	if (ldb_find_module_ops(ops->name) != NULL)
		return -1;

	if (entry == NULL)
		return -1;

	entry->ops = ops;
	entry->next = registered_modules;
	registered_modules = entry;

	return 0;
}

static void *ldb_dso_load_symbol(struct ldb_context *ldb, const char *name,
				 const char *symbol)
{
	char *path;
	void *handle;
	void *sym;

	if (ldb->modules_dir == NULL)
		return NULL;

	path = talloc_asprintf(ldb, "%s/%s.%s", ldb->modules_dir, name,
			       SHLIBEXT);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "trying to load %s from %s", name, path);

	handle = dlopen(path, RTLD_NOW);
	if (handle == NULL) {
		ldb_debug(ldb, LDB_DEBUG_WARNING, "unable to load %s from %s: %s", name, path, dlerror());
		return NULL;
	}

	sym = (int (*)(void))dlsym(handle, symbol);

	if (sym == NULL) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "no symbol `%s' found in %s: %s", symbol, path, dlerror());
		return NULL;
	}

	talloc_free(path);

	return sym;
}

int ldb_load_modules_list(struct ldb_context *ldb, const char **module_list, struct ldb_module *backend, struct ldb_module **out)
{
	struct ldb_module *module;
	int i;

	module = backend;

	for (i = 0; module_list[i] != NULL; i++) {
		struct ldb_module *current;
		const struct ldb_module_ops *ops;

		if (strcmp(module_list[i], "") == 0) {
			continue;
		}

		ops = ldb_find_module_ops(module_list[i]);
		if (ops == NULL) {
			char *symbol_name = talloc_asprintf(ldb, "ldb_%s_module_ops",
												module_list[i]);
			if (symbol_name == NULL) {
				return LDB_ERR_OPERATIONS_ERROR;
			}
			ops = ldb_dso_load_symbol(ldb, module_list[i], symbol_name);
			talloc_free(symbol_name);
		}

		if (ops == NULL) {
			ldb_debug(ldb, LDB_DEBUG_WARNING, "WARNING: Module [%s] not found",
				  module_list[i]);
			continue;
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

int ldb_init_module_chain(struct ldb_context *ldb, struct ldb_module *module)
{
	while (module && module->ops->init_context == NULL)
		module = module->next;

	/* init is different in that it is not an error if modules
	 * do not require initialization */

	if (module) {
		int ret = module->ops->init_context(module);
		if (ret != LDB_SUCCESS) {
			ldb_debug(ldb, LDB_DEBUG_FATAL, "module %s initialization failed", module->ops->name);
			return ret;
		}
	}

	return LDB_SUCCESS;
}

int ldb_load_modules(struct ldb_context *ldb, const char *options[])
{
	const char **modules = NULL;
	int i;
	int ret;
	TALLOC_CTX *mem_ctx = talloc_new(ldb);
	if (!mem_ctx) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* find out which modules we are requested to activate */

	/* check if we have a custom module list passd as ldb option */
	if (options) {
		for (i = 0; options[i] != NULL; i++) {
			if (strncmp(options[i], LDB_MODULE_PREFIX, LDB_MODULE_PREFIX_LEN) == 0) {
				modules = ldb_modules_list_from_string(ldb, mem_ctx, &options[i][LDB_MODULE_PREFIX_LEN]);
			}
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
		ret = ldb_load_modules_list(ldb, modules, ldb->modules, &ldb->modules);
		if (ret != LDB_SUCCESS) {
			talloc_free(mem_ctx);
			return ret;
		}
	} else {
		ldb_debug(ldb, LDB_DEBUG_TRACE, "No modules specified for this database");
	}

	ret = ldb_init_module_chain(ldb, ldb->modules);
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
		ldb_module_done(request, NULL, NULL, ret);
	}
	return ret;
}

int ldb_next_init(struct ldb_module *module)
{
	module = module->next;

	return ldb_init_module_chain(module->ldb, module);
}

int ldb_next_start_trans(struct ldb_module *module)
{
	FIND_OP(module, start_transaction);
	return module->ops->start_transaction(module);
}

int ldb_next_end_trans(struct ldb_module *module)
{
	FIND_OP(module, end_transaction);
	return module->ops->end_transaction(module);
}

int ldb_next_prepare_commit(struct ldb_module *module)
{
	FIND_OP_NOERR(module, prepare_commit);
	if (module == NULL) {
		/* we are allowed to have no prepare commit in
		   backends */
		return LDB_SUCCESS;
	}
	return module->ops->prepare_commit(module);
}

int ldb_next_del_trans(struct ldb_module *module)
{
	FIND_OP(module, del_transaction);
	return module->ops->del_transaction(module);
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
 * 	error: LDB_SUCCESS for a succesful return
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

	req->callback(req, ares);
	return error;
}

/* to be used *only* in modules init functions.
 * this function i synchronous and will register
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

#ifndef STATIC_LIBLDB_MODULES

#ifdef HAVE_LDB_LDAP
#define LDAP_BACKEND LDB_BACKEND(ldap), LDB_BACKEND(ldapi), LDB_BACKEND(ldaps),
#else
#define LDAP_BACKEND
#endif

#ifdef HAVE_LDB_SQLITE3
#define SQLITE3_BACKEND LDB_BACKEND(sqlite3),
#else
#define SQLITE3_BACKEND
#endif

#define STATIC_LIBLDB_MODULES \
	LDB_BACKEND(tdb),	\
	LDAP_BACKEND	\
	SQLITE3_BACKEND	\
	LDB_MODULE(rdn_name),	\
	LDB_MODULE(paged_results),	\
	LDB_MODULE(server_sort),		\
	LDB_MODULE(asq), \
	NULL
#endif

/*
 * this is a bit hacked, as STATIC_LIBLDB_MODULES contains ','
 * between the elements and we want to autogenerate the
 * extern struct declarations, so we do some hacks and let the
 * ',' appear in an unused function prototype.
 */
#undef NULL
#define NULL LDB_MODULE(NULL),

#define LDB_BACKEND(name) \
	int); \
	extern const struct ldb_backend_ops ldb_ ## name ## _backend_ops;\
	extern void ldb_noop ## name (int
#define LDB_MODULE(name) \
	int); \
	extern const struct ldb_module_ops ldb_ ## name ## _module_ops;\
	extern void ldb_noop ## name (int

extern void ldb_start_noop(int,
STATIC_LIBLDB_MODULES
int);

#undef NULL
#define NULL { \
	.backend_ops = (void *)0, \
	.module_ops = (void *)0 \
}

#undef LDB_BACKEND
#define LDB_BACKEND(name) { \
	.backend_ops = &ldb_ ## name ## _backend_ops, \
	.module_ops = (void *)0 \
}
#undef LDB_MODULE
#define LDB_MODULE(name) { \
	.backend_ops = (void *)0, \
	.module_ops = &ldb_ ## name ## _module_ops \
}

static const struct ldb_builtins builtins[] = {
	STATIC_LIBLDB_MODULES
};
