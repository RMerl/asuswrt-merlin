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
"$Id: lpf.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


/***************************************************************************
 *  Filter template and frontend.
 *
 *	A filter is invoked with the following parameters,
 *  which can be in any order, and perhaps some missing.
 *
 *  filtername arguments \   <- from PRINTCAP entry
 *      -PPrinter -wwidth -llength -xwidth -ylength [-c] \
 *      -Kcontrolfilename -Lbnrname \
 *      [-iindent] \
 *		[-Zoptions] [-Cclass] [-Jjob] [-Raccntname] -nlogin -hHost  \
 *      -Fformat [-Tcrlf] [-Tdebug] [affile]
 * 
 *  1. Parameters can be in different order than the above.
 *  2. Optional parameters can be missing
 *  3. Values specified for the width, length, etc., are from PRINTCAP
 *     or from the overridding user specified options.
 *
 *  This program provides a common front end for most of the necessary
 *  grunt work.  This falls into the following classes:
 * 1. Parameter extraction.
 * 2. Suspension when used as the "of" filter.
 * 3. Termination and accounting
 *  The front end will extract parameters,  then call the filter_pgm()
 *  routine,  which is responsible for carrying out the required filter
 *  actions. filter_pgm() is invoked with the printer device on fd 1,
 *	and error log on fd 2.  The npages variable is used to record the
 *  number of pages that were used.
 *  The "halt string", which is a sequence of characters that
 *  should cause the filter to suspend itself, is passed to filter.
 *  When these characters are detected,  the "suspend_ofilter()" routine should be
 *  called.
 *
 *  On successful termination,  the accounting file will be updated.
 *
 *  The filter_pgm() routine should return 0 (success), 1 (retry) or 2 (abort).
 *
 * Parameter Extraction
 *	The main() routine will extract parameters
 *  whose values are placed in the appropriate variables.  This is done
 *  by using the ParmTable[], which has entries for each valid letter
 *  parmeter, such as the letter flag, the type of variable,
 *  and the address of the variable.
 *  The following variables are provided as a default set.
 *      -PPrinter -wwidth -llength -xwidth -ylength [-c] [-iindent] \
 *		[-Zoptions] [-Cclass] [-Jjob] [-Raccntname] -nlogin -hHost  \
 *      -Fformat [affile]
 * VARIABLE  FLAG          TYPE    PURPOSE / PRINTCAP ENTRTY
 * name     name of filter char*    argv[0], program identification
 * width    -wwidth	       int      PW, width in chars
 * length   -llength	   int      PL, length in lines
 * xwidth   -xwidth        int      PX, width in pixels
 * xlength  -xlength       int      PY, length in pixels
 * literal  -c	           int      if set, ignore control chars
 * controlfile -kcontrolfile char*  control file name
 * bnrname  -Lbnrname      char*    banner name
 * indent   -iindent       int      indent amount (depends on device)
 * zopts    -Zoptions      char*    extra options for printer
 * comment  -Scomment      char*    printer name in comment field
 * class    -Cclass        char*    classname
 * job      -Jjob          char*    jobname
 * accntname -Raccntname   char*    account for billing purposes
 * login    -nlogin        char*    login name
 * host     -hhost         char*    host name
 * format   -Fformat       char*    format
 * statusfile  -sstatusfile   char*    statusfile
 * accntfile file          char*    AF, accounting file
 *
 * npages    - number of pages for accounting
 *
 * -Tdebug - increment debug level
 * -Tcrlf  - turn LF to CRLF translation off
 *
 *	The functions logerr(), and logerr_die() can be used to report
 *	status. The variable errorcode can be set by the user before calling
 *	these functions, and will be the exit value of the program. Its default
 *	value will be 2 (abort status).
 *	logerr() reports a message, appends information indicated by errno
 *	(see perror(2) for details), and then returns.
 *	logerr_die() will call logerr(), and then will exit with errorcode
 *	status.
 *
 * DEBUGGING:  a simple minded debugging version can be enabled by
 * compiling with the -DDEBUG option.
 */

#include "portable.h"

/* VARARGS3 */
#ifdef HAVE_STDARGS
int	plp_snprintf (char *str, size_t count, const char *fmt, ...);
int	plp_vsnprintf (char *str, size_t count, const char *fmt, va_list arg);
#else
int plp_snprintf ();
int plp_vsnprintf ();
#endif

/* VARARGS2 */
#ifdef HAVE_STDARGS
 void safefprintf (int fd, char *format,...);
#else
 void safefprintf ();
#endif

#include <sys/types.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
#endif

#ifndef HAVE_ERRNO_DECL
extern int errno;
#endif

#ifdef HAVE_SYS_FCNTL_H
# include <sys/fcntl.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

/* varargs declarations: */

#if defined(HAVE_STDARG_H)
# include <stdarg.h>
# define HAVE_STDARGS    /* let's hope that works everywhere (mj) */
# define VA_LOCAL_DECL   va_list ap;
# define VA_START(f)     va_start(ap, f)
# define VA_SHIFT(v,t)	;	/* no-op for ANSI */
# define VA_END          va_end(ap)
#else
# if defined(HAVE_VARARGS_H)
#  include <varargs.h>
#  undef HAVE_STDARGS
#  define VA_LOCAL_DECL   va_list ap;
#  define VA_START(f)     va_start(ap)		/* f is ignored! */
#  define VA_SHIFT(v,t)	v = va_arg(ap,t)
#  define VA_END		va_end(ap)
# else
XX ** NO VARARGS ** XX
# endif
#endif

char *Time_str(int shortform, time_t t);

/*
 * default exit status, causes abort
 */
int errorcode;
char *name;		/* name of filter */
/* set from flags */
int debug, width, length, xwidth, ylength, literal, indent;
char *zopts, *class, *job, *login, *accntname, *host, *accntfile, *format;
char *printer, *controlfile, *bnrname, *comment;
char *queuename, *errorfile;
int npages;	/* number of pages */
char *statusfile;
char filter_stop[] = "\031\001";	/* sent to cause filter to suspend */
int  accounting_fd;
int crlf;	/* change lf to CRLF */

void getargs( int argc, char *argv[], char *envp[] );
void logerr( char *msg, ... );
void logerr_die( char *msg, ... );
extern void banner( void );
extern void doaccnt( void );
extern void filter_pgm( char * );
int of_filter;

int main( int argc, char *argv[], char *envp[] )
{

	/* check to see if you have the accounting fd */
	accounting_fd = dup(3);
	/* if this works, then you have one */
	if( accounting_fd >= 0 ){
		(void)close( accounting_fd );
		accounting_fd = 3;
	} else {
		accounting_fd = -1;
	}
	if( fcntl(0,F_GETFL,0) == -1 ){
		FPRINTF(STDERR,"BAD FD 0\n");
		exit(2);
	}
	if( fcntl(1,F_GETFL,0) == -1 ){
		FPRINTF(STDERR,"BAD FD 1\n");
		exit(2);
	}
	if( fcntl(2,F_GETFL,0) == -1 ){
		FPRINTF(STDERR,"BAD FD 2\n");
		exit(2);
	}
	getargs( argc, argv, envp );
	/*
	 * Turn off SIGPIPE
	 */
	(void)signal( SIGPIPE, SIG_IGN );
	(void)signal( SIGINT,  SIG_DFL );
	(void)signal( SIGHUP,  SIG_DFL );
	(void)signal( SIGQUIT, SIG_DFL );
	(void)signal( SIGCHLD, SIG_DFL );
	if( of_filter || (format && format[0] == 'o') ){
		filter_pgm( filter_stop );
	} else {
		filter_pgm( (char *)0 );
	}
	return(0);
}

int Write_fd_str( int fd, const char *msg )
{
	int n;
	n = strlen(msg);
	return write(fd,msg,n);
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

	buf[0] = 0;
	(void) VSNPRINTF (buf, sizeof(buf)) format, ap);
	Write_fd_str(fd,buf);
}

/****************************************************************************
 * Extract the necessary definitions for error message reporting
 ****************************************************************************/

#if !defined(HAVE_STRERROR)
# undef  num_errors
# if defined(HAVE_SYS_ERRLIST)
#  if !defined(HAVE_DECL_SYS_ERRLIST)
     extern const char *const sys_errlist[];
#  endif
#  if defined(HAVE_SYS_NERR)
#   if !defined(HAVE_DECL_SYS_NERR)
      extern int sys_nerr;
#   endif
#   define num_errors    (sys_nerr)
#  endif
# endif
# if !defined(num_errors)
#   define num_errors   (-1)            /* always use "errno=%d" */
# endif
#endif

const char * Errormsg ( int err )
{
    const char *cp;

#if defined(HAVE_STRERROR)
	cp = strerror(err);
#else
# if defined(HAVE_SYS_ERRLIST)
    if (err >= 0 && err <= num_errors) {
		cp = sys_errlist[err];
    } else
# endif
	{
		static char msgbuf[32];     /* holds "errno=%d". */
		(void) SNPRINTF(msgbuf, sizeof(msgbuf)) "errno=%d", err);
		cp = msgbuf;
    }
#endif
    return (cp);
}

#ifdef HAVE_STDARGS
void logerr(char *msg, ...)
#else
void logerr( va_alist ) va_dcl
#endif
{
#ifndef HAVE_STDARGS
	char *msg;
#endif
	int err = errno;
	int n;
	char buf[1024];
	VA_LOCAL_DECL
	VA_START(msg);
	VA_SHIFT(msg, char *);

	(void)SNPRINTF(buf,sizeof(buf)) "%s: ", name);
	n = strlen(buf);
	(void)VSNPRINTF(buf+n,sizeof(buf)-n) msg, ap);
	n = strlen(buf);
	(void)SNPRINTF(buf+n,sizeof(buf)-n) "- %s\n", Errormsg(err) );
	Write_fd_str(2,buf);
	VA_END;
}

#ifdef HAVE_STDARGS
void logerr_die(char *msg, ...)
#else
void logerr_die( va_alist ) va_dcl
#endif
{
#ifndef HAVE_STDARGS
	char *msg;
#endif
	int err = errno;
	int n;
	char buf[1024];
	VA_LOCAL_DECL
	VA_START(msg);
	VA_SHIFT(msg, char *);

	(void)SNPRINTF(buf,sizeof(buf)) "%s: ", name);
	n = strlen(buf);
	(void)VSNPRINTF(buf+n,sizeof(buf)-n) msg, ap);
	n = strlen(buf);
	(void)SNPRINTF(buf+n,sizeof(buf)-n) "- %s\n", Errormsg(err) );
	Write_fd_str(2,buf);
	VA_END;
	exit(errorcode);
}

/*
 *	doaccnt()
 *	writes the accounting information to the accounting file
 *  This has the format: user host printer pages format date
 */
void doaccnt(void)
{
	time_t t;
	char buffer[256];
	FILE *f;
	int l, len, c;

	t = time((time_t *)0);
		
	SNPRINTF(buffer, sizeof(buffer)) "%s\t%s\t%s\t%7d\t%s\t%s\n",
		login? login: "NULL", 
		host? host: "NULL", 
		printer? printer: "NULL", 
		npages,
		format? format: "NULL", 
		Time_str(0,0));
	len = strlen( buffer );
	if( accounting_fd < 0 ){
		if(accntfile && (f = fopen(accntfile, "a" )) != NULL ) {
			accounting_fd = fileno( f );
		}
	}
	if( accounting_fd >= 0 ){
		for( c = l = 0; c >= 0 && l < len; l += c ){
			c = write( accounting_fd, &buffer[l], len-l );
		}
		if( c < 0 ){
			logerr( "bad write to accounting file" );
		}
	}
}

void getargs( int argc, char *argv[], char *envp[] )
{
	int i, c;		/* argument index */
	char *arg, *optargv;	/* argument */
	char *s, *end;

	if( (name = argv[0]) == 0 ) name = "FILTER";
	if( (s = strrchr( name, '/' )) ){
		++s;
	} else {
		s = name;
	}
	of_filter =  (strstr( s, "of" ) != 0);
	for( i = 1; i < argc && (arg = argv[i])[0] == '-'; ++i ){
		if( (c = arg[1]) == 0 ){
			FPRINTF( STDERR, "missing option flag");
			i = argc;
			break;
		}
		if( c == 'c' ){
			literal = 1;
			continue;
		}
		optargv = &arg[2];
		if( arg[2] == 0 ){
			optargv = argv[i++];
			if( optargv == 0 ){
				FPRINTF( STDERR, "missing option '%c' value", c );
				i = argc;
				break;
			}
		}
		switch(c){
			case 'C': class = optargv; break; 
			case 'E': errorfile = optargv; break;
			case 'T':
					for( s = optargv; s && *s; s = end ){
						end = strchr( s, ',' );
						if( end ){
							*end++ = 0;
						}
						if( !strcasecmp( s, "crlf" ) ){
							crlf = 1;
						}
						if( !strcasecmp( s, "debug" ) ){
							++debug;
						}
					}
					break;
			case 'F': format = optargv; break; 
			case 'J': job = optargv; break; 
			case 'K': controlfile = optargv; break; 
			case 'L': bnrname = optargv; break; 
			case 'P': printer = optargv; break; 
			case 'Q': queuename = optargv; break; 
			case 'R': accntname = optargv; break; 
			case 'S': comment = optargv; break; 
			case 'Z': zopts = optargv; break; 
			case 'h': host = optargv; break; 
			case 'i': indent = atoi( optargv ); break; 
			case 'l': length = atoi( optargv ); break; 
			case 'n': login = optargv; break; 
			case 's': statusfile = optargv; break; 
			case 'w': width = atoi( optargv ); break; 
			case 'x': xwidth = atoi( optargv ); break; 
			case 'y': ylength = atoi( optargv ); break;
			default: break;
		}
	}
	if( i < argc ){
		accntfile = argv[i];
	}
	if( errorfile ){
		int fd;
		fd = open( errorfile, O_APPEND | O_WRONLY, 0600 );
		if( fd < 0 ){
			FPRINTF( STDERR, "cannot open error log file '%s'", errorfile );
		} else {
			FPRINTF( STDERR, "using error log file '%s'", errorfile );
			if( fd != 2 ){
				dup2(fd, 2 );
				close(fd);
			}
		}
	}
	if( debug ){
		FPRINTF(STDERR, "%s command: ", name );
		for( i = 0; i < argc; ++i ){
			FPRINTF(STDERR, "%s ", argv[i] );
		}
		FPRINTF( STDERR, "\n" );
	}
	if( debug ){
		FPRINTF(STDERR, "FILTER decoded options: " );
		FPRINTF(STDERR,"accntfile '%s'\n", accntfile? accntfile : "null" );
		FPRINTF(STDERR,"accntname '%s'\n", accntname? accntname : "null" );
		FPRINTF(STDERR,"class '%s'\n", class? class : "null" );
		FPRINTF(STDERR,"errorfile '%s'\n", errorfile? errorfile : "null" );
		FPRINTF(STDERR,"format '%s'\n", format? format : "null" );
		FPRINTF(STDERR,"host '%s'\n", host? host : "null" );
		FPRINTF(STDERR,"indent, %d\n", indent);
		FPRINTF(STDERR,"job '%s'\n", job? job : "null" );
		FPRINTF(STDERR,"length, %d\n", length);
		FPRINTF(STDERR,"literal, %d\n", literal);
		FPRINTF(STDERR,"login '%s'\n", login? login : "null" );
		FPRINTF(STDERR,"printer '%s'\n", printer? printer : "null" );
		FPRINTF(STDERR,"queuename '%s'\n", queuename? queuename : "null" );
		FPRINTF(STDERR,"statusfile '%s'\n", statusfile? statusfile : "null" );
		FPRINTF(STDERR,"width, %d\n", width);
		FPRINTF(STDERR,"xwidth, %d\n", xwidth);
		FPRINTF(STDERR,"ylength, %d\n", ylength);
		FPRINTF(STDERR,"zopts '%s'\n", zopts? zopts : "null" );

		FPRINTF(STDERR, "FILTER environment: " );
		for( i = 0; (arg = envp[i]); ++i ){
			FPRINTF(STDERR,"%s\n", arg );
		}
		FPRINTF(STDERR, "RUID: %d, EUID: %d\n", (int)getuid(), (int)geteuid() );
	}
}

/*
 * suspend_ofilter():  suspends the output filter, waits for a signal
 */
void suspend_ofilter(void)
{
	fflush(stdout);
	fflush(stderr);
	if(debug)FPRINTF(STDERR,"FILTER suspending\n");
	kill(getpid(), SIGSTOP);
	if(debug)FPRINTF(STDERR,"FILTER awake\n");
}
/******************************************
 * prototype filter()
 * filter will scan the input looking for the suspend string
 * if any.
 ******************************************/

void filter_pgm(char *stop)
{
	int c;
	int state, i, xout, lastc;
	int lines = 0;
	char inputline[1024];
	int inputcount = 0;

	/*
	 * do whatever initializations are needed
	 */
	/* FPRINTF(STDERR, "filter ('%s')\n", stop ? stop : "NULL" ); */
	/*
	 * now scan the input string, looking for the stop string
	 */
	lastc = xout = state = 0;
	npages = 1;

	inputcount = 0;
	while( (c = getchar()) != EOF ){
		if( inputcount < (int)sizeof(inputline) - 3 ) inputline[inputcount++] = c;
		if( c == '\n' ){
			inputline[inputcount-1] = 0;
			if(debug)FPRINTF(STDERR,"INPUTLINE count %d '%s'\n", inputcount, inputline );
			inputcount = 0;
			++lines;
			if( lines > length ){
				lines -= length;
				++npages;
			}
			if( !literal && crlf == 0 && lastc != '\r' ){
				putchar( '\r' );
			}
		}
		if( c == '\014' ){
			++npages;
			lines = 0;
			if( !literal && crlf == 0 ){
				putchar( '\r' );
			}
		}
		if( stop ){
			if( c == stop[state] ){
				++state;
				if( stop[state] == 0 ){
					state = 0;
					suspend_ofilter();
				}
			} else if( state ){
				for( i = 0; i < state; ++i ){
					putchar( stop[i] );
				}
				state = 0;
				putchar( c );
			} else {
				putchar( c );
			}
		} else {
			putchar( c );
		}
		lastc = c;
	}
	if( ferror( stdin ) ){
		logerr( "error on STDIN");
	}
	for( i = 0; i < state; ++i ){
		putchar( stop[i] );
	}
	if( lines > 0 ){
		++npages;
	}
	doaccnt();
}

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
			logerr_die( "Time_str: gettimeofday failed");
		}
		t = tv.tv_sec;
	}
	tmptr = localtime( &t );
	if( shortform ){
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
	return( buffer );
}
