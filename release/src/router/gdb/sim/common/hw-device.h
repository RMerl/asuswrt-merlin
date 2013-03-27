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


#ifndef HW_DEVICE_H
#define HW_DEVICE_H

/* declared in sim-basics.h, this object is used everywhere */
/* typedef struct _device device; */


/* Introduction:

   As explained in earlier sections, the device, device instance,
   property and ports lie at the heart of PSIM's device model.

   In the below a synopsis of the device object and the operations it
   supports are given.
   */


/* Creation:

   The devices are created using a sequence of steps.  In particular:

	o	A tree framework is created.

		At this point, properties can be modified and extra
		devices inserted (or removed?).

#if LATER

		Any properties that have a run-time value (eg ihandle
		or device instance pointer properties) are entered
		into the device tree using a named reference to the
		corresponding runtime object that is to be created.

#endif

	o	Real devices are created for all the dummy devices.

		A device can assume that all of its parents have been
		initialized.

		A device can assume that all non run-time properties
		have been initialized.

		As part of being created, the device normally attaches
		itself to its parent bus.

#if LATER

		Device instance data is initialized.

#endif

#if LATER

	o	Any run-time properties are created.

#endif

#if MUCH_MUCH_LATER

	o	Some devices, as part of their initialization
		might want to refer to ihandle properties
		in the device tree.

#endif

   NOTES:

	o	It is important to separate the creation
		of an actual device from the creation
		of the tree.  The alternative creating
		the device in two stages: As a separate
		entity and then as a part of the tree.

#if LATER
	o	Run-time properties can not be created
		until after the devices in the tree
		have been created.  Hence an extra pass
		for handling them.
#endif

   */

/* Relationships:

   A device is able to determine its relationship to other devices
   within the tree.  Operations include querying for a devices parent,
   sibling, child, name, and path (from the root).

   */


#define hw_parent(hw) ((hw)->parent_of_hw + 0)

#define hw_sibling(hw) ((hw)->sibling_of_hw + 0)

#define hw_child(hw) ((hw)->child_of_hw + 0)



/* Herritage:

 */

#define hw_family(hw) ((hw)->family_of_hw + 0)

#define hw_name(hw) ((hw)->name_of_hw + 0)

#define hw_args(hw) ((hw)->args_of_hw + 0)

#define hw_path(hw) ((hw)->path_of_hw + 0)



/* Short cut to the root node of the tree */

#define hw_root(hw) ((hw)->root_of_hw + 0)

/* Short cut back to the simulator object */

#define hw_system(hw) ((hw)->system_of_hw)

/* For requests initiated by a CPU the cpu that initiated the request */

struct _sim_cpu *hw_system_cpu (struct hw *hw);


/* Device private data */

#define hw_data(hw) ((hw)->data_of_hw)

#define set_hw_data(hw, value) \
((hw)->data_of_hw = (value))



/* Perform a soft reset of the device */

typedef unsigned (hw_reset_method)
     (struct hw *me);

#define hw_reset(hw) ((hw)->to_reset (hw))

#define set_hw_reset(hw, method) \
((hw)->to_reset = method)


/* Hardware operations:

   Connecting a parent to its children is a common bus. The parent
   node is described as the bus owner and is responisble for
   co-ordinating bus operations. On the bus, a SPACE:ADDR pair is used
   to specify an address.  A device that is both a bus owner (parent)
   and bus client (child) are referred to as a bridging device.

   A child performing a data (DMA) transfer will pass its request to
   the bus owner (the devices parent).  The bus owner will then either
   reflect the request to one of the other devices attached to the bus
   (a child of the bus owner) or bridge the request up the tree to the
   next bus. */


/* Children attached to a bus can register (attach) themselves to
   specific addresses on their attached bus.

   (A device may also be implicitly attached to certain bus
   addresses).

   The SPACE:ADDR pair specify an address on the common bus that
   connects the parent and child devices. */

typedef void (hw_attach_address_method)
     (struct hw *me,
      int level,
      int space,
      address_word addr,
      address_word nr_bytes,
      struct hw *client); /*callback/default*/

#define hw_attach_address(me, level, space, addr, nr_bytes, client) \
((me)->to_attach_address (me, level, space, addr, nr_bytes, client))

#define set_hw_attach_address(hw, method) \
((hw)->to_attach_address = (method))

typedef void (hw_detach_address_method)
     (struct hw *me,
      int level,
      int space,
      address_word addr,
      address_word nr_bytes,
      struct hw *client); /*callback/default*/

#define hw_detach_address(me, level, space, addr, nr_bytes, client) \
((me)->to_detach_address (me, level, space, addr, nr_bytes, client))

#define set_hw_detach_address(hw, method) \
((hw)->to_detach_address = (method))


/* An IO operation from a parent to a child via the conecting bus.

   The SPACE:ADDR pair specify an address on the bus shared between
   the parent and child devices. */

typedef unsigned (hw_io_read_buffer_method)
     (struct hw *me,
      void *dest,
      int space,
      unsigned_word addr,
      unsigned nr_bytes);

#define hw_io_read_buffer(hw, dest, space, addr, nr_bytes) \
((hw)->to_io_read_buffer (hw, dest, space, addr, nr_bytes))

#define set_hw_io_read_buffer(hw, method) \
((hw)->to_io_read_buffer = (method))

typedef unsigned (hw_io_write_buffer_method)
     (struct hw *me,
      const void *source,
      int space,
      unsigned_word addr,
      unsigned nr_bytes);

#define hw_io_write_buffer(hw, src, space, addr, nr_bytes) \
((hw)->to_io_write_buffer (hw, src, space, addr, nr_bytes))

#define set_hw_io_write_buffer(hw, method) \
((hw)->to_io_write_buffer = (method))


/* Conversly, the device pci1000,1@1 may need to perform a dma transfer
   into the cpu/memory core.  Just as I/O moves towards the leaves,
   dma transfers move towards the core via the initiating devices
   parent nodes.  The root device (special) converts the DMA transfer
   into reads/writes to memory.

   The SPACE:ADDR pair specify an address on the common bus connecting
   the parent and child devices. */

typedef unsigned (hw_dma_read_buffer_method)
     (struct hw *bus,
      void *dest,
      int space,
      unsigned_word addr,
      unsigned nr_bytes);

#define hw_dma_read_buffer(bus, dest, space, addr, nr_bytes) \
((bus)->to_dma_read_buffer (bus, dest, space, addr, nr_bytes))

#define set_hw_dma_read_buffer(me, method) \
((me)->to_dma_read_buffer = (method))

typedef unsigned (hw_dma_write_buffer_method)
     (struct hw *bus,
      const void *source,
      int space,
      unsigned_word addr,
      unsigned nr_bytes,
      int violate_read_only_section);

#define hw_dma_write_buffer(bus, src, space, addr, nr_bytes, violate_ro) \
((bus)->to_dma_write_buffer (bus, src, space, addr, nr_bytes, violate_ro))

#define set_hw_dma_write_buffer(me, method) \
((me)->to_dma_write_buffer = (method))

/* Address/size specs for devices are encoded following a convention
   similar to that used by OpenFirmware.  In particular, an
   address/size is packed into a sequence of up to four cell words.
   The number of words determined by the number of {address,size}
   cells attributes of the device. */

typedef struct _hw_unit {
  int nr_cells;
  unsigned_cell cells[4]; /* unused cells are zero */
} hw_unit;


/* For the given bus, the number of address and size cells used in a
   hw_unit. */

#define hw_unit_nr_address_cells(bus) ((bus)->nr_address_cells_of_hw_unit + 0)

#define hw_unit_nr_size_cells(bus) ((bus)->nr_size_cells_of_hw_unit + 0)


/* For the given device, its identifying hw_unit address.

   Each device has an identifying hw_unit address.  That address is
   used when identifying one of a number of identical devices on a
   common controller bus. ex fd0&fd1. */

const hw_unit *hw_unit_address
(struct hw *me);


/* Convert between a textual and the internal representation of a
   hw_unit address/size.

   NOTE: A device asks its parent to translate between a hw_unit and
   textual representation.  This is because the textual address of a
   device is specified using the parent busses notation. */

typedef int (hw_unit_decode_method)
     (struct hw *bus,
      const char *encoded,
      hw_unit *unit);

#define hw_unit_decode(bus, encoded, unit) \
((bus)->to_unit_decode (bus, encoded, unit))

#define set_hw_unit_decode(hw, method) \
((hw)->to_unit_decode = (method))

typedef int (hw_unit_encode_method)
     (struct hw *bus,
      const hw_unit *unit,
      char *encoded,
      int sizeof_buf);
     
#define hw_unit_encode(bus, unit, encoded, sizeof_encoded) \
((bus)->to_unit_encode (bus, unit, encoded, sizeof_encoded))

#define set_hw_unit_encode(hw, method) \
((hw)->to_unit_encode = (method))


/* As the bus that the device is attached too, to translate a devices
   hw_unit address/size into a form suitable for an attach address
   call.

   Return a zero result if the address should be ignored when looking
   for attach addresses. */

typedef int (hw_unit_address_to_attach_address_method)
     (struct hw *bus,
      const hw_unit *unit_addr,
      int *attach_space,
      unsigned_word *attach_addr,
      struct hw *client);

#define hw_unit_address_to_attach_address(bus, unit_addr, attach_space, attach_addr, client) \
((bus)->to_unit_address_to_attach_address (bus, unit_addr, attach_space, attach_addr, client))

#define set_hw_unit_address_to_attach_address(hw, method) \
((hw)->to_unit_address_to_attach_address = (method))

typedef int (hw_unit_size_to_attach_size_method)
     (struct hw *bus,
      const hw_unit *unit_size,
      unsigned *attach_size,
      struct hw *client);

#define hw_unit_size_to_attach_size(bus, unit_size, attach_size, client) \
((bus)->to_unit_size_to_attach_size (bus, unit_size, attach_size, client))

#define set_hw_unit_size_to_attach_size(hw, method) \
((hw)->to_unit_size_to_attach_size = (method))


extern char *hw_strdup (struct hw *me, const char *str);


/* Utilities:

   */

/* IOCTL::

   Often devices require `out of band' operations to be performed.
   For instance a pal device may need to notify a PCI bridge device
   that an interrupt ack cycle needs to be performed on the PCI bus.
   Within PSIM such operations are performed by using the generic
   ioctl call <<hw_ioctl()>>.

   */

typedef enum {
  hw_ioctl_break, /* unsigned_word requested_break */
  hw_ioctl_set_trace, /* void */
  hw_ioctl_create_stack, /* unsigned_word *sp, char **argv, char **envp */
  hw_ioctl_change_media, /* const char *new_image (possibly NULL) */
  nr_hw_ioctl_requests,
} hw_ioctl_request;

typedef int (hw_ioctl_method)
     (struct hw *me,
      hw_ioctl_request request,
      va_list ap);

int hw_ioctl
(struct hw *me,
 hw_ioctl_request request,
 ...);


/* Error reporting::

   So that errors originating from devices appear in a consistent
   format, the <<hw_abort()>> function can be used.  Formats and
   outputs the error message before aborting the simulation

   Devices should use this function to abort the simulation except
   when the abort reason leaves the simulation in a hazardous
   condition (for instance a failed malloc).

   */

void hw_abort
(struct hw *me,
 const char *fmt,
 ...) __attribute__ ((format (printf, 2, 3)));

void hw_vabort
(struct hw *me,
 const char *fmt,
 va_list ap);

void hw_halt
(struct hw *me,
 int reason,
 int status);


#define hw_trace_p(hw) ((hw)->trace_of_hw_p + 0)

void hw_trace
(struct hw *me,
 const char *fmt,
 ...) __attribute__ ((format (printf, 2, 3)));

#define HW_TRACE(ARGS) \
do { \
  if (hw_trace_p (me)) \
    { \
      hw_trace ARGS; \
    } \
} while (0)


/* Some of the related functions require specific types */

struct hw_property_data;
struct hw_port_data;
struct hw_base_data;
struct hw_alloc_data;
struct hw_event_data;
struct hw_handle_data;
struct hw_instance_data;

/* Finally the hardware device - keep your grubby little mits off of
   these internals! :-) */

struct hw {

  /* our relatives */
  struct hw *parent_of_hw;
  struct hw *sibling_of_hw;
  struct hw *child_of_hw;

  /* our identity */
  const char *name_of_hw;
  const char *family_of_hw;
  const char *args_of_hw;
  const char *path_of_hw;

  /* our data */
  void *data_of_hw;

  /* hot links */
  struct hw *root_of_hw;
  struct sim_state *system_of_hw;

  /* identifying data */
  hw_unit unit_address_of_hw;
  int nr_address_cells_of_hw_unit;
  int nr_size_cells_of_hw_unit;

  /* Soft reset */
  hw_reset_method *to_reset;

  /* Basic callbacks */
  hw_io_read_buffer_method *to_io_read_buffer;
  hw_io_write_buffer_method *to_io_write_buffer;
  hw_dma_read_buffer_method *to_dma_read_buffer;
  hw_dma_write_buffer_method *to_dma_write_buffer;
  hw_attach_address_method *to_attach_address;
  hw_detach_address_method *to_detach_address;

  /* More complicated callbacks */
  hw_ioctl_method *to_ioctl;
  int trace_of_hw_p;

  /* address callbacks */
  hw_unit_decode_method *to_unit_decode;
  hw_unit_encode_method *to_unit_encode;
  hw_unit_address_to_attach_address_method *to_unit_address_to_attach_address;
  hw_unit_size_to_attach_size_method *to_unit_size_to_attach_size;

  /* related data */
  struct hw_property_data *properties_of_hw;
  struct hw_port_data *ports_of_hw;
  struct hw_base_data *base_of_hw;
  struct hw_alloc_data *alloc_of_hw;
  struct hw_event_data *events_of_hw;
  struct hw_handle_data *handles_of_hw;
  struct hw_instance_data *instances_of_hw;

};


#endif
