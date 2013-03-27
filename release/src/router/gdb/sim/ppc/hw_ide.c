/*  This file is part of the program psim.

    Copyright (C) 1996, Andrew Cagney <cagney@highland.com.au>

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


#ifndef _HW_IDE_C_
#define _HW_IDE_C_

#include "device_table.h"



/* DEVICE


   ide - Integrated Disk Electronics


   DESCRIPTION


   This device models the primary/secondary <<ide>> controller
   described in the [CHRPIO] document.

   The controller has separate independant interrupt outputs for each
   <<ide>> bus.


   PROPERTIES


   reg = ...  (required)

   The <<reg>> property is described in the document [CHRPIO].


   ready-delay = <integer>  (optional)

   If present, this specifies the time that the <<ide>> device takes
   to complete an I/O operation.


   disk@?/ide-byte-count = <integer>  (optional)

   disk@?/ide-sector-count = <integer>  (optional)

   disk@?/ide-head-count = <integer>  (optional)

   The <<ide>> device checks each child (disk device) node to see if
   it has the above properties.  If present, these values will be used
   to compute the <<LBA>> address in <<CHS>> addressing mode.


   EXAMPLES


   Enable tracing:

   |  -t ide-device \


   Attach the <<ide>> device to the <<pci>> bus at slot one.  Specify
   legacy I/O addresses:

   |  -o '/phb/ide@1/assigned-addresses \
   |        ni0,0,10,1f0 8 \
   |        ni0,0,14,3f8 8 \
   |        ni0,0,18,170 8 \
   |        ni0,0,1c,378 8 \
   |        ni0,0,20,200 8' \
   |  -o '/phb@0x80000000/ide@1/reg \
   |        1 0 \
   |        i0,0,10,0 8 \
   |        i0,0,18,0 8 \
   |        i0,0,14,6 1 \
   |        i0,0,1c,6 1 \
   |        i0,0,20,0 8' \

   Note: the fouth and fifth reg entries specify that the register is
   at an offset into the address specified by the base register
   (<<assigned-addresses>>); Apart from restrictions placed by the
   <<pci>> specification, no restrictions are placed on the number of
   base registers specified by the <<assigned-addresses>> property.
   
   Attach a <<disk>> to the primary and a <<cdrom>> to the secondary
   <<ide>> controller.

   |  -o '/phb@0x80000000/ide@1/disk@0/file "zero' \
   |  -o '/phb@0x80000000/ide@1/cdrom@2/file "/dev/cdrom"' \

   Connect the two interrupt outputs (a and b) to a <<glue>> device to
   allow testing of the interrupt port. In a real simulation they
   would be wired to the interrupt controller.

   |  -o '/phb@0x80000000/glue@2/reg 2 0 ni0,0,0,0 8' \
   |  -o '/phb@0x80000000/ide@1 > a 0 /phb@0x80000000/glue@2' \
   |  -o '/phb@0x80000000/ide@1 > b 1 /phb@0x80000000/glue@2' 

   
   BUGS
   

   While the DMA registers are present, DMA support has not yet been
   implemented.

   The number of supported commands is very limited.

   The standards documents appear to be vague on how to specify the
   <<unit-address>> of disk devices devices being attached to the
   <<ide>> controller.  I've chosen to use integers with devices zero
   and one going to the primary controller while two and three are
   connected to the secondary controller.


   REFERENCES


   [CHRPIO] PowerPC(tm) Microprocessor Common Hardware Reference
   Platform: I/O Device Reference.  http://chrp.apple.com/???.

   [SCHMIDT] The SCSI Bus and IDE Interface - Protocols, Applications
   and Programming.  Friedhelm Schmidt (translated by Michael
   Schultz).  ISBN 0-201-42284-0.  Addison-Wesley Publishing Company.


   */

   

typedef enum _io_direction {
  is_read,
  is_write,
} io_direction;


enum {
  nr_ide_controllers = 2,
  nr_ide_drives_per_controller = 2,
  nr_fifo_entries = 8192,
};

enum {
  /* command register block - read */
  ide_data_reg,
  ide_error_reg, /*ide_feature_reg*/
  ide_sector_count_reg,
  ide_sector_number_reg,
  ide_cylinder_reg0,
  ide_cylinder_reg1,
  ide_drive_head_reg,
  ide_status_reg, /*ide_command_reg*/
  /* command register block - write */
  ide_feature_reg, /*ide_error_reg*/
  ide_command_reg, /*ide_status_reg*/
  /* control register block - read */
  ide_alternate_status_reg, /*ide_control_reg*/
  ide_control_reg, /*ide_alternate_status_reg*/
  /* dma register block */
  ide_dma_command_reg,
  ide_dma_unused_1_reg,
  ide_dma_status_reg,
  ide_dma_unused_3_reg,
  ide_dma_prd_table_address_reg0,
  ide_dma_prd_table_address_reg1,
  ide_dma_prd_table_address_reg2,
  ide_dma_prd_table_address_reg3,
  nr_ide_registers,
};


typedef enum _ide_states {
  idle_state,
  busy_loaded_state,
  busy_drained_state,
  busy_dma_state,
  busy_command_state,
  loading_state,
  draining_state,
} ide_states;

static const char *
ide_state_name(ide_states state)
{
  switch (state) {
  case idle_state: return "idle";
  case busy_loaded_state: return "busy_loaded_state";
  case busy_drained_state: return "busy_drained_state";
  case busy_dma_state: return "busy_dma_state";
  case busy_command_state: return "busy_command_state";
  case loading_state: return "loading_state";
  case draining_state: return "draining_state";
  default: return "illegal-state";
  }
}

typedef struct _ide_geometry {
  int head;
  int sector;
  int byte;
} ide_geometry;

typedef struct _ide_drive {
  int nr;
  device *device;
  ide_geometry geometry;
  ide_geometry default_geometry;
} ide_drive;

typedef struct _ide_controller {
  int nr;
  ide_states state;
  unsigned8 reg[nr_ide_registers];
  unsigned8 fifo[nr_fifo_entries];
  int fifo_pos;
  int fifo_size;
  ide_drive *current_drive;
  int current_byte;
  int current_transfer;
  ide_drive drive[nr_ide_drives_per_controller];
  device *me;
  event_entry_tag event_tag;
  int is_interrupting;
  signed64 ready_delay;
} ide_controller;



static void
set_interrupt(device *me,
	      ide_controller *controller)
{
  if ((controller->reg[ide_control_reg] & 0x2) == 0) {
    DTRACE(ide, ("controller %d - interrupt set\n", controller->nr));
    device_interrupt_event(me, controller->nr, 1, NULL, 0);
    controller->is_interrupting = 1;
  }
}


static void
clear_interrupt(device *me,
		ide_controller *controller)
{
  if (controller->is_interrupting) {
    DTRACE(ide, ("controller %d - interrupt clear\n", controller->nr));
    device_interrupt_event(me, controller->nr, 0, NULL, 0);
    controller->is_interrupting = 0;
  }
}


static void
do_event(void *data)
{
  ide_controller *controller = data;
  device *me = controller->me;
  controller->event_tag = 0;
  switch (controller->state) {
  case busy_loaded_state:
  case busy_drained_state:
    if (controller->current_transfer > 0) {
      controller->state = (controller->state == busy_loaded_state
			   ? loading_state : draining_state);
    }
    else {
      controller->state = idle_state;
    }
    set_interrupt(me, controller);
    break;
  default:
    device_error(me, "controller %d - unexpected event", controller->nr);
    break;
  }
}


static void
schedule_ready_event(device *me,
		     ide_controller *controller)
{
  if (controller->event_tag != 0)
    device_error(me, "controller %d - attempting to schedule multiple events",
		 controller->nr);
  controller->event_tag = 
    device_event_queue_schedule(me, controller->ready_delay,
				do_event, controller);
}


static void
do_fifo_read(device *me,
	     ide_controller *controller,
	     void *dest,
	     int nr_bytes)
{
  if (controller->state != draining_state)
    device_error(me, "controller %d - reading fifo when not ready (%s)",
		 controller->nr,
		 ide_state_name(controller->state));
  if (controller->fifo_pos + nr_bytes > controller->fifo_size)
    device_error(me, "controller %d - fifo underflow", controller->nr);
  if (nr_bytes > 0) {
    memcpy(dest, &controller->fifo[controller->fifo_pos], nr_bytes);
    controller->fifo_pos += nr_bytes;
  }
  if (controller->fifo_pos == controller->fifo_size) {
    controller->current_transfer -= 1;
    if (controller->current_transfer > 0
	&& controller->current_drive != NULL) {
      DTRACE(ide, ("controller %d:%d - reading %d byte block at 0x%x\n",
		   controller->nr,
		   controller->current_drive->nr,
		   controller->fifo_size,
		   controller->current_byte));
      if (device_io_read_buffer(controller->current_drive->device,
				controller->fifo,
				0, controller->current_byte,
				controller->fifo_size,
				NULL, 0)
	  != controller->fifo_size)
	device_error(me, "controller %d - disk %s io read error",
		     controller->nr,
		     device_path(controller->current_drive->device));
    }
    controller->state = busy_drained_state;
    controller->fifo_pos = 0;
    controller->current_byte += controller->fifo_size;
    schedule_ready_event(me, controller);
  }
}


static void
do_fifo_write(device *me,
	      ide_controller *controller,
	      const void *source,
	      int nr_bytes)
{
  if (controller->state != loading_state)
    device_error(me, "controller %d - writing fifo when not ready (%s)",
		 controller->nr,
		 ide_state_name(controller->state));
  if (controller->fifo_pos + nr_bytes > controller->fifo_size)
    device_error(me, "controller %d - fifo overflow", controller->nr);
  if (nr_bytes > 0) {
    memcpy(&controller->fifo[controller->fifo_pos], source, nr_bytes);
    controller->fifo_pos += nr_bytes;
  }
  if (controller->fifo_pos == controller->fifo_size) {
    if (controller->current_transfer > 0
	&& controller->current_drive != NULL) {
      DTRACE(ide, ("controller %d:%d - writing %d byte block at 0x%x\n",
		   controller->nr,
		   controller->current_drive->nr,
		   controller->fifo_size,
		   controller->current_byte));
      if (device_io_write_buffer(controller->current_drive->device,
				 controller->fifo,
				 0, controller->current_byte,
				 controller->fifo_size,
				 NULL, 0)
	  != controller->fifo_size)
	device_error(me, "controller %d - disk %s io write error",
		     controller->nr,
		     device_path(controller->current_drive->device));
    }
    controller->current_transfer -= 1;
    controller->fifo_pos = 0;
    controller->current_byte += controller->fifo_size;
    controller->state = busy_loaded_state;
    schedule_ready_event(me, controller);
  }
}


static void
setup_fifo(device *me,
	   ide_controller *controller,
	   int is_simple,
	   int is_with_disk,
	   io_direction direction)
{
  /* find the disk */
  if (is_with_disk) {
    int drive_nr = (controller->reg[ide_drive_head_reg] & 0x10) != 0;
    controller->current_drive = &controller->drive[drive_nr];
  }
  else {
    controller->current_drive = NULL;
  }

  /* number of transfers */
  if (is_simple)
    controller->current_transfer = 1;
  else {
    int sector_count = controller->reg[ide_sector_count_reg];
    if (sector_count == 0)
      controller->current_transfer = 256;
    else
      controller->current_transfer = sector_count;
  }

  /* the transfer size */
  if (controller->current_drive == NULL)
    controller->fifo_size = 512;
  else
    controller->fifo_size = controller->current_drive->geometry.byte;

  /* empty the fifo */
  controller->fifo_pos = 0;

  /* the starting address */
  if (controller->current_drive == NULL)
    controller->current_byte = 0;
  else if (controller->reg[ide_drive_head_reg] & 0x40) {
    /* LBA addressing mode */
    controller->current_byte = controller->fifo_size
      * (((controller->reg[ide_drive_head_reg] & 0xf) << 24)
	 | (controller->reg[ide_cylinder_reg1] << 16)
	 | (controller->reg[ide_cylinder_reg0] << 8)
	 | (controller->reg[ide_sector_number_reg]));
  }
  else if (controller->current_drive->geometry.head != 0
	   && controller->current_drive->geometry.sector != 0) {
    /* CHS addressing mode */
    int head_nr = controller->reg[ide_drive_head_reg] & 0xf;
    int cylinder_nr = ((controller->reg[ide_cylinder_reg1] << 8)
		    | controller->reg[ide_cylinder_reg0]);
    int sector_nr = controller->reg[ide_sector_number_reg];
    controller->current_byte = controller->fifo_size
      * ((cylinder_nr * controller->current_drive->geometry.head + head_nr)
	 * controller->current_drive->geometry.sector + sector_nr - 1);
  }
  else
    device_error(me, "controller %d:%d - CHS addressing disabled",
		 controller->nr, controller->current_drive->nr);
  DTRACE(ide, ("controller %ld:%ld - transfer (%s) %ld blocks of %ld bytes from 0x%lx\n",
	       (long)controller->nr,
	       controller->current_drive == NULL ? -1L : (long)controller->current_drive->nr,
	       direction == is_read ? "read" : "write",
	       (long)controller->current_transfer,
	       (long)controller->fifo_size,
	       (unsigned long)controller->current_byte));
  switch (direction) {
  case is_read:
    /* force a primeing read */
    controller->current_transfer += 1;
    controller->state = draining_state; 
    controller->fifo_pos = controller->fifo_size;
    do_fifo_read(me, controller, NULL, 0);
    break;
  case is_write:
    controller->state = loading_state;
    break;
  }
}


static void
do_command(device *me,
	   ide_controller *controller,
	   int command)
{
  if (controller->state != idle_state)
    device_error(me, "controller %d - command when not idle", controller->nr);
  switch (command) {
  case 0x20: case 0x21: /* read-sectors */
    setup_fifo(me, controller, 0/*is_simple*/, 1/*is_with_disk*/, is_read);
    break;
  case 0x30: case 0x31: /* write */
    setup_fifo(me, controller, 0/*is_simple*/, 1/*is_with_disk*/, is_write);
    break;
  }
}

static unsigned8
get_status(device *me,
	   ide_controller *controller)
{
  switch (controller->state) {
  case loading_state:
  case draining_state:
    return 0x08; /* data req */
  case busy_loaded_state:
  case busy_drained_state:
    return 0x80; /* busy */
  case idle_state:
    return 0x40; /* drive ready */
  default:
    device_error(me, "internal error");
    return 0;
  }
}
	  

/* The address presented to the IDE controler is decoded and then
   mapped onto a controller:reg pair */

enum {
  nr_address_blocks = 6,
};

typedef struct _address_block {
  int space;
  unsigned_word base_addr;
  unsigned_word bound_addr;
  int controller;
  int base_reg;
} address_block;

typedef struct _address_decoder {
  address_block block[nr_address_blocks];
} address_decoder;

static void
decode_address(device *me,
	       address_decoder *decoder,
	       int space,
	       unsigned_word address,
	       int *controller,
	       int *reg,
	       io_direction direction)
{
  int i;
  for (i = 0; i < nr_address_blocks; i++) {
    if (space == decoder->block[i].space
	&& address >= decoder->block[i].base_addr
	&& address <= decoder->block[i].bound_addr) {
      *controller = decoder->block[i].controller;
      *reg = (address
	      - decoder->block[i].base_addr
	      + decoder->block[i].base_reg);
      if (direction == is_write) {
	switch (*reg) {
	case ide_error_reg: *reg = ide_feature_reg; break;
	case ide_status_reg: *reg = ide_command_reg; break;
	case ide_alternate_status_reg: *reg = ide_control_reg; break;
	default: break;
	}
      }
      return;
    }
  }
  device_error(me, "address %d:0x%lx invalid",
	       space, (unsigned long)address);
}


static void
build_address_decoder(device *me,
		      address_decoder *decoder)
{
  int reg;
  for (reg = 1; reg < 6; reg++) {
    reg_property_spec unit;
    int space;
    unsigned_word address;
    unsigned size;
    /* find and decode the reg property */
    if (!device_find_reg_array_property(me, "reg", reg, &unit))
      device_error(me, "missing or invalid reg entry %d", reg);
    device_address_to_attach_address(device_parent(me), &unit.address,
				     &space, &address, me);
    device_size_to_attach_size(device_parent(me), &unit.size, &size, me);
    /* insert it into the address decoder */
    switch (reg) {
    case 1:
    case 2:
      /* command register block */
      if (size != 8)
	device_error(me, "reg entry %d must have a size of 8", reg);
      decoder->block[reg-1].space = space;
      decoder->block[reg-1].base_addr = address;
      decoder->block[reg-1].bound_addr = address + size - 1;
      decoder->block[reg-1].controller = (reg + 1) % nr_ide_controllers;
      decoder->block[reg-1].base_reg = ide_data_reg;
      DTRACE(ide, ("controller %d command register block at %d:0x%lx..0x%lx\n",
		   decoder->block[reg-1].controller,
		   decoder->block[reg-1].space,
		   (unsigned long)decoder->block[reg-1].base_addr,
		   (unsigned long)decoder->block[reg-1].bound_addr));
      break;
    case 3:
    case 4:
      /* control register block */
      if (size != 1)
	device_error(me, "reg entry %d must have a size of 1", reg);
      decoder->block[reg-1].space = space;
      decoder->block[reg-1].base_addr = address;
      decoder->block[reg-1].bound_addr = address + size - 1;
      decoder->block[reg-1].controller = (reg + 1) % nr_ide_controllers;
      decoder->block[reg-1].base_reg = ide_alternate_status_reg;
      DTRACE(ide, ("controller %d control register block at %d:0x%lx..0x%lx\n",
		   decoder->block[reg-1].controller,
		   decoder->block[reg-1].space,
		   (unsigned long)decoder->block[reg-1].base_addr,
		   (unsigned long)decoder->block[reg-1].bound_addr));
      break;
    case 5:
      /* dma register block */
      if (size != 8)
	device_error(me, "reg entry %d must have a size of 8", reg);
      decoder->block[reg-1].space = space;
      decoder->block[reg-1].base_addr = address;
      decoder->block[reg-1].bound_addr = address + 4 - 1;
      decoder->block[reg-1].base_reg = ide_dma_command_reg;
      decoder->block[reg-1].controller = 0;
      DTRACE(ide, ("controller %d dma register block at %d:0x%lx..0x%lx\n",
		   decoder->block[reg-1].controller,
		   decoder->block[reg-1].space,
		   (unsigned long)decoder->block[reg-1].base_addr,
		   (unsigned long)decoder->block[reg-1].bound_addr));
      decoder->block[reg].space = space;
      decoder->block[reg].base_addr = address + 4;
      decoder->block[reg].bound_addr = address + 8 - 1;
      decoder->block[reg].controller = 1;
      decoder->block[reg].base_reg = ide_dma_command_reg;
      DTRACE(ide, ("controller %d dma register block at %d:0x%lx..0x%lx\n",
		   decoder->block[reg].controller,
		   decoder->block[reg-1].space,
		   (unsigned long)decoder->block[reg].base_addr,
		   (unsigned long)decoder->block[reg].bound_addr));
      break;
    default:
      device_error(me, "internal error - bad switch");
      break;
    }
  }
}
		      


typedef struct _hw_ide_device {
  ide_controller controller[nr_ide_controllers];
  address_decoder decoder;
} hw_ide_device;


static void
hw_ide_init_address(device *me)
{
  hw_ide_device *ide = device_data(me);
  int controller;
  int drive;
 
  /* zero some things */
  for (controller = 0; controller < nr_ide_controllers; controller++) {
    memset(&ide->controller[controller], 0, sizeof(ide_controller));
    for (drive = 0; drive < nr_ide_drives_per_controller; drive++) {
      ide->controller[controller].drive[drive].nr = drive;
    }
    ide->controller[controller].me = me;
    if (device_find_property(me, "ready-delay") != NULL)
      ide->controller[controller].ready_delay =
	device_find_integer_property(me, "ready-delay");
  }

  /* attach this device to its parent */
  generic_device_init_address(me);

  /* determine our own address map */
  build_address_decoder(me, &ide->decoder);

}


static void
hw_ide_attach_address(device *me,
		      attach_type type,
		      int space,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      access_type access,
		      device *client) /*callback/default*/
{
  hw_ide_device *ide = (hw_ide_device*)device_data(me);
  int controller_nr = addr / nr_ide_drives_per_controller;
  int drive_nr = addr % nr_ide_drives_per_controller;
  ide_controller *controller;
  ide_drive *drive;
  if (controller_nr >= nr_ide_controllers)
    device_error(me, "no controller for disk %s",
		 device_path(client));

  controller = &ide->controller[controller_nr];
  drive = &controller->drive[drive_nr];
  drive->device = client;
  if (device_find_property(client, "ide-byte-count") != NULL)
    drive->geometry.byte = device_find_integer_property(client, "ide-byte-count");
  else
    drive->geometry.byte = 512;
  if (device_find_property(client, "ide-sector-count") != NULL)
    drive->geometry.sector = device_find_integer_property(client, "ide-sector-count");
  if (device_find_property(client, "ide-head-count") != NULL)
    drive->geometry.head = device_find_integer_property(client, "ide-head-count");
  drive->default_geometry = drive->geometry;
  DTRACE(ide, ("controller %d:%d %s byte-count %d, sector-count %d, head-count %d\n",
	       controller_nr,
	       drive->nr,
	       device_path(client),
	       drive->geometry.byte,
	       drive->geometry.sector,
	       drive->geometry.head));
}


static unsigned
hw_ide_io_read_buffer(device *me,
		      void *dest,
		      int space,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      cpu *processor,
		      unsigned_word cia)
{
  hw_ide_device *ide = (hw_ide_device *)device_data(me);
  int control_nr;
  int reg;
  ide_controller *controller;

  /* find the interface */
  decode_address(me, &ide->decoder, space, addr, &control_nr, &reg, is_read);
  controller = & ide->controller[control_nr];

  /* process the transfer */
  memset(dest, 0, nr_bytes);
  switch (reg) {
  case ide_data_reg:
    do_fifo_read(me, controller, dest, nr_bytes);
    break;
  case ide_status_reg:
    *(unsigned8*)dest = get_status(me, controller);
    clear_interrupt(me, controller);
    break;
  case ide_alternate_status_reg:
    *(unsigned8*)dest = get_status(me, controller);
    break;
  case ide_error_reg:
  case ide_sector_count_reg:
  case ide_sector_number_reg:
  case ide_cylinder_reg0:
  case ide_cylinder_reg1:
  case ide_drive_head_reg:
  case ide_control_reg:
  case ide_dma_command_reg:
  case ide_dma_status_reg:
  case ide_dma_prd_table_address_reg0:
  case ide_dma_prd_table_address_reg1:
  case ide_dma_prd_table_address_reg2:
  case ide_dma_prd_table_address_reg3:
    *(unsigned8*)dest = controller->reg[reg];
    break;
  default:
    device_error(me, "bus-error at address 0x%lx", addr);
    break;
  }
  return nr_bytes;
}


static unsigned
hw_ide_io_write_buffer(device *me,
		       const void *source,
		       int space,
		       unsigned_word addr,
		       unsigned nr_bytes,
		       cpu *processor,
		       unsigned_word cia)
{
  hw_ide_device *ide = (hw_ide_device *)device_data(me);
  int control_nr;
  int reg;
  ide_controller *controller;

  /* find the interface */
  decode_address(me, &ide->decoder, space, addr, &control_nr, &reg, is_write);
  controller = &ide->controller[control_nr];

  /* process the access */
  switch (reg) {
  case ide_data_reg:
    do_fifo_write(me, controller, source, nr_bytes);
    break;
  case ide_command_reg:
    do_command(me, controller, *(unsigned8*)source);
    break;
  case ide_control_reg:
    controller->reg[reg] = *(unsigned8*)source;
    /* possibly cancel interrupts */
    if ((controller->reg[reg] & 0x02) == 0x02)
      clear_interrupt(me, controller);
    break;
  case ide_feature_reg:
  case ide_sector_count_reg:
  case ide_sector_number_reg:
  case ide_cylinder_reg0:
  case ide_cylinder_reg1:
  case ide_drive_head_reg:
  case ide_dma_command_reg:
  case ide_dma_status_reg:
  case ide_dma_prd_table_address_reg0:
  case ide_dma_prd_table_address_reg1:
  case ide_dma_prd_table_address_reg2:
  case ide_dma_prd_table_address_reg3:
    controller->reg[reg] = *(unsigned8*)source;
    break;
  default:
    device_error(me, "bus-error at 0x%lx", addr);
    break;
  }
  return nr_bytes;
}


static const device_interrupt_port_descriptor hw_ide_interrupt_ports[] = {
  { "a", 0, 0 },
  { "b", 1, 0 },
  { "c", 2, 0 },
  { "d", 3, 0 },
  { NULL }
};



static device_callbacks const hw_ide_callbacks = {
  { hw_ide_init_address, },
  { hw_ide_attach_address, }, /* attach */
  { hw_ide_io_read_buffer, hw_ide_io_write_buffer, },
  { NULL, }, /* DMA */
  { NULL, NULL, hw_ide_interrupt_ports }, /* interrupt */
  { generic_device_unit_decode,
    generic_device_unit_encode,
    generic_device_address_to_attach_address,
    generic_device_size_to_attach_size },
};


static void *
hw_ide_create(const char *name,
	      const device_unit *unit_address,
	      const char *args)
{
  hw_ide_device *ide = ZALLOC(hw_ide_device);
  return ide;
}


const device_descriptor hw_ide_device_descriptor[] = {
  { "ide", hw_ide_create, &hw_ide_callbacks },
  { NULL, },
};

#endif /* _HW_IDE_ */
