/* Copyright 1992, 1993, 1994, 1995, 1996, 1999, 2007
   Free Software Foundation, Inc.

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

/* Simple little program that just generates a core dump from inside some
   nested function calls. */

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef __STDC__
#define	const	/**/
#endif

#define MAPSIZE (8 * 1024)

/* Don't make these automatic vars or we will have to walk back up the
   stack to access them. */

char *buf1;
char *buf2;

int coremaker_data = 1;	/* In Data section */
int coremaker_bss;	/* In BSS section */

const int coremaker_ro = 201;	/* In Read-Only Data section */

/* Note that if the mapping fails for any reason, we set buf2
   to -1 and the testsuite notices this and reports it as
   a failure due to a mapping error.  This way we don't have
   to test for specific errors when running the core maker. */

void
mmapdata ()
{
  int j, fd;

  /* Allocate and initialize a buffer that will be used to write
     the file that is later mapped in. */

  buf1 = (char *) malloc (MAPSIZE);
  for (j = 0; j < MAPSIZE; ++j)
    {
      buf1[j] = j;
    }

  /* Write the file to map in */

  fd = open ("coremmap.data", O_CREAT | O_RDWR, 0666);
  if (fd == -1)
    {
      perror ("coremmap.data open failed");
      buf2 = (char *) -1;
      return;
    }
  write (fd, buf1, MAPSIZE);

  /* Now map the file into our address space as buf2 */

  buf2 = (char *) mmap (0, MAPSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (buf2 == (char *) -1)
    {
      perror ("mmap failed");
      return;
    }

  /* Verify that the original data and the mapped data are identical.
     If not, we'd rather fail now than when trying to access the mapped
     data from the core file. */

  for (j = 0; j < MAPSIZE; ++j)
    {
      if (buf1[j] != buf2[j])
	{
	  fprintf (stderr, "mapped data is incorrect");
	  buf2 = (char *) -1;
	  return;
	}
    }
}

void
func2 ()
{
  int coremaker_local[5];
  int i;

#ifdef SA_FULLDUMP
  /* Force a corefile that includes the data section for AIX.  */
  {
    struct sigaction sa;

    sigaction (SIGABRT, (struct sigaction *)0, &sa);
    sa.sa_flags |= SA_FULLDUMP;
    sigaction (SIGABRT, &sa, (struct sigaction *)0);
  }
#endif

  /* Make sure that coremaker_local doesn't get optimized away. */
  for (i = 0; i < 5; i++)
    coremaker_local[i] = i;
  coremaker_bss = 0;
  for (i = 0; i < 5; i++)
    coremaker_bss += coremaker_local[i];
  coremaker_data = coremaker_ro + 1;
  abort ();
}

void
func1 ()
{
  func2 ();
}

int main ()
{
  mmapdata ();
  func1 ();
  return 0;
}

