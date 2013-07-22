/*
 * Copyright (C) 2011 Davidlohr Bueso <dave@gnu.org>
 *
 * procutils.c: General purpose procfs parsing utilities
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>

#include "procutils.h"
#include "c.h"

/*
 * @pid: process ID for which we want to obtain the threads group
 *
 * Returns: newly allocated tasks structure
 */
struct proc_tasks *proc_open_tasks(pid_t pid)
{
	struct proc_tasks *tasks;
	char path[PATH_MAX];

	sprintf(path, "/proc/%d/task/", pid);

	tasks = malloc(sizeof(struct proc_tasks));
	if (tasks) {
		tasks->dir = opendir(path);
		if (tasks->dir)
			return tasks;
	}

	free(tasks);
	return NULL;
}

/*
 * @tasks: allocated tasks structure
 *
 * Returns: nothing
 */
void proc_close_tasks(struct proc_tasks *tasks)
{
	if (tasks && tasks->dir)
		closedir(tasks->dir);
	free(tasks);
}

/*
 * @tasks: allocated task structure
 * @tid: [output] one of the thread IDs belonging to the thread group
 *        If when an error occurs, it is set to 0.
 *
 * Returns: 0 on success, 1 on end, -1 on failure or no more threads
 */
int proc_next_tid(struct proc_tasks *tasks, pid_t *tid)
{
	struct dirent *d;
	char *end;

	if (!tasks || !tid)
		return -1;

	*tid = 0;
	errno = 0;

	do {
		d = readdir(tasks->dir);
		if (!d)
			return errno ? -1 : 1;		/* error or end-of-dir */

		if (!isdigit((unsigned char) *d->d_name))
			continue;

		*tid = (pid_t) strtol(d->d_name, &end, 10);
		if (errno || d->d_name == end || (end && *end))
			return -1;

	} while (!*tid);

	return 0;
}

#ifdef TEST_PROGRAM

int main(int argc, char *argv[])
{
	pid_t tid, pid;
	struct proc_tasks *ts;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <pid>\n", argv[0]);
		return EXIT_FAILURE;
	}

	pid = strtol(argv[1], (char **) NULL, 10);
	printf("PID=%d, TIDs:", pid);

	ts = proc_open_tasks(pid);
	if (!ts)
		err(EXIT_FAILURE, "open list of tasks failed");

	while (proc_next_tid(ts, &tid) == 0)
		printf(" %d", tid);

	printf("\n");
        proc_close_tasks(ts);
	return EXIT_SUCCESS;
}
#endif /* TEST_PROGRAM */
