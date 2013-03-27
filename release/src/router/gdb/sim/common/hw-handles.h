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


#ifndef HW_HANDLES_H
#define HW_HANDLES_H


/* Export a capability (handle) data base that maps between internal
   data values and those given to a simulation. */


cell_word hw_handle_2ihandle
(struct hw *db,
 struct hw_instance *instance);

struct hw_instance *hw_handle_ihandle2
(struct hw *db,
 cell_word external);

void hw_handle_add_ihandle
(struct hw *db,
 struct hw_instance *instance);

void hw_handle_remove_ihandle
(struct hw *db,
 struct hw_instance *instance);


cell_word hw_handle_2phandle
(struct hw *db,
 struct hw *hw);

struct hw *hw_handle_phandle2
(struct hw *db,
 cell_word external);

void hw_handle_add_phandle
(struct hw *db,
 struct hw *hw);

void hw_handle_remove_phandle
(struct hw *db,
 struct hw *hw);

#endif
