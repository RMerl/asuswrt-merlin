/*
   ldb database library

   Copyright (C) Derrell Lipman  2005
   Copyright (C) Simo Sorce 2005-2009

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
 *  Component: ldb sqlite3 backend
 *
 *  Description: core files for SQLITE3 backend
 *
 *  Author: Derrell Lipman (based on Andrew Tridgell's LDAP backend)
 */

#include "ldb_module.h"

#include <sqlite3.h>

struct lsqlite3_private {
	int trans_count;
	char **options;
        sqlite3 *sqlite;
};

struct lsql_context {
	struct ldb_module *module;
	struct ldb_request *req;

	/* search stuff */
	long long current_eid;
	const char * const * attrs;
	struct ldb_reply *ares;

	bool callback_failed;
	struct tevent_timer *timeout_event;
};

/*
 * Macros used throughout
 */

#ifndef FALSE
# define FALSE  (0)
# define TRUE   (! FALSE)
#endif

#define RESULT_ATTR_TABLE       "temp_result_attrs"


/* for testing, define to nothing, (create non-temporary table) */
#define TEMPTAB                 "TEMPORARY"

/*
 * Static variables
 */
sqlite3_stmt *  stmtGetEID = NULL;

static char *lsqlite3_tprintf(TALLOC_CTX *mem_ctx, const char *fmt, ...)
{
	char *str, *ret;
	va_list ap;

	va_start(ap, fmt);
        str = sqlite3_vmprintf(fmt, ap);
	va_end(ap);

	if (str == NULL) return NULL;

	ret = talloc_strdup(mem_ctx, str);
	if (ret == NULL) {
		sqlite3_free(str);
		return NULL;
	}

	sqlite3_free(str);
	return ret;
}

static char base160tab[161] = {
        48 ,49 ,50 ,51 ,52 ,53 ,54 ,55 ,56 ,57 , /* 0-9 */
        58 ,59 ,65 ,66 ,67 ,68 ,69 ,70 ,71 ,72 , /* : ; A-H */
        73 ,74 ,75 ,76 ,77 ,78 ,79 ,80 ,81 ,82 , /* I-R */
        83 ,84 ,85 ,86 ,87 ,88 ,89 ,90 ,97 ,98 , /* S-Z , a-b */
        99 ,100,101,102,103,104,105,106,107,108, /* c-l */
        109,110,111,112,113,114,115,116,117,118, /* m-v */
        119,120,121,122,160,161,162,163,164,165, /* w-z, latin1 */
        166,167,168,169,170,171,172,173,174,175, /* latin1 */
        176,177,178,179,180,181,182,183,184,185, /* latin1 */
        186,187,188,189,190,191,192,193,194,195, /* latin1 */
        196,197,198,199,200,201,202,203,204,205, /* latin1 */
        206,207,208,209,210,211,212,213,214,215, /* latin1 */
        216,217,218,219,220,221,222,223,224,225, /* latin1 */
        226,227,228,229,230,231,232,233,234,235, /* latin1 */
        236,237,238,239,240,241,242,243,244,245, /* latin1 */
        246,247,248,249,250,251,252,253,254,255, /* latin1 */
        '\0'
};


/*
 * base160()
 *
 * Convert an unsigned long integer into a base160 representation of the
 * number.
 *
 * Parameters:
 *   val --
 *     value to be converted
 *
 *   result --
 *     character array, 5 bytes long, into which the base160 representation
 *     will be placed.  The result will be a four-digit representation of the
 *     number (with leading zeros prepended as necessary), and null
 *     terminated.
 *
 * Returns:
 *   Nothing
 */
static void
base160_sql(sqlite3_context * hContext,
            int argc,
            sqlite3_value ** argv)
{
    int             i;
    long long       val;
    char            result[5];

    val = sqlite3_value_int64(argv[0]);

    for (i = 3; i >= 0; i--) {

        result[i] = base160tab[val % 160];
        val /= 160;
    }

    result[4] = '\0';

    sqlite3_result_text(hContext, result, -1, SQLITE_TRANSIENT);
}


/*
 * base160next_sql()
 *
 * This function enhances sqlite by adding a "base160_next()" function which is
 * accessible via queries.
 *
 * Retrieve the next-greater number in the base160 sequence for the terminal
 * tree node (the last four digits).  Only one tree level (four digits) is
 * operated on.
 *
 * Input:
 *   A character string: either an empty string (in which case no operation is
 *   performed), or a string of base160 digits with a length of a multiple of
 *   four digits.
 *
 * Output:
 *   Upon return, the trailing four digits (one tree level) will have been
 *   incremented by 1.
 */
static void
base160next_sql(sqlite3_context * hContext,
                int argc,
                sqlite3_value ** argv)
{
        int                         i;
        int                         len;
        char *             pTab;
        char *             pBase160 = strdup((const char *)sqlite3_value_text(argv[0]));
        char *             pStart = pBase160;

        /*
         * We need a minimum of four digits, and we will always get a multiple
         * of four digits.
         */
        if (pBase160 != NULL &&
            (len = strlen(pBase160)) >= 4 &&
            len % 4 == 0) {

                if (pBase160 == NULL) {

                        sqlite3_result_null(hContext);
                        return;
                }

                pBase160 += strlen(pBase160) - 1;

                /* We only carry through four digits: one level in the tree */
                for (i = 0; i < 4; i++) {

                        /* What base160 value does this digit have? */
                        pTab = strchr(base160tab, *pBase160);

                        /* Is there a carry? */
                        if (pTab < base160tab + sizeof(base160tab) - 1) {

                                /*
                                 * Nope.  Just increment this value and we're
                                 * done.
                                 */
                                *pBase160 = *++pTab;
                                break;
                        } else {

                                /*
                                 * There's a carry.  This value gets
                                 * base160tab[0], we decrement the buffer
                                 * pointer to get the next higher-order digit,
                                 * and continue in the loop.
                                 */
                                *pBase160-- = base160tab[0];
                        }
                }

                sqlite3_result_text(hContext,
                                    pStart,
                                    strlen(pStart),
                                    free);
        } else {
                sqlite3_result_value(hContext, argv[0]);
                if (pBase160 != NULL) {
                        free(pBase160);
                }
        }
}

static char *parsetree_to_sql(struct ldb_module *module,
			      void *mem_ctx,
			      const struct ldb_parse_tree *t)
{
	struct ldb_context *ldb;
	const struct ldb_schema_attribute *a;
	struct ldb_val value, subval;
	char *wild_card_string;
	char *child, *tmp;
	char *ret = NULL;
	char *attr;
	unsigned int i;

	ldb = ldb_module_get_ctx(module);

	switch(t->operation) {
	case LDB_OP_AND:

		tmp = parsetree_to_sql(module, mem_ctx, t->u.list.elements[0]);
		if (tmp == NULL) return NULL;

		for (i = 1; i < t->u.list.num_elements; i++) {

			child = parsetree_to_sql(module, mem_ctx, t->u.list.elements[i]);
			if (child == NULL) return NULL;

			tmp = talloc_asprintf_append(tmp, " INTERSECT %s ", child);
			if (tmp == NULL) return NULL;
		}

		ret = talloc_asprintf(mem_ctx, "SELECT * FROM ( %s )\n", tmp);

		return ret;

	case LDB_OP_OR:

		tmp = parsetree_to_sql(module, mem_ctx, t->u.list.elements[0]);
		if (tmp == NULL) return NULL;

		for (i = 1; i < t->u.list.num_elements; i++) {

			child = parsetree_to_sql(module, mem_ctx, t->u.list.elements[i]);
			if (child == NULL) return NULL;

			tmp = talloc_asprintf_append(tmp, " UNION %s ", child);
			if (tmp == NULL) return NULL;
		}

		return talloc_asprintf(mem_ctx, "SELECT * FROM ( %s ) ", tmp);

	case LDB_OP_NOT:

		child = parsetree_to_sql(module, mem_ctx, t->u.isnot.child);
		if (child == NULL) return NULL;

		return talloc_asprintf(mem_ctx,
					"SELECT eid FROM ldb_entry "
					"WHERE eid NOT IN ( %s ) ", child);

	case LDB_OP_EQUALITY:
		/*
		 * For simple searches, we want to retrieve the list of EIDs that
		 * match the criteria.
		*/
		attr = ldb_attr_casefold(mem_ctx, t->u.equality.attr);
		if (attr == NULL) return NULL;
		a = ldb_schema_attribute_by_name(ldb, attr);

		/* Get a canonicalised copy of the data */
		a->syntax->canonicalise_fn(ldb, mem_ctx, &(t->u.equality.value), &value);
		if (value.data == NULL) {
			return NULL;
		}

		if (strcasecmp(t->u.equality.attr, "dn") == 0) {
			/* DN query is a special ldb case */
		 	const char *cdn = ldb_dn_get_casefold(
						ldb_dn_new(mem_ctx, ldb,
							      (const char *)value.data));

			return lsqlite3_tprintf(mem_ctx,
						"SELECT eid FROM ldb_entry "
						"WHERE norm_dn = '%q'", cdn);

		} else {
			/* A normal query. */
			return lsqlite3_tprintf(mem_ctx,
						"SELECT eid FROM ldb_attribute_values "
						"WHERE norm_attr_name = '%q' "
						"AND norm_attr_value = '%q'",
						attr,
						value.data);

		}

	case LDB_OP_SUBSTRING:

		wild_card_string = talloc_strdup(mem_ctx,
					(t->u.substring.start_with_wildcard)?"*":"");
		if (wild_card_string == NULL) return NULL;

		for (i = 0; t->u.substring.chunks[i]; i++) {
			wild_card_string = talloc_asprintf_append(wild_card_string, "%s*",
							t->u.substring.chunks[i]->data);
			if (wild_card_string == NULL) return NULL;
		}

		if ( ! t->u.substring.end_with_wildcard ) {
			/* remove last wildcard */
			wild_card_string[strlen(wild_card_string) - 1] = '\0';
		}

		attr = ldb_attr_casefold(mem_ctx, t->u.substring.attr);
		if (attr == NULL) return NULL;
		a = ldb_schema_attribute_by_name(ldb, attr);

		subval.data = (void *)wild_card_string;
		subval.length = strlen(wild_card_string) + 1;

		/* Get a canonicalised copy of the data */
		a->syntax->canonicalise_fn(ldb, mem_ctx, &(subval), &value);
		if (value.data == NULL) {
			return NULL;
		}

		return lsqlite3_tprintf(mem_ctx,
					"SELECT eid FROM ldb_attribute_values "
					"WHERE norm_attr_name = '%q' "
					"AND norm_attr_value GLOB '%q'",
					attr,
					value.data);

	case LDB_OP_GREATER:
		attr = ldb_attr_casefold(mem_ctx, t->u.equality.attr);
		if (attr == NULL) return NULL;
		a = ldb_schema_attribute_by_name(ldb, attr);

		/* Get a canonicalised copy of the data */
		a->syntax->canonicalise_fn(ldb, mem_ctx, &(t->u.equality.value), &value);
		if (value.data == NULL) {
			return NULL;
		}

		return lsqlite3_tprintf(mem_ctx,
					"SELECT eid FROM ldb_attribute_values "
					"WHERE norm_attr_name = '%q' "
					"AND ldap_compare(norm_attr_value, '>=', '%q', '%q') ",
					attr,
					value.data,
					attr);

	case LDB_OP_LESS:
		attr = ldb_attr_casefold(mem_ctx, t->u.equality.attr);
		if (attr == NULL) return NULL;
		a = ldb_schema_attribute_by_name(ldb, attr);

		/* Get a canonicalised copy of the data */
		a->syntax->canonicalise_fn(ldb, mem_ctx, &(t->u.equality.value), &value);
		if (value.data == NULL) {
			return NULL;
		}

		return lsqlite3_tprintf(mem_ctx,
					"SELECT eid FROM ldb_attribute_values "
					"WHERE norm_attr_name = '%q' "
					"AND ldap_compare(norm_attr_value, '<=', '%q', '%q') ",
					attr,
					value.data,
					attr);

	case LDB_OP_PRESENT:
		if (strcasecmp(t->u.present.attr, "dn") == 0) {
			return talloc_strdup(mem_ctx, "SELECT eid FROM ldb_entry");
		}

		attr = ldb_attr_casefold(mem_ctx, t->u.present.attr);
		if (attr == NULL) return NULL;

		return lsqlite3_tprintf(mem_ctx,
					"SELECT eid FROM ldb_attribute_values "
					"WHERE norm_attr_name = '%q' ",
					attr);

	case LDB_OP_APPROX:
		attr = ldb_attr_casefold(mem_ctx, t->u.equality.attr);
		if (attr == NULL) return NULL;
		a = ldb_schema_attribute_by_name(ldb, attr);

		/* Get a canonicalised copy of the data */
		a->syntax->canonicalise_fn(ldb, mem_ctx, &(t->u.equality.value), &value);
		if (value.data == NULL) {
			return NULL;
		}

		return lsqlite3_tprintf(mem_ctx,
					"SELECT eid FROM ldb_attribute_values "
					"WHERE norm_attr_name = '%q' "
					"AND ldap_compare(norm_attr_value, '~%', 'q', '%q') ",
					attr,
					value.data,
					attr);

	case LDB_OP_EXTENDED:
#warning  "work out how to handle bitops"
		return NULL;

	default:
		break;
	};

	/* should never occur */
	abort();
	return NULL;
}

/*
 * query_int()
 *
 * This function is used for the common case of queries that return a single
 * integer value.
 *
 * NOTE: If more than one value is returned by the query, all but the first
 * one will be ignored.
 */
static int
query_int(const struct lsqlite3_private * lsqlite3,
          long long * pRet,
          const char * pSql,
          ...)
{
        int             ret;
        int             bLoop;
        char *          p;
        sqlite3_stmt *  pStmt;
        va_list         args;

        /* Begin access to variable argument list */
        va_start(args, pSql);

        /* Format the query */
        if ((p = sqlite3_vmprintf(pSql, args)) == NULL) {
		va_end(args);
                return SQLITE_NOMEM;
        }

        /*
         * Prepare and execute the SQL statement.  Loop allows retrying on
         * certain errors, e.g. SQLITE_SCHEMA occurs if the schema changes,
         * requiring retrying the operation.
         */
        for (bLoop = TRUE; bLoop; ) {

                /* Compile the SQL statement into sqlite virtual machine */
                if ((ret = sqlite3_prepare(lsqlite3->sqlite,
                                           p,
                                           -1,
                                           &pStmt,
                                           NULL)) == SQLITE_SCHEMA) {
                        if (stmtGetEID != NULL) {
                                sqlite3_finalize(stmtGetEID);
                                stmtGetEID = NULL;
                        }
                        continue;
                } else if (ret != SQLITE_OK) {
                        break;
                }

                /* One row expected */
                if ((ret = sqlite3_step(pStmt)) == SQLITE_SCHEMA) {
                        if (stmtGetEID != NULL) {
                                sqlite3_finalize(stmtGetEID);
                                stmtGetEID = NULL;
                        }
                        (void) sqlite3_finalize(pStmt);
                        continue;
                } else if (ret != SQLITE_ROW) {
                        (void) sqlite3_finalize(pStmt);
                        break;
                }

                /* Get the value to be returned */
                *pRet = sqlite3_column_int64(pStmt, 0);

                /* Free the virtual machine */
                if ((ret = sqlite3_finalize(pStmt)) == SQLITE_SCHEMA) {
                        if (stmtGetEID != NULL) {
                                sqlite3_finalize(stmtGetEID);
                                stmtGetEID = NULL;
                        }
                        continue;
                } else if (ret != SQLITE_OK) {
                        (void) sqlite3_finalize(pStmt);
                        break;
                }

                /*
                 * Normal condition is only one time through loop.  Loop is
                 * rerun in error conditions, via "continue", above.
                 */
                bLoop = FALSE;
        }

        /* All done with variable argument list */
        va_end(args);


        /* Free the memory we allocated for our query string */
        sqlite3_free(p);

        return ret;
}

/*
 * This is a bad hack to support ldap style comparisons within sqlite.
 * val is the attribute in the row currently under test
 * func is the desired test "<=" ">=" "~" ":"
 * cmp is the value to compare against (eg: "test")
 * attr is the attribute name the value of which we want to test
 */

static void lsqlite3_compare(sqlite3_context *ctx, int argc,
					sqlite3_value **argv)
{
	struct ldb_context *ldb = (struct ldb_context *)sqlite3_user_data(ctx);
	const char *val = (const char *)sqlite3_value_text(argv[0]);
	const char *func = (const char *)sqlite3_value_text(argv[1]);
	const char *cmp = (const char *)sqlite3_value_text(argv[2]);
	const char *attr = (const char *)sqlite3_value_text(argv[3]);
	const struct ldb_schema_attribute *a;
	struct ldb_val valX;
	struct ldb_val valY;
	int ret;

	switch (func[0]) {
	/* greater */
	case '>': /* >= */
		a = ldb_schema_attribute_by_name(ldb, attr);
		valX.data = (uint8_t *)cmp;
		valX.length = strlen(cmp);
		valY.data = (uint8_t *)val;
		valY.length = strlen(val);
		ret = a->syntax->comparison_fn(ldb, ldb, &valY, &valX);
		if (ret >= 0)
			sqlite3_result_int(ctx, 1);
		else
			sqlite3_result_int(ctx, 0);
		return;

	/* lesser */
	case '<': /* <= */
		a = ldb_schema_attribute_by_name(ldb, attr);
		valX.data = (uint8_t *)cmp;
		valX.length = strlen(cmp);
		valY.data = (uint8_t *)val;
		valY.length = strlen(val);
		ret = a->syntax->comparison_fn(ldb, ldb, &valY, &valX);
		if (ret <= 0)
			sqlite3_result_int(ctx, 1);
		else
			sqlite3_result_int(ctx, 0);
		return;

	/* approx */
	case '~':
		/* TODO */
		sqlite3_result_int(ctx, 0);
		return;

	/* bitops */
	case ':':
		/* TODO */
		sqlite3_result_int(ctx, 0);
		return;

	default:
		break;
	}

	sqlite3_result_error(ctx, "Value must start with a special operation char (<>~:)!", -1);
	return;
}


/* rename a record */
static int lsqlite3_safe_rollback(sqlite3 *sqlite)
{
	char *errmsg;
	int ret;

	/* execute */
	ret = sqlite3_exec(sqlite, "ROLLBACK;", NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		if (errmsg) {
			printf("lsqlite3_safe_rollback: Error: %s\n", errmsg);
			free(errmsg);
		}
		return -1;
	}

        return 0;
}

/* return an eid as result */
static int lsqlite3_eid_callback(void *result, int col_num, char **cols, char **names)
{
	long long *eid = (long long *)result;

	if (col_num != 1) return SQLITE_ABORT;
	if (strcasecmp(names[0], "eid") != 0) return SQLITE_ABORT;

	*eid = atoll(cols[0]);
	return SQLITE_OK;
}

/*
 * add a single set of ldap message values to a ldb_message
 */
static int lsqlite3_search_callback(void *result, int col_num, char **cols, char **names)
{
	struct ldb_context *ldb;
	struct lsql_context *ac;
	struct ldb_message *msg;
	long long eid;
	unsigned int i;
	int ret;

	ac = talloc_get_type(result, struct lsql_context);
	ldb = ldb_module_get_ctx(ac->module);

	/* eid, dn, attr_name, attr_value */
	if (col_num != 4) return SQLITE_ABORT;

	eid = atoll(cols[0]);

	if (ac->ares) {
		msg = ac->ares->message;
	}

	if (eid != ac->current_eid) { /* here begin a new entry */

		/* call the async callback for the last entry
		 * except the first time */
		if (ac->current_eid != 0) {
			ret = ldb_msg_normalize(ldb, ac->req, msg, &msg);
			if (ret != LDB_SUCCESS) {
				return SQLITE_ABORT;
			}

			ret = ldb_module_send_entry(ac->req, msg, NULL);
			if (ret != LDB_SUCCESS) {
				ac->callback_failed = true;
				/* free msg object */
				TALLOC_FREE(msg);
				return SQLITE_ABORT;
			}

			/* free msg object */
			TALLOC_FREE(msg);
		}

		/* start over */
		ac->ares = talloc_zero(ac, struct ldb_reply);
		if (!ac->ares) return SQLITE_ABORT;

		msg = ldb_msg_new(ac->ares);
		if (!msg) return SQLITE_ABORT;

		ac->ares->type = LDB_REPLY_ENTRY;
		ac->current_eid = eid;
	}

	if (msg->dn == NULL) {
		msg->dn = ldb_dn_new(msg, ldb, cols[1]);
		if (msg->dn == NULL)
			return SQLITE_ABORT;
	}

	if (ac->attrs) {
		int found = 0;
		for (i = 0; ac->attrs[i]; i++) {
			if (strcasecmp(cols[2], ac->attrs[i]) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) goto done;
	}

	if (ldb_msg_add_string(msg, cols[2], cols[3]) != 0) {
		return SQLITE_ABORT;
	}

done:
	ac->ares->message = msg;
	return SQLITE_OK;
}


/*
 * lsqlite3_get_eid()
 * lsqlite3_get_eid_ndn()
 *
 * These functions are used for the very common case of retrieving an EID value
 * given a (normalized) DN.
 */

static long long lsqlite3_get_eid_ndn(sqlite3 *sqlite, void *mem_ctx, const char *norm_dn)
{
	char *errmsg;
	char *query;
	long long eid = -1;
	long long ret;

	/* get object eid */
	query = lsqlite3_tprintf(mem_ctx, "SELECT eid "
					  "FROM ldb_entry "
					  "WHERE norm_dn = '%q';", norm_dn);
	if (query == NULL) return -1;

	ret = sqlite3_exec(sqlite, query, lsqlite3_eid_callback, &eid, &errmsg);
	if (ret != SQLITE_OK) {
		if (errmsg) {
			printf("lsqlite3_get_eid: Fatal Error: %s\n", errmsg);
			free(errmsg);
		}
		return -1;
	}

	return eid;
}

static long long lsqlite3_get_eid(struct lsqlite3_private *lsqlite3,
				  struct ldb_dn *dn)
{
	TALLOC_CTX *local_ctx;
	long long eid = -1;
	char *cdn;

	/* ignore ltdb specials */
	if (ldb_dn_is_special(dn)) {
		return -1;
	}

	/* create a local ctx */
	local_ctx = talloc_named(lsqlite3, 0, "lsqlite3_get_eid local context");
	if (local_ctx == NULL) {
		return -1;
	}

	cdn = ldb_dn_alloc_casefold(local_ctx, dn);
	if (!cdn) goto done;

	eid = lsqlite3_get_eid_ndn(lsqlite3->sqlite, local_ctx, cdn);

done:
	talloc_free(local_ctx);
	return eid;
}

/*
 * Interface functions referenced by lsqlite3_ops
 */

/* search for matching records, by tree */
int lsql_search(struct lsql_context *ctx)
{
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	struct lsqlite3_private *lsqlite3;
	struct ldb_context *ldb;
	char *norm_basedn;
	char *sqlfilter;
	char *errmsg;
	char *query = NULL;
        int ret;

	ldb = ldb_module_get_ctx(module);
	lsqlite3 = talloc_get_type(ldb_module_get_private(module),
				   struct lsqlite3_private);

	if ((( ! ldb_dn_is_valid(req->op.search.base)) ||
	     ldb_dn_is_null(req->op.search.base)) &&
	    (req->op.search.scope == LDB_SCOPE_BASE ||
	     req->op.search.scope == LDB_SCOPE_ONELEVEL)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (req->op.search.base) {
		norm_basedn = ldb_dn_alloc_casefold(ctx, req->op.search.base);
		if (norm_basedn == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	} else norm_basedn = talloc_strdup(ctx, "");

        /* Convert filter into a series of SQL conditions (constraints) */
	sqlfilter = parsetree_to_sql(module, ctx, req->op.search.tree);

        switch(req->op.search.scope) {
        case LDB_SCOPE_DEFAULT:
        case LDB_SCOPE_SUBTREE:
		if (*norm_basedn != '\0') {
			query = lsqlite3_tprintf(ctx,
				"SELECT entry.eid,\n"
				"       entry.dn,\n"
				"       av.attr_name,\n"
				"       av.attr_value\n"
				"  FROM ldb_entry AS entry\n"

				"  LEFT OUTER JOIN ldb_attribute_values AS av\n"
				"    ON av.eid = entry.eid\n"

				"  WHERE entry.eid IN\n"
				"    (SELECT DISTINCT ldb_entry.eid\n"
				"       FROM ldb_entry\n"
				"       WHERE (ldb_entry.norm_dn GLOB('*,%q')\n"
				"       OR ldb_entry.norm_dn = '%q')\n"
				"       AND ldb_entry.eid IN\n"
				"         (%s)\n"
				"    )\n"

				"  ORDER BY entry.eid ASC;",
				norm_basedn,
				norm_basedn,
				sqlfilter);
		} else {
			query = lsqlite3_tprintf(ctx,
				"SELECT entry.eid,\n"
				"       entry.dn,\n"
				"       av.attr_name,\n"
				"       av.attr_value\n"
				"  FROM ldb_entry AS entry\n"

				"  LEFT OUTER JOIN ldb_attribute_values AS av\n"
				"    ON av.eid = entry.eid\n"

				"  WHERE entry.eid IN\n"
				"    (SELECT DISTINCT ldb_entry.eid\n"
				"       FROM ldb_entry\n"
				"       WHERE ldb_entry.eid IN\n"
				"         (%s)\n"
				"    )\n"

				"  ORDER BY entry.eid ASC;",
				sqlfilter);
		}

		break;

        case LDB_SCOPE_BASE:
                query = lsqlite3_tprintf(ctx,
                        "SELECT entry.eid,\n"
                        "       entry.dn,\n"
                        "       av.attr_name,\n"
                        "       av.attr_value\n"
                        "  FROM ldb_entry AS entry\n"

                        "  LEFT OUTER JOIN ldb_attribute_values AS av\n"
                        "    ON av.eid = entry.eid\n"

                        "  WHERE entry.eid IN\n"
                        "    (SELECT DISTINCT ldb_entry.eid\n"
                        "       FROM ldb_entry\n"
                        "       WHERE ldb_entry.norm_dn = '%q'\n"
                        "         AND ldb_entry.eid IN\n"
			"           (%s)\n"
                        "    )\n"

                        "  ORDER BY entry.eid ASC;",
			norm_basedn,
                        sqlfilter);
                break;

        case LDB_SCOPE_ONELEVEL:
                query = lsqlite3_tprintf(ctx,
                        "SELECT entry.eid,\n"
                        "       entry.dn,\n"
                        "       av.attr_name,\n"
                        "       av.attr_value\n"
                        "  FROM ldb_entry AS entry\n"

                        "  LEFT OUTER JOIN ldb_attribute_values AS av\n"
                        "    ON av.eid = entry.eid\n"

                        "  WHERE entry.eid IN\n"
                        "    (SELECT DISTINCT ldb_entry.eid\n"
                        "       FROM ldb_entry\n"
			"       WHERE norm_dn GLOB('*,%q')\n"
			"         AND NOT norm_dn GLOB('*,*,%q')\n"
                        "         AND ldb_entry.eid IN\n(%s)\n"
                        "    )\n"

                        "  ORDER BY entry.eid ASC;",
                        norm_basedn,
                        norm_basedn,
                        sqlfilter);
                break;
        }

        if (query == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
        }

	/* * /
	printf ("%s\n", query);
	/ * */

	ctx->current_eid = 0;
	ctx->attrs = req->op.search.attrs;
	ctx->ares = NULL;

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	ret = sqlite3_exec(lsqlite3->sqlite, query, lsqlite3_search_callback, ctx, &errmsg);
	if (ret != SQLITE_OK) {
		if (errmsg) {
			ldb_set_errstring(ldb, errmsg);
			free(errmsg);
		}
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* complete the last message if any */
	if (ctx->ares) {
		ret = ldb_msg_normalize(ldb, ctx->ares,
		                        ctx->ares->message,
		                        &ctx->ares->message);
		if (ret != LDB_SUCCESS) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = ldb_module_send_entry(req, ctx->ares->message, NULL);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}


	return LDB_SUCCESS;
}

/* add a record */
static int lsql_add(struct lsql_context *ctx)
{
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	struct lsqlite3_private *lsqlite3;
	struct ldb_context *ldb;
	struct ldb_message *msg = req->op.add.message;
        long long eid;
	char *dn, *ndn;
	char *errmsg;
	char *query;
	unsigned int i;
	int ret;

	ldb = ldb_module_get_ctx(module);
	lsqlite3 = talloc_get_type(ldb_module_get_private(module),
				   struct lsqlite3_private);

        /* See if this is an ltdb special */
	if (ldb_dn_is_special(msg->dn)) {
/*
		struct ldb_dn *c;
		c = ldb_dn_new(local_ctx, ldb, "@INDEXLIST");
		if (ldb_dn_compare(ldb, msg->dn, c) == 0) {
#warning "should we handle indexes somehow ?"
			ret = LDB_ERR_UNWILLING_TO_PERFORM;
			goto done;
		}
*/
                /* Others return an error */
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	/* create linearized and normalized dns */
	dn = ldb_dn_alloc_linearized(ctx, msg->dn);
	ndn = ldb_dn_alloc_casefold(ctx, msg->dn);
	if (dn == NULL || ndn == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	query = lsqlite3_tprintf(ctx,
				   /* Add new entry */
				   "INSERT OR ABORT INTO ldb_entry "
				   "('dn', 'norm_dn') "
				   "VALUES ('%q', '%q');",
				dn, ndn);
	if (query == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = sqlite3_exec(lsqlite3->sqlite, query, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		if (errmsg) {
			ldb_set_errstring(ldb, errmsg);
			free(errmsg);
		}
		return LDB_ERR_OPERATIONS_ERROR;
	}

	eid = lsqlite3_get_eid_ndn(lsqlite3->sqlite, ctx, ndn);
	if (eid == -1) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i = 0; i < msg->num_elements; i++) {
		const struct ldb_message_element *el = &msg->elements[i];
		const struct ldb_schema_attribute *a;
		char *attr;
		unsigned int j;

		/* Get a case-folded copy of the attribute name */
		attr = ldb_attr_casefold(ctx, el->name);
		if (attr == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		a = ldb_schema_attribute_by_name(ldb, el->name);

		if (el->num_value == 0) {
			ldb_asprintf_errstring(ldb, "attribute %s on %s specified, but with 0 values (illegal)",
					       el->name, ldb_dn_get_linearized(msg->dn));
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}

		/* For each value of the specified attribute name... */
		for (j = 0; j < el->num_values; j++) {
			struct ldb_val value;
			char *insert;

			/* Get a canonicalised copy of the data */
			a->syntax->canonicalise_fn(ldb, ctx, &(el->values[j]), &value);
			if (value.data == NULL) {
				return LDB_ERR_OPERATIONS_ERROR;
			}

			insert = lsqlite3_tprintf(ctx,
					"INSERT OR ROLLBACK INTO ldb_attribute_values "
					"('eid', 'attr_name', 'norm_attr_name',"
					" 'attr_value', 'norm_attr_value') "
					"VALUES ('%lld', '%q', '%q', '%q', '%q');",
					eid, el->name, attr,
					el->values[j].data, value.data);
			if (insert == NULL) {
				return LDB_ERR_OPERATIONS_ERROR;
			}

			ret = sqlite3_exec(lsqlite3->sqlite, insert, NULL, NULL, &errmsg);
			if (ret != SQLITE_OK) {
				if (errmsg) {
					ldb_set_errstring(ldb, errmsg);
					free(errmsg);
				}
				return LDB_ERR_OPERATIONS_ERROR;
			}
		}
	}

	return LDB_SUCCESS;
}

/* modify a record */
static int lsql_modify(struct lsql_context *ctx)
{
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	struct lsqlite3_private *lsqlite3;
	struct ldb_context *ldb;
	struct ldb_message *msg = req->op.mod.message;
	long long eid;
	char *errmsg;
	unsigned int i;
	int ret;

	ldb = ldb_module_get_ctx(module);
	lsqlite3 = talloc_get_type(ldb_module_get_private(module),
				   struct lsqlite3_private);

        /* See if this is an ltdb special */
	if (ldb_dn_is_special(msg->dn)) {
                /* Others return an error */
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	eid = lsqlite3_get_eid(lsqlite3, msg->dn);
	if (eid == -1) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i = 0; i < msg->num_elements; i++) {
		const struct ldb_message_element *el = &msg->elements[i];
		const struct ldb_schema_attribute *a;
		int flags = el->flags & LDB_FLAG_MOD_MASK;
		char *attr;
		char *mod;
		unsigned int j;

		/* Get a case-folded copy of the attribute name */
		attr = ldb_attr_casefold(ctx, el->name);
		if (attr == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		a = ldb_schema_attribute_by_name(ldb, el->name);

		switch (flags) {

		case LDB_FLAG_MOD_REPLACE:

			for (j=0; j<el->num_values; j++) {
				if (ldb_msg_find_val(el, &el->values[j]) != &el->values[j]) {
					ldb_asprintf_errstring(ldb, "%s: value #%d provided more than once", el->name, j);
					return LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS;
				}
			}

			/* remove all attributes before adding the replacements */
			mod = lsqlite3_tprintf(ctx,
						"DELETE FROM ldb_attribute_values "
						"WHERE eid = '%lld' "
						"AND norm_attr_name = '%q';",
						eid, attr);
			if (mod == NULL) {
				return LDB_ERR_OPERATIONS_ERROR;
			}

			ret = sqlite3_exec(lsqlite3->sqlite, mod, NULL, NULL, &errmsg);
			if (ret != SQLITE_OK) {
				if (errmsg) {
					ldb_set_errstring(ldb, errmsg);
					free(errmsg);
				}
				return LDB_ERR_OPERATIONS_ERROR;
                        }

			/* MISSING break is INTENTIONAL */

		case LDB_FLAG_MOD_ADD:

			if (el->num_values == 0) {
				ldb_asprintf_errstring(ldb, "attribute %s on %s specified, but with 0 values (illigal)",
						       el->name, ldb_dn_get_linearized(msg->dn));
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}

			/* For each value of the specified attribute name... */
			for (j = 0; j < el->num_values; j++) {
				struct ldb_val value;

				/* Get a canonicalised copy of the data */
				a->syntax->canonicalise_fn(ldb, ctx, &(el->values[j]), &value);
				if (value.data == NULL) {
					return LDB_ERR_OPERATIONS_ERROR;
				}

				mod = lsqlite3_tprintf(ctx,
					"INSERT OR ROLLBACK INTO ldb_attribute_values "
					"('eid', 'attr_name', 'norm_attr_name',"
					" 'attr_value', 'norm_attr_value') "
					"VALUES ('%lld', '%q', '%q', '%q', '%q');",
					eid, el->name, attr,
					el->values[j].data, value.data);

				if (mod == NULL) {
					return LDB_ERR_OPERATIONS_ERROR;
				}

				ret = sqlite3_exec(lsqlite3->sqlite, mod, NULL, NULL, &errmsg);
				if (ret != SQLITE_OK) {
					if (errmsg) {
						ldb_set_errstring(ldb, errmsg);
						free(errmsg);
					}
					return LDB_ERR_OPERATIONS_ERROR;
				}
			}

			break;

		case LDB_FLAG_MOD_DELETE:
#warning "We should throw an error if the attribute we are trying to delete does not exist!"
			if (el->num_values == 0) {
				mod = lsqlite3_tprintf(ctx,
							"DELETE FROM ldb_attribute_values "
							"WHERE eid = '%lld' "
							"AND norm_attr_name = '%q';",
							eid, attr);
				if (mod == NULL) {
					return LDB_ERR_OPERATIONS_ERROR;
				}

				ret = sqlite3_exec(lsqlite3->sqlite, mod, NULL, NULL, &errmsg);
				if (ret != SQLITE_OK) {
					if (errmsg) {
						ldb_set_errstring(ldb, errmsg);
						free(errmsg);
					}
					return LDB_ERR_OPERATIONS_ERROR;
                        	}
			}

			/* For each value of the specified attribute name... */
			for (j = 0; j < el->num_values; j++) {
				struct ldb_val value;

				/* Get a canonicalised copy of the data */
				a->syntax->canonicalise_fn(ldb, ctx, &(el->values[j]), &value);
				if (value.data == NULL) {
					return LDB_ERR_OPERATIONS_ERROR;
				}

				mod = lsqlite3_tprintf(ctx,
					"DELETE FROM ldb_attribute_values "
					"WHERE eid = '%lld' "
					"AND norm_attr_name = '%q' "
					"AND norm_attr_value = '%q';",
					eid, attr, value.data);

				if (mod == NULL) {
					return LDB_ERR_OPERATIONS_ERROR;
				}

				ret = sqlite3_exec(lsqlite3->sqlite, mod, NULL, NULL, &errmsg);
				if (ret != SQLITE_OK) {
					if (errmsg) {
						ldb_set_errstring(ldb, errmsg);
						free(errmsg);
					}
					return LDB_ERR_OPERATIONS_ERROR;
				}
			}

			break;
		}
	}

	return LDB_SUCCESS;
}

/* delete a record */
static int lsql_delete(struct lsql_context *ctx)
{
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	struct lsqlite3_private *lsqlite3;
	struct ldb_context *ldb;
        long long eid;
	char *errmsg;
	char *query;
	int ret;

	ldb = ldb_module_get_ctx(module);
	lsqlite3 = talloc_get_type(ldb_module_get_private(module),
				   struct lsqlite3_private);

	eid = lsqlite3_get_eid(lsqlite3, req->op.del.dn);
	if (eid == -1) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	query = lsqlite3_tprintf(ctx,
				   /* Delete entry */
				   "DELETE FROM ldb_entry WHERE eid = %lld; "
				   /* Delete attributes */
				   "DELETE FROM ldb_attribute_values WHERE eid = %lld; ",
				eid, eid);
	if (query == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = sqlite3_exec(lsqlite3->sqlite, query, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		if (errmsg) {
			ldb_set_errstring(ldb, errmsg);
			free(errmsg);
		}
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}

/* rename a record */
static int lsql_rename(struct lsql_context *ctx)
{
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	struct lsqlite3_private *lsqlite3;
	struct ldb_context *ldb;
	char *new_dn, *new_cdn, *old_cdn;
	char *errmsg;
	char *query;
	int ret;

	ldb = ldb_module_get_ctx(module);
	lsqlite3 = talloc_get_type(ldb_module_get_private(module),
				   struct lsqlite3_private);

	/* create linearized and normalized dns */
	old_cdn = ldb_dn_alloc_casefold(ctx, req->op.rename.olddn);
	new_cdn = ldb_dn_alloc_casefold(ctx, req->op.rename.newdn);
	new_dn = ldb_dn_alloc_linearized(ctx, req->op.rename.newdn);
	if (old_cdn == NULL || new_cdn == NULL || new_dn == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* build the SQL query */
	query = lsqlite3_tprintf(ctx,
				 "UPDATE ldb_entry SET dn = '%q', norm_dn = '%q' "
				 "WHERE norm_dn = '%q';",
				 new_dn, new_cdn, old_cdn);
	if (query == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* execute */
	ret = sqlite3_exec(lsqlite3->sqlite, query, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		if (errmsg) {
			ldb_set_errstring(ldb, errmsg);
			free(errmsg);
		}
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}

static int lsql_start_trans(struct ldb_module * module)
{
	int ret;
	char *errmsg;
	struct lsqlite3_private *lsqlite3;

	lsqlite3 = talloc_get_type(ldb_module_get_private(module),
				   struct lsqlite3_private);

	if (lsqlite3->trans_count == 0) {
		ret = sqlite3_exec(lsqlite3->sqlite, "BEGIN IMMEDIATE;", NULL, NULL, &errmsg);
		if (ret != SQLITE_OK) {
			if (errmsg) {
				printf("lsqlite3_start_trans: error: %s\n", errmsg);
				free(errmsg);
			}
			return -1;
		}
	};

	lsqlite3->trans_count++;

	return 0;
}

static int lsql_end_trans(struct ldb_module *module)
{
	int ret;
	char *errmsg;
	struct lsqlite3_private *lsqlite3;

	lsqlite3 = talloc_get_type(ldb_module_get_private(module),
				   struct lsqlite3_private);

	if (lsqlite3->trans_count > 0) {
		lsqlite3->trans_count--;
	} else return -1;

	if (lsqlite3->trans_count == 0) {
		ret = sqlite3_exec(lsqlite3->sqlite, "COMMIT;", NULL, NULL, &errmsg);
		if (ret != SQLITE_OK) {
			if (errmsg) {
				printf("lsqlite3_end_trans: error: %s\n", errmsg);
				free(errmsg);
			}
			return -1;
		}
	}

        return 0;
}

static int lsql_del_trans(struct ldb_module *module)
{
	struct lsqlite3_private *lsqlite3;

	lsqlite3 = talloc_get_type(ldb_module_get_private(module),
				   struct lsqlite3_private);

	if (lsqlite3->trans_count > 0) {
		lsqlite3->trans_count--;
	} else return -1;

	if (lsqlite3->trans_count == 0) {
		return lsqlite3_safe_rollback(lsqlite3->sqlite);
	}

	return -1;
}

static int destructor(struct lsqlite3_private *lsqlite3)
{
	if (lsqlite3->sqlite) {
		sqlite3_close(lsqlite3->sqlite);
	}
	return 0;
}

static void lsql_request_done(struct lsql_context *ctx, int error)
{
	struct ldb_context *ldb;
	struct ldb_request *req;
	struct ldb_reply *ares;

	ldb = ldb_module_get_ctx(ctx->module);
	req = ctx->req;

	/* if we already returned an error just return */
	if (ldb_request_get_status(req) != LDB_SUCCESS) {
		return;
	}

	ares = talloc_zero(req, struct ldb_reply);
	if (!ares) {
		ldb_oom(ldb);
		req->callback(req, NULL);
		return;
	}
	ares->type = LDB_REPLY_DONE;
	ares->error = error;

	req->callback(req, ares);
}

static void lsql_timeout(struct tevent_context *ev,
			 struct tevent_timer *te,
			 struct timeval t,
			 void *private_data)
{
	struct lsql_context *ctx;
	ctx = talloc_get_type(private_data, struct lsql_context);

	lsql_request_done(ctx, LDB_ERR_TIME_LIMIT_EXCEEDED);
}

static void lsql_callback(struct tevent_context *ev,
			  struct tevent_timer *te,
			  struct timeval t,
			  void *private_data)
{
	struct lsql_context *ctx;
	int ret;

	ctx = talloc_get_type(private_data, struct lsql_context);

	switch (ctx->req->operation) {
	case LDB_SEARCH:
		ret = lsql_search(ctx);
		break;
	case LDB_ADD:
		ret = lsql_add(ctx);
		break;
	case LDB_MODIFY:
		ret = lsql_modify(ctx);
		break;
	case LDB_DELETE:
		ret = lsql_delete(ctx);
		break;
	case LDB_RENAME:
		ret = lsql_rename(ctx);
		break;
/* TODO:
	case LDB_EXTENDED:
		ret = lsql_extended(ctx);
		break;
 */
	default:
		/* no other op supported */
		ret = LDB_ERR_PROTOCOL_ERROR;
	}

	if (!ctx->callback_failed) {
		/* Once we are done, we do not need timeout events */
		talloc_free(ctx->timeout_event);
		lsql_request_done(ctx, ret);
	}
}

static int lsql_handle_request(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct tevent_context *ev;
	struct lsql_context *ac;
	struct tevent_timer *te;
	struct timeval tv;

	if (ldb_check_critical_controls(req->controls)) {
		return LDB_ERR_UNSUPPORTED_CRITICAL_EXTENSION;
	}

	if (req->starttime == 0 || req->timeout == 0) {
		ldb_set_errstring(ldb, "Invalid timeout settings");
		return LDB_ERR_TIME_LIMIT_EXCEEDED;
	}

	ldb = ldb_module_get_ctx(module);
	ev = ldb_get_event_context(ldb);

	ac = talloc_zero(req, struct lsql_context);
	if (ac == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->module = module;
	ac->req = req;

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	te = tevent_add_timer(ev, ac, tv, lsql_callback, ac);
	if (NULL == te) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	tv.tv_sec = req->starttime + req->timeout;
	ac->timeout_event = tevent_add_timer(ev, ac, tv, lsql_timeout, ac);
	if (NULL == ac->timeout_event) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}

/*
 * Table of operations for the sqlite3 backend
 */
static const struct ldb_module_ops lsqlite3_ops = {
	.name              = "sqlite",
	.search            = lsql_handle_request,
	.add               = lsql_handle_request,
        .modify            = lsql_handle_request,
        .del               = lsql_handle_request,
        .rename            = lsql_handle_request,
	.extended          = lsql_handle_request,
	.start_transaction = lsql_start_trans,
	.end_transaction   = lsql_end_trans,
	.del_transaction   = lsql_del_trans,
};

/*
 * Static functions
 */

static int initialize(struct lsqlite3_private *lsqlite3,
		      struct ldb_context *ldb, const char *url, int flags)
{
	TALLOC_CTX *local_ctx;
        long long queryInt;
	int rollback = 0;
	char *errmsg;
        char *schema;
        int ret;

	/* create a local ctx */
	local_ctx = talloc_named(lsqlite3, 0, "lsqlite3_rename local context");
	if (local_ctx == NULL) {
		return -1;
	}

	schema = lsqlite3_tprintf(local_ctx,


                "CREATE TABLE ldb_info AS "
                "  SELECT 'LDB' AS database_type,"
                "         '1.0' AS version;"

                /*
                 * The entry table holds the information about an entry.
                 * This table is used to obtain the EID of the entry and to
                 * support scope=one and scope=base.  The parent and child
                 * table is included in the entry table since all the other
                 * attributes are dependent on EID.
                 */
                "CREATE TABLE ldb_entry "
                "("
                "  eid     INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  dn      TEXT UNIQUE NOT NULL,"
		"  norm_dn TEXT UNIQUE NOT NULL"
                ");"


                "CREATE TABLE ldb_object_classes"
                "("
                "  class_name            TEXT PRIMARY KEY,"
                "  parent_class_name     TEXT,"
                "  tree_key              TEXT UNIQUE,"
                "  max_child_num         INTEGER DEFAULT 0"
                ");"

                /*
                 * We keep a full listing of attribute/value pairs here
                 */
                "CREATE TABLE ldb_attribute_values"
                "("
                "  eid             INTEGER REFERENCES ldb_entry,"
                "  attr_name       TEXT,"
                "  norm_attr_name  TEXT,"
                "  attr_value      TEXT,"
                "  norm_attr_value TEXT "
                ");"


                /*
                 * Indexes
                 */
                "CREATE INDEX ldb_attribute_values_eid_idx "
                "  ON ldb_attribute_values (eid);"

                "CREATE INDEX ldb_attribute_values_name_value_idx "
                "  ON ldb_attribute_values (attr_name, norm_attr_value);"



                /*
                 * Triggers
                 */

                "CREATE TRIGGER ldb_object_classes_insert_tr"
                "  AFTER INSERT"
                "  ON ldb_object_classes"
                "  FOR EACH ROW"
                "    BEGIN"
                "      UPDATE ldb_object_classes"
                "        SET tree_key = COALESCE(tree_key, "
                "              ("
                "                SELECT tree_key || "
                "                       (SELECT base160(max_child_num + 1)"
                "                                FROM ldb_object_classes"
                "                                WHERE class_name = "
                "                                      new.parent_class_name)"
                "                  FROM ldb_object_classes "
                "                  WHERE class_name = new.parent_class_name "
                "              ));"
                "      UPDATE ldb_object_classes "
                "        SET max_child_num = max_child_num + 1"
                "        WHERE class_name = new.parent_class_name;"
                "    END;"

                /*
                 * Table initialization
                 */

                "INSERT INTO ldb_object_classes "
                "    (class_name, tree_key) "
                "  VALUES "
                "    ('TOP', '0001');");

        /* Skip protocol indicator of url  */
        if (strncmp(url, "sqlite3://", 10) != 0) {
                return SQLITE_MISUSE;
        }

        /* Update pointer to just after the protocol indicator */
        url += 10;

        /* Try to open the (possibly empty/non-existent) database */
        if ((ret = sqlite3_open(url, &lsqlite3->sqlite)) != SQLITE_OK) {
                return ret;
        }

        /* In case this is a new database, enable auto_vacuum */
	ret = sqlite3_exec(lsqlite3->sqlite, "PRAGMA auto_vacuum = 1;", NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		if (errmsg) {
			printf("lsqlite3 initializaion error: %s\n", errmsg);
			free(errmsg);
		}
		goto failed;
	}

	if (flags & LDB_FLG_NOSYNC) {
		/* DANGEROUS */
		ret = sqlite3_exec(lsqlite3->sqlite, "PRAGMA synchronous = OFF;", NULL, NULL, &errmsg);
		if (ret != SQLITE_OK) {
			if (errmsg) {
				printf("lsqlite3 initializaion error: %s\n", errmsg);
				free(errmsg);
			}
			goto failed;
		}
	}

	/* */

        /* Establish a busy timeout of 30 seconds */
        if ((ret = sqlite3_busy_timeout(lsqlite3->sqlite,
                                        30000)) != SQLITE_OK) {
                return ret;
        }

        /* Create a function, callable from sql, to increment a tree_key */
        if ((ret =
             sqlite3_create_function(lsqlite3->sqlite,/* handle */
                                     "base160_next",  /* function name */
                                     1,               /* number of args */
                                     SQLITE_ANY,      /* preferred text type */
                                     NULL,            /* user data */
                                     base160next_sql, /* called func */
                                     NULL,            /* step func */
                                     NULL             /* final func */
                     )) != SQLITE_OK) {
                return ret;
        }

        /* Create a function, callable from sql, to convert int to base160 */
        if ((ret =
             sqlite3_create_function(lsqlite3->sqlite,/* handle */
                                     "base160",       /* function name */
                                     1,               /* number of args */
                                     SQLITE_ANY,      /* preferred text type */
                                     NULL,            /* user data */
                                     base160_sql,     /* called func */
                                     NULL,            /* step func */
                                     NULL             /* final func */
                     )) != SQLITE_OK) {
                return ret;
        }

        /* Create a function, callable from sql, to perform various comparisons */
        if ((ret =
             sqlite3_create_function(lsqlite3->sqlite, /* handle */
                                     "ldap_compare",   /* function name */
                                     4,                /* number of args */
                                     SQLITE_ANY,       /* preferred text type */
                                     ldb  ,            /* user data */
                                     lsqlite3_compare, /* called func */
                                     NULL,             /* step func */
                                     NULL              /* final func */
                     )) != SQLITE_OK) {
                return ret;
        }

        /* Begin a transaction */
	ret = sqlite3_exec(lsqlite3->sqlite, "BEGIN EXCLUSIVE;", NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		if (errmsg) {
			printf("lsqlite3: initialization error: %s\n", errmsg);
			free(errmsg);
		}
		goto failed;
	}
	rollback = 1;

        /* Determine if this is a new database.  No tables means it is. */
        if (query_int(lsqlite3,
                      &queryInt,
                      "SELECT COUNT(*)\n"
                      "  FROM sqlite_master\n"
                      "  WHERE type = 'table';") != 0) {
		goto failed;
        }

        if (queryInt == 0) {
                /*
                 * Create the database schema
                 */
		ret = sqlite3_exec(lsqlite3->sqlite, schema, NULL, NULL, &errmsg);
		if (ret != SQLITE_OK) {
			if (errmsg) {
				printf("lsqlite3 initializaion error: %s\n", errmsg);
				free(errmsg);
			}
			goto failed;
		}
        } else {
                /*
                 * Ensure that the database we opened is one of ours
                 */
                if (query_int(lsqlite3,
                              &queryInt,
                              "SELECT "
                              "  (SELECT COUNT(*) = 2"
                              "     FROM sqlite_master "
                              "     WHERE type = 'table' "
                              "       AND name IN "
                              "         ("
                              "           'ldb_entry', "
                              "           'ldb_object_classes' "
                              "         ) "
                              "  ) "
                              "  AND "
                              "  (SELECT 1 "
                              "     FROM ldb_info "
                              "     WHERE database_type = 'LDB' "
                              "       AND version = '1.0'"
                              "  );") != 0 ||
                    queryInt != 1) {

                        /* It's not one that we created.  See ya! */
			goto failed;
                }
        }

        /* Commit the transaction */
	ret = sqlite3_exec(lsqlite3->sqlite, "COMMIT;", NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		if (errmsg) {
			printf("lsqlite3: iniialization error: %s\n", errmsg);
			free(errmsg);
		}
		goto failed;
	}

        return SQLITE_OK;

failed:
	if (rollback) lsqlite3_safe_rollback(lsqlite3->sqlite);
	sqlite3_close(lsqlite3->sqlite);
	return -1;
}

/*
 * connect to the database
 */
static int lsqlite3_connect(struct ldb_context *ldb,
			    const char *url,
			    unsigned int flags,
			    const char *options[],
			    struct ldb_module **_module)
{
	struct ldb_module *module;
	struct lsqlite3_private *lsqlite3;
	unsigned int i;
	int ret;

	module = ldb_module_new(ldb, ldb, "ldb_sqlite3 backend", &lsqlite3_ops);
	if (!module) return LDB_ERR_OPERATIONS_ERROR;

	lsqlite3 = talloc(module, struct lsqlite3_private);
	if (!lsqlite3) {
		goto failed;
	}

	lsqlite3->sqlite = NULL;
	lsqlite3->options = NULL;
	lsqlite3->trans_count = 0;

	ret = initialize(lsqlite3, ldb, url, flags);
	if (ret != SQLITE_OK) {
		goto failed;
	}

	talloc_set_destructor(lsqlite3, destructor);

	ldb_module_set_private(module, lsqlite3);

	if (options) {
		/*
                 * take a copy of the options array, so we don't have to rely
                 * on the caller keeping it around (it might be dynamic)
                 */
		for (i=0;options[i];i++) ;

		lsqlite3->options = talloc_array(lsqlite3, char *, i+1);
		if (!lsqlite3->options) {
			goto failed;
		}

		for (i=0;options[i];i++) {

			lsqlite3->options[i+1] = NULL;
			lsqlite3->options[i] =
                                talloc_strdup(lsqlite3->options, options[i]);
			if (!lsqlite3->options[i]) {
				goto failed;
			}
		}
	}

	*_module = module;
	return LDB_SUCCESS;

failed:
        if (lsqlite3 && lsqlite3->sqlite != NULL) {
                (void) sqlite3_close(lsqlite3->sqlite);
        }
	talloc_free(lsqlite3);
	return LDB_ERR_OPERATIONS_ERROR;
}

int ldb_sqlite3_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_backend("sqlite3", lsqlite3_connect, false);
}
