#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"

/* A handy macro to report Out of Memory conditions */
#define map_oom(module) ldb_set_errstring(ldb_module_get_ctx(module), talloc_asprintf(module, "Out of Memory"));

/* The type of search callback functions */
typedef int (*ldb_map_callback_t)(struct ldb_request *, struct ldb_reply *);

/* The special DN from which the local and remote base DNs are fetched */
#define MAP_DN_NAME	"@MAP"
#define MAP_DN_FROM	"@FROM"
#define MAP_DN_TO	"@TO"

/* Private data structures
 * ======================= */

struct map_reply {
	struct map_reply *next;
	struct ldb_reply *remote;
	struct ldb_reply *local;
};

/* Context data for mapped requests */
struct map_context {

	struct ldb_module *module;
	struct ldb_request *req;

	struct ldb_dn *local_dn;
	const struct ldb_parse_tree *local_tree;
	const char * const *local_attrs;
	const char * const *remote_attrs;
	const char * const *all_attrs;

	struct ldb_message *local_msg;
	struct ldb_request *remote_req;

	struct map_reply *r_list;
	struct map_reply *r_current;

	/* The response continaing any controls the remote server gave */
	struct ldb_reply *remote_done_ares;
};

/* Common operations
 * ================= */

/* The following definitions come from lib/ldb/modules/ldb_map.c */
const struct ldb_map_context *map_get_context(struct ldb_module *module);
struct map_context *map_init_context(struct ldb_module *module,
					struct ldb_request *req);

int ldb_next_remote_request(struct ldb_module *module, struct ldb_request *request);

bool map_check_local_db(struct ldb_module *module);
bool map_attr_check_remote(const struct ldb_map_context *data, const char *attr);
bool ldb_dn_check_local(struct ldb_module *module, struct ldb_dn *dn);

const struct ldb_map_attribute *map_attr_find_local(const struct ldb_map_context *data, const char *name);
const struct ldb_map_attribute *map_attr_find_remote(const struct ldb_map_context *data, const char *name);

const char *map_attr_map_local(void *mem_ctx, const struct ldb_map_attribute *map, const char *attr);
const char *map_attr_map_remote(void *mem_ctx, const struct ldb_map_attribute *map, const char *attr);
int map_attrs_merge(struct ldb_module *module, void *mem_ctx, const char ***attrs, const char * const *more_attrs);

struct ldb_val ldb_val_map_local(struct ldb_module *module, void *mem_ctx, const struct ldb_map_attribute *map, const struct ldb_val *val);
struct ldb_val ldb_val_map_remote(struct ldb_module *module, void *mem_ctx, const struct ldb_map_attribute *map, const struct ldb_val *val);

struct ldb_dn *ldb_dn_map_local(struct ldb_module *module, void *mem_ctx, struct ldb_dn *dn);
struct ldb_dn *ldb_dn_map_remote(struct ldb_module *module, void *mem_ctx, struct ldb_dn *dn);
struct ldb_dn *ldb_dn_map_rebase_remote(struct ldb_module *module, void *mem_ctx, struct ldb_dn *dn);

struct ldb_request *map_search_base_req(struct map_context *ac,
					struct ldb_dn *dn,
					const char * const *attrs,
					const struct ldb_parse_tree *tree,
					void *context,
					ldb_map_callback_t callback);
struct ldb_request *map_build_fixup_req(struct map_context *ac,
					struct ldb_dn *olddn,
					struct ldb_dn *newdn,
					void *context,
					ldb_map_callback_t callback);
int map_subtree_collect_remote_simple(struct ldb_module *module, void *mem_ctx,
					struct ldb_parse_tree **new,
					const struct ldb_parse_tree *tree,
					const struct ldb_map_attribute *map);
int map_return_fatal_error(struct ldb_request *req,
			   struct ldb_reply *ares);
int map_return_normal_error(struct ldb_request *req,
			    struct ldb_reply *ares,
			    int error);

int map_return_entry(struct map_context *ac, struct ldb_reply *ares);
