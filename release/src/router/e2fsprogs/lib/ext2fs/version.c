/*
 * version.c --- Return the version of the ext2 library
 *
 * Copyright (C) 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include "config.h"
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "ext2_fs.h"
#include "ext2fs.h"

#include "../../version.h"

static const char *lib_version = E2FSPROGS_VERSION;
static const char *lib_date = E2FSPROGS_DATE;

int ext2fs_parse_version_string(const char *ver_string)
{
	const char *cp;
	int version = 0, dot_count = 0;

	for (cp = ver_string; *cp; cp++) {
		if (*cp == '.') {
			if (dot_count++)
				break;
			else
				continue;
		}
		if (!isdigit(*cp))
			break;
		version = (version * 10) + (*cp - '0');
	}
	return version;
}


int ext2fs_get_library_version(const char **ver_string,
			       const char **date_string)
{
	if (ver_string)
		*ver_string = lib_version;
	if (date_string)
		*date_string = lib_date;

	return ext2fs_parse_version_string(lib_version);
}
