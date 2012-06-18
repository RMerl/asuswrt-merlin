/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef VSFTP_HASH_H
#define VSFTP_HASH_H

struct hash;

typedef unsigned int (*hashfunc_t)(unsigned int, void*);

struct hash* hash_alloc(unsigned int buckets, unsigned int key_size,
                        unsigned int value_size, hashfunc_t hash_func);
void* hash_lookup_entry(struct hash* p_hash, void* p_key);
void hash_add_entry(struct hash* p_hash, void* p_key, void* p_value);
void hash_free_entry(struct hash* p_hash, void* p_key);

#endif /* VSFTP_HASH_H */

