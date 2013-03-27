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


#ifndef _TREE_H_
#define _TREE_H_

#ifndef INLINE_TREE
#define INLINE_TREE
#endif

/* Constructing the device tree:

   The initial device tree populated with devices and basic properties
   is created using the function <<device_tree_add_parsed()>>.  This
   function parses a PSIM device specification and uses it to populate
   the tree accordingly.

   This function accepts a printf style formatted string as the
   argument that describes the entry.  Any properties or interrupt
   connections added to a device tree using this function are marked
   as having a permenant disposition.  When the tree is (re)
   initialized they will be restored to their initial value.

   */

EXTERN_TREE\
(char*) tree_quote_property
(const char *property_value);

EXTERN_TREE\
(device *) tree_parse
(device *root,
 const char *fmt,
 ...) __attribute__ ((format (printf, 2, 3)));


INLINE_TREE\
(void) tree_usage
(int verbose);

INLINE_TREE\
(void) tree_print
(device *root);

INLINE_TREE\
(device_instance*) tree_instance
(device *root,
 const char *device_specifier);


/* Tree traversal::

   The entire device tree can be traversed using the
   <<device_tree_traverse()>> function.  The traversal can be in
   either pre- or postfix order.

   */

typedef void (tree_traverse_function)
     (device *device,
      void *data);

INLINE_DEVICE\
(void) tree_traverse
(device *root,
 tree_traverse_function *prefix,
 tree_traverse_function *postfix,
 void *data);


/* Tree lookup::

   The function <<tree_find_device()>> will attempt to locate
   the specified device within the tree.  If the device is not found a
   NULL device is returned.

   */

INLINE_TREE\
(device *) tree_find_device
(device *root,
 const char *path);


INLINE_TREE\
(const device_property *) tree_find_property
(device *root,
 const char *path_to_property);

INLINE_TREE\
(int) tree_find_boolean_property
(device *root,
 const char *path_to_property);

INLINE_TREE\
(signed_cell) tree_find_integer_property
(device *root,
 const char *path_to_property);

INLINE_TREE\
(device_instance *) tree_find_ihandle_property
(device *root,
 const char *path_to_property);

INLINE_TREE\
(const char *) tree_find_string_property
(device *root,
 const char *path_to_property);


/* Initializing the created tree:

   Once a device tree has been created the <<device_tree_init()>>
   function is used to initialize it.  The exact sequence of events
   that occure during initialization are described separatly.

   */

INLINE_TREE\
(void) tree_init
(device *root,
 psim *system);


#endif /* _TREE_H_ */
