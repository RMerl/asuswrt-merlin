/*
 * finite state machine implementation
 *
 * Author       Karsten Keil <kkeil@novell.com>
 *
 * Thanks to    Jan den Ouden
 *              Fritz Elfert
 * Copyright 2008  by Karsten Keil <kkeil@novell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/string.h>
#include "fsm.h"

#define FSM_TIMER_DEBUG 0

void
mISDN_FsmNew(struct Fsm *fsm,
       struct FsmNode *fnlist, int fncount)
{
	int i;

	fsm->jumpmatrix = kzalloc(sizeof(FSMFNPTR) * fsm->state_count *
		fsm->event_count, GFP_KERNEL);

	for (i = 0; i < fncount; i++)
		if ((fnlist[i].state >= fsm->state_count) ||
		    (fnlist[i].event >= fsm->event_count)) {
			printk(KERN_ERR
			    "mISDN_FsmNew Error: %d st(%ld/%ld) ev(%ld/%ld)\n",
			    i, (long)fnlist[i].state, (long)fsm->state_count,
			    (long)fnlist[i].event, (long)fsm->event_count);
		} else
			fsm->jumpmatrix[fsm->state_count * fnlist[i].event +
			    fnlist[i].state] = (FSMFNPTR) fnlist[i].routine;
}
EXPORT_SYMBOL(mISDN_FsmNew);

void
mISDN_FsmFree(struct Fsm *fsm)
{
	kfree((void *) fsm->jumpmatrix);
}
EXPORT_SYMBOL(mISDN_FsmFree);

int
mISDN_FsmEvent(struct FsmInst *fi, int event, void *arg)
{
	FSMFNPTR r;

	if ((fi->state >= fi->fsm->state_count) ||
	    (event >= fi->fsm->event_count)) {
		printk(KERN_ERR
		    "mISDN_FsmEvent Error st(%ld/%ld) ev(%d/%ld)\n",
		    (long)fi->state, (long)fi->fsm->state_count, event,
		    (long)fi->fsm->event_count);
		return 1;
	}
	r = fi->fsm->jumpmatrix[fi->fsm->state_count * event + fi->state];
	if (r) {
		if (fi->debug)
			fi->printdebug(fi, "State %s Event %s",
				fi->fsm->strState[fi->state],
				fi->fsm->strEvent[event]);
		r(fi, event, arg);
		return 0;
	} else {
		if (fi->debug)
			fi->printdebug(fi, "State %s Event %s no action",
				fi->fsm->strState[fi->state],
				fi->fsm->strEvent[event]);
		return 1;
	}
}
EXPORT_SYMBOL(mISDN_FsmEvent);

void
mISDN_FsmChangeState(struct FsmInst *fi, int newstate)
{
	fi->state = newstate;
	if (fi->debug)
		fi->printdebug(fi, "ChangeState %s",
			fi->fsm->strState[newstate]);
}
EXPORT_SYMBOL(mISDN_FsmChangeState);

static void
FsmExpireTimer(struct FsmTimer *ft)
{
#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "FsmExpireTimer %lx", (long) ft);
#endif
	mISDN_FsmEvent(ft->fi, ft->event, ft->arg);
}

void
mISDN_FsmInitTimer(struct FsmInst *fi, struct FsmTimer *ft)
{
	ft->fi = fi;
	ft->tl.function = (void *) FsmExpireTimer;
	ft->tl.data = (long) ft;
#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "mISDN_FsmInitTimer %lx", (long) ft);
#endif
	init_timer(&ft->tl);
}
EXPORT_SYMBOL(mISDN_FsmInitTimer);

void
mISDN_FsmDelTimer(struct FsmTimer *ft, int where)
{
#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "mISDN_FsmDelTimer %lx %d",
			(long) ft, where);
#endif
	del_timer(&ft->tl);
}
EXPORT_SYMBOL(mISDN_FsmDelTimer);

int
mISDN_FsmAddTimer(struct FsmTimer *ft,
	    int millisec, int event, void *arg, int where)
{

#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "mISDN_FsmAddTimer %lx %d %d",
			(long) ft, millisec, where);
#endif

	if (timer_pending(&ft->tl)) {
		if (ft->fi->debug) {
			printk(KERN_WARNING
				"mISDN_FsmAddTimer: timer already active!\n");
			ft->fi->printdebug(ft->fi,
				"mISDN_FsmAddTimer already active!");
		}
		return -1;
	}
	init_timer(&ft->tl);
	ft->event = event;
	ft->arg = arg;
	ft->tl.expires = jiffies + (millisec * HZ) / 1000;
	add_timer(&ft->tl);
	return 0;
}
EXPORT_SYMBOL(mISDN_FsmAddTimer);

void
mISDN_FsmRestartTimer(struct FsmTimer *ft,
	    int millisec, int event, void *arg, int where)
{

#if FSM_TIMER_DEBUG
	if (ft->fi->debug)
		ft->fi->printdebug(ft->fi, "mISDN_FsmRestartTimer %lx %d %d",
			(long) ft, millisec, where);
#endif

	if (timer_pending(&ft->tl))
		del_timer(&ft->tl);
	init_timer(&ft->tl);
	ft->event = event;
	ft->arg = arg;
	ft->tl.expires = jiffies + (millisec * HZ) / 1000;
	add_timer(&ft->tl);
}
EXPORT_SYMBOL(mISDN_FsmRestartTimer);
