/* -*- buffer-read-only: t -*- vi: set ro: */
/* DO NOT EDIT! GENERATED AUTOMATICALLY! */
/* Test of binary mode I/O.
   Copyright (C) 2005, 2007-2011 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible <bruno@clisp.org>, 2005.  */

#include <config.h>

#include "binary-io.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "macros.h"

int
main ()
{
  /* Test the O_BINARY macro.  */
  {
    int fd =
      open ("t-bin-out2.tmp", O_CREAT | O_TRUNC | O_RDWR | O_BINARY, 0600);
    if (write (fd, "Hello\n", 6) < 0)
      exit (1);
    close (fd);
  }
  {
    struct stat statbuf;
    if (stat ("t-bin-out2.tmp", &statbuf) < 0)
      exit (1);
    ASSERT (statbuf.st_size == 6);
  }
  unlink ("t-bin-out2.tmp");

  /* Test the SET_BINARY macro.  */
  SET_BINARY (1);
  fputs ("Hello\n", stdout);
  fclose (stdout);
  fclose (stderr);
  {
    struct stat statbuf;
    if (stat ("t-bin-out1.tmp", &statbuf) < 0)
      exit (1);
    ASSERT (statbuf.st_size == 6);
  }

  return 0;
}
