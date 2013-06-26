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
 *  Component: ldbadd
 *
 *  Description: utility to add records - modelled on ldapadd
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
	printf("Usage: ldbadd <options> <ldif...>\n");
	printf("Adds records to a ldb, reading ldif the specified list of files\n\n");
	ldb_cmdline_help(ldb, "ldbadd", stdout);
	exit(LDB_ERR_OPERATIONS_ERROR);
}


/*
  add records from an opened file
*/
static int process_file(struct ldb_context *ldb, FILE *f, unsigned int *count)
{
	struct ldb_ldif *ldif;
	int ret = LDB_SUCCESS;
        struct ldb_control **req_ctrls = ldb_parse_control_strings(ldb, ldb, (const char **)options->controls);
	if (options->controls != NULL &&  req_ctrls== NULL) {
		printf("parsing controls failed: %s\n", ldb_errstring(ldb));
		return LDB_ERR_OPERATIONS_ERROR;
	}


	while ((ldif = ldb_ldif_read_file(ldb, f))) {
		if (ldif->changetype != LDB_CHANGETYPE_ADD &&
		    ldif->changetype != LDB_CHANGETYPE_NONE) {
			fprintf(stderr, "Only CHANGETYPE_ADD records allowed\n");
			break;
		}

		ret = ldb_msg_normalize(ldb, ldif, ldif->msg, &ldif->msg);
		if (ret != LDB_SUCCESS) {
			fprintf(stderr,
			        "ERR: Message canonicalize failed - %s\n",
			        ldb_strerror(ret));
			failures++;
			ldb_ldif_read_free(ldb, ldif);
			continue;
		}

		ret = ldb_add_ctrl(ldb, ldif->msg,req_ctrls);
		if (ret != LDB_SUCCESS) {
			fprintf(stderr, "ERR: %s : \"%s\" on DN %s\n",
				ldb_strerror(ret), ldb_errstring(ldb),
				ldb_dn_get_linearized(ldif->msg->dn));
			failures++;
		} else {
			(*count)++;
			if (options->verbose) {
				printf("Added %s\n", ldb_dn_get_linearized(ldif->msg->dn));
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

	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		printf("Failed to start transaction: %s\n", ldb_errstring(ldb));
		return ret;
	}

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

	if (count != 0) {
		ret = ldb_transaction_commit(ldb);
		if (ret != LDB_SUCCESS) {
			printf("Failed to commit transaction: %s\n", ldb_errstring(ldb));
			return ret;
		}
	} else {
		ldb_transaction_cancel(ldb);
	}

	talloc_free(mem_ctx);

	printf("Added %u records with %u failures\n", count, failures);

	return ret;
}
