/* Hardware ports.
   Copyright (C) 1998, 2007 Free Software Foundation, Inc.
   Contributed by Andrew Cagney and Cygnus Solutions.

This file is part of GDB, the GNU debugger.

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


#include "hw-main.h"
#include "hw-base.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <ctype.h>

#define TRACE(x,y)


struct hw_port_edge {
  int my_port;
  struct hw *dest;
  int dest_port;
  struct hw_port_edge *next;
  object_disposition disposition;
};

struct hw_port_data {
  hw_port_event_method *to_port_event;
  const struct hw_port_descriptor *ports;
  struct hw_port_edge *edges;
};

const struct hw_port_descriptor empty_hw_ports[] = {
  { NULL, },
};

static void
panic_hw_port_event (struct hw *me,
		     int my_port,
		     struct hw *source,
		     int source_port,
		     int level)
{
  hw_abort (me, "no port method");
}

void
create_hw_port_data (struct hw *me)
{
  me->ports_of_hw = HW_ZALLOC (me, struct hw_port_data);
  set_hw_port_event (me, panic_hw_port_event);
  set_hw_ports (me, empty_hw_ports);
}

void
delete_hw_port_data (struct hw *me)
{
  hw_free (me, me->ports_of_hw);
  me->ports_of_hw = NULL;
}

void
set_hw_ports (struct hw *me,
	      const struct hw_port_descriptor ports[])
{
  me->ports_of_hw->ports = ports;
}

void
set_hw_port_event (struct hw *me,
		   hw_port_event_method *port_event)
{
  me->ports_of_hw->to_port_event = port_event;
}


static void
attach_hw_port_edge (struct hw *me,
		     struct hw_port_edge **list,
		     int my_port,
		     struct hw *dest,
		     int dest_port,
		     object_disposition disposition)
{
  struct hw_port_edge *new_edge = HW_ZALLOC (me, struct hw_port_edge);
  new_edge->my_port = my_port;
  new_edge->dest = dest;
  new_edge->dest_port = dest_port;
  new_edge->next = *list;
  new_edge->disposition = disposition;
  *list = new_edge;
}


static void
detach_hw_port_edge (struct hw *me,
		     struct hw_port_edge **list,
		     int my_port,
		     struct hw *dest,
		     int dest_port)
{
  while (*list != NULL)
    {
      struct hw_port_edge *old_edge = *list;
      if (old_edge->dest == dest
	  && old_edge->dest_port == dest_port
	  && old_edge->my_port == my_port)
	{
	  if (old_edge->disposition == permenant_object)
	    hw_abort (me, "attempt to delete permenant port edge");
	  *list = old_edge->next;
	  hw_free (me, old_edge);
	  return;
	}
    }
  hw_abort (me, "attempt to delete unattached port");
}


#if 0
static void
clean_hw_port_edges (struct hw_port_edge **list)
{
  while (*list != NULL)
    {
      struct hw_port_edge *old_edge = *list;
      switch (old_edge->disposition)
	{
	case permenant_object:
	  list = &old_edge->next;
	  break;
	case temporary_object:
	  *list = old_edge->next;
	  hw_free (me, old_edge);
	  break;
	}
    }
}
#endif


/* Ports: */

void
hw_port_event (struct hw *me,
	       int my_port,
	       int level)
{
  int found_an_edge = 0;
  struct hw_port_edge *edge;
  /* device's lines directly connected */
  for (edge = me->ports_of_hw->edges;
       edge != NULL;
       edge = edge->next)
    {
      if (edge->my_port == my_port)
	{
	  edge->dest->ports_of_hw->to_port_event (edge->dest,
						  edge->dest_port,
						  me,
						  my_port,
						  level);
	  found_an_edge = 1;
	}
    }
  if (!found_an_edge)
    hw_abort (me, "No edge for port %d", my_port);
}


void
hw_port_attach (struct hw *me,
		int my_port,
		struct hw *dest,
		int dest_port,
		object_disposition disposition)
{
  attach_hw_port_edge (me,
		       &me->ports_of_hw->edges,
		       my_port,
		       dest,
		       dest_port,
		       disposition);
}


void
hw_port_detach (struct hw *me,
		int my_port,
		struct hw *dest,
		int dest_port)
{
  detach_hw_port_edge (me,
		       &me->ports_of_hw->edges,
		       my_port,
		       dest,
		       dest_port);
}


void
hw_port_traverse (struct hw *me,
		  hw_port_traverse_function *handler,
		  void *data)
{
  struct hw_port_edge *port_edge;
  for (port_edge = me->ports_of_hw->edges;
       port_edge != NULL;
       port_edge = port_edge->next)
    {
      handler (me, port_edge->my_port,
	       port_edge->dest, port_edge->dest_port,
	       data);
    }
}


int
hw_port_decode (struct hw *me,
		const char *port_name,
		port_direction direction)
{
  if (port_name == NULL || port_name[0] == '\0')
    return 0;
  if (isdigit(port_name[0]))
    {
      return strtoul (port_name, NULL, 0);
    }
  else
    {
      const struct hw_port_descriptor *ports =
	me->ports_of_hw->ports;
      if (ports != NULL) 
	{
	  while (ports->name != NULL)
	    {
	      if (ports->direction == bidirect_port
		  || ports->direction == direction)
		{
		  if (ports->nr_ports > 0)
		    {
		      int len = strlen (ports->name);
		      if (strncmp (port_name, ports->name, len) == 0)
			{
			  if (port_name[len] == '\0')
			    return ports->number;
			  else if(isdigit (port_name[len]))
			    {
			      int port = (ports->number
					  + strtoul (&port_name[len], NULL, 0));
			      if (port >= ports->number + ports->nr_ports)
				hw_abort (me,
					  "Port %s out of range",
					  port_name);
			      return port;
			    }
			}
		    }
		  else if (strcmp (port_name, ports->name) == 0)
		    return ports->number;
		}
	      ports++;
	    }
	}
    }
  hw_abort (me, "Unreconized port %s", port_name);
  return 0;
}


int
hw_port_encode (struct hw *me,
		int port_number,
		char *buf,
		int sizeof_buf,
		port_direction direction)
{
  const struct hw_port_descriptor *ports = NULL;
  ports = me->ports_of_hw->ports;
  if (ports != NULL) {
    while (ports->name != NULL)
      {
	if (ports->direction == bidirect_port
	    || ports->direction == direction)
	  {
	    if (ports->nr_ports > 0)
	      {
		if (port_number >= ports->number
		    && port_number < ports->number + ports->nr_ports)
		  {
		    strcpy (buf, ports->name);
		    sprintf (buf + strlen(buf), "%d", port_number - ports->number);
		    if (strlen (buf) >= sizeof_buf)
		      hw_abort (me, "hw_port_encode: buffer overflow");
		    return strlen (buf);
		  }
	      }
	    else
	      {
		if (ports->number == port_number)
		  {
		    if (strlen(ports->name) >= sizeof_buf)
		      hw_abort (me, "hw_port_encode: buffer overflow");
		    strcpy(buf, ports->name);
		    return strlen(buf);
		  }
	      }
	  }
	ports++;
      }
  }
  sprintf (buf, "%d", port_number);
  if (strlen(buf) >= sizeof_buf)
    hw_abort (me, "hw_port_encode: buffer overflow");
  return strlen(buf);
}
