/*
 * load_policy
 * Author: Yuichi Nakamura <ynakam@hitachisoft.jp>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//usage:#define load_policy_trivial_usage NOUSAGE_STR
//usage:#define load_policy_full_usage ""

#include "libbb.h"

int load_policy_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int load_policy_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	int rc;

	if (argv[1]) {
		bb_show_usage();
	}

	rc = selinux_mkload_policy(1);
	if (rc < 0) {
		bb_perror_msg_and_die("can't load policy");
	}

	return 0;
}
