/*
   Unix SMB/CIFS implementation.
   Name mangling interface
   Copyright (C) Andrew Tridgell 2002

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

#ifndef _MANGLE_H_
#define _MANGLE_H_
/*
  header for 8.3 name mangling interface 
*/

struct mangle_fns {
	void (*reset)(void);
	bool (*is_mangled)(const char *s, const struct share_params *p);
	bool (*must_mangle)(const char *s, const struct share_params *p);
	bool (*is_8_3)(const char *fname, bool check_case, bool allow_wildcards,
		       const struct share_params *p);
	bool (*lookup_name_from_8_3)(TALLOC_CTX *ctx,
				const char *in,
				char **out, /* talloced on the given context. */
				const struct share_params *p);
	bool (*name_to_8_3)(const char *in,
			char out[13],
			bool cache83,
			int default_case,
			const struct share_params *p);
};
#endif /* _MANGLE_H_ */
