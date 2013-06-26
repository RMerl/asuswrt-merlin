/*
   ldb database library

   Copyright (C) Andrew Tridgell  2004
   Copyright (C) Simo Sorce  2005-2008

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

#define TEVENT_DEPRECATED 1
#include "ldb_private.h"
#include "ldb.h"

static int ldb_context_destructor(void *ptr)
{
	struct ldb_context *ldb = talloc_get_type(ptr, struct ldb_context);

	if (ldb->transaction_active) {
		ldb_debug(ldb, LDB_DEBUG_FATAL,
			  "A transaction is still active in ldb context [%p] on %s",
			  ldb, (const char *)ldb_get_opaque(ldb, "ldb_url"));
	}

	return 0;
}

/*
  this is used to catch debug messages from events
*/
static void ldb_tevent_debug(void *context, enum tevent_debug_level level,
			     const char *fmt, va_list ap)  PRINTF_ATTRIBUTE(3,0);

static void ldb_tevent_debug(void *context, enum tevent_debug_level level,
			     const char *fmt, va_list ap)
{
	struct ldb_context *ldb = talloc_get_type(context, struct ldb_context);
	enum ldb_debug_level ldb_level = LDB_DEBUG_FATAL;
	char *s = NULL;

	switch (level) {
	case TEVENT_DEBUG_FATAL:
		ldb_level = LDB_DEBUG_FATAL;
		break;
	case TEVENT_DEBUG_ERROR:
		ldb_level = LDB_DEBUG_ERROR;
		break;
	case TEVENT_DEBUG_WARNING:
		ldb_level = LDB_DEBUG_WARNING;
		break;
	case TEVENT_DEBUG_TRACE:
		ldb_level = LDB_DEBUG_TRACE;
		break;
	};

	vasprintf(&s, fmt, ap);
	if (!s) return;
	ldb_debug(ldb, ldb_level, "tevent: %s", s);
	free(s);
}

/*
   initialise a ldb context
   The mem_ctx is required
   The event_ctx is required
*/
struct ldb_context *ldb_init(TALLOC_CTX *mem_ctx, struct tevent_context *ev_ctx)
{
	struct ldb_context *ldb;
	int ret;
	const char *modules_path = getenv("LDB_MODULES_PATH");

	if (modules_path == NULL) {
		modules_path = LDB_MODULESDIR;
	}

	ret = ldb_modules_load(modules_path, LDB_VERSION);
	if (ret != LDB_SUCCESS) {
		return NULL;
	}

	ldb = talloc_zero(mem_ctx, struct ldb_context);
	/* A new event context so that callers who don't want ldb
	 * operating on thier global event context can work without
	 * having to provide their own private one explicitly */
	if (ev_ctx == NULL) {
		ev_ctx = tevent_context_init(ldb);
		tevent_set_debug(ev_ctx, ldb_tevent_debug, ldb);
		tevent_loop_allow_nesting(ev_ctx);
	}

	ret = ldb_setup_wellknown_attributes(ldb);
	if (ret != LDB_SUCCESS) {
		talloc_free(ldb);
		return NULL;
	}

	ldb_set_utf8_default(ldb);
	ldb_set_create_perms(ldb, 0666);
	ldb_set_modules_dir(ldb, LDB_MODULESDIR);
	ldb_set_event_context(ldb, ev_ctx);

	/* TODO: get timeout from options if available there */
	ldb->default_timeout = 300; /* set default to 5 minutes */

	talloc_set_destructor((TALLOC_CTX *)ldb, ldb_context_destructor);

	return ldb;
}

/*
  try to autodetect a basedn if none specified. This fixes one of my
  pet hates about ldapsearch, which is that you have to get a long,
  complex basedn right to make any use of it.
*/
void ldb_set_default_dns(struct ldb_context *ldb)
{
	TALLOC_CTX *tmp_ctx;
	int ret;
	struct ldb_result *res;
	struct ldb_dn *tmp_dn=NULL;
	static const char *attrs[] = {
		"rootDomainNamingContext",
		"configurationNamingContext",
		"schemaNamingContext",
		"defaultNamingContext",
		NULL
	};

	tmp_ctx = talloc_new(ldb);
	ret = ldb_search(ldb, tmp_ctx, &res, ldb_dn_new(tmp_ctx, ldb, NULL),
			 LDB_SCOPE_BASE, attrs, "(objectClass=*)");
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return;
	}

	if (res->count != 1) {
		talloc_free(tmp_ctx);
		return;
	}

	if (!ldb_get_opaque(ldb, "rootDomainNamingContext")) {
		tmp_dn = ldb_msg_find_attr_as_dn(ldb, ldb, res->msgs[0],
						 "rootDomainNamingContext");
		ldb_set_opaque(ldb, "rootDomainNamingContext", tmp_dn);
	}

	if (!ldb_get_opaque(ldb, "configurationNamingContext")) {
		tmp_dn = ldb_msg_find_attr_as_dn(ldb, ldb, res->msgs[0],
						 "configurationNamingContext");
		ldb_set_opaque(ldb, "configurationNamingContext", tmp_dn);
	}

	if (!ldb_get_opaque(ldb, "schemaNamingContext")) {
		tmp_dn = ldb_msg_find_attr_as_dn(ldb, ldb, res->msgs[0],
						 "schemaNamingContext");
		ldb_set_opaque(ldb, "schemaNamingContext", tmp_dn);
	}

	if (!ldb_get_opaque(ldb, "defaultNamingContext")) {
		tmp_dn = ldb_msg_find_attr_as_dn(ldb, ldb, res->msgs[0],
						 "defaultNamingContext");
		ldb_set_opaque(ldb, "defaultNamingContext", tmp_dn);
	}

	talloc_free(tmp_ctx);
}

struct ldb_dn *ldb_get_root_basedn(struct ldb_context *ldb)
{
	void *opaque = ldb_get_opaque(ldb, "rootDomainNamingContext");
	return talloc_get_type(opaque, struct ldb_dn);
}

struct ldb_dn *ldb_get_config_basedn(struct ldb_context *ldb)
{
	void *opaque = ldb_get_opaque(ldb, "configurationNamingContext");
	return talloc_get_type(opaque, struct ldb_dn);
}

struct ldb_dn *ldb_get_schema_basedn(struct ldb_context *ldb)
{
	void *opaque = ldb_get_opaque(ldb, "schemaNamingContext");
	return talloc_get_type(opaque, struct ldb_dn);
}

struct ldb_dn *ldb_get_default_basedn(struct ldb_context *ldb)
{
	void *opaque = ldb_get_opaque(ldb, "defaultNamingContext");
	return talloc_get_type(opaque, struct ldb_dn);
}

/*
   connect to a database. The URL can either be one of the following forms
   ldb://path
   ldapi://path

   flags is made up of LDB_FLG_*

   the options are passed uninterpreted to the backend, and are
   backend specific
*/
int ldb_connect(struct ldb_context *ldb, const char *url,
		unsigned int flags, const char *options[])
{
	int ret;
	char *url2;
	/* We seem to need to do this here, or else some utilities don't
	 * get ldb backends */

	ldb->flags = flags;

	url2 = talloc_strdup(ldb, url);
	if (!url2) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ret = ldb_set_opaque(ldb, "ldb_url", url2);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_module_connect_backend(ldb, url, options, &ldb->modules);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (ldb_load_modules(ldb, options) != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_FATAL,
			  "Unable to load modules for %s: %s",
			  url, ldb_errstring(ldb));
		return LDB_ERR_OTHER;
	}

	/* set the default base dn */
	ldb_set_default_dns(ldb);

	return LDB_SUCCESS;
}

void ldb_set_errstring(struct ldb_context *ldb, const char *err_string)
{
	if (ldb->err_string) {
		talloc_free(ldb->err_string);
	}
	ldb->err_string = talloc_strdup(ldb, err_string);
	if (ldb->flags & LDB_FLG_ENABLE_TRACING) {
		ldb_debug(ldb, LDB_DEBUG_TRACE, "ldb_set_errstring: %s", ldb->err_string);
	}
}

void ldb_asprintf_errstring(struct ldb_context *ldb, const char *format, ...)
{
	va_list ap;
	char *old_string = NULL;

	if (ldb->err_string) {
		old_string = ldb->err_string;
	}

	va_start(ap, format);
	ldb->err_string = talloc_vasprintf(ldb, format, ap);
	va_end(ap);
	talloc_free(old_string);
}

void ldb_reset_err_string(struct ldb_context *ldb)
{
	if (ldb->err_string) {
		talloc_free(ldb->err_string);
		ldb->err_string = NULL;
	}
}



/*
  set an ldb error based on file:line
*/
int ldb_error_at(struct ldb_context *ldb, int ecode,
		 const char *reason, const char *file, int line)
{
	if (reason == NULL) {
		reason = ldb_strerror(ecode);
	}
	ldb_asprintf_errstring(ldb, "%s at %s:%d", reason, file, line);
	return ecode;
}


#define FIRST_OP_NOERR(ldb, op) do { \
	module = ldb->modules;					\
	while (module && module->ops->op == NULL) module = module->next; \
	if ((ldb->flags & LDB_FLG_ENABLE_TRACING) && module) { \
		ldb_debug(ldb, LDB_DEBUG_TRACE, "ldb_trace_request: (%s)->" #op, \
			  module->ops->name);				\
	}								\
} while (0)

#define FIRST_OP(ldb, op) do { \
	FIRST_OP_NOERR(ldb, op); \
	if (module == NULL) {	       				\
		ldb_asprintf_errstring(ldb, "unable to find module or backend to handle operation: " #op); \
		return LDB_ERR_OPERATIONS_ERROR;			\
	} \
} while (0)


/*
  start a transaction
*/
int ldb_transaction_start(struct ldb_context *ldb)
{
	struct ldb_module *module;
	int status;

	ldb_debug(ldb, LDB_DEBUG_TRACE,
		  "start ldb transaction (nesting: %d)",
		  ldb->transaction_active);

	/* explicit transaction active, count nested requests */
	if (ldb->transaction_active) {
		ldb->transaction_active++;
		return LDB_SUCCESS;
	}

	/* start a new transaction */
	ldb->transaction_active++;
	ldb->prepare_commit_done = false;

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
	if ((module && module->ldb->flags & LDB_FLG_ENABLE_TRACING)) { 
		ldb_debug(module->ldb, LDB_DEBUG_TRACE, "start ldb transaction error: %s", 
			  ldb_errstring(module->ldb));				
	}
	return status;
}

/*
  prepare for transaction commit (first phase of two phase commit)
*/
int ldb_transaction_prepare_commit(struct ldb_context *ldb)
{
	struct ldb_module *module;
	int status;

	if (ldb->prepare_commit_done) {
		return LDB_SUCCESS;
	}

	/* commit only when all nested transactions are complete */
	if (ldb->transaction_active > 1) {
		return LDB_SUCCESS;
	}

	ldb->prepare_commit_done = true;

	if (ldb->transaction_active < 0) {
		ldb_debug(ldb, LDB_DEBUG_FATAL,
			  "prepare commit called but no ldb transactions are active!");
		ldb->transaction_active = 0;
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* call prepare transaction if available */
	FIRST_OP_NOERR(ldb, prepare_commit);
	if (module == NULL) {
		return LDB_SUCCESS;
	}

	status = module->ops->prepare_commit(module);
	if (status != LDB_SUCCESS) {
		/* if a module fails the prepare then we need
		   to call the end transaction for everyone */
		FIRST_OP(ldb, del_transaction);
		module->ops->del_transaction(module);
		if (ldb->err_string == NULL) {
			/* no error string was setup by the backend */
			ldb_asprintf_errstring(ldb,
					       "ldb transaction prepare commit: %s (%d)",
					       ldb_strerror(status),
					       status);
		}
		if ((module && module->ldb->flags & LDB_FLG_ENABLE_TRACING)) { 
			ldb_debug(module->ldb, LDB_DEBUG_TRACE, "prepare commit transaction error: %s", 
				  ldb_errstring(module->ldb));				
		}
	}

	return status;
}


/*
  commit a transaction
*/
int ldb_transaction_commit(struct ldb_context *ldb)
{
	struct ldb_module *module;
	int status;

	status = ldb_transaction_prepare_commit(ldb);
	if (status != LDB_SUCCESS) {
		return status;
	}

	ldb->transaction_active--;

	ldb_debug(ldb, LDB_DEBUG_TRACE,
		  "commit ldb transaction (nesting: %d)",
		  ldb->transaction_active);

	/* commit only when all nested transactions are complete */
	if (ldb->transaction_active > 0) {
		return LDB_SUCCESS;
	}

	if (ldb->transaction_active < 0) {
		ldb_debug(ldb, LDB_DEBUG_FATAL,
			  "commit called but no ldb transactions are active!");
		ldb->transaction_active = 0;
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ldb_reset_err_string(ldb);

	FIRST_OP(ldb, end_transaction);
	status = module->ops->end_transaction(module);
	if (status != LDB_SUCCESS) {
		if (ldb->err_string == NULL) {
			/* no error string was setup by the backend */
			ldb_asprintf_errstring(ldb,
				"ldb transaction commit: %s (%d)",
				ldb_strerror(status),
				status);
		}
		if ((module && module->ldb->flags & LDB_FLG_ENABLE_TRACING)) { 
			ldb_debug(module->ldb, LDB_DEBUG_TRACE, "commit ldb transaction error: %s", 
				  ldb_errstring(module->ldb));				
		}
		/* cancel the transaction */
		FIRST_OP(ldb, del_transaction);
		module->ops->del_transaction(module);
	}
	return status;
}


/*
  cancel a transaction
*/
int ldb_transaction_cancel(struct ldb_context *ldb)
{
	struct ldb_module *module;
	int status;

	ldb->transaction_active--;

	ldb_debug(ldb, LDB_DEBUG_TRACE,
		  "cancel ldb transaction (nesting: %d)",
		  ldb->transaction_active);

	/* really cancel only if all nested transactions are complete */
	if (ldb->transaction_active > 0) {
		return LDB_SUCCESS;
	}

	if (ldb->transaction_active < 0) {
		ldb_debug(ldb, LDB_DEBUG_FATAL,
			  "cancel called but no ldb transactions are active!");
		ldb->transaction_active = 0;
		return LDB_ERR_OPERATIONS_ERROR;
	}

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
		if ((module && module->ldb->flags & LDB_FLG_ENABLE_TRACING)) { 
			ldb_debug(module->ldb, LDB_DEBUG_TRACE, "cancel ldb transaction error: %s", 
				  ldb_errstring(module->ldb));				
		}
	}
	return status;
}

/*
  cancel a transaction with no error if no transaction is pending
  used when we fork() to clear any parent transactions
*/
int ldb_transaction_cancel_noerr(struct ldb_context *ldb)
{
	if (ldb->transaction_active > 0) {
		return ldb_transaction_cancel(ldb);
	}
	return LDB_SUCCESS;
}


/* autostarts a transacion if none active */
static int ldb_autotransaction_request(struct ldb_context *ldb,
				       struct ldb_request *req)
{
	int ret;

	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	if (ret == LDB_SUCCESS) {
		return ldb_transaction_commit(ldb);
	}
	ldb_transaction_cancel(ldb);

	if (ldb->err_string == NULL) {
		/* no error string was setup by the backend */
		ldb_asprintf_errstring(ldb, "%s (%d)", ldb_strerror(ret), ret);
	}

	return ret;
}

int ldb_wait(struct ldb_handle *handle, enum ldb_wait_type type)
{
	struct tevent_context *ev;
	int ret;

	if (!handle) {
		return LDB_ERR_UNAVAILABLE;
	}

	if (handle->state == LDB_ASYNC_DONE) {
		return handle->status;
	}

	ev = ldb_get_event_context(handle->ldb);
	if (NULL == ev) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	switch (type) {
	case LDB_WAIT_NONE:
		ret = tevent_loop_once(ev);
		if (ret != 0) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		if (handle->state == LDB_ASYNC_DONE ||
		    handle->status != LDB_SUCCESS) {
			return handle->status;
		}
		break;

	case LDB_WAIT_ALL:
		while (handle->state != LDB_ASYNC_DONE) {
			ret = tevent_loop_once(ev);
			if (ret != 0) {
				return LDB_ERR_OPERATIONS_ERROR;
			}
			if (handle->status != LDB_SUCCESS) {
				return handle->status;
			}
		}
		return handle->status;
	}

	return LDB_SUCCESS;
}

/* set the specified timeout or, if timeout is 0 set the default timeout */
int ldb_set_timeout(struct ldb_context *ldb,
		    struct ldb_request *req,
		    int timeout)
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
int ldb_set_timeout_from_prev_req(struct ldb_context *ldb,
				  struct ldb_request *oldreq,
				  struct ldb_request *newreq)
{
	if (newreq == NULL) return LDB_ERR_OPERATIONS_ERROR;

	if (oldreq == NULL) {
		return ldb_set_timeout(ldb, newreq, 0);
	}

	newreq->starttime = oldreq->starttime;
	newreq->timeout = oldreq->timeout;

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

unsigned int ldb_get_create_perms(struct ldb_context *ldb)
{
	return ldb->create_perms;
}

void ldb_set_event_context(struct ldb_context *ldb, struct tevent_context *ev)
{
	ldb->ev_ctx = ev;
}

struct tevent_context * ldb_get_event_context(struct ldb_context *ldb)
{
	return ldb->ev_ctx;
}

void ldb_request_set_state(struct ldb_request *req, int state)
{
	req->handle->state = state;
}

int ldb_request_get_status(struct ldb_request *req)
{
	return req->handle->status;
}


/*
  trace a ldb request
*/
static void ldb_trace_request(struct ldb_context *ldb, struct ldb_request *req)
{
	TALLOC_CTX *tmp_ctx = talloc_new(req);
	unsigned int i;

	switch (req->operation) {
	case LDB_SEARCH:
		ldb_debug_add(ldb, "ldb_trace_request: SEARCH\n");
		ldb_debug_add(ldb, " dn: %s\n",
			      ldb_dn_is_null(req->op.search.base)?"<rootDSE>":
			      ldb_dn_get_linearized(req->op.search.base));
		ldb_debug_add(ldb, " scope: %s\n", 
			  req->op.search.scope==LDB_SCOPE_BASE?"base":
			  req->op.search.scope==LDB_SCOPE_ONELEVEL?"one":
			  req->op.search.scope==LDB_SCOPE_SUBTREE?"sub":"UNKNOWN");
		ldb_debug_add(ldb, " expr: %s\n", 
			  ldb_filter_from_tree(tmp_ctx, req->op.search.tree));
		if (req->op.search.attrs == NULL) {
			ldb_debug_add(ldb, " attr: <ALL>\n");
		} else {
			for (i=0; req->op.search.attrs[i]; i++) {
				ldb_debug_add(ldb, " attr: %s\n", req->op.search.attrs[i]);
			}
		}
		break;
	case LDB_DELETE:
		ldb_debug_add(ldb, "ldb_trace_request: DELETE\n");
		ldb_debug_add(ldb, " dn: %s\n", 
			      ldb_dn_get_linearized(req->op.del.dn));
		break;
	case LDB_RENAME:
		ldb_debug_add(ldb, "ldb_trace_request: RENAME\n");
		ldb_debug_add(ldb, " olddn: %s\n", 
			      ldb_dn_get_linearized(req->op.rename.olddn));
		ldb_debug_add(ldb, " newdn: %s\n", 
			      ldb_dn_get_linearized(req->op.rename.newdn));
		break;
	case LDB_EXTENDED:
		ldb_debug_add(ldb, "ldb_trace_request: EXTENDED\n");
		ldb_debug_add(ldb, " oid: %s\n", req->op.extended.oid);
		ldb_debug_add(ldb, " data: %s\n", req->op.extended.data?"yes":"no");
		break;
	case LDB_ADD:
		ldb_debug_add(ldb, "ldb_trace_request: ADD\n");
		ldb_debug_add(req->handle->ldb, "%s\n", 
			      ldb_ldif_message_string(req->handle->ldb, tmp_ctx, 
						      LDB_CHANGETYPE_ADD, 
						      req->op.add.message));
		break;
	case LDB_MODIFY:
		ldb_debug_add(ldb, "ldb_trace_request: MODIFY\n");
		ldb_debug_add(req->handle->ldb, "%s\n", 
			      ldb_ldif_message_string(req->handle->ldb, tmp_ctx, 
						      LDB_CHANGETYPE_ADD, 
						      req->op.mod.message));
		break;
	case LDB_REQ_REGISTER_CONTROL:
		ldb_debug_add(ldb, "ldb_trace_request: REGISTER_CONTROL\n");
		ldb_debug_add(req->handle->ldb, "%s\n", 
			      req->op.reg_control.oid);
		break;
	case LDB_REQ_REGISTER_PARTITION:
		ldb_debug_add(ldb, "ldb_trace_request: REGISTER_PARTITION\n");
		ldb_debug_add(req->handle->ldb, "%s\n", 
			      ldb_dn_get_linearized(req->op.reg_partition.dn));
		break;
	default:
		ldb_debug_add(ldb, "ldb_trace_request: UNKNOWN(%u)\n", 
			      req->operation);
		break;
	}

	if (req->controls == NULL) {
		ldb_debug_add(ldb, " control: <NONE>\n");
	} else {
		for (i=0; req->controls && req->controls[i]; i++) {
			if (req->controls[i]->oid) {
				ldb_debug_add(ldb, " control: %s  crit:%u  data:%s\n",
					      req->controls[i]->oid,
					      req->controls[i]->critical,
					      req->controls[i]->data?"yes":"no");
			}
		}
	}
	
	ldb_debug_end(ldb, LDB_DEBUG_TRACE);

	talloc_free(tmp_ctx);
}

/*
  check that the element flags don't have any internal bits set
 */
static int ldb_msg_check_element_flags(struct ldb_context *ldb,
				       const struct ldb_message *message)
{
	unsigned i;
	for (i=0; i<message->num_elements; i++) {
		if (message->elements[i].flags & LDB_FLAG_INTERNAL_MASK) {
			ldb_asprintf_errstring(ldb, "Invalid element flags 0x%08x on element %s in %s\n",
					       message->elements[i].flags, message->elements[i].name,
					       ldb_dn_get_linearized(message->dn));
			return LDB_ERR_UNSUPPORTED_CRITICAL_EXTENSION;
		}
	}
	return LDB_SUCCESS;
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

	if (req->callback == NULL) {
		ldb_set_errstring(ldb, "Requests MUST define callbacks");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	ldb_reset_err_string(ldb);

	if (ldb->flags & LDB_FLG_ENABLE_TRACING) {
		ldb_trace_request(ldb, req);
	}

	/* call the first module in the chain */
	switch (req->operation) {
	case LDB_SEARCH:
		/* due to "ldb_build_search_req" base DN always != NULL */
		if (!ldb_dn_validate(req->op.search.base)) {
			ldb_asprintf_errstring(ldb, "ldb_search: invalid basedn '%s'",
					       ldb_dn_get_linearized(req->op.search.base));
			return LDB_ERR_INVALID_DN_SYNTAX;
		}
		FIRST_OP(ldb, search);
		ret = module->ops->search(module, req);
		break;
	case LDB_ADD:
		if (!ldb_dn_validate(req->op.add.message->dn)) {
			ldb_asprintf_errstring(ldb, "ldb_add: invalid dn '%s'",
					       ldb_dn_get_linearized(req->op.add.message->dn));
			return LDB_ERR_INVALID_DN_SYNTAX;
		}
		/*
		 * we have to normalize here, as so many places
		 * in modules and backends assume we don't have two
		 * elements with the same name
		 */
		ret = ldb_msg_normalize(ldb, req, req->op.add.message,
		                        discard_const(&req->op.add.message));
		if (ret != LDB_SUCCESS) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		FIRST_OP(ldb, add);
		ret = ldb_msg_check_element_flags(ldb, req->op.add.message);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
		ret = module->ops->add(module, req);
		break;
	case LDB_MODIFY:
		if (!ldb_dn_validate(req->op.mod.message->dn)) {
			ldb_asprintf_errstring(ldb, "ldb_modify: invalid dn '%s'",
					       ldb_dn_get_linearized(req->op.mod.message->dn));
			return LDB_ERR_INVALID_DN_SYNTAX;
		}
		FIRST_OP(ldb, modify);
		ret = ldb_msg_check_element_flags(ldb, req->op.mod.message);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
		ret = module->ops->modify(module, req);
		break;
	case LDB_DELETE:
		if (!ldb_dn_validate(req->op.del.dn)) {
			ldb_asprintf_errstring(ldb, "ldb_delete: invalid dn '%s'",
					       ldb_dn_get_linearized(req->op.del.dn));
			return LDB_ERR_INVALID_DN_SYNTAX;
		}
		FIRST_OP(ldb, del);
		ret = module->ops->del(module, req);
		break;
	case LDB_RENAME:
		if (!ldb_dn_validate(req->op.rename.olddn)) {
			ldb_asprintf_errstring(ldb, "ldb_rename: invalid olddn '%s'",
					       ldb_dn_get_linearized(req->op.rename.olddn));
			return LDB_ERR_INVALID_DN_SYNTAX;
		}
		if (!ldb_dn_validate(req->op.rename.newdn)) {
			ldb_asprintf_errstring(ldb, "ldb_rename: invalid newdn '%s'",
					       ldb_dn_get_linearized(req->op.rename.newdn));
			return LDB_ERR_INVALID_DN_SYNTAX;
		}
		FIRST_OP(ldb, rename);
		ret = module->ops->rename(module, req);
		break;
	case LDB_EXTENDED:
		FIRST_OP(ldb, extended);
		ret = module->ops->extended(module, req);
		break;
	default:
		FIRST_OP(ldb, request);
		ret = module->ops->request(module, req);
		break;
	}

	return ret;
}

int ldb_request_done(struct ldb_request *req, int status)
{
	req->handle->state = LDB_ASYNC_DONE;
	req->handle->status = status;
	return status;
}

/*
  search the database given a LDAP-like search expression

  returns an LDB error code

  Use talloc_free to free the ldb_message returned in 'res', if successful

*/
int ldb_search_default_callback(struct ldb_request *req,
				struct ldb_reply *ares)
{
	struct ldb_result *res;
	unsigned int n;

	res = talloc_get_type(req->context, struct ldb_result);

	if (!ares) {
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_request_done(req, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		res->msgs = talloc_realloc(res, res->msgs,
					struct ldb_message *, res->count + 2);
		if (! res->msgs) {
			return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
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
			return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
		}

		res->refs[n] = talloc_move(res->refs, &ares->referral);
		res->refs[n + 1] = NULL;
		break;

	case LDB_REPLY_DONE:
		/* TODO: we should really support controls on entries
		 * and referrals too! */
		res->controls = talloc_move(res, &ares->controls);

		/* this is the last message, and means the request is done */
		/* we have to signal and eventual ldb_wait() waiting that the
		 * async request operation was completed */
		talloc_free(ares);
		return ldb_request_done(req, LDB_SUCCESS);
	}

	talloc_free(ares);

	return LDB_SUCCESS;
}

int ldb_modify_default_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_result *res;
	unsigned int n;
	int ret;

	res = talloc_get_type(req->context, struct ldb_result);

	if (!ares) {
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}

	if (ares->error != LDB_SUCCESS) {
		ret = ares->error;
		talloc_free(ares);
		return ldb_request_done(req, ret);
	}

	switch (ares->type) {
	case LDB_REPLY_REFERRAL:
		if (res->refs) {
			for (n = 0; res->refs[n]; n++) /*noop*/ ;
		} else {
			n = 0;
		}

		res->refs = talloc_realloc(res, res->refs, char *, n + 2);
		if (! res->refs) {
			return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
		}

		res->refs[n] = talloc_move(res->refs, &ares->referral);
		res->refs[n + 1] = NULL;
		break;

	case LDB_REPLY_DONE:
		talloc_free(ares);
		return ldb_request_done(req, LDB_SUCCESS);
	default:
		talloc_free(ares);
		ldb_set_errstring(req->handle->ldb, "Invalid reply type!");
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}

	talloc_free(ares);
	return ldb_request_done(req, LDB_SUCCESS);
}

int ldb_op_default_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	int ret;

	if (!ares) {
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}

	if (ares->error != LDB_SUCCESS) {
		ret = ares->error;
		talloc_free(ares);
		return ldb_request_done(req, ret);
	}

	if (ares->type != LDB_REPLY_DONE) {
		talloc_free(ares);
		ldb_set_errstring(req->handle->ldb, "Invalid reply type!");
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}

	talloc_free(ares);
	return ldb_request_done(req, LDB_SUCCESS);
}

int ldb_build_search_req_ex(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			struct ldb_dn *base,
	       		enum ldb_scope scope,
			struct ldb_parse_tree *tree,
			const char * const *attrs,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback,
			struct ldb_request *parent)
{
	struct ldb_request *req;

	*ret_req = NULL;

	req = talloc(mem_ctx, struct ldb_request);
	if (req == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->operation = LDB_SEARCH;
	if (base == NULL) {
		req->op.search.base = ldb_dn_new(req, ldb, NULL);
	} else {
		req->op.search.base = base;
	}
	req->op.search.scope = scope;

	req->op.search.tree = tree;
	if (req->op.search.tree == NULL) {
		ldb_set_errstring(ldb, "'tree' can't be NULL");
		talloc_free(req);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->op.search.attrs = attrs;
	req->controls = controls;
	req->context = context;
	req->callback = callback;

	ldb_set_timeout_from_prev_req(ldb, parent, req);

	req->handle = ldb_handle_new(req, ldb);
	if (req->handle == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (parent) {
		req->handle->nesting++;
		req->handle->parent = parent;
		req->handle->flags = parent->handle->flags;
	}

	*ret_req = req;
	return LDB_SUCCESS;
}

int ldb_build_search_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			struct ldb_dn *base,
			enum ldb_scope scope,
			const char *expression,
			const char * const *attrs,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback,
			struct ldb_request *parent)
{
	struct ldb_parse_tree *tree;
	int ret;

	tree = ldb_parse_tree(mem_ctx, expression);
	if (tree == NULL) {
		ldb_set_errstring(ldb, "Unable to parse search expression");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_search_req_ex(ret_req, ldb, mem_ctx, base,
				      scope, tree, attrs, controls,
				      context, callback, parent);
	if (ret == LDB_SUCCESS) {
		talloc_steal(*ret_req, tree);
	}
	return ret;
}

int ldb_build_add_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			const struct ldb_message *message,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback,
			struct ldb_request *parent)
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

	ldb_set_timeout_from_prev_req(ldb, parent, req);

	req->handle = ldb_handle_new(req, ldb);
	if (req->handle == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (parent) {
		req->handle->nesting++;
		req->handle->parent = parent;
		req->handle->flags = parent->handle->flags;
	}

	*ret_req = req;

	return LDB_SUCCESS;
}

int ldb_build_mod_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			const struct ldb_message *message,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback,
			struct ldb_request *parent)
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

	ldb_set_timeout_from_prev_req(ldb, parent, req);

	req->handle = ldb_handle_new(req, ldb);
	if (req->handle == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (parent) {
		req->handle->nesting++;
		req->handle->parent = parent;
		req->handle->flags = parent->handle->flags;
	}

	*ret_req = req;

	return LDB_SUCCESS;
}

int ldb_build_del_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			struct ldb_dn *dn,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback,
			struct ldb_request *parent)
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

	ldb_set_timeout_from_prev_req(ldb, parent, req);

	req->handle = ldb_handle_new(req, ldb);
	if (req->handle == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (parent) {
		req->handle->nesting++;
		req->handle->parent = parent;
		req->handle->flags = parent->handle->flags;
	}

	*ret_req = req;

	return LDB_SUCCESS;
}

int ldb_build_rename_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			struct ldb_dn *olddn,
			struct ldb_dn *newdn,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback,
			struct ldb_request *parent)
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

	ldb_set_timeout_from_prev_req(ldb, parent, req);

	req->handle = ldb_handle_new(req, ldb);
	if (req->handle == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (parent) {
		req->handle->nesting++;
		req->handle->parent = parent;
		req->handle->flags = parent->handle->flags;
	}

	*ret_req = req;

	return LDB_SUCCESS;
}

int ldb_extended_default_callback(struct ldb_request *req,
				  struct ldb_reply *ares)
{
	struct ldb_result *res;

	res = talloc_get_type(req->context, struct ldb_result);

	if (!ares) {
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_request_done(req, ares->error);
	}

	if (ares->type == LDB_REPLY_DONE) {

		/* TODO: we should really support controls on entries and referrals too! */
		res->extended = talloc_move(res, &ares->response);
		res->controls = talloc_move(res, &ares->controls);

		talloc_free(ares);
		return ldb_request_done(req, LDB_SUCCESS);
	}

	talloc_free(ares);
	ldb_set_errstring(req->handle->ldb, "Invalid reply type!");
	return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
}

int ldb_build_extended_req(struct ldb_request **ret_req,
			   struct ldb_context *ldb,
			   TALLOC_CTX *mem_ctx,
			   const char *oid,
			   void *data,
			   struct ldb_control **controls,
			   void *context,
			   ldb_request_callback_t callback,
			   struct ldb_request *parent)
{
	struct ldb_request *req;

	*ret_req = NULL;

	req = talloc(mem_ctx, struct ldb_request);
	if (req == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	req->operation = LDB_EXTENDED;
	req->op.extended.oid = oid;
	req->op.extended.data = data;
	req->controls = controls;
	req->context = context;
	req->callback = callback;

	ldb_set_timeout_from_prev_req(ldb, parent, req);

	req->handle = ldb_handle_new(req, ldb);
	if (req->handle == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (parent) {
		req->handle->nesting++;
		req->handle->parent = parent;
		req->handle->flags = parent->handle->flags;
	}

	*ret_req = req;

	return LDB_SUCCESS;
}

int ldb_extended(struct ldb_context *ldb,
		 const char *oid,
		 void *data,
		 struct ldb_result **_res)
{
	struct ldb_request *req;
	int ret;
	struct ldb_result *res;

	*_res = NULL;

	res = talloc_zero(ldb, struct ldb_result);
	if (!res) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_extended_req(&req, ldb, ldb,
				     oid, data, NULL,
				     res, ldb_extended_default_callback,
				     NULL);
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
	}

	*_res = res;
	return ret;
}

/*
  note that ldb_search() will automatically replace a NULL 'base' value
  with the defaultNamingContext from the rootDSE if available.
*/
int ldb_search(struct ldb_context *ldb, TALLOC_CTX *mem_ctx,
		struct ldb_result **result, struct ldb_dn *base,
		enum ldb_scope scope, const char * const *attrs,
		const char *exp_fmt, ...)
{
	struct ldb_request *req;
	struct ldb_result *res;
	char *expression;
	va_list ap;
	int ret;

	expression = NULL;
	*result = NULL;
	req = NULL;

	res = talloc_zero(mem_ctx, struct ldb_result);
	if (!res) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (exp_fmt) {
		va_start(ap, exp_fmt);
		expression = talloc_vasprintf(mem_ctx, exp_fmt, ap);
		va_end(ap);

		if (!expression) {
			talloc_free(res);
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	ret = ldb_build_search_req(&req, ldb, mem_ctx,
					base?base:ldb_get_default_basedn(ldb),
	       				scope,
					expression,
					attrs,
					NULL,
					res,
					ldb_search_default_callback,
					NULL);
	ldb_req_set_location(req, "ldb_search");

	if (ret != LDB_SUCCESS) goto done;

	ret = ldb_request(ldb, req);

	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

done:
	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		res = NULL;
	}

	talloc_free(expression);
	talloc_free(req);

	*result = res;
	return ret;
}

/*
  add a record to the database. Will fail if a record with the given class
  and key already exists
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
					ldb_op_default_callback,
					NULL);
	ldb_req_set_location(req, "ldb_add");

	if (ret != LDB_SUCCESS) return ret;

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
					ldb_op_default_callback,
					NULL);
	ldb_req_set_location(req, "ldb_modify");

	if (ret != LDB_SUCCESS) return ret;

	/* do request and autostart a transaction */
	ret = ldb_autotransaction_request(ldb, req);

	talloc_free(req);
	return ret;
}


/*
  delete a record from the database
*/
int ldb_delete(struct ldb_context *ldb, struct ldb_dn *dn)
{
	struct ldb_request *req;
	int ret;

	ret = ldb_build_del_req(&req, ldb, ldb,
					dn,
					NULL,
					NULL,
					ldb_op_default_callback,
					NULL);
	ldb_req_set_location(req, "ldb_delete");

	if (ret != LDB_SUCCESS) return ret;

	/* do request and autostart a transaction */
	ret = ldb_autotransaction_request(ldb, req);

	talloc_free(req);
	return ret;
}

/*
  rename a record in the database
*/
int ldb_rename(struct ldb_context *ldb,
		struct ldb_dn *olddn, struct ldb_dn *newdn)
{
	struct ldb_request *req;
	int ret;

	ret = ldb_build_rename_req(&req, ldb, ldb,
					olddn,
					newdn,
					NULL,
					NULL,
					ldb_op_default_callback,
					NULL);
	ldb_req_set_location(req, "ldb_rename");

	if (ret != LDB_SUCCESS) return ret;

	/* do request and autostart a transaction */
	ret = ldb_autotransaction_request(ldb, req);

	talloc_free(req);
	return ret;
}


/*
  return the global sequence number
*/
int ldb_sequence_number(struct ldb_context *ldb,
			enum ldb_sequence_type type, uint64_t *seq_num)
{
	struct ldb_seqnum_request *seq;
	struct ldb_seqnum_result *seqr;
	struct ldb_result *res;
	TALLOC_CTX *tmp_ctx;
	int ret;

	*seq_num = 0;

	tmp_ctx = talloc_zero(ldb, struct ldb_request);
	if (tmp_ctx == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}
	seq = talloc_zero(tmp_ctx, struct ldb_seqnum_request);
	if (seq == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}
	seq->type = type;

	ret = ldb_extended(ldb, LDB_EXTENDED_SEQUENCE_NUMBER, seq, &res);
	if (ret != LDB_SUCCESS) {
		goto done;
	}
	talloc_steal(tmp_ctx, res);

	if (strcmp(LDB_EXTENDED_SEQUENCE_NUMBER, res->extended->oid) != 0) {
		ldb_set_errstring(ldb, "Invalid OID in reply");
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}
	seqr = talloc_get_type(res->extended->data,
				struct ldb_seqnum_result);
	*seq_num = seqr->seq_num;

done:
	talloc_free(tmp_ctx);
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

int ldb_global_init(void)
{
	/* Provided for compatibility with some older versions of ldb */
	return 0;
}

/* return the ldb flags */
unsigned int ldb_get_flags(struct ldb_context *ldb)
{
	return ldb->flags;
}

/* set the ldb flags */
void ldb_set_flags(struct ldb_context *ldb, unsigned flags)
{
	ldb->flags = flags;
}


/*
  set the location in a ldb request. Used for debugging
 */
void ldb_req_set_location(struct ldb_request *req, const char *location)
{
	if (req && req->handle) {
		req->handle->location = location;
	}
}

/*
  return the location set with dsdb_req_set_location
 */
const char *ldb_req_location(struct ldb_request *req)
{
	return req->handle->location;
}

/**
  mark a request as untrusted. This tells the rootdse module to remove
  unregistered controls
 */
void ldb_req_mark_untrusted(struct ldb_request *req)
{
	req->handle->flags |= LDB_HANDLE_FLAG_UNTRUSTED;
}

/**
  mark a request as trusted.
 */
void ldb_req_mark_trusted(struct ldb_request *req)
{
	req->handle->flags &= ~LDB_HANDLE_FLAG_UNTRUSTED;
}

/**
   return true is a request is untrusted
 */
bool ldb_req_is_untrusted(struct ldb_request *req)
{
	return (req->handle->flags & LDB_HANDLE_FLAG_UNTRUSTED) != 0;
}
