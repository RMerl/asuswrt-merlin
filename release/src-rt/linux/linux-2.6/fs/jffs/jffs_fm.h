
/*
 * JFFS -- Journaling Flash File System, Linux implementation.
 *
 * Copyright (C) 1999, 2000  Axis Communications AB.
 *
 * Created by Finn Hakansson <finn@axis.com>.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Id: jffs_fm.h,v 1.13 2001/01/11 12:03:25 dwmw2 Exp $
 *
 * Ported to Linux 2.3.x and MTD:
 * Copyright (C) 2000  Alexander Larsson (alex@cendio.se), Cendio Systems AB
 *
 */

#ifndef __LINUX_JFFS_FM_H__
#define __LINUX_JFFS_FM_H__

#include <linux/types.h>
#include <linux/jffs.h>
#include <linux/mtd/mtd.h>
#include <linux/mutex.h>

/* The alignment between two nodes in the flash memory.  */
#define JFFS_ALIGN_SIZE 4

/* TESTING:# 64KB blocks to use instead of flash.
* 0 = use flash.
* NZ = use RAM.    1 means we'll need to do inplace GC.
*/
#define JFFS_RAM_BLOCKS 0

#define CHK_INIT (~0)


/* When rewriting nodes, any space that cannot hold at
 * least this many data bytes will be discarded as too
 * small to be useful.
 * When writing a *data* node, ditto.
 */
#define MIN_USEFUL_DATA_SIZE 256


/***********************************************************/

#ifndef CONFIG_JFFS_FS_VERBOSE
#define CONFIG_JFFS_FS_VERBOSE 1
#endif

#if CONFIG_JFFS_FS_VERBOSE > 0
#define D(x) x
#define D1(x) D(x)
#define D1printk(args...) printk(args)
#else
#define D(x)
#define D1(x)
#define D1printk(args...)
#endif

#if CONFIG_JFFS_FS_VERBOSE > 1
#define D2(x) D(x)
#define D2printk(args...) printk(args)
#else
#define D2(x)
#define D2printk(args...)
#endif

#if CONFIG_JFFS_FS_VERBOSE > 2
#define D3(x) D(x)
#define D3printk(args...) printk(args)
#else
#define D3(x)
#define D3printk(args...)
#endif

#define xD1(x) x	/* For temp.  Turn on printk */
#define xD2(x) x
#define xD3(x) x


#define ASSERT(x) x

/* How many padding bytes should be inserted between two chunks of data
   on the flash?  */
#define JFFS_GET_PAD_BYTES(size) ( (JFFS_ALIGN_SIZE-1) & -(__u32)(size) )
#define JFFS_PAD(size) ( (size + (JFFS_ALIGN_SIZE-1)) & ~(JFFS_ALIGN_SIZE-1) )



struct jffs_node_ref
{
	struct jffs_node *node;
	struct jffs_node_ref *next;
};


/* The struct jffs_fm represents a chunk of data in the flash memory.  */
struct jffs_fm
{
	__u32 offset;
	__u32 size;
	struct jffs_fm *prev;
	struct jffs_fm *next;
	struct jffs_node_ref *nodes; /* USED if != 0.  */
};

struct jffs_fmcontrol
{
   __u32 flash_size;
   __u32 used_size;
   __u32 dirty_size;
   __u32 free_size;
   __u32 sector_size;
   __u32 min_free_size;  /* The reserved free space size.  Needed to be able
			  * to perform garbage collections.
			  * 0 => do risky in-place rewrite. */
   __u32 max_chunk_size; /* The maximum size of a chunk of data.  */
   __u32 first_fm;	/* Fm_offset to use for very first fmalloc. */
   unsigned long max_used_size;
   unsigned long max_usewst_size;
   unsigned long low_free_size;
   unsigned long low_frewst_size;
   long wasted_size;	/* Regions of nodes that have been copied
			 * elsewhere and are therefore redundant and
			 * wasted.  Happens when a node gets split.
			 * Corrected when GC hits the node. */
   char *aux_block;	/* RAM block to use for gc_inplace. */
   int do_inplace_gc;	
   struct mtd_info *mtd;
   struct jffs_control *c;
   struct jffs_fm *head;
   struct jffs_fm *tail;
   struct jffs_fm *head_extra;
   struct jffs_fm *tail_extra;
   struct mutex biglock;
};

/* Notice the two members head_extra and tail_extra in the jffs_control
   structure above. Those are only used during the scanning of the flash
   memory; while the file system is being built. If the data in the flash
   memory is organized like

      +----------------+------------------+----------------+
      |  USED / DIRTY  |       FREE       |  USED / DIRTY  |
      +----------------+------------------+----------------+

   then the scan is split in two parts. The first scanned part of the
   flash memory is organized through the members head and tail. The
   second scanned part is organized with head_extra and tail_extra. When
   the scan is completed, the two lists are merged together. The jffs_fm
   struct that head_extra references is the logical beginning of the
   flash memory so it will be referenced by the head member.  */



struct jffs_fmcontrol *jffs_build_begin(struct jffs_control *c, int unit);
void jffs_build_end(struct jffs_fmcontrol *fmc);
void jffs_cleanup_fmcontrol(struct jffs_fmcontrol *fmc);

int jffs_fmalloc(struct jffs_fmcontrol *fmc, __u32 size,
		 struct jffs_node *node, struct jffs_fm **result);
int jffs_fmfree(struct jffs_fmcontrol *fmc, struct jffs_fm *fm,
		struct jffs_node *node);

__u32 jffs_free_size1(struct jffs_fmcontrol *fmc);
__u32 jffs_free_size2(struct jffs_fmcontrol *fmc);
void jffs_sync_erase(struct jffs_fmcontrol *fmc, int erased_size);
struct jffs_fm *jffs_cut_node(struct jffs_fmcontrol *fmc, __u32 size);
struct jffs_node *jffs_get_oldest_node(struct jffs_fmcontrol *fmc);
long jffs_erasable_size(struct jffs_fmcontrol *fmc);
struct jffs_fm *jffs_fmalloced(struct jffs_fmcontrol *fmc, __u32 offset,
			       __u32 size, struct jffs_node *node);
int jffs_add_node(struct jffs_node *node);
void jffs_fmfree_partly(struct jffs_fmcontrol *fmc, struct jffs_fm *fm,
			__u32 size);

#if CONFIG_JFFS_FS_VERBOSE > 0
void jffs_print_fmcontrol(struct jffs_fmcontrol *fmc);
#endif
#if CONFIG_JFFS_FS_VERBOSE > 2
void jffs_print_fm(struct jffs_fm *fm);
#endif
#if 0
void jffs_print_node_ref(struct jffs_node_ref *ref);
#endif

unsigned long used_in_head_block(struct jffs_control *c);
int gc_inplace_begin(struct jffs_fmcontrol *fmc);
void gc_inplace_end(struct jffs_fmcontrol *fmc, int erased);
int flash_memset(struct jffs_fmcontrol *fmc, loff_t to,
		 const u_char c, size_t size);

#endif /* __LINUX_JFFS_FM_H__  */
