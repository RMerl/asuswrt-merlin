/* 
   Unix SMB/CIFS implementation.
   simple tdb dump util
   Copyright (C) Andrew Tridgell              2001

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

#include "replace.h"
#include "system/locale.h"
#include "system/time.h"
#include "system/filesys.h"
#include "system/wait.h"
#include "tdb.h"

static void print_data(TDB_DATA d)
{
	unsigned char *p = (unsigned char *)d.dptr;
	int len = d.dsize;
	while (len--) {
		if (isprint(*p) && !strchr("\"\\", *p)) {
			fputc(*p, stdout);
		} else {
			printf("\\%02X", *p);
		}
		p++;
	}
}

static int traverse_fn(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf, void *state)
{
	printf("{\n");
	printf("key(%d) = \"", (int)key.dsize);
	print_data(key);
	printf("\"\n");
	printf("data(%d) = \"", (int)dbuf.dsize);
	print_data(dbuf);
	printf("\"\n");
	printf("}\n");
	return 0;
}

static int dump_tdb(const char *fname, const char *keyname)
{
	TDB_CONTEXT *tdb;
	TDB_DATA key, value;
	
	tdb = tdb_open(fname, 0, 0, O_RDONLY, 0);
	if (!tdb) {
		printf("Failed to open %s\n", fname);
		return 1;
	}

	if (!keyname) {
		tdb_traverse(tdb, traverse_fn, NULL);
	} else {
		key.dptr = discard_const_p(uint8_t, keyname);
		key.dsize = strlen(keyname);
		value = tdb_fetch(tdb, key);
		if (!value.dptr) {
			return 1;
		} else {
			print_data(value);
			free(value.dptr);
		}
	}

	return 0;
}

static void usage( void)
{
	printf( "Usage: tdbdump [options] <filename>\n\n");
	printf( "   -h          this help message\n");
	printf( "   -k keyname  dumps value of keyname\n");
}

 int main(int argc, char *argv[])
{
	char *fname, *keyname=NULL;
	int c;

	if (argc < 2) {
		printf("Usage: tdbdump <fname>\n");
		exit(1);
	}

	while ((c = getopt( argc, argv, "hk:")) != -1) {
		switch (c) {
		case 'h':
			usage();
			exit( 0);
		case 'k':
			keyname = optarg;
			break;
		default:
			usage();
			exit( 1);
		}
	}

	fname = argv[optind];

	return dump_tdb(fname, keyname);
}
