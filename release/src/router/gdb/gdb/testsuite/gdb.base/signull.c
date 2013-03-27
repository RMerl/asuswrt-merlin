/* This testcase is part of GDB, the GNU debugger.

   Copyright 1996, 1999, 2003, 2004, 2007 Free Software Foundation, Inc.

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

#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

enum tests {
  code_entry_point, code_descriptor, data_read, data_write
};

static volatile enum tests test;

/* Some basic types and zero buffers.  */

typedef long data_t;
typedef long code_t (void);
data_t *volatile data;
code_t *volatile code;
/* "desc" is intentionally initialized to a data object.  This is
   needed to test function descriptors on arches like ia64.  */
data_t zero[10];
code_t *volatile desc = (code_t *) (void *) zero;

sigjmp_buf env;

extern void
keeper (int sig)
{
  siglongjmp (env, 0);
}

extern long
bowler (void)
{
  switch (test)
    {
    case data_read:
      /* Try to read address zero.  */
      return (*data);
    case data_write:
      /* Try to write (the assignment) to address zero.  */
      return (*data) = 1;
    case code_entry_point:
      /* For typical architectures, call a function at address
	 zero.  */
      return (*code) ();
    case code_descriptor:
      /* For atypical architectures that use function descriptors,
	 call a function descriptor, the code field of which is zero
	 (which has the effect of jumping to address zero).  */
      return (*desc) ();
    }
}

int
main ()
{
  static volatile int i;

  struct sigaction act;
  memset (&act, 0, sizeof act);
  act.sa_handler = keeper;
  sigaction (SIGSEGV, &act, NULL);

  for (i = 0; i < 10; i++)
    {
      sigsetjmp (env, 1);
      bowler ();
    }
}
