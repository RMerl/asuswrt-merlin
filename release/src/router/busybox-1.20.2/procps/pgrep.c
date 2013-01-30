/* vi: set sw=4 ts=4: */
/*
 * Mini pgrep/pkill implementation for busybox
 *
 * Copyright (C) 2007 Loic Grenie <loic.grenie@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define pgrep_trivial_usage
//usage:       "[-flnovx] [-s SID|-P PPID|PATTERN]"
//usage:#define pgrep_full_usage "\n\n"
//usage:       "Display process(es) selected by regex PATTERN\n"
//usage:     "\n	-l	Show command name too"
//usage:     "\n	-f	Match against entire command line"
//usage:     "\n	-n	Show the newest process only"
//usage:     "\n	-o	Show the oldest process only"
//usage:     "\n	-v	Negate the match"
//usage:     "\n	-x	Match whole name (not substring)"
//usage:     "\n	-s	Match session ID (0 for current)"
//usage:     "\n	-P	Match parent process ID"
//usage:
//usage:#define pkill_trivial_usage
//usage:       "[-l|-SIGNAL] [-fnovx] [-s SID|-P PPID|PATTERN]"
//usage:#define pkill_full_usage "\n\n"
//usage:       "Send a signal to process(es) selected by regex PATTERN\n"
//usage:     "\n	-l	List all signals"
//usage:     "\n	-f	Match against entire command line"
//usage:     "\n	-n	Signal the newest process only"
//usage:     "\n	-o	Signal the oldest process only"
//usage:     "\n	-v	Negate the match"
//usage:     "\n	-x	Match whole name (not substring)"
//usage:     "\n	-s	Match session ID (0 for current)"
//usage:     "\n	-P	Match parent process ID"

#include "libbb.h"
#include "xregex.h"

/* Idea taken from kill.c */
#define pgrep (ENABLE_PGREP && applet_name[1] == 'g')
#define pkill (ENABLE_PKILL && applet_name[1] == 'k')

enum {
	/* "vlfxons:P:" */
	OPTBIT_V = 0, /* must be first, we need OPT_INVERT = 0/1 */
	OPTBIT_L,
	OPTBIT_F,
	OPTBIT_X,
	OPTBIT_O,
	OPTBIT_N,
	OPTBIT_S,
	OPTBIT_P,
};

#define OPT_INVERT	(opt & (1 << OPTBIT_V))
#define OPT_LIST	(opt & (1 << OPTBIT_L))
#define OPT_FULL	(opt & (1 << OPTBIT_F))
#define OPT_ANCHOR	(opt & (1 << OPTBIT_X))
#define OPT_FIRST	(opt & (1 << OPTBIT_O))
#define OPT_LAST	(opt & (1 << OPTBIT_N))
#define OPT_SID		(opt & (1 << OPTBIT_S))
#define OPT_PPID	(opt & (1 << OPTBIT_P))

static void act(unsigned pid, char *cmd, int signo)
{
	if (pgrep) {
		if (option_mask32 & (1 << OPTBIT_L)) /* OPT_LIST */
			printf("%d %s\n", pid, cmd);
		else
			printf("%d\n", pid);
	} else
		kill(pid, signo);
}

int pgrep_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int pgrep_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned pid;
	int signo;
	unsigned opt;
	int scan_mask;
	int matched_pid;
	int sid2match, ppid2match;
	char *cmd_last;
	procps_status_t *proc;
	/* These are initialized to 0 */
	struct {
		regex_t re_buffer;
		regmatch_t re_match[1];
	} Z;
#define re_buffer (Z.re_buffer)
#define re_match  (Z.re_match )

	memset(&Z, 0, sizeof(Z));

	/* Parse -SIGNAL for pkill. Must be first option, if present */
	signo = SIGTERM;
	if (pkill && argv[1] && argv[1][0] == '-') {
		int temp = get_signum(argv[1]+1);
		if (temp != -1) {
			signo = temp;
			argv++;
		}
	}

	/* Parse remaining options */
	ppid2match = -1;
	sid2match = -1;
	opt_complementary = "s+:P+"; /* numeric opts */
	opt = getopt32(argv, "vlfxons:P:", &sid2match, &ppid2match);
	argv += optind;

	if (pkill && OPT_LIST) { /* -l: print the whole signal list */
		print_signames();
		return 0;
	}

	pid = getpid();
	if (sid2match == 0)
		sid2match = getsid(pid);

	scan_mask = PSSCAN_COMM | PSSCAN_ARGV0;
	if (OPT_FULL)
		scan_mask |= PSSCAN_ARGVN;

	/* One pattern is required, if no -s and no -P */
	if ((sid2match & ppid2match) < 0 && (!argv[0] || argv[1]))
		bb_show_usage();

	if (argv[0])
		xregcomp(&re_buffer, argv[0], REG_EXTENDED | REG_NOSUB);

	matched_pid = 0;
	cmd_last = NULL;
	proc = NULL;
	while ((proc = procps_scan(proc, scan_mask)) != NULL) {
		char *cmd;

		if (proc->pid == pid)
			continue;

		cmd = proc->argv0;
		if (!cmd) {
			cmd = proc->comm;
		} else {
			int i = proc->argv_len;
			while (--i >= 0) {
				if ((unsigned char)cmd[i] < ' ')
					cmd[i] = ' ';
			}
		}

		if (ppid2match >= 0 && ppid2match != proc->ppid)
			continue;
		if (sid2match >= 0  && sid2match != proc->sid)
			continue;

		/* NB: OPT_INVERT is always 0 or 1 */
		if (!argv[0]
		 || (regexec(&re_buffer, cmd, 1, re_match, 0) == 0 /* match found */
		    && (!OPT_ANCHOR || (re_match[0].rm_so == 0 && re_match[0].rm_eo == (regoff_t)strlen(cmd)))
		    ) ^ OPT_INVERT
		) {
			matched_pid = proc->pid;
			if (OPT_LAST) {
				free(cmd_last);
				cmd_last = xstrdup(cmd);
				continue;
			}
			act(proc->pid, cmd, signo);
			if (OPT_FIRST)
				break;
		}
	}

	if (cmd_last) {
		act(matched_pid, cmd_last, signo);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(cmd_last);
	}
	return matched_pid == 0; /* return 1 if no processes listed/signaled */
}
