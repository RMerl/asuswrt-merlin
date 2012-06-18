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
 * $Id: portable.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/

#ifndef _PLP_PORTABLE_H
#define _PLP_PORTABLE_H 1

/***************************************************************************
 * MODULE: portable.h
 * PURPOSE:
 * The configure program generates config.h,  which defines various
 * macros indicating the presence or abscence of include files, etc.
 * However, there are some systems which pass the tests,  but things
 * do not work correctly on them.  This file will try and fix
 * these things up for the user.
 *
 * NOTE:  if there were no problems, this file would be:
 *    #include "config.h"
 *
 * Sigh. Patrick Powell Thu Apr  6 07:00:48 PDT 1995 <papowell@sdsu.edu>
 *    NOTE: thanks to all the folks who worked on the PLP software,
 *    Justin Mason <jmason@iona.ie> especially.  Some of the things
 *    that you have to do to get portability are truely bizzare.
 *
 * portable.h,v 3.14 1998/03/24 02:43:22 papowell Exp
 **************************************************************************/

#if !defined(EXTERN)
#define EXTERN extern
#define DEFINE(X) 
#undef DEFS
#endif

#ifndef __STDC__
LPRng requires ANSI Standard C compiler
#endif

#include "config.h"

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

/*************************************************************************
 * ARGH: some things that "configure" can't get right.
*************************************************************************/

/***************************************************************************
 * porting note: if you port PLP and you get some errors
 * caused by autoconf guessing the wrong set of functions/headers/structs,
 * add or change the entry for your system in the ARGH section below.
 * You might want to try and determine how your system is identified
 * by the C preprocessor and use this informaton rather than trying
 * to look for information in various f1les.
 *    Patrick Powell and Justin Mason
 ***************************************************************************/

/*************************************************************************
 * APOLLO Ports
 *  Thu Apr  6 07:01:51 PDT 1995 Patrick Powell
 * This appears to be historical.
 *************************************************************************/
#ifdef apollo
# define IS_APOLLO OSVERSION
/* #undef __STDC__ */
/* # define CONFLICTING_PROTOS */
#endif

/*************************************************************************
 * ULTRIX.
 * Patrick Powell Thu Apr  6 07:17:34 PDT 1995
 * 
 * Take a chance on using the standard calls
 *************************************************************************/
#ifdef ultrix
# define IS_ULTRIX OSVERSION
#endif


/*************************************************************************
 * AIX.
 *************************************************************************/
#ifdef _AIX32 
# define IS_AIX32 OSVERSION
#endif

/*************************************************************************
 * Sun
 *************************************************************************/

#if defined(sun)
#endif

/*************************************************************************
 * SCO OpenServer 5.0.5
 *************************************************************************/
/* normal include files do not define MAXPATHLEN - rather PATHSIZE in 
   sys/param.h */
#ifdef sco
#ifndef MAXPATHLEN
#define MAXPATHLEN	PATHSIZE
#endif
/* SCO doesn't define the S_ISSOCK POSIX macro to use in testing the 
   stat.st_mode structure member  - it appears as though a socket has
   st_mode = 0020000 (same as character special) */
#ifndef S_ISSOCK
#define S_IFSOCK	0020000
#define S_ISSOCK(m)	(((m) & S_IFMT) == S_IFSOCK)
#endif
#endif

/*************************************************************************
 * Cray
 *************************************************************************/

#if defined(cray)
#define MAXPATHLEN	1023
#define HAVE_SIGLONGJMP	1

/* configure incorrectly chooses STATVFS */
#if defined(USE_STATFS_TYPE)
#undef  USE_STATFS_TYPE
#endif

#define USE_STATFS_TYPE	SRV3_STATFS
#endif


/*************************************************************************/
#if defined(NeXT)
# define IS_NEXT OSVERSION
# define __STRICT_BSD__
#endif

/*************************************************************************/
#if defined(__sgi) && defined(_SYSTYPE_SVR4)
# define IS_IRIX5 OSVERSION
#endif

/*************************************************************************/
#if defined(__sgi) && defined(_SYSTYPE_SYSV)
#define IS_IRIX4 OSVERSION
#endif

/*************************************************************************/
#if defined(__linux__) || defined (__linux) || defined (LINUX)
# define IS_LINUX OSVERSION
#endif

/*************************************************************************/

#if defined(__convex__) /* Convex OS 11.0 - from w_stef */
# define IS_CONVEX OSVERSION
# define LPASS8 (L004000>>16)
#endif

/*************************************************************************/

#ifdef _AUX_SOURCE
# define IS_AUX OSVERSION
# define _POSIX_SOURCE

# undef SETPROCTITLE

#endif

/*************************************************************************/

#if defined(SNI) && defined(sinix)
# define IS_SINIX OSVERSION
#endif


/*************************************************************************/
#if defined(__svr4__) && !defined(SVR4)
# define SVR4 __svr4__
#endif

/***************************************************************************
 * Solaris SUNWorks CC compiler
 *  man page indicates __SVR4 is defined, as is __unix, __sun
 ***************************************************************************/
#if (defined(__SVR4) || defined(_SVR4_)) && !defined(SVR4)
# define SVR4 1
#endif

/*************************************************************************/
#if defined(__bsdi__)
# define IS_BSDI OSVERSION
#endif

/*************************************************************************/

/*************************************************************************
 * we also need some way of spotting IS_DATAGEN (Data Generals),
 * and IS_SEQUENT (Sequent machines). Any suggestions?
 * these ports probably don't work anymore...
 *************************************************************************/

/*************************************************************************
 * END OF ARGH SECTION; next: overrides from the Makefile.
 *************************************************************************/
/*************************
 * STTY functions to use *
 *************************/
#define SGTTYB  0
#define TERMIO  1
#define TERMIOS 2

/*************************
 * FSTYPE functions to use *
 *************************/

#define SVR3_STATFS       0
#define ULTRIX_STATFS     1
#define STATFS            2
#define STATVFS           3

#if defined(MAKE_USE_STATFS)
# undef USE_STATFS
# define USE_STATFS MAKE_USE_STATFS
#endif

#if defined(MAKE_USE_STTY)
# undef  USE_STTY
# define USE_STTY MAKE_USE_STTY 
#endif


/*********************************************************************
 * GET STANDARD INCLUDE FILES
 * This is the one-size-fits-all include that should grab everthing.
 * This has a horrible impact on compilation speed,  but then, do you
 * want compilation speed or portability?
 *
 * Patrick Powell Thu Apr  6 07:21:10 PDT 1995
 *********************************************************************
 * If you do not have the following, you are doomed. Or at least
 * going to have an uphill hard time.
 * NOTE: string.h might also be strings.h on some very very odd systems
 *
 * Patrick Powell Thu Apr  6 07:21:10 PDT 1995
 *********************************************************************/

/*********************************************************************
 * yuck -- this is a nightmare! half-baked-ANSI systems are poxy (jm)
 *
 * Note that configure checks for absolute compliance, i.e.-
 * older versions of SUNOS, HP-UX, do not meet this.
 *
 * Patrick Powell Thu Apr  6 07:21:10 PDT 1995
 *********************************************************************/


#ifdef HAVE_UNISTD_H
# include <unistd.h>
#else
  extern int dup2 ();
  extern int execve ();
  extern uid_t geteuid (), getegid ();
  extern int setgid (), getgid ();
#endif


#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#else
  char *getenv( char * );
  void abort(void);
#endif

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include <sys/stat.h>
#include <pwd.h>
#if defined(HAVE_SYS_SIGNAL_H)
#  include <sys/signal.h>
#endif
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>

#include <errno.h>
#include <grp.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

#ifndef HAVE_STRCHR
# define strchr			index
# define strrchr		rindex
#endif

/* case insensitive compare for OS without it */
#if !defined(HAVE_STRCASECMP)
 int strcasecmp (const char *s1, const char *s2);
#endif
#if !defined(HAVE_STRNCASECMP)
 int strncasecmp (const char *s1, const char *s2, int len );
#endif
#if !defined(HAVE_STRCASECMP_DEF)
 int strcasecmp (const char *s1, const char *s2 );
#endif


/*********************************************************************
 * directory management is nasty.  There are two standards:
 * struct directory and struct dirent.
 * Solution:  macros + a typedef.
 * Patrick Powell Thu Apr  6 07:44:50 PDT 1995
 *
 *See GNU autoconf documentation for this little AHEM gem... and others
 *  too obnoxious to believe
 *********************************************************************/

#if HAVE_DIRENT_H
# include <dirent.h>
# define NLENGTH(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NLENGTH(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

typedef struct dirent plp_dir_t;

/*********************************************************************
 * malloc strikes again. Definition is a la ANSI C.  However,
 * You may need to edit this on historical systems.
 * Patrick Powell Thu Apr  6 07:47:54 PDT 1995
 *********************************************************************/

#if !defined(HAVE_STDLIB_H)
# ifdef HAVE_MALLOC_H
#   include <malloc.h>
# else
   void *malloc(size_t);
   void free(void *);
# endif
#endif

#ifndef HAVE_ERRNO_DECL
 extern int errno;
#endif

/*********************************************************************
 * Note the <time.h> may already be included by some previous
 * lines.  You may need to edit this by hand.
 * Better solution is to put include guards in all of the include files.
 * Patrick Powell Thu Apr  6 07:55:58 PDT 1995
 *********************************************************************/
 
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#   include <sys/time.h>
# else
#   include <time.h>
# endif
#endif

#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#ifdef HAVE_SYS_FCNTL_H
# include <sys/fcntl.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

/*
 * we use the FCNTL code if we have it
 * We want you to define F_SETLK, etc.  If they are not defined,
 *  Then you better put a system dependent configuration
 *  in and define them.
 */
#if defined(HAVE_FCNTL) && ! defined(F_SETLK)
/*ABORT: "defined(HAVE_FCNTL) && ! defined(F_SETLK)"*/
#undef HAVE_FCNTL
#endif

#if defined(HAVE_LOCKF) && ! defined(F_LOCK)
/*ABORT: "defined(HAVE_LOCKF) && ! defined(F_LOCK)"*/
/* You must fix this up */
#undef HAVE_LOCKF
#endif

#if defined(HAVE_FLOCK) && ! defined(LOCK_EX)
/*AB0RT: "defined(HAVE_FLOCK) && ! defined(LOCK_EX)"*/
/* You must fix this up */
#undef HAVE_FLOCK
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

/* 4.2 BSD systems */
#ifndef S_IRUSR
# define S_IRUSR S_IREAD
# define S_IWUSR S_IWRITE
# define S_IXUSR S_IEXEC
# define S_IXGRP (S_IEXEC >> 3)
# define S_IXOTH (S_IEXEC >> 6)
#endif

#ifndef S_ISLNK
# define S_ISLNK(mode) (((mode) & S_IFLNK) == S_IFLNK)
#endif
#ifndef S_ISREG
# define S_ISREG(mode) (((mode) & S_IFREG) == S_IFREG)
#endif
#ifndef S_ISDIR
# define S_ISDIR(mode) (((mode) & S_IFDIR) == S_IFDIR)
#endif


/* 4.2 BSD systems */
#ifndef SEEK_SET
# define SEEK_SET 0
# define SEEK_CUR 1
# define SEEK_END 2
#endif

#ifndef HAVE_KILLPG
# define killpg(pg,sig)	((int) kill ((pid_t)(-(pg)), (sig)))
#else
extern int killpg(pid_t pgrp, int sig);
#endif

/***********************************************************************
 * wait() stuff: most recent systems support a compatability version
 * of "union wait", but it's not as fully-featured as the recent stuff
 * that uses an "int *". However, we want to keep support for the
 * older BSD systems as much as possible, so it's still supported;
 * however, if waitpid() exists, we're POSIX.1 compliant, and we should
 * not use "union wait". (hack hack hack) (jm)
 *
 * I agree.  See the waitchild.c code for a tour through the depths of
 * portability hell.
 *
 * Patrick Powell Thu Apr  6 08:03:58 PDT 1995
 *
 ***********************************************************************/

#ifdef HAVE_WAITPID
# undef HAVE_UNION_WAIT		/* and good riddance */
#endif

/***************************************************************************
 * HAVE_UNION_WAIT will be def'd by configure if it's in <sys/wait.h>,
 * and isn't just there for compatibility (like it is on HP/UX).
 ***************************************************************************/

#ifdef HAVE_UNION_WAIT
typedef union wait		plp_status_t;
/*
 * with some BSDish systems, there are already #defines for this,
 * so we should use them if they're there.
 */
# ifndef WCOREDUMP
#  define WCOREDUMP(x)	((x).w_coredump)
# endif
# ifndef WEXITSTATUS
#  define WEXITSTATUS(x)	((x).w_retcode)
# endif
# ifndef WTERMSIG
#  define WTERMSIG(x)	((x).w_termsig)
# endif
# ifndef WIFSTOPPED
#  define WIFSTOPPED(x)	((x).w_stopval == WSTOPPED)
# endif
# ifndef WIFEXITED
#  define WIFEXITED(x)	((x).w_stopval == WEXITED)
# endif

#else
  typedef int			plp_status_t;
/* The POSIX defaults for these macros. (this is cheating!) */
# ifndef WTERMSIG
#  define WTERMSIG(x)	((x) & 0x7f)
# endif
# ifndef WCOREDUMP
#  define WCOREDUMP(x)	((x) & 0x80)
# endif
# ifndef WEXITSTATUS
#  define WEXITSTATUS(x)	((((unsigned) x) >> 8) & 0xff)
# endif
# ifndef WIFSIGNALED
#  define WIFSIGNALED(x)	(WTERMSIG (x) != 0)
# endif
# ifndef WIFEXITED
#  define WIFEXITED(x)	(WTERMSIG (x) == 0)
# endif
#endif /* HAVE_UNION_WAIT */

/***********************************************************************
 * SVR4: SIGCHLD is really SIGCLD; #define it here.
 * PLP lpd _does_ handle the compatibility semantics properly
 * (Advanced UNIX Programming p. 281).
 ***********************************************************************/

#if !defined(SIGCHLD) && defined(SIGCLD)
# define SIGCHLD			SIGCLD
#endif


/***********************************************************************
 * configure will set RETSIGTYPE to the type returned by signal()
 ***********************************************************************/

typedef RETSIGTYPE plp_signal_t;
typedef plp_signal_t (*plp_sigfunc_t)(int) ;

#ifndef HAVE_GETDTABLESIZE
# ifdef NOFILE
#  define getdtablesize()	NOFILE
# else
#  ifdef NOFILES_MAX
#   define getdtablesize()	NOFILES_MAX
#  endif
# endif
#endif

#ifndef HAVE_STRDUP
# ifdef __STDC__
   char *strdup(const char*);
# else
   char *strdup();
# endif
#endif

#ifndef IPPORT_RESERVED
#define IPPORT_RESERVED 1024
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

#if !defined(IS_ULTRIX) && defined(HAVE_SYSLOG_H)
# include <syslog.h>
#endif
#if defined(HAVE_SYS_SYSLOG_H)
# include <syslog.h>
#endif
# if !(defined(LOG_PID) && defined(LOG_NOWAIT) && defined(HAVE_OPENLOG))
#  undef HAVE_OPENLOG
# endif /* LOG_PID && LOG_NOWAIT */

/*
 *  Priorities (these are ordered)
 */
#ifndef LOG_ERR
# define LOG_EMERG   0   /* system is unusable */
# define LOG_ALERT   1   /* action must be taken immediately */
# define LOG_CRIT    2   /* critical conditions */
# define LOG_ERR     3   /* error conditions */
# define LOG_WARNING 4   /* warning conditions */
# define LOG_NOTICE  5   /* normal but signification condition */
# define LOG_INFO    6   /* informational */
# define LOG_DEBUG   7   /* debug-level messages */
#endif

#ifdef LOG_LPR
# define SYSLOG_FACILITY LOG_LPR
#else
# ifdef LOG_LOCAL0
#  define SYSLOG_FACILITY LOCAL0
# else
#  define SYSLOG_FACILITY (0) /* for Ultrix -- facilities aren't supported */
# endif
#endif

/*************************************************************************
 * If we have SVR4 and no setpgid() then we need getpgrp() 
 *************************************************************************/
#if defined(SVR4) || defined(__alpha__)
# undef HAVE_SETPGRP_0
#endif

/*
 * NONBLOCKING Open and IO - different flags for
 * different systems
 */

#define NONBLOCK (O_NDELAY|O_NONBLOCK)
#if defined(HPUX) && HPUX<110
#  undef NONBLOCK
#  define NONBLOCK (O_NONBLOCK)
#endif

/* fix for HPUX systems with no fd_set values */
#undef FD_SET_FIX
#if !defined(HAVE_FD_SET) && defined(HPUX)
#  define FD_SET_FIX(X) (int *)
#endif


/*********************************************************************
 * AIX systems need this
 *********************************************************************/

#if defined(HAVE_SYS_SELECT_H)
# include <sys/select.h>
#endif

/**********************************************************************
 *  Signal blocking
 **********************************************************************/
#ifdef HAVE_SIGPROCMASK
/* a signal set */
#define plp_block_mask sigset_t
#else
/* an integer */
#define plp_block_mask int
#endif

/**********************************************************************
 *  Select() problems
 **********************************************************************/
#ifdef HAVE_SELECT_H
#include <select.h>
#endif
#if !defined(FD_SET_FIX)
# define FD_SET_FIX(X) X
#endif

/**********************************************************************
 * IPV6 and newer versions
 **********************************************************************/
#if !defined(HAVE_INET_PTON)
int inet_pton( int family, const char *strptr, void *addr );
#endif
#if !defined(HAVE_INET_NTOP)
const char *inet_ntop( int family, const void *addr, char *strptr, size_t len );
#endif


/*****************************************************
 * Internationalisation of messages, using GNU gettext
 *****************************************************/

#if HAVE_LOCALE_H
# include <locale.h>
#endif

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext(Text)
# ifdef gettext_noop
#  define N_(Text) gettext_noop(Text)
# else
#  define N_(Text) Text
# endif
#else
# define _(Text) Text
# define N_(Text) Text
# define textdomain(Domain)
# define bindtextdomain(Package, Directory)
#endif


/**********************************************************************
 *  Cygwin Definitions
 **********************************************************************/
#ifdef __CYGWIN__
#define ROOTUID 18
#else
#define ROOTUID 0
#endif

#ifndef HAVE_FLOCK_DEF
extern int flock( int fd, int operation );
#endif

/**********************************************************************
 *  SUNOS Definitions
 **********************************************************************/
#ifdef SUNOS
extern int _flsbuf(int, FILE *);
extern int _filbuf(FILE *);
extern int accept(int s, struct sockaddr *name, int *namelen);
extern int bind(int s, struct sockaddr *name, int namelen);
extern int connect(int s, struct sockaddr *name, int namelen);
extern void bzero(void *s, size_t n);
extern void endgrent( void );
extern int fflush( FILE *stream );
extern int fclose( FILE *stream );
extern int flock( int fd, int operation );
extern int fprintf(FILE *, const char *, ...);
extern int fputs( const char *, FILE *stream );
extern int fstat(int fd, struct stat *buf );
extern int fseek( FILE *stream, long offset, int ptrname );
extern int ftruncate( int fd, off_t length );
extern int fwrite( char *ptr, int size, int nitems, FILE *stream);
extern int getdtablesize( void );
extern int getpeername(int s, struct sockaddr *name, int *namelen);
extern int getsockname(int s, struct sockaddr *name, int *namelen);
extern int getsockopt(int s, int level, int optname, char *optval,int *optlen);
extern int ioctl(int fd, int request, caddr_t arg );
extern int killpg(int pgrp, int sig );
extern int listen(int s, int backlog );
extern int lockf(int fd, int cmd, long size );
/*extern int lseek(int fd, off_t pos, int how ); */
extern int lstat(const char *path, struct stat *buf );
#define memmove(dest,src,len) bcopy(src,dest,len)
extern void bcopy(const void *src,void *dest,size_t len);
extern int mkstemp(char *s );
extern int openlog( const char *ident, int logopt, int facility );
extern int perror(const char *);
extern int printf( const char *, ...);
extern int rename(const char *, const char *);
extern int select (int width, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
extern void setgrent(void);
extern int seteuid( int euid );
extern int setreuid( int ruid, int euid );
extern int setsockopt(int s, int level, int optname, const char *optval,int optlen);
extern int socket( int domain, int type, int protocol );
extern int socketpair(int, int, int, int *);
extern int sscanf( char *s, char *format, ... );
extern int stat(const char *path, struct stat *buf );
extern int strcasecmp( const char *, const char * );
extern char *strerror( int );
extern int strncasecmp( const char *, const char *, int n );
extern long strtol( const char *str, char **ptr, int base );
extern double strtod( const char *str, char **ptr );
extern int shutdown( int sock, int how );
extern int gettimeofday(struct timeval *tp, struct timezone *tzp);
extern int getrlimit(int resource, struct rlimit *rlp);
extern char * sbrk(int incr);
extern int fchmod(int fd, int mode);
extern int strftime(char *buf, int bufsize, const char *fmt, struct tm *tm);
extern void syslog(int, const char *, ...);
extern int system( const char *str );
extern time_t time( time_t *t );
extern int tolower( int );
extern int toupper( int );
extern int tputs( const char *cp, int affcnt, int (*outc)() );
extern int vfprintf(FILE *, const char *, ...);
extern int vprintf(FILE *, const char *, va_list ap);
#endif


#ifdef SOLARIS
extern int setreuid( uid_t ruid, uid_t euid );
extern int mkstemp(char *s );
#ifdef HAVE_GETDTABLESIZE
extern int getdtablesize(void);
#endif
#endif

#if !defined(HAVE_SYSLOG_DEF)
extern void syslog(int, const char *, ...);
#endif
#if !defined(HAVE_OPENLOG_DEF)
extern int openlog( const char *ident, int logopt, int facility );
#endif

#ifdef IS_AIX32
extern int seteuid(uid_t);
extern int setruid(uid_t);
extern int setenv(char *, char *, int);
#endif


/* IPV6 structures define */

#if defined(AF_INET6)
# if defined(IN_ADDR6)
#  define in6_addr in_addr6
# endif
#endif

#if defined(HAVE_ARPA_NAMESER_H)
# include <arpa/nameser.h>
#endif
#if defined(HAVE_RESOLV_H)
# include <resolv.h>
#endif

#ifdef HAVE_INNETGR
#if !defined(HAVE_INNETGR_DEF)
extern int innetgr(const char *netgroup,
    const char *machine, const char *user, const char *domain);
#endif
#endif


#define Cast_int_to_voidstar(v) ((void *)(long)(v))
#define Cast_ptr_to_int(v) ((int)(long)(v))
#define Cast_ptr_to_long(v) ((long)(v))

/* for testing, set -Wall -Wformat and then make */

#if defined(FORMAT_TEST)
# define FPRINTF fprintf
# define PRINTF printf
# define STDOUT stdout
# define STDERR stderr
# define SNPRINTF(X,Y) printf(
# define VSNPRINTF(X,Y) vprintf(
# define SETSTATUS(X) printf(
#else
# define FPRINTF safefprintf
# define PRINTF safeprintf
# define STDOUT 1
# define STDERR 2
# define SNPRINTF(X,Y) plp_snprintf(X,Y,
# define VSNPRINTF(X,Y) plp_vsnprintf(X,Y,
# define SETSTATUS(X) setstatus(X,
#endif

#endif

#ifdef JYDEBUG//JYWeng
FILE *aaaaaa;
#endif
