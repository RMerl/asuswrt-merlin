/*
 * PS3 Storage Devices
 *
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * Copyright 2007 Sony Corp.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _ASM_POWERPC_PS3STOR_H_
#define _ASM_POWERPC_PS3STOR_H_

#include <linux/interrupt.h>

#include <asm/ps3.h>


struct ps3_storage_region {
	unsigned int id;
	u64 start;
	u64 size;
};

struct ps3_storage_device {
	struct ps3_system_bus_device sbd;

	struct ps3_dma_region dma_region;
	unsigned int irq;
	u64 blk_size;

	u64 tag;
	u64 lv1_status;
	struct completion done;

	unsigned long bounce_size;
	void *bounce_buf;
	u64 bounce_lpar;
	dma_addr_t bounce_dma;

	unsigned int num_regions;
	unsigned long accessible_regions;
	unsigned int region_idx;		/* first accessible region */
	struct ps3_storage_region regions[0];	/* Must be last */
};

static inline struct ps3_storage_device *to_ps3_storage_device(struct device *dev)
{
	return container_of(dev, struct ps3_storage_device, sbd.core);
}

extern int ps3stor_setup(struct ps3_storage_device *dev,
			 irq_handler_t handler);
extern void ps3stor_teardown(struct ps3_storage_device *dev);
extern u64 ps3stor_read_write_sectors(struct ps3_storage_device *dev, u64 lpar,
				      u64 start_sector, u64 sectors,
				      int write);
extern u64 ps3stor_send_command(struct ps3_storage_device *dev, u64 cmd,
				u64 arg1, u64 arg2, u64 arg3, u64 arg4);

#endif /* _ASM_POWERPC_PS3STOR_H_ */
