/**
 * lcnalloc.c - Cluster (de)allocation code. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2004 Anton Altaparmakov
 * Copyright (c) 2004 Yura Pakhuchiy
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
 * along with this program (in the main directory of the Linux-NTFS
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
 * This is not the prettiest function but the complexity stems from the need of
 * implementing the mft vs data zoned approach and from the fact that we have
 * access to the lcn bitmap in portions of up to 8192 bytes at a time, so we
 * need to cope with crossing over boundaries of two buffers. Further, the fact
 * that the allocator allows for caller supplied hints as to the location of
 * where allocation should begin and the fact that the allocator keeps track of
 * where in the data zones the next natural allocation should occur, contribute
 * to the complexity of the function. But it should all be worthwhile, because
 * this allocator should: 1) be a full implementation of the MFT zone approach
 * used by Windows, 2) cause reduction in fragmentation as much as possible,
 * and 3) be speedy in allocations (the code is not optimized for speed, but
 * the algorithm is, so further speed improvements are probably possible).
 *
 * FIXME: We should be monitoring cluster allocation and increment the MFT zone
 * size dynamically but this is something for the future. We will just cause
 * heavier fragmentation by not doing it and I am not even sure Windows would
 * grow the MFT zone dynamically, so it might even be correct not to do this.
 * The overhead in doing dynamic MFT zone expansion would be very large and
 * unlikely worth the effort. (AIA)
 *
 * TODO: I have added in double the required zone position pointer wrap around
 * logic which can be optimized to having only one of the two logic sets.
 * However, having the double logic will work fine, but if we have only one of
 * the sets and we get it wrong somewhere, then we get into trouble, so
 * removing the duplicate logic requires _very_ careful consideration of _all_
 * possible code paths. So at least for now, I am leaving the double logic -
 * better safe than sorry... (AIA)
 */
runlist *ntfs_cluster_alloc(ntfs_volume *vol, VCN start_vcn, s64 count,
		LCN start_lcn, const NTFS_CLUSTER_ALLOCATION_ZONES zone)
{
	LCN zone_start, zone_end, bmp_pos, bmp_initial_pos, last_read_pos, lcn;
	LCN prev_lcn = 0, prev_run_len = 0, mft_zone_size;
	s64 clusters, br;
	runlist *rl = NULL, *trl;
	u8 *buf, *byte;
	int err = 0, rlpos, rlsize, buf_size;
	u8 pass, done_zones, search_zone, need_writeback, bit;

	ntfs_log_trace("Entering with count = 0x%llx, start_lcn = 0x%llx, zone = "
			"%s_ZONE.\n", (long long)count, (long long)start_lcn,
			zone == MFT_ZONE ? "MFT" : "DATA");
	if (!vol || count < 0 || start_lcn < -1 || !vol->lcnbmp_na ||
			(s8)zone < FIRST_ZONE || zone > LAST_ZONE) {
		ntfs_log_trace("Invalid arguments!\n");
		errno = EINVAL;
		return NULL;
	}

	/* Return empty runlist if @count == 0 */
	if (!count) {
		rl = ntfs_malloc(0x1000);
		if (!rl)
			return NULL;
		rl[0].vcn = start_vcn;
		rl[0].lcn = LCN_RL_NOT_MAPPED;
		rl[0].length = 0;
		return rl;
	}

	/* Allocate memory. */
	buf = (u8*)ntfs_malloc(8192);
	if (!buf)
		return NULL;
	/*
	 * If no specific @start_lcn was requested, use the current data zone
	 * position, otherwise use the requested @start_lcn but make sure it
	 * lies outside the mft zone. Also set done_zones to 0 (no zones done)
	 * and pass depending on whether we are starting inside a zone (1) or
	 * at the beginning of a zone (2). If requesting from the MFT_ZONE,
	 * we either start at the current position within the mft zone or at
	 * the specified position. If the latter is out of bounds then we start
	 * at the beginning of the MFT_ZONE.
	 */
	done_zones = 0;
	pass = 1;
	/*
	 * zone_start and zone_end are the current search range. search_zone
	 * is 1 for mft zone, 2 for data zone 1 (end of mft zone till end of
	 * volume) and 4 for data zone 2 (start of volume till start of mft
	 * zone).
	 */
	zone_start = start_lcn;
	if (zone_start < 0) {
		if (zone == DATA_ZONE)
			zone_start = vol->data1_zone_pos;
		else
			zone_start = vol->mft_zone_pos;
		if (!zone_start) {
			/*
			 * Zone starts at beginning of volume which means a
			 * single pass is sufficient.
			 */
			pass = 2;
		}
	} else if (zone == DATA_ZONE && zone_start >= vol->mft_zone_start &&
			zone_start < vol->mft_zone_end) {
		zone_start = vol->mft_zone_end;
		/*
		 * Starting at beginning of data1_zone which means a single
		 * pass in this zone is sufficient.
		 */
		pass = 2;
	} else if (zone == MFT_ZONE && (zone_start < vol->mft_zone_start ||
			zone_start >= vol->mft_zone_end)) {
		zone_start = vol->mft_lcn;
		if (!vol->mft_zone_end)
			zone_start = 0;
		/*
		 * Starting at beginning of volume which means a single pass
		 * is sufficient.
		 */
		pass = 2;
	}
	if (zone == MFT_ZONE) {
		zone_end = vol->mft_zone_end;
		search_zone = 1;
	} else /* if (zone == DATA_ZONE) */ {
		/* Skip searching the mft zone. */
		done_zones |= 1;
		if (zone_start >= vol->mft_zone_end) {
			zone_end = vol->nr_clusters;
			search_zone = 2;
		} else {
			zone_end = vol->mft_zone_start;
			search_zone = 4;
		}
	}
	/*
	 * bmp_pos is the current bit position inside the bitmap. We use
	 * bmp_initial_pos to determine whether or not to do a zone switch.
	 */
	bmp_pos = bmp_initial_pos = zone_start;

	/* Loop until all clusters are allocated, i.e. clusters == 0. */
	clusters = count;
	rlpos = rlsize = 0;
	while (1) {
		ntfs_log_trace("Start of outer while loop: done_zones = 0x%x, "
				"search_zone = %i, pass = %i, zone_start = "
				"0x%llx, zone_end = 0x%llx, bmp_initial_pos = "
				"0x%llx, bmp_pos = 0x%llx, rlpos = %i, rlsize = "
				"%i.\n", done_zones, search_zone, pass,
				(long long)zone_start, (long long)zone_end,
				(long long)bmp_initial_pos, (long long)bmp_pos,
				rlpos, rlsize);
		/* Loop until we run out of free clusters. */
		last_read_pos = bmp_pos >> 3;
		ntfs_log_trace("last_read_pos = 0x%llx.\n", (long long)last_read_pos);
		br = ntfs_attr_pread(vol->lcnbmp_na, last_read_pos, 8192, buf);
		if (br <= 0) {
			if (!br) {
				/* Reached end of attribute. */
				ntfs_log_trace("End of attribute reached. Skipping "
						"to zone_pass_done.\n");
				goto zone_pass_done;
			}
			err = errno;
			ntfs_log_trace("ntfs_attr_pread() failed. Aborting.\n");
			goto err_ret;
		}
		/*
		 * We might have read less than 8192 bytes if we are close to
		 * the end of the attribute.
		 */
		buf_size = (int)br << 3;
		lcn = bmp_pos & 7;
		bmp_pos &= ~7;
		need_writeback = 0;
		ntfs_log_trace("Before inner while loop: buf_size = %i, lcn = "
				"0x%llx, bmp_pos = 0x%llx, need_writeback = %i.\n",
				buf_size, (long long)lcn, (long long)bmp_pos,
				need_writeback);
		while (lcn < buf_size && lcn + bmp_pos < zone_end) {
			byte = buf + (lcn >> 3);
			ntfs_log_trace("In inner while loop: buf_size = %i, lcn = "
					"0x%llx, bmp_pos = 0x%llx, "
					"need_writeback = %i, byte ofs = 0x%x, "
					"*byte = 0x%x.\n", buf_size,
					(long long)lcn, (long long)bmp_pos,
					need_writeback, (unsigned int)(lcn >> 3),
					(unsigned int)*byte);
			/* Skip full bytes. */
			if (*byte == 0xff) {
				lcn = (lcn + 8) & ~7;
				ntfs_log_trace("continuing while loop 1.\n");
				continue;
			}
			bit = 1 << (lcn & 7);
			ntfs_log_trace("bit = %i.\n", bit);
			/* If the bit is already set, go onto the next one. */
			if (*byte & bit) {
				lcn++;
				ntfs_log_trace("continuing while loop 2.\n");
				continue;
			}
			/* Reallocate memory if necessary. */
			if ((rlpos + 2) * (int)sizeof(runlist) >= rlsize) {
				ntfs_log_trace("Reallocating space.\n");
				if (!rl)
					ntfs_log_trace("First free bit is at LCN = "
						"0x%llx.\n", (long long)(lcn + bmp_pos));
				rlsize += 4096;
				trl = (runlist*)realloc(rl, rlsize);
				if (!trl) {
					err = ENOMEM;
					ntfs_log_trace("Failed to allocate memory, "
							"going to wb_err_ret.\n");
					goto wb_err_ret;
				}
				rl = trl;
				ntfs_log_trace("Reallocated memory, rlsize = "
						"0x%x.\n", rlsize);
			}
			/* Allocate the bitmap bit. */
			*byte |= bit;
			vol->nr_free_clusters--;
			/* We need to write this bitmap buffer back to disk! */
			need_writeback = 1;
			ntfs_log_trace("*byte = 0x%x, need_writeback is set.\n",
					(unsigned int)*byte);
			/*
			 * Coalesce with previous run if adjacent LCNs.
			 * Otherwise, append a new run.
			 */
			ntfs_log_trace("Adding run (lcn 0x%llx, len 0x%llx), "
					"prev_lcn = 0x%llx, lcn = 0x%llx, "
					"bmp_pos = 0x%llx, prev_run_len = "
					"0x%llx, rlpos = %i.\n",
					(long long)(lcn + bmp_pos), 1LL,
					(long long)prev_lcn, (long long)lcn,
					(long long)bmp_pos,
					(long long)prev_run_len, rlpos);
			if (prev_lcn == lcn + bmp_pos - prev_run_len && rlpos) {
				ntfs_log_trace("Coalescing to run (lcn 0x%llx, len "
						"0x%llx).\n",
						(long long)rl[rlpos - 1].lcn,
						(long long) rl[rlpos - 1].length);
				rl[rlpos - 1].length = ++prev_run_len;
				ntfs_log_trace("Run now (lcn 0x%llx, len 0x%llx), "
						"prev_run_len = 0x%llx.\n",
						(long long)rl[rlpos - 1].lcn,
						(long long)rl[rlpos - 1].length,
						(long long)prev_run_len);
			} else {
				if (rlpos) {
					ntfs_log_trace("Adding new run, (previous "
						"run lcn 0x%llx, len 0x%llx).\n",
						(long long) rl[rlpos - 1].lcn,
						(long long) rl[rlpos - 1].length);
					rl[rlpos].vcn = rl[rlpos - 1].vcn +
							prev_run_len;
				} else {
					ntfs_log_trace("Adding new run, is first run.\n");
					rl[rlpos].vcn = start_vcn;
				}
				rl[rlpos].lcn = prev_lcn = lcn + bmp_pos;
				rl[rlpos].length = prev_run_len = 1;
				rlpos++;
			}
			/* Done? */
			if (!--clusters) {
				LCN tc;
				/*
				 * Update the current zone position. Positions
				 * of already scanned zones have been updated
				 * during the respective zone switches.
				 */
				tc = lcn + bmp_pos + 1;
				ntfs_log_trace("Done. Updating current zone "
					"position, tc = 0x%llx, search_zone = %i.\n",
					(long long)tc, search_zone);
				switch (search_zone) {
				case 1:
					ntfs_log_trace("Before checks, vol->mft_zone_pos = 0x%llx.\n",
							(long long) vol->mft_zone_pos);
					if (tc >= vol->mft_zone_end) {
						vol->mft_zone_pos =
								vol->mft_lcn;
						if (!vol->mft_zone_end)
							vol->mft_zone_pos = 0;
					} else if ((bmp_initial_pos >=
							vol->mft_zone_pos ||
							tc > vol->mft_zone_pos)
							&& tc >= vol->mft_lcn)
						vol->mft_zone_pos = tc;
					ntfs_log_trace("After checks, vol->mft_zone_pos = 0x%llx.\n",
							(long long) vol->mft_zone_pos);
					break;
				case 2:
					ntfs_log_trace("Before checks, vol->data1_zone_pos = 0x%llx.\n",
							(long long) vol->data1_zone_pos);
					if (tc >= vol->nr_clusters)
						vol->data1_zone_pos =
							     vol->mft_zone_end;
					else if ((bmp_initial_pos >=
						    vol->data1_zone_pos ||
						    tc > vol->data1_zone_pos)
						    && tc >= vol->mft_zone_end)
						vol->data1_zone_pos = tc;
					ntfs_log_trace("After checks, vol->data1_zone_pos = 0x%llx.\n",
							(long long) vol->data1_zone_pos);
					break;
				case 4:
					ntfs_log_trace("Before checks, vol->data2_zone_pos = 0x%llx.\n",
							(long long) vol->data2_zone_pos);
					if (tc >= vol->mft_zone_start)
						vol->data2_zone_pos = 0;
					else if (bmp_initial_pos >=
						      vol->data2_zone_pos ||
						      tc > vol->data2_zone_pos)
						vol->data2_zone_pos = tc;
					ntfs_log_trace("After checks, vol->data2_zone_pos = 0x%llx.\n",
							(long long) vol->data2_zone_pos);
					break;
				default:
					free(rl);
					free(buf);
					NTFS_BUG("switch (search_zone) 1");
					return NULL;
				}
				ntfs_log_trace("Going to done_ret.\n");
				goto done_ret;
			}
			lcn++;
		}
		bmp_pos += buf_size;
		ntfs_log_trace("After inner while loop: buf_size = 0x%x, lcn = "
				"0x%llx, bmp_pos = 0x%llx, need_writeback = %i.\n",
				buf_size, (long long)lcn,
				(long long)bmp_pos, need_writeback);
		if (need_writeback) {
			s64 bw;
			ntfs_log_trace("Writing back.\n");
			need_writeback = 0;
			bw = ntfs_attr_pwrite(vol->lcnbmp_na, last_read_pos,
					br, buf);
			if (bw != br) {
				if (bw == -1)
					err = errno;
				else
					err = EIO;
				ntfs_log_trace("Bitmap writeback failed in read next "
					"buffer code path with error code %i.\n", err);
				goto err_ret;
			}
		}
		if (bmp_pos < zone_end) {
			ntfs_log_trace("Continuing outer while loop, bmp_pos = "
					"0x%llx, zone_end = 0x%llx.\n",
					(long long)bmp_pos,
					(long long)zone_end);
			continue;
		}
zone_pass_done:	/* Finished with the current zone pass. */
		ntfs_log_trace("At zone_pass_done, pass = %i.\n", pass);
		if (pass == 1) {
			/*
			 * Now do pass 2, scanning the first part of the zone
			 * we omitted in pass 1.
			 */
			pass = 2;
			zone_end = zone_start;
			switch (search_zone) {
			case 1: /* mft_zone */
				zone_start = vol->mft_zone_start;
				break;
			case 2: /* data1_zone */
				zone_start = vol->mft_zone_end;
				break;
			case 4: /* data2_zone */
				zone_start = 0;
				break;
			default:
				NTFS_BUG("switch (search_zone) 2");
			}
			/* Sanity check. */
			if (zone_end < zone_start)
				zone_end = zone_start;
			bmp_pos = zone_start;
			ntfs_log_trace("Continuing outer while loop, pass = 2, "
					"zone_start = 0x%llx, zone_end = "
					"0x%llx, bmp_pos = 0x%llx.\n",
					zone_start, zone_end, bmp_pos);
			continue;
		} /* pass == 2 */
done_zones_check:
		ntfs_log_trace("At done_zones_check, search_zone = %i, done_zones "
				"before = 0x%x, done_zones after = 0x%x.\n",
				search_zone, done_zones, done_zones | search_zone);
		done_zones |= search_zone;
		if (done_zones < 7) {
			ntfs_log_trace("Switching zone.\n");
			/* Now switch to the next zone we haven't done yet. */
			pass = 1;
			switch (search_zone) {
			case 1:
				ntfs_log_trace("Switching from mft zone to data1 "
						"zone.\n");
				/* Update mft zone position. */
				if (rlpos) {
					LCN tc;
					ntfs_log_trace("Before checks, vol->mft_zone_pos = 0x%llx.\n",
							(long long) vol->mft_zone_pos);
					tc = rl[rlpos - 1].lcn +
							rl[rlpos - 1].length;
					if (tc >= vol->mft_zone_end) {
						vol->mft_zone_pos =
								vol->mft_lcn;
						if (!vol->mft_zone_end)
							vol->mft_zone_pos = 0;
					} else if ((bmp_initial_pos >=
							vol->mft_zone_pos ||
							tc > vol->mft_zone_pos)
							&& tc >= vol->mft_lcn)
						vol->mft_zone_pos = tc;
					ntfs_log_trace("After checks, vol->mft_zone_pos = 0x%llx.\n",
							(long long) vol->mft_zone_pos);
				}
				/* Switch from mft zone to data1 zone. */
switch_to_data1_zone:		search_zone = 2;
				zone_start = bmp_initial_pos =
						vol->data1_zone_pos;
				zone_end = vol->nr_clusters;
				if (zone_start == vol->mft_zone_end)
					pass = 2;
				if (zone_start >= zone_end) {
					vol->data1_zone_pos = zone_start =
							vol->mft_zone_end;
					pass = 2;
				}
				break;
			case 2:
				ntfs_log_trace("Switching from data1 zone to data2 "
						"zone.\n");
				/* Update data1 zone position. */
				if (rlpos) {
					LCN tc;
					ntfs_log_trace("Before checks, vol->data1_zone_pos = 0x%llx.\n",
							(long long) vol->data1_zone_pos);
					tc = rl[rlpos - 1].lcn +
							rl[rlpos - 1].length;
					if (tc >= vol->nr_clusters)
						vol->data1_zone_pos =
							     vol->mft_zone_end;
					else if ((bmp_initial_pos >=
						    vol->data1_zone_pos ||
						    tc > vol->data1_zone_pos)
						    && tc >= vol->mft_zone_end)
						vol->data1_zone_pos = tc;
					ntfs_log_trace("After checks, vol->data1_zone_pos = 0x%llx.\n",
							(long long) vol->data1_zone_pos);
				}
				/* Switch from data1 zone to data2 zone. */
				search_zone = 4;
				zone_start = bmp_initial_pos =
						vol->data2_zone_pos;
				zone_end = vol->mft_zone_start;
				if (!zone_start)
					pass = 2;
				if (zone_start >= zone_end) {
					vol->data2_zone_pos = zone_start =
							bmp_initial_pos = 0;
					pass = 2;
				}
				break;
			case 4:
				ntfs_log_debug("Switching from data2 zone to data1 "
						"zone.\n");
				/* Update data2 zone position. */
				if (rlpos) {
					LCN tc;
					ntfs_log_trace("Before checks, vol->data2_zone_pos = 0x%llx.\n",
							(long long) vol->data2_zone_pos);
					tc = rl[rlpos - 1].lcn +
							rl[rlpos - 1].length;
					if (tc >= vol->mft_zone_start)
						vol->data2_zone_pos = 0;
					else if (bmp_initial_pos >=
						      vol->data2_zone_pos ||
						      tc > vol->data2_zone_pos)
						vol->data2_zone_pos = tc;
					ntfs_log_trace("After checks, vol->data2_zone_pos = 0x%llx.\n",
							(long long) vol->data2_zone_pos);
				}
				/* Switch from data2 zone to data1 zone. */
				goto switch_to_data1_zone; /* See above. */
			default:
				NTFS_BUG("switch (search_zone) 3");
			}
			ntfs_log_trace("After zone switch, search_zone = %i, pass = "
					"%i, bmp_initial_pos = 0x%llx, "
					"zone_start = 0x%llx, zone_end = "
					"0x%llx.\n", search_zone, pass,
					(long long)bmp_initial_pos,
					(long long)zone_start,
					(long long)zone_end);
			bmp_pos = zone_start;
			if (zone_start == zone_end) {
				ntfs_log_trace("Empty zone, going to "
						"done_zones_check.\n");
				/* Empty zone. Don't bother searching it. */
				goto done_zones_check;
			}
			ntfs_log_trace("Continuing outer while loop.\n");
			continue;
		} /* done_zones == 7 */
		ntfs_log_trace("All zones are finished.\n");
		/*
		 * All zones are finished! If DATA_ZONE, shrink mft zone. If
		 * MFT_ZONE, we have really run out of space.
		 */
		mft_zone_size = vol->mft_zone_end - vol->mft_zone_start;
		ntfs_log_trace("vol->mft_zone_start = 0x%llx, vol->mft_zone_end = "
				"0x%llx, mft_zone_size = 0x%llx.\n",
				(long long)vol->mft_zone_start,
				(long long)vol->mft_zone_end,
				(long long)mft_zone_size);
		if (zone == MFT_ZONE || mft_zone_size <= 0) {
			ntfs_log_trace("No free clusters left, going to err_ret.\n");
			/* Really no more space left on device. */
			err = ENOSPC;
			goto err_ret;
		} /* zone == DATA_ZONE && mft_zone_size > 0 */
		ntfs_log_trace("Shrinking mft zone.\n");
		zone_end = vol->mft_zone_end;
		mft_zone_size >>= 1;
		if (mft_zone_size > 0)
			vol->mft_zone_end = vol->mft_zone_start + mft_zone_size;
		else /* mft zone and data2 zone no longer exist. */
			vol->data2_zone_pos = vol->mft_zone_start =
					vol->mft_zone_end = 0;
		if (vol->mft_zone_pos >= vol->mft_zone_end) {
			vol->mft_zone_pos = vol->mft_lcn;
			if (!vol->mft_zone_end)
				vol->mft_zone_pos = 0;
		}
		bmp_pos = zone_start = bmp_initial_pos =
				vol->data1_zone_pos = vol->mft_zone_end;
		search_zone = 2;
		pass = 2;
		done_zones &= ~2;
		ntfs_log_trace("After shrinking mft zone, mft_zone_size = 0x%llx, "
				"vol->mft_zone_start = 0x%llx, "
				"vol->mft_zone_end = 0x%llx, vol->mft_zone_pos "
				"= 0x%llx, search_zone = 2, pass = 2, "
				"dones_zones = 0x%x, zone_start = 0x%llx, "
				"zone_end = 0x%llx, vol->data1_zone_pos = "
				"0x%llx, continuing outer while loop.\n",
				(long long)mft_zone_size,
				(long long)vol->mft_zone_start,
				(long long)vol->mft_zone_end,
				(long long)vol->mft_zone_pos,
				done_zones,
				(long long)zone_start,
				(long long)zone_end,
				(long long)vol->data1_zone_pos);
	}
	ntfs_log_debug("After outer while loop.\n");
done_ret:
	ntfs_log_debug("At done_ret.\n");
	/* Add runlist terminator element. */
	rl[rlpos].vcn = rl[rlpos - 1].vcn + rl[rlpos - 1].length;
	rl[rlpos].lcn = LCN_RL_NOT_MAPPED;
	rl[rlpos].length = 0;
	if (need_writeback) {
		s64 bw;
		ntfs_log_trace("Writing back.\n");
		need_writeback = 0;
		bw = ntfs_attr_pwrite(vol->lcnbmp_na, last_read_pos, br, buf);
		if (bw != br) {
			if (bw < 0)
				err = errno;
			else
				err = EIO;
			ntfs_log_trace("Bitmap writeback failed in done code path "
					"with error code %i.\n", err);
			goto err_ret;
		}
	}
done_err_ret:
	ntfs_log_debug("At done_err_ret (follows done_ret).\n");
	free(buf);
	/* Done! */
	if (!err)
		return rl;
	ntfs_log_trace("Failed to allocate clusters. Returning with error code "
			"%i.\n", err);
	errno = err;
	return NULL;
wb_err_ret:
	ntfs_log_trace("At wb_err_ret.\n");
	if (need_writeback) {
		s64 bw;
		ntfs_log_trace("Writing back.\n");
		need_writeback = 0;
		bw = ntfs_attr_pwrite(vol->lcnbmp_na, last_read_pos, br, buf);
		if (bw != br) {
			if (bw < 0)
				err = errno;
			else
				err = EIO;
			ntfs_log_trace("Bitmap writeback failed in error code path "
					"with error code %i.\n", err);
		}
	}
err_ret:
	ntfs_log_trace("At err_ret.\n");
	if (rl) {
		if (err == ENOSPC) {
			ntfs_log_trace("err = ENOSPC, first free lcn = 0x%llx, could "
					"allocate up to = 0x%llx clusters.\n",
					(long long)rl[0].lcn,
					(long long)count - clusters);
		}
		/* Add runlist terminator element. */
		rl[rlpos].vcn = rl[rlpos - 1].vcn + rl[rlpos - 1].length;
		rl[rlpos].lcn = LCN_RL_NOT_MAPPED;
		rl[rlpos].length = 0;
		/* Deallocate all allocated clusters. */
		ntfs_log_trace("Deallocating allocated clusters.\n");
		ntfs_cluster_free_from_rl(vol, rl);
		/* Free the runlist. */
		free(rl);
		rl = NULL;
	} else {
		if (err == ENOSPC) {
			ntfs_log_trace("No space left at all, err = ENOSPC, first "
					"free lcn = 0x%llx.\n",
					(long long)vol->data1_zone_pos);
		}
	}
	ntfs_log_trace("rl = NULL, going to done_err_ret.\n");
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
	ntfs_log_trace("Entering.\n");

	for (; rl->length; rl++) {

		ntfs_log_trace("Dealloc lcn 0x%llx, len 0x%llx.\n",
			       (long long)rl->lcn, (long long)rl->length);

		if (rl->lcn >= 0 && ntfs_bitmap_clear_run(vol->lcnbmp_na,
				rl->lcn, rl->length)) {
			int eo = errno;
			ntfs_log_trace("Eeek! Deallocation of clusters failed.\n");
			errno = eo;
			return -1;
		}
	}
	return 0;
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
	s64 nr_freed, delta, to_free;

	if (!vol || !vol->lcnbmp_na || !na || start_vcn < 0 ||
			(count < 0 && count != -1)) {
		ntfs_log_trace("Invalid arguments!\n");
		errno = EINVAL;
		return -1;
	}
	ntfs_log_trace("Entering for inode 0x%llx, attr 0x%x, count 0x%llx, "
		       "vcn 0x%llx.\n", (unsigned long long)na->ni->mft_no,
		       na->type, (long long)count, (long long)start_vcn);

	rl = ntfs_attr_find_vcn(na, start_vcn);
	if (!rl) {
		if (errno == ENOENT)
			return 0;
		else
			return -1;
	}

	if (rl->lcn < 0 && rl->lcn != LCN_HOLE) {
		errno = EIO;
		return -1;
	}

	/* Find the starting cluster inside the run that needs freeing. */
	delta = start_vcn - rl->vcn;

	/* The number of clusters in this run that need freeing. */
	to_free = rl->length - delta;
	if (count >= 0 && to_free > count)
		to_free = count;

	if (rl->lcn != LCN_HOLE) {
		/* Do the actual freeing of the clusters in this run. */
		if (ntfs_bitmap_clear_run(vol->lcnbmp_na, rl->lcn + delta,
				to_free))
			return -1;
		/* We have freed @to_free real clusters. */
		nr_freed = to_free;
	} else {
		/* No real clusters were freed. */
		nr_freed = 0;
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
			ntfs_log_trace("Eeek! invalid lcn (= %lli).  Should attempt "
					"to map runlist!  Leaving inconsistent "
					"metadata!\n", (long long)rl->lcn);
			errno = EIO;
			return -1;
		}

		/* The number of clusters in this run that need freeing. */
		to_free = rl->length;
		if (count >= 0 && to_free > count)
			to_free = count;

		if (rl->lcn != LCN_HOLE) {
			/* Do the actual freeing of the clusters in the run. */
			if (ntfs_bitmap_clear_run(vol->lcnbmp_na, rl->lcn,
					to_free)) {
				int eo = errno;

				// FIXME: Eeek! We need rollback! (AIA)
				ntfs_log_trace("Eeek!  bitmap clear run failed.  "
						"Leaving inconsistent metadata!\n");
				errno = eo;
				return -1;
			}
			/* We have freed @to_free real clusters. */
			nr_freed += to_free;
		}

		if (count >= 0)
			count -= to_free;
	}

	if (count != -1 && count != 0) {
		// FIXME: Eeek! BUG()
		ntfs_log_trace("Eeek!  count still not zero (= %lli).  Leaving "
				"inconsistent metadata!\n", (long long)count);
		errno = EIO;
		return -1;
	}

	/* Done. Return the number of actual clusters that were freed. */
	return nr_freed;
}
