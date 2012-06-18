/* vi: set sw=4 ts=4: */
/*
 * Mini klogd implementation for busybox
 *
 * Copyright (C) 2001 by Gennady Feldman <gfeldman@gena01.com>.
 * Changes: Made this a standalone busybox module which uses standalone
 *					syslog() client interface.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Copyright (C) 2000 by Karl M. Hegbloom <karlheg@debian.org>
 *
 * "circular buffer" Copyright (C) 2000 by Gennady Feldman <gfeldman@gena01.com>
 *
 * Maintainer: Gennady Feldman <gfeldman@gena01.com> as of Mar 12, 2001
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"
#include <syslog.h>
#include <sys/klog.h>

static void klogd_signal(int sig)
{
	/* FYI: cmd 7 is equivalent to setting console_loglevel to 7
	 * via klogctl(8, NULL, 7). */
	klogctl(7, NULL, 0); /* "7 -- Enable printk's to console" */
	klogctl(0, NULL, 0); /* "0 -- Close the log. Currently a NOP" */
	syslog(LOG_NOTICE, "klogd: exiting");
	kill_myself_with_sig(sig);
}

char log_buffer[6 * 1024 + 1];	/* Big enough to not lose msgs at bootup */
enum {
	KLOGD_LOGBUF_SIZE = sizeof(log_buffer),
	OPT_LEVEL      = (1 << 0),
	OPT_FOREGROUND = (1 << 1),
};

int klogd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int klogd_main(int argc UNUSED_PARAM, char **argv)
{
	int i = 0;
	char *opt_c;
	int opt;
	int used;
	unsigned int cnt;

	opt = getopt32(argv, "c:n", &opt_c);
	if (opt & OPT_LEVEL) {
		/* Valid levels are between 1 and 8 */
		i = xatou_range(opt_c, 1, 8);
	}
	if (!(opt & OPT_FOREGROUND)) {
		bb_daemonize_or_rexec(DAEMON_CHDIR_ROOT, argv);
	}

	openlog("kernel", 0, LOG_KERN);

	bb_signals(BB_FATAL_SIGS, klogd_signal);
	signal(SIGHUP, SIG_IGN);

	/* "Open the log. Currently a NOP" */
	klogctl(1, NULL, 0);

	/* "printk() prints a message on the console only if it has a loglevel
	 * less than console_loglevel". Here we set console_loglevel = i. */
	if (i)
		klogctl(8, NULL, i);

	syslog(LOG_NOTICE, "klogd started: %s", bb_banner);

	used = 0;
	cnt = 0;
	while (1) {
		int n;
		int priority;
		char *start;
		char *eor;

		/* "2 -- Read from the log." */
		start = log_buffer + used;
		n = klogctl(2, start, KLOGD_LOGBUF_SIZE-1 - used);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "klogd: error %d in klogctl(2): %m",
					errno);
			break;
		}
		start[n] = '\0';
		eor = &start[n];

		/* Process each newline-terminated line in the buffer */
		start = log_buffer;
		while (1) {
			char *newline = strchrnul(start, '\n');

			if (*newline == '\0') {
				/* This line is incomplete */

				/* move it to the front of the buffer */
				overlapping_strcpy(log_buffer, start);
				used = newline - start;
				if (used < KLOGD_LOGBUF_SIZE-1) {
					/* buffer isn't full */
					break;
				}
				/* buffer is full, log it anyway */
				used = 0;
				newline = NULL;
			} else {
				*newline++ = '\0';
			}

			/* Extract the priority */
			priority = LOG_INFO;
			if (*start == '<') {
				start++;
				if (*start) {
					/* kernel never generates multi-digit prios */
					priority = (*start - '0');
					start++;
				}
				if (*start == '>')
					start++;
			}
			/* Log (only non-empty lines) */
			if (*start) {
				syslog(priority, "%s", start);
				/* give syslog time to catch up */
				++cnt;
				if ((cnt & 0x07) == 0 && (cnt < 300 || (eor - start) > 200))
					usleep(50 * 1000);
			}

			if (!newline)
				break;
			start = newline;
		}
	}

	return EXIT_FAILURE;
}
