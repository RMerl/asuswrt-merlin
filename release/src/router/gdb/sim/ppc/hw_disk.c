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


#ifndef _HW_DISK_C_
#define _HW_DISK_C_

#include "device_table.h"

#include "pk.h"

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef	SEEK_SET
#define	SEEK_SET 0
#endif

/* DEVICE
   

   cdrom - read-only removable mass storage device

   disk - mass storage device
   
   floppy - removable mass storage device

   
   DESCRIPTION
   
   
   Mass storage devices such as a hard-disk or cdrom-drive are not
   normally directly connected to the processor.  Instead, these
   devices are attached to a logical bus, such as SCSI or IDE, and
   then a controller of that bus is made accessible to the processor.
   
   Reflecting this, within a device tree, mass storage devices such as
   a <<cdrom>>, <<disk>> or <<floppy>> are created as children of of a
   logical bus controller node (such as a SCSI or IDE interface).
   That controller, in turn, would be made the child of a physical bus
   node that is directly accessible to the processor.
   
   The above mass storage devices provide two interfaces - a logical
   and a physical.
   
   At the physical level the <<device_io_...>> functions can be used
   perform reads and writes of the raw media.  The address being
   interpreted as an offset from the start of the disk.
   
   At the logical level, it is possible to create an instance of the
   disk that provides access to any of the physical media, a disk
   partition, or even a file within a partition.  The <<disk-label>>
   package, which implements this functionality, is described
   elsewhere.  Both the Open Firmware and Moto BUG rom emulations
   support this interface.
   
   Block devices such as the <<floppy>> and <<cdrom>> have removable
   media.  At the programmer level, the media can be changed using the
   <<change_media>> ioctl.  From within GDB, a <<change-media>>
   operation can be initated by using the command.

   |	(gdb)  sim 


   PROPERTIES
   
   
   file = <file-name>  (required)

   The name of the file that contains an image of the disk.  For
   <<disk>> and <<floppy>> devices, the image will be opened for both
   reading and writing.  Multiple image files may be specified, the
   second and later files being opened when <<change-media>> (with a
   NULL file name) being specified.

   
   block-size = <nr-bytes>  (optional)

   The value is returned by the block-size method.  The default value
   is 512 bytes.


   max-transfer = <nr-bytes>  (optional)

   The value is returned by the max-transfer method. The default value
   is 512 bytes.


   #blocks = <nr-blocks>  (optional)

   The value is returned by the #blocks method.  If no value is
   present then -1 is returned.


   read-only = <anything>  (optional)

   If this property is present, the disk file image is always opened
   read-only.

   EXAMPLES
   
   
   Enable tracing
   
   | $  psim -t 'disk-device' \

   
   Add a CDROM and disk to an IDE bus.  Specify the host operating
   system's cd drive as the CD-ROM image.
   
   |    -o '/pci/ide/disk@0/file "disk-image' \
   |    -o '/pci/ide/cdrom@1/file "/dev/cd0a' \

   
   As part of the code implementing a logical bus device (for instance
   the IDE controller), locate the CDROM device and then read block
   47.
   
   |  device *cdrom = device_tree_find_device(me, "cdrom");
   |  char block[512];
   |  device_io_read_buffer(cdrom, buf, 0,
                            0, 47 * sizeof(block), // space, address
                            sizeof(block), NULL, 0);
   
   
   Use the device instance interface to read block 47 of the file
   called <<netbsd.elf>> on the disks default partition.  Similar code
   would be used in an operating systems pre-boot loader.
   
   |  device_instance *netbsd =
   |    device_create_instance(root, "/pci/ide/disk:,\netbsd.elf");
   |  char block[512];
   |  device_instance_seek(netbsd,  0, 47 * sizeof(block));
   |  device_instance_read(netbsd, block, sizeof(block));
   
   
   BUGS
   
   
   The block device specification includes mechanisms for determining
   the physical device characteristics - such as the disks size.
   Currently this mechanism is not implemented.
   
   The functionality of this device (in particular the device instance
   interface) depends on the implementation of <<disk-label>> package.
   That package may not be fully implemented.

   The disk does not know its size.  Hence it relies on the failure of
   fread(), fwrite() and fseek() calls to detect errors.

   The disk size is limited by the addressable range covered by
   unsigned_word (addr).  An extension would be to instead use the
   concatenated value space:addr.

   The method #blocks should `stat' the disk to determine the number
   of blocks if there is no #blocks property.

   It would appear that OpenFirmware does not define a client call for
   changing (ejecting) the media of a device.

   */

typedef struct _hw_disk_device {
  int name_index;
  int nr_names;
  char *name;
  int read_only;
  /*  unsigned_word size; */
  FILE *image;
} hw_disk_device;

typedef struct _hw_disk_instance {
  unsigned_word pos;
  hw_disk_device *disk;
} hw_disk_instance;


static void
open_disk_image(device *me,
		hw_disk_device *disk,
		const char *name)
{
  if (disk->image != NULL)
    fclose(disk->image);
  if (disk->name != NULL)
    zfree(disk->name);
  disk->name = strdup(name);
  disk->image = fopen(disk->name, disk->read_only ? "r" : "r+");
  if (disk->image == NULL) {
    perror(device_name(me));
    device_error(me, "open %s failed\n", disk->name);
  }

  DTRACE(disk, ("image %s (%s)\n",
                disk->name,
                (disk->read_only ? "read-only" : "read-write")));
}

static void
hw_disk_init_address(device *me)
{
  hw_disk_device *disk = device_data(me);
  unsigned_word address;
  int space;
  const char *name;

  /* attach to the parent. Since the bus is logical, attach using just
     the unit-address (size must be zero) */
  device_address_to_attach_address(device_parent(me), device_unit_address(me),
				   &space, &address, me);
  device_attach_address(device_parent(me), attach_callback,
			space, address, 0/*size*/, access_read_write_exec,
			me);

  /* Tell the world we are a disk.  */
  device_add_string_property(me, "device_type", "block");

  /* get the name of the file specifying the disk image */
  disk->name_index = 0;
  disk->nr_names = device_find_string_array_property(me, "file",
						     disk->name_index, &name);
  if (!disk->nr_names)
    device_error(me, "invalid file property");

  /* is it a RO device? */
  disk->read_only =
    (strcmp(device_name(me), "disk") != 0
     && strcmp(device_name(me), "floppy") != 0
     && device_find_property(me, "read-only") == NULL);

  /* now open it */
  open_disk_image(me, disk, name);
}

static int
hw_disk_ioctl(device *me,
	      cpu *processor,
	      unsigned_word cia,
	      device_ioctl_request request,
	      va_list ap)
{
  switch (request) {
  case device_ioctl_change_media:
    {
      hw_disk_device *disk = device_data(me);
      const char *name = va_arg(ap, const char *);
      if (name != NULL) {
	disk->name_index = -1;
      }
      else {
	disk->name_index = (disk->name_index + 1) % disk->nr_names;
	if (!device_find_string_array_property(me, "file",
					       disk->name_index, &name))
	  device_error(me, "invalid file property");
      }
      open_disk_image(me, disk, name);
    }
    break;
  default:
    device_error(me, "insupported ioctl request");
    break;
  }
  return 0;
}





static unsigned
hw_disk_io_read_buffer(device *me,
		       void *dest,
		       int space,
		       unsigned_word addr,
		       unsigned nr_bytes,
		       cpu *processor,
		       unsigned_word cia)
{
  hw_disk_device *disk = device_data(me);
  unsigned nr_bytes_read;
  if (space != 0)
    device_error(me, "read - extended disk addressing unimplemented");
  if (nr_bytes == 0)
    nr_bytes_read = 0;
  else if (fseek(disk->image, addr, SEEK_SET) < 0)
    nr_bytes_read = 0;
  else if (fread(dest, nr_bytes, 1, disk->image) != 1)
    nr_bytes_read = 0;
  else
    nr_bytes_read = nr_bytes;
  DTRACE(disk, ("io-read - address 0x%lx, nr-bytes-read %d, requested %d\n",
                (unsigned long) addr, (int)nr_bytes_read, (int)nr_bytes));
  return nr_bytes_read;
}


static unsigned
hw_disk_io_write_buffer(device *me,
			const void *source,
			int space,
			unsigned_word addr,
			unsigned nr_bytes,
			cpu *processor,
			unsigned_word cia)
{
  hw_disk_device *disk = device_data(me);
  unsigned nr_bytes_written;
  if (space != 0)
    device_error(me, "write - extended disk addressing unimplemented");
  if (disk->read_only)
    nr_bytes_written = 0;
  else if (nr_bytes == 0)
    nr_bytes_written = 0;
  else if (fseek(disk->image, addr, SEEK_SET) < 0)
    nr_bytes_written = 0;
  else if (fwrite(source, nr_bytes, 1, disk->image) != 1)
    nr_bytes_written = 0;
  else
    nr_bytes_written = nr_bytes;
  DTRACE(disk, ("io-write - address 0x%lx, nr-bytes-written %d, requested %d\n",
                (unsigned long) addr, (int)nr_bytes_written, (int)nr_bytes));
  return nr_bytes_written;
}


/* instances of the hw_disk device */

static void
hw_disk_instance_delete(device_instance *instance)
{
  hw_disk_instance *data = device_instance_data(instance);
  DITRACE(disk, ("delete - instance=%ld\n",
		 (unsigned long)device_instance_to_external(instance)));
  zfree(data);
}

static int
hw_disk_instance_read(device_instance *instance,
		      void *buf,
		      unsigned_word len)
{
  hw_disk_instance *data = device_instance_data(instance);
  DITRACE(disk, ("read - instance=%ld len=%ld\n",
		 (unsigned long)device_instance_to_external(instance),
		 (long)len));
  if ((data->pos + len) < data->pos)
    return -1; /* overflow */
  if (fseek(data->disk->image, data->pos, SEEK_SET) < 0)
    return -1;
  if (fread(buf, len, 1, data->disk->image) != 1)
    return -1;
  data->pos = ftell(data->disk->image);
  return len;
}

static int
hw_disk_instance_write(device_instance *instance,
		       const void *buf,
		       unsigned_word len)
{
  hw_disk_instance *data = device_instance_data(instance);
  DITRACE(disk, ("write - instance=%ld len=%ld\n",
		 (unsigned long)device_instance_to_external(instance),
		 (long)len));
  if ((data->pos + len) < data->pos)
    return -1; /* overflow */
  if (data->disk->read_only)
    return -1;
  if (fseek(data->disk->image, data->pos, SEEK_SET) < 0)
    return -1;
  if (fwrite(buf, len, 1, data->disk->image) != 1)
    return -1;
  data->pos = ftell(data->disk->image);
  return len;
}

static int
hw_disk_instance_seek(device_instance *instance,
		      unsigned_word pos_hi,
		      unsigned_word pos_lo)
{
  hw_disk_instance *data = device_instance_data(instance);
  if (pos_hi != 0)
    device_error(device_instance_device(instance),
		 "seek - extended addressing unimplemented");
  DITRACE(disk, ("seek - instance=%ld pos_hi=%ld pos_lo=%ld\n",
		 (unsigned long)device_instance_to_external(instance),
		 (long)pos_hi, (long)pos_lo));
  data->pos = pos_lo;
  return 0;
}

static int
hw_disk_max_transfer(device_instance *instance,
		     int n_stack_args,
		     unsigned32 stack_args[/*n_stack_args*/],
		     int n_stack_returns,
		     unsigned32 stack_returns[/*n_stack_returns*/])
{
  device *me = device_instance_device(instance);
  if ((n_stack_args != 0)
      || (n_stack_returns != 1)) {
    device_error(me, "Incorrect number of arguments for max-transfer method\n");
    return -1;
  }
  else {
    unsigned_cell max_transfer;
    if (device_find_property(me, "max-transfer"))
      max_transfer = device_find_integer_property(me, "max-transfer");
    else
      max_transfer = 512;
    DITRACE(disk, ("max-transfer - instance=%ld max-transfer=%ld\n",
		   (unsigned long)device_instance_to_external(instance),
		   (long int)max_transfer));
    stack_returns[0] = max_transfer;
    return 0;
  }
}

static int
hw_disk_block_size(device_instance *instance,
		   int n_stack_args,
		   unsigned32 stack_args[/*n_stack_args*/],
		   int n_stack_returns,
		   unsigned32 stack_returns[/*n_stack_returns*/])
{
  device *me = device_instance_device(instance);
  if ((n_stack_args != 0)
      || (n_stack_returns != 1)) {
    device_error(me, "Incorrect number of arguments for block-size method\n");
    return -1;
  }
  else {
    unsigned_cell block_size;
    if (device_find_property(me, "block-size"))
      block_size = device_find_integer_property(me, "block-size");
    else
      block_size = 512;
    DITRACE(disk, ("block-size - instance=%ld block-size=%ld\n",
		   (unsigned long)device_instance_to_external(instance),
		   (long int)block_size));
    stack_returns[0] = block_size;
    return 0;
  }
}

static int
hw_disk_nr_blocks(device_instance *instance,
		  int n_stack_args,
		  unsigned32 stack_args[/*n_stack_args*/],
		  int n_stack_returns,
		  unsigned32 stack_returns[/*n_stack_returns*/])
{
  device *me = device_instance_device(instance);
  if ((n_stack_args != 0)
      || (n_stack_returns != 1)) {
    device_error(me, "Incorrect number of arguments for block-size method\n");
    return -1;
  }
  else {
    unsigned_word nr_blocks;
    if (device_find_property(me, "#blocks"))
      nr_blocks = device_find_integer_property(me, "#blocks");
    else
      nr_blocks = -1;
    DITRACE(disk, ("#blocks - instance=%ld #blocks=%ld\n",
		   (unsigned long)device_instance_to_external(instance),
		   (long int)nr_blocks));
    stack_returns[0] = nr_blocks;
    return 0;
  }
}

static device_instance_methods hw_disk_instance_methods[] = {
  { "max-transfer", hw_disk_max_transfer },
  { "block-size", hw_disk_block_size },
  { "#blocks", hw_disk_nr_blocks },
  { NULL, },
};

static const device_instance_callbacks hw_disk_instance_callbacks = {
  hw_disk_instance_delete,
  hw_disk_instance_read,
  hw_disk_instance_write,
  hw_disk_instance_seek,
  hw_disk_instance_methods,
};

static device_instance *
hw_disk_create_instance(device *me,
			const char *path,
			const char *args)
{
  device_instance *instance;
  hw_disk_device *disk = device_data(me);
  hw_disk_instance *data = ZALLOC(hw_disk_instance);
  data->disk = disk;
  data->pos = 0;
  instance = device_create_instance_from(me, NULL,
					 data,
					 path, args,
					 &hw_disk_instance_callbacks);
  DITRACE(disk, ("create - path=%s(%s) instance=%ld\n",
		 path, args,
		 (unsigned long)device_instance_to_external(instance)));
  return pk_disklabel_create_instance(instance, args);
}

static device_callbacks const hw_disk_callbacks = {
  { hw_disk_init_address, NULL },
  { NULL, }, /* address */
  { hw_disk_io_read_buffer,
      hw_disk_io_write_buffer, },
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { NULL, }, /* unit */
  hw_disk_create_instance,
  hw_disk_ioctl,
};


static void *
hw_disk_create(const char *name,
	       const device_unit *unit_address,
	       const char *args)
{
  /* create the descriptor */
  hw_disk_device *hw_disk = ZALLOC(hw_disk_device);
  return hw_disk;
}


const device_descriptor hw_disk_device_descriptor[] = {
  { "disk", hw_disk_create, &hw_disk_callbacks },
  { "cdrom", hw_disk_create, &hw_disk_callbacks },
  { "floppy", hw_disk_create, &hw_disk_callbacks },
  { NULL },
};

#endif /* _HW_DISK_C_ */
