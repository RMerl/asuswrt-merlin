/*  This file is part of the program psim.
    
    Copyright 1994, 1995, 1996, 2003 Andrew Cagney
    
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


#ifndef _HW_REGISTER_C_
#define _HW_REGISTER_C_

#include "device_table.h"
#include <stdlib.h>
#include "psim.h"

/* DEVICE


   register - dummy device to initialize processor registers


   DESCRIPTION

   The properties of this device are used, during initialization, to
   specify the initial value of various processor registers.  The
   property name specifying the register to be initialized with the
   special form <cpu-nr>.<register> being used to initialize a
   specific processor's register (eg 0.pc).

   Because, when the device tree is created, overriding properties are
   entered into the tree before any default values, this device must
   initialize registers in newest (default) to oldest (overriding)
   property order.

   The actual registers (for a given target) are defined in the file
   registers.c.

   This device is normally a child of the /openprom/init node.


   EXAMPLES


   Given a device tree containing the entry:

   |	/openprom/init/register/pc 0xfff00cf0

   then specifying the command line option:

   |	-o '/openprom/init/register/pc 0x0'

   would override the initial value of processor zero's program
   counter.  The resultant device tree tree containing:

   |	/openprom/init/register/0.pc 0x0
   |	/openprom/init/register/pc 0xfff00cf0

   and would be processed last to first resulting in the sequence: set
   all program counters to 0xfff00cf0; set processor zero's program
   counter to zero.

   */

static void
do_register_init(device *me,
		 const device_property *prop)
{
  psim *system = device_system(me);
  if (prop != NULL) {
    const char *name = prop->name;
    unsigned32 value = device_find_integer_property(me, name);
    int processor;

    do_register_init(me, device_next_property(prop));

    if (strchr(name, '.') == NULL) {
      processor = -1;
      DTRACE(register, ("%s=0x%lx\n", name, (unsigned long)value));
    }
    else {
      char *end;
      processor = strtoul(name, &end, 0);
      ASSERT(end[0] == '.');
      name = end+1;
      DTRACE(register, ("%d.%s=0x%lx\n", processor, name,
			(unsigned long)value));
    }    
    if (psim_write_register(system, processor, /* all processors */
			    &value,
			    name,
			    cooked_transfer) <= 0)
      error("Invalid register name %s\n", name);
  }
}
		 

static void
register_init_data_callback(device *me)
{
  const device_property *prop = device_find_property(me, NULL);
  do_register_init(me, prop);
}


static device_callbacks const register_callbacks = {
  { NULL, register_init_data_callback, },
  { NULL, }, /* address */
  { NULL, }, /* IO */
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { NULL, }, /* unit */
};

const device_descriptor hw_register_device_descriptor[] = {
  { "register", NULL, &register_callbacks },
  { NULL },
};

#endif /* _HW_REGISTER_C_ */
