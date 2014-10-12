/*
   Unix SMB/CIFS implementation.
   In-memory cache
   Copyright (C) Volker Lendecke 2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "memcache.h"
#include "../lib/util/rbtree.h"

static struct memcache *global_cache;

struct memcache_element {
	struct rb_node rb_node;
	struct memcache_element *prev, *next;
	size_t keylength, valuelength;
	uint8 n;		/* This is really an enum, but save memory */
	char data[1];		/* placeholder for offsetof */
};

struct memcache {
	struct memcache_element *mru;
	struct rb_root tree;
	size_t size;
	size_t max_size;
};

static void memcache_element_parse(struct memcache_element *e,
				   DATA_BLOB *key, DATA_BLOB *value);

static bool memcache_is_talloc(enum memcache_number n)
{
	bool result;

	switch (n) {
	case GETPWNAM_CACHE:
	case PDB_GETPWSID_CACHE:
	case SINGLETON_CACHE_TALLOC:
		result = true;
		break;
	default:
		result = false;
		break;
	}

	return result;
}

static int memcache_destructor(struct memcache *cache) {
	struct memcache_element *e, *next;

	for (e = cache->mru; e != NULL; e = next) {
		next = e->next;
		SAFE_FREE(e);
	}
	return 0;
}

struct memcache *memcache_init(TALLOC_CTX *mem_ctx, size_t max_size)
{
	struct memcache *result;

	result = TALLOC_ZERO_P(mem_ctx, struct memcache);
	if (result == NULL) {
		return NULL;
	}
	result->max_size = max_size;
	talloc_set_destructor(result, memcache_destructor);
	return result;
}

void memcache_set_global(struct memcache *cache)
{
	TALLOC_FREE(global_cache);
	global_cache = cache;
}

static struct memcache_element *memcache_node2elem(struct rb_node *node)
{
	return (struct memcache_element *)
		((char *)node - offsetof(struct memcache_element, rb_node));
}

static void memcache_element_parse(struct memcache_element *e,
				   DATA_BLOB *key, DATA_BLOB *value)
{
	key->data = ((uint8 *)e) + offsetof(struct memcache_element, data);
	key->length = e->keylength;
	value->data = key->data + e->keylength;
	value->length = e->valuelength;
}

static size_t memcache_element_size(size_t key_length, size_t value_length)
{
	return sizeof(struct memcache_element) - 1 + key_length + value_length;
}

static int memcache_compare(struct memcache_element *e, enum memcache_number n,
			    DATA_BLOB key)
{
	DATA_BLOB this_key, this_value;

	if ((int)e->n < (int)n) return 1;
	if ((int)e->n > (int)n) return -1;

	if (e->keylength < key.length) return 1;
	if (e->keylength > key.length) return -1;

	memcache_element_parse(e, &this_key, &this_value);
	return memcmp(this_key.data, key.data, key.length);
}

static struct memcache_element *memcache_find(
	struct memcache *cache, enum memcache_number n, DATA_BLOB key)
{
	struct rb_node *node;

	node = cache->tree.rb_node;

	while (node != NULL) {
		struct memcache_element *elem = memcache_node2elem(node);
		int cmp;

		cmp = memcache_compare(elem, n, key);
		if (cmp == 0) {
			return elem;
		}
		node = (cmp < 0) ? node->rb_left : node->rb_right;
	}

	return NULL;
}

bool memcache_lookup(struct memcache *cache, enum memcache_number n,
		     DATA_BLOB key, DATA_BLOB *value)
{
	struct memcache_element *e;

	if (cache == NULL) {
		cache = global_cache;
	}
	if (cache == NULL) {
		return false;
	}

	e = memcache_find(cache, n, key);
	if (e == NULL) {
		return false;
	}

	if (cache->size != 0) {
		DLIST_PROMOTE(cache->mru, e);
	}

	memcache_element_parse(e, &key, value);
	return true;
}

void *memcache_lookup_talloc(struct memcache *cache, enum memcache_number n,
			     DATA_BLOB key)
{
	DATA_BLOB value;
	void *result;

	if (!memcache_lookup(cache, n, key, &value)) {
		return NULL;
	}

	if (value.length != sizeof(result)) {
		return NULL;
	}

	memcpy(&result, value.data, sizeof(result));

	return result;
}

static void memcache_delete_element(struct memcache *cache,
				    struct memcache_element *e)
{
	rb_erase(&e->rb_node, &cache->tree);

	DLIST_REMOVE(cache->mru, e);

	if (memcache_is_talloc(e->n)) {
		DATA_BLOB cache_key, cache_value;
		void *ptr;

		memcache_element_parse(e, &cache_key, &cache_value);
		SMB_ASSERT(cache_value.length == sizeof(ptr));
		memcpy(&ptr, cache_value.data, sizeof(ptr));
		TALLOC_FREE(ptr);
	}

	cache->size -= memcache_element_size(e->keylength, e->valuelength);

	SAFE_FREE(e);
}

static void memcache_trim(struct memcache *cache)
{
	if (cache->max_size == 0) {
		return;
	}

	while ((cache->size > cache->max_size) && DLIST_TAIL(cache->mru)) {
		memcache_delete_element(cache, DLIST_TAIL(cache->mru));
	}
}

void memcache_delete(struct memcache *cache, enum memcache_number n,
		     DATA_BLOB key)
{
	struct memcache_element *e;

	if (cache == NULL) {
		cache = global_cache;
	}
	if (cache == NULL) {
		return;
	}

	e = memcache_find(cache, n, key);
	if (e == NULL) {
		return;
	}

	memcache_delete_element(cache, e);
}

void memcache_add(struct memcache *cache, enum memcache_number n,
		  DATA_BLOB key, DATA_BLOB value)
{
	struct memcache_element *e;
	struct rb_node **p;
	struct rb_node *parent;
	DATA_BLOB cache_key, cache_value;
	size_t element_size;

	if (cache == NULL) {
		cache = global_cache;
	}
	if (cache == NULL) {
		return;
	}

	if (key.length == 0) {
		return;
	}

	e = memcache_find(cache, n, key);

	if (e != NULL) {
		memcache_element_parse(e, &cache_key, &cache_value);

		if (value.length <= cache_value.length) {
			if (memcache_is_talloc(e->n)) {
				void *ptr;
				SMB_ASSERT(cache_value.length == sizeof(ptr));
				memcpy(&ptr, cache_value.data, sizeof(ptr));
				TALLOC_FREE(ptr);
			}
			/*
			 * We can reuse the existing record
			 */
			memcpy(cache_value.data, value.data, value.length);
			e->valuelength = value.length;
			return;
		}

		memcache_delete_element(cache, e);
	}

	element_size = memcache_element_size(key.length, value.length);


	e = (struct memcache_element *)SMB_MALLOC(element_size);

	if (e == NULL) {
		DEBUG(0, ("malloc failed\n"));
		return;
	}

	e->n = n;
	e->keylength = key.length;
	e->valuelength = value.length;

	memcache_element_parse(e, &cache_key, &cache_value);
	memcpy(cache_key.data, key.data, key.length);
	memcpy(cache_value.data, value.data, value.length);

	parent = NULL;
	p = &cache->tree.rb_node;

	while (*p) {
		struct memcache_element *elem = memcache_node2elem(*p);
		int cmp;

		parent = (*p);

		cmp = memcache_compare(elem, n, key);

		p = (cmp < 0) ? &(*p)->rb_left : &(*p)->rb_right;
	}

	rb_link_node(&e->rb_node, parent, p);
	rb_insert_color(&e->rb_node, &cache->tree);

	DLIST_ADD(cache->mru, e);

	cache->size += element_size;
	memcache_trim(cache);
}

void memcache_add_talloc(struct memcache *cache, enum memcache_number n,
			 DATA_BLOB key, void *pptr)
{
	void **ptr = (void **)pptr;
	void *p;

	if (cache == NULL) {
		cache = global_cache;
	}
	if (cache == NULL) {
		return;
	}

	p = talloc_move(cache, ptr);
	memcache_add(cache, n, key, data_blob_const(&p, sizeof(p)));
}

void memcache_flush(struct memcache *cache, enum memcache_number n)
{
	struct rb_node *node;

	if (cache == NULL) {
		cache = global_cache;
	}
	if (cache == NULL) {
		return;
	}

	/*
	 * Find the smallest element of number n
	 */

	node = cache->tree.rb_node;
	if (node == NULL) {
		return;
	}

	/*
	 * First, find *any* element of number n
	 */

	while (true) {
		struct memcache_element *elem = memcache_node2elem(node);
		struct rb_node *next;

		if ((int)elem->n == (int)n) {
			break;
		}

		if ((int)elem->n < (int)n) {
			next = node->rb_right;
		}
		else {
			next = node->rb_left;
		}
		if (next == NULL) {
			break;
		}
		node = next;
	}

	/*
	 * Then, find the leftmost element with number n
	 */

	while (true) {
		struct rb_node *prev = rb_prev(node);
		struct memcache_element *elem;

		if (prev == NULL) {
			break;
		}
		elem = memcache_node2elem(prev);
		if ((int)elem->n != (int)n) {
			break;
		}
		node = prev;
	}

	while (node != NULL) {
		struct memcache_element *e = memcache_node2elem(node);
		struct rb_node *next = rb_next(node);

		if (e->n != n) {
			break;
		}

		memcache_delete_element(cache, e);
		node = next;
	}
}
