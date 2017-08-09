/*
   Unix SMB/CIFS implementation.

   common share info functions

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Tim Potter 2004

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


#ifndef __LIB_UTIL_UTIL_LDB_H__
#define __LIB_UTIL_UTIL_LDB_H__

struct ldb_dn;

/* The following definitions come from lib/util/util_ldb.c  */

int gendb_search_v(struct ldb_context *ldb,
		   TALLOC_CTX *mem_ctx,
		   struct ldb_dn *basedn,
		   struct ldb_message ***msgs,
		   const char * const *attrs,
		   const char *format,
		   va_list ap)  PRINTF_ATTRIBUTE(6,0);
int gendb_search(struct ldb_context *ldb,
		 TALLOC_CTX *mem_ctx,
		 struct ldb_dn *basedn,
		 struct ldb_message ***res,
		 const char * const *attrs,
		 const char *format, ...) PRINTF_ATTRIBUTE(6,7);
int gendb_search_dn(struct ldb_context *ldb,
		 TALLOC_CTX *mem_ctx,
		 struct ldb_dn *dn,
		 struct ldb_message ***res,
		 const char * const *attrs);

#endif /* __LIB_UTIL_UTIL_LDB_H__ */
