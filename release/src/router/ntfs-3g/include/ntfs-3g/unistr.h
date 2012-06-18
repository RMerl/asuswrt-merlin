/*
 * unistr.h - Exports for Unicode string handling. Originated from the Linux-NTFS
 *	      project.
 *
 * Copyright (c) 2000-2004 Anton Altaparmakov
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

#ifndef _NTFS_UNISTR_H
#define _NTFS_UNISTR_H

#include "types.h"
#include "layout.h"

extern BOOL ntfs_names_are_equal(const ntfschar *s1, size_t s1_len,
		const ntfschar *s2, size_t s2_len, const IGNORE_CASE_BOOL ic,
		const ntfschar *upcase, const u32 upcase_size);

extern int ntfs_names_full_collate(const ntfschar *name1, const u32 name1_len,
		const ntfschar *name2, const u32 name2_len,
		const IGNORE_CASE_BOOL ic,
		const ntfschar *upcase, const u32 upcase_len);

extern int ntfs_ucsncmp(const ntfschar *s1, const ntfschar *s2, size_t n);

extern int ntfs_ucsncasecmp(const ntfschar *s1, const ntfschar *s2, size_t n,
		const ntfschar *upcase, const u32 upcase_size);

extern u32 ntfs_ucsnlen(const ntfschar *s, u32 maxlen);

extern ntfschar *ntfs_ucsndup(const ntfschar *s, u32 maxlen);

extern void ntfs_name_upcase(ntfschar *name, u32 name_len,
		const ntfschar *upcase, const u32 upcase_len);

extern void ntfs_name_locase(ntfschar *name, u32 name_len,
		const ntfschar *locase, const u32 locase_len);

extern void ntfs_file_value_upcase(FILE_NAME_ATTR *file_name_attr,
		const ntfschar *upcase, const u32 upcase_len);

extern int ntfs_ucstombs(const ntfschar *ins, const int ins_len, char **outs,
		int outs_len);
extern int ntfs_mbstoucs(const char *ins, ntfschar **outs);

extern char *ntfs_uppercase_mbs(const char *low,
		const ntfschar *upcase, u32 upcase_len);

extern void ntfs_upcase_table_build(ntfschar *uc, u32 uc_len);
extern ntfschar *ntfs_locase_table_build(const ntfschar *uc, u32 uc_cnt);

extern ntfschar *ntfs_str2ucs(const char *s, int *len);

extern void ntfs_ucsfree(ntfschar *ucs);

extern BOOL ntfs_forbidden_chars(const ntfschar *name, int len);
extern BOOL ntfs_collapsible_chars(ntfs_volume *vol,
				const ntfschar *shortname, int shortlen,
				const ntfschar *longname, int longlen);

extern int ntfs_set_char_encoding(const char *locale);

#if defined(__APPLE__) || defined(__DARWIN__)
/**
 * Mac OS X only.
 *
 * Sets file name Unicode normalization form conversion on or off.
 *   normalize=0 : Off
 *   normalize=1 : On
 * If set to on, all filenames returned by ntfs-3g will be converted to the NFD
 * normalization form, while all filenames recieved by ntfs-3g will be converted to the NFC
 * normalization form. Since Windows and most other OS:es use the NFC form while Mac OS X
 * mostly uses NFD, this conversion increases compatibility between Mac applications and
 * NTFS-3G.
 * 
 * @param normalize decides whether or not the string functions will do automatic filename
 *        normalization when converting to and from UTF-8. 0 means normalization is disabled,
 *        1 means it is enabled.
 * @return -1 if the argument was invalid or an error occurred, 0 if all went well.
 */
extern int ntfs_macosx_normalize_filenames(int normalize);

/**
 * Mac OS X only.
 * 
 * Normalizes the input string "utf8_string" to one of the normalization forms NFD or NFC.
 * The parameter "composed" decides whether output should be in composed, NFC, form
 * (composed == 1) or decomposed, NFD, form (composed == 0).
 * Input is assumed to be properly UTF-8 encoded and null-terminated. Output will be a newly
 * ntfs_calloc'ed string encoded in UTF-8. It is the callers responsibility to free(...) the
 * allocated string when it's no longer needed.
 *
 * @param utf8_string the input string, which may be in any normalization form.
 * @param target a pointer where the resulting string will be stored.
 * @param composed decides which composition form to normalize the input string to. 0 means
 *        composed form (NFC), 1 means decomposed form (NFD).
 * @return -1 if the normalization failed for some reason, otherwise the length of the
 *         normalized string stored in target.
 */
extern int ntfs_macosx_normalize_utf8(const char *utf8_string, char **target, int composed);
#endif /* defined(__APPLE__) || defined(__DARWIN__) */

#endif /* defined _NTFS_UNISTR_H */

