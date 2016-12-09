/* vi: set sw=4 ts=4: */
/* unlink for busybox
 *
 * Copyright (C) 2014 Isaac Dunham <ibid.ag@gmail.com>
 *
 * Licensed under GPLv2, see LICENSE in this source tree
 */

//config:config UNLINK
//config:	bool "unlink"
//config:	default y
//config:	help
//config:	  unlink deletes a file by calling unlink()

//kbuild:lib-$(CONFIG_UNLINK) += unlink.o

//applet:IF_UNLINK(APPLET(unlink, BB_DIR_USR_BIN, BB_SUID_DROP))

//usage:#define unlink_trivial_usage
//usage:	"FILE"
//usage:#define unlink_full_usage "\n\n"
//usage:	"Delete FILE by calling unlink()"

#include "libbb.h"

int unlink_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int unlink_main(int argc UNUSED_PARAM, char **argv)
{
	opt_complementary = "=1"; /* must have exactly 1 param */
	getopt32(argv, "");
	argv += optind;
	xunlink(argv[0]);
	return 0;
}
