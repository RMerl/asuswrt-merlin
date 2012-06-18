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
"$Id: proctitle.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";

#include "lp.h"
#include "proctitle.h"
/**** ENDINCLUDE ****/

/*
 *  SETPROCTITLE -- set process title for ps
 *  proctitle( char *str );
 * 
 *	Returns: none.
 * 
 *	Side Effects: Clobbers argv of our main procedure so ps(1) will
 * 		      display the title.
 */

/*
 * From the Sendmail.8.8.8 Source Distribution
 *
 * Copyright (c) 1983, 1995-2000 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)conf.h	8.335 (Berkeley) 10/24/97
 */

#ifdef __hpux
# define SPT_TYPE	SPT_PSTAT
#endif

#if defined(_AIX32) || defined(_AIX) || defined(AIX) || defined(IS_AIX32)
# define SPT_PADCHAR	'\0'	/* pad process title with nulls */
#endif

#ifdef	DGUX
# define SPT_TYPE	SPT_NONE	/* don't use setproctitle */
#endif

#if defined(BSD4_4) && !defined(__bsdi__) && !defined(__GNU__)
# define SPT_TYPE	SPT_PSSTRINGS	/* use PS_STRINGS pointer */
#endif

#ifdef __bsdi__
# if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199312
#  undef SPT_TYPE
#  define SPT_TYPE	SPT_BUILTIN	/* setproctitle is in libc */
# else
#  define SPT_PADCHAR	'\0'	/* pad process title with nulls */
# endif
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
# if defined(__NetBSD__) && (NetBSD > 199307 || NetBSD0_9 > 1)
#  undef SPT_TYPE
#  define SPT_TYPE	SPT_BUILTIN	/* setproctitle is in libc */
# endif
# if defined(__FreeBSD__)
#  undef SPT_TYPE
#  if __FreeBSD__ >= 2
#   include <osreldate.h>		/* and this works */
#   if __FreeBSD_version >= 199512	/* 2.2-current right now */
#    include <libutil.h>
#    define SPT_TYPE	SPT_BUILTIN
#   endif
#  endif
#  ifndef SPT_TYPE
#   define SPT_TYPE	SPT_REUSEARGV
#   define SPT_PADCHAR	'\0'		/* pad process title with nulls */
#  endif
# endif
# if defined(__OpenBSD__)
#  undef SPT_TYPE
#  define SPT_TYPE	SPT_BUILTIN	/* setproctitle is in libc */
# endif
#endif

#ifdef __GNU_HURD__
# define SPT_TYPE	SPT_CHANGEARGV
#endif /* GNU */

/* SCO UNIX 3.2v4.0 Open Desktop 2.0 and earlier */
#ifdef _SCO_unix_
# define SPT_TYPE	SPT_SCO		/* write kernel u. area */
#endif

#ifdef __linux__
# define SPT_PADCHAR	'\0'		/* pad process title with nulls */
#endif

#ifdef _SEQUENT_
# define SPT_TYPE	SPT_NONE	/* don't use setproctitle */
#endif

#ifdef apollo
# define SPT_TYPE	SPT_NONE	/* don't use setproctitle */
#endif
#ifdef NCR_MP_RAS2
# define SPT_TYPE  SPT_NONE
#endif

#ifdef NCR_MP_RAS3
# define SPT_TYPE 	SPT_NONE
#endif
/*
**  SETPROCTITLE -- set process title for ps
**
**	Parameters:
**		fmt -- a printf style format string.
**		a, b, c -- possible parameters to fmt.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Clobbers argv of our main procedure so ps(1) will
**		display the title.
*/

#define SPT_NONE	0	/* don't use it at all */
#define SPT_REUSEARGV	1	/* cover argv with title information */
#define SPT_BUILTIN	2	/* use libc builtin */
#define SPT_PSTAT	3	/* use pstat(PSTAT_SETCMD, ...) */
#define SPT_PSSTRINGS	4	/* use PS_STRINGS->... */
#define SPT_SYSMIPS	5	/* use sysmips() supported by NEWS-OS 6 */
#define SPT_SCO		6	/* write kernel u. area */
#define SPT_CHANGEARGV	7	/* write our own strings into argv[] */

#ifndef SPT_TYPE
# define SPT_TYPE	SPT_REUSEARGV
#endif

#if SPT_TYPE != SPT_NONE && SPT_TYPE != SPT_BUILTIN

# if SPT_TYPE == SPT_PSTAT
#  include <sys/pstat.h>
# endif
# if SPT_TYPE == SPT_PSSTRINGS
#  include <machine/vmparam.h>
#  include <sys/exec.h>
#  ifndef PS_STRINGS	/* hmmmm....  apparently not available after all */
#   undef SPT_TYPE
#   define SPT_TYPE	SPT_REUSEARGV
#  else
#   ifndef NKPDE			/* FreeBSD 2.0 */
#    define NKPDE 63
 typedef unsigned int	*pt_entry_t;
#   endif
#  endif
# endif

# if SPT_TYPE == SPT_PSSTRINGS || SPT_TYPE == SPT_CHANGEARGV
#  define SETPROC_STATIC	static
# else
#  define SETPROC_STATIC
# endif

# if SPT_TYPE == SPT_SYSMIPS
#  include <sys/sysmips.h>
#  include <sys/sysnews.h>
# endif

# if SPT_TYPE == SPT_SCO
#  include <sys/immu.h>
#  include <sys/dir.h>
#  include <sys/user.h>
#  include <sys/fs/s5param.h>
#  if PSARGSZ > LINEBUFFER
#   define SPT_BUFSIZE	PSARGSZ
#  endif
# endif

# ifndef SPT_PADCHAR
#  define SPT_PADCHAR	' '
# endif

# ifndef SPT_BUFSIZE
#  define SPT_BUFSIZE	LINEBUFFER
# endif

#endif /* SPT_TYPE != SPT_NONE && SPT_TYPE != SPT_BUILTIN */

/*
**  Pointers for setproctitle.
**	This allows "ps" listings to give more useful information.
*/
# if SPT_TYPE != SPT_BUILTIN
 static char	**Argv = NULL;		/* pointer to argument vector */
 static char	*LastArgv = NULL;	/* end of argv */
#endif


 void initsetproctitle(argc, argv, envp)
	int argc;
	char **argv;
	char **envp;
{
# if SPT_TYPE != SPT_BUILTIN
	register int i, envpsize = 0;
	extern char **environ;

	/*
	**  Move the environment so setproctitle can use the space at
	**  the top of memory.
	*/

	DEBUG1("initsetproctitle: doing setup");
	for (i = 0; envp[i] != NULL; i++)
		envpsize += safestrlen(envp[i]) + 1;
	{
	char *s;
	environ = (char **) malloc_or_die((sizeof (char *) * (i + 1))+envpsize+1,__FILE__,__LINE__);
	s = ((char *)environ)+((sizeof (char *) * (i + 1)));
	for (i = 0; envp[i] != NULL; i++){
		strcpy(s,envp[i]);
		environ[i] = s;
		s += safestrlen(s)+1;
	}
	}
	environ[i] = NULL;

	/*
	**  Save start and extent of argv for setproctitle.
	*/

	Argv = argv;

	/*
	**  Determine how much space we can use for setproctitle.  
	**  Use all contiguous argv and envp pointers starting at argv[0]
 	*/
	for (i = 0; i < argc; i++)
	{
		if (i==0 || LastArgv + 1 == argv[i])
			LastArgv = argv[i] + safestrlen(argv[i]);
		else
			continue;
	}
	for (i=0; envp[i] != NULL; i++)
	{
		if (LastArgv + 1 == envp[i])
			LastArgv = envp[i] + safestrlen(envp[i]);
		else
			continue;
	}
	DEBUG1("initsetproctitle: Argv 0x%lx, LastArgv 0x%lx", Argv, LastArgv);
#else
	DEBUG1("initsetproctitle: using builtin");
#endif
}

#if SPT_TYPE != SPT_BUILTIN

 void
#ifdef HAVE_STDARGS
 setproctitle (const char *fmt,...)
#else
 setproctitle (va_alist) va_dcl
#endif

{
# if SPT_TYPE != SPT_NONE
	register int i;
	SETPROC_STATIC char buf[SPT_BUFSIZE];
#  if SPT_TYPE == SPT_PSTAT
	union pstun pst;
#  endif
#  if SPT_TYPE == SPT_SCO
	off_t seek_off;
	static int kmem = -1;
	static int kmempid = -1;
	struct user u;
#  endif

    VA_LOCAL_DECL
    /* print the argument string */
    VA_START (fmt);
    VA_SHIFT (fmt, char *);
    (void) VSNPRINTF(buf, sizeof(buf)) fmt, ap);
    VA_END;

	i = safestrlen(buf);

#  if SPT_TYPE == SPT_PSTAT
	pst.pst_command = buf;
	pstat(PSTAT_SETCMD, pst, i, 0, 0);
#  endif
#  if SPT_TYPE == SPT_PSSTRINGS
	PS_STRINGS->ps_nargvstr = 1;
	PS_STRINGS->ps_argvstr = buf;
#  endif
#  if SPT_TYPE == SPT_SYSMIPS
	sysmips(SONY_SYSNEWS, NEWS_SETPSARGS, buf);
#  endif
#  if SPT_TYPE == SPT_SCO
	if (kmem < 0 || kmempid != getpid())
	{
		if (kmem >= 0)
			close(kmem);
		kmem = open(_PATH_KMEM, O_RDWR, 0);
		if (kmem < 0)
			return;
		(void) fcntl(kmem, F_SETFD, 1);
		kmempid = getpid();
	}
	buf[PSARGSZ - 1] = '\0';
	seek_off = UVUBLK + (off_t) u.u_psargs - (off_t) &u;
	if (lseek(kmem, (off_t) seek_off, SEEK_SET) == seek_off)
		(void) write(kmem, buf, PSARGSZ);
#  endif
#  if SPT_TYPE == SPT_REUSEARGV
	if (i > LastArgv - Argv[0] - 2)
	{
		i = LastArgv - Argv[0] - 2;
		buf[i] = '\0';
	}
	(void) strcpy(Argv[0], buf);
	{ char *p;
	p = &Argv[0][i];
	while (p < LastArgv)
		*p++ = SPT_PADCHAR;
	}
	Argv[1] = NULL;
#  endif
#  if SPT_TYPE == SPT_CHANGEARGV
	Argv[0] = buf;
	Argv[1] = 0;
#  endif
# endif /* SPT_TYPE != SPT_NONE */
}

#endif /* SPT_TYPE != SPT_BUILTIN */
