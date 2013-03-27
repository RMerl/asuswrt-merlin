/* This file defines the part of the interface between the standalone
   simaulator program - run - and simulator library - libsim.a - that
   is not used by GDB.  The GDB part is described in include/remote-sim.h.
   
   Copyright 2002, 2007 Free Software Foundation, Inc.

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

#ifndef RUN_SIM_H
#define RUN_SIM_H

#ifdef SIM_TARGET_SWITCHES
  /* Parse the command line, extracting any target specific switches
     before the generic simulator code gets a chance to complain
     about them.  Returns the adjusted value of argc.  */
int sim_target_parse_command_line PARAMS ((int, char **));

  /* Display a list of target specific switches supported by this
     target.  */
void sim_target_display_usage PARAMS ((void));

#endif

/* Provide simulator with a default (global) host_callback_struct.
   THIS PROCEDURE IS DEPRECATED.
   GDB and NRUN do not use this interface.
   This procedure does not take a SIM_DESC argument as it is
   used before sim_open. */

void sim_set_callbacks PARAMS ((struct host_callback_struct *));


/* Set the size of the simulator memory array.
   THIS PROCEDURE IS DEPRECATED.
   GDB and NRUN do not use this interface.
   This procedure does not take a SIM_DESC argument as it is
   used before sim_open. */

void sim_size PARAMS ((int i));


/* Single-step simulator with tracing enabled.
   THIS PROCEDURE IS DEPRECATED.
   THIS PROCEDURE IS EVEN MORE DEPRECATED THAN SIM_SET_TRACE
   GDB and NRUN do not use this interface.
   This procedure returns: ``0'' indicating that the simulator should
   be continued using sim_trace() calls; ``1'' indicating that the
   simulation has finished. */

int sim_trace PARAMS ((SIM_DESC sd));


/* Enable tracing.
   THIS PROCEDURE IS DEPRECATED.
   GDB and NRUN do not use this interface.
   This procedure returns: ``0'' indicating that the simulator should
   be continued using sim_trace() calls; ``1'' indicating that the
   simulation has finished. */

void sim_set_trace PARAMS ((void));


/* Configure the size of the profile buffer.
   THIS PROCEDURE IS DEPRECATED.
   GDB and NRUN do not use this interface.
   This procedure does not take a SIM_DESC argument as it is
   used before sim_open. */

void sim_set_profile_size PARAMS ((int n));


/* Kill the running program.
   THIS PROCEDURE IS DEPRECATED.
   GDB and NRUN do not use this interface.
   This procedure will be replaced as part of the introduction of
   multi-cpu simulators. */

void sim_kill PARAMS ((SIM_DESC sd));

#endif
