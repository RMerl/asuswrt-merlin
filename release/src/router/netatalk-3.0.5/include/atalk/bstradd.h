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
 * Additional functions for bstrlib.
 */

#ifndef ATALK_BSTRADD_H
#define ATALK_BSTRADD_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <atalk/bstrlib.h>

#define cfrombstr(b) ((char *)((b)->data))

/* strip slashes from end of a bstring */
#define BSTRING_STRIP_SLASH(a)                      \
    do {                                            \
        while (bchar((a), blength(a) - 1) == '/')   \
            bdelete((a), blength(a) - 1, 1);        \
    } while (0);

typedef struct tagbstring static_bstring;

extern bstring brefcstr(const char *str);
extern int bunrefcstr(bstring b);

extern struct bstrList *bstrListCreateMin(int min);
extern int bstrListPush(struct bstrList *sl, bstring bs);
extern bstring bstrListPop(struct bstrList *sl);
extern bstring bjoinInv(const struct bstrList * bl, const_bstring sep);
#endif /* ATALK_BSTRADD_H */
