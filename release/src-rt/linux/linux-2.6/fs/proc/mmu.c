/* mmu.c: mmu memory info files
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <asm/pgtable.h>
#include "internal.h"

void get_vmalloc_info(struct vmalloc_info *vmi)
{
	struct vm_struct *vma;
	unsigned long free_area_size;
	unsigned long prev_end;

	vmi->used = 0;

	if (!vmlist) {
		vmi->largest_chunk = VMALLOC_TOTAL;
	}
	else {
		vmi->largest_chunk = 0;

		prev_end = VMALLOC_START;

		read_lock(&vmlist_lock);

		for (vma = vmlist; vma; vma = vma->next) {
			unsigned long addr = (unsigned long) vma->addr;

			/*
			 * Some archs keep another range for modules in vmlist
			 */
			if (addr < VMALLOC_START)
				continue;
			if (addr >= VMALLOC_END)
				break;

			vmi->used += vma->size;

			free_area_size = addr - prev_end;
			if (vmi->largest_chunk < free_area_size)
				vmi->largest_chunk = free_area_size;

			prev_end = vma->size + addr;
		}

		if (VMALLOC_END - prev_end > vmi->largest_chunk)
			vmi->largest_chunk = VMALLOC_END - prev_end;

		read_unlock(&vmlist_lock);
	}
}
