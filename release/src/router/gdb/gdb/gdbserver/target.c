/* Target operations for the remote server for GDB.
   Copyright (C) 2002, 2004, 2005, 2007 Free Software Foundation, Inc.

   Contributed by MontaVista Software.

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

#include "server.h"

struct target_ops *the_target;

void
set_desired_inferior (int use_general)
{
  struct thread_info *found;

  if (use_general == 1)
    {
      found = (struct thread_info *) find_inferior_id (&all_threads,
						       general_thread);
    }
  else
    {
      found = NULL;

      /* If we are continuing any (all) thread(s), use step_thread
	 to decide which thread to step and/or send the specified
	 signal to.  */
      if ((step_thread != 0 && step_thread != -1)
	  && (cont_thread == 0 || cont_thread == -1))
	found = (struct thread_info *) find_inferior_id (&all_threads,
							 step_thread);

      if (found == NULL)
	found = (struct thread_info *) find_inferior_id (&all_threads,
							 cont_thread);
    }

  if (found == NULL)
    current_inferior = (struct thread_info *) all_threads.head;
  else
    current_inferior = found;
}

int
read_inferior_memory (CORE_ADDR memaddr, unsigned char *myaddr, int len)
{
  int res;
  res = (*the_target->read_memory) (memaddr, myaddr, len);
  check_mem_read (memaddr, myaddr, len);
  return res;
}

int
write_inferior_memory (CORE_ADDR memaddr, const unsigned char *myaddr,
		       int len)
{
  /* Lacking cleanups, there is some potential for a memory leak if the
     write fails and we go through error().  Make sure that no more than
     one buffer is ever pending by making BUFFER static.  */
  static unsigned char *buffer = 0;
  int res;

  if (buffer != NULL)
    free (buffer);

  buffer = malloc (len);
  memcpy (buffer, myaddr, len);
  check_mem_write (memaddr, buffer, len);
  res = (*the_target->write_memory) (memaddr, buffer, len);
  free (buffer);
  buffer = NULL;

  return res;
}

unsigned char
mywait (char *statusp, int connected_wait)
{
  unsigned char ret;

  if (connected_wait)
    server_waiting = 1;

  ret = (*the_target->wait) (statusp);

  if (connected_wait)
    server_waiting = 0;

  return ret;
}

void
set_target_ops (struct target_ops *target)
{
  the_target = (struct target_ops *) malloc (sizeof (*the_target));
  memcpy (the_target, target, sizeof (*the_target));
}
