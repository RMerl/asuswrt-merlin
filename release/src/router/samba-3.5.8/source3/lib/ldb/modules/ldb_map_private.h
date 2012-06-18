
/* A handy macro to report Out of Memory conditions */
#define map_oom(module) ldb_set_errstring(module->ldb, talloc_asprintf(module, "Out of Memory"));

/* The type of search callback functions */
typedef int (*ldb_search_callback)(struct ldb_context *, void *, struct ldb_reply *);

/* The special DN from which the local and remote base DNs are fetched */
#define MAP_DN_NAME	"@MAP"
#define MAP_DN_FROM	"@FROM"
#define MAP_DN_TO	"@TO"

/* Private data structures
 * ======================= */

/* Context data for mapped requests */
struct map_context {
	enum map_step {
		MAP_SEARCH_REMOTE,
		MAP_ADD_REMOTE,
		MAP_ADD_LOCAL,
		MAP_SEARCH_SELF_MODIFY,
		MAP_MODIFY_REMOTE,
		MAP_MODIFY_LOCAL,
		MAP_SEARCH_SELF_DELETE,
		MAP_DELETE_REMOTE,
		MAP_DELETE_LOCAL,
		MAP_SEARCH_SELF_RENAME,
		MAP_RENAME_REMOTE,
		MAP_RENAME_FIXUP,
		MAP_RENAME_LOCAL
	} step;

	struct ldb_module *module;

	const struct ldb_dn *local_dn;
	const struct ldb_parse_tree *local_tree;
	const char * const *local_attrs;
	const char * const *remote_attrs;
	const char * const *all_attrs;

	struct ldb_request *orig_req;
	struct ldb_request *local_req;
	struct ldb_request *remote_req;
	struct ldb_request *down_req;
	struct ldb_request *search_req;

	/* for search, we may have a lot of contexts */
	int num_searches;
	struct ldb_request **search_reqs;
};

/* Context data for mapped search requests */
struct map_search_context {
	struct map_context *ac;
	struct ldb_reply *local_res;
	struct ldb_reply *remote_res;
};


/* Common operations
 * ================= */

/* The following definitions come from lib/ldb/modules/ldb_map.c */
const struct ldb_map_context *map_get_context(struct ldb_module *module);
struct map_search_context *map_init_search_context(struct map_context *ac, struct ldb_reply *ares);
struct ldb_handle *map_init_handle(struct ldb_request *req, struct ldb_module *module);

int ldb_next_remote_request(struct ldb_module *module, struct ldb_request *request);

BOOL map_check_local_db(struct ldb_module *module);
BOOL map_attr_check_remote(const struct ldb_map_context *data, const char *attr);
BOOL ldb_dn_check_local(struct ldb_module *module, const struct ldb_dn *dn);

const struct ldb_map_attribute *map_attr_find_local(const struct ldb_map_context *data, const char *name);
const struct ldb_map_attribute *map_attr_find_remote(const struct ldb_map_context *data, const char *name);

const char *map_attr_map_local(void *mem_ctx, const struct ldb_map_attribute *map, const char *attr);
const char *map_attr_map_remote(void *mem_ctx, const struct ldb_map_attribute *map, const char *attr);
int map_attrs_merge(struct ldb_module *module, void *mem_ctx, const char ***attrs, const char * const *more_attrs);

struct ldb_val ldb_val_map_local(struct ldb_module *module, void *mem_ctx, const struct ldb_map_attribute *map, const struct ldb_val *val);
struct ldb_val ldb_val_map_remote(struct ldb_module *module, void *mem_ctx, const struct ldb_map_attribute *map, const struct ldb_val *val);

struct ldb_dn *ldb_dn_map_local(struct ldb_module *module, void *mem_ctx, const struct ldb_dn *dn);
struct ldb_dn *ldb_dn_map_remote(struct ldb_module *module, void *mem_ctx, const struct ldb_dn *dn);
struct ldb_dn *ldb_dn_map_rebase_remote(struct ldb_module *module, void *mem_ctx, const struct ldb_dn *dn);

struct ldb_request *map_search_base_req(struct map_context *ac, const struct ldb_dn *dn, const char * const *attrs, const struct ldb_parse_tree *tree, void *context, ldb_search_callback callback);
struct ldb_request *map_search_self_req(struct map_context *ac, const struct ldb_dn *dn);
struct ldb_request *map_build_fixup_req(struct map_context *ac, const struct ldb_dn *olddn, const struct ldb_dn *newdn);

int map_subtree_collect_remote_simple(struct ldb_module *module, void *mem_ctx, struct ldb_parse_tree **new, const struct ldb_parse_tree *tree, const struct ldb_map_attribute *map);

/* LDB Requests
 * ============ */

/* The following definitions come from lib/ldb/modules/ldb_map_inbound.c */
int map_add_do_remote(struct ldb_handle *handle);
int map_add_do_local(struct ldb_handle *handle);
int map_add(struct ldb_module *module, struct ldb_request *req);

int map_modify_do_remote(struct ldb_handle *handle);
int map_modify_do_local(struct ldb_handle *handle);
int map_modify(struct ldb_module *module, struct ldb_request *req);

int map_delete_do_remote(struct ldb_handle *handle);
int map_delete_do_local(struct ldb_handle *handle);
int map_delete(struct ldb_module *module, struct ldb_request *req);

int map_rename_do_remote(struct ldb_handle *handle);
int map_rename_do_fixup(struct ldb_handle *handle);
int map_rename_do_local(struct ldb_handle *handle);
int map_rename(struct ldb_module *module, struct ldb_request *req);

/* The following definitions come from lib/ldb/modules/ldb_map_outbound.c */
int map_search(struct ldb_module *module, struct ldb_request *req);
