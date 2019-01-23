/* Hash table declarations.
   Copyright (C) 2000, 2007, 2008, 2009, 2010, 2011, 2015 Free Software
   Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

#ifndef HASH_H
#define HASH_H

struct hash_table;

struct hash_table *hash_table_new (int, unsigned long (*) (const void *),
                                   int (*) (const void *, const void *));
void hash_table_destroy (struct hash_table *);

void *hash_table_get (const struct hash_table *, const void *);
int hash_table_get_pair (const struct hash_table *, const void *,
                         void *, void *);
int hash_table_contains (const struct hash_table *, const void *);

void hash_table_put (struct hash_table *, const void *, const void *);
int hash_table_remove (struct hash_table *, const void *);
void hash_table_clear (struct hash_table *);

void hash_table_for_each (struct hash_table *,
                          int (*) (void *, void *, void *), void *);

typedef struct {
  void *key, *value;    /* public members */
  void *pos, *end;      /* private members */
} hash_table_iterator;
void hash_table_iterate (struct hash_table *, hash_table_iterator *);
int hash_table_iter_next (hash_table_iterator *);

int hash_table_count (const struct hash_table *);

struct hash_table *make_string_hash_table (int);
struct hash_table *make_nocase_string_hash_table (int);

unsigned long hash_pointer (const void *);

#endif /* HASH_H */
