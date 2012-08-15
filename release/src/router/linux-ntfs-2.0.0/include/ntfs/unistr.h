/*
 * unistr.h - Exports for Unicode string handling. Part of the Linux-NTFS
 *	      project.
 *
 * Copyright (c) 2000-2006 Anton Altaparmakov
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

#ifndef _NTFS_UNISTR_H
#define _NTFS_UNISTR_H

#include "types.h"
#include "layout.h"

extern BOOL ntfs_names_are_equal(const ntfschar *s1, size_t s1_len,
		const ntfschar *s2, size_t s2_len, const IGNORE_CASE_BOOL ic,
		const ntfschar *upcase, const u32 upcase_size);

extern int ntfs_names_collate(const ntfschar *name1, const u32 name1_len,
		const ntfschar *name2, const u32 name2_len,
		const int err_val, const IGNORE_CASE_BOOL ic,
		const ntfschar *upcase, const u32 upcase_len);

extern int ntfs_ucsncmp(const ntfschar *s1, const ntfschar *s2, size_t n);

extern int ntfs_ucsncasecmp(const ntfschar *s1, const ntfschar *s2, size_t n,
		const ntfschar *upcase, const u32 upcase_size);

extern u32 ntfs_ucsnlen(const ntfschar *s, u32 maxlen);

extern ntfschar *ntfs_ucsndup(const ntfschar *s, u32 maxlen);

extern void ntfs_name_upcase(ntfschar *name, u32 name_len,
		const ntfschar *upcase, const u32 upcase_len);

extern void ntfs_file_value_upcase(FILE_NAME_ATTR *file_name_attr,
		const ntfschar *upcase, const u32 upcase_len);

extern int ntfs_file_values_compare(const FILE_NAME_ATTR *file_name_attr1,
		const FILE_NAME_ATTR *file_name_attr2,
		const int err_val, const IGNORE_CASE_BOOL ic,
		const ntfschar *upcase, const u32 upcase_len);

extern int ntfs_ucstombs(const ntfschar *ins, const int ins_len, char **outs,
		int outs_len);
extern int ntfs_mbstoucs(const char *ins, ntfschar **outs, int outs_len);

extern void ntfs_upcase_table_build(ntfschar *uc, u32 uc_len);

extern ntfschar *ntfs_str2ucs(const char *s, int *len);

extern void ntfs_ucsfree(ntfschar *ucs);

#endif /* defined _NTFS_UNISTR_H */

