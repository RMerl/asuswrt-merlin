/* Basic authentication token and access key management
 *
 * Copyright (C) 2004-2008 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poison.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/security.h>
#include <linux/workqueue.h>
#include <linux/random.h>
#include <linux/err.h>
#include <linux/user_namespace.h>
#include "internal.h"

static struct kmem_cache	*key_jar;
struct rb_root		key_serial_tree; /* tree of keys indexed by serial */
DEFINE_SPINLOCK(key_serial_lock);

struct rb_root	key_user_tree; /* tree of quota records indexed by UID */
DEFINE_SPINLOCK(key_user_lock);

unsigned int key_quota_root_maxkeys = 200;	/* root's key count quota */
unsigned int key_quota_root_maxbytes = 20000;	/* root's key space quota */
unsigned int key_quota_maxkeys = 200;		/* general key count quota */
unsigned int key_quota_maxbytes = 20000;	/* general key space quota */

static LIST_HEAD(key_types_list);
static DECLARE_RWSEM(key_types_sem);

static void key_cleanup(struct work_struct *work);
static DECLARE_WORK(key_cleanup_task, key_cleanup);

/* we serialise key instantiation and link */
DEFINE_MUTEX(key_construction_mutex);

/* any key who's type gets unegistered will be re-typed to this */
static struct key_type key_type_dead = {
	.name		= "dead",
};

#ifdef KEY_DEBUGGING
void __key_check(const struct key *key)
{
	printk("__key_check: key %p {%08x} should be {%08x}\n",
	       key, key->magic, KEY_DEBUG_MAGIC);
	BUG();
}
#endif

/*****************************************************************************/
/*
 * get the key quota record for a user, allocating a new record if one doesn't
 * already exist
 */
struct key_user *key_user_lookup(uid_t uid, struct user_namespace *user_ns)
{
	struct key_user *candidate = NULL, *user;
	struct rb_node *parent = NULL;
	struct rb_node **p;

 try_again:
	p = &key_user_tree.rb_node;
	spin_lock(&key_user_lock);

	/* search the tree for a user record with a matching UID */
	while (*p) {
		parent = *p;
		user = rb_entry(parent, struct key_user, node);

		if (uid < user->uid)
			p = &(*p)->rb_left;
		else if (uid > user->uid)
			p = &(*p)->rb_right;
		else if (user_ns < user->user_ns)
			p = &(*p)->rb_left;
		else if (user_ns > user->user_ns)
			p = &(*p)->rb_right;
		else
			goto found;
	}

	/* if we get here, we failed to find a match in the tree */
	if (!candidate) {
		/* allocate a candidate user record if we don't already have
		 * one */
		spin_unlock(&key_user_lock);

		user = NULL;
		candidate = kmalloc(sizeof(struct key_user), GFP_KERNEL);
		if (unlikely(!candidate))
			goto out;

		/* the allocation may have scheduled, so we need to repeat the
		 * search lest someone else added the record whilst we were
		 * asleep */
		goto try_again;
	}

	/* if we get here, then the user record still hadn't appeared on the
	 * second pass - so we use the candidate record */
	atomic_set(&candidate->usage, 1);
	atomic_set(&candidate->nkeys, 0);
	atomic_set(&candidate->nikeys, 0);
	candidate->uid = uid;
	candidate->user_ns = get_user_ns(user_ns);
	candidate->qnkeys = 0;
	candidate->qnbytes = 0;
	spin_lock_init(&candidate->lock);
	mutex_init(&candidate->cons_lock);

	rb_link_node(&candidate->node, parent, p);
	rb_insert_color(&candidate->node, &key_user_tree);
	spin_unlock(&key_user_lock);
	user = candidate;
	goto out;

	/* okay - we found a user record for this UID */
 found:
	atomic_inc(&user->usage);
	spin_unlock(&key_user_lock);
	kfree(candidate);
 out:
	return user;

} /* end key_user_lookup() */

/*****************************************************************************/
/*
 * dispose of a user structure
 */
void key_user_put(struct key_user *user)
{
	if (atomic_dec_and_lock(&user->usage, &key_user_lock)) {
		rb_erase(&user->node, &key_user_tree);
		spin_unlock(&key_user_lock);
		put_user_ns(user->user_ns);

		kfree(user);
	}

} /* end key_user_put() */

/*****************************************************************************/
/*
 * assign a key the next unique serial number
 * - these are assigned randomly to avoid security issues through covert
 *   channel problems
 */
static inline void key_alloc_serial(struct key *key)
{
	struct rb_node *parent, **p;
	struct key *xkey;

	/* propose a random serial number and look for a hole for it in the
	 * serial number tree */
	do {
		get_random_bytes(&key->serial, sizeof(key->serial));

		key->serial >>= 1; /* negative numbers are not permitted */
	} while (key->serial < 3);

	spin_lock(&key_serial_lock);

attempt_insertion:
	parent = NULL;
	p = &key_serial_tree.rb_node;

	while (*p) {
		parent = *p;
		xkey = rb_entry(parent, struct key, serial_node);

		if (key->serial < xkey->serial)
			p = &(*p)->rb_left;
		else if (key->serial > xkey->serial)
			p = &(*p)->rb_right;
		else
			goto serial_exists;
	}

	/* we've found a suitable hole - arrange for this key to occupy it */
	rb_link_node(&key->serial_node, parent, p);
	rb_insert_color(&key->serial_node, &key_serial_tree);

	spin_unlock(&key_serial_lock);
	return;

	/* we found a key with the proposed serial number - walk the tree from
	 * that point looking for the next unused serial number */
serial_exists:
	for (;;) {
		key->serial++;
		if (key->serial < 3) {
			key->serial = 3;
			goto attempt_insertion;
		}

		parent = rb_next(parent);
		if (!parent)
			goto attempt_insertion;

		xkey = rb_entry(parent, struct key, serial_node);
		if (key->serial < xkey->serial)
			goto attempt_insertion;
	}

} /* end key_alloc_serial() */

/*****************************************************************************/
/*
 * allocate a key of the specified type
 * - update the user's quota to reflect the existence of the key
 * - called from a key-type operation with key_types_sem read-locked by
 *   key_create_or_update()
 *   - this prevents unregistration of the key type
 * - upon return the key is as yet uninstantiated; the caller needs to either
 *   instantiate the key or discard it before returning
 */
struct key *key_alloc(struct key_type *type, const char *desc,
		      uid_t uid, gid_t gid, const struct cred *cred,
		      key_perm_t perm, unsigned long flags)
{
	struct key_user *user = NULL;
	struct key *key;
	size_t desclen, quotalen;
	int ret;

	key = ERR_PTR(-EINVAL);
	if (!desc || !*desc)
		goto error;

	desclen = strlen(desc) + 1;
	quotalen = desclen + type->def_datalen;

	/* get hold of the key tracking for this user */
	user = key_user_lookup(uid, cred->user->user_ns);
	if (!user)
		goto no_memory_1;

	/* check that the user's quota permits allocation of another key and
	 * its description */
	if (!(flags & KEY_ALLOC_NOT_IN_QUOTA)) {
		unsigned maxkeys = (uid == 0) ?
			key_quota_root_maxkeys : key_quota_maxkeys;
		unsigned maxbytes = (uid == 0) ?
			key_quota_root_maxbytes : key_quota_maxbytes;

		spin_lock(&user->lock);
		if (!(flags & KEY_ALLOC_QUOTA_OVERRUN)) {
			if (user->qnkeys + 1 >= maxkeys ||
			    user->qnbytes + quotalen >= maxbytes ||
			    user->qnbytes + quotalen < user->qnbytes)
				goto no_quota;
		}

		user->qnkeys++;
		user->qnbytes += quotalen;
		spin_unlock(&user->lock);
	}

	/* allocate and initialise the key and its description */
	key = kmem_cache_alloc(key_jar, GFP_KERNEL);
	if (!key)
		goto no_memory_2;

	if (desc) {
		key->description = kmemdup(desc, desclen, GFP_KERNEL);
		if (!key->description)
			goto no_memory_3;
	}

	atomic_set(&key->usage, 1);
	init_rwsem(&key->sem);
	key->type = type;
	key->user = user;
	key->quotalen = quotalen;
	key->datalen = type->def_datalen;
	key->uid = uid;
	key->gid = gid;
	key->perm = perm;
	key->flags = 0;
	key->expiry = 0;
	key->payload.data = NULL;
	key->security = NULL;

	if (!(flags & KEY_ALLOC_NOT_IN_QUOTA))
		key->flags |= 1 << KEY_FLAG_IN_QUOTA;

	memset(&key->type_data, 0, sizeof(key->type_data));

#ifdef KEY_DEBUGGING
	key->magic = KEY_DEBUG_MAGIC;
#endif

	/* let the security module know about the key */
	ret = security_key_alloc(key, cred, flags);
	if (ret < 0)
		goto security_error;

	/* publish the key by giving it a serial number */
	atomic_inc(&user->nkeys);
	key_alloc_serial(key);

error:
	return key;

security_error:
	kfree(key->description);
	kmem_cache_free(key_jar, key);
	if (!(flags & KEY_ALLOC_NOT_IN_QUOTA)) {
		spin_lock(&user->lock);
		user->qnkeys--;
		user->qnbytes -= quotalen;
		spin_unlock(&user->lock);
	}
	key_user_put(user);
	key = ERR_PTR(ret);
	goto error;

no_memory_3:
	kmem_cache_free(key_jar, key);
no_memory_2:
	if (!(flags & KEY_ALLOC_NOT_IN_QUOTA)) {
		spin_lock(&user->lock);
		user->qnkeys--;
		user->qnbytes -= quotalen;
		spin_unlock(&user->lock);
	}
	key_user_put(user);
no_memory_1:
	key = ERR_PTR(-ENOMEM);
	goto error;

no_quota:
	spin_unlock(&user->lock);
	key_user_put(user);
	key = ERR_PTR(-EDQUOT);
	goto error;

} /* end key_alloc() */

EXPORT_SYMBOL(key_alloc);

/*****************************************************************************/
/*
 * reserve an amount of quota for the key's payload
 */
int key_payload_reserve(struct key *key, size_t datalen)
{
	int delta = (int)datalen - key->datalen;
	int ret = 0;

	key_check(key);

	/* contemplate the quota adjustment */
	if (delta != 0 && test_bit(KEY_FLAG_IN_QUOTA, &key->flags)) {
		unsigned maxbytes = (key->user->uid == 0) ?
			key_quota_root_maxbytes : key_quota_maxbytes;

		spin_lock(&key->user->lock);

		if (delta > 0 &&
		    (key->user->qnbytes + delta >= maxbytes ||
		     key->user->qnbytes + delta < key->user->qnbytes)) {
			ret = -EDQUOT;
		}
		else {
			key->user->qnbytes += delta;
			key->quotalen += delta;
		}
		spin_unlock(&key->user->lock);
	}

	/* change the recorded data length if that didn't generate an error */
	if (ret == 0)
		key->datalen = datalen;

	return ret;

} /* end key_payload_reserve() */

EXPORT_SYMBOL(key_payload_reserve);

/*****************************************************************************/
/*
 * instantiate a key and link it into the target keyring atomically
 * - called with the target keyring's semaphore writelocked
 */
static int __key_instantiate_and_link(struct key *key,
				      const void *data,
				      size_t datalen,
				      struct key *keyring,
				      struct key *authkey,
				      struct keyring_list **_prealloc)
{
	int ret, awaken;

	key_check(key);
	key_check(keyring);

	awaken = 0;
	ret = -EBUSY;

	mutex_lock(&key_construction_mutex);

	/* can't instantiate twice */
	if (!test_bit(KEY_FLAG_INSTANTIATED, &key->flags)) {
		/* instantiate the key */
		ret = key->type->instantiate(key, data, datalen);

		if (ret == 0) {
			/* mark the key as being instantiated */
			atomic_inc(&key->user->nikeys);
			set_bit(KEY_FLAG_INSTANTIATED, &key->flags);

			if (test_and_clear_bit(KEY_FLAG_USER_CONSTRUCT, &key->flags))
				awaken = 1;

			/* and link it into the destination keyring */
			if (keyring)
				__key_link(keyring, key, _prealloc);

			/* disable the authorisation key */
			if (authkey)
				key_revoke(authkey);
		}
	}

	mutex_unlock(&key_construction_mutex);

	/* wake up anyone waiting for a key to be constructed */
	if (awaken)
		wake_up_bit(&key->flags, KEY_FLAG_USER_CONSTRUCT);

	return ret;

} /* end __key_instantiate_and_link() */

/*****************************************************************************/
/*
 * instantiate a key and link it into the target keyring atomically
 */
int key_instantiate_and_link(struct key *key,
			     const void *data,
			     size_t datalen,
			     struct key *keyring,
			     struct key *authkey)
{
	struct keyring_list *prealloc;
	int ret;

	if (keyring) {
		ret = __key_link_begin(keyring, key->type, key->description,
				       &prealloc);
		if (ret < 0)
			return ret;
	}

	ret = __key_instantiate_and_link(key, data, datalen, keyring, authkey,
					 &prealloc);

	if (keyring)
		__key_link_end(keyring, key->type, prealloc);

	return ret;

} /* end key_instantiate_and_link() */

EXPORT_SYMBOL(key_instantiate_and_link);

/*****************************************************************************/
/*
 * negatively instantiate a key and link it into the target keyring atomically
 */
int key_negate_and_link(struct key *key,
			unsigned timeout,
			struct key *keyring,
			struct key *authkey)
{
	struct keyring_list *prealloc;
	struct timespec now;
	int ret, awaken, link_ret = 0;

	key_check(key);
	key_check(keyring);

	awaken = 0;
	ret = -EBUSY;

	if (keyring)
		link_ret = __key_link_begin(keyring, key->type,
					    key->description, &prealloc);

	mutex_lock(&key_construction_mutex);

	/* can't instantiate twice */
	if (!test_bit(KEY_FLAG_INSTANTIATED, &key->flags)) {
		/* mark the key as being negatively instantiated */
		atomic_inc(&key->user->nikeys);
		set_bit(KEY_FLAG_NEGATIVE, &key->flags);
		set_bit(KEY_FLAG_INSTANTIATED, &key->flags);
		now = current_kernel_time();
		key->expiry = now.tv_sec + timeout;
		key_schedule_gc(key->expiry + key_gc_delay);

		if (test_and_clear_bit(KEY_FLAG_USER_CONSTRUCT, &key->flags))
			awaken = 1;

		ret = 0;

		/* and link it into the destination keyring */
		if (keyring && link_ret == 0)
			__key_link(keyring, key, &prealloc);

		/* disable the authorisation key */
		if (authkey)
			key_revoke(authkey);
	}

	mutex_unlock(&key_construction_mutex);

	if (keyring)
		__key_link_end(keyring, key->type, prealloc);

	/* wake up anyone waiting for a key to be constructed */
	if (awaken)
		wake_up_bit(&key->flags, KEY_FLAG_USER_CONSTRUCT);

	return ret == 0 ? link_ret : ret;

} /* end key_negate_and_link() */

EXPORT_SYMBOL(key_negate_and_link);

/*****************************************************************************/
/*
 * do cleaning up in process context so that we don't have to disable
 * interrupts all over the place
 */
static void key_cleanup(struct work_struct *work)
{
	struct rb_node *_n;
	struct key *key;

 go_again:
	/* look for a dead key in the tree */
	spin_lock(&key_serial_lock);

	for (_n = rb_first(&key_serial_tree); _n; _n = rb_next(_n)) {
		key = rb_entry(_n, struct key, serial_node);

		if (atomic_read(&key->usage) == 0)
			goto found_dead_key;
	}

	spin_unlock(&key_serial_lock);
	return;

 found_dead_key:
	/* we found a dead key - once we've removed it from the tree, we can
	 * drop the lock */
	rb_erase(&key->serial_node, &key_serial_tree);
	spin_unlock(&key_serial_lock);

	key_check(key);

	security_key_free(key);

	/* deal with the user's key tracking and quota */
	if (test_bit(KEY_FLAG_IN_QUOTA, &key->flags)) {
		spin_lock(&key->user->lock);
		key->user->qnkeys--;
		key->user->qnbytes -= key->quotalen;
		spin_unlock(&key->user->lock);
	}

	atomic_dec(&key->user->nkeys);
	if (test_bit(KEY_FLAG_INSTANTIATED, &key->flags))
		atomic_dec(&key->user->nikeys);

	key_user_put(key->user);

	/* now throw away the key memory */
	if (key->type->destroy)
		key->type->destroy(key);

	kfree(key->description);

#ifdef KEY_DEBUGGING
	key->magic = KEY_DEBUG_MAGIC_X;
#endif
	kmem_cache_free(key_jar, key);

	/* there may, of course, be more than one key to destroy */
	goto go_again;

} /* end key_cleanup() */

/*****************************************************************************/
/*
 * dispose of a reference to a key
 * - when all the references are gone, we schedule the cleanup task to come and
 *   pull it out of the tree in definite process context
 */
void key_put(struct key *key)
{
	if (key) {
		key_check(key);

		if (atomic_dec_and_test(&key->usage))
			schedule_work(&key_cleanup_task);
	}

} /* end key_put() */

EXPORT_SYMBOL(key_put);

/*****************************************************************************/
/*
 * find a key by its serial number
 */
struct key *key_lookup(key_serial_t id)
{
	struct rb_node *n;
	struct key *key;

	spin_lock(&key_serial_lock);

	/* search the tree for the specified key */
	n = key_serial_tree.rb_node;
	while (n) {
		key = rb_entry(n, struct key, serial_node);

		if (id < key->serial)
			n = n->rb_left;
		else if (id > key->serial)
			n = n->rb_right;
		else
			goto found;
	}

 not_found:
	key = ERR_PTR(-ENOKEY);
	goto error;

 found:
	/* pretend it doesn't exist if it is awaiting deletion */
	if (atomic_read(&key->usage) == 0)
		goto not_found;

	/* this races with key_put(), but that doesn't matter since key_put()
	 * doesn't actually change the key
	 */
	atomic_inc(&key->usage);

 error:
	spin_unlock(&key_serial_lock);
	return key;

} /* end key_lookup() */

/*****************************************************************************/
/*
 * find and lock the specified key type against removal
 * - we return with the sem readlocked
 */
struct key_type *key_type_lookup(const char *type)
{
	struct key_type *ktype;

	down_read(&key_types_sem);

	/* look up the key type to see if it's one of the registered kernel
	 * types */
	list_for_each_entry(ktype, &key_types_list, link) {
		if (strcmp(ktype->name, type) == 0)
			goto found_kernel_type;
	}

	up_read(&key_types_sem);
	ktype = ERR_PTR(-ENOKEY);

 found_kernel_type:
	return ktype;

} /* end key_type_lookup() */

/*****************************************************************************/
/*
 * unlock a key type
 */
void key_type_put(struct key_type *ktype)
{
	up_read(&key_types_sem);

} /* end key_type_put() */

/*****************************************************************************/
/*
 * attempt to update an existing key
 * - the key has an incremented refcount
 * - we need to put the key if we get an error
 */
static inline key_ref_t __key_update(key_ref_t key_ref,
				     const void *payload, size_t plen)
{
	struct key *key = key_ref_to_ptr(key_ref);
	int ret;

	/* need write permission on the key to update it */
	ret = key_permission(key_ref, KEY_WRITE);
	if (ret < 0)
		goto error;

	ret = -EEXIST;
	if (!key->type->update)
		goto error;

	down_write(&key->sem);

	ret = key->type->update(key, payload, plen);
	if (ret == 0)
		/* updating a negative key instantiates it */
		clear_bit(KEY_FLAG_NEGATIVE, &key->flags);

	up_write(&key->sem);

	if (ret < 0)
		goto error;
out:
	return key_ref;

error:
	key_put(key);
	key_ref = ERR_PTR(ret);
	goto out;

} /* end __key_update() */

/*****************************************************************************/
/*
 * search the specified keyring for a key of the same description; if one is
 * found, update it, otherwise add a new one
 */
key_ref_t key_create_or_update(key_ref_t keyring_ref,
			       const char *type,
			       const char *description,
			       const void *payload,
			       size_t plen,
			       key_perm_t perm,
			       unsigned long flags)
{
	struct keyring_list *prealloc;
	const struct cred *cred = current_cred();
	struct key_type *ktype;
	struct key *keyring, *key = NULL;
	key_ref_t key_ref;
	int ret;

	/* look up the key type to see if it's one of the registered kernel
	 * types */
	ktype = key_type_lookup(type);
	if (IS_ERR(ktype)) {
		key_ref = ERR_PTR(-ENODEV);
		goto error;
	}

	key_ref = ERR_PTR(-EINVAL);
	if (!ktype->match || !ktype->instantiate)
		goto error_2;

	keyring = key_ref_to_ptr(keyring_ref);

	key_check(keyring);

	key_ref = ERR_PTR(-ENOTDIR);
	if (keyring->type != &key_type_keyring)
		goto error_2;

	ret = __key_link_begin(keyring, ktype, description, &prealloc);
	if (ret < 0)
		goto error_2;

	/* if we're going to allocate a new key, we're going to have
	 * to modify the keyring */
	ret = key_permission(keyring_ref, KEY_WRITE);
	if (ret < 0) {
		key_ref = ERR_PTR(ret);
		goto error_3;
	}

	/* if it's possible to update this type of key, search for an existing
	 * key of the same type and description in the destination keyring and
	 * update that instead if possible
	 */
	if (ktype->update) {
		key_ref = __keyring_search_one(keyring_ref, ktype, description,
					       0);
		if (!IS_ERR(key_ref))
			goto found_matching_key;
	}

	/* if the client doesn't provide, decide on the permissions we want */
	if (perm == KEY_PERM_UNDEF) {
		perm = KEY_POS_VIEW | KEY_POS_SEARCH | KEY_POS_LINK | KEY_POS_SETATTR;
		perm |= KEY_USR_VIEW | KEY_USR_SEARCH | KEY_USR_LINK | KEY_USR_SETATTR;

		if (ktype->read)
			perm |= KEY_POS_READ | KEY_USR_READ;

		if (ktype == &key_type_keyring || ktype->update)
			perm |= KEY_USR_WRITE;
	}

	/* allocate a new key */
	key = key_alloc(ktype, description, cred->fsuid, cred->fsgid, cred,
			perm, flags);
	if (IS_ERR(key)) {
		key_ref = ERR_CAST(key);
		goto error_3;
	}

	/* instantiate it and link it into the target keyring */
	ret = __key_instantiate_and_link(key, payload, plen, keyring, NULL,
					 &prealloc);
	if (ret < 0) {
		key_put(key);
		key_ref = ERR_PTR(ret);
		goto error_3;
	}

	key_ref = make_key_ref(key, is_key_possessed(keyring_ref));

 error_3:
	__key_link_end(keyring, ktype, prealloc);
 error_2:
	key_type_put(ktype);
 error:
	return key_ref;

 found_matching_key:
	/* we found a matching key, so we're going to try to update it
	 * - we can drop the locks first as we have the key pinned
	 */
	__key_link_end(keyring, ktype, prealloc);
	key_type_put(ktype);

	key_ref = __key_update(key_ref, payload, plen);
	goto error;

} /* end key_create_or_update() */

EXPORT_SYMBOL(key_create_or_update);

/*****************************************************************************/
/*
 * update a key
 */
int key_update(key_ref_t key_ref, const void *payload, size_t plen)
{
	struct key *key = key_ref_to_ptr(key_ref);
	int ret;

	key_check(key);

	/* the key must be writable */
	ret = key_permission(key_ref, KEY_WRITE);
	if (ret < 0)
		goto error;

	/* attempt to update it if supported */
	ret = -EOPNOTSUPP;
	if (key->type->update) {
		down_write(&key->sem);

		ret = key->type->update(key, payload, plen);
		if (ret == 0)
			/* updating a negative key instantiates it */
			clear_bit(KEY_FLAG_NEGATIVE, &key->flags);

		up_write(&key->sem);
	}

 error:
	return ret;

} /* end key_update() */

EXPORT_SYMBOL(key_update);

/*****************************************************************************/
/*
 * revoke a key
 */
void key_revoke(struct key *key)
{
	struct timespec now;
	time_t time;

	key_check(key);

	/* make sure no one's trying to change or use the key when we mark it
	 * - we tell lockdep that we might nest because we might be revoking an
	 *   authorisation key whilst holding the sem on a key we've just
	 *   instantiated
	 */
	down_write_nested(&key->sem, 1);
	if (!test_and_set_bit(KEY_FLAG_REVOKED, &key->flags) &&
	    key->type->revoke)
		key->type->revoke(key);

	/* set the death time to no more than the expiry time */
	now = current_kernel_time();
	time = now.tv_sec;
	if (key->revoked_at == 0 || key->revoked_at > time) {
		key->revoked_at = time;
		key_schedule_gc(key->revoked_at + key_gc_delay);
	}

	up_write(&key->sem);

} /* end key_revoke() */

EXPORT_SYMBOL(key_revoke);

/*****************************************************************************/
/*
 * register a type of key
 */
int register_key_type(struct key_type *ktype)
{
	struct key_type *p;
	int ret;

	ret = -EEXIST;
	down_write(&key_types_sem);

	/* disallow key types with the same name */
	list_for_each_entry(p, &key_types_list, link) {
		if (strcmp(p->name, ktype->name) == 0)
			goto out;
	}

	/* store the type */
	list_add(&ktype->link, &key_types_list);
	ret = 0;

 out:
	up_write(&key_types_sem);
	return ret;

} /* end register_key_type() */

EXPORT_SYMBOL(register_key_type);

/*****************************************************************************/
/*
 * unregister a type of key
 */
void unregister_key_type(struct key_type *ktype)
{
	struct rb_node *_n;
	struct key *key;

	down_write(&key_types_sem);

	/* withdraw the key type */
	list_del_init(&ktype->link);

	/* mark all the keys of this type dead */
	spin_lock(&key_serial_lock);

	for (_n = rb_first(&key_serial_tree); _n; _n = rb_next(_n)) {
		key = rb_entry(_n, struct key, serial_node);

		if (key->type == ktype) {
			key->type = &key_type_dead;
			set_bit(KEY_FLAG_DEAD, &key->flags);
		}
	}

	spin_unlock(&key_serial_lock);

	/* make sure everyone revalidates their keys */
	synchronize_rcu();

	/* we should now be able to destroy the payloads of all the keys of
	 * this type with impunity */
	spin_lock(&key_serial_lock);

	for (_n = rb_first(&key_serial_tree); _n; _n = rb_next(_n)) {
		key = rb_entry(_n, struct key, serial_node);

		if (key->type == ktype) {
			if (ktype->destroy)
				ktype->destroy(key);
			memset(&key->payload, KEY_DESTROY, sizeof(key->payload));
		}
	}

	spin_unlock(&key_serial_lock);
	up_write(&key_types_sem);

	key_schedule_gc(0);

} /* end unregister_key_type() */

EXPORT_SYMBOL(unregister_key_type);

/*****************************************************************************/
/*
 * initialise the key management stuff
 */
void __init key_init(void)
{
	/* allocate a slab in which we can store keys */
	key_jar = kmem_cache_create("key_jar", sizeof(struct key),
			0, SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL);

	/* add the special key types */
	list_add_tail(&key_type_keyring.link, &key_types_list);
	list_add_tail(&key_type_dead.link, &key_types_list);
	list_add_tail(&key_type_user.link, &key_types_list);

	/* record the root user tracking */
	rb_link_node(&root_key_user.node,
		     NULL,
		     &key_user_tree.rb_node);

	rb_insert_color(&root_key_user.node,
			&key_user_tree);

} /* end key_init() */
