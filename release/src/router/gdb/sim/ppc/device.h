/*  This file is part of the program psim.

    Copyright (C) 1994-1997, Andrew Cagney <cagney@highland.com.au>

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


#ifndef _DEVICE_H_
#define _DEVICE_H_

#ifndef INLINE_DEVICE
#define INLINE_DEVICE
#endif

/* declared in basics.h, this object is used everywhere */
/* typedef struct _device device; */


/* Introduction:

   As explained in earlier sections, the device, device instance,
   property and interrupts lie at the heart of PSIM's device model.

   In the below a synopsis of the device object and the operations it
   supports are given.  Details of this object can be found in the
   files <<device.h>> and <<device.c>>.

   */


/* Device creation: */

INLINE_DEVICE\
(device *) device_create
(device *parent,
 const char *base,
 const char *name,
 const char *unit_address,
 const char *args);

INLINE_DEVICE\
(void) device_usage
(int verbose);


/* Device initialization: */

INLINE_DEVICE\
(void) device_clean
(device *root,
 void *data);

INLINE_DEVICE\
(void) device_init_static_properties
(device *me,
 void *data);

INLINE_DEVICE\
(void) device_init_address
(device *me,
 void *data);

INLINE_DEVICE\
(void) device_init_runtime_properties
(device *me,
 void *data);

INLINE_DEVICE\
(void) device_init_data
(device *me,
 void *data);


/* Relationships:

   A device is able to determine its relationship to other devices
   within the tree.  Operations include querying for a devices parent,
   sibling, child, name, and path (from the root).

   */

INLINE_DEVICE\
(device *) device_parent
(device *me);

INLINE_DEVICE\
(device *) device_root
(device *me);

INLINE_DEVICE\
(device *) device_sibling
(device *me);

INLINE_DEVICE\
(device *) device_child
(device *me);

INLINE_DEVICE\
(const char *) device_name
(device *me);

INLINE_DEVICE\
(const char *) device_base
(device *me);

INLINE_DEVICE\
(const char *) device_path
(device *me);

INLINE_DEVICE\
(void *) device_data
(device *me);

INLINE_DEVICE\
(psim *) device_system
(device *me);

typedef struct _device_unit {
  int nr_cells;
  unsigned_cell cells[4]; /* unused cells are zero */
} device_unit;

INLINE_DEVICE\
(const device_unit *) device_unit_address
(device *me);

INLINE_DEVICE\
(int) device_decode_unit
(device *bus,
 const char *unit,
 device_unit *address);

INLINE_DEVICE\
(int) device_encode_unit
(device *bus,
 const device_unit *unit_address,
 char *buf,
 int sizeof_buf);


/* Convert an Open Firmware size into a form suitable for attach
   address calls.

   Return a zero result if the address should be ignored when looking
   for attach addresses */

INLINE_DEVICE\
(int) device_address_to_attach_address
(device *me,
 const device_unit *address,
 int *attach_space,
 unsigned_word *attach_address,
 device *client);


/* Convert an Open Firmware size into a form suitable for attach
   address calls

   Return a zero result if the address should be ignored */

INLINE_DEVICE\
(int) device_size_to_attach_size
(device *me,
 const device_unit *size,
 unsigned *nr_bytes,
 device *client);


INLINE_DEVICE\
(unsigned) device_nr_address_cells
(device *me);

INLINE_DEVICE\
(unsigned) device_nr_size_cells
(device *me);


/* Properties:

   Attached to a device are a number of properties.  Each property has
   a size and type (both of which can be queried).  A device is able
   to iterate over or query and set a properties value.

   */

/* The following are valid property types.  The property `array' is
   for generic untyped data. */

typedef enum {
  array_property,
  boolean_property,
  ihandle_property, /*runtime*/
  integer_property,
  range_array_property,
  reg_array_property,
  string_property,
  string_array_property,
} device_property_type;

typedef struct _device_property device_property;
struct _device_property {
  device *owner;
  const char *name;
  device_property_type type;
  unsigned sizeof_array;
  const void *array;
  const device_property *original;
  object_disposition disposition;
};


/* iterate through the properties attached to a device */

INLINE_DEVICE\
(const device_property *) device_next_property
(const device_property *previous);

INLINE_DEVICE\
(const device_property *) device_find_property
(device *me,
 const char *property); /* NULL for first property */


/* Manipulate the properties belonging to a given device.

   SET on the other hand will force the properties value.  The
   simulation is aborted if the property was present but of a
   conflicting type.

   FIND returns the specified properties value, aborting the
   simulation if the property is missing.  Code locating a property
   should first check its type (using device_find_property above) and
   then obtain its value using the below.

   void device_add_<type>_property(device *, const char *, <type>)
   void device_add_*_array_property(device *, const char *, const <type>*, int)
   void device_set_*_property(device *, const char *, <type>)
   void device_set_*_array_property(device *, const char *, const <type>*, int)
   <type> device_find_*_property(device *, const char *)
   int device_find_*_array_property(device *, const char *, int, <type>*)

   */


INLINE_DEVICE\
(void) device_add_array_property
(device *me,
 const char *property,
 const void *array,
 int sizeof_array);

INLINE_DEVICE\
(void) device_set_array_property
(device *me,
 const char *property,
 const void *array,
 int sizeof_array);

INLINE_DEVICE\
(const device_property *) device_find_array_property
(device *me,
 const char *property);



INLINE_DEVICE\
(void) device_add_boolean_property
(device *me,
 const char *property,
 int bool);

INLINE_DEVICE\
(int) device_find_boolean_property
(device *me,
 const char *property);



typedef struct _ihandle_runtime_property_spec {
  const char *full_path;
} ihandle_runtime_property_spec;

INLINE_DEVICE\
(void) device_add_ihandle_runtime_property
(device *me,
 const char *property,
 const ihandle_runtime_property_spec *ihandle);

INLINE_DEVICE\
(void) device_find_ihandle_runtime_property
(device *me,
 const char *property,
 ihandle_runtime_property_spec *ihandle);

INLINE_DEVICE\
(void) device_set_ihandle_property
(device *me,
 const char *property,
 device_instance *ihandle);

INLINE_DEVICE\
(device_instance *) device_find_ihandle_property
(device *me,
 const char *property);



INLINE_DEVICE\
(void) device_add_integer_property
(device *me,
 const char *property,
 signed_cell integer);

INLINE_DEVICE\
(signed_cell) device_find_integer_property
(device *me,
 const char *property);

INLINE_DEVICE\
(int) device_find_integer_array_property
(device *me,
 const char *property,
 unsigned index,
 signed_cell *integer);



typedef struct _range_property_spec {
  device_unit child_address;
  device_unit parent_address;
  device_unit size;
} range_property_spec;

INLINE_DEVICE\
(void) device_add_range_array_property
(device *me,
 const char *property,
 const range_property_spec *ranges,
 unsigned nr_ranges);

INLINE_DEVICE\
(int) device_find_range_array_property
(device *me,
 const char *property,
 unsigned index,
 range_property_spec *range);



typedef struct _reg_property_spec {
  device_unit address;
  device_unit size;
} reg_property_spec;

INLINE_DEVICE\
(void) device_add_reg_array_property
(device *me,
 const char *property,
 const reg_property_spec *reg,
 unsigned nr_regs);

INLINE_DEVICE\
(int) device_find_reg_array_property
(device *me,
 const char *property,
 unsigned index,
 reg_property_spec *reg);



INLINE_DEVICE\
(void) device_add_string_property
(device *me,
 const char *property,
 const char *string);

INLINE_DEVICE\
(const char *) device_find_string_property
(device *me,
 const char *property);



typedef const char *string_property_spec;

INLINE_DEVICE\
(void) device_add_string_array_property
(device *me,
 const char *property,
 const string_property_spec *strings,
 unsigned nr_strings);

INLINE_DEVICE\
(int) device_find_string_array_property
(device *me,
 const char *property,
 unsigned index,
 string_property_spec *string);



INLINE_DEVICE\
(void) device_add_duplicate_property
(device *me,
 const char *property,
 const device_property *original);



/* Instances:

   As with IEEE1275, a device can be opened, creating an instance.
   Instances provide more abstract interfaces to the underlying
   hardware.  For example, the instance methods for a disk may include
   code that is able to interpret file systems found on disks.  Such
   methods would there for allow the manipulation of files on the
   disks file system.  The operations would be implemented using the
   basic block I/O model provided by the disk.

   This model includes methods that faciliate the creation of device
   instance and (should a given device support it) standard operations
   on those instances.

   */

typedef struct _device_instance_callbacks device_instance_callbacks;

INLINE_DEVICE\
(device_instance *) device_create_instance_from
(device *me, /*OR*/ device_instance *parent,
 void *data,
 const char *path,
 const char *args,
 const device_instance_callbacks *callbacks);

INLINE_DEVICE\
(device_instance *) device_create_instance
(device *me,
 const char *full_path,
 const char *args);

INLINE_DEVICE\
(void) device_instance_delete
(device_instance *instance);

INLINE_DEVICE\
(int) device_instance_read
(device_instance *instance,
 void *addr,
 unsigned_word len);

INLINE_DEVICE\
(int) device_instance_write
(device_instance *instance,
 const void *addr,
 unsigned_word len);

INLINE_DEVICE\
(int) device_instance_seek
(device_instance *instance,
 unsigned_word pos_hi,
 unsigned_word pos_lo);

INLINE_DEVICE\
(int) device_instance_call_method
(device_instance *instance,
 const char *method,
 int n_stack_args,
 unsigned_cell stack_args[/*n_stack_args*/],
 int n_stack_returns,
 unsigned_cell stack_returns[/*n_stack_returns*/]);

INLINE_DEVICE\
(device *) device_instance_device
(device_instance *instance);

INLINE_DEVICE\
(const char *) device_instance_path
(device_instance *instance);

INLINE_DEVICE\
(void *) device_instance_data
(device_instance *instance);


/* Interrupts:

   */

/* Interrupt Source

   A device drives its interrupt line using the call

   */

INLINE_DEVICE\
(void) device_interrupt_event
(device *me,
 int my_port,
 int value,
 cpu *processor,
 unsigned_word cia);

/* This interrupt event will then be propogated to any attached
   interrupt destinations.

   Any interpretation of PORT and VALUE is model dependant.  However
   as guidelines the following are recommended: PCI interrupts a-d
   correspond to lines 0-3; level sensative interrupts be requested
   with a value of one and withdrawn with a value of 0; edge sensative
   interrupts always have a value of 1, the event its self is treated
   as the interrupt.


   Interrupt Destinations

   Attached to each interrupt line of a device can be zero or more
   desitinations.  These destinations consist of a device/port pair.
   A destination is attached/detached to a device line using the
   attach and detach calls. */

INLINE_DEVICE\
(void) device_interrupt_attach
(device *me,
 int my_port,
 device *dest,
 int dest_port,
 object_disposition disposition);

INLINE_DEVICE\
(void) device_interrupt_detach
(device *me,
 int my_port,
 device *dest,
 int dest_port);

typedef void (device_interrupt_traverse_function)
     (device *me,
      int my_port,
      device *dest,
      int my_dest,
      void *data);

INLINE_DEVICE\
(void) device_interrupt_traverse
(device *me,
 device_interrupt_traverse_function *handler,
 void *data);
 

/* DESTINATION is attached (detached) to LINE of the device ME


   Interrupt conversion

   Users refer to interrupt port numbers symbolically.  For instance a
   device may refer to its `INT' signal which is internally
   represented by port 3.

   To convert to/from the symbolic and internal representation of a
   port name/number.  The following functions are available. */

INLINE_DEVICE\
(int) device_interrupt_decode
(device *me,
 const char *symbolic_name,
 port_direction direction);

INLINE_DEVICE\
(int) device_interrupt_encode
(device *me,
 int port_number,
 char *buf,
 int sizeof_buf,
 port_direction direction);
 

/* Hardware operations:

   */

INLINE_DEVICE\
(unsigned) device_io_read_buffer
(device *me,
 void *dest,
 int space,
 unsigned_word addr,
 unsigned nr_bytes,
 cpu *processor,
 unsigned_word cia);

INLINE_DEVICE\
(unsigned) device_io_write_buffer
(device *me,
 const void *source,
 int space,
 unsigned_word addr,
 unsigned nr_bytes,
 cpu *processor,
 unsigned_word cia);


/* Conversly, the device pci1000,1@1 my need to perform a dma transfer
   into the cpu/memory core.  Just as I/O moves towards the leaves,
   dma transfers move towards the core via the initiating devices
   parent nodes.  The root device (special) converts the DMA transfer
   into reads/writes to memory */

INLINE_DEVICE\
(unsigned) device_dma_read_buffer
(device *me,
 void *dest,
 int space,
 unsigned_word addr,
 unsigned nr_bytes);

INLINE_DEVICE\
(unsigned) device_dma_write_buffer
(device *me,
 const void *source,
 int space,
 unsigned_word addr,
 unsigned nr_bytes,
 int violate_read_only_section);

/* To avoid the need for an intermediate (bridging) node to ask each
   of its child devices in turn if an IO access is intended for them,
   parent nodes maintain a table mapping addresses directly to
   specific devices.  When a device is `connected' to its bus it
   attaches its self to its parent. */

/* Address access attributes */
typedef enum _access_type {
  access_invalid = 0,
  access_read = 1,
  access_write = 2,
  access_read_write = 3,
  access_exec = 4,
  access_read_exec = 5,
  access_write_exec = 6,
  access_read_write_exec = 7,
} access_type;

/* Address attachement types */
typedef enum _attach_type {
  attach_invalid,
  attach_raw_memory,
  attach_callback,
  /* ... */
} attach_type;

INLINE_DEVICE\
(void) device_attach_address
(device *me,
 attach_type attach,
 int space,
 unsigned_word addr,
 unsigned nr_bytes,
 access_type access,
 device *client); /*callback/default*/

INLINE_DEVICE\
(void) device_detach_address
(device *me,
 attach_type attach,
 int space,
 unsigned_word addr,
 unsigned nr_bytes,
 access_type access,
 device *client); /*callback/default*/

/* Utilities:

   */

/* IOCTL::

   Often devices require `out of band' operations to be performed.
   For instance a pal device may need to notify a PCI bridge device
   that an interrupt ack cycle needs to be performed on the PCI bus.
   Within PSIM such operations are performed by using the generic
   ioctl call <<device_ioctl()>>.

   */

typedef enum {
  device_ioctl_break, /* unsigned_word requested_break */
  device_ioctl_set_trace, /* void */
  device_ioctl_create_stack, /* unsigned_word *sp, char **argv, char **envp */
  device_ioctl_change_media, /* const char *new_image (possibly NULL) */
  nr_device_ioctl_requests,
} device_ioctl_request;

EXTERN_DEVICE\
(int) device_ioctl
(device *me,
 cpu *processor,
 unsigned_word cia,
 device_ioctl_request request,
 ...);


/* Error reporting::

   So that errors originating from devices appear in a consistent
   format, the <<device_error()>> function can be used.  Formats and
   outputs the error message before aborting the simulation

   Devices should use this function to abort the simulation except
   when the abort reason leaves the simulation in a hazardous
   condition (for instance a failed malloc).

   */

EXTERN_DEVICE\
(void volatile) device_error
(device *me,
 const char *fmt,
 ...) __attribute__ ((format (printf, 2, 3)));

INLINE_DEVICE\
(int) device_trace
(device *me);



/* External representation:

   Both device nodes and device instances, in OpenBoot firmware have
   an external representation (phandles and ihandles) and these values
   are both stored in the device tree in property nodes and passed
   between the client program and the simulator during emulation
   calls.

   To limit the potential risk associated with trusing `data' from the
   client program, the following mapping operators `safely' convert
   between the two representations

   */

INLINE_DEVICE\
(device *) external_to_device
(device *tree_member,
 unsigned_cell phandle);

INLINE_DEVICE\
(unsigned_cell) device_to_external
(device *me);

INLINE_DEVICE\
(device_instance *) external_to_device_instance
(device *tree_member,
 unsigned_cell ihandle);

INLINE_DEVICE\
(unsigned_cell) device_instance_to_external
(device_instance *me);


/* Event queue:

   The device inherets certain event queue operations from the main
   simulation. */

typedef void device_event_handler(void *data);

INLINE_DEVICE\
(event_entry_tag) device_event_queue_schedule
(device *me,
 signed64 delta_time,
 device_event_handler *handler,
 void *data);

INLINE_EVENTS\
(void) device_event_queue_deschedule
(device *me,
 event_entry_tag event_to_remove);

INLINE_EVENTS\
(signed64) device_event_queue_time
(device *me);

#endif /* _DEVICE_H_ */
