/*
 * raid10.c : Multiple Devices driver for Linux
 *
 * Copyright (C) 2000-2004 Neil Brown
 *
 * RAID-10 support for md.
 *
 * Base on code in raid1.c.  See raid1.c for further copyright information.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example /usr/src/linux/COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/seq_file.h>
#include "md.h"
#include "raid10.h"
#include "raid0.h"
#include "bitmap.h"

/*
 * RAID10 provides a combination of RAID0 and RAID1 functionality.
 * The layout of data is defined by
 *    chunk_size
 *    raid_disks
 *    near_copies (stored in low byte of layout)
 *    far_copies (stored in second byte of layout)
 *    far_offset (stored in bit 16 of layout )
 *
 * The data to be stored is divided into chunks using chunksize.
 * Each device is divided into far_copies sections.
 * In each section, chunks are laid out in a style similar to raid0, but
 * near_copies copies of each chunk is stored (each on a different drive).
 * The starting device for each section is offset near_copies from the starting
 * device of the previous section.
 * Thus they are (near_copies*far_copies) of each chunk, and each is on a different
 * drive.
 * near_copies and far_copies must be at least one, and their product is at most
 * raid_disks.
 *
 * If far_offset is true, then the far_copies are handled a bit differently.
 * The copies are still in different stripes, but instead of be very far apart
 * on disk, there are adjacent stripes.
 */

/*
 * Number of guaranteed r10bios in case of extreme VM load:
 */
#define	NR_RAID10_BIOS 256

static void allow_barrier(conf_t *conf);
static void lower_barrier(conf_t *conf);

static void * r10bio_pool_alloc(gfp_t gfp_flags, void *data)
{
	conf_t *conf = data;
	int size = offsetof(struct r10bio_s, devs[conf->copies]);

	/* allocate a r10bio with room for raid_disks entries in the bios array */
	return kzalloc(size, gfp_flags);
}

static void r10bio_pool_free(void *r10_bio, void *data)
{
	kfree(r10_bio);
}

/* Maximum size of each resync request */
#define RESYNC_BLOCK_SIZE (64*1024)
#define RESYNC_PAGES ((RESYNC_BLOCK_SIZE + PAGE_SIZE-1) / PAGE_SIZE)
/* amount of memory to reserve for resync requests */
#define RESYNC_WINDOW (1024*1024)
/* maximum number of concurrent requests, memory permitting */
#define RESYNC_DEPTH (32*1024*1024/RESYNC_BLOCK_SIZE)

/*
 * When performing a resync, we need to read and compare, so
 * we need as many pages are there are copies.
 * When performing a recovery, we need 2 bios, one for read,
 * one for write (we recover only one drive per r10buf)
 *
 */
static void * r10buf_pool_alloc(gfp_t gfp_flags, void *data)
{
	conf_t *conf = data;
	struct page *page;
	r10bio_t *r10_bio;
	struct bio *bio;
	int i, j;
	int nalloc;

	r10_bio = r10bio_pool_alloc(gfp_flags, conf);
	if (!r10_bio)
		return NULL;

	if (test_bit(MD_RECOVERY_SYNC, &conf->mddev->recovery))
		nalloc = conf->copies; /* resync */
	else
		nalloc = 2; /* recovery */

	/*
	 * Allocate bios.
	 */
	for (j = nalloc ; j-- ; ) {
		bio = bio_kmalloc(gfp_flags, RESYNC_PAGES);
		if (!bio)
			goto out_free_bio;
		r10_bio->devs[j].bio = bio;
	}
	/*
	 * Allocate RESYNC_PAGES data pages and attach them
	 * where needed.
	 */
	for (j = 0 ; j < nalloc; j++) {
		bio = r10_bio->devs[j].bio;
		for (i = 0; i < RESYNC_PAGES; i++) {
			page = alloc_page(gfp_flags);
			if (unlikely(!page))
				goto out_free_pages;

			bio->bi_io_vec[i].bv_page = page;
		}
	}

	return r10_bio;

out_free_pages:
	for ( ; i > 0 ; i--)
		safe_put_page(bio->bi_io_vec[i-1].bv_page);
	while (j--)
		for (i = 0; i < RESYNC_PAGES ; i++)
			safe_put_page(r10_bio->devs[j].bio->bi_io_vec[i].bv_page);
	j = -1;
out_free_bio:
	while ( ++j < nalloc )
		bio_put(r10_bio->devs[j].bio);
	r10bio_pool_free(r10_bio, conf);
	return NULL;
}

static void r10buf_pool_free(void *__r10_bio, void *data)
{
	int i;
	conf_t *conf = data;
	r10bio_t *r10bio = __r10_bio;
	int j;

	for (j=0; j < conf->copies; j++) {
		struct bio *bio = r10bio->devs[j].bio;
		if (bio) {
			for (i = 0; i < RESYNC_PAGES; i++) {
				safe_put_page(bio->bi_io_vec[i].bv_page);
				bio->bi_io_vec[i].bv_page = NULL;
			}
			bio_put(bio);
		}
	}
	r10bio_pool_free(r10bio, conf);
}

static void put_all_bios(conf_t *conf, r10bio_t *r10_bio)
{
	int i;

	for (i = 0; i < conf->copies; i++) {
		struct bio **bio = & r10_bio->devs[i].bio;
		if (*bio && *bio != IO_BLOCKED)
			bio_put(*bio);
		*bio = NULL;
	}
}

static void free_r10bio(r10bio_t *r10_bio)
{
	conf_t *conf = r10_bio->mddev->private;

	/*
	 * Wake up any possible resync thread that waits for the device
	 * to go idle.
	 */
	allow_barrier(conf);

	put_all_bios(conf, r10_bio);
	mempool_free(r10_bio, conf->r10bio_pool);
}

static void put_buf(r10bio_t *r10_bio)
{
	conf_t *conf = r10_bio->mddev->private;

	mempool_free(r10_bio, conf->r10buf_pool);

	lower_barrier(conf);
}

static void reschedule_retry(r10bio_t *r10_bio)
{
	unsigned long flags;
	mddev_t *mddev = r10_bio->mddev;
	conf_t *conf = mddev->private;

	spin_lock_irqsave(&conf->device_lock, flags);
	list_add(&r10_bio->retry_list, &conf->retry_list);
	conf->nr_queued ++;
	spin_unlock_irqrestore(&conf->device_lock, flags);

	/* wake up frozen array... */
	wake_up(&conf->wait_barrier);

	md_wakeup_thread(mddev->thread);
}

/*
 * raid_end_bio_io() is called when we have finished servicing a mirrored
 * operation and are ready to return a success/failure code to the buffer
 * cache layer.
 */
static void raid_end_bio_io(r10bio_t *r10_bio)
{
	struct bio *bio = r10_bio->master_bio;

	bio_endio(bio,
		test_bit(R10BIO_Uptodate, &r10_bio->state) ? 0 : -EIO);
	free_r10bio(r10_bio);
}

/*
 * Update disk head position estimator based on IRQ completion info.
 */
static inline void update_head_pos(int slot, r10bio_t *r10_bio)
{
	conf_t *conf = r10_bio->mddev->private;

	conf->mirrors[r10_bio->devs[slot].devnum].head_position =
		r10_bio->devs[slot].addr + (r10_bio->sectors);
}

static void raid10_end_read_request(struct bio *bio, int error)
{
	int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
	r10bio_t *r10_bio = bio->bi_private;
	int slot, dev;
	conf_t *conf = r10_bio->mddev->private;


	slot = r10_bio->read_slot;
	dev = r10_bio->devs[slot].devnum;
	/*
	 * this branch is our 'one mirror IO has finished' event handler:
	 */
	update_head_pos(slot, r10_bio);

	if (uptodate) {
		/*
		 * Set R10BIO_Uptodate in our master bio, so that
		 * we will return a good error code to the higher
		 * levels even if IO on some other mirrored buffer fails.
		 *
		 * The 'master' represents the composite IO operation to
		 * user-side. So if something waits for IO, then it will
		 * wait for the 'master' bio.
		 */
		set_bit(R10BIO_Uptodate, &r10_bio->state);
		raid_end_bio_io(r10_bio);
	} else {
		/*
		 * oops, read error:
		 */
		char b[BDEVNAME_SIZE];
		if (printk_ratelimit())
			printk(KERN_ERR "md/raid10:%s: %s: rescheduling sector %llu\n",
			       mdname(conf->mddev),
			       bdevname(conf->mirrors[dev].rdev->bdev,b), (unsigned long long)r10_bio->sector);
		reschedule_retry(r10_bio);
	}

	rdev_dec_pending(conf->mirrors[dev].rdev, conf->mddev);
}

static void raid10_end_write_request(struct bio *bio, int error)
{
	int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
	r10bio_t *r10_bio = bio->bi_private;
	int slot, dev;
	conf_t *conf = r10_bio->mddev->private;

	for (slot = 0; slot < conf->copies; slot++)
		if (r10_bio->devs[slot].bio == bio)
			break;
	dev = r10_bio->devs[slot].devnum;

	/*
	 * this branch is our 'one mirror IO has finished' event handler:
	 */
	if (!uptodate) {
		md_error(r10_bio->mddev, conf->mirrors[dev].rdev);
		/* an I/O failed, we can't clear the bitmap */
		set_bit(R10BIO_Degraded, &r10_bio->state);
	} else
		/*
		 * Set R10BIO_Uptodate in our master bio, so that
		 * we will return a good error code for to the higher
		 * levels even if IO on some other mirrored buffer fails.
		 *
		 * The 'master' represents the composite IO operation to
		 * user-side. So if something waits for IO, then it will
		 * wait for the 'master' bio.
		 */
		set_bit(R10BIO_Uptodate, &r10_bio->state);

	update_head_pos(slot, r10_bio);

	/*
	 *
	 * Let's see if all mirrored write operations have finished
	 * already.
	 */
	if (atomic_dec_and_test(&r10_bio->remaining)) {
		/* clear the bitmap if all writes complete successfully */
		bitmap_endwrite(r10_bio->mddev->bitmap, r10_bio->sector,
				r10_bio->sectors,
				!test_bit(R10BIO_Degraded, &r10_bio->state),
				0);
		md_write_end(r10_bio->mddev);
		raid_end_bio_io(r10_bio);
	}

	rdev_dec_pending(conf->mirrors[dev].rdev, conf->mddev);
}


/*
 * RAID10 layout manager
 * As well as the chunksize and raid_disks count, there are two
 * parameters: near_copies and far_copies.
 * near_copies * far_copies must be <= raid_disks.
 * Normally one of these will be 1.
 * If both are 1, we get raid0.
 * If near_copies == raid_disks, we get raid1.
 *
 * Chunks are laid out in raid0 style with near_copies copies of the
 * first chunk, followed by near_copies copies of the next chunk and
 * so on.
 * If far_copies > 1, then after 1/far_copies of the array has been assigned
 * as described above, we start again with a device offset of near_copies.
 * So we effectively have another copy of the whole array further down all
 * the drives, but with blocks on different drives.
 * With this layout, and block is never stored twice on the one device.
 *
 * raid10_find_phys finds the sector offset of a given virtual sector
 * on each device that it is on.
 *
 * raid10_find_virt does the reverse mapping, from a device and a
 * sector offset to a virtual address
 */

static void raid10_find_phys(conf_t *conf, r10bio_t *r10bio)
{
	int n,f;
	sector_t sector;
	sector_t chunk;
	sector_t stripe;
	int dev;

	int slot = 0;

	/* now calculate first sector/dev */
	chunk = r10bio->sector >> conf->chunk_shift;
	sector = r10bio->sector & conf->chunk_mask;

	chunk *= conf->near_copies;
	stripe = chunk;
	dev = sector_div(stripe, conf->raid_disks);
	if (conf->far_offset)
		stripe *= conf->far_copies;

	sector += stripe << conf->chunk_shift;

	/* and calculate all the others */
	for (n=0; n < conf->near_copies; n++) {
		int d = dev;
		sector_t s = sector;
		r10bio->devs[slot].addr = sector;
		r10bio->devs[slot].devnum = d;
		slot++;

		for (f = 1; f < conf->far_copies; f++) {
			d += conf->near_copies;
			if (d >= conf->raid_disks)
				d -= conf->raid_disks;
			s += conf->stride;
			r10bio->devs[slot].devnum = d;
			r10bio->devs[slot].addr = s;
			slot++;
		}
		dev++;
		if (dev >= conf->raid_disks) {
			dev = 0;
			sector += (conf->chunk_mask + 1);
		}
	}
	BUG_ON(slot != conf->copies);
}

static sector_t raid10_find_virt(conf_t *conf, sector_t sector, int dev)
{
	sector_t offset, chunk, vchunk;

	offset = sector & conf->chunk_mask;
	if (conf->far_offset) {
		int fc;
		chunk = sector >> conf->chunk_shift;
		fc = sector_div(chunk, conf->far_copies);
		dev -= fc * conf->near_copies;
		if (dev < 0)
			dev += conf->raid_disks;
	} else {
		while (sector >= conf->stride) {
			sector -= conf->stride;
			if (dev < conf->near_copies)
				dev += conf->raid_disks - conf->near_copies;
			else
				dev -= conf->near_copies;
		}
		chunk = sector >> conf->chunk_shift;
	}
	vchunk = chunk * conf->raid_disks + dev;
	sector_div(vchunk, conf->near_copies);
	return (vchunk << conf->chunk_shift) + offset;
}

/**
 *	raid10_mergeable_bvec -- tell bio layer if a two requests can be merged
 *	@q: request queue
 *	@bvm: properties of new bio
 *	@biovec: the request that could be merged to it.
 *
 *	Return amount of bytes we can accept at this offset
 *      If near_copies == raid_disk, there are no striping issues,
 *      but in that case, the function isn't called at all.
 */
static int raid10_mergeable_bvec(struct request_queue *q,
				 struct bvec_merge_data *bvm,
				 struct bio_vec *biovec)
{
	mddev_t *mddev = q->queuedata;
	sector_t sector = bvm->bi_sector + get_start_sect(bvm->bi_bdev);
	int max;
	unsigned int chunk_sectors = mddev->chunk_sectors;
	unsigned int bio_sectors = bvm->bi_size >> 9;

	max =  (chunk_sectors - ((sector & (chunk_sectors - 1)) + bio_sectors)) << 9;
	if (max < 0) max = 0; /* bio_add cannot handle a negative return */
	if (max <= biovec->bv_len && bio_sectors == 0)
		return biovec->bv_len;
	else
		return max;
}

/*
 * This routine returns the disk from which the requested read should
 * be done. There is a per-array 'next expected sequential IO' sector
 * number - if this matches on the next IO then we use the last disk.
 * There is also a per-disk 'last know head position' sector that is
 * maintained from IRQ contexts, both the normal and the resync IO
 * completion handlers update this position correctly. If there is no
 * perfect sequential match then we pick the disk whose head is closest.
 *
 * If there are 2 mirrors in the same 2 devices, performance degrades
 * because position is mirror, not device based.
 *
 * The rdev for the device selected will have nr_pending incremented.
 */

/*
 * FIXME: possibly should rethink readbalancing and do it differently
 * depending on near_copies / far_copies geometry.
 */
static int read_balance(conf_t *conf, r10bio_t *r10_bio)
{
	const sector_t this_sector = r10_bio->sector;
	int disk, slot, nslot;
	const int sectors = r10_bio->sectors;
	sector_t new_distance, current_distance;
	mdk_rdev_t *rdev;

	raid10_find_phys(conf, r10_bio);
	rcu_read_lock();
	/*
	 * Check if we can balance. We can balance on the whole
	 * device if no resync is going on (recovery is ok), or below
	 * the resync window. We take the first readable disk when
	 * above the resync window.
	 */
	if (conf->mddev->recovery_cp < MaxSector
	    && (this_sector + sectors >= conf->next_resync)) {
		/* make sure that disk is operational */
		slot = 0;
		disk = r10_bio->devs[slot].devnum;

		while ((rdev = rcu_dereference(conf->mirrors[disk].rdev)) == NULL ||
		       r10_bio->devs[slot].bio == IO_BLOCKED ||
		       !test_bit(In_sync, &rdev->flags)) {
			slot++;
			if (slot == conf->copies) {
				slot = 0;
				disk = -1;
				break;
			}
			disk = r10_bio->devs[slot].devnum;
		}
		goto rb_out;
	}


	/* make sure the disk is operational */
	slot = 0;
	disk = r10_bio->devs[slot].devnum;
	while ((rdev=rcu_dereference(conf->mirrors[disk].rdev)) == NULL ||
	       r10_bio->devs[slot].bio == IO_BLOCKED ||
	       !test_bit(In_sync, &rdev->flags)) {
		slot ++;
		if (slot == conf->copies) {
			disk = -1;
			goto rb_out;
		}
		disk = r10_bio->devs[slot].devnum;
	}


	current_distance = abs(r10_bio->devs[slot].addr -
			       conf->mirrors[disk].head_position);

	/* Find the disk whose head is closest,
	 * or - for far > 1 - find the closest to partition beginning */

	for (nslot = slot; nslot < conf->copies; nslot++) {
		int ndisk = r10_bio->devs[nslot].devnum;


		if ((rdev=rcu_dereference(conf->mirrors[ndisk].rdev)) == NULL ||
		    r10_bio->devs[nslot].bio == IO_BLOCKED ||
		    !test_bit(In_sync, &rdev->flags))
			continue;

		/* This optimisation is debatable, and completely destroys
		 * sequential read speed for 'far copies' arrays.  So only
		 * keep it for 'near' arrays, and review those later.
		 */
		if (conf->near_copies > 1 && !atomic_read(&rdev->nr_pending)) {
			disk = ndisk;
			slot = nslot;
			break;
		}

		/* for far > 1 always use the lowest address */
		if (conf->far_copies > 1)
			new_distance = r10_bio->devs[nslot].addr;
		else
			new_distance = abs(r10_bio->devs[nslot].addr -
					   conf->mirrors[ndisk].head_position);
		if (new_distance < current_distance) {
			current_distance = new_distance;
			disk = ndisk;
			slot = nslot;
		}
	}

rb_out:
	r10_bio->read_slot = slot;
/*	conf->next_seq_sect = this_sector + sectors;*/

	if (disk >= 0 && (rdev=rcu_dereference(conf->mirrors[disk].rdev))!= NULL)
		atomic_inc(&conf->mirrors[disk].rdev->nr_pending);
	else
		disk = -1;
	rcu_read_unlock();

	return disk;
}

static int raid10_congested(void *data, int bits)
{
	mddev_t *mddev = data;
	conf_t *conf = mddev->private;
	int i, ret = 0;

	if (mddev_congested(mddev, bits))
		return 1;
	rcu_read_lock();
	for (i = 0; i < conf->raid_disks && ret == 0; i++) {
		mdk_rdev_t *rdev = rcu_dereference(conf->mirrors[i].rdev);
		if (rdev && !test_bit(Faulty, &rdev->flags)) {
			struct request_queue *q = bdev_get_queue(rdev->bdev);

			ret |= bdi_congested(&q->backing_dev_info, bits);
		}
	}
	rcu_read_unlock();
	return ret;
}

static void flush_pending_writes(conf_t *conf)
{
	/* Any writes that have been queued but are awaiting
	 * bitmap updates get flushed here.
	 */
	spin_lock_irq(&conf->device_lock);

	if (conf->pending_bio_list.head) {
		struct bio *bio;
		bio = bio_list_get(&conf->pending_bio_list);
		spin_unlock_irq(&conf->device_lock);
		/* flush any pending bitmap writes to disk
		 * before proceeding w/ I/O */
		bitmap_unplug(conf->mddev->bitmap);

		while (bio) { /* submit pending writes */
			struct bio *next = bio->bi_next;
			bio->bi_next = NULL;
			generic_make_request(bio);
			bio = next;
		}
	} else
		spin_unlock_irq(&conf->device_lock);
}

/* Barriers....
 * Sometimes we need to suspend IO while we do something else,
 * either some resync/recovery, or reconfigure the array.
 * To do this we raise a 'barrier'.
 * The 'barrier' is a counter that can be raised multiple times
 * to count how many activities are happening which preclude
 * normal IO.
 * We can only raise the barrier if there is no pending IO.
 * i.e. if nr_pending == 0.
 * We choose only to raise the barrier if no-one is waiting for the
 * barrier to go down.  This means that as soon as an IO request
 * is ready, no other operations which require a barrier will start
 * until the IO request has had a chance.
 *
 * So: regular IO calls 'wait_barrier'.  When that returns there
 *    is no backgroup IO happening,  It must arrange to call
 *    allow_barrier when it has finished its IO.
 * backgroup IO calls must call raise_barrier.  Once that returns
 *    there is no normal IO happeing.  It must arrange to call
 *    lower_barrier when the particular background IO completes.
 */

static void raise_barrier(conf_t *conf, int force)
{
	BUG_ON(force && !conf->barrier);
	spin_lock_irq(&conf->resync_lock);

	/* Wait until no block IO is waiting (unless 'force') */
	wait_event_lock_irq(conf->wait_barrier, force || !conf->nr_waiting,
			    conf->resync_lock, );

	/* block any new IO from starting */
	conf->barrier++;

	/* Now wait for all pending IO to complete */
	wait_event_lock_irq(conf->wait_barrier,
			    !conf->nr_pending && conf->barrier < RESYNC_DEPTH,
			    conf->resync_lock, );

	spin_unlock_irq(&conf->resync_lock);
}

static void lower_barrier(conf_t *conf)
{
	unsigned long flags;
	spin_lock_irqsave(&conf->resync_lock, flags);
	conf->barrier--;
	spin_unlock_irqrestore(&conf->resync_lock, flags);
	wake_up(&conf->wait_barrier);
}

static void wait_barrier(conf_t *conf)
{
	spin_lock_irq(&conf->resync_lock);
	if (conf->barrier) {
		conf->nr_waiting++;
		wait_event_lock_irq(conf->wait_barrier, !conf->barrier,
				    conf->resync_lock,
				    );
		conf->nr_waiting--;
	}
	conf->nr_pending++;
	spin_unlock_irq(&conf->resync_lock);
}

static void allow_barrier(conf_t *conf)
{
	unsigned long flags;
	spin_lock_irqsave(&conf->resync_lock, flags);
	conf->nr_pending--;
	spin_unlock_irqrestore(&conf->resync_lock, flags);
	wake_up(&conf->wait_barrier);
}

static void freeze_array(conf_t *conf)
{
	/* stop syncio and normal IO and wait for everything to
	 * go quiet.
	 * We increment barrier and nr_waiting, and then
	 * wait until nr_pending match nr_queued+1
	 * This is called in the context of one normal IO request
	 * that has failed. Thus any sync request that might be pending
	 * will be blocked by nr_pending, and we need to wait for
	 * pending IO requests to complete or be queued for re-try.
	 * Thus the number queued (nr_queued) plus this request (1)
	 * must match the number of pending IOs (nr_pending) before
	 * we continue.
	 */
	spin_lock_irq(&conf->resync_lock);
	conf->barrier++;
	conf->nr_waiting++;
	wait_event_lock_irq(conf->wait_barrier,
			    conf->nr_pending == conf->nr_queued+1,
			    conf->resync_lock,
			    flush_pending_writes(conf));

	spin_unlock_irq(&conf->resync_lock);
}

static void unfreeze_array(conf_t *conf)
{
	/* reverse the effect of the freeze */
	spin_lock_irq(&conf->resync_lock);
	conf->barrier--;
	conf->nr_waiting--;
	wake_up(&conf->wait_barrier);
	spin_unlock_irq(&conf->resync_lock);
}

static int make_request(mddev_t *mddev, struct bio * bio)
{
	conf_t *conf = mddev->private;
	mirror_info_t *mirror;
	r10bio_t *r10_bio;
	struct bio *read_bio;
	int i;
	int chunk_sects = conf->chunk_mask + 1;
	const int rw = bio_data_dir(bio);
	const unsigned long do_sync = (bio->bi_rw & REQ_SYNC);
	const unsigned long do_fua = (bio->bi_rw & REQ_FUA);
	unsigned long flags;
	mdk_rdev_t *blocked_rdev;
	int plugged;

	if (unlikely(bio->bi_rw & REQ_FLUSH)) {
		md_flush_request(mddev, bio);
		return 0;
	}

	/* If this request crosses a chunk boundary, we need to
	 * split it.  This will only happen for 1 PAGE (or less) requests.
	 */
	if (unlikely( (bio->bi_sector & conf->chunk_mask) + (bio->bi_size >> 9)
		      > chunk_sects &&
		    conf->near_copies < conf->raid_disks)) {
		struct bio_pair *bp;
		/* Sanity check -- queue functions should prevent this happening */
		if (bio->bi_vcnt != 1 ||
		    bio->bi_idx != 0)
			goto bad_map;
		/* This is a one page bio that upper layers
		 * refuse to split for us, so we need to split it.
		 */
		bp = bio_split(bio,
			       chunk_sects - (bio->bi_sector & (chunk_sects - 1)) );

		/* Each of these 'make_request' calls will call 'wait_barrier'.
		 * If the first succeeds but the second blocks due to the resync
		 * thread raising the barrier, we will deadlock because the
		 * IO to the underlying device will be queued in generic_make_request
		 * and will never complete, so will never reduce nr_pending.
		 * So increment nr_waiting here so no new raise_barriers will
		 * succeed, and so the second wait_barrier cannot block.
		 */
		spin_lock_irq(&conf->resync_lock);
		conf->nr_waiting++;
		spin_unlock_irq(&conf->resync_lock);

		if (make_request(mddev, &bp->bio1))
			generic_make_request(&bp->bio1);
		if (make_request(mddev, &bp->bio2))
			generic_make_request(&bp->bio2);

		spin_lock_irq(&conf->resync_lock);
		conf->nr_waiting--;
		wake_up(&conf->wait_barrier);
		spin_unlock_irq(&conf->resync_lock);

		bio_pair_release(bp);
		return 0;
	bad_map:
		printk("md/raid10:%s: make_request bug: can't convert block across chunks"
		       " or bigger than %dk %llu %d\n", mdname(mddev), chunk_sects/2,
		       (unsigned long long)bio->bi_sector, bio->bi_size >> 10);

		bio_io_error(bio);
		return 0;
	}

	md_write_start(mddev, bio);

	/*
	 * Register the new request and wait if the reconstruction
	 * thread has put up a bar for new requests.
	 * Continue immediately if no resync is active currently.
	 */
	wait_barrier(conf);

	r10_bio = mempool_alloc(conf->r10bio_pool, GFP_NOIO);

	r10_bio->master_bio = bio;
	r10_bio->sectors = bio->bi_size >> 9;

	r10_bio->mddev = mddev;
	r10_bio->sector = bio->bi_sector;
	r10_bio->state = 0;

	if (rw == READ) {
		/*
		 * read balancing logic:
		 */
		int disk = read_balance(conf, r10_bio);
		int slot = r10_bio->read_slot;
		if (disk < 0) {
			raid_end_bio_io(r10_bio);
			return 0;
		}
		mirror = conf->mirrors + disk;

		read_bio = bio_clone_mddev(bio, GFP_NOIO, mddev);

		r10_bio->devs[slot].bio = read_bio;

		read_bio->bi_sector = r10_bio->devs[slot].addr +
			mirror->rdev->data_offset;
		read_bio->bi_bdev = mirror->rdev->bdev;
		read_bio->bi_end_io = raid10_end_read_request;
		read_bio->bi_rw = READ | do_sync;
		read_bio->bi_private = r10_bio;

		generic_make_request(read_bio);
		return 0;
	}

	/*
	 * WRITE:
	 */
	/* first select target devices under rcu_lock and
	 * inc refcount on their rdev.  Record them by setting
	 * bios[x] to bio
	 */
	plugged = mddev_check_plugged(mddev);

	raid10_find_phys(conf, r10_bio);
 retry_write:
	blocked_rdev = NULL;
	rcu_read_lock();
	for (i = 0;  i < conf->copies; i++) {
		int d = r10_bio->devs[i].devnum;
		mdk_rdev_t *rdev = rcu_dereference(conf->mirrors[d].rdev);
		if (rdev && unlikely(test_bit(Blocked, &rdev->flags))) {
			atomic_inc(&rdev->nr_pending);
			blocked_rdev = rdev;
			break;
		}
		if (rdev && !test_bit(Faulty, &rdev->flags)) {
			atomic_inc(&rdev->nr_pending);
			r10_bio->devs[i].bio = bio;
		} else {
			r10_bio->devs[i].bio = NULL;
			set_bit(R10BIO_Degraded, &r10_bio->state);
		}
	}
	rcu_read_unlock();

	if (unlikely(blocked_rdev)) {
		/* Have to wait for this device to get unblocked, then retry */
		int j;
		int d;

		for (j = 0; j < i; j++)
			if (r10_bio->devs[j].bio) {
				d = r10_bio->devs[j].devnum;
				rdev_dec_pending(conf->mirrors[d].rdev, mddev);
			}
		allow_barrier(conf);
		md_wait_for_blocked_rdev(blocked_rdev, mddev);
		wait_barrier(conf);
		goto retry_write;
	}

	atomic_set(&r10_bio->remaining, 1);
	bitmap_startwrite(mddev->bitmap, bio->bi_sector, r10_bio->sectors, 0);

	for (i = 0; i < conf->copies; i++) {
		struct bio *mbio;
		int d = r10_bio->devs[i].devnum;
		if (!r10_bio->devs[i].bio)
			continue;

		mbio = bio_clone_mddev(bio, GFP_NOIO, mddev);
		r10_bio->devs[i].bio = mbio;

		mbio->bi_sector	= r10_bio->devs[i].addr+
			conf->mirrors[d].rdev->data_offset;
		mbio->bi_bdev = conf->mirrors[d].rdev->bdev;
		mbio->bi_end_io	= raid10_end_write_request;
		mbio->bi_rw = WRITE | do_sync | do_fua;
		mbio->bi_private = r10_bio;

		atomic_inc(&r10_bio->remaining);
		spin_lock_irqsave(&conf->device_lock, flags);
		bio_list_add(&conf->pending_bio_list, mbio);
		spin_unlock_irqrestore(&conf->device_lock, flags);
	}

	if (atomic_dec_and_test(&r10_bio->remaining)) {
		/* This matches the end of raid10_end_write_request() */
		bitmap_endwrite(r10_bio->mddev->bitmap, r10_bio->sector,
				r10_bio->sectors,
				!test_bit(R10BIO_Degraded, &r10_bio->state),
				0);
		md_write_end(mddev);
		raid_end_bio_io(r10_bio);
	}

	/* In case raid10d snuck in to freeze_array */
	wake_up(&conf->wait_barrier);

	if (do_sync || !mddev->bitmap || !plugged)
		md_wakeup_thread(mddev->thread);
	return 0;
}

static void status(struct seq_file *seq, mddev_t *mddev)
{
	conf_t *conf = mddev->private;
	int i;

	if (conf->near_copies < conf->raid_disks)
		seq_printf(seq, " %dK chunks", mddev->chunk_sectors / 2);
	if (conf->near_copies > 1)
		seq_printf(seq, " %d near-copies", conf->near_copies);
	if (conf->far_copies > 1) {
		if (conf->far_offset)
			seq_printf(seq, " %d offset-copies", conf->far_copies);
		else
			seq_printf(seq, " %d far-copies", conf->far_copies);
	}
	seq_printf(seq, " [%d/%d] [", conf->raid_disks,
					conf->raid_disks - mddev->degraded);
	for (i = 0; i < conf->raid_disks; i++)
		seq_printf(seq, "%s",
			      conf->mirrors[i].rdev &&
			      test_bit(In_sync, &conf->mirrors[i].rdev->flags) ? "U" : "_");
	seq_printf(seq, "]");
}

static void error(mddev_t *mddev, mdk_rdev_t *rdev)
{
	char b[BDEVNAME_SIZE];
	conf_t *conf = mddev->private;

	/*
	 * If it is not operational, then we have already marked it as dead
	 * else if it is the last working disks, ignore the error, let the
	 * next level up know.
	 * else mark the drive as failed
	 */
	if (test_bit(In_sync, &rdev->flags)
	    && conf->raid_disks-mddev->degraded == 1)
		/*
		 * Don't fail the drive, just return an IO error.
		 * The test should really be more sophisticated than
		 * "working_disks == 1", but it isn't critical, and
		 * can wait until we do more sophisticated "is the drive
		 * really dead" tests...
		 */
		return;
	if (test_and_clear_bit(In_sync, &rdev->flags)) {
		unsigned long flags;
		spin_lock_irqsave(&conf->device_lock, flags);
		mddev->degraded++;
		spin_unlock_irqrestore(&conf->device_lock, flags);
		/*
		 * if recovery is running, make sure it aborts.
		 */
		set_bit(MD_RECOVERY_INTR, &mddev->recovery);
	}
	set_bit(Faulty, &rdev->flags);
	set_bit(MD_CHANGE_DEVS, &mddev->flags);
	printk(KERN_ALERT
	       "md/raid10:%s: Disk failure on %s, disabling device.\n"
	       "md/raid10:%s: Operation continuing on %d devices.\n",
	       mdname(mddev), bdevname(rdev->bdev, b),
	       mdname(mddev), conf->raid_disks - mddev->degraded);
}

static void print_conf(conf_t *conf)
{
	int i;
	mirror_info_t *tmp;

	printk(KERN_DEBUG "RAID10 conf printout:\n");
	if (!conf) {
		printk(KERN_DEBUG "(!conf)\n");
		return;
	}
	printk(KERN_DEBUG " --- wd:%d rd:%d\n", conf->raid_disks - conf->mddev->degraded,
		conf->raid_disks);

	for (i = 0; i < conf->raid_disks; i++) {
		char b[BDEVNAME_SIZE];
		tmp = conf->mirrors + i;
		if (tmp->rdev)
			printk(KERN_DEBUG " disk %d, wo:%d, o:%d, dev:%s\n",
				i, !test_bit(In_sync, &tmp->rdev->flags),
			        !test_bit(Faulty, &tmp->rdev->flags),
				bdevname(tmp->rdev->bdev,b));
	}
}

static void close_sync(conf_t *conf)
{
	wait_barrier(conf);
	allow_barrier(conf);

	mempool_destroy(conf->r10buf_pool);
	conf->r10buf_pool = NULL;
}

/* check if there are enough drives for
 * every block to appear on atleast one
 */
static int enough(conf_t *conf)
{
	int first = 0;

	do {
		int n = conf->copies;
		int cnt = 0;
		while (n--) {
			if (conf->mirrors[first].rdev)
				cnt++;
			first = (first+1) % conf->raid_disks;
		}
		if (cnt == 0)
			return 0;
	} while (first != 0);
	return 1;
}

static int raid10_spare_active(mddev_t *mddev)
{
	int i;
	conf_t *conf = mddev->private;
	mirror_info_t *tmp;
	int count = 0;
	unsigned long flags;

	/*
	 * Find all non-in_sync disks within the RAID10 configuration
	 * and mark them in_sync
	 */
	for (i = 0; i < conf->raid_disks; i++) {
		tmp = conf->mirrors + i;
		if (tmp->rdev
		    && !test_bit(Faulty, &tmp->rdev->flags)
		    && !test_and_set_bit(In_sync, &tmp->rdev->flags)) {
			count++;
			sysfs_notify_dirent(tmp->rdev->sysfs_state);
		}
	}
	spin_lock_irqsave(&conf->device_lock, flags);
	mddev->degraded -= count;
	spin_unlock_irqrestore(&conf->device_lock, flags);

	print_conf(conf);
	return count;
}


static int raid10_add_disk(mddev_t *mddev, mdk_rdev_t *rdev)
{
	conf_t *conf = mddev->private;
	int err = -EEXIST;
	int mirror;
	mirror_info_t *p;
	int first = 0;
	int last = conf->raid_disks - 1;

	if (mddev->recovery_cp < MaxSector)
		/* only hot-add to in-sync arrays, as recovery is
		 * very different from resync
		 */
		return -EBUSY;
	if (!enough(conf))
		return -EINVAL;

	if (rdev->raid_disk >= 0)
		first = last = rdev->raid_disk;

	if (rdev->saved_raid_disk >= 0 &&
	    rdev->saved_raid_disk >= first &&
	    conf->mirrors[rdev->saved_raid_disk].rdev == NULL)
		mirror = rdev->saved_raid_disk;
	else
		mirror = first;
	for ( ; mirror <= last ; mirror++)
		if ( !(p=conf->mirrors+mirror)->rdev) {

			disk_stack_limits(mddev->gendisk, rdev->bdev,
					  rdev->data_offset << 9);
			/* as we don't honour merge_bvec_fn, we must
			 * never risk violating it, so limit
			 * ->max_segments to one lying with a single
			 * page, as a one page request is never in
			 * violation.
			 */
			if (rdev->bdev->bd_disk->queue->merge_bvec_fn) {
				blk_queue_max_segments(mddev->queue, 1);
				blk_queue_segment_boundary(mddev->queue,
							   PAGE_CACHE_SIZE - 1);
			}

			p->head_position = 0;
			rdev->raid_disk = mirror;
			err = 0;
			if (rdev->saved_raid_disk != mirror)
				conf->fullsync = 1;
			rcu_assign_pointer(p->rdev, rdev);
			break;
		}

	md_integrity_add_rdev(rdev, mddev);
	print_conf(conf);
	return err;
}

static int raid10_remove_disk(mddev_t *mddev, int number)
{
	conf_t *conf = mddev->private;
	int err = 0;
	mdk_rdev_t *rdev;
	mirror_info_t *p = conf->mirrors+ number;

	print_conf(conf);
	rdev = p->rdev;
	if (rdev) {
		if (test_bit(In_sync, &rdev->flags) ||
		    atomic_read(&rdev->nr_pending)) {
			err = -EBUSY;
			goto abort;
		}
		/* Only remove faulty devices in recovery
		 * is not possible.
		 */
		if (!test_bit(Faulty, &rdev->flags) &&
		    enough(conf)) {
			err = -EBUSY;
			goto abort;
		}
		p->rdev = NULL;
		synchronize_rcu();
		if (atomic_read(&rdev->nr_pending)) {
			/* lost the race, try later */
			err = -EBUSY;
			p->rdev = rdev;
			goto abort;
		}
		err = md_integrity_register(mddev);
	}
abort:

	print_conf(conf);
	return err;
}


static void end_sync_read(struct bio *bio, int error)
{
	r10bio_t *r10_bio = bio->bi_private;
	conf_t *conf = r10_bio->mddev->private;
	int i,d;

	for (i=0; i<conf->copies; i++)
		if (r10_bio->devs[i].bio == bio)
			break;
	BUG_ON(i == conf->copies);
	update_head_pos(i, r10_bio);
	d = r10_bio->devs[i].devnum;

	if (test_bit(BIO_UPTODATE, &bio->bi_flags))
		set_bit(R10BIO_Uptodate, &r10_bio->state);
	else {
		atomic_add(r10_bio->sectors,
			   &conf->mirrors[d].rdev->corrected_errors);
		if (!test_bit(MD_RECOVERY_SYNC, &conf->mddev->recovery))
			md_error(r10_bio->mddev,
				 conf->mirrors[d].rdev);
	}

	/* for reconstruct, we always reschedule after a read.
	 * for resync, only after all reads
	 */
	rdev_dec_pending(conf->mirrors[d].rdev, conf->mddev);
	if (test_bit(R10BIO_IsRecover, &r10_bio->state) ||
	    atomic_dec_and_test(&r10_bio->remaining)) {
		/* we have read all the blocks,
		 * do the comparison in process context in raid10d
		 */
		reschedule_retry(r10_bio);
	}
}

static void end_sync_write(struct bio *bio, int error)
{
	int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
	r10bio_t *r10_bio = bio->bi_private;
	mddev_t *mddev = r10_bio->mddev;
	conf_t *conf = mddev->private;
	int i,d;

	for (i = 0; i < conf->copies; i++)
		if (r10_bio->devs[i].bio == bio)
			break;
	d = r10_bio->devs[i].devnum;

	if (!uptodate)
		md_error(mddev, conf->mirrors[d].rdev);

	update_head_pos(i, r10_bio);

	rdev_dec_pending(conf->mirrors[d].rdev, mddev);
	while (atomic_dec_and_test(&r10_bio->remaining)) {
		if (r10_bio->master_bio == NULL) {
			/* the primary of several recovery bios */
			sector_t s = r10_bio->sectors;
			put_buf(r10_bio);
			md_done_sync(mddev, s, 1);
			break;
		} else {
			r10bio_t *r10_bio2 = (r10bio_t *)r10_bio->master_bio;
			put_buf(r10_bio);
			r10_bio = r10_bio2;
		}
	}
}

/*
 * Note: sync and recover and handled very differently for raid10
 * This code is for resync.
 * For resync, we read through virtual addresses and read all blocks.
 * If there is any error, we schedule a write.  The lowest numbered
 * drive is authoritative.
 * However requests come for physical address, so we need to map.
 * For every physical address there are raid_disks/copies virtual addresses,
 * which is always are least one, but is not necessarly an integer.
 * This means that a physical address can span multiple chunks, so we may
 * have to submit multiple io requests for a single sync request.
 */
/*
 * We check if all blocks are in-sync and only write to blocks that
 * aren't in sync
 */
static void sync_request_write(mddev_t *mddev, r10bio_t *r10_bio)
{
	conf_t *conf = mddev->private;
	int i, first;
	struct bio *tbio, *fbio;

	atomic_set(&r10_bio->remaining, 1);

	/* find the first device with a block */
	for (i=0; i<conf->copies; i++)
		if (test_bit(BIO_UPTODATE, &r10_bio->devs[i].bio->bi_flags))
			break;

	if (i == conf->copies)
		goto done;

	first = i;
	fbio = r10_bio->devs[i].bio;

	/* now find blocks with errors */
	for (i=0 ; i < conf->copies ; i++) {
		int  j, d;
		int vcnt = r10_bio->sectors >> (PAGE_SHIFT-9);

		tbio = r10_bio->devs[i].bio;

		if (tbio->bi_end_io != end_sync_read)
			continue;
		if (i == first)
			continue;
		if (test_bit(BIO_UPTODATE, &r10_bio->devs[i].bio->bi_flags)) {
			/* We know that the bi_io_vec layout is the same for
			 * both 'first' and 'i', so we just compare them.
			 * All vec entries are PAGE_SIZE;
			 */
			for (j = 0; j < vcnt; j++)
				if (memcmp(page_address(fbio->bi_io_vec[j].bv_page),
					   page_address(tbio->bi_io_vec[j].bv_page),
					   PAGE_SIZE))
					break;
			if (j == vcnt)
				continue;
			mddev->resync_mismatches += r10_bio->sectors;
		}
		if (test_bit(MD_RECOVERY_CHECK, &mddev->recovery))
			/* Don't fix anything. */
			continue;
		/* Ok, we need to write this bio
		 * First we need to fixup bv_offset, bv_len and
		 * bi_vecs, as the read request might have corrupted these
		 */
		tbio->bi_vcnt = vcnt;
		tbio->bi_size = r10_bio->sectors << 9;
		tbio->bi_idx = 0;
		tbio->bi_phys_segments = 0;
		tbio->bi_flags &= ~(BIO_POOL_MASK - 1);
		tbio->bi_flags |= 1 << BIO_UPTODATE;
		tbio->bi_next = NULL;
		tbio->bi_rw = WRITE;
		tbio->bi_private = r10_bio;
		tbio->bi_sector = r10_bio->devs[i].addr;

		for (j=0; j < vcnt ; j++) {
			tbio->bi_io_vec[j].bv_offset = 0;
			tbio->bi_io_vec[j].bv_len = PAGE_SIZE;

			memcpy(page_address(tbio->bi_io_vec[j].bv_page),
			       page_address(fbio->bi_io_vec[j].bv_page),
			       PAGE_SIZE);
		}
		tbio->bi_end_io = end_sync_write;

		d = r10_bio->devs[i].devnum;
		atomic_inc(&conf->mirrors[d].rdev->nr_pending);
		atomic_inc(&r10_bio->remaining);
		md_sync_acct(conf->mirrors[d].rdev->bdev, tbio->bi_size >> 9);

		tbio->bi_sector += conf->mirrors[d].rdev->data_offset;
		tbio->bi_bdev = conf->mirrors[d].rdev->bdev;
		generic_make_request(tbio);
	}

done:
	if (atomic_dec_and_test(&r10_bio->remaining)) {
		md_done_sync(mddev, r10_bio->sectors, 1);
		put_buf(r10_bio);
	}
}

/*
 * Now for the recovery code.
 * Recovery happens across physical sectors.
 * We recover all non-is_sync drives by finding the virtual address of
 * each, and then choose a working drive that also has that virt address.
 * There is a separate r10_bio for each non-in_sync drive.
 * Only the first two slots are in use. The first for reading,
 * The second for writing.
 *
 */

static void recovery_request_write(mddev_t *mddev, r10bio_t *r10_bio)
{
	conf_t *conf = mddev->private;
	int i, d;
	struct bio *bio, *wbio;


	/* move the pages across to the second bio
	 * and submit the write request
	 */
	bio = r10_bio->devs[0].bio;
	wbio = r10_bio->devs[1].bio;
	for (i=0; i < wbio->bi_vcnt; i++) {
		struct page *p = bio->bi_io_vec[i].bv_page;
		bio->bi_io_vec[i].bv_page = wbio->bi_io_vec[i].bv_page;
		wbio->bi_io_vec[i].bv_page = p;
	}
	d = r10_bio->devs[1].devnum;

	atomic_inc(&conf->mirrors[d].rdev->nr_pending);
	md_sync_acct(conf->mirrors[d].rdev->bdev, wbio->bi_size >> 9);
	if (test_bit(R10BIO_Uptodate, &r10_bio->state))
		generic_make_request(wbio);
	else
		bio_endio(wbio, -EIO);
}


/*
 * Used by fix_read_error() to decay the per rdev read_errors.
 * We halve the read error count for every hour that has elapsed
 * since the last recorded read error.
 *
 */
static void check_decay_read_errors(mddev_t *mddev, mdk_rdev_t *rdev)
{
	struct timespec cur_time_mon;
	unsigned long hours_since_last;
	unsigned int read_errors = atomic_read(&rdev->read_errors);

	ktime_get_ts(&cur_time_mon);

	if (rdev->last_read_error.tv_sec == 0 &&
	    rdev->last_read_error.tv_nsec == 0) {
		/* first time we've seen a read error */
		rdev->last_read_error = cur_time_mon;
		return;
	}

	hours_since_last = (cur_time_mon.tv_sec -
			    rdev->last_read_error.tv_sec) / 3600;

	rdev->last_read_error = cur_time_mon;

	/*
	 * if hours_since_last is > the number of bits in read_errors
	 * just set read errors to 0. We do this to avoid
	 * overflowing the shift of read_errors by hours_since_last.
	 */
	if (hours_since_last >= 8 * sizeof(read_errors))
		atomic_set(&rdev->read_errors, 0);
	else
		atomic_set(&rdev->read_errors, read_errors >> hours_since_last);
}

/*
 * This is a kernel thread which:
 *
 *	1.	Retries failed read operations on working mirrors.
 *	2.	Updates the raid superblock when problems encounter.
 *	3.	Performs writes following reads for array synchronising.
 */

static void fix_read_error(conf_t *conf, mddev_t *mddev, r10bio_t *r10_bio)
{
	int sect = 0; /* Offset from r10_bio->sector */
	int sectors = r10_bio->sectors;
	mdk_rdev_t*rdev;
	int max_read_errors = atomic_read(&mddev->max_corr_read_errors);
	int d = r10_bio->devs[r10_bio->read_slot].devnum;

	rcu_read_lock();
	rdev = rcu_dereference(conf->mirrors[d].rdev);
	if (rdev) { /* If rdev is not NULL */
		char b[BDEVNAME_SIZE];
		int cur_read_error_count = 0;

		bdevname(rdev->bdev, b);

		if (test_bit(Faulty, &rdev->flags)) {
			rcu_read_unlock();
			/* drive has already been failed, just ignore any
			   more fix_read_error() attempts */
			return;
		}

		check_decay_read_errors(mddev, rdev);
		atomic_inc(&rdev->read_errors);
		cur_read_error_count = atomic_read(&rdev->read_errors);
		if (cur_read_error_count > max_read_errors) {
			rcu_read_unlock();
			printk(KERN_NOTICE
			       "md/raid10:%s: %s: Raid device exceeded "
			       "read_error threshold "
			       "[cur %d:max %d]\n",
			       mdname(mddev),
			       b, cur_read_error_count, max_read_errors);
			printk(KERN_NOTICE
			       "md/raid10:%s: %s: Failing raid "
			       "device\n", mdname(mddev), b);
			md_error(mddev, conf->mirrors[d].rdev);
			return;
		}
	}
	rcu_read_unlock();

	while(sectors) {
		int s = sectors;
		int sl = r10_bio->read_slot;
		int success = 0;
		int start;

		if (s > (PAGE_SIZE>>9))
			s = PAGE_SIZE >> 9;

		rcu_read_lock();
		do {
			d = r10_bio->devs[sl].devnum;
			rdev = rcu_dereference(conf->mirrors[d].rdev);
			if (rdev &&
			    test_bit(In_sync, &rdev->flags)) {
				atomic_inc(&rdev->nr_pending);
				rcu_read_unlock();
				success = sync_page_io(rdev,
						       r10_bio->devs[sl].addr +
						       sect,
						       s<<9,
						       conf->tmppage, READ, false);
				rdev_dec_pending(rdev, mddev);
				rcu_read_lock();
				if (success)
					break;
			}
			sl++;
			if (sl == conf->copies)
				sl = 0;
		} while (!success && sl != r10_bio->read_slot);
		rcu_read_unlock();

		if (!success) {
			/* Cannot read from anywhere -- bye bye array */
			int dn = r10_bio->devs[r10_bio->read_slot].devnum;
			md_error(mddev, conf->mirrors[dn].rdev);
			break;
		}

		start = sl;
		/* write it back and re-read */
		rcu_read_lock();
		while (sl != r10_bio->read_slot) {
			char b[BDEVNAME_SIZE];

			if (sl==0)
				sl = conf->copies;
			sl--;
			d = r10_bio->devs[sl].devnum;
			rdev = rcu_dereference(conf->mirrors[d].rdev);
			if (rdev &&
			    test_bit(In_sync, &rdev->flags)) {
				atomic_inc(&rdev->nr_pending);
				rcu_read_unlock();
				atomic_add(s, &rdev->corrected_errors);
				if (sync_page_io(rdev,
						 r10_bio->devs[sl].addr +
						 sect,
						 s<<9, conf->tmppage, WRITE, false)
				    == 0) {
					/* Well, this device is dead */
					printk(KERN_NOTICE
					       "md/raid10:%s: read correction "
					       "write failed"
					       " (%d sectors at %llu on %s)\n",
					       mdname(mddev), s,
					       (unsigned long long)(sect+
					       rdev->data_offset),
					       bdevname(rdev->bdev, b));
					printk(KERN_NOTICE "md/raid10:%s: %s: failing "
					       "drive\n",
					       mdname(mddev),
					       bdevname(rdev->bdev, b));
					md_error(mddev, rdev);
				}
				rdev_dec_pending(rdev, mddev);
				rcu_read_lock();
			}
		}
		sl = start;
		while (sl != r10_bio->read_slot) {

			if (sl==0)
				sl = conf->copies;
			sl--;
			d = r10_bio->devs[sl].devnum;
			rdev = rcu_dereference(conf->mirrors[d].rdev);
			if (rdev &&
			    test_bit(In_sync, &rdev->flags)) {
				char b[BDEVNAME_SIZE];
				atomic_inc(&rdev->nr_pending);
				rcu_read_unlock();
				if (sync_page_io(rdev,
						 r10_bio->devs[sl].addr +
						 sect,
						 s<<9, conf->tmppage,
						 READ, false) == 0) {
					/* Well, this device is dead */
					printk(KERN_NOTICE
					       "md/raid10:%s: unable to read back "
					       "corrected sectors"
					       " (%d sectors at %llu on %s)\n",
					       mdname(mddev), s,
					       (unsigned long long)(sect+
						    rdev->data_offset),
					       bdevname(rdev->bdev, b));
					printk(KERN_NOTICE "md/raid10:%s: %s: failing drive\n",
					       mdname(mddev),
					       bdevname(rdev->bdev, b));

					md_error(mddev, rdev);
				} else {
					printk(KERN_INFO
					       "md/raid10:%s: read error corrected"
					       " (%d sectors at %llu on %s)\n",
					       mdname(mddev), s,
					       (unsigned long long)(sect+
					            rdev->data_offset),
					       bdevname(rdev->bdev, b));
				}

				rdev_dec_pending(rdev, mddev);
				rcu_read_lock();
			}
		}
		rcu_read_unlock();

		sectors -= s;
		sect += s;
	}
}

static void raid10d(mddev_t *mddev)
{
	r10bio_t *r10_bio;
	struct bio *bio;
	unsigned long flags;
	conf_t *conf = mddev->private;
	struct list_head *head = &conf->retry_list;
	mdk_rdev_t *rdev;
	struct blk_plug plug;

	md_check_recovery(mddev);

	blk_start_plug(&plug);
	for (;;) {
		char b[BDEVNAME_SIZE];

		flush_pending_writes(conf);

		spin_lock_irqsave(&conf->device_lock, flags);
		if (list_empty(head)) {
			spin_unlock_irqrestore(&conf->device_lock, flags);
			break;
		}
		r10_bio = list_entry(head->prev, r10bio_t, retry_list);
		list_del(head->prev);
		conf->nr_queued--;
		spin_unlock_irqrestore(&conf->device_lock, flags);

		mddev = r10_bio->mddev;
		conf = mddev->private;
		if (test_bit(R10BIO_IsSync, &r10_bio->state))
			sync_request_write(mddev, r10_bio);
		else if (test_bit(R10BIO_IsRecover, &r10_bio->state))
			recovery_request_write(mddev, r10_bio);
		else {
			int mirror;
			/* we got a read error. Maybe the drive is bad.  Maybe just
			 * the block and we can fix it.
			 * We freeze all other IO, and try reading the block from
			 * other devices.  When we find one, we re-write
			 * and check it that fixes the read error.
			 * This is all done synchronously while the array is
			 * frozen.
			 */
			if (mddev->ro == 0) {
				freeze_array(conf);
				fix_read_error(conf, mddev, r10_bio);
				unfreeze_array(conf);
			}

			bio = r10_bio->devs[r10_bio->read_slot].bio;
			r10_bio->devs[r10_bio->read_slot].bio =
				mddev->ro ? IO_BLOCKED : NULL;
			mirror = read_balance(conf, r10_bio);
			if (mirror == -1) {
				printk(KERN_ALERT "md/raid10:%s: %s: unrecoverable I/O"
				       " read error for block %llu\n",
				       mdname(mddev),
				       bdevname(bio->bi_bdev,b),
				       (unsigned long long)r10_bio->sector);
				raid_end_bio_io(r10_bio);
				bio_put(bio);
			} else {
				const unsigned long do_sync = (r10_bio->master_bio->bi_rw & REQ_SYNC);
				bio_put(bio);
				rdev = conf->mirrors[mirror].rdev;
				if (printk_ratelimit())
					printk(KERN_ERR "md/raid10:%s: %s: redirecting sector %llu to"
					       " another mirror\n",
					       mdname(mddev),
					       bdevname(rdev->bdev,b),
					       (unsigned long long)r10_bio->sector);
				bio = bio_clone_mddev(r10_bio->master_bio,
						      GFP_NOIO, mddev);
				r10_bio->devs[r10_bio->read_slot].bio = bio;
				bio->bi_sector = r10_bio->devs[r10_bio->read_slot].addr
					+ rdev->data_offset;
				bio->bi_bdev = rdev->bdev;
				bio->bi_rw = READ | do_sync;
				bio->bi_private = r10_bio;
				bio->bi_end_io = raid10_end_read_request;
				generic_make_request(bio);
			}
		}
		cond_resched();
	}
	blk_finish_plug(&plug);
}


static int init_resync(conf_t *conf)
{
	int buffs;

	buffs = RESYNC_WINDOW / RESYNC_BLOCK_SIZE;
	BUG_ON(conf->r10buf_pool);
	conf->r10buf_pool = mempool_create(buffs, r10buf_pool_alloc, r10buf_pool_free, conf);
	if (!conf->r10buf_pool)
		return -ENOMEM;
	conf->next_resync = 0;
	return 0;
}

/*
 * perform a "sync" on one "block"
 *
 * We need to make sure that no normal I/O request - particularly write
 * requests - conflict with active sync requests.
 *
 * This is achieved by tracking pending requests and a 'barrier' concept
 * that can be installed to exclude normal IO requests.
 *
 * Resync and recovery are handled very differently.
 * We differentiate by looking at MD_RECOVERY_SYNC in mddev->recovery.
 *
 * For resync, we iterate over virtual addresses, read all copies,
 * and update if there are differences.  If only one copy is live,
 * skip it.
 * For recovery, we iterate over physical addresses, read a good
 * value for each non-in_sync drive, and over-write.
 *
 * So, for recovery we may have several outstanding complex requests for a
 * given address, one for each out-of-sync device.  We model this by allocating
 * a number of r10_bio structures, one for each out-of-sync device.
 * As we setup these structures, we collect all bio's together into a list
 * which we then process collectively to add pages, and then process again
 * to pass to generic_make_request.
 *
 * The r10_bio structures are linked using a borrowed master_bio pointer.
 * This link is counted in ->remaining.  When the r10_bio that points to NULL
 * has its remaining count decremented to 0, the whole complex operation
 * is complete.
 *
 */

static sector_t sync_request(mddev_t *mddev, sector_t sector_nr, int *skipped, int go_faster)
{
	conf_t *conf = mddev->private;
	r10bio_t *r10_bio;
	struct bio *biolist = NULL, *bio;
	sector_t max_sector, nr_sectors;
	int disk;
	int i;
	int max_sync;
	sector_t sync_blocks;

	sector_t sectors_skipped = 0;
	int chunks_skipped = 0;

	if (!conf->r10buf_pool)
		if (init_resync(conf))
			return 0;

 skipped:
	max_sector = mddev->dev_sectors;
	if (test_bit(MD_RECOVERY_SYNC, &mddev->recovery))
		max_sector = mddev->resync_max_sectors;
	if (sector_nr >= max_sector) {
		/* If we aborted, we need to abort the
		 * sync on the 'current' bitmap chucks (there can
		 * be several when recovering multiple devices).
		 * as we may have started syncing it but not finished.
		 * We can find the current address in
		 * mddev->curr_resync, but for recovery,
		 * we need to convert that to several
		 * virtual addresses.
		 */
		if (mddev->curr_resync < max_sector) { /* aborted */
			if (test_bit(MD_RECOVERY_SYNC, &mddev->recovery))
				bitmap_end_sync(mddev->bitmap, mddev->curr_resync,
						&sync_blocks, 1);
			else for (i=0; i<conf->raid_disks; i++) {
				sector_t sect =
					raid10_find_virt(conf, mddev->curr_resync, i);
				bitmap_end_sync(mddev->bitmap, sect,
						&sync_blocks, 1);
			}
		} else /* completed sync */
			conf->fullsync = 0;

		bitmap_close_sync(mddev->bitmap);
		close_sync(conf);
		*skipped = 1;
		return sectors_skipped;
	}
	if (chunks_skipped >= conf->raid_disks) {
		/* if there has been nothing to do on any drive,
		 * then there is nothing to do at all..
		 */
		*skipped = 1;
		return (max_sector - sector_nr) + sectors_skipped;
	}

	if (max_sector > mddev->resync_max)
		max_sector = mddev->resync_max; /* Don't do IO beyond here */

	/* make sure whole request will fit in a chunk - if chunks
	 * are meaningful
	 */
	if (conf->near_copies < conf->raid_disks &&
	    max_sector > (sector_nr | conf->chunk_mask))
		max_sector = (sector_nr | conf->chunk_mask) + 1;
	/*
	 * If there is non-resync activity waiting for us then
	 * put in a delay to throttle resync.
	 */
	if (!go_faster && conf->nr_waiting)
		msleep_interruptible(1000);

	/* Again, very different code for resync and recovery.
	 * Both must result in an r10bio with a list of bios that
	 * have bi_end_io, bi_sector, bi_bdev set,
	 * and bi_private set to the r10bio.
	 * For recovery, we may actually create several r10bios
	 * with 2 bios in each, that correspond to the bios in the main one.
	 * In this case, the subordinate r10bios link back through a
	 * borrowed master_bio pointer, and the counter in the master
	 * includes a ref from each subordinate.
	 */
	/* First, we decide what to do and set ->bi_end_io
	 * To end_sync_read if we want to read, and
	 * end_sync_write if we will want to write.
	 */

	max_sync = RESYNC_PAGES << (PAGE_SHIFT-9);
	if (!test_bit(MD_RECOVERY_SYNC, &mddev->recovery)) {
		/* recovery... the complicated one */
		int j, k;
		r10_bio = NULL;

		for (i=0 ; i<conf->raid_disks; i++)
			if (conf->mirrors[i].rdev &&
			    !test_bit(In_sync, &conf->mirrors[i].rdev->flags)) {
				int still_degraded = 0;
				/* want to reconstruct this device */
				r10bio_t *rb2 = r10_bio;
				sector_t sect = raid10_find_virt(conf, sector_nr, i);
				int must_sync;
				/* Unless we are doing a full sync, we only need
				 * to recover the block if it is set in the bitmap
				 */
				must_sync = bitmap_start_sync(mddev->bitmap, sect,
							      &sync_blocks, 1);
				if (sync_blocks < max_sync)
					max_sync = sync_blocks;
				if (!must_sync &&
				    !conf->fullsync) {
					/* yep, skip the sync_blocks here, but don't assume
					 * that there will never be anything to do here
					 */
					chunks_skipped = -1;
					continue;
				}

				r10_bio = mempool_alloc(conf->r10buf_pool, GFP_NOIO);
				raise_barrier(conf, rb2 != NULL);
				atomic_set(&r10_bio->remaining, 0);

				r10_bio->master_bio = (struct bio*)rb2;
				if (rb2)
					atomic_inc(&rb2->remaining);
				r10_bio->mddev = mddev;
				set_bit(R10BIO_IsRecover, &r10_bio->state);
				r10_bio->sector = sect;

				raid10_find_phys(conf, r10_bio);

				/* Need to check if the array will still be
				 * degraded
				 */
				for (j=0; j<conf->raid_disks; j++)
					if (conf->mirrors[j].rdev == NULL ||
					    test_bit(Faulty, &conf->mirrors[j].rdev->flags)) {
						still_degraded = 1;
						break;
					}

				must_sync = bitmap_start_sync(mddev->bitmap, sect,
							      &sync_blocks, still_degraded);

				for (j=0; j<conf->copies;j++) {
					int d = r10_bio->devs[j].devnum;
					if (conf->mirrors[d].rdev &&
					    test_bit(In_sync, &conf->mirrors[d].rdev->flags)) {
						/* This is where we read from */
						bio = r10_bio->devs[0].bio;
						bio->bi_next = biolist;
						biolist = bio;
						bio->bi_private = r10_bio;
						bio->bi_end_io = end_sync_read;
						bio->bi_rw = READ;
						bio->bi_sector = r10_bio->devs[j].addr +
							conf->mirrors[d].rdev->data_offset;
						bio->bi_bdev = conf->mirrors[d].rdev->bdev;
						atomic_inc(&conf->mirrors[d].rdev->nr_pending);
						atomic_inc(&r10_bio->remaining);
						/* and we write to 'i' */

						for (k=0; k<conf->copies; k++)
							if (r10_bio->devs[k].devnum == i)
								break;
						BUG_ON(k == conf->copies);
						bio = r10_bio->devs[1].bio;
						bio->bi_next = biolist;
						biolist = bio;
						bio->bi_private = r10_bio;
						bio->bi_end_io = end_sync_write;
						bio->bi_rw = WRITE;
						bio->bi_sector = r10_bio->devs[k].addr +
							conf->mirrors[i].rdev->data_offset;
						bio->bi_bdev = conf->mirrors[i].rdev->bdev;

						r10_bio->devs[0].devnum = d;
						r10_bio->devs[1].devnum = i;

						break;
					}
				}
				if (j == conf->copies) {
					/* Cannot recover, so abort the recovery */
					put_buf(r10_bio);
					if (rb2)
						atomic_dec(&rb2->remaining);
					r10_bio = rb2;
					if (!test_and_set_bit(MD_RECOVERY_INTR,
							      &mddev->recovery))
						printk(KERN_INFO "md/raid10:%s: insufficient "
						       "working devices for recovery.\n",
						       mdname(mddev));
					break;
				}
			}
		if (biolist == NULL) {
			while (r10_bio) {
				r10bio_t *rb2 = r10_bio;
				r10_bio = (r10bio_t*) rb2->master_bio;
				rb2->master_bio = NULL;
				put_buf(rb2);
			}
			goto giveup;
		}
	} else {
		/* resync. Schedule a read for every block at this virt offset */
		int count = 0;

		bitmap_cond_end_sync(mddev->bitmap, sector_nr);

		if (!bitmap_start_sync(mddev->bitmap, sector_nr,
				       &sync_blocks, mddev->degraded) &&
		    !conf->fullsync && !test_bit(MD_RECOVERY_REQUESTED, &mddev->recovery)) {
			/* We can skip this block */
			*skipped = 1;
			return sync_blocks + sectors_skipped;
		}
		if (sync_blocks < max_sync)
			max_sync = sync_blocks;
		r10_bio = mempool_alloc(conf->r10buf_pool, GFP_NOIO);

		r10_bio->mddev = mddev;
		atomic_set(&r10_bio->remaining, 0);
		raise_barrier(conf, 0);
		conf->next_resync = sector_nr;

		r10_bio->master_bio = NULL;
		r10_bio->sector = sector_nr;
		set_bit(R10BIO_IsSync, &r10_bio->state);
		raid10_find_phys(conf, r10_bio);
		r10_bio->sectors = (sector_nr | conf->chunk_mask) - sector_nr +1;

		for (i=0; i<conf->copies; i++) {
			int d = r10_bio->devs[i].devnum;
			bio = r10_bio->devs[i].bio;
			bio->bi_end_io = NULL;
			clear_bit(BIO_UPTODATE, &bio->bi_flags);
			if (conf->mirrors[d].rdev == NULL ||
			    test_bit(Faulty, &conf->mirrors[d].rdev->flags))
				continue;
			atomic_inc(&conf->mirrors[d].rdev->nr_pending);
			atomic_inc(&r10_bio->remaining);
			bio->bi_next = biolist;
			biolist = bio;
			bio->bi_private = r10_bio;
			bio->bi_end_io = end_sync_read;
			bio->bi_rw = READ;
			bio->bi_sector = r10_bio->devs[i].addr +
				conf->mirrors[d].rdev->data_offset;
			bio->bi_bdev = conf->mirrors[d].rdev->bdev;
			count++;
		}

		if (count < 2) {
			for (i=0; i<conf->copies; i++) {
				int d = r10_bio->devs[i].devnum;
				if (r10_bio->devs[i].bio->bi_end_io)
					rdev_dec_pending(conf->mirrors[d].rdev, mddev);
			}
			put_buf(r10_bio);
			biolist = NULL;
			goto giveup;
		}
	}

	for (bio = biolist; bio ; bio=bio->bi_next) {

		bio->bi_flags &= ~(BIO_POOL_MASK - 1);
		if (bio->bi_end_io)
			bio->bi_flags |= 1 << BIO_UPTODATE;
		bio->bi_vcnt = 0;
		bio->bi_idx = 0;
		bio->bi_phys_segments = 0;
		bio->bi_size = 0;
	}

	nr_sectors = 0;
	if (sector_nr + max_sync < max_sector)
		max_sector = sector_nr + max_sync;
	do {
		struct page *page;
		int len = PAGE_SIZE;
		disk = 0;
		if (sector_nr + (len>>9) > max_sector)
			len = (max_sector - sector_nr) << 9;
		if (len == 0)
			break;
		for (bio= biolist ; bio ; bio=bio->bi_next) {
			page = bio->bi_io_vec[bio->bi_vcnt].bv_page;
			if (bio_add_page(bio, page, len, 0) == 0) {
				/* stop here */
				struct bio *bio2;
				bio->bi_io_vec[bio->bi_vcnt].bv_page = page;
				for (bio2 = biolist; bio2 && bio2 != bio; bio2 = bio2->bi_next) {
					/* remove last page from this bio */
					bio2->bi_vcnt--;
					bio2->bi_size -= len;
					bio2->bi_flags &= ~(1<< BIO_SEG_VALID);
				}
				goto bio_full;
			}
			disk = i;
		}
		nr_sectors += len>>9;
		sector_nr += len>>9;
	} while (biolist->bi_vcnt < RESYNC_PAGES);
 bio_full:
	r10_bio->sectors = nr_sectors;

	while (biolist) {
		bio = biolist;
		biolist = biolist->bi_next;

		bio->bi_next = NULL;
		r10_bio = bio->bi_private;
		r10_bio->sectors = nr_sectors;

		if (bio->bi_end_io == end_sync_read) {
			md_sync_acct(bio->bi_bdev, nr_sectors);
			generic_make_request(bio);
		}
	}

	if (sectors_skipped)
		/* pretend they weren't skipped, it makes
		 * no important difference in this case
		 */
		md_done_sync(mddev, sectors_skipped, 1);

	return sectors_skipped + nr_sectors;
 giveup:
	/* There is nowhere to write, so all non-sync
	 * drives must be failed, so try the next chunk...
	 */
	if (sector_nr + max_sync < max_sector)
		max_sector = sector_nr + max_sync;

	sectors_skipped += (max_sector - sector_nr);
	chunks_skipped ++;
	sector_nr = max_sector;
	goto skipped;
}

static sector_t
raid10_size(mddev_t *mddev, sector_t sectors, int raid_disks)
{
	sector_t size;
	conf_t *conf = mddev->private;

	if (!raid_disks)
		raid_disks = conf->raid_disks;
	if (!sectors)
		sectors = conf->dev_sectors;

	size = sectors >> conf->chunk_shift;
	sector_div(size, conf->far_copies);
	size = size * raid_disks;
	sector_div(size, conf->near_copies);

	return size << conf->chunk_shift;
}


static conf_t *setup_conf(mddev_t *mddev)
{
	conf_t *conf = NULL;
	int nc, fc, fo;
	sector_t stride, size;
	int err = -EINVAL;

	if (mddev->new_chunk_sectors < (PAGE_SIZE >> 9) ||
	    !is_power_of_2(mddev->new_chunk_sectors)) {
		printk(KERN_ERR "md/raid10:%s: chunk size must be "
		       "at least PAGE_SIZE(%ld) and be a power of 2.\n",
		       mdname(mddev), PAGE_SIZE);
		goto out;
	}

	nc = mddev->new_layout & 255;
	fc = (mddev->new_layout >> 8) & 255;
	fo = mddev->new_layout & (1<<16);

	if ((nc*fc) <2 || (nc*fc) > mddev->raid_disks ||
	    (mddev->new_layout >> 17)) {
		printk(KERN_ERR "md/raid10:%s: unsupported raid10 layout: 0x%8x\n",
		       mdname(mddev), mddev->new_layout);
		goto out;
	}

	err = -ENOMEM;
	conf = kzalloc(sizeof(conf_t), GFP_KERNEL);
	if (!conf)
		goto out;

	conf->mirrors = kzalloc(sizeof(struct mirror_info)*mddev->raid_disks,
				GFP_KERNEL);
	if (!conf->mirrors)
		goto out;

	conf->tmppage = alloc_page(GFP_KERNEL);
	if (!conf->tmppage)
		goto out;


	conf->raid_disks = mddev->raid_disks;
	conf->near_copies = nc;
	conf->far_copies = fc;
	conf->copies = nc*fc;
	conf->far_offset = fo;
	conf->chunk_mask = mddev->new_chunk_sectors - 1;
	conf->chunk_shift = ffz(~mddev->new_chunk_sectors);

	conf->r10bio_pool = mempool_create(NR_RAID10_BIOS, r10bio_pool_alloc,
					   r10bio_pool_free, conf);
	if (!conf->r10bio_pool)
		goto out;

	size = mddev->dev_sectors >> conf->chunk_shift;
	sector_div(size, fc);
	size = size * conf->raid_disks;
	sector_div(size, nc);
	/* 'size' is now the number of chunks in the array */
	/* calculate "used chunks per device" in 'stride' */
	stride = size * conf->copies;

	/* We need to round up when dividing by raid_disks to
	 * get the stride size.
	 */
	stride += conf->raid_disks - 1;
	sector_div(stride, conf->raid_disks);

	conf->dev_sectors = stride << conf->chunk_shift;

	if (fo)
		stride = 1;
	else
		sector_div(stride, fc);
	conf->stride = stride << conf->chunk_shift;


	spin_lock_init(&conf->device_lock);
	INIT_LIST_HEAD(&conf->retry_list);

	spin_lock_init(&conf->resync_lock);
	init_waitqueue_head(&conf->wait_barrier);

	conf->thread = md_register_thread(raid10d, mddev, NULL);
	if (!conf->thread)
		goto out;

	conf->mddev = mddev;
	return conf;

 out:
	printk(KERN_ERR "md/raid10:%s: couldn't allocate memory.\n",
	       mdname(mddev));
	if (conf) {
		if (conf->r10bio_pool)
			mempool_destroy(conf->r10bio_pool);
		kfree(conf->mirrors);
		safe_put_page(conf->tmppage);
		kfree(conf);
	}
	return ERR_PTR(err);
}

static int run(mddev_t *mddev)
{
	conf_t *conf;
	int i, disk_idx, chunk_size;
	mirror_info_t *disk;
	mdk_rdev_t *rdev;
	sector_t size;

	/*
	 * copy the already verified devices into our private RAID10
	 * bookkeeping area. [whatever we allocate in run(),
	 * should be freed in stop()]
	 */

	if (mddev->private == NULL) {
		conf = setup_conf(mddev);
		if (IS_ERR(conf))
			return PTR_ERR(conf);
		mddev->private = conf;
	}
	conf = mddev->private;
	if (!conf)
		goto out;

	mddev->thread = conf->thread;
	conf->thread = NULL;

	chunk_size = mddev->chunk_sectors << 9;
	blk_queue_io_min(mddev->queue, chunk_size);
	if (conf->raid_disks % conf->near_copies)
		blk_queue_io_opt(mddev->queue, chunk_size * conf->raid_disks);
	else
		blk_queue_io_opt(mddev->queue, chunk_size *
				 (conf->raid_disks / conf->near_copies));

	list_for_each_entry(rdev, &mddev->disks, same_set) {
		disk_idx = rdev->raid_disk;
		if (disk_idx >= conf->raid_disks
		    || disk_idx < 0)
			continue;
		disk = conf->mirrors + disk_idx;

		disk->rdev = rdev;
		disk_stack_limits(mddev->gendisk, rdev->bdev,
				  rdev->data_offset << 9);
		/* as we don't honour merge_bvec_fn, we must never risk
		 * violating it, so limit max_segments to 1 lying
		 * within a single page.
		 */
		if (rdev->bdev->bd_disk->queue->merge_bvec_fn) {
			blk_queue_max_segments(mddev->queue, 1);
			blk_queue_segment_boundary(mddev->queue,
						   PAGE_CACHE_SIZE - 1);
		}

		disk->head_position = 0;
	}
	/* need to check that every block has at least one working mirror */
	if (!enough(conf)) {
		printk(KERN_ERR "md/raid10:%s: not enough operational mirrors.\n",
		       mdname(mddev));
		goto out_free_conf;
	}

	mddev->degraded = 0;
	for (i = 0; i < conf->raid_disks; i++) {

		disk = conf->mirrors + i;

		if (!disk->rdev ||
		    !test_bit(In_sync, &disk->rdev->flags)) {
			disk->head_position = 0;
			mddev->degraded++;
			if (disk->rdev)
				conf->fullsync = 1;
		}
	}

	if (mddev->recovery_cp != MaxSector)
		printk(KERN_NOTICE "md/raid10:%s: not clean"
		       " -- starting background reconstruction\n",
		       mdname(mddev));
	printk(KERN_INFO
		"md/raid10:%s: active with %d out of %d devices\n",
		mdname(mddev), conf->raid_disks - mddev->degraded,
		conf->raid_disks);
	/*
	 * Ok, everything is just fine now
	 */
	mddev->dev_sectors = conf->dev_sectors;
	size = raid10_size(mddev, 0, 0);
	md_set_array_sectors(mddev, size);
	mddev->resync_max_sectors = size;

	mddev->queue->backing_dev_info.congested_fn = raid10_congested;
	mddev->queue->backing_dev_info.congested_data = mddev;

	/* Calculate max read-ahead size.
	 * We need to readahead at least twice a whole stripe....
	 * maybe...
	 */
	{
		int stripe = conf->raid_disks *
			((mddev->chunk_sectors << 9) / PAGE_SIZE);
		stripe /= conf->near_copies;
		if (mddev->queue->backing_dev_info.ra_pages < 2* stripe)
			mddev->queue->backing_dev_info.ra_pages = 2* stripe;
	}

	if (conf->near_copies < conf->raid_disks)
		blk_queue_merge_bvec(mddev->queue, raid10_mergeable_bvec);

	if (md_integrity_register(mddev))
		goto out_free_conf;

	return 0;

out_free_conf:
	md_unregister_thread(mddev->thread);
	if (conf->r10bio_pool)
		mempool_destroy(conf->r10bio_pool);
	safe_put_page(conf->tmppage);
	kfree(conf->mirrors);
	kfree(conf);
	mddev->private = NULL;
out:
	return -EIO;
}

static int stop(mddev_t *mddev)
{
	conf_t *conf = mddev->private;

	raise_barrier(conf, 0);
	lower_barrier(conf);

	md_unregister_thread(mddev->thread);
	mddev->thread = NULL;
	blk_sync_queue(mddev->queue); /* the unplug fn references 'conf'*/
	if (conf->r10bio_pool)
		mempool_destroy(conf->r10bio_pool);
	kfree(conf->mirrors);
	kfree(conf);
	mddev->private = NULL;
	return 0;
}

static void raid10_quiesce(mddev_t *mddev, int state)
{
	conf_t *conf = mddev->private;

	switch(state) {
	case 1:
		raise_barrier(conf, 0);
		break;
	case 0:
		lower_barrier(conf);
		break;
	}
}

static void *raid10_takeover_raid0(mddev_t *mddev)
{
	mdk_rdev_t *rdev;
	conf_t *conf;

	if (mddev->degraded > 0) {
		printk(KERN_ERR "md/raid10:%s: Error: degraded raid0!\n",
		       mdname(mddev));
		return ERR_PTR(-EINVAL);
	}

	/* Set new parameters */
	mddev->new_level = 10;
	/* new layout: far_copies = 1, near_copies = 2 */
	mddev->new_layout = (1<<8) + 2;
	mddev->new_chunk_sectors = mddev->chunk_sectors;
	mddev->delta_disks = mddev->raid_disks;
	mddev->raid_disks *= 2;
	/* make sure it will be not marked as dirty */
	mddev->recovery_cp = MaxSector;

	conf = setup_conf(mddev);
	if (!IS_ERR(conf)) {
		list_for_each_entry(rdev, &mddev->disks, same_set)
			if (rdev->raid_disk >= 0)
				rdev->new_raid_disk = rdev->raid_disk * 2;
		conf->barrier = 1;
	}

	return conf;
}

static void *raid10_takeover(mddev_t *mddev)
{
	struct raid0_private_data *raid0_priv;

	/* raid10 can take over:
	 *  raid0 - providing it has only two drives
	 */
	if (mddev->level == 0) {
		/* for raid0 takeover only one zone is supported */
		raid0_priv = mddev->private;
		if (raid0_priv->nr_strip_zones > 1) {
			printk(KERN_ERR "md/raid10:%s: cannot takeover raid 0"
			       " with more than one zone.\n",
			       mdname(mddev));
			return ERR_PTR(-EINVAL);
		}
		return raid10_takeover_raid0(mddev);
	}
	return ERR_PTR(-EINVAL);
}

static struct mdk_personality raid10_personality =
{
	.name		= "raid10",
	.level		= 10,
	.owner		= THIS_MODULE,
	.make_request	= make_request,
	.run		= run,
	.stop		= stop,
	.status		= status,
	.error_handler	= error,
	.hot_add_disk	= raid10_add_disk,
	.hot_remove_disk= raid10_remove_disk,
	.spare_active	= raid10_spare_active,
	.sync_request	= sync_request,
	.quiesce	= raid10_quiesce,
	.size		= raid10_size,
	.takeover	= raid10_takeover,
};

static int __init raid_init(void)
{
	return register_md_personality(&raid10_personality);
}

static void raid_exit(void)
{
	unregister_md_personality(&raid10_personality);
}

module_init(raid_init);
module_exit(raid_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RAID10 (striped mirror) personality for MD");
MODULE_ALIAS("md-personality-9"); /* RAID10 */
MODULE_ALIAS("md-raid10");
MODULE_ALIAS("md-level-10");
