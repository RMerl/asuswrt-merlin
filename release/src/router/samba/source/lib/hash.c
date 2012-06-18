/*
   Unix SMB/Netbios implementation.
   Version 2.0

   Copyright (C) Ying Chen 2000.
   Copyright (C) Jeremy Allison 2000.
                 - added some defensive programming.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * NB. We may end up replacing this functionality in a future 2.x 
 * release to reduce the number of hashing/lookup methods we support. JRA.
 */

#include "includes.h"

extern int DEBUGLEVEL;

#define NUM_PRIMES 11

static BOOL enlarge_hash_table(hash_table *table);
static int primes[NUM_PRIMES] = 
        {17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209, 16411};

/****************************************************************************
 *      This function initializes the hash table.
 *      This hash function hashes on string keys.
 *      This number of hash buckets is always rounded up to a power of 
 *      2 first, then to a prime number that is large than the power of two. 
 *      Input:
 *              table -- the hash table pointer.
 *              num_buckets -- the number of buckets to be allocated. This
 *              hash function can dynamically increase its size when the 
 *              the hash table size becomes small. There is a MAX hash table
 *              size defined in hash.h.
 *              compare_func -- the function pointer to a comparison function
 *              used by the hash key comparison.
 ****************************************************************************
 */

BOOL hash_table_init(hash_table *table, int num_buckets, compare_function compare_func)
{
	int 	i;
	ubi_dlList 	*bucket;

	table->num_elements = 0;
	table->size = 2;
	table->comp_func = compare_func;
	while (table->size < num_buckets) 
	table->size <<= 1;
	for (i = 0; i < NUM_PRIMES; i++) {
		if (primes[i] > table->size) {
			table->size = primes[i];
			break;
		}
	}

	DEBUG(5, ("Hash size = %d.\n", table->size));

	if(!(table->buckets = (ubi_dlList *) malloc(sizeof(ubi_dlList) * table->size))) {
		DEBUG(0,("hash_table_init: malloc fail !\n"));
		return False;
	}
	ubi_dlInitList(&(table->lru_chain));
	for (i=0, bucket = table->buckets; i < table->size; i++, bucket++) 
		ubi_dlInitList(bucket);

	return True;
}

/*
 **************************************************************
 *	Compute a hash value based on a string key value.
 *	Make the string key into an array of int's if possible.
 *	For the last few chars that cannot be int'ed, use char instead.
 *	The function returns the bucket index number for the hashed 
 *	key.
 **************************************************************
 */

int string_hash(int hash_size, const char *key)
{
	int j=0;
	while (*key)
		j = j*10 + *key++;
	return(((j>=0)?j:(-j)) % hash_size);
}

/* *************************************************************************
 * 	Search the hash table for the entry in the hash chain.
 *	The function returns the pointer to the 
 *	element found in the chain or NULL if none is found. 
 *	If the element is found, the element is also moved to 
 *	the head of the LRU list.
 *
 *	Input:
 *              table -- The hash table where the element is stored in.
 *              hash_chain -- The pointer to the bucket that stores the 
 *              element to be found.
 *              key -- The hash key to be found. 
 ***************************************************************************
 */

static hash_element *hash_chain_find(hash_table *table, ubi_dlList *hash_chain, char *key)
{
	hash_element *hash_elem;
	ubi_dlNodePtr lru_item;
	int	i = 0;

	for (hash_elem = (hash_element *)(ubi_dlFirst(hash_chain)); i < hash_chain->count; 
		i++, hash_elem = (hash_element *)(ubi_dlNext(hash_elem))) {
		if ((table->comp_func)(hash_elem->key, key) == 0) {
			/* Move to the head of the lru List. */
			lru_item = ubi_dlRemove(&(table->lru_chain), &(hash_elem->lru_link.lru_link));
			ubi_dlAddHead(&(table->lru_chain), lru_item);
			return(hash_elem);
		}
	}
	return ((hash_element *) NULL);
}

/* ***************************************************************************
 *
 * 	Lookup a hash table for an element with key.  
 *      The function returns a pointer to the hash element.
 *	If no element is found, the function returns NULL.
 *
 *      Input:
 *              table -- The hash table to be searched on.
 *              key -- The key to be found.
 *****************************************************************************
 */

hash_element *hash_lookup(hash_table *table, char *key)
{
	return (hash_chain_find(table, &(table->buckets[string_hash(table->size, key)]), key));
}

/* ***************************************************************
 *
 *	This function first checks if an element with key "key"
 *	exists in the hash table. If so, the function moves the 
 *	element to the front of the LRU list. Otherwise, a new 
 *	hash element corresponding to "value" and "key" is allocated
 *	and inserted into the hash table. The new elements are
 *	always inserted in the LRU order to the LRU list as well.
 *
 *      Input:
 *              table -- The hash table to be inserted in.
 *              value -- The content of the element to be inserted.
 *              key -- The key of the new element to be inserted.
 *
 ****************************************************************
 */

hash_element *hash_insert(hash_table *table, char *value, char *key)
{
	hash_element	*hash_elem;
	ubi_dlNodePtr lru_item;
	ubi_dlList *bucket; 

	/* 
	 * If the hash table size has not reached the MAX_HASH_TABLE_SIZE,
	 * the hash table may be enlarged if the current hash table is full.
	 * If the hash table size has reached the MAX_HASH_TABLE_SIZE, 
	 * use LRU to remove the oldest element from the hash table.
	 */

	if ((table->num_elements >= table->size) &&  
		(table->num_elements < MAX_HASH_TABLE_SIZE)) {
		if(!enlarge_hash_table(table))
			return (hash_element *)NULL;
		table->num_elements += 1;
	} else if (table->num_elements >= MAX_HASH_TABLE_SIZE) {
		/* Do an LRU replacement. */
		lru_item = ubi_dlLast(&(table->lru_chain));
		hash_elem = (hash_element *)(((lru_node *)lru_item)->hash_elem);
		bucket = hash_elem->bucket;
		ubi_dlRemThis(&(table->lru_chain), &(hash_elem->lru_link.lru_link));
		ubi_dlRemThis(bucket, (ubi_dlNodePtr)hash_elem);
		free((char*)(hash_elem->value));
		free(hash_elem);
	}  else  {
		table->num_elements += 1;
	}

	bucket = &(table->buckets[string_hash(table->size, key)]);

	/* Since we only have 1-byte for the key string, we need to 
	 * allocate extra space in the hash_element to store the entire key
	 * string.
	 */

	if(!(hash_elem = (hash_element *) malloc(sizeof(hash_element) + strlen(key)))) {
		DEBUG(0,("hash_insert: malloc fail !\n"));
		return (hash_element *)NULL;
	}

	safe_strcpy((char *) hash_elem->key, key, strlen(key)+1);

	hash_elem->value = (char *)value;
	hash_elem->bucket = bucket;
	/* Insert in front of the lru list and the bucket list. */
	ubi_dlAddHead(bucket, hash_elem);
	hash_elem->lru_link.hash_elem = hash_elem;
	ubi_dlAddHead(&(table->lru_chain), &(hash_elem->lru_link.lru_link));

	return(hash_elem);
}

/* **************************************************************************
 *
 * 	Remove a hash element from the hash table. The hash element is 
 * 	removed from both the LRU list and the hash bucket chain.
 *
 *      Input:
 *              table -- the hash table to be manipulated on.
 *              hash_elem -- the element to be removed.
 **************************************************************************
 */

void hash_remove(hash_table *table, hash_element *hash_elem)
{
	if (hash_elem) { 
		ubi_dlRemove(&(table->lru_chain), &(hash_elem->lru_link.lru_link));
		ubi_dlRemove(hash_elem->bucket, (ubi_dlNodePtr) hash_elem);
		if(hash_elem->value)
			free((char *)(hash_elem->value));
		if(hash_elem)
			free((char *) hash_elem);
		table->num_elements--;
	}
}

/* ******************************************************************
 *	Increase the hash table size if it is too small. 
 *	The hash table size is increased by the HASH_TABLE_INCREMENT
 *	ratio.
 *      Input:
 *              table -- the hash table to be enlarged.
 ******************************************************************
 */

static BOOL enlarge_hash_table(hash_table *table)
{
	hash_element  	*hash_elem;
	int size, hash_value;
	ubi_dlList	*buckets;
	ubi_dlList	*old_bucket;
	ubi_dlList	*bucket;
	ubi_dlList  lru_chain;

	buckets = table->buckets;
	lru_chain = table->lru_chain;
	size = table->size;

	/* Reinitialize the hash table. */
	if(!hash_table_init(table, table->size * HASH_TABLE_INCREMENT, table->comp_func))
		return False;

	for (old_bucket = buckets; size > 0; size--, old_bucket++) {
		while (old_bucket->count != 0) {
			hash_elem = (hash_element *) ubi_dlRemHead(old_bucket);
			ubi_dlRemove(&lru_chain, &(hash_elem->lru_link.lru_link));
			hash_value = string_hash(table->size, (char *) hash_elem->key);
			bucket = &(table->buckets[hash_value]);
			ubi_dlAddHead(bucket, hash_elem);
			ubi_dlAddHead(&(table->lru_chain), &(hash_elem->lru_link.lru_link));
			hash_elem->bucket = bucket;
			hash_elem->lru_link.hash_elem = hash_elem;
			table->num_elements++;
		}
	}
	if(buckets)
		free((char *) buckets);

	return True;
}

/* **********************************************************************
 *
 *	Remove everything from a hash table and free up the memory it 
 *	occupies. 
 *      Input: 
 *              table -- the hash table to be cleared.
 *
 *************************************************************************
 */

void hash_clear(hash_table *table)
{
	int i;
	ubi_dlList	*bucket = table->buckets;
	hash_element    *hash_elem;
	for (i = 0; i < table->size; bucket++, i++) {
		while (bucket->count != 0) {
			hash_elem = (hash_element *) ubi_dlRemHead(bucket);
			if(hash_elem->value)
				free((char *)(hash_elem->value));
			if(hash_elem)
				free((char *)hash_elem);
		}
	}
	if(table->buckets)
		free((char *) table->buckets);
}
