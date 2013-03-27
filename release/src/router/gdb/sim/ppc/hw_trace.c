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


#ifndef _HW_TRACE_C_
#define _HW_TRACE_C_

#include "device_table.h"
#include <stdarg.h>

/* DEVICE

   trace - the properties of this dummy device specify trace options

   DESCRIPTION

   The properties of this device are used, during initialization, to
   specify the value various simulation trace options.  The
   initialization can occure implicitly (during device tree init) or
   explicitly using this devices ioctl method.

   The actual options and their default values (for a given target)
   are defined in the file debug.

   This device is normally a child of the /openprom node.

   EXAMPLE

   The trace option dump-device-tree can be enabled by specifying the
   option:

   |	-o '/openprom/trace/dump-device-tree 0x1'

   Alternativly the shorthand version:

   |	-t dump-device-tree

   can be used. */


/* Hook to allow the initialization of the trace options at any time */

static int
hw_trace_ioctl(device *me,
	       cpu *processor,
	       unsigned_word cia,
	       device_ioctl_request request,
	       va_list ap)
{
  switch (request) {
  case device_ioctl_set_trace:
    {
      const device_property *prop = device_find_property(me, NULL);
      while (prop != NULL) {
	const char *name = prop->name;
	unsigned32 value = device_find_integer_property(me, name);
	trace_option(name, value);
	prop = device_next_property(prop);
      }
    }
    break;
  default:
    device_error(me, "insupported ioctl request");
    break;
  }
  return 0;
}


static device_callbacks const hw_trace_callbacks = {
  { NULL, }, /* init */
  { NULL, }, /* address */
  { NULL, }, /* IO */
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { NULL, }, /* unit */
  NULL, /* instance-create */
  hw_trace_ioctl,
};

const device_descriptor hw_trace_device_descriptor[] = {
  { "trace", NULL, &hw_trace_callbacks },
  { NULL },
};

#endif /* _HW_TRACE_C_ */
