/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004
   Copyright (C) Simo Sorce  2005-2006

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
 *  Component: ldb core API
 *
 *  Description: core API routines interfacing to ldb backends
 *
 *  Author: Andrew Tridgell
 */

#include "includes.h"
#include "ldb/include/includes.h"

/* 
   initialise a ldb context
   The mem_ctx is optional
*/
struct ldb_context *ldb_init(void *mem_ctx, struct tevent_context *tev_ctx)
{
	struct ldb_context *ldb = talloc_zero(mem_ctx, struct ldb_context);
	int ret;

	ret = ldb_setup_wellknown_attributes(ldb);
	if (ret != 0) {
		talloc_free(ldb);
		return NULL;
	}

	ldb_set_utf8_default(ldb);
	ldb_set_create_perms(ldb, 0600);

	return ldb;
}

static struct ldb_backend {
	const char *name;
	ldb_connect_fn connect_fn;
	struct ldb_backend *prev, *next;
} *ldb_backends = NULL;


static ldb_connect_fn ldb_find_backend(const char *url)
{
	struct ldb_backend *backend;

	for (backend = ldb_backends; backend; backend = backend->next) {
		if (strncmp(backend->name, url, strlen(backend->name)) == 0) {
			return backend->connect_fn;
		}
	}

	return NULL;
}

/*
 register a new ldb backend
*/
int ldb_register_backend(const char *url_prefix, ldb_connect_fn connectfn)
{
	struct ldb_backend *backend = talloc(talloc_autofree_context(), struct ldb_backend);

	if (ldb_find_backend(url_prefix)) {
		return LDB_SUCCESS;
	}

	/* Maybe check for duplicity here later on? */

	backend->name = talloc_strdup(backend, url_prefix);
	backend->connect_fn = connectfn;
	DLIST_ADD(ldb_backends, backend);

	return LDB_SUCCESS;
}

/* 
   Return the ldb module form of a database. The URL can either be one of the following forms
   ldb://path
   ldapi://path

   flags is made up of LDB_FLG_*

   the options are passed uninterpreted to the backend, and are
   backend specific.

  This allows modules to get at only the backend module, for example where a module 
  may wish to direct certain requests at a particular backend.
*/
int ldb_connect_backend(struct ldb_context *ldb, const char *url, const char *options[],
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
		if (ldb_try_load_dso(ldb, backend) == 0) {
			fn = ldb_find_backend(backend);
		}
	}

	talloc_free(backend);

	if (fn == NULL) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "Unable to find backend for '%s'\n", url);
		return LDB_ERR_OTHER;
	}

	ret = fn(ldb, url, ldb->flags, options, backend_module);

	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "Failed to connect to '%s'\n", url);
		return ret;
	}
	return ret;
}

/*
  try to autodetect a basedn if none specified. This fixes one of my
  pet hates about ldapsearch, which is that you have to get a long,
  complex basedn right to make any use of it.
*/
static const struct ldb_dn *ldb_set_default_basedn(struct ldb_context *ldb)
{
	TALLOC_CTX *tmp_ctx;
	int ret;
	static const char *attrs[] = { "defaultNamingContext", NULL };
	struct ldb_result *res;
	struct ldb_dn *basedn=NULL;

	basedn = (struct ldb_dn *)ldb_get_opaque(ldb, "default_baseDN");
	if (basedn) {
		return basedn;
	}

	tmp_ctx = talloc_new(ldb);
	ret = ldb_search(ldb, ldb, &res, ldb_dn_new(tmp_ctx, ldb, NULL), LDB_SCOPE_BASE, 
			 attrs, "(objectClass=*)");
	if (ret == LDB_SUCCESS) {
		if (res->count == 1) {
			basedn = ldb_msg_find_attr_as_dn(ldb, res->msgs[0], "defaultNamingContext");
			ldb_set_opaque(ldb, "default_baseDN", basedn);
		}
		talloc_free(res);
	}

	talloc_free(tmp_ctx);
	return basedn;
}

const struct ldb_dn *ldb_get_default_basedn(struct ldb_context *ldb)
{
	return (const struct ldb_dn *)ldb_get_opaque(ldb, "default_baseDN");
}

/* 
 connect to a database. The URL can either be one of the following forms
   ldb://path
   ldapi://path

   flags is made up of LDB_FLG_*

   the options are passed uninterpreted to the backend, and are
   backend specific
*/
int ldb_connect(struct ldb_context *ldb, const char *url, unsigned int flags, const char *options[])
{
	int ret;

	ldb->flags = flags;

	ret = ldb_connect_backend(ldb, url, options, &ldb->modules);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (ldb_load_modules(ldb, options) != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "Unable to load modules for %s: %s\n",
			  url, ldb_errstring(ldb));
		return LDB_ERR_OTHER;
	}

	/* TODO: get timeout from options if available there */
	ldb->default_timeout = 300; /* set default to 5 minutes */

	/* set the default base dn */
	ldb_set_default_basedn(ldb);

	return LDB_SUCCESS;
}

void ldb_set_errstring(struct ldb_context *ldb, const char *err_string)
{
	if (ldb->err_string) {
		talloc_free(ldb->err_string);
	}
	ldb->err_string = talloc_strdup(ldb, err_string);
}

void ldb_asprintf_errstring(struct ldb_context *ldb, const char *format, ...)
{
	va_list ap;

	if (ldb->err_string) {
		talloc_free(ldb->err_string);
	}

	va_start(ap, format);
	ldb->err_string = talloc_vasprintf(ldb, format, ap);
	va_end(ap);
}

void ldb_reset_err_string(struct ldb_context *ldb)
{
	if (ldb->err_string) {
		talloc_free(ldb->err_string);
		ldb->err_string = NULL;
	}
}

#define FIRST_OP(ldb, op) do { \
	module = ldb->modules;					\
	while (module && module->ops->op == NULL) module = module->next; \
	if (module == NULL) {						\
		ldb_asprintf_errstring(ldb, "unable to find module or backend to handle operation: " #op); \
		return LDB_ERR_OPERATIONS_ERROR;			\
	} \
} while (0)

/*
  start a transaction
*/
static int ldb_transaction_start_internal(struct ldb_context *ldb)
{
	struct ldb_module *module;
	int status;
	FIRST_OP(ldb, start_transaction);

	ldb_reset_err_string(ldb);

	status = module->ops->start_transaction(module);
	if (status != LDB_SUCCESS) {
		if (ldb->err_string == NULL) {
			/* no error string was setup by the backend */
			ldb_asprintf_errstring(ldb,
						"ldb transaction start: %s (%d)", 
						ldb_strerror(status), 
						status);
		}
	}
	return status;
}

/*
  commit a transaction
*/
static int ldb_transaction_commit_internal(struct ldb_context *ldb)
{
	struct ldb_module *module;
	int status;
	FIRST_OP(ldb, end_transaction);

	ldb_reset_err_string(ldb);

	status = module->ops->end_transaction(module);
	if (status != LDB_SUCCESS) {
		if (ldb->err_string == NULL) {
			/* no error string was setup by the backend */
			ldb_asprintf_errstring(ldb, 
						"ldb transaction commit: %s (%d)", 
						ldb_strerror(status), 
						status);
		}
	}
	return status;
}

/*
  cancel a transaction
*/
static int ldb_transaction_cancel_internal(struct ldb_context *ldb)
{
	struct ldb_module *module;
	int status;
	FIRST_OP(ldb, del_transaction);

	status = module->ops->del_transaction(module);
	if (status != LDB_SUCCESS) {
		if (ldb->err_string == NULL) {
			/* no error string was setup by the backend */
			ldb_asprintf_errstring(ldb, 
						"ldb transaction cancel: %s (%d)", 
						ldb_strerror(status), 
						status);
		}
	}
	return status;
}

int ldb_transaction_start(struct ldb_context *ldb)
{
	/* disable autotransactions */
	ldb->transaction_active++;

	return ldb_transaction_start_internal(ldb);
}

int ldb_transaction_commit(struct ldb_context *ldb)
{
	/* renable autotransactions (when we reach 0) */
	if (ldb->transaction_active > 0)
		ldb->transaction_active--;

	return ldb_transaction_commit_internal(ldb);
}

int ldb_transaction_cancel(struct ldb_context *ldb)
{
	/* renable autotransactions (when we reach 0) */
	if (ldb->transaction_active > 0)
		ldb->transaction_active--;

	return ldb_transaction_cancel_internal(ldb);
}

static int ldb_autotransaction_start(struct ldb_context *ldb)
{
	/* explicit transaction active, ignore autotransaction request */
	if (ldb->transaction_active)
		return LDB_SUCCESS;

	return ldb_transaction_start_internal(ldb);
}

static int ldb_autotransaction_commit(struct ldb_context *ldb)
{
	/* explicit transaction active, ignore autotransaction request */
	if (ldb->transaction_active)
		return LDB_SUCCESS;

	return ldb_transaction_commit_internal(ldb);
}

static int ldb_autotransaction_cancel(struct ldb_context *ldb)
{
	/* explicit transaction active, ignore autotransaction request */
	if (ldb->transaction_active)
		return LDB_SUCCESS;

	return ldb_transaction_cancel_internal(ldb);
}

/* autostarts a transacion if none active */
static int ldb_autotransaction_request(struct ldb_context *ldb, struct ldb_request *req)
{
	int ret;

	ret = ldb_autotransaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	if (ret == LDB_SUCCESS) {
		return ldb_autotransaction_commit(ldb);
	}
	ldb_autotransaction_cancel(ldb);

	if (ldb->err_string == NULL) {
		/* no error string was setup by the backend */
		ldb_asprintf_errstring(ldb, "%s (%d)", ldb_strerror(ret), ret);
	}

	return ret;
}

int ldb_wait(struct ldb_handle *handle, enum ldb_wait_type type)
{
	if (!handle) {
		return LDB_SUCCESS;
	}

	return handle->module->ops->wait(handle, type);
}

/* set the specified timeout or, if timeout is 0 set the default timeout */
/* timeout == -1 means no timeout */
int ldb_set_timeout(struct ldb_context *ldb, struct ldb_request *req, int timeout)
{
	if (req == NULL) return LDB_ERR_OPERATIONS_ERROR;
	
	if (timeout != 0) {
		req->timeout = timeout;
	} else {
		req->timeout = ldb->default_timeout;
	}
	req->starttime = time(NULL);

	return LDB_SUCCESS;
}

/* calculates the new timeout based on the previous starttime and timeout */
int ldb_set_timeout_from_prev_req(struct ldb_context *ldb, struct ldb_request *oldreq, struct ldb_request *newreq)
{
	time_t now;

	if (newreq == NULL) return LDB_ERR_OPERATIONS_ERROR;

	now = time(NULL);

	if (oldreq == NULL)
		return ldb_set_timeout(ldb, newreq, 0);

	if ((now - oldreq->starttime) > oldreq->timeout) {
		return LDB_ERR_TIME_LIMIT_EXCEEDED;
	}
	newreq->starttime = oldreq->starttime;
	newreq->timeout = oldreq->timeout - (now - oldreq->starttime);

	return LDB_SUCCESS;
}


/* 
   set the permissions for new files to be passed to open() in
   backends that use local files
 */
void ldb_set_create_perms(struct ldb_context *ldb, unsigned int perms)
{
	ldb->create_perms = perms;
}

/*
  start an ldb request
  NOTE: the request must be a talloc context.
  returns LDB_ERR_* on errors.
*/
int ldb_request(struct ldb_context *ldb, struct ldb_request *req)
{
	struct ldb_module *module;
	int ret;

	ldb_reset_err_string(ldb);

	/* call the first module in the chain */
	switch (req->operation) {
	case LDB_SEARCH:
		FIRST_OP(ldb, search);
		ret = module->ops->search(module, req);
		break;
	case LDB_ADD:
		FIRST_OP(ldb, add);
		ret = module->ops->add(module, req);
		break;
	case LDB_MODIFY:
		FIRST_OP(ldb, modify);
		ret = module->ops->modify(module, req);
		break;
	case LDB_DELETE:
		FIRST_OP(ldb, del);
		ret = module->ops->del(module, req);
		break;
	case LDB_RENAME:
		FIRST_OP(ldb, rename);
		ret = module->ops->rename(module, req);
		break;
	case LDB_SEQUENCE_NUMBER:
		FIRST_OP(ldb, sequence_number);
		ret = module->ops->sequence_number(module, req);
		break;
	default:
		FIRST_OP(ldb, request);
		ret = module->ops->request(module, req);
		break;
	}

	return ret;
}

/*
  search the database given a LDAP-like search expression

  returns an LDB error code

  Use talloc_free to free the ldb_message returned in 'res', if successful

*/
int ldb_search_default_callback(struct ldb_context *ldb, void *context, struct ldb_reply *ares)
{
	struct ldb_result *res;
	int n;
	
 	if (!context) {
		ldb_set_errstring(ldb, "NULL Context in callback");
		return LDB_ERR_OPERATIONS_ERROR;
	}	

	res = talloc_get_type(context, struct ldb_result);

	if (!res || !ares) {
		ldb_set_errstring(ldb, "NULL res or ares in callback");
		goto error;
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		res->msgs = talloc_realloc(res, res->msgs, struct ldb_message *, res->count + 2);
		if (! res->msgs) {
			goto error;
		}

		res->msgs[res->count + 1] = NULL;

		res->msgs[res->count] = talloc_move(res->msgs, &ares->message);
		res->count++;
		break;
	case LDB_REPLY_REFERRAL:
		if (res->refs) {
			for (n = 0; res->refs[n]; n++) /*noop*/ ;
		} else {
			n = 0;
		}

		res->refs = talloc_realloc(res, res->refs, char *, n + 2);
		if (! res->refs) {
			goto error;
		}

		res->refs[n] = talloc_move(res->refs, &ares->referral);
		res->refs[n + 1] = NULL;
	case LDB_REPLY_EXTENDED:
	case LDB_REPLY_DONE:
		/* TODO: we should really support controls on entries and referrals too! */
		res->controls = talloc_move(res, &ares->controls);
		break;		
	}
	talloc_free(ares);
	return LDB_SUCCESS;

error:
	talloc_free(ares);
	return LDB_ERR_OPERATIONS_ERROR;
}

int ldb_build_search_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			void *mem_ctx,
			const struct ldb_dn *base,
	       		enum ldb_scope scope,
			const char *expression,
			const char * const *attrs,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback)
{
	struct ldb_request *req;

	*ret_req = NULL;

	req = talloc(mem_ctx, struct ldb_request);
	if (req == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->operation = LDB_SEARCH;
	if (base == NULL) {
		req->op.search.base = ldb_dn_new(req, ldb, NULL);
	} else {
		req->op.search.base = base;
	}
	req->op.search.scope = scope;

	req->op.search.tree = ldb_parse_tree(req, expression);
	if (req->op.search.tree == NULL) {
		ldb_set_errstring(ldb, "Unable to parse search expression");
		talloc_free(req);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->op.search.attrs = attrs;
	req->controls = controls;
	req->context = context;
	req->callback = callback;

	*ret_req = req;
	return LDB_SUCCESS;
}

int ldb_build_add_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			void *mem_ctx,
			const struct ldb_message *message,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback)
{
	struct ldb_request *req;

	*ret_req = NULL;

	req = talloc(mem_ctx, struct ldb_request);
	if (req == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->operation = LDB_ADD;
	req->op.add.message = message;
	req->controls = controls;
	req->context = context;
	req->callback = callback;

	*ret_req = req;

	return LDB_SUCCESS;
}

int ldb_build_mod_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			void *mem_ctx,
			const struct ldb_message *message,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback)
{
	struct ldb_request *req;

	*ret_req = NULL;

	req = talloc(mem_ctx, struct ldb_request);
	if (req == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->operation = LDB_MODIFY;
	req->op.mod.message = message;
	req->controls = controls;
	req->context = context;
	req->callback = callback;

	*ret_req = req;

	return LDB_SUCCESS;
}

int ldb_build_del_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			void *mem_ctx,
			const struct ldb_dn *dn,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback)
{
	struct ldb_request *req;

	*ret_req = NULL;

	req = talloc(mem_ctx, struct ldb_request);
	if (req == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->operation = LDB_DELETE;
	req->op.del.dn = dn;
	req->controls = controls;
	req->context = context;
	req->callback = callback;

	*ret_req = req;

	return LDB_SUCCESS;
}

int ldb_build_rename_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			void *mem_ctx,
			const struct ldb_dn *olddn,
			const struct ldb_dn *newdn,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback)
{
	struct ldb_request *req;

	*ret_req = NULL;

	req = talloc(mem_ctx, struct ldb_request);
	if (req == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->operation = LDB_RENAME;
	req->op.rename.olddn = olddn;
	req->op.rename.newdn = newdn;
	req->controls = controls;
	req->context = context;
	req->callback = callback;

	*ret_req = req;

	return LDB_SUCCESS;
}

/*
  note that ldb_search() will automatically replace a NULL 'base' value with the 
  defaultNamingContext from the rootDSE if available.
*/
static int _ldb_search(struct ldb_context *ldb, TALLOC_CTX *mem_ctx,
			   struct ldb_result **_res,
			   const struct ldb_dn *base,
			   enum ldb_scope scope,
			   const char * const *attrs, 
			   const char *expression)
{
	struct ldb_request *req;
	int ret;
	struct ldb_result *res;

	*_res = NULL;

	res = talloc_zero(mem_ctx, struct ldb_result);
	if (!res) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_search_req(&req, ldb, mem_ctx,
					base?base:ldb_get_default_basedn(ldb),
	       				scope,
					expression,
					attrs,
					NULL,
					res,
					ldb_search_default_callback);

	if (ret != LDB_SUCCESS) goto done;

	ldb_set_timeout(ldb, req, 0); /* use default timeout */

	ret = ldb_request(ldb, req);
	
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	talloc_free(req);

done:
	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		res = NULL;
	}

	*_res = res;
	return ret;
}

/*
 a useful search function where you can easily define the expression and that
 takes a memory context where results are allocated
*/

int ldb_search(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, struct ldb_result **result,
                        struct ldb_dn *base, enum ldb_scope scope, const char * const *attrs,
                        const char *exp_fmt, ...)
{
	struct ldb_result *res;
	char *expression;
	va_list ap;
	int ret;

	expression = NULL;
	res = NULL;
	*result = NULL;

	if (exp_fmt) {
		va_start(ap, exp_fmt);
		expression = talloc_vasprintf(mem_ctx, exp_fmt, ap);
		va_end(ap);

		if ( ! expression) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	ret = _ldb_search(ldb, ldb, &res, base, scope, attrs, expression);

	if (ret == LDB_SUCCESS) {
		talloc_steal(mem_ctx, res);
		*result = res;
	} else {
		talloc_free(res);
	}

	talloc_free(expression);

	return ret;
}

/*
  add a record to the database. Will fail if a record with the given class and key
  already exists
*/
int ldb_add(struct ldb_context *ldb, 
	    const struct ldb_message *message)
{
	struct ldb_request *req;
	int ret;

	ret = ldb_msg_sanity_check(ldb, message);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_build_add_req(&req, ldb, ldb,
					message,
					NULL,
					NULL,
					NULL);

	if (ret != LDB_SUCCESS) return ret;

	ldb_set_timeout(ldb, req, 0); /* use default timeout */

	/* do request and autostart a transaction */
	ret = ldb_autotransaction_request(ldb, req);

	talloc_free(req);
	return ret;
}

/*
  modify the specified attributes of a record
*/
int ldb_modify(struct ldb_context *ldb, 
	       const struct ldb_message *message)
{
	struct ldb_request *req;
	int ret;

	ret = ldb_msg_sanity_check(ldb, message);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_build_mod_req(&req, ldb, ldb,
					message,
					NULL,
					NULL,
					NULL);

	if (ret != LDB_SUCCESS) return ret;

	ldb_set_timeout(ldb, req, 0); /* use default timeout */

	/* do request and autostart a transaction */
	ret = ldb_autotransaction_request(ldb, req);

	talloc_free(req);
	return ret;
}


/*
  delete a record from the database
*/
int ldb_delete(struct ldb_context *ldb, const struct ldb_dn *dn)
{
	struct ldb_request *req;
	int ret;

	ret = ldb_build_del_req(&req, ldb, ldb,
					dn,
					NULL,
					NULL,
					NULL);

	if (ret != LDB_SUCCESS) return ret;

	ldb_set_timeout(ldb, req, 0); /* use default timeout */

	/* do request and autostart a transaction */
	ret = ldb_autotransaction_request(ldb, req);

	talloc_free(req);
	return ret;
}

/*
  rename a record in the database
*/
int ldb_rename(struct ldb_context *ldb, const struct ldb_dn *olddn, const struct ldb_dn *newdn)
{
	struct ldb_request *req;
	int ret;

	ret = ldb_build_rename_req(&req, ldb, ldb,
					olddn,
					newdn,
					NULL,
					NULL,
					NULL);

	if (ret != LDB_SUCCESS) return ret;

	ldb_set_timeout(ldb, req, 0); /* use default timeout */

	/* do request and autostart a transaction */
	ret = ldb_autotransaction_request(ldb, req);

	talloc_free(req);
	return ret;
}


/*
  return the global sequence number
*/
int ldb_sequence_number(struct ldb_context *ldb, enum ldb_sequence_type type, uint64_t *seq_num)
{
	struct ldb_request *req;
	int ret;

	req = talloc(ldb, struct ldb_request);
	if (req == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->operation = LDB_SEQUENCE_NUMBER;
	req->controls = NULL;
	req->context = NULL;
	req->callback = NULL;
	ldb_set_timeout(ldb, req, 0); /* use default timeout */

	req->op.seq_num.type = type;
	/* do request and autostart a transaction */
	ret = ldb_request(ldb, req);
	
	if (ret == LDB_SUCCESS) {
		*seq_num = req->op.seq_num.seq_num;
	}

	talloc_free(req);
	return ret;
}



/*
  return extended error information 
*/
const char *ldb_errstring(struct ldb_context *ldb)
{
	if (ldb->err_string) {
		return ldb->err_string;
	}

	return NULL;
}

/*
  return a string explaining what a ldb error constant meancs
*/
const char *ldb_strerror(int ldb_err)
{
	switch (ldb_err) {
	case LDB_SUCCESS:
		return "Success";
	case LDB_ERR_OPERATIONS_ERROR:
		return "Operations error";
	case LDB_ERR_PROTOCOL_ERROR:
		return "Protocol error";
	case LDB_ERR_TIME_LIMIT_EXCEEDED:
		return "Time limit exceeded";
	case LDB_ERR_SIZE_LIMIT_EXCEEDED:
		return "Size limit exceeded";
	case LDB_ERR_COMPARE_FALSE:
		return "Compare false";
	case LDB_ERR_COMPARE_TRUE:
		return "Compare true";
	case LDB_ERR_AUTH_METHOD_NOT_SUPPORTED:
		return "Auth method not supported";
	case LDB_ERR_STRONG_AUTH_REQUIRED:
		return "Strong auth required";
/* 9 RESERVED */
	case LDB_ERR_REFERRAL:
		return "Referral error";
	case LDB_ERR_ADMIN_LIMIT_EXCEEDED:
		return "Admin limit exceeded";
	case LDB_ERR_UNSUPPORTED_CRITICAL_EXTENSION:
		return "Unsupported critical extension";
	case LDB_ERR_CONFIDENTIALITY_REQUIRED:
		return "Confidentiality required";
	case LDB_ERR_SASL_BIND_IN_PROGRESS:
		return "SASL bind in progress";
	case LDB_ERR_NO_SUCH_ATTRIBUTE:
		return "No such attribute";
	case LDB_ERR_UNDEFINED_ATTRIBUTE_TYPE:
		return "Undefined attribute type";
	case LDB_ERR_INAPPROPRIATE_MATCHING:
		return "Inappropriate matching";
	case LDB_ERR_CONSTRAINT_VIOLATION:
		return "Constraint violation";
	case LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS:
		return "Attribute or value exists";
	case LDB_ERR_INVALID_ATTRIBUTE_SYNTAX:
		return "Invalid attribute syntax";
/* 22-31 unused */
	case LDB_ERR_NO_SUCH_OBJECT:
		return "No such object";
	case LDB_ERR_ALIAS_PROBLEM:
		return "Alias problem";
	case LDB_ERR_INVALID_DN_SYNTAX:
		return "Invalid DN syntax";
/* 35 RESERVED */
	case LDB_ERR_ALIAS_DEREFERENCING_PROBLEM:
		return "Alias dereferencing problem";
/* 37-47 unused */
	case LDB_ERR_INAPPROPRIATE_AUTHENTICATION:
		return "Inappropriate authentication";
	case LDB_ERR_INVALID_CREDENTIALS:
		return "Invalid credentials";
	case LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS:
		return "insufficient access rights";
	case LDB_ERR_BUSY:
		return "Busy";
	case LDB_ERR_UNAVAILABLE:
		return "Unavailable";
	case LDB_ERR_UNWILLING_TO_PERFORM:
		return "Unwilling to perform";
	case LDB_ERR_LOOP_DETECT:
		return "Loop detect";
/* 55-63 unused */
	case LDB_ERR_NAMING_VIOLATION:
		return "Naming violation";
	case LDB_ERR_OBJECT_CLASS_VIOLATION:
		return "Object class violation";
	case LDB_ERR_NOT_ALLOWED_ON_NON_LEAF:
		return "Not allowed on non-leaf";
	case LDB_ERR_NOT_ALLOWED_ON_RDN:
		return "Not allowed on RDN";
	case LDB_ERR_ENTRY_ALREADY_EXISTS:
		return "Entry already exists";
	case LDB_ERR_OBJECT_CLASS_MODS_PROHIBITED:
		return "Object class mods prohibited";
/* 70 RESERVED FOR CLDAP */
	case LDB_ERR_AFFECTS_MULTIPLE_DSAS:
		return "Affects multiple DSAs";
/* 72-79 unused */
	case LDB_ERR_OTHER:
		return "Other";
	}

	return "Unknown error";
}

/*
  set backend specific opaque parameters
*/
int ldb_set_opaque(struct ldb_context *ldb, const char *name, void *value)
{
	struct ldb_opaque *o;

	/* allow updating an existing value */
	for (o=ldb->opaque;o;o=o->next) {
		if (strcmp(o->name, name) == 0) {
			o->value = value;
			return LDB_SUCCESS;
		}
	}

	o = talloc(ldb, struct ldb_opaque);
	if (o == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OTHER;
	}
	o->next = ldb->opaque;
	o->name = name;
	o->value = value;
	ldb->opaque = o;
	return LDB_SUCCESS;
}

/*
  get a previously set opaque value
*/
void *ldb_get_opaque(struct ldb_context *ldb, const char *name)
{
	struct ldb_opaque *o;
	for (o=ldb->opaque;o;o=o->next) {
		if (strcmp(o->name, name) == 0) {
			return o->value;
		}
	}
	return NULL;
}
