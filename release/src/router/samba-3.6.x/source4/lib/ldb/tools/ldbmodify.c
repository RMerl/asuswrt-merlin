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
 *  Component: ldbmodify
 *
 *  Description: utility to modify records - modelled on ldapmodify
 *
 *  Author: Andrew Tridgell
 */

#include "ldb.h"
#include "tools/cmdline.h"
#include "ldbutil.h"

static unsigned int failures;
static struct ldb_cmdline *options;

static void usage(struct ldb_context *ldb)
{
	printf("Usage: ldbmodify <options> <ldif...>\n");
	printf("Modifies a ldb based upon ldif change records\n\n");
	ldb_cmdline_help(ldb, "ldbmodify", stdout);
	exit(LDB_ERR_OPERATIONS_ERROR);
}

/*
  process modifies for one file
*/
static int process_file(struct ldb_context *ldb, FILE *f, unsigned int *count)
{
	struct ldb_ldif *ldif;
	int ret = LDB_SUCCESS;
	struct ldb_control **req_ctrls = ldb_parse_control_strings(ldb, ldb, (const char **)options->controls);

	if (options->controls != NULL &&  req_ctrls== NULL) {
		printf("parsing controls failed: %s\n", ldb_errstring(ldb));
		exit(LDB_ERR_OPERATIONS_ERROR);
	}

	while ((ldif = ldb_ldif_read_file(ldb, f))) {
		switch (ldif->changetype) {
		case LDB_CHANGETYPE_NONE:
		case LDB_CHANGETYPE_ADD:
			ret = ldb_add_ctrl(ldb, ldif->msg,req_ctrls);
			break;
		case LDB_CHANGETYPE_DELETE:
			ret = ldb_delete_ctrl(ldb, ldif->msg->dn,req_ctrls);
			break;
		case LDB_CHANGETYPE_MODIFY:
			ret = ldb_modify_ctrl(ldb, ldif->msg,req_ctrls);
			break;
		}
		if (ret != LDB_SUCCESS) {
			fprintf(stderr, "ERR: (%s) \"%s\" on DN %s\n",
				ldb_strerror(ret),
				ldb_errstring(ldb), ldb_dn_get_linearized(ldif->msg->dn));
			failures++;
		} else {
			(*count)++;
			if (options->verbose) {
				printf("Modified %s\n", ldb_dn_get_linearized(ldif->msg->dn));
			}
		}
		ldb_ldif_read_free(ldb, ldif);
	}

	return ret;
}

int main(int argc, const char **argv)
{
	struct ldb_context *ldb;
	unsigned int i, count = 0;
	int ret = LDB_SUCCESS;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);

	ldb = ldb_init(mem_ctx, NULL);
	if (ldb == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	options = ldb_cmdline_process(ldb, argc, argv, usage);

	if (options->argc == 0) {
		ret = process_file(ldb, stdin, &count);
	} else {
		for (i=0;i<options->argc;i++) {
			const char *fname = options->argv[i];
			FILE *f;
			f = fopen(fname, "r");
			if (!f) {
				perror(fname);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			ret = process_file(ldb, f, &count);
			fclose(f);
		}
	}

	talloc_free(mem_ctx);

	printf("Modified %u records with %u failures\n", count, failures);

	return ret;
}
