/* Native-dependent code for NetBSD.

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

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

#include "defs.h"

#include <sys/param.h>

#include "nbsd-nat.h"

/* Return a the name of file that can be opened to get the symbols for
   the child process identified by PID.  */

char *
nbsd_pid_to_exec_file (int pid)
{
  size_t len = MAXPATHLEN;
  char *buf = xcalloc (len, sizeof (char));
  char *path;

  path = xstrprintf ("/proc/%d/exe", pid);
  if (readlink (path, buf, MAXPATHLEN) == -1)
    {
      xfree (buf);
      buf = NULL;
    }

  xfree (path);
  return buf;
}
