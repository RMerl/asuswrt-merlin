/*
 * runqueue-example.c
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

#include <libubox/uloop.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "runqueue.h"

static struct runqueue q;

struct sleeper {
	struct runqueue_process proc;
	int val;
};

static void q_empty(struct runqueue *q)
{
	fprintf(stderr, "All done!\n");
	uloop_end();
}

static void q_sleep_run(struct runqueue *q, struct runqueue_task *t)
{
	struct sleeper *s = container_of(t, struct sleeper, proc.task);
	char str[32];
	pid_t pid;

	fprintf(stderr, "[%d/%d] start 'sleep %d'\n", q->running_tasks, q->max_running_tasks, s->val);

	pid = fork();
	if (pid < 0)
		return;

	if (pid) {
		runqueue_process_add(q, &s->proc, pid);
		return;
	}

	sprintf(str, "%d", s->val);
	execlp("sleep", "sleep", str, NULL);
	exit(1);
}

static void q_sleep_cancel(struct runqueue *q, struct runqueue_task *t, int type)
{
	struct sleeper *s = container_of(t, struct sleeper, proc.task);

	fprintf(stderr, "[%d/%d] cancel 'sleep %d'\n", q->running_tasks, q->max_running_tasks, s->val);
	runqueue_process_cancel_cb(q, t, type);
}

static void q_sleep_complete(struct runqueue *q, struct runqueue_task *p)
{
	struct sleeper *s = container_of(p, struct sleeper, proc.task);

	fprintf(stderr, "[%d/%d] finish 'sleep %d'\n", q->running_tasks, q->max_running_tasks, s->val);
	free(s);
}

static void add_sleeper(int val)
{
	static const struct runqueue_task_type sleeper_type = {
		.run = q_sleep_run,
		.cancel = q_sleep_cancel,
		.kill = runqueue_process_kill_cb,
	};
	struct sleeper *s;

	s = calloc(1, sizeof(*s));
	s->proc.task.type = &sleeper_type;
	s->proc.task.run_timeout = 500;
	s->proc.task.complete = q_sleep_complete;
	s->val = val;
	runqueue_task_add(&q, &s->proc.task, false);
}

int main(int argc, char **argv)
{
	uloop_init();

	runqueue_init(&q);
	q.empty_cb = q_empty;
	q.max_running_tasks = 1;

	if (argc > 1)
		q.max_running_tasks = atoi(argv[1]);

	add_sleeper(1);
	add_sleeper(1);
	add_sleeper(1);
	uloop_run();
	uloop_done();

	return 0;
}
