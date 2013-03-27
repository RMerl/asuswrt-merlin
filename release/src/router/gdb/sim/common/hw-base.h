/* The common simulator framework for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney and Red Hat.

   This file is part of GDB.

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


#ifndef HW_BASE
#define HW_BASE

/* Create a primative device */

struct hw *hw_create
(struct sim_state *sd,
 struct hw *parent,
 const char *family,
 const char *name,
 const char *unit,
 const char *args);


/* Complete the creation of that device (finish overrides methods
   using the set_hw_* operations below) */

void hw_finish
(struct hw *me);

int hw_finished_p
(struct hw *me);


/* Delete the entire device */

void hw_delete
(struct hw *me);


/* Override device methods */

typedef void (hw_delete_callback)
     (struct hw *me);

extern void set_hw_delete(struct hw* hw, hw_delete_callback method);


/* ALLOC */

extern void create_hw_alloc_data
(struct hw *hw);
extern void delete_hw_alloc_data
(struct hw *hw);


/* PORTS */

extern void create_hw_port_data
(struct hw *hw);
extern void delete_hw_port_data
(struct hw *hw);


/* PROPERTIES */

extern void create_hw_property_data
(struct hw *hw);
extern void delete_hw_property_data
(struct hw *hw);


/* EVENTS */

extern void create_hw_event_data
(struct hw *hw);
extern void delete_hw_event_data
(struct hw *hw);


/* HANDLES */

extern void create_hw_handle_data
(struct hw *hw);
extern void delete_hw_handle_data
(struct hw *hw);


/* INSTANCES */

extern void create_hw_instance_data
(struct hw *hw);
extern void delete_hw_instance_data
(struct hw *hw);


#endif
