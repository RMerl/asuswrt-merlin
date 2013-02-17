/*
 * Copyright (C) 1995 Olaf Kirch
 * Modified by Jeffrey A. Uphoff, 1995, 1997, 1999.
 * Modified by H.J. Lu, 1998.
 * Modified by Lon Hohberger, Oct. 2000
 *
 * NSM for Linux.
 */

/* 
 * 	log.c - logging functions for lockd/statd
 *	260295	 okir	started with simply syslog logging.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <syslog.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include "log.h"
#include "statd.h"

static pid_t	mypid;
								/* Turns on logging to console/stderr. */
#if 0
static int	opt_debug = 0;	/* Will be command-line option, eventually */
#endif

void log_init(void)
{
	if (!(run_mode & MODE_LOG_STDERR)) 
		openlog(name_p, LOG_PID | LOG_NDELAY, LOG_DAEMON);

	mypid = getpid();

	note(N_WARNING,"Version %s Starting",version_p);
}

void log_background(void)
{
	/* NOP */
}

void die(char *fmt, ...)
{
	char	buffer[1024];
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf (buffer, 1024, fmt, ap);
	va_end(ap);
	buffer[1023]=0;

	note(N_FATAL, "%s", buffer);

#ifndef DEBUG
	exit (2);
#else
	abort();	/* make a core */
#endif
}

void note(int level, char *fmt, ...)
{
	char	buffer[1024];
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf (buffer, 1024, fmt, ap);
	va_end(ap);
	buffer[1023]=0;

	if ((!(run_mode & MODE_LOG_STDERR)) && (level < N_DEBUG)) {
		syslog(level, "%s", buffer);
	} else if (run_mode & MODE_LOG_STDERR) {
		/* Log everything, including dprintf() stuff to stderr */
		time_t		now;
		struct tm *	tm;

		time(&now);
		tm = localtime(&now);
		fprintf (stderr, "%02d/%02d/%04d %02d:%02d:%02d %s[%d]: %s\n",
			tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900,
			tm->tm_hour, tm->tm_min, tm->tm_sec,
			name_p, mypid,
			buffer);
	}
}
