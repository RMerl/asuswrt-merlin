/* 
   Unix SMB/Netbios implementation.
   Version 2.0

   Copyright (C) Ying Chen 2000.
   
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

#ifndef	_HASH_H_
#define	_HASH_H_

#define MAX_HASH_TABLE_SIZE    32768
#define HASH_TABLE_INCREMENT  2

typedef int (*compare_function)(char *, char *);
typedef int (*hash_function)(int, char *);

/*
 * lru_link: links the node to the LRU list.
 * hash_elem: the pointer to the element that is tied onto the link.
 */
typedef struct lru_node {
        ubi_dlNode lru_link;
        void *hash_elem;
} lru_node;

/* 
 * bucket_link: link the hash element to the bucket chain that it belongs to.
 * lru_link: this element ties the hash element to the lru list. 
 * bucket: a pointer to the hash bucket that this element belongs to.
 * value: a pointer to the hash element content. It can be anything.
 * key: stores the string key. The hash_element is always allocated with
 * more memory space than the structure shown below to accomodate the space
 * used for the whole string. But the memory is always appended at the 
 * end of the structure, so keep "key" at the end of the structure.
 * Don't move it.
 */
typedef struct hash_element {
        ubi_dlNode	bucket_link;          
        lru_node    lru_link;
        ubi_dlList	*bucket;
        void	*value;		
        char key[1];	
} hash_element;

/*
 * buckets: a list of buckets, implemented as a dLinkList. 
 * lru_chain: the lru list of all the hash elements. 
 * num_elements: the # of elements in the hash table.
 * size: the hash table size.
 * comp_func: the compare function used during hash key comparisons.
 */

typedef struct hash_table {
        ubi_dlList      *buckets;
        ubi_dlList      lru_chain;     
        int     num_elements;	
        int     size;
        compare_function        comp_func; 
} hash_table;

#endif /* _HASH_H_ */
