/* This testcase is part of GDB, the GNU debugger.

   Copyright 1992, 1993, 1994, 1995, 1999, 2002, 2003, 2007
   Free Software Foundation, Inc.

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

   Please email any bugs, comments, and/or additions to this file to:
   bug-gdb@prep.ai.mit.edu  */

#ifdef vxworks

#  include <stdio.h>

/* VxWorks does not supply atoi.  */
static int
atoi (z)
     char *z;
{
  int i = 0;

  while (*z >= '0' && *z <= '9')
    i = i * 10 + (*z++ - '0');
  return i;
}

/* I don't know of any way to pass an array to VxWorks.  This function
   can be called directly from gdb.  */

vxmain (arg)
char *arg;
{
  char *argv[2];

  argv[0] = "";
  argv[1] = arg;
  main (2, argv, (char **) 0);
}

#else /* ! vxworks */
#  include <stdio.h>
#  include <stdlib.h>
#endif /* ! vxworks */

#ifdef PROTOTYPES
extern int marker1 (void);
extern int marker2 (int a);
extern void marker3 (char *a, char *b);
extern void marker4 (long d);
#else
extern int marker1 ();
extern int marker2 ();
extern void marker3 ();
extern void marker4 ();
#endif

/*
 *	This simple classical example of recursion is useful for
 *	testing stack backtraces and such.
 */

#ifdef PROTOTYPES
int factorial(int);

int
main (int argc, char **argv, char **envp)
#else
int
main (argc, argv, envp)
int argc;
char *argv[], **envp;
#endif
{
#ifdef usestubs
    set_debug_traps();  /* set breakpoint 5 here */
    breakpoint();
#endif
    if (argc == 12345) {  /* an unlikely value < 2^16, in case uninited */ /* set breakpoint 6 here */
	fprintf (stderr, "usage:  factorial <number>\n");
	return 1;
    }
    printf ("%d\n", factorial (atoi ("6")));  /* set breakpoint 1 here */
    /* set breakpoint 12 here */
    marker1 ();  /* set breakpoint 11 here */
    marker2 (43); /* set breakpoint 20 here */
    marker3 ("stack", "trace"); /* set breakpoint 21 here */
    marker4 (177601976L);
    /* We're used by a test that requires malloc, so make sure it is
       in the executable.  */
    (void)malloc (1);

    argc = (argc == 12345); /* This is silly, but we can step off of it */ /* set breakpoint 2 here */
    return argc;  /* set breakpoint 10 here */
} /* set breakpoint 10a here */

#ifdef PROTOTYPES
int factorial (int value)
#else
int factorial (value)
int value;
#endif
{
  if (value > 1) {  /* set breakpoint 7 here */
	value *= factorial (value - 1);
    }
    return (value); /* set breakpoint 19 here */
}

#ifdef PROTOTYPES
int multi_line_if_conditional (int a, int b, int c)
#else
int multi_line_if_conditional (a, b, c)
  int a, b, c;
#endif
{
  if (a    /* set breakpoint 3 here */
      && b
      && c)
    return 0;
  else
    return 1;
}

#ifdef PROTOTYPES
int multi_line_while_conditional (int a, int b, int c)
#else
int multi_line_while_conditional (a, b, c)
  int a, b, c;
#endif
{
  while (a /* set breakpoint 4 here */
      && b
      && c)
    {
      a--, b--, c--;
    }
  return 0;
}
