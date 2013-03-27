/*  This file is part of the program psim.

    Copyright (C) 1994-1995,1997, Andrew Cagney <cagney@highland.com.au>

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


#ifndef _CAP_C_
#define _CAP_C_

#include "cap.h"

typedef struct _cap_mapping cap_mapping;
struct _cap_mapping {
  unsigned_cell external;
  void *internal;
  cap_mapping *next;
};

struct _cap {
  int nr_mappings;
  cap_mapping *mappings;
};

INLINE_CAP\
(cap *)
cap_create(const char *key)
{
  return ZALLOC(cap);
}

INLINE_CAP\
(void)
cap_init(cap *db)
{
  cap_mapping *current_map = db->mappings;
  if (current_map != NULL) {
    db->nr_mappings = db->mappings->external;
    /* verify that the mappings that were not removed are in sequence
       down to nr 1 */
    while (current_map->next != NULL) {
      if (current_map->external != current_map->next->external + 1)
	error("cap: cap database possibly corrupt");
      current_map = current_map->next;
    }
    ASSERT(current_map->next == NULL);
    if (current_map->external != 1)
      error("cap: cap database possibly currupt");
  }
  else {
    db->nr_mappings = 0;
  }
}

INLINE_CAP\
(void *)
cap_internal(cap *db,
	     signed_cell external)
{
  cap_mapping *current_map = db->mappings;
  while (current_map != NULL) {
    if (current_map->external == external)
      return current_map->internal;
    current_map = current_map->next;
  }
  return (void*)0;
}

INLINE_CAP\
(signed_cell)
cap_external(cap *db,
	     void *internal)
{
  cap_mapping *current_map = db->mappings;
  while (current_map != NULL) {
    if (current_map->internal == internal)
      return current_map->external;
    current_map = current_map->next;
  }
  return 0;
}

INLINE_CAP\
(void)
cap_add(cap *db,
	void *internal)
{
  if (cap_external(db, internal) != 0) {
    error("cap: attempting to add an object already in the data base");
  }
  else {
    /* insert at the front making things in decending order */
    cap_mapping *new_map = ZALLOC(cap_mapping);
    new_map->next = db->mappings;
    new_map->internal = internal;
    db->nr_mappings += 1;
    new_map->external = db->nr_mappings;
    db->mappings = new_map;
  }
}

INLINE_CAP\
(void)
cap_remove(cap *db,
	   void *internal)
{
  cap_mapping **current_map = &db->mappings;
  while (*current_map != NULL) {
    if ((*current_map)->internal == internal) {
      cap_mapping *delete = *current_map;
      *current_map = delete->next;
      zfree(delete);
      return;
    }
    current_map = &(*current_map)->next;
  }
  error("cap: attempt to remove nonexistant internal object");
}

#endif
