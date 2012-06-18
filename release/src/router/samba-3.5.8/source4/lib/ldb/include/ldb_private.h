/* 
   ldb database library

   Copyright (C) Andrew Tridgell    2004
   Copyright (C) Stefan Metzmacher  2004
   Copyright (C) Simo Sorce         2004-2005

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
 *  Component: ldb private header
 *
 *  Description: defines internal ldb structures used by the subsystem and modules
 *
 *  Author: Andrew Tridgell
 *  Author: Stefan Metzmacher
 */

#ifndef _LDB_PRIVATE_H_
#define _LDB_PRIVATE_H_ 1

#include "ldb_includes.h"
#include "ldb.h"
#include "ldb_module.h"

struct ldb_context;

struct ldb_module_ops;

struct ldb_backend_ops;

#define LDB_HANDLE_FLAG_DONE_CALLED 1

struct ldb_handle {
	int status;
	enum ldb_state state;
	struct ldb_context *ldb;
	unsigned flags;
	unsigned nesting;
};

/* basic module structure */
struct ldb_module {
	struct ldb_module *prev, *next;
	struct ldb_context *ldb;
	void *private_data;
	const struct ldb_module_ops *ops;
};

/*
  schema related information needed for matching rules
*/
struct ldb_schema {
	void *attribute_handler_override_private;
	ldb_attribute_handler_override_fn_t attribute_handler_override;
	
	/* attribute handling table */
	unsigned num_attributes;
	struct ldb_schema_attribute *attributes;

	unsigned num_dn_extended_syntax;
	struct ldb_dn_extended_syntax *dn_extended_syntax;
};

/*
  every ldb connection is started by establishing a ldb_context
*/
struct ldb_context {
	/* the operations provided by the backend */
	struct ldb_module *modules;

	/* debugging operations */
	struct ldb_debug_ops debug_ops;

	/* custom utf8 functions */
	struct ldb_utf8_fns utf8_fns;

	/* backend specific opaque parameters */
	struct ldb_opaque {
		struct ldb_opaque *next;
		const char *name;
		void *value;
	} *opaque;

	struct ldb_schema schema;

	char *err_string;

	int transaction_active;

	int default_timeout;

	unsigned int flags;

	unsigned int create_perms;

	char *modules_dir;

	struct tevent_context *ev_ctx;

	bool prepare_commit_done;

	char *partial_debug;
};

/* The following definitions come from lib/ldb/common/ldb.c  */

int ldb_connect_backend(struct ldb_context *ldb, const char *url, const char *options[],
			struct ldb_module **backend_module);


extern const struct ldb_module_ops ldb_objectclass_module_ops;
extern const struct ldb_module_ops ldb_paged_results_module_ops;
extern const struct ldb_module_ops ldb_rdn_name_module_ops;
extern const struct ldb_module_ops ldb_schema_module_ops;
extern const struct ldb_module_ops ldb_asq_module_ops;
extern const struct ldb_module_ops ldb_server_sort_module_ops;
extern const struct ldb_module_ops ldb_ldap_module_ops;
extern const struct ldb_module_ops ldb_ildap_module_ops;
extern const struct ldb_module_ops ldb_paged_searches_module_ops;
extern const struct ldb_module_ops ldb_tdb_module_ops;
extern const struct ldb_module_ops ldb_skel_module_ops;
extern const struct ldb_module_ops ldb_subtree_rename_module_ops;
extern const struct ldb_module_ops ldb_subtree_delete_module_ops;
extern const struct ldb_module_ops ldb_sqlite3_module_ops;
extern const struct ldb_module_ops ldb_wins_ldb_module_ops;
extern const struct ldb_module_ops ldb_ranged_results_module_ops;

extern const struct ldb_backend_ops ldb_tdb_backend_ops;
extern const struct ldb_backend_ops ldb_sqlite3_backend_ops;
extern const struct ldb_backend_ops ldb_ldap_backend_ops;
extern const struct ldb_backend_ops ldb_ldapi_backend_ops;
extern const struct ldb_backend_ops ldb_ldaps_backend_ops;

int ldb_setup_wellknown_attributes(struct ldb_context *ldb);

const char **ldb_subclass_list(struct ldb_context *ldb, const char *classname);
void ldb_subclass_remove(struct ldb_context *ldb, const char *classname);
int ldb_subclass_add(struct ldb_context *ldb, const char *classname, const char *subclass);

/* The following definitions come from lib/ldb/common/ldb_utf8.c */
char *ldb_casefold_default(void *context, void *mem_ctx, const char *s, size_t n);

void ldb_dump_results(struct ldb_context *ldb, struct ldb_result *result, FILE *f);


/* The following definitions come from lib/ldb/common/ldb_modules.c  */

const char **ldb_modules_list_from_string(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, const char *string);
int ldb_load_modules_list(struct ldb_context *ldb, const char **module_list, struct ldb_module *backend, struct ldb_module **out);
int ldb_load_modules(struct ldb_context *ldb, const char *options[]);
int ldb_init_module_chain(struct ldb_context *ldb, struct ldb_module *module);

struct ldb_val ldb_binary_decode(void *mem_ctx, const char *str);

#endif
