/*
  Copyright (c) 2010 Frank Lahm <franklahm@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

/*!
 * @file
 * Additional functions for bstrlib
 */

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <atalk/bstrlib.h>

/* Optionally include a mechanism for debugging memory */

#if defined(MEMORY_DEBUG) || defined(BSTRLIB_MEMORY_DEBUG)
#include "memdbg.h"
#endif

#ifndef bstr__alloc
#define bstr__alloc(x) malloc (x)
#endif

#ifndef bstr__free
#define bstr__free(p) free (p)
#endif

#ifndef bstr__realloc
#define bstr__realloc(p,x) realloc ((p), (x))
#endif

#ifndef bstr__memcpy
#define bstr__memcpy(d,s,l) memcpy ((d), (s), (l))
#endif

#ifndef bstr__memmove
#define bstr__memmove(d,s,l) memmove ((d), (s), (l))
#endif

#ifndef bstr__memset
#define bstr__memset(d,c,l) memset ((d), (c), (l))
#endif

#ifndef bstr__memcmp
#define bstr__memcmp(d,c,l) memcmp ((d), (c), (l))
#endif

#ifndef bstr__memchr
#define bstr__memchr(s,c,l) memchr ((s), (c), (l))
#endif

/*************************************************************************
 * Stuff for making bstrings referencing c-strings
 ************************************************************************/

/*!
 * @brief Create a bstring referencing "str"
 *
 * This is usefull if a well know code path uses string, often doing strlen on string.
 * By converting to bstring which carries the strlen, the repeated computation can be avoided.
 */
bstring brefcstr (char *str) {
    bstring b;
    size_t j;

	if (str == NULL)
        return NULL;
	j = strlen(str);

	b = (bstring)bstr__alloc(sizeof(struct tagbstring));
	if (NULL == b)
        return NULL;

	b->slen = (int) j;
    b->mlen = -1;
    b->data = (unsigned char *)str;

	return b;
}

/*!
 * @brief Free up the bstring, WITHOUT freeing the pointed to c-string!
 */
int bunrefcstr (bstring b) {
	if (b == NULL || b->slen < 0 || b->mlen > 0 || b->data == NULL)
		return BSTR_ERR;

	/* In case there is any stale usage, there is one more chance to 
	   notice this error. */

	b->slen = -1;
	b->mlen = -__LINE__;
	b->data = NULL;

	bstr__free (b);
	return BSTR_OK;
}

/*************************************************************************
 * stuff for bstrList
 ************************************************************************/

/*!
 * @brief Create an empty list with preallocated storage for at least 'min' members
 */
struct bstrList *bstrListCreateMin(int min)
{
    struct bstrList *sl = NULL;

    if ((sl = bstrListCreate()) == NULL)
        return NULL;

    if ((bstrListAlloc(sl, min)) != BSTR_OK) {
        bstrListDestroy(sl);
        return NULL;
    }

    return sl;
}

/*!
 * @brief Push a bstring to the end of a list
 */
int bstrListPush(struct bstrList *sl, bstring bs)
{
    if (sl->qty == sl->mlen) {
        if ((bstrListAlloc(sl, sl->qty + 1)) != BSTR_OK)
            return BSTR_ERR;
    }

    sl->entry[sl->qty] = bs;
    sl->qty++;
    return BSTR_OK;
}

/*!
 * @brief Pop a bstring from the end of a list
 */
bstring bstrListPop(struct bstrList *sl)
{
    return NULL;
}

/*!
 * @brief Inverse bjoin
 */
bstring bjoinInv(const struct bstrList * bl, const_bstring sep) {
    bstring b;
    int i, j, c, v;

    if (bl == NULL || bl->qty < 0)
        return NULL;
    if (sep != NULL && (sep->slen < 0 || sep->data == NULL))
        return NULL;

    for (i = 0, c = 1; i < bl->qty; i++) {
        v = bl->entry[i]->slen;
        if (v < 0)
            return NULL;/* Invalid input */
        c += v;
        if (c < 0)
            return NULL;/* Wrap around ?? */
    }

    if (sep != NULL)
        c += (bl->qty - 1) * sep->slen;

    b = (bstring) bstr__alloc (sizeof (struct tagbstring));
    if (NULL == b)
        return NULL; /* Out of memory */
    b->data = (unsigned char *) bstr__alloc (c);
    if (b->data == NULL) {
        bstr__free (b);
        return NULL;
    }

    b->mlen = c;
    b->slen = c-1;

    for (i = bl->qty - 1, c = 0, j = 0; i >= 0; i--, j++) {
        if (j > 0 && sep != NULL) {
            bstr__memcpy (b->data + c, sep->data, sep->slen);
            c += sep->slen;
        }
        v = bl->entry[i]->slen;
        bstr__memcpy (b->data + c, bl->entry[i]->data, v);
        c += v;
    }
    b->data[c] = (unsigned char) '\0';
    return b;
}
