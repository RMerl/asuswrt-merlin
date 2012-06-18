/*
 * Process id output.
 * Copyright (C) 1998, 1999 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

pid_t
pid_output (char *path)
{
  FILE *fp;
  pid_t pid;

  pid = getpid();

  fp = fopen (path, "w");
  if (fp != NULL) 
    {
      fprintf (fp, "%d\n", (int) pid);
      fclose (fp);
      return -1;
    }
  return pid;
}

pid_t
pid_output_lock (char *path)
{
  int tmp;
  int fd;
  pid_t pid;
  char buf[16], *p;

  pid = getpid ();

  fd = open (path, O_RDWR | O_CREAT | O_EXCL, 0644);
  if (fd < 0)
    {
      fd = open (path, O_RDONLY);
      if (fd < 0)
        fprintf (stderr, "Can't creat pid lock file, exit\n");
      else
        {
          read (fd, buf, sizeof (buf));
          if ((p = index (buf, '\n')) != NULL)
            *p = 0;
          fprintf (stderr, "Another process(%s) running, exit\n", buf);
        }
      exit (-1);
    }
  else
    {
      sprintf (buf, "%d\n", (int) pid);
      tmp = write (fd, buf, strlen (buf));
      close (fd);
    }

  return pid;
}

