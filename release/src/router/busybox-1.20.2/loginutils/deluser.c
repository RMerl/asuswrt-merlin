/* vi: set sw=4 ts=4: */
/*
 * deluser/delgroup implementation for busybox
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 * Copyright (C) 2007 by Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 *
 */

//usage:#define deluser_trivial_usage
//usage:       "USER"
//usage:#define deluser_full_usage "\n\n"
//usage:       "Delete USER from the system"

//usage:#define delgroup_trivial_usage
//usage:	IF_FEATURE_DEL_USER_FROM_GROUP("[USER] ")"GROUP"
//usage:#define delgroup_full_usage "\n\n"
//usage:       "Delete group GROUP from the system"
//usage:	IF_FEATURE_DEL_USER_FROM_GROUP(" or user USER from group GROUP")

#include "libbb.h"

int deluser_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int deluser_main(int argc, char **argv)
{
	/* User or group name */
	char *name;
	/* Username (non-NULL only in "delgroup USER GROUP" case) */
	char *member;
	/* Name of passwd or group file */
	const char *pfile;
	/* Name of shadow or gshadow file */
	const char *sfile;
	/* Are we deluser or delgroup? */
	int do_deluser = (ENABLE_DELUSER && (!ENABLE_DELGROUP || applet_name[3] == 'u'));

	if (geteuid() != 0)
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);

	name = argv[1];
	member = NULL;

	switch (argc) {
	case 3:
		if (!ENABLE_FEATURE_DEL_USER_FROM_GROUP || do_deluser)
			break;
		/* It's "delgroup USER GROUP" */
		member = name;
		name = argv[2];
		/* Fallthrough */

	case 2:
		if (do_deluser) {
			/* "deluser USER" */
			xgetpwnam(name); /* bail out if USER is wrong */
			pfile = bb_path_passwd_file;
			if (ENABLE_FEATURE_SHADOWPASSWDS)
				sfile = bb_path_shadow_file;
		} else {
			struct group *gr;
 do_delgroup:
			/* "delgroup GROUP" or "delgroup USER GROUP" */
			if (do_deluser < 0) { /* delgroup after deluser? */
				gr = getgrnam(name);
				if (!gr)
					return EXIT_SUCCESS;
			} else {
				gr = xgetgrnam(name); /* bail out if GROUP is wrong */
			}
			if (!member) {
				/* "delgroup GROUP" */
				struct passwd *pw;
				struct passwd pwent;
				/* Check if the group is in use */
#define passwd_buf bb_common_bufsiz1
				while (!getpwent_r(&pwent, passwd_buf, sizeof(passwd_buf), &pw)) {
					if (pwent.pw_gid == gr->gr_gid)
						bb_error_msg_and_die("'%s' still has '%s' as their primary group!", pwent.pw_name, name);
				}
				//endpwent();
			}
			pfile = bb_path_group_file;
			if (ENABLE_FEATURE_SHADOWPASSWDS)
				sfile = bb_path_gshadow_file;
		}

		/* Modify pfile, then sfile */
		do {
			if (update_passwd(pfile, name, NULL, member) == -1)
				return EXIT_FAILURE;
			if (ENABLE_FEATURE_SHADOWPASSWDS) {
				pfile = sfile;
				sfile = NULL;
			}
		} while (ENABLE_FEATURE_SHADOWPASSWDS && pfile);

		if (ENABLE_DELGROUP && do_deluser > 0) {
			/* "deluser USER" also should try to delete
			 * same-named group. IOW: do "delgroup USER"
			 */
// On debian deluser is a perl script that calls userdel.
// From man userdel:
//  If USERGROUPS_ENAB is defined to yes in /etc/login.defs, userdel will
//  delete the group with the same name as the user.
			do_deluser = -1;
			goto do_delgroup;
		}
		return EXIT_SUCCESS;
	}
	/* Reached only if number of command line args is wrong */
	bb_show_usage();
}
