 /* 
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell              1999-2005
   Copyright (C) Paul `Rusty' Russell		   2000
   Copyright (C) Jeremy Allison			   2000-2003
   
     ** NOTE! The following LGPL license applies to the tdb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "tdb_private.h"

enum TDB_ERROR tdb_error(struct tdb_context *tdb)
{
	return tdb->ecode;
}

static struct tdb_errname {
	enum TDB_ERROR ecode; const char *estring;
} emap[] = { {TDB_SUCCESS, "Success"},
	     {TDB_ERR_CORRUPT, "Corrupt database"},
	     {TDB_ERR_IO, "IO Error"},
	     {TDB_ERR_LOCK, "Locking error"},
	     {TDB_ERR_OOM, "Out of memory"},
	     {TDB_ERR_EXISTS, "Record exists"},
	     {TDB_ERR_NOLOCK, "Lock exists on other keys"},
	     {TDB_ERR_EINVAL, "Invalid parameter"},
	     {TDB_ERR_NOEXIST, "Record does not exist"},
	     {TDB_ERR_RDONLY, "write not permitted"} };

/* Error string for the last tdb error */
const char *tdb_errorstr(struct tdb_context *tdb)
{
	u32 i;
	for (i = 0; i < sizeof(emap) / sizeof(struct tdb_errname); i++)
		if (tdb->ecode == emap[i].ecode)
			return emap[i].estring;
	return "Invalid error code";
}

