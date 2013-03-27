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


#ifndef _CORE_C_
#define _CORE_C_

#include "basics.h"
#include "device_table.h"
#include "corefile.h"

typedef struct _core_mapping core_mapping;
struct _core_mapping {
  /* common */
  int level;
  int space;
  unsigned_word base;
  unsigned_word bound;
  unsigned nr_bytes;
  /* memory map */
  void *free_buffer;
  void *buffer;
  /* callback map */
  device *device;
  /* growth */
  core_mapping *next;
};

struct _core_map {
  core_mapping *first;
};

typedef enum {
  core_read_map,
  core_write_map,
  core_execute_map,
  nr_core_map_types,
} core_map_types;

struct _core {
  core_map map[nr_core_map_types];
};


INLINE_CORE\
(core *)
core_create(void)
{
  return ZALLOC(core);
}


INLINE_CORE\
(core *)
core_from_device(device *root)
{
  root = device_root(root);
  ASSERT(strcmp(device_name(root), "core") == 0);
  return device_data(root);
}


INLINE_CORE\
(void)
core_init(core *memory)
{
  core_map_types access_type;
  for (access_type = 0;
       access_type < nr_core_map_types;
       access_type++) {
    core_map *map = memory->map + access_type;
    /* blow away old mappings */
    core_mapping *curr = map->first;
    while (curr != NULL) {
      core_mapping *tbd = curr;
      curr = curr->next;
      if (tbd->free_buffer != NULL) {
	ASSERT(tbd->buffer != NULL);
	zfree(tbd->free_buffer);
      }
      zfree(tbd);
    }
    map->first = NULL;
  }
}



/* the core has three sub mappings that the more efficient
   read/write fixed quantity functions use */

INLINE_CORE\
(core_map *)
core_readable(core *memory)
{
  return memory->map + core_read_map;
}

INLINE_CORE\
(core_map *)
core_writeable(core *memory)
{
  return memory->map + core_write_map;
}

INLINE_CORE\
(core_map *)
core_executable(core *memory)
{
  return memory->map + core_execute_map;
}



STATIC_INLINE_CORE\
(core_mapping *)
new_core_mapping(attach_type attach,
		 int space,
		 unsigned_word addr,
		 unsigned nr_bytes,
		 device *device,
		 void *buffer,
		 void *free_buffer)
{
  core_mapping *new_mapping = ZALLOC(core_mapping);
  /* common */
  new_mapping->level = attach;
  new_mapping->space = space;
  new_mapping->base = addr;
  new_mapping->nr_bytes = nr_bytes;
  new_mapping->bound = addr + (nr_bytes - 1);
  if (attach == attach_raw_memory) {
    new_mapping->buffer = buffer;
    new_mapping->free_buffer = free_buffer;
  }
  else if (attach >= attach_callback) {
    new_mapping->device = device;
  }
  else {
    error("new_core_mapping() - internal error - unknown attach type %d\n",
	  attach);
  }
  return new_mapping;
}


STATIC_INLINE_CORE\
(void)
core_map_attach(core_map *access_map,
		attach_type attach,
		int space,
		unsigned_word addr,
		unsigned nr_bytes, /* host limited */
		device *client, /*callback/default*/
		void *buffer, /*raw_memory*/
		void *free_buffer) /*raw_memory*/
{
  /* find the insertion point for this additional mapping and insert */
  core_mapping *next_mapping;
  core_mapping **last_mapping;

  /* actually do occasionally get a zero size map */
  if (nr_bytes == 0) {
    device_error(client, "called on core_map_attach() with size zero");
  }

  /* find the insertion point (between last/next) */
  next_mapping = access_map->first;
  last_mapping = &access_map->first;
  while(next_mapping != NULL
	&& (next_mapping->level < attach
	    || (next_mapping->level == attach
		&& next_mapping->bound < addr))) {
    /* provided levels are the same */
    /* assert: next_mapping->base > all bases before next_mapping */
    /* assert: next_mapping->bound >= all bounds before next_mapping */
    last_mapping = &next_mapping->next;
    next_mapping = next_mapping->next;
  }

  /* check insertion point correct */
  ASSERT(next_mapping == NULL || next_mapping->level >= attach);
  if (next_mapping != NULL && next_mapping->level == attach
      && next_mapping->base < (addr + (nr_bytes - 1))) {
    device_error(client, "map overlap when attaching %d:0x%lx (%ld)",
		 space, (long)addr, (long)nr_bytes);
  }

  /* create/insert the new mapping */
  *last_mapping = new_core_mapping(attach,
				   space, addr, nr_bytes,
				   client, buffer, free_buffer);
  (*last_mapping)->next = next_mapping;
}


INLINE_CORE\
(void)
core_attach(core *memory,
	    attach_type attach,
	    int space,
	    access_type access,
	    unsigned_word addr,
	    unsigned nr_bytes, /* host limited */
	    device *client) /*callback/default*/
{
  core_map_types access_map;
  void *buffer;
  void *free_buffer;
  if (attach == attach_raw_memory) {
    /* Padd out the raw buffer to ensure that ADDR starts on a
       correctly aligned boundary */
    int padding = (addr % sizeof (unsigned64));
    free_buffer = zalloc(nr_bytes + padding);
    buffer = (char*)free_buffer + padding;
  }
  else {
    buffer = NULL;
    free_buffer = &buffer; /* marker for assertion */
  }
  for (access_map = 0; 
       access_map < nr_core_map_types;
       access_map++) {
    switch (access_map) {
    case core_read_map:
      if (access & access_read)
	core_map_attach(memory->map + access_map,
			attach,
			space, addr, nr_bytes,
			client, buffer, free_buffer);
      free_buffer = NULL;
      break;
    case core_write_map:
      if (access & access_write)
	core_map_attach(memory->map + access_map,
			attach,
			space, addr, nr_bytes,
			client, buffer, free_buffer);
      free_buffer = NULL;
      break;
    case core_execute_map:
      if (access & access_exec)
	core_map_attach(memory->map + access_map,
			attach,
			space, addr, nr_bytes,
			client, buffer, free_buffer);
      free_buffer = NULL;
      break;
    default:
      error("core_attach() internal error\n");
      break;
    }
  }
  /* allocated buffer must attach to at least one thing */
  ASSERT(free_buffer == NULL);
}


STATIC_INLINE_CORE\
(core_mapping *)
core_map_find_mapping(core_map *map,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      cpu *processor,
		      unsigned_word cia,
		      int abort) /*either 0 or 1 - helps inline */
{
  core_mapping *mapping = map->first;
  ASSERT((addr & (nr_bytes - 1)) == 0); /* must be aligned */
  ASSERT((addr + (nr_bytes - 1)) >= addr); /* must not wrap */
  while (mapping != NULL) {
    if (addr >= mapping->base
	&& (addr + (nr_bytes - 1)) <= mapping->bound)
      return mapping;
    mapping = mapping->next;
  }
  if (abort)
    error("core_find_mapping() - access to unmaped address, attach a default map to handle this - addr=0x%x nr_bytes=0x%x processor=0x%x cia=0x%x\n",
	  addr, nr_bytes, processor, cia);
  return NULL;
}


STATIC_INLINE_CORE\
(void *)
core_translate(core_mapping *mapping,
	       unsigned_word addr)
{
  return (void *)(((char *)mapping->buffer) + addr - mapping->base);
}


INLINE_CORE\
(unsigned)
core_map_read_buffer(core_map *map,
		     void *buffer,
		     unsigned_word addr,
		     unsigned len)
{
  unsigned count = 0;
  while (count < len) {
    unsigned_word raddr = addr + count;
    core_mapping *mapping =
      core_map_find_mapping(map,
			    raddr, 1,
			    NULL, /*processor*/
			    0, /*cia*/
			    0); /*dont-abort*/
    if (mapping == NULL)
      break;
    if (mapping->device != NULL) {
      int nr_bytes = len - count;
      if (raddr + nr_bytes - 1> mapping->bound)
	nr_bytes = mapping->bound - raddr + 1;
      if (device_io_read_buffer(mapping->device,
				(unsigned_1*)buffer + count,
				mapping->space,
				raddr,
				nr_bytes,
				0, /*processor*/
				0 /*cpu*/) != nr_bytes)
	break;
      count += nr_bytes;
    }
    else {
      ((unsigned_1*)buffer)[count] =
	*(unsigned_1*)core_translate(mapping, raddr);
      count += 1;
    }
  }
  return count;
}


INLINE_CORE\
(unsigned)
core_map_write_buffer(core_map *map,
		      const void *buffer,
		      unsigned_word addr,
		      unsigned len)
{
  unsigned count = 0;
  while (count < len) {
    unsigned_word raddr = addr + count;
    core_mapping *mapping = core_map_find_mapping(map,
						  raddr, 1,
						  NULL, /*processor*/
						  0, /*cia*/
						  0); /*dont-abort*/
    if (mapping == NULL)
      break;
    if (mapping->device != NULL) {
      int nr_bytes = len - count;
      if (raddr + nr_bytes - 1 > mapping->bound)
	nr_bytes = mapping->bound - raddr + 1;
      if (device_io_write_buffer(mapping->device,
				 (unsigned_1*)buffer + count,
				 mapping->space,
				 raddr,
				 nr_bytes,
				 0, /*processor*/
				 0 /*cpu*/) != nr_bytes)
	break;
      count += nr_bytes;
    }
    else {
      *(unsigned_1*)core_translate(mapping, raddr) =
	((unsigned_1*)buffer)[count];
      count += 1;
    }
  }
  return count;
}


/* define the read/write 1/2/4/8/word functions */

#define N 1
#include "corefile-n.h"
#undef N

#define N 2
#include "corefile-n.h"
#undef N

#define N 4
#include "corefile-n.h"
#undef N

#define N 8
#include "corefile-n.h"
#undef N

#define N word
#include "corefile-n.h"
#undef N

#endif /* _CORE_C_ */
