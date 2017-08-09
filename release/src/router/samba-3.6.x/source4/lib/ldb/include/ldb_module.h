/*
   ldb database library

   Copyright (C) Simo Sorce         2008

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
 *  Component: ldb module header
 *
 *  Description: defines ldb modules structures and helpers
 *
 */

#ifndef _LDB_MODULE_H_
#define _LDB_MODULE_H_

#include <ldb.h>

struct ldb_context;
struct ldb_module;

/**
   internal flag bits on message elements. Must be within LDB_FLAG_INTERNAL_MASK
 */
#define LDB_FLAG_INTERNAL_DISABLE_VALIDATION 0x10

/* disable any single value checking on this attribute */
#define LDB_FLAG_INTERNAL_DISABLE_SINGLE_VALUE_CHECK 0x20

/* attribute has failed access check and must not be exposed */
#define LDB_FLAG_INTERNAL_INACCESSIBLE_ATTRIBUTE 0x40

/* force single value checking on this attribute */
#define LDB_FLAG_INTERNAL_FORCE_SINGLE_VALUE_CHECK 0x80


/*
   these function pointers define the operations that a ldb module can intercept
*/
struct ldb_module_ops {
	const char *name;
	int (*init_context) (struct ldb_module *);
	int (*search)(struct ldb_module *, struct ldb_request *); /* search */
	int (*add)(struct ldb_module *, struct ldb_request *); /* add */
	int (*modify)(struct ldb_module *, struct ldb_request *); /* modify */
	int (*del)(struct ldb_module *, struct ldb_request *); /* delete */
	int (*rename)(struct ldb_module *, struct ldb_request *); /* rename */
	int (*request)(struct ldb_module *, struct ldb_request *); /* match any other operation */
	int (*extended)(struct ldb_module *, struct ldb_request *); /* extended operations */
	int (*start_transaction)(struct ldb_module *);
	int (*prepare_commit)(struct ldb_module *);
	int (*end_transaction)(struct ldb_module *);
	int (*del_transaction)(struct ldb_module *);
	int (*sequence_number)(struct ldb_module *, struct ldb_request *);
	void *private_data;
};


/* The following definitions come from lib/ldb/common/ldb_debug.c  */
void ldb_debug(struct ldb_context *ldb, enum ldb_debug_level level, const char *fmt, ...) PRINTF_ATTRIBUTE(3, 4);
void ldb_debug_set(struct ldb_context *ldb, enum ldb_debug_level level, 
		   const char *fmt, ...) PRINTF_ATTRIBUTE(3, 4);
void ldb_debug_add(struct ldb_context *ldb, const char *fmt, ...) PRINTF_ATTRIBUTE(2, 3);
void ldb_debug_end(struct ldb_context *ldb, enum ldb_debug_level level);

#define ldb_error(ldb, ecode, reason) ldb_error_at(ldb, ecode, reason, __FILE__, __LINE__)

#define ldb_oom(ldb) ldb_error(ldb, LDB_ERR_OPERATIONS_ERROR, "ldb out of memory")
#define ldb_module_oom(module) ldb_oom(ldb_module_get_ctx(module))
#define ldb_operr(ldb) ldb_error(ldb, LDB_ERR_OPERATIONS_ERROR, "operations error")

/* The following definitions come from lib/ldb/common/ldb.c  */

void ldb_request_set_state(struct ldb_request *req, int state);
int ldb_request_get_status(struct ldb_request *req);

unsigned int ldb_get_create_perms(struct ldb_context *ldb);

const struct ldb_schema_syntax *ldb_standard_syntax_by_name(struct ldb_context *ldb,
							    const char *syntax);

/* The following definitions come from lib/ldb/common/ldb_attributes.c  */

int ldb_schema_attribute_add_with_syntax(struct ldb_context *ldb,
					 const char *name,
					 unsigned flags,
					 const struct ldb_schema_syntax *syntax);
int ldb_schema_attribute_add(struct ldb_context *ldb, 
			     const char *name,
			     unsigned flags,
			     const char *syntax);
void ldb_schema_attribute_remove(struct ldb_context *ldb, const char *name);

/* we allow external code to override the name -> schema_attribute function */
typedef const struct ldb_schema_attribute *(*ldb_attribute_handler_override_fn_t)(struct ldb_context *, void *, const char *);

void ldb_schema_attribute_set_override_handler(struct ldb_context *ldb,
					       ldb_attribute_handler_override_fn_t override,
					       void *private_data);

/* A useful function to build comparison functions with */
int ldb_any_comparison(struct ldb_context *ldb, void *mem_ctx, 
		       ldb_attr_handler_t canonicalise_fn, 
		       const struct ldb_val *v1,
		       const struct ldb_val *v2);

/* The following definitions come from lib/ldb/common/ldb_controls.c  */
int ldb_save_controls(struct ldb_control *exclude, struct ldb_request *req, struct ldb_control ***saver);
/* Returns a list of controls, except the one specified.  Included
 * controls become a child of returned list if they were children of
 * controls_in */
struct ldb_control **ldb_controls_except_specified(struct ldb_control **controls_in, 
					       TALLOC_CTX *mem_ctx, 
					       struct ldb_control *exclude);
int ldb_check_critical_controls(struct ldb_control **controls);

/* The following definitions come from lib/ldb/common/ldb_ldif.c  */
int ldb_should_b64_encode(struct ldb_context *ldb, const struct ldb_val *val);

/* The following definitions come from lib/ldb/common/ldb_match.c  */
int ldb_match_msg(struct ldb_context *ldb,
		  const struct ldb_message *msg,
		  const struct ldb_parse_tree *tree,
		  struct ldb_dn *base,
		  enum ldb_scope scope);

int ldb_match_msg_error(struct ldb_context *ldb,
			const struct ldb_message *msg,
			const struct ldb_parse_tree *tree,
			struct ldb_dn *base,
			enum ldb_scope scope,
			bool *matched);

int ldb_match_msg_objectclass(const struct ldb_message *msg,
			      const char *objectclass);

/* The following definitions come from lib/ldb/common/ldb_modules.c  */

struct ldb_module *ldb_module_new(TALLOC_CTX *memctx,
				  struct ldb_context *ldb,
				  const char *module_name,
				  const struct ldb_module_ops *ops);

const char * ldb_module_get_name(struct ldb_module *module);
struct ldb_context *ldb_module_get_ctx(struct ldb_module *module);
void *ldb_module_get_private(struct ldb_module *module);
void ldb_module_set_private(struct ldb_module *module, void *private_data);
const struct ldb_module_ops *ldb_module_get_ops(struct ldb_module *module);

int ldb_next_request(struct ldb_module *module, struct ldb_request *request);
int ldb_next_start_trans(struct ldb_module *module);
int ldb_next_end_trans(struct ldb_module *module);
int ldb_next_del_trans(struct ldb_module *module);
int ldb_next_prepare_commit(struct ldb_module *module);
int ldb_next_init(struct ldb_module *module);

void ldb_set_errstring(struct ldb_context *ldb, const char *err_string);
void ldb_asprintf_errstring(struct ldb_context *ldb, const char *format, ...) PRINTF_ATTRIBUTE(2,3);
void ldb_reset_err_string(struct ldb_context *ldb);
int ldb_error_at(struct ldb_context *ldb, int ecode, const char *reason, const char *file, int line);

const char *ldb_default_modules_dir(void);

int ldb_register_module(const struct ldb_module_ops *);

typedef int (*ldb_connect_fn)(struct ldb_context *ldb, const char *url,
			      unsigned int flags, const char *options[],
			      struct ldb_module **module);

struct ldb_backend_ops {
	const char *name;
	ldb_connect_fn connect_fn;
};

const char *ldb_default_modules_dir(void);

int ldb_register_backend(const char *url_prefix, ldb_connect_fn, bool);

struct ldb_handle *ldb_handle_new(TALLOC_CTX *mem_ctx, struct ldb_context *ldb);

int ldb_module_send_entry(struct ldb_request *req,
			  struct ldb_message *msg,
			  struct ldb_control **ctrls);

int ldb_module_send_referral(struct ldb_request *req,
					   char *ref);

int ldb_module_done(struct ldb_request *req,
		    struct ldb_control **ctrls,
		    struct ldb_extended *response,
		    int error);

int ldb_mod_register_control(struct ldb_module *module, const char *oid);

void ldb_set_default_dns(struct ldb_context *ldb);
/**
  Add a ldb_control to a ldb_reply

  \param ares the reply struct where to add the control
  \param oid the object identifier of the control as string
  \param critical whether the control should be critical or not
  \param data a talloc pointer to the control specific data

  \return result code (LDB_SUCCESS on success, or a failure code)
*/
int ldb_reply_add_control(struct ldb_reply *ares, const char *oid, bool critical, void *data);

/**
  mark a request as untrusted. This tells the rootdse module to remove
  unregistered controls
 */
void ldb_req_mark_untrusted(struct ldb_request *req);

/**
  mark a request as trusted.
 */
void ldb_req_mark_trusted(struct ldb_request *req);

/**
   return true is a request is untrusted
 */
bool ldb_req_is_untrusted(struct ldb_request *req);

/* load all modules from the given directory */
int ldb_modules_load(const char *modules_path, const char *version);

/* init functions prototype */
typedef int (*ldb_module_init_fn)(const char *);

/*
  general ldb hook function
 */
enum ldb_module_hook_type { LDB_MODULE_HOOK_CMDLINE_OPTIONS     = 1,
			    LDB_MODULE_HOOK_CMDLINE_PRECONNECT  = 2,
			    LDB_MODULE_HOOK_CMDLINE_POSTCONNECT = 3 };

typedef int (*ldb_hook_fn)(struct ldb_context *, enum ldb_module_hook_type );

/*
  register a ldb hook function
 */
int ldb_register_hook(ldb_hook_fn hook_fn);

/*
  call ldb hooks of a given type
 */
int ldb_modules_hook(struct ldb_context *ldb, enum ldb_module_hook_type t);

#define LDB_MODULE_CHECK_VERSION(version) do { \
 if (strcmp(version, LDB_VERSION) != 0) { \
	fprintf(stderr, "ldb: module version mismatch in %s : ldb_version=%s module_version=%s\n", \
			__FILE__, version, LDB_VERSION); \
        return LDB_ERR_UNAVAILABLE; \
 }} while (0)


/*
  return a string representation of the calling chain for the given
  ldb request
 */
char *ldb_module_call_chain(struct ldb_request *req, TALLOC_CTX *mem_ctx);

/*
  return the next module in the chain
 */
struct ldb_module *ldb_module_next(struct ldb_module *module);

/*
  set the next module in the module chain
 */
void ldb_module_set_next(struct ldb_module *module, struct ldb_module *next);

/*
  load a list of modules
 */
int ldb_module_load_list(struct ldb_context *ldb, const char **module_list,
			 struct ldb_module *backend, struct ldb_module **out);

/*
  get the popt_options pointer in the ldb structure. This allows a ldb
  module to change the command line parsing
 */
struct poptOption **ldb_module_popt_options(struct ldb_context *ldb);

/* modules are called in inverse order on the stack.
   Lets place them as an admin would think the right order is.
   Modules order is important */
const char **ldb_modules_list_from_string(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, const char *string);

/*
  return the current ldb flags LDB_FLG_*
 */
uint32_t ldb_module_flags(struct ldb_context *ldb);

int ldb_module_connect_backend(struct ldb_context *ldb,
			       const char *url,
			       const char *options[],
			       struct ldb_module **backend_module);

/*
  initialise a chain of modules
 */
int ldb_module_init_chain(struct ldb_context *ldb, struct ldb_module *module);

/*
 * prototype for the init function defined by dynamically loaded modules
 */
int ldb_init_module(const char *version);


#endif
