/* frv device support
   Copyright (C) 1998, 1999, 2007 Free Software Foundation, Inc.
   Contributed by Red Hat.

This file is part of the GNU simulators.

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

/* ??? All of this is just to get something going.  wip!  */

#include "sim-main.h"

#ifdef HAVE_DV_SOCKSER
#include "dv-sockser.h"
#endif

device frv_devices;

int
device_io_read_buffer (device *me, void *source, int space,
		       address_word addr, unsigned nr_bytes,
		       SIM_DESC sd, SIM_CPU *cpu, sim_cia cia)
{
  if (STATE_ENVIRONMENT (sd) != OPERATING_ENVIRONMENT)
    return nr_bytes;

#ifdef HAVE_DV_SOCKSER
  if (addr == UART_INCHAR_ADDR)
    {
      int c = dv_sockser_read (sd);
      if (c == -1)
	return 0;
      *(char *) source = c;
      return 1;
    }
  if (addr == UART_STATUS_ADDR)
    {
      int status = dv_sockser_status (sd);
      unsigned char *p = source;
      p[0] = 0;
      p[1] = (((status & DV_SOCKSER_INPUT_EMPTY)
#ifdef UART_INPUT_READY0
	       ? UART_INPUT_READY : 0)
#else
	       ? 0 : UART_INPUT_READY)
#endif
	      + ((status & DV_SOCKSER_OUTPUT_EMPTY) ? UART_OUTPUT_READY : 0));
      return 2;
    }
#endif

  return nr_bytes;
}

int
device_io_write_buffer (device *me, const void *source, int space,
			address_word addr, unsigned nr_bytes,
                        SIM_DESC sd, SIM_CPU *cpu, sim_cia cia)
{

#if WITH_SCACHE
  if (addr == MCCR_ADDR)
    {
      if ((*(const char *) source & MCCR_CP) != 0)
	scache_flush (sd);
      return nr_bytes;
    }
#endif

  if (STATE_ENVIRONMENT (sd) != OPERATING_ENVIRONMENT)
    return nr_bytes;

#if HAVE_DV_SOCKSER
  if (addr == UART_OUTCHAR_ADDR)
    {
      int rc = dv_sockser_write (sd, *(char *) source);
      return rc == 1;
    }
#endif

  return nr_bytes;
}

void device_error (device *me, char* message, ...) {}
