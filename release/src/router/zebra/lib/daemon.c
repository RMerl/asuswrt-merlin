/*
 * Daemonize routine
 * Copyright (C) 1997, 1999 Kunihiro Ishiguro
 * 
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <zebra.h>

#ifndef HAVE_DAEMON

/* Daemonize myself. */
int
daemon (int nochdir, int noclose)
{
  pid_t pid;

  pid = fork ();

  /* In case of fork is error. */
  if (pid < 0)
    {
      perror ("fork");
      return -1;
    }

  /* In case of this is parent process. */
  if (pid != 0)
    exit (0);

  /* Become session leader and get pid. */
  pid = setsid();

  if (pid < -1)
    {
      perror ("setsid");
      return -1;
    }

  /* Change directory to root. */
  if (! nochdir)
    chdir ("/");

  /* File descriptor close. */
  if (! noclose)
    {
      int fd;

      fd = open ("/dev/null", O_RDWR, 0);
      if (fd != -1)
	{
	  dup2 (fd, STDIN_FILENO);
	  dup2 (fd, STDOUT_FILENO);
	  dup2 (fd, STDERR_FILENO);
	  if (fd > 2)
	    close (fd);
	}
    }

  umask (0027);

  return 0;
}

#endif /* HAVE_DAEMON */
