/*
 * JFFS -- Journaling Flash File System, Linux implementation.
 *
 *
 */
/******************** PROBLEMS!!! *******************

- Code compares vs. dirty_size, not free+dirty, to decide if doing GC
  will help.  (fmc->dirty_size < fmc->sector_size)
  Free blocks:  2   Free+dirty blocks: 3  <------- should do GC.

- I think the minimum free size is 2 EBs (see below) in the normal
  case.  this. Must test!!

  But on a file rename, it might be more, since all the file's data
  nodes must be re-written larger.  Maybe we don't much care, since
  this is a router---we'll just warn people to not do that.

- Worst case necessary # of free blocks is: 1 for a full head block +
  1/2 for any nodes that start on the head block but span into the
  next block.  Round up: 2.

  HOWEVER: if we either dis-allow a node to span blocks,
  then we don't need this extra free block.  Maybe.
  NOTE that this is essentially what we do when we're
  re-writing a node when the new size would be > max_chunk_size.  I
  think that at the end of flash, we bypass (make dirty) a chunk of
  flash that we deem to be "too small to be useful".  That is
  certainly what we do on a write_node.  If we split a spanning node
  during a GC, we could allow the initial write to span, thus leaving
  that code alone.
  This would have to be carefully tested!!!

 
- Make special case code for very small filesystem size, to allow for
  1 or even 0 reserved EBs.  This may be an in-place write, which is
  slow and not rebust.  A power failure during the erase/write will
  corrupt the fs.

- Maybe can keep GC'ing and get it so used_head is < free_tail, and
  free up a block.  Unknown if this is possible without actually
  trying to GC a few times.

- GC thread should run periodically, to change dirty into free during
  idle periods.  When things are *very* idle, it should GC until all
  the dirty is together in the head block.  This will give us a head
  start the next time a GC is required.  Thinking about: do a timer in
  gc_thread.  Exponential increase up to maybe 10-20 minutes.  Gets
  set back to 100 msec (??  One second?)  whenever main code decides
  it should trigger GC_thread.  At the max timeout, that's when we
  should aggressively GC to combine all the dirty into one.  That's
  when the head node is dirty and its size is dirty_size.

  All this is merely an optimization.  As long as anything gets
  written to the jffs, then blocks will be dirtied and the GC will
  have to be done at some time anyway.  This just gets the GC done
  sooner, hopefully before it is required.  If required, then a write
  will be delayed while the GC is done to make room for the data being
  written.  This delay is about 1 second for each flash block that
  must be erased.  Far better to do the GC's before they are required!

  GC_trigger can then be sloppy.  It would no longer be so important
  to wake the GC thread, because we know that a GC pass will happen
  eventually.  Heck, we could always trigger when we write.

============================================

- *FIXED When GC moves a file off of the head page, it copies the
whole file, even those parts that are on a different page.  Ver gets
bumped.  Nodes get merged--up to 32KB max,

- *FIXED GC actually does: from the file_offset in the 1st (oldest)
node, copy the next N bytes of the file to the tail.  Wrong thing to
do.  N is capped at: max_chunk_size (32kb), or end-of-flash, or
end-of-file.

a) It copies data that might be on a different block, thereby creating
deeply buried dirty nodes.

b) If there is another node in the headpage with a smaller
file-offset, we don't handle that now.  If the file was written
backward, it might be a long time before these get merged.  If they do
get merged in this pass, the earlier node (with the larger file
offset) will get copied twice, thereby creating an unnecessary dirty
node.  Once in it's own right, and once more when it gets merged.

c) If there is a hole in the file, all those empty bytes will be
written out (as zero).  This will create busy node(s) that didn't
previously exist.  (This is a node with node->fm == NULL).

We *do* want to merge, because that deletes a node header.  As we
re-write nodes, one occasionally gets split because it hits a boundary
(like end-of-flash).  If we don't ever merge, we'll keep splitting
blocks and hence adding node-headers forever.

What it *should* do is: Get the file to which the oldest node belongs,
then skip backwards in nodes to find the node for that file which is
on the head block with the smallest file_offset.  Maybe it's this
node, maybe it's a later (physical) node. Doesn't matter---we'll still
be moving a node off the nead block.

Starting from that node, find the N that also is capped/constrained by
a node whose flash_offset is on the head block.  Worst case is that
there are no other such nodes, so no merge happens.  Best case is that
we hit one of the other caps while we are still on the head block.
This is what actually is the case when a file is written normally,
from 0 to EOF.

Also, stop on a file node that represents a hole (fm == 0).  We don't
want to expand out the 0 bytes that are a hole.

If we want to get frisky, we could allow up to N bytes from node(s)
that are on the next M flash block(s).  Probably want to limit M to 1,
and N to only entire node(s) up to PAGESIZE (4kb).  But remember that
doing this will create some dirt (or not a dirty node but obsoleted
data) deeper in the flash, which will *not* be erased in this GC pass.
So maybe we'd only do this if we had plenty of unused free space and
[something].  Maybe we'd do this only if we didn't merge any nodes that
are on the head block *and* the next node is on the next physical block.
Someday, perhaps.....


- *FIXED If you GC when there are no used nodes, and free wraps around,
the next allocs go to 0 (free2), instead of free1.
*FIXED.  It did not reset tail = head.

 ******************** PROBLEMS!!! [end] ********************/

/* This file contains the garbage collection code for the
   Journaling Flash File System, JFFS.  */

/*
  GC works by moving busy nodes from the head to the tail, thereby making
  the nodes in the head block dirty.  When the entire head block is dirty, the
  block is erased and added to free.

  Conditions:

  1) Used_on_head (including any nodes that span into the
  next block or other blocks) is <= free space on the tail block.
    -- # of free blocks needed: 0

  2) Used_on_head is > free_on_tail.  But is small.
  -- # of free blocks needed: 1

  3) Used_on_head is > free_on_tail and is large.  Used could be as
  much as one entire block + up to 32KB that spans into next block.
  Only 32KB because max node size is 32kb.
  -- # of free blocks needed: 2

  4) Rename file to longer name.  Must re-write every node, making
  each one larger.

  5) When copying nodes to tail, merge adjacent nodes with adjacent
  data.  Up to 32kb (max node size).  Or up to end of flash.

  6) When copying node to tail, it might run into the end of flash,
  when free wraps around to offset 0.  The node must be split -OR- we
  dirty out the remainder of last flash block and put the entire node
  at the satrt of flash.
    -- # of free blocks needed: 1 extra ?
  
  
  
 */


#define __NO_VERSION__
#include <linux/config.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/jffs.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/pagemap.h>
#include <asm/semaphore.h>
#include <asm/byteorder.h>
#include <linux/version.h>
#include <linux/smp_lock.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/ctype.h>
#include <linux/namei.h>

#include "intrep.h"
#include "jffs_fm.h"


   
#define MAX_GC_SLEEP (1024 * HZ)	/* Maximum GC sleep time.  About 17 minutes. */
#define INIT_GC_SLEEP (1 * HZ)		/* Initial GC sleep time. */
/* When GC has been idle for at least this long, it will force (allow) nodes to merge
 * even when not on same block.
 * Wait until the 3rd GC after a write to the fs (8 seconds). */
#define THRESH_MERGE_OFB_NODES (INIT_GC_SLEEP << 3)
   
/* The first few rounds, be lazy. (16 seconds) */
#define THRESH_LAZY_GC (INIT_GC_SLEEP << 4)

/* The first few rounds, be lazy about dirty nodes at the beginning of
 * the head block. After that, we want to merge them into one large
 * dirty node, then GC until all dirt is freed. (128 seconds) */
#define THRESH_LAZY_DIRT (INIT_GC_SLEEP << 7)





/******************************************************************/
/* The space left is too small to be of any use, so make it dirty.
* From 1st free to end of block/flash. */
int dirty_to_eob(struct jffs_control *c, __u32 size)
{
   struct jffs_fmcontrol *fmc = c->fmc;
   struct jffs_fm *dirty_fm;
   
   /* The space left is too small to be of any use really.  */
   dirty_fm = jffs_fmalloced(fmc,
			     fmc->tail->offset + fmc->tail->size,
			     size, NULL);
   if (!dirty_fm) {
      printk(KERN_ERR "JFFS: "
	     "jffs_garbage_collect_next: "
	     "Failed to allocate `dirty' "
	     "flash memory!\n");
      return(-1);
   }
   D1(printk("Dirtying end of flash - too small: %5x  %5x\n",
	     fmc->tail->offset, size));
   jffs_write_dummy_node(c, dirty_fm);
   return(1);
}



/* Rewrite `size' bytes, and begin at `node'.  */
/* Return: 0 if success.   <0 (NZ?) if error. */
int jffs_rewrite_data(struct jffs_file *f, struct jffs_node *node, __u32 size)
{
   struct jffs_control *c = f->c;
   struct jffs_fmcontrol *fmc = c->fmc;
   struct jffs_raw_inode raw_inode;
   struct jffs_node *new_node;
   struct jffs_fm *fm;
   __u32 pos;
   __u32 pos_dchksum;
   __u32 total_name_size;
   __u32 total_data_size;
   __u32 total_size;
   int err;
   __u32 sum = ~0;

   D1(printk("\n***jffs_rewrite_data: node: %u, name: \"%s\"  dsize: %4x   from: %5x\n",
	      f->ino, (f->name ? f->name : "(null)"), size, node->fm->offset));
   /* Create and initialize the new node.  */
   if (!(new_node = jffs_alloc_node())) {
      D(printk("jffs_rewrite_data: "
	       "Failed to allocate node.\n"));
      return -ENOMEM;
   }
   DJM(no_jffs_node++);
   new_node->data_offset = node->data_offset;
   new_node->removed_size = size;
   total_name_size = JFFS_PAD(f->nsize);
   total_data_size = JFFS_PAD(size);
   total_size = sizeof(struct jffs_raw_inode)
      + total_name_size + total_data_size;
   new_node->fm_offset = sizeof(struct jffs_raw_inode)
      + total_name_size;

retry:
   err = 0;
   /* If source & dest would be in the same block, we don't want that.  We will just have
    * to gc it repeatedly until it finally moves to another block.
    * So alloc the remaining space in this block as dirt. */
   if ((node->fm->offset / fmc->sector_size) * fmc->sector_size ==
       ((fmc->tail->offset + fmc->tail->size) / fmc->sector_size) * fmc->sector_size) {
      dirty_to_eob(c, fmc->sector_size - ((fmc->tail->offset + fmc->tail->size) % fmc->sector_size));
   }
   
   if ((err = jffs_fmalloc(fmc, total_size, new_node, &fm)) < 0) {
      DJM(no_jffs_node--);
      D(printk("jffs_rewrite_data: Failed to allocate fm.\n"));
      jffs_free_node(new_node);
      return err;
   }
   else if (!fm->nodes) {
      /* The jffs_fm struct that we got is not big enough.  */
      /* This should never happen, because we deal with this case
	 in jffs_garbage_collect_next().*/
      printk(KERN_WARNING "jffs_rewrite_data: Allocated node is too small (%d bytes of %d)\n", fm->size, total_size);
      if ((err = jffs_write_dummy_node(c, fm)) < 0) {
	 kfree(fm);
	 DJM(no_jffs_fm--);
	 D(printk("jffs_rewrite_data: "
		  "jffs_write_dummy_node() Failed!\n"));
	 return err;
      }
      goto retry;
   }
   new_node->fm = fm;

   /* Initialize the raw inode.  */
   raw_inode.magic = JFFS_MAGIC_BITMASK;
   raw_inode.ino = f->ino;
   raw_inode.pino = f->pino;
   raw_inode.version = f->highest_version + 1;
   raw_inode.mode = f->mode;
   raw_inode.uid = f->uid;
   raw_inode.gid = f->gid;
   raw_inode.atime = f->atime;
   raw_inode.mtime = f->mtime;
   raw_inode.ctime = f->ctime;
   raw_inode.offset = node->data_offset;
   raw_inode.dsize = size;
   raw_inode.rsize = size;
   raw_inode.nsize = f->nsize;
   raw_inode.nlink = f->nlink;
   raw_inode.spare = 0;
   raw_inode.rename = 0;
   raw_inode.deleted = f->deleted;
   raw_inode.accurate = 0xff;

   pos = new_node->fm->offset;
   pos_dchksum = pos + offsetof(struct jffs_raw_inode, dchksum);

   D3(printk("jffs_rewrite_data: Writing this raw inode "
	     "to pos %5x.\n", pos));
   D3(jffs_print_raw_inode(&raw_inode));

   /* Write the inode, all but the checksums. */
   if ((err = flash_safe_write(fmc, pos,
			       (u_char *) &raw_inode,
			       offsetof(struct jffs_raw_inode, dchksum))) < 0) {
      jffs_fmfree_partly(fmc, fm,
			 total_name_size + total_data_size);
      printk(KERN_ERR "JFFS: jffs_rewrite_data: Write error during "
	     "rewrite. (raw inode)\n");
      printk(KERN_ERR "JFFS: jffs_rewrite_data: Now retrying "
	     "rewrite. (raw inode)\n");
      goto retry;
   }
   pos += sizeof(struct jffs_raw_inode);

   /* Write the name to the flash memory.  */
   if (f->nsize) {
      D3(printk("jffs_rewrite_data: Writing name \"%s\" to "
		"pos 0x%ul.\n", f->name, (unsigned int) pos));
      if ((err = flash_safe_write(fmc, pos,
				  (u_char *)f->name,
				  f->nsize)) < 0) {
	 jffs_fmfree_partly(fmc, fm, total_data_size);
	 printk(KERN_ERR "JFFS: jffs_rewrite_data: Write "
		"error during rewrite. (name)\n");
	 printk(KERN_ERR "JFFS: jffs_rewrite_data: Now retrying "
		"rewrite. (name)\n");
	 goto retry;
      }
      pos += total_name_size;
      raw_inode.nchksum = jffs_checksum_16(f->name, f->nsize);
   }

   /* Write the data.  */
   if (size) {
      int r;
      unsigned char *page;
      __u32 offset = node->data_offset;

      D2(printk("jffs_rewrite_data: '%s'; ofs %6x - %6x for %04x bytes.\n", f->name,  offset, offset + size, size));
      if (!(page = (unsigned char *)__get_free_page(GFP_KERNEL))) {
	 jffs_fmfree_partly(fmc, fm, 0);
	 return -1;
      }

      while (size) {
	 __u32 s = min(size, (__u32)PAGE_SIZE);
	 if ((r = jffs_read_data(f, (char *)page,
				 offset, s)) < s) {
	    free_page((unsigned long)page);
	    jffs_fmfree_partly(fmc, fm, 0);
	    printk(KERN_ERR "JFFS: jffs_rewrite_data: "
		   "jffs_read_data() "
		   "failed! (r = %d)\n", r);
	    return -1;
	 }
	 if ((err = flash_safe_write(fmc,
				     pos, page, r)) < 0) {
	    free_page((unsigned long)page);
	    jffs_fmfree_partly(fmc, fm, 0);
	    printk(KERN_ERR "JFFS: jffs_rewrite_data: "
		   "Write error during rewrite. "
		   "(data)\n");
	    goto retry;
	 }
	 pos += r;
	 size -= r;
	 offset += r;
	 sum = crc_32(page, r, sum);
      }
      free_page((unsigned long)page);
   }

   raw_inode.dchksum = sum;
   raw_inode.ichksum = jffs_checksum_16(&raw_inode, INODE_CHK_SIZ);

   /* Write the checksums.  */
   if ((err
	= flash_safe_write(fmc, pos_dchksum,
			   &((u_char *)&raw_inode)[offsetof(struct jffs_raw_inode, dchksum)],
			   sizeof(__u32) + sizeof(__u16) + sizeof(__u16))) < 0) {
      jffs_fmfree_partly(fmc, fm, 0);
      printk(KERN_ERR "JFFS: jffs_rewrite_data: Write error during "
	     "rewrite. (checksum)\n");
      goto retry;
   }

   /* Now make the file system aware of the newly written node.  */
   jffs_insert_node(c, f, &raw_inode, f->name, new_node);

   D3(printk("jffs_rewrite_data: Leaving...\n"));
   return 0;
} /* jffs_rewrite_data()  */







/******************************************************************/
/* jffs_garbage_collect_next implements one step in the garbage collect
   process and is often called multiple times at each occasion of a
   garbage collect.

   This will rotate nodes of the first file in the flash to the end,
   thereby getting them off the head block.  After all the nodes have
   been moved off the head block, it will be all dirty and can be
   erased and added to free.

   Depending on alignment & fragmentation, even if dirty is less than
   blocksize, we might be able to free a block.  If free and
   free+dirty are the same # of blocks, we can't.  But if free+dirty
   is more blocks than free, we can.

   If we only re-write data that is on the head block, we know that
   worst-case we won't write more than one block.  If a node spans
   into the next block (which we don't allow anymore) then the
   worst-case is one block PLUS the max node size (for the case where
   only 1 byte is on this block and the rest has spanned into the next
   block) or 1 + 1/2 block.  This would make the minimum reserve to be
   2 blocks.  If we don't allow span, the worst case is 1 block.

   This means that sometimes when we re-write data, we'll have to
   split a node that otherwise would span into the next block.  During
   GC, e don't (normally) copy data that is on another block, since
   that results in a dirty node that is on another block, thereby
   consuming flash that we can't free up yet.  However, this means
   that we will split one node into two nodes (on different blocks),
   but will never merge them, since they'll always be on different
   blocks.

   Hence, "merge_obn" which tells us how aggressive we should be with
   merging in nodes that are on a different block than the head block.
   If zero, this is an emergency GC to make room for a new node that
   is being written to the fs.  In this case, we cannot afford to
   created extra dirt by copying data from another block.
   
   The other cases are:
   a) include *one* node that is on the *next*
   physical block, regardless of size.  This would pick up the case
   where a node got split due to spanning a block.
   b) include a small node regardless of where it is.  This will
   slowly merge small nodes.  If there is a case where there are small nodes.

   Each of these will create dirt and therefore extra flash.  In (a),
   we'll collect that dirt on the next GC, so it won't be around very
   long.  In (b), the dirt may be far away and therefore live for a
   long time.  But it's small, so shouldn't hurt much.
 

   *** Must disallow span both here in GC and in original node write! 
   *** ***
*/
/* Return: <0 if error.   0 if didn't do anything.
   >0 if we did something to progress.
 */
int jffs_garbage_collect_next(struct jffs_control *c, int merge_obn)
{
   struct jffs_fmcontrol *fmc = c->fmc;
   struct jffs_file *f;
   int err = 0;
   __u32 total_name_size;
   __u32 avail_free;
   struct jffs_node *n, *nn;
   __u32 this_sector, next_sector;	/* Addr of this block & next (ignoring wraparound to zero) */
   __u32 nxt_log_sec;			/* Addr of next block (taking wraparound into account.) */
   /* These define the data that we are going to move from the head to
    * the tail.  The size initially is from the node to EOF.  But:
    * 1) limit it to max-chunk_size (1/2 EB; 32kb).
    * 2) limit further to be only those nodes that are on (or start on)
    *    this (the head) block.
    * 3) limit further to freesize1----the freesize from the tail to
    *    the end of the flash.  We can't span to the start of the flash.
    *    The remainder will get picked up at the next GC_next pass.
    * 3!) The original node that was split gets modified to reflect the
    *     amount of leading data that *did* get moved.  Offset gets incremented,
    *     size gets decremented.
    *     jffs_delete_data / jffs_update_file / jffs_insert_node /
    *	  jffs_rewrite_data
    * 3!!) But the [leading] space is wasted and won't get recovered until
    *      we GC that node.
    * 3a) In this case, if the available chunk won't hold at least 1K of data,
    *     deem it to be uselessly small and discard it.
    * 4) limit further to ?????
    * 5) Optimization: instead of limit to freesize1, limit to free-on-tail.
    *    This means that nodes will NEVER span a block, and this should help
    *    in small filessystems (by reducing the required reserve).  In this
    *    case, the 1K size (above) may be too big--maybe use 256 or 512 instead.
    * 6) Optimization: When we split a node (because it crossed end-of-flash or
    *    end-of-block), do an actual split, don't write the 1st part twice.
    *
    */
   struct jffs_node *node;	/* Starting point. */
   __u32 data_size;		/* The number of bytes to move (file data). */
   __u32 size;			/* data_size plus raw-inode + filename + pad */

   
   D1(printk(KERN_NOTICE "jffs_garbage_collect_next: Enter...\n"));
   D1(printk(KERN_NOTICE "Free blocks:  %u   Free+dirty blocks: %u\n",
	  c->fmc->free_size / c->fmc->sector_size,
	  (c->fmc->free_size + c->fmc->dirty_size) / c->fmc->sector_size));

   /* Get the oldest node in the flash.  */
   node = jffs_get_oldest_node(fmc);
   if (!node) {
      /* Hopefully the only nodes are dirty nodes. */
      if (c->fmc->used_size != 0) {
	 printk(KERN_ERR "JFFS: jffs_garbage_collect_next: "
		"No oldest node found!\n");
	 err = -1;
      }
      goto jffs_garbage_collect_next_end;
   }

   /* Find its corresponding file too.  */
   f = jffs_find_file(c, node->ino);

   if (!f) {
      printk (KERN_ERR "JFFS: jffs_garbage_collect_next: "
	      "No file to garbage collect! "
	      "(ino = 0x%08x)\n", node->ino);
      err = -1;
      goto jffs_garbage_collect_next_end;
   }

   /* We always write out the name. Theoretically, we don't need
      to, but for now it's easier - because otherwise we'd have
      to keep track of how many times the current name exists on
      the flash and make sure it never reaches zero.

      The current approach means that would be possible to cause
      the GC to end up eating its tail by writing lots of nodes
      with no name for it to garbage-collect. Hence the change in
      inode.c to write names with _every_ node.

      It sucks, but it _should_ work.
   */
   total_name_size = JFFS_PAD(f->nsize);

   D1(printk(KERN_NOTICE "jffs_garbage_collect_next: \"%s\", "
	     "ino: %u, version: %04x, fm %5x\n",
	     (f->name ? f->name : ""), node->ino, node->version, 
	     node->fm->offset));

/* This is TRUE if the fm is non-null, and the fm's data
 * starts on the current flash block. */  
#define DATA_IS_ONPAGE(fm) ((fm) && ((fm)->offset >= this_sector && (fm)->offset < next_sector))
   
   this_sector = (node->fm->offset / fmc->sector_size) * fmc->sector_size;
   next_sector = this_sector + fmc->sector_size;
   /* In both these optimizations, stop at a hole in the file.  That's
      represented by a node with no fm. */
   /* In the case where nodes are in reverse physical order, we'd like
      to move back in the file (to nodes later on this flash page) to
      merge them.
      This could happen due to previous GC's or by the file being written
      in reverse order. */
   /* Walk back only, but don't walk off this block. */
   for (n = node; n; n = n->range_prev) {
      if (!(DATA_IS_ONPAGE(n->fm)))
	    break;	
      node = n;
   }
   //printk("Walked back to node: %8p   fm_offset: %8x\n", node, node->fm? node->fm->offset: 0xdeaddead);
   
   /* Compute how many data it's possible to rewrite at the moment.  */
   data_size = f->size - node->data_offset;	/* To the EOF. */
   //printk(" size to EOF: %u\n", data_size);
   
   /* Find the size of all the nodes of this file that are on this flash block.
    * Walk down the range_next chain.  Stop at the last node or a hole or
    * a node whose data is not on this flash block. */
   for (n = node; n; n = n->range_next) {
      //printk("Checking node %p  with fm: %p with fm->offset: %p\n", n, n->fm,  n->fm? n->fm->offset: 0xdeaddead);
      if (!(DATA_IS_ONPAGE(n->fm))) {
	 if (n->fm != NULL && merge_obn != 0) {
	    /* Check next node (which is on other block) for small size or being
	     * in the very next block.  But only if there is plenty of free space.  */
	    if ((nn = n->range_next) != NULL  &&
			 fmc->free_size >  fmc->min_free_size + nn->fm->size) {
	       nxt_log_sec = (next_sector < fmc->flash_size) ? next_sector : 0;
	       if (nn->fm->size <= 1024 ||
		   (nn->fm->offset >= nxt_log_sec && nn->fm->offset < nxt_log_sec + fmc->sector_size)) {
		  n = nn;
	       }
	    }
	 }
	 if (f->size > n->data_offset)	/* For safety! Should always be true.*/
	    data_size = n->data_offset - node->data_offset;
	 break;
      }
   }
   //printk(" size to quit: %u\n", data_size);

   /* And from that, the total size of the chunk we want to write */
   size = sizeof(struct jffs_raw_inode) + total_name_size
      + data_size + JFFS_GET_PAD_BYTES(data_size);

   /* If that's more than max_chunk_size, reduce it accordingly */
   if (size > fmc->max_chunk_size) {
      size = fmc->max_chunk_size;
      data_size = size - sizeof(struct jffs_raw_inode)
	 - total_name_size;
   }

   /* If we're asking to take up more space than free_chunk_size1
      but we _could_ fit in it, shrink accordingly.
      Note: BLOCK_SIZE is 1K.
   */
   /* Don't span into the next block. */
   avail_free = fmc->sector_size - ((fmc->tail->offset + fmc->tail->size) % fmc->sector_size);
   //avail_free = free_chunk_size1;	/* Allow block spanning, but chop off at the end-of-flash. */
   if (size > avail_free) {
      if (avail_free <
	  (sizeof(struct jffs_raw_inode) + total_name_size + MIN_USEFUL_DATA_SIZE)) {
	 /* The space left is too small to be of any use really.  */
	 err = dirty_to_eob(c, avail_free);
	 goto jffs_garbage_collect_next_end;
      }
      D1(printk("Reducing size of new node from %5d to %5d to avoid "
		 "spanning block at %5x\n", size, avail_free, fmc->tail->offset + fmc->tail->size));
      size = avail_free;
      data_size = size - sizeof(struct jffs_raw_inode)
	 - total_name_size;
   }


   /* rvt note:  This code is wierd.  "space_needed" is the minimum
    * freesize minus the entire remaining size from this node to the
    * end of this (head) block.  As if all this was for this file and not
    * other files or dirty.
    * Then "extra_available" is the excess over-and-above min freesize.
    *
    * This appears to be an attempt to see if (worst-case, where all the
    * rest of the head block belongs to this file) the data in the head
    * block from "node" to the end, will still not drop the freesize
    * below min_freesize. If so, reduce the datasize of the rewrite.
    *
    * I don't think it is necessary. At least, not now that we optimise what
    * we move off the head page.
    * 
    */
#if 0
   /* Calculate the amount of space needed to hold the nodes
      which are remaining in the tail */
   space_needed = fmc->min_free_size - (node->fm->offset % fmc->sector_size);

   /* From that, calculate how much 'extra' space we can use to
      increase the size of the node we're writing from the size
      of the node we're obsoleting
   */
   if (space_needed > fmc->free_size) {
      /* If we've gone below min_free_size for some reason,
	 don't fuck up. This is why we have 
	 min_free_size > sector_size. Whinge about it though,
	 just so I can convince myself my maths is right.
      */
      D1(printk(KERN_WARNING "jffs_garbage_collect_next: "
		"space_needed %d exceeded free_size %d\n",
		space_needed, fmc->free_size));
      extra_available = 0;
   } else {
      extra_available = fmc->free_size - space_needed;
   }

   /* Check that we don't use up any more 'extra' space than
      what's available */
   if (size > JFFS_PAD(node->data_size) + total_name_size + 
       sizeof(struct jffs_raw_inode) + extra_available) {
      D1(printk("Reducing size of new node from %d to %ld to avoid "
		"catching our tail\n", size, 
		(long) (JFFS_PAD(node->data_size) + JFFS_PAD(node->name_size) + 
			sizeof(struct jffs_raw_inode) + extra_available)));
      D1(printk("space_needed = %d, extra_available = %d\n", 
		space_needed, extra_available));

      size = JFFS_PAD(node->data_size) + total_name_size + 
	 sizeof(struct jffs_raw_inode) + extra_available;
      data_size = size - sizeof(struct jffs_raw_inode)
	 - total_name_size;
   }
#endif

   D2(printk("  total_name_size: %u\n", total_name_size));
   D2(printk("  data_size: %u\n", data_size));
   D2(printk("  size: %u\n", size));
   D2(printk("  f->nsize: %u\n", f->nsize));
   D2(printk("  f->size: %u\n", f->size));
   D2(printk("  node->data_offset: %u\n", node->data_offset));
   D2(printk("  free_chunk_size1: %u\n", jffs_free_size1(fmc)));
   D2(printk("  free_chunk_size2: %u\n", jffs_free_size2(fmc)));
   D2(printk("  node->fm->offset: %8x   size: %4x\n", node->fm->offset, data_size));

   if ((err = jffs_rewrite_data(f, node, data_size))) {
      printk(KERN_WARNING "jffs_rewrite_data() failed: %d\n", err);
      return err;
   }
   else
      err = 1;

jffs_garbage_collect_next_end:
   D3(printk("jffs_garbage_collect_next: Leaving...\n"));
   return err;
} /* jffs_garbage_collect_next */


/******************************************************************/

/* If an obsolete node is partly going to be erased due to garbage
   collection, the part that isn't going to be erased must be filled
   with zeroes so that the scan of the flash will work smoothly next
   time.  (The data in the file could for instance be a JFFS image
   which could cause enormous confusion during a scan of the flash
   device if we didn't do this.)
   There are two phases in this procedure: First, the clearing of
   the name and data parts of the node. Second, possibly also clearing
   a part of the raw inode as well.  If the box is power cycled during
   the first phase, only the checksum of this node-to-be-cleared-at-
   the-end will be wrong.  If the box is power cycled during, or after,
   the clearing of the raw inode, the information like the length of
   the name and data parts are zeroed.  The next time the box is
   powered up, the scanning algorithm manages this faulty data too
   because:

   - The checksum is invalid and thus the raw inode must be discarded
   in any case.
   - If the lengths of the data part or the name part are zeroed, the
   scanning just continues after the raw inode.  But after the inode
   the scanning procedure just finds zeroes which is the same as
   dirt.

   So, in the end, this could never fail. :-)  Even if it does fail,
   the scanning algorithm should manage that too.  */

static int jffs_clear_end_of_node(struct jffs_control *c, __u32 erase_size)
{
   struct jffs_fm *fm;
   struct jffs_fmcontrol *fmc = c->fmc;
   __u32 zero_offset;
   __u32 zero_size;
   __u32 zero_offset_data;
   __u32 zero_size_data;
   __u32 cutting_raw_inode = 0;

   if (!(fm = jffs_cut_node(fmc, erase_size))) {
      D3(printk("jffs_clear_end_of_node: fm == NULL\n"));
      return 0;
   }

   /* Where and how much shall we clear?  */
   zero_offset = fmc->head->offset + erase_size;
   zero_size = fm->offset + fm->size - zero_offset;

   /* Do we have to clear the raw_inode explicitly?  */
   if (fm->size - zero_size < sizeof(struct jffs_raw_inode)) {
      cutting_raw_inode = sizeof(struct jffs_raw_inode)
	 - (fm->size - zero_size);
   }

   /* First, clear the name and data fields.  */
   zero_offset_data = zero_offset + cutting_raw_inode;
   zero_size_data = zero_size - cutting_raw_inode;
   flash_memset(fmc, zero_offset_data, 0, zero_size_data);

   /* Should we clear a part of the raw inode?  */
   if (cutting_raw_inode) {
      /* I guess it is ok to clear the raw inode in this order.  */
      flash_memset(fmc, zero_offset, 0, cutting_raw_inode);
   }

   return 0;
} /* jffs_clear_end_of_node()  */



/******************************************************************/
/* Try to erase as much as possible of the dirt in the flash memory.  */
long jffs_try_to_erase(struct jffs_control *c)
{
   struct jffs_fmcontrol *fmc = c->fmc;
   long erase_size;
   int err;
   __u32 offset;

   D3(printk("jffs_try_to_erase()\n"));

   erase_size = jffs_erasable_size(fmc);

   D2(printk("jffs_try_to_erase: erase_size = %ld\n", erase_size));

   if (erase_size == 0) {
      return 0;
   }
   else if (erase_size < 0) {
      printk(KERN_ERR "JFFS: jffs_try_to_erase: "
	     "jffs_erasable_size returned %ld.\n", erase_size);
      return erase_size;
   }

   offset = fmc->head->offset;

   //printk("Refuse erase of %lx at %lx\n", erase_size, offset); return -1; //temp
   
   if ((err = jffs_clear_end_of_node(c, erase_size)) < 0) {
      printk(KERN_ERR "JFFS: jffs_try_to_erase: "
	     "Clearing of node failed.\n");
      return err;
   }

   /* Now, let's try to do the erase.  */
   if ((err = flash_erase_region(fmc->mtd,
				 offset, erase_size)) < 0) {
      printk(KERN_ERR "JFFS: Erase of flash failed. "
	     "offset = %u, erase_size = %ld\n",
	     offset, erase_size);
      return err;
   }


   /* Update the flash memory data structures.  */
   jffs_sync_erase(fmc, erase_size);
 
   return erase_size;
}



/******************************************************************/
#if 1
void print_layout(struct jffs_control *c)
{
   struct jffs_fm *fm = 0;
   struct jffs_fm *last_fm = 0;

   /* Get the first item in the list */
   fm = c->fmc->head;
   /* Loop through all of the flash control structures */
   printk(" head: %08lx\n",(unsigned long) fm);
   while (fm) {
      if (fm->nodes) {
	 printk ("%8lX %8lX ino=%7u., ver=%4lX"
		 " [ofs: %8lx  siz: %8lx  repl: %8lx]\n",
		 (unsigned long) fm->offset,
		 (unsigned long) fm->size,
		 (unsigned) fm->nodes->node->ino,
		 (unsigned long) fm->nodes->node->version,
		 (unsigned long) fm->nodes->node->data_offset,
		 (unsigned long) fm->nodes->node->data_size,
		 (unsigned long) fm->nodes->node->removed_size);
      }
      else {
	 printk ("%8lX %08lX dirty\n",
		 (unsigned long) fm->offset,
		 (unsigned long) fm->size);
      }
      last_fm = fm;
      fm = fm->next;
   }
}
#endif


/******************************************************************/
/******************************************************************/
/******************************************************************/
/* We are doing a write-in-place garbage collection.
 * Adjust values so that we seem to have an extra EB.  This pseudo-EB
 * is actually in allocated RAM.
 * When the GC is done, copy that pseudo-EB back to the flash and
 * fix up the fm->offsets. */
int gc_inplace_begin(struct jffs_fmcontrol *fmc)
{
   fmc->aux_block = vmalloc(fmc->sector_size);
   //printk("%s  aux: %p\n", __FUNCTION__, fmc->aux_block);
   if (!fmc->aux_block)
      return -ENOMEM;
   /* temporarily pretend that there's one more block. */
   fmc->flash_size += fmc->sector_size;
   fmc->free_size += fmc->sector_size;
   return 0;
}

#if JFFS_RAM_BLOCKS > 0
extern unsigned char *jffs_ram;
#endif

/******************************************************************/
void gc_inplace_end(struct jffs_fmcontrol *fmc, int erased)
{
   struct jffs_fm *fm;
   int err;
   
   //printk("%s   hofs: %x   er: %5x\n", __FUNCTION__,
   //	fmc->head? fmc->head->offset: 0, erased);
   if (fmc->head == NULL) {	/* Everything got erased. */
      fmc->first_fm = 0;
      goto ip_end;
   }

   if ((fmc->head && fmc->head->offset != 0x10000) || erased == 0) {
      printk("Something has gone seriously wrong with inplace GC!  Prepare to die.\n");
      printk("h: %p    era: %5x  f_fm: %x\n", fmc->head, erased, fmc->first_fm);
      D2(jffs_print_fmcontrol(fmc));
      print_layout(fmc->c);
      goto ip_end;
   }
   
   /* Copy the data to the 1st block. */
   //printk("will cp %5x bytes\n", (fmc->tail->offset + fmc->tail->size) % fmc->sector_size); 
   err = flash_safe_write(fmc, 0, fmc->aux_block,
		    (fmc->tail->offset + fmc->tail->size) % fmc->sector_size);
   if (err < 0) {
      printk(KERN_ERR "JFFS: gc_inplace_end: Error on write to flash.err: %d   Fatal error!\n", err);
   }
   for (fm = fmc->head; fm ; fm = fm->next) {
      //printk("change %5x to %5x\n", fm->offset,  fm->offset - 0x10000);
      fm->offset -= 0x10000;
   }
ip_end:;
   fmc->flash_size -= fmc->sector_size;
   fmc->free_size -= fmc->sector_size;
   vfree(fmc->aux_block);
   fmc->aux_block = NULL;
 }


/******************************************************************/
/* There are different criteria that should trigger a garbage collect:

   1. There is too much dirt in the memory.
   2. The free space is becoming small.
   3. There are many versions of a node.

   The garbage collect should always be done in a manner that guarantees
   that future garbage collects cannot be locked.  E.g. Rewritten chunks
   should not be too large (span more than one sector in the flash memory
   for example).  Of course there is a limit on how intelligent this garbage
   collection can be.  */
/* Returns: <0 if failed, > 0  if we erased block(s).  The # of bytes erased.. */
int jffs_garbage_collect_now(struct jffs_control *c, int force, int merge_obn)
{
   struct jffs_fmcontrol *fmc = c->fmc;
   long erased = 0;
   int result = 0;
   int round = 0;
   int npround = 0;	/* Rounds with no progress. */
   
   D1(printk("\n***jffs_garbage_collect_now: fmc->dirty_size = %5x, fmc->free_size = %5x\n, fcs1=0x%x, fcs2=0x%x\n",
	     fmc->dirty_size, fmc->free_size, jffs_free_size1(fmc), jffs_free_size2(fmc)));
   D2(jffs_print_fmcontrol(fmc));
   
   /* If it is possible to garbage collect, do so.  */
   while (erased == 0) {
      ++round;
      ++npround;
      D1(printk(KERN_NOTICE "***GC__now: round #%u, dirty_size = %5x     free_size = %5x\n",
		 round, fmc->dirty_size, fmc->free_size));
      D2(jffs_print_fmcontrol(fmc));

      if ((erased = jffs_try_to_erase(c)) < 0) {
	 printk(KERN_WARNING "JFFS: Error in "
		"garbage collector.\n");
	 result = erased;
	 break;
      }
      if (erased) {
	 result = erased;
	 break;
      }
      if (npround > 1000) { printk("brokeout\n"); break;} /// Should never happen, though.
      
      if (fmc->free_size == 0) {
	 /* Argh */
	 printk(KERN_ERR "jffs_garbage_collect_now: free_size == 0. This is BAD.\n");
	 result = -ENOSPC;
	 break;
      }

      /* See if we should force a GC, even if it might not free up an entire block.
       * Basically, if we can move all the used data from this block to another block(s) we can
       * erase this block.  Sometimes we need to do that, just to convert dirty to free. */
      if (force || ((fmc->free_size + fmc->dirty_size + fmc->wasted_size) / fmc->sector_size) >
	  (fmc->free_size / fmc->sector_size)) {
	 //printk("Can pick up a block, so try to GC anyway.\n");
      }
      else {
	 if (fmc->dirty_size < fmc->sector_size) {
	    /* Actually, we _may_ have been able to free some, 
	     * if there are many overlapping nodes which aren't
	     * actually marked dirty because they still have
	     * some valid data in each.  The overlap is "wasted".
	     */
	    printk("ENOSPC: %s @ %d\n", __FUNCTION__, __LINE__);
	    result = -ENOSPC;
	    break;
	 }
      }
	
      /* Let's dare to make a garbage collect.  */
      if ((result = jffs_garbage_collect_next(c, merge_obn)) < 0) {
	 printk(KERN_ERR "JFFS: Something "
		"has gone seriously wrong "
		"with a garbage collect.\n");
	 goto gc_end;
      }
      D1(printk(KERN_NOTICE "   jffs_garbage_collect_now: erased: %ld\n", erased));
      DJM(jffs_print_memory_allocation_statistics());
      if (result > 0) {	/* GC_next made progress. */
	 npround = result = 0;
      }
      else {	/* GC didn't do anything.  If the head block is one dirty & the rest free,
		 * dirty the free so we can GC. */
	 if (fmc->head == fmc->tail && fmc->head->nodes == NULL)
	    dirty_to_eob(c, fmc->sector_size - ((fmc->tail->offset + fmc->tail->size) % fmc->sector_size));
      }
   }
	
gc_end:
   D3(printk("   jffs_garbage_collect_now: Leaving...erased = %ld\n", erased));
   D1(if (erased) {
	 printk(KERN_NOTICE "jffs_g_c_now: erased = %ld\n", erased);
      });

   if (!erased && !result) {
      printk("ENOSPC: %s @ %d\n", __FUNCTION__, __LINE__);
      return -ENOSPC;
   }

   return result;
} /* jffs_garbage_collect_now() */




/******************************************************************/
/* Determine if it is reasonable to start garbage collection. */
static inline int thread_should_wake (struct jffs_control *c)
{
   /* If there is too much RAM used by the various structures, GC */
   if (jffs_get_node_inuse() > (c->fmc->used_size/c->fmc->max_chunk_size * 5 + jffs_get_file_count() * 2 + 50)) {
      D2(printk(KERN_NOTICE "thread_should_wake: Waking due to number of nodes\n"));
      return 1;
   }
   /* If there are fewer free bytes than the threshold, GC.
    * 5% of the flash size, but no lower than one block.
    * In a router, this is always 1 block, since the flash size is NEVER 20+ blocks.
    * IOW, this will never trigger.   */
   if (c->fmc->free_size < c->gc_minfree_threshold) {
      D2(printk(KERN_NOTICE "thread_should_wake: Waking due to insufficent free space\n"));
      return 1;
   }
   /* If there are more dirty bytes than the threshold, GC.
    * One-third of the flash size, but no lower than one block.
    * In a router, this will typically be 1 or 2 (or maybe 3) blocks.
    */
   if (c->fmc->dirty_size + c->fmc->wasted_size > c->gc_maxdirty_threshold) {
      D2(printk(KERN_NOTICE "thread_should_wake: Waking due to excessive dirty space\n"));
      return 1;
   }	
   return 0;
}




/******************************************************************/
void jffs_garbage_collect_trigger(struct jffs_control *c)
{
   D3(printk("jffs_garbage_collect_trigger\n"));
   c->gc_sleep_time = INIT_GC_SLEEP;	/* Reset GC sleep time. */
   if (c->gc_task)
      send_sig(SIGHUP, c->gc_task, 1);
}
  
/******************************************************************/
/******************************************************************/
/******************************************************************/
/* Kernel threads take (void *) as arguments.   Thus we pass
   the jffs_control data as a (void *) and then cast it. */

/* Background garbase collection is only an optimization, not necessary.  We do it
   after something has been modified in the fs.  When no nore modifications have
   happened for a while, we GC in the background, to clear out the garbage so
   that we can (hopefully) avoid having to do a GC the next time a write takes place.

   The longer it has been since a write, the more aggreesive we are in the
   garbage collection.   The theory is: we are going to have to do garbage collection
   and erasing flash blocks anyway, so let's do it *before* it will become necessary.
   The only time that we might wind up doing a gratuitous Gc & erase is when it has been
   a long long time since the last write.  That's when we will do a GC which will convert
   dirty space to free even though it won't allow us to pick up an entire free block.  I've
   decided that it is worthwhile to get more free, so that it will be immediately
   usable whenever the next write eventually occurs.
   
   Each call to jffs_garbage_collect_next will rotate some of the file that is
   the first node in the flash to the end, thereby getting it off the head block.
   After all the nodes have been moved off the head block, it will be all dirty and can be
   erased and added to free.

   Depending on alignment & fragmentation, even if dirty is less than blocksize, we
   might be able to free a block.  Also, wasted bytes are dirty but are within a non-dirty node.

   It might not be possible to (reasonably) free up all of the dirty nodes, depending
   on the exact placement of the nodes.  A collect moves a busy node but makes the old node
   dirty, thereby temporarily increasing the dirty_size.  So we stop when the dirty node
   placement is optimal:  all at the beginning of the head block.  This placement will allow
   the head block to quickly get erased when the user starts writing files.
*/
int jffs_garbage_collect_thread(void *ptr)
{
   struct jffs_control *c = (struct jffs_control *) ptr;
   struct jffs_fmcontrol *fmc = c->fmc;
   long erased;
   int result = 0;
   int blocks_would_be_freed;	/* Number of free blocks that a complete GC would add. */
   int dirty_optimal;		/* Boolean: All the dirty is contiguous at the head. */

   allow_signal(SIGKILL);
   allow_signal(SIGSTOP);
   allow_signal(SIGCONT);
   allow_signal(SIGHUP);

   c->gc_task = current;
   complete(&c->gc_thread_init);
   set_user_nice(current, 10);
   c->gc_sleep_time = 0;
   D1(printk (KERN_NOTICE "jffs_garbage_collect_thread: Starting infinite loop.\n"));

   for (;;) {
      /* See if we need to start gc. If we don't, go to sleep. */
      if (!thread_should_wake(c)) {
	 set_current_state(TASK_INTERRUPTIBLE);
	 schedule(); /* Yes, we do this even if we want to go
			on immediately - we're a low priority
			background task. */
      }

      /* This thread is purely an optimisation. But if it runs when
         other things could be running, it actually makes things a
         lot worse. Use yield() and put it at the back of the runqueue
         every time. Especially during boot, pulling an inode in
         with read_inode() is much preferable to having the GC thread
         get there first. */
      yield();

      /* Put_super will send a SIGKILL and then wait on the sem.  */
      while (signal_pending(current)) {
	 siginfo_t info;
	 unsigned long signr;

	 signr = dequeue_signal_lock(current, &current->blocked, &info);

	 switch(signr) {
	 case SIGSTOP:
	    D1(printk("jffs_garbage_collect_thread: SIGSTOP received.\n"));
	    set_current_state(TASK_STOPPED);
	    schedule();
	    break;

	 case SIGKILL:
	    D1(printk("jffs_garbage_collect_thread: SIGKILL received.\n"));
	    goto die;
	 }
      }

      D1(printk (KERN_NOTICE "jffs_garbage_collect_thread: checking.\n"));
      mutex_lock(&fmc->biglock);
      /* Quit if flash is empty or there is no dirt. */
      if (fmc->head == NULL || fmc->dirty_size == 0) {
	 c->gc_sleep_time = 0;
	 goto gc_end;
      }

      /* Must not not not do background GCing when doing write-in-place. */
      if (fmc->do_inplace_gc) {
	 c->gc_sleep_time = 0;
	 goto gc_end;	  
      }
     
      /* And little reason to GC when head & tail are in the same block. */
      if (c->gc_sleep_time < THRESH_LAZY_GC &&
	  fmc->tail && fmc->tail->offset / fmc->sector_size ==
	  fmc->head->offset / fmc->sector_size) {
	 goto gc_end;	  
      }
     
      if (c->gc_sleep_time <THRESH_LAZY_GC && !thread_should_wake(c))
	 goto gc_end;
 
      D1(printk(KERN_NOTICE "*****jffs_garbage_collect_thread pass: "
		"fmc->dirty_size = %5x\n", fmc->dirty_size));
      D2(jffs_print_fmcontrol(fmc));

      dirty_optimal = 0;
      if (c->gc_sleep_time <THRESH_LAZY_DIRT)
	 dirty_optimal = (head_contig_dirty_size(fmc) == fmc->dirty_size); 
      else if (fmc->head->nodes == NULL && fmc->head->size == fmc->dirty_size)
	 dirty_optimal = 1;
     
      blocks_would_be_freed = ((fmc->free_size + fmc->dirty_size + fmc->wasted_size) /
			       fmc->sector_size) - (fmc->free_size / fmc->sector_size);

      if (blocks_would_be_freed == 0 && dirty_optimal && fmc->wasted_size == 0) {
	    D1(printk (KERN_NOTICE "GC_thread: Dirty completely optimized.  No GC possible.\n"));
	    if (c->gc_sleep_time >= MAX_GC_SLEEP)
	       c->gc_sleep_time = 0;
	    goto gc_end;	  
      }
 
      if ((erased = jffs_try_to_erase(c)) < 0)
	 printk(KERN_WARNING "JFFS: Error in garbage collector: %ld.\n", erased);
      if (erased)
	 goto gc_end;

      if (fmc->free_size == 0) {
	 printk(KERN_ERR "jffs_garbage_collect_thread: free_size == 0. This is BAD.\n");
	 c->gc_sleep_time = 0;
	 goto gc_end;
      }
		
      /* Let's dare to make a garbage collect.  */
      if ((result = jffs_garbage_collect_next(c, (c->gc_sleep_time < THRESH_MERGE_OFB_NODES)? 0 : 1 )) < 0) {
	 printk(KERN_ERR "JFFS: Something "
		"has gone seriously wrong "
		"with a garbage collect: %d\n", result);
      }
      if (result > 0) {
	 if ((erased = jffs_try_to_erase(c)) < 0)
	    printk(KERN_WARNING "JFFS: Error in garbage collector: %ld.\n", erased);
      }

   gc_end:
      /* If our work is not done, sleep for longer & longer times. */
      result = c->gc_sleep_time;
      c->gc_sleep_time <<= 1;
      if (c->gc_sleep_time > MAX_GC_SLEEP)
	 c->gc_sleep_time = MAX_GC_SLEEP;
      mutex_unlock(&fmc->biglock);
      if (c->gc_sleep_time != 0) {
	 set_current_state (TASK_INTERRUPTIBLE);
	 schedule_timeout(result);
      }
   }
die: 
   D1(printk("jffs_garbage_collect exiting.\n"));
   c->gc_task = NULL;
   complete_and_exit(&c->gc_thread_comp, 0);
} /* jffs_garbage_collect_thread() */
