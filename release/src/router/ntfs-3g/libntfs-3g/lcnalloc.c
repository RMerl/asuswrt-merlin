/**
 * lcnalloc.c - Cluster (de)allocation code. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2002-2004 Anton Altaparmakov
 * Copyright (c) 2004 Yura Pakhuchiy
 * Copyright (c) 2004-2008 Szabolcs Szakacsits
 * Copyright (c) 2008-2009 Jean-Pierre Andre
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "types.h"
#include "attrib.h"
#include "bitmap.h"
#include "debug.h"
#include "runlist.h"
#include "volume.h"
#include "lcnalloc.h"
#include "logging.h"
#include "misc.h"

/*
 * Plenty possibilities for big optimizations all over in the cluster
 * allocation, however at the moment the dominant bottleneck (~ 90%) is 
 * the update of the mapping pairs which converges to the cubic Faulhaber's
 * formula as the function of the number of extents (fragments, runs).
 */
#define NTFS_LCNALLOC_BSIZE 4096
#define NTFS_LCNALLOC_SKIP  NTFS_LCNALLOC_BSIZE

enum {
	ZONE_MFT = 1,
	ZONE_DATA1 = 2,
	ZONE_DATA2 = 4
} ;

static void ntfs_cluster_set_zone_pos(LCN start, LCN end, LCN *pos, LCN tc)
{
	ntfs_log_trace("pos: %lld  tc: %lld\n", (long long)*pos, (long long)tc);

	if (tc >= end)
		*pos = start;
	else if (tc >= start)
		*pos = tc;
}

static void ntfs_cluster_update_zone_pos(ntfs_volume *vol, u8 zone, LCN tc)
{
	ntfs_log_trace("tc = %lld, zone = %d\n", (long long)tc, zone);
	
	if (zone == ZONE_MFT)
		ntfs_cluster_set_zone_pos(vol->mft_lcn, vol->mft_zone_end,
					  &vol->mft_zone_pos, tc);
	else if (zone == ZONE_DATA1)
		ntfs_cluster_set_zone_pos(vol->mft_zone_end, vol->nr_clusters,
					  &vol->data1_zone_pos, tc);
	else /* zone == ZONE_DATA2 */
		ntfs_cluster_set_zone_pos(0, vol->mft_zone_start, 
					  &vol->data2_zone_pos, tc);
}

/*
 *		Unmark full zones when a cluster has been freed in a full zone
 *
 *	Next allocation will reuse the freed cluster
 */

static void update_full_status(ntfs_volume *vol, LCN lcn)
{
	if (lcn >= vol->mft_zone_end) {
		if (vol->full_zones & ZONE_DATA1) {
			ntfs_cluster_update_zone_pos(vol, ZONE_DATA1, lcn);
			vol->full_zones &= ~ZONE_DATA1;
		}
	} else
		if (lcn < vol->mft_zone_start) {
			if (vol->full_zones & ZONE_DATA2) {
				ntfs_cluster_update_zone_pos(vol, ZONE_DATA2, lcn);
				vol->full_zones &= ~ZONE_DATA2;
			}
		} else {
			if (vol->full_zones & ZONE_MFT) {
				ntfs_cluster_update_zone_pos(vol, ZONE_MFT, lcn);
				vol->full_zones &= ~ZONE_MFT;
			}
		}
}
 
static s64 max_empty_bit_range(unsigned char *buf, int size)
{
	int i, j, run = 0;
	int max_range = 0;
	s64 start_pos = -1;
	
	ntfs_log_trace("Entering\n");
	
	i = 0;
	while (i < size) {
		switch (*buf) {
		case 0 :
			do {
				buf++;
				run += 8;
				i++;
			} while ((i < size) && !*buf);
			break;
		case 255 :
			if (run > max_range) {
				max_range = run;
				start_pos = (s64)i * 8 - run;
			}
			run = 0;
			do {
				buf++;
				i++;
			} while ((i < size) && (*buf == 255));
			break;
		default :
			for (j = 0; j < 8; j++) {
			
				int bit = *buf & (1 << j);
		
				if (bit) {
					if (run > max_range) {
						max_range = run;
						start_pos = (s64)i * 8 + (j - run);
					}
					run = 0;
				} else 
					run++;
			}
			i++;
			buf++;
		
		}
	}
	
	if (run > max_range)
		start_pos = (s64)i * 8 - run;
	
	return start_pos;
}

static int bitmap_writeback(ntfs_volume *vol, s64 pos, s64 size, void *b, 
			    u8 *writeback)
{
	s64 written;
	
	ntfs_log_trace("Entering\n");
	
	if (!*writeback)
		return 0;
	
	*writeback = 0;
	
	written = ntfs_attr_pwrite(vol->lcnbmp_na, pos, size, b);
	if (written != size) {
		if (!written)
			errno = EIO;
		ntfs_log_perror("Bitmap write error (%lld, %lld)",
				(long long)pos, (long long)size);
		return -1;
	}

	return 0;
}

/**
 * ntfs_cluster_alloc - allocate clusters on an ntfs volume
 * @vol:	mounted ntfs volume on which to allocate the clusters
 * @start_vcn:	vcn to use for the first allocated cluster
 * @count:	number of clusters to allocate
 * @start_lcn:	starting lcn at which to allocate the clusters (or -1 if none)
 * @zone:	zone from which to allocate the clusters
 *
 * Allocate @count clusters preferably starting at cluster @start_lcn or at the
 * current allocator position if @start_lcn is -1, on the mounted ntfs volume
 * @vol. @zone is either DATA_ZONE for allocation of normal clusters and
 * MFT_ZONE for allocation of clusters for the master file table, i.e. the
 * $MFT/$DATA attribute.
 *
 * On success return a runlist describing the allocated cluster(s).
 *
 * On error return NULL with errno set to the error code.
 *
 * Notes on the allocation algorithm
 * =================================
 *
 * There are two data zones. First is the area between the end of the mft zone
 * and the end of the volume, and second is the area between the start of the
 * volume and the start of the mft zone. On unmodified/standard NTFS 1.x
 * volumes, the second data zone doesn't exist due to the mft zone being
 * expanded to cover the start of the volume in order to reserve space for the
 * mft bitmap attribute.
 *
 * The complexity stems from the need of implementing the mft vs data zoned 
 * approach and from the fact that we have access to the lcn bitmap via up to 
 * NTFS_LCNALLOC_BSIZE bytes at a time, so we need to cope with crossing over 
 * boundaries of two buffers. Further, the fact that the allocator allows for 
 * caller supplied hints as to the location of where allocation should begin 
 * and the fact that the allocator keeps track of where in the data zones the 
 * next natural allocation should occur, contribute to the complexity of the 
 * function. But it should all be worthwhile, because this allocator: 
 *   1) implements MFT zone reservation
 *   2) causes reduction in fragmentation. 
 * The code is not optimized for speed.
 */
runlist *ntfs_cluster_alloc(ntfs_volume *vol, VCN start_vcn, s64 count,
		LCN start_lcn, const NTFS_CLUSTER_ALLOCATION_ZONES zone)
{
	LCN zone_start, zone_end;  /* current search range */
	LCN last_read_pos, lcn;
	LCN bmp_pos;		/* current bit position inside the bitmap */
	LCN prev_lcn = 0, prev_run_len = 0;
	s64 clusters, br;
	runlist *rl = NULL, *trl;
	u8 *buf, *byte, bit, writeback;
	u8 pass = 1; 	/* 1: inside zone;  2: start of zone */
	u8 search_zone; /* 4: data2 (start) 1: mft (middle) 2: data1 (end) */
	u8 done_zones = 0;
	u8 has_guess, used_zone_pos;
	int err = 0, rlpos, rlsize, buf_size;

	ntfs_log_enter("Entering with count = 0x%llx, start_lcn = 0x%llx, "
		       "zone = %s_ZONE.\n", (long long)count, (long long)
		       start_lcn, zone == MFT_ZONE ? "MFT" : "DATA");
	
	if (!vol || count < 0 || start_lcn < -1 || !vol->lcnbmp_na ||
			(s8)zone < FIRST_ZONE || zone > LAST_ZONE) {
		errno = EINVAL;
		ntfs_log_perror("%s: vcn: %lld, count: %lld, lcn: %lld", 
				__FUNCTION__, (long long)start_vcn, 
				(long long)count, (long long)start_lcn);
		goto out;
	}

	/* Return empty runlist if @count == 0 */
	if (!count) {
		rl = ntfs_malloc(0x1000);
		if (rl) {
			rl[0].vcn = start_vcn;
			rl[0].lcn = LCN_RL_NOT_MAPPED;
			rl[0].length = 0;
		}
		goto out;
	}

	buf = ntfs_malloc(NTFS_LCNALLOC_BSIZE);
	if (!buf)
		goto out;
	/*
	 * If no @start_lcn was requested, use the current zone
	 * position otherwise use the requested @start_lcn.
	 */
	has_guess = 1;
	zone_start = start_lcn;
	
	if (zone_start < 0) {
		if (zone == DATA_ZONE)
			zone_start = vol->data1_zone_pos;
		else
			zone_start = vol->mft_zone_pos;
		has_guess = 0;
	}
	
	used_zone_pos = has_guess ? 0 : 1;
	
	if (!zone_start || zone_start == vol->mft_zone_start ||
			zone_start == vol->mft_zone_end)
		pass = 2;
		
	if (zone_start < vol->mft_zone_start) {
		zone_end = vol->mft_zone_start;
		search_zone = ZONE_DATA2;
	} else if (zone_start < vol->mft_zone_end) {
		zone_end = vol->mft_zone_end;
		search_zone = ZONE_MFT;
	} else {
		zone_end = vol->nr_clusters;
		search_zone = ZONE_DATA1;
	}
	
	bmp_pos = zone_start;

	/* Loop until all clusters are allocated. */
	clusters = count;
	rlpos = rlsize = 0;
	while (1) {
			/* check whether we have exhausted the current zone */
		if (search_zone & vol->full_zones)
			goto zone_pass_done;
		last_read_pos = bmp_pos >> 3;
		br = ntfs_attr_pread(vol->lcnbmp_na, last_read_pos, 
				     NTFS_LCNALLOC_BSIZE, buf);
		if (br <= 0) {
			if (!br)
				goto zone_pass_done;
			err = errno;
			ntfs_log_perror("Reading $BITMAP failed");
			goto err_ret;
		}
		/*
		 * We might have read less than NTFS_LCNALLOC_BSIZE bytes
		 * if we are close to the end of the attribute.
		 */
		buf_size = (int)br << 3;
		lcn = bmp_pos & 7;
		bmp_pos &= ~7;
		writeback = 0;
		
		while (lcn < buf_size) {
			byte = buf + (lcn >> 3);
			bit = 1 << (lcn & 7);
			if (has_guess) {
				if (*byte & bit) {
					has_guess = 0;
					break;
				}
			} else {
				lcn = max_empty_bit_range(buf, br);
				if (lcn < 0)
					break;
				has_guess = 1;
				continue;
			}

			/* First free bit is at lcn + bmp_pos. */
			 
			/* Reallocate memory if necessary. */
			if ((rlpos + 2) * (int)sizeof(runlist) >= rlsize) {
				rlsize += 4096;
				trl = realloc(rl, rlsize);
				if (!trl) {
					err = ENOMEM;
					ntfs_log_perror("realloc() failed");
					goto wb_err_ret;
				}
				rl = trl;
			}
			
			/* Allocate the bitmap bit. */
			*byte |= bit;
			writeback = 1;
			if (vol->free_clusters <= 0) 
				ntfs_log_error("Non-positive free clusters "
					       "(%lld)!\n",
						(long long)vol->free_clusters);
			else	
				vol->free_clusters--; 
			
			/*
			 * Coalesce with previous run if adjacent LCNs.
			 * Otherwise, append a new run.
			 */
			if (prev_lcn == lcn + bmp_pos - prev_run_len && rlpos) {
				ntfs_log_debug("Cluster coalesce: prev_lcn: "
					       "%lld  lcn: %lld  bmp_pos: %lld  "
					       "prev_run_len: %lld\n", 
					       (long long)prev_lcn, 
					       (long long)lcn, (long long)bmp_pos, 
					       (long long)prev_run_len);
				rl[rlpos - 1].length = ++prev_run_len;
			} else {
				if (rlpos)
					rl[rlpos].vcn = rl[rlpos - 1].vcn +
							prev_run_len;
				else {
					rl[rlpos].vcn = start_vcn;
					ntfs_log_debug("Start_vcn: %lld\n", 
						       (long long)start_vcn);
				}
				
				rl[rlpos].lcn = prev_lcn = lcn + bmp_pos;
				rl[rlpos].length = prev_run_len = 1;
				rlpos++;
			}
			
			ntfs_log_debug("RUN:   %-16lld %-16lld %-16lld\n", 
				       (long long)rl[rlpos - 1].vcn, 
				       (long long)rl[rlpos - 1].lcn, 
				       (long long)rl[rlpos - 1].length);
			/* Done? */
			if (!--clusters) {
				if (used_zone_pos)
					ntfs_cluster_update_zone_pos(vol, 
						search_zone, lcn + bmp_pos + 1 +
							NTFS_LCNALLOC_SKIP);
				goto done_ret;
			}
			
			lcn++;
		}
		
		if (bitmap_writeback(vol, last_read_pos, br, buf, &writeback)) {
			err = errno;
			goto err_ret;
		}
		
		if (!used_zone_pos) {
			
			used_zone_pos = 1;
			
			if (search_zone == ZONE_MFT)
				zone_start = vol->mft_zone_pos;
			else if (search_zone == ZONE_DATA1)
				zone_start = vol->data1_zone_pos;
			else
				zone_start = vol->data2_zone_pos;
			
			if (!zone_start || zone_start == vol->mft_zone_start ||
					zone_start == vol->mft_zone_end)
				pass = 2;
			bmp_pos = zone_start;
		} else
			bmp_pos += buf_size;
		
		if (bmp_pos < zone_end)
			continue;

zone_pass_done:
		ntfs_log_trace("Finished current zone pass(%i).\n", pass);
		if (pass == 1) {
			pass = 2;
			zone_end = zone_start;
			
			if (search_zone == ZONE_MFT)
				zone_start = vol->mft_zone_start;
			else if (search_zone == ZONE_DATA1)
				zone_start = vol->mft_zone_end;
			else
				zone_start = 0;
			
			/* Sanity check. */
			if (zone_end < zone_start)
				zone_end = zone_start;
			
			bmp_pos = zone_start;
			
			continue;
		} 
		/* pass == 2 */
done_zones_check:
		done_zones |= search_zone;
		vol->full_zones |= search_zone;
		if (done_zones < (ZONE_MFT + ZONE_DATA1 + ZONE_DATA2)) {
			ntfs_log_trace("Switching zone.\n");
			pass = 1;
			if (rlpos) {
				LCN tc = rl[rlpos - 1].lcn + 
				      rl[rlpos - 1].length + NTFS_LCNALLOC_SKIP;
				
				if (used_zone_pos)
					ntfs_cluster_update_zone_pos(vol, 
						search_zone, tc);
			}
			
			switch (search_zone) {
			case ZONE_MFT:
				ntfs_log_trace("Zone switch: mft -> data1\n");
switch_to_data1_zone:		search_zone = ZONE_DATA1;
				zone_start = vol->data1_zone_pos;
				zone_end = vol->nr_clusters;
				if (zone_start == vol->mft_zone_end)
					pass = 2;
				break;
			case ZONE_DATA1:
				ntfs_log_trace("Zone switch: data1 -> data2\n");
				search_zone = ZONE_DATA2;
				zone_start = vol->data2_zone_pos;
				zone_end = vol->mft_zone_start;
				if (!zone_start)
					pass = 2;
				break;
			case ZONE_DATA2:
				if (!(done_zones & ZONE_DATA1)) {
					ntfs_log_trace("data2 -> data1\n");
					goto switch_to_data1_zone;
				}
				ntfs_log_trace("Zone switch: data2 -> mft\n");
				search_zone = ZONE_MFT;
				zone_start = vol->mft_zone_pos;
				zone_end = vol->mft_zone_end;
				if (zone_start == vol->mft_zone_start)
					pass = 2;
				break;
			}
			
			bmp_pos = zone_start;
			
			if (zone_start == zone_end) {
				ntfs_log_trace("Empty zone, skipped.\n");
				goto done_zones_check;
			}
			
			continue;
		}
		
		ntfs_log_trace("All zones are finished, no space on device.\n");
		err = ENOSPC;
		goto err_ret;
	}
done_ret:
	ntfs_log_debug("At done_ret.\n");
	/* Add runlist terminator element. */
	rl[rlpos].vcn = rl[rlpos - 1].vcn + rl[rlpos - 1].length;
	rl[rlpos].lcn = LCN_RL_NOT_MAPPED;
	rl[rlpos].length = 0;
	if (bitmap_writeback(vol, last_read_pos, br, buf, &writeback)) {
		err = errno;
		goto err_ret;
	}
done_err_ret:
	free(buf);
	if (err) {
		errno = err;
		ntfs_log_perror("Failed to allocate clusters");
		rl = NULL;
	}
out:	
	ntfs_log_leave("\n");
	return rl;

wb_err_ret:
	ntfs_log_trace("At wb_err_ret.\n");
	if (bitmap_writeback(vol, last_read_pos, br, buf, &writeback))
		err = errno;
err_ret:
	ntfs_log_trace("At err_ret.\n");
	if (rl) {
		/* Add runlist terminator element. */
		rl[rlpos].vcn = rl[rlpos - 1].vcn + rl[rlpos - 1].length;
		rl[rlpos].lcn = LCN_RL_NOT_MAPPED;
		rl[rlpos].length = 0;
		ntfs_debug_runlist_dump(rl);
		ntfs_cluster_free_from_rl(vol, rl);
		free(rl);
		rl = NULL;
	}
	goto done_err_ret;
}

/**
 * ntfs_cluster_free_from_rl - free clusters from runlist
 * @vol:	mounted ntfs volume on which to free the clusters
 * @rl:		runlist from which deallocate clusters
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 */
int ntfs_cluster_free_from_rl(ntfs_volume *vol, runlist *rl)
{
	s64 nr_freed = 0;
	int ret = -1;

	ntfs_log_trace("Entering.\n");

	for (; rl->length; rl++) {

		ntfs_log_trace("Dealloc lcn 0x%llx, len 0x%llx.\n",
			       (long long)rl->lcn, (long long)rl->length);

		if (rl->lcn >= 0) { 
			update_full_status(vol,rl->lcn);
			if (ntfs_bitmap_clear_run(vol->lcnbmp_na, rl->lcn, 
						  rl->length)) {
				ntfs_log_perror("Cluster deallocation failed "
					       "(%lld, %lld)",
						(long long)rl->lcn, 
						(long long)rl->length);
				goto out;
			}
			nr_freed += rl->length ; 
		}
	}

	ret = 0;
out:
	vol->free_clusters += nr_freed; 
	if (vol->free_clusters > vol->nr_clusters)
		ntfs_log_error("Too many free clusters (%lld > %lld)!",
			       (long long)vol->free_clusters, 
			       (long long)vol->nr_clusters);
	return ret;
}

/*
 *		Basic cluster run free
 *	Returns 0 if successful
 */

int ntfs_cluster_free_basic(ntfs_volume *vol, s64 lcn, s64 count)
{
	s64 nr_freed = 0;
	int ret = -1;

	ntfs_log_trace("Entering.\n");
	ntfs_log_trace("Dealloc lcn 0x%llx, len 0x%llx.\n",
			       (long long)lcn, (long long)count);

	if (lcn >= 0) { 
		update_full_status(vol,lcn);
		if (ntfs_bitmap_clear_run(vol->lcnbmp_na, lcn, 
						  count)) {
			ntfs_log_perror("Cluster deallocation failed "
				       "(%lld, %lld)",
					(long long)lcn, 
					(long long)count);
				goto out;
		}
		nr_freed += count; 
	}
	ret = 0;
out:
	vol->free_clusters += nr_freed;
	if (vol->free_clusters > vol->nr_clusters)
		ntfs_log_error("Too many free clusters (%lld > %lld)!",
			       (long long)vol->free_clusters, 
			       (long long)vol->nr_clusters);
	return ret;
}

/**
 * ntfs_cluster_free - free clusters on an ntfs volume
 * @vol:	mounted ntfs volume on which to free the clusters
 * @na:		attribute whose runlist describes the clusters to free
 * @start_vcn:	vcn in @rl at which to start freeing clusters
 * @count:	number of clusters to free or -1 for all clusters
 *
 * Free @count clusters starting at the cluster @start_vcn in the runlist
 * described by the attribute @na from the mounted ntfs volume @vol.
 *
 * If @count is -1, all clusters from @start_vcn to the end of the runlist
 * are deallocated.
 *
 * On success return the number of deallocated clusters (not counting sparse
 * clusters) and on error return -1 with errno set to the error code.
 */
int ntfs_cluster_free(ntfs_volume *vol, ntfs_attr *na, VCN start_vcn, s64 count)
{
	runlist *rl;
	s64 delta, to_free, nr_freed = 0;
	int ret = -1;

	if (!vol || !vol->lcnbmp_na || !na || start_vcn < 0 ||
			(count < 0 && count != -1)) {
		ntfs_log_trace("Invalid arguments!\n");
		errno = EINVAL;
		return -1;
	}
	
	ntfs_log_enter("Entering for inode 0x%llx, attr 0x%x, count 0x%llx, "
		       "vcn 0x%llx.\n", (unsigned long long)na->ni->mft_no,
		       na->type, (long long)count, (long long)start_vcn);

	rl = ntfs_attr_find_vcn(na, start_vcn);
	if (!rl) {
		if (errno == ENOENT)
			ret = 0;
		goto leave;
	}

	if (rl->lcn < 0 && rl->lcn != LCN_HOLE) {
		errno = EIO;
		ntfs_log_perror("%s: Unexpected lcn (%lld)", __FUNCTION__, 
				(long long)rl->lcn);
		goto leave;
	}

	/* Find the starting cluster inside the run that needs freeing. */
	delta = start_vcn - rl->vcn;

	/* The number of clusters in this run that need freeing. */
	to_free = rl->length - delta;
	if (count >= 0 && to_free > count)
		to_free = count;

	if (rl->lcn != LCN_HOLE) {
		/* Do the actual freeing of the clusters in this run. */
		update_full_status(vol,rl->lcn + delta);
		if (ntfs_bitmap_clear_run(vol->lcnbmp_na, rl->lcn + delta,
					  to_free))
			goto leave;
		nr_freed = to_free;
	} 

	/* Go to the next run and adjust the number of clusters left to free. */
	++rl;
	if (count >= 0)
		count -= to_free;

	/*
	 * Loop over the remaining runs, using @count as a capping value, and
	 * free them.
	 */
	for (; rl->length && count != 0; ++rl) {
		// FIXME: Need to try ntfs_attr_map_runlist() for attribute
		//	  list support! (AIA)
		if (rl->lcn < 0 && rl->lcn != LCN_HOLE) {
			// FIXME: Eeek! We need rollback! (AIA)
			errno = EIO;
			ntfs_log_perror("%s: Invalid lcn (%lli)", 
					__FUNCTION__, (long long)rl->lcn);
			goto out;
		}

		/* The number of clusters in this run that need freeing. */
		to_free = rl->length;
		if (count >= 0 && to_free > count)
			to_free = count;

		if (rl->lcn != LCN_HOLE) {
			update_full_status(vol,rl->lcn);
			if (ntfs_bitmap_clear_run(vol->lcnbmp_na, rl->lcn,
					to_free)) {
				// FIXME: Eeek! We need rollback! (AIA)
				ntfs_log_perror("%s: Clearing bitmap run failed",
						__FUNCTION__);
				goto out;
			}
			nr_freed += to_free;
		}

		if (count >= 0)
			count -= to_free;
	}

	if (count != -1 && count != 0) {
		// FIXME: Eeek! BUG()
		errno = EIO;
		ntfs_log_perror("%s: count still not zero (%lld)", __FUNCTION__,
			       (long long)count);
		goto out;
	}

	ret = nr_freed;
out:
	vol->free_clusters += nr_freed ; 
	if (vol->free_clusters > vol->nr_clusters)
		ntfs_log_error("Too many free clusters (%lld > %lld)!",
			       (long long)vol->free_clusters, 
			       (long long)vol->nr_clusters);
leave:	
	ntfs_log_leave("\n");
	return ret;
}
