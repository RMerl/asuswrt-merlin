/* Create /proc/self/fd-related names for subfiles of open directories.

   Copyright (C) 2006, 2009-2011 Free Software Foundation, Inc.

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

/* Written by Paul Eggert.  */

#include <config.h>

#include "openat-priv.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "intprops.h"

/* The results of open() in this file are not used with fchdir,
   and we do not leak fds to any single-threaded code that could use stdio,
   therefore save some unnecessary work in fchdir.c.
   FIXME - if the kernel ever adds support for multi-thread safety for
   avoiding standard fds, then we should use open_safer.  */
#undef open
#undef close

#define PROC_SELF_FD_FORMAT "/proc/self/fd/%d/%s"

#define PROC_SELF_FD_NAME_SIZE_BOUND(len) \
  (sizeof PROC_SELF_FD_FORMAT - sizeof "%d%s" \
   + INT_STRLEN_BOUND (int) + (len) + 1)


/* Set BUF to the expansion of PROC_SELF_FD_FORMAT, using FD and FILE
   respectively for %d and %s.  If successful, return BUF if the
   result fits in BUF, dynamically allocated memory otherwise.  But
   return NULL if /proc is not reliable, either because the operating
   system support is lacking or because memory is low.  */
char *
openat_proc_name (char buf[OPENAT_BUFFER_SIZE], int fd, char const *file)
{
  static int proc_status = 0;

  /* Make sure the caller gets ENOENT when appropriate.  */
  if (!*file)
    {
      buf[0] = '\0';
      return buf;
    }

  if (! proc_status)
    {
      /* Set PROC_STATUS to a positive value if /proc/self/fd is
         reliable, and a negative value otherwise.  Solaris 10
         /proc/self/fd mishandles "..", and any file name might expand
         to ".." after symbolic link expansion, so avoid /proc/self/fd
         if it mishandles "..".  Solaris 10 has openat, but this
         problem is exhibited on code that built on Solaris 8 and
         running on Solaris 10.  */

      int proc_self_fd = open ("/proc/self/fd",
                               O_SEARCH | O_DIRECTORY | O_NOCTTY | O_NONBLOCK);
      if (proc_self_fd < 0)
        proc_status = -1;
      else
        {
          /* Detect whether /proc/self/fd/%i/../fd exists, where %i is the
             number of a file descriptor open on /proc/self/fd.  On Linux,
             that name resolves to /proc/self/fd, which was opened above.
             However, on Solaris, it may resolve to /proc/self/fd/fd, which
             cannot exist, since all names in /proc/self/fd are numeric.  */
          char dotdot_buf[PROC_SELF_FD_NAME_SIZE_BOUND (sizeof "../fd" - 1)];
          sprintf (dotdot_buf, PROC_SELF_FD_FORMAT, proc_self_fd, "../fd");
          proc_status = access (dotdot_buf, F_OK) ? -1 : 1;
          close (proc_self_fd);
        }
    }

  if (proc_status < 0)
    return NULL;
  else
    {
      size_t bufsize = PROC_SELF_FD_NAME_SIZE_BOUND (strlen (file));
      char *result = buf;
      if (OPENAT_BUFFER_SIZE < bufsize)
        {
          result = malloc (bufsize);
          if (! result)
            return NULL;
        }
      sprintf (result, PROC_SELF_FD_FORMAT, fd, file);
      return result;
    }
}
