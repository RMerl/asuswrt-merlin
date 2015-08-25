/* $Id: hash.c 4385 2013-02-27 10:11:59Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pj/hash.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/os.h>
#include <pj/ctype.h>
#include <pj/assert.h>

#define THIS_FILE "hash.c"

/**
 * The hash multiplier used to calculate hash value.
 */
#define PJ_HASH_MULTIPLIER	33


struct pj_hash_entry
{
    struct pj_hash_entry *next;
    void *key;
    pj_uint32_t hash;
    pj_uint32_t keylen;
    void *value;
};


struct pj_hash_table_t
{
    pj_hash_entry     **table;
    unsigned		count, rows;
    pj_hash_iterator_t	iterator;
};



PJ_DEF(pj_uint32_t) pj_hash_calc(pj_uint32_t hash, const void *key, 
				 unsigned keylen)
{
    PJ_CHECK_STACK();

    if (keylen==PJ_HASH_KEY_STRING) {
	const pj_uint8_t *p = (const pj_uint8_t*)key;
	for ( ; *p; ++p ) {
	    hash = (hash * PJ_HASH_MULTIPLIER) + *p;
	}
    } else {
	const pj_uint8_t *p = (const pj_uint8_t*)key,
			      *end = p + keylen;
	for ( ; p!=end; ++p) {
	    hash = (hash * PJ_HASH_MULTIPLIER) + *p;
	}
    }
    return hash;
}

PJ_DEF(pj_uint32_t) pj_hash_calc_tolower( pj_uint32_t hval,
                                          char *result,
                                          const pj_str_t *key)
{
    long i;

#if defined(PJ_HASH_USE_OWN_TOLOWER) && PJ_HASH_USE_OWN_TOLOWER != 0
    for (i=0; i<key->slen; ++i) {
	pj_uint8_t c = key->ptr[i];
        char lower;
	if (c & 64)
	    lower = (char)(c | 32);
	else
	    lower = (char)c;
	if (result)
            result[i] = lower;
	hval = hval * PJ_HASH_MULTIPLIER + lower;
    }
#else
    for (i=0; i<key->slen; ++i) {
        char lower = (char)pj_tolower(key->ptr[i]);
	if (result)
            result[i] = lower;
	hval = hval * PJ_HASH_MULTIPLIER + lower;
    }
#endif

    return hval;
}


PJ_DEF(pj_hash_table_t*) pj_hash_create(pj_pool_t *pool, unsigned size)
{
    pj_hash_table_t *h;
    unsigned table_size;
    
    /* Check that PJ_HASH_ENTRY_BUF_SIZE is correct. */
    PJ_ASSERT_RETURN(sizeof(pj_hash_entry)<=PJ_HASH_ENTRY_BUF_SIZE, NULL);

    h = PJ_POOL_ALLOC_T(pool, pj_hash_table_t);
    h->count = 0;

    PJ_LOG( 6, ("hashtbl", "hash table %p created from pool %s", h, pj_pool_getobjname(pool)));

    /* size must be 2^n - 1.
       round-up the size to this rule, except when size is 2^n, then size
       will be round-down to 2^n-1.
     */
    table_size = 8;
    do {
	table_size <<= 1;    
    } while (table_size < size);
    table_size -= 1;
    
    h->rows = table_size;
    h->table = (pj_hash_entry**)
    	       pj_pool_calloc(pool, table_size+1, sizeof(pj_hash_entry*));
    return h;
}

static pj_hash_entry **find_entry( pj_pool_t *pool, pj_hash_table_t *ht, 
				   const void *key, unsigned keylen,
				   void *val, pj_uint32_t *hval,
				   void *entry_buf, pj_bool_t lower)
{
    pj_uint32_t hash;
    pj_hash_entry **p_entry, *entry;

    if (hval && *hval != 0) {
	hash = *hval;
	if (keylen==PJ_HASH_KEY_STRING) {
	    keylen = pj_ansi_strlen((const char*)key);
	}
    } else {
	/* This slightly differs with pj_hash_calc() because we need 
	 * to get the keylen when keylen is PJ_HASH_KEY_STRING.
	 */
	hash=0;
	PJ_LOG(6, ("hashtbl", "%p: lower=[%d], keylen=[%d]", ht, lower, keylen));
	if (keylen==PJ_HASH_KEY_STRING) {
	    const pj_uint8_t *p = (const pj_uint8_t*)key;
	    for ( ; *p; ++p ) {
            if (lower)
                hash = hash * PJ_HASH_MULTIPLIER + pj_tolower(*p);
            else 
				hash = hash * PJ_HASH_MULTIPLIER + *p;
			PJ_LOG(5, ("hashtbl", "%p: hash value=[%u], *p=[%u]", ht, hash, *p));
	    }
	    keylen = p - (const unsigned char*)key;
	} else {
	    const pj_uint8_t *p = (const pj_uint8_t*)key,
				  *end = p + keylen;
	    for ( ; p!=end; ++p) {
		if (lower)
			hash = hash * PJ_HASH_MULTIPLIER + pj_tolower(*p);
        else
			hash = hash * PJ_HASH_MULTIPLIER + *p;
		PJ_LOG(6, ("hashtbl", "%p: hash value=[%u], *p=[%u]", ht, hash, *p));
	    }
	}

	/* Report back the computed hash. */
	if (hval)
	    *hval = hash;
	}
	PJ_LOG(6, ("hashtbl", "%p: hash value=[%u]", ht, hash));

    /* scan the linked list */
    for (p_entry = &ht->table[hash & ht->rows], entry=*p_entry; 
	 entry; 
	 p_entry = &entry->next, entry = *p_entry)
    {
	if (entry->hash==hash && entry->keylen==keylen &&
            ((lower && pj_ansi_strnicmp((const char*)entry->key,
        			        (const char*)key, keylen)==0) ||
	     (!lower && pj_memcmp(entry->key, key, keylen)==0)))
	{
	    break;	
	}
    }

    if (entry || val==NULL)
	return p_entry;

    /* Entry not found, create a new one. 
     * If entry_buf is specified, use it. Otherwise allocate from pool.
     */
    if (entry_buf) {
	entry = (pj_hash_entry*)entry_buf;
    } else {
	/* Pool must be specified! */
	PJ_ASSERT_RETURN(pool != NULL, NULL);

	entry = PJ_POOL_ALLOC_T(pool, pj_hash_entry);
	PJ_LOG(6, ("hashtbl", 
		   "%p: New p_entry %p created, pool used=%u, cap=%u", 
		   ht, entry,  pj_pool_get_used_size(pool), 
		   pj_pool_get_capacity(pool)));
    }
    entry->next = NULL;
    entry->hash = hash;
    if (pool) {
	entry->key = pj_pool_alloc(pool, keylen);
	pj_memcpy(entry->key, key, keylen);
    } else {
	entry->key = (void*)key;
    }
    entry->keylen = keylen;
    entry->value = val;
    *p_entry = entry;
    
    ++ht->count;
    
    return p_entry;
}

PJ_DEF(void *) pj_hash_get( pj_hash_table_t *ht,
			    const void *key, unsigned keylen,
			    pj_uint32_t *hval)
{
    pj_hash_entry *entry;
    entry = *find_entry( NULL, ht, key, keylen, NULL, hval, NULL, PJ_FALSE);
    return entry ? entry->value : NULL;
}

PJ_DEF(void *) pj_hash_get_lower( pj_hash_table_t *ht,
			          const void *key, unsigned keylen,
			          pj_uint32_t *hval)
{
    pj_hash_entry *entry;
    entry = *find_entry( NULL, ht, key, keylen, NULL, hval, NULL, PJ_TRUE);
    return entry ? entry->value : NULL;
}

static void hash_set( pj_pool_t *pool, pj_hash_table_t *ht,
			  const void *key, unsigned keylen, pj_uint32_t hval,
		      void *value, void *entry_buf, pj_bool_t lower )
{
    pj_hash_entry **p_entry;

    p_entry = find_entry( pool, ht, key, keylen, value, &hval, entry_buf,
                          lower);
    if (*p_entry) {
	if (value == NULL) {
	    /* delete entry */
	    PJ_LOG(6, ("hashtbl", "%p: p_entry %p deleted", ht, *p_entry));
	    *p_entry = (*p_entry)->next;
	    --ht->count;
	    
	} else {
	    /* overwrite */
	    (*p_entry)->value = value;
	    PJ_LOG(6, ("hashtbl", "%p: p_entry %p value set to %p", ht, 
		       *p_entry, value));
	}
    }
}

PJ_DEF(void) pj_hash_set( pj_pool_t *pool, pj_hash_table_t *ht,
			  const void *key, unsigned keylen, pj_uint32_t hval,
			  void *value )
{
    hash_set(pool, ht, key, keylen, hval, value, NULL, PJ_FALSE);
}

PJ_DEF(void) pj_hash_set_lower( pj_pool_t *pool, pj_hash_table_t *ht,
			        const void *key, unsigned keylen,
                                pj_uint32_t hval, void *value )
{
    hash_set(pool, ht, key, keylen, hval, value, NULL, PJ_TRUE);
}

PJ_DEF(void) pj_hash_set_np( pj_hash_table_t *ht,
			     const void *key, unsigned keylen, 
			     pj_uint32_t hval, pj_hash_entry_buf entry_buf, 
			     void *value)
{
    hash_set(NULL, ht, key, keylen, hval, value, (void *)entry_buf, PJ_FALSE);
}

PJ_DEF(void) pj_hash_set_np_lower( pj_hash_table_t *ht,
			           const void *key, unsigned keylen,
			           pj_uint32_t hval,
                                   pj_hash_entry_buf entry_buf,
			           void *value)
{
    hash_set(NULL, ht, key, keylen, hval, value, (void *)entry_buf, PJ_TRUE);
}

PJ_DEF(unsigned) pj_hash_count( pj_hash_table_t *ht )
{
    return ht->count;
}

PJ_DEF(pj_hash_iterator_t*) pj_hash_first( pj_hash_table_t *ht,
					   pj_hash_iterator_t *it )
{
    it->index = 0;
    it->entry = NULL;

    for (; it->index <= ht->rows; ++it->index) {
	it->entry = ht->table[it->index];
	if (it->entry) {
	    break;
	}
    }

    return it->entry ? it : NULL;
}

PJ_DEF(pj_hash_iterator_t*) pj_hash_next( pj_hash_table_t *ht, 
					  pj_hash_iterator_t *it )
{
    it->entry = it->entry->next;
    if (it->entry) {
	return it;
    }

    for (++it->index; it->index <= ht->rows; ++it->index) {
	it->entry = ht->table[it->index];
	if (it->entry) {
	    break;
	}
    }

    return it->entry ? it : NULL;
}

PJ_DEF(void*) pj_hash_this( pj_hash_table_t *ht, pj_hash_iterator_t *it )
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(ht);
    return it->entry->value;
}

#if 0
void pj_hash_dump_collision( pj_hash_table_t *ht )
{
    unsigned min=0xFFFFFFFF, max=0;
    unsigned i;
    char line[120];
    int len, totlen = 0;

    for (i=0; i<=ht->rows; ++i) {
	unsigned count = 0;    
	pj_hash_entry *entry = ht->table[i];
	while (entry) {
	    ++count;
	    entry = entry->next;
	}
	if (count < min)
	    min = count;
	if (count > max)
	    max = count;
	len = pj_snprintf( line+totlen, sizeof(line)-totlen, "%3d:%3d ", i, count);
	if (len < 1)
	    break;
	totlen += len;

	if ((i+1) % 10 == 0) {
	    line[totlen] = '\0';
	    PJ_LOG(4,(THIS_FILE, line));
	}
    }

    PJ_LOG(4,(THIS_FILE,"Count: %d, min: %d, max: %d\n", ht->count, min, max));
}
#endif


