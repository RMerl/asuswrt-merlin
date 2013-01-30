/* vi: set sw=4 ts=4: */
/*
 * chpasswd.c
 *
 * Written for SLIND (from passwd.c) by Alexander Shishkin <virtuoso@slind.org>
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

//usage:#define chpasswd_trivial_usage
//usage:	IF_LONG_OPTS("[--md5|--encrypted]") IF_NOT_LONG_OPTS("[-m|-e]")
//usage:#define chpasswd_full_usage "\n\n"
//usage:       "Read user:password from stdin and update /etc/passwd\n"
//usage:	IF_LONG_OPTS(
//usage:     "\n	-e,--encrypted	Supplied passwords are in encrypted form"
//usage:     "\n	-m,--md5	Use MD5 encryption instead of DES"
//usage:	)
//usage:	IF_NOT_LONG_OPTS(
//usage:     "\n	-e	Supplied passwords are in encrypted form"
//usage:     "\n	-m	Use MD5 encryption instead of DES"
//usage:	)

//TODO: implement -c ALGO

#if ENABLE_LONG_OPTS
static const char chpasswd_longopts[] ALIGN1 =
	"encrypted\0" No_argument "e"
	"md5\0"       No_argument "m"
	;
#endif

#define OPT_ENC  1
#define OPT_MD5  2

int chpasswd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int chpasswd_main(int argc UNUSED_PARAM, char **argv)
{
	char *name;
	int opt;

	if (getuid() != 0)
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);

	opt_complementary = "m--e:e--m";
	IF_LONG_OPTS(applet_long_options = chpasswd_longopts;)
	opt = getopt32(argv, "em");

	while ((name = xmalloc_fgetline(stdin)) != NULL) {
		char *free_me;
		char *pass;
		int rc;

		pass = strchr(name, ':');
		if (!pass)
			bb_error_msg_and_die("missing new password");
		*pass++ = '\0';

		xuname2uid(name); /* dies if there is no such user */

		free_me = NULL;
		if (!(opt & OPT_ENC)) {
			char salt[sizeof("$N$XXXXXXXX")];

			crypt_make_salt(salt, 1);
			if (opt & OPT_MD5) {
				salt[0] = '$';
				salt[1] = '1';
				salt[2] = '$';
				crypt_make_salt(salt + 3, 4);
			}
			free_me = pass = pw_encrypt(pass, salt, 0);
		}

		/* This is rather complex: if user is not found in /etc/shadow,
		 * we try to find & change his passwd in /etc/passwd */
#if ENABLE_FEATURE_SHADOWPASSWDS
		rc = update_passwd(bb_path_shadow_file, name, pass, NULL);
		if (rc > 0) /* password in /etc/shadow was updated */
			pass = (char*)"x";
		if (rc >= 0)
			/* 0 = /etc/shadow missing (not an error), >0 = passwd changed in /etc/shadow */
#endif
			rc = update_passwd(bb_path_passwd_file, name, pass, NULL);
		/* LOGMODE_BOTH logs to syslog also */
		logmode = LOGMODE_BOTH;
		if (rc < 0)
			bb_error_msg_and_die("an error occurred updating password for %s", name);
		if (rc)
			bb_info_msg("Password for '%s' changed", name);
		logmode = LOGMODE_STDIO;
		free(name);
		free(free_me);
	}
	return EXIT_SUCCESS;
}
