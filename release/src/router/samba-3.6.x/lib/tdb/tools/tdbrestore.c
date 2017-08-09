/*
   tdbrestore -- construct a tdb from tdbdump output.
   Copyright (C) Volker Lendecke		2010
   Copyright (C) Simon McVittie			2005

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

#include <assert.h>
#include "replace.h"
#include "system/locale.h"
#include "system/time.h"
#include "system/filesys.h"
#include "system/wait.h"
#include "tdb.h"

#define debug_fprintf(file, fmt, ...) do {/*nothing*/} while (0)

static int read_linehead(FILE *f)
{
	int i, c;
	int num_bytes;
	char prefix[128];

	while (1) {
		c = getc(f);
		if (c == EOF) {
			return -1;
		}
		if (c == '(') {
			break;
		}
	}
	for (i=0; i<sizeof(prefix); i++) {
		c = getc(f);
		if (c == EOF) {
			return -1;
		}
		prefix[i] = c;
		if (c == '"') {
			break;
		}
	}
	if (i == sizeof(prefix)) {
		return -1;
	}
	prefix[i] = '\0';

	if (sscanf(prefix, "%d) = ", &num_bytes) != 1) {
		return -1;
	}
	return num_bytes;
}

static int read_hex(void) {
	int c;
	c = getchar();
	if (c == EOF) {
		fprintf(stderr, "Unexpected EOF in data\n");
		return -1;
	} else if (c == '"') {
		fprintf(stderr, "Unexpected \\\" sequence\n");
		return -1;
	} else if ('0' <= c && c <= '9')  {
		return c - '0';
	} else if ('A' <= c && c <= 'F')  {
		return c - 'A' + 10;
	} else if ('a' <= c && c <= 'f')  {
		return c - 'a' + 10;
	} else {
		fprintf(stderr, "Invalid hex: %c\n", c);
		return -1;
	}
}

static int read_data(FILE *f, TDB_DATA *d, size_t size) {
	int c, low, high;
	int i;

	d->dptr = (unsigned char *)malloc(size);
	if (d->dptr == NULL) {
		return -1;
	}
	d->dsize = size;

	for (i=0; i<size; i++) {
		c = getc(f);
		if (c == EOF) {
			fprintf(stderr, "Unexpected EOF in data\n");
			return 1;
		} else if (c == '"') {
			return 0;
		} else if (c == '\\') {
			high = read_hex();
			if (high < 0) {
				return -1;
			}
			high = high << 4;
			assert(high == (high & 0xf0));
			low = read_hex();
			if (low < 0) {
				return -1;
			}
			assert(low == (low & 0x0f));
			d->dptr[i] = (low|high);
		} else {
			d->dptr[i] = c;
		}
	}
	return 0;
}

static int swallow(FILE *f, const char *s, int *eof)
{
	char line[128];

	if (fgets(line, sizeof(line), f) == NULL) {
		if (eof != NULL) {
			*eof = 1;
		}
		return -1;
	}
	if (strcmp(line, s) != 0) {
		return -1;
	}
	return 0;
}

static int read_rec(FILE *f, TDB_CONTEXT *tdb, int *eof)
{
	int length;
	TDB_DATA key, data;
	int ret = -1;

	key.dptr = NULL;
	data.dptr = NULL;

	if (swallow(f, "{\n", eof) == -1) {
		goto fail;
	}
	length = read_linehead(f);
	if (length == -1) {
		goto fail;
	}
	if (read_data(f, &key, length) == -1) {
		goto fail;
	}
	if (swallow(f, "\"\n", NULL) == -1) {
		goto fail;
	}
	length = read_linehead(f);
	if (length == -1) {
		goto fail;
	}
	if (read_data(f, &data, length) == -1) {
		goto fail;
	}
	if ((swallow(f, "\"\n", NULL) == -1)
	    || (swallow(f, "}\n", NULL) == -1)) {
		goto fail;
	}
	if (tdb_store(tdb, key, data, TDB_INSERT) == -1) {
		fprintf(stderr, "TDB error: %s\n", tdb_errorstr(tdb));
		goto fail;
	}

	ret = 0;
fail:
	free(key.dptr);
	free(data.dptr);
	return ret;
}

static int restore_tdb(const char *fname)
{
	TDB_CONTEXT *tdb;

	tdb = tdb_open(fname, 0, 0, O_RDWR|O_CREAT|O_EXCL, 0666);
	if (!tdb) {
		perror("tdb_open");
		fprintf(stderr, "Failed to open %s\n", fname);
		return 1;
	}

	while (1) {
		int eof = 0;
		if (read_rec(stdin, tdb, &eof) == -1) {
			if (eof) {
				break;
			}
			return 1;
		}
	}
	if (tdb_close(tdb)) {
		fprintf(stderr, "Error closing tdb\n");
		return 1;
	}
	fprintf(stderr, "EOF\n");
	return 0;
}

int main(int argc, char *argv[])
{
	char *fname;

	if (argc < 2) {
		printf("Usage: %s dbname < tdbdump_output\n", argv[0]);
		exit(1);
	}

	fname = argv[1];

	return restore_tdb(fname);
}
