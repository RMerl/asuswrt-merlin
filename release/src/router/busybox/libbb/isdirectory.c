/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell
 * Permission has been granted to redistribute this code under the GPL.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <sys/stat.h>
#include "libbb.h"

/*
 * Return TRUE if fileName is a directory.
 * Nonexistent files return FALSE.
 */
int FAST_FUNC is_directory(const char *fileName, int followLinks, struct stat *statBuf)
{
	int status;
	struct stat astatBuf;

	if (statBuf == NULL) {
		/* use auto stack buffer */
		statBuf = &astatBuf;
	}

	if (followLinks)
		status = stat(fileName, statBuf);
	else
		status = lstat(fileName, statBuf);

	status = (status == 0 && S_ISDIR(statBuf->st_mode));

	return status;
}
