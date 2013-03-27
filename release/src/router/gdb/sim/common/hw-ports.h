/* Hardware ports.
   Copyright (C) 1998, 2007 Free Software Foundation, Inc.
   Contributed by Andrew Cagney and Cygnus Solutions.

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


#ifndef HW_PORTS_H
#define HW_PORTS_H

/* Initialize a port */

struct hw_port_descriptor {
  const char *name;
  int number; 
  int nr_ports;
  port_direction direction;
};

void set_hw_ports (struct hw *hw, const struct hw_port_descriptor ports[]);

typedef void (hw_port_event_method)
     (struct hw *me,
      int my_port,
      struct hw *source,
      int source_port,
      int level);

void set_hw_port_event (struct hw *hw, hw_port_event_method *to_port_event);


/* Port source

   A device drives its output ports using the call

   */

void hw_port_event
(struct hw *me,
 int my_port,
 int value);

/* This port event will then be propogated to any attached
   destination ports.

   Any interpretation of PORT and VALUE is model dependant.  As a
   guideline the following are recommended: PCI interrupts A-D should
   correspond to ports 0-3; level sensative interrupts be requested
   with a value of one and withdrawn with a value of 0; edge sensative
   interrupts always have a value of 1, the event its self is treated
   as the interrupt.


   Port destinations

   Attached to each port of a device can be zero or more
   desitinations.  These destinations consist of a device/port pair.
   A destination is attached/detached to a device line using the
   attach and detach calls. */

void hw_port_attach
(struct hw *me,
 int my_port,
 struct hw *dest,
 int dest_port,
 object_disposition disposition);

void hw_port_detach
(struct hw *me,
 int my_port,
 struct hw *dest,
 int dest_port);


/* Iterate over the list of ports attached to a device */

typedef void (hw_port_traverse_function)
     (struct hw *me,
      int my_port,
      struct hw *dest,
      int dest_port,
      void *data);

void hw_port_traverse
(struct hw *me,
 hw_port_traverse_function *handler,
 void *data);
 

/* DESTINATION is attached (detached) to LINE of the device ME


   Port conversion

   Users refer to port numbers symbolically.  For instance a device
   may refer to its `INT' signal which is internally represented by
   port 3.

   To convert to/from the symbolic and internal representation of a
   port name/number.  The following functions are available. */

int hw_port_decode
(struct hw *me,
 const char *symbolic_name,
 port_direction direction);

int hw_port_encode
(struct hw *me,
 int port_number,
 char *buf,
 int sizeof_buf,
 port_direction direction);
 

#endif
