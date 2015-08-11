/* $Id: hash.h 4385 2013-02-27 10:11:59Z nanang $ */
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
#ifndef __PJ_HASH_H__
#define __PJ_HASH_H__

/**
 * @file hash.h
 * @brief Hash Table.
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_HASH Hash Table
 * @ingroup PJ_DS
 * @{
 * A hash table is a dictionary in which keys are mapped to array positions by
 * hash functions. Having the keys of more than one item map to the same 
 * position is called a collision. In this library, we will chain the nodes
 * that have the same key in a list.
 */

/**
 * If this constant is used as keylen, then the key is interpreted as
 * NULL terminated string.
 */
#define PJ_HASH_KEY_STRING	((unsigned)-1)

/**
 * This indicates the size of of each hash entry.
 */
#define PJ_HASH_ENTRY_BUF_SIZE	(3*sizeof(void*) + 2*sizeof(pj_uint32_t))

/**
 * Type declaration for entry buffer, used by #pj_hash_set_np()
 */
typedef void *pj_hash_entry_buf[(PJ_HASH_ENTRY_BUF_SIZE+sizeof(void*)-1)/(sizeof(void*))];

/**
 * This is the function that is used by the hash table to calculate hash value
 * of the specified key.
 *
 * @param hval	    the initial hash value, or zero.
 * @param key	    the key to calculate.
 * @param keylen    the length of the key, or PJ_HASH_KEY_STRING to treat 
 *		    the key as null terminated string.
 *
 * @return          the hash value.
 */
PJ_DECL(pj_uint32_t) pj_hash_calc(pj_uint32_t hval, 
				  const void *key, unsigned keylen);


/**
 * Convert the key to lowercase and calculate the hash value. The resulting
 * string is stored in \c result.
 *
 * @param hval      The initial hash value, normally zero.
 * @param result    Optional. Buffer to store the result, which must be enough
 *                  to hold the string.
 * @param key       The input key to be converted and calculated.
 *
 * @return          The hash value.
 */
PJ_DECL(pj_uint32_t) pj_hash_calc_tolower(pj_uint32_t hval,
                                          char *result,
                                          const pj_str_t *key);

/**
 * Create a hash table with the specified 'bucket' size.
 *
 * @param pool	the pool from which the hash table will be allocated from.
 * @param size	the bucket size, which will be round-up to the nearest 2^n-1
 *
 * @return the hash table.
 */
PJ_DECL(pj_hash_table_t*) pj_hash_create(pj_pool_t *pool, unsigned size);


/**
 * Get the value associated with the specified key.
 *
 * @param ht	    the hash table.
 * @param key	    the key to look for.
 * @param keylen    the length of the key, or PJ_HASH_KEY_STRING to use the
 *		    string length of the key.
 * @param hval	    if this argument is not NULL and the value is not zero,
 *		    the value will be used as the computed hash value. If
 *		    the argument is not NULL and the value is zero, it will
 *		    be filled with the computed hash upon return.
 *
 * @return the value associated with the key, or NULL if the key is not found.
 */
PJ_DECL(void *) pj_hash_get( pj_hash_table_t *ht,
			     const void *key, unsigned keylen,
			     pj_uint32_t *hval );


/**
 * Variant of #pj_hash_get() with the key being converted to lowercase when
 * calculating the hash value.
 *
 * @see pj_hash_get()
 */
PJ_DECL(void *) pj_hash_get_lower( pj_hash_table_t *ht,
			           const void *key, unsigned keylen,
			           pj_uint32_t *hval );


/**
 * Associate/disassociate a value with the specified key. If value is not
 * NULL and entry already exists, the entry's value will be overwritten.
 * If value is not NULL and entry does not exist, a new one will be created
 * with the specified pool. Otherwise if value is NULL, entry will be
 * deleted if it exists.
 *
 * @param pool	    the pool to allocate the new entry if a new entry has to be
 *		    created.
 * @param ht	    the hash table.
 * @param key	    the key. If pool is not specified, the key MUST point to
 * 		    buffer that remains valid for the duration of the entry.
 * @param keylen    the length of the key, or PJ_HASH_KEY_STRING to use the 
 *		    string length of the key.
 * @param hval	    if the value is not zero, then the hash table will use
 *		    this value to search the entry's index, otherwise it will
 *		    compute the key. This value can be obtained when calling
 *		    #pj_hash_get().
 * @param value	    value to be associated, or NULL to delete the entry with
 *		    the specified key.
 */
PJ_DECL(void) pj_hash_set( pj_pool_t *pool, pj_hash_table_t *ht,
			   const void *key, unsigned keylen, pj_uint32_t hval,
			   void *value );


/**
 * Variant of #pj_hash_set() with the key being converted to lowercase when
 * calculating the hash value.
 *
 * @see pj_hash_set()
 */
PJ_DECL(void) pj_hash_set_lower( pj_pool_t *pool, pj_hash_table_t *ht,
			         const void *key, unsigned keylen,
                                 pj_uint32_t hval, void *value );


/**
 * Associate/disassociate a value with the specified key. This function works
 * like #pj_hash_set(), except that it doesn't use pool (hence the np -- no 
 * pool suffix). If new entry needs to be allocated, it will use the entry_buf.
 *
 * @param ht	    the hash table.
 * @param key	    the key.
 * @param keylen    the length of the key, or PJ_HASH_KEY_STRING to use the 
 *		    string length of the key.
 * @param hval	    if the value is not zero, then the hash table will use
 *		    this value to search the entry's index, otherwise it will
 *		    compute the key. This value can be obtained when calling
 *		    #pj_hash_get().
 * @param entry_buf Buffer which will be used for the new entry, when one needs
 *		    to be created.
 * @param value	    value to be associated, or NULL to delete the entry with
 *		    the specified key.
 */
PJ_DECL(void) pj_hash_set_np(pj_hash_table_t *ht,
			     const void *key, unsigned keylen, 
			     pj_uint32_t hval, pj_hash_entry_buf entry_buf, 
			     void *value);

/**
 * Variant of #pj_hash_set_np() with the key being converted to lowercase
 * when calculating the hash value.
 *
 * @see pj_hash_set_np()
 */
PJ_DECL(void) pj_hash_set_np_lower(pj_hash_table_t *ht,
			           const void *key, unsigned keylen,
			           pj_uint32_t hval,
                                   pj_hash_entry_buf entry_buf,
			           void *value);

/**
 * Get the total number of entries in the hash table.
 *
 * @param ht	the hash table.
 *
 * @return the number of entries in the hash table.
 */
PJ_DECL(unsigned) pj_hash_count( pj_hash_table_t *ht );


/**
 * Get the iterator to the first element in the hash table. 
 *
 * @param ht	the hash table.
 * @param it	the iterator for iterating hash elements.
 *
 * @return the iterator to the hash element, or NULL if no element presents.
 */
PJ_DECL(pj_hash_iterator_t*) pj_hash_first( pj_hash_table_t *ht,
					    pj_hash_iterator_t *it );


/**
 * Get the next element from the iterator.
 *
 * @param ht	the hash table.
 * @param it	the hash iterator.
 *
 * @return the next iterator, or NULL if there's no more element.
 */
PJ_DECL(pj_hash_iterator_t*) pj_hash_next( pj_hash_table_t *ht, 
					   pj_hash_iterator_t *it );

/**
 * Get the value associated with a hash iterator.
 *
 * @param ht	the hash table.
 * @param it	the hash iterator.
 *
 * @return the value associated with the current element in iterator.
 */
PJ_DECL(void*) pj_hash_this( pj_hash_table_t *ht,
			     pj_hash_iterator_t *it );


/**
 * @}
 */

PJ_END_DECL

#endif


