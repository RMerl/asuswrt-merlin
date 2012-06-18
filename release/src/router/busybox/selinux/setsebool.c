/*
 * setsebool
 * Simple setsebool
 * NOTE: -P option requires libsemanage, so this feature is
 * omitted in this version
 * Yuichi Nakamura <ynakam@hitachisoft.jp>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

int setsebool_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setsebool_main(int argc, char **argv)
{
	char *p;
	int value;

	if (argc != 3)
		bb_show_usage();

	p = argv[2];

	if (LONE_CHAR(p, '1') || strcasecmp(p, "true") == 0 || strcasecmp(p, "on") == 0) {
		value = 1;
	} else if (LONE_CHAR(p, '0') || strcasecmp(p, "false") == 0 || strcasecmp(p, "off") == 0) {
		value = 0;
	} else {
		bb_show_usage();
	}

	if (security_set_boolean(argv[1], value) < 0)
		bb_error_msg_and_die("can't set boolean");

	return 0;
}
