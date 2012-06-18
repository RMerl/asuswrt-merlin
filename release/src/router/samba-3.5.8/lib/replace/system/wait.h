#ifndef _system_wait_h
#define _system_wait_h
/* 
   Unix SMB/CIFS implementation.

   waitpid system include wrappers

   Copyright (C) Andrew Tridgell 2004

     ** NOTE! The following LGPL license applies to the replace
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.

*/

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <signal.h>

#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif

#ifndef SIGNAL_CAST
#define SIGNAL_CAST (RETSIGTYPE (*)(int))
#endif

#ifdef HAVE_SETJMP_H
#include <setjmp.h>
#endif

#ifndef SA_RESETHAND
#define SA_RESETHAND SA_ONESHOT
#endif

#if !defined(HAVE_SIG_ATOMIC_T_TYPE)
typedef int sig_atomic_t;
#endif

#if !defined(HAVE_WAITPID) && defined(HAVE_WAIT4)
int rep_waitpid(pid_t pid,int *status,int options)
#endif

#endif
