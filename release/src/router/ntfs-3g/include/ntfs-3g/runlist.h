/*
 * runlist.h - Exports for runlist handling. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2002 Anton Altaparmakov
 * Copyright (c) 2002 Richard Russon
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

#ifndef _NTFS_RUNLIST_H
#define _NTFS_RUNLIST_H

#include "types.h"

/* Forward declarations */
typedef struct _runlist_element runlist_element;
typedef runlist_element runlist;

#include "attrib.h"
#include "volume.h"

/**
 * struct _runlist_element - in memory vcn to lcn mapping array element.
 * @vcn:	starting vcn of the current array element
 * @lcn:	starting lcn of the current array element
 * @length:	length in clusters of the current array element
 *
 * The last vcn (in fact the last vcn + 1) is reached when length == 0.
 *
 * When lcn == -1 this means that the count vcns starting at vcn are not
 * physically allocated (i.e. this is a hole / data is sparse).
 */
struct _runlist_element {/* In memory vcn to lcn mapping structure element. */
	VCN vcn;	/* vcn = Starting virtual cluster number. */
	LCN lcn;	/* lcn = Starting logical cluster number. */
	s64 length;	/* Run length in clusters. */
};

extern runlist_element *ntfs_rl_extend(ntfs_attr *na, runlist_element *rl,
			int more_entries);

extern LCN ntfs_rl_vcn_to_lcn(const runlist_element *rl, const VCN vcn);

extern s64 ntfs_rl_pread(const ntfs_volume *vol, const runlist_element *rl,
		const s64 pos, s64 count, void *b);
extern s64 ntfs_rl_pwrite(const ntfs_volume *vol, const runlist_element *rl,
		s64 ofs, const s64 pos, s64 count, void *b);

extern runlist_element *ntfs_runlists_merge(runlist_element *drl,
		runlist_element *srl);

extern runlist_element *ntfs_mapping_pairs_decompress(const ntfs_volume *vol,
		const ATTR_RECORD *attr, runlist_element *old_rl);

extern int ntfs_get_nr_significant_bytes(const s64 n);

extern int ntfs_get_size_for_mapping_pairs(const ntfs_volume *vol,
		const runlist_element *rl, const VCN start_vcn, int max_size);

extern int ntfs_write_significant_bytes(u8 *dst, const u8 *dst_max,
		const s64 n);

extern int ntfs_mapping_pairs_build(const ntfs_volume *vol, u8 *dst,
		const int dst_len, const runlist_element *rl,
		const VCN start_vcn, runlist_element const **stop_rl);

extern int ntfs_rl_truncate(runlist **arl, const VCN start_vcn);

extern int ntfs_rl_sparse(runlist *rl);
extern s64 ntfs_rl_get_compressed_size(ntfs_volume *vol, runlist *rl);

#ifdef NTFS_TEST
int test_rl_main(int argc, char *argv[]);
#endif

#endif /* defined _NTFS_RUNLIST_H */

