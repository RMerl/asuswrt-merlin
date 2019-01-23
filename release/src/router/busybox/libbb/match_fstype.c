/* vi: set sw=4 ts=4: */
/*
 * Match fstypes for use in mount unmount
 * We accept notmpfs,nfs but not notmpfs,nonfs
 * This allows us to match fstypes that start with no like so
 *   mount -at ,noddy
 *
 * Returns 1 for a match, otherwise 0
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#include "libbb.h"

#ifdef HAVE_MNTENT_H

int FAST_FUNC match_fstype(const struct mntent *mt, const char *t_fstype)
{
	int match = 1;

	if (!t_fstype)
		return match;

	if (t_fstype[0] == 'n' && t_fstype[1] == 'o') {
		match--;
		t_fstype += 2;
	}

	while (1) {
		char *after_mnt_type = is_prefixed_with(t_fstype, mt->mnt_type);
		if (after_mnt_type
		 && (*after_mnt_type == '\0' || *after_mnt_type == ',')
		) {
			return match;
		}
		t_fstype = strchr(t_fstype, ',');
		if (!t_fstype)
			break;
		t_fstype++;
	}

	return !match;
}

#endif /* HAVE_MNTENT_H */
