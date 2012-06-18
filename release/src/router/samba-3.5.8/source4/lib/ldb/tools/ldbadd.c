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

static int failures;

static void usage(void)
{
	printf("Usage: ldbadd <options> <ldif...>\n");	
	printf("Adds records to a ldb, reading ldif the specified list of files\n\n");
	ldb_cmdline_help("ldbadd", stdout);
	exit(1);
}


/*
  add records from an opened file
*/
static int process_file(struct ldb_context *ldb, FILE *f, int *count)
{
	struct ldb_ldif *ldif;
	int ret = LDB_SUCCESS;

	while ((ldif = ldb_ldif_read_file(ldb, f))) {
		if (ldif->changetype != LDB_CHANGETYPE_ADD &&
		    ldif->changetype != LDB_CHANGETYPE_NONE) {
			fprintf(stderr, "Only CHANGETYPE_ADD records allowed\n");
			break;
		}

		ldif->msg = ldb_msg_canonicalize(ldb, ldif->msg);

		ret = ldb_add(ldb, ldif->msg);
		if (ret != LDB_SUCCESS) {
			fprintf(stderr, "ERR: \"%s\" on DN %s\n", 
				ldb_errstring(ldb), ldb_dn_get_linearized(ldif->msg->dn));
			failures++;
		} else {
			(*count)++;
		}
		ldb_ldif_read_free(ldb, ldif);
	}

	return ret;
}



int main(int argc, const char **argv)
{
	struct ldb_context *ldb;
	int i, ret=0, count=0;
	struct ldb_cmdline *options;

	ldb = ldb_init(NULL, NULL);

	options = ldb_cmdline_process(ldb, argc, argv, usage);

	if (ldb_transaction_start(ldb) != 0) {
		printf("Failed to start transaction: %s\n", ldb_errstring(ldb));
		exit(1);
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
				exit(1);
			}
			ret = process_file(ldb, f, &count);
			fclose(f);
		}
	}

	if (count != 0) {
		if (ldb_transaction_commit(ldb) != 0) {
			printf("Failed to commit transaction: %s\n", ldb_errstring(ldb));
			exit(1);
		}
	} else {
		ldb_transaction_cancel(ldb);
	}

	talloc_free(ldb);

	printf("Added %d records with %d failures\n", count, failures);
	
	return ret;
}
