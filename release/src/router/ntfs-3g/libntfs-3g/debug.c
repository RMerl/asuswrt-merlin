/**
 * debug.c - Debugging output functions. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2002-2004 Anton Altaparmakov
 * Copyright (c) 2004-2006 Szabolcs Szakacsits
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "types.h"
#include "runlist.h"
#include "debug.h"
#include "logging.h"

#ifdef DEBUG
/**
 * ntfs_debug_runlist_dump - Dump a runlist.
 * @rl:
 *
 * Description...
 *
 * Returns:
 */
void ntfs_debug_runlist_dump(const runlist_element *rl)
{
	int i = 0;
	const char *lcn_str[5] = { "LCN_HOLE         ", "LCN_RL_NOT_MAPPED",
				   "LCN_ENOENT       ", "LCN_EINVAL       ",
				   "LCN_unknown      " };

	ntfs_log_debug("NTFS-fs DEBUG: Dumping runlist (values in hex):\n");
	if (!rl) {
		ntfs_log_debug("Run list not present.\n");
		return;
	}
	ntfs_log_debug("VCN              LCN               Run length\n");
	do {
		LCN lcn = (rl + i)->lcn;

		if (lcn < (LCN)0) {
			int idx = -lcn - 1;

			if (idx > -LCN_EINVAL - 1)
				idx = 4;
			ntfs_log_debug("%-16lld %s %-16lld%s\n", 
				       (long long)rl[i].vcn, lcn_str[idx], 
				       (long long)rl[i].length, 
				       rl[i].length ? "" : " (runlist end)");
		} else
			ntfs_log_debug("%-16lld %-16lld  %-16lld%s\n", 
				       (long long)rl[i].vcn, (long long)rl[i].lcn, 
				       (long long)rl[i].length, 
				       rl[i].length ? "" : " (runlist end)");
	} while (rl[i++].length);
}

#endif

