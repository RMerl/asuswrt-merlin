/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004

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
 *  Component: ldbdel
 *
 *  Description: utility to delete records - modelled on ldapdelete
 *
 *  Author: Andrew Tridgell
 */

#include "replace.h"
#include "ldb.h"
#include "tools/cmdline.h"
#include "ldbutil.h"

static int dn_cmp(struct ldb_message **msg1, struct ldb_message **msg2)
{
	return ldb_dn_compare((*msg1)->dn, (*msg2)->dn);
}

static int ldb_delete_recursive(struct ldb_context *ldb, struct ldb_dn *dn,struct ldb_control **req_ctrls)
{
	int ret;
	unsigned int i, total=0;
	const char *attrs[] = { NULL };
	struct ldb_result *res;
	
	ret = ldb_search(ldb, ldb, &res, dn, LDB_SCOPE_SUBTREE, attrs, "distinguishedName=*");
	if (ret != LDB_SUCCESS) return ret;

	/* sort the DNs, deepest first */
	TYPESAFE_QSORT(res->msgs, res->count, dn_cmp);

	for (i = 0; i < res->count; i++) {
		if (ldb_delete_ctrl(ldb, res->msgs[i]->dn,req_ctrls) == LDB_SUCCESS) {
			total++;
		} else {
			printf("Failed to delete '%s' - %s\n",
			       ldb_dn_get_linearized(res->msgs[i]->dn),
			       ldb_errstring(ldb));
		}
	}

	talloc_free(res);

	if (total == 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	printf("Deleted %u records\n", total);
	return LDB_SUCCESS;
}

static void usage(struct ldb_context *ldb)
{
	printf("Usage: ldbdel <options> <DN...>\n");
	printf("Deletes records from a ldb\n\n");
	ldb_cmdline_help(ldb, "ldbdel", stdout);
	exit(LDB_ERR_OPERATIONS_ERROR);
}

int main(int argc, const char **argv)
{
	struct ldb_control **req_ctrls;
	struct ldb_cmdline *options;
	struct ldb_context *ldb;
	int ret = 0, i;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);

	ldb = ldb_init(mem_ctx, NULL);
	if (ldb == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	options = ldb_cmdline_process(ldb, argc, argv, usage);

	if (options->argc < 1) {
		usage(ldb);
	}

	req_ctrls = ldb_parse_control_strings(ldb, ldb, (const char **)options->controls);
	if (options->controls != NULL &&  req_ctrls== NULL) {
		printf("parsing controls failed: %s\n", ldb_errstring(ldb));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i=0;i<options->argc;i++) {
		struct ldb_dn *dn;

		dn = ldb_dn_new(ldb, ldb, options->argv[i]);
		if (dn == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		if (options->recursive) {
			ret = ldb_delete_recursive(ldb, dn,req_ctrls);
		} else {
			ret = ldb_delete_ctrl(ldb, dn,req_ctrls);
			if (ret == LDB_SUCCESS) {
				printf("Deleted 1 record\n");
			}
		}
		if (ret != LDB_SUCCESS) {
			printf("delete of '%s' failed - (%s) %s\n",
			       ldb_dn_get_linearized(dn),
			       ldb_strerror(ret),
			       ldb_errstring(ldb));
		}
	}

	talloc_free(mem_ctx);

	return ret;
}
