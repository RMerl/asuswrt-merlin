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

/** \example ldbreader.c

The code below shows a simple LDB application.

It lists / dumps the records in a LDB database to standard output.

*/

#include "ldb.h"

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
	const char *expression = "(dn=*)";
	struct ldb_result *resultMsg;
	int i;

	/*
	  This is the always the first thing you want to do in an LDB
	  application - initialise up the context structure.

	  Note that you can use the context structure as a parent
	  for talloc allocations as well
	*/
	ldb = ldb_init(NULL, NULL);

	/*
	  We now open the database. In this example we just hard code the connection path.

	  Also note that the database is being opened read-only. This means that the 
	  call will fail unless the database already exists. 
	*/
	if (LDB_SUCCESS != ldb_connect(ldb, "tdb://tdbtest.ldb", LDB_FLG_RDONLY, NULL) ){
		printf("Problem on connection\n");
		exit(-1);
	}

	/*
	  At this stage we have an open database, and can start using it. It is opened
	  read-only, so a query is possible. 

	  We construct a search that just returns all the (sensible) contents. You can do
	  quite fine grained results with the LDAP search syntax, however it is a bit
	  confusing to start with. See RFC2254.
	*/
	if (LDB_SUCCESS != ldb_search(ldb, ldb, &resultMsg,
				      NULL, LDB_SCOPE_DEFAULT, NULL,
				      "%s", expression)) {
		printf("Problem in search\n");
		exit(-1);
	}
	
	printf("%i records returned\n", resultMsg->count);

	/*
	  We can now iterate through the results, writing them out
	  (to standard output) with our custom output routine as defined
	  at the top of this file
	*/
	for (i = 0; i < resultMsg->count; ++i) {
		struct ldb_ldif ldifMsg;

		printf("Message: %i\n", i+1);
		
		ldifMsg.changetype = LDB_CHANGETYPE_NONE;
		ldifMsg.msg = resultMsg->msgs[i];
		ldb_ldif_write(ldb, vprintf_fn, NULL, &ldifMsg);
	}

	/*
	  There are two objects to clean up - the result from the 
	  ldb_search() query, and the original ldb context.
	*/
	talloc_free(resultMsg);

	talloc_free(ldb);

	return 0;
}
