/*
 * Samba Unix/Linux SMB client library
 * Distributed SMB/CIFS Server Management Utility
 * registry utility functions
 *
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NET_REGISTRY_UTIL_H__
#define __NET_REGISTRY_UTIL_H__

void print_registry_key(const char *keyname, NTTIME *modtime);

void print_registry_value(const struct registry_value *valvalue, bool raw);

void print_registry_value_with_name(const char *valname,
				    const struct registry_value *valvalue);

/**
 * Split path into hive name and subkeyname
 * normalizations performed:
 *  - convert '/' to '\\'
 *  - strip trailing '\\' chars
 */
WERROR split_hive_key(TALLOC_CTX *ctx, const char *path, char **hivename,
		      char **subkeyname);

#endif
