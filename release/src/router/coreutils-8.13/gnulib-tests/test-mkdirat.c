/* -*- buffer-read-only: t -*- vi: set ro: */
/* DO NOT EDIT! GENERATED AUTOMATICALLY! */
/* Tests of mkdirat.
   Copyright (C) 2009-2011 Free Software Foundation, Inc.

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

/* Written by Eric Blake <ebb9@byu.net>, 2009.  */

#include <config.h>

#include <sys/stat.h>

#include "signature.h"
SIGNATURE_CHECK (mkdirat, int, (int, char const *, mode_t));

#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "progname.h"
#include "ignore-value.h"
#include "macros.h"

#define BASE "test-mkdirat.t"

#include "test-mkdir.h"

static int dfd = AT_FDCWD;

/* Wrapper to test mkdirat like mkdir.  */
static int
do_mkdir (char const *name, mode_t mode)
{
  return mkdirat (dfd, name, mode);
}

int
main (int argc _GL_UNUSED, char *argv[])
{
  int result;

  set_program_name (argv[0]);

  /* Clean up any trash from prior testsuite runs.  */
  ignore_value (system ("rm -rf " BASE "*"));

  /* Test basic mkdir functionality.  */
  result = test_mkdir (do_mkdir, false);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_mkdir (do_mkdir, false) == result);

  /* Tests specific to mkdirat.  */
  ASSERT (mkdirat (dfd, BASE "dir1", 0700) == 0);
  ASSERT (chdir (BASE "dir1") == 0);
  ASSERT (close (dfd) == 0);
  dfd = open ("..", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (mkdirat (dfd, BASE "dir2", 0700) == 0);
  ASSERT (close (dfd) == 0);
  errno = 0;
  ASSERT (mkdirat (dfd, BASE "dir3", 0700) == -1);
  ASSERT (errno == EBADF);
  dfd = open ("/dev/null", O_RDONLY);
  ASSERT (0 <= dfd);
  errno = 0;
  ASSERT (mkdirat (dfd, "dir3", 0700) == -1);
  ASSERT (errno == ENOTDIR);
  ASSERT (close (dfd) == 0);
  ASSERT (chdir ("..") == 0);
  ASSERT (rmdir (BASE "dir1") == 0);
  ASSERT (rmdir (BASE "dir2") == 0);

  return result;
}
