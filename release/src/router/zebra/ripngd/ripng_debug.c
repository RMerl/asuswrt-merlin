/*
 * RIPng debug output routines
 * Copyright (C) 1998 Kunihiro Ishiguro
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
#include "command.h"
#include "ripngd/ripng_debug.h"

/* For debug statement. */
unsigned long ripng_debug_event = 0;
unsigned long ripng_debug_packet = 0;
unsigned long ripng_debug_zebra = 0;

DEFUN (debug_ripng_events,
       debug_ripng_events_cmd,
       "debug ripng events",
       DEBUG_STR
       "RIPng configuration\n"
       "Debug option set for ripng events\n")
{
  ripng_debug_event = RIPNG_DEBUG_EVENT;
  return CMD_WARNING;
}

DEFUN (debug_ripng_packet,
       debug_ripng_packet_cmd,
       "debug ripng packet",
       DEBUG_STR
       "RIPng configuration\n"
       "Debug option set for ripng packet\n")
{
  ripng_debug_packet = RIPNG_DEBUG_PACKET;
  ripng_debug_packet |= RIPNG_DEBUG_SEND;
  ripng_debug_packet |= RIPNG_DEBUG_RECV;
  return CMD_SUCCESS;
}

DEFUN (debug_ripng_packet_direct,
       debug_ripng_packet_direct_cmd,
       "debug ripng packet (recv|send)",
       DEBUG_STR
       "RIPng configuration\n"
       "Debug option set for ripng packet\n"
       "Debug option set for receive packet\n"
       "Debug option set for send packet\n")
{
  ripng_debug_packet |= RIPNG_DEBUG_PACKET;
  if (strncmp ("send", argv[0], strlen (argv[0])) == 0)
    ripng_debug_packet |= RIPNG_DEBUG_SEND;
  if (strncmp ("recv", argv[0], strlen (argv[0])) == 0)
    ripng_debug_packet |= RIPNG_DEBUG_RECV;
  ripng_debug_packet &= ~RIPNG_DEBUG_DETAIL;
  return CMD_SUCCESS;
}

DEFUN (debug_ripng_packet_detail,
       debug_ripng_packet_detail_cmd,
       "debug ripng packet (recv|send) detail",
       DEBUG_STR
       "RIPng configuration\n"
       "Debug option set for ripng packet\n"
       "Debug option set for receive packet\n"
       "Debug option set for send packet\n"
       "Debug option set detaied information\n")
{
  ripng_debug_packet |= RIPNG_DEBUG_PACKET;
  if (strncmp ("send", argv[0], strlen (argv[0])) == 0)
    ripng_debug_packet |= RIPNG_DEBUG_SEND;
  if (strncmp ("recv", argv[0], strlen (argv[0])) == 0)
    ripng_debug_packet |= RIPNG_DEBUG_RECV;
  ripng_debug_packet |= RIPNG_DEBUG_DETAIL;
  return CMD_SUCCESS;
}

DEFUN (debug_ripng_zebra,
       debug_ripng_zebra_cmd,
       "debug ripng zebra",
       DEBUG_STR
       "RIPng configuration\n"
       "Debug option set for ripng and zebra communication\n")
{
  ripng_debug_zebra = RIPNG_DEBUG_ZEBRA;
  return CMD_WARNING;
}

DEFUN (no_debug_ripng_events,
       no_debug_ripng_events_cmd,
       "no debug ripng events",
       NO_STR
       DEBUG_STR
       "RIPng configuration\n"
       "Debug option set for ripng events\n")
{
  ripng_debug_event = 0;
  return CMD_SUCCESS;
}

DEFUN (no_debug_ripng_packet,
       no_debug_ripng_packet_cmd,
       "no debug ripng packet",
       NO_STR
       DEBUG_STR
       "RIPng configuration\n"
       "Debug option set for ripng packet\n")
{
  ripng_debug_packet = 0;
  return CMD_SUCCESS;
}

DEFUN (no_debug_ripng_packet_direct,
       no_debug_ripng_packet_direct_cmd,
       "no debug ripng packet (recv|send)",
       NO_STR
       DEBUG_STR
       "RIPng configuration\n"
       "Debug option set for ripng packet\n"
       "Debug option set for receive packet\n"
       "Debug option set for send packet\n")
{
  if (strncmp ("send", argv[0], strlen (argv[0])) == 0)
    {
      if (IS_RIPNG_DEBUG_RECV)
       ripng_debug_packet &= ~RIPNG_DEBUG_SEND;
      else
       ripng_debug_packet = 0;
    }
  else if (strncmp ("recv", argv[0], strlen (argv[0])) == 0)
    {
      if (IS_RIPNG_DEBUG_SEND)
       ripng_debug_packet &= ~RIPNG_DEBUG_RECV;
      else
       ripng_debug_packet = 0;
    }
  return CMD_SUCCESS;
}

DEFUN (no_debug_ripng_zebra,
       no_debug_ripng_zebra_cmd,
       "no debug ripng zebra",
       NO_STR
       DEBUG_STR
       "RIPng configuration\n"
       "Debug option set for ripng and zebra communication\n")
{
  ripng_debug_zebra = 0;
  return CMD_WARNING;
}

DEFUN (show_debugging_ripng,
       show_debugging_ripng_cmd,
       "show debugging ripng",
       SHOW_STR
       "RIPng configuration\n"
       "Debugging information\n")
{
  vty_out (vty, "Zebra debugging status:%s", VTY_NEWLINE);

  if (IS_RIPNG_DEBUG_EVENT)
    vty_out (vty, "  RIPng event debugging is on%s", VTY_NEWLINE);

  if (IS_RIPNG_DEBUG_PACKET)
    {
      if (IS_RIPNG_DEBUG_SEND && IS_RIPNG_DEBUG_RECV)
	{
	  vty_out (vty, "  RIPng packet%s debugging is on%s",
		   IS_RIPNG_DEBUG_DETAIL ? " detail" : "",
		   VTY_NEWLINE);
	}
      else
	{
	  if (IS_RIPNG_DEBUG_SEND)
	    vty_out (vty, "  RIPng packet send%s debugging is on%s",
		     IS_RIPNG_DEBUG_DETAIL ? " detail" : "",
		     VTY_NEWLINE);
	  else
	    vty_out (vty, "  RIPng packet receive%s debugging is on%s",
		     IS_RIPNG_DEBUG_DETAIL ? " detail" : "",
		     VTY_NEWLINE);
	}
    }

  if (IS_RIPNG_DEBUG_ZEBRA)
    vty_out (vty, "  RIPng zebra debugging is on%s", VTY_NEWLINE);

  return CMD_SUCCESS;
}

/* Debug node. */
struct cmd_node debug_node =
{
  DEBUG_NODE,
  ""				/* Debug node has no interface. */
};

int
config_write_debug (struct vty *vty)
{
  int write = 0;

  if (IS_RIPNG_DEBUG_EVENT)
    {
      vty_out (vty, "debug ripng events%s", VTY_NEWLINE);
      write++;
    }
  if (IS_RIPNG_DEBUG_PACKET)
    {
      if (IS_RIPNG_DEBUG_SEND && IS_RIPNG_DEBUG_RECV)
	{
	  vty_out (vty, "debug ripng packet%s%s",
		   IS_RIPNG_DEBUG_DETAIL ? " detail" : "",
		   VTY_NEWLINE);
	  write++;
	}
      else
	{
	  if (IS_RIPNG_DEBUG_SEND)
	    vty_out (vty, "debug ripng packet send%s%s",
		     IS_RIPNG_DEBUG_DETAIL ? " detail" : "",
		     VTY_NEWLINE);
	  else
	    vty_out (vty, "debug ripng packet recv%s%s",
		     IS_RIPNG_DEBUG_DETAIL ? " detail" : "",
		     VTY_NEWLINE);
	  write++;
	}
    }
  if (IS_RIPNG_DEBUG_ZEBRA)
    {
      vty_out (vty, "debug ripng zebra%s", VTY_NEWLINE);
      write++;
    }
  return write;
}

void
ripng_debug_reset ()
{
  ripng_debug_event = 0;
  ripng_debug_packet = 0;
  ripng_debug_zebra = 0;
}

void
ripng_debug_init ()
{
  ripng_debug_event = 0;
  ripng_debug_packet = 0;
  ripng_debug_zebra = 0;

  install_node (&debug_node, config_write_debug);

  install_element (ENABLE_NODE, &show_debugging_ripng_cmd);
  install_element (ENABLE_NODE, &debug_ripng_events_cmd);
  install_element (ENABLE_NODE, &debug_ripng_packet_cmd);
  install_element (ENABLE_NODE, &debug_ripng_packet_direct_cmd);
  install_element (ENABLE_NODE, &debug_ripng_packet_detail_cmd);
  install_element (ENABLE_NODE, &debug_ripng_zebra_cmd);
  install_element (ENABLE_NODE, &no_debug_ripng_events_cmd);
  install_element (ENABLE_NODE, &no_debug_ripng_packet_cmd);
  install_element (ENABLE_NODE, &no_debug_ripng_packet_direct_cmd);
  install_element (ENABLE_NODE, &no_debug_ripng_zebra_cmd);

  install_element (CONFIG_NODE, &debug_ripng_events_cmd);
  install_element (CONFIG_NODE, &debug_ripng_packet_cmd);
  install_element (CONFIG_NODE, &debug_ripng_packet_direct_cmd);
  install_element (CONFIG_NODE, &debug_ripng_packet_detail_cmd);
  install_element (CONFIG_NODE, &debug_ripng_zebra_cmd);
  install_element (CONFIG_NODE, &no_debug_ripng_events_cmd);
  install_element (CONFIG_NODE, &no_debug_ripng_packet_cmd);
  install_element (CONFIG_NODE, &no_debug_ripng_packet_direct_cmd);
  install_element (CONFIG_NODE, &no_debug_ripng_zebra_cmd);

  install_element (VIEW_NODE, &show_debugging_ripng_cmd);
}
