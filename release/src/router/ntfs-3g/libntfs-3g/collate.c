/**
 * collate.c - NTFS collation handling.  Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2004 Anton Altaparmakov
 * Copyright (c) 2005 Yura Pakhuchiy
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
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "attrib.h"
#include "index.h"
#include "collate.h"
#include "debug.h"
#include "unistr.h"
#include "logging.h"

/**
 * ntfs_collate_binary - Which of two binary objects should be listed first
 * @vol: unused
 * @data1:
 * @data1_len:
 * @data2:
 * @data2_len:
 *
 * Description...
 *
 * Returns:
 */
static int ntfs_collate_binary(ntfs_volume *vol __attribute__((unused)),
		const void *data1, const int data1_len,
		const void *data2, const int data2_len)
{
	int rc;

	ntfs_log_trace("Entering.\n");
	rc = memcmp(data1, data2, min(data1_len, data2_len));
	if (!rc && (data1_len != data2_len)) {
		if (data1_len < data2_len)
			rc = -1;
		else
			rc = 1;
	}
	ntfs_log_trace("Done, returning %i.\n", rc);
	return rc;
}

/**
 * ntfs_collate_ntofs_ulong - Which of two long ints should be listed first
 * @vol: unused
 * @data1:
 * @data1_len:
 * @data2:
 * @data2_len:
 *
 * Description...
 *
 * Returns:
 */
static int ntfs_collate_ntofs_ulong(ntfs_volume *vol __attribute__((unused)),
		const void *data1, const int data1_len,
		const void *data2, const int data2_len)
{
	int rc;
	u32 d1, d2;

	ntfs_log_trace("Entering.\n");
	if (data1_len != data2_len || data1_len != 4) {
		ntfs_log_error("data1_len or/and data2_len not equal to 4.\n");
		return NTFS_COLLATION_ERROR;
	}
	d1 = le32_to_cpup(data1);
	d2 = le32_to_cpup(data2);
	if (d1 < d2)
		rc = -1;
	else {
		if (d1 == d2)
			rc = 0;
		else
			rc = 1;
	}
	ntfs_log_trace("Done, returning %i.\n", rc);
	return rc;
}

/**
 * ntfs_collate_ntofs_ulongs - Which of two le32 arrays should be listed first
 *
 * Returns: -1, 0 or 1 depending of how the arrays compare
 */

static int ntfs_collate_ntofs_ulongs(ntfs_volume *vol __attribute__((unused)),
		const void *data1, const int data1_len,
		const void *data2, const int data2_len)
{
	int rc;
	int len;
	const le32 *p1, *p2;
	u32 d1, d2;

	ntfs_log_trace("Entering.\n");
	if ((data1_len != data2_len) || (data1_len <= 0) || (data1_len & 3)) {
		ntfs_log_error("data1_len or data2_len not valid\n");
		return NTFS_COLLATION_ERROR;
	}
	p1 = (const le32*)data1;
	p2 = (const le32*)data2;
	len = data1_len;
	do {
		d1 = le32_to_cpup(p1);
		p1++;
		d2 = le32_to_cpup(p2);
		p2++;
	} while ((d1 == d2) && ((len -= 4) > 0));
	if (d1 < d2)
		rc = -1;
	else {
		if (d1 == d2)
			rc = 0;
		else
			rc = 1;
	}
	ntfs_log_trace("Done, returning %i.\n", rc);
	return rc;
}

/**
 * ntfs_collate_ntofs_security_hash - Which of two security descriptors
 *                    should be listed first
 * @vol: unused
 * @data1:
 * @data1_len:
 * @data2:
 * @data2_len:
 *
 * JPA compare two security hash keys made of two unsigned le32
 *
 * Returns: -1, 0 or 1 depending of how the keys compare
 */
static int ntfs_collate_ntofs_security_hash(ntfs_volume *vol __attribute__((unused)),
		const void *data1, const int data1_len,
		const void *data2, const int data2_len)
{
	int rc;
	u32 d1, d2;
	const le32 *p1, *p2;

	ntfs_log_trace("Entering.\n");
	if (data1_len != data2_len || data1_len != 8) {
		ntfs_log_error("data1_len or/and data2_len not equal to 8.\n");
		return NTFS_COLLATION_ERROR;
	}
	p1 = (const le32*)data1;
	p2 = (const le32*)data2;
	d1 = le32_to_cpup(p1);
	d2 = le32_to_cpup(p2);
	if (d1 < d2)
		rc = -1;
	else {
		if (d1 > d2)
			rc = 1;
		else {
			p1++;
			p2++;
			d1 = le32_to_cpup(p1);
			d2 = le32_to_cpup(p2);
			if (d1 < d2)
				rc = -1;
			else {
				if (d1 > d2)
					rc = 1;
				else
					rc = 0;
			}
		}
	}
	ntfs_log_trace("Done, returning %i.\n", rc);
	return rc;
}

/**
 * ntfs_collate_file_name - Which of two filenames should be listed first
 * @vol:
 * @data1:
 * @data1_len: unused
 * @data2:
 * @data2_len: unused
 *
 * Description...
 *
 * Returns:
 */
static int ntfs_collate_file_name(ntfs_volume *vol,
		const void *data1, const int data1_len __attribute__((unused)),
		const void *data2, const int data2_len __attribute__((unused)))
{
	const FILE_NAME_ATTR *file_name_attr1;
	const FILE_NAME_ATTR *file_name_attr2;
	int rc;

	ntfs_log_trace("Entering.\n");
	file_name_attr1 = (const FILE_NAME_ATTR*)data1;
	file_name_attr2 = (const FILE_NAME_ATTR*)data2;
	rc = ntfs_names_full_collate(
			(ntfschar*)&file_name_attr1->file_name,
			file_name_attr1->file_name_length,
			(ntfschar*)&file_name_attr2->file_name,
			file_name_attr2->file_name_length,
			CASE_SENSITIVE, vol->upcase, vol->upcase_len);
	ntfs_log_trace("Done, returning %i.\n", rc);
	return rc;
}

/*
 *		Get a pointer to appropriate collation function.
 *
 *	Returns NULL if the needed function is not implemented
 */

COLLATE ntfs_get_collate_function(COLLATION_RULES cr)
{
	COLLATE collate;

	switch (cr) {
	case COLLATION_BINARY :
		collate = ntfs_collate_binary;
		break;
	case COLLATION_FILE_NAME :
		collate = ntfs_collate_file_name;
		break;
	case COLLATION_NTOFS_SECURITY_HASH :
		collate = ntfs_collate_ntofs_security_hash;
		break;
	case COLLATION_NTOFS_ULONG :
		collate = ntfs_collate_ntofs_ulong;
		break;
	case COLLATION_NTOFS_ULONGS :
		collate = ntfs_collate_ntofs_ulongs;
		break;
	default :
		errno = EOPNOTSUPP;
		collate = (COLLATE)NULL;
		break;
	}
	return (collate);
}
