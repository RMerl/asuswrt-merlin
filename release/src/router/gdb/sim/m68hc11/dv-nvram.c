/*  dv-nvram.c -- Generic driver for a non volatile ram (battery saved)
    Copyright (C) 1999, 2000, 2007 Free Software Foundation, Inc.
    Written by Stephane Carrez (stcarrez@worldnet.fr)
    (From a driver model Contributed by Cygnus Solutions.)
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    
    */


#include "sim-main.h"
#include "hw-main.h"
#include "sim-assert.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


/* DEVICE

        nvram - Non Volatile Ram

   
   DESCRIPTION

        Implements a generic battery saved CMOS ram. This ram device does
        not contain any realtime clock and does not generate any interrupt.
        The ram content is loaded from a file and saved when it is changed.
        It is intended to be generic.

   
   PROPERTIES

   reg <base> <length>

        Base and size of the non-volatile ram bank.

   file <path>

        Path where the memory must be saved or loaded when we start.

   mode {map | save-modified | save-all}

        Controls how to load and save the memory content.

           map            The file is mapped in memory
           save-modified  The simulator keeps an open file descriptor to
                          the file and saves portion of memory which are
                          modified. 
           save-all       The simulator saves the complete memory each time
                          it's modified (it does not keep an open file
                          descriptor).


   PORTS

        None.


   NOTES

        This device is independent of the Motorola 68hc11.

   */



/* static functions */

/* Control of how to access the ram and save its content.  */

enum nvram_mode
{
  /* Save the complete ram block each time it's changed.
     We don't keep an open file descriptor.  This should be
     ok for small memory banks.  */
  NVRAM_SAVE_ALL,

  /* Save only the memory bytes which are modified.
     This mode means that we have to keep an open file
     descriptor (O_RDWR).  It's good for middle sized memory banks.  */
  NVRAM_SAVE_MODIFIED,

  /* Map file in memory (not yet implemented).
     This mode is suitable for large memory banks.  We don't allocate
     a buffer to represent the ram, instead it's mapped in memory
     with mmap.  */
  NVRAM_MAP_FILE
};

struct nvram 
{
  address_word    base_address; /* Base address of ram.  */
  unsigned        size;         /* Size of ram.  */
  unsigned8       *data;        /* Pointer to ram memory.  */
  const char      *file_name;   /* Path of ram file.  */
  int             fd;           /* File description of opened ram file.  */
  enum nvram_mode mode;         /* How load/save ram file.  */
};



/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc.  */

static hw_io_read_buffer_method  nvram_io_read_buffer;
static hw_io_write_buffer_method nvram_io_write_buffer;



static void
attach_nvram_regs (struct hw *me, struct nvram *controller)
{
  unsigned_word attach_address;
  int attach_space;
  unsigned attach_size;
  reg_property_spec reg;
  int result, oerrno;

  /* Get ram bank description (base and size).  */
  if (hw_find_property (me, "reg") == NULL)
    hw_abort (me, "Missing \"reg\" property");

  if (!hw_find_reg_array_property (me, "reg", 0, &reg))
    hw_abort (me, "\"reg\" property must contain one addr/size entry");

  hw_unit_address_to_attach_address (hw_parent (me),
				     &reg.address,
				     &attach_space,
				     &attach_address,
				     me);
  hw_unit_size_to_attach_size (hw_parent (me),
			       &reg.size,
			       &attach_size, me);

  hw_attach_address (hw_parent (me), 0,
		     attach_space, attach_address, attach_size,
		     me);

  controller->mode         = NVRAM_SAVE_ALL;
  controller->base_address = attach_address;
  controller->size         = attach_size;
  controller->fd           = -1;
  
  /* Get the file where the ram content must be loaded/saved.  */
  if(hw_find_property (me, "file") == NULL)
    hw_abort (me, "Missing \"file\" property");
  
  controller->file_name = hw_find_string_property (me, "file");

  /* Get the mode which defines how to save the memory.  */
  if(hw_find_property (me, "mode") != NULL)
    {
      const char *value = hw_find_string_property (me, "mode");

      if (strcmp (value, "map") == 0)
        controller->mode = NVRAM_MAP_FILE;
      else if (strcmp (value, "save-modified") == 0)
        controller->mode = NVRAM_SAVE_MODIFIED;
      else if (strcmp (value, "save-all") == 0)
        controller->mode = NVRAM_SAVE_ALL;
      else
	hw_abort (me, "illegal value for mode parameter `%s': "
                  "use map, save-modified or save-all", value);
    }

  /* Initialize the ram by loading/mapping the file in memory.
     If the file does not exist, create and give it some content.  */
  switch (controller->mode)
    {
    case NVRAM_MAP_FILE:
      hw_abort (me, "'map' mode is not yet implemented, use 'save-modified'");
      break;

    case NVRAM_SAVE_MODIFIED:
    case NVRAM_SAVE_ALL:
      controller->data = (char*) hw_malloc (me, attach_size);
      if (controller->data == 0)
        hw_abort (me, "Not enough memory, try to use the mode 'map'");

      memset (controller->data, 0, attach_size);
      controller->fd = open (controller->file_name, O_RDWR);
      if (controller->fd < 0)
        {
          controller->fd = open (controller->file_name,
                                 O_RDWR | O_CREAT, 0644);
          if (controller->fd < 0)
            hw_abort (me, "Cannot open or create file '%s'",
                      controller->file_name);
          result = write (controller->fd, controller->data, attach_size);
          if (result != attach_size)
            {
              oerrno = errno;
              hw_free (me, controller->data);
              close (controller->fd);
              errno = oerrno;
              hw_abort (me, "Failed to save the ram content");
            }
        }
      else
        {
          result = read (controller->fd, controller->data, attach_size);
          if (result != attach_size)
            {
              oerrno = errno;
              hw_free (me, controller->data);
              close (controller->fd);
              errno = oerrno;
              hw_abort (me, "Failed to load the ram content");
            }
        }
      if (controller->mode == NVRAM_SAVE_ALL)
        {
          close (controller->fd);
          controller->fd = -1;
        }
      break;

    default:
      break;
    }
}


static void
nvram_finish (struct hw *me)
{
  struct nvram *controller;

  controller = HW_ZALLOC (me, struct nvram);

  set_hw_data (me, controller);
  set_hw_io_read_buffer (me, nvram_io_read_buffer);
  set_hw_io_write_buffer (me, nvram_io_write_buffer);

  /* Attach ourself to our parent bus.  */
  attach_nvram_regs (me, controller);
}



/* generic read/write */

static unsigned
nvram_io_read_buffer (struct hw *me,
                      void *dest,
                      int space,
                      unsigned_word base,
                      unsigned nr_bytes)
{
  struct nvram *controller = hw_data (me);
  
  HW_TRACE ((me, "read 0x%08lx %d [%ld]",
             (long) base, (int) nr_bytes,
             (long) (base - controller->base_address)));

  base -= controller->base_address;
  if (base + nr_bytes > controller->size)
    nr_bytes = controller->size - base;
  
  memcpy (dest, &controller->data[base], nr_bytes);
  return nr_bytes;
}



static unsigned
nvram_io_write_buffer (struct hw *me,
                       const void *source,
                       int space,
                       unsigned_word base,
                       unsigned nr_bytes)
{
  struct nvram *controller = hw_data (me);

  HW_TRACE ((me, "write 0x%08lx %d [%ld]",
             (long) base, (int) nr_bytes,
             (long) (base - controller->base_address)));

  base -= controller->base_address;
  if (base + nr_bytes > controller->size)
    nr_bytes = controller->size - base;
  
  switch (controller->mode)
    {
    case NVRAM_SAVE_ALL:
      {
        int fd, result, oerrno;
        
        fd = open (controller->file_name, O_WRONLY, 0644);
        if (fd < 0)
          {
            return 0;
          }

        memcpy (&controller->data[base], source, nr_bytes);
        result = write (fd, controller->data, controller->size);
        oerrno = errno;
        close (fd);
        errno = oerrno;
  
        if (result != controller->size)
          {
            return 0;
          }
        return nr_bytes;
      }
      
    case NVRAM_SAVE_MODIFIED:
      {
        off_t pos;
        int result;

        pos = lseek (controller->fd, (off_t) base, SEEK_SET);
        if (pos != (off_t) base)
          return 0;

        result = write (controller->fd, source, nr_bytes);
        if (result < 0)
          return 0;

        nr_bytes = result;
        break;
      }

    default:
      break;
    }
  memcpy (&controller->data[base], source, nr_bytes);
  return nr_bytes;
}


const struct hw_descriptor dv_nvram_descriptor[] = {
  { "nvram", nvram_finish, },
  { NULL },
};

