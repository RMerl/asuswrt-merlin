/*
 * Copyright (C) 2007 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * Author: Adrian Hunter
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

struct child_info {
	struct child_info *next;
	pid_t pid;
	int terminated;
	int killed;
	int gone;
};

struct child_info *children = 0;

void kill_children(void)
{
	struct child_info *child;

	child = children;
	while (child) {
		if (!child->gone) {
			if (!child->terminated) {
				child->terminated = 1;
				kill(child->pid, SIGTERM);
			} /*else if (!child->killed) {
				child->killed = 1;
				kill(child->pid, SIGKILL);
			}*/
		}
		child = child->next;
	}
}

void add_child(pid_t child_pid)
{
	struct child_info *child;
	size_t sz;

	sz = sizeof(struct child_info);
	child = (struct child_info *) malloc(sz);
	memset(child, 0, sz);
	child->pid = child_pid;
	child->next = children;
	children = child;
}

void mark_child_gone(pid_t child_pid)
{
	struct child_info *child;

	child = children;
	while (child) {
		if (child->pid == child_pid) {
			child->gone = 1;
			break;
		}
		child = child->next;
	}
}

int have_children(void)
{
	struct child_info *child;

	child = children;
	while (child) {
		if (!child->gone)
			return 1;
		child = child->next;
	}
	return 0;
}

int parse_command_line(char *cmdline, int *pargc, char ***pargv)
{
	char **tmp;
	char *p, *v, *q;
	size_t sz;
	int argc = 0;
	int state = 0;
	char *argv[1024];

	if (!cmdline)
		return 1;
	q = v = (char *) malloc(strlen(cmdline) + 1024);
	if (!v)
		return 1;
	p = cmdline;
	for (;;) {
		char c = *p++;
		if (!c) {
			*v++ = 0;
			break;
		}
		switch (state) {
			case 0: /* Between args */
				if (isspace(c))
					break;
				argv[argc++] = v;
				if (c == '"') {
					state = 2;
					break;
				} else if (c == '\'') {
					state = 3;
					break;
				}
				state = 1;
			case 1: /* Not quoted */
				if (c == '\\') {
					if (*p)
						*v++ = *p;
				} else if (isspace(c)) {
					*v++ = 0;
					state = 0;
				} else
					*v++ = c;
				break;
			case 2: /* Double quoted */
				if (c == '\\' && *p == '"') {
					*v++ = '"';
					++p;
				} else if (c == '"') {
					*v++ = 0;
					state = 0;
				} else
					*v++ = c;
				break;
			case 3: /* Single quoted */
				if (c == '\'') {
					*v++ = 0;
					state = 0;
				} else
					*v++ = c;
				break;
		}
	}
	argv[argc] = 0;
	sz = sizeof(char *) * (argc + 1);
	tmp = (char **) malloc(sz);
	if (!tmp) {
		free(q);
		return 1;
	}
	if (argc == 0)
		free(q);
	memcpy(tmp, argv, sz);
	*pargc = argc;
	*pargv = tmp;
	return 0;
}

void signal_handler(int signum)
{
	kill_children();
}

int result = 0;
int alarm_gone_off = 0;

void alarm_handler(int signum)
{
	if (!result)
		alarm_gone_off = 1;
	kill_children();
}

int main(int argc, char *argv[], char **env)
{
	int p;
	pid_t child_pid;
	int status;
	int duration = 0;

	p = 1;
	if (argc > 1) {
		if (strncmp(argv[p], "--help", 6) == 0 ||
				strncmp(argv[p], "-h", 2) == 0) {
			printf(	"Usage is: "
				"fstest_monitor options programs...\n"
				"    Options are:\n"
				"        -h, --help           "
				"This help message\n"
				"        -d, --duration arg   "
				"Stop after arg seconds\n"
				"\n"
				"Run programs and wait for them."
				" If duration is specified,\n"
				"kill all programs"
				" after that number of seconds have elapsed.\n"
				"Example: "
				"fstest_monitor \"/bin/ls -l\" /bin/date\n"
				);
			return 1;
		}
		if (strncmp(argv[p], "--duration", 10) == 0 ||
				strncmp(argv[p], "-d", 2) == 0) {
			char *s;
			if (p+1 < argc && !isdigit(argv[p][strlen(argv[p])-1]))
				++p;
			s = argv[p];
			while (*s && !isdigit(*s))
				++s;
			duration = atoi(s);
			++p;
		}
	}

	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	for (; p < argc; ++p) {
		child_pid = fork();
		if (child_pid) {
			/* Parent */
			if (child_pid == (pid_t) -1) {
				kill_children();
				result = 1;
				break;
			}
			add_child(child_pid);
		} else {
			/* Child */
			int cargc;
			char **cargv;

			if (parse_command_line(argv[p], &cargc, &cargv))
				return 1;
			execve(cargv[0], cargv, env);
			return 1;
		}
	}
	if (!result && duration > 0) {
		signal(SIGALRM, alarm_handler);
		alarm(duration);
	}
	while (have_children()) {
		status = 0;
		child_pid = wait(&status);
		if (child_pid == (pid_t) -1) {
			if (errno == EINTR)
				continue;
			kill_children();
			return 1;
		}
		mark_child_gone(child_pid);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			result = 1;
			kill_children();
		}
	}

	if (alarm_gone_off)
		return 0;

	return result;
}
