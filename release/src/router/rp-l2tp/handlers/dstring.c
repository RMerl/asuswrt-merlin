/***********************************************************************
*
* l2tp/handlers/dstring.c
*
* Implements simple buffer which grows to accomodate accumulated string
* data
*
* Copyright (C) 2002 by Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: dstring.c 3323 2011-09-21 18:45:48Z lly.dev $";

#define INITIAL_SIZE 128
#define GROW_FACTOR 2

#include "dstring.h"
#include <stdlib.h>
#include <string.h>

static int
expand_for_size(dynstring *str, size_t len)
{
    size_t newlen;

    if (len >= str->alloc_size * GROW_FACTOR) {
	newlen = len + 1;
    } else {
	newlen = str->alloc_size * GROW_FACTOR;
    }

    str->data = realloc(str->data, newlen);
    if (!str->data) {
	str->alloc_size = 0;
	str->actual_size = 0;
	return -1;
    }

    str->alloc_size = newlen;
    return 0;
}

/**********************************************************************
* %FUNCTION: dynstring_init
* %ARGUMENTS:
*  str -- a dynstring structure
* %RETURNS:
*  0 on success; -1 on failure
* %DESCRIPTION:
*  Initializes dynamic string
***********************************************************************/
int
dynstring_init(dynstring *str)
{
    str->data = malloc(INITIAL_SIZE);
    if (!str->data) return -1;

    str->alloc_size = INITIAL_SIZE;
    str->actual_size = 0;
    str->data[0] = 0;
    return 0;
}

/**********************************************************************
* %FUNCTION: dynstring_free
* %ARGUMENTS:
*  str -- a dynstring structure
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Frees resources used by dynamic string
***********************************************************************/
void
dynstring_free(dynstring *str)
{
    if (str->data) {
	free(str->data);
	str->data = NULL;
    }
    str->alloc_size = 0;
    str->actual_size = 0;
}

/**********************************************************************
* %FUNCTION: dynstring_append
* %ARGUMENTS:
*  str -- dynamic string
*  s2 -- string to append
* %RETURNS:
*  -1 on failure; 0 on success
* %DESCRIPTION:
*  Appends s2 to str
***********************************************************************/
int
dynstring_append(dynstring *str, char const *s2)
{
    return dynstring_append_len(str, s2, strlen(s2));
}

/**********************************************************************
* %FUNCTION: dynstring_append_len
* %ARGUMENTS:
*  str -- dynamic string
*  s2 -- string to append
*  len -- length of s2
* %RETURNS:
*  -1 on failure; 0 on success
* %DESCRIPTION:
*  Appends s2 to str
***********************************************************************/
int
dynstring_append_len(dynstring *str, char const *s2, size_t len)
{
    if (!len) return 0;
    if (!str->data) return -1;

    if (len + str->actual_size >= str->alloc_size) {
	if (expand_for_size(str, str->actual_size + len) < 0) {
	    return -1;
	}
    }

    memcpy(str->data + str->actual_size, s2, len);
    str->actual_size += len;
    str->data[str->actual_size] = 0;
    return 0;
}

char const *
dynstring_data(dynstring *str)
{
    return str->data;
}
