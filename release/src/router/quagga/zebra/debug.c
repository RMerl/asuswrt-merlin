/*
 * Zebra debug related function
 * Copyright (C) 1999 Kunihiro Ishiguro
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.  
 */

#include <zebra.h>
#include "command.h"
#include "debug.h"

/* For debug statement. */
unsigned long zebra_debug_event;
unsigned long zebra_debug_packet;
unsigned long zebra_debug_kernel;
unsigned long zebra_debug_rib;
unsigned long zebra_debug_fpm;

DEFUN (show_debugging_zebra,
       show_debugging_zebra_cmd,
       "show debugging zebra",
       SHOW_STR
       "Debugging information\n"
       "Zebra configuration\n")
{
  vty_out (vty, "Zebra debugging status:%s", VTY_NEWLINE);

  if (IS_ZEBRA_DEBUG_EVENT)
    vty_out (vty, "  Zebra event debugging is on%s", VTY_NEWLINE);

  if (IS_ZEBRA_DEBUG_PACKET)
    {
      if (IS_ZEBRA_DEBUG_SEND && IS_ZEBRA_DEBUG_RECV)
	{
	  vty_out (vty, "  Zebra packet%s debugging is on%s",
		   IS_ZEBRA_DEBUG_DETAIL ? " detail" : "",
		   VTY_NEWLINE);
	}
      else
	{
	  if (IS_ZEBRA_DEBUG_SEND)
	    vty_out (vty, "  Zebra packet send%s debugging is on%s",
		     IS_ZEBRA_DEBUG_DETAIL ? " detail" : "",
		     VTY_NEWLINE);
	  else
	    vty_out (vty, "  Zebra packet receive%s debugging is on%s",
		     IS_ZEBRA_DEBUG_DETAIL ? " detail" : "",
		     VTY_NEWLINE);
	}
    }

  if (IS_ZEBRA_DEBUG_KERNEL)
    vty_out (vty, "  Zebra kernel debugging is on%s", VTY_NEWLINE);

  if (IS_ZEBRA_DEBUG_RIB)
    vty_out (vty, "  Zebra RIB debugging is on%s", VTY_NEWLINE);
  if (IS_ZEBRA_DEBUG_RIB_Q)
    vty_out (vty, "  Zebra RIB queue debugging is on%s", VTY_NEWLINE);

  if (IS_ZEBRA_DEBUG_FPM)
    vty_out (vty, "  Zebra FPM debugging is on%s", VTY_NEWLINE);

  return CMD_SUCCESS;
}

DEFUN (debug_zebra_events,
       debug_zebra_events_cmd,
       "debug zebra events",
       DEBUG_STR
       "Zebra configuration\n"
       "Debug option set for zebra events\n")
{
  zebra_debug_event = ZEBRA_DEBUG_EVENT;
  return CMD_WARNING;
}

DEFUN (debug_zebra_packet,
       debug_zebra_packet_cmd,
       "debug zebra packet",
       DEBUG_STR
       "Zebra configuration\n"
       "Debug option set for zebra packet\n")
{
  zebra_debug_packet = ZEBRA_DEBUG_PACKET;
  zebra_debug_packet |= ZEBRA_DEBUG_SEND;
  zebra_debug_packet |= ZEBRA_DEBUG_RECV;
  return CMD_SUCCESS;
}

DEFUN (debug_zebra_packet_direct,
       debug_zebra_packet_direct_cmd,
       "debug zebra packet (recv|send)",
       DEBUG_STR
       "Zebra configuration\n"
       "Debug option set for zebra packet\n"
       "Debug option set for receive packet\n"
       "Debug option set for send packet\n")
{
  zebra_debug_packet = ZEBRA_DEBUG_PACKET;
  if (strncmp ("send", argv[0], strlen (argv[0])) == 0)
    zebra_debug_packet |= ZEBRA_DEBUG_SEND;
  if (strncmp ("recv", argv[0], strlen (argv[0])) == 0)
    zebra_debug_packet |= ZEBRA_DEBUG_RECV;
  zebra_debug_packet &= ~ZEBRA_DEBUG_DETAIL;
  return CMD_SUCCESS;
}

DEFUN (debug_zebra_packet_detail,
       debug_zebra_packet_detail_cmd,
       "debug zebra packet (recv|send) detail",
       DEBUG_STR
       "Zebra configuration\n"
       "Debug option set for zebra packet\n"
       "Debug option set for receive packet\n"
       "Debug option set for send packet\n"
       "Debug option set detailed information\n")
{
  zebra_debug_packet = ZEBRA_DEBUG_PACKET;
  if (strncmp ("send", argv[0], strlen (argv[0])) == 0)
    zebra_debug_packet |= ZEBRA_DEBUG_SEND;
  if (strncmp ("recv", argv[0], strlen (argv[0])) == 0)
    zebra_debug_packet |= ZEBRA_DEBUG_RECV;
  zebra_debug_packet |= ZEBRA_DEBUG_DETAIL;
  return CMD_SUCCESS;
}

DEFUN (debug_zebra_kernel,
       debug_zebra_kernel_cmd,
       "debug zebra kernel",
       DEBUG_STR
       "Zebra configuration\n"
       "Debug option set for zebra between kernel interface\n")
{
  zebra_debug_kernel = ZEBRA_DEBUG_KERNEL;
  return CMD_SUCCESS;
}

DEFUN (debug_zebra_rib,
       debug_zebra_rib_cmd,
       "debug zebra rib",
       DEBUG_STR
       "Zebra configuration\n"
       "Debug RIB events\n")
{
  SET_FLAG (zebra_debug_rib, ZEBRA_DEBUG_RIB);
  return CMD_SUCCESS;
}

DEFUN (debug_zebra_rib_q,
       debug_zebra_rib_q_cmd,
       "debug zebra rib queue",
       DEBUG_STR
       "Zebra configuration\n"
       "Debug RIB events\n"
       "Debug RIB queueing\n")
{
  SET_FLAG (zebra_debug_rib, ZEBRA_DEBUG_RIB_Q);
  return CMD_SUCCESS;
}

DEFUN (debug_zebra_fpm,
       debug_zebra_fpm_cmd,
       "debug zebra fpm",
       DEBUG_STR
       "Zebra configuration\n"
       "Debug zebra FPM events\n")
{
  SET_FLAG (zebra_debug_fpm, ZEBRA_DEBUG_FPM);
  return CMD_SUCCESS;
}

DEFUN (no_debug_zebra_events,
       no_debug_zebra_events_cmd,
       "no debug zebra events",
       NO_STR
       DEBUG_STR
       "Zebra configuration\n"
       "Debug option set for zebra events\n")
{
  zebra_debug_event = 0;
  return CMD_SUCCESS;
}

DEFUN (no_debug_zebra_packet,
       no_debug_zebra_packet_cmd,
       "no debug zebra packet",
       NO_STR
       DEBUG_STR
       "Zebra configuration\n"
       "Debug option set for zebra packet\n")
{
  zebra_debug_packet = 0;
  return CMD_SUCCESS;
}

DEFUN (no_debug_zebra_packet_direct,
       no_debug_zebra_packet_direct_cmd,
       "no debug zebra packet (recv|send)",
       NO_STR
       DEBUG_STR
       "Zebra configuration\n"
       "Debug option set for zebra packet\n"
       "Debug option set for receive packet\n"
       "Debug option set for send packet\n")
{
  if (strncmp ("send", argv[0], strlen (argv[0])) == 0)
    zebra_debug_packet &= ~ZEBRA_DEBUG_SEND;
  if (strncmp ("recv", argv[0], strlen (argv[0])) == 0)
    zebra_debug_packet &= ~ZEBRA_DEBUG_RECV;
  return CMD_SUCCESS;
}

DEFUN (no_debug_zebra_kernel,
       no_debug_zebra_kernel_cmd,
       "no debug zebra kernel",
       NO_STR
       DEBUG_STR
       "Zebra configuration\n"
       "Debug option set for zebra between kernel interface\n")
{
  zebra_debug_kernel = 0;
  return CMD_SUCCESS;
}

DEFUN (no_debug_zebra_rib,
       no_debug_zebra_rib_cmd,
       "no debug zebra rib",
       NO_STR
       DEBUG_STR
       "Zebra configuration\n"
       "Debug zebra RIB\n")
{
  zebra_debug_rib = 0;
  return CMD_SUCCESS;
}

DEFUN (no_debug_zebra_rib_q,
       no_debug_zebra_rib_q_cmd,
       "no debug zebra rib queue",
       NO_STR
       DEBUG_STR
       "Zebra configuration\n"
       "Debug zebra RIB\n"
       "Debug RIB queueing\n")
{
  UNSET_FLAG (zebra_debug_rib, ZEBRA_DEBUG_RIB_Q);
  return CMD_SUCCESS;
}

DEFUN (no_debug_zebra_fpm,
       no_debug_zebra_fpm_cmd,
       "no debug zebra fpm",
       NO_STR
       DEBUG_STR
       "Zebra configuration\n"
       "Debug zebra FPM events\n")
{
  zebra_debug_fpm = 0;
  return CMD_SUCCESS;
}

/* Debug node. */
struct cmd_node debug_node =
{
  DEBUG_NODE,
  "",				/* Debug node has no interface. */
  1
};

static int
config_write_debug (struct vty *vty)
{
  int write = 0;

  if (IS_ZEBRA_DEBUG_EVENT)
    {
      vty_out (vty, "debug zebra events%s", VTY_NEWLINE);
      write++;
    }
  if (IS_ZEBRA_DEBUG_PACKET)
    {
      if (IS_ZEBRA_DEBUG_SEND && IS_ZEBRA_DEBUG_RECV)
	{
	  vty_out (vty, "debug zebra packet%s%s",
		   IS_ZEBRA_DEBUG_DETAIL ? " detail" : "",
		   VTY_NEWLINE);
	  write++;
	}
      else
	{
	  if (IS_ZEBRA_DEBUG_SEND)
	    vty_out (vty, "debug zebra packet send%s%s",
		     IS_ZEBRA_DEBUG_DETAIL ? " detail" : "",
		     VTY_NEWLINE);
	  else
	    vty_out (vty, "debug zebra packet recv%s%s",
		     IS_ZEBRA_DEBUG_DETAIL ? " detail" : "",
		     VTY_NEWLINE);
	  write++;
	}
    }
  if (IS_ZEBRA_DEBUG_KERNEL)
    {
      vty_out (vty, "debug zebra kernel%s", VTY_NEWLINE);
      write++;
    }
  if (IS_ZEBRA_DEBUG_RIB)
    {
      vty_out (vty, "debug zebra rib%s", VTY_NEWLINE);
      write++;
    }
  if (IS_ZEBRA_DEBUG_RIB_Q)
    {
      vty_out (vty, "debug zebra rib queue%s", VTY_NEWLINE);
      write++;
    }
  if (IS_ZEBRA_DEBUG_FPM)
    {
      vty_out (vty, "debug zebra fpm%s", VTY_NEWLINE);
      write++;
    }
  return write;
}

void
zebra_debug_init (void)
{
  zebra_debug_event = 0;
  zebra_debug_packet = 0;
  zebra_debug_kernel = 0;
  zebra_debug_rib = 0;
  zebra_debug_fpm = 0;

  install_node (&debug_node, config_write_debug);

  install_element (VIEW_NODE, &show_debugging_zebra_cmd);

  install_element (ENABLE_NODE, &show_debugging_zebra_cmd);
  install_element (ENABLE_NODE, &debug_zebra_events_cmd);
  install_element (ENABLE_NODE, &debug_zebra_packet_cmd);
  install_element (ENABLE_NODE, &debug_zebra_packet_direct_cmd);
  install_element (ENABLE_NODE, &debug_zebra_packet_detail_cmd);
  install_element (ENABLE_NODE, &debug_zebra_kernel_cmd);
  install_element (ENABLE_NODE, &debug_zebra_rib_cmd);
  install_element (ENABLE_NODE, &debug_zebra_rib_q_cmd);
  install_element (ENABLE_NODE, &debug_zebra_fpm_cmd);
  install_element (ENABLE_NODE, &no_debug_zebra_events_cmd);
  install_element (ENABLE_NODE, &no_debug_zebra_packet_cmd);
  install_element (ENABLE_NODE, &no_debug_zebra_kernel_cmd);
  install_element (ENABLE_NODE, &no_debug_zebra_rib_cmd);
  install_element (ENABLE_NODE, &no_debug_zebra_rib_q_cmd);
  install_element (ENABLE_NODE, &no_debug_zebra_fpm_cmd);

  install_element (CONFIG_NODE, &debug_zebra_events_cmd);
  install_element (CONFIG_NODE, &debug_zebra_packet_cmd);
  install_element (CONFIG_NODE, &debug_zebra_packet_direct_cmd);
  install_element (CONFIG_NODE, &debug_zebra_packet_detail_cmd);
  install_element (CONFIG_NODE, &debug_zebra_kernel_cmd);
  install_element (CONFIG_NODE, &debug_zebra_rib_cmd);
  install_element (CONFIG_NODE, &debug_zebra_rib_q_cmd);
  install_element (CONFIG_NODE, &debug_zebra_fpm_cmd);
  install_element (CONFIG_NODE, &no_debug_zebra_events_cmd);
  install_element (CONFIG_NODE, &no_debug_zebra_packet_cmd);
  install_element (CONFIG_NODE, &no_debug_zebra_kernel_cmd);
  install_element (CONFIG_NODE, &no_debug_zebra_rib_cmd);
  install_element (CONFIG_NODE, &no_debug_zebra_rib_q_cmd);
  install_element (CONFIG_NODE, &no_debug_zebra_fpm_cmd);
}
