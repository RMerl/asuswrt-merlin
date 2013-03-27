/*  This file is part of the program psim.

    Copyright (C) 1994-1996, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _HW_NVRAM_C_
#define _HW_NVRAM_C_

#ifndef STATIC_INLINE_HW_NVRAM
#define STATIC_INLINE_HW_NVRAM STATIC_INLINE
#endif

#include "device_table.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

/* DEVICE


   nvram - non-volatile memory with clock


   DESCRIPTION


   This device implements a small byte addressable non-volatile
   memory.  The top 8 bytes of this memory include a real-time clock.


   PROPERTIES


   reg = <address> <size> (required)

   Specify the address/size of this device within its parents address
   space.


   timezone = <integer> (optional)

   Adjustment to the hosts current GMT (in seconds) that should be
   applied when updating the NVRAM's clock.  If no timezone is
   specified, zero (GMT or UCT) is assumed.


   */

typedef struct _hw_nvram_device {
  unsigned8 *memory;
  unsigned sizeof_memory;
#ifdef HAVE_TIME_H
  time_t host_time;
#else
  long host_time;
#endif
  unsigned timezone;
  /* useful */
  unsigned addr_year;
  unsigned addr_month;
  unsigned addr_date;
  unsigned addr_day;
  unsigned addr_hour;
  unsigned addr_minutes;
  unsigned addr_seconds;
  unsigned addr_control;
} hw_nvram_device;

static void *
hw_nvram_create(const char *name,
		const device_unit *unit_address,
		const char *args)
{
  hw_nvram_device *nvram = ZALLOC(hw_nvram_device);
  return nvram;
}

typedef struct _hw_nvram_reg_spec {
  unsigned32 base;
  unsigned32 size;
} hw_nvram_reg_spec;

static void
hw_nvram_init_address(device *me)
{
  hw_nvram_device *nvram = (hw_nvram_device*)device_data(me);
  
  /* use the generic init code to attach this device to its parent bus */
  generic_device_init_address(me);

  /* find the first non zero reg property and use that as the device
     size */
  if (nvram->sizeof_memory == 0) {
    reg_property_spec reg;
    int reg_nr;
    for (reg_nr = 0;
	 device_find_reg_array_property(me, "reg", reg_nr, &reg);
	 reg_nr++) {
      unsigned attach_size;
      if (device_size_to_attach_size(device_parent(me),
				     &reg.size, &attach_size,
				     me)) {
	nvram->sizeof_memory = attach_size;
	break;
      }
    }
    if (nvram->sizeof_memory == 0)
      device_error(me, "reg property must contain a non-zero phys-addr:size tupple");
    if (nvram->sizeof_memory < 8)
      device_error(me, "NVRAM must be at least 8 bytes in size");
  }

  /* initialize the hw_nvram */
  if (nvram->memory == NULL) {
    nvram->memory = zalloc(nvram->sizeof_memory);
  }
  else
    memset(nvram->memory, 0, nvram->sizeof_memory);
  
  if (device_find_property(me, "timezone") == NULL)
    nvram->timezone = 0;
  else
    nvram->timezone = device_find_integer_property(me, "timezone");
  
  nvram->addr_year = nvram->sizeof_memory - 1;
  nvram->addr_month = nvram->sizeof_memory - 2;
  nvram->addr_date = nvram->sizeof_memory - 3;
  nvram->addr_day = nvram->sizeof_memory - 4;
  nvram->addr_hour = nvram->sizeof_memory - 5;
  nvram->addr_minutes = nvram->sizeof_memory - 6;
  nvram->addr_seconds = nvram->sizeof_memory - 7;
  nvram->addr_control = nvram->sizeof_memory - 8;
  
}

static int
hw_nvram_bcd(int val)
{
  val = val % 100;
  if (val < 0)
    val += 100;
  return ((val / 10) << 4) + (val % 10);
}


/* If reached an update interval and allowed, update the clock within
   the hw_nvram.  While this function could be implemented using events
   it isn't on the assumption that the HW_NVRAM will hardly ever be
   referenced and hence there is little need in keeping the clock
   continually up-to-date */

static void
hw_nvram_update_clock(hw_nvram_device *nvram,
		      cpu *processor)
{
#ifdef HAVE_TIME_H
  if (!(nvram->memory[nvram->addr_control] & 0xc0)) {
    time_t host_time = time(NULL);
    if (nvram->host_time != host_time) {
      time_t nvtime = host_time + nvram->timezone;
      struct tm *clock = gmtime(&nvtime);
      nvram->host_time = host_time;
      nvram->memory[nvram->addr_year] = hw_nvram_bcd(clock->tm_year);
      nvram->memory[nvram->addr_month] = hw_nvram_bcd(clock->tm_mon + 1);
      nvram->memory[nvram->addr_date] = hw_nvram_bcd(clock->tm_mday);
      nvram->memory[nvram->addr_day] = hw_nvram_bcd(clock->tm_wday + 1);
      nvram->memory[nvram->addr_hour] = hw_nvram_bcd(clock->tm_hour);
      nvram->memory[nvram->addr_minutes] = hw_nvram_bcd(clock->tm_min);
      nvram->memory[nvram->addr_seconds] = hw_nvram_bcd(clock->tm_sec);
    }
  }
#else
  error("fixme - where do I find out GMT\n");
#endif
}

static void
hw_nvram_set_clock(hw_nvram_device *nvram, cpu *processor)
{
  error ("fixme - how do I set the localtime\n");
}

static unsigned
hw_nvram_io_read_buffer(device *me,
			void *dest,
			int space,
			unsigned_word addr,
			unsigned nr_bytes,
			cpu *processor,
			unsigned_word cia)
{
  int i;
  hw_nvram_device *nvram = (hw_nvram_device*)device_data(me);
  for (i = 0; i < nr_bytes; i++) {
    unsigned address = (addr + i) % nvram->sizeof_memory;
    unsigned8 data = nvram->memory[address];
    hw_nvram_update_clock(nvram, processor);
    ((unsigned8*)dest)[i] = data;
  }
  return nr_bytes;
}

static unsigned
hw_nvram_io_write_buffer(device *me,
			 const void *source,
			 int space,
			 unsigned_word addr,
			 unsigned nr_bytes,
			 cpu *processor,
			 unsigned_word cia)
{
  int i;
  hw_nvram_device *nvram = (hw_nvram_device*)device_data(me);
  for (i = 0; i < nr_bytes; i++) {
    unsigned address = (addr + i) % nvram->sizeof_memory;
    unsigned8 data = ((unsigned8*)source)[i];
    if (address == nvram->addr_control
	&& (data & 0x80) == 0
	&& (nvram->memory[address] & 0x80) == 0x80)
      hw_nvram_set_clock(nvram, processor);
    else
      hw_nvram_update_clock(nvram, processor);
    nvram->memory[address] = data;
  }
  return nr_bytes;
}

static device_callbacks const hw_nvram_callbacks = {
  { hw_nvram_init_address, },
  { NULL, }, /* address */
  { hw_nvram_io_read_buffer, hw_nvram_io_write_buffer }, /* IO */
};

const device_descriptor hw_nvram_device_descriptor[] = {
  { "nvram", hw_nvram_create, &hw_nvram_callbacks },
  { NULL },
};

#endif /* _HW_NVRAM_C_ */
