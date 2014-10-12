/* 
   Unix SMB/CIFS implementation.

   database wrap headers

   Copyright (C) Andrew Tridgell 2004
   
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

/* IMPORTANT: tdb_wrap should be always preferred over tdb_context for end consumer functions
   it's because if the code will be running inside smbd, then we must use the linked list
   of open tdb files, to determine if the tdb we desire is already open
   as otherwise, when you close the tdb (even on a different file descriptor),
   ALL LOCKS are lost (due to a real screwup in the POSIX specification that nobody has been able to get fixed)
*/

#ifndef _TDB_WRAP_H_
#define _TDB_WRAP_H_

#include <tdb.h>

struct tdb_wrap {
	struct tdb_context *tdb;
};

struct tdb_wrap *tdb_wrap_open(TALLOC_CTX *mem_ctx,
			       const char *name, int hash_size, int tdb_flags,
			       int open_flags, mode_t mode);

#endif /* _TDB_WRAP_H_ */
