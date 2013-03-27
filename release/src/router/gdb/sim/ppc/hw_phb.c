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


#ifndef _HW_PHB_C_
#define _HW_PHB_C_

#include "device_table.h"

#include "hw_phb.h"

#include "corefile.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <ctype.h>


/* DEVICE


   phb - PCI Host Bridge


   DESCRIPTION


   PHB implements a model of the PCI-host bridge described in the PPCP
   document.

   For bridge devices, Open Firmware specifies that the <<ranges>>
   property be used to specify the mapping of address spaces between a
   bridges parent and child busses.  This PHB model configures itsself
   according to the information specified in its ranges property.  The
   <<ranges>> property is described in detail in the Open Firmware
   documentation.

   For DMA transfers, any access to a PCI address space which falls
   outside of the mapped memory space is assumed to be a transfer
   intended for the parent bus.


   PROPERTIES


   ranges = <my-phys-addr> <parent-phys-addr> <my-size> ...  (required)
   
   Define a number of mappings from the parent bus to one of this
   devices PCI busses.  The exact format of the <<parent-phys-addr>>
   is parent bus dependant.  The format of <<my-phys-addr>> is
   described in the Open Firmware PCI bindings document (note that the
   address must be non-relocatable).


   #address-cells = 3  (required)

   Number of cells used by an Open Firmware PCI address.  This
   property must be defined before specifying the <<ranges>> property.


   #size-cells = 2  (required)

   Number of cells used by an Open Firmware PCI size.  This property
   must be defined before specifying the <<ranges>> property.


   EXAMPLES
   

   Enable tracing:

   |  $ psim \
   |    -t phb-device \


   Since device tree entries that are specified on the command line
   are added before most of the device tree has been built it is often
   necessary to explictly add certain device properties and thus
   ensure they are already present in the device tree.  For the
   <<phb>> one such property is parent busses <<#address-cells>>.

   |    -o '/#address-cells 1' \


   Create the PHB remembering to include the cell size properties:
   
   |    -o '/phb@0x80000000/#address-cells 3' \
   |    -o '/phb@0x80000000/#size-cells 2' \


   Specify that the memory address range <<0x80000000>> to
   <<0x8fffffff>> should map directly onto the PCI memory address
   space while the processor address range <<0xc0000000>> to
   <<0xc000ffff>> should map onto the PCI I/O address range starting
   at location zero:

   |    -o '/phb@0x80000000/ranges \
   |                nm0,0,0,80000000 0x80000000 0x10000000 \
   |                ni0,0,0,0 0xc0000000 0x10000' \


   Insert a 4k <<nvram>> into slot zero of the PCI bus.  Have it
   directly accessible in both the I/O (address <<0x100>>) and memory
   (address 0x80001000) spaces:

   |    -o '/phb@0x80000000/nvram@0/assigned-addresses \
   |                nm0,0,10,80001000 4096 \
   |                ni0,0,14,100 4096'
   |    -o '/phb@0x80000000/nvram@0/reg \
   |                0 0 \
   |                i0,0,14,0 4096'
   |    -o '/phb@0x80000000/nvram@0/alternate-reg \
   |                0 0 \
   |                m0,0,10,0 4096'

   The <<assigned-address>> property corresponding to what (if it were
   implemented) be found in the config base registers while the
   <<reg>> and <<alternative-reg>> properties indicating the location
   of registers within each address space.

   Of the possible addresses, only the non-relocatable versions are
   used when attaching the device to the bus.


   BUGS
   

   The implementation of the PCI configuration space is left as an
   exercise for the reader.  Such a restriction should only impact on
   systems wanting to dynamically configure devices on the PCI bus.

   The <<CHRP>> document specfies additional (optional) functionality
   of the primary PHB. The implementation of such functionality is
   left as an exercise for the reader.

   The Open Firmware PCI bus bindings document (rev 1.6 and 2.0) is
   unclear on the value of the "ss" bits for a 64bit memory address.
   The correct value, as used by this module, is 0b11.
   
   The Open Firmware PCI bus bindings document (rev 1.6) suggests that
   the register field of non-relocatable PCI address should be zero.
   Unfortunatly, PCI addresses specified in the <<assigned-addresses>>
   property must be both non-relocatable and have non-zero register
   fields.

   The unit-decode method is not inserting a bus number into any
   address that it decodes.  Instead the bus-number is left as zero.

   Support for aliased memory and I/O addresses is left as an exercise
   for the reader.

   Support for interrupt-ack and special cycles are left as an
   exercise for the reader.  One issue to consider when attempting
   this exercise is how to specify the address of the int-ack and
   special cycle register.  Hint: <</8259-interrupt-ackowledge>> is
   the wrong answer.

   Children of this node can only use the client callback interface
   when attaching themselves to the <<phb>>.

   
   REFERENCES


   http://playground.sun.com/1275/home.html#OFDbusPCI


   */

   
typedef struct _phb_space {
  core *map;
  core_map *readable;
  core_map *writeable;
  unsigned_word parent_base;
  int parent_space;
  unsigned_word my_base;
  int my_space;
  unsigned size;	
  const char *name;
} phb_space;

typedef struct _hw_phb_device  {
  phb_space space[nr_hw_phb_spaces];
} hw_phb_device;


static const char *
hw_phb_decode_name(hw_phb_decode level)
{
  switch (level) {
  case hw_phb_normal_decode: return "normal";
  case hw_phb_subtractive_decode: return "subtractive";
  case hw_phb_master_abort_decode: return "master-abort";
  default: return "invalid decode";
  }
}


static void
hw_phb_init_address(device *me)
{
  hw_phb_device *phb = device_data(me);
 
  /* check some basic properties */
  if (device_nr_address_cells(me) != 3)
    device_error(me, "incorrect #address-cells");
  if (device_nr_size_cells(me) != 2)
    device_error(me, "incorrect #size-cells");

  /* (re) initialize each PCI space */
  {
    hw_phb_spaces space_nr;
    for (space_nr = 0; space_nr < nr_hw_phb_spaces; space_nr++) {
      phb_space *pci_space = &phb->space[space_nr];
      core_init(pci_space->map);
      pci_space->size = 0;
    }
  }

  /* decode each of the ranges properties entering the information
     into the space table */
  {
    range_property_spec range;
    int ranges_entry;
    
    for (ranges_entry = 0;
	 device_find_range_array_property(me, "ranges", ranges_entry,
					  &range);
	 ranges_entry++) {
      int my_attach_space;
      unsigned_word my_attach_address;
      int parent_attach_space;
      unsigned_word parent_attach_address;
      unsigned size;
      phb_space *pci_space;
      /* convert the addresses into something meaningful */
      device_address_to_attach_address(me, &range.child_address,
				       &my_attach_space,
				       &my_attach_address,
				       me);
      device_address_to_attach_address(device_parent(me),
				       &range.parent_address,
				       &parent_attach_space,
				       &parent_attach_address,
				       me);
      device_size_to_attach_size(me, &range.size, &size, me);
      if (my_attach_space < 0 || my_attach_space >= nr_hw_phb_spaces)
	device_error(me, "ranges property contains an invalid address space");
      pci_space = &phb->space[my_attach_space];
      if (pci_space->size != 0)
	device_error(me, "ranges property contains duplicate mappings for %s address space",
		     pci_space->name);
      pci_space->parent_base = parent_attach_address;
      pci_space->parent_space = parent_attach_space;
      pci_space->my_base = my_attach_address;
      pci_space->my_space = my_attach_space;
      pci_space->size = size;
      device_attach_address(device_parent(me),
			    attach_callback,
			    parent_attach_space, parent_attach_address, size,
			    access_read_write_exec,
			    me);
      DTRACE(phb, ("map %d:0x%lx to %s:0x%lx (0x%lx bytes)\n",
		   (int)parent_attach_space,
		   (unsigned long)parent_attach_address,
		   pci_space->name,
		   (unsigned long)my_attach_address,
		   (unsigned long)size));
    }
    
    if (ranges_entry == 0) {
      device_error(me, "Missing or empty ranges property");
    }

  }
  
}

static void
hw_phb_attach_address(device *me,
		      attach_type type,
		      int space,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      access_type access,
		      device *client) /*callback/default*/
{
  hw_phb_device *phb = device_data(me);
  phb_space *pci_space;
  /* sanity checks */
  if (space < 0 || space >= nr_hw_phb_spaces)
    device_error(me, "attach space (%d) specified by %s invalid",
		 space, device_path(client));
  pci_space = &phb->space[space];
  if (addr + nr_bytes > pci_space->my_base + pci_space->size
      || addr < pci_space->my_base)
    device_error(me, "attach addr (0x%lx) specified by %s outside of bus address range",
		 (unsigned long)addr, device_path(client));
  if (type != hw_phb_normal_decode
      && type != hw_phb_subtractive_decode)
    device_error(me, "attach type (%d) specified by %s invalid",
		 type, device_path(client));
  /* attach it to the relevent bus */
  DTRACE(phb, ("attach %s - %s %s:0x%lx (0x%lx bytes)\n",
	       device_path(client),
	       hw_phb_decode_name(type),
	       pci_space->name,
	       (unsigned long)addr,
	       (unsigned long)nr_bytes));
  core_attach(pci_space->map,
	      type,
	      space,
	      access,
	      addr,
	      nr_bytes,
	      client);
}


/* Extract/set various fields from a PCI unit address.

   Note: only the least significant 32 bits of each cell is used.

   Note: for PPC MSB is 0 while for PCI it is 31. */


/* relocatable bit n */

static unsigned
extract_n(const device_unit *address)
{
  return EXTRACTED32(address->cells[0], 0, 0);
}

static void
set_n(device_unit *address)
{
  BLIT32(address->cells[0], 0, 1);
}


/* prefetchable bit p */

static unsigned
extract_p(const device_unit *address)
{
  ASSERT(address->nr_cells == 3);
  return EXTRACTED32(address->cells[0], 1, 1);
}

static void
set_p(device_unit *address)
{
  BLIT32(address->cells[0], 1, 1);
}


/* aliased bit t */

static unsigned
extract_t(const device_unit *address)
{
  ASSERT(address->nr_cells == 3);
  return EXTRACTED32(address->cells[0], 2, 2);
}

static void
set_t(device_unit *address)
{
  BLIT32(address->cells[0], 2, 1);
}


/* space code ss */

typedef enum {
  ss_config_code = 0,
  ss_io_code = 1,
  ss_32bit_memory_code = 2,
  ss_64bit_memory_code = 3,
} ss_type;

static ss_type
extract_ss(const device_unit *address)
{
  ASSERT(address->nr_cells == 3);
  return EXTRACTED32(address->cells[0], 6, 7);
}

static void
set_ss(device_unit *address, ss_type val)
{
  MBLIT32(address->cells[0], 6, 7, val);
}


/* bus number bbbbbbbb */

#if 0
static unsigned
extract_bbbbbbbb(const device_unit *address)
{
  ASSERT(address->nr_cells == 3);
  return EXTRACTED32(address->cells[0], 8, 15);
}
#endif

#if 0
static void
set_bbbbbbbb(device_unit *address, unsigned val)
{
  MBLIT32(address->cells[0], 8, 15, val);
}
#endif


/* device number ddddd */

static unsigned
extract_ddddd(const device_unit *address)
{
  ASSERT(address->nr_cells == 3);
  return EXTRACTED32(address->cells[0], 16, 20);
}

static void
set_ddddd(device_unit *address, unsigned val)
{
  MBLIT32(address->cells[0], 16, 20, val);
}


/* function number fff */

static unsigned
extract_fff(const device_unit *address)
{
  ASSERT(address->nr_cells == 3);
  return EXTRACTED32(address->cells[0], 21, 23);
}

static void
set_fff(device_unit *address, unsigned val)
{
  MBLIT32(address->cells[0], 21, 23, val);
}


/* register number rrrrrrrr */

static unsigned
extract_rrrrrrrr(const device_unit *address)
{
  ASSERT(address->nr_cells == 3);
  return EXTRACTED32(address->cells[0], 24, 31);
}

static void
set_rrrrrrrr(device_unit *address, unsigned val)
{
  MBLIT32(address->cells[0], 24, 31, val);
}


/* MSW of 64bit address hh..hh */

static unsigned
extract_hh_hh(const device_unit *address)
{
  ASSERT(address->nr_cells == 3);
  return address->cells[1];
}

static void
set_hh_hh(device_unit *address, unsigned val)
{
  address->cells[2] = val;
}


/* LSW of 64bit address ll..ll */

static unsigned
extract_ll_ll(const device_unit *address)
{
  ASSERT(address->nr_cells == 3);
  return address->cells[2];
}

static void
set_ll_ll(device_unit *address, unsigned val)
{
  address->cells[2] = val;
}


/* Convert PCI textual bus address into a device unit */

static int
hw_phb_unit_decode(device *me,
		   const char *unit,
		   device_unit *address)
{
  char *end = NULL;
  const char *chp = unit;
  unsigned long val;

  if (device_nr_address_cells(me) != 3)
    device_error(me, "PCI bus should have #address-cells == 3");
  memset(address, 0, sizeof(*address));

  if (unit == NULL)
    return 0;

  address->nr_cells = 3;

  if (isxdigit(*chp)) {
    set_ss(address, ss_config_code);
  }
  else {

    /* non-relocatable? */
    if (*chp == 'n') {
      set_n(address);
      chp++;
    }

    /* address-space? */
    if (*chp == 'i') {
      set_ss(address, ss_io_code);
      chp++;
    }
    else if (*chp == 'm') {
      set_ss(address, ss_32bit_memory_code);
      chp++;
    }
    else if (*chp == 'x') {
      set_ss(address, ss_64bit_memory_code);
      chp++;
    }
    else
      device_error(me, "Problem parsing PCI address %s", unit);

    /* possible alias */
    if (*chp == 't') {
      if (extract_ss(address) == ss_64bit_memory_code)
	device_error(me, "Invalid alias bit in PCI address %s", unit);
      set_t(address);
      chp++;
    }

    /* possible p */
    if (*chp == 'p') {
      if (extract_ss(address) != ss_32bit_memory_code)
	device_error(me, "Invalid prefetchable bit (p) in PCI address %s",
		     unit);
      set_p(address);
      chp++;
    }

  }

  /* required DD */
  if (!isxdigit(*chp))
    device_error(me, "Missing device number in PCI address %s", unit);
  val = strtoul(chp, &end, 16);
  if (chp == end)
    device_error(me, "Problem parsing device number in PCI address %s", unit);
  if ((val & 0x1f) != val)
    device_error(me, "Device number (0x%lx) out of range (0..0x1f) in PCI address %s",
		 val, unit);
  set_ddddd(address, val);
  chp = end;

  /* For config space, the F is optional */
  if (extract_ss(address) == ss_config_code
      && (isspace(*chp) || *chp == '\0'))
    return chp - unit;

  /* function number F */
  if (*chp != ',')
    device_error(me, "Missing function number in PCI address %s", unit);
  chp++;
  val = strtoul(chp, &end, 10);
  if (chp == end)
    device_error(me, "Problem parsing function number in PCI address %s",
		 unit);
  if ((val & 7) != val)
    device_error(me, "Function number (%ld) out of range (0..7) in PCI address %s",
		 (long)val, unit);
  set_fff(address, val);
  chp = end;

  /* for config space, must be end */
  if (extract_ss(address) == ss_config_code) {
    if (!isspace(*chp) && *chp != '\0')
      device_error(me, "Problem parsing PCI config address %s",
		   unit);
    return chp - unit;
  }

  /* register number RR */
  if (*chp != ',')
    device_error(me, "Missing register number in PCI address %s", unit);
  chp++;
  val = strtoul(chp, &end, 16);
  if (chp == end)
    device_error(me, "Problem parsing register number in PCI address %s",
		 unit);
  switch (extract_ss(address)) {
  case ss_io_code:
#if 0
    if (extract_n(address) && val != 0)
      device_error(me, "non-relocatable I/O register must be zero in PCI address %s", unit);
    else if (!extract_n(address)
	     && val != 0x10 && val != 0x14 && val != 0x18
	     && val != 0x1c && val != 0x20 && val != 0x24)
      device_error(me, "I/O register invalid in PCI address %s", unit);
#endif
    break;
  case ss_32bit_memory_code:
#if 0
    if (extract_n(address) && val != 0)
      device_error(me, "non-relocatable memory register must be zero in PCI address %s", unit);
    else if (!extract_n(address)
	     && val != 0x10 && val != 0x14 && val != 0x18
	     && val != 0x1c && val != 0x20 && val != 0x24 && val != 0x30)
      device_error(me, "I/O register (0x%lx) invalid in PCI address %s",
		   val, unit);
#endif
    break;
  case ss_64bit_memory_code:
    if (extract_n(address) && val != 0)
      device_error(me, "non-relocatable 32bit memory register must be zero in PCI address %s", unit);
    else if (!extract_n(address)
	     && val != 0x10 && val != 0x18 && val != 0x20)
      device_error(me, "Register number (0x%lx) invalid in 64bit PCI address %s",
		   val, unit);
  case ss_config_code:
    device_error(me, "internal error");
  }
  if ((val & 0xff) != val)
    device_error(me, "Register number (0x%lx) out of range (0..0xff) in PCI address %s",
		 val, unit);
  set_rrrrrrrr(address, val);
  chp = end;

  /* address */
  if (*chp != ',')
    device_error(me, "Missing address in PCI address %s", unit);
  chp++;
  switch (extract_ss(address)) {
  case ss_io_code:
  case ss_32bit_memory_code:
    val = strtoul(chp, &end, 16);
    if (chp == end)
      device_error(me, "Problem parsing address in PCI address %s", unit);
    switch (extract_ss(address)) {
    case ss_io_code:
      if (extract_n(address) && extract_t(address)
	  && (val & 1024) != val)
	device_error(me, "10bit aliased non-relocatable address (0x%lx) out of range in PCI address %s",
		     val, unit);
      if (!extract_n(address) && extract_t(address)
	  && (val & 0xffff) != val)
	device_error(me, "64k relocatable address (0x%lx) out of range in PCI address %s",
		     val, unit);
      break;
    case ss_32bit_memory_code:
      if (extract_t(address) && (val & 0xfffff) != val)
	device_error(me, "1mb memory address (0x%lx) out of range in PCI address %s",
		     val, unit);
      if (!extract_t(address) && (val & 0xffffffff) != val)
	device_error(me, "32bit memory address (0x%lx) out of range in PCI address %s",
		     val, unit);
      break;
    case ss_64bit_memory_code:
    case ss_config_code:
      device_error(me, "internal error");
    }
    set_ll_ll(address, val);
    chp = end;
    break;
  case ss_64bit_memory_code:
    device_error(me, "64bit addresses unimplemented");
    set_hh_hh(address, val);
    set_ll_ll(address, val);
    break;
  case ss_config_code:
    device_error(me, "internal error");
    break;
  }

  /* finished? */
  if (!isspace(*chp) && *chp != '\0')
    device_error(me, "Problem parsing PCI address %s", unit);

  return chp - unit;
}


/* Convert PCI device unit into its corresponding textual
   representation */

static int
hw_phb_unit_encode(device *me,
		   const device_unit *unit_address,
		   char *buf,
		   int sizeof_buf)
{
  if (unit_address->nr_cells != 3)
    device_error(me, "Incorrect number of cells in PCI unit address");
  if (device_nr_address_cells(me) != 3)
    device_error(me, "PCI bus should have #address-cells == 3");
  if (extract_ss(unit_address) == ss_config_code
      && extract_fff(unit_address) == 0
      && extract_rrrrrrrr(unit_address) == 0
      && extract_hh_hh(unit_address) == 0
      && extract_ll_ll(unit_address) == 0) {
    /* DD - Configuration Space address */
    sprintf(buf, "%x",
	    extract_ddddd(unit_address));
  }
  else if (extract_ss(unit_address) == ss_config_code
	   && extract_fff(unit_address) != 0
	   && extract_rrrrrrrr(unit_address) == 0
	   && extract_hh_hh(unit_address) == 0
	   && extract_ll_ll(unit_address) == 0) {
    /* DD,F - Configuration Space */
    sprintf(buf, "%x,%d",
	    extract_ddddd(unit_address),
	    extract_fff(unit_address));
  }
  else if (extract_ss(unit_address) == ss_io_code
	   && extract_hh_hh(unit_address) == 0) {
    /* [n]i[t]DD,F,RR,NNNNNNNN - 32bit I/O space */
    sprintf(buf, "%si%s%x,%d,%x,%x",
	    extract_n(unit_address) ? "n" : "",
	    extract_t(unit_address) ? "t" : "",
	    extract_ddddd(unit_address),
	    extract_fff(unit_address),
	    extract_rrrrrrrr(unit_address),
	    extract_ll_ll(unit_address));
  }
  else if (extract_ss(unit_address) == ss_32bit_memory_code
	   && extract_hh_hh(unit_address) == 0) {
    /* [n]m[t][p]DD,F,RR,NNNNNNNN - 32bit memory space */
    sprintf(buf, "%sm%s%s%x,%d,%x,%x",
	    extract_n(unit_address) ? "n" : "",
	    extract_t(unit_address) ? "t" : "",
	    extract_p(unit_address) ? "p" : "",
	    extract_ddddd(unit_address),
	    extract_fff(unit_address),
	    extract_rrrrrrrr(unit_address),
	    extract_ll_ll(unit_address));
  }
  else if (extract_ss(unit_address) == ss_32bit_memory_code) {
    /* [n]x[p]DD,F,RR,NNNNNNNNNNNNNNNN - 64bit memory space */
    sprintf(buf, "%sx%s%x,%d,%x,%x%08x",
	    extract_n(unit_address) ? "n" : "",
	    extract_p(unit_address) ? "p" : "",
	    extract_ddddd(unit_address),
	    extract_fff(unit_address),
	    extract_rrrrrrrr(unit_address),
	    extract_hh_hh(unit_address),
	    extract_ll_ll(unit_address));
  }
  else {
    device_error(me, "Invalid PCI unit address 0x%08lx 0x%08lx 0x%08lx",
		 (unsigned long)unit_address->cells[0],
		 (unsigned long)unit_address->cells[1],
		 (unsigned long)unit_address->cells[2]);
  }
  if (strlen(buf) > sizeof_buf)
    error("buffer overflow");
  return strlen(buf);
}


static int
hw_phb_address_to_attach_address(device *me,
				 const device_unit *address,
				 int *attach_space,
				 unsigned_word *attach_address,
				 device *client)
{
  if (address->nr_cells != 3)
    device_error(me, "attach address has incorrect number of cells");
  if (address->cells[1] != 0)
    device_error(me, "64bit attach address unsupported");

  /* directly decode the address/space */
  *attach_address = address->cells[2];
  switch (extract_ss(address)) {
  case ss_config_code:
    *attach_space = hw_phb_config_space;
    break;
  case ss_io_code:
    *attach_space = hw_phb_io_space;
    break;
  case ss_32bit_memory_code:
  case ss_64bit_memory_code:
    *attach_space = hw_phb_memory_space;
    break;
  }

  /* if non-relocatable finished */
  if (extract_n(address))
    return 1;

  /* make memory and I/O addresses absolute */
  if (*attach_space == hw_phb_io_space
      || *attach_space == hw_phb_memory_space) {
    int reg_nr;
    reg_property_spec assigned;
    if (extract_ss(address) == ss_64bit_memory_code)
      device_error(me, "64bit memory address not unsuported");
    for (reg_nr = 0;
	 device_find_reg_array_property(client, "assigned-addresses", reg_nr,
					&assigned);
	 reg_nr++) {
      if (!extract_n(&assigned.address)
	  || extract_rrrrrrrr(&assigned.address) == 0)
	device_error(me, "client %s has invalid assigned-address property",
		     device_path(client));
      if (extract_rrrrrrrr(address) == extract_rrrrrrrr(&assigned.address)) {
	/* corresponding base register */
	if (extract_ss(address) != extract_ss(&assigned.address))
	  device_error(me, "client %s has conflicting types for base register 0x%lx",
		       device_path(client),
		       (unsigned long)extract_rrrrrrrr(address));
	*attach_address += assigned.address.cells[2];
	return 0;
      }
    }
    device_error(me, "client %s missing base address register 0x%lx in assigned-addresses property",
		 device_path(client),
		 (unsigned long)extract_rrrrrrrr(address));
  }
  
  return 0;
}


static int
hw_phb_size_to_attach_size(device *me,
			   const device_unit *size,
			   unsigned *nr_bytes,
			   device *client)
{
  if (size->nr_cells != 2)
    device_error(me, "size has incorrect number of cells");
  if (size->cells[0] != 0)
    device_error(me, "64bit size unsupported");
  *nr_bytes = size->cells[1];
  return size->cells[1];
}


static const phb_space *
find_phb_space(hw_phb_device *phb,
	       unsigned_word addr,
	       unsigned nr_bytes)
{
  hw_phb_spaces space;
  /* find the space that matches the address */
  for (space = 0; space < nr_hw_phb_spaces; space++) {
    phb_space *pci_space = &phb->space[space];
    if (addr >= pci_space->parent_base
	&& (addr + nr_bytes) <= (pci_space->parent_base + pci_space->size)) {
      return pci_space;
    }
  }
  return NULL;
}


static unsigned_word
map_phb_addr(const phb_space *space,
	     unsigned_word addr)
{
  return addr - space->parent_base + space->my_base;
}



static unsigned
hw_phb_io_read_buffer(device *me,
		      void *dest,
		      int space,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      cpu *processor,
		      unsigned_word cia)
{
  hw_phb_device *phb = (hw_phb_device*)device_data(me);
  const phb_space *pci_space = find_phb_space(phb, addr, nr_bytes);
  unsigned_word bus_addr;
  if (pci_space == NULL)
    return 0;
  bus_addr = map_phb_addr(pci_space, addr);
  DTRACE(phb, ("io read - %d:0x%lx -> %s:0x%lx (%u bytes)\n",
	       space, (unsigned long)addr, pci_space->name, (unsigned long)bus_addr,
	       nr_bytes));
  return core_map_read_buffer(pci_space->readable,
			      dest, bus_addr, nr_bytes);
}


static unsigned
hw_phb_io_write_buffer(device *me,
		       const void *source,
		       int space,
		       unsigned_word addr,
		       unsigned nr_bytes,
		       cpu *processor,
		       unsigned_word cia)
{
  hw_phb_device *phb = (hw_phb_device*)device_data(me);
  const phb_space *pci_space = find_phb_space(phb, addr, nr_bytes);
  unsigned_word bus_addr;
  if (pci_space == NULL)
    return 0;
  bus_addr = map_phb_addr(pci_space, addr);
  DTRACE(phb, ("io write - %d:0x%lx -> %s:0x%lx (%u bytes)\n",
	       space, (unsigned long)addr, pci_space->name, (unsigned long)bus_addr,
	       nr_bytes));
  return core_map_write_buffer(pci_space->writeable, source,
			       bus_addr, nr_bytes);
}


static unsigned
hw_phb_dma_read_buffer(device *me,
		       void *dest,
		       int space,
		       unsigned_word addr,
		       unsigned nr_bytes)
{
  hw_phb_device *phb = (hw_phb_device*)device_data(me);
  const phb_space *pci_space;
  /* find the space */
  if (space != hw_phb_memory_space)
    device_error(me, "invalid dma address space %d", space);
  pci_space = &phb->space[space];
  /* check out the address */
  if ((addr >= pci_space->my_base
       && addr <= pci_space->my_base + pci_space->size)
      || (addr + nr_bytes >= pci_space->my_base
	  && addr + nr_bytes <= pci_space->my_base + pci_space->size))
    device_error(me, "Do not support DMA into own bus");
  /* do it */
  DTRACE(phb, ("dma read - %s:0x%lx (%d bytes)\n",
	       pci_space->name, addr, nr_bytes));
  return device_dma_read_buffer(device_parent(me),
				dest, pci_space->parent_space,
				addr, nr_bytes);
}


static unsigned
hw_phb_dma_write_buffer(device *me,
			const void *source,
			int space,
			unsigned_word addr,
			unsigned nr_bytes,
			int violate_read_only_section)
{
  hw_phb_device *phb = (hw_phb_device*)device_data(me);
  const phb_space *pci_space;
  /* find the space */
  if (space != hw_phb_memory_space)
    device_error(me, "invalid dma address space %d", space);
  pci_space = &phb->space[space];
  /* check out the address */
  if ((addr >= pci_space->my_base
       && addr <= pci_space->my_base + pci_space->size)
      || (addr + nr_bytes >= pci_space->my_base
	  && addr + nr_bytes <= pci_space->my_base + pci_space->size))
    device_error(me, "Do not support DMA into own bus");
  /* do it */
  DTRACE(phb, ("dma write - %s:0x%lx (%d bytes)\n",
	       pci_space->name, addr, nr_bytes));
  return device_dma_write_buffer(device_parent(me),
				 source, pci_space->parent_space,
				 addr, nr_bytes,
				 violate_read_only_section);
}


static device_callbacks const hw_phb_callbacks = {
  { hw_phb_init_address, },
  { hw_phb_attach_address, },
  { hw_phb_io_read_buffer, hw_phb_io_write_buffer },
  { hw_phb_dma_read_buffer, hw_phb_dma_write_buffer },
  { NULL, }, /* interrupt */
  { hw_phb_unit_decode,
    hw_phb_unit_encode,
    hw_phb_address_to_attach_address,
    hw_phb_size_to_attach_size }
};


static void *
hw_phb_create(const char *name,
	      const device_unit *unit_address,
	      const char *args)
{
  /* create the descriptor */
  hw_phb_device *phb = ZALLOC(hw_phb_device);

  /* create the core maps now */
  hw_phb_spaces space_nr;
  for (space_nr = 0; space_nr < nr_hw_phb_spaces; space_nr++) {
    phb_space *pci_space = &phb->space[space_nr];
    pci_space->map = core_create();
    pci_space->readable = core_readable(pci_space->map);
    pci_space->writeable = core_writeable(pci_space->map);
    switch (space_nr) {
    case hw_phb_memory_space:
      pci_space->name = "memory";
      break;
    case hw_phb_io_space:
      pci_space->name = "I/O";
      break;
    case hw_phb_config_space:
      pci_space->name = "config";
      break;
    case hw_phb_special_space:
      pci_space->name = "special";
      break;
    default:
      error ("internal error");
      break;
    }
  }

  return phb;
}


const device_descriptor hw_phb_device_descriptor[] = {
  { "phb", hw_phb_create, &hw_phb_callbacks },
  { "pci", NULL, &hw_phb_callbacks },
  { NULL, },
};

#endif /* _HW_PHB_ */
