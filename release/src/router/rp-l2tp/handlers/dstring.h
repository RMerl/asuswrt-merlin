/***********************************************************************
*
* l2tp/handlers/dstring.h
*
* Declares simple buffer which grows to accomodate accumulated string
* data
*
* Copyright (C) 2002 by Roaring Penguin Software Inc.
*
* LIC: GPL
*
***********************************************************************/

#include <stdlib.h>

typedef struct {
    size_t alloc_size;
    size_t actual_size;
    char *data;
} dynstring;

int dynstring_init(dynstring *str);
void dynstring_free(dynstring *str);
int dynstring_append(dynstring *str, char const *s2);
int dynstring_append_len(dynstring *str, char const *s2, size_t len);
char const *dynstring_data(dynstring *str);
