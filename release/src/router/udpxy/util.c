/* @(#) implementation of utility functions for udpxy
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

#include <sys/stat.h>
#include <sys/utsname.h>

#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#include "util.h"
#include "uopt.h"
#include "mtrace.h"
#include "osdef.h"

extern const char  COMPILE_MODE[];
extern const char  VERSION[];
extern const int   BUILDNUM;
extern const char  BUILD_TYPE[];
extern const int   PATCH;

static char s_sysinfo [80] = "\0";

extern struct udpxy_opt g_uopt;

/* write buffer to a file
 *
 */
ssize_t
save_buffer( const void* buf, size_t len, const char* filename )
{
    int fd, rc;
    ssize_t nw, left;
    const char* p;

    assert( buf && len && filename );

    rc = 0;
    fd = creat( filename,
            (mode_t)(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) );
    if( -1 == fd ) {
        perror("creat");
        return 1;
    }

    for( p = (const char*)buf, nw = 0, left = len; left > 0; ) {
        nw = write( fd, p, left );
        if( nw <= 0 ) {
            nw = 0;
            if( EINTR != errno) {
                perror("write");
                rc = -1;
                break;
            }
        }

        left -= nw;
        p += nw;
    }

    (void)close(fd);
    return (rc ? -1 : ((ssize_t)len - left));
}


/* read text file into a buffer
 *
 */
ssize_t
txtf_read (const char* fpath, char* dst, size_t maxlen, FILE* log)
{
    int rc = 0, fd = -1;
    ssize_t n = 0;
    ssize_t left = maxlen - 1;
    char *p = dst;

    assert (fpath && dst && maxlen);
    fd = open (fpath, O_RDONLY, 0);
    if (-1 == fd) {
        mperror (log, errno, "%s open %s", __func__, fpath);
        return -1;
    }

    while (left > 0) {
        n = read (fd, p, maxlen - 1);
        if (!n) break;
        if (n < 0) {
            n = 0; rc = errno;
            if (EINTR != rc) {
                mperror (log, errno, "%s read %s", __func__, fpath);
                break;
            }
            rc = 0;
        }
        left -= (size_t)n;
        p += n;
    }
    if (!rc) *p = '\0';

    if (-1 == close (fd)) {
        mperror (log, errno, "%s close %s", __func__, fpath);
    }
    return (rc ? -1 : ((ssize_t)maxlen - left - 1));
}


/* make current process run as a daemon
 */
int
daemonize(int options, FILE* log)
{
    pid_t pid;
    int rc = 0, fh = -1;

    assert( log );

    if( (pid = fork()) < 0 ) {
        mperror( log, errno,
                "%s: fork", __func__);
        return -1;
    }
    else if( 0 != pid ) {
        exit(0);
    }

    do {
        if( -1 == (rc = setsid()) ) {
            mperror( log, errno,
                    "%s: setsid", __func__);
            break;
        }

        if( -1 == (rc = chdir("/")) ) {
            mperror( log, errno, "%s: chdir", __func__ );
            break;
        }

        (void) umask(0);

        if( !(options & DZ_STDIO_OPEN) ) {
            for( fh = 0; fh < 3; ++fh )
                if( -1 == (rc = close(fh)) ) {
                    mperror( log, errno, "%s: close", __func__);
                    break;
                }
        }

        if( SIG_ERR == signal(SIGHUP, SIG_IGN) ) {
            mperror( log, errno, "%s: signal", __func__ );
            rc = 2;
            break;
        }

    } while(0);

    if( 0 != rc ) return rc;

    /* child exits to avoid session leader's re-acquiring
     * control terminal */
    if( (pid = fork()) < 0 ) {
        mperror( log, errno, "%s: fork", __func__);
        return -1;
    }
    else if( 0 != pid )
        exit(0);

    return 0;
}


/* multiplex error output to custom log
 * and syslog
 */
void
mperror( FILE* fp, int err, const char* format, ... )
{
    char buf[ 256 ] = { '\0' };
    va_list ap;
    int n = 0;

    assert(format);

    va_start( ap, format );
    n = vsnprintf( buf, sizeof(buf) - 1, format, ap );
    va_end( ap );

    if( n <= 0 || n >= ((int)sizeof(buf) - 1) ) return;

    snprintf( buf + n, sizeof(buf) - n - 1, ": %s",
              strerror(err) );

    syslog( LOG_ERR | LOG_LOCAL0, "%s", buf );
    if( fp ) (void) tmfprintf( fp, "%s\n", buf );

    return;
}


/* write-lock on a file handle
 */
static int
wlock_file( int fd )
{
    struct flock  lck;

    lck.l_type      = F_WRLCK;
    lck.l_start     = 0;
    lck.l_whence    = SEEK_SET;
    lck.l_len       = 0;

    return fcntl( fd, F_SETLK, &lck );
}

/* create and lock file with process's ID
 */
int
make_pidfile( const char* fpath, pid_t pid, FILE* log )
{
    int fd = -1, rc = 0, n = -1;
    ssize_t nwr = -1;

    #define LLONG_MAX_DIGITS 21
    char buf[ LLONG_MAX_DIGITS + 1 ];

    mode_t fmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    assert( (NULL != fpath) && pid );

    errno = 0;
    do {
        fd = open( fpath, O_CREAT | O_WRONLY | O_NOCTTY, fmode );
        if( -1 == fd ) {
            mperror(log, errno, "make_pidfile - open");
            rc = EXIT_FAILURE;
            break;
        }

        rc = wlock_file( fd );
        if( 0 != rc ) {
            if( (EACCES == errno) || (EAGAIN == errno) ) {
                (void) fprintf( stderr, "File [%s] is locked "
                    "(another instance of daemon must be running)\n",
                    fpath );
                exit(EXIT_FAILURE);
            }

            mperror(log, errno, "wlock_file");
            break;
        }

        rc = ftruncate( fd, 0 );
        if( 0 != rc ) {
            mperror(log, errno, "make_pidfile - ftruncate");
            break;
        }

        n = snprintf( buf, sizeof(buf) - 1, "%d", pid );
        if( n < 0 ) {
            mperror(log, errno, "make_pidfile - snprintf");
            rc = EXIT_FAILURE;
            break;
        }

        nwr = write( fd, buf, n );
        if( (ssize_t)n != nwr ) {
            mperror( log, errno, "make_pidfile - write");
            rc = EXIT_FAILURE;
            break;
        }

    } while(0);

    if( (0 != rc) && (fd > 0) ) {
        (void)close( fd );
    }

    return rc;
}


/* write path to application's pidfile into the the buffer
 * (fail if destination directory is not writable)
 */
int
set_pidfile( const char* appname, int port, char* buf, size_t len )
{
    int n = -1;

    assert( appname && buf && len );

    if( -1 == access(PIDFILE_DIR, W_OK ) )
        return -1;

    n = snprintf( buf, len, "%s/%s%d.pid", PIDFILE_DIR, appname, port );
    if( n < 0 ) return EXIT_FAILURE;

    buf[ len - 1 ] = '\0';

    return 0;
}


int
would_block(int err)
{
    return (EAGAIN == err) || (EWOULDBLOCK == err);
}


int
no_fault(int err)
{
    return (EPIPE == err) || (ECONNRESET == err) || would_block(err);
}


/* write buffer to designated socket/file
 */
ssize_t
write_buf( int fd, const char* data, const ssize_t len, FILE* log )
{
    ssize_t n = 0, nwr = 0, error = IO_ERR;
    int err = 0;

    for( n = 0; errno = 0, n < len ; ) {
        nwr = write( fd, &(data[n]), len - n );
        if( nwr <= 0 ) {
            err = errno;
            if( EINTR == err ) {
                TRACE( (void)tmfprintf( log,
                            "%s interrupted\n", __func__ ) );
                continue;
            }
            else {
                if( would_block(err) )
                    error = IO_BLK;

                break;
            }
        }

        n += nwr;

        if( nwr != len ) {
            if( NULL != log ) {
                TRACE( (void)tmfprintf( log,
                        "Fragment written %s[%ld:%ld]/[%ld] bytes\n",
                        (len > n ? "P" : "F"), (long)nwr, (long)n, (long)len ) );
            }
        }

    }

    if( nwr <= 0 ) {
        if( log ) {
            if (IO_BLK == error)
                (void)tmfprintf( log, "%s: socket time-out on write", __func__);
            else if( !no_fault(err) || g_uopt.is_verbose )
                mperror( log, errno, "%s: write", __func__ );
        }

        return error;
    }

    return n;
}


/* read data chunk of designated size (or less) into buffer
 * (will *NOT* attempt to re-read if read less than expected
 *  w/o interruption)
 */
ssize_t
read_buf( int fd, char* data, const ssize_t len, FILE* log )
{
    ssize_t n = 0, nrd = 0, err = 0;

    for( n = 0; errno = 0, n < len ; ) {
        nrd = read( fd, &(data[n]), len - n );
        if( nrd <= 0 ) {
            err = errno;
            if( EINTR == err ) {
                TRACE( (void)tmfprintf( log,
                        "%s interrupted\n", __func__ ) );
                errno = 0;
                continue;
            }
            else {
                break;
            }
        }

        n += nrd;
        /*
        if( nrd != len ) {
            if( NULL != log ) {
                TRACE( (void)tmfprintf( log,
                "Fragment read [%ld]/[%ld] bytes\n",
                        (long)nrd, (long)len ) );
            }
        }
        */

        /* we only read as much as we can read at once (uninterrupted) */
        break;
    }

    if( nrd < 0 ) {
        if( log ) {
            if( would_block(err) )
                (void)tmfprintf( log, "%s: socket time-out on read", __func__);
            else if( !no_fault(err) || g_uopt.is_verbose )
                mperror( log, errno, "%s: read", __func__ );
        }
    }

    return n;
}


/* output hex dump of a memory fragment
 */
void
hex_dump( const char* msg, const char* data, size_t len, FILE* log )
{
    u_int i = 0;

    assert( data && (len > (size_t)0) && log );

    if( msg ) (void)fprintf( log, "%s: ", msg );

    for( i = 0; i < len; ++i )
        (void)fprintf( log, "%02x ", data[i] & 0xFF );

    (void) fputs("\n", log);
    return;
}


/* check for expected size, complain if not matched
 */
int
sizecheck( const char* msg, ssize_t expct, ssize_t len,
              FILE* log, const char* func )
{
    assert( msg && log && func );
    if( expct == len ) return 0;

    (void) (msg && log && func); /* NOP to eliminate warnings */

    TRACE( (void)tmfprintf( log, "%s: %s - read only [%ld] "
                "bytes out of [%ld]\n",
                func, msg, (long)len, (long)expct ) );

    return 1;
}


/* check for a potential buffer overrun by
 * evaluating target buffer and the portion
 * of that buffer to be accessed
 */
int
buf_overrun( const char* buf, size_t buflen,
             size_t offset, size_t dlen,
             FILE* log )
{
    const char* tgt = buf + offset + dlen;
    if( tgt > (buf + buflen) ) {
        if( NULL != log ) {
            TRACE( (void)tmfprintf( log, "+++ BUFFER OVERRUN at: "
                    "buf=[%p], size=[%lu] - intending to access [%p]: "
                    "offset=[%lu], data_length=[%lu]\n",
                    buf, (u_long)buflen, tgt, (u_long)offset,
                    (u_long)dlen) );
        }
        return 1;
    }

    return 0;
}


/* create timestamp string in YYYY-mm-dd HH24:MI:SS.MSEC from struct timeval
 */
int
mk_tvstamp( const struct timeval* tv, char* buf, size_t* len,
             int32_t flags )
{
    const char tmfmt_TZ[] = "%Y-%m-%d %H:%M:%S.%%06ld %Z";

    char tfmt_ms[ 80 ] = { '\0' };
    int n = 0;
    struct tm src_tm, *p_tm;
    time_t clock;

    assert( tv && buf && len );


    clock = tv->tv_sec;
    p_tm = (flags & TVSTAMP_GMT)
            ? gmtime_r( &clock, &src_tm )
            : localtime_r( &clock, &src_tm );
    if( NULL == p_tm ) {
        perror("gmtime_r/localtime_r");
        return errno;
    }

    n = strftime( tfmt_ms, sizeof(tfmt_ms) - 1, tmfmt_TZ, &src_tm );
    if( 0 == n ) {
        perror( "strftime" );
        return errno;
    }

    n = snprintf( buf, *len, tfmt_ms, (long)tv->tv_usec );
    if( 0 == n ) {
        perror( "snprintf" );
        return errno;
    }

    *len = (size_t)n;
    return 0;
}



/* write timestamp-prepended formatted message to file
 */
int
tmfprintf( FILE* stream, const char* format, ... )
{
    va_list ap;
    int n = -1, total = 0, rc = 0, NO_RESET = 0;
    char tstamp[ 80 ] = {'\0'};
    size_t ts_len = sizeof(tstamp) - 1;
    struct timeval tv_now;
    const char* pidstr = get_pidstr( NO_RESET, NULL );

    (void)gettimeofday( &tv_now, NULL );

    errno = 0;
    do {
        rc = mk_tvstamp( &tv_now, tstamp, &ts_len, 0 );
        if( 0 != rc ) break;

        n = fprintf( stream, "%s\t%s\t", tstamp, pidstr );
        if( n <= 0 ) break;
        total += n;

        va_start( ap, format );
        n = vfprintf( stream, format, ap );
        va_end( ap );

        if( n <= 0 ) break;
        total += n;

    } while(0);

    if( n <= 0 ) {
        perror( "fprintf/vfprintf" );
        return -1;
    }

    return (0 != rc) ? -1 : total;
}


/* write timestamp-prepended message to file
 */
int
tmfputs( const char* s, FILE* stream )
{
    int n = -1, rc = 0, NO_RESET = 0;
    char tstamp[ 80 ] = {'\0'};
    size_t ts_len = sizeof(tstamp) - 1;
    struct timeval tv_now;
    const char* pidstr = get_pidstr( NO_RESET, NULL );

    (void)gettimeofday( &tv_now, NULL );

    errno = 0;
    do {
        rc = mk_tvstamp( &tv_now, tstamp, &ts_len, 0 );
        if( 0 != rc ) break;

        if( (n = fputs( tstamp, stream )) < 0  ||
            (n = fputs( "\t", stream ))   < 0  ||
            (n = fputs( pidstr, stream )) < 0  ||
            (n = fputs( "\t", stream ))   < 0 )
            break;

        if( (n = fputs( s, stream )) < 0 )
            break;
    } while(0);

    if( n < 0 && errno ) {
        perror( "fputs" );
    }

    return (0 != rc) ? -1 : n;
}


/* print out command-line
 */
void
printcmdln( FILE* stream, const char* msg,
        int argc, char* const argv[] )
{
    int i = 0;

    assert( stream );

    if( msg )
        (void)tmfprintf( stream, "%s: ", msg );
    else
        (void)tmfputs( "", stream );

    for( i = 0; i < argc; ++i )
        (void)fprintf( stream, "%s ", argv[i] );
    (void)fputc( '\n', stream );
}


/* convert timespec to time_t
 * where
 *      timespec format: [+|-]dd:hh24:mi.ss
 *
 * @return 0 if success, n>0 if errno > 0,
 *         n < 0 otherwise (see ERR_ codes)
 */
int
a2time( const char* str, time_t* t, time_t from )
{
    int field[ 4 ] = {0};
    const size_t field_LEN = sizeof(field)/sizeof(field[0]);

    int is_offset = 0;
    int i_sec, i_min, i_hour, i_day;
    int n = 0, fi = 0, i = 0, new_field = 0, has_seconds = 0;
    struct tm stm;
    time_t tgt_time = (time_t)-1, offset_tm = (time_t)0;
    time_t now = from;

    static const int ERR_HSEC = -2;
    static const int ERR_FIELDLEN = -3;
    static const int ERR_NONDIGIT  = -4;

    assert( str );

    /* check if timespec is an offset (to current time) */
    n = 0;
    if( ('-' == str[n]) || ('+' == str[n]) ) {
        is_offset = ('-' == str[n]) ? -1 : 1;
        ++n;
    }

    /* read every field into an array element
     */
    for( fi = 0; (size_t)fi < field_LEN; ++n ) {
        if( '\0' == str[n] ) break;

        /* seconds has to be the last field in timespec
         * if seconds field has already been processed
         * it an error */
        if( (':' == str[n]) && has_seconds ) return ERR_HSEC;

        /* delimiter - move on to the next timespec field */
        if( ':' == str[n] || ('.' == str[n]) ) {
            new_field = 1;
            if( '.' == str[n] ) has_seconds = 1;
            continue;
        }

        if( !isdigit( str[n] ) ) return ERR_NONDIGIT;

        if( new_field ) {
            new_field = 0;
            ++fi;
        }

        field[ fi ] *= 10;
        field[ fi ] += (str[n] - '0');
    }
    if( (fi <= 0) || (size_t)fi >= field_LEN ) return ERR_FIELDLEN;

    /* last field is seconds, the one before is hours, etc. */
    i = fi;
    i_sec = has_seconds ? i-- : -1;
    i_min   = i--;
    i_hour  = i--;
    i_day   = i--;

    if( NULL == localtime_r( &now, &stm ) ) {
        return errno;
    }

    /*
    (void) fprintf( stderr, "sec[%d], min[%d], hour[%d], day[%d]\n",
                stm.tm_sec, stm.tm_min, stm.tm_hour, stm.tm_mday );
    (void) fprintf( stderr, "i_sec[%d], i_min[%d], i_hour[%d], i_day[%d]\n",
                    i_sec, i_min, i_hour, i_day );
    */

    if( !is_offset ) {
        /* hours and days default to current value */
        if( i_hour >= 0 )
            stm.tm_hour = field[ i_hour ];

        if( i_day >= 0  )
            stm.tm_mday = field[ i_day ];

        stm.tm_sec = (i_sec < 0) ? 0 : field[ i_sec ];
        stm.tm_min = (i_min < 0) ? 0 : field[ i_min ];

        /*
        (void) fprintf( stderr, "sec[%d], min[%d], hour[%d], day[%d]\n",
                stm.tm_sec, stm.tm_min, stm.tm_hour, stm.tm_mday );
        */

        tgt_time = mktime( &stm );
    }
    else {
        /*
        (void) fprintf( stderr, "sec[%d], min[%d], hour[%d], day[%d]\n",
                (i_sec < 0 ? 0 : field[i_sec]), (i_min < 0 ? 0 : field[i_min]),
                (i_hour < 0 ? 0 : field[i_hour]),(i_day < 0 ? 0 : field[i_day]) );
        */

        offset_tm =  ( (i_sec < 0 ? 0 : field[ i_sec ]) +
                       60 * (i_min < 0 ? 0 : field[ i_min ]) +
                       3600 * (i_hour < 0 ? 0 : field[ i_hour ]) +
                       (3600*24) * (i_day < 0 ? 0 : field[ i_day ])
                      );
        tgt_time = now + is_offset * offset_tm;
    }

    if( NULL != t )
        *t = tgt_time;

    return ((time_t)-1 == tgt_time) ? -1 : 0;
}




/* convert ASCII size spec (positive) into numeric value
 * size spec format:
 *      num[modifier], where num is an ASCII representation of
 *      any positive integer and
 *      modifier ::= [Kb|K|Mb|M|Gb|G]
 */
static int
a2double( const char* str, double* pval )
{
    int64_t mult = 1;
    const char* p = NULL;
    double dval = 0.0;
    size_t numsz = 0;

    static const int ERR_OVERRUN    = -3;
    static const int ERR_NUMFMT     = -4;
    static const int ERR_BADMULT    = -5;

#define MAX_SZLEN 64
    char buf[ MAX_SZLEN ] = {0};

    assert( str );

    /* skip to the first */
    for( p = str; *p && !isalpha(*p); ++p );

    if( '\0' != *p ) {
        /* there is a modifier, calculate multiplication
         * factor */
        if( 0 == strncasecmp( p, "Kb", MAX_SZLEN ) ||
            0 == strncasecmp( p, "K", MAX_SZLEN ))
            mult = 1024;
        else if( 0 == strncasecmp( p, "Mb", MAX_SZLEN ) ||
                 0 == strncasecmp( p, "M", MAX_SZLEN ) )
            mult = (1024 * 1024);
        else if( 0 == strncasecmp( p, "Gb", MAX_SZLEN ) ||
                 0 == strncasecmp( p, "G", MAX_SZLEN ) )
            mult = (1024 * 1024 * 1024);
        else
            return ERR_BADMULT;


        numsz = p - str;
        if( numsz >= (size_t)MAX_SZLEN ) return ERR_OVERRUN;
        (void) memcpy( buf, str, numsz );

        /*(void)fprintf( stderr, "buf=[%s]\n", buf);*/

        errno = 0;
        dval = strtod( buf, (char**)NULL );
        if( errno ) return ERR_NUMFMT;

        /* apply the modifier-induced multiplication factor */
        dval *= mult;
    }
    else {
        errno = 0;
        dval = strtod( str, (char**)NULL );
        if( errno ) return ERR_NUMFMT;
    }


    /*fprintf( stderr, "dval = [%f]\n", dval );*/

    if( NULL != pval )
        *pval = dval;

    return 0;
}


int
a2size( const char* str, ssize_t* pval )
{
    double dval = 0.0;
    int rc = 0;
    static const int ERR_OVFLW = -2;

    if( 0 != (rc = a2double( str, &dval )) )
        return rc;

    if( dval > LONG_MAX || dval < LONG_MIN )
        return ERR_OVFLW;

    if( NULL != pval ) {
        *pval = (ssize_t)dval;
    }

    return rc;
}

int
a2int64( const char* str, int64_t* pval )
{
/* NB: use LLONG_MAX and LLONG_MIN recommended when
 * compiling with C99 compliance;
 *
 * we still (yet) complie under C89, so LLONGMAX64,
 * LLONGMIN64 are introduced instead to avoid C99-related
 * warnings
 */

# define LLONGMAX64    9223372036854775807.0
# define LLONGMIN64    (-LLONGMAX64 - 1.0)

    double dval = 0.0;
    int rc = 0;
    static const int ERR_OVFLW = -2;

    if( 0 != (rc = a2double( str, &dval )) )
        return rc;

    if( dval > LLONGMAX64 || dval < LLONGMIN64 )
        return ERR_OVFLW;

    if( NULL != pval ) {
        *pval = (int64_t)dval;
    }

    return rc;
}

/* returns asctime w/o CR character at the end
 */
const char*
Zasctime( const struct tm* tm )
{
    size_t n = 0;

#define ASCBUF_SZ 64
    static char buf[ ASCBUF_SZ ], *p = NULL;

    buf[0] = '\0';
    p = asctime_r( tm, buf );
    if( NULL == p ) return p;

    buf[ ASCBUF_SZ - 1 ] = '\0';
    n = strlen( buf );
    if( (n > (size_t)1) && ('\n' == buf[ n - 1 ]) ) {
        buf[ n - 1 ] = '\0';
    }

    return buf;
}


/* adjust nice value if needed
 */
int
set_nice( int val, FILE* log )
{
    int newval = 0;
    assert( log );

    if( 0 != val ) {
        errno = 0;
        newval = nice( val );
        if( (-1 == newval) && (0 != errno) ) {
            mperror( log, errno, "%s: nice", __func__ );
            return -1;
        }
        else {
            TRACE( (void)tmfprintf( log, "Nice value incremented by %d\n",
                                    val) );
        }
    }

    return 0;
}


/* check and report a deviation in values: n_was and n_is are not supposed
 * to differ more than by delta
 */
void
check_fragments( const char* action, ssize_t total, ssize_t n_was, ssize_t n_is,
                 ssize_t delta, FILE* log )
{
    if( NULL == action ) return;

    if( (n_is < (n_was - delta)) || (n_is > (n_was + delta)) ) {
        (void)tmfprintf( log,
                "%s [%ld] bytes out of [%ld], last=[%ld]\n",
                action, (long)n_is, (long)total, (long)n_was);
    }
}


/* retrieve UNIX time value from given environment
 * variable, otherwise return default */
time_t
get_timeval( const char* envar, const time_t deflt  )
{
    char *str = NULL, *eptr = NULL;
    time_t tval = -1;

    assert( envar );
    if( NULL == (str = getenv( envar )) )
        return deflt;

    errno = 0;
    tval = (time_t) strtol( str, &eptr, 10 );
    if( errno ) return deflt;

    if ( eptr && (0 == *eptr) ) return tval;

    return deflt;
}


/* retrieve flag value as 1 = true, 0 = false
 * from an environment variable set in the form
 * of 0|1|'true'|'false'|'yes'|'no'
 */
int
get_flagval( const char* envar, const int deflt )
{
    long lval = (long)deflt;
    char *str = NULL, *eptr = NULL;
    size_t i = 0;

    static char* ON_sym[] = { "true", "yes", "on" };
    static char* OFF_sym[] = { "false", "no", "off" };

    assert( envar );
    str = getenv( envar );
    if ( NULL == str )
        return deflt;

    errno = 0;
    lval = strtol( str, &eptr, 10 );
    if( errno ) return deflt;

    if ( eptr && (0 == *eptr) ) return lval;

    for( i = 0; i < sizeof(ON_sym)/sizeof(ON_sym[0]); ++i )
        if( 0 == strcasecmp(str, ON_sym[i]) ) return 1;

    for( i = 0; i < sizeof(OFF_sym)/sizeof(OFF_sym[0]); ++i )
        if( 0 == strcasecmp(str, OFF_sym[i]) ) return 0;

    return deflt;
}


/* retrieve SSIZE value from given environment
 * variable, otherwise return default */
ssize_t
get_sizeval( const char* envar, const ssize_t deflt )
{
    char *str = NULL, *eptr = NULL;
    long lval = -1;

    assert( envar );
    if( NULL == (str = getenv( envar )) )
        return deflt;

    errno = 0;
    lval = strtol( str, &eptr, 10 );
    if( errno ) return deflt;

    if ( eptr && (0 == *eptr) ) return lval;

    return deflt;
}



/* retrieve/reset string representation of pid
 */
const char*
get_pidstr( int reset, const char* pfx )
{
    static char pidstr[ 24 ];
    static pid_t pid = 0;

    if( 1 == reset || (0 == pid) ) {
        pid = getpid();
        if (pfx) {
            (void) snprintf (pidstr, sizeof(pidstr)-1, "%s(%d)", pfx, pid);
        } else {
            (void) snprintf (pidstr, sizeof(pidstr)-1, "%d", pid );
        }
        pidstr [sizeof(pidstr)-1] = '\0';
    }

    return pidstr;
}


/* retrieve system info string
 */
const char*
get_sysinfo (int* perr)
{
    struct utsname uts;
    int rc = 0;

    if (s_sysinfo[0]) return s_sysinfo;

    (void) memset (&uts, 0, sizeof(uts));
    errno = 0; rc = uname (&uts);
    if (perr) *perr = errno;

    if (0 == rc) {
        s_sysinfo [sizeof(s_sysinfo)-1] = '\0';
        (void) snprintf (s_sysinfo, sizeof(s_sysinfo)-1, "%s %s %s",
            uts.sysname, uts.release, uts.machine);
    }
    return s_sysinfo;
}


void
mk_app_info(const char *appname, char *info, size_t infolen)
{
    assert(info);
    if ('\0' == *info) {
        (void) snprintf(info, infolen,
                "%s %s-%d.%d (%s) %s [%s]", appname, VERSION,
                BUILDNUM, PATCH, BUILD_TYPE,
            COMPILE_MODE, get_sysinfo(NULL) );
    }
}


/* __EOF__ */

