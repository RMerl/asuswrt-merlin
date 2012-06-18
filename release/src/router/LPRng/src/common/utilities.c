/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: utilities.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";

#include "lp.h"

#include "utilities.h"
#include "getopt.h"
#include "errorcodes.h"

/**** ENDINCLUDE ****/

/*
 * Time_str: return "cleaned up" ctime() string...
 *
 * in YY/MO/DY/hr:mn:sc
 * Thu Aug 4 12:34:17 BST 1994 -> 12:34:17
 */

char *Time_str(int shortform, time_t t)
{
    static char buffer[99];
	struct tm *tmptr;
	struct timeval tv;

	tv.tv_usec = 0;
	if( t == 0 ){
		if( gettimeofday( &tv, 0 ) == -1 ){
			Errorcode = JFAIL;
			LOGERR_DIE(LOG_ERR)"Time_str: gettimeofday failed");
		}
		t = tv.tv_sec;
	}
	tmptr = localtime( &t );
	if( shortform && Full_time_DYN == 0 ){
		SNPRINTF( buffer, sizeof(buffer))
			"%02d:%02d:%02d.%03d",
			tmptr->tm_hour, tmptr->tm_min, tmptr->tm_sec,
			(int)(tv.tv_usec/1000) );
	} else {
		SNPRINTF( buffer, sizeof(buffer))
			"%d-%02d-%02d-%02d:%02d:%02d.%03d",
			tmptr->tm_year+1900, tmptr->tm_mon+1, tmptr->tm_mday,
			tmptr->tm_hour, tmptr->tm_min, tmptr->tm_sec,
			(int)(tv.tv_usec/1000) );
	}
	/* now format the time */
	if( Ms_time_resolution_DYN == 0 ){
		char *s;
		if( ( s = safestrrchr( buffer, '.' )) ){
			*s = 0;
		}
	}
	return( buffer );
}


/*
 * Pretty_time: return "cleaned up" ctime() string...
 *
 * in YY/MO/DY/hr:mn:sc
 * Thu Aug 4 12:34:17 BST 1994 -> 12:34:17
 */

char *Pretty_time( time_t t )
{
    static char buffer[99];
	struct tm *tmptr;
	struct timeval tv;

	tv.tv_usec = 0;
	if( t == 0 ){
		if( gettimeofday( &tv, 0 ) == -1 ){
			Errorcode = JFAIL;
			LOGERR_DIE(LOG_ERR)"Time_str: gettimeofday failed");
		}
		t = tv.tv_sec;
	}
	tmptr = localtime( &t );
	strftime( buffer, sizeof(buffer), "%b %d %R %Y", tmptr );

	return( buffer );
}

time_t Convert_to_time_t( char *str )
{
	time_t t = 0;
	if(str) t = strtol(str,0,0);
	DEBUG5("Convert_to_time_t: %s = %ld", str, (long)t );
	return(t);
}

/***************************************************************************
 * Print the usage message list or any list of strings
 *  Use for copyright printing as well
 ***************************************************************************/

void Printlist( char **m, int fd )
{
	char msg[SMALLBUFFER];
	if( m ){
		if( *m ){
			SNPRINTF( msg,sizeof(msg)) _(*m), Name );
			Write_fd_str(fd, msg);
			Write_fd_str(fd,"\n");
			++m;
		}
		for( ; *m; ++m ){
			SNPRINTF( msg,sizeof(msg)) "%s\n", _(*m) );
			Write_fd_str(fd, msg);
		}
	}
}


/***************************************************************************
 * Utility functions: write a string to a fd (bombproof)
 *   write a char array to a fd (fairly bombproof)
 * Note that there is a race condition here that is unavoidable;
 * The problem is that there is no portable way to disable signals;
 *  post an alarm;  <enable signals and do a read simultaneously>
 * There is a solution that involves forking a subprocess, but this
 *  is so painful as to be not worth it.  Most of the timeouts
 *  in the LPR stuff are in the order of minutes, so this is not a problem.
 *
 * Note: we do the write first and then check for timeout.
 ***************************************************************************/

/*
 * Write_fd_len( fd, msg, len )
 * returns:
 *  0  - success
 *  <0 - failure
 */

int Write_fd_len( int fd, const char *msg, int len )
{
	int i;
	int busy_index = 0;//JY1110: add sock

	i = len;
	while( len > 0 && (i = write( fd, msg, len ) ) >= 0 ){
		len -= i, msg += i;
		if(i == 0)
			busy_index++;//JY1110: add sock
		else
			busy_index=0;
			
		if(busy_index > 3){//JY1110: add sock
			check_prn_status("BUSY or ERROR", clientaddr);
		}
		else{
			check_prn_status("Printing", clientaddr);
		}
	}
	return( (i < 0) ? -1 : 0 );
}

/*
 * Write_fd_len_timeout( timeout, fd, msg, len )
 * returns:
 *  0  - success
 *  <0 - failure
 */

int Write_fd_len_timeout( int timeout, int fd, const char *msg, int len )
{
	int i;
	if( timeout > 0 ){
		if( Set_timeout() ){
			Set_timeout_alarm( timeout  );
			i = Write_fd_len( fd, msg, len );
		} else {
			i = -1;
		}
		Clear_timeout();
	} else {
		i = Write_fd_len( fd, msg, len );
	}
	return( i < 0 ? -1 : 0 );
}

/*
 * Write_fd_str_timeout( fd, msg )
 * returns:
 *  0  - success
 *  <0 - failure
 */

int Write_fd_str( int fd, const char *msg )
{
	if( msg && *msg ){
		return( Write_fd_len( fd, msg, safestrlen(msg) ));
	}
	return( 0 );
}


/*
 * Write_fd_str_timeout( timeout, fd, msg )
 * returns:
 *  0  - success
 *  <0 - failure
 */

int Write_fd_str_timeout( int timeout, int fd, const char *msg )
{
	if( msg && *msg ){
		return( Write_fd_len_timeout( timeout, fd, msg, safestrlen(msg) ) );
	}
	return( 0 );
}


/*
 * Read_fd_len_timeout( timeout, fd, msg, len )
 * returns:
 *  n>0 - read n
 *  0  - EOF
 *  <0 - failure
 */

int Read_fd_len_timeout( int timeout, int fd, char *msg, int len )
{
	int i;
	if( timeout > 0 ){
		if( Set_timeout() ){
			Set_timeout_alarm( timeout  );
			i = read( fd, msg, len );
		} else {
			i = -1;
		}
		Clear_timeout();
	} else {
		i = read( fd, msg, len );
	}
	return( i );
}


/**************************************************************
 * 
 * signal handling:
 * SIGALRM should be the only signal that terminates system calls;
 * all other signals should NOT terminate them.
 * This signal() emulation function attepts to do just that.
 * (Derived from Advanced Programming in the UNIX Environment, Stevens, 1992)
 *
 **************************************************************/


/* solaris 2.3 note: don't compile this with "gcc -ansi -pedantic";
 * due to a bug in the header file, struct sigaction doesn't
 * get declared. :(
 */

/* plp_signal will set flags so that signal handlers will continue
 * Note that in Solaris,  you MUST reinstall the
 * signal hanlders in the signal handler!  The default action is
 * to try to restart the system call - note that the code should
 * be written so that you check for error returns from a system call
 * and continue if no error.
 * WARNING: read/write may terminate early? who knows...
 * See plp_signal_break.
 */

plp_sigfunc_t plp_signal (int signo, plp_sigfunc_t func)
{
#ifdef HAVE_SIGACTION
	struct sigaction act, oact;

	act.sa_handler = func;
	(void) sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
# ifdef SA_RESTART
	act.sa_flags |= SA_RESTART;             /* SVR4, 4.3+BSD */
# endif
	if (sigaction (signo, &act, &oact) < 0) {
		return (SIG_ERR);
	}
	return (plp_sigfunc_t) oact.sa_handler;
#else
	/* sigaction is not supported. Just set the signals. */
	return (plp_sigfunc_t)signal (signo, func); 
#endif
}

/* plp_signal_break is similar to plp_signal,  but will cause
 * TERMINATION of a system call if possible.  This allows
 * you to force a signal to cause termination of a system
 * wait or other action.
 * WARNING: read/write may terminate early? who knows... so
 * beware if you are expecting this and don't believe that
 * you got an entire buffer read/written.
 */

plp_sigfunc_t plp_signal_break (int signo, plp_sigfunc_t func)
{
#ifdef HAVE_SIGACTION
	struct sigaction act, oact;

	act.sa_handler = func;
	(void) sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
# ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;            /* SunOS */
# endif
	if (sigaction (signo, &act, &oact) < 0) {
		return (SIG_ERR);
	}
	return (plp_sigfunc_t) oact.sa_handler;
#else
	/* sigaction is not supported. Just set the signals. */
	return (plp_sigfunc_t)signal (signo, func); 
#endif
}

/**************************************************************/

void plp_block_all_signals ( plp_block_mask *oblock )
{
#ifdef HAVE_SIGPROCMASK
	sigset_t block;

	(void) sigfillset (&block); /* block all signals */
	if (sigprocmask (SIG_SETMASK, &block, oblock) < 0)
		LOGERR_DIE(LOG_ERR) "plp_block_all_signals: sigprocmask failed");
#else
	*oblock = sigblock( ~0 ); /* block all signals */
#endif
}


void plp_unblock_all_signals ( plp_block_mask *oblock )
{
#ifdef HAVE_SIGPROCMASK
	sigset_t block;

	(void) sigemptyset (&block); /* block all signals */
	if (sigprocmask (SIG_SETMASK, &block, oblock) < 0)
		LOGERR_DIE(LOG_ERR) "plp_unblock_all_signals: sigprocmask failed");
#else
	*oblock = sigblock( 0 ); /* unblock all signals */
#endif
}

void plp_set_signal_mask ( plp_block_mask *in, plp_block_mask *out )
{
#ifdef HAVE_SIGPROCMASK
	if (sigprocmask (SIG_SETMASK, in, out ) < 0)
		LOGERR_DIE(LOG_ERR) "plp_set_signal_mask: sigprocmask failed");
#else
	sigset_t block;
	if( in ) block = sigsetmask( *in );
	else     block = sigblock( 0 );
	if( out ) *out = block;
#endif
}

void plp_unblock_one_signal ( int sig, plp_block_mask *oblock )
{
#ifdef HAVE_SIGPROCMASK
	sigset_t block;

	(void) sigemptyset (&block); /* clear out signals */
	(void) sigaddset (&block, sig ); /* clear out signals */
	if (sigprocmask (SIG_UNBLOCK, &block, oblock ) < 0)
		LOGERR_DIE(LOG_ERR) "plp_unblock_one_signal: sigprocmask failed");
#else
	*oblock = sigblock( 0 );
	(void) sigsetmask (*oblock & ~ sigmask(sig) );
#endif
}

void plp_block_one_signal( int sig, plp_block_mask *oblock )
{
#ifdef HAVE_SIGPROCMASK
	sigset_t block;

	(void) sigemptyset (&block); /* clear out signals */
	(void) sigaddset (&block, sig ); /* clear out signals */
	if (sigprocmask (SIG_BLOCK, &block, oblock ) < 0)
		LOGERR_DIE(LOG_ERR) "plp_block_one_signal: sigprocmask failed");
#else
	*oblock = sigblock( sigmask( sig ) );
#endif
}

void plp_sigpause( void )
{
#ifdef HAVE_SIGPROCMASK
	sigset_t block;
	(void) sigemptyset (&block); /* clear out signals */
	(void) sigsuspend( &block );
#else
	(void)sigpause( 0 );
#endif
}

/**************************************************************
 * Bombproof versions of strcasecmp() and strncasecmp();
 **************************************************************/

/* case insensitive compare for OS without it */
int safestrcasecmp (const char *s1, const char *s2)
{
	int c1, c2, d = 0;
	if( (s1 == s2) ) return(0);
	if( (s1 == 0 ) && s2 ) return( -1 );
	if( s1 && (s2 == 0 ) ) return( 1 );
	for (;;) {
		c1 = *((unsigned char *)s1); s1++;
		c2 = *((unsigned char *)s2); s2++;
		if( isupper(c1) ) c1 = tolower(c1);
		if( isupper(c2) ) c2 = tolower(c2);
		if( (d = (c1 - c2 )) || c1 == 0 ) break;
	}
	return( d );
}

/* case insensitive compare for OS without it */
int safestrncasecmp (const char *s1, const char *s2, int len )
{
	int c1, c2, d = 0;
	if( (s1 == s2) && s1 == 0 ) return(0);
	if( (s1 == 0 ) && s2 ) return( -1 );
	if( s1 && (s2 == 0 ) ) return( 1 );
	for (;len>0;--len){
		c1 = *((unsigned char *)s1); s1++;
		c2 = *((unsigned char *)s2); s2++;
		if( isupper(c1) ) c1 = tolower(c1);
		if( isupper(c2) ) c2 = tolower(c2);
		if( (d = (c1 - c2 )) || c1 == 0 ) return(d);
	}
	return( 0 );
}

/* perform safe comparison, even with null pointers */
int safestrcmp( const char *s1, const char *s2 )
{
	if( (s1 == s2) ) return(0);
	if( (s1 == 0 ) && s2 ) return( -1 );
	if( s1 && (s2 == 0 ) ) return( 1 );
	return( strcmp(s1, s2) );
}


/* perform safe comparison, even with null pointers */
int safestrlen( const char *s1 )
{
	if( s1 ) return(strlen(s1));
	return(0);
}


/* perform safe comparison, even with null pointers */
int safestrncmp( const char *s1, const char *s2, int len )
{
	if( (s1 == s2) && s1 == 0 ) return(0);
	if( (s1 == 0 ) && s2 ) return( -1 );
	if( s1 && (s2 == 0 ) ) return( 1 );
	return( strncmp(s1, s2, len) );
}


/* perform safe strchr, even with null pointers */
char *safestrchr( const char *s1, int c )
{
	if( s1 ) return( strchr( s1, c ) );
	return( 0 );
}


/* perform safe strrchr, even with null pointers */
char *safestrrchr( const char *s1, int c )
{
	if( s1 ) return( strrchr( s1, c ) );
	return( 0 );
}


/* perform safe strchr, even with null pointers */
char *safestrpbrk( const char *s1, const char *s2 )
{
	if( s1 && s2 ) return( strpbrk( s1, s2 ) );
	return( 0 );
}

/* perform string concatentaton with malloc */
char *safestrappend4( char *s1, const char *s2, const char *s3, const char *s4 )
{
	int m, len;
	m = safestrlen(s1);
	len = m + safestrlen(s2) + safestrlen(s3) + safestrlen(s4);
	s1 = realloc(s1,len+1);
	len = m;
	if( s2 ) strcpy( s1+len, s2 ); len += strlen(s1+len);
	if( s3 ) strcpy( s1+len, s3 ); len += strlen(s1+len);
	if( s4 ) strcpy( s1+len, s4 ); len += strlen(s1+len);
	s1[ len ] = 0;
	return(s1);
}

/***************************************************************************
 * plp_usleep() with select - simple minded way to avoid problems
 ***************************************************************************/
int plp_usleep( int i )
{
	struct timeval t;
	DEBUG3("plp_usleep: starting usleep %d", i );
	if( i > 0 ){
		memset( &t, 0, sizeof(t) );
		t.tv_usec = i%1000000;
		t.tv_sec =  i/1000000;
		i = select( 0,
			FD_SET_FIX((fd_set *))(0),
			FD_SET_FIX((fd_set *))(0),
			FD_SET_FIX((fd_set *))(0),
			&t );
		DEBUG3("plp_usleep: select done, status %d", i );
	}
	return( i );
}


/***************************************************************************
 * plp_sleep() with select - simple minded way to avoid problems
 ***************************************************************************/
int plp_sleep( int i )
{
	struct timeval t;
	DEBUG3("plp_sleep: starting sleep %d", i );
	if( i > 0 ){
		memset( &t, 0, sizeof(t) );
		t.tv_sec = i;
		i = select( 0,
			FD_SET_FIX((fd_set *))(0),
			FD_SET_FIX((fd_set *))(0),
			FD_SET_FIX((fd_set *))(0),
			&t );
		DEBUG3("plp_sleep: select done, status %d", i );
	}
	return( i );
}


/***************************************************************************
 * int get_max_processes()
 *  get the maximum number of processes allowed
 ***************************************************************************/

int Get_max_servers( void )
{
	int n = 0;	/* We need some sort of limit here */

#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_NPROC)
	struct rlimit pcount;
	if( getrlimit(RLIMIT_NPROC, &pcount) == -1 ){
		FATAL(LOG_ERR) "Get_max_servers: getrlimit failed" );
	}
	n = pcount.rlim_cur;
#ifdef RLIMIT_INFINITY
	if( pcount.rlim_cur == RLIM_INFINITY ){
		n = Max_servers_active_DYN;
		DEBUG1("Get_max_servers: using %d", n );
	}
#endif

	DEBUG1("Get_max_servers: getrlimit returns %d", n );
#else
# if defined(HAVE_SYSCONF) && defined(_SC_CHILD_MAX)
	if( n == 0 && (n = sysconf(_SC_CHILD_MAX)) < 0 ){
		FATAL(LOG_ERR) "Get_max_servers: sysconf failed" );
	}
	DEBUG1("Get_max_servers: sysconf returns %d", n );
# else
#  if defined(CHILD_MAX)
		n = CHILD_MAX;
		DEBUG1("Get_max_servers: CHILD_MAX %d", n );
#  else
		n = 0;
		DEBUG1("Get_max_servers: default %d", n );
#  endif
# endif
#endif
	n = n/4;
	if(( n > 0 && n > Max_servers_active_DYN)
		|| (n <= 0 && Max_servers_active_DYN) ){
		n = Max_servers_active_DYN;
	}
	if( n <= 0 ) n = 32;

	DEBUG1("Get_max_servers: returning %d", n );
	return( n );
}


/***************************************************************************
 * int Get_max_fd()
 *  get the maximum number of file descriptors allowed
 ***************************************************************************/

int Get_max_fd( void )
#ifdef ORIGINAL_DEBUG//JY@1020
{
#else
{}
#endif
#ifdef ORIGINAL_DEBUG//JY@1020
	int n = 0;	/* We need some sort of limit here */

#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_NOFILE)
	struct rlimit pcount;
	if( getrlimit(RLIMIT_NOFILE, &pcount) == -1 ){
		FATAL(LOG_ERR) "Get_max_fd: getrlimit failed" );
	}
	n = pcount.rlim_cur;
	DEBUG4("Get_max_fd: getrlimit returns %d", n );
#else
# if defined(HAVE_SYSCONF) && defined(_SC_OPEN_MAX)
	if( n == 0 && (n = sysconf(_SC_OPEN_MAX)) < 0 ){
		FATAL(LOG_ERR) "Get_max_servers: sysconf failed" );
	}
	DEBUG4("Get_max_fd: sysconf returns %d", n );
# else
	n = 20;
	DEBUG4("Get_max_fd: using default %d", n );
# endif
#endif

	if( n <= 0 || n > 10240 ){
		/* we have some systems that will return a VERY
		 * large or negative number for unlimited FD's.  Well, we
		 * don't want to use a large number here.  So we
		 * will make it a small number.  The actual number of
		 * file descriptors open by processes is VERY conservative.
		 */
		n = 256;
	}

	DEBUG1("Get_max_fd: returning %d", n );
	return( n );
}
#endif


char *Brk_check_size( void )
{
	static char b[128];
	static char* Top_of_mem;	/* top of allocated memory */
	char *s = sbrk(0);
	int   v = s - Top_of_mem;
	if( Top_of_mem == 0 ){
		SNPRINTF(b, sizeof(b)) "BRK: initial value 0x%lx", Cast_ptr_to_long(s) );
	} else {
		SNPRINTF(b, sizeof(b)) "BRK: new value 0x%lx, increment %d", Cast_ptr_to_long(s), v );
	}
	Top_of_mem = s;
	return(b);
}

char *mystrncat( char *s1, const char *s2, int len )
{
	int size;
	s1[len-1] = 0;
	size = safestrlen( s1 );
	if( s2 && len - size > 0  ){
		strncpy( s1+size, s2, len - size );
	}
	return( s1 );
}
char *mystrncpy( char *s1, const char *s2, int len )
{
	s1[0] = 0;
	if( s2 && len-1 > 0 ){
		strncpy( s1, s2, len-1 );
		s1[len-1] = 0;
	}
	return( s1 );
}

/*
 * Set_non_block_io(fd)
 * Set_block_io(fd)
 *  Set blocking or non-blocking IO
 *  Dies if unsuccessful
 * Get_nonblock_io(fd)
 *  Returns O_NONBLOCK flag value
 */

int Get_nonblock_io( int fd )
{
	int mask;
	/* we set IO to non-blocking on fd */

	if( (mask = fcntl( fd, F_GETFL, 0 ) ) == -1 ){
		return(-1);
	}
	mask &= O_NONBLOCK;
	return( mask );
}

int Set_nonblock_io( int fd )
{
	int mask;
	/* we set IO to non-blocking on fd */

	if( (mask = fcntl( fd, F_GETFL, 0 ) ) == -1 ){
		return(-1);
	}
	mask |= O_NONBLOCK;
	if( (mask = fcntl( fd, F_SETFL, mask ) ) == -1 ){
		return(-1);
	}
	return(0);
}

int Set_block_io( int fd )
{
	int mask;
	/* we set IO to blocking on fd */

	if( (mask = fcntl( fd, F_GETFL, 0 ) ) == -1 ){
		return(-1);
	}
	mask &= ~O_NONBLOCK;
	if( (mask = fcntl( fd, F_SETFL, mask ) ) == -1 ){
		return(-1);
	}
	return(0);
}

/*
 * Read_write_timeout
 *  int readfd, char *inbuffer, int maxinlen -
 *    read data from this fd into this buffer before this timeout
 *  int *readlen  - reports number of bytes read
 *  int writefd, char **outbuffer, int *outlen -
 *     **outbuffer and **outlen are updated after write
 *     write data from to this fd from this buffer before this timeout
 *  int timeout
 *       > 0  - wait total of this long
 *       0    - wait indefinitely
 *      -1    - do not wait
 *  Returns:
 *   **outbuffer, *outlen updated
 *    0    - success
 *   JRDERR     - IO error on output
 *   JTIMEOUT   - Timeout
 *   JWRERR     - IO error on input
 */

int Read_write_timeout(
	int readfd, char *inbuffer, int maxinlen, int *readlen,
	int writefd, char **outbuffer, int *outlen, int timeout )
#ifdef ORIGINAL_DEBUG//JY@1020
{
#else
{}
#endif
#ifdef ORIGINAL_DEBUG//JY@1020
	time_t start_t, current_t;
	int elapsed, m, err, done, retval;
	struct timeval timeval, *tp;
    fd_set readfds, writefds; /* for select() */
	struct stat statb;

	DEBUG4( "Read_write_timeout: read(fd %d, buffer 0x%lx, maxinlen %d, readlen 0x%lx->%d",
		readfd, Cast_ptr_to_long(inbuffer), maxinlen, Cast_ptr_to_long(readlen),
		readlen?*readlen:0 );
	DEBUG4( "Read_write_timeout: write(fd %d, buffer 0x%lx->0x%lx, len 0x%lx->%d, timeout %d)",
		writefd, Cast_ptr_to_long(outbuffer), Cast_ptr_to_long(outbuffer?*outbuffer:0),
		Cast_ptr_to_long(outlen), outlen?*outlen:0, timeout );

	retval = done = 0;
	time( &start_t );

	if( *outlen == 0 ) return( retval );
	if( readfd  > 0 ){
		if( fstat( readfd, &statb ) ){
			Errorcode = JABORT;
			FATAL(LOG_ERR) "Read_write_timeout: readfd %d closed", readfd );
		}
		Set_nonblock_io( readfd );
	} else {
		Errorcode = JABORT;
		FATAL(LOG_ERR) "Read_write_timeout: no readfd %d", readfd );
	}
	if( writefd  > 0 ){
		if( fstat( writefd, &statb ) ){
			Errorcode = JABORT;
			FATAL(LOG_ERR) "Read_write_timeout: writefd %d closed",
				writefd );
		}
		Set_nonblock_io( writefd );
	} else {
		Errorcode = JABORT;
		FATAL(LOG_ERR) "Read_write_timeout: no write %d", writefd );
	}

	while(!done){
		tp = 0;
		memset( &timeval, 0, sizeof(timeval) );
		m = 0;
		if( timeout > 0 ){
			time( &current_t );
			elapsed = current_t - start_t;
			if( timeout > 0 && elapsed >= timeout ){
				break;
			}
			timeval.tv_sec = m = timeout - elapsed;
			tp = &timeval;
			DEBUG4("Read_write_timeout: timeout now %d", m );
		} else if( timeout < 0 ){
			/* we simply poll once */
			tp = &timeval;
		}
		FD_ZERO( &writefds );
		FD_ZERO( &readfds );
		m = 0;
		FD_SET( writefd, &writefds );
		if( m <= writefd ) m = writefd+1;
		FD_SET( readfd, &readfds );
		if( m <= readfd ) m = readfd+1;
		errno = 0;
		DEBUG4("Read_write_timeout: starting select" );
        m = select( m,
            FD_SET_FIX((fd_set *))&readfds,
            FD_SET_FIX((fd_set *))&writefds,
            FD_SET_FIX((fd_set *))0, tp );
		err = errno;
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUG4("Read_write_timeout: select returned %d, errno '%s'",
			m, Errormsg(err) );
#endif
		if( m < 0 ){
			if( err != EINTR ){
#ifdef ORIGINAL_DEBUG//JY@1020
				LOGERR(LOG_INFO)"Read_write_timeout: select returned %d, errno '%s'",
				m, Errormsg(err) );
#endif
				retval = JTIMEOUT;
				done = 1;
			}
		} else if( m == 0 ){
			/* timeout */
			retval = JTIMEOUT;
			done = 1;
		} else {
			if( FD_ISSET( readfd, &readfds ) ){
				DEBUG4("Read_write_timeout: read possible on fd %d", readfd );
				m = read( readfd, inbuffer, maxinlen );
				DEBUG4("Read_write_timeout: read() returned %d", m );
				if( readlen ) *readlen = m;
				/* caller leaves space for this */
				if( m >= 0 ) inbuffer[m] = 0;
				if( m < 0 ) retval = JRDERR;
				done = 1;
			}
			if( FD_ISSET( writefd, &writefds ) ){
				DEBUG4("Read_write_timeout: write possible on fd %d", writefd );
				Set_nonblock_io( writefd );
				m = write( writefd, *outbuffer, *outlen );
				err = errno;
				Set_block_io( writefd );
				DEBUG4("Read_write_timeout: wrote %d", m );
				if( m < 0 ){
					/* we have EOF on the file descriptor */
					retval = JWRERR;
					done = 1;
				} else {
					*outlen -= m;
					*outbuffer += m;
					if( *outlen == 0 ){
						done = 1;
					}
				}
				errno = err;
			}
		}
	}
	err = errno;
	errno = err;
	return( retval );
}
#endif

/***************************************************************************
 * Set up alarms so LPRng doesn't hang forever during transfers.
 ***************************************************************************/

/*
 * timeout_alarm
 *  When we get the alarm,  we close the file descriptor (if any)
 *  we are working with.  When we next do an option, it will fail
 *  Note that this will cause any ongoing read/write operation to fail
 * We then to a longjmp to the routine, returning a non-zero value
 * We set an alarm using:
 *
 * if( (setjmp(Timeout_env)==0 && Set_timeout_alarm(t,s)) ){
 *   timeout dependent stuff
 * }
 * Clear_alarm
 * We define the Set_timeout macro as:
 *  #define Set_timeout(t,s) (setjmp(Timeout_env)==0 && Set_timeout_alarm(t,s))
 */

 static plp_signal_t timeout_alarm (int sig)
{
	Alarm_timed_out = 1;
	signal( SIGALRM, SIG_IGN );
	errno = EINTR;
#if 1//JY1110: timeout while writing
/*JY1111*/
	check_prn_status("BUSY or ERROR", clientaddr);
	send_ack_packet(currten_sock, ACK_FAIL);//JY1120
/**/
exit(0);
#endif
#if defined(HAVE_SIGLONGJMP)
	siglongjmp(Timeout_env,1);
#else
	longjmp(Timeout_env,1);
#endif
}


 static plp_signal_t timeout_break (int sig)
{
	Alarm_timed_out = 1;
	signal( SIGALRM, SIG_IGN );
}


/***************************************************************************
 * Set_timeout( int timeout, int *socket )
 *  Set up a timeout to occur; note that you can call this
 *   routine several times without problems,  but you must call the
 *   Clear_timeout routine sooner or later to reset the timeout function.
 *  A timeout value of 0 never times out
 * Clear_alarm()
 *  Turns off the timeout alarm
 ***************************************************************************/
void Set_timeout_signal_handler( int timeout, plp_sigfunc_t handler )
{
	int err = errno;
	sigset_t oblock;

	alarm(0);
	signal(SIGALRM, SIG_IGN);
	plp_unblock_one_signal( SIGALRM, &oblock );
	Alarm_timed_out = 0;
	Timeout_pending = 0;

	if( timeout > 0 ){
		Timeout_pending = timeout;
		plp_signal_break(SIGALRM, handler);
		alarm (timeout);
	}
	errno = err;
}


void Set_timeout_alarm( int timeout )
{
	Set_timeout_signal_handler( timeout, timeout_alarm );
}

void Set_timeout_break( int timeout )
{
	Set_timeout_signal_handler( timeout, timeout_break );
}

void Clear_timeout( void )
{
	int err = errno;

	signal( SIGALRM, SIG_IGN );
	alarm(0);
	Timeout_pending = 0;
	errno = err;
}

/*
 * setuid.c:
 * routines to manipulate user-ids securely (and hopefully, portably).
 * The * internals of this are very hairy, because
 * (a) there's lots of sanity checking
 * (b) there's at least three different setuid-swapping
 * semantics to support :(
 * 
 *
 * Note that the various functions saves errno then restores it afterwards;
 * this means it's safe to do "root_to_user();some_syscall();user_to_root();"
 * and errno will be from the system call.
 * 
 * "root" is the user who owns the setuid executable (privileged).
 * "user" is the user who runs it.
 * "daemon" owns data files used by the LPRng utilities (spool directories, etc).
 *   and is set by the 'user' entry in the configuration file.
 * 
 * To_ruid( user );	-- set ruid to user, euid to root
 * To_euid( user );	-- set euid to user, ruid to root
 * To_euid_root();	-- set euid to root, ruid to root
 * To_daemon();	-- set euid to daemon, ruid to root
 * To_user();	-- set euid to user, ruid to root
 * Full_daemon_perms() -- set both UID and EUID, one way, no return
 * 
 */

/***************************************************************************
 * Commentary:
 * Patrick Powell Sat Apr 15 07:56:30 PDT 1995
 * 
 * This has to be one of the ugliest parts of any portability suite.
 * The following models are available:
 * 1. process has <uid, euid>  (old SYSV, BSD)
 * 2. process has <uid, euid, saved uid, saved euid> (new SYSV, BSD)
 * 
 * There are several possibilites:
 * 1. need euid root   to do some operations
 * 2. need euid user   to do some operations
 * 3. need euid daemon to do some operations
 * 
 * Group permissions are almost useless for a server;
 * usually you are running as a specified group ID and do not
 * need to change.  Client programs are slightly different.
 * You need to worry about permissions when creating a file;
 * for this reason most client programs do a u mask(0277) before any
 * file creation to ensure that nobody can read the file, and create
 * it with only user access permissions.
 * 
 * > int setuid(uid) uid_t uid;
 * > int seteuid(euid) uid_t euid;
 * > int setruid(ruid) uid_t ruid;
 * > 
 * > DESCRIPTION
 * >      setuid() (setgid()) sets both the real and effective user ID
 * >      (group  ID) of the current process as specified by uid (gid)
 * >      (see NOTES).
 * > 
 * >      seteuid() (setegid()) sets the effective user ID (group  ID)
 * >      of the current process.
 * > 
 * >      setruid() (setrgid()) sets the real user ID  (group  ID)  of
 * >      the current process.
 * > 
 * >      These calls are only permitted to the super-user or  if  the
 * >      argument  is  the  real  or effective user (group) ID of the
 * >      calling process.
 * > 
 * > SYSTEM V DESCRIPTION
 * >      If the effective user ID  of  the  calling  process  is  not
 * >      super-user,  but if its real user (group) ID is equal to uid
 * >      (gid), or if the saved set-user (group) ID  from  execve(2V)
 * >      is equal to uid (gid), then the effective user (group) ID is
 * >      set to uid (gid).
 * >      .......  etc etc
 * 
 * Conclusions:
 * 1. if EUID == ROOT or RUID == ROOT then you can set EUID, UID to anything
 * 3. if EUID is root, you can set EUID 
 * 
 * General technique:
 * Initialization
 *   - use setuid() system call to force EUID/RUID = ROOT
 * 
 * Change
 *   - assumes that initialization has been carried out and
 * 	EUID == ROOT or RUID = ROOT
 *   - Use the seteuid() system call to set EUID
 * 
 ***************************************************************************/

#if !defined(HAVE_SETREUID) && !defined(HAVE_SETEUID) && !defined(HAVE_SETRESUID)
#error You need one of setreuid(), seteuid(), setresuid()
#endif

/***************************************************************************
 * Commentary
 * setuid(), setreuid(), and now setresuid()
 *  This is probably the easiest road.
 *  Note: we will use the most feature ridden one first, as it probably
 *  is necessary on some wierd system.
 *   Patrick Powell Fri Aug 11 22:46:39 PDT 1995
 ***************************************************************************/
#if !defined(HAVE_SETEUID) && !defined(HAVE_SETREUID) && defined(HAVE_SETRESUID)
# define setreuid(x,y) (setresuid( (x), (y), -1))
# define HAVE_SETREUID
#endif

/***************************************************************************
 * Setup_uid()
 * 1. gets the original EUID, RUID, EGID, RGID
 * 2. if UID 0 or EUID 0 forces both UID and EUID to 0 (test)
 * 3. Sets the EUID to the original RUID
 *    This leaves the UID (EUID) 
 ***************************************************************************/

void Setup_uid(void)
{
	int err = errno;
	static int SetRootUID;	/* did we set UID to root yet? */

	if( SetRootUID == 0 ){
		OriginalEUID = geteuid();	
		OriginalRUID = getuid();	
		OriginalEGID = getegid();	
		OriginalRGID = getgid();	
		DEBUG1("Setup_uid: OriginalEUID %d, OriginalRUID %d",
			OriginalEUID, OriginalRUID );
		DEBUG1("Setup_uid: OriginalEGID %d, OriginalRGID %d",
			OriginalEGID, OriginalRGID );
		/* we now make sure that we are able to use setuid() */
		/* notice that setuid() will work if EUID or RUID is 0 */
		if( OriginalEUID == ROOTUID || OriginalRUID == ROOTUID ){
			/* set RUID/EUID to ROOT - possible if EUID or UID is 0 */
			if(
#				ifdef HAVE_SETEUID
					setuid( (uid_t)ROOTUID ) || seteuid( OriginalRUID )
#				else
					setuid( (uid_t)ROOTUID ) || setreuid( ROOTUID, OriginalRUID )
#				endif
				){
				FATAL(LOG_ERR)
					"Setup_uid: RUID/EUID Start %d/%d seteuid failed",
					OriginalRUID, OriginalEUID);
			}
			if( getuid() != ROOTUID ){
				FATAL(LOG_ERR)
				"Setup_uid: IMPOSSIBLE! RUID/EUID Start %d/%d, now %d/%d",
					OriginalRUID, OriginalEUID, 
					getuid(), geteuid() );
			}
			UID_root = 1;
		}
		DEBUG1( "Setup_uid: Original RUID/EUID %d/%d, RUID/EUID %d/%d",
					OriginalRUID, OriginalEUID, 
					getuid(), geteuid() );
		SetRootUID = 1;
	}
	errno = err;
}

/***************************************************************************
 * seteuid_wrapper()
 * 1. you must have done the initialization
 * 2. check to see if you need to do anything
 * 3. check to make sure you can
 ***************************************************************************/
 static int seteuid_wrapper( uid_t to )
{
	int err = errno;
	uid_t euid;


	DEBUG4(
		"seteuid_wrapper: Before RUID/EUID %d/%d, DaemonUID %d, UID_root %d",
		OriginalRUID, OriginalEUID, DaemonUID, UID_root );
	if( UID_root ){
		/* be brutal: set both to root */
		if( setuid( ROOTUID ) ){
			LOGERR_DIE(LOG_ERR)
			"seteuid_wrapper: setuid() failed!!");
		}
#if defined(HAVE_SETEUID)
		if( seteuid( to ) ){
			LOGERR_DIE(LOG_ERR)
			"seteuid_wrapper: seteuid() failed!!");
		}
#else
		if( setreuid( ROOTUID, to) ){
			LOGERR_DIE(LOG_ERR)
			"seteuid_wrapper: setreuid() failed!!");
		}
#endif
	}
	euid = geteuid();
	DEBUG4( "seteuid_wrapper: After uid/euid %d/%d", getuid(), euid );
	errno = err;
	return( to != euid );
}


/***************************************************************************
 * setruid_wrapper()
 * 1. you must have done the initialization
 * 2. check to see if you need to do anything
 * 3. check to make sure you can
 ***************************************************************************/
 static int setruid_wrapper( uid_t to )
{
	int err = errno;
	uid_t ruid;


	DEBUG4(
		"setruid_wrapper: Before RUID/EUID %d/%d, DaemonUID %d, UID_root %d",
		OriginalRUID, OriginalEUID, DaemonUID, UID_root );
	if( UID_root ){
		/* be brutal: set both to root */
		if( setuid( ROOTUID ) ){
			LOGERR_DIE(LOG_ERR)
			"setruid_wrapper: setuid() failed!!");
		}
#if defined(HAVE_SETRUID)
		if( setruid( to ) ){
			LOGERR_DIE(LOG_ERR)
			"setruid_wrapper: setruid() failed!!");
		}
#elif defined(HAVE_SETREUID)
		if( setreuid( to, ROOTUID) ){
			LOGERR_DIE(LOG_ERR)
			"setruid_wrapper: setreuid() failed!!");
		}
#elif defined(__CYGWIN__)
		if( seteuid( to ) ){
			LOGERR_DIE(LOG_ERR)
			"setruid_wrapper: seteuid() failed!!");
		}
#else
# error - you do not have a way to set ruid
#endif
	}
	ruid = getuid();
	DEBUG4( "setruid_wrapper: After uid/euid %d/%d", getuid(), geteuid() );
	errno = err;
	return( to != ruid );
}


/*
 * Superhero functions - change the EUID to the requested one
 *  - these are really idiot level,  as all of the tough work is done
 * in Setup_uid() and seteuid_wrapper() 
 *  We also change the groups, just to be nasty as well, except for
 *  To_ruid and To_uid,  which only does the RUID and EUID
 *    Sigh...  To every rule there is an exception.
 */
int To_euid_root(void)
{
	return( seteuid_wrapper( ROOTUID )	);
}

 static int To_daemon_called;

int To_daemon(void)
{
	Set_full_group( DaemonUID, DaemonGID );
	To_daemon_called = 1;
	return( seteuid_wrapper( DaemonUID )	);
}

int To_user(void)
{
	if( To_daemon_called ){
		Errorcode = JABORT;
		LOGMSG(LOG_ERR) "To_user: LOGIC ERROR! To_daemon has been called");
		abort();
	}
	/* Set_full_group( OriginalRUID, OriginalRGID ); */
	return( seteuid_wrapper( OriginalRUID )	);
}
int To_ruid(int ruid)
{
	return( setruid_wrapper( ruid )	);
}
int To_euid( int euid )
{
	return( seteuid_wrapper( euid ) );
}

/*
 * set both uid and euid to the same value, using setuid().
 * This is unrecoverable!
 */

int setuid_wrapper(uid_t to)
{
	int err = errno;
	if( UID_root ){
		/* Note: you MUST use setuid() to force saved_setuid correctly */
		if( setuid( (uid_t)ROOTUID ) ){
			LOGERR_DIE(LOG_ERR) "setuid_wrapper: setuid(ROOTUID) failed!!");
		}
		if( setuid( (uid_t)to ) ){
			LOGERR_DIE(LOG_ERR) "setuid_wrapper: setuid(%d) failed!!", to);
		}
		if( to ) UID_root = 0;
	}
    DEBUG4("after setuid: (%d, %d)", getuid(),geteuid());
	errno = err;
	return( to != getuid() || to != geteuid() );
}

int Full_daemon_perms(void)
{
	Setup_uid();
	Set_full_group( DaemonUID, DaemonGID );
	return(setuid_wrapper(DaemonUID));
}

int Full_root_perms(void)
{
	Setup_uid();
	Set_full_group( ROOTUID, ROOTUID );
	return(setuid_wrapper( ROOTUID ));
}

int Full_user_perms(void)
{
	Setup_uid();
	Set_full_group( OriginalRUID, OriginalRGID );
	return(setuid_wrapper(OriginalRUID));
}


/***************************************************************************
 * Getdaemon()
 *  get daemon uid
 *
 ***************************************************************************/

int Getdaemon(void)
{
	char *str = 0;
	char *t;
	struct passwd *pw;
	int uid;

	str = Daemon_user_DYN;
	DEBUG4( "Getdaemon: using '%s'", str );
	if(!str) str = "daemon";
	t = str;
	uid = strtol( str, &t, 10 );
	if( str == t || *t ){
		/* try getpasswd */
		pw = getpwnam( str );
		if( pw ){
			uid = pw->pw_uid;
		}
	}
	DEBUG4( "Getdaemon: uid '%d'", uid );
	if( uid == ROOTUID ) uid = getuid();
	DEBUG4( "Getdaemon: final uid '%d'", uid );
	return( uid );
}

/***************************************************************************
 * Getdaemon_group()
 *  get daemon gid
 *
 ***************************************************************************/

int Getdaemon_group(void)
{
	char *str = 0;
	char *t;
	struct group *gr;
	gid_t gid;

	str = Daemon_group_DYN;
	DEBUG4( "Getdaemon_group: Daemon_group_DYN '%s'", str );
	if( !str ) str = "daemon";
	DEBUG4( "Getdaemon_group: name '%s'", str );
	t = str;
	gid = strtol( str, &t, 10 );
	if( str == t ){
		/* try getpasswd */
		gr = getgrnam( str );
		if( gr ){
			gid = gr->gr_gid;
		}
	}
	DEBUG4( "Getdaemon_group: gid '%d'", gid );
	if( gid == 0 ) gid = getgid();
	DEBUG4( "Getdaemon_group: final gid '%d'", gid );
	return( gid );
}

/***************************************************************************
 * set daemon uid and group
 * 1. get the current EUID
 * 2. set up the permissions changing
 * 3. set the RGID/EGID
 ***************************************************************************/

int Set_full_group( int euid, int gid )
{
	int status=0;
	int err;
	struct passwd *pw = 0;

	DEBUG4( "Set_full_group: euid '%d'", euid );

	/* get the user we want to set the groups for */
	pw = getpwuid(euid);
	if( UID_root ){
		setuid(ROOTUID);	/* set RUID/EUID to root */
#if defined(HAVE_INITGROUPS)
		if( pw ){
			/* froth froth... initgroups() uses the same buffer as
			 * getpwuid... SHRIEK  so we need to copy the user name
			 */
			char user[256];
			safestrncpy(user,pw->pw_name);
			if( safestrlen(user) != safestrlen(pw->pw_name) ){
				FATAL(LOG_ERR) "Set_full_group: CONFIGURATION BOTCH! safestrlen of user name '%s' = %d larger than buffer size %d",
					pw->pw_name, safestrlen(pw->pw_name), sizeof(user) );
			}
			if( initgroups(user, pw->pw_gid ) == -1 ){
				err = errno;
#ifdef ORIGINAL_DEBUG//JY@1020
				LOGERR_DIE(LOG_ERR) "Set_full_group: initgroups failed '%s'",
					Errormsg( err ) );
#endif
			}
		} else
#endif
#if defined(HAVE_SETGROUPS)
			if( setgroups(0,0) == -1 ){
				err = errno;
#ifdef ORIGINAL_DEBUG//JY@1020
				LOGERR_DIE(LOG_ERR) "Set_full_group: setgroups failed '%s'",
					Errormsg( err ) );
#endif
			}
#endif
		status = setgid( gid );
		if( status < 0 ){
			err = errno;
#ifdef ORIGINAL_DEBUG//JY@1020
			LOGERR_DIE(LOG_ERR) "Set_full_group: setgid '%d' failed '%s'",
				gid, Errormsg( err ) );
#endif
		}
	}
	return( 0 );
}

int Setdaemon_group(void)
{
	Set_full_group( DaemonUID, DaemonGID );
	return( 0 );
}


/*
 * Testing magic:
 * if we are running SUID
 *   We have set our RUID to root and EUID daemon
 * However,  we may want to run as another UID for testing.
 * The config file allows us to do this, but we set the SUID values
 * from the hardwired defaults before we read the configuration file.
 * After reading the configuration file,  we check the current
 * DaemonUID and the requested Daemon UID.  If the requested
 * Daemon UID == 0, then we run as the user which started LPD.
 */

void Reset_daemonuid(void)
{
	uid_t uid;
    uid = Getdaemon();  /* get the config file daemon id */
    DaemonGID = Getdaemon_group();  /* get the config file daemon id */
    if( uid != DaemonUID ){
        if( uid == ROOTUID ){
            DaemonUID = OriginalRUID;   /* special case for testing */
        } else {
            DaemonUID = uid;
        }
    }
    DEBUG4( "DaemonUID %d", DaemonUID );
}


#ifdef HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_STATFS_H
# include <sys/statfs.h>
#endif
#if defined(HAVE_SYS_VFS_H) && !defined(SOLARIS)
# include <sys/vfs.h>
#endif

#ifdef SUNOS
 extern int statfs(const char *, struct statfs *);
#endif

# if USE_STATFS_TYPE == STATVFS
#  define plp_statfs(path,buf) statvfs(path,buf)
#  define plp_struct_statfs struct statvfs
#  define statfs(path, buf) statvfs(path, buf)
#  define USING "STATVFS"
#  define BLOCKSIZE(f) (double)(f.f_frsize?f.f_frsize:f.f_bsize)
#  define BLOCKS(f)    (double)f.f_bavail
# endif

# if USE_STATFS_TYPE == ULTRIX_STATFS
#  define plp_statfs(path,buf) statfs(path,buf)
#  define plp_struct_statfs struct fs_data
#  define USING "ULTRIX_STATFS"
#  define BLOCKSIZE(f) (double)f.fd_bsize
#  define BLOCKS(f)    (double)f.fd_bfree
# endif

# if USE_STATFS_TYPE ==  SVR3_STATFS
#  define plp_struct_statfs struct statfs
#  define plp_statfs(path,buf) statfs(path,buf,sizeof(struct statfs),0)
#  define USING "SV3_STATFS"
#  define BLOCKSIZE(f) (double)f.f_bsize
#  define BLOCKS(f)    (double)f.f_bfree
# endif

# if USE_STATFS_TYPE == STATFS
#  define plp_struct_statfs struct statfs
#  define plp_statfs(path,buf) statfs(path,buf)
#  define USING "STATFS"
#  define BLOCKSIZE(f) (double)f.f_bsize
#  define BLOCKS(f)    (double)f.f_bavail
# endif

#ifdef JYDEBUG1//JYWeng
FILE *aaaaaa;
#endif

/***************************************************************************
 * Check_space() - check to see if there is enough space
 ***************************************************************************/

double Space_avail( char *pathname )
{
	double space = 0;
	plp_struct_statfs fsb;

	if( plp_statfs( pathname, &fsb ) == -1 ){
		DEBUG2( "Check_space: cannot stat '%s'", pathname );
	} else {
		space = BLOCKS(fsb) * (BLOCKSIZE(fsb)/1024.0);
	}
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "TYPE=%d\n", USE_STATFS_TYPE);
fprintf(aaaaaa, "Space_avail: fsb_bsize=%d\n", fsb.f_bsize);
fprintf(aaaaaa, "Space_avail: fsb_frsize=%d\n", fsb.f_frsize);
fprintf(aaaaaa, "Space_avail: fsb_block=%d\n", fsb.f_blocks);
fprintf(aaaaaa, "Space_avail: fsb_bfree=%d\n", fsb.f_bfree);
fprintf(aaaaaa, "Space_avail: fsb_bavail=%d\n", fsb.f_bavail);
fprintf(aaaaaa, "Space_avail: fsb_files=%d\n", fsb.f_files);
fprintf(aaaaaa, "Space_avail: fsb_ffree=%d\n", fsb.f_ffree);
fprintf(aaaaaa, "Space_avail: fsb_favail=%d\n", fsb.f_favail);
fprintf(aaaaaa, "Space_avail: fsb_f_flag=%d\n", fsb.f_flag);
fprintf(aaaaaa, "BLOCKS(fsb)=%f\n", BLOCKS(fsb));
fprintf(aaaaaa, "BLOCKSIZE(fsb)=%f\n", BLOCKSIZE(fsb));
fprintf(aaaaaa, "space=%f\n", space);
fclose(aaaaaa);
#endif
#ifdef JYDEBUG//JYWeng
space=3000.0;
#endif
	return(space);
}

/* VARARGS2 */
#ifdef HAVE_STDARGS
 void safefprintf (int fd, char *format,...)
#else
 void safefprintf (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
	int fd;
    char *format;
#endif
	char buf[1024];
    VA_LOCAL_DECL

    VA_START (format);
    VA_SHIFT (fd, int);
    VA_SHIFT (format, char *);

	(void) VSNPRINTF (buf, sizeof(buf)) format, ap);
	Write_fd_str( fd, buf );
}
