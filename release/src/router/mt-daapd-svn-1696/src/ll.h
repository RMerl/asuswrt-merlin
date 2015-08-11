/*
 * $Id: ll.h 1056 2006-05-18 05:11:07Z rpedde $
 * Rock stupid char* indexed linked lists
 *
 * Copyright (C) 2006 Ron Pedde (rpedde@users.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LL_H_
#define _LL_H_

#define LL_TYPE_INT     0
#define LL_TYPE_STRING  1
#define LL_TYPE_LL      2


#define LL_E_SUCCESS   0
#define LL_E_MALLOC    1
#define LL_E_NOKEY     2
#define LL_E_DUP       3
#define LL_E_BADPARAM  4

#define LL_FLAG_HONORCASE  1 /** Make keys case sensitive */
#define LL_FLAG_HEADINSERT 2 /** Insert at head, rather than tail */
#define LL_FLAG_NODUPS     4 /** Do not allow duplicates */
#define LL_FLAG_INHERIT    8 /** Set child flags based on parent flags */

typedef struct _LLITEM {
    int type;
    char *key;
    union {
        int as_int;
        char *as_string;
        struct _LL *as_ll;
    } value;
    struct _LLITEM *next;
} LL_ITEM, *LL_ITEMHANDLE;

typedef struct _LL {
    unsigned int flags;
    struct _LLITEM *tailptr;
    struct _LLITEM itemlist;
} LL, *LL_HANDLE;



extern int ll_create(LL **ppl);
extern int ll_destroy(LL *pl);

extern int ll_add_string(LL *pl, char *key, char *cval);
extern int ll_add_int(LL *pl, char *key, int ival);
extern int ll_add_ll(LL *pl, char *key, LL *pnew);

extern int ll_update_string(LL_ITEM *pli, char *cval);
extern int ll_update_int(LL_ITEM *pli, int ival);
extern int ll_update_ll(LL_ITEM *pli, LL *pnew);

extern int ll_set_flags(LL *pl, unsigned int flags);
extern int ll_get_flags(LL *pl, unsigned int *flags);

extern int ll_del_item(LL *pl, char *key);

extern LL_ITEM *ll_fetch_item(LL *pl, char *key);
extern LL_ITEM *ll_get_next(LL *pl, LL_ITEM *prev);

extern void ll_dump(LL *pl);

#endif /* _LL_H_ */

