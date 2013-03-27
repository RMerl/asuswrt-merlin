/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


/* Frustrating header junk */

#include "config.h"

#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if !defined (__attribute__) && (!defined(__GNUC__) || __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7))
#define __attribute__(arg)
#endif



#include "filter_filename.h"

extern void error
(char *msg, ...);

#define ASSERT(EXPRESSION) \
do { \
  if (!(EXPRESSION)) { \
    error("%s:%d: assertion failed - %s\n", \
	  filter_filename (__FILE__), __LINE__, #EXPRESSION); \
  } \
} while (0)

#define ZALLOC(TYPE) (TYPE*)zalloc(sizeof(TYPE))
#define NZALLOC(TYPE,N) ((TYPE*) zalloc (sizeof(TYPE) * (N)))

extern void *zalloc
(long size);

extern void dumpf
(int indent, char *msg, ...);

extern unsigned target_a2i
(int ms_bit_nr,
 const char *a);

extern unsigned i2target
(int ms_bit_nr,
 unsigned bit);

extern unsigned a2i
(const char *a);

/* Try looking for name in the map table (returning the corresponding
   integer value).  If that fails, try converting the name into an
   integer */

typedef struct _name_map {
  const char *name;
  int i;
} name_map;

extern int name2i
(const char *name,
 const name_map *map);

extern const char *i2name
(const int i,
 const name_map *map);
