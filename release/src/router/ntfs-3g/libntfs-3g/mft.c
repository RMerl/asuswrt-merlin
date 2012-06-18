/**
 * mft.c - Mft record handling code. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2000-2004 Anton Altaparmakov
 * Copyright (c) 2004-2005 Richard Russon
 * Copyright (c) 2004-2008 Szabolcs Szakacsits
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <time.h>

#include "compat.h"
#include "types.h"
#include "device.h"
#include "debug.h"
#include "bitmap.h"
#include "attrib.h"
#include "inode.h"
#include "volume.h"
#include "layout.h"
#include "lcnalloc.h"
#include "mft.h"
#include "logging.h"
#include "misc.h"

/**
 * ntfs_mft_records_read - read records from the mft from disk
 * @vol:	volume to read from
 * @mref:	starting mft record number to read
 * @count:	number of mft records to read
 * @b:		output data buffer
 *
 * Read @count mft records starting at @mref from volume @vol into buffer
 * @b. Return 0 on success or -1 on error, with errno set to the error
 * code.
 *
 * If any of the records exceed the initialized size of the $MFT/$DATA
 * attribute, i.e. they cannot possibly be allocated mft records, assume this
 * is a bug and return error code ESPIPE.
 *
 * The read mft records are mst deprotected and are hence ready to use. The
 * caller should check each record with is_baad_record() in case mst
 * deprotection failed.
 *
 * NOTE: @b has to be at least of size @count * vol->mft_record_size.
 */
int ntfs_mft_records_read(const ntfs_volume *vol, const MFT_REF mref,
		const s64 count, MFT_RECORD *b)
{
	s64 br;
	VCN m;

	ntfs_log_trace("inode %llu\n", (unsigned long long)MREF(mref));
	
	if (!vol || !vol->mft_na || !b || count < 0) {
		errno = EINVAL;
		ntfs_log_perror("%s: b=%p  count=%lld  mft=%llu", __FUNCTION__,
			b, (long long)count, (unsigned long long)MREF(mref));
		return -1;
	}
	m = MREF(mref);
	/* Refuse to read non-allocated mft records. */
	if (m + count > vol->mft_na->initialized_size >>
			vol->mft_record_size_bits) {
		errno = ESPIPE;
		ntfs_log_perror("Trying to read non-allocated mft records "
				"(%lld > %lld)", (long long)m + count,
				(long long)vol->mft_na->initialized_size >>
				vol->mft_record_size_bits);
		return -1;
	}
	br = ntfs_attr_mst_pread(vol->mft_na, m << vol->mft_record_size_bits,
			count, vol->mft_record_size, b);
	if (br != count) {
		if (br != -1)
			errno = EIO;
		ntfs_log_perror("Failed to read of MFT, mft=%llu count=%lld "
				"br=%lld", (long long)m, (long long)count,
				(long long)br);
		return -1;
	}
	return 0;
}

/**
 * ntfs_mft_records_write - write mft records to disk
 * @vol:	volume to write to
 * @mref:	starting mft record number to write
 * @count:	number of mft records to write
 * @b:		data buffer containing the mft records to write
 *
 * Write @count mft records starting at @mref from data buffer @b to volume
 * @vol. Return 0 on success or -1 on error, with errno set to the error code.
 *
 * If any of the records exceed the initialized size of the $MFT/$DATA
 * attribute, i.e. they cannot possibly be allocated mft records, assume this
 * is a bug and return error code ESPIPE.
 *
 * Before the mft records are written, they are mst protected. After the write,
 * they are deprotected again, thus resulting in an increase in the update
 * sequence number inside the data buffer @b.
 *
 * If any mft records are written which are also represented in the mft mirror
 * $MFTMirr, we make a copy of the relevant parts of the data buffer @b into a
 * temporary buffer before we do the actual write. Then if at least one mft
 * record was successfully written, we write the appropriate mft records from
 * the copied buffer to the mft mirror, too.
 */
int ntfs_mft_records_write(const ntfs_volume *vol, const MFT_REF mref,
		const s64 count, MFT_RECORD *b)
{
	s64 bw;
	VCN m;
	void *bmirr = NULL;
	int cnt = 0, res = 0;

	if (!vol || !vol->mft_na || vol->mftmirr_size <= 0 || !b || count < 0) {
		errno = EINVAL;
		return -1;
	}
	m = MREF(mref);
	/* Refuse to write non-allocated mft records. */
	if (m + count > vol->mft_na->initialized_size >>
			vol->mft_record_size_bits) {
		errno = ESPIPE;
		ntfs_log_perror("Trying to write non-allocated mft records "
				"(%lld > %lld)", (long long)m + count,
				(long long)vol->mft_na->initialized_size >>
				vol->mft_record_size_bits);
		return -1;
	}
	if (m < vol->mftmirr_size) {
		if (!vol->mftmirr_na) {
			errno = EINVAL;
			return -1;
		}
		cnt = vol->mftmirr_size - m;
		if (cnt > count)
			cnt = count;
		bmirr = ntfs_malloc(cnt * vol->mft_record_size);
		if (!bmirr)
			return -1;
		memcpy(bmirr, b, cnt * vol->mft_record_size);
	}
	bw = ntfs_attr_mst_pwrite(vol->mft_na, m << vol->mft_record_size_bits,
			count, vol->mft_record_size, b);
	if (bw != count) {
		if (bw != -1)
			errno = EIO;
		if (bw >= 0)
			ntfs_log_debug("Error: partial write while writing $Mft "
					"record(s)!\n");
		else
			ntfs_log_perror("Error writing $Mft record(s)");
		res = errno;
	}
	if (bmirr && bw > 0) {
		if (bw < cnt)
			cnt = bw;
		bw = ntfs_attr_mst_pwrite(vol->mftmirr_na,
				m << vol->mft_record_size_bits, cnt,
				vol->mft_record_size, bmirr);
		if (bw != cnt) {
			if (bw != -1)
				errno = EIO;
			ntfs_log_debug("Error: failed to sync $MFTMirr! Run "
					"chkdsk.\n");
			res = errno;
		}
	}
	free(bmirr);
	if (!res)
		return res;
	errno = res;
	return -1;
}

int ntfs_mft_record_check(const ntfs_volume *vol, const MFT_REF mref, 
			  MFT_RECORD *m)
{			  
	ATTR_RECORD *a;
	int ret = -1;
	
	if (!ntfs_is_file_record(m->magic)) {
		ntfs_log_error("Record %llu has no FILE magic (0x%x)\n",
			       (unsigned long long)MREF(mref), *(le32 *)m);
		goto err_out;
	}
	
	if (le32_to_cpu(m->bytes_allocated) != vol->mft_record_size) {
		ntfs_log_error("Record %llu has corrupt allocation size "
			       "(%u <> %u)\n", (unsigned long long)MREF(mref),
			       vol->mft_record_size,
			       le32_to_cpu(m->bytes_allocated));
		goto err_out;
	}
	
	a = (ATTR_RECORD *)((char *)m + le16_to_cpu(m->attrs_offset));
	if (p2n(a) < p2n(m) || (char *)a > (char *)m + vol->mft_record_size) {
		ntfs_log_error("Record %llu is corrupt\n",
			       (unsigned long long)MREF(mref));
		goto err_out;
	}
	
	ret = 0;
err_out:
	if (ret)
		errno = EIO;
	return ret;
}

/**
 * ntfs_file_record_read - read a FILE record from the mft from disk
 * @vol:	volume to read from
 * @mref:	mft reference specifying mft record to read
 * @mrec:	address of pointer in which to return the mft record
 * @attr:	address of pointer in which to return the first attribute
 *
 * Read a FILE record from the mft of @vol from the storage medium. @mref
 * specifies the mft record to read, including the sequence number, which can
 * be 0 if no sequence number checking is to be performed.
 *
 * The function allocates a buffer large enough to hold the mft record and
 * reads the record into the buffer (mst deprotecting it in the process).
 * *@mrec is then set to point to the buffer.
 *
 * If @attr is not NULL, *@attr is set to point to the first attribute in the
 * mft record, i.e. *@attr is a pointer into *@mrec.
 *
 * Return 0 on success, or -1 on error, with errno set to the error code.
 *
 * The read mft record is checked for having the magic FILE,
 * and for having a matching sequence number (if MSEQNO(*@mref) != 0).
 * If either of these fails, -1 is returned and errno is set to EIO. If you get
 * this, but you still want to read the mft record (e.g. in order to correct
 * it), use ntfs_mft_record_read() directly.
 *
 * Note: Caller has to free *@mrec when finished.
 *
 * Note: We do not check if the mft record is flagged in use. The caller can
 *	 check if desired.
 */
int ntfs_file_record_read(const ntfs_volume *vol, const MFT_REF mref,
		MFT_RECORD **mrec, ATTR_RECORD **attr)
{
	MFT_RECORD *m;

	if (!vol || !mrec) {
		errno = EINVAL;
		ntfs_log_perror("%s: mrec=%p", __FUNCTION__, mrec);
		return -1;
	}
	
	m = *mrec;
	if (!m) {
		m = ntfs_malloc(vol->mft_record_size);
		if (!m)
			return -1;
	}
	if (ntfs_mft_record_read(vol, mref, m))
		goto err_out;

	if (ntfs_mft_record_check(vol, mref, m))
		goto err_out;
	
	if (MSEQNO(mref) && MSEQNO(mref) != le16_to_cpu(m->sequence_number)) {
		ntfs_log_error("Record %llu has wrong SeqNo (%d <> %d)\n",
			       (unsigned long long)MREF(mref), MSEQNO(mref),
			       le16_to_cpu(m->sequence_number));
		errno = EIO;
		goto err_out;
	}
	*mrec = m;
	if (attr)
		*attr = (ATTR_RECORD*)((char*)m + le16_to_cpu(m->attrs_offset));
	return 0;
err_out:
	if (m != *mrec)
		free(m);
	return -1;
}

/**
 * ntfs_mft_record_layout - layout an mft record into a memory buffer
 * @vol:	volume to which the mft record will belong
 * @mref:	mft reference specifying the mft record number
 * @mrec:	destination buffer of size >= @vol->mft_record_size bytes
 *
 * Layout an empty, unused mft record with the mft reference @mref into the
 * buffer @m.  The volume @vol is needed because the mft record structure was
 * modified in NTFS 3.1 so we need to know which volume version this mft record
 * will be used on.
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 */
int ntfs_mft_record_layout(const ntfs_volume *vol, const MFT_REF mref,
		MFT_RECORD *mrec)
{
	ATTR_RECORD *a;

	if (!vol || !mrec) {
		errno = EINVAL;
		ntfs_log_perror("%s: mrec=%p", __FUNCTION__, mrec);
		return -1;
	}
	/* Aligned to 2-byte boundary. */
	if (vol->major_ver < 3 || (vol->major_ver == 3 && !vol->minor_ver))
		mrec->usa_ofs = cpu_to_le16((sizeof(MFT_RECORD_OLD) + 1) & ~1);
	else {
		/* Abort if mref is > 32 bits. */
		if (MREF(mref) & 0x0000ffff00000000ull) {
			errno = ERANGE;
			ntfs_log_perror("Mft reference exceeds 32 bits");
			return -1;
		}
		mrec->usa_ofs = cpu_to_le16((sizeof(MFT_RECORD) + 1) & ~1);
		/*
		 * Set the NTFS 3.1+ specific fields while we know that the
		 * volume version is 3.1+.
		 */
		mrec->reserved = cpu_to_le16(0);
		mrec->mft_record_number = cpu_to_le32(MREF(mref));
	}
	mrec->magic = magic_FILE;
	if (vol->mft_record_size >= NTFS_BLOCK_SIZE)
		mrec->usa_count = cpu_to_le16(vol->mft_record_size /
				NTFS_BLOCK_SIZE + 1);
	else {
		mrec->usa_count = cpu_to_le16(1);
		ntfs_log_error("Sector size is bigger than MFT record size.  "
				"Setting usa_count to 1.  If Windows chkdsk "
				"reports this as corruption, please email %s "
				"stating that you saw this message and that "
				"the file system created was corrupt.  "
				"Thank you.\n", NTFS_DEV_LIST);
	}
	/* Set the update sequence number to 1. */
	*(u16*)((u8*)mrec + le16_to_cpu(mrec->usa_ofs)) = cpu_to_le16(1);
	mrec->lsn = cpu_to_le64(0ull);
	mrec->sequence_number = cpu_to_le16(1);
	mrec->link_count = cpu_to_le16(0);
	/* Aligned to 8-byte boundary. */
	mrec->attrs_offset = cpu_to_le16((le16_to_cpu(mrec->usa_ofs) +
			(le16_to_cpu(mrec->usa_count) << 1) + 7) & ~7);
	mrec->flags = cpu_to_le16(0);
	/*
	 * Using attrs_offset plus eight bytes (for the termination attribute),
	 * aligned to 8-byte boundary.
	 */
	mrec->bytes_in_use = cpu_to_le32((le16_to_cpu(mrec->attrs_offset) + 8 +
			7) & ~7);
	mrec->bytes_allocated = cpu_to_le32(vol->mft_record_size);
	mrec->base_mft_record = cpu_to_le64((MFT_REF)0);
	mrec->next_attr_instance = cpu_to_le16(0);
	a = (ATTR_RECORD*)((u8*)mrec + le16_to_cpu(mrec->attrs_offset));
	a->type = AT_END;
	a->length = cpu_to_le32(0);
	/* Finally, clear the unused part of the mft record. */
	memset((u8*)a + 8, 0, vol->mft_record_size - ((u8*)a + 8 - (u8*)mrec));
	return 0;
}

/**
 * ntfs_mft_record_format - format an mft record on an ntfs volume
 * @vol:	volume on which to format the mft record
 * @mref:	mft reference specifying mft record to format
 *
 * Format the mft record with the mft reference @mref in $MFT/$DATA, i.e. lay
 * out an empty, unused mft record in memory and write it to the volume @vol.
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 */
int ntfs_mft_record_format(const ntfs_volume *vol, const MFT_REF mref)
{
	MFT_RECORD *m;
	int ret = -1;

	ntfs_log_enter("Entering\n");
	
	m = ntfs_calloc(vol->mft_record_size);
	if (!m)
		goto out;
	
	if (ntfs_mft_record_layout(vol, mref, m))
		goto free_m;
	
	if (ntfs_mft_record_write(vol, mref, m))
		goto free_m;
	
	ret = 0;
free_m:
	free(m);
out:	
	ntfs_log_leave("\n");
	return ret;
}

static const char *es = "  Leaving inconsistent metadata.  Run chkdsk.";

/**
 * ntfs_ffz - Find the first unset (zero) bit in a word
 * @word:
 *
 * Description...
 *
 * Returns:
 */
static inline unsigned int ntfs_ffz(unsigned int word)
{
	return ffs(~word) - 1;
}

static int ntfs_is_mft(ntfs_inode *ni)
{
	if (ni && ni->mft_no == FILE_MFT)
		return 1;
	return 0;
}

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define RESERVED_MFT_RECORDS   64

/**
 * ntfs_mft_bitmap_find_free_rec - find a free mft record in the mft bitmap
 * @vol:	volume on which to search for a free mft record
 * @base_ni:	open base inode if allocating an extent mft record or NULL
 *
 * Search for a free mft record in the mft bitmap attribute on the ntfs volume
 * @vol.
 *
 * If @base_ni is NULL start the search at the default allocator position.
 *
 * If @base_ni is not NULL start the search at the mft record after the base
 * mft record @base_ni.
 *
 * Return the free mft record on success and -1 on error with errno set to the
 * error code.  An error code of ENOSPC means that there are no free mft
 * records in the currently initialized mft bitmap.
 */
static int ntfs_mft_bitmap_find_free_rec(ntfs_volume *vol, ntfs_inode *base_ni)
{
	s64 pass_end, ll, data_pos, pass_start, ofs, bit;
	ntfs_attr *mftbmp_na;
	u8 *buf, *byte;
	unsigned int size;
	u8 pass, b;
	int ret = -1;

	ntfs_log_enter("Entering\n");
	
	mftbmp_na = vol->mftbmp_na;
	/*
	 * Set the end of the pass making sure we do not overflow the mft
	 * bitmap.
	 */
	size = PAGE_SIZE;
	pass_end = vol->mft_na->allocated_size >> vol->mft_record_size_bits;
	ll = mftbmp_na->initialized_size << 3;
	if (pass_end > ll)
		pass_end = ll;
	pass = 1;
	if (!base_ni)
		data_pos = vol->mft_data_pos;
	else
		data_pos = base_ni->mft_no + 1;
	if (data_pos < RESERVED_MFT_RECORDS)
		data_pos = RESERVED_MFT_RECORDS;
	if (data_pos >= pass_end) {
		data_pos = RESERVED_MFT_RECORDS;
		pass = 2;
		/* This happens on a freshly formatted volume. */
		if (data_pos >= pass_end) {
			errno = ENOSPC;
			goto leave;
		}
	}
	if (ntfs_is_mft(base_ni)) {
		data_pos = 0;
		pass = 2;
	}
	pass_start = data_pos;
	buf = ntfs_malloc(PAGE_SIZE);
	if (!buf)
		goto leave;
	
	ntfs_log_debug("Starting bitmap search: pass %u, pass_start 0x%llx, "
			"pass_end 0x%llx, data_pos 0x%llx.\n", pass,
			(long long)pass_start, (long long)pass_end,
			(long long)data_pos);
#ifdef DEBUG
	byte = NULL;
	b = 0;
#endif
	/* Loop until a free mft record is found. */
	for (; pass <= 2; size = PAGE_SIZE) {
		/* Cap size to pass_end. */
		ofs = data_pos >> 3;
		ll = ((pass_end + 7) >> 3) - ofs;
		if (size > ll)
			size = ll;
		ll = ntfs_attr_pread(mftbmp_na, ofs, size, buf);
		if (ll < 0) {
			ntfs_log_perror("Failed to read $MFT bitmap");
			free(buf);
			goto leave;
		}
		ntfs_log_debug("Read 0x%llx bytes.\n", (long long)ll);
		/* If we read at least one byte, search @buf for a zero bit. */
		if (ll) {
			size = ll << 3;
			bit = data_pos & 7;
			data_pos &= ~7ull;
			ntfs_log_debug("Before inner for loop: size 0x%x, "
					"data_pos 0x%llx, bit 0x%llx, "
					"*byte 0x%hhx, b %u.\n", size,
					(long long)data_pos, (long long)bit,
					byte ? *byte : -1, b);
			for (; bit < size && data_pos + bit < pass_end;
					bit &= ~7ull, bit += 8) {
				/* 
				 * If we're extending $MFT and running out of the first
				 * mft record (base record) then give up searching since
				 * no guarantee that the found record will be accessible.
				 */
				if (ntfs_is_mft(base_ni) && bit > 400)
					goto out;
				
				byte = buf + (bit >> 3);
				if (*byte == 0xff)
					continue;
				
				/* Note: ffz() result must be zero based. */
				b = ntfs_ffz((unsigned long)*byte);
				if (b < 8 && b >= (bit & 7)) {
					free(buf);
					ret = data_pos + (bit & ~7ull) + b;
					goto leave;
				}
			}
			ntfs_log_debug("After inner for loop: size 0x%x, "
					"data_pos 0x%llx, bit 0x%llx, "
					"*byte 0x%hhx, b %u.\n", size,
					(long long)data_pos, (long long)bit,
					byte ? *byte : -1, b);
			data_pos += size;
			/*
			 * If the end of the pass has not been reached yet,
			 * continue searching the mft bitmap for a zero bit.
			 */
			if (data_pos < pass_end)
				continue;
		}
		/* Do the next pass. */
		pass++;
		if (pass == 2) {
			/*
			 * Starting the second pass, in which we scan the first
			 * part of the zone which we omitted earlier.
			 */
			pass_end = pass_start;
			data_pos = pass_start = RESERVED_MFT_RECORDS;
			ntfs_log_debug("pass %i, pass_start 0x%llx, pass_end "
					"0x%llx.\n", pass, (long long)pass_start,
					(long long)pass_end);
			if (data_pos >= pass_end)
				break;
		}
	}
	/* No free mft records in currently initialized mft bitmap. */
out:	
	free(buf);
	errno = ENOSPC;
leave:
	ntfs_log_leave("\n");
	return ret;
}

static int ntfs_mft_attr_extend(ntfs_attr *na)
{
	int ret = STATUS_ERROR;
	ntfs_log_enter("Entering\n");

	if (!NInoAttrList(na->ni)) {
		if (ntfs_inode_add_attrlist(na->ni)) {
			ntfs_log_perror("%s: Can not add attrlist #3", __FUNCTION__);
			goto out;
		}
		/* We can't sync the $MFT inode since its runlist is bogus. */
		ret = STATUS_KEEP_SEARCHING;
		goto out;
	}

	if (ntfs_attr_update_mapping_pairs(na, 0)) {
		ntfs_log_perror("%s: MP update failed", __FUNCTION__);
		goto out;
	}
	
	ret = STATUS_OK;
out:	
	ntfs_log_leave("\n");
	return ret;
}

/**
 * ntfs_mft_bitmap_extend_allocation_i - see ntfs_mft_bitmap_extend_allocation
 */
static int ntfs_mft_bitmap_extend_allocation_i(ntfs_volume *vol)
{
	LCN lcn;
	s64 ll = 0; /* silence compiler warning */
	ntfs_attr *mftbmp_na;
	runlist_element *rl, *rl2 = NULL; /* silence compiler warning */
	ntfs_attr_search_ctx *ctx;
	MFT_RECORD *m = NULL; /* silence compiler warning */
	ATTR_RECORD *a = NULL; /* silence compiler warning */
	int err, mp_size;
	int ret = STATUS_ERROR;
	u32 old_alen = 0; /* silence compiler warning */
	BOOL mp_rebuilt = FALSE;
	BOOL update_mp = FALSE;

	mftbmp_na = vol->mftbmp_na;
	/*
	 * Determine the last lcn of the mft bitmap.  The allocated size of the
	 * mft bitmap cannot be zero so we are ok to do this.
	 */
	rl = ntfs_attr_find_vcn(mftbmp_na, (mftbmp_na->allocated_size - 1) >>
			vol->cluster_size_bits);
	if (!rl || !rl->length || rl->lcn < 0) {
		ntfs_log_error("Failed to determine last allocated "
				"cluster of mft bitmap attribute.\n");
		if (rl)
			errno = EIO;
		return STATUS_ERROR;
	}
	lcn = rl->lcn + rl->length;
	
	rl2 = ntfs_cluster_alloc(vol, rl[1].vcn, 1, lcn, DATA_ZONE);
	if (!rl2) {
		ntfs_log_error("Failed to allocate a cluster for "
				"the mft bitmap.\n");
		return STATUS_ERROR;
	}
	rl = ntfs_runlists_merge(mftbmp_na->rl, rl2);
	if (!rl) {
		err = errno;
		ntfs_log_error("Failed to merge runlists for mft "
				"bitmap.\n");
		if (ntfs_cluster_free_from_rl(vol, rl2))
			ntfs_log_error("Failed to deallocate "
					"cluster.%s\n", es);
		free(rl2);
		errno = err;
		return STATUS_ERROR;
	}
	mftbmp_na->rl = rl;
	ntfs_log_debug("Adding one run to mft bitmap.\n");
	/* Find the last run in the new runlist. */
	for (; rl[1].length; rl++)
		;
	/*
	 * Update the attribute record as well.  Note: @rl is the last
	 * (non-terminator) runlist element of mft bitmap.
	 */
	ctx = ntfs_attr_get_search_ctx(mftbmp_na->ni, NULL);
	if (!ctx)
		goto undo_alloc;

	if (ntfs_attr_lookup(mftbmp_na->type, mftbmp_na->name,
			mftbmp_na->name_len, 0, rl[1].vcn, NULL, 0, ctx)) {
		ntfs_log_error("Failed to find last attribute extent of "
				"mft bitmap attribute.\n");
		goto undo_alloc;
	}
	m = ctx->mrec;
	a = ctx->attr;
	ll = sle64_to_cpu(a->lowest_vcn);
	rl2 = ntfs_attr_find_vcn(mftbmp_na, ll);
	if (!rl2 || !rl2->length) {
		ntfs_log_error("Failed to determine previous last "
				"allocated cluster of mft bitmap attribute.\n");
		if (rl2)
			errno = EIO;
		goto undo_alloc;
	}
	/* Get the size for the new mapping pairs array for this extent. */
	mp_size = ntfs_get_size_for_mapping_pairs(vol, rl2, ll, INT_MAX);
	if (mp_size <= 0) {
		ntfs_log_error("Get size for mapping pairs failed for "
				"mft bitmap attribute extent.\n");
		goto undo_alloc;
	}
	/* Expand the attribute record if necessary. */
	old_alen = le32_to_cpu(a->length);
	if (ntfs_attr_record_resize(m, a, mp_size +
			le16_to_cpu(a->mapping_pairs_offset))) {
		ntfs_log_info("extending $MFT bitmap\n");
		ret = ntfs_mft_attr_extend(vol->mftbmp_na);
		if (ret == STATUS_OK)
			goto ok;
		if (ret == STATUS_ERROR) {
			ntfs_log_perror("%s: ntfs_mft_attr_extend failed", __FUNCTION__);
			update_mp = TRUE;
		}
		goto undo_alloc;
	}
	mp_rebuilt = TRUE;
	/* Generate the mapping pairs array directly into the attr record. */
	if (ntfs_mapping_pairs_build(vol, (u8*)a +
			le16_to_cpu(a->mapping_pairs_offset), mp_size, rl2, ll,
			NULL)) {
		ntfs_log_error("Failed to build mapping pairs array for "
				"mft bitmap attribute.\n");
		errno = EIO;
		goto undo_alloc;
	}
	/* Update the highest_vcn. */
	a->highest_vcn = cpu_to_sle64(rl[1].vcn - 1);
	/*
	 * We now have extended the mft bitmap allocated_size by one cluster.
	 * Reflect this in the ntfs_attr structure and the attribute record.
	 */
	if (a->lowest_vcn) {
		/*
		 * We are not in the first attribute extent, switch to it, but
		 * first ensure the changes will make it to disk later.
		 */
		ntfs_inode_mark_dirty(ctx->ntfs_ino);
		ntfs_attr_reinit_search_ctx(ctx);
		if (ntfs_attr_lookup(mftbmp_na->type, mftbmp_na->name,
				mftbmp_na->name_len, 0, 0, NULL, 0, ctx)) {
			ntfs_log_error("Failed to find first attribute "
					"extent of mft bitmap attribute.\n");
			goto restore_undo_alloc;
		}
		a = ctx->attr;
	}
ok:
	mftbmp_na->allocated_size += vol->cluster_size;
	a->allocated_size = cpu_to_sle64(mftbmp_na->allocated_size);
	/* Ensure the changes make it to disk. */
	ntfs_inode_mark_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	return STATUS_OK;

restore_undo_alloc:
	err = errno;
	ntfs_attr_reinit_search_ctx(ctx);
	if (ntfs_attr_lookup(mftbmp_na->type, mftbmp_na->name,
			mftbmp_na->name_len, 0, rl[1].vcn, NULL, 0, ctx)) {
		ntfs_log_error("Failed to find last attribute extent of "
				"mft bitmap attribute.%s\n", es);
		ntfs_attr_put_search_ctx(ctx);
		mftbmp_na->allocated_size += vol->cluster_size;
		/*
		 * The only thing that is now wrong is ->allocated_size of the
		 * base attribute extent which chkdsk should be able to fix.
		 */
		errno = err;
		return STATUS_ERROR;
	}
	m = ctx->mrec;
	a = ctx->attr;
	a->highest_vcn = cpu_to_sle64(rl[1].vcn - 2);
	errno = err;
undo_alloc:
	err = errno;

	/* Remove the last run from the runlist. */
	lcn = rl->lcn;
	rl->lcn = rl[1].lcn;
	rl->length = 0;
	
	/* FIXME: use an ntfs_cluster_free_* function */
	if (ntfs_bitmap_clear_bit(vol->lcnbmp_na, lcn))
		ntfs_log_error("Failed to free cluster.%s\n", es);
	else
		vol->free_clusters++;
	if (mp_rebuilt) {
		if (ntfs_mapping_pairs_build(vol, (u8*)a +
				le16_to_cpu(a->mapping_pairs_offset),
				old_alen - le16_to_cpu(a->mapping_pairs_offset),
				rl2, ll, NULL))
			ntfs_log_error("Failed to restore mapping "
					"pairs array.%s\n", es);
		if (ntfs_attr_record_resize(m, a, old_alen))
			ntfs_log_error("Failed to restore attribute "
					"record.%s\n", es);
		ntfs_inode_mark_dirty(ctx->ntfs_ino);
	}
	if (update_mp) {
		if (ntfs_attr_update_mapping_pairs(vol->mftbmp_na, 0))
			ntfs_log_perror("%s: MP update failed", __FUNCTION__);
	}
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	errno = err;
	return ret;
}

/**
 * ntfs_mft_bitmap_extend_allocation - extend mft bitmap attribute by a cluster
 * @vol:	volume on which to extend the mft bitmap attribute
 *
 * Extend the mft bitmap attribute on the ntfs volume @vol by one cluster.
 *
 * Note:  Only changes allocated_size, i.e. does not touch initialized_size or
 * data_size.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
static int ntfs_mft_bitmap_extend_allocation(ntfs_volume *vol)
{
	int ret;
	
	ntfs_log_enter("Entering\n");
	ret = ntfs_mft_bitmap_extend_allocation_i(vol);
	ntfs_log_leave("\n");
	return ret;
}
/**
 * ntfs_mft_bitmap_extend_initialized - extend mft bitmap initialized data
 * @vol:	volume on which to extend the mft bitmap attribute
 *
 * Extend the initialized portion of the mft bitmap attribute on the ntfs
 * volume @vol by 8 bytes.
 *
 * Note:  Only changes initialized_size and data_size, i.e. requires that
 * allocated_size is big enough to fit the new initialized_size.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
static int ntfs_mft_bitmap_extend_initialized(ntfs_volume *vol)
{
	s64 old_data_size, old_initialized_size, ll;
	ntfs_attr *mftbmp_na;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	int err;
	int ret = -1;

	ntfs_log_enter("Entering\n");
	
	mftbmp_na = vol->mftbmp_na;
	ctx = ntfs_attr_get_search_ctx(mftbmp_na->ni, NULL);
	if (!ctx)
		goto out;

	if (ntfs_attr_lookup(mftbmp_na->type, mftbmp_na->name,
			mftbmp_na->name_len, 0, 0, NULL, 0, ctx)) {
		ntfs_log_error("Failed to find first attribute extent of "
				"mft bitmap attribute.\n");
		err = errno;
		goto put_err_out;
	}
	a = ctx->attr;
	old_data_size = mftbmp_na->data_size;
	old_initialized_size = mftbmp_na->initialized_size;
	mftbmp_na->initialized_size += 8;
	a->initialized_size = cpu_to_sle64(mftbmp_na->initialized_size);
	if (mftbmp_na->initialized_size > mftbmp_na->data_size) {
		mftbmp_na->data_size = mftbmp_na->initialized_size;
		a->data_size = cpu_to_sle64(mftbmp_na->data_size);
	}
	/* Ensure the changes make it to disk. */
	ntfs_inode_mark_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	/* Initialize the mft bitmap attribute value with zeroes. */
	ll = 0;
	ll = ntfs_attr_pwrite(mftbmp_na, old_initialized_size, 8, &ll);
	if (ll == 8) {
		ntfs_log_debug("Wrote eight initialized bytes to mft bitmap.\n");
		vol->free_mft_records += (8 * 8); 
		ret = 0;
		goto out;
	}
	ntfs_log_error("Failed to write to mft bitmap.\n");
	err = errno;
	if (ll >= 0)
		err = EIO;
	/* Try to recover from the error. */
	ctx = ntfs_attr_get_search_ctx(mftbmp_na->ni, NULL);
	if (!ctx)
		goto err_out;

	if (ntfs_attr_lookup(mftbmp_na->type, mftbmp_na->name,
			mftbmp_na->name_len, 0, 0, NULL, 0, ctx)) {
		ntfs_log_error("Failed to find first attribute extent of "
				"mft bitmap attribute.%s\n", es);
put_err_out:
		ntfs_attr_put_search_ctx(ctx);
		goto err_out;
	}
	a = ctx->attr;
	mftbmp_na->initialized_size = old_initialized_size;
	a->initialized_size = cpu_to_sle64(old_initialized_size);
	if (mftbmp_na->data_size != old_data_size) {
		mftbmp_na->data_size = old_data_size;
		a->data_size = cpu_to_sle64(old_data_size);
	}
	ntfs_inode_mark_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	ntfs_log_debug("Restored status of mftbmp: allocated_size 0x%llx, "
			"data_size 0x%llx, initialized_size 0x%llx.\n",
			(long long)mftbmp_na->allocated_size,
			(long long)mftbmp_na->data_size,
			(long long)mftbmp_na->initialized_size);
err_out:
	errno = err;
out:
	ntfs_log_leave("\n");
	return ret;
}

/**
 * ntfs_mft_data_extend_allocation - extend mft data attribute
 * @vol:	volume on which to extend the mft data attribute
 *
 * Extend the mft data attribute on the ntfs volume @vol by 16 mft records
 * worth of clusters or if not enough space for this by one mft record worth
 * of clusters.
 *
 * Note:  Only changes allocated_size, i.e. does not touch initialized_size or
 * data_size.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
static int ntfs_mft_data_extend_allocation(ntfs_volume *vol)
{
	LCN lcn;
	VCN old_last_vcn;
	s64 min_nr, nr, ll = 0; /* silence compiler warning */
	ntfs_attr *mft_na;
	runlist_element *rl, *rl2;
	ntfs_attr_search_ctx *ctx;
	MFT_RECORD *m = NULL; /* silence compiler warning */
	ATTR_RECORD *a = NULL; /* silence compiler warning */
	int err, mp_size;
	int ret = STATUS_ERROR;
	u32 old_alen = 0; /* silence compiler warning */
	BOOL mp_rebuilt = FALSE;
	BOOL update_mp = FALSE;

	ntfs_log_enter("Extending mft data allocation.\n");
	
	mft_na = vol->mft_na;
	/*
	 * Determine the preferred allocation location, i.e. the last lcn of
	 * the mft data attribute.  The allocated size of the mft data
	 * attribute cannot be zero so we are ok to do this.
	 */
	rl = ntfs_attr_find_vcn(mft_na,
			(mft_na->allocated_size - 1) >> vol->cluster_size_bits);
	
	if (!rl || !rl->length || rl->lcn < 0) {
		ntfs_log_error("Failed to determine last allocated "
				"cluster of mft data attribute.\n");
		if (rl)
			errno = EIO;
		goto out;
	}
	
	lcn = rl->lcn + rl->length;
	ntfs_log_debug("Last lcn of mft data attribute is 0x%llx.\n", (long long)lcn);
	/* Minimum allocation is one mft record worth of clusters. */
	min_nr = vol->mft_record_size >> vol->cluster_size_bits;
	if (!min_nr)
		min_nr = 1;
	/* Want to allocate 16 mft records worth of clusters. */
	nr = vol->mft_record_size << 4 >> vol->cluster_size_bits;
	if (!nr)
		nr = min_nr;
	
	old_last_vcn = rl[1].vcn;
	do {
		rl2 = ntfs_cluster_alloc(vol, old_last_vcn, nr, lcn, MFT_ZONE);
		if (rl2)
			break;
		if (errno != ENOSPC || nr == min_nr) {
			ntfs_log_perror("Failed to allocate (%lld) clusters "
					"for $MFT", (long long)nr);
			goto out;
		}
		/*
		 * There is not enough space to do the allocation, but there
		 * might be enough space to do a minimal allocation so try that
		 * before failing.
		 */
		nr = min_nr;
		ntfs_log_debug("Retrying mft data allocation with minimal cluster "
				"count %lli.\n", (long long)nr);
	} while (1);
	
	ntfs_log_debug("Allocated %lld clusters.\n", (long long)nr);
	
	rl = ntfs_runlists_merge(mft_na->rl, rl2);
	if (!rl) {
		err = errno;
		ntfs_log_error("Failed to merge runlists for mft data "
				"attribute.\n");
		if (ntfs_cluster_free_from_rl(vol, rl2))
			ntfs_log_error("Failed to deallocate clusters "
					"from the mft data attribute.%s\n", es);
		free(rl2);
		errno = err;
		goto out;
	}
	mft_na->rl = rl;
	
	/* Find the last run in the new runlist. */
	for (; rl[1].length; rl++)
		;
	/* Update the attribute record as well. */
	ctx = ntfs_attr_get_search_ctx(mft_na->ni, NULL);
	if (!ctx)
		goto undo_alloc;

	if (ntfs_attr_lookup(mft_na->type, mft_na->name, mft_na->name_len, 0,
			rl[1].vcn, NULL, 0, ctx)) {
		ntfs_log_error("Failed to find last attribute extent of "
				"mft data attribute.\n");
		goto undo_alloc;
	}
	m = ctx->mrec;
	a = ctx->attr;
	ll = sle64_to_cpu(a->lowest_vcn);
	rl2 = ntfs_attr_find_vcn(mft_na, ll);
	if (!rl2 || !rl2->length) {
		ntfs_log_error("Failed to determine previous last "
				"allocated cluster of mft data attribute.\n");
		if (rl2)
			errno = EIO;
		goto undo_alloc;
	}
	/* Get the size for the new mapping pairs array for this extent. */
	mp_size = ntfs_get_size_for_mapping_pairs(vol, rl2, ll, INT_MAX);
	if (mp_size <= 0) {
		ntfs_log_error("Get size for mapping pairs failed for "
				"mft data attribute extent.\n");
		goto undo_alloc;
	}
	/* Expand the attribute record if necessary. */
	old_alen = le32_to_cpu(a->length);
	if (ntfs_attr_record_resize(m, a,
			mp_size + le16_to_cpu(a->mapping_pairs_offset))) {
		ret = ntfs_mft_attr_extend(vol->mft_na);
		if (ret == STATUS_OK)
			goto ok;
		if (ret == STATUS_ERROR) {
			ntfs_log_perror("%s: ntfs_mft_attr_extend failed", __FUNCTION__);
			update_mp = TRUE;
		}
		goto undo_alloc;
	}
	mp_rebuilt = TRUE;
	/*
	 * Generate the mapping pairs array directly into the attribute record.
	 */
	if (ntfs_mapping_pairs_build(vol,
			(u8*)a + le16_to_cpu(a->mapping_pairs_offset), mp_size,
			rl2, ll, NULL)) {
		ntfs_log_error("Failed to build mapping pairs array of "
				"mft data attribute.\n");
		errno = EIO;
		goto undo_alloc;
	}
	/* Update the highest_vcn. */
	a->highest_vcn = cpu_to_sle64(rl[1].vcn - 1);
	/*
	 * We now have extended the mft data allocated_size by nr clusters.
	 * Reflect this in the ntfs_attr structure and the attribute record.
	 * @rl is the last (non-terminator) runlist element of mft data
	 * attribute.
	 */
	if (a->lowest_vcn) {
		/*
		 * We are not in the first attribute extent, switch to it, but
		 * first ensure the changes will make it to disk later.
		 */
		ntfs_inode_mark_dirty(ctx->ntfs_ino);
		ntfs_attr_reinit_search_ctx(ctx);
		if (ntfs_attr_lookup(mft_na->type, mft_na->name,
				mft_na->name_len, 0, 0, NULL, 0, ctx)) {
			ntfs_log_error("Failed to find first attribute "
					"extent of mft data attribute.\n");
			goto restore_undo_alloc;
		}
		a = ctx->attr;
	}
ok:
	mft_na->allocated_size += nr << vol->cluster_size_bits;
	a->allocated_size = cpu_to_sle64(mft_na->allocated_size);
	/* Ensure the changes make it to disk. */
	ntfs_inode_mark_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	ret = STATUS_OK;
out:
	ntfs_log_leave("\n");
	return ret;

restore_undo_alloc:
	err = errno;
	ntfs_attr_reinit_search_ctx(ctx);
	if (ntfs_attr_lookup(mft_na->type, mft_na->name, mft_na->name_len, 0,
			rl[1].vcn, NULL, 0, ctx)) {
		ntfs_log_error("Failed to find last attribute extent of "
				"mft data attribute.%s\n", es);
		ntfs_attr_put_search_ctx(ctx);
		mft_na->allocated_size += nr << vol->cluster_size_bits;
		/*
		 * The only thing that is now wrong is ->allocated_size of the
		 * base attribute extent which chkdsk should be able to fix.
		 */
		errno = err;
		ret = STATUS_ERROR;
		goto out;
	}
	m = ctx->mrec;
	a = ctx->attr;
	a->highest_vcn = cpu_to_sle64(old_last_vcn - 1);
	errno = err;
undo_alloc:
	err = errno;
	if (ntfs_cluster_free(vol, mft_na, old_last_vcn, -1) < 0)
		ntfs_log_error("Failed to free clusters from mft data "
				"attribute.%s\n", es);
	if (ntfs_rl_truncate(&mft_na->rl, old_last_vcn))
		ntfs_log_error("Failed to truncate mft data attribute "
				"runlist.%s\n", es);
	if (mp_rebuilt) {
		if (ntfs_mapping_pairs_build(vol, (u8*)a +
				le16_to_cpu(a->mapping_pairs_offset),
				old_alen - le16_to_cpu(a->mapping_pairs_offset),
				rl2, ll, NULL))
			ntfs_log_error("Failed to restore mapping pairs "
					"array.%s\n", es);
		if (ntfs_attr_record_resize(m, a, old_alen))
			ntfs_log_error("Failed to restore attribute "
					"record.%s\n", es);
		ntfs_inode_mark_dirty(ctx->ntfs_ino);
	}
	if (update_mp) {
		if (ntfs_attr_update_mapping_pairs(vol->mft_na, 0))
			ntfs_log_perror("%s: MP update failed", __FUNCTION__);
	}
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	errno = err;
	goto out;
}


static int ntfs_mft_record_init(ntfs_volume *vol, s64 size)
{
	int ret = -1;
	ntfs_attr *mft_na, *mftbmp_na;
	s64 old_data_initialized, old_data_size;
	ntfs_attr_search_ctx *ctx;
	
	ntfs_log_enter("Entering\n");
	
	/* NOTE: Caller must sanity check vol, vol->mft_na and vol->mftbmp_na */
	
	mft_na = vol->mft_na;
	mftbmp_na = vol->mftbmp_na;
	
	/*
	 * The mft record is outside the initialized data. Extend the mft data
	 * attribute until it covers the allocated record. The loop is only
	 * actually traversed more than once when a freshly formatted volume
	 * is first written to so it optimizes away nicely in the common case.
	 */
	ntfs_log_debug("Status of mft data before extension: "
			"allocated_size 0x%llx, data_size 0x%llx, "
			"initialized_size 0x%llx.\n",
			(long long)mft_na->allocated_size,
			(long long)mft_na->data_size,
			(long long)mft_na->initialized_size);
	while (size > mft_na->allocated_size) {
		if (ntfs_mft_data_extend_allocation(vol) == STATUS_ERROR)
			goto out;
		ntfs_log_debug("Status of mft data after allocation extension: "
				"allocated_size 0x%llx, data_size 0x%llx, "
				"initialized_size 0x%llx.\n",
				(long long)mft_na->allocated_size,
				(long long)mft_na->data_size,
				(long long)mft_na->initialized_size);
	}
	
	old_data_initialized = mft_na->initialized_size;
	old_data_size = mft_na->data_size;
	
	/*
	 * Extend mft data initialized size (and data size of course) to reach
	 * the allocated mft record, formatting the mft records along the way.
	 * Note: We only modify the ntfs_attr structure as that is all that is
	 * needed by ntfs_mft_record_format().  We will update the attribute
	 * record itself in one fell swoop later on.
	 */
	while (size > mft_na->initialized_size) {
		s64 ll2 = mft_na->initialized_size >> vol->mft_record_size_bits;
		mft_na->initialized_size += vol->mft_record_size;
		if (mft_na->initialized_size > mft_na->data_size)
			mft_na->data_size = mft_na->initialized_size;
		ntfs_log_debug("Initializing mft record 0x%llx.\n", (long long)ll2);
		if (ntfs_mft_record_format(vol, ll2) < 0) {
			ntfs_log_perror("Failed to format mft record");
			goto undo_data_init;
		}
	}
	
	/* Update the mft data attribute record to reflect the new sizes. */
	ctx = ntfs_attr_get_search_ctx(mft_na->ni, NULL);
	if (!ctx)
		goto undo_data_init;

	if (ntfs_attr_lookup(mft_na->type, mft_na->name, mft_na->name_len, 0,
			0, NULL, 0, ctx)) {
		ntfs_log_error("Failed to find first attribute extent of "
				"mft data attribute.\n");
		ntfs_attr_put_search_ctx(ctx);
		goto undo_data_init;
	}
	ctx->attr->initialized_size = cpu_to_sle64(mft_na->initialized_size);
	ctx->attr->data_size = cpu_to_sle64(mft_na->data_size);
	ctx->attr->allocated_size = cpu_to_sle64(mft_na->allocated_size);
	
	/* Ensure the changes make it to disk. */
	ntfs_inode_mark_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	ntfs_log_debug("Status of mft data after mft record initialization: "
			"allocated_size 0x%llx, data_size 0x%llx, "
			"initialized_size 0x%llx.\n",
			(long long)mft_na->allocated_size,
			(long long)mft_na->data_size,
			(long long)mft_na->initialized_size);
	
	/* Sanity checks. */
	if (mft_na->data_size > mft_na->allocated_size ||
	    mft_na->initialized_size > mft_na->data_size)
		NTFS_BUG("mft_na sanity checks failed");
	
	/* Sync MFT to minimize data loss if there won't be clean unmount. */
	if (ntfs_inode_sync(mft_na->ni))
		goto undo_data_init;
	
	ret = 0;
out:	
	ntfs_log_leave("\n");
	return ret;
	
undo_data_init:
	mft_na->initialized_size = old_data_initialized;
	mft_na->data_size = old_data_size;
	goto out;
}

static int ntfs_mft_rec_init(ntfs_volume *vol, s64 size)
{
	int ret = -1;
	ntfs_attr *mft_na, *mftbmp_na;
	s64 old_data_initialized, old_data_size;
	ntfs_attr_search_ctx *ctx;
	
	ntfs_log_enter("Entering\n");
	
	mft_na = vol->mft_na;
	mftbmp_na = vol->mftbmp_na;
	
	if (size > mft_na->allocated_size || size > mft_na->initialized_size) {
		errno = EIO;
		ntfs_log_perror("%s: unexpected $MFT sizes, see below", __FUNCTION__);
		ntfs_log_error("$MFT: size=%lld  allocated_size=%lld  "
			       "data_size=%lld  initialized_size=%lld\n",
			       (long long)size,
			       (long long)mft_na->allocated_size,
			       (long long)mft_na->data_size,
			       (long long)mft_na->initialized_size);
		goto out;
	}
	
	old_data_initialized = mft_na->initialized_size;
	old_data_size = mft_na->data_size;
	
	/* Update the mft data attribute record to reflect the new sizes. */
	ctx = ntfs_attr_get_search_ctx(mft_na->ni, NULL);
	if (!ctx)
		goto undo_data_init;

	if (ntfs_attr_lookup(mft_na->type, mft_na->name, mft_na->name_len, 0,
			0, NULL, 0, ctx)) {
		ntfs_log_error("Failed to find first attribute extent of "
				"mft data attribute.\n");
		ntfs_attr_put_search_ctx(ctx);
		goto undo_data_init;
	}
	ctx->attr->initialized_size = cpu_to_sle64(mft_na->initialized_size);
	ctx->attr->data_size = cpu_to_sle64(mft_na->data_size);

	/* CHECKME: ctx->attr->allocation_size is already ok? */
	
	/* Ensure the changes make it to disk. */
	ntfs_inode_mark_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	
	/* Sanity checks. */
	if (mft_na->data_size > mft_na->allocated_size ||
	    mft_na->initialized_size > mft_na->data_size)
		NTFS_BUG("mft_na sanity checks failed");
out:	
	ntfs_log_leave("\n");
	return ret;
	
undo_data_init:
	mft_na->initialized_size = old_data_initialized;
	mft_na->data_size = old_data_size;
	goto out;
}

static ntfs_inode *ntfs_mft_rec_alloc(ntfs_volume *vol)
{
	s64 ll, bit;
	ntfs_attr *mft_na, *mftbmp_na;
	MFT_RECORD *m;
	ntfs_inode *ni = NULL;
	ntfs_inode *base_ni;
	int err;
	le16 seq_no, usn;

	ntfs_log_enter("Entering\n");

	mft_na = vol->mft_na;
	mftbmp_na = vol->mftbmp_na;

	base_ni = mft_na->ni;

	bit = ntfs_mft_bitmap_find_free_rec(vol, base_ni);
	if (bit >= 0)
		goto found_free_rec;

	if (errno != ENOSPC)
		goto out;
	
	errno = ENOSPC;
	/* strerror() is intentionally used below, we want to log this error. */
	ntfs_log_error("No free mft record for $MFT: %s\n", strerror(errno));
	goto err_out;

found_free_rec:
	if (ntfs_bitmap_set_bit(mftbmp_na, bit)) {
		ntfs_log_error("Failed to allocate bit in mft bitmap #2\n");
		goto err_out;
	}
	
	ll = (bit + 1) << vol->mft_record_size_bits;
	if (ll > mft_na->initialized_size)
		if (ntfs_mft_rec_init(vol, ll) < 0)
			goto undo_mftbmp_alloc;
	/*
	 * We now have allocated and initialized the mft record.  Need to read
	 * it from disk and re-format it, preserving the sequence number if it
	 * is not zero as well as the update sequence number if it is not zero
	 * or -1 (0xffff).
	 */
	m = ntfs_malloc(vol->mft_record_size);
	if (!m)
		goto undo_mftbmp_alloc;
	
	if (ntfs_mft_record_read(vol, bit, m)) {
		free(m);
		goto undo_mftbmp_alloc;
	}
	/* Sanity check that the mft record is really not in use. */
	if (ntfs_is_file_record(m->magic) && (m->flags & MFT_RECORD_IN_USE)) {
		ntfs_log_error("Inode %lld is used but it wasn't marked in "
			       "$MFT bitmap. Fixed.\n", (long long)bit);
		free(m);
		goto undo_mftbmp_alloc;
	}

	seq_no = m->sequence_number;
	usn = *(le16*)((u8*)m + le16_to_cpu(m->usa_ofs));
	if (ntfs_mft_record_layout(vol, bit, m)) {
		ntfs_log_error("Failed to re-format mft record.\n");
		free(m);
		goto undo_mftbmp_alloc;
	}
	if (seq_no)
		m->sequence_number = seq_no;
	seq_no = usn;
	if (seq_no && seq_no != const_cpu_to_le16(0xffff))
		*(le16*)((u8*)m + le16_to_cpu(m->usa_ofs)) = usn;
	/* Set the mft record itself in use. */
	m->flags |= MFT_RECORD_IN_USE;
	/* Now need to open an ntfs inode for the mft record. */
	ni = ntfs_inode_allocate(vol);
	if (!ni) {
		ntfs_log_error("Failed to allocate buffer for inode.\n");
		free(m);
		goto undo_mftbmp_alloc;
	}
	ni->mft_no = bit;
	ni->mrec = m;
	/*
	 * If we are allocating an extent mft record, make the opened inode an
	 * extent inode and attach it to the base inode.  Also, set the base
	 * mft record reference in the extent inode.
	 */
	ni->nr_extents = -1;
	ni->base_ni = base_ni;
	m->base_mft_record = MK_LE_MREF(base_ni->mft_no,
					le16_to_cpu(base_ni->mrec->sequence_number));
	/*
	 * Attach the extent inode to the base inode, reallocating
	 * memory if needed.
	 */
	if (!(base_ni->nr_extents & 3)) {
		ntfs_inode **extent_nis;
		int i;

		i = (base_ni->nr_extents + 4) * sizeof(ntfs_inode *);
		extent_nis = ntfs_malloc(i);
		if (!extent_nis) {
			free(m);
			free(ni);
			goto undo_mftbmp_alloc;
		}
		if (base_ni->nr_extents) {
			memcpy(extent_nis, base_ni->extent_nis,
					i - 4 * sizeof(ntfs_inode *));
			free(base_ni->extent_nis);
		}
		base_ni->extent_nis = extent_nis;
	}
	base_ni->extent_nis[base_ni->nr_extents++] = ni;
	
	/* Make sure the allocated inode is written out to disk later. */
	ntfs_inode_mark_dirty(ni);
	/* Initialize time, allocated and data size in ntfs_inode struct. */
	ni->data_size = ni->allocated_size = 0;
	ni->flags = 0;
	ni->creation_time = ni->last_data_change_time =
			ni->last_mft_change_time =
			ni->last_access_time = ntfs_current_time();
	/* Update the default mft allocation position if it was used. */
	if (!base_ni)
		vol->mft_data_pos = bit + 1;
	/* Return the opened, allocated inode of the allocated mft record. */
	ntfs_log_error("allocated %sinode %lld\n",
			base_ni ? "extent " : "", (long long)bit);
out:
	ntfs_log_leave("\n");	
	return ni;

undo_mftbmp_alloc:
	err = errno;
	if (ntfs_bitmap_clear_bit(mftbmp_na, bit))
		ntfs_log_error("Failed to clear bit in mft bitmap.%s\n", es);
	errno = err;
err_out:
	if (!errno)
		errno = EIO;
	ni = NULL;
	goto out;	
}

/**
 * ntfs_mft_record_alloc - allocate an mft record on an ntfs volume
 * @vol:	volume on which to allocate the mft record
 * @base_ni:	open base inode if allocating an extent mft record or NULL
 *
 * Allocate an mft record in $MFT/$DATA of an open ntfs volume @vol.
 *
 * If @base_ni is NULL make the mft record a base mft record and allocate it at
 * the default allocator position.
 *
 * If @base_ni is not NULL make the allocated mft record an extent record,
 * allocate it starting at the mft record after the base mft record and attach
 * the allocated and opened ntfs inode to the base inode @base_ni.
 *
 * On success return the now opened ntfs (extent) inode of the mft record.
 *
 * On error return NULL with errno set to the error code.
 *
 * To find a free mft record, we scan the mft bitmap for a zero bit.  To
 * optimize this we start scanning at the place specified by @base_ni or if
 * @base_ni is NULL we start where we last stopped and we perform wrap around
 * when we reach the end.  Note, we do not try to allocate mft records below
 * number 24 because numbers 0 to 15 are the defined system files anyway and 16
 * to 24 are special in that they are used for storing extension mft records
 * for the $DATA attribute of $MFT.  This is required to avoid the possibility
 * of creating a run list with a circular dependence which once written to disk
 * can never be read in again.  Windows will only use records 16 to 24 for
 * normal files if the volume is completely out of space.  We never use them
 * which means that when the volume is really out of space we cannot create any
 * more files while Windows can still create up to 8 small files.  We can start
 * doing this at some later time, it does not matter much for now.
 *
 * When scanning the mft bitmap, we only search up to the last allocated mft
 * record.  If there are no free records left in the range 24 to number of
 * allocated mft records, then we extend the $MFT/$DATA attribute in order to
 * create free mft records.  We extend the allocated size of $MFT/$DATA by 16
 * records at a time or one cluster, if cluster size is above 16kiB.  If there
 * is not sufficient space to do this, we try to extend by a single mft record
 * or one cluster, if cluster size is above the mft record size, but we only do
 * this if there is enough free space, which we know from the values returned
 * by the failed cluster allocation function when we tried to do the first
 * allocation.
 *
 * No matter how many mft records we allocate, we initialize only the first
 * allocated mft record, incrementing mft data size and initialized size
 * accordingly, open an ntfs_inode for it and return it to the caller, unless
 * there are less than 24 mft records, in which case we allocate and initialize
 * mft records until we reach record 24 which we consider as the first free mft
 * record for use by normal files.
 *
 * If during any stage we overflow the initialized data in the mft bitmap, we
 * extend the initialized size (and data size) by 8 bytes, allocating another
 * cluster if required.  The bitmap data size has to be at least equal to the
 * number of mft records in the mft, but it can be bigger, in which case the
 * superfluous bits are padded with zeroes.
 *
 * Thus, when we return successfully (return value non-zero), we will have:
 *	- initialized / extended the mft bitmap if necessary,
 *	- initialized / extended the mft data if necessary,
 *	- set the bit corresponding to the mft record being allocated in the
 *	  mft bitmap,
 *	- open an ntfs_inode for the allocated mft record, and we will
 *	- return the ntfs_inode.
 *
 * On error (return value zero), nothing will have changed.  If we had changed
 * anything before the error occurred, we will have reverted back to the
 * starting state before returning to the caller.  Thus, except for bugs, we
 * should always leave the volume in a consistent state when returning from
 * this function.
 *
 * Note, this function cannot make use of most of the normal functions, like
 * for example for attribute resizing, etc, because when the run list overflows
 * the base mft record and an attribute list is used, it is very important that
 * the extension mft records used to store the $DATA attribute of $MFT can be
 * reached without having to read the information contained inside them, as
 * this would make it impossible to find them in the first place after the
 * volume is dismounted.  $MFT/$BITMAP probably does not need to follow this
 * rule because the bitmap is not essential for finding the mft records, but on
 * the other hand, handling the bitmap in this special way would make life
 * easier because otherwise there might be circular invocations of functions
 * when reading the bitmap but if we are careful, we should be able to avoid
 * all problems.
 */
ntfs_inode *ntfs_mft_record_alloc(ntfs_volume *vol, ntfs_inode *base_ni)
{
	s64 ll, bit;
	ntfs_attr *mft_na, *mftbmp_na;
	MFT_RECORD *m;
	ntfs_inode *ni = NULL;
	int err;
	le16 seq_no, usn;

	if (base_ni)
		ntfs_log_enter("Entering (allocating an extent mft record for "
			       "base mft record %lld).\n", 
			       (long long)base_ni->mft_no);
	else
		ntfs_log_enter("Entering (allocating a base mft record)\n");
	if (!vol || !vol->mft_na || !vol->mftbmp_na) {
		errno = EINVAL;
		goto out;
	}
	
	if (ntfs_is_mft(base_ni)) {
		ni = ntfs_mft_rec_alloc(vol);
		goto out;
	}

	mft_na = vol->mft_na;
	mftbmp_na = vol->mftbmp_na;
retry:	
	bit = ntfs_mft_bitmap_find_free_rec(vol, base_ni);
	if (bit >= 0) {
		ntfs_log_debug("found free record (#1) at %lld\n",
				(long long)bit);
		goto found_free_rec;
	}
	if (errno != ENOSPC)
		goto out;
	/*
	 * No free mft records left.  If the mft bitmap already covers more
	 * than the currently used mft records, the next records are all free,
	 * so we can simply allocate the first unused mft record.
	 * Note: We also have to make sure that the mft bitmap at least covers
	 * the first 24 mft records as they are special and whilst they may not
	 * be in use, we do not allocate from them.
	 */
	ll = mft_na->initialized_size >> vol->mft_record_size_bits;
	if (mftbmp_na->initialized_size << 3 > ll &&
			mftbmp_na->initialized_size > RESERVED_MFT_RECORDS / 8) {
		bit = ll;
		if (bit < RESERVED_MFT_RECORDS)
			bit = RESERVED_MFT_RECORDS;
		ntfs_log_debug("found free record (#2) at %lld\n",
				(long long)bit);
		goto found_free_rec;
	}
	/*
	 * The mft bitmap needs to be expanded until it covers the first unused
	 * mft record that we can allocate.
	 * Note: The smallest mft record we allocate is mft record 24.
	 */
	ntfs_log_debug("Status of mftbmp before extension: allocated_size 0x%llx, "
			"data_size 0x%llx, initialized_size 0x%llx.\n",
			(long long)mftbmp_na->allocated_size,
			(long long)mftbmp_na->data_size,
			(long long)mftbmp_na->initialized_size);
	if (mftbmp_na->initialized_size + 8 > mftbmp_na->allocated_size) {

		int ret = ntfs_mft_bitmap_extend_allocation(vol);

		if (ret == STATUS_ERROR)
			goto err_out;
		if (ret == STATUS_KEEP_SEARCHING) {
			ret = ntfs_mft_bitmap_extend_allocation(vol);
			if (ret != STATUS_OK)
				goto err_out;
		}

		ntfs_log_debug("Status of mftbmp after allocation extension: "
				"allocated_size 0x%llx, data_size 0x%llx, "
				"initialized_size 0x%llx.\n",
				(long long)mftbmp_na->allocated_size,
				(long long)mftbmp_na->data_size,
				(long long)mftbmp_na->initialized_size);
	}
	/*
	 * We now have sufficient allocated space, extend the initialized_size
	 * as well as the data_size if necessary and fill the new space with
	 * zeroes.
	 */
	bit = mftbmp_na->initialized_size << 3;
	if (ntfs_mft_bitmap_extend_initialized(vol))
		goto err_out;
	ntfs_log_debug("Status of mftbmp after initialized extension: "
			"allocated_size 0x%llx, data_size 0x%llx, "
			"initialized_size 0x%llx.\n",
			(long long)mftbmp_na->allocated_size,
			(long long)mftbmp_na->data_size,
			(long long)mftbmp_na->initialized_size);
	ntfs_log_debug("found free record (#3) at %lld\n", (long long)bit);
found_free_rec:
	/* @bit is the found free mft record, allocate it in the mft bitmap. */
	if (ntfs_bitmap_set_bit(mftbmp_na, bit)) {
		ntfs_log_error("Failed to allocate bit in mft bitmap.\n");
		goto err_out;
	}
	
	/* The mft bitmap is now uptodate.  Deal with mft data attribute now. */
	ll = (bit + 1) << vol->mft_record_size_bits;
	if (ll > mft_na->initialized_size)
		if (ntfs_mft_record_init(vol, ll) < 0)
			goto undo_mftbmp_alloc;

	/*
	 * We now have allocated and initialized the mft record.  Need to read
	 * it from disk and re-format it, preserving the sequence number if it
	 * is not zero as well as the update sequence number if it is not zero
	 * or -1 (0xffff).
	 */
	m = ntfs_malloc(vol->mft_record_size);
	if (!m)
		goto undo_mftbmp_alloc;
	
	if (ntfs_mft_record_read(vol, bit, m)) {
		free(m);
		goto undo_mftbmp_alloc;
	}
	/* Sanity check that the mft record is really not in use. */
	if (ntfs_is_file_record(m->magic) && (m->flags & MFT_RECORD_IN_USE)) {
		ntfs_log_error("Inode %lld is used but it wasn't marked in "
			       "$MFT bitmap. Fixed.\n", (long long)bit);
		free(m);
		goto retry;
	}
	seq_no = m->sequence_number;
	usn = *(le16*)((u8*)m + le16_to_cpu(m->usa_ofs));
	if (ntfs_mft_record_layout(vol, bit, m)) {
		ntfs_log_error("Failed to re-format mft record.\n");
		free(m);
		goto undo_mftbmp_alloc;
	}
	if (seq_no)
		m->sequence_number = seq_no;
	seq_no = usn;
	if (seq_no && seq_no != const_cpu_to_le16(0xffff))
		*(le16*)((u8*)m + le16_to_cpu(m->usa_ofs)) = usn;
	/* Set the mft record itself in use. */
	m->flags |= MFT_RECORD_IN_USE;
	/* Now need to open an ntfs inode for the mft record. */
	ni = ntfs_inode_allocate(vol);
	if (!ni) {
		ntfs_log_error("Failed to allocate buffer for inode.\n");
		free(m);
		goto undo_mftbmp_alloc;
	}
	ni->mft_no = bit;
	ni->mrec = m;
	/*
	 * If we are allocating an extent mft record, make the opened inode an
	 * extent inode and attach it to the base inode.  Also, set the base
	 * mft record reference in the extent inode.
	 */
	if (base_ni) {
		ni->nr_extents = -1;
		ni->base_ni = base_ni;
		m->base_mft_record = MK_LE_MREF(base_ni->mft_no,
				le16_to_cpu(base_ni->mrec->sequence_number));
		/*
		 * Attach the extent inode to the base inode, reallocating
		 * memory if needed.
		 */
		if (!(base_ni->nr_extents & 3)) {
			ntfs_inode **extent_nis;
			int i;

			i = (base_ni->nr_extents + 4) * sizeof(ntfs_inode *);
			extent_nis = ntfs_malloc(i);
			if (!extent_nis) {
				free(m);
				free(ni);
				goto undo_mftbmp_alloc;
			}
			if (base_ni->nr_extents) {
				memcpy(extent_nis, base_ni->extent_nis,
						i - 4 * sizeof(ntfs_inode *));
				free(base_ni->extent_nis);
			}
			base_ni->extent_nis = extent_nis;
		}
		base_ni->extent_nis[base_ni->nr_extents++] = ni;
	}
	/* Make sure the allocated inode is written out to disk later. */
	ntfs_inode_mark_dirty(ni);
	/* Initialize time, allocated and data size in ntfs_inode struct. */
	ni->data_size = ni->allocated_size = 0;
	ni->flags = 0;
	ni->creation_time = ni->last_data_change_time =
			ni->last_mft_change_time =
			ni->last_access_time = ntfs_current_time();
	/* Update the default mft allocation position if it was used. */
	if (!base_ni)
		vol->mft_data_pos = bit + 1;
	/* Return the opened, allocated inode of the allocated mft record. */
	ntfs_log_debug("allocated %sinode 0x%llx.\n",
			base_ni ? "extent " : "", (long long)bit);
	vol->free_mft_records--; 
out:
	ntfs_log_leave("\n");	
	return ni;

undo_mftbmp_alloc:
	err = errno;
	if (ntfs_bitmap_clear_bit(mftbmp_na, bit))
		ntfs_log_error("Failed to clear bit in mft bitmap.%s\n", es);
	errno = err;
err_out:
	if (!errno)
		errno = EIO;
	ni = NULL;
	goto out;	
}

/**
 * ntfs_mft_record_free - free an mft record on an ntfs volume
 * @vol:	volume on which to free the mft record
 * @ni:		open ntfs inode of the mft record to free
 *
 * Free the mft record of the open inode @ni on the mounted ntfs volume @vol.
 * Note that this function calls ntfs_inode_close() internally and hence you
 * cannot use the pointer @ni any more after this function returns success.
 *
 * On success return 0 and on error return -1 with errno set to the error code.
 */
int ntfs_mft_record_free(ntfs_volume *vol, ntfs_inode *ni)
{
	u64 mft_no;
	int err;
	u16 seq_no;
	le16 old_seq_no;

	ntfs_log_trace("Entering for inode 0x%llx.\n", (long long) ni->mft_no);

	if (!vol || !vol->mftbmp_na || !ni) {
		errno = EINVAL;
		return -1;
	}

	/* Cache the mft reference for later. */
	mft_no = ni->mft_no;

	/* Mark the mft record as not in use. */
	ni->mrec->flags &= ~MFT_RECORD_IN_USE;

	/* Increment the sequence number, skipping zero, if it is not zero. */
	old_seq_no = ni->mrec->sequence_number;
	seq_no = le16_to_cpu(old_seq_no);
	if (seq_no == 0xffff)
		seq_no = 1;
	else if (seq_no)
		seq_no++;
	ni->mrec->sequence_number = cpu_to_le16(seq_no);

	/* Set the inode dirty and write it out. */
	ntfs_inode_mark_dirty(ni);
	if (ntfs_inode_sync(ni)) {
		err = errno;
		goto sync_rollback;
	}

	/* Clear the bit in the $MFT/$BITMAP corresponding to this record. */
	if (ntfs_bitmap_clear_bit(vol->mftbmp_na, mft_no)) {
		err = errno;
		// FIXME: If ntfs_bitmap_clear_run() guarantees rollback on
		//	  error, this could be changed to goto sync_rollback;
		goto bitmap_rollback;
	}

	/* Throw away the now freed inode. */
#if CACHE_NIDATA_SIZE
	if (!ntfs_inode_real_close(ni)) {
#else
	if (!ntfs_inode_close(ni)) {
#endif
		vol->free_mft_records++; 
		return 0;
	}
	err = errno;

	/* Rollback what we did... */
bitmap_rollback:
	if (ntfs_bitmap_set_bit(vol->mftbmp_na, mft_no))
		ntfs_log_debug("Eeek! Rollback failed in ntfs_mft_record_free().  "
				"Leaving inconsistent metadata!\n");
sync_rollback:
	ni->mrec->flags |= MFT_RECORD_IN_USE;
	ni->mrec->sequence_number = old_seq_no;
	ntfs_inode_mark_dirty(ni);
	errno = err;
	return -1;
}

/**
 * ntfs_mft_usn_dec - Decrement USN by one
 * @mrec:	pointer to an mft record
 *
 * On success return 0 and on error return -1 with errno set.
 */
int ntfs_mft_usn_dec(MFT_RECORD *mrec)
{
	u16 usn;
	le16 *usnp;

	if (!mrec) {
		errno = EINVAL;
		return -1;
	}
	usnp = (le16*)((char*)mrec + le16_to_cpu(mrec->usa_ofs));
	usn = le16_to_cpup(usnp);
	if (usn-- <= 1)
		usn = 0xfffe;
	*usnp = cpu_to_le16(usn);

	return 0;
}

