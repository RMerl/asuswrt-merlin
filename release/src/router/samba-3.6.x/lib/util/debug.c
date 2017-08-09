/*
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Elrond               2002
   Copyright (C) Simo Sorce           2002

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
#include "system/syslog.h"
#include "lib/util/time.h"

/* define what facility to use for syslog */
#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY LOG_DAEMON
#endif

/* -------------------------------------------------------------------------- **
 * Defines...
 *
 *  FORMAT_BUFR_MAX - Index of the last byte of the format buffer;
 *                    format_bufr[FORMAT_BUFR_MAX] should always be reserved
 *                    for a terminating null byte.
 */

#define FORMAT_BUFR_SIZE 1024
#define FORMAT_BUFR_MAX (FORMAT_BUFR_SIZE - 1)

/* -------------------------------------------------------------------------- **
 * This module implements Samba's debugging utility.
 *
 * The syntax of a debugging log file is represented as:
 *
 *  <debugfile> :== { <debugmsg> }
 *
 *  <debugmsg>  :== <debughdr> '\n' <debugtext>
 *
 *  <debughdr>  :== '[' TIME ',' LEVEL ']' [ [FILENAME ':'] [FUNCTION '()'] ]
 *
 *  <debugtext> :== { <debugline> }
 *
 *  <debugline> :== TEXT '\n'
 *
 * TEXT     is a string of characters excluding the newline character.
 * LEVEL    is the DEBUG level of the message (an integer in the range 0..10).
 * TIME     is a timestamp.
 * FILENAME is the name of the file from which the debug message was generated.
 * FUNCTION is the function from which the debug message was generated.
 *
 * Basically, what that all means is:
 *
 * - A debugging log file is made up of debug messages.
 *
 * - Each debug message is made up of a header and text.  The header is
 *   separated from the text by a newline.
 *
 * - The header begins with the timestamp and debug level of the message
 *   enclosed in brackets.  The filename and function from which the
 *   message was generated may follow.  The filename is terminated by a
 *   colon, and the function name is terminated by parenthesis.
 *
 * - The message text is made up of zero or more lines, each terminated by
 *   a newline.
 */

/* state variables for the debug system */
static struct {
	bool initialized;
	int fd;   /* The log file handle */
	enum debug_logtype logtype; /* The type of logging we are doing: eg stdout, file, stderr */
	const char *prog_name;
	bool reopening_logs;
	bool schedule_reopen_logs;

	struct debug_settings settings;
	char *debugf;
} state = {
	.settings = {
		.timestamp_logs = true
	}
};

/* -------------------------------------------------------------------------- **
 * External variables.
 *
 *  debugf        - Debug file name.
 *  DEBUGLEVEL    - System-wide debug message limit.  Messages with message-
 *                  levels higher than DEBUGLEVEL will not be processed.
 */

/*
   used to check if the user specified a
   logfile on the command line
*/
bool    override_logfile;

/*
 * This is to allow reading of DEBUGLEVEL_CLASS before the debug
 * system has been initialized.
 */
static const int debug_class_list_initial[DBGC_MAX_FIXED + 1];

static int debug_num_classes = 0;
int     *DEBUGLEVEL_CLASS = discard_const_p(int, debug_class_list_initial);


/* -------------------------------------------------------------------------- **
 * Internal variables.
 *
 *  debug_count     - Number of debug messages that have been output.
 *                    Used to check log size.
 *
 *  syslog_level    - Internal copy of the message debug level.  Written by
 *                    dbghdr() and read by Debug1().
 *
 *  format_bufr     - Used to format debug messages.  The dbgtext() function
 *                    prints debug messages to a string, and then passes the
 *                    string to format_debug_text(), which uses format_bufr
 *                    to build the formatted output.
 *
 *  format_pos      - Marks the first free byte of the format_bufr.
 *
 *
 *  log_overflow    - When this variable is true, never attempt to check the
 *                    size of the log. This is a hack, so that we can write
 *                    a message using DEBUG, from open_logs() when we
 *                    are unable to open a new log file for some reason.
 */

static int     debug_count    = 0;
#ifdef WITH_SYSLOG
static int     syslog_level   = 0;
#endif
static char *format_bufr = NULL;
static size_t     format_pos     = 0;
static bool    log_overflow   = false;

/*
 * Define all the debug class selection names here. Names *MUST NOT* contain
 * white space. There must be one name for each DBGC_<class name>, and they
 * must be in the table in the order of DBGC_<class name>..
 */
static const char *default_classname_table[] = {
	"all",               /* DBGC_ALL; index refs traditional DEBUGLEVEL */
	"tdb",               /* DBGC_TDB	  */
	"printdrivers",      /* DBGC_PRINTDRIVERS */
	"lanman",            /* DBGC_LANMAN       */
	"smb",               /* DBGC_SMB          */
	"rpc_parse",         /* DBGC_RPC_PARSE    */
	"rpc_srv",           /* DBGC_RPC_SRV      */
	"rpc_cli",           /* DBGC_RPC_CLI      */
	"passdb",            /* DBGC_PASSDB       */
	"sam",               /* DBGC_SAM          */
	"auth",              /* DBGC_AUTH         */
	"winbind",           /* DBGC_WINBIND      */
	"vfs",		     /* DBGC_VFS	  */
	"idmap",	     /* DBGC_IDMAP	  */
	"quota",	     /* DBGC_QUOTA	  */
	"acls",		     /* DBGC_ACLS	  */
	"locking",	     /* DBGC_LOCKING	  */
	"msdfs",	     /* DBGC_MSDFS	  */
	"dmapi",	     /* DBGC_DMAPI	  */
	"registry",          /* DBGC_REGISTRY     */
	NULL
};

static char **classname_table = NULL;


/* -------------------------------------------------------------------------- **
 * Functions...
 */

static void debug_init(void);

/***************************************************************************
 Free memory pointed to by global pointers.
****************************************************************************/

void gfree_debugsyms(void)
{
	TALLOC_FREE(classname_table);

	if ( DEBUGLEVEL_CLASS != debug_class_list_initial ) {
		TALLOC_FREE( DEBUGLEVEL_CLASS );
		DEBUGLEVEL_CLASS = discard_const_p(int, debug_class_list_initial);
	}

	TALLOC_FREE(format_bufr);

	debug_num_classes = 0;

	state.initialized = false;
}

/****************************************************************************
utility lists registered debug class names's
****************************************************************************/

char *debug_list_class_names_and_levels(void)
{
	char *buf = NULL;
	unsigned int i;
	/* prepare strings */
	for (i = 0; i < debug_num_classes; i++) {
		buf = talloc_asprintf_append(buf, 
					     "%s:%d%s",
					     classname_table[i],
					     DEBUGLEVEL_CLASS[i],
					     i == (debug_num_classes - 1) ? "\n" : " ");
		if (buf == NULL) {
			return NULL;
		}
	}
	return buf;
}

/****************************************************************************
 Utility to translate names to debug class index's (internal version).
****************************************************************************/

static int debug_lookup_classname_int(const char* classname)
{
	int i;

	if (!classname) return -1;

	for (i=0; i < debug_num_classes; i++) {
		if (strcmp(classname, classname_table[i])==0)
			return i;
	}
	return -1;
}

/****************************************************************************
 Add a new debug class to the system.
****************************************************************************/

int debug_add_class(const char *classname)
{
	int ndx;
	int *new_class_list;
	char **new_name_list;
	int default_level;

	if (!classname)
		return -1;

	/* check the init has yet been called */
	debug_init();

	ndx = debug_lookup_classname_int(classname);
	if (ndx >= 0)
		return ndx;
	ndx = debug_num_classes;

	if (DEBUGLEVEL_CLASS == debug_class_list_initial) {
		/* Initial loading... */
		new_class_list = NULL;
	} else {
		new_class_list = DEBUGLEVEL_CLASS;
	}

	default_level = DEBUGLEVEL_CLASS[DBGC_ALL];

	new_class_list = talloc_realloc(NULL, new_class_list, int, ndx + 1);
	if (!new_class_list)
		return -1;
	DEBUGLEVEL_CLASS = new_class_list;

	DEBUGLEVEL_CLASS[ndx] = default_level;

	new_name_list = talloc_realloc(NULL, classname_table, char *, ndx + 1);
	if (!new_name_list)
		return -1;
	classname_table = new_name_list;

	classname_table[ndx] = talloc_strdup(classname_table, classname);
	if (! classname_table[ndx])
		return -1;

	debug_num_classes = ndx + 1;

	return ndx;
}

/****************************************************************************
 Utility to translate names to debug class index's (public version).
****************************************************************************/

int debug_lookup_classname(const char *classname)
{
	int ndx;

	if (!classname || !*classname)
		return -1;

	ndx = debug_lookup_classname_int(classname);

	if (ndx != -1)
		return ndx;

	DEBUG(0, ("debug_lookup_classname(%s): Unknown class\n",
		  classname));
	return debug_add_class(classname);
}

/****************************************************************************
 Dump the current registered debug levels.
****************************************************************************/

static void debug_dump_status(int level)
{
	int q;

	DEBUG(level, ("INFO: Current debug levels:\n"));
	for (q = 0; q < debug_num_classes; q++) {
		const char *classname = classname_table[q];
		DEBUGADD(level, ("  %s: %d\n",
				 classname,
				 DEBUGLEVEL_CLASS[q]));
	}
}

/****************************************************************************
 parse the debug levels from smbcontrol. Example debug level parameter:
 printdrivers:7
****************************************************************************/

static bool debug_parse_params(char **params)
{
	int   i, ndx;
	char *class_name;
	char *class_level;

	if (!params)
		return false;

	/* Allow DBGC_ALL to be specified w/o requiring its class name e.g."10"
	 * v.s. "all:10", this is the traditional way to set DEBUGLEVEL
	 */
	if (isdigit((int)params[0][0])) {
		DEBUGLEVEL_CLASS[DBGC_ALL] = atoi(params[0]);
		i = 1; /* start processing at the next params */
	} else {
		DEBUGLEVEL_CLASS[DBGC_ALL] = 0;
		i = 0; /* DBGC_ALL not specified OR class name was included */
	}

	/* Array is debug_num_classes long */
	for (ndx = DBGC_ALL; ndx < debug_num_classes; ndx++) {
		DEBUGLEVEL_CLASS[ndx] = DEBUGLEVEL_CLASS[DBGC_ALL];
	}
		
	/* Fill in new debug class levels */
	for (; i < debug_num_classes && params[i]; i++) {
		char *saveptr;
		if ((class_name = strtok_r(params[i],":", &saveptr)) &&
			(class_level = strtok_r(NULL, "\0", &saveptr)) &&
            ((ndx = debug_lookup_classname(class_name)) != -1)) {
				DEBUGLEVEL_CLASS[ndx] = atoi(class_level);
		} else {
			DEBUG(0,("debug_parse_params: unrecognized debug class name or format [%s]\n", params[i]));
			return false;
		}
	}

	return true;
}

/****************************************************************************
 Parse the debug levels from smb.conf. Example debug level string:
  3 tdb:5 printdrivers:7
 Note: the 1st param has no "name:" preceeding it.
****************************************************************************/

bool debug_parse_levels(const char *params_str)
{
	char **params;

	/* Just in case */
	debug_init();

	params = str_list_make(NULL, params_str, NULL);

	if (debug_parse_params(params)) {
		debug_dump_status(5);
		TALLOC_FREE(params);
		return true;
	} else {
		TALLOC_FREE(params);
		return false;
	}
}

/* setup for logging of talloc warnings */
static void talloc_log_fn(const char *msg)
{
	DEBUG(0,("%s", msg));
}

void debug_setup_talloc_log(void)
{
	talloc_set_log_fn(talloc_log_fn);
}


/****************************************************************************
Init debugging (one time stuff)
****************************************************************************/

static void debug_init(void)
{
	const char **p;

	if (state.initialized)
		return;

	state.initialized = true;

	debug_setup_talloc_log();

	for(p = default_classname_table; *p; p++) {
		debug_add_class(*p);
	}
	format_bufr = talloc_array(NULL, char, FORMAT_BUFR_SIZE);
	if (!format_bufr) {
		smb_panic("debug_init: unable to create buffer");
	}
}

/* This forces in some smb.conf derived values into the debug system.
 * There are no pointers in this structure, so we can just
 * structure-assign it in */
void debug_set_settings(struct debug_settings *settings)
{
	state.settings = *settings;
}

/**
  control the name of the logfile and whether logging will be to stdout, stderr
  or a file, and set up syslog

  new_log indicates the destination for the debug log (an enum in
  order of precedence - once set to DEBUG_FILE, it is not possible to
  reset to DEBUG_STDOUT for example.  This makes it easy to override
  for debug to stderr on the command line, as the smb.conf cannot
  reset it back to file-based logging
*/
void setup_logging(const char *prog_name, enum debug_logtype new_logtype)
{
	debug_init();
	if (state.logtype < new_logtype) {
		state.logtype = new_logtype;
	}
	if (prog_name) {
		state.prog_name = prog_name;
	}
	reopen_logs_internal();

	if (state.logtype == DEBUG_FILE) {
#ifdef WITH_SYSLOG
		const char *p = strrchr_m( prog_name,'/' );
		if (p)
			prog_name = p + 1;
#ifdef LOG_DAEMON
		openlog( prog_name, LOG_PID, SYSLOG_FACILITY );
#else
		/* for old systems that have no facility codes. */
		openlog( prog_name, LOG_PID );
#endif
#endif
	}
}

/***************************************************************************
 Set the logfile name.
**************************************************************************/

void debug_set_logfile(const char *name)
{
	if (name == NULL || *name == 0) {
		/* this copes with calls when smb.conf is not loaded yet */
		return;
	}
	TALLOC_FREE(state.debugf);
	state.debugf = talloc_strdup(NULL, name);
}

static void debug_close_fd(int fd)
{
	if (fd > 2) {
		close(fd);
	}
}

bool debug_get_output_is_stderr(void)
{
	return (state.logtype == DEBUG_DEFAULT_STDERR) || (state.logtype == DEBUG_STDERR);
}

/**************************************************************************
 reopen the log files
 note that we now do this unconditionally
 We attempt to open the new debug fp before closing the old. This means
 if we run out of fd's we just keep using the old fd rather than aborting.
 Fix from dgibson@linuxcare.com.
**************************************************************************/

/**
  reopen the log file (usually called because the log file name might have changed)
*/
bool reopen_logs_internal(void)
{
	mode_t oldumask;
	int new_fd = 0;
	int old_fd = 0;
	bool ret = true;

	char *fname = NULL;
	if (state.reopening_logs) {
		return true;
	}

	/* Now clear the SIGHUP induced flag */
	state.schedule_reopen_logs = false;

	switch (state.logtype) {
	case DEBUG_STDOUT:
		debug_close_fd(state.fd);
		state.fd = 1;
		return true;

	case DEBUG_DEFAULT_STDERR:
	case DEBUG_STDERR:
		debug_close_fd(state.fd);
		state.fd = 2;
		return true;

	case DEBUG_FILE:
		break;
	}

	oldumask = umask( 022 );

	fname = state.debugf;
	if (!fname) {
		return false;
	}

	state.reopening_logs = true;

	new_fd = open( state.debugf, O_WRONLY|O_APPEND|O_CREAT, 0644);

	if (new_fd == -1) {
		log_overflow = true;
		DEBUG(0, ("Unable to open new log file '%s': %s\n", state.debugf, strerror(errno)));
		log_overflow = false;
		ret = false;
	} else {
		old_fd = state.fd;
		state.fd = new_fd;
		debug_close_fd(old_fd);
	}

	/* Fix from klausr@ITAP.Physik.Uni-Stuttgart.De
	 * to fix problem where smbd's that generate less
	 * than 100 messages keep growing the log.
	 */
	force_check_log_size();
	(void)umask(oldumask);

	/* Take over stderr to catch output into logs */
	if (state.fd > 0 && dup2(state.fd, 2) == -1) {
		close_low_fds(true); /* Close stderr too, if dup2 can't point it
					at the logfile */
	}

	state.reopening_logs = false;

	return ret;
}

/**************************************************************************
 Force a check of the log size.
 ***************************************************************************/

void force_check_log_size( void )
{
	debug_count = 100;
}

_PUBLIC_ void debug_schedule_reopen_logs(void)
{
	state.schedule_reopen_logs = true;
}


/***************************************************************************
 Check to see if there is any need to check if the logfile has grown too big.
**************************************************************************/

bool need_to_check_log_size( void )
{
	int maxlog;

	if( debug_count < 100)
		return( false );

	maxlog = state.settings.max_log_size * 1024;
	if ( state.fd <=2 || maxlog <= 0 ) {
		debug_count = 0;
		return(false);
	}
	return( true );
}

/**************************************************************************
 Check to see if the log has grown to be too big.
 **************************************************************************/

void check_log_size( void )
{
	int         maxlog;
	struct stat st;

	/*
	 *  We need to be root to check/change log-file, skip this and let the main
	 *  loop check do a new check as root.
	 */

	if( geteuid() != 0) {
		/* We don't check sec_initial_uid() here as it isn't
		 * available in common code and we don't generally
		 * want to rotate and the possibly lose logs in
		 * make test or the build farm */
		return;
	}

	if(log_overflow || (!state.schedule_reopen_logs && !need_to_check_log_size())) {
		return;
	}

	maxlog = state.settings.max_log_size * 1024;

	if (state.schedule_reopen_logs ||
	   (fstat(state.fd, &st) == 0
	    && st.st_size > maxlog )) {
		(void)reopen_logs_internal();
		if (state.fd > 0 && fstat(state.fd, &st) == 0) {
			if (st.st_size > maxlog) {
				char *name = NULL;

				if (asprintf(&name, "%s.old", state.debugf ) < 0) {
					return;
				}
				(void)rename(state.debugf, name);

				if (!reopen_logs_internal()) {
					/* We failed to reopen a log - continue using the old name. */
					(void)rename(name, state.debugf);
				}
				SAFE_FREE(name);
			}
		}
	}

	/*
	 * Here's where we need to panic if state.fd == 0 or -1 (invalid values)
	 */

	if (state.fd <= 0) {
		/* This code should only be reached in very strange
		 * circumstances. If we merely fail to open the new log we
		 * should stick with the old one. ergo this should only be
		 * reached when opening the logs for the first time: at
		 * startup or when the log level is increased from zero.
		 * -dwg 6 June 2000
		 */
		int fd = open( "/dev/console", O_WRONLY, 0);
		if (fd != -1) {
			state.fd = fd;
			DEBUG(0,("check_log_size: open of debug file %s failed - using console.\n",
					state.debugf ));
		} else {
			/*
			 * We cannot continue without a debug file handle.
			 */
			abort();
		}
	}
	debug_count = 0;
}

/*************************************************************************
 Write an debug message on the debugfile.
 This is called by dbghdr() and format_debug_text().
************************************************************************/

 int Debug1( const char *format_str, ... )
{
	va_list ap;
	int old_errno = errno;

	debug_count++;

	if ( state.logtype != DEBUG_FILE ) {
		va_start( ap, format_str );
		if (state.fd > 0)
			(void)vdprintf( state.fd, format_str, ap );
		va_end( ap );
		errno = old_errno;
		goto done;
	}

#ifdef WITH_SYSLOG
	if( !state.settings.syslog_only)
#endif
	{
		if( state.fd <= 0 ) {
			mode_t oldumask = umask( 022 );
			int fd = open( state.debugf, O_WRONLY|O_APPEND|O_CREAT, 0644 );
			(void)umask( oldumask );
			if(fd == -1) {
				errno = old_errno;
				goto done;
			}
			state.fd = fd;
		}
	}

#ifdef WITH_SYSLOG
	if( syslog_level < state.settings.syslog ) {
		/* map debug levels to syslog() priorities
		 * note that not all DEBUG(0, ...) calls are
		 * necessarily errors */
		static const int priority_map[4] = {
			LOG_ERR,     /* 0 */
			LOG_WARNING, /* 1 */
			LOG_NOTICE,  /* 2 */
			LOG_INFO,    /* 3 */
		};
		int     priority;
		char *msgbuf = NULL;
		int ret;

		if( syslog_level >= ARRAY_SIZE(priority_map) || syslog_level < 0)
			priority = LOG_DEBUG;
		else
			priority = priority_map[syslog_level];

		/*
		 * Specify the facility to interoperate with other syslog
		 * callers (vfs_full_audit for example).
		 */
		priority |= SYSLOG_FACILITY;

		va_start(ap, format_str);
		ret = vasprintf(&msgbuf, format_str, ap);
		va_end(ap);

		if (ret != -1) {
			syslog(priority, "%s", msgbuf);
		}
		SAFE_FREE(msgbuf);
	}
#endif

	check_log_size();

#ifdef WITH_SYSLOG
	if( !state.settings.syslog_only)
#endif
	{
		va_start( ap, format_str );
		if (state.fd > 0)
			(void)vdprintf( state.fd, format_str, ap );
		va_end( ap );
	}

 done:
	errno = old_errno;

	return( 0 );
}


/**************************************************************************
 Print the buffer content via Debug1(), then reset the buffer.
 Input:  none
 Output: none
****************************************************************************/

static void bufr_print( void )
{
	format_bufr[format_pos] = '\0';
	(void)Debug1( "%s", format_bufr );
	format_pos = 0;
}

/***************************************************************************
 Format the debug message text.

 Input:  msg - Text to be added to the "current" debug message text.

 Output: none.

 Notes:  The purpose of this is two-fold.  First, each call to syslog()
         (used by Debug1(), see above) generates a new line of syslog
         output.  This is fixed by storing the partial lines until the
         newline character is encountered.  Second, printing the debug
         message lines when a newline is encountered allows us to add
         spaces, thus indenting the body of the message and making it
         more readable.
**************************************************************************/

static void format_debug_text( const char *msg )
{
	size_t i;
	bool timestamp = (state.logtype == DEBUG_FILE && (state.settings.timestamp_logs));

	if (!format_bufr) {
		debug_init();
	}

	for( i = 0; msg[i]; i++ ) {
		/* Indent two spaces at each new line. */
		if(timestamp && 0 == format_pos) {
			format_bufr[0] = format_bufr[1] = ' ';
			format_pos = 2;
		}

		/* If there's room, copy the character to the format buffer. */
		if( format_pos < FORMAT_BUFR_MAX )
			format_bufr[format_pos++] = msg[i];

		/* If a newline is encountered, print & restart. */
		if( '\n' == msg[i] )
			bufr_print();

		/* If the buffer is full dump it out, reset it, and put out a line
		 * continuation indicator.
		 */
		if( format_pos >= FORMAT_BUFR_MAX ) {
			bufr_print();
			(void)Debug1( " +>\n" );
		}
	}

	/* Just to be safe... */
	format_bufr[format_pos] = '\0';
}

/***************************************************************************
 Flush debug output, including the format buffer content.

 Input:  none
 Output: none
***************************************************************************/

void dbgflush( void )
{
	bufr_print();
}

/***************************************************************************
 Print a Debug Header.

 Input:  level - Debug level of the message (not the system-wide debug
                  level. )
	  cls   - Debuglevel class of the calling module.
          file  - Pointer to a string containing the name of the file
                  from which this function was called, or an empty string
                  if the __FILE__ macro is not implemented.
          func  - Pointer to a string containing the name of the function
                  from which this function was called, or an empty string
                  if the __FUNCTION__ macro is not implemented.
         line  - line number of the call to dbghdr, assuming __LINE__
                 works.

  Output: Always true.  This makes it easy to fudge a call to dbghdr()
          in a macro, since the function can be called as part of a test.
          Eg: ( (level <= DEBUGLEVEL) && (dbghdr(level,"",line)) )

  Notes:  This function takes care of setting syslog_level.

****************************************************************************/

bool dbghdrclass(int level, int cls, const char *location, const char *func)
{
	/* Ensure we don't lose any real errno value. */
	int old_errno = errno;

	if( format_pos ) {
		/* This is a fudge.  If there is stuff sitting in the format_bufr, then
		 * the *right* thing to do is to call
		 *   format_debug_text( "\n" );
		 * to write the remainder, and then proceed with the new header.
		 * Unfortunately, there are several places in the code at which
		 * the DEBUG() macro is used to build partial lines.  That in mind,
		 * we'll work under the assumption that an incomplete line indicates
		 * that a new header is *not* desired.
		 */
		return( true );
	}

#ifdef WITH_SYSLOG
	/* Set syslog_level. */
	syslog_level = level;
#endif

	/* Don't print a header if we're logging to stdout. */
	if ( state.logtype != DEBUG_FILE ) {
		return( true );
	}

	/* Print the header if timestamps are turned on.  If parameters are
	 * not yet loaded, then default to timestamps on.
	 */
	if( state.settings.timestamp_logs || state.settings.debug_prefix_timestamp) {
		char header_str[200];

		header_str[0] = '\0';

		if( state.settings.debug_pid)
			slprintf(header_str,sizeof(header_str)-1,", pid=%u",(unsigned int)getpid());

		if( state.settings.debug_uid) {
			size_t hs_len = strlen(header_str);
			slprintf(header_str + hs_len,
			sizeof(header_str) - 1 - hs_len,
				", effective(%u, %u), real(%u, %u)",
				(unsigned int)geteuid(), (unsigned int)getegid(),
				(unsigned int)getuid(), (unsigned int)getgid());
		}

		if (state.settings.debug_class && (cls != DBGC_ALL)) {
			size_t hs_len = strlen(header_str);
			slprintf(header_str + hs_len,
				 sizeof(header_str) -1 - hs_len,
				 ", class=%s",
				 classname_table[cls]);
		}

		/* Print it all out at once to prevent split syslog output. */
		if( state.settings.debug_prefix_timestamp ) {
			char *time_str = current_timestring(NULL,
							    state.settings.debug_hires_timestamp);
			(void)Debug1( "[%s, %2d%s] ",
				      time_str,
				      level, header_str);
			talloc_free(time_str);
		} else {
			char *time_str = current_timestring(NULL,
							    state.settings.debug_hires_timestamp);
			(void)Debug1( "[%s, %2d%s] %s(%s)\n",
				      time_str,
				      level, header_str, location, func );
			talloc_free(time_str);
		}
	}

	errno = old_errno;
	return( true );
}

/***************************************************************************
 Add text to the body of the "current" debug message via the format buffer.

  Input:  format_str  - Format string, as used in printf(), et. al.
          ...         - Variable argument list.

  ..or..  va_alist    - Old style variable parameter list starting point.

  Output: Always true.  See dbghdr() for more info, though this is not
          likely to be used in the same way.

***************************************************************************/

 bool dbgtext( const char *format_str, ... )
{
	va_list ap;
	char *msgbuf = NULL;
	bool ret = true;
	int res;

	va_start(ap, format_str);
	res = vasprintf(&msgbuf, format_str, ap);
	va_end(ap);

	if (res != -1) {
		format_debug_text(msgbuf);
	} else {
		ret = false;
	}
	SAFE_FREE(msgbuf);
	return ret;
}


/* the registered mutex handlers */
static struct {
	const char *name;
	struct debug_ops ops;
} debug_handlers;

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

	if (!reopen_logs_internal()) return;

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
