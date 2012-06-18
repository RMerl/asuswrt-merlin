/*
 * $Id: ll.c 1484 2007-01-17 01:06:16Z rpedde $
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "daapd.h"
#include "ll.h"
#include "err.h"


/** Internal functions  */

int _ll_add_item(LL *pl, char *key, void *vpval, int ival, int type);
int _ll_update_item(LL_ITEM *pli, void *vpval, int ival, int type);
void _ll_dump(LL *pl, int depth);

/**
 * create a ll
 *
 * @param ppl pointer to a LL *.  Returns valid handle on LL_E_SUCCESS
 * @returns LL_E_SUCCESS on success, error code otherwise
 */
int ll_create(LL **ppl) {
    *ppl = (LL *)malloc(sizeof(struct _LL));
    if(!*ppl)
        return LL_E_MALLOC;

    memset(*ppl,0x0,sizeof(struct _LL));

    /* set default flags */
    (*ppl)->flags = LL_FLAG_NODUPS | LL_FLAG_INHERIT;
    return LL_E_SUCCESS;
}

/**
 * destroy a ll, recusively, if neccesary
 *
 * @param pl ll to destroy
 * @returns LL_E_SUCCESS
 */
int ll_destroy(LL *pl) {
    LL_ITEM *current,*last;

    last = &(pl->itemlist);
    current = pl->itemlist.next;

    while(current) {
        switch(current->type) {
        case LL_TYPE_LL:
            ll_destroy(current->value.as_ll);
            break;
        case LL_TYPE_STRING:
            free(current->value.as_string);
            break;
        default:
            break;
        }
        free(current->key);
        last = current;
        current = current->next;
        free(last);
    }

    free(pl);

    return LL_E_SUCCESS;
}

/**
 * thin wrapper for _ll_add_item
 */
int ll_add_string(LL *pl, char *key, char *cval) {
    return _ll_add_item(pl,key,cval,0,LL_TYPE_STRING);
}

/**
 * thin wrapper for _ll_add_item
 */
int ll_add_int(LL *pl, char *key, int ival) {
    return _ll_add_item(pl,key,NULL,ival,LL_TYPE_INT);
}

/**
 * thin wrapper for _ll_add_item
 */
int ll_add_ll(LL *pl, char *key, LL *pnew) {
    int result;

    result = _ll_add_item(pl,key,(void*)pnew,0,LL_TYPE_LL);
    if(result == LL_E_SUCCESS) {
        if(pl->flags & LL_FLAG_INHERIT) {
            pnew->flags = pl->flags;
        }
    }
    return result;
}

int ll_del_item(LL *pl, char *key) {
    LL_ITEM *phead, *ptail;

    ptail = &pl->itemlist;
    phead = pl->itemlist.next;

    while(phead) {
        if((pl->flags & LL_FLAG_HONORCASE) &&
           (strcmp(phead->key,key)==0))
            break;
        if((!(pl->flags & LL_FLAG_HONORCASE) &&
            (strcasecmp(phead->key,key)==0)))
            break;

        ptail=phead;
        phead=phead->next;
    }

    if(phead) {
        /* found the item... */
        if(pl->tailptr == phead)
            pl->tailptr = ptail;
        ptail->next = phead->next;
    } else {
        /* no matching item */
        return LL_E_NOKEY;
    }
    return LL_E_SUCCESS;
}

/**
 * add an item to a ll
 */
int _ll_add_item(LL *pl, char *key, void* vpval, int ival, int type) {
    LL_ITEM *pli;

    if(!key) {
        DPRINTF(E_LOG,L_MISC,"_ll_add_item: passed null key\n");
        return LL_E_BADPARAM;
    }

    if(pl->flags & LL_FLAG_NODUPS) {
        if(ll_fetch_item(pl,key))
            return LL_E_DUP;
    }

    pli=(LL_ITEM *)malloc(sizeof(LL_ITEM));
    if(!pli) {
        return LL_E_MALLOC;
    }

    pli->type = type;
    pli->key = strdup(key);

    switch(type) {
    case LL_TYPE_INT:
        pli->value.as_int = ival;
        break;
    case LL_TYPE_LL:
        pli->value.as_ll = (LL *)vpval;
        break;
    case LL_TYPE_STRING:
        if(!vpval) {
            DPRINTF(E_LOG,L_MISC,"_ll_add_item: passed null value\n");
            free(pli);
            return LL_E_BADPARAM;
        }
        pli->value.as_string = strdup((char*)vpval);
        break;
    default:
        break;
    }

    if(pl->flags & LL_FLAG_HEADINSERT) {
        pli->next = pl->itemlist.next;
        pl->itemlist.next = pli;
    } else {
        pli->next = NULL;
        if(pl->tailptr) {
            pl->tailptr->next = pli;
        } else {
            pl->itemlist.next = pli;
        }
        pl->tailptr = pli;
    }

    return LL_E_SUCCESS;
}

/**
 * thin wrapper for _ll_update_item
 */
int ll_update_string(LL_ITEM *pli, char *cval) {
    return _ll_update_item(pli,cval,0,LL_TYPE_STRING);
}

/**
 * thin wrapper for _ll_update_item
 */
int ll_update_int(LL_ITEM *pli, int ival) {
    return _ll_update_item(pli,NULL,ival,LL_TYPE_INT);
}

/**
 * thin wrapper for _ll_update_item.
 *
 * NOTE: There is a reasonable case to be made about
 * what should happen to flags on an update.  We'll just
 * leave that to the caller, though.
 */
int ll_update_ll(LL_ITEM *pli, LL *pnew) {
    int result;

    result = _ll_update_item(pli,(void*)pnew,0,LL_TYPE_LL);
    return result;
}


/**
 * update an item, given an item pointer
 */
int _ll_update_item(LL_ITEM *pli, void* vpval, int ival, int type) {

    /* dispose of what used to be there*/
    switch(pli->type) {
    case LL_TYPE_LL:
        ll_destroy(pli->value.as_ll);
        break;
    case LL_TYPE_STRING:
        free(pli->value.as_string);
        break;
    case LL_TYPE_INT: /* fallthrough */
    default:
        break;
    }

    pli->type = type;

    switch(pli->type) {
    case LL_TYPE_INT:
        pli->value.as_int = ival;
        break;
    case LL_TYPE_LL:
        pli->value.as_ll = (LL *)vpval;
        break;
    case LL_TYPE_STRING:
        pli->value.as_string = strdup((char*)vpval);
        break;
    default:
        break;
    }

    return LL_E_SUCCESS;
}


/**
 * internal function to get the ll item associated with
 * a specific key, using the case sensitivity specified
 * by the ll flags.  This assumes that the lock is held!
 *
 * @param pl ll to fetch item from
 * @param key key name to fetch
 * @returns pointer to llitem, or null if not found
 */
LL_ITEM *ll_fetch_item(LL *pl, char *key) {
    LL_ITEM *current;

    if(!pl)
        return NULL;

    current = pl->itemlist.next;
    while(current) {
        if(pl->flags & LL_FLAG_HONORCASE) {
            if(!strcmp(current->key,key))
                return current;
        } else {
            if(!strcasecmp(current->key,key))
                return current;
        }
        current = current->next;
    }

    return current;
}

/**
 * set flags
 *
 * @param pl ll to set flags for
 * @returns LL_E_SUCCESS
 */
int ll_set_flags(LL *pl, unsigned int flags) {
    pl->flags = flags;
    return LL_E_SUCCESS;
}

/**
 * get flags
 *
 * @param pl ll to get flags from
 * @returns LL_E_SUCCESS
 */
int ll_get_flags(LL *pl, unsigned int *flags) {
    *flags = pl->flags;
    return LL_E_SUCCESS;
}

/**
 * Dump a linked list
 *
 * @parm pl linked list to dump
 */
void ll_dump(LL *pl) {
    _ll_dump(pl,0);

}

/**
 * Internal support function for dumping linked list that
 * carries depth.
 *
 * @param pl linked list to dump
 * @param depth depth of list
 */

void _ll_dump(LL *pl, int depth) {
    LL_ITEM *pli;
    pli = pl->itemlist.next;
    while(pli) {
        switch(pli->type) {
        case LL_TYPE_INT:
            printf("%*s%s (int): %d\n",depth*2,"",pli->key,pli->value.as_int);
            break;
        case LL_TYPE_STRING:
            printf("%*s%s (string): %s\n",depth*2,"",pli->key,pli->value.as_string);
            break;
        case LL_TYPE_LL:
            printf("%*s%s (list)\n",depth*2,"",pli->key);
            _ll_dump(pli->value.as_ll,depth+1);
            break;
        }
        pli = pli->next;
    }
}


/**
 * Given an item (or NULL for first item), fetch
 * the next item
 *
 * @param pl ll to fetch from
 * @param prev last item we fetched
 */
LL_ITEM *ll_get_next(LL *pl, LL_ITEM *prev) {
    if(!pl)
        return NULL;

    if(!prev)
        return pl->itemlist.next;

    return prev->next;
}
