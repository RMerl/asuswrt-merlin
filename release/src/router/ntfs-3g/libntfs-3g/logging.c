/**
 * logging.c - Centralised logging.  Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2005 Richard Russon
 * Copyright (c) 2005-2008 Szabolcs Szakacsits
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include "logging.h"
#include "misc.h"

#ifndef PATH_SEP
#define PATH_SEP '/'
#endif

#ifdef DEBUG
static int tab;
#endif

/* Some gcc 3.x, 4.[01].X crash with internal compiler error. */
#if __GNUC__ <= 3 || (__GNUC__ == 4 && __GNUC_MINOR__ <= 1)
# define  BROKEN_GCC_FORMAT_ATTRIBUTE
#else
# define  BROKEN_GCC_FORMAT_ATTRIBUTE __attribute__((format(printf, 6, 0)))
#endif

/**
 * struct ntfs_logging - Control info for the logging system
 * @levels:	Bitfield of logging levels
 * @flags:	Flags which affect the output style
 * @handler:	Function to perform the actual logging
 */
struct ntfs_logging {
	u32 levels;
	u32 flags;
	ntfs_log_handler *handler BROKEN_GCC_FORMAT_ATTRIBUTE;
};

/**
 * ntfs_log
 * This struct controls all the logging within the library and tools.
 */
static struct ntfs_logging ntfs_log = {
#ifdef DEBUG
	NTFS_LOG_LEVEL_DEBUG | NTFS_LOG_LEVEL_TRACE | NTFS_LOG_LEVEL_ENTER |
	NTFS_LOG_LEVEL_LEAVE |
#endif
	NTFS_LOG_LEVEL_INFO | NTFS_LOG_LEVEL_QUIET | NTFS_LOG_LEVEL_WARNING |
	NTFS_LOG_LEVEL_ERROR | NTFS_LOG_LEVEL_PERROR | NTFS_LOG_LEVEL_CRITICAL |
	NTFS_LOG_LEVEL_PROGRESS,
	NTFS_LOG_FLAG_ONLYNAME,
#ifdef DEBUG
	ntfs_log_handler_outerr
#else
	ntfs_log_handler_null
#endif
};


/**
 * ntfs_log_get_levels - Get a list of the current logging levels
 *
 * Find out which logging levels are enabled.
 *
 * Returns:  Log levels in a 32-bit field
 */
u32 ntfs_log_get_levels(void)
{
	return ntfs_log.levels;
}

/**
 * ntfs_log_set_levels - Enable extra logging levels
 * @levels:	32-bit field of log levels to set
 *
 * Enable one or more logging levels.
 * The logging levels are named: NTFS_LOG_LEVEL_*.
 *
 * Returns:  Log levels that were enabled before the call
 */
u32 ntfs_log_set_levels(u32 levels)
{
	u32 old;
	old = ntfs_log.levels;
	ntfs_log.levels |= levels;
	return old;
}

/**
 * ntfs_log_clear_levels - Disable some logging levels
 * @levels:	32-bit field of log levels to clear
 *
 * Disable one or more logging levels.
 * The logging levels are named: NTFS_LOG_LEVEL_*.
 *
 * Returns:  Log levels that were enabled before the call
 */
u32 ntfs_log_clear_levels(u32 levels)
{
	u32 old;
	old = ntfs_log.levels;
	ntfs_log.levels &= (~levels);
	return old;
}


/**
 * ntfs_log_get_flags - Get a list of logging style flags
 *
 * Find out which logging flags are enabled.
 *
 * Returns:  Logging flags in a 32-bit field
 */
u32 ntfs_log_get_flags(void)
{
	return ntfs_log.flags;
}

/**
 * ntfs_log_set_flags - Enable extra logging style flags
 * @flags:	32-bit field of logging flags to set
 *
 * Enable one or more logging flags.
 * The log flags are named: NTFS_LOG_LEVEL_*.
 *
 * Returns:  Logging flags that were enabled before the call
 */
u32 ntfs_log_set_flags(u32 flags)
{
	u32 old;
	old = ntfs_log.flags;
	ntfs_log.flags |= flags;
	return old;
}

/**
 * ntfs_log_clear_flags - Disable some logging styles
 * @flags:	32-bit field of logging flags to clear
 *
 * Disable one or more logging flags.
 * The log flags are named: NTFS_LOG_LEVEL_*.
 *
 * Returns:  Logging flags that were enabled before the call
 */
u32 ntfs_log_clear_flags(u32 flags)
{
	u32 old;
	old = ntfs_log.flags;
	ntfs_log.flags &= (~flags);
	return old;
}


/**
 * ntfs_log_get_stream - Default output streams for logging levels
 * @level:	Log level
 *
 * By default, urgent messages are sent to "stderr".
 * Other messages are sent to "stdout".
 *
 * Returns:  "string"  Prefix to be used
 */
static FILE * ntfs_log_get_stream(u32 level)
{
	FILE *stream;

	switch (level) {
		case NTFS_LOG_LEVEL_INFO:
		case NTFS_LOG_LEVEL_QUIET:
		case NTFS_LOG_LEVEL_PROGRESS:
		case NTFS_LOG_LEVEL_VERBOSE:
			stream = stdout;
			break;

		case NTFS_LOG_LEVEL_DEBUG:
		case NTFS_LOG_LEVEL_TRACE:
		case NTFS_LOG_LEVEL_ENTER:
		case NTFS_LOG_LEVEL_LEAVE:
		case NTFS_LOG_LEVEL_WARNING:
		case NTFS_LOG_LEVEL_ERROR:
		case NTFS_LOG_LEVEL_CRITICAL:
		case NTFS_LOG_LEVEL_PERROR:
		default:
			stream = stderr;
			break;
	}

	return stream;
}

/**
 * ntfs_log_get_prefix - Default prefixes for logging levels
 * @level:	Log level to be prefixed
 *
 * Prefixing the logging output can make it easier to parse.
 *
 * Returns:  "string"  Prefix to be used
 */
static const char * ntfs_log_get_prefix(u32 level)
{
	const char *prefix;

	switch (level) {
		case NTFS_LOG_LEVEL_DEBUG:
			prefix = "DEBUG: ";
			break;
		case NTFS_LOG_LEVEL_TRACE:
			prefix = "TRACE: ";
			break;
		case NTFS_LOG_LEVEL_QUIET:
			prefix = "QUIET: ";
			break;
		case NTFS_LOG_LEVEL_INFO:
			prefix = "INFO: ";
			break;
		case NTFS_LOG_LEVEL_VERBOSE:
			prefix = "VERBOSE: ";
			break;
		case NTFS_LOG_LEVEL_PROGRESS:
			prefix = "PROGRESS: ";
			break;
		case NTFS_LOG_LEVEL_WARNING:
			prefix = "WARNING: ";
			break;
		case NTFS_LOG_LEVEL_ERROR:
			prefix = "ERROR: ";
			break;
		case NTFS_LOG_LEVEL_PERROR:
			prefix = "ERROR: ";
			break;
		case NTFS_LOG_LEVEL_CRITICAL:
			prefix = "CRITICAL: ";
			break;
		default:
			prefix = "";
			break;
	}

	return prefix;
}


/**
 * ntfs_log_set_handler - Provide an alternate logging handler
 * @handler:	function to perform the logging
 *
 * This alternate handler will be called for all future logging requests.
 * If no @handler is specified, logging will revert to the default handler.
 */
void ntfs_log_set_handler(ntfs_log_handler *handler)
{
	if (handler) {
		ntfs_log.handler = handler;
#ifdef HAVE_SYSLOG_H
		if (handler == ntfs_log_handler_syslog)
			openlog("ntfs-3g", LOG_PID, LOG_USER);
#endif
	} else
		ntfs_log.handler = ntfs_log_handler_null;
}

/**
 * ntfs_log_redirect - Pass on the request to the real handler
 * @function:	Function in which the log line occurred
 * @file:	File in which the log line occurred
 * @line:	Line number on which the log line occurred
 * @level:	Level at which the line is logged
 * @data:	User specified data, possibly specific to a handler
 * @format:	printf-style formatting string
 * @...:	Arguments to be formatted
 *
 * This is just a redirector function.  The arguments are simply passed to the
 * main logging handler (as defined in the global logging struct @ntfs_log).
 *
 * Returns:  -1  Error occurred
 *            0  Message wasn't logged
 *          num  Number of output characters
 */
int ntfs_log_redirect(const char *function, const char *file,
	int line, u32 level, void *data, const char *format, ...)
{
	int olderr = errno;
	int ret;
	va_list args;

	if (!(ntfs_log.levels & level))		/* Don't log this message */
		return 0;

	va_start(args, format);
	errno = olderr;
	ret = ntfs_log.handler(function, file, line, level, data, format, args);
	va_end(args);

	errno = olderr;
	return ret;
}


/**
 * ntfs_log_handler_syslog - syslog logging handler
 * @function:	Function in which the log line occurred
 * @file:	File in which the log line occurred
 * @line:	Line number on which the log line occurred
 * @level:	Level at which the line is logged
 * @data:	User specified data, possibly specific to a handler
 * @format:	printf-style formatting string
 * @args:	Arguments to be formatted
 *
 * A simple syslog logging handler.  Ignores colors.
 *
 * Returns:  -1  Error occurred
 *            0  Message wasn't logged
 *          num  Number of output characters
 */


#ifdef HAVE_SYSLOG_H

#define LOG_LINE_LEN 	512

int ntfs_log_handler_syslog(const char *function  __attribute__((unused)),
			    const char *file __attribute__((unused)), 
			    int line __attribute__((unused)), u32 level, 
			    void *data __attribute__((unused)), 
			    const char *format, va_list args)
{
	char logbuf[LOG_LINE_LEN];
	int ret, olderr = errno;

#ifndef DEBUG
	if ((level & NTFS_LOG_LEVEL_PERROR) && errno == ENOSPC)
		return 1;
#endif	
	ret = vsnprintf(logbuf, LOG_LINE_LEN, format, args);
	if (ret < 0) {
		vsyslog(LOG_NOTICE, format, args);
		ret = 1;
		goto out;
	}
	
	if ((LOG_LINE_LEN > ret + 3) && (level & NTFS_LOG_LEVEL_PERROR)) {
		strncat(logbuf, ": ", LOG_LINE_LEN - ret - 1);
		strncat(logbuf, strerror(olderr), LOG_LINE_LEN - (ret + 3));
		ret = strlen(logbuf);
	}
	
	syslog(LOG_NOTICE, "%s", logbuf);
out:
	errno = olderr;
	return ret;
}
#endif

/**
 * ntfs_log_handler_fprintf - Basic logging handler
 * @function:	Function in which the log line occurred
 * @file:	File in which the log line occurred
 * @line:	Line number on which the log line occurred
 * @level:	Level at which the line is logged
 * @data:	User specified data, possibly specific to a handler
 * @format:	printf-style formatting string
 * @args:	Arguments to be formatted
 *
 * A simple logging handler.  This is where the log line is finally displayed.
 * It is more likely that you will want to set the handler to either
 * ntfs_log_handler_outerr or ntfs_log_handler_stderr.
 *
 * Note: For this handler, @data is a pointer to a FILE output stream.
 *       If @data is NULL, nothing will be displayed.
 *
 * Returns:  -1  Error occurred
 *            0  Message wasn't logged
 *          num  Number of output characters
 */
int ntfs_log_handler_fprintf(const char *function, const char *file,
	int line, u32 level, void *data, const char *format, va_list args)
{
#ifdef DEBUG
	int i;
#endif
	int ret = 0;
	int olderr = errno;
	FILE *stream;

	if (!data)		/* Interpret data as a FILE stream. */
		return 0;	/* If it's NULL, we can't do anything. */
	stream = (FILE*)data;

#ifdef DEBUG
	if (level == NTFS_LOG_LEVEL_LEAVE) {
		if (tab)
			tab--;
		return 0;
	}
	
	for (i = 0; i < tab; i++)
		ret += fprintf(stream, " ");
#endif	
	if ((ntfs_log.flags & NTFS_LOG_FLAG_ONLYNAME) &&
	    (strchr(file, PATH_SEP)))		/* Abbreviate the filename */
		file = strrchr(file, PATH_SEP) + 1;

	if (ntfs_log.flags & NTFS_LOG_FLAG_PREFIX)	/* Prefix the output */
		ret += fprintf(stream, "%s", ntfs_log_get_prefix(level));

	if (ntfs_log.flags & NTFS_LOG_FLAG_FILENAME)	/* Source filename */
		ret += fprintf(stream, "%s ", file);

	if (ntfs_log.flags & NTFS_LOG_FLAG_LINE)	/* Source line number */
		ret += fprintf(stream, "(%d) ", line);

	if ((ntfs_log.flags & NTFS_LOG_FLAG_FUNCTION) || /* Source function */
	    (level & NTFS_LOG_LEVEL_TRACE) || (level & NTFS_LOG_LEVEL_ENTER))
		ret += fprintf(stream, "%s(): ", function);

	ret += vfprintf(stream, format, args);

	if (level & NTFS_LOG_LEVEL_PERROR)
		ret += fprintf(stream, ": %s\n", strerror(olderr));

#ifdef DEBUG
	if (level == NTFS_LOG_LEVEL_ENTER)
		tab++;
#endif	
	fflush(stream);
	errno = olderr;
	return ret;
}

/**
 * ntfs_log_handler_null - Null logging handler (no output)
 * @function:	Function in which the log line occurred
 * @file:	File in which the log line occurred
 * @line:	Line number on which the log line occurred
 * @level:	Level at which the line is logged
 * @data:	User specified data, possibly specific to a handler
 * @format:	printf-style formatting string
 * @args:	Arguments to be formatted
 *
 * This handler produces no output.  It provides a way to temporarily disable
 * logging, without having to change the levels and flags.
 *
 * Returns:  0  Message wasn't logged
 */
int ntfs_log_handler_null(const char *function __attribute__((unused)), const char *file __attribute__((unused)),
	int line __attribute__((unused)), u32 level __attribute__((unused)), void *data __attribute__((unused)),
	const char *format __attribute__((unused)), va_list args __attribute__((unused)))
{
	return 0;
}

/**
 * ntfs_log_handler_stdout - All logs go to stdout
 * @function:	Function in which the log line occurred
 * @file:	File in which the log line occurred
 * @line:	Line number on which the log line occurred
 * @level:	Level at which the line is logged
 * @data:	User specified data, possibly specific to a handler
 * @format:	printf-style formatting string
 * @args:	Arguments to be formatted
 *
 * Display a log message to stdout.
 *
 * Note: For this handler, @data is a pointer to a FILE output stream.
 *       If @data is NULL, then stdout will be used.
 *
 * Note: This function calls ntfs_log_handler_fprintf to do the main work.
 *
 * Returns:  -1  Error occurred
 *            0  Message wasn't logged
 *          num  Number of output characters
 */
int ntfs_log_handler_stdout(const char *function, const char *file,
	int line, u32 level, void *data, const char *format, va_list args)
{
	if (!data)
		data = stdout;

	return ntfs_log_handler_fprintf(function, file, line, level, data, format, args);
}

/**
 * ntfs_log_handler_outerr - Logs go to stdout/stderr depending on level
 * @function:	Function in which the log line occurred
 * @file:	File in which the log line occurred
 * @line:	Line number on which the log line occurred
 * @level:	Level at which the line is logged
 * @data:	User specified data, possibly specific to a handler
 * @format:	printf-style formatting string
 * @args:	Arguments to be formatted
 *
 * Display a log message.  The output stream will be determined by the log
 * level.
 *
 * Note: For this handler, @data is a pointer to a FILE output stream.
 *       If @data is NULL, the function ntfs_log_get_stream will be called
 *
 * Note: This function calls ntfs_log_handler_fprintf to do the main work.
 *
 * Returns:  -1  Error occurred
 *            0  Message wasn't logged
 *          num  Number of output characters
 */
int ntfs_log_handler_outerr(const char *function, const char *file,
	int line, u32 level, void *data, const char *format, va_list args)
{
	if (!data)
		data = ntfs_log_get_stream(level);

	return ntfs_log_handler_fprintf(function, file, line, level, data, format, args);
}

/**
 * ntfs_log_handler_stderr - All logs go to stderr
 * @function:	Function in which the log line occurred
 * @file:	File in which the log line occurred
 * @line:	Line number on which the log line occurred
 * @level:	Level at which the line is logged
 * @data:	User specified data, possibly specific to a handler
 * @format:	printf-style formatting string
 * @args:	Arguments to be formatted
 *
 * Display a log message to stderr.
 *
 * Note: For this handler, @data is a pointer to a FILE output stream.
 *       If @data is NULL, then stdout will be used.
 *
 * Note: This function calls ntfs_log_handler_fprintf to do the main work.
 *
 * Returns:  -1  Error occurred
 *            0  Message wasn't logged
 *          num  Number of output characters
 */
int ntfs_log_handler_stderr(const char *function, const char *file,
	int line, u32 level, void *data, const char *format, va_list args)
{
	if (!data)
		data = stderr;

	return ntfs_log_handler_fprintf(function, file, line, level, data, format, args);
}


/**
 * ntfs_log_parse_option - Act upon command line options
 * @option:	Option flag
 *
 * Delegate some of the work of parsing the command line.  All the options begin
 * with "--log-".  Options cause log levels to be enabled in @ntfs_log (the
 * global logging structure).
 *
 * Note: The "colour" option changes the logging handler.
 *
 * Returns:  TRUE  Option understood
 *          FALSE  Invalid log option
 */
BOOL ntfs_log_parse_option(const char *option)
{
	if (strcmp(option, "--log-debug") == 0) {
		ntfs_log_set_levels(NTFS_LOG_LEVEL_DEBUG);
		return TRUE;
	} else if (strcmp(option, "--log-verbose") == 0) {
		ntfs_log_set_levels(NTFS_LOG_LEVEL_VERBOSE);
		return TRUE;
	} else if (strcmp(option, "--log-quiet") == 0) {
		ntfs_log_clear_levels(NTFS_LOG_LEVEL_QUIET);
		return TRUE;
	} else if (strcmp(option, "--log-trace") == 0) {
		ntfs_log_set_levels(NTFS_LOG_LEVEL_TRACE);
		return TRUE;
	}

	ntfs_log_debug("Unknown logging option '%s'\n", option);
	return FALSE;
}

