/* 
   Memory leak wrappers
   Copyright (C) 2003, Joe Orton <joe@manyfish.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

/* WARNING: THIS IS AN INTERNAL NEON INTERFACE AND MUST NOT BE USED
 * from NEON APPLICATIONS. */

/* This file contains an alternate interface to the memory allocation
 * wrappers in ne_alloc.c, which perform simple leak detection.  It
 * MUST NOT BE INSTALLED, or used from neon applications. */

#ifndef MEMLEAK_H
#define MEMLEAK_H

#include <stdio.h>

#define ne_malloc(s) ne_malloc_ml(s, __FILE__, __LINE__)
#define ne_calloc(s) ne_calloc_ml(s, __FILE__, __LINE__)
#define ne_realloc(p, s) ne_realloc_ml(p, s, __FILE__, __LINE__)
#define ne_strdup(s) ne_strdup_ml(s, __FILE__, __LINE__)
#define ne_strndup(s, n) ne_strndup_ml(s, n, __FILE__, __LINE__)
#define ne_free ne_free_ml

/* Prototypes of allocation functions: */
void *ne_malloc_ml(size_t size, const char *file, int line);
void *ne_calloc_ml(size_t size, const char *file, int line);
void *ne_realloc_ml(void *ptr, size_t s, const char *file, int line);
char *ne_strdup_ml(const char *s, const char *file, int line);
char *ne_strndup_ml(const char *s, size_t n, const char *file, int line);
void ne_free_ml(void *ptr);

/* Dump the list of currently allocated blocks to 'f'. */
void ne_alloc_dump(FILE *f);

/* Current number of bytes in allocated but not free'd. */
extern size_t ne_alloc_used;

#endif /* MEMLEAK_H */
