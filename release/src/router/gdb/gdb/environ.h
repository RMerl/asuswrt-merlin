/* Header for environment manipulation library.
   Copyright (C) 1989, 1992, 2000, 2005, 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#if !defined (ENVIRON_H)
#define ENVIRON_H 1

/* We manipulate environments represented as these structures.  */

struct gdb_environ
  {
    /* Number of usable slots allocated in VECTOR.
       VECTOR always has one slot not counted here,
       to hold the terminating zero.  */
    int allocated;
    /* A vector of slots, ALLOCATED + 1 of them.
       The first few slots contain strings "VAR=VALUE"
       and the next one contains zero.
       Then come some unused slots.  */
    char **vector;
  };

extern struct gdb_environ *make_environ (void);

extern void free_environ (struct gdb_environ *);

extern void init_environ (struct gdb_environ *);

extern char *get_in_environ (const struct gdb_environ *, const char *);

extern void set_in_environ (struct gdb_environ *, const char *, const char *);

extern void unset_in_environ (struct gdb_environ *, char *);

extern char **environ_vector (struct gdb_environ *);

#endif /* defined (ENVIRON_H) */
