/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * hash.c
 *
 * Routines to handle simple hash table lookups and modifications.
 */

#include "hash.h"
#include "sysutil.h"
#include "utility.h"

struct hash_node
{
  void* p_key;
  void* p_value;
  struct hash_node* p_prev;
  struct hash_node* p_next;
};

struct hash
{
  unsigned int buckets;
  unsigned int key_size;
  unsigned int value_size;
  hashfunc_t hash_func;
  struct hash_node** p_nodes;
};

/* Internal functions */
struct hash_node** hash_get_bucket(struct hash* p_hash, void* p_key);
struct hash_node* hash_get_node_by_key(struct hash* p_hash, void* p_key);

struct hash*
hash_alloc(unsigned int buckets, unsigned int key_size,
           unsigned int value_size, hashfunc_t hash_func)
{
  unsigned int size;
  struct hash* p_hash = vsf_sysutil_malloc(sizeof(*p_hash));
  p_hash->buckets = buckets;
  p_hash->key_size = key_size;
  p_hash->value_size = value_size;
  p_hash->hash_func = hash_func;
  size = (unsigned int) sizeof(struct hash_node*) * buckets;
  p_hash->p_nodes = vsf_sysutil_malloc(size);
  vsf_sysutil_memclr(p_hash->p_nodes, size);
  return p_hash;
}

void*
hash_lookup_entry(struct hash* p_hash, void* p_key)
{
  struct hash_node* p_node = hash_get_node_by_key(p_hash, p_key);
  if (!p_node)
  {
    return p_node;
  }
  return p_node->p_value;
}

void
hash_add_entry(struct hash* p_hash, void* p_key, void* p_value)
{
  struct hash_node** p_bucket;
  struct hash_node* p_new_node;
  if (hash_lookup_entry(p_hash, p_key))
  {
    bug("duplicate hash key");
  }
  p_bucket = hash_get_bucket(p_hash, p_key);
  p_new_node = vsf_sysutil_malloc(sizeof(*p_new_node));
  p_new_node->p_prev = 0;
  p_new_node->p_next = 0;
  p_new_node->p_key = vsf_sysutil_malloc(p_hash->key_size);
  vsf_sysutil_memcpy(p_new_node->p_key, p_key, p_hash->key_size);
  p_new_node->p_value = vsf_sysutil_malloc(p_hash->value_size);
  vsf_sysutil_memcpy(p_new_node->p_value, p_value, p_hash->value_size);

  if (!*p_bucket)
  {
    *p_bucket = p_new_node;    
  }
  else
  {
    p_new_node->p_next = *p_bucket;
    (*p_bucket)->p_prev = p_new_node;
    *p_bucket = p_new_node;
  }
}

void
hash_free_entry(struct hash* p_hash, void* p_key)
{
  struct hash_node* p_node = hash_get_node_by_key(p_hash, p_key);
  if (!p_node)
  {
    bug("hash node not found");
  }
  vsf_sysutil_free(p_node->p_key);
  vsf_sysutil_free(p_node->p_value);

  if (p_node->p_prev)
  {
    p_node->p_prev->p_next = p_node->p_next;
  }
  else
  {
    struct hash_node** p_bucket = hash_get_bucket(p_hash, p_key);
    *p_bucket = p_node->p_next;
  }
  if (p_node->p_next)
  {
    p_node->p_next->p_prev = p_node->p_prev;
  }

  vsf_sysutil_free(p_node);
}

struct hash_node**
hash_get_bucket(struct hash* p_hash, void* p_key)
{
  unsigned int bucket = (*p_hash->hash_func)(p_hash->buckets, p_key);
  if (bucket >= p_hash->buckets)
  {
    bug("bad bucket lookup");
  }
  return &(p_hash->p_nodes[bucket]);
}

struct hash_node*
hash_get_node_by_key(struct hash* p_hash, void* p_key)
{
  struct hash_node** p_bucket = hash_get_bucket(p_hash, p_key);
  struct hash_node* p_node = *p_bucket;
  if (!p_node)
  {
    return p_node;
  }
  while (p_node != 0 &&
         vsf_sysutil_memcmp(p_key, p_node->p_key, p_hash->key_size) != 0)
  {
    p_node = p_node->p_next;
  }
  return p_node;
}

