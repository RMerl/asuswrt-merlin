#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "tdb.h"
#include "ldb_module.h"

/* this private structure is used by the ltdb backend in the
   ldb_context */
struct ltdb_private {
	TDB_CONTEXT *tdb;
	unsigned int connect_flags;
	
	unsigned long long sequence_number;

	/* the low level tdb seqnum - used to avoid loading BASEINFO when
	   possible */
	int tdb_seqnum;

	struct ltdb_cache {
		struct ldb_message *indexlist;
		struct ldb_message *attributes;
		bool one_level_indexes;
		bool attribute_indexes;

		struct {
			char *name;
			int flags;
		} last_attribute;
	} *cache;

	int in_transaction;

	bool check_base;
	struct ltdb_idxptr *idxptr;
	bool prepared_commit;
	int read_lock_count;

	bool warn_unindexed;
};

/*
  the async local context
  holds also internal search state during a full db search
*/
struct ltdb_req_spy {
	struct ltdb_context *ctx;
};

struct ltdb_context {
	struct ldb_module *module;
	struct ldb_request *req;

	bool request_terminated;
	struct ltdb_req_spy *spy;

	/* search stuff */
	const struct ldb_parse_tree *tree;
	struct ldb_dn *base;
	enum ldb_scope scope;
	const char * const *attrs;
	struct tevent_timer *timeout_event;
};

/* special record types */
#define LTDB_INDEX      "@INDEX"
#define LTDB_INDEXLIST  "@INDEXLIST"
#define LTDB_IDX        "@IDX"
#define LTDB_IDXVERSION "@IDXVERSION"
#define LTDB_IDXATTR    "@IDXATTR"
#define LTDB_IDXONE     "@IDXONE"
#define LTDB_BASEINFO   "@BASEINFO"
#define LTDB_OPTIONS    "@OPTIONS"
#define LTDB_ATTRIBUTES "@ATTRIBUTES"

/* special attribute types */
#define LTDB_SEQUENCE_NUMBER "sequenceNumber"
#define LTDB_CHECK_BASE "checkBaseOnSearch"
#define LTDB_MOD_TIMESTAMP "whenChanged"
#define LTDB_OBJECTCLASS "objectClass"

/* The following definitions come from lib/ldb/ldb_tdb/ldb_cache.c  */

int ltdb_cache_reload(struct ldb_module *module);
int ltdb_cache_load(struct ldb_module *module);
int ltdb_increase_sequence_number(struct ldb_module *module);
int ltdb_check_at_attributes_values(const struct ldb_val *value);

/* The following definitions come from lib/ldb/ldb_tdb/ldb_index.c  */

struct ldb_parse_tree;

int ltdb_search_indexed(struct ltdb_context *ctx, uint32_t *);
int ltdb_index_add_new(struct ldb_module *module, const struct ldb_message *msg);
int ltdb_index_delete(struct ldb_module *module, const struct ldb_message *msg);
int ltdb_index_del_element(struct ldb_module *module, struct ldb_dn *dn,
			   struct ldb_message_element *el);
int ltdb_index_add_element(struct ldb_module *module, struct ldb_dn *dn, 
			   struct ldb_message_element *el);
int ltdb_index_del_value(struct ldb_module *module, struct ldb_dn *dn,
			 struct ldb_message_element *el, unsigned int v_idx);
int ltdb_reindex(struct ldb_module *module);
int ltdb_index_transaction_start(struct ldb_module *module);
int ltdb_index_transaction_commit(struct ldb_module *module);
int ltdb_index_transaction_cancel(struct ldb_module *module);

/* The following definitions come from lib/ldb/ldb_tdb/ldb_pack.c  */

int ltdb_pack_data(struct ldb_module *module,
		   const struct ldb_message *message,
		   struct TDB_DATA *data);
void ltdb_unpack_data_free(struct ldb_module *module,
			   struct ldb_message *message);
int ltdb_unpack_data(struct ldb_module *module,
		     const struct TDB_DATA *data,
		     struct ldb_message *message);

/* The following definitions come from lib/ldb/ldb_tdb/ldb_search.c  */

int ltdb_has_wildcard(struct ldb_module *module, const char *attr_name, 
		      const struct ldb_val *val);
void ltdb_search_dn1_free(struct ldb_module *module, struct ldb_message *msg);
int ltdb_search_dn1(struct ldb_module *module, struct ldb_dn *dn, struct ldb_message *msg);
int ltdb_add_attr_results(struct ldb_module *module,
 			  TALLOC_CTX *mem_ctx, 
			  struct ldb_message *msg,
			  const char * const attrs[], 
			  unsigned int *count, 
			  struct ldb_message ***res);
int ltdb_filter_attrs(struct ldb_message *msg, const char * const *attrs);
int ltdb_search(struct ltdb_context *ctx);

/* The following definitions come from lib/ldb/ldb_tdb/ldb_tdb.c  */
int ltdb_lock_read(struct ldb_module *module);
int ltdb_unlock_read(struct ldb_module *module);
struct TDB_DATA ltdb_key(struct ldb_module *module, struct ldb_dn *dn);
int ltdb_store(struct ldb_module *module, const struct ldb_message *msg, int flgs);
int ltdb_modify_internal(struct ldb_module *module, const struct ldb_message *msg, struct ldb_request *req);
int ltdb_delete_noindex(struct ldb_module *module, struct ldb_dn *dn);
int ltdb_err_map(enum TDB_ERROR tdb_code);

struct tdb_context *ltdb_wrap_open(TALLOC_CTX *mem_ctx,
				   const char *path, int hash_size, int tdb_flags,
				   int open_flags, mode_t mode,
				   struct ldb_context *ldb);
