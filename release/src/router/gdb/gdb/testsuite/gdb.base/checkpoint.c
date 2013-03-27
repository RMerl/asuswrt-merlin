/* This testcase is part of GDB, the GNU debugger.

   Copyright 2005, 2006, 2007 Free Software Foundation, Inc.

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

#include <stdlib.h>
#include <stdio.h>

long lines = 0;

main()
{
  char linebuf[128];
  FILE *in, *out;
  char *tmp = &linebuf[0];
  long i;
  int c = 0;

  in  = fopen ("pi.txt", "r");
  out = fopen ("copy1.txt", "w");

  if (!in || !out)
    {
      fprintf (stderr, "File open failed\n");
      exit (1);
    }

  for (i = 0; ; i++)
    {
      if (ftell (in) != i)
	fprintf (stderr, "Input error at %d\n", i);
      if (ftell (out) != i)
	fprintf (stderr, "Output error at %d\n", i);
      c = fgetc (in);
      if (c == '\n')
	lines++;	/* breakpoint 1 */
      if (c == EOF)
	break;
      fputc (c, out);
    }
  printf ("Copy complete.\n");	/* breakpoint 2 */
  fclose (in);
  fclose (out);
  printf ("Deleting copy.\n");	/* breakpoint 3 */
  unlink ("copy1.txt");
  exit (0);			/* breakpoint 4 */
}
