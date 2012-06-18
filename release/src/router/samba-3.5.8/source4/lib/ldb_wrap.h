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

#ifndef _LDB_WRAP_H_
#define _LDB_WRAP_H_

struct auth_session_info;
struct ldb_message;
struct ldb_dn;
struct cli_credentials;
struct loadparm_context;
struct tevent_context;

char *wrap_casefold(void *context, void *mem_ctx, const char *s, size_t n);

struct ldb_context *ldb_wrap_connect(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct loadparm_context *lp_ctx,
				     const char *url,
				     struct auth_session_info *session_info,
				     struct cli_credentials *credentials,
				     unsigned int flags,
				     const char *options[]);

#endif /* _LDB_WRAP_H_ */
