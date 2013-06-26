/* 
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer (utility functions)
 *  Copyright (C) Gerald Carter                     2002-2005
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

#ifndef _REG_UTIL_H
#define _REG_UTIL_H

bool reg_split_path(char *path, char **base, char **new_path);
bool reg_split_key(char *path, char **base, char **key);
char *normalize_reg_path(TALLOC_CTX *ctx, const char *keyname );
char *reg_remaining_path(TALLOC_CTX *ctx, const char *key);

#endif /* _REG_UTIL_H */
