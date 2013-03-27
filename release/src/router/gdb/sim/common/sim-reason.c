/* Generic simulator stop_reason.
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
#include "sim-assert.h"

/* Generic implementation of sim_stop_reason */

void
sim_stop_reason (SIM_DESC sd, enum sim_stop *reason, int *sigrc)
{
  sim_engine *engine = NULL;
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  engine = STATE_ENGINE (sd);
  *reason = engine->reason;
  switch (*reason)
    {
    case sim_exited :
      *sigrc = engine->sigrc;
      break;
    case sim_stopped :
    case sim_signalled :
      *sigrc = sim_signal_to_target (sd, engine->sigrc);
      break;
    default :
      abort ();
    }
}
