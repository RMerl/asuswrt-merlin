/* $Id: lock.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/lock.h>
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/errno.h>

#define THIS_FILE	"lock.c"

typedef void LOCK_OBJ;

/*
 * Lock structure.
 */
struct pj_lock_t
{
    LOCK_OBJ *lock_object;

    pj_status_t	(*acquire)	(LOCK_OBJ*);
    pj_status_t	(*tryacquire)	(LOCK_OBJ*);
    pj_status_t	(*release)	(LOCK_OBJ*);
    pj_status_t	(*destroy)	(LOCK_OBJ*);
};

typedef pj_status_t (*FPTR)(LOCK_OBJ*);

/******************************************************************************
 * Implementation of lock object with mutex.
 */
static pj_lock_t mutex_lock_template = 
{
    NULL,
    (FPTR) &pj_mutex_lock,
    (FPTR) &pj_mutex_trylock,
    (FPTR) &pj_mutex_unlock,
    (FPTR) &pj_mutex_destroy
};

static pj_status_t create_mutex_lock( pj_pool_t *pool,
				      const char *name,
				      int type,
				      pj_lock_t **lock )
{
    pj_lock_t *p_lock;
    pj_mutex_t *mutex;
    pj_status_t rc;

    PJ_ASSERT_RETURN(pool && lock, PJ_EINVAL);

    p_lock = PJ_POOL_ALLOC_T(pool, pj_lock_t);
    if (!p_lock)
	return PJ_ENOMEM;

    pj_memcpy(p_lock, &mutex_lock_template, sizeof(pj_lock_t));
    rc = pj_mutex_create(pool, name, type, &mutex);
    if (rc != PJ_SUCCESS)
	return rc;

    p_lock->lock_object = mutex;
    *lock = p_lock;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_lock_create_simple_mutex( pj_pool_t *pool,
						 const char *name,
						 pj_lock_t **lock )
{
    return create_mutex_lock(pool, name, PJ_MUTEX_SIMPLE, lock);
}

PJ_DEF(pj_status_t) pj_lock_create_recursive_mutex( pj_pool_t *pool,
						    const char *name,
						    pj_lock_t **lock )
{
    return create_mutex_lock(pool, name, PJ_MUTEX_RECURSE, lock);
}


/******************************************************************************
 * Implementation of NULL lock object.
 */
static pj_status_t null_op(void *arg)
{
    PJ_UNUSED_ARG(arg);
    return PJ_SUCCESS;
}

static pj_lock_t null_lock_template = 
{
    NULL,
    &null_op,
    &null_op,
    &null_op,
    &null_op
};

PJ_DEF(pj_status_t) pj_lock_create_null_mutex( pj_pool_t *pool,
					       const char *name,
					       pj_lock_t **lock )
{
    PJ_UNUSED_ARG(name);
    PJ_UNUSED_ARG(pool);

    PJ_ASSERT_RETURN(lock, PJ_EINVAL);

    *lock = &null_lock_template;
    return PJ_SUCCESS;
}


/******************************************************************************
 * Implementation of semaphore lock object.
 */
#if defined(PJ_HAS_SEMAPHORE) && PJ_HAS_SEMAPHORE != 0

static pj_lock_t sem_lock_template = 
{
    NULL,
    (FPTR) &pj_sem_wait,
    (FPTR) &pj_sem_trywait,
    (FPTR) &pj_sem_post,
    (FPTR) &pj_sem_destroy
};

PJ_DEF(pj_status_t) pj_lock_create_semaphore(  pj_pool_t *pool,
					       const char *name,
					       unsigned initial,
					       unsigned max,
					       pj_lock_t **lock )
{
    pj_lock_t *p_lock;
    pj_sem_t *sem;
    pj_status_t rc;

    PJ_ASSERT_RETURN(pool && lock, PJ_EINVAL);

    p_lock = PJ_POOL_ALLOC_T(pool, pj_lock_t);
    if (!p_lock)
	return PJ_ENOMEM;

    pj_memcpy(p_lock, &sem_lock_template, sizeof(pj_lock_t));
    rc = pj_sem_create( pool, name, initial, max, &sem);
    if (rc != PJ_SUCCESS)
        return rc;

    p_lock->lock_object = sem;
    *lock = p_lock;

    return PJ_SUCCESS;
}


#endif	/* PJ_HAS_SEMAPHORE */


PJ_DEF(pj_status_t) pj_lock_acquire( pj_lock_t *lock )
{
    PJ_ASSERT_RETURN(lock != NULL, PJ_EINVAL);
    return (*lock->acquire)(lock->lock_object);
}

PJ_DEF(pj_status_t) pj_lock_tryacquire( pj_lock_t *lock )
{
    PJ_ASSERT_RETURN(lock != NULL, PJ_EINVAL);
    return (*lock->tryacquire)(lock->lock_object);
}

PJ_DEF(pj_status_t) pj_lock_release( pj_lock_t *lock )
{
    PJ_ASSERT_RETURN(lock != NULL, PJ_EINVAL);
    return (*lock->release)(lock->lock_object);
}

PJ_DEF(pj_status_t) pj_lock_destroy( pj_lock_t *lock )
{
    PJ_ASSERT_RETURN(lock != NULL, PJ_EINVAL);
    return (*lock->destroy)(lock->lock_object);
}


/******************************************************************************
 * Group lock
 */

/* Individual lock in the group lock */
typedef struct grp_lock_item
{
    PJ_DECL_LIST_MEMBER(struct grp_lock_item);
    int		 prio;
    pj_lock_t	*lock;

} grp_lock_item;

/* Destroy callbacks */
typedef struct grp_destroy_callback
{
    PJ_DECL_LIST_MEMBER(struct grp_destroy_callback);
    void	*comp;
    void	(*handler)(void*);
} grp_destroy_callback;

#if PJ_GRP_LOCK_DEBUG
/* Store each add_ref caller */
typedef struct grp_lock_ref
{
    PJ_DECL_LIST_MEMBER(struct grp_lock_ref);
    const char	*file;
    int		 line;
} grp_lock_ref;
#endif

/* The group lock */
struct pj_grp_lock_t
{
    pj_lock_t	 	 base;

    pj_pool_t		*pool;
    pj_atomic_t		*ref_cnt;
    pj_lock_t		*own_lock;

    pj_thread_t		*owner;
    int			 owner_cnt;

    grp_lock_item	 lock_list;
    grp_destroy_callback destroy_list;

#if PJ_GRP_LOCK_DEBUG
    grp_lock_ref	 ref_list;
    grp_lock_ref	 ref_free_list;
#endif
};


PJ_DEF(void) pj_grp_lock_config_default(pj_grp_lock_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
}

static void grp_lock_set_owner_thread(pj_grp_lock_t *glock)
{
    if (!glock->owner) {
	glock->owner = pj_thread_this(glock->pool->factory->inst_id);
	glock->owner_cnt = 1;
    } else {
	pj_assert(glock->owner == pj_thread_this(glock->pool->factory->inst_id));
	glock->owner_cnt++;
    }
}

static void grp_lock_unset_owner_thread(pj_grp_lock_t *glock)
{
    pj_assert(glock->owner == pj_thread_this(glock->pool->factory->inst_id));
    pj_assert(glock->owner_cnt > 0);
    if (--glock->owner_cnt <= 0) {
	glock->owner = NULL;
	glock->owner_cnt = 0;
    }
}

static pj_status_t grp_lock_acquire(LOCK_OBJ *p)
{
    pj_grp_lock_t *glock = (pj_grp_lock_t*)p;
    grp_lock_item *lck;

    pj_assert(pj_atomic_get(glock->ref_cnt) > 0);

    lck = glock->lock_list.next;
    while (lck != &glock->lock_list) {
	pj_lock_acquire(lck->lock);
	lck = lck->next;
    }
    grp_lock_set_owner_thread(glock);
    pj_grp_lock_add_ref(glock);
    return PJ_SUCCESS;
}

static pj_status_t grp_lock_tryacquire(LOCK_OBJ *p)
{
    pj_grp_lock_t *glock = (pj_grp_lock_t*)p;
    grp_lock_item *lck;

    pj_assert(pj_atomic_get(glock->ref_cnt) > 0);

    lck = glock->lock_list.next;
    while (lck != &glock->lock_list) {
	pj_status_t status = pj_lock_tryacquire(lck->lock);
	if (status != PJ_SUCCESS) {
	    lck = lck->prev;
	    while (lck != &glock->lock_list) {
		pj_lock_release(lck->lock);
		lck = lck->prev;
	    }
	    return status;
	}
	lck = lck->next;
    }
    grp_lock_set_owner_thread(glock);
    pj_grp_lock_add_ref(glock);
    return PJ_SUCCESS;
}

static pj_status_t grp_lock_release(LOCK_OBJ *p)
{
    pj_grp_lock_t *glock = (pj_grp_lock_t*)p;
    grp_lock_item *lck;

    grp_lock_unset_owner_thread(glock);

    lck = glock->lock_list.prev;
    while (lck != &glock->lock_list) {
	pj_lock_release(lck->lock);
	lck = lck->prev;
    }
    return pj_grp_lock_dec_ref(glock);
}

static pj_status_t grp_lock_add_handler( pj_grp_lock_t *glock,
                             		 pj_pool_t *pool,
                             		 void *comp,
                             		 void (*destroy)(void *comp),
                             		 pj_bool_t acquire_lock)
{
    grp_destroy_callback *cb;

    if (acquire_lock)
        grp_lock_acquire(glock);

    if (pool == NULL)
	pool = glock->pool;

    cb = PJ_POOL_ZALLOC_T(pool, grp_destroy_callback);
    cb->comp = comp;
    cb->handler = destroy;
    pj_list_push_back(&glock->destroy_list, cb);

    if (acquire_lock)
        grp_lock_release(glock);

    return PJ_SUCCESS;
}

static pj_status_t grp_lock_destroy(LOCK_OBJ *p)
{
    pj_grp_lock_t *glock = (pj_grp_lock_t*)p;
    pj_pool_t *pool = glock->pool;
    grp_lock_item *lck;
    grp_destroy_callback *cb;

    if (!glock->pool) {
	/* already destroyed?! */
	return PJ_EINVAL;
    }

    /* Release all chained locks */
    lck = glock->lock_list.next;
    while (lck != &glock->lock_list) {
	if (lck->lock != glock->own_lock) {
	    int i;
	    for (i=0; i<glock->owner_cnt; ++i)
		pj_lock_release(lck->lock);
	}
	lck = lck->next;
    }

    /* Call callbacks */
    cb = glock->destroy_list.next;
    while (cb != &glock->destroy_list) {
	grp_destroy_callback *next = cb->next;
	cb->handler(cb->comp);
	cb = next;
    }

    pj_lock_destroy(glock->own_lock);
    pj_atomic_destroy(glock->ref_cnt);
    glock->pool = NULL;
    pj_pool_release(pool);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_grp_lock_create( pj_pool_t *pool,
                                        const pj_grp_lock_config *cfg,
                                        pj_grp_lock_t **p_grp_lock)
{
    pj_grp_lock_t *glock;
    grp_lock_item *own_lock;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && p_grp_lock, PJ_EINVAL);

    PJ_UNUSED_ARG(cfg);

    pool = pj_pool_create(pool->factory, "glck%p", 512, 512, NULL);
    if (!pool)
	return PJ_ENOMEM;

    glock = PJ_POOL_ZALLOC_T(pool, pj_grp_lock_t);
    glock->base.lock_object = glock;
    glock->base.acquire = &grp_lock_acquire;
    glock->base.tryacquire = &grp_lock_tryacquire;
    glock->base.release = &grp_lock_release;
    glock->base.destroy = &grp_lock_destroy;

    glock->pool = pool;
    pj_list_init(&glock->lock_list);
    pj_list_init(&glock->destroy_list);
#if PJ_GRP_LOCK_DEBUG
    pj_list_init(&glock->ref_list);
    pj_list_init(&glock->ref_free_list);
#endif

    status = pj_atomic_create(pool, 0, &glock->ref_cnt);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_lock_create_recursive_mutex(pool, pool->obj_name,
                                            &glock->own_lock);
    if (status != PJ_SUCCESS)
	goto on_error;

    own_lock = PJ_POOL_ZALLOC_T(pool, grp_lock_item);
    own_lock->lock = glock->own_lock;
    pj_list_push_back(&glock->lock_list, own_lock);

    *p_grp_lock = glock;
    return PJ_SUCCESS;

on_error:
    grp_lock_destroy(glock);
    return status;
}

PJ_DEF(pj_status_t) pj_grp_lock_create_w_handler( pj_pool_t *pool,
                                        	  const pj_grp_lock_config *cfg,
                                        	  void *member,
                                                  void (*handler)(void *member),
                                        	  pj_grp_lock_t **p_grp_lock)
{
    pj_status_t status;

    status = pj_grp_lock_create(pool, cfg, p_grp_lock);
    if (status == PJ_SUCCESS) {
        grp_lock_add_handler(*p_grp_lock, pool, member, handler, PJ_FALSE);
    }
    
    return status;
}

PJ_DEF(pj_status_t) pj_grp_lock_destroy( pj_grp_lock_t *grp_lock)
{
    return grp_lock_destroy(grp_lock);
}

PJ_DEF(pj_status_t) pj_grp_lock_acquire( pj_grp_lock_t *grp_lock)
{
    return grp_lock_acquire(grp_lock);
}

PJ_DEF(pj_status_t) pj_grp_lock_tryacquire( pj_grp_lock_t *grp_lock)
{
    return grp_lock_tryacquire(grp_lock);
}

PJ_DEF(pj_status_t) pj_grp_lock_release( pj_grp_lock_t *grp_lock)
{
    return grp_lock_release(grp_lock);
}

PJ_DEF(pj_status_t) pj_grp_lock_replace( pj_grp_lock_t *old_lock,
                                         pj_grp_lock_t *new_lock)
{
    grp_destroy_callback *ocb;

    /* Move handlers from old to new */
    ocb = old_lock->destroy_list.next;
    while (ocb != &old_lock->destroy_list) {
	grp_destroy_callback *ncb;

	ncb = PJ_POOL_ALLOC_T(new_lock->pool, grp_destroy_callback);
	ncb->comp = ocb->comp;
	ncb->handler = ocb->handler;
	pj_list_push_back(&new_lock->destroy_list, ncb);

	ocb = ocb->next;
    }

    pj_list_init(&old_lock->destroy_list);

    grp_lock_destroy(old_lock);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_grp_lock_add_handler( pj_grp_lock_t *glock,
                                             pj_pool_t *pool,
                                             void *comp,
                                             void (*destroy)(void *comp))
{
    return grp_lock_add_handler(glock, pool, comp, destroy, PJ_TRUE);
}

PJ_DEF(pj_status_t) pj_grp_lock_del_handler( pj_grp_lock_t *glock,
                                             void *comp,
                                             void (*destroy)(void *comp))
{
    grp_destroy_callback *cb;

    grp_lock_acquire(glock);

    cb = glock->destroy_list.next;
    while (cb != &glock->destroy_list) {
	if (cb->comp == comp && cb->handler == destroy)
	    break;
	cb = cb->next;
    }

    if (cb != &glock->destroy_list)
	pj_list_erase(cb);

    grp_lock_release(glock);
    return PJ_SUCCESS;
}

static pj_status_t grp_lock_add_ref(pj_grp_lock_t *glock)
{
    pj_atomic_inc(glock->ref_cnt);
    return PJ_SUCCESS;
}

static pj_status_t grp_lock_dec_ref(pj_grp_lock_t *glock)
{
    int cnt; /* for debugging */
    if ((cnt=pj_atomic_dec_and_get(glock->ref_cnt)) == 0) {
	grp_lock_destroy(glock);
	return PJ_EGONE;
    }
    pj_assert(cnt > 0);
    pj_grp_lock_dump(glock);
    return PJ_SUCCESS;
}

#if PJ_GRP_LOCK_DEBUG
PJ_DEF(pj_status_t) pj_grp_lock_add_ref_dbg(pj_grp_lock_t *glock,
                                            const char *file,
                                            int line)
{
    grp_lock_ref *ref;
    pj_status_t status;

    pj_enter_critical_section();
    if (!pj_list_empty(&glock->ref_free_list)) {
	ref = glock->ref_free_list.next;
	pj_list_erase(ref);
    } else {
	ref = PJ_POOL_ALLOC_T(glock->pool, grp_lock_ref);
    }

    ref->file = file;
    ref->line = line;
    pj_list_push_back(&glock->ref_list, ref);

    pj_leave_critical_section();

    status = grp_lock_add_ref(glock);

    if (status != PJ_SUCCESS) {
	pj_enter_critical_section();
	pj_list_erase(ref);
	pj_list_push_back(&glock->ref_free_list, ref);
	pj_leave_critical_section();
    }

    return status;
}

PJ_DEF(pj_status_t) pj_grp_lock_dec_ref_dbg(pj_grp_lock_t *glock,
                                            const char *file,
                                            int line)
{
    grp_lock_ref *ref;

    pj_enter_critical_section();
    /* Find the same source file */
    ref = glock->ref_list.next;
    while (ref != &glock->ref_list) {
	if (strcmp(ref->file, file) == 0) {
	    pj_list_erase(ref);
	    pj_list_push_back(&glock->ref_free_list, ref);
	    break;
	}
	ref = ref->next;
    }
    pj_leave_critical_section();

    if (ref == &glock->ref_list) {
	PJ_LOG(2,(THIS_FILE, "pj_grp_lock_dec_ref_dbg() could not find "
			      "matching ref for %s", file));
    }

    return grp_lock_dec_ref(glock);
}
#else
PJ_DEF(pj_status_t) pj_grp_lock_add_ref(pj_grp_lock_t *glock)
{
    return grp_lock_add_ref(glock);
}

PJ_DEF(pj_status_t) pj_grp_lock_dec_ref(pj_grp_lock_t *glock)
{
    return grp_lock_dec_ref(glock);
}
#endif

PJ_DEF(int) pj_grp_lock_get_ref(pj_grp_lock_t *glock)
{
    return pj_atomic_get(glock->ref_cnt);
}

PJ_DEF(pj_status_t) pj_grp_lock_chain_lock( pj_grp_lock_t *glock,
                                            pj_lock_t *lock,
                                            int pos)
{
    grp_lock_item *lck, *new_lck;
    int i;

    grp_lock_acquire(glock);

    for (i=0; i<glock->owner_cnt; ++i)
	pj_lock_acquire(lock);

    lck = glock->lock_list.next;
    while (lck != &glock->lock_list) {
	if (lck->prio >= pos)
	    break;
	lck = lck->next;
    }

    new_lck = PJ_POOL_ZALLOC_T(glock->pool, grp_lock_item);
    new_lck->prio = pos;
    new_lck->lock = lock;
    pj_list_insert_before(lck, new_lck);

    /* this will also release the new lock */
    grp_lock_release(glock);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_grp_lock_unchain_lock( pj_grp_lock_t *glock,
                                              pj_lock_t *lock)
{
    grp_lock_item *lck;

    grp_lock_acquire(glock);

    lck = glock->lock_list.next;
    while (lck != &glock->lock_list) {
	if (lck->lock == lock)
	    break;
	lck = lck->next;
    }

    if (lck != &glock->lock_list) {
	int i;

	pj_list_erase(lck);
	for (i=0; i<glock->owner_cnt; ++i)
	    pj_lock_release(lck->lock);
    }

    grp_lock_release(glock);
    return PJ_SUCCESS;
}

PJ_DEF(void) pj_grp_lock_dump(pj_grp_lock_t *grp_lock)
{
#if PJ_GRP_LOCK_DEBUG
    grp_lock_ref *ref = grp_lock->ref_list.next;
    char info_buf[1000];
    pj_str_t info;

    info.ptr = info_buf;
    info.slen = 0;

    pj_grp_lock_acquire(grp_lock);
    pj_enter_critical_section();

    while (ref != &grp_lock->ref_list && info.slen < sizeof(info_buf)) {
	char *start = info.ptr + info.slen;
	int max_len = sizeof(info_buf) - info.slen;
	int len;

	len = pj_ansi_snprintf(start, max_len, "%s:%d ", ref->file, ref->line);
	if (len < 1 || len >= max_len) {
	    len = strlen(ref->file);
	    if (len > max_len - 1)
		len = max_len - 1;

	    memcpy(start, ref->file, len);
	    start[len++] = ' ';
	}

	info.slen += len;

	ref = ref->next;
    }

    if (ref != &grp_lock->ref_list) {
	int i;
	for (i=0; i<4; ++i)
	    info_buf[sizeof(info_buf)-i-1] = '.';
    }
    info.ptr[info.slen-1] = '\0';

    pj_leave_critical_section();
    pj_grp_lock_release(grp_lock);

    PJ_LOG(4,(THIS_FILE, "Group lock %p, ref_cnt=%d. Reference holders: %s",
	       grp_lock, pj_grp_lock_get_ref(grp_lock), info.ptr));
#else
    PJ_UNUSED_ARG(grp_lock);
#endif
}
