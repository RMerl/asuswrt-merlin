/*
 * runqueue.c - a simple task queueing/completion tracking helper
 *
 * Copyright (C) 2013 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __LIBUBOX_RUNQUEUE_H
#define __LIBUBOX_RUNQUEUE_H

#include "list.h"
#include "safe_list.h"
#include "uloop.h"

struct runqueue;
struct runqueue_task;
struct runqueue_task_type;

struct runqueue {
	struct safe_list tasks_active;
	struct safe_list tasks_inactive;
	struct uloop_timeout timeout;

	int running_tasks;
	int max_running_tasks;
	bool stopped;
	bool empty;

	/* called when the runqueue is emptied */
	void (*empty_cb)(struct runqueue *q);
};

struct runqueue_task_type {
	const char *name;

	/*
	 * called when a task is requested to run
	 *
	 * The task is removed from the list before this callback is run. It
	 * can re-arm itself using runqueue_task_add.
	 */
	void (*run)(struct runqueue *q, struct runqueue_task *t);

	/*
	 * called to request cancelling a task
	 *
	 * int type is used as an optional hint for the method to be used when
	 * cancelling the task, e.g. a signal number for processes. Calls
	 * runqueue_task_complete when done.
	 */
	void (*cancel)(struct runqueue *q, struct runqueue_task *t, int type);

	/*
	 * called to kill a task. must not make any calls to runqueue_task_complete,
	 * it has already been removed from the list.
	 */
	void (*kill)(struct runqueue *q, struct runqueue_task *t);
};

struct runqueue_task {
	struct safe_list list;
	const struct runqueue_task_type *type;
	struct runqueue *q;

	void (*complete)(struct runqueue *q, struct runqueue_task *t);

	struct uloop_timeout timeout;
	int run_timeout;
	int cancel_timeout;
	int cancel_type;

	bool queued;
	bool running;
	bool cancelled;
};

struct runqueue_process {
	struct runqueue_task task;
	struct uloop_process proc;
};

void runqueue_init(struct runqueue *q);
void runqueue_cancel(struct runqueue *q);
void runqueue_cancel_active(struct runqueue *q);
void runqueue_cancel_pending(struct runqueue *q);
void runqueue_kill(struct runqueue *q);

void runqueue_stop(struct runqueue *q);
void runqueue_resume(struct runqueue *q);

void runqueue_task_add(struct runqueue *q, struct runqueue_task *t, bool running);
void runqueue_task_complete(struct runqueue_task *t);

void runqueue_task_cancel(struct runqueue_task *t, int type);
void runqueue_task_kill(struct runqueue_task *t);

void runqueue_process_add(struct runqueue *q, struct runqueue_process *p, pid_t pid);

/* to be used only from runqueue_process callbacks */
void runqueue_process_cancel_cb(struct runqueue *q, struct runqueue_task *t, int type);
void runqueue_process_kill_cb(struct runqueue *q, struct runqueue_task *t);

#endif
