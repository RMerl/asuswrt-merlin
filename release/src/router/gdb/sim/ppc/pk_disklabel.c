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


#ifndef _PK_DISKLABEL_C_
#define _PK_DISKLABEL_C_

#ifndef STATIC_INLINE_PK_DISKLABEL
#define STATIC_INLINE_PK_DISKLABEL STATIC_INLINE
#endif

#include "device_table.h"

#include "pk.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif



/* PACKAGE

   disk-label - all knowing disk I/O package

   DESCRIPTION

   The disk-label package provides a generic interface to disk
   devices.  It uses the arguments specified when an instance is being
   created to determine if the raw disk, a partition, or a file within
   a partition should be opened.

   An instance create call to disk-label could result, for instance,
   in the opening of a DOS file system contained within a dos
   partition contained within a physical disk.

   */

/* taken from bfd/ppcboot.c by Michael Meissner */

/* PPCbug location structure */
typedef struct ppcboot_location {
  unsigned8 ind;
  unsigned8 head;
  unsigned8 sector;
  unsigned8 cylinder;
} ppcboot_location_t;

/* PPCbug partition table layout */
typedef struct ppcboot_partition {
  ppcboot_location_t partition_begin;	/* partition begin */
  ppcboot_location_t partition_end;	/* partition end */
  unsigned8 sector_begin[4];		/* 32-bit start RBA (zero-based), little endian */
  unsigned8 sector_length[4];		/* 32-bit RBA count (one-based), little endian */
} ppcboot_partition_t;

#if 0
/* PPCbug boot layout.  */
typedef struct ppcboot_hdr {
  unsigned8		pc_compatibility[446];	/* x86 instruction field */
  ppcboot_partition_t	partition[4];		/* partition information */
  unsigned8		signature[2];		/* 0x55 and 0xaa */
  unsigned8		entry_offset[4];	/* entry point offset, little endian */
  unsigned8		length[4];		/* load image length, little endian */
  unsigned8		flags;			/* flag field */
  unsigned8		os_id;			/* OS_ID */
  char			partition_name[32];	/* partition name */
  unsigned8		reserved1[470];		/* reserved */
} ppcboot_hdr_t;
#endif


typedef struct _disklabel {
  device_instance *parent;
  device_instance *raw_disk;
  unsigned_word pos;
  unsigned_word sector_begin;
  unsigned_word sector_length;
} disklabel;


static unsigned_word
sector2uw(unsigned8 s[4])
{
  return ((s[3] << 24)
	  + (s[2] << 16)
	  + (s[1] << 8)
	  + (s[0] << 0));
}


static void
disklabel_delete(device_instance *instance)
{
  disklabel *label = device_instance_data(instance);
  device_instance_delete(label->raw_disk);
  zfree(label);
}


static int
disklabel_read(device_instance *instance,
	       void *buf,
	       unsigned_word len)
{
  disklabel *label = device_instance_data(instance);
  int nr_read;
  if (label->pos + len > label->sector_length)
    len = label->sector_length - label->pos;
  if (device_instance_seek(label->raw_disk, 0,
			   label->sector_begin + label->pos) < 0)
    return -1;
  nr_read = device_instance_read(label->raw_disk, buf, len);
  if (nr_read > 0)
    label->pos += nr_read;
  return nr_read;
}

static int
disklabel_write(device_instance *instance,
		const void *buf,
		unsigned_word len)
{
  disklabel *label = device_instance_data(instance);
  int nr_written;
  if (label->pos + len > label->sector_length)
    len = label->sector_length - label->pos;
  if (device_instance_seek(label->raw_disk, 0,
			   label->sector_begin + label->pos) < 0)
    return -1;
  nr_written = device_instance_write(label->raw_disk, buf, len);
  if (nr_written > 0)
    label->pos += nr_written;
  return nr_written;
}

static int
disklabel_seek(device_instance *instance,
	       unsigned_word pos_hi,
	       unsigned_word pos_lo)
{
  disklabel *label = device_instance_data(instance);
  if (pos_lo >= label->sector_length || pos_hi != 0)
    return -1;
  label->pos = pos_lo;
  return 0;
}


static const device_instance_callbacks package_disklabel_callbacks = {
  disklabel_delete,
  disklabel_read,
  disklabel_write,
  disklabel_seek,
};

/* Reconize different types of boot block */

static int
block0_is_bpb(const unsigned8 block[])
{
  const char ebdic_ibma[] = { 0xc9, 0xc2, 0xd4, 0xc1 };
  /* ref PowerPC Microprocessor CHRP bindings 1.2b - page 47 */
  /* can't start with IBMA */
  if (memcmp(block, ebdic_ibma, sizeof(ebdic_ibma)) == 0)
    return 0;
  /* must have LE 0xAA55 signature at offset 510 */
  if (block[511] != 0xAA && block[510] != 0x55)
    return 0;
  /* valid 16 bit LE bytes per sector - 256, 512, 1024 */
  if (block[11] != 0
      || (block[12] != 1 && block[12] != 2 && block[12] != 4))
    return 0;
  /* nr fats is 1 or 2 */
  if (block[16] != 1 && block[16] != 2)
    return 0;
  return 1;
}


/* Verify that the device contains an ISO-9660 File system */

static int
is_iso9660(device_instance *raw_disk)
{
  /* ref PowerPC Microprocessor CHRP bindings 1.2b - page 47 */
  unsigned8 block[512];
  if (device_instance_seek(raw_disk, 0, 512 * 64) < 0)
    return 0;
  if (device_instance_read(raw_disk, block, sizeof(block)) != sizeof(block))
    return 0;
  if (block[0] == 0x01
      && block[1] == 'C'
      && block[2] == 'D'
      && block[3] == '0'
      && block[4] == '0'
      && block[5] == '1')
    return 1;
  return 0;
}


/* Verify that the disk block contains a valid DOS partition table.
   While we're at it have a look around for active partitions etc.

   Return 0: invalid
   Return 1..4: valid, value returned is the first active partition
   Return -1: no active partition */

static int
block0_is_fdisk(const unsigned8 block[])
{
  const int partition_type_fields[] = { 0, 0x1c2, 0x1d2, 0x1e2, 0x1f2 };
  const int partition_active_fields[] = { 0, 0x1be, 0x1ce, 0x1de, 0xee };
  int partition;
  int active = -1;
  /* ref PowerPC Microprocessor CHRP bindings 1.2b - page 47 */
  /* must have LE 0xAA55 signature at offset 510 */
  if (block[511/*0x1ff*/] != 0xAA && block[510/*0x1fe*/] != 0x55)
    return 0;
  /* must contain valid partition types */
  for (partition = 1; partition <= 4 && active != 0; partition++) {
    int partition_type = block[partition_type_fields[partition]];
    int is_active = block[partition_active_fields[partition]] == 0x80;
    const char *type;
    switch (partition_type) {
    case 0x00:
      type = "UNUSED";
      break;
    case 0x01:
      type = "FAT 12 File system";
      break;
    case 0x04:
      type = "FAT 16 File system";
      break;
    case 0x05:
    case 0x06:
      type = "rejected - extended/chained partition not supported";
      active = 0;
      break;
    case 0x41:
      type = "Single program image";
      break;
    case 0x82:
      type = "Solaris?";
      break;
    case 0x96:
      type = "ISO 9660 File system";
      break;
    default:
      type = "rejected - unknown type";
      active = 0;
      break;
    }
    PTRACE(disklabel, ("partition %d of type 0x%02x - %s%s\n",
		       partition,
		       partition_type,
		       type,
		       is_active && active != 0 ? " (active)" : ""));
    if (partition_type != 0 && is_active && active < 0)
      active = partition;
  }
  return active;
}


/* Verify that block0 corresponds to a MAC disk */

static int
block0_is_mac_disk(const unsigned8 block[])
{
  /* ref PowerPC Microprocessor CHRP bindings 1.2b - page 47 */
  /* signature - BEx4552 at offset 0 */
  if (block[0] != 0x45 || block[1] != 0x52)
    return 0;
  return 1;
}


/* Open a logical disk/file */

device_instance *
pk_disklabel_create_instance(device_instance *raw_disk,
			     const char *args)
{
  int partition;
  char *filename;

  /* parse the arguments */
  if (args == NULL) {
    partition = 0;
    filename = NULL;
  }
  else {
    partition = strtoul((char*)args, &filename, 0);
    if (filename == args)
      partition = -1; /* not specified */
    if (*filename == ',')
      filename++;
    if (*filename == '\0')
      filename = NULL; /* easier */
  }

  if (partition == 0) {
    /* select the raw disk */
    return raw_disk;
  }
  else {
    unsigned8 boot_block[512];
    /* get the boot block for examination */
    if (device_instance_seek(raw_disk, 0, 0) < 0)
      device_error(device_instance_device(raw_disk),
		   "Problem seeking on raw disk");
    if (device_instance_read(raw_disk, &boot_block, sizeof(boot_block))
	!= sizeof(boot_block))
      device_error(device_instance_device(raw_disk), "Problem reading boot block");

    if (partition < 0) {
      /* select the active partition */
      if (block0_is_bpb(boot_block)) {
	device_error(device_instance_device(raw_disk), "Unimplemented active BPB");
      }
      else if (block0_is_fdisk(boot_block)) {
	int active = block0_is_fdisk(boot_block);
	device_error(device_instance_device(raw_disk), "Unimplemented active FDISK (%d)",
		     active);
      }
      else if (is_iso9660(raw_disk)) {
	device_error(device_instance_device(raw_disk), "Unimplemented active ISO9660");
      }
      else if (block0_is_mac_disk(boot_block)) {
	device_error(device_instance_device(raw_disk), "Unimplemented active MAC DISK");
      }
      else {
	device_error(device_instance_device(raw_disk), "Unreconized bootblock");
      }
    }
    else {
      /* select the specified disk partition */
      if (block0_is_bpb(boot_block)) {
	device_error(device_instance_device(raw_disk), "Unimplemented BPB");
      }
      else if (block0_is_fdisk(boot_block)) {
	/* return an instance */
	ppcboot_partition_t *partition_table = (ppcboot_partition_t*) &boot_block[446];
	ppcboot_partition_t *partition_entry;
	disklabel *label;
	if (partition > 4)
	  device_error(device_instance_device(raw_disk),
		       "Only FDISK partitions 1..4 supported");
	partition_entry = &partition_table[partition - 1];
	label = ZALLOC(disklabel);
	label->raw_disk = raw_disk;
	label->pos = 0;
	label->sector_begin = 512 * sector2uw(partition_entry->sector_begin);
	label->sector_length = 512 * sector2uw(partition_entry->sector_length);
	PTRACE(disklabel, ("partition %ld, sector-begin %ld, length %ld\n",
			   (long)partition,
			   (long)label->sector_begin,
			   (long)label->sector_length));
	if (filename != NULL)
	  device_error(device_instance_device(raw_disk),
		       "FDISK file names not yet supported");
	return device_create_instance_from(NULL, raw_disk,
					   label,
					   NULL, args,
					   &package_disklabel_callbacks);
      }
      else if (block0_is_mac_disk(boot_block)) {
	device_error(device_instance_device(raw_disk), "Unimplemented MAC DISK");
      }
      else {
	device_error(device_instance_device(raw_disk),
		     "Unreconized bootblock");
      }
    }
  }
  
  return NULL;
}
  
  
#endif /* _PK_DISKLABEL_C_ */

