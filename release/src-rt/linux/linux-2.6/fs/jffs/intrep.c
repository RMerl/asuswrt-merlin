/*
 * JFFS -- Journaling Flash File System, Linux implementation.
 *
 * Copyright (C) 1999, 2000  Axis Communications, Inc.
 *
 * Created by Finn Hakansson <finn@axis.com>.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Id: intrep.c,v 1.102 2001/09/23 23:28:36 dwmw2 Exp $
 *
 * Ported to Linux 2.3.x and MTD:
 * Copyright (C) 2000  Alexander Larsson (alex@cendio.se), Cendio Systems AB
 *
 */
/* This file contains the code for the internal structure of the
   Journaling Flash File System, JFFS.  */

/*
 * Todo list:
 *
 * memcpy_to_flash() and memcpy_from_flash() functions.
 *
 * Implementation of hard links.
 *
 * Organize the source code in a better way. Against the VFS we could
 * have jffs_ext.c, and against the block device jffs_int.c.
 * A better file-internal organization too.
 *
 * Consider endianness stuff. ntohl() etc.
 *
 * Are we handling the atime, mtime, ctime members of the inode right?
 *
 * Remove some duplicated code. Take a look at jffs_write_node() and
 * jffs_rewrite_data() for instance.
 *
 * Implement more meaning of the nlink member in various data structures.
 * nlink could be used in conjunction with hard links for instance.
 *
 * Better memory management. Allocate data structures in larger chunks
 * if possible.
 *
 * If too much meta data is stored, a garbage collect should be issued.
 * We have experienced problems with too much meta data with for instance
 * log files.
 *
 * Improve the calls to jffs_ioctl(). We would like to retrieve more
 * information to be able to debug (or to supervise) JFFS during run-time.
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/jffs.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/pagemap.h>
#include <linux/mutex.h>
#include <asm/byteorder.h>
#include <linux/smp_lock.h>
#include <linux/time.h>
#include <linux/ctype.h>
#include <linux/freezer.h>

#include "intrep.h"
#include "jffs_fm.h"

void sleep(int n)
{
   set_current_state(TASK_INTERRUPTIBLE);
   schedule_timeout(n * HZ);
}


long no_jffs_node = 0;
static long no_jffs_file = 0;
#if defined(JFFS_MEMORY_DEBUG) && JFFS_MEMORY_DEBUG
long no_jffs_control = 0;
long no_jffs_raw_inode = 0;
long no_jffs_node_ref = 0;
long no_jffs_fm = 0;
long no_jffs_fmcontrol = 0;
long no_hash = 0;
long no_name = 0;
#endif

#include <linux/vmalloc.h>
#if JFFS_RAM_BLOCKS > 0
unsigned char *jffs_ram = 0;
#endif

static int jffs_scan_flash(struct jffs_control *c);
static int jffs_update_file(struct jffs_file *f, struct jffs_node *node);
unsigned long used_in_head_block(struct jffs_control *c);
unsigned long free_in_tail_block(struct jffs_control *c);
static int jffs_build_file(struct jffs_file *f);
static int jffs_free_file(struct jffs_file *f);
static int jffs_free_node_list(struct jffs_file *f);
static int jffs_insert_file_into_hash(struct jffs_file *f);
static int jffs_remove_redundant_nodes(struct jffs_file *f);
int jffs_print_file_nodes(struct jffs_file *f);

#if CONFIG_JFFS_FS_VERBOSE > 0
static __u8
flash_read_u8(struct mtd_info *mtd, loff_t from)
{
	size_t retlen;
	__u8 ret;
	int res;

	res = mtd->read(mtd, from, 1, &retlen, &ret);
	if (retlen != 1) {
		printk("Didn't read a byte in flash_read_u8(). Returned %d\n", res);
		return 0;
	}

	return ret;
}

static void
jffs_hexdump(struct mtd_info *mtd, loff_t pos, int size)
{
	char line[16];
	int j = 0;

	while (size > 0) {
		int i;

		printk("%ld:", (long) pos);
		for (j = 0; j < 16; j++) {
			line[j] = flash_read_u8(mtd, pos++);
		}
		for (i = 0; i < j; i++) {
			if (!(i & 1)) {
				printk(" %.2x", line[i] & 0xff);
			}
			else {
				printk("%.2x", line[i] & 0xff);
			}
		}

		/* Print empty space */
		for (; i < 16; i++) {
			if (!(i & 1)) {
				printk("   ");
			}
			else {
				printk("  ");
			}
		}
		printk("  ");

		for (i = 0; i < j; i++) {
			if (isgraph(line[i])) {
				printk("%c", line[i]);
			}
			else {
				printk(".");
			}
		}
		printk("\n");
		size -= 16;
	}
}

/* Print the contents of a node.  */
static void
jffs_print_node(struct jffs_node *n)
{
	D(printk("jffs_node: 0x%p\n", n));
	D(printk("{\n"));
	D(printk("        0x%08x, /* version  */\n", n->version));
	D(printk("        0x%08x, /* data_offset  */\n", n->data_offset));
	D(printk("        0x%08x, /* data_size  */\n", n->data_size));
	D(printk("        0x%08x, /* removed_size  */\n", n->removed_size ));
	D(printk("        0x%08x, /* fm_offset  */\n", n->fm_offset));
	D(printk("        0x%02x,       /* name_size  */\n", n->name_size));
	D(printk("        0x%p, /* fm,  fm->offset: %u  */\n",
		 n->fm, (n->fm ? n->fm->offset : 0)));
	D(printk("        0x%p, /* version_prev  */\n", n->version_prev));
	D(printk("        0x%p, /* version_next  */\n", n->version_next));
	D(printk("        0x%p, /* range_prev  */\n", n->range_prev));
	D(printk("        0x%p, /* range_next  */\n", n->range_next));
	D(printk("}\n"));
}

#endif

#if CONFIG_JFFS_FS_VERBOSE > 0
/* Print the contents of a raw inode.  */
void jffs_print_raw_inode(struct jffs_raw_inode *raw_inode)
{
	D(printk("jffs_raw_inode: inode number: %u\n", raw_inode->ino));
	D(printk("{\n"));
	D(printk("        0x%08x, /* magic  */\n", raw_inode->magic));
	D(printk("        0x%08x, /* ino  */\n", raw_inode->ino));
	D(printk("        0x%08x, /* pino  */\n", raw_inode->pino));
	D(printk("        0x%08x, /* version  */\n", raw_inode->version));
	D(printk("        0x%08x, /* mode  */\n", raw_inode->mode));
	D(printk("        0x%04x,     /* uid  */\n", raw_inode->uid));
	D(printk("        0x%04x,     /* gid  */\n", raw_inode->gid));
	D(printk("        0x%08x, /* atime  */\n", raw_inode->atime));
	D(printk("        0x%08x, /* mtime  */\n", raw_inode->mtime));
	D(printk("        0x%08x, /* ctime  */\n", raw_inode->ctime));
	D(printk("        0x%08x, /* offset  */\n", raw_inode->offset));
	D(printk("        0x%08x, /* dsize  */\n", raw_inode->dsize));
	D(printk("        0x%08x, /* rsize  */\n", raw_inode->rsize));
	D(printk("        0x%02x,       /* nsize  */\n", raw_inode->nsize));
	D(printk("        0x%02x,       /* nlink  */\n", raw_inode->nlink));
	D(printk("        0x%02x,       /* spare  */\n",
		 raw_inode->spare));
	D(printk("        %u,          /* rename  */\n",
		 raw_inode->rename));
	D(printk("        %u,          /* deleted  */\n",
		 raw_inode->deleted));
	D(printk("        0x%02x,       /* accurate  */\n",
		 raw_inode->accurate));
	D(printk("        0x%08x, /* dchksum  */\n", raw_inode->dchksum));
	D(printk("        0x%04x,     /* nchksum  */\n", raw_inode->nchksum));
	D(printk("        0x%04x,     /* ichksum  */\n", raw_inode->ichksum));
	D(printk("}\n"));
}
#endif


#if JFFS_RAM_BLOCKS > 0
/* Using RAM instead of flash. */

static int
flash_safe_read(struct mtd_info *mtd, loff_t from,
		u_char *buf, size_t count)
{
	if (from < 0 || from + count > JFFS_RAM_BLOCKS * 64 * 1024) {
		printk("Tried to read out of ram-flash!");
		return (-1);
	}
	memcpy(buf, jffs_ram + from, count);
	return (count);
}


static __u32
flash_read_u32(struct mtd_info *mtd, loff_t from)
{
	__u32 ret;

	flash_safe_read(mtd, from, (unsigned char *)&ret, 4);
	return (ret);
}


int
flash_safe_write(struct jffs_fmcontrol *fmc, loff_t to,
		 const u_char *buf, size_t count)
{
	struct mtd_info *mtd = fmc->mtd;
	int i;
   
#if 0
	printk("flash_safe_write(%p, %08x - %08x, %p, %08x   @: %p)\n",
		mtd, (unsigned int)to, (unsigned int)to + count-1, buf,
		count, jffs_ram + to);
#endif
	if (count > mtd->erasesize - ((unsigned)to % mtd->erasesize))
		printk("Write spans block: %5x - %5x  siz: %5x\n", (unsigned)to,
			(unsigned)to+count,(unsigned)count);
	if (to < 0 || to + count > fmc->flash_size) {
		printk("!!!! Tried to write out of ram-flash. to: %x  count %x\n",
			(unsigned)to, count);
		sleep(4);
		return (-1);
	}
	if (fmc->do_inplace_gc && to >= fmc->sector_size) {
		//printk("   High write. %5x  %4x\n", (unsigned)to, count);
		if (to < 0 || to + count > 2 * fmc->sector_size || fmc->aux_block == NULL ) {
			printk("!!!! Tried to write outsize of aux buffer. to: %x  count %x\n",
				(unsigned)to, count);
			sleep(4);
			return (-1);
		}
		memcpy(fmc->aux_block + (to - fmc->sector_size), buf, count);
		return (count);
	}

	for (i = 0; i < count; ++i) {    /* Verify the area is all F's. */
		if (jffs_ram[to + i] != 0xff) {
			printk("Write to non-erased flash @ %x  \n", (unsigned int)to + i);
			break;
		}
	}
	memcpy(jffs_ram + to, buf, count);
	return (count);
}


int
flash_erase_region(struct mtd_info *mtd, loff_t start, size_t size)
{
	//printk("erase region [0x%lx, 0x%lx]\n", (long)start, (long)size);
	if (start < 0 || start + size > JFFS_RAM_BLOCKS * 64 * 1024) {
		printk("!!!! Tried to write out of ram-flash. to: %x  count %x\n",
			(unsigned)start, size);
		sleep(4);
		return (-1);
	}
	memset(jffs_ram + start, JFFS_EMPTY_BITMASK, size);

	/* Sleep for a little while. */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(HZ/20);
	return (0);
}


#else
static int
flash_safe_read(struct mtd_info *mtd, loff_t from,
		u_char *buf, size_t count)
{
	size_t retlen;
	int res;

	D3(printk(KERN_NOTICE "flash_safe_read(%p, %08x, %p, %08x)\n",
		mtd, (unsigned int) from, buf, count));

	res = mtd->read(mtd, from, count, &retlen, buf);
	if (retlen != count) {
		panic("Didn't read all bytes in flash_safe_read(). Returned %d\n", res);
	}
	return res?res:retlen;
}

int
flash_safe_write(struct jffs_fmcontrol *fmc, loff_t to,
		 const u_char *buf, size_t count)
{
   size_t retlen;
   int res;
   struct mtd_info *mtd = fmc->mtd;

   D3(printk(KERN_NOTICE "flash_safe_write(%p, %08x - %08x, %p, %08x)\n",
	     mtd, (unsigned int) to, (unsigned int) to + count-1, buf, count));
   if (count > mtd->erasesize - ((unsigned)to % mtd->erasesize))
       printk("Write spans block: %5x - %5x  siz: %5x\n", (unsigned)to,
	      (unsigned)to+count,(unsigned)count);
   if (to < 0 || to + count > fmc->flash_size) {
      printk("!!!! Tried to write out of ram-flash. to: %x  count %x\n",
	     (unsigned)to, count);
      sleep(4);
      return (-1);
   }
   if (fmc->do_inplace_gc && to >= fmc->sector_size) {
      printk("   High write. %5x  %4x\n", (unsigned)to, count);
      if (to < 0 || to + count > 2 * fmc->sector_size || fmc->aux_block == NULL ) {
	 printk("!!!! Tried to write outsize of aux buffer. to: %x  count %x\n",
		(unsigned)to, count);
	 sleep(4);
	 return (-1);
      }
      memcpy(fmc->aux_block + (to - fmc->sector_size), buf, count);
      return (count);
   }

   res = mtd->write(mtd, to, count, &retlen, buf);
   if (retlen != count) {
      printk("Didn't write all bytes in flash_safe_write(). Returned %d\n", res);
   }
   return res?res:retlen;
}

static __u32 flash_read_u32(struct mtd_info *mtd, loff_t from)
{
	size_t retlen;
	__u32 ret;
	int res;

	res = mtd->read(mtd, from, 4, &retlen, (unsigned char *)&ret);
	if (retlen != 4) {
		printk("Didn't read all bytes in flash_read_u32(). Returned %d\n", res);
		return 0;
	}

	return ret;
}


static void
intrep_erase_callback(struct erase_info *done)
{
	wait_queue_head_t *wait_q;

	wait_q = (wait_queue_head_t *)done->priv;

	wake_up(wait_q);
}




int flash_erase_region(struct mtd_info *mtd, loff_t start,
		   size_t size)
{
	struct erase_info *erase;
	DECLARE_WAITQUEUE(wait, current);
	wait_queue_head_t wait_q;

	erase = kmalloc(sizeof(struct erase_info), GFP_KERNEL);
	if (!erase)
		return -ENOMEM;

	init_waitqueue_head(&wait_q);

	erase->mtd = mtd;
	erase->callback = intrep_erase_callback;
	erase->addr = start;
	erase->len = size;
	erase->priv = (u_long)&wait_q;

	/* FIXME: Use TASK_INTERRUPTIBLE and deal with being interrupted */
	set_current_state(TASK_UNINTERRUPTIBLE);
	add_wait_queue(&wait_q, &wait);

	if (mtd->erase(mtd, erase) < 0) {
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&wait_q, &wait);
		kfree(erase);

		printk(KERN_WARNING "flash: erase of region [0x%lx, 0x%lx] "
		       "totally failed\n", (long)start, (long)start + size);

		return -1;
	}

	schedule(); /* Wait for flash to finish. */
	remove_wait_queue(&wait_q, &wait);

	kfree(erase);

	return 0;
}
#endif

/* This routine calculates checksums in JFFS.  */
/* Tool function to calculate a CRC32 across some buffer */
/* third argument is either 0xFFFFFFFF to start or value from last piece */
unsigned long
crc_32(const void *src, unsigned len, unsigned long crc32)
{
   const unsigned char *buf = (const unsigned char *)src;

   /* CCITT standard polynomial 0x04C11DB7 */
   static const unsigned crc32_lookup[16] =
      {   /* lookup table for 4 bits at a time is affordable */
	 0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
	 0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
	 0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61,
	 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD
      };

   unsigned char byte;
   unsigned t;

   while (len--) {
      byte = *buf++; /* get one byte of data */

      /* upper nibble of our data */
      t = crc32 >> 28; /* extract the 4 most significant bits */
      t ^= byte >> 4; /* XOR in 4 bits of data into the extracted bits */
      crc32 <<= 4; /* shift the CRC register left 4 bits */
      crc32 ^= crc32_lookup[t]; /* do the table lookup and XOR the result */

      /* lower nibble of our data */
      t = crc32 >> 28; /* extract the 4 most significant bits */
      t ^= byte & 0x0F; /* XOR in 4 bits of data into the extracted bits */
      crc32 <<= 4; /* shift the CRC register left 4 bits */
      crc32 ^= crc32_lookup[t]; /* do the table lookup and XOR the result */
   }
   return crc32;
}

__u32 jffs_checksum(const void *data, int size)
{
	return(crc_32(data, size, CHK_INIT));
}


/* Used for inode & name checksums.
 * Don't allow all F's, because we won't be able to
 * tell the checksum from an erased flash that wasn't written. */
__u16 jffs_checksum_16(const void *data, int size)
{
   __u16 cksum = jffs_checksum(data, size);
   if (cksum == 0xffff)
      cksum = 0;
   return (cksum);
}


int jffs_checksum_flash(struct mtd_info *mtd, loff_t start, int size, __u32 *result)
{
	__u32 sum = CHK_INIT;
	loff_t ptr = start;
	__u8 *read_buf;
	int length;

	/* Allocate read buffer */
	read_buf = kmalloc(sizeof(__u8) * 4096, GFP_KERNEL);
	if (!read_buf) {
		printk(KERN_NOTICE "kmalloc failed in jffs_checksum_flash()\n");
		return -ENOMEM;
	}
	/* Loop until checksum done */
	while (size) {
		/* Get amount of data to read */
		if (size < 4096)
			length = size;
		else
			length = 4096;

		/* Perform flash read */
		D3(printk(KERN_NOTICE "jffs_checksum_flash\n"));
		flash_safe_read(mtd, ptr, &read_buf[0], length);

		/* Compute checksum */
		sum = crc_32(read_buf, length, sum);

		/* Update pointer and size */
		size -= length;
		ptr += length;
	}

	/* Free read buffer */
	kfree(read_buf);

	/* Return result */
	D3(printk("checksum result: 0x%08x\n", sum));
	*result = sum;
	return 0;
}


/* Create and initialize a new struct jffs_file.  */
static struct jffs_file *
jffs_create_file(struct jffs_control *c,
		 const struct jffs_raw_inode *raw_inode)
{
	struct jffs_file *f;

	if (!(f = kzalloc(sizeof(*f), GFP_KERNEL))) {
		D(printk("jffs_create_file: Failed!\n"));
		return NULL;
	}
	no_jffs_file++;
	f->ino = raw_inode->ino;
	f->pino = raw_inode->pino;
	f->nlink = raw_inode->nlink;
	f->deleted = raw_inode->deleted;
	f->c = c;

	return f;
}


/* Build a control block for the file system.  */
static struct jffs_control *
jffs_create_control(struct super_block *sb)
{
	struct jffs_control *c;
	int i;
	D(char *t = 0);

	D2(printk("jffs_create_control()\n"));

	if (!(c = kmalloc(sizeof(*c), GFP_KERNEL))) {
		goto fail_control;
	}
	DJM(no_jffs_control++);
	c->root = NULL;
	c->gc_task = NULL;
	c->hash_len = JFFS_HASH_SIZE;
	if (!(c->hash = kmalloc(sizeof(struct list_head) * c->hash_len, GFP_KERNEL))) {
		goto fail_hash;
	}
	DJM(no_hash++);
	for (i = 0; i < c->hash_len; i++)
		INIT_LIST_HEAD(&c->hash[i]);
	if (!(c->fmc = jffs_build_begin(c, MINOR(sb->s_dev)))) {
		goto fail_fminit;
	}
	c->next_ino = JFFS_MIN_INO + 1;
	c->delete_list = (struct jffs_delete_list *) 0;
#if JFFS_RAM_BLOCKS > 0
	jffs_ram = vmalloc(JFFS_RAM_BLOCKS * 64 * 1024);
	printk("jffs_ram at 0x%p\n", jffs_ram);
	if (jffs_ram == NULL)
	   goto fail_fminit;

	flash_erase_region(0, 0, JFFS_RAM_BLOCKS * 64 * 1024); 
#endif
	return c;

fail_fminit:
	D(t = "c->fmc");
fail_hash:
	kfree(c);
	DJM(no_jffs_control--);
	D(t = t ? t : "c->hash");
fail_control:
	D(t = t ? t : "control");
	D(printk("jffs_create_control: Allocation failed: (%s)\n", t));
	return (NULL);
}


/* Clean up all data structures associated with the file system.  */
void jffs_cleanup_control(struct jffs_control *c)
{
	D2(printk("jffs_cleanup_control()\n"));
#if JFFS_RAM_BLOCKS > 0
	if (jffs_ram)
	   vfree(jffs_ram);
	jffs_ram = NULL;
#endif

	if (!c)
		return;

	while (c->delete_list) {
		struct jffs_delete_list *delete_list_element;
		delete_list_element = c->delete_list;
		c->delete_list = c->delete_list->next;
		kfree(delete_list_element);
	}

	/* Free all files and nodes.  */
	if (c->hash) {
		jffs_foreach_file(c, jffs_free_node_list);
		jffs_foreach_file(c, jffs_free_file);
		kfree(c->hash);
		DJM(no_hash--);
	}
	jffs_cleanup_fmcontrol(c->fmc);
	kfree(c);
	DJM(no_jffs_control--);
	D3(printk("jffs_cleanup_control: Leaving...\n"));
}


/* This function adds a virtual root node to the in-RAM representation.
   Called by jffs_build_fs().  */
static int jffs_add_virtual_root(struct jffs_control *c)
{
	struct jffs_file *root;
	struct jffs_node *node;

	D2(printk("jffs_add_virtual_root: "
		  "Creating a virtual root directory.\n"));

	if (!(root = kzalloc(sizeof(struct jffs_file), GFP_KERNEL))) {
		return -ENOMEM;
	}
	no_jffs_file++;
	if (!(node = jffs_alloc_node())) {
		kfree(root);
		no_jffs_file--;
		return -ENOMEM;
	}
	DJM(no_jffs_node++);
	memset(node, 0, sizeof(struct jffs_node));
	node->ino = JFFS_MIN_INO;
	root->ino = JFFS_MIN_INO;
	root->mode = S_IFDIR | S_IRWXU | S_IRGRP
		     | S_IXGRP | S_IROTH | S_IXOTH;
	root->atime = root->mtime = root->ctime = get_seconds();
	root->nlink = 1;
	root->c = c;
	root->version_head = root->version_tail = node;
	jffs_insert_file_into_hash(root);
	return 0;
}


/* This is where the file system is built and initialized.  */
int
jffs_build_fs(struct super_block *sb)
{
	struct jffs_control *c;
	int err = 0;

	D2(printk("jffs_build_fs()\n"));
	if (!(c = jffs_create_control(sb))) {
		return -ENOMEM;
	}
	c->building_fs = 1;
	c->sb = sb;
	/* Check for write-in-place. */
	if ((c->fmc->flash_size / c->fmc->sector_size) == 1) {
	   if ((sb->s_flags & (MS_MANDLOCK)) == (MS_MANDLOCK)) {
	      printk(KERN_WARNING "JFFS: Doing write-in-place.  Danger of corruption if power fails during a write.\n");
	      c->fmc->do_inplace_gc = 1;
	      c->fmc->min_free_size = 0;
	   }
	   else {
	      printk(KERN_WARNING "JFFS: Only 1 block in jffs.  Jffs too small to be usable without write-in-place.\n");
	      printk(KERN_WARNING "JFFS: Must mount with MS_MANDLOCK or o_mand to enable write-in-place.\n");
	   }
	}
	
 	sb->s_flags |= MS_NOATIME;

	if ((err = jffs_scan_flash(c)) < 0) {
		if(err == -EAGAIN){
			/* scan_flash() wants us to try once more. A flipping 
			   bits sector was detect in the middle of the scan flash.
			   Clean up old allocated memory before going in.
			*/
			D1(printk("jffs_build_fs: Cleaning up all control structures,"
				  " reallocating them and trying mount again.\n"));
			jffs_cleanup_control(c);
			if (!(c = jffs_create_control(sb))) {
				return -ENOMEM;
			}
			c->building_fs = 1;
			c->sb = sb;

			if ((err = jffs_scan_flash(c)) < 0) {
				goto jffs_build_fs_fail;
			}			
		}else{
			goto jffs_build_fs_fail;
		}
	}

	/* Add a virtual root node if no one exists.  */
	if (!jffs_find_file(c, JFFS_MIN_INO)) {
		if ((err = jffs_add_virtual_root(c)) < 0) {
			goto jffs_build_fs_fail;
		}
	}

	while (c->delete_list) {
		struct jffs_file *f;
		struct jffs_delete_list *delete_list_element;

		if ((f = jffs_find_file(c, c->delete_list->ino))) {
			f->deleted = 1;
		}
		delete_list_element = c->delete_list;
		c->delete_list = c->delete_list->next;
		kfree(delete_list_element);
	}

	/* Remove deleted nodes.  */
	if ((err = jffs_foreach_file(c, jffs_possibly_delete_file)) < 0) {
		printk(KERN_ERR "JFFS: Failed to remove deleted nodes.\n");
		goto jffs_build_fs_fail;
	}
	/* Remove redundant nodes.  (We are not interested in the
	   return value in this case.)  */
	jffs_foreach_file(c, jffs_remove_redundant_nodes);
	/* Try to build a tree from all the nodes.  */
	if ((err = jffs_foreach_file(c, jffs_insert_file_into_tree)) < 0) {
		printk("JFFS: Failed to build tree.\n");
		goto jffs_build_fs_fail;
	}
	/* Compute the sizes of all files in the filesystem.  Adjust if
	   necessary.  */
	if ((err = jffs_foreach_file(c, jffs_build_file)) < 0) {
		printk("JFFS: Failed to build file system.\n");
		goto jffs_build_fs_fail;
	}
	sb->s_fs_info = (void *)c;
	c->building_fs = 0;
	return 0;

jffs_build_fs_fail:
	jffs_cleanup_control(c);
	sb->s_fs_info = NULL;
	return err;
} /* jffs_build_fs()  */


/*
  This checks for sectors that were being erased in their previous 
  lifetimes and for some reason or the other (power fail etc.), 
  the erase cycles never completed.
  As the flash array would have reverted back to read status, 
  these sectors are detected by the symptom of the "flipping bits",
  i.e. bits being read back differently from the same location in
  flash if read multiple times.
  The only solution to this is to re-erase the entire
  sector.
  Unfortunately detecting "flipping bits" is not a simple exercise
  as a bit may be read back at 1 or 0 depending on the alignment 
  of the stars in the universe.
  The level of confidence is in direct proportion to the number of 
  scans done. By power fail testing I (Vipin) have been able to 
  proove that reading twice is not enough.
  Maybe 4 times? Change NUM_REREADS to a higher number if you want
  a (even) higher degree of confidence in your mount process. 
  A higher number would of course slow down your mount.
*/
static int check_partly_erased_sectors(struct jffs_fmcontrol *fmc){

#define NUM_REREADS             4 /* see note above */
#define READ_AHEAD_BYTES        4096 /* must be a multiple of 4, 
					usually set to kernel page size */

	__u8 *read_buf1;
	__u8 *read_buf2;

	int err = 0;
	int retlen;
	int i;
	int cnt;
	__u32 offset;
	loff_t pos = 0;
	loff_t end = fmc->flash_size;


	/* Allocate read buffers */
	read_buf1 = kmalloc(sizeof(__u8) * READ_AHEAD_BYTES, GFP_KERNEL);
	if (!read_buf1)
		return -ENOMEM;

	read_buf2 = kmalloc(sizeof(__u8) * READ_AHEAD_BYTES, GFP_KERNEL);
	if (!read_buf2) {
		kfree(read_buf1);
		return -ENOMEM;
	}

 CHECK_NEXT:
	while(pos < end){
		
	   D3(printk("check_partly_erased_sector: checking sector which contains"
	   	  " offset 0x%x for flipping bits..\n", (__u32)pos));
		
		retlen = flash_safe_read(fmc->mtd, pos,
					 &read_buf1[0], READ_AHEAD_BYTES);
		retlen &= ~3;
		
		for(cnt = 0; cnt < NUM_REREADS; cnt++){
			(void)flash_safe_read(fmc->mtd, pos,
					      &read_buf2[0], READ_AHEAD_BYTES);
			
			for (i=0 ; i < retlen ; i+=4) {
				/* buffers MUST match, double word for word! */
				if(*((__u32 *) &read_buf1[i]) !=
				   *((__u32 *) &read_buf2[i])
				   ){
				        /* flipping bits detected, time to erase sector */
					/* This will help us log some statistics etc. */
					D1(printk("Flipping bits detected in re-read round:%i of %i\n",
					       cnt, NUM_REREADS));
					D1(printk("check_partly_erased_sectors:flipping bits detected"
						  " @offset:0x%x(0x%x!=0x%x)\n",
						  (__u32)pos+i, *((__u32 *) &read_buf1[i]), 
						  *((__u32 *) &read_buf2[i])));
					
				        /* calculate start of present sector */
					offset = (((__u32)pos+i)/(__u32)fmc->sector_size) * (__u32)fmc->sector_size;
					
					D1(printk("check_partly_erased_sector: erasing sector starting 0x%x.\n",
						  offset));
					
					if (flash_erase_region(fmc->mtd,
							       offset, fmc->sector_size) < 0) {
						printk(KERN_ERR "JFFS: Erase of flash failed. "
						       "offset = %u, erase_size = %d\n",
						       offset , fmc->sector_size);
						
						err = -EIO;
						goto returnBack;

					}else{
						D1(printk("JFFS: Erase of flash sector @0x%x successful.\n",
						       offset));
						/* skip ahead to the next sector */
						pos = (((__u32)pos+i)/(__u32)fmc->sector_size) * (__u32)fmc->sector_size;
						pos += fmc->sector_size;
						goto CHECK_NEXT;
					}
				}
			}
		}
		pos += READ_AHEAD_BYTES;
	}

 returnBack:
	kfree(read_buf1);
	kfree(read_buf2);

	D2(printk("check_partly_erased_sector: Done checking all sectors till offset 0x%x for flipping bits.\n",
		  (__u32)pos));

	return err;

}/* end check_partly_erased_sectors() */


/* Scan the whole flash memory in order to find all nodes in the
   file systems.  */
static int
jffs_scan_flash(struct jffs_control *c)
{
	char name[JFFS_MAX_NAME_LEN + 2];
	struct jffs_raw_inode raw_inode;
	struct jffs_node *node = NULL;
	struct jffs_fmcontrol *fmc = c->fmc;
	__u32 checksum;
	__u16 checksum16;
	__u8 tmp_accurate;
	__u32 deleted_file;
	loff_t pos = 0;
	loff_t start;
	loff_t test_start;
	loff_t end = fmc->flash_size;
	__u8 *read_buf;
	int i, len, retlen;
	__u32 offset;
	__u32 obsolete_tot = 0;
	__u32 free_chunk_size1;
	__u32 free_chunk_size2;

	
#define NUMFREEALLOWED     2        /* 2 chunks of at least erase size space allowed */
	int num_free_space = 0;       /* Flag err if more than TWO
				       free blocks found. This is NOT allowed
				       by the current jffs design.
				    */
	int num_free_spc_not_accp = 0; /* For debugging purposed keep count 
					of how much free space was rejected and
					marked dirty
				     */

	D1(printk("jffs_scan_flash: start pos = 0x%lx, end = 0x%lx\n",
		  (long)pos, (long)end));

	/*
	  check and make sure that any sector does not suffer
	  from the "partly erased, bit flipping syndrome" (TM Vipin :)
	  If so, offending sectors will be erased.
	*/
	if(check_partly_erased_sectors(fmc) < 0){
		return -EIO; /* bad, bad, bad error. Cannot continue.*/
	}

	/* Allocate read buffer */
	read_buf = kmalloc(sizeof(__u8) * 4096, GFP_KERNEL);
	if (!read_buf) {
		return -ENOMEM;
	}
			      
	/* Start the scan.  */
	while (pos < end) {
		deleted_file = 0;

		/* Remember the position from where we started this scan.  */
		start = pos;

		switch (flash_read_u32(fmc->mtd, pos)) {
		case JFFS_EMPTY_BITMASK:
			/* We have found 0xffffffff at this position.  We have to
			   scan the rest of the flash till the end or till
			   something else than 0xffffffff is found.
		           Keep going till we do not find JFFS_EMPTY_BITMASK 
			   anymore */

			D1(printk("jffs_scan_flash: 0xffffffff at pos 0x%lx.\n",
				  (long)pos));

		        while (pos < end){
			      len = end - pos < 4096 ? end - pos : 4096;
			      retlen = flash_safe_read(fmc->mtd, pos,
						 &read_buf[0], len);
			      retlen &= ~3;
			      for (i=0 ; i < retlen ; i+=4, pos += 4) {
				      if(*((__u32 *) &read_buf[i]) !=
					 JFFS_EMPTY_BITMASK)
					break;
			      }
			      if (i == retlen)
				    continue;
			      else
				    break;
			}

			D1(printk("jffs_scan_flash: 0xffffffff ended at pos 0x%lx.\n",
				  (long)pos));
			
			/* If some free space ends in the middle of a sector,
			   treat it as dirty rather than clean.
			   This is to handle the case where one thread (i.e., program)
			   allocated space for a node, but didn't get to
			   actually _write_ it before power was lost, leaving
			   a gap in the log.   I'm not sure that this can happen (since
			   we take out a lock), but no matter; free space can only end
			   at a sector boundary.
			*/
			if ((__u32) pos % fmc->sector_size) {
				/* If there was free space in previous 
				   sectors, don't mark that dirty too - 
				   only from the beginning of this sector
				   (or from start) 
				*/

			        test_start = pos & ~(fmc->sector_size-1); /* end of last sector */

				if (start < test_start) {
				        /* free space started in the previous sector! */
					if((num_free_space < NUMFREEALLOWED) && 
					   ((unsigned int)(test_start - start) >= fmc->sector_size)){
				                /*
						  Count it in if we are still under NUMFREEALLOWED *and* it is 
						  at least 1 erase sector in length. This will keep us from 
						  picking any little ole' space as "free".
						*/
					  
					        D1(printk("Reducing end of free space to 0x%x from 0x%x\n",
							  (unsigned int)test_start, (unsigned int)pos));

						D1(printk("Free space accepted: Starting 0x%x for 0x%x bytes\n",
							  (unsigned int) start,
							  (unsigned int)(test_start - start)));

						/* below, space from "start" to "pos" will be marked dirty. */
						start = test_start; 
						
						/* Being in here means that we have found at least an entire 
						   erase sector size of free space ending on a sector boundary.
						   Keep track of free spaces accepted.
						*/
						num_free_space++;
					}
					else {
					        num_free_spc_not_accp++;
					       D1(printk("Free space (#%i) found but *Not* accepted: Starting"
							  " 0x%x for 0x%x bytes\n",
							  num_free_spc_not_accp, (unsigned int)start, 
							  (unsigned int)((unsigned int)(pos & ~(fmc->sector_size-1)) - (unsigned int)start)));
					        
					}
					
				}
				if((((__u32)(pos - start)) != 0)){

				        D1(printk("Dirty space: Starting 0x%x for 0x%x bytes\n",
						  (unsigned int) start, (unsigned int) (pos - start)));
					jffs_fmalloced(fmc, (__u32) start,
						       (__u32) (pos - start), NULL);
				}
				else {
					/* "Flipping bits" detected. This means that our scan for them
					   did not catch this offset. See check_partly_erased_sectors() for
					   more info.
					*/
				        
					D1(printk("jffs_scan_flash: wants to allocate dirty flash "
						  "space for 0 bytes.\n"));
					D1(printk("jffs_scan_flash: Flipping bits! We will free "
						  "all allocated memory, erase this sector and remount\n"));

					/* calculate start of present sector */
					offset = (((__u32)pos)/(__u32)fmc->sector_size) * (__u32)fmc->sector_size;
					
					D1(printk("jffs_scan_flash: erasing sector starting 0x%x.\n",
						  offset));
					
					if (flash_erase_region(fmc->mtd,
							       offset, fmc->sector_size) < 0) {
						printk(KERN_ERR "JFFS: Erase of flash failed. "
						       "offset = %u, erase_size = %d\n",
						       offset , fmc->sector_size);

						kfree(read_buf);
						return -1; /* bad, bad, bad! */

					}
					kfree(read_buf);

					return -EAGAIN; /* erased offending sector. Try mount one more time please. */
				}
			}
			else {
			   /* Being in here means that we have found free space that ends on an erase sector
			      boundary.
			      Count it in if we are still under NUMFREEALLOWED *and* it is at least 1 erase 
			      sector in length. This will keep us from picking any little ole' space as "free".
			      But if we are doing inplace GC and this space is at the end of
			      the flash, then this *is* a (actually: the sole) valid free chunk.
			      But perhaps we crashed after doing a GC but before erasing the now dirty block.
			      The nodes moved off the head block will appear twice.  So take the obsolete nodes
			      into account.
			      In these cases, we *do* want to pick up any little ole' space as "free".
			   */
			   if (((num_free_space < NUMFREEALLOWED) && 
				((unsigned int)(pos - start) >= fmc->sector_size)) ||
			       (num_free_space == 0 && fmc->do_inplace_gc && pos == fmc->flash_size) ||
			       (num_free_space == 0 && ((unsigned int)(pos - start) + obsolete_tot >= fmc->sector_size))) {
			      /* We really don't do anything to mark space as free, except *not* 
				 mark it dirty and just advance the "pos" location pointer. 
				 It will automatically be picked up as free space.
			      */ 
			      num_free_space++;
			      D1(printk("Free space accepted: Starting 0x%x for 0x%x bytes\n",
					 (unsigned int) start, (unsigned int) (pos - start)));
			   }
			   else {
			      num_free_spc_not_accp++;
			      D1(printk("Free space (#%i) found but *Not* accepted: Starting "
					 "0x%x for 0x%x bytes\n", num_free_spc_not_accp, 
					 (unsigned int) start, 
					 (unsigned int) (pos - start)));
					 
			      /* Mark this space as dirty. We already have our free space. */
			      D1(printk("Dirty space: Starting 0x%x for 0x%x bytes\n",
					 (unsigned int) start, (unsigned int) (pos - start)));
			      jffs_fmalloced(fmc, (__u32) start,
					     (__u32) (pos - start), 0);				           
			   }
				 
			}
			if(num_free_space > NUMFREEALLOWED){
			         printk(KERN_WARNING "jffs_scan_flash: Found free space "
					"number %i. Only %i free space is allowed.\n",
					num_free_space, NUMFREEALLOWED);			      
			}
			continue;

		case JFFS_DIRTY_BITMASK:
			/* We have found 0x00000000 at this position.  Scan as far
			   as possible to find out how much is dirty.  */
			D1(printk("jffs_scan_flash: 0x00000000 at pos 0x%lx.\n",
				  (long)pos));
			for (; pos < end
			       && JFFS_DIRTY_BITMASK == flash_read_u32(fmc->mtd, pos);
			     pos += 4);
			D1(printk("jffs_scan_flash: 0x00 ended at "
				  "pos 0x%lx.\n", (long)pos));
			jffs_fmalloced(fmc, (__u32) start,
				       (__u32) (pos - start), NULL);
			continue;

		case JFFS_MAGIC_BITMASK:
			/* We have probably found a new raw inode.  */
			break;

		default:
		bad_inode:
			/* We're f*cked.  This is not solved yet.  We have
			   to scan for the magic pattern.  */
			D1(printk("*************** Dirty flash memory or "
				  "bad inode: "
				  "hexdump(pos = 0x%lx, len = 128):\n",
				  (long)pos));
			D1(jffs_hexdump(fmc->mtd, pos, 128));

			for (pos += 4; pos < end; pos += 4) {
				switch (flash_read_u32(fmc->mtd, pos)) {
				case JFFS_MAGIC_BITMASK:
				case JFFS_EMPTY_BITMASK:
					/* handle these in the main switch() loop */
					goto cont_scan;

				default:
					break;
				}
			}

			cont_scan:
			/* First, mark as dirty the region
			   which really does contain crap. */
			jffs_fmalloced(fmc, (__u32) start,
				       (__u32) (pos - start),
				       NULL);
			
			continue;
		}/* switch */

		/* We have found the beginning of an inode.  Create a
		   node for it unless there already is one available.  */
		if (!node) {
			if (!(node = jffs_alloc_node())) {
				/* Free read buffer */
				kfree(read_buf);
				return -ENOMEM;
			}
			DJM(no_jffs_node++);
		}

		/* Read the next raw inode.  */
		flash_safe_read(fmc->mtd, pos, (u_char *) &raw_inode,
				sizeof(struct jffs_raw_inode));

		/* When we compute the checksum for the inode, we never
		   count the 'accurate' or the 'checksum' fields.  */
		tmp_accurate = raw_inode.accurate;
		raw_inode.accurate = 0xff;
		checksum16 = jffs_checksum_16(&raw_inode, INODE_CHK_SIZ);
		raw_inode.accurate = tmp_accurate;

		D3(printk("*** We have found this raw inode at pos 0x%lx "
			  "on the flash:\n", (long)pos));
		D3(jffs_print_raw_inode(&raw_inode));

		if (checksum16 != raw_inode.ichksum) {
			printk("jffs_scan_flash: Bad checksum [%5x]: "
				  "checksum = %04x, "
				  "raw_inode.ichksum = %04x\n",
			       (unsigned)pos, checksum16, raw_inode.ichksum);
			pos += sizeof(struct jffs_raw_inode);
			jffs_fmalloced(fmc, (__u32) start,
				       (__u32) (pos - start), NULL);
			/* Reuse this unused struct jffs_node.  */
			continue;
		}

		/* Check the raw inode read so far.  Start with the
		   maximum length of the filename.  */
		if (raw_inode.nsize > JFFS_MAX_NAME_LEN) {
			printk(KERN_WARNING "jffs_scan_flash: Found a "
			       "JFFS node with name too large\n");
			goto bad_inode;
		}

		if (raw_inode.rename && raw_inode.dsize != sizeof(__u32)) {
			printk(KERN_WARNING "jffs_scan_flash: Found a "
			       "rename node with dsize %u.\n",
			       raw_inode.dsize);
			goto bad_inode;
		}

		/* The node's data segment should not exceed a
		   certain length.  */
		if (raw_inode.dsize > fmc->max_chunk_size) {
			printk(KERN_WARNING "jffs_scan_flash: Found a "
			       "JFFS node with dsize (0x%x) > max_chunk_size (0x%x)\n",
			       raw_inode.dsize, fmc->max_chunk_size);
			goto bad_inode;
		}

		pos += sizeof(struct jffs_raw_inode);

		/* This shouldn't be necessary because a node that
		   violates the flash boundaries shouldn't be written
		   in the first place. */
		if (pos >= end) {
			goto check_node;
		}

		/* Read the name.  */
		*name = 0;
		if (raw_inode.nsize) {
		        flash_safe_read(fmc->mtd, pos, name, raw_inode.nsize);
			name[raw_inode.nsize] = '\0';
			pos += raw_inode.nsize
			       + JFFS_GET_PAD_BYTES(raw_inode.nsize);
			D3(printk("name == \"%s\"\n", name));
			checksum16 = jffs_checksum_16(name, raw_inode.nsize);
			if (checksum16 != raw_inode.nchksum) {
				printk("jffs_scan_flash: Bad checksum [%5x]: "
					  "checksum = %04x, "
					  "raw_inode.nchksum = %04x\n",
				       (unsigned)pos, checksum16, raw_inode.nchksum);
				jffs_fmalloced(fmc, (__u32) start,
					       (__u32) (pos - start), NULL);
				/* Reuse this unused struct jffs_node.  */
				continue;
			}
			if (pos >= end) {
				goto check_node;
			}
		}

		/* Read the data, if it exists, in order to be sure it
		   matches the checksum.  */
		if (raw_inode.dsize) {
			if (raw_inode.rename) {
				deleted_file = flash_read_u32(fmc->mtd, pos);
			}
			if (jffs_checksum_flash(fmc->mtd, pos, raw_inode.dsize, &checksum)) {
				printk("jffs_checksum_flash() failed to calculate a checksum\n");
				jffs_fmalloced(fmc, (__u32) start,
					       (__u32) (pos - start), NULL);
				/* Reuse this unused struct jffs_node.  */
				continue;
			}				
			pos += raw_inode.dsize
			       + JFFS_GET_PAD_BYTES(raw_inode.dsize);

			if (checksum != raw_inode.dchksum) {
				printk("jffs_scan_flash: Bad checksum [%5x]: "
					  "checksum = %08x, "
					  "raw_inode.dchksum = %08x\n",
				       (unsigned)pos, checksum, raw_inode.dchksum);
				jffs_fmalloced(fmc, (__u32) start,
					       (__u32) (pos - start), NULL);
				/* Reuse this unused struct jffs_node.  */
				continue;
			}
		}

		check_node:

		/* Remember the highest inode number in the whole file
		   system.  This information will be used when assigning
		   new files new inode numbers.  */
		if (c->next_ino <= raw_inode.ino) {
			c->next_ino = raw_inode.ino + 1;
		}

		if (raw_inode.accurate) {
			int err;
			node->data_offset = raw_inode.offset;
			node->data_size = raw_inode.dsize;
			node->removed_size = raw_inode.rsize;
			D1(printk("  ver=%04lX [ofs: %8lx  siz: %8lx  remv: %8lx]\n",
				   (long)raw_inode.version,
				   (long)raw_inode.offset,
				   (long)raw_inode.dsize,
				   (long)raw_inode.rsize));
			/* Compute the offset to the actual data in the
			   on-flash node.  */
			node->fm_offset = sizeof(struct jffs_raw_inode)
			  + raw_inode.nsize
			  + JFFS_GET_PAD_BYTES(raw_inode.nsize);
			node->fm = jffs_fmalloced(fmc, (__u32) start,
						  (__u32) (pos - start),
						  node);
			if (!node->fm) {
				D1(printk("jffs_scan_flash: !node->fm\n"));
				jffs_free_node(node);
				DJM(no_jffs_node--);

				/* Free read buffer */
				kfree(read_buf);
				return -ENOMEM;
			}
			if ((err = jffs_insert_node(c, NULL, &raw_inode,
						    name, node)) < 0) {
				printk("JFFS: Failed to handle raw inode. "
				       "(err = %d)\n", err);
				break;
			}
			if (raw_inode.rename) {
				struct jffs_delete_list *dl
				= (struct jffs_delete_list *)
				  kmalloc(sizeof(struct jffs_delete_list),
					  GFP_KERNEL);
				if (!dl) {
					D1(printk("jffs_scan_flash: !dl\n"));
					jffs_free_node(node);
					DJM(no_jffs_node--);
					/* Free read buffer */
					kfree(read_buf);

					return -ENOMEM;
				}
				dl->ino = deleted_file;
				dl->next = c->delete_list;
				c->delete_list = dl;
				node->data_size = 0;
			}
			D3(jffs_print_node(node));
			node = NULL; /* Don't free the node!  */
		}
		else {
			jffs_fmalloced(fmc, (__u32) start,
				       (__u32) (pos - start), NULL);
			D3(printk("jffs_scan_flash: Just found an obsolete "
				   "raw_inode siz: %lx. Continuing the scan...\n", (long)(pos - start)));
			obsolete_tot += (pos - start);
			/* Reuse this unused struct jffs_node.  */
		}
	}

	if (node) {
		jffs_free_node(node);
		DJM(no_jffs_node--);
	}
	jffs_build_end(fmc);

	/* Free read buffer */
	kfree(read_buf);

	if(!num_free_space){
	        printk(KERN_WARNING "jffs_scan_flash: Did not find even a single "
		       "chunk of free space. This is BAD!\n");
	}

	/* Return happy */
	D3(printk("jffs_scan_flash: Leaving...\n"));

	/* This is to trap the "free size accounting screwed error. */
	free_chunk_size1 = jffs_free_size1(fmc);
	free_chunk_size2 = jffs_free_size2(fmc);

	if (free_chunk_size1 + free_chunk_size2 != fmc->free_size) {

		printk(KERN_WARNING "jffs_scan_falsh: Free size accounting screwed\n");
		printk(KERN_WARNING "jfffs_scan_flash: free_chunk_size1 == 0x%x, "
		       "free_chunk_size2 == 0x%x, fmc->free_size == 0x%x\n", 
		       free_chunk_size1, free_chunk_size2, fmc->free_size);

		return -1; /* Do NOT mount f/s so that we can inspect what happened.
			      Mounting this  screwed up f/s will screw us up anyway.
			    */
	}	

	return 0; /* as far as we are concerned, we are happy! */
} /* jffs_scan_flash()  */


/* Insert any kind of node into the file system.  Take care of data
   insertions and deletions.  Also remove redundant information. The
   memory allocated for the `name' is regarded as "given away" in the
   caller's perspective.  */
int jffs_insert_node(struct jffs_control *c, struct jffs_file *f,
		 const struct jffs_raw_inode *raw_inode,
		 const char *name, struct jffs_node *node)
{
	int update_name = 0;
	int insert_into_tree = 0;

	D2(printk("jffs_insert_node: ino = %u, version = %04x, "
		  "name = \"%s\", deleted = %d\n",
		  raw_inode->ino, raw_inode->version,
		  ((name && *name) ? name : ""), raw_inode->deleted));

	/* If there doesn't exist an associated jffs_file, then
	   create, initialize and insert one into the file system.  */
	if (!f && !(f = jffs_find_file(c, raw_inode->ino))) {
		if (!(f = jffs_create_file(c, raw_inode))) {
			return -ENOMEM;
		}
		jffs_insert_file_into_hash(f);
		insert_into_tree = 1;
	}
	node->ino = raw_inode->ino;
	node->version = raw_inode->version;
	node->data_size = raw_inode->dsize;
	node->fm_offset = sizeof(struct jffs_raw_inode) + raw_inode->nsize
			  + JFFS_GET_PAD_BYTES(raw_inode->nsize);
	node->name_size = raw_inode->nsize;

	/* Now insert the node at the correct position into the file's
	   version list.  */
	if (!f->version_head) {
		/* This is the first node.  */
		f->version_head = node;
		f->version_tail = node;
		node->version_prev = NULL;
		node->version_next = NULL;
		f->highest_version = node->version;
		update_name = 1;
		f->mode = raw_inode->mode;
		f->uid = raw_inode->uid;
		f->gid = raw_inode->gid;
		f->atime = raw_inode->atime;
		f->mtime = raw_inode->mtime;
		f->ctime = raw_inode->ctime;
	}
	else if ((f->highest_version < node->version)
		 || (node->version == 0)) {
		/* Insert at the end of the list.  I.e. this node is the
		   newest one so far.  */
		node->version_prev = f->version_tail;
		node->version_next = NULL;
		f->version_tail->version_next = node;
		f->version_tail = node;
		f->highest_version = node->version;
		update_name = 1;
		f->pino = raw_inode->pino;
		f->mode = raw_inode->mode;
		f->uid = raw_inode->uid;
		f->gid = raw_inode->gid;
		f->atime = raw_inode->atime;
		f->mtime = raw_inode->mtime;
		f->ctime = raw_inode->ctime;
	}
	else if (f->version_head->version > node->version) {
		/* Insert at the bottom of the list.  */
		node->version_prev = NULL;
		node->version_next = f->version_head;
		f->version_head->version_prev = node;
		f->version_head = node;
		if (!f->name) {
			update_name = 1;
		}
	}
	else {
		struct jffs_node *n;
		int newer_name = 0;
		/* Search for the insertion position starting from
		   the tail (newest node).  */
		for (n = f->version_tail; n; n = n->version_prev) {
			if (n->version < node->version) {
				node->version_prev = n;
				node->version_next = n->version_next;
				node->version_next->version_prev = node;
				n->version_next = node;
				if (!newer_name) {
					update_name = 1;
				}
				break;
			}
			if (n->name_size) {
				newer_name = 1;
			}
		}
	}

	/* Deletion is irreversible. If any 'deleted' node is ever
	   written, the file is deleted */
	if (raw_inode->deleted)
		f->deleted = raw_inode->deleted;

	/* Perhaps update the name.  */
	if (raw_inode->nsize && update_name && name && *name && (name != f->name)) {
		if (f->name) {
			kfree(f->name);
			DJM(no_name--);
		}
		if (!(f->name = kmalloc(raw_inode->nsize + 1,
						 GFP_KERNEL))) {
			return -ENOMEM;
		}
		DJM(no_name++);
		memcpy(f->name, name, raw_inode->nsize);
		f->name[raw_inode->nsize] = '\0';
		f->nsize = raw_inode->nsize;
		D3(printk("jffs_insert_node: Updated the name of "
			  "the file to \"%s\".\n", name));
	}

	if (!c->building_fs) {
		D3(printk("jffs_insert_node: ---------------------------"
			  "------------------------------------------- 1\n"));
		if (insert_into_tree) {
			jffs_insert_file_into_tree(f);
		}
		/* Once upon a time, we would call jffs_possibly_delete_file()
		   here. That causes an oops if someone's still got the file
		   open, so now we only do it in jffs_delete_inode()
		   -- dwmw2
		*/
		if (node->data_size || node->removed_size) {
			jffs_update_file(f, node);
		}
		jffs_remove_redundant_nodes(f);
		jffs_garbage_collect_trigger(c);

		D3(printk("jffs_insert_node: ---------------------------"
			  "------------------------------------------- 2\n"));
	}

	return 0;
} /* jffs_insert_node()  */


/* Unlink a jffs_node from the version list it is in.  */
static inline void jffs_unlink_node_from_version_list(struct jffs_file *f,
				   struct jffs_node *node)
{
	if (node->version_prev) {
		node->version_prev->version_next = node->version_next;
	} else {
		f->version_head = node->version_next;
	}
	if (node->version_next) {
		node->version_next->version_prev = node->version_prev;
	} else {
		f->version_tail = node->version_prev;
	}
}


/* Unlink a jffs_node from the range list it is in.  */
static inline void jffs_unlink_node_from_range_list(struct jffs_file *f, struct jffs_node *node)
{
	if (node->range_prev) {
		node->range_prev->range_next = node->range_next;
	}
	else {
		f->range_head = node->range_next;
	}
	if (node->range_next) {
		node->range_next->range_prev = node->range_prev;
	}
	else {
		f->range_tail = node->range_prev;
	}
}


/* Function used by jffs_remove_redundant_nodes() below.  This function
   classifies what kind of information a node adds to a file.  */
static inline __u8 jffs_classify_node(struct jffs_node *node)
{
	__u8 mod_type = JFFS_MODIFY_INODE;

	if (node->name_size) {
		mod_type |= JFFS_MODIFY_NAME;
	}
	if (node->data_size || node->removed_size) {
		mod_type |= JFFS_MODIFY_DATA;
	}
	return mod_type;
}


/* Remove redundant nodes from a file.  Mark the on-flash memory
   as dirty.  */
static int
jffs_remove_redundant_nodes(struct jffs_file *f)
{
	struct jffs_node *newest_node;
	struct jffs_node *cur;
	struct jffs_node *prev;
	__u8 newest_type;
	__u8 mod_type;
	__u8 node_with_name_later = 0;

	if (!(newest_node = f->version_tail)) {
		return 0;
	}

	/* What does the `newest_node' modify?  */
	newest_type = jffs_classify_node(newest_node);
	node_with_name_later = newest_type & JFFS_MODIFY_NAME;

	D3(printk("jffs_remove_redundant_nodes: ino: %u, name: \"%s\", "
		  "newest_type: %u\n", f->ino, (f->name ? f->name : ""),
		  newest_type));

	/* Traverse the file's nodes and determine which of them that are
	   superfluous.  Yeah, this might look very complex at first
	   glance but it is actually very simple.  */
	for (cur = newest_node->version_prev; cur; cur = prev) {
		prev = cur->version_prev;
		mod_type = jffs_classify_node(cur);
		if ((mod_type <= JFFS_MODIFY_INODE)
		    || ((newest_type & JFFS_MODIFY_NAME)
			&& (mod_type
			    <= (JFFS_MODIFY_INODE + JFFS_MODIFY_NAME)))
		    || (cur->data_size == 0 && cur->removed_size
			&& !cur->version_prev && node_with_name_later)) {
			/* Yes, this node is redundant. Remove it.  */
			D2(printk("jffs_remove_redundant_nodes: "
				  "Removing node: ino: %u, version: %04x, "
				  "mod_type: %u\n", cur->ino, cur->version,
				  mod_type));
			jffs_unlink_node_from_version_list(f, cur);
			jffs_fmfree(f->c->fmc, cur->fm, cur);
			jffs_free_node(cur);
			DJM(no_jffs_node--);
		}
		else {
			node_with_name_later |= (mod_type & JFFS_MODIFY_NAME);
		}
	}

	return 0;
}


/* Insert a file into the hash table.  */
static int
jffs_insert_file_into_hash(struct jffs_file *f)
{
	int i = f->ino % f->c->hash_len;

	D3(printk("jffs_insert_file_into_hash: f->ino: %u\n", f->ino));

	list_add(&f->hash, &f->c->hash[i]);
	return 0;
}


/* Insert a file into the file system tree.  */
int jffs_insert_file_into_tree(struct jffs_file *f)
{
	struct jffs_file *parent;

	D3(printk("jffs_insert_file_into_tree: name: \"%s\"\n",
		  (f->name ? f->name : "")));

	if (!(parent = jffs_find_file(f->c, f->pino))) {
		if (f->pino == 0) {
			f->c->root = f;
			f->parent = NULL;
			f->sibling_prev = NULL;
			f->sibling_next = NULL;
			return 0;
		}
		else {
			D1(printk("jffs_insert_file_into_tree: Found "
				  "inode with no parent and pino == %u\n",
				  f->pino));
			return -1;
		}
	}
	f->parent = parent;
	f->sibling_next = parent->children;
	if (f->sibling_next) {
		f->sibling_next->sibling_prev = f;
	}
	f->sibling_prev = NULL;
	parent->children = f;
	return 0;
}


/* Remove a file from the hash table.  */
static int
jffs_unlink_file_from_hash(struct jffs_file *f)
{
	D3(printk("jffs_unlink_file_from_hash: f: 0x%p, "
		  "ino %u\n", f, f->ino));

	list_del(&f->hash);
	return 0;
}


/* Just remove the file from the parent's children.  Don't free
   any memory.  */
int jffs_unlink_file_from_tree(struct jffs_file *f)
{
	D3(printk("jffs_unlink_file_from_tree: ino: %d, pino: %d, name: "
		  "\"%s\"\n", f->ino, f->pino, (f->name ? f->name : "")));

	if (f->sibling_prev) {
		f->sibling_prev->sibling_next = f->sibling_next;
	}
	else if (f->parent) {
	        D3(printk("f->parent=%p\n", f->parent));
		f->parent->children = f->sibling_next;
	}
	if (f->sibling_next) {
		f->sibling_next->sibling_prev = f->sibling_prev;
	}
	return 0;
}


/* Find a file with its inode number.  */
struct jffs_file *jffs_find_file(struct jffs_control *c, __u32 ino)
{
	struct jffs_file *f;
	int i = ino % c->hash_len;

	D3(printk("jffs_find_file: ino: %u\n", ino));

	list_for_each_entry(f, &c->hash[i], hash) {
		if (ino != f->ino)
			continue;
		D3(printk("jffs_find_file: Found file with ino "
			       "%u. (name: \"%s\")\n",
			       ino, (f->name ? f->name : ""));
		);
		return f;
	}
	D3(printk("jffs_find_file: Didn't find file "
			 "with ino %u.\n", ino);
	);
	return NULL;
}


/* Find a file in a directory.  We are comparing the names.  */
struct jffs_file *jffs_find_child(struct jffs_file *dir, const char *name, int len)
{
	struct jffs_file *f;

	D3(printk("jffs_find_child()\n"));

	for (f = dir->children; f; f = f->sibling_next) {
		if (!f->deleted && f->name
		    && !strncmp(f->name, name, len)
		    && f->name[len] == '\0') {
			break;
		}
	}

	D3(if (f) {
		printk("jffs_find_child: Found \"%s\".\n", f->name);
	}
	else {
		char *copy = kmalloc(len + 1, GFP_KERNEL);
		if (copy) {
			memcpy(copy, name, len);
			copy[len] = '\0';
		}
		printk("jffs_find_child: Didn't find the file \"%s\".\n",
		       (copy ? copy : ""));
		kfree(copy);
	});

	return f;
}


int flash_memset(struct jffs_fmcontrol *fmc, loff_t to,
	     const u_char c, size_t size)
{
	static unsigned char pattern[64];
	int i;

	/* fill up pattern */

	for(i = 0; i < 64; i++)
		pattern[i] = c;

	/* write as many 64-byte chunks as we can */

	while (size >= 64) {
		flash_safe_write(fmc, to, pattern, 64);
		size -= 64;
		to += 64;
	}

	/* and the rest */

	if(size)
		flash_safe_write(fmc, to, pattern, size);

	return size;
}


/* Write a raw inode that takes up a certain amount of space in the flash
   memory.  At the end of the flash device, there is often space that is
   impossible to use.  At these times we want to mark this space as not
   used.  In the cases when the amount of space is greater or equal than
   a struct jffs_raw_inode, we write a "dummy node" that takes up this
   space.  The space after the raw inode, if it exists, is left as it is.

   If the space left on the device is less than the size of a struct
   jffs_raw_inode, this space is filled with JFFS_DIRTY_BITMASK bytes.
   No raw inode is written this time.  */
int
jffs_write_dummy_node(struct jffs_control *c, struct jffs_fm *dirty_fm)
{
	struct jffs_fmcontrol *fmc = c->fmc;
	int err;
	struct jffs_raw_inode raw_inode;
	unsigned long pos = 0;

	D1(printk("jffs_write_dummy_node: dirty_fm->offset = 0x%08x, "
		  "dirty_fm->size = %u\n",
		  dirty_fm->offset, dirty_fm->size));
	pos = dirty_fm->offset + sizeof(struct jffs_raw_inode);
	if (dirty_fm->size >= sizeof(struct jffs_raw_inode)) {
		memset(&raw_inode, 0, sizeof(struct jffs_raw_inode));
		raw_inode.magic = JFFS_MAGIC_BITMASK;
		raw_inode.dsize = dirty_fm->size
				  - sizeof(struct jffs_raw_inode);
		raw_inode.accurate = 0xff;
		jffs_checksum_flash(fmc->mtd, pos, raw_inode.dsize, &raw_inode.dchksum);
		raw_inode.ichksum = jffs_checksum_16(&raw_inode, INODE_CHK_SIZ);
		raw_inode.accurate = 0;
		if ((err = flash_safe_write(fmc, dirty_fm->offset,
					    (u_char *)&raw_inode,
					    sizeof(struct jffs_raw_inode))) < 0) {
			printk(KERN_ERR "JFFS: jffs_write_dummy_node: "
			       "flash_safe_write failed!\n");
			return err;
		}
	}
	else {
		flash_memset(fmc, dirty_fm->offset, 0, dirty_fm->size);
	}

	D3(printk("jffs_write_dummy_node: Leaving...\n"));
	return 0;
}


int jffs_enough_space(struct jffs_control *c, int needed, int slack)
{
   struct jffs_fmcontrol *fmc = c->fmc;
   int stat;
   
   while (1) {
      if ((fmc->flash_size - (fmc->used_size + fmc->dirty_size)) 
	  >= fmc->min_free_size + needed + slack) {
	 return 1;
      }

      /* If we completely compact the dirty, we may have enough space for this write.
       * Wasted space isn't really used, it's dirt that isn't marked as such. */
      if ((fmc->flash_size - (fmc->used_size + needed) < fmc->min_free_size + needed + slack) &&
	   !(fmc->flash_size - (fmc->used_size - fmc->wasted_size + needed) < fmc->min_free_size + needed + slack))
	 printk("GCing wasted allows us to proceed.\n");

      if (fmc->flash_size - (fmc->used_size - fmc->wasted_size + needed) < fmc->min_free_size + needed + slack)
	 return 0;

      //printk(" Forcing GC pass!\n");
      if (fmc->do_inplace_gc) {
	 if (gc_inplace_begin(fmc) < 0)
	    return 0;
	 stat = jffs_garbage_collect_now(c, 1, 0);	/* We *must* move to convert dirty to free. */
	 /* Will this work right in all cases?  Maybe we do this only if
	  * used is < 1 block??   But it always will be! */
	 gc_inplace_end(fmc, stat);
	 return(stat > 0);
      }
      if (jffs_garbage_collect_now(c, 1, 0) <= 0) {
	 D1(printk("JFFS_ENOUGH_SPACE: jffs_garbage_collect_now() failed.\n"));
	 return 0;
      }
   }
}

/***************************************************************************/
/* Write a raw inode, possibly its name and possibly some data.  */
/* recoverable:
 *   0 = must have additional space for slack
 *   1 = no slack needed
 *  <0 = negative slack.  This node will be deleting a file.
 *  99 = Same as 0, but it is allowed to write this node shortet than dsize.
 *
 * Returns: Actual dsize written.  Or <0 for error.
 */
   
int
jffs_write_node(struct jffs_control *c, struct jffs_node *node,
		struct jffs_raw_inode *raw_inode,
		const char *name, const unsigned char *data,
		int recoverable,
		struct jffs_file *f)
{
	struct jffs_fmcontrol *fmc = c->fmc;
	struct jffs_fm *fm;
	static unsigned char allff[3]={255,255,255};
	__u32 avail_free;

	__u32 pos;
	int err;
	int slack = 0;

	int total_name_size = raw_inode->nsize
				+ JFFS_GET_PAD_BYTES(raw_inode->nsize);
	int total_data_size = raw_inode->dsize
				+ JFFS_GET_PAD_BYTES(raw_inode->dsize);
	int total_size = sizeof(struct jffs_raw_inode)
			   + total_name_size + total_data_size;
	
	/* If this node isn't something that will eventually let
	   GC free even more space, then don't allow it unless
	   there's "enough" free space still available.  The definition
	   of "enough" is somewhat arbitrary.  Might not even be needed.  Dunno.
	   If <0, we are deleting, so bypass the size check.
	*/
	if (recoverable < 0)
	   slack = recoverable;

	else if (recoverable == 0 || recoverable == 99)
	   slack = BLOCK_SIZE;

	/* Fire the retrorockets and shoot the fruiton torpedoes, sir!  */

	ASSERT(if (!node) {
		printk("jffs_write_node: node == NULL\n");
		return -EINVAL;
	});
	ASSERT(if (raw_inode && raw_inode->nsize && !name) {
		printk("*** jffs_write_node: nsize = %u but name == NULL\n",
		       raw_inode->nsize);
		return -EINVAL;
	});

	D1(printk("jffs_write_node: filename = \"%s\", ino = %u, "
		  "total_size = %u\n",
		  (name ? name : ""), raw_inode->ino,
		  total_size));

	if (0) {
retry:	   jffs_fmfree_partly(fmc, fm, 0);
	   printk(KERN_ERR "JFFS: jffs_write_node: Error on write to flash: %5x  err:%d\n", pos, err);
	}
	fm = NULL;
	err = 0;
	while (!fm) {
	   /* Deadlocks suck. */
	   D1(printk("write_node '%s' free_size: %u dirty: %u min_free_size: %u total_size: %u slack:%d \n",
		     name? name: "Null", fmc->free_size, fmc->dirty_size, fmc->min_free_size, total_size , slack));
	   D1(printk(" w: %ld  f+d: %u        minfree+size+slack: %d   nsize = %u \n", fmc->wasted_size, fmc->free_size+fmc->dirty_size,
		     fmc->min_free_size + total_size + slack, raw_inode? raw_inode->nsize: -1)); 
	   /* This fails even when free+dirty is large enough.  Even if free+dirty is enough. */

	   while(fmc->free_size < fmc->min_free_size + total_size + slack) {
	      D2(printk("Test failed.   Loop to try to get enough space.\n"));
	      if (!jffs_enough_space(c, total_size,  slack)) {
		 D1(printk("JFFS_ENOUGH_SPACE said No!\n"));
		 D1(printk("write_node '%s' free_size: %u dirty: %u min_free_size: %u total_size: %u slack:%d \n",
			   name? name: "Null", fmc->free_size, fmc->dirty_size, fmc->min_free_size, total_size , slack));
		 D1(printk(" w: %d  f+d: %u        minfree+size+slack: %d   nsize = %u \n",
			   (int)fmc->wasted_size, fmc->free_size+fmc->dirty_size,
			   fmc->min_free_size + total_size + slack, raw_inode? raw_inode->nsize: -1)); 
		 //printk("ENOSPC: %s @ %d\n", __FUNCTION__, __LINE__);
		 //jffs_print_fmcontrol(fmc);
		 //print_layout(c);
		 //jffs_foreach_file(c, jffs_print_file_nodes);
		 return -ENOSPC;
	      }
	      D1(printk("JFFS_ENOUGH_SPACE said yes!\n"));
	   }
	   D2printk("Enough free space for the write_node.\n");
		
	   /* Don't span into the next block. */
	   if (fmc->tail) {
	      /* Avail area is from the tail to the end of the tail block (used to be: end-of-flash). */
	      avail_free = fmc->sector_size - ((fmc->tail->offset + fmc->tail->size) % fmc->sector_size);
	      if (total_size > avail_free) {
		 if (recoverable != 99 ||
		     sizeof(struct jffs_raw_inode) + total_name_size + MIN_USEFUL_DATA_SIZE > avail_free) {
		    /* Avail area is too small, or we can't short-write this type of node.  Discard it. */
		    fm = jffs_fmalloced(fmc, fmc->tail->offset + fmc->tail->size, avail_free, NULL);
		    if (fm == 0) {
		       D(printk("jffs_write_node: jffs_fmalloced(0x%p, %u) "
				"failed!\n", fmc, total_size));
		       return -ENOMEM;
		    }
		    /*  Mark the space dirty and retry. */
		    if ((err = jffs_write_dummy_node(c, fm)) < 0) {
		       kfree(fm);
		       DJM(no_jffs_fm--);
		       D(printk("jffs_write_node: "
				"jffs_write_dummy_node: Failed!\n"));
		       return err;
		    }
		    fm = NULL;
		 }
		 else {	/* Reduce the size.  Caller must re-do the remaining data. */
		    raw_inode->dsize -= (total_size - avail_free);
		    total_data_size = raw_inode->dsize + JFFS_GET_PAD_BYTES(raw_inode->dsize);
		    total_size = sizeof(struct jffs_raw_inode) + total_name_size + total_data_size;
		 }
	      }
	   }
	   /* First try to allocate some flash memory.  */
	   err = jffs_fmalloc(fmc, total_size, node, &fm);
		
	   if (err == -ENOSPC) {
	      printk("ENOSPC: %s @ %d\n", __FUNCTION__, __LINE__);
	      /* Just out of space. GC and try again */
	      D1(printk(KERN_INFO "jffs_write_node: Calling jffs_garbage_collect_now()\n"));
	      if ((err = jffs_garbage_collect_now(c, 0, 0))) {
		 D(printk("jffs_write_node: jffs_garbage_collect_now() failed\n"));
		 return err;
	      }
	      continue;
	   } 

	   if (err < 0) {
	      D(printk("jffs_write_node: jffs_fmalloc(0x%p, %u) "
		       "failed!\n", fmc, total_size));
	      return err;
	   }

	   if (!fm->nodes) {
	      /* The jffs_fm struct that we got is not good enough.
		 Make that space dirty and try again  */
	      D1(printk("Node too small: req %5d  got %5d\n", total_size, fm->size));
	      if ((err = jffs_write_dummy_node(c, fm)) < 0) {
		 kfree(fm);
		 DJM(no_jffs_fm--);
		 D(printk("jffs_write_node: "
			  "jffs_write_dummy_node: Failed!\n"));
		 return err;
	      }
	      fm = NULL;
	   }
	} /* while(!fm) */
	node->fm = fm;

	ASSERT(if (fm->nodes == 0) {
		printk(KERN_ERR "jffs_write_node: fm->nodes == 0\n");
	});

	pos = node->fm->offset;

	/* Increment the version number here. We can't let the caller
	   set it beforehand, because we might have had to do GC on a node
	   of this file - and we'd end up reusing version numbers.
	*/
	if (f) {
		raw_inode->version = f->highest_version + 1;
		D1(printk (KERN_NOTICE "jffs_write_node: setting version of %s to %04X\n", f->name, raw_inode->version));

		/* if the file was deleted, set the deleted bit in the raw inode */
		if (f->deleted)
			raw_inode->deleted = 1;
	}
	/* Compute the checksum for the data and name chunks.  */
	raw_inode->dchksum = jffs_checksum(data, raw_inode->dsize);
	raw_inode->nchksum = jffs_checksum_16(name, raw_inode->nsize);
	raw_inode->accurate = 0xff;
	raw_inode->ichksum = jffs_checksum_16(raw_inode, INODE_CHK_SIZ);

	D1(printk("jffs_write_node: About to write this raw inode to the "
		  "flash at pos 0x%lx:\n", (long)pos));
	D3(jffs_print_raw_inode(raw_inode));

	/* Write the inode. */
	if ((err = flash_safe_write(fmc, pos, (void *)raw_inode, sizeof(struct jffs_raw_inode))) < 0)
	   goto retry;
	pos += sizeof(struct jffs_raw_inode);
	/* Write the name & pad.  If any. */
	if (raw_inode->nsize) {
	   if ((err = flash_safe_write(fmc, pos, (void *)name, raw_inode->nsize)) < 0)
	   goto retry;
	   pos += raw_inode->nsize;
	   if (JFFS_GET_PAD_BYTES(raw_inode->nsize)) {
	      if ((err = flash_safe_write(fmc, pos, allff, JFFS_GET_PAD_BYTES(raw_inode->nsize))) < 0)
		 goto retry;
	      pos += JFFS_GET_PAD_BYTES(raw_inode->nsize);
	   }
	}
	/* Write the data, if any. */
	if (raw_inode->dsize) {
	   if ((err = flash_safe_write(fmc, pos, (void *)data,  raw_inode->dsize)) < 0)
	      goto retry;
	}

	if (raw_inode->deleted)
		f->deleted = 1;

	D3(printk("jffs_write_node: Leaving...\n"));
	return raw_inode->dsize;
} /* jffs_write_node()  */


/* Read data from the node and write it to the buffer.  'node_offset'
   is how much we have read from this particular node before and which
   shouldn't be read again.  'max_size' is how much space there is in
   the buffer.  */
static int
jffs_get_node_data(struct jffs_file *f, struct jffs_node *node, 
		   unsigned char *buf,__u32 node_offset, __u32 max_size)
{
	struct jffs_fmcontrol *fmc = f->c->fmc;
	__u32 pos = node->fm->offset + node->fm_offset + node_offset;
	__u32 avail = node->data_size - node_offset;
	__u32 r;

	D2(printk("  jffs_get_node_data: file: \"%s\", ino: %u, "
		  "version: %04x, node_offset: %u\n",
		  f->name, node->ino, node->version, node_offset));

	r = min(avail, max_size);
	D3(printk(KERN_NOTICE "jffs_get_node_data\n"));
	flash_safe_read(fmc->mtd, pos, buf, r);

	D3(printk("  jffs_get_node_data: Read %u byte%s.\n",
		  r, (r == 1 ? "" : "s")));

	return r;
}


/* Read data from the file's nodes.  Write the data to the buffer
   'buf'.  'read_offset' tells how much data we should skip.  */
int jffs_read_data(struct jffs_file *f, unsigned char *buf, __u32 read_offset,
	       __u32 size)
{
	struct jffs_node *node;
	__u32 read_data = 0; /* Total amount of read data.  */
	__u32 node_offset = 0;
	__u32 pos = 0; /* Number of bytes traversed.  */

	D2(printk("jffs_read_data: file = \"%s\", read_offset = %d, "
		  "size = %u\n",
		  (f->name ? f->name : ""), read_offset, size));

	if (read_offset >= f->size) {
		D(printk("  f->size: %d\n", f->size));
		return 0;
	}

	/* First find the node to read data from.  */
	node = f->range_head;
	while (pos <= read_offset) {
		node_offset = read_offset - pos;
		if (node_offset >= node->data_size) {
			pos += node->data_size;
			node = node->range_next;
		}
		else {
			break;
		}
	}

	/* "Cats are living proof that not everything in nature
	   has to be useful."
	   - Garrison Keilor ('97)  */

	/* Fill the buffer.  */
	while (node && (read_data < size)) {
		int r;
		if (!node->fm) {
			/* This node does not refer to real data.  */
			r = min(size - read_data,
				     node->data_size - node_offset);
			memset(&buf[read_data], 0, r);
		}
		else if ((r = jffs_get_node_data(f, node, &buf[read_data],
						 node_offset,
						 size - read_data)) < 0) {
			return r;
		}
		read_data += r;
		node_offset = 0;
		node = node->range_next;
	}
	D3(printk("  jffs_read_data: Read %u bytes.\n", read_data));
	return read_data;
}


/* Used for traversing all nodes in the hash table.  */
int jffs_foreach_file(struct jffs_control *c, int (*func)(struct jffs_file *))
{
	int pos;
	int r;
	int result = 0;

	for (pos = 0; pos < c->hash_len; pos++) {
		struct jffs_file *f, *next;

		/* We must do _safe, because 'func' might remove the
		   current file 'f' from the list.  */
		list_for_each_entry_safe(f, next, &c->hash[pos], hash) {
			r = func(f);
			if (r < 0)
				return r;
			result += r;
		}
	}

	return result;
}


/* Free all nodes associated with a file.  */
static int
jffs_free_node_list(struct jffs_file *f)
{
	struct jffs_node *node;
	struct jffs_node *p;

	D3(printk("jffs_free_node_list: f #%u, \"%s\"\n",
		  f->ino, (f->name ? f->name : "")));
	node = f->version_head;
	while (node) {
		p = node;
		node = node->version_next;
		jffs_free_node(p);
		DJM(no_jffs_node--);
	}
	return 0;
}


/* Free a file and its name.  */
static int
jffs_free_file(struct jffs_file *f)
{
	D3(printk("jffs_free_file: f #%u, \"%s\"\n",
		  f->ino, (f->name ? f->name : "")));

	if (f->name) {
		kfree(f->name);
		DJM(no_name--);
	}
	kfree(f);
	no_jffs_file--;
	return 0;
}

long
jffs_get_file_count(void)
{
	return no_jffs_file;
}

/* See if a file is deleted. If so, mark that file's nodes as obsolete.  */
int
jffs_possibly_delete_file(struct jffs_file *f)
{
	struct jffs_node *n;

	ASSERT(if (!f) {
		printk(KERN_ERR "jffs_possibly_delete_file: f == NULL\n");
		return -1;
	});
	D1(printk("jffs_possibly_delete_file: ino: %u\n",
		  f->ino));

	if (f->deleted) {
		/* First try to remove all older versions.  Commence with
		   the oldest node.  */
		for (n = f->version_head; n; n = n->version_next) {
			if (!n->fm) {
				continue;
			}
			if (jffs_fmfree(f->c->fmc, n->fm, n) < 0) {
				break;
			}
		}
		/* Unlink the file from the filesystem.  */
		if (!f->c->building_fs) {
			jffs_unlink_file_from_tree(f);
		}
		jffs_unlink_file_from_hash(f);
		jffs_free_node_list(f);
		jffs_free_file(f);
	}
	return 0;
}


/* Used in conjunction with jffs_foreach_file() to count the number
   of files in the file system.  */
int jffs_file_count(struct jffs_file *f)
{
	return 1;
}


/* Build up a file's range list from scratch by going through the
   version list.  */
static int
jffs_build_file(struct jffs_file *f)
{
	struct jffs_node *n;

	D3(printk("jffs_build_file: ino: %u, name: \"%s\"\n",
		  f->ino, (f->name ? f->name : "")));

	for (n = f->version_head; n; n = n->version_next) {
		jffs_update_file(f, n);
	}
	jffs_remove_redundant_nodes(f);
	return 0;
}


/* Remove an amount of data from a file. If this amount of data is
   zero, that could mean that a node should be split in two parts.
   We remove or change the appropriate nodes in the lists.

   Starting offset of area to be removed is node->data_offset,
   and the length of the area is in node->removed_size.   */
static int jffs_delete_data(struct jffs_file *f, struct jffs_node *node)
{
	struct jffs_node *n;
	__u32 offset = node->data_offset;
	__u32 remove_size = node->removed_size;

	D3(printk("jffs_delete_data: offset = %u, remove_size = %u\n",
		  offset, remove_size));

	if (remove_size == 0
	    && f->range_tail
	    && f->range_tail->data_offset + f->range_tail->data_size
	       == offset) {
		/* A simple append; nothing to remove or no node to split.  */
		return 0;
	}

	/* Find the node where we should begin the removal.  */
	for (n = f->range_head; n; n = n->range_next) {
		if (n->data_offset + n->data_size > offset) {
			break;
		}
	}
	if (!n) {
		/* If there's no data in the file there's no data to
		   remove either.  */
		return 0;
	}

	if (n->data_offset > offset) {
		/* XXX: Not implemented yet.  */
		printk(KERN_WARNING "JFFS: An unexpected situation "
		       "occurred in jffs_delete_data.\n");
	}
	else if (n->data_offset < offset) {
		/* See if the node has to be split into two parts.  */
		if (n->data_offset + n->data_size > offset + remove_size) {
			/* Do the split.
			* A split is really: copy the first chunk to a new node, then
			* adjust the pointers in the original node so as to make that
			* chunk (in the original node) invisible.  When the orig node gets
			* hit by GC, then that invisible part will disappear.  Until then, the
			* data exists twice, once in the new node and once in the orig node.
			* The invisble chunk is "wasted".  It's dirt that isn't marked as dirt.
			*/
			struct jffs_node *new_node;
			D3(printk("jffs_delete_data: Split node with "
				  "version number %u.\n", n->version));

			if (!(new_node = jffs_alloc_node())) {
				D(printk("jffs_delete_data: -ENOMEM\n"));
				return -ENOMEM;
			}
			DJM(no_jffs_node++);

			new_node->ino = n->ino;
			new_node->version = n->version;
			new_node->data_offset = offset;
			new_node->data_size = n->data_size - (remove_size + (offset - n->data_offset));
			new_node->fm_offset = n->fm_offset + (remove_size + (offset - n->data_offset));
			new_node->name_size = n->name_size;
			new_node->fm = n->fm;
			new_node->version_prev = n;
			new_node->version_next = n->version_next;
			if (new_node->version_next) {
				new_node->version_next->version_prev
				= new_node;
			}
			else {
				f->version_tail = new_node;
			}
			n->version_next = new_node;
			new_node->range_prev = n;
			new_node->range_next = n->range_next;
			if (new_node->range_next) {
				new_node->range_next->range_prev = new_node;
			}
			else {
				f->range_tail = new_node;
			}
			/* A very interesting can of worms.  */
			n->range_next = new_node;
			n->data_size = offset - n->data_offset;
			if (new_node->fm)
				jffs_add_node(new_node);
			else {
				D1(printk(KERN_WARNING "jffs_delete_data: Splitting an empty node (file hold).\n!"));
				D1(printk(KERN_WARNING "FIXME: Did dwmw2 do the right thing here?\n"));
			}
			n = new_node->range_next;
			remove_size = 0;
		}
		else {
			/* No.  No need to split the node.  Just remove
			   the end of the node.  */
			int r = min(n->data_offset + n->data_size
					 - offset, remove_size);
			n->data_size -= r;
			remove_size -= r;
			n = n->range_next;
			/* This makes wasted, but is very rare to have happen.  I think. */
		}
	}

	/* Remove as many nodes as necessary.  */
	while (n && remove_size) {
		if (n->data_size <= remove_size) {
			struct jffs_node *p = n;
			remove_size -= n->data_size;
			n = n->range_next;
			D3(printk("jffs_delete_data : Removing node: "
				  "ino: %u, version: %04x%s\n",
				  p->ino, p->version,
				  (p->fm ? "" : " (virtual)")));
			if (p->fm) {
				jffs_fmfree(f->c->fmc, p->fm, p);
			}
			jffs_unlink_node_from_range_list(f, p);
			jffs_unlink_node_from_version_list(f, p);
			jffs_free_node(p);
			DJM(no_jffs_node--);
		}
		else {
			n->data_size -= remove_size;
			n->fm_offset += remove_size;
			n->data_offset -= (node->removed_size - remove_size);
			f->c->fmc->wasted_size += remove_size;
			n = n->range_next;
			break;
		}
	}

	/* Adjust the following nodes' information about offsets etc.  */
	while (n && node->removed_size) {
		n->data_offset -= node->removed_size;
		n = n->range_next;
	}

	if (node->removed_size > (f->size - node->data_offset)) {
		/* It's possible that the removed_size is in fact
		 * greater than the amount of data we actually thought
		 * were present in the first place - some of the nodes 
		 * which this node originally obsoleted may already have
		 * been deleted from the flash by subsequent garbage 
		 * collection.
		 *
		 * If this is the case, don't let f->size go negative.
		 * Bad things would happen :)
		 */
		f->size = node->data_offset;
	} else {
		f->size -= node->removed_size;
	}
	D3(printk("jffs_delete_data: f->size = %d\n", f->size));
	return 0;
} /* jffs_delete_data()  */


/* Insert some data into a file.  Prior to the call to this function,
   jffs_delete_data should be called.  */
static int jffs_insert_data(struct jffs_file *f, struct jffs_node *node)
{
	D3(printk("jffs_insert_data: node->data_offset = %u, "
		  "node->data_size = %u, f->size = %u\n",
		  node->data_offset, node->data_size, f->size));

	/* Find the position where we should insert data.  */
	retry:
	if (node->data_offset == f->size) {
		/* A simple append.  This is the most common operation.  */
		node->range_next = NULL;
		node->range_prev = f->range_tail;
		if (node->range_prev) {
			node->range_prev->range_next = node;
		}
		f->range_tail = node;
		f->size += node->data_size;
		if (!f->range_head) {
			f->range_head = node;
		}
	}
	else if (node->data_offset < f->size) {
		/* Trying to insert data into the middle of the file.  This
		   means no problem because jffs_delete_data() has already
		   prepared the range list for us.  */
		struct jffs_node *n;

		/* Find the correct place for the insertion and then insert
		   the node.  */
		for (n = f->range_head; n; n = n->range_next) {
			D3(printk("Cool stuff's happening!\n"));

			if (n->data_offset == node->data_offset) {
				node->range_prev = n->range_prev;
				if (node->range_prev) {
					node->range_prev->range_next = node;
				}
				else {
					f->range_head = node;
				}
				node->range_next = n;
				n->range_prev = node;
				break;
			}
			ASSERT(else if (n->data_offset + n->data_size >
					node->data_offset) {
				printk(KERN_ERR "jffs_insert_data: "
				       "Couldn't find a place to insert "
				       "the data!\n");
				return -1;
			});
		}

		/* Adjust later nodes' offsets etc.  */
		n = node->range_next;
		while (n) {
			n->data_offset += node->data_size;
			n = n->range_next;
		}
		f->size += node->data_size;
	}
	else if (node->data_offset > f->size) {
		/* Okay.  This is tricky.  This means that we want to insert
		   data at a place that is beyond the limits of the file as
		   it is constructed right now.  This is actually a common
		   event that for instance could occur during the mounting
		   of the file system if a large file have been truncated,
		   rewritten and then only partially garbage collected.  */

		struct jffs_node *n;

		/* We need a place holder for the data that is missing in
		   front of this insertion.  This "virtual node" will not
		   be associated with any space on the flash device.  */
		struct jffs_node *virtual_node;
		if (!(virtual_node = jffs_alloc_node())) {
			return -ENOMEM;
		}

		D(printk("jffs_insert_data: Inserting a virtual node.\n"));
		D(printk("  node->data_offset = %u\n", node->data_offset));
		D(printk("  f->size = %u\n", f->size));

		virtual_node->ino = node->ino;
		virtual_node->version = node->version;
		virtual_node->removed_size = 0;
		virtual_node->fm_offset = 0;
		virtual_node->name_size = 0;
		virtual_node->fm = NULL; /* This is a virtual data holder.  */
		virtual_node->version_prev = NULL;
		virtual_node->version_next = NULL;
		virtual_node->range_next = NULL;

		/* Are there any data at all in the file yet?  */
		if (f->range_head) {
			virtual_node->data_offset
			= f->range_tail->data_offset
			  + f->range_tail->data_size;
			virtual_node->data_size
			= node->data_offset - virtual_node->data_offset;
			virtual_node->range_prev = f->range_tail;
			f->range_tail->range_next = virtual_node;
		}
		else {
			virtual_node->data_offset = 0;
			virtual_node->data_size = node->data_offset;
			virtual_node->range_prev = NULL;
			f->range_head = virtual_node;
		}

		f->range_tail = virtual_node;
		f->size += virtual_node->data_size;

		/* Insert this virtual node in the version list as well.  */
		for (n = f->version_head; n ; n = n->version_next) {
			if (n->version == virtual_node->version) {
				virtual_node->version_prev = n->version_prev;
				n->version_prev = virtual_node;
				if (virtual_node->version_prev) {
					virtual_node->version_prev
					->version_next = virtual_node;
				}
				else {
					f->version_head = virtual_node;
				}
				virtual_node->version_next = n;
				break;
			}
		}

		D(jffs_print_node(virtual_node));

		/* Make a new try to insert the node.  */
		goto retry;
	}

	D3(printk("jffs_insert_data: f->size = %d\n", f->size));
	return 0;
}


/* A new node (with data) has been added to the file and now the range
   list has to be modified.  */
static int jffs_update_file(struct jffs_file *f, struct jffs_node *node)
{
	int err;

	D3(printk("jffs_update_file: ino: %u, version: %04x\n",
		  f->ino, node->version));

	if (node->data_size == 0) {
		if (node->removed_size == 0) {
			/* data_offset == X  */
			/* data_size == 0  */
			/* remove_size == 0  */
		}
		else {
			/* data_offset == X  */
			/* data_size == 0  */
			/* remove_size != 0  */
			if ((err = jffs_delete_data(f, node)) < 0)
				return err;
			/* This node has no data, so it's really an obsolete
			   (empty) dirty node. Its purpose was to do the delete_data.
			   _redundent_ will get rid of it. */
		}
	}
	else {
		/* data_offset == X  */
		/* data_size != 0  */
		/* remove_size == Y  */
		if ((err = jffs_delete_data(f, node)) < 0)
			return err;
		if ((err = jffs_insert_data(f, node)) < 0)
			return err;
	}
	return 0;
}

#if 0
/* Print the contents of a file.  */
int
jffs_print_file(struct jffs_file *f)
{
	D(int i);
	D(printk("jffs_file: 0x%p\n", f));
	D(printk("{\n"));
	D(printk("        0x%08x, /* ino  */\n", f->ino));
	D(printk("        0x%08x, /* pino  */\n", f->pino));
	D(printk("        0x%08x, /* mode  */\n", f->mode));
	D(printk("        0x%04x,     /* uid  */\n", f->uid));
	D(printk("        0x%04x,     /* gid  */\n", f->gid));
	D(printk("        0x%08x, /* atime  */\n", f->atime));
	D(printk("        0x%08x, /* mtime  */\n", f->mtime));
	D(printk("        0x%08x, /* ctime  */\n", f->ctime));
	D(printk("        0x%02x,       /* nsize  */\n", f->nsize));
	D(printk("        0x%02x,       /* nlink  */\n", f->nlink));
	D(printk("        0x%02x,       /* deleted  */\n", f->deleted));
	D(printk("        \"%s\", ", (f->name ? f->name : "")));
	D(for (i = strlen(f->name ? f->name : ""); i < 8; ++i) {
		printk(" ");
	});
	D(printk("/* name  */\n"));
	D(printk("        0x%08x, /* size  */\n", f->size));
	D(printk("        0x%08x, /* highest_version  */\n",
		 f->highest_version));
	D(printk("        0x%p, /* c  */\n", f->c));
	D(printk("        0x%p, /* parent  */\n", f->parent));
	D(printk("        0x%p, /* children  */\n", f->children));
	D(printk("        0x%p, /* sibling_prev  */\n", f->sibling_prev));
	D(printk("        0x%p, /* sibling_next  */\n", f->sibling_next));
	D(printk("        0x%p, /* hash_prev  */\n", f->hash.prev));
	D(printk("        0x%p, /* hash_next  */\n", f->hash.next));
	D(printk("        0x%p, /* range_head  */\n", f->range_head));
	D(printk("        0x%p, /* range_tail  */\n", f->range_tail));
	D(printk("        0x%p, /* version_head  */\n", f->version_head));
	D(printk("        0x%p, /* version_tail  */\n", f->version_tail));
	D(printk("}\n"));
	return 0;
}
#endif  /*  0  */


int
jffs_print_file_nodes(struct jffs_file *f)
{
	struct jffs_node *node;
	struct jffs_node *n;

	printk("File: ino #%u, \"%s\"\n",
		  f->ino, (f->name ? f->name : ""));
	node = f->version_head;
	while (node) {
		n = node;
		node = node->version_next;
		printk("    ver: %4x  dat: %4x  siz: %4x  rmv: %4x  fmofs: %4x fm: %5x fmsiz: %4x\n",
		       n->version, n->data_offset, n->data_size, n->removed_size, n->fm_offset,
		       (n->fm ? n->fm->offset : 0), (n->fm ? n->fm->size : 0));
	}
	return(0);
}


#if defined(JFFS_MEMORY_DEBUG) && JFFS_MEMORY_DEBUG
void
jffs_print_memory_allocation_statistics(void)
{
	static long printout;
	printk("________ Memory printout #%ld ________\n", ++printout);
	printk("no_jffs_file = %ld\n", no_jffs_file);
	printk("no_jffs_node = %ld\n", no_jffs_node);
	printk("no_jffs_control = %ld\n", no_jffs_control);
	printk("no_jffs_raw_inode = %ld\n", no_jffs_raw_inode);
	printk("no_jffs_node_ref = %ld\n", no_jffs_node_ref);
	printk("no_jffs_fm = %ld\n", no_jffs_fm);
	printk("no_jffs_fmcontrol = %ld\n", no_jffs_fmcontrol);
	printk("no_hash = %ld\n", no_hash);
	printk("no_name = %ld\n", no_name);
	printk("\n");
}
#endif

