/*
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *  Copyright (C) Volker Lendecke 2006
 *  Copyright (C) Michael Adam 2007-2010
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Higher level utility functions on top of reg_api.c
 */

#ifndef _REG_API_UTIL_H
#define _REG_API_UTIL_H

/**
 * Utility function to open a complete registry path including the hive prefix.
 */
WERROR reg_open_path(TALLOC_CTX *mem_ctx, const char *orig_path,
		     uint32 desired_access, const struct security_token *token,
		     struct registry_key **pkey);

#if 0
/* currently unused */
WERROR reg_create_path(TALLOC_CTX *mem_ctx, const char *orig_path,
		       uint32 desired_access,
		       const struct security_token *token,
		       enum winreg_CreateAction *paction,
		       struct registry_key **pkey);
WERROR reg_delete_path(const struct security_token *token,
		       const char *orig_path);
#endif

#endif /* _REG_API_UTIL_H */
