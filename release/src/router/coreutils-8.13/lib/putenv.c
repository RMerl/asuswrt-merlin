/* Copyright (C) 1991, 1994, 1997-1998, 2000, 2003-2011 Free Software
   Foundation, Inc.

   NOTE: The canonical source of this file is maintained with the GNU C
   Library.  Bugs can be reported to bug-glibc@prep.ai.mit.edu.

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3 of the License, or any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <stdlib.h>

#include <stddef.h>

/* Include errno.h *after* sys/types.h to work around header problems
   on AIX 3.2.5.  */
#include <errno.h>
#ifndef __set_errno
# define __set_errno(ev) ((errno) = (ev))
#endif

#include <string.h>
#include <unistd.h>

#if HAVE_GNU_LD
# define environ __environ
#else
extern char **environ;
#endif

#if _LIBC
/* This lock protects against simultaneous modifications of `environ'.  */
# include <bits/libc-lock.h>
__libc_lock_define_initialized (static, envlock)
# define LOCK   __libc_lock_lock (envlock)
# define UNLOCK __libc_lock_unlock (envlock)
#else
# define LOCK
# define UNLOCK
#endif

static int
_unsetenv (const char *name)
{
  size_t len;
  char **ep;

  if (name == NULL || *name == '\0' || strchr (name, '=') != NULL)
    {
      __set_errno (EINVAL);
      return -1;
    }

  len = strlen (name);

  LOCK;

  ep = environ;
  while (*ep != NULL)
    if (!strncmp (*ep, name, len) && (*ep)[len] == '=')
      {
        /* Found it.  Remove this pointer by moving later ones back.  */
        char **dp = ep;

        do
          dp[0] = dp[1];
        while (*dp++);
        /* Continue the loop in case NAME appears again.  */
      }
    else
      ++ep;

  UNLOCK;

  return 0;
}


/* Put STRING, which is of the form "NAME=VALUE", in the environment.
   If STRING contains no `=', then remove STRING from the environment.  */
int
putenv (char *string)
{
  const char *const name_end = strchr (string, '=');
  register size_t size;
  register char **ep;

  if (name_end == NULL)
    {
      /* Remove the variable from the environment.  */
      return _unsetenv (string);
    }

  size = 0;
  for (ep = environ; *ep != NULL; ++ep)
    if (!strncmp (*ep, string, name_end - string) &&
        (*ep)[name_end - string] == '=')
      break;
    else
      ++size;

  if (*ep == NULL)
    {
      static char **last_environ = NULL;
      char **new_environ = (char **) malloc ((size + 2) * sizeof (char *));
      if (new_environ == NULL)
        return -1;
      (void) memcpy ((void *) new_environ, (void *) environ,
                     size * sizeof (char *));
      new_environ[size] = (char *) string;
      new_environ[size + 1] = NULL;
      free (last_environ);
      last_environ = new_environ;
      environ = new_environ;
    }
  else
    *ep = string;

  return 0;
}
