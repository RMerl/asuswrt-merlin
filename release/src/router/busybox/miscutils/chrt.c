/* vi: set sw=4 ts=4: */
/*
 * chrt - manipulate real-time attributes of a process
 * Copyright (c) 2006-2007 Bernhard Reutner-Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define chrt_trivial_usage
//usage:       "[-prfom] [PRIO] [PID | PROG ARGS]"
//usage:#define chrt_full_usage "\n\n"
//usage:       "Change scheduling priority and class for a process\n"
//usage:     "\n	-p	Operate on PID"
//usage:     "\n	-r	Set SCHED_RR class"
//usage:     "\n	-f	Set SCHED_FIFO class"
//usage:     "\n	-o	Set SCHED_OTHER class"
//usage:     "\n	-m	Show min/max priorities"
//usage:
//usage:#define chrt_example_usage
//usage:       "$ chrt -r 4 sleep 900; x=$!\n"
//usage:       "$ chrt -f -p 3 $x\n"
//usage:       "You need CAP_SYS_NICE privileges to set scheduling attributes of a process"

#include <sched.h>
#include "libbb.h"

static const struct {
	int policy;
	char name[sizeof("SCHED_OTHER")];
} policies[] = {
	{SCHED_OTHER, "SCHED_OTHER"},
	{SCHED_FIFO, "SCHED_FIFO"},
	{SCHED_RR, "SCHED_RR"}
};

//TODO: add
// -b, SCHED_BATCH
// -i, SCHED_IDLE

static void show_min_max(int pol)
{
	const char *fmt = "%s min/max priority\t: %u/%u\n";
	int max, min;

	max = sched_get_priority_max(pol);
	min = sched_get_priority_min(pol);
	if ((max|min) < 0)
		fmt = "%s not supported\n";
	printf(fmt, policies[pol].name, min, max);
}

#define OPT_m (1<<0)
#define OPT_p (1<<1)
#define OPT_r (1<<2)
#define OPT_f (1<<3)
#define OPT_o (1<<4)

int chrt_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int chrt_main(int argc UNUSED_PARAM, char **argv)
{
	pid_t pid = 0;
	unsigned opt;
	struct sched_param sp;
	char *pid_str;
	char *priority = priority; /* for compiler */
	const char *current_new;
	int policy = SCHED_RR;

	/* only one policy accepted */
	opt_complementary = "r--fo:f--ro:o--rf";
	opt = getopt32(argv, "+mprfo");
	if (opt & OPT_m) { /* print min/max and exit */
		show_min_max(SCHED_FIFO);
		show_min_max(SCHED_RR);
		show_min_max(SCHED_OTHER);
		fflush_stdout_and_exit(EXIT_SUCCESS);
	}
	if (opt & OPT_r)
		policy = SCHED_RR;
	if (opt & OPT_f)
		policy = SCHED_FIFO;
	if (opt & OPT_o)
		policy = SCHED_OTHER;

	argv += optind;
	if (!argv[0])
		bb_show_usage();
	if (opt & OPT_p) {
		pid_str = *argv++;
		if (*argv) { /* "-p <priority> <pid> [...]" */
			priority = pid_str;
			pid_str = *argv;
		}
		/* else "-p <pid>", and *argv == NULL */
		pid = xatoul_range(pid_str, 1, ((unsigned)(pid_t)ULONG_MAX) >> 1);
	} else {
		priority = *argv++;
		if (!*argv)
			bb_show_usage();
	}

	current_new = "current\0new";
	if (opt & OPT_p) {
		int pol;
 print_rt_info:
		pol = sched_getscheduler(pid);
		if (pol < 0)
			bb_perror_msg_and_die("can't %cet pid %d's policy", 'g', (int)pid);
		printf("pid %d's %s scheduling policy: %s\n",
				pid, current_new, policies[pol].name);
		if (sched_getparam(pid, &sp))
			bb_perror_msg_and_die("can't get pid %d's attributes", (int)pid);
		printf("pid %d's %s scheduling priority: %d\n",
				(int)pid, current_new, sp.sched_priority);
		if (!*argv) {
			/* Either it was just "-p <pid>",
			 * or it was "-p <priority> <pid>" and we came here
			 * for the second time (see goto below) */
			return EXIT_SUCCESS;
		}
		*argv = NULL;
		current_new += 8;
	}

	/* from the manpage of sched_getscheduler:
	[...] sched_priority can have a value in the range 0 to 99.
	[...] SCHED_OTHER or SCHED_BATCH must be assigned static priority 0.
	[...] SCHED_FIFO or SCHED_RR can have static priority in 1..99 range.
	*/
	sp.sched_priority = xstrtou_range(priority, 0, policy != SCHED_OTHER ? 1 : 0, 99);

	if (sched_setscheduler(pid, policy, &sp) < 0)
		bb_perror_msg_and_die("can't %cet pid %d's policy", 's', (int)pid);

	if (!argv[0]) /* "-p <priority> <pid> [...]" */
		goto print_rt_info;

	BB_EXECVP_or_die(argv);
}
