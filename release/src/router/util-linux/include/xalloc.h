/*
 * Copyright (C) 2010 Davidlohr Bueso <dave@gnu.org>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * General memory allocation wrappers for malloc, realloc, calloc and strdup
 */

#ifndef UTIL_LINUX_XALLOC_H
#define UTIL_LINUX_XALLOC_H

#include <stdlib.h>
#include <string.h>

#include "c.h"

#ifndef XALLOC_EXIT_CODE
# define XALLOC_EXIT_CODE EXIT_FAILURE
#endif

static inline __ul_alloc_size(1)
void *xmalloc(const size_t size)
{
        void *ret = malloc(size);

        if (!ret && size)
                err(XALLOC_EXIT_CODE, "cannot allocate %zu bytes", size);
        return ret;
}

static inline __ul_alloc_size(2)
void *xrealloc(void *ptr, const size_t size)
{
        void *ret = realloc(ptr, size);

        if (!ret && size)
                err(XALLOC_EXIT_CODE, "cannot allocate %zu bytes", size);
        return ret;
}

static inline __ul_calloc_size(1, 2)
void *xcalloc(const size_t nelems, const size_t size)
{
        void *ret = calloc(nelems, size);

        if (!ret && size && nelems)
                err(XALLOC_EXIT_CODE, "cannot allocate %zu bytes", size);
        return ret;
}

static inline char *xstrdup(const char *str)
{
        char *ret;

        if (!str)
                return NULL;

        ret = strdup(str);

        if (!ret)
                err(XALLOC_EXIT_CODE, "cannot duplicate string");
        return ret;
}

#endif
