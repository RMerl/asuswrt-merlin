/* vi: set sw=4 ts=4: */
/*
 * deluser/delgroup implementation for busybox
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 * Copyright (C) 2007 by Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 *
 */
#include "libbb.h"

static int del_line_matching(char **args, const char *filename)
{
	if (ENABLE_FEATURE_DEL_USER_FROM_GROUP && args[2]) {
		return update_passwd(filename, args[2], NULL, args[1]);
	}
	return update_passwd(filename, args[1], NULL, NULL);
}

int deluser_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int deluser_main(int argc, char **argv)
{
	if (argc != 2
	 && (!ENABLE_FEATURE_DEL_USER_FROM_GROUP
	    || (applet_name[3] != 'g' || argc != 3))
	) {
		bb_show_usage();
	}

	if (geteuid())
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);

	if ((ENABLE_FEATURE_DEL_USER_FROM_GROUP && argc != 3)
	 || ENABLE_DELUSER
	 || (ENABLE_DELGROUP && ENABLE_DESKTOP)
	) {
		if (ENABLE_DELUSER
		 && (!ENABLE_DELGROUP || applet_name[3] == 'u')
		) {
			if (del_line_matching(argv, bb_path_passwd_file) < 0)
				return EXIT_FAILURE;
			if (ENABLE_FEATURE_SHADOWPASSWDS) {
				del_line_matching(argv, bb_path_shadow_file);
			}
		} else if (ENABLE_DESKTOP && ENABLE_DELGROUP && getpwnam(argv[1]))
			bb_error_msg_and_die("can't remove primary group of user %s", argv[1]);
	}
	if (del_line_matching(argv, bb_path_group_file) < 0)
		return EXIT_FAILURE;
	if (ENABLE_FEATURE_SHADOWPASSWDS) {
		del_line_matching(argv, bb_path_gshadow_file);
	}
	return EXIT_SUCCESS;
}
