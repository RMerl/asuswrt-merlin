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

#include <talloc.h>

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
				     unsigned int flags);

void ldb_wrap_fork_hook(void);

struct ldb_context *samba_ldb_init(TALLOC_CTX *mem_ctx,
								  struct tevent_context *ev,
								  struct loadparm_context *lp_ctx,
								  struct auth_session_info *session_info,
								  struct cli_credentials *credentials);
struct ldb_context *ldb_wrap_find(const char *url,
								  struct tevent_context *ev,
								  struct loadparm_context *lp_ctx,
								  struct auth_session_info *session_info,
								  struct cli_credentials *credentials,
								  int flags);
bool ldb_wrap_add(const char *url, struct tevent_context *ev,
				  struct loadparm_context *lp_ctx,
				  struct auth_session_info *session_info,
				  struct cli_credentials *credentials,
				  int flags,
				  struct ldb_context *ldb);
char *ldb_relative_path(struct ldb_context *ldb,
				 TALLOC_CTX *mem_ctx,
				 const char *name);

int samba_ldb_connect(struct ldb_context *ldb, struct loadparm_context *lp_ctx,
		      const char *url, int flags);

#endif /* _LDB_WRAP_H_ */
