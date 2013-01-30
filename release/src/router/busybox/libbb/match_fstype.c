/* vi: set sw=4 ts=4: */
/*
 * Match fstypes for use in mount unmount
 * We accept notmpfs,nfs but not notmpfs,nonfs
 * This allows us to match fstypes that start with no like so
 *   mount -at ,noddy
 *
 * Returns 1 for a match, otherwise 0
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

int FAST_FUNC match_fstype(const struct mntent *mt, const char *t_fstype)
{
	int match = 1;
	int len;

	if (!t_fstype)
		return match;

	if (t_fstype[0] == 'n' && t_fstype[1] == 'o') {
		match--;
		t_fstype += 2;
	}

	len = strlen(mt->mnt_type);
	while (1) {
		if (strncmp(mt->mnt_type, t_fstype, len) == 0
		 && (t_fstype[len] == '\0' || t_fstype[len] == ',')
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
