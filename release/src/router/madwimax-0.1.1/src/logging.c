/*
 * This is a reverse-engineered driver for mobile WiMAX (802.16e) devices
 * based on Samsung CMC-730 chip.
 * Copyright (C) 2008-2009 Alexander Gordeev <lasaine@lvk.cs.msu.su>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "config.h"
#include "logging.h"

/* the reason we define MADWIMAX_VERSION as a static string, rather than a
	macro, is to make dependency tracking easier (only logging.o depends
	on madwimax_version.h), and also to prevent all sources from
	having to be recompiled each time the version changes (they only
	need to be re-linked). */
#include "madwimax_version.h"

const char* get_madwimax_version()
{
	return MADWIMAX_VERSION_MACRO;
}

static int wimax_log_level = 0;

/* set wmlog level to the desired value */
void set_wmlog_level(int level)
{
	wimax_log_level = level;
}

/* increase wmlog level by 1 */
void inc_wmlog_level()
{
	wimax_log_level++;
}

FILE *logfile = NULL;

/* log to stderr */
static void wmlog_file(const char *fmt, va_list va)
{
	vfprintf(logfile, fmt, va);
	fprintf(logfile, "\n");
	fflush(logfile);
}

/* log to syslog */
static void wmlog_syslog(const char *fmt, va_list va)
{
	vsyslog(LOG_INFO, fmt, va);
}

typedef void (*wmlogger_func)(const char *fmt, va_list va);

static wmlogger_func loggers[2] = {[WMLOGGER_FILE] = wmlog_file, [WMLOGGER_SYSLOG] = wmlog_syslog};
static wmlogger_func selected_logger = wmlog_file;

/* set logger to stderr or syslog */
void set_wmlogger(const char *progname, int logger, FILE *file)
{
	selected_logger = loggers[logger];
	if (logger == WMLOGGER_SYSLOG) {
		openlog(progname, LOG_PID, LOG_DAEMON);
	} else {
		closelog();
		logfile = file;
	}
}

/* print wmlog message. */
void wmlog_msg(int level, const char *fmt, ...)
{
	va_list va;

	if (level > wimax_log_level) return;

	va_start(va, fmt);
	selected_logger(fmt, va);
	va_end(va);
}

/* print wmlog message. */
static inline void wmlog_msg_priv(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	selected_logger(fmt, va);
	va_end(va);
}

/* If a character is not printable, return a dot. */
#define toprint(x) (isprint((unsigned int)x) ? (x) : '.')

/* dump message and len bytes from buf in hexadecimal and ASCII. */
void wmlog_dumphexasc(int level, const void *buf, int len, const char *fmt, ...)
{
	int i;
	va_list va;

	if (level > wimax_log_level) return;

	va_start(va, fmt);
	selected_logger(fmt, va);
	va_end(va);

	for (i = 0; i < len; i+=16) {
		int j;
		char hex[49];
		char ascii[17];
		memset(hex, ' ', 48);
		for(j = i; j < i + 16 && j < len; j++) {
			sprintf(hex + ((j - i) * 3), " %02x", ((unsigned char*)buf)[j]);
			ascii[j - i] = toprint(((unsigned char*)buf)[j]);
		}
		hex[(j - i) * 3] = ' ';
		hex[48] = 0;
		ascii[j - i] = 0;
		wmlog_msg_priv("  %08x:%s    %s", i, hex, ascii);
	}
}

