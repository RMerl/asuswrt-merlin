/* $Id: hash.hpp 2394 2008-12-23 17:27:53Z bennylp $ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJPP_HASH_HPP__
#define __PJPP_HASH_HPP__

#include <pj++/types.hpp>
#include <pj++/pool.hpp>
#include <pj/hash.h>

//
// Hash table.
//
class Pj_Hash_Table : public Pj_Object
{
public:
    //
    // Hash table iterator.
    //
    class iterator
    {
    public:
	iterator() 
        {
        }
	explicit iterator(pj_hash_table_t *h, pj_hash_iterator_t *i) 
        : ht_(h), it_(i) 
        {
        }
	iterator(const iterator &rhs) 
        : ht_(rhs.ht_), it_(rhs.it_) 
        {
        }
	void operator++() 
        { 
            it_ = pj_hash_next(ht_, it_); 
        }
	bool operator==(const iterator &rhs) 
        { 
            return ht_ == rhs.ht_ && it_ == rhs.it_; 
        }
	iterator & operator=(const iterator &rhs) 
        { 
            ht_=rhs.ht_; it_=rhs.it_; 
            return *this; 
        }
    private:
	pj_hash_table_t *ht_;
	pj_hash_iterator_t it_val_;
	pj_hash_iterator_t *it_;

	friend class Pj_Hash_Table;
    };

    //
    // Construct hash table.
    //
    Pj_Hash_Table(Pj_Pool *pool, unsigned size)
    {
	table_ = pj_hash_create(pool->pool_(), size);
    }

    //
    // Destroy hash table.
    //
    ~Pj_Hash_Table()
    {
    }

    //
    // Calculate hash value.
    //
    static pj_uint32_t calc( pj_uint32_t initial_hval, 
                             const void *key, 
                             unsigned keylen = PJ_HASH_KEY_STRING)
    {
	return pj_hash_calc(initial_hval, key, keylen);
    }

    //
    // Return pjlib compatible hash table object.
    //
    pj_hash_table_t *pj_hash_table_t_()
    {
	return table_;
    }

    //
    // Get the value associated with the specified key.
    //
    void *get(const void *key, unsigned keylen = PJ_HASH_KEY_STRING)
    {
	return pj_hash_get(table_, key, keylen);
    }

    //
    // Associate a value with a key.
    // Set the value to NULL to delete the key from the hash table.
    //
    void set(Pj_Pool *pool, 
             const void *key, 
             void *value,
             unsigned keylen = PJ_HASH_KEY_STRING)
    {
	pj_hash_set(pool->pool_(), table_, key, keylen, value);
    }

    //
    // Get number of items in the hash table.
    //
    unsigned count()
    {
	return pj_hash_count(table_);
    }

    //
    // Iterate hash table.
    //
    iterator begin()
    {
	iterator it(table_, NULL);
	it.it_ = pj_hash_first(table_, &it.it_val_);
	return it;
    }

    //
    // End of items.
    //
    iterator end()
    {
	return iterator(table_, NULL);
    }

private:
    pj_hash_table_t *table_;
};


#endif	/* __PJPP_HASH_HPP__ */

