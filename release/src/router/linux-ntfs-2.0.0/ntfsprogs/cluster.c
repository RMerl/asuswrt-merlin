/**
 * cluster - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2003 Richard Russon
 *
 * This function will locate the owner of any given sector or cluster range.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "cluster.h"
#include "utils.h"
#include "logging.h"

/**
 * cluster_find
 */
int cluster_find(ntfs_volume *vol, LCN c_begin, LCN c_end, cluster_cb *cb, void *data)
{
	int j;
	int result = -1;
	struct mft_search_ctx *m_ctx = NULL;
	ntfs_attr_search_ctx  *a_ctx = NULL;
	ATTR_RECORD *rec;
	runlist *runs;

	if (!vol || !cb)
		return -1;

	m_ctx = mft_get_search_ctx(vol);
	m_ctx->flags_search = FEMR_IN_USE | FEMR_BASE_RECORD;

	while (mft_next_record(m_ctx) == 0) {

		if (!(m_ctx->flags_match & FEMR_BASE_RECORD))
			continue;

		ntfs_log_verbose("Inode: %llu\n", (unsigned long long)
				m_ctx->inode->mft_no);

		a_ctx = ntfs_attr_get_search_ctx(m_ctx->inode, NULL);

		while ((rec = find_attribute(AT_UNUSED, a_ctx))) {

			if (!rec->non_resident) {
				ntfs_log_verbose("0x%02x skipped - attr is resident\n", a_ctx->attr->type);
				continue;
			}

			runs = ntfs_mapping_pairs_decompress(vol, a_ctx->attr, NULL);
			if (!runs) {
				ntfs_log_error("Couldn't read the data runs.\n");
				goto done;
			}

			ntfs_log_verbose("\t[0x%02X]\n", a_ctx->attr->type);

			ntfs_log_verbose("\t\tVCN\tLCN\tLength\n");
			for (j = 0; runs[j].length > 0; j++) {
				LCN a_begin = runs[j].lcn;
				LCN a_end   = a_begin + runs[j].length - 1;

				if (a_begin < 0)
					continue;	// sparse, discontiguous, etc

				ntfs_log_verbose("\t\t%lld\t%lld-%lld (%lld)\n",
						(long long)runs[j].vcn,
						(long long)runs[j].lcn,
						(long long)(runs[j].lcn +
						runs[j].length - 1),
						(long long)runs[j].length);
				//dprint list

				if ((a_begin > c_end) || (a_end < c_begin))
					continue;	// before or after search range

				if ((*cb) (m_ctx->inode, a_ctx->attr, runs+j, data))
					return 1;
			}
		}

		ntfs_attr_put_search_ctx(a_ctx);
		a_ctx = NULL;
	}

	result = 0;
done:
	ntfs_attr_put_search_ctx(a_ctx);
	mft_put_search_ctx(m_ctx);

	return result;
}

