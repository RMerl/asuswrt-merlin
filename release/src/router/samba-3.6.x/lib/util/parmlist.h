/* 
   Unix SMB/CIFS implementation.
   Generic parameter parsing interface
   Copyright (C) Jelmer Vernooij					  2009
   
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

#ifndef _PARMLIST_H /* _PARMLIST_H */
#define _PARMLIST_H 

struct parmlist_entry {
	struct parmlist_entry *prev, *next;
	char *key;
	char *value;
	int priority;
};

struct parmlist {
	struct parmlist_entry *entries;
};

/** Retrieve an integer from a parameter list. If not found, return default_v. */
int parmlist_get_int(struct parmlist *ctx, const char *name, int default_v);

/** Retrieve a string from a parameter list. If not found, return default_v. */
const char *parmlist_get_string(struct parmlist *ctx, const char *name, 
								const char *default_v);

/** Retrieve the struct for an entry in a parmlist. */
struct parmlist_entry *parmlist_get(struct parmlist *ctx, const char *name);

/** Retrieve a string list from a parameter list. 
 * separator can contain characters to consider separators or can be 
 * NULL for the default set. */
const char **parmlist_get_string_list(struct parmlist *ctx, const char *name, 
									  const char *separator);

/** Retrieve boolean from a parameter list. If not set, return default_v. */
bool parmlist_get_bool(struct parmlist *ctx, const char *name, bool default_v);

/** Set a parameter. */
int parmlist_set_string(struct parmlist *ctx, const char *name, const char *value);

#endif /* _PARMLIST_H */
