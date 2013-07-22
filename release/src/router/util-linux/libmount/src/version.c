/*
 * version.c - Return the version of the blkid library
 *
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 * [Based on libblkid/version.c by Theodore Ts'o]
 *
 * See COPYING.libmount for the License of this software.
 */

/**
 * SECTION: version
 * @title: Version functions
 * @short_description: functions to get library version.
 */

#include <ctype.h>

#include "mountP.h"

static const char *lib_version = LIBMOUNT_VERSION;

/**
 * mnt_parse_version_string:
 * @ver_string: version string (e.g "2.18.0")
 *
 * Returns: release version code.
 */
int mnt_parse_version_string(const char *ver_string)
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
 * mnt_get_library_version:
 * @ver_string: return pointer to the static library version string
 *
 * Returns: release version number.
 */
int mnt_get_library_version(const char **ver_string)
{
	if (ver_string)
		*ver_string = lib_version;

	return mnt_parse_version_string(lib_version);
}

#ifdef TEST_PROGRAM
int test_version(struct libmnt_test *ts, int argc, char *argv[])
{
	const char *ver;

	mnt_get_library_version(&ver);

	printf("Library version: %s\n", ver);
	printf("Library API version: " LIBMOUNT_VERSION "\n");

	if (mnt_get_library_version(NULL) ==
			mnt_parse_version_string(LIBMOUNT_VERSION))
		return 0;

	return -1;
}

int main(int argc, char *argv[])
{
	struct libmnt_test ts[] = {
		{ "--print", test_version, "print versions" },
		{ NULL }
	};

	return mnt_run_test(ts, argc, argv);
}
#endif
