/* euidaccess-stat -- check if effective user id can access lstat'd file
   This function is probably useful only for choosing whether to issue
   a prompt in an implementation of POSIX-specified rm.

   Copyright (C) 2005-2006, 2009-2011 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Adapted for use in GNU remove.c by Jim Meyering.  */

#include <config.h>

#include "euidaccess-stat.h"

#include <unistd.h>

#include "stat-macros.h"

/* Return true if the current user has permission of type MODE
   on the file from which stat buffer *ST was obtained, ignoring
   ACLs, attributes, `read-only'ness, etc...
   Otherwise, return false.

   Like the reentrant version of euidaccess, but starting with
   a stat buffer rather than a file name.  Hence, this function
   never calls access or accessx, and doesn't take into account
   whether the file has ACLs or other attributes, or resides on
   a read-only file system.  */

bool
euidaccess_stat (struct stat const *st, int mode)
{
  uid_t euid;
  unsigned int granted;

  /* Convert the mode to traditional form, clearing any bogus bits.  */
  if (R_OK == 4 && W_OK == 2 && X_OK == 1 && F_OK == 0)
    mode &= 7;
  else
    mode = ((mode & R_OK ? 4 : 0)
            + (mode & W_OK ? 2 : 0)
            + (mode & X_OK ? 1 : 0));

  if (mode == 0)
    return true;		/* The file exists.  */

  euid = geteuid ();

  /* The super-user can read and write any file, and execute any file
     that anyone can execute.  */
  if (euid == 0 && ((mode & X_OK) == 0
                    || (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))))
    return true;

  /* Convert the file's permission bits to traditional form.  */
  if (   S_IRUSR == (4 << 6)
      && S_IWUSR == (2 << 6)
      && S_IXUSR == (1 << 6)
      && S_IRGRP == (4 << 3)
      && S_IWGRP == (2 << 3)
      && S_IXGRP == (1 << 3)
      && S_IROTH == (4 << 0)
      && S_IWOTH == (2 << 0)
      && S_IXOTH == (1 << 0))
    granted = st->st_mode;
  else
    granted = (  (st->st_mode & S_IRUSR ? 4 << 6 : 0)
               + (st->st_mode & S_IWUSR ? 2 << 6 : 0)
               + (st->st_mode & S_IXUSR ? 1 << 6 : 0)
               + (st->st_mode & S_IRGRP ? 4 << 3 : 0)
               + (st->st_mode & S_IWGRP ? 2 << 3 : 0)
               + (st->st_mode & S_IXGRP ? 1 << 3 : 0)
               + (st->st_mode & S_IROTH ? 4 << 0 : 0)
               + (st->st_mode & S_IWOTH ? 2 << 0 : 0)
               + (st->st_mode & S_IXOTH ? 1 << 0 : 0));

  if (euid == st->st_uid)
    granted >>= 6;
  else
    {
      gid_t egid = getegid ();
      if (egid == st->st_gid || group_member (st->st_gid))
        granted >>= 3;
    }

  if ((mode & ~granted) == 0)
    return true;

  return false;
}


#ifdef TEST
# include <errno.h>
# include <stdio.h>
# include <stdlib.h>

# include "error.h"
# define _(msg) msg

char *program_name;

int
main (int argc, char **argv)
{
  char *file;
  int mode;
  bool ok;
  struct stat st;

  program_name = argv[0];
  if (argc < 3)
    abort ();
  file = argv[1];
  mode = atoi (argv[2]);
  if (lstat (file, &st) != 0)
    error (EXIT_FAILURE, errno, _("cannot stat %s"), file);

  ok = euidaccess_stat (&st, mode);
  printf ("%s: %s\n", file, ok ? "y" : "n");
  return 0;
}
#endif
