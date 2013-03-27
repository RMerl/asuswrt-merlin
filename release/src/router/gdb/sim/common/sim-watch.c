/* Generic simulator watchpoint support.
   Copyright (C) 1997, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

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

#include "sim-main.h"
#include "sim-options.h"

#include "sim-assert.h"

#include <ctype.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

enum {
  OPTION_WATCH_DELETE                      = OPTION_START,

  OPTION_WATCH_INFO,
  OPTION_WATCH_CLOCK,
  OPTION_WATCH_CYCLES,
  OPTION_WATCH_PC,

  OPTION_WATCH_OP,
};


/* Break an option number into its op/int-nr */
static watchpoint_type
option_to_type (SIM_DESC sd,
		int option)
{
  sim_watchpoints *watch = STATE_WATCHPOINTS (sd);
  watchpoint_type type = ((option - OPTION_WATCH_OP)
			  / (watch->nr_interrupts + 1));
  SIM_ASSERT (type >= 0 && type < nr_watchpoint_types);
  return type;
}

static int
option_to_interrupt_nr (SIM_DESC sd,
			int option)
{
  sim_watchpoints *watch = STATE_WATCHPOINTS (sd);
  int interrupt_nr = ((option - OPTION_WATCH_OP)
		      % (watch->nr_interrupts + 1));
  return interrupt_nr;
}

static int
type_to_option (SIM_DESC sd,
		watchpoint_type type,
		int interrupt_nr)
{
  sim_watchpoints *watch = STATE_WATCHPOINTS (sd);
  return ((type * (watch->nr_interrupts + 1))
	  + interrupt_nr
	  + OPTION_WATCH_OP);
}


/* Delete one or more watchpoints.  Fail if no watchpoints were found */

static SIM_RC
do_watchpoint_delete (SIM_DESC sd,
		      int ident,
		      watchpoint_type type)
{
  sim_watchpoints *watch = STATE_WATCHPOINTS (sd);
  sim_watch_point **entry = &watch->points;
  SIM_RC status = SIM_RC_FAIL;
  while ((*entry) != NULL)
    {
      if ((*entry)->ident == ident
	  || (*entry)->type == type)
	{
	  sim_watch_point *dead = (*entry);
	  (*entry) = (*entry)->next;
	  sim_events_deschedule (sd, dead->event);
	  zfree (dead);
	  status = SIM_RC_OK;
	}
      else
	entry = &(*entry)->next;
    }
  return status;
}

static char *
watchpoint_type_to_str (SIM_DESC sd,
			watchpoint_type type)
{
  switch (type)
    {
    case  pc_watchpoint:
      return "pc";
    case clock_watchpoint:
      return "clock";
    case cycles_watchpoint:
      return "cycles";
    case invalid_watchpoint:
    case nr_watchpoint_types:
      return "(invalid-type)";
    }
  return NULL;
}

static char *
interrupt_nr_to_str (SIM_DESC sd,
		     int interrupt_nr)
{
  sim_watchpoints *watch = STATE_WATCHPOINTS (sd);
  if (interrupt_nr < 0)
    return "(invalid-interrupt)";
  else if (interrupt_nr >= watch->nr_interrupts)
    return "breakpoint";
  else
    return watch->interrupt_names[interrupt_nr];
}


static void
do_watchpoint_info (SIM_DESC sd)
{
  sim_watchpoints *watch = STATE_WATCHPOINTS (sd);
  sim_watch_point *point;
  sim_io_printf (sd, "Watchpoints:\n");
  for (point = watch->points; point != NULL; point = point->next)
    {
      sim_io_printf (sd, "%3d: watch %s %s ",
		     point->ident,
		     watchpoint_type_to_str (sd, point->type),
		     interrupt_nr_to_str (sd, point->interrupt_nr));
      if (point->is_periodic)
	sim_io_printf (sd, "+");
      if (!point->is_within)
	sim_io_printf (sd, "!");
      sim_io_printf (sd, "0x%lx", point->arg0);
      if (point->arg1 != point->arg0)
	sim_io_printf (sd, ",0x%lx", point->arg1);
      sim_io_printf (sd, "\n");
    }
}
		    


static sim_event_handler handle_watchpoint;

static SIM_RC
schedule_watchpoint (SIM_DESC sd,
		     sim_watch_point *point)
{
  sim_watchpoints *watch = STATE_WATCHPOINTS (sd);
  switch (point->type)
    {
    case pc_watchpoint:
      point->event = sim_events_watch_sim (sd,
					   watch->pc,
					   watch->sizeof_pc,
					   0/* host-endian */,
					   point->is_within,
					   point->arg0, point->arg1,
					   /* PC in arg0..arg1 */
					   handle_watchpoint,
					   point);
      return SIM_RC_OK;
    case clock_watchpoint:
      point->event = sim_events_watch_clock (sd,
					     point->arg0, /* ms time */
					     handle_watchpoint,
					     point);
      return SIM_RC_OK;
    case cycles_watchpoint:
      point->event = sim_events_schedule (sd,
					  point->arg0, /* time */
					  handle_watchpoint,
					  point);
      return SIM_RC_OK;
    default:
      sim_engine_abort (sd, NULL, NULL_CIA,
			"handle_watchpoint - internal error - bad switch");
      return SIM_RC_FAIL;
    }
  return SIM_RC_OK;
}


static void
handle_watchpoint (SIM_DESC sd, void *data)
{
  sim_watchpoints *watch = STATE_WATCHPOINTS (sd);
  sim_watch_point *point = (sim_watch_point *) data;
  int interrupt_nr = point->interrupt_nr;

  if (point->is_periodic)
    /* reschedule this event before processing it */
    schedule_watchpoint (sd, point);
  else
    do_watchpoint_delete (sd, point->ident, invalid_watchpoint);
    
  if (point->interrupt_nr == watch->nr_interrupts)
    sim_engine_halt (sd, NULL, NULL, NULL_CIA, sim_stopped, SIM_SIGINT);
  else
    watch->interrupt_handler (sd, &watch->interrupt_names[interrupt_nr]);
}


static SIM_RC
do_watchpoint_create (SIM_DESC sd,
		      watchpoint_type type,
		      int opt,
		      char *arg)
{
  sim_watchpoints *watch = STATE_WATCHPOINTS (sd);
  sim_watch_point **point;

  /* create the watchpoint */
  point = &watch->points;
  while ((*point) != NULL)
    point = &(*point)->next;
  (*point) = ZALLOC (sim_watch_point);

  /* fill in the details */
  (*point)->ident = ++(watch->last_point_nr);
  (*point)->type = option_to_type (sd, opt);
  (*point)->interrupt_nr = option_to_interrupt_nr (sd, opt);
  /* prefixes to arg - +== periodic, !==not or outside */
  (*point)->is_within = 1;
  while (1)
    {
      if (arg[0] == '+')
	(*point)->is_periodic = 1;
      else if (arg[0] == '!')
	(*point)->is_within = 0;
      else
	break;
      arg++;
    }
	
  (*point)->arg0 = strtoul (arg, &arg, 0);
  if (arg[0] == ',')
    (*point)->arg0 = strtoul (arg, NULL, 0);
  else
    (*point)->arg1 = (*point)->arg0;

  /* schedule it */
  schedule_watchpoint (sd, (*point));

  return SIM_RC_OK;
}


static SIM_RC
watchpoint_option_handler (SIM_DESC sd, sim_cpu *cpu, int opt,
			   char *arg, int is_command)
{
  if (opt >= OPTION_WATCH_OP)
    return do_watchpoint_create (sd, clock_watchpoint, opt, arg);
  else
    switch (opt)
      {
	
      case OPTION_WATCH_DELETE:
	if (isdigit ((int) arg[0]))
	  {
	    int ident = strtol (arg, NULL, 0);
	    if (do_watchpoint_delete (sd, ident, invalid_watchpoint)
		!= SIM_RC_OK)
	      {
		sim_io_eprintf (sd, "Watchpoint %d not found\n", ident);
		return SIM_RC_FAIL;
	      }
	    return SIM_RC_OK;
	  }
	else if (strcasecmp (arg, "all") == 0)
	  {
	    watchpoint_type type;
	    for (type = invalid_watchpoint + 1;
		 type < nr_watchpoint_types;
		 type++)
	      {
		do_watchpoint_delete (sd, 0, type);
	      }
	    return SIM_RC_OK;
	  }
	else if (strcasecmp (arg, "pc") == 0)
	  {
	    if (do_watchpoint_delete (sd, 0, pc_watchpoint)
		!= SIM_RC_OK)
	      {
		sim_io_eprintf (sd, "No PC watchpoints found\n");
		return SIM_RC_FAIL;
	      }
	    return SIM_RC_OK;
	  }
	else if (strcasecmp (arg, "clock") == 0)
	  {
	    if (do_watchpoint_delete (sd, 0, clock_watchpoint) != SIM_RC_OK)
	      {
		sim_io_eprintf (sd, "No CLOCK watchpoints found\n");
		return SIM_RC_FAIL;
	      }
	    return SIM_RC_OK;
	  }
	else if (strcasecmp (arg, "cycles") == 0)
	  {
	    if (do_watchpoint_delete (sd, 0, cycles_watchpoint) != SIM_RC_OK)
	      {
		sim_io_eprintf (sd, "No CYCLES watchpoints found\n");
		return SIM_RC_FAIL;
	      }
	    return SIM_RC_OK;
	  }
	sim_io_eprintf (sd, "Unknown watchpoint type `%s'\n", arg);
	return SIM_RC_FAIL;
	
      case OPTION_WATCH_INFO:
	{
	  do_watchpoint_info (sd);
	  return SIM_RC_OK;
	}
      
      default:
	sim_io_eprintf (sd, "Unknown watch option %d\n", opt);
	return SIM_RC_FAIL;
	
      }
  
}


static SIM_RC
sim_watchpoint_init (SIM_DESC sd)
{
  sim_watchpoints *watch = STATE_WATCHPOINTS (sd);
  sim_watch_point *point;
  /* NOTE: Do not need to de-schedule any previous watchpoints as
     sim-events has already done this */
  /* schedule any watchpoints enabled by command line options */
  for (point = watch->points; point != NULL; point = point->next)
    {
      schedule_watchpoint (sd, point);
    }
  return SIM_RC_OK;
}


static const OPTION watchpoint_options[] =
{
  { {"watch-delete", required_argument, NULL, OPTION_WATCH_DELETE },
      '\0', "IDENT|all|pc|cycles|clock", "Delete a watchpoint",
      watchpoint_option_handler },

  { {"watch-info", no_argument, NULL, OPTION_WATCH_INFO },
      '\0', NULL, "List scheduled watchpoints",
      watchpoint_option_handler },

  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL }
};

static char *default_interrupt_names[] = { "int", 0, };



SIM_RC
sim_watchpoint_install (SIM_DESC sd)
{
  sim_watchpoints *watch = STATE_WATCHPOINTS (sd);
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  /* the basic command set */
  sim_module_add_init_fn (sd, sim_watchpoint_init);
  sim_add_option_table (sd, NULL, watchpoint_options);
  /* fill in some details */
  if (watch->interrupt_names == NULL)
    watch->interrupt_names = default_interrupt_names;
  watch->nr_interrupts = 0;
  while (watch->interrupt_names[watch->nr_interrupts] != NULL)
    watch->nr_interrupts++;
  /* generate more advansed commands */
  {
    OPTION *int_options = NZALLOC (OPTION, 1 + (watch->nr_interrupts + 1) * nr_watchpoint_types);
    int interrupt_nr;
    for (interrupt_nr = 0; interrupt_nr <= watch->nr_interrupts; interrupt_nr++)
      {
	watchpoint_type type;
	for (type = 0; type < nr_watchpoint_types; type++)
	  {
	    char *name;
	    int nr = interrupt_nr * nr_watchpoint_types + type;
	    OPTION *option = &int_options[nr];
	    asprintf (&name, "watch-%s-%s",
		      watchpoint_type_to_str (sd, type),
		      interrupt_nr_to_str (sd, interrupt_nr));
	    option->opt.name = name;
	    option->opt.has_arg = required_argument;
	    option->opt.val = type_to_option (sd, type, interrupt_nr);
	    option->doc = "";
	    option->doc_name = "";
	    option->handler = watchpoint_option_handler;
	  }
      }
    /* adjust first few entries so that they contain real
       documentation, the first entry includes a list of actions. */
    {
      char *prefix = 
	"Watch the simulator, take ACTION in COUNT cycles (`+' for every COUNT cycles), ACTION is";
      char *doc;
      int len = strlen (prefix) + 1;
      for (interrupt_nr = 0; interrupt_nr <= watch->nr_interrupts; interrupt_nr++)
	len += strlen (interrupt_nr_to_str (sd, interrupt_nr)) + 1;
      doc = NZALLOC (char, len);
      strcpy (doc, prefix);
      for (interrupt_nr = 0; interrupt_nr <= watch->nr_interrupts; interrupt_nr++)
	{
	  strcat (doc, " ");
	  strcat (doc, interrupt_nr_to_str (sd, interrupt_nr));
	}
      int_options[0].doc_name = "watch-cycles-ACTION";
      int_options[0].arg = "[+]COUNT";
      int_options[0].doc = doc;
    }
    int_options[1].doc_name = "watch-pc-ACTION";
    int_options[1].arg = "[!]ADDRESS";
    int_options[1].doc =
      "Watch the PC, take ACTION when matches ADDRESS (in range ADDRESS,ADDRESS), `!' negates test";
    int_options[2].doc_name = "watch-clock-ACTION";
    int_options[2].arg = "[+]MILLISECONDS";
    int_options[2].doc =
      "Watch the clock, take ACTION after MILLISECONDS (`+' for every MILLISECONDS)";

    sim_add_option_table (sd, NULL, int_options);
  }
  return SIM_RC_OK;
}
