/*
 * version.c --- Return the version of the blkid library
 *
 * Copyright (C) 2004 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "blkid.h"

/* LIBBLKID_* defined in the global config.h */
static const char *lib_version = LIBBLKID_VERSION;	/* release version */
static const char *lib_date = LIBBLKID_DATE;

/**
 * blkid_parse_version_string:
 * @ver_string:  version string (e.g. "2.16.0")
 *
 * Returns: release version code.
 */
int blkid_parse_version_string(const char *ver_string)
{
	const char *cp;
	int version = 0;

	for (cp = ver_string; *cp; cp++) {
		if (*cp == '.')
			continue;
		if (!isdigit(*cp))
			break;
		version = (version * 10) + (*cp - '0');
	}
	return version;
}

/**
 * blkid_get_library_version:
 * @ver_string: returns relese version (!= SONAME version)
 * @date_string: returns date
 *
 * Returns: release version code.
 */
int blkid_get_library_version(const char **ver_string,
			       const char **date_string)
{
	if (ver_string)
		*ver_string = lib_version;
	if (date_string)
		*date_string = lib_date;

	return blkid_parse_version_string(lib_version);
}
