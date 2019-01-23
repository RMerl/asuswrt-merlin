/*
   Unix SMB/CIFS implementation.

   security descriptor utility functions

   Copyright (C) Andrew Tridgell 		2004
   Copyright (C) Andrew Bartlett 		2010
   Copyright (C) Stefan Metzmacher 		2005

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

#ifndef _LIBCLI_SECURITY_SECURITY_TOKEN_H_
#define _LIBCLI_SECURITY_SECURITY_TOKEN_H_

#include "librpc/gen_ndr/security.h"

#define PRIMARY_USER_SID_INDEX 0
#define PRIMARY_GROUP_SID_INDEX 1

/*
  return a blank security token
*/
struct security_token *security_token_initialise(TALLOC_CTX *mem_ctx);

/****************************************************************************
 prints a struct security_token to debug output.
****************************************************************************/
void security_token_debug(int dbg_class, int dbg_lev, const struct security_token *token);

bool security_token_is_sid(const struct security_token *token, const struct dom_sid *sid);

bool security_token_is_sid_string(const struct security_token *token, const char *sid_string);

bool security_token_is_system(const struct security_token *token);

bool security_token_is_anonymous(const struct security_token *token);

bool security_token_has_sid(const struct security_token *token, const struct dom_sid *sid);

bool security_token_has_sid_string(const struct security_token *token, const char *sid_string);

bool security_token_has_builtin_administrators(const struct security_token *token);

bool security_token_has_nt_authenticated_users(const struct security_token *token);

bool security_token_has_enterprise_dcs(const struct security_token *token);

#endif
