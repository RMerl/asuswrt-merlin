/*
 * selinuxenabled
 *
 * Based on libselinux 1.33.1
 * Port to BusyBox  Hiroshi Shinji <shiroshi@my.email.ne.jp>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

int selinuxenabled_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int selinuxenabled_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	return !is_selinux_enabled();
}
