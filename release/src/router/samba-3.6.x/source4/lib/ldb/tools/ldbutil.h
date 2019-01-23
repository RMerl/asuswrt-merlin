/*
   ldb database library utility header file

   Copyright (C) Matthieu Patou 2009

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
 *  Description: Common function used by ldb_add/ldb_modify/ldb_delete
 *
 *  Author: Matthieu Patou
 */

#include "ldb.h"

int ldb_add_ctrl(struct ldb_context *ldb,
		const struct ldb_message *message,
		struct ldb_control **controls);
int ldb_delete_ctrl(struct ldb_context *ldb, struct ldb_dn *dn,
		struct ldb_control **controls);
int ldb_modify_ctrl(struct ldb_context *ldb,
                const struct ldb_message *message,
                struct ldb_control **controls);
int ldb_search_ctrl(struct ldb_context *ldb, TALLOC_CTX *mem_ctx,
		    struct ldb_result **result, struct ldb_dn *base,
		    enum ldb_scope scope, const char * const *attrs,
		    struct ldb_control **controls,
		    const char *exp_fmt, ...);
