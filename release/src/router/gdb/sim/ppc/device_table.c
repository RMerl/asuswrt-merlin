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


#ifndef _DEVICE_TABLE_C_
#define _DEVICE_TABLE_C_

#include "device_table.h"

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <ctype.h>


/* Helper functions */


/* Go through the devices various reg properties for those that
   specify attach addresses */


void
generic_device_init_address(device *me)
{
  static const char *(reg_property_names[]) = {
    "attach-addresses",
    "assigned-addresses",
    "reg",
    "alternate-reg" ,
    NULL
  };
  const char **reg_property_name;
  int nr_valid_reg_properties = 0;
  for (reg_property_name = reg_property_names;
       *reg_property_name != NULL;
       reg_property_name++) {
    if (device_find_property(me, *reg_property_name) != NULL) {
      reg_property_spec reg;
      int reg_entry;
      for (reg_entry = 0;
	   device_find_reg_array_property(me, *reg_property_name, reg_entry,
					  &reg);
	   reg_entry++) {
	unsigned_word attach_address;
	int attach_space;
	unsigned attach_size;
	if (!device_address_to_attach_address(device_parent(me),
					      &reg.address,
					      &attach_space, &attach_address,
					      me))
	  continue;
	if (!device_size_to_attach_size(device_parent(me),
					&reg.size,
					&attach_size, me))
	  continue;
	device_attach_address(device_parent(me),
			      attach_callback,
			      attach_space, attach_address, attach_size,
			      access_read_write_exec,
			      me);
	nr_valid_reg_properties++;
      }
      /* if first option matches don't try for any others */
      if (reg_property_name == reg_property_names)
	break;
    }
  }
}

int
generic_device_unit_decode(device *bus,
			   const char *unit,
			   device_unit *phys)
{
  memset(phys, 0, sizeof(device_unit));
  if (unit == NULL)
    return 0;
  else {
    int nr_cells = 0;
    const int max_nr_cells = device_nr_address_cells(bus);
    while (1) {
      char *end = NULL;
      unsigned long val;
      val = strtoul(unit, &end, 0);
      /* parse error? */
      if (unit == end)
	return -1;
      /* two many cells? */
      if (nr_cells >= max_nr_cells)
	return -1;
      /* save it */
      phys->cells[nr_cells] = val;
      nr_cells++;
      unit = end;
      /* more to follow? */
      if (isspace(*unit) || *unit == '\0')
	break;
      if (*unit != ',')
	return -1;
      unit++;
    }
    if (nr_cells < max_nr_cells) {
      /* shift everything to correct position */
      int i;
      for (i = 1; i <= nr_cells; i++)
	phys->cells[max_nr_cells - i] = phys->cells[nr_cells - i];
      for (i = 0; i < (max_nr_cells - nr_cells); i++)
	phys->cells[i] = 0;
    }
    phys->nr_cells = max_nr_cells;
    return max_nr_cells;
  }
}

int
generic_device_unit_encode(device *bus,
			   const device_unit *phys,
			   char *buf,
			   int sizeof_buf)
{
  int i;
  int len;
  char *pos = buf;
  /* skip leading zero's */
  for (i = 0; i < phys->nr_cells; i++) {
    if (phys->cells[i] != 0)
      break;
  }
  /* don't output anything if empty */
  if (phys->nr_cells == 0) {
    strcpy(pos, "");
    len = 0;
  }
  else if (i == phys->nr_cells) {
    /* all zero */
    strcpy(pos, "0");
    len = 1;
  }
  else {
    for (; i < phys->nr_cells; i++) {
      if (pos != buf) {
	strcat(pos, ",");
	pos = strchr(pos, '\0');
      }
      if (phys->cells[i] < 10)
	sprintf(pos, "%ld", (unsigned long)phys->cells[i]);
      else
	sprintf(pos, "0x%lx", (unsigned long)phys->cells[i]);
      pos = strchr(pos, '\0');
    }
    len = pos - buf;
  }
  if (len >= sizeof_buf)
    error("generic_unit_encode - buffer overflow\n");
  return len;
}

int
generic_device_address_to_attach_address(device *me,
					 const device_unit *address,
					 int *attach_space,
					 unsigned_word *attach_address,
					 device *client)
{
  int i;
  for (i = 0; i < address->nr_cells - 2; i++) {
    if (address->cells[i] != 0)
      device_error(me, "Only 32bit addresses supported");
  }
  if (address->nr_cells >= 2)
    *attach_space = address->cells[address->nr_cells - 2];
  else
    *attach_space = 0;
  *attach_address = address->cells[address->nr_cells - 1];
  return 1;
}

int
generic_device_size_to_attach_size(device *me,
				   const device_unit *size,
				   unsigned *nr_bytes,
				   device *client)
{
  int i;
  for (i = 0; i < size->nr_cells - 1; i++) {
    if (size->cells[i] != 0)
      device_error(me, "Only 32bit sizes supported");
  }
  *nr_bytes = size->cells[0];
  return *nr_bytes;
}


/* ignore/passthrough versions of each function */

void
passthrough_device_address_attach(device *me,
				  attach_type attach,
				  int space,
				  unsigned_word addr,
				  unsigned nr_bytes,
				  access_type access,
				  device *client) /*callback/default*/
{
  device_attach_address(device_parent(me), attach,
			space, addr, nr_bytes,
			access,
			client);
}

void
passthrough_device_address_detach(device *me,
				  attach_type attach,
				  int space,
				  unsigned_word addr,
				  unsigned nr_bytes,
				  access_type access,
				  device *client) /*callback/default*/
{
  device_detach_address(device_parent(me), attach,
			space, addr, nr_bytes, access,
			client);
}

unsigned
passthrough_device_dma_read_buffer(device *me,
				   void *dest,
				   int space,
				   unsigned_word addr,
				   unsigned nr_bytes)
{
  return device_dma_read_buffer(device_parent(me), dest,
				space, addr, nr_bytes);
}

unsigned
passthrough_device_dma_write_buffer(device *me,
			     const void *source,
			     int space,
			     unsigned_word addr,
			     unsigned nr_bytes,
			     int violate_read_only_section)
{
  return device_dma_write_buffer(device_parent(me), source,
				 space, addr,
				 nr_bytes,
				 violate_read_only_section);
}

int
ignore_device_unit_decode(device *me,
			  const char *unit,
			  device_unit *phys)
{
  memset(phys, 0, sizeof(device_unit));
  return 0;
}


static const device_callbacks passthrough_callbacks = {
  { NULL, }, /* init */
  { passthrough_device_address_attach,
    passthrough_device_address_detach, },
  { NULL, }, /* IO */
  { passthrough_device_dma_read_buffer, passthrough_device_dma_write_buffer, },
  { NULL, }, /* interrupt */
  { generic_device_unit_decode,
    generic_device_unit_encode, },
};


static const device_descriptor ob_device_table[] = {
  /* standard OpenBoot devices */
  { "aliases", NULL, &passthrough_callbacks },
  { "options", NULL, &passthrough_callbacks },
  { "chosen", NULL, &passthrough_callbacks },
  { "packages", NULL, &passthrough_callbacks },
  { "cpus", NULL, &passthrough_callbacks },
  { "openprom", NULL, &passthrough_callbacks },
  { "init", NULL, &passthrough_callbacks },
  { NULL },
};

const device_descriptor *const device_table[] = {
  ob_device_table,
#include "hw.c"
  NULL,
};


#endif /* _DEVICE_TABLE_C_ */
