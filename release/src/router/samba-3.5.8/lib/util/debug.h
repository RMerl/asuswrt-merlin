/* 
   Unix SMB/CIFS implementation.
   Samba debug defines
   Copyright (C) Andrew Tridgell 2003

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

#ifndef _SAMBA_DEBUG_H_
#define _SAMBA_DEBUG_H_

/**
 * @file
 * @brief Debugging macros
 */

/* the debug operations structure - contains function pointers to 
   various debug implementations of each operation */
struct debug_ops {
	/* function to log (using DEBUG) suspicious usage of data structure */
	void (*log_suspicious_usage)(const char* from, const char* info);
				
	/* function to log (using printf) suspicious usage of data structure.
	 * To be used in circumstances when using DEBUG would cause loop. */
	void (*print_suspicious_usage)(const char* from, const char* info);
	
	/* function to return process/thread id */
	uint32_t (*get_task_id)(void);
	
	/* function to log process/thread id */
	void (*log_task_id)(int fd);
};

#define DEBUGLEVEL *debug_level
extern int DEBUGLEVEL;

#define debug_ctx() (_debug_ctx?_debug_ctx:(_debug_ctx=talloc_new(NULL)))

#define DEBUGLVL(level) ((level) <= DEBUGLEVEL)
#define _DEBUG(level, body, header) do { \
	if (DEBUGLVL(level)) { \
		void* _debug_ctx=NULL; \
		if (header) { \
			dbghdr(level, __location__, __FUNCTION__); \
		} \
		dbgtext body; \
		talloc_free(_debug_ctx); \
	} \
} while (0)
/** 
 * Write to the debug log.
 */
#define DEBUG(level, body) _DEBUG(level, body, true)
/**
 * Add data to an existing debug log entry.
 */
#define DEBUGADD(level, body) _DEBUG(level, body, false)

/**
 * Obtain indentation string for the debug log. 
 *
 * Level specified by n.
 */
#define DEBUGTAB(n) do_debug_tab(n)

/** Possible destinations for the debug log (in order of precedence -
 * once set to DEBUG_FILE, it is not possible to reset to DEBUG_STDOUT
 * for example.  This makes it easy to override for debug to stderr on
 * the command line, as the smb.conf cannot reset it back to
 * file-based logging */
enum debug_logtype {DEBUG_STDOUT = 0, DEBUG_FILE = 1, DEBUG_STDERR = 2};

/**
  the backend for debug messages. Note that the DEBUG() macro has already
  ensured that the log level has been met before this is called
*/
_PUBLIC_ void dbghdr(int level, const char *location, const char *func);

_PUBLIC_ void dbghdrclass(int level, int cls, const char *location, const char *func);

/**
  reopen the log file (usually called because the log file name might have changed)
*/
_PUBLIC_ void reopen_logs(void);

/** 
 * this global variable determines what messages are printed 
 */
_PUBLIC_ void debug_schedule_reopen_logs(void);

/**
  control the name of the logfile and whether logging will be to stdout, stderr
  or a file
*/
_PUBLIC_ void setup_logging(const char *prog_name, enum debug_logtype new_logtype);

/**
   Just run logging to stdout for this program 
*/
_PUBLIC_ void setup_logging_stdout(void);

/**
  return a string constant containing n tabs
  no more than 10 tabs are returned
*/
_PUBLIC_ const char *do_debug_tab(int n);

/**
  log suspicious usage - print comments and backtrace
*/	
_PUBLIC_ void log_suspicious_usage(const char *from, const char *info);

/**
  print suspicious usage - print comments and backtrace
*/	
_PUBLIC_ void print_suspicious_usage(const char* from, const char* info);
_PUBLIC_ uint32_t get_task_id(void);
_PUBLIC_ void log_task_id(void);

/**
  register a set of debug handlers. 
*/
_PUBLIC_ void register_debug_handlers(const char *name, struct debug_ops *ops);

/**
  the backend for debug messages. Note that the DEBUG() macro has already
  ensured that the log level has been met before this is called

  @note You should never have to call this function directly. Call the DEBUG()
  macro instead.
*/
_PUBLIC_ void dbgtext(const char *format, ...) PRINTF_ATTRIBUTE(1,2);

struct _XFILE;
extern struct _XFILE *dbf;

#endif
