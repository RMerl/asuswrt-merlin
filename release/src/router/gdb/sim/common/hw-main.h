/* Common hardware header file.
   Copyright (C) 1998, 2007 Free Software Foundation, Inc.
   Contributed by Andrew Cagney and Cygnus Support.

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


#ifndef HW_MAIN
#define HW_MAIN

/* establish a type system */
#include "sim-basics.h"

/* construct a hw device */
#include "hw-device.h"
#include "hw-properties.h"
#include "hw-events.h"
#include "hw-alloc.h"
#include "hw-instances.h"
#include "hw-handles.h"
#include "hw-ports.h"

/* Description of a hardware device */

typedef void (hw_finish_method)
     (struct hw *me);

struct hw_descriptor {
  const char *family;
  hw_finish_method *to_finish;
};

/* Helper functions to make the implementation of a device easier */

/* Go through the devices reg properties and look for those specifying
   an address to attach various registers to */

void do_hw_attach_regs (struct hw *me);

/* Perform a polling read on FD returning either the number of bytes
   or a hw_io status code that indicates the reason for the read
   failure */

enum {
  HW_IO_EOF = -1, HW_IO_NOT_READY = -2, /* See: IEEE 1275 */
};

typedef int (do_hw_poll_read_method)
     (SIM_DESC sd, int, char *, int);

int do_hw_poll_read
(struct hw *me,
 do_hw_poll_read_method *read,
 int sim_io_fd,
 void *buf,
 unsigned size_of_buf);


#endif
