/**
 * bootsect.c - Boot sector handling code. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2000-2006 Anton Altaparmakov
 * Copyright (c) 2003-2008 Szabolcs Szakacsits
 * Copyright (c)      2005 Yura Pakhuchiy
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

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "compat.h"
#include "bootsect.h"
#include "debug.h"
#include "logging.h"

/**
 * ntfs_boot_sector_is_ntfs - check if buffer contains a valid ntfs boot sector
 * @b:		buffer containing putative boot sector to analyze
 * @silent:	if zero, output progress messages to stderr
 *
 * Check if the buffer @b contains a valid ntfs boot sector. The buffer @b
 * must be at least 512 bytes in size.
 *
 * If @silent is zero, output progress messages to stderr. Otherwise, do not
 * output any messages (except when configured with --enable-debug in which
 * case warning/debug messages may be displayed).
 *
 * Return TRUE if @b contains a valid ntfs boot sector and FALSE if not.
 */
BOOL ntfs_boot_sector_is_ntfs(NTFS_BOOT_SECTOR *b)
{
	u32 i;
	BOOL ret = FALSE;

	ntfs_log_debug("Beginning bootsector check.\n");

	ntfs_log_debug("Checking OEMid, NTFS signature.\n");
	if (b->oem_id != cpu_to_le64(0x202020205346544eULL)) { /* "NTFS    " */
		ntfs_log_error("NTFS signature is missing.\n");
		goto not_ntfs;
	}

	ntfs_log_debug("Checking bytes per sector.\n");
	if (le16_to_cpu(b->bpb.bytes_per_sector) <  256 ||
	    le16_to_cpu(b->bpb.bytes_per_sector) > 4096) {
		ntfs_log_error("Unexpected bytes per sector value (%d).\n", 
			       le16_to_cpu(b->bpb.bytes_per_sector));
		goto not_ntfs;
	}

	ntfs_log_debug("Checking sectors per cluster.\n");
	switch (b->bpb.sectors_per_cluster) {
	case 1: case 2: case 4: case 8: case 16: case 32: case 64: case 128:
		break;
	default:
		ntfs_log_error("Unexpected sectors per cluster value (%d).\n",
			       b->bpb.sectors_per_cluster);
		goto not_ntfs;
	}

	ntfs_log_debug("Checking cluster size.\n");
	i = (u32)le16_to_cpu(b->bpb.bytes_per_sector) * 
		b->bpb.sectors_per_cluster;
	if (i > 65536) {
		ntfs_log_error("Unexpected cluster size (%d).\n", i);
		goto not_ntfs;
	}

	ntfs_log_debug("Checking reserved fields are zero.\n");
	if (le16_to_cpu(b->bpb.reserved_sectors) ||
	    le16_to_cpu(b->bpb.root_entries) ||
	    le16_to_cpu(b->bpb.sectors) ||
	    le16_to_cpu(b->bpb.sectors_per_fat) ||
	    le32_to_cpu(b->bpb.large_sectors) ||
	    b->bpb.fats) {
		ntfs_log_error("Reserved fields aren't zero "
			       "(%d, %d, %d, %d, %d, %d).\n",
			       le16_to_cpu(b->bpb.reserved_sectors),
			       le16_to_cpu(b->bpb.root_entries),
			       le16_to_cpu(b->bpb.sectors),
			       le16_to_cpu(b->bpb.sectors_per_fat),
			       le32_to_cpu(b->bpb.large_sectors),
			       b->bpb.fats);
		goto not_ntfs;
	}

	ntfs_log_debug("Checking clusters per mft record.\n");
	if ((u8)b->clusters_per_mft_record < 0xe1 ||
	    (u8)b->clusters_per_mft_record > 0xf7) {
		switch (b->clusters_per_mft_record) {
		case 1: case 2: case 4: case 8: case 0x10: case 0x20: case 0x40:
			break;
		default:
			ntfs_log_error("Unexpected clusters per mft record "
				       "(%d).\n", b->clusters_per_mft_record);
			goto not_ntfs;
		}
	}

	ntfs_log_debug("Checking clusters per index block.\n");
	if ((u8)b->clusters_per_index_record < 0xe1 ||
	    (u8)b->clusters_per_index_record > 0xf7) {
		switch (b->clusters_per_index_record) {
		case 1: case 2: case 4: case 8: case 0x10: case 0x20: case 0x40:
			break;
		default:
			ntfs_log_error("Unexpected clusters per index record "
				       "(%d).\n", b->clusters_per_index_record);
			goto not_ntfs;
		}
	}

	if (b->end_of_sector_marker != cpu_to_le16(0xaa55))
		ntfs_log_debug("Warning: Bootsector has invalid end of sector "
			       "marker.\n");

	ntfs_log_debug("Bootsector check completed successfully.\n");

	ret = TRUE;
not_ntfs:
	return ret;
}

static const char *last_sector_error =
"HINTS: Either the volume is a RAID/LDM but it wasn't setup yet,\n"
"   or it was not setup correctly (e.g. by not using mdadm --build ...),\n"
"   or a wrong device is tried to be mounted,\n"
"   or the partition table is corrupt (partition is smaller than NTFS),\n"
"   or the NTFS boot sector is corrupt (NTFS size is not valid).\n";

/**
 * ntfs_boot_sector_parse - setup an ntfs volume from an ntfs boot sector
 * @vol:	ntfs_volume to setup
 * @bs:		buffer containing ntfs boot sector to parse
 *
 * Parse the ntfs bootsector @bs and setup the ntfs volume @vol with the
 * obtained values.
 *
 * Return 0 on success or -1 on error with errno set to the error code EINVAL.
 */
int ntfs_boot_sector_parse(ntfs_volume *vol, const NTFS_BOOT_SECTOR *bs)
{
	s64 sectors;
	u8  sectors_per_cluster;
	s8  c;

	/* We return -1 with errno = EINVAL on error. */
	errno = EINVAL;

	vol->sector_size = le16_to_cpu(bs->bpb.bytes_per_sector);
	vol->sector_size_bits = ffs(vol->sector_size) - 1;
	ntfs_log_debug("SectorSize = 0x%x\n", vol->sector_size);
	ntfs_log_debug("SectorSizeBits = %u\n", vol->sector_size_bits);
	/*
	 * The bounds checks on mft_lcn and mft_mirr_lcn (i.e. them being
	 * below or equal the number_of_clusters) really belong in the
	 * ntfs_boot_sector_is_ntfs but in this way we can just do this once.
	 */
	sectors_per_cluster = bs->bpb.sectors_per_cluster;
	ntfs_log_debug("SectorsPerCluster = 0x%x\n", sectors_per_cluster);
	if (sectors_per_cluster & (sectors_per_cluster - 1)) {
		ntfs_log_error("sectors_per_cluster (%d) is not a power of 2."
			       "\n", sectors_per_cluster);
		return -1;
	}
	
	sectors = sle64_to_cpu(bs->number_of_sectors);
	ntfs_log_debug("NumberOfSectors = %lld\n", (long long)sectors);
	if (!sectors) {
		ntfs_log_error("Volume size is set to zero.\n");
		return -1;
	}
	if (vol->dev->d_ops->seek(vol->dev, 
				  (sectors - 1) << vol->sector_size_bits,
				  SEEK_SET) == -1) {
		ntfs_log_perror("Failed to read last sector (%lld)",
			       	(long long)sectors);
		ntfs_log_error("%s", last_sector_error);
		return -1;
	}
	
	vol->nr_clusters =  sectors >> (ffs(sectors_per_cluster) - 1);

	vol->mft_lcn = sle64_to_cpu(bs->mft_lcn);
	vol->mftmirr_lcn = sle64_to_cpu(bs->mftmirr_lcn);
	ntfs_log_debug("MFT LCN = %lld\n", (long long)vol->mft_lcn);
	ntfs_log_debug("MFTMirr LCN = %lld\n", (long long)vol->mftmirr_lcn);
	if (vol->mft_lcn     > vol->nr_clusters ||
	    vol->mftmirr_lcn > vol->nr_clusters) {
		ntfs_log_error("$MFT LCN (%lld) or $MFTMirr LCN (%lld) is "
			      "greater than the number of clusters (%lld).\n",
			      (long long)vol->mft_lcn, (long long)vol->mftmirr_lcn,
			      (long long)vol->nr_clusters);
		return -1;
	}
	
	vol->cluster_size = sectors_per_cluster * vol->sector_size;
	if (vol->cluster_size & (vol->cluster_size - 1)) {
		ntfs_log_error("cluster_size (%d) is not a power of 2.\n",
			       vol->cluster_size);
		return -1;
	}
	vol->cluster_size_bits = ffs(vol->cluster_size) - 1;
	/*
	 * Need to get the clusters per mft record and handle it if it is
	 * negative. Then calculate the mft_record_size. A value of 0x80 is
	 * illegal, thus signed char is actually ok!
	 */
	c = bs->clusters_per_mft_record;
	ntfs_log_debug("ClusterSize = 0x%x\n", (unsigned)vol->cluster_size);
	ntfs_log_debug("ClusterSizeBits = %u\n", vol->cluster_size_bits);
	ntfs_log_debug("ClustersPerMftRecord = 0x%x\n", c);
	/*
	 * When clusters_per_mft_record is negative, it means that it is to
	 * be taken to be the negative base 2 logarithm of the mft_record_size
	 * min bytes. Then:
	 *	 mft_record_size = 2^(-clusters_per_mft_record) bytes.
	 */
	if (c < 0)
		vol->mft_record_size = 1 << -c;
	else
		vol->mft_record_size = c << vol->cluster_size_bits;
	if (vol->mft_record_size & (vol->mft_record_size - 1)) {
		ntfs_log_error("mft_record_size (%d) is not a power of 2.\n",
			       vol->mft_record_size);
		return -1;
	}
	vol->mft_record_size_bits = ffs(vol->mft_record_size) - 1;
	ntfs_log_debug("MftRecordSize = 0x%x\n", (unsigned)vol->mft_record_size);
	ntfs_log_debug("MftRecordSizeBits = %u\n", vol->mft_record_size_bits);
	/* Same as above for INDX record. */
	c = bs->clusters_per_index_record;
	ntfs_log_debug("ClustersPerINDXRecord = 0x%x\n", c);
	if (c < 0)
		vol->indx_record_size = 1 << -c;
	else
		vol->indx_record_size = c << vol->cluster_size_bits;
	vol->indx_record_size_bits = ffs(vol->indx_record_size) - 1;
	ntfs_log_debug("INDXRecordSize = 0x%x\n", (unsigned)vol->indx_record_size);
	ntfs_log_debug("INDXRecordSizeBits = %u\n", vol->indx_record_size_bits);
	/*
	 * Work out the size of the MFT mirror in number of mft records. If the
	 * cluster size is less than or equal to the size taken by four mft
	 * records, the mft mirror stores the first four mft records. If the
	 * cluster size is bigger than the size taken by four mft records, the
	 * mft mirror contains as many mft records as will fit into one
	 * cluster.
	 */
	if (vol->cluster_size <= 4 * vol->mft_record_size)
		vol->mftmirr_size = 4;
	else
		vol->mftmirr_size = vol->cluster_size / vol->mft_record_size;
	return 0;
}

