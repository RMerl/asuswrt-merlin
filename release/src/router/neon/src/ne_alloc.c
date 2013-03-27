/* 
   Replacement memory allocation handling etc.
   Copyright (C) 1999-2005, Joe Orton <joe@manyfish.co.uk>

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

#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <stdio.h>

#include "ne_alloc.h"

static ne_oom_callback_fn oom;

void ne_oom_callback(ne_oom_callback_fn callback)
{
    oom = callback;
}

#ifndef NEON_MEMLEAK

#define DO_MALLOC(ptr, len) do {		\
    ptr = malloc((len));			\
    if (!ptr) {					\
	if (oom != NULL)			\
	    oom();				\
	abort();				\
    }						\
} while(0);

void *ne_malloc(size_t len) 
{
    void *ptr;
    DO_MALLOC(ptr, len);
    return ptr;
}

void *ne_calloc(size_t len) 
{
    void *ptr;
    DO_MALLOC(ptr, len);
    return memset(ptr, 0, len);
}

void *ne_realloc(void *ptr, size_t len)
{
    void *ret = realloc(ptr, len);
    if (!ret) {
	if (oom)
	    oom();
	abort();
    }
    return ret;
}

#ifdef WIN32
/* Implemented only to ensure free is bound to the correct DLL. */
void ne_free(void *ptr)
{
    free(ptr);
}
#endif

char *ne_strdup(const char *s) 
{
    char *ret;
    DO_MALLOC(ret, strlen(s) + 1);
    return strcpy(ret, s);
}

char *ne_strndup(const char *s, size_t n)
{
    char *new;
    DO_MALLOC(new, n+1);
    new[n] = '\0';
    memcpy(new, s, n);
    return new;
}

#else /* NEON_MEMLEAK */

#include <assert.h>

/* Memory-leak detection implementation: ne_malloc and friends are
 * #defined to ne_malloc_ml etc by memleak.h, which is conditionally
 * included by config.h. */

/* memory allocated be ne_*alloc, but not freed. */
size_t ne_alloc_used = 0;

static struct block {
    void *ptr;
    size_t len;
    const char *file;
    int line;
    struct block *next;
} *blocks = NULL;

void ne_alloc_dump(FILE *f)
{
    struct block *b;

    for (b = blocks; b != NULL; b = b->next)
        fprintf(f, "%" NE_FMT_SIZE_T "b@%s:%d%s", b->len, b->file, b->line,
                b->next?", ":"");
}

static void *tracking_malloc(size_t len, const char *file, int line)
{
    void *ptr = malloc((len));
    struct block *block;

    if (!ptr) {
	if (oom) oom();
	abort();
    }
    
    block = malloc(sizeof *block);
    if (block != NULL) {
        block->ptr = ptr;
        block->len = len;
        block->file = file;
        block->line = line;
        block->next = blocks;
        blocks = block;
        ne_alloc_used += len;
    }

    return ptr;
}

void *ne_malloc_ml(size_t size, const char *file, int line)
{
    return tracking_malloc(size, file, line);
}

void *ne_calloc_ml(size_t size, const char *file, int line)
{
    return memset(tracking_malloc(size, file, line), 0, size);
}

void *ne_realloc_ml(void *ptr, size_t s, const char *file, int line)
{
    void *ret;
    struct block *b;

    if (ptr == NULL)
        return tracking_malloc(s, file, line);

    ret = realloc(ptr, s);
    if (!ret) {
        if (oom) oom();
        abort();
    }
    
    for (b = blocks; b != NULL; b = b->next) {
        if (b->ptr == ptr) {
            ne_alloc_used += s - b->len;
            b->ptr = ret;
            b->len = s;
            break;
        }
    }
    assert(b != NULL);

    return ret;
}

char *ne_strdup_ml(const char *s, const char *file, int line)
{
    return strcpy(tracking_malloc(strlen(s) + 1, file, line), s);
}

char *ne_strndup_ml(const char *s, size_t n, const char *file, int line)
{
    char *ret = tracking_malloc(n + 1, file, line);
    ret[n] = '\0';
    return memcpy(ret, s, n);
}

void ne_free_ml(void *ptr)
{
    struct block *b, *last = NULL;

    for (b = blocks; b != NULL; last = b, b = b->next) {
        if (b->ptr == ptr) {
            ne_alloc_used -= b->len;
            if (last) 
                last->next = b->next;
            else
                blocks = b->next;
            free(b);
            break;
        }
    }

    free(ptr);
}

#endif /* NEON_MEMLEAK */
