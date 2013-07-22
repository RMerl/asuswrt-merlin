/*
 * chrt.c - chrt
 * Command-line utility for manipulating a task's real-time attributes 
 *
 * Robert Love <rml@tech9.net>
 * 27-Apr-2002: initial version
 * 04-May-2011: make thread aware - Davidlohr Bueso <dave@gnu.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, v2, as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Copyright (C) 2004 Robert Love
 */

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include "c.h"
#include "nls.h"

#include "strutils.h"
#include "procutils.h"

/* the SCHED_BATCH is supported since Linux 2.6.16
 *  -- temporary workaround for people with old glibc headers
 */
#if defined (__linux__) && !defined(SCHED_BATCH)
# define SCHED_BATCH 3
#endif

/* the SCHED_IDLE is supported since Linux 2.6.23
 * commit id 0e6aca43e08a62a48d6770e9a159dbec167bf4c6
 * -- temporary workaround for people with old glibc headers
 */
#if defined (__linux__) && !defined(SCHED_IDLE)
# define SCHED_IDLE 5
#endif

#if defined(__linux__) && !defined(SCHED_RESET_ON_FORK)
#define SCHED_RESET_ON_FORK 0x40000000
#endif


static void __attribute__((__noreturn__)) show_usage(int rc)
{
	FILE *out = rc == EXIT_SUCCESS ? stdout : stderr;

	fprintf(out, _(
	"\nchrt - manipulate real-time attributes of a process\n"
	"\nSet policy:\n"
	"  chrt [options] <policy> <priority> {<pid> | <command> [<arg> ...]}\n"
	"\nGet policy:\n"
	"  chrt [options] {<pid> | <command> [<arg> ...]}\n"));

	fprintf(out, _(
	"\nScheduling policies:\n"
	"  -b | --batch         set policy to SCHED_BATCH\n"
	"  -f | --fifo          set policy to SCHED_FIFO\n"
	"  -i | --idle          set policy to SCHED_IDLE\n"
	"  -o | --other         set policy to SCHED_OTHER\n"
	"  -r | --rr            set policy to SCHED_RR (default)\n"));

#ifdef SCHED_RESET_ON_FORK
	fprintf(out, _(
	"\nScheduling flags:\n"
	"  -R | --reset-on-fork set SCHED_RESET_ON_FORK for FIFO or RR\n"));
#endif
	fprintf(out, _(
	"\nOptions:\n"
	"  -a | --all-tasks     operate on all the tasks (threads) for a given pid\n"
	"  -h | --help          display this help\n"
	"  -m | --max           show min and max valid priorities\n"
	"  -p | --pid           operate on existing given pid\n"
	"  -v | --verbose       display status information\n"
	"  -V | --version       output version information\n\n"));

	exit(rc);
}

static void show_rt_info(pid_t pid, int isnew)
{
	struct sched_param sp;
	int policy;

	/* don't display "pid 0" as that is confusing */
	if (!pid)
		pid = getpid();

	policy = sched_getscheduler(pid);
	if (policy == -1)
		err(EXIT_FAILURE, _("failed to get pid %d's policy"), pid);

	if (isnew)
		printf(_("pid %d's new scheduling policy: "), pid);
	else
		printf(_("pid %d's current scheduling policy: "), pid);

	switch (policy) {
	case SCHED_OTHER:
		printf("SCHED_OTHER\n");
		break;
	case SCHED_FIFO:
		printf("SCHED_FIFO\n");
		break;
#ifdef SCHED_RESET_ON_FORK
	case SCHED_FIFO | SCHED_RESET_ON_FORK:
		printf("SCHED_FIFO|SCHED_RESET_ON_FORK\n");
		break;
#endif
#ifdef SCHED_IDLE
	case SCHED_IDLE:
		printf("SCHED_IDLE\n");
		break;
#endif
	case SCHED_RR:
		printf("SCHED_RR\n");
		break;
#ifdef SCHED_RESET_ON_FORK
	case SCHED_RR | SCHED_RESET_ON_FORK:
		printf("SCHED_RR|SCHED_RESET_ON_FORK\n");
		break;
#endif
#ifdef SCHED_BATCH
	case SCHED_BATCH:
		printf("SCHED_BATCH\n");
		break;
#endif
	default:
		warnx(_("unknown scheduling policy"));
	}

	if (sched_getparam(pid, &sp))
		err(EXIT_FAILURE, _("failed to get pid %d's attributes"), pid);

	if (isnew)
		printf(_("pid %d's new scheduling priority: %d\n"),
		       pid, sp.sched_priority);
	else
		printf(_("pid %d's current scheduling priority: %d\n"),
		       pid, sp.sched_priority);
}

static void show_min_max(void)
{
	unsigned long i;
	int policies[] = {
		SCHED_OTHER,
		SCHED_FIFO,
		SCHED_RR,
#ifdef SCHED_BATCH
		SCHED_BATCH,
#endif
#ifdef SCHED_IDLE
		SCHED_IDLE,
#endif
	};
	const char *names[] = {
		"OTHER",
		"FIFO",
		"RR",
#ifdef SCHED_BATCH
		"BATCH",
#endif
#ifdef SCHED_IDLE
		"IDLE",
#endif
	};

	for (i = 0; i < ARRAY_SIZE(policies); i++) {
		int max = sched_get_priority_max(policies[i]);
		int min = sched_get_priority_min(policies[i]);

		if (max >= 0 && min >= 0)
			printf(_("SCHED_%s min/max priority\t: %d/%d\n"),
					names[i], min, max);
		else
			printf(_("SCHED_%s not supported?\n"), names[i]);
	}
}

int main(int argc, char **argv)
{
	int i, policy = SCHED_RR, priority = 0, verbose = 0, policy_flag = 0,
	    all_tasks = 0;
	struct sched_param sp;
	pid_t pid = -1;

	static const struct option longopts[] = {
		{ "all-tasks",  0, NULL, 'a' },
		{ "batch",	0, NULL, 'b' },
		{ "fifo",	0, NULL, 'f' },
		{ "idle",	0, NULL, 'i' },
		{ "pid",	0, NULL, 'p' },
		{ "help",	0, NULL, 'h' },
		{ "max",        0, NULL, 'm' },
		{ "other",	0, NULL, 'o' },
		{ "rr",		0, NULL, 'r' },
		{ "reset-on-fork", 0, NULL, 'R' },
		{ "verbose",	0, NULL, 'v' },
		{ "version",	0, NULL, 'V' },
		{ NULL,		0, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while((i = getopt_long(argc, argv, "+abfiphmoRrvV", longopts, NULL)) != -1)
	{
		int ret = EXIT_FAILURE;

		switch (i) {
		case 'a':
			all_tasks = 1;
			break;
		case 'b':
#ifdef SCHED_BATCH
			policy = SCHED_BATCH;
#endif
			break;
		case 'f':
			policy = SCHED_FIFO;
			break;
		case 'R':
#ifdef SCHED_RESET_ON_FORK
			policy_flag |= SCHED_RESET_ON_FORK;
#endif
			break;
		case 'i':
#ifdef SCHED_IDLE
			policy = SCHED_IDLE;
#endif
			break;
		case 'm':
			show_min_max();
			return EXIT_SUCCESS;
		case 'o':
			policy = SCHED_OTHER;
			break;
		case 'p':
			errno = 0;
			pid = strtol_or_err(argv[argc - 1], _("failed to parse pid"));
			break;
		case 'r':
			policy = SCHED_RR;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'V':
			printf("%s from %s\n", program_invocation_short_name,
					       PACKAGE_STRING);
			return EXIT_SUCCESS;
		case 'h':
			ret = EXIT_SUCCESS;
		default:
			show_usage(ret);
		}
	}

	if (((pid > -1) && argc - optind < 1) ||
	    ((pid == -1) && argc - optind < 2))
		show_usage(EXIT_FAILURE);

	if ((pid > -1) && (verbose || argc - optind == 1)) {
		if (all_tasks) {
			pid_t tid;
			struct proc_tasks *ts = proc_open_tasks(pid);

			if (!ts)
				err(EXIT_FAILURE, _("cannot obtain the list of tasks"));
			while (!proc_next_tid(ts, &tid))
				show_rt_info(tid, FALSE);
			proc_close_tasks(ts);
		} else
			show_rt_info(pid, FALSE);

		if (argc - optind == 1)
			return EXIT_SUCCESS;
	}

	errno = 0;
	priority = strtol_or_err(argv[optind], _("failed to parse priority"));

#ifdef SCHED_RESET_ON_FORK
	/* sanity check */
	if ((policy_flag & SCHED_RESET_ON_FORK) &&
	    !(policy == SCHED_FIFO || policy == SCHED_RR))
		errx(EXIT_FAILURE, _("SCHED_RESET_ON_FORK flag is suppoted for "
				     "SCHED_FIFO and SCHED_RR policies only"));
#endif

	policy |= policy_flag;

	if (pid == -1)
		pid = 0;
	sp.sched_priority = priority;

	if (all_tasks) {
		pid_t tid;
		struct proc_tasks *ts = proc_open_tasks(pid);

		if (!ts)
			err(EXIT_FAILURE, _("cannot obtain the list of tasks"));
		while (!proc_next_tid(ts, &tid))
			if (sched_setscheduler(tid, policy, &sp) == -1)
				err(EXIT_FAILURE, _("failed to set tid %d's policy"), tid);
		proc_close_tasks(ts);
	} else if (sched_setscheduler(pid, policy, &sp) == -1)
		err(EXIT_FAILURE, _("failed to set pid %d's policy"), pid);

	if (verbose)
		show_rt_info(pid, TRUE);

	if (!pid) {
		argv += optind + 1;
		execvp(argv[0], argv);
		perror("execvp");
		err(EXIT_FAILURE, _("failed to execute %s"), argv[0]);
	}

	return EXIT_SUCCESS;
}
