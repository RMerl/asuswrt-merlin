/* vi: set sw=4 ts=4: */
/*
 * fstrim.c - discard the part (or whole) of mounted filesystem.
 *
 * 03 March 2012 - Malek Degachi <malek-degachi@laposte.net>
 * Adapted for busybox from util-linux-2.12a.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//config:config FSTRIM
//config:	bool "fstrim"
//config:	default y
//config:	select PLATFORM_LINUX
//config:	help
//config:	  Discard unused blocks on a mounted filesystem.

//applet:IF_FSTRIM(APPLET(fstrim, BB_DIR_SBIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_FSTRIM) += fstrim.o

//usage:#define fstrim_trivial_usage
//usage:       "[OPTIONS] MOUNTPOINT"
//usage:#define fstrim_full_usage "\n\n"
//usage:	IF_LONG_OPTS(
//usage:       "	-o,--offset=OFFSET	Offset in bytes to discard from"
//usage:     "\n	-l,--length=LEN		Bytes to discard"
//usage:     "\n	-m,--minimum=MIN	Minimum extent length"
//usage:     "\n	-v,--verbose		Print number of discarded bytes"
//usage:	)
//usage:	IF_NOT_LONG_OPTS(
//usage:       "	-o OFFSET	Offset in bytes to discard from"
//usage:     "\n	-l LEN		Bytes to discard"
//usage:     "\n	-m MIN		Minimum extent length"
//usage:     "\n	-v		Print number of discarded bytes"
//usage:	)

#include "libbb.h"
#include <linux/fs.h>

#ifndef FITRIM
struct fstrim_range {
	uint64_t start;
	uint64_t len;
	uint64_t minlen;
};
#define FITRIM		_IOWR('X', 121, struct fstrim_range)
#endif

static const struct suffix_mult fstrim_sfx[] = {
	{ "KiB", 1024 },
	{ "kiB", 1024 },
	{ "K", 1024 },
	{ "k", 1024 },
	{ "MiB", 1048576 },
	{ "miB", 1048576 },
	{ "M", 1048576 },
	{ "m", 1048576 },
	{ "GiB", 1073741824 },
	{ "giB", 1073741824 },
	{ "G", 1073741824 },
	{ "g", 1073741824 },
	{ "KB", 1000 },
	{ "MB", 1000000 },
	{ "GB", 1000000000 },
	{ "", 0 }
};

int fstrim_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int fstrim_main(int argc UNUSED_PARAM, char **argv)
{
	struct fstrim_range range;
	char *arg_o, *arg_l, *arg_m, *mp;
	unsigned opts;
	int fd;

	enum {
		OPT_o = (1 << 0),
		OPT_l = (1 << 1),
		OPT_m = (1 << 2),
		OPT_v = (1 << 3),
	};

#if ENABLE_LONG_OPTS
	static const char getopt_longopts[] ALIGN1 =
		"offset\0"    Required_argument    "o"
		"length\0"    Required_argument    "l"
		"minimum\0"   Required_argument    "m"
		"verbose\0"   No_argument          "v"
		;
	applet_long_options = getopt_longopts;
#endif

	opt_complementary = "=1"; /* exactly one non-option arg: the mountpoint */
	opts = getopt32(argv, "o:l:m:v", &arg_o, &arg_l, &arg_m);

	memset(&range, 0, sizeof(range));
	range.len = ULLONG_MAX;

	if (opts & OPT_o)
		range.start = xatoull_sfx(arg_o, fstrim_sfx);
	if (opts & OPT_l)
		range.len = xatoull_sfx(arg_l, fstrim_sfx);
	if (opts & OPT_m)
		range.minlen = xatoull_sfx(arg_m, fstrim_sfx);

	mp = argv[optind];
	if (find_block_device(mp)) {
		fd = xopen_nonblocking(mp);
		xioctl(fd, FITRIM, &range);
		if (ENABLE_FEATURE_CLEAN_UP)
			close(fd);

		if (opts & OPT_v)
			printf("%s: %llu bytes trimmed\n", mp, (unsigned long long)range.len);
		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}
