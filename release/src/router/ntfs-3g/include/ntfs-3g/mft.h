/*
 * mft.h - Exports for MFT record handling. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2000-2002 Anton Altaparmakov
 * Copyright (c) 2004-2005 Richard Russon
 * Copyright (c) 2006-2008 Szabolcs Szakacsits
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

#ifndef _NTFS_MFT_H
#define _NTFS_MFT_H

#include "volume.h"
#include "inode.h"
#include "layout.h"
#include "logging.h"

extern int ntfs_mft_records_read(const ntfs_volume *vol, const MFT_REF mref,
		const s64 count, MFT_RECORD *b);

/**
 * ntfs_mft_record_read - read a record from the mft
 * @vol:	volume to read from
 * @mref:	mft record number to read
 * @b:		output data buffer
 *
 * Read the mft record specified by @mref from volume @vol into buffer @b.
 * Return 0 on success or -1 on error, with errno set to the error code.
 *
 * The read mft record is mst deprotected and is hence ready to use. The caller
 * should check the record with is_baad_record() in case mst deprotection
 * failed.
 *
 * NOTE: @b has to be at least of size vol->mft_record_size.
 */
static __inline__ int ntfs_mft_record_read(const ntfs_volume *vol,
		const MFT_REF mref, MFT_RECORD *b)
{
	int ret; 
	
	ntfs_log_enter("Entering for inode %lld\n", (long long)MREF(mref));
	ret = ntfs_mft_records_read(vol, mref, 1, b);
	ntfs_log_leave("\n");
	return ret;
}

extern int ntfs_mft_record_check(const ntfs_volume *vol, const MFT_REF mref, 
		MFT_RECORD *m);

extern int ntfs_file_record_read(const ntfs_volume *vol, const MFT_REF mref,
		MFT_RECORD **mrec, ATTR_RECORD **attr);

extern int ntfs_mft_records_write(const ntfs_volume *vol, const MFT_REF mref,
		const s64 count, MFT_RECORD *b);

/**
 * ntfs_mft_record_write - write an mft record to disk
 * @vol:	volume to write to
 * @mref:	mft record number to write
 * @b:		data buffer containing the mft record to write
 *
 * Write the mft record specified by @mref from buffer @b to volume @vol.
 * Return 0 on success or -1 on error, with errno set to the error code.
 *
 * Before the mft record is written, it is mst protected. After the write, it
 * is deprotected again, thus resulting in an increase in the update sequence
 * number inside the buffer @b.
 *
 * NOTE: @b has to be at least of size vol->mft_record_size.
 */
static __inline__ int ntfs_mft_record_write(const ntfs_volume *vol,
		const MFT_REF mref, MFT_RECORD *b)
{
	int ret; 
	
	ntfs_log_enter("Entering for inode %lld\n", (long long)MREF(mref));
	ret = ntfs_mft_records_write(vol, mref, 1, b);
	ntfs_log_leave("\n");
	return ret;
}

/**
 * ntfs_mft_record_get_data_size - return number of bytes used in mft record @b
 * @m:		mft record to get the data size of
 *
 * Takes the mft record @m and returns the number of bytes used in the record
 * or 0 on error (i.e. @m is not a valid mft record).  Zero is not a valid size
 * for an mft record as it at least has to have the MFT_RECORD itself and a
 * zero length attribute of type AT_END, thus making the minimum size 56 bytes.
 *
 * Aside:  The size is independent of NTFS versions 1.x/3.x because the 8-byte
 * alignment of the first attribute mask the difference in MFT_RECORD size
 * between NTFS 1.x and 3.x.  Also, you would expect every mft record to
 * contain an update sequence array as well but that could in theory be
 * non-existent (don't know if Windows' NTFS driver/chkdsk wouldn't view this
 * as corruption in itself though).
 */
static __inline__ u32 ntfs_mft_record_get_data_size(const MFT_RECORD *m)
{
	if (!m || !ntfs_is_mft_record(m->magic))
		return 0;
	/* Get the number of used bytes and return it. */
	return le32_to_cpu(m->bytes_in_use);
}

extern int ntfs_mft_record_layout(const ntfs_volume *vol, const MFT_REF mref,
		MFT_RECORD *mrec);

extern int ntfs_mft_record_format(const ntfs_volume *vol, const MFT_REF mref);

extern ntfs_inode *ntfs_mft_record_alloc(ntfs_volume *vol, ntfs_inode *base_ni);

extern int ntfs_mft_record_free(ntfs_volume *vol, ntfs_inode *ni);

extern int ntfs_mft_usn_dec(MFT_RECORD *mrec);

#endif /* defined _NTFS_MFT_H */

