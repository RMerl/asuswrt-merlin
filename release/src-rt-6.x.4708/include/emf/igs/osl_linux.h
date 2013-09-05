/*
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: osl_linux.h 347400 2012-07-26 17:29:23Z $
 */

#ifndef _OSL_LINUX_H_
#define _OSL_LINUX_H_

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>

struct osl_lock
{
	spinlock_t slock;       /* Spin lock */
	uint8      name[16];    /* Name of the lock */
};

typedef struct osl_lock *osl_lock_t;

static inline osl_lock_t
OSL_LOCK_CREATE(uint8 *name)
{
	osl_lock_t lock;

	lock = MALLOC(NULL, sizeof(struct osl_lock));

	if (lock == NULL)
	{
		printf("Memory alloc for lock object failed\n");
		return (NULL);
	}

	strncpy(lock->name, name, sizeof(lock->name)-1);
	lock->name[ sizeof(lock->name)-1 ] = '\0';
	spin_lock_init(&lock->slock);

	return (lock);
}

static inline void
OSL_LOCK_DESTROY(osl_lock_t lock)
{
	MFREE(NULL, lock, sizeof(struct osl_lock));
	return;
}

#define OSL_LOCK(lock)          spin_lock_bh(&((lock)->slock))
#define OSL_UNLOCK(lock)        spin_unlock_bh(&((lock)->slock))

#define DEV_IFNAME(dev)         (((struct net_device *)dev)->name)

typedef struct osl_timer {
	struct timer_list timer;
	void   (*fn)(void *);
	void   *arg;
	uint   ms;
	bool   periodic;
	bool   set;
#ifdef BCMDBG
	char    *name;          /* Desription of the timer */
#endif
} osl_timer_t;

extern osl_timer_t *osl_timer_init(const char *name, void (*fn)(void *arg), void *arg);
extern void osl_timer_add(osl_timer_t *t, uint32 ms, bool periodic);
extern void osl_timer_update(osl_timer_t *t, uint32 ms, bool periodic);
extern bool osl_timer_del(osl_timer_t *t);
extern osl_lock_t OSL_LOCK_CREATE(uint8 *name);
extern void OSL_LOCK_DESTROY(osl_lock_t lock);

#endif /* _OSL_LINUX_H_ */
