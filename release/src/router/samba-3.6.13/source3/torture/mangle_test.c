/* 
   Unix SMB/CIFS implementation.
   SMB torture tester - mangling test
   Copyright (C) Andrew Tridgell 2002
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"
#include "torture/proto.h"
#include "libsmb/libsmb.h"
#include "libsmb/clirap.h"
#include "util_tdb.h"

extern int torture_numops;

static TDB_CONTEXT *tdb;

#define NAME_LENGTH 20

static unsigned total, collisions, failures;

static bool test_one(struct cli_state *cli, const char *name)
{
	uint16_t fnum;
	fstring shortname;
	fstring name2;
	NTSTATUS status;
	TDB_DATA data;

	total++;

	if (!NT_STATUS_IS_OK(cli_open(cli, name, O_RDWR|O_CREAT|O_EXCL, DENY_NONE, &fnum))) {
		printf("open of %s failed (%s)\n", name, cli_errstr(cli));
		return False;
	}

	if (!NT_STATUS_IS_OK(cli_close(cli, fnum))) {
		printf("close of %s failed (%s)\n", name, cli_errstr(cli));
		return False;
	}

	/* get the short name */
	status = cli_qpathinfo_alt_name(cli, name, shortname);
	if (!NT_STATUS_IS_OK(status)) {
		printf("query altname of %s failed (%s)\n", name, cli_errstr(cli));
		return False;
	}

	fstr_sprintf(name2, "\\mangle_test\\%s", shortname);
	if (!NT_STATUS_IS_OK(cli_unlink(cli, name2, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN))) {
		printf("unlink of %s  (%s) failed (%s)\n", 
		       name2, name, cli_errstr(cli));
		return False;
	}

	/* recreate by short name */
	if (!NT_STATUS_IS_OK(cli_open(cli, name2, O_RDWR|O_CREAT|O_EXCL, DENY_NONE, &fnum))) {
		printf("open2 of %s failed (%s)\n", name2, cli_errstr(cli));
		return False;
	}
	if (!NT_STATUS_IS_OK(cli_close(cli, fnum))) {
		printf("close of %s failed (%s)\n", name, cli_errstr(cli));
		return False;
	}

	/* and unlink by long name */
	if (!NT_STATUS_IS_OK(cli_unlink(cli, name, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN))) {
		printf("unlink2 of %s  (%s) failed (%s)\n", 
		       name, name2, cli_errstr(cli));
		failures++;
		cli_unlink(cli, name2, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);
		return True;
	}

	/* see if the short name is already in the tdb */
	data = tdb_fetch_bystring(tdb, shortname);
	if (data.dptr) {
		/* maybe its a duplicate long name? */
		if (!strequal(name, (const char *)data.dptr)) {
			/* we have a collision */
			collisions++;
			printf("Collision between %s and %s   ->  %s "
				" (coll/tot: %u/%u)\n", 
				name, data.dptr, shortname, collisions, total);
		}
		free(data.dptr);
	} else {
		TDB_DATA namedata;
		/* store it for later */
		namedata.dptr = CONST_DISCARD(uint8 *, name);
		namedata.dsize = strlen(name)+1;
		tdb_store_bystring(tdb, shortname, namedata, TDB_REPLACE);
	}

	return True;
}


static void gen_name(char *name)
{
	const char *chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz._-$~... ";
	unsigned max_idx = strlen(chars);
	unsigned len;
	int i;
	char *p;

	fstrcpy(name, "\\mangle_test\\");
	p = name + strlen(name);

	len = 1 + random() % NAME_LENGTH;
	
	for (i=0;i<len;i++) {
		p[i] = chars[random() % max_idx];
	}

	p[i] = 0;

	if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0) {
		p[0] = '_';
	}

	/* have a high probability of a common lead char */
	if (random() % 2 == 0) {
		p[0] = 'A';
	}

	/* and a medium probability of a common lead string */
	if (random() % 10 == 0) {
		if (strlen(p) <= 5) {
			fstrcpy(p, "ABCDE");
		} else {
			/* try not to kill off the null termination */
			memcpy(p, "ABCDE", 5);
		}
	}

	/* and a high probability of a good extension length */
	if (random() % 2 == 0) {
		char *s = strrchr(p, '.');
		if (s) {
			s[4] = 0;
		}
	}

	/* ..... and a 100% proability of a file not ending in "." */
	if (p[strlen(p)-1] == '.')
		p[strlen(p)-1] = '_';
}


bool torture_mangle(int dummy)
{
	static struct cli_state *cli;
	int i;
	bool ret = True;

	printf("starting mangle test\n");

	if (!torture_open_connection(&cli, 0)) {
		return False;
	}

	/* we will use an internal tdb to store the names we have used */
	tdb = tdb_open(NULL, 100000, TDB_INTERNAL, 0, 0);
	if (!tdb) {
		printf("ERROR: Failed to open tdb\n");
		return False;
	}

	cli_unlink(cli, "\\mangle_test\\*", FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);
	cli_rmdir(cli, "\\mangle_test");

	if (!NT_STATUS_IS_OK(cli_mkdir(cli, "\\mangle_test"))) {
		printf("ERROR: Failed to make directory\n");
		return False;
	}

	for (i=0;i<torture_numops;i++) {
		fstring name;
		ZERO_STRUCT(name);

		gen_name(name);
		
		if (!test_one(cli, name)) {
			ret = False;
			break;
		}
		if (total && total % 100 == 0) {
			printf("collisions %u/%u  - %.2f%%   (%u failures)\r",
			       collisions, total, (100.0*collisions) / total, failures);
		}
	}

	cli_unlink(cli, "\\mangle_test\\*", FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);
	if (!NT_STATUS_IS_OK(cli_rmdir(cli, "\\mangle_test"))) {
		printf("ERROR: Failed to remove directory\n");
		return False;
	}

	printf("\nTotal collisions %u/%u  - %.2f%%   (%u failures)\n",
	       collisions, total, (100.0*collisions) / total, failures);

	torture_close_connection(cli);

	printf("mangle test finished\n");
	return (ret && (failures == 0));
}
