/*
 * RPC OSL linux port
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: linux_rpc_osl.c 401759 2013-05-13 16:08:08Z $
 */
#if (!defined(WLC_HIGH) && !defined(WLC_LOW))
#error "SPLIT"
#endif

#ifdef BCMDRIVER
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <osl.h>
#include <bcmutils.h>

#include <rpc_osl.h>
#include <linuxver.h>

struct rpc_osl {
	osl_t *osh;
	wait_queue_head_t wait; /* To block awaiting init frame */
	spinlock_t	lock;
	bool wakeup;
	ulong flags;
};

rpc_osl_t *
rpc_osl_attach(osl_t *osh)
{
	rpc_osl_t *rpc_osh;

	if ((rpc_osh = (rpc_osl_t *)MALLOC(osh, sizeof(rpc_osl_t))) == NULL)
		return NULL;

	spin_lock_init(&rpc_osh->lock);
	init_waitqueue_head(&rpc_osh->wait);
	/* set the wakeup flag to TRUE, so we are ready
	   to detect a call return even before we call rpc_osl_wait.
	 */
	rpc_osh->wakeup = TRUE;
	rpc_osh->osh = osh;

	return rpc_osh;
}

void
rpc_osl_detach(rpc_osl_t *rpc_osh)
{
	if (!rpc_osh)
		return;

	MFREE(rpc_osh->osh, rpc_osh, sizeof(rpc_osl_t));
}

int
rpc_osl_wait(rpc_osl_t *rpc_osh, uint ms, bool *ptimedout)
{
	unsigned long j;
	int ret;

	/* printf("%s timeout:%d\n", __FUNCTION__, ms); */
	j = ms * HZ / 1000;

	/* yield the control back to OS, wait for wake_up() call up to j ms */
	ret = wait_event_interruptible_timeout(rpc_osh->wait, rpc_osh->wakeup == FALSE, j);

	/* 0 ret => timeout */
	if (ret == 0) {
		if (ptimedout)
			*ptimedout = TRUE;
	} else if (ret < 0)
		return ret;

	RPC_OSL_LOCK(rpc_osh);
	/* set the flag to be ready for next return  */
	rpc_osh->wakeup = TRUE;
	RPC_OSL_UNLOCK(rpc_osh);

	return 0;
}

void
rpc_osl_wake(rpc_osl_t *rpc_osh)
{
	/* Wake up if someone's waiting */
	if (rpc_osh->wakeup == TRUE) {
		/* this is the only place where this flag is set to FALSE
		   It will be reset to TRUE in rpc_osl_wait.
		 */
		rpc_osh->wakeup = FALSE;
		wake_up(&rpc_osh->wait);
	}
}

void
rpc_osl_lock(rpc_osl_t *rpc_osh)
{
	spin_lock_irqsave(&rpc_osh->lock, rpc_osh->flags);
}

void
rpc_osl_unlock(rpc_osl_t *rpc_osh)
{
	spin_unlock_irqrestore(&rpc_osh->lock, rpc_osh->flags);
}

#else /* !BCMDRIVER */

#include <typedefs.h>
#include <osl.h>
#include <rpc_osl.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

struct rpc_osl {
	osl_t *osh;
	pthread_cond_t wait; /* To block awaiting init frame */
	pthread_mutex_t	lock;
	bool wakeup;
};

rpc_osl_t *
rpc_osl_attach(osl_t *osh)
{
	rpc_osl_t *rpc_osh;

	if ((rpc_osh = (rpc_osl_t *)MALLOC(osh, sizeof(rpc_osl_t))) == NULL)
		return NULL;

	rpc_osh->osh = osh;
	pthread_mutex_init(&rpc_osh->lock, NULL);
	pthread_cond_init(&rpc_osh->wait, NULL);
	rpc_osh->wakeup = FALSE;

	return rpc_osh;
}

void
rpc_osl_detach(rpc_osl_t *rpc_osh)
{
	if (rpc_osh) {
		MFREE(rpc_osh->osh, rpc_osh, sizeof(rpc_osl_t));
	}
}

int
rpc_osl_wait(rpc_osl_t *rpc_osh, uint ms, bool *ptimedout)
{
	struct timespec timeout;
	int ret;

	printf("%s timeout:%d\n", __FUNCTION__, ms);
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += ms/1000;
	ms = ms - ((ms/1000) * 1000);
	timeout.tv_nsec += ms * 1000 * 1000;

	RPC_OSL_LOCK(rpc_osh);
	rpc_osh->wakeup = FALSE;
	while (rpc_osh->wakeup == FALSE) {
		ret = pthread_cond_timedwait(&rpc_osh->wait, &rpc_osh->lock, &timeout);
		/* check for timedout instead of wait condition */
		if (ret == ETIMEDOUT) {
			if (ptimedout)
				*ptimedout = TRUE;
			rpc_osh->wakeup = TRUE;
		} else if (ret)
			break;	/* some other error (e.g. interrupt) */
	}
	RPC_OSL_UNLOCK(rpc_osh);

	return ret;
}
void
rpc_osl_wake(rpc_osl_t *rpc_osh)
{
	rpc_osh->wakeup = TRUE;
	pthread_cond_signal(&rpc_osh->wait);
}

void
rpc_osl_lock(rpc_osl_t *rpc_osh)
{
	pthread_mutex_lock(&rpc_osh->lock);
}

void
rpc_osl_unlock(rpc_osl_t *rpc_osh)
{
	pthread_mutex_unlock(&rpc_osh->lock);
}

#endif /* BCMDRIVER */
