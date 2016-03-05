/* @(#) interface to utility functions for udpxy
 *
 * Copyright 2008-2011 Pavel V. Cherenkov (pcherenkov@gmail.com)
 *
 *  This file is part of udpxy.
 *
 *  udpxy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  udpxy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with udpxy.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UTILH_UDPXY_200712181853
#define UTILH_UDPXY_200712181853

#include <sys/types.h>
#include <stdio.h>

struct timeval;

#ifdef __cplusplus
extern "C" {
#endif

/* write buffer to a file
 */
ssize_t
save_buffer( const void* buf, size_t len, const char* filename );

/* read text file into a buffer
 *
 */
ssize_t
txtf_read (const char* fpath, char* dst, size_t maxlen, FILE* log);

/* start process as a daemon
 *
 */
#define DZ_STDIO_OPEN  1   /* do not close STDIN, STDOUT, STDERR */
int
daemonize(int options, FILE* log);


/* multiplex error output to custom log
 * and syslog
 */
void
mperror( FILE* fp, int err, const char* format, ... );


/* create and lock file with process's ID
 */
int
make_pidfile( const char* fpath, pid_t pid, FILE* log );


/* write path to application's pidfile into the the buffer
 * (fail of destination directory is not writable)
 */
int
set_pidfile( const char* appname, int port, char* buf, size_t len );

/* write buffer to designated socket/file
 * return number of bytes read/written or one of the error
 * codes below
 */
#define IO_ERR      -1      /* generic error */
#define IO_BLK      -2      /* operation timed out or would block */
ssize_t
write_buf( int fd, const char* data, const ssize_t len, FILE* log );

/* read data chunk of designated size into buffer
 */
ssize_t
read_buf( int fd, char* buf, const ssize_t len, FILE* log );

/* output hex dump of a memory fragment
 */
void
hex_dump( const char* msg, const char* data, size_t len, FILE* log );

/* check for expected size, complain if not matched
 */
int
sizecheck( const char* msg, ssize_t expct, ssize_t len,
              FILE* log, const char* func );

/* check for a potential buffer overrun by
 * evaluating target buffer and the portion
 * of that buffer to be accessed
 */
int
buf_overrun( const char* buf, size_t buflen,
             size_t offset, size_t dlen,
             FILE* log );


/* write timestamp-prepended formatted message to file
 */
int
tmfprintf( FILE* stream, const char* format, ... );


/* write timestamp-prepended message to file
 */
int
tmfputs( const char* s, FILE* stream );


/* print out command-line
 */
void
printcmdln( FILE* stream, const char* msg,
        int argc, char* const argv[] );


/* convert timespec to time_t
 * where
 *      timespec format: [+|-]dd:hh24:mi:ss
 *
 * @return 0 if success, n>0 if parse error at position n,
 *         -1 otherwise
 */
int
a2time( const char* str, time_t* t, time_t from );


/* convert ASCII size spec (positive) into numeric value
 * size spec format:
 *      num[modifier], where num is an ASCII representation of
 *      any positive integer and
 *      modifier ::= [Kb|K|Mb|M|Gb|G]
 */
int
a2size( const char* str, ssize_t* pval );

int
a2int64( const char* str, int64_t* pval );

/* returns asctime w/o CR character at the end
 */
struct tm;

const char*
Zasctime( const struct tm* tm );


/* adjust nice value if needed
 */
int
set_nice( int val, FILE* log );


/* check and report a deviation in values: n_was and n_is are not supposed
 * to differ more than by delta
 */
void
check_fragments( const char* action, ssize_t total, ssize_t n_was, ssize_t n_is,
                 ssize_t delta, FILE* log );

/* create timestamp string in YYYY-mm-dd HH24:MI:SS.MSEC from struct timeval
 */
static const int32_t TVSTAMP_GMT = 1;

int
mk_tvstamp( const struct timeval* tv, char* buf, size_t* len,
             int32_t flags );


/* retrieve UNIX time value from given environment
 * variable, otherwise return default */
time_t
get_timeval( const char* envar, const time_t deflt  );


/* retrieve flag value as 1 = true, 0 = false
 * from an environment variable set in the form
 * of 0|1|'true'|'false'|'yes'|'no'
 */
int
get_flagval( const char* envar, const int deflt );

/* retrieve LONG value from given environment
 * variable, otherwise return default */
ssize_t
get_sizeval( const char* envar, const ssize_t deflt );


/* retrieve/reset string representation of pid,
 */
const char*
get_pidstr( int reset, const char* pfx );

/* retrieve system info string
 */
const char*
get_sysinfo (int* perr);

/* return 1 if err is one of the errors signifying possibility of a block, 0 otherwise.
 */
int
would_block(int err);

/* return 1 if this kind of error should not be captures in syslog, 0 otherwise.
 */
int
no_fault(int err);

/* populate info string with application's credentials (version, patch, etc.)
 */
void
mk_app_info(const char *appname, char *info, size_t infolen);

#ifdef __cplusplus
}
#endif

#endif /* UTILH_UDPXY_200712181853 */

/* __EOF__ */

