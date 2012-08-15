/**
 * collate.c - NTFS collation handling.  Part of the Linux-NTFS project.
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
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

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
		const void *data1, size_t data1_len,
		const void *data2, size_t data2_len)
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
		const void *data1, size_t data1_len,
		const void *data2, size_t data2_len)
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
		const void *data1, size_t data1_len __attribute__((unused)),
		const void *data2, size_t data2_len __attribute__((unused)))
{
	int rc;

	ntfs_log_trace("Entering.\n");
	rc = ntfs_file_values_compare(data1, data2, NTFS_COLLATION_ERROR,
			IGNORE_CASE, vol->upcase, vol->upcase_len);
	if (!rc)
		rc = ntfs_file_values_compare(data1, data2,
				NTFS_COLLATION_ERROR, CASE_SENSITIVE,
				vol->upcase, vol->upcase_len);
	ntfs_log_trace("Done, returning %i.\n", rc);
	return rc;
}

typedef int (*ntfs_collate_func_t)(ntfs_volume *, const void *, size_t,
		const void *, size_t);

static ntfs_collate_func_t ntfs_do_collate0x0[3] = {
	ntfs_collate_binary,
	ntfs_collate_file_name,
	NULL/*ntfs_collate_unicode_string*/,
};

static ntfs_collate_func_t ntfs_do_collate0x1[4] = {
	ntfs_collate_ntofs_ulong,
	NULL/*ntfs_collate_ntofs_sid*/,
	NULL/*ntfs_collate_ntofs_security_hash*/,
	NULL/*ntfs_collate_ntofs_ulongs*/,
};

/**
 * ntfs_is_collation_rule_supported - Check if a collation rule is implemented.
 * @cr: The to-be-checked collation rule
 *
 * Use this function to know if @cr is supported by libntfs.
 *
 * 7 collation rules are known to be supported by NTFS as defined
 * in layout.h. However, libntfs only support 3 of them ATM.
 *
 * Return TRUE if @cr is supported. FALSE otherwise.
 */
BOOL ntfs_is_collation_rule_supported(COLLATION_RULES cr)
{
	return (cr == COLLATION_BINARY || cr == COLLATION_NTOFS_ULONG ||
			cr == COLLATION_FILE_NAME);
	/*
	 * FIXME:  At the moment we only support COLLATION_BINARY,
	 * COLLATION_NTOFS_ULONG and COLLATION_FILE_NAME.
	 * The correct future implementation of this function should be:
	 *
	 * u32 i = le32_to_cpu(cr);
	 * return ((i <= 0x02) || ((i >= 0x10) && (i <= 0x13)));
	 */
}

/**
 * ntfs_collate - collate two data items using a specified collation rule
 * @vol:	ntfs volume to which the data items belong
 * @cr:		collation rule to use when comparing the items
 * @data1:	first data item to collate
 * @data1_len:	length in bytes of @data1
 * @data2:	second data item to collate
 * @data2_len:	length in bytes of @data2
 *
 * Collate the two data items @data1 and @data2 using the collation rule @cr
 * and return -1, 0, or 1 if @data1 is found, respectively, to collate before,
 * to match, or to collate after @data2.
 *
 * For speed we use the collation rule @cr as an index into two tables of
 * function pointers to call the appropriate collation function.
 *
 * Return NTFS_COLLATION_ERROR if error occurred.
 */
int ntfs_collate(ntfs_volume *vol, COLLATION_RULES cr,
		const void *data1, size_t data1_len,
		const void *data2, size_t data2_len)
{
	u32 i;

	ntfs_log_trace("Entering.\n");
	if (!vol || !data1 || !data2) {
		ntfs_log_error("Invalid arguments passed.\n");
		return NTFS_COLLATION_ERROR;
	}

	if (!ntfs_is_collation_rule_supported(cr))
		goto err;
	i = le32_to_cpu(cr);
	if (i <= 0x02)
		return ntfs_do_collate0x0[i](vol, data1, data1_len,
				data2, data2_len);
	if (i < 0x10)
		goto err;
	i -= 0x10;
	if (i <= 3)
		return ntfs_do_collate0x1[i](vol, data1, data1_len,
				data2, data2_len);
err:
	ntfs_log_debug("Unknown collation rule.\n");
	return NTFS_COLLATION_ERROR;
}
