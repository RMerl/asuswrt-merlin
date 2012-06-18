/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004
   Copyright (C) Stefan Metzmacher  2004

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
 *  Component: ldbrename
 *
 *  Description: utility to rename records - modelled on ldapmodrdn
 *
 *  Author: Andrew Tridgell
 *  Author: Stefan Metzmacher
 */

#include "includes.h"
#include "ldb/include/includes.h"
#include "ldb/tools/cmdline.h"

static void usage(void)
{
	printf("Usage: ldbrename [<options>] <olddn> <newdn>\n");
	printf("Options:\n");
	printf("  -H ldb_url       choose the database (or $LDB_URL)\n");
	printf("  -o options       pass options like modules to activate\n");
	printf("              e.g: -o modules:timestamps\n");
	printf("\n");
	printf("Renames records in a ldb\n\n");
	exit(1);
}


int main(int argc, const char **argv)
{
	struct ldb_context *ldb;
	int ret;
	struct ldb_cmdline *options;
	const struct ldb_dn *dn1, *dn2;

	ldb_global_init();

	ldb = ldb_init(NULL, NULL);

	options = ldb_cmdline_process(ldb, argc, argv, usage);

	if (options->argc < 2) {
		usage();
	}

	dn1 = ldb_dn_explode(ldb, options->argv[0]);
	dn2 = ldb_dn_explode(ldb, options->argv[1]);

	ret = ldb_rename(ldb, dn1, dn2);
	if (ret == 0) {
		printf("Renamed 1 record\n");
	} else  {
		printf("rename of '%s' to '%s' failed - %s\n", 
			options->argv[0], options->argv[1], ldb_errstring(ldb));
	}

	talloc_free(ldb);
	
	return ret;
}
