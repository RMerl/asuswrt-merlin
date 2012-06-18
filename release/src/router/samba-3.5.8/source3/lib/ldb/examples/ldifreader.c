/* 
   example code for the ldb database library

   Copyright (C) Brad Hards (bradh@frogmouth.net) 2005-2006

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

/** \example ldifreader.c

The code below shows a simple LDB application.

It lists / dumps the entries in an LDIF file to standard output.

*/

#include "includes.h"
#include "ldb/include/ldb.h"
#include "ldb/include/ldb_errors.h"

/*
  ldb_ldif_write takes a function pointer to a custom output
  function. This version is about as simple as the output function can
  be. In a more complex example, you'd likely be doing something with
  the private data function (e.g. holding a file handle).
*/
static int vprintf_fn(void *private_data, const char *fmt, ...)
{
	int retval;
	va_list ap;

	va_start(ap, fmt);
	/* We just write to standard output */
	retval = vprintf(fmt, ap);
	va_end(ap);
	/* Note that the function should return the number of 
	   bytes written, or a negative error code */
	return retval;
}
  
int main(int argc, const char **argv)
{
	struct ldb_context *ldb;
	FILE *fileStream;
	struct ldb_ldif *ldifMsg;

	if (argc != 2) {
		printf("Usage %s filename.ldif\n", argv[0]);
		exit(1);
	}

	/*
	  This is the always the first thing you want to do in an LDB
	  application - initialise up the context structure.

	  Note that you can use the context structure as a parent
	  for talloc allocations as well
	*/
	ldb = ldb_init(NULL);

	fileStream = fopen(argv[1], "r");
	if (0 == fileStream) {
		perror(argv[1]);
		exit(1);
	}

	/*
	  We now work through the filestream to get each entry.
	*/
	while ( (ldifMsg = ldb_ldif_read_file(ldb, fileStream)) ) {
		/*
		  Each message has a particular change type. For Add,
		  Modify and Delete, this will also appear in the
		  output listing (as changetype: add, changetype:
		  modify or changetype:delete, respectively).
		*/
		switch (ldifMsg->changetype) {
		case LDB_CHANGETYPE_NONE:
			printf("ChangeType: None\n");
			break;
		case LDB_CHANGETYPE_ADD:
			printf("ChangeType: Add\n");
			break;
		case LDB_CHANGETYPE_MODIFY:
			printf("ChangeType: Modify\n");
			break;
		case LDB_CHANGETYPE_DELETE:
			printf("ChangeType: Delete\n");
			break;
		default:
			printf("ChangeType: Unknown\n");
		}

		/*
		  We can now write out the results, using our custom
		  output routine as defined at the top of this file. 
		*/
		ldb_ldif_write(ldb, vprintf_fn, NULL, ldifMsg);

		/*
		  Clean up the message
		*/
		ldb_ldif_read_free(ldb, ldifMsg);
	}

	/*
	  Clean up the context
	*/
	talloc_free(ldb);

	return 0;
}
