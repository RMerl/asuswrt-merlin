#ifndef __LIB_UTIL_UTIL_LDB_H__
#define __LIB_UTIL_UTIL_LDB_H__

struct ldb_dn;

/* The following definitions come from lib/util/util_ldb.c  */

int gendb_search_v(struct ldb_context *ldb,
		   TALLOC_CTX *mem_ctx,
		   struct ldb_dn *basedn,
		   struct ldb_message ***msgs,
		   const char * const *attrs,
		   const char *format,
		   va_list ap)  PRINTF_ATTRIBUTE(6,0);
int gendb_search(struct ldb_context *ldb,
		 TALLOC_CTX *mem_ctx,
		 struct ldb_dn *basedn,
		 struct ldb_message ***res,
		 const char * const *attrs,
		 const char *format, ...) PRINTF_ATTRIBUTE(6,7);
int gendb_search_dn(struct ldb_context *ldb,
		 TALLOC_CTX *mem_ctx,
		 struct ldb_dn *dn,
		 struct ldb_message ***res,
		 const char * const *attrs);
int gendb_add_ldif(struct ldb_context *ldb, const char *ldif_string);
char *wrap_casefold(void *context, void *mem_ctx, const char *s, size_t n);

int gendb_search_single_extended_dn(struct ldb_context *ldb,
				    TALLOC_CTX *mem_ctx,
				    struct ldb_dn *basedn,
				    enum ldb_scope scope,
				    struct ldb_message **msg,
				    const char * const *attrs,
				    const char *format, ...)  PRINTF_ATTRIBUTE(7,8);

#endif /* __LIB_UTIL_UTIL_LDB_H__ */
