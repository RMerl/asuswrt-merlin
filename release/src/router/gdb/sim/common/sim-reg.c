/* Generic register read/write.
   Copyright (C) 1998, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

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

/* Generic implementation of sim_fetch_register for simulators using
   CPU_REG_FETCH.
   The contents of BUF are in target byte order.  */
/* ??? Obviously the interface needs to be extended to handle multiple
   cpus.  */

int
sim_fetch_register (SIM_DESC sd, int rn, unsigned char *buf, int length)
{
  SIM_CPU *cpu = STATE_CPU (sd, 0);

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  return (* CPU_REG_FETCH (cpu)) (cpu, rn, buf, length);
}

/* Generic implementation of sim_store_register for simulators using
   CPU_REG_STORE.
   The contents of BUF are in target byte order.  */
/* ??? Obviously the interface needs to be extended to handle multiple
   cpus.  */

int
sim_store_register (SIM_DESC sd, int rn, unsigned char *buf, int length)
{
  SIM_CPU *cpu = STATE_CPU (sd, 0);

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  return (* CPU_REG_STORE (cpu)) (cpu, rn, buf, length);
}
