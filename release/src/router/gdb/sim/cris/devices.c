/* CRIS device support
   Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.
   Contributed by Axis Communications.

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

/* Based on the i960 devices.c (for the purposes, the same as all the
   others).  */

#include "sim-main.h"

#ifdef HAVE_DV_SOCKSER
#include "dv-sockser.h"
#endif

#include "hw-device.h"

/* Placeholder definition.  */
struct _device { char dummy; } cris_devices;

void
device_error (device *me ATTRIBUTE_UNUSED,
	      char *message ATTRIBUTE_UNUSED,
	      ...)
{
  abort ();
}

int
device_io_read_buffer (device *me ATTRIBUTE_UNUSED,
		       void *source ATTRIBUTE_UNUSED,
		       int space ATTRIBUTE_UNUSED,
		       address_word addr ATTRIBUTE_UNUSED,
		       unsigned nr_bytes ATTRIBUTE_UNUSED,
		       SIM_DESC sd ATTRIBUTE_UNUSED,
		       SIM_CPU *cpu ATTRIBUTE_UNUSED,
		       sim_cia cia ATTRIBUTE_UNUSED)
{
#if WITH_HW
  return hw_io_read_buffer ((struct hw *) me, source, space, addr, nr_bytes);
#else
  abort ();
#endif
}

int
device_io_write_buffer (device *me ATTRIBUTE_UNUSED,
			const void *source,
			int space ATTRIBUTE_UNUSED,
			address_word addr, unsigned nr_bytes,
			SIM_DESC sd, SIM_CPU *cpu, sim_cia cia)
{
  static const unsigned char ok[] = { 4, 0, 0, 0x90};
  static const unsigned char bad[] = { 8, 0, 0, 0x90};

  if (cris_have_900000xxif)
    {
      if (addr == 0x90000004 && memcmp (source, ok, sizeof ok) == 0)
	return cris_break_13_handler (cpu, 1, 0, 0, 0, 0, 0, 0, cia);
      else if (addr == 0x90000008
	       && memcmp (source, bad, sizeof bad) == 0)
	return cris_break_13_handler (cpu, 1, 34, 0, 0, 0, 0, 0, cia);
    }
#if WITH_HW
  else
    return hw_io_write_buffer ((struct hw *) me, source, space, addr, nr_bytes);
#endif

  /* If it wasn't one of those, send an invalid-memory signal.  */
  sim_core_signal (sd, cpu, cia, 0, nr_bytes, addr,
		   write_transfer, sim_core_unmapped_signal);

  return 0;
}
