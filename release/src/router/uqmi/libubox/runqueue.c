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

#include <string.h>
#include <stdio.h>
#include "runqueue.h"

static void
__runqueue_empty_cb(struct uloop_timeout *timeout)
{
	struct runqueue *q = container_of(timeout, struct runqueue, timeout);

	q->empty_cb(q);
}

void runqueue_init(struct runqueue *q)
{
	INIT_SAFE_LIST(&q->tasks_active);
	INIT_SAFE_LIST(&q->tasks_inactive);
}

static void __runqueue_start_next(struct uloop_timeout *timeout)
{
	struct runqueue *q = container_of(timeout, struct runqueue, timeout);
	struct runqueue_task *t;

	do {
		if (q->stopped)
			break;

		if (list_empty(&q->tasks_inactive.list))
			break;

		if (q->max_running_tasks && q->running_tasks >= q->max_running_tasks)
			break;

		t = list_first_entry(&q->tasks_inactive.list, struct runqueue_task, list.list);
		safe_list_del(&t->list);
		safe_list_add(&t->list, &q->tasks_active);
		t->running = true;
		q->running_tasks++;
		if (t->run_timeout)
			uloop_timeout_set(&t->timeout, t->run_timeout);
		t->type->run(q, t);
	} while (1);

	if (!q->empty &&
	    list_empty(&q->tasks_active.list) &&
	    list_empty(&q->tasks_inactive.list)) {
		q->empty = true;
		if (q->empty_cb) {
			q->timeout.cb = __runqueue_empty_cb;
			uloop_timeout_set(&q->timeout, 1);
		}
	}
}

static void runqueue_start_next(struct runqueue *q)
{
	if (q->empty)
		return;

	q->timeout.cb = __runqueue_start_next;
	uloop_timeout_set(&q->timeout, 1);
}

static int __runqueue_cancel(void *ctx, struct safe_list *list)
{
	struct runqueue_task *t;

	t = container_of(list, struct runqueue_task, list);
	runqueue_task_cancel(t, 0);

	return 0;
}

void runqueue_cancel_active(struct runqueue *q)
{
	safe_list_for_each(&q->tasks_active, __runqueue_cancel, NULL);
}

void runqueue_cancel_pending(struct runqueue *q)
{
	safe_list_for_each(&q->tasks_inactive, __runqueue_cancel, NULL);
}

void runqueue_cancel(struct runqueue *q)
{
	runqueue_cancel_pending(q);
	runqueue_cancel_active(q);
}

void runqueue_kill(struct runqueue *q)
{
	struct runqueue_task *t;

	while (!list_empty(&q->tasks_active.list)) {
		t = list_first_entry(&q->tasks_active.list, struct runqueue_task, list.list);
		runqueue_task_kill(t);
	}
	runqueue_cancel_pending(q);
	uloop_timeout_cancel(&q->timeout);
}

void runqueue_task_cancel(struct runqueue_task *t, int type)
{
	if (!t->queued)
		return;

	if (!t->running) {
		runqueue_task_complete(t);
		return;
	}

	t->cancelled = true;
	if (t->cancel_timeout)
		uloop_timeout_set(&t->timeout, t->cancel_timeout);
	if (t->type->cancel)
		t->type->cancel(t->q, t, type);
}

static void
__runqueue_task_timeout(struct uloop_timeout *timeout)
{
	struct runqueue_task *t = container_of(timeout, struct runqueue_task, timeout);

	if (t->cancelled)
		runqueue_task_kill(t);
	else
		runqueue_task_cancel(t, t->cancel_type);
}

void runqueue_task_add(struct runqueue *q, struct runqueue_task *t, bool running)
{
	struct safe_list *head;

	if (t->queued)
		return;

	if (!t->type->run && !running) {
		fprintf(stderr, "BUG: inactive task added without run() callback\n");
		return;
	}

	if (running) {
		q->running_tasks++;
		head = &q->tasks_active;
	} else {
		head = &q->tasks_inactive;
	}

	t->timeout.cb = __runqueue_task_timeout;
	t->q = q;
	safe_list_add(&t->list, head);
	t->cancelled = false;
	t->queued = true;
	t->running = running;
	q->empty = false;

	runqueue_start_next(q);
}

void runqueue_task_kill(struct runqueue_task *t)
{
	struct runqueue *q = t->q;
	bool running = t->running;

	if (!t->queued)
		return;

	runqueue_task_complete(t);
	if (running && t->type->kill)
		t->type->kill(q, t);

	runqueue_start_next(q);
}

void runqueue_stop(struct runqueue *q)
{
	q->stopped = true;
}

void runqueue_resume(struct runqueue *q)
{
	q->stopped = false;
	runqueue_start_next(q);
}

void runqueue_task_complete(struct runqueue_task *t)
{
	struct runqueue *q = t->q;

	if (!t->queued)
		return;

	if (t->running)
		t->q->running_tasks--;

	uloop_timeout_cancel(&t->timeout);

	safe_list_del(&t->list);
	t->queued = false;
	t->running = false;
	t->cancelled = false;
	if (t->complete)
		t->complete(q, t);
	runqueue_start_next(t->q);
}

static void
__runqueue_proc_cb(struct uloop_process *p, int ret)
{
	struct runqueue_process *t = container_of(p, struct runqueue_process, proc);

	runqueue_task_complete(&t->task);
}

void runqueue_process_cancel_cb(struct runqueue *q, struct runqueue_task *t, int type)
{
	struct runqueue_process *p = container_of(t, struct runqueue_process, task);

	if (!type)
		type = SIGTERM;

	kill(p->proc.pid, type);
}

void runqueue_process_kill_cb(struct runqueue *q, struct runqueue_task *t)
{
	struct runqueue_process *p = container_of(t, struct runqueue_process, task);

	uloop_process_delete(&p->proc);
	kill(p->proc.pid, SIGKILL);
}

static const struct runqueue_task_type runqueue_proc_type = {
	.name = "process",
	.cancel = runqueue_process_cancel_cb,
	.kill = runqueue_process_kill_cb,
};

void runqueue_process_add(struct runqueue *q, struct runqueue_process *p, pid_t pid)
{
	if (p->proc.pending)
		return;

	p->proc.pid = pid;
	p->proc.cb = __runqueue_proc_cb;
	if (!p->task.type)
		p->task.type = &runqueue_proc_type;
	uloop_process_add(&p->proc);
	if (!p->task.running)
		runqueue_task_add(q, &p->task, true);
}
