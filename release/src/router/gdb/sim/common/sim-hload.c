/* Generic load for hardware simulator models.
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
#include "bfd.h"
#include "sim-utils.h"
#include "sim-assert.h"


/* Generic implementation of sim_load that works with simulators
   modeling a hardware platform. */

SIM_RC
sim_load (sd, prog_name, prog_bfd, from_tty)
     SIM_DESC sd;
     char *prog_name;
     struct bfd *prog_bfd;
     int from_tty;
{
  bfd *result_bfd;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  if (sim_analyze_program (sd, prog_name, prog_bfd) != SIM_RC_OK)
    return SIM_RC_FAIL;
  SIM_ASSERT (STATE_PROG_BFD (sd) != NULL);

  /* NOTE: For historical reasons, older hardware simulators
     incorrectly write the program sections at LMA interpreted as a
     virtual address.  This is still accommodated for backward
     compatibility reasons. */

  result_bfd = sim_load_file (sd, STATE_MY_NAME (sd),
			      STATE_CALLBACK (sd),
			      prog_name,
			      STATE_PROG_BFD (sd),
			      STATE_OPEN_KIND (sd) == SIM_OPEN_DEBUG,
			      STATE_LOAD_AT_LMA_P (sd),
			      sim_write);
  if (result_bfd == NULL)
    {
      bfd_close (STATE_PROG_BFD (sd));
      STATE_PROG_BFD (sd) = NULL;
      return SIM_RC_FAIL;
    }
  return SIM_RC_OK;
}
