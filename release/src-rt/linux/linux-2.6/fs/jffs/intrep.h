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
 * $Id: intrep.h,v 1.14 2001/09/23 23:28:37 dwmw2 Exp $
 *
 */

#ifndef __LINUX_JFFS_INTREP_H__
#define __LINUX_JFFS_INTREP_H__
#include "jffs_fm.h"
struct jffs_node *jffs_alloc_node(void);
void jffs_free_node(struct jffs_node *n);
int jffs_get_node_inuse(void);

__u32 jffs_checksum(const void *data, int size);
__u16 jffs_checksum_16(const void *data, int size);
unsigned long crc_32(const void *src, unsigned len, unsigned long crc32);

void jffs_cleanup_control(struct jffs_control *c);
int jffs_build_fs(struct super_block *sb);

int jffs_insert_node(struct jffs_control *c, struct jffs_file *f,
		     const struct jffs_raw_inode *raw_inode,
		     const char *name, struct jffs_node *node);
struct jffs_file *jffs_find_file(struct jffs_control *c, __u32 ino);
struct jffs_file *jffs_find_child(struct jffs_file *dir, const char *name, int len);

void jffs_free_node(struct jffs_node *node);

int jffs_foreach_file(struct jffs_control *c, int (*func)(struct jffs_file *));
int jffs_possibly_delete_file(struct jffs_file *f);
int jffs_insert_file_into_tree(struct jffs_file *f);
int jffs_unlink_file_from_tree(struct jffs_file *f);
int jffs_file_count(struct jffs_file *f);
long jffs_get_file_count(void);

int jffs_write_node(struct jffs_control *c, struct jffs_node *node,
		    struct jffs_raw_inode *raw_inode,
		    const char *name, const unsigned char *buf,
		    int recoverable, struct jffs_file *f);
int jffs_read_data(struct jffs_file *f, unsigned char *buf, __u32 read_offset, __u32 size);

/* Garbage collection stuff.  */
int jffs_garbage_collect_thread(void *ptr);
void jffs_garbage_collect_trigger(struct jffs_control *c);
int jffs_garbage_collect_now(struct jffs_control *c, int force, int merge_obn);

/* For debugging purposes.  */
#if CONFIG_JFFS_FS_VERBOSE > 0
void jffs_print_raw_inode(struct jffs_raw_inode *raw_inode);
#endif
#if 0
int jffs_print_file(struct jffs_file *f);
#endif
void jffs_print_hash_table(struct jffs_control *c);
void jffs_print_tree(struct jffs_file *first_file, int indent);

int flash_safe_write(struct jffs_fmcontrol *fmc, loff_t to,
		     const u_char *buf, size_t count);

int jffs_write_dummy_node(struct jffs_control *c, struct jffs_fm *dirty_fm);
int flash_erase_region(struct mtd_info *mtd, loff_t start,
		       size_t size);

unsigned long free_in_tail_block(struct jffs_control *c);
unsigned long head_contig_dirty_size(struct jffs_fmcontrol *fmc);

#endif /* __LINUX_JFFS_INTREP_H__  */
