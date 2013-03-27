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


#ifndef _HW_EEPROM_C_
#define _HW_EEPROM_C_

#include "device_table.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


/* DEVICE


   eeprom - JEDEC? compatible electricaly erasable programable device


   DESCRIPTION


   This device implements a small byte addressable EEPROM.
   Programming is performed using the same write sequences as used by
   standard modern EEPROM components.  Writes occure in real time, the
   device returning a progress value until the programing has been
   completed.

   It is based on the AMD 29F040 component.


   PROPERTIES


   reg = <address> <size> (required)

   Determine where the device lives in the parents address space.


   nr-sectors = <integer> (required)

   When erasing an entire sector is cleared at a time.  This specifies
   the number of sectors in the EEPROM component.


   sector-size = <integer> (required)

   The number of bytes in a sector.  When erasing, memory chunks of
   this size are cleared.

   NOTE: The product nr-sectors * sector-size does not need to map the
   size specified in the reg property.  If the specified size is
   smaller part of the eeprom will not be accessible while if it is
   larger the addresses will wrap.


   byte-write-delay = <integer> (required)

   Number of clock ticks before the programming of a single byte
   completes.


   sector-start-delay = <integer> (required)

   When erasing sectors, the number of clock ticks after the sector
   has been specified that the actual erase process commences.


   erase-delay = <intger> (required)

   Number of clock ticks before an erase program completes


   manufacture-code = <integer> (required)

   The one byte value returned when the auto-select manufacturer code
   is read.


   device-code = <integer> (required)

   The one byte value returned when the auto-select device code is
   read.


   input-file = <file-name> (optional)

   Initialize the eeprom using the specified binary file.


   output-file = <file-name> (optional)

   When ever the eeprom is updated, save the modified image into the
   specified file.


   EXAMPLES


   Enable tracing of the eeprom:

   |  bash$ psim -t eeprom-device \


   Configure something very like the Amd Am29F040 - 512byte EEPROM
   (but a bit faster):

   |  -o '/eeprom@0xfff00000/reg 0xfff00000 0x80000' \
   |  -o '/eeprom@0xfff00000/nr-sectors 8' \
   |  -o '/eeprom@0xfff00000/sector-size 0x10000' \
   |  -o '/eeprom@0xfff00000/byte-write-delay 1000' \
   |  -o '/eeprom@0xfff00000/sector-start-delay 100' \
   |  -o '/eeprom@0xfff00000/erase-delay 1000' \
   |  -o '/eeprom@0xfff00000/manufacture-code 0x01' \
   |  -o '/eeprom@0xfff00000/device-code 0xa4' \


   Initialize the eeprom from the file <</dev/zero>>:

   |  -o '/eeprom@0xfff00000/input-file /dev/zero'


   BUGS


   */

typedef enum {
  read_reset,
  write_nr_2,
  write_nr_3,
  write_nr_4,
  write_nr_5,
  write_nr_6,
  byte_program,
  byte_programming,
  chip_erase,
  sector_erase,
  sector_erase_suspend,
  autoselect,
} hw_eeprom_states;

static const char *
state2a(hw_eeprom_states state)
{
  switch (state) {
  case read_reset: return "read_reset";
  case write_nr_2: return "write_nr_2";
  case write_nr_3: return "write_nr_3";
  case write_nr_4: return "write_nr_4";
  case write_nr_5: return "write_nr_5";
  case write_nr_6: return "write_nr_6";
  case byte_program: return "byte_program";
  case byte_programming: return "byte_programming";
  case chip_erase: return "chip_erase";
  case sector_erase: return "sector_erase";
  case sector_erase_suspend: return "sector_erase_suspend";
  case autoselect: return "autoselect";
  }
  return NULL;
}

typedef struct _hw_eeprom_device {
  /* general */
  hw_eeprom_states state;
  unsigned8 *memory;
  unsigned sizeof_memory;
  unsigned erase_delay;
  signed64 program_start_time;
  signed64 program_finish_time;
  unsigned8 manufacture_code;
  unsigned8 device_code;
  unsigned8 toggle_bit;
  /* initialization */
  const char *input_file_name;
  const char *output_file_name;
  /* for sector and sector programming */
  hw_eeprom_states sector_state;
  unsigned8 *sectors;
  unsigned nr_sectors;
  unsigned sizeof_sector;
  unsigned sector_start_delay;
  unsigned sector_start_time;
  /* byte and byte programming */
  unsigned byte_write_delay;
  unsigned_word byte_program_address;
  unsigned8 byte_program_byte;
} hw_eeprom_device;

typedef struct _hw_eeprom_reg_spec {
  unsigned32 base;
  unsigned32 size;
} hw_eeprom_reg_spec;

static void
hw_eeprom_init_data(device *me)
{
  hw_eeprom_device *eeprom = (hw_eeprom_device*)device_data(me);

  /* have we any input or output files */
  if (device_find_property(me, "input-file") != NULL)
    eeprom->input_file_name = device_find_string_property(me, "input-file");
  if (device_find_property(me, "output-file") != NULL)
    eeprom->input_file_name = device_find_string_property(me, "output-file");

  /* figure out the sectors in the eeprom */
  if (eeprom->sectors == NULL) {
    eeprom->nr_sectors = device_find_integer_property(me, "nr-sectors");
    eeprom->sizeof_sector = device_find_integer_property(me, "sector-size");
    eeprom->sectors = zalloc(eeprom->nr_sectors);
  }
  else
    memset(eeprom->sectors, 0, eeprom->nr_sectors);

  /* initialize the eeprom */
  if (eeprom->memory == NULL) {
    eeprom->sizeof_memory = eeprom->sizeof_sector * eeprom->nr_sectors;
    eeprom->memory = zalloc(eeprom->sizeof_memory);
  }
  else
    memset(eeprom->memory, 0, eeprom->sizeof_memory);
  if (eeprom->input_file_name != NULL) {
    int i;
    FILE *input_file = fopen(eeprom->input_file_name, "r");
    if (input_file == NULL) {
      perror("eeprom");
      device_error(me, "Failed to open input file %s\n", eeprom->input_file_name);
    }
    for (i = 0; i < eeprom->sizeof_memory; i++) {
      if (fread(&eeprom->memory[i], 1, 1, input_file) != 1)
	break;
    }
    fclose(input_file);
  }

  /* timing */
  eeprom->byte_write_delay = device_find_integer_property(me, "byte-write-delay");
  eeprom->sector_start_delay = device_find_integer_property(me, "sector-start-delay");
  eeprom->erase_delay = device_find_integer_property(me, "erase-delay");

  /* misc */
  eeprom->manufacture_code = device_find_integer_property(me, "manufacture-code");
  eeprom->device_code = device_find_integer_property(me, "device-code");
}


static void
invalid_read(device *me,
	     hw_eeprom_states state,
	     unsigned_word address,
	     const char *reason)
{
  DTRACE(eeprom, ("Invalid read to 0x%lx while in state %s (%s)\n",
		  (unsigned long)address,
		  state2a(state),
		  reason));
}

static void
invalid_write(device *me,
	      hw_eeprom_states state,
	      unsigned_word address,
	      unsigned8 data,
	      const char *reason)
{
  DTRACE(eeprom, ("Invalid write of 0x%lx to 0x%lx while in state %s (%s)\n",
		  (unsigned long)data,
		  (unsigned long)address,
		  state2a(state),
		  reason));
}

static void
dump_eeprom(device *me,
	    hw_eeprom_device *eeprom)
{
  if (eeprom->output_file_name != NULL) {
    int i;
    FILE *output_file = fopen(eeprom->output_file_name, "w");
    if (output_file == NULL) {
      perror("eeprom");
      device_error(me, "Failed to open output file %s\n",
		   eeprom->output_file_name);
    }
    for (i = 0; i < eeprom->sizeof_memory; i++) {
      if (fwrite(&eeprom->memory[i], 1, 1, output_file) != 1)
	break;
    }
    fclose(output_file);
  }
}


/* program a single byte of eeprom */

static void
start_programming_byte(device *me,
		       hw_eeprom_device *eeprom,
		       unsigned_word address,
		       unsigned8 new_byte)
{
  unsigned8 old_byte = eeprom->memory[address];
  DTRACE(eeprom, ("start-programing-byte - address 0x%lx, new 0x%lx, old 0x%lx\n",
		  (unsigned long)address,
		  (unsigned long)new_byte,
		  (unsigned long)old_byte));
  eeprom->byte_program_address = address;
  /* : old new : ~old : new&~old
     :  0   0  :   1  :    0
     :  0   1  :   1  :    1     -- can not set a bit
     :  1   0  :   0  :    0
     :  1   1  :   0  :    0 */
  if (~old_byte & new_byte)
    invalid_write(me, eeprom->state, address, new_byte, "setting cleared bit");
  /* : old new : old&new
     :  0   0  :    0
     :  0   1  :    0
     :  1   0  :    0
     :  1   1  :    1 */
  eeprom->byte_program_byte = new_byte & old_byte;
  eeprom->memory[address] = ~new_byte & ~0x24; /* LE-bits 5:3 zero */
  eeprom->program_start_time = device_event_queue_time(me);
  eeprom->program_finish_time = (eeprom->program_start_time
				 + eeprom->byte_write_delay);
}

static void
finish_programming_byte(device *me,
			hw_eeprom_device *eeprom)
{
  DTRACE(eeprom, ("finish-programming-byte - address 0x%lx, byte 0x%lx\n",
		  (unsigned long)eeprom->byte_program_address,
		  (unsigned long)eeprom->byte_program_byte));
  eeprom->memory[eeprom->byte_program_address] = eeprom->byte_program_byte;
  dump_eeprom(me, eeprom);
}


/* erase the eeprom completly */

static void
start_erasing_chip(device *me,
		   hw_eeprom_device *eeprom)
{
  DTRACE(eeprom, ("start-erasing-chip\n"));
  memset(eeprom->memory, 0, eeprom->sizeof_memory);
  eeprom->program_start_time = device_event_queue_time(me);
  eeprom->program_finish_time = (eeprom->program_start_time
				 + eeprom->erase_delay);
}

static void
finish_erasing_chip(device *me,
		    hw_eeprom_device *eeprom)
{
  DTRACE(eeprom, ("finish-erasing-chip\n"));
  memset(eeprom->memory, 0xff, eeprom->sizeof_memory);
  dump_eeprom(me, eeprom);
}


/* erase a single sector of the eeprom */

static void
start_erasing_sector(device *me,
		     hw_eeprom_device *eeprom,
		     unsigned_word address)
{
  int sector = address / eeprom->sizeof_sector;
  DTRACE(eeprom, ("start-erasing-sector - address 0x%lx, sector %d\n",
		  (unsigned long)address, sector));
  ASSERT(sector < eeprom->nr_sectors);
  eeprom->sectors[sector] = 1;
  memset(eeprom->memory + sector * eeprom->sizeof_sector,
	 0x4, eeprom->sizeof_sector);
  eeprom->program_start_time = device_event_queue_time(me);
  eeprom->sector_start_time = (eeprom->program_start_time
			       + eeprom->sector_start_delay);
  eeprom->program_finish_time = (eeprom->sector_start_time
				 + eeprom->erase_delay);

}

static void
finish_erasing_sector(device *me,
		      hw_eeprom_device *eeprom)
{
  int sector;
  DTRACE(eeprom, ("finish-erasing-sector\n"));
  for (sector = 0; sector < eeprom->nr_sectors; sector++) {
    if (eeprom->sectors[sector]) {
      eeprom->sectors[sector] = 0;
      memset(eeprom->memory + sector * eeprom->sizeof_sector,
	     0xff, eeprom->sizeof_sector);
    }
  }
  dump_eeprom(me, eeprom);
}


/* eeprom reads */

static unsigned8
toggle(hw_eeprom_device *eeprom,
       unsigned8 byte)
{
  eeprom->toggle_bit = eeprom->toggle_bit ^ 0x40; /* le-bit 6 */
  return eeprom->toggle_bit ^ byte;
}

static unsigned8
read_byte(device *me,
	  hw_eeprom_device *eeprom,
	  unsigned_word address)
{
  /* may need multiple iterations of this */
  while (1) {
    switch (eeprom->state) {

    case read_reset:
      return eeprom->memory[address];

    case autoselect:
      if ((address & 0xff) == 0x00)
	return eeprom->manufacture_code;
      else if ((address & 0xff) == 0x01)
	return eeprom->device_code;
      else
	return 0; /* not certain about this */

    case byte_programming:
      if (device_event_queue_time(me) > eeprom->program_finish_time) {
	finish_programming_byte(me, eeprom);
	eeprom->state = read_reset;
	continue;
      }
      else if (address == eeprom->byte_program_address) {
	return toggle(eeprom, eeprom->memory[address]);
      }
      else {
	/* trash that memory location */
	invalid_read(me, eeprom->state, address, "not byte program address");
	eeprom->memory[address] = (eeprom->memory[address]
				   & eeprom->byte_program_byte);
	return toggle(eeprom, eeprom->memory[eeprom->byte_program_address]);
      }

    case chip_erase:
      if (device_event_queue_time(me) > eeprom->program_finish_time) {
	finish_erasing_chip(me, eeprom);
	eeprom->state = read_reset;
	continue;
      }
      else {
	return toggle(eeprom, eeprom->memory[address]);
      }

    case sector_erase:
      if (device_event_queue_time(me) > eeprom->program_finish_time) {
	finish_erasing_sector(me, eeprom);
	eeprom->state = read_reset;
	continue;
      }
      else if (!eeprom->sectors[address / eeprom->sizeof_sector]) {
	/* read to wrong sector */
	invalid_read(me, eeprom->state, address, "sector not being erased");
	return toggle(eeprom, eeprom->memory[address]) & ~0x8;
      }
      else if (device_event_queue_time(me) > eeprom->sector_start_time) {
	return toggle(eeprom, eeprom->memory[address]) | 0x8;
      }
      else {
	return toggle(eeprom, eeprom->memory[address]) & ~0x8;
      }

    case sector_erase_suspend:
      if (!eeprom->sectors[address / eeprom->sizeof_sector]) {
	return eeprom->memory[address];
      }
      else {
	invalid_read(me, eeprom->state, address, "sector being erased");
	return eeprom->memory[address];
      }

    default:
      invalid_read(me, eeprom->state, address, "invalid state");
      return eeprom->memory[address];

    }
  }
  return 0;
}
		       
static unsigned
hw_eeprom_io_read_buffer(device *me,
			 void *dest,
			 int space,
			 unsigned_word addr,
			 unsigned nr_bytes,
			 cpu *processor,
			 unsigned_word cia)
{
  hw_eeprom_device *eeprom = (hw_eeprom_device*)device_data(me);
  int i;
  for (i = 0; i < nr_bytes; i++) {
    unsigned_word address = (addr + i) % eeprom->sizeof_memory;
    unsigned8 byte = read_byte(me, eeprom, address);
    ((unsigned8*)dest)[i] = byte;
  }
  return nr_bytes;
}


/* eeprom writes */

static void
write_byte(device *me,
	   hw_eeprom_device *eeprom,
	   unsigned_word address,
	   unsigned8 data)
{
  /* may need multiple transitions to process a write */
  while (1) {
    switch (eeprom->state) {

    case read_reset:
      if (address == 0x5555 && data == 0xaa)
	eeprom->state = write_nr_2;
      else if (data == 0xf0)
	eeprom->state = read_reset;
      else {
	invalid_write(me, eeprom->state, address, data, "unexpected");
	eeprom->state = read_reset;
      }
      return;

    case write_nr_2:
      if (address == 0x2aaa && data == 0x55)
	eeprom->state = write_nr_3;
      else {
	invalid_write(me, eeprom->state, address, data, "unexpected");
	eeprom->state = read_reset;
      }
      return;

    case write_nr_3:
      if (address == 0x5555 && data == 0xf0)
	eeprom->state = read_reset;
      else if (address == 0x5555 && data == 0x90)
	eeprom->state = autoselect;
      else if (address == 0x5555 && data == 0xa0) {
	eeprom->state = byte_program;
      }
      else if (address == 0x5555 && data == 0x80)
	eeprom->state = write_nr_4;
      else {
	invalid_write(me, eeprom->state, address, data, "unexpected");
	eeprom->state = read_reset;
      }
      return;

    case write_nr_4:
      if (address == 0x5555 && data == 0xaa)
	eeprom->state = write_nr_5;
      else {
	invalid_write(me, eeprom->state, address, data, "unexpected");
	eeprom->state = read_reset;
      }
      return;

    case write_nr_5:
      if (address == 0x2aaa && data == 0x55)
	eeprom->state = write_nr_6;
      else {
	invalid_write(me, eeprom->state, address, data, "unexpected");
	eeprom->state = read_reset;
      }
      return;

    case write_nr_6:
      if (address == 0x5555 && data == 0x10) {
	start_erasing_chip(me, eeprom);
	eeprom->state = chip_erase;
      }
      else {
	start_erasing_sector(me, eeprom, address);
	eeprom->sector_state = read_reset;
	eeprom->state = sector_erase;
      }
      return;

    case autoselect:
      if (data == 0xf0)
	eeprom->state = read_reset;
      else if (address == 0x5555 && data == 0xaa)
	eeprom->state = write_nr_2;
      else {
	invalid_write(me, eeprom->state, address, data, "unsupported address");
	eeprom->state = read_reset;
      }
      return;

    case byte_program:
      start_programming_byte(me, eeprom, address, data);
      eeprom->state = byte_programming;
      return;

    case byte_programming:
      if (device_event_queue_time(me) > eeprom->program_finish_time) {
	finish_programming_byte(me, eeprom);
	eeprom->state = read_reset;
	continue;
      }
      /* ignore it */
      return;

    case chip_erase:
      if (device_event_queue_time(me) > eeprom->program_finish_time) {
	finish_erasing_chip(me, eeprom);
	eeprom->state = read_reset;
	continue;
      }
      /* ignore it */
      return;

    case sector_erase:
      if (device_event_queue_time(me) > eeprom->program_finish_time) {
	finish_erasing_sector(me, eeprom);
	eeprom->state = eeprom->sector_state;
	continue;
      }
      else if (device_event_queue_time(me) > eeprom->sector_start_time
	       && data == 0xb0) {
	eeprom->sector_state = read_reset;
	eeprom->state = sector_erase_suspend;
      }
      else {
	if (eeprom->sector_state == read_reset
	    && address == 0x5555 && data == 0xaa)
	  eeprom->sector_state = write_nr_2;
	else if (eeprom->sector_state == write_nr_2
		 && address == 0x2aaa && data == 0x55)
	  eeprom->sector_state = write_nr_3;
	else if (eeprom->sector_state == write_nr_3
		 && address == 0x5555 && data == 0x80)
	  eeprom->sector_state = write_nr_4;
	else if (eeprom->sector_state == write_nr_4
		 && address == 0x5555 && data == 0xaa)
	  eeprom->sector_state = write_nr_5;
	else if (eeprom->sector_state == write_nr_5
		 && address == 0x2aaa && data == 0x55)
	  eeprom->sector_state = write_nr_6;
	else if (eeprom->sector_state == write_nr_6
		 && address != 0x5555 && data == 0x30) {
	  if (device_event_queue_time(me) > eeprom->sector_start_time) {
	    DTRACE(eeprom, ("sector erase command after window closed\n"));
	    eeprom->sector_state = read_reset;
	  }
	  else {
	    start_erasing_sector(me, eeprom, address);
	    eeprom->sector_state = read_reset;
	  }
	}
	else {
	  invalid_write(me, eeprom->state, address, data, state2a(eeprom->sector_state));
	  eeprom->state = read_reset;
	}
      }
      return;

    case sector_erase_suspend:
      if (data == 0x30)
	eeprom->state = sector_erase;
      else {
	invalid_write(me, eeprom->state, address, data, "not resume command");
	eeprom->state = read_reset;
      }
      return;

    }
  }
}

static unsigned
hw_eeprom_io_write_buffer(device *me,
			  const void *source,
			  int space,
			  unsigned_word addr,
			  unsigned nr_bytes,
			  cpu *processor,
			  unsigned_word cia)
{
  hw_eeprom_device *eeprom = (hw_eeprom_device*)device_data(me);
  int i;
  for (i = 0; i < nr_bytes; i++) {
    unsigned_word address = (addr + i) % eeprom->sizeof_memory;
    unsigned8 byte = ((unsigned8*)source)[i];
    write_byte(me, eeprom, address, byte);
  }
  return nr_bytes;
}


/* An instance of the eeprom */

typedef struct _hw_eeprom_instance {
  unsigned_word pos;
  hw_eeprom_device *eeprom;
  device *me;
} hw_eeprom_instance;

static void
hw_eeprom_instance_delete(device_instance *instance)
{
  hw_eeprom_instance *data = device_instance_data(instance);
  zfree(data);
}

static int
hw_eeprom_instance_read(device_instance *instance,
			void *buf,
			unsigned_word len)
{
  hw_eeprom_instance *data = device_instance_data(instance);
  int i;
  if (data->eeprom->state != read_reset)
    DITRACE(eeprom, ("eeprom not idle during instance read\n"));
  for (i = 0; i < len; i++) {
    ((unsigned8*)buf)[i] = data->eeprom->memory[data->pos];
    data->pos = (data->pos + 1) % data->eeprom->sizeof_memory;
  }
  return len;
}

static int
hw_eeprom_instance_write(device_instance *instance,
			 const void *buf,
			 unsigned_word len)
{
  hw_eeprom_instance *data = device_instance_data(instance);
  int i;
  if (data->eeprom->state != read_reset)
    DITRACE(eeprom, ("eeprom not idle during instance write\n"));
  for (i = 0; i < len; i++) {
    data->eeprom->memory[data->pos] = ((unsigned8*)buf)[i];
    data->pos = (data->pos + 1) % data->eeprom->sizeof_memory;
  }
  dump_eeprom(data->me, data->eeprom);
  return len;
}

static int
hw_eeprom_instance_seek(device_instance *instance,
		      unsigned_word pos_hi,
		      unsigned_word pos_lo)
{
  hw_eeprom_instance *data = device_instance_data(instance);
  if (pos_lo >= data->eeprom->sizeof_memory)
    device_error(data->me, "seek value 0x%lx out of range\n",
		 (unsigned long)pos_lo);
  data->pos = pos_lo;
  return 0;
}

static const device_instance_callbacks hw_eeprom_instance_callbacks = {
  hw_eeprom_instance_delete,
  hw_eeprom_instance_read,
  hw_eeprom_instance_write,
  hw_eeprom_instance_seek,
};

static device_instance *
hw_eeprom_create_instance(device *me,
			  const char *path,
			  const char *args)
{
  hw_eeprom_device *eeprom = device_data(me);
  hw_eeprom_instance *data = ZALLOC(hw_eeprom_instance);
  data->eeprom = eeprom;
  data->me = me;
  return device_create_instance_from(me, NULL,
				     data,
				     path, args,
				     &hw_eeprom_instance_callbacks);
}



static device_callbacks const hw_eeprom_callbacks = {
  { generic_device_init_address,
    hw_eeprom_init_data },
  { NULL, }, /* address */
  { hw_eeprom_io_read_buffer,
    hw_eeprom_io_write_buffer }, /* IO */
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { NULL, }, /* unit */
  hw_eeprom_create_instance,
};

static void *
hw_eeprom_create(const char *name,
		 const device_unit *unit_address,
		 const char *args)
{
  hw_eeprom_device *eeprom = ZALLOC(hw_eeprom_device);
  return eeprom;
}



const device_descriptor hw_eeprom_device_descriptor[] = {
  { "eeprom", hw_eeprom_create, &hw_eeprom_callbacks },
  { NULL },
};

#endif /* _HW_EEPROM_C_ */
