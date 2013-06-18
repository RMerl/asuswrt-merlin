/*
 * feature.c --- convert between features and strings
 *
 * Copyright (C) 1999  Theodore Ts'o <tytso@mit.edu>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "e2p.h"
#include <ext2fs/ext2fs.h>
#include <ext2fs/jfs_user.h>

struct feature {
	int		compat;
	unsigned int	mask;
	const char	*string;
};

static struct feature feature_list[] = {
	{	E2P_FEATURE_COMPAT, EXT2_FEATURE_COMPAT_DIR_PREALLOC,
			"dir_prealloc" },
	{	E2P_FEATURE_COMPAT, EXT3_FEATURE_COMPAT_HAS_JOURNAL,
			"has_journal" },
	{	E2P_FEATURE_COMPAT, EXT2_FEATURE_COMPAT_IMAGIC_INODES,
			"imagic_inodes" },
	{	E2P_FEATURE_COMPAT, EXT2_FEATURE_COMPAT_EXT_ATTR,
			"ext_attr" },
	{	E2P_FEATURE_COMPAT, EXT2_FEATURE_COMPAT_DIR_INDEX,
			"dir_index" },
	{	E2P_FEATURE_COMPAT, EXT2_FEATURE_COMPAT_RESIZE_INODE,
			"resize_inode" },
	{	E2P_FEATURE_COMPAT, EXT2_FEATURE_COMPAT_LAZY_BG,
			"lazy_bg" },
	{	E2P_FEATURE_COMPAT, EXT2_FEATURE_COMPAT_EXCLUDE_BITMAP,
			"snapshot_bitmap" },

	{	E2P_FEATURE_RO_INCOMPAT, EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER,
			"sparse_super" },
	{	E2P_FEATURE_RO_INCOMPAT, EXT2_FEATURE_RO_COMPAT_LARGE_FILE,
			"large_file" },
	{	E2P_FEATURE_RO_INCOMPAT, EXT4_FEATURE_RO_COMPAT_HUGE_FILE,
			"huge_file" },
	{	E2P_FEATURE_RO_INCOMPAT, EXT4_FEATURE_RO_COMPAT_GDT_CSUM,
			"uninit_bg" },
	{	E2P_FEATURE_RO_INCOMPAT, EXT4_FEATURE_RO_COMPAT_GDT_CSUM,
			"uninit_groups" },
	{	E2P_FEATURE_RO_INCOMPAT, EXT4_FEATURE_RO_COMPAT_DIR_NLINK,
			"dir_nlink" },
	{	E2P_FEATURE_RO_INCOMPAT, EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE,
			"extra_isize" },
	{	E2P_FEATURE_RO_INCOMPAT, EXT4_FEATURE_RO_COMPAT_QUOTA,
			"quota" },
	{	E2P_FEATURE_RO_INCOMPAT, EXT4_FEATURE_RO_COMPAT_BIGALLOC,
			"bigalloc"},
	{	E2P_FEATURE_RO_INCOMPAT, EXT4_FEATURE_RO_COMPAT_METADATA_CSUM,
			"metadata_csum"},
	{	E2P_FEATURE_RO_INCOMPAT, EXT4_FEATURE_RO_COMPAT_REPLICA,
			"replica" },

	{	E2P_FEATURE_INCOMPAT, EXT2_FEATURE_INCOMPAT_COMPRESSION,
			"compression" },
	{	E2P_FEATURE_INCOMPAT, EXT2_FEATURE_INCOMPAT_FILETYPE,
			"filetype" },
	{	E2P_FEATURE_INCOMPAT, EXT3_FEATURE_INCOMPAT_RECOVER,
			"needs_recovery" },
	{	E2P_FEATURE_INCOMPAT, EXT3_FEATURE_INCOMPAT_JOURNAL_DEV,
			"journal_dev" },
	{	E2P_FEATURE_INCOMPAT, EXT3_FEATURE_INCOMPAT_EXTENTS,
			"extent" },
	{	E2P_FEATURE_INCOMPAT, EXT3_FEATURE_INCOMPAT_EXTENTS,
			"extents" },
	{	E2P_FEATURE_INCOMPAT, EXT2_FEATURE_INCOMPAT_META_BG,
			"meta_bg" },
	{	E2P_FEATURE_INCOMPAT, EXT4_FEATURE_INCOMPAT_64BIT,
			"64bit" },
	{       E2P_FEATURE_INCOMPAT, EXT4_FEATURE_INCOMPAT_MMP,
			"mmp" },
	{       E2P_FEATURE_INCOMPAT, EXT4_FEATURE_INCOMPAT_FLEX_BG,
			"flex_bg"},
	{       E2P_FEATURE_INCOMPAT, EXT4_FEATURE_INCOMPAT_EA_INODE,
			"ea_inode"},
	{       E2P_FEATURE_INCOMPAT, EXT4_FEATURE_INCOMPAT_DIRDATA,
			"dirdata"},
	{       E2P_FEATURE_INCOMPAT, EXT4_FEATURE_INCOMPAT_LARGEDIR,
			"large_dir"},
	{       E2P_FEATURE_INCOMPAT, EXT4_FEATURE_INCOMPAT_INLINEDATA,
			"inline_data"},
	{	0, 0, 0 },
};

static struct feature jrnl_feature_list[] = {
       {       E2P_FEATURE_COMPAT, JFS_FEATURE_COMPAT_CHECKSUM,
                       "journal_checksum" },

       {       E2P_FEATURE_INCOMPAT, JFS_FEATURE_INCOMPAT_REVOKE,
                       "journal_incompat_revoke" },
       {       E2P_FEATURE_INCOMPAT, JFS_FEATURE_INCOMPAT_64BIT,
                       "journal_64bit" },
       {       E2P_FEATURE_INCOMPAT, JFS_FEATURE_INCOMPAT_ASYNC_COMMIT,
                       "journal_async_commit" },
       {       0, 0, 0 },
};

const char *e2p_feature2string(int compat, unsigned int mask)
{
	struct feature  *f;
	static char buf[20];
	char	fchar;
	int	fnum;

	for (f = feature_list; f->string; f++) {
		if ((compat == f->compat) &&
		    (mask == f->mask))
			return f->string;
	}
	switch (compat) {
	case  E2P_FEATURE_COMPAT:
		fchar = 'C';
		break;
	case E2P_FEATURE_INCOMPAT:
		fchar = 'I';
		break;
	case E2P_FEATURE_RO_INCOMPAT:
		fchar = 'R';
		break;
	default:
		fchar = '?';
		break;
	}
	for (fnum = 0; mask >>= 1; fnum++);
	sprintf(buf, "FEATURE_%c%d", fchar, fnum);
	return buf;
}

int e2p_string2feature(char *string, int *compat_type, unsigned int *mask)
{
	struct feature  *f;
	char		*eptr;
	int		num;

	for (f = feature_list; f->string; f++) {
		if (!strcasecmp(string, f->string)) {
			*compat_type = f->compat;
			*mask = f->mask;
			return 0;
		}
	}
	if (strncasecmp(string, "FEATURE_", 8))
		return 1;

	switch (string[8]) {
	case 'c':
	case 'C':
		*compat_type = E2P_FEATURE_COMPAT;
		break;
	case 'i':
	case 'I':
		*compat_type = E2P_FEATURE_INCOMPAT;
		break;
	case 'r':
	case 'R':
		*compat_type = E2P_FEATURE_RO_INCOMPAT;
		break;
	default:
		return 1;
	}
	if (string[9] == 0)
		return 1;
	num = strtol(string+9, &eptr, 10);
	if (num > 32 || num < 0)
		return 1;
	if (*eptr)
		return 1;
	*mask = 1 << num;
	return 0;
}

const char *e2p_jrnl_feature2string(int compat, unsigned int mask)
{
	struct feature  *f;
	static char buf[20];
	char	fchar;
	int	fnum;

	for (f = jrnl_feature_list; f->string; f++) {
		if ((compat == f->compat) &&
		    (mask == f->mask))
			return f->string;
	}
	switch (compat) {
	case  E2P_FEATURE_COMPAT:
		fchar = 'C';
		break;
	case E2P_FEATURE_INCOMPAT:
		fchar = 'I';
		break;
	case E2P_FEATURE_RO_INCOMPAT:
		fchar = 'R';
		break;
	default:
		fchar = '?';
		break;
	}
	for (fnum = 0; mask >>= 1; fnum++);
	sprintf(buf, "FEATURE_%c%d", fchar, fnum);
	return buf;
}

int e2p_jrnl_string2feature(char *string, int *compat_type, unsigned int *mask)
{
	struct feature  *f;
	char		*eptr;
	int		num;

	for (f = jrnl_feature_list; f->string; f++) {
		if (!strcasecmp(string, f->string)) {
			*compat_type = f->compat;
			*mask = f->mask;
			return 0;
		}
	}
	if (strncasecmp(string, "FEATURE_", 8))
		return 1;

	switch (string[8]) {
	case 'c':
	case 'C':
		*compat_type = E2P_FEATURE_COMPAT;
		break;
	case 'i':
	case 'I':
		*compat_type = E2P_FEATURE_INCOMPAT;
		break;
	case 'r':
	case 'R':
		*compat_type = E2P_FEATURE_RO_INCOMPAT;
		break;
	default:
		return 1;
	}
	if (string[9] == 0)
		return 1;
	num = strtol(string+9, &eptr, 10);
	if (num > 32 || num < 0)
		return 1;
	if (*eptr)
		return 1;
	*mask = 1 << num;
	return 0;
}
static char *skip_over_blanks(char *cp)
{
	while (*cp && isspace(*cp))
		cp++;
	return cp;
}

static char *skip_over_word(char *cp)
{
	while (*cp && !isspace(*cp) && *cp != ',')
		cp++;
	return cp;
}

/*
 * Edit a feature set array as requested by the user.  The ok_array,
 * if set, allows the application to limit what features the user is
 * allowed to set or clear using this function.  If clear_ok_array is set,
 * then use it tell whether or not it is OK to clear a filesystem feature.
 */
int e2p_edit_feature2(const char *str, __u32 *compat_array, __u32 *ok_array,
		      __u32 *clear_ok_array, int *type_err,
		      unsigned int *mask_err)
{
	char		*cp, *buf, *next;
	int		neg;
	unsigned int	mask;
	int		compat_type;
	int		rc = 0;

	if (!clear_ok_array)
		clear_ok_array = ok_array;

	if (type_err)
		*type_err = 0;
	if (mask_err)
		*mask_err = 0;

	buf = malloc(strlen(str)+1);
	if (!buf)
		return 1;
	strcpy(buf, str);
	for (cp = buf; cp && *cp; cp = next ? next+1 : 0) {
		neg = 0;
		cp = skip_over_blanks(cp);
		next = skip_over_word(cp);

		if (*next == 0)
			next = 0;
		else
			*next = 0;

		if ((strcasecmp(cp, "none") == 0) ||
		    (strcasecmp(cp, "clear") == 0)) {
			compat_array[0] = 0;
			compat_array[1] = 0;
			compat_array[2] = 0;
			continue;
		}

		switch (*cp) {
		case '-':
		case '^':
			neg++;
			/* fallthrough */
		case '+':
			cp++;
			break;
		}
		if (e2p_string2feature(cp, &compat_type, &mask)) {
			rc = 1;
			break;
		}
		if (neg) {
			if (clear_ok_array &&
			    !(clear_ok_array[compat_type] & mask)) {
				rc = 1;
				if (type_err)
					*type_err = (compat_type |
						     E2P_FEATURE_NEGATE_FLAG);
				if (mask_err)
					*mask_err = mask;
				break;
			}
			compat_array[compat_type] &= ~mask;
		} else {
			if (ok_array && !(ok_array[compat_type] & mask)) {
				rc = 1;
				if (type_err)
					*type_err = compat_type;
				if (mask_err)
					*mask_err = mask;
				break;
			}
			compat_array[compat_type] |= mask;
		}
	}
	free(buf);
	return rc;
}

int e2p_edit_feature(const char *str, __u32 *compat_array, __u32 *ok_array)
{
	return e2p_edit_feature2(str, compat_array, ok_array, 0, 0, 0);
}

#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
	int compat, compat2, i;
	unsigned int mask, mask2;
	const char *str;
	struct feature *f;

	for (i = 0; i < 2; i++) {
		if (i == 0) {
			f = feature_list;
			printf("Feature list:\n");
		} else {
			printf("\nJournal feature list:\n");
			f = jrnl_feature_list;
		}
		for (; f->string; f++) {
			if (i == 0) {
				e2p_string2feature((char *)f->string, &compat,
						   &mask);
				str = e2p_feature2string(compat, mask);
			} else {
				e2p_jrnl_string2feature((char *)f->string,
							&compat, &mask);
				str = e2p_jrnl_feature2string(compat, mask);
			}

			printf("\tCompat = %d, Mask = %u, %s\n",
			       compat, mask, f->string);
			if (strcmp(f->string, str)) {
				if (e2p_string2feature((char *) str, &compat2,
						       &mask2) ||
				    (compat2 != compat) ||
				    (mask2 != mask)) {
					fprintf(stderr, "Failure!\n");
					exit(1);
				}
			}
		}
	}
	exit(0);
}
#endif
