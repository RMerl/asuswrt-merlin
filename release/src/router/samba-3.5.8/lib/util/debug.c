/*
   Unix SMB/CIFS implementation.
   Samba debug functions
   Copyright (C) Andrew Tridgell 2003
   Copyright (C) James J Myers	 2003

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"
#include "system/time.h"
#include "dynconfig/dynconfig.h"

/**
 * @file
 * @brief Debug logging
 **/

/** 
 * this global variable determines what messages are printed 
 */
int _debug_level = 0;
_PUBLIC_ int *debug_level = &_debug_level;
static int debug_all_class_hack = 1;
int *DEBUGLEVEL_CLASS = &debug_all_class_hack; /* For samba 3 */
static bool debug_all_class_isset_hack = true;
bool    *DEBUGLEVEL_CLASS_ISSET = &debug_all_class_isset_hack; /* For samba 3 */
XFILE *dbf = NULL; /* For Samba 3*/

/* the registered mutex handlers */
static struct {
	const char *name;
	struct debug_ops ops;
} debug_handlers;

/* state variables for the debug system */
static struct {
	int fd;
	enum debug_logtype logtype;
	const char *prog_name;
	bool reopening_logs;
} state;

static bool reopen_logs_scheduled;
static bool check_reopen_logs(void)
{
	if (state.fd == 0 || reopen_logs_scheduled) {
		reopen_logs_scheduled = false;
		reopen_logs();
	}

	if (state.fd <= 0) 
		return false;

	return true;
}

_PUBLIC_ void debug_schedule_reopen_logs(void)
{
	reopen_logs_scheduled = true;
}

static void log_timestring(int level, const char *location, const char *func)
{
	char *t = NULL;
	char *s = NULL;

	if (!check_reopen_logs()) return;

	if (state.logtype != DEBUG_FILE) return;

	t = timestring(NULL, time(NULL));
	if (!t) return;

	asprintf(&s, "[%s, %d %s:%s()]\n", t, level, location, func);
	talloc_free(t);
	if (!s) return;

	write(state.fd, s, strlen(s));
	free(s);
}

/**
  the backend for debug messages. Note that the DEBUG() macro has already
  ensured that the log level has been met before this is called
*/
_PUBLIC_ void dbghdr(int level, const char *location, const char *func)
{
	log_timestring(level, location, func);
	log_task_id();
}


_PUBLIC_ void dbghdrclass(int level, int dclass, const char *location, const char *func)
{
	/* Simple wrapper, Samba 4 doesn't do debug classes */
	dbghdr(level, location, func);
}

/**
  the backend for debug messages. Note that the DEBUG() macro has already
  ensured that the log level has been met before this is called

  @note You should never have to call this function directly. Call the DEBUG()
  macro instead.
*/
_PUBLIC_ void dbgtext(const char *format, ...)
{
	va_list ap;
	char *s = NULL;

	if (!check_reopen_logs()) return;

	va_start(ap, format);
	vasprintf(&s, format, ap);
	va_end(ap);

	write(state.fd, s, strlen(s));
	free(s);
}

_PUBLIC_ const char *logfile = NULL;

/**
  reopen the log file (usually called because the log file name might have changed)
*/
_PUBLIC_ void reopen_logs(void)
{
	char *fname = NULL;
	int old_fd = state.fd;
	if (state.reopening_logs) {
		return;
	}

	switch (state.logtype) {
	case DEBUG_STDOUT:
		state.fd = 1;
		break;

	case DEBUG_STDERR:
		state.fd = 2;
		break;

	case DEBUG_FILE:
		state.reopening_logs = true;
		if (logfile && (*logfile) == '/') {
			fname = strdup(logfile);
		} else {
			asprintf(&fname, "%s/%s.log", dyn_LOGFILEBASE, state.prog_name);
		}
		if (fname) {
			int newfd = open(fname, O_CREAT|O_APPEND|O_WRONLY, 0600);
			if (newfd == -1) {
				DEBUG(1, ("Failed to open new logfile: %s\n", fname));
				old_fd = -1;
			} else {
				state.fd = newfd;
			}
			free(fname);
		} else {
			DEBUG(1, ("Failed to find name for file-based logfile!\n"));
		}
		state.reopening_logs = false;

		break;
	}

	if (old_fd > 2) {
		close(old_fd);
	}
}

/**
  control the name of the logfile and whether logging will be to stdout, stderr
  or a file
*/
_PUBLIC_ void setup_logging(const char *prog_name, enum debug_logtype new_logtype)
{
	if (state.logtype < new_logtype) {
		state.logtype = new_logtype;
	}
	if (prog_name) {
		state.prog_name = prog_name;
	}
	reopen_logs();
}

/**
   Just run logging to stdout for this program 
*/
_PUBLIC_ void setup_logging_stdout(void)
{
	setup_logging(NULL, DEBUG_STDOUT);
}

/**
  return a string constant containing n tabs
  no more than 10 tabs are returned
*/
_PUBLIC_ const char *do_debug_tab(int n)
{
	const char *tabs[] = {"", "\t", "\t\t", "\t\t\t", "\t\t\t\t", "\t\t\t\t\t", 
			      "\t\t\t\t\t\t", "\t\t\t\t\t\t\t", "\t\t\t\t\t\t\t\t", 
			      "\t\t\t\t\t\t\t\t\t", "\t\t\t\t\t\t\t\t\t\t"};
	return tabs[MIN(n, 10)];
}


/**
  log suspicious usage - print comments and backtrace
*/	
_PUBLIC_ void log_suspicious_usage(const char *from, const char *info)
{
	if (!debug_handlers.ops.log_suspicious_usage) return;

	debug_handlers.ops.log_suspicious_usage(from, info);
}


/**
  print suspicious usage - print comments and backtrace
*/	
_PUBLIC_ void print_suspicious_usage(const char* from, const char* info)
{
	if (!debug_handlers.ops.print_suspicious_usage) return;

	debug_handlers.ops.print_suspicious_usage(from, info);
}

_PUBLIC_ uint32_t get_task_id(void)
{
	if (debug_handlers.ops.get_task_id) {
		return debug_handlers.ops.get_task_id();
	}
	return getpid();
}

_PUBLIC_ void log_task_id(void)
{
	if (!debug_handlers.ops.log_task_id) return;

	if (!check_reopen_logs()) return;

	debug_handlers.ops.log_task_id(state.fd);
}

/**
  register a set of debug handlers. 
*/
_PUBLIC_ void register_debug_handlers(const char *name, struct debug_ops *ops)
{
	debug_handlers.name = name;
	debug_handlers.ops = *ops;
}
