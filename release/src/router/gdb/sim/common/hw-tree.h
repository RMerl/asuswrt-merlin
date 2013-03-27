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


#ifndef HW_TREE
#define HW_TREE


struct hw *hw_tree_create
(SIM_DESC sd,
 const char *device);

void hw_tree_delete
(struct hw *root);

struct hw *hw_tree_parse
(struct hw *root,
 const char *fmt,
 ...) __attribute__ ((format (printf, 2, 3)));

struct hw *hw_tree_vparse
(struct hw *root,
 const char *fmt,
 va_list ap);


void hw_tree_finish
(struct hw *root);

typedef void (hw_tree_print_callback)
     (void *,
      const char *fmt,
      ...);

void hw_tree_print
(struct hw *root,
 hw_tree_print_callback *print,
 void *file);


/* Tree traversal::

   The entire device tree can be traversed using the
   <<device_tree_traverse()>> function.  The traversal can be in
   either prefix or postfix order.

   */

typedef void (hw_tree_traverse_function)
     (struct hw *device,
      void *data);

void hw_tree_traverse
(struct hw *root,
 hw_tree_traverse_function *prefix,
 hw_tree_traverse_function *postfix,
 void *data);


/* Tree lookup::

   The function <<hw_tree_find_device()>> will attempt to locate the
   specified device within the tree.  If the device is not found a
   NULL device is returned.

   */

struct hw * hw_tree_find_device
(struct hw *root,
 const char *path);


const struct hw_property *hw_tree_find_property
(struct hw *root,
 const char *path_to_property);

int hw_tree_find_boolean_property
(struct hw *root,
 const char *path_to_property);

signed_cell hw_tree_find_integer_property
(struct hw *root,
 const char *path_to_property);

#if NOT_YET
device_instance *hw_tree_find_ihandle_property
(struct hw *root,
 const char *path_to_property);
#endif

const char *hw_tree_find_string_property
(struct hw *root,
 const char *path_to_property);


/* Perform a soft reset on the created tree. */

void hw_tree_reset
(struct hw *root);


#endif
