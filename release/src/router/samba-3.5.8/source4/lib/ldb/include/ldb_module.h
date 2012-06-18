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

#include "ldb.h"

struct ldb_context;
struct ldb_module;

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

#define ldb_oom(ldb) ldb_debug_set(ldb, LDB_DEBUG_FATAL, "ldb out of memory at %s:%d\n", __FILE__, __LINE__)

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

/* The following definitions come from lib/ldb/common/ldb_controls.c  */
struct ldb_control *get_control_from_list(struct ldb_control **controls, const char *oid);
int save_controls(struct ldb_control *exclude, struct ldb_request *req, struct ldb_control ***saver);
int check_critical_controls(struct ldb_control **controls);

/* The following definitions come from lib/ldb/common/ldb_ldif.c  */
int ldb_should_b64_encode(struct ldb_context *ldb, const struct ldb_val *val);

/* The following definitions come from lib/ldb/common/ldb_match.c  */
int ldb_match_msg(struct ldb_context *ldb,
		  const struct ldb_message *msg,
		  const struct ldb_parse_tree *tree,
		  struct ldb_dn *base,
		  enum ldb_scope scope);

/* The following definitions come from lib/ldb/common/ldb_modules.c  */

struct ldb_module *ldb_module_new(TALLOC_CTX *memctx,
				  struct ldb_context *ldb,
				  const char *module_name,
				  const struct ldb_module_ops *ops);

const char * ldb_module_get_name(struct ldb_module *module);
struct ldb_context *ldb_module_get_ctx(struct ldb_module *module);
void *ldb_module_get_private(struct ldb_module *module);
void ldb_module_set_private(struct ldb_module *module, void *private_data);

int ldb_next_request(struct ldb_module *module, struct ldb_request *request);
int ldb_next_start_trans(struct ldb_module *module);
int ldb_next_end_trans(struct ldb_module *module);
int ldb_next_del_trans(struct ldb_module *module);
int ldb_next_prepare_commit(struct ldb_module *module);
int ldb_next_init(struct ldb_module *module);

void ldb_set_errstring(struct ldb_context *ldb, const char *err_string);
void ldb_asprintf_errstring(struct ldb_context *ldb, const char *format, ...) PRINTF_ATTRIBUTE(2,3);
void ldb_reset_err_string(struct ldb_context *ldb);

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

int ldb_register_backend(const char *url_prefix, ldb_connect_fn);

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

#endif
