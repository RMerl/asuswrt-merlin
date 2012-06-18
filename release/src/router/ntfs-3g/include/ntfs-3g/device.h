/*
 * device.h - Exports for low level device io. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2000-2006 Anton Altaparmakov
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_DEVICE_H
#define _NTFS_DEVICE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "device_io.h"
#include "types.h"
#include "support.h"
#include "volume.h"

/**
 * enum ntfs_device_state_bits -
 *
 * Defined bits for the state field in the ntfs_device structure.
 */
typedef enum {
	ND_Open,	/* 1: Device is open. */
	ND_ReadOnly,	/* 1: Device is read-only. */
	ND_Dirty,	/* 1: Device is dirty, needs sync. */
	ND_Block,	/* 1: Device is a block device. */
} ntfs_device_state_bits;

#define  test_ndev_flag(nd, flag)	   test_bit(ND_##flag, (nd)->d_state)
#define   set_ndev_flag(nd, flag)	    set_bit(ND_##flag, (nd)->d_state)
#define clear_ndev_flag(nd, flag)	  clear_bit(ND_##flag, (nd)->d_state)

#define NDevOpen(nd)		 test_ndev_flag(nd, Open)
#define NDevSetOpen(nd)		  set_ndev_flag(nd, Open)
#define NDevClearOpen(nd)	clear_ndev_flag(nd, Open)

#define NDevReadOnly(nd)	 test_ndev_flag(nd, ReadOnly)
#define NDevSetReadOnly(nd)	  set_ndev_flag(nd, ReadOnly)
#define NDevClearReadOnly(nd)	clear_ndev_flag(nd, ReadOnly)

#define NDevDirty(nd)		 test_ndev_flag(nd, Dirty)
#define NDevSetDirty(nd)	  set_ndev_flag(nd, Dirty)
#define NDevClearDirty(nd)	clear_ndev_flag(nd, Dirty)

#define NDevBlock(nd)		 test_ndev_flag(nd, Block)
#define NDevSetBlock(nd)	  set_ndev_flag(nd, Block)
#define NDevClearBlock(nd)	clear_ndev_flag(nd, Block)

/**
 * struct ntfs_device -
 *
 * The ntfs device structure defining all operations needed to access the low
 * level device underlying the ntfs volume.
 */
struct ntfs_device {
	struct ntfs_device_operations *d_ops;	/* Device operations. */
	unsigned long d_state;			/* State of the device. */
	char *d_name;				/* Name of device. */
	void *d_private;			/* Private data used by the
						   device operations. */
};

struct stat;

/**
 * struct ntfs_device_operations -
 *
 * The ntfs device operations defining all operations that can be performed on
 * the low level device described by an ntfs device structure.
 */
struct ntfs_device_operations {
	int (*open)(struct ntfs_device *dev, int flags);
	int (*close)(struct ntfs_device *dev);
	s64 (*seek)(struct ntfs_device *dev, s64 offset, int whence);
	s64 (*read)(struct ntfs_device *dev, void *buf, s64 count);
	s64 (*write)(struct ntfs_device *dev, const void *buf, s64 count);
	s64 (*pread)(struct ntfs_device *dev, void *buf, s64 count, s64 offset);
	s64 (*pwrite)(struct ntfs_device *dev, const void *buf, s64 count,
			s64 offset);
	int (*sync)(struct ntfs_device *dev);
	int (*stat)(struct ntfs_device *dev, struct stat *buf);
	int (*ioctl)(struct ntfs_device *dev, int request, void *argp);
};

extern struct ntfs_device *ntfs_device_alloc(const char *name, const long state,
		struct ntfs_device_operations *dops, void *priv_data);
extern int ntfs_device_free(struct ntfs_device *dev);

extern s64 ntfs_pread(struct ntfs_device *dev, const s64 pos, s64 count,
		void *b);
extern s64 ntfs_pwrite(struct ntfs_device *dev, const s64 pos, s64 count,
		const void *b);

extern s64 ntfs_mst_pread(struct ntfs_device *dev, const s64 pos, s64 count,
		const u32 bksize, void *b);
extern s64 ntfs_mst_pwrite(struct ntfs_device *dev, const s64 pos, s64 count,
		const u32 bksize, void *b);

extern s64 ntfs_cluster_read(const ntfs_volume *vol, const s64 lcn,
		const s64 count, void *b);
extern s64 ntfs_cluster_write(const ntfs_volume *vol, const s64 lcn,
		const s64 count, const void *b);

extern s64 ntfs_device_size_get(struct ntfs_device *dev, int block_size);
extern s64 ntfs_device_partition_start_sector_get(struct ntfs_device *dev);
extern int ntfs_device_heads_get(struct ntfs_device *dev);
extern int ntfs_device_sectors_per_track_get(struct ntfs_device *dev);
extern int ntfs_device_sector_size_get(struct ntfs_device *dev);
extern int ntfs_device_block_size_set(struct ntfs_device *dev, int block_size);

#endif /* defined _NTFS_DEVICE_H */
