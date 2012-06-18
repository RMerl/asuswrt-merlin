/***********************************************************************
*
* hash.c
*
* Implementation of hash tables.  Each item inserted must include
* a hash_bucket member.
*
* Copyright (C) 2002 Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2 or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id$";

#include "hash.h"

#include <limits.h>
#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))

#define GET_BUCKET(tab, data) ((hash_bucket *) ((char *) (data) + (tab)->hash_offset))

#define GET_ITEM(tab, bucket) ((void *) (((char *) (void *) bucket) - (tab)->hash_offset))

static void *hash_next_cursor(hash_table *tab, hash_bucket *b);

/**********************************************************************
* %FUNCTION: hash_init
* %ARGUMENTS:
*  tab -- hash table
*  hash_offset -- offset of hash_bucket data member in inserted items
*  compute -- pointer to function to compute hash value
*  compare -- pointer to comparison function.  Returns 0 if items are equal,
*             non-zero otherwise.
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Initializes a hash table.
***********************************************************************/
void
hash_init(hash_table *tab,
	  size_t hash_offset,
	  unsigned int (*compute)(void *data),
	  int (*compare)(void *item1, void *item2))
{
    size_t i;

    tab->hash_offset = hash_offset;
    tab->compute_hash = compute;
    tab->compare = compare;
    for (i=0; i<HASHTAB_SIZE; i++) {
	tab->buckets[i] = NULL;
    }
    tab->num_entries = 0;
}

/**********************************************************************
* %FUNCTION: hash_insert
* %ARGUMENTS:
*  tab -- hash table to insert into
*  item -- the item we're inserting
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Inserts an item into the hash table.  It must not currently be in any
*  hash table.
***********************************************************************/
void
hash_insert(hash_table *tab,
	    void *item)
{
    hash_bucket *b = GET_BUCKET(tab, item);
    unsigned int val = tab->compute_hash(item);
    b->hashval = val;
    val %= HASHTAB_SIZE;
    b->prev = NULL;
    b->next = tab->buckets[val];
    if (b->next) {
	b->next->prev = b;
    }
    tab->buckets[val] = b;
    tab->num_entries++;
}

/**********************************************************************
* %FUNCTION: hash_remove
* %ARGUMENTS:
*  tab -- hash table
*  item -- item in hash table
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Removes item from hash table
***********************************************************************/
void
hash_remove(hash_table *tab,
	    void *item)
{
    hash_bucket *b = GET_BUCKET(tab, item);
    unsigned int val = b->hashval % HASHTAB_SIZE;

    if (b->prev) {
	b->prev->next = b->next;
    } else {
	tab->buckets[val] = b->next;
    }
    if (b->next) {
	b->next->prev = b->prev;
    }
    tab->num_entries--;
}

/**********************************************************************
* %FUNCTION: hash_find
* %ARGUMENTS:
*  tab -- hash table
*  item -- item equal to one we're seeking (in the compare-function sense)
* %RETURNS:
*  A pointer to the item in the hash table, or NULL if no such item
* %DESCRIPTION:
*  Searches hash table for item.
***********************************************************************/
void *
hash_find(hash_table *tab,
	  void *item)
{
    unsigned int val = tab->compute_hash(item) % HASHTAB_SIZE;
    hash_bucket *b;
    for (b = tab->buckets[val]; b; b = b->next) {
	void *item2 = GET_ITEM(tab, b);
	if (!tab->compare(item, item2)) return item2;
    }
    return NULL;
}

/**********************************************************************
* %FUNCTION: hash_find_next
* %ARGUMENTS:
*  tab -- hash table
*  item -- an item returned by hash_find or hash_find_next
* %RETURNS:
*  A pointer to the next equal item in the hash table, or NULL if no such item
* %DESCRIPTION:
*  Searches hash table for anoter item equivalent to this one.  Search
*  starts from item.
***********************************************************************/
void *
hash_find_next(hash_table *tab,
	       void *item)
{
    hash_bucket *b = GET_BUCKET(tab, item);
    for (b = b->next; b; b = b->next) {
	void *item2 = GET_ITEM(tab, b);
	if (!tab->compare(item, item2)) return item2;
    }
    return NULL;
}
/**********************************************************************
* %FUNCTION: hash_start
* %ARGUMENTS:
*  tab -- hash table
*  cursor -- a void pointer to keep track of location
* %RETURNS:
*  "first" entry in hash table, or NULL if table is empty
* %DESCRIPTION:
*  Starts an iterator -- sets cursor so hash_next will return next entry.
***********************************************************************/
void *
hash_start(hash_table *tab, void **cursor)
{
    int i;
    for (i=0; i<HASHTAB_SIZE; i++) {
	if (tab->buckets[i]) {
	    /* Point cursor to NEXT item so it is valid
	       even if current item is free'd */
	    *cursor = hash_next_cursor(tab, tab->buckets[i]);
	    return GET_ITEM(tab, tab->buckets[i]);
	}
    }
    *cursor = NULL;
    return NULL;
}

/**********************************************************************
* %FUNCTION: hash_next
* %ARGUMENTS:
*  tab -- hash table
*  cursor -- cursor into hash table
* %RETURNS:
*  Next item in table, or NULL.
* %DESCRIPTION:
*  Steps cursor to next item in table.
***********************************************************************/
void *
hash_next(hash_table *tab, void **cursor)
{
    hash_bucket *b;

    if (!*cursor) return NULL;

    b = (hash_bucket *) *cursor;
    *cursor = hash_next_cursor(tab, b);
    return GET_ITEM(tab, b);
}

/**********************************************************************
* %FUNCTION: hash_next_cursor
* %ARGUMENTS:
*  tab -- a hash table
*  b -- a hash bucket
* %RETURNS:
*  Cursor value for bucket following b in hash table.
***********************************************************************/
static void *
hash_next_cursor(hash_table *tab, hash_bucket *b)
{
    unsigned int i;
    if (!b) return NULL;
    if (b->next) return b->next;

    i = b->hashval % HASHTAB_SIZE;
    for (++i; i<HASHTAB_SIZE; ++i) {
	if (tab->buckets[i]) return tab->buckets[i];
    }
    return NULL;
}

size_t
hash_num_entries(hash_table *tab)
{
    return tab->num_entries;
}

/**********************************************************************
* %FUNCTION: hash_pjw
* %ARGUMENTS:
*  str -- a zero-terminated string
* %RETURNS:
*  A hash value using the hashpjw algorithm
* %DESCRIPTION:
*  An adaptation of Peter Weinberger's (PJW) generic hashing
*  algorithm based on Allen Holub's version. Accepts a pointer
*  to a datum to be hashed and returns an unsigned integer.
***********************************************************************/
unsigned int
hash_pjw(char const * str)
{
    unsigned int hash_value, i;

    for (hash_value = 0; *str; ++str) {
        hash_value = ( hash_value << ONE_EIGHTH ) + *str;
        if (( i = hash_value & HIGH_BITS ) != 0 ) {
            hash_value =
                ( hash_value ^ ( i >> THREE_QUARTERS )) &
		~HIGH_BITS;
	}
    }
    return hash_value;
}
