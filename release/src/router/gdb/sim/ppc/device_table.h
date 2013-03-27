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


#ifndef _DEVICE_TABLE_H_
#define _DEVICE_TABLE_H_

#include "basics.h"
#include "device.h"
#include "tree.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


typedef struct _device_callbacks device_callbacks;


/* The creator, returns a pointer to any data that should be allocated
   once during (multiple) simulation runs */

typedef void *(device_creator)
     (const char *name,
      const device_unit *unit_address,
      const char *args);


/* two stages of initialization */

typedef void (device_init_callback)
     (device *me);
     
typedef struct _device_init_callbacks {
  device_init_callback *address; /* NULL - ignore */
  device_init_callback *data; /* NULL - ignore */
} device_init_callbacks;


/* attaching/detaching a devices address space to its parent */

typedef void (device_address_callback)
     (device *me,
      attach_type attach,
      int space,
      unsigned_word addr,
      unsigned nr_bytes,
      access_type access,
      device *client); /*callback/default*/

typedef struct _device_address_callbacks {
  device_address_callback *attach;
  device_address_callback *detach;
} device_address_callbacks;


/* I/O operations - from parent */

typedef unsigned (device_io_read_buffer_callback)
     (device *me,
      void *dest,
      int space,
      unsigned_word addr,
      unsigned nr_bytes,
      cpu *processor,
      unsigned_word cia);

typedef unsigned (device_io_write_buffer_callback)
     (device *me,
      const void *source,
      int space,
      unsigned_word addr,
      unsigned nr_bytes,
      cpu *processor,
      unsigned_word cia);

typedef struct _device_io_callbacks { /* NULL - error */
  device_io_read_buffer_callback *read_buffer;
  device_io_write_buffer_callback *write_buffer;
} device_io_callbacks;


/* DMA transfers by a device via its parent */

typedef unsigned (device_dma_read_buffer_callback)
     (device *me,
      void *dest,
      int space,
      unsigned_word addr,
      unsigned nr_bytes);

typedef unsigned (device_dma_write_buffer_callback)
     (device *me,
      const void *source,
      int space,
      unsigned_word addr,
      unsigned nr_bytes,
      int violate_read_only_section);

typedef struct _device_dma_callbacks { /* NULL - error */
  device_dma_read_buffer_callback *read_buffer;
  device_dma_write_buffer_callback *write_buffer;
} device_dma_callbacks;


/* Interrupts */

typedef void (device_interrupt_event_callback)
     (device *me,
      int my_port,
      device *source,
      int source_port,
      int level,
      cpu *processor,
      unsigned_word cia);

typedef void (device_child_interrupt_event_callback)
     (device *me,
      device *parent,
      device *source,
      int source_port,
      int level,
      cpu *processor,
      unsigned_word cia);
      
typedef struct _device_interrupt_port_descriptor {
  const char *name;
  int number; 
  int nr_ports;
  port_direction direction;
} device_interrupt_port_descriptor;

typedef struct _device_interrupt_callbacks {
  device_interrupt_event_callback *event;
  device_child_interrupt_event_callback *child_event;
  const device_interrupt_port_descriptor *ports;
} device_interrupt_callbacks;


/* symbolic value decoding */

typedef int (device_unit_decode_callback)
     (device *bus,
      const char *unit,
      device_unit *address);

typedef int (device_unit_encode_callback)
     (device *bus,
      const device_unit *unit_address,
      char *buf,
      int sizeof_buf);

typedef int (device_address_to_attach_address_callback)
     (device *bus,
      const device_unit *address,
      int *attach_space,
      unsigned_word *attach_address,
      device *client);

typedef int (device_size_to_attach_size_callback)
     (device *bus,
      const device_unit *size,
      unsigned *nr_bytes,
      device *client);

typedef struct _device_convert_callbacks {
  device_unit_decode_callback *decode_unit;
  device_unit_encode_callback *encode_unit;
  device_address_to_attach_address_callback *address_to_attach_address;
  device_size_to_attach_size_callback *size_to_attach_size;
} device_convert_callbacks;


/* instances */

typedef void (device_instance_delete_callback)
     (device_instance *instance);

typedef int (device_instance_read_callback)
     (device_instance *instance,
      void *buf,
      unsigned_word len);

typedef int (device_instance_write_callback)
     (device_instance *instance,
      const void *buf,
      unsigned_word len);

typedef int (device_instance_seek_callback)
     (device_instance *instance,
      unsigned_word pos_hi,
      unsigned_word pos_lo);

typedef int (device_instance_method)
     (device_instance *instance,
      int n_stack_args,
      unsigned_cell stack_args[/*n_stack_args*/],
      int n_stack_returns,
      unsigned_cell stack_returns[/*n_stack_returns*/]);

typedef struct _device_instance_methods {
  const char *name;
  device_instance_method *method;
} device_instance_methods;

struct _device_instance_callbacks { /* NULL - error */
  device_instance_delete_callback *delete;
  device_instance_read_callback *read;
  device_instance_write_callback *write;
  device_instance_seek_callback *seek;
  const device_instance_methods *methods;
};

typedef device_instance *(device_create_instance_callback)
     (device *me,
      const char *full_path,
      const char *args);

typedef device_instance *(package_create_instance_callback)
     (device_instance *parent,
      const char *args);


/* all else fails */

typedef int (device_ioctl_callback)
     (device *me,
      cpu *processor,
      unsigned_word cia,
      device_ioctl_request request,
      va_list ap);

typedef void (device_usage_callback)
     (int verbose);


/* the callbacks */

struct _device_callbacks {

  /* initialization */
  device_init_callbacks init;

  /* address/data config - from child */
  device_address_callbacks address;

  /* address/data transfer - from parent */
  device_io_callbacks io;

  /* address/data transfer - from child */
  device_dma_callbacks dma;

  /* interrupt signalling */
  device_interrupt_callbacks interrupt;

  /* bus address decoding */
  device_convert_callbacks convert;

  /* instances */
  device_create_instance_callback *instance_create;

  /* back door to anything we've forgot */
  device_ioctl_callback *ioctl;
  device_usage_callback *usage;
};


/* Table of all the devices and a function to lookup/create a device
   from its name */

typedef struct _device_descriptor device_descriptor;
struct _device_descriptor {
  const char *name;
  device_creator *creator;
  const device_callbacks *callbacks;
};

extern const device_descriptor *const device_table[];
#include "hw.h"


/* Pass through, ignore and generic callback functions.  A call going
   towards the root device are passed on up, local calls are ignored
   and call downs abort */

extern device_address_callback passthrough_device_address_attach;
extern device_address_callback passthrough_device_address_detach;
extern device_dma_read_buffer_callback passthrough_device_dma_read_buffer;
extern device_dma_write_buffer_callback passthrough_device_dma_write_buffer;

extern device_unit_decode_callback ignore_device_unit_decode;

extern device_init_callback generic_device_init_address;
extern device_unit_decode_callback generic_device_unit_decode;
extern device_unit_encode_callback generic_device_unit_encode;
extern device_address_to_attach_address_callback generic_device_address_to_attach_address;
extern device_size_to_attach_size_callback generic_device_size_to_attach_size;


extern const device_callbacks passthrough_device_callbacks;

#endif /* _DEVICE_TABLE_H_ */
