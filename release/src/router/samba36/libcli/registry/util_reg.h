/*
 * Unix SMB/CIFS implementation.
 * Registry helper routines
 * Copyright (C) Volker Lendecke 2006
 * Copyright (C) Guenther Deschner 2009
 * Copyright (C) Jelmer Vernooij 2003-2007
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LIBCLI_REGISTRY_UTIL_REG_H__
#define __LIBCLI_REGISTRY_UTIL_REG_H__

const char *str_regtype(int type);
int regtype_by_string(const char *str);
bool push_reg_sz(TALLOC_CTX *mem_ctx, DATA_BLOB *blob, const char *s);
bool push_reg_multi_sz(TALLOC_CTX *mem_ctx, DATA_BLOB *blob, const char **a);
bool pull_reg_sz(TALLOC_CTX *mem_ctx, const DATA_BLOB *blob, const char **s);
bool pull_reg_multi_sz(TALLOC_CTX *mem_ctx, const DATA_BLOB *blob, const char ***a);

#endif /* __LIBCLI_REGISTRY_UTIL_REG_H__ */
