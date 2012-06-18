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

static int failures;

static void usage(void)
{
	printf("Usage: ldbmodify <options> <ldif...>\n");
	printf("Modifies a ldb based upon ldif change records\n\n");
	ldb_cmdline_help("ldbmodify", stdout);
	exit(1);
}

/*
  process modifies for one file
*/
static int process_file(struct ldb_context *ldb, FILE *f, int *count)
{
	struct ldb_ldif *ldif;
	int ret = LDB_SUCCESS;
	
	while ((ldif = ldb_ldif_read_file(ldb, f))) {
		switch (ldif->changetype) {
		case LDB_CHANGETYPE_NONE:
		case LDB_CHANGETYPE_ADD:
			ret = ldb_add(ldb, ldif->msg);
			break;
		case LDB_CHANGETYPE_DELETE:
			ret = ldb_delete(ldb, ldif->msg->dn);
			break;
		case LDB_CHANGETYPE_MODIFY:
			ret = ldb_modify(ldb, ldif->msg);
			break;
		}
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
	int count=0;
	int i, ret=LDB_SUCCESS;
	struct ldb_cmdline *options;

	ldb = ldb_init(NULL, NULL);

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
				exit(1);
			}
			ret = process_file(ldb, f, &count);
			fclose(f);
		}
	}

	talloc_free(ldb);

	printf("Modified %d records with %d failures\n", count, failures);

	return ret;
}
