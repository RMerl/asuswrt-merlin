/* Portable <sys/ptrace.h>

   Copyright (C) 2004, 2005, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */
   
#ifndef GDB_PTRACE_H
#define GDB_PTRACE_H

/* The <sys/ptrace.h> header was introduced with 4.4BSD, and provided
   the PT_* symbolic constants for the ptrace(2) request numbers.  The
   ptrace(2) prototype was added later to the same header on BSD.
   SunOS and GNU/Linux have slightly different symbolic names for the
   constants that start with PTRACE_*.  System V still doesn't have
   (and probably never will have) a <sys/ptrace.h> with symbolic
   constants; the ptrace(2) prototype can be found in <unistd.h>.
   Fortunately all systems use the same numerical constants for the
   common ptrace requests.  */

#ifdef HAVE_PTRACE_H
# include <ptrace.h>
#elif defined(HAVE_SYS_PTRACE_H)
# include <sys/ptrace.h>
#endif

/* No need to include <unistd.h> since it's already included by
   "defs.h".  */

#ifndef PT_TRACE_ME
# define PT_TRACE_ME	0
#endif

#ifndef PT_READ_I
# define PT_READ_I	1	/* Read word in child's I space.  */
#endif

#ifndef PT_READ_D
# define PT_READ_D	2	/* Read word in child's D space.  */
#endif

#ifndef PT_READ_U
# define PT_READ_U	3	/* Read word in child's U space.  */
#endif

#ifndef PT_WRITE_I
# define PT_WRITE_I	4	/* Write word in child's I space.  */
#endif

#ifndef PT_WRITE_D
# define PT_WRITE_D	5	/* Write word in child's D space.  */
#endif

#ifndef PT_WRITE_U
# define PT_WRITE_U	6	/* Write word in child's U space.  */
#endif

/* HP-UX doesn't define PT_CONTINUE and PT_STEP.  Instead of those two
   ptrace requests, it has PT_CONTIN, PT_CONTIN1, PT_SINGLE and
   PT_SINGLE1.  PT_CONTIN1 and PT_SINGLE1 preserve pending signals,
   which apparently is what is wanted by the HP-UX native code.  */

#ifndef PT_CONTINUE
# ifdef PT_CONTIN1
#  define PT_CONTINUE	PT_CONTIN1
# else
#  define PT_CONTINUE	7	/* Continue the child.  */
# endif
#endif

#ifndef PT_KILL
# define PT_KILL	8	/* Kill the child process.  */
#endif

#ifndef PT_STEP
# ifdef PT_SINGLE1
#  define PT_STEP	PT_SINGLE1
# else
#  define PT_STEP	9	/* Single step the child.   */
# endif
#endif

/* Not all systems support attaching and detaching.   */

#ifndef PT_ATTACH
# ifdef PTRACE_ATTACH
#  define PT_ATTACH PTRACE_ATTACH
# endif
#endif

#ifndef PT_DETACH
# ifdef PTRACE_DETACH
#  define PT_DETACH PTRACE_DETACH
# endif
#endif

/* Some systems, in particular DEC OSF/1, Digital Unix, Compaq Tru64
   or whatever it's called these days, don't provide a prototype for
   ptrace.  Provide one to silence compiler warnings.  */

#ifndef HAVE_DECL_PTRACE
extern PTRACE_TYPE_RET ptrace();
#endif

/* Some systems, at least AIX and HP-UX have a ptrace with five
   arguments.  Since we never use the fifth argument, define a ptrace
   macro that calls the real ptrace with the last argument set to
   zero.  */

#ifdef PTRACE_TYPE_ARG5
# define ptrace(request, pid, addr, data) ptrace (request, pid, addr, data, 0)
#endif

#endif /* gdb_ptrace.h */
