/* The IGEN simulator generator for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney.

   This file is part of GDB.

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



/* Frustrating header junk */

#include "config.h"


enum
{
  default_insn_bit_size = 32,
  max_insn_bit_size = 64,
};


/* Define a 64bit data type */

#if defined __GNUC__ || defined _WIN32
#ifdef __GNUC__

typedef long long signed64;
typedef unsigned long long unsigned64;

#else /* _WIN32 */

typedef __int64 signed64;
typedef unsigned __int64 unsigned64;

#endif /* _WIN32 */
#else /* Not GNUC or WIN32 */
/* Not supported */
#endif


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



#include "filter_host.h"

typedef struct _line_ref line_ref;
struct _line_ref
{
  const char *file_name;
  int line_nr;
};

/* Error appends a new line, warning and notify do not */
typedef void error_func (const line_ref *line, char *msg, ...);

extern error_func error;
extern error_func warning;
extern error_func notify;


#define ERROR(EXPRESSION) \
do { \
  line_ref line; \
  line.file_name = filter_filename (__FILE__); \
  line.line_nr = __LINE__; \
  error (&line, EXPRESSION); \
} while (0)

#define ASSERT(EXPRESSION) \
do { \
  if (!(EXPRESSION)) { \
    line_ref line; \
    line.file_name = filter_filename (__FILE__); \
    line.line_nr = __LINE__; \
    error(&line, "assertion failed - %s\n", #EXPRESSION); \
  } \
} while (0)

#define ZALLOC(TYPE) ((TYPE*) zalloc (sizeof(TYPE)))
#define NZALLOC(TYPE,N) ((TYPE*) zalloc (sizeof(TYPE) * (N)))
#if 0
#define STRDUP(STRING) (strcpy (zalloc (strlen (STRING) + 1), (STRING)))
#define STRNDUP(STRING,LEN) (strncpy (zalloc ((LEN) + 1), (STRING), (LEN)))
#endif

extern void *zalloc (long size);

extern unsigned target_a2i (int ms_bit_nr, const char *a);

extern unsigned i2target (int ms_bit_nr, unsigned bit);

extern unsigned long long a2i (const char *a);


/* Try looking for name in the map table (returning the corresponding
   integer value).

   If the the sentinal (NAME == NULL) its value if >= zero is returned
   as the default. */

typedef struct _name_map
{
  const char *name;
  int i;
}
name_map;

extern int name2i (const char *name, const name_map * map);

extern const char *i2name (const int i, const name_map * map);
